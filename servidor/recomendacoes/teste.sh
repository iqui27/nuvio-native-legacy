#!/usr/bin/env bash
# Teste de ponta a ponta contra `wrangler dev --local`.
#
# A identidade e o UNICO ponto que fala com a rede externa (Trakt / Supabase);
# aqui ela e curto-circuitada gravando a sessao direto no D1 local, que e o que
# o cache faria depois da primeira verificacao. O resto do servico e exercitado
# de verdade. QUEM GRAVA ESSAS SESSOES e preparar-local.sh, que tem de rodar
# antes — sem ele nenhum token deste arquivo existe e TUDO responde 401:
#
#   bash servidor/recomendacoes/preparar-local.sh
#   npx wrangler@4 dev --local --port 8799 --config servidor/recomendacoes/wrangler.toml
#   bash servidor/recomendacoes/teste.sh
#
# O AVATAR NAO TEM COMO ENTRAR POR AQUI, e por isso ele tem uma porta propria:
# a foto so e gravada quando o servidor VERIFICA a identidade contra o Trakt, e
# a verificacao e exatamente o que este teste curto-circuita. Passe o caminho do
# SQLite do D1 local em NV_D1 e o teste semeia a foto na mao para exercitar o
# JOIN de `deAvatar`; sem ele, essas duas conferencias sao PULADAS e o resto
# roda igual.
#
#   NV_D1=servidor/recomendacoes/.wrangler/state/v3/d1/miniflare-D1DatabaseObject/<hash>.sqlite \
#     bash servidor/recomendacoes/teste.sh
set -u
BASE="${1:-http://127.0.0.1:8799}"
NV_D1="${NV_D1:-}"
A=(-H "authorization: Bearer tok-a" -H "x-nuvio-auth: nuvio" -H "content-type: application/json")
B=(-H "authorization: Bearer tok-b" -H "x-nuvio-auth: nuvio" -H "content-type: application/json")
# C, D e E existem para a roda de amigo-de-amigo: C conhece D, D conhece E, e E
# so pode ser sugerido a C se ELE tiver aceitado aparecer. P e de TRAKT, para o
# outro ramo da sugestao (ver preparar-local.sh).
C=(-H "authorization: Bearer tok-c" -H "x-nuvio-auth: nuvio" -H "content-type: application/json")
D=(-H "authorization: Bearer tok-d" -H "x-nuvio-auth: nuvio" -H "content-type: application/json")
E=(-H "authorization: Bearer tok-e" -H "x-nuvio-auth: nuvio" -H "content-type: application/json")
P=(-H "authorization: Bearer tok-t" -H "x-nuvio-auth: trakt" -H "content-type: application/json")
ok=0; falhou=0
checa() { # nome, esperado, obtido
  if [ "$2" = "$3" ]; then ok=$((ok+1)); printf 'ok   %s\n' "$1"
  else falhou=$((falhou+1)); printf 'FALHOU %s\n  esperado: %s\n  obtido:   %s\n' "$1" "$2" "$3"; fi
}

checa "sem token da 401" 401 "$(curl -s -o /dev/null -w '%{http_code}' "$BASE/v1/rec")"

eu_a=$(curl -s -X POST "${A[@]}" "$BASE/v1/eu")
eu_b=$(curl -s -X POST "${B[@]}" "$BASE/v1/eu")
cod_b=$(printf '%s' "$eu_b" | sed -E 's/.*"codigo":"([a-z0-9]{6})".*/\1/')
checa "codigo de 6 chars" 6 "${#cod_b}"

checa "nao-contato nao recebe" 403 "$(curl -s -o /dev/null -w '%{http_code}' -X POST "${A[@]}" \
  -d '{"para":"nuvio:bbb","imdb":"tt0111161","titulo":"Shawshank"}' "$BASE/v1/rec")"

curl -s -X POST "${A[@]}" -d "{\"codigo\":\"$cod_b\"}" "$BASE/v1/contatos" > /dev/null
checa "A ve B na lista" 1 "$(curl -s "${A[@]}" "$BASE/v1/contatos" | grep -c 'nuvio:bbb')"
checa "vinculo e simetrico" 1 "$(curl -s "${B[@]}" "$BASE/v1/contatos" | grep -c 'nuvio:aaa')"

checa "envio aceito" 1 "$(curl -s -X POST "${A[@]}" \
  -d '{"para":"nuvio:bbb","imdb":"tt0111161","tipo":"movie","titulo":"Shawshank","ano":"1994","modelo":2,"nota":93}' \
  "$BASE/v1/rec" | grep -c '"ok":1')"

