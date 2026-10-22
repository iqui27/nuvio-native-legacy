-- MIGRACAO 011 — Diario por perfil (cofre opcional) e pendentes do Letterboxd.
-- Plano: docs/plans/social-pessoal/social-pessoal-plano.md (4.3, 6.5, 10.1).
-- Rotas: src/diario.js. Parte pura (CSV/ZIP/links): src/letterboxd.js.
--
-- NAO APLICADA EM PRODUCAO. Ordem obrigatoria: ESTA MIGRACAO PRIMEIRO, depois o
-- `wrangler deploy` (as rotas /v1/diario* consultam estas tabelas; contra o banco
-- velho so elas dao 500). Da raiz do repositorio:
--
--   npx wrangler@4 d1 execute nuvio-recomendacoes --remote \
--     --config servidor/recomendacoes/wrangler.toml \
--     --file servidor/recomendacoes/migracao-011-diario.sql
--
-- SO ADITIVA E IDEMPOTENTE (IF NOT EXISTS).
--
-- AS TABELAS DE META (filme_meta, filmografia) NAO ESTAO AQUI: sao de outra frente.
-- REVIEW LONGA: o texto das reviews mora na migracao 013 (onda posterior). Por ora
-- bastam `flags & 128` (tem review) e `link` (so a URL da review no Letterboxd; o
-- servidor nunca busca a pagina).

-- Uma linha por vista. `pessoa` e a identidade canonica da conta/perfil
-- (`nuvio:<sub>` ou `nuvio:<sub>:<n>`), a mesma de todo o servico.
CREATE TABLE IF NOT EXISTS diario (
  pessoa     TEXT NOT NULL,
  chave      TEXT NOT NULL,                  -- tt123 | tt123_s2e5 | tmdb:603
  quando     INTEGER NOT NULL,               -- epoch (s) do fim
  nota10     INTEGER NOT NULL DEFAULT 0,     -- 0 = sem nota; 1..10
  -- 1 rever, 2 dormiu, 4 coracao, 8 juntos, 16 importado, 32 data aproximada,
  -- 64 suspeita de sono, 128 review, 256 parcial
  flags      INTEGER NOT NULL DEFAULT 0,
  tags       INTEGER NOT NULL DEFAULT 0,     -- bitmask das 12 tags fechadas
  cor        INTEGER NOT NULL DEFAULT 0,
  seg        INTEGER NOT NULL DEFAULT 0,
  hora       INTEGER NOT NULL DEFAULT -1,    -- dia*24+hora local do inicio; -1 = nao se sabe
  origem     TEXT NOT NULL DEFAULT 'p',      -- p proprio, t trakt, c catalogo, l letterboxd, m manual
  tmdb       INTEGER NOT NULL DEFAULT 0,
  dormiu     INTEGER NOT NULL DEFAULT 0,
  dur        INTEGER NOT NULL DEFAULT 0,
  com        INTEGER NOT NULL DEFAULT 0,
  titulo     TEXT NOT NULL DEFAULT '',
  ano        INTEGER NOT NULL DEFAULT 0,
  link       TEXT NOT NULL DEFAULT '',       -- URL de review do Letterboxd, nada mais
  atualizado INTEGER NOT NULL,               -- ms; ordena a restauracao incremental
  PRIMARY KEY (pessoa, chave, quando)
);
CREATE INDEX IF NOT EXISTS diario_atualizado ON diario(pessoa, atualizado);

-- Consentimento: uma linha por pessoa que ja decidiu. ligado=0 depois de apagar.
CREATE TABLE IF NOT EXISTS diario_cofre (
  pessoa  TEXT PRIMARY KEY,
  ligado  INTEGER NOT NULL,
  quando  INTEGER NOT NULL                   -- epoch (s) da decisao
);

-- Linhas importadas do Letterboxd que ainda nao casaram com um id imdb/tmdb
-- ("Nao achei N filmes").
CREATE TABLE IF NOT EXISTS diario_lb_pendente (
  pessoa  TEXT NOT NULL,
  titulo  TEXT NOT NULL,
  ano     INTEGER NOT NULL DEFAULT 0,
  quando  INTEGER NOT NULL,
  nota10  INTEGER NOT NULL DEFAULT 0,
  flags   INTEGER NOT NULL DEFAULT 0,
  tags    INTEGER NOT NULL DEFAULT 0,
  link    TEXT NOT NULL DEFAULT '',
  criado  INTEGER NOT NULL,
  PRIMARY KEY (pessoa, titulo, ano, quando)
);
