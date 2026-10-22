#!/usr/bin/env bash
# Teste de ponta a ponta do perfil publico, busca, pedidos, bloqueio e atividade
# (src/amigos.js) contra `wrangler dev --local`. Mesma receita de teste.sh:
#
#   bash servidor/recomendacoes/preparar-local.sh
#   npx wrangler@4 dev --local --port 8799 --config servidor/recomendacoes/wrangler.toml
#   bash servidor/recomendacoes/teste-amigos.sh
#
# PRECISA DE ESTADO LIMPO (preparar-local.sh zera pessoa/perfil/pedido/...). O
# SQLite do D1 local e achado sozinho em .wrangler/ — o teste o usa para duas
# coisas que o servico proprio nao expoe de proposito: semear a foto da conta
# (que so o Trakt/Supabase escreve) e envelhecer linhas para a limpeza diaria.
#
# O NOME DA CONTA DE F, G E H E UM NOME COMPLETO DE PROPOSITO ("Fabiana Souza
# Real"): varias conferencias abaixo procuram esse texto nas respostas para
# estranhos e exigem que ele NUNCA apareca — so o apelido escolhido.
set -u
cd "$(dirname "$0")/../.."
BASE="${1:-http://127.0.0.1:8799}"
NV_D1="${NV_D1:-$(find servidor/recomendacoes/.wrangler -name '*.sqlite' ! -name 'metadata.sqlite' 2>/dev/null | head -1)}"
H=(-H "content-type: application/json")
# chamada: quem metodo rota [corpo]
api() { local q="$1" m="$2" r="$3" b="${4:-}"
  if [ "$m" = GET ]; then curl -s -H "authorization: Bearer tok-$q" -H "x-nuvio-auth: nuvio" "$BASE$r"
  else curl -s -X POST -H "authorization: Bearer tok-$q" -H "x-nuvio-auth: nuvio" "${H[@]}" -d "${b:-{\}}" "$BASE$r"; fi; }
cod() { curl -s -o /dev/null -w '%{http_code}' -X POST -H "authorization: Bearer tok-$1" -H "x-nuvio-auth: nuvio" "${H[@]}" -d "${3:-{\}}" "$BASE$2"; }
ok=0; falhou=0
checa() { if [ "$2" = "$3" ]; then ok=$((ok+1)); printf 'ok   %s\n' "$1"
  else falhou=$((falhou+1)); printf 'FALHOU %s\n  esperado: %s\n  obtido:   %s\n' "$1" "$2" "$3"; fi; }
tem() { printf '%s' "$1" | grep -c -- "$2"; }      # n de linhas que casam (0/1)
sql() { sqlite3 "$NV_D1" "$1"; }

[ -n "$NV_D1" ] || { echo "sem sqlite do D1 local (rode preparar-local.sh e o wrangler dev antes)"; exit 2; }

for q in f g h; do api $q POST /v1/eu > /dev/null; done
api a POST /v1/eu > /dev/null; api e POST /v1/eu > /dev/null

# --- 1. NADA APARECE SEM TER LIGADO ------------------------------------------
r=$(api f GET /v1/perfil)
checa "perfil nasce nao publicado" 1 "$(tem "$r" '"publicado":0')"
checa "nem pesquisavel" 1 "$(tem "$r" '"pesquisavel":0')"
checa "atividade nasce desligada" 1 "$(tem "$r" '"ativ":0')"
checa "sem id de conta na resposta do proprio perfil" 0 "$(tem "$r" 'nuvio:')"

# G ainda nao publicou: busca por "fabi" e por qualquer outra coisa volta vazia
checa "ninguem publicou: busca vazia" 1 "$(tem "$(api g POST /v1/perfis/buscar '{"q":"fabi"}')" '"resultados":\[\]')"

