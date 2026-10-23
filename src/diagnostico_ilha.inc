// DIAGNOSTICO, TESTE DE VELOCIDADE E APRESENTACAO NO GLASS UI (mockup de
// Ajustes, quadros "diagnostico*" e "velocidade", aprovado em 03/10). As
// telas viram ilhas sobre a arte, com as pecas de Ajustes (ajustes_ui_*). Os
// numeros sao os mesmos de antes; so o desenho mudou. Incluido em
// diagnostico.c antes de diagnostico_desenhar.

#define DG_TX 243, 242, 239, 255
#define DG_VERDE 0.298f, 0.765f, 0.541f
#define DG_AMBAR 0.910f, 0.722f, 0.290f
#define DG_VERM  0.898f, 0.325f, 0.294f
#ifdef AJUSTES_TESTE
static int dgMemFixa;
#endif
static float dgCapsCom(TxtEstilo e, const char *s, float track, float x, float y, int r, int g, int b, float a) {
  char up[200];
  idioma_maiusc_em(ajustes_idioma(), up, sizeof up, i18n(s));
  return txt_tracking(e, up, r, g, b, x, y, a, track);
}
static float ajCapsDg(const char *s, float x, float y) { return dgCapsCom(TXT_AJ_CAPS13, s, 1.82f, x, y, 243, 242, 239, 0.45f); }
static float ajCapsCor(const char *s, float x, float y, float r, float g, float b) {
  return dgCapsCom(TXT_MINI, s, 2.1f, x, y, (int)(r * 255), (int)(g * 255), (int)(b * 255), 1);
}
static float dgX0(void) { return 48.0f + ajustes_rail_largura_fixa(); }

static TxtLinha dgT(TxtEstilo e, const char *s) { return txt_linha(e, s, DG_TX); }
static TxtLinha dgTC(TxtEstilo e, const char *s, float w) { return txt_linha_corta(e, s, DG_TX, w); }
static float dgBloco(TxtEstilo e, const char *s, float x, float y, float w, float lh, float al, int n) {
  return txt_bloco(e, i18n(s), 243, 242, 239, x, y, w, lh, al, n);
}
static void dgIcone(const char *n, float x, float y, float l, float al) {
  gfx_icone((GfxRect){ x, y, l, l }, n, 0.953f, 0.949f, 0.937f, al);
}
// Titulo e sub da tela ao lado da ilha do relogio (o cabDiag do mockup).
static void dgCabecalho(const char *titulo, const char *sub) {
  float x = ajustes_ilha_x() + 202.0f;
  TxtLinha t = dgT(TXT_AJ_TIT28, titulo), s = dgTC(TXT_AJ_ESTADO, sub, NV_TELA_W - x - t.w - 60);
  txt_desenhar(t, x, 64 - t.h * 0.5f);
  txt_desenhar_alpha(s, x + t.w + 18, 64 - s.h * 0.5f, 0.62f);
}
// Kicker + titulo de ilha.
static float dgTitulo(const char *kick, const char *titulo, TxtEstilo e, float x, float y, float w) {
  ajustes_ui_kicker(kick, x, y, 1);
  y += 22.0f;
  return 22.0f + dgBloco(e, titulo, x, y, w, e == TXT_ILHA_TITULO ? 46.0f : 40.0f, 1, 2);
}
// Barra com o brilho que corre DENTRO do preenchido (pedido de 26/09).
static void dgBarra(GfxRect r, float pct, int animado, Uint32 agora) {
  float ar, ag, ab, w = r.w * limitePct(pct) / 100.0f;
  ajustes_acento(&ar, &ag, &ab);
  gfx_cor(r, 0.5f, 1, 1, 1, 0.08f);
  if (w > 1.0f) gfx_cor((GfxRect){ r.x, r.y, w, r.h }, 0.5f, ar, ag, ab, 1);
  if (animado && w > 24.0f && !ajustes_animacoes_reduzidas()) {
    float larg = w < 240.0f ? w * 0.35f : 84.0f;
    float x0 = r.x - larg + fmodf((float)agora * 0.24f, w + larg), x1 = x0 + larg;
    if (x0 < r.x) x0 = r.x;
    if (x1 > r.x + w) x1 = r.x + w;
    if (x1 - x0 > 1.0f) gfx_cor((GfxRect){ x0, r.y, x1 - x0, r.h }, 0.5f, 1, 1, 1, 0.22f);
  }
}
static void dgLinha(float x0, float y0, float x1, float y1, float esp, float r, float g, float b, float a) {
  float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy);
  int n, i;
  if (len < 1.0f) return;
  n = (int)ceilf(len / 40.0f);
  for (i = 0; i < n; i++) {
    float ax = x0 + dx * i / n, ay = y0 + dy * i / n, bx = x0 + dx * (i + 1) / n, by = y0 + dy * (i + 1) / n;
    float halo = 2.0f;
    GfxRect q = { fminf(ax, bx) - halo, fminf(ay, by) - halo, fabsf(bx - ax) + 2 * halo, fabsf(by - ay) + 2 * halo };
    gfx_rect(q, 0, GFX_LINHA, esp / q.h, ((bx - ax) * (by - ay) < 0.0f) ? 1.0f : 0.0f, 0, halo / q.h, r, g, b, a);
  }
}
// "Rotulo ...... valor" com fio de 7% (.stat do mockup).
static float dgStat(const char *k, const char *v, float x, float y, float w) {
  return ajustes_ui_meta(i18n(k), v, x, y, w) - 4.0f;
}
static void dgPonto(float x, float yc, float r, float g, float b) {
  gfx_cor((GfxRect){ x, yc - 4.5f, 9, 9 }, 0.5f, r, g, b, 1);
}

// --- APRESENTACAO (primeira abertura e o "Antes de comecar") ---------------------
#ifdef NV_TOUCH_UI
static TelefoneCartao dgIntroPhone;
static int dgIntroPhoneRolar(const PonteiroRolagem *e) {
  return introGlobal || d.intro ? toquerol_evento(&dgIntroPhone.rolagem, e) : 0;
}
static void dgIntroPhoneAcao(int i, int primeira) {
  if (primeira) {
    if (!introGlobal) return;
    if (!i) marcarApresentacaoVista();
    introGlobal = 0;
  } else {
    if (!d.intro || introGlobal || sairTela) return;
    if (!i) marcarApresentacaoVista();
    else sairTela = 1;
    d.intro = 0;
  }
}
static float dgIntroPhoneCorpo(float x, float y, float w, float a) {
  static const char *const IC[3] = {"aj_images", "aj_activity", "aj_circle-play"};
  static const char *const TX[3] = {"Artes, logos e fundos", "Tempos, bytes e falhas", "Fontes de vídeo"};
  float inicio = y;
  y += telefonecartao_titulo("Antes de começar", x, y, w, a);
  y += telefonecartao_texto(TXT_AJ_SUB, "O que será medido", x, y, w, 34, .7f * a) + 24;
  for (int i = 0; i < 3; i++) y += telefonecartao_item(IC[i], TX[i], NULL, x, y, w, a);
  y += telefonecartao_texto(TXT_AJ_TEXTO, "O app lê manifestos e catálogos, testa artes e fontes de vídeo e compara o caminho de carregamento desta TV.",
                            x, y, w, 30, .7f * a) + 24;
  y += telefonecartao_item("aj_triangle-alert", "Haverá testes de rede e consumo de dados. O relatório técnico será enviado automaticamente ao suporte.",
                           NULL, x, y, w, a);
  y += telefonecartao_texto(TXT_ILHA_GENERO, "Não altera assistidos, histórico, progresso ou scrobbling.", x, y, w, 30, .65f * a) + 12;
  return y - inicio;
}
static void dgApresentacaoPhone(int primeira) {
  float antes = gfx_escala();
  gfx_escala_sair(gfx_escala_ui());
  float W = nv_layout_w / gfx_escala(), H = nv_layout_h / gfx_escala();
  telefonecartao_medir(&dgIntroPhone, W, H, 2);
  GfxRect corpo = dgIntroPhone.corpo;
  float total = dgIntroPhoneCorpo(corpo.x, corpo.y, corpo.w, 0);
  telefonecartao_comecar(&dgIntroPhone, total, primeira, primeira ? introGlobal : d.intro,
                        dgIntroPhoneRolar, 1);
  dgIntroPhoneCorpo(corpo.x, corpo.y - dgIntroPhone.offset, corpo.w, 1);
  gfx_sem_recorte();
  telefonecartao_botao(&dgIntroPhone, 0, primeira ? "Continuar" : "Começar", 1, NULL, dgIntroPhoneAcao, primeira, 1);
  telefonecartao_botao(&dgIntroPhone, 1, primeira ? "Fechar" : "Cancelar", 0, NULL, dgIntroPhoneAcao, primeira, 1);
  gfx_escala_sair(antes);
}
#endif
static void dgApresentacao(int primeiraAbertura) {
#ifdef NV_TOUCH_UI
  if (telefoneui_ativo()) { dgApresentacaoPhone(primeiraAbertura); return; }
#endif
  GfxRect m = { 300, 0, 1320, 0 };
  float x, y, cw;
  static const char *const IC[3] = { "aj_images", "aj_activity", "aj_circle-play" };
  static const char *const TX[3] = { "Artes, logos e fundos", "Tempos, bytes e falhas", "Fontes de vídeo" };
  int i;
  m.h = 52 + 22 + 48 + 22 + 25 + 14 + 150 + 28 + 60 + 18 + 56 + 30 + 30 + 52;
  m.y = (NV_TELA_H - m.h) * 0.5f;
  ajustes_ui_veu();
  ajustes_ui_ilha(m, 36, 1);
  x = m.x + 56; y = m.y + 52;
  ajustes_ui_kicker("Diagnóstico e otimização", x, y, 1);
  y += 22;
  { TxtLinha t = dgT(TXT_ILHA_TITULO, "Antes de começar"); txt_desenhar(t, x, y); y += 48; }
  y += 22;
  { TxtLinha t = dgT(TXT_AJ_SUB, "O que será medido"); txt_desenhar_alpha(t, x, y, 0.55f); y += 25 + 14; }
  cw = (m.w - 112 - 32) / 3.0f;
  for (i = 0; i < 3; i++) {
    GfxRect c = { x + i * (cw + 16), y, cw, 150 };
    TxtLinha t = dgT(TXT_ILHA_ITEM, TX[i]);
    ajustes_ui_neutro(c, 24, 0.06f);
    ajustes_ui_neutro((GfxRect){ c.x + 24, c.y + 24, 52, 52 }, 16, 0.08f);
    dgIcone(IC[i], c.x + 24 + 13, c.y + 24 + 13, 26, 0.85f);
    txt_desenhar(t, c.x + 24, c.y + 150 - 24 - t.h);
  }
  y += 150 + 28;
  y += dgBloco(TXT_AJ_TEXTO, "O app lê manifestos e catálogos, testa artes e fontes de vídeo e compara o caminho de carregamento desta TV.",
               x, y, m.w - 112, 30, 0.62f, 2) + 18;
  y += ajustes_ui_ef("aj_triangle-alert", i18n("Haverá testes de rede e consumo de dados. O relatório técnico será enviado automaticamente ao suporte."),
                     x, y, m.w - 112);
  y += 30;
  { TxtLinha n = dgT(TXT_ILHA_GENERO, "Não altera assistidos, histórico, progresso ou scrobbling.");
    const char *k[2] = { "OK", "Voltar" };
    const char *l[2] = { primeiraAbertura ? "Continuar" : "Começar", primeiraAbertura ? "Fechar" : "Cancelar" };
    float dw = ajustes_ui_dicas(k, l, 2, 0, 0, 0);
    txt_desenhar_alpha(n, x, y + (30 - n.h) * 0.5f, 0.48f);
    ajustes_ui_dicas(k, l, 2, m.x + m.w - 56 - dw, y, 1); }
}

