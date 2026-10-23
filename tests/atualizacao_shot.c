// CAPTURA DA ATUALIZACAO no Glass UI v2 (mockup ajustes-v2, "v2-upd-*",
// aprovado em 03/10): o aviso na ilha, a ilha crescendo ate o cartao, os
// estados do cartao (disponivel, sem instalador, baixando, conferindo,
// instalando, pronto, falhou), a barra que fica na ilha ao fechar no meio, e a
// consulta dos Ajustes falando pela ilha (procurando, em dia).
//
// Existe porque este cartao e o unico da interface que NAO DA PARA FOTOGRAFAR
// usando o app: ele so abre quando ha no GitHub uma versao mais nova que a
// instalada, e o ramo com botoes e barra so existe onde ha instalador.
//
// Inclui src/atualizacao.c: tagNova, notas, estado e a barra sao estaticos, e
// semear por dentro e o unico jeito de encenar os estados sem rede. A ilha e
// a de verdade (ilha.c), ancorada a 48,36 como no mockup.
//
//   bash tests/atualizacao_shot.sh /tmp/nuvio-upd        # vidro
//   NUVIO_SHOT_VIDRO=0 bash tests/atualizacao_shot.sh /tmp/nuvio-upd-solido
#include "../src/atualizacao.c"
#include "nvtmp.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "ajustes.h"
#include "anim.h"
#include <SDL2/SDL_image.h>
#include <assert.h>

static SDL_Window *win;
static const char *saida;
static GLuint arte;
static int capW = 1920, capH = 1080;
// RELOGIO DE 16 ms POR QUADRO, cravado (quadros espera o de verdade alcancar):
// a mola do cartao tem de estar no mesmo ponto em toda rodada.
static Uint32 falso;

