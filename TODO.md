# Feature Parity TODO

## Purpose

This file is the repo-wide checklist for reaching native feature parity with the legacy Win32 Orbiter path.
It should be updated whenever scope changes, work lands, or a dependency/blocker changes status.

## Current Checkpoint

The repo is currently at a constrained native bootstrap milestone:

- `Orbiter_server` and `Orbiter_macOS` can load `Scenarios/Quadcopter/Quadcopter at KSC.scn`
- the bootstrap path has a shared `.msh` loader and a shared `SimulationSession`
- the macOS app renders the Quadcopter plus KSC base meshes/primitives with a simulation-backed external camera
- the Quadcopter physics is still a native bootstrap approximation, not the historical Orbiter runtime
- the repo still lacks Earth terrain/elevation archives, so the KSC scene uses a procedural ground fallback

Everything below is still left to do before native feature parity can be claimed.

## Definition Of Done

Feature parity means the native path can do the work the legacy Win32 Orbiter path is expected to do, with equivalent user-facing capability and acceptable behavioral fidelity.

That includes:

- real Orbiter simulation/runtime integration instead of bootstrap-only approximations
- scenario loading breadth, state persistence, and vessel/module compatibility
- planetary rendering and environment systems
- usable native UI, input, sound, and scripting support
- a stable add-on and SDK story for non-Windows builds
- installation, testing, and documentation that support ongoing maintenance

## Critical Path

These are the highest-priority dependency chains:

1. Replace the bootstrap Quadcopter session with a reusable seam into the real Orbiter runtime.
2. Restore real planet/terrain/base rendering on the native path.
3. Restore vessel/module/plugin loading compatibility on the native path.
4. Replace the bootstrap-only app shell with a maintainable native host architecture.
5. Close the remaining user-facing gaps: UI, input breadth, sound, scripting, persistence, and packaging.

## Workstreams

### 1. Native Runtime Architecture

- [ ] Decide whether the long-term architecture is:
  - a refactored shared `OrbiterSimCore` linked by native hosts
  - or a thinner host shim around a still-monolithic Orbiter core
- [ ] Move simulation ownership out of bootstrap-specific code and into a reusable runtime layer.
- [ ] Eliminate duplicate bootstrap-only domain logic once the real runtime seam exists.
- [ ] Define stable host-facing APIs for:
  - startup and shutdown
  - frame stepping
  - scenario load/reset
  - camera state
  - input/control submission
  - scene extraction
  - diagnostics/warnings/errors
- [ ] Make the native host able to run more than one scenario/vessel combination without code changes.
- [ ] Keep `Orbiter_server` as a real headless regression target, not a special-case side path.

### 2. Build System And Project Layout

- [ ] Continue separating portable targets from Win32-only targets at the CMake level.
- [ ] Remove remaining assumptions that the legacy `Orbiter` executable import library is the central integration point for everything.
- [ ] Introduce first-class native build targets for the sim core, renderer, and shared runtime assets.
- [ ] Audit all top-level options so native builds disable only what is truly unavailable, not what is merely unfinished.
- [ ] Add CI coverage for native configure/build/test flows once they are stable enough.
- [ ] Keep `mise` tasks current for configure/build/test/run/install/uninstall across supported native targets.
- [ ] Add native convenience tasks for headless regression runs and scenario smoke tests.

### 3. Core Simulation Integration

- [ ] Replace the bootstrap Quadcopter flight model with the actual Orbiter vessel/runtime path.
- [ ] Bring over real object lifecycle management:
  - planets
  - moons
  - bases
  - vessels
  - attached modules/plugins
- [ ] Integrate real timekeeping and simulation clock behavior.
- [ ] Integrate the real force model, propagation, atmosphere, and environment hooks.
- [ ] Restore scenario-driven object creation beyond the single bootstrap ship path.
- [ ] Support focus object changes, deletes, reloads, and multi-object scenes.
- [ ] Validate that simulation state matches legacy behavior closely enough to trust native regressions.

### 4. Celestial Bodies, Terrain, And Surface Systems

- [ ] Support the actual planetary texture installation path in the native renderer/runtime flow.
- [ ] Implement real Earth surface tile loading instead of the current procedural ground fallback.
- [ ] Implement elevation tile loading and terrain mesh generation.
- [ ] Implement surface tile LOD selection and caching.
- [ ] Integrate base placement against real planet/elevation data instead of a flat local frame.
- [ ] Restore additional celestial body support beyond the current KSC-centered bootstrap scene.
- [ ] Render coastlines, water, and large-scale planet views correctly.
- [ ] Restore atmospheric rendering features that matter for parity:
  - haze
  - clouds
  - horizon transitions
  - night lights
