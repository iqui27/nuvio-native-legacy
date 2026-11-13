# language-scr integration (postmerge documentation)

Scope: #785 (FON785) and #706 (SCR706) source integration into `integracao/2.1`. Source only.

## Identity
- Base: `4211dd864006ddfbf8383d7b7dac453ded86b582`
- Candidate: `0e5606a6119e3089f9585ce3a7bcb1876ca38b4e` (diffSHA256 `2c41c9c318e0e344ffe4411b707a8d8e6dad1706e69616707f07615e22e669c1`, base..candidate)
- Integration merge HEAD before this doc commit: `440b19fcafc9d3590c2252b989e6ad1e4cfc5154` (HEAD^1 base, HEAD^2 candidate, worktree clean at start)
- Reviewer: fv-language-scr-static-review2 (served model UNKNOWN). Verifier: fv-language-scr-runtime-verifier2 (served model UNKNOWN).

## #785 (FON785) — source fix
- Recognized release-label languages (flags, aliases such as VFF, Türkçe/Dublaj, Castellano, LATAM) now reach the existing inferred "Possível áudio" row.
- Pre-existing eight audio-filter false positives fixed through shared recognition and explicit generic precedence. Test expectations unchanged.
- Generic `language`/`languages` fields still take precedence over release text (`src/stream_parse.c`, `metadadosFiltro`).

## #706 (SCR706) — test fixture only
- Test fixture repair only. No production screensaver fix. No root-cause claim for the reporter's symptom.
- Source artifact e9c34d01d is regression provenance, not an external issue closure.

## Tests on merged HEAD 440b19f (run sequentially, TMPDIR and NUVIO_PROPERTIES=/dev/null)
- `bash tests/source_audio_language.sh` rc 0
- `bash tests/fon785_rotulos.sh` rc 0
- `bash tests/video_pausa_lg.sh` rc 0
- Prior diagnostic omission corrected: `source_audio_language` was rc 134 at candidate stage (recorded in language-scr-verification-correction.md). It is rc 0 on merged HEAD. No failure waiver.
- Central canonical gate: 7 declared, 7 executed, 7 required and 7 passed; 0 skipped, failed, timed out or divergent; agreement true, errors empty. Checks: source_audio_language, fon785_rotulos, stream_parser, video_pausa_lg, lsregistro, build-local and varredura-i18n. All rc 0 on the exact candidate above.
- Verification digest: `bf4ec68c5b2319475d3c9588d7f0269d7158bc40b87dd1903e255ef1ca05d902`. Guarded merge status passed; i18n postcheck rc 0, stdout SHA-256 `e2dc1d85ba32029673aefafb6a1c449a89170cefb60cd397499b0dff77cc29e1`.
- The central supplied this gate evidence after the documentation worker could not locate its record. The map retains that worker's original entry and appends this supplement; earlier validation records and item statuses remain unchanged.

## Not run / limits (unknown, not claimed)
- Physical TV acceptance: UNKNOWN for both issues. Issues stay OPEN.
- LG webOS screensaver physical root (C1/C2/C3/C5): UNKNOWN.
- FON row on TV: UNKNOWN.
- Retained P2 notes: FON785-R1/R2, SCR-R1/R2 and LANG2-R1/R2/R3; no confirmed P0/P1. Evidence gaps FON785-R3/R4 were closed by the verifier's hashed red and green logs.
- GUI runners `fontes_lote_e` / `fontes_formatter`: not run. Their flag/alias coverage was verified only by manual scratch probe.
- No Home, sync or cache crossing.
- BIB639 / PAG789 full global gate: pending, unwaived, not integrated.
