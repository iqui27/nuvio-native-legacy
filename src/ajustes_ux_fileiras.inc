// FILEIRAS DA HOME (Ordenar e ativar fileiras) NO GLASS UI — mockup de
// Ajustes, quadros "fileiras" e "fileiras-fora". Uma ilha grande com o
// segmentado Na Home / Fora da Home; foco da linha = superficie, a celula em
// foco e uma pilula um degrau mais clara (saiu o anel de acento); a fila vira
// um cabecalho de grupo; a direita, o painel com a forma escolhida em arte de
// verdade e todas as formas na proporcao de home.c. A navegacao e a de antes
// (eventoFileiras): so o desenho mudou. Os botoes que eram o fim da lista
// (Agrupar, Atualizar tudo) moram no cabecalho, e o foco chega neles pelo
// mesmo passo de sempre (descer depois da ultima linha).

// Arte de uma fileira: os titulos do catalogo DELA quando ja foi lido, a
// amostra antes disso. `fundo` = arte deitada (backdrop) em vez do cartaz.
//
// TRES CASOS (tcl-agora5, fileira "Canais de TV" do FrostView): um catalogo de
// canais so traz LOGO no lugar de cartaz. Cortado em "cover" 2:3, o logo vira
// borrao sobre cinza e, pior, quando os primeiros canais repetem o mesmo logo
// (varias afiliadas da Globo) os 4 quadros e as 8 formas ficam identicos.
//   ART_REAL  : cartaz/fundo de verdade, como sempre;
//   ART_LOGO  : fileira de canais com logos diferentes: o logo INTEIRO sobre
//               uma placa clara (ajImgFil), nunca cortado;
//   ART_AMOSTRA: o catalogo nao ajuda (sem arte, ou tudo igual): a arte de
//               outros titulos do catalogo, so para mostrar a FORMA.
// Tipos de catalogo que so trazem logo (ver addons.c: "channel", "tv").
static int ajTipoCanal(const char *t) {
  return !strcasecmp(t, "channel") || !strcasecmp(t, "channels") || !strcasecmp(t, "tv");
}
enum { ART_REAL = 0, ART_LOGO, ART_AMOSTRA };
// So as fileiras lidas guardam arte; 768 x 8 x 2 x 2048 fixos custariam 24 MiB.
static char (*ajFilCache[FIL_MAX])[2][NV_TEX_URL_MAX];
static unsigned char ajFilPronto[FIL_MAX], ajFilModo[FIL_MAX];
static void ajFilLer(int fil) {
  static unsigned rev = 0xffffffffu;
  if (cat_revisao() != rev) {
    for (int i = 0; i < FIL_MAX; i++) { free(ajFilCache[i]); ajFilCache[i] = NULL; }
    rev = cat_revisao(); memset(ajFilPronto, 0, sizeof ajFilPronto);
  }
  if (fil >= 0 && fil < FIL_MAX && !ajFilPronto[fil]) {
    static CatItem it[40];
    CatFileira meta;
    int n, j, i, dif = 0, comArte = 0;
    memset(&meta, 0, sizeof meta);
    n = cat_n() > 0 ? cat_copiar_fileira(fil_chave(fil), it, 40, &meta) : 0;
    if (n > 0) ajFilCache[fil] = calloc(8, sizeof *ajFilCache[fil]);
    if (!ajFilCache[fil]) { ajFilModo[fil] = ART_AMOSTRA; ajFilPronto[fil] = 1; return; }
    for (j = 0; j < 8; j++) { ajFilCache[fil][j][0][0] = ajFilCache[fil][j][1][0] = 0; }
    // Os 8 quadros pegam itens de arte DIFERENTE (varias afiliadas de uma rede
    // repetem o mesmo logo nos primeiros itens); so se faltar, repete.
    for (j = 0; j < n; j++) {
      const char *a = it[j].poster[0] ? it[j].poster : it[j].backdrop;
      if (!a[0]) continue;
      comArte++;
      for (i = 0; i < dif; i++) {
        const char *b = ajFilCache[fil][i][0][0] ? ajFilCache[fil][i][0] : ajFilCache[fil][i][1];
        if (!strcmp(a, b)) break;
      }
      if (i == dif && dif < 8) {
        snprintf(ajFilCache[fil][dif][0], sizeof ajFilCache[fil][dif][0], "%s", it[j].poster);
        snprintf(ajFilCache[fil][dif][1], sizeof ajFilCache[fil][dif][1], "%s", it[j].backdrop);
        dif++;
      }
    }
    // Menos de 8 distintos: completa repetindo, para o quadro k nunca ficar vazio.
    for (j = dif; dif > 0 && j < 8; j++) {
      memcpy(ajFilCache[fil][j], ajFilCache[fil][j % dif], sizeof ajFilCache[fil][j]);
    }
    if (n < 1 || comArte < 1) ajFilModo[fil] = ART_AMOSTRA;
    else if (dif < 2 && comArte >= 2) ajFilModo[fil] = ART_AMOSTRA;
    else if (ajTipoCanal(meta.tipo)) ajFilModo[fil] = ART_LOGO;
    else ajFilModo[fil] = ART_REAL;
    ajFilPronto[fil] = 1;
  }
}
static const char *ajArteFileira(int fil, int k, int fundo) {
  if (k < 0) k = 0;
  k %= 8;
  ajFilLer(fil);
  if (fil >= 0 && fil < FIL_MAX && ajFilModo[fil] != ART_AMOSTRA) {
    const char *c = ajFilCache[fil][k][fundo ? 1 : 0];
    if (!c[0]) c = ajFilCache[fil][k][fundo ? 0 : 1];
    if (c[0]) return c;
  }
  // Amostra: titulos de filme/serie de OUTRAS fileiras (nunca os canais, que
  // sao logo), e so se o catalogo nao tiver nenhum, a amostra embarcada.
  { int nc = cat_n(), i, ok = 0, alvo = ((fil < 0 ? 0 : fil) * 3 + k) % 14, quero;
    for (i = 0; i < nc; i++) {
      const CatItem *c = cat_item(i);
      const char *a;
      if (!c || ajTipoCanal(c->tipo)) continue;
      a = fundo ? (c->backdrop[0] ? c->backdrop : c->poster) : (c->poster[0] ? c->poster : c->backdrop);
      if (a[0]) ok++;
    }
    if (ok > 0) {
      quero = alvo % ok;
      for (i = 0; i < nc; i++) {
        const CatItem *c = cat_item(i);
        const char *a;
        if (!c || ajTipoCanal(c->tipo)) continue;
        a = fundo ? (c->backdrop[0] ? c->backdrop : c->poster) : (c->poster[0] ? c->poster : c->backdrop);
        if (a[0] && quero-- == 0) return a;
      }
    } }
  { const char *r;
    ajArteSoAmostra = 1;
    r = ajArte(fundo ? AJA_FUNDO : AJA_POSTER, AJ_POS[((fil < 0 ? 0 : fil) * 3 + k) % 14]);
    ajArteSoAmostra = 0;
    return r; }
}
// ajImg da arte da fileira: logo de canal inteiro numa placa clara.
static int ajImgFil(int fil, int k, int fundo, GfxRect r, float raioPx, float a) {
  const char *c = ajArteFileira(fil, k, fundo);
  GLuint t;
  float asp, w, h;
  if (fil < 0 || fil >= FIL_MAX || ajFilModo[fil] != ART_LOGO) return ajImg(c, r, raioPx, a);
  gfx_cor(r, raioPx / r.h, 0.27f, 0.28f, 0.31f, a);
  t = tex_obter_larg(c, r.w > r.h ? r.w : r.h);
  if (!t) return 0;
  asp = tex_aspecto(c);
  if (asp <= 0.0f) asp = 1.0f;
  w = r.w * 0.78f; h = w / asp;
  if (h > r.h * 0.78f) { h = r.h * 0.78f; w = h * asp; }
  gfx_tex_aspect_atual = asp;
  gfx_rect((GfxRect){ r.x + (r.w - w) * 0.5f, r.y + (r.h - h) * 0.5f, w, h }, t, GFX_ARTE, 0, 0, 0, 0, 1, 1, 1, a);
  gfx_tex_aspect_atual = 0;
  return 1;
}
static int ajFormaDeitada(int t) {
  return t != FIL_TIPO_CARTAZ && t != FIL_TIPO_TOP10 && t != FIL_TIPO_RANKING && t != FIL_TIPO_AUTO;
}

