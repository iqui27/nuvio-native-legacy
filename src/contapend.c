#include "contapend.h"
#include "contacache.h"
#include "sessao.h"
#include "perfis.h"
#include "dados.h"
#include "js.h"
#include "jsw.h"
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Mesmo prototipo que contalib.c e app.c usam; mora em catalogo.c.
extern void cat_historico_definir_id(const char *imdb, const char *tipo, int visto);

// Lote por RPC de vistos. O web manda ate 5000 num corpo so; 200 deixa o corpo
// abaixo de ~30 KB e uma falha custa pouco para repetir.
#define CP_LOTE 200
// Paginas de sync_pull_library no ler-mesclar-escrever: o mesmo 500 do web.
// Sem teto de ultima pagina NAO HA PUSH — uma lista que nao coube inteira nao
// pode ser a base de um push que substitui a remota.
#define CP_PAG 500
#define CP_PAGS 40

typedef struct {
  char sup;            // 'V' visto, 'L' lista (biblioteca/Salvos)
  char op;             // '+' marca/salva, '-' desmarca/tira
  char conf;           // 1 = a conta ja respondeu 2xx (fica ate a poda)
  int  perfil;
  char id[24];
  char tipo[8];
  int  temp, ep;       // 0/0 = titulo inteiro (season/episode nulos)
  long long ms;        // quando a pessoa fez o gesto
  long long confMs;
  char nome[160];
  char poster[1024];   // o de CatItem.poster (#361); o arquivo e texto com TAB
} Ent;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static Ent *ents;
static int nEnt, capEnt, carregado;
static char usuario[80];
static long long (*relogio)(void);
static int semFio;

// ---------------------------------------------------------------- base

long long contapend_agora_ms(void) {
  struct timespec ts;
  if (relogio) return relogio();
  clock_gettime(CLOCK_REALTIME, &ts);
  return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
}
void contapend_relogio(long long (*f)(void)) { relogio = f; }
void contapend_sem_fio(int sim) { semFio = sim; }

static int ok2xx(const char *r, int st) { return r && st >= 200 && st < 300; }

// Vistos: o id do titulo e tudo antes do primeiro ':' (a regra de syncprog.c e
// vistoep.c). Lista: so o sufixo ":<temp>:<ep>" em digitos sai — "tmdb:55" e
// um id inteiro (a regra de salvos_id_titulo).
static void idVisto(const char *s, char *d, size_t tam) {
  size_t k = 0;
  d[0] = 0;
  if (!s) return;
  while (s[k] && s[k] != ':' && k + 1 < tam) { d[k] = s[k]; k++; }
  d[k] = 0;
}
static void idLista(const char *s, char *d, size_t tam) {
  const char *a, *b;
  size_t n;
  d[0] = 0;
  if (!s) return;
  n = strlen(s);
  b = strrchr(s, ':');
  if (b && b > s && b[1] && isdigit((unsigned char)b[1])) {
    const char *q = b + 1;
    while (*q && isdigit((unsigned char)*q)) q++;
    if (!*q) {
      a = b - 1;
      while (a > s && isdigit((unsigned char)*a)) a--;
      if (*a == ':' && a < b - 1) n = (size_t)(a - s);
    }
  }
  if (n + 1 > tam) n = tam - 1;
  memcpy(d, s, n);
  d[n] = 0;
}

static void limpo(char *d, size_t tam, const char *s) {
  size_t k = 0;
  if (!s) s = "";
  while (s[k] && k + 1 < tam) {
    d[k] = (s[k] == '\t' || s[k] == '\n' || s[k] == '\r') ? ' ' : s[k];
    k++;
  }
  d[k] = 0;
}

static void nomeArquivo(char *d, size_t tam) {
  char u[64];
  size_t k = 0, j = 0;
  while (usuario[k] && j + 1 < sizeof u) {
    char c = usuario[k++];
    if (isalnum((unsigned char)c) || c == '-') u[j++] = c;
  }
  u[j] = 0;
  snprintf(d, tam, "conta-pend-%s.txt", u[0] ? u : "anon");
}

// So com a trava.
static void gravar(void) {
  char arq[128], *buf, *p;
  size_t tam = 64;
  int i;
  for (i = 0; i < nEnt; i++) tam += sizeof(Ent) + 64;   // linha <= campos + TABs
  buf = (char *)malloc(tam);
  if (!buf) return;
  p = buf;
  *p = 0;
  for (i = 0; i < nEnt; i++) {
    const Ent *e = &ents[i];
    int k = snprintf(p, tam - (size_t)(p - buf),
                     "%c\t%c\t%d\t%d\t%s\t%s\t%d\t%d\t%lld\t%lld\t%s\t%s\n",
                     e->sup, e->op, e->conf, e->perfil, e->id, e->tipo,
                     e->temp, e->ep, e->ms, e->confMs, e->nome, e->poster);
    if (k < 0 || (size_t)k >= tam - (size_t)(p - buf)) break;
    p += k;
  }
  nomeArquivo(arq, sizeof arq);
  if (nEnt) dados_gravar(arq, buf);
  else dados_apagar(arq);
  free(buf);
}

