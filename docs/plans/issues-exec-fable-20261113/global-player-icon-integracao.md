# global-player-icon — integration record (2026-11-14)

- Merge commit on `integracao/2.1`: `83c7975c7099a85bbc58a90f67c6db584285d7c5` (`Merge agente/21-fv-global-player-icon`).
- BASE `f572d89f4f47498e3a388ff55932f7bf1550fffd`, candidate HEAD `6a64ef3eb88b3e6aba2826ebd05f531d28289c9e`, `git diff BASE HEAD | shasum -a 256` = `b81950896461f51b437469f8a59a7d98efedd9039f5c695ae15fd471b24e7976`.
- Verification digest `0e1eeb39667608bde5d153b9005b54649f1b917be0bd0e8a1898135791c97509`. Merge log: merge_rc 0, status passed, postcheck passed.
- Diff: 5 test files (`tests/erro409.c`, `tests/erro409.sh`, `tests/gfx_icone_embutido.c`, `tests/gfx_icone_embutido.sh`, `tests/gfx_icone_embutido_gl.h`). 0 files under `src/`.
- Static reviewer `fv-player-icon-static1` (nv-revisao, medium; served model UNKNOWN to launcher). Runtime verifier `fv-player-icon-runtime1` (distinct, same tier). No P0-P2.
- Central canonical gate: declared 4, executed 4, passed 4, skipped 0, divergences 0, errors `[]`.
- Runtime verifier's own guarded run at committed HEAD 6a64ef3 closes the static reviewer's pre-commit provenance gap.
- BASE link reds only: erro409 and gfx_icone_embutido fail to link at BASE (undefined `_extplayer_tentar`, `_ajustes_*`, `_arte_medida_*`, `_perftv_*`, `_tex_dims_gl`, etc.). No behavioral reds at BASE.
- Negative GL probes: extra allocation and delete are caught. Batch `{tex,0}` with n=2 is not caught (P3).
- P3 notes (not fixed): `ajustes_acento()` fixture values .3/.6/.9 are not the app default (theme 0 / corviva, `src/ajustes.c:3787`), not icon-relevant; GL macro-redefinition warnings are cosmetic; the lldb claim is UNPROVEN (transcript missing).
- No production source changes in this merge.

## Post-merge checks (executor, guarded, HEAD 83c7975)
| cmd | rc | stdout sha256 (prefix) |
|---|---|---|
| `bash tests/erro409.sh` | 0 | 4c3d458cf7e9357f (`PASS erro409`) |
| `bash tests/gfx_icone_embutido.sh` | 0 | ac59aedbf1682726 (`lifecycle ok`) |
| `python3 tools/varredura-i18n.py` | 0 | e2dc1d85ba320296 (0/0/0) |

- Build not repeated. Identity (HEAD, status 0 lines, diff sha `b81950…`) unchanged before and after each run.
- Logs: `/Volumes/ExternalSSD/tmp/jobtmp/fv-global-player-icon-postmerge/logs/pm-*.{meta,stdout,stderr,rc}`.
- First attempt of `erro409` was killed by a tool timeout before its rc was written; kept as `attempt1-killed-pm-erro409.*`, not counted. The rerun was clean.

## Limits (unchanged, not closed here)
- Full global AV15fb gate: rc 1, ENOSPC, incomplete. REQUIRED and unwaived.
- Full integration / BIB suite: NOT passed.
- No TV proof. No issue closed. No device validation.
