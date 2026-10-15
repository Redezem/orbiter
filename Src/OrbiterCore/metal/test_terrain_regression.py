import math
from pathlib import Path
import unittest
from terrain_regression import Reference, coordinates, geography, point


CONTENT=Path(__file__).resolve().parents[3]/'out/stage/content'


class InstalledTerrainContracts(unittest.TestCase):
    def test_fixed_geography_is_inside_known_elevation_coverage(self):
        key=geography(False)
        for row,col in ((0,0),(16,16),(32,32)):
            lng,lat=coordinates(key,row,col)
            # Independent ancestor arithmetic: installed mountain archive(10,19,92).
            self.assertEqual((int((.5-lat/math.pi)*64),int((1+lng/math.pi)*64)),(19,92))
        lng,lat=map(math.radians,(-80.6209,28.6235))
        level,r,c=geography(True);n=2**(level-4)
        self.assertLessEqual(c,(1+lng/math.pi)*n)
        self.assertLess((1+lng/math.pi)*n,c+1)
        self.assertLessEqual(r,(.5-lat/math.pi)*n)
        self.assertLess((.5-lat/math.pi)*n,r+1)

    def test_actual_sparse_surface_and_subrectangle(self):
        ref=Reference(CONTENT,False)
        self.addCleanup(ref.close)
        self.assertLess(ref.ancestor[0],ref.key[0])
        self.assertFalse(ref.ancestors[0]['payload'])
        self.assertTrue(ref.ancestors[-1]['payload'])
        verts=ref.geometry(6371010.) # fixture config known; live GPU path reads API radius.
        uv0,uv1=verts[0]['uv'],verts[-1]['uv']
        self.assertLess(uv0[0],uv1[0]);self.assertLess(uv0[1],uv1[1])
        self.assertGreater(uv0[0],0);self.assertLess(uv1[0],1)
        self.assertAlmostEqual(uv1[0]-uv0[0],.998/2**(ref.key[0]-ref.ancestor[0]))
        self.assertEqual(set(v['actual'] for v in verts),{13})
        self.assertGreater(max(v['h'] for v in verts)-min(v['h'] for v in verts),1000)

    def test_installed_coast_valid_levels_including_zero_height(self):
        ref=Reference(CONTENT,True)
        self.addCleanup(ref.close)
        verts=ref.geometry(6371010.)
        self.assertEqual(set(v['actual'] for v in verts),{20})
        self.assertTrue(any(v['h']==0 for v in verts))
        self.assertGreater(len(ref.provenance),0)

    def test_curvature_and_precision_contract(self):
        key=geography(False);lng,lat=coordinates(key,16,16)
        anchor=point(6371010,0,lng,lat)
        p=point(6371010,0,*coordinates(key,0,0))
        radial=[v/6371010 for v in anchor]
        # Curved sphere drops below the anchor tangent plane; not a flat heightfield.
        sag=sum((a-b)*u for a,b,u in zip(p,anchor,radial))
        self.assertLess(sag,-5)


if __name__=='__main__':unittest.main()