# --- 2. PUBLICAR: so o que a pessoa escolheu, e sanitizado ---------------------
r=$(api f POST /v1/perfil '{"apelido":"Fabi Cine","bio":"fa de terror. escreva f@x.com ou http://x.io","generos":["terror","hack","drama","terror"],"avatar":1,"recentes":0}')
checa "publicar responde ok e devolve o handle" 1 "$(tem "$r" '"pub":"[a-z0-9]\{10\}"')"
pub_f=$(printf '%s' "$r" | jq -r .pub)
r=$(api f GET /v1/perfil)
checa "apelido em a-z0-9" 1 "$(tem "$r" '"apelido":"fabi cine"')"
# a-z0-9 e espaco: NAO ha como escrever e-mail nem link (sem @, ponto, dois-pontos)
checa "bio sem arroba nem ponto nem barra" 0 "$(printf '%s' "$r" | jq -r .bio | grep -c '[@./:]')"
checa "genero fora da lista cai" 0 "$(tem "$r" 'hack')"
checa "genero repetido conta uma vez" 1 "$(printf '%s' "$r" | jq -c .generos | grep -c '^\["terror","drama"\]$')"
checa "apelido curto e recusado" 400 "$(cod g /v1/perfil '{"apelido":"a"}')"
checa "sem apelido nao publica" 400 "$(cod g /v1/perfil '{"bio":"x"}')"

# --- 3. BUSCA: minimo, prefixo, sem enumeracao, sem dado da conta --------------
checa "busca de 2 letras e recusada" 400 "$(cod g /v1/perfis/buscar '{"q":"fa"}')"
checa "busca vazia e recusada" 400 "$(cod g /v1/perfis/buscar '{}')"
r=$(api g POST /v1/perfis/buscar '{"q":"fabi"}')
checa "acha por prefixo do apelido" 1 "$(tem "$r" '"apelido":"fabi cine"')"
checa "prefixo maiusculo/espacado tambem" 1 "$(tem "$(api g POST /v1/perfis/buscar '{"q":" FaBi c"}')" 'fabi cine')"
checa "NAO vaza o nome da conta" 0 "$(tem "$r" 'Fabiana\|Souza')"
checa "NAO vaza o id da conta" 0 "$(tem "$r" 'nuvio:\|fff')"
checa "NAO vaza codigo de pareamento" 0 "$(tem "$r" '"codigo"')"
checa "nao e sufixo: 'cine' nao acha" 1 "$(tem "$(api g POST /v1/perfis/buscar '{"q":"cine"}')" '"resultados":\[\]')"
# so o nome de G nao publicado nao e achavel
checa "quem nao publicou nao e achado pelo apelido" 1 "$(tem "$(api f POST /v1/perfis/buscar '{"q":"guil"}')" '"resultados":\[\]')"

# codigo de pareamento EXATO acha mesmo sem perfil publico (quem dita o codigo escolheu ser achado)
cod_h=$(api h POST /v1/eu | jq -r .codigo)
r=$(api g POST /v1/perfis/buscar "{\"q\":\"$cod_h\"}")
checa "acha por codigo de pareamento" 1 "$(tem "$r" '"pub":"[a-z0-9]\{10\}"')"
checa "codigo errado nao acha" 1 "$(tem "$(api g POST /v1/perfis/buscar '{"q":"zzzzzz"}')" '"resultados":\[\]')"
pub_h=$(printf '%s' "$r" | jq -r '.resultados[0].pub')

# --- 4. FOTO: so de host conhecido e so se a pessoa marcou --------------------
sql "UPDATE pessoa SET avatar='https://secure.gravatar.com/avatar/0123456789abcdef?s=200' WHERE id='nuvio:fff';"
checa "gravatar (hash de e-mail) nunca sai" 0 "$(tem "$(api g POST /v1/perfis/buscar '{"q":"fabi"}')" 'gravatar')"
sql "UPDATE pessoa SET avatar='https://walter.trakt.tv/images/users/1/avatar.jpg' WHERE id='nuvio:fff';"
checa "foto do trakt sai quando a pessoa marcou" 1 "$(tem "$(api g POST /v1/perfis/buscar '{"q":"fabi"}')" 'walter.trakt.tv')"
api f POST /v1/perfil '{"apelido":"Fabi Cine","bio":"","avatar":0}' > /dev/null
checa "sem marcar 'mostrar foto' ela nao sai" 0 "$(tem "$(api g POST /v1/perfis/buscar '{"q":"fabi"}')" 'walter.trakt.tv')"
api f POST /v1/perfil '{"apelido":"Fabi Cine","bio":"fa de terror","generos":["terror"],"avatar":1,"recentes":0}' > /dev/null

