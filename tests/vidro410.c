// #410: pixels reais, sem janela, rede ou TV. Inclui Ajustes para escolher os
// mesmos valores e executar as previas/linhas reais, sem costura no release.
#ifdef NV410_TPK
#define NV_TPK 1
#endif
#include "../src/ajustes.c"
#include <OpenGL/OpenGL.h>
#include <assert.h>
#include <math.h>

static int falhas;
static GLuint alvo, textura;
static void confere(int ok, const char *nome) {
  printf("%s: %s\n", ok ? "PASSA" : "FALHA", nome);
  falhas += !ok;
}
static void quadro(void) {
  glBindFramebuffer(GL_FRAMEBUFFER, alvo);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  gfx_escala_sair(1);
  gfx_definir_efeitos_leves(0); gfx_definir_efeitos_minimos(0);
  gfx_modos_desligados = 0;
  glDisable(GL_SCISSOR_TEST);
  glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
  ajMovReiniciar();
  memset(&C, 0, sizeof C); C.s = C.a = 1; C.ar = C.ag = C.ab = 1; C.red = 1;
}
static double media(int x, int y, int w, int h) {
  unsigned char *p = malloc((size_t)w*h*4);
  double soma = 0;
  assert(p);
  glReadPixels(x, 1080-y-h, w, h, GL_RGBA, GL_UNSIGNED_BYTE, p);
  for (int i=0;i<w*h;i++) soma += p[4*i]+p[4*i+1]+p[4*i+2];
  free(p); return soma/(w*h*3);
}
static double cartaz(int brilho, int prof, int leves, float foco) {
  quadro(); valor[AJ_PROF_BRILHO]=brilho; valor[AJ_PROF]=prof ? 0 : 1;
  gfx_definir_efeitos_leves(leves);
  gfx_borda_foco_atual=0;
  gfx_rect((GfxRect){100,100,200,300}, textura, GFX_CARD, foco,0,0,0,1,1,1,1);
  return media(170,220,60,60);
}
static void contexto(void) {
  CGLPixelFormatAttribute a[]={kCGLPFAAllowOfflineRenderers,0};
  CGLPixelFormatObj f; CGLContextObj c; GLint n;
  assert(CGLChoosePixelFormat(a,&f,&n)==kCGLNoError && n);
  assert(CGLCreateContext(f,NULL,&c)==kCGLNoError);
  CGLDestroyPixelFormat(f); assert(CGLSetCurrentContext(c)==kCGLNoError);
  printf("GL: %s | Ajustes: %s\n", glGetString(GL_RENDERER),
#ifdef NV_TPK
         "NV_TPK"
#else
         "host"
#endif
  );
  assert(gfx_iniciar());
  GLuint t;
  glGenTextures(1,&t); glBindTexture(GL_TEXTURE_2D,t);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1920,1080,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glGenFramebuffers(1,&alvo); glBindFramebuffer(GL_FRAMEBUFFER,alvo);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,t,0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
  unsigned char cinza[]={128,128,128,255};
  glGenTextures(1,&textura); glBindTexture(GL_TEXTURE_2D,textura);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,cinza);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  gfx_tex_esquecer(0);
}
int main(void) {
  memcpy(valor,valorPadrao,sizeof valor);
  contexto();
  // O cartaz em foco conserva a arte a 0%; 50 e 100% aumentam apenas o reflexo.
  double zero=cartaz(0,1,0,1), meio=cartaz(50,1,0,1), cheio=cartaz(100,1,0,1);
  printf("cartaz RGB medio: 0=%.2f 50=%.2f 100=%.2f\n",zero,meio,cheio);
  confere(fabs(zero-128)<1,"Reflexo 0% preserva RGB do poster em foco");
  confere(meio>zero+2 && cheio>meio+2,"Reflexo 50/100% chega ao shader");
  confere(fabs(cartaz(100,0,0,1)-128)<1,"Profundidade desligada zera o reflexo");
  confere(fabs(cartaz(0,1,1,1)-128)<1,"Reflexo 0% tambem nos efeitos leves");
  confere(fabs(cartaz(0,1,0,0)-cartaz(100,1,0,0))<1,"Reflexo nao muda o cartaz sem foco");
  // A linha dos Ajustes precisa responder ao contorno mesmo nos niveis leves.
  for(int leve=0;leve<2;leve++) {
    double m[2];
    for(int on=0;on<2;on++) {
      quadro(); valor[AJ_VIDRO]=0; valor[AJ_VIDRO_CONTORNO]=on ? 0 : 1;
      gfx_definir_efeitos_leves(leve);
      ajFocoLinha((GfxRect){100,100,400,80},16,1,1);
      m[on]=media(150,100,300,3);
    }
    confere(m[1]>m[0]+20,leve ? "Contorno chega a linha com efeitos leves" : "Contorno chega a linha dos Ajustes");
  }
  // Comparacao sem/com da previa: zero deve produzir o MESMO cartaz dos dois lados.
  quadro(); valor[AJ_PROF_BRILHO]=0;
  assert(ajcNovaCartazes(AJS_CARTAZES,AJ_PROF_BRILHO));
  double esq=media(90,65,150,170),dir=media(400,65,150,170);
  printf("previa Reflexo 0%%: esquerda=%.2f direita=%.2f\n",esq,dir);
  confere(fabs(esq-dir)<1,"Previa de Reflexo sem luz residual a 0%");
  quadro(); valor[AJ_PROF_BRILHO]=100;
  ajcNovaCartazes(AJS_CARTAZES,AJ_PROF_BRILHO);
  confere(media(400,65,150,170)>media(90,65,150,170)+2,"Previa de Reflexo responde a 100%");
  // Borda a zero: mesmo pixel que o cartaz e sombra, sem realce branco fixo.
  quadro(); czSombra(220,40,200,230,12,0.7f); ajc_cartaz(220,40,200,230,2,1);
  double sem=media(240,40,160,2);
  quadro(); valor[AJ_PROF_BORDA]=0; ajcNovaCartazes(AJS_CARTAZES,AJ_PROF_BORDA);
  confere(fabs(media(240,40,160,2)-sem)<1,"Previa de borda sem luz residual a 0%");
  quadro(); valor[AJ_PROF_BORDA]=100; ajcNovaCartazes(AJS_CARTAZES,AJ_PROF_BORDA);
  confere(media(240,40,160,2)>sem+20,"Previa de borda responde a 100%");
  // Caracterizacao: a escala dos Ajustes nao troca o alvo/FBO de 1080p.
  for(int i=0;i<3;i++) {
    quadro(); valor[AJ_TAMANHO_AJUSTES]=i;
    GLint antes,depois,vp[4]; glGetIntegerv(GL_FRAMEBUFFER_BINDING,&antes);
    { AJ_ESCALA_INI(); ajFocoLinha((GfxRect){100,100,400,80},16,1,1); AJ_ESCALA_FIM(); }
    glGetIntegerv(GL_FRAMEBUFFER_BINDING,&depois); glGetIntegerv(GL_VIEWPORT,vp);
    confere(antes==depois && vp[2]==1920 && vp[3]==1080 && gfx_escala()==1,
            "Tamanho 80/90/100% preserva alvo 1080p e restaura escala");
  }
  assert(glGetError()==GL_NO_ERROR);
  printf("vidro410: %d falha(s)\n",falhas);
  return falhas ? 1 : 0;
}
