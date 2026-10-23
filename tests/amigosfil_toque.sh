#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-abas-shot.XXXXXX")
trap 'rm -rf "$task_dir"' EXIT
export NUVIO_DADOS="$task_dir" FRIENDS_TOUCH_WORK="$task_dir"
python3 - <<'PY'
import concurrent.futures,os,pathlib,shlex,subprocess,sys,hashlib,json,shutil
work=pathlib.Path(os.environ['FRIENDS_TOUCH_WORK'])
flags=['-std=gnu11','-O0','-g','-Isrc','-DNV_TOUCH_UI','-DNV_AMIGOSFIL_TESTE','-DNV_SOCIALVIS_DEMO','-DNV_AMIGOS_TOUCH_AUDIT']
flags+=shlex.split(subprocess.check_output(['pkg-config','--cflags','sdl2','SDL2_image','SDL2_ttf'],text=True))
libs=shlex.split(subprocess.check_output(['pkg-config','--libs','sdl2','SDL2_image','SDL2_ttf'],text=True))
if sys.platform=='darwin':libs+=['-lz','-framework','OpenGL']
else:flags+=['-DNV_LINUX_DESKTOP','-DGL_UNPACK_ROW_LENGTH=0x0CF2'];libs+=['-lz','-lGLESv2','-pthread','-lm','-ldl']
if os.environ.get('SANITIZE')=='1':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
flags+=['-Wno-deprecated-declarations']
sources=sorted(pathlib.Path('src').glob('*.c'))+sorted(pathlib.Path('src/dts').glob('*.c'))
sources=[s for s in sources if str(s) not in ['src/main.c','src/recomenda.c','src/rede.c','src/celular.c']]
sources+=[pathlib.Path('tests/amigos_estilos_shot.c')]
cache=pathlib.Path(os.environ.get('FRIENDS_TOUCH_CACHE',str(work)));cache.mkdir(parents=True,exist_ok=True)
def compile(s):
 obj=cache/(str(s).replace('/','_')+'.o');dep=pathlib.Path(str(obj)+'.d');stamp=pathlib.Path(str(obj)+'.sha')
 def digest():
  deps=[str(s)]
  if dep.exists():deps+=shlex.split(dep.read_text().replace('\\\n',' ').split(':',1)[-1])
  h=hashlib.sha256(json.dumps(flags).encode())
  for path in sorted(set(deps)):
   f=pathlib.Path(path)
   if not f.exists():return ''
   h.update(path.encode());h.update(f.read_bytes())
  return h.hexdigest()
 key=digest()
 if key and obj.exists() and stamp.exists() and stamp.read_text()==key:return obj,subprocess.CompletedProcess([],0,'')
 r=subprocess.run(['cc',*flags,'-MMD','-MF',str(dep),'-c',str(s),'-o',str(obj)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 if not r.returncode:stamp.write_text(digest())
 return obj,r
objects=[]
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
 for obj,r in pool.map(compile,sources):
  if r.returncode:print(r.stdout[-12000:]);raise SystemExit(r.returncode)
  objects.append(str(obj))
def link(objs,output):
 r=subprocess.run(['cc',*flags,*objs,*libs,'-o',str(output)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 if r.returncode:print(r.stdout[-12000:]);raise SystemExit(r.returncode)
# Optional local before/after proof; the normal regression does not need git history.
if base_dir:=os.environ.get('FRIENDS_BASELINE_DIR'):
 base=work/'before';base.mkdir()
 for name in ['amigosfil.c','amigosfil_estilos.inc']:
  shutil.copyfile(pathlib.Path(base_dir)/name,base/name)
 obj,r=compile(base/'amigosfil.c')
 if r.returncode:print(r.stdout[-12000:]);raise SystemExit(r.returncode)
 before=[str(obj) if s.endswith('src_amigosfil.c.o') else s for s in objects]
 link(before,work/'before-test')
 r=subprocess.run([str(work/'before-test'),str(work/'shot')],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 print(r.stdout[-5000:]);assert r.returncode!=0 and 'has no touch target' in r.stdout,'Baseline did not reproduce the expected hit-target failure'
 print('PASS: unchanged baseline reproduces missing friend-card touch targets')
link(objects,work/'test')
subprocess.run([str(work/'test'),str(work/'shot')],check=True)
PY
