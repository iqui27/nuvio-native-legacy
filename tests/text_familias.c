// Renderer de tipografia: seleção runtime, pesos, fallback e independência da
// legenda SRT em relação à família da interface. Fora da suite: requer GL.
#include "catalogo.h"
#include "player.h"
#include "faixas.h"
#include "legenda.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "episodios.h"
#include "streams.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint fbo, fboTex;
static SDL_Window *win;

static void quadro(void) {
  txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6);
  player_atualizar(1.f / 60.f, SDL_GetTicks());
  episodios_atualizar(1.f / 60.f);
  stream_folha_atualizar(1.f / 60.f, SDL_GetTicks());
  faixas_atualizar(1.f / 60.f, SDL_GetTicks());
  glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, 1920, 1080);
  glClearColor(.10f, .12f, .16f, 1); glClear(GL_COLOR_BUFFER_BIT);
  player_desenhar(SDL_GetTicks());
  SDL_Delay(2);
}

static void captura(const char *path) {
  for (int i = 0; i < 45; i++) { SDL_PumpEvents(); quadro(); }
  SDL_Surface *s = SDL_CreateRGBSurface(0, 1920, 1080, 24,
                                        0xff, 0xff00, 0xff0000, 0);
  assert(s); glFinish(); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, 1920, 1080, GL_RGB, GL_UNSIGNED_BYTE, s->pixels);
  unsigned char *tmp = malloc((size_t)s->pitch); assert(tmp);
  for (int y = 0; y < 540; y++) {
    memcpy(tmp, (unsigned char *)s->pixels + y * s->pitch, (size_t)s->pitch);
    memcpy((unsigned char *)s->pixels + y * s->pitch,
           (unsigned char *)s->pixels + (1079 - y) * s->pitch, (size_t)s->pitch);
    memcpy((unsigned char *)s->pixels + (1079 - y) * s->pitch, tmp, (size_t)s->pitch);
  }
  free(tmp); assert(SDL_SaveBMP(s, path) == 0); SDL_FreeSurface(s);
  printf("captura: %s\n", path); (void)win;
}

static int largura(TxtEstilo estilo, const char *s) {
  txt_novo_quadro();
  TxtLinha l = txt_linha(estilo, s, 240, 241, 243, 255);
  assert(l.tex && l.w > 0 && l.h > 0);
  return l.w;
}

