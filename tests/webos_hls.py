"""Check the real loopback server and the muxed packets without a TV."""
import array
import contextlib
import json
import os
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
def playback(filename, dv7, donor=None, buffer_mib=None, offset=0, speed=0, track=1,
             ordinal=None, mapping=False, allow_dts=False, switch_to=None, inplace_to=None, ts=False):
    cmd = [work / 'server', urljoin(origin, filename), str(dv7)]
    if donor:
        cmd += [urljoin(origin, donor), str(buffer_mib or 0), str(track), str(offset), str(speed)]
    elif ordinal is not None:
        cmd += [str(buffer_mib or 0), str(ordinal)]
    elif buffer_mib:
        cmd += [str(buffer_mib)]
    env = {**os.environ, 'NV_TEST_P7_MAPPING': str(int(mapping)),
           'NV_TEST_HLS_DTS': str(int(allow_dts))}
    if switch_to is not None:
        env['NV_TEST_HLS_SWITCH_ORDINAL'] = str(switch_to)
    if inplace_to is not None:
        env['NV_TEST_HLS_INPLACE_ORDINAL'] = str(inplace_to)
    if ts:
        env['NV_TEST_HLS_TS'] = '1'
    process = subprocess.Popen(
        cmd, env=env,
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True,
    )
    earlier = []
    try:
        for line in process.stdout:
            if line.startswith('playlist '):
                base = line.split()[1]
                if switch_to is not None:
                    get(base, 'v.m3u8')
                    process.stdin.write('\n')
                    process.stdin.flush()
                    switch_to = None
                    continue
                playback.process, playback.earlier = process, earlier
                yield base
                break
            earlier.append(line)
        else:
            raise AssertionError('Server did not start')
    finally:
        process.stdin.close()
        output = ''.join(earlier) + process.stdout.read()
        assert process.wait(timeout=10) == 0, output
        (work / f'{filename}.log').write_text(output)
        if sys.exc_info()[0] is not None:
            print(output)


def switch_in_place():
    process = playback.process
    process.stdin.write('s\n')
    process.stdin.flush()
    for line in process.stdout:
        if line.startswith('switched '):
            return int(line.split()[1])
        playback.earlier.append(line)
    raise AssertionError('No in-place switch result')


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


def nal_type(nal):
    return (nal[0] >> 1) & 63


def base_layer_frames(packets):
    return [tuple(nal for nal in hevc_nals(packet)
                  if nal_type(nal) < 32 and not (nal[0] & 1 or nal[1] >> 3))
            for packet in packets if packet['codec_type'] == 'video']


def hdr10_metadata(media):
    frames = json.loads(subprocess.check_output([
        'ffprobe', '-v', 'error', '-select_streams', 'v:0', '-show_frames',
        '-show_entries', 'frame=color_transfer,color_primaries,side_data_list', '-of', 'json', media,
    ]))['frames']
    assert frames and all(frame.get('color_transfer') == 'smpte2084'
                          and frame.get('color_primaries') == 'bt2020' for frame in frames)
    names = {'Mastering display metadata', 'Content light level metadata'}
    return {sd['side_data_type']: sd for frame in frames for sd in frame.get('side_data_list', [])
            if sd['side_data_type'] in names}