// O "sub" da linha: "Cinemeta · Filmes", "Do app", "Coleção".
static void ajSubFileira(int idx, char *b, size_t tam) {
  const char *addon = fil_linha_addon(idx), *cont = fil_linha_conteudo(idx);
  if (addon[0] && cont && cont[0]) snprintf(b, tam, "%s · %s", addon, i18n(cont));
  else if (addon[0]) snprintf(b, tam, "%s", addon);
  else snprintf(b, tam, "%s", i18n(fil_origem_rotulo(fil_linha_origem(idx))));
}

// Celula da tabela (Estado/Card/Tamanho): pilula 56 que acende com o foco.
static void ajCelula(float x, float yc, float w, const char *t, float al, int foco) {
  TxtLinha l = ajTxtC(TXT_AJ_VALOR18, t, w - 32);
  if (foco && !gfx_foco_placa((GfxRect){ x, yc - 28, w, 56 }, .5f, 1, 1))
    ajNeutro((GfxRect){ x, yc - 28, w, 56 }, 28, 0.14f, 0.204f, 0.212f, 0.243f, 1);
  txt_desenhar_alpha(l, x + 16, yc - l.h * 0.5f, foco ? 1.0f : al);
}

#define AJF_ROW_H   66.0f
#define AJF_FORA_H  72.0f
#define AJF_SEC_H   50.0f
#define AJF_PAINEL_W 560.0f

static void ajFilPainel(GfxRect pr, int n, int lim);
typedef struct { GfxRect ilha, painel; float cx, cw, estado, card, tamanho; } AjFilMedidas;
static AjFilMedidas ajFilMedir(void) {
  AjFilMedidas m;
  float rail = ajustes_rail_largura_fixa();
  float x = (ajTelaCheia() && rail <= 0.0f ? 0.0f : 48.0f) + rail;
  float topo = ajTelaCheia() ? 0.0f : AJI_TOPO * ajustes_tamanho_ajustes();
  float margem = ajTelaCheia() ? 0.0f : AJI_MARGEM * ajustes_tamanho_ajustes();
  m.ilha = (GfxRect){x, topo, NV_TELA_W - (ajTelaCheia() ? 0.0f : 48.0f) - x, NV_TELA_H - topo - margem};
  m.painel = (GfxRect){m.ilha.x + m.ilha.w - 24 - AJF_PAINEL_W, m.ilha.y + 24, AJF_PAINEL_W, m.ilha.h - 48};
  if (ajRetrato()) m.painel.x = m.ilha.x + m.ilha.w;
  m.cx = m.ilha.x + 30; m.cw = m.painel.x - 30 - m.cx;
  float fator = ajRetrato() ? fmaxf(0.0f, (m.cw - 28.0f) * 0.5f) / 630.0f : 1.0f;
  m.estado = 200.0f * fator; m.card = 230.0f * fator; m.tamanho = 200.0f * fator;
  return m;
}

