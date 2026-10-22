#!/usr/bin/env python3
"""Check 16KB ELF load/RELRO alignment of every ARM64/x86_64 library in an APK.

Uses only Python's standard library, including for compressed native libraries.
32-bit libraries are excluded from the Android 16KB compatibility requirement.
"""
import argparse
from pathlib import Path
import struct
import sys
import zipfile

PAGE_SIZE = 16384
MACHINES = {'arm64-v8a': 183, 'x86_64': 62}
PT_LOAD = 1
PT_GNU_RELRO = 0x6474E552


def check_elf(data, abi):
    """Return load-segment count, minimum alignment and a list of errors."""
    if len(data) < 64 or data[:4] != b'\x7fELF':
        raise ValueError('missing/truncated ELF64 header')
    if data[4:7] != bytes((2, 1, 1)):
        raise ValueError('expected a little-endian ELF64 file')
    header = struct.unpack_from('<16sHHIQQQIHHHHHH', data)
    if header[1] != 3 or header[2] != MACHINES[abi]:
        raise ValueError(f'ELF type/machine does not match {abi}')
    phoff, entsize, count = header[5], header[9], header[10]
    if header[8] < 64 or entsize < 56 or count == 0 or count == 0xFFFF:
        raise ValueError('invalid/unsupported ELF program-header table')
    if phoff < 64 or phoff + entsize * count > len(data):
        raise ValueError('truncated ELF program-header table')
    aligns, errors = [], []
    for index in range(count):
        kind, _, offset, address, _, file_size, memory_size, align = struct.unpack_from(
            '<IIQQQQQQ', data, phoff + index * entsize)
        if kind == PT_LOAD:
            aligns.append(align)
            if align < PAGE_SIZE or align & (align - 1):
                errors.append(f'LOAD[{index}] alignment={align}, expected a power of two >= {PAGE_SIZE}')
            elif offset % align != address % align:
                errors.append(f'LOAD[{index}] offset/address are not congruent modulo alignment')
            if file_size > memory_size or offset + file_size > len(data):
                errors.append(f'LOAD[{index}] invalid file/memory range')
        elif kind == PT_GNU_RELRO and (address + memory_size) % PAGE_SIZE:
            errors.append(f'GNU_RELRO[{index}] end is not {PAGE_SIZE}-byte aligned')
    if not aligns:
        raise ValueError('ELF has no LOAD segment')
    return len(aligns), min(aligns), errors


def check_apk(path, output=sys.stdout):
    checked = failures = skipped = 0
    seen = set()
    with zipfile.ZipFile(path) as apk:
        for item in sorted(apk.infolist(), key=lambda entry: entry.filename):
            parts = item.filename.split('/')
            if len(parts) != 3 or parts[0] != 'lib' or not parts[2].endswith('.so'):
                continue
            if parts[1] not in MACHINES:
                skipped += 1
                continue
            checked += 1
            try:
                if item.filename in seen:
                    raise ValueError('duplicate library path')
                seen.add(item.filename)
                count, minimum, errors = check_elf(apk.read(item), parts[1])
                if errors:
                    failures += 1
                    print(f'UNALIGNED {item.filename}: {"; ".join(errors)}', file=output)
                else:
                    print(f'ALIGNED {item.filename}: {count} LOAD segments, min alignment={minimum}', file=output)
            except (ValueError, struct.error, zipfile.BadZipFile) as error:
                failures += 1
                print(f'INVALID {item.filename}: {error}', file=output)
    print(f'android-apk-pages: {checked} 64-bit libraries, {failures} failed, '
          f'{skipped} other-ABI libraries excluded', file=output)
    return failures == 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('apk', type=Path)
    args = parser.parse_args()
    try:
        return 0 if check_apk(args.apk) else 1
    except (OSError, zipfile.BadZipFile) as error:
        print(f'android-apk-pages: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
