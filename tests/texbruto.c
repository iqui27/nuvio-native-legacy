// O card com a arte de OUTRO titulo (24/09/2026, C9 do dono): "Resident Evil"
// mostrava o logo de "Searching" e "Hokum" o de "Missing", na fileira e no
// painel de Salvos ao mesmo tempo, com o catalogo e os arquivos do cache de
// disco CERTOS.
//
// O caminho que este teste refaz, com as funcoes reais do tex_cache:
//   1. o fio de rede baixa o logo e deixa os bytes em Item.bruto;
//   2. o painel rolou, o pedido ficou velho, e o fio de decode desiste dele
//      (desistir: VAZIO, caminho zerado) — sem soltar os bytes;
//   3. o slot VAZIO e o primeiro que slotLivre devolve, e o cartaz de outro
//      titulo entra nele;
//   4. o cartaz ja esta no disco: baixarParaItem acha o arquivo e devolve sem
//      tocar em `bruto`;
//   5. o decode prefere `bruto` ao arquivo — e decodifica o LOGO no cartaz. A
//      guarda "o slot ainda e deste pedido?" passa, porque o caminho do item e
//      mesmo o do cartaz. A textura errada fica no cache com o nome certo.
#include "../src/sdlcompat.h"
#include <SDL2/SDL_image.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../src/tex_cache.c"
#include <assert.h>
#include <stdio.h>

#define LOGO   "https://image.tmdb.org/t/p/w780/4AmlH0mEn8gL6HEmSX0QYbdItmA.png"
#define POSTER "https://images.metahub.space/poster/medium/tt35672862/img"
#define DIR    "/tmp/nuvio-texbruto-cache"

// PNG com ruido, para passar dos 512 bytes que acertoDisco exige. `r`/`b`
// dizem de que cor ele e: o logo vermelho, o cartaz azul.
static SDL_Surface *imagem(int r, int b) {
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 64, 64, 32, SDL_PIXELFORMAT_ABGR8888);
  unsigned x = 12345;
  int i;
  assert(s);
  for (i = 0; i < 64 * 64; i++) {
    unsigned char *p = (unsigned char *)s->pixels + i * 4;
    x = x * 1103515245u + 12345u;
    p[0] = r ? 200 + (x >> 16) % 50 : (x >> 16) % 20;
    p[1] = (x >> 20) % 20;
    p[2] = b ? 200 + (x >> 24) % 50 : (x >> 24) % 20;
    p[3] = 255;
  }
  return s;
}

static unsigned char *bytesPng(SDL_Surface *s, long *n) {
  const char *tmp = DIR "/tmp.png";
  unsigned char *buf;
  FILE *f;
  assert(IMG_SavePNG(s, tmp) == 0);
  f = fopen(tmp, "rb"); assert(f);
  fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET);
  buf = malloc((size_t)*n); assert(buf);
  assert(fread(buf, 1, (size_t)*n, f) == (size_t)*n);
  fclose(f); unlink(tmp);
  return buf;
}

static int achar(const char *c) {
  int i;
  for (i = 0; i < nMax; i++) if (itens[i].estado != VAZIO && !strcmp(itens[i].caminho, c)) return i;
  return -1;
}

static void esperar(int idx, Estado e) {
  int k;
  for (k = 0; k < 400; k++) {
    int ok;
    SDL_LockMutex(mtx); ok = itens[idx].estado == e && !itens[idx].naFilaDec; SDL_UnlockMutex(mtx);
    if (ok) return;
    SDL_Delay(5);
  }
  fprintf(stderr, "slot %d nao chegou ao estado %d (esta em %d)\n", idx, e, itens[idx].estado);
  assert(0);
}

