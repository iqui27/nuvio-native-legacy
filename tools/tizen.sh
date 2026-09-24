#!/bin/bash
# Compila para Samsung Tizen: WebAssembly (Emscripten) dentro de um .wgt.
#
# POR QUE WASM E NAO UM BINARIO NATIVO. O perfil TV do Tizen Studio so gera
# .wgt; .tpk nativo e perfil mobile/wearable e o firmware da TV recusa sem
# certificado de parceiro. Tizen .NET para TV acabou e o NaCl foi encerrado em
# 2021. O que a Samsung oferece no lugar e WebAssembly, a partir do Tizen 5.5
# (modelos 2020). Entao "nativo no Tizen" = este mesmo src/*.c compilado para
# WASM, com o video pela API AVPlay atras de um canvas transparente.
#
# EMSDK UPSTREAM, NAO O FORK DA SAMSUNG. O fork so acrescenta as APIs de WASM
# Player / ML deles, que este port nao usa — o video sai pelo AVPlay em JS, que
# e API de web app comum. O emcc de upstream produz WASM padrao, que o Chromium
# da TV executa igual.
set -e
cd "$(dirname "$0")/.."

: "${EMSDK_DIR:=$HOME/emsdk}"
[ -f "$EMSDK_DIR/emsdk_env.sh" ] || {
  echo "tizen.sh: emsdk nao encontrado em $EMSDK_DIR" >&2
  echo "  git clone https://github.com/emscripten-core/emsdk ~/emsdk && cd ~/emsdk && ./emsdk install latest && ./emsdk activate latest" >&2
  exit 2
}
# shellcheck disable=SC1091
source "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1

# --alto-cache: a mesma variante do tools/arm.sh --alto-cache, para quem tem
# Samsung com RAM sobrando e quer testar o cache de texturas cravado em 300 MB
# (NV_TEX_MB_FIXO). Sai em build/tizen-highcache, e o tizen-wgt.sh que vier
# depois herda a pasta e o nome pelas mesmas variaveis. La as texturas moram
# no processo da GPU, fora do heap de 256 MiB do wasm — ninguem mediu ate onde
# o navegador da TV aguenta; e por isso e variante, e nao o padrao.
VARIANTE=""
# --leve: BUILD DE DIAGNOSTICO A/B (20/09/2026). Uma Samsung que rodava bem a
# 1.0.26 travou na 1.3.2 e na 1.3.3, sem log. Entre as duas versoes o pool de
# fios foi de 12 para 20 e nasceram quatro trabalhos de fundo (canal de
# avisos, recomendacoes, sync periodico, GIF de foco). Esta variante devolve
# o pool a 12 e desliga os quatro (NV_LEVE em app.c/avisos.c/home.c) para a
# pessoa comparar com a 1.0.26 e com a normal. Nao e para publicar.
POOL=20
if [ "${1:-}" = "--leve" ]; then
  VARIANTE="leve"
  POOL=12
  export NUVIO_EXTRA_CFLAGS="${NUVIO_EXTRA_CFLAGS:-} -DNV_LEVE=1"
  export NUVIO_SAIDA="${NUVIO_SAIDA:-build/tizen-leve}"
  export NUVIO_WGT_NOME="${NUVIO_WGT_NOME:-NuvioTV-native-leve}"
  echo "tizen.sh: variante LEVE (pool 12, sem trabalhos de fundo) -> $NUVIO_SAIDA"
fi
if [ "${1:-}" = "--alto-cache" ] || [ "${1:-}" = "--high-cache" ]; then
  VARIANTE="highcache"
  export NUVIO_EXTRA_CFLAGS="${NUVIO_EXTRA_CFLAGS:-} -DNV_TEX_MB_FIXO=300"
  export NUVIO_SAIDA="${NUVIO_SAIDA:-build/tizen-highcache}"
  export NUVIO_WGT_NOME="${NUVIO_WGT_NOME:-NuvioTV-native-highcache}"
  echo "tizen.sh: variante ALTO CACHE (300 MB de texturas) -> $NUVIO_SAIDA"
fi
# --tizen4: VARIANTE EXPERIMENTAL para TVs 2018 (Tizen 4.0, Chromium M56).
# O M56 nao tem WebAssembly nem SharedArrayBuffer: o build sai em wasm2js
# (-sWASM=0) e SEM pthreads. Os fios viram fibras cooperativas (src/coop.c),
# ligadas por -include src/coop_fio.h so nesta variante; o codigo dos fios nao
# muda. ASYNCIFY vai INTEIRO (sem a lista ASYNCIFY_ONLY), porque qualquer
# funcao na pilha de uma fibra pode estar no caminho de uma troca. NV_LEVE
# junto: TV de 2018 e a mais fraca que ja miramos. Pacote com id proprio
# (tools/tizen4-config.xml), para nunca substituir o app de uma TV 5.5+.
TIZEN4=""
if [ "${1:-}" = "--tizen4" ]; then
  VARIANTE="tizen4"
  TIZEN4=1
  export NUVIO_EXTRA_CFLAGS="${NUVIO_EXTRA_CFLAGS:-} -DNV_LEVE=1 -DNV_COOP=1 -include src/coop_fio.h"
  export NUVIO_SAIDA="${NUVIO_SAIDA:-build/tizen4}"
  export NUVIO_WGT_NOME="${NUVIO_WGT_NOME:-NuvioTV-native-tizen4}"
  export NUVIO_TIZEN_CONFIG="${NUVIO_TIZEN_CONFIG:-tools/tizen4-config.xml}"
  echo "tizen.sh: variante TIZEN 4 (wasm2js, fibras, sem pthreads) -> $NUVIO_SAIDA"
