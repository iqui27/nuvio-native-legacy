#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
quota_dir=$(mktemp -d /tmp/nuvio-seekrquota-settings.XXXXXX)
trap 'rm -rf "$quota_dir"' EXIT
# -undefined dynamic_lookup: o ajustes.c inteiro entra e o dead_strip nao
# corta tudo o que as telas novas (#311, #312) alcancam; o que a parte do Seekr
# nao chama fica sem resolver em vez de pedir dezenas de dubles.
cc src/dados.c tests/seekrquota_settings.c -Isrc -I/opt/homebrew/include \
  -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib -lSDL2 \
  -O1 -g -ffunction-sections -fdata-sections -Wl,-dead_strip -Wl,-undefined,dynamic_lookup \
  -Wno-deprecated-declarations -Wno-macro-redefined -o /tmp/nuvio-seekrquota-settings
NUVIO_DADOS="$quota_dir" /tmp/nuvio-seekrquota-settings
