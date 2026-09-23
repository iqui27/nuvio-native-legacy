/* DISCO LENTO NAO ATRASA A ARTE (22/09/2026).
 *
 * Reproduz no Mac o que o log da C9 mostrou: pasta de cache com ~3000
 * arquivos, ~500 pinos da home e um eMMC que leva centenas de ms por
 * gravacao. Antes da correcao o download so entregava os bytes ao decode
 * depois de gravar e de varrer a pasta duas vezes sob discoMtx, e o acerto
 * de disco de outro fio esperava a mesma trava. MEDIDO com este cenario no
 * codigo antigo (gravacao simulada de 400 ms): entrega em 863 ms e acerto
 * concorrente em 809 ms; com disco instantaneo, 447 ms e 392 ms — so as
 * varreduras.
 *
 * Aqui se prova a ORDEM: os bytes chegam ao decode (e viram superficie)
 * antes de o arquivo existir, e a gravacao acontece depois, em fundo. */
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
static int atrasoMs, downloads;
static size_t escritaLenta(const void *p, size_t a, size_t b, FILE *f) {
  if (atrasoMs) usleep((useconds_t)atrasoMs * 1000);
  return fwrite(p, a, b, f);
}
#define fwrite escritaLenta
#define rede_baixar_bin redeTeste
#include "../src/tex_cache.c"
#undef fwrite
#undef rede_baixar_bin

static unsigned char *jpg; static long njpg;
char *redeTeste(const char *url, int timeout, long *n) {
  char *b = malloc((size_t)njpg);
  (void)url; (void)timeout;
  memcpy(b, jpg, (size_t)njpg); *n = njpg; downloads++;
  return b;
}
static double agora(void) {
  struct timeval tv; gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}
static double hitMs;
static void *leitor(void *a) {
  char dst[600]; double t;
  (void)a;
  usleep(50000);   /* entra no meio da gravacao lenta */
  t = agora();
  assert(garantirLocal("https://teste/antiga-0001.jpg", dst, sizeof dst, NULL, NULL));
  hitMs = agora() - t;
  return NULL;
}
static int existe(const char *p) { return access(p, F_OK) == 0; }
static int protegido(const char *p, void *ctx) { return discoProtegido(p, ctx); }

