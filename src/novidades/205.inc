// Conteudo da 2.0.5: docs/releases/2.0.5/NOVIDADES.md.
// Uma cena (as colecoes) e as artes dela. Tetos de linhas para as traducoes; o motor mede antes de paginar.
//
// A CENA mostra UMA colecao trocando de exibicao (Grade, Linha do tempo,
// Ranking, Saga, Decadas, Mural, Vitrine): os mesmos dez cartazes andam de uma
// tabela de posicoes para a outra, como a tela de verdade (vertudo.c monta uma
// tabela de celulas por exibicao). So o que ja existe na tela: retangulos de
// arte do pacote, tracos e texto; nenhuma textura nova por quadro, nenhum blur.
#define CO_N       10
#define CO_EXIB     7
#define CO_PASSO    1.7f                 // s por exibicao
#define CO_TROCA    0.6f                 // s da passagem para a proxima (fim do passo)
#define CO_DUR      (CO_PASSO * (float)CO_EXIB)
#define CO_CW     640.0f                 // o corpo da colecao dentro da janela
#define CO_CH     402.0f

static const char *const CO_NOME[CO_EXIB] = { "Grade", "Linha do tempo", "Ranking", "Saga", "Décadas", "Mural", "Vitrine" };
// Cinco artes do pacote, de modo que vizinhos (de lado e de cima) nunca repetem na Grade.
static const int CO_ART[CO_N] = { 14, 6, 12, 29, 22, 29, 12, 22, 14, 6 };

typedef struct { float x, y, w, h; } CoR;

static float coSuave(float u) { u = anim_clamp(u, 0.0f, 1.0f); return u * u * (3.0f - 2.0f * u); }

// A exibicao em que a cena esta, com fracao: 0..6. Cada passo segura e, no fim
// dele, passa para a seguinte em CO_TROCA s; a ultima segura ate o fim.
static float coPos(float t) {
  int k = (int)(t / CO_PASSO);
  if (k >= CO_EXIB - 1) return (float)(CO_EXIB - 1);
  return (float)k + coSuave((t - (float)k * CO_PASSO - (CO_PASSO - CO_TROCA)) / CO_TROCA);
}

// O retangulo do cartaz `i` na exibicao `L` (relativo ao corpo); 0 se ele nao aparece nela.
static int coRect(int L, int i, CoR *r) {
  switch (L) {
    case 0:   // Grade: duas fileiras de cinco
      *r = (CoR){ (float)(i % 5) * 131.0f, 19.0f + (float)(i / 5) * 189.0f, 116.0f, 174.0f }; return 1;
    case 1:   // Linha do tempo: cartoes deitados, um por ano, no eixo
      if (i > 2) return 0;
      *r = (CoR){ 100.0f, 15.0f + (float)i * 130.0f, 200.0f, 112.0f }; return 1;
    case 2:   // Ranking: o numeral mora no vao a esquerda do cartaz
      if (i > 5) return 0;
      *r = (CoR){ (float)(i % 3) * 220.0f + 76.0f, 5.0f + (float)(i / 3) * 206.0f, 124.0f, 186.0f }; return 1;
    case 3:   // Saga: tres capitulos deitados por linha, a trilha por cima
      if (i > 5) return 0;
      *r = (CoR){ (float)(i % 3) * 225.0f, 52.0f + (float)(i / 3) * 190.0f, 190.0f, 107.0f }; return 1;
    case 4:   // Decadas: uma secao por decada, cartazes pequenos
      if (i < 6) *r = (CoR){ (float)i * 108.8f, 38.0f, 96.0f, 144.0f };
      else *r = (CoR){ (float)(i - 6) * 108.8f, 240.0f, 96.0f, 144.0f };
      return 1;
    case 5: { // Mural: blocos de cinco, um grande e quatro pequenos
      int b = i / 5, k = i % 5, q = k < 2 ? k : k - 1;
      float y = (float)b * 196.0f;
      if (!b) {
        if (!k) *r = (CoR){ 0.0f, 0.0f, 316.0f, 178.0f };
        else *r = (CoR){ 324.0f + (float)((k - 1) & 1) * 162.0f, (float)((k - 1) >> 1) * 93.0f, 154.0f, 85.0f };
      } else if (k == 2) *r = (CoR){ 324.0f, y, 316.0f, 178.0f };
      else *r = (CoR){ (float)(q & 1) * 162.0f, y + (float)(q >> 1) * 93.0f, 154.0f, 85.0f };
      return 1; }
    default:  // Vitrine: o destaque grande e uma fileira
      if (i == 0) { *r = (CoR){ 0.0f, 0.0f, 640.0f, 214.0f }; return 1; }
      if (i > 5) return 0;
      *r = (CoR){ (float)(i - 1) * 133.0f, 250.0f, 108.0f, 146.0f }; return 1;
  }
}

