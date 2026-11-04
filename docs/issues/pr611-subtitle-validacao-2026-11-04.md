# PR611 complementary subtitle technical classification — validation

Reviewed commit `b5432e54905b70c228c8757cddbe58f758b5c413`, branch `agente/pr611-subtitle-technical`, base `7b87d35af51c1a42cc2b462958dd3509226cc12d`, merged into integracao/2.1 as `67505e3abffa70225367f6db15fca2ccca35d03a` under existing owner authorization on 2026-11-04.

The technical classifier now retains subtitle-first lines containing technical tags; the language pass continues excluding subtitle languages. Executor demonstrated target FAIL rc1 → PASS rc0. Independent review bbepa2z2c: OK, no P0/P1. Reviewer executed a standalone Linux base/head classifier harness reproducing technical restoration and subtitle language exclusion; parser callers were inspected, and full Mac scripts/build were not rerun by reviewer.

Coordinator postmerge checks, each with disposable HOME, NUVIO_DADOS and TMPDIR and NUVIO_PROPERTIES=/dev/null:

| Check | Exit code |
|---|---:|
| `bash tests/source_exclusion_languages.sh` | 0 |
| `bash tests/source_audio_language.sh` | 0 |
| `bash tests/source_audio_diag.sh` | 0 |
| `python3 tools/varredura-i18n.py` | 0 |
| `bash tools/build-local.sh` | 0 |

Evidence: `/Volumes/ExternalSSD/tmp/rev/rastreio/mutirao-20261104/postmerge-611-subtitle/` (logs and results.json). Independent review: `j-review-pr611-subtitle.log` in the mutirao directory.

Known nonblocking P2 findings: bare subtitle codes such as ts/dv/cam/dd can become false technical tags and affect exclusions; a release-shaped subtitle line can provide a bogus release group. The pre-existing separated-section case `Audio:X | Subs:Y •1080p` still loses the final technical tag. These limitations are recorded under the owner's P0/P1-only blocking policy; no physical TV acceptance is claimed. No keyed platform package, installation, publication or issue closure was performed.

Pre-existing modified mapa.json was preserved byte-for-byte through merge (SHA256 `f607506399b2682701c4766df664fe65134050ca94ccb7b4a67b0d8e3b4f9b1a`) and excluded from this documentation commit.
