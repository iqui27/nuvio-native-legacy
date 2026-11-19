#!/bin/bash
# Compile the real Tizen 6.5 release host using cached SDK/reference assemblies.
# No restore, package downloads, signing, installation, or native SDK invocation.
set -euo pipefail
cd "$(dirname "$0")/.."
export DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-i612-build.XXXXXXXX")
trap 'rm -rf "$work"' EXIT
python3 - "$work" <<'PY'
from pathlib import Path
import os, subprocess, sys, xml.etree.ElementTree as ET
home = Path.home()
w = Path(sys.argv[1])
dotnet = os.environ.get('NUVIO_DOTNET', str(home/'.dotnet/dotnet'))
csc = sorted((home/'.dotnet/sdk').glob('*/Roslyn/bincore/csc.dll'))[-1]
framework = sorted((home/'.dotnet/packs/Microsoft.NETCore.App.Ref').glob('*/ref/net8.0'))[-1]
api = sorted((home/'.nuget/packages/tizen.net.api9').glob('*/ref/*'))[-1]
project = Path('tizen-tpk/NuvioTpk65/NuvioTpk65.csproj')
p = ET.parse(project).getroot()
sources = [project.parent/x.attrib['Include'] for x in p.findall('.//Compile')
           if x.attrib.get('Condition') != "'$(NvTextoCanario)' == '1'"]
refs = list(framework.glob('*.dll')) + [x for x in api.glob('*.dll') if not (framework/x.name).exists()]
rsp = w/'host.rsp'
rsp.write_text('\n'.join('-r:"'+str(x)+'"' for x in refs)+'\n'+
               '\n'.join('"'+str(x.resolve())+'"' for x in sources))
subprocess.run([dotnet, str(csc), '-nologo', '-langversion:8', '-target:library',
                '-define:NV_API9', '-out:'+str(w/'NuvioTpk65.dll'), '@'+str(rsp)], check=True)
print('PASS: full Tizen 6.5 release host compiled against cached API9')
PY
bash -n tools/tpk.sh
printf '%s\n' 'PASS: TPK build script shell syntax (no dry-run mode available)'
