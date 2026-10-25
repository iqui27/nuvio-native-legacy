#!/usr/bin/env python3
"""Check every supported sport has both bounded, packaged JPEG variants."""
import pathlib
import re
import struct
import sys


def dimensions(path):
    data = path.read_bytes()
    assert data[:2] == b"\xff\xd8", f"Not a JPEG: {path}"
    i = 2
    while i + 4 <= len(data):
        assert data[i] == 255, f"Invalid JPEG marker: {path}"
        marker = data[i + 1]
        length = struct.unpack_from(">H", data, i + 2)[0]
        if marker in (0xC0, 0xC1, 0xC2):
            height, width = struct.unpack_from(">HH", data, i + 5)
            return width, height
        assert length >= 2
        i += length + 2
    raise AssertionError(f"Missing JPEG dimensions: {path}")


root = pathlib.Path(__file__).resolve().parents[1]
source = (root / "src/esportes_parse.c").read_text()
modalities = source.split("modalidades[] = {", 1)[1].split("};", 1)[0]
sports = re.findall(r'\{"([a-z-]+)",', modalities)
assert len(sports) == 16
art = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else root / "deploy/app/art/esportes"
expected = {f"{sport}-{quality}.jpg" for sport in sports for quality in ("low", "high")}
assert {p.name for p in art.glob("*.jpg")} == expected
total = 0
for sport in sports:
    low, high = (art / f"{sport}-{q}.jpg" for q in ("low", "high"))
    assert dimensions(low) == (320, 180), low
    assert dimensions(high) == (836, 470), high
    assert low.stat().st_size < high.stat().st_size
    total += low.stat().st_size + high.stat().st_size
assert total < 2 * 1024 * 1024, total
for icon in ("menu_sports", *("esp_" + sport for sport in sports)):
    path = art.parent / "icones" / f"{icon}.png"
    assert path.read_bytes().startswith(b"\x89PNG\r\n\x1a\n"), path
fingerprints = set()
for sport in sports:
    path = art.parent / "icones" / f"esp_{sport}.png"
    data = path.read_bytes()
    assert struct.unpack_from(">II", data, 16) == (256, 256), path
    assert (art.parent / "icones/esportes" / f"{sport}.svg").is_file(), sport
    fingerprints.add(data)
assert len(fingerprints) == len(sports), "Every sport needs its own distinct default"
print(f"Sports backgrounds: {len(sports)} sports, {len(expected)} JPEGs, {total} bytes; Low/High sizes passed")
