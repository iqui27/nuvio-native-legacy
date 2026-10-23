"""PNG-only visual guard for untouched season controls above the episode row."""
from pathlib import Path
import struct
import zlib


def pixels(path):
    data = path.read_bytes()
    offset, compressed = 8, bytearray()
    while offset < len(data):
        length, kind = struct.unpack_from('>I4s', data, offset)
        body = data[offset+8:offset+8+length]
        if kind == b'IHDR':
            w, h, depth, color, _, _, interlace = struct.unpack('>IIBBBBB', body)
            assert depth == 8 and color in (2, 6) and interlace == 0
        elif kind == b'IDAT':
            compressed.extend(body)
        offset += 12 + length
    channels = 4 if color == 6 else 3
    stride = w * channels
    raw, result, previous = zlib.decompress(compressed), [], bytearray(stride)
    for y in range(h):
        start = y * (stride+1)
        filtering = raw[start]
        row = bytearray(raw[start+1:start+1+stride])
        for i in range(stride):
            left = row[i-channels] if i >= channels else 0
            up = previous[i]
            corner = previous[i-channels] if i >= channels else 0
            if filtering == 1:
                value = left
            elif filtering == 2:
                value = up
            elif filtering == 3:
                value = (left+up)//2
            elif filtering == 4:
                p = left+up-corner
                distances = (abs(p-left),abs(p-up),abs(p-corner))
                value = (left,up,corner)[distances.index(min(distances))]
            else:
                assert filtering == 0
                value = 0
            row[i] = (row[i]+value)&255
        result.append(row)
        previous = row
    return w, h, channels, result


root = Path('/Volumes/ExternalSSD/tmp/rev/epinfo')
for layout in ('modern', 'refined'):
    for height in (1080, 720):
        for lighting in ('dark', 'light'):
            suffix = f'{layout}-{height}p-{lighting}.png'
            w,h,c,before = pixels(root / ('antes-'+suffix))
            aw,ah,ac,after = pixels(root / ('depois-'+suffix))
            assert (w,h,c) == (aw,ah,ac) and (w,h) == (height*16//9,height)
            # Hero text fades use live ticks; season controls are fully settled.
            differences = sum(abs(a-b)>2 for row in range(260*height//1080,400*height//1080)
                              for a,b in zip(before[row],after[row]))
            assert differences == 0, (suffix, differences)
            print('PASS: unchanged season pixels:', suffix)
