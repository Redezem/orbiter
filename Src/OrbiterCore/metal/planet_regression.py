#!/usr/bin/env python3
"""Independent API-pose sphere/mesh/Pillow oracle for real pre-UI Metal pixels.
No renderer vertices, UVs, matrices or GPU colors participate in sample selection.
"""
import hashlib
import io
import json
import math
from pathlib import Path
import re
import struct
import subprocess
import sys
import zlib
from PIL import Image, ImageFilter
from fixture_staging import stage
from mesh_regression import require

HASHES = ('e99f8df00bf3b72f0b25087695b63473225a8a52bdb23e95908afc7795b52a12',
          '63b9705ddcb3d318cdb506d6a2f6dc4266530870f2bc9a339f03d570943e4f99')
BG = (32, 64, 128)


def roots(path):
    """Independent documented TX/1 layout; no core reader/decoder reuse."""
    images, identities = [], []
    with Path(path).open('rb') as f:
        header = f.read(48)
        magic, size, flags, offset, length, count, r1, r2, r3, west, east = struct.unpack('<4sIIIQ6I', header)
        require(magic == b'TX\x01\x00' and size == 48 and offset >= size+count*32, 'archive header')
        toc = f.read(count*32)
        for h, index in enumerate((west, east)):
            require(0 <= index < count, 'root range')
            pos, inflated = struct.unpack_from('<QI', toc, index*32)
            end = struct.unpack_from('<Q', toc, (index+1)*32)[0] if index+1 < count else length
            require(128 <= inflated <= 4*1024*1024 and 0 <= pos < end <= length, 'root payload bounds')
            f.seek(offset+pos)
            dds = zlib.decompress(f.read(end-pos))
            digest = hashlib.sha256(dds).hexdigest()
            require(len(dds) == inflated and digest == HASHES[h], 'actual root identity')
            image = Image.open(io.BytesIO(dds)).convert('RGBA')
            require(image.size == (512, 512) and image.getchannel('A').getextrema() == (255, 255), 'root dimensions/opacity')
            images.append(image.convert('RGB'))
            identities.append(dict(hemisphere=h, index=index, bytes=len(dds), sha256=digest))
    return images, identities


def dot(a, b):
    return sum(x*y for x, y in zip(a, b))


def pose(f):
    axes = [f['cameraR'][i::3] for i in range(3)]
    delta = [b-c for b, c in zip(f['body'], f['camera'])]
    center = [dot(axis, delta) for axis in axes]
    r = [[dot(axis, f['bodyR'][i::3]) for i in range(3)] for axis in axes]
    return center, r


def project(f, point):
    center, rotation = pose(f)
    x, y, z = [dot(row, point)+center[i] for i, row in enumerate(rotation)]
    sy = 1/math.tan(f['aperture'])
    return (320+240*sy*x/z, 240-240*sy*y/z, z)


def sphere_mask(f):
    center, _ = pose(f)
    k = dot(center, center)-f['radius']**2
    require(k > 0 and abs(math.sqrt(dot(center, center))/f['radius']-3) < 1e-7, 'camera must be at 3 live Earth radii')
    scale = math.tan(f['aperture'])/240
    mask = bytearray(640*480)
    for y in range(480):
        dy = (240-y-.5)*scale
        for x in range(640):
            dx = (x+.5-320)*scale
            b = dx*center[0]+dy*center[1]+center[2]
            if b > 0 and b*b >= (dx*dx+dy*dy+1)*k:
                mask[y*640+x] = 255
    return Image.frombytes('L', (640, 480), bytes(mask))


def samples(f):
    """Independent 64-cell spherical mesh; scalar perspective UV raster at grid.
    Depth selects the frontmost triangle before boundary tests. Never GPU colors.
    """
    found = {}
    radius = f['radius']
    for h in range(2):
        vertices = []
        for row in range(65):
            lat = math.pi*(.5-row/64)
            for col in range(65):
                lon = math.pi*(-1+h+col/64)
                p = (radius*math.cos(lat)*math.cos(lon), radius*math.sin(lat), radius*math.cos(lat)*math.sin(lon))
                vertices.append((*project(f, p), col/64, row/64))
        for row in range(64):
            for col in range(64):
                a = row*65+col
                for ids in ((a, a+65, a+1), (a+1, a+65, a+66)):
                    v = [vertices[i] for i in ids]
                    ax, ay = v[0][:2]; bx, by = v[1][:2]; cx, cy = v[2][:2]
                    denom = (by-cy)*(ax-cx)+(cx-bx)*(ay-cy)
                    if abs(denom) < 1e-10 or min(p[2] for p in v) <= 1000:
                        continue
                    xmin, xmax = max(4, math.ceil(min(ax, bx, cx)-.5)), min(635, math.floor(max(ax, bx, cx)-.5))
                    ymin, ymax = max(4, math.ceil(min(ay, by, cy)-.5)), min(475, math.floor(max(ay, by, cy)-.5))
                    # Pixel centers are x+.5/y+.5, including for depth candidates.
                    for y in range(ymin+(4-ymin)%8, ymax+1, 8):
                        for x in range(xmin+(4-xmin)%8, xmax+1, 8):
                            u = ((by-cy)*(x+.5-cx)+(cx-bx)*(y+.5-cy))/denom
                            vweight = ((cy-ay)*(x+.5-cx)+(ax-cx)*(y+.5-cy))/denom
                            weights = (u, vweight, 1-u-vweight)
                            if min(weights) < 0:
                                continue
                            reciprocal = sum(w/p[2] for w, p in zip(weights, v))
                            depth = 1/reciprocal
                            old = found.get((x, y))
                            if old and old[0] <= depth:
                                continue
                            uv = [sum(w*p[k]/p[2] for w, p in zip(weights, v))/reciprocal for k in (3, 4)]
                            # Reject triangle edges, poles, hemispheric seams and
                            # nearest texel boundaries solely from expected geometry.
                            safe = min(weights) > .06 and all(.01 < t < .99 and .15 < (t*512)%1 < .85 for t in uv)
                            found[x, y] = (depth, h, uv, safe)
    return [(x, y, h, min(511, int(uv[0]*512)), min(511, int(uv[1]*512)))
            for (x, y), (depth, h, uv, safe) in sorted(found.items()) if safe]


