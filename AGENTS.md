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
- The new host-facing native session/runtime seam lives in `Src/Runtime`.
- The new shared bootstrap code lives in `Src/Bootstrap`.
- `Src/Runtime/Runtime.{h,cpp}` now owns launch-option parsing, the runtime-owned scene/control/state contracts, backend selection, and the `Session` factory used by both native hosts.
- `Src/Bootstrap/SimulationSession.{h,cpp}` now implements the runtime `Session` interface for the working bootstrap backend: scenario parsing, KSC base config parsing, a narrow Quadcopter flight model, camera state, and renderer-facing scene extraction.
- `Src/Bootstrap/OrbiterMesh.{h,cpp}` is now the shared Orbiter `.msh` loader used by the bootstrap backend and produces runtime-owned mesh data.
- `Orbiter_server` now creates its session through `Src/Runtime`, defaults to the `bootstrap` backend, and reports focus vessel state at exit.
- `Orbiter_macOS` now creates its session through `Src/Runtime` instead of including bootstrap headers directly and still renders the Quadcopter plus KSC base meshes/primitives with a simulation-backed external camera.
- macOS presets are `macos-arm64-debug` and `macos-arm64-release`.
- Apple builds use the system `ZLIB` package instead of the vendored `zlib` fetch target.
- The legacy `meshc` utility is excluded from the macOS/bootstrap build because it still depends on Direct3D 7 headers.
- The bootstrap runtime now discovers a content root by walking upward until it finds `Scenarios/`, so the macOS app bundle resolves assets from the build root instead of `Contents/MacOS`.
- `CopyData` now also stages the Quadcopter mesh and the Earth base config needed by the native KSC bootstrap path, in addition to the existing DeltaGlider staging.
- The Metal renderer still draws opaque and transparent submeshes separately and uses back-face culling to reduce flicker on thin polygons, but it is now scene-driven rather than a one-mesh demo.
- Native hosts now accept `--backend=<bootstrap|orbiter-core>`. `bootstrap` is the working default backend; `orbiter-core` is currently a fail-fast stub with the fixed error `Orbiter core backend is not wired yet`.
- The repo still does not include Earth terrain/elevation tile archives, so the native KSC scene currently uses a procedural ground patch instead of true Orbiter terrain tiles.
- `Tests/Runtime.Session.cpp` plus the `Scenario.QuadcopterBootstrap` and `Scenario.OrbiterCoreStubFails` CTest entries now provide native regression coverage for the runtime seam.
- `TODO.md` now exists as the canonical parity backlog and should be updated whenever scope, blockers, or completed work changes.
- `mise.toml` provides `configure-macos`, `build-macos`, `test-macos`, `run-macos`, `clean`, `install-macos`, and `uninstall-macos`, and the macOS tasks now use `cmake --fresh` to avoid stale build-graph state.

## Handoff

- The runtime/session seam has landed. `Src/Runtime/Runtime.h` now defines the host-facing contracts, `Src/Runtime/Runtime.cpp` owns option parsing and session creation, and both native hosts now depend on that layer instead of bootstrap headers.
- The bootstrap path still remains the only working backend. `Src/Bootstrap/SimulationSession.cpp` still owns Quadcopter/KSC scenario parsing, KSC base extraction, procedural-ground scene construction, and the narrow native Quadcopter flight model, but it now sits behind the runtime `Session` interface.
- The new `orbiter-core` backend is intentionally only a stub. It is selectable with `--backend=orbiter-core`, but `create_session` currently fails immediately with `Orbiter core backend is not wired yet`. The next agent should replace that stub rather than expanding bootstrap-only behavior first.
- Native regression coverage now exists for the seam itself. A local Linux configure/build of the portable targets succeeded in `/workspace/out/build/codex-runtime`, and `ctest --test-dir out/build/codex-runtime -R 'Runtime.Session|Scenario\.'` passed before cleanup. The passing tests were `Runtime.Session`, `Scenario.QuadcopterBootstrap`, and `Scenario.OrbiterCoreStubFails`.
- Terrain is still externally constrained. The repo does not ship Earth surface/elevation tile archives, so the KSC scene still falls back to a procedural ground patch. Terrain work remains important, but the highest-value next step is wiring a real historical-runtime backend into `Src/Runtime`.
- `OrbiterMacApp.mm` is still a large renderer/cache hotspot, but it is no longer the critical-path blocker. The next agent should keep renderer extraction secondary to replacing the `orbiter-core` stub with the first real Orbiter backend implementation.

## Changelog

- 2026-04-05: Added `Src/Runtime` as the native host/session seam, moved launch/session contracts out of bootstrap ownership, rewired `Orbiter_server` and `Orbiter_macOS` through that seam, added a selectable fail-fast `orbiter-core` stub backend, added native runtime regression coverage, and refreshed the contributor/docs handoff around the new boundary.
- 2026-04-05: Reviewed the repo-owned Markdown roadmap/docs, documented the immediate next work chunk in `TODO.md`, and rewrote the handoff to prioritize introducing a reusable native runtime/session seam ahead of terrain or renderer follow-on work.
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
