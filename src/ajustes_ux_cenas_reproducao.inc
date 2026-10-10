// CENAS PROPRIAS DAS OPCOES (Reproducao), 2.0.3: cada opcao mostra o que ela muda,
// em vez da cena generica da categoria. Incluido por ajustes_ux_cenas.inc;
// usa o kit de ajcena.inc e o apoio do comeco daquele arquivo.
// Devolve 1 quando desenhou a cena da opcao; 0 cai na cena antiga.
//
// Sem palavras nas cenas: so numeros, siglas e padroes de regex. Texto = barra.

// --- apoio ---------------------------------------------------------------------------
static float rK(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
static float rMix(float a, float b, float t) { return a + (b - a) * t; }
static int rInt(int op, int mx) { int v = valor[op]; return v < 0 ? 0 : v > mx ? mx : v; }

// Uma fonte da lista: bolinha do add-on (matiz), duas barras e ate dois selos a direita.
static float rAtivo2 = 0;   // o segundo selo da proxima rLinha no acento (0..1)
static void rLinha(float x, float y, float w, float h, const char *q, const char *q2, int matiz, float a) {
  float r, g, b, sx = x + w - 10, sy = y + h * 0.5f - 11.4f;
  ajc_neutro(x, y, w, h, 12, a);
  ajc_matiz(matiz, &r, &g, &b);
  ajc_bolinha(x + h * 0.5f, y + h * 0.5f, h * 0.58f, r, g, b, a);
  ajc_barra(x + h + 4, y + h * 0.5f - 10, w * 0.30f, 0.75f * a);
  ajc_barra(x + h + 4, y + h * 0.5f + 4, w * 0.18f, 0.35f * a);
  if (q2) { sx -= ajcLargura(q2, 12, 1) + 17; ajcSelo(q2, sx, sy, 12, rAtivo2, a); sx -= 6; rAtivo2 = 0; }
  if (q) { sx -= ajcLargura(q, 12, 1) + 17; ajcSelo(q, sx, sy, 12, 0, a); }
}
static float rYL(int i) { return 40.0f + i * 62.0f; }   // 4 linhas de 52

// Um selo com cor propria (matiz) misturada com o neutro por k.
static float rSeloCor(const char *t, float x, float y, float px, int matiz, float k, float a) {
  float w = ajcLargura(t, px, 1) + px * 1.4f, h = px * 1.9f, r, g, b;
  ajc_matiz(matiz, &r, &g, &b);
  ajc_neutro(x, y, w, h, h * 0.5f, a);
  ajc_ret(x, y, w, h, h * 0.5f, r * 1.25f, g * 1.25f, b * 1.25f, k * a);
  ajcTexto(t, px, 1, x + px * 0.7f, y + (h - px * 1.25f) * 0.5f, (0.7f + 0.3f * k) * a);
  return w;
}
static void rTelaHdr(float x, float y, float w, float h, float dinamica, float brilho, float a) {
  float o = ajc_onda(2.6f), m = dinamica * o + (1 - dinamica) * 0.5f;
  ajc_tv(x, y, w, h, a);
  ajc_ret(x + 6, y + 6, w - 12, h - 12, 8, 0.06f, 0.06f, 0.08f, a);
  ajc_luz(x + 6, y + 6, w - 12, h - 12, 8, (w - 12) * (0.25f + 0.5f * m), 30, h * 0.9f, 0.95f, 0.6f, 0.3f, brilho * a);
  ajc_luz(x + 6, y + 6, w - 12, h - 12, 8, (w - 12) * (0.85f - 0.5f * m), h - 30, h * 0.8f, 0.3f, 0.5f, 0.95f, brilho * 0.8f * a);
}

// --- qualidade -----------------------------------------------------------------------
static void rHdr(void) {   // Preferir / Indiferente / Evitar
  static const char *Q[3] = { "1080p", "4K", "4K" };
  static const char *T[3] = { "HDR", "SDR", "HDR" };
  int v = rInt(AJ_FONTE_HDR, 2), i, pick = v == 0 ? 2 : 1;
  for (i = 0; i < 3; i++) {
    int hdr = i != 1;
    float al = (v == 2 && hdr) ? 0.3f : 1.0f, y = rYL(i) + 10;
    rAtivo2 = hdr && v == 0 ? 1.0f : 0.0f;
    rLinha(40, y, 520, 52, Q[i], T[i], i, al);
    if (v == 2 && hdr) ajc_icone("aj_x", 584, y + 26, 18, 0.8f);
    if (i == pick) { float p = 0.55f + 0.45f * ajc_onda(2.2f); ajc_foco(40, y, 520, 52, 12, p); ajc_icone("aj_check", 584, y + 26, 18, p); }
  }
}
static void rDv(int op) {   // HDR10 fixo x Dolby Vision cena a cena
  float k = ajcLig(op), o = ajc_onda(2.6f);
  rTelaHdr(30, 60, 270, 152, 0, 0.5f, 1);
  rTelaHdr(340, 60, 270, 152, 1, 0.30f + 0.55f * o, 1);
  ajc_foco(k > 0.5f ? 340 : 30, 60, 270, 152, 14, 0.5f + 0.5f * ajc_onda(2.0f));
  ajcSelo("HDR10", 135, 236, 14, k > 0.5f ? 0 : 1, 1);
  ajcSelo("DV", 456, 236, 14, k > 0.5f ? 1 : 0, 1);
  ajc_icone("aj_chevron-right", 320, 136, 20, 0.5f);
  ajcChaveCanto(k);
}
static void rAtmos(int op) {   // sons como objetos no espaco (ligado) x dois canais (desligado)
  float k = ajcLig(op), t = ajc_t();
  int i;
  ajc_tv(210, 70, 220, 124, 1);
  ajc_ret(216, 76, 208, 112, 8, 0.07f, 0.07f, 0.09f, 1);
  ajc_ret(240, 222, 160, 12, 6, 0.25f, 0.26f, 0.30f, 1);
  for (i = 0; i < 2; i++) {   // estereo: dois pilares
    float bar = 18 + 22 * ajc_onda(1.4f + 0.3f * i);
    ajc_ret(80 - i * 0 + i * 440, 220 - bar, 18, bar, 5, 1, 1, 1, 0.45f * (1 - k) + 0.12f);
  }
  for (i = 0; i < 5; i++) {   // objetos Atmos orbitando, inclusive por cima
    float a = t * (0.8f + 0.15f * i) + i * 1.3f, cx = 320 + cosf(a) * (150 + 14 * i), cy = 120 + sinf(a) * (70 + 10 * i) - 20;
    ajc_bolinha(cx, cy, 12, C.ar, C.ag, C.ab, k * 0.9f);
    ajc_bolinha(cx, cy, 24, C.ar, C.ag, C.ab, k * 0.2f);
  }
  ajcSelo("ATMOS", 276, 268, 14, k, 1);
  ajcChaveCanto(k);
}

// --- escolha da fonte ----------------------------------------------------------------
static void rManual(int op) {   // Play -> lista aberta (ligado) ou video direto (desligado)
  float k = ajcLig(op), f = ajc_fase(4.5f), ap = f < 0.3f ? 0 : anim_suave(rK((f - 0.3f) / 0.15f));
  int i, s = ajc_passo(0.9f, 3);
  if (ajc_reduzido()) ap = 1;
  ajc_cartaz(36, 50, 150, 220, 1, 1);
  ajc_acento_ret(52, 224, 100, 30, 15, 1);
  ajc_icone_tinta("aj_play", 76, 239, 14, 1);
  ajc_ret(92, 236, 44, 6, 3, ajTinta() / 255.0f, ajTinta() / 255.0f, ajTinta() / 255.0f, 0.9f);
  if (f > 0.18f && f < 0.4f) ajc_foco(52, 224, 100, 30, 15, 1);
  ajc_icone("aj_chevron-right", 214, 160, 22, 0.5f);
  if (k > 0.01f) {
    ajc_vidro(236, 40, 372, 232, 18, ap * k);
    for (i = 0; i < 3; i++) {
      float y = 56 + i * 68;
      ajc_neutro(252, y, 340, 56, 12, ap * k);
      ajc_barra(274, y + 17, 150, 0.75f * ap * k); ajc_barra(274, y + 32, 90, 0.35f * ap * k);
      ajcSelo(i == 1 ? "1080p" : "4K", 520, y + 17, 12, 0, ap * k);
    }
    ajc_foco(252, 56 + s * 68, 340, 56, 12, ap * k);
  }
  if (k < 0.99f) { ajc_video(236, 56, 372, 210, 14, ap * (1 - k)); ajc_play(422, 161, 44, ap * (1 - k) * 0.9f); }
  ajcChaveCanto(k);
}
static void rPrioridade(void) {   // Equilibrio / Qualidade / Comecar rapido
  static const char *Q[3] = { "4K", "1080p", "720p" };
  static const char *IC[3] = { "aj_sliders-horizontal", "aj_star", "aj_zap" };
  static const float VEL[3] = { 0.22f, 0.55f, 0.95f };
  int v = rInt(AJ_FONTE_PRIORIDADE, 2), pick = v == 1 ? 0 : v == 0 ? 1 : 2, i;
  float p = 0.55f + 0.45f * ajc_onda(2.2f);
  for (i = 0; i < 3; i++) {
    float y = rYL(i) + 10;
    rLinha(40, y, 470, 52, Q[i], NULL, i + 3, i == pick ? 1.0f : 0.55f);
    ajc_icone("aj_gauge", 290, y + 26, 16, 0.5f);
    ajc_progresso(308, y + 23.5f, 80, VEL[i], i == pick ? 1.0f : 0.5f);
    if (i == pick) ajc_foco(40, y, 470, 52, 12, p);
  }
  ajc_ladrilho(IC[v], 570, 140, 64, 1, 1);
  ajc_foco(538, 108, 64, 64, 18, p);
}
static void rPrazo(void) {   // a linha do tempo dos add-ons e o corte
  static const int PZ[7] = { 0, 3, 5, 8, 15, 30, -1 };
  static const float TM[6] = { 0.8f, 2.0f, 4.0f, 9.0f, 14.0f, 26.0f };
  int v = rInt(AJ_FONTE_PRAZO, 6), i;
  float cut = PZ[v] < 0 ? 1.0f : PZ[v] / 30.0f, f = ajc_fase(5.0f) * 1.15f, sw = rK(f), X0 = 70, W = 500, cx = X0 + W * cut;
  ajc_ret(X0, 186, W, 6, 3, 1, 1, 1, 0.16f);
  ajc_acento_ret(X0, 186, W * (sw < cut ? sw : cut), 6, 3, 1);
  for (i = 0; i < 7; i++) ajc_ret(X0 + i * W / 6 - 1, 198, 2, 8, 1, 1, 1, 1, 0.3f);
  ajcTexto("0", 12, 0, X0 - 3, 212, 0.55f); ajcTexto("15", 12, 0, X0 + W * 0.5f - 7, 212, 0.55f); ajcTexto("30 s", 12, 0, X0 + W - 22, 212, 0.55f);
  for (i = 0; i < 6; i++) {
    float px = X0 + W * TM[i] / 30.0f, h = 30 + 16 * (i % 3);
    int dentro = TM[i] / 30.0f <= cut + 0.001f, chegou = sw >= TM[i] / 30.0f;
    ajc_ret(px - 1.5f, 189 - h, 3, h, 1.5f, 1, 1, 1, chegou ? 0.35f : 0.1f);
    ajc_bolinha(px, 189 - h - 8, 16, dentro ? C.ar : 0.5f, dentro ? C.ag : 0.5f, dentro ? C.ab : 0.55f, chegou ? (dentro ? 1.0f : 0.35f) : 0.15f);
    ajc_icone("aj_puzzle", px, 189 - h - 8, 9, chegou && dentro ? 0.9f : 0.2f);
  }
  if (PZ[v] != 0 && PZ[v] != -1) ajc_ret(cx - 1, 90, 2, 130, 1, C.ar, C.ag, C.ab, 1);
  if (PZ[v] == 0) ajc_ret(X0 - 1, 90, 2, 130, 1, C.ar, C.ag, C.ab, 1);
  ajc_ladrilho("aj_alarm-clock", PZ[v] == -1 ? cx - 20 : cx, 70, 36, 1, 1);
  ajc_ladrilho("aj_play", 90, 270, 52, sw >= cut ? 1 : 0, sw >= cut ? 1 : 0.4f);
  ajc_progresso(150, 268, 300, sw >= cut ? 1.0f : 0.0f, 1);
}
static void rRepor(void) {   // a escolhida falha: o automatico tenta N outras
  int n = rInt(AJ_FONTE_REPOR, 3), s = ajc_passo(1.1f, n + 2), i;
  char b[8];
  for (i = 0; i < 4; i++) {
    float y = rYL(i) + 6, al = i <= n ? 1.0f : 0.28f;
    int falhou = i < s && s <= n + 1 && i <= n;
    rLinha(40, y, 500, 52, i == 2 ? "1080p" : "4K", NULL, i + 1, al);
    if (falhou) { ajc_ret(40, y, 500, 52, 12, 0.03f, 0.03f, 0.04f, 0.6f); ajc_icone("aj_x", 566, y + 26, 20, 0.9f); }
    if (i == s && i <= n && s > 0) { ajc_foco(40, y, 500, 52, 12, 1); }
    if (i == 0 && s == 0) ajc_foco(40, y, 500, 52, 12, 1);
  }
  snprintf(b, sizeof b, "+%d", n);
  ajc_ladrilho("aj_rotate-cw", 590, 268, 44, 1, 1);
  ajcTexto(b, 16, 1, 566, 290, 0.9f);
  if (s == n + 1 && n == 0) ajc_icone("aj_list-video", 566, 100, 26, 0.9f);
}
static void rTamanho(int op) {   // a faixa de tamanho: a linha de corte e quem fica de fora
  static const float GB[5] = { 28.0f, 9.0f, 18.0f, 3.0f, 12.0f };
  int max = op == AJ_TAM_MAX, i, gb = max ? ajustes_tamanho_max_gb() : ajustes_tamanho_min_gb();
  float X0 = 170, W = 420, lim = (float)gb, cx, fant = gb > 0 ? 1.0f : 0.45f;
  if (gb <= 0) lim = max ? 35.0f - 26.0f * ajc_onda(4.0f) : 1.0f + 22.0f * ajc_onda(4.0f);   // sem limite: so mostra como seria
  cx = X0 + W * lim / 35.0f;
  char b[16];
  for (i = 0; i < 5; i++) {
    float y = 40 + i * 46, bw = W * GB[i] / 35.0f;
    int fora = max ? GB[i] > lim : GB[i] < lim;
    float al = fora ? (gb > 0 ? 0.28f : 0.6f) : 1.0f;
    ajc_neutro(40, y, 560, 38, 10, al);
    ajc_icone("aj_film", 66, y + 19, 20, 0.7f * al);
    ajc_barra(90, y + 15, 64, 0.5f * al);
    ajc_ret(X0, y + 10, bw, 18, 6, 1, 1, 1, 0.55f * al);
    if (fora) ajc_icone("aj_x", 580, y + 19, 14, 0.6f);
  }
  ajc_ret(max ? cx : X0, 30, max ? X0 + W - cx + 20 : cx - X0, 224, 8, 0.05f, 0.05f, 0.07f, 0.45f * fant);
  ajc_ret(cx - 1.5f, 30, 3, 224, 1.5f, C.ar, C.ag, C.ab, fant);
  ajc_ladrilho(max ? "aj_arrow-up" : "aj_arrow-down", cx, 18, 28, gb > 0, gb > 0 ? 1.0f : 0.5f);
  snprintf(b, sizeof b, gb > 0 ? "%d GB" : "---", gb);
  ajcSelo(b, cx - ajcLargura(b, 14, 1) * 0.5f - 10, 276, 14, gb > 0, gb > 0 ? 1.0f : 0.6f);
}

// --- filtro, escopo, ordem -----------------------------------------------------------
static void rRegex(void) {   // Desligada / Exigir / Preferir
  static const int CASA[4] = { 0, 1, 0, 1 };
  int v = rInt(AJ_FONTE_REGEX, 2), i;
  float t = ajc_passo_t(3.2f) * (ajc_passo(3.2f, 2) ? 1.0f : 0.0f);
  if (ajc_reduzido()) t = 1;
  ajcSelo("ESP|PT", 40, 20, 14, v ? 1 : 0, v ? 1 : 0.5f);
  ajc_icone("aj_funnel", 150, 33, 18, v ? 0.9f : 0.3f);
  for (i = 0; i < 4; i++) {
    int c = CASA[i], pos = i;
    float y, al = 1;
    if (v == 2) { int antes = 0, j; for (j = 0; j < 4; j++) if (CASA[j] == c && j < i) antes++;
      pos = c ? antes : 2 + antes; y = rMix(66 + i * 56.0f, 66 + pos * 56.0f, t); }
    else y = 66 + i * 56.0f;
    if (v == 1 && !c) al = 1 - 0.75f * t;
    rLinha(40, y, 560, 46, "4K", NULL, i, al);
    if (c && v) { ajc_foco(40, y, 560, 46, 12, 0.6f + 0.4f * ajc_onda(2.0f)); ajc_icone("aj_check", 580, y + 23, 16, 0.9f); }
    if (v == 1 && !c) ajc_icone("aj_x", 580, y + 23, 16, 0.6f * t);
  }
}
static void rRegexPadrao(void) {   // o campo do padrao sendo digitado
  static const char *TX = "ESP|Latino|Castellano";
  char b[32];
  int n = (int)(ajc_fase(5.0f) * 1.3f * (float)strlen(TX)), i;
  if (n > (int)strlen(TX) || ajc_reduzido()) n = (int)strlen(TX);
  memcpy(b, TX, (size_t)n); b[n] = 0;
  ajc_painel(40, 60, 440, 54, 14, 1);
  ajc_foco(40, 60, 440, 54, 14, 1);
  ajc_icone("aj_pencil", 66, 87, 20, 0.7f);
  ajcTexto(b, 22, 1, 92, 72, 1);
  if (ajc_onda(0.8f) > 0.5f) ajc_acento_ret(94 + ajcLargura(b, 22, 1), 70, 3, 32, 1.5f, 1);
  ajc_ladrilho("aj_smartphone", 540, 87, 64, 0, 1);
  ajc_icone("aj_keyboard", 540, 87, 28, 0.9f);
  for (i = 0; i < 3; i++) { rLinha(40, 150 + i * 56, 560, 46, "4K", NULL, i, i == 1 ? 0.35f : 1.0f);
    if (i != 1) ajc_foco(40, 150 + i * 56, 560, 46, 12, 0.5f); }
}
static void rRegexModelo(void) {   // os modelos prontos, o atual no acento
  static const char *L[7] = { NULL, "ESP", "PT", "EN", "MULTI", "DUAL", "4K|Remux" };
  int v = rInt(AJ_FONTE_REGEX_MODELO, 6), i;
  float x[7] = { 40, 130, 220, 310, 400, 40, 160 }, y[7] = { 130, 130, 130, 130, 130, 200, 200 };
  float p = 0.55f + 0.45f * ajc_onda(2.2f);
  ajc_painel(40, 40, 560, 54, 14, 1);
  ajc_icone("aj_wand", 68, 67, 20, 0.8f);
  if (v > 0) ajcTexto(L[v], 22, 1, 100, 52, 1); else ajc_barra(100, 63, 180, 0.5f);
  for (i = 0; i < 7; i++) {
    if (!L[i]) { ajc_ladrilho("aj_pencil", x[i] + 24, y[i] + 14, 44, v == 0, 1); if (v == 0) ajc_foco(x[i] + 2, y[i] - 8, 44, 44, 12, p); continue; }
    ajcSelo(L[i], x[i], y[i], 16, v == i ? 1 : 0, 1);
    if (v == i) ajc_foco(x[i] - 2, y[i] - 2, ajcLargura(L[i], 16, 1) + 26, 34, 17, p);
  }
}
static void rEscopo(void) {   // de onde o automatico pode tocar
  int v = rInt(AJ_FONTE_ESCOPO, 2), i;
  int on[2] = { v != 2, v != 1 };
  const char *IC[2] = { "aj_puzzle", "aj_plug" };
  float f = ajc_fase(1.8f);
  for (i = 0; i < 2; i++) {
    float x = 40 + i * 400, al = on[i] ? 1.0f : 0.3f;
    int j;
    ajc_neutro(x, 60, 200, 200, 18, al);
    if (on[i]) ajc_foco(x, 60, 200, 200, 18, 1);
    ajc_ladrilho(IC[i], x + 100, 110, 56, on[i], al);
    for (j = 0; j < 3; j++) { ajc_bolinha(x + 50, 176 + j * 24, 12, 1, 1, 1, 0.5f * al); ajc_barra(x + 68, 172 + j * 24, 90 - j * 14, 0.45f * al); }
    if (!on[i]) ajc_icone("aj_x", x + 100, 160, 40, 0.7f);
    else { float p = i ? 1 - f : f;
      ajc_bolinha(x + (i ? 0 : 200) + (i ? -1 : 1) * (p * 130 + 6), 160, 12, C.ar, C.ag, C.ab, 1 - p * 0.5f); }
  }
  ajc_ladrilho("aj_play", 320, 160, 64, 1, 1);
}
static void rPermitidos(int op) {   // quais add-ons/plugins o automatico usa primeiro
  int plug = op == AJ_FONTE_PLUGINS_PERM, n = plug ? 3 : 4, i, s = ajc_passo(1.5f, n);
  const char *IC = plug ? "aj_plug" : "aj_puzzle";
  for (i = 0; i < n; i++) {
    float y = 40 + i * 62, r, g, b;
    int marcado = plug ? (i == 1 || (i == 2 && s == 2)) : (i == 0 || i == 2 || (i == 3 && s == 3));
    ajc_neutro(40, y, 290, 52, 12, 1);
    ajc_matiz(i + (plug ? 3 : 0), &r, &g, &b);
    ajc_ret(54, y + 11, 30, 30, plug ? 15 : 8, r, g, b, 1);
    ajc_icone(IC, 69, y + 26, 16, 0.9f);
    ajc_barra(96, y + 20, 100, 0.7f);
    ajc_bolinha(300, y + 26, 22, marcado ? C.ar : 0.3f, marcado ? C.ag : 0.3f, marcado ? C.ab : 0.34f, 1);
    if (marcado) ajc_icone_tinta("aj_check", 300, y + 26, 13, 1);
    if (i == s) ajc_foco(40, y, 290, 52, 12, 1);
  }
  ajc_icone("aj_chevron-right", 366, 150, 26, 0.55f);
  for (i = 0; i < 4; i++) {
    float y = 60 + i * 56, r, g, b;
    int pri = i < 2;
    ajc_neutro(404, y, 196, 44, 10, pri ? 1.0f : 0.4f);
    ajc_matiz(plug ? (i == 0 ? 4 : 5) : (i == 0 ? 0 : 2), &r, &g, &b);
    ajc_bolinha(426, y + 22, 20, r, g, b, pri ? 1.0f : 0.4f);
    ajc_barra(446, y + 19, 90, pri ? 0.7f : 0.25f);
    if (pri) ajc_acento_ret(404, y + 8, 3, 28, 1.5f, 1);
  }
}
static void rOrdem(void) {   // a fila de add-ons: um sobe e os outros descem
  float p = ajc_onda(3.6f);
  float slot[4];
  int i;
  char b[4];
  slot[0] = 0 + p; slot[1] = 1 + p; slot[2] = 2 - 2 * p; slot[3] = 3;
  if (ajc_reduzido()) { slot[0] = 1; slot[1] = 2; slot[2] = 0; }
  for (i = 0; i < 4; i++) {
    float y = 40 + slot[i] * 62;
    int pos = (int)(slot[i] + 0.5f) + 1;
    rLinha(90, y, 470, 52, NULL, NULL, i + 1, 1);
    ajc_bolinha(62, y + 26, 30, 1, 1, 1, 0.16f);
    snprintf(b, sizeof b, "%d", pos);
    ajcTexto(b, 17, 1, 62 - (pos > 9 ? 9 : 5), y + 14, 1);
    if (i == 2) { ajc_foco(90, y, 470, 52, 12, 1); ajc_icone("aj_arrow-up-down", 586, y + 26, 22, 1); }
  }
}
static void rOrdemUso(void) {   // Nao / Para desempatar / Ordem estrita
  static const char *Q[3] = { "1080p", "4K", "4K" };
  int v = rInt(AJ_FONTE_ORDEM_USO, 2), i, pick = v == 0 ? 2 : v == 1 ? 1 : 0;
  float p = 0.55f + 0.45f * ajc_onda(2.2f);
  char b[4];
  for (i = 0; i < 3; i++) {
    float y = rYL(i) + 20;
    rLinha(90, y, 470, 52, Q[i], i == 2 ? "HDR" : NULL, i + 1, 1);
    ajc_bolinha(62, y + 26, 30, v ? C.ar : 1, v ? C.ag : 1, v ? C.ab : 1, v ? 0.9f : 0.12f);
    snprintf(b, sizeof b, "%d", i + 1);
    if (v) ajcTextoTinta(b, 17, 1, 57, y + 14, 1); else ajcTexto(b, 17, 1, 57, y + 14, 0.4f);
    if (i == pick) { ajc_foco(90, y, 470, 52, 12, p); ajc_icone("aj_check", 586, y + 26, 18, p); }
  }
}
static void rOutros(int op) {   // permitidos vazios: usa os outros (ligado) ou abre a lista (desligado)
  float k = ajcLig(op), f = ajc_fase(4.0f), s = rK((f - 0.25f) / 0.3f);
  int i;
  ajc_neutro(40, 60, 220, 190, 18, 1);
  ajc_foco(40, 60, 220, 190, 18, 1);
  ajc_icone("aj_shield-check", 150, 110, 40, 0.85f);
  ajc_icone("aj_x", 150, 175, 34, 0.5f);
  ajc_neutro(380, 60, 220, 190, 18, 0.7f);
  for (i = 0; i < 3; i++) { ajc_bolinha(420, 110 + i * 38, 14, 1, 1, 1, 0.4f); ajc_barra(440, 106 + i * 38, 110, 0.4f); }
  ajc_icone("aj_clock", 320, 90, 22, 0.6f * (1 - s));
  if (k > 0.5f) {
    ajc_bolinha(272 + 96 * s, 160, 14, C.ar, C.ag, C.ab, s > 0 ? 1 : 0);
    ajc_ret(272, 158, 96, 4, 2, 1, 1, 1, 0.16f);
    if (s >= 1) ajc_foco(380, 60, 220, 190, 18, 1);
  } else {
    ajc_icone("aj_list-video", 320, 170, 40, s > 0 ? 0.9f : 0.2f);
    ajc_icone("aj_x", 372, 160, 16, 0.5f * s);
  }
  ajc_ladrilho("aj_play", 320, 290, 40, 0, 0.5f);
  ajcChaveCanto(k);
}

// --- como o automatico escolhe e prepara -----------------------------------------------
static void rAuto(void) {   // Melhor fonte (confere e escolhe) x Primeira da lista (toca na hora)
  static const char *Q[4] = { "1080p", "4K", "720p", "4K" };
  int primeira = valor[AJ_FONTE_AUTO] == 1, i, s = ajc_passo(0.5f, 8);
  float f = ajc_fase(4.0f);
  for (i = 0; i < 4; i++) {
    float y = rYL(i) + 6, al = 1;
    int alvo = primeira ? 0 : 1;
    if (primeira && i > 0) al = 0.35f;
    rLinha(40, y, 500, 52, Q[i], NULL, i, al);
    if (!primeira && s < 4 && i == s) { ajc_foco(40, y, 500, 52, 12, 0.7f); ajc_icone("aj_search", 566, y + 26, 18, 0.8f); }
    if (i == alvo && (primeira || s >= 4 || ajc_reduzido())) { ajc_foco(40, y, 500, 52, 12, 1); ajc_icone("aj_check", 566, y + 26, 20, 1); }
  }
  ajc_ladrilho(primeira ? "aj_zap" : "aj_sparkles", 590, 280, 44, 1, 1);
  (void)f;
}
static void rLane(float y, const char *ic, float ini, float fim, float f, int video) {
  float p = rK((f - ini) / (fim - ini));
  ajc_ladrilho(ic, 70, y + 12, 38, video, 1);
  ajc_ret(110, y + 4, 480, 16, 8, 1, 1, 1, 0.12f);
  if (p > 0.01f) {
    if (video) ajc_video(110, y + 4, 480 * (fim - ini) * p, 16, 8, 1);
    else ajc_acento_ret(110, y + 4, 480 * (fim - ini) * p, 16, 8, 1);
  }
}
static void rTocarConferindo(int op) {   // conferir e tocar em paralelo (ligado) ou um depois do outro
  float k = ajcLig(op), f = ajc_fase(5.0f) * 1.12f, ini = k > 0.5f ? 0.05f : 0.5f;
  if (ajc_reduzido()) f = 1;
  { float xx = 110 + 480 * (k > 0.5f ? 0.05f : 0.45f);
    ajc_ret(xx - 1, 90, 2, 150, 1, 1, 1, 1, 0.2f); }
  rLane(110, "aj_shield-check", 0.05f, 0.45f, f, 0);
  rLane(190, "aj_play", ini, 0.95f, f, 1);
  if (k > 0.5f) ajc_ret(110, 135, 480 * 0.4f, 3, 1.5f, C.ar, C.ag, C.ab, 0.8f);
  ajc_icone(k > 0.5f ? "aj_zap" : "aj_clock", 320, 270, 30, 0.8f);
  ajcChaveCanto(k);
}
static void rConferirVarias(int op) {   // 3 conferencias de uma vez (ligado) x uma por vez
  float k = ajcLig(op), f = ajc_fase(5.0f) * 1.1f;
  int i;
  if (ajc_reduzido()) f = 1;
  for (i = 0; i < 3; i++) {
    float y = 44 + i * 70, ini = k > 0.5f ? 0.0f : i * 0.3f, p = rK((f - ini) / 0.3f);
    rLinha(40, y, 400, 54, i == 1 ? "1080p" : "4K", NULL, i, 1);
    ajc_progresso(456, y + 24, 100, p, 1);
    ajc_icone(p >= 1 ? "aj_circle-check" : "aj_shield-check", 584, y + 27, 22, p >= 1 ? 1.0f : 0.5f);
    if (p > 0 && p < 1) ajc_foco(40, y, 400, 54, 12, 0.8f);
  }
  ajc_icone("aj_clock", 580, 270, 24, 0.5f);
  ajc_ret(60, 268, k > 0.5f ? 140 : 380, 6, 3, C.ar, C.ag, C.ab, 0.9f);
  ajcChaveCanto(k);
}
static void rAquecer(int op) {   // abrir a conexao antes do Play
  float k = ajcLig(op), f = ajc_fase(5.0f), w = rK(((k > 0.5f) ? (f - 0.1f) : (f - 0.6f)) / 0.2f), vid = rK((f - (k > 0.5f ? 0.65f : 0.9f)) / 0.08f);
  int i;
  if (ajc_reduzido()) { w = 1; vid = 1; }
  ajc_cartaz(24, 70, 96, 140, 1, 1);
  ajc_acento_ret(34, 176, 60, 20, 10, 1);
  if (f > 0.58f && f < 0.7f) ajc_foco(34, 176, 60, 20, 10, 1);
  for (i = 0; i < 3; i++) {
    float y = 70 + i * 52;
    ajc_ret(122, 140, 70, 3, 1.5f, 1, 1, 1, 0.12f);
    ajc_ret(196, y + 18, 70, 3, 1.5f, 1, 1, 1, 0.12f);
    ajc_ret(196, y + 18, 70 * w, 3, 1.5f, C.ar, C.ag, C.ab, 1);
    ajc_ladrilho("aj_globe", 172, y + 20, 34, w > 0.5f, w > 0.5f ? 1 : 0.6f);
  }
  ajc_ret(122, 140, 50 * w, 3, 1.5f, C.ar, C.ag, C.ab, 1);
  ajc_ladrilho("aj_server", 300, 160, 56, w > 0.5f, w > 0.5f ? 1 : 0.6f);
  ajc_ret(330, 158, 90, 3, 1.5f, 1, 1, 1, 0.12f);
  ajc_ret(330, 158, 90 * w, 3, 1.5f, C.ar, C.ag, C.ab, 1);
  ajc_tv(430, 100, 180, 100, 1);
  ajc_ret(436, 106, 168, 88, 8, 0.06f, 0.06f, 0.08f, 1);
  ajc_video(436, 106, 168, 88, 8, vid);
  ajc_play(520, 150, 30, 1 - vid);
  ajc_icone("aj_wifi", 300, 250, 26, 0.3f + 0.6f * w);
  ajcChaveCanto(k);
}
static void rPreparar(int op) {   // o arquivo ja escolhido e posto no debrid antes do Play
  float k = ajcLig(op), f = ajc_fase(5.0f), m = rK(((k > 0.5f) ? (f - 0.1f) : (f - 0.6f)) / 0.25f), vid = rK((f - (k > 0.5f ? 0.65f : 0.92f)) / 0.07f);
  if (ajc_reduzido()) { m = 1; vid = 1; }
  ajc_cartaz(24, 60, 140, 200, 3, 1);
  ajc_acento_ret(40, 218, 90, 28, 14, 1);
  ajc_icone_tinta("aj_play", 62, 232, 13, 1);
  ajc_ret(80, 229, 38, 6, 3, ajTinta() / 255.0f, ajTinta() / 255.0f, ajTinta() / 255.0f, 0.9f);
  if (f > 0.58f && f < 0.7f) ajc_foco(40, 218, 90, 28, 14, 1);
  ajc_ret(176, 158, 150, 4, 2, 1, 1, 1, 0.12f);
  ajc_ladrilho("aj_package", 176 + 150 * m, 160, 40, 1, m > 0 && m < 1 ? 1 : 0);
  ajc_neutro(340, 90, 150, 140, 22, 1);
  ajc_icone("aj_cloud", 415, 140, 70, 0.9f);
  ajc_ladrilho("aj_check", 456, 196, 30, 1, m);
  ajc_tv(510, 120, 110, 62, 1);
  ajc_video(515, 125, 100, 52, 6, vid);
  ajc_play(565, 151, 22, 1 - vid);
  ajc_icone("aj_zap", 565, 215, 22, vid * 0.9f);
  ajcChaveCanto(k);
}
static void rOrdemFontes(void) {
  int addon = rInt(AJ_FONTE_ORDEM_ADDON, 1);
  const char *res[] = { "720p", "4K", "1080p" };
  for (int i = 0; i < 3; i++) {
    float y = 32 + i * 96;
    if (!addon) ajcSelo(i == 0 ? "4K" : i == 1 ? "1080p" : "720p", 52, y, 12, 0, 0.8f);
    ajc_neutro(40, y + (addon ? 0 : 22), 560, 62, 12, 0.8f);
    ajc_barra(66, y + (addon ? 18 : 40), 230 - i * 40, 0.8f);
    if (addon) ajcSelo(res[i], 460, y + 22, 12, 0, 0.8f);
  }
}
static void rTexto(void) {   // Do Nuvio / Do addon / Logo do titulo
  int v = rInt(AJ_FONTE_TEXTO, 2), i;
  for (i = 0; i < 3; i++) {
    float y = 30 + i * 100, al = i == v ? 1.0f : 0.3f;
    ajc_neutro(40, y, 560, 84, 14, al);
    if (i == 0) {   // titulo em cima, selos embaixo
      ajc_barra(66, y + 18, 220, al * 0.9f);
      ajcSelo("4K", 66, y + 44, 12, 0, al); ajcSelo("HDR", 108, y + 44, 12, 0, al); ajcSelo("DV", 164, y + 44, 12, 0, al);
    } else if (i == 1) {   // o texto cru do addon, varias linhas
      int j;
      for (j = 0; j < 4; j++) ajc_barra(66, y + 12 + j * 16, 150 + (j % 3) * 70 - (j == 3 ? 60 : 0), al * (j ? 0.45f : 0.8f));
      ajc_icone("aj_scroll-text", 560, y + 42, 22, 0.6f * al);
    } else {   // a logo do titulo no lugar do nome
      ajc_ret(66, y + 14, 150, 34, 8, 1, 1, 1, al * 0.9f);
      ajc_icone("aj_image", 141, y + 31, 20, al * 0.6f);
      ajcSelo("4K", 66, y + 54, 12, 0, al); ajcSelo("HDR", 108, y + 54, 12, 0, al);
    }
    if (i == v) ajc_foco(40, y, 560, 84, 14, 0.6f + 0.4f * ajc_onda(2.2f));
  }
}

// --- selos -----------------------------------------------------------------------------
static void rSelosCores(int op) {
  static const char *T[6] = { "4K", "HDR", "DV", "ATMOS", "x265", "RD" };
  int i;
  float k = ajcLig(op), x;
  for (i = 0; i < 2; i++) {
    int j;
    x = 60 + i * 20;
    ajc_neutro(40, 70 + i * 90, 560, 66, 14, 0.7f);
    for (j = 0; j < 3; j++) x += rSeloCor(T[i * 3 + j], x + (j ? 0 : 0), 92 + i * 90, 20, 2 + i * 3 + j, k, 1) + 14;
  }
  ajc_icone("aj_palette", 320, 270, 30, 0.35f + 0.6f * k);
  ajcChaveCanto(k);
}
static void rCartaoPacote(float x, float y, float w, float h, int estilo, float a) {
  int j;
  ajc_painel(x, y, w, h, 16, a);
  gfx_anel(ajc_r(x, y, w, h), 16.0f * C.s / (h * C.s), 1.5f, 1, 1, 1, 0.16f * a * C.a);
  for (j = 0; j < 3; j++) {
    float yy = y + 18 + j * 34, r, g, b;
    ajc_matiz(j * 2 + estilo, &r, &g, &b);
    if (estilo == 0) ajc_ret(x + 18, yy, w - 36, 22, 11, r * 1.25f, g * 1.25f, b * 1.25f, a);
    else if (estilo == 1) ajc_vidro(x + 18, yy, w - 36, 22, 4, a);
    else { ajc_bolinha(x + 29, yy + 11, 16, 1, 1, 1, 0.8f * a); ajc_barra(x + 46, yy + 8, w - 74, 0.5f * a); }
    if (estilo < 2) ajc_barra(x + 30, yy + 8, w - 60, 0.7f * a);
  }
}
static void rPacote(void) {
  int v = valor[AJ_SELOS_PACOTE], i, sel = v > 0 ? (v - 1) % 3 : 0;
  float p = 0.55f + 0.45f * ajc_onda(2.2f);
  for (i = 0; i < 3; i++) {
    float x = 40 + i * 200;
    rCartaoPacote(x, 70, 170, 140, i, i == sel ? 1.0f : 0.5f);
    if (i == sel) { ajc_foco(x, 70, 170, 140, 16, p); ajc_ladrilho("aj_check", x + 85, 238, 34, 1, 1); }
  }
}
static void rPacoteAdd(void) {   // um pacote novo entra na fila
  float f = ajc_fase(3.6f), e = anim_suave(rK(f / 0.4f));
  if (ajc_reduzido()) e = 1;
  rCartaoPacote(40, 80, 170, 140, 0, 1);
  rCartaoPacote(230, 80, 170, 140, 1, 1);
  rCartaoPacote(420 + 160 * (1 - e), 80, 170, 140, 2, e);
  ajc_foco(420, 80, 170, 140, 16, e);
  ajc_ladrilho("aj_plus", 505, 40, 44, 1, 1);
  ajc_ladrilho("aj_package", 320, 270, 44, 0, 0.7f);
}
static void rPacoteRem(void) {   // um pacote sai e os outros fecham o espaco
  float f = ajc_fase(3.6f), e = anim_suave(rK(f / 0.45f)), a1 = 1 - e;
  if (ajc_reduzido()) e = 1, a1 = 0;
  rCartaoPacote(40, 80, 170, 140, 0, 1);
  rCartaoPacote(230, 80, 170, 140, 1, a1);
  rCartaoPacote(420 - 190 * e, 80, 170, 140, 2, 1);
  ajc_ret(230, 80, 170, 140, 16, 0.03f, 0.03f, 0.04f, 0.5f * a1);
  ajc_icone("aj_delete", 315, 150, 44, 0.9f * a1);
  ajc_ladrilho("aj_package", 320, 270, 44, 0, 0.7f);
}

// --- som e video desta TV ---------------------------------------------------------------
static void rDts(void) {   // DTS -> estereo (2 caixas) ou 5.1 (6 caixas)
  int s51 = valor[AJ_DTS_AC3] == 1, i;
  float o = ajc_onda(1.2f);
  ajcSelo("DTS", 90, 26, 16, 0, 1);
  ajc_icone("aj_chevron-right", 190, 43, 24, 0.7f);
  ajcSelo(s51 ? "5.1" : "AAC", 232, 26, 16, 1, 1);
  ajc_tv(250, 110, 140, 78, 1);
  ajc_ret(256, 116, 128, 66, 6, 0.06f, 0.06f, 0.08f, 1);
  for (i = 0; i < (s51 ? 6 : 2); i++) {
    static const float PX[6] = { 150, 490, 320, 120, 520, 560 }, PY[6] = { 150, 150, 214, 270, 270, 290 };
    static const char *NOME[6] = { "", "", "", "", "", "" };
    float d = i == 5 ? 36 : 28;
    ajc_ret(PX[i] - d * 0.4f, PY[i] - d * 0.6f, d * 0.8f, d * 1.2f, 6, 0.95f, 0.95f, 0.94f, 0.85f);
    ajc_ret(PX[i] - d * 0.4f - 8 * o, PY[i] - d * 0.6f - 8 * o, d * 0.8f + 16 * o, d * 1.2f + 16 * o, 10, C.ar, C.ag, C.ab, 0.35f * (1 - o));
    (void)NOME;
  }
  if (!s51) { ajc_icone("aj_volume-2", 320, 262, 30, 0.6f); }
}
static void rDvMkv(int op) {   // arquivo MKV em Dolby Vision: a TV recebe DV ou cai para HDR10
  float k = ajcLig(op), o = ajc_onda(2.6f);
  ajc_ret(40, 70, 130, 170, 14, 0.17f, 0.18f, 0.21f, 1);
  ajc_ret(40, 70, 130, 36, 14, 0.30f, 0.31f, 0.35f, 1);
  ajcTexto("MKV", 20, 1, 76, 76, 1);
  ajc_barras(58, 126, 94, 4, 1);
  ajcSelo("DV", 72, 206, 14, 1, 1);
  ajc_icone("aj_chevron-right", 208, 150, 28, 0.55f);
  rTelaHdr(250, 70, 350, 197, k, k > 0.5f ? 0.35f + 0.5f * o : 0.4f, 1);
  ajcSelo(k > 0.5f ? "DV" : "HDR10", 392, 286, 14, k > 0.5f ? 1 : 0, 1);
  ajcChaveCanto(k);
}
static void rProporcao(int op) {   // o mesmo video 4:3 nos 9 modos de ocupar a tela 16:9
  static const float W[9] = { 240, 240, 320, 320, 270, 300, 380, 240, 320 };
  static const float H[9] = { 180, 180, 240, 180, 203, 225, 285, 180, 240 };
  float f = ajcValor(op), cw, ch;
  int i, v = rInt(op, 8);
  if (f < 0) f = 0;
  if (f > 8) f = 8;
  i = (int)floorf(f);
  { int j = i < 8 ? i + 1 : 8; float t = f - i; cw = rMix(W[i], W[j], t); ch = rMix(H[i], H[j], t); }
  ajc_tv(142, 28, 356, 200, 1);
  ajc_ret(150, 36, 340, 184, 6, 0, 0, 0, 1);
  gfx_recorte(ajc_x(150), ajc_y(36), ajc_px(340), ajc_px(184));
  ajc_cartaz(320 - cw * 0.5f, 128 - ch * 0.5f, cw, ch, 0, 1);
  ajc_ret(320 - cw * 0.26f, 128 - ch * 0.26f, cw * 0.52f, ch * 0.52f, ch * 0.26f, 0.95f, 0.95f, 0.94f, 0.85f);   // o circulo vira elipse quando estica
  gfx_recorte(ajc_x(0), ajc_y(0), ajc_px(640), ajc_px(360));
  { const char *ic[9] = { "aj_rotate-ccw-clock", "aj_image", "aj_maximize-2", "aj_move-horizontal", "aj_maximize-2", "aj_maximize-2", "aj_maximize-2", "aj_arrow-up-down", "aj_move-horizontal" };
    ajc_ladrilho(ic[v], 320, 284, 52, 1, 1); }
  if (v == 4 || v == 5 || v == 6) { char b[8]; snprintf(b, sizeof b, "x%.1f", v == 4 ? 1.1f : v == 5 ? 1.25f : 1.6f); ajcTexto(b, 18, 1, 362, 274, 0.9f); }
  if (v == 7) { ajc_ret(494, 36, 3, 184, 1.5f, C.ar, C.ag, C.ab, 1); }
  if (v == 8) { ajc_ret(150, 224, 340, 3, 1.5f, C.ar, C.ag, C.ab, 1); }
}

// --- durante e depois --------------------------------------------------------------------
static void rClassif(int op) {   // a ilha desce no comeco do filme com a classificacao
  float k = ajcLig(op), f = ajc_fase(5.0f), d = f < 0.1f ? 0 : f < 0.25f ? anim_suave((f - 0.1f) / 0.15f) : f < 0.7f ? 1 : f < 0.85f ? 1 - anim_suave((f - 0.7f) / 0.15f) : 0;
  if (ajc_reduzido()) d = 1;
  d *= k;
  ajc_video(0, 0, 640, 360, 0, 1);
  ajc_veu(0, 230, 640, 130, 0, 0.7f);
  ajc_progresso(40, 318, 560, 0.2f + 0.1f * f, 1);
  ajc_vidro(150, 22 - 70 * (1 - d), 340, 64, 32, d);
  ajc_ret(168, 32 - 70 * (1 - d), 44, 44, 10, 0.94f, 0.48f, 0.18f, d);
  ajcTexto("14", 22, 1, 177, 41 - 70 * (1 - d), d);
  { int j; for (j = 0; j < 3; j++) { ajc_barra(232, 34 + j * 14 - 70 * (1 - d), 160 - j * 30, d * 0.7f); ajc_bolinha(430 + j * 18, 54 - 70 * (1 - d), 10 + j * 0, 1, 1, 1, d * (0.3f + 0.25f * j)); } }
  ajcChaveCanto(k);
}
static void rSaida(void) {   // sair no meio: o video vai para a ilha da Home ou para a pagina do titulo
  float k = ajc_onda(4.0f);
  int v = rInt(AJ_SAIDA_PLAYER, 1);
  ajc_ret(0, 0, 640, 360, 0, 0.075f, 0.082f, 0.102f, 1);
  if (v == 0) {
    AjcHome o = ajcHomePadrao();
    float g = gfx_opacidade_grupo;
    o.hero = 1; o.focoAnda = -1;
    gfx_opacidade_grupo = g * k; ajcHome(&o); gfx_opacidade_grupo = g;
    ajc_vidro(430 - 190 * k + 190 - 190, 304, 170, 40, 20, k);
  } else {
    ajc_cartaz(0, 0, 640, 360, 2, k);
    ajc_veu(0, 0, 460, 360, 2, 0.95f * k);
    ajc_ret(34, 64, 210, 30, 8, 1, 1, 1, 0.92f * k);
    ajc_barras(34, 112, 300, 3, k);
    ajc_acento_ret(34, 232, 112, 34, 17, k);
    ajc_icone("aj_corner-up-left", 170, 250, 28, k);
  }
  // o video comeca na tela inteira (0,0) e encolhe ate o destino
  { float vx = (v ? 300 : 440) * k, vy = (v ? 60 : 250) * k, vw = 640 - (v ? 340 : 460) * k, vh = 360 - (v ? 250 : 260) * k;
    ajc_video(vx, vy, vw, vh, 14 * k, 1 - (v ? 0.8f * k : 0));
    ajc_foco(vx, vy, vw, vh, 14, k * 0.8f); }
}
static void rManter(int op) {   // o video fica pronto na ilha por 2 minutos
  float k = ajcLig(op), f = ajc_fase(5.0f), cd = k > 0.5f ? 1 - f : 0;
  ajc_vidro(120, 100, 400, 130, 26, 0.35f + 0.65f * k);
  ajc_video(142, 120, 160, 90, 12, k);
  ajc_ret(142, 120, 160, 90, 12, 0.07f, 0.07f, 0.09f, 0.6f * (1 - k));
  ajc_icone("aj_pause", 222, 165, 34, 0.95f * k + 0.3f * (1 - k));
  ajcTexto("2:00", 26, 1, 330, 126, k);
  ajc_progresso(330, 168, 160, cd, k);
  ajc_acento_ret(330, 186, 90, 26, 13, k * (0.6f + 0.4f * ajc_onda(1.4f)));
  ajc_icone_tinta("aj_play", 375, 199, 13, k);
  if (k < 0.5f) ajc_icone("aj_x", 320, 165, 90, 0.5f);
  ajc_icone("aj_memory-stick", 320, 290, 30, 0.45f);
  ajcChaveCanto(k);
}

// --- miniaturas na barra de tempo (Seekr) -------------------------------------------------
static void rSeekrBase(float cur) {
  ajc_video(0, 0, 640, 360, 0, 0.55f);
  ajc_veu(0, 260, 640, 100, 0, 0.75f);
  ajc_progresso(40, 318, 560, cur, 1);
  ajc_bolinha(40 + 560 * cur, 320.5f, 14, 1, 1, 1, 1);
}
static void rSeekr(int op) {
  float cur = 0.15f + 0.7f * ajc_onda(5.0f), k = ajcLig(op), cx = 40 + 560 * cur;
  char b[8];
  int mm = (int)(cur * 100);
  switch (op) {
    case AJ_SEEKR_LIGADO:
      rSeekrBase(cur);
      snprintf(b, sizeof b, "%d:%02d", mm / 60, mm % 60);
      if (k > 0.5f) {
        ajc_cartaz(cx - 70, 196, 140, 80, 4 + (int)(cur * 5), 1);
        ajc_foco(cx - 70, 196, 140, 80, 10, 1);
        ajcTexto(b, 15, 1, cx - 20, 284, 1);
      } else { ajc_vidro(cx - 30, 284, 60, 24, 12, 1); ajcTexto(b, 14, 1, cx - 19, 287, 1); }
      ajcChaveCanto(k);
      return;
    case AJ_SEEKR_FITA: {   // a fita de quadros (ligada) x um quadro por vez
      int i;
      rSeekrBase(cur);
      { float off = -cur * 4 * 120 + 120, al = k > 0.5f ? 1.0f : 0.3f;   // desligada: a fita some, um quadro por vez
        ajc_ret(0, 150, 640, 96, 0, 0.03f, 0.03f, 0.04f, 0.9f);
        for (i = 0; i < 9; i++) {
          float x = 20 + i * 120 + off;
          ajc_cartaz(x, 162, 108, 62, 2 + i, al);
          ajc_ret(x, 152, 108, 6, 2, 1, 1, 1, 0.18f * al); ajc_ret(x, 230, 108, 6, 2, 1, 1, 1, 0.18f * al);
        }
        ajc_foco(266, 160, 108, 66, 4, 1);
        if (k <= 0.5f) ajc_icone("aj_loader", 320, 193, 26, 0.95f); }
      ajcChaveCanto(k);
      return; }
    case AJ_SEEKR_CHAVE: {
      static const char *TX = "************";
      char c[16];
      int n = (int)(ajc_fase(4.5f) * 1.3f * 12.0f);
      if (n > 12 || ajc_reduzido()) n = 12;
      memcpy(c, TX, (size_t)n); c[n] = 0;
      ajc_ladrilho("aj_key-round", 90, 150, 76, 1, 1);
      ajc_painel(150, 120, 410, 60, 16, 1);
      ajc_foco(150, 120, 410, 60, 16, 1);
      { int i; for (i = 0; i < n; i++) ajc_bolinha(184 + i * 26, 150, 14, 1, 1, 1, 0.9f); }
      if (ajc_onda(0.8f) > 0.5f) ajc_acento_ret(180 + n * 26, 134, 3, 32, 1.5f, 1);
      ajc_ret(150, 220, 300, 8, 4, 1, 1, 1, 0.1f);
      return; }
    case AJ_SEEKR_TESTAR: {
      float f = ajc_fase(3.6f), p = rK(f / 0.6f), ok = f > 0.65f ? 1.0f : 0.0f;
      ajc_ladrilho("aj_key-round", 90, 160, 70, 0, 1);
      ajc_ret(140, 158, 130, 4, 2, 1, 1, 1, 0.15f);
      ajc_bolinha(140 + 130 * p, 160, 10, 1, 1, 1, ok ? 0.0f : 1.0f);
      ajc_ladrilho("aj_server", 320, 160, 70, 0, 1);
      ajc_ret(370, 158, 100, 4, 2, 1, 1, 1, 0.15f);
      if (!ok) ajc_anel(530, 160, 38, p, 1);
      else { ajc_ladrilho("aj_check", 530, 160, 80, 1, 1); ajc_foco(490, 120, 80, 80, 22, 1); }
      return; }
    default: {   // AJ_SEEKR_AJUSTE: a miniatura mostra a cena de alguns segundos antes/depois
      int off = valor[AJ_SEEKR_AJUSTE];
      float d = (off < -60 ? -60 : off > 60 ? 60 : off) * 3.2f, x0 = 60, W = 520;
      snprintf(b, sizeof b, "%+d s", off);
      ajc_ret(x0, 270, W, 6, 3, 1, 1, 1, 0.16f);
      ajc_bolinha(x0 + W * 0.5f, 273, 18, 1, 1, 1, 1);
      ajc_bolinha(x0 + W * 0.5f + d * 0.8f, 273, 12, C.ar, C.ag, C.ab, 1);
      ajc_ret(x0 + W * 0.5f + (d < 0 ? d * 0.8f : 0), 271, fabsf(d * 0.8f), 4, 2, C.ar, C.ag, C.ab, 0.8f);
      ajc_cartaz(110, 70, 170, 96, 4, 1);
      ajc_cartaz(360, 70, 170, 96, 4 + (off ? 1 : 0), 1);
      ajc_icone("aj_image", 195, 118, 30, 0.8f);
      ajc_icone("aj_eye", 445, 118, 30, 0.8f);
      ajc_ret(110 - 0, 170, 170, 5, 2.5f, C.ar, C.ag, C.ab, 1);
      ajc_ret(286, 116, 68, 4, 2, 1, 1, 1, 0.3f);
      ajcSelo(b, 276, 186, 16, 1, 1);
      return; }
  }
}

// --- servidor P2P --------------------------------------------------------------------------
static void rP2p(int op) {
  int i;
  switch (op) {
    case AJ_P2P_LIGADO: {   // o enxame de pares empurrando o video para a TV (ligado)
      float k = ajcLig(op), t = ajc_fase(2.0f);
      ajc_tv(250, 120, 140, 80, 1);
      ajc_ret(256, 126, 128, 68, 6, 0.06f, 0.06f, 0.08f, 1);
      ajc_video(256, 126, 128, 68, 6, k);
      ajc_play(320, 160, 28, 1 - k);
      for (i = 0; i < 6; i++) {
        float a = 6.2831853f * i / 6.0f - 1.5707963f, px = 320 + cosf(a) * 210, py = 160 + sinf(a) * 120;
        float q = fmodf(t + i / 6.0f, 1.0f);
        int j;
        for (j = 1; j <= 3; j++) ajc_bolinha(px + (320 - px) * j * 0.2f, py + (160 - py) * j * 0.2f, 5, 1, 1, 1, 0.2f);
        ajc_bolinha(px, py, 38, 0.17f, 0.18f, 0.21f, 1);
        ajc_foco(px - 19, py - 19, 38, 38, 19, 0.6f * k + 0.1f);
        ajc_icone("aj_user-round", px, py, 18, 0.7f + 0.3f * k);
        ajc_bolinha(px + (320 - px) * q * 0.8f, py + (160 - py) * q * 0.8f, 10, C.ar, C.ag, C.ab, k * (1 - q * 0.5f));
      }
      ajcChaveCanto(k);
      return; }
    case AJ_P2P_URL: {
      static const char *TX = "192.168.1.20:8080";
      char b[24];
      int n = (int)(ajc_fase(5.0f) * 1.3f * (float)strlen(TX));
      if (n > (int)strlen(TX) || ajc_reduzido()) n = (int)strlen(TX);
      memcpy(b, TX, (size_t)n); b[n] = 0;
      ajc_ladrilho("aj_server", 90, 140, 80, 1, 1);
      ajc_painel(150, 110, 440, 60, 16, 1);
      ajc_foco(150, 110, 440, 60, 16, 1);
      ajc_icone("aj_link", 180, 140, 22, 0.8f);
      ajcTexto(b, 22, 1, 208, 126, 1);
      if (ajc_onda(0.8f) > 0.5f) ajc_acento_ret(210 + ajcLargura(b, 22, 1), 123, 3, 32, 1.5f, 1);
      ajc_ret(150, 200, 300, 8, 4, 1, 1, 1, 0.1f);
      return; }
    case AJ_P2P_LIMITE: {   // #334: a barra do disco e o teto que o P2P nao passa (0 = Automatico)
      static const float TETO[] = { 0.18f, 0.28f, 0.42f, 0.62f, 0.86f };
      float t = TETO[rInt(op, 4)], f = ajc_fase(4.0f), enc = t * (f < 0.8f ? f / 0.8f : 1.0f);
      ajc_icone("aj_database", 320, 110, 44, 0.9f);
      ajc_ret(90, 190, 460, 26, 13, 1, 1, 1, 0.12f);
      ajc_acento_ret(90, 190, 460 * enc, 26, 13, 1);
      ajc_ret(90 + 460 * t - 2, 178, 4, 50, 2, 1, 1, 1, 0.9f);
      ajcTexto(textoValor(op), 22, 1, 90 + 460 * t - 40, 236, 1);
      return; }
    default: {   // AJ_P2P_TESTAR: o ping vai e volta e o resultado aparece
      float f = ajc_fase(3.4f), p = f < 0.35f ? f / 0.35f : f < 0.7f ? 1 - (f - 0.35f) / 0.35f : 0, ok = f > 0.72f ? 1.0f : 0.0f;
      ajc_tv(60, 110, 150, 84, 1);
      ajc_ret(66, 116, 138, 72, 6, 0.06f, 0.06f, 0.08f, 1);
      ajc_play(135, 152, 28, 0.8f);
      ajc_ret(224, 150, 150, 4, 2, 1, 1, 1, 0.15f);
      ajc_bolinha(224 + 150 * p, 152, 12, C.ar, C.ag, C.ab, f < 0.7f ? 1.0f : 0.0f);
      ajc_ladrilho("aj_server", 430, 152, 80, ok, 1);
      if (ok > 0.5f) { ajc_ladrilho("aj_check", 540, 112, 40, 1, 1); ajcTexto("42 ms", 18, 1, 500, 220, 1); }
      return; }
  }
}

// Percentual assistido: a barra de tempo em close; passou da marca do valor,
// o titulo conta como visto (o check acende).
static void rPercentual(void) {
  float pc = ajcValor(AJ_CW_CONCLUIDO), m = 40 + 560 * pc / 100.0f, cur = 0.55f + 0.43f * ajc_onda(4.0f);
  float cx = 40 + 560 * cur, visto = cx >= m ? 1.0f : 0.0f;
  char t[16];
  ajc_video(0, 0, 640, 360, 0, 0.45f);
  ajc_veu(0, 150, 640, 210, 0, 0.8f);
  ajc_ret(40, 236, 560, 12, 6, 1, 1, 1, 0.12f);
  ajc_ret(40, 236, cx - 40, 12, 6, 1, 1, 1, 0.75f);
  ajc_acento_ret(m, 236, 600 - m, 12, 6, 0.45f);
  ajc_ret(m - 1.5f, 214, 3, 56, 1.5f, 1, 1, 1, 0.9f);
  snprintf(t, sizeof t, "%d%%", (int)(pc + 0.5f));
  ajcSelo(t, m - ajcLargura(t, 16, 1) * 0.5f - 11, 176, 16, 1, 1);
  ajc_bolinha(cx, 242, 22, 1, 1, 1, 1);
  ajc_ladrilho("aj_check", 560, 80, 64, visto > 0.5f, 0.3f + 0.7f * visto);
  ajc_cartaz(440, 48, 64, 64, 3, 0.9f);
}

static int ajcNovaReproducao(int sec, int op) {
  (void)sec;
  switch (op) {
    case AJ_FONTE_HDR: rHdr(); return 1;
    case AJ_DV: rDv(op); return 1;
    case AJ_ATMOS: rAtmos(op); return 1;
    case AJ_FONTE_MANUAL: rManual(op); return 1;
    case AJ_FONTE_PRIORIDADE: rPrioridade(); return 1;
    case AJ_FONTE_PRAZO: rPrazo(); return 1;
    case AJ_FONTE_REPOR: rRepor(); return 1;
    case AJ_TAM_MAX: case AJ_TAM_MIN: rTamanho(op); return 1;
    case AJ_FONTE_REGEX: rRegex(); return 1;
    case AJ_FONTE_REGEX_PADRAO: rRegexPadrao(); return 1;
    case AJ_FONTE_REGEX_MODELO: rRegexModelo(); return 1;
    case AJ_FONTE_ESCOPO: rEscopo(); return 1;
    case AJ_FONTE_ADDONS_PERM: case AJ_FONTE_PLUGINS_PERM: rPermitidos(op); return 1;
    case AJ_FONTE_ORDEM: rOrdem(); return 1;
    case AJ_FONTE_ORDEM_USO: rOrdemUso(); return 1;
    case AJ_FONTE_OUTROS: rOutros(op); return 1;
    case AJ_FONTE_AUTO: rAuto(); return 1;
    case AJ_FONTE_TOCAR_CONFERINDO: rTocarConferindo(op); return 1;
    case AJ_FONTE_CONFERIR_VARIAS: rConferirVarias(op); return 1;
    case AJ_FONTE_AQUECER: rAquecer(op); return 1;
    case AJ_FONTE_PREPARAR: rPreparar(op); return 1;
    case AJ_FONTE_ORDEM_ADDON: rOrdemFontes(); return 1;
    case AJ_FONTE_TEXTO: rTexto(); return 1;
    case AJ_SELOS_CORES: rSelosCores(op); return 1;
    case AJ_SELOS_PACOTE: rPacote(); return 1;
    case AJ_SELOS_PACOTE_ADD: rPacoteAdd(); return 1;
    case AJ_SELOS_PACOTE_REM: rPacoteRem(); return 1;
    case AJ_DTS_AC3: rDts(); return 1;
    case AJ_DV_MKV: rDvMkv(op); return 1;
    case AJ_PROPORCAO_PADRAO: rProporcao(op); return 1;
    case AJ_PLR_CLASSIF: rClassif(op); return 1;
    case AJ_SAIDA_PLAYER: rSaida(); return 1;
    case AJ_CW_CONCLUIDO: rPercentual(); return 1;
    case AJ_MANTER_VIDEO: rManter(op); return 1;
    case AJ_SEEKR_LIGADO: case AJ_SEEKR_FITA: case AJ_SEEKR_CHAVE: case AJ_SEEKR_TESTAR: case AJ_SEEKR_AJUSTE:
      rSeekr(op); return 1;
#ifndef __EMSCRIPTEN__
    case AJ_P2P_LIGADO: case AJ_P2P_URL: case AJ_P2P_TESTAR: case AJ_P2P_LIMITE: rP2p(op); return 1;
#endif
    default: return 0;
  }
}