fi
SAIDA="${NUVIO_SAIDA:-build/tizen}"
mkdir -p "$SAIDA"

# O que muda entre o alvo normal e o Tizen 4. Ver a nota do --tizen4 acima.
if [ -n "$TIZEN4" ]; then
  ALVO_JS=chrome56
  # O esbuild REFORMATA ao rebaixar, com indentacao: num wasm2js de
  # blocos aninhados isso sozinho dobrava o arquivo. Aqui sai compacto.
  ESBUILD_EXTRA="--minify-whitespace"
  FIOS_FLAGS=""
  MOTOR_FLAGS="${NUVIO_T4_MOTOR:--sWASM=0}"   # NUVIO_T4_MOTOR=-sWASM=1: so diagnostico
  ASYNC_FLAGS="-sASYNCIFY -sASYNCIFY_STACK_SIZE=65536"
  RUNTIME_EXPORTS='["ccall"]'
  # Sem --profiling-funcs: em wasm2js ele desliga a minificacao do JS, e o
  # index.js saiu com 44,8 MB (MEDIDO, 23/09) — indentacao e nomes longos.
  # --emit-symbol-map: index.js.symbols traduz os nomes minificados de volta
  # (pilha do depurador, longtask). Fica FORA do pacote.
  PERFIL_FLAGS="--emit-symbol-map"
else
  ALVO_JS=chrome69
  ESBUILD_EXTRA=""
  FIOS_FLAGS="-pthread -sPTHREAD_POOL_SIZE=$POOL -sPTHREAD_POOL_SIZE_STRICT=0 -sDEFAULT_PTHREAD_STACK_SIZE=2097152"
  MOTOR_FLAGS="-sWASM_BIGINT=0"
  ASYNC_FLAGS="-sASYNCIFY -sASYNCIFY_STACK_SIZE=32768 $( [ -n "${NUVIO_ASYNCIFY_TUDO:-}" ] || printf -- "-sASYNCIFY_ONLY=[main,dados_iniciar]" )"
  RUNTIME_EXPORTS='["PThread","ccall"]'
  PERFIL_FLAGS="--profiling-funcs"
fi

# Os mesmos -D de servidor do Mac e da TV LG. O pacote principal precisa falhar
# antes do emcc se URL, anon key ou base de login estiverem vazios. Harnesses de
# diagnostico nao sao release: eles precisam declarar o opt-out explicitamente.
if [ "${NUVIO_TIZEN_DIAGNOSTIC:-0}" = "1" ]; then
  ENV_D=$(tools/env.sh --allow-unconfigured)
  echo "tizen.sh: diagnostico explicito — guarda de configuracao desativada"
else
  ENV_D=$(tools/env.sh --require-core)
fi
# TIZEN 4: A VERSAO DO PACOTE E A DO APP, com sufixo. O manifesto proprio
# (tools/tizen4-config.xml) tem de acompanhar o appinfo.json como o
# tizen-config.xml acompanha (tools/env.sh confere so aquele), e o app se
# identifica como "<versao>-tizen4-exp.N" nos Ajustes e em todo registro que
# manda — e o que separa relato de TV 2018 do resto. NUVIO_TIZEN4_EXP=N muda o N.
if [ -n "$TIZEN4" ]; then
  VER_APP=$(sed -n 's/^[[:space:]]*"version":[[:space:]]*"\([^"]*\)".*/\1/p' deploy/app/appinfo.json | head -1)
  VER_T4=$(grep -v "<?xml" "$NUVIO_TIZEN_CONFIG" | sed -n 's/.*[[:space:]]version="\([0-9.]*\)".*/\1/p' | head -1)
  if [ "$VER_APP" != "$VER_T4" ]; then
    echo "tizen.sh: appinfo.json diz $VER_APP e $NUVIO_TIZEN_CONFIG diz $VER_T4 -- alinhe antes de compilar" >&2
    exit 2
  fi
  T4_EXP="${NUVIO_TIZEN4_EXP:-1}"
  # O carimbo (.nuvio-build-stamp) confere a configuracao pelo que o env.sh
  # devolve, SEM o sufixo; o tizen-wgt.sh recalcula do env.sh e compara.
  ENV_D_CARIMBO="$ENV_D"
  ENV_D=$(printf '%s' "$ENV_D" | sed "s|-DNV_VERSAO=\\\\\"$VER_APP\\\\\"|-DNV_VERSAO=\\\\\"$VER_APP-tizen4-exp.$T4_EXP\\\\\"|")
  printf '%s' "$ENV_D" | grep -q "tizen4-exp.$T4_EXP" || { echo "tizen.sh: sufixo de versao do Tizen 4 nao entrou" >&2; exit 1; }
  echo "tizen.sh: versao $VER_APP-tizen4-exp.$T4_EXP"
