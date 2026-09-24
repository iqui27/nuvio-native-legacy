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
#include "../src/jpegrapido.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL_image.h>
#include <emscripten.h>

typedef struct {
  int zero;
  int opaque;
  int max;
} AlphaStats;

/* Read alpha through SDL's format masks, so this covers both the browser
 * ABGR8888 surface and the legacy IMG_Load_RW result. */
static AlphaStats alphaStats(const SDL_Surface *s) {
  AlphaStats a = { 0, 0, 0 };
  int x, y;
  if (!s || !s->format || !s->format->Amask) return a;
  if (SDL_MUSTLOCK((SDL_Surface *)s)) SDL_LockSurface((SDL_Surface *)s);
  for (y = 0; y < s->h; y++) for (x = 0; x < s->w; x++) {
    Uint32 p = 0;
    Uint8 r, g, b, al;
    memcpy(&p, (const Uint8 *)s->pixels + y * s->pitch + x * s->format->BytesPerPixel,
           s->format->BytesPerPixel);
    SDL_GetRGBA(p, s->format, &r, &g, &b, &al);
    if (al == 0) a.zero++;
    if (al == 255) a.opaque++;
    if (al > a.max) a.max = al;
  }
  if (SDL_MUSTLOCK((SDL_Surface *)s)) SDL_UnlockSurface((SDL_Surface *)s);
  return a;
}

static SDL_Surface *legacyPng(const unsigned char *dados, long n) {
  SDL_RWops *rw = SDL_RWFromConstMem(dados, (int)n);
  return rw ? IMG_Load_RW(rw, 1) : NULL;
}

