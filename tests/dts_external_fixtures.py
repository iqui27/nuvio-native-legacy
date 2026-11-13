# Offline, deterministic audio beds for the real engine and signal checks.
import array, math, random, sys, wave
R = 8000
def bursts(seed, seconds, count, amp):
    rnd = random.Random(seed); ev = []
    for _ in range(count):
        ev.append((rnd.random() * seconds, amp * (0.3 + rnd.random()), rnd.uniform(0.05, 0.25), rnd.random()))
    return ev
def biquad(hp, fc):   # RBJ, Q 0.707, as dts_match.inc
    w0 = 2 * math.pi * fc / R; c = math.cos(w0); al = math.sin(w0) / (2 * 0.7071067811865476); a0 = 1 + al
    b0 = ((1 + c) if hp else (1 - c)) / 2 / a0
    return (b0, (-(1 + c) if hp else (1 - c)) / a0, b0, -2 * c / a0, (1 - al) / a0)
# Speech sits in 300-3000 Hz (the band dts_match.h says a dub changes); the
# low band keeps the shared music and effects.
SPEECH = [biquad(1, 300), biquad(1, 300), biquad(0, 3000), biquad(0, 3000)]
def render(path, seconds, layers):
    n = int(seconds * R); x = [0.0] * n
    for ev, warp, *speech in layers:
        for at, a, ln, ph in ev:
            t0 = warp(at)
            if t0 is None or t0 < 0 or t0 >= seconds: continue
            i0 = int(t0 * R); m = int(ln * R)
            rnd = random.Random(int(ph * 1e9))
            seg = [a * (rnd.random() * 2 - 1) * (1 - (i - i0) / m) for i in range(i0, min(n, i0 + m))]
            for b0, b1, b2, a1, a2 in (SPEECH if speech else ()):
                z1 = z2 = 0.0
                for k, v in enumerate(seg):
                    y = b0 * v + z1; z1 = b1 * v - a1 * y + z2; z2 = b2 * v - a2 * y; seg[k] = y
            for k, v in enumerate(seg): x[i0 + k] += v
    a = array.array('h', (max(-32767, min(32767, int(v * 9000))) for v in x))
    with wave.open(path, 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(R); w.writeframes(a.tobytes())
K = 25 / (24000 / 1001)
bed = bursts(11, 200, 1400, 1.0)
render(sys.argv[1] + '/c_main.wav', 120, [(bed, lambda t: t), (bursts(12, 120, 500, 0.5), lambda t: t)])
render(sys.argv[1] + '/c_dub.wav', 160, [(bed, lambda t: t + 40), (bursts(13, 160, 650, 0.5), lambda t: t),
                                         (bursts(14, 40, 160, 1.0), lambda t: t)])
render(sys.argv[1] + '/c_pal.wav', 120 / K, [(bed, lambda t: t / K), (bursts(15, 120, 500, 0.5), lambda t: t)])
# Another cut to follow: the "extended" picture has 25 s at 60 s that the
# "theatrical" version lacks (there, its own unrelated sound).
ext = bursts(21, 200, 1400, 1.0)
def warp_ext(at):   # the bed's event at `at` (theatrical time) in the extended timeline
    return at if at < 60 else at + 25
render(sys.argv[1] + '/f_ext.wav', 120, [(ext, warp_ext), (bursts(22, 25, 160, 1.0), lambda t: t + 60)])
render(sys.argv[1] + '/f_th.wav', 95, [(ext, lambda t: t), (bursts(23, 95, 100, 0.25), lambda t: t)])
# The same cut in other languages (owner's log 08/11, The Godfather): this
# file has a Russian dub (default) over the same effects and its English
# original; the other version is a Polish voice-over on the original (quiet)
# with loud Polish speech, and 30 s more at the end (credits), so it is taken
# for "another cut". Nothing is missing: no switch may happen.
fx = bursts(31, 150, 700, 0.6)
en = bursts(32, 150, 900, 1.0)
render(sys.argv[1] + '/x_ru.wav', 120, [(fx, lambda t: t), (bursts(33, 120, 900, 1.3), lambda t: t)])
render(sys.argv[1] + '/x_en.wav', 120, [(fx, lambda t: t), (en, lambda t: t)])
render(sys.argv[1] + '/x_pl.wav', 150, [(fx, lambda t: t), (en, lambda t: t), (bursts(34, 150, 1300, 1.6), lambda t: t)])
# Only a dub in each over the same effects. Dialogue scenes (both talk at the
# same places, in the speech band) drown the shared effects there: 40 s at
# 72-112 s score ~0.2 at the right offset, still the peak.
def talk(seed, count, amp):
    rnd = random.Random(seed); ev = []
    for a, b in ((22, 38), (50, 64), (72, 112)):
        for _ in range(count):
            ev.append((rnd.uniform(a, b), amp * (0.5 + rnd.random()), rnd.uniform(0.08, 0.3), rnd.random()))
    return ev
fx2 = bursts(35, 150, 900, 0.8)
render(sys.argv[1] + '/y_ru.wav', 120, [(fx2, lambda t: t), (talk(36, 260, 2.6), lambda t: t, 1)])
render(sys.argv[1] + '/y_pl.wav', 150, [(fx2, lambda t: t), (talk(37, 260, 2.6), lambda t: t, 1)])
# Two episode releases (owner's log 08/11, Chicago Fire): a gap 0.6 s longer
# at 70 s in the other one (and 15 s more at the end): the sync shifts by 0.6 s.
ep = bursts(41, 200, 1400, 1.0)
render(sys.argv[1] + '/s_main.wav', 120, [(ep, lambda t: t)])
render(sys.argv[1] + '/s_dub.wav', 135, [(ep, lambda t: t if t < 70 else t + 0.6), (bursts(42, 135, 120, 0.2), lambda t: t)])
# Same frame rate, a gap 0.5 s longer at 200 s: the second look (5 min after
# the first find) must move the offset, not believe a speed difference.
lg = bursts(43, 400, 2600, 1.0)
render(sys.argv[1] + '/l_main.wav', 380, [(lg, lambda t: t)])
render(sys.argv[1] + '/l_dub.wav', 380, [(lg, lambda t: t if t < 200 else t + 0.5), (bursts(44, 380, 300, 0.2), lambda t: t)])
