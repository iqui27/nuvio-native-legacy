-- Esquema do servico de recomendacoes entre amigos.
-- Nada aqui guarda token, e-mail ou IP: so o identificador estavel da pessoa.

CREATE TABLE IF NOT EXISTS pessoa (
  id     TEXT PRIMARY KEY,          -- "trakt:<slug>" | "nuvio:<sub>"
  nome   TEXT NOT NULL DEFAULT '',
  codigo TEXT UNIQUE,               -- 6 chars a-z0-9, para pareamento
  -- FOTO DE PERFIL, capturada na verificacao de identidade e nunca enviada
  -- pelo cliente: quem diz qual e a cara de alguem e quem emitiu o token.
  -- Vazio e o estado NORMAL da conta Nuvio (o Supabase nao devolve foto na
  -- verificacao) — a TV cai na inicial num disco colorido, como perfilsel.c.
  avatar TEXT NOT NULL DEFAULT '',
  -- "ACEITO APARECER PARA OUTRAS PESSOAS". 0 = nao, e 0 e o padrao de todo
  -- mundo, inclusive de quem ja estava no banco antes da coluna existir (ver
  -- migracao-002). So vira 1 por um POST /v1/descobrivel autenticado COM O
  -- TOKEN DA PROPRIA PESSOA — a rota nao aceita id de terceiro, entao nao ha
  -- pedido possivel que ligue o sinalizador de outra pessoa.
  --
  -- O QUE ELE GOVERNA, e so isto: se a pessoa pode APARECER na lista de
  -- sugestoes de alguem. Nao governa receber recomendacao, nao governa ser
  -- encontrada pelo codigo de pareamento (quem dita o codigo ja escolheu ser
  -- achado) e nao governa a lista de contatos que ela ja tem.
  descobrivel INTEGER NOT NULL DEFAULT 0,
  criado INTEGER NOT NULL,
  visto  INTEGER NOT NULL
);

-- Simetrico de proposito: vincular grava as DUAS linhas, bloquear apaga as
-- duas. Ler "somos contatos?" vira um SELECT de chave primaria.
CREATE TABLE IF NOT EXISTS contato (
  a      TEXT NOT NULL,
  b      TEXT NOT NULL,
  criado INTEGER NOT NULL,
  PRIMARY KEY (a, b)
);

