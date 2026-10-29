// txt_largura (medida sem rasterizar) tem de bater com a largura da linha
// rasterizada, e txt_bloco/txt_linha_corta so podem rasterizar as linhas FINAIS
// (antes rasterizavam cada prefixo tentado). Fora da suite: requer GL.
#include "gfx.h"
#include "text.h"
#include "legfontes.h"
#include "player.h"
#include <SDL2/SDL_ttf.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *win = SDL_CreateWindow("t", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(win);
  SDL_GLContext gl = SDL_GL_CreateContext(win); assert(gl);
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));

  const char *amostras[] = {
    "A", "A vida de", "Uma familia comum descobre um segredo antigo",
    "Ação · Aventura · Ficção científica", "The quick brown fox jumps over",
    "Diretor: Christopher Nolan", "çãõéÉ àü — “aspas” …",
  };
  const TxtEstilo estilos[] = { TXT_DET_SIN, TXT_DET_META2, TXT_CAPTION, TXT_TITULO1, TXT_DET_BOTAO };
  int n = 0;
  for (unsigned e = 0; e < sizeof estilos / sizeof *estilos; e++)
    for (unsigned i = 0; i < sizeof amostras / sizeof *amostras; i++) {
      txt_novo_quadro();
      int m = txt_largura(estilos[e], amostras[i]);
      TxtLinha l = txt_linha(estilos[e], amostras[i], 255, 255, 255, 255);
      if (m != l.w) { printf("DIVERGE est=%d '%s': medida %d, linha %d\n", estilos[e], amostras[i], m, l.w); return 1; }
      n++;
    }
  printf("larguras iguais em %d combinacoes\n", n);

  const char *sin = "Quando uma familia comum descobre um segredo antigo escondido no porao da casa, "
    "todos precisam decidir entre a verdade e a seguranca, enquanto lá fora alguem observa cada "
    "movimento e espera o momento certo para agir contra eles e contra tudo o que construiram juntos.";
  txt_novo_quadro();
  int r0 = txt_rasterizadas;
  float h = txt_bloco(TXT_DET_SIN, sin, 255, 255, 255, 0, 0, 1040.0f, 40.0f, 1.0f, 5);
  int usadas = txt_rasterizadas - r0;
  int linhas = (int)(h / 40.0f + 0.5f);
  printf("sinopse: %d linhas, %d rasterizadas\n", linhas, usadas);
  assert(linhas >= 2 && usadas <= linhas);

  txt_novo_quadro();
  r0 = txt_rasterizadas;
  TxtLinha c = txt_linha_corta(TXT_DET_SIN, sin, 255, 255, 255, 255, 600.0f);
  printf("corta: w=%d (<=600), %d rasterizadas\n", c.w, txt_rasterizadas - r0);
  assert(c.w > 0 && c.w <= 600 && txt_rasterizadas - r0 <= 2);
  // A private imported face must reach both measuring and rasterizing. The
  // real SDL_ttf face is the independent metric oracle, not the registry label.
  char dir[] = "/tmp/nuvio-font-render-XXXXXX", staged[512];
  assert(mkdtemp(dir)); player_dir(dir);
  const char *sampleFont = getenv("NV_TEST_FONT");
  if (!sampleFont) sampleFont = "deploy/app/fonts/JetBrainsMonoNL-Regular.ttf";
  snprintf(staged, sizeof staged, "%s/fontes-importadas/entrada-ab.tmp", dir);
  FILE *src = fopen(sampleFont, "rb"), *dst = fopen(staged, "wb");
  assert(src && dst);
  char bytes[8192]; size_t z;
  while ((z = fread(bytes, 1, sizeof bytes, src))) assert(fwrite(bytes, 1, z, dst) == z);
  assert(!ferror(src)); fclose(src); assert(!fclose(dst));
  assert(legfontes_importar(0, "entrada-ab.tmp"));
  assert(legfontes_escolher(1, 0));
  const char *sample = "iiiiWWWW0123";
  int size, weight, expected, height;
  txt_estilo_medidas(TXT_LEG_120, &size, &weight);
  TTF_Font *face = TTF_OpenFont(legfontes_caminho(0), size); assert(face);
  assert(!TTF_SizeUTF8(face, sample, &expected, &height)); TTF_CloseFont(face);
  int builtin = txt_largura_familia(TXT_LEG_120, sample, TXT_FAMILIA_INTER);
  assert(expected > 0 && expected != builtin);
  txt_novo_quadro();
  assert(txt_largura_familia(TXT_LEG_120, sample, TXT_FAMILIA_IMPORTADA1) == expected);
  assert(txt_largura_familia(TXT_LEG_120, sample, TXT_FAMILIA_IMPORTADA2) == expected);
  assert(txt_linha_familia(TXT_LEG_120, sample, 255, 255, 255, 255, TXT_FAMILIA_IMPORTADA1).w == expected);
  assert(txt_linha_familia(TXT_LEG_120, sample, 255, 255, 255, 255, TXT_FAMILIA_IMPORTADA2).w == expected);
  // Exercise the actual player preference transaction, not a replacement
  // callback: failed builtin/Restore writes retain both imported selections
  // and live style; successful actions clear only the primary override.
  player_leg_estilo_carregar();
  VideoLegendaEstilo previous = *player_leg_estilo();
  int touched = player_leg_estilo_tocado(PLR_LEG_FONTE);
  snprintf(staged, sizeof staged, "%s/player.txt.tmp", dir); assert(!mkdir(staged, 0700));
  assert(!player_leg_fonte_definir(TXT_FAMILIA_INTER));
  assert(!player_leg_estilo_restaurar());
  assert(!memcmp(&previous, player_leg_estilo(), sizeof previous));
  assert(player_leg_estilo_tocado(PLR_LEG_FONTE) == touched);
  assert(legfontes_selecionada(0) == 0 && legfontes_selecionada(1) == 0);
  assert(!rmdir(staged));
  assert(player_leg_fonte_definir(TXT_FAMILIA_INTER));
  assert(legfontes_selecionada(0) == -1 && legfontes_selecionada(1) == 0);
  assert(legfontes_escolher(0, 0));
  assert(player_leg_estilo_restaurar());
  assert(legfontes_selecionada(0) == -1 && legfontes_selecionada(1) == 0);
  assert(legfontes_escolher(0, 0));
  // Removal invalidates all custom width/glyph/texture caches and restores a
  // bundled fallback even when the allocator later reuses a face pointer.
  assert(legfontes_remover(0)); txt_novo_quadro();
  assert(txt_largura_familia(TXT_LEG_120, sample, TXT_FAMILIA_IMPORTADA1) == builtin);
  assert(txt_linha_familia(TXT_LEG_120, sample, 255, 255, 255, 255, TXT_FAMILIA_IMPORTADA2).w == builtin);
  snprintf(staged, sizeof staged, "%s/fontes-legenda.txt", dir); assert(!unlink(staged));
  snprintf(staged, sizeof staged, "%s/player.txt", dir); assert(!unlink(staged));
  snprintf(staged, sizeof staged, "%s/fontes-importadas", dir); assert(!rmdir(staged)); assert(!rmdir(dir));
  puts("imported subtitle font: real face metrics, both slots, raster and delete fallback passed");
  puts("imported subtitle font: actual player builtin/Restore transaction passed");
  puts("ok");
  return 0;
}
