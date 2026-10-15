# Portable DDS CPU decoder foundation

`dds_decoder.h/.cpp` is standalone C++17, with no engine, Metal, SDL, Win32,
Pillow, or other third-party runtime dependency. It supports legacy **2D DXT1
(BC1), DXT3 (BC2), DXT5 (BC3)** and returns an owned, tightly packed top-level
RGBA8 image. It validates the entire declared mip chain before allocating.
See the public header for caps, errors and color/interpolation semantics.
DX10, DXT2/4 premultiplied formats, uncompressed formats, cubemaps and volumes
are explicitly unsupported. DDS pitch/linear-size hints are not trusted for
bounds; block dimensions determine the actual storage required. Trailing bytes
are allowed. Allocation failures return an error with no partial image.

This directory deliberately has its own CMake test project; it is **not wired
into the renderer or the normal Windows build**.

```sh
cmake -S Src/OrbiterCore/metal/dds -B "$TMPDIR/orbiter-dds-build" \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS='-Wall -Wextra -Werror'
cmake --build "$TMPDIR/orbiter-dds-build" -j 4
ctest --test-dir "$TMPDIR/orbiter-dds-build" --output-on-failure
```

For Clang ASan/UBSan, use a separate build directory and add
`-fsanitize=address,undefined -fno-omit-frame-pointer` to CXX flags and
`-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined`.

`orbiter-dds-decode-file input.dds output.rgba` is a validation utility, not an
engine API. It writes raw CPU RGBA8 and prints `width height`.

With an independently installed Pillow:

```sh
python3 Src/OrbiterCore/metal/dds/compare_pillow.py \
  --decoder "$TMPDIR/orbiter-dds-build/orbiter-dds-decode-file" \
  --output-dir "$TMPDIR/orbiter-dds-comparison" \
  --asset /absolute/path/to/real-vessel-texture.dds
```

Repeat `--asset` to compare more files. The script always also runs 300 seeded
synthetic **codec unit fixtures**, guarantees all BC3 alpha branches/selectors,
and saves exact source/decoded hashes in `comparison.json`. Real assets and
synthetic fixtures are labelled separately. Synthetic data and decoded CPU
files are never evidence of engine/GPU texture loading or rendering.

Unit coverage includes BC1 four-color interpolation, three-color midpoint and
transparent selector (including equal endpoints); all BC2 alpha nibbles;
BC3 alpha endpoint order >, <, = and all selectors; header/PF/FourCC errors;
null, unaligned and every BC1 prefix boundary; mip-chain bounds; odd cropped
blocks; dimension/pixel caps and overflow-size rejection; and 20,000 bounded
seeded mutations under the same public decode entry point. This bounded smoke
fuzz is not exhaustive fuzzing.