# --- 5. CARTAO ------------------------------------------------------------------
r=$(api g POST /v1/perfis/ver "{\"pub\":\"$pub_f\"}")
checa "cartao mostra apelido, bio e genero" 3 "$(( $(tem "$r" 'fabi cine') + $(tem "$r" 'fa de terror') + $(tem "$r" '"terror"') ))"
checa "cartao sem vistos recentes por padrao" 1 "$(tem "$r" '"recentes":\[\]')"
checa "cartao nao vaza nome nem id" 0 "$(tem "$r" 'Fabiana\|nuvio:')"
checa "handle inexistente da 404" 404 "$(cod g /v1/perfis/ver '{"pub":"aaaaaaaaaa"}')"
checa "handle do proprio perfil da 404" 404 "$(cod f /v1/perfis/ver "{\"pub\":\"$pub_f\"}")"

# --- 6. PEDIDO DE AMIZADE PRECISA DOS DOIS LADOS -------------------------------
checa "sem apelido nao pode pedir" 409 "$(cod g /v1/pedidos/enviar "{\"pub\":\"$pub_f\"}")"
api g POST /v1/perfil '{"apelido":"Gui Nerd"}' > /dev/null
api h POST /v1/perfil '{"apelido":"Helena Tv","bio":"","generos":[]}' > /dev/null
r=$(api g POST /v1/pedidos/enviar "{\"pub\":\"$pub_f\"}")
checa "pedido enviado fica pendente" 1 "$(tem "$r" '"estado":"enviado"')"
checa "ainda NAO sao amigos (so um lado)" 0 "$(tem "$(api g GET /v1/contatos)" 'nuvio:fff')"
checa "nem do outro lado" 0 "$(tem "$(api f GET /v1/contatos)" 'nuvio:ggg')"
r=$(api f GET /v1/pedidos)
checa "F ve o pedido, com o APELIDO de G" 1 "$(tem "$r" '"apelido":"gui nerd"')"
checa "e nao o nome da conta de G" 0 "$(tem "$r" 'Guilherme\|nuvio:')"
pub_g=$(printf '%s' "$r" | jq -r '.recebidos[0].pub')
checa "F ve o cartao de quem pediu, mesmo sem G ser pesquisavel de fato" 1 \
  "$(tem "$(api f POST /v1/perfis/ver "{\"pub\":\"$pub_g\"}")" '"relacao":"recebido"')"

# recusar e silencioso e nao deixa reenviar
checa "F recusa" 1 "$(tem "$(api f POST /v1/pedidos/recusar "{\"pub\":\"$pub_g\"}")" '"ok":1')"
checa "some da caixa de F" 1 "$(tem "$(api f GET /v1/pedidos)" '"recebidos":\[\]')"
r=$(api g POST /v1/pedidos/enviar "{\"pub\":\"$pub_f\"}")
checa "G reenvia e continua vendo 'enviado' (nao sabe da recusa)" 1 "$(tem "$r" '"estado":"enviado"')"
checa "mas a caixa de F continua vazia" 1 "$(tem "$(api f GET /v1/pedidos)" '"recebidos":\[\]')"