static int caber(int n) {
  Ent *x;
  int novo;
  if (n <= capEnt) return 1;
  novo = capEnt ? capEnt * 2 : 64;
  while (novo < n) novo *= 2;
  if (novo > CONTAPEND_MAX) novo = CONTAPEND_MAX;
  if (novo < n) return 0;
  x = (Ent *)realloc(ents, sizeof *x * (size_t)novo);
  if (!x) return 0;
  ents = x;
  capEnt = novo;
  return 1;
}

static void lerArquivo(void) {
  char arq[128], *b, *linha;
  nomeArquivo(arq, sizeof arq);
  b = dados_ler(arq);
  if (!b) return;
  for (linha = b; linha && *linha;) {
    char *fim = strchr(linha, '\n'), *c[12];
    int k = 0;
    if (fim) *fim = 0;
    c[k++] = linha;
    { char *q;
      for (q = linha; *q && k < 12; q++) if (*q == '\t') { *q = 0; c[k++] = q + 1; } }
    if (k >= 10 && (c[0][0] == 'V' || c[0][0] == 'L') &&
        (c[1][0] == '+' || c[1][0] == '-') && c[4][0] && caber(nEnt + 1)) {
      Ent *e = &ents[nEnt];
      memset(e, 0, sizeof *e);
      e->sup = c[0][0];
      e->op = c[1][0];
      e->conf = (char)(atoi(c[2]) ? 1 : 0);
      e->perfil = atoi(c[3]);
      snprintf(e->id, sizeof e->id, "%s", c[4]);
      snprintf(e->tipo, sizeof e->tipo, "%s", c[5]);
      e->temp = atoi(c[6]);
      e->ep = atoi(c[7]);
      e->ms = atoll(c[8]);
      e->confMs = atoll(c[9]);
      if (k > 10) snprintf(e->nome, sizeof e->nome, "%s", c[10]);
      if (k > 11) snprintf(e->poster, sizeof e->poster, "%s", c[11]);
      if (e->perfil > 0) nEnt++;
    }
    linha = fim ? fim + 1 : NULL;
  }
  free(b);
}

// So com a trava. 1 quando ha usuario logado e o jornal dele esta em memoria.
static int garantir(void) {
  const char *u;
  if (!sessao_logada()) return 0;
  u = sessao_usuario();
  if (!u || !u[0]) return 0;
  if (carregado && !strcmp(usuario, u)) return 1;
  nEnt = 0;
  snprintf(usuario, sizeof usuario, "%s", u);
  carregado = 1;
  lerArquivo();
  if (nEnt) {
    int i, p = 0;
    for (i = 0; i < nEnt; i++) if (!ents[i].conf) p++;
    printf("[sync] jornal da conta: %d entradas, %d pendentes de envio\n", nEnt, p);
    fflush(stdout);
  }
  return 1;
}

static int achar(char sup, int perfil, const char *id, int t, int e) {
  int i;
  for (i = 0; i < nEnt; i++)
    if (ents[i].sup == sup && ents[i].perfil == perfil && ents[i].temp == t &&
        ents[i].ep == e && !strcmp(ents[i].id, id)) return i;
  return -1;
}

static void tirar(int i) {
  memmove(&ents[i], &ents[i + 1], sizeof *ents * (size_t)(nEnt - i - 1));
  nEnt--;
}