fi

# -lidbfs.js NAO E OPCIONAL. Sem ele o objeto IDBFS simplesmente nao existe no
# JS gerado, FS.mount lanca, e a unica pista e a linha "[dados] IDBFS nao
# montou" — o app segue funcionando e esquece a sessao a cada recarga, que no
# alvo Tizen significa refazer o login por QR toda vez.
#
# PTHREADS DE VERDADE, e nao uma emulacao cooperativa.
#
# A duvida era se SharedArrayBuffer existe dentro de um .wgt — sem ele nao ha
# pthread no navegador. A Samsung DOCUMENTA o contrario: a pagina de WebAssembly
# deles manda usar exatamente `-pthread -sUSE_PTHREADS=1 -sPTHREAD_POOL_SIZE=N`
# em app de TV. Com isso os 18 arquivos que criam fio (~40 pthread_create) ficam
# INTOCADOS, e o leque paralelo de src/addons.c continua paralelo — foi ele que
# derrubou de 16,5 s a busca de fontes, consultando os addons ao mesmo tempo.
#
# POOL 12 com STRICT=0: 4 fios de fontes (ADD_FIOS) + legendas + sonda + sync +
# descoberta + artes partindo no arranque. STRICT=0 deixa criar alem do pool em
# vez de falhar; o pool so evita a pausa de criar o worker na hora.
#
# UMA PENDENCIA CONHECIDA: a Samsung tambem passa `-s ENVIRONMENT_MAY_BE_TIZEN`,
# que so existe no fork Emscripten deles. O emcc de upstream nao conhece a
# flag. Se os workers nao subirem NA TV, e aqui que se olha primeiro — e a
# saida e trocar este emsdk pelo fork da Samsung, nao mexer no codigo C.
#
# ASYNCIFY, e nao emscripten_set_main_loop: o laco de quadro em src/main.c tem
# ~140 linhas de telemetria com estado em variaveis locais, e parti-lo num
# callback trocaria uma mudanca de 6 linhas por uma reescrita. O custo e
# tamanho e um pouco de desempenho — ambos a medir na TV, nao a supor.
#
# 256 MiB RESERVADOS NO INICIO, SEM CRESCIMENTO.
# A foto da TV de 2026-09-05 mostra o heap preso em 134217728 bytes:
# Memory.grow e recusado, malloc falha e um novo pthread aborta no profiler.
# Cada thread ativa reserva 8 MiB de pilha, alem de TLS; os workers do pool
# ociosos nao equivalem a pilhas C alocadas. Arte, catalogo e imagens tambem
# competem pelo heap. tests/tizen-memory.sh reproduz pressao sem crescimento.
# 256 MiB passam no teste local; a reserva ainda precisa ser aceita na TV.
# ABORTING_MALLOC evita prosseguir com NULL: este SDK usa malloc sem checar
# em pthread_create, causando depois o enganoso erro de corrupcao no endereco 0.
# Nao reduzir pilhas sem medir uso, nem reativar crescimento baseado so no Mac.
#
# A ARTE VAI NO PACOTE, mas so um pedaco dela. deploy/app/art tem 236 MB, e
# collections/ (165 MB) e o catalogo CURADO de quem empacota — quem instalasse
# veria a colecao de outra pessoa. tools/tizen-art.sh escolhe o que pode sair
# daqui e ABORTA se credencial ou catalogo pessoal entrar no estagio.
#
# Sem isto o app abre com retangulo preto no lugar de cada icone, botao, logo,
# foto de elenco e miniatura de episodio: MEDIDO no navegador, com o log
# repetindo "[tex] decode falhou ... No such file or directory".
ARTE=$(bash tools/tizen-art.sh)

# SO png e jpg em SDL2_IMAGE_FORMATS, e NAO webp: o Emscripten nao tem port de
# libwebp, e pedir "webp" quebra o build do proprio SDL_image ("webp/decode.h
# file not found"). Os 41 selos webp sao convertidos para png por tizen-art.sh.
#
# ARTE DE REDE EM WEBP nao passa por tizen-art.sh — vem do addon em tempo de
# execucao, e um addon de posters manda webp. Quem le esse caso e src/webp.c,
# que no alvo Emscripten devolve o arquivo ao proprio navegador
# (createImageBitmap) em vez de procurar uma libwebp que nao existe em WASM.
#
# E NAO PONHA COMENTARIO DENTRO DA LISTA DE ARGUMENTOS abaixo: um `#` no meio de
# uma linha continuada por `\` encerra o comando ali. O shell entao executa o
# resto como comando solto ("-sSDL2_IMAGE_FORMATS=[...]: comando nao encontrado"),
# o emcc roda sem os argumentos seguintes — inclusive sem --preload-file — e o
# build SAI, torto, sem arte nenhuma. Perdi uma rodada inteira nisso.

