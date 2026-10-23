// Offline scale-factor proof: native-size UI references, alpha and GPU/host medians.
#include "gfx.c" // Reuse the real shader compiler and production quad.
#include "app.h"
#include "ajustes.h"
#include "addons.h"
#include "fileiras.h"
#include "artehero.h"
#include "catalogo.h"
#include "dados.h"
#include "gpunivel.h"
#include "resolucao.h"
#include "home.h"
#include "tex_cache.h"
#include "text.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Exercise weak-GPU dispatch without replacing the real drawing context.
static int pretendUtgard;
static const GLubyte *testGlString(GLenum e) {
  if (pretendUtgard && e==GL_RENDERER) return (const GLubyte*)"Mali-450";
  return glGetString(e);
}
#define glGetString testGlString
#define telaW gpunTelaW
#define telaH gpunTelaH
#ifdef IMGBAIXA_BASELINE
#include IMGBAIXA_BASELINE
int gpun_upscale_suportado(void) { return 1; }
void gpun_ampliar_app(int app) { (void)app; }
#else
#include "../src/gpunivel.c"
#endif
#undef telaW
#undef telaH
#undef glGetString

void ui720_settings(int width, int height, int app, int weak, int sourceHeight);
void ui720_saved(int width, int height, int app, int resolution);
static GLuint source, sourceFbo, query;
static Programa productionCopy;
static int sw=1920, sh=1080, iw=1280, ih=720;
static unsigned char *pixels, *flipped, *linear, *native;
static const GfxRect screen = {0,0,1920,1080};
static void readback(void) {
  glReadPixels(0,0,sw,sh,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  assert(glGetError()==GL_NO_ERROR);
}
static void png(const char *dir, const char *name) {
  static char written[64][1024];
  static size_t count;
  char path[1024];
  assert(snprintf(path,sizeof path,"%s/%s.png",dir,name)<(int)sizeof path);
  for(size_t i=0;i<count;i++) {
    if(!strcmp(written[i],path)) {
      fprintf(stderr,"FAIL duplicate PNG path: %s\n",path);
      abort();
    }
  }
  assert(count<sizeof written/sizeof written[0]);
  strcpy(written[count++],path);
  readback();
  for (int y=0;y<sh;y++) memcpy(flipped+(size_t)y*sw*4,pixels+(size_t)(sh-1-y)*sw*4,sw*4);
  SDL_Surface *s=SDL_CreateRGBSurfaceWithFormatFrom(flipped,sw,sh,32,sw*4,SDL_PIXELFORMAT_RGBA32);
  assert(s);
  assert(IMG_SavePNG(s,path)==0); SDL_FreeSurface(s);
  printf("PNG %s\n",path);
}
static void artCrop(const char *dir, const char *name, SDL_Rect rect) {
  char path[1024];
  assert(snprintf(path,sizeof path,"%s/%s.png",dir,name)<(int)sizeof path);
  SDL_Surface *full=SDL_CreateRGBSurfaceWithFormatFrom(flipped,sw,sh,32,sw*4,SDL_PIXELFORMAT_RGBA32);
  SDL_Surface *crop=SDL_CreateRGBSurfaceWithFormat(0,rect.w,rect.h,32,SDL_PIXELFORMAT_RGBA32);
  assert(full && crop);
  SDL_SetSurfaceBlendMode(full,SDL_BLENDMODE_NONE);
  assert(SDL_BlitSurface(full,&rect,crop,NULL)==0);
  assert(IMG_SavePNG(crop,path)==0);
  SDL_FreeSurface(crop); SDL_FreeSurface(full);
  printf("PNG %s\n",path);
}
static void frame(int settings) {
  txt_novo_quadro(); tex_novo_quadro(); tex_bombear(16); gfx_novo_quadro();
  gfx_sem_recorte();
  glClearColor(0.051f,0.051f,0.051f,1); glClear(GL_COLOR_BUFFER_BIT);
  if (settings) ajustes_desenhar(10000); else home_desenhar(10000);
  gfx_ambiente_descarregar();
  gfx_furo((GfxRect){1500,200,180,120});
  gfx_furo_raio((GfxRect){1500,400,180,120},24);
  gfx_furo_alfa((GfxRect){1500,600,180,120},0.45f);
}
static void target(int low) {
  progs[GFX_COPIA]=productionCopy;progAtual=-1;
  glBindFramebuffer(GL_FRAMEBUFFER,low?sourceFbo:0);
  glViewport(0,0,low?iw:sw,low?ih:sh); gfx_tamanho_alvo(low?iw:sw,low?ih:sh);
}
static int candidate;
static const char *names[]={"bilinear","unused","sgsr","sharp-bilinear"};
static Programa filters[4];
static void copy(int mode) {
  target(0); gfx_sem_recorte(); glDisable(GL_BLEND);
  glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
  gfx_tex_aspect_atual=0; gfx_opacidade_grupo=1;
  progs[GFX_COPIA]=filters[candidate]; progAtual=-1;
  float scale=(float)sw/iw;
  gfx_rect(screen,source,GFX_COPIA,candidate==7?1:scale,
           candidate==3?1.f/iw:candidate&&mode?iw:0,
           candidate==3?1.f/ih:candidate&&mode?ih:1,0,1,1,1,1);
  glEnable(GL_BLEND);
}
static double gradient(const unsigned char *p, int settings) {
  int x0=(settings?180:100)*sw/1920, x1=(settings?600:700)*sw/1920;
  int y0=(settings?180:620)*sh/1080, y1=(settings?950:845)*sh/1080;
  double sum=0;
  for (int y=y0;y<y1;y++) for (int x=x0;x<x1;x++) {
    size_t i=((size_t)(sh-1-y)*sw+x)*4;
    for (int c=0;c<3;c++) sum+=abs(p[i+c]-p[i+4+c])+abs(p[i+c]-p[i+sw*4+c]);
  }
  return sum/((x1-x0)*(y1-y0)*6.0);
}
static double error(const unsigned char *p, int settings) {
  int x0=(settings?180:100)*sw/1920, x1=(settings?600:700)*sw/1920;
  int y0=(settings?180:620)*sh/1080, y1=(settings?950:845)*sh/1080;
  double sum=0;
  for (int y=y0;y<y1;y++) for (int x=x0;x<x1;x++) {
    size_t i=((size_t)(sh-1-y)*sw+x)*4;
    for (int c=0;c<3;c++) sum+=abs(p[i+c]-native[i+c]);
  }
  return sum/((x1-x0)*(y1-y0)*3.0);
}
static double median(double *s) {
  for(int i=0;i<5;i++) for(int j=i+1;j<5;j++) if(s[j]<s[i]) {double t=s[i];s[i]=s[j];s[j]=t;}
  return s[2];
}
static void timing(int settings, int mode, int whole, double *host, double *gpu) {
  double hs[5],gs[5];
  for(int k=-1;k<5;k++) {
    glFinish(); Uint64 start=SDL_GetPerformanceCounter();
    glBeginQuery(GL_TIME_ELAPSED_EXT,query);
    if(whole==2) {
      progs[GFX_COPIA]=productionCopy;progAtual=-1;
      gpun_quadro_inicio();frame(settings);gpun_quadro_fim();
    } else {
      if(whole) {target(mode!=0); frame(settings);}
      if(!whole || mode) copy(mode==1?0:1);
    }
    glEndQuery(GL_TIME_ELAPSED_EXT); glFinish();
    double ms=1000.0*(SDL_GetPerformanceCounter()-start)/SDL_GetPerformanceFrequency();
    GLuint64 ns; glGetQueryObjectui64vEXT(query,GL_QUERY_RESULT,&ns);
    if(k>=0) {hs[k]=ms;gs[k]=ns/1e6;}
  }
  *host=median(hs); *gpu=median(gs);
}
int main(int argc,char **argv) {
  assert(argc==2 || argc==3); SDL_SetHint("SDL_MAC_BACKGROUND_APP","1");
  assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0); IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,8);
  SDL_Window *window=SDL_CreateWindow("SGSR proof",0,0,3840,2160,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  assert(window && SDL_GL_CreateContext(window)); SDL_GL_SetSwapInterval(0);
  assert(gfx_iniciar() && txt_iniciar("deploy/app",2));
  productionCopy=progs[GFX_COPIA];
  dados_iniciar("deploy/app/art"); ajustes_dir(getenv("NUVIO_DADOS"));
  tex_iniciar(64); gfx_icones_dir("deploy/app/art"); artehero_definir_falhou(tex_falhou);
  assert(home_iniciar("deploy/app/art"));
  AddonRemoto addon={.nome="Offline",.url="local",.ativo=1}; assert(addons_definir_lista(&addon,1));
  fil_registrar("offline_movie","Popular movies - Offline catalogue","Offline","Filmes",12);
  assert(fil_definir_tipo("offline_movie",FIL_TIPO_CARTAZ));
  static CatItem items[12]; CatFileira row={.ini=0,.n=12};
  snprintf(row.chave,sizeof row.chave,"offline_movie");
  snprintf(row.titulo,sizeof row.titulo,"Popular movies - Offline catalogue");
  snprintf(row.tipo,sizeof row.tipo,"movie");
  for(int i=0;i<12;i++) {
    snprintf(items[i].imdb,sizeof items[i].imdb,"tt90%05d",i);
    snprintf(items[i].tipo,sizeof items[i].tipo,"movie");
    snprintf(items[i].titulo,sizeof items[i].titulo,"The sharpness of small text %d",i+1);
    snprintf(items[i].sinopse,sizeof items[i].sinopse,
      "A local story about distant worlds. Every detail matters when reading from the sofa. "
      "Explore the catalogue, choose a movie and continue watching.");
    snprintf(items[i].logo,sizeof items[i].logo,"deploy/app/art/icone-home.png");
    snprintf(items[i].meta,sizeof items[i].meta,"2026 · 120 min");
    snprintf(items[i].poster,sizeof items[i].poster,"deploy/app/art/poster/%02d.jpg",i%8);
    snprintf(items[i].backdrop,sizeof items[i].backdrop,"deploy/app/art/%02d.jpg",i%8);
  }
  cat_definir_tudo(items,12,&row,1);
  target(0);
  for(int n=0;n<30;n++) {home_atualizar(1.f/60,10000);frame(0);}
  assert(ajustes_iniciar());
  assert(strstr((const char*)glGetString(GL_EXTENSIONS),"GL_EXT_timer_query"));
  printf("GPU %s; GL_EXT_timer_query; synchronized host wall-time medians, 5 frames\n",glGetString(GL_RENDERER));
  glGenQueries(1,&query);
  Programa sgsr=progs[GFX_COPIA];productionCopy=sgsr;
  filters[0]=filters[2]=sgsr;
  filters[3]=progs[GFX_COPIA_NITIDA];
  size_t capacity=(size_t)3840*2160*4;
  pixels=malloc(capacity);flipped=malloc(capacity);linear=malloc(capacity);native=malloc(capacity);
  assert(pixels&&flipped&&linear&&native);
  int failed=0;
  if (getenv("NUVIO_SHOT_C9")) {
    sw=1920; sh=1080; iw=1920; ih=1080;
    ui720_settings(3840,2160,1,0,1080);
    png(argv[1],"c9-hero");
    artCrop(argv[1],"c9-backdrop",(SDL_Rect){900,100,500,400});
    SDL_Event down={0};
    for (int row=0;row<2;row++) {
      down.type=SDL_KEYDOWN; down.key.keysym.sym=SDLK_DOWN; home_evento(&down);
      down.type=SDL_KEYUP; home_evento(&down);
      for (int n=0;n<90;n++) { home_atualizar(1.f/60,10000); frame(0); }
    }
    // Home art is stable; Settings frost changes between consecutive frames.
    for (int effects=0;effects<4;effects++) {
      gpun_iniciar(sw,sh);
      gpun_preferencia(effects);
      gpun_ampliar_app(ajustes_ampliar_app());
      target(0); frame(0); readback(); memcpy(native,pixels,(size_t)sw*sh*4);
      gpun_quadro_inicio(); frame(0); gpun_quadro_fim();
      readback();
      GLint viewport[4]; glGetIntegerv(GL_VIEWPORT,viewport);
      assert(viewport[2]==1920 && viewport[3]==1080 && intW==0);
      assert(!memcmp(native,pixels,(size_t)sw*sh*4));
      if (effects==3) {
        png(argv[1],"c9-home");
        artCrop(argv[1],"c9-poster",(SDL_Rect){100,620,600,225});
      }
      printf("PASS C9 effects=%d native 1920x1080; gradient=%.6f MAE=%.6f\n",
             effects,gradient(pixels,0),error(pixels,0));
    }
    // Negative control: explicit 720p must still work and lose native detail.
    gpun_iniciar(sw,sh); gpun_preferencia(3); gpun_forcar_720();
    gpun_ampliar_app(ajustes_ampliar_app());
    gpun_quadro_inicio(); frame(0); gpun_quadro_fim(); readback();
    assert(intW==1280 && intH==720 && error(pixels,0)>0.1);
    printf("PASS C9 explicit 720p negative control MAE=%.6f\n",error(pixels,0));
    return 0;
  }
#ifndef IMGBAIXA_BASELINE
  if(argc==3) {
    // C9 drawable: effects never select a lower-resolution target.
    sw=1920; sh=1080;
    for(int level=0;level<=2;level++) {
      gpun_iniciar(sw,sh);gpun_definir_nivel(level);
      gpun_quadro_inicio();
      GLint bound,viewport[4];glGetIntegerv(GL_FRAMEBUFFER_BINDING,&bound);
      glGetIntegerv(GL_VIEWPORT,viewport);
      assert(!bound && viewport[2]==1920 && viewport[3]==1080);
      frame(1);gpun_quadro_fim();
      char name[64];snprintf(name,sizeof name,"native-level-%d",level);png(argv[1],name);
    }
    puts("PASS C9 levels 0/1/2 retain native 1080p; 720p only explicit");
    for(int algo=0;algo<=5;algo++) for(int sharp=0;sharp<=4;sharp+=4) {
      gpun_iniciar(sw,sh);gpun_forcar_720();gpun_ampliar_app(1);gpun_ampliacao(algo,sharp);
      for(int repeat=0;repeat<2;repeat++) {
        gpun_quadro_inicio();frame(1);
        glEnable(GL_SCISSOR_TEST);glScissor(40,40,200,120);
        glClearColor(0.2f,0.4f,0.6f,1);glClear(GL_COLOR_BUFFER_BIT);glDisable(GL_SCISSOR_TEST);
        gpun_quadro_fim();readback();
        size_t bytes=(size_t)sw*sh*4;
        memcpy(native,pixels,bytes);
        char name[64];snprintf(name,sizeof name,"algo-%d-sharp-%d-frame-%d",algo,sharp,repeat);png(argv[1],name);
        source=intTex;candidate=0;copy(0);readback();
        int pass=1;
        for(size_t i=3;i<bytes;i+=4) if(native[i]!=pixels[i]) {pass=0;break;}
        size_t patch=((size_t)100*sw+150)*4;
        for(int c=0;c<3;c++) if(abs(native[patch+c]-(c+1)*51)>2) pass=0;
        printf("%s C9 algorithm=%d sharpness=%d frame=%d flat RGB=%d/%d/%d alpha=bilinear contract\n",
          pass?"PASS":"FAIL",algo,sharp,repeat,native[patch],native[patch+1],native[patch+2]);
        if(!pass) failed=1;
      }
    }
    return failed;
  }
#endif
  for(int test=0;test<4;test++) {
    sw=test==1||test==2?3840:1920;sh=sw*9/16;
    iw=test>=2?1920:1280;ih=iw*9/16;
    size_t bytes=(size_t)sw*sh*4;
    glGenTextures(1,&source);glBindTexture(GL_TEXTURE_2D,source);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,iw,ih,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);gfx_tex_esquecer(0);
    glGenFramebuffers(1,&sourceFbo);glBindFramebuffer(GL_FRAMEBUFFER,sourceFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,source,0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    ui720_settings(3840,2160,test<3,0,ih);
    for(int settings=0;settings<2;settings++) for(int glass=0;glass<2;glass++) {
      const char *scene=settings?"settings":"home";
      ajustes_definir_vidro(glass);
      gpun_iniciar(sw,sh);
      if(ih==720) gpun_forcar_720();
      else if(sh==2160) gpun_alvo_1080();
      gpun_ampliar_app(ajustes_ampliar_app());
      // Isolate scaling quality from the internal target's effect level.
      gfx_definir_efeitos_leves(0);gfx_definir_efeitos_minimos(0);
      target(0);for(int n=0;n<2;n++) frame(settings);
      readback();memcpy(native,pixels,bytes);
      double ref=gradient(native,settings);
      gpun_quadro_inicio();frame(settings);
      if(test==3) {
        GLint bound;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&bound);assert(bound==0);
        gpun_quadro_fim();continue;
      }
      // Production must restore alpha writes after a masked pass.
      glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_FALSE);
      gpun_quadro_fim();readback();memcpy(flipped,pixels,bytes);
      double g=gradient(pixels,settings),e=error(pixels,settings);
      GLuint savedSource=source;source=intTex;candidate=0;
      copy(0);readback();memcpy(linear,pixels,bytes);
      double b=gradient(linear,settings);
      for(size_t i=3;i<bytes;i+=4) assert(flipped[i]==linear[i]);
      assert(flipped[((size_t)(sh-1-250*sh/1080)*sw+1550*sw/1920)*4+3]==0);
      // Require a measurable edge gain, including the subtle 3x Settings case.
      int pass=memcmp(flipped,linear,bytes)!=0 && g>b*1.005;
      printf("%s production quality %d->%d %s glass=%d gradient=%.6f bilinear=%.6f native=%.6f MAE=%.6f alpha=identical\n",
             pass?"PASS":"FAIL",ih,sh,scene,glass,g,b,ref,e);
      if(!pass) failed=1;
      // The same source and production dispatch must retain exact TV output.
      gpun_ampliar_app(0);
      gpun_quadro_inicio();gpun_quadro_fim();readback();
      assert(!memcmp(linear,pixels,bytes));
      source=savedSource;
      printf("PASS TV %d->%d %s glass=%d exact bilinear RGBA\n",ih,sh,scene,glass);
    }
    // Choice costs use the production level, including minimal 720p effects.
    gpun_iniciar(sw,sh);
    if(ih==720) gpun_forcar_720();
    else if(sh==2160) gpun_alvo_1080();
    ui720_settings(3840,2160,test<3,0,ih);
    gpun_ampliar_app(ajustes_ampliar_app());
    for(int settings=0;settings<2;settings++) {
      double host,gpu;
      candidate=test==0?2:test==3?0:3;
      for(int n=0;n<2;n++) {target(ih<sh);frame(settings);if(ih<sh)copy(1);}
      target(0);
      timing(settings,test==3?0:2,2,&host,&gpu);
      printf("CHOICE ampliar=%s %d->%d %s level=%d host=%.4f GPU=%.4f ms\n",
        test<3?"app":"tv",ih,sh,settings?"settings":"home",gpun_nivel(),host,gpu);
      if(settings) {
        char name[160];snprintf(name,sizeof name,"r4-test%d-%d-to-%d-settings-choice-%s-%s",test,ih,sh,test<3?"app":"tv",test==3?"native":test==0?"sgsr":"sharp-bilinear");
        png(argv[1],name);
      }
    }
    glDeleteFramebuffers(1,&sourceFbo);glDeleteTextures(1,&source);gfx_tex_esquecer(0);
  }
  // Saved app/TV choice must survive 720p -> Automatic/4K -> runtime fallback.
  // Compare real pixels and one-copy dispatch with the integration bilinear path.
  dados_iniciar(getenv("NUVIO_DADOS"));
  for(int panel=0;panel<2;panel++) for(int weak=0;weak<2;weak++)
    for(int app=0;app<2;app++) for(int resolution=RES_AUTO;resolution<=RES_4K;resolution+=2) {
      sw=panel?3840:2560;sh=panel?2160:1440;iw=1920;ih=1080;
      size_t bytes=(size_t)sw*sh*4;
      pretendUtgard=weak;
      ui720_saved(sw,sh,app,resolution);
      progs[GFX_COPIA]=productionCopy;progAtual=-1;
      gpun_iniciar(sw,sh);
      gpun_ampliar_app(ajustes_ampliar_app());
      if(!panel) {
        assert(!ajustes_ampliar_app());
      }
      gpun_quadro_inicio();
      GLint bound;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&bound);assert(bound==0);
      ResAuto automatic={0};ResVigia watch={0};
      int fallback=0;
      for(int n=0;n<RES_4K_SEGUIDAS && !fallback;n++)
        fallback=resolution==RES_AUTO ? res_auto_amostra(&automatic,20,55,1)==RES_AUTO_REBAIXOU
                                     : res_vigia_amostra(&watch,20,55,1);
      assert(fallback);
      gpun_alvo_1080();
      gpun_quadro_inicio();frame(0);gfx_novo_quadro();
      gpun_quadro_fim();
      assert(gfx_n_rect==1);
      assert(progAtual==(app && panel && !weak?GFX_COPIA_NITIDA:GFX_COPIA));
      readback();memcpy(native,pixels,bytes);
      source=intTex;candidate=app && panel && !weak?3:0;
      copy(candidate!=0);readback();assert(!memcmp(native,pixels,bytes));
      printf("PASS saved %s resolution=%d panel=%dx%d Mali=%d runtime fallback %s, one copy, exact RGBA\n",
             app?"app":"TV",resolution,sw,sh,weak,candidate?"sharp-bilinear":"bilinear");
    }
  candidate=0;progs[GFX_COPIA]=sgsr;progAtual=-1;
  sw=1920;sh=1080;iw=1280;ih=720;
  pretendUtgard=1;gpun_iniciar(sw,sh);assert(!gpun_upscale_suportado());gpun_forcar_720();
  ui720_settings(3840,2160,0,1,720);
  gpun_quadro_inicio();frame(0);
  glGenTextures(1,&source);glBindTexture(GL_TEXTURE_2D,source);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,iw,ih,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,iw,ih);gfx_tex_esquecer(0);
  gpun_quadro_fim();readback();memcpy(linear,pixels,(size_t)sw*sh*4);
  copy(0);readback();assert(!memcmp(linear,pixels,(size_t)sw*sh*4));
  progs[GFX_COPIA]=productionCopy;progAtual=-1;
  for(int n=0;n<8;n++) {gpun_quadro_inicio();frame(1);gpun_quadro_fim();}
  png(argv[1],"r4-test4-720-to-1080-settings-choice-mali-disabled-bilinear");
  pretendUtgard=0;glDeleteTextures(1,&source);gfx_tex_esquecer(0);
  gpun_iniciar(sw,sh);ui720_settings(1920,1080,0,0,1080);target(0);frame(1);
  png(argv[1],"r4-test5-1080-to-1080-settings-choice-non4k-hidden-native");
  puts("PASS Mali-450 automatically selects exact bilinear output");
  pretendUtgard=0;sw=1920;sh=1080;
  gpun_iniciar(sw,sh);gpun_forcar_720();gpun_ampliar_app(0);
  gpun_quadro_inicio();frame(0);gpun_quadro_fim();
  assert(progAtual==GFX_COPIA);
  puts("PASS forced 720p with TV scaling keeps bilinear");
  gpun_iniciar(1280,720);gpun_forcar_720();gfx_novo_quadro();gpun_quadro_inicio();
  GLint bound;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&bound);assert(bound==0);
  gpun_quadro_fim();assert(gfx_n_rect==0);
  gpun_iniciar(3840,2160);gpun_quadro_inicio();glGetIntegerv(GL_FRAMEBUFFER_BINDING,&bound);assert(bound==0);
  puts("PASS native 720/4K bypass, all production targets and alpha");
  free(pixels);free(flipped);free(linear);free(native);
  return failed;
}
