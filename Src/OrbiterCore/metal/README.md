# Real-core Metal lifecycle probe

This opt-in executable is separate from the bootstrap macOS application.
It registers an actual GraphicsClient and runs the real Orbiter scenario/core loop.

## Verified milestone

Build with ORBITER_BUILD_CORE_METAL=ON, then run from the repository root:

```sh
cmake --build out/build/macos-core-spike --target orbiter-core-metal -j6
python3 Src/OrbiterCore/metal/regression.py \
  out/build/macos-core-spike/Src/OrbiterCore/metal/orbiter-core-metal \
  out/stage/content
```

Three fresh hidden offscreen lifecycles each verify 12 update/newframe/render/
drawdata/display callbacks, 12 completed Metal command buffers, 12 exact BGRA
clear-pixel readbacks, zero GPU errors, one create/close/destroy and ImGui
initialization/shutdown, normal teardown, and process exit zero.

This default mode establishes offscreen clear-frame lifecycle only. The separate
mesh mode below adds static vessel-template rendering, not a full scene renderer.
Neither establishes terrain rendering, presentation, HUD/MFD/Sketchpad, keyboard
input, or long-running resource behavior. The bootstrap app remains separate.

## Static focus-vessel mesh diagnostic

```sh
python3 Src/OrbiterCore/metal/mesh_regression.py \
  out/build/macos-core-spike/Src/OrbiterCore/metal/orbiter-core-metal \
  out/stage/content /Users/redezem/.hermes/cache/scratch/orbiter-metal-mesh
```

The explicit executable option is `--mesh /absolute/path/snapshot.ppm` after the
scenario and frame count; append `--hidden-present` to use the hidden drawable path.
This uses the actual focus `VESSEL` and `GetMeshCount`, `GetMeshTemplate`,
`GetMeshOffset`, `GetMeshVisibilityMode`, `oapiMeshGroupCount`, and `oapiMeshGroup`.
No separate mesh-file parser or replacement simulation is used. External-visible
preloaded templates are copied read-only into Metal vertex/index buffers; a null
visible template fails explicitly (on-demand mesh loading is unsupported).
The visual is registered with the engine and explicitly unregistered before
`CloseSession` destroys the world. Instance/deformation/animation callbacks remain
unsupported and return zero; an event counter makes that limitation observable.

The view is deliberately a **fixed vessel-local, oblique, auto-fit orthographic
diagnostic camera**, applying engine mesh offsets, not the engine's global camera
or world transforms. It draws untextured, grayscale normal-shaded, double-sided
triangles with a depth buffer. No materials, textures, animation, terrain, other
vessels, mesh instance edits, or dynamic focus/visibility updates are implemented.
It is a static template snapshot, not the fully animated visual state.

The `Delta-glider/Smack!` fixture uploads exactly one mesh, 122 groups, 14,339
vertices and 11,803 triangles. Three independent 12-frame runs each verify 12 draw
calls/141,636 submitted triangles, GPU completion without error, exact corner
clear BGRA, more than 1,000 mesh-shaded and clear-background pixels in every GPU
readback, normal close/destruction and visual registration/unregistration. The
runner independently parses actual PPM readback snapshots and compares repeat
signatures. Observed output on Apple M1: 83,515 mesh pixels and 223,685 background
pixels; snapshots are 640x480. These are diagnostic image counts, not evidence of
engine camera support. The new runner uses explicit failures (also under `-O`);
the original runner's deferred assertion limitation below is unchanged.

## Bounded real engine camera/world-transform verification

`--engine-camera snapshot.ppm` preserves the scenario's actual camera;
`--engine-camera-test snapshot.ppm` explicitly changes the engine's camera using
`CameraMode_Track` at frames 1 and 7 of a 12-frame run. Neither mode uses the
fixed diagnostic camera. Both upload vessel-local static templates and compute
`P * transpose(cameraRotation) * [vesselRotation, vesselGlobal-cameraGlobal]`
each render callback, subtracting positions in double precision before float
conversion. The perspective uses the engine half-vertical aperture, 480/640
aspect, positive-forward Z and Metal [0,1] depth. Near/far are explicitly
client-owned 0.1 m/1,000,000 m limits, not engine frustum policy.

The earlier camera candidate was blocked by NaN celestial/world state, including
the root Sun ephemeris dependency even in the Kepler-Vesta fixture. Native
celestial-module dependency initialization/staging now enables this bounded
fixture: stage the real Sun module and its VSOP87 runtime/data dependencies
alongside DeltaGlider (see `../modules/README.md`). No synthetic camera/world
pose, NaN clamp, or diagnostic-camera fallback is used. Full scientific Sol
satellite support is **not** established; missing Windows-only satellite modules
and incomplete scientific coverage remain outside this renderer milestone.