static void gravar(const char *nome) {
  unsigned char *pix = malloc((size_t)capW * capH * 4);
  SDL_Surface *s;
  char cam[700];
  int y;
  assert(pix);
  glReadPixels(0, 0, capW, capH, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, capW, capH, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < capH; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (size_t)(capH - 1 - y) * capW * 4, (size_t)capW * 4);
  snprintf(cam, sizeof cam, "%s-%s.bmp", saida, nome);
  assert(SDL_SaveBMP(s, cam) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", cam);
}

// O que app.c faz por quadro com a ilha e o cartao (ordem e guardas iguais):
// o aviso com acao entrega a chave, e "av:update:" abre o cartao.
static void quadros(int n, const char *nome) {
  int i;
  for (i = 0; i < n; i++) {
    Uint32 agora = (falso += 16u);
    // NO PASSO DO RELOGIO DE VERDADE: a atividade da ilha vence pelo
    // SDL_GetTicks (400 ms sem renovar), entao os dois andam juntos.
    while ((Sint32)(SDL_GetTicks() - agora) < 0) SDL_Delay(1);
    char chave[96];
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    gfx_novo_quadro();
    if (ilha_aviso_pediu(chave, sizeof chave) == 1 && !strncmp(chave, "av:update:", 10))
      atualizacao_abrir();
    glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (arte) {
      gfx_tex_aspect_atual = 0.0f;
      gfx_rect((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, arte, GFX_SNAP, 0.0f, 0, 0, 0.0f, 1, 1, 1, 1);
    }
    atualizacao_atualizar(1.0f / 60.0f, agora);
    atualizacao_desenhar(agora);
    ilha_relogio_visivel(1);
    ilha_posicionar(1);
    ilha_coberta(atualizacao_cobre_ilha());
    ilha_desenhar(agora);
    if (i == n - 1 && nome) gravar(nome);
    SDL_GL_SwapWindow(win);
  }
}

static void tecla(SDL_Keycode k, SDL_Scancode sc) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  e.key.keysym.scancode = sc;
  if (!ilha_evento(&e) && atualizacao_aberta()) atualizacao_evento(&e);
}

// As notas da 1.7.3 do GitHub (as do mockup), em MARKDOWN CRU: a captura
// prova tambem o cortador de "## Notes" e o resumo no cabecalho.
static const char MD[] =
  "Where to watch in the source picker, cleaner Settings and reliability fixes.\n"
  "\n"
  "## Added\n"
  "- Where to watch in the source picker: see regional streaming services such as Netflix and open their installed apps. Store availability depends on the TV.\n"
  "- Optional app icons using the existing provisional supporter setting.\n"
  "\n"
  "## Fixed\n"
  "- Android queries installed streaming apps in the background, preserving the last complete list while refreshing.\n"
  "- Streaming-service cards keep readable text when selected.\n"
  "- Cleaner Settings explanations without placeholder pictures; glass outline is easier to find.\n"
  "- Collections snapshots now handle object responses and stay available after network failures.\n"
  "- Samsung playback errors retain their diagnostic code.\n"
  "\n"
  "## Notes\n"
  "Install with the Homebrew Channel or Developer Mode.\n";

static void semear(int comoEstado, float pct, const char *passo, int comInstalador) {
  snprintf(tagNova, sizeof tagNova, "%s", "1.7.3");
  limparNotas(MD, notas, sizeof notas);
  if (comInstalador)
    snprintf(ipkUrl, sizeof ipkUrl, "%s",
             "https://github.com/iqui27/nuvio-native-legacy/releases/download/"
             "v1.7.3/space.nuvio.native.legacy_1.7.3_arm.ipk");
  else ipkUrl[0] = 0;
  estado = comoEstado;
  instPct = pct;
  snprintf(instPasso, sizeof instPasso, "%s", passo ? passo : "");
}

// O aviso "update:" exatamente como avisos.c o anuncia (anunciarItem).
static void avisoUpdate(void) {
  IlhaAvisoEx e;
  char txt[160];
  memset(&e, 0, sizeof e);
  snprintf(txt, sizeof txt, i18n("Versão %s disponível. Você está na %s."), "1.7.3", NV_VERSAO);
  e.chave = "av:update:1.7.3"; e.grupo = 1; e.texto = txt;
  e.titulo = i18n("Atualização disponível");
  e.tipo = ILHA_ACENTO; e.icone = "aj_download"; e.ms = 60000u;
  e.acao = 1; e.dica = i18n("Ver o que mudou");
  ilha_avisar_ex(&e);
}

int main(int argc, char **argv) {
  SDL_GLContext gl;
  const char *vid = getenv("NUVIO_SHOT_VIDRO");
  saida = argc > 1 ? argv[1] : nvt("/nuvio-upd");

  // PORTUGUES e o acento Oceano (#42a5f5), os do mockup; o material vem de
  // NUVIO_SHOT_VIDRO (0 = solido). tests/atualizacao_shot.sh poe NUVIO_DADOS.
  { const char *dir = getenv("NUVIO_DADOS");
    char cam[600];
    FILE *f;
    assert(dir && *dir);
    snprintf(cam, sizeof cam, "%s/ajustes.txt", dir);
    f = fopen(cam, "w");
    assert(f);
    fprintf(f, "idioma 0\nselected_theme 2\nvidroLocal %d\n", vid && *vid == '0' ? 1 : 0);
    fclose(f);
    ajustes_dir(dir); }
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("Nuvio: atualizacao", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(win);
  gl = SDL_GL_CreateContext(win);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  ajustes_definir_vidro(!(vid && *vid == '0'));
  mtx = SDL_CreateMutex();

  falso = SDL_GetTicks();
  arte = tex_obter("deploy/app/art/00.jpg");
  { int i; for (i = 0; i < 40 && !arte; i++) { tex_bombear(6);
      arte = tex_obter("deploy/app/art/00.jpg"); SDL_Delay(16); } }

  // 1. O aviso na ilha; 2. a tecla dele faz a ilha crescer (meio do morph).
  semear(AT_PARADO, 0.0f, "", 1);
  quadros(30, NULL);
  avisoUpdate();
  quadros(70, "aviso");
  tecla(SDLK_s, SDL_SCANCODE_S);           // AZUL no Mac (ilha.c, teclaAzul)
  quadros(8, "crescendo");
  assert(aberto);
  quadros(90, "disponivel");
  assert(foco == 0);

  // Sem instalador: o rodape e o endereco com o QR.
  semear(AT_PARADO, 0.0f, "", 0);
  quadros(20, "sem-instalador");

  // As etapas.
  semear(AT_INSTALANDO, 42.0f, "Downloading", 1);
  quadros(20, "baixando");
  semear(AT_INSTALANDO, 0.0f, "Verificando", 1);
  quadros(20, "conferindo");
  semear(AT_INSTALANDO, 0.0f, "Install", 1);
  quadros(20, "instalando");
  semear(AT_PRONTO, 100.0f, "", 1);
  soEncenada = 1;
  quadros(20, "pronto");
  soEncenada = 0;
  semear(AT_FALHOU, 0.0f, "Verificando", 1);
  quadros(20, "falhou");

  // Rolar ate o fim nao mexe no foco e chega no fim.
  semear(AT_PARADO, 0.0f, "", 1);
  { int i; for (i = 0; i < 40; i++) tecla(SDLK_DOWN, SDL_SCANCODE_DOWN); }
  quadros(30, "rolado");
  assert(foco == 0);
  assert(rolar > rolarMax() - 1.0f);
  { int i; for (i = 0; i < 40; i++) tecla(SDLK_UP, SDL_SCANCODE_UP); }
  quadros(30, NULL);

  // Voltar no meio do download: o cartao recolhe e a ilha leva a barra.
  semear(AT_INSTALANDO, 61.0f, "Downloading", 1);
  quadros(2, NULL);
  tecla(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE);
  assert(!aberto);
  quadros(8, "recolhendo");
  quadros(90, "ilha");
  assert(!atualizacao_cobre_ilha());

  // A consulta dos Ajustes pela ilha: procurando e, no fim, em dia.
  semear(AT_PARADO, 0.0f, "", 1);
  tagNova[0] = 0;
  manualIlha = 1; busca = ATUALIZACAO_BUSCA_PROCURANDO;
  quadros(60, "procurando");
  busca = ATUALIZACAO_BUSCA_EM_DIA;
  quadros(70, "em-dia");

  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(win);
  SDL_Quit();
  puts("PASS: capturas da atualizacao gravadas.");
  return 0;
}