r=$(curl -s -D /tmp/rec.h "${B[@]}" "$BASE/v1/rec?desde=0")
checa "B recebe 1" 1 "$(printf '%s' "$r" | grep -c '"novas":1')"
checa "titulo chegou" 1 "$(printf '%s' "$r" | grep -c 'Shawshank')"
# A NOTA VIAJA COM A RECOMENDACAO, em centesimos. Ela NAO e procurada no
# catalogo de quem recebe — o titulo pode nao estar la, que e justamente o caso
# que uma recomendacao cobre.
checa "nota chegou em centesimos" 1 "$(printf '%s' "$r" | grep -c '"nota":93')"
# O CAMPO DA FOTO EXISTE SEMPRE, mesmo vazio: o cliente distingue "sem foto" de
# "campo ausente" para decidir entre a foto e a inicial no disco colorido, e uma
# chave que some conforme o dado deixaria a TV sem essa resposta.
checa "campo da foto existe" 1 "$(printf '%s' "$r" | grep -c '"deAvatar":""')"
checa "contato tem campo de foto" 1 "$(curl -s "${A[@]}" "$BASE/v1/contatos" \
  | grep -c '"avatar":""')"
etag=$(grep -i '^etag:' /tmp/rec.h | tr -d '\r' | cut -d' ' -f2)
checa "304 com o mesmo etag" 304 "$(curl -s -o /dev/null -w '%{http_code}' "${B[@]}" \
  -H "if-none-match: $etag" "$BASE/v1/rec?desde=0")"

id=$(printf '%s' "$r" | sed -E 's/.*"itens":\[\{"id":([0-9]+).*/\1/')
curl -s -X POST "${B[@]}" -d "{\"ids\":[$id]}" "$BASE/v1/rec/visto" > /dev/null
checa "marcada como vista" 1 "$(curl -s "${B[@]}" "$BASE/v1/rec?desde=0" | grep -c '"novas":0')"

checa "texto livre e limpo" 1 "$(curl -s -X POST "${A[@]}" \
  -d '{"para":"nuvio:bbb","imdb":"tt0111161","modelo":-1,"texto":"OLHA ISSO!!! <script>alert(1)</script>"}' \
  "$BASE/v1/rec" | grep -c '"ok":1')"
# A limpeza nao "escapa" marcacao: ela derruba tudo que nao e a-z0-9 ou espaco,
# entao o que sobra de um <script> sao letras soltas, sem nenhum sinal de
# marcacao. E por isso que o teste procura o texto EXATO, nao a palavra.
checa "texto guardado em a-z0-9" 1 "$(curl -s "${B[@]}" "$BASE/v1/rec?desde=0" \
  | grep -c '"texto":"olha isso script alert 1 script"')"
checa "sem sinal de marcacao" 0 "$(curl -s "${B[@]}" "$BASE/v1/rec?desde=0" | grep -c '[<>]')"

# NOTA FORA DA FAIXA VIRA 0 E O ENVIO PASSA. Ela e decorativa: derrubar uma
# recomendacao inteira por causa de um campo que so pinta um selo seria trocar
# um selo errado por uma funcao que nao funciona.
curl -s -o /dev/null -X POST "${A[@]}" \
  -d '{"para":"nuvio:bbb","imdb":"tt1375666","titulo":"Origem","nota":9999}' "$BASE/v1/rec"
checa "nota fora da faixa vira 0" 1 "$(curl -s "${B[@]}" "$BASE/v1/rec?desde=0" \
  | grep -c '"titulo":"Origem","poster":"","ano":"","modelo":0,"texto":"","nota":0')"

if [ -n "$NV_D1" ] && command -v sqlite3 > /dev/null 2>&1; then
  sqlite3 "$NV_D1" \
    "UPDATE pessoa SET avatar = 'https://walter.trakt.tv/a/1a2b3c.jpg' WHERE id = 'nuvio:aaa';"
  checa "a foto de quem mandou chega em deAvatar" 1 \
    "$(curl -s "${B[@]}" "$BASE/v1/rec?desde=0" | grep -c 'walter.trakt.tv/a/1a2b3c.jpg')"
  checa "a foto tambem sai na lista de contatos" 1 \
    "$(curl -s "${B[@]}" "$BASE/v1/contatos" | grep -c 'walter.trakt.tv/a/1a2b3c.jpg')"
  sqlite3 "$NV_D1" "UPDATE pessoa SET avatar = '' WHERE id = 'nuvio:aaa';"
