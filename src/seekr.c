#include "seekr.h"
#include "seekrvtt.h"
#include "rede.h"
#include "gfx.h"
#include "jpegrapido.h"
#include <SDL2/SDL.h>
#ifndef __EMSCRIPTEN__
#include <SDL2/SDL_image.h>
#endif
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static char     chave[96];
static int      chavePropria;
static unsigned geracao;
static int      estado = SEEKR_DESLIGADO;
static SeekrVtt vtt;
// O que ja foi pedido, para a mesma combinacao nao gastar outra consulta da
// cota (o player re-chama a cada troca de estado).
static char pedImdb[64]; static int pedT, pedE; static long pedDur;
static long long expiraUtc, retryUtc;
static Uint32 expiraTick;
static int ultimoHttp;
static unsigned ultimaLatencia;

// FOLHA DECODIFICADA: uma so, a ultima. Uma folha serve dezenas de cues
// seguidas, e procurar no filme anda quase sempre dentro dela; decodificar de
// novo a cada cue seria um JPEG inteiro por tecla.
//
// MEDIDO em 02/10/2026 com a API real: a folha e 3200x1800 (10x10 quadros), o
// que decodificado da ~22 MB. Por isso ela so vive ENQUANTO a pessoa procura:
// seekr_ocioso() a solta quando o avanco termina, e fica so o JPEG (bem menor)
// para a proxima busca nao baixar de novo.
static int            folhaIdx = -1;
static unsigned       folhaG;
static SDL_Surface   *folhaSup;
static char          *jpgBytes; static long jpgN; static int jpgIdx = -1;
static unsigned       jpgG;

// Recortes prontos, esperando o upload no fio de desenho.
#define SK_SLOTS 4
typedef struct { unsigned char *px; int w, h, cue; unsigned g; } Pronto;
static Pronto prontos[SK_SLOTS];
static int    emVoo;       // 1 = um fio de recorte trabalhando
// Ajuste de sincronia em ms, somado a posicao antes da escolha da cue (o
// "preview sync" da documentacao).
static long   ajusteMs;

// Lado do desenho (so o fio de desenho mexe): texturas por cue, para a fita
// (anterior, atual, seguinte) nao refazer upload a cada quadro.
typedef struct { GLuint tex; int cue; unsigned g; Uint32 uso; } Slot;
static Slot slots[SK_SLOTS];
static Uint32 relogioUso;

void seekr_definir_chave(const char *c) {
  pthread_mutex_lock(&trava);
  if (!strcmp(chave, c ? c : "")) { pthread_mutex_unlock(&trava); return; }
  snprintf(chave, sizeof chave, "%s", c ? c : "");
  // Chave nova: o que foi recusado ou achado com a anterior nao vale mais.
  geracao++; estado = SEEKR_DESLIGADO; pedImdb[0] = 0;
  expiraUtc = retryUtc = 0; ultimoHttp = 0; ultimaLatencia = 0;
  pthread_mutex_unlock(&trava);
}
void seekr_chave_propria(int propria) {
  pthread_mutex_lock(&trava); chavePropria = propria != 0; pthread_mutex_unlock(&trava);
}
int seekr_tem_chave(void) {
  int r; pthread_mutex_lock(&trava); r = chave[0] != 0; pthread_mutex_unlock(&trava);
  return r;
}
#ifdef NV_SHOT_HOOKS
static int shotEstado;
#endif
int seekr_estado(void) {
  int r;
#ifdef NV_SHOT_HOOKS
  if (shotEstado) return shotEstado;
#endif
  pthread_mutex_lock(&trava); r = estado; pthread_mutex_unlock(&trava);
  return r;
}
const char *seekr_estado_rotulo(int est) {
  switch (est) {
    case SEEKR_BUSCANDO: return "Buscando miniaturas";
    case SEEKR_PRONTO: return "Miniaturas disponíveis";
    case SEEKR_SEM_PREVIA: return "Este título não tem miniaturas";
    case SEEKR_CHAVE_RECUSADA: return "Chave do Seekr recusada";
    case SEEKR_LIMITE_LOCAL: return "Limite diário desta TV atingido";
    case SEEKR_LIMITE_PROVEDOR: return "Limite temporário do Seekr";
    case SEEKR_REDE_INDISPONIVEL: return "Seekr sem conexão";
    case SEEKR_ARMAZENAMENTO_INDISPONIVEL: return "Não foi possível guardar o uso do Seekr";
    case SEEKR_RELOGIO_INDISPONIVEL: return "Ajuste a data e a hora desta TV";
    default: return "Miniaturas não solicitadas";
  }
}
int seekr_horario_local(long long utc, char *saida, size_t tamanho) {
  struct tm local;
  time_t instante = (time_t)utc;
  if (!saida || !tamanho) return 0;
  saida[0] = 0;
  if (utc <= 0 || (long long)instante != utc || !localtime_r(&instante, &local)) return 0;
  return strftime(saida, tamanho, "%d/%m %H:%M", &local) != 0;
}
void seekr_uso(SeekrUso *uso) {
  if (!uso) return;
  SeekrQuotaUso q;
  seekrquota_uso((long long)time(NULL), &q);
  pthread_mutex_lock(&trava);
  *uso = (SeekrUso){q.usadas, q.restantes, q.limite, q.relogioAtrasado,
                   q.persistente, q.reinicioUtc, retryUtc, ultimoHttp, ultimaLatencia,
                   chavePropria};
  pthread_mutex_unlock(&trava);
}
void seekr_tentar_novamente(void) {
  pthread_mutex_lock(&trava);
  if (estado != SEEKR_BUSCANDO && (long long)time(NULL) >= retryUtc) pedImdb[0] = 0;
  pthread_mutex_unlock(&trava);
}

