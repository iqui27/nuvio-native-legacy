// Reuse the offline series fixture and real GL renderer.
#include "tex_cache.h"
#include "gfx.h"
GLuint fx_desfocado(GLuint src, const char *chave);
static int checkThumbnailWidth, thumbnailRequests;
static GLuint checkedThumbnail(const char *path, float width);
#include "sinopse.h"
static int checkPanel, synopsisCalls, metaCalls;
static GfxRect panelCard;
static char panelMeta[512];
static float checkedSynopsis(TxtEstilo style, const char *s, int r, int g, int b,
                             float x, float y, float w, float leading, float a, int lines);
static float checkedMeta(TxtEstilo style, const char *s, int r, int g, int b,
                         float x, float y, float a, float tracking);
static int checkP1, cardDraws, plateDraws, traktDraws;
static GfxRect drawnArt;
static float drawnFocus;
static void checkedRect(GfxRect r, GLuint tex, GfxModo mode, float focus,
                        float px, float py, float radius, float cr, float cg, float cb, float a);
static void checkedColor(GfxRect r, float radius, float cr, float cg, float cb, float a);
static float checkedBrand(int source, float x, float cy, float h, float a);
#define gfx_rect checkedRect
#define gfx_cor checkedColor
#define notasui_marca checkedBrand
#define sinopse_bloco_corta checkedSynopsis
#define txt_tracking checkedMeta
#define tex_obter_larg checkedThumbnail
#define main episode_fixture_main
#include "detail_eps_shot.c"
#undef main
#undef tex_obter_larg
#undef sinopse_bloco_corta
#undef txt_tracking
#undef gfx_rect
#undef gfx_cor
#undef notasui_marca
float notasui_marca(int source, float x, float cy, float h, float a);

static void checkedRect(GfxRect r, GLuint tex, GfxModo mode, float focus,
                        float px, float py, float radius, float cr, float cg, float cb, float a) {
  if (checkP1 && mode == GFX_CARD && tex) {
    drawnArt = r; drawnFocus = focus; cardDraws++;
  }
  gfx_rect(r,tex,mode,focus,px,py,radius,cr,cg,cb,a);
}
static void checkedColor(GfxRect r, float radius, float cr, float cg, float cb, float a) {
  if (checkP1 && !epModerna() && cr == 1 && cg == 1 && cb == 1 &&
      r.w == panelCard.w && r.h > epThumbH()) {
    assert(r.x == panelCard.x && r.y == panelCard.y + epThumbH() + 10);
    assert(r.h == epCardH() - epThumbH() - 18);
    assert(fabsf(a - .12f) < .001f);
    plateDraws++;
  }
  gfx_cor(r,radius,cr,cg,cb,a);
}
static float checkedBrand(int source, float x, float cy, float h, float a) {
  if (checkP1 && epModerna() && source == EX_TRAKT && a > 0) {
    assert(x >= panelCard.x + 24 && x < panelCard.x + panelCard.w - 24);
    assert(cy >= panelCard.y + epThumbH() + NV_DETP_EPM_TIT_DY);
    assert(cy <= panelCard.y + epThumbH() + NV_DETP_EPM_TIT_DY + 36);
    traktDraws++;
  }
  return notasui_marca(source,x,cy,h,a);
}

