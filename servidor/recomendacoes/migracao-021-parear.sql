-- MIGRACAO 021 — codigo de 6 digitos para entrar na TV de outra pessoa.
-- Rotas: src/parear.js. Cliente: src/parear.h, src/parearrede.c, src/login.c.
--
-- NAO APLICADA EM PRODUCAO. Ordem: ESTA MIGRACAO PRIMEIRO, depois definir
-- PAREAR_BASES e o `wrangler deploy`. Sem a tabela ou sem a variavel as rotas
-- /v1/parear* respondem 503 e a TV simplesmente nao mostra o codigo. Da raiz:
--
--   npx wrangler@4 d1 execute nuvio-recomendacoes --remote \
--     --config servidor/recomendacoes/wrangler.toml \
--     --file servidor/recomendacoes/migracao-021-parear.sql
--
-- SO ADITIVA E IDEMPOTENTE. O worker velho nao conhece a tabela.
--
-- `codigo` = 6 digitos; `url` = a pagina de autorizacao do login da TV (so
-- prefixos de PAREAR_BASES entram); `expira` = segundos Unix. A linha some na
-- limpeza diaria e e reaproveitada quando vencida.
CREATE TABLE IF NOT EXISTS parear (
  codigo TEXT PRIMARY KEY,
  url    TEXT NOT NULL,
  expira INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS parear_url ON parear (url);
