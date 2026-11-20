#!/bin/bash
# Offline compiler gates for #492; no sockets, downloads, packaging or deployment.
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-warm-offline.XXXXXX")
trap 'rm -rf "$dir"' EXIT
cc -O1 -g -std=gnu11 -Wall -Wextra -Werror -Isrc -pthread \
  tests/rede_pedido_buffer.c src/redeurl.c -o "$dir/buffer"
"$dir/buffer"
cc -fsyntax-only -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined src/main.c
# Emscripten headers/backend syntax only; no WGT packaging or I/O.
/Users/hrocha/emsdk/upstream/emscripten/emcc -fsyntax-only -Isrc \
  -I/opt/homebrew/include -Wno-macro-redefined src/main.c src/rede.c src/retomamorna.c
python3 - "$dir" <<'PY'
from pathlib import Path
import subprocess,sys
home=Path.home(); d=Path(sys.argv[1]); root=Path.cwd()
sdk=sorted((home/'.dotnet/sdk').glob('*/Roslyn/bincore/csc.dll'))[-1]
framework=sorted((home/'.dotnet/packs/Microsoft.NETCore.App.Ref').glob('*/ref/net8.0'))[-1]
refs=list(framework.glob('*.dll'))
for name in ['tizen.net.api4','tizen.net.tv']:
    base=home/'.nuget/packages'/name
    refs += [p for p in sorted(base.glob('*/ref/*/*.dll')) if not (framework/p.name).exists()]
# Each cached package is unique on this host; keep one assembly per name.
refs=list({p.name:p for p in refs}.values())
sources=[root/'tizen-tpk/NuvioTpk40/Program40.cs']+[root/'tizen-tpk'/n for n in ['Apps.cs','Video.cs','VideoWindowMetrics.cs','Carga.cs']]
rsp=d/'legacy.rsp'; rsp.write_text('\n'.join('-r:"'+str(p)+'"' for p in refs)+'\n'+'\n'.join('"'+str(p)+'"' for p in sources))
subprocess.run([str(home/'.dotnet/dotnet'),str(sdk),'-nologo','-langversion:8','-target:library','-out:'+str(d/'legacy.dll'),'@'+str(rsp)],check=True)
print('PASS: cached API4 legacy host compile; main syntax; network callbacks without I/O')
PY
