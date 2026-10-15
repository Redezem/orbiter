import math
import unittest
from terrain_vessel_regression import clipped, raster, near_edges, vessel_project


class SharedSceneGeometryTests(unittest.TestCase):
    def test_common_near_far_clip(self):
        screen=[(0.,0.,.1,0.,0.),(640.,0.,2.,1.,0.),(320.,480.,6000.,.5,1.)]
        vs,tris=clipped(screen,[((0,1,2),None)],math.radians(20))
        self.assertTrue(tris)
        self.assertTrue(all(.5-1e-10<=v[2]<=5000.+1e-10 for v in vs))
        self.assertTrue(all(math.isfinite(x) for v in vs for x in v))

    def test_scalar_front_depth_and_order(self):
        vs=[]
        for z in (20.,10.):
            vs.extend([(10.,10.,z,0.,0.),(30.,10.,z,1.,0.),(10.,30.,z,0.,1.)])
        tris=[((0,1,2),'back'),((3,4,5),'front')]
        for ts in (tris,list(reversed(tris))):
            hits=raster(vs,ts)
            self.assertEqual(hits[15,15]['texture'],'front')
            self.assertAlmostEqual(hits[15,15]['z'],10.)
            self.assertTrue(hits[15,15]['safe'])

    def test_half_pixel_bound(self):
        vs=[(5.49,5.49,10.,0.,0.),(5.6,5.49,10.,0.,0.),(5.49,5.6,10.,0.,0.)]
        self.assertIn((5,5),raster(vs,[((0,1,2),None)]))

    def test_any_edge_including_outside_coverage(self):
        vs=[(5.51,0.,10.),(5.51,20.,10.),(20.,20.,10.)]
        self.assertIn((5,8),near_edges(vs,[((0,1,2),None)]))
        self.assertNotIn((3,8),near_edges(vs,[((0,1,2),None)]))

    def test_scalar_pose_not_bound_matrix(self):
        identity=[1,0,0,0,1,0,0,0,1]
        frame=dict(camera_rotation=identity,vessel_rotation=identity,
                   vessel_position=[1e11,0,100],camera_position=[1e11,0,0],aperture=math.pi/4,
                   clip_matrix=[0]*16)
        point=vessel_project(frame,{'vertices':[[1,2,0,1,0,0,1,1,1,1]]})[0]
        self.assertAlmostEqual(point[0],322.4)
        self.assertAlmostEqual(point[1],235.2)
        self.assertEqual(point[2],100)


if __name__=='__main__':unittest.main()
