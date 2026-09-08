// A PONTE DE WEBP DO ALVO TIZEN, exercitada num Chromium de verdade.
//
// src/webp.c no Emscripten devolve o arquivo ao navegador (createImageBitmap
// no fio principal) e dorme num futex ate a resposta. Nada disso da para
// conferir com -fsyntax-only, e a Samsung nao esta na bancada — entao este
// teste monta exatamente o mesmo arranjo: um pthread chama webp_carregar
// enquanto o fio principal roda o laco de eventos.
//
// Roda por tests/webp-tizen.sh, que serve a pagina com COOP/COEP (sem os dois
// cabecalhos nao ha SharedArrayBuffer, logo nao ha pthread, logo nao ha teste).
#include "../src/webp.h"
#include <pthread.h>
#include <stdio.h>

static void *fioDeDecode(void *arg) {
  SDL_Surface *s;
  (void)arg;
  s = webp_carregar("/amostra.webp");
  if (!s) { printf("FALHOU: webp_carregar devolveu NULL\n"); return NULL; }
  printf("ok  webp %dx%d formato=%s\n", s->w, s->h,
         s->format->format == SDL_PIXELFORMAT_ABGR8888 ? "ABGR8888" : "ERRADO");
  // Um pixel do meio, para provar que veio imagem e nao um bloco de zeros.
  // NADA DE EM_ASM AQUI. Este fio e um Worker: `window` nao existe nele, e a
  // primeira versao deste teste morreu justamente assim — com a ponte ja tendo
  // funcionado, o que faz o erro parecer da ponte. O console basta.
  { unsigned char *p = (unsigned char *)s->pixels + (s->h / 2) * s->pitch + (s->w / 2) * 4;
    printf("    pixel central rgba=%d,%d,%d,%d\n", p[0], p[1], p[2], p[3]);
  }
  SDL_FreeSurface(s);
  // Nao e WebP: NULL sem alarde, o mesmo contrato do teste nativo.
  if (webp_carregar("/nao-e-webp.txt") != NULL) printf("FALHOU: aceitou nao-webp\n");
  else printf("webp: tudo ok\n");
  return NULL;
}

int main(void) {
  pthread_t t;
  // O fio principal PRECISA voltar ao laco de eventos: e nele que a chamada
  // proxiada roda e que a promessa do createImageBitmap resolve. main() retorna
  // e o runtime segue vivo (EXIT_RUNTIME=0), que e o mesmo desenho do app.
  pthread_create(&t, NULL, fioDeDecode, NULL);
  pthread_detach(t);
  return 0;
}
