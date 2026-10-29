#!/bin/bash
# #488: trocar de perfil pede nova volta da descoberta depois do sync_iniciar
#   bash tests/troca_perfil_repete_desc.sh
set -euo pipefail
cd "$(dirname "$0")/.."
f=src/app.c
a=$(grep -n 'desc_esquecer_catalogo_perfil();' "$f" | head -1 | cut -d: -f1)
b=$(awk -v a="$a" 'NR>a && /^[[:space:]]*sync_iniciar\(\);/ {print NR; exit}' "$f")
c=$(awk -v b="$b" 'NR>b && /perfilsel_preparar\(1, agora\);/ {print NR; exit}' "$f")
[ -n "$a" ] && [ -n "$b" ] && [ -n "$c" ] || { echo "FAIL: bloco de troca de perfil nao achado"; exit 1; }
awk -v b="$b" -v c="$c" 'NR>b && NR<c && /^[[:space:]]*desc_repetir\(\);/ {f=1} END{exit !f}' "$f" || { echo "FAIL #488: troca de perfil nao chama desc_repetir() apos sync_iniciar()"; exit 1; }
awk -v b="$b" -v c="$c" 'NR>b && NR<c && /^[[:space:]]*desc_esperar_conta\(\);/ {e=NR} NR>b && NR<c && /^[[:space:]]*desc_repetir\(\);/ {r=NR} END{exit !(e && r && e<r)}' "$f" || { echo "FAIL #488: desc_esperar_conta() deve vir antes de desc_repetir()"; exit 1; }
echo "ok: desc_repetir apos sync_iniciar, esperando a conta"
