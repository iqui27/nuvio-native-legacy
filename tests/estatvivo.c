// ESTATISTICAS AO VIVO (src/estatvivo.c): a conta de cada plataforma e a
// origem escrita do lado, sem SDL nem rede. Ver tests/estatvivo.sh.
#include "estatvivo.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static EvEntrada base(void) {
  EvEntrada in;
  memset(&in, 0, sizeof in);
  video_stats_vazio(&in.vs);
  in.tocando = 1;
  return in;
}

int main(void) {
  EvEstado e;
  EvEntrada in;
  char b[32];
  int i;

  // ANDROID: bytes contados -> Medido; 5,35 MB em 1 s = 42,8 Mbps.
  ev_zerar(&e);
  in = base();
  in.vs.bytes = 0; in.vs.bufferSeg = 38; in.vs.bufferAlvoSeg = 50; in.vs.conexoes = 4;
  in.vs.primeiroByteMs = 212; in.vs.quadros = 41280; in.vs.quadrosPerdidos = 3;
  snprintf(in.vs.host, sizeof in.vs.host, "cdn-17.exemplo.net");
  in.taxaKbps = 38000;
  ev_amostra(&e, 1000, &in, 21, 0);
  assert(e.kbps == -1);                       // a 1a amostra so e a base
  in.vs.bytes = 5350000;
  ev_amostra(&e, 2000, &in, 21, 0);
  assert(e.kbps == 42800 && e.velOrigem == EVO_MEDIDO);
  assert(e.bufOrigem == EVO_MEDIDO && e.bufSeg == 38 && e.bufMb > 180 && e.bufMb < 181);
  assert(e.servOrigem == EVO_MEDIDO && e.conexoes == 4 && !strcmp(e.host, "cdn-17.exemplo.net"));
  assert(e.quadrosOrigem == EVO_MEDIDO && e.perdidos == 3);
  // contador que voltou (reabriu): nao vira numero negativo
  in.vs.bytes = 100;
  ev_amostra(&e, 3000, &in, 21, 0);
  assert(e.kbps == 42800 && e.nHist == 1);
  in.vs.bytes = 100 + 7650000;                 // 61,2 Mbps
  ev_amostra(&e, 4000, &in, 21, 0);
  assert(e.picoKbps == 61200 && e.mediaKbps == (42800 + 61200) / 2 && e.nHist == 2);
  // pausado: o numero aparece, mas nao entra no historico
  in.tocando = 0; in.vs.bytes += 1000;
  ev_amostra(&e, 5000, &in, 21, 0);
  assert(e.nHist == 2 && e.kbps == 8);

  // WEBOS: segundos de buffer -> velocidade ESTIMADA pela taxa do arquivo.
  ev_zerar(&e);
  in = base();
  in.vs.bufferSeg = 20; in.pos = 100; in.taxaKbps = 20000;
  snprintf(in.vs.host, sizeof in.vs.host, "cdn.x");
  ev_amostra(&e, 0, &in, 0, 0);
  in.pos = 101; in.vs.bufferSeg = 21;          // baixou 2 s de video em 1 s
  ev_amostra(&e, 1000, &in, 0, 0);
  assert(e.kbps == 40000 && e.velOrigem == EVO_ESTIMADA);
  assert(e.bufOrigem == EVO_MEDIDO && e.servOrigem == EVO_LINK && e.quadrosOrigem == EVO_SEM);
  in.pos = 20; in.vs.bufferSeg = 3;            // busca para tras: sem numero novo
  ev_amostra(&e, 2000, &in, 0, 0);
  assert(e.kbps == 40000 && e.nHist == 1);
  // sem a taxa do arquivo nao ha como estimar
  ev_zerar(&e);
  in.taxaKbps = 0;
  ev_amostra(&e, 0, &in, 0, 0);
  ev_amostra(&e, 1000, &in, 0, 0);
  assert(e.kbps == -1);

  // SAMSUNG: so % -> "So em %", velocidade estimada pela reproducao.
  ev_zerar(&e);
  in = base();
  in.vs.bufferPct = 62; in.taxaKbps = 36000; in.pos = 10;
  ev_amostra(&e, 0, &in, 0, 0);
  in.pos = 11;
  ev_amostra(&e, 1000, &in, 0, 0);
  assert(e.kbps == 36000 && e.velOrigem == EVO_ESTIMADA && e.bufOrigem == EVO_SO_PCT && e.bufPct == 62);
  assert(e.servOrigem == EVO_SEM && e.quadrosOrigem == EVO_SEM);
  in.bufferando = 1; in.pos = 11;              // parado no buffer: mantem o ultimo
  ev_amostra(&e, 2000, &in, 0, 0);
  assert(e.kbps == 36000 && e.nHist == 1);

  // RECONEXOES e ERROS: cada subida conta uma vez, com a hora.
  ev_zerar(&e);
  in = base();
  in.reconectando = 1;
  for (i = 0; i < 3; i++) ev_amostra(&e, (unsigned)i * 1000, &in, 21, 12);
  in.reconectando = 0;
  ev_amostra(&e, 4000, &in, 21, 13);
  assert(e.reconexoes == 1 && e.reconHora == 21 && e.reconMin == 12 && e.erros == 0);
  in.falhou = 1;
  ev_amostra(&e, 5000, &in, 21, 13);
  ev_amostra(&e, 6000, &in, 21, 13);
  assert(e.erros == 1);

  // HISTORICO: anel de EV_HIST.
  ev_zerar(&e);
  in = base(); in.vs.bytes = 0;
  for (i = 0; i < EV_HIST + 10; i++) { in.vs.bytes += 125000; ev_amostra(&e, (unsigned)i * 1000, &in, 0, 0); }
  assert(e.nHist == EV_HIST && e.hist[EV_HIST - 1] == 1000);

  // TAXA DO ARQUIVO e formatos
  assert(ev_taxa_arquivo(0, 0, 7200) == 0);
  assert(ev_taxa_arquivo(0, 40000, 30) == 0);
  assert(ev_taxa_arquivo(34200000000ULL, 0, 7200) == 38000);
  ev_fmt_mbps(b, sizeof b, 42800, ','); assert(!strcmp(b, "42,8"));
  ev_fmt_mbps(b, sizeof b, 412000, ','); assert(!strcmp(b, "412"));
  ev_fmt_mbps(b, sizeof b, 950, '.'); assert(!strcmp(b, "1.0"));
  ev_fmt_mbps(b, sizeof b, -1, '.'); assert(!strcmp(b, "—"));
  ev_fmt_pct2(b, sizeof b, 3, 41280, ','); assert(!strcmp(b, "0,01"));
  ev_fmt_milhar(b, sizeof b, 41280); assert(!strcmp(b, "41 280"));
  ev_fmt_milhar(b, sizeof b, 999); assert(!strcmp(b, "999"));
  ev_fmt_milhar(b, sizeof b, 1234567); assert(!strcmp(b, "1 234 567"));
  assert(!strcmp(ev_origem_rotulo(EVO_SO_PCT), "Só em %"));
  puts("PASS: estatisticas ao vivo");
  return 0;
}
