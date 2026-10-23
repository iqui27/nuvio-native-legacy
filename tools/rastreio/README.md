# Rastreio issue <-> codigo <-> teste

Python 3 stdlib so (o graphify precisa de LLM para docs; aqui o grafo e feito dos fontes C e do git).
Saidas em `graphify-out/` (ignorado pelo git): rode `grafo.py` de novo para regenerar.

- `python3 tools/rastreio/grafo.py` constroi `graphify-out/issues/grafo.json` e `RELATORIO.md` (~1 min; mesmo HEAD, mesma saida).
- `grafo.py depende --arquivo src/x.c | --funcao nome` lista issues e testes.
- `grafo.py sem-codigo` (abertas sem src tocado), `grafo.py sem-teste` (consertada/lancada sem teste).
- `python3 tools/rastreio/travar.py` preenche `teste`/`sem_teste` no `mapa.json` (idempotente); `mapa.py --check` falha se faltar um dos dois.
- `bash tools/rastreio/guarda.sh <base>` roda so os testes-trava das issues que dependem dos arquivos mudados desde `<base>`; `--tudo` roda todos. Imprime `#N ok` / `#N QUEBROU (tests/x.sh rc=N)`; sai != 0 se algo quebrou. Falhas listadas em `falhas-conhecidas.txt` (padroes fnmatch) saem num bloco a parte e nao contam.
- Release: `tools/auditoria-release.sh` chama o guarda (etapa 2b) com `BASE_GUARDA` (padrao: ultima tag `v*`).

## Contrato de `graphify-out/issues/guarda-ultimo.json` (para o painel)

`{"head": str, "base": str, "quebrou": int, "testes": {"tests/x.sh": rc}, "issues": [{"issue": N, "resultado": "ok"|"quebrou", "testes": {"tests/x.sh": rc}}]}`
rc 0 = passou, 124 = timeout, 127 = teste inexistente. O campo `teste` por issue vem de `docs/issues/mapa.json`. Falha conhecida aparece com rc != 0 e `resultado: "ok"`.

Limites: merges nao entram no diff (vale o commit filho); funcao = definicao na coluna 0 terminada em `{`; commits que mexem em mais de 12 testes nao provam issue.
