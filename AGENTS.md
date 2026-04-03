- ## Top-Level Directives
	- 1. Keep every Markdown document in this repository current at the end of each task.
	- 2. Maintain this file as the primary contributor guide for future code agents.
	- 3. Preserve a changelog below as append-only and date-stamped.
	- 4. Rewrite the handoff section completely after every task so the next agent starts from current reality. If it's not there, create it.
	- 5. Ensure `.gitignore` excludes build artefacts, packaging output, and local-only tool state.
	- 6. Remove build artefacts before returning control to the user.
	- 7. Use `mise` to manage the build environment. This should include at the bare minimum tasks for building, cleaning, testing, running, installing, and uninstalling the components of the project.
	- 8. Keep `TODO.md` up to date as the canonical feature-parity roadmap and remaining-work checklist.
		- NOTE: cmd line tools should install to $HOME/.local/bin. Mac applications should install to $HOME/Applications. Linux applications should install to /usr/local/bin. Windows applications should install to a relevant subdirectory under C:/Program Files/

## Current Reality

- The repo now has a split build story:
  - legacy Win32 Orbiter remains the full historical simulator path
  - portable bootstrap targets now run a constrained native scenario path: `Orbiter_server` and `Orbiter_macOS`
- `Orbitersdk/include/OrbiterPlatform.h` is now the entry point for public platform typedefs and export macros.
- The new portable runtime code lives in `Src/Platform`.
- The new shared bootstrap code lives in `Src/Bootstrap`.
- `Src/Bootstrap/SimulationSession.{h,cpp}` now owns the native single-scenario bootstrap path: scenario parsing, KSC base config parsing, a narrow Quadcopter flight model, camera state, and renderer-facing scene extraction.
- `Src/Bootstrap/OrbiterMesh.{h,cpp}` is now the shared Orbiter `.msh` loader used by the bootstrap layer and the macOS host.
- `Orbiter_server` now loads `Scenarios/Quadcopter/Quadcopter at KSC.scn`, steps the native bootstrap session, and reports focus vessel state at exit.
- `Orbiter_macOS` now consumes the shared bootstrap scene instead of a hardwired DeltaGlider mesh and renders the Quadcopter plus KSC base meshes/primitives with a simulation-backed external camera.
- macOS presets are `macos-arm64-debug` and `macos-arm64-release`.
- Apple builds use the system `ZLIB` package instead of the vendored `zlib` fetch target.
- The legacy `meshc` utility is excluded from the macOS/bootstrap build because it still depends on Direct3D 7 headers.
- The bootstrap runtime now discovers a content root by walking upward until it finds `Scenarios/`, so the macOS app bundle resolves assets from the build root instead of `Contents/MacOS`.
- `CopyData` now also stages the Quadcopter mesh and the Earth base config needed by the native KSC bootstrap path, in addition to the existing DeltaGlider staging.
- The Metal renderer still draws opaque and transparent submeshes separately and uses back-face culling to reduce flicker on thin polygons, but it is now scene-driven rather than a one-mesh demo.
- The repo still does not include Earth terrain/elevation tile archives, so the native KSC scene currently uses a procedural ground patch instead of true Orbiter terrain tiles.
- `TODO.md` now exists as the canonical parity backlog and should be updated whenever scope, blockers, or completed work changes.
- `mise.toml` provides `configure-macos`, `build-macos`, `test-macos`, `run-macos`, `clean`, `install-macos`, and `uninstall-macos`, and the macOS tasks now use `cmake --fresh` to avoid stale build-graph state.

## Handoff

