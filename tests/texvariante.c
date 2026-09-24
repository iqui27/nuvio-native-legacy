/* A VARIANTE DO TAMANHO NO CACHE DE TEXTURA (23/09/2026, artetamanho.h).
 *
 * O que tem de continuar valendo com a URL de download diferente da chave:
 *   1. o card baixa a variante (w780), grava no disco sob o nome DELA e o
 *      item continua com a URL original como chave;
 *   2. o decode grava tetoUsado = teto da variante e fonteW desconhecido, e
 *      a promocao a heroi (1920) acontece e baixa a original;
 *   3. um w1280 decodificado a 1280 nao bloqueia a promocao a 1920 — sem a
 *      correcao, fonteW=1280 = w e a condicao `fonteW > w` travava o heroi;
 *   4. o card de novo, com a variante ja no disco: sem rede, e o decode le o
 *      arquivo da variante (e nao tenta baixar a original).
 *
 * A rede e substituida: cada tamanho do TMDB devolve um JPEG da largura
 * real daquele tamanho (argv[1..3]: 780, 1280 e 1600 de largura). */
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#define rede_baixar_bin redeTeste
#include "../src/tex_cache.c"
#undef rede_baixar_bin

static const char *arqW780, *arqW1280, *arqOrig;
static int downloads;
static char ultimaUrl[600];

static char *lerArq(const char *p, long *n) {
  FILE *f = fopen(p, "rb"); char *b;
  assert(f);
  fseek(f, 0, SEEK_END); *n = ftell(f); rewind(f);
  b = malloc((size_t)*n);
  assert(fread(b, 1, (size_t)*n, f) == (size_t)*n); fclose(f);
  return b;
}

char *redeTeste(const char *url, int timeout, long *n) {
  (void)timeout;
  downloads++;
  snprintf(ultimaUrl, sizeof ultimaUrl, "%s", url);
  if (strstr(url, "/t/p/w780/")) return lerArq(arqW780, n);
  if (strstr(url, "/t/p/w1280/")) return lerArq(arqW1280, n);
  return lerArq(arqOrig, n);
}

static const char *ORIG = "https://image.tmdb.org/t/p/original/fIhXD6m9LJgC7yDMOHMpYgkYAJ.jpg";
static const char *W780 = "https://image.tmdb.org/t/p/w780/fIhXD6m9LJgC7yDMOHMpYgkYAJ.jpg";

static int existe(const char *p) { return access(p, F_OK) == 0; }

/* Um slot com o pedido, como tex_obter_limite o deixa. */
static void pedido(int idx, const char *url, int limite) {
  memset(&itens[idx], 0, sizeof itens[idx]);
  itens[idx].estado = PENDENTE;
  itens[idx].limite = limite;
  itens[idx].hash = hashCaminho(url);
  itens[idx].lum = -1;
  snprintf(itens[idx].caminho, sizeof itens[idx].caminho, "%s", url);
}

/* Roda o fio de decode ate o item sair de PENDENTE. */
static void decodificar(int idx) {
  SDL_Thread *th;
  int k;
  SDL_LockMutex(mtx); rodando = 1; paraDecode(idx); SDL_UnlockMutex(mtx);
  th = SDL_CreateThread(threadDecode, "tex-variante", NULL);
  assert(th);
  for (k = 0; k < 400; k++) {
    int pronto;
    SDL_LockMutex(mtx); pronto = itens[idx].estado != PENDENTE; SDL_UnlockMutex(mtx);
    if (pronto) break;
    SDL_Delay(10);
  }
  SDL_LockMutex(mtx); rodando = 0; SDL_CondSignal(condDec); SDL_UnlockMutex(mtx);
  SDL_WaitThread(th, NULL);
  assert(itens[idx].estado == DECODIFICADO && itens[idx].sup);
  /* O que tex_bombear faria, sem GL: textura publicada na largura que saiu. */
  itens[idx].w = itens[idx].sup->w; itens[idx].h = itens[idx].sup->h;
  SDL_FreeSurface(itens[idx].sup); itens[idx].sup = NULL;
  itens[idx].tex = 1;
  itens[idx].estado = PRONTO;
}

/* A promocao pede de novo pela API: 1 se o item voltou para a fila de rede. */
static int promoveu(int idx, int limite) {
  int antes = filaFim;
  (void)tex_obter_limite(itens[idx].caminho, limite, 1, 0);
  return itens[idx].estado == PENDENTE && filaFim != antes && fila[antes] == idx;
}

