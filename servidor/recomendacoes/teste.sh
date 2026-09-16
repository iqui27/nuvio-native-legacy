#!/usr/bin/env bash
# Teste de ponta a ponta contra `wrangler dev --local`.
#
# A identidade e o UNICO ponto que fala com a rede externa (Trakt / Supabase);
# aqui ela e curto-circuitada gravando a sessao direto no D1 local, que e o que
# o cache faria depois da primeira verificacao. O resto do servico e exercitado
# de verdade.
set -u
BASE="${1:-http://127.0.0.1:8799}"
A=(-H "authorization: Bearer tok-a" -H "x-nuvio-auth: nuvio" -H "content-type: application/json")
B=(-H "authorization: Bearer tok-b" -H "x-nuvio-auth: nuvio" -H "content-type: application/json")
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
  -d '{"para":"nuvio:bbb","imdb":"tt0111161","tipo":"movie","titulo":"Shawshank","ano":"1994","modelo":2}' \
  "$BASE/v1/rec" | grep -c '"ok":1')"

r=$(curl -s -D /tmp/rec.h "${B[@]}" "$BASE/v1/rec?desde=0")
checa "B recebe 1" 1 "$(printf '%s' "$r" | grep -c '"novas":1')"
checa "titulo chegou" 1 "$(printf '%s' "$r" | grep -c 'Shawshank')"
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

checa "imdb invalido recusado" 400 "$(curl -s -o /dev/null -w '%{http_code}' -X POST "${A[@]}" \
  -d '{"para":"nuvio:bbb","imdb":"nao-e-imdb"}' "$BASE/v1/rec")"

for _ in 1 2 3 4 5; do curl -s -o /dev/null -X POST "${A[@]}" \
  -d '{"para":"nuvio:bbb","imdb":"tt0068646","titulo":"Poderoso Chefao"}' "$BASE/v1/rec"; done
checa "limite por par barra" 429 "$(curl -s -o /dev/null -w '%{http_code}' -X POST "${A[@]}" \
  -d '{"para":"nuvio:bbb","imdb":"tt0068646"}' "$BASE/v1/rec")"

curl -s -X POST "${B[@]}" -d '{"id":"nuvio:aaa"}' "$BASE/v1/contatos/remover" > /dev/null
checa "bloquear corta o envio" 403 "$(curl -s -o /dev/null -w '%{http_code}' -X POST "${A[@]}" \
  -d '{"para":"nuvio:bbb","imdb":"tt0111161"}' "$BASE/v1/rec")"

printf '\n%d ok, %d falharam\n' "$ok" "$falhou"
[ "$falhou" -eq 0 ]