// So com a trava. Devolve 1 quando a entrada mudou.
static int registrar(char sup, int perfil, const char *id, const char *tipo,
                     int t, int e, char op, const char *nome,
                     const char *poster, long long ms) {
  int i = achar(sup, perfil, id, t, e);
  Ent *x;
  if (i >= 0) {
    // A MESMA INTENCAO AINDA PENDENTE nao muda nada. Confirmada, o gesto novo
    // sobe de novo: e idempotente na conta e e o que a pessoa pediu agora.
    if (ents[i].op == op && !ents[i].conf) return 0;
    x = &ents[i];
  } else {
    if (nEnt >= CONTAPEND_MAX || !caber(nEnt + 1)) {
      // CHEIO: sai a confirmada mais velha; sem nenhuma, a nova e recusada e o
      // log diz. Nunca sai uma pendente — seria perder um gesto em silencio.
      int k, velho = -1;
      for (k = 0; k < nEnt; k++)
        if (ents[k].conf && (velho < 0 || ents[k].confMs < ents[velho].confMs)) velho = k;
      if (velho < 0) {
        printf("[sync] jornal da conta cheio (%d pendentes): gesto em %s nao "
               "entrou\n", nEnt, id);
        fflush(stdout);
        return 0;
      }
      tirar(velho);
    }
    x = &ents[nEnt++];
    memset(x, 0, sizeof *x);
    x->sup = sup;
    x->perfil = perfil;
    snprintf(x->id, sizeof x->id, "%s", id);
    x->temp = t;
    x->ep = e;
  }
  x->op = op;
  x->conf = 0;
  x->confMs = 0;
  x->ms = ms;
  limpo(x->tipo, sizeof x->tipo, tipo && tipo[0] ? tipo : (sup == 'V' && e > 0 ? "series" : "movie"));
  if (strcmp(x->tipo, "series") && strcmp(x->tipo, "movie"))
    snprintf(x->tipo, sizeof x->tipo, "%s",
             (!strcmp(x->tipo, "show") || !strcmp(x->tipo, "tv")) ? "series" : "movie");
  limpo(x->nome, sizeof x->nome, nome);
  limpo(x->poster, sizeof x->poster, poster);
  return 1;
}

// ---------------------------------------------------------------- registro

int contapend_episodios(const char *imdb, const char *tipo,
                        const VistoPar *pares, int n, int visto) {
  char id[24];
  int i, k = 0, perfil = perfis_ativo();
  long long ms = contapend_agora_ms();
  idVisto(imdb, id, sizeof id);
  if (!id[0] || !pares || n < 1 || perfil < 1) return 0;
  pthread_mutex_lock(&trava);
  if (garantir()) {
    for (i = 0; i < n; i++)
      if (pares[i].episodio > 0 && pares[i].temporada >= 0)
        k += registrar('V', perfil, id, tipo && tipo[0] ? tipo : "series",
                       pares[i].temporada, pares[i].episodio,
                       visto ? '+' : '-', "", "", ms);
    if (k) gravar();
  }
  pthread_mutex_unlock(&trava);
  return k;
}

int contapend_titulo(const char *imdb, const char *tipo, int visto) {
  char id[24];
  int k = 0, perfil = perfis_ativo();
  idVisto(imdb, id, sizeof id);
  if (!id[0] || perfil < 1) return 0;
  pthread_mutex_lock(&trava);
  if (garantir()) {
    k = registrar('V', perfil, id, tipo && !strcmp(tipo, "series") ? "series" : "movie",
                  0, 0, visto ? '+' : '-', "", "", contapend_agora_ms());
    if (k) gravar();
  }
  pthread_mutex_unlock(&trava);
  return k;
}

int contapend_lista(const char *imdb, const char *tipo, const char *nome,
                    const char *poster, int salvo) {
  char id[24];
  int k = 0, perfil = perfis_ativo();
  idLista(imdb, id, sizeof id);
  if (!id[0] || perfil < 1) return 0;
  pthread_mutex_lock(&trava);
  if (garantir()) {
    k = registrar('L', perfil, id, tipo, 0, 0, salvo ? '+' : '-',
                  nome, poster, contapend_agora_ms());
    if (k) gravar();
  }
  pthread_mutex_unlock(&trava);
  return k;
}

// ---------------------------------------------------------------- confirmacao

// A copia enviada so quita a entrada se ela nao mudou durante a viagem (mesma
// intencao, mesmo instante). Um gesto novo no meio fica pendente.
static void confirmar(const char *usu, const Ent *env, int n) {
  int i, k;
  long long agora = contapend_agora_ms();
  pthread_mutex_lock(&trava);
  if (carregado && !strcmp(usuario, usu)) {
    for (i = 0; i < n; i++) {
      k = achar(env[i].sup, env[i].perfil, env[i].id, env[i].temp, env[i].ep);
      if (k >= 0 && ents[k].op == env[i].op && ents[k].ms == env[i].ms && !ents[k].conf) {
        ents[k].conf = 1;
        ents[k].confMs = agora;
      }
    }
    gravar();
  }
  pthread_mutex_unlock(&trava);
}

// A conta tem uma mudanca MAIS NOVA que o gesto: o gesto sai (ultima vence).
static void superar(const char *usu, const Ent *env) {
  int k;
  pthread_mutex_lock(&trava);
  if (carregado && !strcmp(usuario, usu)) {
    k = achar(env->sup, env->perfil, env->id, env->temp, env->ep);
    if (k >= 0 && ents[k].op == env->op && ents[k].ms == env->ms) { tirar(k); gravar(); }
  }
  pthread_mutex_unlock(&trava);
}

// ---------------------------------------------------------------- vistos

