-- FAROL (PLANO.md secao 2.7): registro estruturado enviado pela TV e tabela
-- de correcoes conhecidas, mantida pelo dono. `pessoa_h` e HMAC(FAROL_SAL, id):
-- limita abuso, nao identifica. Retencao de 30 dias na limpeza diaria.
CREATE TABLE IF NOT EXISTS farol_registro (
  id         INTEGER PRIMARY KEY AUTOINCREMENT,
  criado     INTEGER NOT NULL,
  janela     INTEGER NOT NULL DEFAULT 0,  -- dia (criado/86400), informativo
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
-- Dedupe atomico (issue 5): a chave e (pessoa_h, fp) SEM o dia UTC — a janela
-- movel de 24 h e decidida no ON CONFLICT da gravacao (src/farol.js), entao
-- dois envios atravessando 00:00 UTC continuam um recibo so. Num banco que ja
-- tinha linhas repetidas (a 018 sem dedupe nao impedia), a fusao soma `vezes`
-- na linha mais antiga, que guarda o recibo original.
UPDATE farol_registro AS r SET vezes = r.vezes + COALESCE((
  SELECT SUM(o.vezes) FROM farol_registro AS o
  WHERE o.pessoa_h = r.pessoa_h AND o.fp = r.fp AND o.id > r.id), 0)
WHERE EXISTS (
  SELECT 1 FROM farol_registro AS o
  WHERE o.pessoa_h = r.pessoa_h AND o.fp = r.fp AND o.id > r.id);
DELETE FROM farol_registro WHERE id NOT IN (
  SELECT MIN(id) FROM farol_registro GROUP BY pessoa_h, fp);
DROP INDEX IF EXISTS farol_dedup;
CREATE UNIQUE INDEX farol_dedup ON farol_registro (pessoa_h, fp);
-- O codigo de 6 caracteres e a MESMA bijecao de `registro` (src/codigo.js).
-- Para os dois espacos nao colidirem, os ids do Farol comecam em 2^29.
-- Repetivel: sqlite_sequence.name admite duplicata (INSERT OR IGNORE nao basta)
-- e a sequencia nunca pode DESCER nem ficar atras de um id ja gravado.
INSERT INTO sqlite_sequence (name, seq)
  SELECT 'farol_registro', MAX(536870912, COALESCE((SELECT MAX(id) FROM farol_registro), 0))
  WHERE NOT EXISTS (SELECT 1 FROM sqlite_sequence WHERE name = 'farol_registro');
UPDATE sqlite_sequence
  SET seq = MAX(seq, 536870912, COALESCE((SELECT MAX(id) FROM farol_registro), 0))
  WHERE name = 'farol_registro';

CREATE TABLE IF NOT EXISTS farol_conhecido (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  fp TEXT, codigo TEXT, plataforma TEXT, so_maior INTEGER,   -- NULL = qualquer
  estado TEXT NOT NULL,                -- 'corrigido' | 'conhecido' | 'contorno'
  versao TEXT NOT NULL DEFAULT '',     -- a partir de qual versao esta corrigido
  nota_pt TEXT NOT NULL DEFAULT '', nota_en TEXT NOT NULL DEFAULT ''
);
