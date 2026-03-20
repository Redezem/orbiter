# Orbiter macOS Porting Assessment

## Executive Summary

Porting Orbiter to native macOS is feasible, but it is a substantial engineering project rather than a build-system tweak.

## Current Implementation Status

Groundwork completed in-repo so far:

- Added `Orbitersdk/include/OrbiterPlatform.h` so public SDK headers no longer directly include `windows.h`.
- Updated the common SDK headers to compile on non-Windows hosts with Orbiter-owned type aliases.
- Added a portable runtime layer for shared-library loading and user-data path selection under `Src/Platform`.
- Added a portable bootstrap library under `Src/Bootstrap`.
- Added `Orbiter_server` as a renderer-free bootstrap executable that validates scenario selection and runtime path split.
- Added `Orbiter_macOS` scaffolding as an SDL3/Metal app bundle for first-pixels bring-up on Apple Silicon.
- Added macOS CMake presets, `mise` tasks, and build gating so Apple hosts do not try to build the legacy Win32 stack by default.
- Switched Apple builds to the system `ZLIB` package and made the macOS `mise` tasks use `cmake --fresh` so stale `_deps/zlib-*` state does not survive preset changes.
- Excluded the legacy `meshc` utility from the macOS/bootstrap build because it still depends on Direct3D 7 headers and is only needed by the legacy vessel/tool chain.
- Added runtime content-root discovery so `Orbiter_server` and the macOS app resolve `Scenarios/` from the build root instead of `Orbiter.app/Contents/MacOS`.
- Made the macOS app behave like an interactive app by default: it now runs until the user closes the window unless `--maxframes` is explicitly provided. `Orbiter_server` still exits after bootstrap validation.
- Added a lightweight Orbiter `.msh` reader and a minimal Metal draw path that renders the stock DeltaGlider Mk4 mesh with copied vessel textures from `Meshes/DG` and `Textures/DG`.
- Split the Metal draw path into opaque and transparent passes and enabled back-face culling to reduce flicker on thin transparent polygons such as canopy/HUD surfaces.

What is still intentionally incomplete:

- The full Orbiter simulation core is not yet wired into the portable bootstrap targets.
- The macOS app currently renders only a standalone stock DeltaGlider Mk4 model with a fixed/bootstrap camera; scenario-driven scene, HUD, MFD, planets, and other vessel rendering remain future work.
- Legacy Win32 UI, modules, and renderer code still exist and remain the primary full-feature path.

### Where We Are Now

- `mise run build-macos` is able to configure and build the current Apple Silicon bootstrap path.
- `Orbiter_server` now proves the portable launch/bootstrap path, resolves the content root correctly, validates scenario lookup, and exits.
- `Orbiter_macOS` now opens a native SDL3/Metal window, stays open by default, resolves runtime assets from the build root, and renders the stock DeltaGlider Mk4 mesh with its copied vessel textures.
- The bootstrap renderer now has a depth buffer, a basic lit textured mesh path, and separate opaque/transparent handling to reduce thin-polygon flicker.
- The current macOS result is therefore no longer just "first pixels"; it is "first textured vessel rendered under a native Metal client".

### Recommended Next Steps

1. Wire the portable bootstrap path into real Orbiter simulation startup instead of rendering a standalone test vessel.
2. Replace the fixed bootstrap camera with simulation-backed camera state and basic user camera controls.
3. Load vessel selection and placement from a real scenario so the rendered DeltaGlider comes from scenario data rather than a hardwired asset path.
4. Add a minimal scene graph path for more than one object: vessel plus a simple planetary body or pad reference geometry.
5. Move mesh/texture loading out of the app file into a cleaner renderer asset layer so additional vessel and world assets can be added without inflating `OrbiterMacApp.mm`.
6. Add a small debug overlay for camera state, loaded asset counts, and render timing so bring-up problems are visible without attaching a debugger.
7. Start replacing remaining DirectInput-era assumptions with a portable input abstraction shared by the macOS host and any future non-Windows host.
8. Decide whether the next visual milestone is "scenario vessel with movable camera" or "scenario vessel plus Earth/terrain context"; both are reasonable, but that choice should drive the next renderer/API cut.

