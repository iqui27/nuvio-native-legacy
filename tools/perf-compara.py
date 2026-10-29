#!/usr/bin/env python3
"""Compare per-screen TV windows; capture accepts only the numeric perf schema.

Usage: python3 tools/perf-compara.py a.csv b.csv
       python3 tools/perf-compara.py --capture perf.log output-dir
CSV retains each window. Summary percentiles are frame-weighted window estimates;
exact full-run percentiles cannot be reconstructed from window percentiles.
"""
import argparse
import csv
import math
from pathlib import Path
import re
import sys

FIELDS = ("screen", "frames", "p50_ms", "p95_ms", "max_ms", "over33",
          "uploads_pf", "text_pf", "blur_pf", "glass_pf")
PATTERN = re.compile(r"\[perf-tv\] screen=([a-z-]+) frames=(\d+) "
                     r"p50_ms=(\d+\.\d+) p95_ms=(\d+\.\d+) max_ms=(\d+\.\d+) "
                     r"over33=(\d+) uploads_pf=(\d+\.\d+) text_pf=(\d+\.\d+) "
                     r"blur_pf=(\d+\.\d+) glass_pf=(\d+\.\d+)")
EXPECTED = ("home", "home-stage", "home-round", "collection-grid",
            "collection-timeline", "collection-cover-flow", "collection-ranking",
            "collection-saga", "collection-decades", "collection-wall",
            "collection-showcase", "detail", "context-menu", "settings",
            "player-overlay", "stats-panel")


def read_csv(path):
    with Path(path).open(newline="", encoding="utf-8") as src:
        reader = csv.DictReader(src)
        if tuple(reader.fieldnames or ()) != FIELDS:
            raise ValueError("invalid perf CSV schema")
        rows = list(reader)
    for row in rows:
        if not re.fullmatch(r"[a-z-]+", row["screen"]):
            raise ValueError("invalid screen tag")
        for field in FIELDS[1:]:
            value = float(row[field])
            if not math.isfinite(value) or value < 0:
                raise ValueError("invalid numeric metric")
        if float(row["frames"]) <= 0 or float(row["over33"]) > float(row["frames"]):
            raise ValueError("invalid frame counts")
    return rows


def aggregate(rows):
    groups = {}
    for row in rows:
        n = float(row["frames"])
        group = groups.setdefault(row["screen"], dict.fromkeys(FIELDS[1:], 0.0))
        for field in FIELDS[1:]:
            value = float(row[field])
            if field == "max_ms":
                group[field] = max(group[field], value)
            else:
                group[field] += value if field in ("frames", "over33") else value * n
    for group in groups.values():
        for field in FIELDS[2:]:
            if field not in ("max_ms", "over33"):
                group[field] /= group["frames"]
    return groups


def summary(groups):
    lines = ["screen                       frames p50~ms p95~ms max_ms >33 uploads/f text/f blur/f glass/f"]
    for screen, g in sorted(groups.items()):
        lines.append(f"{screen:28} {g['frames']:6.0f} {g['p50_ms']:7.2f} {g['p95_ms']:7.2f} "
                     f"{g['max_ms']:7.2f} {g['over33']:4.0f} {g['uploads_pf']:9.3f} "
                     f"{g['text_pf']:6.3f} {g['blur_pf']:6.3f} {g['glass_pf']:7.3f}")
    lines.append("~ frame-weighted window percentile estimate; max and counts exact.")
    missing = sorted(set(EXPECTED) - groups.keys())
    if missing:
        lines.append("MISSING: " + ", ".join(missing))
    return "\n".join(lines) + "\n"


def capture(source, directory):
    dest = Path(directory)
    dest.mkdir(parents=True, exist_ok=True)
    rows = []
    with Path(source).open(encoding="utf-8", errors="replace") as src:
        for line in src:
            match = PATTERN.fullmatch(line.strip())
            if match:
                rows.append(dict(zip(FIELDS, match.groups())))
    with (dest / "perf.csv").open("w", newline="", encoding="utf-8") as out:
        writer = csv.DictWriter(out, fieldnames=FIELDS)
        writer.writeheader()
        writer.writerows(rows)
    rows = read_csv(dest / "perf.csv")
    report = summary(aggregate(rows))
    (dest / "summary.txt").write_text(report, encoding="utf-8")
    print(report, end="")
    return 0 if rows else 1


def compare(a, b):
    left, right = aggregate(read_csv(a)), aggregate(read_csv(b))
    print("Delta B - A; p50~/p95~ are frame-weighted window estimates.")
    print("screen                       frames p50~ms p95~ms max_ms >33 uploads/f text/f blur/f glass/f")
    for screen in sorted(left.keys() | right.keys()):
        if screen not in left or screen not in right:
            print(f"{screen:28} MISSING in {'A' if screen not in left else 'B'}")
            continue
        delta = {field: right[screen][field] - left[screen][field] for field in FIELDS[1:]}
        print(f"{screen:28} {delta['frames']:+6.0f} {delta['p50_ms']:+7.2f} {delta['p95_ms']:+7.2f} "
              f"{delta['max_ms']:+7.2f} {delta['over33']:+4.0f} {delta['uploads_pf']:+9.3f} "
              f"{delta['text_pf']:+6.3f} {delta['blur_pf']:+6.3f} {delta['glass_pf']:+7.3f}")
    return 0 if left and right else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", action="store_true")
    parser.add_argument("a")
    parser.add_argument("b")
    args = parser.parse_args()
    try:
        return capture(args.a, args.b) if args.capture else compare(args.a, args.b)
    except (OSError, ValueError, KeyError, csv.Error):
        print("perf: invalid or unreadable input", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
