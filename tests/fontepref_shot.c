// CAPTURA DA FOLHA DE FONTES com a marca "Sua escolha anterior".
//
// Existe porque duas regressoes deste repositorio so foram vistas OLHANDO: o
// raio de gfx_cor e uma FRACAO DA ALTURA (nao pixels), e um anel de foco branco
// sobre pilula clara e invisivel. Uma marca de texto nova a 3 m de distancia
// entra na mesma categoria — cabe na linha? some sob o realce? colide com o
// provedor?
//
// NAO ENTRA NA SUITE (tools/testa-tudo.sh pula *_shot.sh): precisa de janela GL
// e de olho humano. Nao chama dados_iniciar: sem pasta de dados, fontepref nao
// le nem escreve arquivo nenhum e a captura nao toca no ~/.nuvio de quem roda.
#include "catalogo.h"
#include "nvtmp.h"
#include "streams.h"
#include "fontepref.h"
#include "badges.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "ajustes.h"
#include "selospacote.h"
#include "vidro_fundo.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fonte(Stream *s, const char *provedor, const char *rotulo,
                  const char *descricao, int altura, int mp4, int dv) {
  memset(s, 0, sizeof *s);
  snprintf(s->provedor, sizeof s->provedor, "%s", provedor);
  snprintf(s->rotulo, sizeof s->rotulo, "%s", rotulo);
  snprintf(s->descricao, sizeof s->descricao, "%s", descricao);
  snprintf(s->url, sizeof s->url, "https://exemplo.invalido/%s.mkv", provedor);
  s->altura = altura; s->mp4 = mp4; s->dolbyVision = dv;
  s->tamanhoMB = altura >= 2160 ? 11264 : 2048;
  // A FILEIRA DE BADGES E O SEGUNDO ITEM QUE A CAPTURA PRECISA MOSTRAR: no app
  // ela e preenchida por stream_parse, que esta captura nao usa. Sem esta
  // linha a base da linha sai vazia e a regressao de "badge branca sobre linha
  // clara" fica invisivel justamente na ferramenta que existe para ve-la.
  s->badges = badges_detectar(descricao);
}

