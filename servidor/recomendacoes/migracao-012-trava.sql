-- MIGRACAO 012 — trava de serie (social & pessoal, Onda 1; amigos na Onda 4).
-- Plano: docs/plans/social-pessoal/social-pessoal-plano.md, secoes 5 e 10.2.
-- Rotas: src/trava.js. Estado: docs/plans/social-pessoal/STATUS-trava.md.
--
-- NAO APLICADA EM PRODUCAO. Ordem obrigatoria: ESTA MIGRACAO PRIMEIRO, depois
-- o `wrangler deploy` do worker. Contra o banco velho o worker novo responde
-- 500 so nas rotas /v1/trava*; /v1/rec continua (travaRev cai para 0). Da raiz
-- do repositorio:
--
--   npx wrangler@4 d1 execute nuvio-recomendacoes --remote \
--     --config servidor/recomendacoes/wrangler.toml \
--     --file servidor/recomendacoes/migracao-012-trava.sql
--
-- SO ADITIVA E IDEMPOTENTE (IF NOT EXISTS). Worker velho com este banco segue
-- funcionando: ele nao conhece as tabelas novas.
--
-- NUMERACAO: 010 e do Watch Together, 011 e do diario (e 011-meta das
-- conquistas). Esta e a 012 do plano.

-- A trava. `id` = 128 bits aleatorios em hex. `serie` = id do titulo sem :T:E.
-- `modo` 0 = juntos, 1 = separados. `pin` 1 = "Assistir mesmo assim" pede PIN
-- (quem confere o PIN e a TV, pela conta Nuvio; o servidor so guarda a escolha).
CREATE TABLE IF NOT EXISTS trava (
  id         TEXT PRIMARY KEY,
  serie      TEXT NOT NULL,
  titulo     TEXT NOT NULL DEFAULT '',
  poster     TEXT NOT NULL DEFAULT '',
  modo       INTEGER NOT NULL,
  pin        INTEGER NOT NULL DEFAULT 0,
  criador    TEXT NOT NULL,
  criado     INTEGER NOT NULL,
  atualizado INTEGER NOT NULL,
  pausada    INTEGER NOT NULL DEFAULT 0
);

-- Membros e a POSICAO de cada um: o episodio mais adiantado visto (temporada,
-- episodio; 0,0 = nao comecou). `estado` 0 convidado (amigos, Onda 4), 1
-- dentro, 2 saiu. A posicao so e lida pelos OUTROS membros desta trava.
CREATE TABLE IF NOT EXISTS trava_membro (
  trava     TEXT NOT NULL,
  pessoa    TEXT NOT NULL,
  estado    INTEGER NOT NULL DEFAULT 0,
  temporada INTEGER NOT NULL DEFAULT 0,
  episodio  INTEGER NOT NULL DEFAULT 0,
  passo_em  INTEGER NOT NULL DEFAULT 0,
  PRIMARY KEY (trava, pessoa)
);
CREATE INDEX IF NOT EXISTS trava_membro_pessoa ON trava_membro(pessoa, estado);

-- "Pedir licenca" entre amigos (Onda 4). Criada agora para a 012 ser uma so;
-- nenhuma rota desta onda escreve aqui.
CREATE TABLE IF NOT EXISTS trava_licenca (
  trava    TEXT NOT NULL,
  de       TEXT NOT NULL,
  ep       TEXT NOT NULL,
  criado   INTEGER NOT NULL,
  resposta INTEGER NOT NULL DEFAULT -1,
  PRIMARY KEY (trava, de, ep)
);

-- A REVISAO por pessoa: sobe a cada mudanca de qualquer trava em que ela esta.
-- /v1/rec (ja sondado a cada 60 s com ETag) devolve `travaRev`; a TV so faz
-- GET /v1/travas quando ele muda. Uma linha por pessoa, lida por chave.
CREATE TABLE IF NOT EXISTS trava_rev (
  pessoa TEXT PRIMARY KEY,
  rev    INTEGER NOT NULL DEFAULT 0
);