// --- ESTADO 0: OBJETIVO --------------------------------------------------------
static void dgObjetivo(void) {
  GfxRect e = { dgX0(), 112, 900, NV_TELA_H - 112 - 40 };
  GfxRect dr = { e.x + e.w + 24, 40, 0, NV_TELA_H - 80 };
  float x = e.x + 44, w = e.w - 88, y, ar, ag, ab;
  PtvPerfil atual, cand;
  int travado, i;
  long mem;
  static const char *const PASSOS[5] = {
    "Manifestos, catálogos e fontes de vídeo dos add-ons",
    "Cada fonte de arte: catálogo, Metahub, TMDB, Trakt, Apple TV, fanart.tv, anime e logo",
    "As mesmas artes pelo cache, com o perfil atual",
    "Aplica o candidato e mede as mesmas artes de novo",
    "Aplica só se ajudar; senão restaura o perfil anterior",
  };
  static const char *const PIC[5] = { "aj_puzzle", "aj_images", "aj_database", "aj_zap", "aj_rotate-ccw-clock" };
  dr.w = NV_TELA_W - 40 - dr.x;
  ajustes_acento(&ar, &ag, &ab);
  perfilAtual(&atual, &travado, &mem);
  ptv_candidato(ptv_plataforma(), mem, focoModo ? PTV_DESEMPENHO : PTV_QUALIDADE, travado, &cand);
  ajustes_ui_ilha(e, 36, 0);
  y = e.y + 42;
  y += dgTitulo("Desempenho desta TV", "Diagnóstico e otimização", TXT_ILHA_TITULO, x, y, w);
  y += 8 + dgBloco(TXT_AJ_SUB, "Teste os addons, artes e fontes desta TV; o relatório é enviado ao suporte.", x, y + 8, w, 26.6f, 0.55f, 2);
  y += 34;
  { TxtLinha t = dgT(TXT_AJ_SECAO, "Objetivo"), d2 = dgT(TXT_ILHA_GENERO, "Esquerda e direita escolhem o objetivo");
    txt_desenhar(t, x, y); txt_desenhar_alpha(d2, x + w - d2.w, y + t.h - d2.h - 2, 0.42f); y += t.h + 16; }
  for (i = 0; i < 2; i++) {
    float cw = (w - 16) * 0.5f;
    GfxRect c = { x + i * (cw + 16), y, cw, 126 };
    int sel = focoModo == i;
    ponteiro_alvo(c.x, c.y, c.w, c.h, ponteiroModo, ponteiroModo, i, 0);
    if (sel && !focoLinha) ajustes_ui_foco_linha(c, 26);
    else ajustes_ui_neutro(c, 26, sel ? 0.14f : 0.05f);
    { TxtLinha t = dgT(TXT_AJ_NOME, i ? "Desempenho" : "Qualidade");
      float tx = c.x + 26;
      if (sel) { gfx_cor((GfxRect){ tx, c.y + 26 + t.h * 0.5f - 4, 8, 8 }, 0.5f, ar, ag, ab, 1); tx += 18; }
      txt_desenhar_alpha(t, tx, c.y + 26, sel ? 1.0f : 0.8f);
      dgBloco(TXT_AJ_ESTADO, i ? "Fontes mais leves, menos antecipação e menor pressão de memória"
                               : "Nitidez na resolução exibida e preferências atuais preservadas",
              c.x + 26, c.y + 26 + t.h + 10, c.w - 52, 24.6f, 0.58f, 2); }
  }
  y += 126 + 32;
  { TxtLinha t = dgT(TXT_AJ_SECAO, "Candidato desta rodada"); txt_desenhar(t, x, y); y += t.h + 6; }
  y += ajustes_ui_antes_depois("Memória para imagens", atual.texMb, cand.texMb, "MB", 512, x, y, w, 0, -1, 0, 0);
  y += ajustes_ui_antes_depois("Fios de rede das artes", atual.fiosRede, cand.fiosRede, "fios", 6, x, y, w, 0, -1, 0, 0);
  y += ajustes_ui_antes_depois("Largura do fundo em tela cheia", atual.heroiLarg, cand.heroiLarg, "px", 2560, x, y, w, 0, -1, 0, 0);
  y += 14;
  y += dgBloco(TXT_ILHA_GENERO, "Se o reteste da mesma amostra piorar, o perfil anterior volta sozinho e a tela diz o motivo.", x, y, w, 24, 0.48f, 2);
  if (travado) {
    TxtLinha t = txt_linha_corta(TXT_ILHA_GENERO, i18n("A memória para imagens foi escolhida em Ajustes e não muda."), 232, 184, 74, 255, w);
    txt_desenhar(t, x, y + 8);
  }
  { float by = e.y + e.h - 42 - 18 - 16 - 60, bx = x;
    { float bw = ajustes_ui_botao(i18n("Iniciar diagnóstico"), "aj_activity", bx, by, !focoLinha);
      ponteiro_alvo(bx, by, bw, 60, ponteiroLinhaObj, NULL, 0, 0);
      bx += bw + 12; }
    { float bw = ajustes_ui_botao(i18n("Teste de velocidade"), "aj_gauge", bx, by, focoLinha);
      ponteiro_alvo(bx, by, bw, 60, ponteiroLinhaObj, NULL, 1, 0); }
    { TxtLinha t = dgTC(TXT_ILHA_APOIO, i18n("Até 3 títulos, 6 fontes e 12 imagens; limite de 8 minutos"), w);
      txt_desenhar_alpha(t, x, by + 60 + 16, 0.45f); } }
  // Como funciona
  ajustes_ui_ilha(dr, 36, 0);
  x = dr.x + 44; w = dr.w - 88; y = dr.y + 42;
  ajustes_ui_kicker("Como funciona", x, y, 1);
  { TxtLinha t = dgT(TXT_AJ_TIT28, "Cinco etapas, nesta ordem"); txt_desenhar(t, x, y + 22 + 6); y += 22 + 6 + t.h + 30; }
  gfx_cor((GfxRect){ x + 27, y + 30, 2, 4 * 104.0f }, 0, 1, 1, 1, 0.08f);
  for (i = 0; i < 5; i++) {
    float cy = y + 52 + i * 104.0f, h;
    char k[32];
    gfx_cor((GfxRect){ x, cy - 28, 56, 56 }, 0.5f, 0.106f, 0.110f, 0.129f, 1);
    gfx_anel((GfxRect){ x, cy - 28, 56, 56 }, 0.5f, 1, 1, 1, 1, 0.08f);
    dgIcone(PIC[i], x + 16, cy - 12, 24, 0.8f);
    snprintf(k, sizeof k, i18n("Etapa %d"), i + 1);
    h = txt_bloco(TXT_AJ_ITEM, i18n(PASSOS[i]), 243, 242, 239, x + 78, -4000, w - 78, 29.4f, 0, 2);
    ajCapsDg(k, x + 78, cy - (18 + 4 + h) * 0.5f);
    txt_bloco(TXT_AJ_ITEM, i18n(PASSOS[i]), 243, 242, 239, x + 78, cy - (18 + 4 + h) * 0.5f + 22, w - 78, 29.4f, 0.88f, 2);
  }
  { float h2 = 0, yb = dr.y + dr.h - 42;
    float a1 = txt_bloco(TXT_AJ_ESTADO, i18n("O relatório compacto é guardado localmente e só fica como enviado após confirmação do servidor."), 0, 0, 0, 0, -4000, w - 64, 24.65f, 0, 3);
    h2 = 28 + 24.65f;
    yb -= h2;
    ajustes_ui_ef("aj_eye", i18n("Não altera assistidos, histórico, progresso ou scrobbling."), x, yb, w);
    yb -= 10 + a1 + 28;
    ajustes_ui_ef("aj_file-text", i18n("O relatório compacto é guardado localmente e só fica como enviado após confirmação do servidor."), x, yb, w); }
}

