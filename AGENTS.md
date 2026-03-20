- ## Top-Level Directives
	- 1. Keep every Markdown document in this repository current at the end of each task.
	- 2. Maintain this file as the primary contributor guide for future code agents.
	- 3. Preserve a changelog below as append-only and date-stamped.
	- 4. Rewrite the handoff section completely after every task so the next agent starts from current reality. If it's not there, create it.
	- 5. Ensure `.gitignore` excludes build artefacts, packaging output, and local-only tool state.
	- 6. Remove build artefacts before returning control to the user.
	- 7. Use `mise` to manage the build environment. This should include at the bare minimum tasks for building, cleaning, testing, running, installing, and uninstalling the components of the project.
		- NOTE: cmd line tools should install to $HOME/.local/bin. Mac applications should install to $HOME/Applications. Linux applications should install to /usr/local/bin. Windows applications should install to a relevant subdirectory under C:/Program Files/

## Current Reality

- The repo now has a split build story:
  - legacy Win32 Orbiter remains the full historical simulator path
  - portable bootstrap targets exist for groundwork: `Orbiter_server` and `Orbiter_macOS`
- `Orbitersdk/include/OrbiterPlatform.h` is now the entry point for public platform typedefs and export macros.
- The new portable runtime code lives in `Src/Platform`.
- The new shared bootstrap code lives in `Src/Bootstrap`.
- `Orbiter_macOS` is still a bootstrap app rather than a full simulator host, but it now renders the stock DeltaGlider Mk4 mesh with a minimal Metal renderer.
- `Orbiter_server` currently validates scenario argument handling and runtime directory separation; it is not yet a full headless simulation runner.
- macOS presets are `macos-arm64-debug` and `macos-arm64-release`.
- Apple builds use the system `ZLIB` package instead of the vendored `zlib` fetch target.
- The legacy `meshc` utility is excluded from the macOS/bootstrap build because it still depends on Direct3D 7 headers.
- The bootstrap runtime now discovers a content root by walking upward until it finds `Scenarios/`, so the macOS app bundle resolves assets from the build root instead of `Contents/MacOS`.
- `Orbiter_server` is still a validation executable that exits immediately after reporting its resolved launch state. `Orbiter_macOS` now runs until the window is closed unless `--maxframes` was explicitly requested.
- `CopyData` now also stages the stock DeltaGlider Mk4 assets into `Meshes/DG` and `Textures/DG`, and the macOS client contains a minimal `.msh` loader plus a Metal renderer for that vessel.
- The Metal renderer now draws opaque and transparent submeshes separately and uses back-face culling to reduce flicker on thin canopy/HUD polygons.
- `mise.toml` provides `configure-macos`, `build-macos`, `test-macos`, `run-macos`, `clean`, `install-macos`, and `uninstall-macos`, and the macOS tasks now use `cmake --fresh` to avoid stale build-graph state.

## Handoff

- The repo has passed the "first textured vessel" milestone on macOS: the native SDL3/Metal app opens, resolves its asset root from the build tree, and renders the stock DeltaGlider Mk4 with copied vessel textures.
- The bootstrap path is still intentionally separate from the real Orbiter simulation/runtime. `Orbiter_server` validates launch/bootstrap state and exits; `Orbiter_macOS` currently renders a standalone vessel rather than a scenario-backed scene.
- Public SDK groundwork is in place under `Orbitersdk/include/OrbiterPlatform.h`, and the portable runtime/bootstrap layer lives under `Src/Platform` and `Src/Bootstrap`.
- The key build blockers already addressed were: vendored `zlib` on Apple, legacy `meshc` pulling in `d3d.h`, bundle-relative asset lookup under `Contents/MacOS`, app auto-exit due to the shared frame-limit default, and AppleClang ObjC++ issues in the Metal renderer.
- The current Metal path has a depth buffer, textured mesh draw path, transparent/opaque split, and back-face culling. It is good enough for continued renderer bring-up, but it is not yet a scene renderer.
- The next engineering step should be to connect the bootstrap app to actual simulation/scenario startup, then replace the standalone hardwired vessel draw with scenario-driven object submission and a simulation-backed camera.

## Changelog

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
