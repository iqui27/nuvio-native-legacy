#!/bin/bash
# #482: trocar de perfil limpa biblioteca/vistos da conta (sementes do a seguir)
#   bash tests/troca_perfil_esquece_contalib.sh
set -euo pipefail
cd "$(dirname "$0")/.."
f=src/app.c
a=$(grep -n 'desc_esquecer_catalogo_perfil();' "$f" | head -1 | cut -d: -f1)
b=$(awk -v a="$a" 'NR>a && /^[[:space:]]*sync_iniciar\(\);/ {print NR; exit}' "$f")
c=$(awk -v b="$b" 'NR>b && /perfilsel_preparar\(1, agora\);/ {print NR; exit}' "$f")
[ -n "$a" ] && [ -n "$b" ] && [ -n "$c" ] || { echo "FAIL: bloco de troca de perfil nao achado"; exit 1; }
awk -v a="$a" -v b="$b" 'NR>a && NR<b && /^[[:space:]]*contalib_esquecer\(\);/ {f=1} END{exit !f}' "$f" || { echo "FAIL #482: troca de perfil nao chama contalib_esquecer()"; exit 1; }
echo "ok: contalib_esquecer na troca"