static void soltarFolha(void) {
  if (folhaSup) SDL_FreeSurface(folhaSup);
  folhaSup = NULL; folhaIdx = -1;
}
static void soltarProntos(void) {
  int i;
  for (i = 0; i < SK_SLOTS; i++) { free(prontos[i].px); prontos[i].px = NULL; }
}
static void soltarJpg(void) { free(jpgBytes); jpgBytes = NULL; jpgN = 0; jpgIdx = -1; }

// Chamar com a trava.
static void limparTudo(void) {
  seekr_vtt_liberar(&vtt);
  soltarFolha(); soltarJpg(); soltarProntos();
  pedImdb[0] = 0;
  expiraUtc = 0;
}

void seekr_ocioso(void) {
  pthread_mutex_lock(&trava);
  if (!emVoo) soltarFolha();
  pthread_mutex_unlock(&trava);
}
void seekr_definir_ajuste_ms(long ms) {
  pthread_mutex_lock(&trava); ajusteMs = ms; pthread_mutex_unlock(&trava);
}

void seekr_desligar(void) {
  pthread_mutex_lock(&trava);
  geracao++; estado = SEEKR_DESLIGADO;
  limparTudo();
  pthread_mutex_unlock(&trava);
}

typedef struct {
  char url[512], cab[160], key[96], imdb[64];
  int t, e;
  long dur;
  unsigned g;
} Pedido;
#define SK_LOOKUPS 8
static Pedido *consultas[SK_LOOKUPS];

static int pedidoAtual(Pedido *p) {
  int atual;
  pthread_mutex_lock(&trava); atual = p->g == geracao; pthread_mutex_unlock(&trava);
  return atual;
}
static long long urlExpira(const char *url, long long agora) {
  const char *p = strstr(url, "?exp=");
  if (!p) p = strstr(url, "&exp=");
  if (p) {
    char *fim;
    long long exp = strtoll(p + 5, &fim, 10);
    if (fim > p + 5 && (*fim == 0 || *fim == '&' || *fim == '#') && exp > 0)
      return exp;
  }
  // Unknown TTL is kept only in memory and capped conservatively.
  return agora + 300;
}

