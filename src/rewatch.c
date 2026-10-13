#include "rewatch.h"
#include "sessao.h"
#include "perfis.h"
#include "dados.h"
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Mesmo criterio de vistonao.c: relogio de TV recem-ligada nao e crivel. Antes
// de RW_RELOGIO_SAO o carimbo vira "o anterior + 1" e a sessao nunca perde de si.
#define RW_RELOGIO_SAO 1704067200000LL        // 01/01/2024 UTC
#define RW_CHAVE       "rewatchSessions"

typedef struct { short t, e, n; } Rev;        // n: vezes revisto NA sessao

typedef struct {
  int       perfil;
  char      id[24];        // o id ate o ':' (a mesma chave de vistoep.c)
  long long ini;           // quando comecou
  long long atual;         // ultima mudanca: decide a mesclagem com a conta
  long long fim;           // 0 = ativa; senao lapide (encerrada em `fim`)
  int       origem;
  short     pt, pe, pct;   // ponto parcial (pct 0 = nenhum)
  short     nrev;
  Rev       rev[RW_EPS];
} Sess;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static Sess sess[RW_MAX];
static int nSess, carregado, sujo;
static char usuario[80];
static long long (*relogio)(void);

static long long agoraMs(void) {
  struct timespec ts;
  if (relogio) return relogio();
  clock_gettime(CLOCK_REALTIME, &ts);
  return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
}
void rewatch_relogio(long long (*f)(void)) { relogio = f; }

// Carimbo sempre maior que o anterior da mesma sessao, mesmo com relogio torto.
static long long carimbo(long long prev) {
  long long ms = agoraMs();
  if (ms < RW_RELOGIO_SAO || ms <= prev) ms = prev + 1;
  return ms;
}

static int idBase(const char *s, char *d, size_t tam) {
  size_t k = 0;
  d[0] = 0;
  if (!s) return 0;
  while (s[k] && s[k] != ':' && k + 1 < tam) {
    unsigned char c = (unsigned char)s[k];
    if (!isalnum(c) && c != '-' && c != '_') { d[0] = 0; return 0; }
    d[k] = (char)c;
    k++;
  }
  d[k] = 0;
  return k > 0;
}

static void nomeArquivo(char *d, size_t tam) {
  char u[64];
  size_t k = 0, j = 0;
  while (usuario[k] && j + 1 < sizeof u) {
    char c = usuario[k++];
    if (isalnum((unsigned char)c) || c == '-') u[j++] = c;
  }
  u[j] = 0;
  snprintf(d, tam, "rewatch-%s.txt", u[0] ? u : "anon");
}

// ---- texto: uma linha por sessao ------------------------------------------
// "perfil id ini atual fim origem pt pe pct t.e.n,t.e.n,..." (ou "-" sem eps).
// Sem tab e sem aspas: serve igual no arquivo (sep '\n') e dentro de uma string
// JSON da conta (sep '~').

static size_t tamTexto(int soPerfil) {
  size_t tam = 16;
  int i;
  for (i = 0; i < nSess; i++)
    if (!soPerfil || sess[i].perfil == soPerfil) tam += 160 + (size_t)sess[i].nrev * 20;
  return tam;
}

// So com a trava. soPerfil 0 = todos.
static char *serializar(char sep, int soPerfil) {
  size_t tam = tamTexto(soPerfil);
  char *buf = (char *)malloc(tam), *p;
  int i, k;
  if (!buf) return NULL;
  p = buf;
  *p = 0;
  for (i = 0; i < nSess; i++) {
    const Sess *s = &sess[i];
    if (soPerfil && s->perfil != soPerfil) continue;
    p += sprintf(p, "%d %s %lld %lld %lld %d %d %d %d ", s->perfil, s->id, s->ini, s->atual,
                 s->fim, s->origem, s->pt, s->pe, s->pct);
    if (!s->nrev) *p++ = '-';
    for (k = 0; k < s->nrev; k++)
      p += sprintf(p, "%s%d.%d.%d", k ? "," : "", s->rev[k].t, s->rev[k].e, s->rev[k].n);
    *p++ = sep;
    *p = 0;
  }
  return buf;
}

