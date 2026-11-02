# Add-on source glyph rendering

Branch: `agente/21-21-glyphrender`, based on `fab11c7bf`.

## Root cause and prior evidence

- `0bbce98ed` already integrated the add-on verbatim renderer, color emoji atlas, source-field preservation and measured sheet layout. Reused that implementation. `agente/21-21-fonteincompat` concerns source compatibility and retains that renderer; no compatibility changes were duplicated.
- JSON decoding in `src/js.c` combines valid surrogate pairs into four-byte UTF-8. `stream_parse.c` preserves source names and combines title/description. Valid sample code points reach the text renderer unchanged; decoding was not the cause.
- The atlas generator deliberately excluded ZWJ sequences. Its matcher read only three code points. For `👩🏽‍💻`, the baseline consumes 8 of 15 UTF-8 bytes, leaving the joiner and laptop to separate rendering runs.
- Font selection ignored decorative and supplementary code points and treated unsupported styled letters as replaceable. That policy also affected the verbatim renderer's font selection, despite its cleanup bypass. A whole text run could select one script fallback while leaving other characters uncovered.
- TV SDL_ttf can run without HarfBuzz. Arabic uses the existing `visualDe`/bidi shaping path; emoji need the existing atlas compositor. Remaining text reaches `TTF_RenderUTF8_Blended`, then the same cached SDL surface/OpenGL line texture. No browser, platform emoji font or new runtime library is needed.

## Change

Use all 3,689 Twemoji 14.0.2 images, including ZWJ families, modifier combinations and subdivision flags. Match the longest complete sequence, interpreting optional VS16 while retaining the original UTF-8 byte span. VS15 selects text presentation and has no separate glyph or advance. Wrapping and preview boundaries keep supported clusters and their selectors together.

In add-on mode, check full 32-bit glyph coverage, keep its font-choice cache separate from ordinary UI policy, and split text into runs using the matching existing font. Reuse bundled Roboto for phonetic/small-cap letters. Add bundled Noto Sans Symbols 2 and existing Droid fallback to the remaining-script candidates. Provider strings, code points and capitalization are not folded into simplified labels. The parser and source sheet geometry/actions were not changed.

base-dono: existing Sources island, `DESIGN.md`, the owner's sheet/card language in `base-dono.md`, and `TV · Design System` / source-row components described in `.claude/tv-design-system-sketch.md` in the original checkout. No board or visual style was changed. Corrected only absent glyphs, fragmented emoji and selector rendering.

## Assets and licensing

- Existing Twemoji 14.0.2 graphics attribution and CC BY 4.0 license remain bundled. Regenerated from the existing upstream tarball; modifications remain resizing to 32 px and atlas packing. Atlas is 2048 × 1856, within the GLES2 minimum texture dimension limit.
- Noto Sans Symbols 2, unmodified, from `https://raw.githubusercontent.com/google/fonts/main/ofl/notosanssymbols2/NotoSansSymbols2-Regular.ttf`. Read the upstream SIL OFL 1.1 before adding the font; copyright and complete license are bundled in `NotoSansSymbols2-OFL.txt`.
- Font SHA-256: `7d5fb73b7ca67a6798101741f5d280a3d016a56a197afcd4199dbb57b4b82a21`.
- Added font: 1,233,128 bytes. Atlas PNG: 548,308 to 902,770 bytes. Decoded atlas grows by approximately 5.4 MiB; loaded lazily and shared across cached text lines. Existing font lifecycle manages fallback faces.

## Validation

`bash tests/glyphrender.sh` — rc 0. Compiles common code once and links baseline/current renderers. Baseline returns the expected rc 1 for the incomplete ZWJ cluster; current renderer returns rc 0. Covers parser byte preservation, escaped surrogates, ZWJ/modifier combinations, flags/tags, optional VS16, VS15 text presentation, small caps, BMP/supplementary symbols, mixed scripts with system fallback disabled, nonempty color rasterization, atomic wrapping and cached layout reuse over 30 calls. Fixtures contain reserved `example.invalid` media URLs only and never play them.

`bash tools/build-local.sh` — rc 0. Existing `ATOMIC_VAR_INIT` deprecation warning in `src/perfis.c`; no new build errors.

`git diff --check` — rc 0.

Captures: `/Volumes/ExternalSSD/tmp/rev/glyphrender/before-1-foco-primeira.png` and `after-1-foco-primeira.png`, both 1920 × 1080; compact comparison `source-sheet-before-after.png` in the same directory. Inspected only a JPG reduced to 1000 px.

## Remaining validation

Owner TV check required; no TV keys sent. Emoji coverage follows Twemoji 14.0.2; glyphs outside bundled/system font coverage remain original Unicode and may still show missing-glyph boxes. Host checks do not establish TV memory/performance or universal Unicode coverage. No push, deploy, release, merge or issue closure.
