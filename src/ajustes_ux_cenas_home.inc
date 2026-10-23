// CENAS PROPRIAS DAS OPCOES (Home), 2.0.3: cada opcao mostra o que ela muda,
// em vez da cena generica da categoria. Incluido por ajustes_ux_cenas.inc;
// usa o kit de ajcena.inc e o apoio do comeco daquele arquivo.
// Devolve 1 quando desenhou a cena da opcao; 0 cai na cena antiga.
//
// Regra da casa: cada cena mostra a PECA que a opcao muda, em close, e nunca a
// Home inteira (a Home inteira e a cena de todo mundo e e o que o dono chamou
// de "horrivel"). Sem palavras: so numeros, siglas e icones.

static float ajcHK(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
static float ajcHMix(float a, float b, float t) { return a + (b - a) * t; }
// Cartaz barato: um bloco no matiz com a barrinha do titulo (2 quads).
static void ajcHBloco(float x, float y, float w, float h, int m, float a) {
  float r, g, b, rd = (w < h ? w : h) * 0.1f;
  ajc_matiz(m, &r, &g, &b);
  ajc_ret(x, y, w, h, rd, r * 0.82f, g * 0.82f, b * 0.82f, a);
  if (h > 24) ajc_ret(x + w * 0.12f, y + h - 9, w * 0.55f, 3, 1.5f, 1, 1, 1, 0.55f * a);
}
// A moldura de TV com a tela escura por dentro.
static void ajcHTela(float x, float y, float w, float h, float a) {
  ajc_tv(x, y, w, h, a);
  ajc_ret(x + 5, y + 5, w - 10, h - 10, 8, 0.075f, 0.082f, 0.102f, a);
}
static void ajcHNum(const char *t, float px, float x, float y, float a) { ajcTexto(t, px, 1, x, y, a); }
// Os dois lados de um antes/depois: o do estado atual ganha o foco.
static void ajcHLado(int lado, int atual, float x, float y, float w, float h, float r) {
  ajc_painel(x, y, w, h, r, lado == atual ? 1.0f : 0.7f);
  if (lado == atual) ajc_foco(x, y, w, h, r, 1);
}
static void ajcHSeta(float cx, float cy) { ajc_icone("aj_chevron-right", cx, cy, 26, 0.7f); }

// --- pecas: barra lateral e cartoes do Continuar assistindo -----------------------------
static const char *const AJC_H_ICO[6] = { "aj_house", "aj_search", "aj_compass", "aj_library", "aj_calendar", "aj_settings-2" };
// Uma linha da barra lateral: ladrilho do icone + barra de rotulo.
static void ajcHItemRail(float x, float y, float d, const char *ico, int sel, float rot, float a) {
  if (sel) ajc_acento_ret(x, y, d, d, d * 0.3f, a);
  if (sel) ajc_icone_tinta(ico, x + d * 0.5f, y + d * 0.5f, d * 0.54f, a);
  else ajc_icone(ico, x + d * 0.5f, y + d * 0.5f, d * 0.54f, 0.7f * a);
  if (rot > 0.01f) ajc_barra(x + d + 10, y + d * 0.5f - 3.5f, 60, (sel ? 0.8f : 0.4f) * rot * a);
}
static void ajcHCw(float x, float y, float w, float h, int m, float prog, float a) {
  ajcHBloco(x, y, w, h, m, a);
  if (prog >= 0) ajc_progresso(x + 8, y + h - 12, w - 16, prog, a);
}

// --- HOME ---------------------------------------------------------------------------------
// Posteres horizontais: o mesmo catalogo em pe e deitado; cabem menos por fileira.
static void ajcHPaisagem(int op) {
  int lado, i, j, on = ajcLig(op) > 0.5f;
  for (lado = 0; lado < 2; lado++) {
    float x = 24 + lado * 312;
    ajcHLado(lado, on, x, 36, 280, 236, 16);
    for (j = 0; j < 2; j++)
      for (i = 0; i < (lado ? 2 : 4); i++) {
        if (lado) ajcHBloco(x + 14 + i * 130, 56 + j * 100, 122, 76, j * 2 + i, 1);
        else ajcHBloco(x + 14 + i * 66, 56 + j * 104, 56, 88, j * 4 + i, 1);
      }
    ajc_ret(x + 140 - (lado ? 16 : 8), 296, lado ? 32 : 16, lado ? 18 : 26, 3, 1, 1, 1, lado == on ? 0.9f : 0.3f);
    if (lado == on) ajc_acento_ret(x + 140 - (lado ? 16 : 8), 296, lado ? 32 : 16, lado ? 18 : 26, 3, 1);
  }
  ajcHSeta(320, 154);
  ajcChaveCanto(ajcLig(op));
}
// Destaque: o banner do alto abre e fecha, e as fileiras sobem ou descem.
static void ajcHDestaque(int op) {
  float k = ajcLig(op), ban = 120 * k, y0 = 40, p = ajc_onda(3.0f);
  int i, j;
  ajcHTela(60, 24, 520, 280, 1);
  if (k > 0.02f) {
    ajc_cartaz(70, y0, 500, ban, 1, k);
    ajc_veu(70, y0, 300, ban, 2, 0.7f * k);
    ajc_ret(94, y0 + ban * 0.3f, 130, 14, 4, 1, 1, 1, 0.9f * k);
    ajc_barras(94, y0 + ban * 0.3f + 24, 180, 2, k);
    ajc_acento_ret(94, y0 + ban * 0.3f + 58, 70, 20, 10, k);
    ajc_foco(70, y0, 500, ban, 8, k * (0.5f + 0.5f * p));
  }
  for (i = 0; i < 2; i++) {
    float ry = y0 + ban + 14 + i * 74;
    ajc_barra(86, ry, 70, 0.5f);
    for (j = 0; j < 5; j++) ajcHBloco(86 + j * 94, ry + 14, 82, 48, i * 3 + j, 1);
  }
  ajcChaveCanto(k);
}
// Espaco entre fileiras / entre titulos: o valor real, com a regua no vao.
static void ajcHEspaco(int op) {
  int fil = op == AJ_ESPACO_FILEIRAS, i, j;
  float f = valor[op] > 0 ? ajustes_espaco_fator(valor[op]) : 1.0f;
  // 50..300 % cabe na tela da previa: 3 fileiras ate 34 px, 5 cartazes ate 49.
  float g = (fil ? 4.0f + 12.0f * (f - 0.5f) : 4.0f + 18.0f * (f - 0.5f)) + 3.0f * (ajc_onda(2.4f) - 0.5f);
  char b[16];
  ajcHTela(60, 24, 520, 280, 1);
  if (fil) {
    for (i = 0; i < 3; i++) {
      float ry = 42 + i * (52 + g);
      ajc_barra(86, ry, 70, 0.5f);
      for (j = 0; j < 5; j++) ajcHBloco(86 + j * 94, ry + 12, 82, 40, i * 3 + j, 1);
    }
    ajc_acento_ret(80, 42 + 52, 480, g, 3, 0.4f);
    ajc_acento_ret(80, 42 + 52, 480, 2, 1, 1);
    ajc_acento_ret(80, 42 + 52 + g - 2, 480, 2, 1, 1);
    ajc_icone("aj_arrow-up-down", 320, 42 + 52 + g * 0.5f, 18, 1);
  } else {
    float w = 56;
    for (i = 0; i < 2; i++) {
      float ry = 52 + i * 112;
      ajc_barra(86, ry, 70, 0.5f);
      for (j = 0; j < 5; j++) ajcHBloco(86 + j * (w + g), ry + 14, w, 84, i * 3 + j, 1);
    }
    ajc_acento_ret(86 + w, 52 + 14, g, 84, 3, 0.4f);
    ajc_acento_ret(86 + w, 52 + 14, 2, 84, 1, 1);
    ajc_acento_ret(86 + w + g - 2, 52 + 14, 2, 84, 1, 1);
    ajc_icone("aj_move-horizontal", 86 + w + g * 0.5f, 52 + 14 + 42, g > 20 ? 18 : 12, 1);
  }
  snprintf(b, sizeof b, "%d%%", (int)(f * 100.0f + 0.5f));
  ajcTextoVivo(b, 22, 1, 270, 322, 1);
}
// Barra lateral moderna: icones pequenos (antiga) contra ladrilhos grandes.
static void ajcHRailModerna(int op) {
  int lado, i, on = ajcLig(op) > 0.5f;
  for (lado = 0; lado < 2; lado++) {
    float x = 24 + lado * 312;
    ajcHLado(lado, on, x, 30, 280, 270, 16);
    for (i = 0; i < 3; i++) ajcHBloco(x + (lado ? 112 : 84) + i * 58, 70 + (i % 2) * 70, 50, 78, i, 0.6f);
    ajc_ret(x + 10, 40, lado ? 84 : 52, 250, 14, 0.04f, 0.045f, 0.06f, 0.95f);
    for (i = 0; i < 6; i++) {
      float d = lado ? 40 : 18, py = 52 + i * (lado ? 40 : 36);
      ajcHItemRail(x + 10 + (lado ? 22 : 17), py, d, AJC_H_ICO[i], i == 1, 0, 1);
    }
  }
  ajcHSeta(320, 164);
  ajcChaveCanto(ajcLig(op));
}
// Recolhida / fixa: a largura da barra anda com o valor; a Home encolhe.
static void ajcHRailFixa(int op) {
  float v = ajcHK(ajcValor(op)), w = ajcHMix(50, 170, v);
  int i;
  ajcHTela(60, 24, 520, 280, 1);
  ajc_painel(68, 32, w, 264, 14, 1);
  for (i = 0; i < 6; i++) ajcHItemRail(68 + 8, 46 + i * 40, 34, AJC_H_ICO[i], i == 1, v, 1);
  for (i = 0; i < 4; i++) {
    float px = 68 + w + 14 + i * 74;
    if (px < 566) ajcHBloco(px, 70 + (i % 2) * 24, 64, 100, i, 1);
  }
  ajc_foco(68, 32, w, 264, 14, 0.5f + 0.5f * ajc_onda(2.4f));
  ajc_icone("aj_panel-left", 548, 322, 22, 0.8f);
}
// Itens da barra lateral: a linha do item entra ou sai, e a tela dele ao lado.
static void ajcHMenuItem(int op, const char *ico) {
  float k = ajcLig(op), p = ajc_onda(2.4f), y = 46;
  const char *its[5];
  int i, j;
  its[0] = "aj_house"; its[1] = "aj_search"; its[2] = ico; its[3] = "aj_library"; its[4] = "aj_settings-2";
  ajcHTela(40, 24, 560, 280, 1);
  ajc_painel(50, 32, 168, 264, 14, 1);
  for (i = 0; i < 5; i++) {
    float s = i == 2 ? k : 1;
    if (s < 0.02f) continue;
    ajcHItemRail(62, y, 34, its[i], i == 2, s, s);
    if (i == 2) ajc_foco(58, y - 4, 152, 42, 12, k * (0.4f + 0.6f * p));
    y += 46 * s;
  }
  // a tela do item: so existe (clara) quando a barra tem o item
  { float a = 0.18f + 0.82f * k, x0 = 238, y0 = 44;
    if (op == AJ_MENU_EXPLORAR) {   // filtros + grade de cartazes
      for (i = 0; i < 3; i++) ajc_neutro(x0 + i * 60, y0, 52, 18, 9, a);
      ajc_acento_ret(x0, y0, 52, 18, 9, a * 0.9f);
      for (j = 0; j < 2; j++) for (i = 0; i < 4; i++) ajcHBloco(x0 + i * 84, y0 + 34 + j * 108, 74, 98, j * 4 + i, a);
    } else if (op == AJ_MENU_GUIA) {   // canais e programas em faixas; a linha de agora
      for (j = 0; j < 5; j++) {
        float yy = y0 + j * 50, x = x0 + 52;
        ajc_neutro(x0, yy, 40, 40, 8, a);
        for (i = 0; i < 3; i++) {
          float w = 70 + ((i * 37 + j * 53) % 60);
          ajc_ret(x, yy, w, 40, 6, 0.17f, 0.18f, 0.21f, a);
          ajc_barra(x + 8, yy + 16, w - 24, 0.45f * a);
          x += w + 6;
          if (x > 540) break;
        }
      }
      ajc_acento_ret(x0 + 172, y0 - 4, 3, 252, 1.5f, a);
    } else if (op == AJ_MENU_AGENDA) {   // grade de dias com estreias
      for (j = 0; j < 4; j++) for (i = 0; i < 7; i++) {
        int est = (i * 3 + j * 5) % 7 == 0;
        float cx = x0 + i * 46, cy = y0 + j * 62;
        if (est) ajcHBloco(cx, cy, 40, 54, i + j, a);
        else ajc_neutro(cx, cy, 40, 54, 8, a * 0.7f);
        if (est && i == 3 && j == 1) ajc_foco(cx, cy, 40, 54, 8, a);
      }
    } else {   // perfil: avatar, numeros e colunas de grafico
      ajc_bolinha(x0 + 40, y0 + 46, 70, 0.30f, 0.38f, 0.52f, a);
      ajc_icone("aj_user-round", x0 + 40, y0 + 46, 34, a);
      for (i = 0; i < 3; i++) {
        ajc_neutro(x0 + 100 + i * 70, y0 + 20, 62, 54, 10, a);
        ajc_barra(x0 + 108 + i * 70, y0 + 32, 30, 0.7f * a);
        ajc_barra(x0 + 108 + i * 70, y0 + 50, 44, 0.35f * a);
      }
      for (i = 0; i < 9; i++) {
        float h = 30 + ((i * 47) % 90);
        if (i == 6) ajc_acento_ret(x0 + 6 + i * 36, y0 + 230 - h, 26, h, 5, a);
        else ajc_ret(x0 + 6 + i * 36, y0 + 230 - h, 26, h, 5, 0.9f, 0.9f, 0.92f, 0.28f * a);
      }
    } }
  ajcChaveCanto(k);
}
// Desfoque da barra: solida contra vidro sobre a arte.
static void ajcHRailBlur(int op) {
  int lado, i, on = ajcLig(op) > 0.5f;
  for (lado = 0; lado < 2; lado++) {
    float x = 24 + lado * 312;
    ajcHLado(lado, on, x, 30, 280, 270, 16);
    for (i = 0; i < 4; i++) ajc_cartaz(x + 14 + i * 66, 52, 62, 226, 2 + i, 1);
    if (lado) {
      ajc_ret(x + 14, 40, 118, 250, 14, 0.80f, 0.84f, 0.92f, 0.18f);
      ajc_veu(x + 14, 40, 118, 250, 3, 0.35f);
      gfx_anel(ajc_r(x + 14, 40, 118, 250), 14.0f / 250.0f, ajc_px(1.5f), 1, 1, 1, 0.3f * C.a);
    } else ajc_ret(x + 14, 40, 118, 250, 14, 0.06f, 0.065f, 0.08f, 0.97f);
    for (i = 0; i < 4; i++) ajcHItemRail(x + 24, 54 + i * 52, 34, AJC_H_ICO[i], i == 1, 0, 1);
  }
  ajcHSeta(320, 164);
  ajcChaveCanto(ajcLig(op));
}
// Botao de microfone na busca (.tpk): a barra da busca (a da cena do Cinemeta)
// com o microfone no acento ao lado do celular. O laco mostra o que ele faz: o
// teclado da Samsung aparece, o microfone do controle ouve, o texto chega no
// campo e o teclado some. Os valores ficam no seletor da propria tela, por
// cima do canto de baixo: Automatico ganha o selo de "a TV tem microfone";
// Desligado tira o botao (fica a vaga vazia) e o laco, e o controle apaga.
static void ajcHMicBusca(int op) {
  int p = ajc_passo(1.2f, 4), letras, i;
  float t = ajc_passo_t(1.2f);
  float tem = ajcHK(2.0f - ajcValor(op));    // 1 com o botao (Automatico, Ligado), 0 Desligado
  float autoK = ajcHK(1.0f - ajcValor(op));  // 1 so no Automatico
  float sobe = p == 1 ? t : p == 2 ? 1.0f : p == 3 ? 1.0f - t : 0.0f;
  float ouve = p == 2 ? tem : 0.0f, ctl = 0.3f + 0.7f * tem;
  letras = tem > 0.5f && p == 3 ? 1 + (int)(t * 6.0f) : 0;

  // A barra da busca: lupa, o que chegou (ou o texto apagado do vazio) e o
  // cursor; no fim, microfone e celular, o mesmo par da tela de verdade.
  ajc_painel(40, 30, 560, 56, 28, 1);
  ajc_icone("aj_search", 74, 58, 22, 0.9f);
  if (letras) { for (i = 0; i < letras; i++) ajc_ret(100 + i * 18, 50, 12, 16, 3, 1, 1, 1, 0.8f); }
  else ajc_barra(100, 54, 170, 0.22f);
  ajc_ret(100 + letras * 18, 46, 2, 24, 1, 1, 1, 1, ajc_onda(0.8f) > 0.5f ? 0.9f : 0.1f);
  ajc_bolinha(566, 58, 40, 0.18f, 0.185f, 0.205f, 1);
  ajc_icone("aj_smartphone", 566, 58, 19, 0.85f);
  if (tem < 0.99f) {   // Desligado: a vaga do botao, vazia
    gfx_anel(ajc_r(492, 38, 40, 40), 0.5f, ajc_px(1.5f), 1, 1, 1, 0.22f * (1 - tem) * C.a);
    ajc_icone("aj_eye-off", 512, 58, 16, 0.45f * (1 - tem));
  }
  if (tem > 0.01f) {
    ajc_bolinha(512, 58, 40, C.ar, C.ag, C.ab, tem);
    ajc_icone_tinta("aj_mic", 512, 58, 19, tem);
    ajc_foco(492, 38, 40, 40, 20, tem * (0.5f + 0.5f * ajc_onda(2.4f)));
  }

  // O controle: o botao do microfone no alto (no acento enquanto ouve, com as
  // ondas), o direcional e dois botoes. No Automatico, o selo de que a TV tem
  // microfone.
  ajc_ret(70, 132, 48, 186, 24, 0.10f, 0.105f, 0.12f, ctl);
  gfx_anel(ajc_r(70, 132, 48, 186), 24.0f / 186.0f, ajc_px(1.5f), 1, 1, 1, 0.14f * ctl * C.a);
  ajc_bolinha(94, 160, 24, 0.18f, 0.185f, 0.205f, ctl * (1 - ouve));
  ajc_bolinha(94, 160, 24, C.ar, C.ag, C.ab, ouve);
  if (ouve > 0.5f) ajc_icone_tinta("aj_mic", 94, 160, 13, ouve);
  else ajc_icone("aj_mic", 94, 160, 13, 0.85f * ctl);
  for (i = 0; i < 3 && ouve > 0.01f; i++) {
    float ph = fmodf(ajc_fase(1.2f) + i / 3.0f, 1.0f);
    gfx_anel_fora(ajc_r(82, 148, 24, 24), 0.5f, ajc_px(4 + ph * 22), ajc_px(2), C.ar, C.ag, C.ab,
                  (1 - ph) * 0.5f * ouve * C.a);
  }
  gfx_anel(ajc_r(77, 206, 34, 34), 0.5f, ajc_px(2), 1, 1, 1, 0.30f * ctl * C.a);
  ajc_bolinha(94, 223, 12, 1, 1, 1, 0.30f * ctl);
  ajc_bolinha(83, 268, 10, 1, 1, 1, 0.22f * ctl);
  ajc_bolinha(105, 268, 10, 1, 1, 1, 0.22f * ctl);
  if (autoK * tem > 0.01f) {
    ajc_bolinha(116, 136, 22, C.ar, C.ag, C.ab, autoK * tem);
    ajc_icone_tinta("aj_check", 116, 136, 13, autoK * tem);
  }

  // O teclado da Samsung, so durante o laco: aparece subindo um pouco (sem
  // chegar ao seletor de valores do canto de baixo), tres fileiras de teclas e
  // a de baixo com o espaco e o microfone dele (no acento enquanto ouve).
  if (sobe * tem > 0.01f) {
    float kx = 168, ky = 156 - 20 * sobe, ka = sobe * tem;
    ajc_painel(kx, ky, 330, 140, 18, ka);
    ajc_vidro(kx, ky, 330, 140, 18, 0.5f * ka);
    ajc_icone("aj_keyboard", kx + 22, ky + 18, 16, 0.6f * ka);
    for (i = 0; i < 27; i++)
      ajc_neutro(kx + 16 + (i % 9) * 34, ky + 32 + (i / 9) * 26, 28, 20, 5, ka);
    ajc_neutro(kx + 50, ky + 110, 196, 22, 6, ka);
    ajc_ret(kx + 254, ky + 108, 60, 26, 8, 0.17f, 0.18f, 0.21f, ka * (1 - ouve));
    ajc_acento_ret(kx + 254, ky + 108, 60, 26, 8, ka * ouve);
    if (ouve > 0.5f) ajc_icone_tinta("aj_mic", kx + 284, ky + 121, 14, ka);
    else ajc_icone("aj_mic", kx + 284, ky + 121, 14, 0.85f * ka);
  }
  if (ouve > 0.01f) ajc_icone("aj_audio-lines", 143, 160, 18, (0.4f + 0.5f * ajc_onda(0.6f)) * ouve);
}
// Relogio na tela: o canto de cima, em close.
static void ajcHRelogio(int op) {
  float k = ajcLig(op), b = ajc_fase(2.0f) < 0.5f ? 1.0f : 0.25f;
  int i;
  ajcHTela(60, 24, 520, 280, 1);
  for (i = 0; i < 4; i++) ajcHBloco(86 + i * 120, 130, 108, 150, i, 0.55f);
  ajc_vidro(398, 44, 160, 56, 28, 0.2f + 0.8f * k);
  ajcTexto("20", 30, 1, 422, 56, 0.2f + 0.8f * k);
  ajc_ret(465, 62, 5, 5, 2.5f, 1, 1, 1, b * k); ajc_ret(465, 76, 5, 5, 2.5f, 1, 1, 1, b * k);
  ajcTexto("30", 30, 1, 480, 56, 0.2f + 0.8f * k);
  ajcChaveCanto(k);
}
// Formato do relogio: 24 h contra 12 h, o atual em foco.
static void ajcHRelogio12(int op) {
  int on = valor[op] == 1, i;
  for (i = 0; i < 2; i++) {
    float x = 40 + i * 300;
    ajcHLado(i, on, x, 100, 260, 120, 24);
    if (i) { ajcTexto("8:30", 46, 1, x + 30, 128, 1); ajcSelo("PM", x + 188, 150, 18, on ? 1.0f : 0.0f, 1); }
    else ajcTexto("20:30", 54, 1, x + 40, 124, 1);
  }
  ajcHSeta(320, 160);
  ajc_icone("aj_clock", 320, 272, 40, 0.5f);
}
// --- FILEIRAS -----------------------------------------------------------------------------
// Ordenar e ativar: as fileiras trocam de lugar, e cada uma tem sua chave.
static void ajcHOrdem(void) {
  float f = ajc_passo_t(2.4f);
  int par = ajc_passo(2.4f, 2), i, j;
  for (i = 0; i < 4; i++) {
    float pos = (float)i, y;
    if (i == 1) pos = par ? 2 - f : 1 + f;
    if (i == 2) pos = par ? 1 + f : 2 - f;
    y = 34 + pos * 70;
    ajc_painel(40, y, 560, 62, 14, 1);
    if (i == 1 || i == 2) ajc_foco(40, y, 560, 62, 14, 0.6f);
    ajc_icone("aj_list-ordered", 68, y + 31, 22, 0.6f);
    ajc_barra(92, y + 12, 80 + i * 14, 0.65f);
    for (j = 0; j < 5; j++) ajcHBloco(92 + j * 50, y + 28, 42, 26, i * 2 + j, 0.85f);
    ajc_chave(522, y + 20, i == 3 ? 0.0f : 1.0f, 1);
  }
  ajc_icone("aj_arrow-up-down", 614, 150, 20, 0.7f);
}
// Limite de fileiras: a pilha de fileiras e cortada no numero.
static void ajcHLimite(void) {
  int v = valor[AJ_FIL_LIMITE] < 3 ? 3 : valor[AJ_FIL_LIMITE] > 40 ? 40 : valor[AJ_FIL_LIMITE], i, vis;
  float yc;
  char b[16];
  vis = 2 + (v * 6) / 40;
  for (i = 0; i < 9; i++) {
    float y = 28 + i * 33;
    float a = i < vis ? 1.0f : 0.25f;
    ajc_ret(60, y, 300, 26, 8, 0.17f, 0.18f, 0.21f, a);
    ajc_barra(72, y + 9, 60, a * 0.7f);
    ajc_ret(150, y + 6, 14, 14, 3, 0.55f, 0.6f, 0.7f, a * 0.9f);
    ajc_ret(170, y + 6, 14, 14, 3, 0.55f, 0.6f, 0.7f, a * 0.9f);
    ajc_ret(190, y + 6, 14, 14, 3, 0.55f, 0.6f, 0.7f, a * 0.9f);
  }
  yc = 28 + vis * 33 - 4;
  ajc_acento_ret(40, yc, 340, 3, 1.5f, 0.9f);
  ajc_icone("aj_funnel", 394, yc + 2, 20, 0.9f);
  snprintf(b, sizeof b, "%d", v);
  ajc_painel(430, 100, 170, 120, 22, 1);
  ajcHNum(b, 64, v > 9 ? 466 : 490, 128, 1);
}
// Itens por fileira: quantos cartazes cabem antes do ver tudo.
static void ajcHItens(void) {
  static const int N[3] = { 12, 18, 24 };
  int idx = valor[AJ_ITENS_FILEIRA] < 0 || valor[AJ_ITENS_FILEIRA] > 2 ? 0 : valor[AJ_ITENS_FILEIRA], n = N[idx], i;
  char b[16];
  for (i = 0; i < 24; i++) {
    float x = 40 + (i % 12) * 40, y = 70 + (i / 12) * 70;
    if (i < n) ajcHBloco(x, y, 32, 56, i, 1);
    else ajc_ret(x, y, 32, 56, 4, 1, 1, 1, 0.06f);
  }
  ajc_ladrilho("aj_chevron-right", 580, 98 + (n > 12 ? 70 : 0), 36, 1, 0.5f + 0.5f * ajc_onda(2.0f));
  snprintf(b, sizeof b, "%d", n);
  ajcHNum(b, 40, 40, 248, 1);
}
// Ocultar nao lancados: o cartaz sem fonte some e a fileira fecha o buraco.
static void ajcHNaoLancados(int op) {
  float k = ajcLig(op), p = k * ajc_onda(3.0f);
  int i;
  ajcHTela(60, 24, 520, 280, 1);
  ajc_barra(86, 52, 80, 0.5f);
  for (i = 0; i < 5; i++) {
    float x = 86 + i * 94 - (i > 2 ? p * 94 : 0);
    if (i == 2) {
      ajcHBloco(x, 72, 82, 130, 6, 1 - p);
      ajc_ret(x, 72, 82, 130, 8, 0.05f, 0.055f, 0.07f, 0.55f * (1 - p));
      ajc_icone("aj_calendar", x + 41, 137, 30, 0.9f * (1 - p));
      ajc_foco(x, 72, 82, 130, 8, 0.5f * (1 - p));
    } else ajcHBloco(x, 72, 82, 130, i, 1);
  }
  ajc_icone("aj_eye-off", 330, 250, 30, 0.3f + 0.7f * p);
  ajcChaveCanto(k);
}
// Nota do IMDb: a etiqueta amarela no canto do cartaz.
static void ajcHNotas(int op) {
  float k = ajcLig(op);
  static const char *const N[3] = { "7.8", "8.4", "6.9" };
  int i;
  ajcHTela(60, 24, 520, 280, 1);
  for (i = 0; i < 3; i++) {
    float x = 90 + i * 158;
    ajcHBloco(x, 46, 140, 220, i, 1);
    ajc_ret(x + 10, 56, 62, 26, 7, 0.96f, 0.77f, 0.18f, 0.95f * k);
    ajc_icone("aj_star", x + 24, 69, 14, k);
    ajTextoMini(N[i], ajc_px(14), 1, ajc_x(x + 36), ajc_y(62), 26, 26, 26, k);
  }
  ajcTexto("IMDb", 14, 1, 288, 276, 0.4f + 0.6f * k);
  ajcChaveCanto(k);
}
// Selo de assistido: o check no canto de cima dos que voce ja viu.
static void ajcHSelo(int op) {
  float k = ajcLig(op), p = 0.8f + 0.2f * ajc_onda(2.4f);
  int i;
  ajcHTela(60, 24, 520, 280, 1);
  for (i = 0; i < 3; i++) {
    float x = 90 + i * 158;
    ajcHBloco(x, 46, 140, 220, i + 3, 1);
    if (i != 1) {
      ajc_ret(x, 46, 140, 220, 10, 0, 0, 0, 0.30f * k);
      ajc_bolinha(x + 120, 66, 30 * p, C.ar, C.ag, C.ab, k);
      ajc_icone_tinta("aj_check", x + 120, 66, 18 * p, k);
    }
  }
  ajc_icone("aj_eye", 320, 282, 22, 0.5f + 0.5f * k);
  ajcChaveCanto(k);
}
// Ocultar filmes assistidos: o cartaz marcado sai da descoberta e fecha o vao.
static void ajcHOcultarVistos(int op) {
  float k = ajcLig(op), p = k * ajc_onda(3.0f);
  int i;
  ajcHTela(60, 24, 520, 280, 1);
  ajc_barra(86, 52, 80, 0.5f);
  for (i = 0; i < 5; i++) {
    float x = 86 + i * 94 - (i > 2 ? p * 94 : 0);
    float a = i == 2 ? 1 - p : 1;
    ajcHBloco(x, 72, 82, 130, i, a);
    if (i == 2) {
      ajc_bolinha(x + 66, 88, 24, C.ar, C.ag, C.ab, a);
      ajc_icone_tinta("aj_check", x + 66, 88, 15, a);
    }
  }
  ajc_icone("aj_eye-off", 330, 250, 30, 0.3f + 0.7f * p);
  ajcChaveCanto(k);
}
// Rotulos: o nome escrito abaixo do cartaz.
static void ajcHRotulos(int op) {
  float k = ajcLig(op);
  int i;
  ajcHTela(60, 24, 520, 280, 1);
  for (i = 0; i < 4; i++) {
    float x = 86 + i * 118, h = 150 - 30 * k;
    ajcHBloco(x, 50, 100, h, i, 1);
    ajc_barra(x + 4, 50 + h + 12, 80, 0.9f * k);
    ajc_barra(x + 4, 50 + h + 26, 52, 0.4f * k);
  }
  ajc_icone("aj_type", 320, 266, 26, 0.3f + 0.7f * k);
  ajcChaveCanto(k);
}
// Titulo da fileira com o nome do addon (cartucho) ou o tipo (filme / serie).
static void ajcHTituloFileira(int op) {
  float k = ajcLig(op), bar = 70;
  int i, j;
  ajcHTela(60, 24, 520, 280, 1);
  for (i = 0; i < 2; i++) {
    float y = 48 + i * 120, x = 86;
    ajc_ret(x, y, bar, 9, 4.5f, 0.95f, 0.95f, 0.94f, 0.85f);
    if (op == AJ_NOME_ADDON) {
      ajc_ret(x + bar + 10, y - 7, 98 * k, 24, 12, 0.17f, 0.18f, 0.21f, k);
      if (k > 0.5f) {
        ajc_icone("aj_puzzle", x + bar + 28, y + 5, 14, k);
        ajc_barra(x + bar + 44, y + 1.5f, 50, 0.7f * k);
        if (i == 0) ajc_foco(x + bar + 10, y - 7, 98, 24, 12, 0.6f + 0.4f * ajc_onda(2.0f));
      }
    } else {
      ajc_ret(x + bar + 10, y - 7, 34 * k, 24, 12, 0.17f, 0.18f, 0.21f, k);
      if (k > 0.5f) {
        ajc_icone(i == 0 ? "aj_film" : "aj_tv-minimal-play", x + bar + 27, y + 5, 16, k);
        ajc_foco(x + bar + 10, y - 7, 34, 24, 12, 0.5f + 0.5f * ajc_onda(2.0f));
      }
    }
    for (j = 0; j < 5; j++) ajcHBloco(x + j * 94, y + 22, 82, 70, i * 5 + j, 1);
  }
  ajcChaveCanto(k);
}

// --- CONTINUAR ASSISTINDO -----------------------------------------------------------------
// Mostrar a fileira: ela entra por cima e empurra a Home para baixo.
static void ajcHCwLigado(int op) {
  float k = ajcLig(op), y2 = 62 + 112 * k;
  int i;
  ajcHTela(60, 24, 520, 280, 1);
  if (k > 0.02f) {
    ajc_barra(86, 42, 90, 0.6f * k);
    for (i = 0; i < 4; i++) ajcHCw(86 + i * 118, 58, 106, 62, 3 + i, 0.2f + 0.2f * i, k);
    ajc_foco(86, 58, 106, 62, 9, k * (0.4f + 0.6f * ajc_onda(2.4f)));
  }
  ajc_barra(86, y2 + 4, 70, 0.5f);
  for (i = 0; i < 5; i++) ajcHBloco(86 + i * 94, y2 + 20, 82, 70, i, 1);
  ajcChaveCanto(k);
}
// OK no card: o card, o botao e o que acontece depois.
static void ajcHCwOk(int op) {
  int abre = valor[op] == 1, i;
  float p = ajc_onda(2.0f);
  ajcHCw(36, 120, 170, 96, 3, 0.4f, 1);
  ajc_foco(36, 120, 170, 96, 10, 1);
  ajc_ret(78, 232, 86, 28, 14, 0.17f, 0.18f, 0.21f, 1);
  ajcTexto("OK", 16, 1, 108, 238, 1);
  ajc_ret(78, 232, 86, 28, 14, C.ar, C.ag, C.ab, 0.3f * p);
  ajcHSeta(244, 168);
  if (!abre) {   // retomar: o video abre de onde parou
    ajc_video(290, 78, 320, 180, 12, 1);
    ajc_play(450, 168, 56, 1);
    ajc_progresso(310, 238, 280, 0.4f, 1);
    ajc_bolinha(310 + 280 * 0.4f, 240.5f, 12, 1, 1, 1, 1);
  } else {   // abrir pagina: a pagina do titulo
    ajc_cartaz(290, 78, 320, 180, 2, 1);
    ajc_veu(290, 78, 200, 180, 2, 0.8f);
    ajc_ret(312, 118, 110, 12, 4, 1, 1, 1, 0.9f);
    ajc_barras(312, 142, 150, 2, 1);
    ajc_acento_ret(312, 188, 64, 22, 11, 1);
    for (i = 0; i < 3; i++) ajcHBloco(420 + i * 56, 200, 48, 48, 4 + i, 0.8f);
  }
}
// Estilo: as tres formas do card; a atual em foco.
static void ajcHCwEstilo(void) {
  static const float W[3] = { 112, 196, 76 };
  int i, at = valor[AJ_CW_ESTILO] < 0 || valor[AJ_CW_ESTILO] > 2 ? 0 : valor[AJ_CW_ESTILO];
  float x = 92;
  for (i = 0; i < 3; i++) {
    float on = i == at ? 1.0f : 0.4f;
    ajcHBloco(x, 124, W[i], 112, 3 + i * 2, on);
    ajc_progresso(x + 8, 124 + 100, W[i] - 16, 0.25f + 0.2f * i, on);
    if (i == at) ajc_foco(x, 124, W[i], 112, 10, 1);
    x += W[i] + 40;
  }
}
// Miniatura do episodio: a arte da serie contra o quadro do episodio.
static void ajcHCwThumb(int op) {
  int on = ajcLig(op) > 0.5f, lado;
  for (lado = 0; lado < 2; lado++) {
    float x = 40 + lado * 300;
    ajcHLado(lado, on, x, 90, 260, 170, 16);
    if (lado) {
      ajc_video(x + 14, 104, 232, 130, 10, 1);
      ajc_play(x + 130, 168, 36, 0.9f);
      ajc_ret(x + 22, 112, 54, 20, 10, 0, 0, 0, 0.55f);
      ajcTexto("1x04", 13, 1, x + 29, 115, 0.95f);
    } else ajcHBloco(x + 14, 104, 232, 130, 1, 1);
    ajc_progresso(x + 24, 220, 212, 0.45f, 1);
  }
  ajcHSeta(320, 175);
  ajcChaveCanto(ajcLig(op));
}
// Desfocar o proximo: o episodio que voce ainda nao viu vira mosaico.
static void ajcHCwBlur(int op) {
  float k = ajcLig(op);
  int i, j;
  ajcHCw(60, 100, 230, 130, 3, 0.7f, 1);
  ajc_icone("aj_check", 90, 124, 18, 0.9f);
  ajcHBloco(350, 100, 230, 130, 1, 1);
  if (k > 0.02f) {
    for (j = 0; j < 4; j++) for (i = 0; i < 6; i++) {
      float r, g, b, q = 0.5f + 0.4f * (float)((i * 5 + j * 3) % 4) / 3.0f;
      ajc_matiz((i + j) % 4, &r, &g, &b);
      ajc_ret(350 + i * 38.3f, 100 + j * 32.5f, 39, 33, 0, r * q, g * q, b * q, k);
    }
    ajc_ret(350, 100, 230, 130, 10, 0.075f, 0.082f, 0.102f, 0.35f * k);
    ajc_icone("aj_eye-off", 465, 165, 38, k);
  }
  ajc_foco(350, 100, 230, 130, 10, 0.5f + 0.5f * ajc_onda(2.4f));
  ajcChaveCanto(k);
}
// Ordenacao: os mesmos seis cards reorganizados por cada modo.
// Tipos: 0 em andamento (barra), 1 proximo episodio (seta), 2 futuro (calendario).
static void ajcHCwOrdem(void) {
  static const int TIPO[6] = { 0, 1, 0, 2, 1, 0 };
  static const int ORD[3][6] = { { 0, 1, 2, 3, 4, 5 }, { 0, 2, 5, 1, 4, 3 }, { 0, 1, 2, 4, 5, 3 } };
  float v = ajcHK(ajcValor(AJ_CW_ORDEM) / 2.0f) * 2.0f;
  int m0 = (int)v, m1 = m0 < 2 ? m0 + 1 : 2, i, s;
  float t = v - m0, dv = ajcHK(ajcValor(AJ_CW_ORDEM) - 1.0f);
  float xs[2][6];
  for (i = 0; i < 2; i++) {
    int m = i ? m1 : m0;
    for (s = 0; s < 6; s++) xs[i][ORD[m][s]] = 28 + s * 92 + (m == 2 && s == 5 ? 26 : 0);
  }
  ajc_ret(497, 90, 3, 150, 1.5f, C.ar, C.ag, C.ab, 0.85f * dv);
  for (i = 0; i < 6; i++) {
    float x = ajcHMix(xs[0][i], xs[1][i], t), y = 110;
    ajc_ret(x, y, 82, 110, 12, 0.17f, 0.18f, 0.21f, 1);
    if (TIPO[i] == 0) {
      ajcHBloco(x + 6, y + 6, 70, 72, 3 + i, 0.9f);
      ajc_progresso(x + 12, y + 92, 58, 0.3f + 0.1f * i, 1);
    } else if (TIPO[i] == 1) {
      ajcHBloco(x + 6, y + 6, 70, 72, 3 + i, 0.9f);
      ajc_ladrilho("aj_chevron-right", x + 41, y + 42, 30, 1, 1);
    } else {
      ajcHBloco(x + 6, y + 6, 70, 72, 3 + i, 0.5f);
      ajc_icone("aj_calendar", x + 41, y + 42, 28, 0.95f);
    }
  }
  ajc_foco(ajcHMix(xs[0][0], xs[1][0], t), 110, 82, 110, 12, 1);
}
// Nao exibidos: o card do episodio que ainda vai ao ar.
static void ajcHCwNaoExibidos(int op) {
  float k = ajcLig(op), p = 0.85f + 0.15f * ajc_onda(2.4f);
  int i;
  for (i = 0; i < 2; i++) {
    ajcHCw(40 + i * 170, 104, 154, 100, 3 + i, i ? -1.0f : 0.5f, 1);
    if (i) ajc_ladrilho("aj_chevron-right", 40 + i * 170 + 77, 154, 30, 1, 1);
  }
  ajc_ret(380, 104, 154, 100, 10, 1, 1, 1, 0.05f * (1 - k));
  gfx_anel(ajc_r(380, 104, 154, 100), 10.0f / 100.0f, ajc_px(1.5f), 1, 1, 1, 0.12f * (1 - k) * C.a);
  if (k > 0.02f) {
    ajcHBloco(380, 104, 154, 100, 6, 0.75f * k * p);
    ajc_ret(380, 104, 154, 100, 10, 0.05f, 0.055f, 0.07f, 0.4f * k);
    ajc_icone("aj_calendar", 457, 142, 30, k);
    ajcTexto("15/10", 16, 1, 431, 164, k);
    ajc_foco(380, 104, 154, 100, 10, k * (0.5f + 0.5f * ajc_onda(2.0f)));
  }
  ajcChaveCanto(k);
}
// Tambem em Continuar assistindo: o card da ilha cai na fileira.
static void ajcHCwRetido(int op) {
  float k = ajcLig(op), f = ajc_fase(2.8f), p = ajcHK(f * 1.6f);
  int i;
  ajc_vidro(200, 24, 240, 54, 27, 1);
  ajcHBloco(212, 34, 44, 34, 6, 1);
  ajc_barra(266, 42, 90, 0.8f);
  ajc_barra(266, 54, 56, 0.4f);
  ajc_icone("aj_clapperboard", 412, 51, 18, 0.8f);
  ajc_icone("aj_arrow-down", 320, 100 + 14 * p, 24, 0.9f * k);
  ajc_barra(40, 144, 90, 0.5f);
  for (i = 0; i < 4; i++) {
    float x = 40 + i * 140;
    if (i == 0) {
      ajc_ret(x, 160, 128, 84, 10, 1, 1, 1, 0.05f * (1 - k));
      if (k > 0.02f) { ajcHCw(x, 160, 128, 84, 6, 0.5f, k); ajc_foco(x, 160, 128, 84, 10, k); }
    } else ajcHCw(x, 160, 128, 84, i + 2, 0.2f + 0.2f * i, 1);
  }
  ajcChaveCanto(k);
}
// Proximo do episodio mais alto: a seta nasce do mais alto, nao do mais recente.
static void ajcHCwFurthest(int op) {
  int on = ajcLig(op) > 0.5f, i, alvo = on ? 5 : 2;
  float bump = 5.0f * ajc_onda(1.6f);
  char b[8];
  for (i = 0; i < 8; i++) {
    float x = 30 + i * 74, y = 130;
    int visto = i == 0 || i == 1 || i == 4;
    ajc_ret(x, y, 62, 62, 12, 0.17f, 0.18f, 0.21f, i == alvo ? 1.0f : 0.8f);
    if (i == alvo) { ajc_acento_ret(x, y, 62, 62, 12, 0.25f); ajc_foco(x, y, 62, 62, 12, 1); }
    snprintf(b, sizeof b, "%d", i + 1);
    ajcTexto(b, 22, 1, x + 24, y + 17, visto ? 0.5f : 1.0f);
    if (visto) ajc_icone("aj_check", x + 50, y + 12, 14, 0.9f);
    if (i == 1) ajc_icone("aj_clock", x + 31, y - 20, 18, 0.9f);
    if (i == 4) ajc_icone("aj_arrow-up", x + 31, y - 20, 18, 0.9f);
  }
  ajc_icone("aj_arrow-up", 30 + alvo * 74 + 31, 222 + bump, 26, 1);
  ajcChaveCanto(ajcLig(op));
}
// Escolha de perfil: o Continuar embaixo do perfil em foco.
static void ajcHPsContinuar(int op) {
  float k = ajcLig(op), cx;
  int i, f = ajc_passo(1.6f, 3), j;
  for (i = 0; i < 4; i++) {
    float x = 130 + i * 126, cy = 100, d = i == f ? 84 : 72, r, g, b;
    ajc_matiz(i * 2 + 1, &r, &g, &b);
    ajc_bolinha(x, cy, d, r, g, b, 1);
    ajc_icone("aj_user-round", x, cy, d * 0.42f, 0.85f);
    if (i == f) ajc_foco(x - d * 0.5f, cy - d * 0.5f, d, d, d * 0.5f, 1);
    ajc_barra(x - 26, cy + 56, 52, i == f ? 0.8f : 0.35f);
  }
  cx = 130 + f * 126;
  if (cx < 190) cx = 190;
  if (cx > 450) cx = 450;
  for (j = 0; j < 3; j++) ajcHCw(cx - 130 + j * 90, 218, 80, 50, 3 + j + f, 0.3f + 0.2f * j, k);
  ajc_foco(cx - 136, 212, 272, 62, 10, k * 0.6f);
  ajcChaveCanto(k);
}

// --- APARENCIA ----------------------------------------------------------------------------
// Fundo de cartazes com um detalhe fino (para a transparencia aparecer).
static void ajcHArte(float x, float y, float w, float h, int detalhe) {
  int i;
  for (i = 0; i < 5; i++) ajc_cartaz(x + 8 + i * (w - 16) / 5.0f, y + 8, (w - 16) / 5.0f - 4, h - 16, i + 1, 1);
  if (detalhe) for (i = 0; i < 3; i++) ajc_ret(x + 14, y + 40 + i * 52, w - 28, 3, 1.5f, 1, 1, 1, 0.65f);
}
// Um painel de vidro sobre a arte: opacidade, contorno e fosco.
static void ajcHVidroPainel(float x, float y, float w, float h, float opac, int contorno, int fosco, int solido, int detalhe) {
  float px = x + 30, py = y + 54, pw = w - 60, ph = h - 108;
  ajcHArte(x, y, w, h, detalhe);
  if (solido) ajc_ret(px, py, pw, ph, 18, 0.08f, 0.085f, 0.10f, 0.98f);
  else {
    ajc_ret(px, py, pw, ph, 18, 0.08f, 0.085f, 0.10f, opac);
    ajc_ret(px, py, pw, ph, 18, 0.80f, 0.85f, 0.95f, 0.10f);
    if (fosco) ajc_ret(px, py, pw, ph, 18, 0.5f, 0.52f, 0.58f, 0.5f);
    if (contorno) gfx_anel(ajc_r(px, py, pw, ph), 18.0f / ph, ajc_px(contorno > 1 ? 3.0f : 1.5f), 1, 1, 1, (contorno > 1 ? 0.9f : 0.35f) * C.a);
  }
  ajc_barra(px + 16, py + 16, pw * 0.55f, 0.85f);
  ajc_barras(px + 16, py + 38, pw * 0.8f, 2, 1);
  ajc_acento_ret(px + 16, py + ph - 38, 64, 22, 11, 1);
}
static void ajcHVidro(int op) {
  int on = ajcLig(AJ_VIDRO) > 0.5f, lado;
  float o = 0.6f + 0.08f * ajcValor(AJ_VIDRO_OPAC);
  for (lado = 0; lado < 2; lado++) {
    float x = 24 + lado * 312;
    ajcHLado(lado, on, x, 36, 280, 250, 16);
    ajcHVidroPainel(x, 36, 280, 250, o, 1, 0, !lado, 0);
  }
  ajcHSeta(320, 160);
  ajcChaveCanto(ajcLig(op));
}
static void ajcHVidroContorno(int op) {
  int on = ajcLig(op) > 0.5f, lado;
  for (lado = 0; lado < 2; lado++) {
    float x = 24 + lado * 312;
    ajcHLado(lado, on, x, 36, 280, 250, 16);
    ajcHVidroPainel(x, 36, 280, 250, 0.62f, lado ? 2 : 0, 0, 0, 0);
  }
  ajcHSeta(320, 160);
  ajcChaveCanto(ajcLig(op));
}
static void ajcHVidroFosco(int op) {
  int on = ajcLig(op) > 0.5f, lado;
  for (lado = 0; lado < 2; lado++) {
    float x = 24 + lado * 312;
    ajcHLado(lado, on, x, 36, 280, 250, 16);
    ajcHVidroPainel(x, 36, 280, 250, 0.45f, 1, lado, 0, 1);
  }
  ajcHSeta(320, 160);
  ajcChaveCanto(ajcLig(op));
}
static void ajcHVidroOpac(void) {
  static const int P[5] = { 60, 70, 78, 86, 92 };
  int i, at = valor[AJ_VIDRO_OPAC] < 0 || valor[AJ_VIDRO_OPAC] > 4 ? 2 : valor[AJ_VIDRO_OPAC];
  char b[8];
  ajcHArte(24, 36, 592, 250, 1);
  for (i = 0; i < 5; i++) {
    float x = 40 + i * 112, a = P[i] / 100.0f;
    ajc_ret(x, 100, 100, 130, 14, 0.08f, 0.085f, 0.10f, a);
    ajc_ret(x, 100, 100, 130, 14, 0.80f, 0.85f, 0.95f, 0.10f);
    gfx_anel(ajc_r(x, 100, 100, 130), 14.0f / 130.0f, ajc_px(1.5f), 1, 1, 1, 0.3f * C.a);
    snprintf(b, sizeof b, "%d", P[i]);
    ajcTexto(b, 22, 1, x + 30, 150, i == at ? 1.0f : 0.5f);
    if (i == at) { ajc_foco(x, 100, 100, 130, 14, 1); ajc_acento_ret(x + 20, 206, 60, 5, 2.5f, 1); }
  }
  ajc_progresso(40, 308, 560, (float)at / 4.0f, 1);
}
// Preview every size, using the same factors as the actual interface.
static void ajcHTamanhoUi(void) {
  float v = ajcHK(ajcValor(AJ_TAMANHO_UI) / 8.0f) * 8.0f, e, by;
  int m0 = (int)v, m1 = m0 < 8 ? m0 + 1 : 8;
  e = ajcHMix(ui_size_choice(m0, tamanhoUiAutomatico()), ui_size_choice(m1, tamanhoUiAutomatico()), v - m0);
  by = 334 - 12 * e - 34 * e;
  ajc_video(0, 0, 640, 360, 0, 1);
  ajc_veu(0, 200, 640, 160, 0, 0.8f);
  ajc_ret(28, 26, 90 * e, 7 * e, 3.5f * e, 1, 1, 1, 0.8f);
  ajc_vidro(628 - 160 * e, 24, 160 * e, 44 * e, 14 * e, 1);
  ajc_bolinha(628 - 160 * e + 22 * e, 24 + 22 * e, 22 * e, C.ar, C.ag, C.ab, 1);
  ajc_barra(628 - 160 * e + 42 * e, 24 + 18 * e, 100 * e, 0.7f);
  ajc_ret(28, 334 - 6 * e, 584, 5 * e, 2.5f * e, 1, 1, 1, 0.2f);
  ajc_acento_ret(28, 334 - 6 * e, 240, 5 * e, 2.5f * e, 1);
  ajc_bolinha(268, 334 - 6 * e + 2.5f * e, 14 * e, 1, 1, 1, 1);
  ajc_play(320, by, 56 * e, 1);
  ajc_icone("aj_rotate-ccw-clock", 320 - 80 * e, by, 28 * e, 0.9f);
  ajc_icone("aj_volume-2", 320 + 80 * e, by, 28 * e, 0.9f);
  ajc_icone("aj_captions", 320 + 124 * e, by, 28 * e, 0.9f);
  ajcSelo(i18n(V_TAMANHO_UI[(int)(v + 0.5f)]), 28, 62, 16, 1.0f, 1);
}
// Tamanho dos ajustes: a lista e o painel dos proprios Ajustes, menores ou maiores.
static void ajcHTamanhoAj(void) {
  static const float E[3] = { 0.8f, 0.9f, 1.0f };
  static const char *const T[3] = { "80%", "90%", "100%" };
  float v = ajcHK(ajcValor(AJ_TAMANHO_AJUSTES) / 2.0f) * 2.0f, e;
  int m0 = (int)v, m1 = m0 < 2 ? m0 + 1 : 2, i;
  e = ajcHMix(E[m0], E[m1], v - m0);
  ajcHTela(30, 24, 580, 296, 1);
  for (i = 0; i < 6; i++) {
    float y = 40 + i * 44 * e;
    if (y + 36 * e > 306) break;
    ajc_ret(44, y, 330 * e + 20, 38 * e, 10 * e, 0.17f, 0.18f, 0.21f, i == 1 ? 1.0f : 0.7f);
    ajc_bolinha(44 + 20 * e, y + 19 * e, 22 * e, 0.30f, 0.34f, 0.42f, 1);
    ajc_barra(44 + 40 * e, y + 15 * e, 120 * e, 0.7f);
    ajc_chave(44 + 330 * e - 56 * e, y + 8 * e, i % 2 ? 0.0f : 1.0f, 1);
    if (i == 1) ajc_foco(44, y, 330 * e + 20, 38 * e, 10 * e, 1);
  }
  ajc_painel(400 + 20 * (1 - e), 40, 196 * e + 4, 250 * e, 14, 1);
  ajc_cartaz(410 + 20 * (1 - e), 50, 176 * e + 4, 110 * e, 2, 1);
  ajc_barras(414 + 20 * (1 - e), 50 + 124 * e, 150 * e, 3, 1);
  ajcSelo(T[(int)(v + 0.5f)], 40, 326, 13, 1.0f, 1);
}
// Estilo do descanso: o que a TV mostra parada: vitrine, relogio ou so escuro.
static void ajcHDescansoEstilo(void) {
  int at = valor[AJ_DESCANSO_ESTILO] < 0 || valor[AJ_DESCANSO_ESTILO] > 2 ? 0 : valor[AJ_DESCANSO_ESTILO], i;
  float d = ajc_onda(6.0f);
  ajcHTela(60, 24, 520, 280, 1);
  if (at == 0) {   // vitrine: um titulo por vez em tela cheia
    int m = ajc_passo(3.0f, 4);
    ajc_cartaz(70, 34, 500, 260, 1 + m, 1);
    ajc_veu(70, 34, 300, 260, 2, 0.8f);
    ajc_ret(96, 190, 150, 16, 4, 1, 1, 1, 0.9f);
    ajc_barras(96, 218, 210, 2, 1);
    ajc_acento_ret(96, 254, 62, 20, 10, 1);
    ajc_icone_tinta("aj_play", 127, 264, 11, 1);
    for (i = 0; i < 4; i++) ajc_bolinha(490 + i * 16, 270, i == m ? 9 : 6, 1, 1, 1, i == m ? 0.95f : 0.4f);
  } else if (at == 1) {   // relogio grande + a proxima estreia
    ajcTexto("20:30", 74, 1, 190 + 10 * d, 96 + 8 * d, 1);
    ajc_ret(210, 214, 220, 40, 20, 0.17f, 0.18f, 0.21f, 1);
    ajc_icone("aj_calendar", 236, 234, 20, 0.9f);
    ajc_barra(256, 227, 120, 0.6f);
    ajc_barra(256, 238, 70, 0.35f);
  } else {   // so escurecer: quase tudo apagado e um relogio pequeno que anda
    ajc_ret(66, 30, 508, 268, 6, 0, 0, 0, 0.85f);
    ajcTexto("20:30", 22, 1, 110 + 340 * d, 250 - 140 * d, 0.45f);
  }
  for (i = 0; i < 3; i++) {
    ajc_ret(272 + i * 36, 322, 28, 5, 2.5f, 1, 1, 1, 0.3f);
    if (i == at) ajc_acento_ret(272 + i * 36, 322, 28, 5, 2.5f, 1);
  }
}
// Titulos da vitrine: de onde saem os cartazes que passam.
static void ajcHDescansoFonte(void) {
  int lista = valor[AJ_DESCANSO_FONTE] == 1, i, m = ajc_passo(3.0f, 3);
  ajc_painel(24, 50, 300, 236, 16, 1);
  ajc_foco(24, 50, 300, 236, 16, 1);
  for (i = 0; i < 12; i++) {
    float x = 40 + (i % 4) * 70, y = 66 + (i / 4) * 70;
    if (!lista) ajcHBloco(x, y, 58, 62, i, 1);
    else if (i == 1 || i == 6 || i == 8) {
      ajcHBloco(x, y, 58, 62, i, 1);
      ajc_icone(i == 6 ? "aj_circle-play" : "aj_bookmark", x + 46, y + 12, 14, 1);
    } else ajc_ret(x, y, 58, 62, 6, 1, 1, 1, 0.05f);
  }
  ajc_icone(lista ? "aj_bookmark" : "aj_library", 56, 268, 18, 0.7f);
  ajcHSeta(352, 168);
  ajcHTela(380, 90, 240, 150, 1);
  ajc_cartaz(388, 98, 224, 134, 1 + m * 2, 1);
  ajc_veu(388, 98, 140, 134, 2, 0.8f);
  ajc_ret(404, 170, 70, 8, 3, 1, 1, 1, 0.9f);
  ajc_barras(404, 188, 100, 2, 1);
}

static int ajcNovaHome(int sec, int op) {
  (void)sec;
  switch (op) {
    case AJ_LANDSCAPE: ajcHPaisagem(op); return 1;
    case AJ_HERO: ajcHDestaque(op); return 1;
    case AJ_ESPACO_FILEIRAS: case AJ_ESPACO_TITULOS: ajcHEspaco(op); return 1;
    case AJ_RAIL_MODERNA: ajcHRailModerna(op); return 1;
    case AJ_RAIL: ajcHRailFixa(op); return 1;
    case AJ_MENU_EXPLORAR: ajcHMenuItem(op, "aj_compass"); return 1;
    case AJ_MENU_GUIA: ajcHMenuItem(op, "aj_tv-minimal-play"); return 1;
    case AJ_MENU_AGENDA: ajcHMenuItem(op, "aj_calendar"); return 1;
    case AJ_MENU_PERFIL: ajcHMenuItem(op, "aj_user-round"); return 1;
    case AJ_RAIL_BLUR: ajcHRailBlur(op); return 1;
    case AJ_MIC_BUSCA: ajcHMicBusca(op); return 1;
    case AJ_RELOGIO: ajcHRelogio(op); return 1;
    case AJ_RELOGIO_12H: ajcHRelogio12(op); return 1;
    case AJ_FIL_ORDEM: ajcHOrdem(); return 1;
    case AJ_FIL_LIMITE: ajcHLimite(); return 1;
    case AJ_ITENS_FILEIRA: ajcHItens(); return 1;
    case AJ_OCULTAR_NLANC: ajcHNaoLancados(op); return 1;
    case AJ_NOTAS_HOME: ajcHNotas(op); return 1;
    case AJ_SELO_VISTO: ajcHSelo(op); return 1;
    case AJ_OCULTAR_FILMES_VISTOS: ajcHOcultarVistos(op); return 1;
    case AJ_ROTULOS: ajcHRotulos(op); return 1;
    case AJ_NOME_ADDON: case AJ_SUFIXO_TIPO: ajcHTituloFileira(op); return 1;
    case AJ_CW_LIGADO: ajcHCwLigado(op); return 1;
    case AJ_CW_OK: ajcHCwOk(op); return 1;
    case AJ_CW_ESTILO: ajcHCwEstilo(); return 1;
    case AJ_CW_THUMB: ajcHCwThumb(op); return 1;
    case AJ_CW_BLUR_PROX: ajcHCwBlur(op); return 1;
    case AJ_CW_ORDEM: ajcHCwOrdem(); return 1;
    case AJ_CW_NAO_EXIBIDOS: ajcHCwNaoExibidos(op); return 1;
    case AJ_CW_RETIDO_TAMBEM: ajcHCwRetido(op); return 1;
    case AJ_CW_FURTHEST: ajcHCwFurthest(op); return 1;
    case AJ_PS_CONTINUAR: ajcHPsContinuar(op); return 1;
    case AJ_VIDRO: ajcHVidro(op); return 1;
    case AJ_VIDRO_CONTORNO: ajcHVidroContorno(op); return 1;
    case AJ_VIDRO_OPAC: ajcHVidroOpac(); return 1;
    case AJ_VIDRO_FOSCO: ajcHVidroFosco(op); return 1;
    case AJ_TAMANHO_UI: ajcHTamanhoUi(); return 1;
    case AJ_TAMANHO_AJUSTES: ajcHTamanhoAj(); return 1;
    case AJ_DESCANSO_ESTILO: ajcHDescansoEstilo(); return 1;
    case AJ_DESCANSO_FONTE: ajcHDescansoFonte(); return 1;
    default: return 0;
  }
}
