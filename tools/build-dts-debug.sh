#!/usr/bin/env bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Build an independent DTS Debug IPK. All arm.sh mutations occur in a private tree.
# Never installs, launches, or contacts a TV. Production staging/IPKs stay untouched.
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
output=${NUVIO_DTS_DEBUG_OUTPUT:-"$repo/build/dts-debug"}
image=${NUVIO_DTS_DEBUG_SDK_IMAGE:-${NUVIO_SDK_IMAGE:-nuvio-webos-sdk}}
runtime=${NUVIO_CONTAINER_RUNTIME:-podman}
packager=${NUVIO_ARES_PACKAGE:-}
if [ "$#" -gt 0 ]; then
  if [ "$#" = 2 ] && [ "$1" = --output ]; then output=$2
  else echo 'Usage: bash tools/build-dts-debug.sh [--output DIRECTORY]' >&2; exit 2; fi
fi
if [ -z "$packager" ]; then
  packager=$(command -v ares-package || true)
  if [ -z "$packager" ] && [ -x "$repo/../NuvioWeb-0.3.38-beta/node_modules/.bin/ares-package" ]; then
    packager="$repo/../NuvioWeb-0.3.38-beta/node_modules/.bin/ares-package"
  fi
fi
if [ -z "$packager" ] || [ ! -x "$packager" ]; then
  echo 'Set NUVIO_ARES_PACKAGE to an executable ares-package CLI (absolute path).' >&2; exit 2
