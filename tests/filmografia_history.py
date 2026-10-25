"""Pixel checks for the three independently reviewed History regressions."""
from pathlib import Path
import subprocess
import sys

root = Path(sys.argv[1])
failures = []


def pixels(path, x, y, w, h):
    return subprocess.check_output(['magick', str(path), '-crop', f'{w}x{h}+{x}+{y}',
                                    '+repage', '-depth', '8', 'rgb:-'])


def bounds(data, w, threshold=225):
    points = [(i // 3 % w, i // 3 // w) for i in range(0, len(data), 3)
              if min(data[i:i + 3]) > threshold]
    return (min(x for x, y in points), min(y for x, y in points),
            max(x for x, y in points), max(y for x, y in points))


for scale in (85, 100):
    for glass in (0, 1):
        path = root / f'filmography-{scale}-g{glass}-historia.png'
        # Empty hero patches, sampled from the independently authored mock.
        # Catch a broad saturated wash replacing the localized pale light.
        patches = ((1420, 300, (203, 109, 180)), (1850, 100, (70, 28, 52)),
                   (1800, 300, (124, 55, 90)), (1420, 80, (84, 35, 66)),
                   (1420, 600, (42, 21, 25)), (50, 300, (16, 10, 14)))
        for x, y, expected in patches:
            data = pixels(path, x, y, 10, 10)
            actual = tuple(sum(data[c::3]) / 100 for c in range(3))
            if max(abs(a - b) for a, b in zip(actual, expected)) > 14:
                failures.append(f'P1 hero {scale}/g{glass} at {x},{y}: {actual}')
        # Gold glyphs plus a separate round marker; no filled generic badge.
        data = pixels(path, 800, 210, 850, 40)
        gold = {(i // 3 % 850, i // 3 // 850) for i in range(0, len(data), 3)
                if data[i] > 130 and 1.2 * data[i + 1] < data[i] < 1.65 * data[i + 1]
                and data[i + 1] > 1.6 * data[i + 2]}
        groups = []
        while gold:
            group = {gold.pop()}
            pending = list(group)
            while pending:
                x, y = pending.pop()
                for point in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                    if point in gold:
                        gold.remove(point)
                        group.add(point)
                        pending.append(point)
            if len(group) > 5:
                groups.append(group)
        groups.sort(key=lambda group: min(x for x, y in group))
        dot = groups[0] if groups else set()
        text = set().union(*groups[1:]) if groups else set()
        if not dot or not text:
            failures.append(f'P1 achievement {scale}/g{glass}: missing gold dot/label')
        else:
            dw = max(x for x, y in dot) - min(x for x, y in dot) + 1
            dh = max(y for x, y in dot) - min(y for x, y in dot) + 1
            tw = max(x for x, y in text) - min(x for x, y in text)
            if abs(dw - dh) > 2 or not 4 <= dw <= 8 or tw < 180 * scale / 100:
                failures.append(f'P1 achievement {scale}/g{glass}: dot {dw}x{dh}, label {tw}')
        name = bounds(pixels(path, 90, 120, 950, 75), 950)
        year = bounds(pixels(path, 90, 650, 300, 100), 300)
        # Real anchors match the mock; glyphs and posters share the chosen scale.
        if abs(name[0] + 90 - 120) > 5 or not 760 * scale / 100 < name[2] - name[0] < 810 * scale / 100:
            failures.append(f'P1 geometry {scale}/g{glass}: name {name}')
        if abs(year[0] + 90 - 120) > 5 or abs(year[1] + 650 - 656) > 6:
            failures.append(f'P1 geometry {scale}/g{glass}: year {year}')
        # Caption must start with the release year, verified against a separately
        # rendered expected prefix by the native harness.
        result = root / f'filmography-{scale}-g{glass}-caption.txt'
        if not result.exists() or result.read_text().strip() != 'PASS':
            failures.append(f'P1 caption {scale}/g{glass}: release-year prefix missing')
        if glass:
            # A perimeter stroke is much brighter than the adjacent dark fill.
            strip = pixels(path, 160, 372, 260, 6)
            bright = sum(min(strip[i:i + 3]) > 95 for i in range(0, len(strip), 3))
            if bright > 100:
                failures.append(f'P1 glass rail {scale}: {bright} bright perimeter pixels')

for failure in failures:
    print(failure)
print(f'History P1 regressions: {"FAIL" if failures else "PASS"}')
sys.exit(bool(failures))