`camera_regression.py` now passes three scenario-preserved and three API-controlled
12-frame real-engine lifecycles. It checks independent scalar projection against
the computed matrix and the actual bound GPU uniform, completed command buffers,
GPU geometry/background, and normal engine/ImGui shutdown. Camera snapshots are
copied on the GPU **before** ImGui compositing into a separate mesh-only target;
all pixels must belong to the exact clear or grayscale shader palette. ImGui
still renders normally into the main target with the actual engine lifecycle.
The controlled mesh-only images change by 35,091 pixels in the verified run,
not the earlier UI-confounded whole-frame difference count. This proves bounded
static-template projection, not a complete scene renderer or near/far boundary
clipping, animation, dynamic focus, or visible-presentation support.

The runner atomically reserves a fresh `content-*` launch tree each invocation,
reserves the fixture config name instead of linking it, and never reuses or
recursively deletes an unchecked launch tree. Existing output is retained for
evidence; input Config/Scenarios are not modified. Run staging safety/parser
tests with `python3 Src/OrbiterCore/metal/test_camera_regression.py`.

An automatic seventh lifecycle uses the explicit diagnostic-only executable
flag `--diagnostic-frozen-camera-test`: real API viewpoints still change, but
the GPU uniform remains frame 1's matrix. The same changed-view oracle must fail
with **zero changed mesh pixels**. To run this expected-failure control alone,
append `--negative-control` to the runner command below; its expected exit is
nonzero with `GPU viewpoint did not change`. The renderer itself closes normally;
the nonzero result belongs to the image oracle. This flag is not a production
camera mode. Clear and diagnostic mesh regression behavior/signatures remain
unchanged.

```sh
python3 Src/OrbiterCore/metal/camera_regression.py \
  out/build/macos-core-spike/Src/OrbiterCore/metal/orbiter-core-metal \
  out/stage/content /Users/redezem/.hermes/cache/scratch/orbiter-engine-camera
```

## Hidden Metal drawable submission (not visible-presentation QA)

```sh
python3 Src/OrbiterCore/metal/camera_regression.py \
  out/build/macos-core-spike/Src/OrbiterCore/metal/orbiter-core-metal \
  out/stage/content /Users/redezem/.hermes/cache/scratch/orbiter-metal-presentation \
  --hidden-present
```

Append `--hidden-present` **after** the existing executable mode/snapshot options.
Clear-only `scenario 12 --hidden-present` remains supported. This opts into a real
SDL hidden window's `CAMetalLayer.nextDrawable`, renders the same engine-camera
mesh directly into its texture, GPU-copies that drawable to the mesh-only oracle
**before** actual ImGui compositing, copies the composited drawable into the
CPU-readable target, then calls `presentDrawable`, commits and waits for GPU
completion. `framebufferOnly=NO` permits these real drawable readbacks. The usual
independent projection, mesh palette and changed-view oracles remain active,
including the frozen-GPU negative control. No alternate synthetic image is used.

Every frame checks SDL hidden/no-input-focus flags; SDL's macOS background-app
hint is set before initialization. No show/raise/activate/input API is called.
A missing drawable fails explicitly and normal cleanup is attempted; the probe
never shows a window as a fallback.

Counts deliberately distinguish acquired drawables, **scheduled** presentation
requests, GPU completion and **observed** positive `drawable.presentedTime`.
GPU completion alone never increments `presented`. Observation is bounded to
100 ms per frame, and per-frame drawable IDs/timestamps are logged. On the tested
Apple M1 macOS host, all 12 drawables per run were acquired, rendered, mirrored,
scheduled and completed without GPU errors, but all `presentedTime` values stayed
zero. Thus hidden drawable rendering/submission works; actual display/scanout is
**not established**. A PASS is for that bounded submission/readback lifecycle,
not visible presentation. The executable prints this limitation explicitly.
The runner repeats both camera modes three times and reports actual observed
presentation counts instead of requiring or fabricating them. Offscreen clear,
mesh and camera regression oracles/signatures are unchanged.

## Bounded opaque diffuse-material pass (textures NOT implemented)

`--materials snapshot.ppm` is a separate opt-in engine-camera mode preserving the
scenario camera. `--materials-test snapshot.ppm` uses two explicit API-controlled
front/top views of the existing DeltaGlider template. The test-only
`--materials-reference-test` uses exactly those views with the old grayscale
shader. Original `--mesh` and all camera flags retain their original behavior.
Append `--hidden-present` for the same hidden drawable path; no window is shown.

