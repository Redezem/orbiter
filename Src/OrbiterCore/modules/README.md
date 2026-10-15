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
or a successful GPU test. No fake origin, finite-value clamp, invented orbit setup,
or replacement ephemeris has been introduced.

## Native Earth atmosphere (explicit selection only)

`native-atmospheres` builds the unchanged stock `EarthAtmJ71G.cpp` model; explicit
`stage-native-atmospheres` stages `Modules/Celbody/Earth/Atmosphere/EarthAtmJ71G.dylib`.
J71G has no external coefficient files or common model library: ATMOSPHERE/CELBODY2
SDK methods resolve against the real exported core. Its original InitModule and
ExitModule callbacks (currently empty) run through the native dependency callback
seam, verified with module-qualified LLDB breakpoints.

**Neither target edits Config.** In particular an existing
`Config/Earth/Atmosphere.cfg` selecting `VenusAtm2006` continues to win over the
`Earth.cfg` default and still fails if that selected module is unavailable. This
is not a general Earth-atmosphere fix or a silent selection policy change.

```sh
cmake -S . -B out/build/macos-core-spike -DORBITER_BUILD_ATMOSPHERE_PROBE=ON
cmake --build out/build/macos-core-spike --target stage-native-atmospheres orbiter-atmosphere-probe -j6
python3 Src/OrbiterCore/smoke/atmosphere_regression.py \
  out/build/macos-core-spike/Src/OrbiterCore/orbiter-atmosphere-probe \
  out/stage/content /path/to/scratch/atmosphere-results
python3 -m unittest discover -s Src/OrbiterCore/smoke -p test_atmosphere_regression.py -v
```

The runner uses fresh owned Config and Config/Earth directories and an exclusive
regular nested override selecting J71G, never writing through the staged Earth
link. It samples actual engine atmosphere/constants and engine sound speed at 11
altitudes (0–400 km); checks finite positive quantities, decreasing pressure and
density, expected temperature-layer trends, finite real Sun/Earth pose/velocity,
and the engine's above-limit rejection. Eight 0–80 km samples are independently
checked against the source-defined lower-atmosphere hydrostatic integral and
molecular-weight correction, with relative tolerance 2e-12. Upper Jacchia samples
have trend/finite checks, not independent scientific validation of all polynomial
fits. The source's computed sea-level pressure is approximately 101329.12949 Pa,
not its advertised 101325 Pa constant; the test preserves that stock distinction.
Missing-module and `[None]` fixtures exit 4 with a clear error, no fake atmospheric
samples, and a post-normal-teardown FAIL marker. Nested aliases/symlinks/dangling
source overrides and stale output links have sentinel isolation tests.
