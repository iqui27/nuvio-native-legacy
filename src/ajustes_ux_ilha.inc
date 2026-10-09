// A TELA DE AJUSTES NO GLASS UI: inspetor, lista, diferencas e os modais
// (editor de escolha longa, editor numerico, confirmacao, dependencia).
// Incluido por ajustes_ux_desenho.inc, depois das pecas e das previas.

// --- INSPETOR -----------------------------------------------------------------------
static const char *ajTipoRotulo(int op) {
  if (inativa(op)) return "Indisponível agora";
  if (op == AJ_ENVIAR_LOG) return "Ação";
  switch (ajTipo(op)) {
    case AJL_TOG: return "Ligar ou desligar";
    case AJL_ACAO: return "Abre uma tela";
    case AJL_LER: return "Informação";
    default: return "Personalizar";
  }
}
static const char *ajIconeOpcao(int op) {
  return ajVisualIcone(op);
}
// As teclas que valem AGORA, pelo tipo da linha em foco.
static int ajDicasDe(int op, AjDica *d) {
  if (focoIndice) {
    d[0] = (AjDica){ "↑ ↓", "Navegar" }; d[1] = (AjDica){ "OK", "Abrir" };
    d[2] = (AjDica){ "Voltar", "Sair" };
    return 3;
  }
  if (op == AJ_TEMA && ajEditandoLinha(op)) {
    d[0] = (AjDica){ "← →", "Escolher" }; d[1] = (AjDica){ "↑ ↓", "Fileira" };
    d[2] = (AjDica){ "OK", "Confirmar" };
    return 3;
  }
  if (op >= 0 && ajEditandoLinha(op)) {
    d[0] = (AjDica){ "← →", "Escolher" }; d[1] = (AjDica){ "OK", "Confirmar" };
    d[2] = (AjDica){ "Voltar", "Cancelar" };
    return 3;
  }
  if (op >= 0 && ajSegmentada(op) && !inativa(op)) {
    d[0] = (AjDica){ "OK", "Mudar" }; d[1] = (AjDica){ "← →", "Escolher" };
    d[2] = (AjDica){ "Voltar", "Categorias" };
    return 3;
  }
  { int n = 0;
    if (op >= 0 && inativa(op) && uxRequisito(op) >= 0) d[n++] = (AjDica){ "OK", "Mostrar" };
    else if (op == AJ_ENVIAR_LOG && !inativa(op)) d[n++] = (AjDica){ "OK", "Enviar" };
    else if (op >= 0 && !inativa(op)) switch (ajTipo(op)) {
      case AJL_TOG: d[n++] = (AjDica){ "OK", "Mudar" }; break;
      case AJL_ESC: d[n++] = (AjDica){ "OK", op == AJ_TEMA ? "Mudar" : "Escolher" }; break;
      case AJL_NUM: d[n++] = (AjDica){ "OK", "Ajustar" }; break;
      case AJL_ACAO: d[n++] = (AjDica){ "OK", "Abrir" }; break;
      default: break;
    }
    d[n++] = (AjDica){ "↑ ↓", "Navegar" };
    d[n++] = (AjDica){ "←", "Categorias" };
    return n; }
}
// O inspetor da v2 (infoV2): padding 36, a previa no alto (612 a 100%),
// kicker 18 com icone 24, titulo 40/700, ajuda 26 (linha 1,45), meta 24 e as
// dicas 22 (tecla 18) no rodape.
static float aj2Meta(const char *k, const char *v, float x, float y, float w, float a) {
  TxtLinha lk = ajTxt(TXT_V2_26, k);
  TxtLinha lv = ajTxtC(TXT_V2_26, v, w - lk.w - 24);
  gfx_cor((GfxRect){ x, y, w, 1 }, 0, 1, 1, 1, 0.07f * a);
  txt_desenhar_alpha(lk, x, y + 14 + (29 - lk.h) * 0.5f, 0.45f * a);
  txt_desenhar_alpha(lv, x + w - lv.w, y + 14 + (29 - lv.h) * 0.5f, 0.85f * a);
  return 14 + 29 + 14;
}
static float aj2Ef(const char *icone, const char *s, float x, float y, float w, float a) {
  float tw = w - 22 - 26 - 14 - 22, h;
  h = txt_bloco(TXT_V2_26, s, 0, 0, 0, x, -4000, tw, 34.8f, 0, 6);
  ajNeutro((GfxRect){ x, y, w, h + 40 }, 22, 0.05f, 0.106f, 0.110f, 0.129f, a);
  ajIcone(icone, x + 22, y + 22, 26, 0.55f * a);
  ajBloco(TXT_V2_26, s, x + 22 + 26 + 14, y + 20, tw, 34.8f, 0.72f * a, 6);
  return h + 40;
}
// Rodape de dicas: fio de 7% e as teclas (quebram linha se nao couberem).
static void aj2Dicas(const AjDica *d, int n, float x, float yBase, float w, float a) {
  float wl = aj2DicasLinha(d, n, x, 0, TXT_V2_KBD18, 36, 40, 11, TXT_V2_24, 10, 24, 0, a);
  if (wl > w - 12.0f && n > 1) {
    gfx_cor((GfxRect){ x, yBase - 36.0f * 2 - 12.0f - 20.0f, w, 1 }, 0, 1, 1, 1, 0.07f * a);
    aj2DicasLinha(d, n - 1, x + 6.0f, yBase - 36.0f - 12.0f - 18.0f, TXT_V2_KBD18, 36, 40, 11, TXT_V2_24, 10, 24, 1, a);
    aj2DicasLinha(d + n - 1, 1, x + 6.0f, yBase - 18.0f, TXT_V2_KBD18, 36, 40, 11, TXT_V2_24, 10, 24, 1, a);
    return;
  }
  gfx_cor((GfxRect){ x, yBase - 36.0f - 20.0f, w, 1 }, 0, 1, 1, 1, 0.07f * a);
  aj2DicasLinha(d, n, x + 6.0f, yBase - 18.0f, TXT_V2_KBD18, 36, 40, 11, TXT_V2_24, 10, 24, 1, a);
}
static float aj2Previa(int op, float x, float y, float w) {
  return ajPrevia(op, x, y, w);   // 2.0.2: a cor tambem e a previa gerada
}
// A COLUNA DA ARTE (A3): a previa da opcao em foco grande, sem ilha, com o
// segmentado da opcao (80/90/100...) sobre o canto de baixo; embaixo o kicker
// (a categoria, caixa alta no acento claro), o titulo da opcao em 46/700 e a
// descricao em 22 (ate 3 linhas). Sobrando altura, o "o que muda" e o "Vale
// para / Padrao". As dicas foram para o rodape da lista.
static void ajDesenharInspetor(void) {
  GfxRect ir = aj2InspR();
  float x = ir.x, w = ir.w, y = ir.y + 4.0f, fim = ir.y + ir.h - 8.0f;
  int categoria = focoIndice;
  int op = categoria ? -1 : focoOp;
  if (!aj2TemInsp()) return;
  { int opPv = op >= 0 ? op : -1;
    // Categoria em foco (cabecalho ou menu): a imagem da categoria, nao a
    // previa da primeira opcao dela.
    if (opPv < 0) y += ajCenaTroca(AJ_TROCA_INSPETOR, secAtual, -1, x, y, w, 0);
    if (opPv >= 0) {
      // Editando na linha, a previa ja mostra o valor PENDENTE (so desenho).
      int salvo = valor[opPv], ed = ajEditandoLinha(opPv);
      float ph, caixa = w * 9.0f / 16.0f;
      if (ed && opPv != AJ_FUNDO) valor[opPv] = uxPendente;
      ajMiniRotuloV2 = 1;
      ph = ajCenaTroca(AJ_TROCA_INSPETOR, secAtual, opPv, x, y, w, 0);
      ajMiniRotuloV2 = 0;
      if (ed) valor[opPv] = salvo;
      // O segmentado da opcao em foco, no canto de baixo da previa, sobre um
      // fundo escuro (a arte por tras pode ser clara).
      if (op >= 0 && ajSegmentada(op) && !inativa(op)) {
        int n = nValores(op);
        float bx = x + w - 26.0f, by = y + (ph > 0 && ph < caixa ? ph : caixa) - 26.0f - 31.5f, sw;
        if (n > 3) n = 3;
        sw = aj2SegLargura(op, n);
        gfx_cor((GfxRect){ bx - sw - 6.0f, by - 37.5f, sw + 12.0f, 75.0f }, 0.5f, 0, 0, 0, 0.45f);
        aj2Seg(op, n, ed ? uxPendente : valor[op], ed, bx, by, 1);
      }
      if (ph > 0) y += ph;
    } }
  y += 30.0f;
  // Kicker: a categoria (na lista de diferencas, de onde a opcao e).
  ajKickerAcento(TELA[secIni[secAtual]].titulo, x + 2.0f, y, 1);
  y += 22.0f + 12.0f;
  // Cor de destaque: o titulo e o NOME da cor (a pendente, editando), como
  // no acentos-mockup.html; a opcao ja esta no rotulo da linha.
  { const char *t = categoria ? TELA[secIni[secAtual]].titulo
                  : op == AJ_TEMA ? V_TEMA[ajEditandoLinha(op) ? uxPendente : valor[AJ_TEMA]]
                  : rotuloOpcao(op);
    y += ajBloco(TXT_V2_A3TIT, t, x, y, w, 56.0f, 1, 2); }
  y += 10.0f;
  { int maxL = (int)((fim - y) / 32.0f);
    if (maxL > 5) maxL = 5;   // a descricao inteira: cortar no meio da frase nao serve
    if (maxL < 1) maxL = 1;
    y += ajBloco(TXT_V2_24, categoria ? SECAO_AJUDA[secAtual] : uxResumo(op), x, y, w, 32.0f, 0.68f, maxL);
    if (op == AJ_TEMA) {   // a cor PENDENTE, como a previa
      int salvo = valor[AJ_TEMA];
      if (ajEditandoLinha(AJ_TEMA)) valor[AJ_TEMA] = uxPendente;
      y += 18.0f;
      y += aj2CorMedidas(x, y, w, fim, 1);
      valor[AJ_TEMA] = salvo;
    }
  if (!categoria) {
    const char *ef = efeitoOpcao(op);
    if (ef && ef[0] && y + 18.0f + txt_bloco(TXT_V2_26, i18n(ef), 0, 0, 0, x, -4000, w - 84, 34.8f, 0, 6) + 40 <= fim) {
      y += 18.0f; y += aj2Ef("aj_info", i18n(ef), x, y, w, 1); }
    if (uxTemPadrao(op) && op != AJ_TEMA && y + 18.0f + 2 * 57.0f <= fim) {
      char padrao[160];
      uxValorTexto(op, uxValorPadrao(op), padrao, sizeof padrao);
      y += 18.0f;
      y += aj2Meta(i18n("Vale para"), i18n(uxEscopo(op)), x, y, w, 1);
      y += aj2Meta(i18n("Padrão"), padrao, x, y, w, 1);
    }
    // ENVIAR REGISTRO: o ultimo envio, o ultimo codigo e o destino.
    if (op == AJ_ENVIAR_LOG) {
      AvisosEnvio env;
      char v[96], ha[48];
      time_t ult;
      int autom;
      avisos_envio_info(&env);
      autom = env.autoQuando > env.quando;
      ult = autom ? env.autoQuando : env.quando;
      y += 18.0f;
      if (ult) {
        long s = (long)(time(NULL) - ult);
        if (s < 60) snprintf(ha, sizeof ha, i18n("há %ld s"), s);
        else if (s < 3600) snprintf(ha, sizeof ha, i18n("há %ld min"), s / 60);
        else snprintf(ha, sizeof ha, i18n("há %ld h"), s / 3600);
        snprintf(v, sizeof v, "%s · %s", ha, autom ? i18n("automático") : i18n("manual"));
      } else snprintf(v, sizeof v, "%s", i18n("nenhum nesta sessão"));
      y += aj2Meta(i18n("Último envio"), v, x, y, w, 1);
      if (env.ultimoCodigo[0]) {
        struct tm a, b;
        time_t agora = time(NULL);
        localtime_r(&env.ultimoCodigoQuando, &a);
        localtime_r(&agora, &b);
        if (a.tm_yday == b.tm_yday && a.tm_year == b.tm_year)
          snprintf(v, sizeof v, i18n("%s · hoje %02d:%02d"), env.ultimoCodigo, a.tm_hour, a.tm_min);
        else if (b.tm_yday - a.tm_yday == 1 && a.tm_year == b.tm_year)
          snprintf(v, sizeof v, i18n("%s · ontem %02d:%02d"), env.ultimoCodigo, a.tm_hour, a.tm_min);
        else snprintf(v, sizeof v, i18n("%s · %02d/%02d %02d:%02d"), env.ultimoCodigo, a.tm_mday, a.tm_mon + 1, a.tm_hour, a.tm_min);
        y += aj2Meta(i18n("Último código"), v, x, y, w, 1);
      }
      y += aj2Meta(i18n("Destino"), env.disponivel ? i18n("servidor de registros do app") : i18n("indisponível nesta versão"), x, y, w, 1);
    }
  } }
}

