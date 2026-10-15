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
input, or long-running resource behavior. The separate opt-in session bridge below
adds bounded held-key input verification, not a usable full app. The bootstrap
app remains separate.

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

## Bounded opaque diffuse-material pass (untextured, preserved)

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

Material-only and all older modes deliberately continue returning null from
texture load callbacks. Their original byte-for-byte images and texture callback
accounting are preserved. Texture loading is enabled only by the separate modes
below; no fake texture or white stand-in is returned on failure.

## Bounded real static-vessel diffuse texture pass

`--textures snapshot.ppm` preserves the actual scenario camera;
`--textures-test snapshot.ppm` uses two explicit engine API-controlled upper views
at a closer distance for inspecting the real DG livery. The diagnostic-only
`--textures-ignore-sampling-test` creates the **same actual engine texture handles,
uploads, UVs and group draws**, but disables sampling in the fragment shader.
Append `--hidden-present` for hidden drawable rendering; no focus/input is used.

`texture_store.h` owns stable opaque handle identities, a global bit-3 repository,
independent non-global owners, and mesh pins. Foreign/stale handles are rejected
before Metal access. Global loads reuse the same canonical-file handle until
graphics destruction; releasing a global handle is harmless. Non-global loads
have distinct handles; a release drops their owner, but pinned mesh draws retain
them until detach. Tombstones prevent stale identity reuse (4096 identities max).
Normal engine shutdown flushes templates and destroys the world before
`clbkDestroyRenderWindow`, which destroys the store; mesh pins and UV/color copies
are released before world teardown, after synchronous GPU completion.

Lookup uses the **actual Config HightexDir, then TextureDir**, normalizes Windows
separators, and matches ASCII case-insensitively per component. Ambiguous sibling
case matches fail rather than selecting arbitrarily. Absolute/drive/traversal
paths and non-DDS names fail. Configured root symlinks are canonicalized (staged
content uses them); child links must resolve inside that same canonical root.
Final files are opened read-only with O_NOFOLLOW and checked as regular files.
Reads are bounded to 128 MiB, decoding to 64 MiB/image, and retained residency to
256 MiB using the larger of decoded byte count and Metal allocatedSize. Decoded
CPU/file scratch bytes are transient, not additional retained cache copies.
Missing/malformed/unsupported DDS, unknown flag bits, cap exhaustion or failed
uploads return null. A referenced unavailable texture fails the bounded vessel
pass explicitly instead of drawing it as successfully loaded white.

The reviewed dependency-free DDS codec supplies BC1/BC2/BC3 mip-0 RGBA pixels;
the separate terrain decoder is not reused. `metal_textures.h` uploads the real
decoded image into shared `RGBA8Unorm` ShaderRead storage. Low flag bits 0/1/2 are
fulfilled by system-visible shared/decompressed/mip0 storage. Higher bits fail.
This is **mip0-only, nearest-repeat, unlit diffuse RGB × normal shade × sampled
RGB, output alpha 1 with blending disabled**. Alpha is decoded but not blended;
no sRGB conversion, full mip chain, lighting, HDR, animation or surface edits.

Mesh templates and UVs come only from actual VESSEL/mesh APIs, not a second .msh
parser. `MESHGROUP::TexIdx` is **zero-based** (0 means the first texture), while
`oapiGetTextureHandle` is **one-based**. DEFAULT (-1) means no texture and INHERIT
(-2) keeps the previous resolved slot, including hidden/empty groups. This follows
`Mesh.cpp`'s file-loader decrement and D3D9Mesh::ProcessInherit's increment; do not
confuse a group raw index with an API slot. Per-group index ranges bind the actual
validated engine handle, or genuinely untextured branch, before each draw.

```sh
cmake -S Src/OrbiterCore/metal/tests \
  -B /Users/redezem/.hermes/cache/scratch/orbiter-texture-cpu
cmake --build /Users/redezem/.hermes/cache/scratch/orbiter-texture-cpu -j6
TMPDIR=/Users/redezem/.hermes/cache/scratch ctest \
  --test-dir /Users/redezem/.hermes/cache/scratch/orbiter-texture-cpu --output-on-failure
python3 Src/OrbiterCore/metal/texture_regression.py \
  out/build/macos-core-spike/Src/OrbiterCore/metal/orbiter-core-metal \
  out/stage/content /Users/redezem/.hermes/cache/scratch/orbiter-texture-check
# Optional --hidden-present. Python runner requires installed Pillow.
```

