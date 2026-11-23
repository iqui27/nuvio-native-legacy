-- FAROL 020 (PLANO.md secao 2.7, issue 13): garante a tabela `farol_conhecido`
-- e os indices das duas chaves de busca da ordem de match:
--   1. `fp` exato
--   2. `(codigo, plataforma, so_maior)` e `(codigo, plataforma)`
-- Repetivel em banco novo e sobre bancos que ja aplicaram 018/019.
CREATE TABLE IF NOT EXISTS farol_conhecido (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  fp TEXT,
  codigo TEXT,
  plataforma TEXT,
  so_maior INTEGER,
  estado TEXT NOT NULL,
  versao TEXT NOT NULL DEFAULT '',
  nota_pt TEXT NOT NULL DEFAULT '',
  nota_en TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS farol_conhecido_fp ON farol_conhecido (fp);
CREATE INDEX IF NOT EXISTS farol_conhecido_cod ON farol_conhecido (codigo, plataforma, so_maior);
