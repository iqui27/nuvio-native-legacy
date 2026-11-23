#include <assert.h>
#include <stdio.h>
#include "nvtmp.h"
#define rede_baixar_bin redeTeste
#define rede_pedir pedirTeste
#define arte_fonte_resolver fonteTeste
#define arte_reserva_url reservaTeste
#include "../src/tex_cache.c"
#undef rede_baixar_bin
#undef rede_pedir
#undef arte_fonte_resolver
#undef arte_reserva_url

static char ultima[NV_TEX_URL_MAX];
static int pedidos, falha, vazio, invalido;
static const char *original = "https://image.tmdb.org/t/p/original/teste.jpg";
static const char *episodio = "https://episodes.metahub.space/tt0944947/1/1/w780.jpg";

static char *imagem(long *n) {
  char *b;
  *n = vazio ? 0 : 1024;
  b = calloc(1, (size_t)*n + 1); assert(b);
  if (*n && !invalido) { b[0] = (char)0xff; b[1] = (char)0xd8; }
  return b;
}

char *redeTeste(const char *url, int prazo, long *n) {
  (void)prazo;
  snprintf(ultima, sizeof ultima, "%s", url); pedidos++;
  if (strstr(url, "falhou")) { *n = 0; return NULL; }
  return imagem(n);
}

int pedirTeste(const RedePedido *p, RedeResposta *r) {
  long n;
  assert(p->seguir && p->prazo_ms == 8000 && p->max_bytes > 0);
  snprintf(ultima, sizeof ultima, "%s", p->url); pedidos++;
  memset(r, 0, sizeof *r);
  r->status = falha ? 503 : 200;
  r->corpo = imagem(&n); r->n_corpo = (size_t)n;
  snprintf(r->final, sizeof r->final, "https://image.tmdb.org/t/p/w780/still.jpg");
  return !falha;
}

int fonteTeste(const char *url, char *saida, size_t tam) {
  if (!strcmp(url, "https://virtual/ausente")) return -1;
  if (strcmp(url, "https://virtual/fundo")) return 0;
  snprintf(saida, tam, "%s", original); return 1;
}

int reservaTeste(const char *url, char *saida, size_t tam) {
  if (!strstr(url, "falhou")) return 0;
  snprintf(saida, tam, "%s", original); return 1;
}

static void baixar(const char *url, int limite, const char *esperado) {
  long n = 0;
  char *b = baixarImagemTamanho(url, &n, NULL, limite);
  assert(b && n == 1024 && !strcmp(ultima, esperado)); free(b);
}