def check_dv7_playlist(filename, kind, mapping):
    rpu_file = f'{kind}-mapping-output.nal' if mapping else f'{kind}-output.nal'
    expected_rpu = (work / rpu_file).read_bytes()
    source = json.loads(subprocess.check_output([
        'ffprobe', '-v', 'error', '-select_streams', 'v:0', '-show_packets', '-show_data',
        '-of', 'json', work / filename,
    ]))
    source_frames = base_layer_frames(source['packets'])
    source_hdr10 = hdr10_metadata(work / filename)
    assert 'Mastering display metadata' in source_hdr10
    assert source_hdr10['Content light level metadata']['max_content'] == 1000
    assert source_hdr10['Content light level metadata']['max_average'] == 400

    with playback(filename, 1, mapping=mapping) as base:
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
            media = f'{kind}-{int(mapping)}-{i}.mp4'
            info = probe(media, init + seg, packets=True)
            video, audio = info['streams']
            assert video['codec_tag_string'] == 'hvc1'
            dv = next(s for s in video['side_data_list'] if s.get('dv_profile') == 8)
            assert dv['dv_bl_signal_compatibility_id'] == 1
            assert dv['rpu_present_flag'] == dv['bl_present_flag'] == 1 and dv['el_present_flag'] == 0
            assert audio['codec_name'] == 'ac3' and audio['channels'] == 6
            packets = [p for p in info['packets'] if p['codec_type'] == 'video']
            assert packets
            for packet in packets:
                nals = list(hevc_nals(packet))
                assert all(nal_type(nal) != 63 and not (nal[0] & 1 or nal[1] >> 3) for nal in nals)
                assert [nal for nal in nals if nal_type(nal) == 62] == [expected_rpu]
            frames = base_layer_frames(packets)
            assert all(frames) and any(source_frames[start:start + len(frames)] == frames
                                       for start in range(len(source_frames) - len(frames) + 1))
            if i not in segments:
                assert hdr10_metadata(work / media) == source_hdr10
            dts = float(packets[0]['dts_time'])
            if i in segments:
                assert abs(segments[i] - dts) < 0.05, (segments[i], dts)
            segments[i] = dts
        assert 1.9 < segments[1] - segments[0] < 2.1, segments
        assert 3.9 < segments[2] - segments[0] < 4.1, segments
    log = (work / f'{filename}.log').read_text()
    assert 'input audio ordinal=0 stream=1 selected=0 codec=truehd' in log
    assert 'input audio ordinal=1 stream=2 selected=1 codec=ac3' in log
    print(f'P7 {kind.upper()} → P8.1 HLS, mapping {"on" if mapping else "off"}: '
          'RPU, base layer, HDR10 metadata and seek passed.')


for kind, fixture in (('fel', 'indexed.mkv'), ('mel', 'indexed-mel.mkv')):
    for mapping in (False, True):
        check_dv7_playlist(fixture, kind, mapping)

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


for ordinal, expected_hz in ((0, 440), (1, 770)):
    with playback('p7-dts-tail.mkv', 1, ordinal=ordinal, mapping=True, allow_dts=True) as base:
        playlist = get(base, 'v.m3u8')
        lengths = re.findall(rb'^#EXTINF:([\d.]+),', playlist, re.M)
        assert 6.9 < sum(map(float, lengths)) < 7.1, playlist
        init = get(base, 'init.mp4')
        middle = init + get(base, 'seg1.m4s')  # Resume after reopening with the chosen track.
        assert abs(tone(decoded_audio(f'p7-tail-audio-{ordinal}.mp4', middle), 0.5) - expected_hz) < 20
        last = re.findall(rb'^seg\d+\.m4s$', playlist, re.M)[-1].decode()
        info = probe(f'p7-tail-end-{ordinal}.mp4', init + get(base, last), packets=True)
        video, audio = info['streams']
        assert any(sd.get('dv_profile') == 8 and sd.get('dv_bl_signal_compatibility_id') == 1
                   for sd in video['side_data_list'])
        assert audio['codec_name'] == 'ac3' and audio['channels'] == 6
        ends = {kind: max(float(p['pts_time']) + float(p['duration_time'])
                          for p in info['packets'] if p['codec_type'] == kind)
                for kind in ('video', 'audio')}
        assert abs(ends['audio'] - ends['video']) < 0.08, ends
    log = (work / 'p7-dts-tail.mkv.log').read_text()
    assert 'MKV tail: verified, stop=demux-eof' in log
    if ordinal == 1:
        assert 'input audio ordinal=1 stream=2 selected=1 codec=dts' in log
