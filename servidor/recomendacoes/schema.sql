-- Esquema do servico de recomendacoes entre amigos.
-- Nada aqui guarda token, e-mail ou IP: so o identificador estavel da pessoa.

CREATE TABLE IF NOT EXISTS pessoa (
  id     TEXT PRIMARY KEY,          -- "trakt:<slug>" | "nuvio:<sub>"
  nome   TEXT NOT NULL DEFAULT '',
  codigo TEXT UNIQUE,               -- 6 chars a-z0-9, para pareamento
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