// --- ESTADO 1: EM ANDAMENTO ----------------------------------------------------
static void dgAndamento(Uint32 agora) {
  static const char *const ETAPAS[6] = {
    "Sondando manifestos, catálogos e fontes", "Medindo cada fonte de arte", "Artes com o cache frio",
    "Artes com o perfil atual", "Artes com o perfil candidato", "Gravando e enviando o relatório",
  };
  int fase = atomic_load(&d.fase), feito = atomic_load(&d.feitos), total = atomic_load(&d.total), i;
  float pct, ar, ag, ab;
  GfxRect e = { dgX0(), 112, 1100, NV_TELA_H - 152 }, dr;
  float x = e.x + 48, w = e.w - 96, y;
  char v[96];
  ajustes_acento(&ar, &ag, &ab);
  if (fase < 1) fase = 1;
  if (fase > 6) fase = 6;
  pct = fase <= 2 ? (total > 0 ? 60.0f * (float)feito / (float)total : 4.0f) : 60.0f + (float)(fase - 2) * 9.0f;
  dgCabecalho(i18n("Diagnóstico e otimização"), i18n("Teste os addons, artes e fontes desta TV"));
  ajustes_ui_ilha(e, 36, 0);
  y = e.y + 44;
  { char k[160];
    snprintf(k, sizeof k, "%s · %s", i18n("Diagnóstico em andamento"), i18n(d.modo == DIAG_DESEMPENHO ? "Desempenho" : "Qualidade"));
    ajustes_ui_kicker(k, x, y, 1); }
  { TxtLinha t = dgT(TXT_AJ_INSP, ETAPAS[fase - 1]); txt_desenhar(t, x, y + 26); y += 26 + t.h + 22; }
  snprintf(v, sizeof v, "%d%%", (int)pct);
  { TxtLinha n = dgT(TXT_AJ_NUM110, v), s;
    char es[64];
    snprintf(es, sizeof es, i18n("etapa %d de %d"), fase, 6);
    s = dgT(TXT_AJ_SUB, es);
    txt_desenhar(n, x, y);
    txt_desenhar_alpha(s, x + n.w + 14, y + n.h * 0.78f - s.h, 0.5f);
    y += n.h * 0.9f + 24; }
  dgBarra((GfxRect){ x, y, w, 14 }, pct, 1, agora);
  y += 14 + 34;
  for (i = 0; i < 6; i++) {
    int feita = i + 1 < fase, at = i + 1 == fase;
    GfxRect r = { x, y + i * 64, w, 58 };
    TxtLinha t = txt_linha(at ? TXT_ILHA_ITEM : TXT_AJ_ITEM, ETAPAS[i], DG_TX);
    float cx = r.x + 18 + 15, cy = r.y + 29;
    if (at) ajustes_ui_neutro(r, 20, 0.08f);
    if (feita) { gfx_cor((GfxRect){ cx - 15, cy - 15, 30, 30 }, 0.5f, ar, ag, ab, 0.22f);
                 gfx_icone((GfxRect){ cx - 8, cy - 8, 16, 16 }, "aj_check", ar, ag, ab, 1); }
    else if (at) { float p = ajustes_animacoes_reduzidas() ? 1.0f : 0.55f + 0.45f * sinf((float)agora * 0.0052f);
                   gfx_cor((GfxRect){ cx - 12, cy - 12, 24, 24 }, 0.5f, ar, ag, ab, 0.22f * p);
                   gfx_cor((GfxRect){ cx - 6, cy - 6, 12, 12 }, 0.5f, ar, ag, ab, 1); }
    else { char n[4]; TxtLinha l; snprintf(n, sizeof n, "%d", i + 1); l = dgT(TXT_AJ_KBD, n);
           gfx_cor((GfxRect){ cx - 15, cy - 15, 30, 30 }, 0.5f, 1, 1, 1, 0.06f);
           txt_desenhar_alpha(l, cx - l.w * 0.5f, cy - l.h * 0.5f, 0.4f); }
    txt_desenhar_alpha(t, r.x + 66, cy - t.h * 0.5f, feita ? 0.62f : at ? 1.0f : 0.4f);
    if (at) { TxtLinha a = dgT(TXT_ILHA_GENERO, "agora"); txt_desenhar_alpha(a, r.x + r.w - 18 - a.w, cy - a.h * 0.5f, 0.5f); }
  }
  { const char *k[1] = { "Voltar" }, *l[1] = { "Cancelar" };
    float by = e.y + e.h - 44 - 30, dw = ajustes_ui_dicas(k, l, 1, 0, 0, 0);
    TxtLinha t = dgT(TXT_ILHA_GENERO, "Voltar cancela e interrompe as requisições da sessão.");
    txt_desenhar_alpha(t, x, by + (30 - t.h) * 0.5f, 0.48f);
    ajustes_ui_dicas(k, l, 1, x + w - dw, by, 1); }
  // AGORA NESTA TV
  dr = (GfxRect){ e.x + e.w + 24, 112, 0, NV_TELA_H - 152 };
  dr.w = NV_TELA_W - 40 - dr.x;
  ajustes_ui_ilha(dr, 36, 0);
  x = dr.x + 40; w = dr.w - 80; y = dr.y + 40;
  ajustes_ui_kicker("Agora nesta TV", x, y, 1);
  y += 18 + 14;
  { long bytes = 0, lim = tex_orcamento_bytes();
    char a[32], b[96];
    TxtLinha n, s;
    tex_estatisticas(NULL, NULL, &bytes, NULL, NULL);
#ifdef AJUSTES_TESTE
    if (dgMemFixa) { bytes = (long)(151.4 * 1048576); lim = 240L << 20; }
#endif
    snprintf(a, sizeof a, "%ld", bytes >> 20);
    snprintf(b, sizeof b, i18n("/ %ld MB de imagens"), lim >> 20);
    n = dgT(TXT_ILHA_TITULO, a); s = dgT(TXT_AJ_TEXTO, b);
    txt_desenhar(n, x, y);
    txt_desenhar_alpha(s, x + n.w + 10, y + n.h * 0.8f - s.h, 0.55f);
    y += n.h + 16;
    ajustes_ui_grafico_memoria(x, y, w, 150);
    y += 150 + 8;
    { TxtLinha l = dgT(TXT_AJ_MINI14, "há 2 min"), r2 = dgT(TXT_AJ_MINI14, "agora");
      txt_desenhar_alpha(l, x, y, 0.42f); txt_desenhar_alpha(r2, x + w - r2.w, y, 0.42f); y += 17 + 22; }
    snprintf(b, sizeof b, i18n("%d de %d"), feito, total);
    y += dgStat("Etapas de rede", b, x, y, w);
    snprintf(b, sizeof b, "%ld / %ld MB", bytes >> 20, lim >> 20);
    y += dgStat("Memória usada por imagens", b, x, y, w);
    { int fios = 0, fiosMax = 0;
      tex_threads_info(&fios, &fiosMax);
      snprintf(b, sizeof b, i18n("%d de %d"), fios, fiosMax);
      { TxtLinha vl = dgT(TXT_ILHA_GENERO, b); int k;
        float ar2, ag2, ab2;
        ajustes_acento(&ar2, &ag2, &ab2);
        for (k = 0; k < fiosMax && k < 12; k++) {
          float px = x + w - vl.w - 10 - (fiosMax - k) * 15.0f;
          if (k < fios) gfx_cor((GfxRect){ px, y + 13 + (vl.h - 10) * 0.5f, 10, 10 }, 0.3f, ar2, ag2, ab2, 1);
          else gfx_cor((GfxRect){ px, y + 13 + (vl.h - 10) * 0.5f, 10, 10 }, 0.3f, 1, 1, 1, 0.12f);
        } }
      y += dgStat("Fios em uso", b, x, y, w); }
    snprintf(b, sizeof b, "%d px", tex_teto_heroi());
    y += dgStat("Largura do fundo em tela cheia", b, x, y, w); }
  if (d.perfCand.texMb > 0) {
    y += 28;
    ajustes_ui_kicker("Candidato desta rodada", x, y, 1);
    y += 18 + 8;
    y += ajustes_ui_antes_depois("Memória para imagens", d.perfAntes.texMb, d.perfCand.texMb, "MB", 512, x, y, w, 0, -1, 0, 0);
    y += ajustes_ui_antes_depois("Fios de rede das artes", d.perfAntes.fiosRede, d.perfCand.fiosRede, "fios", 6, x, y, w, 0, -1, 0, 0);
    ajustes_ui_antes_depois("Largura do fundo em tela cheia", d.perfAntes.heroiLarg, d.perfCand.heroiLarg, "px", 2560, x, y, w, 0, -1, 0, 0);
  }
}

