// CENAS PROPRIAS DAS OPCOES (Cartazes), 2.0.3: cada opcao mostra o que ela muda,
// em vez da cena generica da categoria. Incluido por ajustes_ux_cenas.inc;
// usa o kit de ajcena.inc e o apoio do comeco daquele arquivo.
// Devolve 1 quando desenhou a cena da opcao; 0 cai na cena antiga.
// Sem palavras: so numeros, siglas e nomes proprios (TMDB, IMDb, RPDB...).

// 0..1 com pausa nos extremos: o efeito acontece, fica, e volta.
static float czPulso(float periodo) {
  float f, k;
  if (ajc_reduzido()) return 1.0f;
  f = ajc_fase(periodo);
  if (f < 0.25f) k = 0.0f;
  else if (f < 0.42f) k = (f - 0.25f) / 0.17f;
  else if (f < 0.80f) k = 1.0f;
  else if (f < 0.95f) k = 1.0f - (f - 0.80f) / 0.15f;
  else k = 0.0f;
  return k * k * (3.0f - 2.0f * k);
}
static float czLimita(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
static void czSombra(float x, float y, float w, float h, float raio, float k) {
  if (k < 0.01f) return;
  ajc_ret(x + 6, y + h * 0.10f, w - 12, h, raio, 0, 0, 0, 0.55f * k);
}
static void czBorda(float x, float y, float w, float h, float raio, float k) {
  GfxRect q = ajc_r(x, y, w, h);
  if (k < 0.01f) return;
  gfx_anel(q, raio * C.s / q.h, 2.0f, 1, 1, 1, 0.8f * k * C.a);
}
static void czBrilho(float x, float y, float w, float h, float raio, float pos, float k) {
  if (k < 0.01f) return;
  ajc_luz(x, y, w, h, raio, w * (0.1f + 0.8f * pos), -h * 0.1f, h * 0.9f, 1, 1, 1, 0.38f * k);
}
// Um cartaz com relevo (sombra + borda + reflexo) na intensidade k.
static void czRelevo(float x, float y, float w, float h, int matiz, float raio, float k, float pos) {
  float yy = y - 6.0f * k;
  czSombra(x, yy, w, h, raio, k);
  ajc_cartaz(x, yy, w, h, matiz, 1);
  czBorda(x, yy, w, h, raio, k);
  czBrilho(x, yy, w, h, raio, pos, k);
}
static void czNumero(const char *fmt, float v, float x, float y, float px) {
  char b[24];
  snprintf(b, sizeof b, fmt, v);
  ajcTexto(b, px, 1, x, y, 1.0f);
}
static void czDigita(const char *t, float px, float x, float y, float frac, float a) {
  char b[48];
  int n = (int)strlen(t), k = (int)(frac * n + 0.5f);
  if (k > n) k = n;
  if (k > 46) k = 46;
  if (k <= 0) return;
  memcpy(b, t, (size_t)k);
  b[k] = 0;
  ajcTexto(b, px, 0, x, y, a);
}
static float czFracDigitar(float periodo) {
  float f;
  if (ajc_reduzido()) return 1.0f;
  f = ajc_fase(periodo) / 0.6f;
  return f > 1.0f ? 1.0f : f;
}
static void czPontos(float x, float y, int n, int total, float a) {
  int i;
  for (i = 0; i < total; i++) {
    if (i < n) ajc_bolinha(x + i * 15, y, 8, 0.95f, 0.95f, 0.94f, a);
    else ajc_bolinha(x + i * 15, y, 8, 1, 1, 1, 0.16f * a);
  }
}
static void czCampo(float x, float y, float w, const char *icone, float a) {
  ajc_vidro(x, y, w, 44, 12, a);
  ajc_icone(icone, x + 24, y + 22, 20, 0.9f * a);
}
// Os selos que um servico de posteres poe no proprio cartaz.
static void czSelosPoster(float x, float y, float w, float h, float a) {
  if (a < 0.01f) return;
  ajc_ret(x + w - 38, y + 8, 30, 16, 5, 0.96f, 0.77f, 0.18f, a);
  ajc_icone_tinta("aj_star", x + w - 23, y + 16, 11, a);
  ajcSelo("4K", x + 8, y + h - 54, 10, 0, a);
  ajcSelo("HDR", x + 8, y + h - 30, 10, 0, a);
}
static void czPuzzle(float cx, float cy, float d, float a) {
  ajc_ladrilho("aj_puzzle", cx, cy, d, 0, a);
}
// O destaque da Home: banner no alto e uma fileira embaixo.
static void czHero(float vid) {
  int i;
  ajc_cartaz(0, 0, 640, 230, 1, 1);
  if (vid > 0.01f) ajc_video(0, 0, 640, 230, 0, vid);
  ajc_veu(0, 0, 380, 230, 2, 0.7f);
  ajc_ret(40, 92, 170, 18, 5, 1, 1, 1, 0.92f);
  ajc_barras(40, 122, 230, 2, 1);
  ajc_acento_ret(40, 162, 84, 26, 13, 1);
  for (i = 0; i < 6; i++) ajc_cartaz(24 + i * 102, 248, 90, 100, i + 3, 1);
}
// A pagina do titulo: arte atras, logo, meta e botoes. `trailerBtn` 0..1 e o
// botao de trailer ao lado de Play.
static void czDetalhe(float vid, float trailerBtn) {
  ajc_cartaz(0, 0, 640, 360, 2, 1);
  if (vid > 0.01f) ajc_video(0, 0, 640, 360, 0, vid);
  ajc_veu(0, 0, 460, 360, 2, 0.8f);
  ajc_veu(0, 180, 640, 180, 0, 0.7f);
  ajc_ret(34, 70, 210, 30, 8, 1, 1, 1, 0.92f);
  ajc_barra(34, 118, 150, 0.6f);
  ajc_barras(34, 150, 280, 3, 1);
  ajc_acento_ret(34, 270, 96, 36, 18, 1);
  ajc_icone_tinta("aj_play", 82, 288, 18, 1);
  ajc_neutro(140, 270, 36, 36, 18, 1);
  ajc_icone("aj_plus", 158, 288, 16, 0.9f);
  if (trailerBtn > 0.01f) {
    ajc_neutro(186, 270, 36, 36, 18, trailerBtn);
    ajc_icone("aj_film", 204, 288, 16, 0.95f * trailerBtn);
  } else {
    ajc_ret(186, 270, 36, 36, 18, 1, 1, 1, 0.07f);
  }
}
static void czSom(int op) {
  float k = ajcLig(op);
  int i;
  ajc_vidro(560, 24, 52, 52, 26, 1);
  ajc_icone(k > 0.5f ? "aj_audio-lines" : "aj_x", 586, 50, 24, 0.95f);
  for (i = 0; i < 3; i++) {
    float ph = fmodf(ajc_fase(1.6f) + i / 3.0f, 1.0f);
    gfx_anel_fora(ajc_r(586 - 12, 50 - 12, 24, 24), 0.5f, ajc_px(10 + ph * 34), ajc_px(2), 1, 1, 1, (1 - ph) * 0.4f * k);
  }
}
// Fileira de fontes (selos) em colunas: a atual no acento.
static void czLista(const char **nomes, int n, int atual, float x, float y, float passo, float px) {
  int i;
  for (i = 0; i < n; i++) {
    if (!nomes[i]) ajc_ladrilho("aj_wand", x + 17, y + i * passo + 12, 34, i == atual, 1);
    else ajcSelo(nomes[i], x, y + i * passo, px, i == atual ? 1.0f : 0.0f, 1);
  }
}

static int ajcNovaCartazes(int sec, int op) {
  (void)sec;
  switch (op) {
    case AJ_EXPANDIR: {   // o cartaz abre a arte deitada atras dele, por cima dos vizinhos
      float k = ajcLig(op) * czPulso(4.0f);
      int i;
      for (i = 0; i < 5; i++) {
        float a = i <= 1 ? 1.0f : 1.0f - k;
        if (i == 1 || a < 0.02f) continue;
        ajc_cartaz(40 + i * 120, 100, 100, 150, i, a);
      }
      ajc_cartaz(160, 100, 100 + 400 * k, 150 + 0 * k, 1 + (k > 0.5f ? 5 : 0), 1);
      if (k > 0.02f) {
        ajc_veu(160, 100, 100 + 400 * k, 150, 2, 0.7f * k);
        ajc_ret(280, 160, 130 * k, 12, 4, 1, 1, 1, 0.9f * k);
        ajc_barras(280, 186, 200 * k, 2, k);
        ajc_cartaz(160, 100, 100, 150, 1, 1);
      }
      ajc_foco(160, 100, 100 + 400 * k, 150, 10, 1);
      ajc_barra(40, 276, 80, 0.5f);
      ajcChaveCanto(ajcLig(op));
      return 1;
    }
    case AJ_BORDA_FOCO: {   // o anel no acento anda com o foco; sem ele, so o tamanho
      float k = ajcLig(op), fp = (float)ajc_passo(1.6f, 4), t = ajc_passo_t(1.6f);
      float ax = 40 + 130 * (fp > 0 ? fp - 1 : 0), bx = 40 + 130 * fp;
      float fx = ax + (bx - ax) * (fp > 0 ? t : 1.0f);
      int i;
      for (i = 0; i < 4; i++) {
        float w = 100, ox = 0;
        if (fabsf(40 + 130 * i - fx) < 40) { w = 112; ox = -6; }
        ajc_cartaz(40 + 130 * i + ox, 110 - (w > 100 ? 8 : 0), w, 150 + (w > 100 ? 16 : 0), i, 1);
      }
      if (k > 0.01f) ajc_foco(fx - 6, 102, 112, 166, 10, k);
      ajcChaveCanto(k);
      return 1;
    }
    case AJ_GRAD_CLASSICO: {   // o realce antigo: um degrade escuro embaixo do cartaz em foco
      float k = ajcLig(op);
      int i;
      for (i = 0; i < 4; i++) ajc_cartaz(40 + i * 140, 90, 120, 180, i, 1);
      ajc_veu(180, 90 + 180 * 0.45f, 120, 180 * 0.55f, 0, 0.85f * k);
      ajc_ret(192, 238, 70, 7, 3, 1, 1, 1, 0.8f * k);
      ajc_foco(180, 90, 120, 180, 9, 1);
      ajcChaveCanto(k);
      return 1;
    }
    case AJ_PROF: {   // relevo: sombra embaixo, borda acesa e reflexo passando
      float k = ajcLig(op);
      int i;
      for (i = 0; i < 5; i++) {
        if (i == 2) continue;
        ajc_cartaz(24 + i * 124, 104, 100, 150, i, 1);
      }
      czRelevo(148 + 124, 104, 100, 150, 2, 9, k, ajc_fase(2.6f));
      ajcChaveCanto(k);
      return 1;
    }
    case AJ_PROF_BORDA: {   // o valor: o contorno do cartaz grande acende nesse tanto
      float v = czLimita(ajcValor(op) / 100.0f);
      float x = 220, y = 40, w = 200, h = 230;
      czSombra(x, y, w, h, 12, 0.7f);
      ajc_cartaz(x, y, w, h, 2, 1);
      czBorda(x, y, w, h, 12, v);
      czBorda(x - 4, y - 4, w + 8, h + 8, 15, v * 0.6f);
      ajc_progresso(170, 300, 300, v, 1);
      czNumero("%.0f%%", v * 100.0f, 290, 316, 15);
      ajc_icone("aj_maximize-2", 100, 150, 40, 0.9f);
      return 1;
    }
    case AJ_PROF_BRILHO: {   // o reflexo: sem ele a esquerda, com o valor a direita
      float v = czLimita(ajcValor(op) / 100.0f), p = ajc_fase(3.0f);
      ajc_cartaz(80, 50, 170, 210, 2, 1);
      ajc_cartaz(390, 50, 170, 210, 2, 1);
      ajc_luz(390, 50, 170, 210, 10, 170 * (0.1f + 0.8f * p), -20, 200, 1, 1, 1, 0.75f * v);
      ajc_luz(390, 50, 170, 210, 10, 170 * 0.5f, 60, 120, 1, 1, 1, 0.45f * v);
      ajc_icone("aj_sparkle", 320, 155, 34, 0.9f);
      ajc_progresso(170, 300, 300, v, 1);
      czNumero("%.0f%%", v * 100.0f, 290, 316, 15);
      return 1;
    }
    case AJ_PROF_COBERTURA: {   // a volta do cartaz: quanto dela a borda acesa percorre
      float v = czLimita(ajcValor(op) / 100.0f);
      float x = 230, y = 50, w = 180, h = 210, per = 2 * (w + h);
      int i, n = 40;
      ajc_cartaz(x, y, w, h, 4, 1);
      for (i = 0; i < n; i++) {
        float s = (i + 0.5f) / n, d = s < 0.5f ? s : 1 - s, d2 = s * per, px, py;
        int on = d * 2 <= v;
        if (d2 < w) { px = x + d2; py = y; }
        else if (d2 < w + h) { px = x + w; py = y + (d2 - w); }
        else if (d2 < 2 * w + h) { px = x + w - (d2 - w - h); py = y + h; }
        else { px = x; py = y + h - (d2 - 2 * w - h); }
        ajc_bolinha(px, py, on ? 7 : 4, 1, 1, 1, on ? 0.95f : 0.18f);
      }
      ajc_progresso(170, 300, 300, v, 1);
      czNumero("%.0f%%", v * 100.0f, 290, 316, 15);
      return 1;
    }
    case AJ_PROF_POSTERS: case AJ_PROF_CW: case AJ_PROF_EPS: case AJ_PROF_ELENCO: case AJ_PROF_TRAILERS: {
      float k = ajcLig(op), pos = ajc_fase(2.6f);
      int i;
      if (op == AJ_PROF_POSTERS) {
        for (i = 0; i < 5; i++) {
          if (i == 2) czRelevo(24 + i * 124, 110, 100, 150, 2, 9, k, pos);
          else ajc_cartaz(24 + i * 124, 110, 100, 150, i + 3, 1);
        }
        ajc_ladrilho("aj_images", 590, 40, 44, 1, 1);
      } else if (op == AJ_PROF_CW) {
        for (i = 0; i < 3; i++) {
          float x = 28 + i * 204;
          if (i == 1) czRelevo(x, 110, 184, 104, 1, 9, k, pos);
          else ajc_cartaz(x, 110, 184, 104, i + 4, 1);
          ajc_progresso(x + 12, 196, 160, 0.3f + 0.25f * i, 1);
        }
        ajc_ladrilho("aj_rotate-ccw-clock", 590, 40, 44, 1, 1);
      } else if (op == AJ_PROF_EPS) {
        for (i = 0; i < 3; i++) {
          float y = 34 + i * 100;
          if (i == 1) czRelevo(100, y, 156, 88, 3, 9, k, pos);
          else ajc_cartaz(100, y, 156, 88, 5 + i, 1);
          ajc_barra(280, y + 14, 200, 0.7f);
          ajc_barras(280, y + 38, 240, 3, 0.8f);
        }
        ajc_ladrilho("aj_list-video", 590, 40, 44, 1, 1);
      } else if (op == AJ_PROF_ELENCO) {
        for (i = 0; i < 5; i++) {
          float x = 40 + i * 118, r, g, b;
          ajc_matiz(i * 3 + 1, &r, &g, &b);
          if (i == 2) { czSombra(x, 110, 96, 96, 48, k); }
          ajc_bolinha(x + 48, 158 - (i == 2 ? 6 * k : 0), 96, r, g, b, 1);
          ajc_icone("aj_user-round", x + 48, 158 - (i == 2 ? 6 * k : 0), 40, 0.85f);
          if (i == 2) { czBorda(x, 104, 96, 96, 48, k); czBrilho(x, 104, 96, 96, 48, pos, k); }
          ajc_barra(x + 10, 226, 76, 0.5f);
        }
        ajc_ladrilho("aj_users", 590, 40, 44, 1, 1);
      } else {
        for (i = 0; i < 3; i++) {
          float x = 28 + i * 204;
          if (i == 1) czRelevo(x, 110, 184, 104, 1, 9, k, pos);
          else ajc_cartaz(x, 110, 184, 104, i + 4, 1);
          ajc_play(x + 92, 162 - (i == 1 ? 6 * k : 0), 34, 1);
        }
        ajc_ladrilho("aj_film", 590, 40, 44, 1, 1);
      }
      ajcChaveCanto(k);
      return 1;
    }
    case AJ_HERO_FUNDO: {   // de onde vem a arte grande: a fonte atual no acento
      static const char *F[8] = { NULL, "Cinemeta", "IMDb", "TMDB", "Trakt", "Apple TV", "fanart.tv", "Kitsu" };
      int v = (int)(ajcValor(op) + 0.5f), i;
      float x = 24, y = 232;
      if (v < 0) v = 0;
      if (v > 7) v = 7;
      ajc_cartaz(0, 0, 640, 214, v + 1, 1);
      ajc_veu(0, 0, 360, 214, 2, 0.7f);
      ajc_ret(40, 84, 150, 16, 5, 1, 1, 1, 0.9f);
      ajc_barras(40, 112, 210, 2, 1);
      for (i = 0; i < 8; i++) {
        if (!F[i]) { ajc_ladrilho("aj_wand", x + 17, y + 14, 34, i == v, 1); x += 44; continue; }
        x += ajcSelo(F[i], x, y + (x > 460 ? 40 : 0) * 0, 13, i == v ? 1.0f : 0.0f, 1) + 8;
        if (x > 520 && i < 7 && y < 270) { x = 24; y += 40; }
      }
      return 1;
    }
    case AJ_HERO_ARTE_DIF: {   // o cartaz fica; o destaque ganha outra foto (ou repete a mesma)
      float k = ajcLig(op);
      ajc_cartaz(50, 60, 150, 230, 0, 1);
      ajc_cartaz(270, 60, 330, 186, 0, 1);
      ajc_cartaz(270, 60, 330, 186, 4, k);
      ajc_veu(270, 60, 200, 186, 2, 0.6f);
      ajc_ret(290, 150, 110, 12, 4, 1, 1, 1, 0.9f);
      ajc_barras(290, 172, 150, 2, 1);
      ajc_ladrilho(k > 0.5f ? "aj_unlink" : "aj_link", 235, 175, 44, k > 0.5f, 1);
      ajcChaveCanto(k);
      return 1;
    }
    case AJ_POSTER_PROV: {   // o cartaz ganha notas, selos e faixa do servico escolhido
      static const char *S[4] = { "", "SpatialPosters", "RPDB", "URL" };
      int v = (int)(ajcValor(op) + 0.5f), i;
      float on = v > 0 ? 1.0f : 0.0f;
      ajc_cartaz(70, 36, 180, 270, 1, 1);
      czSelosPoster(70, 36, 180, 270, on);
      if (on > 0.5f) {
        ajc_acento_ret(70, 250, 180, 30, 0, 0.95f);
        czNumero("%.0f", 10, 154, 253, 16);
      }
      ajc_ladrilho("aj_x", 335, 70, 36, v == 0, 1);
      for (i = 1; i < 4; i++) ajcSelo(S[i], 310, 70 + i * 52 - 12, 14, i == v ? 1.0f : 0.0f, 1);
      return 1;
    }
    case AJ_FANART_CHAVE: {   // a chave digitada abre a fonte fanart.tv para o fundo
      float p = czPulso(5.0f), f = ajc_reduzido() ? 1.0f : czLimita(ajc_fase(5.0f) / 0.4f);
      ajcSelo("fanart.tv", 60, 54, 15, 1, 1);
      czCampo(60, 96, 360, "aj_key-round", 1);
      czPontos(100, 118, (int)(f * 14 + 0.5f), 14, 1);
      ajc_cartaz(450, 60, 150, 90, 6, 0.25f + 0.75f * p);
      ajc_ladrilho("aj_check", 525, 105, 36, 1, p);
      ajc_cartaz(60, 190, 540, 110, 1, 0.3f);
      ajc_veu(60, 190, 540, 110, 0, 0.6f);
      return 1;
    }
    case AJ_POSTER_INST: {   // o campo do endereco sendo preenchido; o servidor responde
      float f = czFracDigitar(4.0f), p = czPulso(4.0f);
      czCampo(40, 70, 400, "aj_globe", 1);
      czDigita("https://p.app", 15, 80, 83, f, 1);
      ajc_ladrilho("aj_server", 520, 92, 56, 1, 1);
      ajc_cartaz(290, 170, 90, 135, 3, p);
      czSelosPoster(290, 170, 90, 135, p);
      ajc_barra(60, 200, 150, 0.4f);
      return 1;
    }
    case AJ_POSTER_TOKEN: {   // a senha do servico: pontinhos e escudo
      int n = ajc_reduzido() ? 12 : ajc_passo(0.25f, 13);
      ajc_vidro(60, 90, 380, 52, 12, 1);
      ajc_icone("aj_key-round", 90, 116, 22, 0.9f);
      czPontos(126, 116, n, 12, 1);
      ajc_ladrilho("aj_shield-check", 520, 116, 56, 1, 1);
      ajc_barras(60, 200, 320, 3, 0.8f);
      return 1;
    }
    case AJ_POSTER_EXTRA: {   // parametros extras: pecas que se juntam ao fim da URL
      static const char *P[3] = { "?a=1", "&b=2", "&c=3" };
      int i, n = ajc_reduzido() ? 3 : 1 + ajc_passo(0.9f, 3);
      ajc_vidro(40, 70, 560, 44, 12, 1);
      ajc_barra(66, 89, 250, 0.5f);
      for (i = 0; i < 3; i++) {
        float x = 324 + i * 86;
        if (i < n) ajcSelo(P[i], x, 78, 13, 1, 1);
        else ajc_ret(x, 78, 74, 25, 12, 1, 1, 1, 0.08f);
      }
      ajc_ladrilho("aj_plus", 320, 190, 56, 1, 1);
      ajc_cartaz(380, 150, 90, 135, 3, 1);
      czSelosPoster(380, 150, 90, 135, 1);
      return 1;
    }
    case AJ_POSTER_CHAVE: {   // chave do RPDB
      int n = ajc_reduzido() ? 12 : ajc_passo(0.25f, 13);
      ajcSelo("RPDB", 60, 54, 15, 1, 1);
      ajc_vidro(60, 100, 380, 52, 12, 1);
      ajc_icone("aj_key-round", 90, 126, 22, 0.9f);
      czPontos(126, 126, n, 12, 1);
      ajc_cartaz(480, 70, 100, 150, 5, 1);
      czSelosPoster(480, 70, 100, 150, czLimita((float)n / 12.0f));
      return 1;
    }
    case AJ_POSTER_MODELO: {   // o modelo: o espaco {id} e trocado pelo codigo de cada titulo
      float p = czPulso(4.0f);
      ajc_vidro(40, 70, 560, 50, 12, 1);
      ajcTexto("https://img.app/", 15, 0, 62, 85, 0.9f);
      ajc_acento_ret(236, 80, 56, 30, 8, 1);
      ajcTextoTinta("{id}", 15, 1, 246, 85, 1);
      ajcTexto(".jpg", 15, 0, 300, 85, 0.9f);
      ajc_icone("aj_arrow-down", 264, 150, 24, 0.8f);
      ajcSelo("tt0111161", 200, 176, 14, p > 0.5f ? 1.0f : 0.0f, 1);
      ajc_cartaz(480, 150, 90, 135, 2, 0.4f + 0.6f * p);
      return 1;
    }
    case AJ_POSTER_TESTAR: {   // o teste: baixa um cartaz de amostra e termina com o visto
      float f = ajc_reduzido() ? 1.0f : czLimita(ajc_fase(4.0f) / 0.6f);
      ajc_cartaz(240, 40, 150, 225, 4, 1);
      czSelosPoster(240, 40, 150, 225, f >= 1.0f ? 1.0f : 0.0f);
      if (f < 1.0f) {
        ajc_ret(240, 40, 150, 225, 10, 0.075f, 0.082f, 0.102f, 0.7f);
        ajc_anel(315, 152, 34, f, 1);
      } else {
        ajc_bolinha(390, 56, 40, C.ar, C.ag, C.ab, 1);
        ajc_icone_tinta("aj_check", 390, 56, 24, 1);
      }
      ajc_ladrilho("aj_zap", 130, 152, 56, 1, 1);
      return 1;
    }
    case AJ_ADDON_POSTER: {   // o cartaz do addon fica; o servico entra so nos outros
      float k = ajcLig(op);
      int i;
      for (i = 0; i < 4; i++) {
        float x = 50 + i * 140;
        ajc_cartaz(x, 90, 110, 165, i == 0 ? 3 : i + 1, 1);
        czSelosPoster(x, 90, 110, 165, i == 0 ? 1.0f - k : 1.0f);
      }
      czPuzzle(144, 104, 30, 1);
      ajc_foco(50, 90, 110, 165, 9, k);
      ajcChaveCanto(k);
      return 1;
    }
    case AJ_ADDON_FUNDO: {   // o fundo do destaque e o que o addon manda, nao o da fonte
      float k = ajcLig(op);
      ajc_cartaz(60, 40, 520, 230, 4, 1);
      ajc_cartaz(60, 40, 520, 230, 3, k);
      ajc_veu(60, 40, 300, 230, 2, 0.7f);
      ajc_ret(90, 140, 150, 16, 5, 1, 1, 1, 0.9f);
      ajc_barras(90, 168, 200, 2, 1);
      czPuzzle(550, 70, 36, k);
      ajcSelo("TMDB", 500, 232, 13, 0, 1 - k);
      ajcChaveCanto(k);
      return 1;
    }
    case AJ_ADDON_LOGO: {   // o logo do addon nao e trocado pelo logo traduzido do TMDB
      float k = ajcLig(op);
      ajc_cartaz(60, 40, 520, 230, 3, 1);
      ajc_veu(60, 40, 300, 230, 2, 0.7f);
      ajc_ret(90, 100, 210, 34, 9, 1, 1, 1, 0.92f * k);
      ajc_ret(90, 100, 150, 34, 9, 0.9f, 0.9f, 0.9f, 0.92f * (1 - k));
      ajc_barras(90, 160, 200, 2, 1);
      czPuzzle(330, 117, 32, k);
      ajc_ladrilho("aj_languages", 330, 117, 32, 0, 1 - k);
      ajcSelo("TMDB", 90, 76 - 4, 12, 0, 1 - k);
      ajcChaveCanto(k);
      return 1;
    }
    case AJ_COL_ARTE_CONTA: {   // as pastas trocam a arte do app pela da sua conta
      float k = ajcLig(op);
      int i;
      for (i = 0; i < 4; i++) {
        float x = 40 + i * 144;
        ajc_cartaz(x, 100, 124, 90, i, 1);
        ajc_cartaz(x, 100, 124, 90, i + 4, k);
        ajc_icone("aj_folders", x + 62, 145, 30, 0.9f);
        ajc_barra(x + 22, 206, 80, 0.5f);
      }
      ajc_ladrilho("aj_cloud", 560, 56, 48, k > 0.5f, 1);
      ajcChaveCanto(k);
      return 1;
    }
    // --- trailers: cada um no lugar onde acontece ---
    case AJ_HERO_TRAILER: {   // o destaque da Home: a arte vira trailer
      float k = ajcLig(op), vid = k * czPulso(5.0f);
      czHero(vid);
      ajc_play(560, 114, 46, 1 - vid);
      ajcChaveCanto(k);
      return 1;
    }
    case AJ_HERO_TRAILER_SOM: {
      czHero(1);
      czSom(op);
      ajcChaveCanto(ajcLig(op));
      return 1;
    }
    case AJ_DET_TRAILER_SOM: {
      czDetalhe(1, 0);
      czSom(op);
      ajcChaveCanto(ajcLig(op));
      return 1;
    }
    case AJ_HERO_TRAILER_ESPERA: {   // o relogio: o destaque fica parado ate o anel encher
      float f = ajc_reduzido() ? 1.0f : ajc_fase(5.0f), a = czLimita(f / 0.6f), vid = f > 0.62f ? 1.0f : 0.0f;
      czHero(vid);
      if (vid < 0.5f) {
        ajc_ret(250, 66, 140, 100, 20, 0, 0, 0, 0.5f);
        ajc_anel(320, 116, 34, a, 1);
        ajc_icone("aj_clock", 320, 116, 26, 0.9f);
      }
      czNumero("%.0f s", (float)valor[op], 540, 192, 15);
      return 1;
    }
    case AJ_DET_TRAILER_AUTO: {   // a pagina assenta e o trailer comeca sozinho, mudo
      float k = ajcLig(op), p = czPulso(6.0f), f = ajc_reduzido() ? 1.0f : czLimita(ajc_fase(6.0f) / 0.25f);
      czDetalhe(k * p, 1);
      ajc_progresso(34, 330, 140, k * f, 1);
      ajc_icone("aj_volume-2", 600, 40, 20, 0.0f);
      ajcChaveCanto(k);
      return 1;
    }
    case AJ_DET_TRAILER: {   // o botao de trailer ao lado de Play
      float k = ajcLig(op), p = czPulso(5.0f);
      czDetalhe(k * p, k);
      if (k > 0.5f) ajc_foco(186, 270, 36, 36, 18, 0.4f + 0.6f * p);
      ajcChaveCanto(k);
      return 1;
    }
    case AJ_TRAILER_FONTE: {   // de onde o trailer vem: a fonte atual no acento
      static const char *F[4] = { NULL, "Apple TV", "IMDb", "YouTube" };
      int v = (int)(ajcValor(op) + 0.5f);
      float f = ajc_fase(2.0f);
      if (v < 0) v = 0;
      if (v > 3) v = 3;
      ajc_video(30, 60, 360, 202, 12, 1);
      ajc_play(210, 161, 54, 1);
      czLista(F, 4, v, 450, 70, 46, 15);
      ajc_bolinha(450 - 60 * f, 82 + v * 46 + 12 + (161 - 82 - v * 46 - 12) * f, 10, C.ar, C.ag, C.ab, 1 - f * 0.5f);
      return 1;
    }
    case AJ_TRAILER_QUAL: {   // a definicao: barras de qualidade, a escolhida no acento
      static const char *Q[4] = { NULL, "1080p", "720p", "480p" };
      int v = (int)(ajcValor(op) + 0.5f), i;
      if (v < 0) v = 0;
      if (v > 3) v = 3;
      ajc_video(30, 60, 360, 202, 12, 1);
      ajc_play(210, 161, 54, 1);
      for (i = 0; i < 4; i++) {
        float h = 24 + i * 22;
        if (i >= v) ajc_acento_ret(430 + i * 22, 130 - h + 40, 14, h, 4, 1);
        else ajc_ret(430 + i * 22, 130 - h + 40, 14, h, 4, 1, 1, 1, 0.16f);
      }
      for (i = 0; i < 4; i++) {
        if (!Q[i]) ajc_ladrilho("aj_sparkles", 447, 205 - 0, 34, i == v, 1);
      }
      for (i = 1; i < 4; i++) ajcSelo(Q[i], 480 + (i - 1) * 0 + 0, 140 + i * 36, 14, i == v ? 1.0f : 0.0f, 1);
      return 1;
    }
    default: return 0;
  }
}