int main(void) {
  char dst[600], dir[NVT_MAX], final[NV_TEX_URL_MAX];
  long n;
  baixar("https://virtual/fundo", 480, "https://image.tmdb.org/t/p/w500/teste.jpg");
  baixar("https://virtual/fundo", 185, "https://image.tmdb.org/t/p/w185/teste.jpg");
  baixar("https://outro/fundo.jpg", 480, "https://outro/fundo.jpg");
  assert(!baixarImagemTamanho("https://virtual/ausente", &n, NULL, 480));
  puts("ok  fonte resolvida usa o teto; host desconhecido e fonte ausente preservados");

  nvt_set(dir, sizeof dir, "/nuvio-texartetamanho-XXXXXX"); assert(mkdtemp(dir));
  snprintf(dirCache, sizeof dirCache, "%s", dir);
  assert(garantirLocalTamanho("https://teste/falhou.jpg", dst, sizeof dst, NULL, NULL, 480));
  assert(!strcmp(ultima, "https://image.tmdb.org/t/p/w500/teste.jpg"));
  char chave[600]; nomeDeCache("https://image.tmdb.org/t/p/w500/teste.jpg", chave, sizeof chave);
  assert(!strcmp(chave, dst));
  puts("ok  reserva usa o teto e grava a variante real no disco");

  mtx = SDL_CreateMutex(); condLivre = SDL_CreateCond(); assert(mtx && condLivre);
  nMax = 1; rodando = 1; orcamento = 128L * 1048576;
  char marca[600]; snprintf(marca, sizeof marca, "%s/.limpo-143", dir);
  FILE *fm = fopen(marca, "w"); assert(fm); fputs("1\n", fm); fclose(fm);
  iniciarGravador();
  for (int limite = 480; limite <= 1920; limite *= 4) {
    memset(&itens[0], 0, sizeof itens[0]);
    itens[0].estado = PENDENTE; itens[0].limite = limite;
    snprintf(itens[0].caminho, sizeof itens[0].caminho, "https://virtual/fundo");
    int antes = pedidos;
    assert(baixarParaItem(0, itens[0].caminho, dst, sizeof dst, NULL, NULL));
    assert(!strcmp(itens[0].caminho, "https://virtual/fundo"));
    assert(itens[0].bruto && itens[0].nBruto == 1024);
    if (limite == 480) {
      assert(pedidos == antes && itens[0].limiteTamanho == 480);
      assert(strstr(itens[0].urlCache, "/w500/"));
    } else {
      assert(pedidos == antes + 1 && !strcmp(ultima, original));
      assert(itens[0].limiteTamanho == 0);
    }
    free(itens[0].bruto); itens[0].bruto = NULL;
  }
  tex_cache_esperar_gravacoes(); rodando = 0;
  DIR *d = opendir(dir); assert(d);
  struct dirent *e;
  while ((e = readdir(d))) if (e->d_name[0] != '.') {
    char cam[NVT_MAX + 256];
    snprintf(cam, sizeof cam, "%s/%s", dir, e->d_name); unlink(cam);
  }
  closedir(d); unlink(marca); assert(!rmdir(dir)); dirCache[0] = 0;
  puts("ok  chave do item preservada; card usa disco e heroi nao reutiliza bytes pequenos");

  assert(!buscarRedirectArte(episodio, final, sizeof final));
  falha = 1; assert(!baixarImagemTamanho(episodio, &n, NULL, 480)); falha = 0;
  assert(!buscarRedirectArte(episodio, final, sizeof final));
  const char *epVazio = "https://episodes.metahub.space/tt0944947/1/2/w780.jpg";
  vazio = 1; assert(!baixarImagemTamanho(epVazio, &n, NULL, 480)); vazio = 0;
  assert(!buscarRedirectArte(epVazio, final, sizeof final));
  int antes = pedidos;
  assert(!baixarImagemTamanho(epVazio, &n, NULL, 480) && pedidos == antes);
  const char *epInvalido = "https://episodes.metahub.space/tt0944947/1/3/w780.jpg";
  invalido = 1; assert(!baixarImagemTamanho(epInvalido, &n, NULL, 480)); invalido = 0;
  assert(!buscarRedirectArte(epInvalido, final, sizeof final));
  baixar(episodio, 480, episodio);
  assert(buscarRedirectArte(episodio, final, sizeof final));
  assert(!buscarRedirectArte(episodio, final, 4));
  baixar(episodio, 480, "https://image.tmdb.org/t/p/w500/still.jpg");
  baixar(episodio, 185, "https://image.tmdb.org/t/p/w185/still.jpg");
  puts("ok  redirect valido evita o metahub e aplica a escada; erro/vazio/invalido nao entram");

  lembrarRedirectArte(episodio, "https://image.tmdb.org.evil/t/p/w780/x.jpg");
  assert(buscarRedirectArte(episodio, final, sizeof final) && strstr(final, "/still.jpg"));
  for (int i = 0; i < NV_ARTE_REDIRECT_MAX; i++) {
    char u[160]; snprintf(u, sizeof u, "https://episodes.metahub.space/tt1234567/1/%d/w780.jpg", i + 1);
    lembrarRedirectArte(u, original);
  }
  assert(!buscarRedirectArte(episodio, final, sizeof final));
  assert(pedidos > NV_ARTE_REDIRECT_MAX / 16);
  puts("ok  memoria de redirects limitada e restrita ao CDN do TMDB");
  return 0;
}
