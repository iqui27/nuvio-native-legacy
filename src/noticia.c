// Ver noticia.h.
#include "noticia.h"
#include "leitura.h"
#include "rede.h"
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "nvbackend.h"
#ifndef NV_REC_URL
#define NV_REC_URL ""
#endif

#define NTC_ENTRADAS 8
#define NTC_PAGINA_MAX (512L * 1024L)
// A pagina do artigo NO GOOGLE NEWS e maior que a do veiculo: ~570 KB medidos,
// e a assinatura esta perto do fim (offset 566 005 de 567 534). 768 KB cobre
// com folga; passou disso sem assinatura, desiste.
#define NTC_GN_MAX (768L * 1024L)
#define NTC_PRAZO_S 10
// Falha e lembrada por pouco tempo: rede que voltou merece nova tentativa na
// proxima abertura, mas nao a cada quadro com o modal aberto.
#define NTC_FALHA_S 120

typedef struct {
  char link[400];
  NoticiaTexto t;
  long quando;      // time(NULL) do fim (pronta/falhou)
  unsigned seq;     // ordem do pedido: o mais novo sai primeiro
  unsigned uso;     // ultimo acesso, para reciclar
  int iniciado;
} Entrada;

static Entrada ent[NTC_ENTRADAS];
static unsigned seqPedido, seqUso;
static int fioVivo;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static NtcBaixar baixarTeste;

void noticia_rede_teste(NtcBaixar f) { baixarTeste = f; }

static Entrada *achar(const char *link) {
  int i;
  for (i = 0; i < NTC_ENTRADAS; i++) if (ent[i].link[0] && !strcmp(ent[i].link, link)) return &ent[i];
  return NULL;
}

// --- rede ---------------------------------------------------------------------

// Navegador de mesa: varios veiculos devolvem 403 a "Nuvio/1.0" (o UA padrao de
// rede.c) e a pagina certa ao mesmo pedido com UA de navegador. O idioma vai
// junto para o portal escolher a edicao certa.
static const char *const CAB_PAGINA[] = {
  "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/128.0 Safari/537.36",
  "Accept: text/html,application/xhtml+xml;q=0.9,*/*;q=0.8",
  NULL
};

static char *pegar(const char *url, long teto, int *status) {
  if (baixarTeste) return baixarTeste(url, NULL, status);
  { RedeControle c = { teto, NULL };
    RedeMedida m;
    char *r;
    memset(&m, 0, sizeof m);
    r = rede_baixar_medido_controle(url, NTC_PRAZO_S, CAB_PAGINA, &c, &m);
    *status = m.status;
    return r; }
}

#if !defined(__EMSCRIPTEN__)
static char *postar(const char *url, const char *corpo, int *status) {
  static const char *const CAB[] = {
    "Content-Type: application/x-www-form-urlencoded;charset=UTF-8",
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/128.0 Safari/537.36",
    NULL };
  if (baixarTeste) return baixarTeste(url, corpo, status);
  return rede_postar_st(url, NTC_PRAZO_S, CAB, corpo, status);
}
#endif

static int ok2xx(int st) { return st >= 200 && st < 300; }

#if !defined(__EMSCRIPTEN__)
// Link do agregador -> URL do veiculo. 1 quando resolveu.
static int resolver(const char *link, char *dst, size_t tam) {
  char id[300], sg[120], ts[32], *pg, *resp;
  static char corpo[2048];
  int st = 0, ok = 0;
  dst[0] = 0;
  if (leitura_link_direto(link, dst, tam)) return 1;
  if (!leitura_gn_link(link)) { snprintf(dst, tam, "%s", link); return 1; }
  if (!leitura_gn_id(link, id, sizeof id)) return 0;
  if (leitura_gn_antigo(id, dst, tam)) return 1;
  pg = pegar(link, NTC_GN_MAX, &st);
  if (pg && ok2xx(st) && leitura_gn_assinatura(pg, sg, sizeof sg, ts, sizeof ts) &&
      leitura_gn_corpo(id, ts, sg, corpo, sizeof corpo)) {
    st = 0;
    resp = postar("https://news.google.com/_/DotsSplashUi/data/batchexecute", corpo, &st);
    if (resp && ok2xx(st)) ok = leitura_gn_resposta(resp, dst, tam);
    free(resp);
  }
  free(pg);
  return ok;
}

