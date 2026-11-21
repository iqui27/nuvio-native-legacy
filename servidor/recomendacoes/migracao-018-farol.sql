-- FAROL (PLANO.md secao 2.7): registro estruturado enviado pela TV e tabela
-- de correcoes conhecidas, mantida pelo dono. `pessoa_h` e HMAC(FAROL_SAL, id):
-- limita abuso, nao identifica. Retencao de 30 dias na limpeza diaria.
CREATE TABLE IF NOT EXISTS farol_registro (
  id         INTEGER PRIMARY KEY AUTOINCREMENT,
  criado     INTEGER NOT NULL,
  pessoa_h   TEXT NOT NULL,
  versao     TEXT NOT NULL,
  plataforma TEXT NOT NULL,
  so_maior   INTEGER NOT NULL DEFAULT 0,
  codigo     TEXT NOT NULL,           -- codigo do playbook (ums_204, m3_2004...)
  fp         TEXT NOT NULL,           -- 16 hex do SHA-256 do fingerprint
  fingerprint TEXT NOT NULL,
  vezes      INTEGER NOT NULL DEFAULT 1,
  json       TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS farol_fp ON farol_registro (fp, criado);
CREATE INDEX IF NOT EXISTS farol_criado ON farol_registro (criado);
-- O codigo de 6 caracteres e a MESMA bijecao de `registro` (src/codigo.js).
-- Para os dois espacos nao colidirem, os ids do Farol comecam em 2^29.
INSERT OR IGNORE INTO sqlite_sequence (name, seq) VALUES ('farol_registro', 536870912);

CREATE TABLE IF NOT EXISTS farol_conhecido (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  fp TEXT, codigo TEXT, plataforma TEXT, so_maior INTEGER,   -- NULL = qualquer
  estado TEXT NOT NULL,                -- 'corrigido' | 'conhecido' | 'contorno'
  versao TEXT NOT NULL DEFAULT '',     -- a partir de qual versao esta corrigido
  nota_pt TEXT NOT NULL DEFAULT '', nota_en TEXT NOT NULL DEFAULT ''
);
