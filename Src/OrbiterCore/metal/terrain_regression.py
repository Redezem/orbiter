#!/usr/bin/env python3
"""Actual-engine bounded terrain acceptance. Independent installed ELE/TX/Pillow
reference predicts geometry and pre-UI GPU pixels. Never replacement runtime data.
"""
import hashlib
import io
import json
import math
from pathlib import Path
import re
import struct
import subprocess
import sys
import zlib
from PIL import Image, ImageDraw, ImageFilter
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'smoke'))
from elevation_regression import Archive, grid, locate, interpolate
from fixture_staging import stage
from planet_regression import dot, pose, BG


def require(b, message):
    if not b:
        raise AssertionError(message)


def point(radius, height, lng, lat):
    return [(radius+height)*math.cos(lat)*math.cos(lng), (radius+height)*math.sin(lat),
            (radius+height)*math.cos(lat)*math.sin(lng)]


def geography(coast):
    lng, lat = map(math.radians, (-80.6209, 28.6235) if coast else (80.91683349609374, 35.31412353515625))
    p = 13 if coast else 10
    n = 2**p
    return p+4, int((.5-lat/math.pi)*n), int((1+lng/math.pi)*n)


def coordinates(key, row, col):
    level, r, c = key
    n = 2**(level-4)
    return math.pi*((c+.001+.998*col/32)/n-1), math.pi*(.5-(r+.001+.998*row/32)/n)


class Reference:
    def __init__(self, content, coast):
        self.base = Archive(content/'Textures/Earth/Archive/Elev.tree')
        self.mods = Archive(content/'Textures/Earth/Archive/Elev_mod.tree')
        self.surface = Archive(content/'Textures/Earth/Archive/Surf.tree')
        self.key = geography(coast)
        self.grids = {}
        self.provenance = {}
        level, row, col = self.key
        self.ancestors = []
        available=False; index=None
        while level >= 4:
            index = self.surface.tiles.get((level, row, col))
            available = index is not None and self.surface.nodes[index][1] >= 128
            self.ancestors.append(dict(key=[level, row, col], index=index, payload=available))
            if available:
                break
            level, row, col = level-1, row//2, col//2
        require(available and index is not None, 'no independent Surf ancestor')
        self.ancestor = level, row, col
        archive = self.surface
        pos, length, *_ = archive.nodes[index]
        end = archive.nodes[index+1][0] if index+1 < len(archive.nodes) else archive.length
        archive.f.seek(archive.offset+pos)
        dds = zlib.decompress(archive.f.read(end-pos))
        require(len(dds) == length, 'independent surface inflation')
        self.texture = Image.open(io.BytesIO(dds)).convert('RGB')
        self.surface_identity = dict(path=str(archive.path.resolve()), key=self.ancestor,
                                     sha256=hashlib.sha256(dds).hexdigest(), bytes=length,
                                     fallback_walk=self.ancestors, dimensions=self.texture.size)

    def close(self):
        for archive in (self.base,self.mods,self.surface):
            archive.f.close()

    def __enter__(self):
        return self

    def __exit__(self,*_):
        self.close()

    def height(self, lng, lat):
        key = locate(self.base, lng, lat, 21)
        if key not in self.grids:
            base, mod = self.base.tile(key), self.mods.tile(key)
            self.grids[key] = grid(base, mod)[0]
            self.provenance[str(key)] = dict(base_sha256=base['sha256'], mod_sha256=mod['sha256'] if mod else None)
        return interpolate(self.grids[key], key, lng, lat, 2), key[0]+3

    def geometry(self, radius):
        verts = []
        level, row, col = self.key
        a, ar, ac = self.ancestor
        factor = 2**(level-a)
        for r in range(33):
            for c in range(33):
                lng, lat = coordinates(self.key, r, c)
                h, actual = self.height(lng, lat)
                uv = ((col+.001+.998*c/32)/factor-ac, (row+.001+.998*r/32)/factor-ar)
                verts.append(dict(lng=lng, lat=lat, h=h, actual=actual, p=point(radius, h, lng, lat), uv=uv))
        return verts


def projected(f, vertices):
    # Independent actual body/camera matrices. Anchor computed from reference geography.
    center, rotation = pose(f)
    out = []
    sy = 1/math.tan(f['aperture'])
    for v in vertices:
        x, y, z = [dot(row, v['p'])+center[i] for i, row in enumerate(rotation)]
        out.append((320+240*sy*x/z, 240-240*sy*y/z, z, *v['uv']))
    return out