static float ajAlturaItem(int i) {
  if (!visivel(i)) return 0.0f;
  switch (TELA[i].tipo) {
    case IT_SEC: return 0.0f;
    case IT_ROT: return AJ2_GRUPO_H;
    case IT_GRP: return AJ2_LN_GAP + AJ2_LN_H;
    default: return AJ2_LN_GAP + ajLinhaH(i);
  }
}

// --- A LISTA (v2) -------------------------------------------------------------------
// Alfa da mascara da lista: some nos ultimos 90 px de baixo e, rolada, nos 60
// de cima (o mask-image do mockup), medido no centro da peca.
static float ajMascara(float yc, float topo, float base) {
  float a = 1.0f;
  if (yc > base - 50.0f) a = (base - yc) / 50.0f;   // A3: o rodape tem lugar proprio
  if (scrollY > 1.0f && yc < topo + 60.0f) { float b = (yc - topo) / 60.0f; if (b < a) a = b; }
  return a < 0 ? 0 : a > 1 ? 1 : a;
}
// Cabecalho da lista (listaV2): padding 42 46 18 50; kicker 18/700, titulo
// 52/700 com 4 de margem, sub 24 com 8. Devolve onde a lista comeca.
static float aj2Cabecalho(GfxRect fr, const char *kick, const char *titulo, const char *sub,
                          float reservaDir, float a) {
  float x = fr.x + 50.0f, y = fr.y + 42.0f;
  ajCaps(TXT_V2_KICK, kick, 2.52f, x, y + (21.8f - 19.0f * 1.21f) * 0.5f, 0.45f * a);
  y += 21.8f + 4.0f;
  { TxtLinha t = ajTxtC(TXT_V2_TIT, titulo, fr.w - 96.0f - reservaDir);
    txt_desenhar_alpha(t, x, y + (62.9f - t.h) * 0.5f, a); }
  y += 62.9f + 8.0f;
  if (sub && sub[0]) {
    TxtLinha t = ajTxtC(TXT_V2_26, sub, fr.w - 96.0f);
    txt_desenhar_alpha(t, x, y + (29.0f - t.h) * 0.5f, 0.55f * a);
    y += 29.0f;
  }
  return y + 18.0f;
}
// A3: o cabecalho compacto da ilha da lista (o nome da categoria em 26/800 e,
// a direita, o chip Avancados, que continua focavel com ↑ da primeira linha).
// O kicker grande e a descricao da categoria moram agora na coluna da arte.
// 2.0.2: o titulo foi para o cabecalho da pagina (aj2DesenharPagina).
static void aj2CabecalhoA3(GfxRect fr, const char *titulo, float reservaDir, float a) {
  (void)fr; (void)titulo; (void)reservaDir; (void)a;
}
// RODAPE DA LISTA (A3): as dicas a esquerda e o aviso ("Ajuste salvo nesta
// TV.") a direita, na mesma faixa abaixo da lista. Antes o aviso era desenhado
// em cima da ultima linha (foto do dono, 04/10). Sem lugar para os dois, o
// aviso fica e as dicas somem enquanto ele dura.
static void aj2RodapeA3(GfxRect fr, const AjDica *d, int nd, float a) {
  float x = fr.x + 34.0f, w = fr.w - 68.0f, yc = fr.y + fr.h - AJ_A3_RODAPE * 0.5f - 6.0f;
  float wd = aj2DicasLinha(d, nd, x, yc, TXT_V2_KBD18, 34, 38, 10, TXT_V2_24, 8, 22, 0, a), wa = 0;
  int dicas = 1;
  gfx_cor((GfxRect){ fr.x + 28.0f, fr.y + fr.h - AJ_A3_RODAPE, fr.w - 56.0f, 1 }, 0, 1, 1, 1, 0.07f * a);
  if (uxAviso[0] && !(uxEditor && (uxRestaurar || uxAvisoRisco || uxEditor == 2))) {
    TxtLinha t = ajTxtC(TXT_V2_24, uxAviso, w - 34.0f);
    wa = t.w + 34.0f;
    if (wd + 24.0f + wa > w) dicas = 0;
    ajIcone("aj_check", x + w - wa, yc - 11.0f, 22.0f, 0.7f * a);
    txt_desenhar_alpha(t, x + w - t.w, yc - t.h * 0.5f, 0.75f * a);
  }
  if (dicas) aj2DicasLinha(d, nd, x, yc, TXT_V2_KBD18, 34, 38, 10, TXT_V2_24, 8, 22, 1, a);
}
static void ajDesenharFolha(void) {
  GfxRect fr = aj2ListaR();
  int sec = secAtual, i, nd;
  float topo, base, y, aPag = anim_suave(paginaA) * aj2TextoLista;
  const Item *s = &TELA[secIni[sec]];
  AjDica d[4];
  ajIlha(fr, 40.0f, 1, 0);
  aj2CabecalhoA3(fr, s->titulo, 20.0f, aPag);
  topo = AJ_TOPO;
  base = AJ_BASE;
  gfx_recorte(fr.x, topo, fr.w, base - topo);
  y = topo - scrollY;
  for (i = secIni[sec] + 1; i < secFim(sec); i++) {
    float h = ajAlturaItem(i);
    if (h <= 0.0f) continue;
    if (y + h >= topo - 4 && y <= base + 4) {
      if (TELA[i].tipo == IT_ROT) {
        ajDesenharGrupo(i, y, ajMascara(y + 20.0f + 15.6f, topo, base) * aPag);
      } else if (TELA[i].tipo == IT_OPC) {
        float lh = ajLinhaH(i), yy = y + AJ2_LN_GAP;
        // A superficie de foco nao some com o texto (editor por cima): o
        // alfa da mascara vai inteiro, o do texto em ajDesenharLinha.
        ajDesenharLinha(i, yy, animItem[i], ajMascara(yy + (lh > 102 ? AJ2_LN_LINHA1 * 0.5f : lh * 0.5f), topo, base) * anim_suave(paginaA));
      }
    }
    y += h;
  }
  gfx_sem_recorte();
  nd = ajDicasDe(focoIndice ? -1 : focoOp, d);
  aj2RodapeA3(fr, d, nd, aPag);
}

// --- MODAIS ----------------------------------------------------------------------
// Atras do modal a tela RECUA: as ilhas a 16% e um veu preto de 40% (42% no
// solido) por cima de tudo. O modal e uma ilha centrada com o miolo a 86%.
static void ajVeuModal(void) {
  gfx_cor((GfxRect){ 0, 0, NV_VTELA_W, NV_VTELA_H }, 0, 0, 0, 0, ajVid() ? 0.40f : 0.42f);
}
// Botao pilula 60 (21/600): o focado cheio no acento, o outro neutro 8%.
static float ajBotao(const char *rot, const char *icone, float x, float y, int foco, float a) {
  TxtLinha t = foco ? ajTxtT(TXT_ILHA_ITEM, rot) : ajTxt(TXT_ILHA_ITEM, rot);
  float w = 28 + (icone ? 22 + 10 : 0) + t.w + 28;
  if (foco) ajPilulaAcento((GfxRect){ x, y, w, 60 }, 30, a);
  else ajNeutro((GfxRect){ x, y, w, 60 }, 30, 0.08f, AJ_CHIP_SOL, a);
  if (icone) {
    if (foco) ajIconeT(icone, x + 28, y + 19, 22, a);
    else ajIcone(icone, x + 28, y + 19, 22, 0.88f * a);
  }
  txt_desenhar_alpha(t, x + 28 + (icone ? 32 : 0), y + (60 - t.h) * 0.5f, (foco ? 1.0f : 0.88f) * a);
  return w;
}
// Chip 56 (19/600) com icone: "Restaurar padrao" no rodape dos editores.
static float ajChip(const char *rot, const char *icone, float x, float y, int foco, float a) {
  TxtLinha t = foco ? ajTxtT(TXT_AJ_SEG, rot) : ajTxt(TXT_AJ_SEG, rot);
  float w = 22 + (icone ? 20 + 10 : 0) + t.w + 22;
  if (foco) ajPilulaAcento((GfxRect){ x, y, w, 56 }, 28, a);
  else ajNeutro((GfxRect){ x, y, w, 56 }, 28, 0.08f, AJ_CHIP_SOL, a);
  if (icone) {
    if (foco) ajIconeT(icone, x + 22, y + 18, 20, a);
    else ajIcone(icone, x + 22, y + 18, 20, 0.85f * a);
  }
  txt_desenhar_alpha(t, x + 22 + (icone ? 30 : 0), y + (56 - t.h) * 0.5f, (foco ? 1.0f : 0.85f) * a);
  return w;
}
// A previa da opcao com o valor PENDENTE (so o desenho; nada e gravado).
static float aj2PreviaEditor(int op, float x, float y, float w);
static float aj2PreviaPendente(float x, float y, float w) {
  int salvo = valor[uxOp];
  float h;
  valor[uxOp] = uxPendente;
  h = aj2PreviaEditor(uxOp, x, y, w);
  valor[uxOp] = salvo;
  return h;
}
// Modal centrado na tela virtual, e no maximo do tamanho dela menos 40 de cada
// lado (escala.h). Em 100% os numeros fixos de cada modal ja sao os centrados.
static void ajCentraModal(GfxRect *m) {
  if (ajustes_tamanho_ajustes() == 1.0f) return;
  if (m->w > NV_VTELA_W - 80.0f) m->w = NV_VTELA_W - 80.0f;
  if (m->h > NV_VTELA_H - 80.0f) m->h = NV_VTELA_H - 80.0f;
  m->x = (NV_VTELA_W - m->w) * 0.5f;
  m->y = (NV_VTELA_H - m->h) * 0.5f;
}

// Confirmacao: kicker com o alerta ambar, pergunta 36/700, texto e "de › para".
static void ajConfirmacao(const char *kick, int alerta, const char *pergunta, const char *texto,
                          const char *de, const char *para, const char *sim, const char *nao, int focoSim) {
  GfxRect m = { 560, 300, 800, 0 };
  float x = m.x + 44, y, hTxt;
  hTxt = txt_bloco(TXT_AJ_TEXTO, texto, 0, 0, 0, 0, -4000, m.w - 88, 30, 0, 6);
  m.h = 44 + 22 + 8 + 44 + 14 + hTxt + (de ? 22 + 24 : 0) + 34 + 60 + 44;
  m.y = m.h < 480 ? 300 : (NV_VTELA_H - m.h) * 0.5f;
  if (ajustes_tamanho_ajustes() != 1.0f) { ajCentraModal(&m); x = m.x + 44; }
  ajIlha(m, 36, 1, 1);
  y = m.y + 44;
  if (alerta) { gfx_icone((GfxRect){ x, y - 2, 22, 22 }, "aj_triangle-alert", 0.961f, 0.784f, 0.298f, 1); ajKicker(kick, x + 34, y, 1); }
  else ajKicker(kick, x, y, 1);
  y += 22 + 8;
  { TxtLinha t = ajTxtC(TXT_ILHA_PERGUNTA, pergunta, m.w - 88); txt_desenhar(t, x, y); y += 44; }
  y += 14;
  y += ajBloco(TXT_AJ_TEXTO, texto, x, y, m.w - 88, 30, 0.62f, 6);
  if (de) {
    TxtLinha a = ajTxt(TXT_AJ_SUB, de), b = ajTxt(TXT_AJ_SEG, para);
    y += 22;
    txt_desenhar_alpha(a, x, y, 0.55f);
    ajIcone("aj_chevron-right", x + a.w + 16, y + (a.h - 20) * 0.5f, 20, 1);
    txt_desenhar(b, x + a.w + 16 + 20 + 16, y);
    y += 24;
  }
  y += 34;
  { float bx = x;
    bx += ajBotao(sim, NULL, bx, y, focoSim, 1) + 12;
    ajBotao(nao, NULL, bx, y, !focoSim, 1); }
}