int main(int argc, char **argv) {
  char dir[] = "/tmp/nuvio-texvariante-XXXXXX", dst[600], arq[600], cmd[700];
  assert(argc == 4);
  arqW780 = argv[1]; arqW1280 = argv[2]; arqOrig = argv[3];
  assert(mkdtemp(dir));
  memset(itens, 0, sizeof itens);
  nMax = 8;
  mtx = SDL_CreateMutex(); cond = SDL_CreateCond();
  condDec = SDL_CreateCond(); condLivre = SDL_CreateCond();
  assert(mtx && cond && condDec && condLivre);
  { char m[600]; FILE *fm; snprintf(m, sizeof m, "%s/.limpo-143", dir);
    fm = fopen(m, "w"); if (fm) { fputs("1\n", fm); fclose(fm); } }
  tex_cache_dir(dir);

  /* 1. Card a 704: baixa w780, chave continua a original. */
  { int foi = 0;
    pedido(0, ORIG, 704);
    assert(baixarParaItem(0, ORIG, dst, sizeof dst, &foi, NULL) == 1);
    assert(foi == 1 && downloads == 1 && !strcmp(ultimaUrl, W780));
    assert(itens[0].limiteTamanho == 704 && itens[0].bruto);
    assert(!strcmp(itens[0].caminho, ORIG));
    tex_cache_esperar_gravacoes();
    nomeDeCache(W780, arq, sizeof arq); assert(existe(arq));
    nomeDeCache(ORIG, arq, sizeof arq); assert(!existe(arq));
    puts("ok  card a 704 baixa o w780 e grava sob o nome dele; a chave e a original"); }

  /* 2. Decode e promocao a heroi. */
  decodificar(0);
  printf("    card: %dx%d tetoUsado=%d fonteW=%d\n", itens[0].w, itens[0].h,
         itens[0].tetoUsado, itens[0].fonteW);
  assert(itens[0].w == 704 && itens[0].tetoUsado == 704 && itens[0].fonteW == 0);
  assert(promoveu(0, 1920));
  { int foi = 0;
    assert(baixarParaItem(0, ORIG, dst, sizeof dst, &foi, NULL) == 1);
    assert(foi == 1 && downloads == 2 && !strcmp(ultimaUrl, ORIG));
    assert(itens[0].limiteTamanho == 0); }
  decodificar(0);
  printf("    heroi: %dx%d tetoUsado=%d fonteW=%d\n", itens[0].w, itens[0].h,
         itens[0].tetoUsado, itens[0].fonteW);
  assert(itens[0].w == 1600 && itens[0].fonteW == 1600);
  assert(!promoveu(0, 1920));
  puts("ok  promocao a heroi baixa a original e para quando a fonte acaba");

  /* 3. Heroi de 1280 (perfil Desempenho) em w1280, depois 1920. */
  { int foi = 0;
    filaIni = filaFim = 0;
    memset(&itens[0], 0, sizeof itens[0]);   /* mesma chave: a busca acharia o 0 */
    pedido(1, ORIG, 1280);
    assert(baixarParaItem(1, ORIG, dst, sizeof dst, &foi, NULL) == 1);
    assert(strstr(ultimaUrl, "/t/p/w1280/") && itens[1].limiteTamanho == 1280); }
  decodificar(1);
  printf("    w1280: %dx%d tetoUsado=%d fonteW=%d\n", itens[1].w, itens[1].h,
         itens[1].tetoUsado, itens[1].fonteW);
  assert(itens[1].w == 1280 && itens[1].fonteW == 0);
  assert(promoveu(1, 1920));
  puts("ok  w1280 decodificado a 1280 nao trava a promocao a 1920");

  /* 4. Card de novo, w780 no disco: sem rede no fio de rede nem no decode. */
  { int foi = 1, antes;
    filaIni = filaFim = 0;
    antes = downloads;
    memset(&itens[1], 0, sizeof itens[1]);
    pedido(2, ORIG, 704);
    assert(baixarParaItem(2, ORIG, dst, sizeof dst, &foi, NULL) == 1);
    assert(foi == 0 && downloads == antes && !itens[2].bruto);
    assert(itens[2].limiteTamanho == 704);
    decodificar(2);
    assert(downloads == antes && itens[2].w == 704);
    puts("ok  variante no disco: acerto sem rede e o decode le o arquivo dela"); }

  /* 5. Host desconhecido: a URL do item e a do download. */
  { int foi = 0;
    const char *png = "https://pub-x.r2.dev/Backdrops/Untitled%20design.png";
    pedido(3, png, 704);
    assert(baixarParaItem(3, png, dst, sizeof dst, &foi, NULL) == 1);
    assert(!strcmp(ultimaUrl, png) && itens[3].limiteTamanho == 0);
    soltarBruto(&itens[3]);
    puts("ok  host desconhecido baixa a URL como veio"); }

  tex_cache_esperar_gravacoes();
  snprintf(cmd, sizeof cmd, "rm -rf '%s'", dir); assert(system(cmd) == 0);
  puts("texvariante: tudo ok");
  return 0;
}