def raster(vertices, stride=4):
    """Frontmost perspective-correct samples, selected without observed GPU data."""
    found = {}
    for row in range(32):
        for col in range(32):
            q = row*33+col
            for ids in ((q, q+33, q+1), (q+1, q+33, q+34)):
                vs = [vertices[i] for i in ids]
                ax, ay = vs[0][:2]; bx, by = vs[1][:2]; cx, cy = vs[2][:2]
                d = (by-cy)*(ax-cx)+(cx-bx)*(ay-cy)
                if abs(d) < 1e-10 or min(v[2] for v in vs) <= 10:
                    continue
                xmin, xmax = max(2, math.ceil(min(ax,bx,cx)-.5)), min(637, math.floor(max(ax,bx,cx)-.5))
                ymin, ymax = max(2, math.ceil(min(ay,by,cy)-.5)), min(477, math.floor(max(ay,by,cy)-.5))
                for y in range(ymin+(2-ymin)%stride, ymax+1, stride):
                    for x in range(xmin+(2-xmin)%stride, xmax+1, stride):
                        wa=((by-cy)*(x+.5-cx)+(cx-bx)*(y+.5-cy))/d
                        wb=((cy-ay)*(x+.5-cx)+(ax-cx)*(y+.5-cy))/d
                        w=(wa,wb,1-wa-wb)
                        if min(w)<0:
                            continue
                        rec=sum(t/v[2] for t,v in zip(w,vs)); depth=1/rec
                        if (x,y) in found and found[x,y][0] <= depth:
                            continue
                        uv=[sum(t*v[k]/v[2] for t,v in zip(w,vs))/rec for k in (3,4)]
                        found[x,y]=(depth, uv, min(w)>.08)
    return found


def oracle(f, snapshot, ref, vertices):
    image=Image.open(snapshot).convert('RGB')
    require(image.size==(640,480),'GPU snapshot dimensions')
    screens=projected(f,vertices)
    predicted=raster(screens)
    # Independent complete triangle-union silhouette; integer polygon coverage
    # differs from Metal's pixel-center top-left rule only at the bounded edge.
    mask=Image.new('L',(640,480)); draw=ImageDraw.Draw(mask)
    for row in range(32):
        for col in range(32):
            q=row*33+col
            for ids in ((q,q+33,q+1),(q+1,q+33,q+34)):
                draw.polygon([screens[i][:2] for i in ids],fill=255)
    interior=mask.filter(ImageFilter.MinFilter(5)).tobytes()
    exterior=mask.filter(ImageFilter.MaxFilter(5)).tobytes()
    missing=extra=0
    for i,rgb in enumerate(image.getdata()):
        missing+=bool(interior[i] and rgb==BG)
        extra+=bool(not exterior[i] and rgb!=BG)
    require(missing==0 and extra==0,
            f'GPU terrain geometry/imagery mismatch silhouette missing={missing} extra={extra} (2px boundary tolerance)')
    width,height=ref.texture.size
    samples=[]; errors=[]
    for (x,y),(depth,uv,safe) in sorted(predicted.items()):
        if not safe or not all(.15<(t*n)%1<.85 for t,n in zip(uv,(width,height))):
            continue
        texel=(min(width-1,int(uv[0]*width)),min(height-1,int(uv[1]*height)))
        expected=ref.texture.getpixel(texel); actual=image.getpixel((x,y))
        err=max(abs(a-b) for a,b in zip(expected,actual))
        samples.append(dict(x=x,y=y,depth=depth,uv=uv,texel=texel,expected=expected,actual=actual,error=err))
        if err>2:
            errors.append(samples[-1])
    Path(str(snapshot)+'.oracle.json').write_text(json.dumps(dict(samples=samples),indent=2)+'\n')
    require(len(samples)>=128,'insufficient independent GPU witnesses')
    # Same positive pixel gate used unchanged for flat/frozen/UV negatives.
    require(not errors,f'GPU terrain geometry/imagery mismatch {len(errors)}/{len(samples)} first={errors[:2]}')
    return dict(samples=len(samples),max_channel_error=max(s['error'] for s in samples),
                missing_interior=missing,extra_exterior=extra,boundary_tolerance_px=2),image


