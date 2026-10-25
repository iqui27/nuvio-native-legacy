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