int main(int argc, char **argv) {
  assert(argc >= 3);
  const char *saida = argv[1], *incompleta = argv[2];
  const char *texto = "The quick brown fox jumps over the lazy dog 0123456789";
  char path[768];
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("Nuvio: fontes", 0, 0, 64, 64,
                         SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(win);
  SDL_GLContext gl = SDL_GL_CreateContext(win); assert(gl);
  SDL_GL_SetSwapInterval(0);
  glGenTextures(1, &fboTex); glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");

  // Padrão Inter e troca runtime: a segunda chamada precisa rasterizar uma
  // nova textura e refletir as métricas reais da Montserrat.
  int inter = largura(TXT_DET_SIN, texto);
  assert(txt_fonte_interface() == TXT_FAMILIA_INTER);
  txt_novo_quadro(); int rasterAntes = txt_rasterizadas;
  txt_definir_fonte_interface(TXT_FAMILIA_MONTSERRAT);
  assert(txt_fonte_interface() == TXT_FAMILIA_MONTSERRAT);
  int montserrat = largura(TXT_DET_SIN, texto);
  assert(txt_rasterizadas == rasterAntes + 1);
  assert(inter != montserrat);

  // Cada nova família carrega sob demanda e apresenta fontes reais em três
  // pesos. Para 25 px, Regular/Medium usam estilos homólogos do renderer.
  const TxtFamilia familias[] = { TXT_FAMILIA_MONTSERRAT, TXT_FAMILIA_ROBOTO,
                                  TXT_FAMILIA_ATKINSON };
  for (int i = 0; i < 3; i++) {
    txt_definir_fonte_interface(familias[i]);
    int reg = largura(TXT_DET_META, texto);
    int med = largura(TXT_DET_BOTAO, texto);
    int bold = largura(TXT_PAINEL_ITEM, texto);
    assert(reg > 0 && med > 0 && bold > 0);
    txt_novo_quadro();
    TxtLinha cortada = txt_linha_corta(TXT_DET_SIN, texto, 240, 241, 243, 255, 600.f);
    txt_novo_quadro();
    TxtLinha cortadaEsperada = txt_linha_corta_familia(
        TXT_DET_SIN, texto, 240, 241, 243, 255, 600.f, familias[i]);
    assert(cortada.tex && cortadaEsperada.tex && cortada.w <= 600);
    assert(cortada.w == cortadaEsperada.w && cortada.h == cortadaEsperada.h);
    printf("%s: regular=%d medium=%d bold=%d\n",
           TXT_FAMILIAS_PT[familias[i]], reg, med, bold);
  }

  // FONTE DE CADA LINHA (30 idiomas): a familia escolhida desenha o que tem; o que
  // ela nao tem vai para a Inter embarcada (grego e cirilico na Atkinson e na
  // Montserrat, vietnamita na Atkinson) e so o CJK sai para uma reserva. A regra
  // e por linha INTEIRA e por QUALQUER caractere que falte, nao pelo primeiro
  // fora do ASCII: "OK · 再生" abre com um "·" que a Inter tem.
  { const char *gr = "Σεπτέμβριος · Ρυθμίσεις", *vi = "Tiếng Việt · Cài đặt",
               *ru = "Настройки", *pl = "Wrzesień · łódź", *ja = "設定 · 再生",
               *misto = "OK · 再生", *r;
    const TxtFamilia todas[] = { TXT_FAMILIA_INTER, TXT_FAMILIA_MONTSERRAT,
                                 TXT_FAMILIA_ROBOTO, TXT_FAMILIA_ATKINSON };
    for (int i = 0; i < 4; i++) {
      const TxtFamilia f = todas[i];
      r = txt_fonte_da_linha(f, TXT_DET_SIN, "Only ASCII 123"); assert(r && !strcmp(r, "principal"));
      r = txt_fonte_da_linha(f, TXT_DET_SIN, pl); assert(r && !strcmp(r, "principal"));
      r = txt_fonte_da_linha(f, TXT_DET_SIN, gr);
      assert(r && !strcmp(r, f == TXT_FAMILIA_INTER || f == TXT_FAMILIA_ROBOTO ? "principal" : "inter"));
      r = txt_fonte_da_linha(f, TXT_DET_SIN, vi);
      assert(r && !strcmp(r, f == TXT_FAMILIA_ATKINSON ? "inter" : "principal"));
      r = txt_fonte_da_linha(f, TXT_DET_SIN, ru);
      assert(r && !strcmp(r, f == TXT_FAMILIA_ATKINSON ? "inter" : "principal"));
      r = txt_fonte_da_linha(f, TXT_DET_SIN, ja); assert(r && !strncmp(r, "reserva:CJK", 11));
      r = txt_fonte_da_linha(f, TXT_DET_SIN, misto); assert(r && !strncmp(r, "reserva:CJK", 11));
      /* e nenhuma delas devolve linha vazia ou de largura zero */
      assert(largura(TXT_DET_SIN, gr) > 0 && largura(TXT_DET_SIN, vi) > 0 && largura(TXT_DET_SIN, ja) > 0);
    }
    /* decorativo que a Inter nao tem NAO derruba a linha para a reserva */
    r = txt_fonte_da_linha(TXT_FAMILIA_INTER, TXT_DET_SIN, "\xe2\x9a\xa1 1080p"); assert(r && !strcmp(r, "principal"));
  }

  // O WASM da Samsung nao tem fonte de sistema: so a CJK embarcada. Reabre o
  // renderer fingindo isso (NUVIO_SEM_RESERVA_DE_SISTEMA) e confere que ja e zh
  // saem da DroidSansFallback-Subset.ttf, e que a linha existe.
  txt_definir_fonte_interface(TXT_FAMILIA_INTER);
  txt_encerrar();
  setenv("NUVIO_SEM_RESERVA_DE_SISTEMA", "1", 1);
  assert(txt_iniciar("deploy/app", 1));
  { const char *r = txt_fonte_da_linha(TXT_FAMILIA_INTER, TXT_DET_SIN, "\xe8\xae\xbe\xe7\xbd\xae \xc2\xb7 \xe6\x92\xad\xe6\x94\xbe");
    assert(r && strstr(r, "DroidSansFallback-Subset.ttf") && !strncmp(r, "reserva:CJK", 11));
    assert(largura(TXT_DET_SIN, "\xe8\xae\xbe\xe7\xbd\xae \xc2\xb7 \xe6\x92\xad\xe6\x94\xbe") > 0);
    assert(largura(TXT_DET_SIN, "\xe8\xa8\xad\xe5\xae\x9a") > 0);
    r = txt_fonte_da_linha(TXT_FAMILIA_ATKINSON, TXT_DET_SIN, "\xce\xa3\xce\xb5\xcf\x80"); assert(r && !strcmp(r, "inter"));
    // Arabe na Samsung (#253, #258): letras base e as formas de apresentacao
    // que o bidi.c produz saem da Noto Sans Arabic embarcada.
    r = txt_fonte_da_linha(TXT_FAMILIA_INTER, TXT_DET_SIN, "\xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7");
    assert(r && strstr(r, "NotoSansArabic-Regular-Subset.ttf") && !strncmp(r, "reserva:arabe", 13));
    r = txt_fonte_da_linha(TXT_FAMILIA_INTER, TXT_DET_SIN, "\xef\xbb\xa3\xef\xba\xae\xef\xba\xa3\xef\xba\x92\xef\xba\x8e");
    assert(r && strstr(r, "NotoSansArabic-Regular-Subset.ttf"));
    assert(largura(TXT_DET_SIN, "\xef\xbb\xa3\xef\xba\xae\xef\xba\xa3\xef\xba\x92\xef\xba\x8e") > 0); }
  txt_encerrar();
  unsetenv("NUVIO_SEM_RESERVA_DE_SISTEMA");
  assert(txt_iniciar("deploy/app", 1));
  txt_definir_fonte_interface(TXT_FAMILIA_ATKINSON);

  // O family explícito da legenda é independente do getter global. A largura
  // Inter fica igual, mesmo com Atkinson selecionada para a interface.
  txt_definir_fonte_interface(TXT_FAMILIA_ATKINSON);
  txt_novo_quadro();
  TxtLinha subInterAntes = txt_linha_familia(TXT_LEG_100,
      "Legenda normal: ação, coração e acentuação", 255, 255, 255, 255,
      TXT_FAMILIA_INTER);
  txt_definir_fonte_interface(TXT_FAMILIA_ROBOTO);
  txt_novo_quadro();
  TxtLinha subInterDepois = txt_linha_familia(TXT_LEG_100,
      "Legenda normal: ação, coração e acentuação", 255, 255, 255, 255,
      TXT_FAMILIA_INTER);
  assert(subInterAntes.tex && subInterDepois.tex);
  assert(subInterAntes.w == subInterDepois.w && subInterAntes.h == subInterDepois.h);

  // Preview SRT pelo player: frase idêntica em cada família da legenda, com a
  // interface fixada em Atkinson. A última captura comprova as preferências
  // independentes: UI Atkinson e legenda Inter.
  { CatItem c; memset(&c, 0, sizeof c); snprintf(c.tipo, sizeof c.tipo, "movie");
    snprintf(c.titulo, sizeof c.titulo, "Tipografia no player"); cat_definir(&c, 1); }
  player_abrir(0, NULL); player_erro_fonte(); player_limpar_erro_fonte();
  txt_definir_fonte_interface(TXT_FAMILIA_ATKINSON);
  static const char SRT[] = "1\n00:00:00,000 --> 00:00:30,000\n"
    "Legenda SRT: ação, coração, 0123.\n";
  legenda_definir_corpo(SRT);
  const TxtFamilia legendas[] = { TXT_FAMILIA_INTER, TXT_FAMILIA_MONTSERRAT,
                                  TXT_FAMILIA_ROBOTO, TXT_FAMILIA_ATKINSON };
  const char *ids[] = { "inter", "montserrat", "roboto", "atkinson" };
  for (int i = 0; i < 4; i++) {
    *player_leg_estilo() = (VideoLegendaEstilo){ 120, 0, 0, 3, 1, 0, 0, legendas[i] };
    player_leg_estilo_tocou(PLR_LEG_NADA);
    snprintf(path, sizeof path, "%s-legenda-%s-ui-atkinson.bmp", saida, ids[i]);
    captura(path);
  }
  // Comparação visual da família da interface, com a mesma legenda Inter.
  *player_leg_estilo() = (VideoLegendaEstilo){ 120, 0, 0, 3, 1, 0, 0, TXT_FAMILIA_INTER };
  const TxtFamilia interfaces[] = { TXT_FAMILIA_INTER, TXT_FAMILIA_MONTSERRAT,
                                    TXT_FAMILIA_ROBOTO, TXT_FAMILIA_ATKINSON };
  for (int i = 0; i < 4; i++) {
    txt_definir_fonte_interface(interfaces[i]);
    snprintf(path, sizeof path, "%s-interface-%s-legenda-inter.bmp", saida, ids[i]);
    captura(path);
  }
  *player_leg_estilo() = (VideoLegendaEstilo){ 120, 0, 0, 3, 1, 0, 0, TXT_FAMILIA_INTER };
  snprintf(path, sizeof path, "%s-independencia-ui-atkinson-legenda-inter.bmp", saida);
  captura(path);

  // Ausência de fonte: outro init com pacote mínimo deve desenhar em Inter sem
  // perder a preferência solicitada nem vazar buffers da primeira abertura.
  txt_encerrar();
  assert(txt_iniciar(incompleta, 1));
  txt_definir_fonte_interface(TXT_FAMILIA_MONTSERRAT);
  txt_novo_quadro();
  TxtLinha fallback = txt_linha(TXT_DET_SIN, texto, 240, 241, 243, 255);
  txt_novo_quadro();
  TxtLinha esperado = txt_linha_familia(TXT_DET_SIN, texto, 240, 241, 243, 255,
                                        TXT_FAMILIA_INTER);
  assert(fallback.tex && esperado.tex && fallback.w == esperado.w && fallback.h == esperado.h);
  puts("text_familias: runtime/cache/independência/fallback ok");
  txt_encerrar();
  tex_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(win); IMG_Quit(); SDL_Quit();
  return 0;
}
