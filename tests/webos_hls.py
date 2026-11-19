"""Check the real loopback server and the muxed packets without a TV."""
import array
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
def playback(filename, dv7, donor=None, buffer_mib=None, offset=0, speed=0, track=1, ordinal=None):
    cmd = [work / 'server', urljoin(origin, filename), str(dv7)]
    if donor:
        cmd += [urljoin(origin, donor), str(buffer_mib or 0), str(track), str(offset), str(speed)]
    elif ordinal is not None:
        cmd += [str(buffer_mib or 0), str(ordinal)]
    elif buffer_mib:
        cmd += [str(buffer_mib)]
    process = subprocess.Popen(
        cmd,
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
    master = get(base)
    assert b'v.m3u8' in master and re.search(rb'CODECS="avc1.[0-9a-f]{6},ac-3"', master)
    assert b'#EXT-X-INDEPENDENT-SEGMENTS' not in master
    playlist = get(base, 'v.m3u8')
    assert b'\nseg0.m4s\n' in playlist and b'\nseg2.m4s\n' in playlist
    assert b'#EXT-X-INDEPENDENT-SEGMENTS' not in playlist
    streams = probe('dts.mp4', get(base, 'init.mp4') + get(base, 'seg2.m4s'))['streams']
    assert [s['codec_name'] for s in streams] == ['h264', 'ac3']
    assert streams[1]['channels'] == 6
    print('DTS HLS fMP4, AC-3 5.1 and out-of-order seek passed.')

with playback('long-tail.mkv', 0) as base:
    playlist = get(base, 'v.m3u8')
    assert b'#EXTINF:79.' in playlist and b'\nseg1.m4s\n' in playlist, playlist
    init = get(base, 'init.mp4')
    assert boxes(init) == [b'ftyp', b'moov']
    tail = get(base, 'seg1.m4s')
    fragments = boxes(tail)
    assert len(tail) > 48 * 1048576 and len(fragments) >= 4, (len(tail), fragments)
    assert fragments == [b'moof', b'mdat'] * (len(fragments) // 2), fragments
    assert [s['codec_name'] for s in probe('long-tail.mp4', init + tail)['streams']] == ['h264', 'ac3']
assert 'MKV tail: verified, stop=demux-eof' in (work / 'long-tail.mkv.log').read_text()
print('A long final GOP starts and streams several bounded fMP4 fragments.')

with playback('overstated-duration.mkv', 0) as base:
    playlist = get(base, 'v.m3u8')
    assert b'#EXT-X-ENDLIST' in playlist, playlist
    lengths = re.findall(rb'^#EXTINF:([\d.]+),', playlist, re.M)
    assert 82.0 <= sum(map(float, lengths)) < 82.1, playlist
assert 'MKV tail: verified, stop=demux-eof' in (work / 'overstated-duration.mkv.log').read_text()
print('An overstated MKV duration is corrected using the last packets and track metadata.')

with playback('short-source.mkv', 0) as base:
    try:
        get(base)
    except HTTPError as error:
        assert error.code == 503
    else:
        raise AssertionError('A tail ending before the declared duration must be refused')
assert 'MKV tail: unverified, stop=demux-eof' in (work / 'short-source.mkv.log').read_text()


with playback('indexed.mkv', 1) as base:
    master = get(base)
    assert re.search(rb'CODECS="hvc1\.[ABC]?\d+\.[0-9A-F]+\.[LH]\d+(?:\.[0-9A-F]{2})*,ac-3"', master)
    assert b'VIDEO-RANGE=PQ' in master and b'SUPPLEMENTAL-CODECS="dvh1.08.06/db1p"' in master
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
        assert video['codec_tag_string'] == 'hvc1'
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
log = (work / 'indexed.mkv.log').read_text()
assert 'input audio ordinal=0 stream=1 selected=0 codec=truehd' in log
assert 'input audio ordinal=1 stream=2 selected=1 codec=ac3' in log

with playback('plain.mkv', 1) as base:
    try:
        get(base)
    except HTTPError as error:
        assert error.code == 503
    else:
        raise AssertionError('A source without P7 must be refused by the P7 HLS path')


def check_input_buffer(log, mib, readers):
    match = re.search(r'input buffer: (\d+) / (\d+) bytes; (\d+) readers; (\d+) bytes received', log)
    assert match, log
    ready, capacity, count, received = map(int, match.groups())
    assert capacity == mib * 1048576 and count == readers, match.group()
    assert 0 <= ready <= capacity and received > 0, match.group()


def decoded_audio(name, data):
    media = work / name
    media.write_bytes(data)
    raw = subprocess.check_output(['ffmpeg', '-v', 'error', '-i', media, '-map', '0:a:0',
                                   '-ac', '1', '-ar', '8000', '-f', 's16le', '-'])
    samples = array.array('h', raw)
    if sys.byteorder != 'little':
        samples.byteswap()
    return samples


def tone(samples, start, duration=0.25):
    section = samples[round(start * 8000):round((start + duration) * 8000)]
    assert len(section) == round(duration * 8000)
    return sum(a < 0 <= b for a, b in zip(section, section[1:])) / duration


with playback('indexed.mkv', 1, ordinal=0) as base:
    changed = get(base, 'init.mp4') + get(base, 'seg2.m4s')
    video, audio = probe('alternate-p8.mp4', changed)['streams']
    assert any(sd.get('dv_profile') == 8 for sd in video['side_data_list'])
    assert audio['codec_name'] == 'ac3' and audio['channels'] == 6
    assert 530 < tone(decoded_audio('alternate-p8.mp4', changed), 0.5) < 570
log = (work / 'indexed.mkv.log').read_text()
assert 'input audio ordinal=0 stream=1 selected=1 codec=truehd' in log
print('The alternate P7 audio track plays as AC-3 at a nonzero segment with Dolby Vision P8.1.')

with playback('dts.mkv', 0, ordinal=1) as base:
    changed = get(base, 'init.mp4') + get(base, 'seg2.m4s')
    assert probe('alternate-sdr.mp4', changed)['streams'][1]['codec_name'] == 'ac3'
    assert 640 < tone(decoded_audio('alternate-sdr.mp4', changed), 0.5) < 680
log = (work / 'dts.mkv.log').read_text()
assert 'input audio ordinal=0 stream=1 selected=0 codec=dts' in log
assert 'input audio ordinal=1 stream=2 selected=1 codec=aac language=pol' in log

with playback('dts.mkv', 0, 'donor.m4a', 64, 750) as base:
    init = get(base, 'init.mp4')
    first = init + get(base, 'seg0.m4s')
    assert probe('external-language.mp4', first)['streams'][1]['tags']['language'] == 'pol'
    start = decoded_audio('external-0.mp4', first)
    assert max(map(abs, start[:4000])) < 150, 'positive offset should start with silence'
    assert 860 < tone(start, 1) < 900, 'expected the selected 880 Hz donor track'
    middle = decoded_audio('external-2.mp4', init + get(base, 'seg2.m4s'))
    assert 860 < tone(middle, 0.5) < 900, 'expected donor audio after seek'
    assert 6.8 < sum(map(float, re.findall(rb'#EXTINF:([0-9.]+)', get(base, 'v.m3u8')))) < 7.2
log = (work / 'dts.mkv.log').read_text()
assert 'input rings: video=48 MiB external=16 MiB, external track=1 offset=750 ms' in log
assert re.search(r'reader rings peak 2 = 64 MiB', log)
check_input_buffer(log, 48 + 8, 2)  # sparse donor uses an 8 MiB cache
assert 'input audio ordinal=0 stream=0 selected=0 codec=aac language=eng' in log
assert 'input audio ordinal=1 stream=1 selected=1 codec=aac language=pol' in log

with playback('indexed.mkv', 1, 'donor.m4a', 128, -150, 40000) as base:
    init = get(base, 'init.mp4')
    middle = get(base, 'seg1.m4s')
    info = probe('external-p8.mp4', init + middle)['streams']
    assert info[0]['codec_tag_string'] == 'hvc1' and info[1]['codec_name'] == 'ac3'
    assert 825 < tone(decoded_audio('external-p8.mp4', init + middle), 0.5) < 865
log = (work / 'indexed.mkv.log').read_text()
assert 'input rings: video=112 MiB external=16 MiB, external track=1 offset=-150 ms' in log
assert re.search(r'reader rings peak 2 = 128 MiB', log)
check_input_buffer(log, 112 + 8, 2)
with playback('plain.mkv', 0, 'donor.mkv', 256, 3500, track=2) as base:
    init = get(base, 'init.mp4')
    silence = decoded_audio('external-silent.mp4', init + get(base, 'seg0.m4s'))
    assert max(map(abs, silence)) < 150
    later = decoded_audio('external-late.mp4', init + get(base, 'seg2.m4s'))
    assert 860 < tone(later, 0.5) < 900
log = (work / 'plain.mkv.log').read_text()
assert 'input rings: video=240 MiB external=16 MiB, external track=2 offset=3500 ms' in log
assert re.search(r'reader rings peak 2 = 256 MiB', log)
check_input_buffer(log, 256, 2)
assert 'input audio ordinal=1 stream=2 selected=1 codec=aac language=pol title=Polski lektor' in log

with playback('dts.mkv', 0, buffer_mib=256) as base:
    assert get(base, 'seg0.m4s')
log = (work / 'dts.mkv.log').read_text()
assert 'input ring: video=256 MiB' in log
assert re.search(r'reader rings peak 1 = 256 MiB', log)
check_input_buffer(log, 256, 1)
print('External AAC -> AC-3 track, offsets, speed, out-of-order seek and 64/128/256 MiB presets passed.')
