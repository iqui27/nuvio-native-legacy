// CAPTURA DA MODAL "ENCONTRAR PESSOAS", sem rede. recomenda.c e pessoas.c sao
// INCLUIDOS: os achados, o cartao e os pedidos moram em estaticos do modulo e
// semea-los por dentro e o unico jeito de ter as telas cheias sem servidor.
//
// Cada estado do redesenho de 06/10 sai numa imagem: primeira abertura (com e
// sem pedidos), teclado, carregando, resultados com o foco na acao e na
// pessoa, enviando, enviado, aceito, falha da acao, lista sem rede, busca
// vazia, cartao do criador e de um estranho, comunidade rolada, pedidos e Meu
// perfil. O idioma vem de PESSOAS_IDIOMA (IDIOMA_*; padrao 0 = pt) e o
// prefixo dos arquivos de PESSOAS_PREFIXO (padrao "after").
#define NV_REC_URL "http://127.0.0.1:1"
#include "../src/recomenda.c"
#include "../src/pessoas.c"
#include "dados.h"
#include <SDL2/SDL_image.h>
#include <assert.h>

static GLuint fbo, fboTex;

static void captura(const char *nome, SDL_Window *win) {
  int i;
  for (i = 0; i < 40; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    gfx_novo_quadro();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, 1920, 1080);
    glClearColor(0.025f, 0.025f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    pessoas_atualizar(1.0f / 30.0f, SDL_GetTicks());
    pessoas_desenhar(SDL_GetTicks());
    if (i == 39) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s;
      int y;
      assert(pix);
      glFinish();
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

static void pessoa(RecPessoa *p, const char *pub, const char *nome, const char *ap,
                   const char *bio, unsigned g, const char *rel, int comum, const char *selo) {
  memset(p, 0, sizeof *p);
  snprintf(p->pub, sizeof p->pub, "%s", pub);
  snprintf(p->nome, sizeof p->nome, "%s", nome);
  snprintf(p->apelido, sizeof p->apelido, "%s", ap);
  snprintf(p->bio, sizeof p->bio, "%s", bio);
  snprintf(p->relacao, sizeof p->relacao, "%s", rel);
  snprintf(p->selo, sizeof p->selo, "%s", selo);
  p->generos = g; p->emComum = comum;
}

static const char *saida, *prefixo;
static SDL_Window *janela;
static void shot(const char *estado) {
  char nome[600];
  snprintf(nome, sizeof nome, "%s/%s-%s.bmp", saida, prefixo, estado);
  captura(nome, janela);
}

static void tecla(SDL_Keycode k) {
  SDL_Event ev;
  memset(&ev, 0, sizeof ev);
  ev.type = SDL_KEYDOWN; ev.key.keysym.sym = k;
  pessoas_evento(&ev);
}

// Os resultados de "fabi": uma relacao de cada tipo, o criador, nome + apelido,
// so apelido, e um nome que o servidor antigo mandava como "Amigo #343".
static void semearBusca(void) {
  pessoa(&achados[0], "k9ptiwtmrb", "", "fabi cine", "fã de terror e ficção", 0, "", 0, "");
  pessoa(&achados[1], "m3n4p5q6r7", "", "fabio filmes", "", 0, "enviado", 0, "");
  pessoa(&achados[2], "a2b3c4d5e6", "Fabrício Andrade", "fabricio", "só drama coreano", 0, "amigo", 0, "");
  pessoa(&achados[3], "f7g8h9i2j3", "", "fabiana lima", "maratonista de séries", 0, "recebido", 0, "");
  pessoa(&achados[4], "iqui270000", "Henrique Rocha", "fabi do iqui", "", 0, "amigo", 0, "criador");
  pessoa(&achados[5], "zz34567890", "", "Amigo #343", "", 0, "", 0, "");
  nAchados = 6; achadosOrigem = 1;
  snprintf(ultimaBusca, sizeof ultimaBusca, "fabi");
}

int main(int argc, char **argv) {
  const char *dir = getenv("NUVIO_DADOS");
  const char *idi = getenv("PESSOAS_IDIOMA");
  SDL_GLContext gl;
  int k;
  saida = argc > 1 ? argv[1] : "/tmp/nv-amigos-shots";
  prefixo = getenv("PESSOAS_PREFIXO") ? getenv("PESSOAS_PREFIXO") : "after";
  assert(dir && *dir);
  { char cam[600]; FILE *f;
    snprintf(cam, sizeof cam, "%s/ajustes.txt", dir);
    f = fopen(cam, "w"); assert(f);
    fprintf(f, "idioma %d\nidiomaAutoLocal 0\n", idi ? atoi(idi) : 0);
    // PESSOAS_TEMA=<indice de TEMA_ACENTO> (2 = Oceano): um acento fixo, para
    // ver o selo e as pilulas em cor. Sem ele vale a migracao da 2.0 (Da arte,
    // que sem arte na tela e o acento claro).
    if (getenv("PESSOAS_TEMA") && *getenv("PESSOAS_TEMA")) {
      fprintf(f, "selected_theme %d\n", atoi(getenv("PESSOAS_TEMA")));
      fclose(f);
      snprintf(cam, sizeof cam, "%s/aparencia-20.txt", dir);
      f = fopen(cam, "w"); assert(f); fprintf(f, "1\n");
    }
    fclose(f); }
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  dados_iniciar(dir);
  assert(!strcmp(dados_dir(), dir));
  ajustes_iniciar();
  ajustes_dir(dir);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Nuvio: captura pessoas", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(janela);
  gl = SDL_GL_CreateContext(janela);
  assert(gl);
  glGenTextures(1, &fboTex);
  glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  if (!mtx) mtx = SDL_CreateMutex();

  memset(&perfil, 0, sizeof perfil);
  snprintf(perfil.apelido, sizeof perfil.apelido, "henrique tv");
  snprintf(perfil.bio, sizeof perfil.bio, "cinema, séries e muita tela grande");
  perfil.publicado = 1; perfil.foto = 0; perfil.recentes = 1; perfil.ativ = 1;
  perfil.generos = (1u << 10) | (1u << 6);

  /* 1. PRIMEIRA ABERTURA de quem ainda nao ligou nada: sem pedidos, sem perfil. */
  { RecPerfil vazio; memset(&vazio, 0, sizeof vazio); RecPerfil guard = perfil; perfil = vazio;
    rascVivo = 0; nPedidos = 0;
    pessoas_abrir();
    shot("primeira-abertura");
    perfil = guard; rascVivo = 0; }

  /* 2. ABERTURA COM PEDIDOS: dois pedidos no topo, um deles de quem tem nome. */
  pessoa(&pedidosRec[0], "aaaaaaaaaa", "", "gui nerd", "", 0, "recebido", 0, "");
  pessoa(&pedidosRec[1], "bbbbbbbbbb", "", "helena tv", "séries policiais e café", 0, "recebido", 0, "");
  nPedidos = 2;
  pessoas_abrir();
  shot("menu");
  tecla(SDLK_DOWN);
  shot("menu-pedido-foco");

  /* 3. DIGITANDO A BUSCA: o teclado de sempre por cima. */
  irPara(PG_MENU); foco = 0;
  abrirTeclado(A_BUSCAR);
  shot("teclado");
  { SDL_Event ev; memset(&ev, 0, sizeof ev); ev.type = SDL_KEYDOWN; ev.key.keysym.sym = SDLK_ESCAPE;
    teclado_evento(&ev);
    for (k = 0; k < 60; k++) teclado_atualizar(1.0f / 30.0f, SDL_GetTicks()); }
  tecladoPara = 0;

  /* 4. CARREGANDO: a pagina da busca com o esqueleto. */
  snprintf(ultimaBusca, sizeof ultimaBusca, "fabi");
  irPara(PG_LISTA); listaOrigem = 1; listaCarregando = 1; listaErro = 0;
  shot("carregando");

  /* 5. RESULTADOS, com o foco na primeira pessoa (a pilula Adicionar acesa). */
  semearBusca();
  listaCarregando = 0; foco = 1; coluna = 1;
  shot("resultados");
  /* foco na PESSOA (← na pilula) */
  tecla(SDLK_LEFT);
  shot("resultados-foco-perfil");
  tecla(SDLK_RIGHT);

  /* 6. OK EM ADICIONAR: enviando (anel), depois enviado (aceso) e o rodape. */
  opAtual = A_PEDIR; snprintf(acaoPub, sizeof acaoPub, "%s", achados[0].pub);
  shot("enviando");
  snprintf(achados[0].relacao, sizeof achados[0].relacao, "enviado");
  socEstado = REC_SOC_OK;
  pessoas_atualizar(1.0f / 30.0f, SDL_GetTicks());
  shot("enviado");

  /* 7. ACEITAR o pedido de "fabiana lima": aceito, a pilula vira Amigos. */
  foco = 4; coluna = 1;
  opAtual = A_ACEITAR; snprintf(acaoPub, sizeof acaoPub, "%s", achados[3].pub);
  snprintf(achados[3].relacao, sizeof achados[3].relacao, "amigo");
  socEstado = REC_SOC_OK;
  pessoas_atualizar(1.0f / 30.0f, SDL_GetTicks());
  shot("aceito");

  /* 8. A ACAO FALHOU (sem rede): a pilula vira "Tentar de novo" e o rodape diz. */
  semearBusca(); irPara(PG_LISTA); listaOrigem = 1; foco = 3; coluna = 1;
  feitoPub[0] = 0;
  opAtual = A_PEDIR; snprintf(acaoPub, sizeof acaoPub, "%s", achados[2].pub);
  pessoa(&achados[2], "a2b3c4d5e6", "Fabrício Andrade", "fabricio", "só drama coreano", 0, "", 0, "");
  socEstado = REC_SOC_FALHA;
  pessoas_atualizar(1.0f / 30.0f, SDL_GetTicks());
  shot("erro-acao");

  /* 9. A LISTA NAO CARREGOU (sem rede) */
  aviso[0] = 0; acaoPub[0] = 0; acaoFalhou = 0;
  irPara(PG_LISTA); listaOrigem = 3; listaCarregando = 0; listaErro = 1;
  shot("erro-offline");

  /* 10. BUSCA SEM NINGUEM */
  nAchados = 0; achadosOrigem = 1;
  irPara(PG_LISTA); listaOrigem = 1; listaErro = 0;
  snprintf(ultimaBusca, sizeof ultimaBusca, "zzzq");
  shot("vazio");

  /* 11. CARTAO DO CRIADOR (amigo: nome, apelido e o selo) */
  pessoa(&cartaoP, "iqui270000", "Henrique Rocha", "iqui",
         "fiz o nuvio native. cinema, séries e muita tela grande", (1u << 13) | (1u << 9) | (1u << 6),
         "amigo", 0, "criador");
  temCartao = 1;
  snprintf(cartaoRec[0], sizeof cartaoRec[0], "Um Sonho de Liberdade");
  snprintf(cartaoRec[1], sizeof cartaoRec[1], "Origem");
  snprintf(cartaoRec[2], sizeof cartaoRec[2], "Duna: Parte Dois");
  nCartaoRec = 3;
  irPara(PG_CARTAO);
  shot("cartao-criador");

  /* 12. CARTAO DE UM ESTRANHO (so apelido) */
  pessoa(&cartaoP, "k9ptiwtmrb", "", "fabi cine", "fã de terror e ficção científica. maratonista de fim de semana",
         (1u << 13) | (1u << 9), "", 0, "");
  nCartaoRec = 0;
  irPara(PG_CARTAO);
  shot("cartao-estranho");
  snprintf(cartaoP.relacao, sizeof cartaoP.relacao, "enviado");
  irPara(PG_CARTAO);
  shot("cartao-enviado");

  /* 13. COMUNIDADE: 12 perfis, o criador entre eles, rolada ate o fim. */
  { static const char *ap[12] = { "fabi cine", "gui nerd", "helena tv", "marcos 4k",
      "ana series", "joao terror", "iqui", "leo docs", "carla k drama",
      "rafa maratona", "nina classicos", "tito sci fi" };
    static const char *no[12] = { "", "Guilherme Souza", "", "", "", "", "Henrique Rocha", "",
      "", "", "", "" };
    static const char *vi[12] = { "Silo", "", "Duna: Parte Dois", "Project Hail Mary", "",
      "Hereditário", "Frieren", "", "Pousando no Amor", "Ruptura", "", "Andor" };
    static const char *rel[12] = { "", "amigo", "", "enviado", "", "recebido", "amigo", "", "", "", "", "" };
    for (k = 0; k < 12; k++) {
      char pub[12];
      snprintf(pub, sizeof pub, "c%09d", k);
      pessoa(&achados[k], pub, no[k], ap[k], k == 7 ? "só documentário" : "", 0, rel[k], 0,
             k == 6 ? "criador" : "");
      snprintf(achados[k].vendo, sizeof achados[k].vendo, "%s", vi[k]);
    }
    nAchados = 12; achadosOrigem = 3; comMais = 1; comPagina = 0; }
  irPara(PG_LISTA); listaOrigem = 3;
  shot("comunidade");
  foco = 6; coluna = 0;
  shot("comunidade-criador");
  foco = 12;
  shot("comunidade-mais");

  /* 13b. HOMONIMOS (#202): duas pessoas diferentes chamadas "marina" ganham
     a pista do handle; quem tem nome unico continua sem pista. */
  pessoa(&achados[0], "k7q2abcdef", "", "marina", "mae", 0, "", 0, "");
  pessoa(&achados[1], "p9x4ghijkm", "", "Marina", "filha", 0, "", 0, "");
  pessoa(&achados[2], "r5t6npqrst", "", "marina luz", "", 0, "", 0, "");
  nAchados = 3; achadosOrigem = 1;
  irPara(PG_LISTA); listaOrigem = 1; listaErro = 0; listaCarregando = 0;
  snprintf(ultimaBusca, sizeof ultimaBusca, "marina");
  montar();
  { int n = 0, i;
    for (i = 0; i < nL; i++) if (linhas[i].tipo == T_PESSOA) {
      if (!strcmp(linhas[i].p.pub, "k7q2abcdef")) { assert(!strcmp(linhas[i].dica, "#k7q2")); n++; }
      if (!strcmp(linhas[i].p.pub, "p9x4ghijkm")) { assert(!strcmp(linhas[i].dica, "#p9x4")); n++; }
      if (!strcmp(linhas[i].p.pub, "r5t6npqrst")) { assert(!linhas[i].dica[0]); n++; }
    }
    assert(n == 3); }
  shot("homonimos");

  /* 13c. GOSTO PARECIDO (#202): cada linha diz POR QUE a pessoa foi sugerida.
     lerPessoa e o parser real da resposta de /v1/perfis/sugeridos; o ultimo
     perfil vem de um servidor ANTIGO (sem "motivo") e cai na bio, como antes. */
  { static const char *json[5] = {
      "{\"pub\":\"g000000001\",\"apelido\":\"fabi cine\",\"bio\":\"fa de terror\",\"generos\":[\"terror\"],"
        "\"relacao\":\"\",\"emComum\":4,\"motivo\":{\"tipo\":\"titulos\",\"n\":4,\"generos\":[\"terror\"]}}",
      "{\"pub\":\"g000000002\",\"apelido\":\"helena tv\",\"bio\":\"\",\"generos\":[\"terror\",\"ficcao\",\"drama\"],"
        "\"relacao\":\"\",\"emComum\":0,\"motivo\":{\"tipo\":\"generos\",\"generos\":[\"terror\",\"ficcao\"]}}",
      "{\"pub\":\"g000000003\",\"apelido\":\"marcos 4k\",\"bio\":\"\",\"generos\":[],"
        "\"relacao\":\"\",\"emComum\":0,\"motivo\":{\"tipo\":\"ativos\"}}",
      "{\"pub\":\"g000000004\",\"apelido\":\"ana series\",\"bio\":\"maratonista de series\",\"generos\":[\"drama\"],"
        "\"relacao\":\"\",\"emComum\":0}",
      "{\"pub\":\"g000000005\",\"apelido\":\"leo docs\",\"bio\":\"\",\"generos\":[\"documentario\"],"
        "\"relacao\":\"\",\"emComum\":0,\"motivo\":{\"tipo\":\"inventado\"}}" };
    for (k = 0; k < 5; k++) assert(lerPessoa(json[k], json[k] + strlen(json[k]), &achados[k]));
    assert(achados[0].motivo == REC_MOTIVO_TITULOS && achados[0].emComum == 4);
    assert(achados[1].motivo == REC_MOTIVO_GENEROS && achados[1].motivoGeneros == ((1u << 13) | (1u << 9)));
    assert(achados[1].generos != achados[1].motivoGeneros);   /* nao confunde os generos do cartao */
    assert(achados[2].motivo == REC_MOTIVO_ATIVOS);
    assert(achados[3].motivo == REC_MOTIVO_NENHUM && achados[4].motivo == REC_MOTIVO_NENHUM);
    nAchados = 4; achadosOrigem = 2; }
  irPara(PG_LISTA); listaOrigem = 2; listaErro = 0; listaCarregando = 0;
  montar();
  { char c[160]; int vistos = 0, i;
    for (i = 0; i < nL; i++) if (linhas[i].tipo == T_PESSOA) {
      contexto(c, sizeof c, &linhas[i].p);
      if (!strcmp(linhas[i].p.pub, "g000000001")) { assert(!strcmp(c, "4 títulos em comum")); vistos++; }
      if (!strcmp(linhas[i].p.pub, "g000000002")) { assert(!strcmp(c, "Também gosta de Ficção científica, Terror")); vistos++; }
      if (!strcmp(linhas[i].p.pub, "g000000003")) { assert(!strcmp(c, "Perfil ativo na comunidade")); vistos++; }
      if (!strcmp(linhas[i].p.pub, "g000000004")) { assert(!strcmp(c, "maratonista de series")); vistos++; }
    }
    assert(vistos == 4); }
  shot("gosto");
  nAchados = 0;
  irPara(PG_LISTA); listaOrigem = 2;
  shot("gosto-vazio");

  /* 14. PEDIDOS */
  irPara(PG_PEDIDOS);
  shot("pedidos");

  /* 15. MEU PERFIL */
  rasc = perfil; rascVivo = 1;
  irPara(PG_PERFIL);
  shot("meu-perfil");

  printf("pronto\n");
  return 0;
}
