#!/usr/bin/env python3
"""Compare text/button scale and artwork sizes, frame by frame.

Larger text reflows a screen: more wrapped lines, fewer Settings rows, another
row of cards scrolled into view. Draw order and counts then stop lining up, so
each captured frame is compared by size: every text/button height at the
target scale is a baseline height of that frame times the factor, and every
card size already existed in that baseline frame (draw by draw when the card
counts match); a frame may lose only cards that were already off screen. A
tree without the feature draws the same heights at both scales, so its smallest text height has no baseline match and the check fails.
Text from 150 px up is poster decoration (the Ranking numerals follow the card
height), like the spy's 150 px button limit. Texture names are not compared: text textures of another size shift the GL
names handed to the artwork.
"""
import sys


def read(path):
    frames = []
    with open(path) as stream:
        for line in stream:
            kind, *values = line.split()
            values = [float(x) for x in values]
            if kind == 'F':
                frames.append({'T': [], 'R': [], 'C': []})
            elif frames:
                frames[-1][kind].append(values)
    return frames


def scaled(what, i, before, after, factor, tol):
    assert bool(before) == bool(after), f'frame {i}: {what} appeared/vanished'
    for h in after:
        assert any(abs(h / (b * factor) - 1) <= tol for b in before), \
            f'frame {i} {what} height {h} is no baseline height x{factor} ({sorted(set(before))})'


def compare(name, base, target, factor):
    f0, f1 = read(base), read(target)
    assert f0, 'no captured frames'
    assert len(f0) == len(f1), f'captured frame count changed: {len(f0)} -> {len(f1)}'
    assert any(f['T'] for f in f0), 'no text drawn'
    same = lambda p, q: all(abs(x - y) <= 2 for x, y in zip(p[1:3], q[1:3]))
    same = lambda p, q: all(abs(x - y) <= 2 for x, y in zip(p[1:3], q[1:3]))
    def seen(c):
        # On screen = at least 15% of the card inside the 1920x1080 frame; a
        # sliver that reflow pushes fully out (a 7 px tail of a 144 px card)
        # may vanish without counting as lost artwork.
        w = min(c[3] + c[1], 1920) - max(c[3], 0)
        h = min(c[4] + c[2], 1080) - max(c[4], 0)
        return w > 0 and h > 0 and w * h >= 0.15 * c[1] * c[2]
    cards = 0
    for i, (a, b) in enumerate(zip(f0, f1)):
        scaled('text', i, [t[0] for t in a['T'] if t[0] < 150], [t[0] for t in b['T'] if t[0] < 150], factor, .08)
        assert not a['R'] or b['R'], f'frame {i}: buttons vanished'
        if a['R'] and b['R']:   # a transient 44x34 rect shows on Home in ~1 run of 7
            scaled('button', i, [r[1] for r in a['R']], [r[1] for r in b['R']], factor, .12)
        assert a['C'] or not b['C'], f'frame {i}: artwork appeared'
        assert b['C'] or not any(map(seen, a['C'])), f'frame {i}: on-screen artwork vanished'
        assert len(b['C']) >= sum(map(seen, a['C'])), f'frame {i}: on-screen artwork vanished'
        if len(a['C']) == len(b['C']):
            for p, q in zip(a['C'], b['C']):
                assert same(p, q), f'frame {i} artwork resized: {p[1:3]} -> {q[1:3]}'
        for q in b['C']:
            assert any(same(p, q) for p in a['C']), f'frame {i} artwork resized: new size {q[1:3]}'
        cards += len(b['C'])
    buttons = 'buttons scaled' if any(f['R'] for f in f0) else 'no rect buttons on this screen'
    print(f'PASS {name}: text scaled, {buttons}, {cards} artwork draws keep their sizes')


if __name__ == '__main__':
    try:
        compare(*sys.argv[1:4], float(sys.argv[4]))
    except (AssertionError, ValueError, ZeroDivisionError) as error:
        sys.exit(f'FAIL {sys.argv[1]}: {error}')
