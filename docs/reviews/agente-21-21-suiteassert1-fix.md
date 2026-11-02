The patch makes an existing rendering regression check unconditional, leaving it unable to detect rectangular fills in the tested path. The targeted tests run during review passed, but do not expose that loss of coverage.

Review comment:

- [P2] Preserve the rectangular-fill check when the focus ring is enabled — /Volumes/ExternalSSD/nv-21-suiteassert1fix/tests/homelayouts_shot.c:674-674
  In this test path, `ajustes_borda_foco()` was already asserted true, so the new condition always passes regardless of how many `GFX_COR` fills are drawn. A rectangular fill added elsewhere in `desenhaAtalhos()` would therefore go undetected; the source check in `colecaoround.sh` only inspects `desenhaRedonda()`. Track the ring fill separately or otherwise verify that no additional rectangular fills are rendered.

## Follow-up (P2 fix, agente/21-suiteassert1fix)

- **Change:** `NV_COL_ROUND` now measures an isolated `svd_aro_foco` ring on solid (glass must keep `GFX_COR == 0`) and asserts `desenhaAtalhos` does not exceed that fill; `colecaoround.sh` also guards `desenhaAtalhos` round shortcuts (no raw `gfx_cor` outside `!redonda`).
- **Regression:** temporary extra `gfx_cor` in the round `desenhaAtalhos` branch → `bash tests/colecaoround.sh` **rc=1** (source guard); reverted → **rc=0**.
- **Command:** `bash tests/colecaoround.sh` → **exit 0**.
- **Limits:** no pixel-level shape proof; catalog round rows not instrumented the same way as collection shortcuts; full suite / device not run.