#ifdef NV_TOUCH_UI
static float ajFilToqueAltura(int i) {
  float h = (filAba == 0 ? AJF_ROW_H : AJF_FORA_H) + 4.0f;
  if (filAba == 0 && filSep >= 0 && i == filSep) h += AJF_SEC_H;
  if (filAba == 1 && filForaAgrupada &&
      (i == 0 || strcasecmp(fil_linha_addon(filLista[i - 1]), fil_linha_addon(filLista[i])))) h += AJF_SEC_H;
  return h;
}
static float ajFilToqueTopo(int i) {
  float h = 0.0f;
  for (int j = 0; j < i && j < filListaN; j++) h += ajFilToqueAltura(j);
  return h;
}
static void ajFilToqueFocar(int i, int campo) {
  if (!layout_modo_mobile() || !filAberta || filPegou) return;
  filNaBarra = 0; filFoco = i; filCampo = campo;
}
static void ajFilToqueSoltar(int destino, int campo) {
  int passos = FIL_MAX + 1, idx = filIdx(filFoco);
  (void)campo;
  if (!layout_modo_mobile() || !filAberta || !filPegou || filAba || destino < 0 || destino >= filListaN || filIdx(destino) < 0 || idx < 0) return;
  while (filFoco != destino && passos-- > 0) {
    int dir = destino > filFoco ? 1 : -1;
    int novo = filPegou == 2 ? fil_mover_grupo(idx, dir) : fil_mover(idx, dir);
    if (novo == idx) break;
    idx = novo;
    filMontarLista();
    int pos = -1;
    for (int i = 0; i < filListaN; i++) if (filLista[i] == idx) { pos = i; break; }
    if (pos < 0) break;
    filFoco = pos;
    if ((dir > 0 && pos >= destino) || (dir < 0 && pos <= destino)) break;
  }
  filPegou = 0; filCampo = 0; filNaBarra = 0;
  if (idx != filPegouDe) fileirasReagir();
}
static void ajFilToqueAba(int aba, int b) {
  (void)b;
  if (!layout_modo_mobile() || !filAberta || filPegou || aba < 0 || aba > 1) return;
  filAba = aba; filNaBarra = 1; filBarra = aba; filFoco = 0; filTopo = 0; filCampo = 0;
  ajToqueLivre[AJT_FILEIRAS] = 0;
  filMontarLista();
}
// Chips da barra (2 = Ordenar, 3 = Atualizar tudo): o foco vai para a barra e o
// OK que o ponteiro entrega em seguida age neles.
static void ajFilToqueChip(int chip, int b) {
  (void)b;
  if (!layout_modo_mobile() || !filAberta || filPegou) return;
  filNaBarra = 1; filBarra = chip;
}
static void ajFilToqueModo(int a, int b) {
  (void)a; (void)b;
  if (!layout_modo_mobile() || !filAberta || filAba || !filPegou || filNaBarra) return;
  eventoFileiras(SDLK_RIGHT);
}
#endif

static void desenhaFileiras(void) {
  // #359: a ilha comeca depois da rail fixa, como o resto de Ajustes (ajX0):
  // em x 48 a pilula do menu cobria o titulo e o icone das linhas. Desenhado
  // em px da tela REAL (ESCALA_REAL), entao soma a rail sem dividir.
  AjFilMedidas m = ajFilMedir();
  GfxRect ilha = m.ilha, pr = m.painel;
  float cx = m.cx, cw = m.cw;
  float campos = m.estado + m.card + m.tamanho;
  float hx = cx + 14, y, topo, base, segL = cx + cw;
  int n, lim = fil_limite(), i;
  filMontarLista();
  n = filListaN;
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0, 0, 0, 0, ajVid() ? 0.30f : 0.42f);
  ajPaginaIlha(ilha, 36, 1);
  // CABECALHO
  ajKicker("Tela inicial · Ordenar e ativar", hx, ilha.y + 36, 1);
  { TxtLinha t = ajTxt(TXT_ILHA_TITULO, "Fileiras da Home");
    txt_desenhar(t, hx, ilha.y + 36 + 22 + (46 - t.h) * 0.5f); }
  // Segmentado das abas + os botoes do cabecalho, alinhados pela base.
  { char a0[48], a1[48], n0[32], n1[32];
    const char *rot[2], *num[2];
    float bx = cx + cw - 14, by = ilha.y + 36 + 22 + 46 + 8 + 24 - 56;
    if (ajRetrato()) by += 56.0f;
    int fAtualizar = filNaBarra && filBarra == 3;
    int fAgrupar = filNaBarra && filBarra == 2;
    snprintf(a0, sizeof a0, "%s", i18n("Na Home")); snprintf(a1, sizeof a1, "%s", i18n("Fora da Home"));
    snprintf(n0, sizeof n0, "%d", fil_n_na_home());
    snprintf(n1, sizeof n1, "%d", fil_n() - fil_n_na_home() - fil_n_fila());
    rot[0] = a0; rot[1] = a1; num[0] = n0; num[1] = n1;
#ifdef NV_TOUCH_UI
    // Com uma fileira na mao, ofereca o modo de movimento no lugar do refresh.
    if (layout_modo_mobile() && filPegou && !filAba) {
      const char *r = filPegou == 2 ? "Bloco do addon" : "Só a fileira";
      float max = fmaxf(56.0f, cw - ajSegLargura(rot, num, 2) - 38.0f);
      float w = fminf(txt_largura(TXT_AJ_SEG, i18n(r)) + 44.0f, max);
      TxtLinha t = ajTxtC(TXT_AJ_SEG, r, w - 44.0f);
      bx -= w;
      ajNeutro((GfxRect){bx, by, w, 56}, 28, 0.14f, AJ_CHIP_SOL, 1);
      txt_desenhar_alpha(t, bx + 22.0f, by + (56 - t.h) * 0.5f, 1);
      ponteiro_alvo(bx, by, w, 56, NULL, ajFilToqueModo, 0, 0);
    } else
#endif
    {
    // Atualizar tudo: disco 56 com o icone.
    bx -= 56;
#ifdef NV_TOUCH_UI
    if (layout_modo_mobile()) ponteiro_alvo(bx, by, 56, 56, ajFilToqueChip, NULL, 3, 0);
#endif
    if (fAtualizar) { ajPilulaAcento((GfxRect){ bx, by, 56, 56 }, 28, 1); ajIconeT("aj_rotate-cw", bx + 17, by + 17, 22, 1); }
    else { ajNeutro((GfxRect){ bx, by, 56, 56 }, 28, 0.08f, AJ_CHIP_SOL, 1); ajIcone("aj_rotate-cw", bx + 17, by + 17, 22, 0.85f); }
    }
    if (filAba == 1) {
      const char *r = filForaAgrupada ? (ajRetrato() ? "Addon" : "Por addon") : (ajRetrato() ? "A–Z" : "Alfabética");
      TxtLinha t = fAgrupar ? ajTxtT(TXT_AJ_SEG, r) : ajTxt(TXT_AJ_SEG, r);
      float w = 22 + 20 + 10 + t.w + 22;
      bx -= 10 + w;
#ifdef NV_TOUCH_UI
      if (layout_modo_mobile()) ponteiro_alvo(bx, by, w, 56, ajFilToqueChip, NULL, 2, 0);
#endif
      if (fAgrupar) { ajPilulaAcento((GfxRect){ bx, by, w, 56 }, 28, 1); ajIconeT("aj_rows-3", bx + 22, by + 18, 20, 1); }
      else { ajNeutro((GfxRect){ bx, by, w, 56 }, 28, 0.08f, AJ_CHIP_SOL, 1); ajIcone("aj_rows-3", bx + 22, by + 18, 20, 0.85f); }
      txt_desenhar_alpha(t, bx + 52, by + (56 - t.h) * 0.5f, fAgrupar ? 1.0f : 0.85f);
    }
    { float w = ajSegLargura(rot, num, 2);
      bx -= 10 + w;
      ajSeg(rot, num, 2, filNaBarra && filBarra <= 1 ? filBarra : filAba, filNaBarra && filBarra <= 1, bx, by + 28, 56, 1);
#ifdef NV_TOUCH_UI
      if (layout_modo_mobile()) {
        float tx = bx + 5.0f;
        for (int t = 0; t < 2; t++) {
          float tw = txt_largura(TXT_AJ_SEG, i18n(rot[t])) + 40.0f + 9.0f + txt_largura(TXT_ILHA_GENERO, num[t]);
          ponteiro_alvo(tx, by + 5.0f, tw, 46, NULL, ajFilToqueAba, t, 0);
          tx += tw + 4.0f;
        }
      }
#endif
      segL = bx; } }
  { TxtLinha s = ajTxtC(TXT_AJ_ESTADO, filAba == 0 ? "Vale só nesta TV e neste perfil · não altera a Home dos outros aparelhos"
                                                   : "OK adiciona: entra na Home se couber, senão entra na fila", ajRetrato() ? cw - 28.0f : segL - hx - 24);
    txt_desenhar_alpha(s, hx, ilha.y + 36 + 22 + 46 + 8 + (ajRetrato() ? 92.0f : 0.0f), 0.55f); }
  topo = ilha.y + 36 + 22 + 46 + 8 + 24 + 22 + (ajRetrato() ? 92.0f : 0.0f);
  base = ilha.y + ilha.h - 20;
  // CABECALHO DAS COLUNAS (so na aba Na Home)
  if (filAba == 0 && n > 0) {
    float x = cx + cw - 14;
    x -= m.tamanho; ajCaps(TXT_AJ_CAPS13, "Tamanho", 1.68f, x + 16, topo, 0.38f);
    x -= m.card; ajCaps(TXT_AJ_CAPS13, "Card", 1.68f, x + 16, topo, 0.38f);
    x -= m.estado; ajCaps(TXT_AJ_CAPS13, "Estado", 1.68f, x + 16, topo, 0.38f);
    ajCaps(TXT_AJ_CAPS13, "Fileira", 1.68f, cx + 92, topo, 0.38f);
    topo += 17 + 8;
  } else topo += 4;
  if (n < 1) {
    TxtLinha t = ajTxt(TXT_AJ_ROTULO, filAba == 0 ? "Nenhuma fileira ligada" : "Tudo o que o app conhece já está na Home");
    txt_desenhar(t, cx + 14, topo + 20);
    ajBloco(TXT_AJ_ESTADO, filAba == 0
            ? "Vá para a aba \"Fora da Home\" (↑ e depois →) e aperte OK numa fileira para adicioná-la."
            : "As fileiras aparecem aqui depois que o app lê os catálogos dos seus addons. Abra a Home, espere o catálogo carregar e volte a esta tela.",
            cx + 14, topo + 64, cw - 60, 25, 0.55f, 3);
  }
  // ROLAGEM: a primeira linha desenhada (filTopo) acompanha o foco.
