// A fila do cache de textura: o URGENTE fura, o resto mantem a ordem.
//
// POR QUE ESTE TESTE EXISTE. O #55 do @rawldon mediu `[hero] arte atrasada
// chegou em 12249 ms (prazo e 400)` com `pend=17` no painel de log: a arte que
// ocupa a tela inteira esperava atras de dezessete miniaturas porque a fila era
// FIFO pura. A ordem de uma fila e o tipo de coisa que quebra em silencio — nao
// ha tela que mostre "o hero furou" —, entao ela e exercitada aqui.
#include "../src/sdlcompat.h"
#include <SDL2/SDL_image.h>
#include <unistd.h>

static SDL_mutex *raceMtx;
static SDL_cond *raceCond;
static int racePopped;
static int raceRelease;

static void nv_tex_test_after_pop(void) {
  SDL_LockMutex(raceMtx);
  racePopped = 1;
  SDL_CondSignal(raceCond);
  while (!raceRelease) SDL_CondWait(raceCond, raceMtx);
  SDL_UnlockMutex(raceMtx);
}

#define NV_TEX_TEST_AFTER_POP nv_tex_test_after_pop
#include "../src/tex_cache.c"
#include <assert.h>
#include <stdio.h>

static void encher(int *f, int *ini, int *fim, const int *idx, int n) {
  int k;
  *ini = *fim = 0;
  for (k = 0; k < MAX_ITENS_ABS; k++) itens[k].urgente = 0;
  for (k = 0; k < n; k++) { f[*fim] = idx[k]; *fim = (*fim + 1) % MAX_FILA; }
}

static void prepararApi(void) {
  memset(itens, 0, sizeof itens);
  nMax = 8;
  filaIni = filaFim = 0;
  decIni = decFim = 0;
  mtx = SDL_CreateMutex();
  cond = SDL_CreateCond();
  condDec = SDL_CreateCond();
  condLivre = SDL_CreateCond();
  rodando = 1;
  assert(mtx && cond && condDec && condLivre);
}

static int itemPorNome(const char *nome) {
  int i;
  for (i = 0; i < nMax; i++)
    if (!strcmp(itens[i].caminho, nome)) return i;
  return -1;
}

static void limparApi(void) {
  rodando = 0;
  SDL_DestroyCond(condLivre);
  SDL_DestroyCond(condDec);
  SDL_DestroyCond(cond);
  SDL_DestroyMutex(mtx);
  condLivre = condDec = cond = NULL;
  mtx = NULL;
}