// Dependencia: a opcao esta apagada por causa de outra.
static void ajDependencia(void) {
  int req = uxRequisito(uxOp);
  GfxRect m = { 560, 0, 800, 0 };
  float x = m.x + 44, y, hTxt;
  char k[200];
  const char *b = uxBloco(uxOp);
  hTxt = txt_bloco(TXT_AJ_TEXTO, ajudaOpcao(uxOp), 0, 0, 0, 0, -4000, m.w - 88, 30, 0, 6);
  m.h = 44 + 30 + 44 + 14 + hTxt + 34 + 60 + 44;
  m.y = (NV_VTELA_H - m.h) * 0.5f;
  ajCentraModal(&m);
  x = m.x + 44;
  ajIlha(m, 36, 1, 1);
  y = m.y + 44;
  if (b[0]) snprintf(k, sizeof k, "%s · %s", i18n(uxCaminho(uxOp)), i18n(b));
  else snprintf(k, sizeof k, "%s", i18n(uxCaminho(uxOp)));
  ajKicker(k, x, y, 1);
  y += 30;
  { TxtLinha t = ajTxtC(TXT_ILHA_PERGUNTA, rotuloOpcao(uxOp), m.w - 88); txt_desenhar(t, x, y); y += 44 + 14; }
  y += ajBloco(TXT_AJ_TEXTO, ajudaOpcao(uxOp), x, y, m.w - 88, 30, 0.62f, 6) + 34;
  { float bx = x;
    if (req >= 0) bx += ajBotao("Ver o ajuste necessário", NULL, bx, y, 1, 1) + 12;
    ajBotao("Fechar", NULL, bx, y, req < 0, 1); }
}

static void ajDesenharModais(void) {
  if (!uxEditor || ajEditandoLinha(uxOp)) return;
  // v2: os editores sao o terceiro degrau da pilha (aj2DesenharEditor); aqui
  // ficam so as perguntas — confirmar, restaurar e a dependencia.
  if (!uxRestaurar && !uxAvisoRisco && uxEditor != 2) return;
  ajVeuModal();
  if (uxRestaurar || uxAvisoRisco) {
    char padrao[160], de[96], para[96], texto[500];
    uxValorTexto(uxOp, uxValorPadrao(uxOp), padrao, sizeof padrao);
    if (uxAvisoRisco) {
      int fil = uxAvisoRisco == SEG_AVISO_FILEIRAS;
      char k[200];
      const char *b = uxBloco(uxOp);
      if (b[0]) snprintf(k, sizeof k, "%s · %s", i18n(uxCaminho(uxOp)), i18n(b));
      else snprintf(k, sizeof k, "%s", i18n(uxCaminho(uxOp)));
      uxValorTexto(uxOp, uxOriginal, de, sizeof de);
      uxValorTexto(uxOp, uxPendente, para, sizeof para);
      if (fil) { snprintf(de, sizeof de, i18n("%d fileiras"), uxOriginal); snprintf(para, sizeof para, i18n("%d fileiras"), uxPendente); }
      ajConfirmacao(k, 1, fil ? "Mais fileiras na Home" : "Mais itens por fileira",
                    fil ? i18n("Mais fileiras usam mais memória e rede; em TVs com 1 GB a Home pode ficar lenta ou fechar. Se o app fechar, ele volta sozinho ao valor anterior.")
                        : i18n("Mais itens por fileira usam mais memória; em TVs com 1 GB a Home pode ficar lenta ou fechar. Se o app fechar, ele volta sozinho ao valor anterior."),
                    de, para, "Continuar", "Cancelar", uxConfirmar);
    } else {
      snprintf(texto, sizeof texto, i18n("O valor voltará para “%s”. %s"), padrao,
        uxOp == AJ_AUTO_CREDITOS ? i18n("A mudança vale só para este perfil nesta TV. Nenhum serviço será desconectado.") :
        !strcmp(uxEscopoValor(uxOp, uxValorPadrao(uxOp)), "Conta/perfil") ? i18n("Esta preferência participa dos ajustes do perfil. Nenhum serviço será desconectado.") : i18n("A mudança vale só nesta TV. Nenhum serviço será desconectado."));
      uxValorTexto(uxOp, uxValorAtual(uxOp), de, sizeof de);
      ajConfirmacao(rotuloOpcao(uxOp), 0, "Restaurar este ajuste?", texto, de, padrao, "Restaurar", "Cancelar", uxConfirmar);
    }
  } else if (uxEditor == 2) ajDependencia();
  if (uxAviso[0]) {
    TxtLinha t = ajTxtC(TXT_AJ_SUB, uxAviso, 1100);
    txt_desenhar_alpha(t, (NV_VTELA_W - t.w) * 0.5f, NV_VTELA_H - 70, 0.8f);
  }
}

// A folha do aviso de memoria das setas (riscoFolha), no mesmo modal.
static void desenhaRiscoFolha(void) {
  char de[64], para[64];
  int fil = riscoFolha == SEG_AVISO_FILEIRAS;
  int v = riscoAtual(riscoFolhaOp), nv = v + riscoFolhaDir * (OPCOES[riscoFolhaOp].tipo == OP_NUMERO ? OPCOES[riscoFolhaOp].passo : 1);
  ajVeuModal();
  if (fil) { snprintf(de, sizeof de, i18n("%d fileiras"), v); snprintf(para, sizeof para, i18n("%d fileiras"), nv); }
  else { uxValorTexto(riscoFolhaOp, v, de, sizeof de); uxValorTexto(riscoFolhaOp, nv, para, sizeof para); }
  ajConfirmacao(fil ? "Tela inicial · Fileiras" : uxCaminho(riscoFolhaOp), 1, riscoFolhaTitulo(), i18n(riscoFolhaTexto()),
                de, para, "Continuar", "Cancelar", riscoFolhaFoco == 0);
}

// VINCULO (Trakt/Simkl) como ilha modal: o QR real (src/qr.c) num ladrilho
// branco, endereco e codigo grandes, e o "aguardando" com o ponto que respira
// da ilha do relogio. O codigo curto e o endereco continuam ao lado do QR:
// a pagina de ativacao pede o codigo depois de aberta.
#ifdef AJUSTES_TESTE
static int ajVinculoTeste;
#endif
static void desenhaVinculo(const char *servico, const char *codigo,
                           const char *endereco, const char *falha, int esperando) {
  GfxRect m = { 410, 230, 1100, 620 };
  float x, y;
  char t[96];
  ajCentraModal(&m);
  ajIlha(m, 36, 1, 1);
  x = m.x + 56 + 356 + 56; y = m.y + 50;
  ajKicker("Fontes e addons", x, y, 1);
  y += 22 + 4;
  snprintf(t, sizeof t, i18n("Conectar %s"), servico);
  { TxtLinha l = ajTxtC(TXT_ILHA_TITULO, t, m.x + m.w - 56 - x); txt_desenhar(l, x, y); y += 48; }
  if (falha && falha[0]) {
    TxtLinha l = txt_linha_corta(TXT_AJ_NOME, falha, 236, 108, 108, 255, m.x + m.w - 56 - x);
    txt_desenhar(l, x, y + 30);
    { AjDica d[2] = { { "OK", "Tentar de novo" }, { "Voltar", "Fechar" } };
      ajDicasLinha(d, 2, x, m.y + m.h - 50 - 30, 1); }
    return;
  }
  if (!codigo || !codigo[0]) {
    TxtLinha l = ajTxt(TXT_AJ_SUB, "Preparando o código…");
    txt_desenhar_alpha(l, x, y + 30, 0.6f);
    return;
  }
  qrVinTex(endereco);
  { GfxRect tile = { m.x + 56, m.y + (m.h - 356) * 0.5f, 356, 356 };
    gfx_cor(tile, 28.0f / 356, 1, 1, 1, 1);
    if (texQrVin) {
      gfx_tex_aspect_atual = 0.0f;
      gfx_rect((GfxRect){ tile.x + 14, tile.y + 14, 328, 328 }, texQrVin, GFX_SNAP, 0, 0, 0, 0, 0, 0, 0, 1);
    } }
  y += 30;
  { TxtLinha l = ajTxt(TXT_AJ_TEXTO, "Aponte a câmera do celular, ou abra:"); txt_desenhar_alpha(l, x, y, 0.6f); y += l.h + 6; }
  { char e[200]; const char *p = endereco ? strstr(endereco, "://") : NULL;
    snprintf(e, sizeof e, "%s", p ? p + 3 : endereco ? endereco : "-");
    { TxtLinha l = ajTxtC(TXT_ILHA_TITULO, e, m.x + m.w - 56 - x); txt_desenhar(l, x, y); y += l.h + 26; } }
  { TxtLinha l = ajTxt(TXT_AJ_TEXTO, "e informe o código:"); txt_desenhar_alpha(l, x, y, 0.6f); y += l.h + 4; }
  txt_tracking(TXT_AJ_NUM64, codigo, 243, 242, 239, x, y, 1.0f, 14.0f);
  { float by = m.y + m.h - 50 - 30;
    AjDica d[1] = { { "Voltar", "Fechar" } };
    float dw = ajDicasLargura(d, 1);
    if (esperando) {
      float ar, ag, ab, p = ajustes_animacoes_reduzidas() ? 1.0f
                          : 0.55f + 0.45f * sinf((float)SDL_GetTicks() * (2.0f * 3.14159265f / 1200.0f));
      TxtLinha l = ajTxt(TXT_AJ_ESTADO, "Aguardando a autorização…");
      ajustes_acento(&ar, &ag, &ab);
      gfx_cor((GfxRect){ x - 6, by + 15 - 11, 22, 22 }, 0.5f, ar, ag, ab, 0.22f * p);
      gfx_cor((GfxRect){ x, by + 10, 10, 10 }, 0.5f, ar, ag, ab, p);
      txt_desenhar_alpha(l, x + 20, by + (30 - l.h) * 0.5f, 0.62f);
    }
    ajDicasLinha(d, 1, m.x + m.w - 56 - dw, by, 1); }
}
static int ajVinculoAtivo(void) {
  TraEstado ta = traktauth_estado();
  SmkEstado sa = simklauth_estado();
  DisEstado da = discord_estado();
#ifdef AJUSTES_TESTE
  if (ajVinculoTeste) return 1;
#endif
  return ta == TRA_PEDINDO || ta == TRA_AGUARDANDO || ta == TRA_ERRO ||
         sa == SMK_PEDINDO || sa == SMK_AGUARDANDO || sa == SMK_ERRO ||
         da == DIS_PEDINDO || da == DIS_AGUARDANDO || da == DIS_ERRO;
}