print('Switching from AC-3 to longer DTS keeps P8.1 HLS seekable and ends with the picture.')

for initial in (2, 0):
    with playback('p7-dts-tail.mkv', 1, ordinal=initial, mapping=True, allow_dts=True,
                  switch_to=2 if initial == 0 else None) as base:
        playlist = get(base, 'v.m3u8')
        assert 6.9 < sum(map(float, re.findall(rb'^#EXTINF:([\d.]+),', playlist, re.M))) < 7.1
        if initial == 0:
            assert b'#EXT-X-START:TIME-OFFSET=2.250,PRECISE=YES' in playlist
        data = get(base, 'init.mp4') + get(base, 'seg1.m4s')
        assert abs(tone(decoded_audio('p7-short-ac3.mp4', data), 0.5) - 990) < 20
    log = (work / 'p7-dts-tail.mkv.log').read_text()
    assert log.count('MKV tail: verified, stop=demux-eof') == 1
    assert ('MKV tail: reused verified video end' in log) == (initial == 0)
    assert 'input audio ordinal=2 stream=3 selected=1 codec=ac3' in log
print('A short AC-3 track plays from the requested position without rescanning the verified video tail.')

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

with playback('dts.mkv', 0, ordinal=0, inplace_to=1) as base:
    init = get(base, 'init.mp4')
    before = init + get(base, 'seg2.m4s')
    assert abs(tone(decoded_audio('inplace-dts.mp4', before), 0.5) - 440) < 20
    assert switch_in_place() == 1
    assert get(base, 'init.mp4') == init
    after = init + get(base, 'seg1.m4s')
    assert probe('inplace-aac.mp4', after)['streams'][1]['channels'] == 6
    assert 640 < tone(decoded_audio('inplace-aac.mp4', after), 0.5) < 680
    # The following segment continues the same generator with the new track.
    assert 640 < tone(decoded_audio('inplace-aac-next.mp4', init + get(base, 'seg2.m4s')), 0.5) < 680
log = (work / 'dts.mkv.log').read_text()
assert 'audio switched in place: ordinal 1, stream 2, aac 1 ch' in log
assert 'input audio ordinal=1 stream=2 selected=1 codec=aac language=pol' in log

with playback('p7-dts-tail.mkv', 1, ordinal=0, mapping=True, allow_dts=True, inplace_to=1) as base:
    init = get(base, 'init.mp4')
    assert abs(tone(decoded_audio('inplace-p7-ac3.mp4', init + get(base, 'seg1.m4s')), 0.5) - 440) < 20
    assert switch_in_place() == 1
    assert get(base, 'init.mp4') == init
    after = init + get(base, 'seg1.m4s')
    video, audio = probe('inplace-p7-dts.mp4', after)['streams']
    assert any(sd.get('dv_profile') == 8 for sd in video['side_data_list'])
    assert abs(tone(decoded_audio('inplace-p7-dts.mp4', after), 0.5) - 770) < 20
log = (work / 'p7-dts-tail.mkv.log').read_text()
assert 'audio switched in place: ordinal 1, stream 2, dts 6 ch' in log
print('An audio track change in the same HLS session keeps the init segment and the picture.')

def annexb_nals(raw):
    starts = [m.end() for m in re.finditer(rb'\x00\x00\x01', raw)]
    nals = []
    for k, start in enumerate(starts):
        end = starts[k + 1] - 3 if k + 1 < len(starts) else len(raw)
        nal = raw[start:end].rstrip(b'\x00')
        if nal:
            nals.append(nal)
    return nals


def ts_video_nals(name, data):
    media = work / name
    media.write_bytes(data)
    raw = subprocess.check_output(['ffmpeg', '-v', 'error', '-i', media, '-map', '0:v:0',
                                   '-c', 'copy', '-f', 'hevc', '-'])
    return annexb_nals(raw)


