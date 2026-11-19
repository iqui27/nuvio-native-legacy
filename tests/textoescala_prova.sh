#!/bin/bash
# PROOF for "text and component scale" (Ajustes > Escala de texto e componentes).
# Needs a GL window (like the *_shot.sh tools), so it is not in the default suite.
#
#   bash tests/textoescala_prova.sh            # compares this tree with $NV_BASE
#   NV_BASE=<ref> bash tests/textoescala_prova.sh
#   NV_ONLY_NEW=1 ...                          # skip the baseline comparison
#   NV_REUSE=<previous kept W> ...             # reuse binaries after a link-only fix
#   NV_PNG_PICK=<regex> ...                    # select PNG frame names
#
# For each screen (Home, Library, Settings, Title page, Player pause panel) the
# real shot program runs three times and a spy on gfx_rect logs what it draws
# in real pixels:
#   1. explicit 100%            -> baseline BMPs, must equal the BASE tree's BMPs
#   2. escalaTextoLocal = 100%  -> metrics baseline
#   3. escalaTextoLocal = 120%  -> text and button heights x1.2, poster rects
#                                  unchanged
# Run against a tree without the feature, step 3 draws the same sizes as step 2
# and the check FAILS.
# Usage on an old tree: NV_SRC=<dir with src/ and tests/> bash tests/textoescala_prova.sh
set -u
cd "$(dirname "$0")/.."
export TMPDIR="${NV_TMP:-/tmp}"; TMPDIR="${TMPDIR%/}"
SRC="${NV_SRC:-.}"
BASE_REF="${NV_BASE:-f22761e28}"   # fork point of agente/205-textoescala
W="${NV_REUSE:-$(mktemp -d "$TMPDIR/nv-textoescala.XXXXXX")}"
[ -n "${NV_KEEP:-}" ] && echo "keeping $W" || trap 'rm -rf "$W"' EXIT
TESTS="$PWD/tests"
CFLAGS=(-O1 -w -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -DNV_TRAKT_CLIENT_ID='"chave-de-teste"')
LIBS=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)

# objs <srcroot> <objdir> [extra cc flags]: every source, with the spy renames
objs() {
  local root="$1" o="$2" f n=0
  shift 2
  mkdir -p "$o"
  for f in $(ls "$root"/src/*.c "$root"/src/dts/*.c | grep -v '/main\.c$'); do
    flags=()
    case "$f" in */gfx.c) flags=(-Dgfx_rect=nvreal_gfx_rect);; */ajustes.c) flags=(-Dajustes_iniciar=nvreal_ajustes_iniciar -Dajustes_dir=nvreal_ajustes_dir);; esac
    cc "${CFLAGS[@]}" "$@" ${flags[@]+"${flags[@]}"} -I"$root/src" -c "$f" -o "$o/$(echo "${f#./}" | tr / _).o" || touch "$o/erro" &
    n=$((n + 1)); [ $((n % JOBS)) -eq 0 ] && wait
  done
  wait
  [ ! -e "$o/erro" ]
}

SHOTS="${NV_SHOTS:-homelayouts biblioteca ajustes detail_secoes player_pausa busca vertudo teclado}"
JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 8)

