#!/usr/bin/env python3
"""Real engine Ctrl+P buffered commands, hidden process-local SDL events only."""
import json
from pathlib import Path
import subprocess
import sys
from fixture_staging import stage
from mesh_regression import require
from camera_regression import verify_presentation

def main():
    executable, content, output = [Path(x).resolve() for x in sys.argv[1:4]]
    require(sys.argv[4:] in ([], ['--hidden-present']), 'unknown options')
    presentation = sys.argv[4:]
    output.mkdir(parents=True, exist_ok=True)
    launch = stage(content, output)
    for run in range(4):
        negative = run == 3
        flag = '--buffered-input-ignore-test' if negative else '--buffered-input-test'
        result = subprocess.run([str(executable), 'MetalCamera', '23', '--textures',
                                 str(output / f'buffered-{run}.ppm'), flag] + presentation,
                                cwd=launch, capture_output=True, text=True, timeout=60)
        log = result.stdout + result.stderr
        (output / f'buffered-{run}.log').write_text(log)
        records = [json.loads(line.split('buffered_json=', 1)[1]) for line in log.splitlines() if 'buffered_json=' in line]
        require(len(records) == 19 and [r['frame'] for r in records] == list(range(19)), 'exact frame count/early quit')
        for r in records:
            f = r['frame']
            keys = [1, 0, 0] if f == 5 else [1, 1, 0] if f in (7, 8, 9, 11, 12) else [0, 1, 0] if f == 10 else [1, 0, 1] if f in (14, 16) else [0, 0, 0]
            require(r['polled_keys'] == keys, 'held publication must remain intact even in no-buffer control: ' + str(r))
            expected = False if negative else (8 <= f <= 11 or 15 <= f <= 16)
            require(r['pause'] == expected, 'real pause state: ' + str(r))
            delta = r['sim_after'] - r['sim_before']
            require(f == 0 or (delta == 0 if r['pause'] else delta > 0), 'real simulation stepping: ' + str(r))
        require(result.returncode != 0 if negative else result.returncode == 0, 'exit status: ' + log)
        for marker in ('buffered input verified=' + ('0' if negative else '1'),
                       'teardown complete; ' + ('FAIL' if negative else 'PASS'),
                       'create=1 close=1 destroy=1 imgui_init=1 imgui_shutdown=1',
                       'draw_calls=19 drawn_triangles=224257 geometry_readbacks=19 visual_register=1 visual_unregister=1'):
            require(marker in log, 'missing ' + marker + '\n' + log)
        if presentation:
            verify_presentation(log, frames=19)
        else:
            require('completed=19 presented=0 readback=19 errors=0' in log and 'actual_presented=0 hidden_checks=19' in log, 'GPU/hidden lifecycle')
        print(json.dumps({'run': run, 'negative': negative, 'frames': len(records), 'pause_frames': [r['frame'] for r in records if r['pause']]}), flush=True)
    print('PASS: 3 real engine Ctrl+P pause/resume sessions; repeats/held/up/focus/right-Ctrl; no-buffer control rejected after normal teardown')

if __name__ == '__main__':
    main()