- [ ] Validate terrain/base alignment against legacy scenarios.

### 5. Base And Surface Object Fidelity

- [ ] Replace placeholder primitive rendering for `BLOCK`, `HANGAR`, `HANGAR2`, `HANGAR3`, `TANK`, and similar base objects with proper textured/object-type-specific rendering.
- [ ] Parse and apply base primitive texture assignments such as `TEX`, `TEX1`, `TEX2`, and `TEX3`.
- [ ] Respect additional base object parameters now ignored by the bootstrap path, such as:
  - roof geometry variants
  - wrap-to-surface behavior
  - under-shadow/shadow semantics
  - runway segmentation details
  - lights and navigational aids
- [ ] Support more base object classes used throughout the stock content, not just the subset currently surfaced.
- [ ] Validate representative bases beyond Cape Canaveral/KSC.

### 6. Vessel, Module, And SDK Compatibility

- [ ] Finish the public SDK portability layer so non-Windows add-ons do not depend on Win32 headers/types.
- [ ] Provide a stable native dynamic-library loading story for vessel modules, plugins, and celestial-body modules.
- [ ] Replace or abstract Win32-only module metadata/resource lookup.
- [ ] Build and load real stock vessel modules on native targets.
- [ ] Build and load real stock celestial-body modules on native targets.
- [ ] Define what binary/source compatibility guarantees native add-ons will have.
- [ ] Audit sample vessels and modules for Win32-only dependencies and triage them:
  - portable as-is
  - portable with cleanup
  - permanently Windows-only
- [ ] Restore DeltaGlider through the real runtime path rather than special asset staging.
- [ ] Restore add-on compatibility expectations as far as practical on the native platform.

### 7. Rendering Architecture

- [ ] Split renderer asset loading, cache management, scene extraction, and draw submission out of `OrbiterMacApp.mm`.
- [ ] Introduce a maintainable renderer module/layer instead of growing app-file logic.
- [ ] Support scene updates from a live simulation rather than static bootstrap snapshots where needed.
- [ ] Implement a richer material system compatible with Orbiter mesh expectations.
- [ ] Support mesh features and edge cases not yet covered by the lightweight loader/renderer.
- [ ] Add proper fallback/error reporting for missing meshes and textures.
- [ ] Add debug visualization and diagnostics for scene extraction/render state.
- [ ] Determine the long-term relationship between the native renderer and historical graphics-client concepts.

### 8. Camera And View Systems

- [ ] Replace the bootstrap external-camera-only implementation with the real Orbiter camera/view model.
- [ ] Support internal/external camera modes expected by stock scenarios and vessels.
- [ ] Support camera targeting, tracking modes, and focus changes consistently with legacy behavior.
- [ ] Integrate FOV, clipping, and view transforms with the real runtime state.
- [ ] Support scenario camera blocks more completely.

### 9. Input And Control Systems

- [ ] Replace the current minimal keyboard/mouse control mapping with a portable input layer that can express Orbiter control semantics.
- [ ] Support attitude/translation control modes expected by stock vessels.
- [ ] Support joystick/gamepad input.
- [ ] Support configurable bindings.
- [ ] Decide how much DirectInput-era behavior will be emulated versus replaced.
- [ ] Ensure input behavior remains stable under pause, focus change, and window state changes.

### 10. User Interface

- [ ] Decide whether Launchpad remains native, cross-platform, or separately scoped.
- [ ] Replace or port Win32 dialog/control flows needed for actual usability.
- [ ] Restore in-sim UI elements needed for parity:
  - HUD
  - MFDs
  - dialogs/panels
  - scenario/startup controls
- [ ] Replace any remaining Win32-only resource/dialog assumptions in the native host.
- [ ] Provide a native path for configuration editing and startup selection.
- [ ] Preserve user workflows expected from the legacy Orbiter experience where it still makes sense.

### 11. Cockpits, Panels, HUD, And MFDs

- [ ] Restore 2D panels and virtual cockpit support.
- [ ] Restore HUD rendering and surface/orbit/docking modes.
- [ ] Restore MFD rendering, interaction, and plugin integration.
- [ ] Support panel texture/render paths expected by stock vessels.
- [ ] Validate stock vessels that rely on cockpit/UI systems.

