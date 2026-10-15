#!/usr/bin/env python3
"""Hidden real-core session controls; only process-local SDL_PushEvent fixtures."""
import json
from pathlib import Path
import subprocess
import sys
from fixture_staging import stage
from mesh_regression import require
from camera_regression import verify_presentation


def main():
    executable, content, output = [Path(x).resolve() for x in sys.argv[1:4]]
    require(sys.argv[4:] in ([], ['--hidden-present']), 'unknown session runner options')
    presentation = sys.argv[4:]
    output.mkdir(parents=True, exist_ok=True)
    launch = stage(content, output)
    for run in range(3):
        result = subprocess.run([str(executable), 'MetalCamera', '20', '--textures',
                                 str(output / f'session-{run}.ppm'), '--session-input-test'] + presentation,
                                cwd=launch, capture_output=True, text=True, timeout=60)
        log = result.stdout + result.stderr
        (output / f'session-{run}.log').write_text(log)
        require(result.returncode == 0, 'session input executable failed: ' + log)
        records = [json.loads(line.split('session_json=', 1)[1]) for line in log.splitlines() if 'session_json=' in line]
        require(len(records) == 16, 'session must exercise exactly 16 actual SingleFrame calls')
        require([r['frame'] for r in records] == list(range(16)), 'frame ordering')
        for r in records:
            frame = r['frame']
            expected = 'increase' if frame in (5, 6, 8, 11) else 'decrease' if frame == 13 else 'stable'
            require(r['expected'] == expected, 'oracle phase differs from independent schedule')
            keys = {5: [1, 0, 1, 0], 6: [1, 0, 1, 0], 7: [0, 0, 1, 0],
                    8: [1, 0, 0, 1], 11: [1, 0, 1, 0], 13: [0, 1, 1, 0]}
            require(r['polled_keys'] == keys.get(frame, [0, 0, 0, 0]), 'actual engine-polled keys differ from SDL physical event schedule')
            change = r['after'] - r['before']
            require(change > 1e-10 if expected == 'increase' else change < -1e-10 if expected == 'decrease' else abs(change) < 1e-12,
                    'actual engine throttle response mismatch: ' + str(r))
        for marker in ('session input verified=1', 'teardown complete; PASS',
                       'create=1 close=1 destroy=1 imgui_init=1 imgui_shutdown=1',
                       'draw_calls=16 drawn_triangles=188848 geometry_readbacks=16 visual_register=1 visual_unregister=1'):
            require(marker in log, 'missing ' + marker)
        if presentation:
            verify_presentation(log, frames=16)
        else:
            require('completed=16 presented=0 readback=16 errors=0' in log and
                    'actual_presented=0 hidden_checks=16' in log, 'hidden offscreen lifecycle')
        print(json.dumps({'run': run, 'frames': len(records), 'initial': records[0]['before'], 'final': records[-1]['after']}), flush=True)
    # Same actual SDL queue and SingleFrame schedule with publication disabled:
    # must fail the engine-control oracle, but still complete normal teardown.
    result = subprocess.run([str(executable), 'MetalCamera', '20', '--session-input-ignore-test'] + presentation,
                            cwd=launch, capture_output=True, text=True, timeout=60)
    log = result.stdout + result.stderr
    (output / 'ignored-bridge.log').write_text(log)
    require(result.returncode != 0 and 'session input verified=0' in log and
            'create=1 close=1 destroy=1 imgui_init=1 imgui_shutdown=1' in log and
            'teardown complete; FAIL' in log, 'ignored-bridge sensitivity control not rejected: ' + log)
    print('PASS: 3 hidden real-core textured input sessions; held/up/modifiers/focus-loss/foreign-window/quit; ignored-bridge control rejected')


if __name__ == '__main__':
    main()
