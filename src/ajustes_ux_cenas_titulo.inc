// CENAS PROPRIAS DAS OPCOES (Titulo), 2.0.3: cada opcao mostra o que ela muda,
// em vez da cena generica da categoria. Incluido por ajustes_ux_cenas.inc;
// usa o kit de ajcena.inc e o apoio do comeco daquele arquivo.
// Devolve 1 quando desenhou a cena da opcao; 0 cai na cena antiga.
// Cobre Titulo (notas e TMDB), Idiomas, Desempenho e Sobre. Sem palavras: so
// siglas, nomes proprios e numeros. Prefixo ajcT_ para nao colidir com os
// outros arquivos de cenas (tudo vive na mesma unidade).

static float ajcT_cl(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
static void ajcT_txtc(const char *t, float px, int forte, float x, float y, int r, int g, int b, float a) {
  ajTextoMini(t, ajc_px(px), forte, ajc_x(x), ajc_y(y), r, g, b, a);
}
// O VIVO (ajcTextoVivoRGB): contador que anda com o relogio da cena.
static void ajcT_txtcVivo(const char *t, float px, int forte, float x, float y, int r, int g, int b, float a) {
  ajcTextoVivoRGB(t, px, forte, x, y, r, g, b, a);
}
static void ajcT_txtAcento(const char *t, float px, float x, float y, float a) {
  float r, g, b;
  ajc_acento(&r, &g, &b);
  ajcT_txtc(t, px, 1, x, y, (int)(r * 255), (int)(g * 255), (int)(b * 255), a);
}
static void ajcT_fundoVideo(void) {
  ajc_video(0, 0, 640, 360, 0, 1);
  ajc_veu(0, 200, 640, 160, 0, 0.4f);
}
// Brilho que varre a regiao: "isto esta sendo preenchido".
static void ajcT_brilho(float x, float y, float w, float h, float k) {
  if (k > 0.01f) ajc_luz(x, y, w, h, 8, w * (ajc_fase(2.6f) * 1.4f - 0.2f), h * 0.5f, h * 0.9f, 1, 1, 1, 0.14f * k);
}
static void ajcT_ghost(float x, float y, float w, float h, float raio, float a) {
  ajc_ret(x, y, w, h, raio, 1, 1, 1, 0.07f * a);
}

// --- notas ----------------------------------------------------------------------------
typedef struct { const char *sg, *val; float r, g, b; int escuro; const char *icone; } AjcTFonte;
static const AjcTFonte NF[11] = {
  { "IMDb", "8.4", 0.96f, 0.77f, 0.18f, 1, NULL },
  { "RT", "92%", 0.90f, 0.26f, 0.20f, 0, NULL },
  { "Popcorn", "87%", 0.94f, 0.48f, 0.18f, 0, "aj_users" },
  { "MC", "74", 0.36f, 0.75f, 0.35f, 1, NULL },
  { "MC", "7.8", 0.25f, 0.55f, 0.90f, 0, "aj_user-round" },
  { "Trakt", "86%", 0.85f, 0.20f, 0.35f, 0, NULL },
  { "TMDB", "7.9", 0.20f, 0.75f, 0.85f, 1, NULL },
  { "Letterboxd", "4.1/5", 0.25f, 0.80f, 0.45f, 1, NULL },
  { "MAL", "8.6", 0.18f, 0.32f, 0.62f, 0, NULL },
  { "Ebert", "3.5/4", 0.82f, 0.82f, 0.84f, 1, "aj_star" },
  { "MDB", "78", 0.42f, 0.42f, 0.92f, 0, NULL },
};
static float ajcT_seloW(const AjcTFonte *f, float px) {
  return ajcLargura(f->sg, px, 1) + ajcLargura(f->val, px, 1) + px * 3.4f;
}
// A pilula da fonte: bloco na cor dela com a sigla, e o numero no formato dela.
static void ajcT_selo(const AjcTFonte *f, float x, float y, float px, float a) {
  float h = px * 2.2f, w1 = ajcLargura(f->sg, px, 1) + px * 1.7f, w2 = ajcLargura(f->val, px, 1) + px * 1.7f;
  int c = f->escuro ? 22 : 250;
  ajc_ret(x, y, w1 + w2, h, h * 0.5f, f->r, f->g, f->b, a);
  ajc_ret(x + w1, y + h * 0.1f, w2 - h * 0.1f, h * 0.8f, h * 0.4f, 0.07f, 0.075f, 0.09f, 0.92f * a);
  ajcT_txtc(f->sg, px, 1, x + px * 0.85f, y + (h - px * 1.25f) * 0.5f, c, c, c, a);
  ajcT_txtc(f->val, px, 1, x + w1 + px * 0.7f, y + (h - px * 1.25f) * 0.5f, 243, 242, 239, a);
}
// Fundo da pagina do titulo, bem apagado, com a linha de notas em contexto.
static void ajcT_paginaNotas(int destaque, float k, float yRow) {
  static const float GX[4] = { 40, 138, 440, 538 };
  int i;
  ajc_cartaz(0, 0, 640, 360, 2, 0.45f);
  ajc_veu(0, 0, 460, 360, 2, 0.9f);
  ajc_veu(0, 150, 640, 210, 0, 0.7f);
  ajc_ret(34, 34, 190, 24, 7, 1, 1, 1, 0.55f);
  ajc_barra(34, 70, 120, 0.35f);
  for (i = 0; i < 4; i++) ajc_neutro(GX[i], yRow, 80, 30, 15, 0.55f);
  if (destaque >= 0) {
    const AjcTFonte *f = &NF[destaque];
    float w = ajcT_seloW(f, 14.0f);
    if (k > 0.02f) ajcT_selo(f, 320 - w * 0.5f, yRow, 14.0f, k);
    else ajc_ret(320 - w * 0.5f, yRow, w, 31, 15, 1, 1, 1, 0.08f);
  }
}
static void ajcT_nota(int op, int fi) {
  const AjcTFonte *f = &NF[fi];
  float k = ajcLig(op), W = ajcT_seloW(f, 14.0f), s = 300.0f / W, h, x0, y0 = 88;
  float pulso = 0.5f + 0.5f * ajc_onda(2.0f);
  if (s > 3.4f) s = 3.4f;
  h = 14.0f * 2.2f * s;
  x0 = 320 - W * s * 0.5f;
  ajcT_paginaNotas(fi, k, 258);
  ajc_luz(0, 0, 640, 200, 0, 320, y0 + h * 0.5f, 260, f->r, f->g, f->b, 0.28f * k);
  // o close-up da pilula da fonte
  ajc_sub(x0, y0, s);
  ajcT_selo(f, 0, 0, 14.0f, 0.30f + 0.70f * k);
  ajc_sub_fim();
  if (f->icone) ajc_ladrilho(f->icone, x0 + W * s + 4, y0 - 4, 30, 0, 0.4f + 0.6f * k);
  if (k > 0.05f) ajc_foco(x0 - 8, y0 - 8, W * s + 16, h + 16, 16, k * (0.45f + 0.55f * pulso));
  else ajc_icone("aj_eye-off", x0 - 34, y0 + h * 0.5f, 30, 0.8f);
  // do close-up para a linha, no tamanho real
  ajc_icone("aj_arrow-down", 320, 196, 22, 0.35f + 0.5f * k * ajc_onda(1.6f));
  ajcChaveCanto(k);
}
static void ajcT_notasTodas(int op) {
  float k = ajcLig(op), y = 112;
  int i;
  ajcT_paginaNotas(-1, 0, 258);
  for (i = 0; i < 10; i++) {
    const AjcTFonte *f = &NF[i];
    float w = ajcT_seloW(f, 11.0f);
    int c = i % 5, r = i / 5;
    float x = 40 + c * 112 + (100 - w) * 0.5f, yy = y + r * 54;
    if (k > 0.02f) ajcT_selo(f, x, yy, 11.0f, k);
    else ajc_ret(x, yy, w, 24, 12, 1, 1, 1, 0.07f);
  }
  if (k < 0.5f) ajc_icone("aj_eye-off", 320, 150, 36, 0.6f);
  ajcT_brilho(30, 100, 580, 120, k);
  ajc_foco(26, 100, 588, 128, 14, k * (0.4f + 0.6f * ajc_onda(2.4f)));
  ajc_ladrilho("aj_chart-column", 590, 40, 36, 1, 0.5f + 0.5f * ajc_onda(2.4f));
  ajcChaveCanto(k);
}
static void ajcT_mdbChave(void) {
  static const int FI[6] = { 0, 1, 3, 5, 6, 10 };
  int i, n = (int)(ajc_fase(4.0f) * 7.0f);
  ajc_cartaz(0, 0, 640, 360, 2, 0.3f);
  ajc_veu(0, 0, 640, 360, 0, 0.85f);
  ajc_ladrilho("aj_key-round", 90, 110, 64, 1, 1);
  ajc_neutro(140, 90, 300, 40, 12, 1);
  for (i = 0; i < 12; i++) ajc_bolinha(164 + i * 20, 110, 8, 1, 1, 1, i < n * 2 ? 0.9f : 0.18f);
  ajc_icone("aj_chevron-right", 470, 110, 26, 0.5f + 0.4f * ajc_onda(1.2f));
  ajc_ladrilho(n >= 6 ? "aj_circle-check" : "aj_loader", 540, 110, 48, n >= 6, 1);
  for (i = 0; i < 6; i++) {
    const AjcTFonte *f = &NF[FI[i]];
    float w = ajcT_seloW(f, 11.0f);
    float x = 56 + (i % 3) * 190 + (170 - w) * 0.5f, y = 200 + (i / 3) * 56;
    ajcT_selo(f, x, y, 11.0f, i < n ? 1.0f : 0.1f);
  }
}
// --- pagina do titulo (data e escurecimento) ------------------------------------------
static void ajcT_paginaMini(float veu, float x, float y, float escala, const char *data) {
  ajc_sub(x, y, escala);
  ajc_cartaz(0, 0, 640, 360, 2, 1);
  ajc_veu(0, 0, 460, 360, 2, 0.95f * veu);
  ajc_veu(0, 160, 640, 200, 0, 0.7f * veu);
  ajc_ret(34, 64, 210, 30, 8, 1, 1, 1, 0.92f);
  if (data) ajcT_txtc(data, 40, 1, 34, 108, 243, 242, 239, 0.95f);
  else ajc_barra(34, 112, 150, 0.6f);
  ajc_barras(34, 176, 300, 3, 1);
  ajc_acento_ret(34, 250, 112, 34, 17, 1);
  ajc_sub_fim();
}
static void ajcT_veu(void) {
  int v = valor[AJ_DET_VEU] < 0 ? 0 : valor[AJ_DET_VEU] > 100 ? 100 : valor[AJ_DET_VEU];
  char b[16];
  float p = ajc_onda(3.0f);
  ajcT_paginaMini(0.0f, 12, 70, 0.48f, NULL);
  ajcT_paginaMini((float)v / 100.0f, 322, 70, 0.48f, NULL);
  ajc_icone("aj_chevron-right", 320, 160, 26, 0.5f + 0.4f * p);
  ajcT_txtc("0%", 22, 1, 130, 270, 243, 242, 239, 0.7f);
  snprintf(b, sizeof b, "%d%%", v);
  ajcT_txtc(b, 22, 1, 430, 270, 243, 242, 239, 0.95f);
  ajc_foco(322, 70, 307, 172.6f, 8, 0.4f + 0.6f * p);
  ajc_ret(70, 312, 500, 6, 3, 1, 1, 1, 0.16f);
  ajc_acento_ret(70, 312, 5.0f * (float)v, 6, 3, 1);
  ajc_bolinha(70 + 5.0f * v, 315, 16, 1, 1, 1, 0.95f);
}
static void ajcT_dataCheia(int op) {
  float k = ajcLig(op);
  ajcT_paginaMini(0.9f, 40, 46, 0.7f, k > 0.5f ? "15.03.2024" : "2024");
  ajc_ladrilho("aj_calendar", 580, 60, 52, 1, 0.6f + 0.4f * ajc_onda(2.4f));
  ajcT_txtc("2024", 20, 1, 40, 296, 243, 242, 239, 0.9f - 0.5f * k);
  ajc_icone("aj_chevron-right", 130, 306, 20, 0.5f);
  ajcT_txtc("15.03.2024", 20, 1, 150, 296, 243, 242, 239, 0.3f + 0.65f * k);
  ajcChaveCanto(k);
}

// --- TMDB: cada opcao acende a parte da pagina que ela preenche -------------------------
static float ajcT_quadro(int op) {
  float k = ajcLig(op);
  ajc_cartaz(0, 0, 640, 360, 2, 0.30f);
  ajc_veu(0, 0, 640, 360, 0, 0.85f);
  ajcSelo("TMDB", 28, 22, 13, k, 1);
  ajcChaveCanto(k);
  return k;
}
static void ajcT_cinza(float x, float y, float w, float h, float raio, float k) {
  ajc_ret(x, y, w, h, raio, 0.30f, 0.31f, 0.34f, 0.55f * (1 - k));
}
static void ajcT_tmdbArte(float k) {
  ajc_cartaz(60, 70, 150, 225, 1, 1);
  ajcT_cinza(60, 70, 150, 225, 10, k);
  ajc_cartaz(250, 70, 340, 190, 4, 1);
  ajcT_cinza(250, 70, 340, 190, 10, k);
  ajc_ret(76, 250, 118 * (0.4f + 0.6f * k), 8, 4, 1, 1, 1, 0.3f + 0.6f * k);
  ajc_ret(272, 226, 160 * (0.4f + 0.6f * k), 12, 4, 1, 1, 1, 0.3f + 0.6f * k);
  ajcSelo(k > 0.5f ? "PT" : "EN", 72, 80, 14, k, 1);
  ajcSelo(k > 0.5f ? "PT" : "EN", 266, 82, 14, k, 1);
  ajcT_brilho(60, 70, 530, 225, k);
}
static void ajcT_tmdbBasico(float k) {
  int i;
  ajc_ret(70, 90, 120 + 200 * k, 24, 7, 1, 1, 1, 0.35f + 0.6f * k);
  for (i = 0; i < 5; i++) ajc_barra(70, 140 + i * 24, (i == 4 ? 220 : 480) * (0.45f + 0.55f * k), 0.2f + 0.55f * k);
  ajc_ladrilho(k > 0.5f ? "aj_database" : "aj_puzzle", 560, 100, 48, k > 0.5f, 1);
  ajcT_brilho(60, 80, 520, 190, k);
}
static void ajcT_tmdbFicha(float k) {
  static const char *IC[3] = { "aj_clock", "aj_globe", "aj_info" };
  static const char *TX[3] = { "128 min", "US BR", "OK" };
  int i;
  ajc_vidro(110, 80, 420, 190, 18, 1);
  for (i = 0; i < 3; i++) {
    float y = 104 + i * 54;
    ajc_icone(IC[i], 150, y + 16, 28, 0.8f);
    ajc_barra(190, y + 12, 70, 0.35f);
    if (k > 0.5f) ajcT_txtc(TX[i], 22, 1, 330, y + 4, 243, 242, 239, 0.95f);
    else ajc_ret(330, y + 12, 90, 8, 4, 1, 1, 1, 0.14f);
  }
  ajcT_brilho(110, 80, 420, 190, k);
}
static void ajcT_tmdbDatas(float k) {
  ajc_ladrilho("aj_calendar", 170, 160, 100, 1, 1);
  ajcT_txtc(k > 0.5f ? "15.03.2024" : "--.--.----", 44, 1, 250, 128, 243, 242, 239, 0.45f + 0.5f * k);
  ajc_barra(250, 196, 150, 0.35f);
  ajc_bolinha(520, 160, 74, 0.10f, 0.105f, 0.125f, 1);
  ajc_bolinha(520, 160, 66, k > 0.5f ? 0.9f : 0.3f, k > 0.5f ? 0.5f : 0.3f, k > 0.5f ? 0.2f : 0.34f, 0.35f + 0.65f * k);
  ajcT_txtc(k > 0.5f ? "14" : "--", 34, 1, 520 - ajcLargura(k > 0.5f ? "14" : "--", 34, 1) * 0.5f, 143, 243, 242, 239, 0.95f);
  ajcT_brilho(100, 100, 480, 130, k);
}
static void ajcT_tmdbElenco(float k) {
  int i;
  for (i = 0; i < 5; i++) {
    float cx = 90 + i * 115, r, g, b;
    ajc_matiz(i + 1, &r, &g, &b);
    ajc_bolinha(cx, 140, 80, 0.30f * (1 - k) + r * k, 0.31f * (1 - k) + g * k, 0.34f * (1 - k) + b * k, 1);
    ajc_icone("aj_user-round", cx, 140, 36, 0.4f + 0.5f * k);
    ajc_barra(cx - 30, 198, 60 * (0.5f + 0.5f * k), 0.3f + 0.6f * k);
    ajc_barra(cx - 24, 214, 48 * (0.5f + 0.5f * k), 0.2f + 0.3f * k);
  }
  ajcT_brilho(40, 90, 560, 150, k);
}
static void ajcT_tmdbLogos(float k, int redes) {
  static const char *R[4] = { "HBO", "AMC", "FX", "TNT" };
  int i, f = ajc_passo(1.5f, 4);
  for (i = 0; i < 4; i++) {
    float x = redes ? 40 + i * 140 : 70 + i * 134, r, g, b;
    if (redes) {
      ajc_matiz(i * 2 + 2, &r, &g, &b);
      ajc_ret(x, 84, 124, 56, 14, 0.17f * (1 - k) + r * k, 0.18f * (1 - k) + g * k, 0.21f * (1 - k) + b * k, 0.45f + 0.55f * k);
      if (k > 0.5f) ajcT_txtc(R[i], 24, 1, x + 62 - ajcLargura(R[i], 24, 1) * 0.5f, 98, 255, 255, 255, 0.95f);
      if (k > 0.5f && i == f) ajc_foco(x, 84, 124, 56, 14, 1);
    } else {
      ajc_bolinha(x + 32, 112, 72, 0.17f, 0.18f, 0.21f, 0.45f + 0.55f * k);
      if (k > 0.5f) ajc_icone("aj_clapperboard", x + 32, 112, 32, 0.9f);
      if (k > 0.5f && i == f) ajc_foco(x - 4, 76, 72, 72, 36, 1);
    }
  }
  ajc_icone(k > 0.5f ? "aj_chevrons-down" : "aj_eye-off", 320, 168, 26, 0.6f);
  for (i = 0; i < 5; i++) {
    if (k > 0.5f) ajc_cartaz(50 + i * 110, 196, 92, 130, i + f, 1);
    else ajcT_ghost(50 + i * 110, 196, 92, 130, 8, 1);
  }
  ajcT_brilho(40, 76, 560, 72, k);
}
static void ajcT_tmdbEps(float k) {
  int i;
  for (i = 0; i < 3; i++) {
    float y = 70 + i * 82;
    char b[8];
    snprintf(b, sizeof b, "1x0%d", i + 1);
    ajc_neutro(60, y, 520, 70, 12, 0.5f + 0.5f * k);
    if (k > 0.5f) ajc_cartaz(70, y + 6, 110, 58, i + 3, 1); else ajcT_ghost(70, y + 6, 110, 58, 8, 1);
    ajcT_txtc(b, 18, 1, 196, y + 10, 243, 242, 239, 0.4f + 0.5f * k);
    ajc_barra(196, y + 38, (180 + 20 * i) * (0.5f + 0.5f * k), 0.25f + 0.5f * k);
    ajcT_txtc(k > 0.5f ? "45 min" : "-- min", 16, 1, 496, y + 24, 243, 242, 239, 0.4f + 0.5f * k);
  }
  ajcT_brilho(60, 70, 520, 230, k);
}
static void ajcT_tmdbTrailers(float k) {
  static const char *D[3] = { "2:14", "1:48", "0:52" };
  int i;
  for (i = 0; i < 3; i++) {
    float x = 40 + i * 190;
    if (k > 0.5f) {
      ajc_cartaz(x, 100, 172, 100, i * 2 + 1, 1);
      ajc_play(x + 86, 150, 34, 1);
      ajc_neutro(x + 118, 178, 46, 16, 8, 0.9f);
      ajcT_txtc(D[i], 12, 1, x + 124, 179, 243, 242, 239, 0.95f);
      ajc_barra(x, 214, 130, 0.5f);
    } else {
      ajcT_ghost(x, 100, 172, 100, 10, 1);
      ajc_barra(x, 214, 130, 0.14f);
    }
  }
  if (k <= 0.5f) ajc_icone("aj_eye-off", 320, 150, 44, 0.7f);
  ajcT_brilho(40, 100, 560, 100, k);
}
static void ajcT_tmdbMais(float k) {
  int i;
  for (i = 0; i < 3; i++) {
    ajc_neutro(60 + i * 110, 70, 100, 30, 15, 0.5f);
    if (i == 1) ajc_acento_ret(60 + i * 110, 70, 100, 30, 15, 1);
    ajc_barra(78 + i * 110, 81, 64, i == 1 ? 0.0f : 0.4f);
  }
  ajc_icone_tinta("aj_sparkles", 170 + 50, 85, 18, 1);
  for (i = 0; i < 5; i++) {
    if (k > 0.5f) ajc_cartaz(50 + i * 110, 130, 92, 138, i + 2, 1);
    else ajcT_ghost(50 + i * 110, 130, 92, 138, 8, 1);
  }
  ajc_ladrilho(k > 0.5f ? "aj_database" : "aj_puzzle", 580, 85, 40, k > 0.5f, 1);
  ajcT_brilho(50, 130, 552, 138, k);
}
static void ajcT_tmdbCol(float k) {
  int i, n = k > 0.5f ? 4 : 1;
  ajc_ret(80, 164, 480, 4, 2, 1, 1, 1, 0.25f * k);
  for (i = 0; i < 4; i++) {
    float x = 56 + i * 134;
    char b[4];
    snprintf(b, sizeof b, "%d", i + 1);
    if (i < n) {
      ajc_cartaz(x, 92, 100, 150, i + 1, 1);
      ajc_neutro(x + 6, 98, 26, 26, 13, 0.95f);
      ajcT_txtc(b, 16, 1, x + 15, 102, 243, 242, 239, 0.95f);
      if (i == 1 && k > 0.5f) ajc_foco(x, 92, 100, 150, 9, 1);
    } else ajcT_ghost(x, 92, 100, 150, 8, 1);
  }
  ajc_icone("aj_layers", 320, 280, 34, 0.5f + 0.4f * k);
  ajcT_brilho(50, 92, 540, 150, k);
}
static void ajcT_tmdbCW(float k) {
  int i;
  ajc_barra(40, 78, 110, 0.5f);
  for (i = 0; i < 4; i++) {
    float x = 40 + i * 146;
    if (k > 0.5f) ajc_cartaz(x, 100, 132, 74, i + 3, 1);
    else { ajcT_ghost(x, 100, 132, 74, 8, 1); ajc_icone("aj_image", x + 66, 137, 22, 0.35f); }
    ajc_progresso(x + 8, 164, 116, 0.25f + 0.17f * i, 1);
  }
  ajc_foco(40, 100, 132, 74, 9, 1);
  ajcT_brilho(40, 100, 580, 74, k);
}
static void ajcT_tmdbTudo(float k) {
  int i;
  float c = k > 0.5f ? 1.0f : 0.0f;
  ajc_cartaz(60, 64, 120, 180, 1, 0.3f + 0.7f * c);
  ajcT_cinza(60, 64, 120, 180, 10, k);
  ajc_ret(204, 70, 100 + 160 * k, 20, 6, 1, 1, 1, 0.3f + 0.6f * k);
  for (i = 0; i < 3; i++) ajc_barra(204, 108 + i * 18, (360 - 40 * i) * (0.5f + 0.5f * k), 0.2f + 0.45f * k);
  for (i = 0; i < 5; i++) {
    float r, g, b;
    ajc_matiz(i + 2, &r, &g, &b);
    ajc_bolinha(224 + i * 50, 200, 38, 0.30f * (1 - k) + r * k, 0.31f * (1 - k) + g * k, 0.34f * (1 - k) + b * k, 1);
  }
  for (i = 0; i < 3; i++) {
    if (k > 0.5f) ajc_cartaz(60 + i * 128, 262, 116, 62, i + 4, 1); else ajcT_ghost(60 + i * 128, 262, 116, 62, 8, 1);
  }
  for (i = 0; i < 4; i++) ajc_ret(450 + i * 44, 262, 36, 62, 8, 0.4f + 0.2f * i, 0.5f, 0.6f, 0.15f + 0.6f * k);
  ajcT_brilho(40, 60, 560, 270, k);
}
static void ajcT_metaExt(int op) {
  float k = ajcLig(op), e = ajcT_cl(k), p = ajc_onda(1.8f);
  float xa = 60 + (1 - e) * 320, xb = 380 - (1 - e) * 320;
  int i;
  ajc_cartaz(0, 0, 640, 360, 2, 0.25f);
  ajc_veu(0, 0, 640, 360, 0, 0.85f);
  ajc_neutro(xa, 60, 200, 110, 16, 1);
  ajc_acento_ret(xa, 60, 200, 110, 16, e * 0.9f);
  ajc_icone_tinta("aj_puzzle", xa + 100, 115, 46, e);
  ajc_icone("aj_puzzle", xa + 100, 115, 46, 1 - e);
  ajc_neutro(xb, 60, 200, 110, 16, 1);
  ajc_acento_ret(xb, 60, 200, 110, 16, (1 - e) * 0.9f);
  ajc_icone_tinta("aj_database", xb + 100, 115, 46, 1 - e);
  ajc_icone("aj_database", xb + 100, 115, 46, e);
  ajcT_txtc("1", 22, 1, e > 0.5f ? xa + 14 : xb + 14, 68, ajTinta(), ajTinta(), ajTinta(), 0.95f);
  ajcT_txtc("2", 22, 1, e > 0.5f ? xb + 14 : xa + 14, 68, 243, 242, 239, 0.55f);
  ajc_foco(60, 60, 200, 110, 16, 0.5f + 0.5f * p);
  ajc_icone("aj_arrow-down", 160, 196, 24, 0.5f + 0.5f * p);
  ajc_vidro(60, 220, 520, 100, 16, 1);
  for (i = 0; i < 4; i++) ajc_barra(84, 238 + i * 17, (380 - 40 * i) * (0.5f + 0.5f * ajc_fase(2.0f)), i == 0 ? 0.7f : 0.4f);
  ajcChaveCanto(k);
}
static void ajcT_soCinemeta(int op) {
  float k = ajcLig(op), p = ajc_onda(1.8f);
  int i;
  ajc_cartaz(0, 0, 640, 360, 2, 0.25f);
  ajc_veu(0, 0, 640, 360, 0, 0.85f);
  for (i = 0; i < 3; i++) {
    float y = 56 + i * 76;
    ajc_neutro(50, y, 130, 62, 14, 1 - 0.7f * k);
    ajc_icone("aj_puzzle", 115, y + 31, 30, 1 - 0.75f * k);
    ajc_ret(190, y + 29, 110, 4, 2, 1, 1, 1, (1 - k) * 0.5f * (0.4f + 0.6f * ajc_fase(1.6f)));
    if (k > 0.1f) ajc_icone("aj_x", 115, y + 31, 40, k * 0.9f);
  }
  ajc_neutro(330, 100, 150, 110, 18, 1);
  ajc_acento_ret(330, 100, 150, 110, 18, 0.4f + 0.5f * k);
  ajc_icone("aj_database", 405, 155, 50, 1);
  ajc_foco(330, 100, 150, 110, 18, k * (0.5f + 0.5f * p));
  ajc_icone("aj_chevron-right", 505, 155, 28, 0.5f + 0.4f * p);
  ajc_vidro(530, 110, 90, 90, 14, 1);
  ajc_barras(542, 128, 66, 4, 1);
  ajcChaveCanto(k);
}

// --- idiomas --------------------------------------------------------------------------
static void ajcT_idioma(void) {
  static const char *L[4] = { "PT", "EN", "ES", "DE" };
  int i, a = ajc_passo(2.0f, 4);
  float t = ajc_passo_t(2.0f);
  ajc_tv(60, 40, 520, 290, 1);
  ajc_ret(66, 46, 508, 278, 10, 0.08f, 0.085f, 0.1f, 1);
  ajc_painel(80, 60, 150, 250, 14, 1);
  for (i = 0; i < 5; i++) {
    float w0 = 60 + ((i * 37 + a * 23) % 50), w1 = 60 + ((i * 37 + ((a + 3) % 4) * 23) % 50);
    ajc_barra(100, 84 + i * 40, w1 + (w0 - w1) * t, i == 1 ? 0.9f : 0.45f);
  }
  ajc_foco(88, 112, 134, 30, 10, 1);
  ajc_icone("aj_languages", 320, 100, 44, 0.9f);
  for (i = 0; i < 4; i++) ajcSeloVivo(L[i], 258 + i * 76, 150, 16, i == a ? t : 0, 1);
  ajc_cartaz(258, 200, 140, 100, 2, 1);
  ajc_barras(414, 210, 130, 5, 1);
}
static void ajcT_tmdbIdioma(void) {
  static const char *L[3] = { "PT", "EN", "ES" };
  int i, a = ajc_passo(2.0f, 3);
  float t = ajc_passo_t(2.0f);
  ajc_cartaz(0, 0, 640, 360, 2, 0.3f);
  ajc_veu(0, 0, 640, 360, 0, 0.85f);
  ajcSelo("TMDB", 28, 22, 13, 1, 1);
  ajc_ret(70, 90, 160 + 80 * ((a & 1) ? t : 1 - t), 24, 7, 1, 1, 1, 0.9f);
  for (i = 0; i < 5; i++) ajc_barra(70, 140 + i * 24, (i == 4 ? 200 : 440) - 30 * ((i + a) % 3) * t, 0.55f);
  for (i = 0; i < 3; i++) ajcSeloVivo(L[i], 440 + i * 56, 80, 14, i == a ? t : 0, 1);
  ajc_icone("aj_languages", 560, 30, 28, 0.8f);
  ajc_foco(60, 80, 400, 190, 14, 0.5f + 0.5f * ajc_onda(2.0f));
}
static void ajcT_audioLingua(void) {
  static const char *L[3] = { "PT", "EN", "ES" };
  static const char *C[3] = { "5.1", "2.0", "5.1" };
  int i, a = ajc_passo(1.8f, 3);
  ajcT_fundoVideo();
  ajc_vidro(330, 40, 280, 230, 18, 1);
  for (i = 0; i < 3; i++) {
    float y = 60 + i * 66;
    ajc_neutro(346, y, 248, 54, 12, i == a ? 0.9f : 0.5f);
    ajc_icone(i == a ? "aj_audio-lines" : "aj_volume-2", 376, y + 27, 26, i == a ? 1.0f : 0.5f);
    ajcSeloVivo(L[i], 412, y + 14, 14, i == a ? 1.0f : 0, 1);
    ajcT_txtc(C[i], 16, 1, 520, y + 16, 243, 242, 239, 0.7f);
    if (i == a) ajc_foco(346, y, 248, 54, 12, 1);
  }
  for (i = 0; i < 9; i++) {
    float hh = 12 + 56 * fabsf(sinf(ajc_t() * 3 + i));
    ajc_ret(44 + i * 16, 170 - hh * 0.5f, 8, hh, 4, 1, 1, 1, 0.6f);
  }
  ajc_legenda(180, 330, 1.0f, 0.45f, 1, 1, 1, 0, 0.5f);
}
static void ajcT_legendaLingua(void) {
  static const char *L[3] = { "PT", "EN", "ES" };
  int i, a = ajc_passo(1.8f, 3);
  ajcT_fundoVideo();
  ajc_legenda(320, 330, 1.0f, 0.45f, 1, 1, 1, 0, 1);
  ajc_vidro(430, 30, 180, 60, 16, 1);
  ajc_icone("aj_captions", 458, 60, 22, 0.9f);
  for (i = 0; i < 3; i++) ajcSeloVivo(L[i], 482 + i * 42 - 20, 46, 12, i == a ? 1.0f : 0, 1);
  ajc_icone("aj_chevron-right", 560, 190, 0.0f, 0.0f);
}
static void ajcT_forcada(int op) {
  float k = ajcLig(op), f = ajc_fase(4.0f);
  int i;
  ajcT_fundoVideo();
  ajc_ret(20, 20, 600, 190, 12, 0, 0, 0, 0.25f);
  ajcSelo("PT", 36, 34, 14, 1, 1);
  ajc_icone("aj_audio-lines", 100, 49, 22, 0.9f);
  // um letreiro: a placa em outra lingua, que a forcada mostra
  if (f > 0.35f && f < 0.75f) { ajc_legenda(320, 190, 1.0f, 0.45f, 1, 0.9f, 0.2f, 0, 1); ajcSeloVivo("JP", 36, 70, 13, 0, 1); }
  // a linha do tempo: todas as falas (desligado) ou so as forcadas (ligado)
  ajc_ret(30, 250, 580, 4, 2, 1, 1, 1, 0.16f);
  for (i = 0; i < 12; i++) {
    int forcada = i == 2 || i == 5 || i == 9;
    float x = 36 + i * 48, a = forcada ? 1.0f : 1.0f - k;
    if (a > 0.02f) {
      if (forcada) ajc_acento_ret(x, 262, 40, 18, 9, a); else ajc_ret(x, 262, 40, 18, 9, 1, 1, 1, 0.45f * a);
    }
  }
  ajc_ret(30 + 580 * f, 240, 3, 52, 1.5f, 1, 1, 1, 0.8f);
  ajcChaveCanto(k);
}
static void ajcT_leg2(int op) {
  static const float COR[7][3] = { { 1, 1, 1 }, { 1, 1, 1 }, { 1, 0.9f, 0.2f }, { 0.4f, 0.9f, 0.4f },
                                   { 0.4f, 0.7f, 1 }, { 1, 0.35f, 0.3f }, { 0.1f, 0.1f, 0.1f } };
  static const float FUN[6] = { 0.45f, 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
  static const char *PCT[6] = { "=", "0", "25", "50", "75", "100" };
  static const char *L[3] = { "PT", "EN", "ES" };
  float p = ajc_onda(2.0f), junto = ajcValor(AJ_LEG2_POS), yb = 74 + (276 - 74) * junto, tam, lw;
  int c = valor[AJ_LEG2_COR] >= 0 && valor[AJ_LEG2_COR] < 7 ? valor[AJ_LEG2_COR] : 0, i;
  int vf = valor[AJ_LEG2_FUNDO] >= 0 && valor[AJ_LEG2_FUNDO] < 6 ? valor[AJ_LEG2_FUNDO] : 0;
  int vb = valor[AJ_LEG2_BORDA] >= 0 && valor[AJ_LEG2_BORDA] < 4 ? valor[AJ_LEG2_BORDA] : 0;
  // Tamanho real da opcao (ajustes_leg2_tamanho): 0 = auto, 90% da principal;
  // senao o percentual escolhido (60..300), nao mais a tabela antiga de 7.
  int pct2 = ajustes_leg2_tamanho();
  float tm, fundo = FUN[vf], borda = 0;
  tam = (pct2 > 0 ? pct2 / 100.0f : 0.9f) * 0.9f;
  tm = tam;
  lw = 220 * tam;
  ajcT_fundoVideo();
  ajc_legenda(320, 330, 1.0f, 0.45f, 1, 1, 1, 0, 0.7f);
  if (op == AJ_LEG2_POS) {   // as duas posicoes possiveis: a ativa cheia, a outra so o rastro
    ajc_ret(320 - lw * 0.5f, 52, lw, 32, 6, 1, 1, 1, 0.10f * junto + 0.05f);
    ajc_ret(320 - lw * 0.5f, 250, lw, 32, 6, 1, 1, 1, 0.10f * (1 - junto) + 0.05f);
    ajc_icone("aj_arrow-up-down", 320, 168, 96, 0.35f + 0.55f * p);
    ajc_acento_ret(560, 40 + 220 * junto, 8, 40, 4, 1);
    ajc_ret(560, 40, 8, 260, 4, 1, 1, 1, 0.14f);
  }
  if (op == AJ_LEG_LINGUA2) {
    int a = ajc_passo(1.8f, 3);
    for (i = 0; i < 3; i++) ajcSeloVivo(L[i], 130 + i * 140, 140, 28, i == a ? 1.0f : 0, 1);
    ajc_icone("aj_languages", 70, 160, 44, 0.85f);
  }
  if (op == AJ_LEG2_TAMANHO) tm = tam * (0.82f + 0.18f * ajc_onda(2.4f));
  if (op == AJ_LEG2_FUNDO) fundo = (vf == 0 ? 0.45f : FUN[vf]) * (0.35f + 0.65f * p);
  if (op == AJ_LEG2_BORDA) borda = vb >= 2 ? 1.0f : 0.0f;
  if (op == AJ_LEG2_COR && c == 6 && fundo < 0.3f) fundo = 0.5f;
  ajc_legenda(320, yb, tm, fundo, COR[c][0], COR[c][1], COR[c][2], borda, 1);
  ajc_foco(320 - lw * 0.5f - 6, yb - 34 * tam - 4, lw + 12, 34 * tam + 12, 8, 0.4f + 0.6f * p);
  if (op == AJ_LEG2_TAMANHO) {   // a regua do tamanho
    const char *rot = textoValor(AJ_LEG2_TAMANHO);   // o rotulo real: "Automático", "60%".."300%"
    ajc_acento_ret(66, yb - 34 * tam, 3, 34 * tam, 1.5f, 1);
    ajcT_txtc(rot, 70, 1, 320 - ajcLargura(rot, 70, 1) * 0.5f, 126, 243, 242, 239, 0.9f);
    ajc_icone("aj_type", 90, 160, 44, 0.85f);
  }
  if (op == AJ_LEG2_COR) {
    for (i = 1; i < 7; i++) {
      float x = 130 + i * 70;
      ajc_bolinha(x, 160, 52, COR[i][0], COR[i][1], COR[i][2], 1);
      if (i == c || (c == 0 && i == 1)) ajc_foco(x - 30, 130, 60, 60, 30, 1);
    }
    ajc_icone("aj_palette", 66, 160, 40, 0.85f);
  }
  if (op == AJ_LEG2_FUNDO) {
    for (i = 1; i < 6; i++) {
      float x = 130 + i * 80;
      ajc_ret(x - 34, 124, 68, 70, 10, 0, 0, 0, FUN[i]);
      gfx_anel(ajc_r(x - 34, 124, 68, 70), 10.0f / 70.0f, 1.5f, 1, 1, 1, 0.25f * C.a);
      ajcT_txtc(PCT[i], 18, 1, x - ajcLargura(PCT[i], 18, 1) * 0.5f, 206, 243, 242, 239, 0.85f);
      if (i == vf) ajc_foco(x - 34, 124, 68, 70, 10, 1);
    }
    ajc_icone("aj_blend", 56, 160, 40, 0.85f);
  }
  if (op == AJ_LEG2_BORDA) {   // "Aa" em tres acabamentos: sem borda, contorno, sombra
    for (i = 1; i < 4; i++) {
      float x = 150 + (i - 1) * 150, y = 100;
      if (i == 3) ajcT_txtc("Aa", 80, 1, x + 4, y + 4, 0, 0, 0, 0.85f);
      if (i == 2) {
        int dx, dy;
        for (dx = -2; dx <= 2; dx += 2) for (dy = -2; dy <= 2; dy += 2) if (dx || dy) ajcT_txtc("Aa", 80, 1, x + dx, y + dy, 0, 0, 0, 0.9f);
      }
      ajcT_txtc("Aa", 80, 1, x, y, 255, 255, 255, 1);
      if (i == vb || (vb == 0 && i == 1)) ajc_foco(x - 14, y - 6, 130, 110, 12, 1);
    }
  }
  if (op == AJ_LEG_CONTORNO_COR) {   // #475: "Aa" com o contorno na cor escolhida
    int oR, oG, oB, dx, dy;
    ajustes_leg_contorno_rgb(&oR, &oG, &oB);
    for (dx = -3; dx <= 3; dx += 3) for (dy = -3; dy <= 3; dy += 3) if (dx || dy) ajcT_txtc("Aa", 100, 1, 270 + dx, 110 + dy, oR, oG, oB, 1);
    ajcT_txtc("Aa", 100, 1, 270, 110, 255, 255, 255, 1);
  }
}

// --- desempenho ------------------------------------------------------------------------
static void ajcT_qualImg(void) {
  static const int N[3] = { 2, 5, 0 };
  static const float MEM[3] = { 0.30f, 0.62f, 1.0f };
  int q = valor[AJ_QUALIDADE_IMG] >= 0 && valor[AJ_QUALIDADE_IMG] < 3 ? valor[AJ_QUALIDADE_IMG] : 1;
  int i, a, b;
  for (i = 0; i < 3; i++) {
    float x = 40 + i * 200, y = 50, w = 160, h = 220, al = i == q ? 1.0f : 0.6f;
    int n = N[i];
    if (n == 0) ajc_cartaz(x, y, w, h, 1, al);
    else {
      float cw = w / n, ch = h / (n * 1.4f);
      int rows = (int)(h / ch) + 1;
      for (a = 0; a < rows; a++) for (b = 0; b < n; b++) {
        float r, g, bl, hh = ch, v;
        if (a * ch + ch > h) hh = h - a * ch;
        if (hh < 1) continue;
        ajc_matiz(1, &r, &g, &bl);
        v = 0.55f + 0.45f * sinf((float)(a * 2 + b) * 1.3f);
        ajc_ret(x + b * cw, y + a * ch, cw, hh, 0, r * v, g * v * 0.9f, bl * v, al);
      }
    }
    if (i == q) ajc_foco(x, y, w, h, 8, 1);
    ajc_ret(x, 292, w, 8, 4, 1, 1, 1, 0.14f);
    ajc_ret(x, 292, w * MEM[i], 8, 4, C.ar, C.ag, C.ab, al);
  }
  ajc_icone("aj_memory-stick", 320, 328, 22, 0.6f);
}
static void ajcT_medidor(void) {
  int v = (int)(ajcValor(AJ_MEDIDOR) + 0.5f), i;
  ajc_tv(100, 24, 440, 247, 1);
  ajc_ret(106, 30, 428, 235, 10, 0.075f, 0.082f, 0.102f, 1);
  ajc_cartaz(112, 36, 416, 223, 4, 0.5f);
  if (v <= 0) {   // desligado: so a hora
    ajc_vidro(268, 48, 104, 30, 15, 1);
    ajcT_txtc("20:30", 16, 1, 296, 54, 243, 242, 239, 0.95f);
  } else if (v == 1) {   // minimo: o fps na linha da hora
    ajc_vidro(240, 48, 160, 30, 15, 1);
    ajcT_txtc("20:30", 16, 1, 258, 54, 243, 242, 239, 0.95f);
    ajc_ret(326, 58, 3, 12, 1.5f, 1, 1, 1, 0.4f);
    ajcT_txtAcento("60", 16, 340, 54, 1);
  } else if (v == 2) {   // menor: segunda linha
    ajc_vidro(240, 48, 160, 60, 18, 1);
    ajcT_txtc("20:30", 16, 1, 290, 54, 243, 242, 239, 0.95f);
    ajcT_txtAcento("60 fps", 14, 262, 82, 1);
    ajcT_txtc("12 ms", 14, 1, 336, 82, 243, 242, 239, 0.8f);
  } else {   // grande: o painel
    ajc_vidro(150, 44, 340, 200, 18, 1);
    ajcT_txtAcento("60", 40, 172, 56, 1);
    ajcT_txtc("fps", 16, 1, 232, 78, 243, 242, 239, 0.7f);
    ajcT_txtc("12 ms", 16, 1, 172, 112, 243, 242, 239, 0.9f);
    ajcT_txtc("310 MB", 16, 1, 172, 134, 243, 242, 239, 0.9f);
    for (i = 0; i < 14; i++) {
      float hh = 24 + 70 * fabsf(sinf(i * 0.9f + ajc_t() * 1.2f));
      ajc_ret(278 + i * 15, 224 - hh, 10, hh, 3, 1, 1, 1, 0.3f);
      if (i == 13) ajc_acento_ret(278 + i * 15, 224 - hh, 10, hh, 3, 1);
    }
  }
  for (i = 0; i < 4; i++) {
    if (i == v) ajc_acento_ret(238 + i * 44, 316, 36, 8, 4, 1);
    else ajc_ret(238 + i * 44, 316, 36, 8, 4, 1, 1, 1, 0.18f);
  }
  ajc_icone("aj_gauge", 80, 320, 24, 0.7f);
}
static void ajcT_diagnostico(void) {
  static const char *IC[4] = { "aj_file-text", "aj_server", "aj_image", "aj_clock" };
  static const char *MS[4] = { "40 ms", "210 ms", "95 ms", "18 ms" };
  int i, n = (int)(ajc_fase(5.0f) * 5.0f);
  ajc_ladrilho("aj_stethoscope", 90, 90, 72, 1, 1);
  for (i = 0; i < 4; i++) {
    float y = 40 + i * 62;
    int ok = i < n;
    ajc_neutro(180, y, 330, 50, 12, 1);
    ajc_icone(IC[i], 210, y + 25, 24, 0.85f);
    ajc_barra(240, y + 21, 130, ok ? 0.5f : 0.2f);
    ajcT_txtc(MS[i], 14, 1, 410, y + 16, 243, 242, 239, ok ? 0.9f : 0.25f);
    ajc_icone(ok ? "aj_circle-check" : "aj_loader", 488, y + 25, 22, ok ? 1.0f : 0.5f);
    if (i == n) ajc_foco(180, y, 330, 50, 12, 1);
  }
  ajc_ladrilho("aj_shield-check", 570, 290, 52, n >= 4, n >= 4 ? 1.0f : 0.5f);
  ajc_ladrilho("aj_sliders-horizontal", 570, 190, 52, 0, n >= 4 ? 1.0f : 0.6f);
}
static void ajcT_velocidade(void) {
  float f = ajc_onda(3.2f);
  char b[16];
  snprintf(b, sizeof b, "%d", 20 + (int)(f * 60));
  ajc_anel(190, 170, 96, 0.2f + 0.7f * f, 1);
  ajcT_txtcVivo(b, 54, 1, 190 - ajcLarguraViva(b, 54, 1) * 0.5f, 130, 243, 242, 239, 1);
  ajcT_txtc("Mb/s", 20, 1, 190 - ajcLargura("Mb/s", 20, 1) * 0.5f, 196, 243, 242, 239, 0.7f);
  ajc_icone("aj_arrow-down", 190, 280 + 4 * f, 26, 0.5f + 0.5f * f);
  ajc_neutro(370, 80, 230, 70, 16, 1);
  ajc_icone("aj_film", 404, 115, 30, 0.9f);
  ajcT_txtc("12 GB", 28, 1, 440, 98, 243, 242, 239, 0.95f);
  ajc_neutro(370, 170, 230, 70, 16, 1);
  ajc_icone("aj_tv-minimal-play", 404, 205, 30, 0.9f);
  ajcT_txtc("2 GB", 28, 1, 440, 188, 243, 242, 239, 0.95f);
  ajc_foco(370, 80 + 90 * (float)ajc_passo(2.0f, 2), 230, 70, 16, 1);
}
static void ajcT_memImg(int op) {
  static const int MB[7] = { 0, 96, 160, 240, 300, 400, 512 };
  int v = valor[AJ_TEX_MB] >= 0 && valor[AJ_TEX_MB] < 7 ? valor[AJ_TEX_MB] : 0;
  int i, cap = v == 0 ? 6 : 1 + v;
  float f = ajc_fase(4.0f);
  int cheios = op == AJ_ESPACO ? 2 + (int)(f * 6.0f) : cap;
  char b[16];
  if (cheios > 10) cheios = 10;
  ajc_icone("aj_memory-stick", 70, 130, 52, 0.9f);
  ajc_neutro(110, 100, 460, 60, 16, 1);
  for (i = 0; i < 10; i++) {
    if (op == AJ_TEX_MB && i >= cap) { ajc_ret(118 + i * 45, 108, 40, 44, 6, 1, 1, 1, 0.04f); continue; }
    if (i < cheios) ajc_cartaz(118 + i * 45, 108, 40, 44, i, 1);
    else ajc_ret(118 + i * 45, 108, 40, 44, 6, 1, 1, 1, 0.07f);
  }
  if (op == AJ_ESPACO) {
    snprintf(b, sizeof b, "%d MB", 30 * cheios + 20);
    ajcT_txtcVivo(b, 36, 1, 110, 190, 243, 242, 239, 1);
    ajc_progresso(110, 250, 460, (float)cheios / 10.0f, 1);
    ajc_icone("aj_images", 540, 205, 30, 0.7f);
  } else {
    float x = 118 + cap * 45 - 3;
    if (v == 0) snprintf(b, sizeof b, "RAM");
    else snprintf(b, sizeof b, "%d MB", MB[v] > tex_teto_ram_mb() ? tex_teto_ram_mb() : MB[v]);
    ajc_acento_ret(x, 90, 4, 80, 2, 1);
    ajcT_txtc(b, 36, 1, 110, 190, 243, 242, 239, 1);
    ajc_ret(118, 108, 40, 44, 6, 0, 0, 0, 0.5f * ajc_onda(2.0f));   // o teto empurra o mais antigo para fora
    ajc_icone("aj_cpu", 540, 205, 30, 0.7f);
  }
}

// --- sobre -----------------------------------------------------------------------------
static void ajcT_guia(void) {
  int i, f = ajc_passo(0.9f, 12);
  ajc_ladrilho("aj_book-open-text", 110, 150, 110, 1, 1);
  for (i = 0; i < 12; i++) {
    float x = 220 + (i % 4) * 90, y = 40 + (i / 4) * 100;
    char b[4];
    snprintf(b, sizeof b, "%d", i + 1);
    ajc_neutro(x, y, 78, 84, 14, i == f ? 1.0f : 0.6f);
    ajcT_txtc(b, 28, 1, x + 39 - ajcLargura(b, 28, 1) * 0.5f, y + 18, 243, 242, 239, i == f ? 1.0f : 0.55f);
    ajc_barra(x + 12, y + 62, 54, i == f ? 0.7f : 0.25f);
    if (i == f) ajc_foco(x, y, 78, 84, 14, 1);
  }
}
static void ajcT_novidades(void) {
  int i, a = ajc_passo(1.8f, 4);
  float t = ajc_passo_t(1.8f);
  ajcT_txtc("2.0", 110, 1, 70, 48, 243, 242, 239, 1);
  ajc_icone("aj_sparkles", 300, 70, 52, 0.5f + 0.5f * ajc_onda(2.0f));
  for (i = 0; i < 3; i++) ajc_cartaz(330 + i * 94, 140, 84, 120, i + a, i == 0 ? 1.0f : 0.7f);
  ajc_foco(330, 140, 84, 120, 9, 1);
  for (i = 0; i < 5; i++) ajc_bolinha(100 + i * 26, 300, i == a ? 14 : 8, 1, 1, 1, i == a ? 1.0f : 0.3f);
  ajc_icone("aj_chevron-right", 596, 200, 28, 0.4f + 0.5f * t);
}
static void ajcT_apoiar(void) {
  int i, j;
  float s = ajc_fase(2.8f);
  ajc_icone("aj_smartphone", 100, 190, 120, 0.9f);
  ajc_ret(224, 40, 340, 270, 18, 1, 1, 1, 0.94f);
  for (i = 0; i < 11; i++) for (j = 0; j < 11; j++) {
    int fin = (i < 3 && j < 3) || (i > 7 && j < 3) || (i < 3 && j > 7);
    if (fin) continue;
    if ((i * 7 + j * 13 + i * j) % 3 == 0) ajc_ret(262 + i * 24, 72 + j * 20, 20, 16, 3, 0.08f, 0.09f, 0.11f, 1);
  }
  { static const int FX[3] = { 0, 8, 0 }, FY[3] = { 0, 0, 8 };
    for (i = 0; i < 3; i++) {
      ajc_ret(262 + FX[i] * 24, 72 + FY[i] * 20, 68, 56, 6, 0.08f, 0.09f, 0.11f, 1);
      ajc_ret(268 + FX[i] * 24, 78 + FY[i] * 20, 56, 44, 4, 0.94f, 0.94f, 0.95f, 1);
      ajc_ret(278 + FX[i] * 24, 88 + FY[i] * 20, 36, 24, 3, 0.08f, 0.09f, 0.11f, 1);
    } }
  ajc_acento_ret(240, 56 + 232 * s, 308, 4, 2, 0.9f);
  ajc_icone("aj_heart", 600, 48, 32 + 6 * ajc_onda(1.2f), 0.9f);
}
static void ajcT_versao(void) {
  ajc_ladrilho("aj_tag", 100, 120, 80, 1, 1);
  ajc_vidro(180, 80, 340, 80, 40, 1);
  ajcT_txtc(AJ_VERSAO, 44, 1, 350 - ajcLargura(AJ_VERSAO, 44, 1) * 0.5f, 98, 243, 242, 239, 1);
  ajc_luz(180, 80, 340, 80, 40, 340 * ajc_fase(3.0f), 40, 70, 1, 1, 1, 0.12f);
  ajc_barra(180, 200, 220, 0.4f);
  ajc_barra(180, 220, 160, 0.25f);
}
static void ajcT_atualizar(void) {
  float f = ajc_fase(4.0f);
  float pr = f < 0.8f ? f / 0.8f : 1.0f;
  ajc_ladrilho("aj_cloud", 110, 120, 80, 0, 1);
  ajc_icone("aj_arrow-down", 110, 126 + 6 * ajc_onda(1.0f), 24, 0.9f);
  ajc_anel(320, 150, 70, pr, 1);
  ajc_icone(f >= 0.8f ? "aj_check" : "aj_download", 320, 150, 40, 1);
  ajc_vidro(430, 112, 170, 76, 38, 1);
  ajcT_txtc(AJ_VERSAO, 24, 1, 515 - ajcLargura(AJ_VERSAO, 24, 1) * 0.5f, 136, 243, 242, 239, 0.55f + (f >= 0.8f ? 0.4f : 0));
  ajc_icone("aj_search", 110, 250, 30, 0.5f);
  ajc_progresso(170, 250, 400, pr, 1);
}
static void ajcT_verRegistro(void) {
  int i;
  float off = ajc_reduzido() ? 0 : fmodf(ajc_t() * 22.0f, 18.0f);
  ajc_tv(100, 24, 440, 247, 1);
  ajc_ret(106, 30, 428, 235, 10, 0.075f, 0.082f, 0.102f, 1);
  ajc_cartaz(112, 36, 416, 223, 4, 0.4f);
  ajc_vidro(124, 110, 392, 140, 14, 1);
  gfx_recorte(ajc_x(124), ajc_y(118), ajc_px(392), ajc_px(126));
  for (i = 0; i < 9; i++) {
    float y = 120 + i * 18 - off;
    int er = (i + (int)(ajc_t() * 1.0f)) % 7 == 3;
    ajc_bolinha(138, y + 4, 6, er ? C.ar : 1, er ? C.ag : 1, er ? C.ab : 1, er ? 1.0f : 0.4f);
    ajc_ret(150, y, 60 + ((i * 53) % 190), 7, 3.5f, 1, 1, 1, er ? 0.7f : 0.38f);
  }
  gfx_recorte(ajc_x(0), ajc_y(0), ajc_px(640), ajc_px(360));
  ajc_ladrilho("aj_scroll-text", 574, 44, 40, 1, 1);
  ajc_icone("aj_eye", 320, 316, 26, 0.6f);
}
static void ajcT_enviarLog(int op) {
  float f = ajc_fase(2.8f);
  int i, auto_ = op == AJ_ENVIO_AUTO;
  float k = auto_ ? ajcLig(op) : 1.0f;
  if (!auto_) {   // um envio, agora: o botao apertado, o arquivo vai para a nuvem
    int fim = f > 0.85f;
    ajc_ladrilho("aj_send", 90, 100, 76, 1, 0.6f + 0.4f * ajc_onda(1.4f));
    ajc_neutro(190, 70, 150, 110, 16, 1);
    ajc_icone("aj_file-text", 265, 110, 44, 0.9f);
    ajcT_txtc("200 KB", 16, 1, 265 - ajcLargura("200 KB", 16, 1) * 0.5f, 144, 243, 242, 239, 0.85f);
    ajc_ret(360, 124, 120, 3, 1.5f, 1, 1, 1, 0.2f);
    for (i = 0; i < 3; i++) {
      float u = f * 1.2f - i * 0.12f;
      if (u > 0 && u < 1) ajc_bolinha(360 + 120 * u, 125, 9, C.ar, C.ag, C.ab, 1);
    }
    ajc_ladrilho(fim ? "aj_circle-check" : "aj_cloud", 540, 125, 80, fim, 1);
    ajc_icone("aj_shield-check", 190, 250, 30, 0.6f);
    ajc_barra(230, 246, 220, 0.3f);
    ajc_barra(230, 262, 150, 0.2f);
    return;
  }
  // automatico: o ciclo (relogio girando) e varios envios ao longo do tempo
  ajc_icone("aj_rotate-cw", 320, 110, 100, 0.25f + 0.65f * k);
  ajc_icone("aj_clock", 320, 110, 44, 0.3f + 0.6f * k);
  ajc_ret(60, 250, 520, 3, 1.5f, 1, 1, 1, 0.2f);
  for (i = 0; i < 4; i++) {
    float x = 90 + i * 150;
    int on = k > 0.5f && (i < 1 + (int)(f * 4.0f));
    ajc_ladrilho(on ? "aj_circle-check" : "aj_cloud", x, 250, 48, on, 0.35f + 0.65f * (on ? 1.0f : k));
    ajc_icone("aj_file-text", x, 190, 26, 0.3f + 0.5f * k);
  }
  if (k < 0.5f) ajc_icone("aj_x", 320, 110, 60, 0.85f);
  ajcChaveCanto(k);
}

static int ajcNovaTitulo(int sec, int op) {
  (void)sec;
  switch (op) {
    case AJ_NT_IMDB: ajcT_nota(op, 0); return 1;
    case AJ_NT_TOMATES: ajcT_nota(op, 1); return 1;
    case AJ_NT_AUDIENCIA: ajcT_nota(op, 2); return 1;
    case AJ_NT_META: ajcT_nota(op, 3); return 1;
    case AJ_NT_METAUSER: ajcT_nota(op, 4); return 1;
    case AJ_NT_TRAKT: ajcT_nota(op, 5); return 1;
    case AJ_NT_TMDB: ajcT_nota(op, 6); return 1;
    case AJ_NT_LETTER: ajcT_nota(op, 7); return 1;
    case AJ_NT_MAL: ajcT_nota(op, 8); return 1;
    case AJ_NT_EBERT: ajcT_nota(op, 9); return 1;
    case AJ_NT_SCORE: ajcT_nota(op, 10); return 1;
    case AJ_MDB_LIGADO: ajcT_notasTodas(op); return 1;
    case AJ_MDB_CHAVE: ajcT_mdbChave(); return 1;
    case AJ_DET_VEU: ajcT_veu(); return 1;
    case AJ_DET_DATA_CHEIA: ajcT_dataCheia(op); return 1;
    case AJ_TMDB_LIGADO: ajcT_tmdbTudo(ajcT_quadro(op)); return 1;
    case AJ_TMDB_ARTE: ajcT_tmdbArte(ajcT_quadro(op)); return 1;
    case AJ_TMDB_BASICO: ajcT_tmdbBasico(ajcT_quadro(op)); return 1;
    case AJ_TMDB_FICHA: ajcT_tmdbFicha(ajcT_quadro(op)); return 1;
    case AJ_TMDB_DATAS: ajcT_tmdbDatas(ajcT_quadro(op)); return 1;
    case AJ_TMDB_ELENCO: ajcT_tmdbElenco(ajcT_quadro(op)); return 1;
    case AJ_TMDB_PROD: ajcT_tmdbLogos(ajcT_quadro(op), 0); return 1;
    case AJ_TMDB_REDES: ajcT_tmdbLogos(ajcT_quadro(op), 1); return 1;
    case AJ_TMDB_EPS: ajcT_tmdbEps(ajcT_quadro(op)); return 1;
    case AJ_TMDB_TRAILERS: ajcT_tmdbTrailers(ajcT_quadro(op)); return 1;
    case AJ_TMDB_MAIS: ajcT_tmdbMais(ajcT_quadro(op)); return 1;
    case AJ_TMDB_COL: ajcT_tmdbCol(ajcT_quadro(op)); return 1;
    case AJ_TMDB_CW: ajcT_tmdbCW(ajcT_quadro(op)); return 1;
    case AJ_DET_META_EXT: ajcT_metaExt(op); return 1;
    case AJ_DET_SO_CINEMETA: ajcT_soCinemeta(op); return 1;
    case AJ_IDIOMA: ajcT_idioma(); return 1;
    case AJ_TMDB_IDIOMA: ajcT_tmdbIdioma(); return 1;
    case AJ_AUD_LINGUA: case AJ_AUD_LINGUA2: ajcT_audioLingua(); return 1;
    case AJ_LEG_LINGUA: ajcT_legendaLingua(); return 1;
    case AJ_LEG_FORCADA: ajcT_forcada(op); return 1;
    case AJ_LEG_LINGUA2: case AJ_LEG2_POS: case AJ_LEG2_TAMANHO: case AJ_LEG2_COR: case AJ_LEG2_FUNDO: case AJ_LEG2_BORDA:
    case AJ_LEG_CONTORNO_COR:
      ajcT_leg2(op); return 1;
    case AJ_QUALIDADE_IMG: ajcT_qualImg(); return 1;
    case AJ_MEDIDOR: ajcT_medidor(); return 1;
    case AJ_DIAGNOSTICO: ajcT_diagnostico(); return 1;
    case AJ_VELOCIDADE: ajcT_velocidade(); return 1;
    case AJ_ESPACO: case AJ_TEX_MB: ajcT_memImg(op); return 1;
    case AJ_GUIA: ajcT_guia(); return 1;
    case AJ_NOVIDADES20: ajcT_novidades(); return 1;
    case AJ_APOIAR: ajcT_apoiar(); return 1;
    case AJ_VERSAO_I: ajcT_versao(); return 1;
    case AJ_ATUALIZAR: ajcT_atualizar(); return 1;
    case AJ_VER_REGISTRO: ajcT_verRegistro(); return 1;
    case AJ_ENVIAR_LOG: case AJ_ENVIO_AUTO: ajcT_enviarLog(op); return 1;
    default: return 0;
  }
}
