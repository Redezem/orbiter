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
scenario and frame count; it cannot be combined with `--hidden-present`.
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
