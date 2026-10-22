// CENAS PROPRIAS DAS OPCOES (Servicos), 2.0.3: cada opcao mostra o que ela muda,
// em vez da cena generica da categoria. Incluido por ajustes_ux_cenas.inc;
// usa o kit de ajcena.inc e o apoio do comeco daquele arquivo.
// Devolve 1 quando desenhou a cena da opcao; 0 cai na cena antiga.
// Cobre Fontes (debrid, servidores pessoais), TV ao vivo (Xtream, Stalker, grade,
// formato, proxy) e Contas (perfil, sincronizacao, Trakt/Simkl, social).
// Sem palavras: so nomes de marca, numeros e siglas.

static float ajcSvEase(float x) { x = x < 0 ? 0 : x > 1 ? 1 : x; return x * x * (3.0f - 2.0f * x); }
// trecho [a,b] de uma fase 0..1 como 0..1
static float ajcSvTr(float f, float a, float b) { return ajcSvEase((f - a) / (b - a)); }

// A marca: ladrilho neutro com o icone e o nome proprio.
static float ajcSvMarca(const char *nome, const char *icone, float x, float y, float a) {
  float w = ajcLargura(nome, 14, 1) + 58;
  ajc_neutro(x, y, w, 34, 17, a);
  ajc_icone(icone, x + 19, y + 17, 18, a);
  ajcTexto(nome, 14, 1, x + 36, y + 8, a);
  return w;
}
// Texto sendo digitado: frac 0..1 do texto aparece, com o cursor no acento.
static void ajcSvDigita(const char *s, float x, float y, float px, float frac, float a) {
  char b[48];
  int n = (int)strlen(s), k = (int)(frac * n + 0.5f);
  float w;
  if (n > 44) n = 44;
  if (k > n) k = n;
  memcpy(b, s, (size_t)k);
  b[k] = 0;
  ajcTexto(b, px, 1, x, y, a);
  w = k ? ajcLargura(b, px, 1) : 0;
  if (frac < 0.999f || ajc_reduzido() == 0) ajc_acento_ret(x + w + 2, y - 1, 2, px * 1.3f, 1, a * (ajc_fase(0.8f) < 0.6f ? 1.0f : 0.15f));
}
// Campo: caixa escura com contorno; foco = anel do acento.
static void ajcSvCampo(float x, float y, float w, int foco, float a) {
  ajc_painel(x, y, w, 34, 10, a);
  if (foco) ajc_foco(x, y, w, 34, 10, a);
  else gfx_anel(ajc_r(x, y, w, 34), 10.0f / 34.0f, 1.5f, 1, 1, 1, 0.20f * a * 1.0f);
}
static void ajcSvPontos(float x, float y, int n, float frac, float a) {
  int i, k = (int)(frac * n + 0.5f);
  for (i = 0; i < n; i++) ajc_bolinha(x + i * 15, y, 8, 1, 1, 1, (i < k ? 0.92f : 0.14f) * a);
}
// A TV que recebe o resultado: o que cada servico entrega (posters, canais, cards, estante),
// ou o elo cortado quando nao ha conexao.
static void ajcSvTelaTV(float conn, int m0, int estilo, float a) {
  int i;
  ajc_tv(372, 70, 236, 133, a);
  ajc_ret(378, 76, 224, 121, 8, 0.06f, 0.06f, 0.08f, a);
  {
    float cg = 0.22f + 0.78f * conn;   // o que o servico entrega, esmaecido ate conectar
    if (estilo == 1) {   // canais: fileiras de icone e barra
      for (i = 0; i < 4; i++) {
        ajc_neutro(388, 88 + i * 26, 204, 21, 6, cg * a);
        ajc_icone("aj_tv-minimal-play", 402, 98.5f + i * 26, 12, 0.8f * cg * a);
        ajc_barra(418, 95 + i * 26, 90 - i * 8, 0.5f * cg * a);
      }
    } else if (estilo == 3) {   // Emby: cards largos
      for (i = 0; i < 4; i++) ajc_cartaz(388 + (i % 2) * 104, 88 + (i / 2) * 54, 98, 48, m0 + i, cg * a);
    } else if (estilo == 4) {   // Plex: um cartaz grande e a estante
      ajc_cartaz(388, 86, 80, 100, m0, cg * a);
      for (i = 0; i < 3; i++) ajc_cartaz(480 + (i % 2) * 58, 86 + (i / 2) * 52, 52, 48 + (i / 2) * 0, m0 + 1 + i, cg * a);
    } else {
      for (i = 0; i < 3; i++) ajc_cartaz(388 + i * 68, 92, 60, 90, m0 + i, cg * a);
    }
  }
  if (conn < 0.98f) ajc_icone("aj_unlink", 490, 136, 30, 0.7f * (1 - conn) * a);
}
// O elo entre a ficha e a TV, com a bolinha andando durante `t` (0..1).
static void ajcSvElo(float t, float a) {
  ajc_ret(344, 134, 24, 3, 1.5f, 1, 1, 1, 0.18f * a);
  if (t > 0.01f && t < 0.99f) ajc_bolinha(344 + 24 * t, 135.5f, 9, 1, 1, 1, a);
}

// --- debrid: a chave vira fonte instantanea ----------------------------------------
// Uma chave digitada a esquerda; a direita, as fontes: as que o servico ja tem em
// cache trocam o relogio pelo raio (abrem na hora).
static void ajcSvDebrid(const char *marca, const char *icone, int chaveN, int cacheMask, int testar) {
  static const char *Q[4] = { "4K", "1080p", "1080p", "720p" };
  float f = ajc_fase(5.0f), digita = ajcSvTr(f, 0.05f, 0.45f), pronto = ajcSvTr(f, 0.55f, 0.7f);
  int i;
  { float gr, gg, gb; int mt = chaveN % 8; ajc_matiz(mt, &gr, &gg, &gb);
    ajc_luz(0, 0, 640, 360, 0, 60 + mt * 40, 300 - (mt % 3) * 120, 340, gr, gg, gb, 0.34f); }
  ajc_ladrilho(icone, 66, 50, 56, 1, 1);
  ajcTexto(marca, 24, 1, 104, 36, 1);
  ajcSvCampo(36, 96, 262, !testar && digita < 0.99f, 1);
  ajc_icone("aj_key-round", 58, 113, 16, 0.8f);
  ajcSvPontos(78, 113, chaveN, testar ? 1.0f : digita, 1);
  if (testar) {   // testar: a chave sendo conferida, depois a conta premium com a validade
    float r = ajc_fase(3.0f);
    ajc_anel(167, 214, 46, r, r < 0.98f ? 1.0f : 0.0f);
    if (r >= 0.98f || ajc_reduzido()) {
      ajc_bolinha(167, 214, 74, C.ar, C.ag, C.ab, 1);
      ajc_icone_tinta("aj_check", 167, 214, 40, 1);
    } else ajc_icone("aj_key-round", 167, 214, 28, 0.6f);
    ajc_neutro(340, 120, 264, 130, 16, 1);
    ajc_icone("aj_calendar", 376, 160, 26, 0.9f);
    ajcTexto("31/12", 26, 1, 408, 144, 1);
    ajc_progresso(366, 206, 212, r < 0.98f ? r * 0.8f : 0.8f, 1);
    ajc_icone("aj_badge-check", 376, 230, 20, 0.85f);
    ajc_barra(400, 226, 110, 0.5f);
    return;
  }
  ajc_tv(36, 160, 200, 112, 1);
  ajc_cartaz(42, 166, 188, 100, 1, 1);
  ajc_play(136, 216, 34, 0.95f);
  ajc_progresso(52, 254, 168, 0.1f + 0.5f * pronto, 1);
  for (i = 0; i < 4; i++) {
    float y = 36 + i * 62;
    int cache = (cacheMask >> i) & 1;
    ajc_neutro(332, y, 272, 52, 12, 1);
    ajc_barra(350, y + 14, 120, 0.7f); ajc_barra(350, y + 29, 80, 0.32f);
    ajcSelo(Q[i], 484, y + 7, 11, 0, 1);
    if (cache && pronto > 0.01f) {
      ajc_acento_ret(560, y + 11, 30, 30, 9, pronto);
      ajc_icone_tinta("aj_zap", 575, y + 26, 18, pronto);
    } else ajc_icone("aj_clock", 575, y + 26, 20, 0.45f);
    ajc_icone("aj_clock", 575, y + 26, 20, cache ? 0.45f * (1 - pronto) : 0.0f);
  }
}

