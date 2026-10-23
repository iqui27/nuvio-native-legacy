#!/bin/bash
# Monta, num diretorio temporario, o SUBCONJUNTO da arte que pode ser
# distribuido — e so ele. Imprime o caminho no stdout, para tools/tizen.sh
# passar em --preload-file.
#
# POR QUE ESCOLHER EM VEZ DE LEVAR TUDO. deploy/app/art tem 236 MB, e a maior
# parte NAO E ASSET DO APP: e o acervo de quem empacotou.
#
#   collections/   165 MB, 3197 arquivos  -> o catalogo CURADO do dono do build.
#                                            Quem instalasse veria a colecao de
#                                            outra pessoa como se fosse sua. Ja
#                                            e pendencia conhecida do .ipk da LG.
#   cache/          18 MB                 -> poster baixado da rede, descartavel
#                                            por construcao; o tex_cache rebaixa.
#   catalogo-rede.bin 1,7 MB              -> retrato do catalogo pessoal. Cada
#                                            fileira leva a `base` do addon, e
#                                            no Xperience o JWT viaja DENTRO do
#                                            caminho: e credencial, com extensao
#                                            que nao parece. Hoje ele nasce em
#                                            dados_dir() (/nuvio, IDBFS) e nao
#                                            mais aqui, mas uma copia antiga do
#                                            deploy/ ainda o tem no lugar velho.
#   *.txt                                 -> CREDENCIAL DE PESSOA (trakt, tmdb,
#                                            mdblist) e ajustes. Nunca.
#
# O que sobra e o que o app precisa para nao nascer sem cara:
#   icones/ badges/ selos/ marcas/ prov/  ~1,0 MB -> cromo da interface (marcas/ inclui os logos do 2.0: logo-novo-*.png, logo-classico.png). Sem icones/ a
#                                             interface fica sem icone nenhum.
#   *.jpg da raiz                  8,1 MB  -> os backdrops da home. Sem eles o
#                                             log diz "home: nenhum backdrop".
#   editorial/ logo/ poster/        12 MB  -> arte curada das fileiras.
#   cinematic/                      24 MB  -> herois curados opcionais. Nao e
#                                             usado pelo seletor de fonte real
#                                             do hero e continua fora do pacote
#                                             padrao.
set -e
cd "$(dirname "$0")/.."

ORIGEM="deploy/app/art"
DESTINO="${NUVIO_ARTE_ESTAGIO:-build/art-pacote}"

rm -rf "$DESTINO"
mkdir -p "$DESTINO"

for d in icones icones-app badges selos marcas prov editorial logo poster ep elenco texturas; do
  [ -d "$ORIGEM/$d" ] && cp -R "$ORIGEM/$d" "$DESTINO/"
