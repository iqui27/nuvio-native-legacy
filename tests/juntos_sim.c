// SIMULADOR DE PARES DO WATCH TOGETHER (plano 8.5, item 2), em processo e
// deterministico: 4 motores de membro (src/juntosmotor.c, o MESMO codigo do
// app) contra uma sala falsa, com rede ruim por par e quatro players
// simulados de perfis diferentes. Dez minutos simulados em passos de 5 ms.
//
//   p0 Android  nivel A, velocidade anda, buffering aleatorio, rede 20-60 ms
//   p1 LG       nivel A, posicao lida a cada 200 ms, play lento, rede 80-200 ms, relogio +1 h
//   p2 .wgt     nivel B, keyframe de 2 s, seek de 800 ms, SEM sinal de buffering,
//               rede 150-300 ms +-80 ms
//   p3 .tpk     DIZ nivel A mas a velocidade nao anda: o motor tem de provar e cair para B
//
// Metrica: |posicao do par - posicao da sala| amostrada a cada 100 ms com a sala
// tocando, fora dos 5 s depois de cada comando e de cada travada do proprio par.
// Criterio do plano: nivel A p95 < 150 ms, nivel B p95 < 800 ms; ninguem preso
// esperando alem do prazo; todo seek alinhado (A em 150 ms, B em 800 ms) em
// menos de 12 s. Rodado com 20 sementes em 06/10; a fixa e a primeira.
//
// A sala falsa reproduz o que importa de servidor/juntos/src/logica.js (relogio,
// play agendado, pausa no quadro, seek, espera de 3 s com prazo). A regra
// completa do servidor e testada la (teste-logica.mjs, teste-juntos.mjs).
#include "juntosmotor.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define N 4
#define PASSO 5.0
#define DURACAO (10.0 * 60000.0)

static double agoraV;     // tempo verdadeiro (ms) = relogio do servidor
static unsigned semente = 2463534242u;
static double aleat(void) { semente ^= semente << 13; semente ^= semente >> 17; semente ^= semente << 5; return (semente % 1000000) / 1000000.0; }
static double entre(double a, double b) { return a + (b - a) * aleat(); }

// ---- rede -------------------------------------------------------------------------------
#define FILA 512
typedef struct { double em; char j[2600]; } Pac;
typedef struct { Pac p[FILA]; int ini, n; double ult; } Fila;
typedef struct { double a, b, jit; } Rede;
static Fila subida[N], descida[N];
static Rede rede[N];

static void por(Fila *f, const Rede *r, const char *j) {
  double em = agoraV + entre(r->a, r->b) + entre(-r->jit, r->jit);
  Pac *p;
  if (em < agoraV + 1) em = agoraV + 1;
  if (em < f->ult) em = f->ult;          // TCP: atrasa, nao embaralha
  f->ult = em;
  if (f->n == FILA) { printf("fila cheia\n"); exit(2); }
  p = &f->p[(f->ini + f->n) % FILA];
  p->em = em;
  snprintf(p->j, sizeof p->j, "%s", j);
  f->n++;
}
static const char *tirar(Fila *f) {
  static char buf[2600];
  if (!f->n || f->p[f->ini].em > agoraV) return NULL;
  snprintf(buf, sizeof buf, "%s", f->p[f->ini].j);
  f->ini = (f->ini + 1) % FILA;
  f->n--;
  return buf;
}

// ---- sala falsa -------------------------------------------------------------------------
typedef struct { char st[16]; double rtt, prep; double carregDesde; int alc; double alcAte; } MembroF;
static struct {
  int estado; double pos0, t0; int seq; char autor[8], op[12];
  MembroF m[N];
  int esperando[N]; double prazo;
  int nEsperas; double maxEspera, esperaDesde;
} S;

