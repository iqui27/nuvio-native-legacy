-- Contador de verificacao de identidade por hora (11/10/2026): o Nuvio bloqueou
-- o worker quando 86% das chamadas a /auth/v1/user eram tokens vencidos. Isto
-- deixa a taxa visivel antes de alguem reclamar. Guarda so contagens.
--   npx wrangler d1 execute nuvio-recomendacoes --remote --file=migracao-015-ident-hora.sql
--   SELECT datetime(hora,'unixepoch'), verif, recusa, local FROM ident_hora ORDER BY hora DESC LIMIT 24;
CREATE TABLE IF NOT EXISTS ident_hora (
  hora   INTEGER PRIMARY KEY,   -- inicio da hora, unix
  verif  INTEGER NOT NULL DEFAULT 0,
  recusa INTEGER NOT NULL DEFAULT 0,
  local  INTEGER NOT NULL DEFAULT 0
);