static void buscarNativo(const char *link, NoticiaTexto *t) {
  char *pg;
  int st = 0;
  char host[120];
  if (!resolver(link, t->url, sizeof t->url)) { t->estado = NTC_FALHOU; return; }
  pg = pegar(t->url, NTC_PAGINA_MAX, &st);
  leitura_host(t->url, host, sizeof host);
  if (!pg || !ok2xx(st)) {
    // So o host: o caminho de uma URL de noticia nao e segredo, mas a query
    // pode trazer rastreio do agregador, e o log fica curto.
    printf("[noticia] %s: HTTP %d\n", host, st);
    free(pg);
    t->estado = NTC_FALHOU;
    return;
  }
  leitura_extrair(pg, t->url, &t->l);
  free(pg);
  // PRONTA exige TEXTO: so manchete e resumo o modal ja tinha pelo RSS, e a
  // diferenca e o que decide entre o corpo e o QR.
  t->estado = t->l.n > 0 || t->l.resumo[0] ? NTC_PRONTA : NTC_FALHOU;
  printf("[noticia] %s: %d paragrafo(s)%s\n", host, t->l.n, t->l.imagem[0] ? ", com capa" : "");
}
#endif

// A resposta do worker: {"url","titulo","resumo","imagem","site","paragrafos":[...]}.
static void lerJsonWorker(const char *js, NoticiaTexto *t) {
  const char *fim = js + strlen(js);
  static const struct { const char *k; size_t off, tam; } C[] = {
    { "\"url\"",    offsetof(NoticiaTexto, url),      sizeof(((NoticiaTexto *)0)->url) },
    { "\"titulo\"", offsetof(NoticiaTexto, l.titulo), sizeof(((NoticiaTexto *)0)->l.titulo) },
    { "\"resumo\"", offsetof(NoticiaTexto, l.resumo), sizeof(((NoticiaTexto *)0)->l.resumo) },
    { "\"imagem\"", offsetof(NoticiaTexto, l.imagem), sizeof(((NoticiaTexto *)0)->l.imagem) },
    { "\"site\"",   offsetof(NoticiaTexto, l.site),   sizeof(((NoticiaTexto *)0)->l.site) },
  };
  size_t i;
  for (i = 0; i < sizeof C / sizeof C[0]; i++) {
    const char *p = strstr(js, C[i].k);
    if (!p) continue;
    p = strchr(p + strlen(C[i].k), ':');
    if (!p) continue;
    p++;
    while (*p == ' ') p++;
    leitura_json_string(p, fim, (char *)t + C[i].off, C[i].tam);
  }
  { const char *p = strstr(js, "\"paragrafos\"");
    if (p) p = strchr(p, '[');
    while (p && *p && *p != ']' && t->l.n < LEI_PAR_MAX) {
      p = strchr(p, '"');
      if (!p) break;
      p = leitura_json_string(p, fim, t->l.par[t->l.n], LEI_PAR_TAM);
      if (!p) break;
      if (t->l.par[t->l.n][0]) t->l.n++;
      while (*p == ' ' || *p == ',' || *p == '\n') p++;
    } }
}

static void buscarWorker(const char *link, NoticiaTexto *t) {
  char url[1400], cod[1100];
  char *js;
  int st = 0;
  size_t k = 0;
  const char *s;
  if (!NV_REC_URL[0] && !baixarTeste) { t->estado = NTC_FALHOU; return; }
  for (s = link; *s && k + 4 < sizeof cod; s++) {
    unsigned char c = (unsigned char)*s;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~') cod[k++] = (char)c;
    else { snprintf(cod + k, 4, "%%%02X", c); k += 3; }
  }
  cod[k] = 0;
  snprintf(url, sizeof url, "%s/v1/noticia?u=%s", NV_REC_URL, cod);
  js = pegar(url, 64L * 1024L, &st);
  if (js && ok2xx(st)) lerJsonWorker(js, t);
  free(js);
  t->estado = (t->l.n > 0 || t->l.resumo[0]) ? NTC_PRONTA : NTC_FALHOU;
  printf("[noticia] worker: HTTP %d, %d paragrafo(s)\n", st, t->l.n);
}

