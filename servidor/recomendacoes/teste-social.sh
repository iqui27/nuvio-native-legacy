#!/usr/bin/env bash
# Teste do redesenho do Social (src/social.js, migracao-006) contra
# `wrangler dev --local`. PRECISA DE ESTADO LIMPO e do sqlite3:
#
#   bash servidor/recomendacoes/preparar-local.sh
#   npx wrangler@4 dev --local --port 8799 --config servidor/recomendacoes/wrangler.toml
#   bash servidor/recomendacoes/teste-social.sh
#
# Prova: perfil vira pessoa (X-Nuvio-Perfil), nome nunca e UUID, nome de
# exibicao, foto so de host conhecido, nivel 0 nao grava nem vaza nada, nivel 1
# so amigos, nivel 2 tambem amigo de amigo (com handle, sem id da conta),
# bloqueio, rec de origem, agregados, "assistindo agora" vence, gosto parecido,
# ETag/304 e que o corpo antigo de /v1/atividade continua aceito.
set -u
cd "$(dirname "$0")/../.."
BASE="${1:-http://127.0.0.1:8799}"
NV_D1="${NV_D1:-$(find servidor/recomendacoes/.wrangler -name '*.sqlite' ! -name 'metadata.sqlite' 2>/dev/null | head -1)}"
[ -n "$NV_D1" ] || { echo "sem sqlite do D1 local (rode preparar-local.sh e o wrangler dev antes)"; exit 2; }
ok=0; falhou=0
checa() { if [ "$2" = "$3" ]; then ok=$((ok+1)); printf 'ok   %s\n' "$1"
  else falhou=$((falhou+1)); printf 'FALHOU %s\n  esperado: %s\n  obtido:   %s\n' "$1" "$2" "$3"; fi; }
tem() { printf '%s' "$1" | grep -c -- "$2"; }
sql() { sqlite3 "$NV_D1" "$1"; }
# api <tok> <GET|POST> <rota> [corpo] [perfil]
api() { local q="$1" m="$2" r="$3" b="${4:-}" pf="${5:-}"
  local extra=(); [ -n "$pf" ] && extra=(-H "x-nuvio-perfil: $pf")
  if [ "$m" = GET ]; then curl -s -H "authorization: Bearer tok-$q" -H "x-nuvio-auth: nuvio" "${extra[@]}" "$BASE$r"
  else curl -s -X POST -H "authorization: Bearer tok-$q" -H "x-nuvio-auth: nuvio" "${extra[@]}" \
         -H "content-type: application/json" -d "${b:-{\}}" "$BASE$r"; fi; }
ev() { api "$1" POST /v1/atividade "$2"; }
codigo() { printf '%s' "$1" | sed -E 's/.*"codigo":"([a-z0-9]{6})".*/\1/'; }

# Uma conta Nuvio SEM nome nenhum (user_metadata vazio), o caso do UUID na TV.
hz=$(printf 'nuvio:tok-z' | shasum -a 256 | cut -d' ' -f1)
sql "INSERT OR REPLACE INTO sessao (hash, id, nome, expira) VALUES ('$hz','nuvio:5269539e-0000-4000-8000-000000000000','',9999999999);"

# --- identidade e nome ---------------------------------------------------------
ez=$(api z POST /v1/eu)
checa "conta sem nome vira 'Amigo #n'" 1 "$(tem "$ez" '"nome":"Amigo #[0-9]*"')"
checa "e nunca o UUID" 0 "$(tem "$ez" '"nome":"5269539e')"
ea=$(api a POST /v1/eu)
eb=$(api b POST /v1/eu)
ec=$(api c POST /v1/eu); ed=$(api d POST /v1/eu)
checa "nivel nasce -1 (nao respondeu)" 1 "$(tem "$ea" '"alcance":-1')"
a2=$(api a POST /v1/eu '{"nome":"Júlia","avatar":"https://evil.example/x.png"}' 2)
checa "perfil 2 e outra pessoa" 1 "$(tem "$a2" '"id":"nuvio:aaa:2"')"
checa "com o nome do perfil" 1 "$(tem "$a2" '"nome":"Júlia"')"
checa "foto de host desconhecido e descartada" 1 "$(tem "$a2" '"avatar":""')"
checa "e codigo proprio" 0 "$(tem "$a2" "\"codigo\":\"$(codigo "$ea")\"")"
a2b=$(api a POST /v1/eu '{"avatar":"https://walter.trakt.tv/a/ok.jpg"}' 2)
checa "foto de host conhecido fica" 1 "$(tem "$a2b" 'walter.trakt.tv/a/ok.jpg')"
checa "corpo sem nome nao apaga o nome do perfil" 1 "$(tem "$a2b" '"nome":"Júlia"')"
checa "perfil invalido cai na pessoa principal" 1 "$(tem "$(api a POST /v1/eu '' abc)" '"id":"nuvio:aaa"')"
checa "perfil fora da faixa tambem" 1 "$(tem "$(api a POST /v1/eu '' 99)" '"id":"nuvio:aaa"')"
checa "principal manteve o nome da conta" 1 "$(tem "$(api a POST /v1/eu)" '"nome":"Henrique"')"