#ifdef NV_TOUCH_UI
  if (!layout_modo_mobile() || !ajToqueLivre[AJT_FILEIRAS])
#endif
  { float area = base - topo, h = 0;
    int f = filFoco < n ? filFoco : n - 1, j;
    if (filTopo > f) filTopo = f;
    if (filTopo < 0) filTopo = 0;
    for (;;) {
      h = 0;
      for (j = filTopo; j <= f && j < n; j++) {
        int idx = filLista[j];
        if (filAba == 0 && filSep >= 0 && j == filSep) h += AJF_SEC_H;
        if (filAba == 1 && filForaAgrupada && (j == 0 || strcasecmp(fil_linha_addon(filLista[j - 1]), fil_linha_addon(idx)))) h += AJF_SEC_H;
        h += (filAba == 0 ? AJF_ROW_H : AJF_FORA_H) + 4;
      }
      if (h <= area - 20 || filTopo >= f) break;
      filTopo++;
    } }
  // LINHAS
  gfx_recorte(ilha.x, topo - 4, pr.x - ilha.x, base - topo + 4);
  y = topo;
#ifdef NV_TOUCH_UI
  if (layout_modo_mobile()) {
    y -= ajToqueRegistrar(AJT_FILEIRAS, (GfxRect){cx, topo, cw, base - topo}, &ajToqueOffset[AJT_FILEIRAS],
                          ajFilToqueTopo(n) - (base - topo), ajFilToqueTopo(filTopo), filAba * 2 + filForaAgrupada);
    i = 0;
  } else i = filTopo;
#else
  i = filTopo;
