// Watch Together: matematica do sync — ver juntossync.h.
#include "juntossync.h"
#include <math.h>
#include <string.h>

#define EMA_ALFA      0.3
#define SALTO_MS      250.0
#define LAT_SEEK_PAD  250.0   // ms antes da primeira medida
#define LAT_PLAY_PAD  150.0

void jsr_zerar(JsRelogio *r) { memset(r, 0, sizeof *r); }

void jsr_amostra(JsRelogio *r, double c, double s, double agora) {
  double rtt = agora - c, off;
  int k, melhor = -1;
  if (rtt < 0 || rtt > 30000) return;   // eco de outro relogio ou de antes de dormir
  off = s - (c + rtt / 2.0);
  r->rtt[r->i] = rtt;
  r->off[r->i] = off;
  r->i = (r->i + 1) % JSR_AMOSTRAS;
  if (r->n < JSR_AMOSTRAS) r->n++;
  for (k = 0; k < r->n; k++) if (melhor < 0 || r->rtt[k] < r->rtt[melhor]) melhor = k;
  r->melhorRtt = r->rtt[melhor];
  // Rajada inicial (anel ainda nao cheio): vale a melhor amostra, sem media.
  // Depois, a media anda devagar e so um salto grande entra direto.
  if (!r->tem || r->n < JSR_AMOSTRAS || fabs(r->off[melhor] - r->offset) > SALTO_MS) r->offset = r->off[melhor];
  else r->offset += EMA_ALFA * (r->off[melhor] - r->offset);
  r->tem = 1;
}

double jsr_servidor(const JsRelogio *r, double agoraLocal) { return agoraLocal + r->offset; }
double jsr_local(const JsRelogio *r, double servidor) { return servidor - r->offset; }

double jsr_pos_alvo(const JsRel *rel, double agoraServidor) {
  double p;
  if (rel->estado != JSE_TOCANDO) return rel->pos0;
  p = rel->pos0 + (agoraServidor - rel->t0) / 1000.0;
  // Play agendado ainda no futuro: o alvo e o quadro de partida.
  return p < rel->pos0 ? rel->pos0 : p;
}

int jsr_estado_de(const char *n) {
  static const char *nomes[] = { "lobby", "tocando", "pausado", "esperando", "fim" };
  int i;
  if (!n) return -1;
  for (i = 0; i < 5; i++) if (!strcmp(n, nomes[i])) return i;
  return -1;
}

double jss_pos_local(double posAmostra, double tAmostraMs, double agoraMs, int tocando, double taxa) {
  double dt;
  if (!tocando) return posAmostra;
  dt = agoraMs - tAmostraMs;
  if (dt < 0) dt = 0;
  if (dt > 2000) dt = 2000;   // backend parou de falar: nao inventa mais de 2 s
  return posAmostra + dt / 1000.0 * taxa;
}

void jss_zerar(JsSync *s, char nivel) {
  memset(s, 0, sizeof *s);
  s->nivel = nivel == 'A' ? 'A' : 'B';
  s->latSeekMs = LAT_SEEK_PAD;
  s->latPlayMs = LAT_PLAY_PAD;
}

static double media(double atual, int n, double nova) {
  return n == 0 ? nova : atual + EMA_ALFA * (nova - atual);
}

void jss_seek_concluiu(JsSync *s, double ms, double residuo) {
  if (ms >= 0 && ms < 15000) { s->latSeekMs = media(s->latSeekMs, s->nLatSeek, ms); s->nLatSeek++; }
  // Caiu ATRAS do alvo (keyframe): a margem cresce ate cobrir o pior caso
  // visto; caiu a frente, ela encolhe devagar (a frente e barato: segura).
  if (residuo < -0.15) { double q = -residuo + 0.25; if (q > s->margemSeek) s->margemSeek = q > 5.0 ? 5.0 : q; }
  else if (residuo > 0.6) s->margemSeek *= 0.85;
  // Nivel B, MKV/HLS que caiu no keyframe longe do alvo: um segundo seek curto,
  // uma vez so (6.5). Nunca depois de um seek que ja era o segundo.
  if (s->nivel == 'B' && fabs(residuo) > 1.0 && s->residualOk >= 0) s->residualOk = 1;
}

void jss_play_concluiu(JsSync *s, double ms) {
  if (ms < 0 || ms > 10000) return;
  s->latPlayMs = media(s->latPlayMs, s->nLatPlay, ms);
  s->nLatPlay++;
}

double jss_instante_play(const JsSync *s, const JsRelogio *r, double t0Servidor) {
  return jsr_local(r, t0Servidor) - s->latPlayMs;
}

