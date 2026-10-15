#!/usr/bin/env python3
"""DG mesh-0 gear only: scalar source rotation + independent sparse GPU oracle."""
import json
import math
from pathlib import Path
import subprocess
import sys
from PIL import Image
from fixture_staging import stage
from mesh_regression import require
from camera_regression import verify_frame
from texture_regression import pixels, BG


def records(log, key):
    return [json.loads(line.split(key+'=', 1)[1]) for line in log.splitlines() if key+'=' in line]


def cross(a, b):
    return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]


def transform(point, definitions, state, group, normal=False):
    # Scalar quaternion vector action, not the renderer's matrix constructor or
    # composed group matrices. Preserve source axis length, even DG's .052/.999.
    p = list(point)
    for r in reversed(definitions):  # actual DG default 1 -> target <=1
        if group not in r['groups']:
            continue
        ds = (min(r['end'], max(r['start'], state))-r['end'])/(r['end']-r['start'])
        half = ds*r['angle']/2
        q = [x*math.sin(half) for x in r['axis']]
        w = math.cos(half)
        pivot = [0, 0, 0] if normal else r['pivot']
        v = [x-y for x, y in zip(p, pivot)]
        a = cross(q, v)
        b = cross(q, a)
        p = [v[i]+2*w*a[i]+2*b[i]+pivot[i] for i in range(3)]
    return p


def project(p, frame):
    # Engine camera/world doubles, independent of the uploaded clip matrix.
    vr, cr = frame['vessel_rotation'], frame['camera_rotation']
    relative = [frame['vessel_position'][i]-frame['camera_position'][i] for i in range(3)]
    world = [sum(vr[r*3+c]*p[c] for c in range(3))+relative[r] for r in range(3)]
    x, y, z = [sum(cr[c*3+r]*world[c] for c in range(3)) for r in range(3)]
    require(z > 0, 'gear oracle eye plane')
    sy = 1/math.tan(frame['aperture'])
    near, far = frame['near'], frame['far']
    a = far/(far-near)
    return ((x/z*sy*.75+1)*320, (1-y/z*sy)*240, a-near*a/z, 1/z)


