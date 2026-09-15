#!/bin/bash
# Le local.properties (o MESMO arquivo do app web) e imprime os -D que a
# compilacao precisa. Nada de segredo entra em arquivo de codigo: o valor viaja
# da propriedade direto para a linha de comando do compilador.
#
# A anon key do Supabase e publica por projeto — e o que o RLS espera receber, e
# o app web ja a publica no bundle. Quem NAO pode entrar no pacote e o que hoje
# esta em art/trakt.txt e art/addons.txt: aquilo e credencial de PESSOA.
#
#   eval "cc src/*.c $(tools/env.sh) ..."
set -e

PROP="${NUVIO_PROPERTIES:-$(cd "$(dirname "$0")/../.." && pwd)/NuvioWeb-0.3.38-beta/local.properties}"

valor() {
  [ -f "$PROP" ] || return 0
  sed -n "s/^$1=//p" "$PROP" | head -1 | tr -d '\r'
}

URL=$(valor NUVIO_SUPABASE_URL)
KEY=$(valor NUVIO_SUPABASE_ANON_KEY)
TVB=$(valor TV_LOGIN_WEB_BASE_URL)
TRK=$(valor TRAKT_CLIENT_ID)
TRS=$(valor TRAKT_CLIENT_SECRET)
SMK=$(valor SIMKL_CLIENT_ID)
SMA=$(valor SIMKL_APP_NAME)
TMD=$(valor TMDB_API_KEY)
# A versao do app sai do appinfo.json — FONTE UNICA. Ela ja vivia em tres
# lugares (appinfo.json, tizen-config.xml e um #define em ajustes.c) e o
# terceiro ficou parado em 1.0.44 por nove releases: a tela de Ajustes mentia
# a versao. O binario recebe -DNV_VERSAO e a conferencia do arm.sh exige
# encontrar o valor dentro dele, como com as demais chaves.
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
VER=$(sed -n 's/^[[:space:]]*"version":[[:space:]]*"\([^"]*\)".*/\1/p' "$RAIZ/deploy/app/appinfo.json" | head -1)
VERT=$(grep -v "<?xml" "$RAIZ/tools/tizen-config.xml" | sed -n 's/.*[[:space:]]version="\([0-9.]*\)".*/\1/p' | head -1)
if [ -z "$VER" ]; then
  echo "env.sh: nao achei \"version\" em deploy/app/appinfo.json" >&2; exit 2
fi
if [ "$VER" != "$VERT" ]; then
  echo "env.sh: appinfo.json diz $VER e tizen-config.xml diz $VERT -- alinhe antes de compilar" >&2; exit 2
fi

if [ -z "$URL" ] || [ -z "$KEY" ]; then
  # Falhar em silencio produziria um .ipk que abre, mostra a tela de login e
  # nunca sai dela. O aviso vai para stderr para nao sujar os -D no stdout.
  echo "env.sh: sem NUVIO_SUPABASE_URL/ANON_KEY em $PROP -- o app vai compilar SEM login" >&2
fi

# --env-file: escreve as variaveis num arquivo para o `docker run --env-file`.
# Existe porque a compilacao ARM roda DENTRO de um container: passar os -D na
# linha de comando exigiria aspas dentro de aspas dentro de `sh -c`, e o erro
# ali e mudo — o compilador recebe a macro vazia e o app sai SEM LOGIN, que foi
# exatamente o que aconteceu no primeiro deploy para a TV.
if [ "$1" = "--env-file" ]; then
  [ -n "$2" ] || { echo "env.sh --env-file precisa do caminho" >&2; exit 2; }
  {
    printf 'NV_SUPABASE_URL=%s\n' "$URL"
    printf 'NV_SUPABASE_ANON_KEY=%s\n' "$KEY"
    printf 'NV_TV_LOGIN_BASE=%s\n' "$TVB"
    printf 'NV_TRAKT_CLIENT_ID=%s\n' "$TRK"
    printf 'NV_TRAKT_CLIENT_SECRET=%s\n' "$TRS"
    printf 'NV_SIMKL_CLIENT_ID=%s\n' "$SMK"
    printf 'NV_SIMKL_APP=%s\n' "$SMA"
    printf 'NV_TMDB_API_KEY=%s\n' "$TMD"
    printf 'NV_VERSAO=%s\n' "$VER"
  } > "$2"
  chmod 600 "$2"
  exit 0
fi

printf -- '-DNV_SUPABASE_URL=\\"%s\\" -DNV_SUPABASE_ANON_KEY=\\"%s\\" -DNV_TV_LOGIN_BASE=\\"%s\\" -DNV_TRAKT_CLIENT_ID=\\"%s\\" -DNV_TRAKT_CLIENT_SECRET=\\"%s\\" -DNV_SIMKL_CLIENT_ID=\\"%s\\" -DNV_SIMKL_APP=\\"%s\\" -DNV_TMDB_API_KEY=\\"%s\\" -DNV_VERSAO=\\"%s\\"' \
  "$URL" "$KEY" "$TVB" "$TRK" "$TRS" "$SMK" "$SMA" "$TMD" "$VER"
