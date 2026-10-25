// AS PREVIAS GERADAS DOS AJUSTES (2.0.2): qual desenho do kit (ajcena.c) cada
// categoria e cada opcao mostra, e o que ele anima.
//
// Regras (dono, 06/10: "mais visual do que vai acontecer do que usar a foto"):
// - Nada de foto nem de arte do catalogo: cartaz = bloco de cor, texto = barra.
// - A previa mostra O QUE MUDA. Interruptor LIGADO: o efeito acontecendo em
//   laco (o trailer toca, a fileira anda); DESLIGADO: o estado parado, sem o
//   efeito. Lista de valores: o desenho do valor ATUAL, com a peca que muda no
//   acento. A troca de valor anda pela mola da linha (ajMov), entao a previa
//   acompanha o Esquerda/Direita.
// - Opcao sem nada para mostrar (chave, conta, teste): o icone dela num
//   ladrilho e uma ilustracao de uma linha.
// Todas no mesmo quadro de 640 x 360 do kit; a altura devolvida e sempre
// w * 9/16 (a troca de cena nunca corta por altura).

// --- apoio ---------------------------------------------------------------------------
static float ajcLig(int op) {   // 0..1: o interruptor, animado pela mola da linha
  if (op < 0 || op >= AJ_N) return 1.0f;
  { float v = ajMov(op, (float)uxValorAtual(op)); v = 1.0f - v; return v < 0 ? 0 : v > 1 ? 1 : v; }
}
static float ajcValor(int op) { return op >= 0 && op < AJ_N ? ajMov(op, (float)valor[op]) : 0.0f; }
static void ajcTexto(const char *t, float px, int forte, float x, float y, float a) {
  ajTextoMini(t, ajc_px(px), forte, ajc_x(x), ajc_y(y), 243, 242, 239, a);
}
static void ajcTextoTinta(const char *t, float px, int forte, float x, float y, float a) {
  int ti = ajTinta();
  ajTextoMini(t, ajc_px(px), forte, ajc_x(x), ajc_y(y), ti, ti, ti, a);
}
static float ajcLargura(const char *t, float px, int forte) {
  TxtEstilo e = ajEstiloPxS(ajc_px(px), forte);
  return (int)e >= 0 ? (float)txt_largura(e, t) / ajc_px(1.0f) : (float)strlen(t) * px * 0.6f;
}
// Os VIVOS: glifo a glifo (ajTextoVivo), para o texto que muda com o relogio da
// cena. A largura tambem glifo a glifo, para medir do mesmo jeito que desenha.
static void ajcTextoVivoRGB(const char *t, float px, int forte, float x, float y, int r, int g, int b, float a) {
  ajTextoVivo(t, ajc_px(px), forte, ajc_x(x), ajc_y(y), r, g, b, a);
}
static void ajcTextoVivo(const char *t, float px, int forte, float x, float y, float a) {
  ajcTextoVivoRGB(t, px, forte, x, y, 243, 242, 239, a);
}
static void ajcTextoVivoTinta(const char *t, float px, int forte, float x, float y, float a) {
  int ti = ajTinta();
  ajcTextoVivoRGB(t, px, forte, x, y, ti, ti, ti, a);
}
static float ajcLarguraViva(const char *t, float px, int forte) {
  TxtEstilo e = ajEstiloPxS(ajc_px(px), forte);
  return (int)e >= 0 ? txt_glifos_largura(e, t) / ajc_px(1.0f) : (float)strlen(t) * px * 0.6f;
}
// Selo (pilula com texto): no acento quando `ativo`.
static float ajcSelo(const char *t, float x, float y, float px, float ativo, float a) {
  float w = ajcLargura(t, px, 1) + px * 1.4f, h = px * 1.9f;
  ajc_neutro(x, y, w, h, h * 0.5f, a);
  if (ativo > 0.01f) ajc_acento_ret(x, y, w, h, h * 0.5f, ativo * a);
  if (ativo > 0.5f) ajcTextoTinta(t, px, 1, x + px * 0.7f, y + (h - px * 1.25f) * 0.5f, a);
  else ajcTexto(t, px, 1, x + px * 0.7f, y + (h - px * 1.25f) * 0.5f, 0.9f * a);
  return w;
}
// O selo do texto VIVO (percentual que conta, lingua que circula): a pilula
// acompanha a largura glifo a glifo do que o ajcTextoVivo desenha dentro.
static float ajcSeloVivo(const char *t, float x, float y, float px, float ativo, float a) {
  float w = ajcLarguraViva(t, px, 1) + px * 1.4f, h = px * 1.9f;
  ajc_neutro(x, y, w, h, h * 0.5f, a);
  if (ativo > 0.01f) ajc_acento_ret(x, y, w, h, h * 0.5f, ativo * a);
  if (ativo > 0.5f) ajcTextoVivoTinta(t, px, 1, x + px * 0.7f, y + (h - px * 1.25f) * 0.5f, a);
  else ajcTextoVivo(t, px, 1, x + px * 0.7f, y + (h - px * 1.25f) * 0.5f, 0.9f * a);
  return w;
}
// O que o interruptor faz, no canto de baixo: a chave sincronizada com o efeito.
static void ajcChaveCanto(float liga) {
  ajc_painel(574, 318, 52, 30, 15, 0.85f);
  ajc_chave(580, 322, liga, 1);
}

// O bloco (titulo ROT) da opcao na TELA: as faixas do enum nao seguem os
// blocos, entao "esta no bloco X" e por nome.
static int ajcNoBloco(int op, const char *rot) {
  int i, r = -1;
  for (i = 0; i < AJ_N_TELA; i++) {
    if (TELA[i].tipo == IT_ROT || TELA[i].tipo == IT_SEC) r = i;
    if (TELA[i].tipo == IT_OPC && TELA[i].op == op) return r >= 0 && TELA[r].tipo == IT_ROT && !strcmp(TELA[r].titulo, rot);
  }
  return 0;
}

// --- pecas maiores, reusadas por varias cenas -----------------------------------------
// A Home esquematica: barra lateral, destaque e duas fileiras.
typedef struct { int layout; float hero, heroCheio, paisagem, rail, railVidro, cw, focoAnda; int marca; } AjcHome;
enum { AJC_M_NADA = 0, AJC_M_HERO, AJC_M_FILEIRAS, AJC_M_TITULOS, AJC_M_RAIL, AJC_M_CW };
static void ajcHome(const AjcHome *o) {
  // em coordenadas do quadro; a miniatura dos diagramas usa ajc_sub
  const float k = 1.0f;
#define HX(v) (v)
#define HY(v) (v)
  float railW = 46 * o->rail, cx = 24 + railW, fy;
  int i;
  float pw = 58 + 42 * o->paisagem, ph = 86 - 30 * o->paisagem, gap = 12;
  float foco = o->focoAnda >= 0 ? o->focoAnda : -1;
  // destaque
  if (o->layout == 0) {   // Moderna: o destaque atras de tudo
    float hh = 200 + 160 * o->heroCheio;
    if (o->hero > 0.01f) {
      ajc_cartaz(HX(0), HY(0), 640 * k, hh * k, 1, o->hero);
      ajc_veu(HX(0), HY(0), 420 * k, hh * k, 2, 0.75f * o->hero);
      ajc_veu(HX(0), HY(hh * 0.45f), 640 * k, hh * 0.55f * k, 0, 0.8f * o->hero);
      ajc_ret(HX(cx + 18), HY(70), 150 * k, 16 * k, 4 * k, 1, 1, 1, 0.9f * o->hero);
      ajc_barras(HX(cx + 18), HY(98), 200 * k, 2, o->hero);
      ajc_acento_ret(HX(cx + 18), HY(134), 74 * k, 22 * k, 11 * k, o->hero);
    }
    fy = 214 - 74 * (1 - o->hero);
  } else if (o->layout == 1) {   // Padrao: um banner no alto
    if (o->hero > 0.01f) {
      ajc_cartaz(HX(cx + 10), HY(18), (600 - cx) * k, 150 * k, 4, o->hero);
      ajc_veu(HX(cx + 10), HY(18), 320 * k, 150 * k, 2, 0.7f * o->hero);
      ajc_ret(HX(cx + 30), HY(64), 120 * k, 14 * k, 4 * k, 1, 1, 1, 0.9f * o->hero);
      ajc_barras(HX(cx + 30), HY(90), 160 * k, 2, o->hero);
    }
    fy = 186 - 150 * (1 - o->hero);
  } else {   // Dinamica (Apple TV): o cartao grande em foco e a faixa de numeros
    if (o->hero > 0.01f) {
      ajc_cartaz(HX(cx + 10), HY(20), 330 * k, 168 * k, 2, o->hero);
      ajc_foco(HX(cx + 10), HY(20), 330 * k, 168 * k, 12 * k, o->hero);
      for (i = 0; i < 3; i++) ajc_cartaz(HX(cx + 356 + i * 82), HY(60), 70 * k, 104 * k, 5 + i, 0.8f * o->hero);
    }
    fy = 204 - 180 * (1 - o->hero);
  }
  // barra lateral
  if (o->rail > 0.01f) {
    if (o->railVidro > 0.5f) ajc_vidro(HX(12), HY(16), 46 * k, 328 * k, 16 * k, o->rail);
    else ajc_painel(HX(12), HY(16), 46 * k, 328 * k, 16 * k, 0.9f * o->rail);
    for (i = 0; i < 6; i++) {
      float iy = 60 + i * 40;
      if (i == 1) ajc_acento_ret(HX(23), HY(iy - 2), 24 * k, 24 * k, 8 * k, o->rail);
      else ajc_ret(HX(29), HY(iy + 4), 12 * k, 12 * k, 4 * k, 1, 1, 1, 0.35f * o->rail);
    }
    if (o->marca == AJC_M_RAIL) ajc_foco(HX(12), HY(16), 46 * k, 328 * k, 16 * k, ajc_onda(2.4f) * 0.7f + 0.3f);
  }
  // fileiras (a primeira pode ser o Continuar assistindo)
  for (i = 0; i < 2; i++) {
    float ry = fy + i * (ph + 40);
    int ehCw = i == 0 && o->cw > 0.01f, j;
    if (ry > 360) break;
    ajc_barra(HX(cx + 10), HY(ry), (o->marca == AJC_M_TITULOS ? 110 : 70) * k, 0.55f);
    if (o->marca == AJC_M_TITULOS) ajc_acento_ret(HX(cx + 10), HY(ry), (40 + 70 * ajc_onda(3.0f)) * k, 7 * k, 3.5f * k, 1);
    if (ehCw) {
      for (j = 0; j < 5; j++) {
        float cxj = cx + 10 + j * 112;
        ajc_cartaz(HX(cxj), HY(ry + 16), 100 * k, 56 * k, 3 + j, o->cw);
        ajc_progresso(HX(cxj + 8), HY(ry + 62), 84 * k, 0.25f + 0.15f * j, o->cw);
      }
      if (foco >= 0) ajc_foco(HX(cx + 10 + foco * 112), HY(ry + 16), 100 * k, 56 * k, 9 * k, 1);
      if (o->marca == AJC_M_CW) ajc_foco(HX(cx + 4), HY(ry - 6), (5 * 112 + 4) * k, 84 * k, 12 * k, ajc_onda(2.4f) * 0.6f + 0.2f);
      continue;
    }
    { int n = (int)((616 - cx) / (pw + gap)) + 1;
      if (n > 9) n = 9;
      for (j = 0; j < n; j++) ajc_cartaz(HX(cx + 10 + j * (pw + gap)), HY(ry + 16), pw * k, ph * k, i * 3 + j, 1);
      if (i == 0 && foco >= 0 && o->cw <= 0.01f) ajc_foco(HX(cx + 10 + foco * (pw + gap)), HY(ry + 16), pw * k, ph * k, 6 * k, 1); }
  }
  if (o->marca == AJC_M_HERO && o->hero > 0.01f) {
    float hh = o->layout == 0 ? 200 : 168;
    ajc_foco(HX(o->layout == 0 ? 4 : cx + 10), HY(o->layout == 0 ? 4 : 18), (o->layout == 0 ? 632 : 600 - cx) * k, (hh - 8) * k,
             14 * k, ajc_onda(2.4f) * 0.6f + 0.3f);
  }
#undef HX
#undef HY
}
static AjcHome ajcHomePadrao(void) {
  AjcHome o;
  o.layout = valor[AJ_HOME_LAYOUT] >= 0 && valor[AJ_HOME_LAYOUT] < 3 ? valor[AJ_HOME_LAYOUT] : 0;
  o.hero = ajcLig(AJ_HERO); o.heroCheio = 0; o.paisagem = ajcLig(AJ_LANDSCAPE);
  o.rail = 1; o.railVidro = ajcLig(AJ_RAIL_BLUR); o.cw = 0;
  o.focoAnda = (float)ajc_passo(1.2f, 4) - 1.0f + ajc_passo_t(1.2f);
  if (o.focoAnda < 0) o.focoAnda = 0;
  o.marca = AJC_M_NADA;
  return o;
}