else
  printf 'pulado  deAvatar preenchido (defina NV_D1 com o sqlite do D1 local)\n'
fi

checa "imdb invalido recusado" 400 "$(curl -s -o /dev/null -w '%{http_code}' -X POST "${A[@]}" \
  -d '{"para":"nuvio:bbb","imdb":"nao-e-imdb"}' "$BASE/v1/rec")"

for _ in 1 2 3 4 5; do curl -s -o /dev/null -X POST "${A[@]}" \
  -d '{"para":"nuvio:bbb","imdb":"tt0068646","titulo":"Poderoso Chefao"}' "$BASE/v1/rec"; done
checa "limite por par barra" 429 "$(curl -s -o /dev/null -w '%{http_code}' -X POST "${A[@]}" \
  -d '{"para":"nuvio:bbb","imdb":"tt0068646"}' "$BASE/v1/rec")"

curl -s -X POST "${B[@]}" -d '{"id":"nuvio:aaa"}' "$BASE/v1/contatos/remover" > /dev/null
checa "bloquear corta o envio" 403 "$(curl -s -o /dev/null -w '%{http_code}' -X POST "${A[@]}" \
  -d '{"para":"nuvio:bbb","imdb":"tt0111161"}' "$BASE/v1/rec")"

# --- APARECER PARA OUTRAS PESSOAS, E AS SUGESTOES --------------------------
#
# A roda: C conhece D por codigo, D conhece E por codigo. Ninguem aceitou
# aparecer ainda, entao a resposta certa para "quem eu poderia adicionar?" e
# NINGUEM — e e essa a primeira coisa conferida.

eu_c=$(curl -s -X POST "${C[@]}" "$BASE/v1/eu")
eu_d=$(curl -s -X POST "${D[@]}" "$BASE/v1/eu")
eu_e=$(curl -s -X POST "${E[@]}" "$BASE/v1/eu")
eu_p=$(curl -s -X POST "${P[@]}" "$BASE/v1/eu")
cod_d=$(printf '%s' "$eu_d" | sed -E 's/.*"codigo":"([a-z0-9]{6})".*/\1/')
cod_e=$(printf '%s' "$eu_e" | sed -E 's/.*"codigo":"([a-z0-9]{6})".*/\1/')

# O PADRAO E NAO APARECER. Se esta conferencia cair, a migracao entrou sem o
# DEFAULT 0 ou alguem passou a escrever a coluna no registro — e nos dois casos
# todo mundo que ja usava o servico virou visivel sem ter respondido nada.
checa "quem registra nasce nao descobrivel" 1 \
  "$(printf '%s' "$eu_c" | grep -c '"descobrivel":0')"
checa "e vale para quem chega pelo trakt tambem" 1 \
  "$(printf '%s' "$eu_p" | grep -c '"descobrivel":0')"

curl -s -o /dev/null -X POST "${C[@]}" -d "{\"codigo\":\"$cod_d\"}" "$BASE/v1/contatos"
curl -s -o /dev/null -X POST "${D[@]}" -d "{\"codigo\":\"$cod_e\"}" "$BASE/v1/contatos"

# E E AMIGO DE D, QUE E AMIGO DE C — mas E nao aceitou aparecer. A lista de C
# tem de voltar vazia. Esta e a conferencia que separa "sugestao" de "lista de
# todo mundo que usa o app".
checa "nao-descobrivel nao aparece em sugestao" 1 \
  "$(curl -s -X POST "${C[@]}" -d '{}' "$BASE/v1/sugestoes" | grep -c '"sugestoes":\[\]')"

# NINGUEM LIGA O SINALIZADOR DE NINGUEM. C manda o id de E no corpo; a rota nao
# tem esse campo, entao o efeito recai sobre QUEM MANDOU.
curl -s -o /dev/null -X POST "${C[@]}" -d '{"id":"nuvio:eee","descobrivel":1}' \
  "$BASE/v1/descobrivel"
checa "C nao liga o sinalizador de E" 1 \
  "$(curl -s -X POST "${E[@]}" "$BASE/v1/eu" | grep -c '"descobrivel":0')"
checa "o pedido caiu sobre o proprio C" 1 \
  "$(curl -s -X POST "${C[@]}" "$BASE/v1/eu" | grep -c '"descobrivel":1')"
curl -s -o /dev/null -X POST "${C[@]}" -d '{"descobrivel":0}' "$BASE/v1/descobrivel"