#endif
  for (; i < n && y < base; i++) {
    int idx = filLista[i];
    int foco = i == filFoco && !filNaBarra && !(filAba == 1 && filCampo == FIL_CAMPO_REGUA);
    float rh = filAba == 0 ? AJF_ROW_H : AJF_FORA_H;
    GfxRect ln;
#ifdef NV_TOUCH_UI
    if (layout_modo_mobile() && y + ajFilToqueAltura(i) < topo) { y += ajFilToqueAltura(i); continue; }
#endif
    // Cabecalhos de grupo: a fila, ou o addon na aba Fora.
    if (filAba == 0 && filSep >= 0 && i == filSep) {
      char q[16];
      TxtLinha b = ajTxt(TXT_AJ_SECAO, "Na fila"), c;
      float bx2;
      snprintf(q, sizeof q, "%d", fil_n_fila());
      txt_desenhar(b, cx + 22, y + 14 + 6);
      bx2 = cx + 22 + b.w + 12;
      c = ajTxt(TXT_ILHA_GENERO, q);
      ajCapsC(TXT_AJ_CAPS13, "entram sozinhas quando abrir vaga", 1.82f, bx2, y + 14 + 6 + b.h - 17, 0.4f, cx + cw - 22 - c.w - 16 - bx2);
      txt_desenhar_alpha(c, cx + cw - 22 - c.w, y + 14 + 6 + b.h - c.h, 0.38f);
      gfx_cor((GfxRect){ cx, y + AJF_SEC_H - 5, cw, 1 }, 0, 1, 1, 1, 0.08f);
      y += AJF_SEC_H;
    }
    if (filAba == 1 && filForaAgrupada && (i == 0 || strcasecmp(fil_linha_addon(filLista[i - 1]), fil_linha_addon(idx)))) {
      const char *ad = fil_linha_addon(idx);
      int q = 0, j;
      char qs[16];
      TxtLinha b, c;
      for (j = i; j < n && !strcasecmp(fil_linha_addon(filLista[j]), ad); j++) q++;
      snprintf(qs, sizeof qs, "%d", q);
      b = ajTxt(TXT_AJ_SECAO, ad[0] ? ad : i18n(fil_origem_rotulo(fil_linha_origem(idx))));
      txt_desenhar(b, cx + 22, y + 14 + 6);
      ajCaps(TXT_AJ_CAPS13, fil_origem_rotulo(fil_linha_origem(idx)), 1.82f, cx + 22 + b.w + 12, y + 14 + 6 + b.h - 17, 0.4f);
      c = ajTxt(TXT_ILHA_GENERO, qs);
      txt_desenhar_alpha(c, cx + cw - 22 - c.w, y + 14 + 6 + b.h - c.h, 0.38f);
      gfx_cor((GfxRect){ cx, y + AJF_SEC_H - 5, cw, 1 }, 0, 1, 1, 1, 0.08f);
      y += AJF_SEC_H;
    }
    ln = (GfxRect){ cx, y, cw, rh };
#ifdef NV_TOUCH_UI
    if (layout_modo_mobile()) {
      PonteiroFn soltar = filPegou ? ajFilToqueSoltar : NULL;
      ponteiro_alvo_faixa(cx, y, cw, rh, topo, base, ajFilToqueFocar, soltar, i, 0);
      if (filAba == 0 && idx != AJ_FIL_DESTAQUE) {
        float xc = cx + cw - 14.0f - campos;
        ponteiro_alvo_faixa(xc, y, m.estado, rh, topo, base, ajFilToqueFocar, soltar, i, 1);
        ponteiro_alvo_faixa(xc + m.estado, y, m.card, rh, topo, base, ajFilToqueFocar, soltar, i, 2);
        ponteiro_alvo_faixa(xc + m.estado + m.card, y, m.tamanho, rh, topo, base, ajFilToqueFocar, soltar, i, 3);
      }
    }
#endif
    if (foco && filPegou) {
      gfx_rect((GfxRect){ ln.x - 16, ln.y + 4, ln.w + 32, ln.h + 50 }, 0, GFX_SOMBRA, 1, 0, 0, 0.5f, 0, 0, 0, 0.55f);
      ln = (GfxRect){ ln.x - ln.w * 0.005f, ln.y - ln.h * 0.005f, ln.w * 1.01f, ln.h * 1.01f };
    }
    if (foco) ajFocoLinha(ln, 22, 1, 1);
    { float yc = y + rh * 0.5f, x = cx + 18;
      const char *ic;
      if (idx == AJ_FIL_DESTAQUE) {
        float nomeW = cw - campos - 122.0f;
        TxtLinha t = ajRetrato() ? ajTxtC(TXT_AJ_ITEM, "Destaque do topo", nomeW) : ajTxt(TXT_AJ_ITEM, "Destaque do topo");
        TxtLinha s = ajRetrato() ? ajTxtC(TXT_ILHA_GENERO, "O que aparece no destaque da Home", nomeW) : ajTxt(TXT_ILHA_GENERO, "O que aparece no destaque da Home");
        TxtLinha v = ajTxtC(TXT_AJ_VALOR18, heroFonteRotulo(), ajRetrato() ? campos - 32.0f : 480);
        ajIcone("aj_panel-top", x + 30 + 4, yc - 12, 24, 0.6f);
        txt_desenhar_alpha(t, x + 30 + 40, yc - (t.h + 2 + s.h) * 0.5f, foco ? 1.0f : 0.86f);
        txt_desenhar_alpha(s, x + 30 + 40, yc - (t.h + 2 + s.h) * 0.5f + t.h + 2, 0.42f);
        txt_desenhar_alpha(v, cx + cw - 14 - campos + 16, yc - v.h * 0.5f, foco ? 0.85f : 0.55f);
        if (foco) { ajIcone("aj_chevron-right", cx + cw - 14 - 24, yc - 12, 24, 0.7f); }
        y += rh + 4;
        continue;
      }
      if (filAba == 0) {
        char num[8];
        TxtLinha l;
        snprintf(num, sizeof num, "%d", i);
        l = ajTxt(TXT_ILHA_GENERO, num);
        txt_desenhar_alpha(l, x + (30 - l.w) * 0.5f, yc - l.h * 0.5f, 0.35f);
        x += 30 + 6;
      } else x += 4;
      ic = foco && filPegou ? "aj_arrow-up-down"
         : !strcmp(fil_chave(idx), "social_activity") ? "aj_users" : fil_origem_icone(fil_linha_origem(idx));
      ajIcone(ic, x + 4, yc - 12, 24, 0.6f);
      x += 4 + 24 + 6 + 6;
      { char sb[160];
        float wt = filAba == 0 ? cx + cw - 14 - campos - x - 10 : cx + cw - 14 - 260 - x;
        TxtLinha t, s;
        ajSubFileira(idx, sb, sizeof sb);
        if (filAba == 1) {
          int it = fil_linha_itens(idx);
          const char *cont = fil_linha_conteudo(idx);
          if (it >= 0) snprintf(sb, sizeof sb, it == 1 ? i18n("%s · %d título") : i18n("%s · %d títulos"),
                                cont && cont[0] ? i18n(cont) : i18n(fil_origem_rotulo(fil_linha_origem(idx))), it);
          else snprintf(sb, sizeof sb, "%s", cont && cont[0] ? i18n(cont) : i18n(fil_origem_rotulo(fil_linha_origem(idx))));
        }
        t = ajTxtC(TXT_AJ_ITEM, fil_titulo(idx)[0] ? fil_titulo(idx) : fil_chave(idx), wt);
        s = ajTxtC(TXT_ILHA_GENERO, sb, wt);
        txt_desenhar_alpha(t, x, yc - (t.h + 2 + s.h) * 0.5f, foco ? 1.0f : 0.86f);
        txt_desenhar_alpha(s, x, yc - (t.h + 2 + s.h) * 0.5f + t.h + 2, 0.42f); }
      if (filAba == 0) {
        int naFila = filSep >= 0 && i >= filSep, aceita = fil_aceita_tipo(idx);
        float xc = cx + cw - 14 - campos;
        ajCelula(xc, yc, m.estado, foco && filCampo == 1 ? "Remover" : naFila ? "Na fila" : "Na Home", 0.55f, foco && filCampo == 1 && !filPegou);
        ajCelula(xc + m.estado, yc, m.card, aceita ? fil_linha_tipo_rotulo(idx) : "Fixo", aceita ? 0.55f : 0.35f, foco && filCampo == 2 && !filPegou);
        ajCelula(xc + m.estado + m.card, yc, m.tamanho, fil_tam_rotulo(fil_linha_tam(idx)), 0.55f, foco && filCampo == 3 && !filPegou);
      } else if (foco || (i == filFoco && !filNaBarra)) {
        int noBotao = filCampo == 1 && !filNaBarra && i == filFoco;
        TxtLinha t = noBotao ? ajTxtT(TXT_AJ_CHIP, "Adicionar à Home") : ajTxt(TXT_AJ_CHIP, "Adicionar à Home");
        float w = 18 + 20 + 10 + t.w + 18, bx = cx + cw - 22 - w;
        if (noBotao) ajPilulaAcento((GfxRect){ bx, yc - 24, w, 48 }, 24, 1);
        else ajNeutro((GfxRect){ bx, yc - 24, w, 48 }, 24, 0.08f, AJ_CHIP_SOL, 1);
        if (noBotao) ajIconeT("aj_plus", bx + 18, yc - 10, 20, 1);
        else ajIcone("aj_plus", bx + 18, yc - 10, 20, 0.85f);
        txt_desenhar_alpha(t, bx + 48, yc - t.h * 0.5f, noBotao ? 1.0f : 0.85f);
      } }
    y += rh + 4;
  }
  gfx_sem_recorte();
  // REGUA DE LETRAS da aba Fora (o salto por letra ao segurar ↑ ↓).
  if (filAba == 1 && n > 0) {
    static const char L[] = "#ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char at = filFoco < n ? filLetra(filLista[filFoco]) : 0;
    float rx = cx + cw + 6, ry = topo + 8, rh = base - topo - 16, passo = rh / 27.0f;
    int k;
    for (k = 0; k < 27; k++) {
      char s[2] = { L[k], 0 };
      TxtLinha l = ajTxt(TXT_AJ_CAPS13, s);
      float yy = ry + k * passo + passo * 0.5f;
      if (L[k] == at && filCampo == FIL_CAMPO_REGUA && !filNaBarra) { ajPilulaAcento((GfxRect){ rx + 3, yy - 12, 24, 24 }, 12, 1); txt_desenhar(ajTxtT(TXT_AJ_CAPS13, s), rx + 15 - l.w * 0.5f, yy - l.h * 0.5f); }
      else if (L[k] == at) { ajNeutro((GfxRect){ rx + 3, yy - 12, 24, 24 }, 12, 0.14f, 0.204f, 0.212f, 0.243f, 1); txt_desenhar(l, rx + 15 - l.w * 0.5f, yy - l.h * 0.5f); }
      else txt_desenhar_alpha(l, rx + 15 - l.w * 0.5f, yy - l.h * 0.5f, 0.3f);
    }
  }
  // AVISO ("Home cheia...", "Adicionada..."): chip no rodape da lista.
  if (filAviso[0] && SDL_GetTicks() < filAvisoAte) {
    TxtLinha l = ajTxtC(TXT_AJ_ESTADO, filAviso, cw - 80);
    GfxRect pil = { cx + (cw - l.w - 48) * 0.5f, base - 64, l.w + 48, 48 };
    ajIlha(pil, 24, 1, 1);
    txt_desenhar(l, pil.x + 24, pil.y + (48 - l.h) * 0.5f);
  }
  if (!ajRetrato()) ajFilPainel(pr, n, lim);
}

