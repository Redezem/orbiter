#!/usr/bin/env python3
"""Real-engine shared-depth acceptance; CPU-selected witnesses, never GPU selection."""
import json
import hashlib
import math
import re
from pathlib import Path
import subprocess
import sys
from PIL import Image, ImageDraw, ImageFilter
from fixture_staging import stage
from terrain_regression import Reference, projected, numeric, parse, lifecycle, require
from camera_regression import verify_frame

BG=(32,64,128)

def records(log,key):
    return [json.loads(s.split(key+'=',1)[1]) for s in log.splitlines() if key+'=' in s]


def vessel_project(frame,geometry):
    # Scalar world projection from actual API doubles, NOT uploaded clip matrix.
    cr=frame['camera_rotation']; vr=frame['vessel_rotation']
    t=[a-b for a,b in zip(frame['vessel_position'],frame['camera_position'])]
    out=[]; sy=1/math.tan(frame['aperture'])
    for v in geometry['vertices']:
        world=[sum(vr[r*3+c]*v[c] for c in range(3))+t[r] for r in range(3)]
        x,y,z=[sum(cr[r*3+c]*world[r] for r in range(3)) for c in range(3)]
        require(z>.5,'bounded vessel crosses common near plane')
        out.append((320+240*sy*x/z,240-240*sy*y/z,z,*v[3:]))
    return out


def raster(vertices,triangles,stride=1,terrain=False):
    """Scalar pixel-centre depth, perspective attributes and independent safety."""
    hits={}; second={}
    for ids,texture in triangles:
        vs=[vertices[i] for i in ids]
        if min(v[2] for v in vs)<.5-1e-9: continue
        a,b,c=vs; ax,ay=a[:2];bx,by=b[:2];cx,cy=c[:2]
        den=(by-cy)*(ax-cx)+(cx-bx)*(ay-cy)
        if abs(den)<1e-10: continue
        xmin=max(0,math.ceil(min(ax,bx,cx)-.5));xmax=min(639,math.floor(max(ax,bx,cx)-.5))
        ymin=max(0,math.ceil(min(ay,by,cy)-.5));ymax=min(479,math.floor(max(ay,by,cy)-.5))
        for y in range(ymin+(-ymin)%stride,ymax+1,stride):
            for x in range(xmin+(-xmin)%stride,xmax+1,stride):
                wa=((by-cy)*(x+.5-cx)+(cx-bx)*(y+.5-cy))/den
                wb=((cy-ay)*(x+.5-cx)+(ax-cx)*(y+.5-cy))/den
                w=(wa,wb,1-wa-wb)
                if min(w)<0: continue
                rec=sum(t/v[2] for t,v in zip(w,vs)); z=1/rec; key=(x,y)
                old=hits.get(key)
                if old and old['z']<=z:
                    second[key]=min(second.get(key,math.inf),z);continue
                if old: second[key]=old['z']
                attrs=[sum(t*v[k]/v[2] for t,v in zip(w,vs))/rec for k in range(3,len(a))]
                distance=min(w[i]*abs(den)/math.hypot(vs[(i+1)%3][0]-vs[(i+2)%3][0],vs[(i+1)%3][1]-vs[(i+2)%3][1]) for i in range(3))
                hits[key]=dict(z=z,edge=min(w),edge_distance=distance,attrs=attrs,texture=texture)
    for key,h in hits.items():
        # Four common Depth32Float ULP, expressed in camera-space metres.
        h['safe']=(h['edge_distance']>.6 if terrain else h['edge']>.04) and second.get(key,math.inf)-h['z']>4*2**-24*h['z']**2/.5
    return hits


def triangles(geometry):
    ind=geometry['indices']
    return [(ind[i:i+3],r['texture']) for r in geometry['ranges'] for i in range(r['first'],r['first']+r['count'],3)]


def terrain_triangles():
    out=[]
    for r in range(32):
        for c in range(32):
            q=r*33+c
            out.extend([((q,q+33,q+1),None),((q+1,q+33,q+34),None)])
    return out


