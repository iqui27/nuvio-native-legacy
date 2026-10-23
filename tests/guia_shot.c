// CAPTURA DO CARTAO DE CANAL do Guia, nos DOIS estados, sem rede.
//
// Existe por um pedido do dono (16/09) que so se confere olhando: "tira a
// borda quando nao ta selecionado" e "muda o fundo da logo do canal, deixa so
// branco e preto, e o selecionado tb vai ser um ou outro".
//
// A "borda" era o azulejo de 92x92 pintado numa cor derivada do proprio logo.
// Ele saiu; o cartao passou a ter duas cores e o logo acompanha. Esta captura
// prova as duas metades:
//
//   sem foco:  cartao quase preto, logo BRANCO
//   com foco:  cartao branco,      logo QUASE PRETO
//
// DOIS LOGOS DE MENTIRA, e os dois casos importam:
//   recortado.png  fundo transparente, marca opaca — vai por GFX_MARCA e
//                  inverte junto com o cartao;
//   comfundo.png   quadrado preto opaco com a marca branca (o caso do HBO Max
//                  da foto) — vai por GFX_TEXTO, porque tinta-lo por alfa
//                  cheio desenharia um bloco chapado no lugar da marca.
//
// Inclui src/guia.c: desenharCard e a lista de canais sao estaticos, e semear
// por dentro e o unico jeito de fotografar o cartao sem addon no ar.
#include "badges.h"
#include "nvtmp.h"
#include "aovivo.h"
#include "../src/guia.c"
#include "guialembrete.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "vidro_fundo.h"
#include "shot_arte.h"
#include <SDL2/SDL_image.h>
#include <assert.h>

static void captura(const char *nome, SDL_Window *win) {
  int i;
  time_t agoraT = time(NULL);
  for (i = 0; i < 90; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    gfx_novo_quadro();
    glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    // Os dois estados LADO A LADO, para a comparacao ser uma foto so.
    desenharCard(&canais[0], 120.0f, 120.0f, 0.0f, 1.0f, agoraT);
    desenharCard(&canais[1], 120.0f + G_CARD_W + 40.0f, 120.0f, 1.0f, 1.0f, agoraT);
    desenharCard(&canais[2], 120.0f, 120.0f + G_CARD_H + 40.0f, 0.0f, 1.0f, agoraT);
    desenharCard(&canais[3], 120.0f + G_CARD_W + 40.0f,
                 120.0f + G_CARD_H + 40.0f, 1.0f, 1.0f, agoraT);
    { TxtLinha l = txt_linha(TXT_CAPTION,
        "esquerda: sem foco   direita: com foco   "
        "linha 1: logo recortado   linha 2: logo com fundo proprio",
        170, 172, 180, 255);
      txt_desenhar(l, 120.0f, 120.0f + (G_CARD_H + 40.0f) * 2.0f + 10.0f); }
    if (i == 89) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s;
      int y;
      assert(pix);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      assert(s);
      for (y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(SDL_SaveBMP(s, nome) == 0);
      SDL_FreeSurface(s);
      free(pix);
    }
    SDL_GL_SwapWindow(win);
  }
  printf("captura: %s\n", nome);
}

// A TELA INTEIRA DO GUIA, e nao so o cartao. Existe porque o dono relatou "tv
// guide nao ta abrindo" numa noite em que os DOIS addons de canal estavam fora
// (FrostView devolvendo 408 em meio segundo, Minha TV sem responder o catalogo
// em 40 s), e a pergunta que o log nao responde e o que aparece na tela quando
// nao ha canal nenhum: a frase de "nenhum catalogo" ou um retangulo preto.
//
// `estado` e `nCanais` sao estaticos de guia.c, que este teste inclui — dai
// dar para encenar o caso sem addon no ar e sem esperar dois timeouts.
static void capturaTela(const char *nome, SDL_Window *win, int comCanais) {
  int i;
  for (i = 0; i < 90; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    gfx_novo_quadro();
    // A hostile underlying Home color must never survive preview-off rendering.
    glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    guia_desenhar(SDL_GetTicks());
    glem_desenhar(SDL_GetTicks());
    if (i == 89) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s;
      int y;
      assert(pix);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32,
                                         SDL_PIXELFORMAT_RGBA32);
      assert(s);
      for (y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch,
               pix + (1079 - y) * 1920 * 4, 1920 * 4);
      if (!previewLigado && !player_mini_no_guia_ativo()) {
        unsigned char *p = (unsigned char *)s->pixels + 240 * s->pitch + 1800 * 4;
        assert(p[3] == 255 && p[0] < 100 && p[2] < 100);
      }
      assert((strstr(nome, ".png") ? IMG_SavePNG(s, nome) : SDL_SaveBMP(s, nome)) == 0);
      SDL_FreeSurface(s);
      free(pix);
    }
    SDL_GL_SwapWindow(win);
  }
  (void)comCanais;
  printf("captura: %s\n", nome);
}

