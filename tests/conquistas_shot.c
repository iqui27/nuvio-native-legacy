// Captura e regras da tela Conquistas (2.1) em 1920x1080: hub, aba, familias,
// detalhe de um emblema (cinco niveis) e o aviso "depois dos creditos". Janela GL
// ESCONDIDA e desenho num FBO, como tests/comecar_shot.c.
//
// Antes das capturas confere, so por evento de tecla (o que app_evento entrega):
//   layout:  todo alvo focavel cabe nos 1920x1080 e nenhum cobre outro;
//   D-pad:   uma varredura de setas alcanca todo alvo do hub, e o foco nunca cai
//            fora da lista de alvos do quadro;
//   Voltar:  detalhe -> hub, familias -> hub, hub fecha;
//   aviso:   armar nao mexe no foco, setas e OK passam direto, so o Voltar e
//            dele (com debounce da rajada), e some depois de mostrado;
//   Perfil:  de cada secao a seta para cima chega ao botao Conquistas, OK pede a
//            tela, baixo volta, esquerda sai.
//
// ONDE ESCREVE. dados_dir() tem de ser a pasta descartavel que
// tests/conquistas_shot.sh exporta (NUVIO_DADOS); fora dela o teste recusa rodar.
#include "conquistas.h"
#include "conquistasdados.h"
#include "conquistasregra.h"
#include "perfil.h"
#include "ajustes.h"
#include "dados.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#endif

static GLuint fbo, fboTex;

static void evento(int tipo, SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = tipo;
  e.key.keysym.sym = k;
  conquistas_evento(&e);
}

static void tecla(SDL_Keycode k) { evento(SDL_KEYDOWN, k); evento(SDL_KEYUP, k); }

// Voltar como a TV manda: descida, soltura e uma pausa maior que o debounce.
static void voltar(void) {
  evento(SDL_KEYDOWN, SDLK_ESCAPE); evento(SDL_KEYUP, SDLK_ESCAPE);
  SDL_Delay(300);
}

static void quadro(float dt) {
  txt_novo_quadro();
  tex_novo_quadro();
  tex_bombear(6);
  gfx_novo_quadro();
  conquistas_atualizar(dt, SDL_GetTicks());
  glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  // Fundo de mentira: a tela do app por baixo (importa para o aviso).
  gfx_cor((GfxRect){ 200, 200, 900, 560 }, 0.02f, 0.02f, 0.30f, 0.50f, 1.0f);
  gfx_cor((GfxRect){ 1100, 500, 600, 400 }, 0.02f, 0.40f, 0.20f, 0.30f, 1.0f);
  if (conquistas_aberto()) conquistas_desenhar(SDL_GetTicks());
  else conquistas_desenhar(SDL_GetTicks());   // o esmaecer de saida
  conquistas_aviso_desenhar();
}

static void adiantar(int n) {
  int i;
  for (i = 0; i < n; i++) quadro(1.0f / 60.0f);
}

static void gravar(const char *nome) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  int y;
  assert(pix);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(IMG_SavePNG(s, nome) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", nome);
}

static void captura(const char *prefixo, const char *nome, int quadros) {
  char caminho[700];
  adiantar(quadros);
  SDL_Delay(200);   // os icones sobem por tex_bombear: da tempo aos fios de decode
  adiantar(10);
  snprintf(caminho, sizeof caminho, "%s-%s.png", prefixo, nome);
  gravar(caminho);
}

