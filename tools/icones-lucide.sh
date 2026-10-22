#!/bin/bash
# Icones dos AJUSTES, do Lucide (lucide.dev, licenca ISC — texto em
# deploy/app/art/icones/lucide/LICENSE).
#
# POR QUE EXISTE: os Ajustes usavam os mesmos seis ou sete PNG do app web para
# tudo ("aspecto" era Cartazes, Diagnostico, memoria, veu e o destaque; o
# "menu_settings" era idioma, tema, versao e envio de registro). O dono pediu
# (25/09/2026) icones que digam o que a linha e. O Lucide tem desenho para
# quase tudo e UM so estilo — traco 2 em grade 24, ponta e junta redondas —,
# entao a tela inteira fica com o mesmo peso de linha.
#
# O QUE FAZ, nesta ordem:
#   1. baixa cada SVG da TAG fixa $TAG (nao de main: o desenho de main muda sem
#      aviso e o PNG deixaria de bater com o SVG guardado) para
#      deploy/app/art/icones/lucide/<nome>.svg, SEM EDITAR — o arquivo e o do
#      upstream, e e a prova de origem;
#   2. rasteriza em deploy/app/art/icones/aj_<nome>.png, 128x128, BRANCO sobre
#      transparente, forma no ALFA (gfx_icone tinge pelo alfa — ver gfx.h).
#
# Rasterizacao: o mesmo caminho do svg2png original de gfx.h (currentColor ->
# branco, width/height injetados, sips), so que a 512 e reduzido a 128 com
# filtro box pelo ImageMagick: a borda do traco sai com alfa limpo em vez do
# degrau do sips a 128. Nenhum padding extra: a grade 24 do Lucide ja deixa
# ~2/24 de respiro, e o glifo sai com caixa de ~108 px — a mesma de play.png,
# addon.png e episodios.png, que e o que faz o peso optico casar.
#
# O prefixo aj_ separa estes dos PNG que outras telas usam (menu, player,
# detalhe): trocar um icone aqui nunca muda outra tela.
#
# Uso: bash tools/icones-lucide.sh            (baixa e rasteriza)
#      bash tools/icones-lucide.sh --offline  (so rasteriza os SVG guardados)
# Precisa de: curl, sips (macOS), magick.
set -e
cd "$(dirname "$0")/.."

TAG="1.48.0"
BASE="https://raw.githubusercontent.com/lucide-icons/lucide/$TAG/icons"
DIR="deploy/app/art/icones"
SVG="$DIR/lucide"

