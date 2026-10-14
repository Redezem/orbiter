#!/usr/bin/env python3
"""Orbiter asset manifest integrity check.

Compares a staged runtime tree (the CMake binary directory) against the pinned
reference manifest of the official Orbiter 2024 release layout
(Utils/assets/release_manifest_2024.json).

Usage:
  check_assets.py --tree <build-dir> [--planet-tex-dir <dir>] [--manifest <json>]

Exit codes:
  0  staged tree covers every required manifest entry
  1  gaps found (printed per category)
  2  manifest or tree missing

Terrain archives (*.tree under Textures/<Planet>/Archive/) are resolved against
the planet texture directory (Orbiter.cfg PlanetTexDir) rather than the build
tree, matching the runtime lookup path.
"""
import argparse, json, os, sys

# Manifest entries intentionally absent from the staged tree.
# Terrain archives live in PlanetTexDir; sample sources ship under Src/.
def load_manifest(path):
    with open(path) as f:
        m = json.load(f)
    return m["files"]

def tree_index(root):
    idx = {}
    for dirpath, _dirs, files in os.walk(root):
        for fn in files:
            rel = os.path.relpath(os.path.join(dirpath, fn), root).replace(os.sep, "/")
            idx[rel.lower()] = rel
    return idx

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tree", required=True, help="staged runtime tree (CMake binary dir)")
    ap.add_argument("--planet-tex-dir", default=None, help="PlanetTexDir for terrain archives")
    ap.add_argument("--manifest", default=os.path.join(os.path.dirname(__file__), "release_manifest_2024.json"))
    args = ap.parse_args()

    if not os.path.isdir(args.tree):
        print(f"tree not found: {args.tree}", file=sys.stderr); return 2
    if not os.path.isfile(args.manifest):
        print(f"manifest not found: {args.manifest}", file=sys.stderr); return 2

    manifest = load_manifest(args.manifest)
    idx = tree_index(args.tree)
    pidx = tree_index(args.planet_tex_dir) if args.planet_tex_dir else {}

    gaps = []
    terrain_ok = 0
    for entry in manifest:
        p = entry["path"]
        low = p.lower()
        if low.endswith(".tree") or "/archive/" in low:
            if low in pidx:
                terrain_ok += 1
            else:
                gaps.append(("terrain", p))
            continue
        if low in idx:
            continue
        gaps.append(("asset", p))

    if not gaps:
        print(f"OK: {len(manifest)} manifest entries covered "
              f"({terrain_ok} terrain archives via planet-tex-dir).")
        return 0

    by_cat = {}
    for cat, p in gaps:
        by_cat.setdefault(cat, []).append(p)
    for cat, items in sorted(by_cat.items()):
        print(f"GAP [{cat}] {len(items)} missing:")
        for p in items[:40]:
            print(f"  {p}")
        if len(items) > 40:
            print(f"  ... {len(items)-40} more")
    return 1

if __name__ == "__main__":
    sys.exit(main())
