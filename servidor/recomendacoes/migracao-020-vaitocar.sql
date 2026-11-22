-- Vai tocar nesta TV (pesquisa de mercado 4.2): registro ANONIMO de
-- compatibilidade. So CONTAGEM por combinacao tecnica; nao ha linha por
-- pessoa, conta, sessao ou IP, nem titulo, hash ou endereco (a gramatica de
-- cada coluna esta em src/vtreg.js, espelho de src/vtreg.h do app).
-- `dia` e o dia UTC (epoch/86400): permite ver a evolucao por firmware e
-- apagar o que passou da retencao.
--   wrangler d1 execute nuvio-rec --remote --file migracao-020-vaitocar.sql
CREATE TABLE IF NOT EXISTS vaitocar (
  dia        INTEGER NOT NULL,
  plataforma TEXT NOT NULL,
  modelo     TEXT NOT NULL,
  firmware   TEXT NOT NULL,
  container  TEXT NOT NULL,
  video      TEXT NOT NULL,
  audio      TEXT NOT NULL,
  hdr        TEXT NOT NULL,
  mbps       INTEGER NOT NULL,
  altura     INTEGER NOT NULL,
  resultado  TEXT NOT NULL,
  trocou     INTEGER NOT NULL,
  n          INTEGER NOT NULL DEFAULT 1,
  PRIMARY KEY (dia, plataforma, modelo, firmware, container, video, audio, hdr, mbps, altura, resultado, trocou)
) WITHOUT ROWID;
CREATE INDEX IF NOT EXISTS vaitocar_modelo ON vaitocar (plataforma, modelo, video, hdr);
