-- Registros de sessao que morreu, enviados pela pessoa com o botao "Enviar
-- registro" (avisos.h no cliente). Sem token, sem IP: o texto e o log do app,
-- que ja sai sem credencial. Retencao de 30 dias na limpeza diaria.
CREATE TABLE IF NOT EXISTS registro (
  id         INTEGER PRIMARY KEY AUTOINCREMENT,
  pessoa     TEXT NOT NULL,
  versao     TEXT NOT NULL DEFAULT '',
  plataforma TEXT NOT NULL DEFAULT '',
  quando     TEXT NOT NULL DEFAULT '',
  texto      TEXT NOT NULL DEFAULT '',
  criado     INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS registro_criado ON registro (criado);
