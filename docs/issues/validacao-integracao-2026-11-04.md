# Reviewed readiness integration — 2026-11-04

Owner explicitly authorized integrating reviewed corrections and continuing parallel work on 2026-11-04. Integration tested at `18fa79f55`.

| Merge | Scope |
|---|---|
| `b65553aa9` | Jellyfin escaped Unicode token boundary and escaped NUL rejection; literal backslash preserved |
| `990ef9e16` | 32 Jellyfin/Emby libraries, selection persistence capacity, mixed types and item budgets preserved; Next Up prerequisite included once |
| `e032efe4e` | Real episode close-path regression harness; test-only |
| `5268898d9` | Synthetic long-token persistence across client restart; test-only |
| `18fa79f55` | Library suite link repair; script-only, assertions preserved |

All four newly merged branches had independent static review OK, no P0/P1. Review did not itself rerun tests. Parent coordinator ran the combined integration checks below after all merges.

| Check | Exit code |
|---|---:|
| `bash tests/jellyfin.sh` | 0 |
| `bash tests/emby.sh` | 0 |
| `bash tests/servidores_library.sh` | 0 |
| `bash tests/ep_close_proof.sh` | 0 |
| `python3 tools/varredura-i18n.py` | 0 |
| `bash tools/build-local.sh` | 0 |

Each command ran with fresh disposable HOME, NUVIO_DADOS and TMPDIR on the SSD. Episode harness additionally denies network at runtime and requires its negative mutation to fail. Logs and machine-readable results: `/Volumes/ExternalSSD/tmp/rev/rastreio/mutirao-20261104/postmerge-18fa79f55/`.

Pre-existing uncommitted `docs/issues/mapa.json` was preserved byte-for-byte through merges, SHA256 `f959354f62180e1ca113b039ba37b29cdcfa3cae0e2a0d437593974ef00c29b9`. It was not included in this documentation commit.

## Coverage limits

No physical device acceptance, release publication or GitHub closure follows from these checks. Episode proof exercises production close path and local progress/watch effects, but not the app.c caller, suspension or rendered Home. Library aggregate coverage does not establish simultaneous saturation of all three providers or final Home clipping on TV. PGS fixture remains invalid and is not validated by ASS media or this test batch. Fresh platform packages and device checks remain separate work.
