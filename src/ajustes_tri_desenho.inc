// AJUSTES 2.1 — TRES ILHAS: o desenho. Estado e eventos em ajustes_tri.inc.
// Medidas do mockup (docs/reviews/ajustes-fable/mockup, 1920 x 1080): cabecalho
// a partir de y 112, as ilhas de 232 a 1000, dicas em 1034. Ilha 1 = 400, ilha 3
// = 480, a lista fica com o resto; sem inspetor ("Lista" em Aparencia, tela
// estreita ou retrato) a lista toma a largura dele.
#include "menu.h"
_Static_assert(AJS_HOME == 4 && AJS_CONTAS == 8, "triGrupo segue a ordem dos SEC de TELA");

#ifdef NV_DESKTOP_UI
#define TRI_TOPO      (layout_desktop_ativo() ? 208.0f / AJ2_S : 232.0f)
#else
#define TRI_TOPO      232.0f
#endif
#define TRI_BASE      (triBaseTela())
#ifdef NV_DESKTOP_UI
#define TRI_VAO        (layout_desktop_ativo() ? 64.0f / 3.0f / AJ2_S : 16.0f)
// Owner's Mac board: stable category, content and inspector columns.
#define TRI_SEC_W     (layout_desktop_ativo() ? 904.0f / 3.0f / AJ2_S : 400.0f)
#define TRI_INSP_W    (layout_desktop_ativo() ? 360.0f / AJ2_S : 480.0f)
#define TRI_SEC_LN     (layout_desktop_ativo() ? 46.0f / AJ2_S : ajRetrato() ? AJ2_LN_H : 54.0f)
#define TRI_LISTA_CAB  (layout_desktop_ativo() ? 128.0f : 104.0f)
#else
#define TRI_VAO        16.0f
#define TRI_SEC_W     (layout_desktop_ativo() ? 470.0f : 400.0f)
#define TRI_INSP_W    (layout_desktop_ativo() ? 560.0f : 480.0f)
#define TRI_SEC_LN     (ajRetrato() ? AJ2_LN_H : 54.0f)
#define TRI_LISTA_CAB  104.0f
#endif

// PALCO (mockup claude-c, aprovado 30/10/2026): a tela e uma PILHA DE BLADES.
// Menu 380 em (48,112); a partir da profundidade 2 vira pilula de icones 104.
// Blades de 500, vão 20, topo 112, altura 932; base x 452 (prof. 1) e 176
// (prof. 2+); na prof. 3 o mais antigo sai e a tira desliza. O breadcrumb
// (ilha 56) diz o caminho; o PALCO a direita nao tem controle nenhum: previa
// real, kicker, titulo 44/700, ajuda 24 (3 linhas) e Agora/Padrao/Vale para.
#define TRI_P_COL       500.0f
#define TRI_P_VAO        20.0f
#define TRI_P_TOPO      112.0f
#define TRI_P_H         (NV_VTELA_H - 148.0f)   /* 932 at 1080; shrinks with text/Settings scale */
#define TRI_P_X1        452.0f
#define TRI_P_X2        176.0f
static float triBladeLargura(void) { return TRI_P_COL; }
// triProfF (a profundidade animada, mola NV_MOLA_TELA) e o estado em
// ajustes_tri.inc, incluido antes deste arquivo: enxergamos direto.

// Onde o conteudo comeca: depois da rail fixa (Padrao e Moderna, como a home
// faz com ajustes_conteudo_x), ou na margem de 48 quando nao ha rail na tela
// (rail recolhida, menu no topo, layout Dinamica com a pilula da Apple TV).
// O menu real e desenhado pelo app (menu.c) por cima; aqui nada o imita.
static float triX0(void) {
  float s = ajustes_tamanho_ajustes();
#ifdef NV_DESKTOP_UI
  if (layout_desktop_ativo()) return (NV_DESKTOP_RAIL_W + 208.0f / 3.0f) / s;
#endif
  if (ajRetrato()) return 16.0f * NV_LAYOUT_REAL_W / (466.0f * s);
  return (ajustes_rail_largura_fixa() > 0.0f ? fmaxf(48.0f, ajustes_conteudo_x()) : 48.0f) / s;
}
static float triXFim(void) {
#ifdef NV_DESKTOP_UI
  if (layout_desktop_ativo()) return NV_VTELA_W - 36.0f / AJ2_S;
#endif
  return NV_VTELA_W - (ajRetrato() ? 0.0f : 48.0f);
}
static int triTemSecoes(void) { return !ajRetrato(); }

