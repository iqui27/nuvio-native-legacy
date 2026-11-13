"""Small real dual-track ISO-BMFF files, with synthetic HEVC and MIT RPU data.

The EL pictures are separately encoded; only their UNSPEC62 metadata may be
attached to the BL. Includes reordered PTS, unrelated MP4 and broken pairing.
"""
import json
import pathlib
import struct
import sys

root = pathlib.Path(sys.argv[1])
data = json.loads((root / 'split.json').read_text())


def unhex(s):
    return bytes.fromhex(''.join(line.split(':', 1)[1].split('  ')[0].replace(' ', '')
                                 for line in s.strip().splitlines()))


def box(name, payload):
    return struct.pack('>I4s', len(payload) + 8, name.encode()) + payload


def full(name, payload, version=0, flags=0):
    return box(name, bytes([version]) + flags.to_bytes(3, 'big') + payload)


def ints(*values):
    return struct.pack('>' + 'I' * len(values), *values)


matrix = ints(0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x40000000)
ftyp = box('ftyp', b'isom' + ints(0x200) + b'isomiso6mp41dby1')
packets = [[p for p in data['packets'] if p['stream_index'] == i] for i in range(4)]
assert len(packets[0]) == len(packets[1]) == 20
assert [p['pts_time'] for p in packets[0]] == [p['pts_time'] for p in packets[1]]