// --- O EDITOR (v2: terceiro degrau) ------------------------------------------------
// Desliza por cima da lista ate x = 300, com o miolo modal (88%): kicker 18,
// titulo 52, "Escopo · Padrão", e o conteudo em duas colunas — o editor a
// esquerda e a previa (520) a direita. Rodape: "Restaurar padrão" (64) e as
// dicas 22.
static int aj2EdVisto;
static float aj2CabecalhoEditor(GfxRect m) {
  char k[200], linha[320], padrao[160];
  const char *b = uxBloco(uxOp);
  float x = m.x + 52, y = m.y + 40;
  if (b[0]) snprintf(k, sizeof k, "%s · %s", i18n(uxCaminho(uxOp)), i18n(b));
  else snprintf(k, sizeof k, "%s", i18n(uxCaminho(uxOp)));
  ajCaps(TXT_V2_KICK, k, 2.52f, x, y + (21.8f - 19.0f * 1.21f) * 0.5f, 0.45f);
  y += 21.8f + 4.0f;
  { TxtLinha t = ajTxtC(TXT_V2_TIT, rotuloOpcao(uxOp), m.w - 104);
    txt_desenhar(t, x, y + (62.9f - t.h) * 0.5f); y += 62.9f + 8.0f; }
  if (ajPadraoVisual(uxOp) >= 0) uxValorTexto(uxOp, ajPadraoVisual(uxOp), padrao, sizeof padrao);
  if (ajPadraoVisual(uxOp) >= 0) snprintf(linha, sizeof linha, i18n("%s · Padrão: %s"), i18n(uxEscopoValor(uxOp, uxPendente)), padrao);
  else snprintf(linha, sizeof linha, "%s", i18n(uxEscopoValor(uxOp, uxPendente)));
  { TxtLinha t = ajTxtC(TXT_V2_26, linha, m.w - 104);
    txt_desenhar_alpha(t, x, y + (29.0f - t.h) * 0.5f, 0.55f); }
  return y + 29.0f + 30.0f;
}
static void aj2RodapeEditor(GfxRect m, float colW, const AjDica *d, int n) {
  float y = m.y + m.h - 40 - 64;
  if (uxTemPadrao(uxOp)) {
    TxtLinha t = uxRodape ? ajTxtT(TXT_V2_SEG, "Restaurar padrão") : ajTxt(TXT_V2_SEG, "Restaurar padrão");
    float w = 24 + 24 + 10 + t.w + 24;
    if (uxRodape) ajPilulaAcento((GfxRect){ m.x + 52, y, w, 64 }, 32, 1);
    else ajNeutro((GfxRect){ m.x + 52, y, w, 64 }, 32, 0.08f, AJ_CHIP_SOL, 1);
    if (uxRodape) ajIconeT("aj_rotate-ccw-clock", m.x + 52 + 24, y + 20, 24, 1);
    else ajIcone("aj_rotate-ccw-clock", m.x + 52 + 24, y + 20, 24, 0.85f);
    txt_desenhar_alpha(t, m.x + 52 + 58, y + (64 - t.h) * 0.5f, uxRodape ? 1.0f : 0.85f);
  }
  { float w = aj2DicasLinha(d, n, 0, 0, TXT_V2_KBD18, 36, 40, 11, TXT_V2_24, 10, 24, 0, 1);
    aj2DicasLinha(d, n, m.x + 52 + colW - w, y + 32, TXT_V2_KBD18, 36, 40, 11, TXT_V2_24, 10, 24, 1, 1); }
}
// A ordem real da Home ao lado do limite (gr-wrap: padding 20 24, linhas de
// 44, nome 22, posicao 17, cartazes 24 x 36).
static float aj2OrdemHome(float x, float y, float w) {
  int ordem[40], n = 0, i, salvo = uxOriginal;
  int pend = valor[AJ_FIL_LIMITE], lim, corte;
  float h;
  for (i = 0; i < fil_n() && n < 40; i++) if (fil_estado(i) != FIL_FORA) ordem[n++] = i;
  lim = pend > salvo ? pend : salvo;
  if (lim > n) lim = n;
  if (lim > 13) lim = 13;
  corte = pend < salvo ? pend : salvo;
  h = 20 * 2 + lim * 44 + (corte < lim ? 40 : 0);
  ajCaps(TXT_V2_KICK, "Ordem da Home", 2.52f, x, y + (21.8f - 19.0f * 1.21f) * 0.5f, 0.45f);
  y += 21.8f + 14.0f;
  ajNeutro((GfxRect){ x, y, w, h }, 16, 0.04f, 0.106f, 0.110f, 0.129f, 1);
  y += 20;
  for (i = 0; i < lim; i++) {
    int f = ordem[i], k, novo = pend > salvo && i >= salvo && i < pend, sai = pend < salvo && i >= pend;
    char num[8];
    TxtLinha l;
    float rx = x + 24, rw = w - 48;
    if (i == corte && corte < lim) {
      char t[64];
      snprintf(t, sizeof t, pend > salvo ? i18n("Entram com %d") : i18n("Saem com %d"), pend);
      { float dw = ajCaps(TXT_G16B, t, 1.92f, -1, 0, 0), cx = rx + (rw - dw) * 0.5f;
        gfx_cor((GfxRect){ rx, y + 20, cx - 12 - rx, 1 }, 0, 1, 1, 1, 0.14f);
        gfx_cor((GfxRect){ cx + dw + 12, y + 20, rx + rw - cx - dw - 12, 1 }, 0, 1, 1, 1, 0.14f);
        ajCaps(TXT_G16B, t, 1.92f, cx, y + 20 - 10, 0.4f); }
      y += 40;
    }
    snprintf(num, sizeof num, "%d", i + 1);
    l = ajTxt(TXT_V2_18, num);
    txt_desenhar_alpha(l, rx + 26 - l.w, y + (44 - l.h) * 0.5f, 0.35f);
    ajIcone(fil_origem_icone(fil_linha_origem(f)), rx + 26 + 14, y + 11, 22, 0.45f);
    l = ajTxtC(TXT_V2_24, fil_titulo(f)[0] ? fil_titulo(f) : fil_chave(f), rw - 26 - 14 - 22 - 14 - 3 * 24 - 2 * 6 - 14);
    txt_desenhar_alpha(l, rx + 26 + 14 + 22 + 14, y + (44 - l.h) * 0.5f, sai ? 0.35f : (novo || i >= salvo) ? 1.0f : 0.62f);
    for (k = 0; k < 3; k++)
      ajIcone("aj_image", rx + rw - 3 * 24 - 2 * 14 + k * 38, y + 10, 24, sai ? 0.4f : 1.0f);
    y += 44;
  }
  return 21.8f + 14.0f + h;
}
static float aj2PreviaEditor(int op, float x, float y, float w) {
  if (op == AJ_FIL_LIMITE) return aj2OrdemHome(x, y, w);
  return ajPreviaEditor(op, x, y, w);
}
static void aj2EditorNumero(GfxRect m) {
  const Opcao *o = &OPCOES[uxOp];
  float y, lx, lw, ar, ag, ab, pw = 520.0f;
  char v[64], salvo[64], uni[64];
  int risco = uxOp == AJ_FIL_LIMITE ? FIL_LIMITE_SEGURO : -1;
  ajustes_acento(&ar, &ag, &ab);
  y = aj2CabecalhoEditor(m);
  if (m.w - 104 - pw - 56 < 560) pw = 0;
  lx = m.x + 52; lw = pw > 0 ? m.w - 104 - pw - 56 : m.w - 104;
  snprintf(v, sizeof v, "%d", uxPendente);
  uxValorTexto(uxOp, uxOriginal, salvo, sizeof salvo);
  if (uxOp == AJ_FIL_LIMITE) snprintf(uni, sizeof uni, "%s", i18n("fileiras"));
  else snprintf(uni, sizeof uni, "%s", o->sufixo ? o->sufixo + (o->sufixo[0] == ' ') : "");
  { GfxRect r = { lx, y, lw, 34 + 150 + 40 + 10 + 14 + 40 + 34 };
    float fr = ajMovFracao(uxOp, uxPendente), fp = ajPadraoVisual(uxOp) >= 0 ? ajFracao(uxOp, ajPadraoVisual(uxOp)) : -1;
    float tx = r.x + 40, tw = r.w - 80, ty = r.y + 34 + 150 + 40, cy = r.y + 34 + 75;
    TxtLinha big, u = ajTxt(TXT_V2_36B, uni), ls;
    char sv[96];
    if (uxOp == AJ_HERO_TRAILER_ESPERA) snprintf(v, sizeof v, "%d,%d", uxPendente / 10, uxPendente % 10);
    big = ajTxt(TXT_V2_NUM150, v);
    if (ajVid()) gfx_cor(r, 30.0f / r.h, 1, 1, 1, uxRodape ? 0.0f : 0.12f);
    else ajFocoLinha(r, 30, uxRodape ? 0 : 1, 1);
    txt_desenhar(big, tx - 4, cy - big.h * 0.5f);
    snprintf(sv, sizeof sv, i18n("salvo: %s"), salvo);
    ls = ajTxt(TXT_V2_26, sv);
    { float bx = tx - 4 + big.w + 30, bh = 43.6f + 6 + 29;
      txt_desenhar(u, bx, cy - bh * 0.5f + (43.6f - u.h) * 0.5f);
      txt_desenhar_alpha(ls, bx, cy - bh * 0.5f + 43.6f + 6 + (29 - ls.h) * 0.5f, 0.5f); }
    ajIcone("aj_plus", r.x + r.w - 40 - 40, cy - 20, 40, 0.75f);
    ajIcone("aj_minus", r.x + r.w - 40 - 96, cy - 20, 40, 0.75f);
    gfx_cor((GfxRect){ tx, ty, tw, 10 }, 0.5f, 1, 1, 1, 0.12f);
    if (fr > 0) gfx_cor((GfxRect){ tx, ty, tw * (fr > 1 ? 1 : fr), 10 }, 0.5f, ar, ag, ab, 1);
    if (risco >= o->min && risco < o->max) {
      float fx = ajFracao(uxOp, uxPendente > risco ? uxPendente : risco), sx;
      // Faixa vigiada: listras ambar a 135 graus (repeating-linear-gradient).
      gfx_recorte(tx + tw * fx, ty, tw * (1 - fx), 10);
      for (sx = tx + tw * fx - 10; sx < tx + tw; sx += 14.0f) {
        int q;
        for (q = 0; q < 5; q++) gfx_cor((GfxRect){ sx + q * 2.0f, ty + q * 2.0f, 7, 2 }, 0, 0.91f, 0.722f, 0.29f, 0.32f);
      }
      gfx_sem_recorte();
    }
    if (fp >= 0) gfx_cor((GfxRect){ tx + tw * fp - 1.5f, ty - 11, 3, 32 }, 0.5f, 0.953f, 0.949f, 0.937f, 0.6f);
    { char t[64]; TxtLinha l;
      float lc = ty + 10 + 14 + 13.5f;
      snprintf(t, sizeof t, "%d", o->min); l = ajTxt(TXT_V2_24, t); txt_desenhar_alpha(l, tx, lc - l.h * 0.5f, 0.5f);
      snprintf(t, sizeof t, "%d", o->max); l = ajTxt(TXT_V2_24, t); txt_desenhar_alpha(l, tx + tw - l.w, lc - l.h * 0.5f, 0.5f);
      if (fp >= 0) {
        char pv[64];
        uxValorTexto(uxOp, ajPadraoVisual(uxOp), pv, sizeof pv);
        snprintf(t, sizeof t, i18n("padrão %s"), pv); l = ajTxt(TXT_V2_24, t);
        txt_desenhar_alpha(l, tx + tw * fp - l.w * 0.5f, lc - l.h * 0.5f, 0.5f);
      }
      if (risco >= o->min && risco < o->max && uxPendente > uxValorAtual(uxOp)) {
        snprintf(t, sizeof t, "%d", uxPendente);
        l = txt_linha(TXT_G23B, t, 232, 184, 74, 255);
        txt_desenhar(l, tx + tw * fr - l.w * 0.5f, lc - l.h * 0.5f);
      } }
    y += r.h; }
  if (uxOp == AJ_FIL_LIMITE) {
    char t[400];
    snprintf(t, sizeof t, i18n("Mais fileiras usam mais memória e rede: em TV com 1 GB a Home pode ficar lenta ou fechar. Acima de %d a TV pede confirmação."), FIL_LIMITE_SEGURO);
    y += 22; y += aj2Ef("aj_triangle-alert", t, lx, y, lw, 1);
  }
  { const char *ef = uxOp == AJ_FIL_LIMITE ? NULL : efeitoOpcao(uxOp);
    if (ef && ef[0] && y + 12 + 80 <= m.y + m.h - 40 - 64 - 20) {
      y += 12; y += aj2Ef("aj_info", i18n(ef), lx, y, lw, 1); } }
  if (pw > 0) aj2PreviaPendente(m.x + m.w - 52 - pw, aj2CabecalhoEditor(m), pw);
  { AjDica d[3] = { { "← →", "Ajustar" }, { "OK", "Confirmar" }, { "Voltar", "Cancelar" } };
    aj2RodapeEditor(m, lw, d, 3); }
}
static void aj2EditorEscolha(GfxRect m) {
  const Opcao *o = &OPCOES[uxOp];
  int n = nValores(uxOp), i, primeiro, vis;
  float y, lx, lw, pw = 520.0f, y0;
  y = aj2CabecalhoEditor(m);
  y0 = y;
  if (m.w - 104 - pw - 56 < 560) pw = 0;
  lx = m.x + 52; lw = pw > 0 ? m.w - 104 - pw - 56 : m.w - 104;
  vis = (int)((m.y + m.h - 40 - 64 - 24 - y) / 100.0f);
  if (vis < 3) vis = 3;
  primeiro = uxPendente - vis / 2;
  if (primeiro + vis > n) primeiro = n - vis;
  if (primeiro < 0) primeiro = 0;
  for (i = primeiro; i < n && i < primeiro + vis; i++) {
    char t[160];
    int foco = !uxRodape && i == uxPendente;
    GfxRect r = { lx, y, lw, 96 };
    int fv;
    uxValorTexto(uxOp, i, t, sizeof t);
    if (foco) ajFocoLinha(r, 28, 1, 1);
    fv = marcaDoValor(uxOp, o->valores ? o->valores[i] : NULL);
    if (fv >= 0) marca_formato((FormatoMarca)fv, r.x + 30, r.y + 26, 44, 1, 1, 1, foco ? 0.9f : 0.6f);
    else {
      float textoX = r.x + 30;
      if (uxOp == AJ_ICONE_APP) {
        iconeapp_desenhar(i, (GfxRect){r.x + 24, r.y + 16, 64, 64}, 1, foco ? 1.0f : 0.7f);
        textoX = r.x + 110;
      }
      if (uxOp == AJ_LOGO_APP) {
        logoapp_miniatura(i, (GfxRect){r.x + 24, r.y + 16, 64, 64}, 1, foco ? 1.0f : 0.7f);
        textoX = r.x + 110;
      }
      TxtLinha l = ajTxtC(TXT_V2_ROT, t, lw - 210 - (textoX - r.x));
      txt_desenhar_alpha(l, textoX, r.y + (96 - l.h) * 0.5f, foco ? 1.0f : 0.88f);
    }
    if (i == uxOriginal) {
      TxtLinha s = ajTxt(TXT_V2_28, "Salvo");
      float sx = r.x + r.w - 30 - s.w;
      txt_desenhar_alpha(s, sx, r.y + (96 - s.h) * 0.5f, 0.55f);
      ajIcone("aj_check", sx - 12 - 26, r.y + 35, 26, 0.55f);
    }
    y += 100;
  }
  if (pw > 0) aj2PreviaPendente(m.x + m.w - 52 - pw, y0, pw);
  { AjDica d[3] = { { "↑ ↓", "Escolher" }, { "OK", "Confirmar" }, { "Voltar", "Cancelar" } };
    aj2RodapeEditor(m, lw, d, 3); }
}
static int aj2EditorPainel(void) {
  return uxEditor == 1 && uxOp >= 0 && !ajEmLinha(uxOp);
}
static void aj2DesenharEditor(void) {
  GfxRect m = aj2EditorR();
  ajIlhaMiolo(m, 40, 0.88f);
  if (OPCOES[uxOp].tipo == OP_NUMERO) aj2EditorNumero(m);
  else aj2EditorEscolha(m);
}

