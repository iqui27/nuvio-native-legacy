# global-link-next — integration post-merge (2026-11-13)

- Branch: `integracao/2.1`, merge HEAD `322ff155fc6536d90b3713b94f73759e2851fe49` ("Merge agente/21-fv-global-link-next").
- Parents: HEAD^1 BASE `49e721eefba6575ee39eef137c083f5c8c711251`; HEAD^2 candidate `1073617326f0a165acb79434701b09665239d9f6` (ancestor of HEAD, verified).
- Diff `git diff 49e721eef HEAD` sha256 `20dcd8224cddcf1cd923118efdab8da6bff815c4a86804b3e0842a67d75518b7` (matches canonical).
- Files: `tests/colfileiras.sh` (+6 -1), `tests/jellyfin.sh` (+1 -1). Zero `src/` changes; `tests/colfileiras.c` and `tests/jellyfin.c` unchanged.
- What it fixes: `colfileiras` link failure (undefined `cat_nome_episodio_{generico,arquivo,reserva}` after they moved to `src/catalogo.c`; the test now extracts the three real bodies with awk from `src/catalogo.c`), and `jellyfin` link failure (zlib `deflate/uncompress`; adds `-lz`).
- Review: `fv-global-link-next-static1` (static, verdict OK, no P0-P2; P3-a/b/c retained). Runtime verifier: `fv-global-link-next-runtime1` (5 required checks rc 0 at HEAD; BASE link red confirmed; mutation red; control green). Canonical gate `global-link-next-canonical.log`: passed, 5/5 executed, 0 divergences.
- Post-merge checks, run from the worktree root under the guard (env -i, private HOME/XDG/data/TMPDIR, NUVIO_PROPERTIES=/dev/null, sandbox-exec -f global-gate-local-only.sb sha256 223ba6e24f1fc60c6b931ef4758eb64dd079a2def09c6b143999826caa49fd33, SDL_MAC_BACKGROUND_APP=1, timeout 900, sequential, exclusive TMPDIR):
  - `bash tests/jellyfin.sh` rc 0 (sha256 `2fd18e831de45cf69d498afc5d97c071c2dde3b5b235e4a282c8c518c2166583`)
  - `bash tests/embybib.sh` rc 0 (sha256 `5d6bff3ff5d7f60dce960526ec48609e8d63ff200950f435ebd39003a4a646a5`)
  - `bash tests/colfileiras.sh` rc 0 (sha256 `efd3d55d99feda05a366cedc88fc31671bc362e8a8c6f87792fc69b126b849b7`)
  - `python3 tools/varredura-i18n.py` rc 0 (sha256 `6505c2cb0ff20caa0f2c6f2cb7854f4107ce3120fb0f98d945a6b72fef7a4f3b`)
- Logs: `/Volumes/ExternalSSD/tmp/rev/issues-exec-fable-20261113/global-link-next-postmerge-logs/`
- Open P3 (not fixed, auxiliary): fixed temp name `$TMPDIR/nuvio-colfileiras-catnome.c` in `tests/colfileiras.sh` (race if runs share TMPDIR); extraction relies on col-0 `}` style (fails loud); pre-existing `-ldl` before sources in `tests/jellyfin.sh` (non-Darwin).
- Limits: Mac host harness only; no TV/device/Android/webOS build; no issue closed or reclassified. AV/BIB full global gate REQUIRED and unwaived (last run rc1 incomplete, ENOSPC).