// Uma linha -> Sess. 1 se valida.
static int lerLinha(const char *linha, Sess *s) {
  int consumido = 0, perfil, origem, pt, pe, pct;
  long long ini, atual, fim;
  char id[24];
  const char *q;
  memset(s, 0, sizeof *s);
  if (sscanf(linha, "%d %23s %lld %lld %lld %d %d %d %d %n", &perfil, id, &ini, &atual, &fim,
             &origem, &pt, &pe, &pct, &consumido) < 9 || !consumido)
    return 0;
  if (perfil < 1 || atual < 1 || ini < 0 || fim < 0 || pct < 0 || pct > 99 || pt < 0 ||
      pt > 32767 || pe < 0 || pe > 32767)
    return 0;
  if (!idBase(id, s->id, sizeof s->id) || strcmp(id, s->id)) return 0;
  s->perfil = perfil;
  s->ini = ini;
  s->atual = atual;
  s->fim = fim;
  s->origem = origem == RW_ORIGEM_CONTINUAR ? RW_ORIGEM_CONTINUAR : RW_ORIGEM_TITULO;
  s->pt = (short)pt;
  s->pe = (short)pe;
  s->pct = (short)pct;
  for (q = linha + consumido; *q && s->nrev < RW_EPS;) {
    char *fimNum;
    long t = strtol(q, &fimNum, 10), e, n;
    if (fimNum == q || *fimNum != '.') break;
    q = fimNum + 1;
    e = strtol(q, &fimNum, 10);
    if (fimNum == q || *fimNum != '.') break;
    q = fimNum + 1;
    n = strtol(q, &fimNum, 10);
    if (fimNum == q) break;
    if (t >= 0 && t <= 32767 && e >= 1 && e <= 32767 && n >= 1 && n <= 32767 && !fim) {
      s->rev[s->nrev].t = (short)t;
      s->rev[s->nrev].e = (short)e;
      s->rev[s->nrev].n = (short)n;
      s->nrev++;
    }
    q = fimNum;
    if (*q == ',') q++;
  }
  return 1;
}

// ---- tabela ---------------------------------------------------------------

static int achar(int perfil, const char *id) {
  int i;
  for (i = 0; i < nSess; i++)
    if (sess[i].perfil == perfil && !strcmp(sess[i].id, id)) return i;
  return -1;
}

// Vaga para uma sessao nova. Cheia: sai a lapide mais velha; sem lapide, a
// ativa mais velha (uma sessao esquecida ha meses e a que menos vale).
static Sess *vaga(void) {
  int i, v = -1;
  if (nSess < RW_MAX) { memset(&sess[nSess], 0, sizeof sess[0]); return &sess[nSess++]; }
  for (i = 0; i < nSess; i++) {
    if (v < 0) { v = i; continue; }
    if ((sess[i].fim > 0) != (sess[v].fim > 0)) { if (sess[i].fim > 0) v = i; continue; }
    if (sess[i].atual < sess[v].atual) v = i;
  }
  memset(&sess[v], 0, sizeof sess[0]);
  return &sess[v];
}

static void podar(void) {
  long long agora = agoraMs();
  int i;
  if (agora < RW_RELOGIO_SAO) return;
  for (i = nSess - 1; i >= 0; i--)
    if (sess[i].fim > 0 && agora - sess[i].fim > RW_LAPIDE_MS) {
      memmove(&sess[i], &sess[i + 1], sizeof sess[0] * (size_t)(nSess - i - 1));
      nSess--;
    }
}

// So com a trava.
static void gravar(void) {
  char arq[128], *txt;
  podar();
  nomeArquivo(arq, sizeof arq);
  if (!nSess) { dados_apagar(arq); return; }
  txt = serializar('\n', 0);
  if (!txt) return;
  dados_gravar(arq, txt);
  free(txt);
}

// So com a trava. Funde uma sessao vinda de fora. 1 se mudou algo aqui.
static int fundir(const Sess *r) {
  int i = achar(r->perfil, r->id);
  if (i < 0) { *vaga() = *r; return 1; }
  if (r->atual > sess[i].atual) { sess[i] = *r; return 1; }
  return 0;
}

