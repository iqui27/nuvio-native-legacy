// CAPTURAS DA TELA DE ESCOLHA DE PERFIL, sem rede e sem conta.
//
// Existe pelo mesmo motivo de tests/player_regression.c --profile: interface de
// TV nao se revisa lendo codigo. A tela e olhada a 3 m, e coisas que so
// aparecem no pixel (nome que estoura a coluna, cinza sobre cinza, foco que nao
// se acha) sao invisiveis numa leitura.
//
// A LISTA VEM DO CACHE EM DISCO, pela API publica: o teste escreve perfis.txt
// numa pasta temporaria e chama perfis_carregar_ativo(). Ou seja, alem de
// desenhar a tela, ele exercita o mesmo caminho de arranque que a TV usa.
//
// As URLs de avatar e de fundo apontam para arte do PACOTE, nao para o Storage
// da conta: nenhum dado de pessoa entra aqui, e as capturas rodam sem rede.
//
//   bash tests/perfilsel.sh --capturas
#include "perfilsel.h"
#include "nvtmp.h"
#include "perfis.h"
#include "dados.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "catalogo.h"
#include "ajustes.h"
#include "gpunivel.h"
#include "progresso.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Window *win;
static int capturaQuadros = 90;

static const char *shotPath(const char *nome) {
  static char caminho[768];
  const char *prefixo = getenv("NUVIO_PERFILSEL_SHOT_PREFIX");
  const char *base;
  if (!prefixo || !*prefixo) return nome;
  base = strrchr(nome, '/');
  base = base ? base + 1 : nome;
  snprintf(caminho, sizeof caminho, "%s-%s", prefixo, base);
  return caminho;
}

static void salvarTela(const char *nome) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  int y;
  assert(pix);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch,
           pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(SDL_SaveBMP(s, nome) == 0);
  SDL_FreeSurface(s); free(pix);
}

static void escreverCache(const char *conteudo, int ativo) {
  char linha[16];
  assert(dados_gravar("perfis.txt", conteudo));
  snprintf(linha, sizeof linha, "%d\n", ativo);
  assert(dados_gravar("perfil.txt", linha));
}

static void semearMuralCatalogo(void) {
  static CatItem itens[9];
  int i;
  memset(itens, 0, sizeof itens);
  for (i = 0; i < 9; i++) {
    snprintf(itens[i].imdb, sizeof itens[i].imdb, "tt-mural-%02d", i);
    snprintf(itens[i].tipo, sizeof itens[i].tipo, "%s", "movie");
    snprintf(itens[i].titulo, sizeof itens[i].titulo, "Capa de teste %d", i + 1);
    snprintf(itens[i].poster, sizeof itens[i].poster,
             "deploy/app/art/poster/%02d.jpg", i);
  }
  // O mural precisa receber exatamente os poster URLs do catalogo, como a
  // Home. Nao ha lista paralela nem download especial neste shot.
  cat_definir_tudo(itens, 9, NULL, 0);
}

static void ajustesDeTeste(int reduzidas) {
  FILE *f;
  char caminho[700];
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dados_dir());
  f = fopen(caminho, "w");
  assert(f);
  fprintf(f, "animacoes %d\n", reduzidas);
  // Convencoes das capturas: NUVIO_SHOT_FONTE=3 (Montserrat, a da TV),
  // NUVIO_PERFILSEL_FUNDO (0 mural, 1 listras, 2 arte do perfil) e
  // NUVIO_PERFILSEL_VIDRO (0 vidro, 1 solido).
  if (getenv("NUVIO_SHOT_FONTE") && *getenv("NUVIO_SHOT_FONTE"))
    fprintf(f, "fonteInterface %d\n", atoi(getenv("NUVIO_SHOT_FONTE")));
  if (getenv("NUVIO_PERFILSEL_FUNDO"))
    fprintf(f, "perfilFundoLocal %d\n", atoi(getenv("NUVIO_PERFILSEL_FUNDO")));
  if (getenv("NUVIO_PERFILSEL_VIDRO"))
    fprintf(f, "vidroLocal %d\n", atoi(getenv("NUVIO_PERFILSEL_VIDRO")));
  fclose(f);
  ajustes_dir(dados_dir());
}

