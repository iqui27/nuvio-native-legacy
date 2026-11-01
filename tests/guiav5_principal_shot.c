/* GUIA TV v5 — captura da TELA PRINCIPAL do guia (grade, cartoes, esportes,
 * folhas e estados de erro/vazio), em pt-BR, lado a lado com os PNGs do mock
 * aprovado (tests/guiav5_compare.py monta o <estado>-compare.png).
 *
 * Inclui src/guiav5ui.c E src/guia.c: os canais e a grade EPG sao estaticos
 * dos dois modulos, e semear por dentro e o unico jeito de fotografar o guia
 * sem addon no ar. Os canais e programas sao inventados; nenhum dado de conta
 * passa por aqui. Nao entra na suite (tools/testa-tudo.sh pula *_shot.sh).
 */
#ifndef GUIA_UNDER_TEST
#define GUIA_UNDER_TEST "../src/guia.c"
#endif
#include GUIA_UNDER_TEST
#ifndef GUIAV5_UNDER_TEST
#define GUIAV5_UNDER_TEST "../src/guiav5ui.c"
#endif
#include GUIAV5_UNDER_TEST
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL_image.h>
#include <sys/stat.h>
#include <assert.h>

static int shot_zap;   /* zapping band (guia.c overlay) instead of the v5 screen */
static void quadro(void) {
  SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro(); g5ui_draw();
}
static void capture(SDL_Window *window, const char *dir, const char *name) {
  char path[1024];
  snprintf(path, sizeof path, "%s/%s-app.png", dir, name);
  for (int frame = 0; frame < 90; frame++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(8);
    gfx_novo_quadro();
    glClearColor(.045f, .047f, .051f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    if (shot_zap) desenharBanda(1, SDL_GetTicks());
    else { if(!strcmp(name,"preview-focus-moving")&&frame==87)gv_event(SDLK_DOWN,0);
      g5ui_draw(); if(stream_folha_canais_aberta()){stream_folha_atualizar(.016f,SDL_GetTicks());stream_folha_desenhar(SDL_GetTicks());} }
    if (frame == 89) {
      unsigned char *pixels = malloc(1920 * 1080 * 4);
      assert(pixels);
      SDL_Surface *surface =
          SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      assert(surface);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
      for (int y = 0; y < 1080; y++)
        memcpy((char *)surface->pixels + y * surface->pitch,
               pixels + (1079 - y) * 1920 * 4, 1920 * 4);
      if (!strcmp(name, "preview-playing") || !strcmp(name,"pins-focused")) {
        /* Host player has no decoder: composite a declared still fixture behind
         * the production video hole, as the TV video plane would be. */
        SDL_Surface *frame = IMG_Load("design/glass-ilha/arte.jpg");
        SDL_Surface *back = SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32);
        float x,y,w,h;guia_preview_rect(&x,&y,&w,&h);
        SDL_Rect rect = {(int)x,(int)y,(int)w,(int)h};
        assert(frame && back);
        SDL_FillRect(back,NULL,SDL_MapRGBA(back->format,0,0,0,255));
        SDL_Rect crop={0,0,frame->w,frame->h};
        if((float)crop.w/crop.h>(float)rect.w/rect.h){crop.w=(int)(crop.h*(float)rect.w/rect.h);crop.x=(frame->w-crop.w)/2;}
        else{crop.h=(int)(crop.w*(float)rect.h/rect.w);crop.y=(frame->h-crop.h)/2;}
        assert(SDL_BlitScaled(frame,&crop,back,&rect)==0);
        SDL_SetSurfaceBlendMode(surface,SDL_BLENDMODE_BLEND);
        assert(SDL_BlitSurface(surface,NULL,back,NULL)==0);
        SDL_FreeSurface(frame);SDL_FreeSurface(surface);surface=back;
      }
      assert(IMG_SavePNG(surface, path) == 0);
      SDL_FreeSurface(surface);
      free(pixels);
    }
    SDL_GL_SwapWindow(window);
    SDL_Delay(8);   /* real time for the springs and entrances to settle */
  }
}