static void *consultar(void *u) {
  Pedido *p = u;
  const char *cabs[2] = { p->cab, NULL };
  int st = 0, n = 0, novo = SEEKR_SEM_PREVIA, tentativas = 0;
  char vttUrl[2300];
  long long expira = 0, repetir = 0;
  Uint32 inicio = SDL_GetTicks();
  SeekrVtt v; memset(&v, 0, sizeof v);
  // At most one bounded transport/5xx or expired-signature retry. Every
  // /sprites attempt independently reserves BEFORE dispatching.
  for (int tentativa = 0; tentativa < 2 && pedidoAtual(p); tentativa++) {
    pthread_mutex_lock(&trava);
    long long freio = !strcmp(chave, p->key) ? retryUtc : 0;
    pthread_mutex_unlock(&trava);
    long long agora = (long long)time(NULL);
    if (freio > agora) { novo = SEEKR_LIMITE_PROVEDOR; repetir = freio; break; }
    pthread_mutex_lock(&trava); int propria = chavePropria; pthread_mutex_unlock(&trava);
    int reserva = propria ? SEEKR_QUOTA_OK : seekrquota_reservar(agora);
    if (reserva != SEEKR_QUOTA_OK) {
      novo = reserva == SEEKR_QUOTA_LIMITE ? SEEKR_LIMITE_LOCAL :
             reserva == SEEKR_QUOTA_RELOGIO ? SEEKR_RELOGIO_INDISPONIVEL :
             SEEKR_ARMAZENAMENTO_INDISPONIVEL;
      if (reserva != SEEKR_QUOTA_LIMITE) repetir = agora + 30;
      break;
    }
    if (!pedidoAtual(p)) break; // Reserved calls are not refunded after cancellation.
    int depois = 0;
    tentativas++;
    novo = SEEKR_SEM_PREVIA; repetir = 0;
    char *corpo = rede_baixar_st_retry(p->url, 12, cabs, &st, &depois);
    if (st == 401 || st == 403) novo = SEEKR_CHAVE_RECUSADA;
    else if (st == 429) {
      novo = SEEKR_LIMITE_PROVEDOR;
      repetir = (long long)time(NULL) + (depois > 0 ? depois : 60);
    } else if (st == 0 || st >= 500 || (st == 200 && !corpo)) {
      novo = SEEKR_REDE_INDISPONIVEL;
      repetir = agora + 30;
      free(corpo);
      if (!tentativa && pedidoAtual(p)) { SDL_Delay(200 + SDL_GetTicks() % 100); continue; }
      break;
    } else if (st == 200 && corpo && seekr_ler_lookup(corpo, vttUrl, sizeof vttUrl - 8)) {
      expira = urlExpira(vttUrl, agora);
      if (expira <= agora) {
        free(corpo);
        if (!tentativa) continue;
        break;
      }
      // Signed VTT/tiles never receive the API key and never reserve quota.
      strcat(vttUrl, strchr(vttUrl, '?') ? "&st=1" : "?st=1");
      int vttSt = 0;
      char *txt = rede_baixar_st(vttUrl, 15, NULL, &vttSt);
      if (vttSt == 403) {
        free(corpo); free(txt);
        if (!tentativa) continue; // Fresh signed URLs cost another lookup.
        break;
      }
      n = vttSt == 200 && txt ? seekr_vtt_ler(txt, &v) : 0;
      if (n > 0) {
        for (int i = 0; i < v.nFolhas; i++)
          if (urlExpira(v.folhas[i], agora) < expira) expira = urlExpira(v.folhas[i], agora);
        novo = expira > agora ? SEEKR_PRONTO : SEEKR_SEM_PREVIA;
      } else if (vttSt == 0 || vttSt >= 500) {
        novo = SEEKR_REDE_INDISPONIVEL; repetir = agora + 30;
      }
      free(txt);
    }
    free(corpo);
    break;
  }
  SeekrQuotaUso uso;
  seekrquota_uso((long long)time(NULL), &uso);
  unsigned latencia = SDL_GetTicks() - inicio;
  // Stable English events; never print keys, title IDs or signed URLs.
  const char *razao = novo == SEEKR_LIMITE_LOCAL ? "device_daily_limit" :
                     novo == SEEKR_LIMITE_PROVEDOR ? "provider_rate_limit" :
                     novo == SEEKR_CHAVE_RECUSADA ? "key_rejected" :
                     novo == SEEKR_REDE_INDISPONIVEL ? "network" :
                     novo == SEEKR_ARMAZENAMENTO_INDISPONIVEL ? "storage" :
                     novo == SEEKR_RELOGIO_INDISPONIVEL ? "clock_unset" :
                     novo == SEEKR_SEM_PREVIA ? "no_preview" : "ready";
  printf("[seekr] lookup result=%s used=%d limit=%d\n", razao, uso.usadas, chavePropria ? 0 : SEEKR_QUOTA_DIA);
  printf("[seekr] lookup http=%d state=%d attempts=%d cues=%d sheets=%d used=%d limit=%d latency_ms=%u\n",
         st, novo, tentativas, n, v.nFolhas, uso.usadas, chavePropria ? 0 : SEEKR_QUOTA_DIA, latencia);
  fflush(stdout);
  pthread_mutex_lock(&trava);
  for (int i = 0; i < SK_LOOKUPS; i++) if (consultas[i] == p) consultas[i] = NULL;
  if (st == 429 && !strcmp(chave, p->key) && repetir > retryUtc) retryUtc = repetir;
  if (p->g == geracao) {
    seekr_vtt_liberar(&vtt); soltarFolha(); soltarJpg();
    vtt = v; memset(&v, 0, sizeof v);
    estado = novo;
    expiraUtc = novo == SEEKR_PRONTO ? expira : 0;
    long long validade = expiraUtc - (long long)time(NULL);
    if (validade < 0) validade = 0;
    if (validade > 21600) validade = 21600;
    expiraTick = SDL_GetTicks() + (Uint32)(validade * 1000);
    retryUtc = repetir;
    ultimoHttp = st; ultimaLatencia = latencia;
  }
  pthread_mutex_unlock(&trava);
  seekr_vtt_liberar(&v);
  free(p);
  return NULL;
}

