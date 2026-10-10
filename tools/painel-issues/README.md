# Painel de issues e roadmap

Site estatico (HTML + JS puro, sem CDN, funciona offline na LAN) que mostra
`docs/issues/mapa.json`: KPIs, barras por alvo e status, roadmap por release,
as decisoes do dono com o contexto de cada pergunta, auditoria de validacoes,
relatos fora do GitHub, "fechar com a 2.0.3", "nao fechar ainda" e a tabela de issues.

## Gerar e ver localmente

    python3 tools/painel-issues/gerar.py [--repo <worktree>] [--out <dir>]
    python3 -m http.server -d tools/painel-issues/site

Sem argumentos le o mapa do proprio repo e escreve em `tools/painel-issues/site/`
(ignorado pelo git). Falha com codigo != 0 se o `mapa.json` faltar ou for invalido.

## Como o painel le o mapa

- `meta.repo`, `meta.commits` e `meta.branches` vem do `gerar.py`: so entram
  hash e branch citados no mapa que **existem** no repo, para nao sair link morto.
- Toda exibicao de texto passa por `linkify()`, que monta nos (nunca `innerHTML`:
  o mapa tem texto de terceiros). `#NNN` vira issue/PR, hash de `meta.commits` vira
  commit, branch de `meta.branches` vira tree, `http(s)` vira link.
- O painel recarrega `data.json` e `/api/respostas` a cada 30 s e mostra
  "atualizado ha X s". A recarga nao perde o que esta sendo digitado nem o scroll
  (rascunho por id de decisao, foco e cursor do textarea, scroll das caixas).
  Sem a API, avisa e fica so leitura.

## Publicar

    tools/painel-issues/publicar.sh [--dry-run] [worktree]

Gera o site, envia por rsync/ssh para `zimaos-lan:/DATA/AppData/nuvio-painel/html/`
e garante o container `nuvio-painel` (nginx:alpine, porta 8094). Nunca toca outros
containers; aborta se a 8094 estiver ocupada por outra coisa. Padrao de worktree:
`/Volumes/ExternalSSD/nv-2031-int`.

## Acesso

Somente LAN (`http://192.168.1.20:8094`) ou Tailscale. Nao expor publicamente:
as notas contem detalhes internos.

## Respostas do dono (Aceitar/Negar + informacao adicional)

Cada decisao em `docs/issues/mapa.json` tem um `id` estavel (`dec-...`, validado
por `docs/issues/mapa.py`). O cartao de cada decisao **pendente** mostra, sem o
dono precisar procurar em outro lugar:

- a pergunta e a recomendacao;
- a release afetada (campo `release` da decisao ou, na falta, o alvo das issues ligadas);
- as issues ligadas com titulo, status, alvo e plataforma (link para o GitHub), e o
  item de roadmap que cita essas issues;
- as validacoes da auditoria dessas issues, com o resultado colorido;
- os commits e branches citados, como link.

Tem **Aceitar** e **Negar** e o campo **Informacao adicional** (ate 2000
caracteres), que vai junto da resposta para a coordenacao usar como contexto no
proximo prompt. Depois de responder, o cartao mostra a resposta, a hora e
"aguardando a coordenacao aplicar". Decisoes com `estado` `decidida` vao para a
lista **Decididas**, com decisao, data e quem. A resposta pode ser alterada ate a
coordenacao preencher `aplicada_em` (AAAA-MM-DD). Sem a API, o painel fica so leitura.

- API: `api/servidor.py` (stdlib), `POST/GET /api/respostas`, grava `/data/respostas/respostas.json` (no ZimaOS: `/DATA/AppData/nuvio-painel/respostas/`; o `html/` entra no container so leitura, e a API roda sem root)
  (historico por id, ultima = atual). **Sem autenticacao**: so LAN/Tailscale, como o painel;
  a defesa e o cabecalho Origin (origens do painel) + Content-Type JSON.
