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
-- Dedupe (issue 5): a janela MOVEL de 24 h nao cabe numa chave unica estatica
-- e uma ocorrencia passadas 24 h e um registro NOVO — o recibo anterior fica
-- na farol_registro ate o fim da retencao (o codigo dele continua resolvendo
-- a linha). O slot vivo de (pessoa_h, fp) mora entao na farol_dedup, decidido
-- pelo ON CONFLICT do src/farol.js.
--
-- A fusao abaixo so junta reenvios DA MESMA janela movel, nao todas as linhas
-- do par como a versao anterior (que apagava ocorrencias legitimas de dias
-- diferentes). A janela e FIXA no criado da ancora — o mesmo corte do runtime
-- (`criado > agora - 86400` sobre o criado ORIGINAL do slot farol_dedup): a
-- proxima ocorrencia >= ancora+86400 abre janela NOVA mesmo estando a menos
-- de 24 h da linha anterior, entao uma corrente de reenvios a cada 12 h NAO
-- colapsa num recibo so. Achar as ancoras e recursivo: cada uma e a primeira
-- linha do par a partir de 86400 s depois da ancora anterior.
CREATE TABLE IF NOT EXISTS farol_anc018 (id INTEGER PRIMARY KEY, base INTEGER NOT NULL);
DELETE FROM farol_anc018;
WITH RECURSIVE anc (ph, fp, id, base) AS (
  SELECT r.pessoa_h, r.fp, r.id, r.criado FROM farol_registro AS r
  WHERE NOT EXISTS (
    SELECT 1 FROM farol_registro AS s
    WHERE s.pessoa_h = r.pessoa_h AND s.fp = r.fp
      AND (s.criado < r.criado OR (s.criado = r.criado AND s.id < r.id)))
  UNION ALL
  SELECT a.ph, a.fp,
    (SELECT r.id FROM farol_registro AS r
      WHERE r.pessoa_h = a.ph AND r.fp = a.fp AND r.criado >= a.base + 86400
      ORDER BY r.criado, r.id LIMIT 1),
    (SELECT r.criado FROM farol_registro AS r
      WHERE r.pessoa_h = a.ph AND r.fp = a.fp AND r.criado >= a.base + 86400
      ORDER BY r.criado, r.id LIMIT 1)
  FROM anc AS a
  WHERE EXISTS (
    SELECT 1 FROM farol_registro AS r
    WHERE r.pessoa_h = a.ph AND r.fp = a.fp AND r.criado >= a.base + 86400))
INSERT OR IGNORE INTO farol_anc018 (id, base) SELECT id, base FROM anc;
UPDATE farol_registro AS o SET vezes = o.vezes + COALESCE((
  SELECT SUM(d.vezes) FROM farol_registro AS d, farol_anc018 AS a
  WHERE a.id = o.id AND d.pessoa_h = o.pessoa_h AND d.fp = o.fp
    AND d.criado >= a.base AND d.criado < a.base + 86400 AND d.id <> o.id), 0)
WHERE o.id IN (SELECT id FROM farol_anc018);
DELETE FROM farol_registro WHERE id NOT IN (SELECT id FROM farol_anc018);
DROP TABLE farol_anc018;
-- O indice unico da propria tabela sai de cena: (pessoa_h, fp) unico na
-- farol_registro impediria o registro novo da ocorrencia apos 24 h e ainda
-- reaproveitaria/apagaria a linha de um recibo valido.
DROP INDEX IF EXISTS farol_dedup;
CREATE TABLE IF NOT EXISTS farol_dedup (
  pessoa_h   TEXT NOT NULL,
  fp         TEXT NOT NULL,
  criado     INTEGER NOT NULL,              -- criado do registro vivo
  registro   INTEGER,                       -- id confirmado; NULL = envio em voo
  confirmado INTEGER NOT NULL DEFAULT 0,
  toques     INTEGER NOT NULL DEFAULT 0,    -- reclamacoes do slot (dedupe + retomadas)
  PRIMARY KEY (pessoa_h, fp)
);
-- O slot aponta para a linha MAIS NOVA do par — e ela que ainda recebe
-- reenvio dentro da janela movel; as anteriores ficam so com o recibo.
INSERT OR IGNORE INTO farol_dedup (pessoa_h, fp, criado, registro, confirmado, toques)
  SELECT r.pessoa_h, r.fp, r.criado, r.id, 1, r.vezes
  FROM farol_registro AS r
  WHERE NOT EXISTS (
    SELECT 1 FROM farol_registro AS n
    WHERE n.pessoa_h = r.pessoa_h AND n.fp = r.fp
      AND (n.criado > r.criado OR (n.criado = r.criado AND n.id > r.id)));
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