// --- DIFERENTES DO PADRAO (degrau 1) ------------------------------------------------
static void aj2DesenharDiferencas(void) {
  GfxRect fr = aj2ListaR();
  float topo = AJ_TOPO, base = AJ_BASE + AJ_A3_RODAPE, y = topo + 8.0f;
  int primeiro = uxDifFoco > 9 ? uxDifFoco - 9 : 0, i;
  float x = fr.x + 18, w = fr.w - 36;
  ajIlha(fr, 40.0f, 1, 0);
  aj2CabecalhoA3(fr, "Diferentes do padrão", 0, 1);
  gfx_recorte(fr.x, topo, fr.w, base - topo);
  if (!uxNDifs) {
    TxtLinha t = ajTxt(TXT_V2_LN, "Tudo está no padrão");
    txt_desenhar(t, x + 22, y + 20);
    y += 76;
  }
  for (i = primeiro; i < uxNDifs; i++) {
    char atual[160], padrao[160], det[300];
    int op = uxDifs[i], foco = !focoIndice && i == uxDifFoco;
    GfxRect r = { x, y, w, 76 };
    float xd = x + w - 22, a = ajMascara(y + 38, topo, base), yc = y + 38;
    TxtLinha la, lp, lr, ls;
    if (y > base) break;
    if (foco) {
      float ar, ag, ab;
      ajFocoLinha(r, 18, 1, a);
      ajustes_acento(&ar, &ag, &ab);
      gfx_cor(r, 18.0f / r.h, ar, ag, ab, 0.14f * a);
    }
    uxValorTexto(op, uxValorAtual(op), atual, sizeof atual);
    uxValorTexto(op, uxValorPadrao(op), padrao, sizeof padrao);
    la = ajTxtC(TXT_V2_24, atual, 380);
    lp = ajTxtC(TXT_V2_24, padrao, 300);
    txt_desenhar_alpha(la, xd - la.w, yc - la.h * 0.5f, (foco ? 1.0f : 0.8f) * a);
    xd -= la.w + 12;
    if (op == AJ_TEMA) {
      float sr, sg, sb;
      ajustes_acento(&sr, &sg, &sb);
      gfx_cor((GfxRect){ xd - 20, yc - 10, 20, 20 }, 0.5f, sr, sg, sb, a);
      xd -= 30;
    }
    ajIcone("aj_chevron-right", xd - 20, yc - 10, 20, 0.35f * a);
    xd -= 20 + 12;
    txt_desenhar_alpha(lp, xd - lp.w, yc - lp.h * 0.5f, 0.4f * a);
    gfx_cor((GfxRect){ xd - lp.w, yc, lp.w, 2 }, 0, 0.953f, 0.949f, 0.937f, 0.25f * a);
    xd -= lp.w;
    lr = ajTxtC(foco ? TXT_V2_LN_B : TXT_V2_LN, rotuloOpcao(op), xd - x - 44);
    snprintf(det, sizeof det, "%s · %s", i18n(uxCaminho(op)), i18n(uxEscopo(op)));
    ls = ajTxtC(TXT_V2_18, det, xd - x - 44);
    txt_desenhar_alpha(lr, x + 22, y + 12 + (32.0f - lr.h) * 0.5f, (foco ? 1.0f : 0.88f) * a);
    txt_desenhar_alpha(ls, x + 22, y + 12 + 32.0f + 4 + (22.0f - ls.h) * 0.5f, 0.5f * a);
    y += 78;
  }
  if (y < base - 60) {
    TxtLinha n = ajTxt(TXT_V2_24, "Seus serviços e credenciais não entram nesta lista.");
    txt_desenhar_alpha(n, x + 22, y + 24, 0.42f);
  }
  gfx_sem_recorte();
}
static void aj2InspetorDiferencas(void) {
  GfxRect ir = aj2InspR();
  if (!aj2TemInsp()) return;
  // A3: a coluna da arte, sem ilha (como a das categorias).
  if (uxNDifs) {
    int op = uxDifs[uxDifFoco], salvo = valor[op], i;
    float x = ir.x, w = ir.w, y = ir.y + 4, cw = (w - 16) * 0.5f, ph = 0;
    char padrao[160], atual[160], t[220];
    uxValorTexto(op, uxValorPadrao(op), padrao, sizeof padrao);
    uxValorTexto(op, salvo, atual, sizeof atual);
    for (i = 0; i < 2; i++) {
      float h;
      valor[op] = i == 0 ? uxValorPadrao(op) : salvo;
      h = ajPreviaCompacta(op, x + i * (cw + 16), y, cw);
      if (h > ph) ph = h;
    }
    valor[op] = salvo;
    if (ph > 0) {
      TxtLinha c;
      snprintf(t, sizeof t, i18n("Padrão · %s"), padrao);
      c = ajTxtC(TXT_V2_24, t, cw);
      txt_desenhar_alpha(c, x, y + ph + 10, 0.45f);
      snprintf(t, sizeof t, i18n("Atual · %s"), atual);
      c = ajTxtC(TXT_V2_24, t, cw);
      txt_desenhar(c, x + cw + 16, y + ph + 10);
      y += ph + 10 + 26;
    }
    y += 28;
    { char k[40];
      snprintf(k, sizeof k, i18n("%d de %d"), uxDifFoco + 1, uxNDifs);
      ajIcone("aj_rotate-ccw-clock", x, y, 24, 0.45f);
      ajCaps(TXT_V2_KICK, k, 2.52f, x + 36, y + 0.5f, 0.45f);
      y += 34; }
    y += ajBloco(TXT_V2_A3TIT, rotuloOpcao(op), x, y, w, 56, 1, 2) + 10;
    y += ajBloco(TXT_V2_24, uxResumo(op), x, y, w, 32.0f, 0.68f, 3) + 18;
    y += aj2Meta(i18n("Vale para"), i18n(uxEscopo(op)), x, y, w, 1);
    { const char *b = uxBloco(op);
      char sb[200];
      if (b[0]) snprintf(sb, sizeof sb, "%s › %s", i18n(uxCaminho(op)), i18n(b));
      else snprintf(sb, sizeof sb, "%s", i18n(uxCaminho(op)));
      aj2Meta(i18n("Seção"), sb, x, y, w, 1); }
  }
  { AjDica d[3] = { { "OK", "Revisar" }, { "↑ ↓", "Navegar" }, { "←", "Categorias" } };
    aj2Dicas(d, 3, ir.x, ir.y + ir.h - 8, ir.w, 1); }
}

// --- A TELA ---------------------------------------------------------------------
static int ajModalAberto(void) {
  return (uxEditor && (uxRestaurar || uxAvisoRisco || uxEditor == 2)) || riscoFolha || frAberta || ajVinculoAtivo() ||
         teclado_aberto() || registro_envio_aberto();
}
static void ajDesenharTela(void) {
  int modal = ajModalAberto();
  Aj2Pose p = aj2PoseAgora();
  float g0;
  ajFundo();
  if (filAberta) return;   // a folha de fileiras ocupa a tela (desenhada depois)
  if (modal) gfx_opacidade_grupo = 0.16f;
  g0 = gfx_opacidade_grupo;
  aj2DesenharMenu(&p);
  if (p.rA > 0.003f) aj2DesenharGrade(p.rA);
  if (p.rA < 0.997f && uxIndice >= 1) aj2DesenharPagina(1.0f - p.rA);
  // O inspetor antes da lista: na transicao a lista desliza por cima dele.
  if (p.iA > 0.003f && uxIndice >= 1) {
    GfxRect ir = aj2InspR();
    gfx_opacidade_grupo = g0 * p.iA;
    gfx_transformar(ir.x, ir.y, 1, p.iDx, 0);
    if (uxIndice >= 2) ajDesenharInspetor(); else aj2InspetorDiferencas();
    gfx_sem_transformar();
  }
  // A LISTA nasce da linha da categoria: escala em torno dela.
  if (p.lA > 0.003f && uxIndice >= 1) {
    GfxRect fr = aj2ListaR();
    float oy = p.lOy > 0 ? p.lOy : aj2GradeOrigemY(uxIndice);
    gfx_opacidade_grupo = g0 * p.lA;
    gfx_transformar(fr.x, oy, p.lS, p.lDx, 0);
    aj2TextoLista = p.lTxt;
    if (uxIndice >= 2) ajDesenharFolha(); else aj2DesenharDiferencas();
    gfx_sem_transformar();
  }
  if (aj2EditorPainel()) aj2EdVisto = 1;
  else if (p.eA <= 0.003f) aj2EdVisto = 0;
  if (p.eA > 0.003f && aj2EdVisto && uxOp >= 0) {
    GfxRect m = aj2EditorR(), fr = aj2ListaR();
    float oy = fr.y + fr.h * 0.5f;
    gfx_opacidade_grupo = g0 * p.eA;
    gfx_transformar(m.x, oy, p.eS, p.eDx, 0);
    aj2DesenharEditor();
    gfx_sem_transformar();
  }
  gfx_opacidade_grupo = 1.0f;
  if (modal && !(uxEditor && (uxRestaurar || uxAvisoRisco || uxEditor == 2)) && !riscoFolha && !teclado_aberto()) ajVeuModal();
  // O aviso mora no rodape da lista (aj2RodapeA3); com o editor por cima (ou na
  // lista de diferencas, que nao tem rodape de aviso) ele vai na base do painel.
  if (uxAviso[0] && !(uxEditor && (uxRestaurar || uxAvisoRisco || uxEditor == 2)) && (aj2EdVisto || uxIndice < 2)) {
    GfxRect fr = aj2EdVisto ? aj2EditorR() : aj2ListaR();
    TxtLinha t = ajTxtC(TXT_V2_24, uxAviso, fr.w - 100);
    txt_desenhar_alpha(t, fr.x + 50, fr.y + fr.h - 30, 0.7f);
  }
  ajDesenharModais();
  if (registro_envio_aberto()) registro_envio_desenhar();
}

