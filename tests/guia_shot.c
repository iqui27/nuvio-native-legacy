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
#include "../src/guia.c"
#include "guialembrete.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
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
    glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
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
      assert(SDL_SaveBMP(s, nome) == 0);
      SDL_FreeSurface(s);
      free(pix);
    }
    SDL_GL_SwapWindow(win);
  }
  (void)comCanais;
  printf("captura: %s\n", nome);
}

static void poeCanal(int i, const char *nome, const char *logo) {
  snprintf(canais[i].id, sizeof canais[i].id, "c%d", i);
  snprintf(canais[i].nome, sizeof canais[i].nome, "%s", nome);
  snprintf(canais[i].logo, sizeof canais[i].logo, "%s", logo);
  canais[i].desc[0] = 0;
  canais[i].cat = 0;
  canais[i].epg = -2;      // sem grade real: o cartao mostra "AO VIVO"
  canais[i].fav = i == 1;
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

// Monta a lista do guia: duas categorias, canais com e sem grade.
static void semearGuia(void) {
  // logo: 0 recortado (silhueta de um tom), 1 com fundo proprio, 2 AZULEJO
  // (quadrado claro arredondado com margem transparente e marca escura — o
  // formato que virava bloco creme na C9), 3 arquivo que NAO EXISTE (o logo
  // que falha: tem de aparecer o azulejo escuro com as iniciais).
  static const char *LOGO[] = {
    "tests/fixtures/logos/recortado.png", "tests/fixtures/logos/comfundo.png",
    "tests/fixtures/logos/azulejo.png", "tests/fixtures/logos/nao-existe.png",
  };
  static const struct { const char *nome; int cat; int logo; } C[] = {
    { "Globo RJ", 0, 2 }, { "SporTV", 1, 2 }, { "GE TV", 1, 3 },
    { "GNT", 0, 1 }, { "Cartoon Network", 0, 3 }, { "Canal Recortado HD", 0, 0 },
    { "Discovery", 0, 2 }, { "Canal 24h Classicos", 0, 3 },
  };
  int i, n = (int)(sizeof C / sizeof C[0]), w = 0, c;
  // Ordem publicada = agrupada por categoria, como empacotar() deixa.
  for (c = 0; c < 2; c++)
    for (i = 0; i < n; i++) {
      if (C[i].cat != c) continue;
      poeCanal(w, C[i].nome, LOGO[C[i].logo]);
      canais[w].cat = c; canais[w].epg = -1; canais[w].fav = 0;
      w++;
    }
  // O molde do addon (medido na C9): vira selos + "8 fontes", sem a
  // "Categoria:" repetida. GE TV leva texto livre, que tem de ir como veio.
  snprintf(canais[0].desc, sizeof canais[0].desc, "%s",
           "Categoria: Abertos Qualidades: 4K, FHD, HD, SD 8 fonte(s)");
  snprintf(canais[3].desc, sizeof canais[3].desc, "%s",
           "Categoria: Abertos 1 fonte(s)");
  snprintf(canais[7].desc, sizeof canais[7].desc, "%s",
           "Canal de esportes com jogos ao vivo, resenha e bastidores.");
  canais[1].fav = 1;
  nCanais = w;
  snprintf(cats[0], sizeof cats[0], "%s", "Abertos");
  snprintf(cats[1], sizeof cats[1], "%s", "Esportes");
  nCats = 2;
  catIni[0] = 0; catN[0] = 6;
  catIni[1] = 6; catN[1] = 2;
  nFavOrd = 0;
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-guia";
  char nome[600];
  SDL_Window *w;
  SDL_GLContext gl;

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
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");

  poeCanal(0, "Canal Recortado HD", "tests/fixtures/logos/recortado.png");
  poeCanal(1, "Canal Recortado HD", "tests/fixtures/logos/recortado.png");
  poeCanal(2, "Canal Com Fundo HD", "tests/fixtures/logos/comfundo.png");
  poeCanal(3, "Canal Com Fundo HD", "tests/fixtures/logos/comfundo.png");
  nCanais = 4;
  snprintf(cats[0], sizeof cats[0], "%s", "Aberta");
  nCats = 1; catIni[0] = 0; catN[0] = 4;

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

  focoLin = 0; focoCol = 1; rolL = 0.0f; focoAnelOk = 0;
  focoTopo = 1; topoCol = G_TOPO_PREVIEW; animTopo[G_TOPO_PREVIEW] = 1.0f;
  snprintf(nome, sizeof nome, "%s-lista-topo.bmp", saida);
  capturaTela(nome, w, 1);
  focoTopo = 0; animTopo[G_TOPO_PREVIEW] = 0.0f;

  janelaDesl = 60; focoAnelOk = 0;
  snprintf(nome, sizeof nome, "%s-lista-adiante.bmp", saida);
  capturaTela(nome, w, 1);
  janelaDesl = 0;

  modoLista = 0; focoLin = 0; focoCol = 0; focoAnelOk = 0;
  snprintf(nome, sizeof nome, "%s-tela.bmp", saida);
  capturaTela(nome, w, 1);

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
    int k, nc = (int)(sizeof NC / sizeof NC[0]);
    for (k = 0; k < nc; k++) {
      snprintf(cats[k], sizeof cats[k], "%s", NC[k]);
      catIni[k] = k * 2 > 6 ? 6 : k * 2; catN[k] = k % 3 == 0 ? 117 : 4 + k;
    }
    // So o painel le catN; a grade nao e desenhada com foco nela.
    nCats = nc; }
  modoLista = 1; focoLin = 0; focoCol = 0; focoAnelOk = 0;
  catAberto = 2; catFoco = 14; catAnim = 1.0f;
  { float areaH = G_CAT_BASE - G_CAT_TOPO;
    catRol = (float)catFoco * G_CAT_ROW - (areaH - G_CAT_ROW) * 0.5f;
    if (catRol > (float)nCats * G_CAT_ROW - areaH) catRol = (float)nCats * G_CAT_ROW - areaH;
    if (catRol < 0.0f) catRol = 0.0f; }
  focoLin = 12;
  snprintf(nome, sizeof nome, "%s-lista-categorias.bmp", saida);
  capturaTela(nome, w, 1);
  catAberto = 0; catAnim = 0.0f; focoLin = 0;

  nCanais = 0; nCats = 0; nFontes = 0; fontesOk = 1; estado = G_FALHOU;
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