- Origens aceitas: `PAINEL_ORIGENS` (lista separada por virgula; padrao `http://192.168.1.20:8094,http://100.77.116.81:8094`).
  Outros ambientes: `PAINEL_DATA_JSON`, `PAINEL_RESPOSTAS`, `PAINEL_HOST`, `PAINEL_PORTA`.
- Implantar (precisa de OK do dono): `api/implantar.sh` mostra o plano; `--aplicar` executa. Nenhum container existente
  e apagado ou renomeado: os novos tem nome versionado, o nginx antigo so e parado e e religado se algo falhar ou o
  script for interrompido; reexecutar religa o ultimo bom primeiro. `api/implantar.sh --limpar` so lista o que removeria;
  `--limpar --sim` confirma e remove os parados antigos (nunca o legado `nuvio-painel` nem o ultimo bom).
  A API roda como uid 1000 (a pasta `respostas/` recebe chown); `respostas.json` do caminho antigo e copiado para `respostas/`.
  Uma execucao por vez (trava `.deploy.lock` com pid; dono morto = retomada). Os antigos seguem servindo ate a troca; durante
  a validacao na porta temporaria o alias `nuvio-painel-api` aponta para a API antiga e a nova (a nova ja passou pelo readiness).
- Testes: `tests/api-respostas.sh` (API, mapa.py e `tests/painel-js.mjs`, o polling do `index.html` sem navegador)
  e `tests/implantar-shim.sh` (deploy e `publicar.sh` com docker falso).
- Ler as respostas: `tools/painel-issues/respostas.sh baixar` -> `docs/issues/respostas-dono.json`;
  depois `python3 docs/issues/mapa.py` mostra "resposta do dono" no MAPA.md. `publicar.sh` ja baixa antes de gerar.
- Teste local: `tools/painel-issues/tests/api-respostas.sh`.

- API: `api/servidor.py` (stdlib), `POST/GET /api/respostas`, grava `/data/respostas/respostas.json` (no ZimaOS: `/DATA/AppData/nuvio-painel/respostas/`; o `html/` entra no container so leitura, e a API roda sem root)
  (historico por id, ultima = atual). **Sem autenticacao**: so LAN/Tailscale, como o painel;
  a defesa e o cabecalho Origin (origens do painel) + Content-Type JSON.
- Origens aceitas: `PAINEL_ORIGENS` (lista separada por virgula; padrao `http://192.168.1.20:8094,http://100.77.116.81:8094`).
  Outros ambientes: `PAINEL_DATA_JSON`, `PAINEL_RESPOSTAS`, `PAINEL_HOST`, `PAINEL_PORTA`.
- Implantar (precisa de OK do dono): `api/implantar.sh` mostra o plano; `--aplicar` executa. Nenhum container existente
  e apagado ou renomeado: os novos tem nome versionado, o nginx antigo so e parado e e religado se algo falhar ou o
  script for interrompido; reexecutar religa o ultimo bom primeiro. `api/implantar.sh --limpar` so lista o que removeria;
  `--limpar --sim` confirma e remove os parados antigos (nunca o legado `nuvio-painel` nem o ultimo bom).
  A API roda como uid 1000 (a pasta `respostas/` recebe chown); `respostas.json` do caminho antigo e copiado para `respostas/`.
  Uma execucao por vez (trava `.deploy.lock` com pid; dono morto = retomada). Os antigos seguem servindo ate a troca; durante
  a validacao na porta temporaria o alias `nuvio-painel-api` aponta para a API antiga e a nova (a nova ja passou pelo readiness).
- Testes: `tests/api-respostas.sh` (API, mapa.py e `tests/painel-js.mjs`, o polling do `index.html` sem navegador)
  e `tests/implantar-shim.sh` (deploy e `publicar.sh` com docker falso).
- Ler as respostas: `tools/painel-issues/respostas.sh baixar` -> `docs/issues/respostas-dono.json`;
  depois `python3 docs/issues/mapa.py` mostra "resposta do dono" no MAPA.md. `publicar.sh` ja baixa antes de gerar.
- Teste local: `tools/painel-issues/tests/api-respostas.sh`.