The pass obtains each actual group material from `oapiMeshMaterialCount` and
`oapiMeshMaterial` (zero-based indices), copies its normalized diffuse RGBA into
an owned GPU buffer, and multiplies RGB by the existing diagnostic normal shade.
`SPEC_DEFAULT` (-1) is opaque white, and `SPEC_INHERIT` (-2) resolves the previous
group including hidden/empty groups. Material zero is NOT a no-material sentinel.
Invalid indices or nonfinite/out-of-range diffuse values fail explicitly.
RGBA is recorded, but this is deliberately an **opaque RGB-only pass**: alpha
blending, specular/ambient/emissive lighting, texture sampling, animations and
instance edits remain unsupported. It is not a faithful glass or lit-vessel pass.
All material GPU bytes are client-owned copies; no material pointer survives
upload. After completed GPU work they are released on visual detach, before
engine world/template teardown. Normal ImGui init/newframe/render/shutdown remain
active; the image oracle uses only pre-UI mesh GPU pixels.

```sh
python3 Src/OrbiterCore/metal/material_regression.py \
  out/build/macos-core-spike/Src/OrbiterCore/metal/orbiter-core-metal \
  out/stage/content /Users/redezem/.hermes/cache/scratch/orbiter-material-check
# Repeat with --hidden-present appended.
c++ -std=c++17 Src/OrbiterCore/metal/test_material_semantics.cpp \
  -o /Users/redezem/.hermes/cache/scratch/orbiter-material-semantics-test
/Users/redezem/.hermes/cache/scratch/orbiter-material-semantics-test
```

Each runner executes three paired 12-frame real-engine lifecycles and validates
GPU completion, engine projection/bound matrices, exact geometry silhouettes,
all-pixel diffuse/shade membership, deterministic snapshots, two changed views,
and exact fixture material coverage. Real DG exposed material 24 is neutral
(.235,.235,.235), not chromatic: it changes 445 and 343 pixels respectively from
the grayscale reference, with every changed channel matching the engine diffuse
multiplier within one UNORM LSB. The colored interior materials are occluded;
no chromatic render support evidence is claimed. The original underside camera
views expose only white materials, so comparing those would falsely pass a
material-ignoring shader. The dedicated oracle rejects an actual grayscale
reference image, requires material-specific dark pixels, and asserts unchanged
white-material pixels and silhouette. Offscreen and hidden-drawable snapshots
match byte-for-byte; hidden submission still does not establish visible scanout.

**Texture dependency blocker / honest fallback:** the actual SDK entry point is
`oapiGetTextureHandle(mesh,index)` (1-based), not `oapiMeshTexture`. The DG external
template declares ten textures; all ten actual engine handles are null. The
material-mode callback records 124 real load requests (including UI/base/cockpit
assets), returns null for every request and reports zero successes. There is no
owned Metal SURFHANDLE repository, global-resource cache/release contract, or
vessel UV/texture draw path yet. No fake texture or white stand-in is returned.
The existing portable terrain decoder (`Src/Bootstrap/TerrainArchive.cpp`) is
private to its translation unit and **not safe to reuse as a verified DDS decoder**:
its BC1 four-color branch interpolates 3:1 / 1:3 instead of BC1's 2:1 / 1:2, and
forces alpha to 255 even for the BC1 transparent selector. It supports no BC2/BC3.
DG's declared external diffuse assets are staged BC1 DDS files; additional real
cockpit requests include BC3. A correct bounded decoder or verified system path,
owned registry, texture-root/case resolution and UV GPU oracle are prerequisites
for a follow-up texture slice. This is an implementation/validation blocker,
not evidence that Metal or these assets cannot support textures. Nothing here
claims texture rendering or addresses terrain/surfaces/Sketchpad/HUD.

## Review dispositions

Independent review requested staging safety and UI-isolated camera image checks;
the fixes and frozen-GPU sensitivity gate above address those findings.
Two Low findings are deferred, not claimed fixed:

- Run regression.py with ordinary non-optimized Python. Its assert checks are
  removed by python -O/PYTHONOPTIMIZE; optimized execution is not an accepted
  verification command. Explicit conditional failures should replace assertions
  before this becomes a general-purpose test runner.
- Early Launch failure can already close the session inside the engine; probe
  exception cleanup may close it again. Normal launch/close is verified, but
  injected startup failures and general exception-safe recovery are not.

Engine fixes accompanying this milestone initialize the native input-framework
owner before graphics Launch and honor null optional particle-stream callbacks
without storing/configuring unsupported streams.
