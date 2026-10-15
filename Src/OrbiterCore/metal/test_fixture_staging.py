#!/usr/bin/env python3
"""Real filesystem sentinel regressions; false exits before any graphics launch."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
RUNNERS = ('camera', 'material', 'texture', 'session', 'buffered', 'imgui_input')


class FixtureStaging(unittest.TestCase):
    def check_stage(self, runner, fixture='MetalCamera.cfg', config='Config', scenarios='Scenarios'):
        with tempfile.TemporaryDirectory(dir=Path.home()/'.hermes/cache/scratch') as tmp:
            root = Path(tmp)
            source, output = root/'source', root/'output'
            (source/config).mkdir(parents=True)
            (source/scenarios).mkdir()
            (source/'Meshes').mkdir()
            sentinel = source/config/fixture
            sentinel.write_bytes(b'SOURCE-SENTINEL\n')
            scenario = source/scenarios/'MetalCamera.scn'
            scenario.write_bytes(b'SCENARIO-SENTINEL\n')
            (source/'Orbiter.cfg').write_bytes(b'ROOT-SENTINEL\n')
            # Existing output and dangling symlinks must not be reused/deleted.
            output.mkdir()
            stale = output/'content'
            stale.symlink_to(source, target_is_directory=True)
            dangling = source/config/'unused.cfg'
            dangling.symlink_to(source/'absent')
            result = subprocess.run([sys.executable, str(HERE/(runner+'_regression.py')),
                                     '/usr/bin/false', str(source), str(output)], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(sentinel.read_bytes(), b'SOURCE-SENTINEL\n', result.stderr)
            self.assertEqual(scenario.read_bytes(), b'SCENARIO-SENTINEL\n')
            self.assertTrue(stale.is_symlink())
            self.assertEqual(stale.resolve(), source)
            launches = [p for p in output.iterdir() if p.is_dir() and not p.is_symlink()]
            self.assertEqual(len(launches), 1, result.stderr)
            launch = launches[0]
            for folder, name in [('Config', 'MetalCamera.cfg'), ('Scenarios', 'MetalCamera.scn')]:
                self.assertFalse((launch/folder).is_symlink())
                staged = launch/folder/name
                self.assertFalse(staged.is_symlink())
                self.assertEqual(staged.read_bytes(), (HERE/'fixtures'/name).read_bytes())
            self.assertTrue(os.path.lexists(launch/'Config'/'unused.cfg'))
            self.assertEqual((launch/'Meshes').resolve(), source/'Meshes')
            self.assertFalse((launch/'Orbiter.cfg').is_symlink())
            (launch/'Orbiter.cfg').write_bytes(b'ISOLATED\n')
            self.assertEqual((source/'Orbiter.cfg').read_bytes(), b'ROOT-SENTINEL\n')

    def test_fresh_roots_and_symlinked_fixture_source(self):
        from fixture_staging import stage
        with tempfile.TemporaryDirectory(dir=Path.home()/'.hermes/cache/scratch') as tmp:
            root = Path(tmp)
            source, output = root/'source', root/'output'
            (source/'Config').mkdir(parents=True)
            output.mkdir()
            sentinel = root/'sentinel'
            sentinel.write_bytes(b'EXTERNAL-SENTINEL\n')
            (source/'Config'/'metalcamera.cfg').symlink_to(sentinel)
            first, second = stage(source, output), stage(source, output)
            self.assertNotEqual(first, second)
            self.assertTrue(first.is_dir())
            self.assertTrue(second.is_dir())
            self.assertEqual(sentinel.read_bytes(), b'EXTERNAL-SENTINEL\n')
            for launch in (first, second):
                self.assertFalse((launch/'Config'/'MetalCamera.cfg').is_symlink())
                self.assertEqual((launch/'Config'/'MetalCamera.cfg').read_bytes(),
                                 (HERE/'fixtures'/'MetalCamera.cfg').read_bytes())

    def test_exact_reserved_fixture_and_stale_output(self):
        for runner in RUNNERS:
            with self.subTest(runner=runner):
                self.check_stage(runner)

    def test_case_variant_reserved_fixture(self):
        with tempfile.TemporaryDirectory(dir=Path.home()/'.hermes/cache/scratch') as tmp:
            probe = Path(tmp)/'probe'; probe.touch()
            if not (Path(tmp)/'PROBE').exists():
                self.skipTest('actual staging filesystem is case-sensitive')
        for runner in RUNNERS:
            for name in ('metalcamera.cfg', 'METALCAMERA.CFG'):
                with self.subTest(runner=runner, name=name):
                    self.check_stage(runner, fixture=name)

    def test_case_variant_reserved_directories(self):
        with tempfile.TemporaryDirectory(dir=Path.home()/'.hermes/cache/scratch') as tmp:
            probe = Path(tmp)/'probe'; probe.touch()
            if not (Path(tmp)/'PROBE').exists():
                self.skipTest('actual staging filesystem is case-sensitive')
        for runner in RUNNERS:
            for config, scenarios in [('config', 'Scenarios'), ('Config', 'scenarios'), ('CONFIG', 'SCENARIOS')]:
                with self.subTest(runner=runner, config=config, scenarios=scenarios):
                    self.check_stage(runner, config=config, scenarios=scenarios)


if __name__ == '__main__':
    unittest.main()
