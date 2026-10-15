"""Nested fixture symlink/alias sentinels: no engine process needed."""
from pathlib import Path
import tempfile
import unittest
from atmosphere_regression import fixture


class AtmosphereFixture(unittest.TestCase):
    def test_nested_override_is_private_with_aliases_and_symlinked_sources(self):
        for filename in ('Atmosphere.cfg', 'atmosphere.cfg', 'ATMOSPHERE.CFG'):
            for dangling in (False, True):
                with self.subTest(filename=filename, dangling=dangling), tempfile.TemporaryDirectory(dir=Path.home()/'.hermes/cache/scratch') as tmp:
                    root = Path(tmp)
                    source, output = root/'source', root/'output'
                    (source/'Config').mkdir(parents=True)
                    output.mkdir()
                    actual = root/'earth-source'; actual.mkdir()
                    (source/'Config/Earth').symlink_to(actual, target_is_directory=True)
                    sentinel = root/'override'; sentinel.write_bytes(b'KEEP VENUS\n')
                    (actual/filename).symlink_to(root/'missing' if dangling else sentinel)
                    (actual/'Data').mkdir()
                    (source/'Orbiter.cfg').write_bytes(b'ROOT KEEP\n')
                    stale = output/'content'; stale.symlink_to(source, target_is_directory=True)
                    launch = fixture(source, output, 'EarthAtmJ71G')
                    self.assertFalse((launch/'Config').is_symlink())
                    self.assertFalse((launch/'Config/Earth').is_symlink())
                    target = launch/'Config/Earth/Atmosphere.cfg'
                    self.assertFalse(target.is_symlink())
                    self.assertEqual(target.read_text(), 'MODULE_ATM = EarthAtmJ71G\n')
                    target.write_text('MODULE_ATM = [None]\n')
                    self.assertEqual(sentinel.read_bytes(), b'KEEP VENUS\n')
                    self.assertTrue((actual/filename).is_symlink())
                    self.assertEqual((launch/'Config/Earth/Data').resolve(), actual/'Data')
                    (launch/'Orbiter.cfg').write_text('PRIVATE\n')
                    self.assertEqual((source/'Orbiter.cfg').read_bytes(), b'ROOT KEEP\n')
                    self.assertTrue(stale.is_symlink())
                    second = fixture(source, output, 'EarthAtmJ71G')
                    self.assertNotEqual(second, launch)


if __name__ == '__main__':
    unittest.main()