// --- O PALCO (mockup claude-c): o layout de blades --------------------------------------
// Ativo na tela larga com secoes (o retrato/"Lista"/celular mantem o caminho
// antigo: uma faixa de secoes e uma blade por vez, sem palco).
static int triBladeLayout(void) {
#ifdef NV_DESKTOP_UI
  if (layout_desktop_ativo()) return 0;
#endif
  return !ajRetrato() && !ajustes_layout_lista() && !ajTelaCheia() &&
         triXFim() - triX0() >= 1240.0f;
}
static float triClamp(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
// O x da base da tira de blades: 404 depois do menu aberto (380+24), 128
// depois do menu condensado (104+24); a mudanca acompanha a mola da profundidade.
static float triBaseX(float d) { return triX0() + 404.0f - 276.0f * triClamp(d - 1.0f, 0.0f, 1.0f); }
// A posicao horizontal do blade k: na prof. 3 o mais antigo sai e a tira
// desliza (vis cresce com a profundidade passando de 2).
static float triBladeX(int k) {
  float vis = triProfF > 2.0f ? triProfF - 2.0f : 0.0f;
  return triBaseX(triProfF) + (k - vis) * (TRI_P_COL + TRI_P_VAO);
}
static float triBladeA(int k) { return 1.0f - triClamp(triProfF - 2.0f - k, 0.0f, 1.0f); }
// A CENA DO ESTAGIO (a direita dos blades, sem ilha e sem controle nenhum):
// ancora em cada profundidade e interpola com a mola. d 0: 476 x 1396 x 640
// (corte largo); 1: 988 x 884; 2+: 1240 x 632 (16:9). Centrada na vertical
// com a legenda (mockup: top = max(112, 578 - (h + 270) / 2 - 10)).
static GfxRect triPalcoR(void) {
  float d = triProfF, xf = triXFim(), x, w, h;
  float a0 = triX0() + 428.0f, a1 = triX0() + 940.0f, a2 = triX0() + 1192.0f;
  if (d < 1.0f) x = a0 + (a1 - a0) * d;
  else if (d < 2.0f) x = a1 + (a2 - a1) * (d - 1.0f);
  else x = a2;
  if (x > xf - 200.0f) x = xf - 200.0f;
  w = xf - x; h = fminf(w * 9.0f / 16.0f, 640.0f);
  return (GfxRect){ x, fmaxf(TRI_P_TOPO, TRI_P_TOPO + TRI_P_H * 0.5f - (h + 270.0f) * 0.5f - 10.0f), w, h };
}
// O blade que esta sendo desenhado: -1 = o do topo; 0..2 = um pai (a geometria
// da "ilha 2" do nucleo 2.0.3 segue o blade em desenho).
static int triNivelDesenho = -1;
static float triFilaDx;   // Palco: o deslize da linha em desenho (entrada escalonada)
static int triTopoNivel(void) { int n = triProfundidade(); return n > 0 ? n - 1 : 0; }

// Portrait reuses the section island as a grouped list, then opens its page.
static float triIlhaTopo(void) { return triBladeLayout() ? TRI_P_TOPO : TRI_TOPO; }
static float triBaseTela(void) {
#ifdef NV_DESKTOP_UI
  if (layout_desktop_ativo()) return NV_VTELA_H - 48.0f / AJ2_S;
#endif
  return triBladeLayout() ? TRI_P_TOPO + TRI_P_H : NV_VTELA_H - 80.0f;
}
static int triTemInsp(void) {
#ifdef NV_DESKTOP_UI
  if (layout_desktop_ativo()) return 1;
#endif
  if (ajustes_layout_lista() || ajRetrato()) return 0;
  return triBladeLayout() || triXFim() - triX0() - TRI_SEC_W - TRI_VAO - TRI_INSP_W - TRI_VAO >= 560.0f;
}
static GfxRect triSecR(void) {
  if (triBladeLayout()) {   // o menu do Palco: 380, condensando para 104 na prof. 2
    float w = 380.0f - 276.0f * triClamp(triProfF - 1.0f, 0.0f, 1.0f);
    return (GfxRect){ triX0(), TRI_P_TOPO, w, TRI_P_H };
  }
  return (GfxRect){ triX0(), triIlhaTopo(), ajRetrato() ? triXFim() - triX0() : triTemSecoes() ? TRI_SEC_W : 0.0f, TRI_BASE - triIlhaTopo() };
}
// Editores que precisam da largura do 2.0.3 (lista + painel): a ilha 3 cresce.
static int triEditorLargo(int op) {
  if (ajTelaCheia()) return 1;   // celular: o editor 2.0.3 toma a pagina (aj2EditorR)
  return op == AJ_GUIA_CATEGORIAS || op == AJ_INTERFACE_MODO || op == AJ_P2P_DESTINO || op == AJ_FIL_LIMITE || op == AJ_IPTV_CONTA || ajIptvMenu(op);
}
// A largura NAO pergunta a aj2EditorPainel/ajEmLinha: ajSegmentada mede a lista,
// que mede esta ilha (recursao infinita). Os editores largos nunca sao "na linha".
static GfxRect triInspR(void) {
  if (triBladeLayout()) {   // o PALCO: a direita dos blades, sem ilha por tras
    GfxRect p = triPalcoR();
    p.y = TRI_P_TOPO; p.h = TRI_P_H;   // a coluna inteira (editor largo, rotas utilitarias)
    if (uxEditor == 1 && uxOp >= 0 && triEditorLargo(uxOp)) {   // editor largo: 760 sobre o palco
      float w = 760.0f;
      if (p.x + w > triXFim()) p.x = triXFim() - w;
      return (GfxRect){ p.x, p.y, fminf(w, triXFim() - p.x), p.h };
    }
    return p;
  }
  float w = triTemInsp() ? (uxEditor == 1 && uxOp >= 0 && triEditorLargo(uxOp) ? 760.0f : TRI_INSP_W) : 0.0f;
  return (GfxRect){ triXFim() - w, triIlhaTopo(), w, TRI_BASE - triIlhaTopo() };
}
static GfxRect triIlha2R(void) {
  if (triBladeLayout()) {   // a "ilha 2" e o blade em desenho (pai ou topo)
    int k = triNivelDesenho >= 0 ? triNivelDesenho : triTopoNivel();
    return (GfxRect){ triBladeX(k) + triFilaDx, TRI_P_TOPO, TRI_P_COL, TRI_P_H };
  }
  float x = triX0() + (triTemSecoes() ? TRI_SEC_W + TRI_VAO : 0.0f);
  float fim = triTemInsp() ? triInspR().x - TRI_VAO : triXFim();
  return (GfxRect){ x, triIlhaTopo(), fim - x, TRI_BASE - triIlhaTopo() };
}
// O retangulo que o nucleo 2.0.3 usa como "a lista": a ilha 2 sem o cabecalho.
static GfxRect triListaR(void) {
  GfxRect r = triIlha2R();
  r.y += TRI_LISTA_CAB; r.h -= TRI_LISTA_CAB;
  // Shared row renderers inset by 18; phone rows reach both card edges.
  if (ajRetrato()) { r.x -= 18.0f; r.w += 36.0f; }
  return r;
}
static float triListaBase(void) { return TRI_BASE - 12.0f; }
// O inspetor desenha dentro da ilha 3 com margem de 24 (a previa de 432).
// No Palco o estagio flutua: so 8 de folga (a cena tem raio e sombra proprios).
static GfxRect triInspMiolo(void) {
  GfxRect r = triInspR();
  float m = triBladeLayout() ? 8.0f : 24.0f;
  return (GfxRect){ r.x + m, r.y + m, r.w - 2.0f * m, r.h - 2.0f * m };
}

// --- CABECALHO: kicker, titulo e os chips de ferramenta -------------------------------
static void triPontChip(int i, int b) { (void)b; if (uxEditor || triUtil || i < 0 || i >= TRI_C_N) return; triIlha = TRI_CHIPS; focoIndice = 1; triChip = i; }
static float triChipLargura(int i, const char *rot, const char *n) {
  float w = 22.0f + 22.0f + 12.0f + (float)txt_largura(TXT_V2_SEG, rot) + 22.0f;
  if (n && n[0]) w += 10.0f + (float)txt_largura(TXT_V2_SEG, n);
  (void)i;
  return w;
}
static void triCabecalho(void) {
  static const char *const icones[] = { "aj_search", "aj_wand-sparkles", "aj_rotate-ccw", "aj_sparkles" };
  static const char *const rot[] = { "Buscar ajuste", "Predefinições", "Diferente do padrão", "O que o Nuvio faz?" };
  float x = triX0(), y = 112.0f, xd = triXFim(), ar, ag, ab;
#ifdef NV_DESKTOP_UI
  if (layout_desktop_ativo()) y = 88.0f / AJ2_S;
#endif
  char n[TRI_C_N][24];
  int i, difs = 0;
  ajustes_acento(&ar, &ag, &ab);
  if (triBladeLayout()) {
    // PALCO: no alto ficam o BREADCRUMB (ilha 56, entra na prof. 1) e os CHIPS
    // de ferramenta (saem na prof. 1, como o mockup claude-c). Sem barra de
    // dicas: nada de rodape (o titulo "Ajustes" mora no menu).
    float fchips = 1.0f - triClamp(triProfF, 0.0f, 1.0f);
    float ftrilha = triClamp(triProfF, 0.0f, 1.0f);
    for (i = 0; i < AJ_N; i++) if (TELA[i].tipo == IT_OPC && ajOpcaoDisponivel(TELA[i].op) && uxDiferente(TELA[i].op) && triMutavel(TELA[i].op)) difs++;
    for (i = 0; i < TRI_C_N; i++) n[i][0] = 0;
    if (difs) snprintf(n[TRI_C_DIF], sizeof n[0], "· %d", difs);
    if (fchips > 0.01f) {
      float a = fchips;
      for (i = TRI_C_N - 1; i >= 0; i--) {
        float f = triChipF[i];
        float w = triChipLargura(i, i18n(rot[i]), n[i]);
        GfxRect r = { xd - w, 36.0f, w, 56.0f };
        TxtLinha t = f > 0.5f ? ajTxtT(TXT_V2_SEG, rot[i]) : ajTxt(TXT_V2_SEG, rot[i]);
        float tx = r.x + 22.0f;
        if (a > 0.9f) ponteiro_alvo(r.x, r.y, r.w, r.h, triPontChip, NULL, i, 0);
        gfx_opacidade_grupo = a;
        if (f > 0.01f) ajPilulaAcento(r, 28.0f, f);
        if (f < 0.99f) ajNeutro(r, 28.0f, 0.08f * (1.0f - f), AJ_CHIP_SOL, 1.0f - f);
        if (f > 0.5f) ajIconeT(icones[i], tx, r.y + 17.0f, 22.0f, 1); else ajIcone(icones[i], tx, r.y + 17.0f, 22.0f, 0.8f);
        tx += 34.0f;
        txt_desenhar_alpha(t, tx, r.y + (56.0f - t.h) * 0.5f, f > 0.5f ? 1.0f : 0.86f);
        tx += t.w;
        if (n[i][0]) {
          TxtLinha c = f > 0.5f ? ajTxtT(TXT_V2_SEG, n[i]) : txt_linha(TXT_V2_SEG, n[i], (int)(ar * 255), (int)(ag * 255), (int)(ab * 255), 255);
          txt_desenhar_alpha(c, tx + 10.0f, r.y + (56.0f - c.h) * 0.5f, f > 0.5f ? 0.8f : 1.0f);
        }
        gfx_opacidade_grupo = 1.0f;
        xd = r.x - 12.0f;
      }
    }
    if (ftrilha > 0.01f) {
      // A ilha do caminho: "Ajustes › Reproducao › Durante e depois", o ultimo
      // em branco forte. Sempre legivel, em qualquer profundidade.
      const char *partes[4];
      int np = 0, p;
      float w = 0.0f, tx;
      GfxRect r;
      partes[np++] = "Ajustes";
      partes[np++] = TELA[secIni[secAtual]].titulo;
      if (triSubmenu >= 0) partes[np++] = TELA[triSubmenu].titulo;
      if (triEditorBlade()) partes[np++] = rotuloOpcao(uxOp);
      for (p = 0; p < np; p++)
        w += (float)txt_largura(TXT_G21B, i18n(partes[p])) + (p ? 10.0f + 18.0f + 10.0f : 0.0f);
      w += 48.0f;
      r = (GfxRect){ triX0() + 176.0f, 36.0f, fminf(w, triPalcoR().x - triX0() - 200.0f), 56.0f };
      if (r.w < 120.0f) r.w = 120.0f;
      gfx_opacidade_grupo = ftrilha;
      ajIlha(r, 28.0f, 1, 0);
      gfx_opacidade_grupo = 1.0f;
      tx = r.x + 24.0f;
      // Cada segmento entra deslizando com a profundidade dele (mesma mola).
      gfx_recorte(r.x, r.y, r.w, r.h);
      for (p = 0; p < np; p++) {
        float fs = p ? triClamp(triProfF - (float)(p - 1), 0.0f, 1.0f) : 1.0f, sx = 22.0f * (1.0f - fs);
        TxtLinha t = ajTxtC(TXT_G21B, i18n(partes[p]), r.w - (tx - r.x) - 20.0f);
        txt_desenhar_alpha(t, tx + sx, r.y + (56.0f - t.h) * 0.5f, (p == np - 1 ? 1.0f : 0.62f) * fs);
        tx += t.w;
        if (p + 1 < np) {
          float fc = triClamp(triProfF - (float)p, 0.0f, 1.0f);
          ajIcone("aj_chevron-right", tx + 10.0f + 22.0f * (1.0f - fc), r.y + 19.0f, 18.0f, 0.5f * fc);
          tx += 38.0f;
        }
        if (tx > r.x + r.w - 30.0f) break;
      }
      gfx_sem_recorte();
    }
    return;
  }
  if (ajRetrato()) {
    xd -= nv_inset_d / ajustes_tamanho_ajustes() + triX0();
    y = fmaxf(y, (nv_inset_t + 112.0f) / ajustes_tamanho_ajustes());
  }
  // O kicker: o perfil em uso (as preferencias "Conta/perfil" sao dele).
  // Layout Dinamica: a pilula da Apple TV (menu.c) ja diz "Ajustes" nesse
  // canto; o titulo grande ficaria duplicado atras dela (como perfil.c).
#ifdef NV_DESKTOP_UI
  if (layout_desktop_ativo()) {
    ajKicker("Nuvio · Desktop", x, y, 1);
    if (!desktop_native_control(4, 4, i18n("Ajustes"), 34, 1, x, y + 32 / AJ2_S, 480, 64 / AJ2_S, 1)) {
      TxtLinha title = ajTxt(TXT_AJ_NUM64, "Ajustes");
      txt_desenhar(title, x, y + 30);
    }
    for (i = 0; i < AJ_N; i++) if (TELA[i].tipo == IT_OPC && ajOpcaoDisponivel(TELA[i].op) && uxDiferente(TELA[i].op) && triMutavel(TELA[i].op)) difs++;
    const float points = (4.0f / 3.0f) / AJ2_S;
    static const float widths[] = {190, 118, 182, 150};
    static const float gaps[] = {0, 8, 18, 60};
    xd -= 38 * points;
    for (i = TRI_C_N - 1; i >= 0; i--) {
      char label[200];
      snprintf(label, sizeof label, "%s", i18n(rot[i]));
      if (i == TRI_C_DIF && difs) snprintf(label, sizeof label, "%s · %d", i18n(rot[i]), difs);
      float w = fmaxf(widths[i] * points, txt_largura(TXT_V2_SEG, label) + 44);
      GfxRect r = {xd - w, 100 * points, w, 24 * points};
      if (!desktop_native_control(i, i == TRI_C_BUSCA ? 3 : 2, label, 0, 1, r.x, r.y, r.w, r.h, 1)) {
        ajNeutro(r, 8, .08f, AJ_CHIP_SOL, 1);
        TxtLinha t = ajTxtC(TXT_V2_SEG, label, r.w - 24);
        txt_desenhar(t, r.x + 12, r.y + (r.h - t.h) * .5f);
      }
      ponteiro_alvo(r.x, r.y, r.w, r.h, triPontChip, NULL, i, 0);
      xd = r.x - gaps[i] * points;
    }
    return;
  }
#endif
  if (!menu_pilula_titulo()) {
    const ContaPerfil *p = perfis_item_ativo();
    char k[160];
    float largura = ajRetrato() ? fmaxf(1, xd - x - 4 * 68.0f) : NV_VTELA_W;
    TxtLinha t = ajTxtC(TXT_V2_TIT, "Ajustes", largura);
    if (p && p->nome[0]) snprintf(k, sizeof k, "Nuvio · %s", p->nome);
    else snprintf(k, sizeof k, "Nuvio");
    ajKicker(k, x, y, 1);
    txt_desenhar(t, x, y + 30.0f);
  }
  float total = 0.0f;
  int compacto;
  for (i = 0; i < AJ_N; i++) if (TELA[i].tipo == IT_OPC && ajOpcaoDisponivel(TELA[i].op) && uxDiferente(TELA[i].op) && triMutavel(TELA[i].op)) difs++;
  for (i = 0; i < TRI_C_N; i++) n[i][0] = 0;
  if (difs) snprintf(n[TRI_C_DIF], sizeof n[0], "· %d", difs);
  for (i = 0; i < TRI_C_N; i++) total += triChipLargura(i, i18n(rot[i]), n[i]) + 12.0f;
  // Tela estreita (retrato, celular): os quatro chips viram so o icone, e o
  // rotulo aparece no chip em foco. Nenhum chip some: todos continuam alvo.
  compacto = total - 12.0f > xd - (x + 300.0f);
  // Da direita para a esquerda, para a pilula da busca ficar onde couber.
  for (i = TRI_C_N - 1; i >= 0; i--) {
    float f = triChipF[i];
    int rotulo = !compacto || f > 0.5f;
    float w = rotulo ? triChipLargura(i, i18n(rot[i]), n[i]) : 56.0f;
    GfxRect r = { xd - w, y + 20.0f, w, 56.0f };
    TxtLinha t = f > 0.5f ? ajTxtT(TXT_V2_SEG, rot[i]) : ajTxt(TXT_V2_SEG, rot[i]);
    float tx = r.x + (rotulo ? 22.0f : 17.0f);
    if (r.x < x + 300.0f && !compacto) break;   // nao deveria acontecer: compacto cobre
    ponteiro_alvo(r.x, r.y, r.w, r.h, triPontChip, NULL, i, 0);
    if (f > 0.01f) ajPilulaAcento(r, 28.0f, f);
    if (f < 0.99f) ajNeutro(r, 28.0f, 0.08f * (1.0f - f), AJ_CHIP_SOL, 1.0f - f);
    if (f > 0.5f) ajIconeT(icones[i], tx, r.y + 17.0f, 22.0f, 1); else ajIcone(icones[i], tx, r.y + 17.0f, 22.0f, 0.8f);
    if (!rotulo) { xd = r.x - 12.0f; continue; }
    tx += 22.0f + 12.0f;
    txt_desenhar_alpha(t, tx, r.y + (56.0f - t.h) * 0.5f, f > 0.5f ? 1.0f : 0.86f);
    tx += t.w;
    if (n[i][0]) {
      TxtLinha c = f > 0.5f ? ajTxtT(TXT_V2_SEG, n[i]) : txt_linha(TXT_V2_SEG, n[i], (int)(ar * 255), (int)(ag * 255), (int)(ab * 255), 255);
      txt_desenhar_alpha(c, tx + 10.0f, r.y + (56.0f - c.h) * 0.5f, f > 0.5f ? 0.8f : 1.0f);
    }
    xd = r.x - 12.0f;
  }
}

// --- ILHA 1: as secoes ------------------------------------------------------------------
static void triPontSecao(int i, int b) {
  (void)b;
  if (uxEditor || i < 0 || i > nSecoes) return;
  if (triIlha == TRI_SECOES && i == triSecFoco) { triEntrar(i); return; }
  triFocarSecao(i);
}
static void triPontSecaoAbrir(int i, int b) {
  (void)b;
  if (!uxEditor && i >= 0 && i <= nSecoes) { triSecFoco = i; triEntrar(i); }
}
// A altura das secoes com os rotulos de grupo (o mesmo passo do desenho).
static float triSecoesAltura(void) {
#ifdef NV_DESKTOP_UI
  if (layout_desktop_ativo()) return (nSecoes + 1) * TRI_SEC_LN + 3 * 51 + 2 * 18;
#endif
  float h = 0.0f; int j, g = -1;
  for (j = 0; j <= nSecoes; j++) {
    int gj = triParental(j) ? 2 : triGrupo(j);
    if (gj != g) { if (g >= 0) h += 6.0f; g = gj; h += 10.0f + 15.0f + 6.0f; }
    h += TRI_SEC_LN;
  }
  return h;
}
static void triIlhaSecoes(void) {
  GfxRect ilha = triSecR();
  float topo = ilha.y + 12.0f, base = ilha.y + ilha.h - 12.0f, y, ar, ag, ab, total, fy0 = -1.0f, fy1 = -1.0f, alvo;
  int j, g = -1;
  int palco = triBladeLayout();
  float condK = palco ? triClamp(triProfF - 1.0f, 0.0f, 1.0f) : 0.0f;   // 1 = menu condensado (104)
  // Labels and counts are gone before the gliding icons reach x+62 (condK ~0.12).
  float rotA = triClamp(1.0f - condK / 0.11f, 0.0f, 1.0f);
  rotA *= rotA;
  if (ilha.w <= 0.0f) return;
  ajustes_acento(&ar, &ag, &ab);
  ajIlha(ilha, 28.0f, 1, 0);
  if (palco) {
    // A cabeca do menu (mockup claude-c): kicker do perfil + "Ajustes" 40/700;
    // condensado, eles cedem lugar ao icone de ajustes centrado (104 de pilula).
    if (condK < 0.99f) {
      const ContaPerfil *pf = perfis_item_ativo();
      char k[160];
      float a = rotA;   // gone before the gear icon reaches the title
      gfx_opacidade_grupo = a;
      gfx_recorte(ilha.x, ilha.y, ilha.w, 104.0f);
      if (pf && pf->nome[0]) snprintf(k, sizeof k, "Nuvio · %s", pf->nome);
      else snprintf(k, sizeof k, "Nuvio");
      if (!menu_pilula_titulo()) {
        ajKicker(k, ilha.x + 28.0f, ilha.y + 22.0f, 1);
        { TxtLinha t = ajTxtC(TXT_V3_NOME, "Ajustes", 380.0f - 56.0f);   // largura cheia: o recorte corta
          txt_desenhar(t, ilha.x + 28.0f, ilha.y + 48.0f); }
      }
      gfx_sem_recorte();
      gfx_opacidade_grupo = 1.0f;
    }
    if (condK > 0.01f) {
      gfx_opacidade_grupo = condK;
      ajIcone("aj_settings-2", ilha.x + (ilha.w - 28.0f) * 0.5f, ilha.y + 34.0f, 28.0f, 0.7f);
      gfx_opacidade_grupo = 1.0f;
    }
  }
  // Texto grande (ate 130%) ou tela baixa: as 12 entradas nao cabem; a ilha
  // rola ate a secao em foco (triSecRolY, mola em triAtualizar) e, no
  // celular, pelo dedo. Os alvos ficam recortados a parte visivel.
  if (palco) topo += 104.0f;   // a cabeca do menu (kicker + Ajustes) ocupa o alto da ilha
  total = triSecoesAltura();
#ifdef NV_TOUCH_UI
  if (ajRolagemLivre() && !uxEditor && !ajModalAberto())
    triSecRolY = ajToqueRegistrar(AJT_SECOES, (GfxRect){ ilha.x, topo, ilha.w, base - topo }, &triSecRolY, total - (base - topo), triSecRolY, 0);
#endif
#ifdef NV_DESKTOP_UI
  if (layout_desktop_ativo()) topo += 12;
#endif
  y = topo - triSecRolY;
  gfx_recorte(ilha.x, topo, ilha.w, base - topo);
  for (j = 0; j <= nSecoes; j++) {
    int gj = triParental(j) ? 2 : triGrupo(j), foco = triIlha == TRI_SECOES && j == triSecFoco;
    int aberta = !triUtil ? (!triParental(j) && j == secAtual) : triUtil == TRI_U_PARENTAL ? triParental(j) : 0;
    int primaria = j < 8;
    float f = triSecF[j], al = (foco || aberta) ? 1.0f : primaria ? 0.86f : 0.62f;
    const char *titulo = triParental(j) ? "Controle parental" : TELA[secIni[j]].titulo;
    const char *icone = triParental(j) ? "aj_shield" : TELA[secIni[j]].icone;
    GfxRect r;
    if (gj != g) {
#ifdef NV_DESKTOP_UI
      if (layout_desktop_ativo()) {
        if (g >= 0) y += 18;
        g = gj;
        ajCaps(TXT_V2_18, i18n(triGrupoNome(g)), 0, ilha.x + 24, y + 10, .5f);
        y += 51;
      } else
#endif
      {
        if (g >= 0) y += 6.0f;
        g = gj;
        if (condK < 0.99f) { gfx_opacidade_grupo = rotA; ajKicker(triGrupoNome(g), ilha.x + 32.0f, y + 10.0f, 1); gfx_opacidade_grupo = 1.0f; }
        y += 10.0f + 15.0f + 6.0f;
      }
    }
    float inset = ajRetrato() ? 0.0f : 14.0f;
    r = (GfxRect){ ilha.x + inset, y, ilha.w - 2.0f * inset, TRI_SEC_LN };
    if (j == triSecFoco) { fy0 = y + triSecRolY; fy1 = fy0 + TRI_SEC_LN; }
    ponteiro_alvo_faixa(r.x, r.y, r.w, r.h, topo, base, ajRetrato() ? NULL : triPontSecao, ajRetrato() ? triPontSecaoAbrir : NULL, j, 0);
    ajFocoLinha(r, 20.0f, f, 1);
#ifdef NV_DESKTOP_UI
    if (layout_desktop_ativo() && aberta) gfx_cor(r, 12 / r.h, 1, 1, 1, .06f * (1 - f));
#endif
    if (
#ifdef NV_DESKTOP_UI
        !layout_desktop_ativo() &&
#endif
        aberta && f < 0.99f) gfx_cor((GfxRect){ r.x + 7.0f, r.y + r.h * 0.5f - 2.5f, 5, 5 }, 0.5f, ar, ag, ab, 1.0f - f);
    float iconH = primaria ? 26.0f : 22.0f;
    float ix = r.x + 20.0f + (26.0f - iconH) * 0.5f;
    if (condK > 0.0f) {   // condensado: o icone centra na pilula de 104
      float cx = r.x + (r.w - iconH) * 0.5f;
      ix = ix + (cx - ix) * condK;
    }
    if (condK > 0.01f && aberta)   // a pilula da secao aberta no menu condensado
      ajNeutro((GfxRect){ r.x + (r.w - 58.0f) * 0.5f, r.y + (r.h - 58.0f) * 0.5f, 58.0f, 58.0f }, 18.0f, 0.10f, AJ_CHIP_SOL, condK);
    if (condK > 0.5f && aberta) {   // a secao aberta acende no acento (mockup .cond .aberta .m-ic)
      float ar2, ag2, ab2;
      ajustes_acento(&ar2, &ag2, &ab2);
      gfx_icone((GfxRect){ ix, r.y + (r.h - iconH) * 0.5f, iconH, iconH }, icone, ar2, ag2, ab2, al);
    } else
    ajIcone(icone, ix, r.y + (r.h - iconH) * 0.5f, iconH, al);
    if (condK < 0.99f) {
      { float width = (palco ? 380.0f - 28.0f : r.w) - 62.0f - 48.0f;   // Palco: mede no menu aberto; o recorte da ilha corta
        int style = primaria || foco ? TXT_W20_24B : TXT_AJ_SUB;
#ifdef NV_DESKTOP_UI
        if (layout_desktop_ativo()) style = TXT_V2_LN;
#endif
        if (primaria && txt_largura(style, i18n(titulo)) > width) style = TXT_ILHA_SECAO;
        TxtLinha t = ajTxtC(style, titulo, width);
        txt_desenhar_alpha(t, r.x + 62.0f, r.y + (r.h - t.h) * 0.5f, al * rotA); }
      if (!triParental(j)) {
        char c[16]; int nOp; TxtLinha t;
        { int nAv; triContar(j, &nOp, &nAv); nOp += nAv; }
        snprintf(c, sizeof c, "%d", nOp);
        t = ajTxt(TXT_V2_18, c);
        txt_desenhar_alpha(t, r.x + r.w - 20.0f - t.w, r.y + (r.h - t.h) * 0.5f, (foco ? 0.6f : 0.42f) * rotA);
      }
    }
    y += TRI_SEC_LN;
  }
  gfx_sem_recorte();
  alvo = triSecRolYAlvo;
  if (fy0 >= 0.0f) {
    if (fy1 - alvo > base) alvo = fy1 - base;
    if (fy0 - alvo < topo) alvo = fy0 - topo;
    // a 1.a secao de um grupo traz o rotulo do grupo junto
    if (fy0 - alvo - topo < 31.0f + 6.0f) alvo = fmaxf(0.0f, fy0 - topo - 37.0f);
  }
  triSecRolYAlvo = fmaxf(0.0f, fminf(alvo, fmaxf(0.0f, total - (base - topo))));
}

// --- ILHA 2: a lista da secao ---------------------------------------------------------------
static void triListaCabecalho(GfxRect ilha, const char *titulo, const char *sub, float a) {
  TxtLinha t = ajTxtC(TXT_V2_LN_B, titulo, ilha.w - 60.0f);
  float x = ilha.x + 30.0f, y = ilha.y + 22.0f;
#ifdef NV_DESKTOP_UI
  if (layout_desktop_ativo()) y += 16;
#endif
  txt_desenhar_alpha(t, x, y, a);
  if (sub && sub[0]) {
    TxtLinha s = ajTxtC(TXT_V2_18, sub, ilha.w - 60.0f);
    txt_desenhar_alpha(s, x, y + t.h + 8.0f, 0.55f * a);
  }
}
static void triLinhaUtil(GfxRect r, float f, float a, const char *icone, const char *titulo, const char *apoio,
                         const char *valor, int chevron, int ligado);
static void triPontSubVoltar(int i, int b) {
  (void)i; (void)b;
  if (uxEditor) return;
  if (triSubmenu >= 0) triSubVoltar();
  else if (ajRetrato()) triIrSecoes();
}
// O icone do bloco (mockup claude-c, ICG): o titulo do grupo escolhe o glifo.
static const char *triTopicoIcone(int item) {
  static const char *const M[][2] = {
    { "Imagem e som", "aj_monitor-play" }, { "Escolha da fonte", "aj_list-ordered" },
    { "Durante e depois", "aj_pause" }, { "Miniatura ao avançar", "aj_images" },
    { "Cores e vidro", "aj_palette" }, { "Fileiras", "aj_rows-3" }, { "Logo e abertura", "aj_sparkle" },
    { "Texto e tamanho", "aj_type" }, { "Movimento e relógio", "aj_clock" }, { "Tela de descanso", "aj_moon" } };
  size_t i;
  for (i = 0; i < sizeof M / sizeof *M; i++) if (!strcmp(TELA[item].titulo, M[i][0])) return M[i][1];
  return "aj_sliders-horizontal";
}
static int triRotItem = -1;   // the topic row triLinhaUtil is drawing (long-label wrap/marquee)
static void triTopicoDesenhar(int item, float y, float a) {
  GfxRect fr = triListaR(), r = {fr.x + 18, y + AJ2_LN_GAP, fr.w - 36, ajAlturaItem(item) - AJ2_LN_GAP};
  int n = 0;
  for (int i = item + 1; i < secFim(secAtual) && TELA[i].tipo == IT_OPC; i++) if (visivel(i)) n++;
  char count[48];
  // Palco: a contagem pura ("8 ›"), como o mockup; fora dele "8 opcoes".
  if (triBladeLayout()) snprintf(count, sizeof count, "%d", n);
  else snprintf(count, sizeof count, i18n("%d opções"), n);
  if (ajFocoCompart && aj2FocoNaLista(item)) triSupReportar(r);
  triRotItem = triBladeLayout() ? item : -1;
  triLinhaUtil(r, animItem[item], a, triBladeLayout() ? triTopicoIcone(item) : "aj_sliders-horizontal", TELA[item].titulo, NULL, count, 1, -1);
  triRotItem = -1;
  if (ajPontLista) ponteiro_alvo_faixa(r.x, r.y, r.w, r.h, AJ_TOPO, AJ_BASE, ajPonteiroItem, NULL, item, 0);
}
static void triLista(void) {
  GfxRect ilha = triIlha2R(), fr = triListaR();
  float topo = AJ_TOPO, base = AJ_BASE, y, k = anim_suave(paginaA), dy = 18.0f * (1.0f - k);
  float g0 = gfx_opacidade_grupo, tPg = 10.0f;
  int sec = secAtual, i, nOp, nAv, palco = triBladeLayout(), fila = 0;
  char sub[96];
  const Item *s = &TELA[secIni[sec]];
  ajIlha(ilha, 28.0f, 1, 0);
  // Pai da pilha: conteudo mais apagado (a linha que abriu o filho leva um
  // realce leve, triBladeRealce). O blade do topo acende com o foco.
  { float pai = triNivelDesenho >= 0 && triNivelDesenho < triTopoNivel() ? 0.8f : 1.0f;
    gfx_opacidade_grupo = g0 * (0.86f + 0.14f * anim_suave(triListaF)) * pai; }
  triContar(sec, &nOp, &nAv);
  if (nAv == 1) snprintf(sub, sizeof sub, i18n("%d opções · 1 técnica"), nOp);
  else if (nAv) snprintf(sub, sizeof sub, i18n("%d opções · %d técnicas"), nOp, nAv);
  else snprintf(sub, sizeof sub, i18n("%d opções"), nOp);
  if (palco) {
    // Cabecalho do blade (mockup claude-c): kicker caps + titulo 40/700, sem
    // botao de voltar (o Voltar do controle fecha o blade) e sem sub. O blade
    // inteiro entra pela mola da profundidade (triPalcoDesenhar): sem o
    // relogio de pagina aqui; as linhas entram escalonadas (tPg).
    const char *kick = triSubmenu >= 0 ? s->titulo : triGrupoNome(triGrupo(secAtual));
    const char *tit = triSubmenu >= 0 ? TELA[triSubmenu].titulo : s->titulo;
    int lv = triNivelDesenho >= 0 ? triNivelDesenho : triTopoNivel();
    k = 1.0f; dy = 0.0f;
    if (lv >= 0 && lv < 4) tPg = triPgT[lv];
    ajCaps(TXT_V2_KICK, i18n(kick), 2.52f, ilha.x + 30.0f, ilha.y + 26.0f, 0.45f);
    { TxtLinha t = ajTxtC(TXT_V3_NOME, i18n(tit), ilha.w - 60.0f);
      txt_desenhar_alpha(t, ilha.x + 30.0f, ilha.y + 48.0f, 1.0f); }
    // A SUPERFICIE DE FOCO unica (mola em triAtualizar): por baixo das linhas,
    // clipped to its blade (gliding between blades it must not cross the gap).
    if (ajFocoCompart && triSupA > 0.01f) {
      GfxRecorteAnt ra = gfx_recorte_dentro(ilha.x, ilha.y, ilha.w, ilha.h);
      ajFocoCompart = 0;
      ajFocoLinha((GfxRect){ triSup[0], triSup[1], triSup[2], triSup[3] }, 20.0f, triSupA, 1.0f);
      ajFocoCompart = 1;
      gfx_recorte_voltar(ra);
    }
  } else if (triSubmenu >= 0 || ajRetrato()) {
    GfxRect back = {ilha.x + 18, ilha.y + 18, 56, 56};
    char crumb[240];
    if (triSubmenu >= 0) snprintf(crumb, sizeof crumb, "%s / %s", i18n(s->titulo), i18n(TELA[triSubmenu].titulo));
    else snprintf(crumb, sizeof crumb, "%s", sub);
    triListaCabecalho((GfxRect){ilha.x + 56, ilha.y, ilha.w - 56, ilha.h}, triSubmenu >= 0 ? TELA[triSubmenu].titulo : s->titulo, crumb, k);
    ajNeutro(back, 28, 0.08f, AJ_CHIP_SOL, k);
    ajIcone("aj_arrow-left", back.x + 16, back.y + 16, 24, k);
    if (ajPontLista) ponteiro_alvo(back.x, back.y, back.w, back.h, NULL, triPontSubVoltar, 0, 0);
  } else triListaCabecalho(ilha, s->titulo, sub, k);
  if (ajustes_animacoes_reduzidas()) dy = 0.0f;
#ifdef NV_TOUCH_UI
  if (ajRolagemLivre() && ajPontLista) {
    float total = 0.0f;
    for (int q = secIni[sec]; q < secFim(sec); q++) total += alturaItem(q);
    ajToqueRegistrar(AJT_LISTA, (GfxRect){fr.x, topo, fr.w, base - topo}, &scrollY, total - (base - topo), scrollY, sec);
  }
#endif
  gfx_recorte(fr.x, topo, fr.w, base - topo);
  y = topo - scrollY + dy;
  for (i = secIni[sec] + 1; i < secFim(sec); i++) {
    float h = ajAlturaItem(i);
    if (h <= 0.0f) continue;
    if (y + h >= topo - 4 && y <= base + 4) {
      float ka = k;
      if (palco) {   // entrada escalonada: 34 px e 22 ms por linha, mola critica
        float t = tPg - 0.022f * (float)fila++, e = t <= 0.0f ? 0.0f : 1.0f - (1.0f + 16.0f * t) * expf(-16.0f * t);
        if (ajustes_animacoes_reduzidas()) e = 1.0f;
        triFilaDx = 34.0f * (1.0f - e); k = ka * e;
      }
      if (triTopico(i)) {
        triTopicoDesenhar(i, y, k);
      } else if (TELA[i].tipo == IT_ROT) {
        ajDesenharGrupo(i, y, ajMascara(y + 20.0f + 15.6f, topo, base) * k);
      } else if (TELA[i].tipo == IT_MAIS) {
        float yy = y + 14.0f + AJ2_LN_GAP;
        if (ajPontLista
#ifdef NV_TOUCH_UI
            && !ajPontInline
#endif
           ) ponteiro_alvo_faixa(fr.x + 18.0f, yy, fr.w - 36.0f, AJ2_LN_H, topo, base, ajPonteiroItem, NULL, i, 0);
        ajDesenharMais(i, yy, animItem[i], ajMascara(yy + AJ2_LN_H * 0.5f, topo, base) * k);
      } else if (TELA[i].tipo == IT_OPC) {
        float lh = ajLinhaH(i), yy = y + AJ2_LN_GAP;
        if (ajPontLista
#ifdef NV_TOUCH_UI
            && !ajPontInline
#endif
           ) ponteiro_alvo_faixa(fr.x + 18.0f, yy, fr.w - 36.0f, lh, topo, base, ajPonteiroItem, NULL, i, 0);
        ajDesenharLinha(i, yy, animItem[i], ajMascara(yy + (lh > AJ2_LN_H + 0.5f ? AJ2_LN_LINHA1 * 0.5f : lh * 0.5f), topo, base) * k);
      }
      k = ka; triFilaDx = 0.0f;
    }
    y += h;
  }
  gfx_sem_recorte();
  gfx_opacidade_grupo = g0;
}

// --- ROTAS UTILITARIAS NA ILHA 2 (presets, revisao, parental) -------------------------------
// Uma linha generica: icone, titulo, apoio e, a direita, valor ou chevron.
static void triLinhaUtil(GfxRect r, float f, float a, const char *icone, const char *titulo, const char *apoio,
                         const char *valor, int chevron, int ligado) {
  float yc = r.y + r.h * 0.5f, xd = r.x + r.w - 22.0f, aT = a;
  TxtLinha t;
  if (a <= 0.004f) return;
  ajFocoLinha(r, 20.0f, f, a);
  if (icone) ajIcone(icone, r.x + 20.0f, yc - 13.0f, 26.0f, (f > 0.5f ? 0.9f : 0.6f) * aT);
  if (chevron) { aj2Chevron(xd, yc, (f > 0.5f ? 0.7f : 0.32f) * aT); xd -= 22.0f + 12.0f; }
  if (ligado >= 0) { xd -= aj2PilulaEstado(-1, xd, yc, ligado, aT) + 12.0f; }
  if (valor && valor[0]) {
    t = ajTxtC(TXT_V2_24, valor, r.w * 0.4f);
    txt_desenhar_alpha(t, xd - t.w, yc - t.h * 0.5f, (f > 0.5f ? 0.95f : 0.72f) * aT);
    xd -= t.w + 16.0f;
  }
  { float tx = r.x + (icone ? 62.0f : 22.0f), maxW = xd - tx - 10.0f;
    TxtLinha tt = ajTxtC(TXT_V2_LN, titulo, maxW);
    if (apoio && apoio[0]) {
      TxtLinha s = ajTxtC(TXT_V2_18, apoio, maxW);
      float th = tt.h + 4.0f + s.h;
      txt_desenhar_alpha(tt, tx, yc - th * 0.5f, (f > 0.5f ? 1.0f : 0.88f) * aT);
      txt_desenhar_alpha(s, tx, yc - th * 0.5f + tt.h + 4.0f, 0.5f * aT);
    } else {
      int longo = 0, it = triRotItem;
      if (it >= 0 && f > 0.5f) {
        if (aj2FocoNaLista(it)) ajRotModo[it] = (unsigned char)ajRotMedir(TXT_V2_LN, titulo, maxW);
        longo = ajRotDuas(it) ? 1 : ajRotModo[it];
      }
      if (longo == 1) txt_bloco_corta(TXT_V2_LN, titulo, 243, 242, 239, tx, yc - (AJ_ROT_LH + tt.h) * 0.5f, maxW, AJ_ROT_LH, aT, 2);
      else if (longo == 2) ajLetreiro(TXT_V2_LN, titulo, tx, yc, maxW, aT);
      else txt_desenhar_alpha(tt, tx, yc - tt.h * 0.5f, (f > 0.5f ? 1.0f : 0.88f) * aT);
    }
  }
}
// A linha em foco de uma rota utilitaria fica visivel: o desenho sabe onde ela
// esta (y sem a rolagem) e deixa o alvo; triAtualizar leva triUtilRol ate la.
static void triUtilRolar(float topoLinha, float baseLinha, float topo, float base, float total) {
  float alvo = triUtilRolAlvo;
  if (topoLinha >= 0.0f) {
    if (baseLinha - alvo > base) alvo = baseLinha - base;
    if (topoLinha - alvo < topo) alvo = topoLinha - topo;
  }
  triUtilRolAlvo = fmaxf(0.0f, fminf(alvo, fmaxf(0.0f, total - (base - topo))));
}
// O pe da lista das rotas: acima das pilulas de acao (triPilulas) com folga.
static float triUtilBase(void) { return TRI_BASE - 30.0f - 56.0f - 16.0f; }
// No celular a lista da rota rola pelo dedo (regiao vertical); `total` e a
// altura do conteudo, que o desenho sabe de antemao.
static float triUtilRolToque(GfxRect fr, float topo, float base, float total) {
#ifdef NV_TOUCH_UI
  if (ajRolagemLivre() && ajPontUtil)
    triUtilRol = ajToqueRegistrar(AJT_UTIL, (GfxRect){ fr.x, topo, fr.w, base - topo }, &triUtilRol, total - (base - topo), triUtilRol, triUtil);
#else
  (void)fr; (void)topo; (void)base; (void)total;
#endif
  return triUtilRol;
}
static void triUtilAlvoLinha(GfxRect r, float topo, float base, int i) {
  if (ajPontUtil) ponteiro_alvo_faixa(r.x, r.y, r.w, r.h, topo, base, triPontUtilLinha, NULL, i, 0);
}
// Duas pilulas de acao no pe da ilha (Aplicar / Manter; Restaurar tudo / Desfazer).
static float triPilulas(GfxRect ilha, const char *const *rot, const char *const *ic, int n, int foco, float a) {
  float x = ilha.x + 30.0f, y = ilha.y + ilha.h - 30.0f - 56.0f;
  int i;
  for (i = 0; i < n; i++) {
    float w = 22.0f + 22.0f + 12.0f + (float)txt_largura(TXT_V2_SEG, i18n(rot[i])) + 22.0f;
    GfxRect r = { x, y, w, 56.0f };
    int f = foco == i + 1;
    TxtLinha t = f ? ajTxtT(TXT_V2_SEG, rot[i]) : ajTxt(TXT_V2_SEG, rot[i]);
    if (ajPontUtil) ponteiro_alvo(r.x, r.y, r.w, r.h, triPontUtilAcao, NULL, i + 1, 0);
    if (f) ajPilulaAcento(r, 28.0f, a); else ajNeutro(r, 28.0f, 0.08f, AJ_CHIP_SOL, a);
    if (f) ajIconeT(ic[i], r.x + 22.0f, r.y + 17.0f, 22.0f, a); else ajIcone(ic[i], r.x + 22.0f, r.y + 17.0f, 22.0f, 0.8f * a);
    txt_desenhar_alpha(t, r.x + 56.0f, r.y + (56.0f - t.h) * 0.5f, (f ? 1.0f : 0.86f) * a);
    x += w + 12.0f;
  }
  return y;
}
static void triPresetsDesenhar(void) {
  GfxRect ilha = triIlha2R(), fr = triListaR();
  float topo = fr.y + 6.0f, base, y, fl0 = -1.0f, fl1 = -1.0f;
  int i, n;
  static const char *const rot[] = { "Salvar o estado atual" };
  static const char *const ic[] = { "aj_plus" };
  ajPenSavedLoad();
  n = TRI_PRESET_N + ajPenSavedCount;
  ajIlha(ilha, 28.0f, 1, 0);
  triListaCabecalho(ilha, "Predefinições", "Combos prontos ou os seus; nada muda sem o OK em Aplicar", 1);
  base = triPilulas(ilha, rot, ic, 1, triLinha == n ? 1 : 0, 1) - 16.0f;
  triUtilRolToque(fr, topo, base, 34.0f + n * 76.0f + 42.0f + (ajPenSavedCount ? 0.0f : 50.0f));
  gfx_recorte(fr.x, topo, fr.w, base - topo);
  y = topo - triUtilRol;
  ajKicker("Prontos", fr.x + 30.0f, y + 6.0f, 1); y += 34.0f;
  for (i = 0; i < n; i++) {
    GfxRect r = { fr.x + 18.0f, y, fr.w - 36.0f, 72.0f };
    if (i == TRI_PRESET_N) { ajKicker("Seus presets", fr.x + 30.0f, y + 14.0f, 1); y += 42.0f; r.y = y; }
    if (i == triLinha) { fl0 = r.y + triUtilRol; fl1 = fl0 + r.h; }
    triUtilAlvoLinha(r, topo, base, i);
    if (i < TRI_PRESET_N) triLinhaUtil(r, triUtilF[i], ajMascara(r.y + 36.0f, topo, base), TRI_PRESET_ICONE[i], AJ_PEN_PRESET_NAMES[i], TRI_PRESET_DESC[i], triPresetCombina(i) ? i18n("em uso") : "", 1, -1);
    else {
      char lang[96], size[96], resumo[240];
      int k = i - TRI_PRESET_N, audio = ajPenSavedValues[k][AJ_AUD_LINGUA], scale = ajPenSavedValues[k][AJ_LEG1_TAMANHO];
      uxValorTexto(AJ_AUD_LINGUA, audio >= 0 ? audio : uxValorAtual(AJ_AUD_LINGUA), lang, sizeof lang);
      uxValorTexto(AJ_LEG1_TAMANHO, scale >= 0 ? scale : uxValorAtual(AJ_LEG1_TAMANHO), size, sizeof size);
      snprintf(resumo, sizeof resumo, i18n("Áudio: %s · Legenda: %s"), lang, size);
      triLinhaUtil(r, triUtilF[i], ajMascara(r.y + 36.0f, topo, base), "aj_bookmark", ajPenSavedNames[k], resumo,
                   triLinha == i && triAcao ? i18n("Excluir") : i18n("Usar"), 1, -1);
    }
    y += 76.0f;
  }
  if (!ajPenSavedCount) {
    TxtLinha t = ajTxt(TXT_V2_24, "Nenhum preset salvo. OK em \"Salvar o estado atual\" guarda o de hoje.");
    ajKicker("Seus presets", fr.x + 30.0f, y + 14.0f, 1); y += 42.0f;
    txt_desenhar_alpha(t, fr.x + 40.0f, y + 10.0f, 0.5f);
    y += 50.0f;
  }
  gfx_sem_recorte();
  triUtilRolar(fl0, fl1, topo, base, y + triUtilRol - topo);
}
static void triRevisaoDesenhar(void) {
  GfxRect ilha = triIlha2R(), fr = triListaR();
  float topo = fr.y + 6.0f, base, y, fl0 = -1.0f, fl1 = -1.0f;
  int i;
  static const char *const rot[] = { "Aplicar mudanças", "Manter como está" };
  static const char *const ic[] = { "aj_check", "aj_x" };
  char sub[96];
  snprintf(sub, sizeof sub, i18n(rvN == 1 ? "%d ajuste muda" : "%d ajustes mudam"), rvN);
  ajIlha(ilha, 28.0f, 1, 0);
  triListaCabecalho(ilha, "Prévia da mudança", sub, 1);
  base = triPilulas(ilha, rot, ic, 2, triAcao, 1) - 16.0f;
  triUtilRolToque(fr, topo, base, rvN * 76.0f);
  gfx_recorte(fr.x, topo, fr.w, base - topo);
  y = topo - triUtilRol;
  for (i = 0; i < rvN; i++) {
    int op = rvOps[i];
    GfxRect r = { fr.x + 18.0f, y, fr.w - 36.0f, 72.0f };
    char antes[160], depois[160], troca[340];
    uxValorTexto(op, rvAntes[i], antes, sizeof antes);
    uxValorTexto(op, rvDepois[i], depois, sizeof depois);
    snprintf(troca, sizeof troca, "%s → %s", antes, depois);
    if (i == triLinha && !triAcao) { fl0 = r.y + triUtilRol; fl1 = fl0 + r.h; }
    triUtilAlvoLinha(r, topo, base, i);
    triLinhaUtil(r, triUtilF[i], ajMascara(r.y + 36.0f, topo, base), NULL, rotuloOpcao(op),
                 escopoDe(op) == ESC_TV ? "Só esta TV" : "A escolha acompanha este perfil.", troca, 0, -1);
    y += 76.0f;
  }
  if (!rvN) { TxtLinha t = ajTxt(TXT_V2_LN, "Nenhuma alteração"); txt_desenhar_alpha(t, fr.x + 40.0f, topo + 20.0f, 0.6f); }
  gfx_sem_recorte();
  triUtilRolar(fl0, fl1, topo, base, y + triUtilRol - topo);
}
static void triParentalLinha(int i, char *titulo, size_t nt, char *apoio, size_t na, char *valor, size_t nv, const char **icone, int *ligado) {
  int alvo = ajPenParentProfile();
  const ContaPerfil *p = perfis_item(alvo);
  const char *nome = p && p->nome[0] ? p->nome : "";
  *ligado = -1; valor[0] = 0; apoio[0] = 0;
  switch (i) {
    case TRI_P_INFANTIL: snprintf(titulo, nt, "%s", i18n("Perfil infantil")); snprintf(apoio, na, "%s", i18n("Marca o perfil em uso como infantil")); *icone = "aj_baby"; *ligado = perfil_infantil_ativo(); break;
    case TRI_P_TRAVAR: snprintf(titulo, nt, "%s", i18n("Ajustes travados")); snprintf(apoio, na, "%s", i18n("Só o PIN abre os ajustes")); *icone = "aj_lock"; *ligado = pfTravado() ? 1 : 0; break;
    case TRI_P_PIN: snprintf(titulo, nt, "%s", i18n("PIN do perfil")); snprintf(apoio, na, "%s", i18n("Um código de 4 dígitos")); *icone = "aj_key-round"; snprintf(valor, nv, "%s", i18n(admin_perfil_pin_definido(perfis_ativo()) ? "Definido" : "Sem PIN")); break;
    case TRI_P_REMOVER_PIN: snprintf(titulo, nt, "%s", i18n("Remover PIN")); snprintf(apoio, na, "%s", i18n("Pede o PIN de administrador")); *icone = "aj_lock-open"; break;
    case TRI_P_TEMPO: snprintf(titulo, nt, "%s", i18n("Tempo de tela por perfil")); snprintf(apoio, na, "%s", i18n("Quanto tempo o perfil pode usar")); *icone = "aj_alarm-clock"; ajPenParentText(PR_TEMPO, perfil_regra(alvo, PR_TEMPO), valor, nv); break;
    case TRI_P_IDADE: snprintf(titulo, nt, "%s", i18n("Classificação máxima")); snprintf(apoio, na, "%s", i18n("A idade mínima do perfil")); *icone = "aj_film"; ajPenParentText(PR_IDADE, perfil_regra(alvo, PR_IDADE), valor, nv); break;
    case TRI_P_BUSCA: snprintf(titulo, nt, "%s", i18n("Busca sem filtro")); *icone = "aj_search"; *ligado = perfil_regra(alvo, PR_BUSCA_LIVRE) ? 1 : 0; break;
    case TRI_P_COMPRAS: snprintf(titulo, nt, "%s", i18n("Compras bloqueadas")); snprintf(apoio, na, "%s", i18n("Não deixa comprar na loja")); *icone = "aj_shopping-bag"; *ligado = perfil_regra(alvo, PR_COMPRAS) ? 1 : 0; break;
    case TRI_P_FILMES: snprintf(titulo, nt, "%s", i18n("Filmes permitidos")); snprintf(apoio, na, "%s", i18n("O que o perfil pode abrir")); *icone = "aj_tv"; ajPenParentText(PR_FILMES, perfil_regra(alvo, PR_FILMES), valor, nv); break;
    default: snprintf(titulo, nt, "%s", i18n("Perfil")); snprintf(apoio, na, "%s", i18n("Pede o PIN de administrador")); *icone = "aj_users"; snprintf(valor, nv, "%s", nome); break;
  }
}
static void triParentalDesenhar(void) {
  GfxRect ilha = triIlha2R(), fr = triListaR();
  float topo = fr.y + 6.0f, base = AJ_BASE, y = topo - triUtilRol, fl0 = -1.0f, fl1 = -1.0f;
  int i;
  ajIlha(ilha, 28.0f, 1, 0);
  triListaCabecalho(ilha, "Controle parental", "PIN e limites por perfil", 1);
  triUtilRolToque(fr, topo, base, 28.0f + 34.0f + TRI_P_N * 76.0f);
  y = topo - triUtilRol;
  gfx_recorte(fr.x, topo, fr.w, base - topo);
  for (i = 0; i < TRI_P_N; i++) {
    GfxRect r = { fr.x + 18.0f, y, fr.w - 36.0f, 72.0f };
    char titulo[96], apoio[120], valor[96];
    const char *icone = NULL; int ligado;
    if (i == TRI_P_TEMPO) { ajKicker("Limites", fr.x + 30.0f, y + 8.0f, 1); y += 34.0f; r.y = y; }
    else if (i == 0) { ajKicker("Perfil infantil", fr.x + 30.0f, y + 2.0f, 1); y += 28.0f; r.y = y; }
    triParentalLinha(i, titulo, sizeof titulo, apoio, sizeof apoio, valor, sizeof valor, &icone, &ligado);
    if (i == triLinha) { fl0 = r.y + triUtilRol; fl1 = fl0 + r.h; }
    triUtilAlvoLinha(r, topo, base, i);
    triLinhaUtil(r, triUtilF[i], ajMascara(r.y + 36.0f, topo, base), icone, titulo, apoio, valor, ligado < 0, ligado);
    y += 76.0f;
  }
  gfx_sem_recorte();
  triUtilRolar(fl0, fl1, topo, base, y + triUtilRol - topo);
}
// A escolha de uma regra (tempo, idade, filmes) na ilha 3.
static void triParentalEscolha(GfxRect m) {
  int rule = ajPenParentEditor, i, n;
  const int *choices = rule == PR_TEMPO ? AJ_PEN_TIME : AJ_PEN_AGE;
  float y = m.y + 24.0f;
  n = rule == PR_TEMPO ? 7 : rule == PR_IDADE || rule == PR_FILMES ? 6 : 2;
  ajKicker("Controle parental", m.x + 24.0f, y, 1); y += 26.0f;
  { TxtLinha t = ajTxtC(TXT_V3_GRP, AJ_PEN_PARENT_LABEL[rule == PR_TEMPO ? 0 : rule == PR_IDADE ? 1 : rule == PR_BUSCA_LIVRE ? 2 : rule == PR_COMPRAS ? 3 : 4], m.w - 48.0f);
    txt_desenhar(t, m.x + 24.0f, y); y += t.h + 18.0f; }
  for (i = 0; i < n; i++) {
    int value = rule == PR_BUSCA_LIVRE || rule == PR_COMPRAS ? i : choices[i];
    int foco = value == ajPenParentDraft;
    GfxRect r = { m.x + 16.0f, y, m.w - 32.0f, 60.0f };
    char label[96];
    TxtLinha t;
    ajPenParentText(rule, value, label, sizeof label);
    ponteiro_alvo(r.x, r.y, r.w, r.h, triPontParentalValor, NULL, value, 0);
    if (foco) ajFocoLinha(r, 18.0f, 1, 1);
    t = ajTxtC(foco ? TXT_V3_LN_B : TXT_V3_LN, label, r.w - 40.0f);
    txt_desenhar_alpha(t, r.x + 20.0f, r.y + (r.h - t.h) * 0.5f, foco ? 1.0f : 0.8f);
    y += 64.0f;
  }
  { AjDica d[3] = { { "↑ ↓", "Escolher" }, { "OK", "Confirmar" }, { "Voltar", "Cancelar" } };
    aj2DicasLinha(d, 3, m.x + 24.0f, m.y + m.h - 36.0f, TXT_V2_KBD18, 34, 38, 10, TXT_V2_24, 8, 22, 1, 1); }
}
// O inspetor das rotas utilitarias: o que a rota faz, na ilha 3.
static void triUtilInspetor(GfxRect m) {
  const char *kick = "Ajustes", *tit = "", *txt = "";
  float y = m.y;
  if (triUtil == TRI_U_PRESETS) {
    tit = "Predefinições";
    txt = triLinha < TRI_PRESET_N ? "Um preset muda várias opções de uma vez. OK mostra a prévia do que muda antes de aplicar; nada é gravado sem o segundo OK."
        : triLinha < TRI_PRESET_N + ajPenSavedCount ? "Um preset seu guarda a qualidade, os idiomas e o estilo que estavam em uso quando foi salvo. Direita escolhe Excluir."
        : "Salva a qualidade, os idiomas e o estilo de hoje como um preset novo. Vale nesta TV e vai para a conta.";
  } else if (triUtil == TRI_U_REVISAO) {
    tit = "Prévia da mudança";
    txt = "Cada linha mostra o valor de agora e o que ele vira. Aplicar grava tudo de uma vez; Desfazer, em Diferente do padrão, volta exatamente estes valores.";
  } else if (triUtil == TRI_U_PARENTAL) {
    tit = "Controle parental";
    txt = triLinha == TRI_P_PERFIL ? "Escolhe para qual perfil os limites abaixo valem. Pede o PIN de administrador."
        : triLinha <= TRI_P_REMOVER_PIN ? "O PIN do perfil tranca os ajustes e a troca de perfil. Perfil infantil só vê classificação livre e até 10 anos."
        : "Os limites valem para o perfil escolhido em Perfil: tempo de uso por dia, idade máxima dos títulos, busca e compras.";
  }
  ajKicker(kick, m.x, y, 1); y += 26.0f;
  { TxtLinha t = ajTxtC(TXT_V2_A3TIT, tit, m.w); txt_desenhar(t, m.x, y); y += t.h + 16.0f; }
  ajBloco(TXT_V3_INSP, txt, m.x, y, m.w, 37.0f, 0.70f, 8);
}

// --- OS EDITORES COMPACTOS DA ILHA 3 ----------------------------------------------------
// Escolha: kicker, titulo, "Vale para · Padrao", a lista de valores (o salvo com
// o check), a ajuda da opcao e a pilula "Restaurar padrao". Numero: o valor
// grande com a unidade, - e +, o trilho com a marca do padrao, "o que muda".
static float triEditorSubY;   // a linha "Vale para · Padrao" (o "31 de 34" vai a direita dela)
static float triEditorCabecalho(GfxRect m) {
  float x = m.x + 24.0f, y = m.y + 24.0f, w = m.w - 48.0f;
  char k[200], linha[320], padrao[160];
  const char *b = uxBloco(uxOp);
  if (b[0]) snprintf(k, sizeof k, "%s · %s", i18n(uxCaminho(uxOp)), i18n(b));
  else snprintf(k, sizeof k, "%s", i18n(uxCaminho(uxOp)));
  ajKickerC(k, x, y, 1, w); y += 26.0f;
  y += ajBloco(TXT_V2_A3TIT, rotuloOpcao(uxOp), x, y, w, 50.0f, 1, 2) + 6.0f;
  if (ajPadraoVisual(uxOp) >= 0) {
    uxValorTexto(uxOp, ajPadraoVisual(uxOp), padrao, sizeof padrao);
    snprintf(linha, sizeof linha, i18n("%s · Padrão: %s"), i18n(uxEscopoValor(uxOp, uxPendente)), padrao);
  } else snprintf(linha, sizeof linha, "%s", i18n(uxEscopoValor(uxOp, uxPendente)));
  { TxtLinha t = ajTxtC(TXT_V2_24, linha, w - 90.0f);
    txt_desenhar_alpha(t, x, y, 0.55f); triEditorSubY = y; y += t.h + 20.0f; }
  return y;
}
static float triEditorBase(GfxRect m) { return m.y + m.h - 24.0f - (uxTemPadrao(uxOp) ? 56.0f + 20.0f : 0.0f); }
static void triEditorRodape(GfxRect m) {
  if (!uxTemPadrao(uxOp)) return;
  { TxtLinha t = uxRodape ? ajTxtT(TXT_V2_SEG, "Restaurar padrão") : ajTxt(TXT_V2_SEG, "Restaurar padrão");
    float w = 22.0f + 22.0f + 12.0f + t.w + 22.0f, y = m.y + m.h - 24.0f - 56.0f;
    GfxRect r = { m.x + 24.0f, y, w, 56.0f };
    if (ajPontEditor) ponteiro_alvo(r.x, r.y, r.w, r.h, ajPontEdRodape, NULL, 0, 0);
    if (uxRodape) ajPilulaAcento(r, 28.0f, 1); else ajNeutro(r, 28.0f, 0.08f, AJ_CHIP_SOL, 1);
    if (uxRodape) ajIconeT("aj_rotate-ccw-clock", r.x + 22.0f, y + 17.0f, 22.0f, 1);
    else ajIcone("aj_rotate-ccw-clock", r.x + 22.0f, y + 17.0f, 22.0f, 0.85f);
    txt_desenhar_alpha(t, r.x + 56.0f, y + (56.0f - t.h) * 0.5f, uxRodape ? 1.0f : 0.85f); }
}
static void triEditorEscolha(GfxRect m) {
  const Opcao *o = &OPCOES[uxOp];
  int n = nValores(uxOp), i, primeiro, vis, linhasAjuda = 3;
  float y = triEditorCabecalho(m), lx = m.x + 16.0f, lw = m.w - 32.0f, passo = 60.0f, base = triEditorBase(m), yAjuda;
  const char *ajuda = ajudaOpcao(uxOp);
  vis = (int)((base - y - linhasAjuda * 28.0f - 16.0f) / passo);
  if (vis < 3) vis = 3;
  if (vis > n) vis = n;
  primeiro = uxPendente - vis / 2;
  if (primeiro + vis > n) primeiro = n - vis;
  if (primeiro < 0) primeiro = 0;
  gfx_recorte(lx, y, lw, vis * passo);
  for (i = primeiro; i < n && i < primeiro + vis; i++) {
    char t[160];
    int foco = !uxRodape && i == uxPendente, indisp = uxOp == AJ_TEX_MB && texAcima(i), fv;
    GfxRect r = { lx, y, lw, passo - 4.0f };
    float tx = r.x + 20.0f, dir = r.x + r.w - 20.0f;
    if (uxOp == AJ_SALVOS_DEST && !ajustes_fonte_oferecida(i, trakt_ativo(), simkl_ativo())) { y += passo; continue; }
    uxValorTexto(uxOp, i, t, sizeof t);
    if (indisp) snprintf(t, sizeof t, "%s", i18n(o->valores[i]));
    if (ajPontEditor) ponteiro_alvo(r.x, r.y, r.w, r.h, indisp ? NULL : ajPontEdValor, NULL, i, 0);
    if (foco) ajFocoLinha(r, 18.0f, 1, 1);
    if (uxOp == AJ_ICONE_APP) { iconeapp_desenhar(i, (GfxRect){ r.x + 12.0f, r.y + 6.0f, 44.0f, 44.0f }, 1, foco ? 1.0f : 0.7f); tx = r.x + 70.0f; }
    if (uxOp == AJ_LOGO_APP) { logoapp_miniatura(i, (GfxRect){ r.x + 12.0f, r.y + 6.0f, 44.0f, 44.0f }, 1, foco ? 1.0f : 0.7f); tx = r.x + 70.0f; }
    if (i == uxOriginal) { ajIcone("aj_check", dir - 24.0f, r.y + (r.h - 24.0f) * 0.5f, 24.0f, 0.8f); dir -= 36.0f; }
    fv = marcaDoValor(uxOp, o->valores && i < o->n ? o->valores[i] : NULL);
    if (fv >= 0) marca_formato((FormatoMarca)fv, tx, r.y + (r.h - 30.0f) * 0.5f, 30.0f, 1, 1, 1, foco ? 0.9f : 0.6f);
    else {
      TxtLinha l = ajTxtC(foco ? TXT_V3_LN_B : TXT_V3_LN, t, dir - tx - 8.0f);
      txt_desenhar_alpha(l, tx, r.y + (r.h - l.h) * 0.5f, indisp ? 0.4f : foco ? 1.0f : 0.82f);
    }
    y += passo;
  }
  gfx_sem_recorte();
  yAjuda = y + 12.0f;
  if (n > vis) {   // "31 de 34", na linha do kicker, a direita
    char c[48];
    TxtLinha t;
    snprintf(c, sizeof c, i18n("%d de %d"), uxPendente + 1, n);
    t = ajTxt(TXT_V2_18, c);
    txt_desenhar_alpha(t, m.x + m.w - 24.0f - t.w, triEditorSubY + 4.0f, 0.42f);
  }
  if (ajuda && ajuda[0] && yAjuda + 28.0f <= base)
    ajBloco(TXT_V2_18, ajuda, m.x + 24.0f, yAjuda, m.w - 48.0f, 28.0f, 0.55f, (int)((base - yAjuda) / 28.0f) > linhasAjuda ? linhasAjuda : (int)((base - yAjuda) / 28.0f));
  triEditorRodape(m);
}
static void triEditorNumero(GfxRect m) {
  const Opcao *o = &OPCOES[uxOp];
  float y = triEditorCabecalho(m), x = m.x + 24.0f, w = m.w - 48.0f, ar, ag, ab, base = triEditorBase(m);
  char v[64], salvo[64], uni[64], sv[96], t[64];
  float fr = ajMovFracao(uxOp, uxPendente), fp = ajPadraoVisual(uxOp) >= 0 ? ajFracao(uxOp, ajPadraoVisual(uxOp)) : -1.0f;
  GfxRect r = { x, y, w, 196.0f };
  float tx = r.x + 24.0f, tw = r.w - 48.0f, cy = r.y + 62.0f, ty = r.y + 134.0f;
  TxtLinha big, u, ls, l;
  const char *ef = efeitoOpcao(uxOp);
  ajustes_acento(&ar, &ag, &ab);
  snprintf(v, sizeof v, "%d", uxPendente);
  if (uxOp == AJ_HERO_TRAILER_ESPERA) snprintf(v, sizeof v, "%d,%d", uxPendente / 10, uxPendente % 10);
  uxValorTexto(uxOp, uxOriginal, salvo, sizeof salvo);
  snprintf(uni, sizeof uni, "%s", o->sufixo ? o->sufixo + (o->sufixo[0] == ' ') : "");
  if (ajVid()) gfx_cor(r, 24.0f / r.h, 1, 1, 1, uxRodape ? 0.04f : 0.10f);
  else ajFocoLinha(r, 24.0f, uxRodape ? 0 : 1, 1);
  big = ajTxt(TXT_V2_TIT, v);
  u = ajTxt(TXT_V2_36B, uni);
  txt_desenhar(big, tx, cy - big.h * 0.5f);
  txt_desenhar_alpha(u, tx + big.w + 10.0f, cy - u.h * 0.5f + 6.0f, 0.8f);
  snprintf(sv, sizeof sv, i18n("salvo: %s"), salvo);
  ls = ajTxtC(TXT_V2_18, sv, tw - 120.0f);
  txt_desenhar_alpha(ls, tx, cy + big.h * 0.5f + 2.0f, 0.5f);
  if (ajPontEditor) {
    ponteiro_alvo(r.x, r.y, r.w, r.h, ajPontEdNumero, NULL, 0, 0);
    ponteiro_alvo(r.x + r.w - 24.0f - 48.0f, cy - 28.0f, 56.0f, 56.0f, NULL, ajPontEdPasso, 1, 0);
    ponteiro_alvo(r.x + r.w - 24.0f - 104.0f, cy - 28.0f, 56.0f, 56.0f, NULL, ajPontEdPasso, -1, 0);
  }
  ajNeutro((GfxRect){ r.x + r.w - 24.0f - 48.0f, cy - 24.0f, 48.0f, 48.0f }, 24.0f, 0.08f, AJ_CHIP_SOL, 1);
  ajNeutro((GfxRect){ r.x + r.w - 24.0f - 104.0f, cy - 24.0f, 48.0f, 48.0f }, 24.0f, 0.08f, AJ_CHIP_SOL, 1);
  ajIcone("aj_plus", r.x + r.w - 24.0f - 48.0f + 12.0f, cy - 12.0f, 24.0f, 0.85f);
  ajIcone("aj_minus", r.x + r.w - 24.0f - 104.0f + 12.0f, cy - 12.0f, 24.0f, 0.85f);
  gfx_cor((GfxRect){ tx, ty, tw, 8 }, 0.5f, 1, 1, 1, 0.12f);
  if (fr > 0) gfx_cor((GfxRect){ tx, ty, tw * (fr > 1 ? 1 : fr), 8 }, 0.5f, ar, ag, ab, 1);
  if (fp >= 0) gfx_cor((GfxRect){ tx + tw * fp - 1.5f, ty - 9.0f, 3, 26 }, 0.5f, 0.953f, 0.949f, 0.937f, 0.6f);
  snprintf(t, sizeof t, "%d", o->min); l = ajTxt(TXT_V2_18, t); txt_desenhar_alpha(l, tx, ty + 20.0f, 0.5f);
  snprintf(t, sizeof t, "%d", o->max); l = ajTxt(TXT_V2_18, t); txt_desenhar_alpha(l, tx + tw - l.w, ty + 20.0f, 0.5f);
  y += r.h + 20.0f;
  if (ef && ef[0] && y + 28.0f * 2 <= base) {
    TxtLinha lp = ajTxt(TXT_V3_INSP_B, "O que muda:");
    txt_desenhar_alpha(lp, x, y, 0.95f); y += lp.h + 4.0f;
    ajBloco(TXT_V2_18, i18n(ef), x, y, w, 28.0f, 0.6f, (int)((base - y) / 28.0f) > 5 ? 5 : (int)((base - y) / 28.0f));
  }
  triEditorRodape(m);
}
static void triEditor(GfxRect m) {
  ajIlha(m, 28.0f, 1, 0);
  if (OPCOES[uxOp].tipo == OP_NUMERO) triEditorNumero(m);
  else triEditorEscolha(m);
}

// --- O PALCO: a pilha de blades, o menu condensado e o estagio --------------------------
// O clique numa amostra da paleta so PREVE (o ponteiro entrega o OK em seguida,
// como em toda linha): o valor grava no OK, nunca no foco.
static void triPontSwatch(int v, int b) {
  (void)b;
  if (uxEditor == 1 && ajPaleta(uxOp)) uxPendente = v;
}
// Amostra de uma dinamica (Dinamicas/Textura): TEMA_ACENTO dela nao tem cor
// (o disco saia preto, vazio). O disco leva a cor que ela tiraria da arte agora
// (aj2CorDin, como a regua antiga) e o icone do que ela faz.
static void triBolinhaDin(int v, float cx, float cy, int foco, int gravada) {
  static const char *const IC[5] = { "aj_pipette", "aj_blend", "aj_spotlight", "aj_paint-roller", "aj_feather" };
  const char *ic = v == AJ_TEMA_DINAMICA ? IC[0] : v == AJ_TEMA_GRADIENTE ? IC[1] : v == AJ_TEMA_IMERSIVA ? IC[2] :
                   v == AJ_TEMA_TEXTURA ? IC[3] : IC[4];
  float c1[3], c2[3], k, d = (foco ? 66.0f : 56.0f) * aj2BolinhaK, s = 26.0f * aj2BolinhaK;
  int i;
  aj2CorDin(v, ajFundoArte(), c1, c2);
  if (v == AJ_TEMA_GRADIENTE) for (i = 0; i < 3; i++) c1[i] = (c1[i] + c2[i]) * 0.5f;
  if (foco) {
    gfx_cor((GfxRect){ cx - d * 0.5f - 6, cy - d * 0.5f - 6, d + 12, d + 12 }, 0.5f, 0.953f, 0.949f, 0.937f, 0.92f);
    gfx_cor((GfxRect){ cx - d * 0.5f - 3.5f, cy - d * 0.5f - 3.5f, d + 7, d + 7 }, 0.5f, 0.055f, 0.059f, 0.071f, 1);
  }
  gfx_cor((GfxRect){ cx - d * 0.5f, cy - d * 0.5f, d, d }, 0.5f, c1[0], c1[1], c1[2], 1);
  k = 0.2126f * c1[0] + 0.7152f * c1[1] + 0.0722f * c1[2] > 0.6f ? 18.0f / 255.0f : 1.0f;
  gfx_icone((GfxRect){ cx - s * 0.5f, cy - s * 0.5f, s, s }, gravada ? "aj_check" : ic, k, k, k, 1);
}
// O BLADE DA PALETA (prof. 3, mockup claude-c): as fileiras de acentos, a
// amostroca em foco cresce, a gravada leva o visto; embaixo o nome e a ajuda.
static void triPaletaBlade(GfxRect m) {
  static const char *const KICK[4][2] = {
    { "Claros", "Tinta escura" }, { "Profundos", "Tinta branca" },
    { "Dinâmicas", "Só nesta TV" }, { "Textura", "Do título" } };
  int sel = valor[uxOp], alvo = uxPendente, filas = uxOp == AJ_FUNDO_DIN_COR ? 2 : 4;
  int f, k, n;
  float x = m.x + 30.0f, w = m.w - 60.0f, y = m.y + 26.0f;
  char kbuf[96];
  // Cabecalho do blade: o caminho (secao · topico) e o titulo da opcao.
  { const char *b = uxBloco(uxOp);
    if (b[0]) snprintf(kbuf, sizeof kbuf, "%s · %s", i18n(uxCaminho(uxOp)), i18n(b));
    else snprintf(kbuf, sizeof kbuf, "%s", i18n(uxCaminho(uxOp)));
    ajCaps(TXT_V2_KICK, kbuf, 2.52f, x, y, 0.45f); y += 22.0f;
    { TxtLinha t = ajTxtC(TXT_V3_NOME, rotuloOpcao(uxOp), w);
      txt_desenhar(t, x, y); y += t.h + 26.0f; } }
  for (f = 0; f < filas; f++) {
    const int *L = ajReguaFila(uxOp, f, &n);
    snprintf(kbuf, sizeof kbuf, "%s · %s", i18n(KICK[f][0]), i18n(KICK[f][1]));
    ajCaps(TXT_AJ_CAPS13, kbuf, 1.82f, x, y, 0.45f);
    y += 22.0f;
    { float d = fminf(64.0f, (w - 8.0f * (float)(n - 1)) / (float)(n > 0 ? n : 1)), lx = x;
      aj2BolinhaK = d / 64.0f;
      for (k = 0; k < n; k++) {
        int v = L[k], foco = v == alvo;
        float cx = lx + d * 0.5f, cy = y + d * 0.5f;
        GfxRect r = { lx, y, d, d };
        ponteiro_alvo(r.x, r.y, r.w, r.h, triPontSwatch, NULL, v, 0);
#ifdef NV_TOUCH_UI
        if (f < 2) ajToqueEscolhaAlvo(r, v, uxOp, AJ_TOPO, AJ_BASE);
#endif
        if (f >= 2 && uxOp != AJ_FUNDO_DIN_COR) triBolinhaDin(v, cx, cy, foco, v == sel && !foco);
        else aj2Bolinha(uxOp == AJ_FUNDO_DIN_COR ? FUNDO_DIN_TEMA[v] : v, cx, cy, foco, v == sel && !foco, 1);
        lx += d + 8.0f;
      }
      aj2BolinhaK = 1.0f;
      y += d + 22.0f; }
  }
  // O nome da cor em foco (a pendente) e a ajuda da opcao.
  { TxtLinha t = ajTxtC(TXT_V2_LN_B, i18n(V_TEMA[uxOp == AJ_FUNDO_DIN_COR ? FUNDO_DIN_TEMA[alvo] : alvo]), w);
    txt_desenhar(t, x, y); y += t.h + 10.0f; }
  ajBloco(TXT_V2_18, ajudaOpcao(uxOp), x, y, w, 28.0f, 0.55f, 3);
}
// O realce leve da linha que abriu o blade filho, desenhado no blade pai.
static void triBladeRealce(int item, float a) {
  GfxRect fr = triListaR();
  float y = AJ_TOPO - scrollY;   // the parent blade's own scroll (triPalcoDesenhar)
  int i;
  if (item <= secIni[secAtual] || item >= secFim(secAtual)) return;
  gfx_recorte(fr.x, AJ_TOPO, fr.w, AJ_BASE - AJ_TOPO);
  for (i = secIni[secAtual] + 1; i < item; i++) y += ajAlturaItem(i);
  { GfxRect r = { fr.x + 18.0f, y + AJ2_LN_GAP, fr.w - 36.0f, ajLinhaH(item) };
    ajFocoLinha(r, 20.0f, 0.4f, a); }
  gfx_sem_recorte();
}
// --- O ESTAGIO DO PALCO (mockup claude-c) -------------------------------------------------
// A cena grande, a legenda (kicker, titulo 44, ajuda 24/34 em 3 linhas) e, numa
// opcao, os fatos Agora / Padrao / Vale para; no indice, os chips dos blocos.
// Nada aqui e alvo: o estagio so mostra.
// Opcoes de cor/interface: a Home de verdade com o acento (a pendente, editando).
static int triCenaHome(int sec, int op) {
  return op == AJ_TEMA || op == AJ_COR_LOGO || op == AJ_INTERFACE_MODO || (op < 0 && sec == AJS_APARENCIA);
}
// op -2: indice. Cada secao usa sua cena gerada no corte largo do estagio;
// cartaz de filme nao explica categorias como Reproducao ou Conta.
static int triCenaChave(int sec, int op) { return op == -2 ? 3000 + sec : triCenaHome(sec, op) ? -100 : op >= 0 ? op : 2000 + sec; }
static float triCenaPintar(int sec, int op, float x, float y, float w) {
  int salvo = op >= 0 ? valor[op] : 0;
  float h;
  if (op == -2) return ajVisualCena(sec, -1, x, y, w);
  if (op >= 0 && uxEditor == 1 && uxOp == op && op != AJ_FUNDO) valor[op] = uxPendente;   // so desenho
  if (triCenaHome(sec, op)) h = ajPreviaHome(x, y, w);
  else if (op < 0 && sec == AJS_REPRODUCAO) { h = ajc_comecar(x, y, w, 1.0f); ajcCenaPlayer(-1); ajc_fim(); }
  else if (op < 0) h = ajVisualCena(sec, -1, x, y, w);
  else { ajMiniRotuloV2 = 1; h = aj2Previa(op, x, y, w); ajMiniRotuloV2 = 0; }
  if (op >= 0) valor[op] = salvo;
  return h;
}
static float triEstagioBorda;   // o estagio comeca onde o blade mais a direita termina (ele empurra)
static void triEstagio(void) {
  GfxRect p = triPalcoR();
  if (triEstagioBorda > p.x) p.x = triEstagioBorda;   // empurrado pelo blade que entra/sai
  int indice = focoIndice || triIlha == TRI_SECOES || triIlha == TRI_CHIPS;
  int parental = indice && triParental(triSecFoco);
  int sec = secAtual, op = -1, topico = -1, naMais = 0, chave;
  float g0 = gfx_opacidade_grupo, x = p.x, w = p.w, y, ld, la, sh = p.w * 9.0f / 16.0f;
  char kick[200], ajuda[400];
  const char *tit;
  const char *aj = "";
  if (parental) sec = AJS_CONTAS;   // o parental mora na conta: a cena dela
  if (indice) op = parental ? -1 : -2;
  else {
    if (uxEditor == 1 && uxOp >= 0) op = uxOp;
    else if (focoOp >= 0) op = focoOp;
    else if (triTopico(focoItem)) topico = focoItem;
    else naMais = TELA[focoItem].tipo == IT_MAIS;
  }
  // A CENA: trocou, cruza pela mola (a velha some crescendo 3%, a nova entra
  // de 1,04 com 28 px de paralaxe). O corte largo do indice centra a cena 16:9.
  if (triCenaChave(triCenaSec[0], triCenaOp[0]) != triCenaChave(sec, op) || triCenaSec[0] == -2) {
    if (triCenaSec[0] != -2) { triCenaSec[1] = triCenaSec[0]; triCenaOp[1] = triCenaOp[0]; triCenaF = 0.0f; }
    triCenaSec[0] = sec; triCenaOp[0] = op;
  }
  gfx_rect((GfxRect){ p.x - 10.0f, p.y + 8.0f, p.w + 20.0f, p.h + 40.0f }, 0, GFX_SOMBRA, 1, 0, 0, 0.5f, 0, 0, 0, 0.45f * g0);
  { GfxRecorteAnt ra = gfx_recorte_dentro(p.x, p.y, p.w, p.h);
    float cx = p.x + p.w * 0.5f, cy = p.y + p.h * 0.5f, F = triCenaF, yc = p.y - (sh - p.h) * 0.5f;
    if (F < 0.999f && triCenaSec[1] != -2) {
      gfx_opacidade_grupo = g0 * (1.0f - F);
      gfx_transformar(cx, cy, 1.0f + 0.03f * F, -18.0f * F, 0.0f);
      triCenaPintar(triCenaSec[1], triCenaOp[1], x, yc, w);
      gfx_sem_transformar();
    }
    gfx_opacidade_grupo = g0 * (F < 0.999f ? F : 1.0f);
    if (F < 0.999f) gfx_transformar(cx, cy, 1.04f - 0.04f * F, 28.0f * (1.0f - F), 0.0f);
    triCenaPintar(sec, op, x, yc, w);
    if (F < 0.999f) gfx_sem_transformar();
    gfx_opacidade_grupo = g0;
    gfx_recorte_voltar(ra); }
  // A LEGENDA sobe 22 px a cada troca de foco (mola de tela).
  chave = indice ? -1000 - (parental ? 99 : sec) : focoItem * 4 + (uxEditor == 1);
  if (chave != triLegChave) { if (triLegChave != -1) triLegF = 0.0f; triLegChave = chave; }
  ld = 22.0f * (1.0f - triLegF); la = triLegF;
  y = p.y + p.h + 34.0f;
  kick[0] = ajuda[0] = 0;
  if (indice) {
    snprintf(kick, sizeof kick, "%s", i18n(triGrupoNome(parental ? 2 : triGrupo(sec))));
    tit = i18n(parental ? "Controle parental" : TELA[secIni[sec]].titulo);
    if (!parental) aj = SECAO_AJUDA[sec];
  } else if (op >= 0) {
    const char *bl = uxBloco(op);
    if (bl[0]) snprintf(kick, sizeof kick, "%s · %s", i18n(TELA[secIni[sec]].titulo), i18n(uxAvancada(op) ? "Mais opções" : bl));
    else snprintf(kick, sizeof kick, "%s", i18n(TELA[secIni[sec]].titulo));
    tit = rotuloOpcao(op); aj = uxResumo(op);
  } else {
    snprintf(kick, sizeof kick, "%s", i18n(TELA[secIni[sec]].titulo));
    if (topico >= 0) {   // o bloco: o titulo e o que ha dentro
      size_t u = 0;
      int i;
      tit = i18n(TELA[topico].titulo);
      for (i = topico + 1; i < secFim(sec) && TELA[i].tipo == IT_OPC && u + 4 < sizeof ajuda; i++)
        if (visivel(i)) u += (size_t)snprintf(ajuda + u, sizeof ajuda - u, "%s%s", u ? ", " : "", rotuloOpcao(TELA[i].op));
      aj = ajuda;
    } else if (naMais) { tit = i18n("Mais opções"); maisContar(sec, ajuda, sizeof ajuda); aj = ajuda; }
    else { tit = i18n(TELA[secIni[sec]].titulo); aj = SECAO_AJUDA[sec]; }
  }
  ajCaps(TXT_V2_KICK, kick, 2.52f, x + 2.0f, y + ld, 0.45f * la);
  y += 22.0f + 12.0f;
  if (op == AJ_TEMA && uxEditor == 1 && uxOp == op) tit = i18n(V_TEMA[uxPendente]);   // a paleta aberta: o nome da cor
  y += ajBloco(TXT_LOG_N44, tit, x, y + ld, w, 50.0f, la, 2) + 10.0f;
  if (aj && aj[0]) y += ajBloco(TXT_V2_24, aj, x, y + ld, w, 34.0f, 0.70f * la, 3) + 18.0f;
  if (indice && !parental) {
    // Os chips dos blocos da secao (so os de apresentacao; os tecnicos ficam em "Mais opcoes").
    float cxp = x;
    int j;
    for (j = secIni[sec] + 1; j < secFim(sec) && TELA[j].tipo != IT_MAIS; j++) {
      if (TELA[j].tipo != IT_ROT) continue;
      { TxtLinha t = ajTxtC(TXT_V2_KICK, i18n(TELA[j].titulo), 260.0f);
        GfxRect r = { cxp, y + ld, t.w + 36.0f, 44.0f };
        if (r.x + r.w > x + w) break;
        ajNeutro(r, 22.0f, 0.08f, AJ_CHIP_SOL, la);
        txt_desenhar_alpha(t, r.x + 18.0f, r.y + (44.0f - t.h) * 0.5f, 0.86f * la);
        cxp += r.w + 10.0f; }
    }
  } else if (op >= 0 && uxTemPadrao(op)) {
    // OS FATOS: o "Agora" rola quando o valor muda (o novo sobe de 20 px).
    static const char *const ROT[3] = { "Agora", "Padrão", "Vale para" };
    char v[3][96];
    int vi = uxEditor == 1 && uxOp == op ? uxPendente : uxValorAtual(op), c, fk = op * 256 + (vi & 255);
    float fx = x;
    if (fk != triFatoChave) { if (triFatoChave != -1 && triFatoChave / 256 == op) triFatoF = 0.0f; triFatoChave = fk; }
    uxValorTexto(op, vi, v[0], sizeof v[0]);
    uxValorTexto(op, uxValorPadrao(op), v[1], sizeof v[1]);
    snprintf(v[2], sizeof v[2], "%s", i18n(uxEscopo(op)));
    for (c = 0; c < 3; c++) {
      TxtLinha l = ajTxt(TXT_ILHA_SUB, i18n(ROT[c])), b = ajTxtC(TXT_ILHA_SECAO, v[c], 320.0f);
      float r = c == 0 ? triFatoF : 1.0f, cw = fmaxf(l.w, b.w);
      if (fx + cw > x + w) break;
      txt_desenhar_alpha(l, fx, y + ld, 0.55f * la);
      txt_desenhar_alpha(b, fx, y + ld + 26.0f + 20.0f * (1.0f - r), la * r);
      fx += cw + 40.0f;
    }
  }
}

// A TELA DO PALCO: o menu (condensa na prof. 2), a pilha de blades e o
// estagio. Cada nivel entra pela mola da profundidade (da direita, 0,96 -> 1
// e fade); o que fechou continua desenhado (fantasma) ate a mola o levar; na
// prof. 3 o mais antigo sai pela esquerda (triBladeA) com a tira deslizando.
static void triPalcoDesenhar(int modal, float g0) {
  int n = triProfundidade(), ate = n, k, dono = n - 1;
  int sm = triSubmenu, pl = ajPontLista, ue = uxEditor, uo = uxOp, up = uxPendente, uu = triUtil;
  float sc = scrollY;
  while (ate < 4 && triProfF - (float)ate > 0.01f) ate++;
  // O limite acompanha a presenca do blade, inclusive ao inverter a entrada.
  // O fantasma continua por cima do estagio enquanto desaparece.
  triEstagioBorda = 0.0f;
  for (k = 0; k < ate; k++) {
    float e = triClamp(triProfF - (float)k, 0.0f, 1.0f);
    GfxRect b;
    if (triBladeA(k) * e <= 0.0f || !triPgTipo[k]) continue;
    triNivelDesenho = k; b = triIlha2R();
    { float borda = b.x + 80.0f * (1.0f - e) + b.w * (0.96f + 0.04f * e) + 16.0f;
      float x = triPalcoR().x;
      triEstagioBorda = fmaxf(triEstagioBorda, x + fmaxf(0.0f, borda - x) * e * triBladeA(k)); }
  }
  triNivelDesenho = -1;
  if (triTemInsp() && !(ajPenParentEditor >= 0 || (uxEditor == 1 && uxOp >= 0 && triEditorLargo(uxOp))) && !triUtil) triEstagio();
  // A superficie de foco mora no blade da lista em foco; com o editor aberto
  // por cima, fica na linha que o abriu (o pai), sem trocar de dono.
  if (n >= 2 && triPgTipo[n - 1] == TRI_PG_ED) dono = n - 2;
  for (k = 0; k < ate; k++) {
    float e = triClamp(triProfF - (float)k, 0.0f, 1.0f), a = triBladeA(k) * e;
    int tipo = triPgTipo[k], val = triPgVal[k], topo = k == n - 1, fant = k >= n, mov;
    GfxRect b;
    if (a <= 0.01f || !tipo) continue;
    triNivelDesenho = k;
    b = triIlha2R();
    mov = e < 0.999f && !ajustes_animacoes_reduzidas();
    gfx_opacidade_grupo = g0 * a;
    if (mov) gfx_transformar(b.x, b.y + b.h * 0.5f, 0.96f + 0.04f * e, 80.0f * (1.0f - e), 0.0f);
    ajPontLista = topo ? pl : 0;
    ajFocoCompart = k == dono && !fant; ajSupNivel = k;
    // Each list blade keeps its own scroll: the live list (dono with an editor
    // blade on top) uses scrollY, its parents the scroll they had.
    if (tipo == TRI_PG_SEC || tipo == TRI_PG_SUB) { if (!topo && k != dono) scrollY = triPgRol[k]; }
    else if (!topo) scrollY = 0;
    if (tipo == TRI_PG_UTIL) {
      triUtil = val;
      if (triUtil == TRI_U_DIF) { ajIlha(b, 28.0f, 1, 0); triListaCabecalho(b, "Diferente do padrão", uxNDifs ? "" : "Tudo está no padrão", 1); aj2DesenharDiferencas();
        { static const char *const rot[] = { "Restaurar tudo", "Desfazer" }; static const char *const ic[] = { "aj_rotate-ccw", "aj_corner-up-left" };
          triPilulas(b, rot, ic, 1 + (rvUndoN > 0), triAcao, 1); } }
      else if (triUtil == TRI_U_PRESETS) triPresetsDesenhar();
      else if (triUtil == TRI_U_REVISAO) triRevisaoDesenhar();
      else if (triUtil == TRI_U_PARENTAL) triParentalDesenhar();
    } else if (tipo == TRI_PG_ED) {
      // O editor de escolha/paleta e um BLADE (o fantasma lembra a pendente).
      uxEditor = 1; uxOp = val; uxPendente = fant ? triPgPend[k] : up;
      if (topo && (modal || ue)) ponteiro_camada();
      if (ajPaleta(uxOp)) { ajIlha(b, 28.0f, 1, 0); triPaletaBlade(b); }
      else triEditor(b);
    } else {
      triSubmenu = tipo == TRI_PG_SUB ? val : -1;
      ajFocoMudo = k != dono;   // parents/ghosts: no per-row focus plate (realce / the shared surface mark the row)
      triLista();
      ajFocoMudo = 0;
      // A linha que abriu o filho leva o realce leve no pai.
      if (!topo && !fant && k != dono && k + 1 < n) triBladeRealce(triPgTipo[k + 1] == TRI_PG_SUB ? triPgVal[k + 1] : focoItem, 0.55f);
    }
    triSubmenu = sm; uxEditor = ue; uxOp = uo; uxPendente = up; triUtil = uu;
    scrollY = sc; ajPontLista = pl; ajFocoCompart = 0;
    if (mov) gfx_sem_transformar();
    gfx_opacidade_grupo = g0;
  }
  triNivelDesenho = -1;
  // O ESTAGIO: flutua a direita, sem ilha e sem controle. O editor LARGO
  // (categorias do guia, pendrive, contas) continua em ilha de 760 sobre ele.
  if (triTemInsp()) {
    GfxRect m = triInspR();
    if (ajPenParentEditor >= 0) { ponteiro_camada(); ajIlha(m, 28.0f, 1, 0); triParentalEscolha(m); }
    else if (uxEditor == 1 && uxOp >= 0 && triEditorLargo(uxOp)) {
      float ke = anim_suave(triEditorF);
      if (modal || uxEditor) ponteiro_camada();
      gfx_opacidade_grupo = g0 * ke;
      gfx_transformar(m.x, m.y, 1.0f, 0.0f, 16.0f * (1.0f - ke));
      aj2DesenharEditor();
      gfx_sem_transformar();
    } else {
      if (triUtil == TRI_U_DIF) aj2InspetorDiferencas();
      else if (triUtil) triUtilInspetor(triInspMiolo());
    }
    gfx_opacidade_grupo = g0;
  }
}

// --- A TELA --------------------------------------------------------------------------
static void triRodape(void) {
  AjDica d[4];
  int nd;
  float x = triX0(), yc = NV_VTELA_H - 46.0f, xd = triXFim();
  char rota[260];
#ifdef NV_TOUCH_UI
  // Celular editando na linha: no lugar das dicas do controle, os botoes
  // Confirmar/Cancelar do 2.0.3 (aj2RodapeA3), alvos de toque de verdade.
  if (ajPontInline) { ajToqueBotoes(xd, yc, 1.0f); return; }
#endif
  if (ajTelaCheia()) return;
#ifdef NV_DESKTOP_UI
  if (layout_desktop_ativo()) {
    snprintf(rota, sizeof rota, "%s · %s", i18n("Ajustes"), i18n(TELA[secIni[secAtual]].titulo));
    TxtLinha t = ajTxt(TXT_G18R, uxAviso[0] ? uxAviso : rota);
    txt_desenhar_alpha(t, xd - t.w, NV_VTELA_H - 28, .45f);
    return;
  }
#endif
  if (triBladeLayout()) {
    // Palco: SEM barra de dicas (o mockup claude-c nao tem; as teclas moram no
    // proprio controle). So o aviso "Ajuste salvo..." quando existe.
    if (uxAviso[0] && !uxEditor) {
      TxtLinha t = ajTxtC(TXT_V2_24, uxAviso, 700.0f);
      ajIcone("aj_check", xd - t.w - 34.0f, yc - 11.0f, 22.0f, 0.7f);
      txt_desenhar_alpha(t, xd - t.w, yc - t.h * 0.5f, 0.75f);
    }
    return;
  }
  if (uxEditor) { d[0] = (AjDica){ "OK", "Confirmar" }; d[1] = (AjDica){ "Voltar", "Cancelar" }; nd = 2; }
  else if (triUtil) { d[0] = (AjDica){ "OK", "Abrir" }; d[1] = (AjDica){ "↑ ↓", "Navegar" }; d[2] = (AjDica){ "Voltar", "Sair" }; nd = 3; }
  else if (triIlha == TRI_SECOES) { d[0] = (AjDica){ "OK", "Entrar" }; d[1] = (AjDica){ "↑ ↓", "Navegar" }; d[2] = (AjDica){ "Voltar", "Sair" }; nd = 3; }
  else if (triIlha == TRI_CHIPS) { d[0] = (AjDica){ "OK", "Abrir" }; d[1] = (AjDica){ "← →", "Escolher" }; d[2] = (AjDica){ "↓", "Seções" }; nd = 3; }
  else if (triTopico(focoItem)) { d[0] = (AjDica){ "OK", "Abrir" }; d[1] = (AjDica){ "↑ ↓", "Navegar" }; d[2] = (AjDica){ "Voltar", "Categorias" }; nd = 3; }
  else { nd = ajDicasDe(focoOp, d); if (triSubmenu >= 0) d[nd - 1] = (AjDica){ "Voltar", "Voltar" }; }
  aj2DicasLinha(d, nd, x, yc, TXT_V2_KBD18, 34, 38, 10, TXT_V2_24, 8, 22, 1, 1);
  // Portrait reserves the footer for controls; the fixed header shows context.
  if (ajRetrato()) return;
  // A direita: onde a pessoa esta ("Reproducao · Imagem e som · 3 de 46").
  rota[0] = 0;
  if (triUtil == TRI_U_PRESETS) snprintf(rota, sizeof rota, "%s · %s", i18n("Ajustes"), i18n("Predefinições"));
  else if (triUtil == TRI_U_DIF) snprintf(rota, sizeof rota, "%s · %s", i18n("Ajustes"), i18n("Diferente do padrão"));
  else if (triUtil == TRI_U_REVISAO) snprintf(rota, sizeof rota, "%s · %s", i18n("Ajustes"), i18n("Prévia da mudança"));
  else if (triUtil == TRI_U_PARENTAL) snprintf(rota, sizeof rota, "%s · %s", i18n("Ajustes"), i18n("Controle parental"));
  else if (triIlha == TRI_LISTA && focoOp >= 0) {
    int i, pos = 0, n = 0;
    const char *bl = uxBloco(focoOp);
    for (i = secIni[secAtual] + 1; i < secFim(secAtual); i++)
      if (TELA[i].tipo == IT_OPC && triMostrar(i) && visivel(i)) { n++; if (i == focoItem) pos = n; }
    if (bl[0]) snprintf(rota, sizeof rota, i18n("%s · %s · %d de %d"), i18n(TELA[secIni[secAtual]].titulo), i18n(bl), pos, n);
    else snprintf(rota, sizeof rota, i18n("%s · %d de %d"), i18n(TELA[secIni[secAtual]].titulo), pos, n);
  } else snprintf(rota, sizeof rota, "%s · %s", i18n("Ajustes"), i18n(triParental(triSecFoco) && triIlha == TRI_SECOES ? "Controle parental" : TELA[secIni[secAtual]].titulo));
  if (uxAviso[0] && !(uxEditor && (uxRestaurar || uxAvisoRisco || uxEditor == 2))) {
    TxtLinha t = ajTxtC(TXT_V2_24, uxAviso, 700.0f);
    ajIcone("aj_check", xd - t.w - 34.0f, yc - 11.0f, 22.0f, 0.7f);
    txt_desenhar_alpha(t, xd - t.w, yc - t.h * 0.5f, 0.75f);
  } else {
    TxtLinha t = ajTxtC(TXT_V2_24, rota, 760.0f);
    txt_desenhar_alpha(t, xd - t.w, yc - t.h * 0.5f, 0.42f);
  }
}
static void triDesenhar(void) {
  int modal = ajModalAberto();
  float g0;
  ponteiro_camada();
#ifdef NV_TOUCH_UI
  ajPontInline = 0;
#endif
  ajFundo();
  if (filAberta) return;   // a folha de fileiras ocupa a tela (desenhada depois)
  if (modal) gfx_opacidade_grupo = 0.16f;
  g0 = gfx_opacidade_grupo;
  triCabecalho();
  if (!ajRetrato() || (focoIndice && !triUtil)) triIlhaSecoes();
  ajPontLista = !modal && !uxEditor && !triUtil && paginaA > 0.997f;
  ajPontUtil = !modal && !uxEditor && triUtil && triUtilA > 0.997f && ajPenParentEditor < 0 && !ajPenPinAsk;
#ifdef NV_TOUCH_UI
  // Celular: a edicao na linha (paleta, escolha segmentada, interruptor) tem um
  // alvo por valor e o rodape Confirmar/Cancelar (ajToqueEscolhaAlvo,
  // ajToqueBotoes); as linhas de tras nao recebem toque, a rolagem continua.
  ajPontInline = layout_modo_mobile() && !modal && uxOp >= 0 && ajEditandoLinha(uxOp) && paginaA > 0.997f;
  if (ajPontInline) ajPontLista = 1;
#endif
  if (uxEditor && !modal) { ponteiro_camada();
#ifdef NV_TOUCH_UI
    ajToqueCamada();
#endif
  }
  // O PALCO (mockup claude-c, aprovado 30/10): pilha de blades + menu que
  // condensa + estagio sem controle. O resto do caminho (retrato, "Lista",
  // celular) continua nas tres ilhas de sempre.
  if (triBladeLayout()) {
    ajPontEditor = triEditorBlade() && !(uxRestaurar || uxAvisoRisco) && !riscoFolha && !frAberta;
    triPalcoDesenhar(modal, g0);
    triRodape();
    gfx_opacidade_grupo = 1.0f;
    if (modal && !(uxEditor && (uxRestaurar || uxAvisoRisco || uxEditor == 2)) && !riscoFolha && !teclado_aberto()) ajVeuModal();
    ajDesenharModais();
    if (registro_envio_aberto()) registro_envio_desenhar();
    return;
  }
  // ILHA 2 (uma rota utilitaria entra em 220 ms: fade e deslize curto)
  if (triUtil) { float k = anim_suave(triUtilA); gfx_opacidade_grupo = g0 * k; gfx_transformar(triIlha2R().x, triIlha2R().y, 1.0f, 0.0f, 18.0f * (1.0f - k)); }
  if (triUtil == TRI_U_DIF) { ajIlha(triIlha2R(), 28.0f, 1, 0); triListaCabecalho(triIlha2R(), "Diferente do padrão", uxNDifs ? "" : "Tudo está no padrão", 1); aj2DesenharDiferencas();
    { static const char *const rot[] = { "Restaurar tudo", "Desfazer" }; static const char *const ic[] = { "aj_rotate-ccw", "aj_corner-up-left" };
      triPilulas(triIlha2R(), rot, ic, 1 + (rvUndoN > 0), triAcao, 1); } }
  else if (triUtil == TRI_U_PRESETS) triPresetsDesenhar();
  else if (triUtil == TRI_U_REVISAO) triRevisaoDesenhar();
  else if (triUtil == TRI_U_PARENTAL) {
    // Sem ilha 3 ("Lista", retrato): a escolha parental toma o lugar da lista.
    if (!triTemInsp() && ajPenParentEditor >= 0) { GfxRect m = triIlha2R(); ponteiro_camada(); ajIlha(m, 28.0f, 1, 0); triParentalEscolha(m); }
    else triParentalDesenhar();
  }
  else if (!ajRetrato() || !focoIndice) triLista();
  if (triUtil) { gfx_sem_transformar(); gfx_opacidade_grupo = g0; }
  // ILHA 3: o editor cobre o inspetor; a escolha parental tambem.
  ajPontEditor = aj2EditorPainel() && !(uxRestaurar || uxAvisoRisco) && !riscoFolha && !frAberta;
  if (triTemInsp()) {
    GfxRect m = triInspR();
    if (ajPenParentEditor >= 0) { ponteiro_camada(); ajIlha(m, 28.0f, 1, 0); triParentalEscolha(m); }
    else if (aj2EditorPainel() && uxOp >= 0) {
      float k = anim_suave(triEditorF);
      if (modal || uxEditor) { ponteiro_camada();
#ifdef NV_TOUCH_UI
        ajToqueCamada();
#endif
      }
      // O editor entra por cima do inspetor: fade e deslize curto (sem fade da tela).
      gfx_opacidade_grupo = g0 * k;
      gfx_transformar(m.x, m.y, 1.0f, 0.0f, 16.0f * (1.0f - k));
      if (triEditorLargo(uxOp)) aj2DesenharEditor(); else triEditor(m);
      gfx_sem_transformar();
      gfx_opacidade_grupo = g0;
    }
    else {
      ajIlha(m, 28.0f, 1, 0);
      if (triUtil == TRI_U_DIF) aj2InspetorDiferencas();
      else if (triUtil) triUtilInspetor(triInspMiolo());
      else ajDesenharInspetor();
    }
  }
  if (!triTemInsp() && aj2EditorPainel() && uxOp >= 0) {
    // Sem ilha 3 ("Lista", retrato): o editor cobre a lista, como no 2.0.3.
    GfxRect m = triIlha2R();
    float k = anim_suave(triEditorF);
    if (modal || uxEditor) { ponteiro_camada();
#ifdef NV_TOUCH_UI
      ajToqueCamada();
#endif
    }
    gfx_opacidade_grupo = g0 * k;
    gfx_transformar(m.x, m.y, 1.0f, 0.0f, 16.0f * (1.0f - k));
    if (triEditorLargo(uxOp) || m.w >= 700.0f) aj2DesenharEditor(); else triEditor(m);
    gfx_sem_transformar();
    gfx_opacidade_grupo = g0;
  }
  triRodape();
  gfx_opacidade_grupo = 1.0f;
  if (modal && !(uxEditor && (uxRestaurar || uxAvisoRisco || uxEditor == 2)) && !riscoFolha && !teclado_aberto()) ajVeuModal();
  (void)g0;
  ajDesenharModais();
  if (registro_envio_aberto()) registro_envio_desenhar();
}
#ifdef AJUSTES_TESTE
// 1 enquanto a lista de uma secao nova ainda entra (fade + deslize de 220 ms).
int ajustes_teste_transicao(void) { return paginaA < 0.999f; }
#endif