# aceitar vira amizade nos dois sentidos
api h POST /v1/pedidos/enviar "{\"pub\":\"$pub_f\"}" > /dev/null
pub_h2=$(api f GET /v1/pedidos | jq -r '.recebidos[0].pub')
checa "F aceita H" 1 "$(tem "$(api f POST /v1/pedidos/aceitar "{\"pub\":\"$pub_h2\"}")" '"estado":"amigo"')"
checa "amizade simetrica (F ve H)" 1 "$(tem "$(api f GET /v1/contatos)" 'nuvio:hhh')"
checa "amizade simetrica (H ve F)" 1 "$(tem "$(api h GET /v1/contatos)" 'nuvio:fff')"
checa "aceitar sem pedido e 404" 404 "$(cod f /v1/pedidos/aceitar "{\"pub\":\"$pub_g\"}")"
# pedido pendurado de quem JA e amigo (vinculo por outro caminho) nao aparece
# na caixa: aceitar seria um botao sem efeito (06/10: 2 de 21 em producao)
sql "INSERT OR IGNORE INTO pedido (de, para, criado, estado) VALUES ('nuvio:hhh', 'nuvio:fff', strftime('%s','now'), 0)"
checa "pedido de quem ja e amigo nao aparece" 1 "$(tem "$(api f GET /v1/pedidos)" '"recebidos":\[\]')"
sql "DELETE FROM pedido WHERE de = 'nuvio:hhh' AND para = 'nuvio:fff'"

# dois pedidos cruzados = amigos sem passo extra
api g POST /v1/pedidos/enviar "{\"pub\":\"$pub_h\"}" > /dev/null
pub_g_h=$(api h GET /v1/pedidos | jq -r '.recebidos[0].pub')
r=$(api h POST /v1/pedidos/enviar "{\"pub\":\"$pub_g_h\"}")
checa "pedidos cruzados viram amizade" 1 "$(tem "$r" '"estado":"amigo"')"

# --- 7. ATIVIDADE: so amigo mutuo, so de quem ligou ----------------------------
POST='{"imdb":"tt0111161","tipo":"movie","titulo":"Um Sonho de Liberdade","ano":"1994","nota":93,"poster":"http://espiao.exemplo/x.jpg","agora":0}'
checa "atividade desligada nao grava" 1 "$(tem "$(api f POST /v1/atividade "$POST")" '"guardado":0')"
checa "e nao aparece para o amigo" 1 "$(tem "$(api h GET /v1/amigos/atividade)" '"itens":\[\]')"
checa "imdb invalido e 400" 400 "$(cod f /v1/atividade '{"imdb":"xx"}')"

api f POST /v1/perfil/atividade '{"nivel":1}' > /dev/null
checa "nivel 1 grava" 1 "$(tem "$(api f POST /v1/atividade "$POST")" '"guardado":1')"
r=$(api h GET /v1/amigos/atividade)
checa "amigo ve o titulo" 1 "$(tem "$r" 'Um Sonho de Liberdade')"
checa "com o APELIDO do amigo" 1 "$(tem "$r" '"deNome":"fabi cine"')"
checa "sem poster guardado (capa e montada na TV de quem ve)" 0 "$(tem "$r" 'espiao\|poster')"
checa "quem nao e amigo nao ve" 1 "$(tem "$(api e GET /v1/amigos/atividade)" '"itens":\[\]')"
checa "G (pediu mas nao e amigo de F) nao ve" 0 "$(tem "$(api g GET /v1/amigos/atividade)" 'Um Sonho')"
checa "'assistindo agora' sem nivel 2 vira 'assistiu'" 1 \
  "$(api f POST /v1/atividade '{"imdb":"tt0068646","titulo":"O Poderoso Chefao","agora":1}' > /dev/null; tem "$(api h GET /v1/amigos/atividade)" '"agora":0')"
