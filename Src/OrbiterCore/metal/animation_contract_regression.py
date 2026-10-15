#!/usr/bin/env python3
"""Actual engine animation contract proof; deliberately NOT animated GPU support."""
import json
from pathlib import Path
import subprocess
import sys
from fixture_staging import stage
from mesh_regression import require


def records(log, marker):
    return [json.loads(line.split(marker, 1)[1]) for line in log.splitlines() if marker in line]


def verify(log):
    components = records(log, 'animation_component_json=')
    require(len(components) == 14, 'DG gear definition count')
    require([c['component'] for c in components] == list(range(14)), 'component order')
    require(all(c['kind'] == 'rotate' and c['mesh'] == 0 and not c['parent'] and c['children'] == 0 for c in components), 'bounded gear shape')
    require(components[0]['pivot'] == [0, -1.048, 8.561] and components[0]['axis'] == [1, 0, 0], 'source-defined nose-wheel pivot/axis')
    require([c['state0'] for c in components] == [.3, .3, 0, .7, 0, .7, 0, 0, 0, 0, .7, 0, 0, .7], 'source-defined intervals')
    states = records(log, 'animation_state_json=')
    require(len(states) == 3, 'state count')
    for requested, state in zip([0, .5, 1], states):
        require(state == dict(requested=requested, observed=requested, definition_state=requested, visual_events=0, template_unchanged=True), 'setter/readback/template/event proof')
    for marker in ('event_transport=1 identity=1 message=2 context=0 handled=0',
                   'animation_contract verified=1', 'device_mesh=null',
                   'GPU remains static', 'visual_register=1 visual_unregister=1',
                   'completed=12 presented=0 readback=12 errors=0',
                   'create=1 close=1 destroy=1 imgui_init=1 imgui_shutdown=1',
                   'teardown complete; PASS'):
        require(marker in log, 'missing ' + marker)
    # Check the oracle cannot accept an inert setter or unregistered event seam.
    for corrupted in (log.replace('"observed":0.5', '"observed":0'),
                      log.replace('event_transport=1', 'event_transport=0')):
        try:
            verify_without_sensitivity(corrupted)
        except RuntimeError:
            pass
        else:
            raise RuntimeError('contract oracle accepted corrupted evidence')


def verify_without_sensitivity(log):
    states = records(log, 'animation_state_json=')
    require([s['observed'] for s in states] == [0, .5, 1], 'inert setter')
    require('event_transport=1' in log, 'missing transport')


def main():
    require(len(sys.argv) == 4, 'usage: animation_contract_regression.py executable content output')
    executable, content, output = [Path(x).resolve() for x in sys.argv[1:]]
    output.mkdir(parents=True, exist_ok=True)
    launch = stage(content, output)
    signatures = []
    for run in range(3):
        images = []
        for contract in (False, True):
            name = f'contract-{run}-{int(contract)}'
            snapshot = output / (name + '.ppm')
            command = [str(executable), 'MetalCamera', '12', '--textures', str(snapshot)]
            if contract:
                command += ['--animation-contract-test']
            result = subprocess.run(command, cwd=launch, capture_output=True, text=True, timeout=60)
            log = result.stdout + result.stderr
            (output / (name + '.log')).write_text(log)
            require(result.returncode == 0, 'real engine failed: ' + log)
            if contract:
                verify(log)
            images.append(snapshot.read_bytes())
        require(images[0] == images[1], 'restored API proof changed static textured GPU image')
        signatures.append(images[1])
        print(json.dumps(dict(run=run, definitions=14, states=[0, .5, 1], template_unchanged=True, registered_event=True, static_gpu_unchanged=True)), flush=True)
    require(all(s == signatures[0] for s in signatures), 'repeat GPU image changed')
    print('PASS: 3 real engine definition/state/event proofs; 3 paired unchanged static GPU lifecycles; NOT animated rendering')


if __name__ == '__main__':
    main()