void seekr_pedir(const char *imdb, int t, int e, long durMs) {
  Pedido *p; pthread_t fio;
  char url[512];
  if (!seekr_url_lookup(url, sizeof url, imdb, t, e, durMs)) { seekr_desligar(); return; }
  pthread_mutex_lock(&trava);
  if (!chave[0]) { pthread_mutex_unlock(&trava); return; }
  // Mesma combinacao: nada a fazer. A duracao entra com folga de 2 s — o
  // pipeline refina o numero nos primeiros segundos e cada refino seria uma
  // consulta a mais da cota.
  long long agora = (long long)time(NULL);
  int mesmo = !strcmp(pedImdb, imdb) && pedT == t && pedE == e && labs(pedDur - durMs) < 2000;
  if (retryUtc > agora) { pthread_mutex_unlock(&trava); return; }
  if (mesmo) {
    if (estado == SEEKR_BUSCANDO || estado == SEEKR_CHAVE_RECUSADA || estado == SEEKR_SEM_PREVIA ||
        (estado == SEEKR_PRONTO && expiraUtc > agora &&
         (Sint32)(expiraTick - SDL_GetTicks()) > 0)) { pthread_mutex_unlock(&trava); return; }
    if (estado == SEEKR_LIMITE_LOCAL) {
      SeekrQuotaUso uso; seekrquota_uso(agora, &uso);
      if (!uso.restantes) { pthread_mutex_unlock(&trava); return; }
    }
  }
  int livre = -1;
  Pedido *igual = NULL;
  for (int i = 0; i < SK_LOOKUPS; i++) {
    Pedido *q = consultas[i];
    if (!q) { if (livre < 0) livre = i; continue; }
    if (!strcmp(q->key, chave) && !strcmp(q->imdb, imdb) && q->t == t && q->e == e &&
        labs(q->dur - durMs) < 2000) igual = q;
  }
  if (igual) {
    snprintf(pedImdb, sizeof pedImdb, "%s", imdb); pedT = t; pedE = e; pedDur = durMs;
    geracao++; igual->g = geracao; estado = SEEKR_BUSCANDO;
    seekr_vtt_liberar(&vtt); soltarFolha(); soltarJpg(); soltarProntos();
    pthread_mutex_unlock(&trava);
    return;
  }
  if (livre < 0) { pthread_mutex_unlock(&trava); return; }
  p = calloc(1, sizeof *p);
  if (!p) { pthread_mutex_unlock(&trava); return; }
  snprintf(pedImdb, sizeof pedImdb, "%s", imdb); pedT = t; pedE = e; pedDur = durMs;
  geracao++; estado = SEEKR_BUSCANDO;
  seekr_vtt_liberar(&vtt); soltarFolha(); soltarJpg(); soltarProntos();
  snprintf(p->url, sizeof p->url, "%s", url);
  snprintf(p->cab, sizeof p->cab, "X-API-Key: %s", chave);
  snprintf(p->key, sizeof p->key, "%s", chave);
  snprintf(p->imdb, sizeof p->imdb, "%s", imdb);
  p->t = t; p->e = e; p->dur = durMs;
  p->g = geracao;
  consultas[livre] = p;
  pthread_mutex_unlock(&trava);
  if (pthread_create(&fio, NULL, consultar, p) == 0) pthread_detach(fio);
  else {
    pthread_mutex_lock(&trava);
    for (int i = 0; i < SK_LOOKUPS; i++) if (consultas[i] == p) consultas[i] = NULL;
    if (p->g == geracao) { estado = SEEKR_REDE_INDISPONIVEL; retryUtc = agora + 10; }
    pthread_mutex_unlock(&trava);
    free(p);
  }
}

