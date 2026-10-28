#!/usr/bin/env python3
"""Mirror Latin GPOS pair advances into legacy kern for TV SDL_ttf.

Requires fontTools. GPOS remains authoritative on HarfBuzz platforms.
Usage: python3 tools/fonte-kern.py deploy/app/fonts/InterDisplay-Regular.ttf ...
"""
import sys
import unicodedata
from collections import defaultdict
from fontTools.ttLib import TTFont, newTable
from fontTools.ttLib.tables._k_e_r_n import KernTable_format_0
from fontTools.ttLib.tables.DefaultTable import DefaultTable


def pairs_from_gpos(font):
    gpos = font['GPOS'].table
    script = next(s.Script for s in gpos.ScriptList.ScriptRecord if s.ScriptTag == 'latn')
    lookups = []
    for index in script.DefaultLangSys.FeatureIndex:
        feature = gpos.FeatureList.FeatureRecord[index]
        if feature.FeatureTag == 'kern':
            for lookup in feature.Feature.LookupListIndex:
                if lookup not in lookups:
                    lookups.append(lookup)
    pairs = defaultdict(int)
    # Keep Latin text and punctuation; complex scripts retain their GPOS.
    glyphs = {name for cp, name in font.getBestCmap().items()
              if 'LATIN' in unicodedata.name(chr(cp), '')
              or unicodedata.category(chr(cp))[0] in 'NPZ'}
    for index in lookups:
        lookup = gpos.LookupList.Lookup[index]
        current = {}
        for sub in lookup.SubTable:
            if lookup.LookupType == 9:
                assert sub.ExtensionLookupType == 2
                sub = sub.ExtSubTable
            else:
                assert lookup.LookupType == 2
            assert sub.ValueFormat1 == 4 and sub.ValueFormat2 == 0
            if sub.Format == 1:
                for left, pairset in zip(sub.Coverage.glyphs, sub.PairSet):
                    for pair in pairset.PairValueRecord:
                        current.setdefault((left, pair.SecondGlyph), pair.Value1.XAdvance)
            else:
                assert sub.Format == 2
                right_classes = defaultdict(list)
                for right in glyphs:
                    right_classes[sub.ClassDef2.classDefs.get(right, 0)].append(right)
                for left in sub.Coverage.glyphs:
                    row = sub.Class1Record[sub.ClassDef1.classDefs.get(left, 0)]
                    for cls, record in enumerate(row.Class2Record):
                        value = record.Value1.XAdvance if record.Value1 else 0
                        for right in right_classes[cls]:
                            current.setdefault((left, right), value)
        for pair, value in current.items():
            pairs[pair] += value
    return {pair: value for pair, value in pairs.items()
            if value and pair[0] in glyphs and pair[1] in glyphs}


def convert(path):
    font = TTFont(path, recalcTimestamp=False)
    original = {tag: font.getTableData(tag) for tag in font.keys()
                if tag not in ('GlyphOrder', 'head', 'kern')}
    pairs = pairs_from_gpos(font)
    order = font.getReverseGlyphMap()
    pairs = sorted(pairs.items(), key=lambda p: (order[p[0][0]], order[p[0][1]]))
    kern = newTable('kern')
    kern.version = 0
    kern.kernTables = []
    # A format-0 subtable has a 16-bit byte length; FreeType sums subtables.
    for start in range(0, len(pairs), 10920):
        table = KernTable_format_0()
        table.version, table.coverage = 0, 1
        table.kernTable = dict(pairs[start:start + 10920])
        kern.kernTables.append(table)
    font['kern'] = kern
    # Keep original tables byte-for-byte (including GPOS lookup ordering).
    for tag, data in original.items():
        table = DefaultTable(tag)
        table.data = data
        font[tag] = table
    font.save(path)
    print(f'{path}: {len(pairs)} kerning pairs')


if __name__ == '__main__':
    for path in sys.argv[1:]:
        convert(path)
