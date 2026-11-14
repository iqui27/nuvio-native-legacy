# global-link-boundaries — integration record (test-only, 2026-11-14)

Scope: test-only merge into `integracao/2.1`. No production fix, no TV proof, no issue closed.

## Merge
- Branch `agente/21-fv-global-link-boundaries` merged into `integracao/2.1`.
- Merge commit: `516e73ab8f5fde326568f01da2d79d7307a6ff85` (parents: BASE `5c66e894a1e289558ca1754303b727a7e64c8a86`, candidate `a526023292502189583e54da87f2a172887fb733`).
- Diff `git diff BASE HEAD` sha256: `554faffa865e993cf6608b6aba4a00d655f0481b4c10cc19c59e67eca820d590`.
- Files: `tests/ctxmenu_semfio.sh` (1 line), `tests/esportes_v5_shot.c` (1 line), `tests/streamfit_sheet.c` (+26). Zero `src/` changes.
- Verification digest: `45925e80d780ff5c397060737953d4a19392d96cf6af2678195090bc7d6bf4b5`.
- Static review: `global-link-boundaries-code-review2.md` (sha256 `8fbeac5b877c632fde304bc78d3221eb35e1348eddb45d7f7233d54942b8d904`), findings none.
- Runtime verification: `global-link-boundaries-verification2.md` (sha256 `ba0310707cd120124246bb56b0660490b79bfc63819850f63c7c8df36034dcf8`), runtime2, findings none.
- Canonical gate: 6/6 passed, errors empty (`global-link-boundaries-canonical.log`).

## Prior evidence retained (not rerun)
- BASE (fce3b818-era) link red: ctxmenu_semfio rc1 `_tirarremoto_capturar`; fontescanais_folha rc1 `_ajustes_fonte_ocultar`; streamfit_sheet rc1, 13 undefined symbols.
- ctxmenu behavioral mutation (sync `fioTirarRemoto` on draw thread): 1 remote call, 309 ms; control 0 calls, 0 ms.

## Post-merge checks (2026-11-14, sandbox-exec `global-gate-local-only.sb`, sha256 `223ba6e24f1fc60c6b931ef4758eb64dd079a2def09c6b143999826caa49fd33`)
Run in integration worktree `/Volumes/ExternalSSD/nv-21-int`, private HOME/XDG/data/TMPDIR under `/Volumes/ExternalSSD/tmp/jobtmp/fv-global-link-boundaries-postmerge`.

| command | rc | log sha256 |
|---|---|---|
| `bash tests/ctxmenu_semfio.sh` | 0 | `966eaf71818cd7c73bc3b15a25650f2c3ef9cea0bba875ac127345f5b2626612` |
| `bash tests/esportes619.sh` | 0 | `c2221e2aeca8636183bc081b320f8aabde8ab35ed06161dc604710ea26b3c1fb` |
| `bash tests/fontescanais_folha.sh` | 0 | `a9f78d7846169b714e4dcc6640880d2184343babea45cf75fe98a9d6aab56534` |
| `bash tests/streamfit_sheet.sh` | 0 | `615db577716441ac98582934558f5d48fd8d25b10a8878093148bcaee1bfbd3e` |
| `python3 tools/varredura-i18n.py` | 0 | `e2dc1d85ba32029673aefafb6a1c449a89170cefb60cd397499b0dff77cc29e1` |

- Build not repeated (source unchanged). Retained build record from verify2: `tools/build-local.sh` rc 0, log sha256 `9d117ce3f1a348dfc77af11a34c665d8a5dbe6b56cdea9f1d04171e88d1f120b`.
- esportes619 wrote 6 PNGs in `/Volumes/ExternalSSD/tmp/rev/esportes619/`. Pre-run byte copies and hashes: `/Volumes/ExternalSSD/tmp/rev/issues-exec-fable-20261113/global-link-boundaries-postmerge-logs/preimage-esportes619/`. All 6 are PNG 1920x1080, std dev above 3600 (non-blank). Output hashes are time-dependent (clock text) and are not a golden claim.
- No OS focus/window-server measurement was taken; window HIDDEN is a source and flag claim only.

## Known limits (P3, not findings)
- P3N1 `tests/ctxmenu_semfio.sh:26` uses broad `-Wl,-undefined,dynamic_lookup`.
- P3N2 `tests/streamfit_sheet.c:87` hide-rule double fixed at 0; product default `stream_incompat_padrao` coverage unverified.
- P3N3 hold-OK audioext menu unreachable in this fixture; covered by audioext tests (unverified here).
- HIDDEN window also affects sibling fixtures not run here; images are clock-dependent.

## Global gate status
- Campaign global suite ran on AV candidate `15fbcbf2dcd5a7a8697a36cd9229b3963d4514ab`, rc1 incomplete (ENOSPC). No passing full gate, BIB/PAG-candidate or current-integration global run is established. Required, unwaived.
- TV/GUI physical acceptance: NOT RUN (validacoes index 220 unchanged). Device acceptance UNKNOWN.

## mapa.json
- `docs/issues/mapa.json` `validacoes`: 231 -> 232. Indexes 223-229 `o_que` corrected: stale "full/overall global NOT RUN" clauses replaced with the campaign-run wording above. All other fields and records unchanged. New record index 231 (ref `516e73ab8f5fde326568f01da2d79d7307a6ff85`).
- Counts: `itens` 642 before and after.