static void captura(const char *nome, SDL_Window *win) {
  int i;
  for (i = 0; i < 60; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    stream_folha_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(0.025f, 0.025f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    gfx_novo_quadro();
    // UMA ARTE DE MENTIRA atras da folha: sem ela o vidro (corpo translucido)
    // e o degrade da borda nao tem o que deixar passar, e a captura nao
    // prova se o texto fica sobre fundo. Faixas quentes e um bloco claro que
    // atravessa a borda da folha, o pior caso de contraste.
    if (vidroFundoAtivo()) vidroFundoDesenhar();
    else {
    gfx_cor((GfxRect){ 0, 0, 1920, 1080 }, 0, .26f, .17f, .12f, 1);
    gfx_cor((GfxRect){ 0, 0, 1920, 360 }, 0, .55f, .36f, .22f, 1);
    gfx_cor((GfxRect){ 900, 420, 900, 260 }, 0, .82f, .78f, .70f, 1);
    }
    stream_folha_desenhar(SDL_GetTicks());
    if (i == 59) {
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

static void tecla(SDL_Keycode k) {
  SDL_Event e = { 0 };
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  stream_folha_evento(&e);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-fontepref");
  char nome[600];
  SDL_Window *w;
  SDL_GLContext gl;
  Stream v[5];
  int i;

  // A captura de foco usa o mesmo accent Ocean do album de sidebar quando
  // NUVIO_DADOS vem do wrapper; o tema continua selecionavel no ambiente.
  { const char *dir = getenv("NUVIO_DADOS");
    if (dir && *dir) {
      const char *temaEnv = getenv("NUVIO_SHOT_THEME");
      char caminho[700];
      FILE *f;
      int tema = temaEnv && *temaEnv ? atoi(temaEnv) : 2;
      if (tema < 0 || tema >= 12) tema = 2;
      snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
      f = fopen(caminho, "w"); assert(f);
      { const char *li = getenv("NUVIO_SHOT_IDIOMA");   // 2 = English
        fprintf(f, "idioma %d\nselected_theme %d\n", li && *li ? atoi(li) : 0, tema); }
      { const char *fu = getenv("NUVIO_SHOT_FONTE_UI");   // 3 = Montserrat
        if (fu && *fu) fprintf(f, "fonteInterface %d\n", atoi(fu)); }
      { const char *t = getenv("NUVIO_SHOT_TEXTO");
        if (t && *t == '1') fprintf(f, "fonteTextoLocal 1\n");
        // NUVIO_SHOT_LOGO=1: Texto das fontes > Logo do titulo.
        if (getenv("NUVIO_SHOT_LOGO")) fprintf(f, "fonteTextoLocal 2\n");
        // NUVIO_SHOT_SELOS=1: Selos coloridos ligado.
        if (getenv("NUVIO_SHOT_SELOS")) fprintf(f, "selosColoridosLocal 0\n"); }
      // Material: NUVIO_SHOT_VIDRO=0 desliga a Interface de vidro (folha solida).
      { const char *v = getenv("NUVIO_SHOT_VIDRO");
        fprintf(f, "vidroLocal %d\n", v && *v == '0' ? 1 : 0); }
      fclose(f);
      ajustes_dir(dir);
      ajustes_teste_vidro_env();   // NUVIO_SHOT_VIDRO_OPAC / _FOSCO
    } }

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: folha de fontes", SDL_WINDOWPOS_CENTERED,
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
  vidroFundoPreparar();
  gfx_icones_dir("deploy/app/art");
  badges_carregar("deploy/app/art");   // quem faz isto no app e home.c
  // NUVIO_SHOT_PACOTE=<json>: um pacote de selos do Nuvio (3 formas aceitas)
  // vira o pacote ativo, como depois de "Adicionar pacote de selos". Amostra:
  // tests/selospacote_amostra.json (imagens locais + filtros so de texto).
  { const char *pc = getenv("NUVIO_SHOT_PACOTE");
    if (pc && *pc) {
      FILE *f = fopen(pc, "rb");
      char *buf; long n;
      assert(f);
      fseek(f, 0, SEEK_END); n = ftell(f); rewind(f);
      buf = malloc((size_t)n + 1); assert(buf);
      assert(fread(buf, 1, (size_t)n, f) == (size_t)n);
      buf[n] = 0; fclose(f);
      assert(selospacote_adicionar(buf, "https://pacote.exemplo.invalido/amostra.json") == SELOS_OK);
      free(buf);
    } }

  // O CONJUNTO DE UMA FOLHA REAL: cinco fontes como o AIOStreams do dono as
  // mandou para Silo S02E05 em 02/10 — `name` igual em todas, `description`
  // em linhas (tamanho | taxa, grupo, idiomas, arquivo). O mesmo conjunto
  // serve aos dois modos de Ajustes > Texto das fontes: no "Do Nuvio" ele
  // exercita tituloDa (origem · destaque) sobre texto real; no "Do addon"
  // (NUVIO_SHOT_TEXTO=1) mostra o texto como chegou, ⚡ e ⚑ inclusive.
#define AIO "AIOStreams | ElfHosted"
#define NOME "\xe2\x9a\xa1\xef\xb8\x8e  Silo S02 E05 "
  fonte(&v[0], AIO, NOME,
        "11.1 GB  |   30.1 Mbps  |\nSGF   \n\xe2\x9a\x91 English | Spanish | German | Italian | French | Portuguese\n"
        "Silo.S02E05.Eng.Fre.Ger.Ita.Por.Spa.2160p.WEBMux.DV.HDR.HEVC.Atmos-SGF.mkv", 2160, 0, 1);
  fonte(&v[1], AIO, NOME, "10.2 GB  |   27.8 Mbps  |\nSilo.S02E05.2160p.DV.HDR.mkv", 2160, 0, 1);
  fonte(&v[2], AIO, NOME, "4.1 GB  |   11.2 Mbps  |\nSilo.2024.S02E05.WEB-DL.1080p.HDREZKA.STUDIO.mkv", 1080, 0, 0);
  fonte(&v[3], AIO, NOME, "413 MB  |   1.12 Mbps  |\nELiTE\nSilo.S02E05.1080p.x265-ELiTE.mkv", 1080, 0, 0);
  fonte(&v[4], AIO, NOME, "1.2 GB  |   3.27 Mbps  |\nSilo S02E05.mp4", 720, 1, 0);
  // O alvo e o nome fazem o titulo de cada linha: "Silo Temporada 2 Episodio 5".
  stream_definir_alvo("tt14688458:2:5");
  stream_definir_lista(v, 5);
  stream_folha_nome("Silo");
  // A logo do primeiro titulo do catalogo de exemplo (deploy/app/art/logo).
  if (getenv("NUVIO_SHOT_LOGO") && cat_carregar("deploy/app/art")) stream_folha_item(0);
  stream_folha_contexto("T2:E5 · Silo");

  // A DUBLADA E A LEMBRADA (indice 3), e a que esta TOCANDO e a 4K (indice 0):
  // e o caso que interessa olhar, porque as duas marcas aparecem na mesma
  // coluna em linhas diferentes.
  stream_definir_atual(0);
  stream_preferir(3);
  stream_folha_abrir();
  // stream_folha_abrir poe o foco na que esta tocando; descer ate a lembrada
  // mostra tambem como a marca se comporta SOB o realce.
  tecla(SDLK_DOWN);   // do grupo de botoes para a lista
  snprintf(nome, sizeof nome, "%s-folha.bmp", saida);
  captura(nome, w);

  // A LEMBRADA (indice 3) SOB O REALCE. E a unica combinacao que prova as duas
  // coisas de uma vez: a marca e a fileira de badges sobre a superficie CLARA.
  // Sem esta captura sobra so a linha 1 (badges sem marca) e a linha 4 (marca
  // nenhuma) — e foi assim que "pilula clara sobre linha clara" passou batido.
  for (i = 0; i < 2; i++) tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-folha-marca.bmp", saida);
  captura(nome, w);

  tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-folha-foco.bmp", saida);
  captura(nome, w);

  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  puts("PASS: capturas da folha de fontes gravadas.");
  return 0;
}