// --- ADDONS (addonsui.c) COMO UMA CATEGORIA DE AJUSTES ------------------------------
// A tela de addons no mesmo arranjo: indice com Contas aberta, a folha com os
// addons (rosto neutro, o que fornecem embaixo do nome, o chip Ligado) e o
// inspetor com o que o manifesto declara.
static const char *ajAddonFornece(int i) {
  static char b[96];
  if (!addons_sondado(i)) return i18n("conferindo o manifesto…");
  b[0] = 0;
  if (addons_fornece(i, ADD_CATALOGO)) strcat(b, i18n("Catálogo"));
  if (addons_fornece(i, ADD_STREAM)) { if (b[0]) strcat(b, " · "); strcat(b, i18n("Fontes")); }
  if (addons_fornece(i, ADD_LEGENDA)) { if (b[0]) strcat(b, " · "); strcat(b, i18n("Legendas")); }
  if (!b[0]) snprintf(b, sizeof b, "%s", i18n("não fornece nada que este app use"));
  return b;
}
static void ajRosto(const char *nome, float x, float y, float lado, float raio, float vidA, float tinta, int estilo) {
  char ini[8] = "?";
  TxtLinha l;
  if (nome && nome[0]) {
    unsigned char c0 = (unsigned char)nome[0];
    int nb = c0 >= 0xF0 ? 4 : c0 >= 0xE0 ? 3 : c0 >= 0xC0 ? 2 : 1;
    snprintf(ini, sizeof ini, "%.*s", nb, nome);
    if (ini[0] >= 'a' && ini[0] <= 'z') ini[0] = (char)(ini[0] - 32);
  }
  ajNeutro((GfxRect){ x, y, lado, lado }, raio, vidA, 0.141f, 0.149f, 0.173f, 1);
  l = ajTxt((TxtEstilo)estilo, ini);
  txt_desenhar_alpha(l, x + (lado - l.w) * 0.5f, y + (lado - l.h) * 0.5f, tinta);
}
static void ajDesenharAddons(int foco);
void ajustes_desenhar_addons(int foco) {
  AJ_ESCALA_INI();
  ajDesenharAddons(foco);
  AJ_ESCALA_FIM();
}
static void ajDesenharAddons(int foco) {
  int n = addons_n(), i, lig = 0, salvoIdx = uxIndice, salvoFoco = focoIndice;
  GfxRect fr, ir;
  float topo, base = NV_VTELA_H - 40.0f / ajustes_tamanho_ajustes(), y, alvo;
  char sub[96];
  montarTela();
  for (i = 0; i < n; i++) if (addons_ativo(i)) lig++;
  ajFundo();
  uxIndice = uxSecaoDe(AJ_ADDONS) + 2; focoIndice = 0;
  ajDesenharIndice();
  uxIndice = salvoIdx; focoIndice = salvoFoco;
  fr = ajFolhaR(); ir = ajInspR();
  ajIlha(fr, 36, 1, 0);
  snprintf(sub, sizeof sub, i18n("%d de %d ligados nesta TV"), lig, n);
  topo = ajCabecalhoFolha(fr, "Fontes e addons", "Addons", n ? sub : "Nenhum addon nesta conta.", 0, 1);
  if (foco >= n) foco = n - 1;
  alvo = foco * 84.0f - (base - topo - 80.0f) * 0.5f;
  if (alvo > n * 84.0f - (base - topo)) alvo = n * 80.0f - (base - topo);
  if (alvo < 0) alvo = 0;
  gfx_recorte(fr.x, topo, fr.w, base - topo);
  y = topo - alvo;
  for (i = 0; i < n; i++, y += 84) {
    GfxRect r = { fr.x + 18, y, fr.w - 36, 80 };
    float a = ajMascara(y + 40, topo, base);
    int ligado = addons_ativo(i), f = i == foco;
    TxtLinha t, c;
    if (y + 80 < topo || y > base) continue;
    if (f) ajFocoLinha(r, 22, 1, a);
    ajRosto(addons_nome(i), r.x + 22, y + 16, 48, 14, 0.09f, 0.85f * a, TXT_ILHA_INICIAL);
    { float cw = ajChipLiga(r.x + r.w - 22, y + 40, ligado, a);
      t = ajTxtC(TXT_AJ_ROTULO, addons_nome(i), r.w - 22 - 64 - cw - 40);
      c = ajTxtC(TXT_ILHA_GENERO, ajAddonFornece(i), r.w - 22 - 64 - cw - 40); }
    txt_desenhar_alpha(t, r.x + 22 + 48 + 16, y + 40 - (t.h + 4 + c.h) * 0.5f, (f ? 1.0f : 0.86f) * a);
    txt_desenhar_alpha(c, r.x + 22 + 48 + 16, y + 40 - (t.h + 4 + c.h) * 0.5f + t.h + 4, 0.42f * a);
  }
  gfx_sem_recorte();
  if (!ajTemInspetor()) return;
  ajIlha(ir, 36, 1, 0);
  if (n > 0) {
    float x = ir.x + 32, w = ir.w - 64, py = ir.y + 32, ix = x + 26;
    static const int REC[3] = { ADD_CATALOGO, ADD_STREAM, ADD_LEGENDA };
    static const char *const NOME[3] = { "Catálogo", "Fontes", "Legendas" };
    const char *base0 = addons_base(foco);
    char host[160] = "";
    float h = 26 + 64 + 24 + 18 + 12 + 44 + 22 + 2 * 42.5f + 22;
    if (base0) {
      const char *p = strstr(base0, "://");
      p = p ? p + 3 : base0;
      snprintf(host, sizeof host, "%.*s", (int)strcspn(p, "/"), p);
    }
    ajNeutro((GfxRect){ x, py, w, h }, 22, 0.04f, 0.106f, 0.110f, 0.129f, 1);
    py += 26;
    ajRosto(addons_nome(foco), ix, py, 64, 20, 0.10f, 1, TXT_AJ_TIT28);
    { TxtLinha nm = ajTxtC(TXT_AJ_NOME, addons_nome(foco), w - 52 - 80);
      TxtLinha st = ajTxt(TXT_ILHA_GENERO, addons_sondado(foco) ? "manifesto conferido" : "conferindo o manifesto…");
      float ty = py + 32 - (nm.h + 4 + st.h) * 0.5f;
      txt_desenhar(nm, ix + 80, ty);
      txt_desenhar_alpha(st, ix + 80, ty + nm.h + 4, 0.55f); }
    py += 64 + 24;
    ajKicker("Fornece", ix, py, 1);
    py += 18 + 12;
    { float cx = ix;
      for (i = 0; i < 3; i++) {
        int sim = addons_fornece(foco, REC[i]);
        TxtLinha l = ajTxt(TXT_ILHA_GENERO, NOME[i]);
        float cw = 16 + 18 + 10 + l.w + 16;
        ajNeutro((GfxRect){ cx, py, cw, 44 }, 22, 0.08f, AJ_CHIP_SOL, 1);
        ajIcone(sim ? "aj_check" : "aj_x", cx + 16, py + 13, 18, sim ? 0.85f : 0.4f);
        txt_desenhar_alpha(l, cx + 16 + 18 + 10, py + (44 - l.h) * 0.5f, sim ? 0.85f : 0.4f);
        cx += cw + 10;
      } }
    py += 44 + 22;
    py += ajMeta(i18n("Nesta TV"), i18n(addons_ativo(foco) ? "Ligado" : "Desligado"), ix, py, w - 52, 1) - 4;
    ajMeta(i18n("Endereço"), host, ix, py, w - 52, 1);
    { float yy = ir.y + 32 + h + 26, tx = x + 6, tw = w - 12;
      ajIcone("aj_puzzle", tx, yy - 1, 20, 0.45f);
      ajCaps(TXT_MINI, "Ligar ou desligar", 2.1f, tx + 30, yy, 0.45f);
      yy += 28;
      yy += ajBloco(TXT_AJ_INSP, addons_nome(foco), tx, yy, tw, 38.4f, 1, 2) + 12;
      ajBloco(TXT_AJ_SUB, "A lista de addons da sua conta, para ligar e desligar cada um nesta TV.", tx, yy, tw, 28.5f, 0.62f, 5); }
  }
  { AjDica d[2] = { { "OK", "Ligar ou desligar" }, { "Voltar", "Ajustes" } };
    ajDicas(d, 2, ir.x + 32, ir.y + ir.h - 32, ir.w - 64, 1); }
}

// "Add-ons permitidos" / "Plugins permitidos" (#202): o modal com "Todos" no
// topo e um chip por nome. As linhas sao as da folha de Addons (84 px).
static void desenhaFolhaPermitidos(void) {
  int plugin = frAberta == 2, i, n;
  GfxRect m = { 460, 90, 1000, 900 };
  float topo, base, y, alvo;
  char sub[200];
  frMontar();
  n = frN + 1;
  ajVeuModal();
  ajCentraModal(&m);
  ajIlha(m, 36, 1, 1);
  snprintf(sub, sizeof sub, "%s", i18n(ajustes_fonte_usar_outros()
    ? "Nenhum marcado = todos. Os marcados vêm primeiro; os outros só tocam se eles não tiverem fonte."
    : "Nenhum marcado = todos. Só os marcados tocam no automático; sem fonte neles, a lista abre."));
  { char k[160];
    snprintf(k, sizeof k, "%s · %s", i18n("Fontes e addons"), i18n("Escolha da fonte"));
    topo = ajCabecalhoFolha(m, k, plugin ? "Plugins permitidos" : "Add-ons permitidos", sub, 0, 1); }
  base = m.y + m.h - 96.0f;
  alvo = frFoco * 84.0f - (base - topo - 80.0f) * 0.5f;
  if (alvo > n * 84.0f - (base - topo)) alvo = n * 84.0f - (base - topo);
  if (alvo < 0) alvo = 0;
  gfx_recorte(m.x, topo, m.w, base - topo);
  y = topo - alvo;
  for (i = 0; i < n; i++, y += 84) {
    GfxRect r = { m.x + 18, y, m.w - 36, 80 };
    int f = i == frFoco, lig;
    const char *nome, *apoio;
    float cw, tw;
    if (y + 80 < topo || y > base) continue;
    if (i == 0) {
      nome = plugin ? "Todos os plugins ligados" : "Todos os add-ons";
      apoio = "";
      lig = fonteregra_n(plugin) == 0;
    } else {
      nome = frNome[i - 1]; apoio = frApoio[i - 1];
      lig = fonteregra_contem(plugin, nome);
    }
    if (f) ajFocoLinha(r, 22, 1, 1);
    if (i == 0) {
      ajNeutro((GfxRect){ r.x + 22, y + 16, 48, 48 }, 14, 0.09f, 0.141f, 0.149f, 0.173f, 1);
      ajIcone(plugin ? "aj_plug" : "aj_puzzle", r.x + 22 + 14, y + 16 + 14, 20, 0.85f);
    } else ajRosto(nome, r.x + 22, y + 16, 48, 14, 0.09f, 0.85f, TXT_ILHA_INICIAL);
    cw = ajChipLiga(r.x + r.w - 22, y + 40, lig, 1);
    tw = r.w - 22 - 64 - cw - 40;
    // O nome do add-on e do usuario: nao passa por i18n.
    { TxtLinha t = i == 0 ? ajTxtC(TXT_AJ_ROTULO, nome, tw) : txt_linha_corta(TXT_AJ_ROTULO, nome, AJ_TX, tw);
      TxtLinha c = ajTxtC(TXT_ILHA_GENERO, apoio, tw);
      float ty = y + 40 - (t.h + (apoio[0] ? 4 + c.h : 0)) * 0.5f;
      txt_desenhar_alpha(t, r.x + 22 + 48 + 16, ty, f ? 1.0f : 0.86f);
      if (apoio[0]) txt_desenhar_alpha(c, r.x + 22 + 48 + 16, ty + t.h + 4, 0.42f); }
  }
  gfx_sem_recorte();
  { AjDica d[2] = { { "OK", "Ligar ou desligar" }, { "Voltar", "Fechar" } };
    ajDicas(d, 2, m.x + 44, m.y + m.h - 36, m.w - 88, 1); }
  if (uxAviso[0]) {
    TxtLinha t = ajTxtC(TXT_AJ_SUB, uxAviso, 1100);
    txt_desenhar_alpha(t, (NV_VTELA_W - t.w) * 0.5f, NV_VTELA_H - 50, 0.8f);
  }
}

