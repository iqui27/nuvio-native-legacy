-- MIGRACAO 010 — Watch Together ("Assistir Juntos"): salas, convites e as
-- regras do perfil infantil. Rotas: src/sala.js. A sala em si (relogio,
-- estados, chat) vive no Durable Object do Worker nuvio-juntos; aqui fica so o
-- necessario para o CONVITE chegar e para conferir quem pode entrar.
--
-- NAO APLICADA EM PRODUCAO. Ordem obrigatoria: ESTA MIGRACAO PRIMEIRO, depois o
-- deploy deste worker (de master!) e do nuvio-juntos. Da raiz do repositorio:
--
--   npx wrangler@4 d1 execute nuvio-recomendacoes --remote \
--     --config servidor/recomendacoes/wrangler.toml \
--     --file servidor/recomendacoes/migracao-010-juntos.sql
--
-- SO ADITIVA E IDEMPOTENTE (IF NOT EXISTS). Worker velho com este banco segue
-- funcionando: ele nao conhece as tabelas novas.

-- Uma linha por sala viva. `imp` e a impressao do release (JSON: hash, idx,
-- bytes, arquivo, binge, altura, durMs) para o convidado ver o veredito da
-- fonte ANTES de entrar; nunca URL. Apagada quando a sala expira (scheduled)
-- ou quando o anfitriao encerra (POST /v1/sala/fim).
CREATE TABLE IF NOT EXISTS sala (
  id        TEXT PRIMARY KEY,           -- 128 bits aleatorios (hex)
  codigo    TEXT NOT NULL,              -- 6 caracteres, Crockford base32
  anfitriao TEXT NOT NULL,
  ep        TEXT NOT NULL,              -- imdb[:temporada:episodio]
  titulo    TEXT NOT NULL DEFAULT '',
  poster    TEXT NOT NULL DEFAULT '',
  imp       TEXT NOT NULL DEFAULT '{}',
  controle  TEXT NOT NULL DEFAULT 'todos',
  infantil  INTEGER NOT NULL DEFAULT 0, -- sala criada por um perfil infantil
  criado    INTEGER NOT NULL,
  expira    INTEGER NOT NULL
);
CREATE UNIQUE INDEX IF NOT EXISTS sala_codigo ON sala(codigo);
CREATE INDEX IF NOT EXISTS sala_expira ON sala(expira);

-- estado: 0 pendente, 1 entrou (ou entrou por codigo), 2 recusou.
CREATE TABLE IF NOT EXISTS convite (
  sala   TEXT NOT NULL,
  para   TEXT NOT NULL,
  de     TEXT NOT NULL,
  criado INTEGER NOT NULL,
  estado INTEGER NOT NULL DEFAULT 0,
  PRIMARY KEY (sala, para)
);
CREATE INDEX IF NOT EXISTS convite_para ON convite(para, estado);

-- "Nao receber convites de X" (de = id) e "Nao receber convites" (de = '*').
CREATE TABLE IF NOT EXISTS sala_silencio (
  pessoa TEXT NOT NULL,
  de     TEXT NOT NULL,
  criado INTEGER NOT NULL,
  PRIMARY KEY (pessoa, de)
);

-- Perfis que a TV declarou infantis nas rotas /v1/sala*. Serve para "amigo de
-- um perfil ADULTO da mesma conta" (a regra do convite para criancas).
CREATE TABLE IF NOT EXISTS sala_infantil (
  pessoa TEXT PRIMARY KEY,
  visto  INTEGER NOT NULL
);
