#!/usr/bin/env python3
"""CPU codec differential tests, NOT renderer evidence. Requires installed Pillow.

Compare real --asset files and 300 seeded synthetic codec fixtures with Pillow's
independent DDS/BC decoder. Persist per-input SHA256 and exact RGBA comparison.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import random
import struct
import subprocess
from PIL import Image, __version__ as pillow_version


def sha(data):
    return hashlib.sha256(data).hexdigest()


def synthetic(fmt, rng, index):
    header = bytearray(128)
    for offset, value in [(0, 0x20534444), (4, 124), (8, 0x81007),
                          (12, 4), (16, 4), (76, 32), (80, 4), (108, 0x1000)]:
        struct.pack_into('<I', header, offset, value)
    header[84:88] = fmt.encode('ascii')
    block = bytearray(rng.getrandbits(8) for _ in range(8 if fmt == 'DXT1' else 16))
    if fmt == 'DXT5':
        # Guaranteed >, < and = branch coverage, selectors 0..7 twice.
        block[0:2] = [(240, 16), (10, 210), (50, 50)][index % 3]
        block[2:8] = sum((i % 8) << (3*i) for i in range(16)).to_bytes(6, 'little')
    return bytes(header + block)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--decoder', required=True, type=Path)
    parser.add_argument('--output-dir', required=True, type=Path)
    parser.add_argument('--asset', action='append', default=[], type=Path)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    rng = random.Random(0xDD5123)
    sources = [(str(p.resolve()), 'actual_asset', p.read_bytes()) for p in sorted(set(args.asset))]
    for fmt in ['DXT1', 'DXT3', 'DXT5']:
        for index in range(100):
            sources.append((f'synthetic-codec-unit-{fmt}-{index:03}', 'synthetic_codec_unit_test', synthetic(fmt, rng, index)))
    rows = []
    for index, (name, kind, data) in enumerate(sources):
        source = args.output_dir / f'input-{index:03}.dds'
        output = args.output_dir / f'output-{index:03}.rgba'
        source.write_bytes(data)
        run = subprocess.run([str(args.decoder.resolve()), str(source), str(output)], capture_output=True, text=True)
        row: dict = dict(source=name, kind=kind, file_sha256=sha(data), fourcc=data[84:88].decode('ascii'))
        if run.returncode:
            row.update(equal=False, error=run.stderr, exit_code=run.returncode)
        else:
            with Image.open(io.BytesIO(data)) as image:
                dims = image.size
                reference = image.convert('RGBA').tobytes()
            actual = output.read_bytes()
            equal = actual == reference
            row.update(dimensions=dims, dimensions_match=run.stdout.strip() == f'{dims[0]} {dims[1]}',
                       equal=equal, decoded_bytes=len(actual), rgba_sha256=sha(actual), pillow_rgba_sha256=sha(reference))
            if not equal:
                diffs = [abs(a-b) for a, b in zip(actual, reference)]
                row.update(differing_channels=sum(d != 0 for d in diffs), max_channel_delta=max(diffs, default=0))
        rows.append(row)
    report = dict(note='CPU codec differential tests only; no engine or GPU success claim.',
                  pillow_version=pillow_version, actual_asset_count=sum(r['kind']=='actual_asset' for r in rows),
                  synthetic_count=sum(r['kind']=='synthetic_codec_unit_test' for r in rows),
                  exact_match_count=sum(r['equal'] and r.get('dimensions_match', False) for r in rows), rows=rows)
    (args.output_dir / 'comparison.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!='rows'}, indent=2))
    return 0 if report['exact_match_count'] == len(rows) else 1


if __name__ == '__main__':
    raise SystemExit(main())