def clipped(screen,tris,aperture):
    """Independent Sutherland-Hodgman common near/far clipping in camera space."""
    sy=1/math.tan(aperture); out=[]; indices=[]
    camera=[((p[0]-320)*p[2]/(240*sy),(240-p[1])*p[2]/(240*sy),*p[2:]) for p in screen]
    for ids,texture in tris:
        poly=[camera[i] for i in ids]
        for bound,sign in ((.5,1),(5000.,-1)):
            result=[]
            for a,b in zip(poly,poly[1:]+poly[:1]):
                ia=sign*(a[2]-bound)>=0;ib=sign*(b[2]-bound)>=0
                if ia:result.append(a)
                if ia!=ib:
                    t=(bound-a[2])/(b[2]-a[2]);result.append(tuple(x+t*(y-x) for x,y in zip(a,b)))
            poly=result
        if len(poly)<3:continue
        base=len(out)
        out.extend((320+240*sy*p[0]/p[2],240-240*sy*p[1]/p[2],*p[2:]) for p in poly)
        indices.extend(((base,base+i,base+i+1),texture) for i in range(1,len(poly)-1))
    return out,indices


def expected(hit,ref,images,terrain=False):
    if not hit['safe']: return None
    if terrain:
        uv=hit['attrs']; tex=ref.texture; shade=1; diffuse=(1,1,1)
        u,v=uv; tx=max(0,min(tex.width-1,u*tex.width));ty=max(0,min(tex.height-1,v*tex.height))
    else:
        shade,u,v,*diffuse=hit['attrs']; path=hit['texture']
        if not path:return None
        if path not in images: images[path]=Image.open(path).convert('RGB')
        tex=images[path];tx=(u%1)*tex.width;ty=(v%1)*tex.height
    if min(tx%1,1-tx%1,ty%1,1-ty%1)<.1:return None
    texel=tex.getpixel((int(tx),int(ty)))
    return tuple(round(texel[i]*shade*diffuse[i]) for i in range(3))


def silhouette(image,screen,tris):
    mask=Image.new('L',(640,480)); d=ImageDraw.Draw(mask)
    for ids,_ in tris:d.polygon([screen[i][:2] for i in ids],fill=255)
    inner=mask.filter(ImageFilter.MinFilter(5)).tobytes();outer=mask.filter(ImageFilter.MaxFilter(5)).tobytes()
    missing=sum(bool(inner[i] and p==BG) for i,p in enumerate(image.getdata()))
    extra=sum(bool(not outer[i] and p!=BG) for i,p in enumerate(image.getdata()))
    require(missing==extra==0,f'independent silhouette missing={missing} extra={extra}')
    return dict(missing=missing,extra=extra,boundary_tolerance_px=2)


def near_edges(screen,tris,tolerance=.6):
    # Reject proximity to ANY projected source edge, including a foreground
    # triangle just outside CPU coverage. Winner-only barycentrics miss those.
    excluded=set()
    for ids,_ in tris:
        vs=[screen[i] for i in ids]
        for a,b in zip(vs,vs[1:]+vs[:1]):
            dx=b[0]-a[0];dy=b[1]-a[1];den=dx*dx+dy*dy
            if not den:continue
            for y in range(max(0,math.ceil(min(a[1],b[1])-.5-tolerance)),min(479,math.floor(max(a[1],b[1])-.5+tolerance))+1):
                for x in range(max(0,math.ceil(min(a[0],b[0])-.5-tolerance)),min(639,math.floor(max(a[0],b[0])-.5+tolerance))+1):
                    t=max(0,min(1,((x+.5-a[0])*dx+(y+.5-a[1])*dy)/den))
                    if math.hypot(x+.5-a[0]-t*dx,y+.5-a[1]-t*dy)<tolerance:excluded.add((x,y))
    return excluded