// The real drawer renderers against artwork; no add-on service is started.
static void capturaPainel(const char *nome, SDL_Window *win, int addonsPane) {
  int q,y;
  for(q=0;q<70;q++) {
    SDL_PumpEvents();txt_novo_quadro();tex_novo_quadro();tex_bombear(6);gfx_novo_quadro();
    glClearColor(.03f,.03f,.035f,1);glClear(GL_COLOR_BUFFER_BIT);
    if (vidroFundoAtivo()) vidroFundoDesenhar(); else shot_arte_desenhar(0);
    if(addonsPane==3) { char fo[64]; CatItem it; const char *nm=NULL; fo[0]=0; if (guia_numero_canal(101,&it,fo,sizeof fo,0)) nm=it.titulo; aovivo_numero_desenhar("1 0 1",nm,fo,1); }
    else if(addonsPane==2) { desenharGrupos(1); } else if(addonsPane) desenharPainelAddons(1); else desenharPainelCategorias(1);
    if(q==69) {
      unsigned char *pix=malloc(1920*1080*4);
      SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32);assert(pix&&s);
      glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,pix);
      for(y=0;y<1080;y++) memcpy((char*)s->pixels+y*s->pitch,pix+(1079-y)*1920*4,1920*4);
      assert(IMG_SavePNG(s,nome)==0);SDL_FreeSurface(s);free(pix);
    }
    SDL_GL_SwapWindow(win);
  }
  printf("capture: %s\n",nome);
}

// SEMEIA A LISTA PELO CAMINHO DE VERDADE (M1): staging -> adotarStaging, como o
// fio de carga. Cada canal: fonte, grupo, nome, logo, descricao e numero.
typedef struct { const char *fonte, *grupo, *nome, *logo, *desc; int numero; } ShotCanal;
static void semearCanais(const ShotCanal *v, int n) {
  int i;
  stagingZerar();
  sTeto = 30000;
  for (i = 0; i < n; i++) {
    int fl = sFonte(LTV_ADDON, v[i].fonte, v[i].fonte, "");
    char id[32];
    GCru c;
    snprintf(id, sizeof id, "c%d", i);
    memset(&c, 0, sizeof c);
    c.id = id; c.nome = v[i].nome; c.logo = v[i].logo; c.desc = v[i].desc ? v[i].desc : "";
    c.grupo = v[i].grupo; c.numero = v[i].numero;
    c.epgId = c.tvgNome = c.url = c.ua = c.ref = "";
    sGrupoInfo(fl, v[i].grupo, 1);
    sCanalAdd(fl, &c);
    sLtvF[fl].total++;
  }
  favLido = 1;
  adotarStaging();
  estado = G_PRONTO;
}

// GRADE DE MENTIRA para a tela do guia (25/09/2026): o heroi e a grade so se
// conferem com programa de verdade no ar, e a rede nao entra no teste. O
// XMLTV e montado relativo a meia hora corrente, entao a linha "agora" cai
// sempre no primeiro terco da janela, como na TV. Canais e titulos sao
// inventados; nenhum dado de conta passa por aqui.
static void xtv(char *dst, time_t t) {
  struct tm *m = gmtime(&t);
  strftime(dst, 24, "%Y%m%d%H%M%S +0000", m);
}
static void semearEpg(void) {
  static const struct { const char *canal; int ini, fim; const char *titulo; } P[] = {
    { "Globo.RJ.br", -40, 25, "Jornal da Noite" },
    { "Globo.RJ.br", 25, 95, "Novela das Nove" },
    { "Globo.RJ.br", 95, 185, "Cinema Especial: A Grande Viagem" },
    { "SporTV.br", -70, 50, "Futebol ao Vivo: Rio x Sao Paulo" },
    { "SporTV.br", 50, 80, "Resenha" },
    { "SporTV.br", 80, 200, "Tenis: Final do Aberto" },
    { "GNT.br", -10, 20, "Receitas Rapidas" },
    { "GNT.br", 20, 50, "Casa Nova" },
    { "GNT.br", 50, 80, "Viagem Curta" },
    { "GNT.br", 80, 140, "Papo de Sofa" },
    { "Cartoon.Network.br", -15, 5, "Desenho A" },
    { "Cartoon.Network.br", 5, 25, "Desenho B" },
    { "Cartoon.Network.br", 25, 45, "Desenho C" },
    { "Cartoon.Network.br", 45, 105, "Maratona de Aventuras" },
    { "Discovery.br", -55, 35, "Engenharia Extrema" },
    { "Discovery.br", 35, 125, "Vida Selvagem" },
  };
  char *xml = malloc(20000), ini[24], fim[24];
  size_t n = 0;
  unsigned k;
  assert(xml);
  n += (size_t)snprintf(xml + n, 20000 - n, "%s",
    "<?xml version=\"1.0\"?><tv>"
    "<channel id=\"Globo.RJ.br\"><display-name>Globo RJ</display-name></channel>"
    "<channel id=\"SporTV.br\"><display-name>SporTV</display-name></channel>"
    "<channel id=\"GNT.br\"><display-name>GNT</display-name></channel>"
    "<channel id=\"Cartoon.Network.br\"><display-name>Cartoon Network</display-name></channel>"
    "<channel id=\"Discovery.br\"><display-name>Discovery</display-name></channel>");
  for (k = 0; k < sizeof P / sizeof P[0]; k++) {
    xtv(ini, time(NULL) + (time_t)P[k].ini * 60);
    xtv(fim, time(NULL) + (time_t)P[k].fim * 60);
    n += (size_t)snprintf(xml + n, 20000 - n,
      "<programme channel=\"%s\" start=\"%s\" stop=\"%s\"><title>%s</title></programme>",
      P[k].canal, ini, fim, P[k].titulo);
  }
  snprintf(xml + n, 20000 - n, "</tv>");
  assert(epg_xml_processar(xml) == (int)(sizeof P / sizeof P[0]));
  free(xml);
}