def numeric(f, g, ref, vertices):
    require(tuple(g['patch'])==ref.key and tuple(g['ancestor'])==ref.ancestor,'source address mismatch')
    require(len(g['vertices'])==1089,'terrain vertex count')
    lng,lat=coordinates(ref.key,16,16)
    anchor=point(f['radius'],0,lng,lat)
    require(max(abs(a-b) for a,b in zip(anchor,g['anchor']))<1e-8,'anchor independent reference')
    max_offset=max_bad=max_height=max_projection=0
    def f32(v):return struct.unpack('f',struct.pack('f',v))[0]
    predicted=projected(f,vertices)
    for v, uploaded, screen in zip(vertices,g['vertices'],predicted):
        h_error=abs(v['h']-uploaded[2]);max_height=max(max_height,h_error)
        require(h_error<1e-6 and v['actual']==uploaded[3] and uploaded[3]>0,'scalar elevation/actual level mismatch')
        offset=[a-b for a,b in zip(v['p'],anchor)]
        max_offset=max(max_offset,max(abs(a-b) for a,b in zip(offset,uploaded[4:7])))
        max_bad=max(max_bad,max(abs((f32(a)-f32(b))-c) for a,b,c in zip(v['p'],anchor,offset)))
        require(max(abs(a-b) for a,b in zip(v['uv'],uploaded[7:9]))<1e-7,'independent ancestor UV mismatch')
        p=uploaded[4:7]+[1]
        clip=[sum(f['bound'][c*4+r]*p[c] for c in range(4)) for r in range(4)]
        xy=(320+320*clip[0]/clip[3],240-240*clip[1]/clip[3])
        max_projection=max(max_projection,max(abs(a-b) for a,b in zip(xy,screen[:2])))
    require(max_offset<.002,'anchor offset precision lost')
    require(max_bad>.05,'radius-scale float precision negative not sensitive')
    require(max_projection<.02,'bound GPU projection mismatch')
    # Verify geometric normals independently of the SDK normal and GPU shading.
    minimum=1
    for row in range(32):
        for col in range(32):
            q=row*33+col
            for ia,ib,ic in ((q,q+33,q+1),(q+1,q+33,q+34)):
                p=vertices[ia]['p']; u=[a-b for a,b in zip(vertices[ic]['p'],p)]; v=[a-b for a,b in zip(vertices[ib]['p'],p)]
                n=[u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]]
                minimum=min(minimum,dot(n,p)/math.sqrt(dot(n,n)*dot(p,p)))
    require(minimum>0,'independent outward geometric normals')
    return dict(max_height_error_m=max_height,max_local_float_error_m=max_offset,
                radius_float_negative_error_m=max_bad,max_bound_projection_error_px=max_projection,
                minimum_normal_radial_dot=minimum,actual_levels=sorted(set(v['actual'] for v in vertices)),
                height_range_m=[min(v['h'] for v in vertices),max(v['h'] for v in vertices)])


def parse(log):
    frames=[json.loads(s.split('terrain_json=',1)[1]) for s in log.splitlines() if 'terrain_json=' in s]
    geometry=[json.loads(s.split('terrain_geometry_json=',1)[1]) for s in log.splitlines() if 'terrain_geometry_json=' in s]
    require(len(frames)==12 and len(geometry)==1,'actual terrain frame/geometry count')
    return frames,geometry[0]


def lifecycle(log):
    for marker in ('teardown complete; PASS','create=1 close=1 destroy=1 imgui_init=1 imgui_shutdown=1',
                   'completed=12 presented=0 readback=12 errors=0','hidden_checks=12',
                   'terrain_cache_released_before_world_close=1','terrain_resources_released=1','world_closed=1'):
        require(marker in log,'normal terrain lifecycle missing: '+marker)
    for marker in ('terrain_cache_released_before_world_close=1','terrain_resources_released=1'):
        require(log.index(marker)<log.index('world_closed=1'),'terrain resource survived world close')


def verify(log,snapshot,ref,controlled=True,check_numeric=True):
    frames,g=parse(log)
    vertices=ref.geometry(frames[0]['radius'])
    # Pixel gates FIRST: negative rejection must not rely on exported vertex disagreement.
    a,im1=oracle(frames[0],snapshot,ref,vertices)
    result=dict(first=a)
    if controlled:
        b,im2=oracle(frames[6],Path(str(snapshot)+'.view2.ppm'),ref,vertices)
        require(max(abs(a-b) for a,b in zip(frames[0]['cameraR'],frames[6]['cameraR']))>.1,'actual engine camera did not change')
        changed=sum(a!=b for a,b in zip(im1.getdata(),im2.getdata()))
        require(changed>1000,'GPU camera image unchanged')
        result.update(second=b,changed_pixels=changed)
    if check_numeric:
        result['geometry']=numeric(frames[0],g,ref,vertices)
        result['final_geometry']=numeric(frames[-1],g,ref,vertices)
        measured=float(re.search(r'minimum_radial_dot=(\S+)',log)[1])
        require(abs(measured-result['geometry']['minimum_normal_radial_dot'])<1e-8,'normal independent comparison')
    return result


