#include "capmkv.h"
#include "mkvass.h"
#include "rede.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// #385: rede.c define; fraca para os testes que trocam rede.c por um stub.
__attribute__((weak)) void rede_lateral(int sim) { (void)sim; }

#define CAP_JANELA (320L * 1024)
#define CAP_TENTATIVAS 3
#define CAP_CACHE 4

int capmkv_espera_inicial_ms = 4000;
void (*capmkv_teste_antes_de_publicar)(void);
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static unsigned ger;                       // reproducao corrente
static double nomeado, ultimo;             // regra da LG: nome > ultimo no ultimo quarto
// A lista INTEIRA da reproducao corrente, para a barra e a fileira de capitulos.
static MkvCap lista[MKV_MAX_CAPS];
static int nLista;

// URL NO HEAP, do tamanho dela, e nao char[512]: o player guarda ate 4096
// (video_tpk.c/video_android.c urlAtual) e link assinado de debrid/CDN passa
// de 512 facil — acima disso o capmkv voltava calado e o video ficava sem
// capitulos. Sao CAP_CACHE entradas: o custo e o tamanho real de 4 URLs.
typedef struct { char *url; int n; MkvCap caps[MKV_MAX_CAPS]; } Entrada;
static Entrada cache[CAP_CACHE];
static int cacheProx;

static int urlMkvPossivel(const char *u) {
  // MP4/M4V nao tem Chapters do Matroska: nem vale a descida.
  const char *q = strpbrk(u, "?#");
  size_t n = q ? (size_t)(q - u) : strlen(u);
  return !(n > 4 && (!strncasecmp(u + n - 4, ".mp4", 4) || !strncasecmp(u + n - 4, ".m4v", 4)));
}

static int cacheLer(const char *url, MkvCap *caps, int max) {
  int i, n = -1;
  pthread_mutex_lock(&trava);
  for (i = 0; i < CAP_CACHE; i++)
    if (cache[i].url && !strcmp(cache[i].url, url)) {
      n = cache[i].n < max ? cache[i].n : max;
      if (n > 0) memcpy(caps, cache[i].caps, (size_t)n * sizeof *caps);
      break;
    }
  pthread_mutex_unlock(&trava);
  return n;
}
static void cacheGuardar(const char *url, const MkvCap *caps, int n) {
  Entrada *e;
  char *copia = strdup(url);
  if (!copia) {
    printf("[mkv] sem memoria para guardar no cache a URL de %zu bytes\n", strlen(url));
    fflush(stdout);
    return;
  }
  pthread_mutex_lock(&trava);
  e = &cache[cacheProx++ % CAP_CACHE];
  free(e->url);
  e->url = copia;
  e->n = n;
  if (n > 0) memcpy(e->caps, caps, (size_t)n * sizeof *caps);
  pthread_mutex_unlock(&trava);
}

static void dormir(int ms) {
  struct timespec t = { ms / 1000, (long)(ms % 1000) * 1000000L };
  nanosleep(&t, NULL);
}
static int vale(unsigned g) { int v; pthread_mutex_lock(&trava); v = g == ger; pthread_mutex_unlock(&trava); return v; }

int capmkv_trechos(const MkvCap *caps, int n, IntroTrecho *out, int max) {
  MkvSeg seg[6];
  int k = 0, i, ns;
  if (!caps || n < 1 || !out || max < 1) return 0;
  // Sem a duracao: o ultimo capitulo vai "ate o fim" (fim 0) e a janela de
  // intro.c mede quando a duracao chegar.
  ns = mkv_segmentos(caps, n, 0.0, seg, 6);
  for (i = 0; i < ns && k < max; i++) {
    // A previa final nao e credito: so marca onde o cartao do proximo pode subir.
    double fim = seg[i].tipo == MKV_SEG_PREVIA ? 0.0 : seg[i].fim;
    out[k++] = (IntroTrecho){ seg[i].inicio, fim, seg[i].tipo, 0 };
  }
  return k;
}

// [seg] Um log por capitulo: tipo, regra e por que foi recusado (diagnostico
// do feat/mkv-chapter-segments, tudo no mesmo build).
static void logSegmentos(const MkvCap *caps, int n) {
  static const char *NOME[] = { "-", "abertura", "resumo", "creditos", "previa", "pos-creditos" };
  MkvSeg seg[6];
  int ns = mkv_segmentos(caps, n, 0.0, seg, 6), i, j;
  for (i = 0; i < n; i++) {
    int forca, t = mkv_classificar_nome(caps[i].nome, &forca), usado = 0;
    for (j = 0; j < ns; j++) if (seg[j].cap == i) usado = 1;
    printf("[seg] cap %d %.1fs \"%s\" -> %s (%s)%s\n", i, caps[i].inicio, caps[i].nome,
           NOME[t >= 0 && t <= 5 ? t : 0],
           forca == 3 ? "estrito" : forca == 2 ? "intro" : forca == 1 ? "extra" : "sem nome util",
           t && !usado ? " RECUSADO (duracao, posicao ou estrito do mesmo tipo)" : "");
  }
  for (j = 0; j < ns; j++)
    printf("[seg] segmento %s %.1f-%.1fs (%s)\n", NOME[seg[j].tipo], seg[j].inicio, seg[j].fim,
           seg[j].estrito ? "estrito" : "extra");
  fflush(stdout);
}