// So com a trava. Texto -> tabela. soPerfil 0 = aceita qualquer perfil.
static int absorver(const char *txt, char sep, int soPerfil) {
  char *c = strdup(txt), *linha, *fim;
  int mudou = 0;
  static Sess r;           // grande demais para a pilha de uma TV; sob a trava
  if (!c) return 0;
  for (linha = c; linha && *linha; linha = fim ? fim + 1 : NULL) {
    fim = strchr(linha, sep);
    if (fim) *fim = 0;
    if (lerLinha(linha, &r) && (!soPerfil || r.perfil == soPerfil)) mudou += fundir(&r);
  }
  free(c);
  return mudou;
}

// So com a trava. O arquivo e por usuario da conta ("anon" sem conta).
static void garantir(void) {
  const char *u = sessao_logada() ? sessao_usuario() : "";
  char arq[128], *b;
  if (!u) u = "";
  if (carregado && !strcmp(usuario, u)) return;
  nSess = 0;
  snprintf(usuario, sizeof usuario, "%s", u);
  carregado = 1;
  nomeArquivo(arq, sizeof arq);
  b = dados_ler(arq);
  if (!b) return;
  absorver(b, '\n', 0);
  free(b);
  podar();
  // O que estava no disco pode nunca ter subido (app fechado antes do push).
  if (nSess) sujo = 1;
}

// ---- API ------------------------------------------------------------------

// So com a trava. Sessao ATIVA do perfil ativo, ou -1.
static int ativaIdx(const char *imdb) {
  char id[24];
  int i;
  if (!idBase(imdb, id, sizeof id)) return -1;
  garantir();
  i = achar(perfis_ativo(), id);
  return i >= 0 && !sess[i].fim ? i : -1;
}

int rewatch_ativa(const char *imdb) {
  int r;
  pthread_mutex_lock(&trava);
  r = ativaIdx(imdb) >= 0;
  pthread_mutex_unlock(&trava);
  return r;
}

int rewatch_iniciar(const char *imdb, int origem) {
  char id[24];
  int i, perfil = perfis_ativo(), criou = 0;
  if (!idBase(imdb, id, sizeof id) || perfil < 1) return 0;
  pthread_mutex_lock(&trava);
  garantir();
  i = achar(perfil, id);
  if (i < 0 || sess[i].fim) {
    long long prev = i >= 0 ? sess[i].atual : 0;
    Sess *s = i >= 0 ? &sess[i] : vaga();
    memset(s, 0, sizeof *s);
    s->perfil = perfil;
    snprintf(s->id, sizeof s->id, "%s", id);
    s->atual = s->ini = carimbo(prev);
    s->origem = origem == RW_ORIGEM_CONTINUAR ? RW_ORIGEM_CONTINUAR : RW_ORIGEM_TITULO;
    criou = 1;
    sujo = 1;
    gravar();
  }
  pthread_mutex_unlock(&trava);
  if (criou) { printf("[rewatch] %s: sessao iniciada (perfil %d)\n", id, perfil); fflush(stdout); }
  return criou;
}

void rewatch_encerrar(const char *imdb) {
  int i, feitos = 0;
  char id[24] = "";
  pthread_mutex_lock(&trava);
  i = ativaIdx(imdb);
  if (i >= 0) {
    Sess *s = &sess[i];
    feitos = s->nrev;
    snprintf(id, sizeof id, "%s", s->id);
    s->atual = s->fim = carimbo(s->atual);
    s->nrev = 0;
    s->pt = s->pe = s->pct = 0;
    sujo = 1;
    gravar();
  }
  pthread_mutex_unlock(&trava);
  if (i >= 0) { printf("[rewatch] %s: sessao encerrada (%d episodios revistos)\n", id, feitos); fflush(stdout); }
}

