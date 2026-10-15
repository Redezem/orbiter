# Real-engine elevation scalar acceptance (CPU-only)

This opt-in probe links **OrbiterCore**, launches the real native Sun+Earth fixture,
then calls `oapiSurfaceElevationEx`. It creates no window or graphics client and
introduces no terrain renderer or alternative simulator.

```sh
cmake -S . -B out/build/macos-core-spike -DORBITER_BUILD_ELEVATION_PROBE=ON
cmake --build out/build/macos-core-spike --target orbiter-elevation-probe -j8
python3 -m unittest discover -s Src/OrbiterCore/smoke -p 'test_elevation_reference.py' -v
python3 Src/OrbiterCore/smoke/elevation_regression.py \
  out/build/macos-core-spike/Src/OrbiterCore/orbiter-elevation-probe \
  out/stage/content /Users/redezem/.hermes/cache/scratch/orbiter-elevation-proof
```

Requires installed Earth Elev/Elev_mod archives, native Earth/Sun/VSOP87 modules,
VSOP87B.dat, and the existing DeltaGlider fixture dependency. Missing assets fail;
there is no generated terrain fallback. The Python driver stages a fresh owned
launch tree using the existing safe fixture helper. It copies master config and
reserves fixture config/scenario files before linking installed assets. Each run
retains scalar input, process output, engine log and JSON payload provenance.

## Source contract and actual acceptance

* `OrbiterAPI.cpp:1233-1255`: public argument order is
  `(planet, longitude, latitude, requestedSDKLevel, cache, normal, actualSDKLevel)`.
  Radians in, metres out. There is no explicit manager handle in this call.
* `elevmgr.cpp:325,370-385,505`: requested SDK level is reduced by seven,
  clamped to 0..14 for installed Earth, then archive level is internal+4.
  Returned SDK level is internal+7: **archive = actualSDKLevel-3**. Requested
  zero means maximum, not disabled. Actual zero means no successfully sampled
  tile; a zero *height* with nonzero actual level can be real coastal terrain.
* `elevmgr.cpp:38-54` captures visual mode, source flags and 0.5 m Earth target
  resolution on manager construction. Probe sets mode 1/2 and archives-only
  flags 2 **after Create, before Launch**; mode 0 is a real-engine negative.
  Do not change mode after Launch and assume an existing manager will follow.
  Actual `Config.cpp:125` default is mode **2**, not zero; `:682-683` only
  accepts explicit config key values 0/1, despite engine cubic mode 2. Direct
  controlled fixture configuration avoids depending on that parser limitation.
* `elevmgr.cpp:121-304`: loader supports flat 0, **unsigned**8 and signed16;
  real scalar witnesses exercise unsigned8 and signed16, not flat payloads.
  Signed8, unsigned16 and float are not implemented by
  this engine loader and the oracle explicitly rejects them. Header 100 bytes,
  `ELE\1`, 259x259 with one border sample, scale/offset doubles. Quantization
  truncates scale/target-resolution and offset/target-resolution separately to
  INT16; apply modification before interpolation, preserve sentinel 255 or
  32767 samples, flat modification replaces whole tile. Earth output is then
  multiplied by 0.5 m. This is not generic float terrain loading.
* `elevmgr.cpp:382-425,442-476`: tile row index increases southward, but payload
  sample rows increase **northward**. Strip one padding row/column; 257 endpoint
  samples span 256 intervals. Mode 1 is bilinear; mode 2 cardinal cubic needs
  a 4x4 stencil including border samples. Oracle evaluates independent tensor
  basis weights rather than copying the engine's nested polynomial routine.
  Some installed coastal headers have all geographic bounds zero; use archive
  quadtree address for projection, not these optional header values.
* Caller-owned cache size two exercises repeated hits, different request levels,
  fallback, multiple regions and evictions; compare against null-cache engine
  route for every scalar. ReleaseTileCache precedes world CloseSession. No cache
  or planet handle may survive world close. There is no elevation filter graphics
  client in this CPU fixture, so archive/reference equivalence is intentional.

The installed-data oracle traverses TX/1 metadata, decompresses actual matching
payloads with Python zlib, checks bounds/formats and hashes, applies sentinel
mods and samples independently. Fixed KSC coordinates were transcribed from
installed Earth.cfg (the runner does not parse that observer dynamically);
mountain coordinates are selected from actual root maxima then refined within
available archive coverage. Mountain patch is archive (10,19,92), SDK13 even for
request21; KSC uses archive17, SDK20. Do not label either SDK21 terrain.

Acceptance runs mode1/2 fractional coastal/mountain/modification samples and
padding stencils, plus mixed request levels 7,10,14,21,0. All returned actual
levels must match independently traversed coverage and be nonzero. Actual-engine
mode0 and swapped argument runs must fail the original oracle. Frozen123 m and
ignored-modification CPU controls must also be distinguishable. Parser unit tests
corrupt a real payload (truncate, invalid magic/header/grid/padding/format); they
never supply invented elevations to the engine. A pinned mountain payload hash
and exact 5968 m scalar provide a repeatable installed-data golden.

## Next boundary: curved anchored local patch, NOT implemented here

Only after this CPU acceptance, add a bounded real-engine local patch, not a
parallel heightfield world. For each sampled longitude/latitude, form in DOUBLE:

```
u = (cos(lat)*cos(lng), sin(lat), cos(lat)*sin(lng))
pBody = (liveEarthRadius + engineHeightMetres) * u
vertexFloat = float3(pBody - anchorBodyDouble)
anchorCameraDouble = transpose(cameraRotation) *
    ((bodyGlobalPosition - cameraGlobalPosition) + bodyRotation * anchorBodyDouble)
```

Keep the anchor in body-local double precision; subtract it BEFORE float
conversion. Transform offsets by body/camera rotations, and translate by the
camera-relative anchor, never by radius-scale float vertices or AU-scale floats.
Use the live body's radius, rotation and camera APIs; no height exaggeration.

Surface tile internal patch level P maps to archive P+4, but elevation SDK
requests and actual fallback are a separate contract. Resolve a *real available*
Surf ancestor at internal A<=P. For child `(ilat,ilng)` and local north-to-south
texture UV `(u,v)`, d=P-A and ancestor row/col `(ar,ac)`:

```
uAncestor = (ilng + u) / 2^d - ac
vAncestor = (ilat + v) / 2^d - ar
```

Use exact ancestor subrectangle, not UV reset to 0..1 on each child and not the
south-to-north elevation row convention. Verify Surf payload availability (a TOC
node can exist solely to point to descendants), longitude wrap and boundaries.
Before rendering acceptance require independent GPU depth/geometry and imagery
witnesses at both KSC and the available mountain patch; frozen/zero elevation,
wrong level, bad UV ancestor, and radius-scale float controls must fail. Current
coarse unlit globe remains unchanged and is not local-flight terrain.

Not established here: exact +/-pi or pole endpoint sampling, arbitrary loose-file
priority/filter callbacks, normals, collision/contact behavior, asynchronous cache
sharing, atmosphere, general terrain renderer or sub-metre touchdown accuracy.
No core dependency fix was necessary.
