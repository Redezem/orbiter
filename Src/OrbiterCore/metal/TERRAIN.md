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

For these terrain-only modes, no depth-buffer readback is claimed: actual relief is established by independent
projected geometry, texture pixels and silhouette change. No atmosphere, water,
shadows, terrain/vessel occlusion, full geographic coverage, dynamic camera flight,
scientific ephemeris accuracy, contact physics or touchdown accuracy is established.
The existing focus vessel is loaded for the fixture/lifecycle but not drawn in
terrain-only mode; it remains drawn normally in all legacy modes.

## Combined real-engine vessel + anchored KSC terrain

```sh
python3 Src/OrbiterCore/metal/terrain_vessel_regression.py \
 out/build/macos-core-spike/Src/OrbiterCore/metal/orbiter-core-metal \
 out/stage/content /Users/redezem/.hermes/cache/scratch/orbiter-terrain-vessel-proof
```

`<scenario> 12 --terrain-vessel-test /absolute/snapshot.ppm` explicitly draws
the actual focus-vessel template and the same KSC archive(17,2793,4522) patch.
No terrain heights, mesh offsets or world/model poses are altered for visibility.
The private fixture `MetalTerrainVessel.scn` initializes the DG at the exact KSC
patch midpoint (-80.628662109375,+28.619384765625 degrees), with `STATUS Landed
Earth`, heading0 and gear0. Actual SDK `CameraMode_Track::TM_RELATIVE` targets the
real vessel, distance4 vessel sizes, phi2.74/2.04 and theta+.02 radians, FOV40.
Camera.cpp:260-277 resolves these SDK settings, and Vessel.cpp:3460-3544 owns
landed initialization. Every frame records actual camera/vessel/body API doubles,
rotations and bound matrices; production uses no alternate client camera pose.

Both objects use positive-forward Metal projection, common near0.5/far5000 m,
the same render encoder and the same Depth32Float attachment with Less/write
enabled. Vessel global-camera subtraction and terrain anchor translation remain
double precision before local float upload. Existing terrain-only and legacy
vessel/globe modes retain their original frusta, draw paths and assertions.
The combined close view can cover the old clear-corner pixel, so only this mode
uses independent object silhouettes/pixel oracles instead of a clear-corner gate.

Trailing diagnostic-only controls actually omit GPU draws, not image content:
`--diagnostic-omit-vessel`, `--diagnostic-omit-terrain`. They retain the same
loaded scenario, camera, geometry and resources. `--diagnostic-terrain-last`
reverses object order with depth intact. `--diagnostic-disable-scene-depth`
unbinds the vessel depth test/write state while terrain is drawn first; both
object draws otherwise remain real. No hidden presentation/session/gear mode is
combined with this bounded slice. All five renderer lifecycles exit0 with12
completed pre-UI readbacks, normal ImGui/engine shutdown and zero Metal errors.

The independent oracle reconstructs terrain solely from installed ELE/Elev_mod
and Surf payloads, uses engine API-uploaded source vessel geometry/material/UV
and API slot provenance (no second mesh parser), independently Pillow-decodes
the source DDS, and scalar-projects both from API world poses, not GPU matrices.
It clips terrain against the common near/far planes and rasterizes fixed integer
pixel centres with perspective attributes/depth. Geometry-only exclusions cover
all foreground vessel triangle edges (including just-outside foreground
triangles), terrain projected edge distances, nearest-texel boundaries and
near-coplanar Depth32Float layers. Overlap witnesses additionally lie inside a
2px-eroded independent vessel silhouette. No observed color selects a witness.
Independent solo silhouettes allow2px raster-boundary tolerance; all safe texture
samples must match within2 channels. Numeric checks retain every height/level/UV
and local-float test; only the *screen projection* comparison in close views
excludes offscreen/behind-eye vertices whose pixel division is ill-conditioned.
The bound terrain depth clip coordinates are independently checked for every
vertex, including clipped/offscreen vertices, against the common frustum.

Measured first-view acceptance: **8 terrain-front +101 vessel-front nonedge
overlap witnesses**, **86895 terrain +543 vessel texture witnesses**, zero channel
mismatches and zero solo silhouette errors. Reversing object order produces a
byte-identical real GPU PPM. The depth-disabled renderer still closes normally,
but the same standalone oracle exits1: all8 selected terrain-front overlap
witnesses are wrong (472 total safe pixel mismatches including self-occlusion).
This establishes measured occlusion by independent expected depth ordering and
actual pixel output, not a claimed depth-buffer readback.

**State/physics limitation:** the real first-render status readback is0/active,
not landed, despite the scenario's landed initialization. The static template is
not gear-deformed; its independently measured vertex-to-installed-elevation
clearance spans -4.98038..+4.79823m. Penetration is retained and explicitly exposed,
not fixed with renderer offsets or represented as correct contact simulation.
The installed Earth config declares EarthAtmJ71G; actual engine logs also report
a missing selected VenusAtm2006 native atmosphere module. No atmosphere/drag,
touchdown physics, animation, arbitrary flight/camera coverage, streaming/LOD,
lighting, shadows, HUD or whole-application claim is made.
