#!/usr/bin/env bash
# Prepara o D1 LOCAL para `wrangler dev` e para teste.sh.
#
# POR QUE ELE EXISTE. teste.sh sempre assumiu que as identidades `nuvio:aaa` e
# `nuvio:bbb` ja respondiam aos tokens `tok-a` e `tok-b` — a verificacao de
# identidade e o UNICO ponto do servico que fala com a rede externa (Trakt /
# Supabase), e ela e curto-circuitada gravando a sessao direto no banco, que e
# exatamente o que o cache faria depois da primeira verificacao de verdade. Essa
# semeadura morava na cabeca de quem escreveu o teste; com cinco identidades
# (sugestoes precisam de uma roda de amigos) isso deixou de caber num comentario.
#
# ELE NAO TOCA EM NADA REMOTO. Todas as chamadas levam `--local`: o banco
# alterado e o SQLite do miniflare em .wrangler/state/, que nasce e morre com
# esta maquina.
#
#   bash servidor/recomendacoes/preparar-local.sh
#   npx wrangler@4 dev --local --port 8799 --config servidor/recomendacoes/wrangler.toml
#   bash servidor/recomendacoes/teste.sh
set -u
cd "$(dirname "$0")/../.."
CFG=servidor/recomendacoes/wrangler.toml
W="npx wrangler@4 d1 execute nuvio-recomendacoes --local --config $CFG"

exec_arquivo() { $W --file "$1" > /tmp/nv-d1.log 2>&1; }
exec_sql()     { $W --command "$1" > /tmp/nv-d1.log 2>&1; }

echo "== esquema"
exec_arquivo servidor/recomendacoes/schema.sql || { tail -20 /tmp/nv-d1.log; exit 1; }

# AS MIGRACOES PODEM FALHAR AQUI, E ISSO E O ESPERADO. `schema.sql` acima ja
# cria a tabela COM as colunas novas num banco vazio, entao o ALTER TABLE
# responde "duplicate column name". Num banco antigo (criado por um schema.sql
# de antes) ele e que faz o trabalho. Uma das duas sempre e redundante, e nao
# ha como saber qual sem perguntar ao banco — perguntar custaria mais codigo do
# que ignorar o erro que so pode ter esta causa.
for m in servidor/recomendacoes/migracao-*.sql; do
  echo "== $(basename "$m")"
  exec_arquivo "$m" || echo "   (ja aplicada)"
done

# AS CINCO IDENTIDADES. O hash e o SHA-256 de "<via>:<token>", o mesmo que
# quemE() calcula; `expira` bem no futuro para o teste nao depender do relogio.
#
#   tok-a -> nuvio:aaa  Henrique    tok-d -> nuvio:ddd  Daniel
#   tok-b -> nuvio:bbb  Gustavo     tok-e -> nuvio:eee  Elisa
#   tok-c -> nuvio:ccc  Carolina    tok-t -> trakt:pedrinho  Pedrinho
#
# A SEXTA E DE TRAKT, e a diferenca nao e cosmetica: a primeira fonte de
# sugestao so olha ids que comecam com "trakt:" (e o que um slug do Trakt vira),
# entao sem uma identidade dessas esse ramo inteiro ficaria sem teste. O hash
# dela e o de "trakt:tok-t", com o prefixo da VIA, e o pedido tem de mandar
# `x-nuvio-auth: trakt`.
echo "== sessoes"
exec_sql "
DELETE FROM sessao;
INSERT INTO sessao (hash, id, nome, expira) VALUES
 ('7dff4dfa9cf8d6ef75ac848f15b6e4560baf9b5a27920a28fb9e9a0d0fe56623','nuvio:aaa','Henrique',9999999999),
 ('d5446ac7de7ef1a4d40fedd4e192ed4eca6a73898735cd3da689282d4a3b3a36','nuvio:bbb','Gustavo',9999999999),
 ('95f2937f171b75f0d1591ff4458254979b8b59b982d61595b86862dbe9cbffa0','nuvio:ccc','Carolina',9999999999),
 ('7e4ba5a52843391d31227f027f5313a438abef42894010aa09f2a1a9372fe351','nuvio:ddd','Daniel',9999999999),
 ('8e25cb48137fb326f871eee317e055db8e5e07d62ea06dbbe11c7446d260fc64','nuvio:eee','Elisa',9999999999),
 ('a8cbd03defa6a0b09930e16fb608647fc347f04ede826c7e1ab8a1ac60a22a60','trakt:pedrinho','Pedrinho',9999999999);
" || { tail -20 /tmp/nv-d1.log; exit 1; }

# ESTADO LIMPO A CADA RODADA. teste.sh conta recomendacoes ("B recebe 1") e
# esbarra no limite por par (5 por dia): rodar duas vezes seguidas sem apagar
# faria a segunda falhar por um limite que esta CERTO, e um teste que so passa
# na primeira execucao e um teste que ninguem roda.
echo "== zerando dados de teste"
exec_sql "DELETE FROM rec; DELETE FROM contato; DELETE FROM pessoa;" \
  || { tail -20 /tmp/nv-d1.log; exit 1; }

echo "pronto."
