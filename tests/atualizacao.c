// Aviso de atualizacao: comparacao de versao, leitura do JSON do GitHub e
// limpeza das notas — e uma captura do cartao para ser OLHADA.
//
//   bash tests/atualizacao.sh            # so as conferencias
//   bash tests/atualizacao.sh /tmp/x.bmp # e a captura
//
// Inclui o .c de proposito: as funcoes de parse sao estaticas, e expo-las so
// para o teste seria API a mais. O .sh compila tudo MENOS src/atualizacao.c.
#define NV_VERSAO "1.0.53"
#include "../src/atualizacao.c"
#include "tex_cache.h"
#include <SDL2/SDL_image.h>
#include <assert.h>

static int falhas = 0;
#define CONFERE(c, ...) do { if (!(c)) { falhas++; printf("FALHA: " __VA_ARGS__); printf("\n"); } } while (0)

static char *lerArquivo(const char *nome) {
  FILE *f = fopen(nome, "rb"); long n; char *s;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  s = malloc((size_t)n + 1); fread(s, 1, (size_t)n, f); s[n] = 0; fclose(f);
  return s;
}

int main(int argc, char **argv) {
  CONFERE(maisNova("1.0.54", "1.0.53"), "1.0.54 > 1.0.53");
  CONFERE(!maisNova("1.0.53", "1.0.53"), "igual nao e mais nova");
  CONFERE(!maisNova("1.0.52", "1.0.53"), "1.0.52 nao e mais nova");
  CONFERE(maisNova("1.1.0", "1.0.99"), "1.1.0 > 1.0.99");
  CONFERE(maisNova("2.0", "1.9.9"), "2.0 > 1.9.9");
  CONFERE(maisNova("1.0.53", "dev"), "qualquer tag e mais nova que dev");

  { char tag[48] = "", body[8192] = "";
    char *j = lerArquivo("tests/atualizacao_latest.json");
    CONFERE(j != NULL, "fixture tests/atualizacao_latest.json");
    if (j) {
      CONFERE(textoJson(j, "tag_name", tag, sizeof tag), "tag_name");
      CONFERE(tag[0] == 'v', "tag comeca com v: %s", tag);
      CONFERE(textoJson(j, "body", body, sizeof body), "body");
      CONFERE(strchr(body, '\n') != NULL, "body tem quebra de linha de verdade");
      CONFERE(strstr(body, "\\n") == NULL, "body sem \\n literal");
      limparNotas(body, notas, sizeof notas);
      printf("--- notas limpas ---\n%s--------------------\n", notas);
      CONFERE(strstr(notas, "\001Fixed") != NULL || strstr(notas, "\001Added") != NULL, "secao virou \\x01");
      CONFERE(strstr(notas, "\xe2\x80\xa2 ") != NULL, "item virou bala");
      CONFERE(strstr(notas, "**") == NULL, "sem negrito markdown");
      CONFERE(strstr(notas, "Notes") == NULL, "secao Notes cortada");
      CONFERE(strstr(notas, ".wgt") == NULL, "rodape de instalacao cortado");
      free(j);
    } }

  // JSON com escapes variados
  { char d[64];
    CONFERE(textoJson("{\"body\":\"a\\nb \\\"c\\\" \\u00e9 d\"}", "body", d, sizeof d), "escapes");
    CONFERE(!strcmp(d, "a\nb \"c\"   d"), "escapes decodificados: [%s]", d); }

  printf(falhas ? "atualizacao: %d falhas\n" : "atualizacao: ok\n", falhas);
  if (argc < 2 || falhas) return falhas ? 1 : 0;

  // captura
  { SDL_Window *w; SDL_GLContext gl; int i;
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    w = SDL_CreateWindow("atualizacao", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    assert(w); gl = SDL_GL_CreateContext(w); assert(gl);
    SDL_GL_SetSwapInterval(0);
    glViewport(0, 0, 1920, 1080);
    gfx_tamanho_alvo(1920, 1080);
    assert(gfx_iniciar());
    assert(txt_iniciar("deploy/app", 1));
    tex_iniciar(64);
    snprintf(tagNova, sizeof tagNova, "1.0.54");
    aberto = 1;
    for (i = 0; i < 60; i++) {
      SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro();
      atualizacao_atualizar(1.0f / 60.0f, SDL_GetTicks());
      glClearColor(0.025f, 0.025f, 0.03f, 1.0f); glClear(GL_COLOR_BUFFER_BIT);
      atualizacao_desenhar(SDL_GetTicks());
      if (i == 59) {
        unsigned char *pix = malloc(1920 * 1080 * 4); SDL_Surface *s; int y;
        glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
        s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
        for (y = 0; y < 1080; y++)
          memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
        assert(SDL_SaveBMP(s, argv[1]) == 0);
        SDL_FreeSurface(s); free(pix);
      }
      SDL_GL_SwapWindow(w);
    }
    printf("captura: %s\n", argv[1]); }
  return 0;
}