# A <-> B por codigo; B <-> C por codigo. D nao conhece ninguem.
api a POST /v1/contatos "{\"codigo\":\"$(codigo "$eb")\"}" > /dev/null
api b POST /v1/contatos "{\"codigo\":\"$(codigo "$ec")\"}" > /dev/null
ct=$(api b GET /v1/contatos)
checa "contato diz por onde" 1 "$(tem "$ct" '"via":"codigo"')"
checa "e desde quando" 1 "$(tem "$ct" '"desde":[1-9]')"

r=$(api a POST /v1/eu/nome '{"nome":"Rique <b>@x.com"}')
checa "nome de exibicao limpo (sem @ . <>)" 1 "$(tem "$r" '"exibicao":"Rique b x com"')"
checa "amigo ve o nome de exibicao" 1 "$(tem "$(api b GET /v1/contatos)" '"nome":"Rique b x com"')"
api a POST /v1/eu/nome '{"nome":""}' > /dev/null
checa "apagar a exibicao volta ao nome da conta" 1 "$(tem "$(api b GET /v1/contatos)" '"nome":"Henrique"')"

# --- nivel 0 / nao respondeu: nada grava, nada vaza ---------------------------
r=$(ev a '{"ev":"inicio","imdb":"tt0000001","midia":"movie","titulo":"Filme Um","seg":60}')
checa "nao respondeu: evento nao e guardado" 1 "$(tem "$r" '"guardado":0')"
checa "nada no banco (evento/agregado/agora)" "0 0 0" "$(sql "SELECT COUNT(*) FROM evento") $(sql "SELECT COUNT(*) FROM agregado") $(sql "SELECT COUNT(*) FROM agora")"
checa "feed do amigo vazio" 1 "$(tem "$(api b GET /v1/feed)" '"itens":\[\]')"
pa=$(api b GET '/v1/amigo?id=nuvio:aaa')
checa "perfil do amigo abre" 1 "$(tem "$pa" '"nome":"Henrique"')"
checa "mas sem atividade" 1 "$(tem "$pa" '"compartilha":0,"mes":null,"agora":null,"gostou":\[\]')"

# --- nivel 1: so amigos ---------------------------------------------------------
checa "A escolhe nivel 1" 1 "$(tem "$(api a POST /v1/alcance '{"nivel":1}')" '"alcance":1')"
ev a '{"ev":"inicio","imdb":"tt0000001","midia":"movie","titulo":"Filme Um","poster":"https://evil.example/p.jpg","rec":0}' > /dev/null
ev a '{"ev":"inicio","imdb":"tt0000001","midia":"movie","titulo":"Filme Um"}' > /dev/null
checa "inicio repetido em 10 min conta uma vez" 1 "$(sql "SELECT COUNT(*) FROM evento WHERE pessoa='nuvio:aaa' AND ev='inicio'")"
ev a '{"ev":"progresso","imdb":"tt0000001","midia":"movie","titulo":"Filme Um","pct":40,"seg":1800}' > /dev/null
fb=$(api b GET /v1/feed)
checa "inicio de filme nao entra no feed (o agora mostra)" 0 "$(tem "$fb" '"ev":"inicio"')"
ev a '{"ev":"inicio","imdb":"tt0000009","midia":"series","titulo":"Serie","temporada":1,"episodio":1}' > /dev/null
fb=$(api b GET /v1/feed)
checa "amigo ve o comeco de uma serie" 1 "$(tem "$fb" '"ev":"inicio"')"
checa "progresso nao entra no feed" 0 "$(tem "$fb" '"ev":"progresso"')"
checa "capa de host desconhecido nao sai" 0 "$(tem "$fb" 'evil.example')"
checa "amigo de amigo (C) nao ve no nivel 1" 1 "$(tem "$(api c GET /v1/feed)" '"itens":\[\]')"
checa "estranho (D) nao ve" 1 "$(tem "$(api d GET /v1/feed)" '"itens":\[\]')"
pa=$(api b GET '/v1/amigo?id=nuvio:aaa')
checa "assistindo agora aparece (o mais novo: a serie)" 1 "$(tem "$pa" '"agora":{"imdb":"tt0000009"')"
checa "segundos do mes somam" 1 "$(tem "$pa" '"seg":1800')"
# Sair do player (06/10/2026): fecha o "agora", nunca vira linha de evento/feed.
ev a '{"ev":"progresso","imdb":"tt0000009","midia":"series","titulo":"Serie","temporada":1,"episodio":1,"pct":30,"seg":60,"parou":1}' > /dev/null
checa "parou=1 fecha o agora" 1 "$(tem "$(api b GET '/v1/amigo?id=nuvio:aaa')" '"agora":null')"
ev a '{"ev":"abandono","imdb":"tt0000008","midia":"movie","titulo":"Largado","pct":5,"seg":30}' > /dev/null
checa "abandono (cliente antigo) nao e guardado" 0 "$(sql "SELECT COUNT(*) FROM evento WHERE ev='abandono'")"
checa "nem aparece no feed" 0 "$(tem "$(api b GET /v1/feed)" '"ev":"abandono"')"
checa "C nao abre o perfil de A no nivel 1" 1 "$(tem "$(api c GET "/v1/amigo?id=pub:$(sql "SELECT pub FROM perfil WHERE pessoa='nuvio:aaa'")")" 'nao encontrado')"