// Quem nao aparece na exibicao se recolhe atras de um que aparece e some.
static void coTile(int L, int i, CoR *r, float *a) {
  static const int NV[CO_EXIB] = { 10, 3, 6, 6, 10, 10, 6 };
  if (coRect(L, i, r)) { *a = 1.0f; return; }
  coRect(L, i % NV[L], r);
  r->x += r->w * 0.07f; r->y += r->h * 0.07f; r->w *= 0.86f; r->h *= 0.86f;
  *a = 0.0f;
}

static void coTraco(float x, float y, float w, float h, float al, float a) {
  gfx_cor((GfxRect){ x, y, w, h }, 0.5f, 1, 1, 1, al * a);
}

static void coNo(float cx, float cy, float d, int cheio, float ar, float ag, float ab, float a) {
  GfxRect no = { cx - d * 0.5f, cy - d * 0.5f, d, d };
  if (cheio) gfx_cor(no, 0.5f, ar, ag, ab, a);
  else { gfx_cor(no, 0.5f, 0.12f, 0.13f, 0.15f, a);
         gfx_rect(no, 0, GFX_ANEL, 0, 2.0f / d, 0, 0.5f, 1, 1, 1, 0.45f * a); }
}

// O que cada exibicao tem ALEM dos cartazes. `ov` = o quanto ela esta na tela (0..1).
// Os numerais do Ranking vao ANTES dos cartazes: o cartaz cobre a ponta deles.
static void coAntes(float bx, float by, float ov, float ar, float ag, float ab, float a) {
  static const char *const NUM[6] = { "1", "2", "3", "4", "5", "6" };
  int i;
  if (ov < 0.01f) return;
  for (i = 0; i < 6; i++) {
    CoR r;
    TxtLinha nu = txt_linha(TXT_RANK_GRANDE, NUM[i], 255, 255, 255, 255);
    coRect(2, i, &r);
    if (nu.tex && nu.w > 0) {
      float vao = 76.0f + 0.22f * r.w, e = vao / (float)nu.w, nw, nh;
      if (e > r.h * 0.86f / (float)nu.h) e = r.h * 0.86f / (float)nu.h;
      nw = (float)nu.w * e; nh = (float)nu.h * e;
      gfx_rect((GfxRect){ bx + r.x + 0.22f * r.w - nw, by + r.y + r.h - nh * NV_TOP10_NUM_BASE, nw, nh },
               nu.tex, GFX_TEXTO, 0, 0, 0, 0.0f, i < 3 ? ar : 0.91f, i < 3 ? ag : 0.92f, i < 3 ? ab : 0.94f,
               ov * a * (i < 3 ? 0.95f : 0.62f));
    }
  }
}

