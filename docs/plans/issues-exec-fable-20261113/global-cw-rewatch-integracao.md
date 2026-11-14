# global-cw-rewatch — post-merge integration record (2026-11-14)

- Scope: test runners only. Merged into `integracao/2.1` at `5caf6ed6884ce8f942642f6c41a9510d53dd72d3` (guarded merge, status passed).
- BASE `b436054a791f498be9ef4fb41af37267a3e4a873`; candidate `9bd2334d7159a7cbdedd72c145cacd5091c45f95`; diff sha256 `80b847f6cf0aeb31523535da1e81eaf7d7bf583fcc444ade8f65a2f93720cf3c`.
- Change: `tests/cw_prioridade.sh`, `tests/cwdesmarcado.sh`, `tests/cwdup244.sh` link the existing `tests/discovery_boundary_stub.c`. No source, no assertion change.
- Review: static `fv-cw-rewatch-static1` (source only) and runtime `fv-cw-rewatch-runtime1` (distinct verifier), both OK, no P0-P2. Central canonical 5/5, zero errors, verification digest `4d4353d24b808575b4f062ace8293e1ab8bb7ecadf186cd5499ae86e19a42421`.
- Post-merge checks (HEAD `5caf6ed6884ce8f942642f6c41a9510d53dd72d3`, clean tree, derived sandbox guard, rc from actual exit):
  - `bash tests/cw_prioridade.sh` rc 0
  - `bash tests/cwdesmarcado.sh` rc 0
  - `bash tests/cwdup244.sh` rc 0
  - `python3 tools/varredura-i18n.py` rc 0
  - Build not repeated (source unchanged since the verified build).
- Logs: `/Volumes/ExternalSSD/tmp/rev/issues-exec-fable-20261113/fv-global-cw-rewatch-postmerge-logs/` (stdout/stderr/meta per check, SHA-256 in meta). One earlier attempt failed only because the private `mktemp` dir did not exist (rc 1, kept as `*.attempt0-tmpdir-missing.*`); no guard or deny was involved.
- P3 notes carried: N1 author report wrongly says `desc_lapides_primeira` absent (it is at `src/descoberta.h:403`, `TEM_AJUSTE` active). N2 stub `dados_caminho` writes fixed `/tmp/nuvio-fixture-*` and ignores private TMPDIR; the derived guard denies that path. N3 author meta lacked git identity; repaired by the independent run's meta.
- Open: full global `testa-tudo` (AV15fb) previously rc 1 ENOSPC, incomplete; required and unwaived. No full suite on current integration, no BIB/TV proof, no closure.
- Docs record: `docs/issues/mapa.json` validacoes 233 -> 234; itens 642 unchanged; prior values unchanged.
