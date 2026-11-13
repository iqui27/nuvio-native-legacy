# Integracao `local-services` (Fable 2.1)

- Data: 2026-11-13
- Branch integrada: `agente/21-fv-local-services`
- Base anterior em `integracao/2.1`: `62a4b49693b584be2b83d445beb64f2e9c252a8f`
- Candidate HEAD: `3da5ad3cfcae0380dd694f9b7478375d76de6831`
- Commit de merge em `integracao/2.1`: `0618e13c6fd0330f117927430fb8507d75b92904`
- Escopo integrado (9 arquivos de testes/infraestrutura local, 0 arquivos em `src/`):
  - `servidor/recomendacoes/preparar-local.sh`
  - `tests/diarionuvem_servidor.sh`
  - `tests/frost_flicker.sh`
  - `tests/ios_bundle_seguro.sh`
  - `tests/juntos_e2e.c`
  - `tests/juntos_e2e.sh`
  - `tests/recomenda_e2e.c`
  - `tests/recomenda_e2e.sh`
  - `tests/servidor_local.sh`

## O que mudou (infraestrutura de testes/Workers offline apenas)

- Adicionado helper compartilhado `tests/servidor_local.sh` para executar `wrangler dev --local` offline usando o cache local (`~/.npm/_npx` ou `NUVIO_WRANGLER`), versao exata validada por JSON (`4.149.0` por padrao), isolamento por execucao (`HOME`, `XDG_*`, `TMPDIR`, sem egress de banner/`cf.json`), semente D1 descartavel e encerramento completo do grupo de processos (`nv_dev_stop`, zero `workerd` orfao e zero `.wrangler` no worktree).
- `tests/recomenda_e2e.sh`, `tests/diarionuvem_servidor.sh`, `tests/juntos_e2e.sh`, `tests/juntos_e2e.c`, `tests/recomenda_e2e.c`: isolamento offline com `--local`, semente minima mantida quando URL explicita e passada, clock relativo na assecao de `juntos_e2e.c` com host emitindo `comecar` e tolerancia de 150 ms.
- `servidor/recomendacoes/preparar-local.sh`: injecao opcional de `NUVIO_WRANGLER`, `NUVIO_D1_CFG`, `NUVIO_D1_PERSIST` e `NUVIO_D1_LOG` preservando o fluxo padrao.
- `tests/frost_flicker.sh`, `tests/ios_bundle_seguro.sh`: caminhos de saida/log configuraveis para execucao sob sandbox.

## Verificacao e gate

- Revisao estatica independente: `j-rev-fv-local-services2-static` — `veredito: "OK"`, sem P0/P1/P2 em aberto (notas P3 mantidas em `/Volumes/ExternalSSD/tmp/rev/issues-exec-fable-20261113/local-services-code-review2.md`).
- Verificador runtime distinto: `fv-local-services-runtime-verifier` (`/Volumes/ExternalSSD/tmp/rev/issues-exec-fable-20261113/j-verify-fv-local-services.log`, `/Volumes/ExternalSSD/tmp/rev/issues-exec-fable-20261113/local-services-verification.md`) — probes R1-R9 e 7/7 checagens alvo passaram.
- `central.py verifica` sob `env -i` + `sandbox-exec -f /Volumes/ExternalSSD/tmp/rev/issues-exec-fable-20261113/global-gate-local-only.sb`: `gate == "passed"`, `7/7` (`tests/recomenda_e2e.sh`, `tests/diarionuvem_servidor.sh`, `tests/juntos_e2e.sh`, `tests/frost_flicker.sh`, `tests/ios_bundle_seguro.sh`, `tools/build-local.sh`, `tools/varredura-i18n.py`) rc 0, `0` divergencias, `evidence_digest` `4cca8c51788c31a5c929aba3d6260953e000b8a5b42fc62a6a7a21b61f41a814`, `log_digest` `bef34433905e9ea76a05b875b15660bac724610ebd39d5e933a8ba127b4447f9`.
- `integrate-attributed.py agente/21-fv-local-services`: `merge_rc: 0`, `postcheck` `varredura-i18n.py` rc 0 (`e2dc1d85ba32029673aefafb6a1c449a89170cefb60cd397499b0dff77cc29e1`).
- Checagens pos-merge em `/Volumes/ExternalSSD/nv-21-int` sob `global-gate-local-only.sb`:
  - `bash tests/recomenda_e2e.sh`: rc 0 (`recomenda_e2e: ok`)
  - `bash tests/diarionuvem_servidor.sh`: rc 0 (`diarionuvem_servidor: ok`, `desligar apagou: ok`)
  - `bash tests/juntos_e2e.sh`: rc 0 (`juntos_e2e: motor + websocket do app contra o nuvio-juntos local: alinhados`)
  - `python3 tools/varredura-i18n.py`: rc 0 (0 ocorrencias)
  - `workerd` restante: nenhum; arvore limpa apos limpeza de scratch.

## Limites e pendencias explicitas

- Apenas infraestrutura de testes/servicos locais (sem correcao de produto, sem deploy de Workers, sem aceite fisico em TV).
- Suite global `tools/testa-tudo.sh` nao executada neste recorte (nao dispensada; exigida nos lotes com `src/`).
- Nenhuma issue fechada nem alterada no GitHub.