def oracle(log,snapshot,content,vessel_only,terrain_only):
    text=log.read_text(); frames,g=parse(text); f=records(text,'camera_json')[0]
    api=records(text,'camera_json');bound=records(text,'gpu_clip_json')
    # Omit-vessel diagnostic legitimately binds no vessel uniforms.
    if bound:require(len(bound)==12 and all(a['clip_matrix']==b for a,b in zip(api,bound)),'actual bound vessel clip mismatch')
    for a,b in zip(api,frames):
        verify_frame(a)
        require(a['camera_position']==b['camera'] and a['camera_rotation']==b['cameraR'] and a['aperture']==b['aperture'],'separate scene camera')
        require(a['near']==.5 and a['far']==5000,'common bounded frustum')
    geometry=json.loads(Path(str(snapshot)+'.geometry.json').read_text())
    slots={int(slot):path for slot,path in re.findall(r'texture_slot slot=(\d+) handle=\S+ path=(.*)',text)}
    groups=records(text,'material_json');previous=0
    require(len(slots)==10 and len(groups)==len(geometry['ranges'])==122,'actual DG texture slot provenance')
    for group,draw in zip(groups,geometry['ranges']):
        raw=group['texture_index'];slot=previous if raw==0xfffffffe else 0 if raw==0xffffffff else raw+1
        require(draw['texture']==(slots[slot] if slot else ''),'API texture slot/range mismatch');previous=slot
    with Reference(content,True) as ref:
        verts=ref.geometry(frames[0]['radius'])
        tv=projected(frames[0],verts); vv=vessel_project(f,geometry)
        depth_error=0
        for vertex,screen,uploaded in zip(verts,tv,g['vertices']):
            p=uploaded[4:7]+[1]
            clip=[sum(frames[0]['bound'][c*4+r]*p[c] for c in range(4)) for r in range(4)]
            depth_error=max(depth_error,abs(clip[3]-screen[2]),abs(clip[2]-(screen[2]-.5)*5000/4999.5))
        require(depth_error<.002,'terrain depth projection differs from common frustum')
        vt=triangles(geometry);tt=terrain_triangles()
        tv,tt=clipped(tv,tt,f['aperture'])
        v=raster(vv,vt);t=raster(tv,tt,terrain=True)
        edge_exclusions=near_edges(vv,vt)
        vessel_mask=Image.new('L',(640,480));md=ImageDraw.Draw(vessel_mask)
        for ids,_ in vt:md.polygon([vv[i][:2] for i in ids],fill=255)
        vessel_interior=vessel_mask.filter(ImageFilter.MinFilter(5))
        im=Image.open(snapshot).convert('RGB'); vi=Image.open(vessel_only).convert('RGB');ti=Image.open(terrain_only).convert('RGB')
        silhouettes=dict(vessel=silhouette(vi,vv,vt),terrain=silhouette(ti,tv,tt))
        images={};counts=dict(terrain_front=0,vessel_front=0,terrain_texture=0,vessel_texture=0);errors=[];witnesses=[]
        max_channel_error=0
        for xy in sorted(t.keys()|v.keys()):
            a=v.get(xy);b=t.get(xy)

            # All selection is geometric/UV/depth before observed GPU data.
            if a and b and abs(a['z']-b['z'])<.03:continue
            front='vessel' if a and (not b or a['z']<b['z']) else 'terrain'
            if front=='vessel' and xy in edge_exclusions:continue
            hit=a if front=='vessel' else b
            color=expected(hit,ref,images,front=='terrain')
            if color is None:continue
            obs=im.getpixel(xy);solo=(vi if front=='vessel' else ti).getpixel(xy)
            err=max(abs(x-y) for x,y in zip(color,obs));soloerr=max(abs(x-y) for x,y in zip(color,solo))
            max_channel_error=max(max_channel_error,err,soloerr)
            counts[front+'_texture']+=1
            overlap=bool(a and b and a['safe'] and b['safe'] and vessel_interior.getpixel(xy))
            if overlap:counts[front+'_front']+=1
            record=dict(x=xy[0],y=xy[1],winner=front,vessel_z=a['z'] if a else None,terrain_z=b['z'] if b else None,expected=color,actual=obs,solo=solo,error=err,solo_error=soloerr,overlap=overlap)
            if overlap or len(witnesses)<100:witnesses.append(record)
            if err>2 or soloerr>2:errors.append(record)
        result=dict(counts=counts,silhouettes=silhouettes,errors=len(errors),max_channel_error=max_channel_error,first_errors=errors[:4],witnesses=witnesses,max_terrain_clip_depth_error_m=depth_error,
                    source_surface=ref.surface_identity,source_elevation=ref.provenance,
                    vessel_textures={p:dict(sha256=hashlib.sha256(Path(p).read_bytes()).hexdigest(),dimensions=im.size) for p,im in images.items()})
        Path(str(snapshot)+'.occlusion.json').write_text(json.dumps(result,indent=2)+'\n')
        require(counts['terrain_front']>=8 and counts['vessel_front']>=80,'insufficient independent mixed depth overlap '+str(counts))
        require(counts['terrain_texture']>128 and counts['vessel_texture']>80,'insufficient texture witnesses')
        require(not errors,'GPU shared-depth occlusion/texture mismatch '+json.dumps(errors[:2]))
        result['terrain_numeric']=numeric(frames[0],g,ref,verts,visible_projection_only=True)
        # Actual template penetration is reported, not hidden by invented height
        # offsets or treated as proof of gear/contact simulation correctness.
        cr=frames[0]['bodyR'];vr=f['vessel_rotation']
        rel=[a-b for a,b in zip(f['vessel_position'],frames[0]['body'])]
        clearances=[]
        for vertex in geometry['vertices']:
            world=[sum(vr[r*3+c]*vertex[c] for c in range(3))+rel[r] for r in range(3)]
            body=[sum(cr[r*3+c]*world[r] for r in range(3)) for c in range(3)]
            radius=math.sqrt(sum(x*x for x in body));lng=math.atan2(body[2],body[0]);lat=math.asin(body[1]/radius)
            clearances.append(radius-frames[0]['radius']-ref.height(lng,lat)[0])
        result['static_template_surface_clearance_m']=[min(clearances),max(clearances)]
        verify_frame(f)
        return {k:v for k,v in result.items() if k!='witnesses'}