// --- servidores pessoais, Xtream e Stalker -----------------------------------------
enum { SV_LIGA, SV_URL, SV_USER, SV_SENHA, SV_ENTRAR, SV_SAIR, SV_CONTA, SV_LIMPAR, SV_MAC, SV_CODIGO, SV_ESCOLHER };
static void ajcSvStb(float conn, const char *mac, float a) {   // o aparelho do portal com o MAC na etiqueta
  ajc_tv(372, 70, 236, 133, a);
  ajc_ret(378, 76, 224, 121, 8, 0.06f, 0.06f, 0.08f, a);
  ajc_icone("aj_monitor-cog", 412, 124, 36, 0.8f * a);
  ajc_neutro(444, 100, 150, 50, 8, a);
  ajcTexto(mac, 11, 1, 454, 111, a);
  ajc_progresso(454, 134, 130, conn, a);
  if (conn > 0.99f) ajc_icone("aj_circle-check", 590, 90, 20, a);
}
static void ajcSvServ(int modo, const char *marca, const char *icone, const char *url, int m0, int estilo, const char *mac) {
  float f = ajc_fase(5.0f), conn = 0;
  int i;
  { float gr, gg, gb; ajc_matiz(m0 + estilo, &gr, &gg, &gb);
    ajc_luz(0, 0, 640, 360, 0, 100 + estilo * 110 + (modo % 3) * 40, 330 - estilo * 40, 360, gr, gg, gb, 0.30f); }
  ajc_painel(30, 40, 310, 280, 18, 0.9f);
  gfx_anel(ajc_r(30, 40, 310, 280), 18.0f / 280.0f, 1.5f, 1, 1, 1, 0.14f * C.a);
  ajcSvMarca(marca, icone, 46, 54, 1);
  switch (modo) {
    case SV_URL:
      ajcSvCampo(46, 112, 278, 1, 1);
      ajc_icone("aj_link", 66, 129, 16, 0.8f);
      ajcSvDigita(url, 88, 121, 14, ajcSvTr(f, 0.05f, 0.55f), 1);
      ajcSvCampo(46, 166, 278, 0, 0.3f); ajc_icone("aj_user-round", 66, 183, 16, 0.3f);
      ajcSvCampo(46, 220, 278, 0, 0.3f); ajc_icone("aj_key-round", 66, 237, 16, 0.3f);
      conn = ajcSvTr(f, 0.65f, 0.85f);
      break;
    case SV_USER:
      ajcSvCampo(46, 112, 278, 0, 0.55f); ajc_icone("aj_link", 66, 129, 16, 0.5f); ajc_barra(88, 125, 150, 0.4f);
      ajcSvCampo(46, 166, 278, 1, 1); ajc_icone("aj_user-round", 66, 183, 16, 0.9f);
      ajc_ret(88, 179, 150 * ajcSvTr(f, 0.05f, 0.55f), 7, 3.5f, 0.95f, 0.95f, 0.94f, 0.9f);
      ajc_acento_ret(88 + 150 * ajcSvTr(f, 0.05f, 0.55f) + 3, 177, 2, 12, 1, ajc_fase(0.8f) < 0.6f ? 1.0f : 0.15f);
      ajcSvCampo(46, 220, 278, 0, 0.3f); ajc_icone("aj_key-round", 66, 237, 16, 0.3f);
      conn = 0;
      break;
    case SV_SENHA:
      ajcSvCampo(46, 112, 278, 0, 0.55f); ajc_icone("aj_link", 66, 129, 16, 0.5f); ajc_barra(88, 125, 150, 0.4f);
      ajcSvCampo(46, 166, 278, 0, 0.55f); ajc_icone("aj_user-round", 66, 183, 16, 0.5f); ajc_barra(88, 179, 90, 0.4f);
      ajcSvCampo(46, 220, 278, 1, 1); ajc_icone("aj_key-round", 66, 237, 16, 0.9f);
      ajcSvPontos(92, 237, 10, ajcSvTr(f, 0.05f, 0.6f), 1);
      ajc_icone("aj_eye-off", 300, 237, 18, 0.7f);
      break;
    case SV_ENTRAR: {
      float press = ajcSvTr(f, 0.35f, 0.5f);
      ajcSvCampo(46, 104, 278, 0, 0.55f); ajc_icone("aj_user-round", 66, 121, 16, 0.5f); ajc_barra(88, 117, 110, 0.5f);
      ajcSvCampo(46, 150, 278, 0, 0.55f); ajc_icone("aj_key-round", 66, 167, 16, 0.5f); ajcSvPontos(92, 167, 8, 1, 0.7f);
      ajc_acento_ret(46, 214, 278, 40, 20, 0.75f + 0.25f * (1 - press));
      ajc_icone_tinta("aj_log-in", 160, 234, 20, 1);
      ajc_foco(46, 214, 278, 40, 20, 1);
      conn = ajcSvTr(f, 0.5f, 0.75f);
      if (conn > 0.9f) { ajc_bolinha(298, 234, 22, 1, 1, 1, 0.95f); ajc_icone("aj_check", 298, 234, 14, 1); }
      break; }
    case SV_SAIR: case SV_LIMPAR: {
      float sai = ajcSvTr(f, 0.35f, 0.7f);
      conn = 1 - sai;
      if (modo == SV_SAIR) {
        ajc_bolinha(185, 140, 64, 0.30f, 0.34f, 0.46f, conn * 0.9f + 0.1f);
        ajc_icone("aj_user-round", 185, 140, 30, 0.8f * conn + 0.15f);
        ajc_barra(140, 190, 90, 0.5f * conn + 0.1f);
      } else {   // remover: os campos preenchidos se apagam um a um
        for (i = 0; i < 3; i++) {
          float k = 1 - ajcSvTr(f, 0.35f + i * 0.1f, 0.5f + i * 0.1f);
          ajcSvCampo(46, 104 + i * 46, 278, 0, 0.45f + 0.5f * k);
          ajc_icone(i == 0 ? "aj_link" : i == 1 ? "aj_user-round" : "aj_key-round", 66, 121 + i * 46, 16, 0.3f + 0.5f * k);
          if (i == 2) ajcSvPontos(92, 167 + 92, 8, 1, k); else ajc_barra(88, 117 + i * 46, 110, 0.5f * k);
        }
      }
      ajc_neutro(46, 262, 278, 40, 20, 1);
      ajc_icone(modo == SV_SAIR ? "aj_log-out" : "aj_delete", 160, 282, 20, 1);
      if (f > 0.25f && f < 0.8f) ajc_foco(46, 262, 278, 40, 20, 1);
      break; }
    case SV_CONTA:
      conn = 1;
      ajc_bolinha(86, 138, 56, C.ar, C.ag, C.ab, 1);
      ajc_icone_tinta("aj_check", 86, 138, 30, 1);
      ajc_barra(126, 128, 120, 0.7f); ajc_barra(126, 144, 80, 0.35f);
      ajc_icone("aj_calendar", 70, 206, 22, 0.85f);
      ajcTexto("31/12", 20, 1, 92, 195, 1);
      for (i = 0; i < 3; i++) {   // as telas em uso: 2 de 3
        ajc_icone("aj_monitor-play", 214 + i * 36, 206, 24, i < 2 ? 0.95f : 0.25f);
      }
      ajcTexto("2/3", 14, 1, 262, 232, 0.7f);
      ajc_progresso(54, 262, 262, 0.62f, 1);
      break;
    case SV_MAC:
      ajcSvCampo(46, 112, 278, 0, 0.55f); ajc_icone("aj_link", 66, 129, 16, 0.5f); ajc_barra(88, 125, 150, 0.4f);
      ajcSvCampo(46, 166, 278, 1, 1); ajc_icone("aj_monitor-cog", 66, 183, 16, 0.9f);
      ajcSvDigita(mac, 88, 175, 14, ajcSvTr(f, 0.05f, 0.6f), 1);
      conn = ajcSvTr(f, 0.7f, 0.9f);
      break;
    case SV_CODIGO: {
      static const char *C4[4] = { "K", "7", "P", "2" };
      float n = ajcSvTr(f, 0.05f, 0.55f) * 4.0f;
      for (i = 0; i < 4; i++) {
        float x = 62 + i * 64;
        ajc_painel(x, 124, 54, 66, 12, 1);
        if (n > i + 0.5f) ajcTexto(C4[i], 36, 1, x + 17, 138, 1);
        else if ((int)n == i) ajc_foco(x, 124, 54, 66, 12, 1);
      }
      for (i = 0; i < 3; i++) ajc_bolinha(130 + i * 28, 232, 10, 1, 1, 1, ajc_passo(0.5f, 3) == i ? 0.9f : 0.25f);
      conn = ajcSvTr(f, 0.65f, 0.85f);
      break; }
    case SV_ESCOLHER: {
      int pick = f > 0.45f ? 1 : (int)(f * 8) % 2;
      for (i = 0; i < 3; i++) {
        float y = 108 + i * 62;
        ajc_neutro(46, y, 278, 52, 12, 1);
        ajc_icone("aj_server", 74, y + 26, 22, 0.85f);
        ajc_barra(98, y + 14, 110, 0.7f); ajc_barra(98, y + 29, 70, 0.32f);
        ajcSelo("32400", 236, y + 14, 11, i == pick && f > 0.45f ? 1.0f : 0.0f, 1);
        if (i == pick) ajc_foco(46, y, 278, 52, 12, 1);
      }
      conn = ajcSvTr(f, 0.6f, 0.8f);
      break; }
    case SV_LIGA: {
      float k = ajcLig(AJ_JF_LIGADO);
      static const char *N[3] = { "Jellyfin", "Emby", "Plex" };
      for (i = 0; i < 3; i++) {
        float y = 112 + i * 62;
        ajc_neutro(46, y, 278, 52, 12, 1);
        { float r, g, b; ajc_matiz(2 + i * 2, &r, &g, &b); ajc_ret(58, y + 10, 32, 32, 9, r, g, b, 1); }
        ajc_icone("aj_server", 74, y + 26, 18, 0.9f);
        ajcTexto(N[i], 15, 1, 104, y + 17, 1);
        ajc_chave(264, y + 15, k, 1);
      }
      conn = k * (0.55f + 0.45f * ajc_onda(3.0f));
      break; }
    default: break;
  }
  if (estilo == 2) ajcSvStb(conn, mac, 1); else ajcSvTelaTV(conn, m0, estilo, 1);
  if (modo == SV_URL || modo == SV_ENTRAR || modo == SV_MAC || modo == SV_CODIGO || modo == SV_ESCOLHER)
    ajcSvElo(ajcSvTr(f, 0.55f, 0.75f), 1);
  if (modo == SV_LIGA) ajcChaveCanto(ajcLig(AJ_JF_LIGADO));
  if (modo == SV_CODIGO) {   // o telefone que confirma o codigo
    ajc_icone("aj_smartphone", 490, 270, 52, 0.9f);
    if (conn > 0.5f) { ajc_bolinha(520, 246, 20, C.ar, C.ag, C.ab, 1); ajc_icone_tinta("aj_check", 520, 246, 12, 1); }
  }
}