static void coDepois(float bx, float by, const float *ov, float ar, float ag, float ab, float a) {
  int i;
  if (ov[1] > 0.01f) {   // LINHA DO TEMPO: o eixo, um no e o ano por cartao, e o texto ao lado
    static const char *const ANO[3] = { "1982", "1991", "2004" };
    float o = ov[1] * a;
    gfx_cor((GfxRect){ bx + 75.0f, by + 30.0f, 2.0f, 2.0f * 130.0f + 82.0f }, 0, 1, 1, 1, 0.18f * o);
    for (i = 0; i < 3; i++) {
      CoR r;
      TxtLinha y = txt_linha(TXT_HERO_META, ANO[i], 176, 182, 194, 255);
      float cy;
      coRect(1, i, &r);
      cy = by + r.y + r.h * 0.5f;
      txt_desenhar_alpha(y, bx, cy - (float)y.h * 0.5f, o);
      coNo(bx + 76.0f, cy, 14.0f, i == 0, ar, ag, ab, o);
      coTraco(bx + 316.0f, cy - 30.0f, 200.0f, 14.0f, 0.85f, o);
      coTraco(bx + 316.0f, cy - 4.0f, 150.0f, 8.0f, 0.38f, o);
      coTraco(bx + 316.0f, cy + 14.0f, 110.0f, 8.0f, 0.24f, o);
    }
  }
  if (ov[3] > 0.01f) {   // SAGA: a trilha de capitulos (cheio = visto) e o numero no cartao
    float o = ov[3] * a;
    for (i = 0; i < 6; i++) {
      CoR r;
      char nn[8];
      TxtLinha l;
      float cx, ty;
      int visto = i < 4;
      coRect(3, i, &r);
      cx = bx + r.x + r.w * 0.5f; ty = by + r.y - 22.0f;
      if (i % 3 < 2) {
        float x2 = cx + 225.0f;
        if (visto && i + 1 < 4) gfx_cor((GfxRect){ cx, ty - 2.0f, x2 - cx, 4.0f }, 0.5f, ar, ag, ab, 0.9f * o);
        else gfx_cor((GfxRect){ cx, ty - 1.0f, x2 - cx, 2.0f }, 0.5f, 1, 1, 1, 0.20f * o);
      }
      coNo(cx, ty, 16.0f, visto, ar, ag, ab, o);
      gfx_veu_base((GfxRect){ bx + r.x, by + r.y, r.w, r.h }, 10.0f / r.h, 0.5f, 0.75f * o);
      snprintf(nn, sizeof nn, "%02d", i + 1);
      l = txt_linha(TXT_ILHA_NOME, nn, 246, 247, 250, 255);
      txt_desenhar_alpha(l, bx + r.x + 14.0f, by + r.y + r.h - (float)l.h - 8.0f, 0.96f * o);
      coTraco(bx + r.x, by + r.y + r.h + 14.0f, 120.0f, 10.0f, 0.7f, o);
      coTraco(bx + r.x, by + r.y + r.h + 32.0f, 76.0f, 7.0f, 0.28f, o);
    }
  }
  if (ov[4] > 0.01f) {   // DECADAS: o cabecalho de cada secao, com o fio ate o fim
    float o = ov[4] * a;
    for (i = 0; i < 2; i++) {
      char ano[24];
      TxtLinha t;
      float y = by + (i ? 198.0f : 2.0f);
      snprintf(ano, sizeof ano, i18n("Anos %d"), i ? 2000 : 1990);
      t = txt_linha(TXT_ILHA_NOME, ano, 244, 245, 248, 255);
      gfx_cor((GfxRect){ bx, y + 4.0f, 5.0f, (float)t.h - 8.0f }, 0.5f, ar, ag, ab, o);
      txt_desenhar_alpha(t, bx + 18.0f, y, o);
      gfx_cor((GfxRect){ bx + 18.0f + (float)t.w + 18.0f, y + (float)t.h * 0.5f, CO_CW - ((float)t.w + 36.0f), 1.0f },
              0, 1, 1, 1, 0.24f * o);
    }
  }
  if (ov[6] > 0.01f) {   // VITRINE: titulo e botao sobre o destaque, e o nome da fileira
    float o = ov[6] * a;
    CoR h;
    GfxRect b;
    int tf = ajustes_tinta_foco();
    coRect(6, 0, &h);
    gfx_veu_base((GfxRect){ bx, by, h.w, h.h }, 18.0f / h.h, 0.85f, 0.85f * o);
    coTraco(bx + 28.0f, by + h.h - 104.0f, 250.0f, 20.0f, 0.92f, o);
    coTraco(bx + 28.0f, by + h.h - 74.0f, 170.0f, 9.0f, 0.5f, o);
    b = (GfxRect){ bx + 28.0f, by + h.h - 52.0f, 104.0f, 34.0f };
    gfx_cor(b, 0.5f, ar, ag, ab, 0.96f * o);
    gfx_icone((GfxRect){ b.x + 40.0f, b.y + 8.0f, 18.0f, 18.0f }, "aj_play", (float)tf / 255.0f, (float)tf / 255.0f,
              (float)tf / 255.0f, o);
    coTraco(bx, by + 224.0f, 150.0f, 12.0f, 0.8f, o);
  }
}

