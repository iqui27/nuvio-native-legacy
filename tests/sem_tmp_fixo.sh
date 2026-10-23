#!/bin/bash
# GUARDA: nenhum /tmp fixo host-side novo em tests/ e tools/ (#205).
# /tmp do HOST (build, scratch) deve vir de $TMPDIR com fallback:
#   shell: ${TMPDIR:-/tmp}/...   |   C: nvt()/nvt_dir() de tests/nvtmp.h.
# Fora do alcance (nao falha):
#   - comentarios (linhas iniciando em # ou //);
#   - paths do lado do dispositivo/container/TV (adb/ssh/scp/sdb/docker,
#     TV_IP, root@..., tools/arm.sh e tools/tpk* rodando DENTRO da imagem).
set -u
cd "$(dirname "$0")/.."
falhas=0
is_device() {
  case "$1" in tools/arm.sh|tools/tpk.sh|tools/tpk/x64.sh|tools/tizen-uep-spike.sh|tools/tizen-memfd-spike.sh|tests/mkvass_grande.sh|tests/tpk40_tls.sh|tools/tpk-testa.sh|tools/tpk/hostfalso.c|tools/auditoria-release.sh) return 0;; esac
  return 1
}
while IFS= read -r f; do
  case "$f" in tests/nvtmp.h|tests/sem_tmp_fixo.sh) continue;; esac
  is_device "$f" && continue
  case "$f" in tools/Dockerfile|*.py|*.md) continue;; esac
  n=0
  while IFS= read -r linha; do
    n=$((n+1))
    case "$linha" in \#*|//*) continue;; esac
    case "$linha" in *'//'*) sem_com=$(printf '%s' "$linha" | sed -E "s#//.*##"); printf '%s' "$sem_com" | grep -q '/tmp' || continue;; esac
    # tira o que ja honra TMPDIR antes de procurar sobra.
    sem_tmpdir=$(printf '%s' "$linha" | sed -E 's/\$\{TMPDIR[^}]*\}//g; s/getenv\("TMPDIR"\)/NVT/g')
    case "$sem_tmpdir" in
      *"/tmp"*) ;;
      *) continue;;
    esac
    # pega cada /tmp e olha o caractere anterior.
    printf '%s\n' "$sem_tmpdir" | grep -oE '(^|[^A-Za-z0-9_/.-])/tmp([^A-Za-z0-9_-]|$)' >/dev/null || continue
    echo "FALHA $f:$n: /tmp fixo host-side: $linha" >&2
    falhas=$((falhas+1))
  done < "$f"
done < <(git ls-files 'tests/*.sh' 'tests/*.c' 'tests/*.h' 'tools/*.sh' 'tools/*.c' 'tools/*/*.c' 'tools/*/*.sh' 'tests/*.py' 'tools/*.py')
if [ "$falhas" -gt 0 ]; then echo "sem_tmp_fixo: $falhas linha(s) com /tmp fixo host-side" >&2; exit 1; fi
echo "ok: nenhum /tmp fixo host-side novo em tests/ e tools/"
