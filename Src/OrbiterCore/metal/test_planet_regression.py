"""CPU-only unit fixtures, not actual-engine/GPU acceptance evidence."""
import math
import unittest
from planet_regression import pose, project, sphere_mask, samples


class PlanetOracleMath(unittest.TestCase):
    def setUp(self):
        self.f = dict(radius=7100000., camera=[1e11, 5e9, -3e10],
                      body=[1e11, 5e9, -3e10+21300000.],
                      cameraR=[1, 0, 0, 0, 1, 0, 0, 0, 1],
                      bodyR=[1, 0, 0, 0, 1, 0, 0, 0, 1], aperture=math.radians(20))

    def test_double_relative_projection(self):
        center, rotation = pose(self.f)
        self.assertEqual(center[:2], [0, 0])
        self.assertEqual(rotation, [[1, 0, 0], [0, 1, 0], [0, 0, 1]])
        x, y, z = project(self.f, [0, 0, -7100000.])
        self.assertEqual((x, y), (320, 240))
        self.assertAlmostEqual(z, 14200000., places=5)
        self.assertGreater(project(self.f, [1, 0, 0])[0], 320)
        self.assertLess(project(self.f, [0, 1, 0])[1], 240)

    def test_analytic_silhouette(self):
        # Avoid global subtraction rounding in this exact synthetic 3R test.
        self.f.update(camera=[0, 0, 0], body=[0, 0, 21300000.])
        mask = sphere_mask(self.f)
        self.assertEqual(mask.getpixel((320, 240)), 255)
        self.assertEqual(mask.getpixel((639, 479)), 0)
        ideal_radius = 240/math.tan(self.f['aperture'])/math.sqrt(8)
        self.assertAlmostEqual(ideal_radius, 233.13, places=2)
        self.assertEqual(mask.getpixel((320+int(ideal_radius)-1, 240)), 255)
        self.assertEqual(mask.getpixel((320+int(ideal_radius)+1, 240)), 0)

    def test_frontmost_triangle_and_texture_orientation(self):
        self.f.update(camera=[0, 0, 0], body=[0, 0, 21300000.])
        selected = samples(self.f)
        self.assertGreater(len(selected), 64)
        self.assertTrue(all(h == 0 for x, y, h, u, v in selected))
        for x, y, h, u, v in selected:
            self.assertTrue(0 <= u < 512 and 0 <= v < 512)
            # Compare only central samples to an independent ray/sphere UV.
            # This catches a missing frontmost half-pixel bbox candidate without
            # using any generated image or renderer matrix as expected values.
            if (x-320)**2+(y-240)**2 > 120**2:
                continue
            dx = (x+.5-320)*math.tan(self.f['aperture'])/240
            dy = (240-y-.5)*math.tan(self.f['aperture'])/240
            d2 = 1+dx*dx+dy*dy
            b = 21300000.
            t = (b-math.sqrt(b*b-d2*(21300000.**2-7100000.**2)))/d2
            p = (t*dx, t*dy, t-21300000.)
            lng, lat = math.atan2(p[2], p[0]), math.asin(p[1]/7100000.)
            self.assertLess(abs(u+.5-(lng+math.pi)/math.pi*512), 1)
            self.assertLess(abs(v+.5-(.5-lat/math.pi)*512), 1)
        # Rotate real unit fixture orientation 180 degrees: the same front disc
        # must now select the east root, never backside west.
        self.f['bodyR'] = [-1, 0, 0, 0, 1, 0, 0, 0, -1]
        self.assertTrue(all(h == 1 for x, y, h, u, v in samples(self.f)))


if __name__ == '__main__':
    unittest.main()
