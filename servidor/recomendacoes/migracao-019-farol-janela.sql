-- FAROL 019: garante a coluna `janela` na farol_registro de um jeito que
-- SEMPRE termina rc 0. Num banco novo ela ja veio pelo CREATE TABLE da 018;
-- num banco que tinha a 018 de 705a519ac falta — e o ADD COLUMN unico nao e
-- repetivel: o "duplicate column name" cortava o arquivo antes do backfill e
-- deixava `janela` zerada no formato novo. O rebuild recria a tabela com a
-- coluna e recalcula o dia de `criado` direto, sem depender da coluna velha
-- existir ou nao; na reaplicacao o resultado e identico.
CREATE TABLE IF NOT EXISTS farol_registro_019 (
  id         INTEGER PRIMARY KEY AUTOINCREMENT,
  criado     INTEGER NOT NULL,
  janela     INTEGER NOT NULL DEFAULT 0,
  pessoa_h   TEXT NOT NULL,
  versao     TEXT NOT NULL,
  plataforma TEXT NOT NULL,
  so_maior   INTEGER NOT NULL DEFAULT 0,
  codigo     TEXT NOT NULL,
  fp         TEXT NOT NULL,
  fingerprint TEXT NOT NULL,
  vezes      INTEGER NOT NULL DEFAULT 1,
  json       TEXT NOT NULL
);
INSERT INTO farol_registro_019
  (id, criado, janela, pessoa_h, versao, plataforma, so_maior, codigo, fp, fingerprint, vezes, json)
  SELECT id, criado, criado / 86400, pessoa_h, versao, plataforma, so_maior, codigo, fp, fingerprint, vezes, json
  FROM farol_registro;
DROP TABLE farol_registro;
ALTER TABLE farol_registro_019 RENAME TO farol_registro;
CREATE INDEX IF NOT EXISTS farol_fp ON farol_registro (fp, criado);
CREATE INDEX IF NOT EXISTS farol_criado ON farol_registro (criado);
-- O DROP leva a entrada do sqlite_sequence junto; repoe o piso de 2^29 da
-- 018 sem nunca descer abaixo de um id ja gravado.
INSERT INTO sqlite_sequence (name, seq)
  SELECT 'farol_registro', MAX(536870912, COALESCE((SELECT MAX(id) FROM farol_registro), 0))
  WHERE NOT EXISTS (SELECT 1 FROM sqlite_sequence WHERE name = 'farol_registro');
UPDATE sqlite_sequence
  SET seq = MAX(seq, 536870912, COALESCE((SELECT MAX(id) FROM farol_registro), 0))
  WHERE name = 'farol_registro';