int main(void) {
  char dir[] = "/tmp/nuvio-texlento-XXXXXX", dst[600], arq[600], cmd[700];
  FILE *f; int i; pthread_t th; double t, entrega, superficie;
  SDL_Surface *s;
  f = fopen("tests/amostra.jpg", "rb"); assert(f);
  fseek(f, 0, SEEK_END); njpg = ftell(f); rewind(f);
  jpg = malloc((size_t)njpg); assert(fread(jpg, 1, (size_t)njpg, f) == (size_t)njpg); fclose(f);
  assert(mkdtemp(dir));
  snprintf(dirCache, sizeof dirCache, "%s", dir);
  for (i = 0; i < 3000; i++) {
    char u[128], nome[600]; struct utimbuf ut = {1000 + i, 1000 + i};
    snprintf(u, sizeof u, "https://teste/antiga-%04d.jpg", i);
    nomeDeCache(u, nome, sizeof nome);
    f = fopen(nome, "wb"); assert(f); fwrite(jpg, 1, (size_t)njpg, f); fclose(f);
    utime(nome, &ut);
  }
  for (i = 0; i < 500; i++) {
    char u[128]; snprintf(u, sizeof u, "https://teste/pino-%04d.jpg", i);
    cachearte_marcar_grupo(NV_CACHE_ARTE_GRUPO_HOME, u, NV_CACHE_ARTE_MEDIUM, 1, 0);
  }
  /* As 5 mais velhas sao essenciais: a poda tem de pula-las. */
  for (i = 0; i < 5; i++) {
    char u[128]; snprintf(u, sizeof u, "https://teste/antiga-%04d.jpg", i);
    cachearte_marcar_grupo(NV_CACHE_ARTE_GRUPO_PERFIL, u, NV_CACHE_ARTE_MEDIUM, 1, 0);
  }
  mtx = SDL_CreateMutex(); nMax = 4;

  t = agora(); tex_cache_dir(dir);
  printf("tex_cache_dir: %.1f ms (antes: ~150 ms, varredura sincrona)\n", agora() - t);
  for (i = 0; i < 500 && !cachearte_nativo_indice_pronto(); i++) usleep(10000);
  assert(cachearte_nativo_indice_pronto());
  tex_cache_esperar_gravacoes();
  printf("ok  indice em fundo: %.1f MB, teto efetivo %.0f MB\n",
         cachearte_nativo_indice_bytes() / 1048576.0, tetoDisco() / 1048576.0);
  assert(tetoDisco() <= NV_CACHE_DISCO_MAX && tetoDisco() >= NV_CACHE_DISCO_PISO);

  /* 1. Download com disco lento: entrega imediata, arquivo so depois. */
  atrasoMs = 400;
  pthread_create(&th, NULL, leitor, NULL);
  { TexFetchTrace tr = {0, 0, 0, 0, 0}; int foi = 0;
    t = agora();
    assert(baixarParaItem(0, "https://teste/nova.jpg", dst, sizeof dst, &foi, &tr));
    entrega = agora() - t;
    assert(foi == 1 && itens[0].bruto && itens[0].nBruto == njpg);
    nomeDeCache("https://teste/nova.jpg", arq, sizeof arq);
    assert(!existe(arq));   /* A ORDEM: bytes no decode antes do arquivo */
    s = jpeg_rapido_carregar_mem(itens[0].bruto, (size_t)itens[0].nBruto, 640, NULL, NULL);
    superficie = agora() - t;
    assert(s); SDL_FreeSurface(s);
    printf("ok  entrega ao decode %.1f ms, superficie pronta %.1f ms, gravacao simulada %d ms"
           " (persist_ms=%u; antes: 863 ms)\n", entrega, superficie, atrasoMs, tr.persistMs);
    assert(superficie < atrasoMs / 4.0); }
  pthread_join(th, NULL);
  printf("ok  acerto de disco de outro fio durante a gravacao: %.1f ms (antes: 809 ms)\n", hitMs);
  assert(hitMs < atrasoMs / 4.0);

  /* 2. Mesmo pedido com a gravacao ainda na fila: sem rede. */
  { int foi = 1, antes = downloads;
    assert(baixarParaItem(1, "https://teste/nova.jpg", dst, sizeof dst, &foi, NULL));
    if (!existe(arq)) assert(foi == 0 && downloads == antes && itens[1].bruto);
    puts("ok  bytes ainda na fila servem o mesmo pedido sem voltar a rede"); }
  soltarBruto(&itens[0]); soltarBruto(&itens[1]);

  /* 3. A gravacao acontece, em fundo, e entra no indice. */
  tex_cache_esperar_gravacoes();
  { struct stat st; assert(!stat(arq, &st) && st.st_size == njpg); }
  snprintf(cmd, sizeof cmd, "%s.fila", arq); assert(!existe(cmd));
  puts("ok  arquivo gravado depois, sem temporario sobrando");

  /* 4. Fila cheia descarta a gravacao, nunca atrasa a entrega. */
  atrasoMs = 100;
  { double pior = 0; long descAntes = gravDescartes;
    for (i = 0; i < 60; i++) {
      char u[128]; int foi = 0;
      snprintf(u, sizeof u, "https://teste/rajada-%02d.jpg", i);
      t = agora();
      assert(baixarParaItem(2, u, dst, sizeof dst, &foi, NULL) && foi == 1);
      if (agora() - t > pior) pior = agora() - t;
      soltarBruto(&itens[2]);
    }
    printf("ok  rajada de 60 com disco a %d ms/arquivo: pior entrega %.1f ms, %ld gravacoes descartadas\n",
           atrasoMs, pior, gravDescartes - descAntes);
    assert(gravDescartes > descAntes && pior < atrasoMs / 2.0); }
  atrasoMs = 0;
  tex_cache_esperar_gravacoes();

  /* 5. Poda pelo indice: mais velhas primeiro, essenciais ficam. */
  { long apagados = cachearte_nativo_podar(0, 8L * 1024 * 1024, 0, 0, protegido, NULL);
    char velha[600];
    assert(apagados > 0 && cachearte_nativo_indice_bytes() <= 6L * 1024 * 1024);
    nomeDeCache("https://teste/antiga-0000.jpg", velha, sizeof velha); assert(existe(velha));
    nomeDeCache("https://teste/antiga-0005.jpg", velha, sizeof velha); assert(!existe(velha));
    printf("ok  poda pelo indice: %ld apagados, essenciais preservados\n", apagados); }

  snprintf(cmd, sizeof cmd, "rm -rf '%s'", dir); assert(system(cmd) == 0);
  puts("texlento: tudo ok");
  return 0;
}
