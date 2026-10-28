#!/bin/bash
# Native approved-v4 scenes, isolated fixtures, no account or network.
set -eu
cd "$(dirname "$0")/.."
output="${1:-/Volumes/ExternalSSD/tmp/rev/ajustes-v4-png}"
mkdir -p "$output"
export NUVIO_AJUSTES_UX=1
export NUVIO_AJ_QUADROS="v4-inicio-simples v4-inicio-avancado v4-secao v4-secao-grupos v4-submenu v4-opcao-lista v4-perfil v4-predefinicoes v4-diferentes v4-busca v4-review"
bash tests/ajustes_shot.sh "$output/app"
