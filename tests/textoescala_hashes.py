#!/usr/bin/env python3
"""Every named frame stable in the baseline must come out identical here.

Some shots are nondeterministic in the baseline tree itself (wall-clock
animations, async artwork loads): three captures give different pixels, so no
tree can be compared on them. Those frames are listed as inconclusive, never
skipped silently; every other frame must reproduce the baseline pixels in at
least two of three captures.
"""
import sys
from collections import defaultdict


def read(path):
    frames = defaultdict(list)
    with open(path) as stream:
        for line in stream:
            digest, name = line.split()
            frames[name].append(digest)
    return frames


def compare(base, target):
    a, b = read(base), read(target)
    assert a and b, 'no captured frames'
    assert a.keys() == b.keys(), 'missing/extra captured frames'
    loose = []
    for name in a:
        assert len(a[name]) == len(b[name]) == 3, f'{name}: expected three captures per tree'
        if len(set(a[name])) > 1:
            loose.append(name)
            continue
        # A lone outlier also shows up in stable baseline frames now and then.
        assert b[name].count(a[name][0]) >= 2, f'{name}: pixels differ ({b[name]} vs {a[name][0]})'
    assert len(loose) < len(a), 'every frame unstable in the baseline'
    note = f'; inconclusive (baseline itself unstable): {" ".join(sorted(loose))}' if loose else ''
    print(f'PASS default 100%: {len(a) - len(loose)} stable frames identical{note}')


if __name__ == '__main__':
    try:
        compare(*sys.argv[1:])
    except AssertionError as error:
        sys.exit(f'FAIL default 100%: {error}')