The good news:

- A large part of the simulation, scenario, vessel, celestial-body, and data-processing code is ordinary C++ and should survive a platform move with moderate cleanup.
- The project already uses CMake and can be configured on a non-Windows host if the Windows-only renderer and audio stack are disabled.
- Some utilities already point toward portable directions, such as the Qt-based `Utils/tileedit/qt`.

The hard parts:

- The public SDK and much of the core executable directly expose Win32 types and include `windows.h`.
- The runtime assumes Windows DLL loading, Win32 resources, Win32 dialogs, and Windows-style paths.
- Input is built on DirectInput.
- The rendering stack is heavily tied to DirectDraw/Direct3D 7 in the core and Direct3D 9 in the high-end client.
- XRSound is packaged around Windows irrKlang binaries.

My overall conclusion is:

- A native macOS port is realistic if you treat it as a platform separation project first and a renderer port second.
- The correct near-term objective is not "make D3D9Client work on macOS".
- The correct near-term objective is "make Orbiter core and SDK compile without Win32 headers, then bring up a minimal native macOS host, then add a native renderer".

## What I Evaluated

I reviewed the core build and runtime structure with emphasis on:

- Top-level build and packaging:
  - `CMakeLists.txt`
  - `README.md`
  - `COMPILE.md`
  - `CMakePresets.json`
- Core executable and public SDK:
  - `Src/Orbiter/CMakeLists.txt`
  - `Orbitersdk/include/OrbiterAPI.h`
  - `Orbitersdk/include/GraphicsAPI.h`
  - `Orbitersdk/include/ModuleAPI.h`
  - `Orbitersdk/include/DlgCtrl.h`
  - `Src/Orbiter/GraphicsAPI.cpp`
  - `Src/Orbiter/Input.cpp`
  - `Src/Orbiter/TabVideo.cpp`
  - `Src/Orbiter/Orbiter.cpp`
  - `Src/Orbiter/Script.cpp`
  - `Src/Orbiter/Config.cpp`
  - `Src/Orbiter/Texture.h`
  - `Src/Orbiter/Di7frame.h`
- Graphics clients:
  - `OVP/GDIClient/*`
  - `OVP/D3D9Client/CMakeLists.txt`
- Audio:
  - `Sound/XRSound/CMakeLists.txt`
  - `Sound/XRSound/src/CMakeLists.txt`
- Tests and utilities:
  - `Tests/CMakeLists.txt`
  - `Utils/CMakeLists.txt`
  - `Utils/tileedit/qt/CMakeLists.txt`

I also ran an empirical non-Windows configure/build check:

```bash
cmake -S . -B /tmp/orbiter-macos-eval2 \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DORBITER_BUILD_D3D9CLIENT=OFF \
  -DORBITER_BUILD_XRSOUND=OFF \
  -DORBITER_MAKE_DOC=OFF \
  -DORBITER_MAKE_TESTS=OFF

cmake --build /tmp/orbiter-macos-eval2 --target Orbiter -j4
```

Observed result:

- Configure succeeded after forcing an older policy floor for a vendored `zlib` CMake file.
- The first native build failures were:
  - missing `windows.h` from `Orbitersdk/include/DlgCtrl.h`
  - MSVC-only `/MP` being passed into GCC/Clang

This is important because it shows the project is not blocked only at the renderer layer. The public headers and general build flags already prevent a native Unix-like compile.

## Current Portability Picture

### What is probably portable with moderate work

- Physics and simulation logic
- Scenario parsing and object lifecycle logic
- Many vessel and celestial-body implementations
- Lua integration at the language level
- Asset formats and most data files
- Large parts of the math and domain-model code

### What is not portable today

- Public SDK surface
- Main windowing and launchpad UI
- Dialog/control system
- Input subsystem
- Dynamic module loading ABI
- Resource discovery and metadata loading
- Core texture/surface interfaces built around old DirectX types
- D3D9 graphics client
- GDI graphics client
- XRSound packaging and likely parts of its implementation