int jss_vel_medida(JsSync *s, double agoraMs, double posAgora) {
  double dt = (agoraMs - s->velT0) / 1000.0, taxa, pedida = s->velCent / 100.0;
  if (dt < 2.0 || s->velCent == 100) return 0;   // janela curta demais para concluir
  taxa = (posAgora - s->velP0) / dt;
  if (fabs(taxa - 1.0) < 0.012 && fabs(pedida - 1.0) >= 0.04) s->velFalhas++;
  else if (fabs(taxa - pedida) < 0.015) s->velFalhas = 0;
  return s->velFalhas >= 2;
}

static void podar(JsSync *s, double agora) {
  int i, k = 0;
  for (i = 0; i < s->nSeeks; i++) if (agora - s->seeks[i] < 60000.0) s->seeks[k++] = s->seeks[i];
  s->nSeeks = k;
}

JsAcao jss_decidir(JsSync *s, double agora, double posLocal, double posAlvo, int valido) {
  JsAcao a;
  double d = posLocal - posAlvo, ad = fabs(d), tol;
  memset(&a, 0, sizeof a);
  if (!valido) {
    if (s->velAte) { s->velAte = 0; a.tipo = JSA_VELOCIDADE; a.cent = 100; }
    return a;
  }
  if (s->velAte) {
    // Empurrao em curso: termina no prazo, quando passa do ponto, ou vira seek
    // se o desvio cresceu (a velocidade nao esta dando conta).
    int passou = (s->velCent < 100 && d < -JSS_ZONA_A) || (s->velCent > 100 && d > JSS_ZONA_A);
    if (ad <= JSS_VEL_MAX && agora < s->velAte && !passou) return a;
    s->velAte = 0;
    if (ad <= JSS_VEL_MAX) { a.tipo = JSA_VELOCIDADE; a.cent = 100; return a; }
  }
  // O frouxo e um recuo, nao uma sentenca: dois minutos sem loop e volta ao normal.
  if (s->frouxo && agora >= s->frouxoAte) s->frouxo = 0;
  tol = s->frouxo ? JSS_FROUXO : (s->nivel == 'A' ? JSS_ZONA_A : JSS_ZONA_B);
  // Segurar nao espera o cooldown do seek nem afrouxa: e preciso (a latencia
  // de play e medida) e nao entra em loop de keyframe. Tem o proprio intervalo.
  if (s->nivel == 'B' && d > JSS_ZONA_B && d <= JSS_SEGURAR_MAX && agora >= s->seguraAte) {
    a.tipo = JSA_SEGURAR;
    a.ms = d * 1000.0;
    s->seguraAte = agora + a.ms + 1500.0;
    if (s->cooldownAte < s->seguraAte) s->cooldownAte = s->seguraAte;
    a.aviso = d > JSS_AVISO_SEEK;
    return a;
  }
  if (s->residualOk == 1 && d < -1.0) {
    s->residualOk = -1;     // o proximo concluiu nao libera outro
    a.tipo = JSA_SEEK;
    a.pos = posAlvo + s->latSeekMs / 1000.0 + s->margemSeek;
    if (a.pos < 0) a.pos = 0;
    return a;
  }
  if (agora < s->cooldownAte) return a;
  if (ad < tol) return a;
  if (s->nivel == 'A' && !s->frouxo && ad <= JSS_VEL_MAX) {
    s->velCent = d > 0 ? JSS_VEL_LENTA : JSS_VEL_RAPIDA;
    // Corrige `ad` segundos a 5% de diferenca: ad / 0,05 segundos.
    s->velAte = agora + ad / 0.05 * 1000.0;
    s->velT0 = agora;
    s->velP0 = posLocal;
    a.tipo = JSA_VELOCIDADE;
    a.cent = s->velCent;
    return a;
  }
  podar(s, agora);
  if (s->nSeeks >= JSS_SEEKS_MIN) {
    // Seeks demais em um minuto: a fonte provavelmente pula para keyframe e o
    // seek nunca acerta. Afrouxa em vez de entrar em loop.
    if (!s->frouxo) { s->frouxo = 1; a.frouxo = 1; }
    s->frouxoAte = agora + JSS_FROUXO_MS;
    return a;
  }
  s->seeks[s->nSeeks++] = agora;
  s->cooldownAte = agora + JSS_COOLDOWN_MS;
  if (s->residualOk == -1) s->residualOk = 0;
  a.tipo = JSA_SEEK;
  // O alvo anda enquanto o seek acontece: pede onde ele VAI estar. No nivel B,
  // um pouco alem (margem do keyframe), para cair a frente e segurar.
  a.pos = posAlvo + s->latSeekMs / 1000.0 + (s->nivel == 'B' ? s->margemSeek : 0.0);
  if (a.pos < 0) a.pos = 0;
  a.aviso = ad > JSS_AVISO_SEEK;
  return a;
}