int rewatch_registrar(const char *imdb, int temporada, int episodio) {
  int i, k, r = 0;
  if (temporada < 0 || temporada > 32767 || episodio < 1 || episodio > 32767) return 0;
  pthread_mutex_lock(&trava);
  i = ativaIdx(imdb);
  if (i >= 0) {
    Sess *s = &sess[i];
    for (k = 0; k < s->nrev; k++)
      if (s->rev[k].t == temporada && s->rev[k].e == episodio) break;
    if (k < s->nrev) { if (s->rev[k].n < 32767) s->rev[k].n++; r = 1 + s->rev[k].n; }
    else if (s->nrev < RW_EPS) {
      s->rev[k].t = (short)temporada;
      s->rev[k].e = (short)episodio;
      s->rev[k].n = 1;
      s->nrev++;
      r = 2;
    } else r = 2;
    s->pt = s->pe = s->pct = 0;
    s->atual = carimbo(s->atual);
    sujo = 1;
    gravar();
  }
  pthread_mutex_unlock(&trava);
  return r;
}

int rewatch_vezes(const char *imdb, int temporada, int episodio) {
  int i, k, r = 0;
  pthread_mutex_lock(&trava);
  i = ativaIdx(imdb);
  if (i >= 0)
    for (k = 0; k < sess[i].nrev; k++)
      if (sess[i].rev[k].t == temporada && sess[i].rev[k].e == episodio) {
        r = 1 + sess[i].rev[k].n;
        break;
      }
  pthread_mutex_unlock(&trava);
  return r;
}

int rewatch_feitos(const char *imdb) {
  int i, r = 0;
  pthread_mutex_lock(&trava);
  i = ativaIdx(imdb);
  if (i >= 0) r = sess[i].nrev;
  pthread_mutex_unlock(&trava);
  return r;
}

long long rewatch_inicio_ms(const char *imdb) {
  int i;
  long long r = 0;
  pthread_mutex_lock(&trava);
  i = ativaIdx(imdb);
  if (i >= 0) r = sess[i].ini;
  pthread_mutex_unlock(&trava);
  return r;
}

int rewatch_origem(const char *imdb) {
  int i, r = 0;
  pthread_mutex_lock(&trava);
  i = ativaIdx(imdb);
  if (i >= 0) r = sess[i].origem;
  pthread_mutex_unlock(&trava);
  return r;
}

void rewatch_parcial(const char *imdb, int temporada, int episodio, int pct) {
  int i;
  pthread_mutex_lock(&trava);
  i = ativaIdx(imdb);
  if (i >= 0) {
    Sess *s = &sess[i];
    if (pct >= 1 && pct <= 99 && temporada >= 0 && temporada <= 32767 && episodio >= 1 &&
        episodio <= 32767) {
      s->pt = (short)temporada;
      s->pe = (short)episodio;
      s->pct = (short)pct;
    } else s->pt = s->pe = s->pct = 0;
    s->atual = carimbo(s->atual);
    sujo = 1;
    gravar();
  }
  pthread_mutex_unlock(&trava);
}

int rewatch_parcial_de(const char *imdb, int *temporada, int *episodio, int *pct) {
  int i, r = 0;
  pthread_mutex_lock(&trava);
  i = ativaIdx(imdb);
  if (i >= 0 && sess[i].pct > 0) {
    if (temporada) *temporada = sess[i].pt;
    if (episodio) *episodio = sess[i].pe;
    if (pct) *pct = sess[i].pct;
    r = 1;
  }
  pthread_mutex_unlock(&trava);
  return r;
}

// So com a trava. Indices das ativas do perfil ativo, mais recente primeiro.
static int listarAtivas(int *idx) {
  int i, n = 0, k, perfil = perfis_ativo();
  garantir();
  for (i = 0; i < nSess; i++) {
    if (sess[i].perfil != perfil || sess[i].fim) continue;
    for (k = n; k > 0 && sess[idx[k - 1]].atual < sess[i].atual; k--) idx[k] = idx[k - 1];
    idx[k] = i;
    n++;
  }
  return n;
}

int rewatch_n_ativas(void) {
  int idx[RW_MAX], n;
  pthread_mutex_lock(&trava);
  n = listarAtivas(idx);
  pthread_mutex_unlock(&trava);
  return n;
}

int rewatch_ativa_n(int i, char *imdb, size_t tam) {
  int idx[RW_MAX], n, r = 0;
  pthread_mutex_lock(&trava);
  n = listarAtivas(idx);
  if (i >= 0 && i < n && imdb && tam) {
    snprintf(imdb, tam, "%s", sess[idx[i]].id);
    r = 1;
  }
  pthread_mutex_unlock(&trava);
  return r;
}