// --- TV ao vivo --------------------------------------------------------------------
static void ajcSvGrade(void) {
  int v = (int)(ajcValor(AJ_EPG_PAIS) + 0.5f), i, j;
  char cod[3] = { 0, 0, 0 };
  float agora = 330 + 60 * ajc_fase(8.0f);
  static const float LP[4][4] = { { 90, 130, 60, 110 }, { 150, 70, 120, 60 }, { 60, 110, 90, 130 }, { 120, 90, 80, 90 } };
  if (v > 0 && v < AJ_N_EPG_PAIS) { cod[0] = V_EPG_PAIS[v][0]; cod[1] = V_EPG_PAIS[v][1]; }
  ajc_icone("aj_globe", 56, 40, 24, 0.85f);
  if (cod[0]) { ajc_acento_ret(80, 26, 52, 28, 14, 1); ajcTextoTinta(cod, 16, 1, 90, 31, 1); }
  else { ajc_acento_ret(80, 26, 52, 28, 14, 1); ajc_icone_tinta("aj_sparkles", 106, 40, 18, 1); }
  for (i = 0; i < 4; i++) {
    float y = 78 + i * 62, x = 150;
    ajc_neutro(30, y, 104, 52, 10, 1);
    ajc_icone("aj_tv-minimal-play", 58, y + 26, 20, 0.8f);
    ajc_barra(78, y + 22, 44, 0.5f);
    for (j = 0; j < 4; j++) {
      float w = LP[i][j] * 1.55f;
      if (x + w > 610) w = 610 - x;
      if (w < 20) break;
      ajc_neutro(x, y, w - 6, 52, 8, j == 1 && i == (int)(ajc_t() * 0.5f) % 4 ? 1.0f : 0.8f);
      ajc_barra(x + 12, y + 22, w * 0.5f, 0.55f);
      x += w;
    }
  }
  ajc_acento_ret(agora, 70, 3, 258, 1.5f, 1);
}
static void ajcSvEspera(void) {
  static const char *T[3] = { "15 s", "25 s", "45 s" };
  int v = (int)(ajcValor(AJ_LIVETV_ESPERA) + 0.5f);
  float f = ajc_fase(4.0f), r = f < 0.8f ? f / 0.8f : 1.0f;
  ajc_tv(60, 64, 250, 140, 1);
  ajc_ret(66, 70, 238, 128, 8, 0.05f, 0.05f, 0.07f, 1);
  ajc_anel(185, 134, 40, r, 1);
  ajcTexto(T[v < 0 ? 0 : v > 2 ? 2 : v], 22, 1, 185 - ajcLargura(T[v < 0 ? 0 : v > 2 ? 2 : v], 22, 1) * 0.5f, 122, 1);
  ajc_neutro(380, 64, 220, 60, 14, f > 0.8f ? 0.45f : 1.0f);
  ajc_icone("aj_loader", 410, 94, 22, f > 0.8f ? 0.3f : 0.8f);
  ajc_barra(436, 88, 120, 0.6f);
  ajc_icone("aj_x", 580, 94, 16, f > 0.8f ? 0.9f : 0.0f);
  ajc_neutro(380, 144, 220, 60, 14, 1);
  ajc_icone("aj_tv-minimal-play", 410, 174, 22, 0.8f);
  ajc_barra(436, 168, 120, 0.6f);
  if (f > 0.8f) { ajc_foco(380, 144, 220, 60, 14, ajcSvTr(f, 0.8f, 0.9f)); ajc_icone("aj_corner-up-left", 345, 134, 22, ajcSvTr(f, 0.8f, 0.9f)); }
  if (v == 0) ajc_icone("aj_sparkles", 300, 84, 18, 0.85f);
  ajc_progresso(60, 250, 540, f, 1);
}
static void ajcSvDiag(void) {
  static const char *IC[4] = { "aj_wifi", "aj_server", "aj_tv-minimal-play", "aj_wand" };
  static const char *TX[3] = { "42 Mb/s", "HLS", "1080p" };
  float f = ajc_fase(6.0f);
  int i, passo = (int)(f * 5.0f);
  for (i = 0; i < 4; i++) {
    float x = 90 + i * 150, on = passo >= i + (i == 3 ? 0 : 0) ? 1.0f : 0.0f;
    if (i < 3) ajc_ret(x + 30, 118, 90, 3, 1.5f, 1, 1, 1, passo > i ? 0.55f : 0.14f);
    ajc_ladrilho(IC[i], x, 120, 56, i == 3 ? passo >= 4 : on > 0.5f && passo > i, 1);
    if (i < 3 && passo > i) { ajcTexto(TX[i], 13, 1, x - ajcLargura(TX[i], 13, 1) * 0.5f, 160, 1); }
  }
  for (i = 0; i < 3; i++) {   // os canais testados, um a um
    float y = 206 + i * 44;
    ajc_neutro(90, y, 460, 36, 10, 1);
    ajc_icone("aj_tv-minimal-play", 116, y + 18, 18, 0.7f);
    ajc_barra(140, y + 14, 150 - i * 20, 0.6f);
    if (passo > 1 + i) { ajc_acento_ret(504, y + 6, 24, 24, 12, 1); ajc_icone_tinta(i == 1 ? "aj_x" : "aj_check", 516, y + 18, 14, 1); }
    else ajc_icone("aj_loader", 516, y + 18, 16, 0.4f);
  }
}
static void ajcSvRes(void) {
  static const char *Q[4] = { "4K", "1080p", "720p", "SD" };
  int v = (int)(ajcValor(AJ_LIVETV_RES) + 0.5f), i, alvo = v > 0 ? v - 1 : 1;
  float f = ajc_fase(3.0f);
  for (i = 0; i < 4; i++) {
    float y = 40 + i * 70;
    int on = i == alvo;
    ajc_neutro(40, y, 400, 58, 14, 1);
    ajc_icone("aj_tv-minimal-play", 72, y + 29, 22, 0.8f);
    ajc_barra(100, y + 18, 150, 0.6f); ajc_barra(100, y + 33, 90, 0.3f);
    ajcSelo(Q[i], 330, y + 18, 13, on ? 1.0f : 0.0f, 1);
    if (on) ajc_foco(40, y, 400, 58, 14, 1);
  }
  ajc_ladrilho(v == 0 ? "aj_sparkles" : "aj_list-ordered", 530, 130, 74, 1, 1);
  ajc_bolinha(530, 214, 30, 1, 1, 1, 0.9f);
  ajcTextoTinta("1", 18, 1, 524, 203, 1);
  ajc_icone("aj_arrow-down", 530, 250 + 4 * f, 20, 0.5f);
}
static void ajcSvFormato(void) {
  int v = (int)(ajcValor(AJ_LIVETV_FORMATO) + 0.5f), i;
  float f = ajc_fase(2.4f), a0 = v == 2 ? 0.3f : 1.0f, a1 = v == 1 ? 0.3f : 1.0f;
  int auto_ = v == 0, pick = ajc_passo(2.4f, 2);
  // pista HLS: pedacos soltos; pista TS: uma fita continua
  ajcSelo("HLS", 40, 62, 14, v == 1, 1);
  ajc_icone("aj_server", 70, 160 - 60, 1, 0);
  for (i = 0; i < 7; i++) {
    float x = 140 + i * 56 + 56 * f * 0;
    ajc_neutro(x, 60, 46, 26, 7, 0.8f * a0);
    if (v == 1 || (auto_ && pick == 0)) ajc_acento_ret(x, 60, 46, 26, 7, 0.55f * ajc_onda(1.2f + i * 0.0f) * 0 + 0.5f);
  }
  ajcSelo("TS", 40, 150, 14, v == 2, 1);
  ajc_neutro(140, 150, 440, 26, 13, 0.8f * a1);
  ajc_ret(140 + 380 * ajc_fase(2.0f), 150, 60, 26, 13, 1, 1, 1, 0.2f * a1);
  if (v == 2 || (auto_ && pick == 1)) ajc_acento_ret(140, 150, 440 * (0.3f + 0.7f * ajc_fase(2.0f)), 26, 13, 0.7f);
  if (auto_) ajc_icone("aj_sparkles", 600, pick == 0 ? 73 : 163, 20, 1);
  ajc_tv(250, 222, 150, 84, 1);
  ajc_cartaz(256, 228, 138, 72, 2, 1);
  ajc_play(325, 264, 28, 0.9f);
  ajc_icone("aj_arrow-down", 325, 196, 18, 0.5f);
}
static void ajcSvModo(void) {
  static const char *L[3] = { "A", "B", "C" };
  int v = (int)(ajcValor(AJ_LIVETV_MODO) + 0.5f), i, k;
  float f = ajc_fase(3.0f);
  for (i = 0; i < 3; i++) {
    float x = 40 + i * 190, on = i == v ? 1.0f : 0.0f;
    ajc_neutro(x, 50, 170, 90, 18, 1);
    if (on > 0.5f) { ajc_acento_ret(x, 50, 170, 90, 18, 1); ajc_foco(x, 50, 170, 90, 18, 1); }
    if (on > 0.5f) ajcTextoTinta(L[i], 54, 1, x + 85 - ajcLargura(L[i], 54, 1) * 0.5f, 62, 1);
    else ajcTexto(L[i], 54, 1, x + 85 - ajcLargura(L[i], 54, 1) * 0.5f, 62, 0.8f);
    // o caminho do canal ate a imagem: B pula a escolha da faixa; C pede como transmissao ao vivo
    for (k = 0; k < 3; k++) {
      float sx = x + 14 + k * 54;
      int pula = i == 1 && k == 0;
      ajc_neutro(sx, 176, 46, 46, 12, pula ? 0.35f : 1.0f);
      ajc_icone(k == 0 ? "aj_layers" : k == 1 ? (i == 2 ? "aj_activity" : "aj_download") : "aj_play", sx + 23, 199, 20, pula ? 0.25f : 0.85f);
      if (pula) ajc_ret(sx + 6, 197, 34, 3, 1.5f, 1, 1, 1, 0.5f);
      if (k < 2) ajc_ret(sx + 46, 198, 8, 2, 1, 1, 1, 1, 0.25f);
    }
    if (i == v) {
      int s = (int)(f * 3.0f);
      ajc_acento_ret(x + 14 + s * 54, 228, 46, 5, 2.5f, 1);
    }
  }
  ajc_tv(250, 262, 140, 78, 1);
  ajc_cartaz(256, 268, 128, 66, 4, 1);
  ajc_play(320, 301, 26, 0.9f);
}
static void ajcSvProxy(void) {
  float k = ajcLig(AJ_LIVETV_PROXY), f = ajc_fase(2.4f);
  int i;
  ajc_ladrilho("aj_server", 70, 150, 64, 0, 1);
  // pedacos de HLS saindo do servidor
  for (i = 0; i < 3; i++) {
    float t = fmodf(f + i * 0.33f, 1.0f);
    ajc_neutro(110 + t * (k > 0.5f ? 110 : 330), 140, 26, 20, 6, 0.9f * (1 - t * 0.3f));
  }
  if (k > 0.5f) {
    ajc_ladrilho("aj_cpu", 310, 150, 80, 1, 1);
    ajc_ret(350, 148, 120, 8, 4, C.ar, C.ag, C.ab, 0.8f);
    ajc_ret(350 + 120 * f, 148, 30, 8, 4, 1, 1, 1, 0.5f);
    ajc_tv(470, 100, 130, 76, 1);
    ajc_cartaz(476, 106, 118, 64, 2, 1);
    ajc_play(535, 138, 26, 0.95f);
  } else {
    ajc_ret(436, 150, 20, 2, 1, 1, 1, 1, 0.2f);
    ajc_tv(470, 100, 130, 76, 1);
    ajc_ret(476, 106, 118, 64, 6, 0.05f, 0.05f, 0.07f, 1);
    ajc_bolinha(535, 138, 36, 0.55f, 0.16f, 0.14f, 0.9f);
    ajc_icone("aj_x", 535, 138, 20, 1);
  }
  ajc_barra(110, 232, 60, 0.4f);
  ajcSelo("HLS", 120, 260, 13, 0, 1);
  ajcSelo("TS", 470, 260, 13, k > 0.5f ? 1.0f : 0.0f, 1);
  ajcChaveCanto(k);
}