/* One frame + PNG, for the layout-switch morph frames. */
static void capture_now(SDL_Window *window, const char *dir, const char *name) {
  char path[1024];
  snprintf(path, sizeof path, "%s/%s-app.png", dir, name);
  unsigned char *pixels = malloc(1920 * 1080 * 4);
  assert(pixels);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  SDL_Surface *surface =
      SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(surface);
  for (int y = 0; y < 1080; y++)
    memcpy((char *)surface->pixels + y * surface->pitch,
           pixels + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(IMG_SavePNG(surface, path) == 0);
  SDL_FreeSurface(surface);
  free(pixels);
}
/* Two slots live at once: the host has no decoder, so test art stands in for
 * the two video planes (like preview-playing) under the real overlays. */
static void capture_mv_live(SDL_Window *window, const char *dir, const char *name) {
  g5_mv_ctl_set(&mvctl, 0, mv_items[0].id, mv_fonte[0], 0);
  g5_mv_ctl_set(&mvctl, 1, mv_items[1].id, mv_fonte[1], 0);
  g5_mv_ctl_pronto(&mvctl, 0);
  g5_mv_ctl_pronto(&mvctl, 1);
  for (int frame = 0; frame < 90; frame++) { quadro(); SDL_GL_SwapWindow(window); SDL_Delay(8); }
  char path[1024];
  snprintf(path, sizeof path, "%s/%s-app.png", dir, name);
  unsigned char *pixels = malloc(1920 * 1080 * 4);
  assert(pixels);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  SDL_Surface *surface =
      SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(surface);
  for (int y = 0; y < 1080; y++)
    memcpy((char *)surface->pixels + y * surface->pitch,
           pixels + (1079 - y) * 1920 * 4, 1920 * 4);
  SDL_Surface *arts[2] = { IMG_Load("design/glass-ilha/arte.jpg"), IMG_Load("design/glass-ilha/detalhe.jpg") };
  assert(arts[0] && arts[1]);
  SDL_Surface *original = SDL_ConvertSurface(surface, surface->format, 0);
  assert(original);
  for (int s = 0; s < 2; s++) {
    GfxRect r = mv_rect(s);
    SDL_Rect dst = { (int)r.x + 4, (int)r.y + 4, (int)r.w - 8, (int)r.h - 96 };
    SDL_Surface *art = arts[s];
    SDL_Rect crop = { 0, 0, art->w, art->h };
    if ((float)crop.w / crop.h > (float)dst.w / dst.h) { crop.w = (int)(crop.h * (float)dst.w / dst.h); crop.x = (art->w - crop.w) / 2; }
    else { crop.h = (int)(crop.w * (float)dst.h / dst.w); crop.y = (art->h - crop.h) / 2; }
    assert(SDL_BlitScaled(art, &crop, surface, &dst) == 0);
    /* keep the audio badge (main slot, top-right) and the bottom overlay
     * chip from the real draw on top of the stand-in art */
    SDL_Rect topo = { (int)r.x + 2, (int)r.y + 2, (int)r.w - 4, 86 };
    assert(SDL_BlitSurface(original, &topo, surface, &topo) == 0);
  }
  SDL_FreeSurface(original);
  assert(IMG_SavePNG(surface, path) == 0);
  SDL_FreeSurface(arts[0]); SDL_FreeSurface(arts[1]);
  SDL_FreeSurface(surface);
  free(pixels);
  printf("[mv-shot] dois quadros ao vivo compostos: %s\n", path);
}

static void xtv(char *dst, time_t t) {
  struct tm *m = gmtime(&t);
  strftime(dst, 24, "%Y%m%d%H%M%S +0000", m);
}

/* Grade de mentira, relativa a meia hora corrente: a linha "agora" cai sempre
 * no comeco da janela, como na TV. Titulos no molde do mock (times e placar)
 * exercitam o heroi, o placar da previa e os cartoes de esporte. */
static void capture_now(SDL_Window *, const char *, const char *);
static void capture_mv_live(SDL_Window *, const char *, const char *);
static void semear(void) {
  static const struct { const char *canal, *titulo; int ini, fim; } P[] = {
    { "SporTV", "Brasileirão: Flamengo × Palmeiras", -0, 118 },
    { "SporTV", "Resenha esportiva", 118, 178 },
    { "SporTV", "Melhores momentos", -62, -2 },
    { "GloboNews", "Jornal da tarde", -22, 18 },
    { "GloboNews", "Economia em pauta", 18, 78 },
    { "GloboNews", "Em foco", 78, 138 },
    { "Discovery", "Caçadores de mitos", -22, 88 },
    { "Discovery", "Expedição selvagem", 88, 148 },
    { "Telecine Pipoca", "Filme: O resgate", -42, 170 },
    { "Telecine Pipoca", "A última estação", 170, 230 },
    { "Gloob", "Mundo Bita", -2, 8 },
    { "Band", "Flamengo hoje", 98, 158 },
    { "Canal Brasil", "Fla × Flu: Retrospectiva", 200, 260 },
    { "Gloob", "Peppa Pig", 8, 28 },
  };
  char *xml = malloc(24000), ini[24], fim[24];
  size_t n = 0;
  assert(xml);
  n += (size_t)snprintf(xml + n, 24000 - n, "%s", "<?xml version=\"1.0\"?><tv>");
  static const char *CH[] = { "SporTV", "GloboNews", "Discovery", "Telecine Pipoca", "Gloob" };
  for (unsigned k = 0; k < sizeof CH / sizeof *CH; k++)
    n += (size_t)snprintf(xml + n, 24000 - n,
      "<channel id=\"%s\"><display-name>%s</display-name></channel>", CH[k], CH[k]);
  for (unsigned k = 0; k < sizeof P / sizeof *P; k++) {
    xtv(ini, time(NULL) + (time_t)P[k].ini * 60);
    xtv(fim, time(NULL) + (time_t)P[k].fim * 60);
    n += (size_t)snprintf(xml + n, 24000 - n,
      "<programme channel=\"%s\" start=\"%s\" stop=\"%s\"><title>%s</title></programme>",
      P[k].canal, ini, fim, P[k].titulo);
  }
  snprintf(xml + n, 24000 - n, "</tv>");
  assert(epg_xml_processar(xml) > 0);
  free(xml);

  static const struct { const char *nome; int cat; int fav; } C[] = {
    { "SporTV", 0, 1 }, { "GloboNews", 1, 1 }, { "Discovery", 2, 1 },
    { "Telecine Pipoca", 3, 0 }, { "Record", 4, 1 }, { "BBC News", 1, 0 },
    { "CNN Int'l", 1, 0 }, { "Gloob", 5, 0 }, { "Canal Brasil", 3, 0 },
    { "Band", 1, 0 }, { "Flash News 24", 1, 0 }, { "Canal Flamengo TV", 0, 0 },
  };
  static const char *CATS[] = { "Esportes", "Notícias", "Documentários", "Filmes", "Abertos", "Infantil" };
  static const char *IDS[] = { "c1", "c2", "c3", "c4", "c5", "c6", "c7", "c8", "c9", "c10", "c11", "c12" };
  static const int INI[] = { 0, 1, 2, 3, 4, 7 }, NUM[] = { 1, 4, 1, 2, 1, 3 };
  nCats = 6;
  nCanais = (int)(sizeof C / sizeof *C);
  if (!cats) cats = calloc(8, sizeof *cats);
  if (!canais) canais = calloc(16, sizeof *canais);
  if (!catIni) catIni = calloc(8, sizeof *catIni);
  if (!catN) catN = calloc(8, sizeof *catN);
  assert(cats && canais && catIni && catN);
  for (int i = 0; i < nCats; i++) {
    cats[i] = (GCat){ CATS[i], 0, i };
    catIni[i] = INI[i]; catN[i] = NUM[i];
  }
  for (int i = 0; i < nCanais; i++) {
    GCanal *c = &canais[i];
    *c = (GCanal){0};
    c->id = IDS[i];
    c->nome = C[i].nome;
    c->base = "FrostView";
    c->tipo = "channel"; c->logo = c->desc = "";
    c->cat = C[i].cat;
    c->epg = -1;
    c->fav = C[i].fav;
    c->numero = 101 + i;
  }
  nLtvF = 1; ltvF[0].tipo = LTV_M3U; ltvF[0].base = "FrostView";
  snprintf(ltvF[0].chave, sizeof ltvF[0].chave, "fixture");
  estado = G_PRONTO;
  dados_gravar("guia-fav.txt", "c1\nc2\nc3\nc5\n");
}

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : "/Volumes/ExternalSSD/tmp/rev/guiav5-png/principal";
  mkdir(dir, 0755);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *window = SDL_CreateWindow("Guia v5 principal", SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(window);
  assert(SDL_GL_CreateContext(window));
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  dados_iniciar(getenv("NUVIO_DADOS"));
  /* NUVIO_SHOT_ESCALA=85|100 and NUVIO_SHOT_VIDRO=0|1 pick the text scale and
   * material; default stays 100% glass. */
  {
    char aj[160];
    const char *esc = getenv("NUVIO_SHOT_ESCALA");
    snprintf(aj, sizeof aj, "idioma 0\nidiomaAutoLocal 0\nselected_theme 2\nescalaTextoLocal %d\nescalaTextoVersaoLocal 1\n",
             esc && atoi(esc) == 85 ? 0 : 2);
    dados_gravar("ajustes.txt", aj);
  }
  ajustes_dir(getenv("NUVIO_DADOS"));
  gfx_escala_txt_definir(ajustes_escala_texto());
  ajustes_definir_vidro(!getenv("NUVIO_SHOT_VIDRO") || atoi(getenv("NUVIO_SHOT_VIDRO")));
  assert(ajustes_idioma() == 0);
  context();
  semear();
#ifdef NV_SHOT_HOOKS
  if (getenv("NUVIO_SHOT_VARIANTS")) {
    G5Item entries[32];G5Playlist parser;g5_playlist_init(&parser,entries,32);
    FILE *f=fopen("tests/fixtures/channel-variants.m3u","rb");assert(f);
    char bytes[71];size_t n;
    while((n=fread(bytes,1,sizeof bytes,f)))assert(g5_playlist_feed(&parser,bytes,n));
    fclose(f);assert(g5_playlist_finish(&parser)==13);
    stagingZerar();sTeto=30000;
    int source=sFonteChave("fixture",LTV_M3U,"Playlist","");
    for(int i=0;i<parser.count;i++){
      char id[80];snprintf(id,sizeof id,"m3u:fixture:%s",entries[i].id);
      GCru c={.id=id,.nome=entries[i].name,.epgId=entries[i].epg,.grupo="Channels",.url=entries[i].url,.tipo="channel"};
      assert(sCanalAdd(source,&c));
    }
    adotarStaging();estado=G_PRONTO;nFav=0;favAplicar();
    assert(ajustes_livetv_agrupar());
    ajustes_shot_valor("liveTvGroupVariantsLocal",1);guia_reordenar();
    g5ui_open("grade");gv_snap_force=1;gv_snapshot();gv_zone=0;gv_lin=0;
    capture(window,dir,"before");
    ajustes_shot_valor("liveTvGroupVariantsLocal",0);guia_reordenar();
    gv_snap_force=1;gv_snapshot();
    assert(gv_total_n==9);capture(window,dir,"after");
    tex_encerrar();txt_encerrar();gfx_encerrar();SDL_DestroyWindow(window);SDL_Quit();return 0;
  }
#endif
  if (getenv("NUVIO_SHOT_POLIR")) {
    int baseline = getenv("GUIAPOLIR_BASELINE") != NULL;
    g5ui_open("grade"); gv_zone=5; focus=1105; gv_go("ajustes");
    capture(window, dir, "options-first");
    if (!baseline) {
      assert(focus==1940 && gv_gfolha_visto);
      const int down[]={1942,1944,1945,1947,1948,1949,1937,1938,1939};
      for(unsigned i=0;i<sizeof down/sizeof *down;i++) {
        gv_event(SDLK_DOWN,0); assert(focus==down[i]); quadro();
      }
      gv_event(SDLK_DOWN,0); assert(focus==1939);
      gv_event(SDLK_LEFT,0); assert(focus==1939);
      gv_event(SDLK_UP,0); assert(focus==1938);
      gv_event(SDLK_RIGHT,0); assert(focus==1938);
      const int up[]={1937,1949,1948,1947,1945,1944,1942,1940};
      for(unsigned i=0;i<sizeof up/sizeof *up;i++) {
        gv_event(SDLK_UP,0); assert(focus==up[i]); quadro();
      }
      gv_event(SDLK_UP,0); assert(focus==1936);
      gv_event(SDLK_DOWN,0); assert(focus==1940);
      gv_event(SDLK_LEFT,0); assert(focus==1940 && !gv_side_open);
      gv_event(SDLK_ESCAPE,0); assert(!strcmp(screen,"grade") && focus==1105);
      gv_go("ajustes");
#ifndef GUIAPOLIR_BASELINE_CODE
      focus=1944;gv_activate();
      focus=1943;gv_activate();assert(ajustes_guia_previa_destacar());
      focus=1944;gv_activate();assert(!ajustes_guia_previa_destacar());
      gv_voltar();
      /* Preview starts audible since 5b0491ed0; OK mutes, OK again unmutes. */
      assert(!ajustes_guia_previa_sem_som());
      focus=1945;gv_activate();assert(ajustes_guia_previa_sem_som());
      gv_activate();assert(!ajustes_guia_previa_sem_som());
      focus=1947;gv_activate();
      focus=1947;gv_activate();assert(ajustes_guia_dias()==3);
      focus=1946;gv_activate();assert(ajustes_guia_dias()==1);
      gv_voltar();
#endif
    }
    gv_voltar(); gv_zone=5; focus=1105; gv_event(SDLK_RIGHT,0);
    capture(window, dir, "home-multiview");
    if(!baseline) assert(focus==1106);
    g5ui_open("multiview-2"); focus=0;
    capture(window, dir, "multiview-slot");
    if(!baseline) {
      SDL_Event e={.type=SDL_KEYDOWN};
      #define MV_KEY(k) do{e.key.keysym.sym=(k);g5ui_event(&e);}while(0)
      /* 16:9 slots in every mode. */
      for(int m=2;m<=4;m++){G5MvRect rs[4];g5_mv_layout(m,48,MV_AREA_Y,1824,MV_AREA_H,24,rs);
        for(int i=0;i<m;i++){float a=rs[i].w/rs[i].h;assert(a>16.0f/9*.99f&&a<16.0f/9*1.01f);}}
      /* D-pad: Right walks to the second slot; Up reaches the control row. */
      MV_KEY(SDLK_RIGHT);assert(focus==1);
      MV_KEY(SDLK_UP);assert(focus>=800&&focus<=806);
      focus=801;MV_KEY(SDLK_RETURN);assert(mode==3);
      focus=802;MV_KEY(SDLK_RETURN);assert(mode==4);
      /* OK on an empty slot opens the picker sheet (search first). */
      focus=1;MV_KEY(SDLK_RETURN);assert(!strcmp(screen,"multiview-escolher"));
      quadro();assert(focus==899);
      capture(window,dir,"multiview-picker");
      /* First channel row fills the slot; Back closes the sheet. */
      MV_KEY(SDLK_DOWN);assert(focus>900&&!picks[focus-900].header);
      MV_KEY(SDLK_RETURN);assert(!strcmp(screen,"multiview-4")&&mv_items[1].id[0]&&focus==1);
      /* OK on a filled slot opens the slot menu; Remove empties it. */
      focus=1;MV_KEY(SDLK_RETURN);assert(!strcmp(screen,"multiview-slot"));
      focus=853;MV_KEY(SDLK_RETURN);assert(!mv_items[1].id[0]&&!strcmp(screen,"multiview-4"));
      /* Limits modal from the top chip. */
      focus=805;MV_KEY(SDLK_RETURN);assert(!strcmp(screen,"multiview-limite")&&focus==810);
      capture(window,dir,"multiview-limit");
      MV_KEY(SDLK_RETURN);assert(!strcmp(screen,"multiview-4"));
      /* Back to the guide from the top chip. */
      focus=806;MV_KEY(SDLK_RETURN);assert(!strcmp(screen,"grade") && focus==1106);
      #undef MV_KEY
    }
    /* --- Multiview proof PNGs (owner's review packet) --- */
    {
      /* A second, Xtream-keyed source: per-source limit proof in the picker. */
      nLtvF=2;ltvF[1].tipo=LTV_XTREAM;snprintf(ltvF[1].chave,sizeof ltvF[1].chave,"xt");
      snprintf(ltvF[1].nome,sizeof ltvF[1].nome,"Provedor Xtream");ltvF[1].base="Provedor Xtream";
      grupoContas[0]=(IptvConta){.id=0,.tipo=IPTV_XTREAM,.ativa=1};grupoNContas=1;
      struct{const char *id,*nome;} XC[]={{"xtream:501","HBO Max 01"},{"xtream:502","ESPN"}};
      for(int i=0;i<2;i++){GCanal *c=&canais[12+i];*c=(GCanal){0};
        c->id=XC[i].id;c->nome=XC[i].nome;c->base="Provedor Xtream";c->tipo="channel";c->logo=c->desc="";
        c->cat=0;c->epg=-1;c->fonte=1;c->numero=201+i;}
      nCanais=14;
      SDL_Event e={.type=SDL_KEYDOWN};
      #define MV_KEY(k) do{e.key.keysym.sym=(k);g5ui_event(&e);}while(0)
      g5ui_open("multiview-2");
      /* slot 0: guide channel (M3U fixture source); slot 1: Xtream channel. */
      mv_items[0]=(G5Item){0};mv_items[0].kind=G5_LIVE;mv_items[0].age=-1;
      snprintf(mv_items[0].id,80,"c1");snprintf(mv_items[0].name,140,"SporTV");
      snprintf(mv_fonte[0],24,"fixture");
      mv_items[1]=(G5Item){0};mv_items[1].kind=G5_LIVE;mv_items[1].age=-1;
      snprintf(mv_items[1].id,80,"xtream:501");snprintf(mv_items[1].name,140,"HBO Max 01");
      snprintf(mv_items[1].source,48,"xtream");snprintf(mv_fonte[1],24,"xt");
      mv_count=2;mv_main=0;focus=1;
      for(int i=0;i<4;i++)mv_glide[i].ok=0;
      for(int i=0;i<2;i++){g5_mv_ctl_set(&mvctl,i,mv_items[i].id,mv_fonte[i],0);g5_mv_ctl_pronto(&mvctl,i);}
      capture(window,dir,"mv-2");
      focus=802;MV_KEY(SDLK_RETURN);focus=2;capture(window,dir,"mv-4");
      focus=801;MV_KEY(SDLK_RETURN);mv_items[2]=mv_items[0];snprintf(mv_items[2].id,80,"c2");snprintf(mv_items[2].name,140,"GloboNews");
      snprintf(mv_fonte[2],24,"fixture");mv_count=3;focus=2;for(int i=0;i<4;i++)mv_glide[i].ok=0;
      g5_mv_ctl_set(&mvctl,2,"c2","fixture",0);g5_mv_ctl_pronto(&mvctl,2);
      capture(window,dir,"mv-3");
      /* empty slot tile */
      memset(&mv_items[2],0,sizeof mv_items[2]);mv_fonte[2][0]=0;mv_count=2;focus=2;
      g5_mv_ctl_set(&mvctl,2,"","",0);
      capture(window,dir,"mv-empty");
      /* picker: search row, sections per source, Xtream rows at the limit */
      mv_target=2;g5ui_open("multiview-escolher");quadro();
      focus=899;capture(window,dir,"mv-picker");
      {
        int dis=-1;for(int i=0;i<npicks;i++)if(!picks[i].header&&picks[i].disabled){dis=i;break;}
        assert(dis>0);focus=900+dis;capture(window,dir,"mv-picker-limit");
      }
      pg_tras();
      /* two slots live at once (composite stand-ins for the video planes) */
      focus=1;capture_mv_live(window,dir,"mv-live");
      /* layout switch morph: frames during the spring */
      focus=802;MV_KEY(SDLK_RETURN);
      for(int f=0;f<26;f++){quadro();SDL_GL_SwapWindow(window);SDL_Delay(8);
        if(f==4||f==8||f==14||f==25){char nm[32];snprintf(nm,sizeof nm,"mv-switch-%d",f);capture_now(window,dir,nm);}}
      /* No frame drops, no per-frame texture uploads: reuse is the rule. */
      {double draw=0,worst=0;int rasters=0,uploads=0;Uint64 hz=SDL_GetPerformanceFrequency();
        for(int i=0;i<120;i++){
          txt_novo_quadro();tex_novo_quadro();gfx_novo_quadro();tex_bombear(8);txt_rasterizadas=tex_upl_n=0;
          Uint64 a=SDL_GetPerformanceCounter();g5ui_draw();glFinish();double ms=(SDL_GetPerformanceCounter()-a)*1000.0/hz;
          if(i>=20){draw+=ms;worst=worst>ms?worst:ms;rasters+=txt_rasterizadas;uploads+=tex_upl_n;}
          SDL_GL_SwapWindow(window);
        }
        printf("[mv-perf] frames=100 draw=%.3fms worst=%.3fms text_uploads=%d art_uploads=%d\n",draw/100,worst,rasters,uploads);
        assert(draw/100<16);assert(!rasters);assert(!uploads);}
      #undef MV_KEY
    }
    g5ui_open("grade"); gv_zone=0;
    ajustes_definir_guia_previa_grande(0);capture(window,dir,"preview-card");
    GfxRect card=gv_preview_rect();
    ajustes_definir_guia_previa_grande(1);capture(window,dir,"preview-large");
    GfxRect large=gv_preview_rect();
    if(!baseline) {
      /* Stale upstream assert: fails on integracao/2.1 before this branch
         (the preview card geometry changed); the multiview work does not touch
         it. Guarded with the same macro as the other skipped legacy check. */
#ifndef GUIAPOLIR_BASELINE_CODE
      /* 123ac39a7: the preview is the owner's marked frame (same contract as
         tests/guialayout.c), whichever preview size is chosen. */
      for (int k=0;k<2;k++) { GfxRect r=k?large:card;
        assert(r.x==GV_MX+GV_MW*.51f && r.y==GV_CHIP_Y && r.w==GV_MW*.49f && r.x-GV_MX>=640); }
#endif
#ifndef GUIAPOLIR_BASELINE_CODE
      ajustes_definir_guia_previa_destacar(1);ajustes_definir_guia_previa_sem_som(0);ajustes_definir_guia_dias(3);
      ajustes_iniciar();
      assert(ajustes_guia_previa_destacar() && !ajustes_guia_previa_sem_som() && ajustes_guia_dias()==3);
      guia_definir_preview(1);gv_tick();SDL_Delay(620);gv_tick();
      CatItem preview;assert(guia_pediu_preview(&preview) && !strcmp(preview.imdb,"c1"));
      ajustes_definir_guia_previa_destacar(0);pedPreview=0;gv_tick();SDL_Delay(620);gv_tick();
      assert(!guia_pediu_preview(&preview));
#endif
    }
    puts("PASS: guide options rows, settings, multiview navigation and preview shots");
    return 0;
  }
  /* Back returns one level with focus restored; a closing sheet fades out. */
  g5ui_open("grade");gv_zone=5;focus=1104;gv_go("busca");assert(spot_aberto()&&!strcmp(screen,"grade"));
  spot_fechar();assert(focus==1104);
  focus=1105;gv_go("ajustes");focus=1949;gv_go("epg-saude");focus=1975;gv_go("epg-verificando");
  gv_voltar();assert(!strcmp(screen,"epg-saude")&&focus==1975);
  gv_voltar();assert(!strcmp(screen,"ajustes")&&focus==1949);
  gv_voltar();assert(!strcmp(screen,"grade")&&focus==1105);
  focus=1103;g5ui_open("vod-filmes");gv_voltar();assert(!strcmp(screen,"grade")&&focus==1103);
  gv_sai_t=0;gv_zone=0;
  /* Sports: Up from the first row reaches the tabs and chips; OK on a tab works. */
  g5ui_open("grade");gv_zone=5;focus=1102;gv_go("esportes-ligas");quadro();
  focus=2000;gv_event(SDLK_UP,0);assert((focus>=1100&&focus<1106)||(focus>=1200&&focus<1206));
  focus=1100;quadro();gv_event(SDLK_RETURN,0);assert(!strcmp(screen,"grade"));
  /* Search uses Spotlight; sheet motion remains covered by the options sheet. */
  gv_zone=5;focus=1105;gv_go("ajustes");focus=1940;quadro();SDL_Delay(20);quadro();
  {float y0=gv_gfolha.y;focus=1949;SDL_Delay(20);quadro();assert(gv_gfolha.ok&&gv_gfolha.y!=y0);}
  gv_voltar();query[0]=0;gv_sai_t=0;gv_zone=0;
  const char *names[] = {
    "preview-idle-dark", "preview-focus-moving", "fontes-gerenciar", "preview-playing", "grade", "cartoes", "preview", "preview-grade", "preview-foco", "preview-hero-cheio",
    "preview-troca", "dock", "catchup", "categorias", "pastas", "busca", "canal-opcoes",
    "numero", "opcoes", "ajustes", "epg-saude", "epg-verificando", "epg-adicionado",
    "epg-falha", "erros", "fontes-erro", "vazio", "esportes-ligas", "esportes-liga",
    "esportes-favoritos", "esportes-ajustes", "esportes-onde-assistir", "zap",
    /* navigation states without a mock: base page @ focus */
    "grade@rolada", "grade@futuro", "grade@heroi", "grade@chips", "grade@topo", "cartoes@lateral",
  };
  printf("shot: glass=%d text=%.2f\n", ajustes_vidro(), gfx_escala_txt());
  const char *only = getenv("NUVIO_SHOT_SO");   /* iterate on one state */
  for (unsigned n = 0; n < sizeof names / sizeof *names; n++) {
    if (only && *only && !strstr(only, names[n])) continue;
    if (!strcmp(names[n], "vazio")) {
      nCanais = 0; nCats = 0; gv_n = 0; gv_total_n = 0;  /* estado vazio real */
    }
    if (!strcmp(names[n], "zap")) {
      nLtvF = 1; snprintf(ltvF[0].nome, sizeof ltvF[0].nome, "FrostView"); ltvF[0].tipo = LTV_M3U;
      g5ui_close(); vistaMontar(); focoValido(); overlay = 1; shot_zap = 1;
      capture(window, dir, names[n]);
      overlay = 0; shot_zap = 0;
      continue;
    }
    if (!strcmp(names[n], "fontes-gerenciar")) {
      nLtvF = 2;
      snprintf(ltvF[0].nome, sizeof ltvF[0].nome, "IPTV playlist");
      snprintf(ltvF[1].nome, sizeof ltvF[1].nome, "Live addon");
      ltvF[0].tipo = LTV_M3U; ltvF[1].tipo = LTV_ADDON;
      ltvF[0].nCanais = 4; ltvF[1].nCanais = 4;
    }
    char page[48];
    snprintf(page, sizeof page, "%s", names[n]);
    if (strchr(page, '@')) *strchr(page, '@') = 0;
    if (!strcmp(page, "preview-playing") || !strcmp(page,"preview-idle-dark") || !strcmp(page,"preview-focus-moving"))
      snprintf(page, sizeof page, "grade");
    g5ui_open(page);
    if (!strcmp(names[n], "preview-playing")) {
      aberta = 1;
      gv_assistir("c1");
      CatItem it; assert(guia_pediu_preview(&it));
      float x,y,w,h;guia_preview_rect(&x,&y,&w,&h);
      player_mini_no_guia(x,y,w,h);player_manter_mini();
      cat_definir(&it,1);player_abrir(0,NULL);player_marcar_canal(&it);
      static const VideoSimulacao sim = {.aceita=1,.pronto=1,.tocando=1};
      video_simular(&sim);player_definir_fonte("https://example.invalid/live.m3u8");
      assert(guia_preview_no_ar());
    }
    gv_zone = 0; gv_lin = 0; gv_pin = 0; gv_sb = 3; gv_chipi = 0; gv_card = 0; gv_hbt = 0;
    gv_snap_force = 1; gv_snapshot();
    if (!strcmp(names[n], "grade@rolada")) gv_lin = 6;
    if (!strcmp(names[n], "grade@futuro")) for (int k = 0; k < 3; k++) gv_tempo_passo(1);
    if (!strcmp(names[n], "grade@heroi")) { gv_zone = 2; gv_hbt = 1; }
    if (!strcmp(names[n], "grade@chips")) { gv_zone = 4; gv_chipi = 1; }
    if (!strcmp(names[n], "grade@topo")) { gv_zone = 5; focus = 1101; }
    if (!strcmp(names[n], "cartoes@lateral")) { gv_zone = 3; gv_sb = 2; }
    if (!strcmp(names[n], "preview-foco")) { gv_zone = 2; gv_hbt = 2; }
    if (!strcmp(names[n], "dock")) gv_zone = 1;
    if (!strcmp(names[n], "categorias")) { gv_zone = 3; gv_sb = 6; }
    if (!strcmp(page, "cartoes") && gv_zone == 0) gv_zone = 6;
    if (!strcmp(names[n], "numero")) snprintf(gv_numero, sizeof gv_numero, "1");
    if (!strcmp(names[n], "busca")) snprintf(query, sizeof query, "fla");
    if (!strcmp(names[n], "fontes-erro")) {
      falhas = 0; nLtvF = 1; snprintf(ltvF[0].nome, sizeof ltvF[0].nome, "FrostView");
      ltvF[0].tipo = LTV_M3U; ltvF[0].saude = LTV_SAUDE_FORA; ltvF[0].nCanais = nCanais;
      ltvF[0].atualizada = time(NULL) - 2 * 86400;
    }
    if (!strcmp(names[n], "epg-saude") || !strcmp(names[n], "epg-adicionado")) {
      nLtvF = 1; snprintf(ltvF[0].nome, sizeof ltvF[0].nome, "FrostView");
      ltvF[0].tipo = LTV_M3U; ltvF[0].saude = LTV_SAUDE_OK; ltvF[0].nCanais = nCanais; ltvF[0].epgCom = 5;
    }
    if (!strcmp(names[n], "esportes-onde-assistir"))
      snprintf(gs_selected.nome, sizeof gs_selected.nome, "%s", "Brasileirão: Flamengo × Palmeiras");
    capture(window, dir, names[n]);
    if (!strcmp(names[n], "preview-playing")) player_encerrar();
    nLtvF = 1;
    if (!strcmp(names[n], "vazio")) semear();
  }
  puts("PASS: native v5 guide states captured");
  return 0;
}