The GPU suite repeats three positive/ignored-sampling pairs, two views each,
requires all ten real DG template handles, validates zero-based group-to-slot
binding independently, real camera matrices, completed GPU work and teardown.
Actual geometry/UV/material/range provenance is exported beside each PPM.
An independent scalar sparse CPU rasterizer uses perspective-correct UV/shade,
Pillow DDS decoding of the exact selected source files, and compares actual GPU
RGB samples within 2 UNORM LSB. It excludes triangle/texel boundaries and
near-coplanar layers within four Depth32Float ULP, based on geometry alone.
Verified views have 254/333 independent samples (191/240 chromatic), zero
mismatches, and deterministic images. Ignoring real sampling changes 62,588
first-view pixels without changing the silhouette, and the independent texture
oracle process exits nonzero. PPMs are pre-UI GPU readbacks, never synthesized
CPU render evidence. Hidden/offscreen images match; actual scanout remains
unproven. Scope remains the first static DG diffuse pass, not terrain, HUD,
Sketchpad, general surfaces, animation or fully lit/transparent vessels.

## Bounded native buffered keyboard commands

The same `--session-input` pump now supplies genuine physical transitions to the
shim's `GetDeviceData`, separately from `GetDeviceState`. The actual default
pause binding is **Ctrl+P**, not bare P or the Pause key (`Keymap.cpp`). The engine
route is `SingleFrame -> UserInput -> GetDeviceData -> KbdInputBuffered_System ->
IsLogicalKey(OAPI_LKEY_Pause) -> TogglePause`. `Pause` only requests a state change;
**the next BeginTimeStep applies it**. Neither the engine input handlers nor the
pause implementation is patched or bypassed.

The single-main-thread keyboard backend implements a fixed allocation ring
(maximum 256 events; the engine requests 10 with `DIPROP_BUFFERSIZE`). Each real
change stores DIK `dwOfs`, `dwData` 0x80/0, monotonic milliseconds modulo DWORD,
monotonic sequence modulo DWORD, and zero `uUser`. Repeated held publications and
all already-clear entries in the adapter's 256-key clear loop enqueue nothing.
Focus/hide/quit/teardown publish only genuine releases. Held-state polling is
independent of event consumption. No global SDL hardware state is consulted.

Supported reads require the exact native `sizeof(DIDEVICEOBJECTDATA)` and flags
zero or `DIGDD_PEEK`. `*items` is the maximum requested; it becomes the number
returned. A null output counts/discards up to that maximum, while null+PEEK counts
without consuming. Zero requests return zero. A full ring preserves oldest unread
events, drops incoming events and latches `DI_BUFFEROVERFLOW`; PEEK leaves the
latch set and a non-PEEK read acknowledges it. Unacquire flushes queued commands;
transitions while unacquired affect held state but are not replayed on Acquire.
Acquire is idempotent. Release flushes and disables buffering and clears held
state. Configuration requires an unacquired keyboard and a valid device-scoped
DIPROPDWORD; capacity over 256 is rejected, not silently clamped. Invalid stride,
flags, null count, unbuffered reads and unsupported properties return errors.
Only one keyboard is supported; the mouse token is separate and explicitly has
no buffered keyboard queue. This is not general DirectInput emulation.

```sh
cmake --build out/build/macos-core-spike \
  --target orbiter-core-metal orbiter-buffered-input-tests -j6
out/build/macos-core-spike/Src/OrbiterCore/metal/orbiter-buffered-input-tests
python3 Src/OrbiterCore/metal/buffered_regression.py \
  out/build/macos-core-spike/Src/OrbiterCore/metal/orbiter-core-metal \
  out/stage/content /Users/redezem/.hermes/cache/scratch/orbiter-buffered-check
# Optional: append --hidden-present (still hidden, no scanout guarantee).
```

The test-only `--buffered-input-test` runs 19 actual frames inside a requested
23-frame bound and quits via process-local SDL events. Three runs independently
verify actual `oapiGetPause`, simulation-time advance/freeze, and engine-polled
P/left-Ctrl/right-Ctrl, including bare P, repeated down, held frames with no events,
keyup/repress, focus-clear and right-Ctrl. Normal core/ImGui/Metal teardown and
exact GPU completion/readback counts are required. The negative
`--buffered-input-ignore-test` publishes identical held keys but deliberately
suppresses buffered events: it must stay running and fail the pause oracle with a
nonzero exit **after normal teardown**. No `oapiSetPause`, direct engine SendKbd,
visible window, focus activation or OS injection is used. SDL-to-ImGui platform
input is still missing: this is not a full keyboard/UI or interactive-app claim.

## Bounded native SDL held-key session bridge