// ---------------------------------------------------------------- estado
static void semear(void) {
  CqEstado s;
  memset(&s, 0, sizeof s);
  cqregra_registrar(&s, "dir:nolan", CQ_FAM_DIRETOR, "Christopher Nolan", cqregra_nivel(7, 12, 0), 7, 12, 1700000000L);
  cqregra_registrar(&s, "dir:kubrick", CQ_FAM_DIRETOR, "Stanley Kubrick", cqregra_nivel(13, 13, 1), 13, 13, 1700100000L);
  cqregra_registrar(&s, "trilha:ghibli", CQ_FAM_TRILHA, "Estúdio Ghibli", cqregra_nivel(14, 22, 0), 14, 22, 1700200000L);
  cqregra_registrar(&s, "trilha:cult-80s", CQ_FAM_TRILHA, "Cults dos anos 80", cqregra_nivel(3, 20, 0), 3, 20, 1700300000L);
  cqregra_registrar(&s, "trilha:noir", CQ_FAM_TRILHA, "Film noir clássico", cqregra_nivel(9, 12, 0), 9, 12, 1700400000L);
  cqregra_registrar(&s, "top:250", CQ_FAM_EXPLORADOR, "Top 250 do Trakt", cqregra_nivel_top(60), 60, 250, 1700500000L);
  cqregra_registrar(&s, "saga:matrix", CQ_FAM_TRILHA, "Saga Matrix", cqregra_nivel(4, 4, 1), 4, 4, 1700600000L);
  cqregra_registrar(&s, "ator:tilda", CQ_FAM_EXPLORADOR, "Tilda Swinton", cqregra_nivel(2, 10, 0), 2, 10, 1700700000L);
  cqregra_registrar(&s, "trilha:fantasia", CQ_FAM_TRILHA, "Fantasia sem pressa", 0, 0, 15, 0);
  cqregra_marcar_avisados(&s);   // nada pendente: o aviso so abre quando o teste pede
  assert(cqd_gravar(&s));
}

// Sobe um nivel de uma entrada: deixa um aviso pendente.
static void subir(const char *chave, int vistos) {
  CqEstado s;
  const CqEntrada *e;
  cqd_carregar(&s);
  e = cqregra_buscar(&s, chave);
  assert(e);
  assert(cqregra_registrar(&s, chave, e->familia, e->nome, cqregra_nivel(vistos, e->total, 0), vistos, e->total, 1710000000L) > 0);
  assert(cqd_gravar(&s));
}

// ---------------------------------------------------------------- layout
static void conferir_layout(const char *onde) {
  int ids[64], n, i, j;
  GfxRect rs[64];
  n = conquistas_teste_itens(ids, rs, 64);
  assert(n > 0);
  for (i = 0; i < n; i++) {
    if (rs[i].x < 0 || rs[i].y < 0 || rs[i].x + rs[i].w > 1920.5f || rs[i].y + rs[i].h > 1080.5f ||
        rs[i].w < 40 || rs[i].h < 40) {
      printf("FALHA (%s): alvo %d fora da tela ou pequeno (%.0f,%.0f %.0fx%.0f)\n", onde, ids[i], rs[i].x, rs[i].y, rs[i].w, rs[i].h);
      exit(1);
    }
    for (j = i + 1; j < n; j++) {
      float ix = (rs[i].x + rs[i].w < rs[j].x + rs[j].w ? rs[i].x + rs[i].w : rs[j].x + rs[j].w) -
                 (rs[i].x > rs[j].x ? rs[i].x : rs[j].x);
      float iy = (rs[i].y + rs[i].h < rs[j].y + rs[j].h ? rs[i].y + rs[i].h : rs[j].y + rs[j].h) -
                 (rs[i].y > rs[j].y ? rs[i].y : rs[j].y);
      if (ix > 1.0f && iy > 1.0f) {
        printf("FALHA (%s): alvos %d e %d se cobrem\n", onde, ids[i], ids[j]);
        exit(1);
      }
    }
  }
}

static int no_quadro(int foco) {
  int ids[64], n, i;
  GfxRect rs[64];
  n = conquistas_teste_itens(ids, rs, 64);
  for (i = 0; i < n; i++) if (ids[i] == foco) return 1;
  return 0;
}

// Setas pseudo-aleatorias (semente fixa): o foco tem de estar sempre num alvo
// do quadro, e a varredura tem de passar por todos os alvos do hub inicial.
static void varrer_hub(void) {
  int ids[64], vistos[64] = { 0 }, n, i, k;
  GfxRect rs[64];
  unsigned long x = 12345;
  static const SDL_Keycode T[4] = { SDLK_LEFT, SDLK_RIGHT, SDLK_UP, SDLK_DOWN };
  conquistas_abrir();
  adiantar(30);
  n = conquistas_teste_itens(ids, rs, 64);
  for (k = 0; k < 800; k++) {
    x = x * 1103515245UL + 12345UL;
    tecla(T[(x >> 16) & 3]);
    assert(conquistas_vista() == CONQUISTAS_VISTA_HUB);
    assert(no_quadro(conquistas_foco()));
    for (i = 0; i < n; i++) if (ids[i] == conquistas_foco()) vistos[i] = 1;
  }
  for (i = 0; i < n; i++) {
    if (!vistos[i]) { printf("FALHA: varredura D-pad nunca chegou ao alvo %d\n", ids[i]); exit(1); }
  }
}