## Quantitative Signals

Approximate counts from source scanning:

- Files including `windows.h`: 102
- Files using `LoadLibrary`/`GetProcAddress`/`FreeLibrary`: 25
- Files including DirectInput/DirectDraw/Direct3D headers: 35
- Resource script files (`.rc`): 27
- Files using COM/WIC-related APIs: 4
- Files using Windows thread handles or `_beginthreadex` patterns: 19

These counts are not the real scope by themselves, but they confirm the Windows coupling is broad, not isolated.

## Major Findings By Subsystem

### 1. Build System: Windows-first, but not hopeless

Evidence:

- `COMPILE.md` explicitly says Orbiter requires a compiler that creates Windows binaries.
- `README.md` instructs users to build with Visual Studio.
- `CMakePresets.json` has Windows presets and Linux `winegcc`/`wineg++` presets, which means Linux is currently used as a host for producing Windows binaries, not native Linux binaries.
- The top-level `CMakeLists.txt` applies some MSVC-style flags in ways that are not fully isolated from non-MSVC generators.

Impact:

- Native macOS configuration is possible only after general CMake cleanup.
- The existing Linux support path is not directly reusable for native macOS; it is cross-compilation to Win32.

Assessment:

- This is a medium blocker.
- It should be addressed early, but it is not the hardest part of the port.

Recommendation:

- Add first-class `APPLE` support in CMake.
- Move all MSVC-only compile/link flags behind strict `if(MSVC)` guards.
- Add platform options so `D3D9Client`, `XRSound`, MFC tools, and HTML Help paths are disabled automatically on macOS.

### 2. Public SDK: the highest-leverage blocker

Evidence:

- `Orbitersdk/include/OrbiterAPI.h` includes `<windows.h>` and defines `DLLEXPORT`, `DLLIMPORT`, and `DLLCLBK` with `__declspec`.
- `Orbitersdk/include/GraphicsAPI.h` includes `<windows.h>` and uses `HWND`, `HDC`, and other Win32 concepts.
- `Orbitersdk/include/ModuleAPI.h` and `Orbitersdk/include/DlgCtrl.h` expose `HINSTANCE`, `HWND`, `WPARAM`, `LPARAM`, and related types.

Impact:

- Any attempt to compile Orbiter or third-party add-ons natively on macOS fails before getting into interesting logic.
- The current plugin ABI is source-level Windows-specific and binary-level Windows-only.

Assessment:

- This is a critical blocker.
- Until the SDK stops requiring Win32 headers, every other porting effort will be slower and noisier than necessary.

Recommendation:

- Introduce a dedicated portability header, for example `Orbitersdk/include/OrbiterPlatform.h`.
- Replace direct Win32 exposure with:
  - fixed-width integer types from `<cstdint>`
  - portable export macros using GCC/Clang visibility attributes on non-Windows
  - opaque handle types for platform windows/modules instead of raw `HWND`/`HINSTANCE`
- Preserve source compatibility where possible, but accept that Windows binary compatibility is not realistic on macOS.

### 3. Dynamic Module Loading: hard-coded to Windows DLL semantics

Evidence:

- `Src/Orbiter/Orbiter.cpp`, `Src/Orbiter/Script.cpp`, `Src/Orbiter/Celbody.cpp`, `Src/Orbiter/Vessel.cpp`, and `Src/Orbiter/Planet.cpp` use `LoadLibrary`, `LoadLibraryEx`, `GetProcAddress`, and `FreeLibrary`.
- `Src/Orbiter/TabVideo.cpp` scans `Modules\\Plugin` and expects `.dll`.
- Module metadata is read through Win32 string resources loaded from DLLs.

Impact:

- Native macOS modules would need `dlopen`/`dlsym` and `.dylib` or bundle handling.
- The current metadata discovery path through Win32 resources has no direct macOS equivalent.

Assessment:

- This is a critical blocker.
- It is also a clean abstraction boundary, which makes it a good early refactor target.

