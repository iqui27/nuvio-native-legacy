// CAPTURAS DA TELA DE PERFIL E STATS, sem rede e sem conta do Trakt.
//
// Existe pelo mesmo motivo de tests/perfilsel_visual.c: interface de TV nao se
// revisa lendo codigo. A tela e olhada a 3 m, e colisao de texto, cinza sobre
// cinza e foco que nao se acha sao invisiveis numa leitura do fonte — foi assim
// que o aviso do topo ficou desenhado DUAS vezes e que o numeral do streak
// passou por cima da legenda dele.
//
// O snapshot e PerfilDados de mentira, com a forma que o trakt.c produz de
// verdade: `parcial` ligado, `aviso` preenchido (os dois juntos eram o caso em
// que o aviso aparecia em duplicata), nome + usuario + periodo, um mes cheio de
// atividade irregular e quatro destaques com arte do PACOTE — nenhuma URL de
// rede, nenhum dado de pessoa.
//
// ONDE ESCREVE. Este teste NAO grava dado de app, mas chama dados_iniciar()
// como o app chama, e por isso se recusa a rodar se dados_dir() nao for a pasta
// descartavel que tests/perfil_shot.sh exporta. A regra do repo: nunca ~/.nuvio.
//
//   bash tests/perfil_shot.sh [prefixo]
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

static SDL_Window *win;

static void tecla(SDL_Keycode k) {
  SDL_Event e = { 0 };
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  perfil_evento(&e);
}

// 90 quadros: sobra para a mola de entrada assentar (NV_MOLA_TELA), para a do
// foco fechar os 120 ms medidos e para o decode das artes subir para a GPU.
static void captura(const char *nome) {
  int i;
  for (i = 0; i < 90; i++) {
    SDL_PumpEvents();
    txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6); gfx_novo_quadro();
    perfil_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(0.051f, 0.051f, 0.051f, 1.0f); glClear(GL_COLOR_BUFFER_BIT);
    perfil_desenhar(SDL_GetTicks());
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
      SDL_FreeSurface(s); free(pix);
    }
    SDL_GL_SwapWindow(win); SDL_Delay(2);
  }
  printf("  %s  (preenchimento %.2f telas, %d desenhos)\n",
         nome, gfx_fill, gfx_n_rect);
}

// A FORMA REAL do snapshot do trakt.c: mes de 30 dias comecando numa terca
// (primeiroDiaSemana = 2), atividade irregular com um pico, streak em curso e
// os avisos de recorte que o produtor sempre liga.
static void montar(PerfilDados *d) {
  static const unsigned short ATIV[30] = {
    0,2,0,1,0,0,3, 1,0,0,0,4,1,0, 0,1,1,0,7,2,0, 0,0,1,2,3,1, 2,1,1
  };
  static const char *TIT[4] = {
    "Pluribus", "A Casa do Dragão", "Duna: Parte Dois",
    "Uma Série de Eventos Muito Longos com Nome Enorme"
  };
  static const char *DET[4] = { "T1E6", "T2E4", "Filme", "T3E11" };
  static const char *GEN[4] = { "Drama", "Ficção científica", "Ação", "Comédia" };
  static const int GQ[4] = { 34, 21, 17, 9 };
  int i;
  memset(d, 0, sizeof *d);
  snprintf(d->nome, sizeof d->nome, "Henrique Rocha");
  snprintf(d->usuario, sizeof d->usuario, "iqui27");
  snprintf(d->periodo, sizeof d->periodo, "SETEMBRO 2026");
  d->minutos = 2384; d->plays = 41; d->filmes = 6; d->episodios = 35;
  d->nDias = 30; d->primeiroDiaSemana = 2;
  for (i = 0; i < 30; i++) {
    d->atividade[i] = ATIV[i];
    if (ATIV[i]) d->diasAtivosMes++;
  }
  d->streakAtual = 3;
  d->nGeneros = PERFIL_MAX_GENEROS;
  for (i = 0; i < d->nGeneros; i++) {
    snprintf(d->generos[i].nome, sizeof d->generos[i].nome, "%s", GEN[i]);
    d->generos[i].quantidade = GQ[i];
  }
  d->nDestaques = PERFIL_MAX_DESTAQUES;
  for (i = 0; i < d->nDestaques; i++) {
    PerfilDestaque *p = &d->destaques[i];
    snprintf(p->id, sizeof p->id, "tt000000%d", i);
    snprintf(p->titulo, sizeof p->titulo, "%s", TIT[i % 4]);
    snprintf(p->detalhe, sizeof p->detalhe, "%s", DET[i % 4]);
    snprintf(p->backdrop, sizeof p->backdrop, "deploy/app/art/0%d.jpg", i);
    p->plays = 9 - i * 2; p->minutos = 480 - i * 90;
  }
  // Os dois que o trakt.c sempre liga, e que faziam o aviso sair duplicado.
  d->parcial = 1;
  snprintf(d->aviso, sizeof d->aviso,
           "Recorte das 100 reproduções mais recentes do mês. Durações informadas pelo Trakt.");
}