// ---------------------------------------------------------------- Perfil
static void perfil_tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  perfil_evento(&e);
  e.type = SDL_KEYUP;
  perfil_evento(&e);
}

static void perfil_dados(PerfilDados *d) {
  static const unsigned short ATIV[30] = { 0,2,0,1,0,0,3, 1,0,0,0,4,1,0, 0,1,1,0,7,2,0, 0,0,1,2,3,1, 2,1,1 };
  static const char *TIT[4] = { "Pluribus", "A Casa do Dragão", "Duna: Parte Dois", "Andor" };
  int i;
  memset(d, 0, sizeof *d);
  snprintf(d->nome, sizeof d->nome, "Teste");
  snprintf(d->usuario, sizeof d->usuario, "teste");
  snprintf(d->periodo, sizeof d->periodo, "SETEMBRO 2026");
  d->minutos = 2384; d->plays = 41; d->filmes = 6; d->episodios = 35;
  d->nDias = 30; d->primeiroDiaSemana = 2;
  for (i = 0; i < 30; i++) { d->atividade[i] = ATIV[i]; if (ATIV[i]) d->diasAtivosMes++; }
  d->nDestaques = PERFIL_MAX_DESTAQUES;
  for (i = 0; i < d->nDestaques; i++) {
    snprintf(d->destaques[i].id, sizeof d->destaques[i].id, "tt000000%d", i);
    snprintf(d->destaques[i].titulo, sizeof d->destaques[i].titulo, "%s", TIT[i % 4]);
    d->destaques[i].plays = 9 - i;
  }
}

static void perfil_quadros(int n) {
  int i;
  for (i = 0; i < n; i++) {
    txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6); gfx_novo_quadro();
    perfil_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(0.051f, 0.051f, 0.051f, 1.0f); glClear(GL_COLOR_BUFFER_BIT);
    perfil_desenhar(SDL_GetTicks());
  }
}

static void perfil_abrir_com_dados(void) {
  PerfilDados d;
  perfil_fechar();
  perfil_abrir();
  perfil_dados(&d);
  perfil_definir_dados(&d);
  perfil_quadros(60);
}

static void regras_perfil(const char *prefixo) {
  // Direitas antes da seta para cima: 0 = calendario, 31 = fim do calendario
  // (destaques), 60 = tudo que houver a direita (amigos).
  static const int DIREITAS[3] = { 0, 31, 60 };
  int s, i;
  perfil_iniciar();
  for (s = 0; s < 3; s++) {
    perfil_abrir_com_dados();
    assert(!perfil_foco_conquistas());
    for (i = 0; i < DIREITAS[s]; i++) perfil_tecla(SDLK_RIGHT);
    for (i = 0; i < 8 && !perfil_foco_conquistas(); i++) perfil_tecla(SDLK_UP);
    if (!perfil_foco_conquistas()) { printf("FALHA: seta para cima nao chega ao botao Conquistas (secao %d)\n", s); exit(1); }
    // Baixo volta a secao de onde veio.
    perfil_tecla(SDLK_DOWN);
    assert(!perfil_foco_conquistas());
    // OK pede a tela, uma vez so.
    for (i = 0; i < 8 && !perfil_foco_conquistas(); i++) perfil_tecla(SDLK_UP);
    assert(perfil_foco_conquistas());
    assert(!perfil_pediu_conquistas());
    perfil_tecla(SDLK_RETURN);
    assert(perfil_pediu_conquistas());
    assert(!perfil_pediu_conquistas());
    // Esquerda escapa da tela inteira.
    assert(!perfil_quer_sair());
    perfil_tecla(SDLK_LEFT);
    assert(perfil_quer_sair());
  }
  // Conquistas locais continuam acessiveis sem resumo do tracker.
  {
    static const PerfilEstado estados[] = { PERFIL_ESTADO_CARREGANDO,
      PERFIL_ESTADO_DESCONECTADO, PERFIL_ESTADO_PRIVADO, PERFIL_ESTADO_INDISPONIVEL };
    for (unsigned j = 0; j < sizeof estados / sizeof estados[0]; j++) {
      perfil_definir_dados(NULL);
      perfil_abrir();
      perfil_definir_estado(estados[j], NULL);
      perfil_tecla(SDLK_UP);
      assert(perfil_foco_conquistas());
      perfil_tecla(SDLK_RETURN);
      assert(perfil_pediu_conquistas() && !perfil_pediu_atualizar());
      perfil_tecla(SDLK_DOWN);
      assert(!perfil_foco_conquistas());
      perfil_tecla(SDLK_RETURN);
      assert(perfil_pediu_atualizar() == (estados[j] != PERFIL_ESTADO_CARREGANDO));
    }
  }
  // Captura com o botao em foco.
  perfil_abrir_com_dados();
  for (i = 0; i < 8 && !perfil_foco_conquistas(); i++) perfil_tecla(SDLK_UP);
  assert(perfil_foco_conquistas());
  perfil_quadros(90);
  SDL_Delay(400);
  perfil_quadros(10);
  { char nome[700]; snprintf(nome, sizeof nome, "%s-perfil.png", prefixo); gravar(nome); }
  perfil_fechar();
}

