-- Estado da triagem automatica dos registros (tools/triagem.py, rodada pela
-- Action .github/workflows/triagem.yml). Fica AQUI, e nao no repositorio,
-- porque o repositorio e publico e os registros trazem ids de pessoa; o que
-- sai para o GitHub e so o agregado (contagens por padrao/versao/plataforma).
--
-- Uma linha por (rodada, padrao, versao, plataforma). `ultimo_log` e o maior
-- registro.id que a rodada leu: a proxima comeca dele.
CREATE TABLE IF NOT EXISTS triagem (
  id          INTEGER PRIMARY KEY AUTOINCREMENT,
  criado      INTEGER NOT NULL,
  ultimo_log  INTEGER NOT NULL,
  padrao      TEXT NOT NULL,
  versao      TEXT NOT NULL DEFAULT '',
  plataforma  TEXT NOT NULL DEFAULT '',
  ocorrencias INTEGER NOT NULL DEFAULT 0,
  pessoas     INTEGER NOT NULL DEFAULT 0,
  issue       INTEGER,
  estado      TEXT NOT NULL DEFAULT 'novo'
);
CREATE INDEX IF NOT EXISTS triagem_padrao ON triagem (padrao, versao);
CREATE INDEX IF NOT EXISTS triagem_ultimo ON triagem (ultimo_log);