int main(int argc, char **argv) {
  const char *prefixo = argc > 1 ? argv[1] : "/tmp/nuvio-perfil";
  char nome[600];
  PerfilDados d;
  SDL_GLContext gl;
  int i;

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("Nuvio: revisão do Perfil e Stats",
                         SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(win);
  gl = SDL_GL_CreateContext(win); assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");

  // GUARDA DE PASTA. O argumento de dados_iniciar e a pasta de ARTE, nao um
  // desvio dos dados: o desvio e a variavel NUVIO_DADOS. Sem esta conferencia
  // um teste grafico escreve em ~/.nuvio, os dados REAIS de quem roda.
  dados_iniciar(NULL);
  { const char *dir = dados_dir();
    const char *tmp = getenv("NUVIO_TESTE_DIR");
    if (!tmp || !*tmp || !dir || strcmp(dir, tmp)) {
      fprintf(stderr,
              "perfil_shot: recusando rodar fora de uma pasta descartável.\n"
              "  dados_dir()=%s   NUVIO_TESTE_DIR=%s\n"
              "  Rode por tests/perfil_shot.sh, que exporta NUVIO_DADOS.\n",
              dir && *dir ? dir : "(nenhuma)", tmp ? tmp : "(vazia)");
      return 1;
    } }

  // IDIOMA E TEMA VINDOS DO DISCO, pelo caminho real (ajustes_dir le
  // ajustes.txt da pasta de dados). Duas razoes:
  //   - "idioma 0" poe a interface em portugues; o padrao de fabrica e o INGLES
  //     (ajustes.c:620), e quem revisa esta tela le portugues;
  //   - "selected_theme 2" e o acento OCEANO. O anel de foco tem de sair AZUL:
  //     ele estava cravado em branco nesta tela (o antigo anel() usava
  //     0.96/0.96/0.98), entao o tema nao fazia nada aqui. Azul ao lado das
  //     celulas violeta tambem mostra que o acento do TEMA e o violeta do DADO
  //     sao dois papeis, e nao a mesma cor.
  { char caminho[700]; FILE *f;
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dados_dir());
    f = fopen(caminho, "w");
    assert(f);
    fprintf(f, "idioma 0\nselected_theme 2\n");
    fclose(f);
    ajustes_dir(dados_dir()); }

  montar(&d);

  // 1. CARREGANDO: o esqueleto tem de cair nas mesmas caixas do conteudo real.
  //    Compare esta captura com a seguinte — nada pode saltar de lugar.
  perfil_iniciar(); perfil_abrir(); perfil_definir_carregando(1);
  snprintf(nome, sizeof nome, "%s-carregando.bmp", prefixo);
  captura(nome);

  // 2. PRONTO, foco no calendario (a primeira parada).
  perfil_definir_dados(&d);
  snprintf(nome, sizeof nome, "%s-calendario.bmp", prefixo);
  captura(nome);

  // 3. Andando pelo calendario: uma semana para baixo e tres dias a direita,
  //    para o anel cair num dia aceso no meio da grade.
  tecla(SDLK_DOWN); tecla(SDLK_DOWN);
  for (i = 0; i < 4; i++) tecla(SDLK_RIGHT);
  snprintf(nome, sizeof nome, "%s-dia.bmp", prefixo);
  captura(nome);

  // 4. A segunda parada: direita ate o fim do mes passa para os destaques.
  for (i = 0; i < 40; i++) tecla(SDLK_RIGHT);
  tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-destaques.bmp", prefixo);
  captura(nome);

  // 5. SEM IDENTIDADE E SEM GENERO: o caso do Trakt que so devolve historico.
  //    A banda de numeros nao pode desabar nem deslocar as secoes de baixo.
  { PerfilDados m = d;
    m.nome[0] = m.usuario[0] = m.avatar[0] = 0;
    m.nGeneros = 0; m.minutos = 0; m.streakCompleto = 1;
    perfil_definir_dados(&m); }
  snprintf(nome, sizeof nome, "%s-sem-identidade.bmp", prefixo);
  captura(nome);

  // 6. ESTADO VAZIO: sem snapshot, OK pede nova tentativa.
  perfil_definir_dados(NULL);
  perfil_definir_estado(PERFIL_ESTADO_DESCONECTADO, NULL);
  snprintf(nome, sizeof nome, "%s-vazio.bmp", prefixo);
  captura(nome);

  tex_encerrar(); txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(win); SDL_Quit();
  puts("PASS: capturas do Perfil e Stats gravadas.");
  return 0;
}
