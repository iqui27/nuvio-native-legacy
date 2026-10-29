"""Make an MKV engine fixture from synthetic HEVC/E-AC-3 + MIT RPU data.

Input: ffprobe -show_streams -show_packets -show_data -of json plain.mkv
The EL NAL is synthetic and must be discarded; this is not a picture-quality
or physical-TV test.
"""
import json
import pathlib
import struct
import sys

root = pathlib.Path(sys.argv[1])
source = json.loads((root / 'plain.json').read_text())


def unhex(s):
    return bytes.fromhex(''.join(line.split(':', 1)[1].split('  ')[0].replace(' ', '')
                                 for line in s.strip().splitlines()))


def elem(tag, data):
    length = len(data)
    for n in range(1, 9):
        if length < (1 << (7*n))-1:
            return bytes.fromhex(tag) + ((1 << (7*n)) | length).to_bytes(n, 'big') + data
    raise ValueError(length)


def uint(tag, value):
    return elem(tag, value.to_bytes(max(1, (value.bit_length()+7)//8), 'big'))


header = elem('1a45dfa3', uint('4286', 1) + uint('42f7', 1) + uint('42f2', 4)
              + uint('42f3', 8) + elem('4282', b'matroska') + uint('4287', 4) + uint('4285', 2))
info = elem('1549a966', uint('2ad7b1', 1000000) + elem('4489', struct.pack('>d', 832.0)))
hvcc = unhex(source['streams'][0]['extradata'])
mapping = elem('41e4', uint('41f0', 1) + uint('41e7', int.from_bytes(b'dvcC', 'big'))
               + elem('41ed', bytes([1, 0, 7 << 1, (6 << 3) | 7, 6 << 4]) + bytes(19)))
video = elem('ae', uint('d7', 1) + uint('73c5', 1) + uint('83', 1)
             + elem('86', b'V_MPEGH/ISO/HEVC') + elem('63a2', hvcc) + uint('23e383', 40000000)
             + elem('e0', uint('b0', 64) + uint('ba', 64)) + mapping)
audio = elem('ae', uint('d7', 2) + uint('73c5', 2) + uint('83', 2) + elem('86', b'A_EAC3')
             + elem('e1', elem('b5', struct.pack('>d', 48000.0)) + uint('9f', 2)))
tracks = elem('1654ae6b', video + audio)
for kind in ['mel', 'fel']:
    rpu = (root / (kind+'-input.nal')).read_bytes()
    for corrupt in [False, True]:
        blocks = b''
        for packet in source['packets']:
            index = packet['stream_index']
            data = unhex(packet['data'])
            if index == 0:
                nal = bytearray(rpu)
                if corrupt:
                    nal[-2] ^= 0x5a
                el = bytes([0x7e, 1, 0xab, 0xcd])
                data += len(el).to_bytes(4, 'big') + el + len(nal).to_bytes(4, 'big') + nal
            ms = round(float(packet['pts_time'])*1000)
            block = bytes([0x81+index]) + struct.pack('>h', ms) + bytes([0x80]) + data
            blocks += elem('a3', block)
        cluster = elem('1f43b675', uint('e7', 0) + blocks)
        (root / (kind + ('-bad' if corrupt else '') + '.mkv')).write_bytes(
            header + elem('18538067', info + tracks + cluster))
