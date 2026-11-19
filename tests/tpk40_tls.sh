#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# A libnuvio.so do Tizen 4/5 NAO PODE TER TLS DE COMPILADOR (#137, #180).
#
# O host NuvioTpk40 pode carrega-la por um carregador de ELF proprio
# (tizen-tpk/NuvioTpk40/Program40.cs) que nao monta TLS: um _Thread_local vira
# PT_TLS + R_ARM_TLS_DTPMOD32 + __tls_get_addr, e o primeiro acesso — a
# primeira requisicao HTTPS, no fio do login — derrubava o processo na TV
# 2018-2020 sem mensagem. tools/tpk.sh compila essa .so a parte (-DNV_TPK40)
# e ja falha se ela sair com TLS; este teste e a prova independente, com o
# readelf da mesma imagem, e o contraste com a .so comum, que CONTINUA com TLS
# (a dos hosts Tizen 6+ nao muda: la o glibc carrega e o TLS funciona).
#
#   bash tools/tpk.sh          # gera build/tpk/libnuvio.so e libnuvio-tpk40.so
#   bash tests/tpk40_tls.sh
#
# Precisa do Docker e das duas .so: sem eles, avisa e sai 0 (nao e porteiro
# da suite leve; e a conferencia de release do .tpk 4/5).
set -eu
cd "$(dirname "$0")/.." && source tools/ctr.sh
source tools/tpk/sdk.sh
COMUM=build/tpk/libnuvio.so
TPK40=build/tpk/libnuvio-tpk40.so
if ! ctr_check_system 2>/dev/null || ! ctr_image_exists "$TPK_IMAGE" 2>/dev/null; then
  echo "pulado: sem Apple Container ou sem a imagem nuvio-tpk-sdk"; exit 0; fi
if [ ! -f "$COMUM" ] || [ ! -f "$TPK40" ]; then
  echo "pulado: rode bash tools/tpk.sh antes ($COMUM / $TPK40)"; exit 0; fi

# Imprime, para cada .so: PT_TLS? relocs de TLS? __tls_get_addr? DT_HASH?
ctr_run --rm --platform "$TPK_PLATFORM" -v "$PWD/build/tpk":/t "$TPK_IMAGE" sh -c '
  for f in libnuvio.so libnuvio-tpk40.so; do
    L=/t/$f
    tls=$(readelf -lW "$L" | grep -cE "^\s*TLS\s" || true)
    rtls=$(readelf -rW "$L" | grep -cE "R_ARM_TLS_" || true)
    tga=$(readelf -sW --dyn-syms "$L" | grep -c "__tls_get_addr" || true)
    hash=$(readelf -dW "$L" | grep -cE "\(HASH\)" || true)
    ghash=$(readelf -dW "$L" | grep -cE "\(GNU_HASH\)" || true)
    echo "$f pt_tls=$tls relocs_tls=$rtls tls_get_addr=$tga dt_hash=$hash gnu_hash=$ghash"
  done' > "${TMPDIR:-/tmp}/nv-tpk40-tls.txt"
cat "${TMPDIR:-/tmp}/nv-tpk40-tls.txt"

falhou=0
# A .so do 4/5: nada de TLS, e DT_HASH presente.
grep -q "^libnuvio-tpk40.so pt_tls=0 relocs_tls=0 tls_get_addr=0 dt_hash=1 " "${TMPDIR:-/tmp}/nv-tpk40-tls.txt" ||
  { echo "FALHA: libnuvio-tpk40.so tem TLS ou nao tem DT_HASH"; falhou=1; }
# A .so comum: continua como sempre (com TLS de rede.c). Se isto mudar, ou
# alguem tirou o _Thread_local do caminho comum (nao era para) ou a macro
# NV_TPK40 vazou para a build comum.
grep -qE "^libnuvio.so pt_tls=1 relocs_tls=[1-9]" "${TMPDIR:-/tmp}/nv-tpk40-tls.txt" ||
  { echo "FALHA: libnuvio.so comum deveria continuar com PT_TLS (rede.c); a NV_TPK40 vazou?"; falhou=1; }
# As duas exportam a mesma interface nv_tpk_*.
ctr_run --rm --platform "$TPK_PLATFORM" -v "$PWD/build/tpk":/t "$TPK_IMAGE" sh -c '
  objdump -T /t/libnuvio.so | grep -oE " nv_tpk[a-z0-9_]*" | sort > /tmp/a
  objdump -T /t/libnuvio-tpk40.so | grep -oE " nv_tpk[a-z0-9_]*" | sort > /tmp/b
  diff /tmp/a /tmp/b' || { echo "FALHA: as duas .so exportam nv_tpk_* diferentes"; falhou=1; }
[ $falhou -eq 0 ] && echo "ok: libnuvio-tpk40.so sem TLS e com DT_HASH; libnuvio.so comum inalterada"
exit $falhou