# Um nome por linha. Nos Ajustes o icone mora SO nas nove categorias e nos
# cabecalhos dos grupos recolhiveis (TELA[] em src/ajustes.c) — as linhas nao
# tem icone desde o merge da 1.5 (feat/ajustes-ux), e os ~60 desenhos por
# opcao sairam da lista. fileiras.c usa house/folders/puzzle na folha de
# fileiras. O menu do cartaz (ctxmenu.c) usa info/library/eye/eye-off/users/
# x/rows-3/folders, o traco do mockup "ilha" tela 7.
NOMES="
circle-play file-text folders house
images info layout-dashboard monitor-cog palette panel-top plug puzzle
rotate-ccw-clock rows-3 star user-round mic keyboard smartphone
rotate-cw x library eye eye-off users
clock triangle-alert tv-minimal-play calendar download wifi-off wifi link-2-off
cloud megaphone bookmark
clapperboard languages search chevron-right check plus minus memory-stick delete
activity gauge zap database
arrow-up-down heart moon sun-dim
arrow-up arrow-down pencil
pause send scroll-text server-crash shield-check cpu log-out
panel-left list-x bell audio-lines layers type sliders-horizontal book-open user-plus
alarm-clock badge-check captions chart-column chevrons-down circle-check circle-dot compass droplet feather film funnel globe image image-plus inbox key key-round layout-list life-buoy link list-ordered list-video loader log-in map maximize-2 menu message-square monitor-play mouse-pointer move-horizontal pause picture-in-picture-2 play quote send server settings-2 shield sparkle stethoscope telescope thumbs-up unlink wand sparkles corner-up-left book-open-text
pipette blend spotlight paint-roller
ellipsis
hard-drive volume-2 package tag
"
# A linha que comeca em "clock" e da ILHA DO RELOGIO (02/10, mockup aprovado em design/ilha):
# os icones dos avisos (alerta, wifi, debrid baixando, Trakt desconectado...) e
# dos botoes dos modais (Depois, Salvar, Reconectar).
# A antepenultima e dos AJUSTES no Glass UI (03/10, mockup aprovado): Trailers e
# Idiomas com desenho proprio, a lupa e a seta das linhas, o "Salvo", o mais e
# o menos do editor numerico e a memoria de imagens.
# A penultima e do REGISTRO DO APP no Glass UI (03/10, logs-mockup.html): pausa,
# envio, painel vazio, erro do servidor, consentimento e o rastro de etapas.
# As duas ultimas sao do cartao de NOVIDADES DA 1.8.0 e do GUIA DE USO
# (ajustes_ux_guia_dados.inc, um icone por capitulo e por recurso) (03/10, mockup aprovado):
# os discos da lista, as pecas da previa, capitulos e recursos do guia.
# A penultima linha e da REGUA de cores (05/10): um desenho por dinamica (Da arte,
# Gradiente, Imersiva, Textura), que antes mostravam todas a mesma miniatura.
# A ultima e do "..." da pagina do titulo (06/10): o circular "Mais opcoes" que
# guarda trailer, Explorar, Trocar arte e Recomendar (src/detmais.h).
# A de baixo e do TEXTO DO ADDON na folha de fontes (07/10): os emojis do AIOStreams
# (💾 👤 🎞️ 🔊 🌐 ⚙️ ⚡ 📦 🏷️ ⏱️) viram estes icones — a TV nao tem emoji colorido.
# Conferencia: todo aj_* citado em src/ tem de estar em NOMES, e todo NOMES
# tem de ser citado — senao sobra PNG morto no pacote ou falta icone na tela
# (gfx_icone falha em silencio).

mkdir -p "$SVG"
if [ "$1" != "--offline" ]; then
  curl -fsSL -o "$SVG/LICENSE" "https://raw.githubusercontent.com/lucide-icons/lucide/$TAG/LICENSE"
  for n in $NOMES; do
    curl -fsSL -o "$SVG/$n.svg" "$BASE/$n.svg" || { echo "icones-lucide: $n nao existe em $TAG" >&2; exit 1; }
  done
fi

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
for n in $NOMES; do
  sed -e 's/currentColor/#ffffff/g' -e 's/width="24"/width="512"/' \
      -e 's/height="24"/height="512"/' "$SVG/$n.svg" > "$TMP/a.svg"
  sips -s format png "$TMP/a.svg" --out "$TMP/a.png" >/dev/null
  magick "$TMP/a.png" -alpha extract -filter Box -resize 128x128 "$TMP/m.png"
  magick -size 128x128 xc:white "$TMP/m.png" -alpha off -compose CopyOpacity \
    -composite -strip -define png:color-type=6 "$DIR/aj_$n.png"
done

# Sobra de uma lista anterior: sai, para o pacote nao levar icone que ninguem usa.
for f in "$DIR"/aj_*.png "$SVG"/*.svg; do
  n=$(basename "$f"); n=${n#aj_}; n=${n%.png}; n=${n%.svg}
  case " $(echo $NOMES) " in *" $n "*) ;; *) rm -f "$f"; echo "icones-lucide: removido $f" ;; esac
done

USADOS=$(grep -ho '"aj_[a-z0-9-]*"' src/*.c src/*.inc | tr -d '"' | sed 's/^aj_//' | sort -u)
FALHA=0
for n in $USADOS; do
  case " $(echo $NOMES) " in *" $n "*) ;; *) echo "icones-lucide: src/ usa aj_$n, que nao esta em NOMES" >&2; FALHA=1 ;; esac
done
for n in $NOMES; do
  printf '%s\n' "$USADOS" | grep -qx "$n" || { echo "icones-lucide: aj_$n nao e usado em src/" >&2; FALHA=1; }
done
[ "$FALHA" = 0 ] || exit 1
echo "icones-lucide: $(echo $NOMES | wc -w | tr -d ' ') icones em $DIR/aj_*.png (Lucide $TAG)"