static int enviarVistos(const char *usu, int perfil, const Ent *env, int n, char op) {
  Jsw w;
  char *r;
  int i, st = 0, ok;
  if (n < 1) return 0;
  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_ci(&w, "p_profile_id", perfil);
  jsw_cs(&w, "p_origin_client_id", dados_cliente_id());
  jsw_chave(&w, op == '+' ? "p_items" : "p_keys");
  jsw_arr_ini(&w);
  for (i = 0; i < n; i++) {
    const Ent *e = &env[i];
    jsw_obj_ini(&w);
    jsw_cs(&w, "content_id", e->id);
    if (op == '+') {
      // toRemoteItem do web: content_id, content_type, title, season, episode,
      // watched_at (ms). Titulo inteiro = season/episode nulos.
      jsw_cs(&w, "content_type", e->tipo);
      jsw_cs(&w, "title", "");
      if (e->ep > 0) { jsw_ci(&w, "season", e->temp); jsw_ci(&w, "episode", e->ep); }
      else { jsw_chave(&w, "season"); jsw_nulo(&w); jsw_chave(&w, "episode"); jsw_nulo(&w); }
      jsw_ci(&w, "watched_at", e->ms);
    } else if (e->ep > 0) {
      // toDeleteKey: so content_id, e season/episode quando existem.
      jsw_ci(&w, "season", e->temp);
      jsw_ci(&w, "episode", e->ep);
    }
    jsw_obj_fim(&w);
  }
  jsw_arr_fim(&w);
  jsw_obj_fim(&w);
  r = sessao_rpc(op == '+' ? "sync_push_watched_items" : "sync_delete_watched_items",
                 jsw_texto_final(&w), &st);
  jsw_livre(&w);
  ok = ok2xx(r, st);
  free(r);
  printf("[sync] vistos perfil %d: %s %d na conta -> %s (HTTP %d)\n", perfil,
         op == '+' ? "marcar" : "desmarcar", n, ok ? "ok" : "falhou", st);
  fflush(stdout);
  if (!ok) return -1;
  confirmar(usu, env, n);
  return n;
}

// ---------------------------------------------------------------- lista

typedef struct { char *pag[CP_PAGS]; int nPag, nLinhas; } Paginas;

static void paginasLivrar(Paginas *p) {
  int i;
  for (i = 0; i < p->nPag; i++) free(p->pag[i]);
  memset(p, 0, sizeof *p);
}

static int arrayCru(const char *r) {
  while (r && (*r == ' ' || *r == '\n' || *r == '\r' || *r == '\t')) r++;
  return r && *r == '[';
}

// A lista INTEIRA da conta, ou -1. Pagina que falha, resposta que nao e array
// ou o teto de paginas = -1: sem a lista inteira nao ha base para o push.
static int puxarListaInteira(int perfil, Paginas *out) {
  int pagina;
  memset(out, 0, sizeof *out);
  for (pagina = 0; pagina < CP_PAGS; pagina++) {
    char corpo[128];
    const char *p;
    char *r;
    int st = 0, c = 0;
    snprintf(corpo, sizeof corpo,
             "{\"p_profile_id\":%d,\"p_limit\":%d,\"p_offset\":%d}",
             perfil, CP_PAG, pagina * CP_PAG);
    r = sessao_rpc("sync_pull_library", corpo, &st);
    if (!ok2xx(r, st) || !arrayCru(r)) {
      printf("[sync] lista perfil %d: pagina %d nao veio (HTTP %d); nada sobe\n",
             perfil, pagina + 1, st);
      fflush(stdout);
      free(r);
      paginasLivrar(out);
      return -1;
    }
    for (p = js_raiz_array(r); p; p = js_prox(js_fim(p))) c++;
    out->pag[out->nPag++] = r;
    out->nLinhas += c;
    if (c < CP_PAG) return out->nLinhas;
  }
  printf("[sync] lista perfil %d: mais de %d linhas na conta; nada sobe\n",
         perfil, CP_PAG * CP_PAGS);
  fflush(stdout);
  paginasLivrar(out);
  return -1;
}

static long long instanteMs(const char *ini, const char *fim, const char *chave) {
  char t[64];
  double d;
  if (js_texto(ini, fim, chave, t, sizeof t)) {
    if (strchr(t, '-') || strchr(t, 'T') || strchr(t, ' ')) return js_ms_iso(t);
    d = atof(t);
  } else d = js_num(ini, fim, chave, 0.0);
  if (d <= 0.0) return 0;
  return (long long)(d < 1e12 ? d * 1000.0 : d);
}

