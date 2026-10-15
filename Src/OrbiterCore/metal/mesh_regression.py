#!/usr/bin/env python3
"""Independent hidden real-core mesh runs, GPU readback and lifecycle checks."""
import json
from pathlib import Path
import re
import subprocess
import sys


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def main():
    executable, content, output = map(lambda x: Path(x).resolve(), sys.argv[1:4])
    output.mkdir(parents=True, exist_ok=True)
    signatures = []
    for run in range(3):
        snapshot = output / f'focus-mesh-{run}.ppm'
        result = subprocess.run([str(executable), 'Delta-glider/Smack!', '12', '--mesh', str(snapshot)],
                                cwd=content, capture_output=True, text=True, timeout=60)
        (output / f'mesh-{run}.log').write_text(result.stdout + result.stderr)
        print(json.dumps({'run': run, 'exit': result.returncode,
                          'markers': [s for s in result.stdout.splitlines() if '[core-metal]' in s]}), flush=True)
        require(result.returncode == 0, 'mesh executable failed: ' + result.stderr)
        require('[core-metal] teardown complete; PASS' in result.stdout, 'normal teardown missing')
        require('completed=12 presented=0 readback=12 errors=0' in result.stdout, 'GPU frame counters')
        require('create=1 close=1 destroy=1 imgui_init=1 imgui_shutdown=1' in result.stdout, 'lifecycle counters')
        m = re.search(r'mesh uploads=(\d+) groups=(\d+) vertices=(\d+) triangles=(\d+) draw_calls=(\d+) drawn_triangles=(\d+) geometry_readbacks=(\d+) visual_register=(\d+) visual_unregister=(\d+)', result.stdout)
        if m is None:
            raise RuntimeError('mesh counters missing')
        meshes, groups, vertices, triangles, draws, drawn, reads, reg, unreg = map(int, m.groups())
        require((meshes, groups, vertices, triangles) == (1, 122, 14339, 11803),
                'Smack! engine-template upload fixture changed')
        require(min(meshes, groups, vertices, triangles) > 0, 'empty mesh upload')
        require((draws, drawn, reads, reg, unreg) == (12, triangles * 12, 12, 1, 1), 'exact mesh/frame counters')
        data = snapshot.read_bytes()
        header = b'P6\n640 480\n255\n'
        require(data.startswith(header) and len(data) == len(header) + 640 * 480 * 3, 'PPM layout')
        pixels = data[len(header):]
        colors = [pixels[i:i+3] for i in range(0, len(pixels), 3)]
        background = colors.count(bytes((32, 64, 128)))
        geometry = sum(c[0] == c[1] and c[1] == c[2] and c[0] >= 80 for c in colors)
        require(background > 1000 and geometry > 1000, 'snapshot lacks clear background or mesh pixels')
        signatures.append((meshes, groups, vertices, triangles, background, geometry))
    require(len(set(signatures)) == 1, 'non-deterministic static diagnostic mesh output')
    print('PASS: 3 mesh runs, exact upload/draw/lifecycle counters and GPU geometry readback; signature=' + str(signatures[0]))


if __name__ == '__main__':
    main()