// "Ordem dos add-ons" (2.0.2): a lista na ordem do automatico, com as setas de
// subir e descer em cada linha (o arranjo da folha de editar abas do social).
// "Sem ordem" no topo apaga a ordem.
static void desenhaFolhaOrdem(void) {
  int i, n;
  GfxRect m = { 460, 90, 1000, 900 };
  float topo, base, y, alvo;
  char sub[200], k[160];
  frMontar();
  n = frN + 1;
  ajVeuModal();
  ajCentraModal(&m);
  ajIlha(m, 36, 1, 1);
  snprintf(sub, sizeof sub, "%s", i18n("Esquerda e direita escolhem subir ou descer; OK move. Quem está fora da ordem toca depois de todos."));
  snprintf(k, sizeof k, "%s · %s", i18n("Fontes e addons"), i18n("Escolha da fonte"));
  topo = ajCabecalhoFolha(m, k, "Ordem dos add-ons", sub, 0, 1);
  base = m.y + m.h - 96.0f;
  alvo = frFoco * 84.0f - (base - topo - 80.0f) * 0.5f;
  if (alvo > n * 84.0f - (base - topo)) alvo = n * 84.0f - (base - topo);
  if (alvo < 0) alvo = 0;
  gfx_recorte(m.x, topo, m.w, base - topo);
  y = topo - alvo;
  for (i = 0; i < n; i++, y += 84) {
    GfxRect r = { m.x + 18, y, m.w - 36, 80 };
    int f = i == frFoco;
    if (y + 80 < topo || y > base) continue;
    if (f) ajFocoLinha(r, 22, 1, 1);
    if (i == 0) {
      TxtLinha t = ajTxtC(TXT_AJ_ROTULO, i18n("Sem ordem"), r.w - 120);
      ajNeutro((GfxRect){ r.x + 22, y + 16, 48, 48 }, 14, 0.09f, 0.141f, 0.149f, 0.173f, 1);
      ajIcone("aj_arrow-up-down", r.x + 22 + 14, y + 16 + 14, 20, 0.85f);
      txt_desenhar_alpha(t, r.x + 22 + 48 + 16, y + 40 - t.h * 0.5f, f ? 1.0f : 0.86f);
    } else {
      const char *nome = frNome[i - 1], *apoio = frApoio[i - 1];
      float bw = 56.0f, bx = r.x + r.w - 22 - bw * 2 - 8.0f, tw = bx - (r.x + 22 + 48 + 16) - 24;
      int c;
      char pos[16];
      TxtLinha t = txt_linha_corta(TXT_AJ_ROTULO, nome, AJ_TX, tw);
      TxtLinha cap = ajTxtC(TXT_ILHA_GENERO, apoio[0] ? i18n(apoio) : "", tw);
      float ty = y + 40 - (t.h + (apoio[0] ? 4 + cap.h : 0)) * 0.5f;
      snprintf(pos, sizeof pos, "%d", i);
      ajRosto(nome, r.x + 22, y + 16, 48, 14, 0.09f, 0.85f, TXT_ILHA_INICIAL);
      txt_desenhar_alpha(t, r.x + 22 + 48 + 16, ty, f ? 1.0f : 0.86f);
      if (apoio[0]) txt_desenhar_alpha(cap, r.x + 22 + 48 + 16, ty + t.h + 4, 0.42f);
      for (c = 0; c < 2; c++) {
        GfxRect b = { bx + c * (bw + 8.0f), y + 12, bw, bw };
        int bf = f && frCol == c, sem = (c == 0 && i == 1) || (c == 1 && i == frN);
        if (bf) ajPilulaAcento(b, 28, 1);
        else ajNeutro(b, 28, 0.08f, AJ_CHIP_SOL, 1);
        if (bf) ajIconeT(c ? "aj_arrow-down" : "aj_arrow-up", b.x + 14, b.y + 14, 28, sem ? 0.3f : 1.0f);
        else ajIcone(c ? "aj_arrow-down" : "aj_arrow-up", b.x + 14, b.y + 14, 28, sem ? 0.3f : 0.85f);
      }
    }
  }
  gfx_sem_recorte();
  { AjDica d[2] = { { "OK", "Mover" }, { "Voltar", "Fechar" } };
    ajDicas(d, 2, m.x + 44, m.y + m.h - 36, m.w - 88, 1); }
  if (uxAviso[0]) {
    TxtLinha t = ajTxtC(TXT_AJ_SUB, uxAviso, 1100);
    txt_desenhar_alpha(t, (NV_VTELA_W - t.w) * 0.5f, NV_VTELA_H - 50, 0.8f);
  }
}

// --- PLUGINS (pluginsui.c, F09) NO MESMO ARRANJO ----------------------------------
// Indice com Contas aberta, a folha com o liga/desliga, "Adicionar" e os
// repositorios (ou os scrapers de um), e o inspetor do item focado. Sem motor
// neste aparelho, a folha diz isso e nada mais (nunca uma linha que nao faz
// nada).
static const char *ajPlugEstado(int i) {
  switch (plugins_repo_estado(i)) {
    case 1:  return "manifesto conferido";
    case -1: return "o manifesto não respondeu";
    case 2:  return "Extensões CloudStream: só no app Android";
    default: return "conferindo o manifesto…";
  }
}
static void ajPlugHost(const char *url, char *dst, size_t tam) {
  const char *p = strstr(url, "://");
  p = p ? p + 3 : url;
  snprintf(dst, tam, "%.*s", (int)strcspn(p, "/"), p);
}
static void ajDesenharPlugins(const AjPluginsVista *v);
void ajustes_desenhar_plugins(const AjPluginsVista *v) {
  AJ_ESCALA_INI();
  ajDesenharPlugins(v);
  AJ_ESCALA_FIM();
}
static void ajDesenharPlugins(const AjPluginsVista *v) {
  int disp = plugins_disponivel(), nr = plugins_n_repos(), n, i, salvoIdx = uxIndice, salvoFoco = focoIndice;
  int foco = v->foco;
  GfxRect fr, ir;
  float topo, base = NV_VTELA_H - 40.0f / ajustes_tamanho_ajustes(), y, alvo;
  char sub[160], linha[200];
  PlugRepo rp;
  montarTela();
  ajFundo();
  uxIndice = uxSecaoDe(AJ_PLUGINS) + 2; focoIndice = 0;
  ajDesenharIndice();
  uxIndice = salvoIdx; focoIndice = salvoFoco;
  fr = ajFolhaR(); ir = ajInspR();
  ajIlha(fr, 36, 1, 0);
  memset(&rp, 0, sizeof rp);
  if (v->nivel == 1) plugins_repo(v->repo, &rp);
  if (!disp) snprintf(sub, sizeof sub, "%s", i18n("Indisponível nesta plataforma"));
  else if (v->nivel == 0) {
    int ns = 0, k;
    for (k = 0; k < nr; k++) ns += plugins_repo_scrapers(k, NULL);
    snprintf(sub, sizeof sub, i18n("%d repositórios · %d scrapers"), nr, ns);
  } else {
    int lig, tot = plugins_repo_scrapers(v->repo, &lig);
    snprintf(sub, sizeof sub, i18n("%d scrapers · %d ligados"), tot, lig);
  }
  topo = ajCabecalhoFolha(fr, v->nivel == 1 ? "Plugins" : "Fontes e addons",
                          v->nivel == 1 ? (rp.nome[0] ? rp.nome : rp.url) : "Plugins", sub, 0, 1);
  n = !disp ? 1 : v->nivel == 0 ? 2 + nr : 1 + plugins_repo_scrapers(v->repo, NULL);
  if (foco >= n) foco = n - 1;
  alvo = foco * 84.0f - (base - topo - 80.0f) * 0.5f;
  if (alvo > n * 84.0f - (base - topo)) alvo = n * 84.0f - (base - topo);
  if (alvo < 0) alvo = 0;
  gfx_recorte(fr.x, topo, fr.w, base - topo);
  y = topo - alvo;
  for (i = 0; i < n; i++, y += 84) {
    GfxRect r = { fr.x + 18, y, fr.w - 36, 80 };
    float a = ajMascara(y + 40, topo, base), cw = 0;
    int f = i == foco;
    const char *nome = "", *apoio = "", *icone = NULL;
    char nomeBuf[128] = "", apoioBuf[160] = "";
    int chip = -1;   // -1 sem chip; 0/1 desligado/ligado
    if (y + 80 < topo || y > base) continue;
    if (!disp) { nome = "Plugins"; apoio = "O motor de plugins não roda neste aparelho."; icone = "aj_puzzle"; }
    else if (v->nivel == 0 && i == 0) {
      nome = "Plugins"; apoio = "Scrapers dos repositórios Nuvio como mais uma fonte, ao lado dos add-ons"; icone = "aj_puzzle"; chip = plugins_ligado();
    } else if (v->nivel == 0 && i == 1) {
      nome = "Adicionar repositório"; apoio = "Digite a URL do manifest.json"; icone = "aj_plus";
    } else if (v->nivel == 0) {
      PlugRepo q; int lig, tot;
      plugins_repo(i - 2, &q);
      tot = plugins_repo_scrapers(i - 2, &lig);
      snprintf(nomeBuf, sizeof nomeBuf, "%s", q.nome[0] ? q.nome : q.url);
      nome = nomeBuf;
      if (plugins_repo_estado(i - 2) == 1) snprintf(apoioBuf, sizeof apoioBuf, i18n("%d scrapers · %d ligados"), tot, lig);
      else snprintf(apoioBuf, sizeof apoioBuf, "%s", i18n(ajPlugEstado(i - 2)));
      apoio = apoioBuf; chip = q.ativo;
    } else if (i == 0) {
      nome = "Remover repositório"; icone = "aj_x";
      apoio = v->armado ? "OK de novo remove o repositório" : "OK para remover";
    } else {
      PlugScraper s;
      if (plugins_scraper(v->repo, i - 1, &s)) {
        snprintf(nomeBuf, sizeof nomeBuf, "%s", s.nome);
        snprintf(apoioBuf, sizeof apoioBuf, "%s", i18n(s.filme && s.serie ? "Filmes e séries" : s.filme ? "Filmes" : "Séries"));
        nome = nomeBuf; apoio = apoioBuf; chip = s.ativo;
      }
    }
    if (f) ajFocoLinha(r, 22, 1, a);
    if (icone) {
      ajNeutro((GfxRect){ r.x + 22, y + 16, 48, 48 }, 14, 0.09f, 0.141f, 0.149f, 0.173f, a);
      ajIcone(icone, r.x + 22 + 14, y + 16 + 14, 20, 0.85f * a);
    } else ajRosto(nome, r.x + 22, y + 16, 48, 14, 0.09f, 0.85f * a, TXT_ILHA_INICIAL);
    if (chip >= 0) cw = ajChipLiga(r.x + r.w - 22, y + 40, chip, a);
    else if (v->nivel == 0 && i >= 2) cw = 0;
    { TxtLinha t = ajTxtC(TXT_AJ_ROTULO, nome, r.w - 22 - 64 - cw - 40);
      TxtLinha c = ajTxtC(TXT_ILHA_GENERO, apoio, r.w - 22 - 64 - cw - 40);
      float ty = y + 40 - (t.h + 4 + c.h) * 0.5f;
      txt_desenhar_alpha(t, r.x + 22 + 48 + 16, ty, (f ? 1.0f : 0.86f) * a);
      txt_desenhar_alpha(c, r.x + 22 + 48 + 16, ty + t.h + 4, 0.42f * a); }
  }
  gfx_sem_recorte();
  if (v->aviso && v->aviso[0]) {
    TxtLinha t = ajTxtC(TXT_AJ_SUB, v->aviso, fr.w - 100);
    txt_desenhar_alpha(t, fr.x + 50, fr.y + fr.h - 30 - t.h, 0.7f);
  }
  if (!ajTemInspetor()) return;
  ajIlha(ir, 36, 1, 0);
  { float x = ir.x + 32, w = ir.w - 64, py = ir.y + 32, tx = x + 6, tw = w - 12;
    const char *titulo = "Plugins", *texto =
      "Scrapers em JavaScript dos repositórios de plugins do Nuvio, como mais uma fonte na lista, ao lado dos add-ons. Os repositórios vêm da sua conta. Desligado por padrão.";
    char host[160] = "";
    if (!disp) texto = "O motor de plugins não roda neste aparelho.";
    else if (v->nivel == 0 && foco == 1) { titulo = "Adicionar repositório"; texto = "A URL do manifest.json. Ex.: raw.githubusercontent.com/usuario/repo/main"; }
    else if (v->nivel == 0 && foco >= 2 && plugins_repo(foco - 2, &rp)) {
      int lig, tot = plugins_repo_scrapers(foco - 2, &lig);
      ajPlugHost(rp.url, host, sizeof host);
      ajKicker("Repositório", tx, py, 1); py += 30;
      py += ajBloco(TXT_AJ_INSP, rp.nome[0] ? rp.nome : host, tx, py, tw, 38.4f, 1, 2) + 14;
      py += ajMeta(i18n("Conta"), i18n(rp.ativo ? "Ligado" : "Desligado"), tx, py, tw, 1) - 4;
      py += ajMeta(i18n("Estado"), i18n(ajPlugEstado(foco - 2)), tx, py, tw, 1) - 4;
      snprintf(linha, sizeof linha, i18n("%d scrapers · %d ligados"), tot, lig);
      py += ajMeta(i18n("Scrapers"), linha, tx, py, tw, 1) - 4;
      ajMeta(i18n("Endereço"), host, tx, py, tw, 1);
      titulo = NULL;
    } else if (v->nivel == 1 && foco >= 1) {
      PlugScraper s;
      if (plugins_scraper(v->repo, foco - 1, &s)) {
        ajKicker("Scraper", tx, py, 1); py += 30;
        py += ajBloco(TXT_AJ_INSP, s.nome, tx, py, tw, 38.4f, 1, 2) + 14;
        py += ajMeta(i18n("Nesta TV"), i18n(s.ativo ? "Ligado" : "Desligado"), tx, py, tw, 1) - 4;
        ajMeta(i18n("Tipo"), i18n(s.filme && s.serie ? "Filmes e séries" : s.filme ? "Filmes" : "Séries"), tx, py, tw, 1);
        titulo = NULL;
      }
    } else if (v->nivel == 1) { titulo = "Remover repositório"; texto = "Remove o repositório desta TV e da sua conta. Remover o último deixa a lista vazia também na conta."; }
    if (titulo) {
      ajIcone("aj_puzzle", tx, py - 1, 20, 0.45f);
      ajCaps(TXT_MINI, "Plugins", 2.1f, tx + 30, py, 0.45f);
      py += 28;
      py += ajBloco(TXT_AJ_INSP, titulo, tx, py, tw, 38.4f, 1, 2) + 12;
      ajBloco(TXT_AJ_SUB, texto, tx, py, tw, 28.5f, 0.62f, 7);
    } }
  if (!disp) { AjDica d[1] = { { "Voltar", "Ajustes" } }; ajDicas(d, 1, ir.x + 32, ir.y + ir.h - 32, ir.w - 64, 1); }
  else if (v->nivel == 0) {
    AjDica d[3] = { { "OK", foco == 1 ? "Adicionar" : "Ligar ou desligar" }, { "→", "Scrapers" }, { "Voltar", "Ajustes" } };
    if (foco < 2) d[1] = d[2];   // sem scrapers para abrir nesta linha
    ajDicas(d, foco >= 2 ? 3 : 2, ir.x + 32, ir.y + ir.h - 32, ir.w - 64, 1);
  } else {
    AjDica d[2] = { { "OK", foco == 0 ? "Remover" : "Ligar ou desligar" }, { "←", "Repositórios" } };
    ajDicas(d, 2, ir.x + 32, ir.y + ir.h - 32, ir.w - 64, 1);
  }
}