checa "nao ha agora:1 no nivel 1" 0 "$(tem "$(api h GET /v1/amigos/atividade)" '"agora":1')"
api f POST /v1/perfil/atividade '{"nivel":2}' > /dev/null
api f POST /v1/atividade '{"imdb":"tt0468569","titulo":"Batman","agora":1}' > /dev/null
checa "nivel 2 mostra 'assistindo agora'" 1 "$(tem "$(api h GET /v1/amigos/atividade)" '"agora":1')"
# "ASSISTINDO AGORA" VENCE EM 10 MIN E SOME. Nao vira "assistiu": quem largou o
# filme no meio nao assistiu, e dizer que sim a um amigo seria falso.
sql "UPDATE atividade SET criado = criado - 1200 WHERE imdb = 'tt0468569';"
checa "'agora' vencido some do feed (e nao vira 'assistiu')" 0 "$(tem "$(api h GET /v1/amigos/atividade)" 'Batman')"
# so um lado ligado nao e amizade: H NAO compartilha, F ve vazio
checa "H nao ligou, F nao ve nada de H" 1 "$(tem "$(api f GET /v1/amigos/atividade)" '"itens":\[\]')"
# desligar apaga o que estava guardado, na hora
api f POST /v1/perfil/atividade '{"nivel":0}' > /dev/null
checa "desligar apaga as linhas guardadas" 0 "$(sql "SELECT COUNT(*) FROM atividade WHERE pessoa='nuvio:fff';")"
checa "e o amigo nao ve mais" 1 "$(tem "$(api h GET /v1/amigos/atividade)" '"itens":\[\]')"

# --- 8. VISTOS RECENTES PUBLICOS (opt-in separado) e gosto parecido -------------
checa "recentes desligado: cartao sem titulos" 1 \
  "$(tem "$(api e POST /v1/perfis/ver "{\"pub\":\"$pub_f\"}")" '"recentes":\[\]')"
api f POST /v1/perfil '{"apelido":"Fabi Cine","bio":"fa de terror","generos":["terror"],"avatar":1,"recentes":1}' > /dev/null
for t in tt0111161 tt0068646 tt0468569 tt1375666; do
  api f POST /v1/atividade "{\"imdb\":\"$t\",\"titulo\":\"T $t\",\"agora\":0}" > /dev/null; done
checa "recentes ligado (sem atividade p/ amigos) grava" 4 "$(sql "SELECT COUNT(*) FROM atividade WHERE pessoa='nuvio:fff';")"
checa "e o cartao publico mostra" 1 "$(tem "$(api g POST /v1/perfis/ver "{\"pub\":\"$pub_f\"}")" 'T tt0111161')"
checa "mas o feed dos amigos continua vazio (ativ=0)" 1 "$(tem "$(api h GET /v1/amigos/atividade)" '"itens":\[\]')"
# gosto parecido
checa "3 titulos em comum sugere" 1 "$(tem "$(api g POST /v1/perfis/sugeridos '{"imdbs":["tt0111161","tt0068646","tt0468569","tt9999999"]}')" '"emComum":3')"
checa "limiar adaptativo: 2 titulos (poucos enviados) ja sugerem, com o motivo" 1 "$(tem "$(api g POST /v1/perfis/sugeridos '{"imdbs":["tt0111161","tt0068646"]}')" '"motivo":{"tipo":"titulos","n":2')"
checa "o motivo de titulos nunca diz QUAIS titulos" 0 "$(tem "$(api g POST /v1/perfis/sugeridos '{"imdbs":["tt0111161","tt0068646"]}')" 'T tt0111161')"
checa "sem titulo nenhum: ainda volta alguem, com motivo (generos ou ativos)" 1 "$(tem "$(api g POST /v1/perfis/sugeridos '{"imdbs":[]}')" '"motivo":{"tipo":"\(generos\|ativos\)"')"
checa "sem titulo em comum o motivo nao inventa titulos" 0 "$(tem "$(api g POST /v1/perfis/sugeridos '{"imdbs":["tt7777777"]}')" '"tipo":"titulos"')"
checa "o cartao de gosto nao vaza nome/id" 0 "$(tem "$(api g POST /v1/perfis/sugeridos '{"imdbs":["tt0111161","tt0068646","tt0468569"]}')" 'Fabiana\|nuvio:')"
checa "quem nao e pesquisavel nao ve os outros" 1 "$(tem "$(api a POST /v1/perfis/sugeridos '{"imdbs":["tt0111161","tt0068646","tt0468569"]}')" '"sugeridos":\[\]')"
# amigo nao vira sugestao
checa "amigo nao e sugerido" 0 "$(tem "$(api h POST /v1/perfis/sugeridos '{"imdbs":["tt0111161","tt0068646","tt0468569"]}')" 'fabi cine')"