// --- contas ------------------------------------------------------------------------
static void ajcSvAvatar(float cx, float cy, float d, int i, float a) {
  float r, g, b;
  ajc_matiz(i * 2 + 1, &r, &g, &b);
  ajc_bolinha(cx, cy, d, r, g, b, a);
  ajc_icone("aj_user-round", cx, cy, d * 0.45f, 0.85f * a);
}
static void ajcSvPerfilAtivo(void) {
  int i;
  ajcSvAvatar(180, 150, 130, 0, 1);
  ajc_foco(115, 85, 130, 130, 65, 1);
  ajc_barra(135, 236, 90, 0.8f);
  ajc_ladrilho("aj_tv-minimal-play", 242, 90, 44, 1, 1);
  for (i = 1; i < 4; i++) { ajcSvAvatar(360 + (i - 1) * 90, 160, 64, i, 0.45f); ajc_barra(340 + (i - 1) * 90, 214, 40, 0.25f); }
  ajc_tv(430, 40, 160, 90, 0.0f);
}
static void ajcSvSync(void) {
  float f = ajc_fase(3.0f), g = fmodf(f + 0.5f, 1.0f);
  static const char *IC[4] = { "aj_puzzle", "aj_clock", "aj_folders", "aj_sliders-horizontal" };
  int i;
  ajc_tv(40, 110, 170, 96, 1); ajc_cartaz(46, 116, 158, 84, 1, 1);
  ajc_tv(430, 110, 170, 96, 1); ajc_cartaz(436, 116, 158, 84, 1, 1);
  ajc_ladrilho("aj_cloud", 320, 80, 84, 1, 1);
  ajc_ret(212, 160, 100, 3, 1.5f, 1, 1, 1, 0.18f); ajc_ret(328, 160, 100, 3, 1.5f, 1, 1, 1, 0.18f);
  ajc_bolinha(212 + 100 * f, 161, 10, 1, 1, 1, 1);
  ajc_bolinha(428 - 100 * g, 161, 10, 1, 1, 1, 1);
  for (i = 0; i < 4; i++) ajc_ladrilho(IC[i], 150 + i * 114, 270, 50, ajc_passo(0.8f, 4) == i, 1);
  ajc_icone("aj_clock", 296, 330, 16, 0.6f);
  ajcTexto("20:30", 14, 1, 312, 322, 0.8f);
  ajc_bolinha(400, 330, 18, C.ar, C.ag, C.ab, 1);
  ajc_icone_tinta("aj_check", 400, 330, 11, 1);
}
static void ajcSvPesq(void) {
  float k = ajcLig(AJ_PERFIL_PESQ), f = ajc_fase(4.0f);
  ajc_painel(90, 50, 460, 46, 23, 1);
  gfx_anel(ajc_r(90, 50, 460, 46), 23.0f / 46.0f, 1.5f, 1, 1, 1, 0.22f);
  ajc_icone("aj_search", 122, 73, 20, 0.85f);
  ajc_ret(150, 69, 130 * ajcSvTr(f, 0.05f, 0.5f), 7, 3.5f, 0.95f, 0.95f, 0.94f, 0.85f);
  if (k > 0.5f) {
    float s = ajcSvTr(f, 0.5f, 0.7f);
    ajc_vidro(150, 130 + 14 * (1 - s), 340, 130, 18, s);
    ajcSvAvatar(210, 195 + 14 * (1 - s), 64, 2, s);
    ajc_barra(260, 172 + 14 * (1 - s), 130, 0.85f * s); ajc_barra(260, 190 + 14 * (1 - s), 180, 0.4f * s); ajc_barra(260, 206 + 14 * (1 - s), 90, 0.3f * s);
    ajc_acento_ret(260, 232 + 14 * (1 - s), 90, 14, 7, s);
  } else {
    ajc_icone("aj_eye-off", 320, 190, 56, 0.4f);
    ajc_barra(270, 240, 100, 0.2f);
  }
  ajcChaveCanto(k);
}
static void ajcSvPerfilEditar(void) {
  float f = ajc_fase(4.0f);
  int i;
  ajc_vidro(40, 40, 330, 280, 20, 1);
  ajcSvAvatar(110, 120, 80, 3, 1);
  ajc_ladrilho("aj_pencil", 142, 150, 30, 1, 1);
  ajc_barra(170, 100, 160, 0.9f);
  ajcSvCampo(166, 114, 176, ajc_passo(4.0f, 2) == 0, 1);
  ajc_ret(178, 127, 100 * ajcSvTr(f, 0.1f, 0.6f), 7, 3.5f, 0.95f, 0.95f, 0.94f, 0.85f);
  ajc_barras(70, 200, 270, 3, 1);
  for (i = 0; i < 3; i++) ajc_ladrilho(i == 0 ? "aj_film" : i == 1 ? "aj_heart" : "aj_star", 90 + i * 52, 282, 38, i == (int)(f * 3) % 3, 1);
  for (i = 0; i < 3; i++) {   // o que mostrar: tres chaves, a atividade comecando desligada
    float y = 60 + i * 66;
    ajc_neutro(400, y, 200, 52, 12, 1);
    ajc_icone(i == 0 ? "aj_eye" : i == 1 ? "aj_users" : "aj_activity", 428, y + 26, 20, 0.85f);
    ajc_chave(500, y + 15, i == 2 ? 0.0f : 1.0f, 1);
  }
  ajc_icone("aj_users", 440, 290, 24, 0.7f);
  ajc_acento_ret(470, 280, 22, 22, 11, 1);
  ajc_icone_tinta("aj_user-plus", 481, 291, 12, 1);
}
static void ajcSvSair(void) {
  float f = ajc_fase(4.0f), sai = ajcSvTr(f, 0.35f, 0.75f);
  static const char *IC[3] = { "aj_puzzle", "aj_clock", "aj_database" };
  int i;
  ajc_tv(40, 70, 330, 186, 1);
  ajc_ret(46, 76, 318, 174, 8, 0.06f, 0.06f, 0.08f, 1);
  ajcSvAvatar(90, 118, 52, 0, 1 - sai);
  ajc_barra(126, 108, 90, 0.7f * (1 - sai));
  for (i = 0; i < 3; i++) {
    float k = 1 - ajcSvTr(f, 0.4f + i * 0.1f, 0.6f + i * 0.1f);
    ajc_neutro(64, 150 + i * 32, 282, 26, 8, k);
    ajc_icone(IC[i], 82, 163 + i * 32, 16, 0.8f * k);
    ajc_barra(104, 159 + i * 32, 160, 0.5f * k);
  }
  ajc_icone("aj_log-in", 205, 170, 56, sai * 0.7f);
  ajc_ladrilho("aj_log-out", 500, 150, 84, 1, 1);
  ajc_foco(458, 108, 84, 84, 24, f > 0.2f && f < 0.8f ? 1.0f : 0.0f);
  ajc_icone("aj_arrow-up", 500, 230, 0, 0);
}
static void ajcSvLista(int simkl) {
  float f = ajc_fase(4.0f);
  int i, passo = ajc_passo(0.9f, 6);
  ajcSvMarca(simkl ? "Simkl" : "Trakt", "aj_link", 40, 30, 1);
  ajc_tv(40, 110, 200, 112, 1); ajc_cartaz(46, 116, 188, 100, simkl ? 4 : 2, 1); ajc_play(140, 166, 32, 0.9f);
  ajc_progresso(56, 204, 168, 0.3f + 0.6f * f, 1);
  ajc_ret(252, 166, 56, 3, 1.5f, 1, 1, 1, 0.18f);
  ajc_bolinha(252 + 56 * f, 167.5f, 9, 1, 1, 1, 1);
  if (!simkl) {   // Trakt: o que foi visto ganha o visto, no cartaz
    for (i = 0; i < 6; i++) {
      float x = 330 + (i % 3) * 90, y = 62 + (i / 3) * 130;
      ajc_cartaz(x, y, 76, 114, i + 1, 1);
      if (i <= passo) { ajc_bolinha(x + 62, y + 14, 22, C.ar, C.ag, C.ab, 1); ajc_icone_tinta("aj_check", x + 62, y + 14, 13, 1); }
    }
  } else {   // Simkl: as series com o progresso por episodio
    for (i = 0; i < 4; i++) {
      float y = 56 + i * 68;
      ajc_neutro(330, y, 270, 58, 12, 1);
      ajc_cartaz(338, y + 6, 32, 46, i + 3, 1);
      ajc_barra(382, y + 14, 120, 0.7f);
      ajc_progresso(382, y + 36, 190, i == passo % 4 ? 0.4f + 0.5f * f : 0.25f + 0.2f * i, 1);
    }
  }
}
static void ajcSvSalvos(void) {
  static const char *N[3] = { "Nuvio", "Trakt", "Simkl" };
  static const char *I[3] = { "aj_hard-drive", "aj_link", "aj_link" };
  int v = (int)(ajcValor(AJ_SALVOS_DEST) + 0.5f), i;
  float f = ajc_fase(2.8f), t = ajcSvTr(f, 0.2f, 0.7f), ty;
  if (v < 0) v = 0;
  if (v > 2) v = 2;
  ajc_cartaz(50, 80, 120, 180, 3, 1);
  ajc_ladrilho("aj_plus", 170, 260, 50, 1, 1);
  ajc_foco(145, 235, 50, 50, 14, 1);
  for (i = 0; i < 3; i++) {
    float y = 56 + i * 90;
    int on = i == v;
    ajc_neutro(400, y, 200, 70, 18, 1);
    if (on) { ajc_acento_ret(400, y, 200, 70, 18, 0.9f); ajc_foco(400, y, 200, 70, 18, 1); }
    ajc_icone(I[i], 436, y + 35, 24, on ? 0 : 0.85f);
    if (on) ajc_icone_tinta(I[i], 436, y + 35, 24, 1);
    if (on) ajcTextoTinta(N[i], 18, 1, 468, y + 24, 1); else ajcTexto(N[i], 18, 1, 468, y + 24, 0.85f);
  }
  ty = 56 + v * 90 + 35;
  ajc_bolinha(195 + (418 - 195) * t, 260 + (ty - 260) * t, 16, C.ar, C.ag, C.ab, f < 0.8f ? 1.0f : 0.0f);
  ajc_icone_tinta("aj_bookmark", 195 + (418 - 195) * t, 260 + (ty - 260) * t, 10, f < 0.8f ? 1.0f : 0.0f);
}
static void ajcSvContinuar(void) {
  static const char *N[3] = { "Nuvio", "Trakt", "Simkl" };
  int v = (int)(ajcValor(AJ_CW_FONTE) + 0.5f), i;
  float f = ajc_fase(2.4f);
  for (i = 0; i < 3; i++) {
    float y = 50 + i * 86;
    int on = v == 0 || v == i + 1;
    ajc_neutro(40, y, 170, 66, 16, on ? 1.0f : 0.4f);
    if (on) ajc_foco(40, y, 170, 66, 16, 0.8f);
    ajc_icone(i == 0 ? "aj_hard-drive" : "aj_link", 72, y + 33, 22, on ? 0.9f : 0.3f);
    ajcTexto(N[i], 16, 1, 98, y + 22, on ? 1.0f : 0.35f);
    ajc_ret(212, y + 32, 60, 3, 1.5f, 1, 1, 1, on ? 0.35f : 0.08f);
    if (on) ajc_bolinha(212 + 60 * fmodf(f + i * 0.3f, 1.0f), y + 33.5f, 9, C.ar, C.ag, C.ab, 1);
  }
  ajc_painel(290, 100, 320, 170, 18, 0.9f);
  ajc_barra(308, 116, 80, 0.5f);
  for (i = 0; i < 3; i++) { ajc_cartaz(308 + i * 98, 140, 86, 100, i + 2, 1); ajc_progresso(316 + i * 98, 228, 70, 0.3f + 0.2f * i, 1); }
}
static void ajcSvHist(void) {
  float k = ajcLig(AJ_HIST_CONTA), f = ajc_fase(2.4f);
  ajc_tv(40, 100, 180, 100, 1); ajc_cartaz(46, 106, 168, 88, 4, 1); ajc_play(130, 150, 28, 0.9f);
  ajc_progresso(58, 184, 144, 0.5f, 1);
  ajc_ret(224, 70, 90, 3, 1.5f, 1, 1, 1, 0.18f);
  ajc_ladrilho("aj_cloud", 400, 70, 76, k > 0.5f, 1);
  ajcTexto("Nuvio", 13, 1, 400 - ajcLargura("Nuvio", 13, 1) * 0.5f, 120, 0.8f);
  if (k > 0.5f) ajc_bolinha(224 + 90 * f, 71.5f, 10, C.ar, C.ag, C.ab, 1);
  else { ajc_bolinha(224 + 50 * f, 71.5f, 10, 1, 1, 1, 0.5f); ajc_icone("aj_x", 292, 71, 18, 0.9f); }
  // Trakt e Simkl continuam recebendo
  ajc_ret(224, 160, 40, 3, 1.5f, 1, 1, 1, 0.18f);
  ajc_ret(264, 160, 156, 3, 1.5f, 1, 1, 1, 0.18f);
  ajc_bolinha(224 + 196 * f, 161.5f, 9, 1, 1, 1, 0.9f);
  ajcSvMarca("Trakt", "aj_link", 440, 140, 1);
  ajcSvMarca("Simkl", "aj_link", 440, 190, 1);
  ajcChaveCanto(k);
}
static void ajcSvSocial(void) {
  float k = ajcLig(AJ_SOCIAL), f = ajc_fase(3.0f);
  int i;
  ajcSvAvatar(320, 228, 84, 0, 1);
  for (i = 0; i < 3; i++) {
    float cx = 120 + i * 200, cy = i == 1 ? 50 : 90;
    ajcSvAvatar(cx, cy, 56, i + 1, 0.25f + 0.75f * k);
    ajc_ret(320 + (cx - 320) * 0.5f - 1, 130 + (cy - 130) * 0.2f, 2, 2, 1, 1, 1, 1, 0);
    if (k > 0.5f) {
      float p = fmodf(f + i * 0.3f, 1.0f);
      ajc_vidro(cx - 50, cy + 40, 100, 40, 12, k);
      ajc_cartaz(cx - 44, cy + 46, 22, 28, i + 4, k);
      ajc_barra(cx - 16, cy + 53, 56, 0.7f * k); ajc_barra(cx - 16, cy + 65, 36, 0.3f * k);
      ajc_bolinha(cx + (320 - cx) * p * 0.8f, cy + 60 + (228 - cy - 60) * p * 0.8f, 9, C.ar, C.ag, C.ab, k * (1 - p * 0.6f));
    } else {
      ajc_icone("aj_eye-off", cx, cy + 60, 22, 0.35f);
    }
  }
  ajc_icone("aj_users", 320, 304, 24, 0.4f + 0.5f * k);
  ajcChaveCanto(k);
}
static void ajcSvDiscord(void) {
  float k = ajcLig(AJ_DISCORD), f = ajc_fase(5.0f);
  ajc_painel(70, 40, 500, 280, 20, 1);
  gfx_anel(ajc_r(70, 40, 500, 280), 20.0f / 280.0f, 1.5f, 1, 1, 1, 0.14f);
  ajc_neutro(70, 40, 120, 280, 20, 0.9f);
  ajcTexto("Discord", 15, 1, 88, 56, 1);
  ajcSvAvatar(110, 140, 44, 5, 1);
  ajc_barra(140, 134, 40, 0.5f);
  ajc_barra(210, 70, 100, 0.5f);
  ajcSvAvatar(240, 140, 52, 5, 1);
  ajc_barra(284, 126, 110, 0.85f);
  ajc_icone("aj_circle-dot", 478, 140, 16, k > 0.5f ? 0.9f : 0.25f);
  if (k > 0.5f) {
    ajc_neutro(210, 190, 330, 110, 16, 1);
    ajc_cartaz(224, 204, 66, 82, 2, 1);
    ajc_barra(306, 214, 150, 0.8f); ajc_barra(306, 230, 100, 0.4f);
    ajc_icone("aj_play", 316, 262, 14, 0.8f);
    ajc_progresso(334, 259, 180, 0.2f + 0.7f * f, 1);
  } else {
    ajc_icone("aj_eye-off", 375, 245, 46, 0.3f);
  }
  ajcChaveCanto(k);
}
static void ajcSvEnquete(void) {
  float k = ajcLig(AJ_ENQUETES), f = ajc_fase(4.0f), s = k > 0.5f ? ajcSvTr(f, 0.1f, 0.3f) : 0.0f;
  ajc_painel(220, 26, 200, 40, 20, 1);
  gfx_anel(ajc_r(220, 26, 200, 40), 0.5f, 1.5f, 1, 1, 1, 0.22f);
  ajcTexto("20:30", 18, 1, 290, 36, 1);
  ajc_icone("aj_bell", 246, 46, 16, 0.5f + 0.5f * s);
  if (k > 0.5f) {
    float y = 76 + 14 * (1 - s), v1 = ajcSvTr(f, 0.45f, 0.75f);
    ajc_vidro(150, y, 340, 220, 22, s);
    ajc_barra(180, y + 26, 160, 0.8f * s);
    ajc_neutro(180, y + 66, 280, 40, 12, s); ajc_acento_ret(180, y + 66, 280 * (0.6f * v1), 40, 12, 0.8f * s); ajcTexto("60%", 14, 1, 196, y + 78, s);
    ajc_neutro(180, y + 118, 280, 40, 12, s); ajc_ret(180, y + 118, 280 * (0.4f * v1), 40, 12, 1, 1, 1, 0.3f * s); ajcTexto("40%", 14, 1, 196, y + 130, s);
    ajc_foco(180, y + 66, 280, 40, 12, s);
    ajc_icone("aj_check", 440, y + 86, 18, s * v1);
  } else {
    ajc_icone("aj_bell", 320, 190, 60, 0.18f);
    ajc_ret(272, 170, 96, 4, 2, 1, 1, 1, 0.55f);
  }
  ajcChaveCanto(k);
}

