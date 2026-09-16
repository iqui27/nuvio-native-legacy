-- MIGRACAO 001 — foto de perfil de quem manda, e a nota do IMDb do titulo.
--
-- O BANCO ESTA NO AR e a TV do dono ja esta registrada nele, entao esta
-- migracao e SO ADITIVA: duas colunas novas com DEFAULT, nenhuma coluna
-- renomeada, nenhuma apagada, nenhum indice refeito. As linhas que ja existem
-- passam a ter `avatar = ''` e `nota = 0`, que sao exatamente os dois estados
-- que o cliente ja precisa saber desenhar de qualquer jeito (amigo de conta
-- Nuvio nao tem foto; titulo sem nota nao mostra selo).
--
-- APLICAR NO BANCO DE VERDADE, da raiz do repositorio:
--
--   npx wrangler@4 d1 execute nuvio-recomendacoes --remote \
--     --config servidor/recomendacoes/wrangler.toml \
--     --file servidor/recomendacoes/migracao-001-avatar-nota.sql
--
-- E no D1 LOCAL do teste (`--local` no lugar de `--remote`). Depois dela,
-- `npx wrangler@4 deploy` publica o Worker que usa as colunas — nesta ordem:
-- o codigo novo le `p.avatar` e `r.nota`, e contra a tabela velha isso e um
-- erro de SQL em TODA sondagem de TODA TV.
--
-- RODAR UMA VEZ SO. `ALTER TABLE ... ADD COLUMN` nao tem "IF NOT EXISTS" no
-- SQLite: a segunda execucao responde "duplicate column name" e nao altera
-- nada. Esse erro e o sinal de que a migracao ja passou, nao de que quebrou.

ALTER TABLE pessoa ADD COLUMN avatar TEXT NOT NULL DEFAULT '';
ALTER TABLE rec    ADD COLUMN nota INTEGER NOT NULL DEFAULT 0;
