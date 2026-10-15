#!/usr/bin/env python3
"""Real native J71G engine samples; independently integrated lower atmosphere.
No runtime cfg is changed: even the nested Earth override is fixture-owned.
"""
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'metal'))
from fixture_staging import stage


def fixture(content, output, selection):
    launch = stage(content, output, 'atmosphere-content-')
    earth = launch / 'Config/Earth'
    # stage() intentionally links ordinary config directories. Replace ONLY the
    # fresh fixture link; reserve the actual nested override before adding links.
    if not earth.is_symlink():
        raise AssertionError('expected fresh fixture Earth link')
    source = earth.resolve(strict=True)
    earth.unlink()
    earth.mkdir()
    override = earth / 'Atmosphere.cfg'
    with override.open('xb') as target:
        target.write(('MODULE_ATM = '+selection+'\n').encode())
    for entry in source.iterdir():
        destination = earth / entry.name
        if not os.path.lexists(destination):
            destination.symlink_to(entry, target_is_directory=entry.is_dir())
    assert not earth.is_symlink() and not override.is_symlink()
    assert override.resolve().is_relative_to(launch.resolve())
    return launch


def reference(alt):
    # Source J77 lower branch, expressed as hydrostatic log-pressure integral,
    # not calls to or compilation of the module's helper functions. Exact source
    # constants are deliberately retained (not rounded textbook ISA substitutes).
    z = alt/1000
    h = 6369*z/(6369+z)
    layers = [(11,0,1,288.15,-6.5), (20,11,.2233611,216.65,0),
              (32,20,.05403295,216.65,1), (47,32,.0085666784,228.65,2.8),
              (51,47,.0010945601,270.65,0), (71,51,.00066063531,270.65,-2.8),
              (85,71,.000039046834,214.65,-2)]
    _, base, pressure, temp, lapse = next(layer for layer in layers if z <= layer[0])
    T = temp + lapse*(h-base)
    integral = math.log(T/temp)/lapse if lapse else (h-base)/temp
    fraction = pressure * math.exp(-34.163195*integral)
    number = 2.547e19*288.15/T*fraction
    molecular = 28.96*(1-10**(-3.7469+(z-85)*(.226434-(z-85)*.005945)))
    rho = molecular*number*1e3/6.02257e23
    p = number*1e6*1.38066e-23*T
    return T, p, rho, math.sqrt(1.4*286.91*T)


def main():
    exe, content, output = (Path(p).resolve() for p in sys.argv[1:4])
    output.mkdir(parents=True, exist_ok=True)
    original = content/'Config/Earth/Atmosphere.cfg'
    before = original.read_bytes()
    results = []
    for selection in ['EarthAtmJ71G', 'AtmosphereFixtureMissing', '[None]']:
        launch = fixture(content, output, selection)
        process = subprocess.run([str(exe), 'MetalEarth'], cwd=launch, capture_output=True, text=True, timeout=90)
        (launch/'probe.log').write_text(process.stdout+process.stderr)
        assert original.read_bytes() == before, 'source override modified'
        assert 'world_closed=1' in process.stdout, 'normal teardown absent'
        if selection != 'EarthAtmJ71G':
            assert process.returncode == 4 and 'ERROR: Earth native atmosphere unavailable' in process.stderr
            assert 'sample ' not in process.stdout and 'world_closed=1 FAIL' in process.stdout
            results.append(dict(selection=selection, exit=process.returncode, launch=str(launch), rejected=True))
            continue
        assert process.returncode == 0 and 'world_closed=1 PASS' in process.stdout, str(launch/'probe.log')
        assert 'model=J71G pointer=0x' in process.stdout
        samples = [list(map(float,line.split()[1:])) for line in process.stdout.splitlines() if line.startswith('sample ')]
        assert len(samples) == 11
        errors = []
        for alt, *actual in samples:
            if alt <= 80000:
                expected = reference(alt)
                for a, b in zip(actual, expected):
                    error = abs(a-b)/abs(b)
                    assert error < 2e-12, (alt,actual,expected,error)
                    errors.append(error)
        assert samples[0][1] > samples[2][1] > samples[3][1] # tropospheric cooling
        assert samples[4][1] < samples[5][1] # stratospheric inversion
        assert samples[-1][1] > samples[-3][1] # thermosphere warming
        results.append(dict(selection=selection,exit=0,launch=str(launch),samples=samples,
                            lower_reference_count=8,max_relative_error=max(errors)))
    (output/'results.json').write_text(json.dumps(dict(results=results,override_sha256=hashlib.sha256(before).hexdigest()),indent=2))
    print('PASS: real J71G 11 altitudes; 8 independent lower-reference samples; missing/disabled rejected; override unchanged')


if __name__ == '__main__':
    main()