int main(void) {
  int f[MAX_FILA], ini, fim, k;
  int ordem[6] = { 10, 11, 12, 13, 14, 15 };

  // 1. SEM URGENTE E FIFO, que e o comportamento de sempre.
  encher(f, &ini, &fim, ordem, 6);
  for (k = 0; k < 6; k++) assert(tirarFila(f, &ini, fim) == ordem[k]);
  assert(ini == fim);
  puts("ok  sem urgente a fila continua FIFO");

  // 2. O URGENTE DO MEIO SAI PRIMEIRO, e os outros mantem a ordem de chegada.
  //    E a ordem que faz a fileira aparecer da esquerda para a direita.
  encher(f, &ini, &fim, ordem, 6);
  itens[13].urgente = 1;
  assert(tirarFila(f, &ini, fim) == 13);
  { int esperado[5] = { 10, 11, 12, 14, 15 };
    for (k = 0; k < 5; k++) assert(tirarFila(f, &ini, fim) == esperado[k]); }
  puts("ok  urgente do meio fura e o resto mantem a ordem");

  // 3. URGENTE NA CABECA nao embaralha nada.
  encher(f, &ini, &fim, ordem, 6);
  itens[10].urgente = 1;
  for (k = 0; k < 6; k++) assert(tirarFila(f, &ini, fim) == ordem[k]);
  puts("ok  urgente na cabeca sai como sairia");

  // 4. O ULTIMO da fila tambem fura — e o caso do #55: o hero e pedido DEPOIS
  //    dos posteres da fileira que ja estavam na fila.
  encher(f, &ini, &fim, ordem, 6);
  itens[15].urgente = 1;
  assert(tirarFila(f, &ini, fim) == 15);
  { int esperado[5] = { 10, 11, 12, 13, 14 };
    for (k = 0; k < 5; k++) assert(tirarFila(f, &ini, fim) == esperado[k]); }
  puts("ok  urgente no fim fura a fila inteira");

  // 5. DOIS URGENTES saem antes dos comuns e entre si respeitam a chegada.
  encher(f, &ini, &fim, ordem, 6);
  itens[12].urgente = 1; itens[14].urgente = 1;
  assert(tirarFila(f, &ini, fim) == 12);
  assert(tirarFila(f, &ini, fim) == 14);
  { int esperado[4] = { 10, 11, 13, 15 };
    for (k = 0; k < 4; k++) assert(tirarFila(f, &ini, fim) == esperado[k]); }
  puts("ok  dois urgentes saem na ordem de chegada entre si");

  // 6. A FILA QUE DEU A VOLTA no vetor circular. O bug classico deste tipo de
  //    codigo mora aqui: com ini > fim o "puxar para a frente" atravessa o zero.
  { int idx[4] = { 20, 21, 22, 23 };
    ini = fim = MAX_FILA - 2;
    for (k = 0; k < MAX_ITENS_ABS; k++) itens[k].urgente = 0;
    for (k = 0; k < 4; k++) { f[fim] = idx[k]; fim = (fim + 1) % MAX_FILA; }
    assert(ini > fim);                 // deu a volta mesmo
    itens[23].urgente = 1;             // o ultimo, ja depois do zero
    assert(tirarFila(f, &ini, fim) == 23);
    { int esperado[3] = { 20, 21, 22 };
      for (k = 0; k < 3; k++) assert(tirarFila(f, &ini, fim) == esperado[k]); }
    assert(ini == fim); }
  puts("ok  fila circular que deu a volta tambem reordena certo");

  // 7. FUNDO AUTOMATICO NAO E HERO URGENTE. Este usa as funcoes reais do
  // tex_cache, em vez de copiar a politica numa fixture: o caminho de
  // prefetch (`tex_obter`) entra na fila de rede; o caminho local vai direto
  // para a fila de decode, sem ocupar worker de rede nem bloquear a UI.
  prepararApi();
  assert(tex_obter("/app/art/icones/menu.png") == 0);
  assert(tex_obter("https://catalogo/fundo-auto.jpg") == 0);
  { int icon = itemPorNome("/app/art/icones/menu.png");
    int autoBg = itemPorNome("https://catalogo/fundo-auto.jpg");
    assert(icon >= 0 && autoBg >= 0);
    assert(!itens[icon].urgente && !itens[autoBg].urgente);
    assert(itens[icon].localDireto && itens[icon].naFilaDec);
    assert(!itens[autoBg].localDireto);
    assert(tirarFila(filaDec, &decIni, decFim) == icon);
    itens[icon].naFilaDec = 0;
    assert(tirarFila(fila, &filaIni, filaFim) == autoBg); }
  limparApi();
  puts("ok  prefetch de fundo automatico cede aos icones locais");

  // 8. PROMOCAO SO DEPOIS DO PREFETCH PRONTO. O pedido hero que sobe a
  // qualidade final de uma reserva ja presente continua normal: mesmo que um
  // icone entre depois, a reserva nao ganha prioridade artificial.
  prepararApi();
  assert(tex_obter("https://catalogo/fundo-auto.jpg") == 0);
  { int autoBg = itemPorNome("https://catalogo/fundo-auto.jpg");
    assert(autoBg >= 0);
    itens[autoBg].estado = PRONTO;
    itens[autoBg].tex = 42;
    itens[autoBg].w = 640;
    itens[autoBg].h = 360;
    itens[autoBg].tetoUsado = 640;
    itens[autoBg].fonteW = 1920;
    assert(tex_obter_hero("https://catalogo/fundo-auto.jpg") == 0);
    assert(!itens[autoBg].urgente);
    assert(itens[autoBg].estado == PENDENTE);
    assert(tirarFila(fila, &filaIni, filaFim) == autoBg); }
  limparApi();
  puts("ok  promocao do fundo automatico para hero continua normal");

  // 9. ARTE CUSTOMIZADA PRESERVA A PRIORIDADE HERO. O conserto nao muda a
  // imagem escolhida pela conta: quando ela pede hero diretamente, ele segue
  // furando a fila dos posteres.
  prepararApi();
  assert(tex_obter("/app/art/icones/menu.png") == 0);
  assert(tex_obter_hero("https://conta/fundo-custom.jpg") == 0);
  { int custom = itemPorNome("https://conta/fundo-custom.jpg");
    int icon = itemPorNome("/app/art/icones/menu.png");
    assert(custom >= 0 && icon >= 0 && itens[custom].urgente);
    assert(tirarFila(fila, &filaIni, filaFim) == custom);
    assert(itens[icon].localDireto && itens[icon].naFilaDec);
    assert(tirarFila(filaDec, &decIni, decFim) == icon); }
  limparApi();
  puts("ok  fundo customizado continua urgente");

  // 10. LOCAL CHEIO NAO BLOQUEIA. O item fica PENDENTE sem duplicar a fila;
  // quando um slot abre, o proximo pedido o coloca exatamente uma vez.
  prepararApi();
  decIni = 0; decFim = MAX_FILA - 1;
  assert(tex_obter("/app/art/icones/menu.png") == 0);
  { int icon = itemPorNome("/app/art/icones/menu.png");
    assert(icon >= 0 && itens[icon].localDireto && !itens[icon].naFilaDec);
    decIni = decFim = 0;
    assert(tex_obter("/app/art/icones/menu.png") == 0);
    assert(itens[icon].naFilaDec);
    assert(tirarFila(filaDec, &decIni, decFim) == icon); }
  limparApi();
  puts("ok  local nao espera fila cheia e reenfileira sem duplicar");

  // 11. O DECODER EM ANDAMENTO TAMBEM BLOQUEIA DUPLICACAO. A barreira fica
  // entre a retirada da fila e a leitura do BMP: enquanto o fio esta parado
  // com PENDENTE, varios pedidos da UI devem observar o mesmo trabalho ativo,
  // e nao criar uma segunda entrada de decode.
  {
    const char *path = "/tmp/nuvio-texfila-race.bmp";
    SDL_Surface *bmp = SDL_CreateRGBSurface(0, 8, 8, 32,
                                             0x00ff0000, 0x0000ff00,
                                             0x000000ff, 0xff000000);
    SDL_Thread *worker;
    int idx, q;
    assert(bmp);
    SDL_FillRect(bmp, NULL, SDL_MapRGBA(bmp->format, 31, 97, 211, 255));
    assert(SDL_SaveBMP(bmp, path) == 0);
    SDL_FreeSurface(bmp);
    prepararApi();
    raceMtx = SDL_CreateMutex(); raceCond = SDL_CreateCond();
    assert(raceMtx && raceCond);
    racePopped = 0; raceRelease = 0;
    dirCache[0] = 0;
    assert(tex_obter(path) == 0);
    idx = itemPorNome(path);
    assert(idx >= 0 && itens[idx].naFilaDec);
    worker = SDL_CreateThread(threadDecode, "tex-race", NULL);
    assert(worker);
    SDL_LockMutex(raceMtx);
    while (!racePopped) SDL_CondWait(raceCond, raceMtx);
    SDL_UnlockMutex(raceMtx);
    for (q = 0; q < 8; q++) assert(tex_obter(path) == 0);
    SDL_LockMutex(mtx);
    assert(decIni == decFim);
    assert(itens[idx].naFilaDec);
    SDL_UnlockMutex(mtx);
    SDL_LockMutex(raceMtx);
    raceRelease = 1; SDL_CondSignal(raceCond);
    SDL_UnlockMutex(raceMtx);
    SDL_Delay(50);
    SDL_LockMutex(mtx); rodando = 0; SDL_CondSignal(condDec); SDL_UnlockMutex(mtx);
    SDL_WaitThread(worker, NULL);
    SDL_LockMutex(mtx);
    assert(itens[idx].estado == DECODIFICADO);
    assert(!itens[idx].naFilaDec);
    assert(itens[idx].sup != NULL);
    SDL_FreeSurface(itens[idx].sup); itens[idx].sup = NULL;
    SDL_UnlockMutex(mtx);
    SDL_DestroyCond(raceCond); SDL_DestroyMutex(raceMtx);
    raceCond = NULL; raceMtx = NULL;
    limparApi();
    unlink(path);
  }
  puts("ok  decoder ativo mantem uma unica entrada sob pedidos repetidos");

  puts("texfila: tudo ok");
  return 0;
}