// --- o fio ----------------------------------------------------------------------

static void *fio(void *arg) {
  static NoticiaTexto t;   // ~7 KB: fora da pilha do fio
  (void)arg;
  for (;;) {
    char link[400];
    Entrada *e = NULL;
    int i;
    pthread_mutex_lock(&trava);
    for (i = 0; i < NTC_ENTRADAS; i++)
      if (ent[i].link[0] && ent[i].t.estado == NTC_BUSCANDO && !ent[i].iniciado &&
          (!e || ent[i].seq > e->seq)) e = &ent[i];
    if (!e) { fioVivo = 0; pthread_mutex_unlock(&trava); return NULL; }
    e->iniciado = 1;
    snprintf(link, sizeof link, "%s", e->link);
    pthread_mutex_unlock(&trava);

    memset(&t, 0, sizeof t);
#if defined(__EMSCRIPTEN__)
    buscarWorker(link, &t);
#else
    // Teste: um link em worker.test passa pelo caminho da SAMSUNG (o JSON do
    // worker), para a captura exercitar esse parse fora do WASM.
    if (baixarTeste && strstr(link, "://worker.test/")) buscarWorker(link, &t);
    else buscarNativo(link, &t);
#endif

    pthread_mutex_lock(&trava);
    // A entrada pode ter sido reciclada enquanto o fio trabalhava: so escreve
    // se ainda e a mesma noticia.
    e = achar(link);
    if (e && e->t.estado == NTC_BUSCANDO) {
      e->t = t;
      e->quando = (long)time(NULL);
    }
    pthread_mutex_unlock(&trava);
  }
}

void noticia_pedir(const char *link) {
  Entrada *e;
  pthread_t f;
  int sobe = 0;
  if (!link || !link[0] || strlen(link) >= sizeof ent[0].link) return;
  pthread_mutex_lock(&trava);
  e = achar(link);
  if (e && (e->t.estado == NTC_BUSCANDO || e->t.estado == NTC_PRONTA ||
            (e->t.estado == NTC_FALHOU && (long)time(NULL) - e->quando < NTC_FALHA_S))) {
    e->uso = ++seqUso;
    // Pedido de novo com o fio ocupado: sobe na fila.
    if (e->t.estado == NTC_BUSCANDO && !e->iniciado) e->seq = ++seqPedido;
    pthread_mutex_unlock(&trava);
    return;
  }
  if (!e) {
    int i;
    for (i = 0; i < NTC_ENTRADAS; i++) {
      Entrada *c = &ent[i];
      if (c->t.estado == NTC_BUSCANDO && c->iniciado) continue;   // em voo: nao mexe
      if (!e || c->uso < e->uso) e = c;
    }
    if (!e) { pthread_mutex_unlock(&trava); return; }
  }
  memset(e, 0, sizeof *e);
  snprintf(e->link, sizeof e->link, "%s", link);
  e->t.estado = NTC_BUSCANDO;
  e->seq = ++seqPedido;
  e->uso = ++seqUso;
  if (!fioVivo) { fioVivo = 1; sobe = 1; }
  pthread_mutex_unlock(&trava);
  if (sobe) {
    if (pthread_create(&f, NULL, fio, NULL) == 0) pthread_detach(f);
    else { pthread_mutex_lock(&trava); fioVivo = 0; e->t.estado = NTC_FALHOU; pthread_mutex_unlock(&trava); }
  }
}

const NoticiaTexto *noticia_texto(const char *link, int *estado) {
  Entrada *e;
  int est;
  const NoticiaTexto *r = NULL;
  pthread_mutex_lock(&trava);
  e = link && link[0] ? achar(link) : NULL;
  est = e ? e->t.estado : NTC_NADA;
  if (e && (est == NTC_PRONTA || est == NTC_FALHOU)) { r = &e->t; e->uso = ++seqUso; }
  pthread_mutex_unlock(&trava);
  if (estado) *estado = est;
  // Pronta ou falhou a entrada nao muda mais ate ser reciclada, e so
  // noticia_pedir (este mesmo fio) recicla: ler fora da trava e seguro.
  return r;
}
