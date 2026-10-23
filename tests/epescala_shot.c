// Real episode card, action menu, ratings and clock; offline scale matrix.
#define main epescala_unused_main
#include "detail_eps_shot.c"
#undef main
#include "epcoment.h"
#include "epescala_fixture.h"
#include "extras.h"

void epescala_request(const char *id, int t, int e) { (void)id; (void)t; (void)e; }
int epescala_matches(const char *id, int t, int e) { (void)id; (void)t; (void)e; return 1; }
int epescala_count(void) { return 8; }
int epescala_loading(void) { return 0; }
const char *epescala_user(int i) { return i ? "anaa_mgs" : "JasperKazai"; }
const char *epescala_text(int i) { (void)i; return "A long road ahead. The group leaves Atlanta and encounters a new threat along the way. An episode worth remembering."; }
const char *epescala_date(int i) { (void)i; return "2022-02-23T12:00:00Z"; }
int epescala_likes(int i) { (void)i; return 8; }
int epescala_score(int i) { (void)i; return 8; }
int epescala_replies(int i) { (void)i; return 1; }
int epescala_spoiler(int i) { (void)i; return 1; }
void epescala_ratings(ExEpNotas *n) {
  memset(n, 0, sizeof *n); n->nota=80; n->votos=100;
  for (int i=1;i<=10;i++) n->distribuicao[i]=i*2;
}