// As formas de card na proporcao de home.c, com arte; a escolhida acesa.
static void ajFormas(float x, float y, float w, int tipo, int idx) {
  static const int T[8] = { FIL_TIPO_AUTO, FIL_TIPO_CARTAZ, FIL_TIPO_DESTAQUE, FIL_TIPO_COLECAO,
                            FIL_TIPO_SERVICO, FIL_TIPO_RANKING, FIL_TIPO_DESTAQUE_QUADRADO, FIL_TIPO_LARGA };
  float cw = (w - 36) / 4.0f, esc = 0.27f;
  int i;
  for (i = 0; i < 8; i++) {
    int t = T[i], sel;
    float cx = x + (i % 4) * (cw + 12), cy = y + (i / 4) * (142 + 12), fw, fh, bx, by;
    TxtLinha l;
    if (t == FIL_TIPO_RANKING && tipo == FIL_TIPO_TOP10) t = FIL_TIPO_TOP10;
    sel = t == tipo || (t == FIL_TIPO_DESTAQUE_QUADRADO && (tipo == FIL_TIPO_DESTAQUE_QUADRADO_M || tipo == FIL_TIPO_DESTAQUE_QUADRADO_G));
    ajNeutro((GfxRect){ cx, cy, cw, 142 }, 16, sel ? 0.12f : 0.03f, sel ? 0.169f : 0.106f, sel ? 0.176f : 0.110f, sel ? 0.204f : 0.129f, 1);
    l = sel ? ajTxtC(TXT_AJ_MINI12, fil_tipo_rotulo(tipo), cw - 8) : ajTxtC(TXT_AJ_MINI12, fil_tipo_rotulo(t), cw - 8);
    txt_desenhar_alpha(l, cx + (cw - l.w) * 0.5f, cy + 142 - 10 - l.h, sel ? 1.0f : 0.62f * 0.72f);
    by = cy + 142 - 10 - l.h - 9;
    if (t == FIL_TIPO_AUTO) {
      // Duas pecas lado a lado: a deitada a esquerda, o cartaz na frente e rente a
      // direita, sobre um fundo opaco da cor do quadro (antes as duas ficavam
      // translucidas uma sobre a outra e o logo/arte de uma vazava na outra).
      fw = 360 * esc; fh = 203 * esc;
      if (fw > cw - 8) { fh *= (cw - 8) / fw; fw = cw - 8; }
      { float wx = cx + (cw - fw) * 0.5f, pw = 150 * esc, ph = 225 * esc, px = wx + fw - pw + 2;
        float al = sel ? 1.0f : 0.72f;
        ajImgFil(idx, 5, 1, (GfxRect){ wx, by - fh, fw, fh }, 7, 0.5f * al);
        gfx_cor((GfxRect){ px - 2, by - ph - 2, pw + 4, ph + 4 }, 9.0f / (ph + 4), sel ? 0.169f : 0.106f, sel ? 0.176f : 0.110f, sel ? 0.204f : 0.129f, 1);
        ajImgFil(idx, 4, 0, (GfxRect){ px, by - ph, pw, ph }, 7, al); }
      continue;
    }
    fw = AJ_FIL_FORMA[t].w * esc; fh = AJ_FIL_FORMA[t].h * esc;
    if (fw > cw - 12) { fh *= (cw - 12) / fw; fw = cw - 12; }
    bx = cx + (cw - fw) * 0.5f;
    if (sel) gfx_rect((GfxRect){ bx - 10, by - fh + 4, fw + 20, fh + 24 }, 0, GFX_SOMBRA, 1, 0, 0, 0.5f, 0, 0, 0, 0.5f);
    if (t == FIL_TIPO_TOP10 || t == FIL_TIPO_RANKING) {
      TxtLinha num = ajTxt(TXT_AJ_NUM58, "1");
      txt_desenhar_alpha(num, bx - 22, by + 8 - num.h, 0.25f);
    }
    ajImgFil(idx, i, ajFormaDeitada(t), (GfxRect){ bx, by - fh, fw, fh }, 7, sel ? 1 : 0.72f);
  }
}