# Chrome 76 tem BigInt em JS, mas NAO a passagem i64 entre JS e WASM
# (Chrome 85). clock_gettime em marco_iniciar e a primeira chamada que a usa:
# o modulo instancia e cria GL, depois falha com "wasm function signature
# contains illegal type". esbuild nao altera a ABI do .wasm; WASM_BIGINT=0
# faz o emcc converter esses parametros para pares de i32. Regressao:
# tests/tizen-clock.sh, usando um V8 anterior ao suporte dessa integracao.

# COLETOR DE LOG OPCIONAL. Com NUVIO_LOG_URL setado, o shell recebe o endereco
# e o app passa a MANDAR o log em vez de depender de foto da tela. Sem a
# variavel o placeholder fica no arquivo e o envio nem e armado — desligado por
# padrao porque log de app carrega titulo assistido e URL de fonte.
#
#   NUVIO_LOG_URL=http://192.168.1.10:8899/log bash tools/tizen.sh
SHELL_USADO="${NUVIO_TIZEN_SHELL:-tools/tizen-shell.html}"
if [ -n "${NUVIO_LOG_URL:-}" ]; then
  SHELL_USADO="$SAIDA/shell-com-log.html"
  sed "s|@NUVIO_LOG_URL@|${NUVIO_LOG_URL}|" tools/tizen-shell.html > "$SHELL_USADO"
  echo "tizen.sh: log sera enviado para $NUVIO_LOG_URL"
fi
# BUILD DE DIAGNOSTICO: NUVIO_DIAG_TOKEN (o DIAG_TOKEN do worker de
# recomendacoes) arma no shell o envio automatico do registro para
# NUVIO_REC_URL/v1/registro. Nunca na release: ver a nota em tizen-shell.html.
if [ -n "${NUVIO_DIAG_TOKEN:-}" ]; then
  PROP="${NUVIO_PROPERTIES:-$(cd "$(dirname "$0")/../.." && pwd)/NuvioWeb-0.3.38-beta/local.properties}"
  REC_URL=$(sed -n 's/^[[:space:]]*NUVIO_REC_URL[[:space:]]*=[[:space:]]*//p' "$PROP" | head -1 | tr -d '\r"')
  [ -n "$REC_URL" ] || { echo "tizen.sh: NUVIO_REC_URL ausente no local.properties" >&2; exit 1; }
  DIAG_SHELL="$SAIDA/shell-diag.html"
  sed -e "s|@NUVIO_DIAG_TOKEN@|${NUVIO_DIAG_TOKEN}|" -e "s|@NUVIO_REC_URL@|${REC_URL}|" "$SHELL_USADO" > "$DIAG_SHELL"
  SHELL_USADO="$DIAG_SHELL"
  echo "tizen.sh: BUILD DE DIAGNOSTICO — registro sobe sozinho para $REC_URL"
fi

# TIZEN 4: POLYFILLS ANTES DE QUALQUER SCRIPT. O esbuild rebaixa SINTAXE, nao
# API: o glue do Emscripten usa globalThis (M71), o shell usa padStart (M57) e
# dados.c/cachearte.c usam Atomics (M60). No M56 qualquer um deles e
# ReferenceError no primeiro uso, e a tela fica preta sem log. O arquivo
# tools/tizen4-polyfill.js entra num <script> logo apos <head>.
if [ -n "$TIZEN4" ]; then
  T4_SHELL="$SAIDA/shell-tizen4.html"
  awk -v pf=tools/tizen4-polyfill.js '
    { print }
    /<head>/ && !feito { print "<script>"; while ((getline l < pf) > 0) print l; print "</script>"; feito = 1 }
  ' "$SHELL_USADO" > "$T4_SHELL"
  grep -q 'g.globalThis = g' "$T4_SHELL" || { echo "tizen.sh: polyfill do Tizen 4 nao entrou no shell" >&2; exit 1; }
  SHELL_USADO="$T4_SHELL"
fi

