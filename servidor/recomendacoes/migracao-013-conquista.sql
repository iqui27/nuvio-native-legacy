-- Caca a filmografia e conquistas (plano social-pessoal, 10.1/10.3, Onda 2).
-- NAO APLICADA. Aplicar com:
--   wrangler d1 execute nuvio-recomendacoes --remote --file migracao-013-conquista.sql
-- A parte de review/celular/retro da 013 do plano e da Onda 3 e entra depois.

-- Filmografia classificada, compartilhada entre todos (cache do TMDB, 14 dias).
-- IF NOT EXISTS: a 011 do Diario tambem pode cria-la.
CREATE TABLE IF NOT EXISTS filmografia (
  pessoa INTEGER NOT NULL, papel TEXT NOT NULL,    -- dir | ator
  corpo TEXT NOT NULL,                             -- JSON pronto
  atualizado INTEGER NOT NULL,
  PRIMARY KEY (pessoa, papel)
);

-- Uma linha por alvo cacado, so no desbloqueio. nivel 1..5 (Primeiro passo ..
-- Em ordem), nunca desce. visivel 1 = amigos veem (com alcance >= 1 na leitura).
CREATE TABLE IF NOT EXISTS conquista (
  pessoa TEXT NOT NULL, chave TEXT NOT NULL, nivel INTEGER NOT NULL,
  quando INTEGER NOT NULL, visivel INTEGER NOT NULL DEFAULT 0,
  nome TEXT NOT NULL DEFAULT '', vistos INTEGER NOT NULL DEFAULT 0, total INTEGER NOT NULL DEFAULT 0,
  PRIMARY KEY (pessoa, chave)
);
CREATE INDEX IF NOT EXISTS conquista_recente ON conquista(pessoa, visivel, quando);