static double posSala(double t) { return S.estado == JSE_TOCANDO ? S.pos0 + (t - S.t0) / 1000.0 : S.pos0; }
static double lead(void) {
  double r = 0, p = 0; int i;
  for (i = 0; i < N; i++) { if (S.m[i].rtt > r) r = S.m[i].rtt; if (S.m[i].prep > p) p = S.m[i].prep; }
  return fmax(600.0, fmin(3000.0, fmax(2 * r, p + 150)));
}
static void difundir(void) {
  static const char *nomes[] = { "lobby", "tocando", "pausado", "esperando", "fim" };
  char b[2600], mem[1400] = "";
  int i, k = 0;
  for (i = 0; i < N; i++)
    k += snprintf(mem + k, sizeof mem - (size_t)k, "%s{\"id\":\"p%d\",\"nome\":\"P%d\",\"st\":\"%s\",\"anf\":%s,\"on\":true%s}",
                  i ? "," : "", i, i, S.m[i].st, i == 0 ? "true" : "false", S.m[i].alc ? ",\"alc\":true" : "");
  snprintf(b, sizeof b, "{\"t\":\"sala\",\"seq\":%d,\"s\":%.0f,\"rel\":{\"ep\":\"tt1\",\"estado\":\"%s\",\"pos0\":%.4f,"
           "\"t0\":%.0f,\"autor\":\"%s\",\"op\":\"%s\"},\"membros\":[%s],\"controle\":\"todos\",\"max\":5,"
           "\"cfg\":{\"ep\":\"tt1\",\"titulo\":\"T\",\"codigo\":\"AAAAAA\",\"espera\":20}}",
           S.seq, agoraV, nomes[S.estado], S.pos0, S.t0, S.autor, S.op, mem);
  for (i = 0; i < N; i++) por(&descida[i], &rede[i], b);
}
static int depurar;
static void mudar(int estado, double pos0, double t0, int autor, const char *op) {
  if (depurar) printf("%8.1f sala %-8s autor p%d pos0 %.2f t0+%.0f\n", agoraV / 1000, op, autor, pos0, t0 - agoraV);
  S.estado = estado; S.pos0 = pos0; S.t0 = t0; S.seq++;
  snprintf(S.autor, sizeof S.autor, "p%d", autor);
  snprintf(S.op, sizeof S.op, "%s", op);
  difundir();
}
static double numDe(const char *j, const char *k) {
  char pad[24]; const char *p;
  snprintf(pad, sizeof pad, "\"%s\":", k);
  p = strstr(j, pad);
  return p ? atof(p + strlen(pad)) : -1;
}
static void textoDe(const char *j, const char *k, char *d, size_t n) {
  char pad[24]; const char *p, *q;
  snprintf(pad, sizeof pad, "\"%s\":\"", k);
  d[0] = 0;
  p = strstr(j, pad);
  if (!p) return;
  p += strlen(pad);
  q = strchr(p, '"');
  if (q && (size_t)(q - p) < n) { memcpy(d, p, (size_t)(q - p)); d[q - p] = 0; }
}
static int ninguemEsperando(void) { int i; for (i = 0; i < N; i++) if (S.esperando[i]) return 0; return 1; }
static void servidor(int de, const char *j) {
  char t[16], op[16], st[16];
  textoDe(j, "t", t, sizeof t);
  if (!strcmp(t, "ping")) {
    char b[96];
    snprintf(b, sizeof b, "{\"t\":\"pong\",\"c\":%.0f,\"s\":%.0f}", numDe(j, "c"), agoraV);
    por(&descida[de], &rede[de], b);
  } else if (!strcmp(t, "ola") || !strcmp(t, "estado")) {
    double r = numDe(j, "rtt");
    if (r > 0) S.m[de].rtt = r;
    if (numDe(j, "prep") > 0) S.m[de].prep = fmin(2500, numDe(j, "prep"));
    if (t[0] == 'e') {
      textoDe(j, "st", st, sizeof st);
      snprintf(S.m[de].st, sizeof S.m[de].st, "%s", st);
      if (!strcmp(st, "carregando")) {
        if (S.m[de].alc) S.m[de].alcAte = 0;
        else if (S.estado == JSE_TOCANDO && !S.m[de].carregDesde) S.m[de].carregDesde = agoraV;
      } else {
        S.m[de].carregDesde = 0;
        if (!strcmp(st, "tocando") && S.m[de].alc && !S.m[de].alcAte) S.m[de].alcAte = agoraV + 30000;
        if ((!strcmp(st, "pronto") || !strcmp(st, "tocando")) && S.esperando[de]) {
          S.esperando[de] = 0;
          if (ninguemEsperando() && S.estado == JSE_ESPERANDO) {
            double d = agoraV - S.esperaDesde;
            if (d > S.maxEspera) S.maxEspera = d;
            mudar(JSE_TOCANDO, S.pos0, agoraV + lead(), de, "pronto");
          }
        }
      }
    }
  } else if (!strcmp(t, "cmd")) {
    double pos = numDe(j, "pos");
    textoDe(j, "op", op, sizeof op);
    if (!strcmp(op, "comecar") && S.estado == JSE_LOBBY) mudar(JSE_TOCANDO, S.pos0, agoraV + lead(), de, op);
    else if (!strcmp(op, "play") && (S.estado == JSE_PAUSADO || S.estado == JSE_ESPERANDO)) {
      memset(S.esperando, 0, sizeof S.esperando);
      mudar(JSE_TOCANDO, S.pos0, agoraV + lead(), de, op);
    } else if (!strcmp(op, "pausa") && (S.estado == JSE_TOCANDO || S.estado == JSE_ESPERANDO)) {
      double a = posSala(agoraV);
      memset(S.esperando, 0, sizeof S.esperando);
      mudar(JSE_PAUSADO, pos >= 0 && fabs(pos - a) < 5 ? pos : a, agoraV, de, op);
    } else if (!strcmp(op, "seek") && pos >= 0) {
      if (S.estado == JSE_TOCANDO) mudar(JSE_TOCANDO, pos, agoraV + lead(), de, op);
      else mudar(S.estado, pos, agoraV, de, op);
    }
  }
}
static void servidorTempo(void) {
  int i;
  for (i = 0; i < N; i++) {
    MembroF *m = &S.m[i];
    if (m->alcAte && agoraV >= m->alcAte) { m->alc = 0; m->alcAte = 0; }
    if (m->carregDesde && agoraV - m->carregDesde >= 3000) {
      m->carregDesde = 0;
      if (S.estado != JSE_TOCANDO && S.estado != JSE_ESPERANDO) continue;
      S.esperando[i] = 1;
      S.nEsperas++;
      if (S.estado == JSE_TOCANDO) {
        S.prazo = agoraV + 20000;
        S.esperaDesde = agoraV;
        mudar(JSE_ESPERANDO, posSala(agoraV), agoraV, i, "esperar");
      }
    }
  }
  if (S.estado == JSE_ESPERANDO && agoraV >= S.prazo) {
    for (i = 0; i < N; i++) if (S.esperando[i]) { S.esperando[i] = 0; S.m[i].alc = 1; S.m[i].alcAte = 0; }
    if (agoraV - S.esperaDesde > S.maxEspera) S.maxEspera = agoraV - S.esperaDesde;
    mudar(JSE_TOCANDO, S.pos0, agoraV + lead(), 0, "prazo");
  }
}

