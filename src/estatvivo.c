// Ver estatvivo.h.
#include "estatvivo.h"
#include <stdio.h>
#include <string.h>

const char *ev_origem_rotulo(EvOrigem o) {
  switch (o) {
    case EVO_MEDIDO: return "Medido";
    case EVO_ESTIMADA: return "Estimada";
    case EVO_SO_PCT: return "Só em %";
    case EVO_ADDON: return "Do add-on";
    case EVO_LINK: return "Do link";
    default: return "Sem dado";
  }
}

void ev_zerar(EvEstado *e) {
  memset(e, 0, sizeof *e);
  e->kbps = -1;
  e->bufSeg = e->bufMb = -1.0;
  e->bufPct = -1;
  e->conexoes = e->primeiroByteMs = -1;
  e->perdidos = e->quadros = -1;
}

int ev_taxa_arquivo(unsigned long long bytes, long tamanhoMB, double duracaoS) {
  double bits;
  if (duracaoS < 60.0) return 0;   // trailer, aviso ou duracao ainda desconhecida
  if (bytes) bits = (double)bytes * 8.0;
  else if (tamanhoMB > 0) bits = (double)tamanhoMB * 1048576.0 * 8.0;
  else return 0;
  return (int)(bits / duracaoS / 1000.0 + 0.5);
}

// A conta da velocidade, por ordem de confianca. Devolve kbps (>= 0) ou -1 e
// diz a origem.
static int velocidade(EvEstado *e, unsigned agoraMs, const EvEntrada *in, int *origem) {
  double dt = (double)(agoraMs - e->tUlt) / 1000.0;
  *origem = EVO_SEM;
  if (!e->temUlt || dt < 0.25 || dt > 10.0) return -1;
  // 1. BYTES CONTADOS (Android). Contador que voltou = abertura nova
  //    (reconexao, troca de fonte): a linha de base recomeca.
  if (in->vs.bytes >= 0) {
    *origem = EVO_MEDIDO;
    if (e->bytesUlt < 0 || in->vs.bytes < e->bytesUlt) return -1;
    return (int)((double)(in->vs.bytes - e->bytesUlt) * 8.0 / 1000.0 / dt + 0.5);
  }
  if (in->taxaKbps <= 0) return -1;
  // 2. BUFFER EM SEGUNDOS (webOS): o fim do que ja foi baixado andou quantos
  //    segundos de video neste intervalo; vezes a taxa do arquivo.
  if (in->vs.bufferSeg >= 0.0) {
    double av = in->pos + in->vs.bufferSeg, d = av - e->avancoUlt;
    *origem = EVO_ESTIMADA;
    if (e->avancoUlt < 0.0 || d < 0.0 || d > dt * 60.0) return -1;   // busca: recomeca
    return (int)(d / dt * (double)in->taxaKbps + 0.5);
  }
  // 3. SO A REPRODUCAO (Samsung): tocando sem parar, a rede entregou pelo
  //    menos o que foi tocado. Parado no buffer nao da numero.
  *origem = EVO_ESTIMADA;
  if (!in->tocando || in->bufferando || e->posUlt < 0.0) return -1;
  { double d = in->pos - e->posUlt;
    if (d < 0.0 || d > dt * 4.0) return -1;
    return (int)(d / dt * (double)in->taxaKbps + 0.5); }
}