static void saveShot(const char *path, int w, int h);
static void checkReviewP1(const char *out, const char *finding) {
  const int standard = !strcmp(finding,"standard-focus");
  dados_gravar("aparencia-20.txt","1\n");
  for (int layout=0;layout<3;layout++) {
    if ((layout == HOME_LAYOUT_PADRAO) != standard) continue;
    for (int glass=0;glass<(standard ? 2 : 1);glass++) {
      for (int ring=0;ring<(standard ? 2 : 1);ring++) {
        char path[900]; snprintf(path,sizeof path,"%s/ajustes.txt",dados_dir());
        FILE *file=fopen(path,"w"); assert(file);
        // Saved values 0/1 mean enabled/disabled for these toggles.
        fprintf(file,"performanceAutoVersaoLocal 1\nidioma 1\nhomeLayoutLocal %d\nbordaFocoCartaz %d\nvidroLocal %d\nnotaEpisodio 0\nselected_theme 2\nescalaTextoVersaoLocal 1\nescalaTextoLocal 2\n",layout,ring?0:1,glass?0:1);
        fclose(file); ajustes_dir(dados_dir());
        assert(ajustes_home_layout()==layout && ajustes_vidro()==glass && ajustes_borda_foco()==ring);
        assert(ajustes_nota_episodio(EX_TRAKT));
        montarCatalogo(); semear(1,T1_N,3); abrir(0,0); nosEpisodios(3);
        parado=1; quadros(32);
        panelCard=(GfxRect){100,100,epCardW(),epCardH()};
        cardDraws=plateDraws=traktDraws=0; checkP1=1;
        txt_novo_quadro(); desenhaEpisodio(panelCard,3,1,1,10000);
        checkP1=0;
        if (standard) {
          assert(cardDraws==1 && drawnFocus==1 && plateDraws==1);
          GfxRect expected={100,100,420,236};
          assert(drawnArt.x==expected.x && drawnArt.y==expected.y &&
                 drawnArt.w==expected.w && drawnArt.h==expected.h);
        } else {
          assert(traktDraws==1 && cardDraws==1 && drawnFocus==1 && plateDraws==0);
          assert(drawnArt.x==100 && drawnArt.y==100 && drawnArt.w==panelCard.w);
          int option=ajustes_rapido_op("notaTituloTrakt");
          assert(option>=0 && ajustes_rapido_passo(option,1));
          traktDraws=0; checkP1=1;
          txt_novo_quadro(); desenhaEpisodio(panelCard,3,1,1,10000);
          checkP1=0; assert(traktDraws==0);
          assert(ajustes_rapido_passo(option,1));
        }
        gfx_novo_quadro(); txt_novo_quadro(); gfx_sem_recorte();
        glClearColor(.02f,.02f,.025f,1); glClear(GL_COLOR_BUFFER_BIT);
        detail_desenhar(10000);
        snprintf(path,sizeof path,"%s/p1-%s-1080p-dark%s.png",out,
                 standard?"standard-focused":layout?"refined-trakt":"modern-trakt",
                 standard ? (glass ? (ring ? "-glass-ring":"-glass-zoom") : (ring ? "-ring":"-zoom")) : "");
        saveShot(path,1920,1080);
      }
    }
  }
  printf("PASS: %s\n",finding);
}

static float checkedSynopsis(TxtEstilo style, const char *s, int r, int g, int b,
                             float x, float y, float w, float leading, float a, int lines) {
  if (checkPanel) {
    assert(x == panelCard.x + 24 && y == panelCard.y + epThumbH() + 104);
    assert(lines == 5 && y + lines * leading <= panelCard.y + panelCard.h);
    synopsisCalls++;
  }
  return sinopse_bloco_corta(style,s,r,g,b,x,y,w,leading,a,lines);
}
static float checkedMeta(TxtEstilo style, const char *s, int r, int g, int b,
                         float x, float y, float a, float tracking) {
  if (checkPanel) {
    assert(y == panelCard.y + epThumbH() + 62 && tracking == 1);
    assert(r == 169 && g == 171 && b == 178);
    snprintf(panelMeta,sizeof panelMeta,"%s",s); metaCalls++;
  }
  return txt_tracking(style,s,r,g,b,x,y,a,tracking);
}

static GLuint checkedThumbnail(const char *path, float width) {
  if (checkThumbnailWidth) {
    assert(width == epCardW());
    thumbnailRequests++;
  }
  return tex_obter_larg(path, width);
}