def oracle(f, path, textures):
    im = Image.open(path).convert('RGB')
    require(im.size == (640, 480), 'GPU dimensions')
    expected = sphere_mask(f)
    interior = expected.filter(ImageFilter.MinFilter(5)).tobytes()
    exterior = expected.filter(ImageFilter.MaxFilter(5)).tobytes()
    observed = im.tobytes()
    interior_missing = exterior_extra = 0
    for i, rgb in enumerate(im.getdata()):
        is_planet = rgb != BG
        interior_missing += bool(interior[i] and not is_planet)
        exterior_extra += bool(not exterior[i] and is_planet)
    require(interior_missing == 0 and exterior_extra == 0,
            f'sphere geometry mismatch: missing={interior_missing} extra={exterior_extra} (2px limb tolerance)')
    selected = samples(f)
    counts = [sum(s[2] == h for s in selected) for h in range(2)]
    errors, records = [], []
    max_error = 0
    for x, y, h, u, v in selected:
        predicted = textures[h].getpixel((u, v))
        actual = im.getpixel((x, y))
        error = max(abs(a-b) for a, b in zip(actual, predicted))
        max_error = max(max_error, error)
        records.append(dict(x=x, y=y, hemisphere=h, texel=[u, v], expected=predicted, actual=actual, error=error))
        if error > 2:
            errors.append(dict(x=x, y=y, hemisphere=h, uv_texel=[u, v], expected=predicted, actual=actual))
    require(len(selected) >= 64, 'insufficient geometry-selected texture witnesses')
    Path(str(path)+'.oracle.json').write_text(json.dumps(dict(api_pose=f, samples=records,
         missing_interior=interior_missing, extra_exterior=exterior_extra), indent=2)+'\n')
    require(not errors, f'texture/pose mismatch: {len(errors)}/{len(selected)} samples max_error={max_error}; first={errors[:3]}')
    return dict(samples=len(selected), hemispheres=counts, max_channel_error=max_error,
                missing_interior=interior_missing, extra_exterior=exterior_extra,
                limb_tolerance_pixels=2, samples_selected_from='geometry-only'), observed


def verify_run(log, snapshot, textures, controlled):
    frames = [json.loads(s.split('planet_json=', 1)[1]) for s in log.splitlines() if 'planet_json=' in s]
    require(len(frames) == 12, '12 live planet poses required')
    for f in frames:
        require(all(math.isfinite(v) for key in ('body', 'camera', 'bodyR', 'cameraR') for v in f[key]), 'nonfinite actual API pose')
        require(f['radius'] > 6e6 and abs(f['aperture']-math.radians(20)) < 1e-9, 'live radius/FOV')
    first, rgb1 = oracle(frames[0], snapshot, textures)
    result = dict(first=first)
    if controlled:
        require(frames[0]['view'] == 1 and frames[6]['view'] == 2, 'controlled engine views missing')
        require(max(abs(a-b) for a, b in zip(frames[0]['cameraR'], frames[6]['cameraR'])) > .1, 'API camera unchanged')
        second, rgb2 = oracle(frames[6], Path(str(snapshot)+'.view2.ppm'), textures)
        changed = sum(rgb1[i:i+3] != rgb2[i:i+3] for i in range(0, len(rgb1), 3))
        require(changed > 10000, f'GPU pose image unchanged: {changed}')
        require(all(a+b >= 64 for a, b in zip(first['hemispheres'], second['hemispheres'])), 'both hemisphere witnesses required')
        result.update(second=second, changed_pixels=changed)
    else:
        require(all(f['view'] == 0 for f in frames), 'scenario camera overwritten')
    return result


def lifecycle(log):
    for marker in ('teardown complete; PASS', 'create=1 close=1 destroy=1 imgui_init=1 imgui_shutdown=1',
                   'completed=12 presented=0 readback=12 errors=0', 'hidden_checks=12',
                   'planet_resources_released=1 engine_handle_dropped=1 before_world_close=1', 'world_closed=1'):
        require(marker in log, 'missing normal lifecycle: '+marker)
    require(log.index('planet_resources_released=1') < log.index('world_closed=1'), 'planet retired after engine close')
    require(log.count('planet_root hemisphere=') == 2, 'root textures must upload only once')


