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
