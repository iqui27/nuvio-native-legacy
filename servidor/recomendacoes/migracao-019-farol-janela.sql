-- FAROL 019: a coluna `janela` da farol_registro chegou na 018 so pelo
-- CREATE TABLE IF NOT EXISTS, que num banco que ja tinha a 018 de 705a519ac
-- nao altera a tabela existente — e o CREATE UNIQUE INDEX que a usava falhava
-- com "no such column: janela" e deixava a rota sem gravar. O ALTER cobre o
-- banco antigo; num banco novo (ou na reaplicacao) a coluna ja existe e o
-- "duplicate column name" e o sinal de "ja aplicada" que preparar-local.sh
-- tolera para todas as migracoes. Sem perda: a coluna e so informativa — o
-- dedupe atomico de (pessoa_h, fp) nao a usa mais — e o UPDATE repoe o dia
-- UTC de criacao nas linhas antigas.
ALTER TABLE farol_registro ADD COLUMN janela INTEGER NOT NULL DEFAULT 0;
UPDATE farol_registro SET janela = criado / 86400 WHERE janela = 0;
