#!/bin/bash
# Monta o site static que o Cloudflare Worker serve para Hisense VIDAA.
#
# O alvo VIDAA carrega uma URL hospedada, nao um pacote de sistema. Este script
# recolhe os artefatos do build (dois: mt/ com pthreads, st/ single-thread) e
# os publica no layout esperado pelo wrangler — build/vidaa-site/tv/<versao>/
# — com as 6 opcoes de download, icones redimensionados e um arquivo de versao
# para descoberta.
#
# Entrada: build/vidaa/mt/ e build/vidaa/st/ (output de tools/tizen.sh --vidaa)
# Saida: build/vidaa-site/tv/ com versoes vivas e cleanup automatico dos 3
#        mais recentes.
set -e
cd "$(dirname "$0")/.."

# Ambiente: o input e o output podem ser sobrescitos com VIDAA_ENTRADA e
# VIDAA_SAIDA para testes.
ENTRADA="${VIDAA_ENTRADA:-build/vidaa}"
SAIDA="${VIDAA_SAIDA:-build/vidaa-site}"

# Guarda de arquivo: confere se ambos os dirs existem e tem os arquivos
# esperados. A presenca do .nuvio-build-stamp prova que tizen.sh foi rodado,
# nao so que os dirs existem.
for modo in mt st; do
  for f in index.html index.js index.wasm index.data decodificador.js hls.min.js .nuvio-build-stamp; do
    arquivo="$ENTRADA/$modo/$f"
    [ -f "$arquivo" ] || {
      echo "vidaa-site.sh: falta $modo/$f em $ENTRADA" >&2
      exit 1
    }
  done
done

# Valida versao: extrai de appinfo.json e confere que e a mesma em ambas as
# builds. O format= deve ser 1, e os config-fingerprint devem ser IGUAIS (mesma
# compilacao, mesma config); os wasm-sha256 devem bater com o arquivo de cada
# diretorio.
sha256() {
  if command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$@" | awk '{print $1}'
  else
    sha256sum "$@" | awk '{print $1}'
  fi
}

VERSAO=$(sed -n 's/^[[:space:]]*"version":[[:space:]]*"\([^"]*\)".*/\1/p' "deploy/app/appinfo.json" | head -1)
[ -n "$VERSAO" ] || {
  echo "vidaa-site.sh: versao vazia em deploy/app/appinfo.json" >&2
  exit 1
}

# Valida .nuvio-build-stamp em ambas as builds: format, config-fingerprint
# igualmente em mt/ e st/, e wasm-sha256 bate com o index.wasm de cada um.
for modo in mt st; do
  STAMP="$ENTRADA/$modo/.nuvio-build-stamp"
  FMT=$(sed -n 's/^format=//p' "$STAMP" | head -1)
  [ "$FMT" = "1" ] || {
    echo "vidaa-site.sh: $modo/.nuvio-build-stamp format invalido: $FMT" >&2
    exit 1
  }

  CONF_FP=$(sed -n 's/^config-fingerprint=//p' "$STAMP" | head -1)
  WASM_SHA=$(sed -n 's/^wasm-sha256=//p' "$STAMP" | head -1)

  [ -n "$CONF_FP" ] || {
    echo "vidaa-site.sh: $modo/.nuvio-build-stamp sem config-fingerprint" >&2
    exit 1
  }
  [ -n "$WASM_SHA" ] || {
    echo "vidaa-site.sh: $modo/.nuvio-build-stamp sem wasm-sha256" >&2
    exit 1
  }

  # Primeiro modo (mt) guarda o config-fingerprint esperado.
  if [ "$modo" = "mt" ]; then
    CONF_FP_ESPERADO="$CONF_FP"
    WASM_SHA_MT="$WASM_SHA"
  else
    # st deve bater com mt no config-fingerprint (mesma build, mesma config).
    [ "$CONF_FP" = "$CONF_FP_ESPERADO" ] || {
      echo "vidaa-site.sh: st/ tem config diferente de mt/" >&2
      echo "  mt: $CONF_FP_ESPERADO" >&2
      echo "  st: $CONF_FP" >&2
      exit 1
    }
  fi

  # Confere que o hash no stamp bate com o arquivo de verdade.
  WASM_SHA_REAL=$(sha256 "$ENTRADA/$modo/index.wasm")
  [ "$WASM_SHA" = "$WASM_SHA_REAL" ] || {
    echo "vidaa-site.sh: $modo/index.wasm foi alterado apos o build" >&2
    echo "  stamp: $WASM_SHA" >&2
    echo "  real:  $WASM_SHA_REAL" >&2
    exit 1
  }
