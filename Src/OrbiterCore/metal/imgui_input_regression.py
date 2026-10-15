#!/usr/bin/env python3
"""Hidden real engine InputText, capture, throttle and buffered pause oracle."""
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
    output.mkdir(parents=True, exist_ok=True)
    launch = stage(content, output)
    for run in range(4):
        negative = run == 3
        flag = '--imgui-input-ignore-test' if negative else '--imgui-input-test'
        result = subprocess.run([str(executable), 'MetalCamera', '26', '--textures',
                                 str(output / f'imgui-{run}.ppm'), flag] + sys.argv[4:],
                                cwd=launch, capture_output=True, text=True, timeout=60)
        log = result.stdout + result.stderr
        (output / f'imgui-{run}.log').write_text(log)
        records = [json.loads(line.split('imgui_input_json=', 1)[1]) for line in log.splitlines() if 'imgui_input_json=' in line]
        keys = [json.loads(line.split('imgui_keys_json=', 1)[1]) for line in log.splitlines() if 'imgui_keys_json=' in line]
        require(len(records) == len(keys) == 22 and [r['frame'] for r in records] == list(range(22)), 'exact frames/early quit')
        require((result.returncode != 0) if negative else (result.returncode == 0), 'process exit: ' + log)
        for frame in (5, 6):
            r = records[frame]
            require(not r['capture_before'] and r['after'] > r['before'], 'real uncaptured held throttle')
        for frame in (12, 13):
            r, k = records[frame], keys[frame]
            require(r['capture_before'] and r['after'] == r['before'] and not r['pause'], 'real captured controls suppressed')
            require(k['raw_add'] == k['raw_p'] == 1 and k['engine_add'] == k['engine_p'] == 0, 'raw held preserved, engine gated')
        require(records[17]['after'] < records[17]['before'] and not records[17]['capture_before'], 'control resumes after widget/focus loss')
        require([r['frame'] for r in records if r['pause']] == [18, 19], 'real buffered pause/resume boundary')
        require(all(keys[f]['raw_add'] == keys[f]['raw_p'] == 0 for f in (14, 15, 16, 20, 21)), 'physical releases/loss clearing')
        if negative:
            require(all(r['text'] == '' for r in records), 'disabled forwarding must produce no text')
            require(not all(r['ok'] for r in records), 'same text oracle rejects missing forwarding')
        else:
            require(all(r['ok'] for r in records), 'all real per-frame oracles')
            require(records[8]['text'] == 'orbit' and records[9]['text'] == records[10]['text'] == 'orbi' and records[11]['text'] == 'orbit', 'actual InputText text/backspace/release')
            require(keys[12]['gui_add'] and keys[13]['gui_add'] and keys[12]['gui_p'] and keys[13]['gui_p'], 'actual ImGui held keys')
            require(all(not keys[f]['gui_add'] and not keys[f]['gui_p'] for f in (14, 15, 16)), 'actual ImGui focus clearing')
        for marker in ('imgui input verified=' + ('0' if negative else '1'),
                       'teardown complete; ' + ('FAIL' if negative else 'PASS'),
                       'create=1 close=1 destroy=1 imgui_init=1 imgui_shutdown=1',
                       'platform_shutdown context_alive=1 window_alive=1 backend_cleared=1',
                       'platform_session_destroyed actual_device_keys=all-released',
                       'event_only=1 global_mouse_poll=0 cursor_change=0 os_ime=0 clipboard=disabled screensaver_enabled=1'):
            require(marker in log, 'missing ' + marker)
        if sys.argv[4:]:
            verify_presentation(log, frames=22)
        else:
            require('completed=22 presented=0 readback=22 errors=0' in log and 'actual_presented=0 hidden_checks=22' in log, 'GPU hidden lifecycle')
        print(json.dumps({'run': run, 'negative': negative, 'frames': len(records), 'final_text': records[-1]['text'], 'pause_frames': [18, 19]}), flush=True)
    print('PASS: 3 real engine InputText/capture/throttle/pause lifecycles; no-forwarding control rejected after normal teardown')


if __name__ == '__main__':
    main()
