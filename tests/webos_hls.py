"""Check the real loopback server and the muxed packets without a TV."""
import contextlib
import json
import pathlib
import re
import subprocess
import sys
from urllib.error import HTTPError
from urllib.parse import urljoin
from urllib.request import urlopen

work = pathlib.Path(sys.argv[1])
origin = f"http://127.0.0.1:{(work / 'port').read_text()}/"


@contextlib.contextmanager
def playback(filename, dv7):
    process = subprocess.Popen(
        [work / 'server', urljoin(origin, filename), str(dv7)],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True,
    )
    try:
        for line in process.stdout:
            if line.startswith('playlist '):
                yield line.split()[1]
                break
        else:
            raise AssertionError('Server did not start')
    finally:
        process.stdin.close()
        output = process.stdout.read()
        assert process.wait(timeout=10) == 0, output
        (work / f'{filename}.log').write_text(output)
        if sys.exc_info()[0] is not None:
            print(output)


def get(base, path=''):
    with urlopen(urljoin(base, path), timeout=35) as response:
        return response.read()


def probe(name, data, packets=False):
    media = work / name
    media.write_bytes(data)
    command = ['ffprobe', '-v', 'error', '-show_streams', '-of', 'json']
    if packets:
        command += ['-show_packets', '-show_data']
    return json.loads(subprocess.check_output(command + [media]))


def boxes(data):
    types = []
    while data:
        length = int.from_bytes(data[:4], 'big')
        assert 8 <= length <= len(data)
        types.append(data[4:8])
        data = data[length:]
    return types


def hevc_nals(packet):
    raw = bytes.fromhex(''.join(line.split(':', 1)[1].split('  ')[0]
                                for line in packet['data'].strip().splitlines()))
    while raw:
        length = int.from_bytes(raw[:4], 'big')
        assert 2 <= length <= len(raw) - 4
        yield raw[4:4 + length]
        raw = raw[4 + length:]


with playback('dts.mkv', 0) as base:
    assert b'v.m3u8' in get(base)
    playlist = get(base, 'v.m3u8')
    assert b'\nseg0.ts\n' in playlist and b'\nseg2.ts\n' in playlist
    streams = probe('dts.ts', get(base, 'seg2.ts'))['streams']
    assert [s['codec_name'] for s in streams] == ['h264', 'ac3']
    assert streams[1]['channels'] == 6
    print('DTS HLS MPEG-TS, AC-3 5.1 and out-of-order seek passed.')

with playback('indexed.mkv', 1) as base:
    assert b'CODECS="dvh1.08.06,ac-3"' in get(base)
    playlist = get(base, 'v.m3u8')
    assert b'#EXT-X-MAP:URI="init.mp4"' in playlist
    seg_names = re.findall(rb'^seg\d+\.m4s$', playlist, re.M)
    assert len(seg_names) >= 3, playlist
    init = get(base, 'init.mp4')
    assert boxes(init) == [b'ftyp', b'moov'], boxes(init)
    segments = {}
    for i in (2, 0, 1, 2):
        seg = get(base, seg_names[i].decode())
        assert boxes(seg) == [b'moof', b'mdat'], boxes(seg)
        info = probe(f'dv7-{i}.mp4', init + seg, packets=True)
        video, audio = info['streams']
        assert video['codec_tag_string'] == 'dvh1'
        dv = next(s for s in video['side_data_list'] if s.get('dv_profile') == 8)
        assert dv['dv_bl_signal_compatibility_id'] == 1 and dv['el_present_flag'] == 0
        assert audio['codec_name'] == 'ac3' and audio['channels'] == 6
        packets = [p for p in info['packets'] if p['codec_type'] == 'video']
        nals = list(hevc_nals(packets[0]))
        assert all((nal[0] >> 1) & 63 != 63 for nal in nals)
        assert (work / 'fel-output.nal').read_bytes() in nals
        dts = float(packets[0]['dts_time'])
        if i in segments:
            assert abs(segments[i] - dts) < 0.05, (segments[i], dts)
        segments[i] = dts
    assert 1.9 < segments[1] - segments[0] < 2.1, segments
    assert 3.9 < segments[2] - segments[0] < 4.1, segments
    print('DV7 HLS fMP4, converted P8.1 RPU, AC-3 5.1 and segment timestamps passed.')

with playback('plain.mkv', 1) as base:
    try:
        get(base)
    except HTTPError as error:
        assert error.code == 503
    else:
        raise AssertionError('A source without P7 must be refused by the P7 HLS path')
