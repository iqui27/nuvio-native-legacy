// Implementacao de farolsinal.h — issue 38 (PLANO-GOSTO 1.3 e 2.2).
//
// O desenho inteiro gira em volta de uma ideia so: o que a tabela 2.2 paga e
// por VEZ (uma entrada do diario, um gesto, uma saida do player), e cada vez
// creditada fica num registro (`Cred`) com a chave da ficha usada e o delta
// que entrou. Desfazer e achar o registro e devolver o delta com sinal
// trocado — apagar entrada do diario, desmarcar visto, tirar do salvos e
// desfazer "nao e para mim" passam pelo mesmo caminho. Pendencia e o mesmo
// registro ainda sem ficha; quando ela chega o delta esperado aplica na
// hora, sem recomputar nada.
//
// Identidades (todas hash, nunca texto):
//   idh = fnv64(id)                    — a obra ("desmarcar o titulo inteiro")
//   hb  = fnv64(id | T | E | dom)      — a vez, no dominio do sinal
//   eh  = hb ^ mix(quando)             — a entrega pontual (dedup de evento)
// K_VEZ e K_GESTO dividem o dominio das vezes assistidas: o eco que a conta
// devolve de um gesto manual (ou a entrada do player que chega DEPOIS do
// check otimista) casa pela hb e vira ajuste, nunca segundo credito.
#include "farolsinal.h"
#include "dados.h"
#include "dormiu.h"
#include "farolanel.h"
#include "mapa.h"
#include "perfis.h"
#include "progresso.h"
#include "salvos.h"
#include "visto.h"
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef NV_UM_FIO
#include "fio1.h"
#endif

// Dominios de sinal: o "tipo" do credito, dobrado na hb para que canais de
// desfazer nao se cruzem (tirar dos salvos nao apaga o abandono etc.).
enum {
  K_VEZ,        // entrada do diario (o caminho central)
  K_GESTO,      // marcou assistido na mao (mesma hb da vez -> fusao no eco)
  K_SAIDA,      // abandono cedo na saida do player (nao desfaz com "nao vi")
  K_SALVO,
  K_TIROU,
  K_INTERESSE,
  K_RECUSA,
  K_MAIS,
  K_TEMP,       // temporada completa (uma vez por temporada)
  K_LARGADA     // serie largada 21 dias (uma vez por titulo)
};

#define FSIN_PEND_N   32    // fila de pendencias (secao 2.1: "fila de 32")
#define FSIN_CRED_N   512   // vezes creditadas recentes (por perfil, em disco)
#define FSIN_OBRA_N   1024  // chaves de ficha ja contadas em `titulos`
#define FSIN_PED_N    64    // pedidos de ficha para a fila lenta
#define FSIN_ALIAS_N  64    // chave pedida -> chave real da ficha
#define FSIN_ORI_N    64    // origens de Play recentes
#define FSIN_TEMP_N   8     // temporadas esperando conferida de "completa"
#define FSIN_ABAND_MIN_S  60.0     // parou antes de 1 min: nem sinal
#define FSIN_ABAND_PCT    20.0     // largou antes de 20%: abandono cedo
#define FSIN_JANELA_FALHA (5u*60u*1000u)   // ms; falha de fonte anula abandono
#define FSIN_SONO_JANELA  (2*3600)         // suspeita de sono vale 2 h
#define FSIN_LARGADA_MS   (21LL*86400LL*1000LL)   // serie largada (secao 1.3)
#define FSIN_PEND_DIAS    30
#define FSIN_GRAVA_S      60
#define DIA_EPOCA         1767225600LL    // 1/1/2026 UTC = dia 0 do modelo
#define FSIN_VEZ_MAX      512             // entregas por fatia de varredura
#define FSIN_SEQ_TODA     INT_MAX         // curSeq sentinela: grupo do cursor
                                          // consumido inteiro (cursor FS 1)

typedef struct {
  uint64_t idh, hb, eh;   // obra / vez / entrega (hashes do topo)
  long long quando;       // epoch s da vez (decai junto; tambem da o dia)
  int64_t  chave;         // chave da ficha usada no credito (ja real, se veio)
  int32_t  delta;         // o que esta valendo no modelo AGORA (Q8.8)
  int16_t  faixa;         // faixaHora usada (para desfazer na mesma celula)
  uint8_t  kind;          // K_*
} Cred;

typedef struct {
  Cred     c;             // o credito esperando ficha (eh/kind identificam a vez)
  char     id[DIARIO_ID_MAX];
  long     tmdb;
  uint8_t  serie, tent;   // tent = buscas ja feitas (teto 2; depois solta)
  uint8_t  devolve;       // 1 = divida de estorno (o delta fica POSITIVO:
                          // sinais de tabela ja nascem negativos — o sinal
                          // nao pode ser o marcador)
} Pend;

typedef struct {
  int64_t  chave;         // a chave que as pendencias usam
  char     id[DIARIO_ID_MAX];
  long     tmdb;
  uint8_t  serie;
} Ped;

typedef struct { int64_t de, para; } Ali;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  condPed = PTHREAD_COND_INITIALIZER;
#define TRAVA()  pthread_mutex_lock(&trava)
#define DTRAVA() pthread_mutex_unlock(&trava)

static struct {
  int    ligado;                    // ouvintes registrados / modulo acordado
  int    perfil;                    // 0 = nenhum modelo carregado
  int    rec;                       // "Recomendar nesta TV"
  long long cur;                    // cursor da varredura (epoch s)
  int    curSeq;                    // consumidas dentro do grupo `quando`==cur
  int    varrer;                    // varredura de diario pendente
  int    varGer;                    // geracao do cursor (RECARREGADO/limpeza)
  int    largadaVarrida;            // serie-largada ja conferida nesta carga
  FarolGosto g;
  FarolFichaCache fc;
  Cred   cred[FSIN_CRED_N];   int nCred;
  Pend   pend[FSIN_PEND_N];   int nPend;
  int64_t obra[FSIN_OBRA_N];  int nObra, obraProx;
  Ali    ali[FSIN_ALIAS_N];   int nAli;
  Ped    ped[FSIN_PED_N];     int nPed;
  struct { char id[DIARIO_ID_MAX]; short t; } tempChk[FSIN_TEMP_N];
  int    nTempChk;
  struct { uint32_t h; uint8_t o, dia; } ori[FSIN_ORI_N];
  int    nOri, oriProx;
  int    sujo;                      // modelo/estado com gravacao pendente
  int    fichaSujo;
  long long ultGrav;
  char   dir[768];
} S;

static FarolSinalBusca busca;
static int      fioVivo, fioMorre;
static pthread_t fioId;
// O fio de tras do passo: conferidas de temporada, varredura de largada e
// gravacoes — tudo o que a r2 viu parar o quadro (correcao FAROL38C).
static pthread_cond_t condTrab = PTHREAD_COND_INITIALIZER;
static int      fioTrabVivo, fioTrabOcup;
static pthread_t fioTrabId;