# build <srcroot> <outdir>: one shot binary per selected screen
build() {
  local root="$1" out="$2" s
  if [ -z "${NV_LINK_ONLY:-}" ]; then
    objs "$root" "$out/o" || return 1
    case " $SHOTS " in *" ajustes "*|*" teclado "*) objs "$root" "$out/oa" -DAJUSTES_TESTE || return 1;; esac
  fi
  cc "${CFLAGS[@]}" -I"$root/src" -c "$TESTS/textoescala_espiao.c" -o "$out/espiao.o" || return 1
  for s in $SHOTS; do
    set -- "$out"/o/*.o; ex=()
    if [ "$s" = detail_secoes ]; then set -- $(ls "$out"/o/*.o | grep -v 'src_detail\.c\.o'); fi
    if [ "$s" = vertudo ]; then set -- $(ls "$out"/o/*.o | grep -v 'src_vertudo\.c\.o'); fi
    if [ "$s" = ajustes ] || [ "$s" = teclado ]; then set -- "$out"/oa/*.o; ex=(-DAJUSTES_TESTE); fi
    if [ "$s" = teclado ]; then set -- "$@" "$TESTS/textoescala_relogio.c"; fi
    shot="$TESTS/${s}_shot.c"
    if [ "$s" = detail_secoes ] || [ "$s" = vertudo ]; then   # shots include the production source
      mkdir -p "$out/t"; ln -sfn "$(cd "$root" && pwd)/src" "$out/src"
      cp "$shot" "$out/t/"; shot="$out/t/${s}_shot.c"
    fi
    cc "${CFLAGS[@]}" -DglClear=nvspy_glClear -DglReadPixels=nvspy_glReadPixels ${ex[@]+"${ex[@]}"} -I"$root/src" -I"$TESTS" "$shot" "$@" "$out/espiao.o" "${LIBS[@]}" -o "$out/$s" || return 1
  done
}

if [ -n "${NV_LINK_ONLY:-}" ]; then
  build "$SRC" "$NV_LINK_ONLY"
  exit $?
fi

# run <tree> <shot> <idx|-> <tag>: runs the shot, leaves $W/<tree>/<tag>-<shot>.{log,bmp.sum}
run() {
  local t="$1" s="$2" idx="$3" tag="$4" d dados saida
  saida="$W/$t/$tag-$s"; mkdir -p "$saida"
  case "$s" in
    homelayouts) dados="$(mktemp -d "$TMPDIR/nuvio-homelayouts-shot.XXXXXX")"; args=("$saida/h" 1);;
    biblioteca)  dados="$(mktemp -d "$TMPDIR/nuvio-bibshot-XXXXXX")/dados"; mkdir -p "$dados"; args=("$saida/b");;
    detail_secoes) dados="$(mktemp -d "$TMPDIR/nuvio-detsec-dados.XXXXXX")"; args=("$saida/d");;
    *) dados="$(mktemp -d "$TMPDIR/nv-te-dados.XXXXXX")"; args=("$saida/x");;
  esac
  ( export NV_TXT_IDX="${idx/-/2}"
    export NV_TXT_LOG="$W/$t/$tag-$s.log" NUVIO_DADOS="$dados" NUVIO_TESTE_DIR="$dados" NUVIO_AJUSTES_UX=1
    export NV_SHOT_PACOTE="${NV_SHOT_PACOTE:-$PWD/deploy/app/art/03.jpg}"
    "$W/$t/$s" "${args[@]}" > "$W/$t/$tag-$s.out" 2>&1 ) || { echo "  run failed: $t $tag $s"; tail -5 "$W/$t/$tag-$s.out"; rm -rf "$dados"; return 1; }
  rm -rf "$dados"
  ( cd "$saida" && find . \( -name '*.bmp' -o -name '*.png' \) | sort | xargs shasum 2>/dev/null ) > "$W/$t/$tag-$s.sum"
  if [ -n "${NV_PNG_OUT:-}" ] && [ "$t" = new ] && case "$tag" in p85|p100|p130) true;; *) false;; esac; then   # keep a few frames per run as PNG
    find "$saida" \( -name '*.png' -o -name '*.bmp' \) | sort | while read -r f; do
      [[ "$(basename "$f")" =~ ${NV_PNG_PICK:-.*} ]] || continue
      sips -s format png "$f" --out "$NV_PNG_OUT/$s-$tag-$(basename "${f%.*}").png" >/dev/null 2>&1
    done
  fi
  rm -rf "$saida"   # the disk is shared; only the hashes are kept
}

rc=0
if [ -z "${NV_REUSE:-}" ]; then
  echo "build: this tree"; mkdir -p "$W/new"; build "$SRC" "$W/new" || { echo "FAIL: build"; exit 1; }
fi
if [ -z "${NV_ONLY_NEW:-}" ]; then
  echo "build: baseline $BASE_REF"; mkdir -p "$W/old/src"; git archive "$BASE_REF" src | tar -x -C "$W/old"
  build "$W/old" "$W/base" || { echo "FAIL: baseline build"; exit 1; }
fi
for s in $SHOTS; do
  echo "== $s"
  run new "$s" - def || rc=1
  run new "$s" 2 p100 || rc=1
  run new "$s" 4 p120 || rc=1
  if [ -n "${NV_PNG_OUT:-}" ]; then run new "$s" 0 p85 || rc=1; run new "$s" 5 p130 || rc=1; fi   # screenshots for review
  python3 "$TESTS/textoescala_confere.py" "$s" "$W/new/p100-$s.log" "$W/new/p120-$s.log" 1.2 || rc=1
  if [ -z "${NV_ONLY_NEW:-}" ]; then
    # Unstable frames are inconclusive and fail; never silently skip them.
    mkdir -p "$W/base"; run base "$s" - def || rc=1
    for n in def2 def3; do run base "$s" - $n || rc=1; run new "$s" - $n || rc=1; done
    cat "$W/base"/def*-"$s".sum > "$W/base/all-$s.sum"; cat "$W/new"/def*-"$s".sum > "$W/new/all-$s.sum"
    python3 "$TESTS/textoescala_hashes.py" "$W/base/all-$s.sum" "$W/new/all-$s.sum" || rc=1
  fi
done
[ $rc -eq 0 ] && echo "PASS textoescala" || echo "FAIL textoescala"
exit $rc
