#!/usr/bin/env python3
"""CPU-only independent installed TX/1 + ELE/1 scalar oracle (stdlib only).
No engine archive loader, grid, interpolator, or renderer is reused.
"""
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'metal'))
from fixture_staging import stage
import zlib

MISSING = 0xffffffff


def require(value, message):
    if not value:
        raise AssertionError(message)


class Archive:
    def __init__(self, path):
        self.path = Path(path)
        self.f = self.path.open('rb')
        h = self.f.read(48)
        magic, size, flags, self.offset, self.length, count, *roots = struct.unpack('<4sIIIQ6I', h)
        require(magic == b'TX\x01\0' and size == 48 and flags in (0, 1), 'TX header')
        require(self.offset >= 48+32*count and self.offset+self.length == self.path.stat().st_size, 'TX bounds')
        toc = self.f.read(32*count)
        self.nodes = [struct.unpack_from('<QI4I', toc, 32*i) for i in range(count)]
        self.roots = roots[-2:]
        self.tiles = {}
        def walk(index, level, row, col):
            if index == MISSING:
                return
            require(index < count, 'child index')
            self.tiles[level, row, col] = index
            for q, child in enumerate(self.nodes[index][2:]):
                walk(child, level+1, row*2+q//2, col*2+q%2)
        for col, root in enumerate(self.roots):
            walk(root, 4, 0, col)
        self.cache = {}

    def tile(self, key):
        if key in self.cache:
            return self.cache[key]
        index = self.tiles.get(key)
        if index is None or self.nodes[index][1] == 0:
            return None
        pos, length, *_ = self.nodes[index]
        end = self.nodes[index+1][0] if index+1 < len(self.nodes) else self.length
        require(0 <= pos < end <= self.length and 100 <= length <= 100+259*259*2, 'payload bounds')
        self.f.seek(self.offset+pos)
        raw = zlib.decompress(self.f.read(end-pos))
        require(len(raw) == length and len(raw) >= 100, 'inflate length')
        magic, size, dtype, nx, ny, px, py, *values = struct.unpack_from('<4s6i9d', raw)
        require(magic == b'ELE\x01' and 100 <= size <= len(raw), 'ELE header')
        require((nx, ny, px, py) == (259, 259, 1, 1), 'grid and border contract')
        # Source supports 0, UINT8 and INT16 only. Signed8/UINT16/float
        # are NOT safe engine formats despite wider format comments elsewhere.
        require(dtype in (0, 8, -16), 'unsupported engine dtype '+str(dtype))
        n = nx*ny
        require(len(raw) == size+n*({0: 0, 8: 1, -16: 2}[dtype]), 'exact payload length')
        data = [0]*n if dtype == 0 else list(struct.unpack_from('<'+str(n)+('B' if dtype == 8 else 'h'), raw, size))
        tile = dict(key=key, dtype=dtype, scale=values[0], offset=values[1], bounds=values[2:6], extrema=values[6:9],
                    sha256=hashlib.sha256(raw).hexdigest(), raw=data)
        self.cache[key] = tile
        return tile


def grid(base, modification):
    # Earth target quantization is independently read/checked from installed cfg.
    data = [math.trunc(x*base['scale']/.5)+math.trunc(base['offset']/.5) for x in base['raw']]
    changed = []
    if modification:
        mask = {0: None, 8: 255, -16: 32767}[modification['dtype']]
        for i, x in enumerate(modification['raw']):
            if mask is None or x != mask:
                value = math.trunc(x*modification['scale']/.5)+math.trunc(modification['offset']/.5)
                if value != data[i]:
                    changed.append(i)
                data[i] = value
    require(all(-32768 <= x <= 32767 for x in data), 'INT16 range')
    return data, changed


def locate(archive, lng, lat, requested):
    # SDK cap 21 from installed Earth.cfg, and SURF_MAX_PATCHLEVEL2 is >=21.
    internal = min(max(requested-7, 0), 14) if requested else 14
    for p in range(internal, -1, -1):
        n = 2**p
        key = (p+4, int((.5-lat/math.pi)*n), int((1+lng/math.pi)*n))
        if archive.tile(key):
            return key
    raise AssertionError('no installed tile')


def interpolate(data, key, lng, lat, mode):
    level, row, col = key
    n = 2**(level-4)
    x = ((lng/math.pi+1)*n-col)*256
    y = ((lat/math.pi-.5)*n+row+1)*256
    ix, iy = math.floor(x), math.floor(y)
    fx, fy = x-ix, y-iy
    def weights(t):
        # Cardinal cubic convolution basis, not engine nested polynomial code.
        return [-t*(1-t)**2/2, 1-2.5*t*t+1.5*t**3, t/2+2*t*t-1.5*t**3, -t*t*(1-t)/2]
    if mode == 1:
        xs, ys, wx, wy = [ix, ix+1], [iy, iy+1], [1-fx, fx], [1-fy, fy]
    else:
        xs, ys, wx, wy = list(range(ix-1, ix+3)), list(range(iy-1, iy+3)), weights(fx), weights(fy)
    require(min(xs) >= -1 and max(xs) <= 257 and min(ys) >= -1 and max(ys) <= 257, 'padding bounds')
    return .5*sum(a*b*data[(y+1)*259+x+1] for y, b in zip(ys, wy) for x, a in zip(xs, wx))


def coordinate(key, x, y):
    level, row, col = key
    n = 2**(level-4)
    return math.pi*((col+x/256)/n-1), math.pi*(.5-(row+1-y/256)/n)


def main():
    exe, content, output = (Path(p).resolve() for p in sys.argv[1:4])
    output.mkdir(parents=True, exist_ok=True)
    launch = stage(content, output, 'elevation-content-')
    earthcfg = (launch/'Config/Earth.cfg').read_text()
    require('ElevationResolution = 0.5' in earthcfg and 'MaxPatchResolution = 21' in earthcfg, 'Earth config changed')
    base = Archive(launch/'Textures/Earth/Archive/Elev.tree')
    mods = Archive(launch/'Textures/Earth/Archive/Elev_mod.tree')
    witnesses = []
    def add(name, lng, lat, req=21):
        witnesses.append(dict(name=name, lng=lng, lat=lat, requested=req))
    # Installed Earth.cfg fixed observer; offsets make genuine fractional coastal probes.
    for delta in (0, -.02, .02):
        add('KSC-coast', math.radians(-80.6209+delta), math.radians(28.6235+delta))
    # Select mountain from ACTUAL root scalar maxima, not assumed high-resolution coverage.
    peaks = []
    for col in (0, 1):
        t = base.tile((4, 0, col))
        g, _ = grid(t, None)
        for y in range(1, 256):
            for x in range(1, 256):
                peaks.append((g[(y+1)*259+x+1], col, x, y))
    _, col, x, y = max(peaks)
    lng, lat = coordinate((4, 0, col), x, y)
    mountain_key = locate(base, lng, lat, 21)
    mountain, _ = grid(base.tile(mountain_key), mods.tile(mountain_key))
    maximum = max((mountain[(y+1)*259+x+1], x, y) for y in range(2, 255) for x in range(2, 255))
    _, x, y = maximum
    for dx, dy in ((.23, .37), (3.61, .81), (-2.42, -1.24)):
        add('archive-selected-mountain', *coordinate(mountain_key, x+dx, y+dy))
    for xedge, yedge in ((.02, .03), (255.97, 255.98), (.02, 127.45)):
        add('mountain-padding-stencil', *coordinate(mountain_key, xedge, yedge), mountain_key[0]+3)
    # A real modified scalar with changed value, at archive-supported level.
    for key in sorted(mods.tiles, reverse=True):
        b, m = base.tile(key), mods.tile(key)
        if not b or not m:
            continue
        g, changed = grid(b, m)
        selected = [i for i in changed if 2 <= i//259 <= 255 and 2 <= i%259 <= 255]
        if selected:
            i = selected[len(selected)//2]
            add('installed-Elev_mod', *coordinate(key, i%259-1+.21, i//259-1+.31), key[0]+3)
            break
    require(any(w['name'] == 'installed-Elev_mod' for w in witnesses), 'no changed installed mod witness')
    # Request-level and eviction ordering: repeat after other locations and levels.
    for req in (7, 10, 14, 21, 0):
        add('mountain-level-cache', lng, lat, req)
    witnesses += [dict(witnesses[0]), dict(witnesses[3])]
    def expected(w, mode):
        key = locate(base, w['lng'], w['lat'], w['requested'])
        b, m = base.tile(key), mods.tile(key)
        g, changed = grid(b, m)
        return interpolate(g, key, w['lng'], w['lat'], mode), key[0]+3, dict(key=key, base_sha256=b['sha256'], mod_sha256=m['sha256'] if m else None,
                    mod_changed_scalars=len(changed), mod_dtype=m['dtype'] if m else None,
                    mod_preserved_sentinels=sum(v == {0: None, 8: 255, -16: 32767}[m['dtype']] for v in m['raw']) if m else 0,
                    dtype=b['dtype'], scale=b['scale'], offset=b['offset'], bounds=b['bounds'])
    def run(mode, swap=False):
        name = 'swap' if swap else 'mode'+str(mode)
        inputs = output/(name+'.txt')
        inputs.write_text(''.join(f"{w['lat'] if swap else w['lng']:.17g} {w['lng'] if swap else w['lat']:.17g} {w['requested']}\n" for w in witnesses))
        p = subprocess.run([str(exe), 'MetalEarth', str(inputs), str(mode)], cwd=launch, capture_output=True, text=True, timeout=90)
        log = p.stdout+p.stderr
        (output/(name+'.log')).write_text(log)
        require(p.returncode == 0 and 'world_closed=1 PASS' in log and 'cache_released_before_world_close=1' in log, 'engine lifecycle: '+log[-2000:])
        engine = (launch/'Orbiter_elevation.log').read_text()
        (output/(name+'.engine.log')).write_text(engine)
        require('VSOP87(B) Earth:' in engine and 'VSOP87(E) Sun:' in engine, 'native Earth/Sun not initialized')
        rows = [line.split()[1:] for line in log.splitlines() if line.startswith('sample ')]
        require(len(rows) == len(witnesses), 'sample count')
        return rows
    provenance = {}
    for rel in ('Config/Earth.cfg', 'Orbiter.cfg', 'Modules/Celbody/Earth.dylib',
                'Modules/Celbody/Sun.dylib', 'Modules/Vsop87.dylib', 'Config/Earth/Data/Vsop87B.dat'):
        path = launch/rel
        payload = path.read_bytes()
        provenance[rel] = dict(path=str(path.resolve()), bytes=len(payload), sha256=hashlib.sha256(payload).hexdigest())
    report: dict = dict(launch=str(launch), mountain_key=mountain_key, provenance=provenance, witnesses=[], modes={}, negatives={})
    for mode in (1, 2):
        rows = run(mode)
        errors, mountain_heights = [], []
        for w, row in zip(witnesses, rows):
            h, level, metadata = expected(w, mode)
            actual = float(row[4]); error = abs(h-actual)
            require(int(row[3]) == level and level != 0 and error < 1e-6, f'oracle mismatch {w}: {row} != {h}, SDK {level}')
            errors.append(error)
            if w['name'] == 'archive-selected-mountain':
                mountain_heights.append(actual)
            report['witnesses'].append(dict(w, mode=mode, expected=h, actual=actual, sdk_level=level, metadata=metadata))
        require(min(mountain_heights) > 1000 and max(mountain_heights)-min(mountain_heights) > 10, 'zero/frozen/flat mountain')
        report['modes'][mode] = dict(max_error=max(errors), samples=len(rows), mountain_heights=mountain_heights)
    zero = run(0)
    require(all(int(r[3]) == 0 and float(r[4]) == 0 for r in zero), 'mode-zero contract')
    for name, rows in [('disabled-real-engine', zero), ('swapped-real-API-arguments', run(1, True))]:
        mismatches = sum(int(row[3]) != expected(w, 1)[1] or abs(float(row[4])-expected(w, 1)[0]) > 1e-6 for w, row in zip(witnesses, rows))
        require(mismatches > 0, 'oracle accepted negative '+name)
        report['negatives'][name] = mismatches
    ignored_mod = []
    for w in witnesses:
        key = locate(base, w['lng'], w['lat'], w['requested'])
        unmodified, _ = grid(base.tile(key), None)
        ignored_mod.append(abs(interpolate(unmodified, key, w['lng'], w['lat'], 1)-expected(w, 1)[0]))
    require(max(ignored_mod) > 1, 'modification bypass would not be detected')
    report['negatives']['ignored-mod-max-error-m'] = max(ignored_mod)
    frozen_mismatches = sum(abs(expected(w, 1)[0]-123.0) > 1e-6 for w in witnesses)
    require(frozen_mismatches > 0, 'frozen scalar rejection')
    report['negatives']['frozen-123m-mismatches'] = frozen_mismatches
    report['archives'] = {str(a.path.resolve()): dict(nodes=len(a.nodes), tiles=len(a.tiles), levels=sorted(set(k[0] for k in a.tiles))) for a in (base, mods)}
    (output/'results.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(dict(status='PASS', modes=report['modes'], negatives=report['negatives'], mountain_key=mountain_key)))


if __name__ == '__main__':
    main()
