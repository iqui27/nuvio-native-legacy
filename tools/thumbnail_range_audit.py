"""Offline byte audit and size-aware keyframe simulation. Never fetches media.

Input: numeric CSV emitted by opt-in Android ThumbnailRangeAudit.
Selection is simulated only; indexed candidates are NOT decode/visual validated.
"""
import argparse
import bisect
import csv
import json
from pathlib import Path


def merge(ranges):
    result = []
    for a, b in sorted(ranges):
        if a < 0 or b <= a:
            raise ValueError("invalid range")
        if result and a <= result[-1][1]:
            result[-1] = (result[-1][0], max(b, result[-1][1]))
        else:
            result.append((a, b))
    return result


def size(ranges):
    return sum(b - a for a, b in merge(ranges))


def intersection(left, right):
    a, b = merge(left), merge(right)
    i = j = result = 0
    while i < len(a) and j < len(b):
        result += max(0, min(a[i][1], b[j][1]) - max(a[i][0], b[j][0]))
        if a[i][1] < b[j][1]:
            i += 1
        else:
            j += 1
    return result


def read(path):
    if path.stat().st_size > 16 * 1024 * 1024:
        raise ValueError("audit too large")
    meta, lines = {}, []
    for line in path.read_text().splitlines():
        if line.startswith("# "):
            key, value = line[2:].split("=", 1)
            meta[key] = value
        else:
            lines.append(line)
    if meta.get("version") != "1" or meta.get("truncated") != "false":
        raise ValueError("unsupported or truncated capture")
    frames, downloads = [], []
    total = int(meta["source_bytes"])
    duration = int(meta["duration_ms"])
    if not 60_000 <= duration <= 86_400_000 or total <= 0:
        raise ValueError("missing/invalid duration or source size")
    rows = list(csv.DictReader(lines))
    if len(rows) > 70_000:
        raise ValueError("too many records")
    ids = set()
    for row in rows:
        index, pts, start, end, selected = (int(row[k]) for k in ("index", "pts_ns", "start", "end_exclusive", "selected"))
        if row["kind"] == "frame":
            if index in ids or pts < 0 or selected not in (0, 1):
                raise ValueError("invalid frame")
            ids.add(index)
            if (start, end) != (-1, -1) and not 0 <= start < end <= total:
                raise ValueError("invalid frame bounds")
            frames.append(dict(index=index, pts=pts, start=start, end=end, selected=selected))
        elif row["kind"] == "download":
            if not 0 <= start < end <= total:
                raise ValueError("invalid download bounds")
            downloads.append((start, end))
        else:
            raise ValueError("unknown record")
    frames.sort(key=lambda f: f["pts"])
    if not frames or any(a["pts"] >= b["pts"] for a, b in zip(frames, frames[1:])):
        raise ValueError("missing or non-monotonic index")
    return duration, frames, downloads


def analyse(path, tolerance_ms=5000):
    if not 0 <= tolerance_ms <= 5000:
        raise ValueError("simulation must not relax current 5s tolerance")
    duration, frames, downloads = read(path)
    metadata = dict(line[2:].split("=", 1) for line in path.read_text().splitlines() if line.startswith("# "))
    sampled = metadata.get("sample_targets_ms")
    evaluation_targets = [int(v) for v in sampled.split(",")] if sampled else list(range(10_000, duration + 1, 10_000))
    if (len(evaluation_targets) != len(set(evaluation_targets)) or
            any(t < 10_000 or t > duration or t % 10_000 for t in evaluation_targets)):
        raise ValueError("invalid sampled targets")
    known = [f for f in frames if f["start"] >= 0]
    selected = [f for f in frames if f["selected"]]
    wanted = [(f["start"], f["end"]) for f in selected if f["start"] >= 0]
    transferred = sum(b-a for a, b in downloads)
    unique = size(downloads)
    overlap = intersection(downloads, wanted)
    pts = [f["pts"] for f in frames]
    base, chosen = [], []
    baseline_valid = comparable = unknown = alternatives = 0
    tolerance = tolerance_ms * 1_000_000
    targets = duration // 10_000  # zero intentionally excluded, matching direct mode.
    for ms in evaluation_targets:
        t = ms * 1_000_000
        k = bisect.bisect_right(pts, t) - 1
        if k < 0 or t - pts[k] > tolerance:
            continue
        baseline_valid += 1
        lo = bisect.bisect_left(pts, t - tolerance)
        hi = bisect.bisect_right(pts, t + tolerance)
        candidates = frames[lo:hi]
        if any(f["start"] < 0 for f in candidates):
            unknown += 1
            continue
        comparable += 1
        if len(candidates) > 1:
            alternatives += 1
        previous = frames[k]
        # Fixed smallest-block heuristic. Union accounting below prevents
        # counting the same chosen frame once for each aliased slot.
        best = min(candidates, key=lambda f: (f["end"]-f["start"], abs(f["pts"]-t), f["index"]))
        base.append((previous["start"], previous["end"]))
        chosen.append((best["start"], best["end"]))
    baseline_bytes, chosen_bytes = size(base), size(chosen)
    return {
        "scope": "sampled-header-probe-simulation" if sampled else "offline-index-simulation-not-a-playback-benchmark",
        "probeStatus": metadata.get("probe_status"),
        "inventory": {"frames": len(frames), "knownSizeFrames": len(known),
                      "observedPrefetchSelectedFrames": len(selected),
                      "selectedUnknownSizes": sum(f["start"] < 0 for f in selected)},
        "byteAudit": {"successfulParsedRangeBytes": transferred, "uniqueDownloadedBytes": unique,
                      "repeatedDownloadedBytes": transferred-unique,
                      "knownSelectedBlockUnionBytes": size(wanted),
                      "downloadIntersectionWithKnownSelectedBlocks": overlap,
                      "downloadOutsideKnownSelectedBlocks": unique-overlap,
                      "outsideBytesAreNotAutomaticallyWaste": True,
                      "partialFailuresAndMultipartFramingIncluded": False},
        "simulation": {"toleranceMs": tolerance_ms, "targetSlots": targets,
                       "evaluationTargetSlots": len(evaluation_targets),
                       "baselineEligibleSlots": baseline_valid, "comparableSlots": comparable,
                       "slotsExcludedForUnknownCandidateSizes": unknown,
                       "comparableSlotsWithAlternatives": alternatives,
                       "baselineUniqueBlockBytes": baseline_bytes, "smallestUniqueBlockBytes": chosen_bytes,
                       "savingBytes": baseline_bytes-chosen_bytes,
                       "savingPercent": 100*(baseline_bytes-chosen_bytes)/baseline_bytes if baseline_bytes else None,
                       "allTargetSlotsComparable": comparable == targets,
                       "allSampledSlotsComparable": comparable == len(evaluation_targets),
                       "decodeAndVisualValidation": "not-performed",
                       "productionSelectorChanged": False},
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--tolerance-ms", type=int, default=5000)
    args = parser.parse_args()
    print(json.dumps(analyse(args.capture, args.tolerance_ms), indent=2))
