# Campaign status — issues-exec-fable-20261113

Scope: TV only. Original 118 planned IDs; 114 in TV scope, 4 outside (inactive).
Authorized by owner 2026-11-13. Base: `9115d8bbadd09f98fef5b939732d5334d7b726c7`.

## Merged
- #736 — TPK40 experimental DTS (default OFF only) merged into `integracao/2.1` at `6c035dd5e028fcbdd9949a3945bf0e64fdc1c12f` (BASE `4719643de2750b75a110523f3624c37a15683ce7`, candidate `48e105cecf8db3f88581f9a9ec96606f8a9faa0e`, diff sha256 `1d0a89c62719f0f56049f89fd373706d837e205ae03b18a09c600b315725d6b9`). Canonical gate 5/5 passed; post-merge `dts_tpk_legacy_gate.sh` rc 0 and `tpk40_experimental_output.sh` rc 0. Issue stays open. TV unknown; physical Tizen 4/5.5 pending; no release graduation. Details: `av736-integracao.md`.
- #773 — first code cut merged into `integracao/2.1` at `261ab41c90838b86c0cdf40811a5ffc2ece08fe1` (source `1daef874bf1dce36dc74dc2dc2200f1ac03c5dfd`). Gate digest `a1202ed80b0b73cbe93481e5ffea52fbf0ee6607d9cc1a0c2dbc3da8e7a34ceb`, 2/2 passed. Issue OPEN. Details: `leg773-integracao.md`.
- ready-tv lot (source only, NOT TV acceptance): merge `c5548b46578a02d69c541e8043e874ae8a3e8d18` from candidate `5f1898a2c8196fb62cca835199b0ce7c4434e4de` (base `21e1565df9b6cd4112c4cabc9f7ab2f26eef295d`, diffSHA `b046cffc816b5f2881c17413f83c17a8c62b244d2232c81122dcd44810a05726`, digest `896288673a9acae814aae0e2060d7bb79f20ccc57ab62c18f382d2ca95bf97e1`). Items: DMP766 (storage hygiene, not crash root), SAM/P2P783 (VOD Home-return mechanism), ASS719 (active and same-centisecond sibling retention). Host gate 7/7 rc 0 (zero failures). Post-merge `queda.sh`, `tpk-preso.sh`, `mkvass_despejo719.sh` rc 0 on c5548b4. Details: `ready-tv-integracao.md`.
- language-scr source (#785 FON785 source fix; #706 test fixture only; no production screensaver fix) merged at `440b19fcafc9d3590c2252b989e6ad1e4cfc5154` (HEAD^1 `4211dd8`, HEAD^2 candidate `0e5606a`, diffSHA `2c41c9c3...`). Post-merge rc 0: source_audio_language, fon785_rotulos, video_pausa_lg. Issues OPEN. Details: `language-scr-integracao.md`.
- No other campaign candidate merged. BIB639 / PAG789 full global gate still pending (not waived, not integrated).

## Evidence for #773
- Red on base: rc 134, assertion at `tests/video_acb_lg.c:196`. Green on source head: rc 0; build rc 0.
- Post-merge `tests/video_acb_lg.sh` on `261ab41`: rc 0, log SHA-256 `68be098982c7a41263a09e44a1555a135404556fbdf003905df24527d9db6a55`.
- i18n post-check rc 0.
- Reviewer: `j-rev-fv-leg773-code`, model UNKNOWN. Verifier: `j-verify-fv-leg773`, model UNKNOWN.

## Open / unproven
- #773: B8 (webOS 4.4.3) device proof and on-device selection still missing. Not claimed fixed on device.
- P2 notes F1–F5 (see `leg773-integracao.md`): kept, not blockers.
- DMP766, SAM/P2P783, ASS719: source integrated; physical TV acceptance UNKNOWN; issues stay open.
- P2 notes retained for ready-tv (SAM F1-F5, ASS F4-F6, DMP F7): not blockers. See `ready-tv-integracao.md`.
- BIB639 / PAG789: full global gate pending.
- #785 / #706: physical TV acceptance UNKNOWN; LG screensaver root and FON row on TV unknown; GUI runners fontes_lote_e/fontes_formatter not run; P2 notes kept.
- All other issues: NOT integrated, pending candidate flow.

## Fixtures merged (test-only, #639 / #789 infrastructure)
- Offline account/trailer fixtures merged at `b9e9dfa7a1922ae15990c56e5c618ebae5ce46bd` (BASE `82af242653ec035a71857b442bea9be7abb473fc`, candidate `3e509a3bc09ddf8c77d627542f42b286fb918a37`, diff SHA-256 `4aa1dc7362f67d8009c9f790c9027d3d92817d1a8c2a345b1870d0c0e2602330`). Independent static review OK, distinct runtime 5/5, canonical replay 5/5 under localhost-only OS guard, post-merge account/Apple tests 2/2. Validation 225 appended; all 642 items preserved. Fixture isolation, data initialization and deterministic trailer coverage only; optional QR, F5 P2 and P3 notes remain. Production BIB/PAG and full global gate still pending. Details: `offline-fixtures-integracao.md`.
- Local-services test/Wrangler harness isolation (`agente/21-fv-local-services`) merged at `0618e13c6fd0330f117927430fb8507d75b92904` (BASE `62a4b49693b584be2b83d445beb64f2e9c252a8f`, candidate `3da5ad3cfcae0380dd694f9b7478375d76de6831`, diff SHA-256 `323641fa35c1e1c84c0e7fedad127ba6f5a4e03855c70bacb2d3342a4d4a9784`). Tests and `servidor/recomendacoes/preparar-local.sh` only (0 `src/` changes). Independent static review `j-rev-fv-local-services2-static` OK (P3 notes only), distinct runtime verifier `fv-local-services-runtime-verifier` probes R1–R9 + 7/7 OK, guarded `central.py verifica` 7/7 passed (`4cca8c51788c31a5c929aba3d6260953e000b8a5b42fc62a6a7a21b61f41a814`), post-merge `recomenda_e2e.sh`, `diarionuvem_servidor.sh`, `juntos_e2e.sh` and `varredura-i18n.py` rc 0 under localhost-only OS guard. Validation 226 appended; all 642 items preserved. Details: `local-services-integracao.md`.
- Sync-gate fixture merge into `integracao/2.1` at `98aaea16cb67c55d072ec483131fa7079fcc68d7` (parents BASE `85624bba84243244187efa6ad6f0d50b650632be`, candidate `7d429206d3e4739c4ef51e858a0adea3d684bbf3`, diff sha256 `eef67a8136e18ade2305af06e787bbe19072856e839df09618f8cbfedfaed14b`). Tests only (4 files under `tests/`); no `src/` or `tools/` change. Canonical gate 8/8 passed. Postmerge `tests/syncordem.sh` and `tests/synccolecoes.sh` rc 0. Details: `sync-fixtures-integracao.md`.
- Issues #639 and #789: fixture infrastructure evidence ONLY. Not fixed, not closed. BIB639 / PAG789 production candidates NOT integrated. Full global gate NOT RUN, not waived. Device acceptance UNKNOWN.
- Open P3 notes: N4 (`extra==NULL` accepted), N5 (nuvem forwarders ignore bearer). N1/N2 recorded, N3 closed.

## Global compile fixture cut (fixture-only, #639 / #789 infrastructure)
- Merged into `integracao/2.1` at `9ddae314efae99413f95f236ebcc7f5fa7f253a0` (prior `6ac09088b04123a4b29e45901e253a652477f968`, candidate `fa32c8e68d7eeb92127d82179ea7958f01a9959d`, review diff sha256 `120b888ffc8fa2e88c37a00566c621dbcc69b641e48f5e812870b05eeca1d03a`, verification digest `900744a7c1122ce1b4c695a9e3f213d25270c4ad75f2e7a9d43effdbda486124`). 7 test files, 0 `src/`. 4 compile failures repaired; Poster G expectation matches Oct 17 contract. Review OK (no P0/P1).
- Post-merge (2026-11-13, sandbox): `colestilos.sh` rc 0, `colordem.sh` rc 0 (rerun after env-only PATH fix; first attempt rc 1 kept as evidence), `varredura-i18n.py` rc 0.
- Open: F1 P2 Padrão collection/catalog width parity unverified; F2 P2 colordem_menu Stage-row coverage gap; overall AV/BIB global gate REQUIRED and unwaived. Not a global-pass claim. No issue closed; no TV source fix claimed. Details: `global-compile-integracao.md`.