// A linha REMOTA de volta, no formato toRemoteItem do web. Os valores que
// existem vao CRUS (o que veio, volta igual); os que faltam ganham o padrao do
// web. `buf` tem o tamanho da linha inteira, entao nenhum valor deixa de caber.
static void emitirLinha(Jsw *w, const char *p, const char *f, char *buf, size_t tam,
                        long long addedMs) {
  static const struct { const char *chave, *padrao; } campos[] = {
    { "content_id", NULL }, { "content_type", "\"movie\"" }, { "name", NULL },
    { "poster", "null" }, { "poster_shape", "\"POSTER\"" }, { "background", "null" },
    { "description", "\"\"" }, { "release_info", "\"\"" }, { "imdb_rating", "null" },
    { "genres", "[]" }, { "addon_base_url", "null" },
  };
  unsigned i;
  jsw_obj_ini(w);
  for (i = 0; i < sizeof campos / sizeof *campos; i++) {
    jsw_chave(w, campos[i].chave);
    if (js_bruto(p, f, campos[i].chave, buf, tam)) { jsw_bruto(w, buf); continue; }
    if (!strcmp(campos[i].chave, "name")) {
      if (js_bruto(p, f, "title", buf, tam)) jsw_bruto(w, buf);
      else jsw_str(w, "Untitled");
      continue;
    }
    jsw_bruto(w, campos[i].padrao);
  }
  // `logo` e coluna da tabela e o web nao manda; se veio, volta.
  if (js_bruto(p, f, "logo", buf, tam)) { jsw_chave(w, "logo"); jsw_bruto(w, buf); }
  jsw_ci(w, "added_at", addedMs > 0 ? addedMs : contapend_agora_ms());
  jsw_obj_fim(w);
}

static void emitirNovo(Jsw *w, const Ent *e) {
  jsw_obj_ini(w);
  jsw_cs(w, "content_id", e->id);
  jsw_cs(w, "content_type", e->tipo);
  jsw_cs(w, "name", e->nome[0] ? e->nome : "Untitled");
  if (e->poster[0]) jsw_cs(w, "poster", e->poster); else { jsw_chave(w, "poster"); jsw_nulo(w); }
  jsw_cs(w, "poster_shape", "POSTER");
  jsw_chave(w, "background"); jsw_nulo(w);
  jsw_cs(w, "description", "");
  jsw_cs(w, "release_info", "");
  jsw_chave(w, "imdb_rating"); jsw_nulo(w);
  jsw_chave(w, "genres"); jsw_arr_ini(w); jsw_arr_fim(w);
  jsw_chave(w, "addon_base_url"); jsw_nulo(w);
  jsw_ci(w, "added_at", e->ms);
  jsw_obj_fim(w);
}

static int indiceOp(const Ent *ops, int n, const char *id) {
  int i;
  for (i = 0; i < n; i++) if (!strcmp(ops[i].id, id)) return i;
  return -1;
}

// 1 quando `id` esta em alguma pagina.
static int naLista(const Paginas *pg, const char *id) {
  int i;
  for (i = 0; i < pg->nPag; i++) {
    const char *p;
    for (p = js_raiz_array(pg->pag[i]); p; p = js_prox(js_fim(p))) {
      char cid[64], b[24];
      if (!js_texto(p, js_fim(p), "content_id", cid, sizeof cid)) continue;
      idLista(cid, b, sizeof b);
      if (!strcmp(b, id)) return 1;
    }
  }
  return 0;
}