static void ajFilPainel(GfxRect pr, int n, int lim) {
  float x = pr.x + 30, w = pr.w - 60, y = pr.y + 30;
  int idx = filFoco < n ? filLista[filFoco] : -1;
  ajNeutro(pr, 28, 0.04f, 0.106f, 0.110f, 0.129f, 1);
  if (filNaBarra) {
    const char *t = filBarra == 2 ? (filForaAgrupada ? "Por addon" : "Alfabética")
                  : filBarra == 3 ? "Atualizar tudo"
                  : filBarra == 0 ? "Na Home" : "Fora da Home";
    const char *s = filBarra == 2 ? "OK alterna entre agrupado por addon e lista alfabética."
                  : filBarra == 3 ? "Sincroniza a conta e refaz a Home agora: addons, coleções e fileiras."
                  : filBarra == 0 ? "As fileiras ligadas, na ordem em que a Home monta. As que passam do limite esperam na fila."
                  : "Tudo o que está desligado, agrupado por addon e em ordem alfabética.";
    ajKicker("Fileiras da Home", x, y, 1);
    { TxtLinha l = ajTxt(TXT_AJ_TIT28, t); txt_desenhar(l, x, y + 28); }
    ajBloco(TXT_AJ_ESTADO, s, x, y + 28 + 40, w, 27, 0.62f, 4);
  } else if (idx == AJ_FIL_DESTAQUE) {
    int k;
    ajKickerC("Destaque do topo", x, y, 1, w);
    { TxtLinha l = ajTxtC(TXT_AJ_TIT28, heroFonteRotulo(), w); txt_desenhar(l, x, y + 28); }
    y += 28 + 40 + ajBloco(TXT_AJ_ESTADO, "Automático usa os primeiros títulos do catálogo; o sorteio troca a cada abertura; uma fileira mostra os títulos dela.",
                           x, y + 28 + 40, w, 27, 0.62f, 4) + 22;
    ajNeutro((GfxRect){ x, y, w, 178 }, 18, 0.25f, 0, 0, 0, 1);
    for (k = 0; k < 2; k++) ajImg(ajArte(AJA_FUNDO, k ? 15 : ajArteFundoN), (GfxRect){ x + 16 + k * 260, y + 24, 250, 140 }, 10, k ? 0.6f : 1);
  } else if (idx >= 0) {
    char k[220], sb[160];
    const char *cont = fil_linha_conteudo(idx);
    int it = fil_linha_itens(idx), tipo = fil_aceita_tipo(idx) ? fil_linha_tipo(idx) : FIL_TIPO_AUTO;
    int len = snprintf(k, sizeof k, "%s", fil_titulo(idx));
    if (filAba == 1) len = snprintf(k, sizeof k, "%s", i18n(fil_origem_rotulo(fil_linha_origem(idx))));
    if (fil_linha_addon(idx)[0]) len += snprintf(k + len, sizeof k - len, " · %s", fil_linha_addon(idx));
    if (cont && cont[0]) len += snprintf(k + len, sizeof k - len, " · %s", i18n(cont));
    if (it >= 0) snprintf(k + len, sizeof k - len, it == 1 ? i18n(" · %d título") : i18n(" · %d títulos"), it);
    ajKickerC(k, x, y, 1, w);
    y += 28;
    if (filAba == 1) {
      int j;
      TxtLinha l = ajTxtC(TXT_AJ_TIT28, fil_titulo(idx), w);
      txt_desenhar(l, x, y); y += 40;
      y += ajBloco(TXT_AJ_ESTADO, fil_origem_ajuda(fil_linha_origem(idx)), x, y, w, 27, 0.62f, 3) + 26;
      ajKickerC("Do catálogo, quando ele já foi lido", x, y, 1, w);
      y += 18 + 14;
      { float pw = (w - 36) / 4.0f, ph = pw * 1.5f;
        for (j = 0; j < 8; j++) ajImgFil(idx, j, 0, (GfxRect){ x + (j % 4) * (pw + 12), y + (j / 4) * (ph + 12), pw, ph }, 10, 1);
        y += 2 * ph + 12 + 22; }
      if (fil_linha_origem(idx) == FIL_ORIGEM_CATALOGO && fil_n_capacidade() >= lim) {
        snprintf(sb, sizeof sb, i18n("Home cheia (%d de %d): esta fileira entra na fila e sobe sozinha quando abrir vaga."), fil_n_capacidade(), lim);
        ajEf("aj_info", sb, x, y, w, 1);
      }
    } else {
      const char *frase;
      char t[160];
      if (filCampo == 3) snprintf(t, sizeof t, i18n("Tamanho: %s"), i18n(fil_tam_rotulo(fil_linha_tam(idx))));
      else if (!fil_aceita_tipo(idx)) snprintf(t, sizeof t, "%s", fil_titulo(idx));
      else snprintf(t, sizeof t, i18n("Card: %s"), i18n(fil_tipo_rotulo(tipo)));
      frase = filCampo == 3 ? "O fator vale sobre a medida do tipo, então a proporção do card não muda."
            : !fil_aceita_tipo(idx) ? motivoFormaFixa(fil_chave(idx))
            : aj_fil_forma_ajuda(tipo);
      { TxtLinha l = ajTxtC(TXT_AJ_TIT28, t, w); txt_desenhar(l, x, y); y += 40; }
      y += ajBloco(TXT_AJ_ESTADO, frase, x, y, w, 27, 0.62f, 3) + 22;
      // COMO FICA NA HOME: tres titulos da propria fileira na forma escolhida.
      { int deit = ajFormaDeitada(tipo) || (tipo == FIL_TIPO_AUTO && fil_linha_origem(idx) != FIL_ORIGEM_CATALOGO);
        float fh = 120, fw = deit ? fh * 16.0f / 9.0f : fh * 2.0f / 3.0f, s = fil_tam_escala(fil_linha_tam(idx));
        int j;
        if (filCampo == 3) { fh *= s; fw *= s; }
        if (tipo == FIL_TIPO_DESTAQUE_QUADRADO || tipo == FIL_TIPO_DESTAQUE_QUADRADO_M || tipo == FIL_TIPO_DESTAQUE_QUADRADO_G) fw = fh * 4.0f / 3.0f;
        ajNeutro((GfxRect){ x, y, w, 178 }, 18, 0.25f, 0.06f, 0.063f, 0.075f, 1);
        { TxtLinha l = ajTxt(TXT_AJ_MINI14, "Como fica na Home"); txt_desenhar_alpha(l, x + 16, y + 12, 0.7f); }
        gfx_recorte(x, y, w, 178);
        for (j = 0; j < 4; j++) {
          GfxRect r = { x + 16 + j * (fw + 14), y + 42 + (120 - fh), fw, fh };
          if (j == 0) gfx_rect((GfxRect){ r.x - 10, r.y + 4, r.w + 20, r.h + 26 }, 0, GFX_SOMBRA, 1, 0, 0, 0.5f, 0, 0, 0, 0.5f);
          ajImgFil(idx, j, deit, r, 10, j ? 0.8f : 1);
        }
        gfx_sem_recorte();
        y += 178 + 24; }
      ajKickerC("Card · proporções da Home", x, y, 1, w);
      y += 18 + 12;
      ajFormas(x, y, w, tipo, idx);
    }
  }
  // DICAS
  { AjDica d[4];
    int nd = 0;
    if (filPegou) {
#ifdef NV_TOUCH_UI
      if (layout_modo_mobile()) {
        d[nd++] = (AjDica){ "Toque", "Escolher destino" }; d[nd++] = (AjDica){ "Voltar", "Cancelar" };
      } else {
        d[nd++] = (AjDica){ "↑ ↓", filPegou == 2 ? "Mover o bloco" : "Mover" }; d[nd++] = (AjDica){ "← →", filPegou == 2 ? "Só a fileira" : "Bloco do addon" };
        d[nd++] = (AjDica){ "OK", "Soltar" }; d[nd++] = (AjDica){ "Voltar", "Cancelar" };
      }
#else
      d[nd++] = (AjDica){ "↑ ↓", filPegou == 2 ? "Mover o bloco" : "Mover" }; d[nd++] = (AjDica){ "← →", filPegou == 2 ? "Só a fileira" : "Bloco do addon" };
      d[nd++] = (AjDica){ "OK", "Soltar" }; d[nd++] = (AjDica){ "Voltar", "Cancelar" };
#endif
    }
    else if (filNaBarra) {
      d[nd++] = (AjDica){ "← →", "Mover na barra" };
      d[nd++] = (AjDica){ "OK", filBarra <= 1 ? "Abrir a aba" : filBarra == 2 ? "Alternar" : "Atualizar" };
      d[nd++] = (AjDica){ "↓", "Lista" }; d[nd++] = (AjDica){ "Voltar", "Fechar" };
    }
    else if (filAba == 1 && filCampo == FIL_CAMPO_REGUA) { d[nd++] = (AjDica){ "↑ ↓", "Letra" }; d[nd++] = (AjDica){ "←", "Lista" }; d[nd++] = (AjDica){ "Voltar", "Fechar" }; }
    else if (filAba == 1 && filCampo == 1) { d[nd++] = (AjDica){ "OK", "Adicionar" }; d[nd++] = (AjDica){ "→", "Regua A-Z" }; d[nd++] = (AjDica){ "Voltar", "Fechar" }; }
    else if (idx == AJ_FIL_DESTAQUE) { d[nd++] = (AjDica){ "← →", "Trocar" }; d[nd++] = (AjDica){ "↑", "Abas" }; d[nd++] = (AjDica){ "Voltar", "Fechar" }; }
    else if (filAba == 1) { d[nd++] = (AjDica){ "OK", "Adicionar" }; d[nd++] = (AjDica){ "→", "Botão e regua A-Z" }; d[nd++] = (AjDica){ "Segurar ↑ ↓", "Pular letra" }; d[nd++] = (AjDica){ "↑", "Barra do topo" }; }
    else {
      d[nd++] = (AjDica){ "← →", "Coluna" };
      d[nd++] = (AjDica){ "OK", filCampo == 0 ? "Pegar e mover" : filCampo == 1 ? "Remover" : filCampo == 2 ? "Trocar o card" : "Trocar o tamanho" };
      if (filCampo != 0) d[nd++] = (AjDica){ "OK", "na 1ª coluna move" };
      else d[nd++] = (AjDica){ "Voltar", "Fechar" };
    }
    ajDicasQuebra(d, nd, x - 6, pr.y + pr.h - 30, w + 12, 1); }
}

