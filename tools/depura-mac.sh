#!/bin/bash
# Reproducao local do travamento, com NUVIO_DADOS DESCARTAVEL.
#
# NUNCA ~/.nuvio: aquela e a pasta real do dono e um teste ja destruiu o perfil
# dele uma vez. O argumento de dados_iniciar e a pasta de ARTE, nao a de dados —
# quem manda na pasta de dados e a variavel de ambiente.
set -e
cd "$(dirname "$0")"
export NUVIO_DADOS=/tmp/nuvio-crash-$$
mkdir -p "$NUVIO_DADOS"
cc src/*.c -o /tmp/nuvio-crash -O0 -g \
  -DNV_SUPABASE_URL='""' -DNV_SUPABASE_ANON_KEY='""' -DNV_TV_LOGIN_BASE='""' \
  -DNV_TRAKT_CLIENT_ID='""' -DNV_TRAKT_CLIENT_SECRET='""' \
  -DNV_SIMKL_CLIENT_ID='""' -DNV_SIMKL_APP='""' -DNV_TMDB_API_KEY='""' \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf \
  -framework OpenGL -Wno-deprecated-declarations 2>/dev/null
echo "dados descartaveis em $NUVIO_DADOS"
lldb -b -o run -o "bt all" -o quit -- /tmp/nuvio-crash "$(pwd)/deploy/app/art" 2>&1