Recommendation:

- Add a platform-independent dynamic library wrapper:
  - `PlatformDynamicLibrary::load`
  - `PlatformDynamicLibrary::symbol`
  - `PlatformDynamicLibrary::unload`
- Replace module metadata-from-resource with one of:
  - exported metadata functions
  - sidecar manifest files
  - embedded JSON/plist-like text resource read through a cross-platform mechanism

### 4. Windowing, Launchpad, and UI: Win32 throughout

Evidence:

- `Src/Orbiter/CMakeLists.txt` builds the Win32 ImGui backend `imgui_impl_win32.cpp` directly into the core executable.
- `Src/Orbiter/GraphicsAPI.cpp`, `Src/Orbiter/LpadTab.h`, `Src/Orbiter/TabVideo.cpp`, `Src/DlgCtrl/*`, and many dialog classes use `HWND`, message procs, `CreateDialogParam`, `SendDlgItemMessage`, `LoadString`, and related APIs.
- There are many `.rc` files for dialogs, strings, and icons.
- HTML Help is used in the build and runtime help flow.

Impact:

- The current launcher and many configuration panels cannot be moved to macOS incrementally without a host UI rewrite or a heavy compatibility layer.

Assessment:

- This is a critical blocker.
- It is likely the second-largest chunk of work after rendering.

Recommendation:

- Do not attempt to emulate Win32 dialogs on macOS.
- Replace the launchpad/config UI with a native cross-platform UI stack.
- Since Orbiter already uses ImGui, the lowest-friction recommendation is:
  - SDL3 for windowing, events, and gamepad support
  - Dear ImGui for launchpad/config/debug UI
- Alternative:
  - Qt 6 for the launcher and tools
  - but avoid making Qt a mandatory runtime dependency for the simulator itself unless that is an explicit product decision

### 5. Input: DirectInput-only

Evidence:

- `Src/Orbiter/Input.cpp` and `Src/Orbiter/Di7frame.h` are built around DirectInput and DirectX-era device structures.

Impact:

- Keyboard, mouse, and joystick handling must be rewritten for macOS.

Assessment:

- This is a high blocker, but conceptually straightforward once windowing is abstracted.

Recommendation:

- Replace DirectInput with SDL3 input APIs.
- Normalize Orbiter's internal input representation so the rest of the sim does not care whether events came from Win32, SDL, or something else.

### 6. Rendering: the biggest technical risk

Evidence:

- The core app still uses DirectDraw/Direct3D 7-era surface types in places like `Src/Orbiter/Texture.h`.
- `Src/Orbiter/GraphicsAPI.cpp` uses COM/WIC for image handling and Win32 window creation.
- `OVP/GDIClient` is explicitly built around GDI.
- `OVP/D3D9Client/CMakeLists.txt` links directly to `d3d9.lib`, `d3dx9.lib`, `user32.lib`, `gdi32.lib`, `shell32.lib`, `ole32.lib`, and more.

Impact:

- The current rendering backends are not portable.
- Parts of the core render/surface abstractions are also not portable.

Assessment:

- This is the single largest blocker.
- It is also the area most likely to dominate schedule and regressions.

Recommendation:

- Do not port `D3D9Client` as-is.
- Instead:
  1. First separate renderer-independent scene/model logic from backend code.
  2. Then build a new native macOS renderer.

Recommended renderer target:

- Metal backend for long-term viability on macOS.

Possible implementation approaches:

- New `MetalClient` that implements Orbiter's graphics-client contract.
- Or a rendering abstraction library such as `bgfx`, if you want cross-platform renderer growth beyond macOS.

Temporary shortcut option:

- An OpenGL backend may get "first pixels" faster, but it is strategically weak because OpenGL is deprecated on macOS.

### 7. Audio: XRSound is Windows-packaged

Evidence:

- `Sound/XRSound/CMakeLists.txt` fetches Windows irrKlang packages and copies `irrKlang.dll`, `ikpMP3.dll`, and `ikpFlac.dll`.
- `Sound/XRSound/src` includes Win32 usage such as `windows.h`, `MessageBox`, `GetProcAddress`, and DLL-centric loading.

