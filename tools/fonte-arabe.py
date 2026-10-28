#!/usr/bin/env python3
"""Subset the two Noto Sans Arabic UI faces (SIL OFL 1.1).

Source: https://github.com/notofonts/noto-fonts/tree/main/hinted/ttf/NotoSansArabic
Usage: python3 tools/fonte-arabe.py /path/NotoSansArabic-Regular.ttf /path/NotoSansArabic-Bold.ttf
Requires fontTools. Keep presentation forms: src/bidi.c joins letters without
HarfBuzz. Naskh remains packaged for subtitle rendering and UI fallback.
"""
import pathlib
import subprocess
import sys

FONTES = pathlib.Path(__file__).resolve().parent.parent / "deploy/app/fonts"
FAIXAS = ("U+0020-007F,U+00A0,U+0600-06FF,U+0750-077F,U+08A0-08FF,"
          "U+FB50-FDFF,U+FE70-FEFF,U+200C-200F,U+2010-2027")


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    for source, weight in zip(sys.argv[1:], ("Regular", "Bold")):
        target = FONTES / ("NotoSansArabic-%s-Subset.ttf" % weight)
        subprocess.check_call([
            sys.executable, "-m", "fontTools.subset", source,
            "--unicodes=%s" % FAIXAS, "--output-file=%s" % target,
            "--no-hinting", "--desubroutinize", "--layout-features=",
            "--notdef-outline", "--drop-tables+=DSIG",
        ])
        print("%s: %d bytes" % (target, target.stat().st_size))


if __name__ == "__main__":
    main()