### 12. Sound

- [ ] Decide on the native sound architecture and replacement/port strategy for XRSound functionality.
- [ ] Support sound playback for stock/native scenarios.
- [ ] Determine packaging and runtime dependencies for native sound backends.
- [ ] Validate sound integration with vessel events, camera modes, and environmental conditions.

### 13. Scripting And Automation

- [ ] Restore Lua integration on the native path through the real runtime.
- [ ] Validate interpreter startup and vessel/plugin Lua hooks.
- [ ] Restore script-driven scenarios and automation paths.
- [ ] Ensure native builds still ship the necessary scripting/runtime assets.

### 14. Scenario Loading, Saving, And Persistence

- [ ] Expand beyond the current narrow scenario parser to the full scenario surface needed for parity.
- [ ] Restore scenario save/write support.
- [ ] Support scenario sections currently ignored or only partially parsed by bootstrap code.
- [ ] Validate load/save round-tripping against legacy scenarios.
- [ ] Support user data/config persistence expectations on native platforms.

### 15. Content Coverage

- [ ] Audit all stock scenarios and classify native readiness.
- [ ] Expand from the Quadcopter KSC path to representative scenarios covering:
  - launch
  - orbit
  - reentry
  - docking
  - lunar/interplanetary operations
- [ ] Restore stock vessel breadth on the native path.
- [ ] Restore plugin/module breadth needed for stock content.
- [ ] Verify content staging/copy/install rules so required assets are available in the native runtime tree.

### 16. Performance And Stability

- [ ] Profile scene extraction, mesh loading, and renderer hot paths.
- [ ] Add asset caches that survive across scene reloads where appropriate.
- [ ] Improve terrain/base/mesh streaming behavior once real terrain is implemented.
- [ ] Add crash diagnostics for native hosts.
- [ ] Validate long-running session stability.
- [ ] Validate pause/resume/reload/shutdown behavior.

### 17. Testing And Regression Coverage

- [ ] Add automated headless scenario smoke tests for the native runtime.
- [ ] Add tests for scenario parsing and base-config parsing.
- [ ] Add tests for mesh loading edge cases.
- [ ] Add golden or tolerance-based comparisons for simulation state where practical.
- [ ] Add renderer smoke coverage where practical.
- [ ] Add explicit regression coverage for known portability breakpoints:
  - asset-root discovery
  - dynamic library loading
  - texture lookup case sensitivity
  - sampler/pipeline/platform API mismatches

### 18. Packaging And Installation

- [ ] Complete install layouts for native command-line tools and app bundles.
- [ ] Ensure runtime assets required by native builds are installed correctly.
- [ ] Handle external planet texture installs cleanly and document the workflow.
- [ ] Validate uninstall behavior.
- [ ] Add release packaging for native builds when the product is ready.

### 19. Documentation And Contributor Guidance

- [ ] Keep `AGENTS.md`, `PORTING.md`, `README.md`, and this file synchronized with actual repo state.
- [ ] Document the intended native architecture once the real runtime seam is chosen.
- [ ] Document the supported native build matrix and known limitations.
- [ ] Document the add-on/module portability story for contributors.
- [ ] Keep a clear list of blocked work that depends on missing external content, especially planetary terrain data.

## Known Blockers And External Dependencies

- Earth terrain/elevation archives are not present in-repo, so full terrain parity cannot be finished until the runtime has access to an external Orbiter texture installation or equivalent staged content.
- Several stock and sample components still depend on Win32-specific APIs or historical DirectX-era assumptions.
- Native sound and some UI decisions still require architectural choices, not just implementation effort.

## Near-Term Recommended Order

1. Integrate the real Orbiter simulation/runtime behind the native session seam.
2. Implement true terrain/elevation loading for Earth and remove the procedural ground fallback.
3. Upgrade base primitive rendering so KSC and other surface bases stop showing placeholder cubes.
4. Factor the renderer out of `OrbiterMacApp.mm`.
5. Expand content breadth from Quadcopter/KSC to representative stock scenarios and vessels.
6. Close the remaining user-facing gaps: HUD/MFD/cockpit/UI/input/sound.

## Notes

- This file is intentionally broader than the current bootstrap backlog.
- If a task is completed, remove or rewrite the remaining work rather than leaving stale items behind.
- If scope changes, update both this file and `AGENTS.md` in the same task.
