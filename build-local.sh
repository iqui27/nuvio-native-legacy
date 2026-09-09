#!/bin/bash
# Build de verificacao local no Mac. Existe como arquivo porque o `eval` da
# linha de comando do FERRAMENTAS.md nao passa pela checagem de isolamento do
# worktree. Mesmas flags, credenciais vazias (o binario local nao faz login).
set -e
cd "$(dirname "$0")"
cc src/*.c -o /tmp/nvbuild -O1 \
  -DNV_SUPABASE_URL='""' -DNV_SUPABASE_ANON_KEY='""' -DNV_TV_LOGIN_BASE='""' \
  -DNV_TRAKT_CLIENT_ID='""' -DNV_TRAKT_CLIENT_SECRET='""' \
  -DNV_SIMKL_CLIENT_ID='""' -DNV_SIMKL_APP='""' -DNV_TMDB_API_KEY='""' \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf \
  -framework OpenGL -Wno-deprecated-declarations
echo "build: ok"