// --- ESTADO 2: RESULTADO --------------------------------------------------------
static void dgResultado(void) {
  float X = dgX0(), W = NV_TELA_W - 40 - X, cw = (W - 18) * 0.5f, ar, ag, ab;
  GfxRect tl = { X, 112, cw, 452 }, tr = { X + cw + 18, 112, cw, 452 };
  GfxRect bl = { X, 112 + 452 + 18, cw, NV_TELA_H - 40 - 60 - 18 - (112 + 452 + 18) }, br = { tr.x, bl.y, cw, bl.h };
  int cobertura = coberturaPercentual(), cor, i;
  char v[200];
  float x, y, w;
  ajustes_acento(&ar, &ag, &ab);
  dgCabecalho(i18n("Diagnóstico e otimização"), i18n(d.coberturaParcial ? "Cobertura parcial da amostra" : "Amostra concluída"));
  // Resultado geral
  ajustes_ui_ilha(tl, 32, 0);
  x = tl.x + 34; w = tl.w - 68; y = tl.y + 30;
  ajustes_ui_kicker("Resultado geral", x, y, 1);
  y += 28;
  snprintf(v, sizeof v, "%d%%", cobertura);
  { TxtLinha n = dgT(TXT_AJ_NUM64, v), s = dgT(TXT_AJ_SUB, "cobertura da amostra");
    txt_desenhar(n, x, y); txt_desenhar_alpha(s, x + n.w + 12, y + n.h * 0.8f - s.h, 0.5f); y += n.h * 0.92f + 18; }
  dgBarra((GfxRect){ x, y, w, 10 }, (float)cobertura, 0, 0);
  y += 10 + 20;
  { TxtLinha k = dgT(TXT_AJ_ESTADO, "Gargalo principal"), g = txt_linha(TXT_AJ_ESTADO, gargaloPrincipal(), (int)(0.910f * 255), (int)(0.722f * 255), (int)(0.290f * 255), 255);
    gfx_cor((GfxRect){ x, y, w, 1 }, 0, 1, 1, 1, 0.07f);
    txt_desenhar_alpha(k, x, y + 13, 0.45f); txt_desenhar(g, x + w - g.w, y + 13); y += 42.5f; }
  { int ok = d.imagensOk, fal = d.imagensFalhas, k;
    for (k = 1; k < PTV_N_FONTES; k++) { ok += d.fonte[k].ok; fal += d.fonte[k].falhas; }
    snprintf(v, sizeof v, i18n("%d ok · %d falhas"), ok, fal);
    y += dgStat("Artes", v, x, y, w); }
  { const char *rel = atomic_load(&d.enviando) ? "enviando…" : d.envioFalhou ? "guardado; envio falhou"
                    : d.enviado ? "enviado ao suporte" : "guardado localmente";
    TxtLinha k = dgT(TXT_AJ_ESTADO, "Relatório");
    TxtLinha r = d.envioFalhou ? txt_linha(TXT_AJ_ESTADO, rel, 229, 83, 75, 255) : d.enviado ? txt_linha(TXT_AJ_ESTADO, rel, 76, 195, 138, 255) : dgT(TXT_AJ_ESTADO, rel);
    gfx_cor((GfxRect){ x, y, w, 1 }, 0, 1, 1, 1, 0.07f);
    txt_desenhar_alpha(k, x, y + 13, 0.45f);
    txt_desenhar(r, x + w - r.w, y + 13);
    if (d.envioFalhou) dgPonto(x + w - r.w - 17, y + 13 + r.h * 0.5f, DG_VERM);
    else if (d.enviado) dgPonto(x + w - r.w - 17, y + 13 + r.h * 0.5f, DG_VERDE); }
  // Artes por fonte
  ajustes_ui_ilha(tr, 32, 0);
  x = tr.x + 34; w = tr.w - 68; y = tr.y + 30;
  ajustes_ui_kicker("Artes por fonte", x, y, 1);
  { TxtLinha s = dgT(TXT_ILHA_APOIO, "Tempo médio por arte: consulta e download"); txt_desenhar_alpha(s, x, y + 24, 0.45f); }
  y += 24 + 20 + 16;
  { int f, maior = 1, lenta = 0, msLenta = -1, linhas = 0;
    float passo;
    for (f = 1; f < PTV_N_FONTES; f++) {
      int ms = ptv_fonte_ms(&d.fonte[f]);
      if (ms > maior) maior = ms;
      if (ms > msLenta) { msLenta = ms; lenta = f; }
      if (d.fonte[f].ok || d.fonte[f].falhas) linhas++;
    }
    passo = linhas > 8 ? 248.0f / linhas : 31.0f;
    for (f = 1; f < PTV_N_FONTES; f++) {
      const PtvFonte *s = &d.fonte[f];
      int ms = ptv_fonte_ms(s), dest = f == lenta && ms > 0;
      TxtLinha n, t;
      if (!s->ok && !s->falhas && linhas > 0) continue;
      n = dest ? txt_linha(TXT_ILHA_GENERO, ptv_fonte_rotulo(f), 232, 184, 74, 255) : dgTC(TXT_ILHA_GENERO, ptv_fonte_rotulo(f), 160);
      txt_desenhar_alpha(n, x, y + (passo - n.h) * 0.5f, dest ? 1.0f : 0.75f);
      { float bx = x + 176, bw = w - 176 - 316;
        gfx_cor((GfxRect){ bx, y + passo * 0.5f - 4, bw, 8 }, 0.5f, 1, 1, 1, 0.07f);
        if (ms > 0) {
          if (dest) gfx_cor((GfxRect){ bx, y + passo * 0.5f - 4, bw * ms / maior, 8 }, 0.5f, DG_AMBAR, 1);
          else gfx_cor((GfxRect){ bx, y + passo * 0.5f - 4, bw * ms / maior, 8 }, 0.5f, 0.953f, 0.949f, 0.937f, 0.5f);
        } }
      if (!s->ok && !s->falhas) snprintf(v, sizeof v, "%s", i18n("não medida"));
      else if (ms < 0) snprintf(v, sizeof v, "%s", i18n("falhou"));
      else if (s->iguais > 0 && f <= PTV_FONTE_FUNDO_MAX) snprintf(v, sizeof v, i18n("%d ms · igual ao card"), ms);
      else if (s->largura > 0) snprintf(v, sizeof v, i18n("%d ms · %d×%d · %d falhas"), ms, s->largura, s->altura, s->falhas);
      else snprintf(v, sizeof v, i18n("%d ms · %d falhas"), ms, s->falhas);
      t = txt_linha_corta(TXT_ILHA_APOIO, v, DG_TX, 300);
      txt_desenhar_alpha(t, x + w - t.w, y + (passo - t.h) * 0.5f, 0.6f);
      y += passo;
    } }
  y += 14;
  gfx_cor((GfxRect){ x, y, w, 1 }, 0, 1, 1, 1, 0.07f);
  y += 14;
  { char a[220], b[220];
    a[0] = b[0] = 0;
    if (d.sugEstado == DS_PROPOSTA || d.sugEstado == DS_TESTANDO) {
      if (d.sug.motivo == PTV_MOTIVO_IGUAL && d.sug.alvo > 0)
        snprintf(a, sizeof a, i18n("Destaque em %s repete a imagem do card; %s é outra arte, %d ms."),
                 i18n(ptv_fonte_rotulo(d.sug.lenta)), i18n(ptv_fonte_rotulo(d.sug.alvo)), ptv_fonte_ms(&d.fonte[d.sug.alvo]));
      else snprintf(a, sizeof a, i18n("Destaque em %s: %d ms por arte; %s: %d ms."),
                    i18n(ptv_fonte_rotulo(d.sug.lenta)), d.sug.msLenta, i18n(ptv_fonte_rotulo(d.sug.base)), d.sug.msBase);
      if (!d.sug.diferente) snprintf(b, sizeof b, "%s", i18n("Vai mudar: Destaque com outra arte, de Ligado para Desligado."));
      else snprintf(b, sizeof b, i18n("Vai mudar: Background do hero, de %s para %s."),
                    i18n(nomeFonteAjuste(ajustes_hero_fonte())), i18n(nomeFonteAjuste(d.sug.fonte)));
      if (d.sugEstado == DS_TESTANDO) snprintf(b, sizeof b, "%s", i18n("Retestando a arte do destaque…"));
      { TxtLinha l = txt_linha_corta(TXT_ILHA_APOIO, a, 232, 184, 74, 255, w); txt_desenhar(l, x, y); }
      { TxtLinha l = dgTC(TXT_ILHA_APOIO, b, w); txt_desenhar_alpha(l, x, y + 24, 0.75f); }
    } else {
      const char *s = d.sugEstado == DS_MANTIDA ? "Sugestão aplicada: o destaque ficou mais rápido no reteste."
                    : d.sugEstado == DS_DESFEITA ? NULL : "A fonte do destaque não está mais lenta que a do card.";
      if (!s) { snprintf(a, sizeof a, i18n("Sugestão desfeita sozinha: %s"), i18n(d.sugMotivo ? d.sugMotivo : "")); s = a; }
      { TxtLinha l = dgTC(TXT_ILHA_APOIO, s, w); txt_desenhar_alpha(l, x, y, 0.55f); }
    } }
  // Tempo por etapa
  ajustes_ui_ilha(bl, 32, 0);
  x = bl.x + 34; w = bl.w - 68; y = bl.y + 30;
  ajustes_ui_kicker("Tempo por etapa", x, y, 1);
  { TxtLinha s = dgTC(TXT_ILHA_APOIO, i18n("Maior tempo agregado da amostra; falha isolada não derruba o addon."), w); txt_desenhar_alpha(s, x, y + 24, 0.45f); }
  y += 24 + 20 + 18;
  { static const char *const ROT[4] = { "Manifestos", "Catálogos", "Artes", "Fontes" };
    int t[4], maior = 1;
    float bw = (w - 3 * 22) / 4.0f, H = bl.h - 30 - 62 - 50 - 2 * 42.5f - 20;
    t[0] = d.manifestMs; t[1] = d.catalogMs; t[2] = d.assetsMs + d.fontesMs; t[3] = d.streamMs;
    for (i = 0; i < 4; i++) if (t[i] > maior) maior = t[i];
    for (i = 0; i < 4; i++) {
      float bx = x + i * (bw + 22), h = H * t[i] / maior;
      int m = t[i] == maior;
      char s[32];
      TxtLinha l, r;
      if (h < 6) h = 6;
      snprintf(s, sizeof s, "%.1f s", t[i] / 1000.0);
      if (!idioma_ponto_decimal(ajustes_idioma())) { char *p = strchr(s, '.'); if (p) *p = ','; }
      l = m ? txt_linha(TXT_AJ_CHIP, s, 232, 184, 74, 255) : dgT(TXT_AJ_CHIP, s);
      r = dgT(TXT_ILHA_APOIO, ROT[i]);
      if (m) gfx_cor((GfxRect){ bx, y + 26 + H - h, bw, h }, 12.0f / (h > 24 ? h : 24), DG_AMBAR, 0.85f);
      else gfx_cor((GfxRect){ bx, y + 26 + H - h, bw, h }, 12.0f / (h > 24 ? h : 24), 0.953f, 0.949f, 0.937f, 0.22f);
      txt_desenhar_alpha(l, bx + (bw - l.w) * 0.5f, y + 26 + H - h - 8 - l.h, m ? 1.0f : 0.6f);
      txt_desenhar_alpha(r, bx + (bw - r.w) * 0.5f, y + 26 + H + 8, 0.55f);
    }
    y += 26 + H + 8 + 20 + 12; }
  { long bytes = 0, lim = tex_orcamento_bytes(); int pend = 0, quentes = 0;
    tex_estatisticas(NULL, &pend, &bytes, &quentes, NULL);
    snprintf(v, sizeof v, "%ld / %ld MB", bytes >> 20, lim > 0 ? lim >> 20 : 0L);
    y += dgStat("Memória usada por imagens", v, x, y, w);
    snprintf(v, sizeof v, i18n("%d pendentes · %d quentes"), pend, quentes);
    dgStat("Fila de imagens", v, x, y, w); }
  // O que foi aplicado
  ajustes_ui_ilha(br, 32, 0);
  x = br.x + 34; w = br.w - 68; y = br.y + 30;
  ajustes_ui_kicker("Agora nesta TV", x, y, 1);
  { TxtLinha m = dgT(TXT_ILHA_APOIO, d.modo == DIAG_DESEMPENHO ? "Desempenho" : "Qualidade");
    txt_desenhar_alpha(m, x + w - m.w, y, 0.5f); }
  y += 18 + 12;
  { const char *txtAp = textoAplicacao(&cor);
    TxtLinha t = cor == 1 ? txt_linha_corta(TXT_ILHA_NOME, txtAp, 76, 195, 138, 255, w - 20)
               : cor == 2 ? txt_linha_corta(TXT_ILHA_NOME, txtAp, 232, 184, 74, 255, w - 20) : dgTC(TXT_ILHA_NOME, txtAp, w - 20);
    if (cor == 1) dgPonto(x, y + t.h * 0.5f, DG_VERDE);
    else if (cor == 2) dgPonto(x, y + t.h * 0.5f, DG_AMBAR);
    txt_desenhar(t, x + (cor ? 17 : 0), y);
    y += t.h + 6;
    if (d.motivo) { TxtLinha m = dgTC(TXT_ILHA_APOIO, d.motivo, w - 17); txt_desenhar_alpha(m, x + 17, y, 0.55f); y += 22; } }
  y += 8;
  { int pior = d.aplicacao == DA_RESTAURADO_AUTO;
    PtvPerfil a, b;
    float hw = (w - 32) * 0.5f, yy = y;
    perfilDoResultado(&a, &b);
    yy += ajustes_ui_antes_depois("Memória para imagens agora", a.texMb, b.texMb, "MB", 512, x, yy, hw, 1, pior ? 0.910f : -1, 0.722f, 0.290f);
    yy += ajustes_ui_antes_depois("Fios de rede das artes", a.fiosRede, b.fiosRede, "fios", 6, x, yy, hw, 1, pior ? 0.910f : -1, 0.722f, 0.290f);
    ajustes_ui_antes_depois("Largura do fundo em tela cheia", a.heroiLarg, b.heroiLarg, "px", 2560, x, yy, hw, 1, pior ? 0.910f : -1, 0.722f, 0.290f);
    if (d.medAntes.artesMs || d.medDepois.artesMs) {
      int V[3] = { d.medFrio.artesMs, d.medAntes.artesMs, d.medDepois.artesMs }, mx = 1, k;
      static const char *const N[3] = { "Cache frio", "Antes", "Depois" };
      float cx = x + hw + 32, cy = y + 10;
      ajCapsDg("Artes, mesma amostra", cx, cy);
      cy += 22;
      for (k = 0; k < 3; k++) if (V[k] > mx) mx = V[k];
      for (k = 0; k < 3; k++) {
        TxtLinha n = dgT(TXT_ILHA_APOIO, N[k]), t;
        char ms[32];
        float bx = cx + 92, bw = hw - 92 - 96;
        snprintf(ms, sizeof ms, "%d ms", V[k]);
        t = dgT(TXT_ILHA_APOIO, ms);
        txt_desenhar_alpha(n, cx, cy + (32 - n.h) * 0.5f, 0.55f);
        gfx_cor((GfxRect){ bx, cy + 11, bw, 10 }, 0.5f, 1, 1, 1, 0.06f);
        if (k == 2) { if (pior) gfx_cor((GfxRect){ bx, cy + 11, bw * V[k] / mx, 10 }, 0.5f, DG_AMBAR, 1);
                      else gfx_cor((GfxRect){ bx, cy + 11, bw * V[k] / mx, 10 }, 0.5f, DG_VERDE, 1); }
        else gfx_cor((GfxRect){ bx, cy + 11, bw * V[k] / mx, 10 }, 0.5f, 0.953f, 0.949f, 0.937f, k ? 0.5f : 0.25f);
        txt_desenhar_alpha(t, cx + hw - t.w, cy + (32 - t.h) * 0.5f, 0.75f);
        cy += 32;
      }
    } }
  // Botoes
  { int lista[B_N], n = botoesVisiveis(lista);
    float bx = X, by = NV_TELA_H - 40 - 60;
    if (d.botao >= n) d.botao = n > 0 ? n - 1 : 0;
    for (i = 0; i < n; i++) {
      float bw = ajustes_ui_botao(i18n(BOTAO_ROTULO[lista[i]]), NULL, bx, by, i == d.botao);
      if (d.sugEstado != DS_TESTANDO) ponteiro_alvo(bx, by, bw, 60, ponteiroBotaoRes, NULL, i, 0);
      bx += bw + 12;
    }
    { const char *k[3] = { "← →", "OK", "Voltar" }, *l[3] = { "Escolher", "Confirmar", "Sair" };
      float dw = ajustes_ui_dicas(k, l, 3, 0, 0, 0);
      ajustes_ui_dicas(k, l, 3, NV_TELA_W - 40 - dw, by + 15, 1); } }
}

