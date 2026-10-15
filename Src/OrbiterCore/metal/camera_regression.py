#!/usr/bin/env python3
"""Real camera inputs, independent double projection, GPU output and lifecycle."""
import json
import math
import re
from pathlib import Path
import subprocess
import sys
from fixture_staging import stage
from mesh_regression import require


def dot(a, b):
    return sum(x*y for x, y in zip(a, b))


def verify_frame(f):
    c, r = f['camera_rotation'], f['vessel_rotation']
    delta = [v-p for v, p in zip(f['vessel_position'], f['camera_position'])]
    axes = [c[i::3] for i in range(3)]
    for i in range(3):
        for j in range(3):
            require(abs(dot(axes[i], axes[j]) - (i == j)) < 1e-9, 'camera not orthonormal')
    require(max(abs(a-b) for a, b in zip(axes[2], f['direction'])) < 1e-9, 'camera +Z direction convention')
    # Independent scalar world-point -> camera -> perspective calculation.
    p = f['sample_local']
    world_relative = [dot(r[i*3:i*3+3], p)+delta[i] for i in range(3)]
    x, y, z = [dot(axis, world_relative) for axis in axes]
    sy = 1/math.tan(f['aperture'])
    near, far = f['near'], f['far']
    expected = [x*sy*480/640, y*sy, (z-near)*far/(far-near), z]
    m = f['clip_matrix']  # column-major computed float matrix; actual binding checked separately
    actual = [sum(m[col*4+row]*([*p, 1][col]) for col in range(4)) for row in range(4)]
    require(max(abs(a-b) for a, b in zip(expected, actual)) < 2e-4, 'independent world/camera/clip projection mismatch')
    # Check every matrix column using independent scalar projections, not only one vertex.
    for col in range(4):
        local = [float(col == i) for i in range(3)]
        w = [dot(r[i*3:i*3+3], local)+(delta[i] if col == 3 else 0) for i in range(3)]
        vx, vy, vz = [dot(axis, w) for axis in axes]
        out = [vx*sy*480/640, vy*sy, far/(far-near)*(vz-(near if col == 3 else 0)), vz]
        require(max(abs(out[row]-m[col*4+row]) for row in range(4)) < 2e-4, 'matrix column mismatch')
    require(0 < near < far and 0 < f['aperture'] < math.pi/2, 'invalid projection limits')
    require(max(abs(v) for v in f['vessel_position']) > 1e11, 'fixture does not exercise heliocentric precision')


def image(path):
    data = path.read_bytes()
    header = b'P6\n640 480\n255\n'
    require(data.startswith(header) and len(data) == len(header)+640*480*3, 'PPM layout')
    pixels = data[len(header):]
    colors = [pixels[i:i+3] for i in range(0, len(pixels), 3)]
    bg = colors.count(bytes((32, 64, 128)))
    geo = sum(c[0] == c[1] == c[2] and c[0] >= 80 for c in colors)
    require(bg > 1000 and geo > 1000, 'missing GPU mesh or background')
    require(bg + geo == 640*480, 'non-mesh palette in camera oracle (UI contamination)')
    return pixels, bg, geo


def verify_presentation(log, frames=12):
    require('scene_source=drawable pre_ui_oracle=drawable' in log, 'oracle must mirror actual drawable before UI')
    records = re.findall(r'drawable frame=(\d+) id=(\d+) presented_time=([0-9.]+) gpu_completed=1', log)
    require(len(records) == frames, 'missing completed real drawable records')
    require([int(r[0]) for r in records] == list(range(1, frames+1)), 'drawable frame sequence')
    actual = sum(float(r[2]) > 0 for r in records)
    require(f'drawable_acquired={frames} present_scheduled={frames} actual_presented={actual} hidden_checks={frames}' in log,
            'drawable/scheduled/actual/hidden counts inconsistent')
    require(f'completed={frames} presented={actual} readback={frames} errors=0' in log, 'GPU lifecycle/readback counts')
    return actual