def main():
    exe,content,output=(Path(p).resolve() for p in sys.argv[1:4]);output.mkdir(parents=True,exist_ok=True)
    launch=stage(content,output,'terrain-content-')
    results=dict(runs=[],negatives=[],commands=[],provenance={})
    for rel in ('Orbiter.cfg','Config/Earth.cfg','Modules/Celbody/Sun.dylib','Modules/Celbody/Earth.dylib','Modules/Vsop87.dylib','Config/Earth/Data/Vsop87B.dat'):
        p=launch/rel;raw=p.read_bytes();results['provenance'][rel]=dict(path=str(p.resolve()),sha256=hashlib.sha256(raw).hexdigest())
    refs={c:Reference(launch,c) for c in (False,True)}
    def run(name,coast,option=None,controlled=True):
        snapshot=output/(name+'.ppm')
        cmd=[str(exe),'MetalEarth','12','--terrain-patch-test' if controlled else '--terrain-patch',str(snapshot)]
        if coast:cmd.append('--terrain-coast')
        if option:cmd.append(option)
        p=subprocess.run(cmd,cwd=launch,capture_output=True,text=True,timeout=90)
        log=p.stdout+p.stderr;(output/(name+'.log')).write_text(log)
        results['commands'].append(dict(argv=cmd,cwd=str(launch),exit=p.returncode))
        require(p.returncode==0,'renderer failed '+name+': '+log[-1800:])
        lifecycle(log)
        engine=(launch/'Orbiter_core_metal.log').read_text();(output/(name+'.engine.log')).write_text(engine)
        for theory,body in (('E','Sun'),('B','Earth')):
            m=re.search(r'VSOP87\('+theory+r'\) '+body+r': Precision \S+, Terms (\d+)/(\d+)',engine)
            require(m is not None and int(m[1])>0,'native ephemeris initialization')
        configured=re.search(r'terrain_source path=(.*?) patch=',log)[1]
        require((launch/configured/'Archive/Surf.tree').resolve()==refs[coast].surface.path.resolve(),'actual engine configured surface path')
        return log,snapshot
    for coast in (False,True):
        for repeat in range(3):
            name=('coast' if coast else 'mountain')+'-'+str(repeat)
            log,snapshot=run(name,coast)
            result=verify(log,snapshot,refs[coast]);results['runs'].append(dict(name=name,**result));print(json.dumps(dict(name=name,**result)),flush=True)
    log,snapshot=run('fixed-camera',False,controlled=False)
    results['runs'].append(dict(name='fixed-camera',**verify(log,snapshot,refs[False],False)))
    for name,option in (('flat','--diagnostic-flat-terrain'),('frozen','--diagnostic-freeze-terrain-transform'),('uv','--diagnostic-swap-terrain-uv')):
        log,snapshot=run('negative-'+name,False,option)
        frames,_=parse(log)
        require(max(abs(a-b) for a,b in zip(frames[0]['cameraR'],frames[6]['cameraR']))>.1,'negative retained actual camera change')
        if name=='frozen':require(all(f['bound']==frames[0]['bound'] for f in frames),'actual bound matrix not frozen')
        try:verify(log,snapshot,refs[False],check_numeric=False)
        except AssertionError as e:
            require('GPU terrain geometry/imagery mismatch' in str(e),'negative rejected outside GPU gate: '+str(e))
            child=subprocess.run([sys.executable,str(Path(__file__).resolve()),'--oracle',str(output/('negative-'+name+'.log')),str(snapshot),str(launch)],capture_output=True,text=True,timeout=90)
            (output/('negative-'+name+'.oracle.log')).write_text(child.stdout+child.stderr)
            require(child.returncode!=0 and 'GPU terrain geometry/imagery mismatch' in child.stderr,'same standalone oracle did not reject')
            result=dict(name=name,renderer_exit=0,oracle_exit=child.returncode,reason=str(e));results['negatives'].append(result);print(json.dumps(result),flush=True)
        else:raise AssertionError('GPU causal control accepted '+name)
    for coast,ref in refs.items():
        results['coast' if coast else 'mountain']=dict(surface=ref.surface_identity,elevation=ref.provenance)
    positive=Image.open(output/'mountain-0.ppm');flat=Image.open(output/'negative-flat.ppm')
    changed=sum(a!=b for a,b in zip(positive.getdata(),flat.getdata()))
    silhouette=sum((a==BG)!=(b==BG) for a,b in zip(positive.getdata(),flat.getdata()))
    require(changed>10000 and silhouette>1000,'insufficient unexaggerated actual mountain GPU relief impact')
    results['actual_relief_impact']=dict(changed_pixels=changed,silhouette_pixels=silhouette)
    (output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    for ref in refs.values():ref.close()
    print('PASS: bounded real-engine mountain + KSC terrain; independent scalar/anchored geometry/UV/GPU pixels; flat/frozen/UV negatives rejected; '+json.dumps(results['actual_relief_impact']))


if __name__=='__main__':
    if len(sys.argv)>1 and sys.argv[1]=='--oracle':
        log,snapshot,content=map(Path,sys.argv[2:5])
        with Reference(content,False) as ref:
            verify(log.read_text(),snapshot,ref,check_numeric=False)
    else:main()