def predict(source, geometry, frame, state, bound):
    vertices = [list(v) for v in geometry['vertices']]
    gear_groups = {g for r in source['definitions'] for g in r['groups']}
    moving, invariant, max_error = 0, 0, 0
    group_for_triangle = {}
    for draw in source['draws']:
        g = draw['group']
        m = bound[g]
        for j, v in enumerate(draw['vertices']):
            expected = transform(v[:3], source['definitions'], state, g)
            normal = transform(v[3:6], source['definitions'], state, g, True)
            actual = [sum(m[c*4+r]*v[c] for c in range(3))+m[12+r] for r in range(3)]
            error = max(abs(x-y) for x, y in zip(expected, actual))
            max_error = max(max_error, error)
            if g not in gear_groups:
                invariant += 1
                require(expected == v[:3] and actual == v[:3], 'non-gear geometry transformed')
            else:
                moving += 1
            p = [expected[i]+source['offset'][i] for i in range(3)]
            shade = max(.35, min(.95, .35+.6*abs(sum(x*y for x, y in zip(normal, [.48, .6, .64])))))
            vertices[draw['base']+j][:4] = p+[shade]
        for first in range(draw['first'], draw['first']+draw['count'], 3):
            group_for_triangle[first//3] = g
    projected = [project(v[:3], frame) for v in vertices]
    indices = geometry['indices']
    samples, depths = {}, {}
    # Pixel grid fixed before looking at GPU data. Two-pixel spacing exposes
    # individual gear covers/struts; depth/edge rejection is purely geometric.
    for draw in geometry['ranges']:
        for first in range(draw['first'], draw['first']+draw['count'], 3):
            ids = indices[first:first+3]
            a, b, c = [projected[i] for i in ids]
            den = (b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
            if abs(den) < 1e-9:
                continue
            xmin = max(0, math.ceil((min(a[0], b[0], c[0])-1.5)/2))
            xmax = min(319, math.floor((max(a[0], b[0], c[0])-1.5)/2))
            ymin = max(0, math.ceil((min(a[1], b[1], c[1])-1.5)/2))
            ymax = min(239, math.floor((max(a[1], b[1], c[1])-1.5)/2))
            for gy in range(ymin, ymax+1):
                for gx in range(xmin, xmax+1):
                    x, y = 1+gx*2, 1+gy*2
                    px, py = x+.5, y+.5
                    l0 = ((b[1]-c[1])*(px-c[0])+(c[0]-b[0])*(py-c[1]))/den
                    l1 = ((c[1]-a[1])*(px-c[0])+(a[0]-c[0])*(py-c[1]))/den
                    weights = (l0, l1, 1-l0-l1)
                    if min(weights) < 0:
                        continue
                    z = sum(w*p[2] for w, p in zip(weights, (a, b, c)))
                    depths.setdefault((x, y), []).append(z)
                    if not 0 <= z <= 1 or ((x, y) in samples and z >= samples[x, y]['z']):
                        continue
                    ws = [w*p[3] for w, p in zip(weights, (a, b, c))]
                    total = sum(ws)
                    attrs = [sum(w*vertices[i][channel] for w, i in zip(ws, ids))/total for channel in range(3, 10)]
                    samples[x, y] = dict(z=z, edge=min(weights), attrs=attrs, texture=draw['texture'], group=group_for_triangle[first//3])
    images, prediction = {}, {}
    for key, sample in samples.items():
        zs = sorted(depths[key])
        if sample['edge'] < .04 or (len(zs)>1 and zs[1]-zs[0] < 4*2**-24):
            continue
        shade, u, v, red, green, blue, _ = sample['attrs']
        texel = (255, 255, 255)
        if sample['texture']:
            path = sample['texture']
            if path not in images:
                images[path] = Image.open(path).convert('RGBA')
            image = images[path]
            tx, ty = (u % 1)*image.width, (v % 1)*image.height
            if min(tx % 1, 1-tx % 1, ty % 1, 1-ty % 1) < .04:
                continue
            texel = image.getpixel((math.floor(tx), math.floor(ty)))
        prediction[key] = (tuple(round(texel[i]*shade*d) for i, d in enumerate((red, green, blue))), sample['group'] in gear_groups)
    return prediction, dict(gear_vertices=moving, invariant_vertices=invariant, max_bound_error=max_error)


def oracle(snapshot, log_path, evidence_path):
    log = log_path.read_text()
    source = json.loads(Path(str(snapshot)+'.gear-source.json').read_text())
    geometry = json.loads(Path(str(snapshot)+'.geometry.json').read_text())
    frames, bound = records(log, 'camera_json'), records(log, 'gear_frame_json')
    require(len(frames) == len(bound) == 12, 'gear frame counts')
    require([b['state'] for b in bound] == [0]*6+[1]*6 and all(b['readback']==b['state'] and b['template_unchanged'] for b in bound), 'engine gear state sequence')
    require(all(f['clip_matrix']==frames[0]['clip_matrix'] for f in frames), 'gear camera changed')
    require(log.count('gear_after_gpu frame=')==12 and log.count('unchanged=1\n')>=12, 'engine state retained after GPU completion')
    for i, b in enumerate(bound):
        require(b['bound']==bound[0 if i<6 else 6]['bound'], 'bound transforms changed within one engine state')
    for f in frames:
        verify_frame(f)
    # Bound matrices are checked separately below; CPU/GPU image predictions use
    # definitions, source vertices and engine camera, never bound transforms.
    predictions, checks, matrix_failure = [], [], None
    for state, frame in ((0, frames[0]), (1, frames[6])):
        # Predictions never use bound matrices. Keep their numeric disagreement
        # as separate evidence and still exercise the independent image oracle.
        prediction, check = predict(source, geometry, frame, state, bound[state*6]['bound'])
        if check['max_bound_error'] >= 4e-5:
            matrix_failure = 'bound group transform disagrees with independent source evaluation'
        predictions.append(prediction)
        checks.append(check)
    gpu = [pixels(snapshot), pixels(Path(str(snapshot)+'.view2.ppm'))]
    mismatches, gear_samples, static_samples, motion_samples = [], 0, 0, 0
    for state in (0, 1):
        for (x, y), (expected, gear) in predictions[state].items():
            offset = (y*640+x)*3
            actual = tuple(gpu[state][offset:offset+3])
            if max(abs(a-b) for a, b in zip(expected, actual)) > 3:
                mismatches.append(dict(state=state,x=x,y=y,expected=expected,actual=actual,gear=gear))
            gear_samples += gear
    for key in predictions[0].keys() & predictions[1].keys():
        p0, p1 = predictions[0][key], predictions[1][key]
        offset = (key[1]*640+key[0])*3
        a, b = [g[offset:offset+3] for g in gpu]
        if not p0[1] and not p1[1] and p0[0]==p1[0]:
            require(a==b, 'non-gear GPU geometry changed')
            static_samples += 1
        elif (p0[1] or p1[1]) and max(abs(a-b) for a,b in zip(p0[0],p1[0])) > 12:
            motion_samples += 1
            if a==b:
                mismatches.append(dict(x=key[0],y=key[1],reason='predicted moving gear frozen'))
    evidence = dict(gear_samples=gear_samples,static_samples=static_samples,motion_samples=motion_samples,mismatches=len(mismatches),matrix_failure=matrix_failure,checks=checks,first_mismatches=mismatches[:12])
    evidence_path.write_text(json.dumps(evidence,indent=2))
    require(gear_samples>20 and static_samples>100 and motion_samples>10, 'insufficient independently predicted gear motion samples: '+str(evidence))
    require(not mismatches, 'CPU/GPU gear motion mismatch: '+str(evidence))
    require(not matrix_failure, matrix_failure or '')
    return evidence


def main():
    if sys.argv[1:] and sys.argv[1]=='--oracle':
        oracle(*map(Path,sys.argv[2:5])); return
    executable, content, output = [Path(x).resolve() for x in sys.argv[1:4]]
    output.mkdir(parents=True,exist_ok=True)
    launch = stage(content,output)
    signatures = []
    for run in range(3):
        for frozen in (False,True):
            stem = f'gear-{run}-{int(frozen)}'
            snapshot, log_path = output/(stem+'.ppm'), output/(stem+'.log')
            mode = '--gear-animation-frozen-test' if frozen else '--gear-animation-test'
            result = subprocess.run([str(executable),'MetalCamera','12','--textures',str(snapshot),mode],cwd=launch,capture_output=True,text=True,timeout=90)
            log=result.stdout+result.stderr
            log_path.write_text(log)
            require(result.returncode==0,'gear engine failed: '+log[-4000:])
            for marker in ('teardown complete; PASS','completed=12 presented=0 readback=12 errors=0','create=1 close=1 destroy=1 imgui_init=1 imgui_shutdown=1','gear_attach id=7 default=1 loaded=0 definitions=14 initial_evaluated=1','gear_device_lookup exact=1 foreign_visual_rejected=1 foreign_index_rejected=1 get_edit=unsupported','gear_detach stale_handle_rejected=1 template_unchanged=1 state_restored=1','texture_mesh_pins_released=1 before_engine_teardown=1','invalid_releases=0','visual_register=1 visual_unregister=1'):
                require(marker in log,'missing '+marker)
            evidence_path=output/(stem+'.oracle.json')
            if frozen:
                negative=subprocess.run([sys.executable,__file__,'--oracle',str(snapshot),str(log_path),str(evidence_path)],capture_output=True,text=True)
                (output/(stem+'.negative.log')).write_text(negative.stdout+negative.stderr)
                require(negative.returncode!=0 and 'CPU/GPU gear motion mismatch' in negative.stderr,'frozen GPU transform oracle did not reject actual motion')
                require(pixels(snapshot)==pixels(Path(str(snapshot)+'.view2.ppm')),'negative was not actually frozen')
                print(json.dumps(dict(run=run,frozen_negative_exit=negative.returncode)),flush=True)
            else:
                evidence=oracle(snapshot,log_path,evidence_path)
                signatures.append((pixels(snapshot),pixels(Path(str(snapshot)+'.view2.ppm'))))
                print(json.dumps(dict(run=run,**evidence)),flush=True)
    require(all(s==signatures[0] for s in signatures),'gear repeats nondeterministic')
    print('PASS: 3 real engine DG gear GPU lifecycles + 3 actual frozen-transform negative oracle processes; not general animation or gear physics')


if __name__=='__main__':
    main()