done
# Public trust roots only; never copy arbitrary PEM or token files.
cp "$ORIGEM/discord-ca.pem" "$DESTINO/discord-ca.pem"
# Backdrops da home: os .jpg numerados na raiz.
cp "$ORIGEM"/*.jpg "$DESTINO"/ 2>/dev/null || true
[ "${NUVIO_ARTE_CINEMATIC:-0}" = "1" ] && [ -d "$ORIGEM/cinematic" ] && cp -R "$ORIGEM/cinematic" "$DESTINO/"

# WEBP VIRA PNG, porque o alvo Tizen NAO SABE LER WEBP.
#
# O Emscripten nao tem port de libwebp: pedir -sSDL2_IMAGE_FORMATS com "webp"
# COMPILA a intencao e falha no meio do build do proprio SDL_image
# ("webp/decode.h file not found"). E sem o formato o arquivo esta no pacote e
# o app recusa em silencio, com o log dizendo "decode falhou (Unsupported image
# format)" — nao "No such file". Sao os 42 selos de badges/ (p-netflix, r-4k,
# a-dtshdma, co-x265...), ou seja, TODOS eles.
#
# Converter no estagio, e nao no repositorio: deploy/app/art continua como esta
# para os alvos LG e Mac, que leem webp sem problema. O nome do arquivo sai do
# campo "image" de badges/index.json, entao trocar a extensao la fecha o
# circuito sem mexer em uma linha de C (ver src/badges.c:63).
if find "$DESTINO" -name '*.webp' | grep -q .; then
  # CONVERTER: sips on macOS, ffmpeg on Linux — the test bench (galaxy) has no
  # sips and the whole build used to stop here. ffmpeg is already required by
  # tests/mkv_legendas.sh, and `scale=-1:64` reproduces sips' `--resampleHeight
  # 64` exactly (fixed height, width by aspect ratio).
  # (Note: `dwebp -getinfo` does not exist in Ubuntu's webp 1.5.0, and plain
  # `-scale` would need the width read and computed by hand. ffmpeg does both.)
  if command -v sips >/dev/null; then CONV=sips
  elif command -v ffmpeg >/dev/null; then CONV=ffmpeg
  else
    echo "tizen-art.sh: no webp converter (sips on macOS, ffmpeg on Linux)" >&2
    exit 1
  fi
  N=0
  for w in $(find "$DESTINO" -name '*.webp'); do
    # BADGES AT THEIR DRAWING HEIGHT (#159). The badges ship 194 px tall and are
    # drawn at 28 at most (BADGE_H); on Tizen each cost ~300 ms to decode
    # ("[tex] decode lento: 312 ms ... 663x194 (saiu 160x47)", records
    # 3595-3632) right when the source sheet opens. 64 px is ~2.3x the drawn
    # height: still sharp and decodes ~9x fewer pixels.
    RED=""
    case "$w" in */badges/*|*/selos/*) RED="--resampleHeight 64" ;; esac
    if [ "$CONV" = sips ]; then
      sips -s format png $RED "$w" --out "${w%.webp}.png" >/dev/null 2>&1 || {
        echo "tizen-art.sh: failed converting $w" >&2; exit 1; }
    elif [ -n "$RED" ]; then
      if ! ffmpeg -v error -i "$w" -vf "scale=-1:64:flags=lanczos" -y "${w%.webp}.png" 2>/dev/null; then
        echo "tizen-art.sh: failed converting $w" >&2; exit 1; fi
    else
      if ! ffmpeg -v error -i "$w" -y "${w%.webp}.png" 2>/dev/null; then
        echo "tizen-art.sh: failed converting $w" >&2; exit 1; fi
    fi
    rm -f "$w"; N=$((N+1))
  done
  # The index points at the old names; without this the app looks for a .webp
  # that is gone and one silent defect replaces another.
  # `sed -i ''` is BSD syntax: on GNU sed the '' becomes the script itself and
  # the command dies with "cannot read ...: No such file". `-i.bak` + rm works
  # on both.
  # selos/ has no index to rewrite: src/selospacote.c falls back from the .webp
  # name to .png by itself (the name is derived from each filter's imageURL).
  if [ -f "$DESTINO/badges/index.json" ]; then
    sed -i.bak 's/\.webp"/.png"/g' "$DESTINO/badges/index.json" && rm -f "$DESTINO/badges/index.json.bak"
  fi
  echo "tizen-art.sh: $N webp converted to png (via $CONV)" >&2
fi

# CONFERE QUE A ARTE DE MARCA ENTROU (D1 de logs: "res/art/marcas/abertura.jpg"
# e "login-fundo.jpg" faltando no .tpk em 5 TVs). marcas/ e copiada inteira
# acima, mas um estagio sem estes arquivos nao pode virar pacote em silencio:
# abertura e login caem no fundo liso, e os logos do 2.0 voltam ao Classico.
for f in abertura.jpg login-fundo.jpg logo-classico.png logo-novo-marca.png \
         logo-novo-simbolo.png logo-novo-horizontal.png nuvio_wordmark.png; do
  [ -s "$DESTINO/marcas/$f" ] || { echo "tizen-art.sh: marcas/$f ausente do estagio — abortado" >&2; exit 1; }
done

# CONFERE QUE NADA DE PESSOA ENTROU. Nao e paranoia: o .ipk ja saiu uma vez com
# art/trakt.txt dentro, entregando o token do dono a quem instalasse. A checagem
# vale mais que a intencao de quem editar este script depois.
#
# *.tmp entrou na varredura junto com *.bin: cat_gravar_cache escreve num
# temporario e so entao renomeia, e uma escrita interrompida deixa para tras um
# catalogo-rede.bin.tmp que ja tem as fileiras dentro — mesma credencial, outra
# extensao. "Outra extensao" e exatamente como collections.json passou pela
# lista de .txt do .ipk uma vez.
if find "$DESTINO" \( -name '*.txt' -o -name '*.bin' -o -name '*.tmp' \) | grep -q .; then
  echo "tizen-art.sh: ARQUIVO DE CREDENCIAL OU CATALOGO no estagio — abortado" >&2
  find "$DESTINO" \( -name '*.txt' -o -name '*.bin' -o -name '*.tmp' \) >&2
  exit 1
fi
if [ -d "$DESTINO/collections" ] || [ -d "$DESTINO/cache" ]; then
  echo "tizen-art.sh: collections/ ou cache/ no estagio — abortado" >&2
  exit 1
fi

# A UNICA EXCECAO A "NENHUM .txt", e ela entra DEPOIS da conferencia de
# proposito: addons-recomendados.txt e a lista curada que o guia mostra em
# "Sugestoes" — conteudo de pacote, nao de pessoa, com a regra de nunca conter
# URL com chave escrita no proprio cabecalho. Sem ele o guia Samsung abria a
# secao vazia (o 1.2.0 quase saiu assim). Se um dia esta linha for
# generalizada para "*.txt", o trakt.txt do dono volta a viajar no .wgt.
cp "$ORIGEM/addons-recomendados.txt" "$DESTINO/" 2>/dev/null || true

echo "tizen-art.sh: $(du -sh "$DESTINO" | cut -f1) em $DESTINO" >&2
echo "$DESTINO"