// Plugins: cada plugin entrega fontes; elas descem para a lista e a primeira toca.
static void ajcSvPlugins(void) {
  int i, chega = ajc_passo(0.9f, 5);
  for (i = 0; i < 3; i++) {
    float y = 60 + i * 84;
    ajc_neutro(40, y, 200, 64, 16, 1);
    ajc_ladrilho("aj_plug", 76, y + 32, 38, i == chega % 3, 1);
    ajc_barra(104, y + 24, 100, 0.75f); ajc_barra(104, y + 38, 60, 0.35f);
  }
  ajc_painel(300, 40, 300, 280, 18, 1);
  for (i = 0; i < 4; i++) {
    float y = 60 + i * 60, a = i < chega ? 1.0f : 0.15f;
    ajc_neutro(316, y, 268, 46, 12, a);
    ajc_barra(334, y + 14, 120, 0.75f * a); ajc_barra(334, y + 27, 70, 0.35f * a);
    if (i < chega) ajcSelo(i == 0 ? "4K" : i == 1 ? "1080p" : "720p", 500, y + 12, 11, i == 0, a);
    if (i == 0 && chega > 0) ajc_foco(316, y, 268, 46, 12, 1);
  }
  ajc_bolinha(270, 92 + (chega % 3) * 84 - 32 + 32 * ajc_passo_t(0.9f), 10, 1, 1, 1, 0.8f);
}
// Usar os addons do perfil principal: os outros perfis copiam a pilha do
// principal (ligado) ou cada um tem a sua (desligado).
static void ajcSvPrincipal(int op) {
  float k = ajcLig(op);
  int i, j;
  for (i = 0; i < 3; i++) {
    float cx = 120 + i * 200, cy = 96, r, g, b;
    ajc_matiz(i * 3 + 1, &r, &g, &b);
    ajc_bolinha(cx, cy, i == 0 ? 84 : 70, r, g, b, 1);
    ajc_icone("aj_user-round", cx, cy, i == 0 ? 36 : 30, 0.85f);
    if (i == 0) { ajc_bolinha(cx + 30, cy - 30, 26, 0.10f, 0.11f, 0.13f, 1); ajc_icone("aj_star", cx + 30, cy - 30, 14, 1); }
    for (j = 0; j < 3; j++) {
      // ligado: a mesma pilha do principal (matizes 0,1,2); desligado: a propria
      int m = (i == 0 || k > 0.5f) ? j : i * 3 + j + 3;
      float y = 170 + j * 46;
      ajc_neutro(cx - 70, y, 140, 38, 10, 1);
      { float rr, gg, bb; ajc_matiz(m, &rr, &gg, &bb); ajc_ret(cx - 60, y + 9, 20, 20, 6, rr, gg, bb, 0.9f); }
      ajc_icone("aj_puzzle", cx - 50, y + 19, 12, 0.9f);
      ajc_barra(cx - 30, y + 16, 80, 0.6f);
    }
    if (i > 0 && k > 0.01f) {
      float f = ajc_fase(2.0f);
      ajc_ret(120 + 72, 189 + 46, (cx - 70) - 192, 2, 1, 1, 1, 1, 0.25f * k);
      ajc_bolinha(192 + ((cx - 70) - 192) * f, 236, 9, 1, 1, 1, k * (1 - f * 0.5f));
    }
  }
  ajc_foco(50, 170, 140, 130, 12, 0.5f + 0.5f * ajc_onda(2.4f));
  ajcChaveCanto(k);
}
// Buscar no Cinemeta: a busca consulta o catalogo do Nuvio e o Cinemeta;
// desligado, o Cinemeta sai e os resultados dele somem.
static void ajcSvCinemeta(int op) {
  float k = ajcLig(op);
  int i, letras = 1 + ajc_passo(0.35f, 6);
  ajc_painel(40, 36, 560, 56, 28, 1);
  ajc_icone("aj_search", 74, 64, 22, 0.9f);
  for (i = 0; i < letras; i++) ajc_ret(100 + i * 18, 56, 12, 16, 3, 1, 1, 1, 0.8f);
  ajc_ret(100 + letras * 18, 52, 2, 24, 1, 1, 1, 1, ajc_onda(0.8f) > 0.5f ? 0.9f : 0.1f);
  // duas origens: Nuvio (sempre) e Cinemeta (pela chave)
  ajc_neutro(40, 120, 150, 40, 20, 1); ajc_icone("aj_database", 64, 140, 16, 0.9f); ajcTexto("Nuvio", 15, 1, 82, 131, 0.9f);
  ajc_neutro(40, 176, 150, 40, 20, 0.35f + 0.65f * k); ajc_icone(k > 0.5f ? "aj_film" : "aj_eye-off", 64, 196, 16, 0.4f + 0.5f * k);
  ajcTexto("Cinemeta", 15, 1, 82, 187, 0.35f + 0.55f * k);
  if (k > 0.5f) ajc_acento_ret(40, 176, 150, 40, 20, 0.35f * k);
  for (i = 0; i < 6; i++) {
    int doCinemeta = i % 2 == 1;
    float a = doCinemeta ? k : 1.0f, x = 220 + (i % 3) * 126, y = 120 + (i / 3) * 112;
    ajc_cartaz(x, y, 110, 96, i + 2, 0.15f + 0.85f * a);
    if (doCinemeta) ajc_icone("aj_film", x + 96, y + 14, 12, 0.8f * a);
  }
  ajcChaveCanto(k);
}