// LER-MESCLAR-ESCREVER. Ver o cabecalho: sync_push_library e a UNICA escrita
// da biblioteca que o servidor tem (OpenAPI do PostgREST, 06/10/2026: nenhuma
// sync_delete_library), e o web a usa como "a lista do perfil e esta". Entao o
// que sobe e SEMPRE a lista remota inteira, menos o que a pessoa tirou, mais o
// que ela salvou.
static int enviarLista(const char *usu, int perfil, Ent *ops, int n) {
  Paginas pg;
  Jsw w;
  char *buf = NULL, *r;
  size_t bufTam = 0;
  int i, remoto, tirados = 0, novos = 0, st = 0, ok, saida;
  char *usado;

  remoto = puxarListaInteira(perfil, &pg);
  if (remoto < 0) return -1;

  // LISTA REMOTA VAZIA (regra 1 da secao 1.6): pode ser perfil errado, 401 mal
  // tratado ou servidor fora. Com a ultima copia boa nao vazia, nada sobe.
  if (remoto == 0) {
    long q = 0;
    char *c = contacache_ler(CC_BIBLIOTECA, perfil, usu, &q);
    int k = 0;
    const char *p;
    for (p = c ? js_raiz_array(c) : NULL; p; p = js_prox(js_fim(p))) k++;
    free(c);
    if (k > 0) {
      printf("[sync] lista perfil %d: conta respondeu vazia e a copia tem %d; "
             "nada sobe\n", perfil, k);
      fflush(stdout);
      paginasLivrar(&pg);
      return -1;
    }
  }

  usado = (char *)calloc((size_t)n + 1, 1);
  if (!usado) { paginasLivrar(&pg); return -1; }
  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_ci(&w, "p_profile_id", perfil);
  jsw_cs(&w, "p_origin_client_id", dados_cliente_id());
  jsw_chave(&w, "p_items");
  jsw_arr_ini(&w);
  saida = 0;
  for (i = 0; i < pg.nPag; i++) {
    const char *p;
    for (p = js_raiz_array(pg.pag[i]); p; p = js_prox(js_fim(p))) {
      const char *f = js_fim(p);
      char cid[64], id[24];
      long long added;
      int k;
      size_t lin = (size_t)(f - p) + 1;
      // REGRA 3: linha que nao se sabe representar recusa o push inteiro.
      // Omiti-la seria apaga-la da conta.
      if (!js_texto(p, f, "content_id", cid, sizeof cid) || !cid[0]) {
        printf("[sync] lista perfil %d: linha da conta sem content_id; nada sobe\n", perfil);
        fflush(stdout);
        jsw_livre(&w); free(buf); free(usado); paginasLivrar(&pg);
        return -1;
      }
      if (lin > bufTam) {
        char *nb = (char *)realloc(buf, lin);
        if (!nb) { jsw_livre(&w); free(buf); free(usado); paginasLivrar(&pg); return -1; }
        buf = nb; bufTam = lin;
      }
      idLista(cid, id, sizeof id);
      added = instanteMs(p, f, "added_at");
      k = indiceOp(ops, n, id);
      if (k >= 0 && ops[k].op == '-') {
        if (added > ops[k].ms) {
          // Salvo de novo em outro aparelho DEPOIS de a pessoa tirar aqui.
          usado[k] = 2;
        } else { usado[k] = 1; tirados++; continue; }
      } else if (k >= 0) usado[k] = 1;   // '+' que a conta ja tem
      emitirLinha(&w, p, f, buf, bufTam, added);
      saida++;
    }
  }
  for (i = 0; i < n; i++)
    if (ops[i].op == '+' && !usado[i]) { emitirNovo(&w, &ops[i]); usado[i] = 1; novos++; saida++; }
  jsw_arr_fim(&w);
  jsw_obj_fim(&w);
  free(buf);

  for (i = 0; i < n; i++) if (usado[i] == 2) superar(usu, &ops[i]);

  if (!tirados && !novos) {
    // A conta ja esta como a pessoa quer (ou um '-' de item que nem esta la).
    jsw_livre(&w);
    for (i = 0; i < n; i++) if (usado[i] != 2) confirmar(usu, &ops[i], 1);
    printf("[sync] lista perfil %d: conta %d, nada a mudar\n", perfil, remoto);
    fflush(stdout);
    free(usado); paginasLivrar(&pg);
    return n;
  }
  if (saida == 0) {
    // Tirar o ULTIMO titulo exigiria subir lista vazia — e push vazio nunca
    // sai (regra 2). O web tambem nao tem como (savedLibrarySyncService.js
    // pula o push vazio; nao ha RPC nem delete de tabela para isso). Fica
    // pendente e oculto aqui; sai sozinho quando a lista ganhar outro titulo.
    // Nao e falha: o log diz UMA vez por sessao.
    static int avisou;
    jsw_livre(&w);
    if (!avisou) {
      avisou = 1;
      printf("[sync] lista perfil %d: tirar o ultimo titulo pediria push vazio; "
             "fica pendente ate a lista ganhar outro\n", perfil);
      fflush(stdout);
    }
    free(usado); paginasLivrar(&pg);
    return 0;
  }
  r = sessao_rpc("sync_push_library", jsw_texto_final(&w), &st);
  jsw_livre(&w);
  ok = ok2xx(r, st);
  free(r);
  printf("[sync] lista perfil %d: conta %d, +%d -%d -> %d -> %s (HTTP %d)\n",
         perfil, remoto, novos, tirados, saida, ok ? "ok" : "falhou", st);
  fflush(stdout);
  paginasLivrar(&pg);
  if (!ok) { free(usado); return -1; }

  // CONFERENCIA: um pull a mais so quando algo mudou. E a prova, no log, de que
  // o push fez o que se esperava — inclusive se o servidor NAO apagar os
  // tirados (ai a linha diz, e a entrada fica oculta aqui ate a poda).
  { Paginas v;
    if (puxarListaInteira(perfil, &v) >= 0) {
      int presentes = 0, ausentes = 0;
      for (i = 0; i < n; i++) {
        if (usado[i] == 2) continue;
        if (ops[i].op == '+') presentes += naLista(&v, ops[i].id);
        else ausentes += !naLista(&v, ops[i].id);
      }
      printf("[sync] lista perfil %d: conferido %d linhas; %d de %d salvos "
             "presentes, %d de %d tirados ausentes\n", perfil, v.nLinhas,
             presentes, novos, ausentes, tirados);
      fflush(stdout);
      paginasLivrar(&v);
    } }
  for (i = 0; i < n; i++) if (usado[i] != 2) confirmar(usu, &ops[i], 1);
  free(usado);
  return n;
}