time_t epescala_time(time_t *out) { time_t t=1760000000; if (out) *out=t; return t; }
static int probe, failures, rects, fbw, fbh;
static float ui, textScale;
// Called after production group transforms and layer scaling, before clipping.
void epescala_probe(GfxRect r, GfxModo mode, float alpha, int clipped, GfxRect clip) {
  if (!probe || alpha < .01f || mode==GFX_SOMBRA || mode==GFX_LUZ) return;
  if (clipped) {
    float right=fminf(r.x+r.w,clip.x+clip.w), bottom=fminf(r.y+r.h,clip.y+clip.h);
    r.x=fmaxf(r.x,clip.x); r.y=fmaxf(r.y,clip.y);
    r.w=right-r.x; r.h=bottom-r.y;
    if (r.w<=0 || r.h<=0) return;
  }
  // The full-screen modal veil is intentionally edge-to-edge.
  if (r.x==0 && r.y==0 && r.w>=1919 && r.h>=1079) return;
  rects++;
  if (r.w<0 || r.h<0 || r.x<23 || r.y<23 || r.x+r.w>1897 || r.y+r.h>1057) {
    if (failures<25) printf("FAIL %dx%d ui=%.2f text=%.2f mode=%d rect=%.1f,%.1f %.1fx%.1f\n",fbw,fbh,ui,textScale,mode,r.x,r.y,r.w,r.h);
    failures++;
  }
}
static void save(const char *dir, const char *phase) {
  char path[1024];
  snprintf(path,sizeof path,"%s/%s-%dx%d-ui%.2f-text%.2f.png",dir,phase,fbw,fbh,ui,textScale);
  unsigned char *pixels=malloc(fbw*fbh*4); assert(pixels);
  SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,fbw,fbh,32,SDL_PIXELFORMAT_RGBA32); assert(s);
  glFinish(); glReadPixels(0,0,fbw,fbh,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  for (int y=0;y<fbh;y++) memcpy((char *)s->pixels+y*s->pitch,pixels+(fbh-1-y)*fbw*4,fbw*4);
  assert(!IMG_SavePNG(s,path)); SDL_FreeSurface(s); free(pixels);
}
int main(int argc, char **argv) {
  assert(argc==3);
  char path[1024]; snprintf(path,sizeof path,"%s/ajustes.txt",getenv("NUVIO_DADOS"));
  FILE *f=fopen(path,"w"); assert(f);
  fprintf(f,"idioma 0\nhomeLayoutLocal 0\nvidroLocal 1\nselected_theme 2\nescalaTextoVersaoLocal 1\nescalaTextoLocal 2\n"); fclose(f);
  ajustes_dir(getenv("NUVIO_DADOS"));
  assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)); IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  janela=SDL_CreateWindow("episode scale proof",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN); assert(janela);
  SDL_GLContext gl=SDL_GL_CreateContext(janela); assert(gl);
  assert(gfx_iniciar()); assert(txt_iniciar("deploy/app",1)); tex_iniciar(64); gfx_icones_dir("deploy/app/art");
  dados_iniciar("deploy/app/art"); extras_carregar("deploy/app/art");
  montarCatalogo(); semear(1,T1_N,T1_N);
  for (int i=0;i<T1_N;i++) {
    CatEp *ep=&episodios[i];
    snprintf(ep->nome,sizeof ep->nome,"O que vem pela frente");
    snprintf(ep->thumb,sizeof ep->thumb,"deploy/app/art/03.jpg");
    ep->notaImdb=83; ep->nota=83;
  }
  cat_definir_episodios(0,episodios,T1_N+T2_N+T3_N);
  IlhaCartao c={0}; snprintf(c.chave,sizeof c.chave,"scale-proof"); snprintf(c.titulo,sizeof c.titulo,"The Walking Dead"); c.progresso=.3f;
  ilha_cartao(ILHA_VIVO,&c); ilha_relogio_visivel(1);
  const float interfaces[]={1,1.2f,1.3f,1.5f,.8f,.9f};
  const float texts[]={1,1.2f,1.3f,1.5f,.85f,.9f,1.1f};
  glGenFramebuffers(1,&fbo); glGenTextures(1,&fboTex);
  for (int resolution=0;resolution<2;resolution++) {
    fbw=resolution?1280:1920; fbh=resolution?720:1080;
    glBindTexture(GL_TEXTURE_2D,fboTex); glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,fbw,fbh,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER,fbo); glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,fboTex,0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    gfx_tamanho_alvo(fbw,fbh); glViewport(0,0,fbw,fbh);
    for (unsigned a=0;a<sizeof interfaces/sizeof *interfaces;a++) for (unsigned b=0;b<sizeof texts/sizeof *texts;b++) {
      int failuresBefore=failures;
      ui=interfaces[a]; textScale=texts[b]; gfx_escala_ui_definir(ui); gfx_escala_txt_definir(textScale);
      float previous=gfx_escala_txt_entrar();
      episodios_menu_visto(0,1,7,"O que vem pela frente");
      gfx_escala_sair(previous);
      foco.fileira=SEC_EPISODIOS; foco.coluna=6; idx=0;
      for (int frame=0;frame<90;frame++) {
        txt_novo_quadro(); tex_novo_quadro(); tex_bombear(8); episodios_menu_atualizar(1.f/60);
        gfx_novo_quadro(); gfx_sem_recorte(); glClearColor(.025f,.025f,.03f,1); glClear(GL_COLOR_BUFFER_BIT);
        float old=gfx_escala_txt_entrar();
        menuOrigem=(GfxRect){100/textScale,310/textScale,epCardW(),epCardH()}; menuOrigemCol=6; menuOrigemF=1; menuOrigemA=1;
        probe=frame==89; rects=0;
        menuDeVisto(10000+frame*16); gfx_escala_sair(old);
        SDL_Delay(3);
        ilha_posicionar(1); ilha_desenhar(10000+frame*16);
        probe=0;
        if (frame==89) { assert(rects>20); save(argv[1],argv[2]); }
      }
      printf("%s combo %dx%d ui=%.2f text=%.2f violations=%d\n",failures==failuresBefore?"PASS":"FAIL",fbw,fbh,ui,textScale,failures-failuresBefore);
      SDL_Event ev={0}; ev.type=SDL_KEYDOWN; ev.key.keysym.sym=SDLK_ESCAPE; episodios_menu_evento(&ev);
    }
  }
  printf("%s: 84 framebuffer/scale combinations; %d out-of-safe-area rectangles\n",failures?"FAIL":"PASS",failures);
  tex_encerrar(); txt_encerrar(); gfx_encerrar(); SDL_GL_DeleteContext(gl); SDL_DestroyWindow(janela); SDL_Quit();
  return failures?1:0;
}