# --- 9. BLOQUEIO -------------------------------------------------------------------
checa "F bloqueia G" 1 "$(tem "$(api f POST /v1/bloquear "{\"pub\":\"$pub_g\"}")" '"ok":1')"
checa "G nao acha mais F na busca" 1 "$(tem "$(api g POST /v1/perfis/buscar '{"q":"fabi"}')" '"resultados":\[\]')"
checa "G nao abre o cartao de F" 404 "$(cod g /v1/perfis/ver "{\"pub\":\"$pub_f\"}")"
checa "sugestao de gosto nao mostra F a G" 0 "$(tem "$(api g POST /v1/perfis/sugeridos '{"imdbs":["tt0111161","tt0068646","tt0468569"]}')" 'fabi cine')"
r=$(api g POST /v1/pedidos/enviar "{\"pub\":\"$pub_f\"}")
checa "pedido de G parece enviado (nao revela o bloqueio)" 1 "$(tem "$r" '"estado":"enviado"')"
checa "mas F nao recebe nada" 1 "$(tem "$(api f GET /v1/pedidos)" '"recebidos":\[\]')"
cod_f=$(api f POST /v1/eu | jq -r .codigo)
checa "codigo de F nao vincula G (responde como inexistente)" 404 "$(cod g /v1/contatos "{\"codigo\":\"$cod_f\"}")"
checa "F ve G na lista de bloqueados" 1 "$(tem "$(api f GET /v1/bloqueados)" "$pub_g")"
# bloquear um AMIGO derruba a amizade nos dois lados (por id, como na lista)
checa "F bloqueia H (amigo) pelo id" 1 "$(tem "$(api f POST /v1/bloquear '{"id":"nuvio:hhh"}')" '"ok":1')"
checa "F nao tem mais H" 0 "$(tem "$(api f GET /v1/contatos)" 'nuvio:hhh')"
checa "H nao tem mais F" 0 "$(tem "$(api h GET /v1/contatos)" 'nuvio:fff')"
checa "bloquear a si mesmo e recusado" 404 "$(cod f /v1/bloquear '{"id":"nuvio:fff"}')"
# desbloquear devolve a visibilidade, mas nao a amizade
api f POST /v1/desbloquear "{\"pub\":\"$pub_g\"}" > /dev/null
checa "desbloqueado: G acha F de novo" 1 "$(tem "$(api g POST /v1/perfis/buscar '{"q":"fabi"}')" 'fabi cine')"
checa "e a amizade nao volta sozinha" 0 "$(tem "$(api f GET /v1/contatos)" 'nuvio:hhh')"

# --- 10. DESPUBLICAR: some na hora, o handle antigo morre ----------------------------
api f POST /v1/pedidos/enviar "{\"pub\":\"$pub_g\"}" > /dev/null
api f POST /v1/perfil/atividade '{"nivel":0}' > /dev/null
checa "despublicar responde ok" 1 "$(tem "$(api f POST /v1/perfil/despublicar)" '"publicado":0')"
checa "some da busca NA HORA" 1 "$(tem "$(api g POST /v1/perfis/buscar '{"q":"fabi"}')" '"resultados":\[\]')"
checa "o handle antigo nao abre mais" 404 "$(cod g /v1/perfis/ver "{\"pub\":\"$pub_f\"}")"
checa "o banco nao guarda apelido, bio nem generos" "||" "$(sql "SELECT apelido||'|'||bio||'|'||generos FROM perfil WHERE pessoa='nuvio:fff';")"
checa "flag descobrivel = 0" 0 "$(sql "SELECT descobrivel FROM pessoa WHERE id='nuvio:fff';")"
checa "atividade publica apagada (recentes=0 e ativ=0)" 0 "$(sql "SELECT COUNT(*) FROM atividade WHERE pessoa='nuvio:fff';")"
checa "pedido pendente de quem sumiu foi retirado" 1 "$(tem "$(api g GET /v1/pedidos)" '"recebidos":\[\]')"
checa "handle novo e diferente do antigo" 0 "$(api f GET /v1/perfil | jq -r .pub | grep -c "^$pub_f\$")"