static long long (*f_agora_s)(void);
static uint32_t  (*f_agora_ms)(void);
static long long agoraS(void) { return f_agora_s ? f_agora_s() : (long long)time(NULL); }
static uint32_t  agoraMs(void) {
  if (f_agora_ms) return f_agora_ms();
  struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

// ---------------------------------------------------------------- hash/dia --
static uint64_t mix64(uint64_t x) {
  x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33;
  x *= 0xc4ceb9fe1a85ec53ULL; x ^= x >> 33;
  return x;
}
static uint64_t fnv(const char *s) {
  uint64_t h = 1469598103934665603ULL;
  if (s) while (*s) { h ^= (unsigned char)*s++; h *= 1099511628211ULL; }
  return h;
}
static uint64_t h1(const char *id) { return fnv(id); }
static uint64_t h3(const char *id, int t, int e) {
  return fnv(id) ^ mix64((uint64_t)(unsigned)t * 0x9E3779B1u +
                         (uint64_t)(unsigned)e);
}
// hb de um sinal avulso: a obra no dominio `kind` (+ um inteiro, quando o
// sinal vale por temporada/dia).
static uint64_t hDom(const char *id, int kind, int n) {
  return fnv(id) ^ mix64((uint64_t)kind * 0xD6E8FEB86659FD93ULL + (unsigned)n);
}
static int diaDe(long long quando) {
  long long d = (quando - DIA_EPOCA) / 86400;
  if (d < 0) d = 0;
  if (d > 65535) d = 65535;
  return (int)d;
}
// "dia_semana*24 + hora" (DiarioEntrada.hora) -> faixa 0-7 do modelo:
// fds (dom/sab) * 4 + [manha, tarde, noite, madrugada].
static int faixaDeHora(int hora167) {
  int wd, h, p;
  if (hora167 < 0 || hora167 > 167) return -1;
  wd = hora167 / 24; h = hora167 % 24;
  p = h < 6 ? 3 : h < 12 ? 0 : h < 18 ? 1 : 2;
  return ((wd == 0 || wd == 6) ? 4 : 0) + p;
}
static int faixaAgora(long long quando) {
  time_t t = (time_t)quando; struct tm tmv;
  localtime_r(&t, &tmv);
  return faixaDeHora(tmv.tm_wday * 24 + tmv.tm_hour);
}

// ----------------------------------------------------------------- caminho --
static int caminho(const char *nome, char *dst, size_t cap) {
  int n = snprintf(dst, cap, "%s/%s", S.dir, nome);
  return n > 0 && (size_t)n < cap;
}
static void nomeEstado(int perfil, char *dst, size_t cap) {
  snprintf(dst, cap, "farol-sinal-p%d.txt", perfil);
}
static void nomeGosto(int perfil, char *dst, size_t cap) {
  farolgosto_nome((uint16_t)perfil, dst, cap);
}

// ------------------------------------------------------------------ modelo --
// Grava farol-gosto-p<n>.txt com o comprimento EXATO que escrever devolveu
// (ressalva do lote: nada de strlen nem "%s" — o texto nao e fechado aqui).
static void gravaModelo(void) {   // com a trava tomada
  static char buf[FAROL_GOSTO_TXT_MAX + 1];
  char cam[1100], tmp[1200], nome[64];
  size_t n;
  FILE *f;
  if (S.perfil < 1) return;
  nomeGosto(S.perfil, nome, sizeof nome);
  if (!caminho(nome, cam, sizeof cam)) return;
  n = farolgosto_escrever(&S.g, buf, sizeof buf - 1);
  if (!n) return;                      // nao coube: nao grava por cima
  snprintf(tmp, sizeof tmp, "%s.tmp", cam);
  f = fopen(tmp, "w");
  if (!f) return;
  if (fwrite(buf, 1, n, f) != n) { fclose(f); remove(tmp); return; }
  fclose(f);
  rename(tmp, cam);
}
static void leModelo(void) {           // com a trava tomada
  static char buf[FAROL_GOSTO_TXT_MAX + 8];
  char cam[1100], nome[64];
  FILE *f; size_t n;
  farolgosto_iniciar(&S.g, (uint16_t)S.perfil);
  nomeGosto(S.perfil, nome, sizeof nome);
  if (!caminho(nome, cam, sizeof cam)) return;
  f = fopen(cam, "r");
  if (!f) return;
  n = fread(buf, 1, sizeof buf - 1, f);
  fclose(f);
  if (n) farolgosto_ler(&S.g, buf, n); // tamanho explicito, arquivo nao tem NUL
}

// ------------------------------------------------------------- conjuntos ----
static int64_t aliasPara(int64_t c) {
  int i;
  for (i = 0; i < S.nAli; i++) if (S.ali[i].de == c) return S.ali[i].para;
  return c;
}
static void aliasPoe(int64_t de, int64_t para) {
  int i;
  if (!de || !para || de == para) return;
  for (i = 0; i < S.nAli; i++)
    if (S.ali[i].de == de) { S.ali[i].para = para; break; }
  if (i >= S.nAli) {
    if (S.nAli < FSIN_ALIAS_N) S.ali[S.nAli].de = de,
                               S.ali[S.nAli].para = para, S.nAli++;
    else { S.ali[S.nAli - 1].de = de; S.ali[S.nAli - 1].para = para; }
  }
  // A reserva ja contada migra para a chave real: sem isso a mesma obra
  // contava duas vezes em `titulos` (correcao FAROL38B). Se as duas chaves
  // ja estavam no anel a reserva sai — o real fica como a unica reserva.
  for (i = 0; i < S.nObra; i++)
    if (S.obra[i] == de) {
      int j, dup = 0;
      for (j = 0; j < S.nObra; j++)
        if (j != i && S.obra[j] == para) { dup = 1; break; }
      if (dup) { S.obra[i] = S.obra[S.nObra - 1]; S.nObra--; }
      else S.obra[i] = para;
      break;
    }
  S.sujo = 1;
}
static int obraVista(int64_t chave) {
  int i;
  for (i = 0; i < S.nObra; i++) if (S.obra[i] == chave) return 1;
  return 0;
}
static void obraMarca(int64_t chave) {
  int i;
  if (obraVista(chave)) return;
  if (S.nObra < FSIN_OBRA_N) i = S.nObra++;
  else { i = S.obraProx; S.obraProx = (S.obraProx + 1) % FSIN_OBRA_N; }
  S.obra[i] = chave; S.sujo = 1;
}
static int achaEh(uint64_t eh) {  // vaga de cred pela entrega exata; -1 sem
  int i;
  for (i = 0; i < S.nCred; i++) if (S.cred[i].eh == eh) return i;
  return -1;
}
// A vez no dominio visto/gesto com o `quando` mais perto (a fusao do diario
// pode mexer alguns segundos; o limite e a mesma janela de 6 h).
static int achaVez(uint64_t hb, long long perto) {
  int i, ach = -1;
  long long melhor = DIARIO_JANELA_S + 1;
  for (i = 0; i < S.nCred; i++) {
    long long d;
    if (S.cred[i].hb != hb || S.cred[i].kind > K_GESTO) continue;
    d = S.cred[i].quando > perto ? S.cred[i].quando - perto
                               : perto - S.cred[i].quando;
    if (d < melhor) { melhor = d; ach = i; }
  }
  return ach;
}
static int achaHb(uint64_t hb) {  // qualquer dominio: "ja recebeu este sinal?"
  int i;
  for (i = 0; i < S.nCred; i++) if (S.cred[i].hb == hb) return i;
  return -1;
}
static void credPoe(const Cred *c) {
  int i;
  if (S.nCred < FSIN_CRED_N) i = S.nCred++;
  else {  // cheio: sai a mais velha por `quando`
    int j; i = 0;
    for (j = 1; j < S.nCred; j++) if (S.cred[j].quando < S.cred[i].quando) i = j;
  }
  S.cred[i] = *c;
}
static void credTira(int i) {
  if (i >= 0 && i < S.nCred) S.cred[i] = S.cred[--S.nCred];
}
// A vez (K_VEZ/K_GESTO) desta hb ja creditou? Sem janela — para conferir
// "episodio visto" dentro da trava, sem perguntar ao diario.
static int vezCredida(uint64_t hb) {
  int i;
  for (i = 0; i < S.nCred; i++)
    if (S.cred[i].hb == hb && S.cred[i].kind <= K_GESTO) return 1;
  return 0;
}
static void pendTira(int i) {
  if (i >= 0 && i < S.nPend) S.pend[i] = S.pend[--S.nPend];
}

// ------------------------------------------------------------------ ficha --
// A chave que um sinal pede: com TMDB a do detalhe; sem ele a reserva do
// proprio id — a MESMA conta de reservaDeId+farolficha_chave, entao um
// CatItem sem TMDB gera a mesma chave negativa da ficha que ele monta
// (ressalva do lote: catalogo e detalhe nao podem cobrar duas vezes).
static int64_t chaveObra(const char *id, int serie, long tmdb) {
  int64_t ff = serie ? FF_SERIE : FF_FILME;
  if (tmdb <= 0 && id && !strncmp(id, "tmdb:", 5)) tmdb = strtol(id + 5, NULL, 10);
  if (tmdb > 0) return (ff << 32) | (int64_t)(uint32_t)tmdb;
  if (!id || !id[0]) return 0;
  return -((ff << 32) | (int64_t)(0x40000000u | (mapa_hash_titulo(id) & 0x3fffffffu)));
}
// Tenta a ficha pela chave (ja traduzida por alias), depois pelo catalogo.
// Devolve a CHAVE REAL (a positiva do TMDB ou a reserva da ficha montada), ou
// 0 quando nao ha ficha ainda. Com a trava tomada — cache e arquivo so andam
// aqui dentro (ressalva: nada de leitura e escrita concorrentes).
static int64_t pegaFicha(int64_t chave, const char *id, int serie,
                         FarolFicha *f) {
  int idx;
  const CatItem *ci;
  int64_t real;
  (void)serie;   // a ficha do CatItem ja sabe o tipo dela
  if (farolficha_obter(&S.fc, chave, f))
    return farolficha_chave(f);
  if (id && id[0] && (idx = cat_indice_por_imdb(id)) >= 0 &&
      (ci = cat_item(idx)) != NULL && farolficha_ler_item(ci, f)) {
    real = farolficha_chave(f);
    // Cache ja tem ficha nesta chave (a detalhada do TMDB): NAO sobrescreve
    // com a reconstrucao pobre do catalogo (correcao FAROL38B).
    if (farolficha_obter(&S.fc, real, f)) {
      if (real != chave) aliasPoe(chave, real);
      return real;
    }
    farolficha_guardar(&S.fc, f);
    S.fichaSujo = 1;
    if (real != chave) aliasPoe(chave, real);
    return real;
  }
  return 0;
}

static void pedeFicha(int64_t chave, const char *id, int serie, long tmdb);

// Credito esperando ficha; com `devolve` e a devolucao tardia (estorno que
// faltou ficha) — dominio separado: uma intencao e uma divida do mesmo eh
// nao se confundem.
static void pendPoe(const Cred *c, const char *id, int serie, long tmdb,
                    int devolve) {
  int i;
  for (i = 0; i < S.nPend; i++)      // a mesma entrega nao entra duas vezes
    if (S.pend[i].c.eh == c->eh && S.pend[i].c.kind == c->kind &&
        S.pend[i].devolve == !!devolve) {
      S.pend[i].c.delta = c->delta; S.sujo = 1; return;
    }
  if (S.nPend >= FSIN_PEND_N) {  // teto 32: sai a mais velha (secao 2.1)
    int j; i = 0;
    for (j = 1; j < S.nPend; j++)
      if (S.pend[j].c.quando < S.pend[i].c.quando) i = j;
    pendTira(i);
  }
  i = S.nPend++;
  memset(&S.pend[i], 0, sizeof S.pend[i]);
  S.pend[i].c = *c;
  snprintf(S.pend[i].id, sizeof S.pend[i].id, "%s", id ? id : "");
  S.pend[i].tmdb = tmdb; S.pend[i].serie = (uint8_t)!!serie;
  S.pend[i].devolve = (uint8_t)!!devolve;
  S.sujo = 1;
  pedeFicha(c->chave, id, serie, tmdb);
}

// Converte a ficha da 36 para a ficha do modelo (os mesmos campos que a
// integracao de aplicar sempre montou).
static void montaGf(const FarolFicha *f, FarolGostoFicha *g) {
  memset(g, 0, sizeof *g);
  g->tmdb = f->tmdb;
  g->tipo = f->tipo == FF_SERIE ? FG_TIPO_SERIE : FG_TIPO_FILME;
  g->ano = (int16_t)f->ano;
  g->idioma = f->idioma;
  g->minutos = (int16_t)f->minutos;
  memcpy(g->genero, f->genero, sizeof g->genero);
  memcpy(g->pessoa, f->pessoa, sizeof g->pessoa);
  memcpy(g->tema, f->tema, sizeof g->tema);
}

// Converte e aplica; `obraNova` sai do anel de chaves (contrato do plano 3.4:
// quarenta episodios da mesma serie valem um titulo).
static void aplicar(const FarolFicha *f, int64_t real, int delta,
                    int dia, int faixa) {
  FarolGostoFicha g;
  int nova;
  montaGf(f, &g);
  nova = !obraVista(real);
  if (nova) obraMarca(real);
  farolgosto_aplicar(&S.g, &g, delta, (uint16_t)dia, faixa, nova);
}

// Devolve EXATAMENTE o que esta vez somou (correcao FAROL38B): por celula,
// depois[k] conta os creds vivos mais novos que ela na mesma celula — o
// estorno acha a saturacao com que ela entrou mesmo com creds ja despejados.
// So o cache fala: devolver nao remonta ficha do catalogo.
static void estornaCred(const FarolFicha *f, const Cred *c) {
  FarolGostoFicha g;
  int dims[FG_CEL_N], depois[FG_CEL_N], n, i;
  int32_t ids[FG_CEL_N];
  montaGf(f, &g);
  n = farolgosto_celulas(&g, dims, ids, FG_CEL_N);
  memset(depois, 0, sizeof depois);
  for (i = 0; i < S.nCred; i++) {
    const Cred *o = &S.cred[i];
    FarolFicha fo; FarolGostoFicha go;
    int dm[FG_CEL_N], m, a, b;
    int32_t ii[FG_CEL_N];
    int64_t ro;
    if (o->quando <= c->quando) continue;
    ro = aliasPara(o->chave);
    if (!ro || !farolficha_obter(&S.fc, ro, &fo)) continue;
    montaGf(&fo, &go);
    m = farolgosto_celulas(&go, dm, ii, FG_CEL_N);
    for (a = 0; a < m; a++)
      for (b = 0; b < n; b++)
        if (dm[a] == dims[b] && ii[a] == ids[b]) { depois[b]++; break; }
  }
  farolgosto_estornar(&S.g, &g, c->delta, depois,
                      (uint16_t)diaDe(c->quando), c->faixa);
}

// Solta as pendencias cuja chave (traduzida) e a ficha que acabou de entrar.
static void drenaPends(int64_t real) {
  int i;
  FarolFicha f;
  for (i = S.nPend - 1; i >= 0; i--) {
    Pend p;
    if (aliasPara(S.pend[i].c.chave) != real) continue;
    // Coleta desligada: a ficha em voo nao credita nada — a intencao cai;
    // a DIVIDA de estorno espera a coleta voltar, porque ela e correcao,
    // nao coleta (correcao FAROL38B).
    if (!S.rec) {
      if (!S.pend[i].devolve) { pendTira(i); S.sujo = 1; }
      continue;
    }
    if (!farolficha_obter(&S.fc, real, &f)) continue;
    p = S.pend[i]; pendTira(i);
    if (p.devolve) {   // devolucao tardia: estorno, nunca credito novo
      estornaCred(&f, &p.c);
      S.sujo = 1;
      continue;          // a devolucao nao vira Cred: a vez ja saiu do anel
    }
    aplicar(&f, real, p.c.delta, diaDe(p.c.quando), p.c.faixa);
    p.c.chave = real;
    credPoe(&p.c);
  }
}
// Depois de um credito direto, a pendencia da MESMA entrega nao pode ficar:
// senao a ficha que chegar depois a aplicaria de novo.
static void pendCancelaEntrega(uint64_t eh, int kind) {
  int i;
  for (i = S.nPend - 1; i >= 0; i--)
    if (S.pend[i].c.eh == eh && S.pend[i].c.kind == kind)
      { pendTira(i); S.sujo = 1; }
}

// O credito de uma vez: resolve a ficha (cache -> catalogo) ou pendencia +
// pedido para a fila lenta. Com a trava tomada.
static void credita(const char *id, int serie, long tmdb, const Cred *c) {
  int64_t real;
  FarolFicha f;
  Cred cc = *c;
  if (!cc.chave) return;                 // id vazio: sem ficha nem fila
  real = pegaFicha(aliasPara(cc.chave), id, serie, &f);
  if (!real) { pendPoe(&cc, id, serie, tmdb, 0); return; }
  if (real != cc.chave) aliasPoe(cc.chave, real);
  aplicar(&f, real, cc.delta, diaDe(cc.quando), cc.faixa);
  cc.chave = real;
  credPoe(&cc);
  pendCancelaEntrega(cc.eh, cc.kind);
  drenaPends(real);
  S.sujo = 1;
}

// Devolve a contribuicao gravada de um Cred — o caminho unico de "desfazer":
// a chave guardada busca a ficha pela mesma estrada do credito e o estorno
// sai celula a celula, sem residuo e sem evidencia a mais (correcao
// FAROL38B). Sem ficha por perto a devolucao vira pendencia DEVOLVE (o dia
// e a faixa originais vao junto, para sair da mesma celula onde entrou) —
// e divida de estorno NAO e cancelada pelas varreduras de intencao.
static void desfaz(Cred *c, const char *id, int serie, long tmdb) {
  int64_t real;
  FarolFicha f;
  real = pegaFicha(aliasPara(c->chave), id, serie, &f);
  if (real) { estornaCred(&f, c); S.sujo = 1; }
  else pendPoe(c, id, serie, tmdb, 1);
}

// ----------------------------------------------------------------- estado --
static void gravaEstado(void) {  // com a trava tomada
  char cam[1100], tmp[1200], nome[64];
  FILE *f; int i;
  if (S.perfil < 1) return;
  nomeEstado(S.perfil, nome, sizeof nome);
  if (!caminho(nome, cam, sizeof cam)) return;
  snprintf(tmp, sizeof tmp, "%s.tmp", cam);
  f = fopen(tmp, "w");
  if (!f) return;
  fprintf(f, "FS 2 %d %d %lld %d\n", S.perfil, S.rec, S.cur, S.curSeq);
  for (i = 0; i < S.nPend; i++) {
    const Pend *p = &S.pend[i];
    fprintf(f, "p %lld %llu %llu %llu %lld %d %d %u %u %u %u %ld %s\n",
            (long long)p->c.chave, (unsigned long long)p->c.idh,
            (unsigned long long)p->c.hb, (unsigned long long)p->c.eh,
            p->c.quando, (int)p->c.delta, (int)p->c.faixa,
            (unsigned)p->c.kind, (unsigned)p->tent, (unsigned)p->serie,
            (unsigned)p->devolve, p->tmdb, p->id);
  }
  for (i = 0; i < S.nCred; i++) {
    const Cred *c = &S.cred[i];
    fprintf(f, "c %llu %llu %llu %lld %lld %d %d %u\n",
            (unsigned long long)c->idh, (unsigned long long)c->hb,
            (unsigned long long)c->eh, c->quando, (long long)c->chave,
            (int)c->delta, (int)c->faixa, (unsigned)c->kind);
  }
  for (i = 0; i < S.nObra; i++) fprintf(f, "o %lld\n", (long long)S.obra[i]);
  for (i = 0; i < S.nAli; i++)
    fprintf(f, "a %lld %lld\n", (long long)S.ali[i].de, (long long)S.ali[i].para);
  for (i = 0; i < S.nOri; i++)
    fprintf(f, "i %u %u %u\n", (unsigned)S.ori[i].h, (unsigned)S.ori[i].o,
            (unsigned)S.ori[i].dia);
  fclose(f);
  rename(tmp, cam);
}
static void leEstado(void) {     // com a trava tomada
  char cam[1100], nome[64], linha[640];
  FILE *f;
  int i;
  nomeEstado(S.perfil, nome, sizeof nome);
  if (!caminho(nome, cam, sizeof cam)) return;
  f = fopen(cam, "r");
  if (!f) return;
  // A primeira linha TEM de ser o cabecalho "FS <ver> <perfil> <rec> <cur>
  // [<curSeq>]": versao desconhecida, perfil de outro ou arquivo
  // truncado/corrompido — recusa o arquivo INTEIRO (correcao FAROL38B: meio-
  // registro aplicava delta torto). FS 1 nao guardava a posicao dentro do
  // grupo de mesmo `quando`: o cursor exclusivo dele vale como "o grupo de
  // S.cur ja entrou inteiro" (correcao FAROL38C).
  if (!fgets(linha, sizeof linha, f)) { fclose(f); return; }
  {
    int p = 0, r = 0, sq = 0; long long c = 0; char resto[8] = "";
    if (sscanf(linha, "FS 2 %d %d %lld %d %7s", &p, &r, &c, &sq, resto) == 4 &&
        p == S.perfil && sq >= 0) {
      S.rec = !!r; S.cur = c; S.curSeq = sq;
    } else if (sscanf(linha, "FS 1 %d %d %lld %7s", &p, &r, &c, resto) == 3 &&
               p == S.perfil) {
      S.rec = !!r; S.cur = c; S.curSeq = FSIN_SEQ_TODA;
    } else { fclose(f); return; }
  }
  {
    // Qualquer registro malformado rejeita o arquivo INTEIRO, como o
    // cabecalho: um "p" truncado depois de um valido nao pode deixar metade
    // do estado de pe (correcao FAROL38C — ressalva da r2). Linha de tipo
    // desconhecido, ou alem do teto da fila, nao e corte: ignora.
    int mal = 0;
    while (fgets(linha, sizeof linha, f)) {
      if (linha[0] == 'p' && S.nPend < FSIN_PEND_N) {
        Pend *p = &S.pend[S.nPend];
        int d, fx, k, te, sr, dv;
        long long ch, q;
        unsigned long long ih, hb, eh;
        memset(p, 0, sizeof *p);
        // Registro de pendencia e TUDO ou nada: 13 campos e kind valido
        // (correcao FAROL38B: a linha cortada entrava com campo de lixo).
        if (sscanf(linha + 1,
                   "%lld %llu %llu %llu %lld %d %d %d %d %d %d %ld %31s",
                   &ch, &ih, &hb, &eh, &q, &d, &fx, &k, &te, &sr, &dv, &p->tmdb,
                   p->id) == 13 && k >= 0 && k <= K_LARGADA) {
          p->c.chave = ch; p->c.idh = ih; p->c.hb = hb; p->c.eh = eh;
          p->c.quando = q; p->c.delta = d; p->c.faixa = (int16_t)fx;
          p->c.kind = (uint8_t)k; p->tent = (uint8_t)te; p->serie = (uint8_t)sr;
          p->devolve = (uint8_t)!!dv;
          S.nPend++;
        } else { mal = 1; break; }
      } else if (linha[0] == 'c' && S.nCred < FSIN_CRED_N) {
        Cred *c = &S.cred[S.nCred];
        int d, fx, k; long long ch, q;
        unsigned long long ih, hb, eh;
        if (sscanf(linha + 1, "%llu %llu %llu %lld %lld %d %d %d",
                   &ih, &hb, &eh, &q, &ch, &d, &fx, &k) == 8) {
          c->idh = ih; c->hb = hb; c->eh = eh; c->quando = q; c->chave = ch;
          c->delta = d; c->faixa = (int16_t)fx; c->kind = (uint8_t)k;
          S.nCred++;
        } else { mal = 1; break; }
      } else if (linha[0] == 'o' && S.nObra < FSIN_OBRA_N) {
        long long ch;
        if (sscanf(linha + 1, "%lld", &ch) == 1) S.obra[S.nObra++] = ch;
        else { mal = 1; break; }
      } else if (linha[0] == 'a') {
        long long de, para;
        if (sscanf(linha + 1, "%lld %lld", &de, &para) == 2)
          aliasPoe(de, para);
        else { mal = 1; break; }
      } else if (linha[0] == 'i' && S.nOri < FSIN_ORI_N) {
        unsigned h, o, d;
        if (sscanf(linha + 1, "%u %u %u", &h, &o, &d) == 3) {
          S.ori[S.nOri].h = h; S.ori[S.nOri].o = (uint8_t)o;
          S.ori[S.nOri].dia = (uint8_t)d; S.nOri++;
        } else { mal = 1; break; }
      }
    }
    fclose(f);
    if (mal) {   // estado parcial nao sobe: recomeça do zero como o cabecalho
      S.nPend = S.nCred = S.nObra = S.obraProx = S.nAli = 0;
      S.nOri = S.oriProx = 0;
      S.rec = 1; S.cur = 0; S.curSeq = 0;
      return;
    }
  }
  // Pendencia que veio do disco volta a fila de busca: o reinicio nao pode
  // deixar ficha pedida para tras (correcao FAROL38B).
  for (i = 0; i < S.nPend; i++)
    if (S.pend[i].tent < 2)
      pedeFicha(S.pend[i].c.chave, S.pend[i].id, S.pend[i].serie,
                S.pend[i].tmdb);
}

// Troca o modelo carregado — a fronteira entre perfis: grava o que sai,
// zera tudo o que e do perfil e carrega o que entra. NENHUMA vaga de
// cred/pend cruza a fronteira (ressalva: delta nao vaza para outro perfil).
static void trocaPerfil(int p) {  // com a trava tomada
  char cam[1100];
  if (p == S.perfil) return;
  if (S.perfil > 0) { gravaModelo(); gravaEstado(); }
  if (S.fc.sujo && caminho("farol-fichas.bin", cam, sizeof cam))
    { farolficha_gravar(&S.fc, cam); S.fc.sujo = 0; }
  memset(&S.g, 0, sizeof S.g);
  S.nCred = S.nPend = S.nObra = S.obraProx = 0;
  S.nTempChk = S.nOri = S.oriProx = 0;
  S.cur = 0; S.curSeq = 0; S.varrer = 1; S.varGer++;
  S.largadaVarrida = 0;
  S.rec = 1; S.sujo = 0; S.perfil = p;
  if (p > 0) { leModelo(); leEstado(); }
}

// ---------------------------------------------------------------- garante --
static void cbDiario(int perfil, int ev, const DiarioEntrada *e, void *ctx);
static void cbVisto(const char *imdb, const char *tipo, const VistoPar *pares,
                    int n, int visto);
static void cbSalvos(const CatItem *ci, int salvo);
static void cbTirou(const char *imdb);

// O registro dos ouvintes e UMA VEZ e FORA de qualquer trava: diario_ouvir
// toma o mutex do diario e diario_percorrer chama cbVar com ele tomado —
// registrar sob a nossa trava inverteria a ordem e trancaria os dois
// (correcao FAROL38B). Chamado por pthread_once no iniciar.
static void ligaOuvintes(void) {
  diario_ouvir(cbDiario, NULL);
  salvos_ao_definir(cbSalvos);
  visto_ao_marcar(cbVisto);
  prog_ao_retirar(cbTirou);
}
static pthread_once_t onceOuv = PTHREAD_ONCE_INIT;

// COM A TRAVA TOMADA: o chamador faz a conferencia e o uso do perfil no
// mesmo fecho — a corrida do TSan 66 era ler/decidir S.perfil fora do fecho
// (correcao FAROL38B).
static int garante_i(void) {
  if (!S.dir[0]) snprintf(S.dir, sizeof S.dir, "%s", dados_dir());
  if (!S.ligado) {
    char cam[1100];
    S.ligado = 1;
    farolficha_cache_iniciar(&S.fc);
    // O arquivo das fichas so anda dentro da trava — leitura e escrita
    // serializadas aqui e na fila lenta (ressalva do lote).
    if (caminho("farol-fichas.bin", cam, sizeof cam)) {
      farolficha_ler_arquivo(&S.fc, cam);
      S.fc.sujo = 0;
    }
  }
  trocaPerfil(perfis_ativo());
  return S.perfil > 0;
}

// "Terminou" so vale para quem viu ate o fim: registro do player com
// posicao (seg/dur) pede 90% — 10 s de 1 h nao e fim (correcao FAROL38B).
// Historico sem posicao (sync da conta, nota manual) continua valendo.
static int concluiu(const DiarioEntrada *e) {
  if (e->origem != DIARIO_ORIG_PLAYER || e->durSeg <= 0) return 1;
  return (long long)e->seg * 10 >= (long long)e->durSeg * 9;
}

// ----------------------------------------------------------- delta da vez --
// A tabela 2.2, linha a linha, para uma entrada do diario. Sono e parcial
// nao pesam (secao 1.3: "adormeceu nao e dislike"); "mais ou menos" (nota
// 4-6) pesa 0 — a ficha conta a obra, o sinal e neutro.
static int deltaDeEntrada(const DiarioEntrada *e) {
  int d;
  if (e->flags & (DIARIO_F_DORMIU | DIARIO_F_DORMIU_SUS | DIARIO_F_PARCIAL))
    return 0;
  if (e->nota10 >= 1 && e->nota10 <= 3) d = FG_D_REPROVOU;
  else if ((e->flags & DIARIO_F_CORACAO) || e->nota10 >= 7) d = FG_D_GOSTOU;
  else if (e->flags & DIARIO_F_REVER) d = FG_D_REVIU;
  else if (e->nota10 >= 4 && e->nota10 <= 6) d = 0;
  else d = concluiu(e) ? FG_D_TERMINOU : 0;
  // Serie pontua 0,1 por episodio; a temporada completa conta o valor cheio
  // uma vez — cobrado em confereTemporada.
  if (d > 0 && e->tipo == DIARIO_SERIE && (e->temporada > 0 || e->episodio > 0))
    d = (d * 26 + 128) / 256;
  if (e->flags & DIARIO_F_JUNTOS) d = (d + (d >= 0 ? 1 : -1)) / 2;
  return d;
}

// NOVA (e o passe da varredura): a vez ainda nao tem registro... ou tem o de
// um gesto/eco — a segunda entrega casa pela hb e so cobra a diferenca.
static void vezNova(const DiarioEntrada *e) {   // com a trava tomada
  Cred c;
  uint64_t hb = h3(e->id, e->temporada, e->episodio);
  uint64_t eh = hb ^ mix64((uint64_t)e->quando);
  int i, delta = deltaDeEntrada(e), viaVez = 0;
  int faixa = e->hora >= 0 ? faixaDeHora(e->hora) : faixaAgora(e->quando);
  int serie = e->tipo == DIARIO_SERIE;
  i = achaEh(eh);
  if (i < 0) { i = achaVez(hb, e->quando); viaVez = i >= 0; }
  // O player com REVER entregou uma vez NOVA (a fusao nao colou porque a
  // segunda ida e de verdade): cai fora e credita cheio — mas so quando o
  // registro foi achado pela hb de vez; o achado pela eh exata e a MESMA
  // entrega, nunca vez nova (correcao FAROL38B). Nos demais casos e o eco
  // de um gesto ou a fusao re-entregue — so o que faltava, e so para cima.
  if (i >= 0 && !(viaVez && e->origem == DIARIO_ORIG_PLAYER &&
                  (e->flags & DIARIO_F_REVER) && S.cred[i].kind == K_VEZ)) {
    int diff = delta - S.cred[i].delta;
    if (diff > 0) {
      int64_t real; FarolFicha f;
      if ((real = pegaFicha(aliasPara(S.cred[i].chave), e->id, serie, &f))) {
        aplicar(&f, real, diff, diaDe(e->quando), faixa);
      } else {   // sem ficha: a diferenca espera como a vez esperou
        Cred cc = S.cred[i]; cc.delta = diff; cc.eh = eh ^ 0xD1FF;
        pendPoe(&cc, e->id, serie, e->tmdb, 0);
      }
      S.cred[i].delta += diff;
      S.sujo = 1;
    }
    if (S.cred[i].kind == K_GESTO) {  // a entrada e a verdade do gesto
      S.cred[i].eh = eh; S.cred[i].quando = e->quando; S.cred[i].kind = K_VEZ;
    }
    return;
  }
  memset(&c, 0, sizeof c);
  c.idh = h1(e->id); c.hb = hb; c.eh = eh; c.quando = e->quando;
  c.chave = chaveObra(e->id, serie, e->tmdb);
  c.delta = delta; c.faixa = (int16_t)faixa; c.kind = K_VEZ;
  credita(e->id, serie, e->tmdb, &c);
  // Episodio de serie: talvez fechou a temporada.
  if (delta > 0 && serie && e->temporada > 0 && S.nTempChk < FSIN_TEMP_N) {
    int k;
    for (k = 0; k < S.nTempChk; k++)
      if (S.tempChk[k].t == e->temporada && !strcmp(S.tempChk[k].id, e->id))
        break;
    if (k == S.nTempChk) {
      snprintf(S.tempChk[k].id, sizeof S.tempChk[k].id, "%s", e->id);
      S.tempChk[k].t = e->temporada; S.nTempChk++;
    }
  }
}

// A temporada que tinha o bonus K_TEMP ficou incompleta? O bonus volta —
// a contagem e pelas vezes creditadas (a trava do diario nao se toma aqui
// dentro). Com a trava tomada.
static void temporadaDesfeita(const char *id, int t, long tmdb) {
  int j = achaHb(hDom(id, K_TEMP, t));
  int idx, ne, k, tot = 0, vist = 0;
  if (t <= 0 || j < 0) return;
  if ((idx = cat_indice_por_imdb(id)) < 0) return;
  ne = cat_n_episodios(idx);
  for (k = 0; k < ne; k++) {
    const CatEp *ep = cat_episodio(idx, k);
    if (ep && ep->temporada == t) {
      tot++;
      if (vezCredida(h3(id, t, ep->episodio))) vist++;
    }
  }
  if (tot && vist < tot) {
    Cred cc = S.cred[j];
    credTira(j);
    desfaz(&cc, id, 1, tmdb);
  }
}

// A entrada apagada desfaz o que a vez somou — so pelo registro, nunca por
// palpite: entrada que nunca creditou nao tira nada.
static void vezApagada(const DiarioEntrada *e) {  // com a trava tomada
  uint64_t hb = h3(e->id, e->temporada, e->episodio);
  uint64_t eh = hb ^ mix64((uint64_t)e->quando);
  int i = achaEh(eh), serie = e->tipo == DIARIO_SERIE;
  if (i < 0) i = achaVez(hb, e->quando);
  if (i >= 0) {
    Cred c = S.cred[i];
    credTira(i);
    desfaz(&c, e->id, serie, e->tmdb);
    // a temporada que esta entrada fechava nao fecha mais (correcao FAROL38B)
    temporadaDesfeita(e->id, e->temporada, e->tmdb);
  }
  // e se ainda era so pendencia, cancela antes da ficha chegar — mas NUNCA
  // a pendencia DEVOLVE de estorno: ela e divida, nao intencao (correcao
  // FAROL38B).
  for (i = S.nPend - 1; i >= 0; i--)
    if (S.pend[i].c.hb == hb && S.pend[i].c.kind <= K_GESTO &&
        !S.pend[i].devolve &&
        (S.pend[i].c.eh == eh ||
         (S.pend[i].c.quando > e->quando - DIARIO_JANELA_S &&
          S.pend[i].c.quando < e->quando + DIARIO_JANELA_S)))
      { pendTira(i); S.sujo = 1; }
}

// ALTERADA (nota, flags, data ou fusao que enriqueceu): so a diferenca anda.
static void vezAlterada(const DiarioEntrada *e) {   // com a trava tomada
  uint64_t hb = h3(e->id, e->temporada, e->episodio);
  uint64_t eh = hb ^ mix64((uint64_t)e->quando);
  int i = achaEh(eh), serie = e->tipo == DIARIO_SERIE;
  int delta = deltaDeEntrada(e);
  if (i < 0) i = achaVez(hb, e->quando);
  if (i >= 0) {
    int diff = delta - S.cred[i].delta;
    if (diff) {
      int64_t real; FarolFicha f;
      int fx = e->hora >= 0 ? faixaDeHora(e->hora) : S.cred[i].faixa;
      if ((real = pegaFicha(aliasPara(S.cred[i].chave), e->id, serie, &f))) {
        aplicar(&f, real, diff, diaDe(e->quando), fx);
        S.sujo = 1;
      } else {
        Cred cc = S.cred[i]; cc.delta = diff;   // ajusta quando a ficha voltar
        pendPoe(&cc, e->id, serie, e->tmdb, 0);
      }
      S.cred[i].delta += diff;
    }
    S.cred[i].eh = eh; S.cred[i].quando = e->quando;
    S.sujo = 1;
    return;
  }
  for (i = S.nPend - 1; i >= 0; i--)
    if (S.pend[i].c.hb == hb && S.pend[i].c.kind == K_VEZ &&
        !S.pend[i].devolve)
      { S.pend[i].c.delta = delta; S.pend[i].c.eh = eh;
        S.pend[i].c.quando = e->quando; S.sujo = 1; return; }
  vezNova(e);   // registro desconhecido: e como se fosse nova
}

static void cbDiario(int perfil, int ev, const DiarioEntrada *e, void *ctx) {
  (void)ctx;
  TRAVA();
  if (!garante_i()) { DTRAVA(); return; }
  if (ev == DIARIO_EV_RECARREGADO)
    { S.varrer = 1; S.cur = 0; S.curSeq = 0; S.varGer++; }
  else if (e && perfil == S.perfil && S.rec) {
    if (ev == DIARIO_EV_NOVA) vezNova(e);
    else if (ev == DIARIO_EV_ALTERADA) vezAlterada(e);
    else if (ev == DIARIO_EV_APAGADA) vezApagada(e);
  }
  DTRAVA();
}

// -------------------------------------------------------- visto / desvisto --
// Marcar assistido vale +1,0 por vez — a entrada do diario que a conta (ou o
// player) devolve depois casa pela hb e so completa o que faltava.
static void cbVisto(const char *imdb, const char *tipo, const VistoPar *pares,
                    int n, int visto) {
  int serie, i, idx;
  long tmdb = 0;
  const CatItem *ci;
  if (!imdb || !imdb[0]) return;
  serie = tipo && !strcmp(tipo, "series");
  if ((idx = cat_indice_por_imdb(imdb)) >= 0 &&
      (ci = cat_item(idx)) != NULL) tmdb = ci->tmdb;
  if (visto) {
    long long agora = agoraS();
    int faixa = faixaAgora(agora);
    TRAVA();
    if (garante_i() && S.rec) {
      int total = n < 1 ? 1 : n;
      for (i = 0; i < total; i++) {
        int t = n < 1 ? 0 : pares[i].temporada;
        int e = n < 1 ? 0 : pares[i].episodio;
        uint64_t hb = h3(imdb, t, e);
        Cred c;
        if (achaVez(hb, agora) >= 0) continue;   // a vez ja conta
        memset(&c, 0, sizeof c);
        c.idh = h1(imdb); c.hb = hb; c.eh = hb ^ mix64((uint64_t)agora);
        c.quando = agora; c.chave = chaveObra(imdb, serie, tmdb);
        // episodio de serie preve a entrada de 0,1 — nao a vez inteira
        // (tabela 2.2; correcao FAROL38B).
        c.delta = serie && (t > 0 || e > 0)
                  ? (FG_D_TERMINOU * 26 + 128) / 256 : FG_D_TERMINOU;
        c.faixa = (int16_t)faixa; c.kind = K_GESTO;
        credita(imdb, serie, tmdb, &c);
      }
    }
    DTRAVA();
    return;
  }
  // DESMARCOU: devolve os creditos de vez/gesto do alvo (e, no titulo
  // inteiro, o bonus de temporada e o "largada"). O DIARIO NAO E TOCADO:
  // desmarcar visto desfaz so o sinal do Farol — a entrada continua la
  // (correcao FAROL38B: o gesto apagava o historico do usuario).
  TRAVA();
  if (garante_i() && S.rec) {
    uint64_t idh = h1(imdb);
    for (i = S.nCred - 1; i >= 0; i--) {
      int alvo = 0, k;
      if (S.cred[i].idh != idh) continue;
      if (S.cred[i].kind > K_GESTO &&
          !(n < 1 && (S.cred[i].kind == K_TEMP || S.cred[i].kind == K_LARGADA)))
        continue;
      if (n < 1) alvo = 1;
      else for (k = 0; k < n; k++)
        if (S.cred[i].hb == h3(imdb, pares[k].temporada, pares[k].episodio))
          alvo = 1;
      if (alvo) {
        Cred cc = S.cred[i];
        credTira(i);
        desfaz(&cc, imdb, serie, tmdb);
      }
    }
    for (i = S.nPend - 1; i >= 0; i--) {
      int alvo = 0, k;
      if (S.pend[i].devolve || S.pend[i].c.idh != idh) continue;
      if (S.pend[i].c.kind > K_GESTO &&
          !(n < 1 && (S.pend[i].c.kind == K_TEMP ||
                      S.pend[i].c.kind == K_LARGADA)))
        continue;
      if (n < 1) alvo = 1;
      else for (k = 0; k < n; k++)
        if (S.pend[i].c.hb == h3(imdb, pares[k].temporada, pares[k].episodio))
          alvo = 1;
      if (alvo) { pendTira(i); S.sujo = 1; }
    }
    // episodio desmarcado pode ter deixado a temporada incompleta
    if (serie && n >= 1)
      for (i = 0; i < n; i++) temporadaDesfeita(imdb, pares[i].temporada, tmdb);
  }
  DTRAVA();
}

// Salvos ("+"): +0,6; tirar devolve. Nivel de titulo, dominio proprio.
static void cbSalvos(const CatItem *ci, int salvo) {
  uint64_t hb;
  int i;
  if (!ci || !ci->imdb[0]) return;
  hb = hDom(ci->imdb, K_SALVO, 0);
  TRAVA();
  if (garante_i() && S.rec) {
    if (salvo) {
      if (achaHb(hb) < 0) {
        Cred c; long long agora = agoraS();
        memset(&c, 0, sizeof c);
        c.idh = h1(ci->imdb); c.hb = hb; c.eh = hb ^ mix64((uint64_t)agora);
        c.quando = agora;
        c.chave = chaveObra(ci->imdb, !strcmp(ci->tipo, "series"), ci->tmdb);
        c.delta = FG_D_SALVOU; c.faixa = (int16_t)faixaAgora(agora);
        c.kind = K_SALVO;
        credita(ci->imdb, !strcmp(ci->tipo, "series"), ci->tmdb, &c);
      }
    } else {
      if ((i = achaHb(hb)) >= 0) {
        Cred c = S.cred[i];
        credTira(i);
        desfaz(&c, ci->imdb, !strcmp(ci->tipo, "series"), ci->tmdb);
      }
      // Retirou antes da ficha chegar: a intencao pendente morre junto —
      // a ficha tardia nao pode creditar um salvo que ja nao existe
      // (correcao FAROL38B). Pendencia DEVOLVE (estorno) fica: e divida.
      for (i = S.nPend - 1; i >= 0; i--)
        if (S.pend[i].c.hb == hb && S.pend[i].c.kind == K_SALVO &&
            !S.pend[i].devolve) { pendTira(i); S.sujo = 1; }
    }
  }
  DTRAVA();
}

// "Tirou do Continuar" (prog_marcar_removido / prog_ocultar_continuar): -0,6,
// uma vez por titulo — a recusa leve da secao 1.3.
static void cbTirou(const char *imdb) {
  char id[DIARIO_ID_MAX];
  uint64_t hb;
  int idx, serie = 0;
  long tmdb = 0;
  const CatItem *ci;
  if (!imdb || !imdb[0]) return;
  // o id composto so se divide nos ":T:E" finais — "tmdb:321" inteiro e o
  // proprio id (correcao FAROL38B: cortar no primeiro ':' juntava tmdb:321
  // e tmdb:654 na mesma obra).
  diario_separar_id(imdb, id, sizeof id, NULL, NULL);
  if ((idx = cat_indice_por_imdb(id)) >= 0 && (ci = cat_item(idx)) != NULL) {
    serie = !strcmp(ci->tipo, "series"); tmdb = ci->tmdb;
  }
  hb = hDom(id, K_TIROU, 0);
  TRAVA();
  if (!garante_i()) { DTRAVA(); return; }
  if (S.rec && achaHb(hb) < 0) {
    Cred c; long long agora = agoraS();
    memset(&c, 0, sizeof c);
    c.idh = h1(id); c.hb = hb; c.eh = hb ^ mix64((uint64_t)agora);
    c.quando = agora; c.chave = chaveObra(id, serie, tmdb);
    c.delta = FG_D_TIROU; c.faixa = (int16_t)faixaAgora(agora);
    c.kind = K_TIROU;
    credita(id, serie, tmdb, &c);
  }
  DTRAVA();
}

// Um sinal avulso de CatItem (interesse, mais assim, recusa): dedup pela hb
// do dominio e credito direto. Com a trava tomada. 1 = sinal novo.
static int avulso(const CatItem *ci, int kind, int nDom, int delta) {
  Cred c;
  uint64_t hb = hDom(ci->imdb, kind, nDom);
  long long agora = agoraS();
  if (achaHb(hb) >= 0) return 0;
  memset(&c, 0, sizeof c);
  c.idh = h1(ci->imdb); c.hb = hb; c.eh = hb ^ mix64((uint64_t)agora);
  c.quando = agora;
  c.chave = chaveObra(ci->imdb, !strcmp(ci->tipo, "series"), ci->tmdb);
  c.delta = delta; c.faixa = (int16_t)faixaAgora(agora); c.kind = (uint8_t)kind;
  credita(ci->imdb, !strcmp(ci->tipo, "series"), ci->tmdb, &c);
  return 1;
}
static int desfazAvulso(const CatItem *ci, int kind, int nDom) {
  int i = achaHb(hDom(ci->imdb, kind, nDom));
  if (i >= 0) {
    Cred c = S.cred[i];
    credTira(i);
    desfaz(&c, ci->imdb, !strcmp(ci->tipo, "series"), ci->tmdb);
    return 1;
  }
  return 0;
}

// ------------------------------------------------------------------ saida --
void farolsinal_saida(const char *id, char tipo, int temporada, int episodio,
                      long tmdb, double posSeg, double durSeg, int concluiu) {
  DiarioEntrada sus;
  int falha, serie;
  Cred c;
  long long agora;
  (void)temporada; (void)episodio;   // o abandono e da obra inteira
  if (!id || !id[0] || concluiu || posSeg < FSIN_ABAND_MIN_S ||
      durSeg <= 0 || posSeg * 100.0 >= durSeg * FSIN_ABAND_PCT) return;
  // Sono confirmado ou suspeito recente neste titulo: a saida era descanso,
  // nao desgosto (secao 1.3: "adormeceu nao e dislike"). A suspeita precisa
  // estar na janela DOS DOIS LADOS — nem velha demais nem no futuro
  // (correcao FAROL38B: relogio torto dispensava abandono real).
  if (dormiu_suspeita(id, &sus) && sus.quando > agoraS() - FSIN_SONO_JANELA &&
      sus.quando <= agoraS() + 300)
    return;
  // Falha de fonte/video nos ultimos 5 min: o player morreu, nao o gosto.
  falha = farolanel_contar(FE_ERRO_FONTE, FSIN_JANELA_FALHA, agoraMs()) ||
          farolanel_contar(FE_ERRO_NATIVO, FSIN_JANELA_FALHA, agoraMs()) ||
          farolanel_contar(FE_PIPELINE_MORREU, FSIN_JANELA_FALHA, agoraMs()) ||
          farolanel_contar(FE_PARADO, FSIN_JANELA_FALHA, agoraMs());
  if (falha) return;
  serie = tipo == DIARIO_SERIE;
  agora = agoraS();
  TRAVA();
  if (garante_i() && S.rec) {
    memset(&c, 0, sizeof c);
    c.idh = h1(id);
    c.hb = hDom(id, K_SAIDA, 0) ^ mix64((uint64_t)agora);
    c.eh = c.hb; c.quando = agora;
    c.chave = chaveObra(id, serie, tmdb);
    c.delta = FG_D_ABANDONOU; c.faixa = (int16_t)faixaAgora(agora);
    c.kind = K_SAIDA;
    credita(id, serie, tmdb, &c);
  }
  DTRAVA();
}

// --------------------------------------------------------- sinais avulsos --
void farolsinal_interesse(const CatItem *ci) {
  if (!ci || !ci->imdb[0]) return;
  TRAVA();
  if (garante_i() && S.rec)
    avulso(ci, K_INTERESSE, diaDe(agoraS()), FG_D_INTERESSE);
  DTRAVA();
}
void farolsinal_mais_assim(const CatItem *ci) {
  if (!ci || !ci->imdb[0]) return;
  TRAVA();
  if (garante_i() && S.rec) avulso(ci, K_MAIS, 0, FG_D_MAIS_ASSIM);
  DTRAVA();
}
void farolsinal_recusar(const CatItem *ci) {
  if (!ci || !ci->imdb[0]) return;
  TRAVA();
  if (garante_i() && S.rec && avulso(ci, K_RECUSA, 0, FG_D_REPROVOU))
    { farolgosto_recusar(&S.g, mapa_hash_titulo(ci->imdb)); S.sujo = 1; }
  DTRAVA();
}
void farolsinal_desfazer_recusa(const CatItem *ci) {
  if (!ci || !ci->imdb[0]) return;
  TRAVA();
  // a recusa sai tambem do anel — o titulo volta a aparecer nas fileiras
  // (correcao FAROL38B: "desfazer" deixava o hash para sempre).
  if (garante_i() && desfazAvulso(ci, K_RECUSA, 0))
    farolgosto_desrecusar(&S.g, mapa_hash_titulo(ci->imdb));
  DTRAVA();
}
void farolsinal_mostrado(const CatItem *ci) {
  if (!ci || !ci->imdb[0]) return;
  TRAVA();
  if (garante_i() && S.rec) {
    farolgosto_mostrado(&S.g, mapa_hash_titulo(ci->imdb),
                        (uint8_t)(diaDe(agoraS()) & 0xff));
    S.sujo = 1;
  }
  DTRAVA();
}
void farolsinal_origem(const CatItem *ci, int origem) {
  int i;
  if (!ci || !ci->imdb[0] ||
      origem <= FS_ORIGEM_NENHUMA || origem >= FS_ORIGEM_N) return;
  TRAVA();
  if (!garante_i()) { DTRAVA(); return; }
  if (S.nOri < FSIN_ORI_N) i = S.nOri++;
  else { i = S.oriProx; S.oriProx = (S.oriProx + 1) % FSIN_ORI_N; }
  S.ori[i].h = mapa_hash_titulo(ci->imdb);
  S.ori[i].o = (uint8_t)origem;
  S.ori[i].dia = (uint8_t)(diaDe(agoraS()) & 0xff);
  S.sujo = 1;
  DTRAVA();
}

// ------------------------------------------------------------- fila lenta --
static void pedeFicha(int64_t chave, const char *id, int serie, long tmdb) {
  int i;
  if (!chave) return;
  chave = aliasPara(chave);
  for (i = 0; i < S.nPed; i++) if (S.ped[i].chave == chave) return;
  if (S.nPed >= FSIN_PED_N) return;
  i = S.nPed++;
  S.ped[i].chave = chave; S.ped[i].tmdb = tmdb; S.ped[i].serie = (uint8_t)!!serie;
  snprintf(S.ped[i].id, sizeof S.ped[i].id, "%s", id ? id : "");
  pthread_cond_signal(&condPed);
}

static void *fioLenta(void *u) {
  FarolFichaTmp tmp;
  char *corpo = malloc(128 * 1024);   // do fio, nao estatico: o fio solto
  (void)u;                            // na saida nao divide buffer com outro
  memset(&tmp, 0, sizeof tmp);
  if (!corpo) return NULL;
  for (;;) {
    Ped p;
    FarolSinalBusca b;
    int n;
    TRAVA();
    while (!S.nPed && !fioMorre) pthread_cond_wait(&condPed, &trava);
    if (fioMorre) { DTRAVA(); break; }
    p = S.ped[0];
    memmove(&S.ped[0], &S.ped[1], sizeof(Ped) * (size_t)(--S.nPed));
    b = busca;
    DTRAVA();
    if (!b) continue;
    n = b(p.id[0] ? p.id : NULL, p.tmdb, p.serie, corpo, 128 * 1024 - 1);
    if (n > 0) {
      FarolFicha f;
      corpo[n] = 0;
      if (farolficha_ler_tmdb(corpo, p.serie, &tmp, &f))
        { farolsinal_ficha_ok(p.chave, &f); continue; }
    }
    farolsinal_ficha_falhou(p.chave);
  }
  free(corpo);
  return NULL;
}

void farolsinal_ficha_ok(int64_t chavePed, const FarolFicha *f) {
  int64_t real;
  if (!f) return;
  real = farolficha_chave(f);
  TRAVA();
  if (garante_i() && real) {
    farolficha_guardar(&S.fc, f);
    S.fichaSujo = 1;
    if (real != chavePed) aliasPoe(chavePed, real);
    drenaPends(real);
  }
  DTRAVA();
}
void farolsinal_ficha_falhou(int64_t chavePed) {
  int i;
  TRAVA();
  if (!garante_i()) { DTRAVA(); return; }
  for (i = S.nPend - 1; i >= 0; i--)
    if (S.pend[i].c.chave == chavePed) {
      if (++S.pend[i].tent < 2)
        pedeFicha(chavePed, S.pend[i].id, S.pend[i].serie, S.pend[i].tmdb);
      else { pendTira(i); S.sujo = 1; }   // solta: nao fica pendente pra sempre
    }
  DTRAVA();
}
void farolsinal_busca(FarolSinalBusca fn) {
  TRAVA(); busca = fn; pthread_cond_signal(&condPed); DTRAVA();
}
int  farolsinal_pendencias(void) {
  int n;
  TRAVA(); n = garante_i() ? S.nPend : 0; DTRAVA(); return n;
}

// ---------------------------------------------------------------- ciclo ----
int  farolsinal_recomendar(void) {
  int r; TRAVA(); r = garante_i() ? S.rec : 0; DTRAVA(); return r;
}
void farolsinal_definir_recomendar(int ligado) {
  TRAVA();
  if (garante_i() && S.rec != !!ligado) { S.rec = !!ligado; S.sujo = 1; }
  DTRAVA();
}
int farolsinal_modelo(FarolGosto *dst) {
  int ok;
  if (!dst) return 0;
  TRAVA();
  ok = garante_i();
  if (ok) *dst = S.g;
  DTRAVA();
  return ok;
}
int farolsinal_origens(uint32_t *h, uint8_t *o, uint8_t *d, int max) {
  int i, n = 0;
  TRAVA();
  if (!garante_i()) { DTRAVA(); return 0; }
  n = S.nOri < max ? S.nOri : max;
  for (i = 0; i < n; i++) {
    if (h) h[i] = S.ori[i].h;
    if (o) o[i] = S.ori[i].o;
    if (d) d[i] = S.ori[i].dia;
  }
  DTRAVA();
  return n;
}

void farolsinal_iniciar(void) {
  // ouvintes: uma vez, fora de toda trava (ver ligaOuvintes)
  pthread_once(&onceOuv, ligaOuvintes);
  TRAVA(); garante_i(); DTRAVA();
}
void farolsinal_pasta(const char *dir) {
  TRAVA();
  // pasta nova so faz sentido com o modelo ainda por carregar
  if (dir && *dir && S.perfil == 0) snprintf(S.dir, sizeof S.dir, "%s", dir);
  DTRAVA();
}
void farolsinal_relogio(long long (*s)(void), uint32_t (*ms)(void)) {
  f_agora_s = s; f_agora_ms = ms;
}

// O passe da varredura virou FATIA de teto rigido (correcao FAROL38C): no
// maximo FSIN_VEZ_MAX entregas por chamada, cortadas ate no meio de uma
// leva de mesmo `quando` — antes 700 vezes no mesmo instante entravam
// juntas num quadro so porque o teto nao cortava em empate. O cursor e a
// chave composta (S.cur, S.curSeq): tudo com `quando` maior que S.cur ja
// entrou e, dentro do grupo de mesmo `quando`, as S.curSeq primeiras na
// ordem da varredura tambem — f.saltar pula exatamente elas na fatia
// seguinte, sem entrega e sem processar de novo.
typedef struct {
  int n;              // processadas nesta fatia
  int cortou;         // 1 = bateu o teto, ha mais por varrer
  int abortou;        // 1 = perfil trocou ou fio morreu no meio
  int perfil;
  long long qUlt;     // `quando` da ultima processada
  int nUlt;           // processadas nesse `quando` nesta fatia
} VarCtx;
static int cbVar(const DiarioEntrada *e, void *ctx) {
  VarCtx *c = (VarCtx *)ctx;
  if (c->n >= FSIN_VEZ_MAX) { c->cortou = 1; return 0; }
  TRAVA();
  if (S.perfil != c->perfil || fioMorre) c->abortou = 1;
  else if (S.rec) vezNova(e);
  DTRAVA();
  if (c->abortou) return 0;
  c->n++;
  if (c->n == 1 || e->quando != c->qUlt) { c->qUlt = e->quando; c->nUlt = 1; }
  else c->nUlt++;
  return 1;
}

// Uma fatia da varredura, chamada pelo passo: a entrega mais pesada do
// quadro fica limitada a FSIN_VEZ_MAX vezes por passada, nunca mais a uma
// leva inteira de mesmo instante (correcao FAROL38C). O cursor so anda se a
// geracao nao mudou — um RECARREGADO/esquecer no meio da fatia manda
// recomeçar, nunca herda um cursor velho.
static void fatiaVarre(int perfil) {
  VarCtx c;
  DiarioFiltro f;
  long long q0;
  int seq0, ger;
  memset(&c, 0, sizeof c); memset(&f, 0, sizeof f);
  TRAVA();
  c.perfil = perfil;
  q0 = S.cur; seq0 = S.curSeq; ger = S.varGer;
  if (q0) {
    if (seq0 >= FSIN_SEQ_TODA) f.ate = q0;         // grupo do cursor consumido
    else { f.ate = q0 + 1; f.saltar = seq0; }      // retoma dentro do grupo
  }
  f.decrescente = 1;
  DTRAVA();
  diario_percorrer(perfil, &f, cbVar, &c);
  TRAVA();
  if (!c.abortou && S.varGer == ger) {
    if (c.cortou) {
      S.cur = c.qUlt;
      S.curSeq = c.nUlt + (c.qUlt == q0 && seq0 < FSIN_SEQ_TODA ? seq0 : 0);
    } else S.varrer = 0;
    S.sujo = 1;
  }
  DTRAVA();
}

// O episodio completou a temporada? Credita o valor cheio UMA vez por
// temporada (regra da serie na tabela 2.2). Roda no fio de tras, fora do
// diario.
static void confereTemporada(const char *id, int t) {
  int idx, ne, i, tot = 0, vistos = 0, perfil;
  const CatEp *ep;
  uint64_t hb;
  TRAVA(); perfil = S.perfil; DTRAVA();
  if ((idx = cat_indice_por_imdb(id)) < 0) return;
  ne = cat_n_episodios(idx);
  for (i = 0; i < ne; i++) {
    if ((ep = cat_episodio(idx, i)) == NULL || ep->temporada != t) continue;
    tot++;
    if (diario_episodio(perfil, id, t, ep->episodio, NULL)) vistos++;
  }
  if (!tot || vistos < tot) return;
  hb = hDom(id, K_TEMP, t);
  TRAVA();
  if (achaHb(hb) < 0) {
    Cred c;
    const CatItem *ci = cat_item(idx);
    long tmdb = ci ? ci->tmdb : 0;
    long long agora = agoraS();
    memset(&c, 0, sizeof c);
    c.idh = h1(id); c.hb = hb; c.eh = hb ^ mix64((uint64_t)agora);
    c.quando = agora; c.chave = chaveObra(id, 1, tmdb);
    c.delta = FG_D_TERMINOU; c.faixa = (int16_t)faixaAgora(agora);
    c.kind = K_TEMP;
    credita(id, 1, tmdb, &c);
  }
  DTRAVA();
}

// Conta episodios vistos do titulo — a regra §1.3 e "1-3 episodios".
static int contaEps(const DiarioEntrada *e, void *c) {
  int *n = (int *)c;
  if (e->temporada || e->episodio) (*n)++;
  return *n < 4;                       // so interessa saber 1..3
}

// "Serie largada" (secao 1.3): progresso parado ha 21+ dias, 1-3 episodios
// vistos, ultima vez ha 21+ dias, episodio mais adiante existe no catalogo
// e o titulo nunca tomou este sinal. Uma varredura por carga do modelo, no
// fio de tras.
static void varreLargada(void) {
  ProgRegistro regs[64];
  int n, i, perfil;
  long long agora = agoraS();
  long long agoraMs = agora * 1000LL;
  TRAVA(); perfil = S.perfil; DTRAVA();
  n = prog_ler(regs, 64);
  for (i = 0; i < n; i++) {
    int t = 0, e = 0, idx, ne, k, ha = 0, neps = 0;
    long long q = 0;
    uint64_t hb;
    DiarioFiltro df;
    if (strcmp(regs[i].tipo, "series") || !regs[i].contentId[0] ||
        regs[i].perfil != perfil ||
        regs[i].lastWatchedMs <= 0 ||
        agoraMs - regs[i].lastWatchedMs < FSIN_LARGADA_MS) continue;
    hb = hDom(regs[i].contentId, K_LARGADA, 0);
    TRAVA(); ha = achaHb(hb) >= 0; DTRAVA();
    if (ha) continue;
    // §1.3: a data do ultimo episodio tambem tem de fazer os 21 dias — um
    // episodio visto esta semana nao e serie largada (correcao FAROL38B).
    if (!diario_ultimo_episodio(perfil, regs[i].contentId, &t, &e, &q) ||
        q <= 0 || q > agora || agora - q < FSIN_LARGADA_MS / 1000) continue;
    // e so com 1-3 episodios vistos: quem viu mais nao "largou", parou.
    memset(&df, 0, sizeof df);
    df.id = regs[i].contentId; df.tipo = DIARIO_SERIE;
    diario_percorrer(perfil, &df, contaEps, &neps);
    if (neps < 1 || neps > 3) continue;
    if ((idx = cat_indice_por_imdb(regs[i].contentId)) < 0) continue;
    ne = cat_n_episodios(idx);
    for (k = 0; k < ne && !ha; k++) {
      const CatEp *ep = cat_episodio(idx, k);
      if (ep && (ep->temporada > t || (ep->temporada == t && ep->episodio > e)))
        ha = 1;
    }
    if (!ha) continue;
    TRAVA();
    if (achaHb(hb) < 0) {
      Cred c;
      long long agora = agoraS();
      memset(&c, 0, sizeof c);
      c.idh = h1(regs[i].contentId); c.hb = hb;
      c.eh = hb ^ mix64((uint64_t)agora);
      c.quando = agora; c.chave = chaveObra(regs[i].contentId, 1, 0);
      c.delta = FG_D_LARGOU; c.faixa = (int16_t)faixaAgora(agora);
      c.kind = K_LARGADA;
      credita(regs[i].contentId, 1, 0, &c);
    }
    DTRAVA();
  }
}

// ------------------------------------------------------------- fio de tras --
// O que o passo delegava ao quadro e a r2 marcou: as conferidas de
// temporada, a varredura de serie-largada e as gravacoes de
// modelo/estado/fichas. Ficaram num fio proprio (correcao FAROL38C): o
// passo so acorda o fio; nenhuma escrita de arquivo volta ao fio chamador.
// Cada volta do fio executa UMA unidade e o NV_UM_FIO cede a fibra entre
// elas — sem fio de verdade o trabalho continua fatiado por rodada.
static int trabPendente(long long agora) {   // com a trava tomada
  return S.perfil > 0 &&
         (S.nTempChk > 0 || !S.largadaVarrida ||
          ((S.sujo || S.fichaSujo) && agora - S.ultGrav >= FSIN_GRAVA_S));
}
static void trabFatia(void) {
  char id[DIARIO_ID_MAX]; int t;
  long long agora;
  TRAVA();
  if (!garante_i()) { DTRAVA(); return; }
  agora = agoraS();
  // temporada completa? (fila alimentada pelas vezes de episodio)
  if (S.nTempChk) {
    S.nTempChk--;
    snprintf(id, sizeof id, "%s", S.tempChk[S.nTempChk].id);
    t = S.tempChk[S.nTempChk].t;
    DTRAVA();
    confereTemporada(id, t);
    return;
  }
  if (!S.largadaVarrida) {
    S.largadaVarrida = 1;
    DTRAVA();
    varreLargada();
    return;
  }
  // gravacoes juntas, com folga de 60 s — agora fora do fio da UI
  if ((S.sujo || S.fichaSujo) && agora - S.ultGrav >= FSIN_GRAVA_S) {
    char cam[1100];
    S.ultGrav = agora;
    if (S.sujo) { gravaModelo(); gravaEstado(); S.sujo = 0; }
    if (S.fichaSujo && caminho("farol-fichas.bin", cam, sizeof cam))
      { farolficha_gravar(&S.fc, cam); S.fichaSujo = 0; }
  }
  DTRAVA();
}
static void *fioTrab(void *u) {
  (void)u;
  for (;;) {
    TRAVA();
    while (!fioMorre && !trabPendente(agoraS()))
      pthread_cond_wait(&condTrab, &trava);
    if (fioMorre) { DTRAVA(); break; }
    fioTrabOcup = 1;
    DTRAVA();
    trabFatia();
    TRAVA();
    fioTrabOcup = 0;
    DTRAVA();
#ifdef NV_UM_FIO
    fio1_ceder();
#endif
  }
  return NULL;
}

void farolsinal_passo(void) {
  int i, perfil, varre;
  long long agora;
  TRAVA();
  if (!garante_i()) { DTRAVA(); return; }
  perfil = S.perfil; agora = agoraS();
  // a fila lenta so nasce quando ha transporte e pedido — leitura e
  // criacao no mesmo fecho (correcao FAROL38B: busca/fioVivo liam soltos)
  if (busca && !fioVivo && !fioMorre && S.nPed &&
      !pthread_create(&fioId, NULL, fioLenta, NULL)) fioVivo = 1;
  // pendencias velhas demais morrem: a ficha nao veio em 30 dias
  for (i = S.nPend - 1; i >= 0; i--)
    if (S.pend[i].c.quando < agora - (long long)FSIN_PEND_DIAS * 86400)
      { pendTira(i); S.sujo = 1; }
  varre = S.varrer;
  // conferidas e gravacoes vao para o fio de tras — o quadro so agenda
  // (correcao FAROL38C: a UI nao espera diario, catalogo nem disco).
  if (!fioTrabVivo && !fioMorre && trabPendente(agora) &&
      !pthread_create(&fioTrabId, NULL, fioTrab, NULL)) fioTrabVivo = 1;
  else if (fioTrabVivo && trabPendente(agora))
    pthread_cond_signal(&condTrab);
  DTRAVA();
  // varredura do diario (pos-RECARREGADO ou primeira carga): UMA fatia por
  // passada com teto rigido de FSIN_VEZ_MAX entregas, cursor composto que
  // avanca ate em empate de `quando` — termina sempre (correcao FAROL38C).
  if (varre) fatiaVarre(perfil);
}

void farolsinal_sair(void) {
  char cam[1100];
  int vivo, tvivo;
  TRAVA();
  if (!garante_i()) { DTRAVA(); return; }
  // NAO espera busca nem varredura em curso: o fio da UI nao pode pendurar
  // na saida (correcao FAROL38B/C). Os fios veem fioMorre e morrem
  // sozinhos; o que ainda faltava grava do estado — a pendencia volta na
  // proxima carga.
  fioMorre = 1;
  pthread_cond_signal(&condPed);
  pthread_cond_signal(&condTrab);
  vivo = fioVivo; fioVivo = 0;
  tvivo = fioTrabVivo; fioTrabVivo = 0;
  gravaModelo(); gravaEstado();
  if (caminho("farol-fichas.bin", cam, sizeof cam) && (S.fc.sujo || S.fc.n))
    farolficha_gravar(&S.fc, cam);
  S.fc.sujo = 0; S.sujo = 0;
  DTRAVA();
  if (vivo) pthread_detach(fioId);
  if (tvivo) pthread_detach(fioTrabId);
}

void farolsinal_esquecer(void) {
  char cam[1100], nome[64];
  TRAVA();
  if (!garante_i()) { DTRAVA(); return; }
  if (S.perfil > 0) {
    nomeGosto(S.perfil, nome, sizeof nome);
    if (caminho(nome, cam, sizeof cam)) remove(cam);
    nomeEstado(S.perfil, nome, sizeof nome);
    if (caminho(nome, cam, sizeof cam)) remove(cam);
  }
  if (caminho("farol-fichas.bin", cam, sizeof cam)) remove(cam);
  farolgosto_iniciar(&S.g, (uint16_t)S.perfil);
  farolficha_cache_iniciar(&S.fc);
  S.nCred = S.nPend = S.nObra = S.obraProx = S.nAli = 0;
  S.nPed = S.nTempChk = S.nOri = S.oriProx = 0;
  S.cur = 0; S.curSeq = 0; S.varrer = 1; S.varGer++;
  S.largadaVarrida = 0; S.rec = 1;
  S.sujo = 0;
  S.fichaSujo = 0;
  DTRAVA();
}

// 1 enquanto houver trabalho de fundo pendente ou em curso — fatia de
// varredura, conferida, largada ou gravacao (teste/servico; correcao
// FAROL38C).
int farolsinal_teste_trabalho(void) {
  int r;
  TRAVA();
  r = garante_i() &&
      (S.varrer || fioTrabOcup || trabPendente(agoraS()));
  DTRAVA();
  return r;
}

void farolsinal_teste_zera(void) {
  TRAVA();
  memset(&S.g, 0, sizeof S.g);
  S.perfil = 0; S.rec = 1; S.cur = 0; S.curSeq = 0;
  S.varrer = 0; S.varGer++; S.largadaVarrida = 0;
  S.nCred = S.nPend = S.nObra = S.obraProx = S.nAli = 0;
  S.nPed = S.nTempChk = S.nOri = S.oriProx = 0;
  S.sujo = S.fichaSujo = 0; S.ultGrav = 0;
  fioMorre = 0;                          // "reboot" tambem do fio lento
  DTRAVA();
}