void ev_amostra(EvEstado *e, unsigned agoraMs, const EvEntrada *in, int hora, int minuto) {
  int origem, k;
  if (!e || !in) return;
  k = velocidade(e, agoraMs, in, &origem);
  if (k > 4000000) k = -1;   // 4 Gbps: conta errada, nao velocidade
  if (origem != EVO_SEM) e->velOrigem = origem;
  if (k >= 0) {
    e->kbps = k;
    // Pausado nao entra no historico nem na media: o buffer cheio para a rede
    // de proposito, e isso nao e a velocidade da conexao.
    if (in->tocando) {
      if (e->nHist == EV_HIST) { memmove(e->hist, e->hist + 1, sizeof e->hist - sizeof *e->hist); e->nHist--; }
      e->hist[e->nHist++] = k;
      if (k > e->picoKbps) e->picoKbps = k;
      e->somaKbps += k;
      e->nMedia++;
      e->mediaKbps = (int)(e->somaKbps / e->nMedia);
    }
  } else if (e->velOrigem == EVO_SEM) e->kbps = -1;
  e->tUlt = agoraMs;
  e->temUlt = 1;
  e->bytesUlt = in->vs.bytes;
  e->avancoUlt = in->vs.bufferSeg >= 0.0 ? in->pos + in->vs.bufferSeg : -1.0;
  e->posUlt = in->pos;

  // BUFFER
  e->bufAlvo = in->vs.bufferAlvoSeg;
  if (in->vs.bufferSeg >= 0.0) {
    e->bufSeg = in->vs.bufferSeg;
    e->bufPct = -1;
    e->bufMb = in->taxaKbps > 0 ? in->vs.bufferSeg * in->taxaKbps / 8.0 / 1000.0 : -1.0;
    e->bufOrigem = EVO_MEDIDO;
  } else if (in->vs.bufferPct >= 0) {
    e->bufSeg = e->bufMb = -1.0;
    e->bufPct = in->vs.bufferPct;
    e->bufOrigem = EVO_SO_PCT;
  } else {
    e->bufSeg = e->bufMb = -1.0;
    e->bufPct = -1;
    e->bufOrigem = EVO_SEM;
  }

  // SERVIDOR
  snprintf(e->host, sizeof e->host, "%s", in->vs.host);
  e->conexoes = in->vs.conexoes;
  e->primeiroByteMs = in->vs.primeiroByteMs;
  e->servOrigem = in->vs.conexoes >= 0 ? EVO_MEDIDO : in->vs.host[0] ? EVO_LINK : EVO_SEM;

  // ERROS E RECONEXOES: cada SUBIDA conta uma vez.
  if (in->reconectando && !e->estavaRecon) { e->reconexoes++; e->reconHora = hora; e->reconMin = minuto; }
  if (in->falhou && !e->estavaFalhou) e->erros++;
  e->estavaRecon = in->reconectando != 0;
  e->estavaFalhou = in->falhou != 0;

  // QUADROS
  e->perdidos = in->vs.quadrosPerdidos;
  e->quadros = in->vs.quadros;
  e->quadrosOrigem = in->vs.quadros >= 0 && in->vs.quadrosPerdidos >= 0 ? EVO_MEDIDO : EVO_SEM;
}

void ev_fmt_mbps(char *dst, size_t n, int kbps, char sep) {
  if (!dst || !n) return;
  if (kbps < 0) { snprintf(dst, n, "—"); return; }
  if (kbps >= 99950) { snprintf(dst, n, "%d", (kbps + 500) / 1000); return; }
  { int d = (kbps + 50) / 100;   // decimos de Mbps
    snprintf(dst, n, "%d%c%d", d / 10, sep, d % 10); }
}

void ev_fmt_pct2(char *dst, size_t n, long long parte, long long todo, char sep) {
  long long c;
  if (!dst || !n) return;
  if (todo <= 0 || parte < 0) { snprintf(dst, n, "0%c00", sep); return; }
  c = (parte * 10000LL + todo / 2) / todo;   // centesimos de porcento
  snprintf(dst, n, "%lld%c%02lld", c / 100, sep, c % 100);
}

void ev_fmt_milhar(char *dst, size_t n, long long v) {
  char t[32];
  int len, i, o = 0;
  if (!dst || !n) return;
  if (v < 0) v = 0;
  snprintf(t, sizeof t, "%lld", v);
  len = (int)strlen(t);
  for (i = 0; i < len && o + 2 < (int)n; i++) {
    if (i && (len - i) % 3 == 0) dst[o++] = ' ';
    dst[o++] = t[i];
  }
  dst[o] = 0;
}