// 90 quadros: tempo de sobra para a mola do foco assentar (120 ms medidos) e
// para o decode das texturas subir para a GPU.
static void captura(const char *nome) {
  int i;
  for (i = 0; i < capturaQuadros; i++) {
    SDL_PumpEvents();
    txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6); gfx_novo_quadro();
    perfilsel_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(0.051f, 0.051f, 0.051f, 1); glClear(GL_COLOR_BUFFER_BIT);
    perfilsel_desenhar(SDL_GetTicks());
    // GL_BACK ainda e valido neste ponto. Depois do SwapWindow o back buffer
    // pode ser descartado, portanto a captura do frame final fica aqui.
    if (i == capturaQuadros - 1) salvarTela(shotPath(nome));
    SDL_GL_SwapWindow(win); SDL_Delay(4);
  }
  // Captura o quadro realmente final da simulação. O antigo `if (i == 89)`
  // fazia a captura cair no quadro 89 mesmo quando o settled tinha 600.
  printf("  %s  (preenchimento %.2f telas, %d desenhos)\n",
         shotPath(nome), gfx_fill, gfx_n_rect);
}

static void tecla(SDL_Keycode k) {
  SDL_Event e = {0};
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  perfilsel_evento(&e);
}

static void verificaAnimacoes(void) {
  PerfilSelTesteEstado antes, depois;
  int i;
  perfilsel_teste_estado(&antes);
  tecla(SDLK_RIGHT);
  perfilsel_atualizar(10.0f, SDL_GetTicks()); /* resume longo precisa ser limitado */
  perfilsel_teste_estado(&depois);
  assert(depois.foco[0] < antes.foco[0]);
  assert(depois.foco[1] > antes.foco[1] && depois.foco[1] < 1.0f);
  assert(depois.mural_tempo - antes.mural_tempo <= 0.0501f);
  /* #1E88E5 -> #E53935: a luz tem de passar pelo meio, nao saltar no foco. */
  assert(depois.luz[0] > antes.luz[0] && depois.luz[0] < depois.luz_alvo[0]);
  for (i = 0; i < 6; i++) perfilsel_atualizar(0.05f, SDL_GetTicks());
  perfilsel_teste_estado(&depois);
  assert(fabsf(depois.luz[0] - depois.luz_alvo[0]) < 0.002f);
  tecla(SDLK_LEFT);
  for (i = 0; i < 6; i++) perfilsel_atualizar(0.05f, SDL_GetTicks());
}

static void verificaRevisaoSemTrocaDeUrl(void) {
  CatItem itens[9];
  PerfilSelTesteEstado antes, depois;
  int i;
  for (i = 0; i < 9; i++) {
    const CatItem *item = cat_item(i);
    assert(item);
    itens[i] = *item;
    snprintf(itens[i].titulo, sizeof itens[i].titulo, "Revisao %d", i);
  }
  perfilsel_teste_estado(&antes);
  assert(antes.mural_n == 9 && antes.fade[0] > 0.95f);
  cat_definir_tudo(itens, 9, NULL, 0);
  perfilsel_atualizar(1.0f / 60.0f, SDL_GetTicks());
  perfilsel_teste_estado(&depois);
  assert(fabsf(depois.fade[0] - antes.fade[0]) < 0.0001f);
}

static void verificaPressaoSustentada(void) {
  PerfilSelTesteEstado e;
  int i;
  perfilsel_teste_estado(&e);
  assert(e.particulas == 32 || e.particulas == 20);
  if (e.particulas == 32) {
    for (i = 0; i < 30; i++) perfilsel_atualizar(0.04f, SDL_GetTicks());
    perfilsel_teste_estado(&e);
    assert(e.particulas == 20 && e.burst == 18);
    /* The downgrade is latched for this screen session. */
    for (i = 0; i < 30; i++) perfilsel_atualizar(1.0f / 60.0f, SDL_GetTicks());
    perfilsel_teste_estado(&e);
    assert(e.particulas == 20 && e.burst == 18);
  }
}