def main():
    executable, content, output = [Path(x).resolve() for x in sys.argv[1:4]]
    output.mkdir(parents=True, exist_ok=True)
    launch = stage(content, output)
    options = sys.argv[4:]
    require(len(options) == len(set(options)) and all(o in ('--negative-control', '--hidden-present') for o in options), 'unknown/duplicate runner option')
    negative = '--negative-control' in options
    present = '--hidden-present' in options
    modes = ('--diagnostic-frozen-camera-test',) if negative else ('--engine-camera', '--engine-camera-test')
    for mode in modes:
        for run in range(1 if negative else 3):
            name = mode[2:] + '-' + str(run)
            snapshot = output / (name + '.ppm')
            result = subprocess.run([str(executable), 'MetalCamera', '12', mode, str(snapshot)] + (['--hidden-present'] if present else []), cwd=launch, capture_output=True, text=True, timeout=60)
            (output / (name+'.log')).write_text(result.stdout+result.stderr)
            require(result.returncode == 0, name+' executable failed: '+result.stdout+result.stderr)
            for marker in ('teardown complete; PASS', 'create=1 close=1 destroy=1 imgui_init=1 imgui_shutdown=1', 'camera_frames=12', 'draw_calls=12 drawn_triangles=141636 geometry_readbacks=12 visual_register=1 visual_unregister=1'):
                require(marker in result.stdout, 'missing '+marker)
            actual_presented = verify_presentation(result.stdout) if present else 0
            if not present:
                require('completed=12 presented=0 readback=12 errors=0' in result.stdout, 'offscreen GPU lifecycle')
            frames = [json.loads(line.split('camera_json=', 1)[1]) for line in result.stdout.splitlines() if 'camera_json=' in line]
            require(len(frames) == 12, 'missing camera inputs')
            for frame in frames:
                verify_frame(frame)
            gpu = [json.loads(line.split('gpu_clip_json=', 1)[1]) for line in result.stdout.splitlines() if 'gpu_clip_json=' in line]
            require(len(gpu) == 12, 'missing uploaded GPU matrices')
            if negative:
                require(all(m == gpu[0] for m in gpu), 'negative control did not freeze GPU clip')
                require(gpu[0] == frames[0]['clip_matrix'] and gpu[-1] != frames[-1]['clip_matrix'],
                        'negative control must freeze the first valid matrix, not the changing API projection')
            else:
                require(all(m == f['clip_matrix'] for m, f in zip(gpu, frames)), 'GPU binding differs from independently verified matrix')
            first, bg, geo = image(snapshot)
            summary = dict(mode=mode, run=run, background=bg, geometry=geo, hidden_present=present, actual_presented=actual_presented)
            if mode.endswith('-test'):
                second, bg2, geo2 = image(Path(str(snapshot)+'.view2.ppm'))
                changed = sum(first[i:i+3] != second[i:i+3] for i in range(0, len(first), 3))
                require(max(abs(a-b) for a, b in zip(frames[0]['camera_rotation'], frames[-1]['camera_rotation'])) > 0.1, 'engine camera rotation unchanged')
                require(frames[0]['controlled_view'] == 1 and frames[-1]['controlled_view'] == 2, 'test camera phases missing')
                summary.update(second_geometry=geo2, changed_pixels=changed)
                summary.update(api_camera_changed=True, frozen_gpu=negative)
                print(json.dumps(summary), flush=True)
                require(changed > 1000, 'GPU viewpoint did not change')
            else:
                require(all(f['controlled_view'] == 0 for f in frames), 'scenario camera overridden')
                print(json.dumps(summary), flush=True)
    # The same image oracle MUST reject frozen GPU clip despite changing API state.
    control = subprocess.run([sys.executable, str(Path(__file__).resolve()), str(executable), str(content),
                              str(output / 'negative-control'), '--negative-control'] + (['--hidden-present'] if present else []), capture_output=True, text=True, timeout=90)
    (output / 'negative-control.log').write_text(control.stdout + control.stderr)
    require(control.returncode != 0 and 'GPU viewpoint did not change' in control.stderr
            and '"api_camera_changed": true' in control.stdout and '"changed_pixels": 0' in control.stdout,
            'negative sensitivity control did not fail at the changed-view oracle: '+control.stdout+control.stderr)
    print(('hidden drawable submission + mirrored camera oracle; actual scanout reported separately. ' if present else '') + 'PASS: 6 real engine-camera lifecycles, independent projection and mesh-only changed-view GPU checks; frozen-GPU negative control rejected (0 changed pixels)')


if __name__ == '__main__':
    main()