# Agora E aceita, por conta propria.
checa "E aceita aparecer" 1 "$(curl -s -X POST "${E[@]}" -d '{"descobrivel":1}' \
  "$BASE/v1/descobrivel" | grep -c '"descobrivel":1')"
sug_c=$(curl -s -X POST "${C[@]}" -d '{}' "$BASE/v1/sugestoes")
checa "E aparece como amigo de amigo" 1 "$(printf '%s' "$sug_c" | grep -c 'nuvio:eee')"
checa "a sugestao diz por onde chegou" 1 "$(printf '%s' "$sug_c" | grep -c '"origem":"amigo"')"
# O NOME DO INTERMEDIARIO E O QUE A TV ESCREVE na linha ("amigo de Daniel"). Sem
# ele a sugestao seria um nome solto, que e o que faz alguem recusar.
checa "e por quem" 1 "$(printf '%s' "$sug_c" | grep -c '"viaNome":"Daniel"')"
# D JA E CONTATO DE C: sugerir de novo quem ja esta na lista de contatos logo
# acima e o defeito mais facil de cometer neste JOIN.
checa "quem ja e contato nao vira sugestao" 0 "$(printf '%s' "$sug_c" | grep -c 'nuvio:ddd')"
checa "eu nao me sugiro" 0 "$(printf '%s' "$sug_c" | grep -c 'nuvio:ccc')"

# O RAMO DO TRAKT. C manda os slugs de quem segue — a MESMA lista que
# /v1/contatos/trakt ja recebe. Pedrinho usa o servico e ainda assim nao aparece
# enquanto nao aceitar.
checa "seguido no trakt sem aceitar nao aparece" 0 \
  "$(curl -s -X POST "${C[@]}" -d '{"slugs":["pedrinho","fulano"]}' "$BASE/v1/sugestoes" \
     | grep -c 'trakt:pedrinho')"
curl -s -o /dev/null -X POST "${P[@]}" -d '{"descobrivel":1}' "$BASE/v1/descobrivel"
sug_t=$(curl -s -X POST "${C[@]}" -d '{"slugs":["pedrinho","fulano"]}' "$BASE/v1/sugestoes")
checa "seguido no trakt que aceitou aparece" 1 "$(printf '%s' "$sug_t" | grep -c 'trakt:pedrinho')"
checa "e vem marcado como trakt" 1 "$(printf '%s' "$sug_t" | grep -c '"origem":"trakt"')"

# ADICIONAR EM UMA ACAO, e so quem estava na propria lista de sugestoes. Um id
# qualquer — aqui o de A, que nao tem nenhuma relacao com C — e recusado com 403
# mesmo sendo um id valido e existente.
checa "adicionar sugestao vincula" 1 "$(curl -s -X POST "${C[@]}" \
  -d '{"id":"trakt:pedrinho","slugs":["pedrinho"]}' "$BASE/v1/contatos/sugerido" \
  | grep -c '"ok":1')"
checa "e o vinculo e simetrico" 1 \
  "$(curl -s "${P[@]}" "$BASE/v1/contatos" | grep -c 'nuvio:ccc')"
checa "id fora das sugestoes e recusado" 403 \
  "$(curl -s -o /dev/null -w '%{http_code}' -X POST "${C[@]}" \
     -d '{"id":"nuvio:aaa"}' "$BASE/v1/contatos/sugerido")"

# REVOGAR TIRA DA SUGESTAO DE TODO MUNDO, na hora. Nao ha copia da lista em
# lugar nenhum: a consulta le a coluna a cada pedido.
curl -s -o /dev/null -X POST "${E[@]}" -d '{"descobrivel":0}' "$BASE/v1/descobrivel"
checa "revogar tira das sugestoes" 0 \
  "$(curl -s -X POST "${C[@]}" -d '{}' "$BASE/v1/sugestoes" | grep -c 'nuvio:eee')"
checa "e tambem das de D, que era o contato em comum" 0 \
  "$(curl -s -X POST "${D[@]}" -d '{}' "$BASE/v1/sugestoes" | grep -c 'nuvio:eee')"
# QUEM REVOGA NAO PERDE CONTATO NENHUM. O sinalizador governa aparecer na
# sugestao dos outros, e mais nada — desligar nao pode desfazer um vinculo que
# as duas pessoas ja aceitaram.
checa "revogar nao desfaz contato" 1 \
  "$(curl -s "${E[@]}" "$BASE/v1/contatos" | grep -c 'nuvio:ddd')"

printf '\n%d ok, %d falharam\n' "$ok" "$falhou"
[ "$falhou" -eq 0 ]