// --- TESTE DE VELOCIDADE ----------------------------------------------------------
static void dgVazaoResultado(Uint32 agora) {
  GfxRect e = { dgX0(), 112, 900, NV_TELA_H - 152 }, dr;
  float x = e.x + 44, w = e.w - 88, y, ar, ag, ab;
  char a[200], b[32], c[32];
  int nf = atomic_load(&vz.nFonte), i;
  (void)agora;
  ajustes_acento(&ar, &ag, &ab);
  ajustes_ui_ilha(e, 36, 0);
  y = e.y + 44;
  snprintf(a, sizeof a, "%s · %s", i18n("Teste de velocidade"), i18n("Desempenho desta TV"));
  y += dgTitulo(a, "Tamanho de arquivo que toca sem parar nesta TV", TXT_ILHA_TITULO, x, y, w);
  y += 30;
  vazao_fmt_mbps(b, sizeof b, vz.resumo.medianaKbps, sepDecimal());
  vazao_fmt_mbps(c, sizeof c, vz.resumo.p20Kbps, sepDecimal());
  { TxtLinha n = dgT(TXT_AJ_NUM110, b), u = dgT(TXT_AJ_TIT28, "Mbps"), p;
    snprintf(a, sizeof a, i18n("pior trecho %s Mbps"), c);
    p = dgT(TXT_AJ_TEXTO, a);
    txt_desenhar(n, x, y);
    txt_desenhar_alpha(u, x + n.w + 14, y + n.h * 0.8f - u.h, 0.6f);
    txt_desenhar_alpha(p, x + n.w + 14 + u.w + 18, y + n.h * 0.8f - p.h, 0.5f);
    y += n.h * 0.9f + 30; }
  { float cw = (w - 16) * 0.5f;
    int k;
    for (k = 0; k < 2; k++) {
      int kb = k ? vz.resumo.maximoKbps : vz.resumo.otimoKbps;
      GfxRect cr = { x + k * (cw + 16), y, cw, 200 };
      char gb[16], ep[16], mb[16];
      ajustes_ui_neutro(cr, 16, 0.04f);
      if (k) ajustes_ui_kicker("Máximo", cr.x + 24, cr.y + 24, 1);
      else ajCapsCor("Ótimo", cr.x + 24, cr.y + 24, ar, ag, ab);
      vazao_fmt_gb(gb, sizeof gb, vazao_gb(kb, VAZAO_FILME_S), sepDecimal());
      vazao_fmt_gb(ep, sizeof ep, vazao_gb(kb, VAZAO_EPISODIO_S), sepDecimal());
      snprintf(a, sizeof a, "%s GB", gb);
      { TxtLinha n = dgT(TXT_ILHA_TITULO, a); txt_desenhar(n, cr.x + 24, cr.y + 24 + 26); }
      snprintf(a, sizeof a, i18n("por filme de 2 h · %s GB por episódio"), ep);
      { TxtLinha l = dgTC(TXT_AJ_ESTADO, a, cw - 48); txt_desenhar_alpha(l, cr.x + 24, cr.y + 24 + 26 + 50, 0.6f); }
      vazao_fmt_mbps(mb, sizeof mb, kb, sepDecimal());
      snprintf(a, sizeof a, i18n(k ? "Até %s Mbps toca, mas pode pausar quando a rede cai."
                                   : "Arquivos de até %s Mbps tocam sem parar, mesmo nos piores trechos."), mb);
      txt_bloco(TXT_ILHA_GENERO, a, 243, 242, 239, cr.x + 24, cr.y + 24 + 26 + 50 + 34, cw - 48, 23, 0.45f, 2);
    }
    y += 200 + 34; }
  ajustes_ui_kicker("Filme de 2 h · tamanho do arquivo", x, y, 1);
  y += 18 + 16;
  { double go = vazao_gb(vz.resumo.otimoKbps, VAZAO_FILME_S), gm = vazao_gb(vz.resumo.maximoKbps, VAZAO_FILME_S);
    double esc = 40.0;
    char so[16], sm[16], se[16];
    TxtLinha l;
    while (gm > esc * 0.9) esc *= 2;
    gfx_cor((GfxRect){ x, y, w, 14 }, 0.5f, 1, 1, 1, 0.07f);
    gfx_recorte(x, y, w, 14);
    gfx_cor((GfxRect){ x, y, (float)(w * gm / esc) + 7, 14 }, 0.5f, DG_AMBAR, 0.75f);
    gfx_cor((GfxRect){ x, y, (float)(w * go / esc), 14 }, 0.5f, DG_VERDE, 0.75f);
    gfx_sem_recorte();
    vazao_fmt_gb(so, sizeof so, go, sepDecimal()); vazao_fmt_gb(sm, sizeof sm, gm, sepDecimal());
    snprintf(se, sizeof se, "%.0f GB", esc);
    y += 14 + 10;
    l = dgT(TXT_ILHA_HORA, "0"); txt_desenhar_alpha(l, x, y, 0.45f);
    l = dgT(TXT_ILHA_HORA, se); txt_desenhar_alpha(l, x + w - l.w, y, 0.45f);
    l = txt_linha(TXT_AJ_CHIP, so, 76, 195, 138, 255); txt_desenhar(l, x + (float)(w * go / esc) - l.w * 0.5f, y);
    l = txt_linha(TXT_AJ_CHIP, sm, 232, 184, 74, 255); txt_desenhar(l, x + (float)(w * gm / esc) - l.w * 0.5f, y);
    y += 44; }
  { static const char *const L[3] = { "toca sem parar", "pode pausar", "acima do máximo" };
    float lx = x;
    for (i = 0; i < 3; i++) {
      TxtLinha l = dgT(TXT_ILHA_HORA, L[i]);
      if (i == 0) gfx_cor((GfxRect){ lx, y + (l.h - 10) * 0.5f, 10, 10 }, 0.3f, DG_VERDE, 1);
      else if (i == 1) gfx_cor((GfxRect){ lx, y + (l.h - 10) * 0.5f, 10, 10 }, 0.3f, DG_AMBAR, 1);
      else gfx_cor((GfxRect){ lx, y + (l.h - 10) * 0.5f, 10, 10 }, 0.3f, 1, 1, 1, 0.12f);
      txt_desenhar_alpha(l, lx + 18, y, 0.5f);
      lx += 18 + l.w + 22;
    }
    y += 18 + 22; }
  dgBloco(TXT_ILHA_APOIO, "Filme de 2 h e episódio de 45 min; o tamanho do arquivo aparece no nome da fonte.", x, y, w, 22, 0.42f, 2);
  { int lista[VB_N], n = vazBotoes(lista);
    float bx = x, by = e.y + e.h - 40 - 60;
    if (vz.botao >= n) vz.botao = n - 1;
    for (i = 0; i < n; i++) {
      float bw = ajustes_ui_botao(i18n(VB_ROTULO[lista[i]]), i == 0 ? "aj_rotate-cw" : NULL, bx, by, i == vz.botao);
      ponteiro_alvo(bx, by, bw, 60, ponteiroBotaoVaz, NULL, i, 0);
      bx += bw + 12;
    }
    snprintf(a, sizeof a, i18n("%d de %d tentadas · %d segundos"), nf, vz.tentadas, vz.nAmostra);
    { TxtLinha t = dgTC(TXT_ILHA_APOIO, a, x + w - bx - 10); txt_desenhar_alpha(t, x + w - t.w, by + (60 - t.h) * 0.5f, 0.45f); } }
  // VAZAO POR FONTE
  dr = (GfxRect){ e.x + e.w + 24, 40, 0, NV_TELA_H - 80 };
  dr.w = NV_TELA_W - 40 - dr.x;
  ajustes_ui_ilha(dr, 36, 0);
  x = dr.x + 40; w = dr.w - 80; y = dr.y + 40;
  ajustes_ui_kicker("Vazão por fonte", x, y, 1);
  y += 26;
  y += dgBloco(TXT_AJ_SUB, "Baixa por 8 s até 3 fontes de servidores diferentes, começando no meio do arquivo; os dados são descartados.",
               x, y, w, 27.5f, 0.62f, 2) + 26;
  { float H = 300, mx = 10, p20 = vz.resumo.p20Kbps / 1000.0f;
    int f, s, nseg = 0, linhas[4] = { 10, 20, 30, 40 };
    static const float COR[3][4] = { { -1, 0, 0, 1 }, { 0.953f, 0.949f, 0.937f, 0.75f }, { 0.953f, 0.949f, 0.937f, 0.4f } };
    for (f = 0; f < nf && f < 3; f++) for (s = 0; s < vz.fonte[f].n; s++) {
      if (vz.fonte[f].kbps[s] / 1000.0f > mx) mx = vz.fonte[f].kbps[s] / 1000.0f;
      if (vz.fonte[f].n > nseg) nseg = vz.fonte[f].n; }
    mx = ceilf(mx * 1.15f / 10.0f) * 10.0f;
    if (nseg < 2) nseg = 2;
    for (i = 0; i < 4; i++) if (linhas[i] < mx) {
      float ly = y + H - H * linhas[i] / mx;
      char t[8]; TxtLinha l;
      snprintf(t, sizeof t, "%d", linhas[i]);
      gfx_cor((GfxRect){ x, ly, w, 1 }, 0, 1, 1, 1, 0.07f);
      l = dgT(TXT_AJ_MINI14, t); txt_desenhar_alpha(l, x, ly - 6 - l.h, 0.35f);
    }
    if (p20 > 0) {
      float ly = y + H - H * p20 / mx, dx;
      TxtLinha l;
      snprintf(a, sizeof a, i18n("pior trecho %s Mbps"), c);
      l = txt_linha(TXT_AJ_MINI12, a, 245, 200, 76, 255);
      for (dx = 0; dx < w; dx += 11) gfx_cor((GfxRect){ x + dx, ly, 5, 1 }, 0, 0.961f, 0.784f, 0.298f, 0.7f);
      txt_desenhar(l, x + w - l.w, ly - 8 - l.h);
    }
    for (f = nf < 3 ? nf - 1 : 2; f >= 0; f--) {
      const VazFonte *vf = &vz.fonte[f];
      float px = x, py = y + H, cr = COR[f][0], cg = COR[f][1], cb = COR[f][2], ca = COR[f][3];
      if (cr < 0) { cr = ar; cg = ag; cb = ab; }
      for (s = 0; s < vf->n; s++) {
        float nx = x + w * (s + 1) / (float)nseg, ny = y + H - H * (vf->kbps[s] / 1000.0f) / mx;
        dgLinha(px, py, nx, ny, f == 0 ? 1.75f : 1.25f, cr, cg, cb, ca);
        px = nx; py = ny;
      }
    }
    for (i = 0; i <= 4; i++) {
      char t[8]; TxtLinha l; float tx = x + w * i / 4.0f;
      snprintf(t, sizeof t, "%d s", nseg * i / 4);
      l = dgT(TXT_AJ_MINI14, t);
      txt_desenhar_alpha(l, i == 0 ? tx : i == 4 ? tx - l.w : tx - l.w * 0.5f, y + H + 6, 0.38f);
    }
    y += H + 26 + 14;
    { float lx = x;
      for (f = 0; f < nf && f < 3; f++) {
        char mb[16]; TxtLinha l;
        float cr = COR[f][0], cg = COR[f][1], cb = COR[f][2], ca = COR[f][3];
        if (cr < 0) { cr = ar; cg = ag; cb = ab; ca = 1; }
        vazao_fmt_mbps(mb, sizeof mb, vz.fonte[f].r.medianaKbps, sepDecimal());
        snprintf(a, sizeof a, i18n("Fonte %d · %s Mbps"), f + 1, mb);
        l = dgT(TXT_ILHA_HORA, a);
        gfx_cor((GfxRect){ lx, y + (l.h - 10) * 0.5f, 10, 10 }, 0.3f, cr, cg, cb, ca);
        txt_desenhar_alpha(l, lx + 17, y, 0.5f);
        lx += 17 + l.w + 18;
      } }
    y += 18 + 34; }
  { TxtLinha t = dgT(TXT_AJ_SECAO, "Add-ons e fontes");
    txt_desenhar(t, x, y);
    ajCapsDg("Resposta de cada add-on", x + t.w + 12, y + t.h - 16);
    y += t.h + 10;
    gfx_cor((GfxRect){ x, y, w, 1 }, 0, 1, 1, 1, 0.08f);
    y += 6; }
  { int maiorMs = 1, linha = 0;
    for (i = 0; i < vz.nAddon; i++) if (vz.addon[i].medido && vz.addon[i].ms > maiorMs) maiorMs = vz.addon[i].ms;
    for (i = 0; i < vz.nAddon && linha < 5 && y + 52 < dr.y + dr.h - 30; i++) {
      const VazAddon *va = &vz.addon[i];
      char val[96];
      float fr = (float)va->ms / maiorMs;
      TxtLinha n, t;
      if (!va->medido) continue;
      linha++;
      n = dgTC(TXT_ILHA_ITEM, addons_nome(i), 170);
      txt_desenhar(n, x, y + 14 + (24 - n.h) * 0.5f);
      { float bx = x + 188, bw = w - 188 - 218;
        gfx_cor((GfxRect){ bx, y + 22, bw, 8 }, 0.5f, 1, 1, 1, 0.07f);
        if (va->ok) {
          if (fr < 0.4f) gfx_cor((GfxRect){ bx, y + 22, bw * fr, 8 }, 0.5f, DG_VERDE, 0.85f);
          else if (fr < 0.8f) gfx_cor((GfxRect){ bx, y + 22, bw * fr, 8 }, 0.5f, DG_AMBAR, 0.85f);
          else gfx_cor((GfxRect){ bx, y + 22, bw * fr, 8 }, 0.5f, DG_VERM, 0.85f);
        } }
      if (va->ok) snprintf(val, sizeof val, i18n("%d ms · %d fontes"), va->ms, va->fontes);
      else if (va->http) snprintf(val, sizeof val, i18n("HTTP %d"), va->http);
      else snprintf(val, sizeof val, "%s", i18n("falhou"));
      t = dgTC(TXT_ILHA_GENERO, val, 200);
      txt_desenhar_alpha(t, x + w - t.w, y + 14 + (24 - t.h) * 0.5f, 0.6f);
      y += 52;
      gfx_cor((GfxRect){ x, y - 1, w, 1 }, 0, 1, 1, 1, 0.06f);
    } }
}