// --- recorte -----------------------------------------------------------------

static SDL_Surface *decodificar(const unsigned char *b, long n) {
  SDL_Surface *s, *c;
  int w0 = 0, h0 = 0;
  // Largura pedida enorme = sem reducao de DCT: o recorte precisa do pixel
  // inteiro (o quadro ja e pequeno, 320x180).
  s = jpeg_rapido_carregar_mem(b, (size_t)n, 1 << 20, &w0, &h0);
#ifndef __EMSCRIPTEN__
  if (!s) { SDL_RWops *rw = SDL_RWFromConstMem(b, (int)n); s = rw ? IMG_Load_RW(rw, 1) : NULL; }
#endif
  if (!s) return NULL;
  if (s->format->format == SDL_PIXELFORMAT_ABGR8888) return s;
  c = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ABGR8888, 0);
  SDL_FreeSurface(s);
  return c;
}

typedef struct { char url[2300]; int folha, cue, x, y, w, h; unsigned g; } Recorte;

static char *baixarFolha(const char *url, long *n, int *http) {
  RedeMedida medida = {0};
  char *bytes = rede_baixar_bin_medido_controle(url, 15, NULL, NULL, n, &medida);
  if (http) *http = medida.status;
  return bytes;
}

static void *recortar(void *u) {
  Recorte *r = u;
  SDL_Surface *s = NULL;
  unsigned char *px = NULL;
  int precisa;
  pthread_mutex_lock(&trava);
  precisa = !(folhaSup && folhaIdx == r->folha && folhaG == r->g);
  pthread_mutex_unlock(&trava);
  if (precisa) {
    long n = 0;
    char *b = NULL;
    Uint32 t0 = SDL_GetTicks();
    int baixou = 0;
    // O JPEG desta folha ja esta guardado? Entao so decodifica.
    pthread_mutex_lock(&trava);
    if (jpgBytes && jpgIdx == r->folha && jpgG == r->g && (b = malloc((size_t)jpgN)) != NULL) {
      memcpy(b, jpgBytes, (size_t)jpgN); n = jpgN;
    }
    pthread_mutex_unlock(&trava);
    int http = 0;
    if (!b) { b = baixarFolha(r->url, &n, &http); baixou = 1; }   // assinada, sem chave
    s = b && n > 0 ? decodificar((unsigned char *)b, n) : NULL;
    printf("[seekr] sheet index=%d width=%d height=%d kb=%ld source=%s latency_ms=%u\n",
           r->folha, s ? s->w : 0, s ? s->h : 0, n / 1024,
           baixou ? "network" : "cache", SDL_GetTicks() - t0);
    fflush(stdout);
    pthread_mutex_lock(&trava);
    if (!s && baixou && r->g == geracao) {
      if (http == 403) expiraUtc = 0; // Next player tick refreshes through quota.
      else if (http == 0 || http >= 500) {
        estado = SEEKR_REDE_INDISPONIVEL; retryUtc = (long long)time(NULL) + 30;
      }
    }
    if (s && r->g == geracao) {
      soltarFolha(); folhaSup = s; folhaIdx = r->folha; folhaG = r->g;
      if (baixou) { soltarJpg(); jpgBytes = b; jpgN = n; jpgIdx = r->folha; jpgG = r->g; b = NULL; }
    } else if (s) { SDL_FreeSurface(s); s = NULL; }
    pthread_mutex_unlock(&trava);
    free(b);
  }
  pthread_mutex_lock(&trava);
  s = folhaSup;
  // Recorta DENTRO da trava: a folha pode ser trocada por outro pedido.
  if (s && folhaIdx == r->folha && r->g == geracao && r->x + r->w <= s->w && r->y + r->h <= s->h &&
      (px = malloc((size_t)r->w * (size_t)r->h * 4u)) != NULL) {
    int y, k, livre = 0;
    if (SDL_MUSTLOCK(s)) SDL_LockSurface(s);
    for (y = 0; y < r->h; y++)
      memcpy(px + (size_t)y * (size_t)r->w * 4u,
             (const unsigned char *)s->pixels + (size_t)(r->y + y) * (size_t)s->pitch + (size_t)r->x * 4u,
             (size_t)r->w * 4u);
    if (SDL_MUSTLOCK(s)) SDL_UnlockSurface(s);
    for (k = 0; k < SK_SLOTS; k++) if (!prontos[k].px) { livre = k; break; }
    if (k == SK_SLOTS) livre = 0;
    free(prontos[livre].px);
    prontos[livre] = (Pronto){ px, r->w, r->h, r->cue, r->g };
  }
  emVoo = 0;
  pthread_mutex_unlock(&trava);
  free(r);
  return NULL;
}