static void checkThumbnailHoles(void) {
  assert(!ajustes_borda_foco() && !ajustes_vidro());
  assert(animFoco[SEC_EPISODIOS][foco.coluna] > .999f);
  float settled = animFoco[SEC_EPISODIOS][foco.coluna];
  const float focus[] = {0, .5f, 1};
  for (int f=0;f<3;f++) {
    animFoco[SEC_EPISODIOS][foco.coluna] = focus[f];
    GfxRect holes[N_ITENS * 2];
    thumbnailRequests=0; checkThumbnailWidth=1;
    int n=epFuros(holes), expected=0;
    checkThumbnailWidth=0;
    assert(n > 0 && n == thumbnailRequests * 2);
    for (int c=0;c<epVisiveis() && c<N_ITENS && c<DET_REV_EP;c++) {
      float x=xItem(SEC_EPISODIOS,c)-scrollSec[SEC_EPISODIOS];
      if (x>NV_TELA_W || x+epCardW()<0) continue;
      float ft=anim_clamp(animFoco[SEC_EPISODIOS][c],0,1), scale=1+.035f*ft;
      float y=epRowY()-scrollY-epCardH()*(scale-1)*.5f-8*ft;
      x-=epCardW()*(scale-1)*.5f;
      float inset=(NV_DETP_EP_RAIO+2)*scale;
      GfxRect rects[] = {{x+inset,y+2,epCardW()*scale-2*inset,epThumbH()*scale-4},
                         {x+2,y+inset,epCardW()*scale-4,epThumbH()*scale-2*inset}};
      for (int i=0;i<2;i++) {
        assert(expected < n);
        GfxRect h=holes[expected++], r=rects[i];
        assert(fabsf(h.x-r.x)<.001f && fabsf(h.y-r.y)<.001f &&
               fabsf(h.w-r.w)<.001f && fabsf(h.h-r.h)<.001f);
      }
    }
    assert(expected == n);
  }
  animFoco[SEC_EPISODIOS][foco.coluna] = settled;
  puts("PASS: thumbnail holes and unchanged image cache widths across focus changes");
}

static void saveShot(const char *path, int w, int h) {
  unsigned char *pixels = malloc((size_t)w * h * 4);
  SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_RGBA32);
  assert(pixels && surface);
  glFinish();
  glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  for (int y=0;y<h;y++)
    memcpy((char *)surface->pixels+y*surface->pitch,pixels+(h-1-y)*w*4,w*4);
  assert(IMG_SavePNG(surface,path)==0);
  SDL_FreeSurface(surface); free(pixels);
  printf("PNG %s\n",path);
}

