#!/usr/bin/env python3
"""Stage Orbiter runtime assets into a build tree at the release layout.

Derives a source mapping from the pinned reference manifest
(Utils/assets/release_manifest_2024.json) by scanning the repository:

  1. Files already at their release-layout path are copied as-is.
  2. Meshes/textures/config/script assets are located by basename under the
     category-appropriate source roots (Src/Vessel/*/Meshes|Textures|Textures2,
     Src/Celbody/*/Config, Src/Vessel/*/Script, ...).
  3. Lua vessel definitions (Config/Vessels/**) come from Orbitersdk/samples/Lua/*.
  4. Vsop87B.dat per-planet ephemerides map to Src/Celbody/Vsop87/Data/Vsop87B_<xxx>.dat.
  5. Terrain archives (*.tree) come from --planet-tex-dir (default
     ~/Codeforge/orbiter-textures) and land at Textures/<Planet>/Archive/.

Usage:
  stage_assets.py --tree <build-dir> [--planet-tex-dir DIR] [--manifest JSON]
                  [--dry-run]

Exit codes: 0 all entries staged, 1 gaps remain, 2 inputs missing.
"""
import argparse, json, os, re, shutil, sys
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))

CATS = ("config/", "meshes/", "textures/", "textures2/", "scenarios/",
        "script/", "gravitymodels/", "flights/", "fonts/")
EXCL_TOPS = ("orbitersdk", "doc", "html", "images", "utils", "xrsound",
             "modules", "binassets")
SKIP_DIRS = {".git", "out", "build", ".cache", ".mise"}


def scan_repo(repo):
    tracked = []
    for dirpath, dirs, files in os.walk(repo):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        for f in files:
            tracked.append(os.path.relpath(os.path.join(dirpath, f), repo).replace(os.sep, "/"))
    bybase = defaultdict(list)
    for t in tracked:
        bybase[os.path.basename(t).lower()].append(t)
    return tracked, bybase


def category_score(target, cand):
    t, c = target.lower(), cand.lower()
    s = 0
    if t.startswith("meshes/") and "/meshes/" in c: s += 10
    if t.startswith("textures2/") and "/textures2/" in c: s += 10
    elif t.startswith("textures/") and "/textures/" in c: s += 10
    if t.startswith("config/") and "/config/" in c: s += 10
    if t.startswith("script/") and "/script/" in c: s += 10
    if t.startswith("scenarios/") and "/scenarios/" in c: s += 10
    if t.startswith("config/vessels/") and "/vessel/" in c: s += 8
    # canonical Lua vessel definitions live in the SDK samples tree
    if t.startswith("config/vessels/") and c.startswith("orbitersdk/samples/lua/"): s += 18
    # planet ephemeris data shipped loose under the celbody source trees
    if t.startswith("config/") and "/data/" in t and c.startswith("src/celbody/"): s += 10
    # UI fonts bundled with the Extern/imgui module sources
    if t.startswith("fonts/") and c.startswith("extern/imgui/"): s += 18
    if t.startswith("fonts/") and "/misc/fonts/" in c: s += 18
    # legacy MenuInfoBar icons: repo ships the Default/ variants
    if "menuinfobar/" in t and "/menuinfobar/default/" in c: s += 18
    # plugin-registered menu icons live beside the plugin sources
    if "menuinfobar/" in t and c.startswith("src/plugin/"): s += 18
    # Lua sample scenarios ship in the SDK samples tree
    if t.startswith("scenarios/lua/") and c.startswith("orbitersdk/samples/lua/"): s += 18
    if "/thumbnail/" in c: s -= 5
    if "/help/" in c: s -= 5
    if c.startswith("orbitersdk/"): s -= 8
    if c.startswith("extern/"): s -= 2
    return s


def derive_plan(repo, manifest_files):
    tracked, bybase = scan_repo(repo)
    trackedset = {t.lower() for t in tracked}
    plan, gaps = {}, []
    for e in manifest_files:
        p = e["path"]
        if p.endswith("/"):
            continue
        low = p.lower()
        top = low.split("/")[0]
        if not low.startswith(CATS) or top in EXCL_TOPS:
            continue
        if low in trackedset:
            plan[p] = p
            continue
        base = os.path.basename(p).lower()
        if low.endswith(".tree") or "/archive/" in low:
            plan[p] = None  # terrain: resolved from planet-tex-dir at copy time
            continue
        if re.fullmatch(r"vsop87b\.dat", base) and "/data/" in low:
            planet = p.split("/")[1].lower()
            for stem in (f"Vsop87B_{planet[:3]}", f"Vsop87E_{planet[:3]}"):
                src = f"Src/Celbody/Vsop87/Data/{stem}.dat"
                if os.path.isfile(os.path.join(repo, src)):
                    plan[p] = src
                    break
            else:
                gaps.append((p, "ephemeris-missing"))
            continue
        if base == "vsop87e.dat":
            src = "Src/Celbody/Vsop87/Data/Vsop87E_sun.dat"
            if os.path.isfile(os.path.join(repo, src)):
                plan[p] = src
            else:
                gaps.append((p, "ephemeris-missing"))
            continue
        cands = sorted(bybase.get(base, []), key=lambda c: category_score(p, c), reverse=True)
        if cands and category_score(p, cands[0]) >= 8:
            plan[p] = cands[0]
        else:
            gaps.append((p, "no-source"))
    return plan, gaps


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tree", required=True, help="build tree to stage into")
    ap.add_argument("--planet-tex-dir", default=os.path.expanduser("~/Codeforge/orbiter-textures"))
    ap.add_argument("--manifest", default=os.path.join(HERE, "release_manifest_2024.json"))
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    if not os.path.isfile(args.manifest):
        print(f"manifest not found: {args.manifest}", file=sys.stderr); return 2

    with open(args.manifest) as f:
        manifest = json.load(f)["files"]

    plan, gaps = derive_plan(REPO, manifest)

    # terrain archives: index planet-tex-dir by basename
    terrain = defaultdict(list)
    if os.path.isdir(args.planet_tex_dir):
        for dirpath, _dirs, files in os.walk(args.planet_tex_dir):
            for fn in files:
                if fn.lower().endswith(".tree"):
                    terrain[fn.lower()].append(os.path.join(dirpath, fn))

    staged = 0
    terrain_missing = []
    for target, source in sorted(plan.items()):
        dest = os.path.join(args.tree, target)
        if source is None:  # terrain archive
            base = os.path.basename(target).lower()
            hits = terrain.get(base)
            if not hits:
                terrain_missing.append(target)
                continue
            if not args.dry_run:
                os.makedirs(os.path.dirname(dest), exist_ok=True)
                shutil.copy2(hits[0], dest)
            staged += 1
            continue
        src_abs = os.path.join(REPO, source)
        if not os.path.isfile(src_abs):
            gaps.append((target, f"source-vanished:{source}"))
            continue
        if not args.dry_run:
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            shutil.copy2(src_abs, dest)
        staged += 1

    print(f"staged: {staged}  gaps: {len(gaps)}  terrain-missing: {len(terrain_missing)}")
    for t in terrain_missing[:20]:
        print(f"  TERRAIN MISSING {t}")
    for p, r in gaps[:20]:
        print(f"  GAP [{r}] {p}")
    if gaps or terrain_missing:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