CREATE TABLE IF NOT EXISTS rec (
  id     INTEGER PRIMARY KEY AUTOINCREMENT,
  de     TEXT NOT NULL,
  para   TEXT NOT NULL,
  criado INTEGER NOT NULL,
  imdb   TEXT NOT NULL,
  tipo   TEXT NOT NULL DEFAULT 'movie',
  titulo TEXT NOT NULL DEFAULT '',
  poster TEXT NOT NULL DEFAULT '',
  ano    TEXT NOT NULL DEFAULT '',
  modelo INTEGER NOT NULL DEFAULT 0,   -- indice do template; -1 = texto livre
  texto  TEXT NOT NULL DEFAULT '',
  -- NOTA DO IMDb EM CENTESIMOS (83 = 8,3), como o `nota` do CatItem. Viaja COM
  -- a recomendacao porque quem recebe pode nao ter o titulo no catalogo dele —
  -- que e exatamente o caso que uma recomendacao cobre. 0 = desconhecida.
  nota   INTEGER NOT NULL DEFAULT 0,
  visto  INTEGER NOT NULL DEFAULT 0,
  aberto INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS rec_para ON rec(para, id);
CREATE INDEX IF NOT EXISTS rec_de   ON rec(de, criado);

-- Cache da verificacao de identidade. Sem ele cada TV bateria no Trakt (ou no
-- Supabase do Nuvio) uma vez por minuto so para dizer quem e. Guarda o SHA-256
-- do token, nunca o token.
CREATE TABLE IF NOT EXISTS sessao (
  hash   TEXT PRIMARY KEY,
  id     TEXT NOT NULL,
  nome   TEXT NOT NULL DEFAULT '',
  expira INTEGER NOT NULL
);

-- Ver migracao-003-registro.sql (registro de sessao que morreu).
-- Ver migracao-005-amigos.sql (perfil publico, pedidos, bloqueio, atividade):
-- as tabelas dela sao todas CREATE IF NOT EXISTS e tambem valem para um banco
-- novo, entao `preparar-local.sh` aplica o arquivo inteiro apos este esquema.

-- Ver migracao-011-diario.sql (diario por perfil, cofre e pendentes do
-- Letterboxd). `preparar-local.sh` aplica todo migracao-*.sql apos este esquema.

-- Ver migracao-015-ident-hora.sql (contador de verificacao de identidade).
-- Ver migracao-018-farol.sql (registro estruturado do Farol e correcoes
-- conhecidas); ids da farol_registro comecam em 2^29 para o codigo de 6
-- caracteres nao colidir com o de `registro`. Em banco que ja tinha a 018
-- sem a coluna `janela`, quem a cria e a migracao-019-farol-janela.sql.
CREATE TABLE IF NOT EXISTS farol_registro (
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
CREATE INDEX IF NOT EXISTS farol_fp ON farol_registro (fp, criado);
CREATE INDEX IF NOT EXISTS farol_criado ON farol_registro (criado);
-- Dedupe por (pessoa_h, fp) na janela MOVEL de 24 h: o slot vivo mora na
-- farol_dedup (decidido no ON CONFLICT do src/farol.js), porque uma
-- ocorrencia passadas 24 h e registro NOVO e o recibo anterior fica aqui ate
-- a retencao. A fusao e a MESMA da migracao-018, copiada de la: so reenvios
-- da MESMA janela somam `vezes` na ancora e a janela e FIXA no criado dela —
-- as ancoras sao achadas pela CTE recursiva, entao ocorrencias de janelas
-- distintas ficam intactas, com recibo e JSON (a versao anterior decidia pela
-- linha imediatamente anterior e apagava dias legitimos).
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
DROP INDEX IF EXISTS farol_dedup;
CREATE TABLE IF NOT EXISTS farol_dedup (
  pessoa_h   TEXT NOT NULL,
  fp         TEXT NOT NULL,
  criado     INTEGER NOT NULL,
  registro   INTEGER,
  confirmado INTEGER NOT NULL DEFAULT 0,
  toques     INTEGER NOT NULL DEFAULT 0,
  PRIMARY KEY (pessoa_h, fp)
);
INSERT OR IGNORE INTO farol_dedup (pessoa_h, fp, criado, registro, confirmado, toques)
  SELECT r.pessoa_h, r.fp, r.criado, r.id, 1, r.vezes
  FROM farol_registro AS r
  WHERE NOT EXISTS (
    SELECT 1 FROM farol_registro AS n
    WHERE n.pessoa_h = r.pessoa_h AND n.fp = r.fp
      AND (n.criado > r.criado OR (n.criado = r.criado AND n.id > r.id)));
-- Ids a partir de 2^29, reservados ANTES da primeira gravacao.
INSERT INTO sqlite_sequence (name, seq)
  SELECT 'farol_registro', MAX(536870912, COALESCE((SELECT MAX(id) FROM farol_registro), 0))
  WHERE NOT EXISTS (SELECT 1 FROM sqlite_sequence WHERE name = 'farol_registro');

CREATE TABLE IF NOT EXISTS farol_conhecido (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  fp TEXT, codigo TEXT, plataforma TEXT, so_maior INTEGER,
  estado TEXT NOT NULL,
  versao TEXT NOT NULL DEFAULT '',
  nota_pt TEXT NOT NULL DEFAULT '', nota_en TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS farol_conhecido_fp ON farol_conhecido (fp);
CREATE INDEX IF NOT EXISTS farol_conhecido_cod ON farol_conhecido (codigo, plataforma, so_maior);

CREATE TABLE IF NOT EXISTS ident_hora (
  hora   INTEGER PRIMARY KEY,
  verif  INTEGER NOT NULL DEFAULT 0,
  recusa INTEGER NOT NULL DEFAULT 0,
  local  INTEGER NOT NULL DEFAULT 0
);
