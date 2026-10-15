#!/usr/bin/env python3
"""Hidden offscreen GPU/lifecycle regression (no input or window focus).

Usage: python3 regression.py /absolute/path/orbiter-core-metal /path/to/content
"""
import json
from pathlib import Path
import subprocess
import sys


def main():
    executable = Path(sys.argv[1]).resolve()
    content = Path(sys.argv[2]).resolve()
    for run in range(3):
        result = subprocess.run(
            [str(executable), 'Delta-glider/Smack!', '12'], cwd=content,
            capture_output=True, text=True, timeout=60,
        )
        markers = [line for line in result.stdout.splitlines() if '[core-metal]' in line]
        print(json.dumps({'run': run, 'exit': result.returncode, 'markers': markers}), flush=True)
        if result.returncode:
            print(result.stderr, file=sys.stderr)
        assert result.returncode == 0
        assert '[core-metal] update=12 newframe=12 render=12 drawdata=12 display=12 completed=12 presented=0 readback=12 errors=0' in markers
        assert '[core-metal] create=1 close=1 destroy=1 imgui_init=1 imgui_shutdown=1' in markers
        assert '[core-metal] teardown complete; PASS' in markers
    print('PASS: 3 hidden offscreen runs, 12 completed/read-back GPU frames each')


if __name__ == '__main__':
    main()