static void *fioDeDecode(void *arg) {
  SDL_Surface *s;
  (void)arg;
  // PNG 4K pelo navegador. Este e o primeiro decode de proposito: webp.c
  // imprime a origem do primeiro pedido, permitindo ao console provar
  // "image/png, no worker"; uma execucao sem decodificador.js deve registrar
  // "image/png, no fio principal".
  { unsigned char *dados; long n; FILE *f = fopen("/amostra-4k.png", "rb");
    Uint32 inicio;
    if (!f) { printf("FALHOU: amostra-4k.png nao preloadado\n"); return NULL; }
    fseek(f, 0, SEEK_END); n = ftell(f); rewind(f);
    dados = malloc((size_t)n);
    if (!dados || fread(dados, 1, (size_t)n, f) != (size_t)n) {
      printf("FALHOU: leitura de amostra-4k.png\n"); fclose(f); free(dados); return NULL;
    }
    fclose(f);
    inicio = SDL_GetTicks();
    { int ow = 0, oh = 0;
      s = jpeg_rapido_carregar_mem(dados, (size_t)n, 1280, &ow, &oh);
      if (!s) printf("FALHOU: png 4K reduzido devolveu NULL\n");
      else {
        unsigned char *p = (unsigned char *)s->pixels + (s->h / 4) * s->pitch + (s->w / 4) * 4;
        int dimensoes = s->w == 1280 && s->h == 720 && ow == 3840 && oh == 2160;
        int pixel = abs((int)p[0] - 220) <= 2 && abs((int)p[1] - 30) <= 2 &&
                    abs((int)p[2] - 40) <= 2 && p[3] == 255;
        printf("%s png 4K reduzido %dx%d (arquivo %dx%d, pixel=%d,%d,%d,%d, ms=%u)\n",
               dimensoes && pixel ? "ok " : "FALHOU:", s->w, s->h, ow, oh,
               p[0], p[1], p[2], p[3], (unsigned)(SDL_GetTicks() - inicio));
        SDL_FreeSurface(s);
      }
    }
    free(dados);
  }
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
  // WEBP REDUZIDO PELO NAVEGADOR: pedido a 320 sai 320 e diz o tamanho do
  // arquivo. E o caminho dos fundos 3840x2160 do Xperience no Tizen.
  { int ow = 0, oh = 0;
    SDL_Surface *r = webp_carregar_larg("/amostra.webp", 320, &ow, &oh);
    if (!r) printf("FALHOU: webp reduzido devolveu NULL\n");
    else {
      printf("%s webp reduzido %dx%d (arquivo %dx%d)\n",
             (r->w == 320 && ow == 1477 && oh == 980) ? "ok " : "FALHOU:", r->w, r->h, ow, oh);
      SDL_FreeSurface(r);
    } }
  // Nao e WebP: NULL sem alarde, o mesmo contrato do teste nativo.
  if (webp_carregar("/nao-e-webp.txt") != NULL) printf("FALHOU: aceitou nao-webp\n");
  else printf("webp: tudo ok\n");
  // JPEG PELO NAVEGADOR, JA REDUZIDO: um 640 pedido a 320 sai 320 de largura
  // e diz que o arquivo tinha 640. E o caminho que substitui o IMG_Load em
  // software no Tizen (ver jpegrapido.c).
  { int ow = 0, oh = 0;
    s = jpeg_rapido_carregar("/amostra.jpg", 320, &ow, &oh);
    if (!s) printf("FALHOU: jpeg_rapido_carregar devolveu NULL\n");
    else {
      printf("%s jpeg %dx%d (arquivo %dx%d)\n",
             (s->w == 320 && ow == 640 && oh > 0 && s->h == (oh * 320 + ow / 2) / ow) ? "ok " : "FALHOU:",
             s->w, s->h, ow, oh);
      SDL_FreeSurface(s);
    }
    // Pedido maior que o arquivo: sai inteiro.
    s = jpeg_rapido_carregar("/amostra.jpg", 4000, &ow, &oh);
    if (!s) printf("FALHOU: jpeg inteiro devolveu NULL\n");
    else { printf("%s jpeg inteiro %dx%d\n", s->w == 640 ? "ok " : "FALHOU:", s->w, s->h); SDL_FreeSurface(s); }
    if (jpeg_rapido_carregar("/nao-e-webp.txt", 320, &ow, &oh) != NULL) printf("FALHOU: aceitou nao-imagem\n");
    else printf("jpeg: tudo ok\n"); }
  // PNG PELO NAVEGADOR (21/09/2026, registro 1106): o caso pequeno preserva
  // a regressao original, mas o caso 4K acima e a prova de que a reducao nao
  // depende de um fixture pequeno.
  { unsigned char *dados; long n; FILE *f = fopen("/amostra.png", "rb");
    if (!f) { printf("FALHOU: amostra.png nao preloadado\n"); return NULL; }
    fseek(f, 0, SEEK_END); n = ftell(f); rewind(f);
    dados = malloc((size_t)n);
    if (fread(dados, 1, (size_t)n, f) != (size_t)n) { printf("FALHOU: leitura de amostra.png\n"); }
    fclose(f);
    { int ow = 0, oh = 0;
      s = jpeg_rapido_carregar_mem(dados, (size_t)n, 64, &ow, &oh);
      if (!s) printf("FALHOU: png reduzido devolveu NULL\n");
      else {
        // 160x160 e PNG PEQUENO (<= 256): desde 22/09/2026 decodifica local,
        // em tamanho de arquivo; a reducao a 64 fica para o tex_reduzir do
        // tex_cache, como no LG. Ver jpegrapido.c.
        printf("%s png pequeno local %dx%d (arquivo %dx%d, pedido 64)\n",
               (s->w == 160 && ow == 160 && oh == 160) ? "ok " : "FALHOU:", s->w, s->h, ow, oh);
        SDL_FreeSurface(s);
      } }
    { int ow = 0, oh = 0;
      s = jpeg_rapido_carregar_mem(dados, (size_t)n, 4000, &ow, &oh);
      if (!s) printf("FALHOU: png inteiro devolveu NULL\n");
      else { printf("%s png inteiro %dx%d\n", s->w == 160 ? "ok " : "FALHOU:", s->w, s->h); SDL_FreeSurface(s); } }
    free(dados);
    printf("png: tudo ok\n"); }
  // PNG transparente: sao os mesmos tres icones que a tela de menu usa.
  // O teste 4K acima so tem alpha=255 e, portanto, nao detecta a regressao
  // em que a ponte browser entregava um quadrado opaco. Medimos a superficie
  // depois de jpeg_rapido_carregar_mem, isto e, depois da rota PNG do Tizen.
  // Tambem repetimos a rota legada IMG_Load_RW -> SDL_ConvertSurfaceFormat;
  // assim o resultado separa uma mudanca no arquivo de uma mudanca na ponte.
  { const char *nomes[] = { "/icone-home.png", "/icone-guide.png", "/icone-search.png", NULL };
    int i;
    for (i = 0; nomes[i]; i++) {
      unsigned char *dados = NULL; long n; FILE *f = fopen(nomes[i], "rb");
      AlphaStats direto = {0, 0, 0}, convertido = {0, 0, 0};
      AlphaStats legado = {0, 0, 0}, legadoConvertido = {0, 0, 0};
      int ow = 0, oh = 0, mesmaPonte = 0, mesmaRota = 0, mesmaFonte = 0;
      SDL_BlendMode bm = SDL_BLENDMODE_NONE; Uint8 am = 0; Uint32 key = 0;
      int chaveAusente = 0;
      SDL_Surface *ic = NULL, *cv = NULL, *old = NULL, *oldCv = NULL;
      if (!f) { printf("FALHOU: %s nao preloadado\n", nomes[i]); continue; }
      fseek(f, 0, SEEK_END); n = ftell(f); rewind(f);
      dados = malloc((size_t)n);
      if (!dados || fread(dados, 1, (size_t)n, f) != (size_t)n) {
        printf("FALHOU: leitura de %s\n", nomes[i]); fclose(f); free(dados); continue;
      }
      fclose(f);
      ic = jpeg_rapido_carregar_mem(dados, (size_t)n, 256, &ow, &oh);
      old = legacyPng(dados, n);
      if (ic) {
        SDL_GetSurfaceBlendMode(ic, &bm); SDL_GetSurfaceAlphaMod(ic, &am);
        chaveAusente = SDL_GetColorKey(ic, &key) == -1;
        direto = alphaStats(ic);
        // Este e o ponto que o caminho de decode antigo fazia antes do
        // upload. O resultado precisa conservar alpha0 e alpha255.
        cv = SDL_ConvertSurfaceFormat(ic, SDL_PIXELFORMAT_ABGR8888, 0);
        convertido = alphaStats(cv);
      }
      if (old) {
        oldCv = SDL_ConvertSurfaceFormat(old, SDL_PIXELFORMAT_ABGR8888, 0);
        legado = alphaStats(old);
        legadoConvertido = alphaStats(oldCv);
      }
      mesmaPonte = cv && direto.zero > 0 && direto.opaque > 0 && direto.max == 255 &&
                   convertido.zero == direto.zero && convertido.opaque == direto.opaque &&
                   convertido.max == direto.max;
      mesmaRota = oldCv && legado.zero > 0 && legado.opaque > 0 && legado.max == 255 &&
                  legadoConvertido.zero == legado.zero && legadoConvertido.opaque == legado.opaque &&
                  legadoConvertido.max == legado.max;
      mesmaFonte = mesmaPonte && mesmaRota && convertido.zero == legadoConvertido.zero &&
                   convertido.opaque == legadoConvertido.opaque && convertido.max == legadoConvertido.max;
      // Formato e metadata tambem fazem parte do contrato da ponte: gfx sobe
      // o buffer como RGBA e o shader GFX_MARCA usa a alpha da textura.
      printf("%s png transparente %s %dx%d arquivo %dx%d ponte=%d/%d/%d->%d/%d/%d legado=%d/%d/%d->%d/%d/%d formato=%s->%s old=%s->%s blend=%d alphaMod=%u colorkey=%s\n",
             mesmaFonte && ic->format->Amask != 0 && cv->format->Amask != 0 &&
             old->format->Amask != 0 && oldCv->format->Amask != 0 &&
             bm == SDL_BLENDMODE_BLEND && am == 255 && chaveAusente ? "ok " : "FALHOU:",
             nomes[i], ic ? ic->w : 0, ic ? ic->h : 0, ow, oh,
             direto.zero, direto.opaque, direto.max, convertido.zero, convertido.opaque, convertido.max,
             legado.zero, legado.opaque, legado.max, legadoConvertido.zero, legadoConvertido.opaque, legadoConvertido.max,
             ic ? SDL_GetPixelFormatName(ic->format->format) : "NULL",
             cv ? SDL_GetPixelFormatName(cv->format->format) : "NULL",
             old ? SDL_GetPixelFormatName(old->format->format) : "NULL",
             oldCv ? SDL_GetPixelFormatName(oldCv->format->format) : "NULL", (int)bm, am,
             chaveAusente ? "off" : "on");
      SDL_FreeSurface(oldCv);
      SDL_FreeSurface(old);
      SDL_FreeSurface(cv);
      SDL_FreeSurface(ic);
      free(dados);
    }
  }
  // WEBP COM O FIO PRINCIPAL OCUPADO (23/09/2026): blocos de 1 s com 20 ms
  // livres entre eles, o papel das tarefas longas da Samsung 1.4.1. Os bytes
  // sao lidos ANTES (fopen/fread de pthread tambem sao proxiados ao fio
  // principal) e nada e impresso durante a medida (printf de pthread idem).
  // Pelo canal direto o decode nao deve sentir; pela ponte antiga cada
  // pedido espera o bloco em curso.
  // GIF PELO NAVEGADOR (24/09/2026): o SDL_image do Tizen nao le GIF, e a
  // foto de perfil .gif fora de foco ficava so na inicial. A ponte devolve o
  // PRIMEIRO quadro (vermelho); o segundo (azul) nao pode aparecer.
  { int ow = 0, oh = 0;
    s = jpeg_rapido_carregar("/amostra.gif", 0, &ow, &oh);
    if (!s) printf("FALHOU: gif devolveu NULL\n");
    else {
      Uint32 p = 0; Uint8 r, g, b, al;
      memcpy(&p, s->pixels, 4);
      SDL_GetRGBA(p, s->format, &r, &g, &b, &al);
      printf("%s gif primeiro quadro %dx%d (arquivo %dx%d, pixel=%d,%d,%d,%d)\n",
             (s->w == 8 && s->h == 4 && ow == 8 && oh == 4 && r > 200 && b < 50 && al == 255) ? "ok " : "FALHOU:",
             s->w, s->h, ow, oh, r, g, b, al);
      SDL_FreeSurface(s);
    } }
  { unsigned char *dados; long n; FILE *f = fopen("/amostra.webp", "rb");
    double soma = 0, maxMs = 0; int i, ok = 0;
    if (!f) { printf("FALHOU: amostra.webp\n"); return NULL; }
    fseek(f, 0, SEEK_END); n = ftell(f); rewind(f);
    dados = malloc((size_t)n);
    if (!dados || fread(dados, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(dados); return NULL; }
    fclose(f);
    MAIN_THREAD_ASYNC_EM_ASM({
      window.__nvOcupado = setInterval(function () { var t = performance.now(); while (performance.now() - t < 1000) {} }, 20);
    });
    for (i = 0; i < 10; i++) {
      double t0 = emscripten_get_now(), ms;
      int ow = 0, oh = 0;
      SDL_Surface *r = webp_carregar_larg_mem(dados, (size_t)n, 320, &ow, &oh);
      ms = emscripten_get_now() - t0;
      if (r && r->w == 320) ok++;
      SDL_FreeSurface(r);
      soma += ms; if (ms > maxMs) maxMs = ms;
    }
    MAIN_THREAD_ASYNC_EM_ASM({ clearInterval(window.__nvOcupado); });
    free(dados);
    printf("%s webp com fio principal ocupado: %d/10 ok, media=%.0f ms, max=%.0f ms\n",
           ok == 10 ? "ok " : "FALHOU:", ok, soma / 10, maxMs);
    printf("fim do teste\n");
  }
  return NULL;
}

int main(void) {
  pthread_t t;
#ifndef NV_SEM_INICIAR
  navegador_iniciar();   // como em main.c: o decodificador nasce no arranque
#endif
  // O fio principal PRECISA voltar ao laco de eventos: e nele que a chamada
  // proxiada roda e que a promessa do createImageBitmap resolve. main() retorna
  // e o runtime segue vivo (EXIT_RUNTIME=0), que e o mesmo desenho do app.
  pthread_create(&t, NULL, fioDeDecode, NULL);
  pthread_detach(t);
  return 0;
}
