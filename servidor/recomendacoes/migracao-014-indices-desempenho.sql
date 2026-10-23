-- #203: lentidao do Worker (>12 s entre 22h e 1h). NAO APLICADA — rodar so com o
-- aval do dono:
--   npx wrangler d1 execute nuvio-recomendacoes --remote --file=migracao-014-indices-desempenho.sql
-- As tabelas do servico sao pequenas (pessoa 1,2 mil linhas, rec 13); o que pesa
-- e `registro` (38 mil linhas, 5,6 GB). Estes indices so tiram varredura de
-- linhas de `registro` e de `agregado_titulo`. Criar o primeiro percorre a
-- tabela uma vez (~38 mil leituras de linha); fazer fora do horario de pico.

-- (criado, pessoa) COBRE a contagem de rotaArranque
--   SELECT COUNT(*) FROM registro WHERE pessoa LIKE 'arranque:%' AND criado > ?
-- e a busca do "(auto)" anterior em rotaRegistro: hoje cada uma le ~14 mil
-- linhas da tabela (plano: SEARCH registro USING INDEX registro_criado), com o
-- indice novo nao toca na tabela.
CREATE INDEX IF NOT EXISTS registro_criado_pessoa ON registro (criado, pessoa);

-- amigos.js: SELECT pessoa, imdb FROM agregado_titulo WHERE imdb IN (...)
-- hoje e SCAN agregado_titulo.
CREATE INDEX IF NOT EXISTS agregado_titulo_imdb ON agregado_titulo (imdb);