static int slotDe(int cue, unsigned g) {
  int i;
  for (i = 0; i < SK_SLOTS; i++) if (slots[i].tex && slots[i].cue == cue && slots[i].g == g) return i;
  return -1;
}
static int slotLivre(const int *proteger, int n) {
  int i, k, melhor = -1;
  for (i = 0; i < SK_SLOTS; i++) {
    int prot = 0;
    for (k = 0; k < n; k++) if (slots[i].cue == proteger[k]) prot = 1;
    if (prot && slots[i].tex) continue;
    if (melhor < 0 || slots[i].uso < slots[melhor].uso) melhor = i;
  }
  return melhor < 0 ? 0 : melhor;
}

// 1 quando a atual devolvida e a ULTIMA USADA no lugar da que ainda chega.
static int quadroVelho;
int seekr_quadro_velho(void) { return quadroVelho; }
#ifdef NV_SHOT_HOOKS
// Capturas: quadros fixos (anterior, atual, seguinte), sem rede nem cota.
static int shotVelho;
static GLuint shotTex[3];
static double shotCue[3];
void seekr_shot(int est, const GLuint *t, const double *cue, int velho) {
  int k;
  shotEstado = est; shotVelho = velho;
  for (k = 0; k < 3; k++) { shotTex[k] = t ? t[k] : 0; shotCue[k] = cue ? cue[k] : -1.0; }
}
#endif
int seekr_quadros(double posSeg, int n, GLuint *texs, double *cueSeg) {
  int quer[3], nq = 0, i, k, achou = 0;
  Recorte *pedir = NULL;
  Pronto pr[SK_SLOTS];
  unsigned g;
  if (n < 1) return 0;
  if (n > 3) n = 3;
#ifdef NV_SHOT_HOOKS
  if (shotEstado) {
    (void)posSeg;
    for (k = 0; k < n; k++) {
      int o = n == 1 ? 1 : k;
      texs[k] = shotTex[o];
      if (cueSeg) cueSeg[k] = shotCue[o];
    }
    quadroVelho = shotVelho;
    return texs[n == 1 ? 0 : 1] != 0;
  }
#endif
  for (k = 0; k < n; k++) { texs[k] = 0; if (cueSeg) cueSeg[k] = -1.0; }
  pthread_mutex_lock(&trava);
  g = geracao;
  memcpy(pr, prontos, sizeof pr);
  memset(prontos, 0, sizeof prontos);
  if (estado != SEEKR_PRONTO || expiraUtc <= (long long)time(NULL) ||
      (Sint32)(expiraTick - SDL_GetTicks()) <= 0) {
    pthread_mutex_unlock(&trava); for (k = 0; k < SK_SLOTS; k++) free(pr[k].px); return 0;
  }
  i = seekr_vtt_cue(&vtt, (long)(posSeg * 1000.0) + ajusteMs);
  if (i >= 0) {
    // Com fita: anterior, atual, seguinte (as que existem). A ATUAL vem
    // primeiro na fila de pedidos — e a que a pessoa esta olhando.
    if (n == 1) quer[nq++] = i;
    else {
      quer[nq++] = i;
      if (i > 0) quer[nq++] = i - 1;
      if (i + 1 < vtt.nCues) quer[nq++] = i + 1;
    }
    for (k = 0; k < nq; k++) {
      int c = quer[k], ja = slotDe(c, g), j, pend = 0;
      for (j = 0; j < SK_SLOTS; j++) if (pr[j].px && pr[j].g == g && pr[j].cue == c) pend = 1;
      if (ja < 0 && !pend && !emVoo && !pedir) {
        const SeekrCue *q = &vtt.cues[c];
        pedir = calloc(1, sizeof *pedir);
        if (pedir) {
          snprintf(pedir->url, sizeof pedir->url, "%s", vtt.folhas[q->folha]);
          pedir->folha = q->folha; pedir->cue = c;
          pedir->x = q->x; pedir->y = q->y; pedir->w = q->w; pedir->h = q->h;
          pedir->g = g; emVoo = 1;
        }
      }
    }
    if (cueSeg) {
      // Ordem de SAIDA: anterior, atual, seguinte (n == 3) ou so a atual.
      if (n == 1) cueSeg[0] = vtt.cues[i].ini / 1000.0;
      else {
        cueSeg[0] = i > 0 ? vtt.cues[i - 1].ini / 1000.0 : -1.0;
        cueSeg[1] = vtt.cues[i].ini / 1000.0;
        cueSeg[2] = i + 1 < vtt.nCues ? vtt.cues[i + 1].ini / 1000.0 : -1.0;
      }
    }
  }
  pthread_mutex_unlock(&trava);
  if (pedir) {
    pthread_t fio;
    if (pthread_create(&fio, NULL, recortar, pedir) == 0) pthread_detach(fio);
    else { free(pedir); pthread_mutex_lock(&trava); emVoo = 0; pthread_mutex_unlock(&trava); }
  }
  // Upload do que ficou pronto.
  for (k = 0; k < SK_SLOTS; k++) {
    if (!pr[k].px) continue;
    if (pr[k].g == g && slotDe(pr[k].cue, g) < 0) {
      int sl = slotLivre(quer, nq);
      if (!slots[sl].tex) glGenTextures(1, &slots[sl].tex);
      glBindTexture(GL_TEXTURE_2D, slots[sl].tex);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pr[k].w, pr[k].h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pr[k].px);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      gfx_tex_esquecer(0);
      slots[sl].cue = pr[k].cue; slots[sl].g = pr[k].g;
    }
    free(pr[k].px);
  }
  if (i < 0) return 0;
  relogioUso++;
  {
    int ordem[3], no = 0;
    if (n == 1) ordem[no++] = i;
    else { ordem[no++] = i - 1; ordem[no++] = i; ordem[no++] = i + 1; }
    for (k = 0; k < no; k++) {
      int sl = ordem[k] >= 0 ? slotDe(ordem[k], g) : -1;
      if (sl >= 0) { texs[k] = slots[sl].tex; slots[sl].uso = relogioUso; if (ordem[k] == i) achou = 1; }
    }
    // A atual ainda nao chegou: a ultima usada do MESMO titulo no lugar dela
    // (melhor que piscar vazio entre duas cues).
    quadroVelho = 0;
    if (!achou) {
      int m = -1, c = n == 1 ? 0 : 1;
      for (k = 0; k < SK_SLOTS; k++)
        if (slots[k].tex && slots[k].g == g && (m < 0 || slots[k].uso > slots[m].uso)) m = k;
      if (m >= 0) { texs[c] = slots[m].tex; achou = 1; quadroVelho = 1; }
    }
  }
  return achou;
}

GLuint seekr_quadro(double posSeg, double *cueSeg) {
  GLuint t = 0;
  seekr_quadros(posSeg, 1, &t, cueSeg);
  return t;
}

int seekr_validar(const char *c) {
  char cab[160]; const char *cabs[2] = { cab, NULL };
  int st = 0, r;
  char *corpo;
  if (!c || !*c) return 0;
  snprintf(cab, sizeof cab, "X-API-Key: %s", c);
  corpo = rede_baixar_st("https://api.seekr.tv/v1/keys/validate", 10, cabs, &st);
  if (st == 401 || st == 403) r = 0;
  else if (!corpo || st == 0) r = -1;
  else if (st >= 500 || st == 429) r = -1;
  else {
    const char *v = strstr(corpo, "\"valid\"");
    v = v ? strchr(v, ':') : NULL;
    if (v) { v++; while (isspace((unsigned char)*v)) v++; }
    r = v && !strncmp(v, "true", 4) ? 1 : 0;
  }
  printf("[seekr] validate http=%d result=%d\n", st, r); fflush(stdout);
  free(corpo);
  return r;
}
