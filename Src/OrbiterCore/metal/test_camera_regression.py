#!/usr/bin/env python3
"""Staging safety tests; /usr/bin/false deliberately stops before graphics."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from camera_regression import image


class StagingSafety(unittest.TestCase):
    def test_image_rejects_non_mesh_palette(self):
        with tempfile.TemporaryDirectory(dir=Path.home() / '.hermes/cache/scratch') as tmp:
            path = Path(tmp) / 'contaminated.ppm'
            # Parser boundary only: never used as renderer evidence.
            path.write_bytes(b'P6\n640 480\n255\n' + bytes((32, 64, 128))*2000
                             + bytes((100, 100, 100))*2000 + bytes((25, 50, 100))*(640*480-4000))
            with self.assertRaisesRegex(RuntimeError, 'non-mesh'):
                image(path)

    def test_collision_and_existing_output_changed_root(self):
        with tempfile.TemporaryDirectory(dir=Path.home() / '.hermes/cache/scratch') as tmp:
            root = Path(tmp).resolve()
            old, source, output = [root / name for name in ('old', 'source', 'output')]
            for tree in (old, source):
                (tree / 'Config').mkdir(parents=True)
                (tree / 'Meshes').mkdir()
            sentinel = source / 'Config' / 'MetalCamera.cfg'
            sentinel.write_bytes(b'original source sentinel\n')
            output.mkdir()
            # An unchecked previous launch tree must not be reused or deleted.
            stale = output / 'content'
            stale.symlink_to(old, target_is_directory=True)
            result = subprocess.run([sys.executable, str(Path(__file__).with_name('camera_regression.py')),
                                     '/usr/bin/false', str(source), str(output)], capture_output=True)
            self.assertNotEqual(result.returncode, 0)  # deliberately non-rendering executable
            self.assertEqual(sentinel.read_bytes(), b'original source sentinel\n')
            self.assertTrue(stale.is_symlink())
            self.assertEqual(stale.resolve(), old)
            launches = list(output.glob('content-*'))
            self.assertEqual(len(launches), 1)
            launch = launches[0]
            fixture = launch / 'Config' / 'MetalCamera.cfg'
            self.assertFalse(fixture.is_symlink())
            self.assertEqual(fixture.read_bytes(), Path(__file__).with_name('fixtures').joinpath('MetalCamera.cfg').read_bytes())
            self.assertEqual((launch / 'Meshes').resolve(), source / 'Meshes')
            self.assertFalse((old / 'Config' / 'MetalCamera.cfg').exists())


if __name__ == '__main__':
    unittest.main()