static void cenaColecao(GfxRect p, float t, float a) {
  int red = ajustes_animacoes_reduzidas(), i, k, k0, k1;
  float ar, ag, ab, pos, fr, ov[CO_EXIB], press = 0.0f;
  GfxRect win = { p.x + 24.0f, p.y + 24.0f, p.w - 48.0f, 574.0f };
  float bx = win.x + (win.w - CO_CW) * 0.5f, by = win.y + 146.0f, cy = win.y + 92.0f;
  if (red) t = CO_DUR;
  ajustes_acento(&ar, &ag, &ab);
  pos = coPos(t);
  k0 = (int)pos; if (k0 > CO_EXIB - 1) k0 = CO_EXIB - 1;
  k1 = k0 + 1 < CO_EXIB ? k0 + 1 : k0;
  fr = pos - (float)k0;
  for (i = 0; i < CO_EXIB; i++) ov[i] = anim_clamp(1.0f - fabsf(pos - (float)i) * 2.0f, 0.0f, 1.0f);
  // O OK: a pilula afunda um instante quando a passagem comeca.
  k = (int)(t / CO_PASSO);
  if (!red && k < CO_EXIB - 1) {
    float tr = (t - (float)k * CO_PASSO - (CO_PASSO - CO_TROCA)) / 0.28f;
    if (tr > 0.0f && tr < 1.0f) press = sinf(tr * 3.14159265f);
  }

  novArte(12, p, PV_RAIO, a);
  gfx_cor(p, PV_RAIO / p.h, 0.02f, 0.02f, 0.025f, 0.74f * a);
  gfx_rect((GfxRect){ win.x - 24.0f, win.y - 8.0f, win.w + 48.0f, win.h + 44.0f }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.12f,
           0, 0, 0, 0.55f * a);
  gfx_cor(win, 22.0f / win.h, 0.036f, 0.038f, 0.044f, 0.93f * a);
  gfx_anel(win, 22.0f / win.h, 1.0f, 1, 1, 1, 0.10f * a);

  // O cabecalho: titulo e legenda (tracos), as abas e o botao "Exibição".
  coTraco(bx, win.y + 24.0f, 210.0f, 18.0f, 0.88f, a);
  coTraco(bx, win.y + 52.0f, 300.0f, 9.0f, 0.30f, a);
  gfx_cor((GfxRect){ bx, cy - 20.0f, 118.0f, 40.0f }, 0.5f, 1, 1, 1, 0.13f * a);
  coTraco(bx + 22.0f, cy - 4.0f, 74.0f, 8.0f, 0.8f, a);
  gfx_cor((GfxRect){ bx + 130.0f, cy - 20.0f, 118.0f, 40.0f }, 0.5f, 1, 1, 1, 0.06f * a);
  coTraco(bx + 152.0f, cy - 4.0f, 74.0f, 8.0f, 0.38f, a);
  { int t1 = ajustes_tinta_foco(), t2 = ajustes_tinta_foco2();
    TxtLinha lb = txt_linha(TXT_HERO_META, i18n("Exibição"), t2, t2, t2, 255);
    TxtLinha sl = txt_linha(TXT_HERO_META, "/", t2, t2, t2, 255);
    TxtLinha n0 = txt_linha_corta(TXT_ILHA_NOME, i18n(CO_NOME[k0]), t1, t1, t1, 255, 260.0f);
    TxtLinha n1 = txt_linha_corta(TXT_ILHA_NOME, i18n(CO_NOME[k1]), t1, t1, t1, 255, 260.0f);
    float wn = (float)n0.w + ((float)n1.w - (float)n0.w) * fr;
    float W = 22.0f + 18.0f + 12.0f + (float)lb.w + 12.0f + (float)sl.w + 12.0f + wn + 24.0f;
    GfxRect pill = { bx + CO_CW - W + press * 2.5f, cy - 26.0f + press * 2.5f, W - press * 5.0f, 52.0f - press * 5.0f };
    float x, c = (float)t1 / 255.0f, f0 = anim_clamp(1.0f - fr * 3.0f, 0.0f, 1.0f), f1 = anim_clamp(fr * 3.0f - 1.8f, 0.0f, 1.0f);
    gfx_cor(pill, NV_RAIO_PILL, ar, ag, ab, a);
    x = pill.x + 22.0f;
    for (i = 0; i < 4; i++)
      gfx_cor((GfxRect){ x + (float)(i & 1) * 10.0f, pill.y + (pill.h - 18.0f) * 0.5f + (float)(i >> 1) * 10.0f, 8.0f, 8.0f },
              0.25f, c, c, c, a);
    x += 30.0f;
    txt_desenhar_alpha(lb, x, pill.y + (pill.h - (float)lb.h) * 0.5f, 0.82f * a);
    x += (float)lb.w + 12.0f;
    txt_desenhar_alpha(sl, x, pill.y + (pill.h - (float)sl.h) * 0.5f, 0.55f * a);
    x += (float)sl.w + 12.0f;
    txt_desenhar_alpha(n0, x, pill.y + (pill.h - (float)n0.h) * 0.5f, f0 * a);
    txt_desenhar_alpha(n1, x, pill.y + (pill.h - (float)n1.h) * 0.5f, f1 * a);
    // A dica do botao de verdade, e uma bolinha por exibicao.
    { TxtLinha d = txt_linha(TXT_MINI, i18n("OK troca a exibição"), 186, 192, 204, 255);
      txt_desenhar_alpha(d, bx + CO_CW - (float)d.w, cy + 34.0f, 0.85f * a); }
    for (i = 0; i < CO_EXIB; i++) {
      int agora = (int)(pos + 0.5f) == i;
      gfx_cor((GfxRect){ bx + (float)i * 16.0f, cy + 40.0f, agora ? 10.0f : 6.0f, 6.0f }, 0.5f,
              agora ? ar : 1.0f, agora ? ag : 1.0f, agora ? ab : 1.0f, (agora ? 1.0f : 0.24f) * a);
    }
  }

  coAntes(bx, by, ov[2], ar, ag, ab, a);
  for (i = 0; i < CO_N; i++) {
    CoR r0, r1;
    float a0, a1, al, m;
    GfxRect r;
    coTile(k0, i, &r0, &a0);
    coTile(k1, i, &r1, &a1);
    al = a0 + (a1 - a0) * fr;
    if (al < 0.01f) continue;
    r = (GfxRect){ bx + r0.x + (r1.x - r0.x) * fr, by + r0.y + (r1.y - r0.y) * fr,
                   r0.w + (r1.w - r0.w) * fr, r0.h + (r1.h - r0.h) * fr };
    m = r.w < r.h ? r.w : r.h;
    m = 0.07f * m + 4.0f;
    if (m > 18.0f) m = 18.0f;
    novArte(CO_ART[i], r, m, al * a);
    gfx_anel(r, m / r.h, 1.0f, 1, 1, 1, 0.10f * al * a);
  }
  coDepois(bx, by, ov, ar, ag, ab, a);
}