// Publica capitulos. Com `checa`, so se `g` ainda e a reproducao corrente: a
// checagem, os creditos e a entrega ao modulo de intro ficam na MESMA secao
// critica que capmkv_zerar usa para invalidar e limpar (revisao 2.0.3, achado
// 8). Ordem de travas: capmkv -> intro (intro.c nunca chama capmkv).
// Tudo que classifica nomes (regex, ~6 ms para 64 capitulos num PC) roda ANTES
// da trava: o player le a lista com a mesma trava a cada quadro (capmkv_lista).
static int publicar(const MkvCap *caps, int n, int checa, unsigned g) {
  IntroTrecho v[6];
  int k = n > 0 ? capmkv_trechos(caps, n, v, 6) : 0;
  double a = n > 0 ? mkv_creditos_nomeados(caps, n) : 0.0;
  double b = n > 0 ? mkv_creditos_ultimo(caps, n) : 0.0;
  IntroTrecho pv[2];
  MkvSeg ps[2];
  int np = n > 0 ? mkv_segmentos_palpite(caps, n, ps, 2) : 0, i;
  for (i = 0; i < np; i++) pv[i] = (IntroTrecho){ ps[i].inicio, ps[i].fim, ps[i].tipo, 0 };
  double mc[MKV_MAX_CAPS];
  int nm = n > MKV_MAX_CAPS ? MKV_MAX_CAPS : (n > 0 ? n : 0);
  for (i = 0; i < nm; i++) mc[i] = caps[i].inicio;
  pthread_mutex_lock(&trava);
  if (checa && g != ger) { pthread_mutex_unlock(&trava); return 0; }
  nomeado = a;
  ultimo = b;
  nLista = n > MKV_MAX_CAPS ? MKV_MAX_CAPS : (n > 0 ? n : 0);
  if (nLista > 0) memcpy(lista, caps, (size_t)nLista * sizeof *caps);
  if (k > 0) intro_definir_capitulos(v, k);
  intro_definir_capitulos_palpite(np > 0 ? pv : NULL, np);
  intro_definir_capitulos_marcas(nm > 0 ? mc : NULL, nm);
  pthread_mutex_unlock(&trava);
  if (n > 0) {
    printf("[mkv] %d capitulos; creditos nomeados em %.0fs, ultimo em %.0fs\n", n, a, b);
    fflush(stdout);
    logSegmentos(caps, n);
  }
  return 1;
}

void capmkv_aplicar(const MkvCap *caps, int n) { publicar(caps, n, 0, 0); }
int capmkv_aplicar_ger(const MkvCap *caps, int n, unsigned g) { return publicar(caps, n, 1, g); }
unsigned capmkv_geracao(void) {
  unsigned g;
  pthread_mutex_lock(&trava); g = ger; pthread_mutex_unlock(&trava);
  return g;
}

double capmkv_creditos(double dur) {
  double a = capmkv_creditos_nomeado();
  return a > 1.0 ? a : capmkv_creditos_ultimo(dur);
}
double capmkv_creditos_nomeado(void) {
  double a;
  pthread_mutex_lock(&trava); a = nomeado; pthread_mutex_unlock(&trava);
  return a > 1.0 ? a : 0.0;
}
// "Ultimo capitulo = creditos" (2.0.3): so depois de 75% e deixando ao menos
// CAPMKV_ULTIMO_FOLGA_S. Um toco no fim do arquivo (Ch07 1420 de 1421 s, log da
// C9 em 04/11) nao e creditos: o cartao ficava com 1 s.
double capmkv_creditos_ultimo(double dur) {
  double b;
  pthread_mutex_lock(&trava); b = ultimo; pthread_mutex_unlock(&trava);
  if (b > 1.0 && dur > 1.0 && b > dur * 0.75 && dur - b >= CAPMKV_ULTIMO_FOLGA_S) return b;
  return 0.0;
}

