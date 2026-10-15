#!/usr/bin/env python3
"""Independent scalar CPU triangle/UV + Pillow DDS oracle for actual GPU PPMs."""
import hashlib
import json
import math
import re
from pathlib import Path
import subprocess
import sys
from PIL import Image
from material_regression import stage
from camera_regression import verify_frame, verify_presentation
from mesh_regression import require

BG = bytes((32, 64, 128))
HEADER = b'P6\n640 480\n255\n'


def pixels(path):
    data = path.read_bytes()
    require(data.startswith(HEADER) and len(data) == len(HEADER)+640*480*3, 'texture PPM layout')
    return data[len(HEADER):]


def oracle(snapshot, geometry_path, frame, evidence_path=None):
    """Rasterize sparse pixel centers independently, perspective-correct UV/shade.

    Engine-uploaded positions/UV/index/material are evidence inputs, not .msh
    parsing. Pillow independently decodes the exact selected source DDS files.
    Exclude triangle edges and nearest-texel boundaries to avoid raster tie rules.
    """
    geometry = json.loads(geometry_path.read_text())
    matrix = frame['clip_matrix']
    vertices = geometry['vertices']
    projected = []
    for v in vertices:
        p = v[:3]+[1]
        c = [sum(matrix[col*4+row]*p[col] for col in range(4)) for row in range(4)]
        require(c[3] > 0, 'fixture crosses eye plane; CPU oracle unsupported')
        projected.append(((c[0]/c[3]+1)*320, (1-c[1]/c[3])*240, c[2]/c[3], 1/c[3]))
    indices = geometry['indices']
    samples = {}
    depths = {}
    # Fixed sparse raster, not coordinates selected because they happened to match.
    for r in geometry['ranges']:
        for first in range(r['first'], r['first']+r['count'], 3):
            ids = indices[first:first+3]
            a, b, c = [projected[i] for i in ids]
            den = (b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
            if abs(den) < 1e-9:
                continue
            minx = max(0, math.ceil((min(a[0], b[0], c[0])-.5-6)/13))
            maxx = min(48, math.floor((max(a[0], b[0], c[0])-.5-6)/13))
            miny = max(0, math.ceil((min(a[1], b[1], c[1])-.5-6)/13))
            maxy = min(36, math.floor((max(a[1], b[1], c[1])-.5-6)/13))
            for gy in range(miny, maxy+1):
                for gx in range(minx, maxx+1):
                    x, y = 6+gx*13, 6+gy*13
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
                    perspective = [w*p[3] for w, p in zip(weights, (a, b, c))]
                    total = sum(perspective)
                    attrs = [sum(w*vertices[i][channel] for w, i in zip(perspective, ids))/total for channel in range(3, 10)]
                    samples[x, y] = dict(z=z, edge=min(weights), attrs=attrs, texture=r['texture'], triangle=first//3)
    source_images, sources, records = {}, {}, []
    actual = pixels(snapshot)
    mismatches = []
    chromatic = 0
    for (x, y), s in sorted(samples.items()):
        ordered_depths = sorted(depths[x, y])
        # Independently exclude near-coplanar layers within four Depth32Float
        # ULP at this fixture's ~1.0 depth, not pixels chosen by observed value.
        if len(ordered_depths) > 1 and ordered_depths[1]-ordered_depths[0] < 4*2**-24:
            continue
        if s['edge'] < .03 or not s['texture']:
            continue
        path = s['texture']
        if path not in source_images:
            source_images[path] = Image.open(path).convert('RGBA')
            sources[path] = dict(sha256=hashlib.sha256(Path(path).read_bytes()).hexdigest(), dimensions=source_images[path].size)
        texture = source_images[path]
        shade, u, v, red, green, blue, alpha = s['attrs']
        tx, ty = (u % 1)*texture.width, (v % 1)*texture.height
        if min(tx % 1, 1-tx % 1, ty % 1, 1-ty % 1) < .04:
            continue
        texel = texture.getpixel((math.floor(tx), math.floor(ty)))
        expected = [round(texel[c]*shade*d) for c, d in enumerate((red, green, blue))]
        offset = (y*640+x)*3
        observed = list(actual[offset:offset+3])
        error = max(abs(a-b) for a, b in zip(expected, observed))
        record = dict(x=x, y=y, triangle=s['triangle'], uv=[u, v], texel=texel, shade=shade,
                      diffuse=[red, green, blue], expected=expected, actual=observed, max_error=error, texture=path)
        records.append(record)
        chromatic += max(expected)-min(expected) > 10
        if error > 2:
            mismatches.append(record)
    result = dict(samples=len(records), chromatic_samples=chromatic, mismatches=len(mismatches),
                  sources=sources, records=records, image_sha256=hashlib.sha256(actual).hexdigest())
    if evidence_path:
        evidence_path.write_text(json.dumps(result, indent=2))
    require(len(records) > 80 and chromatic > 20, 'insufficient independent texture/UV samples')
    require(not mismatches, 'CPU DDS/UV/diffuse sample mismatch: '+json.dumps(mismatches[:3]))
    return result


def main():
    if len(sys.argv) > 1 and sys.argv[1] == '--oracle':
        snapshot, geometry, log, output = map(Path, sys.argv[2:6])
        frame = next(json.loads(line.split('camera_json=')[1]) for line in log.read_text().splitlines() if 'camera_json=' in line)
        oracle(snapshot, geometry, frame, output)
        return
    executable, content, output = [Path(p).resolve() for p in sys.argv[1:4]]
    present = sys.argv[4:] == ['--hidden-present']
    require(not sys.argv[4:] or present, 'runner options')
    output.mkdir(parents=True, exist_ok=True)
    launch = stage(content, output)
    signatures = []
    for run in range(3):
        snapshots = {}
        for mode in ('--textures-test', '--textures-ignore-sampling-test'):
            stem = f'{mode[2:]}-{run}'
            snapshot, log_path = output/(stem+'.ppm'), output/(stem+'.log')
            process = subprocess.run([str(executable), 'MetalCamera', '12', mode, str(snapshot)] + (['--hidden-present'] if present else []),
                                     cwd=launch, capture_output=True, text=True, timeout=90)
            log = process.stdout+process.stderr
            log_path.write_text(log)
            require(process.returncode == 0, stem+' engine failed: '+log[-4000:])
            for marker in ('teardown complete; PASS', 'create=1 close=1 destroy=1 imgui_init=1 imgui_shutdown=1',
                           'engine_materials=27 engine_textures=10 nonnull_texture_handles=10',
                           'texture_mesh_pins_released=1 before_engine_teardown=1', 'texture_store_destroyed=1',
                           'invalid_releases=0', 'group_draws=1464', 'completed=12 presented=0 readback=12 errors=0'):
                # Hidden timestamps can be >0; use the separate presentation contract.
                if present and marker.startswith('completed='):
                    continue
                require(marker in log, 'missing '+marker)
            if present:
                verify_presentation(log)
            frames = [json.loads(line.split('camera_json=')[1]) for line in log.splitlines() if 'camera_json=' in line]
            gpu = [json.loads(line.split('gpu_clip_json=')[1]) for line in log.splitlines() if 'gpu_clip_json=' in line]
            require(len(frames) == len(gpu) == 12, 'camera record count')
            for frame, bound in zip(frames, gpu):
                verify_frame(frame)
                require(frame['clip_matrix'] == bound, 'GPU camera uniform')
            geometry_path = Path(str(snapshot)+'.geometry.json')
            slots = {int(slot): path for slot, path in re.findall(r'texture_slot slot=(\d+) handle=\S+ path=(.*)', log)}
            require(len(slots) == 10 and Path(slots[1]).name.lower() == 'dgmk4_4.dds' and
                    Path(slots[2]).name.lower() == 'dgmk4_1.dds', 'actual DG template slot provenance')
            groups = [json.loads(line.split('material_json=')[1]) for line in log.splitlines() if 'material_json=' in line]
            ranges = json.loads(geometry_path.read_text())['ranges']
            require(len(groups) == len(ranges) == 122, 'DG group/range fixture count')
            previous = 0
            for group, draw in zip(groups, ranges):
                raw = group['texture_index']
                slot = previous if raw == 0xfffffffe else 0 if raw == 0xffffffff else raw+1
                require(draw['texture'] == (slots[slot] if slot else ''), 'group zero-based texture index to one-based API slot binding')
                previous = slot
            if mode == '--textures-test':
                for suffix, frame in (('', frames[0]), ('.view2.ppm', frames[6])):
                    result = oracle(Path(str(snapshot)+suffix), geometry_path, frame, output/(stem+suffix+'.oracle.json'))
                    signatures.append((suffix, result['image_sha256']))
                    print(json.dumps(dict(run=run, view=suffix or 'first', **{key: result[key] for key in ('samples', 'chromatic_samples', 'mismatches', 'image_sha256')})), flush=True)
            else:
                # The exact same scene and engine-owned handles, with actual GPU
                # sampling disabled, must fail the independent oracle as a process.
                negative = subprocess.run([sys.executable, __file__, '--oracle', str(snapshot), str(geometry_path), str(log_path),
                                           str(output/(stem+'.oracle.json'))], capture_output=True, text=True)
                (output/(stem+'.negative.log')).write_text(negative.stdout+negative.stderr)
                require(negative.returncode != 0 and 'CPU DDS/UV/diffuse sample mismatch' in negative.stderr,
                        'ignored-sampling negative oracle did not fail for texture mismatch')
            snapshots[mode] = snapshot
        positive, ignored = [pixels(snapshots[m]) for m in ('--textures-test', '--textures-ignore-sampling-test')]
        mask = lambda p: [p[i:i+3] == BG for i in range(0, len(p), 3)]
        require(mask(positive) == mask(ignored), 'sampling changed scene silhouette')
        changed = sum(positive[i:i+3] != ignored[i:i+3] for i in range(0, len(positive), 3))
        require(changed > 10000, 'texture-specific GPU difference missing')
        print(json.dumps(dict(run=run, changed_from_ignored_sampling=changed, negative_oracle_exit=negative.returncode)), flush=True)
    require(len(set(signatures)) == 2, 'nondeterministic texture snapshots')
    print('PASS: 3 paired engine/GPU texture lifecycles; sparse independent Pillow DDS/perspective UV/diffuse oracle; ignored-sampling negative process rejected')


if __name__ == '__main__':
    main()