// ---- Conta Nuvio ------------------------------------------------------------

char *rewatch_texto(void) {
  char *t;
  pthread_mutex_lock(&trava);
  garantir();
  t = serializar('~', perfis_ativo());
  pthread_mutex_unlock(&trava);
  return t;
}

int rewatch_mesclar(const char *texto) {
  int mudou;
  if (!texto || !*texto) return 0;
  pthread_mutex_lock(&trava);
  garantir();
  mudou = absorver(texto, '~', perfis_ativo());
  if (mudou) gravar();
  pthread_mutex_unlock(&trava);
  if (mudou) { printf("[rewatch] conta: %d sessoes atualizadas\n", mudou); fflush(stdout); }
  return mudou;
}

// Acha `"rewatchSessions":"` em `blob`. Devolve o inicio do VALOR (depois da
// aspa de abertura) e, em *fim, a aspa de fechamento; NULL se nao existe.
static const char *acharChave(const char *blob, const char **fim) {
  const char *k = strstr(blob, "\"" RW_CHAVE "\""), *v;
  if (!k) return NULL;
  v = k + strlen(RW_CHAVE) + 2;
  while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r') v++;
  if (*v != ':') return NULL;
  v++;
  while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r') v++;
  if (*v != '"') return NULL;
  v++;
  *fim = strchr(v, '"');
  return *fim ? v : NULL;
}

char *rewatch_blob_costurar(const char *base) {
  char *texto, *saida;
  const char *v, *fim, *fecha;
  size_t nb, nt, off;
  if (!base) return NULL;
  texto = rewatch_texto();
  if (!texto) return NULL;
  nb = strlen(base);
  nt = strlen(texto);
  v = acharChave(base, &fim);
  if (v) {
    saida = (char *)malloc(nb + nt + 1);
    if (!saida) { free(texto); return NULL; }
    off = (size_t)(v - base);
    memcpy(saida, base, off);
    memcpy(saida + off, texto, nt);
    strcpy(saida + off + nt, fim);
    free(texto);
    return saida;
  }
  // Chave nova: so se ha o que levar. Sem sessao nenhuma o blob fica como veio.
  fecha = base + nb;
  while (fecha > base && fecha[-1] != '}') fecha--;
  if (fecha == base) { free(texto); return NULL; }
  fecha--;                        // aponta o '}'
  if (!nt) { free(texto); return strdup(base); }
  {
    const char *ant = fecha;
    int vazio;
    while (ant > base && isspace((unsigned char)ant[-1])) ant--;
    vazio = ant > base && ant[-1] == '{';
    saida = (char *)malloc(nb + nt + 32);
    if (!saida) { free(texto); return NULL; }
    off = (size_t)(fecha - base);
    memcpy(saida, base, off);
    off += (size_t)sprintf(saida + off, "%s\"" RW_CHAVE "\":\"%s\"", vazio ? "" : ",", texto);
    strcpy(saida + off, fecha);
  }
  free(texto);
  return saida;
}

int rewatch_blob_ler(const char *blob) {
  const char *v, *fim;
  char *t;
  int r;
  if (!blob || !(v = acharChave(blob, &fim)) || fim == v) return 0;
  t = (char *)malloc((size_t)(fim - v) + 1);
  if (!t) return 0;
  memcpy(t, v, (size_t)(fim - v));
  t[fim - v] = 0;
  r = rewatch_mesclar(t);
  free(t);
  return r;
}

int rewatch_sujo(void) {
  int r;
  pthread_mutex_lock(&trava);
  garantir();
  r = sujo;
  sujo = 0;
  pthread_mutex_unlock(&trava);
  return r;
}

void rewatch_sujar(void) {
  pthread_mutex_lock(&trava);
  sujo = 1;
  pthread_mutex_unlock(&trava);
}

void rewatch_esquecer(void) {
  pthread_mutex_lock(&trava);
  nSess = 0;
  carregado = 0;
  sujo = 0;
  usuario[0] = 0;
  pthread_mutex_unlock(&trava);
}
