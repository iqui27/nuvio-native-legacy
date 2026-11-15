# Sync-gate fixtures — post-merge into integracao/2.1

Scope: test fixtures only. No production fix. Issues #639 and #789 get fixture-infrastructure evidence only; no production candidate (BIB639 / PAG789) is integrated and the full global gate remains required and unwaived. Current campaign correction: the suite ran on AV candidate `15fbcbf2dcd5a7a8697a36cd9229b3963d4514ab`, rc 1 incomplete due to ENOSPC; no BIB/PAG-candidate global run is established (see `STATUS.md`). No issue is marked fixed or closed. No field or TV root proof. Device acceptance UNKNOWN.

## Refs
- BASE: `85624bba84243244187efa6ad6f0d50b650632be` (integracao/2.1 before merge)
- CANDIDATE: `7d429206d3e4739c4ef51e858a0adea3d684bbf3` (agente/21-fv-sync-gate-audit)
- MERGE HEAD: `98aaea16cb67c55d072ec483131fa7079fcc68d7`, parents exactly BASE and CANDIDATE (asserted in this job)
- Candidate diff sha256 (BASE..CANDIDATE and BASE..HEAD, identical): `eef67a8136e18ade2305af06e787bbe19072856e839df09618f8cbfedfaed14b`
- HEAD tree == CANDIDATE tree. `git diff BASE HEAD -- src tools` empty.
- Files (4, +94/-4, all under tests/): `tests/addons_patch_transport.py`, `tests/sync_prioridade.c`, `tests/synccolecoes.c`, `tests/syncordem.c`.
- Reviewer: `fv-sync-gate-static-review` (static, doc/diff review). Verifier: `fv-sync-gate-runtime-verifier` (runtime). Served models UNKNOWN (requested nv-revisao medium).
- Static review `sync-gate-code-review.md`: P0-P2 none. P3 notes N1-N5.