done

# Cria saida, filtra e copia os 6 arquivos de cada modo.
TV_DIR="$SAIDA/tv"
VERSION_DIR="$TV_DIR/$VERSAO"
mkdir -p "$VERSION_DIR"/mt "$VERSION_DIR"/st

echo "vidaa-site.sh: montando $VERSION_DIR"

for modo in mt st; do
  # Os arquivos que DEVEM ir (nao a .nuvio-build-stamp, ni o resto).
  for arquivo in index.html index.js index.wasm index.data decodificador.js hls.min.js; do
    src="$ENTRADA/$modo/$arquivo"
    dst="$VERSION_DIR/$modo/$arquivo"

    # Guarda de tamanho: Workers Assets recusa arquivo acima de 25 MiB.
    SIZE=$(stat -f%z "$src" 2>/dev/null || stat -c%s "$src" 2>/dev/null)
    MAX_SIZE=$((25 * 1024 * 1024))
    if [ "$SIZE" -gt "$MAX_SIZE" ]; then
      echo "vidaa-site.sh: $modo/$arquivo excede limite de 25 MiB: $((SIZE / 1024 / 1024)) MiB" >&2
      exit 1
    fi

    cp "$src" "$dst"
  done
done

# Icones: redimensiona de deploy/app/tizen/icon.png com sips (macOS).
# Hisense pede 220x220 e 400x400.
ICON_SRC="deploy/app/tizen/icon.png"
[ -f "$ICON_SRC" ] || ICON_SRC="deploy/app/icon.png"
[ -f "$ICON_SRC" ] || {
  echo "vidaa-site.sh: icone nao encontrado" >&2
  exit 1
}

if command -v sips >/dev/null 2>&1; then
  echo "vidaa-site.sh: redimensionando icones"
  sips -z 220 220 "$ICON_SRC" --out "$TV_DIR/icone-220.png" >/dev/null 2>&1
  sips -z 400 400 "$ICON_SRC" --out "$TV_DIR/icone-400.png" >/dev/null 2>&1
elif command -v magick >/dev/null 2>&1; then
  echo "vidaa-site.sh: redimensionando icones com ImageMagick (magick)"
  magick "$ICON_SRC" -resize 220x220 "$TV_DIR/icone-220.png"
  magick "$ICON_SRC" -resize 400x400 "$TV_DIR/icone-400.png"
elif command -v convert >/dev/null 2>&1; then
  echo "vidaa-site.sh: redimensionando icones com ImageMagick (convert)"
  convert "$ICON_SRC" -resize 220x220 "$TV_DIR/icone-220.png"
  convert "$ICON_SRC" -resize 400x400 "$TV_DIR/icone-400.png"
else
  echo "vidaa-site.sh: erro — sem sips, magick ou convert para gerar icones" >&2
  exit 1
fi
[ -s "$TV_DIR/icone-220.png" ] && [ -s "$TV_DIR/icone-400.png" ] || {
  echo "vidaa-site.sh: erro — icones obrigatorios nao foram gerados" >&2
  exit 1
}

# Arquivo de versao: apenas o numero, sem newline. Qualquer outro dado e overhead.
printf "%s" "$VERSAO" > "$TV_DIR/versao.txt"

# Rotacao: guarda 3 versoes, deleta o resto.
#
# CONSERVADOR DE PROPOSITO: os builds de uma versao anterior podem ainda estar
# sendo carregados por um aparelho ligado no meio de uma sessao. Deletar
# versoes antidas muito rápido quebra aquela TV.
echo "vidaa-site.sh: rotacao de versoes (manter 3 mais recentes)"
ls -d "$TV_DIR"/[0-9]* 2>/dev/null | sort -V -r | tail -n +4 |
while IFS= read -r v; do
  [ -n "$v" ] || continue
  echo "  deletando $v"
  rm -rf -- "$v"
done

echo "vidaa-site.sh: pronto em $VERSION_DIR"
echo "vidaa-site.sh: publicar com:"
echo "  cd servidor/recomendacoes && npx wrangler deploy"
