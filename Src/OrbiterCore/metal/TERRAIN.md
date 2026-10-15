# Bounded actual-engine curved terrain patch

Opt-in hidden/offscreen test slice. This is **not** general low-flight coverage,
a streaming/LOD renderer, a globe replacement, collision/contact terrain, or an
interactive launch. Existing globe/vessel shaders and modes are unchanged.

```sh
cmake --build out/build/macos-core-spike --target orbiter-core-metal -j8
python3 -m unittest discover -s Src/OrbiterCore/metal -p 'test_*.py' -v
python3 Src/OrbiterCore/metal/terrain_regression.py \
 out/build/macos-core-spike/Src/OrbiterCore/metal/orbiter-core-metal \
 out/stage/content /Users/redezem/.hermes/cache/scratch/orbiter-terrain-proof-new
```

The runner stages a fresh private Sun+Earth launch tree using the established
fixture helper, real native Sun/Earth/VSOP87 modules and installed data. It never
edits source configs, modules, archives or scenarios. Missing actual data fails.

## Scope and implementation

`terrain_geometry.h` defines fixed bounded regions, geographic addressing and
curved body points. `terrain_patch.h` owns the actual API sampling, core ZTreeMgr
Surf ancestor lookup, committed DDS decoder, GPU allocations, anchor transform and
explicit SDK ground observer. Main adds lifecycle/CLI hooks only.

* `--terrain-patch snapshot.ppm`: fixed test-controlled actual-engine camera.
* `--terrain-patch-test snapshot.ppm`: two actual-engine orientations, changed at
  the midpoint of the bounded frame count. These are explicitly **not** preserved
  scenario-camera views. Both modes accept `--terrain-coast` for KSC.
* Test-only controls: `--diagnostic-flat-terrain`,
  `--diagnostic-freeze-terrain-transform`, `--diagnostic-swap-terrain-uv`.
  Flags follow the snapshot filename. No OS input/window activation is used.

The mountain witness was selected from installed Elev archive `(10,19,92)` by the
prior scalar acceptance. The fixed child patch is Surf archive `(14,311,1484)`.
KSC uses transcribed Pad39B `(-80.6209,+28.6235)` degrees and the containing
archive `(17,2793,4522)`. Each patch has 32x32 cells/1089 vertices/2048 triangles;
endpoints are inset 0.001 tile fraction to avoid cross-tile boundary ambiguity.
Live radius is obtained from the engine, not fixed in production geometry.

Height comes only from `oapiSurfaceElevationEx(Earth,lng,lat,21,cache,null,&actual)`
with ElevMode=2 and archive-source flags=2 set before Launch. Every vertex logs
actual metres/SDK level; no nonzero-height requirement is placed on coastal data.
Caller cache is RAII-owned and released before world close, including exceptions.
There is no runtime Python/oracle terrain path.

Double body points `(R+h)*unit(lng,lat)` subtract a fixed double body-local
mean-radius anchor **before** float vertex conversion. Anchor camera translation
is computed in double from actual body/camera global positions and both rotations;
float local offsets use the corresponding rotation. The patch-specific frustum
is 10..200000 m. SDK ground observer mean-radius altitude is 30000 m mountain or
3500 m KSC, aimed vertically down; yaw 0/.55 radians gives the two controlled views.
See `Camera.cpp` ground-mode handling at 279-295, mean-radius altitude parser at
1709-1716, and ground-frame rotation at 1078-1083. Production code does not create
an alternate camera pose or modify world positions.

Surf payload selection uses the configured engine Earth texture path and actual
core ZTreeMgr, not Bootstrap. Missing/metadata-only requested nodes walk to an
available ancestor; payload sizes/texture allocations are bounded. Exact
ancestor UV is `((childCol+u)/2^d-ancestorCol,(childRow+v)/2^d-ancestorRow)` with
west-to-east U and north-to-south V. Elevation grid row order is separate.
Sampling is unlit, opaque RGBA8Unorm, nearest/clamp mip0. Geometric normals are
computed/checked from curved triangle edges, not SDK shading normals; the unlit
shader does not consume them. No seams/LOD transitions/lighting claims.

## Independent acceptance

The Python reference independently traverses installed TX metadata, inflates and
hashes selected Surf/Elev/Elev_mod bytes, decodes DDS with Pillow, and reuses only
the prior independent ELE parser/tensor cubic reference (never core interpolation).
It recomputes every vertex/actual level, ancestor UV, geometric normal, double
anchor precision and actual-API-pose projection. GPU matrix/vertex exports are
comparison targets, **not** the source of expected GPU pixels.

Actual completed pre-UI 640x480 GPU PPMs are checked against independent triangle
union silhouettes (2px edge tolerance) and frontmost perspective-correct texture
samples chosen from geometry only. Triangle-edge and nearest-texel-boundary
witnesses are excluded before examining observed colors. All retained samples
must match within 2 channel units, at least 128 per view; measured positives have
zero channel error. The same unchanged positive image gate must reject real GPU
flat-height, frozen-bound-matrix and wrong-UV runs; each renderer still completes
normal teardown, while a standalone oracle exits nonzero. Image checks precede
numeric export checks, so negative rejection cannot be credited merely to logged
vertex/matrix differences.

Acceptance repeats each region three times, runs a separate fixed-camera mode,
and records actual API poses, uploaded geometry, source paths/addresses/hashes,
sparse fallback walks, raw engine logs, PPMs and oracle witnesses. It additionally
requires substantial mountain-vs-flat actual GPU silhouette/pixel differences.
A CPU radius-scale-float control must demonstrate >0.05 m lost precision while
anchored local floats retain <0.002 m error. This is a numeric precision control,
not a claimed float-precision GPU causal negative.

No depth-buffer readback is claimed: actual relief is established by independent
projected geometry, texture pixels and silhouette change. No atmosphere, water,
shadows, terrain/vessel occlusion, full geographic coverage, dynamic camera flight,
scientific ephemeris accuracy, contact physics or touchdown accuracy is established.
The existing focus vessel is loaded for the fixture/lifecycle but not drawn in
terrain-only mode; it remains drawn normally in all legacy modes.
