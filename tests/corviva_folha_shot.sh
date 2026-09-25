#!/bin/bash
# Folha de contato da cor viva: fundo + logo de cada titulo com o destaque, o
# degrade, as luzes de regiao e a base que corviva_extrair tira deles, e o
# botao "Reproduzir" como sairia. E como a extracao foi afinada (25/09/2026);
# nao entra na suite (precisa de REDE para baixar as artes e de olho humano).
#
#   bash tests/corviva_folha_shot.sh /tmp/nuvio-folha        # -> /tmp/nuvio-folha.png
#
# As artes vem do metahub (fundo e logo pelo id do IMDb) e ficam em
# <saida>-artes/ para a proxima rodada nao baixar de novo.
set -eu
cd "$(dirname "$0")/.."
SAIDA="${1:-/tmp/nuvio-folha}"
D="$SAIDA-artes"
mkdir -p "$D"
cc -O2 tests/corviva_folha_shot.c src/corviva.c -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -o /tmp/nuvio-corviva-folha
args=()
while read -r tt nome; do
  [ -z "$tt" ] && continue
  [ -s "$D/$tt-bg.jpg" ]   || curl -sL -m 20 -o "$D/$tt-bg.jpg"   "https://images.metahub.space/background/medium/$tt/img"
  [ -s "$D/$tt-logo.png" ] || curl -sL -m 20 -o "$D/$tt-logo.png" "https://images.metahub.space/logo/medium/$tt/img"
  args+=("$nome" "$D/$tt-bg.jpg" "$D/$tt-logo.png")
done <<'EOF'
tt0264464 Catch_Me_If_You_Can
tt1160419 Dune
tt1517268 Barbie
tt0133093 The_Matrix
tt4633694 Spider-Verse
tt0903747 Breaking_Bad
tt4574334 Stranger_Things
tt0468569 The_Dark_Knight
tt2582802 Whiplash
tt0816692 Interstellar
tt6751668 Parasite
tt2543164 Arrival
tt1375666 Inception
tt0110912 Pulp_Fiction
tt3581920 The_Last_of_Us
tt0944947 Game_of_Thrones
tt1877830 The_Batman
tt2380307 Coco
tt0266543 Finding_Nemo
tt1745960 Top_Gun_Maverick
tt10872600 Spider-Man_NWH
tt0120338 Titanic
tt2560140 Attack_on_Titan
tt11280740 Severance
EOF
for i in 00 07 28; do args+=("arte$i" "deploy/app/art/$i.jpg" "-"); done
/tmp/nuvio-corviva-folha "$SAIDA.png" "${args[@]}"
