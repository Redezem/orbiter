#!/usr/bin/env python3
"""Package the real-core preview; never opens it or alters the bootstrap bundle."""
import argparse
import plistlib
import shutil
import subprocess
from pathlib import Path


def package(executable, content, bundle, sdl):
    executable, content, bundle, sdl = map(lambda p: Path(p).resolve(), (executable, content, bundle, sdl))
    if bundle.name != 'OrbiterCorePreview.app':
        raise ValueError('destination must be separate OrbiterCorePreview.app')
    if bundle.exists():
        raise FileExistsError('use a fresh bundle destination; no recursive overwrite')
    mac = bundle / 'Contents/MacOS'
    resources = bundle / 'Contents/Resources'
    frameworks = bundle / 'Contents/Frameworks'
    for folder in (mac, resources, frameworks):
        folder.mkdir(parents=True)
    target = mac / 'orbiter-core-preview'
    shutil.copy2(executable, target)
    shutil.copy2(sdl, frameworks / 'libSDL3.0.dylib')
    fixtures = Path(__file__).resolve().with_name('fixtures')
    for name in ('MetalEarth.cfg', 'MetalTerrainVessel.scn'):
        shutil.copy2(fixtures / name, resources / name)
    (resources / 'content-root.txt').write_text(str(content) + '\n')
    lines = subprocess.check_output(['otool', '-l', str(target)], text=True).splitlines()
    minimum_os = next(line.split()[1] for line in lines if line.strip().startswith('minos '))
    (bundle / 'Contents/Info.plist').write_bytes(plistlib.dumps(dict(
        CFBundleExecutable='orbiter-core-preview', CFBundleIdentifier='org.orbiter.core-preview',
        CFBundleName='Orbiter Core Preview', CFBundlePackageType='APPL',
        CFBundleVersion='1', CFBundleShortVersionString='0.1',
        NSHighResolutionCapable=False,
        LSMinimumSystemVersion=minimum_os)))
    # Remove build-machine SDL search roots; keep only session-local module lookup
    # and the bundled SDL. Engine/plugin dependencies intentionally use staged root.
    for i, line in enumerate(lines):
        if line.strip() == 'cmd LC_RPATH':
            path = lines[i + 2].strip().split('path ', 1)[1].split(' (offset', 1)[0]
            subprocess.run(['install_name_tool', '-delete_rpath', path, str(target)], check=True)
    for path in ('@executable_path/../Frameworks', '.'):
        subprocess.run(['install_name_tool', '-add_rpath', path, str(target)], check=True)
    for path in (frameworks / 'libSDL3.0.dylib', target, bundle):
        subprocess.run(['codesign', '--force', '--sign', '-', str(path)], check=True)
    subprocess.run(['plutil', '-lint', str(bundle / 'Contents/Info.plist')], check=True)
    subprocess.run(['codesign', '--verify', '--deep', '--strict', str(bundle)], check=True)
    print(bundle)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('executable', 'content', 'bundle', 'sdl'):
        p.add_argument(name, type=Path)
    a = p.parse_args()
    package(a.executable, a.content, a.bundle, a.sdl)
