# Native celestial ephemerides

Build and stage the stock source-based theories (from the repository root):

```sh
cmake --build out/build/macos-core-spike --target stage-native-ephemerides -j6
```

`native-ephemerides` builds without staging. `ORB_NATIVE_CONTENT_DIR` is a CMake
cache path, defaulting to `out/stage/content`. Staging copies exact source theory
data and built dylibs; it does not change planetary config/scenario assets.
Each VSOP build-tree coefficient file has an explicit `OUTPUT`/source `DEPENDS`
rule attached to its body target. Updating source data or deleting an intermediate
copy regenerates it before staging, without requiring a body module relink.

Isolated incremental-staging regression (use a new scratch directory):

```sh
python3 Src/OrbiterCore/modules/staging_regression.py \
  --work-dir /path/to/scratch/new-vsop-staging-test
```

This runs the actual ephemeris CMake build/data/stage rules with inert stub
libraries and explicitly labelled sentinel data in a separate source/build/stage
tree. It checks all nine VSOP files after coefficient-only changes and deleted
intermediates, checks byte equality, and verifies modules did not relink. It
never changes the repository's theory coefficients or the production content
root; it is a build-system regression, not scientific ephemeris validation.

The real SDK CELBODY2 callbacks are provided by VSOP87 Sun/planets, ELP82 Moon,
GALSAT Jupiter/Galilean satellites, and TASS17 Saturn/satellites. Support dylibs
live in `Modules`, body dylibs in `Modules/Celbody`; `@loader_path/..` resolves
support dependencies independently of the executable/current working directory.

The Windows SDK invokes InitModule through DllMain, even for dependent theory
libraries. Native dyld does not. `celbody_dependency_init.cpp` restores the real
InitModule callback for Moon, Galsat, and Satsat (and ExitModule for Moon). This
is required for correct theory coefficients, not just finite output: omitting
it leaves SATSAT with zero terms and Moon with uninitialised ELP82 constants.

Verification (hidden offscreen, no input/focus):

```sh
python3 Src/OrbiterCore/metal/camera_regression.py \
  out/build/macos-core-spike/Src/OrbiterCore/metal/orbiter-core-metal \
  out/stage/content /path/to/scratch/camera-results
python3 Src/OrbiterCore/metal/regression.py \
  out/build/macos-core-spike/Src/OrbiterCore/metal/orbiter-core-metal out/stage/content
python3 Src/OrbiterCore/metal/mesh_regression.py \
  out/build/macos-core-spike/Src/OrbiterCore/metal/orbiter-core-metal \
  out/stage/content /path/to/scratch/mesh-results
```

## Not a complete native Sol physics port

Phobos, Deimos, Miranda, Ariel, Umbriel, Titania, and Oberon ship only Windows
DLLs here, not buildable theory sources. They still fail to load on macOS.
The engine's existing fallback is **not** proof of correct satellite states:
observed finite globals can be wrong (including coincident/doubled parent
positions). Do not claim scientifically valid complete Sol from a finite camera
or a successful GPU test. Native atmospheric modules are also not built by this
target. No fake origin, finite-value clamp, invented orbit setup, or replacement
ephemeris has been introduced.