Impact:

- XRSound is not portable in its current form.

Assessment:

- This is a high blocker for feature parity, but not for the earliest boot milestones.

Recommendation:

- Disable XRSound for the first native macOS milestones.
- Replace it later with a native audio layer built on:
  - `miniaudio`
  - OpenAL Soft
  - or SDL audio

### 8. Paths, resources, and mutable install layout are Windows-oriented

Evidence:

- `Src/Orbiter/Config.cpp` normalizes directory paths with backslashes.
- Many module/resource paths are written as `Modules\\Plugin`, `Modules\\Startup`, and similar.
- `Src/Orbiter/Orbiter.cpp` writes configuration back out during normal operation.

Inference:

- The current runtime expects a writable install tree.
- That clashes with standard macOS app-bundle practice, where the app bundle should be treated as read-only once installed and signed.

Impact:

- A native macOS build should split immutable app resources from mutable user data.

Assessment:

- This is a high blocker for a polished macOS app, even if a developer build can ignore it temporarily.

Recommendation:

- Put immutable assets in the `.app` bundle resources.
- Put mutable config, saves, quicksaves, logs, and generated files in:
  - `~/Library/Application Support/Orbiter`
  - or an equivalent user data directory

### 9. Tooling and tests are not yet arranged for a clean native port

Evidence:

- `Utils/CMakeLists.txt` still assumes Windows packaging for several tools.
- `Date` and `Shipedit` rely on MFC.
- `Tests/CMakeLists.txt` references an `Orbiter_server` target guarded by `BUILD_ORBITER_SERVER`, but I did not find a corresponding active build target in the current core CMake.

Inference:

- There was either past or intended headless/server support that is no longer wired in.
- Reintroducing a true headless target would materially reduce porting risk by separating simulation correctness from renderer/UI bring-up.

Assessment:

- This is an opportunity, not just a blocker.

Recommendation:

- Create or restore a headless/native test target early.
- Use it to validate scenario loading, plugin loading, vessel creation, and stepping on macOS before the renderer is ready.

## Recommended Target Architecture

I recommend this target shape for a sustainable macOS port:

### Core principle

Split Orbiter into:

- platform-neutral simulation core
- platform-neutral SDK surface
- platform-specific host layer
- one or more graphics/audio/input backends

### Host stack recommendation

- Windowing/input/gamepads: SDL3
- UI for launcher/config/debug: Dear ImGui
- Native renderer: Metal
- Dynamic loading: `dlopen`/`dlsym`
- Image codecs: `stb_image`/`stb_image_write` or `libpng`/`jpeg-turbo`
- Audio: `miniaudio` or OpenAL Soft
- Threads/timers: standard C++ where possible

### Why this stack

- SDL3 removes large amounts of custom platform event code.
- ImGui is already in the repository, which lowers UI migration risk.
- Metal is the correct long-term graphics backend for macOS.
- Standard C++ threads/timing eliminate a lot of Win32 plumbing.

## Recommended Porting Strategy

Do this in phases. Do not try to "flip macOS on" across the whole repo in one pass.

### Phase 0: Lock the product scope

Decide these before touching code:

1. Is the first target Apple Silicon only, or universal (`arm64` + `x86_64`)?
2. Is Windows add-on binary compatibility required?
   - My recommendation: no.
   - Target source compatibility with recompilation where reasonable.
3. Is sound required in milestone 1?
   - My recommendation: no.
4. Is the launcher required in milestone 1?
   - My recommendation: a minimal in-app launcher is enough.

Exit criteria:

- A written compatibility policy exists for add-ons, saved data, renderer selection, and supported macOS versions.

### Phase 1: Sanitize CMake and platform flags

Tasks:

1. Add a dedicated macOS configure preset.
2. Fence every MSVC-only compile/link flag behind `if(MSVC)`.
3. Auto-disable Windows-only components on `APPLE`.
4. Stop hard-coding `.dll`, `.exe`, backslashes, and Windows-only install assumptions in shared build logic.
5. Make third-party dependency versions compatible with modern CMake without manual policy overrides.