// O resto do teste de velocidade (rodando, sem medida, ciclo completo): os
// paineis de sempre, dentro das ilhas.
static void dgVazaoGenerico(Uint32 agora) {
  GfxRect e = { dgX0(), 112, 900, NV_TELA_H - 152 }, dr = { 0, 112, 0, NV_TELA_H - 152 };
  float ar, ag, ab;
  int ve = atomic_load(&vz.estado);
  ajustes_acento(&ar, &ag, &ab);
  dr.x = e.x + e.w + 24; dr.w = NV_TELA_W - 40 - dr.x;
  e.h -= 84;
  dgCabecalho(i18n("Teste de velocidade"), i18n("Desempenho desta TV"));
  ajustes_ui_ilha(e, 36, 0);
  ajustes_ui_ilha(dr, 36, 0);
  desenharVazaoResumo(e, ar, ag, ab, agora);
  if (vz.ciclo != VCM_RAPIDO) desenharCicloRanking(dr, ar, ag, ab);
  else desenharVazaoOrigem(dr, ar, ag, ab);
  if (ve != 1) {
    int lista[VB_N], n = vazBotoes(lista), i;
    float bx = e.x, by = NV_TELA_H - 40 - 60;
    if (vz.botao >= n) vz.botao = n - 1;
    for (i = 0; i < n; i++) {
      float bw = ajustes_ui_botao(i18n(VB_ROTULO[lista[i]]), NULL, bx, by, i == vz.botao);
      ponteiro_alvo(bx, by, bw, 60, ponteiroBotaoVaz, NULL, i, 0);
      bx += bw + 12;
    }
  } else {
    const char *k[1] = { "Voltar" }, *l[1] = { "Cancelar" };
    ajustes_ui_dicas(k, l, 1, e.x, NV_TELA_H - 40 - 45, 1);
  }
}