## Static old/new doc-only rebind proof
- Static review was bound to BASE `6c035dd5e` / HEAD `01a023e28`. Current BASE `85624bba8` / CANDIDATE `7d429206` differ only by doc commits.
- `git diff OLDBASE OLDHEAD` and `git diff NEWBASE NEWHEAD` both sha256 `eef67a81…d14b`, byte-identical (verifier `rebind/old.diff`, `rebind/new.diff`).
- Base update `6c035dd5e..85624bba8` and candidate update `01a023e2..7d429206` touch exactly `docs/issues/mapa.json`, `STATUS.md`, `av736-integracao.md`. 0 paths under src/, tools/, tests/*.sh differ. The 4 test blobs are identical at old and new HEAD.
- Conclusion: static review reused; no new source opinion needed.

## Canonical gate (central record, 8/8)
| # | cmd | rc | stdout sha256 (prefix) |
|---|---|---|---|
| 1 | bash tests/syncordem.sh | 0 | cac01a267761 |
| 2 | bash tests/sync_prioridade.sh | 0 | ce66c4fb0737 |
| 3 | bash tests/synccolecoes.sh | 0 | 81750357bd9c |
| 4 | bash tests/sync_resumo_corrida.sh | 0 | 4e5403ab36c8 |
| 5 | bash tests/addons_pull_enabled.sh | 0 | 9c34f8143ef9 |
| 6 | bash tests/ajustes_sync.sh | 0 | 1fb6c3584bd8 |
| 7 | bash tools/build-local.sh | 0 | 1f8f0346bf01 |
| 8 | python3 tools/varredura-i18n.py | 0 | e2dc1d85ba32 |

Canonical digest `6bf57f9572457e07107bf25c2b20f3406ebec983e83378bed9f110653f904a39`. Counts: declared 8, executed 8, passed 8, skipped 0, failed 0, timeouts 0, divergences 0, errors 0. Verifier reran all 8 independently at 7d429206 (`sync-gate-verification.md`), same rc 0.

## Actual postmerge runs (this job, on the merge HEAD)
- Sequential, env `NUVIO_PROPERTIES=/dev/null TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp/fv-sync-gate-postmerge`, 420 s per test (perl alarm).
- `tests/syncordem.sh`: rc 0, stdout sha cac01a267761… (equals canonical).
- `tests/synccolecoes.sh`: rc 0, log sha `2ae210b0…` (log text varies run to run; rc stable, canonical also rc 0).
- Scripts inspected first: compile local `src/` and `tests/` C with `cc`, run local binaries; no SDL, window, network, account, SDK or device.
- Other 6 canonical checks not rerun here (scope); their rc 0 from the canonical record.

## Mutation evidence (verifier, at 7d429206 = same test blobs; not rerun here)
Control = unmutated `src/sync.c`. Mutants differ from HEAD `src/sync.c` only by the mutation. All mutant builds rc 0, so the reds below are behavioral (assert or grep), not compile or link failures.
- m4, pointer detection replacing generation check (`sync.c:1920`): session legconta rc 1 x5; only failing assert "blob do perfil novo entrega os idiomas dele". Control rc 0 x5. Per-run logs saved (closes N3).
- m1, protect early return (`sync.c:2358`): troca rc 1. Asserts FALHOU "pendencia do perfil 1 guardada com ele, fora do 2" and "de volta ao 1: a mudanca local dele continua protegida".
- m2, `if (0 && pendente)` (`sync.c:2284`): troca rc 1, same two asserts.
- m3, restore flag dropped (`sync.c:2301`): troca rc 1, only "de volta ao 1…" fails.
- m6, discard disabled (`sync.c:1731`): troca binary rc 0, no assert fails; the shell grep `[sync] ciclo do perfil 1 descartado` is missing, so `tests/syncordem.sh` fails. This is a grep-level red. The colDoOutro blind spot is pre-existing and NOT claimed fixed.
- Control troca rc 0 with discard line found.

## Fixture changes and causes
- Original 21 `confere(` assertions: count 21 at BASE and HEAD; extracted assertion lines identical. Shell grep checks in the test scripts unchanged.
- Removed lines vs `4719643de` total: only the old `pedir(` signature in `addons_patch_transport.py`, and two legconta setup lines (`sync_reaplicar_ajustes();`, `sync_proteger_ajustes_locais();`). No assert, expected value or skip removed.
- `addons_patch_transport.py`: transport stub updated for `pedir2`; `extra` may be NULL (N4).
- `sync_prioridade.c`, `synccolecoes.c`, `syncordem.c`: include bindings for the neutral session hooks and `synccolecoes` nuvem forwarders (own-nuven). 15 neutral stubs (no edit, no migration, clean rewatch, identity costurar) compile under -Wall.
- `syncordem.c` real restored-profile lifecycle and protected-profile switch: warmup `troca` now holds the 2nd cycle (N1). In-flight-first-cycle coverage remains in legconta-troca and sessions `2 segura` / `1 tarde`.
- Compile/link reds: none observed in this job; all builds rc 0. Behavioral reds are listed above.

## P3 notes carried
- N1 troca warmup holds 2nd cycle: acceptable, coverage moved.
- N2 legconta m4 sensitivity depends on allocator reuse (macOS malloc). A SANITIZE/quarantine allocator would hide m4. No sanitizer coverage claimed.
- N3 per-run m4 logs: CLOSED by verifier (logs saved).
- N4 `addons_patch_transport` accepts `extra==NULL`: OPEN, P3.
- N5 `synccolecoes` nuvem forwarders ignore bearer: OPEN, P3.

## Limits
- Full global gate required, unwaived and not passed: AV candidate run rc 1 incomplete (ENOSPC), no BIB/PAG-candidate global run established. BIB639 / PAG789 production candidates NOT integrated.
- Issues #639 and #789: fixture infrastructure evidence only. Not fixed, not closed.
- No field, TV or root proof. Device acceptance UNKNOWN.
- No source, Home, cache or sync-runtime changes in this job.
