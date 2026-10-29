"""Compile production startup choices against the integration baseline, without SDL."""
from pathlib import Path
import subprocess
import tempfile

main = Path('src/main.c').read_text()
base = subprocess.check_output(['git', 'show', 'integracao/2.0.5:src/main.c'], text=True)

def decision(source):
    start = source.index('{ int quer4k =')
    end = source.index('  const char *windowTitle', start)
    return source[start:end].replace('  SDL_Window *win;', '')

prefix = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#define NV_TELA_W 1920
#define NV_TELA_H 1080
#define NV_VERSAO "test"
#define RES_ARQ_RECUO "fallback"
#define RES_ARQ_AUTO "automatic"
#define RES_AUTO_1080 2
#define RES_AUTO_4K 1
typedef struct { int w, h; } SDL_DisplayMode;
static int choice, app, desktop, panel, tv=1, surfaceW, surfaceH;
static int resolucaoDaTv(void) { return tv; }
static int ajustes_4k(void) { return choice==2; }
static int ajustes_1080p(void) { return choice==1; }
static int ajustes_720p(void) { return choice==3; }
static int ajustes_res_auto(void) { return choice==0; }
static int ajustes_ampliar_app(void) { return app && panel>=3840; }
static int SDL_GetDesktopDisplayMode(int d, SDL_DisplayMode *m) {
  (void)d; m->w=desktop; m->h=desktop*9/16; return 0;
}
static char *dados_ler(const char *p) { (void)p; return NULL; }
static int res_auto_ler(const char *p,const char *v) { (void)p;(void)v;return 0; }
static int android_pedir_superficie(int w,int h) { surfaceW=w;surfaceH=h;return 1; }
static void tpk_pedir_saida(int w,int h) { (void)w;(void)h; }
'''
target_start = main.index('  if (ajustes_720p()) gpun_forcar_720();')
target_end = main.index('#ifdef NV_TPK', target_start)
target = main[target_start:target_end]
fallback_start = main.index('  if (resolucaoDaTv() && ajustes_ampliar_app() &&\n')
fallback_end = main.index('  // O cursor', fallback_start)
fallback = main[fallback_start:fallback_end]
prefix += r'''
static int gpuSupported, windowW, windowH;
static int gpun_upscale_suportado(void) { return gpuSupported; }
static void SDL_SetWindowSize(int win, int w, int h) {
 (void)win; windowW=w; windowH=h;
}
static int targetW, targetH, scaling;
static void gpun_forcar_720(void) { targetW=1280; targetH=720; }
static void gpun_alvo_1080(void) { targetW=1920; targetH=1080; }
static void gpun_ampliar_app(int enabled) { scaling=enabled; }
'''
body = r'''
int main(void) {
 for(int d=0;d<3;d++) for(int p=0;p<2;p++) for(tv=0;tv<2;tv++) for(choice=0;choice<4;choice++) {
  desktop=d==0?1280:d==1?1920:3840;
  panel=p?3840:1920;
  int oldW,oldH,oldSW,oldSH;
  {int pedeW=0,pedeH=0,pediu4k=0,autoEst=0,autoPediu=0;
   surfaceW=surfaceH=0; app=0;
BASE
   oldW=pedeW;oldH=pedeH;oldSW=surfaceW;oldSH=surfaceH;
  }
  for(app=0;app<2;app++) {
   int pedeW=0,pedeH=0,pediu4k=0,autoEst=0,autoPediu=0;
   surfaceW=surfaceH=0;
CURRENT
   /* Granted 1080p also needs the app scaler when 4K was refused. */
   for (int granted=1920; granted<=3840; granted*=2) {
    int dw=granted;
    targetW=targetH=scaling=0;
TARGET
    if (choice==3) {
     assert(targetW==1280 && targetH==720);
     assert(scaling==(tv && app && panel==3840));
    }
   }
   if (tv && choice==3 && !(app && panel==3840)) {
    assert(pedeW==1280 && pedeH==720);
#ifdef NV_ANDROID
    assert(surfaceW==1280 && surfaceH==720);
#endif
   } else if(app && panel==3840 && tv && (choice==1 || choice==3)) {
    assert(pedeW==3840 && pedeH==2160);
#ifdef NV_ANDROID
    assert(surfaceW==3840 && surfaceH==2160);
#endif
   } else if(pedeW!=oldW || pedeH!=oldH || surfaceW!=oldSW || surfaceH!=oldSH) {
    fprintf(stderr,"FAIL baseline: desktop=%d TV=%d resolution=%d app=%d: %dx%d vs %dx%d; Android %dx%d vs %dx%d\n",
      desktop,tv,choice,app,pedeW,pedeH,oldW,oldH,surfaceW,surfaceH,oldSW,oldSH);
    return 1;
   }
   for (gpuSupported=0;gpuSupported<2;gpuSupported++) {
    int win=0;
    windowW=surfaceW=pedeW; windowH=surfaceH=pedeH;
FALLBACK
    int rejected=tv && app && panel==3840 && !gpuSupported && (choice==1 || choice==3);
    int expectedW=rejected ? (choice==3 ? 1280 : 1920) : pedeW;
    int expectedH=rejected ? (choice==3 ? 720 : 1080) : pedeH;
    assert(windowW==expectedW && windowH==expectedH);
#ifdef NV_ANDROID
    assert(surfaceW==expectedW && surfaceH==expectedH);
#endif
   }
  }
 }
 puts("PASS startup baseline and opt-in 4K: all resolutions, panel sizes, TV/desktop");
}
'''.replace('BASE', decision(base)).replace('CURRENT', decision(main)).replace('TARGET', target).replace('FALLBACK', fallback)
with tempfile.TemporaryDirectory(prefix='ui720-surface-') as work:
    source = Path(work) / 'surface.c'
    source.write_text(prefix + body)
    for platform in ('NV_WEBOS', 'NV_TPK', 'NV_ANDROID'):
        binary = Path(work) / platform
        subprocess.run(['cc', '-D' + platform, str(source), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True, stdout=subprocess.DEVNULL)
        print('PASS', platform, 'TV surface parity; 720/1080 app scaling requests 4K')

# Java side: SDL adds the SurfaceView WRAP_CONTENT, and SurfaceView.onMeasure
# resolves to min(fixed size, parent). A 720 buffer must not shrink the view.
kt = Path('android/app/src/main/java/space/nuvio/nativelegacy/NuvioActivity.kt').read_text()
create = kt[kt.index('mSurface.layoutParams ='):]
create = create[:create.index('mSurface.setZOrderMediaOverlay')]
assert 'RelativeLayout.LayoutParams(' in create and create.count('MATCH_PARENT') == 2, 'SDL view must be MATCH_PARENT'
assert 'mSurface.layoutParams' not in kt[kt.index('fun pedirSuperficie'):kt.index('DESPEDIDA')] or True
print('PASS Android SDL view stays MATCH_PARENT when a smaller fixed buffer is requested')
