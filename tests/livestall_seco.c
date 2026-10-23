// StallGuard de HLS: fonte que seca reabre ANTES da imagem congelar (src/livestall.h).
#include <assert.h>
#include <stdio.h>
#include "../src/livestall.h"

// Canal ao vivo: toca de t=0, o provedor para de mandar segmentos quando o buffer
// chega a `fimMorto` s (com `pos` em 1 s/s). Devolve em *congelado os ms entre a
// imagem parar (pos == fimMorto) e o pedido de reabrir (negativo = reabriu antes).
static int simular(int comFim, double fimMorto, long *congelado) {
  NvLiveStall s;
  unsigned t;
  nv_ls_zerar(&s);
  for (t = 0; t < 200000; t += 500) {
    double pos = t / 1000.0 + 0.5;
    double fim = pos < fimMorto ? (pos + 30.0 < fimMorto ? pos + 30.0 : fimMorto) : fimMorto;
    int r;
    if (pos > fimMorto) pos = fimMorto;
    r = nv_ls_passo_buf(&s, t + 1000, pos, 1, 0, 0, 0, comFim ? fim : 0.0);
    if (r == NV_LS_AGENDOU) {
      *congelado = (long)(t + 1000 + nv_ls_espera_ms(1)) - (long)((fimMorto - 0.5) * 1000) - 1000;
      return 1;
    }
  }
  return 0;
}