// ---- players simulados -----------------------------------------------------------------
typedef struct {
  double skew;                 // relogio local = verdadeiro + skew
  double pos; int quer, vel, pronto; double prontoEm;
  int buscando; double seekAte, seekAlvo;
  double playAte, bufAte, bufInicio;
  double keyframe, latSeek, latPlay, cadencia;
  int velOk, semSinalBuffer, buffering;
  double amostra, tAmostra, ultAmostraV;
  char nivel; const char *plat;
} SimP;
static SimP P[N];
static JsMotor M[N];

static double localDe(int i) { return agoraV + P[i].skew; }
static double spPos(void *u, double *t) { SimP *p = u; *t = p->tAmostra; return p->amostra; }
static int spTocando(void *u) {
  SimP *p = u;
  return p->pronto && p->quer && !p->buscando && agoraV >= p->playAte && agoraV >= p->bufAte;
}
static int spPronto(void *u) { return ((SimP *)u)->pronto; }
static unsigned spBuf(void *u) {
  SimP *p = u;
  if (p->semSinalBuffer || agoraV >= p->bufAte) return 0;
  return (unsigned)(agoraV - p->bufInicio);
}
static double spDur(void *u) { (void)u; return 7200.0; }
static void spPausar(void *u, int pausado) {
  SimP *p = u;
  if (!pausado && !p->quer) p->playAte = agoraV + p->latPlay;
  p->quer = !pausado;
}
static void spBuscar(void *u, double s) { SimP *p = u; p->buscando = 1; p->seekAte = agoraV + p->latSeek; p->seekAlvo = s; }
static void spVel(void *u, int c) { ((SimP *)u)->vel = c; }
static void spAvancar(SimP *p, double dt) {
  int i = (int)(p - P);
  if (!p->pronto && agoraV >= p->prontoEm) p->pronto = 1;
  if (p->buscando && agoraV >= p->seekAte) {
    p->pos = p->keyframe > 0 ? floor(p->seekAlvo / p->keyframe) * p->keyframe : p->seekAlvo;
    p->buscando = 0;
    if (p->quer) p->playAte = agoraV + 60;
  }
  if (spTocando(p)) p->pos += dt / 1000.0 * (p->velOk ? p->vel / 100.0 : 1.0);
  if (agoraV - p->ultAmostraV >= p->cadencia) { p->amostra = p->pos; p->tAmostra = localDe(i); p->ultAmostraV = agoraV; }
}