// --- cenas ---------------------------------------------------------------------------
// Tres diagramas de layout lado a lado; o atual no acento.
static void ajcCenaLayouts(void) {
  int i, atual = (int)(ajcValor(AJ_HOME_LAYOUT) + 0.5f);
  for (i = 0; i < 3; i++) {
    float x = 24 + i * 204, y = 70, w = 184, h = 103.5f;
    AjcHome o = ajcHomePadrao();
    o.layout = i; o.hero = 1; o.rail = 1;
    o.focoAnda = i == atual ? o.focoAnda : -1;
    ajc_tv(x, y, w, h, 1);
    gfx_recorte(ajc_x(x + 4), ajc_y(y + 4), ajc_px(w - 8), ajc_px(h - 8));
    ajc_sub(x + 4, y + 4, (w - 8) / 640.0f);
    ajcHome(&o);
    ajc_sub_fim();
    gfx_recorte(ajc_x(0), ajc_y(0), ajc_px(640), ajc_px(360));
    if (i == atual) ajc_foco(x, y, w, h, 12, 1);
    ajc_ret(x + w * 0.5f - 30, y + h + 32, 60, 6, 3, i == atual ? 1 : 0.6f, i == atual ? 1 : 0.6f, i == atual ? 1 : 0.6f, i == atual ? 0.9f : 0.35f);
    if (i == atual) ajc_acento_ret(x + w * 0.5f - 30, y + h + 32, 60, 6, 3, 1);
  }
}
static void ajcCenaHome(int op) {
  AjcHome o = ajcHomePadrao();
  switch (op) {
    case AJ_HERO: o.hero = ajcLig(op); o.marca = AJC_M_HERO; break;
    case AJ_HERO_CHEIO: o.layout = 0; o.heroCheio = ajcLig(op) * 0.45f; o.marca = AJC_M_HERO; break;
    case AJ_HERO_CATALOGOS: case AJ_HERO_TRANSICAO: o.marca = AJC_M_HERO; break;
    case AJ_LANDSCAPE: o.paisagem = ajcLig(op); break;
    case AJ_FIL_ORDEM: case AJ_FIL_LIMITE: case AJ_ITENS_FILEIRA: case AJ_OCULTAR_NLANC:
    case AJ_OCULTAR_FILMES_VISTOS: case AJ_BIB_HOME:
      o.hero = 0.0f; o.marca = AJC_M_FILEIRAS; break;
    case AJ_ROTULOS: case AJ_NOME_ADDON: case AJ_SUFIXO_TIPO: o.hero = 0; o.marca = AJC_M_TITULOS; break;
    case AJ_NOTAS_HOME: case AJ_SELO_VISTO: o.hero = 0; break;
    case AJ_MENU_TOPO: case AJ_RAIL_MODERNA: case AJ_RAIL: case AJ_MENU_EXPLORAR: case AJ_MENU_GUIA: case AJ_MENU_AGENDA:
    case AJ_MENU_PERFIL: case AJ_RAIL_BLUR: case AJ_DESCOBRIR:
      o.marca = AJC_M_RAIL; break;
    default:
      if ((op == AJ_CW_LIGADO || op == AJ_CW_OK || op == AJ_CW_ESTILO || op == AJ_CW_THUMB ||
          op == AJ_CW_BLUR_PROX || op == AJ_CW_ORDEM || op == AJ_CW_NAO_EXIBIDOS || op == AJ_CW_FURTHEST || op == AJ_CW_RETIDO_TAMBEM)) {
        o.hero = 0; o.cw = op == AJ_CW_LIGADO ? ajcLig(op) : 1; o.marca = AJC_M_CW;
      }
      break;
  }
  if (op == AJ_HERO_TRANSICAO && o.hero > 0.01f) {   // o destaque troca de titulo: um cruzamento de cor
    float k = ajcLig(op) > 0.5f ? ajc_onda(3.6f) : 0;
    ajcHome(&o);
    ajc_cartaz(0, 0, 640, 200, 6, k * 0.85f);
    ajc_veu(0, 100, 640, 100, 0, 0.8f * k);
    return;
  }
  ajcHome(&o);
  // detalhes por cima
  if (op == AJ_HERO_CATALOGOS && o.hero > 0.01f) {
    float k = ajcLig(op), i;
    for (i = 0; i < 3; i++) ajc_bolinha(300 + i * 16, 186, i == (float)ajc_passo(1.4f, 3) ? 9 : 6, 1, 1, 1, (0.4f + 0.5f * k) * o.hero);
    ajc_luz(0, 0, 640, 200, 0, 520, 40, 200, 1, 1, 1, 0.10f * k);
  }
  if (op == AJ_NOTAS_HOME || op == AJ_SELO_VISTO) {
    float k = ajcLig(op);
    int j;
    for (j = 0; j < 5; j++) {
      float px = 80 + j * (o.paisagem > 0.5f ? 112 : 70), py = 230 - 74 + 16;
      if (op == AJ_NOTAS_HOME) { ajc_ret(px + 4, py + 4, 30, 14, 4, 0.96f, 0.77f, 0.18f, 0.95f * k); }
      else if (j % 2 == 0) { float ar, ag, ab; ajc_acento(&ar, &ag, &ab);
        ajc_bolinha(px + (o.paisagem > 0.5f ? 88 : 46), py + 12, 18, ar, ag, ab, k);
        ajc_icone_tinta("aj_check", px + (o.paisagem > 0.5f ? 88 : 46), py + 12, 11, k); }
    }
    ajcChaveCanto(k);
  }
  if (op == AJ_OCULTAR_NLANC) {   // o cartaz ainda nao lancado some da fileira
    float k = ajcLig(op) * ajc_onda(3.0f);
    ajc_ret(80 + 2 * 70, 230 - 74 + 16 - 4, 58, 94, 8, 0.075f, 0.082f, 0.102f, k);
    ajc_icone("aj_calendar", 80 + 2 * 70 + 29, 156 + 43, 22, 0.6f * (1 - k) + 0.2f);
    ajcChaveCanto(ajcLig(op));
  }
  if (op == AJ_HERO || op == AJ_LANDSCAPE || op == AJ_RAIL_BLUR || op == AJ_CW_LIGADO || op == AJ_RAIL_MODERNA)
    ajcChaveCanto(ajcLig(op));
}
// A tela de escolha de perfil: os rostos em circulo, o do meio em foco.
static void ajcCenaPerfis(int op) {
  int i, f = ajc_passo(1.6f, 3);
  if (op == AJ_PS_FUNDO) { ajc_cartaz(0, 0, 640, 360, 4, 0.5f * ajcLig(op) + 0.15f); ajc_veu(0, 120, 640, 240, 0, 0.6f); }
  for (i = 0; i < 4; i++) {
    float cx = 140 + i * 120, cy = 160, d = i == f ? 92 : 80;
    float r, g, b;
    ajc_matiz(i * 2 + 1, &r, &g, &b);
    ajc_bolinha(cx, cy, d, r, g, b, 1);
    ajc_icone("aj_user-round", cx, cy, d * 0.42f, 0.85f);
    if (i == f) ajc_foco(cx - d * 0.5f, cy - d * 0.5f, d, d, d * 0.5f, 1);
    ajc_barra(cx - 30, cy + 64, 60, i == f ? 0.8f : 0.35f);
  }
  if (op == AJ_PS_FUNDO || op == AJ_PS_CONTINUAR) ajcChaveCanto(ajcLig(op));
}
// Pagina do titulo: o fundo, o "logo", meta, notas e botoes.
static void ajcCenaTitulo(int op) {
  float veu = op == AJ_DET_VEU ? 0.25f + 0.75f * ajc_onda(3.0f) : 0.6f + 0.4f * valor[AJ_DET_VEU] / 100.0f;
  int i;
  ajc_cartaz(0, 0, 640, 360, 2, 1);
  ajc_veu(0, 0, 460, 360, 2, 0.95f * veu);
  ajc_veu(0, 160, 640, 200, 0, 0.7f * veu);
  ajc_ret(34, 64, 210, 30, 8, 1, 1, 1, 0.92f);           // logo
  ajc_barra(34, 112, 150, 0.6f);                          // meta
  if (op == AJ_DET_DATA_CHEIA) ajc_acento_ret(34, 112, 60 + 60 * ajcLig(op), 7, 3.5f, 1);
  // as notas: chips coloridos; a da opcao pulsa
  { static const float NC[6][3] = { { 0.96f, 0.77f, 0.18f }, { 0.90f, 0.26f, 0.20f }, { 0.36f, 0.75f, 0.35f },
                                     { 0.25f, 0.55f, 0.90f }, { 0.85f, 0.85f, 0.85f }, { 0.94f, 0.48f, 0.18f } };
    int nota = ajcNoBloco(op, "Notas") && op != AJ_MDB_CHAVE && !(op >= AJ_MDB_LIGADO && op <= AJ_MDB_MAL);
    int marca = nota ? (op * 7) % 6 : -1;
    for (i = 0; i < 6; i++) {
      float cx = 34 + i * 58, on = 1;
      if (i == marca) on = ajcLig(op);
      ajc_ret(cx, 132, 50, 22, 6, NC[i][0], NC[i][1], NC[i][2], 0.9f * on + 0.08f);
      ajc_ret(cx + 8, 140, 24, 6, 3, 0, 0, 0, 0.5f * on);
      if (i == marca) ajc_foco(cx, 132, 50, 22, 6, 0.4f + 0.6f * ajc_onda(2.0f));
    }
    if (ajcNoBloco(op, "Notas") && marca < 0) ajc_foco(28, 128, 352, 30, 10, 0.4f + 0.6f * ajc_onda(2.4f)); }
  ajc_barras(34, 176, 300, 3, 1);
  ajc_acento_ret(34, 232, 112, 34, 17, 1);
  ajc_icone_tinta("aj_play", 54, 249, 14, 1);
  ajc_ret(70, 245, 60, 7, 3.5f, ajTinta() / 255.0f, ajTinta() / 255.0f, ajTinta() / 255.0f, 0.9f);
  ajc_vidro(156, 232, 34, 34, 17, 1); ajc_icone("aj_bookmark", 173, 249, 14, 0.9f);
  if (op == AJ_DET_BLUR_NAO_VISTOS) {   // episodios nao vistos borrados
    float k = ajcLig(op);
    for (i = 0; i < 3; i++) {
      ajc_cartaz(34 + i * 130, 286, 118, 66, 5 + i, 1);
      if (i > 0) ajc_ret(34 + i * 130, 286, 118, 66, 8, 0.3f, 0.32f, 0.38f, 0.85f * k);
    }
    ajcChaveCanto(k);
  } else if (ajcNoBloco(op, "Dados do título")) {   // dados do titulo: a ficha a direita
    ajc_vidro(420, 40, 196, 280, 16, 1);
    for (i = 0; i < 6; i++) { ajc_barra(438, 64 + i * 40, 70, 0.4f); ajc_barra(438, 78 + i * 40, 140 - (i % 3) * 20, 0.75f); }
    ajc_ladrilho("aj_database", 590, 30, 36, 1, 0.5f + 0.5f * ajc_onda(2.4f));
  } else {
    for (i = 0; i < 5; i++) ajc_bolinha(52 + i * 52, 314, 40, 0.3f + 0.1f * i, 0.32f, 0.4f, 0.9f);
  }
}
// Cartazes e arte: uma fileira grande com o cartaz em foco.
static void ajcCenaCartazes(int op) {
  float lw = 64 + 40 * ajcValor(AJ_LARGURA_DP) / 40.0f, raio = 2 + ajcValor(AJ_RAIO_DP) * 0.6f;
  float w, h = 150, gap = 18, x0 = 30, y0 = 130, exp = 0, prof = 0;
  int i, f = 1;
  if (lw < 60) lw = 60;
  if (lw > 130) lw = 130;
  w = op == AJ_LARGURA_DP ? 70 + 40 * ajc_onda(3.0f) : 100;
  if (op == AJ_RAIO_DP) raio = 2 + 16 * ajc_onda(3.0f);
  else if (raio > 18) raio = 18;
  (void)lw;
  if (op == AJ_EXPANDIR || op == AJ_EXPANDIR_ATRASO) exp = ajcLig(AJ_EXPANDIR) * ajc_passo_t(3.0f) * (ajc_passo(3.0f, 2) ? 1.0f : 0.0f);
  if (op == AJ_EXPANDIR_ATRASO) {   // a espera antes de abrir: o anel enche, depois abre
    float fr = ajc_fase(3.0f) * 2.0f;
    exp = fr > 1 ? anim_suave((fr - 1) * 3 > 1 ? 1 : (fr - 1) * 3) : 0;
    ajc_anel(x0 + 118 + 50, y0 - 46, 18, fr > 1 ? 1 : fr, 1);
  }
  if (ajcNoBloco(op, "Forma") && op != AJ_LARGURA_DP && op != AJ_RAIO_DP) prof = ajcLig(AJ_PROF) * ajc_onda(2.8f);
  ajc_cartaz(0, 0, 640, 110, 3, 0.35f);
  ajc_veu(0, 0, 640, 110, 0, 0.8f);
  for (i = 0; i < 5; i++) {
    float x = x0 + i * (w + gap) + (i > f ? exp * 150 : 0), ww = w + (i == f ? exp * 150 : 0);
    float yy = y0 - (i == f ? prof * 10 : 0);
    if (i == f && prof > 0.01f) ajc_ret(x + 6, yy + 14, ww - 12, h, raio, 0, 0, 0, 0.5f * prof);
    if (i == f && exp > 0.01f) {
      ajc_cartaz(x, yy, ww, h, i, 1);
    }
    ajc_cartaz(x, yy, ww, h, i + (i == f && exp > 0.5f ? 4 : 0), 1);
    if (i == f && prof > 0.01f) ajc_luz(x, yy, ww, h, raio, ww * 0.3f, -20, h * 0.8f, 1, 1, 1, 0.25f * prof);
    if (i == f && (op != AJ_BORDA_FOCO || ajcLig(AJ_BORDA_FOCO) > 0.5f)) ajc_foco(x, yy, ww, h, raio, op == AJ_BORDA_FOCO ? ajcLig(op) : 1);
  }
  if (op == AJ_GRAD_CLASSICO) ajc_veu(x0 + w + gap, y0 + h * 0.4f, w, h * 0.6f, 0, 0.8f * ajcLig(op));
  ajc_barra(x0 + w + gap, y0 + h + 18, 80, 0.7f);
  // de onde vem a arte: o cartaz recebe a arte de uma fonte (seta)
  if (ajcNoBloco(op, "De onde vem a arte")) {
    float k = ajc_fase(2.4f);
    ajc_ladrilho("aj_download", 560, 60, 48, 1, 1);
    ajc_bolinha(560 - (560 - (x0 + w + gap + w * 0.5f)) * k, 60 + (y0 + 40 - 60) * k, 10, 1, 1, 1, 1 - k * 0.6f);
  }
  if (op == AJ_EXPANDIR || op == AJ_BORDA_FOCO || op == AJ_PROF || op == AJ_GRAD_CLASSICO) ajcChaveCanto(ajcLig(op));
}
// Trailers: o destaque (ou o cartaz) onde o play vira video.
static void ajcCenaTrailer(int op) {
  // O trailer do cartaz tem tres modos (0 = Desligado), nao o V_LIGA.
  int ligado = op == AJ_FOCO_TRAILER ? valor[op] != AJ_TRAILER_CARTAZ_DESLIGADO
             : op >= 0 ? ajcLig(op) > 0.5f : 1;
  float f = ajc_fase(5.0f), vid;
  // o laco: 0-30% parado com o play, depois o video entra e fica
  vid = op == AJ_TRAILER_QUAL || op == AJ_TRAILER_ASPECTO || op == AJ_LOGO_TRAILER || op == AJ_TRAILER_FONTE ? 1.0f
        : (f < 0.3f ? 0.0f : anim_suave((f - 0.3f) / 0.15f > 1 ? 1 : (f - 0.3f) / 0.15f));
  if (ajc_reduzido()) vid = 1;
  if (!ligado && op != AJ_HERO_TRAILER_ESPERA) vid = 0;
  if (op == AJ_FOCO_TRAILER) {   // no cartaz em foco da fileira
    int i;
    // Tela cheia: o video toma a previa e os cartazes apagam (modo cinema).
    if (valor[op] == AJ_TRAILER_CARTAZ_CHEIA) {
      for (i = 0; i < 5; i++) ajc_cartaz(30 + i * 118, 120, 100, 150, i, 1 - vid);
      if (vid > 0.01f) ajc_video(0, 0, 640, 360, 0, vid);
      ajc_play(198, 195, 34, 1 - vid);
      ajcChaveCanto(1.0f);
      return;
    }
    // No card (e Desligado): o video no cartaz em foco, deitado.
    ajc_cartaz(30, 120, 100, 150, 0, 1);
    for (i = 2; i < 4; i++) ajc_cartaz(334 + (i - 2) * 118, 120, 100, 150, i, 1);
    ajc_cartaz(148, 120, 168, 150, 1, 1);
    if (vid > 0.01f) ajc_video(148, 120, 168, 150, 9, vid);
    ajc_play(232, 195, 34, 1 - vid);
    ajc_foco(148, 120, 168, 150, 9, 1);
    ajcChaveCanto(ligado ? 1.0f : 0.0f);
    return;
  }
  ajc_cartaz(0, 0, 640, 360, 1, 1);
  if (vid > 0.01f) ajc_video(0, 0, 640, 360, 0, vid);
  if (op == AJ_TRAILER_ASPECTO) {   // as tarjas pretas do formato
    float k = ajc_onda(3.0f);
    ajc_ret(0, 0, 640, 44 * k, 0, 0, 0, 0, 1); ajc_ret(0, 360 - 44 * k, 640, 44 * k, 0, 0, 0, 0, 1);
  }
  ajc_veu(0, 0, 400, 360, 2, 0.7f);
  ajc_veu(0, 200, 640, 160, 0, 0.7f);
  if (op != AJ_LOGO_TRAILER || ajcLig(op) > 0.5f || ajc_onda(4) < 0.5f)
    ajc_ret(40, 210, 180, 26, 7, 1, 1, 1, op == AJ_LOGO_TRAILER ? 0.9f * (1 - ajcLig(op) * ajc_onda(4.0f) * 0.7f) : 0.92f);
  ajc_barras(40, 250, 220, 2, 1);
  ajc_acento_ret(40, 284, 96, 30, 15, 1);
  ajc_play(320, 150, 54, 1 - vid);
  if (op == AJ_HERO_TRAILER_SOM || op == AJ_DET_TRAILER_SOM) {   // o som: ondas saindo do alto-falante
    float k = ajcLig(op);
    int i;
    ajc_vidro(560, 30, 52, 52, 26, 1);
    ajc_icone(k > 0.5f ? "aj_audio-lines" : "aj_x", 586, 56, 24, 0.95f);
    for (i = 0; i < 3; i++) {
      float ph = fmodf(ajc_fase(1.6f) + i / 3.0f, 1.0f);
      gfx_anel_fora(ajc_r(586 - 12, 56 - 12, 24, 24), 0.5f, ajc_px(10 + ph * 34), ajc_px(2), 1, 1, 1, (1 - ph) * 0.4f * k);
    }
  }
  if (op == AJ_HERO_TRAILER_ESPERA) ajc_anel(320, 150, 48, ajc_fase(5.0f) / 0.3f > 1 ? 1 : ajc_fase(5.0f) / 0.3f, 1);
  if (op == AJ_TRAILER_QUAL) {
    static const char *R[3] = { "480p", "720p", "1080p" };
    int i, a = ajc_passo(1.6f, 3);
    for (i = 0; i < 3; i++) ajcSeloVivo(R[i], 400 + i * 74, 300, 13, i == a ? 1.0f : 0.0f, 1);
  }
  if (op == AJ_TRAILER_FONTE) ajc_ladrilho("aj_film", 586, 56, 52, 1, 1);
  if (op == AJ_HERO_TRAILER || op == AJ_DET_TRAILER_AUTO || op == AJ_DET_TRAILER || op == AJ_LOGO_TRAILER
      || op == AJ_HERO_TRAILER_SOM || op == AJ_DET_TRAILER_SOM) ajcChaveCanto(ajcLig(op));
}
// Cor de destaque: o foco de uma fileira e um botao, recoloridos; embaixo a
// paleta com a cor atual marcada.
static void ajcCenaCor(int op) {
  int i, f = ajc_passo(1.4f, 3) + 1;
  float ar, ag, ab;
  ajc_acento(&ar, &ag, &ab);
  for (i = 0; i < 5; i++) ajc_cartaz(40 + i * 116, 60, 100, 150, i + 2, 1);
  ajc_foco(40 + f * 116, 60, 100, 150, 9, 1);
  ajc_luz(40 + f * 116 - 30, 40, 160, 190, 20, 80, 95, 110, ar, ag, ab, 0.25f);
  ajc_acento_ret(40, 244, 120, 34, 17, 1); ajc_icone_tinta("aj_play", 62, 261, 14, 1);
  ajc_ret(78, 258, 60, 7, 3.5f, ajTinta() / 255.0f, ajTinta() / 255.0f, ajTinta() / 255.0f, 0.9f);
  ajc_chave(180, 250, 1, 1);
  ajc_progresso(240, 259, 180, 0.62f, 1);
  if (op == AJ_COR_LOGO) { ajc_ladrilho("aj_sparkles", 560, 261, 44, ajcLig(op) > 0.5f, 1); ajcChaveCanto(ajcLig(op)); return; }
  for (i = 0; i < 9; i++) {
    float r, g, b;
    int t = (i + 1) % AJ_N_TEMAS_OPC;
    r = ((TEMA_ACENTO[t].fill >> 16) & 255) / 255.0f; g = ((TEMA_ACENTO[t].fill >> 8) & 255) / 255.0f; b = (TEMA_ACENTO[t].fill & 255) / 255.0f;
    ajc_bolinha(52 + i * 30, 320, 20, r, g, b, 0.9f);
  }
  ajc_bolinha(52 + 9 * 30 + 20, 320, 26, ar, ag, ab, 1);
  ajc_foco(52 + 9 * 30 + 7, 307, 26, 26, 13, 1);
}
// Uma tela com o fundo `modo` (0 Arte, 1 Borrada, 2 Frost) e uma ilha por cima.
static void ajcFundoTela(int i, float x, float y, float w, float h) {
  float ar, ag, ab, s = w / 180.0f;
  ajc_acento(&ar, &ag, &ab);
  if (i == 0) { ajc_cartaz(x, y, w, h, 1, 1); ajc_veu(x, y, w, h, 2, 0.5f); }
  else if (i == 1) { ajc_ret(x, y, w, h, 9 * s, 0.42f, 0.25f, 0.24f, 1); ajc_luz(x, y, w, h, 9 * s, 40 * s, 30 * s, 110 * s, 0.85f, 0.55f, 0.4f, 0.5f);
                     ajc_luz(x, y, w, h, 9 * s, 150 * s, 90 * s, 90 * s, 0.4f, 0.3f, 0.7f, 0.5f); }
  else { ajc_ret(x, y, w, h, 9 * s, 0.07f, 0.08f, 0.10f, 1);
         ajc_luz(x, y, w, h, 9 * s, w * (0.3f + 0.4f * ajc_onda(5.0f)), -10 * s, 110 * s, ar, ag, ab, 0.45f);
         ajc_luz(x, y, w, h, 9 * s, 20 * s, 100 * s, 90 * s, 0.5f, 0.6f, 0.8f, 0.2f); }
  ajc_vidro(x + 14 * s, y + 18 * s, 70 * s, 50 * s, 8 * s, 1);   // a ilha por cima: o que se le
  ajc_ret(x + 22 * s, y + 28 * s, 46 * s, 7 * s, 3.5f * s, 1, 1, 1, 0.8f);
  ajc_ret(x + 22 * s, y + 42 * s, 34 * s, 7 * s, 3.5f * s, 1, 1, 1, 0.45f);
}
// Os cartoes da linha Fundo (aj2CartoesFundo): a mesma tela, gerada.
static void ajcMiniFundo(int modo, float x, float y, float w, float a) {
  ajc_comecar(x, y, w, a);
  ajcFundoTela(modo, 0, 0, 640, 360);
  ajc_fim();
}
// Fundo: as tres opcoes (Arte, Borrada, Frost) em tres telas; a atual em foco.
static void ajcCenaFundo(void) {
  int i, atual = valor[AJ_FUNDO] >= 0 && valor[AJ_FUNDO] < 3 ? valor[AJ_FUNDO] : 0;
  for (i = 0; i < 3; i++) {
    float x = 30 + i * 200, y = 80, w = 180, h = 101;
    ajc_tv(x, y, w, h, 1);
    ajcFundoTela(i, x + 4, y + 4, w - 8, h - 8);
    if (i == atual) ajc_foco(x, y, w, h, 12, 1);
    ajc_ret(x + w * 0.5f - 26, y + h + 30, 52, 6, 3, 1, 1, 1, i == atual ? 0 : 0.3f);
    if (i == atual) ajc_acento_ret(x + w * 0.5f - 26, y + h + 30, 52, 6, 3, 1);
  }
}
// Vidro: paineis sobre cartazes; ligado, eles deixam a cor passar com o aro.
static void ajcCenaVidro(int op) {
  float k = op == AJ_VIDRO ? ajcLig(op) : ajcLig(AJ_VIDRO);
  float opac = op == AJ_VIDRO_OPAC ? 0.4f + 0.5f * ajc_onda(3.0f) : 0.6f + 0.08f * ajcValor(AJ_VIDRO_OPAC);
  int i;
  for (i = 0; i < 6; i++) ajc_cartaz(30 + i * 100, 50, 86, 260, i, 1);
  ajc_ret(120, 110, 400, 140, 22, 0.08f, 0.085f, 0.10f, (1 - k) * 0.97f + k * opac);
  if (k > 0.01f) {
    ajc_ret(120, 110, 400, 140, 22, 0.8f, 0.85f, 0.95f, 0.10f * k);
    if (op != AJ_VIDRO_CONTORNO || ajcLig(op) > 0.5f) gfx_anel(ajc_r(120, 110, 400, 140), 22.0f / 140.0f, ajc_px(1.5f), 1, 1, 1, 0.3f * k);
    if (op == AJ_VIDRO_FOSCO && ajcLig(op) > 0.5f) ajc_ret(120, 110, 400, 140, 22, 0.5f, 0.5f, 0.55f, 0.25f);
  }
  ajc_barra(150, 140, 200, 0.85f); ajc_barras(150, 162, 300, 3, 1);
  ajc_acento_ret(150, 210, 100, 26, 13, 1);
  if (op != AJ_VIDRO_OPAC) ajcChaveCanto(op == AJ_VIDRO ? k : ajcLig(op));
}
// Foco na cor de destaque: tres linhas, a do meio em foco; o cinza de sempre
// cede ao vidro tingido com o realce.
static void ajcCenaFocoAcento(int op) {
  float k = ajcLig(op), v = ajcLig(AJ_VIDRO);
  int i;
  ajc_ret(120, 70, 400, 220, 22, 0.08f, 0.085f, 0.10f, 0.9f);
  for (i = 0; i < 3; i++) {
    float y = 92 + i * 62;
    if (i == 1) {
      ajc_ret(136, y, 368, 50, 25, 1, 1, 1, 0.12f * (1 - k * v));
      ajc_ret(136, y, 368, 50, 25, 1, 1, 1, 0.05f * k * v);
      ajc_ret(136, y, 368, 50, 25, C.ar, C.ag, C.ab, gfx_foco_placa_alfa(C.ar, C.ag, C.ab) * k * v);
      gfx_anel(ajc_r(136, y, 368, 50), 25.0f / 50.0f, ajc_px(1.5f), C.ar, C.ag, C.ab, 0.62f * k * v * C.a);
    }
    ajc_ret(158, y + 13, 24, 24, 12, 1, 1, 1, 0.55f);
    ajc_barra(196, y + 16, i == 1 ? 190 : 140, i == 1 ? 0.9f : 0.5f);
  }
  ajcChaveCanto(k);
}
// Logo, abertura e icone do app.
static void ajcCenaMarca(int op) {
  float f = ajc_onda(3.0f);
  if (op == AJ_ABERTURA) {   // a abertura: o logo cresce e a tela clareia
    float k = ajc_fase(3.0f);
    ajc_ret(0, 0, 640, 360, 0, 0.03f, 0.03f, 0.04f, 1);
    ajc_ladrilho("aj_play", 320, 170, 60 + 50 * anim_suave(k > 0.6f ? 1 : k / 0.6f), 1, 1);
    ajc_progresso(250, 270, 140, k, 1);
    return;
  }
  if (op == AJ_ICONE_APP) {   // a grade de apps da TV com o nosso em foco
    int i;
    for (i = 0; i < 6; i++) {
      float x = 60 + i * 90;
      if (i == 2) { ajc_ladrilho("aj_play", x + 36, 180, 72, 1, 1); ajc_foco(x, 144, 72, 72, 20, 1); }
      else ajc_neutro(x, 144, 72, 72, 20, 0.8f);
    }
    return;
  }
  ajc_ladrilho("aj_play", 250, 180, 96, 1, 1);
  ajc_ret(316, 150, 150 + 20 * f, 30, 8, 1, 1, 1, 0.9f);
  ajc_barra(316, 194, 110, 0.5f);
}
// Texto e tamanho: a mesma linha de interface crescendo / trocando de letra.
static void ajcCenaTexto(int op) {
  float esc = 1.0f;
  int i;
  if (op == AJ_TAMANHO_UI || op == AJ_TAMANHO_AJUSTES) {
    static const float E[4] = { 1.0f, 1.2f, 1.3f, 1.5f };
    int v = valor[op] >= 0 && valor[op] < 4 ? valor[op] : 0;
    esc = 0.85f + (E[v] - 1.0f) * 0.9f + 0.04f * ajc_onda(3.0f);
  }
  ajc_tv(60, 40, 520, 290, 1);
  gfx_recorte(ajc_x(66), ajc_y(46), ajc_px(508), ajc_px(278));
  ajc_ret(66, 46, 508, 278, 10, 0.08f, 0.085f, 0.1f, 1);
  for (i = 0; i < 3; i++) {
    float y = 80 + i * 70 * esc;
    ajc_ret(90, y, 52 * esc, 52 * esc, 12 * esc, 0.17f, 0.18f, 0.21f, 1);
    if (i == 0) ajc_foco(90, y, 52 * esc, 52 * esc, 12 * esc, 1);
    ajcTexto(op == AJ_FONTE_UI ? "Aa  Nuvio" : "Nuvio", 22 * esc, i == 0, 160 * 1.0f, y + 6 * esc, i == 0 ? 1 : 0.6f);
    ajc_barra(160, y + 36 * esc, 200 * esc, 0.4f);
  }
  ajc_fim();
  gfx_recorte(ajc_x(0), ajc_y(0), ajc_px(640), ajc_px(360));
}
// #339: o arranjo dos Ajustes. Painel = a arte a esquerda e a lista a direita;
// Lista = as linhas uma embaixo da outra na largura toda, a do foco aberta com
// a explicacao embaixo.
static void ajcCenaLayoutAjustes(int op) {
  int lista = valor[op] == 1, i;
  float k = ajc_onda(3.0f);
  ajc_tv(60, 40, 520, 290, 1);
  gfx_recorte(ajc_x(66), ajc_y(46), ajc_px(508), ajc_px(278));
  ajc_ret(66, 46, 508, 278, 10, 0.06f, 0.065f, 0.08f, 1);
  ajc_neutro(82, 60, 476, 34, 9, 0.9f);              // o cabecalho da pagina
  ajc_acento_ret(92, 68, 18, 18, 5, 1);
  ajc_barra(120, 72, 90, 0.8f);
  if (!lista) {
    ajc_neutro(82, 104, 196, 206, 12, 0.7f);         // a arte da opcao
    ajc_barra(96, 270, 120, 0.6f); ajc_barra(96, 286, 160, 0.3f);
    ajc_neutro(290, 104, 268, 206, 12, 0.9f);
    for (i = 0; i < 6; i++) {
      float y = 116 + i * 31;
      if (i == 2) ajc_foco(298, y - 4, 252, 26, 7, 1);
      ajc_barra(308, y + 6, 110 + (i % 3) * 18, i == 2 ? 0.95f : 0.55f);
      ajc_ret(510, y + 3, 30, 10, 5, 1, 1, 1, i == 2 ? 0.8f : 0.3f);
    }
  } else {
    float y = 106;
    ajc_neutro(82, 104, 476, 206, 12, 0.9f);
    for (i = 0; i < 5; i++) {
      float h = i == 1 ? 60 + 4 * k : 30;
      if (i == 1) ajc_foco(90, y, 460, h, 8, 1);
      ajc_barra(104, y + 11, 150 + (i % 3) * 30, i == 1 ? 0.95f : 0.55f);
      ajc_ret(500, y + 9, 40, 10, 5, 1, 1, 1, i == 1 ? 0.8f : 0.3f);
      if (i == 1) { ajc_barra(104, y + 32, 330, 0.4f); ajc_barra(104, y + 44, 240, 0.3f); }
      else if (i != 0) ajc_ret(100, y + h + 1, 440, 1, 0, 1, 1, 1, 0.07f);
      y += h + 4;
    }
  }
  ajc_fim();
  gfx_recorte(ajc_x(0), ajc_y(0), ajc_px(640), ajc_px(360));
}
// Movimento, relogio e descanso.
static void ajcCenaMovimento(int op) {
  if (op == AJ_ANIM) {   // completas: o foco desliza; reduzidas: pula
    int red = valor[AJ_ANIM] == 1, i;
    float p = red ? (float)ajc_passo(0.9f, 4) : ajc_onda(3.2f) * 3.0f;
    for (i = 0; i < 4; i++) ajc_cartaz(70 + i * 130, 110, 110, 150, i, 1);
    ajc_foco(70 + p * 130, 110, 110, 150, 9, 1);
    return;
  }
  if (op == AJ_RELOGIO || op == AJ_RELOGIO_12H) {
    AjcHome o = ajcHomePadrao();
    ajcHome(&o);
    ajc_vidro(500, 16, 120, 40, 20, ajcLig(AJ_RELOGIO) > 0.5f || op == AJ_RELOGIO_12H ? 1 : 0.2f);
    ajcTexto(op == AJ_RELOGIO_12H && ajcLig(op) > 0.5f ? "8:30 PM" : "20:30", 18, 1, 520, 25, ajcLig(AJ_RELOGIO) > 0.5f ? 1 : 0.2f);
    ajcChaveCanto(ajcLig(op));
    return;
  }
  { // descanso: a tela vai escurecendo e a arte lenta passa
    float k = ajc_onda(6.0f);
    AjcHome o = ajcHomePadrao();
    ajcHome(&o);
    ajc_ret(0, 0, 640, 360, 0, 0, 0, 0, 0.75f * k);
    if (op != AJ_ESMAECER) { ajc_cartaz(170 + 40 * k, 70, 300, 200, 6, k); ajc_veu(170 + 40 * k, 170, 300, 100, 0, 0.7f * k);
      ajcTexto("20:30", 30, 1, 200 + 40 * k, 210, k); }
  }
}
// Formato no player (Ajustes > Formato no player): a fileira num estilo por vez
// (Texto, Pilula, Marcas, Selos; Desligado nao poe nada), na posicao escolhida
// (Ajustes > Posicao do formato no player): embaixo do titulo (a padrao), ao lado
// dele (na altura da barra do titulo, que fica mais estreita para caber), ou no canto
// de cima, do lado oposto ao da ilha do relogio. Cada troca anda pela mola: no meio
// dela os estilos e as posicoes aparecem com o peso de cada um, e sempre ha fileira
// na tela. Com a posicao em foco, a fileira ganha o anel. O 4K (a resolucao) e sempre
// o que acende, no acento.
// Com o codec em foco, o anel vai na peca do audio (o idioma; sem ele, o Atmos).
static float ajcPeso(float v, int k) { float d = fabsf(v - (float)k); return d >= 1.0f ? 0.0f : 1.0f - d; }
static void ajcTextoAcento(const char *t, float px, int forte, float x, float y, float a) {
  float r, g, b;
  ajc_acento(&r, &g, &b);
  ajTextoMini(t, ajc_px(px), forte, ajc_x(x), ajc_y(y), (int)(r * 255), (int)(g * 255), (int)(b * 255), a);
}
// Texto pelo meio da linha (cy): a caixa do ajTextoMini comeca em y.
static void ajcFmtTexto(const char *t, float px, float x, float cy, int acento, float a) {
  if (acento) ajcTextoAcento(t, px, 1, x, cy - px * 0.62f, a);
  else ajcTexto(t, px, 1, x, cy - px * 0.62f, a);
}
// A pilula do player: o mesmo material escuro e de vidro do relogio.
static void ajcPilula(float x, float cy, float w, float h, float a) {
  ajc_ret(x, cy - h * 0.5f, w, h, h * 0.5f, 0.04f, 0.045f, 0.06f, 0.6f * a);
  ajc_vidro(x, cy - h * 0.5f, w, h, h * 0.5f, a);
}
// Selo do pacote (folha de Fontes): caixa cheia na cor do matiz; o 4K no acento.
static float ajcSeloW(const char *t, float s) { return ajcLargura(t, 15.0f * s, 1) + 22.0f * s; }
static void ajcSeloCheio(const char *t, float x, float cy, int m, int acento, float s, float a) {
  const float px = 15.0f * s, h = 28.0f * s;
  float r, g, b;
  if (acento) ajc_acento(&r, &g, &b);
  else { ajc_matiz(m, &r, &g, &b); r *= 0.85f; g *= 0.85f; b *= 0.85f; }
  ajc_ret(x, cy - h * 0.5f, ajcSeloW(t, s), h, 9.0f * s, r, g, b, a);
  if (acento) ajcTextoTinta(t, px, 1, x + 11.0f * s, cy - px * 0.62f, a);
  else ajcTexto(t, px, 1, x + 11.0f * s, cy - px * 0.62f, a);
}
// Cada estilo desenha a fileira a partir de x (canto esquerdo), centrada em cy, na
// escala s (1 = o tamanho da ilha). Com d = 0 so mede. Devolve a largura. kc (0..1) e o
// codec ligado: a peca do audio (o idioma; nas outras, o Atmos) ganha uma barra curta
// depois dela e fica mais longa. aud (se nao for NULL) recebe o x e a largura dessa peca.
static float ajcFmtCodecW(float s, float kc) { return kc * (8.0f + 26.0f) * s; }
static void ajcFmtCodec(float x, float cy, float s, float a, float kc) {
  if (kc > 0.01f) ajc_barra(x, cy - 3.5f, 26.0f * s * kc, 0.45f * a * kc);
}
static float ajcFmtTextoEst(float x, float cy, float s, float a, float kc, int d, float *aud) {
  const float px = 17.0f * s, gap = 16.0f * s;
  float w4 = ajcLargura("4K", px, 1), wH = ajcLargura("HDR10", px, 1), wA = ajcLargura("ATMOS", px, 1);
  float xA = x + w4 + gap + wH + gap;
  if (aud) { aud[0] = xA; aud[1] = wA + ajcFmtCodecW(s, kc); }
  if (d) {
    ajcFmtTexto("4K", px, x, cy, 1, a);
    ajcFmtTexto("HDR10", px, x + w4 + gap, cy, 0, 0.95f * a);
    ajcFmtTexto("ATMOS", px, xA, cy, 0, 0.95f * a);
    ajcFmtCodec(xA + wA + 8.0f * s, cy, s, a, kc);
  }
  return xA + wA - x + ajcFmtCodecW(s, kc);
}
// Pilulas separadas, como na ilha: [4K] [icone Dolby Vision | icone Atmos] | [onda + idioma].
// O idioma e a barra no texto secundario da ilha (0.6 do branco). Com o codec ligado, a
// pilula do idioma ganha a barra do codec no fim, e fica mais longa.
static float ajcFmtPilulaEst(float x, float cy, float s, float a, float kc, int d, float *aud) {
  const float px = 15.0f * s, h = 28.0f * s, pad = 12.0f * s, gap = 8.0f * s, ic = 22.0f * s, sep = 1.5f * s;
  const float ico = 18.0f * s;
  float w1 = ajcLargura("4K", px, 1) + 2.0f * pad;
  float wDv = ajcLargura("Dolby Vision", px, 1), wAt = ajcLargura("Atmos", px, 1);
  float w2 = 2.0f * pad + 2.0f * ic + wDv + wAt + 4.0f * gap + sep;
  float w3 = 2.0f * pad + ico + gap + 58.0f * s + ajcFmtCodecW(s, kc);
  float x2 = x + w1 + gap, x3 = x2 + w2 + 12.0f * s;
  if (aud) { aud[0] = x3; aud[1] = w3; }
  if (d) {
    float sepX = x2 + pad + ic + gap + wDv + gap;
    ajc_acento_ret(x, cy - h * 0.5f, w1, h, h * 0.5f, a);
    ajcTextoTinta("4K", px, 1, x + pad, cy - px * 0.62f, a);
    ajcPilula(x2, cy, w2, h, a);
    ajc_icone("marca_dolby", x2 + pad + ic * 0.5f, cy, ic, a);
    ajcFmtTexto("Dolby Vision", px, x2 + pad + ic + gap, cy, 0, a);
    ajc_ret(sepX, cy - h * 0.3f, sep, h * 0.6f, sep * 0.5f, 1, 1, 1, 0.3f * a);
    ajc_icone("marca_dolby", sepX + sep + gap + ic * 0.5f, cy, ic, a);
    ajcFmtTexto("Atmos", px, sepX + sep + gap + ic + gap, cy, 0, a);
    ajc_ret(x3 - 6.0f * s, cy - h * 0.45f, 2.0f * s, h * 0.9f, s, 1, 1, 1, 0.3f * a);
    ajcPilula(x3, cy, w3, h, a);
    ajc_icone("aj_audio-lines", x3 + pad + ico * 0.5f, cy, ico, 0.6f * a);
    ajc_barra(x3 + pad + ico + gap, cy - 3.5f, 58.0f * s, 0.6f * a);
    ajcFmtCodec(x3 + pad + ico + gap + 58.0f * s + gap, cy, s, a, kc);
  }
  return x3 + w3 - x;
}
// Marcas: as quatro marcas na arte branca de badges.h (4K, HDR10, Dolby Vision, Atmos),
// soltas e sem fundo, como o OSD sempre mostrou; 22 px entre elas (s = escala da fileira).
static float ajcFmtMarcasEst(float x, float cy, float s, float a, float kc, int d, float *aud) {
  static const FormatoMarca M[4] = { FMT_4K, FMT_HDR10, FMT_DV, FMT_ATMOS };
  const float mh = 34.0f * s, gap = 22.0f * s;
  float cx = x, xA = x, wA = 0.0f;
  int i;
  for (i = 0; i < 4; i++) {
    float wm = marca_formato_largura(M[i], ajc_px(mh)) / ajc_px(1.0f);
    if (i == 3) { xA = cx; wA = wm; }
    if (d) marca_formato(M[i], ajc_x(cx), ajc_y(cy - mh * 0.5f), ajc_px(mh), 1.0f, 1.0f, 1.0f, a * C.a);
    cx += wm + gap;
  }
  if (aud) { aud[0] = xA; aud[1] = wA + ajcFmtCodecW(s, kc); }
  if (d) ajcFmtCodec(xA + wA + 8.0f * s, cy, s, a, kc);
  return xA + wA - x + ajcFmtCodecW(s, kc);
}
// Selos: caixas cheias na cor de cada selo, o 4K no acento.
static float ajcFmtSelosEst(float x, float cy, float s, float a, float kc, int d, float *aud) {
  const float gap = 8.0f * s;
  float w1 = ajcSeloW("4K", s), wH = ajcSeloW("HDR10", s), wA = ajcSeloW("ATMOS", s);
  float xA = x + w1 + gap + wH + gap;
  if (aud) { aud[0] = xA; aud[1] = wA + ajcFmtCodecW(s, kc); }
  if (d) {
    ajcSeloCheio("4K", x, cy, 0, 1, s, a);
    ajcSeloCheio("HDR10", x + w1 + gap, cy, 1, 0, s, a);
    ajcSeloCheio("ATMOS", xA, cy, 3, 0, s, a);
    ajcFmtCodec(xA + wA + gap, cy, s, a, kc);
  }
  return w1 + gap + wH + gap + wA + ajcFmtCodecW(s, kc);
}
static float ajcFmtEstilo(int est, float x, float cy, float s, float a, float kc, int d, float *aud) {
  switch (est) {
    case PLR_FMT_TEXTO: return ajcFmtTextoEst(x, cy, s, a, kc, d, aud);
    case PLR_FMT_PILULA: return ajcFmtPilulaEst(x, cy, s, a, kc, d, aud);
    case PLR_FMT_MARCAS: return ajcFmtMarcasEst(x, cy, s, a, kc, d, aud);
    default: return ajcFmtSelosEst(x, cy, s, a, kc, d, aud);
  }
}
static void ajcCenaFormato(int op) {
  const float v = ajcValor(AJ_PLR_FORMATO), vp = ajcValor(AJ_PLR_FORMATO_POS);
  const float pb = ajcPeso(vp, PLR_POS_ABAIXO), pc = ajcPeso(vp, PLR_POS_CANTO);
  const float kc = ajcLig(AJ_PLR_CODEC);   // o codec ligado (0..1, animado)
  float sw[PLR_FMT_OFF], on = 0.0f, ty, k = ajc_onda(4.0f);
  int est;
  for (est = 0; est < PLR_FMT_OFF; est++) { sw[est] = ajcPeso(v, est); on += sw[est]; }
  ty = 250.0f - 30.0f * pb * on;   // embaixo da fileira o titulo sobe ao lugar dela
  if (pc * on > 0.01f) ajc_veu(0, 0, 640, 110, 1, 0.5f * pc * on);   // sombra de cima, como o OSD
  ajc_vidro(500, 16, 120, 40, 20, 1);   // a ilha do relogio: sempre a direita
  ajcTexto("20:30", 18, 1, 531, 25, 0.95f);
  ajc_ret(40, ty, 220, 14, 7, 0.95f, 0.95f, 0.94f, 0.92f);   // o titulo: a barra grossa e a de baixo
  ajc_barra(40, ty + 21.0f, 140, 0.5f);
  for (est = 0; est < PLR_FMT_OFF; est++) {
    float w = sw[est], s, a, c, xe, ce, wd, hb;
    if (w < 0.01f) continue;
    xe = pb * 40.0f + pc * 20.0f;
    ce = pb * 272.0f + pc * 36.0f;   // embaixo e no canto (na altura da ilha)
    s = pb + pc;
    c = ce + (1.0f - w) * 8.0f;
    a = w;
    if (est == PLR_FMT_TEXTO || est == PLR_FMT_MARCAS) a = w * (0.9f + 0.1f * k);
    if (est == PLR_FMT_SELOS) a = w * (0.85f + 0.15f * ajc_onda(2.4f));
    ajcFmtEstilo(est, xe, c, s, a, kc, 1, NULL);
    if (op == AJ_PLR_FORMATO_POS) {   // a peca da posicao em foco: o anel em volta da fileira
      wd = ajcFmtEstilo(est, xe, c, s, a, kc, 0, NULL);
      hb = est == PLR_FMT_MARCAS ? 34.0f * s : 28.0f * s;
      ajc_foco(xe - 8.0f * s, c - hb * 0.5f - 6.0f * s, wd + 16.0f * s, hb + 12.0f * s, 14.0f * s, w);
    }
    if (op == AJ_PLR_CODEC) {   // a peca do audio em foco: o anel em volta dela (o idioma, ou o Atmos)
      float aud[2];
      ajcFmtEstilo(est, xe, c, s, a, kc, 0, aud);
      hb = est == PLR_FMT_MARCAS ? 34.0f * s : 28.0f * s;
      ajc_foco(aud[0] - 8.0f * s, c - hb * 0.5f - 6.0f * s, aud[1] + 16.0f * s, hb + 12.0f * s, 14.0f * s, w);
    }
  }
}
// Texto escuro (sobre a pilula amarela e o quadrado verde).
static void ajcTextoEscuro(const char *t, float px, float x, float y, float a) {
  ajTextoMini(t, ajc_px(px), 1, ajc_x(x), ajc_y(y), 26, 26, 26, a);
}
// O painel da pausa, igual nos dois casos: o veu, o vidro, o titulo e a peca
// de pause (desligado) ou de play (ligado). O que o painel mostra fica no caso.
static void ajcPausaPainel(int op) {
  ajc_ret(0, 0, 640, 360, 0, 0, 0, 0, 0.45f);
  ajc_vidro(40, 76, 330, 150, 18, 1);
  ajc_ret(60, 96, 170, 20, 6, 1, 1, 1, 0.9f);
  ajc_barra(60, 124, 270, 0.5f);
  if (ajcLig(op) < 0.5f) ajc_ladrilho("aj_pause", 380, 150, 60, 0, 0.9f);   // desligado: so o pause
  else ajc_play(380, 150, 56, 0.75f + 0.25f * ajc_onda(4.0f));
  ajcChaveCanto(ajcLig(op));
}
// Player: o video com a barra de tempo; cada opcao acende a sua peca.
static void ajcCenaPlayer(int op) {
  float f = ajc_fase(6.0f);
  int i;
  ajc_video(0, 0, 640, 360, 0, 1);
  if (op == AJ_BRILHO_PLAYER) ajc_ret(0, 0, 640, 360, 0, 0, 0, 0, 0.55f * ajc_onda(3.0f));
  ajc_veu(0, 220, 640, 140, 0, 0.75f);
  ajc_progresso(40, 318, 560, op == AJ_CW_CONCLUIDO ? 0.6f + 0.35f * ajc_onda(4.0f) : 0.35f + 0.1f * f, 1);
  ajc_bolinha(40 + 560 * (op == AJ_CW_CONCLUIDO ? 0.6f + 0.35f * ajc_onda(4.0f) : 0.35f + 0.1f * f), 320.5f, 14, 1, 1, 1, 1);
  ajc_barra(40, 296, 120, 0.7f);
  switch (op) {
    case AJ_PAUSA_OVERLAY: {   // pausado: o painel com o resumo sobe
      float k = ajcLig(op) * ajc_onda(4.0f);
      ajc_ret(0, 0, 640, 360, 0, 0, 0, 0, 0.45f * k);
      ajc_vidro(40, 80 - 20 * (1 - k), 300, 150, 18, k);
      ajc_ret(60, 100 - 20 * (1 - k), 160, 20, 6, 1, 1, 1, 0.9f * k);
      ajc_barras(60, 136 - 20 * (1 - k), 250, 4, k);
      if (ajcLig(op) < 0.5f) ajc_ladrilho("aj_pause", 320, 150, 60, 0, 0.9f);   // desligado: so o pause, sem painel
      else ajc_play(320, 150, 50, 1 - k * 0.4f);
      ajcChaveCanto(ajcLig(op)); break; }
    case AJ_PAUSA_COMENTARIOS: {   // pausado com o painel: um comentario do Trakt; as bolinhas giram
      float k = ajcLig(op), s = ajc_onda(4.0f), a = k * (0.85f + 0.15f * s), y = 128 + (1 - s) * 3.0f;
      int at = ajc_passo(2.0f, 3), j;
      ajcPausaPainel(op);
      if (k > 0.01f) {   // o cartao de vidro: sempre no lugar, a onda so o sobe uns pixels
        ajc_ret(60, y, 290, 76, 12, 0.08f, 0.085f, 0.10f, 0.9f * a);
        ajc_vidro(60, y, 290, 76, 12, a);
        ajc_icone("aj_quote", 82, y + 24, 22, 0.9f * a);
        ajc_barra(104, y + 15, 180, 0.85f * a);
        ajc_barra(104, y + 29, 140, 0.5f * a);
        ajc_barra(104, y + 54, 52, 0.35f * a);
        ajc_barra(164, y + 54, 26, 0.35f * a);
        for (j = 0; j < 3; j++) ajc_bolinha(306 + j * 14, y + 57, 7, 1, 1, 1, (j == at ? 0.95f : 0.3f) * a);
      }
      break; }
    case AJ_PAUSA_NOTAS: {   // pausado com o painel: as notas do titulo, na linha de baixo do ano
      float k = ajcLig(op), s = ajc_onda(4.0f), a = k * (0.85f + 0.15f * s), cy = 152 + (1 - s) * 3.0f, x = 56, w;
      const float p = 14;
      ajcPausaPainel(op);
      if (k > 0.01f) {   // a linha nunca some: a onda so a sobe uns pixels e acende
        ajc_ret(x, cy - 14, 50, 28, 14, 0.96f, 0.77f, 0.18f, a);             // IMDb: pilula amarela com a nota
        ajcTextoEscuro("8.4", p, x + (50 - ajcLargura("8.4", p, 1)) * 0.5f, cy - p * 0.625f, a);
        x += 50 + 8; ajc_barra(x, cy - 3.5f, 16, 0.5f * a); x += 16 + 10;
        ajc_bolinha(x + 11, cy, 22, 0.93f, 0.27f, 0.22f, a);                  // Rotten Tomatoes: bolinha vermelha
        x += 22 + 6; w = ajcLargura("91%", p, 1);
        ajcTexto("91%", p, 1, x, cy - p * 0.625f, 0.95f * a); x += w + 8;
        ajc_barra(x, cy - 3.5f, 16, 0.5f * a); x += 16 + 10;
        ajc_ret(x, cy - 12, 24, 24, 6, 0.30f, 0.80f, 0.45f, a);              // Metacritic: quadrado verde
        ajcTextoEscuro("78", p, x + (24 - ajcLargura("78", p, 1)) * 0.5f, cy - p * 0.625f, a);
        x += 24 + 8; ajc_barra(x, cy - 3.5f, 16, 0.5f * a); x += 16 + 10;
        ajc_bolinha(x + 10, cy, 20, 0.93f, 0.11f, 0.14f, a);                  // Trakt: bolinha vermelha da marca
        x += 20 + 6; ajc_barra(x, cy - 3.5f, 16, 0.5f * a);
      }
      ajc_barra(60, 186, 270, 0.3f * k);
      break; }
    case AJ_PAUSA_ELENCO: {   // pausado com o painel: a fileira do elenco, fotos redondas com nome e papel
      float k = ajcLig(op), s = ajc_onda(4.0f), a = k * (0.85f + 0.15f * s);
      ajcPausaPainel(op);
      if (k > 0.01f)
        for (i = 0; i < 6; i++) {
          float cx = 62 + i * 56, cy = 250 + (1 - s) * 3.0f;
          ajc_bolinha(cx, cy, 36, 0.62f, 0.64f, 0.68f, a * (i % 2 ? 0.55f : 0.75f));   // foto
          ajc_barra(cx - 18, cy + 24, 36, 0.55f * a);                                   // nome
          ajc_barra(cx - 12, cy + 33, 24, 0.3f * a);                                    // papel
        }
      break; }
    case AJ_REACAO_CREDITOS: {   // nos creditos: a pergunta aparece
      float k = ajcLig(op) * ajc_onda(4.0f);
      for (i = 0; i < 6; i++) ajc_barra(250, 60 + i * 24 - 30 * f, 140, 0.25f);
      ajc_vidro(380, 200, 220, 70, 16, k);
      for (i = 0; i < 3; i++) ajc_ladrilho(i == 0 ? "aj_thumbs-up" : i == 1 ? "pl_thumbs-down" : "aj_heart", 420 + i * 70, 235, 40, i == 0, k);
      ajcChaveCanto(ajcLig(op)); break; }
    case AJ_CACHE_SEEK: case AJ_SEEKR_LIGADO: case AJ_SEEKR_FITA: case AJ_SEEKR_AJUSTE: case AJ_SEEKR_CHAVE: case AJ_SEEKR_TESTAR: {
      // a fita de miniaturas sobre a barra; o cursor anda nela
      float c = ajc_onda(4.0f);
      for (i = 0; i < 6; i++) ajc_cartaz(40 + i * 94, 210, 84, 48, i, op == AJ_CACHE_SEEK ? 0.5f : 1);
      ajc_foco(40 + c * 5 * 94, 210, 84, 48, 6, 1);
      if (op == AJ_CACHE_SEEK) for (i = 0; i < 8; i++) ajc_ret(40 + i * 70, 316, 60, 9, 4, 1, 1, 1, i < (int)(c * 8) + 1 ? 0.5f : 0.08f);
      if (op == AJ_SEEKR_LIGADO || op == AJ_SEEKR_FITA) ajcChaveCanto(ajcLig(op));
      break; }
    case AJ_SAIDA_PLAYER: case AJ_MANTER_VIDEO: {   // ao sair: o video encolhe para a ilha no canto
      float k = ajc_onda(4.0f);
      AjcHome o = ajcHomePadrao();
      ajc_ret(0, 0, 640, 360, 0, 0.075f, 0.082f, 0.102f, k);
      if (k > 0.01f) { float g = gfx_opacidade_grupo;
        o.hero = 1; o.focoAnda = -1;
        gfx_opacidade_grupo = g * k; ajcHome(&o); gfx_opacidade_grupo = g; }
      ajc_video(640 - (640 - 440) * k, 360 - (360 - 250) * k, 640 - 460 * k, 360 - 260 * k, 14 * k, 1);
      if (k > 0.3f) ajc_foco(640 - (640 - 440) * k, 360 - (360 - 250) * k, 640 - 460 * k, 360 - 260 * k, 14, k);
      if (op == AJ_MANTER_VIDEO) ajcChaveCanto(ajcLig(op));
      break; }
    case AJ_PLR_FORMATO: case AJ_PLR_FORMATO_POS: case AJ_PLR_CODEC: ajcCenaFormato(op); break;
    case AJ_CW_CONCLUIDO: {
      ajc_ladrilho("aj_check", 600, 40, 44, 1, ajc_onda(4.0f) > 0.85f ? 1 : 0.25f);
      break; }
    default:
      ajc_play(320, 150, 54, 0.35f);
      break;
  }
}
// Qualidade e formatos: os selos de resolucao crescendo ate o maximo.
static void ajcCenaQualidade(int op) {
  if (op == AJ_DV || op == AJ_ATMOS || op == AJ_FONTE_HDR) {
    float k = op == AJ_FONTE_HDR ? 1.0f : ajcLig(op), w = ajc_onda(3.0f);
    ajc_tv(120, 40, 400, 225, 1);
    ajc_ret(126, 46, 388, 213, 8, 0.06f, 0.06f, 0.08f, 1);
    // a mesma imagem: apagada (SDR) ou com o brilho e a cor do HDR
    ajc_luz(126, 46, 388, 213, 8, 120 + 160 * w, 60, 200, 0.95f, 0.6f, 0.3f, 0.35f + 0.45f * k);
    ajc_luz(126, 46, 388, 213, 8, 320 - 120 * w, 180, 180, 0.3f, 0.5f, 0.95f, 0.30f + 0.4f * k);
    if (op == AJ_ATMOS) { int i; for (i = 0; i < 5; i++) ajc_ret(300 + i * 12, 150 - 30 * k * fabsf(sinf(ajc_t() * 3 + i)), 6,
                          10 + 60 * k * fabsf(sinf(ajc_t() * 3 + i)), 3, 1, 1, 1, 0.8f); }
    ajcSelo(op == AJ_ATMOS ? "ATMOS" : op == AJ_DV ? "DOLBY VISION" : "HDR", 140, 300, 14, k, 1);
    if (op != AJ_FONTE_HDR) ajcChaveCanto(k);
    return;
  }
  { static const char *R[3] = { "720p", "1080p", "4K" };
    // Automatica = ate 4K; 4K; 1080p; 720p
    int v = valor[AJ_QUALIDADE], max = v == 3 ? 0 : v == 2 ? 1 : 2, i;
    float c = ajc_passo_t(3.0f);
    if (op == AJ_RESOLUCAO) max = valor[AJ_RESOLUCAO] == 1 ? 2 : valor[AJ_RESOLUCAO] == 2 ? 0 : 1;
    for (i = 0; i < 3; i++) {
      float w = 120 + i * 50, h = w * 9.0f / 16.0f, x = 52 + i * 144 + (i > 1 ? 50 : 0), y = 250 - h;
      int on = i <= max;
      ajc_tv(x, y, w, h, on ? 1 : 0.35f);
      if (on) { ajc_cartaz(x + 4, y + 4, w - 8, h - 8, 1 + i, 1); ajc_veu(x + 4, y + 4, w - 8, h - 8, 0, 0.3f); }
      ajcSelo(R[i], x + w * 0.5f - 26, 290, 14, i == max ? c : 0, on ? 1 : 0.35f);
    }
    { float w = 120 + max * 50; ajc_foco(52 + max * 144 + (max > 1 ? 50 : 0), 250 - w * 9.0f / 16.0f, w, w * 9.0f / 16.0f, 12, c); } }
}
// Fontes: a lista de fontes com selos; a automatica escolhe uma.
static void ajcCenaFontes(int op) {
  static const char *Q[5] = { "4K", "1080p", "4K", "720p", "1080p" };
  int i, esc = ajc_passo(2.2f, 5);
  float tam[5] = { 0.9f, 0.45f, 0.7f, 0.2f, 0.35f };
  if (op == AJ_FONTE_PRAZO) {   // a espera pelos addons: o anel enche, as fontes vao chegando
    float f = ajc_fase(4.0f);
    ajc_anel(120, 180, 60, f, 1);
    ajc_icone("aj_clock", 120, 180, 30, 0.8f);
    for (i = 0; i < 5; i++) if (f > i * 0.18f) { ajc_neutro(240, 70 + i * 48, 360, 38, 10, 1); ajc_barra(256, 86 + i * 48, 150, 0.7f); ajcSelo(Q[i], 520, 76 + i * 48, 12, 0, 1); }
    return;
  }
  for (i = 0; i < 5; i++) {
    float y = 40 + i * 56, al = 1;
    if (op == AJ_TAM_MAX && tam[i] > 0.8f) al = 0.25f;
    if (op == AJ_TAM_MIN && tam[i] < 0.3f) al = 0.25f;
    if (op == AJ_FONTE_MANUAL && ajcLig(op) > 0.5f) al = 1;
    ajc_neutro(40, y, 560, 46, 12, al);
    ajc_ladrilho("aj_puzzle", 66, y + 23, 30, 0, al);
    ajc_barra(92, y + 13, 170, 0.75f * al); ajc_barra(92, y + 27, 110, 0.35f * al);
    ajcSelo(Q[i], 420, y + 12, 12, op == AJ_SELOS_CORES ? 0.5f : 0, al);
    if (op == AJ_TAM_MAX || op == AJ_TAM_MIN) { ajc_progresso(500, y + 21, 80, tam[i], al); }
    if (i == esc && op != AJ_FONTE_MANUAL && al > 0.5f) { ajc_foco(40, y, 560, 46, 12, ajc_passo_t(2.2f)); ajc_icone("aj_check", 580, y + 23, 16, ajc_passo_t(2.2f)); }
  }
  if (op == AJ_FONTE_MANUAL) {   // perguntar: a lista abre e o foco desce
    ajc_foco(40, 40 + esc * 56, 560, 46, 12, 1);
    ajcChaveCanto(ajcLig(op));
  }
  if (op == AJ_FONTE_REPOR) {   // outra fonte se falhar: a primeira fica vermelha, o foco pula
    float f = ajc_fase(3.0f);
    ajc_ret(40, 40, 560, 46, 12, 0.85f, 0.25f, 0.22f, f > 0.4f ? 0.45f : 0);
    ajc_foco(40, f > 0.55f ? 96 : 40, 560, 46, 12, 1);
  }
}
// Addons e plugins: pecas de quebra-cabeca com interruptores.
static void ajcCenaAddons(int op) {
  int i, sel = ajc_passo(1.8f, 4);
  for (i = 0; i < 4; i++) {
    float y = 50 + i * 66;
    ajc_neutro(60, y, 360, 54, 14, 1);
    ajc_ladrilho(op == AJ_PLUGINS ? "aj_plug" : "aj_puzzle", 92, y + 27, 34, 0, 1);
    ajc_barra(120, y + 18, 150, 0.75f); ajc_barra(120, y + 32, 90, 0.35f);
    ajc_chave(364, y + 16, i == 2 ? (sel == 2 ? 1 - ajc_passo_t(1.8f) : 0) : 1, 1);
    if (i == sel) ajc_foco(60, y, 360, 54, 14, 1);
  }
  // o catalogo que eles alimentam
  for (i = 0; i < 3; i++) ajc_cartaz(450 + (i % 2) * 70, 60 + (i / 2) * 110, 60, 90, i + 3, i == 2 && sel == 2 ? 1 - ajc_passo_t(1.8f) : 1);
  if (op == AJ_ADDONS_PRINCIPAL || op == AJ_BUSCA_CINEMETA) ajcChaveCanto(ajcLig(op));
}
// Debrid, P2P, servidores, TV ao vivo: a TV ligada a um servico, com o pulso.
static void ajcCenaServico(int op, const char *icone) {
  float f = ajc_fase(1.8f);
  ajc_tv(60, 110, 200, 112, 1);
  ajc_cartaz(64, 114, 192, 104, 4, 1);
  ajc_play(160, 166, 30, 0.9f);
  ajc_ladrilho(icone, 500, 166, 90, 1, 1);
  ajc_ret(276, 164, 172, 4, 2, 1, 1, 1, 0.15f);
  ajc_bolinha(448 - 172 * f, 166, 10, 1, 1, 1, 1 - f * 0.5f);
  ajc_bolinha(448 - 172 * fmodf(f + 0.5f, 1.0f), 166, 8, 1, 1, 1, 0.6f);
  (void)op;
}
// Legendas: a cena do player com a legenda (e a segunda) desenhada como a
// opcao diz.
static void ajcCenaLegenda(int op) {
  static const float COR[7][3] = { { 1, 1, 1 }, { 1, 1, 1 }, { 1, 0.9f, 0.2f }, { 0.4f, 0.9f, 0.4f },
                                   { 0.4f, 0.7f, 1 }, { 1, 0.35f, 0.3f }, { 0.1f, 0.1f, 0.1f } };
  static const float FUN[6] = { 0.5f, 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
  int c = valor[AJ_LEG2_COR] >= 0 && valor[AJ_LEG2_COR] < 7 ? valor[AJ_LEG2_COR] : 0;
  // Tamanho real da opcao (ajustes_leg2_tamanho), nao mais a tabela antiga
  // de 7: 0 = auto, 90% da principal; senao o percentual escolhido (60..300).
  int pct2 = ajustes_leg2_tamanho();
  float tam = pct2 > 0 ? pct2 / 100.0f : 0.9f;
  float fun = FUN[valor[AJ_LEG2_FUNDO] >= 0 && valor[AJ_LEG2_FUNDO] < 6 ? valor[AJ_LEG2_FUNDO] : 0];
  float borda = op == AJ_LEG2_BORDA ? ajcLig(op) : 0.0f, junto = ajcValor(AJ_LEG2_POS);
  int segunda = op == AJ_LEG_LINGUA2 || op == AJ_LEG2_POS || op == AJ_LEG2_TAMANHO || op == AJ_LEG2_COR
                || op == AJ_LEG2_FUNDO || op == AJ_LEG2_BORDA;
  ajc_video(0, 0, 640, 360, 0, 1);
  ajc_veu(0, 200, 640, 160, 0, 0.4f);
  // a principal
  ajc_legenda(320, 330, 1.0f, 0.45f, 1, 1, 1, 0, segunda ? 0.7f : 1);
  if (segunda) {
    float yb = 52 + (300 - 18 * 2 - 52) * junto + 24 * tam;
    if (op == AJ_LEG2_TAMANHO) tam *= 0.85f + 0.15f * ajc_onda(2.5f);
    ajc_legenda(320, yb, tam * 0.9f, op == AJ_LEG2_FUNDO ? fun * (0.5f + 0.5f * ajc_onda(2.5f)) + 0.1f : fun,
                COR[c][0], COR[c][1], COR[c][2], borda, 1);
    if (op == AJ_LEG2_POS || op == AJ_LEG2_COR || op == AJ_LEG2_TAMANHO || op == AJ_LEG2_FUNDO) {
      float lw = 220 * tam * 0.9f;
      ajc_foco(320 - lw * 0.5f, yb - 32 * tam * 0.9f, lw, 32 * tam * 0.9f, 6, 0.4f + 0.6f * ajc_onda(2.0f));
    }
  }
  if (op == AJ_LEG_LINGUA || op == AJ_AUD_LINGUA || op == AJ_AUD_LINGUA2 || op == AJ_LEG_SYNC_AUTO || op == AJ_LEG_SYNC_AUDIO) {
    static const char *L[3] = { "PT", "EN", "ES" };
    int i, a = ajc_passo(1.6f, 3);
    ajc_vidro(430, 30, 180, 60, 16, 1);
    ajc_icone(op == AJ_AUD_LINGUA || op == AJ_AUD_LINGUA2 ? "aj_audio-lines" : "aj_captions", 458, 60, 22, 0.9f);
    for (i = 0; i < 3; i++) ajcSeloVivo(L[i], 482 + i * 42, 46, 12, i == a ? 1.0f : 0, 1);
  }
}
// Idioma do app / metadados: o mesmo menu trocando de lingua (barras que
// mudam de largura) e o chip da lingua.
static void ajcCenaIdioma(int op) {
  static const char *L[4] = { "PT", "EN", "ES", "DE" };
  int i, a = ajc_passo(2.0f, 4);
  float t = ajc_passo_t(2.0f);
  ajc_tv(60, 40, 520, 290, 1);
  ajc_ret(66, 46, 508, 278, 10, 0.08f, 0.085f, 0.1f, 1);
  ajc_painel(80, 60, 150, 250, 14, 1);
  for (i = 0; i < 5; i++) {
    float w0 = 60 + ((i * 37 + a * 23) % 50), w1 = 60 + ((i * 37 + ((a + 3) % 4) * 23) % 50);
    ajc_barra(100, 84 + i * 40, w1 + (w0 - w1) * t, i == 1 ? 0.9f : 0.45f);
    if (i == 1) ajc_foco(88, 72 + i * 40, 134, 30, 10, 1);
  }
  for (i = 0; i < 4; i++) ajcSeloVivo(L[i], 270 + i * 70, 70, 16, i == a ? t : 0, 1);
  ajc_cartaz(270, 130, 140, 170, 2, 1);
  ajc_barras(424, 140, 130, 5, 1);
  (void)op;
}
// TV ao vivo: a grade de canais com a faixa do agora.
static void ajcCenaAoVivo(int op) {
  int i, j, f = ajc_passo(1.5f, 4);
  float agora = 200 + 80 * ajc_fase(8.0f);
  if (op == AJ_LIVETV_DIAG || op == AJ_LIVETV_ESPERA) {
    float fr = ajc_fase(3.0f);
    ajc_tv(170, 50, 300, 170, 1);
    ajc_ret(176, 56, 288, 158, 8, 0.06f, 0.06f, 0.08f, 1);
    ajc_anel(320, 135, 40, fr, 1);
    for (i = 0; i < 3; i++) { ajc_ladrilho(i < (int)(fr * 4) ? "aj_check" : "aj_circle-dot", 230 + i * 90, 280, 36, i < (int)(fr * 4), 1); }
    return;
  }
  for (i = 0; i < 5; i++) {
    float y = 40 + i * 58;
    ajc_neutro(30, y, 120, 48, 10, 1);
    ajc_ladrilho("aj_tv-minimal-play", 56, y + 24, 30, i == f, 1);
    ajc_barra(78, y + 20, 56, 0.5f);
    for (j = 0; j < 3; j++) {
      float x = 160 + j * 156 - (i % 2) * 40;
      ajc_neutro(x, y, 148, 48, 10, 0.7f);
      ajc_barra(x + 12, y + 20, 90 - j * 10, 0.55f);
    }
    if (i == f) ajc_foco(30, y, 120, 48, 10, 1);
  }
  ajc_acento_ret(agora, 30, 3, 300, 1.5f, 1);
}
// Conta, perfis e servicos de lista.
static void ajcCenaConta(int op) {
  if (op == AJ_SYNC || op == AJ_TRAKT || op == AJ_SIMKL || op == AJ_CW_FONTE || op == AJ_CW_PROXIMO || op == AJ_SALVOS_DEST || op == AJ_HIST_CONTA || op == AJ_SOCIAL) {
    // duas telas trocando o progresso pela nuvem
    float f = ajc_fase(2.4f);
    ajc_tv(40, 120, 180, 101, 1); ajc_cartaz(44, 124, 172, 93, 1, 1); ajc_progresso(56, 200, 148, 0.4f + 0.3f * ajc_onda(4.8f), 1);
    ajc_tv(420, 120, 180, 101, 1); ajc_cartaz(424, 124, 172, 93, 1, 1); ajc_progresso(436, 200, 148, 0.4f + 0.3f * ajc_onda(4.8f), 1);
    ajc_ladrilho(op == AJ_TRAKT ? "aj_link" : op == AJ_SIMKL ? "aj_link" : "aj_cloud", 320, 90, 70, 1, 1);
    ajc_bolinha(220 + 100 * f, 170 - 80 * f, 9, 1, 1, 1, 1 - f * 0.4f);
    ajc_bolinha(420 - 100 * f, 90 + 80 * f, 9, 1, 1, 1, 1 - f * 0.4f);
    return;
  }
  if (op == AJ_ENQUETES || op == AJ_DISCORD) {
    float k = ajc_onda(3.0f);
    AjcHome o = ajcHomePadrao();
    ajcHome(&o);
    ajc_vidro(380, 20 + 20 * (1 - k), 236, 70, 16, k);
    ajc_ladrilho(op == AJ_DISCORD ? "aj_message-square" : "aj_bell", 414, 55 + 20 * (1 - k), 38, 1, k);
    ajc_barra(444, 46 + 20 * (1 - k), 140, 0.8f * k); ajc_barra(444, 62 + 20 * (1 - k), 100, 0.4f * k);
    ajcChaveCanto(ajcLig(op));
    return;
  }
  ajcCenaPerfis(op);
}
// Desempenho: o medidor, a velocidade e o espaco.
static void ajcCenaDesempenho(int op) {
  int i;
  if (op == AJ_RESOLUCAO) { ajcCenaQualidade(op); return; }
  if (op == AJ_GPU_EFEITOS || op == AJ_QUALIDADE_IMG) { ajcCenaVidro(AJ_VIDRO); return; }
  if (op == AJ_NAV_RAPIDA) {   // a fileira correndo: com a navegacao rapida o foco pula de 3 em 3
    float p = ajc_fase(2.0f) * 6.0f;
    for (i = 0; i < 8; i++) ajc_cartaz(30 + i * 80 - p * 20, 110, 70, 105, i, 1);
    ajc_foco(190, 110, 70, 105, 7, 1);
    ajcChaveCanto(ajcLig(op));
    return;
  }
  if (op == AJ_ESPACO || op == AJ_TEX_MB) {   // o espaco: a barra enchendo de blocos coloridos
    float f = ajc_onda(4.0f);
    ajc_neutro(60, 150, 520, 50, 14, 1);
    for (i = 0; i < 10; i++) if (i < 3 + (int)(f * 6)) ajc_cartaz(66 + i * 51, 156, 46, 38, i, 1);
    ajc_barra(60, 220, 160, 0.5f);
    return;
  }
  // medidor / diagnostico / velocidade: um velocimetro e o grafico de quadros
  { float f = ajc_onda(3.0f);
    ajc_anel(170, 180, 80, 0.3f + 0.6f * f, 1);
    ajcTexto(op == AJ_VELOCIDADE ? "Mb/s" : "fps", 18, 1, 152, 170, 0.9f);
    for (i = 0; i < 14; i++) {
      float hh = 30 + 60 * fabsf(sinf(i * 0.9f + ajc_t() * 1.5f));
      ajc_ret(320 + i * 20, 260 - hh, 12, hh, 3, 1, 1, 1, 0.35f);
      if (i == 13) ajc_acento_ret(320 + i * 20, 260 - hh, 12, hh, 3, 1);
    } }
}
// Sem desenho proprio: o icone da opcao num ladrilho e uma linha de barras.
static void ajcCenaIcone(int op, int sec) {
  const char *ic = op >= 0 ? ajVisualIcone(op) : TELA[secIni[sec]].icone;
  float k = ajc_onda(3.0f);
  if (!ic || !ic[0]) ic = "aj_settings-2";
  ajc_ladrilho(ic, 320, 150, 100 + 6 * k, 1, 1);
  ajc_luz(0, 0, 640, 360, 0, 320, 150, 220, 1, 1, 1, 0.05f + 0.04f * k);
  ajc_barra(230, 240, 180, 0.6f);
  ajc_barra(260, 256, 120, 0.3f);
}

// --- cenas proprias por opcao (2.0.3), uma pasta por grupo de categorias ---------------
#include "ajustes_ux_cenas_reproducao.inc"
#include "ajustes_ux_cenas_cartazes.inc"
#include "ajustes_ux_cenas_servicos.inc"
#include "ajustes_ux_cenas_home.inc"
#include "ajustes_ux_cenas_titulo.inc"

// --- despacho ------------------------------------------------------------------------
static void ajcCenaOpcao(int sec, int op) {
  if (ajcNovaReproducao(sec, op) || ajcNovaCartazes(sec, op) || ajcNovaServicos(sec, op) ||
      ajcNovaHome(sec, op) || ajcNovaTitulo(sec, op)) return;
  switch (op) {
    case AJ_HOME_LAYOUT: ajcCenaLayouts(); return;
    case AJ_PS_FUNDO: case AJ_PS_CONTINUAR: ajcCenaPerfis(op); return;
    case AJ_TEMA: case AJ_COR_LOGO: ajcCenaCor(op); return;
    case AJ_FUNDO: ajcCenaFundo(); return;
    case AJ_VIDRO: case AJ_VIDRO_CONTORNO: case AJ_VIDRO_OPAC: case AJ_VIDRO_FOSCO: case AJ_VIDRO_QUALIDADE: ajcCenaVidro(op); return;
    case AJ_FOCO_ACENTO: ajcCenaFocoAcento(op); return;
    case AJ_LOGO_APP: case AJ_ABERTURA: case AJ_ICONE_APP: ajcCenaMarca(op); return;
    case AJ_FONTE_UI: case AJ_TAMANHO_UI: case AJ_TAMANHO_AJUSTES: ajcCenaTexto(op); return;
    case AJ_LAYOUT_AJUSTES: ajcCenaLayoutAjustes(op); return;
    case AJ_ANIM: case AJ_RELOGIO: case AJ_RELOGIO_12H: case AJ_ESMAECER: case AJ_DESCANSO_ESTILO: case AJ_DESCANSO_FONTE:
      ajcCenaMovimento(op); return;
    case AJ_QUALIDADE: case AJ_DV: case AJ_ATMOS: case AJ_FONTE_HDR: ajcCenaQualidade(op); return;
    case AJ_ADDONS: case AJ_PLUGINS: case AJ_ADDONS_PRINCIPAL: case AJ_BUSCA_CINEMETA: case AJ_BUSCA_NUVIO: case AJ_BUSCA_ORIGEM: ajcCenaAddons(op); return;
    case AJ_IDIOMA: case AJ_TMDB_IDIOMA: ajcCenaIdioma(op); return;
    case AJ_DEBRID_RD: case AJ_DEBRID_TB: case AJ_DEBRID_PM: case AJ_DEBRID_AD: case AJ_DEBRID_AD_TESTAR:
    case AJ_DEBRID_RD_TESTAR: case AJ_DEBRID_TB_TESTAR: case AJ_DEBRID_PM_TESTAR:
      ajcCenaServico(op, "aj_zap"); return;
#ifndef __EMSCRIPTEN__
    case AJ_P2P_LIGADO: case AJ_P2P_URL: case AJ_P2P_TESTAR: case AJ_P2P_LIMITE: case AJ_P2P_DESTINO: ajcCenaServico(op, "aj_link"); return;
#endif
    case AJ_JF_LIGADO: case AJ_JF_SERVIDOR: case AJ_JF_ENTRAR: case AJ_JF_SAIR: case AJ_EM_SERVIDOR: case AJ_EM_ENTRAR:
    case AJ_EM_SAIR: case AJ_PX_ENTRAR: case AJ_PX_SERVIDOR: case AJ_PX_SAIR:
      ajcCenaServico(op, "aj_server"); return;
    case AJ_XTREAM_SERVIDOR: case AJ_XTREAM_USUARIO: case AJ_XTREAM_SENHA: case AJ_XTREAM_CONTA: case AJ_XTREAM_LIMPAR:
    case AJ_STALKER_PORTAL: case AJ_STALKER_MAC: case AJ_STALKER_LIMPAR:
      ajcCenaServico(op, "aj_tv-minimal-play"); return;
    default: break;
  }
  // As opcoes de trailer moram em Cartazes e trailers, mas a cena e a do trailer.
  if (sec == AJS_CARTAZES) switch (op) {
    case AJ_HERO_TRAILER: case AJ_FOCO_TRAILER: case AJ_DET_TRAILER_AUTO: case AJ_DET_TRAILER:
    case AJ_HERO_TRAILER_SOM: case AJ_DET_TRAILER_SOM: case AJ_TRAILER_FONTE: case AJ_HERO_TRAILER_ESPERA:
    case AJ_TRAILER_QUAL: case AJ_TRAILER_ASPECTO: case AJ_TRAILER_ZOOM_TPK: case AJ_LOGO_TRAILER:  case AJ_TRAILER_AUDIO:
      sec = AJS_TRAILERS; break;
    default: break;
  }
  switch (sec) {
    case AJS_HOME: ajcCenaHome(op); return;
    case AJS_TITULO: ajcCenaTitulo(op); return;
    case AJS_CARTAZES: ajcCenaCartazes(op); return;
    case AJS_TRAILERS: ajcCenaTrailer(op); return;
    case AJS_APARENCIA: ajcCenaIcone(op, sec); return;
    case AJS_REPRODUCAO: ajcCenaPlayer(op); return;
    case AJS_FONTES: ajcCenaFontes(op); return;
    case AJS_IDIOMAS: ajcCenaLegenda(op); return;
    case AJS_TVAOVIVO: ajcCenaAoVivo(op); return;
    case AJS_CONTAS: ajcCenaConta(op); return;
    case AJS_DESEMPENHO: ajcCenaDesempenho(op); return;
    default: ajcCenaIcone(op, sec); return;
  }
}
static void ajcCenaCategoria(int sec) {
  switch (sec) {
    case AJS_HOME: ajcCenaHome(-1); return;
    case AJS_TITULO: ajcCenaTitulo(-1); return;
    case AJS_CARTAZES: ajcCenaCartazes(AJ_PROF); return;
    case AJS_TRAILERS: ajcCenaTrailer(-1); return;
    case AJS_APARENCIA: ajcCenaCor(AJ_TEMA); return;
    case AJS_REPRODUCAO: ajcCenaQualidade(AJ_QUALIDADE); return;
    case AJS_FONTES: ajcCenaAddons(AJ_ADDONS); return;
    case AJS_IDIOMAS: ajcCenaLegenda(AJ_LEG_LINGUA); return;
    case AJS_TVAOVIVO: ajcCenaAoVivo(-1); return;
    case AJS_CONTAS: ajcCenaPerfis(-1); return;
    case AJS_DESEMPENHO: ajcCenaDesempenho(AJ_MEDIDOR); return;
    default: ajcCenaIcone(-1, sec); return;
  }
}
// A entrada: categoria (op < 0) ou opcao. Devolve a altura (w * 9/16).
static float ajCenaGerada(int sec, int op, float x, float y, float w) {
  float h = ajc_comecar(x, y, w, 1.0f);
  if (op >= 0) ajcCenaOpcao(sec >= 0 ? sec : uxSecaoDe(op), op);
  else ajcCenaCategoria(sec);
  ajc_fim();
  return h;
}