// Uma leitura: cabecalho da pre-busca (sem rede) ou 320 KB; Chapters fora da
// janela vai pelo SeekHead. `*recusa` = 1 quando o servidor recusou (recuar).
static int lerUma(const char *url, MkvCap *caps, int max, int *recusa) {
  unsigned char *cab = NULL;
  long cabN = 0;
  int n = 0, st = 0, proprio = 0;
  *recusa = 0;
  if (!mkvass_cabecalho(url, &cab, &cabN) || !cab) {
    cab = (unsigned char *)rede_baixar_trecho_st(url, 20, 0, CAP_JANELA - 1, &cabN, &st, NULL, NULL, 0);
    proprio = 1;
    if (!cab) { *recusa = 1; return -1; }
  }
  st = -1;
  n = mkv_capitulos_alem(url, cab, cabN, caps, max, &st);
  free(cab);
  // st == -1: nao houve pedido extra, o resultado (inclusive 0) e definitivo.
  // Pedido extra sem 2xx: transporte/CDN recusou — recuar e tentar de novo.
  if (!n && st != -1 && (st < 200 || st >= 300)) { *recusa = 1; return -1; }
  (void)proprio;
  return n;
}

typedef struct { unsigned g; char url[]; } Pedido;   // url do tamanho dela

static int lerComRecuo(const char *url, MkvCap *caps, int max, unsigned g, int esperaMs) {
  static const int RECUO_MS[CAP_TENTATIVAS] = { 0, 3000, 10000 };
  int t, n;
  for (t = 0; t < CAP_TENTATIVAS; t++) {
    int recusa = 0, ms = RECUO_MS[t] + (t == 0 ? esperaMs : 0);
    while (ms > 0) { int p = ms > 250 ? 250 : ms; dormir(p); ms -= p; if (g && !vale(g)) return -2; }
    n = lerUma(url, caps, max, &recusa);
    if (n >= 0) return n;                    // inclusive "arquivo sem capitulos"
    if (g && !vale(g)) return -2;
  }
  return -1;
}

static void *fio(void *arg) {
  Pedido *p = arg;
  MkvCap caps[MKV_MAX_CAPS];
  int n;
  rede_lateral(1);   // #385: leitura lateral ao video (ver rede.h)
  n = lerComRecuo(p->url, caps, MKV_MAX_CAPS, p->g, capmkv_espera_inicial_ms);
  if (n >= 0) {
    cacheGuardar(p->url, caps, n);      // por URL: vale mesmo se o video ja mudou
    if (capmkv_teste_antes_de_publicar) capmkv_teste_antes_de_publicar();
    if (n > 0 && !publicar(caps, n, 1, p->g)) {
      printf("[mkv] capitulos de um video anterior descartados\n");
      fflush(stdout);
    } else {
      printf("[mkv] capitulos (android/tpk): %d\n", n);
      fflush(stdout);
    }
  }
  free(p);
  return NULL;
}

void capmkv_zerar(void) {
  pthread_mutex_lock(&trava);
  ger++; nomeado = ultimo = 0.0; nLista = 0;
  intro_definir_capitulos(NULL, 0);   // sob a trava: nenhum fio publica no meio
  intro_definir_capitulos_palpite(NULL, 0);
  intro_definir_capitulos_marcas(NULL, 0);
  pthread_mutex_unlock(&trava);
}

void capmkv_iniciar(const char *url) {
  Pedido *p;
  pthread_t t;
  MkvCap caps[MKV_MAX_CAPS];
  int n;
  capmkv_zerar();
  if (!url || !url[0] || !urlMkvPossivel(url)) return;
  n = cacheLer(url, caps, MKV_MAX_CAPS);
  if (n >= 0) { if (n > 0) capmkv_aplicar(caps, n); return; }
  { size_t tam = strlen(url) + 1;
    p = malloc(sizeof *p + tam);
    if (!p) {
      printf("[mkv] sem memoria para o pedido de capitulos (URL de %zu bytes)\n", tam - 1);
      fflush(stdout);
      return;
    }
    memcpy(p->url, url, tam); }
  pthread_mutex_lock(&trava); p->g = ger; pthread_mutex_unlock(&trava);
  if (pthread_create(&t, NULL, fio, p) == 0) pthread_detach(t); else free(p);
}

int capmkv_lista(MkvCap *out, int max) {
  int n;
  if (!out || max <= 0) return 0;
  pthread_mutex_lock(&trava);
  n = nLista < max ? nLista : max;
  if (n > 0) memcpy(out, lista, (size_t)n * sizeof *out);
  pthread_mutex_unlock(&trava);
  return n;
}

int capmkv_ler_agora(const char *url, MkvCap *caps, int max, int esperaMs) {
  int n = lerComRecuo(url, caps, max, 0, esperaMs);
  return n;
}
