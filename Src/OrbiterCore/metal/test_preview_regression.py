"""CPU-only production-startup and runner guards; all writes use fresh scratch homes."""
import hashlib
import json
import os
from pathlib import Path
import plistlib
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import preview_regression as runner

HERE = Path(__file__).resolve().parent


class PreviewPreflight(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.base = Path(tempfile.mkdtemp(prefix='preview-preflight-', dir=os.environ['TMPDIR']))
        cls.app = cls.base / 'Probe.app'
        cls.mac = cls.app / 'Contents/MacOS'
        cls.res = cls.app / 'Contents/Resources'
        cls.mac.mkdir(parents=True)
        cls.res.mkdir()
        (cls.app / 'Contents/Info.plist').write_bytes(plistlib.dumps(dict(
            CFBundleExecutable='orbiter-core-preview', CFBundlePackageType='APPL',
            CFBundleIdentifier='test.cpu.preview')))
        source = cls.base / 'probe.mm'
        source.write_text('''#include <array>
#include <string>
#include <stdexcept>
#define NSHomeDirectory TestHome
#include "''' + str(HERE / 'preview_startup.h') + '''"
#undef NSHomeDirectory
NSString *TestHome(void) { return [NSString stringWithUTF8String:getenv("TEST_HOME")]; }
int main(int argc, char **argv) { @autoreleasepool { try {
core_metal::PreviewStartup p; p.prepare(argc,argv); return 0;
} catch(const std::exception &e) { fprintf(stderr,"%s\\n",e.what()); return 2; } } }
''')
        subprocess.run(['clang++', '-std=c++17', '-framework', 'Foundation', str(source),
                        '-o', str(cls.mac / 'orbiter-core-preview')], check=True, capture_output=True)

    def test_preflight_before_session_writes(self):
        rows = []
        for case in ('normal', 'missing-earth-dir', 'wrong-earth-type', 'wrong-file-type',
                     'unreadable-root-config', 'missing-fixture', 'unreadable-fixture'):
            with self.subTest(case=case):
                base = self.base / case
                content = base / 'content'
                home = base / 'home'
                content.mkdir(parents=True)
                home.mkdir()
                for name in ('Orbiter.cfg', 'keymap.cfg', 'Config/Earth.cfg',
                             'Textures/Earth/Archive/Surf.tree',
                             'Modules/Celbody/Earth/Atmosphere/EarthAtmJ71G.dylib',
                             'Modules/DeltaGlider.dylib'):
                    f = content / name
                    f.parent.mkdir(parents=True, exist_ok=True)
                    f.write_text('source sentinel\n')
                (content / 'Meshes').mkdir()
                (content / 'Config/Earth').mkdir()
                for name in ('MetalEarth.cfg', 'MetalTerrainVessel.scn'):
                    f = self.res / name
                    if f.exists():
                        f.chmod(0o600)
                    f.write_text('private fixture\n')
                if case == 'missing-earth-dir':
                    (content / 'Config/Earth').rmdir()
                elif case == 'wrong-earth-type':
                    (content / 'Config/Earth').rmdir()
                    (content / 'Config/Earth').write_text('not a directory')
                elif case == 'wrong-file-type':
                    (content / 'Orbiter.cfg').unlink()
                    (content / 'Orbiter.cfg').mkdir()
                elif case == 'unreadable-root-config':
                    (content / 'keymap.cfg').chmod(0)
                elif case == 'missing-fixture':
                    (self.res / 'MetalEarth.cfg').unlink()
                elif case == 'unreadable-fixture':
                    (self.res / 'MetalEarth.cfg').chmod(0)
                def inventory():
                    return {str(p.relative_to(content)): (p.stat().st_mode,
                            hashlib.sha256(p.read_bytes()).hexdigest() if p.is_file() and os.access(p, os.R_OK) else None)
                            for p in content.rglob('*')}
                before = inventory()
                (self.res / 'content-root.txt').write_text(str(content) + '\n')
                p = subprocess.run([str(self.mac / 'orbiter-core-preview'), '--self-test'],
                                   env=dict(os.environ, TEST_HOME=str(home)), capture_output=True, text=True)
                sessions = list(home.rglob('session-*'))
                rows.append(dict(case=case, exit=p.returncode, sessions=len(sessions), stderr=p.stderr,
                                 source_unchanged=inventory() == before))
                self.assertEqual(inventory(), before)
                self.assertEqual(p.returncode, 0 if case == 'normal' else 2)
                self.assertEqual(len(sessions), 1 if case == 'normal' else 0)
                if case != 'normal':
                    self.assertFalse((home / 'Library').exists(), 'preflight wrote session parent')
        (self.base / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
        print('CPU evidence:', self.base)


class RunnerBinding(unittest.TestCase):
    def test_inventory_rejects_file_and_directory_symlinks(self):
        for directory in (False, True):
            base = Path(tempfile.mkdtemp(prefix='preview-inventory-', dir=os.environ['TMPDIR']))
            root = base / 'content'
            root.mkdir()
            target = base / 'target'
            if directory:
                target.mkdir()
                (target / 'asset').write_text('sentinel')
            else:
                target.write_text('sentinel')
            (root / 'link').symlink_to(target, target_is_directory=directory)
            with self.assertRaisesRegex(RuntimeError, 'symlinks'):
                runner.digest_tree(root)

    def test_wrong_or_missing_content_never_launches(self):
        for case in ('mismatch', 'nonexistent'):
            with self.subTest(case=case):
                base = Path(tempfile.mkdtemp(prefix='preview-binding-', dir=os.environ['TMPDIR']))
                bundle = base / 'OrbiterCorePreview.app'
                res = bundle / 'Contents/Resources'
                res.mkdir(parents=True)
                actual = base / 'actual'
                actual.mkdir()
                supplied = base / case
                if case == 'mismatch':
                    supplied.mkdir()
                (res / 'content-root.txt').write_text(str(actual) + '\n')
                with patch.object(sys, 'argv', ['runner', str(bundle), str(supplied), str(base / 'output')]), \
                     patch.object(runner.subprocess, 'run') as launch:
                    with self.assertRaisesRegex(RuntimeError, 'content.*(directory|match)'):
                        runner.main()
                    launch.assert_not_called()

    def test_presentation_counts_accept_positive_but_reject_inconsistent(self):
        text = ('[core-metal] drawable_acquired=24 present_scheduled=24 actual_presented=7 hidden_checks=24\n'
                '[core-metal] update=24 newframe=24 render=24 drawdata=24 display=24 completed=24 presented=7 readback=24 errors=0\n')
        self.assertEqual(runner.verify_presentation(text), 7)
        with self.assertRaises(RuntimeError):
            runner.verify_presentation(text.replace('presented=7 readback', 'presented=0 readback'))
        with self.assertRaises(RuntimeError):
            runner.verify_presentation(text.replace('=7', '=25'))


if __name__ == '__main__':
    unittest.main()
