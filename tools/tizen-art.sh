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
#   icones/ badges/ marcas/ prov/  532 KB  -> cromo da interface. Sem icones/ a
#                                             interface fica sem icone nenhum.
#   *.jpg da raiz                  8,1 MB  -> os backdrops da home. Sem eles o
#                                             log diz "home: nenhum backdrop".
#   editorial/ logo/ poster/        12 MB  -> arte curada das fileiras.
#   cinematic/                      24 MB  -> heroi animado. Opcional: ligue com
#                                             NUVIO_ARTE_CINEMATIC=1.
set -e
cd "$(dirname "$0")/.."

ORIGEM="deploy/app/art"
DESTINO="${NUVIO_ARTE_ESTAGIO:-build/art-pacote}"

rm -rf "$DESTINO"
mkdir -p "$DESTINO"

for d in icones badges marcas prov editorial logo poster ep elenco; do
  [ -d "$ORIGEM/$d" ] && cp -R "$ORIGEM/$d" "$DESTINO/"
done
# Backdrops da home: os .jpg numerados na raiz.
cp "$ORIGEM"/*.jpg "$DESTINO"/ 2>/dev/null || true
[ "${NUVIO_ARTE_CINEMATIC:-0}" = "1" ] && [ -d "$ORIGEM/cinematic" ] && cp -R "$ORIGEM/cinematic" "$DESTINO/"

# WEBP VIRA PNG, porque o alvo Tizen NAO SABE LER WEBP.
#
# O Emscripten nao tem port de libwebp: pedir -sSDL2_IMAGE_FORMATS com "webp"
# COMPILA a intencao e falha no meio do build do proprio SDL_image
# ("webp/decode.h file not found"). E sem o formato o arquivo esta no pacote e
# o app recusa em silencio, com o log dizendo "decode falhou (Unsupported image
# format)" — nao "No such file". Sao os 41 selos de badges/ (p-netflix, r-4k,
# a-dtshdma, co-x265...), ou seja, TODOS eles.
#
# Converter no estagio, e nao no repositorio: deploy/app/art continua como esta
# para os alvos LG e Mac, que leem webp sem problema. O nome do arquivo sai do
# campo "image" de badges/index.json, entao trocar a extensao la fecha o
# circuito sem mexer em uma linha de C (ver src/badges.c:63).
if find "$DESTINO" -name '*.webp' | grep -q .; then
  command -v sips >/dev/null || { echo "tizen-art.sh: sips ausente, nao da para converter webp" >&2; exit 1; }
  N=0
  for w in $(find "$DESTINO" -name '*.webp'); do
    sips -s format png "$w" --out "${w%.webp}.png" >/dev/null 2>&1 || {
      echo "tizen-art.sh: falhou convertendo $w" >&2; exit 1; }
    rm -f "$w"; N=$((N+1))
  done
  # O indice aponta para os nomes antigos; sem isto o app procura .webp que nao
  # existe mais e troca um defeito silencioso por outro.
  [ -f "$DESTINO/badges/index.json" ] && sed -i '' 's/\.webp"/.png"/g' "$DESTINO/badges/index.json"
  echo "tizen-art.sh: $N webp convertidos para png" >&2
fi

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

echo "tizen-art.sh: $(du -sh "$DESTINO" | cut -f1) em $DESTINO" >&2
echo "$DESTINO"
