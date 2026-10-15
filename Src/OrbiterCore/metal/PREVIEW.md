# Orbiter Core Preview (native macOS, deliberately bounded)

This is a separate real-core application, **not** the bootstrap `Orbiter.app` and
not a complete Orbiter simulator port. The bundled Mach-O is the actual
`orbiter-core-metal` executable, renamed `orbiter-core-preview` to select its
explicit application startup policy. Existing positional hidden probe modes are
unchanged. Nothing in packaging opens, activates or registers a visible app.

## Build/package

From the repository root, with the existing staged content and native modules:

```sh
cmake --build out/build/macos-core-spike --target orbiter-core-metal -j6
python3 Src/OrbiterCore/metal/package_preview.py \
  out/build/macos-core-spike/Src/OrbiterCore/metal/orbiter-core-metal \
  out/stage/content \
  out/build/macos-core-spike/preview-v2/OrbiterCorePreview.app \
  /Users/redezem/Codeforge/third_party/SDL-install/lib/libSDL3.0.dylib
```

Packaging requires a **fresh** destination named `OrbiterCorePreview.app`; it
refuses recursive overwrite. Archive a prior preview before rebuilding. It copies
SDL into `Contents/Frameworks`, removes the executable's build-machine search
roots, adds `@executable_path/../Frameworks` plus the private session `.` for
engine module dependencies, and ad-hoc signs SDL, executable and bundle. The plist
minimum OS is read from the built executable, not a portability promise. This
local build currently targets macOS 27 on arm64. No Swift runtime is involved.

## User launch (only when ready for a visible program)

Double-click `out/build/macos-core-spike/preview-v2/OrbiterCorePreview.app` in Finder, or:

```sh
open /Users/redezem/Codeforge/orbiter/out/build/macos-core-spike/preview-v2/OrbiterCorePreview.app
```

**Do not run that command as part of hidden verification.** Visible-window QA and
physical keyboard review remain user-controlled. The app starts the real landed
DG in its private Sun/Earth fixture, uses the actual tracked external engine
camera, renders textured static DG triangles and the existing curved KSC terrain
patch into one Metal encoder/depth attachment, composites the real engine ImGui,
and submits each drawable. It calls actual `Orbiter::Pause(true)` after launch.
Use **Ctrl+P** to resume/pause; keypad flight controls (including Ctrl+keypad +/-
for main thrust) go through SDL physical scancodes, the DirectInput shim and the
real engine. On keyboards without a keypad these controls are limited; no new
remapping is provided. Only keypad/modifiers/P are published to the engine in
preview mode. With the supported staged **default keymap**, save/dialog/gear
shortcuts are excluded and Ctrl+P has the behavior above. This physical-key
filter is not a semantic restriction for arbitrary user rebindings: customized
maps can assign other commands to allowed keys and are outside this contract. Close the
window or Quit the application to exit through its own SDL event loop and normal
engine shutdown. The title reflects actual engine pause state.

The window is fixed at 640×480, nonresizable. The engine owns the only ImGui
context and its lifetime; this preview deliberately retains the event-only SDL
platform policy (no global mouse polling/capture/cursor changes, clipboard or OS
IME). **Text entry/IME is unsupported**, not a complete visible ImGui platform
integration. No HUD/MFD/Sketchpad is rendered. No gear animation is integrated:
this is the static template, not the vessel's animated visual instance. Other
vessels, focus changes, full celestial/scene rendering, terrain streaming, broad
camera navigation and contact/flight accuracy are not acceptance claims.

The KSC patch is fixed geographically; the vessel and its camera use fresh real
engine transforms each frame. Moving/flying beyond that patch does **not** load
new terrain. The bounded 0.5–5000 m shared frustum and diagnostic nearest-sampled,
opaque, unlit/normal-shaded material path remain; visible aliasing and rough
surface appearance are expected. GPU readback/validation is synchronous each
frame and logs are verbose: this is a review preview, not a performance build or
an endurance-tested session. Geometry validation can stop an out-of-scope view.

## Private session and content policy

`Contents/Resources/content-root.txt` explicitly names the existing staged
`out/stage/content` directory. Large textures, meshes, modules and data are shared
by symlink, **not copied into the bundle**. This is not a relocatable standalone
release; moving/removing the content root produces a clear missing-asset error.
Rebuild the bundle for a different root. Staged module dependencies remain part
of that content contract.

Each launch creates a fresh, exclusively owned directory under:

`~/Library/Application Support/OrbiterCorePreview/Sessions/session-XXXXXX/`

Root `.cfg` files are private copies; `Config`, nested `Config/Earth`, and
`Scenarios` are real private directories, with exclusive fixture files reserved
before introducing shared links. `Config/Earth/Atmosphere.cfg` selects
`EarthAtmJ71G` only in that session. The installed `VenusAtm2006` selection is
**not repaired or changed globally**. Other config/data entries are linked for
reading; this is a filesystem ownership policy, not an OS sandbox.

Before creating the session parent or session, startup validates directory types,
read/search access and enumeration for its required directories (including
`Config/Earth`), regular-file/read access for required assets and bundle fixtures,
and every root `.cfg` it will copy. Incomplete, wrong-type or unreadable inputs
fail without a session. This preflight assumes trusted, stable content; it does
not promise race-free behavior if another process changes files during startup,
nor pre-validate every dependency the engine or plugins may subsequently read.

Logs, engine outputs, GPU PPM and exported geometry are written to the session.
Sessions are retained for evidence, never recursively purged by startup. Main
logs: `preview-session.log`, `preview-errors.log`, `Orbiter_core_metal.log`.
`preview.ppm` is an actual pre-ImGui GPU readback, not an invented image.

## Hidden acceptance (safe to run without visible-launch authorization)

```sh
python3 Src/OrbiterCore/metal/preview_regression.py \
  out/build/macos-core-spike/preview-v2/OrbiterCorePreview.app \
  out/stage/content \
  /Users/redezem/.hermes/cache/scratch/orbiter-preview-acceptance
```

The bundled executable's **only** alternative argument is `--self-test`. It uses
the same fixture startup, core creation/launch, SDL platform/Metal renderer,
continuous session loop and normal teardown, but creates a hidden background
window and keeps the per-frame hidden/no-input-focus checks. It posts only a
process-local SDL quit after 24 real frames; no key or hardware input is injected
by this app self-test. It observes actual engine pause on every frame, GPU
completion and shared-depth scene draws, then proves event-driven loop exit,
released device keys, visual detach, world close and renderer/ImGui shutdown.

The acceptance runner binds the resolved supplied content directory to the bundle
manifest before any launch, and persists that root's path/device/inode plus the
before/after regular-file SHA-256 byte inventories. It rejects symlink entries
rather than silently omitting linked directory contents. These inventories do
not establish permissions, ownership, hardlink or empty-directory preservation.
It repeats three independent sessions, verifies private config ownership and
shared textures, preserves actual GPU snapshots, and tests missing content
using a freshly copied/re-signed negative-control bundle. Presentation timestamp
counts must be consistent across records and within the completed-frame count;
positive observations are allowed and reported independently. Hidden presentation
scheduling/GPU completion is **not visible scanout**; on this host positive
`presentedTime` remained zero. The executable reports this distinction. Visible
presentation, physical controls, visual legibility, and longer unpaused flight
remain for explicit user review.

CPU-only production-header preflight and runner-negative tests (scratch homes,
no engine/window launch):

```sh
python3 -m unittest discover -s Src/OrbiterCore/metal -p test_preview_regression.py
```