int main(int argc, char **argv) {
  const char *prefixo = argc > 1 ? argv[1] : "/tmp/nuvio-conquistas";
  const char *dir = getenv("NUVIO_DADOS");
  SDL_Window *w;
  SDL_GLContext gl;
  int i, ids[64];
  GfxRect rs[64];
  SDL_Event e;
  FILE *f;
  char caminho[700];

  if (!dir || !dir[0]) return 2;
  dados_iniciar(NULL);
  if (!dados_dir() || strcmp(dados_dir(), dir)) {
    fprintf(stderr, "conquistas_shot: recusando rodar fora da pasta descartavel (dados_dir=%s)\n",
            dados_dir() ? dados_dir() : "(nenhuma)");
    return 1;
  }
  // Portugues na interface (o padrao de fabrica e o ingles).
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dados_dir());
  f = fopen(caminho, "w");
  assert(f);
  fputs("idioma 0\nselected_theme 2\n", f);
  fclose(f);
  ajustes_dir(dados_dir());

  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("conquistas-shot", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  glGenTextures(1, &fboTex);
  glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_tex_esquecer(0);
  gfx_icones_dir("deploy/app/art");

  assert(ajustes_conquistas() && ajustes_conquistas_aviso());
  semear();

  // ---- Hub: layout, varredura D-pad, captura.
  conquistas_teste_esquecer();
  conquistas_abrir();
  adiantar(30);
  assert(conquistas_aberto() && conquistas_vista() == CONQUISTAS_VISTA_HUB);
  assert(conquistas_filtradas() == 9);
  conferir_layout("hub");
  varrer_hub();
  conquistas_teste_esquecer();
  conquistas_abrir();
  tecla(SDLK_DOWN);   // do primeiro filtro ao primeiro cartao
  assert(conquistas_foco() >= 100);
  captura(prefixo, "hub", 60);

  // ---- Aba "Em andamento" (so o que ainda falta).
  conquistas_teste_esquecer();
  conquistas_abrir();
  tecla(SDLK_RIGHT); tecla(SDLK_RIGHT);
  tecla(SDLK_RETURN);
  adiantar(2);   // o filtro refaz no quadro
  assert(conquistas_aba() == 2);
  assert(conquistas_filtradas() > 0 && conquistas_filtradas() < 9);
  conferir_layout("hub-andamento");
  captura(prefixo, "andamento", 30);

  // ---- Familias: abre pelo botao do topo, Voltar volta ao hub.
  conquistas_teste_esquecer();
  conquistas_abrir();
  // O botao fica no topo, a direita: anda ate ele pelas setas (semente fixa).
  { unsigned long x = 777;
    static const SDL_Keycode T[4] = { SDLK_LEFT, SDLK_RIGHT, SDLK_UP, SDLK_DOWN };
    for (i = 0; i < 400 && conquistas_foco() != 2; i++) { x = x * 1103515245UL + 12345UL; tecla(T[(x >> 16) & 3]); }
    assert(conquistas_foco() == 2); }   // B_FAMS
  tecla(SDLK_RETURN);
  assert(conquistas_vista() == CONQUISTAS_VISTA_FAMILIAS);
  conferir_layout("familias");
  assert(no_quadro(conquistas_foco()));
  captura(prefixo, "familias", 30);
  voltar();
  assert(conquistas_aberto() && conquistas_vista() == CONQUISTAS_VISTA_HUB);

  // ---- Detalhe: OK num cartao, cinco niveis; Voltar volta ao mesmo cartao.
  conquistas_teste_esquecer();
  conquistas_abrir();
  tecla(SDLK_DOWN);
  { int antes = conquistas_foco();
    tecla(SDLK_RETURN);
    assert(conquistas_vista() == CONQUISTAS_VISTA_DETALHE);
    conferir_layout("detalhe");
    captura(prefixo, "detalhe", 30);
    voltar();
    assert(conquistas_vista() == CONQUISTAS_VISTA_HUB && conquistas_foco() == antes); }
  // Voltar no hub fecha a tela.
  voltar();
  assert(!conquistas_aberto());

  // ---- Aviso depois dos creditos.
  conquistas_teste_esquecer();
  assert(!conquistas_aviso_aberto());
  conquistas_aviso_armar();               // nada pendente: nao abre
  assert(!conquistas_aviso_aberto());
  subir("dir:nolan", 9);                  // 9 de 12 = 75%: nivel 3
  conquistas_aviso_armar();
  assert(conquistas_aviso_aberto());
  captura(prefixo, "aviso", 30);
  // Nao toma o foco: setas e OK passam direto, o foco do app segue onde estava.
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = SDLK_DOWN;
  assert(!conquistas_aviso_evento(&e));
  e.key.keysym.sym = SDLK_RETURN;
  assert(!conquistas_aviso_evento(&e));
  assert(conquistas_aviso_aberto());
  // Voltar: o aviso fecha e consome a tecla, a soltura e a rajada logo depois.
  e.key.keysym.sym = SDLK_ESCAPE;
  assert(conquistas_aviso_evento(&e));
  assert(!conquistas_aviso_aberto());
  e.type = SDL_KEYUP;
  assert(conquistas_aviso_evento(&e));
  e.type = SDL_KEYDOWN;
  assert(conquistas_aviso_evento(&e));    // rajada: ainda dele
  SDL_Delay(300);
  e.type = SDL_KEYUP;
  conquistas_aviso_evento(&e);
  e.type = SDL_KEYDOWN;
  assert(!conquistas_aviso_evento(&e));   // passou o debounce: Voltar e do app
  conquistas_aviso_armar();               // ja mostrado: nao repete
  assert(!conquistas_aviso_aberto());
  // Varios de uma vez: um aviso so, agrupado.
  subir("dir:nolan", 12);
  subir("trilha:ghibli", 22);
  conquistas_aviso_armar();
  assert(conquistas_aviso_aberto());
  captura(prefixo, "aviso-varios", 30);
  conquistas_aviso_fechar();
  assert(!conquistas_aviso_aberto());
  // O aviso nao mexe no foco da tela aberta.
  conquistas_teste_esquecer();
  conquistas_abrir();
  tecla(SDLK_DOWN); tecla(SDLK_RIGHT);
  { int foco = conquistas_foco();
    subir("trilha:noir", 12);
    conquistas_aviso_armar();
    assert(conquistas_aviso_aberto() && conquistas_foco() == foco && conquistas_aberto());
    tecla(SDLK_DOWN);
    assert(conquistas_aviso_aberto()); }    // seta mexe a tela, o aviso fica
  // Aviso oculto pela tela nao captura o primeiro Voltar.
  memset(&e, 0, sizeof e); e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_ESCAPE;
  assert(!conquistas_aviso_evento(&e));
  conquistas_evento(&e);
  assert(!conquistas_aberto());
  conquistas_teste_esquecer();
  subir("trilha:cult-80s", 10);
  conquistas_aviso_armar();
  assert(conquistas_aviso_aberto());
  conquistas_abrir();
  assert(!conquistas_aviso_evento(&e));
  conquistas_evento(&e);
  assert(!conquistas_aberto());
  conquistas_aviso_fechar();
  conquistas_teste_esquecer();
  (void)ids; (void)rs;

  // ---- Perfil: o botao do cabecalho.
  regras_perfil(prefixo);

  tex_encerrar(); txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(w); SDL_Quit();
  puts("PASS: conquistas_shot");
  return 0;
}