static void verificaReducedMotion(void) {
  PerfilSelTesteEstado e;
  perfilsel_teste_estado(&e);
  tecla(SDLK_RIGHT);
  perfilsel_atualizar(0.04f, SDL_GetTicks());
  perfilsel_teste_estado(&e);
  assert(e.foco[5] == 1.0f);
  for (int i = 0; i < 6; i++) assert(fabsf(e.luz[i] - e.luz_alvo[i]) < 0.0001f);
  assert(e.mural_tempo == 0.0f && e.burst_tempo == 0.0f);
  tecla(SDLK_LEFT);
  perfilsel_atualizar(0.04f, SDL_GetTicks());
}

int main(void) {
  const char *arte = "deploy/app/art";
  SDL_GLContext gl;

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("Nuvio: revisao da escolha de perfil",
                         SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(win);
  gl = SDL_GL_CreateContext(win); assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir(arte);
  // O ARGUMENTO DE dados_iniciar E A PASTA DE ARTE, nao um desvio dos dados: o
  // desvio e a variavel NUVIO_DADOS. Passando a pasta temporaria aqui o teste
  // escrevia em ~/.nuvio — os perfis REAIS de quem roda — e o perfis_esquecer()
  // logo abaixo apagava os arquivos de la. Aconteceu uma vez; esta guarda
  // existe para que nao aconteca duas.
  dados_iniciar(NULL);
  { const char *d = dados_dir();
    const char *tmp = getenv("NUVIO_TESTE_DIR");
    if (!tmp || !*tmp || !d || strcmp(d, tmp)) {
      fprintf(stderr,
              "perfilsel_visual: recusando rodar fora de uma pasta descartavel.\n"
              "  dados_dir()=%s   NUVIO_TESTE_DIR=%s\n"
              "  Rode por tests/perfilsel.sh --capturas, que exporta NUVIO_DADOS.\n",
              d && *d ? d : "(nenhuma)", tmp ? tmp : "(vazia)");
      return 1;
    } }

  // indice \t temPin \t primario \t usaAddons \t cor \t nome \t avatar \t fundo
  semearMuralCatalogo();
  escreverCache("1\t0\t1\t0\t#1E88E5\tHenrique\t\tdeploy/app/art/03.jpg\n", 1);
  perfis_carregar_ativo();
  ajustesDeTeste(0);
  if (getenv("NUVIO_PERFILFUNDO_TEST")) {
    const char *level = getenv("NUVIO_PERFILFUNDO_TEST");
    if (!strcmp(level, "auto")) {
      gpun_preferencia(0);
      // Healthy 50 FPS on a 60 Hz surface: live reports now reach Minimal.
      for (int i = 0; i < 750; i++) gpun_medir(20, 15, 4, 1, 1);
#ifndef NV_BEFORE_GPUAUTO
      for (int i = 0; i < 6; i++) gpun_relatar(50, 20, 1000.0 / 60, 1, 1);
#endif
      printf("automatic effects level: %d\n", gpun_nivel());
    } else gpun_definir_nivel(atoi(level));
    perfilsel_iniciar();
    captura("/picker.bmp");
    unsigned char pixels[400 * 300 * 4];
    int colorful = 0;
    glReadPixels(0, 380, 400, 300, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    for (int i = 0; i < 400 * 300; i++) {
      int r = pixels[i * 4], g = pixels[i * 4 + 1], b = pixels[i * 4 + 2];
      int hi = r > g ? r : g, lo = r < g ? r : g;
      if (b > hi) hi = b;
      if (b < lo) lo = b;
      if (hi - lo > 15) colorful++;
    }
    printf("picker background: %d art pixels\n", colorful);
    return colorful > 3000 ? 0 : 1;
  }
  perfilsel_iniciar();
  assert(!perfilsel_concluido());
  perfilsel_continuar_ativo();
  assert(perfilsel_concluido());
  captura(nvt("/nuvio-perfilsel-1.bmp"));

  escreverCache("1\t0\t1\t0\t#1E88E5\tHenrique\t\tdeploy/app/art/03.jpg\n"
                "2\t0\t0\t1\t#E53935\tÁlvaro\t\t\n", 2);
  perfis_esquecer(); dados_iniciar(NULL);
  escreverCache("1\t0\t1\t0\t#1E88E5\tHenrique\t\tdeploy/app/art/03.jpg\n"
                "2\t0\t0\t1\t#E53935\tÁlvaro\t\t\n", 2);
  perfis_carregar_ativo();
  ajustesDeTeste(0);
  perfilsel_iniciar();
  captura(nvt("/nuvio-perfilsel-2.bmp"));

  perfis_esquecer(); dados_iniciar(NULL);
  escreverCache("1\t0\t1\t0\t#1E88E5\tHenrique\t"
                "deploy/app/art/poster/00.jpg\tdeploy/app/art/03.jpg\n"
                "2\t0\t0\t1\t#E53935\tÁlvaro Nascimento da Silva\t\t\n"
                "3\t1\t0\t0\t#43A047\tInfantil\t\tdeploy/app/art/07.jpg\n"
                "4\t0\t0\t0\t#8E24AA\tVisitas\t\t\n", 1);
  perfis_carregar_ativo();
  ajustesDeTeste(0);
  perfilsel_iniciar();
  captura(nvt("/nuvio-perfilsel-4.bmp"));
  verificaAnimacoes();
  verificaRevisaoSemTrocaDeUrl();

  // O terceiro perfil e o travado: tres DIREITA e a tela do selo de PIN em foco.
  tecla(SDLK_RIGHT); tecla(SDLK_RIGHT);
  captura(nvt("/nuvio-perfilsel-4-travado.bmp"));

  // OK sobre ele abre o teclado. Depois, quatro digitos e um erro de rede
  // fabricado nao — este e o estado normal de digitacao.
  tecla(SDLK_RETURN);
  { PerfilSelTesteEstado antes, depois;
    perfilsel_teste_estado(&antes);
    perfilsel_atualizar(10.0f, SDL_GetTicks());
    perfilsel_teste_estado(&depois);
    assert(depois.pin > antes.pin && depois.pin < 1.0f);
    assert(depois.mural_tempo == antes.mural_tempo); }
  // #289: three digits only — the 4th would verify against the server. The
  // pad opens on "5"; the bottom row is blank, 0, delete (no OK key).
  tecla(SDLK_UP); tecla(SDLK_LEFT); tecla(SDLK_RETURN);  // 1
  tecla(SDLK_RIGHT); tecla(SDLK_RETURN);                 // 2
  tecla(SDLK_8);                                         // remote number key
  tecla(SDLK_DOWN); tecla(SDLK_DOWN); tecla(SDLK_DOWN);  // focus on "0"
  captura(nvt("/nuvio-perfilsel-pin.bmp"));

  // Oito perfis: o pior caso do layout (CONTA_PERFIL_MAX).
  perfis_esquecer(); dados_iniciar(NULL);
  escreverCache("1\t0\t1\t0\t#1E88E5\tHenrique\t\tdeploy/app/art/03.jpg\n"
                "2\t0\t0\t0\t#E53935\tÁlvaro\t\t\n"
                "3\t1\t0\t0\t#43A047\tInfantil\t\t\n"
                "4\t0\t0\t0\t#8E24AA\tVisitas\t\t\n"
                "5\t0\t0\t0\t#FB8C00\tMariana\t\t\n"
                "6\t0\t0\t0\t#00ACC1\tRoberto\t\t\n"
                "7\t1\t0\t0\t#C0CA33\tCarla\t\t\n"
                "8\t0\t0\t0\t#5E35B1\tPedro\t\t\n", 5);
  perfis_carregar_ativo();
  ajustesDeTeste(0);
  perfilsel_iniciar();
  captura(nvt("/nuvio-perfilsel-8.bmp"));

  // A primeira captura pega o boom de abertura; esta espera longa prova que
  // ele se dissipa e deixa somente o campo orbital, sem textura adicional.
  capturaQuadros = 600;
  captura(nvt("/nuvio-perfilsel-8-settled.bmp"));
  capturaQuadros = 90;
  verificaPressaoSustentada();

  // Acessibilidade: a mesma composição com movimento congelado. As capas,
  // rastros e foco permanecem legíveis, mas nenhuma posição usa delta-time.
  ajustesDeTeste(1);
  perfilsel_iniciar();
  verificaReducedMotion();
  captura(nvt("/nuvio-perfilsel-8-reduzido.bmp"));
  { PerfilSelTesteEstado e;
    perfilsel_teste_estado(&e);
    assert(e.burst_desenhado == 0); }
  ajustesDeTeste(0);

  // TROCA DE PERFIL: escolhido o Alvaro, a tela fica com o indicador no cartao
  // dele enquanto a home e preparada, e as setas nao mexem mais no foco.
  perfis_esquecer(); dados_iniciar(NULL);
  escreverCache("1\t0\t1\t0\t#1E88E5\tHenrique\t"
                "deploy/app/art/poster/00.jpg\tdeploy/app/art/03.jpg\n"
                "2\t0\t0\t1\t#E53935\tÁlvaro\t\t\n"
                "3\t1\t0\t0\t#43A047\tInfantil\t\tdeploy/app/art/07.jpg\n", 1);
  perfis_carregar_ativo();
  perfilsel_iniciar();
  tecla(SDLK_RIGHT);
  perfilsel_preparar(1, SDL_GetTicks());
  assert(perfilsel_preparando());
  tecla(SDLK_RIGHT);
  captura(nvt("/nuvio-perfilsel-preparando.bmp"));
  perfilsel_iniciar();
  assert(!perfilsel_preparando());

  // CARTAO "CONTINUAR" (2.0, variante B): o que cada pessoa estava vendo, sob o
  // perfil em foco. Henrique: serie; Alvaro: filme; Infantil tem PIN (sem
  // cartao); Visitas nao viu nada. Funciona com qualquer fundo.
  { static CatItem it[11];
    PerfilSelTesteEstado e;
    int i;
    for (i = 0; i < 9; i++) it[i] = *cat_item(i);
    snprintf(it[9].imdb, sizeof it[9].imdb, "tt-cont-a");
    snprintf(it[9].tipo, sizeof it[9].tipo, "series");
    snprintf(it[9].titulo, sizeof it[9].titulo, "Cidade Submersa");
    snprintf(it[9].poster, sizeof it[9].poster, "deploy/app/art/poster/03.jpg");
    it[9].temporada = 2; it[9].episodio = 4;
    snprintf(it[9].nomeEpisodio, sizeof it[9].nomeEpisodio, "A maré");
    snprintf(it[10].imdb, sizeof it[10].imdb, "tt-cont-b");
    snprintf(it[10].tipo, sizeof it[10].tipo, "movie");
    snprintf(it[10].titulo, sizeof it[10].titulo, "Verão em Lisboa");
    snprintf(it[10].poster, sizeof it[10].poster, "deploy/app/art/poster/05.jpg");
    cat_definir_tudo(it, 11, NULL, 0);
    perfis_esquecer(); dados_iniciar(NULL); prog_invalidar();
    escreverCache("1\t0\t1\t0\t#1E88E5\tHenrique\t\tdeploy/app/art/03.jpg\n"
                  "2\t0\t0\t1\t#E53935\tÁlvaro\t\tdeploy/app/art/05.jpg\n"
                  "3\t1\t0\t0\t#43A047\tInfantil\t\tdeploy/app/art/07.jpg\n"
                  "4\t0\t0\t0\t#8E24AA\tVisitas\t\t\n", 1);
    perfis_carregar_ativo();
    perfis_definir_ativo(3); assert(prog_gravar_local("tt-cont-b", 0, 0, 1000, 6000, -1));
    perfis_definir_ativo(2); assert(prog_gravar_local("tt-cont-b", 0, 0, 2300, 6000, -1));
    perfis_definir_ativo(1); assert(prog_gravar_local("tt-cont-a", 2, 4, 1560, 2640, -1));
    ajustesDeTeste(0);
    perfilsel_iniciar();
    captura(nvt("/nuvio-perfilsel-continuar-1.bmp"));
    perfilsel_teste_estado(&e);
    assert(e.cont_tem[0] && e.cont_tem[1] && !e.cont_tem[2] && !e.cont_tem[3]);
    tecla(SDLK_RIGHT);
    captura(nvt("/nuvio-perfilsel-continuar-2.bmp"));
    tecla(SDLK_RIGHT);   // Infantil: PIN, sem cartao
    captura(nvt("/nuvio-perfilsel-continuar-pin.bmp"));
    tecla(SDLK_LEFT); tecla(SDLK_LEFT); tecla(SDLK_LEFT);
    /* meio da entrada: Henrique volta ao foco, o cartao esta subindo */
    tecla(SDLK_RIGHT);
    for (i = 0; i < 4; i++) {
      txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6); gfx_novo_quadro();
      perfilsel_atualizar(1.0f / 60.0f, SDL_GetTicks());
    }
    glClearColor(0.051f, 0.051f, 0.051f, 1); glClear(GL_COLOR_BUFFER_BIT);
    txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro();
    perfilsel_desenhar(SDL_GetTicks());
    salvarTela(shotPath(nvt("/nuvio-perfilsel-continuar-meio.bmp")));
    tecla(SDLK_LEFT);
    /* animacoes reduzidas: o cartao ja nasce no lugar */
    ajustesDeTeste(1);
    perfilsel_iniciar();
    perfilsel_atualizar(0.016f, SDL_GetTicks());
    captura(nvt("/nuvio-perfilsel-continuar-reduzido.bmp"));
    ajustesDeTeste(0);
  }

  // AMBIENTE DO PERFIL (2.0, variante A): Henrique e Infantil tem arte, Alvaro
  // nao. So roda com NUVIO_PERFILSEL_FUNDO=2.
  if (ajustes_ps_fundo() == 2) {
    PerfilSelTesteEstado e;
    int i;
    perfis_esquecer(); dados_iniciar(NULL);
    escreverCache("1\t0\t1\t0\t#1E88E5\tHenrique\t\tdeploy/app/art/03.jpg\n"
                  "2\t0\t0\t1\t#E53935\tÁlvaro\t\t\n"
                  "3\t1\t0\t0\t#43A047\tInfantil\t\tdeploy/app/art/07.jpg\n"
                  // #295: avatar but no background art.
                  "4\t0\t0\t0\t#8E24AA\tLia\tdeploy/app/art/poster/05.jpg\t\n", 1);
    perfis_carregar_ativo();
    ajustesDeTeste(0);
    perfilsel_iniciar();
    captura(nvt("/nuvio-perfilsel-amb-arte.bmp"));
    perfilsel_teste_estado(&e);
    assert(e.amb_t == 1.0f && e.amb_atual == 0);
    // Meio da troca: Henrique -> Infantil, ~0,2 s de 0,45 s.
    tecla(SDLK_RIGHT); tecla(SDLK_RIGHT);
    for (i = 0; i < 12; i++) {
      txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6); gfx_novo_quadro();
      perfilsel_atualizar(1.0f / 60.0f, SDL_GetTicks());
    }
    perfilsel_teste_estado(&e);
    assert(e.amb_ant == 0 && e.amb_atual == 2 && e.amb_t > 0.2f && e.amb_t < 0.6f);
    glClearColor(0.051f, 0.051f, 0.051f, 1); glClear(GL_COLOR_BUFFER_BIT);
    txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro();
    perfilsel_desenhar(SDL_GetTicks());
    salvarTela(shotPath(nvt("/nuvio-perfilsel-amb-meio.bmp")));
    for (i = 0; i < 60; i++) {
      txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6); gfx_novo_quadro();
      perfilsel_atualizar(1.0f / 60.0f, SDL_GetTicks());
    }
    perfilsel_teste_estado(&e);
    assert(e.amb_t == 1.0f);
    perfilsel_teste_estado(&e);
    assert(e.amb_fonte == 1);
    // #295: no art and no avatar -> the profile's colour light, not the wall.
    tecla(SDLK_LEFT);
    captura(nvt("/nuvio-perfilsel-amb-sem-arte.bmp"));
    perfilsel_teste_estado(&e);
    assert(e.amb_atual == 1 && e.amb_fonte == 0);
    // #295: avatar only -> the blurred avatar with the profile colour.
    tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); tecla(SDLK_RIGHT);
    captura(nvt("/nuvio-perfilsel-amb-avatar.bmp"));
    perfilsel_teste_estado(&e);
    assert(e.amb_atual == 3 && e.amb_fonte == 2 && e.amb_t == 1.0f);
  }

  tex_encerrar(); txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(win); SDL_Quit();
  printf("PASS: capturas em %s/nuvio-perfilsel-*.bmp (dados de teste, sem conta real).\n", nvt_dir());
  return 0;
}
