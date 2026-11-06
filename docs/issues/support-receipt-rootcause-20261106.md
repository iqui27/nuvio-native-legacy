# Support receipt ("log not found") — code/history root cause, 2026-11-06

Offline only. No receipt fields, private logs, properties, URLs, accounts or production read.
SHAs: R = 87c8b8909f0d605f1d8351147fe5afb9de233b8d (v2.0.8), B = 98f2077db1e767a1a038826646045c1fb313d167 (baseline), C = 24d0383627fe919051bd4e80273f3261ded075f9 (integration).
Grades: A = code + runnable check; B = code reading; C = inference, needs production metadata.

## Codec identity
Blob hashes identical at R, B, C: src/regcodigo.c 78b89f0fc7, src/regcodigo.h c6f18ae4c3, src/registro_envio.inc 63f9b7eb06, src/diagnostico.c bae7e924b1, servidor/recomendacoes/src/codigo.js 0332f09142, servidor/recomendacoes/codigo-registro.mjs 4f586a7888. B is not an ancestor of C (merge-base 7cac4f25d); receipt code is the same on both lines. Release-to-current diffs touch only avisos.c focus drawing and unrelated worker modules.

## Findings

### F1 (main candidate, B for mechanism / C for reporters): normal logs are deleted after 4 days; UI keeps the code forever
- R:servidor/recomendacoes/src/index.js:813 / C:...:816 `REGISTRO_RETENCAO = 4 * 24 * 3600` (all `registro` rows except `pessoa LIKE 'arranque:%'`; arranque keeps 30 days, :814/:817).
- Daily cron: R:index.js:1143 / C:1153 `limparRegistro`. Introduced 581e8fb9b (2026-10-07, #203; was 14 days). Codes exist since 8daaf549e (2026-10-03).
- Stale docs: R:index.js:801 / C:804 "fica 30 dias"; R/C:servidor/recomendacoes/migracao-003-registro.sql:3 "Retencao de 30 dias". Anyone following them expects 30 days.
- Client keeps the last code with no expiry: R/C:src/avisos.c:619 writes registro-codigo.txt; avisos.c:1934-1940 reads it; shown as "Último código" at R:src/ajustes_ux_ilha.inc:278 / C:297. Panel text R/C:src/registro_envio.inc:211,361 says the code lets support find the log, without saying for how long.
- Scenario: user sends log, posts the code in an issue/group; support looks it up after more than 4 days; row was deleted by cron; SELECT succeeds with 0 rows. The TV still shows the code as valid.
- AUTOINCREMENT (migracao-003-registro.sql:5) means deleted ids are never reused: a missing id means deleted (or never created in this DB), not overwritten.

### F2 (A, proven defect): support decoder reads all-digit codes as ids
- R/C:servidor/recomendacoes/codigo-registro.mjs:15 checks `/^\d+$/` before the code regex (:16). The alphabet includes 0-9, so 1,000,000 of the 2^30-1 valid codes are all digits (exhaustive C scan; first id 3212; 89 ids below 100000). For those the tool prints another code instead of the id; the id then queried is wrong, so the lookup misses.
- Not the cause for ids 718/719/766: they round-trip correctly (tests/regcodigo_suporte.sh). Affects about 0.09% of codes.
- Repro: `bash tests/regcodigo_suporte.sh` -> rc 1 on R/B/C (`FAIL id=3212`).
- Proposed minimum fix (not applied): in codigo-registro.mjs, encode only with an explicit `--id=N`, and keep the bare-number branch only when the length is not 6:
  ```js
  if (/^--ID=\d+$/.test(arg)) { console.log(codigoRegistro(arg.slice(5))); process.exit(0); }
  if (/^\d+$/.test(arg) && arg.length !== 6) { console.log(codigoRegistro(arg)); process.exit(0); }
  ```
  Checked in a scratch copy: same test rc 0.

### F3 (C, timeline): ids 718/719/766 are low for codes issued after 2026-10-03
- `registro` created 541b5fd24 (2026-09-19). migracao-014 header (00b568c9e, 2026-10-07) records ~38k rows and the #203 message records ~13.7k auto rows/day before the change. With AUTOINCREMENT the ids should have passed 766 well before codes existed (2026-10-03).
- If the max id or sqlite_sequence is far above 766, those ids would be rows from about 2026-09-19..22: deleted by retention long ago, and also unlikely to be codes from a v2.x TV. Possible explanations: code mistyped or misread (Crockford O->0, I/L->1 fixes only some errors; a wrong but valid code decodes to a random id), a different DB/worker, or a reset sequence. Central should compare `SELECT seq FROM sqlite_sequence WHERE name='registro'` and `MIN(id)` against the missing ids.

### F4 (B, P2): dedup can give a manual receipt for an auto row that is later overwritten
- R:index.js:846-858 / C:849-861. A manual send with text equal to a row less than 24 h old returns that row's id. If that row is `(auto)`, a later `(auto)` from the same person/platform/version within 15 min runs an UPDATE on the same id (text, quando, criado). The row exists, but its content is no longer what the user sent. Dedup also does not refresh `criado`, so a repeated receipt can expire up to 24 h sooner than the user expects. Neither case makes the id go missing.

### F5 (B, P3): "sent" without a receipt on any 2xx
- R/C:src/avisos.c:592,626: a manual send is estado 2 on any 2xx, even with no registro_id (e.g. a proxy/portal 200). The panel then says "O servidor guardou o registro" (registro_envio.inc:212). No code is shown in this case. The diagnostico path is stricter (avisos.c:445-446, needs registro_id).

### F6 (B, P3): cleanup batch counter is shared
- R:index.js:917 / C:920: `n` is not reset between the two passes. If the first pass uses 40 batches (8000 rows), the arranque pass is skipped that day. This deletes less, not more, so it is not a cause.

## Eliminated (with evidence)
- Constants C vs JS vs decoder: A, B, M equal; A*A_INV mod 2^30 = 1 (node check). Round trip of ids 1, 2, 718, 719, 766, 2^30-1 passes.
- Signed/float/overflow: C rejects n >= 2^30 on every digit (regcodigo.c:20), no 64-bit overflow; rejects 0, -1, 7.5, null, "", 2^30 (test). JS BigInt is exact; D1 last_row_id is an integer.
- Premature or locally invented code: the code is set only after a 2xx response that carries `"codigo"` or `registro_id` (avisos.c:595-609). The local value comes from the server's id with the same bijection. envCodigo is cleared before a manual send (avisos.c:1885) and overwritten when any non-auto send completes (:609). The panel shows it only at estado 2 (registro_envio.inc:208,355). The diagnostico screen shows no id or code.
- `"registro_id": null` is not taken as a receipt (avisos.c:401).
- Retries (rede_postar_repetindo x3, avisos.c:588) with the same text dedup to the same id (index.js dedup), so no orphan code.
- Endpoint: one compile-time key, NUVIO_REC_URL -> NV_REC_URL (tools/env.sh:53,118,126; tools/arm.sh:211; android CMakeLists.txt:132). No per-platform endpoint switch in the source. Value not read.
- Id reuse: AUTOINCREMENT (migracao-003:5); schema.sql does not redefine `registro`.

## Unknown (needs production metadata, central)
- Send dates for the three codes vs deletion (F1), and sqlite_sequence / MIN(id) vs 718/719/766 (F3).
- Whether reporters typed the code exactly (F3). Served model: unknown.