#ifdef NV_TOUCH_UI
static ToqueRolagem toqueDiagPagina;
static float diagPaginaY;
static int diagPaginaEstado = -1;
static void dgPhoneReiniciar(void) {
  diagPaginaEstado = -1; diagPaginaY = 0; memset(&toqueDiagPagina, 0, sizeof toqueDiagPagina);
  telefonecartao_limpar(&dgIntroPhone);
}
static int dgPhoneRolar(const PonteiroRolagem *e) {
  return d.intro ? 0 : toquerol_evento(&toqueDiagPagina, e);
}
static void dgPhoneAcao(int tipo, int i) {
  if (d.intro || introGlobal || sairTela) return;
  int estado = atomic_load(&d.estado), ve = atomic_load(&vz.estado);
  if (tipo == 0 && !vz.aberto && estado == 0) { focoModo = i; focoLinha = 0; }
  else if (tipo == 1 && !vz.aberto && estado == 0) { focoLinha = i; if (i) iniciarVazao(); else iniciarTeste(); }
  else if (tipo == 5 && !vz.aberto && estado > 2) iniciarTeste();
  else if (tipo == 2 && !vz.aberto && estado == 2 && d.sugEstado != DS_TESTANDO) {
    int lista[B_N], n = botoesVisiveis(lista);
    if (i >= 0 && i < n) { d.botao = i; acionarBotao(lista[i]); }
  } else if (tipo == 3 && vz.aberto && ve != 1) {
    int lista[VB_N], n = vazBotoes(lista);
    if (i >= 0 && i < n) { vz.botao = i; acionarVazBotao(lista[i]); }
  } else if (tipo == 4) {
    if (vz.aberto) {
      if (ve == 1) atomic_store(&vz.cancelado, 1);
      else { vz.aberto = 0; if (soVelocidade) { soVelocidade = 0; sairTela = 1; } }
    } else if (estado == 1 || d.sugEstado == DS_TESTANDO) atomic_store(&d.cancelado, 1);
    else sairTela = 1;
  }
}
static float dgPhoneCard(const char *titulo, const char *texto, float y, float W, int draw) {
  float h = 36 + txt_bloco_corta(TXT_AJ_TIT28, i18n(titulo), 243, 242, 239, 80, y + 20, W - 160, 34, 0, 8);
  if (texto && texto[0]) h += 12 + txt_bloco_corta(TXT_AJ_TEXTO, texto, 243, 242, 239, 80, y + h, W - 160, 30, 0, 40);
  h += 20;
  if (draw) { /* text above is measured first, material must precede it */
    ajustes_ui_ilha((GfxRect){48, y, W - 96, h}, 24, 0);
    float yy = y + 20;
    yy += txt_bloco_corta(TXT_AJ_TIT28, i18n(titulo), 243, 242, 239, 80, yy, W - 160, 34, 1, 8) + 12;
    if (texto && texto[0]) txt_bloco_corta(TXT_AJ_TEXTO, texto, 243, 242, 239, 80, yy, W - 160, 30, .7f, 40);
  }
  return y + h + 18;
}
static float dgPhoneBotao(const char *rot, int tipo, int i, int foco, float y, float W, float topo, float base, int draw) {
  GfxRect r = {48, y, W - 96, 60};
  if (draw) {
    if (foco) plrui_pilula_foco(r, 1); else plrui_botao_repouso(r, 1);
    int tinta = ajustes_tinta_foco();
    TxtLinha l = foco ? txt_linha_corta(TXT_ILHA_ITEM, i18n(rot), tinta, tinta, tinta, 255, r.w - 40)
                     : txt_linha_corta(TXT_ILHA_ITEM, i18n(rot), DG_TX, r.w - 40);
    txt_desenhar_alpha(l, r.x + 20, y + (60 - l.h) * .5f, 1);
    if (!d.intro) ponteiro_alvo_faixa(r.x, r.y, r.w, r.h, topo, base, NULL, dgPhoneAcao, tipo, i);
  }
  return y + 72;
}
static float dgPhoneCorpo(float y, float W, float topo, float base, int draw) {
  int est = atomic_load(&d.estado), i, cor;
  char texto[1600], v[200];
  if (vz.aberto) {
    int ve = atomic_load(&vz.estado);
    char med[32], pior[32], ot[200], mx[200];
    vazao_fmt_mbps(med, sizeof med, vz.resumo.medianaKbps, sepDecimal());
    vazao_fmt_mbps(pior, sizeof pior, vz.resumo.p20Kbps, sepDecimal());
    if (ve == 1) {
      int fase = atomic_load(&vz.fase), nf = atomic_load(&vz.nFonte);
      snprintf(texto, sizeof texto, "%s\n%s: %d / %d\n%s: %d / %d",
               i18n(fase <= 1 ? "Perguntando as fontes a cada add-on" : "Baixando um trecho de cada fonte"),
               i18n("Etapas de rede"), atomic_load(&vz.feitos), atomic_load(&vz.total),
               i18n("Fontes medidas"), vz.ciclo == VCM_RAPIDO ? nf : atomic_load(&vz.nCic),
               vz.ciclo == VCM_RAPIDO ? VAZAO_FONTES_MAX : vz.nFila);
      y = dgPhoneCard("Teste de velocidade", texto, y, W, draw);
      if (draw) {
        int total = atomic_load(&vz.total);
        float pct = total > 0 ? 100.0f * atomic_load(&vz.feitos) / total : 0;
        float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
        barraProgresso((GfxRect){80, y, W - 160, 12}, pct, ar, ag, ab, 1, SDL_GetTicks());
      }
      y += 30;
    } else if (vz.resultado == VR_OK && vz.ciclo == VCM_RAPIDO) {
      snprintf(texto, sizeof texto, i18n("Velocidade: %s Mbps (pior trecho %s Mbps)"), med, pior);
      y = dgPhoneCard("Tamanho de arquivo que toca sem parar nesta TV", texto, y, W, draw);
      linhaTamanho(ot, sizeof ot, i18n("Ótimo: até %s GB por filme de 2 h · %s GB por episódio"), vz.resumo.otimoKbps);
      linhaTamanho(mx, sizeof mx, i18n("Máximo: até %s GB por filme de 2 h · %s GB por episódio"), vz.resumo.maximoKbps);
      snprintf(texto, sizeof texto, "%s\n%s\n%s\n%s", ot, mx, i18n(vazao_dica(vz.resumo.otimoKbps)),
               i18n("Filme de 2 h e episódio de 45 min; o tamanho do arquivo aparece no nome da fonte."));
      y = dgPhoneCard("Filme de 2 h · tamanho do arquivo", texto, y, W, draw);
      snprintf(texto, sizeof texto, i18n("%d de %d tentadas · %d segundos"), atomic_load(&vz.nFonte), vz.tentadas, vz.nAmostra);
      y = dgPhoneCard("Fontes medidas", texto, y, W, draw);
    }
    y = dgPhoneCard("Vazão por fonte", i18n(vz.ciclo == VCM_RAPIDO
             ? "Baixa por 8 s até 3 fontes de servidores diferentes, começando no meio do arquivo; os dados são descartados."
             : "Mediana de cada add-on e ranking das fontes"), y, W, draw);
    if (vz.ciclo != VCM_RAPIDO) {
      int n = atomic_load(&vz.nCic), ordem[VAZ_CICLO_LISTA_MAX];
      vazao_ordenar(vz.cic, n, ordem);
      for (i = 0; i < n; i++) {
        const VazCicloRes *c = &vz.cic[ordem[i]];
        char mb[32], selos[96];
        vazao_fmt_mbps(mb, sizeof mb, c->r.medianaKbps, sepDecimal()); selosDaFonte(selos, sizeof selos, c);
        snprintf(texto, sizeof texto, "#%d %s · %s\n%s · %s · HTTP %d · %d ms\n%s Mbps · %s",
                 i + 1, addons_nome(c->addon), c->nome, selos, c->host, c->http, c->esperaMs, mb, i18n(veredito(c, &cor)));
        y = dgPhoneCard("Vazão por fonte", texto, y, W, draw);
      }
      for (i = 0; i < vz.nAddon; i++) {
        int usa, mb = vazao_mediana_addon(vz.cic, n, i, &usa);
        if (!usa) continue;
        char b[32]; vazao_fmt_mbps(b, sizeof b, mb, sepDecimal());
        snprintf(texto, sizeof texto, "%s · %s Mbps · %d %s", addons_nome(i), b, usa, i18n("Fontes medidas"));
        y = dgPhoneCard("Mediana por add-on", texto, y, W, draw);
      }
      snprintf(texto, sizeof texto, "%s: %d\n%s: %d\n%s: %d", i18n("Não testadas (debrid)"), vz.debridTotal,
               i18n("Fora do limite"), vz.sel.foraDoLimite, i18n("Não testadas (tempo)"), vz.agenda.restantes);
      y = dgPhoneCard("Fontes medidas", texto, y, W, draw);
    } else {
      for (i = 0; i < atomic_load(&vz.nFonte); i++) {
        char b[32], p[32]; vazao_fmt_mbps(b, sizeof b, vz.fonte[i].r.medianaKbps, sepDecimal());
        vazao_fmt_mbps(p, sizeof p, vz.fonte[i].r.p20Kbps, sepDecimal());
        snprintf(texto, sizeof texto, "%s Mbps · %s %s Mbps · HTTP %d · %lu ms", b, i18n("pior trecho"), p, vz.fonte[i].http, vz.fonte[i].ms);
        y = dgPhoneCard("Vazão por fonte", texto, y, W, draw);
      }
    }
    if (ve != 1 || atomic_load(&vz.fase) >= 2) for (i = 0; i < vz.nAddon; i++) {
      const VazAddon *a = &vz.addon[i];
      if (!a->medido) continue;
      if (a->ok) snprintf(texto, sizeof texto, "%s · %d ms · %d %s", addons_nome(i), a->ms, a->fontes, i18n("Fontes"));
      else snprintf(texto, sizeof texto, "%s · HTTP %d · %s", addons_nome(i), a->http, i18n("falhou"));
      y = dgPhoneCard("Add-ons e fontes", texto, y, W, draw);
    }
    /* Keep the original explicit empty/error explanation, rather than fake zero success. */
    if (ve != 1 && vz.resultado != VR_OK) {
      const char *det, *falha = textoFalhaVazao(vz.resultado, &det);
      snprintf(texto, sizeof texto, "%s%s%s", i18n(falha), det ? "\n" : "", det ? i18n(det) : "");
      y = dgPhoneCard("Teste de velocidade", texto, y, W, draw);
    }
    if (ve != 1) {
      int lista[VB_N], n = vazBotoes(lista);
      for (i = 0; i < n; i++) y = dgPhoneBotao(VB_ROTULO[lista[i]], 3, i, i == vz.botao, y, W, topo, base, draw);
    }
  } else if (est == 0) {
    PtvPerfil atual, cand; int travado; long mem;
    perfilAtual(&atual, &travado, &mem);
    ptv_candidato(ptv_plataforma(), mem, focoModo ? PTV_DESEMPENHO : PTV_QUALIDADE, travado, &cand);
    y = dgPhoneCard("Objetivo", i18n("Teste os addons, artes e fontes desta TV; o relatório é enviado ao suporte."), y, W, draw);
    y = dgPhoneBotao("Qualidade", 0, 0, focoModo == 0, y, W, topo, base, draw);
    y = dgPhoneBotao("Desempenho", 0, 1, focoModo == 1, y, W, topo, base, draw);
    snprintf(texto, sizeof texto, "%s: %d → %d MB\n%s: %d → %d\n%s: %d → %d px",
             i18n("Memória para imagens"), atual.texMb, cand.texMb, i18n("Fios de rede das artes"), atual.fiosRede, cand.fiosRede,
             i18n("Largura do fundo em tela cheia"), atual.heroiLarg, cand.heroiLarg);
    y = dgPhoneCard("Candidato desta rodada", texto, y, W, draw);
    y = dgPhoneCard("Como funciona", i18n("Se o reteste da mesma amostra piorar, o perfil anterior volta sozinho e a tela diz o motivo."), y, W, draw);
    y = dgPhoneCard("Como funciona", i18n("Até 3 títulos, 6 fontes e 12 imagens; limite de 8 minutos"), y, W, draw);
    if (travado) y = dgPhoneCard("Memória para imagens", i18n("A memória para imagens foi escolhida em Ajustes e não muda."), y, W, draw);
    y = dgPhoneBotao("Iniciar diagnóstico", 1, 0, !focoLinha, y, W, topo, base, draw);
    y = dgPhoneBotao("Teste de velocidade", 1, 1, focoLinha, y, W, topo, base, draw);
  } else if (est == 1) {
    static const char *const etapas[] = {"Sondando manifestos, catálogos e fontes", "Medindo cada fonte de arte", "Artes com o cache frio",
      "Artes com o perfil atual", "Artes com o perfil candidato", "Gravando e enviando o relatório"};
    int fase = atomic_load(&d.fase); fase = fase < 1 ? 1 : fase > 6 ? 6 : fase;
    snprintf(texto, sizeof texto, "%s · %d / %d", i18n(etapas[fase - 1]), atomic_load(&d.feitos), atomic_load(&d.total));
    y = dgPhoneCard("Diagnóstico em andamento", texto, y, W, draw);
    for (i = 0; i < 6; i++) y = dgPhoneCard(etapas[i], i + 1 == fase ? i18n("agora") : NULL, y, W, draw);
  } else if (est > 2) {
    y = dgPhoneCard("Diagnóstico e otimização", i18n(d.erro[0] ? d.erro : "O teste foi cancelado."), y, W, draw);
    y = dgPhoneCard("Agora nesta TV", i18n(d.aplicacao == DA_CANCELADO ? textoAplicacao(&cor)
                                          : "Nenhuma configuração foi alterada."), y, W, draw);
    y = dgPhoneBotao("Testar de novo", 5, 0, 1, y, W, topo, base, draw);
  } else {
    snprintf(texto, sizeof texto, "%d%% · %s", coberturaPercentual(), gargaloPrincipal());
    y = dgPhoneCard("Resultado geral", texto, y, W, draw);
    y = dgPhoneCard("Agora nesta TV", i18n(textoAplicacao(&cor)), y, W, draw);
    if (d.motivo) y = dgPhoneCard("Resultado geral", i18n(d.motivo), y, W, draw);
    y = dgPhoneCard("Relatório", i18n(atomic_load(&d.enviando) ? "enviando…" : d.envioFalhou ? "guardado; envio falhou"
                    : d.enviado ? "enviado ao suporte" : "guardado localmente"), y, W, draw);
    for (i = 1; i < PTV_N_FONTES; i++) {
      const PtvFonte *f = &d.fonte[i];
      if (!f->ok && !f->falhas) continue;
      snprintf(texto, sizeof texto, "%d ms · %d×%d · %d %s", ptv_fonte_ms(f), f->largura, f->altura, f->falhas, i18n("falhas"));
      y = dgPhoneCard(ptv_fonte_rotulo(i), texto, y, W, draw);
    }
    snprintf(texto, sizeof texto, "%s: %d ms\n%s: %d ms\n%s: %d ms\n%s: %d ms",
             i18n("Manifestos"), d.manifestMs, i18n("Catálogos"), d.catalogMs, i18n("Artes"), d.assetsMs + d.fontesMs, i18n("Fontes"), d.streamMs);
    y = dgPhoneCard("Tempo por etapa", texto, y, W, draw);
    PtvPerfil antes, depois; perfilDoResultado(&antes, &depois);
    snprintf(texto, sizeof texto, "%s: %d → %d MB\n%s: %d → %d\n%s: %d → %d px\n%s: %d / %d / %d ms",
             i18n("Memória para imagens agora"), antes.texMb, depois.texMb, i18n("Fios de rede das artes"), antes.fiosRede, depois.fiosRede,
             i18n("Largura do fundo em tela cheia"), antes.heroiLarg, depois.heroiLarg, i18n("Artes, mesma amostra"), d.medFrio.artesMs, d.medAntes.artesMs, d.medDepois.artesMs);
    y = dgPhoneCard("Agora nesta TV", texto, y, W, draw);
    if (d.sugEstado == DS_PROPOSTA || d.sugEstado == DS_TESTANDO) {
      if (d.sug.motivo == PTV_MOTIVO_IGUAL && d.sug.alvo > 0)
        snprintf(texto, sizeof texto, i18n("Destaque em %s repete a imagem do card; %s é outra arte, %d ms."),
                 i18n(ptv_fonte_rotulo(d.sug.lenta)), i18n(ptv_fonte_rotulo(d.sug.alvo)), ptv_fonte_ms(&d.fonte[d.sug.alvo]));
      else snprintf(texto, sizeof texto, i18n("Destaque em %s: %d ms por arte; %s: %d ms."),
                    i18n(ptv_fonte_rotulo(d.sug.lenta)), d.sug.msLenta, i18n(ptv_fonte_rotulo(d.sug.base)), d.sug.msBase);
      y = dgPhoneCard("Artes por fonte", texto, y, W, draw);
      if (d.sugEstado == DS_TESTANDO) snprintf(texto, sizeof texto, "%s", i18n("Retestando a arte do destaque…"));
      else if (!d.sug.diferente) snprintf(texto, sizeof texto, "%s", i18n("Vai mudar: Destaque com outra arte, de Ligado para Desligado."));
      else snprintf(texto, sizeof texto, i18n("Vai mudar: Background do hero, de %s para %s."),
                    i18n(nomeFonteAjuste(ajustes_hero_fonte())), i18n(nomeFonteAjuste(d.sug.fonte)));
      y = dgPhoneCard("Artes por fonte", texto, y, W, draw);
    } else {
      if (d.sugEstado == DS_DESFEITA) snprintf(texto, sizeof texto, i18n("Sugestão desfeita sozinha: %s"), i18n(d.sugMotivo ? d.sugMotivo : ""));
      else snprintf(texto, sizeof texto, "%s", i18n(d.sugEstado == DS_MANTIDA
                 ? "Sugestão aplicada: o destaque ficou mais rápido no reteste." : "A fonte do destaque não está mais lenta que a do card."));
      y = dgPhoneCard("Artes por fonte", texto, y, W, draw);
    }
    int lista[B_N], n = botoesVisiveis(lista);
    for (i = 0; i < n; i++) y = dgPhoneBotao(BOTAO_ROTULO[lista[i]], 2, i, i == d.botao, y, W, topo, base, draw);
  }
  long bytes = 0; int pend = 0, quentes = 0;
  tex_estatisticas(NULL, &pend, &bytes, &quentes, NULL);
  snprintf(v, sizeof v, "%ld / %ld MB · %d %s · %d %s", bytes >> 20, tex_orcamento_bytes() >> 20,
           pend, i18n("pendentes"), quentes, i18n("quentes"));
  return dgPhoneCard("Memória usada por imagens", v, y, W, draw);
}
static void dgTelefone(Uint32 agora) {
  (void)agora;
  float W = nv_layout_w / gfx_escala(), H = nv_layout_h / gfx_escala(), topo = 116, base = H - 36;
  int key = vz.aberto ? 100 + atomic_load(&vz.estado) : atomic_load(&d.estado);
  ajustes_ui_fundo();
  if (key != diagPaginaEstado) { diagPaginaEstado = key; diagPaginaY = 0; memset(&toqueDiagPagina, 0, sizeof toqueDiagPagina); }
  TxtLinha t = txt_linha_corta(TXT_TITULO3, i18n(vz.aberto ? "Teste de velocidade" : "Diagnóstico e otimização"), DG_TX, W - 196);
  txt_desenhar(t, 48, 32);
  GfxRect fechar = {W - 100, 32, 52, 52};
  gfx_icone(fechar, "aj_x", 1, 1, 1, 1);
  if (!d.intro) ponteiro_alvo(fechar.x, fechar.y, fechar.w, fechar.h, NULL, dgPhoneAcao, 4, 0);
  float fim = dgPhoneCorpo(topo, W, topo, base, 0), max = fmaxf(0, fim - base);
  toquerol_vincular(&toqueDiagPagina, (GfxRect){48, topo, W - 96, base - topo}, gfx_escala(), 0, max, 1, &diagPaginaY);
  diagPaginaY = toquerol_clamp(diagPaginaY, 0, max);
  if (!d.intro) ponteiro_rolagem(dgPhoneRolar);
  gfx_recorte(48, topo, W - 96, base - topo);
  dgPhoneCorpo(topo - diagPaginaY, W, topo, base, 1);
  gfx_sem_recorte();
}
#endif