// ---------------------------------------------------------------- envio

static pthread_mutex_t envioTrava = PTHREAD_MUTEX_INITIALIZER;
static int enviarJa(void);

// UM ENVIO POR VEZ: o fio do gesto e o ciclo do sync podem chegar juntos, e
// dois ler-mesclar-escrever da lista em paralelo sobem bases diferentes.
int contapend_enviar(void) {
  int r;
  pthread_mutex_lock(&envioTrava);
  r = enviarJa();
  pthread_mutex_unlock(&envioTrava);
  return r;
}

static int enviarJa(void) {
  Ent *snap;
  char usu[80];
  int i, n = 0, falhou = 0, total = 0, perfis[16], nPerfis = 0;

  pthread_mutex_lock(&trava);
  if (!garantir()) { pthread_mutex_unlock(&trava); return 0; }
  for (i = 0; i < nEnt; i++) if (!ents[i].conf) n++;
  if (!n) { pthread_mutex_unlock(&trava); return 0; }
  snap = (Ent *)malloc(sizeof *snap * (size_t)n);
  if (!snap) { pthread_mutex_unlock(&trava); return -1; }
  n = 0;
  for (i = 0; i < nEnt; i++) if (!ents[i].conf) snap[n++] = ents[i];
  snprintf(usu, sizeof usu, "%s", usuario);
  pthread_mutex_unlock(&trava);

  for (i = 0; i < n; i++) {
    int k, tem = 0;
    for (k = 0; k < nPerfis; k++) if (perfis[k] == snap[i].perfil) tem = 1;
    if (!tem && nPerfis < 16) perfis[nPerfis++] = snap[i].perfil;
  }

  { Ent *grupo = (Ent *)malloc(sizeof *grupo * (size_t)n);
    int p;
    if (!grupo) { free(snap); return -1; }
    for (p = 0; p < nPerfis; p++) {
      static const char ops[2] = { '+', '-' };
      int o, g;
      // CADA GESTO NO PERFIL ONDE FOI FEITO. O ativo pode ter mudado depois;
      // a marca e de quem a fez.
      for (o = 0; o < 2; o++) {
        g = 0;
        for (i = 0; i < n; i++)
          if (snap[i].sup == 'V' && snap[i].perfil == perfis[p] && snap[i].op == ops[o]) {
            grupo[g++] = snap[i];
            if (g == CP_LOTE) {
              int r = enviarVistos(usu, perfis[p], grupo, g, ops[o]);
              if (r < 0) falhou = 1; else total += r;
              g = 0;
            }
          }
        if (g) {
          int r = enviarVistos(usu, perfis[p], grupo, g, ops[o]);
          if (r < 0) falhou = 1; else total += r;
        }
      }
      g = 0;
      for (i = 0; i < n; i++)
        if (snap[i].sup == 'L' && snap[i].perfil == perfis[p]) grupo[g++] = snap[i];
      if (g) {
        int r = enviarLista(usu, perfis[p], grupo, g);
        if (r < 0) falhou = 1; else total += r;
      }
    }
    free(grupo); }
  free(snap);
  if (total > 0) {
    printf("[sync] jornal da conta: %d confirmadas, %d pendentes\n", total,
           contapend_pendentes());
    fflush(stdout);
  }
  return falhou ? -1 : total;
}

static pthread_mutex_t fioTrava = PTHREAD_MUTEX_INITIALIZER;
static int fioNoAr, deNovo;

static void *fio(void *u) {
  (void)u;
  for (;;) {
    contapend_enviar();
    pthread_mutex_lock(&fioTrava);
    if (!deNovo) { fioNoAr = 0; pthread_mutex_unlock(&fioTrava); break; }
    deNovo = 0;
    pthread_mutex_unlock(&fioTrava);
  }
  return NULL;
}

void contapend_chutar(void) {
  pthread_t t;
  if (semFio) { contapend_enviar(); return; }
  pthread_mutex_lock(&fioTrava);
  if (fioNoAr) { deNovo = 1; pthread_mutex_unlock(&fioTrava); return; }
  fioNoAr = 1;
  pthread_mutex_unlock(&fioTrava);
  if (pthread_create(&t, NULL, fio, NULL) != 0) {
    pthread_mutex_lock(&fioTrava);
    fioNoAr = 0;
    pthread_mutex_unlock(&fioTrava);
    return;
  }
  pthread_detach(t);
}