static int ajcNovaServicos(int sec, int op) {
  (void)sec;
  switch (op) {
    case AJ_PLUGINS: ajcSvPlugins(); return 1;
    case AJ_ADDONS_PRINCIPAL: ajcSvPrincipal(op); return 1;
    case AJ_BUSCA_CINEMETA: ajcSvCinemeta(op); return 1;
    case AJ_DEBRID_RD: ajcSvDebrid("Real-Debrid", "aj_download", 10, 0x5, 0); return 1;
    case AJ_DEBRID_TB: ajcSvDebrid("TorBox", "aj_package", 7, 0x6, 0); return 1;
    case AJ_DEBRID_PM: ajcSvDebrid("Premiumize", "aj_sparkle", 9, 0xB, 0); return 1;
    case AJ_DEBRID_AD: ajcSvDebrid("AllDebrid", "aj_hard-drive", 12, 0x3, 0); return 1;
    case AJ_DEBRID_AD_TESTAR: ajcSvDebrid("AllDebrid", "aj_hard-drive", 12, 0x3, 1); return 1;
    case AJ_JF_LIGADO: ajcSvServ(SV_LIGA, "Jellyfin", "aj_server", "", 2, 0, ""); return 1;
    case AJ_JF_SERVIDOR: ajcSvServ(SV_URL, "Jellyfin", "aj_server", "192.168.1.20:8096", 2, 0, ""); return 1;
    case AJ_JF_ENTRAR: ajcSvServ(SV_ENTRAR, "Jellyfin", "aj_server", "", 2, 0, ""); return 1;
    case AJ_JF_SAIR: ajcSvServ(SV_SAIR, "Jellyfin", "aj_server", "", 2, 0, ""); return 1;
    case AJ_EM_SERVIDOR: ajcSvServ(SV_URL, "Emby", "aj_layers", "192.168.1.30:8096", 6, 3, ""); return 1;
    case AJ_EM_ENTRAR: ajcSvServ(SV_ENTRAR, "Emby", "aj_layers", "", 6, 3, ""); return 1;
    case AJ_EM_SAIR: ajcSvServ(SV_SAIR, "Emby", "aj_layers", "", 6, 3, ""); return 1;
    case AJ_PX_ENTRAR: ajcSvServ(SV_CODIGO, "Plex", "aj_clapperboard", "", 3, 4, ""); return 1;
    case AJ_PX_SERVIDOR: ajcSvServ(SV_ESCOLHER, "Plex", "aj_clapperboard", "", 3, 4, ""); return 1;
    case AJ_PX_SAIR: ajcSvServ(SV_SAIR, "Plex", "aj_clapperboard", "", 3, 4, ""); return 1;
    case AJ_XTREAM_SERVIDOR: ajcSvServ(SV_URL, "Xtream", "aj_tv-minimal-play", "203.0.113.7:8080", 4, 1, ""); return 1;
    case AJ_XTREAM_USUARIO: ajcSvServ(SV_USER, "Xtream", "aj_tv-minimal-play", "", 4, 1, ""); return 1;
    case AJ_XTREAM_SENHA: ajcSvServ(SV_SENHA, "Xtream", "aj_tv-minimal-play", "", 4, 1, ""); return 1;
    case AJ_XTREAM_CONTA: ajcSvServ(SV_CONTA, "Xtream", "aj_tv-minimal-play", "", 4, 1, ""); return 1;
    case AJ_XTREAM_LIMPAR: ajcSvServ(SV_LIMPAR, "Xtream", "aj_tv-minimal-play", "", 4, 1, ""); return 1;
    case AJ_STALKER_PORTAL: ajcSvServ(SV_URL, "Stalker", "aj_monitor-cog", "10.0.0.5:8080", 0, 2, "00:1A:79"); return 1;
    case AJ_STALKER_MAC: ajcSvServ(SV_MAC, "Stalker", "aj_monitor-cog", "", 0, 2, "00:1A:79:4F:C2:B8"); return 1;
    case AJ_STALKER_LIMPAR: ajcSvServ(SV_LIMPAR, "Stalker", "aj_monitor-cog", "", 0, 2, "00:1A:79"); return 1;
    case AJ_EPG_PAIS: ajcSvGrade(); return 1;
    case AJ_LIVETV_DIAG: ajcSvDiag(); return 1;
    case AJ_LIVETV_RES: ajcSvRes(); return 1;
    case AJ_LIVETV_ESPERA: ajcSvEspera(); return 1;
    case AJ_LIVETV_FORMATO: ajcSvFormato(); return 1;
    case AJ_LIVETV_MODO: ajcSvModo(); return 1;
    case AJ_LIVETV_PROXY: ajcSvProxy(); return 1;
    case AJ_PERFIL_ATIVO: ajcSvPerfilAtivo(); return 1;
    case AJ_SYNC: ajcSvSync(); return 1;
    case AJ_PERFIL_PESQ: ajcSvPesq(); return 1;
    case AJ_PERFIL_EDITAR: ajcSvPerfilEditar(); return 1;
    case AJ_SAIR: ajcSvSair(); return 1;
    case AJ_TRAKT: ajcSvLista(0); return 1;
    case AJ_SIMKL: ajcSvLista(1); return 1;
    case AJ_SALVOS_DEST: ajcSvSalvos(); return 1;
    case AJ_CW_FONTE: ajcSvContinuar(); return 1;
    case AJ_HIST_CONTA: ajcSvHist(); return 1;
    case AJ_SOCIAL: ajcSvSocial(); return 1;
    case AJ_DISCORD: ajcSvDiscord(); return 1;
    case AJ_ENQUETES: ajcSvEnquete(); return 1;
    default: return 0;
  }
}