// --- KIT DAS ILHAS PARA AS TELAS VIZINHAS (diagnostico.c, livetvdiag.c) -----------
// As mesmas pecas de Ajustes, para o diagnostico, o teste de velocidade e o
// diagnostico da Live TV ornarem com a tela de onde saem. Ver ajustes.h.
void ajustes_ui_fundo(void) { ajFundo(); }
void ajustes_ui_arte(int n) { ajArteFundoN = n; }
void ajustes_ui_ilha(GfxRect r, float raioPx, int modal) { ajIlha(r, raioPx, 1, modal); }
void ajustes_ui_veu(void) { ajVeuModal(); }
float ajustes_ui_kicker(const char *s, float x, float y, float a) { return ajKicker(s, x, y, a); }
float ajustes_ui_meta(const char *k, const char *v, float x, float y, float w) { return ajMeta(k, v, x, y, w, 1); }
float ajustes_ui_ef(const char *icone, const char *s, float x, float y, float w) {
  float h = ajEf(icone, s, x, y, w, 1);
  // O alerta das telas vizinhas e AMBAR (aviso de consumo de dados).
  if (!strcmp(icone, "aj_triangle-alert"))
    gfx_icone((GfxRect){ x + 16, y + 16, 20, 20 }, icone, 0.910f, 0.722f, 0.290f, 1);
  return h;
}
float ajustes_ui_botao(const char *rot, const char *icone, float x, float y, int foco) { return ajBotao(rot, icone, x, y, foco, 1); }
float ajustes_ui_dicas(const char *const *teclas, const char *const *rotulos, int n, float x, float y, int desenha) {
  AjDica d[6];
  int i;
  if (n > 6) n = 6;
  for (i = 0; i < n; i++) { d[i].k = teclas[i]; d[i].l = rotulos[i]; }
  if (!desenha) return ajDicasLargura(d, n);
  return ajDicasLinha(d, n, x, y, 1);
}
void ajustes_ui_neutro(GfxRect r, float raioPx, float vidA) { ajNeutro(r, raioPx, vidA, 0.106f, 0.110f, 0.129f, 1); }
void ajustes_ui_foco_linha(GfxRect r, float raioPx) { ajFocoLinha(r, raioPx, 1, 1); }
// "Rotulo ........ a → b unidade" com a barra de cada valor na mesma escala.
float ajustes_ui_antes_depois(const char *rot, int a, int b, const char *unid, int max,
                              float x, float y, float w, int compacto, float cr, float cg, float cb) {
  float pad = compacto ? 9.0f : 14.0f, ar, ag, ab;
  char t[96];
  TxtLinha lr = ajTxtC(TXT_AJ_ESTADO, rot, w * 0.6f), lv;
  ajustes_acento(&ar, &ag, &ab);
  if (cr >= 0) { ar = cr; ag = cg; ab = cb; }
  gfx_cor((GfxRect){ x, y, w, 1 }, 0, 1, 1, 1, 0.07f);
  y += pad;
  txt_desenhar_alpha(lr, x, y, 0.5f);
  snprintf(t, sizeof t, "%d %s", b, i18n(unid));
  lv = txt_linha(TXT_AJ_ESTADO, t, AJ_TX);
  txt_desenhar_alpha(lv, x + w - lv.w, y, 0.9f);
  if (a != b) {
    char s[32];
    TxtLinha la;
    snprintf(s, sizeof s, "%d → ", a);
    la = ajTxt(TXT_AJ_ESTADO, s);
    txt_desenhar_alpha(la, x + w - lv.w - la.w, y, 0.45f);
  }
  y += lr.h + (compacto ? 7.0f : 10.0f);
  if (max <= 0) max = 1;
  gfx_cor((GfxRect){ x, y, w, 6 }, 0.5f, 1, 1, 1, 0.07f);
  if (a != b) {
    gfx_cor((GfxRect){ x, y, w * (b > max ? 1.0f : (float)b / max), 6 }, 0.5f, ar, ag, ab, 0.85f);
    gfx_cor((GfxRect){ x, y, w * (float)(a < b ? a : b) / max, 6 }, 0.5f, 0.953f, 0.949f, 0.937f, 0.4f);
  } else gfx_cor((GfxRect){ x, y, w * (a > max ? 1.0f : (float)a / max), 6 }, 0.5f, 0.953f, 0.949f, 0.937f, 0.28f);
  return pad + lr.h + (compacto ? 7.0f : 10.0f) + 6 + pad * 0.5f;
}
// O grafico de pressao da memoria (os ultimos 2 min) sem o painel em volta.
// O historico de EXEMPLO (o do mockup): sobe, passa do teto e volta. Usado
// pelas capturas e pela previa do cartao de novidades da 1.8.0.
static void ajMemExemplo(long *hist, long teto) {
  int i;
  for (i = 0; i < 120; i++) {
    double v = i < 30 ? .30 + i * .009 + sin(i * .9) * .01 : i < 52 ? .58 + (i - 30) * .0165 + sin(i * 1.3) * .012
             : i < 74 ? (i % 5 < 3 ? .91 : .80) + sin(i) * .015 : .66 - (i - 74) * .0008 + sin(i * .7) * .012;
    hist[i] = (long)(v * teto);
  }
}
static void ajGraficoMemoria(const long *hist, int nh, long teto, float x, float y, float w, float h);
void ajustes_ui_grafico_memoria(float x, float y, float w, float h) {
  long hist[120], teto = tex_orcamento_bytes();
  int nh = tex_historico(hist, 120);
#ifdef AJUSTES_TESTE
  if (ajMemFixa) { teto = 240L << 20; nh = 120; ajMemExemplo(hist, teto); }
#endif
  ajGraficoMemoria(hist, nh, teto, x, y, w, h);
}
void ajustes_ui_grafico_exemplo(float x, float y, float w, float h) {
  long hist[120];
  ajMemExemplo(hist, 240L << 20);
  ajGraficoMemoria(hist, 120, 240L << 20, x, y, w, h);
}
static void ajGraficoMemoria(const long *hist, int nh, long teto, float x, float y, float w, float h) {
  int i;
  float pw = w / 120.0f;
  if (teto <= 0) teto = 1;
  gfx_cor((GfxRect){ x, y, w, h }, 12.0f / h, 0, 0, 0, 0.18f);
  for (i = 0; i < nh; i++) {
    float v = (float)hist[i] / (float)teto, hh, cx, r2, g2, b2;
    if (v > 1) v = 1;
    hh = h * v;
    if (hh < 1) continue;
    corPressao(v, &r2, &g2, &b2);
    cx = x + w - (float)(nh - i) * pw;
    gfx_cor((GfxRect){ cx, y + h - hh, pw - 1.2f, hh }, 0, r2, g2, b2, 0.38f);
    gfx_cor((GfxRect){ cx, y + h - hh, pw - 1.2f, 2.5f }, 0, r2, g2, b2, 1);
  }
  { static const float F[2] = { 0.7f, 0.9f };
    static const char *const N[2] = { "70%", "90%" };
    int k;
    for (k = 0; k < 2; k++) {
      float ly = y + h * (1 - F[k]), dx;
      TxtLinha l = ajTxt(TXT_AJ_MINI12, N[k]);
      for (dx = 0; dx < w; dx += 10) gfx_cor((GfxRect){ x + dx, ly, 4, 1 }, 0, 1, 1, 1, 0.22f);
      txt_desenhar_alpha(l, x + 8, ly + 4, 0.45f);
    } }
}
#ifdef AJUSTES_TESTE
void ajustes_teste_mem_fixa(int liga) { ajMemFixa = liga; }
#endif