// ---------------------------------------------------------------- pull

static int oculto(char sup, const char *id, int t, int e, long long remotoMs) {
  int k, perfil = perfis_ativo(), res = 0;
  pthread_mutex_lock(&trava);
  if (garantir()) {
    k = achar(sup, perfil, id, t, e);
    if (k >= 0 && ents[k].op == '-') {
      if (remotoMs > ents[k].ms) {
        // A conta tem uma marca MAIS NOVA que o "tirar" daqui: ela ganha. A
        // pendente sai (nao vai apagar o que outro aparelho fez depois).
        if (!ents[k].conf) { tirar(k); gravar(); }
      } else res = 1;
    }
  }
  pthread_mutex_unlock(&trava);
  return res;
}

int contapend_visto_oculto(const char *imdb, int temporada, int episodio,
                           long long remotoMs) {
  char id[24];
  idVisto(imdb, id, sizeof id);
  if (!id[0]) return 0;
  if (episodio <= 0) temporada = episodio = 0;
  return oculto('V', id, temporada, episodio, remotoMs);
}

int contapend_lista_oculta(const char *imdb, long long remotoMs) {
  char id[24];
  idLista(imdb, id, sizeof id);
  if (!id[0]) return 0;
  return oculto('L', id, 0, 0, remotoMs);
}

int contapend_aplicar_local(void) {
  int i, k = 0, perfil = perfis_ativo();
  Ent *copia = NULL;
  VistoFonte fonte;
  int n = 0;
  memset(&fonte, 0, sizeof fonte);
  pthread_mutex_lock(&trava);
  if (garantir() && nEnt) {
    copia = (Ent *)malloc(sizeof *copia * (size_t)nEnt);
    if (copia)
      for (i = 0; i < nEnt; i++)
        if (ents[i].sup == 'V' && ents[i].perfil == perfil) copia[n++] = ents[i];
  }
  pthread_mutex_unlock(&trava);
  // Fora da trava: o historico tem a trava dele.
  for (i = 0; i < n; i++) {
    // O JORNAL TAMBEM E FONTE (vistoep_fonte): um '+' daqui e um gesto ANTIGO
    // sendo reaplicado a cada ciclo, com o instante dele — nao pode passar por
    // cima de uma desmarcacao mais nova do mesmo episodio.
    if (copia[i].ep > 0) {
      int antes = vistoep_estado(copia[i].id, copia[i].temp, copia[i].ep), depois;
      vistoep_fonte(copia[i].id, copia[i].temp, copia[i].ep, copia[i].op == '+',
                    copia[i].ms, &fonte);
      depois = vistoep_estado(copia[i].id, copia[i].temp, copia[i].ep);
      if (antes != depois) {
        printf("[vistoep] %s T%dE%d: jornal perfil=%d op=%c gesto_ms=%lld confirmado=%d "
               "conf_ms=%lld mapa=%d->%d\n", copia[i].id, copia[i].temp, copia[i].ep,
               copia[i].perfil, copia[i].op, copia[i].ms, copia[i].conf,
               copia[i].confMs, antes, depois);
        fflush(stdout);
      }
    } else
      cat_historico_definir_id(copia[i].id, copia[i].tipo, copia[i].op == '+');
    k++;
  }
  free(copia);
  // Mudo no caso normal (roda a cada ciclo); fala quando algo foi barrado.
  if (fonte.bloqueados || fonte.venceu)
    printf("[vistoep] jornal da conta: +%d (bloqueados %d; remoto mais novo %d)\n",
           fonte.vistos, fonte.bloqueados, fonte.venceu);
  return k;
}

void contapend_podar(long long desdeMs) {
  int i, mudou = 0;
  pthread_mutex_lock(&trava);
  if (garantir()) {
    for (i = nEnt - 1; i >= 0; i--)
      if (ents[i].conf && ents[i].confMs < desdeMs) { tirar(i); mudou = 1; }
    if (mudou) gravar();
  }
  pthread_mutex_unlock(&trava);
}

int contapend_pendentes(void) {
  int i, k = 0;
  pthread_mutex_lock(&trava);
  if (garantir()) for (i = 0; i < nEnt; i++) if (!ents[i].conf) k++;
  pthread_mutex_unlock(&trava);
  return k;
}

void contapend_esquecer(void) {
  pthread_mutex_lock(&trava);
  nEnt = 0;
  carregado = 0;
  usuario[0] = 0;
  pthread_mutex_unlock(&trava);
}