Exit criteria:

- `cmake -S . -B build-macos` completes on macOS without workaround flags.
- The generated build graph excludes D3D9Client, MFC tools, HTML Help, and XRSound by default.

### Phase 2: De-Windowsize the public SDK

Tasks:

1. Create `OrbiterPlatform.h` to centralize:
   - export macros
   - portable integer aliases
   - opaque platform handle types
   - platform feature macros
2. Remove direct `windows.h` inclusion from public SDK headers.
3. Replace public `HINSTANCE`, `HWND`, `BOOL`, `DWORD`, etc. with portable forms where possible.
4. Keep Win32-specific APIs behind platform extension layers instead of in the common SDK.

Exit criteria:

- `Orbitersdk/include/*.h` can be included by a trivial macOS Clang project without Win32 headers.

### Phase 3: Add a dynamic library abstraction

Tasks:

1. Replace direct `LoadLibrary`/`GetProcAddress`/`FreeLibrary` usage with a small platform wrapper.
2. Support `.dylib` naming and search paths.
3. Replace module metadata loaded from Win32 string resources with portable metadata.
4. Update module scanning code to use `std::filesystem` path joins and native separators.

Exit criteria:

- Orbiter on macOS can discover and load a trivial sample plugin compiled as a native dylib.

### Phase 4: Extract a headless or minimal-core target

Tasks:

1. Reintroduce or create a true headless/server executable.
2. Compile simulation, scenario, vessel, and module systems without renderer/UI dependencies.
3. Port tests to run against that target.

Exit criteria:

- A macOS CI job can load a scenario, step the simulation, and exit cleanly without a renderer.

This phase is worth doing even if Windows remains the main platform, because it reduces renderer coupling and makes future ports far easier.

### Phase 5: Replace platform services

Tasks:

1. Replace WIC-based image handling with portable codecs.
2. Replace Win32 sleep/thread/event usage with standard C++ primitives or SDL where needed.
3. Replace Windows memory-stat and help integrations.
4. Replace resource-based strings/icons with portable assets or compiled tables.

Exit criteria:

- Core non-renderer code no longer depends on Win32 service APIs.

### Phase 6: Replace windowing, launcher, and input

Tasks:

1. Bring up an SDL3 application shell.
2. Replace the launchpad and config dialogs with ImGui-based screens.
3. Replace DirectInput with SDL keyboard/mouse/gamepad handling.
4. Preserve Orbiter's internal control semantics while changing the event source.

Exit criteria:

- A native macOS app opens a window, accepts input, loads a scenario, and can run without crashing.

### Phase 7: Bring up a minimal renderer

Tasks:

1. Define a backend-neutral surface/texture abstraction that no longer depends on DirectDraw 7 types.
2. Implement only the minimum rendering features required to:
   - draw the main scene
   - show the HUD
   - render MFD text/surfaces
3. Start with correctness, not parity.
4. Defer advanced visual features such as high-end atmospheric effects until the basics are stable.

Exit criteria:

- A stock scenario is visible and interactive on macOS.

### Phase 8: Expand renderer capabilities

Tasks:

1. Add offscreen surfaces, panel rendering, VC HUD/MFD support, texture loading, and shader equivalents.
2. Port reusable scene logic cautiously from existing render clients.
3. Add performance instrumentation and regression comparisons.

Exit criteria:

- Common stock vessels and panels render correctly enough for normal play.

### Phase 9: Rebuild optional subsystems

Tasks:

1. Decide whether `GDIClient` is replaced or removed on macOS.
2. Build a native audio module to replace or supersede XRSound.
3. Port or defer utilities one by one.
4. Recompile stock plugins and samples against the cleaned SDK.

Exit criteria:

- macOS build has a coherent "supported feature set" instead of a partially disabled collection of Windows leftovers.

### Phase 10: Packaging and CI

Tasks:

1. Create a real macOS `.app` bundle.
2. Separate read-only app resources from writable user data.
3. Add macOS CI for:
   - configure
   - build
   - headless scenario smoke tests
   - plugin-load smoke tests
4. Later, add code signing and notarization if public distribution is planned.

Exit criteria:

- Developers can build, run, and test the macOS port reproducibly.

## Recommended Order of First Pull Requests

If I were executing the port, I would structure the first PRs like this:

1. CMake cleanup: isolate MSVC-only flags and add `APPLE` option gates.
2. Add `OrbiterPlatform.h` and start removing `windows.h` from public headers.
3. Introduce platform-neutral dynamic library wrapper.
4. Restore or add a headless/server target plus basic CI smoke tests.
5. Convert path handling and module discovery away from `.dll` and backslashes.
6. Replace Win32 thread/timer helpers with portable implementations.
7. Add SDL3 app shell and event loop.
8. Replace DirectInput with SDL input.
9. Bring up minimal renderer backend.
10. Port launcher/config UI into ImGui.

This sequence reduces risk because each PR improves buildability and testability before the renderer rewrite absorbs attention.

## Risks and Non-Obvious Decisions

### Risk: Renderer work dominates the schedule

This is the main risk. The codebase does not just have a Windows renderer. It has Windows assumptions in the core render-facing abstractions too.

Mitigation:

- Get a headless target working first.
- Treat rendering as a separate milestone with separate acceptance criteria.

### Risk: Add-on compatibility expectations are unrealistic

Windows Orbiter add-ons compiled as DLLs will not be binary-compatible with macOS.

Mitigation:

- Define compatibility as "same SDK at source level where possible, recompile required".
- Publish a small add-on migration guide once the SDK cleanup is stable.

### Risk: UI rewrite stalls the port

The launcher and configuration dialogs are deeply Win32-specific.

Mitigation:

- Do not try to preserve the exact dialog implementation.
- Replace it with a smaller cross-platform UI first.

### Risk: App bundle vs writable data

Inference from the current config/scenario write flow: Orbiter expects to write into its own tree. That is a poor fit for macOS app distribution.

Mitigation:

- Design the bundle/data split early.

### Risk: Architecture confusion around 32-bit vs 64-bit

The repository has x64 presets, but `README.md` still contains older 32-bit guidance. macOS is 64-bit only.

Mitigation:

- Standardize the port on 64-bit only from day one.
- Audit any remaining pointer-size assumptions while cleaning the SDK.

## Practical Recommendations

### What I would not do

- I would not start by porting `D3D9Client`.
- I would not attempt to preserve Win32 dialogs on macOS.
- I would not make XRSound a milestone-1 dependency.
- I would not promise Windows binary add-on compatibility.

### What I would do first

- Make the SDK compile without Win32 headers.
- Add a portable module loader abstraction.
- Create a headless validation path.
- Bring up SDL3 + ImGui shell before renderer parity work.

## Suggested Milestone Definition

A reasonable "first usable macOS milestone" is:

- native macOS app launches
- no Win32 headers required by the public SDK
- native dylib plugin loading works
- one stock scenario loads
- camera moves and input works
- one renderer displays the scene, HUD, and at least basic MFD output
- no audio required
- no advanced D3D9 visual parity required

That is a meaningful deliverable and a solid base for feature parity later.

## Final Assessment

Orbiter is portable in principle, but not portable in its current host/runtime shape.

The codebase is best described as:

- simulation logic: portable enough
- host platform layer: strongly Windows-bound
- renderer stack: strongly DirectX-bound
- SDK ABI: currently Windows-shaped

If the project is willing to:

- introduce a real platform abstraction layer
- accept a native macOS renderer instead of trying to preserve D3D9
- accept source-level rather than binary add-on compatibility
- stage the work through a headless/core milestone

then a macOS port is practical and technically coherent.

If the project instead tries to preserve the current Win32 UI, current DLL/resource model, and current DirectX renderer architecture unchanged, the port will be slow, brittle, and much harder than it needs to be.