static int enviar(void *u, const char *j) { por(&subida[(int)(intptr_t)u], &rede[(int)(intptr_t)u], j); return 0; }

// ---- metrica --------------------------------------------------------------------------
#define AMOSTRAS 8000
static double erros[N][AMOSTRAS]; static int nErros[N];
static int cmpd(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }
static double p95(int i) {
  if (!nErros[i]) return 99;
  qsort(erros[i], (size_t)nErros[i], sizeof(double), cmpd);
  return erros[i][(int)(0.95 * (nErros[i] - 1))];
}

int main(void) {
  int i, falhas = 0, nSeeks = 0, nRajadas = 0, nPausas = 0, nivelB3 = 0;
  double ultimoCmd = 0, proxCmd = 30000, playEm = 0, seekEm = 0, piorAlinhar = 0;
  int seekSeq = 0;
  double proxAmostra = 0;
  JsPlayer jp = { spPos, spTocando, spPronto, spBuf, spDur, spPausar, spBuscar, spVel, NULL };
  const char *plats[N] = { "android", "lg", "wgt", "tpk" };
  char niveis[N] = { 'A', 'A', 'B', 'A' };
  depurar = getenv("JS_DEPURAR") != NULL;
  memset(&S, 0, sizeof S);
  S.estado = JSE_LOBBY;
  for (i = 0; i < N; i++) snprintf(S.m[i].st, sizeof S.m[i].st, "entrando");
  rede[0] = (Rede){ 20, 60, 10 };
  rede[1] = (Rede){ 80, 200, 40 };
  rede[2] = (Rede){ 150, 300, 80 };
  rede[3] = (Rede){ 40, 120, 30 };
  P[0] = (SimP){ .skew = 12345, .vel = 100, .prontoEm = 1500, .latSeek = 300, .latPlay = 150, .cadencia = 0, .velOk = 1 };
  P[1] = (SimP){ .skew = 3600000, .vel = 100, .prontoEm = 3000, .latSeek = 600, .latPlay = 350, .cadencia = 200, .velOk = 1 };
  P[2] = (SimP){ .skew = 777, .vel = 100, .prontoEm = 4000, .keyframe = 2.0, .latSeek = 800, .latPlay = 400, .cadencia = 0,
                 .semSinalBuffer = 1 };
  P[3] = (SimP){ .skew = 55, .vel = 100, .prontoEm = 2500, .latSeek = 500, .latPlay = 250, .cadencia = 0, .velOk = 0 };
  for (i = 0; i < N; i++) {
    char id[8];
    snprintf(id, sizeof id, "p%d", i);
    jp.u = &P[i];
    jsm_iniciar(&M[i], id, plats[i], "teste", niveis[i], &jp, enviar, (void *)(intptr_t)i);
    jsm_estado_app(&M[i], "carregando", 10);
    jsm_conectou(&M[i], localDe(i));
  }
  for (agoraV = 0; agoraV < DURACAO; agoraV += PASSO) {
    const char *j;
    for (i = 0; i < N; i++) {
      spAvancar(&P[i], PASSO);
      // Buffering aleatorio no Android (e no .wgt, que nem avisa): 1-5 s a cada ~2 min.
      if ((i == 0 || i == 2) && agoraV > 20000 && agoraV >= P[i].bufAte && aleat() < PASSO / 120000.0) {
        P[i].bufInicio = agoraV;
        P[i].bufAte = agoraV + entre(1000, 5000);
      }
      while ((j = tirar(&subida[i]))) servidor(i, j);
    }
    servidorTempo();
    for (i = 0; i < N; i++) {
      while ((j = tirar(&descida[i]))) jsm_receber(&M[i], j, localDe(i));
      jsm_passo(&M[i], localDe(i));
      { JsEvento e; while (jsm_evento(&M[i], &e)) if (e.tipo == JSV_NIVEL_B && i == 3) nivelB3 = 1; }
    }
    // O roteiro: comecar, depois um comando a cada 40-90 s de um par qualquer.
    if (agoraV >= 6000 && S.estado == JSE_LOBBY && !S.seq) { jsm_cmd(&M[0], "comecar", -1, NULL, localDe(0)); ultimoCmd = agoraV; }
    if (playEm && agoraV >= playEm) { int q = (int)entre(0, N - 0.01); jsm_cmd(&M[q], "play", -1, NULL, localDe(q)); playEm = 0; ultimoCmd = agoraV; }
    if (agoraV >= proxCmd && S.estado == JSE_TOCANDO && !playEm) {
      int q = (int)entre(0, N - 0.01), op = (int)entre(0, 2.99);
      double alvo = posSala(agoraV);
      if (op == 0) { jsm_cmd(&M[q], "pausa", -1, NULL, localDe(q)); playEm = agoraV + entre(3000, 8000); nPausas++; }
      else if (op == 1) {
        double novo = fmax(0, alvo + (aleat() < 0.5 ? -1 : 1) * entre(10, 300));
        jsm_cmd(&M[q], "seek", novo, NULL, localDe(q)); seekEm = agoraV; seekSeq = S.seq; nSeeks++;
      } else {
        // D-pad: 5 toques de +10 s, 250 ms entre eles; a sala recebe um so.
        int k; double p = alvo;
        for (k = 0; k < 5; k++) { p += 10; spBuscar(&P[q], p); jsm_seek_rajada(&M[q], p, localDe(q)); }
        seekEm = agoraV; seekSeq = S.seq; nRajadas++;
      }
      ultimoCmd = agoraV;
      proxCmd = agoraV + entre(40000, 90000);
    }
    // Tempo para alinhar depois de um seek (todos dentro da tolerancia do nivel).
    if (seekEm && S.seq > seekSeq && S.estado == JSE_TOCANDO && agoraV > S.t0) {
      int todos = 1;
      double alvo = posSala(agoraV);
      for (i = 0; i < N; i++) if (fabs(P[i].pos - alvo) > (M[i].sync.nivel == 'A' ? 0.15 : 0.8)) todos = 0;
      if (depurar && fmod(agoraV - seekEm, 1000.0) < PASSO) {
        printf("   alinhando %.1f s:", (agoraV - seekEm) / 1000);
        for (i = 0; i < N; i++) printf(" p%d %+.0f", i, (P[i].pos - alvo) * 1000);
        printf("\n");
      }
      if (todos) { if (agoraV - seekEm > piorAlinhar) piorAlinhar = agoraV - seekEm; seekEm = 0; }
      else if (agoraV - seekEm > 20000) { piorAlinhar = 20000; seekEm = 0; }
    }
    if (depurar && fmod(agoraV, 1000.0) < PASSO) {
      printf("%8.1f %-9s", agoraV / 1000, S.estado == JSE_TOCANDO ? "tocando" : "outro");
      for (i = 0; i < N; i++) printf(" p%d %+6.0f%s%s", i, (P[i].pos - posSala(agoraV)) * 1000, M[i].stEnviado[0] == 'c' ? "c" : " ",
                                    M[i].velCent != 100 ? "v" : " ");
      printf("\n");
    }
    if (agoraV >= proxAmostra) {
      proxAmostra = agoraV + 100;
      if (S.estado == JSE_TOCANDO && agoraV > S.t0 + 5000 && agoraV > ultimoCmd + 5000)
        for (i = 0; i < N; i++)
          if (P[i].pronto && agoraV > P[i].bufAte + 5000 && nErros[i] < AMOSTRAS)
            erros[i][nErros[i]++] = fabs(P[i].pos - posSala(agoraV));
    }
  }
  for (i = 0; i < N; i++) {
    double q = p95(i), lim = M[i].sync.nivel == 'A' ? 0.150 : 0.800;
    printf("  p%d %-8s nivel %c  p95 %4.0f ms  (limite %3.0f, %d amostras, latSeek %.0f, latPlay %.0f%s)\n",
           i, plats[i], M[i].sync.nivel, q * 1000, lim * 1000, nErros[i], M[i].sync.latSeekMs, M[i].sync.latPlayMs,
           M[i].sync.frouxo ? ", frouxo" : "");
    if (q > lim || nErros[i] < 1000) { printf("FALHA p%d\n", i); falhas++; }
  }
  printf("  comandos: %d pausas, %d seeks, %d rajadas; esperas %d (a maior %.1f s); pior alinhamento depois de seek %.1f s\n",
         nPausas, nSeeks, nRajadas, S.nEsperas, S.maxEspera / 1000, piorAlinhar / 1000);
  if (!nivelB3 || M[3].sync.nivel != 'B') { printf("FALHA: o .tpk sem velocidade nao caiu para o nivel B\n"); falhas++; }
  if (M[2].sync.nivel != 'B' || M[0].sync.nivel != 'A' || M[1].sync.nivel != 'A') { printf("FALHA: nivel trocado\n"); falhas++; }
  if (S.maxEspera > 20500) { printf("FALHA: espera passou do prazo\n"); falhas++; }
  // Nivel A fecha o resto do desvio por velocidade (5%): 0,4 s levam 8 s, sem
  // ninguem ver. O teto pega o que seria um loop ou um seek perdido.
  if (piorAlinhar > 12000) { printf("FALHA: seek demorou a alinhar\n"); falhas++; }
  if (nPausas + nSeeks + nRajadas < 6) { printf("FALHA: roteiro curto\n"); falhas++; }
  if (falhas) return 1;
  printf("juntos_sim: 4 pares, 10 min simulados com rede ruim: dentro do criterio\n");
  return 0;
}