static const NovCena CENAS_205[] = {
  { "Coleções", "A mesma coleção, de sete jeitos.", cenaColecao, CO_DUR, 0 },
};
static const int ARTES_205[] = { 14, 6, 12, 29, 22 };

static const NovItem ITENS_205[] = {
  { .grupo = "Coleções", .icone = "aj_layout-dashboard", .nome = "Novas exibições",
    .frase = "Grade, Linha do tempo, Ranking, Saga, Décadas, Mural e Vitrine: toda coleção e pasta pode ser vista de sete jeitos.", .linhas = 4,
    .frase2 = "A Saga mostra o que você já viu. A Vitrine traz um destaque grande e uma fileira por aba.", .linhas2 = 3 },
  { .icone = "aj_sliders-horizontal", .nome = "Botão Exibição",
    .frase = "Fica ao lado das abas da coleção. Aperte OK nele para passar para a próxima exibição.", .linhas = 3,
    .frase2 = "Sem escolher, nada muda. A escolha fica gravada em cada coleção, para cada perfil.", .linhas2 = 3 },
  { .grupo = "Player", .icone = "aj_clapperboard", .nome = "Abertura Cinema",
    .frase = "A abertura estilo cinema agora é o padrão. O cartão detalhado continua nos Ajustes.", .linhas = 2 },
  { .icone = "aj_circle-check", .nome = "Marcado como visto",
    .frase = "Só depois de reprodução real. Um trailer não marca mais o título como visto.", .linhas = 2 },
  { .icone = "aj_circle-play", .nome = "Pular abertura",
    .frase = "Novas opções para pular abertura, recapitulação e créditos sozinho.", .linhas = 2 },
  { .icone = "aj_info", .nome = "Detalhes da fonte",
    .frase = "O botão de informação da fonte mostra o add-on, a descrição completa e o nome do arquivo.", .linhas = 3 },
  { .icone = "aj_circle-play", .nome = "Fonte recusada",
    .frase = "Se a TV recusar uma fonte, o app passa para a próxima em vez de parar.", .linhas = 2 },
  { .grupo = "Legendas", .icone = "aj_captions", .nome = "Lista completa",
    .frase = "Legendas agrupadas por idioma, com a linha de sincronização automática.", .linhas = 2 },
  { .icone = "aj_captions", .nome = "Segunda legenda",
    .frase = "Segurar OK abre mais opções e desliga a segunda legenda. Tamanho de até 300%.", .linhas = 2 },
  { .icone = "aj_captions", .nome = "Sync automático",
    .frase = "Agora funciona na LG, troca a legenda no máximo uma vez por episódio, e o botão de sync mostra o que está fazendo.", .linhas = 3 },
  { .grupo = "Início", .icone = "aj_house", .nome = "Logos e dados",
    .frase = "Logos e informações do título aparecem sem esperar o detalhe completo.", .linhas = 2 },
  { .icone = "aj_user-round", .nome = "Perfis",
    .frase = "Continuar assistindo sob cada avatar e troca rápida de perfil.", .linhas = 2 },
  { .icone = "aj_puzzle", .nome = "Add-ons",
    .frase = "Até 128 add-ons da conta (antes eram 64). Botão para sincronizar; a TV só recebe, nunca envia.", .linhas = 3 },
  { .icone = "aj_library", .nome = "Biblioteca",
    .frase = "Selo de assistido no pôster e ordenar por assistidos e não assistidos.", .linhas = 2 },
  { .grupo = "Plataformas", .icone = "aj_sparkles", .nome = "Ícone do app",
    .frase = "Novo ícone do app, com a etiqueta LEGACY." },
  { .icone = "aj_memory-stick", .nome = "Memória de imagens",
    .frase = "Os Ajustes mostram o limite de memória de imagens da sua TV.", .linhas = 2 },
  { .icone = "aj_sliders-horizontal", .nome = "Samsung .tpk",
    .frase = "O player nativo não trava mais a interface, e o botão de proporção muda a imagem. Reinstale o .tpk para ter tudo: a atualização pelo app não troca essa parte.", .linhas = 4,
    .plataformas = NOV_TPK | NOV_OUTRAS },
  { .icone = "aj_tv-minimal-play", .nome = "LG webOS",
    .frase = "Repositório próprio no Homebrew Channel, para instalar e atualizar.", .linhas = 2,
    .plataformas = NOV_LG | NOV_OUTRAS },
};
static const NovConteudo NOV_ATUAL = {
  .versao = "2.0.5",
  .arquivo = "novidades-205.txt",
  .subtitulo = "Cinema, legendas e perfis.",
  .itens = ITENS_205, .nItens = (int)(sizeof ITENS_205 / sizeof *ITENS_205),
  .cenas = CENAS_205, .nCenas = (int)(sizeof CENAS_205 / sizeof *CENAS_205),
  .artes = ARTES_205, .nArtes = (int)(sizeof ARTES_205 / sizeof *ARTES_205),
};