def dac3(raw):
    assert raw[:2] == b'\x0b\x77'
    bits = ''.join(f'{v:08b}' for v in raw)
    pos = 32

    def read(n):
        nonlocal pos
        value = int(bits[pos:pos+n], 2)
        pos += n
        return value
    fs, size, bsid, mode, acmod = read(2), read(6), read(5), read(3), read(3)
    if acmod & 1 and acmod != 1:
        read(2)
    if acmod & 4:
        read(2)
    if acmod == 2:
        read(2)
    lfe = read(1)
    return ((fs << 22) | (bsid << 17) | (mode << 14) | (acmod << 11)
            | (lfe << 10) | ((size // 2) << 5)).to_bytes(3, 'big')


def write(kind, variant, reverse=False):
    single = variant.startswith('p8') or variant == 'p5'
    zero = variant.startswith('p8-zero') or variant == 'p8-unknown'
    inband = variant.startswith('inband') or variant in ['p81-inband-nocolr', 'p8-zero-inband', 'p8-zero-nosps', 'p8-zero-sdr', 'p8-zero-hlg']
    nosps = variant in ['inband-nosps', 'p8-zero-nosps']
    nocolr = variant in ['inband-nocolr', 'hvcc-nocolr', 'inband-nosps', 'p81-inband-nocolr', 'p8-zero-inband', 'p8-zero-nosps', 'p8-zero-sdr', 'p8-zero-hlg']
    source = data
    if variant in ['p8-zero-sdr', 'p8-zero-colr-sdr', 'p8-zero-hlg', 'p8-zero-colr-hlg']:
        colour = 'hlg' if variant.endswith('hlg') else 'sdr'
        source = json.loads((root / f'split-{colour}.json').read_text())
    samples_by_track = [[p for p in source['packets'] if p['stream_index'] == i] for i in range(4)]
    rpu = bytearray((root / (kind + ('-output.nal' if single and variant not in ['p81-p7-rpu', 'p8-zero-p7-rpu'] else '-input.nal'))).read_bytes())
    if variant in ['bad-rpu', 'p81-bad-rpu', 'p8-zero-bad-rpu']:
        rpu[-2] ^= 0x5a
    tracks = []
    for i, stream in enumerate(source['streams']):
        samples = []
        for number, p in enumerate(samples_by_track[i]):
            raw = unhex(p['data'])
            if i < 2 and inband and 'K' in p['flags']:
                # The user's MP4 has a 23-byte hvcC (no parameter-set arrays).
                # Carry the actual VPS/SPS/PPS in each random-access sample.
                hvcc = unhex(stream['extradata'])
                pos, params = 23, b''
                for _ in range(hvcc[22]):
                    count = int.from_bytes(hvcc[pos+1:pos+3], 'big')
                    pos += 3
                    for _ in range(count):
                        size = int.from_bytes(hvcc[pos:pos+2], 'big')
                        pos += 2
                        if not nosps or (hvcc[pos] >> 1) & 63 != 33:
                            params += size.to_bytes(4, 'big') + hvcc[pos:pos+size]
                        pos += size
                raw = params + raw
            if (i == 0 if single else i == 1) and not (variant == 'missing-rpu' and number == 5) and variant != 'p8-zero-no-rpu':
                raw += len(rpu).to_bytes(4, 'big') + rpu
            samples.append(raw)
        tracks.append({'stream': stream, 'samples': samples, 'offsets': [0] * len(samples)})
    order = [0, 2, 3] if single and variant not in ['p81-extra-video', 'p8-zero-extra-video'] else ([1, 0, 2, 3] if reverse else [0, 1, 2, 3])

    def trak(i):
        t = tracks[i]
        s = t['stream']
        video = i < 2
        count = len(t['samples'])
        rate, duration = (1000, 40) if video else (48000, 1536)
        width, height = (s['width'], s['height']) if video else (0, 0)
        tkhd = full('tkhd', ints(0, 0, i+1, 0, round(count*duration*1000/rate))
                    + bytes(8) + struct.pack('>hhhh', 0, 0, 0 if video else 256, 0)
                    + matrix + ints(width << 16, height << 16), flags=3)
        mdhd = full('mdhd', ints(0, 0, rate, count*duration) + struct.pack('>HH', 0x55c4, 0))
        hdlr = full('hdlr', ints(0) + (b'vide' if video else b'soun') + bytes(12) + b'fixture\0')
        if video:
            tag = 'hev1' if i == 0 else 'dvhe'
            entry = bytes(6) + struct.pack('>H', 1) + bytes(16)
            entry += struct.pack('>HH', width, height) + ints(0x480000, 0x480000, 0)
            entry += struct.pack('>H', 1) + bytes(32) + struct.pack('>Hh', 24, -1)
            hvcc = unhex(s['extradata'])
            if inband:
                hvcc = hvcc[:22] + b'\0'
            entry += box('hvcC', hvcc)
            if not nocolr:
                colour = (1, 1, 1, 0) if variant in ['sdr-colr', 'p81-sdr'] else (9, 16, 9, 0)
                if variant == 'inband-conflict':
                    colour = (1, 2, 9, 0)
                entry += box('colr', b'nclx' + struct.pack('>HHHB', *colour))
            if (i == 0 if single else i == 1):
                profile = 5 if variant == 'p5' else (8 if single else 7)
                compat = 4 if variant == 'p84' else (2 if variant == 'p8-compat2' else (0 if zero else (1 if single else 6)))
                flags = 6 if not single else (7 if variant in ['p81-with-el', 'p8-zero-with-el'] else (4 if variant in ['p81-no-bl', 'p8-zero-no-bl'] else 5))
                entry += box('dvcC', bytes([1, 0, profile << 1, (6 << 3) | flags, compat << 4]) + bytes(19))
            entry = box(tag, entry)
            media_header = full('vmhd', bytes(8), flags=1)
        else:
            entry = bytes(6) + struct.pack('>H', 1) + bytes(8)
            entry += struct.pack('>HHHHI', s['channels'], 16, 0, 0, rate << 16)
            entry = box('ac-3', entry + box('dac3', dac3(t['samples'][0])))
            media_header = full('smhd', bytes(4))
        stbl = full('stsd', ints(1) + entry)
        stbl += full('stts', ints(1, count, duration))
        if video:
            offsets = [round(float(p['pts_time'])*1000) - n*40 for n, p in enumerate(samples_by_track[i])]
            if variant == 'mismatch' and i == 1:
                offsets = [v+1 for v in offsets]
            stbl += full('ctts', ints(count) + b''.join(ints(1) + struct.pack('>i', v) for v in offsets), version=1)
            key = [n+1 for n, p in enumerate(samples_by_track[i]) if 'K' in p['flags']]
            if variant == 'seek-preroll' and i == 1:
                key = key[:1]
            stbl += full('stss', ints(len(key), *key))
        stbl += full('stsc', ints(1, 1, 1, 1))
        stbl += full('stsz', ints(0, count, *(len(p) for p in t['samples'])))
        stbl += full('stco', ints(count, *t['offsets']))
        dinf = box('dinf', full('dref', ints(1) + full('url ', b'', flags=1)))
        minf = box('minf', media_header + dinf + box('stbl', stbl))
        tref = box('tref', box('vdep', ints(1))) if i == 1 else b''
        return box('trak', tkhd + tref + box('mdia', mdhd + hdlr + minf))

    def moov():
        mvhd = full('mvhd', ints(0, 0, 1000, len(samples_by_track[0])*40, 0x10000)
                    + struct.pack('>HH', 256, 0) + bytes(8) + matrix + bytes(24) + ints(5))
        return box('moov', mvhd + b''.join(trak(i) for i in order))
    entries = sorted((n*(.04 if i < 2 else .032), order.index(i), i, n)
                     for i in order for n in range(len(tracks[i]['samples'])))
    offset = len(ftyp) + len(moov()) + 8
    body = bytearray()
    for _, _, i, n in entries:
        t = tracks[i]
        t['offsets'][n] = offset + len(body)
        body += t['samples'][n]
    (root / f'{kind}-{variant}{"-reverse" if reverse else ""}.mp4').write_bytes(ftyp + moov() + box('mdat', body))


for kind in ['mel', 'fel']:
    for variant in ['dual', 'bad-rpu', 'missing-rpu', 'mismatch', 'inband',
                    'inband-nocolr', 'hvcc-nocolr', 'inband-nosps', 'sdr-colr', 'inband-conflict']:
        write(kind, variant)
    write(kind, 'dual', reverse=True)
print('Generated MEL/FEL dual-track MP4: B-frames, reversed track order, bad/missing RPU and timestamp mismatch')

for kind in ['mel', 'fel']:
    for variant in ['p81','p81-inband-nocolr','p84','p8-unknown','p81-extra-video','p81-with-el','p81-no-bl','p81-bad-rpu','p81-p7-rpu','p81-sdr','p5']:
        write(kind, variant)
    for variant in ['p8-zero-inband','p8-zero-nosps','p8-zero-sdr','p8-zero-hlg',
                    'p8-zero-colr-sdr','p8-zero-colr-hlg','p8-zero-bad-rpu','p8-zero-p7-rpu',
                    'p8-zero-no-rpu','p8-zero-extra-video','p8-zero-with-el','p8-zero-no-bl','p8-compat2']:
        write(kind, variant)
if (root / 'split-long.json').exists():
    data = json.loads((root / 'split-long.json').read_text())
    packets = [[p for p in data['packets'] if p['stream_index'] == i] for i in range(4)]
    assert len(packets[0]) == len(packets[1]) == 200
    assert [p['pts_time'] for p in packets[0]] == [p['pts_time'] for p in packets[1]]
    for kind in ['mel','fel']:
        write(kind, 'seek-preroll')