Append `--session-input` to any existing mode to opt into a main-thread SDL event
pump before each real `Orbiter::SingleFrame`. The existing frame bound (1..10000)
remains mandatory. All windows remain **hidden and without input focus**; this is
not a visible launch option, and there is currently no supported visible-session
command. No window show/raise/activation, OS input injection or global keyboard
polling is implemented or used. `--hidden-present` still only submits hidden
Metal drawables and does not establish scanout.

`session_input.{h,cpp}` explicitly maps SDL physical USB scancodes to the SDK's
DirectInput/OAPI_KEY PC-scancode values, preserving keypad/main-key distinctions
and independent left/right Ctrl, Shift and Alt. Held state survives frames with
no repeat event; duplicate down/repeat is idempotent and key-up releases it.
Foreign-window keys are ignored. Focus-loss, hide, minimize, close, SDL quit,
new-session construction and teardown clear the entire published device state.
Close/quit requests finish the current frame and take normal teardown. The host
checks hidden/no-input-focus flags every rendered frame. The shim callback and
adapter are single-main-thread owned; no shared worker input state is introduced.

The actual engine route was traced, not assumed: native Create owns DInput;
`CreateRenderWindow` creates/acquires its keyboard device and activates the
logical graphics session; `SingleFrame` reaches `UserInput`, which calls the
shim device's `GetDeviceState`. It copies `shim_kbd_state` into the engine's
`simkstate`, respects dialogs/module consumption and ImGui keyboard capture, then
`KbdInputImmediate_OnRunning` resolves the actual keymap. Ctrl+keypad-plus/minus
calls `Vessel::IncMainRetroLevel` with the real simulation timestep. No core
Input.cpp, world pose, physics or throttle setter is used by the probe. The native
DirectInput shim now additionally implements the bounded buffered path below.
A logically active graphics session is distinct from OS keyboard/window focus.

The explicit **test-only** `--session-input-test` requires a bound of at least
16 frames and requests quit after the sixteenth actual frame. The runner passes
a 20-frame bound, independently proving quit ends the host loop early.
`session_control_probe.h` pushes events only into that process's SDL queue:
four untouched UI warmup frames, baseline, left-Ctrl/keypad-plus down, another
held frame with no repeat, release, right-Ctrl plus a duplicate repeat, focus
loss, foreign-window keys, another press, hide, Ctrl/keypad-minus, release and
quit. Real engine ImGui briefly wants keyboard capture during startup; warmup
allows it to settle without bypassing capture or changing core input semantics.
Every frame logs real `VESSEL::GetThrusterGroupLevel(THGROUP_MAIN)` before/after
and the engine's actual polled Add/Subtract/left-Ctrl/right-Ctrl state. There are
no test throttle setters, replacement physics or direct `SendKbdImmediate` calls.

```sh
cmake --build out/build/macos-core-spike \
  --target orbiter-core-metal orbiter-session-input-tests -j6
out/build/macos-core-spike/Src/OrbiterCore/metal/orbiter-session-input-tests
python3 Src/OrbiterCore/metal/session_regression.py \
  out/build/macos-core-spike/Src/OrbiterCore/metal/orbiter-core-metal \
  out/stage/content /Users/redezem/.hermes/cache/scratch/orbiter-session-check
# Optional: append --hidden-present to repeat with real hidden drawables.
```

The camera, material, texture and session runners share `fixture_staging.py`.
It reserves a fresh owned launch tree, real Config/Scenarios directories and
exclusive fixture files before adding source links. Destination `lexists` checks
skip filesystem aliases (including case/normalization aliases and dangling
links), rather than trusting exact filename spelling. Stale output trees are
never reused or deleted. CPU-only sentinel tests exercise each runner without
launching graphics:

```sh
python3 -m unittest discover -s Src/OrbiterCore/metal -p 'test_*.py' -v
```

The session runner repeats three real-core
textured 16-frame sessions, independently checks exact polled states and throttle
increase/hold/release/decrease phases, GPU completion/readback and full normal
engine/ImGui/resource teardown. `--session-input-ignore-test` is a diagnostic
negative control: identical real SDL events, but publication to the shim is
disabled. Its control oracle must fail nonzero after normal teardown. CPU adapter
tests also poll the actual DI shim device and exercise modifiers, repeats,
window isolation and all clear/quit paths without initialising SDL video.

This implements held-key immediate controls and the bounded buffered keyboard
path below, not SDL-to-ImGui platform input, mouse/cockpit interaction,
menu/key-command completeness, joystick/gamepad,
full HUD/MFD/Sketchpad, terrain, animation, alpha blending or general interactive
app readiness. Existing clear/mesh/camera/material/texture modes do not pump
session input unless explicitly opted in; their regressions remain separate.

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
