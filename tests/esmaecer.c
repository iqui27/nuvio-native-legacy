// Protecao de OLED (esmaecer.h): estagios por tempo, tecla que acorda e e
// consumida, nunca durante a reproducao, e o fator de brilho do OSD.
#define ESMAECER_SEM_GFX
#include "../src/esmaecer.c"
#include <assert.h>
#include <stdio.h>

#define MIN(n) ((unsigned)(n) * 60u * 1000u)

static void quadros(unsigned de, unsigned ate, int reproduzindo) {
  unsigned t;
  for (t = de; t <= ate; t += 100) esmaecer_quadro(t, 0.1f, reproduzindo);
}

int main(void) {
  // Tempos -> estagio (5 min: veu aos 5, escuro 3 min depois).
  assert(esmaecer_estagio_para(MIN(4) + 59000u, 4, ESM_ESTILO_ESCURECER) == ESM_ACESO);
  assert(esmaecer_estagio_para(MIN(5), 4, ESM_ESTILO_ESCURECER) == ESM_VEU);
  assert(esmaecer_estagio_para(MIN(8) - 1u, 4, ESM_ESTILO_ESCURECER) == ESM_VEU);
  assert(esmaecer_estagio_para(MIN(8), 4, ESM_ESTILO_ESCURECER) == ESM_ESCURO);
  assert(esmaecer_estagio_para(MIN(2), 3, ESM_ESTILO_ESCURECER) == ESM_VEU && esmaecer_estagio_para(MIN(10), 5, ESM_ESTILO_ESCURECER) == ESM_VEU);
  assert(esmaecer_estagio_para(MIN(1000), 0, ESM_ESTILO_ESCURECER) == ESM_ACESO);   // desligado
  assert(esmaecer_estagio_para(MIN(1000), 9, ESM_ESTILO_ESCURECER) == ESM_ACESO);   // escolha invalida
  // 30 s e 1 min (2.0, "ate 30 s").
  assert(esmaecer_estagio_para(29999u, 1, ESM_ESTILO_ESCURECER) == ESM_ACESO && esmaecer_estagio_para(30000u, 1, ESM_ESTILO_ESCURECER) == ESM_VEU);
  assert(esmaecer_estagio_para(MIN(1), 2, ESM_ESTILO_ESCURECER) == ESM_VEU);
  // Padrao = 2 min.
  esmaecer_escolha(-1); assert(escolhaAtual == ESM_PADRAO && esmaecer_ms(ESM_PADRAO) == MIN(2));
  // TELA DE DESCANSO (vitrine/relogio): 5 s de veu e depois o preto inteiro.
  assert(esmaecer_estagio_para(MIN(2), 3, ESM_ESTILO_VITRINE) == ESM_VEU);
  assert(esmaecer_estagio_para(MIN(2) + ESM_DESCANSO_MS - 1u, 3, ESM_ESTILO_VITRINE) == ESM_VEU);
  assert(esmaecer_estagio_para(MIN(2) + ESM_DESCANSO_MS, 3, ESM_ESTILO_RELOGIO) == ESM_ESCURO);
  assert(esmaecer_alfa_do_estagio(ESM_ESCURO, ESM_ESTILO_VITRINE) == ESM_ALFA_DESCANSO);
  assert(esmaecer_alfa_do_estagio(ESM_ESCURO, ESM_ESTILO_ESCURECER) == ESM_ALFA_ESCURO);
  esmaecer_reiniciar(); esmaecer_escolha(3); esmaecer_estilo(ESM_ESTILO_VITRINE);
  quadros(0, MIN(2) + ESM_DESCANSO_MS + 3000u, 0);
  assert(esmaecer_descanso() && esmaecer_veu() == ESM_ALFA_DESCANSO);
  assert(esmaecer_entrada(MIN(3), 1) == 1 && !esmaecer_descanso());
  esmaecer_estilo(ESM_ESTILO_ESCURECER);

  // Ocioso: acende, esmaece em ~1,5 s ate 60%, depois a 92%.
  esmaecer_reiniciar(); esmaecer_escolha(4);
  esmaecer_quadro(0, 0.1f, 0);
  quadros(100, MIN(5) - 100, 0);
  assert(esmaecer_estagio() == ESM_ACESO && esmaecer_veu() == 0.0f);
  quadros(MIN(5), MIN(5) + 700, 0);
  assert(esmaecer_estagio() == ESM_VEU && esmaecer_veu() > 0.0f && esmaecer_veu() < ESM_ALFA_VEU);
  quadros(MIN(5) + 800, MIN(5) + 2000, 0);
  assert(esmaecer_veu() == ESM_ALFA_VEU && !esmaecer_apagado());
  quadros(MIN(5) + 2100, MIN(8) + 6000, 0);
  assert(esmaecer_estagio() == ESM_ESCURO && esmaecer_apagado() && esmaecer_veu() == ESM_ALFA_ESCURO);
  assert(!esmaecer_descanso());   // "so escurecer" nao tem tela de descanso
  { int esc = -1; assert(esmaecer_mudou_escuro(&esc) && esc == 1 && !esmaecer_mudou_escuro(&esc)); }

  // A primeira tecla acorda na hora e e consumida; a repeticao logo depois
  // tambem; depois de 300 ms passa; tecla solta/movimento acorda mas nao e engolido.
  { unsigned t = MIN(8) + 7000u; int esc = -1;
    assert(esmaecer_entrada(t, 1) == 1);
    assert(esmaecer_estagio() == ESM_ACESO && esmaecer_veu() == 0.0f);
    assert(esmaecer_mudou_escuro(&esc) && esc == 0);
    assert(esmaecer_entrada(t + 100, 1) == 1);
    assert(esmaecer_entrada(t + 400, 1) == 0);
    esmaecer_quadro(t + 500, 0.1f, 0);
    assert(esmaecer_estagio() == ESM_ACESO);
    // O relogio recomeca da tecla: de novo 5 min ate o veu.
    quadros(t + 600, t + 400 + MIN(5) - 200, 0);
    assert(esmaecer_estagio() == ESM_ACESO);
    quadros(t + 400 + MIN(5) - 100, t + 400 + MIN(5) + 300, 0);
    assert(esmaecer_estagio() == ESM_VEU);
    assert(esmaecer_entrada(t + 400 + MIN(5) + 400, 0) == 0);   // movimento: acorda, nao engole
    assert(esmaecer_estagio() == ESM_ACESO);
  }

  // NUNCA durante a reproducao: tocando por 30 minutos sem tecla nao esmaece.
  esmaecer_reiniciar(); esmaecer_escolha(3);
  { unsigned t;
    for (t = 0; t <= MIN(30); t += 1000) esmaecer_quadro(t, 1.0f, 1);
    assert(esmaecer_estagio() == ESM_ACESO && esmaecer_veu() == 0.0f);
    // Pausou: os minutos contam da pausa, nao do ultimo toque.
    quadros(MIN(30) + 100, MIN(30) + MIN(2) - 200, 0);
    assert(esmaecer_estagio() == ESM_ACESO);
    quadros(MIN(30) + MIN(2) - 100, MIN(30) + MIN(2) + 2500, 0);
    assert(esmaecer_estagio() == ESM_VEU && esmaecer_veu() == ESM_ALFA_VEU);
    // Voltou a tocar com o veu ligado (ex.: proximo episodio): some na hora.
    esmaecer_quadro(MIN(30) + MIN(2) + 2600, 0.1f, 1);
    assert(esmaecer_estagio() == ESM_ACESO && esmaecer_veu() == 0.0f);
  }

  // Desligado: nada esmaece, nem tecla e engolida.
  esmaecer_reiniciar(); esmaecer_escolha(0);
  quadros(0, MIN(60), 0);
  assert(esmaecer_estagio() == ESM_ACESO && esmaecer_entrada(MIN(60) + 1000u, 1) == 0);

  // Brilho do OSD: bases, degrau automatico so tocando, rampa suave, piso.
  assert(esmaecer_brilho_base(0) == 1.0f && esmaecer_brilho_base(1) == 0.80f);
  assert(esmaecer_brilho_base(2) == 0.65f && esmaecer_brilho_base(3) == 0.50f);
  assert(esmaecer_brilho_base(7) == 0.80f);
  assert(esmaecer_brilho_osd(1, 0, 1) == 0.80f && esmaecer_brilho_osd(1, BRILHO_AUTO_MS, 1) == 0.80f);
  { float meio = esmaecer_brilho_osd(1, BRILHO_AUTO_MS + BRILHO_AUTO_RAMPA / 2, 1);
    float fim = esmaecer_brilho_osd(1, 60000u, 1);
    assert(meio < 0.80f && meio > fim);
    assert(fim > 0.679f && fim < 0.681f);   // 0,80 * 0,85
  }
  assert(esmaecer_brilho_osd(1, 60000u, 0) == 0.80f);   // pausado: sem degrau automatico
  assert(esmaecer_brilho_osd(0, 60000u, 1) > 0.849f && esmaecer_brilho_osd(0, 60000u, 1) < 0.851f);
  assert(esmaecer_brilho_osd(3, 60000u, 1) >= 0.40f);   // piso de legibilidade

  // UI EM HDR (#205): so com o video em HDR/DV o fator sai de 1.
  assert(esmaecer_brilho_hdr("none", 0) == 1.0f && esmaecer_brilho_hdr("", 0) == 1.0f);
  assert(esmaecer_brilho_hdr(NULL, 0) == 1.0f && esmaecer_brilho_hdr("desconhecido", 0) == 1.0f);
  assert(esmaecer_brilho_hdr("HDR10", 0) == HDR_UI_BRILHO && esmaecer_brilho_hdr("HLG", 0) == HDR_UI_BRILHO);
  assert(esmaecer_brilho_hdr("none", 1) == HDR_UI_BRILHO && esmaecer_brilho_hdr("DolbyVision", 1) == HDR_UI_BRILHO);
  assert(HDR_UI_BRILHO < 1.0f);
  { float r = 1, g = 1, b = 1, a = 0.5f;          // SDR: nada muda
    esmaecer_cor_hdr(1.0f, &r, &g, &b, &a);
    assert(r == 1 && g == 1 && b == 1 && a == 0.5f); }
  { float r = 1, g = 1, b = 1, a = 0.12f;         // branco (texto, vidro de foco): mais baixo, forma igual
    esmaecer_cor_hdr(HDR_UI_BRILHO, &r, &g, &b, &a);
    assert(r == HDR_UI_BRILHO && b == HDR_UI_BRILHO && a == 0.12f); }
  { float r = 0.055f, g = 0.059f, b = 0.071f, a = 0.80f;   // material da ilha: mais denso
    esmaecer_cor_hdr(HDR_UI_BRILHO, &r, &g, &b, &a);
    assert(r < 0.055f && a > 0.87f && a < 0.89f); }
  { float r = 0, g = 0, b = 0, a = 0;             // o furo do video continua furo
    esmaecer_cor_hdr(HDR_UI_BRILHO, &r, &g, &b, &a); assert(a == 0); }
  puts("esmaecer: estagios, tecla consumida, nunca tocando e brilho do OSD ok");
  return 0;
}