int main(int argc, char **argv) {
  assert(argc == 3);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP","1");
  assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0);
  IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  janela=SDL_CreateWindow("Episode information",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  assert(janela);
  assert(SDL_GL_CreateContext(janela));
  glGenTextures(1,&fboTex); glBindTexture(GL_TEXTURE_2D,fboTex);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1920,1080,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_FRAMEBUFFER,fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,fboTex,0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
  gfx_tamanho_alvo(1920,1080);
  assert(gfx_iniciar()); assert(txt_iniciar("deploy/app",1));
  tex_iniciar(16); gfx_icones_dir("deploy/app/art");
  ajustes_iniciar(); dados_iniciar("deploy/app/art");
  extras_carregar("deploy/app/art");

  if (!strcmp(argv[2],"standard-focus") || !strcmp(argv[2],"trakt-ratings")) {
    checkReviewP1(argv[1],argv[2]);
    return 0;
  }

  // Preserve image detail while exercising both luminance extremes.
  for (int light=0;light<2;light++) {
    char art[700];
    snprintf(art,sizeof art,"%s/%s.png",dados_dir(),light?"light":"dark");
    SDL_Surface *source=IMG_Load("deploy/app/art/03.jpg"); assert(source);
    SDL_Surface *image=SDL_ConvertSurfaceFormat(source,SDL_PIXELFORMAT_RGBA32,0); assert(image);
    SDL_FreeSurface(source);
    for(int y=0;y<image->h;y++) for(int x=0;x<image->w;x++) {
      unsigned char *p=(unsigned char *)image->pixels+y*image->pitch+x*4;
      for(int c=0;c<3;c++) p[c]=light ? 190+p[c]/4 : p[c]/3;
    }
    assert(IMG_SavePNG(image,art)==0); SDL_FreeSurface(image);
    for(int layout=0;layout<3;layout+=2) {
      char settings[700]; snprintf(settings,sizeof settings,"%s/ajustes.txt",dados_dir());
      FILE *file=fopen(settings,"w"); assert(file);
      fprintf(file,"performanceAutoVersaoLocal 1\nidioma 1\nhomeLayoutLocal %d\nbordaFocoCartaz 1\nvidroLocal 1\nnotaEpisodio 3\nselected_theme %d\nshowFullReleaseDate 0\nescalaTextoVersaoLocal 1\nescalaTextoLocal 2\n",layout,light?0:2); fclose(file);
      ajustes_dir(dados_dir());
      montarCatalogo();
      snprintf(itens[0].backdrop,sizeof itens[0].backdrop,"%s",art);
      itens[0].temporada=1; itens[0].episodio=4; itens[0].progresso=45;
      cat_definir_tudo(itens,2,&filEnsaio,1);
      for(int i=0;i<T1_N;i++) {
        snprintf(episodios[i].thumb,sizeof episodios[i].thumb,"%s",art);
        snprintf(episodios[i].sinopse,sizeof episodios[i].sinopse,
          "Sinopse de enchimento do episódio, comprida o bastante para ocupar as linhas que o card agora reserva e mostrar onde o bloco corta. Com mais espaço embaixo, a pessoa lê quase tudo sem abrir o episódio, e o texto respira melhor na TV. Mais detalhes continuam aqui para comprovar as reticências na quinta linha, sem invadir a base do cartão.");
      }
      cat_definir_episodios(0,episodios,T1_N+T2_N+T3_N);
      semear(1,T1_N,3); abrir(0,0); nosEpisodios(3);
      parado=1; quadros(32);
      checkThumbnailHoles();
      panelCard = (GfxRect){100,100,epCardW(),epCardH()};
      assert(panelCard.w == 640 && epThumbH() == 360 && panelCard.h == 624);
      assert(fabsf(epRowY()-scrollY + epCardH()) < 1080);
      CatEp *sample = &episodios[3];
      CatEp saved = *sample;
      for (int missing=0;missing<4;missing++) {
        *sample = saved;
        if (missing & 1) sample->data[0]=0;
        if (missing & 2) sample->duracao[0]=0;
        sample->notaImdb = sample->nota = missing ? 0 : 85;
        cat_definir_episodios(0,episodios,T1_N+T2_N+T3_N);
        synopsisCalls=metaCalls=0; checkPanel=1;
        txt_novo_quadro(); desenhaEpisodio(panelCard,3,1,1,10000);
        checkPanel=0;
        assert(synopsisCalls == 1 && metaCalls == 1);
        assert((strstr(panelMeta,"·") != NULL) == (missing == 0));
        if (missing == 3) assert(!panelMeta[0]);
      }
      *sample = saved; cat_definir_episodios(0,episodios,T1_N+T2_N+T3_N);
      // Warm caches, then compare actual focus frames, including text and still.
      unsigned char *rest=malloc(1920*1080*4), *focused=malloc(1920*1080*4);
      assert(rest && focused);
      for (int pass=0;pass<4;pass++) {
        gfx_novo_quadro(); txt_novo_quadro(); gfx_sem_recorte();
        glClearColor(.02f,.02f,.025f,1); glClear(GL_COLOR_BUFFER_BIT);
        desenhaEpisodio(panelCard,3,pass&1,1,10000);
        glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,pass&1?focused:rest);
      }
      int changes=0;
      for(int y=0;y<1080;y++) for(int x=0;x<1920;x++) {
        int p=((1079-y)*1920+x)*4;
        int delta=0;
        for (int channel=0;channel<4;channel++)
          if (abs(rest[p+channel]-focused[p+channel])>delta)
            delta=abs(rest[p+channel]-focused[p+channel]);
        if (delta<=2) continue;
        // Focus now lifts/scales the whole card and sweeps light over its image.
        assert(x>=88 && x<752 && y>=81 && y<728);
        changes++;
      }
      assert(changes>1000); free(rest); free(focused);
      puts("PASS: missing metadata, five synopsis lines, card lift/sheen pixels bounded");
      for(int h=1080;h>=720;h-=360) {
        int w=h*16/9;
        gfx_tamanho_alvo(w,h); glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glViewport(0,0,w,h); gfx_novo_quadro(); txt_novo_quadro();
        gfx_sem_recorte(); glClearColor(.02f,.02f,.025f,1); glClear(GL_COLOR_BUFFER_BIT);
        detail_desenhar(10000);
        char path[900];
        snprintf(path,sizeof path,"%s/%s-%s-%dp-%s.png",argv[1],argv[2],
                 layout?"refined":"modern",h,light?"light":"dark");
        saveShot(path,w,h);
      }
      gfx_tamanho_alvo(1920,1080);
    }
  }
  return 0;
}
