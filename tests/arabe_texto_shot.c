// Captura de texto ARABE vindo do TMDB/addon pelos caminhos de desenho comuns
// (linha, linha cortada com reticencias, bloco quebrado). Relato do .tpk 4/5
// (Tizen 5): sinopse, elenco e meta em quadradinhos ou com as letras soltas e
// invertidas. Fora da suite: precisa de janela GL e de olho para julgar.
//
//   NUVIO_SEM_RESERVA_DE_SISTEMA=1 bash tests/arabe_texto_shot.sh /tmp/saida.bmp
//
// Com NUVIO_SEM_RESERVA_DE_SISTEMA=1 so as fontes embarcadas existem, como na
// Samsung (o arabe sai da NotoNaskhArabic-Subset.ttf, que nao tem latim).
#include "gfx.h"
#include "nvtmp.h"
#include "text.h"
#include "ajustes.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *SINOPSE =
  "\xe2\x80\x8f" "بعد أن فقد كل شيء في عام 2008، يعود جون ويك (كيانو ريفز) إلى نيويورك "
  "ليواجه «الطاولة العليا» من جديد - هل سينجو هذه المرة؟ مُسَلْسَلٌ رائــع من إنتاج "
  "Lionsgate بميزانية 100 مليون دولار.";

static void desenhar(void) {
  float y = 40.0f;
  const float x = 80.0f;
  txt_desenhar(txt_linha(TXT_DET_META2, "1. txt_linha (nome do ator, meta, episodio)", 150, 150, 160, 255), x, y);
  y += 44;
  txt_desenhar(txt_linha(TXT_DET_META, "كيانو ريفز", 255, 255, 255, 255), x, y); y += 48;
  txt_desenhar(txt_linha(TXT_DET_META, "2023 · أكشن · إثارة · 2س 49د", 255, 255, 255, 255), x, y); y += 48;
  txt_desenhar(txt_linha(TXT_DET_META, "الحلقة 3: المواجهة الأخيرة", 255, 255, 255, 255), x, y); y += 48;
  txt_desenhar(txt_linha(TXT_DET_META, "مسلسل Breaking Bad من إنتاج AMC، الموسم 5", 255, 255, 255, 255), x, y); y += 48;
  txt_desenhar(txt_linha(TXT_DET_META, "\xe2\x80\xaa" "جون ويك / John Wick" "\xe2\x80\xac" " \xe2\x80\x8e(2014)", 255, 255, 255, 255), x, y); y += 64;
  txt_desenhar(txt_linha(TXT_DET_META2, "2. txt_linha_corta 700 px", 150, 150, 160, 255), x, y); y += 44;
  txt_desenhar(txt_linha_corta(TXT_DET_SIN, SINOPSE, 255, 255, 255, 255, 700.0f), x, y); y += 64;
  txt_desenhar(txt_linha(TXT_DET_META2, "3. txt_bloco 1040 px, 5 linhas (sinopse)", 150, 150, 160, 255), x, y); y += 44;
  y += txt_bloco(TXT_DET_SIN, SINOPSE, 255, 255, 255, x, y, 1040.0f, 40.0f, 1.0f, 5) + 20;
  txt_desenhar(txt_linha(TXT_DET_META2, "4. txt_bloco_corta 1040 px, 2 linhas", 150, 150, 160, 255), x, y); y += 44;
  y += txt_bloco_corta(TXT_DET_SIN, SINOPSE, 255, 255, 255, x, y, 1040.0f, 40.0f, 1.0f, 2) + 20;
  txt_desenhar(txt_linha(TXT_DET_META2, "5. latim igual a antes", 150, 150, 160, 255), x, y); y += 44;
  txt_desenhar(txt_linha(TXT_DET_SIN, "Ação · Aventura — “John Wick 4” (2023) …", 255, 255, 255, 255), x, y);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-arabe-texto.bmp");
  SDL_Window *w;
  SDL_GLContext gl;
  int i;
  { const char *dir = getenv("NUVIO_DADOS");
    if (dir && *dir) ajustes_dir(dir); }
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("arabe", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  // A TV usa Montserrat (a Inter e a do Mac).
  txt_definir_fonte_interface(getenv("NUVIO_SHOT_INTER") ? TXT_FAMILIA_INTER : TXT_FAMILIA_MONTSERRAT);
  for (i = 0; i < 30; i++) {
    txt_novo_quadro();
    glClearColor(0.05f, 0.05f, 0.06f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    desenhar();
    if (i == 29) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s;
      int y;
      assert(pix);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      assert(s);
      for (y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(SDL_SaveBMP(s, saida) == 0);
      SDL_FreeSurface(s);
      free(pix);
    }
    SDL_GL_SwapWindow(w);
  }
  printf("captura: %s\n", saida);
  return 0;
}
