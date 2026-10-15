#!/usr/bin/env python3
"""Hidden real bundle acceptance, read-only content and missing-root rejection."""
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def digest_tree(root):
    """Regular-file bytes only; reject links rather than omit linked subtrees."""
    files = {}
    for p in root.rglob('*'):
        require(not p.is_symlink(), 'content inventory does not support symlinks: ' + str(p))
        if p.is_file():
            files[str(p.relative_to(root))] = hashlib.sha256(p.read_bytes()).hexdigest()
    return files


def verify_presentation(text):
    submission = re.findall(r'drawable_acquired=(\d+) present_scheduled=(\d+) actual_presented=(\d+) hidden_checks=(\d+)', text)
    lifecycle = re.findall(r'update=(\d+) newframe=(\d+) render=(\d+) drawdata=(\d+) display=(\d+) completed=(\d+) presented=(\d+) readback=(\d+) errors=(\d+)', text)
    require(len(submission) == len(lifecycle) == 1, 'missing/duplicate presentation counters')
    acquired, scheduled, actual, hidden = map(int, submission[0])
    update, newframe, render, drawdata, display, completed, presented, readback, errors = map(int, lifecycle[0])
    require(all(n == 24 for n in (acquired, scheduled, hidden, update, newframe, render, drawdata, display, completed, readback))
            and errors == 0 and actual == presented and 0 <= actual <= completed,
            'inconsistent hidden GPU/presentation counters')
    return actual  # Timestamp observations, never proof of visible scanout.


def require(value, message):
    if not value:
        raise RuntimeError(message)


def main():
    bundle, content, output = (Path(p).resolve() for p in sys.argv[1:4])
    require(content.is_dir(), 'content must be an existing directory: ' + str(content))
    manifest = bundle / 'Contents/Resources/content-root.txt'
    manifest_bytes = manifest.read_bytes()
    named = manifest_bytes.decode('utf-8').split('\n', 1)[0]  # Match C++ getline, without path trimming.
    require(bool(named) and Path(named).is_absolute(), 'content manifest must name an absolute directory')
    actual = Path(named).resolve()
    require(actual.is_dir(), 'content manifest root must be an existing directory: ' + str(actual))
    require(actual == content, f'content root mismatch: manifest={actual} supplied={content}')
    stat = actual.stat()
    identity = dict(manifest=str(manifest), manifest_text=named, resolved_root=str(actual),
                    device=stat.st_dev, inode=stat.st_ino, symlinks='rejected',
                    inventory_scope='regular-file SHA-256 bytes; not permissions/ownership/hardlink or empty-directory identity')
    source = digest_tree(actual)
    output.mkdir(parents=True, exist_ok=True)
    (output / 'content-identity.json').write_text(json.dumps(identity, indent=2) + '\n')
    (output / 'content-before.json').write_text(json.dumps(source, indent=2, sort_keys=True) + '\n')
    sessions = Path.home() / 'Library/Application Support/OrbiterCorePreview/Sessions'
    results = []
    for run in range(3):
        before = set(sessions.iterdir()) if sessions.exists() else set()
        p = subprocess.run([str(bundle / 'Contents/MacOS/orbiter-core-preview'), '--self-test'],
                           capture_output=True, text=True, timeout=90)
        created = set(sessions.iterdir()) - before
        require(p.returncode == 0 and len(created) == 1, f'hidden bundle run failed: {p.returncode} {p.stderr}')
        launch = created.pop()
        text = (launch / 'preview-session.log').read_text()
        actual_presented = verify_presentation(text)
        for marker in ('hidden=1 continuous=0', 'bounded frames=24 real_pause=1 SDL_quit=1',
                       'drawable_acquired=24 present_scheduled=24', 'hidden_checks=24',
                       'create=1 close=1 destroy=1 imgui_init=1 imgui_shutdown=1',
                       'combined_draws terrain=24 vessel=24 same_encoder=1 same_depth_attachment=1',
                       'platform_shutdown context_alive=1 window_alive=1 backend_cleared=1',
                       'world_closed=1', 'teardown complete; PASS'):
            require(marker in text, 'missing ' + marker)
        require(not (launch / 'Config/Earth').is_symlink(), 'private Earth is linked')
        atm = launch / 'Config/Earth/Atmosphere.cfg'
        require(not atm.is_symlink() and atm.read_text() == 'MODULE_ATM = EarthAtmJ71G\n', 'private atmosphere override')
        for name in ('Orbiter.cfg', 'Config/MetalEarth.cfg', 'Scenarios/MetalTerrainVessel.scn'):
            require(not (launch / name).is_symlink(), 'private file linked: ' + name)
        require((launch / 'Textures').is_symlink(), 'shared textures duplicated')
        ppm = (launch / 'preview.ppm').read_bytes()
        require(ppm.startswith(b'P6\n640 480\n255\n'), 'missing actual GPU image')
        shutil.copy2(launch / 'preview.ppm', output / f'preview-{run}.ppm')
        shutil.copy2(launch / 'preview-session.log', output / f'preview-{run}.log')
        results.append(dict(exit=p.returncode, session=str(launch), actual_presented=actual_presented,
                            snapshot_sha256=hashlib.sha256(ppm).hexdigest()))
    # Alter only a fresh COPY of our own bundle, never the shipped bundle/source.
    negative_parent = Path(tempfile.mkdtemp(prefix='missing-content-', dir=output))
    negative = negative_parent / 'OrbiterCorePreview.app'
    shutil.copytree(bundle, negative)
    (negative / 'Contents/Resources/content-root.txt').write_text(str(negative_parent / 'absent') + '\n')
    subprocess.run(['codesign', '--force', '--sign', '-', str(negative)], check=True, capture_output=True)
    before = set(sessions.iterdir())
    p = subprocess.run([str(negative / 'Contents/MacOS/orbiter-core-preview'), '--self-test'],
                       capture_output=True, text=True, timeout=15)
    require(p.returncode == 2 and 'preview missing staged asset:' in p.stderr, 'missing asset was not clearly rejected')
    require(set(sessions.iterdir()) == before, 'missing root created a session')
    after = digest_tree(actual)
    (output / 'content-after.json').write_text(json.dumps(after, indent=2, sort_keys=True) + '\n')
    require(after == source, 'preview changed staged file bytes')
    require(actual.stat().st_dev == stat.st_dev and actual.stat().st_ino == stat.st_ino,
            'content root directory identity changed')
    require(manifest.read_bytes() == manifest_bytes, 'bundle content manifest changed')
    subprocess.run(['codesign', '--verify', '--deep', '--strict', str(bundle)], check=True)
    result = dict(runs=results, missing_asset=dict(exit=p.returncode, stderr=p.stderr), source_unchanged=True)
    (output / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    print('PASS: 3 hidden bundle sessions, 24 real GPU frames each; actual paused state, SDL quit, private configs, source unchanged; missing root rejected')


if __name__ == '__main__':
    main()