with playback('dts.mkv', 0, ordinal=0, ts=True) as base:
    assert base.endswith('/v.m3u8'), base
    playlist = get(base)
    assert b'#EXT-X-VERSION:3' in playlist and b'#EXT-X-MAP' not in playlist, playlist
    assert b'\nseg0.ts\n' in playlist and b'\nseg2.ts\n' in playlist, playlist
    seg = get(base, 'seg2.ts')
    assert seg[0] == 0x47 and len(seg) % 188 == 0
    streams = probe('ts-dts.ts', seg)['streams']
    assert [s['codec_name'] for s in streams] == ['h264', 'ac3'] and streams[1]['channels'] == 6
    assert abs(tone(decoded_audio('ts-dts.ts', seg), 0.5) - 440) < 20
    assert get(base, 'seg0.ts')[0] == 0x47
    assert b'DOVI' not in seg
print('DTS -> AC-3 5.1 in MPEG-TS segments and an out-of-order seek passed.')

for name in ('indexed.mkv', 'indexed-mel.mkv'):
    with playback(name, 1, mapping=True, ts=True) as base:
        for k in (1, 0, 2):
            seg = get(base, f'seg{k}.ts')
            assert seg[0] == 0x47 and len(seg) % 188 == 0
            video, audio = probe(f'ts-{name}-{k}.ts', seg)['streams']
            dovi = [sd for sd in video.get('side_data_list', []) if 'dv_profile' in sd]
            assert dovi, video
            assert (dovi[0]['dv_profile'], dovi[0]['dv_bl_signal_compatibility_id'], dovi[0]['rpu_present_flag'],
                    dovi[0]['el_present_flag'], dovi[0]['bl_present_flag']) == (8, 1, 1, 0, 1), dovi
            assert audio['codec_name'] == 'ac3' and audio['channels'] == 6
            nals = ts_video_nals(f'ts-{name}-video-{k}.ts', seg)
            types = [(n[0] >> 1) & 63 for n in nals]
            assert 62 in types and 63 not in types, types
            assert all(not (n[0] & 1 or n[1] >> 3) for n in nals), 'enhancement layer left in'
print('P7 FEL and MEL -> P8.1 in MPEG-TS: PMT Dolby Vision descriptor, RPU, no EL, out-of-order seek passed.')

with playback('p7-dts-tail.mkv', 1, ordinal=0, mapping=True, allow_dts=True, inplace_to=1, ts=True) as base:
    assert abs(tone(decoded_audio('ts-p7-ac3.ts', get(base, 'seg1.ts')), 0.5) - 440) < 20
    assert switch_in_place() == 1
    after = get(base, 'seg1.ts')
    video, audio = probe('ts-p7-dts.ts', after)['streams']
    assert any(sd.get('dv_profile') == 8 for sd in video.get('side_data_list', []))
    assert audio['codec_name'] == 'ac3' and audio['channels'] == 6
    assert abs(tone(decoded_audio('ts-p7-dts.ts', after), 0.5) - 770) < 20
log = (work / 'p7-dts-tail.mkv.log').read_text()
assert 'audio switched in place: ordinal 1, stream 2, dts 6 ch' in log
print('A DTS track change in place keeps Dolby Vision P8.1 in MPEG-TS.')

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

with playback('dts.mkv', 0, buffer_mib=32) as base:
    assert get(base, 'seg0.m4s')
log = (work / 'dts.mkv.log').read_text()
assert 'input ring: video=32 MiB' in log
check_input_buffer(log, 32, 1)

with playback('dts.mkv', 0, buffer_mib=256) as base:
    assert get(base, 'seg0.m4s')
log = (work / 'dts.mkv.log').read_text()
assert 'input ring: video=256 MiB' in log
assert re.search(r'reader rings peak 1 = 256 MiB', log)
check_input_buffer(log, 256, 1)
print('External AAC -> AC-3 track, offsets, speed, out-of-order seek and 64/128/256 MiB presets passed.')
