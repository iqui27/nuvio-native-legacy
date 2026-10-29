#!/bin/bash
# Existing per-screen TV shot harnesses, base/current, sharing one object build.
set -euo pipefail
cd "$(dirname "$0")/.."
root=$PWD
proof=/Volumes/ExternalSSD/tmp/rev/ios-m3/tv
base=${NUVIO_M3_BASE:-00fd00ba5}
mkdir -p "$proof" "$proof/objects" "$proof/base-src"
flags=(-O1 -g -DAJUSTES_TESTE -DNV_SHOT_HOOKS -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -Wno-macro-redefined)
libs=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
objects=()
for source in src/*.c src/dts/*.c; do
  [ "$source" = src/main.c ] && continue
  object="$proof/objects/$(basename "$source" .c).o"
  if [ ! -f "$object" ] || [ "$source" -nt "$object" ]; then
    ccache cc "${flags[@]}" -c "$source" -o "$object"
  fi
  objects+=("$object")
done
# Only changed translation units differ. Headers/inc files come from base too.
for source in $(git diff --name-only "$base" -- src); do
  mkdir -p "$proof/base-src/$(dirname "$source")"
  git show "$base:$source" > "$proof/base-src/$source"
done
for version in base after; do
  dest="$proof/$version"; mkdir -p "$dest"
  selected=()
  for object in "${objects[@]}"; do
    name=$(basename "$object" .o)
    if [ "$version" = base ] && [ -f "$proof/base-src/src/$name.c" ]; then
      before="$proof/objects/$name-base.o"
      cc "${flags[@]}" -I"$root/src" -I"$proof/base-src/src" -c "$proof/base-src/src/$name.c" -o "$before"
      selected+=("$before")
    else selected+=("$object"); fi
  done
  screens='home settings search sources detail library'
  [ "${1:-}" != --library-only ] || screens=library
  for screen in $screens; do
    case "$screen" in
      home) harness=tests/home21_shot.c;;
      settings) harness=tests/ajustes_shot.c;;
      search) harness=tests/busca_shot.c;;
      sources) harness=tests/fontes_cabecalho_shot.c;;
      detail) harness=tests/detalhe_glass_shot.c;;
      library) harness=tests/biblioteca_shot.c;;
    esac
    link=()
    for object in "${selected[@]}"; do
      if [ "$screen" = detail ] && [[ "$(basename "$object")" = detail*.o ]]; then continue; fi
      link+=("$object")
    done
    cc "${flags[@]}" "$harness" "${link[@]}" "${libs[@]}" -o "$dest/$screen-shot"
    prefix=nuvio-home21-shot-m3
    [ "$screen" != detail ] || prefix=nuvio-detglass-dados
    data=$(mktemp -d "${TMPDIR:-/tmp}/$prefix.XXXXXX")
    mkdir -p "$dest/$screen"
    set +e
    case "$screen" in
      settings) NUVIO_DADOS="$data" NUVIO_AJ_QUADROS='tri-indice tri-secao tri-editor' "$dest/$screen-shot" "$dest/$screen/app" > "$dest/$screen/log.txt" 2>&1;;
      detail) NUVIO_DADOS="$data" "$dest/$screen-shot" "$dest/$screen" > "$dest/$screen/log.txt" 2>&1;;
      *) NUVIO_DADOS="$data" "$dest/$screen-shot" "$dest/$screen/app" > "$dest/$screen/log.txt" 2>&1;;
    esac
    rc=$?
    set -e
    printf '%s\n' "$rc" > "$dest/$screen/rc"
    rm -rf "$data"
    printf '%s %s rc=%d\n' "$version" "$screen" "$rc"
  done
done
python3 - "$proof" <<'PY'
from pathlib import Path
import sys,json,subprocess,tempfile,struct

def pixels(path):
    if path.suffix.lower()=='.bmp': raw=path.read_bytes()
    else:
        with tempfile.TemporaryDirectory() as folder:
            dest=Path(folder)/'image.bmp'
            subprocess.run(['sips','-s','format','bmp',str(path),'--out',str(dest)],check=True,stdout=subprocess.DEVNULL)
            raw=dest.read_bytes()
    offset=struct.unpack_from('<I',raw,10)[0]
    width,height=struct.unpack_from('<ii',raw,18)
    return (width,height),raw[offset:]

root=Path(sys.argv[1]); results=[]
for screen in ('home','settings','search','sources','detail','library'):
    before=root/'base'/screen; after=root/'after'/screen
    assert (before/'rc').read_text()==(after/'rc').read_text(), f'exit code changed: {screen}'
    images=[p for p in before.iterdir() if p.suffix.lower() in ('.bmp','.png')]
    assert images, f'no shots: {screen}'
    for path in images:
        other=after/path.name
        assert other.exists(), other
        size,a=pixels(path); other_size,b=pixels(other)
        assert size==other_size and len(a)==len(b)
        mean=sum(abs(x-y) for x,y in zip(a,b))/len(a)
        results.append({'screen':screen,'file':path.name,'mean_byte_delta':round(mean,5),'identical':a==b})
(root/'comparison.json').write_text(json.dumps(results,indent=2)+'\n')
print(f'TV shots compared: {len(results)}; exact: {sum(r["identical"] for r in results)}')
print('Inspect comparison.json for timed/ambient differences.')
PY
