// Hidden macOS GL fixture. Observe the production renderer before each real draw.
#include "escala_ancoras.h"
#include "ajustes.h"
#include "catalogo.h"
#include "dados.h"
#include "home.h"
#include "addons.h"
#include "ilha.h"
#include "tex_cache.h"
#include <SDL2/SDL_image.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

typedef struct { GfxRect r; GLuint tex; } Draw;
static Draw draws[8192];
static Draw boxes[16384];
static int nboxes, ndraws, failures;
static const char *scene;
static float scale;
static struct { char scene[80], name[60]; GfxRect r; } refs[512];
static int nrefs;
static const char *shots;
// Keep the clock deterministic without replacing its production draw path.
time_t time(time_t *out) { time_t t = 1800000000; if (out) *out = t; return t; }
void anchor_probe(GfxRect r, GLuint tex, GfxModo mode, float f, float px, float py,
              float radius, float cr, float cg, float cb, float ca) {
  if(ca * gfx_opacidade_grupo > .1f && nboxes<16384) {
    float e=gfx_escala(); boxes[nboxes++]=(Draw){{r.x*e,r.y*e,r.w*e,r.h*e},tex};
  }
  if (mode == GFX_TEXTO && ca * gfx_opacidade_grupo > .1f && ndraws < 8192) {
    float e = gfx_escala();
    draws[ndraws++] = (Draw){{r.x * e, r.y * e, r.w * e, r.h * e}, tex};
  }
}
void shot_no_text_overlap(void) {
  for(int i=0;i<ndraws;i++) for(int j=i+1;j<ndraws;j++) {
    GfxRect a=draws[i].r,b=draws[j].r;
    if(draws[i].tex==draws[j].tex && fabsf(a.x-b.x)<3 && fabsf(a.y-b.y)<3) continue;
    float w=fminf(a.x+a.w,b.x+b.w)-fmaxf(a.x,b.x);
    float h=fminf(a.y+a.h,b.y+b.h)-fmaxf(a.y,b.y);
    if(w>2 && h>2) {
      fprintf(stderr,"text overlap %.0f,%.0f %.0fx%.0f / %.0f,%.0f %.0fx%.0f\n",a.x,a.y,a.w,a.h,b.x,b.y,b.w,b.h);
      shot_check("text does not overlap",0); return;
    }
  }
}
void shot_check(const char *name, int ok) {
  if (!ok) { fprintf(stderr, "FAIL %s %.0f%% %s\n", scene, scale * 100, name); failures++; }
}
void shot_reset(void) { ndraws = nboxes = 0; }
GfxRect shot_box(GfxRect expected) {
  float e=gfx_escala();
  expected=(GfxRect){expected.x*e,expected.y*e,expected.w*e,expected.h*e};
  for(int i=0;i<nboxes;i++) { GfxRect r=boxes[i].r;
    if(fabsf(r.x-expected.x)<1 && fabsf(r.y-expected.y)<1 &&
       fabsf(r.w-expected.w)<1 && fabsf(r.h-expected.h)<1) return r;
  }
  shot_check("expected component actually drawn",0); return (GfxRect){0};
}
GfxRect shot_text(TxtLinha line) {
  for (int i = 0; i < ndraws; i++) if (draws[i].tex == line.tex) return draws[i].r;
  shot_check("expected text was rasterized and drawn", 0);
  return (GfxRect){0};
}
GfxRect shot_text_bounds(void) {
  GfxRect r = {0};
  shot_check("screen draws text", ndraws > 0);
  for (int i = 0; i < ndraws; i++) {
    GfxRect d = draws[i].r;
    if (!i) r = d;
    else { float right = fmaxf(r.x+r.w, d.x+d.w), bottom = fmaxf(r.y+r.h,d.y+d.h);
      r.x = fminf(r.x,d.x); r.y = fminf(r.y,d.y); r.w=right-r.x; r.h=bottom-r.y; }
  }
  return r;
}
GfxRect shot_component_bounds(GfxRect area) {
  GfxRect bounds={0}; int count=0;
  float e=gfx_escala();
  area=(GfxRect){area.x*e,area.y*e,area.w*e,area.h*e};
  for(int i=0;i<nboxes;i++) {
    GfxRect r=boxes[i].r;
    if(r.x<area.x-1 || r.y<area.y-1 || r.x+r.w>area.x+area.w+1 || r.y+r.h>area.y+area.h+1) continue;
    if(!count++) bounds=r;
    else {
      float right=fmaxf(bounds.x+bounds.w,r.x+r.w), bottom=fmaxf(bounds.y+bounds.h,r.y+r.h);
      bounds.x=fminf(bounds.x,r.x); bounds.y=fminf(bounds.y,r.y);
      bounds.w=right-bounds.x; bounds.h=bottom-bounds.y;
    }
  }
  shot_check("component has rendered bounds",count>0);
  return bounds;
}
void shot_anchor(const char *name, GfxRect r, int edges) {
  // edges: 0 left/top, 1 left/bottom, 2 right/top; dimensions may grow.
  if (edges == 1) r.y += r.h;
  if (edges == 2) r.x += r.w;
  for (int i=0;i<nrefs;i++) if (!strcmp(refs[i].scene,scene) && !strcmp(refs[i].name,name)) {
    int ok = fabsf(r.x-refs[i].r.x)<=2 && fabsf(r.y-refs[i].r.y)<=2;
    printf("%s %s %.0f%% %s %.1f,%.1f reference %.1f,%.1f\n", ok?"PASS":"FAIL",scene,scale*100,name,r.x,r.y,refs[i].r.x,refs[i].r.y);
    shot_check(name,ok); return;
  }
  shot_check("100% reference first",scale==1);
  assert(nrefs<512);
  snprintf(refs[nrefs].scene,80,"%s",scene); snprintf(refs[nrefs].name,60,"%s",name);
  refs[nrefs++].r=r;
}
void shot_disjoint(const char *name, GfxRect a, GfxRect b) {
  shot_check(name,a.x+a.w<=b.x+1 || b.x+b.w<=a.x+1 || a.y+a.h<=b.y+1 || b.y+b.h<=a.y+1);
}
void shot_contains(const char *name,GfxRect outer,GfxRect inner) {
  shot_check(name,inner.x>=outer.x-1 && inner.y>=outer.y-1 && inner.x+inner.w<=outer.x+outer.w+1 && inner.y+inner.h<=outer.y+outer.h+1);
}
void shot_begin(const char *name,float s) {
  scene=name; scale=s; shot_reset();
  txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro();
  gfx_opacidade_grupo=1; gfx_sem_recorte();
  glClearColor(.025,.025,.03,1); glClear(GL_COLOR_BUFFER_BIT);
}
void shot_end(void) {
  char path[1024];
  unsigned char *pixels=malloc(1920*1080*4);
  SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32);
  assert(pixels && s);
  glFinish(); glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  for(int y=0;y<1080;y++) memcpy((char*)s->pixels+y*s->pitch,pixels+(1079-y)*1920*4,1920*4);
  snprintf(path,sizeof path,"%s/%s-%03.0f.png",shots,scene,scale*100);
  assert(IMG_SavePNG(s,path)==0); SDL_FreeSurface(s); free(pixels);
}
extern void ajustes_teste_cena_t(float);
int main(void) {
  int focused=getenv("NV_ANCHOR_FOCUSED")!=NULL;
  int quick=getenv("NV_ANCHOR_FULL")==NULL;
  shots=getenv("NV_ANCHOR_SHOTS"); assert(shots);
  const char *dir=getenv("NUVIO_DADOS"); assert(dir); dados_iniciar(dir);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP","1");
  assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *win=SDL_CreateWindow("Rendered anchors",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  assert(win && SDL_GL_CreateContext(win));
  GLuint texture,fbo; glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1920,1080,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_FRAMEBUFFER,fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
  glViewport(0,0,1920,1080); gfx_tamanho_alvo(1920,1080); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app",1)); tex_iniciar(64); gfx_icones_dir("deploy/app/art");
  ajustes_teste_cena_t(1);
  static CatItem items[6]; CatFileira row={0}; CatEp eps[3]={0};
  for(int i=0;i<6;i++) {
    snprintf(items[i].titulo,sizeof items[i].titulo,"Fixture title %d",i);
    strcpy(items[i].tipo,"series"); strcpy(items[i].genero,"Drama"); strcpy(items[i].meta,"2026 · 48 min");
    strcpy(items[i].sinopse,"A family explores a changing world. Every choice reveals another story, another challenge, and another reason to return home.");
    snprintf(items[i].poster,sizeof items[i].poster,"deploy/app/art/poster/%02d.jpg",i);
    snprintf(items[i].backdrop,sizeof items[i].backdrop,"deploy/app/art/%02d.jpg",i);
    items[i].temporadas[0]=1; items[i].nTemporadas=1;
    items[i].nota=80; items[i].nElenco=2; strcpy(items[i].elenco[0].nome,"Alex Smith"); strcpy(items[i].elenco[1].nome,"Jordan Lee");
  }
  strcpy(row.chave,"fixture"); strcpy(row.titulo,"Fixture first row"); strcpy(row.tipo,"series");
  strcpy(row.base,"https://addon.invalid/x"); strcpy(row.catId,"fixture"); row.n=6;
  cat_definir_tudo(items,6,&row,1);
  for(int i=0;i<3;i++) {
    eps[i].temporada=1; eps[i].episodio=i+1;
    snprintf(eps[i].nome,sizeof eps[i].nome,"Episode %d",i+1);
    strcpy(eps[i].sinopse,"A long episode synopsis exercises every reserved line without crossing into the following section. The story continues with another unexpected turn.");
    strcpy(eps[i].duracao,"48 min");
    if(getenv("NV_ANCHOR_UPCOMING")) strcpy(eps[i].data,"03/15/2027");
  }
  cat_definir_episodios(0,eps,3);
  int upcoming=getenv("NV_ANCHOR_UPCOMING")!=NULL;
  for(int layout=0;layout<3;layout++) {
    if(upcoming && layout!=HOME_LAYOUT_DINAMICA) continue;
    char path[1024]; snprintf(path,sizeof path,"%s/ajustes.txt",dir);
    FILE *f=fopen(path,"w"); assert(f);
    fprintf(f,"homeLayoutLocal %d\ncollapseSidebar 1\nmodernSidebar 0\ntamanhoAjustesLocal 2\n",layout); fclose(f);
    ajustes_dir(dir); ajustes_iniciar(); assert(home_iniciar("deploy/app/art"));
    AddonRemoto addon={.nome="Fixture",.url="http://127.0.0.1:1/manifest.json",.ativo=1};
    addons_definir_lista(&addon,1); assert(addons_n()==1);
    cat_definir_tudo(items,6,&row,1); cat_definir_episodios(0,eps,3);
    float scales[]={1,.8f,.85f,.9f,1.15f,1.3f};
    for(int i=0;i<(int)(sizeof scales/sizeof scales[0]);i++) {
      if(upcoming && scales[i]!=1 && scales[i]!=1.3f) continue;
      if(quick && scales[i]!=1 && scales[i]!=.85f && scales[i]!=1.3f) continue;
      gfx_escala_txt_definir(scales[i]);
      if(upcoming) { fixture_detail(scales[i],1); continue; }
      if(quick) {
        fixture_home(scales[i],0); fixture_home(scales[i],1);
        if(layout==HOME_LAYOUT_PADRAO) fixture_detail(scales[i],1);
        continue;
      }
      if(!focused) { fixture_home(scales[i],0); fixture_home(scales[i],1); }
      fixture_search(scales[i]);
      for(int section=-1;section<4;section++)
        if(!focused || section==1) fixture_detail(scales[i],section);
      if(focused) continue;
      if(layout==0 && scales[i]!=.9f && scales[i]!=1.3f) { fixture_settings(scales[i],0); fixture_settings(scales[i],1); }
      gfx_escala_txt_definir(scales[i]);
      shot_begin(layout==0?"clock-modern":layout==1?"clock-standard":"clock-dynamic",scales[i]);
      ilha_relogio_visivel(1);
      for(int frame=0;frame<100;frame++) { shot_reset(); ilha_posicionar(1); ilha_desenhar(10000+frame*16); }
      GfxRect clock; shot_check("clock visible",ilha_rect(&clock.x,&clock.y,&clock.w,&clock.h));
      float old=gfx_escala_entrar(); clock=shot_box(clock); gfx_escala_sair(old);
      shot_anchor("clock right/top",clock,2);
      shot_contains("clock text inside pill",clock,shot_text_bounds());
      shot_contains("clock inside screen",(GfxRect){0,0,1920,1080},clock);
      shot_end();
    }
  }
  printf("Rendered anchor failures: %d\n",failures);
  return failures?1:0;
}