EXTRA_SOURCES="${NUVIO_TIZEN_EXTRA_SOURCES:-}"
SOURCES="src/*.c"
if [ -n "${NUVIO_TIZEN_EXCLUDE_MAIN:-}" ]; then
  SOURCES=""
  for source in src/*.c; do
    [ "$source" = "src/main.c" ] || SOURCES="$SOURCES $source"
  done
fi
ASS_ROOT_REAL="${NUVIO_ASS_ROOT:-$PWD/build/ass-wasm}"
ASS_ROOT="$ASS_ROOT_REAL"
ASS_CFLAGS=""
ASS_LIBS=""
if [ "${NUVIO_ASS_LIBASS:-1}" = "1" ]; then
  if [ ! -f "$ASS_ROOT_REAL/include/ass/ass.h" ] || [ ! -f "$ASS_ROOT_REAL/lib/libass.a" ]; then
    echo "tizen.sh: libass WASM ausente em $ASS_ROOT_REAL" >&2
    echo "  rode tools/build-ass-wasm.sh apos ativar o emsdk, ou use NUVIO_ASS_LIBASS=0 apenas para diagnostico" >&2
    exit 2
  fi
  # As flags do emcc entram no eval abaixo; um caminho do checkout com espacos
  # vira dois argumentos mesmo dentro de ASS_CFLAGS. O symlink fica fora do
  # repositorio e nunca substitui um caminho preexistente que nao controlamos.
  ASS_ROOT_SHORT="${TMPDIR:-/tmp}/nuvio-ass-wasm-root-$(id -u)"
  ASS_ROOT_CANON=$(cd "$ASS_ROOT_REAL" && pwd -P)
  if [ -L "$ASS_ROOT_SHORT" ]; then
    [ "$(readlink "$ASS_ROOT_SHORT")" = "$ASS_ROOT_CANON" ] || {
      echo "tizen.sh: symlink libass inesperado em $ASS_ROOT_SHORT" >&2
      exit 2
    }
  elif [ -e "$ASS_ROOT_SHORT" ]; then
    echo "tizen.sh: caminho temporario libass ja existe e nao e symlink: $ASS_ROOT_SHORT" >&2
    exit 2
  else
    ln -s "$ASS_ROOT_CANON" "$ASS_ROOT_SHORT"
  fi
  ASS_ROOT="$ASS_ROOT_SHORT"
  ASS_CFLAGS="-DNV_ASS_LIBASS -I$ASS_ROOT/include"
  ASS_LIBS="-L$ASS_ROOT/lib -Wl,--start-group -lass -lharfbuzz -lfribidi -lfreetype -Wl,--end-group"
fi
eval emcc $SOURCES ${EXTRA_SOURCES} -o "$SAIDA/index.html" -O2 "$ENV_D" ${NUVIO_EXTRA_CFLAGS:-} $ASS_CFLAGS $ASS_LIBS \
  $MOTOR_FLAGS \
  -sUSE_SDL=2 -sUSE_SDL_IMAGE=2 -sUSE_SDL_TTF=2 -sUSE_LIBJPEG=1 \
  `# zlib do emscripten: epg.c infla o XMLTV .gz do epgshare01 com inflate.` \
  -sUSE_ZLIB=1 \
  -sSDL2_IMAGE_FORMATS='["png","jpg"]' \
  -sMAX_WEBGL_VERSION=1 \
  -sINITIAL_MEMORY=268435456 -sALLOW_MEMORY_GROWTH=0 -sABORTING_MALLOC=1 \
  `# PILHAS DE 8 MB, e nao o padrao de 64 KB do emscripten. Esta build estava` \
  `# SEM as duas linhas, sozinha entre as builds do projeto: a bancada de teste` \
  `# do AVPlay ja usava 8 MB nos dois. Pilha de fio pequena ja custou caro aqui` \
  `# uma vez — foi ela, e nao o ALLOW_MEMORY_GROWTH que eu acusei na epoca, que` \
  `# causava um "memory access out of bounds". Estouro de pilha nao avisa: com` \
  `# ASSERTIONS=0 o app morre calado, que e o sintoma na TV.` \
  `# PILHA DO FIO PRINCIPAL 8 MB, DOS WORKERS 2 MB — e a diferenca importa.` \
  `# MEDIDO NA TV: malloc=60,3 MiB e livre-no-heap=85,5 MiB, ou seja 146 MiB` \
  `# contabilizados, e mesmo assim o app abortou sem achar 28 MiB num heap de` \
  `# 256. Os 110 MiB que faltavam na conta sao PILHA: 8 MB x 12 workers do pool` \
  `# mais o fio principal = 104 MB reservados que nenhum contador mostra.` \
  `#` \
  `# Os 8 MB vieram de um estouro de pilha real deste projeto, mas aquele era no` \
  `# FIO PRINCIPAL, que segue com 8. Worker daqui faz HTTP e decodifica imagem —` \
  `# 2 MB e o proprio padrao do emscripten e sobra. Libera ~72 MB, que e mais do` \
  `# que o app pedia quando morreu.` \
  -sSTACK_SIZE=8388608 \
  `# 32 KB de pilha do asyncify, o mesmo valor da bancada que roda. O laco de` \
  `# quadro desenrola por aqui a cada SwapWindow; 16 KB era aperto sem motivo.` \
  `# --profiling-funcs: so a secao de NOMES das funcoes no wasm (~5% do` \
  `# tamanho), sem custo de execucao. Sem ela o profiler (CDP no Mac ou o Web` \
  `# Inspector da TV) mostra wasm-function[729] e nao diz o que e.` \
  $PERFIL_FLAGS \
  $ASYNC_FLAGS \
  `# SO main E dados_iniciar SAO INSTRUMENTADOS. Sem esta lista o ASYNCIFY` \
  `# instrumenta toda funcao que possa estar na pilha de uma chamada assincrona` \
  `# — e como nv_ceder_quadro e chamada do laco em main, isso era o app` \
  `# INTEIRO: cada funcao de desenho, decode e rede ganhava o codigo de` \
  `# desenrolar/reenrolar pilha (custo em toda chamada, e wasm maior). As duas` \
  `# unicas chamadas assincronas (nv_idbfs_montar em dados_iniciar e` \
  `# nv_ceder_quadro em main) so tem main e dados_iniciar acima delas. Se` \
  `# alguem puser outra EM_ASYNC_JS mais fundo, a TV aborta com "unreachable"` \
  `# no desenrolar — e a lista aqui que precisa crescer. NUVIO_ASYNCIFY_TUDO=1` \
  `# volta ao comportamento antigo para comparar.` \
  `# POOL DE 12. Ja esteve em 4, por um palpite meu que a evidencia derrubou:` \
  `# cortei supondo que o arranque estava LENTO por causa dos doze workers, e os` \
  `# marcos de tempo mostraram depois que ele estava CONGELADO, no WASM_BIGINT.` \
  `# A razao do corte evaporou e o corte tem custo real: o app tem 41 pontos de` \
  `# pthread_create, e quando o pool esgota cada fio novo exige criar um Worker` \
  `# e instanciar de novo os 3,2 MB de wasm. sessao_login_comecar cria fio na` \
  `# tela de login — que e onde a TV travou com o pool em 4.` \
  `# 12 NAO BASTOU, e agora ha medida de campo e nao suposicao: o log de uma TV` \
  `# Samsung em uso mostrou "[fios] livres=0 vivos=12  <<< POOL NO LIMITE"` \
  `# repetido, com o aviso "Blocking on the main thread is very dangerous" do` \
  `# Emscripten logo antes — que e o quadro descrito acima, o de pool seco.` \
  `# A CONTA explica: os orcamentos de fios simultaneos somam mais de 20 quando` \
  `# as fases se sobrepoem (ADD_FIOS 4, BUSCA_FIOS 3, CAT_FIOS 3, VER_FIOS 4,` \
  `# NV_TEX_FIOS 2 + NV_TEX_FIOS_REDE 2 (4 no LG), TK_FIOS 3, mais os avulsos de sync,` \
  `# video e extras). Com 12 o pool seca em qualquer arranque com conta.` \
  `# 20 cobre o pico observado com folga. CUSTO MEDIDO no navegador, e ele NAO` \
  `# e o que eu supus: com 12 e com 20 o heap fica igual (malloc=18,9 MiB,` \
  `# livre-no-heap=7,9 MiB). Worker OCIOSO nao aloca pilha — os 2 MiB de` \
  `# DEFAULT_PTHREAD_STACK_SIZE so saem no pthread_create. O que os 8 workers a` \
  `# mais custam e memoria do NAVEGADOR (cada um instancia os ~3,2 MB de wasm),` \
  `# e e exatamente esse trabalho que sai do caminho critico: com o pool seco` \
  `# ele acontecia no MEIO da sessao e no FIO PRINCIPAL.` \
  $FIOS_FLAGS \
  -sEXPORTED_FUNCTIONS='["_main","_malloc","_free"]' \
  `# PThread exportado para o medidor de fios de tizen-shell.html. NAO e` \
  `# opcional: sem o export, LER a variavel dispara o abort() do runtime` \
  `# ("'PThread' was not exported"), ou seja, o proprio medidor mataria o app.` \
  -sEXPORTED_RUNTIME_METHODS='$RUNTIME_EXPORTS' \
  -lidbfs.js \
  `# ASSERTIONS=0 NA BUILD DE ENTREGA (20/09/2026, #72). Com 1 o glue confere` \
  `# pilha e assinatura a cada chamada JS<->wasm e cada erro de FS monta um` \
  `# ErrnoError com pilha. O que ele dava — morrer falando em vez de calado —` \
  `# hoje o registro em localStorage (tizen-shell.html) da. NUVIO_ASSERTS=1 liga.` \
  -sEXIT_RUNTIME=0 -sASSERTIONS="${NUVIO_ASSERTS:-0}" \
  --preload-file deploy/app/fonts@/app/fonts \
  --preload-file "$ARTE"@/app/art \
  --shell-file "$SHELL_USADO"
# O WORKER DE DECODE (#72) e um arquivo a parte, carregado por index.html como
# `decodificador.js`; sem ele o app roda, mas cada arte custa fio principal.
# No Tizen 4 nao ha Worker de decode: ele depende de SharedArrayBuffer,
# Atomics e OffscreenCanvas (M60+). A arte decodifica no fio principal
# (src/webp.c, ramo NV_COOP) e o pacote nao leva o arquivo.
if [ -n "$TIZEN4" ]; then
  rm -f "$SAIDA/decodificador.js"
else
  cp tools/decodificador.js "$SAIDA/decodificador.js"
fi

# REBAIXAR O GLUE PARA CHROMIUM 76 — sem isto o app NAO ARRANCA na TV.
#
# MEDIDO NO APARELHO, nao suposto. O userAgent da TV Samsung diz
# "SMART-TV; LINUX; Tizen 6.0 ... 76.0.3809.146". O emcc emite JS moderno e o
# glue morre na PRIMEIRA LINHA com "Uncaught SyntaxError: Unexpected token" —
# antes de uma linha sequer do nosso codigo. Na tela isso e PRETO depois de
# 100%, sem nenhuma outra pista; foi exatamente assim que apareceu.
#
# Contagem no index.js gerado: 45 optional chaining (?.), 36 nullish (??) e
# 11 ??= mais 18 ||= — todos Chrome 80/85, todos numa unica linha minificada de
# 278 KB, o que descarta remendo a mao.
#
# POR QUE NAO -sMIN_CHROME_VERSION=76, que seria o caminho obvio: este emsdk
# recusa com "MIN_CHROME_VERSION older than 85 is not supported". A alternativa
# seria instalar um emsdk antigo; transpilar a saida custa menos e nao prende o
# projeto a uma versao de SDK obsoleta.
#
# Os workers de pthread nascem DESTE MESMO arquivo, entao rebaixa-lo cobre o
# fio principal e os workers de uma vez.
# CHROME69, E NAO CHROME76. O config.xml declara required_version 5.5, e a
# tabela oficial de motores da Samsung diz que Tizen 5.5 (todos os modelos 2020)
# e Chromium M69 — 6.0 que e M76. Rebaixar para 76 deixava o pacote incompativel
# com o minimo que ele proprio anuncia.
#
# MEDIDO em 18/09, comparando as duas saidas do esbuild sobre o MESMO glue: a de
# chrome69 sai 600 B maior, e o diff mostra o que sobrevivia ao 76 — CLASS
# FIELDS, que chegaram no Chrome 72:
#
#   class ExitStatus { name = "ExitStatus" }      <- M72+
#   class FS         { shared = {} }
#
# Eles vem do glue gerado pelo PROPRIO Emscripten (ExitStatus, ErrnoError, as
# classes do FS), entao estavam em toda build. Numa TV 2020 isso nao degrada:
# e erro de sintaxe no parse, o script inteiro morre e a tela fica PRETA — o
# mesmo sintoma que este rebaixamento existe para evitar no caso do "?.".
#
# A CONFERENCIA ABAIXO TAMBEM ESTAVA CURTA: ela contava so ?. ?? e ||=, que sao
# sintaxe pos-M76, e por isso dizia "0" enquanto os class fields passavam. Agora
# procura tambem campo de classe.
# NUVIO_SEM_REBAIXAR=1: SO DIAGNOSTICO, pula o esbuild (o glue sai como o
# emcc deixou, que so roda em Chrome moderno). Serve para separar defeito do
# rebaixamento de defeito do app.
if [ "${NUVIO_SEM_REBAIXAR:-0}" = "1" ]; then
  echo "tizen.sh: AVISO — NUVIO_SEM_REBAIXAR=1, glue NAO rebaixado (so diagnostico)" >&2
elif command -v npx >/dev/null 2>&1; then
  npx --yes esbuild@0.25.0 "$SAIDA/index.js" --target=$ALVO_JS $ESBUILD_EXTRA \
      --outfile="$SAIDA/index.rebaixado.js" --log-level=warning
  mv "$SAIDA/index.rebaixado.js" "$SAIDA/index.js"
  RESTO=$(grep -oE '\?\.([^0-9]|$)|\?\?|\|\|=' "$SAIDA/index.js" | wc -l | tr -d ' ')
  # CONFERENCIA POR IDEMPOTENCIA, e nao por grep.
  #
  # Contar construcoes a mao nao serve: a primeira versao desta linha procurava
  # so "?.", "??" e "||=" e dizia "0" com CENTENAS de class fields intactos no
  # arquivo — sintaxe M72 passando por um alvo que se dizia M69. E um grep de
  # class field casa atribuicao comum e acusa 417 num arquivo limpo.
  #
  # Rebaixar de novo o que ja foi rebaixado nao pode mudar NADA. Se mudar,
  # sobrou algo que o alvo transforma, e a TV 2020 daria tela preta no parse.
  npx --yes esbuild@0.25.0 "$SAIDA/index.js" --target=$ALVO_JS $ESBUILD_EXTRA \
      --outfile="$SAIDA/index.conferencia.js" --log-level=error
  if cmp -s "$SAIDA/index.conferencia.js" "$SAIDA/index.js"; then
    rm -f "$SAIDA/index.conferencia.js"
    echo "tizen.sh: glue rebaixado para $ALVO_JS (sintaxe pos-M76: $RESTO, idempotente)"
  else
    rm -f "$SAIDA/index.conferencia.js"
    echo "tizen.sh: ERRO — o glue ainda muda ao ser rebaixado de novo." >&2
    echo "  Sobrou sintaxe que o Chromium M69 (Tizen 5.5) nao entende." >&2
    exit 1
  fi
else
  echo "tizen.sh: AVISO — npx ausente, glue NAO rebaixado; a TV vai dar tela preta" >&2
fi

# globalThis NA TV 2020. O esbuild rebaixa SINTAXE, nao poe API que falta, e o
# glue do Emscripten le globalThis ja na linha 6 (ENVIRONMENT_IS_WEB), antes de
# qualquer codigo nosso. globalThis e Chrome 71; Tizen 5.5 e M69.
#
# MEDIDO em 23/09 com Node 10 (V8 6.8, sem globalThis), tests/tizen-globalthis.sh:
# o index.js morre com "ReferenceError: globalThis is not defined" na linha 6,
# tanto como pagina quanto como pthread. Na TV isso NAO foi medido.
#
# POR QUE PREPEND AQUI e nao --pre-js nem --banner do esbuild:
#   * o emcc poe o pre-js DEPOIS do bloco ENVIRONMENT_IS_*, tarde demais;
#   * o banner seria reimpresso pelo esbuild da conferencia acima e o cmp de
#     idempotencia deixaria de bater.
# Os workers de pthread fazem new Worker(<este index.js>), entao a primeira
# linha deste arquivo vale para a pagina e para cada worker.
cat tools/tizen-globalthis.js "$SAIDA/index.js" > "$SAIDA/index.polyfill.js"
mv "$SAIDA/index.polyfill.js" "$SAIDA/index.js"
# Conferencia: com globalThis no corpo, a linha 1 TEM de ser o polyfill, e ele
# so uma vez. A mesma guarda roda no tools/tizen-wgt.sh, contra build velho.
GT_USOS=$(tail -n +2 "$SAIDA/index.js" | grep -o 'globalThis' | wc -l | tr -d ' ')
GT_SENT=$(grep -c 'nuvio:globalThis' "$SAIDA/index.js" || true)
if [ "$GT_USOS" -gt 0 ] && { [ "$GT_SENT" -ne 1 ] || ! head -n 1 "$SAIDA/index.js" | grep -q 'nuvio:globalThis'; }; then
  echo "tizen.sh: ERRO — index.js usa globalThis $GT_USOS vezes sem o polyfill na linha 1" >&2
  echo "  (sentinela nuvio:globalThis encontrada $GT_SENT vezes). Tizen 5.5/M69 nao tem globalThis." >&2
  exit 1
fi
echo "tizen.sh: globalThis: polyfill na linha 1 ($GT_USOS usos no glue)"

# Proveniencia minima do artefato. O empacotador pode ser chamado horas depois
# de uma compilacao, e um build/index.wasm velho com um local.properties valido
# passaria apenas pela guarda de ambiente. Guardamos somente fingerprints: nem
# a configuracao nem seus valores entram no arquivo de estagio.
sha256() {
  if command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$@" | awk '{print $1}'
  else
    sha256sum "$@" | awk '{print $1}'
  fi
}
fingerprint() {
  if command -v shasum >/dev/null 2>&1; then
    shasum -a 256 | awk '{print $1}'
  else
    sha256sum | awk '{print $1}'
  fi
}
CONFIG_FP=$(printf '%s' "${ENV_D_CARIMBO:-$ENV_D}" | fingerprint)
# O MOTOR e o arquivo com o codigo do app: index.wasm, ou index.js no Tizen 4
# (wasm2js poe o app inteiro no JS). O carimbo diz qual e, e o tizen-wgt.sh
# confere o pacote por ele.
if [ -n "$TIZEN4" ]; then MOTOR=wasm2js; MOTOR_ARQ="$SAIDA/index.js"
else MOTOR=wasm; MOTOR_ARQ="$SAIDA/index.wasm"; fi
WASM_SHA=$(sha256 "$MOTOR_ARQ")
{
  printf 'format=1\n'
  printf 'config-fingerprint=%s\n' "$CONFIG_FP"
  printf 'motor=%s\n' "$MOTOR"
  printf 'wasm-sha256=%s\n' "$WASM_SHA"
} > "$SAIDA/.nuvio-build-stamp"
chmod 600 "$SAIDA/.nuvio-build-stamp"

echo "tizen.sh: $SAIDA/index.html  ($(du -h "$MOTOR_ARQ" | cut -f1) de $MOTOR)"
