#!/usr/bin/env python3
"""Unpaused real-core session regression: reproduces the preview lifecycle.

The session starts paused (host applies Pause(true) exactly like the preview),
a real Ctrl+P buffered command unpauses it, and the sim runs unpaused for the
remaining frames. Two independent oracles:
1. probe JSON: real oapiGetPause() transitions at the right frames and real
   sim-time advancing only while unpaused (must pass before AND after the
   LuaInline fix - the unpause path itself already works).
2. engine log: the unpaused window must contain ZERO engine module-load
   failures. This is the red-capable assertion for the LuaInline diagnosis:
   the DeltaGlider script system currently retries a missing LuaInline module
   during startup and unpaused flight, which produced the user's repeated
   on-screen error toasts.
The negative control withholds key publication; Ctrl+P never reaches the
engine, the session stays paused, and the run must fail while tearing down.
"""
import json
from pathlib import Path
import subprocess
import sys
from fixture_staging import stage
from mesh_regression import require

MODULE_ERROR = 'Could not find a module named'


def main():
    executable, content, output = [Path(x).resolve() for x in sys.argv[1:4]]
    output.mkdir(parents=True, exist_ok=True)
    launch = stage(content, output, prefix='unpause-')
    result = subprocess.run([str(executable), 'MetalCamera', '30', '--session-unpause-test'],
                            cwd=launch, capture_output=True, text=True, timeout=120)
    log = result.stdout + result.stderr
    (output / 'unpause.log').write_text(log)
    require(result.returncode == 0, 'unpaused session executable failed: ' + log[-4000:])
    records = [json.loads(line.split('unpause_json=', 1)[1]) for line in log.splitlines() if 'unpause_json=' in line]
    require(len(records) == 30 and [r['frame'] for r in records] == list(range(30)),
            'unpause frame count/order: ' + str(len(records)))
    require(all(r['ok'] for r in records), 'unpause oracle frame mismatch: ' +
            json.dumps([r for r in records if not r['ok']][:3]))
    require(records[5]['pause'] is True and records[6]['pause'] is False,
            'real Ctrl+P did not unpause at the expected frame boundary')
    advance = [r['advance'] for r in records[7:]]
    require(all(a > 0 for a in advance), 'sim time stalled while unpaused: ' + json.dumps(advance[:6]))
    require('unpause verified=1' in log and 'teardown complete; PASS' in log, 'missing unpause markers')
    # Red-capable oracle: the engine's own error log must be clean of module
    # load failures across the whole lifecycle, unpaused flight included.
    engine_log = (launch / 'Orbiter_core_metal.log').read_text()
    failures = [line for line in engine_log.splitlines() if MODULE_ERROR in line]
    require(not failures, 'engine module-load failures during unpaused session: ' +
            ' | '.join(failures[:4]))
    # Positive evidence: with verbose=2 the engine's real LoadModule logs the
    # handshake for every module it resolves (Src/Orbiter/Orbiter.cpp). This
    # line only appears when LuaInline.dylib actually loaded and registered.
    require('Loading module LuaInline' in engine_log,
            'engine never reported loading the LuaInline module')
    print(json.dumps({'frames': 30, 'module_errors': 0, 'sim_advance_last': advance[-1]}))
    # Negative control: same lifecycle, key publication withheld -> must fail.
    neg = stage(content, output, prefix='unpause-')
    result = subprocess.run([str(executable), 'MetalCamera', '30', '--session-unpause-ignore-test'],
                            cwd=neg, capture_output=True, text=True, timeout=120)
    log = result.stdout + result.stderr
    (output / 'unpause-ignored.log').write_text(log)
    require(result.returncode != 0 and 'unpause verified=0' in log,
            'unpause negative control not rejected: ' + log[-2000:])
    print('PASS: real Ctrl+P unpauses a paused real-core session; engine log free of module-load failures; negative control rejected')


if __name__ == '__main__':
    main()
