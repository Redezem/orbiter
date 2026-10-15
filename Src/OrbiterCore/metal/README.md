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

## Review dispositions

Independent review found no material issues for this narrow milestone.
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