def main():
    if sys.argv[1]=='--oracle':
        print(json.dumps(oracle(*map(Path,sys.argv[2:7]))));return
    exe,content,output=map(lambda x:Path(x).resolve(),sys.argv[1:4]);output.mkdir(parents=True,exist_ok=True)
    launch=stage(content,output,'terrain-vessel-content-')
    # Exclusive new fixture, no write through links or stock scenario changes.
    with (launch/'Scenarios/MetalTerrainVessel.scn').open('xb') as f:f.write(Path(__file__).with_name('fixtures').joinpath('MetalTerrainVessel.scn').read_bytes())
    runs={};results=dict(commands=[])
    for name,flags in [('combined',[]),('vessel-only',['--diagnostic-omit-terrain']),('terrain-only',['--diagnostic-omit-vessel']),('reverse',['--diagnostic-terrain-last']),('no-depth',['--diagnostic-disable-scene-depth'])]:
        snapshot=output/(name+'.ppm');log=output/(name+'.log')
        cmd=[str(exe),'MetalTerrainVessel','12','--terrain-vessel-test',str(snapshot),*flags]
        p=subprocess.run(cmd,cwd=launch,capture_output=True,text=True,timeout=90);log.write_text(p.stdout+p.stderr)
        results['commands'].append(dict(argv=cmd,cwd=str(launch),exit=p.returncode))
        require(p.returncode==0,'renderer failed '+name+': '+(p.stdout+p.stderr)[-2500:]);lifecycle(log.read_text())
        (output/(name+'.engine.log')).write_bytes((launch/'Orbiter_core_metal.log').read_bytes())
        runs[name]=(log,snapshot)
    args=(launch,runs['vessel-only'][1],runs['terrain-only'][1])
    first=records(runs['combined'][0].read_text(),'camera_json')[0]
    for name,(log,_) in runs.items():
        actual=records(log.read_text(),'camera_json')[0]
        for key in ('camera_position','vessel_position','camera_rotation','vessel_rotation'):
            require(max(abs(a-b) for a,b in zip(first[key],actual[key]))<(1e-3 if key.endswith('position') else 1e-9),'diagnostic changed actual engine pose '+name+' '+key)
    results['positive']=oracle(*runs['combined'],*args)
    results['reverse']=oracle(*runs['reverse'],*args)
    require(runs['combined'][1].read_bytes()==runs['reverse'][1].read_bytes(),'draw order changed depth-tested GPU output')
    negative=subprocess.run([sys.executable,__file__,'--oracle',*map(str,(*runs['no-depth'],*args))],capture_output=True,text=True,timeout=120)
    (output/'no-depth.oracle.log').write_text(negative.stdout+negative.stderr)
    require(negative.returncode!=0 and 'GPU shared-depth occlusion/texture mismatch' in negative.stderr,'same occlusion gate failed to reject no-depth')
    rejected=json.loads(Path(str(runs['no-depth'][1])+'.occlusion.json').read_text())
    require(any(w['winner']=='terrain' and w['overlap'] and w['error']>2 for w in rejected['witnesses']),'negative rejected only self-texture, not terrain occlusion')
    results['negative']=dict(renderer_exit=0,oracle_exit=negative.returncode)
    (output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    print('PASS shared actual-engine terrain/vessel depth; '+json.dumps(results['positive'])+'; no-depth rejected; order invariant')

if __name__=='__main__':main()