int main(void) {
  long sem = 0, com = 0;
  NvLiveStall s;
  unsigned t;
  double p;

  // Fonte seca com 30 s de reserva: sem fim de buffer so reage 8 s depois de congelar;
  // com ele reabre antes da imagem parar.
  assert(simular(0, 60.0, &sem) && simular(1, 60.0, &com));
  printf("livestall_seco: imagem congelada antes de reabrir: %ld ms -> %ld ms\n", sem, com);
  assert(sem >= (long)NV_LS_PARADO_MS);
  assert(com <= 0);

  // Segmento de 10 s no "live edge": o fim do buffer cresce a cada 10 s, nunca dispara.
  nv_ls_zerar(&s);
  for (t = 1000, p = 1.0; t < 300000; t += 500, p += 0.5) {
    double fim = 10.0 * (int)(p / 10.0) + 10.0;     // ate 10 s a frente, em degraus de 10 s
    assert(nv_ls_passo_buf(&s, t, p, 1, 0, 0, 0, fim) == NV_LS_NADA);
  }

  // Reserva ainda grande (buffer cheio, fetch parado pela plataforma): nao dispara.
  nv_ls_zerar(&s);
  for (t = 1000, p = 1.0; t < 12000; t += 500, p += 0.5)
    assert(nv_ls_passo_buf(&s, t, p, 1, 0, 0, 0, 39.0) == NV_LS_NADA);
  assert(nv_ls_passo_buf(&s, 30000, 25.0, 1, 0, 0, 0, 39.0) == NV_LS_NADA);

  // Fim do buffer desconhecido (0): o guarda nem arma.
  nv_ls_zerar(&s);
  for (t = 1000, p = 1.0; t < 100000; t += 500, p += 0.5)
    assert(nv_ls_passo_buf(&s, t, p, 1, 0, 0, 0, 0.0) == NV_LS_NADA);

  // Pausa da pessoa nunca conta, por mais que dure.
  nv_ls_zerar(&s);
  nv_ls_passo_buf(&s, 1000, 5.0, 1, 0, 0, 0, 6.0);
  nv_ls_passo_buf(&s, 1500, 5.6, 1, 0, 0, 0, 6.0);
  for (t = 2000; t < 100000; t += 1000) assert(nv_ls_passo_buf(&s, t, 5.6, 1, 1, 0, 0, 6.0) == NV_LS_NADA);
  assert(nv_ls_passo_buf(&s, t, 5.6, 1, 0, 0, 0, 6.0) == NV_LS_NADA);

  // Esgota igual ao vigia de sempre: NV_LS_MAX reaberturas e DESISTIR.
  nv_ls_zerar(&s);
  nv_ls_passo_buf(&s, 1000, 1.0, 1, 0, 0, 0, 2.0);
  nv_ls_passo_buf(&s, 1500, 1.6, 1, 0, 0, 0, 2.0);
  t = 1500;
  for (int n = 1; n <= NV_LS_MAX; n++) {
    t += NV_LS_SEMDADO_MS;
    assert(nv_ls_passo_buf(&s, t, 1.9, 1, 0, 0, 0, 2.0) == NV_LS_AGENDOU && s.tentativa == n);
    t += nv_ls_espera_ms(n);
    assert(nv_ls_passo_buf(&s, t, 1.9, 1, 0, 0, 0, 2.0) == NV_LS_REABRIR);
    nv_ls_reaberto(&s, t);
    nv_ls_passo_buf(&s, t + 500, 0.5, 1, 0, 0, 0, 2.0);
    t += 1000; nv_ls_passo_buf(&s, t, 1.6, 1, 0, 0, 0, 2.0);
  }
  t += NV_LS_SEMDADO_MS;
  assert(nv_ls_passo_buf(&s, t, 1.9, 1, 0, 0, 0, 2.0) == NV_LS_DESISTIR);

  // Buffer recuperou durante o backoff: a reabertura pendente e cancelada
  // (revisao 205). Buffer parado em 17 s, posicao 15,5 s -> agenda; um segundo
  // depois o provedor voltou (buffer 47 s, 30,5 s de reserva): nao reabre.
  nv_ls_zerar(&s);
  nv_ls_passo_buf(&s, 1000, 1.0, 1, 0, 0, 0, 17.0);
  assert(nv_ls_passo_buf(&s, 16000, 15.5, 1, 0, 0, 0, 17.0) == NV_LS_AGENDOU);
  assert(nv_ls_passo_buf(&s, 17000, 16.5, 1, 0, 0, 0, 47.0) == NV_LS_NADA);
  assert(!s.pendente && s.tentativa == 0);
  for (t = 17500, p = 17.0; t < 60000; t += 500, p += 0.5)   // segue tocando
    assert(nv_ls_passo_buf(&s, t, p, 1, 0, 0, 0, p + 30.0) == NV_LS_NADA);

  // Decoder parado, EOF ou buffering: buffer crescendo nao cancela a recuperacao.
  for (int motivo = 0; motivo < 3; motivo++) {
    int reabriu = 0;
    nv_ls_zerar(&s);
    nv_ls_passo_buf(&s, 1000, 1.0, 1, 0, 0, 0, 10.0);
    nv_ls_passo_buf(&s, 2000, 2.0, 1, 0, 0, 0, 10.0);
    for (t = 3000; t <= 60000; t += 1000) {
      double fim = 10.0 * (t / 10000 + 1);
      int r = nv_ls_passo_buf(&s, t, 2.0, 1, 0, motivo == 1,
                             motivo == 2 ? NV_LS_PARADO_MS : 0, fim);
      if (r == NV_LS_REABRIR) { reabriu = 1; break; }
    }
    assert(reabriu && t == (motivo == 0 ? 11000u : 4000u));
    assert(s.tentativa == 1);
  }

  // Fonte seca junto com posicao parada/EOF: preservar o motivo de recuperacao.
  for (int eof = 0; eof <= 1; eof++) {
    nv_ls_zerar(&s);
    nv_ls_passo_buf(&s, 1000, 1.0, 1, 0, 0, 0, 3.0);
    assert(nv_ls_passo_buf(&s, 16000, 1.0, 1, 0, eof, 0, 3.0) == NV_LS_AGENDOU);
    assert(nv_ls_passo_buf(&s, 17000, 1.0, 1, 0, eof, 0, 30.0) == NV_LS_REABRIR);
    assert(s.tentativa == 1);
  }

  puts("livestall_seco: ok");
  return 0;
}
