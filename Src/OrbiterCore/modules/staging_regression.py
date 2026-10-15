#!/usr/bin/env python3
"""Exercise production ephemeris CMake rules with isolated sentinel data.

The fixture compiles tiny inert shared libraries, NOT celestial theory. It
executes the production data/build/stage rules verbatim; source coefficients
and the configured production content root are never modified.
"""
import argparse
import pathlib
import subprocess


BODIES = "Sun Mercury Venus Earth Mars Jupiter Saturn Uranus Neptune".split()


def run(*args):
    subprocess.run(args, check=True)


def check_equal(source, destination, label, failures):
    if not destination.is_file() or source.read_bytes() != destination.read_bytes():
        failures.append(label)
        print("FAIL:", label, flush=True)
    else:
        print("PASS:", label, flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work-dir", type=pathlib.Path, required=True,
                        help="New isolated fixture directory (must not exist)")
    parser.add_argument("--generator", default="Ninja")
    args = parser.parse_args()
    root = args.work_dir.resolve()
    root.mkdir(parents=True, exist_ok=False)
    source = root / "source"
    source.mkdir()
    build = root / "build"
    stage = root / "stage"
    production = pathlib.Path(__file__).with_name("CMakeLists.txt").read_text()
    # Keep ALL actual ephemeris targets/data/staging commands. Only replace
    # orb_celbody_target's compilation implementation with inert fixture DLLs.
    rules = production[production.index("orb_celbody_target(Vsop87 "):
                       production.index("# Lua interpreter library")]
    (source / "stub.c").write_text("int orb_staging_fixture(void) { return 0; }\n")
    (source / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.20)
project(ephemeris_staging_fixture C)
set(ORB_CELBODY_SRC "${CMAKE_SOURCE_DIR}/Celbody")
set(ORB_MODULE_DIR "${CMAKE_BINARY_DIR}/Modules")
function(orb_celbody_target name)
  add_library(${name} SHARED "${CMAKE_SOURCE_DIR}/stub.c")
  set_target_properties(${name} PROPERTIES PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${ORB_MODULE_DIR}/Celbody")
endfunction()
''' + rules)
    inputs = {}
    for body in BODIES:
        series = "E" if body == "Sun" else "B"
        filename = f"Vsop87{series}_{body[:3].lower()}.dat"
        path = source / "Celbody" / "Vsop87" / "Data" / filename
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(f"STAGING TEST FIXTURE ONLY: {body} original\n".encode())
        inputs[body] = (path, pathlib.Path("Config") / body / "Data" / f"Vsop87{series}.dat")
    for relative in ("Moon/ELP82.dat", "Galsat/ephem_e15.dat", "Satsat/tass17.dat"):
        path = source / "Celbody" / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("STAGING TEST FIXTURE ONLY: " + relative + "\n")
    run("cmake", "-S", str(source), "-B", str(build), "-G", args.generator,
        f"-DORB_NATIVE_CONTENT_DIR={stage}")

    def stage_build():
        run("cmake", "--build", str(build), "--target", "stage-native-ephemerides", "-j6")

    stage_build()
    failures = []
    for body, (path, relative) in inputs.items():
        check_equal(path, build / relative, body + " initial intermediate", failures)
        check_equal(path, stage / relative, body + " initial stage", failures)
    libraries = list((build / "Modules").rglob("*.dylib"))
    if not libraries:  # Also permit Linux fixture execution.
        libraries = list((build / "Modules").rglob("*.so"))
    if len(libraries) != 25:
        raise AssertionError(f"Expected 25 fixture libraries, got {len(libraries)}")
    library_mtimes = {p: p.stat().st_mtime_ns for p in libraries}
    # New mtimes are explicitly later than outputs, independent of FS clock
    # resolution, without touching any real scientific source data.
    import os
    newest = max(p.stat().st_mtime_ns for p in build.rglob("*") if p.is_file())
    for body, (path, relative) in inputs.items():
        path.write_bytes(f"STAGING TEST FIXTURE ONLY: {body} updated\n".encode())
        timestamp = max(newest + 2_000_000_000, path.stat().st_mtime_ns)
        os.utime(path, ns=(timestamp, timestamp))
    run("cmake", "--build", str(build), "--target", "native-ephemerides", "-j6")
    for body, (path, relative) in inputs.items():
        check_equal(path, build / relative, body + " coefficient update intermediate", failures)
    stage_build()
    for body, (path, relative) in inputs.items():
        check_equal(path, stage / relative, body + " coefficient update stage", failures)
    # Deleting an intermediate must regenerate it without any module relink.
    for path, relative in inputs.values():
        (build / relative).unlink()
        (stage / relative).unlink()
    try:
        stage_build()
    except subprocess.CalledProcessError:
        failures.append("deleted-intermediate staging command failed")
        print("FAIL: deleted-intermediate staging command failed", flush=True)
    for body, (path, relative) in inputs.items():
        check_equal(path, build / relative, body + " deleted intermediate recovery", failures)
        check_equal(path, stage / relative, body + " deleted stage recovery", failures)
    if library_mtimes != {p: p.stat().st_mtime_ns for p in libraries}:
        failures.append("coefficient changes unexpectedly relinked fixture modules")
    else:
        print("PASS: no fixture module relinks", flush=True)
    print(f"RESULT: {len(failures)} failures; fixture retained at {root}", flush=True)
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