int main(void) {
  SDL_Thread *dec;
  unsigned char *logo;
  long nLogo;
  int a, b;

  mkdir(DIR, 0755);
  memset(itens, 0, sizeof itens);
  nMax = 8;
  mtx = SDL_CreateMutex(); cond = SDL_CreateCond();
  condDec = SDL_CreateCond(); condLivre = SDL_CreateCond();
  rodando = 1;
  snprintf(dirCache, sizeof dirCache, "%s", DIR);

  { SDL_Surface *azul = imagem(0, 1), *verm = imagem(1, 0);
    char arq[600];
    nomeDeCache(POSTER, arq, sizeof arq);
    assert(IMG_SavePNG(azul, arq) == 0);   // o cartaz certo, ja no disco
    logo = bytesPng(verm, &nLogo);          // o logo que a rede entregou
    SDL_FreeSurface(azul); SDL_FreeSurface(verm); }

  dec = SDL_CreateThread(threadDecode, "tex-dec", NULL);
  assert(dec);

  // 1. O logo e pedido e o fio de rede entrega os bytes no item.
  quadroAtual = 10;
  assert(tex_obter(LOGO) == 0);
  a = achar(LOGO); assert(a >= 0);
  SDL_LockMutex(mtx);
  assert(tirarFila(fila, &filaIni, filaFim) == a);
  itens[a].bruto = logo; itens[a].nBruto = nLogo;
  // 2. ...e o card saiu da tela: 8 quadros e 200 ms sem ninguem desenha-lo.
  quadroAtual += NV_TEX_STALE_FRAMES + 2;
  itens[a].ultimoPedido = SDL_GetTicks();
  SDL_UnlockMutex(mtx);
  SDL_Delay(NV_TEX_STALE_MS + 20);
  SDL_LockMutex(mtx); paraDecode(a); SDL_UnlockMutex(mtx);
  esperar(a, VAZIO);
  // Quem desiste solta os bytes. Conferido so no fim, depois do sintoma: o
  // que importa mostrar e o cartaz errado, nao o campo.
  { int sobrou = itens[a].bruto != NULL;
    printf("     bytes do logo no slot vago: %s\n", sobrou ? "SIM" : "nao");

  // 3. O cartaz de outro titulo entra no slot que ficou vago.
  quadroAtual++;
  assert(tex_obter(POSTER) == 0);
  b = achar(POSTER); assert(b == a);
  SDL_LockMutex(mtx);
  assert(tirarFila(fila, &filaIni, filaFim) == b);
  SDL_UnlockMutex(mtx);
  // 4. Acerto de disco: o fio de rede nao baixa nada.
  { char local[600]; int foiRede = 1;
    assert(baixarParaItem(b, POSTER, local, sizeof local, &foiRede, NULL) == 1);
    assert(!foiRede); }
  SDL_LockMutex(mtx); paraDecode(b); SDL_UnlockMutex(mtx);
  esperar(b, DECODIFICADO);

  // 5. O que saiu do decode tem de ser o cartaz (azul), nao o logo (vermelho).
  { SDL_Surface *s = itens[b].sup;
    unsigned char *p;
    assert(s && s->format->format == SDL_PIXELFORMAT_ABGR8888);
    p = (unsigned char *)s->pixels + (s->h / 2) * s->pitch + (s->w / 2) * 4;
    printf("     pixel do cartaz: r=%d b=%d\n", p[0], p[2]);
    assert(p[2] > 150 && p[0] < 60);
    SDL_FreeSurface(s); itens[b].sup = NULL; }
  puts("ok  cartaz que entra no slot vago mostra o cartaz, nao a arte do anterior");
  assert(!sobrou);
  puts("ok  pedido abandonado no decode nao deixa bytes no slot"); }

  SDL_LockMutex(mtx); rodando = 0; SDL_CondBroadcast(condDec); SDL_UnlockMutex(mtx);
  SDL_WaitThread(dec, NULL);
  { char arq[600]; nomeDeCache(POSTER, arq, sizeof arq); unlink(arq); }
  rmdir(DIR);
  puts("texbruto: tudo ok");
  return 0;
}
