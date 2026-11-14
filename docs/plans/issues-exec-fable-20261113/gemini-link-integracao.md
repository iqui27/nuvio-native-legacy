# gemini-link — integration record (test-only, 2026-11-14)

Scope: test-only merge into `integracao/2.1`. No production fix, no TV or device proof, no issue closed, no integration full-suite pass claimed.

## Merge
- Branch `agente/21-fv-gemini-link` merged into `integracao/2.1`.
- Merge commit: `a6f7eecc5ab822d965bfe17e94cc7a388c5ae3a0` (BASE `c489031a9adfc1aa5cfe232a6c3313a7d3e7604a`, candidate `d1e2566f15964d932e49e27d6f7ec7a8b42c9a71`).
- Diff `git diff BASE candidate` sha256: `0b96aac2b3cd249a67d74d3db6e15905408b6bf0fd227aab00991f489bb06fcd` (5 test files, +50/-3, 0 `src/`).
- Verification digest: `8d27f4098db53b81782dc20876f7b493f303138f2d593b5ba7604bccbf851fff`.
- Merge log: `/Volumes/ExternalSSD/tmp/rev/issues-exec-fable-20261113/gemini-link-merge.log` (`status` passed, `postcheck` rc 0, `merge_rc` 0).
- Author's ASan trace claim: UNPROVEN (raw artifact deleted). Not reproduced; not claimed.

## Reviewers and identities (kept separate)
- Static reviewer: `fv-gemini-link-static1` (nv-revisao). Review: `/Volumes/ExternalSSD/tmp/rev/issues-exec-fable-20261113/gemini-link-code-review.md`. No P0-P2.
- Distinct runtime verifier: `fv-gemini-link-runtime1` (nv-revisao, medium, served model UNKNOWN). Record: `/Volumes/ExternalSSD/tmp/rev/issues-exec-fable-20261113/gemini-link-verification.md`. No P0-P2. Its runs are bound to `d1e2566f1` only.
- Central canonical tool (`central.py verifica`): `/Volumes/ExternalSSD/tmp/rev/issues-exec-fable-20261113/gemini-link-canonical.log`: gate passed, 4 declared / 4 executed / 4 required executed / 4 passed, 0 skipped, 0 failed, 0 timeouts, 0 divergences, `errors: []`.
- Post-merge executor: nv-mec, low, served model UNKNOWN. Separate from both reviewers.

## Post-merge checks (fresh, at integration HEAD `a6f7eecc5`)
Run sequentially, foreground, cwd `/Volumes/ExternalSSD/nv-21-int`, through a new persisted guarded wrapper. Each check exited before the next started. No build repeated.

| check | rc | stdout SHA-256 |
|---|---:|---|
| `bash tests/custom_id_stream.sh` | 0 | `be7fc00f55f1d1237a0705dee50010e0a474acb8e78b05628a10cc3b11c153f8` |
| `bash tests/cncverse_regression.sh` (whole: parent + live child + postconditions) | 0 | `5cb4b87299c791f692c077f264711e8a9b7743ff7e37f776e2831a05256ae722` |
| `python3 tools/varredura-i18n.py` | 0 | `e2dc1d85ba32029673aefafb6a1c449a89170cefb60cd397499b0dff77cc29e1` |

- All stderr empty (`e3b0c442…b855`).
- Raw logs: `/Volumes/ExternalSSD/tmp/rev/issues-exec-fable-20261113/gemini-link-postmerge-logs/` (`*.stdout`, `*.stderr`, `manifest.txt`, `SHA256SUMS.txt`).
- Wrapper: `run-postmerge.sh`, SHA-256 `6ff4bbee825c0f7a2560b20d71e10ee3c287367f7073245ffda29a615363b8e5`. Copy in the same log directory.
- Pre/post identity per run: HEAD `a6f7eecc5` = merge, `git status --porcelain` = 0, candidate diff sha `0b96aac2…` = match. All 3 runs `post_identity_ok=yes`.

## Guard proof
- Sandbox: `timeout 900 sandbox-exec -f global-cw-rewatch-verify1-guard.sb`, SHA-256 `942c598d8c72a4cb3eac5c08013aa35f17dd14e1262a188783eb0a34a1c7d20e`.
- Original `global-gate-local-only.sb`, SHA-256 `223ba6e24f1fc60c6b931ef4758eb64dd079a2def09c6b143999826caa49fd33`, pinned and checked before each execution.
- Env: `env -i` with PATH only; private HOME, XDG_*, NUVIO_DADOS, TMPDIR under `/Volumes/ExternalSSD/tmp/jobtmp/fv-gemini-link-postmerge`; `NUVIO_PROPERTIES=/dev/null`; SDL and Homebrew flags as specified.
- Network denied except localhost. Owner `local.properties` and `.ssh`/`.aws`/wrangler denied by the guard.

## BASE link reds (from verifier, BASE `c489031a9` archive, not HEAD)
- `custom_id_stream`: rc 1, undefined `_addonmodo_id`, `_addonmodo_nova_busca`, `_addonmodo_resposta`.
- `cncverse_regression`: rc 1, undefined `_addonmodo_id`, `_addonmodo_resposta` (stops at `set -e` before the child).
- `cncverse_live` (standalone child): rc 1, undefined `_g5ui_channel_allowed`, `_perfis_ativo`.
- All three are link reds only. No behavioral red at BASE.

## Limits (not hidden)
- P3 `tests/cncverse_live.c:79`: published alias `canais`/`cats` dangles after `stagingZerar()` at `:101`. No current reader or free; latent only.
- P3: duplicated overlay in the test files is a hygiene issue, out of scope.
- Verifier bound to `d1e2566f1` only. Author's runs are NOT claimed bound to HEAD.
- Global AV15fb rc 1 ENOSPC remains incomplete, mandatory, unwaived.
- Integration full suite NOT passed.
- No physical TV proof. No issue closed.

## Open
- Full global AV15fb rc 1 ENOSPC: incomplete, REQUIRED, unwaived.