# ETag/304
curl -s -D /tmp/nv-feed.h -o /dev/null -H "authorization: Bearer tok-b" -H "x-nuvio-auth: nuvio" "$BASE/v1/feed"
et=$(grep -i '^etag:' /tmp/nv-feed.h | tr -d '\r' | cut -d' ' -f2)
checa "feed responde 304 com o mesmo etag" 304 "$(curl -s -o /dev/null -w '%{http_code}' \
  -H "authorization: Bearer tok-b" -H "x-nuvio-auth: nuvio" -H "if-none-match: $et" "$BASE/v1/feed")"

# "assistindo agora" vence em 15 min sem evento novo
sql "UPDATE agora SET atualizado = atualizado - 901 WHERE pessoa='nuvio:aaa'"
checa "assistindo agora vence em 15 min" 1 "$(tem "$(api b GET '/v1/amigo?id=nuvio:aaa')" '"agora":null')"

# fim de filme conta no mes e tira o "agora"
ev a '{"ev":"progresso","imdb":"tt0000001","midia":"movie","pct":80,"seg":600}' > /dev/null
ev a '{"ev":"fim","imdb":"tt0000001","midia":"movie","titulo":"Filme Um","pct":100,"seg":300}' > /dev/null
ev a '{"ev":"inicio","imdb":"tt0000009","midia":"series","titulo":"Serie","temporada":1,"episodio":2}' > /dev/null
pa=$(api b GET '/v1/amigo?id=nuvio:aaa')
checa "agregado: seg, filmes e series do mes" 1 "$(tem "$pa" '"seg":2790,"filmes":1,"series":1')"

# --- rec de origem: B manda a A, A reage ----------------------------------------
api b POST /v1/rec '{"para":"nuvio:aaa","imdb":"tt0000002","tipo":"movie","titulo":"Filme Dois"}' > /dev/null
rid=$(sql "SELECT id FROM rec WHERE de='nuvio:bbb' AND para='nuvio:aaa' ORDER BY id DESC LIMIT 1")
checa "rec nasce entregue" 1 "$(tem "$(api b GET '/v1/amigo?id=nuvio:aaa')" '"estado":"entregue"')"
ev a "{\"ev\":\"inicio\",\"imdb\":\"tt0000002\",\"midia\":\"movie\",\"rec\":$rid}" > /dev/null
checa "comecou" 1 "$(tem "$(api b GET '/v1/amigo?id=nuvio:aaa')" '"estado":"comecou"')"
ev a "{\"ev\":\"reacao\",\"imdb\":\"tt0000002\",\"midia\":\"movie\",\"reacao\":1,\"rec\":$rid}" > /dev/null
pa=$(api b GET '/v1/amigo?id=nuvio:aaa')
checa "quem mandou ve a reacao" 1 "$(tem "$pa" '"estado":"reacao","reacao":1')"
checa "gostou recentemente" 1 "$(tem "$pa" '"gostou":\[{"imdb":"tt0000002"')"
# rec de outro: o id nao e de uma rec para C, nada muda
api c POST /v1/alcance '{"nivel":1}' > /dev/null
ev c "{\"ev\":\"reacao\",\"imdb\":\"tt0000002\",\"reacao\":-1,\"rec\":$rid}" > /dev/null
checa "rec alheia nao e alterada" 1 "$(sql "SELECT reacao FROM rec WHERE id=$rid")"

