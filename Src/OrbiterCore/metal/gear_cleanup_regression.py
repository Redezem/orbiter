#!/usr/bin/env python3
"""Real hidden engine teardown with an explicit CLIENT-ONLY validation fault.
Does not mutate live shared geometry; CPU seam covers actual geometry rejection.
"""
import json
from pathlib import Path
import subprocess
import sys
from fixture_staging import stage
from mesh_regression import require


def main():
    executable, content, output = [Path(x).resolve() for x in sys.argv[1:4]]
    output.mkdir(parents=True, exist_ok=True)
    launch = stage(content, output)
    result = subprocess.run([str(executable), 'MetalCamera', '12', '--textures',
                             str(output/'fault.ppm'), '--gear-animation-test',
                             '--gear-cleanup-fault-test'], cwd=launch,
                            capture_output=True, text=True, timeout=90)
    log = result.stdout+result.stderr
    (output/'fault.log').write_text(log)
    require(result.returncode == 1, 'diagnostic must fail normally, not abort or pass')
    require('teardown complete; PASS' not in log, 'cleanup failure swallowed as PASS')
    frames = [json.loads(line.split('gear_frame_json=', 1)[1])
              for line in log.splitlines() if 'gear_frame_json=' in line]
    require(len(frames) == 12 and frames[-1]['state'] == frames[-1]['readback'] == 1,
            'fault must follow actual changed engine state')
    ordered = ['gear_detach stale_handle_rejected=1 template_unchanged=0 state_restored=1 restore_pending=0',
               'texture_mesh_pins_released=1 before_engine_teardown=1',
               'visual_unregistered=1 before_engine_teardown=1',
               'material_resources_released=1 before_engine_teardown=1',
               'world_closed=1', 'teardown complete; FAIL']
    positions = [log.find(marker) for marker in ordered]
    require(all(p >= 0 for p in positions) and positions == sorted(positions),
            'missing/out-of-order cleanup evidence')
    for marker in ['gear_attach id=7 default=1 loaded=0',
                   'injected client-only gear validation fault (engine data untouched)',
                   'visual_register=1 visual_unregister=1', 'invalid_releases=0',
                   'create=1 close=1 destroy=1 imgui_init=1 imgui_shutdown=1',
                   'completed=12 presented=0 readback=12 errors=0']:
        require(marker in log, 'missing '+marker)
    evidence = dict(engine_exit=result.returncode, changed_engine_state=1,
                    loaded_state_restored=True, cleanup_markers=ordered,
                    fault='client-only validation; no shared engine geometry mutation')
    (output/'evidence.json').write_text(json.dumps(evidence, indent=2))
    print('PASS failure-path regression: engine exit=1, restoration and independent teardown verified; explicit client-only validation fault')


if __name__ == '__main__':
    main()