// Monta a lista do guia: duas fontes, tres grupos, canais com e sem grade.
// "Esportes" existe nas duas fontes e aparece DUAS vezes (decisao 2).
static void semearGuia(void) {
  // logo: 0 recortado, 1 com fundo proprio, 2 AZULEJO, 3 arquivo que NAO EXISTE.
  static const char *LOGO[] = {
    "tests/fixtures/logos/recortado.png", "tests/fixtures/logos/comfundo.png",
    "tests/fixtures/logos/azulejo.png", "tests/fixtures/logos/nao-existe.png",
  };
  ShotCanal C[] = {
    { "FrostView", "Abertos", "Globo RJ", NULL, "Categoria: Abertos Qualidades: 4K, FHD, HD, SD 8 fonte(s)", 4 },
    { "FrostView", "Abertos", "GNT", NULL, "Categoria: Abertos 1 fonte(s)", 41 },
    { "FrostView", "Abertos", "Cartoon Network", NULL, NULL, 33 },
    { "FrostView", "Abertos", "Canal Recortado HD", NULL, NULL, 50 },
    { "FrostView", "Abertos", "Discovery", NULL, NULL, 52 },
    { "FrostView", "Abertos", "Canal 24h Classicos", NULL, NULL, 99 },
    { "FrostView", "Esportes", "SporTV", NULL, NULL, 101 },
    { "FrostView", "Esportes", "GE TV", NULL, "Canal de esportes com jogos ao vivo, resenha e bastidores.", 102 },
    { "Xtream", "Esportes", "ESPN", NULL, NULL, 101 },
    { "Xtream", "Esportes", "Premiere Clubes", NULL, NULL, 103 },
  };
  static const int L[] = { 2, 1, 3, 0, 2, 3, 2, 3, 0, 1 };
  int i, n = (int)(sizeof C / sizeof C[0]);
  for (i = 0; i < n; i++) C[i].logo = LOGO[L[i]];
  semearCanais(C, n);
  canais[1].fav = 1;
  // CANAL M3U (M9): o app toca a URL guardada com o UA e o Referer do canal.
  { const char *idAntes = canais[2].id, *urlAntes = canais[2].url, *uaAntes = canais[2].ua, *refAntes = canais[2].ref;
    char u[200], ua[64], rf[64];
    assert(!guia_m3u_canal(canais[2].id, u, sizeof u, ua, sizeof ua, rf, sizeof rf));
    canais[2].id = "m3u:canaisabertos:0";
    assert(!guia_e_m3u(canais[2].id)); // Add-on IDs keep the add-on stream resolver.
    int tipoAntes = ltvF[canais[2].fonte].tipo;
    ltvF[canais[2].fonte].tipo = LTV_M3U;
    canais[2].id = "m3u:m1234abcd:0000beef"; canais[2].url = "http://exemplo.invalid/c.m3u8";
    canais[2].ua = "VLC/3.0"; canais[2].ref = "";
    assert(guia_e_m3u(canais[2].id) && guia_m3u_canal(canais[2].id, u, sizeof u, ua, sizeof ua, rf, sizeof rf));
    assert(!strcmp(u, "http://exemplo.invalid/c.m3u8") && !strcmp(ua, "VLC/3.0") && !rf[0]);
    ltvF[canais[2].fonte].tipo = tipoAntes;
    canais[2].id = idAntes; canais[2].url = urlAntes; canais[2].ua = uaAntes; canais[2].ref = refAntes; }
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-guia");
  char nome[600];
  SDL_Window *w;
  SDL_GLContext gl;

  // genero de addon em Python (json.dumps escapa o nao-ASCII): a categoria
  // saia "Notu00edcias" (medido em 01/10/2026). Ver tests/jscadeia.c.
  { const char *m = "{\"genre\":[\"Not\\u00edcias\"]}";
    char gen[64];
    assert(lerStrEl(js_array(m, NULL, "genre"), gen, sizeof gen));
    assert(!strcmp(gen, "Not\xc3\xad" "cias")); }

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: cartao de canal", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1920, 1080,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  if (getenv("NUVIO_SHOT_VIDRO")) ajustes_definir_vidro(1);
  if (getenv("NUVIO_SHOT_TOPO")) {
    extern void ajustes_teste_vidro_env(void);
    const char *dir = getenv("NUVIO_DADOS"); FILE *f;
    assert(dir && *dir); snprintf(nome, sizeof nome, "%s/ajustes.txt", dir);
    f = fopen(nome, "w"); assert(f);
    fprintf(f, "idioma 0\nselected_theme %s\nvidroLocal %d\n",
            getenv("NUVIO_SHOT_TEMA") ? getenv("NUVIO_SHOT_TEMA") : "2",
            getenv("NUVIO_SHOT_SOLIDO") ? 1 : 0);
    fclose(f);
    ajustes_dir(dir); ajustes_teste_vidro_env();
  }
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  badges_carregar("deploy/app/art");   // marcas de resolucao no heroi; no app quem faz e home.c

  if (argc > 2 && !strcmp(argv[2], "header")) {
    const char *dir = getenv("NUVIO_DADOS");
    char settings[600];
    snprintf(settings, sizeof settings, "%s/ajustes.txt", dir);
    semearGuia();
    aberta = 1; entrada = heroA = 1; overlay = 0;
    modoLista = 0; previewLigado = 0; previewLido = modoLido = 1;
    focoLin = 0; focoCol = 2; focoTopo = focoFontes = 0;
    SDL_Event e = {0}; e.type = SDL_KEYDOWN;
    for (int lang = 0; lang < IDIOMA_N; lang++) {
      FILE *f = fopen(settings, "w"); assert(f);
      fprintf(f, "idioma %d\nidiomaAutoLocal 0\n", lang); fclose(f);
      ajustes_dir(dir); txt_novo_quadro();
      const char *labels[G_TOPO_N]; float widths[G_TOPO_N], xs[G_TOPO_N];
      gTopoRotulos(labels); gTopoLarguras(labels, widths);
      gTopoAjustar(widths, xs, G_INFO_X + G_INFO_W);
      assert(xs[0] == G_AREA_X);
      for (int i = 0; i < G_TOPO_PRIMARIOS; i++) {
        float icon = i <= G_TOPO_CATEGORIAS ? 26 : 0;
        assert(widths[i] > 2 * G_CHIP_PAD + icon);
        assert(xs[i] + widths[i] <= G_INFO_X + G_INFO_W + .1f);
        if (i) assert(xs[i] >= xs[i-1] + widths[i-1] + (i == G_TOPO_LISTA ? 0 : 11.9f));
      }
    }
    // UP reaches chips directly even when the sources bar is visible.
    for (int mode = 0; mode < 2; mode++) {
      modoLista = mode; focoLin = focoCol = focoTopo = focoFontes = dirSeg = 0;
      e.key.keysym.sym = SDLK_UP; guia_evento(&e);
      assert(focoTopo && !focoFontes);
      e.key.keysym.sym = SDLK_LEFT;
      for (int i = 0; i < G_TOPO_PRIMARIOS; i++) guia_evento(&e);
      assert(topoCol == G_TOPO_BUSCAR);
      e.key.keysym.sym = SDLK_RIGHT;
      for (int i = 0; i < G_TOPO_PRIMARIOS; i++) guia_evento(&e);
      assert(topoCol == G_TOPO_OPCOES);
      e.key.keysym.sym = SDLK_RETURN; guia_evento(&e);
      assert(opcoesAberto && opcoesFoco == G_TOPO_ADDONS);
      e.key.keysym.sym = SDLK_ESCAPE; guia_evento(&e);
      assert(!opcoesAberto && aberta && focoTopo && topoCol == G_TOPO_OPCOES);
    }
    // Each entry dispatches its existing action; clear defaults to cancel.
    for (int action = G_TOPO_ADDONS; action <= G_TOPO_PREVIEW; action++) {
      e.key.keysym.sym = SDLK_RETURN; guia_evento(&e);
      e.key.keysym.sym = SDLK_DOWN;
      for (int i = G_TOPO_ADDONS; i < action; i++) guia_evento(&e);
      assert(opcoesFoco == action);
      e.key.keysym.sym = SDLK_RETURN; guia_evento(&e);
      assert(!opcoesAberto);
      if (action == G_TOPO_ADDONS) assert(painel && !paModo);
      if (action == G_TOPO_FILTROS) assert(painel && paModo == 1);
      if (action == G_TOPO_DIAG) assert(guia_pediu_livetv_diag());
      if (action == G_TOPO_LIMPAR) assert(limparConfirm && limparFoco == 1);
      if (action == G_TOPO_PREVIEW) assert(previewLigado);
      if (painel || limparConfirm) {
        e.key.keysym.sym = SDLK_ESCAPE; guia_evento(&e);
      }
      assert(aberta);
    }
    previewLigado = 0; previewParar();
    modoLista = 0; focoLin = 0; focoCol = 2; focoTopo = focoFontes = 0;
    for (int capture = 0; capture < 3; capture++) {
      FILE *f = fopen(settings, "w"); assert(f);
      fprintf(f, "idioma %d\nidiomaAutoLocal 0\n", capture == 2 ? 0 : IDIOMA_EN); fclose(f);
      ajustes_dir(dir);
      opcoesAberto = capture == 1; opcoesFoco = G_TOPO_PREVIEW;
      focoTopo = opcoesAberto; topoCol = G_TOPO_OPCOES;
      for (int i = 0; i < G_TOPO_N; i++) animTopo[i] = 0;
      snprintf(nome, sizeof nome, "%s/depois-guia%s.png", saida,
               capture == 1 ? "-opcoes" : capture == 2 ? "-ptbr" : "");
      capturaTela(nome, w, 1);
    }
    puts("PASS: all UI languages fit four header groups; UP/arrows/Options/Back and five actions.");
    tex_encerrar(); txt_encerrar(); gfx_encerrar();
    SDL_GL_DeleteContext(gl); SDL_DestroyWindow(w); SDL_Quit();
    return 0;
  }

  if (getenv("NUVIO_GUIALIMPA_SHOT")) {
    semearGuia(); aberta = 1; entrada = 1; overlay = 0;
    focoTopo = 1; topoCol = G_TOPO_OPCOES; opcoesAberto = 1; opcoesFoco = G_TOPO_LIMPAR;
    snprintf(nome, sizeof nome, "%s-action.bmp", saida); capturaTela(nome, w, 1);
    limparConfirm = 1; limparFoco = 1;
    snprintf(nome, sizeof nome, "%s-confirm.bmp", saida); capturaTela(nome, w, 1);
    stagingZerar(); adotarStaging(); estado = G_FALHOU;
    snprintf(nome, sizeof nome, "%s-empty-confirm.bmp", saida); capturaTela(nome, w, 0);
    tex_encerrar(); txt_encerrar(); gfx_encerrar();
    SDL_GL_DeleteContext(gl); SDL_DestroyWindow(w); SDL_Quit();
    return 0;
  }

  if(getenv("NUVIO_GUIDE_PANELS")) {
    extern void ajustes_teste_vidro_env(void);
    const char *dir=getenv("NUVIO_DADOS");FILE *f;int i;
    assert(dir&&*dir);snprintf(nome,sizeof nome,"%s/ajustes.txt",dir);f=fopen(nome,"w");assert(f);
    fprintf(f,"idioma 0\nselected_theme 2\nvidroLocal %d\n",getenv("NUVIO_SHOT_SOLIDO")?1:0);fclose(f);
    ajustes_dir(dir);ajustes_teste_vidro_env();vidroFundoPreparar();
    semearGuia();catAnim=1;catFoco=1;focoLin=0;
    snprintf(nome,sizeof nome,"%s-categories.png",saida);capturaPainel(nome,w,0);
    paN=0;nRec=3;paFoco=0;
    for(i=0;i<3;i++) {
      snprintf(rec[i].nome,sizeof rec[i].nome,"%s",i==0?"FrostView":i==1?"Minha TV":"Canais ao vivo");
      snprintf(rec[i].desc,sizeof rec[i].desc,"%s","Canais e programação ao vivo");
      snprintf(rec[i].url,sizeof rec[i].url,"https://fixture.invalid/%d",i);
    }
    snprintf(nome,sizeof nome,"%s-addons.png",saida);capturaPainel(nome,w,1);
    tex_encerrar();txt_encerrar();gfx_encerrar();SDL_GL_DeleteContext(gl);SDL_DestroyWindow(w);SDL_Quit();
    return 0;
  }

  { static const ShotCanal C4[] = {
      { "Addon", "Aberta", "Canal Recortado HD", "tests/fixtures/logos/recortado.png", NULL, 1 },
      { "Addon", "Aberta", "Canal Recortado HD", "tests/fixtures/logos/recortado.png", NULL, 2 },
      { "Addon", "Aberta", "Canal Com Fundo HD", "tests/fixtures/logos/comfundo.png", NULL, 3 },
      { "Addon", "Aberta", "Canal Com Fundo HD", "tests/fixtures/logos/comfundo.png", NULL, 4 } };
    int k;
    semearCanais(C4, 4);
    for (k = 0; k < 4; k++) canais[k].epg = -2;   // sem grade real: "AO VIVO"
    canais[1].fav = 1; }

  // O MOLDE DA DESCRICAO nas duas formas medidas na C9, e texto livre.
  { GDesc d;
    assert(descMolde("Categoria: HBO Qualidades: 4K, FHD, HD, SD 8 fonte(s)", &d));
    assert(d.nq == 4 && !strcmp(d.q[0], "4K") && !strcmp(d.q[3], "SD") && d.fontes == 8);
    assert(!d.resto[0]);
    assert(descMolde("Categoria: Canais 24 Horas 1 fonte(s)", &d));
    assert(d.nq == 0 && d.fontes == 1 && !d.resto[0]);
    assert(descMolde("Filmes o dia todo. Categoria: Filmes Qualidades: HD 2 fonte(s) Sinal SP", &d));
    assert(d.nq == 1 && d.fontes == 2 && !strcmp(d.resto, "Filmes o dia todo. Sinal SP"));
    assert(!descMolde("Canal de esportes com jogos ao vivo.", &d));
    puts("ok: molde da descricao do addon"); }

  snprintf(nome, sizeof nome, "%s-cartoes.bmp", saida);
  captura(nome, w);

  // A TELA DO GUIA com grade de verdade (heroi + grade, 25/09/2026). Tres
  // estados que so o olho confere:
  //   lista-epg      foco num canal COM programa: heroi com titulo, horario,
  //                  barra e "A seguir"; celula em foco com o anel; a linha
  //                  agora e o veu do passado na grade;
  //   lista-sem-epg  foco num canal SEM grade: heroi limpo ("Sem grade de
  //                  programacao") e a faixa unica discreta na grade;
  //   lista-topo     foco nos chips do cabecalho (anel no chip Preview);
  //   lista-adiante  janela adiantada 1 h: o heroi mostra o programa da
  //                  celula em foco ("Comeca em ..."), sem linha agora.
  // O preview de video nao aparece em captura nenhuma: o plano de video fica
  // ATRAS da superficie GL, e glReadPixels so ve o furo. Aqui ele nem abre
  // (sem pipeline), entao o lugar mostra o logo — o estado "sem video".
  semearEpg();
  semearGuia();
  // LEMBRETES: dois programas FUTUROS marcados, um na janela de agora (Novela
  // das Nove, Globo) e um na janela adiantada (Viagem Curta, GNT) — o sino
  // tem de aparecer nas duas celulas, e o heroi da adiantada diz "Lembrete
  // marcado".
  lembrete_carregar(1);
  { int k;
    for (k = 0; k < 2; k++) {
      GCanal *c = k ? &canais[1] : &canais[0];
      const char *tit = k ? "Viagem Curta" : "Novela das Nove";
      EpgProg pp; int e = epgDo(c), j;
      for (j = 0; e >= 0 && j < 8 && epg_proximo(e, time(NULL), j, &pp); j++)
        if (!strcmp(pp.titulo, tit)) {
          lembrete_alternar(c->id, c->nome, pp.titulo, c->base, pp.ini, pp.fim);
          break;
        }
    } }
  aberta = 1; entrada = 1.0f; estado = G_PRONTO; heroA = 1.0f;
  previewLigado = 1; previewLido = 1;
  modoLista = 1; modoLido = 1;
  focoLin = 0; focoCol = 0; focoTopo = 0; focoAnelOk = 0;
  snprintf(nome, sizeof nome, "%s-lista-epg.bmp", saida);
  capturaTela(nome, w, 1);

  focoLin = 1; focoCol = 1; focoAnelOk = 0;      // GE TV, sem grade
  // A mesma conta de rolagem de guia_atualizar, ja assentada.
  { float maxY = listaAltura() + G_L_FADE - (G_L_BASE - G_L_TOPO);
    rolL = listaYDe(focoLin, focoCol) - G_L_HEAD - G_L_ROW;
    if (rolL > maxY) rolL = maxY;
    if (rolL < 0.0f) rolL = 0.0f; }
  snprintf(nome, sizeof nome, "%s-lista-sem-epg.bmp", saida);
  capturaTela(nome, w, 1);

  // BARRA DE FONTES (M3): foco no chip do Xtream, com a linha da fonte.
  focoLin = 0; focoCol = 0; rolL = 0.0f; focoAnelOk = 0;
  ltvF[1].saude = LTV_SAUDE_DEGRADADA; ltvF[1].cortada = 1; ltvF[1].total = 16462;
  focoFontes = 1; fontesCol = 3;
  snprintf(nome, sizeof nome, "%s-fontes.bmp", saida);
  capturaTela(nome, w, 1);
  // ESCOLHER GRUPOS (M2): teto de 3000 e uma fonte com grupos grandes.
  { static const struct { const char *n; int c; } G[] = {
      {"Esportes",412},{"Filmes",980},{"Series",1450},{"Abertos",190},{"Noticias",120},
      {"Infantil",260},{"Documentarios",310},{"Musica",150},{"Adulto",500},{"Radio",90},
      {"Internacional",1700},{"Religiosos",75},{"PPV",64} };
    GFonteLtv *F = &ltvF[1];
    int k;
    free(F->gi); F->gi = calloc(13, sizeof *F->gi); F->ngi = 13;
    for (k = 0; k < 13; k++) { F->gi[k].nome = G[k].n; F->gi[k].n = G[k].c; F->gi[k].entra = k < 3; }
    tetoCarga = 3000;
    gpAbrir(1);
    gpMarca[0] = 1; gpMarca[3] = 1; gpMarca[4] = 1; gpMarca[5] = 1; gpFoco = 5;
    snprintf(nome, sizeof nome, "%s-grupos.png", saida);
    capturaPainel(nome, w, 2);
    gpAberto = 0; tetoCarga = 30000;
    // DIGITAR O NUMERO (M7): 101 existe em duas fontes (FrostView e Xtream).
    snprintf(nome, sizeof nome, "%s-numero.png", saida);
    capturaPainel(nome, w, 3); }
  // ESCOLHER GRADE DESTE CANAL (M5): opcoes do canal -> lista de grades.
  focoFontes = 0;
  fopCanalAbrir(&canais[0]); fopFoco = 1; fopCanalOk(); fopFoco = 2;
  snprintf(nome, sizeof nome, "%s-grade-manual.bmp", saida);
  capturaTela(nome, w, 1);
  fopAberto = 0;
  focoLin = 0; focoCol = 1; rolL = 0.0f; focoAnelOk = 0;
  focoTopo = 1; topoCol = G_TOPO_OPCOES; animTopo[G_TOPO_OPCOES] = 1.0f;
  snprintf(nome, sizeof nome, "%s-lista-topo.bmp", saida);
  capturaTela(nome, w, 1);
  focoTopo = 0; animTopo[G_TOPO_OPCOES] = 0.0f;

  // Barra de cima, cada botao em foco (W18): NUVIO_SHOT_TOPO=1; tema/material
  // pelo ambiente (NUVIO_SHOT_TEMA = selected_theme, NUVIO_SHOT_SOLIDO).
  if (getenv("NUVIO_SHOT_TOPO")) {
    int k;
    for (k = 0; k < G_TOPO_PRIMARIOS; k++) {
      int j;
      for (j = 0; j < G_TOPO_N; j++) animTopo[j] = 0.0f;
      focoTopo = 1; topoCol = k; animTopo[k] = 1.0f; focoAnelOk = 0;
      snprintf(nome, sizeof nome, "%s-topo-%d.bmp", saida, k);
      capturaTela(nome, w, 1);
    }
    focoTopo = 0;
    { int j; for (j = 0; j < G_TOPO_N; j++) animTopo[j] = 0.0f; }
    return 0;
  }

  janelaDesl = 60; focoAnelOk = 0;
  snprintf(nome, sizeof nome, "%s-lista-adiante.bmp", saida);
  capturaTela(nome, w, 1);
  janelaDesl = 0;

  modoLista = 0; focoLin = 0; focoCol = 0; focoAnelOk = 0;
  snprintf(nome, sizeof nome, "%s-tela.bmp", saida);
  capturaTela(nome, w, 1);
  // A DICA DE PRIMEIRA VEZ da ESQUERDA (dicaTalvez), por cima da mesma tela.
  dicaDesde = SDL_GetTicks() - 1000u;
  snprintf(nome, sizeof nome, "%s-tela-dica.bmp", saida);
  capturaTela(nome, w, 1);
  dicaDesde = 0;
  // A BUSCA DO GUIA: canais pelo nome e programas pela grade.
  buscaFazer("sport");
  buscaEstado = 2; buscaAnim = 1.0f; buscaFoco = 1;
  snprintf(nome, sizeof nome, "%s-busca.bmp", saida);
  capturaTela(nome, w, 1);
  buscaFazer("jornal");
  buscaFoco = 0;
  snprintf(nome, sizeof nome, "%s-busca-programa.bmp", saida);
  capturaTela(nome, w, 1);
  buscaEstado = 0; buscaAnim = 0.0f;

  // O CARTAO DO LEMBRETE por cima da tela (o que aparece em qualquer lugar
  // do app quando o programa comeca), e o aviso curto de quem ja esta no
  // canal.
  modoLista = 1; focoLin = 0; focoCol = 0; focoAnelOk = 0;
  glem_teste_cartao("Novela das Nove", "Globo RJ", 0);
  snprintf(nome, sizeof nome, "%s-lembrete-cartao.bmp", saida);
  capturaTela(nome, w, 1);
  glem_teste_cartao("Novela das Nove", "Globo RJ", 1);
  snprintf(nome, sizeof nome, "%s-lembrete-curto.bmp", saida);
  capturaTela(nome, w, 1);
  glem_teste_cartao("", "", 1); glem_passo(10.0f, SDL_GetTicks() + 999999u);
  modoLista = 0;

  // A FAIXA DO MINI GUIA por cima do video em tela cheia: cinco canais com o
  // focado no meio, regua, linha agora. Aqui o "video" e o fundo liso da
  // captura — o plano de hardware nao aparece em glReadPixels.
  aberta = 0; overlay = 1; focoLin = 0; focoCol = 3; focoAnelOk = 0; bandaUlt = SDL_GetTicks();
  snprintf(nome, sizeof nome, "%s-faixa.bmp", saida);
  capturaTela(nome, w, 1);
  overlay = 0; aberta = 1; focoCol = 0;

  // O PAINEL DE CATEGORIAS aberto pelo chip, com 35 secoes de nome comprido
  // (a conta real do dono tem 35): gaveta opaca, contagem a direita, secao
  // atual marcada, foco preenchido, rolagem no meio.
  { static const char *NC[] = { "Canais 24 Horas", "Canais Abertos", "Canais Esportes",
      "Canais Infantil", "Canais Liga Futsal", "Canais Max", "Canais NBA",
      "Canais Noticias", "Canais Paramount", "Canais PPV", "Canais Prime Video",
      "Canais Reality Shows", "Canais Recordtv", "Canais Religiosos",
      "Canais Series 24h Maratonas Sem Fim", "Canais Sportv", "Canais Telecine",
      "Canais Variedades", "Disney+", "HBO", "Premiere" };
    static ShotCanal V[1200];
    int k, j, n = 0, nc = (int)(sizeof NC / sizeof NC[0]);
    for (k = 0; k < nc; k++)
      for (j = 0; j < (k % 3 == 0 ? 117 : 4 + k) && n < 1200; j++) {
        V[n].fonte = "FrostView"; V[n].grupo = NC[k]; V[n].nome = "Canal";
        V[n].logo = "tests/fixtures/logos/recortado.png"; V[n].desc = NULL; V[n].numero = n + 1; n++;
      }
    semearCanais(V, n); }
  modoLista = 1; focoLin = 0; focoCol = 0; focoAnelOk = 0;
  catAberto = 2; catFoco = 14; catAnim = 1.0f;
  { float areaH = G_CAT_BASE - G_CAT_TOPO;
    catRol = (float)catFoco * G_CAT_ROW - (areaH - G_CAT_ROW) * 0.5f;
    if (catRol > (float)nCats * G_CAT_ROW - areaH) catRol = (float)nCats * G_CAT_ROW - areaH;
    if (catRol < 0.0f) catRol = 0.0f; }
  focoLin = 12;
  snprintf(nome, sizeof nome, "%s-lista-categorias.bmp", saida);
  capturaTela(nome, w, 1);
  // A MESMA gaveta no MEIO da lista e no meio de uma rolagem (catRol fora do
  // multiplo de linha): e o quadro da foto do dono, com linha pela metade
  // sob o titulo e sobre o rodape. Aqui elas tem de sair apagadas, nao
  // cortadas no meio da letra.
  catFoco = 7; catRol = 3.0f * G_CAT_ROW + 27.0f;
  snprintf(nome, sizeof nome, "%s-lista-categorias-meio.bmp", saida);
  capturaTela(nome, w, 1);
  catAberto = 0; catAnim = 0.0f; focoLin = 0;

  stagingZerar(); adotarStaging(); nFontes = 0; fontesOk = 1; estado = G_FALHOU;
  falhas = 0;
  snprintf(nome, sizeof nome, "%s-tela-vazia.bmp", saida);
  capturaTela(nome, w, 0);

  // O OUTRO caso vazio: o addon ESTA instalado e nao respondeu. Sao duas
  // frases diferentes de proposito — uma pede uma instalacao, a outra conta o
  // que houve — e so a captura prova que a tela escolhe a certa.
  falhas = 2;
  snprintf(nome, sizeof nome, "%s-tela-sem-resposta.bmp", saida);
  capturaTela(nome, w, 0);

  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  puts("PASS: capturas do cartao de canal e da tela do guia gravadas.");
  return 0;
}
