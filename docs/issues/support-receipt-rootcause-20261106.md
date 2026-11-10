# Support receipt ("log not found") — code/history root cause, 2026-11-06

Offline only. No receipt fields, private logs, properties, URLs, accounts or production read.
SHAs: R = 87c8b8909f0d605f1d8351147fe5afb9de233b8d (v2.0.8), B = 98f2077db1e767a1a038826646045c1fb313d167 (baseline), C = 24d0383627fe919051bd4e80273f3261ded075f9 (integration).
Grades: A = code + runnable check; B = code reading; C = inference, needs production metadata.

## Codec identity
Blob hashes identical at R, B, C: src/regcodigo.c 78b89f0fc7, src/regcodigo.h c6f18ae4c3, src/registro_envio.inc 63f9b7eb06, src/diagnostico.c bae7e924b1, servidor/recomendacoes/src/codigo.js 0332f09142, servidor/recomendacoes/codigo-registro.mjs 4f586a7888. B is not an ancestor of C (merge-base 7cac4f25d); receipt code is the same on both lines. Release-to-current diffs touch only avisos.c focus drawing and unrelated worker modules. codigo-registro.mjs CLI changed later on this branch (F2); codec math unchanged.

## Findings

### F1 (B for mechanism; not audited as the cause of any individual report): normal logs are deleted after 4 days; UI keeps the code with no expiry
- R:servidor/recomendacoes/src/index.js:813 / C:...:816 `REGISTRO_RETENCAO = 4 * 24 * 3600` (all `registro` rows except `pessoa LIKE 'arranque:%'`; arranque keeps 30 days, :814/:817).
- Daily cron: R:index.js:1143 / C:1153 `limparRegistro`. Introduced 581e8fb9b (2026-10-07, #203; was 14 days). Codes exist since 8daaf549e (2026-10-03).
- Stale docs: R:index.js:801 / C:804 "fica 30 dias"; R/C:servidor/recomendacoes/migracao-003-registro.sql:3 "Retencao de 30 dias". Anyone following them expects 30 days.
- Client keeps the last code with no expiry: R/C:src/avisos.c:619 writes registro-codigo.txt (code + send time); avisos.c:1934-1940 reads it; shown as "Último código" with its send date/time at R:src/ajustes_ux_ilha.inc:278 / C:297. Panel text R/C:src/registro_envio.inc:211,361 says the code lets support find the log, without saying for how long.
- Scenario: user sends log, posts the code; support looks it up after more than 4 days; row was deleted by cron; SELECT succeeds with 0 rows. The TV still shows the code (with its timestamp) and no expiry notice.
- AUTOINCREMENT (migracao-003-registro.sql:5) means deleted ids are never reused: a missing id means deleted (or never created in this DB), not overwritten.

### F2 (A, proven defect, fixed on this branch): support decoder read all-digit codes as ids
- Before: R/C:servidor/recomendacoes/codigo-registro.mjs:15 checked `/^\d+$/` before the code regex. The alphabet includes 0-9, so 1,000,000 of the 2^30-1 valid codes are all digits (exhaustive C scan; first id 3212). For those the tool printed another code instead of the id, so the lookup used a wrong id.
- General source finding only; no report is attributed to it here.
- Implemented contract (codigo-registro.mjs): a bare operand is always a six-character code, including all digits; id -> code only with explicit `--id <decimal id>`, valid 1..2^30-1 via the existing encoder. Arity and the flag are checked on the raw argv before Crockford normalization (lowercase, O->0, I/L->1, unchanged). Bare short/long numbers, `--id=N`, missing/extra arguments and invalid ids exit 1 with no stdout. The earlier `--ID=N`-after-normalization proposal is withdrawn (normalization turns I into 1).
- Test: `bash tests/regcodigo_suporte.sh` rc 1 before (`FAIL id=3212` plus new CLI cases), rc 0 after; `bash tests/regcodigo.sh` rc 0. Synthetic ids only.
- Does not recover deleted/absent rows and does not change retention (F1).

### F3 withdrawn
- The earlier F3 treated 718/719/766 as row ids. They are issue numbers, never row ids. Withdrawn entirely; no timeline claim remains.

### F4 (B, P2): dedup can give a manual receipt for an auto row that is later overwritten
- R:index.js:846-858 / C:849-861. A manual send with text equal to a row less than 24 h old returns that row's id. If that row is `(auto)`, a later `(auto)` from the same person/platform/version within 15 min runs an UPDATE on the same id (text, quando, criado). The row exists, but its content is no longer what the user sent. Dedup also does not refresh `criado`, so a repeated receipt can expire up to 24 h sooner than the user expects. Neither case makes the id go missing.

### F5 (B, P3): "sent" without a receipt on any 2xx
- R/C:src/avisos.c:592,626: a manual send is estado 2 on any 2xx, even with no registro_id (e.g. a proxy/portal 200). The panel then says "O servidor guardou o registro" (registro_envio.inc:212). No code is shown in this case. The diagnostico path is stricter (avisos.c:445-446, needs registro_id).

### F6 (B, P3): cleanup batch counter is shared
- R:index.js:917 / C:920: `n` is not reset between the two passes. If the first pass uses 40 batches (8000 rows), the arranque pass is skipped that day. This deletes less, not more, so it is not a cause.

## Eliminated (with evidence)
- Constants C vs JS vs decoder: A, B, M equal; A*A_INV mod 2^30 = 1 (node check). Round trip of synthetic ids 1, 2, 42, 3212, 123456, 2^30-1 passes.
- Signed/float/overflow: C rejects n >= 2^30 on every digit (regcodigo.c:20), no 64-bit overflow; rejects 0, -1, 7.5, null, "", 2^30 (test). JS BigInt is exact; D1 last_row_id is an integer.
- Premature or locally invented code: the code is set only after a 2xx response that carries `"codigo"` or `registro_id` (avisos.c:595-609). The local value comes from the server's id with the same bijection. envCodigo is cleared before a manual send (avisos.c:1885) and overwritten when any non-auto send completes (:609). The panel shows it only at estado 2 (registro_envio.inc:208,355). The diagnostico screen shows no id or code.
- `"registro_id": null` is not taken as a receipt (avisos.c:401).
- Retries (rede_postar_repetindo x3, avisos.c:588) with the same text dedup to the same id (index.js dedup), so no orphan code.
- Endpoint: one compile-time key, NUVIO_REC_URL -> NV_REC_URL (tools/env.sh:53,118,126; tools/arm.sh:211; android CMakeLists.txt:132). No per-platform endpoint switch in the source. Value not read.
- Id reuse: AUTOINCREMENT (migracao-003:5); schema.sql does not redefine `registro`.

## Unknown (needs production metadata, central)
- For any individual report: send time vs 4-day deletion (F1), whether the code was typed exactly, and which DB was queried. Not audited; no individual cause is claimed here. Served model: unknown.