# --- gosto parecido ---------------------------------------------------------------
checa "B que nao compartilha nao ve gosto" 1 "$(tem "$pa" '"gosto":null')"
api b POST /v1/alcance '{"nivel":1}' > /dev/null
ev b '{"ev":"reacao","imdb":"tt0000002","reacao":1}' > /dev/null
ev a '{"ev":"reacao","imdb":"tt0000003","reacao":1}' > /dev/null
ev b '{"ev":"reacao","imdb":"tt0000003","reacao":-1}' > /dev/null
checa "gosto parecido 50% de 2" 1 "$(tem "$(api b GET '/v1/amigo?id=nuvio:aaa')" '"gosto":{"total":2,"iguais":1,"pct":50,')"

# --- nivel 2: amigo de amigo -------------------------------------------------------
api a POST /v1/alcance '{"nivel":2}' > /dev/null
fc=$(api c GET /v1/feed)
checa "C (amigo de amigo) ve A no nivel 2" 1 "$(tem "$fc" '"grau":2')"
checa "sem o id da conta" 0 "$(tem "$fc" 'nuvio:aaa')"
checa "com handle opaco" 1 "$(tem "$fc" '"de":"pub:[a-z0-9]\{10\}"')"
checa "e por quem (via)" 1 "$(tem "$fc" '"via":"Gustavo"')"
checa "sem a foto" 0 "$(tem "$fc" '"grau":2[^}]*"deAvatar":"https')"
pub=$(printf '%s' "$fc" | sed -E 's/.*"de":"(pub:[a-z0-9]{10})".*/\1/')
pc=$(api c GET "/v1/amigo?id=$pub")
checa "C abre o perfil de A pelo handle" 1 "$(tem "$pc" '"grau":2')"
checa "sem recs (nao sao contatos)" 1 "$(tem "$pc" '"recs":\[\]')"
checa "D (sem ligacao) continua sem ver" 1 "$(tem "$(api d GET /v1/feed)" '"itens":\[\]')"
checa "D nao abre pelo handle" 1 "$(tem "$(api d GET "/v1/amigo?id=$pub")" 'nao encontrado')"
api c POST /v1/bloquear '{"id":"nuvio:aaa"}' > /dev/null
checa "bloqueio tira do feed" 0 "$(tem "$(api c GET /v1/feed)" '"grau":2')"
api c POST /v1/desbloquear '{"id":"nuvio:aaa"}' > /dev/null

# --- de volta a 0: apaga ----------------------------------------------------------
api a POST /v1/alcance '{"nivel":0}' > /dev/null
checa "nivel 0 apaga eventos, agora e agregados" "0 0 0" \
  "$(sql "SELECT COUNT(*) FROM evento WHERE pessoa='nuvio:aaa'") $(sql "SELECT COUNT(*) FROM agora WHERE pessoa='nuvio:aaa'") $(sql "SELECT COUNT(*) FROM agregado WHERE pessoa='nuvio:aaa'")"
checa "e o feed do amigo esvazia" 0 "$(tem "$(api b GET /v1/feed)" '"de":"nuvio:aaa"')"

# --- contrato antigo continua ---------------------------------------------------------
checa "corpo antigo de /v1/atividade continua aceito" 1 \
  "$(tem "$(ev b '{"imdb":"tt0000004","tipo":"movie","titulo":"X","agora":0}')" '"ok":1')"
checa "ev invalido da 400" 400 "$(curl -s -o /dev/null -w '%{http_code}' -X POST -H 'authorization: Bearer tok-b' \
  -H 'x-nuvio-auth: nuvio' -d '{"ev":"hackear","imdb":"tt1"}' "$BASE/v1/atividade")"

# --- migracao 006 sobre linha antiga --------------------------------------------------
# Pessoa de antes da migracao, sem nome (o caso do UUID). Roda SO os UPDATE do
# arquivo (os ALTER ja passaram em preparar-local.sh).
sql "INSERT INTO pessoa (id, nome, codigo, avatar, criado, visto) VALUES ('nuvio:velho', '', 'zz9zz9', '', 1, 1);"
sql "INSERT INTO pessoa (id, nome, codigo, avatar, criado, visto, nome_conta) VALUES ('nuvio:velho2', 'Marta', 'zz8zz8', '', 1, 1, '');"
grep '^UPDATE' servidor/recomendacoes/migracao-006-social.sql | sqlite3 "$NV_D1"
checa "migracao: sem nome vira Amigo #rowid" "Amigo #$(sql "SELECT rowid FROM pessoa WHERE id='nuvio:velho'")" \
  "$(sql "SELECT nome FROM pessoa WHERE id='nuvio:velho'")"
checa "migracao: nome antigo vira nome da conta" "Marta|Marta" "$(sql "SELECT nome, nome_conta FROM pessoa WHERE id='nuvio:velho2'")"
checa "migracao: nivel de quem ja existia fica -1" "-1" "$(sql "SELECT alcance FROM pessoa WHERE id='nuvio:velho2'")"

printf '\n%d ok, %d falharam\n' "$ok" "$falhou"
[ "$falhou" -eq 0 ]
