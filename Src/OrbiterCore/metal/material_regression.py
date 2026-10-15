#!/usr/bin/env python3
"""Real-engine opaque diffuse-material slice; mesh-only GPU evidence, no textures."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
from fixture_staging import stage
from camera_regression import image, verify_frame, verify_presentation
from mesh_regression import require


def material_image(path, palette):
    data = path.read_bytes()
    header = b'P6\n640 480\n255\n'
    require(data.startswith(header) and len(data) == len(header)+640*480*3, 'PPM layout')
    pixels = data[len(header):]
    colors = [tuple(pixels[i:i+3]) for i in range(0, len(pixels), 3)]
    background = colors.count((32, 64, 128))
    colored = sum(len(set(c)) > 1 and c != (32, 64, 128) for c in colors)
    # DG's exposed nonwhite material in these actual static views is material 24,
    # neutral gray (.235,.235,.235). The colored interior materials are occluded.
    # Its <80 pixels cannot come from the old grayscale shade oracle [89,242].
    material_specific = sum(c != (32,64,128) and (len(set(c)) > 1 or max(c) < 80) for c in colors)
    # Each channel is diffuse*interpolated shade with a half-UNORM-LSB tolerance.
    # Intersect possible shade intervals: tests color ratios, not just non-gray output.
    def allowed(c):
        if c == (32, 64, 128):
            return True
        for diffuse in palette:
            low, high = .34999, .95001
            for channel, d in zip(c, diffuse):
                if d == 0:
                    if channel != 0:
                        break
                else:
                    low = max(low, (channel-.51)/(255*d))
                    high = min(high, (channel+.51)/(255*d))
            else:
                if low <= high:
                    return True
        return False
    require(all(allowed(c) for c in set(colors)), 'non-material palette (wrong material, UI or shader)')
    require(background > 1000 and len(colors)-background > 1000, 'missing material mesh/background')
    require(material_specific > 100, 'material shader produced no material-specific geometry')
    return pixels, background, colored, material_specific


def main():
    executable, content, output = [Path(x).resolve() for x in sys.argv[1:4]]
    require(all(x == '--hidden-present' for x in sys.argv[4:]) and len(sys.argv[4:]) <= 1, 'runner options')
    present = bool(sys.argv[4:])
    output.mkdir(parents=True, exist_ok=True)
    launch = stage(content, output)
    signatures = []
    for run in range(3):
        snapshots, logs = {}, {}
        for mode in ('--materials-reference-test', '--materials-test'):
            name = f'{mode[2:]}-{run}'
            snapshot = output/(name+'.ppm')
            result = subprocess.run([str(executable), 'MetalCamera', '12', mode, str(snapshot)] + (['--hidden-present'] if present else []), cwd=launch, text=True, capture_output=True, timeout=60)
            log = result.stdout+result.stderr
            (output/(name+'.log')).write_text(log)
            require(result.returncode == 0, name+' executable failed: '+log)
            for marker in ('teardown complete; PASS', 'create=1 close=1 destroy=1 imgui_init=1 imgui_shutdown=1', 'camera_frames=12', 'draw_calls=12 drawn_triangles=141636 geometry_readbacks=12 visual_register=1 visual_unregister=1'):
                require(marker in log, 'missing '+marker)
            if present:
                verify_presentation(log)
            else:
                require('completed=12 presented=0 readback=12 errors=0' in log, 'GPU lifecycle')
            frames = [json.loads(line.split('camera_json=')[1]) for line in log.splitlines() if 'camera_json=' in line]
            gpu = [json.loads(line.split('gpu_clip_json=')[1]) for line in log.splitlines() if 'gpu_clip_json=' in line]
            require(len(frames) == len(gpu) == 12, 'camera record count')
            for f, g in zip(frames, gpu):
                verify_frame(f)
                require(g == f['clip_matrix'], 'GPU camera binding')
            snapshots[mode], logs[mode] = snapshot, log
        log = logs['--materials-test']
        require('materials_mode=opaque-diffuse-only textures=unsupported' in log, 'explicit mode limitation')
        require('material_resources_released=1 before_engine_teardown=1' in log, 'resource lifetime')
        records = [json.loads(line.split('material_json=')[1]) for line in log.splitlines() if 'material_json=' in line]
        require(len(records) == 122, 'actual group material provenance count')
        require([r['group'] for r in records] == list(range(122)), 'engine group provenance sequence')
        require(records[0]['resolved'] == 0 and records[0]['diffuse'] == [1,1,1,1], 'zero-based engine material')
        require(records[121]['resolved'] == 24 and abs(records[121]['diffuse'][0]-.235) < 1e-7,
                'exposed DG material fixture changed')
        require('engine_materials=27 engine_textures=10 nonnull_texture_handles=0' in log, 'engine texture dependency evidence')
        require('texture_load_callbacks=124 texture_successes=0 textures_supported=0' in log, 'unsupported callback accounting')
        require(all(r['source'] in ('engine', 'default') for r in records), 'fabricated material')
        palette = list({tuple(r['diffuse'][:3]) for r in records})
        first = None
        for suffix in ('', '.view2.ppm'):
            gray, bg, geo = image(Path(str(snapshots['--materials-reference-test'])+suffix))
            color, cbg, chromatic, material_specific = material_image(Path(str(snapshots['--materials-test'])+suffix), palette)
            require(bg == cbg, 'material mode changed background/geometry coverage')
            graymask = [gray[i:i+3] == bytes((32,64,128)) for i in range(0,len(gray),3)]
            colormask = [color[i:i+3] == bytes((32,64,128)) for i in range(0,len(color),3)]
            require(graymask == colormask, 'material mode changed geometry silhouette')
            changed = sum(gray[i:i+3] != color[i:i+3] for i in range(0,len(gray),3))
            require(changed > 100, 'materials not used by GPU')
            require(changed == material_specific, 'unexpected white-material changes')
            require(changed == (343 if suffix else 445), 'DG material coverage fixture changed')
            diffuse = records[121]['diffuse'][0]
            require(all(max(abs(color[i+c]-gray[i+c]*diffuse) for c in range(3)) <= 1
                        for i in range(0,len(color),3) if color[i:i+3] != gray[i:i+3]),
                    'GPU material multiplication differs from actual engine diffuse')
            if first is None:
                first = color
            else:
                require(sum(first[i:i+3] != color[i:i+3] for i in range(0,len(color),3)) > 1000, 'material camera viewpoint unchanged')
            signature = hashlib.sha256(color).hexdigest()
            signatures.append((suffix, signature))
            print(json.dumps(dict(run=run, view=suffix or 'first', background=cbg, chromatic=chromatic, material_specific=material_specific, changed_from_gray=changed, sha256=signature)), flush=True)
        # Sensitivity: unchanged grayscale must not pass the material oracle.
        try:
            material_image(snapshots['--materials-reference-test'], palette)
        except RuntimeError:
            pass
        else:
            raise RuntimeError('grayscale negative control accepted as material rendering')
    require(len(set(signatures)) == 2, 'material GPU output not deterministic')
    print('PASS: 3 paired real-engine lifecycles; two-view material palette/silhouette/GPU checks; grayscale negative rejected; textures unsupported')


if __name__ == '__main__':
    main()
