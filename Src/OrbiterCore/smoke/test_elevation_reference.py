"""Boundary negatives mutate a real installed payload, never invent terrain."""
import io
from pathlib import Path
import struct
import unittest
import zlib
from elevation_regression import Archive, grid, interpolate, coordinate


class InstalledPayloadTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = Path(__file__).resolve().parents[3]/'out/stage/content/Textures/Earth/Archive/Elev.tree'
        cls.archive = Archive(path)
        key = (10, 19, 92)
        index = cls.archive.tiles[key]
        pos, length, *_ = cls.archive.nodes[index]
        end = cls.archive.nodes[index+1][0]
        cls.archive.f.seek(cls.archive.offset+pos)
        cls.raw = zlib.decompress(cls.archive.f.read(end-pos))
        assert len(cls.raw) == length

    def decode(self, payload):
        compressed = zlib.compress(payload)
        a = Archive.__new__(Archive)
        a.cache, a.tiles = {}, {(4, 0, 0): 0}
        a.nodes = [(0, len(payload))]
        a.length, a.offset, a.f = len(compressed), 0, io.BytesIO(compressed)
        return a.tile((4, 0, 0))

    def test_installed_golden_scalar(self):
        tile = self.decode(self.raw)
        self.assertEqual(tile['sha256'], '5170cd064d56cae44955d45dc53081750ee0dbcca6efb2ff37e30b8c56e26a48')
        data, _ = grid(tile, None)
        # Actual archived scalar at a provenance-pinned grid coordinate.
        lng, lat = coordinate((10, 19, 92), 64, 64)
        self.assertEqual(interpolate(data, (10, 19, 92), lng, lat, 1), 5968.0)

    def test_truncated_header(self):
        with self.assertRaises(AssertionError):
            self.decode(self.raw[:99])

    def test_truncated_body(self):
        with self.assertRaises(AssertionError):
            self.decode(self.raw[:-1])

    def test_bad_magic(self):
        with self.assertRaises(AssertionError):
            self.decode(b'BAD!'+self.raw[4:])

    def test_unsupported_signed8_unsigned16_float(self):
        for dtype in (-8, 16, 32, -32):
            with self.subTest(dtype=dtype), self.assertRaises(AssertionError):
                raw = bytearray(self.raw)
                struct.pack_into('<i', raw, 8, dtype)
                self.decode(raw)

    def test_bad_border_and_header_length(self):
        for offset, value in ((4, 99), (4, len(self.raw)+1), (12, 258), (20, 0), (24, 2)):
            with self.subTest(offset=offset, value=value), self.assertRaises(AssertionError):
                raw = bytearray(self.raw)
                struct.pack_into('<i', raw, offset, value)
                self.decode(raw)


if __name__ == '__main__':
    unittest.main()