- The repo is still at the first native scenario milestone on the bootstrap track: `Src/Bootstrap/SimulationSession.cpp` parses `Scenarios/Quadcopter/Quadcopter at KSC.scn`, extracts the focus/camera/ship state, parses `Config/Earth/Base/Canaveral.cfg`, builds a renderer-facing scene, and steps a narrow native Quadcopter flight model.
- `Src/OrbiterServer/main.cpp` remains the headless regression target for that session. It loads the scenario, steps for `--maxframes`, and prints focus vessel position/velocity/rotation plus warnings at exit. A local Linux build of this path succeeded earlier in `/workspace/out/build/codex-dev`, and the resulting `Orbiter_server` run against the Quadcopter KSC scenario exited cleanly.
- `Src/OrbiterMac/OrbiterMacApp.mm` consumes the shared `SimulationSession` scene instead of a hardwired `Meshes/DG/deltaglider.msh` path. A follow-up fix on 2026-04-03 corrected the Metal sampler descriptor enums to `MTLSamplerMinMagFilterLinear`, which was required for AppleClang/macOS builds after the scene-renderer rewrite.
- The bootstrap implementation is still intentionally not the historical Orbiter runtime. The Quadcopter physics is a native bootstrap approximation, module loading is still not wired in, and the scene path is limited to one scenario/vessel class.
- The biggest remaining functional gap is terrain. The repo does not ship the Earth surface/elevation archives, and the bootstrap scene therefore falls back to a procedural KSC ground plane. The next agent should not mistake the presence of KSC base geometry for full planetary-terrain support.
- `TODO.md` is now the authoritative detailed checklist for everything left before native feature parity. The highest-value next steps remain: replace the bootstrap Quadcopter physics/session seam with real Orbiter runtime integration, implement true Earth terrain/elevation extraction once the tile archives are available at runtime, and then factor the growing Metal renderer/cache code out of `OrbiterMacApp.mm`.

## Changelog

- 2026-04-03: Added `TODO.md` as the canonical feature-parity backlog and updated contributor instructions to keep it current.
- 2026-04-03: Fixed the macOS Metal sampler setup after the scene-renderer rewrite by using `MTLSamplerMinMagFilterLinear` for `MTLSamplerDescriptor::minFilter` and `magFilter`.
- 2026-04-03: Added a shared bootstrap `SimulationSession` and `.msh` loader, staged Quadcopter/Earth base assets into the runtime tree, rewired `Orbiter_server` into a headless session runner, rewired `Orbiter_macOS` to render the Quadcopter KSC scene, and updated docs for the current procedural-ground limitation.
- 2026-04-03: Reviewed the repo state after the first textured-vessel milestone, rewrote the AGENTS handoff around the actual bootstrap/macOS entry points, and clarified the next engineering steps toward simulation-backed scene startup.
- 2026-03-20: Added `OrbiterPlatform.h`, portable runtime/bootstrap libraries, `Orbiter_server`, `Orbiter_macOS`, macOS CMake presets, `mise` tasks, and updated build/docs for the new macOS groundwork path.
- 2026-03-20: Switched Apple builds to system `ZLIB`, made macOS `mise` tasks reconfigure with `cmake --fresh`, and updated docs/handoff for stale `_deps/zlib-*` recovery.
- 2026-03-20: Stopped building the legacy `meshc` utility on the macOS/bootstrap path because it still depends on `d3d.h`.
- 2026-03-20: Added runtime content-root discovery so the macOS app bundle and `Orbiter_server` find `Scenarios/` from the build root instead of `Contents/MacOS`.
- 2026-03-20: Made `Orbiter_macOS` stay open by default unless `--maxframes` is explicitly provided, while keeping `Orbiter_server` as a short-lived bootstrap validation tool.
- 2026-03-20: Added a lightweight `.msh` loader, staged the stock DeltaGlider Mk4 assets into the bootstrap runtime tree, and replaced the clear-only macOS path with a minimal textured Metal vessel renderer.
- 2026-03-20: Fixed AppleClang ObjC++ build issues in the macOS Metal renderer (`MTKTextureLoader` declaration, Objective-C selector syntax, vector literal calls, and SDL3 init check).
- 2026-03-20: Reduced thin-polygon flicker in the macOS Metal renderer by separating opaque/transparent passes, disabling transparent depth writes, and enabling back-face culling.
- 2026-03-20: Fixed the AppleClang `NSError **` pipeline-construction compile error by replacing the block-based Metal pipeline helper with explicit pipeline descriptor setup.
- 2026-03-20: Updated `PORTING.md` with an explicit current-state snapshot and an ordered next-steps roadmap after reaching the first textured-vessel milestone on macOS.
