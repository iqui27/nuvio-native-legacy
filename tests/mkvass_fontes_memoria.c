// Log 12 (owner 23/11/2026): a re-pick on the same file downloaded its
// attached fonts again and loaded a 7231-event document twice (before and
// after the fonts: 510 + 353 ms of frozen app).
//   1. The fonts of the last file stay in memory: the same file gets them
//      with no download; another file does not, and replaces them.
//   2. A big first document waits (bounded) for the fonts; a small one, or
//      one with the fonts already here, does not wait.
// Includes the module, like mkvass_ordem.c: no network or disk.
#include "../src/mkvass.c"

// No network here: the pool is never asked (the fonts job is a fake one).
char *rede_baixar_trecho_st(const char *url, int segundos, long long ini, long long fim, long *tam, int *st, int *erro,
                            char *fin, unsigned finN) {
  (void)url; (void)segundos; (void)ini; (void)fim; (void)fin; (void)finN;
  if (tam) *tam = 0; if (st) *st = 0; if (erro) *erro = 0; return NULL;
}
long rede_corte_host(const char *url) { (void)url; return 0; }
int rede_resto_recusado(void) { return 0; }

static int falhas;
static void conferir(int ok, const char *caso) {
  printf("%s: %s\n", ok ? "ok" : "FALHA", caso);
  if (!ok) falhas++;
}
static void umaFonte(Fio *f, const char *nome, long tam, unsigned char v) {
  f->fontes = realloc(f->fontes, (size_t)(f->nFontes + 1) * sizeof *f->fontes);
  f->fontes[f->nFontes].nome = strdup(nome);
  f->fontes[f->nFontes].dados = malloc((size_t)tam);
  memset(f->fontes[f->nFontes].dados, v, (size_t)tam);
  f->fontes[f->nFontes].tam = tam;
  f->nFontes++;
}

int main(void) {
  static Fio a, b, c, d;
  long t0;
  // 1. memory cache
  umaFonte(&a, "Alegreya-Fake.ttf", 4000, 7); umaFonte(&a, "Tsuku.otf", 9000, 9);
  a.fontesCompletas = 1;
  memFontesGuardar(&a, "https://cdn/fate-e02.mkv");
  conferir(memFontesPegar(&b, "https://cdn/fate-e02.mkv") && b.nFontes == 2 && b.fontes[1].tam == 9000 &&
           b.fontes[1].dados[8999] == 9 && !strcmp(b.fontes[0].nome, "Alegreya-Fake.ttf") && b.fontes != a.fontes,
           "same file: the fonts come from memory (own copy)");
  conferir(!memFontesPegar(&c, "https://cdn/other.mkv") && c.nFontes == 0, "another file: nothing from memory");
  liberarFontes(&c);
  umaFonte(&c, "Other.ttf", 100, 1); c.fontesCompletas = 1;
  memFontesGuardar(&c, "https://cdn/other.mkv");
  conferir(!memFontesPegar(&d, "https://cdn/fate-e02.mkv"), "a new file replaces the old one's fonts");
  liberarFontes(&a); liberarFontes(&b); liberarFontes(&c); liberarFontes(&d);

  // 2. the bounded wait before a big first delivery
  memset(&d, 0, sizeof d);
  d.nColhidos = 200; d.fontesPasso = 1;
  t0 = agoraMs(); esperarFontesGrande(&d);
  conferir(agoraMs() - t0 < 100, "small first document (200 blocks): no wait");
  d.nColhidos = 7231; d.fontesCompletas = 1;
  t0 = agoraMs(); esperarFontesGrande(&d);
  conferir(agoraMs() - t0 < 100, "big document with the fonts already here: no wait");
  { static Job j;   // a fonts request that never finishes (estado 1)
    d.fontesCompletas = 0; d.fontesPasso = 1; j.estado = 1; d.jobFontes = &j;
    t0 = agoraMs(); esperarFontesGrande(&d);
    conferir(agoraMs() - t0 >= MKVASS_ESPERA_FONTES_MS - 50 && agoraMs() - t0 < MKVASS_ESPERA_FONTES_MS + 500,
             "big document, fonts not arriving: waits at most the bound, then loads as before");
    d.jobFontes = NULL; }
  d.eventosEntregues = 7231; d.fontesPasso = 1;
  t0 = agoraMs(); esperarFontesGrande(&d);
  conferir(agoraMs() - t0 < 100, "not the first delivery: no wait");

  printf(falhas ? "mkvass_fontes_memoria: FAIL\n" : "mkvass_fontes_memoria: ok\n");
  return falhas ? 1 : 0;
}