# a rota ANTIGA de descobrivel=0 tambem despublica de verdade
api f POST /v1/perfil '{"apelido":"Fabi Cine","bio":"volta"}' > /dev/null
api f POST /v1/descobrivel '{"descobrivel":0}' > /dev/null
checa "rota antiga descobrivel=0 limpa o perfil" "|" "$(sql "SELECT apelido||'|'||bio FROM perfil WHERE pessoa='nuvio:fff';" | tr -d 'a-z0-9 ')"

# apagar tudo
api f POST /v1/perfil '{"apelido":"Fabi Cine"}' > /dev/null
api f POST /v1/perfil/atividade '{"nivel":1}' > /dev/null
api f POST /v1/atividade '{"imdb":"tt0111161","titulo":"x"}' > /dev/null
api f POST /v1/perfil/apagar > /dev/null
checa "apagar zera atividade e nivel" "0|0" "$(sql "SELECT (SELECT COUNT(*) FROM atividade WHERE pessoa='nuvio:fff')||'|'||(SELECT ativ FROM perfil WHERE pessoa='nuvio:fff');")"

# --- 11. LIMITE DE USO E LIMPEZA ------------------------------------------------------
# E ainda nao buscou: 40 buscas passam, a 41a e barrada (janela de 1 h).
for i in $(seq 1 40); do cod e /v1/perfis/buscar '{"q":"zzz"}' > /dev/null; done
checa "a 41a busca da hora e 429" 429 "$(cod e /v1/perfis/buscar '{"q":"zzz"}')"
checa "o limite e por pessoa: outra busca normal" 200 "$(cod a /v1/perfis/buscar '{"q":"zzz"}')"

# A limpeza diaria apaga atividade velha, pedido antigo e contador vencido.
sql "INSERT OR REPLACE INTO atividade (pessoa, imdb, criado) VALUES ('nuvio:ggg','tt0000001',1000);
     INSERT OR REPLACE INTO pedido (de, para, criado, estado) VALUES ('nuvio:ggg','nuvio:hhh',1000,0);"
curl -s "$BASE/cdn-cgi/handler/scheduled" > /dev/null
sleep 1
checa "limpeza apaga atividade com mais de 30 dias" 0 "$(sql "SELECT COUNT(*) FROM atividade WHERE imdb='tt0000001';")"
checa "e pedido pendente parado ha mais de 30 dias" 0 "$(sql "SELECT COUNT(*) FROM pedido WHERE criado=1000;")"

# --- 12. NADA DE ROTA ABERTA -----------------------------------------------------------
for r in /v1/perfil /v1/pedidos /v1/bloqueados /v1/amigos/atividade; do
  checa "sem token: GET $r da 401" 401 "$(curl -s -o /dev/null -w '%{http_code}' "$BASE$r")"; done
for r in /v1/perfis/buscar /v1/perfis/ver /v1/pedidos/enviar /v1/bloquear /v1/atividade; do
  checa "sem token: POST $r da 401" 401 "$(curl -s -o /dev/null -w '%{http_code}' -X POST -d '{}' "$BASE$r")"; done

printf '\n%d ok, %d falharam\n' "$ok" "$falhou"
[ "$falhou" -eq 0 ]
