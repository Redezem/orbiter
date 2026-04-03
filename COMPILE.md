PREREQUISITES
=============

Orbiter currently has two supported build tracks in this repository:

- Legacy Win32 simulator build
- Portable bootstrap/macOS groundwork build

LEGACY WIN32 PREREQUISITES
==========================

To build the legacy simulator from its sources you need a C++ compiler able to create Windows binaries.
Orbiter has been built successfully with Visual Studio 2019 Community
https://visualstudio.microsoft.com/downloads/

To quickly install Visual Studio components required for development, launch installer and 
import the [.vsconfig](./.vsconfig) file provided in the repository ([documentation](https://learn.microsoft.com/en-us/visualstudio/install/import-export-installation-configurations?view=vs-2022#import-a-configuration-using-the-visual-studio-installer))

If you want to build the Orbiter documentation, you need LaTeX. Multiple LaTeX distros for
Windows are available, for example MiKTeX.
https://miktex.org/download

To build the code-level documentation, you need doxygen.
https://www.doxygen.nl/index.html

MACOS BOOTSTRAP PREREQUISITES
=============================

To build the Apple Silicon macOS bootstrap targets you need:

- a recent Clang toolchain (Xcode command line tools are sufficient)
- CMake
- Ninja
- SDL3 development files discoverable by CMake

The repo now ships a `mise.toml` with tasks for configure/build/test/run/install/uninstall.


BUILDING ORBITER
================
Legacy Win32 path
-----------------

After cloning the Orbiter Git repository, you can either

- Load the Orbiter download directory as a local directory into VS2019 or later.
  
- Select Project | CMake Settings for Orbiter
  to check the build settings and make sure that all required components are found
  
- Select Project | Configure Orbiter
  This will configure the CMake build files.

- Select Build | Build All
  to build Orbiter and all its components.
  
Or, run CMake externally:

- Create a build directory separate from the Orbiter source directory.

- Run CMake, and select the correct source and build directories.

- Select Configure, and pick the Win32 platform

- Edit options as required.

- Select Generate, then close CMake

- This should have generated a solution file (Orbiter.sln) in the build directory.
  Load this into Visual Studio, and Build All.


MACOS BOOTSTRAP BUILD
=====================

Use the dedicated preset:

```bash
cmake --fresh --preset macos-arm64-debug
cmake --build --preset macos-arm64-debug
```

or the equivalent `mise` workflow:

```bash
mise run configure-macos
mise run build-macos
mise run test-macos
```

Important notes:

- The macOS preset is Apple Silicon (`arm64`) first.
- The preset intentionally disables the legacy Win32 core, D3D9Client, GDIClient, XRSound, Windows-only utilities, and SDK sample add-ons.
- The legacy `meshc` utility is also excluded on macOS for now because it still depends on Direct3D 7 headers.
- `Orbiter_server` now loads `Scenarios/Quadcopter/Quadcopter at KSC.scn`, steps the native bootstrap session, and reports focus vessel state at exit.
- `Orbiter_macOS` now loads the same scenario and renders the Quadcopter plus Cape Canaveral/KSC scene geometry through the shared bootstrap session layer. It is still not a full simulator port.
- The macOS `mise` tasks intentionally reconfigure with `cmake --fresh` so stale dependency state is discarded after CMake graph changes.
- The bootstrap path resolves runtime assets such as `Scenarios/` from the build root next to `Orbiter.app`, so running the bundle binary directly works.
- `CopyData` now also stages the Quadcopter mesh and the Earth base config needed for the native KSC scenario path.
- `Orbiter_macOS` runs until the window is closed by default; use `--maxframes=<n>` if you want it to auto-exit.
- The current Metal renderer separates transparent submeshes from opaque geometry, enables back-face culling, and consumes scenario-driven vessel/base/ground instances from the shared session.
- The repo still does not include Earth terrain/elevation archives, so the native KSC scene currently uses a procedural ground fallback instead of real planetary tiles.
  
  
PLANETARY TEXTURES
==================
The Orbiter git repository does not include the planetary texture files for most
celestial bodies. You need to install these separately (e.g. by installing Orbiter
2016 and optionally downloading high-res texture packs from the Orbiter website).

Set environment variable ORBITER_PLANET_TEXTURE_INSTALL_DIR in your profile so that
Orbiter build correctly configures the reference in configuration. 

Assuming Orbiter 2016 is installed in c:\orbiter2016:

```
setx ORBITER_PLANET_TEXTURE_INSTALL_DIR c:\orbiter2016\Textures
```

Alternatively, you can specify the location of the texture files as CMake variable:
```
cmake --preset windows-x64-debug -DORBITER_PLANET_TEXTURE_INSTALL_DIR=c:\orbiter2016\Textures
```
You can also set the planetary texture directory after building
Orbiter by setting the PlanetTexDir entry in Orbiter.cfg.


TROUBLESHOOTING
===============
* If you get errors during build, in particular when building documentation (pdf from
latex sources), try disabling multithreaded build support (limit to a single
thread). Some of the document converters/compilers you are using may not be
thread-safe. 

* CMake errors during build (cannot find system include files etc.). This may happen
when using the Ninja generator. You may need to install and configure vcpkg to allow
Ninja to find the VS2019 toolset (https://github.com/microsoft/vcpkg).

* LaTeX build components not found: If using MiKTeX, make sure you install it for all
users instead of locally for a single user. CMake won't automatically find the 
single-user installation, so you would have to specify the paths to all components
manually.

* Problems launching Orbiter from the build directory: If you use the VS2019
generator, it puts binaries in configuration-dependent subdirectories (Debug/Release).
This means that Orbiter may not find plugin DLLs. You need to run Orbiter from
the install directory. The Ninja generator separates the Debug and Release builds
at the top level and doesn't have that problem.

* macOS configure errors about `SDL3` not being found: install SDL3 and make sure CMake can discover `SDL3Config.cmake`.

* macOS build errors from `_deps/zlib-*`: the Apple path now uses the system `ZLIB` package. Remove the old build tree with `mise run clean`, then rerun `mise run build-macos`.