def main():
    executable, content, output = [Path(x).resolve() for x in sys.argv[1:4]]
    output.mkdir(parents=True, exist_ok=True)
    launch = stage(content, output)
    # Record real native inputs, not a full scientific ephemeris validation.
    provenance = {}
    for rel in ('Modules/Celbody/Sun.dylib', 'Modules/Celbody/Earth.dylib', 'Modules/Vsop87.dylib',
                'Config/Earth/Data/Vsop87B.dat'):
        p = launch/rel
        data = p.read_bytes()
        provenance[rel] = dict(path=str(p.resolve()), bytes=len(data), sha256=hashlib.sha256(data).hexdigest())
    results = dict(provenance=provenance, runs=[], negatives=[])
    def run(name, controlled, negative=None):
        snapshot = output/(name+'.ppm')
        cmd = [str(executable), 'MetalEarth', '12', '--planet-surface-test' if controlled else '--planet-surface', str(snapshot)]
        if negative:
            cmd.append(negative)
        p = subprocess.run(cmd, cwd=launch, capture_output=True, text=True, timeout=90)
        log = p.stdout+p.stderr
        (output/(name+'.log')).write_text(log)
        require(p.returncode == 0, name+' renderer failed: '+log[-2500:])
        engine_log = (launch/'Orbiter_core_metal.log').read_text()
        (output/(name+'.engine.log')).write_text(engine_log)
        for theory, body in (('E', 'Sun'), ('B', 'Earth')):
            initialized = re.search(r'VSOP87\('+theory+r'\) '+body+r': Precision \S+, Terms (\d+)/(\d+)', engine_log)
            require(initialized is not None and int(initialized[1]) > 0, 'native '+body+' theory not initialized')
        lifecycle(log)
        path = re.search(r'planet_source name=Earth path=(.*?) radius=', log)
        require(path is not None, 'engine configured planet path missing')
        archive = Path(path[1])/'Archive/Surf.tree'
        if not archive.is_absolute():
            archive = launch/archive
        textures, identities = roots(archive)
        results['roots'] = identities
        results['archive'] = str(archive.resolve())
        results.setdefault('commands', []).append(dict(argv=cmd, cwd=str(launch), exit=p.returncode))
        return log, snapshot, textures
    for controlled in (False, True):
        for repeat in range(3):
            name = ('controlled' if controlled else 'scenario')+'-'+str(repeat)
            log, snapshot, textures = run(name, controlled)
            result = verify_run(log, snapshot, textures, controlled)
            results['runs'].append(dict(name=name, **result))
            print(json.dumps(dict(name=name, **result)), flush=True)
    for name, option in (('freeze', '--diagnostic-freeze-planet-transform'),
                         ('sampling', '--diagnostic-ignore-planet-sampling'),
                         ('hemispheres', '--diagnostic-swap-planet-hemispheres')):
        log, snapshot, textures = run('negative-'+name, True, option)
        frames = [json.loads(s.split('planet_json=', 1)[1]) for s in log.splitlines() if 'planet_json=' in s]
        require(max(abs(a-b) for a, b in zip(frames[0]['cameraR'], frames[6]['cameraR'])) > .1, 'negative must keep API change')
        if name == 'freeze':
            require(all(f['bound'] == frames[0]['bound'] for f in frames), 'negative bound transform not frozen')
        try:
            verify_run(log, snapshot, textures, True)
        except RuntimeError as e:
            require('texture/pose mismatch' in str(e) or 'GPU pose image unchanged' in str(e) or 'sphere geometry mismatch' in str(e), 'control failed for wrong reason')
            # A standalone invocation of exactly the same oracle returns nonzero;
            # renderer processes must still have completed normal engine cleanup.
            child = subprocess.run([sys.executable, str(Path(__file__).resolve()), '--oracle',
                                    str(output/('negative-'+name+'.log')), str(snapshot), results['archive']], capture_output=True, text=True, timeout=90)
            (output/('negative-'+name+'.oracle.log')).write_text(child.stdout+child.stderr)
            require(child.returncode != 0 and 'mismatch' in child.stderr, 'standalone positive oracle failed to reject control')
            result = dict(name=name, rejected=True, oracle_exit=child.returncode, renderer_exit=0, reason=str(e))
            results['negatives'].append(result)
            print(json.dumps(result), flush=True)
        else:
            raise AssertionError('causal negative accepted: '+name)
    (output/'results.json').write_text(json.dumps(results, indent=2)+'\n')
    print('PASS: six hidden actual-engine Earth lifecycles; independent sphere/mesh/Pillow GPU oracle; all three causal negatives rejected with normal engine teardown')


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--oracle':
        logfile, snapshot, archive = map(Path, sys.argv[2:5])
        log = logfile.read_text()
        textures, _ = roots(archive)
        verify_run(log, snapshot, textures, True)
    else:
        main()
