#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
work=$(mktemp -d "$TMPDIR/nuvio-search-test.XXXXXXXX")
trap 'rm -rf "$work"' EXIT
mkdir "$work/data" "$work/home"
export NUVIO_DADOS="$work/data" HOME="$work/home"
cc tests/desktop_search.c src/buscanorm.c -DNV_TOUCH_UI -DNV_DESKTOP_UI -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations \
  -ffunction-sections -fdata-sections -Wl,-dead_strip -Wl,-undefined,dynamic_lookup \
  -L/opt/homebrew/lib -lSDL2 -o "$work/search"
"$work/search"
cc tests/desktop_capture_data.c -DNV_DESKTOP_UI -Isrc -I/opt/homebrew/include \
  -ffunction-sections -fdata-sections -Wl,-dead_strip -Wl,-undefined,dynamic_lookup -o "$work/data-test"
"$work/data-test"
# Compile both untouched platforms from identical stdin paths, then compare bytes.
python3 - "$work" <<'PY'
from pathlib import Path
import subprocess, sys
work=Path(sys.argv[1])
for mode, flags in [('tv',[]),('phone',['-DNV_TOUCH_UI'])]:
    for name in ('busca','dados','main'):
        original=subprocess.check_output(['git','show','af596b224:src/'+name+'.c'])
        for variant, source in [('before',original),('after',Path('src/'+name+'.c').read_bytes())]:
            subprocess.run(['cc','-x','c','-','-c','-O1','-Isrc','-I/opt/homebrew/include','-I/opt/homebrew/include/SDL2','-Wno-deprecated-declarations',*flags,'-o',str(work/(name+'-'+mode+'-'+variant+'.o'))],input=source,check=True)
        assert (work/(name+'-'+mode+'-before.o')).read_bytes()==(work/(name+'-'+mode+'-after.o')).read_bytes(), (name,mode,'platform bytes changed')
print('PASS: TV/phone busca, dados, main object files byte-identical to baseline')
PY