fi
packager=$(python3 -c 'import os,sys;print(os.path.abspath(sys.argv[1]))' "$packager")
runtime=$(command -v "$runtime")
[ -f "$repo/src/app_id.h" ] || { echo 'DTS Debug app identity support is missing (src/app_id.h).' >&2; exit 2; }
mkdir -p "$output"
output=$(cd "$output" && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-dts-debug.XXXXXXXX")
mkdir -p "$work/tmp"
export TMPDIR="$work/tmp"
cleanup() { python3 - "$work" <<'PY'
import shutil,sys
shutil.rmtree(sys.argv[1],ignore_errors=True)
PY
}
trap cleanup EXIT
# Copy sources and distributable assets; no production executable, caches or
# person/account files enter the private tree. arm.sh independently audits the IPK.
python3 - "$repo" "$work" <<'PY'
import fnmatch,json,pathlib,shutil,sys
source=pathlib.Path(sys.argv[1]); target=pathlib.Path(sys.argv[2])
for directory in ('src','tools'):
    shutil.copytree(source/directory,target/directory,ignore=shutil.ignore_patterns('__pycache__','*.pyc'))
person={'trakt.txt','addons.txt','tmdb.txt','mdblist.txt','ajustes.txt','progresso.txt',
        'nuvem.txt','sessao.txt','perfil.txt','cliente.txt','listas.txt','guia-fav.txt',
        'debrid.txt','fanart.txt','p2p.txt','collections.json','catalogo-rede.bin',
        'catalogo-rede.bin.tmp','appinfo.json.stamped','.DS_Store'}
globs=('stalker-p*.txt','xtream-p*.txt','listas-p*.txt','trakt-p*.txt',
       'trakt-fluxo*.txt','simkl*.txt','conta-*.txt','conta-*.txt.tmp',
       'discord-p*.txt','discord-p*.txt.tmp','dts-starfish-webos*.so')
def ignore(directory,names):
    parent=pathlib.Path(directory)
    omitted={n for n in names if n in person or any(fnmatch.fnmatch(n,g) for g in globs)}
    if parent==source/'deploy'/'app': omitted.add('nuvio-proto')
    if parent==source/'deploy'/'app'/'art': omitted.update(('cache','collections'))
    return omitted
shutil.copytree(source/'deploy'/'app',target/'deploy'/'app',ignore=ignore)
metadata=target/'deploy'/'app'/'appinfo.json'
app=json.loads(metadata.read_text())
app['id']='space.nuvio.native.legacy.dtsdebug';app['title']='Nuvio Legacy DTS Debug'
metadata.write_text(json.dumps(app,ensure_ascii=False,indent=2)+'\n')
PY
# The normal SDK includes DTS; the debug helper only changes package identity.
export NUVIO_CONTAINER_RUNTIME="$runtime" NUVIO_SDK_IMAGE="$image" NUVIO_ARES_PACKAGE="$packager"
export NUVIO_BUILD_PLATFORM="${NUVIO_BUILD_PLATFORM:-linux/amd64}"
export NUVIO_DTS_FFMPEG=1
export NUVIO_EXTRA_CFLAGS="${NUVIO_EXTRA_CFLAGS:-} -DNV_DTS_DEBUG -DNV_APP_ID=\"space.nuvio.native.legacy.dtsdebug\""
# Read the original configuration by absolute path; never copy it into the package.
if [ -n "${NUVIO_PROPERTIES:-}" ]; then
  export NUVIO_PROPERTIES=$(python3 -c 'import os,sys;print(os.path.abspath(sys.argv[1]))' "$NUVIO_PROPERTIES")
else
  if [ -f "$repo/local.properties" ]; then export NUVIO_PROPERTIES="$repo/local.properties"
  else export NUVIO_PROPERTIES="$repo/../NuvioWeb-0.3.38-beta/local.properties"; fi
fi
( cd "$work" && bash tools/arm.sh --ipk --build )
# Validate the actual package identity and required native/notices payload, then
# publish only the IPK and checksums into the ignored output directory.
python3 - "$work" "$output" <<'PY'
import hashlib,io,json,pathlib,shutil,subprocess,sys,tarfile
work=pathlib.Path(sys.argv[1]); output=pathlib.Path(sys.argv[2])
packages=list(work.glob('*.ipk'))
if len(packages)!=1: raise SystemExit('Expected exactly one DTS Debug IPK')
package=packages[0]
data=subprocess.check_output(['ar','p',str(package),'data.tar.gz'])
with tarfile.open(fileobj=io.BytesIO(data),mode='r:gz') as archive:
    members=archive.getmembers()
    apps=[m for m in members if m.name.endswith('/appinfo.json')]
    if len(apps)!=1: raise SystemExit('Package has no unique appinfo.json')
    app=json.load(archive.extractfile(apps[0]))
    if app['id']!='space.nuvio.native.legacy.dtsdebug' or app['title']!='Nuvio Legacy DTS Debug':
        raise SystemExit('Wrong DTS Debug package identity')
    names={m.name for m in members}
    for suffix in ('/nuvio-proto','/lib/dts-starfish-webos3.so','/lib/dts-starfish-webos4.so',
                   '/licenses/dts/COPYING.LGPLv2.1','/licenses/dts/SOURCE.txt'):
        if not any(n.endswith(suffix) for n in names): raise SystemExit('Package missing '+suffix)
    binary=next(m for m in members if m.name.endswith('/nuvio-proto'))
    if binary.mode&0o777 != 0o755: raise SystemExit('Debug executable is not mode 755')
    binary_bytes=archive.extractfile(binary).read()
    if b'space.nuvio.native.legacy.dtsdebug' not in binary_bytes:
        raise SystemExit('Native binary does not contain the debug app identity')
destination=output/package.name
if destination.exists() and destination.read_bytes()!=package.read_bytes():
    raise SystemExit('Output already contains a different build; choose --output DIRECTORY')
shutil.copy2(package,destination)
digest=hashlib.sha256(package.read_bytes()).hexdigest()
(destination.with_suffix('.ipk.sha256')).write_text(digest+'  '+destination.name+'\n')
manifest={'id':app['id'],'title':app['title'],'version':app['version'],'file':destination.name,'sha256':digest}
(destination.with_suffix('.ipk.json')).write_text(json.dumps(manifest,indent=2)+'\n')
print('DTS Debug package: '+str(destination))
print('SHA256: '+digest)
PY
