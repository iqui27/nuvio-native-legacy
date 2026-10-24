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
  if (ajPaleta(op) && ajEditandoLinha(op)) {
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
    if (op < 0 && focoItem >= 0 && TELA[focoItem].tipo == IT_MAIS)
      d[n++] = (AjDica){ "OK", maisAberto[secAtual] ? "Esconder" : "Abrir" };
    else if (op >= 0 && inativa(op) && uxRequisito(op) >= 0) {
      int liga = uxLigarRequisitos(op, 0);
      d[n++] = (AjDica){ "OK", liga == 2 ? "Ligar as duas" : liga == 1 ? "Ligar a outra" : "Mostrar" };
    }
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
  TxtLinha lk = ajTxt(TXT_V3_INSP, k);
  TxtLinha lv = ajTxtC(TXT_V3_INSP, v, w - lk.w - 24);
  gfx_cor((GfxRect){ x, y, w, 1 }, 0, 1, 1, 1, 0.07f * a);
  txt_desenhar_alpha(lk, x, y + 14 + (29 - lk.h) * 0.5f, 0.45f * a);
  txt_desenhar_alpha(lv, x + w - lv.w, y + 14 + (29 - lv.h) * 0.5f, 0.85f * a);
  return 14 + 29 + 14;
}
// "Recomendado ......... 4K ✓" (mockup v3): o padrao de fabrica com o check.
static float aj2MetaRecomendado(const char *v, float x, float y, float w, float a) {
  TxtLinha lk = ajTxt(TXT_V3_INSP, "Recomendado");
  TxtLinha lv = ajTxtC(TXT_V3_INSP, v, w - lk.w - 60);
  gfx_cor((GfxRect){ x, y, w, 1 }, 0, 1, 1, 1, 0.07f * a);
  txt_desenhar_alpha(lk, x, y + 14 + (29 - lk.h) * 0.5f, 0.45f * a);
  txt_desenhar_alpha(lv, x + w - lv.w - 34, y + 14 + (29 - lv.h) * 0.5f, 0.85f * a);
  { float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
    gfx_icone((GfxRect){ x + w - 24, y + 14 + 2.5f, 24, 24 }, "aj_check", 0.55f + 0.45f * ar, 0.55f + 0.45f * ag, 0.55f + 0.45f * ab, a); }
  return 14 + 29 + 14;
}
// "Custo ......... (globe) usa mais internet" (mockup v3).
static float aj2MetaCusto(int op, float x, float y, float w, float a) {
  int c = ajCusto(op);
  const char *ic = (c & 1) ? "aj_zap" : (c & 2) ? "aj_globe" : "aj_key-round";
  TxtLinha lk = ajTxt(TXT_V3_INSP, "Custo");
  TxtLinha lv = ajTxtC(TXT_V3_INSP, ajCustoTexto(c), w - lk.w - 70);
  gfx_cor((GfxRect){ x, y, w, 1 }, 0, 1, 1, 1, 0.07f * a);
  txt_desenhar_alpha(lk, x, y + 14 + (29 - lk.h) * 0.5f, 0.45f * a);
  txt_desenhar_alpha(lv, x + w - lv.w, y + 14 + (29 - lv.h) * 0.5f, 0.85f * a);
  ajIcone(ic, x + w - lv.w - 34, y + 14 + 2.5f, 24, 0.7f * a);
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
  // Na linha "Mais opcoes" (focoOp -1) o inspetor fala da categoria: a cena
  // dela e o que ha dentro do bloco.
  int naMais = !focoIndice && focoOp < 0 && TELA[focoItem].tipo == IT_MAIS;
  int categoria = focoIndice || focoOp < 0;
  int op = categoria ? -1 : focoOp;
  int rtl = ajustes_idioma() == IDIOMA_AR;
  if (!aj2TemInsp()) return;
  if (ajRetrato()) {
    float pw = fminf(w, 560.0f), px = x + (w - pw) * 0.5f;
    int salvo = op >= 0 ? valor[op] : 0, ed = op >= 0 && ajEditandoLinha(op);
    if (ed && op != AJ_FUNDO) valor[op] = uxPendente;
    gfx_recorte(ir.x, ir.y, ir.w, ir.h);
    ajCenaTroca(AJ_TROCA_INSPETOR, secAtual, op, px, y, pw, 0);
    if (op >= 0) valor[op] = salvo;
    y += pw * 9.0f / 16.0f + 16.0f;
    const char *titulo = naMais ? "Mais opções" : categoria ? TELA[secIni[secAtual]].titulo : rotuloOpcao(op);
    TxtLinha t = ajTxtC(TXT_V3_LN_B, titulo, w);
    txt_desenhar_alpha(t, x, y, 1);
    y += 40.0f;
    ajBloco(TXT_V3_SUB, categoria ? SECAO_AJUDA[secAtual] : uxResumo(op), x, y, w, 30.0f, 0.70f, 2);
    gfx_sem_recorte();
    return;
  }
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
        aj2Seg(op, n, ed ? uxPendente : uxValorAtual(op), ed, bx, by, 1);
      }
      if (ph > 0) y += ph;
    } }
  y += 30.0f;
  // Kicker: a categoria e o bloco ("REPRODUCAO · IMAGEM E SOM", mockup v3).
  { char k[200];
    const char *bl = op >= 0 ? uxBloco(op) : naMais ? "Mais opções" : "";
    if (bl[0]) snprintf(k, sizeof k, "%s · %s", i18n(TELA[secIni[secAtual]].titulo),
                        i18n(op >= 0 && uxAvancada(op) ? "Mais opções" : bl));
    else snprintf(k, sizeof k, "%s", i18n(TELA[secIni[secAtual]].titulo));
    ajKickerAcento(k, x + 2.0f, y, 1); }
  y += 22.0f + 12.0f;
  // Cor de destaque: o titulo e o NOME da cor (a pendente, editando), como
  // no acentos-mockup.html; a opcao ja esta no rotulo da linha.
  { const char *t = naMais ? "Mais opções" : categoria ? TELA[secIni[secAtual]].titulo
                  : op == AJ_TEMA ? V_TEMA[ajEditandoLinha(op) ? uxPendente : valor[AJ_TEMA]]
                  : rotuloOpcao(op);
    y += ajBloco(TXT_V2_A3TIT, t, x, y, w, 56.0f, 1, 2); }
  y += 10.0f;
  { int maxL = (int)((fim - y) / 32.0f);
    if (maxL > 5) maxL = 5;   // a descricao inteira: cortar no meio da frase nao serve
    if (maxL < 1) maxL = 1;
    if (naMais) {
      char resumo[400];
      maisContar(secAtual, resumo, sizeof resumo);
      y += ajBloco(TXT_V3_INSP, resumo, x, y, w, 37.0f, 0.70f, maxL);
    } else if (rtl)
      y += txt_bloco_dir(TXT_V3_INSP, categoria ? SECAO_AJUDA[secAtual] : uxResumo(op),
                        243, 242, 239, x + w, y, w, 37.0f, 0.70f, maxL);
    else
      y += ajBloco(TXT_V3_INSP, categoria ? SECAO_AJUDA[secAtual] : uxResumo(op), x, y, w, 37.0f, 0.70f, maxL);
    if (op == AJ_TEMA) {   // a cor PENDENTE, como a previa
      int salvo = valor[AJ_TEMA];
      if (ajEditandoLinha(AJ_TEMA)) valor[AJ_TEMA] = uxPendente;
      y += 18.0f;
      y += aj2CorMedidas(x, y, w, fim, 1);
      valor[AJ_TEMA] = salvo;
    }
  if (!categoria) {
    const char *ef = efeitoOpcao(op);
    // "O que muda:" em negrito, na mesma frase (mockup v3): o texto regular
    // comeca depois do rotulo com espacos que nao quebram.
    if (ef && ef[0]) {
      const char *t = i18n(ef);
      TxtLinha lp = ajTxt(TXT_V3_INSP_B, "O que muda:");
      float hh = txt_bloco(TXT_V3_INSP, t, 0, 0, 0, x, -4000, w, 37.0f, 0, 4) + 37.0f;
      if (y + 4.0f + hh <= fim) {
        // A primeira linha continua depois do rotulo em negrito: as palavras
        // que cabem nela; o resto quebra normalmente embaixo.
        char l1[400];
        size_t u = 0, ult = 0;
        float livre = w - lp.w - 10.0f;
        y += 4.0f;
        // Arabic surfaces include tall font padding: match paragraph tops.
        txt_desenhar_alpha(lp, rtl ? x + w - lp.w : x,
                           y + (rtl ? 0 : (37.0f - lp.h) * 0.5f), 0.95f);
        while (t[u]) {
          size_t v = u;
          size_t tok;
          while (t[v] == ' ') v++;
          tok = txt_token_tam(t + v);
          if (!tok) break;
          v += tok;
          if (v >= sizeof l1) break;
          memcpy(l1, t, v); l1[v] = 0;
          if ((float)txt_largura(TXT_V3_INSP, l1) > livre) break;
          ult = v; u = v;
          if (!t[u]) break;
        }
        memcpy(l1, t, ult); l1[ult] = 0;
        if (ult) {
          TxtLinha r1 = ajTxt(TXT_V3_INSP, l1);
          txt_desenhar_alpha(r1, rtl ? x + w - lp.w - 10.0f - r1.w : x + lp.w + 10.0f,
                             y + (rtl ? 0 : (37.0f - r1.h) * 0.5f), 0.70f);
        }
        y += 37.0f;
        while (t[ult] == ' ') ult++;
        if (t[ult]) {
          if (rtl) y += txt_bloco_dir(TXT_V3_INSP, t + ult, 243, 242, 239,
                                     x + w, y, w, 37.0f, 0.70f, 3);
          else y += ajBloco(TXT_V3_INSP, t + ult, x, y, w, 37.0f, 0.70f, 3);
        }
      }
    }
    if (uxTemPadrao(op) && op != AJ_TEMA && y + 18.0f + 2 * 57.0f <= fim) {
      char padrao[160];
      const char *tec = rotuloTecnico(op);
      uxValorTexto(op, uxValorPadrao(op), padrao, sizeof padrao);
      y += 26.0f;
      y += aj2Meta(i18n("Vale para"), i18n(uxEscopo(op)), x, y, w, 1);
      y += aj2MetaRecomendado(padrao, x, y, w, 1);
      if (ajCusto(op) && y + 57.0f <= fim + 20.0f) y += aj2MetaCusto(op, x, y, w, 1);
      if (tec && y + 57.0f <= fim + 20.0f) y += aj2Meta(i18n("Nome técnico"), i18n(tec), x, y, w, 1);
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

// "MAIS OPCOES · N" (mockup v3, telas 02 e 07): a linha fechada no fim da
// categoria, com contorno fino em vez de superficie, o icone de controles, a
// contagem e, a direita, o que ha dentro ("DTS, Dolby Vision em MKV, ...").
// Aberta, a direita diz "Esconder" e as avancadas aparecem logo abaixo.
static void ajDesenharMais(int item, float y, float f, float a) {
  GfxRect fr = aj2ListaR();
  GfxRect ln = { fr.x + 18.0f, y, fr.w - 36.0f, AJ2_LN_H };
  int sec = secDoItem[item], aberto = maisAberto[sec];
  char resumo[300], rot[96];
  int n = maisContar(sec, resumo, sizeof resumo);
  float yc = y + AJ2_LN_H * 0.5f, xDir = ln.x + ln.w - 22.0f, aTx = a * aj2TextoLista, x;
  TxtLinha r, v;
  if (a <= 0.004f) return;
  ajFocoLinha(ln, 14.0f, f, aTx);
  if (f > 0.01f) { float ar, ag, ab; ajustes_acento(&ar, &ag, &ab); gfx_cor(ln, 14.0f / ln.h, ar, ag, ab, 0.14f * f * aTx); }
  if (f < 0.99f) gfx_anel(ln, 14.0f / ln.h, 1.5f, 1, 1, 1, 0.16f * (1.0f - f) * aTx);
  if (aTx <= 0.004f) return;
  ajIcone("aj_sliders-horizontal", ln.x + 20.0f, yc - 12.0f, 24.0f, (f > 0.5f ? 0.9f : 0.6f) * aTx);
  snprintf(rot, sizeof rot, "%s · %d", i18n("Mais opções"), n);
  r = ajTxt(f > 0.5f ? TXT_V2_LN_B : TXT_V2_LN, rot);
  txt_desenhar_alpha(r, ln.x + 58.0f, yc - r.h * 0.5f, (f > 0.5f ? 1.0f : 0.85f) * aTx);
  aj2Chevron(xDir, yc, (f > 0.5f ? 0.7f : 0.32f) * aTx);
  x = xDir - 22.0f - 12.0f;
  v = ajTxtC(TXT_V2_24, aberto ? i18n("Esconder") : resumo, x - (ln.x + 58.0f + r.w + 40.0f));
  txt_desenhar_alpha(v, x - v.w, yc - v.h * 0.5f, (f > 0.5f ? 0.75f : 0.45f) * aTx);
}

static float ajAlturaItem(int i) {
  if (!visivel(i)) return 0.0f;
  switch (TELA[i].tipo) {
    case IT_SEC: return 0.0f;
    case IT_ROT: return AJ2_GRUPO_H;
    case IT_GRP: return AJ2_LN_GAP + AJ2_LN_H;
    case IT_MAIS: return 14.0f + AJ2_LN_GAP + AJ2_LN_H + 4.0f;
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
// aviso fica e as dicas somem enquanto ele dura. `desenha`: 1 desenha e
// registra, 2 so registra (replay de TV/desktop: a funcao escolhe de novo o
// que aparece — mesmas dicas e a mesma supressao pelo aviso, nenhum alvo
// invisivel).
static void aj2RodapeA3(GfxRect fr, const AjDica *d, int nd, float a, int desenha) {
  float x = fr.x + 34.0f, w = fr.w - 68.0f, yc = fr.y + fr.h - AJ_A3_RODAPE * 0.5f - 6.0f;
  float wd = aj2DicasLinha(d, nd, x, yc, TXT_V2_KBD18, 34, 38, 10, TXT_V2_24, 8, 22, 0, a), wa = 0;
  int dicas = 1;
  if (desenha == 1) gfx_cor((GfxRect){ fr.x + 28.0f, fr.y + fr.h - AJ_A3_RODAPE, fr.w - 56.0f, 1 }, 0, 1, 1, 1, 0.07f * a);
#ifdef NV_TOUCH_UI
  if (ajTelaCheia() && ajPontInline) {
    if (uxAviso[0]) {
      TxtLinha t = ajTxtC(TXT_V2_24, uxAviso, w - 40.0f);
      GfxRect r = { x, fr.y + fr.h - AJ_A3_RODAPE - 8.0f - t.h - 24.0f, w, t.h + 24.0f };
      gfx_cor(r, 12.0f / r.h, AJ_CHIP_SOL, 0.96f * a);
      txt_desenhar_alpha(t, r.x + 20.0f, r.y + 12.0f, a);
    }
    ajToqueBotoes(x + w, yc, a);
    return;
  }
#endif
  if (uxAviso[0] && !(uxEditor && (uxRestaurar || uxAvisoRisco || uxEditor == 2))) {
    TxtLinha t = ajTxtC(TXT_V2_24, uxAviso, w - 34.0f);
    wa = t.w + 34.0f;
    if (wd + 24.0f + wa > w) dicas = 0;
    if (desenha == 1) { ajIcone("aj_check", x + w - wa, yc - 11.0f, 22.0f, 0.7f * a);
      txt_desenhar_alpha(t, x + w - t.w, yc - t.h * 0.5f, 0.75f * a); }
  }
  if (dicas) aj2DicasLinha(d, nd, x, yc, TXT_V2_KBD18, 34, 38, 10, TXT_V2_24, 8, 22, desenha, a);
}
// PONTEIRO (#99) NA LISTA DA CATEGORIA. A linha sob o cursor ganha o foco
// pelo mesmo focar() das setas (com o foco saindo do indice); o OK do clique
// segue o caminho de sempre (abrir o editor, alternar, abrir "Mais opcoes").
// So com a lista assentada: durante a transicao ela esta escalada e deslizada
// (gfx_transformar) e o retangulo do desenho nao e o da tela.
static int ajPontLista;
static void ajPonteiroItem(int i, int b) {
  (void)b;
  if (uxEditor || i < 0 || i >= AJ_N_TELA || !focavel(i)) return;
  if (i == focoItem && !focoIndice && !uxCabLayout) return;
  focar(i);
  focoIndice = 0;
}
static void ajDesenharFolha(void) {
  GfxRect fr = aj2ListaR();
  int sec = secAtual, i, nd;
  float topo, base, y, aPag = anim_suave(paginaA) * aj2TextoLista;
  const Item *s = &TELA[secIni[sec]];
  AjDica d[4];
  ajPaginaIlha(fr, 40.0f, 1);
  aj2CabecalhoA3(fr, s->titulo, 20.0f, aPag);
  topo = AJ_TOPO;
  base = AJ_BASE;
#ifdef NV_TOUCH_UI
  if (ajTelaCheia() && ajPontLista) {
    float total = 0.0f;
    for (int k = secIni[sec]; k < secFim(sec); k++) total += alturaItem(k);
    ajToqueRegistrar(AJT_LISTA, (GfxRect){fr.x, topo, fr.w, base - topo}, &scrollY, total - (base - topo), scrollY, sec);
  }
#endif
  gfx_recorte(fr.x, topo, fr.w, base - topo);
  y = topo - scrollY;
  for (i = secIni[sec] + 1; i < secFim(sec); i++) {
    float h = ajAlturaItem(i);
    if (h <= 0.0f) continue;
    if (y + h >= topo - 4 && y <= base + 4) {
      if (TELA[i].tipo == IT_ROT) {
        ajDesenharGrupo(i, y, ajMascara(y + 20.0f + 15.6f, topo, base) * aPag);
      } else if (TELA[i].tipo == IT_MAIS) {
        float yy = y + 14.0f + AJ2_LN_GAP;
        if (ajPontLista
#ifdef NV_TOUCH_UI
            && !ajPontInline
#endif
           ) ponteiro_alvo_faixa(fr.x + 18.0f, yy, fr.w - 36.0f, AJ2_LN_H, topo, base, ajPonteiroItem, NULL, i, 0);
        ajDesenharMais(i, yy, animItem[i], ajMascara(yy + AJ2_LN_H * 0.5f, topo, base) * anim_suave(paginaA));
      } else if (TELA[i].tipo == IT_OPC) {
        float lh = ajLinhaH(i), yy = y + AJ2_LN_GAP;
        if (ajPontLista
#ifdef NV_TOUCH_UI
            && !ajPontInline
#endif
           ) ponteiro_alvo_faixa(fr.x + 18.0f, yy, fr.w - 36.0f, lh, topo, base, ajPonteiroItem, NULL, i, 0);
        // A superficie de foco nao some com o texto (editor por cima): o
        // alfa da mascara vai inteiro, o do texto em ajDesenharLinha.
        ajDesenharLinha(i, yy, animItem[i], ajMascara(yy + (lh > AJ2_LN_H + 0.5f ? AJ2_LN_LINHA1 * 0.5f : lh * 0.5f), topo, base) * anim_suave(paginaA));
      }
    }
    y += h;
  }
  gfx_sem_recorte();
  nd = ajDicasDe(focoIndice ? -1 : focoOp, d);
  aj2RodapeA3(fr, d, nd, aPag, 1);
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
  if (ajustes_tamanho_ajustes() == 1.0f && !ajTelaCheia()) return;
  if (m->w > NV_VTELA_W - 80.0f) m->w = NV_VTELA_W - 80.0f;
  if (m->h > NV_VTELA_H - 80.0f) m->h = NV_VTELA_H - 80.0f;
  m->x = (NV_VTELA_W - m->w) * 0.5f;
  m->y = (NV_VTELA_H - m->h) * 0.5f;
}

// Confirmacao: kicker com o alerta ambar, pergunta 36/700, texto e "de › para".
// PONTEIRO (#99) NOS MODAIS DE AJUSTES. Os botoes da confirmacao ganham o foco
// pelo `focar` de quem abriu o modal (focar(1) = sim, focar(0) = nao, a mesma
// variavel das setas laterais); o OK do clique segue o caminho de sempre.
static void ajPontConfirmar(int sim, int b) { (void)b; if (uxEditor && (uxRestaurar || uxAvisoRisco)) uxConfirmar = sim; }
static void ajPontRisco(int sim, int b) { (void)b; if (riscoFolha) riscoFolhaFoco = sim ? 0 : 1; }
static void ajConfirmacao(const char *kick, int alerta, const char *pergunta, const char *texto,
                          const char *de, const char *para, const char *sim, const char *nao, int focoSim,
                          PonteiroFn focar) {
  GfxRect m = { 560, 300, 800, 0 };
  float x = m.x + 44, y, hTxt;
  hTxt = txt_bloco(TXT_AJ_TEXTO, texto, 0, 0, 0, 0, -4000, m.w - 88, 30, 0, 6);
  m.h = 44 + 22 + 8 + 44 + 14 + hTxt + (de ? 22 + 24 : 0) + 34 + 60 + 44;
  m.y = m.h < 480 ? 300 : (NV_VTELA_H - m.h) * 0.5f;
  if (ajustes_tamanho_ajustes() != 1.0f || ajTelaCheia()) { ajCentraModal(&m); x = m.x + 44; }
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
    float ws = ajBotao(sim, NULL, bx, y, focoSim, 1), wn;
    ponteiro_alvo(bx, y, ws, 60, focar, NULL, 1, 0);
    bx += ws + 12;
    wn = ajBotao(nao, NULL, bx, y, !focoSim, 1);
    ponteiro_alvo(bx, y, wn, 60, focar, NULL, 0, 0); }
}

// Alvo cujo foco ja e o unico possivel: o clique e so o OK.
static void ajPontNada(int a, int b) { (void)a; (void)b; }
static void ajPontFecharEditor(int a, int b) { (void)a; (void)b; if (uxEditor == 2) uxCancelar(); }
// --- Pendrive: o painel de informacoes do cache P2P (#205) -----------------------
// Uma so rotina mede (draw=0) e desenha (draw=1); devolve a altura.
#ifdef NV_WEBOS
static int ajUsbWebos = 1;   // so o webOS tem a jaula do modo desenvolvedor
#else
static int ajUsbWebos = 0;
#endif
static void ajBytes(uint64_t b, char *o, size_t n) {
  if (b >= (1ull << 30)) snprintf(o, n, "%.1f GB", (double)b / 1073741824.0);
  else snprintf(o, n, "%u MB", (unsigned)(b >> 20));
}
static const char *ajUsbFs(const char *fs) {
  if (!strcmp(fs, "vfat") || !strcmp(fs, "msdos")) return "FAT32";
  if (!strcmp(fs, "exfat")) return "exFAT";
  if (!strncmp(fs, "ntfs", 4) || !strcmp(fs, "fuseblk")) return "NTFS";
  if (!strncmp(fs, "ext", 3)) return fs;
  return fs[0] && strcmp(fs, "?") ? fs : i18n("sistema de arquivos desconhecido");
}
static const char *ajUsbRecusa(P2pmUsbMotivo m) {
  switch (m) {
    case P2PM_USB_READONLY: return i18n("O pendrive é somente leitura: o app não consegue gravar nele.");
    case P2PM_USB_NOT_MOUNT: case P2PM_USB_ALREADY_SEEN: case P2PM_USB_DATA_DIR:
      return i18n("O pendrive não está visível para o app.");
    case P2PM_USB_TIMEOUT: return i18n("O pendrive demorou demais para responder.");
    default: return i18n("Não foi possível ler o pendrive: sistema de arquivos sem suporte ou erro de leitura.");
  }
}
static float ajUsbPainel(float x, float y, float w, int draw) {
  P2pmUsbInfo u;
  char a[96], b[96], c[96], t[300];
  float y0 = y, ar, ag, ab;
  int semEspaco, aviso;
  p2pmotor_usb_info(&u);
  semEspaco = u.estado == 1 && u.livre < ((uint64_t)P2PM_LIVRE_MIN_MB << 20);
  aviso = u.estado == 2 || semEspaco;
  ajustes_acento(&ar, &ag, &ab);
  // Estado: uma linha com o icone.
  snprintf(t, sizeof t, "%s",
           u.estado == 0 ? i18n("Nenhum pendrive encontrado") :
           u.estado == 2 ? i18n("Pendrive recusado") :
           semEspaco ? i18n("Pendrive sem espaço livre") :
           u.emUso ? i18n("Pendrive em uso") : i18n("Pendrive encontrado, cache no interno"));
  if (draw) {
    TxtLinha l = ajTxtC(TXT_AJ_SEG, t, w - 44);
    if (aviso) gfx_icone((GfxRect){ x, y + 2, 28, 28 }, "aj_triangle-alert", 0.961f, 0.784f, 0.298f, 1);
    else ajIcone(u.estado ? "aj_check" : "aj_database", x, y + 2, 28, u.estado ? 1.0f : 0.6f);
    txt_desenhar(l, x + 44, y + (32 - l.h) * 0.5f);
  }
  y += 32;
  if (u.estado == 2 || semEspaco) {
    const char *r = semEspaco ? i18n("Libere espaço no pendrive para usar o cache nele.") : ajUsbRecusa(u.motivo);
    if (draw) ajBloco(TXT_V3_INSP, r, x + 44, y + 4, w - 44, 28, 0.7f, 3);
    y += 4 + txt_bloco(TXT_V3_INSP, r, 0, 0, 0, 0, -4000, w - 44, 28, 0, 3);
  }
  y += 16;
  if (u.estado && u.total) {   // grafico: cache | outros | livre, e o marcador do limite
    uint64_t usado = u.total > u.livre ? u.total - u.livre : 0;
    float fu = (float)usado / (float)u.total, fc = (float)u.cache / (float)u.total, fl = (float)u.limite / (float)u.total;
    if (fu > 1) fu = 1;
    if (fc > fu) fc = fu;
    if (fl > 1) fl = 1;
    if (draw) {
      gfx_cor((GfxRect){ x, y, w, 22 }, 0.5f, 1, 1, 1, 0.10f);
      if (fu > 0) gfx_cor((GfxRect){ x, y, fmaxf(w * fu, 8), 22 }, 0.5f, 1, 1, 1, 0.38f);
      if (u.cache) gfx_cor((GfxRect){ x, y, fmaxf(w * fc, 8), 22 }, 0.5f, ar, ag, ab, 1);
      if (u.estado == 1 && u.limite) gfx_cor((GfxRect){ x + w * fl - 2, y - 7, 4, 36 }, 0.1f, 1, 1, 1, 0.9f);
    }
    y += 22 + 16;
    { // legenda: quebra de linha se nao couber
      float lx = x, ly = y;
      for (int i = 0; i < (u.estado == 1 ? 4 : 3); i++) {
        static const char *const ROT[] = { "Cache", "Outros", "Livre", "Limite" };
        uint64_t v = i == 0 ? u.cache : i == 1 ? usado - (usado > u.cache ? u.cache : usado) : i == 2 ? u.livre : u.limite;
        TxtLinha l;
        float iw;
        ajBytes(v, a, sizeof a);
        snprintf(t, sizeof t, "%s %s", i18n(ROT[i]), a);
        l = ajTxt(TXT_V3_INSP, t);
        iw = 22 + l.w + 26;
        if (lx + iw - 26 > x + w) { lx = x; ly += 32; }
        if (draw) {
          if (i == 3) gfx_cor((GfxRect){ lx + 4, ly + 2, 4, 22 }, 0.1f, 1, 1, 1, 0.9f);
          else gfx_cor((GfxRect){ lx, ly + 5, 16, 16 }, 0.3f, i == 0 ? ar : 1, i == 0 ? ag : 1, i == 0 ? ab : 1,
                       i == 0 ? 1.0f : i == 1 ? 0.38f : 0.10f);
          txt_desenhar_alpha(l, lx + 24, ly + (26 - l.h) * 0.5f, 0.75f);
        }
        lx += iw;
      }
      y = ly + 26 + 10;
    }
  }
  // Linhas de detalhe.
  { int n = 0;
    const char *k[6], *v[6];
    char vs[6][160];
    if (u.estado) {
      snprintf(vs[n], 160, "%s · %s", u.nome[0] ? u.nome : u.caminho, ajUsbFs(u.fs));
      k[n] = i18n("Pendrive"); v[n] = vs[n]; n++;
      if (u.total) {
        ajBytes(u.livre, a, sizeof a); ajBytes(u.total, b, sizeof b);
        snprintf(vs[n], 160, i18n("%s livres de %s"), a, b);
        k[n] = i18n("Capacidade"); v[n] = vs[n]; n++;
      }
    }
    if (u.estado == 1) {
      ajBytes(u.cache, a, sizeof a); ajBytes(u.limite, b, sizeof b);
      snprintf(vs[n], 160, i18n("%s · limite %s"), a, b);
      k[n] = i18n("Cache"); v[n] = vs[n]; n++;
    }
    if (u.estado) {
      snprintf(vs[n], 160, "%s", u.estado == 1 && u.gravavel ? i18n("OK, grava normalmente") :
               u.motivo == P2PM_USB_READONLY || (u.estado == 1 && !u.gravavel) ? i18n("Somente leitura") : i18n("Falhou"));
      k[n] = i18n("Teste de escrita"); v[n] = vs[n]; n++;
    }
    snprintf(vs[n], 160, "%s", i18n("Armazenamento interno"));
    k[n] = u.estado == 1 ? i18n("Se falhar") : i18n("Cache em"); v[n] = vs[n]; n++;
    (void)c;
    for (int i = 0; i < n; i++) {
      if (draw) aj2Meta(k[i], v[i], x, y, w, 1);
      y += 14 + 29 + 14;
    }
  }
  if (ajUsbWebos) {   // fato curto: a jaula do modo desenvolvedor so enxerga o que estava na TV ao ligar
    const char *nota = i18n("No webOS, conecte o pendrive ANTES de ligar a TV. Um pendrive conectado com a TV ligada só aparece para o app depois de reiniciar a TV.");
    y += 12;
    if (draw) y += aj2Ef("aj_info", nota, x, y, w, 1);
    else y += txt_bloco(TXT_V2_26, nota, 0, 0, 0, x, -4000, w - 22 - 26 - 14 - 22, 34.8f, 0, 6) + 40;
  }
  return y - y0;
}
#ifdef NV_TOUCH_UI
static void ajToqueFecharDependencia(int a, int b) {
  if (layout_modo_mobile()) ajPontFecharEditor(a, b);
}
#endif
// Dependencia: a opcao esta apagada por causa de outra.
static void ajDependencia(void) {
  int req = uxRequisito(uxOp);
  GfxRect m = { 560, 0, 800, 0 };
  float x = m.x + 44, y, hTxt;
  char k[200];
  const char *b = uxBloco(uxOp);
  int usb = uxOp == AJ_P2P_DESTINO;   // o modal do pendrive mostra o painel no lugar do texto de ajuda
  hTxt = usb ? ajUsbPainel(0, 0, m.w - 88, 0) : txt_bloco(TXT_AJ_TEXTO, ajudaOpcao(uxOp), 0, 0, 0, 0, -4000, m.w - 88, 30, 0, 6);
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
  if (usb) y += ajUsbPainel(x, y, m.w - 88, 1) + 34;
  else y += ajBloco(TXT_AJ_TEXTO, ajudaOpcao(uxOp), x, y, m.w - 88, 30, 0.62f, 6) + 34;
  { float bx = x, w;
    // Ponteiro: o botao em foco e o OK; "Fechar" fora do foco fecha como o Voltar.
    if (req >= 0) {
      w = ajBotao("Ver o ajuste necessário", NULL, bx, y, 1, 1);
      ponteiro_alvo(bx, y, w, 60, ajPontNada, NULL, 0, 0);
      bx += w + 12;
    }
    w = ajBotao("Fechar", NULL, bx, y, req < 0, 1);
    if (req < 0) {
#ifdef NV_TOUCH_UI
      if (ajTelaCheia()) ponteiro_alvo(bx, y, w, 60, NULL, ajToqueFecharDependencia, 0, 0);
      else
#endif
        ponteiro_alvo(bx, y, w, 60, ajPontNada, NULL, 0, 0);
    }
    else ponteiro_alvo(bx, y, w, 60, NULL, ajPontFecharEditor, 0, 0); }
}

static void ajDesenharModais(void) {
  if (!uxEditor || ajEditandoLinha(uxOp)) return;
  // v2: os editores sao o terceiro degrau da pilha (aj2DesenharEditor); aqui
  // ficam so as perguntas — confirmar, restaurar e a dependencia.
  if (!uxRestaurar && !uxAvisoRisco && uxEditor != 2) return;
  ponteiro_camada();   // a pergunta tem o teclado: o editor por baixo nao vale
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
                    de, para, "Continuar", "Cancelar", uxConfirmar, ajPontConfirmar);
    } else {
      snprintf(texto, sizeof texto, i18n("O valor voltará para “%s”. %s"), padrao,
        !strcmp(uxEscopoValor(uxOp, uxValorPadrao(uxOp)), "Conta/perfil") ? i18n("Esta preferência participa dos ajustes do perfil. Nenhum serviço será desconectado.")
          : !strcmp(uxEscopoValor(uxOp, uxValorPadrao(uxOp)), "Só este perfil, nesta TV") ? i18n("A mudança vale só para este perfil, nesta TV. Nenhum serviço será desconectado.")
          : i18n("A mudança vale para todos os perfis desta TV. Nenhum serviço será desconectado."));
      uxValorTexto(uxOp, uxValorAtual(uxOp), de, sizeof de);
      ajConfirmacao(rotuloOpcao(uxOp), 0, "Restaurar este ajuste?", texto, de, padrao, "Restaurar", "Cancelar", uxConfirmar, ajPontConfirmar);
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
  ponteiro_camada();
  ajVeuModal();
  if (fil) { snprintf(de, sizeof de, i18n("%d fileiras"), v); snprintf(para, sizeof para, i18n("%d fileiras"), nv); }
  else { uxValorTexto(riscoFolhaOp, v, de, sizeof de); uxValorTexto(riscoFolhaOp, nv, para, sizeof para); }
  ajConfirmacao(fil ? "Tela inicial · Fileiras" : uxCaminho(riscoFolhaOp), 1, riscoFolhaTitulo(), i18n(riscoFolhaTexto()),
                de, para, "Continuar", "Cancelar", riscoFolhaFoco == 0, ajPontRisco);
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
  if (ajRetrato()) m.h = 1040.0f;
  float x, y;
  char t[96];
  ajCentraModal(&m);
  ajIlha(m, 36, 1, 1);
  x = m.x + 56 + 356 + 56; y = m.y + 50;
  if (ajRetrato()) { x = m.x + 56; y += 396.0f; }
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
    if (ajRetrato()) { tile.x = m.x + (m.w - 356.0f) * 0.5f; tile.y = m.y + 40.0f; }
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
      if (!strcmp(servico, "Simkl")) {
        int rest = simklauth_restante_s();
        if (rest >= 0) {
          char c[64];
          TxtLinha lc;
          snprintf(c, sizeof c, i18n("Expira em %d:%02d"), rest / 60, rest % 60);
          lc = ajTxt(TXT_AJ_ESTADO, c);
          txt_desenhar_alpha(lc, x + 20 + l.w + 28, by + (30 - lc.h) * 0.5f, rest < 60 ? 0.95f : 0.45f);
        }
      }
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
  if(uxOp==AJ_IPTV_EXCLUIR)snprintf(linha,sizeof linha,"%s",iptvAlvo.nome);
  if(uxOp==AJ_GUIA_CATEGORIAS)snprintf(linha,sizeof linha,"%s: %d · %s",i18n("Categorias"),uxNCategorias,i18n(uxEscopoValor(uxOp,uxPendente)));
  { TxtLinha t = ajTxtC(TXT_V2_26, linha, m.w - 104);
    txt_desenhar_alpha(t, x, y + (29.0f - t.h) * 0.5f, 0.55f); }
  return y + 29.0f + 30.0f;
}
// PONTEIRO (#99) NO EDITOR (o painel do 3o degrau). So com ele assentado
// (ajDesenharTela: sem escala nem deslize). Escolha: o valor sob o cursor vira
// o pendente, como as setas; Numero: o mostrador foca, o "-" e o "+" andam um
// passo (as setas laterais); o chip "Restaurar padrao" foca o rodape. O OK do
// clique e o de sempre (aplicar / perguntar se restaura).
static int ajPontEditor;
static int ajPontEdVale(void) { return uxEditor == 1 && uxOp >= 0 && !uxRestaurar && !uxAvisoRisco; }
static void ajPontEdRodape(int a, int b) { (void)a; (void)b; if (ajPontEdVale() && uxTemPadrao(uxOp)) uxRodape = 1; }
static void ajPontEdValor(int i, int b) {
  (void)b;
  if (!ajPontEdVale() || i < 0 || i >= nValores(uxOp) || (!uxRodape && uxPendente == i)) return;
  if (uxOp == AJ_TEX_MB && texAcima(i)) return;
  if (uxOp == AJ_SALVOS_DEST && !ajustes_fonte_oferecida(i, trakt_ativo(), simkl_ativo())) return;
  uxRodape = 0; uxPendente = i;
}
static void ajIptvToqueEscolher(int i,int b) {
  if(!ajPontEdVale() || (uxOp!=AJ_GUIA_CATEGORIAS && uxOp!=AJ_IPTV_CONTA && !ajIptvMenu(uxOp)) || i<0 || i>=nValores(uxOp))return;
  ajPontEdValor(i,b);
  if(uxOp==AJ_GUIA_CATEGORIAS)uxCategoriaAlternar(i);else uxAplicar();

}
static void ajPontEdNumero(int a, int b) { (void)a; (void)b; if (ajPontEdVale()) uxRodape = 0; }
static void ajPontEdPasso(int d, int b) {
  (void)b;
  if (!ajPontEdVale() || OPCOES[uxOp].tipo != OP_NUMERO) return;
  uxRodape = 0; uxPendente = limita(uxOp, uxPendente + d * OPCOES[uxOp].passo);
}
static void aj2RodapeEditor(GfxRect m, float colW, const AjDica *d, int n) {
  float y = m.y + m.h - 40 - 64;
  if(uxOp==AJ_GUIA_CATEGORIAS && ajPontEditor)
    ajToqueBotoes(m.x+m.w-52.0f,y+32.0f,1.0f);
#ifdef NV_TOUCH_UI
  // A phone saved in TV still needs explicit controls to select Mobile or Auto.
  else if (ajPontEditor && (ajTelaCheia() || uxOp == AJ_INTERFACE_MODO))
    ajToqueBotoes(m.x + m.w - 52.0f, y + 32.0f, 1.0f);
#endif
  if(uxOp==AJ_GUIA_CATEGORIAS)return;
  if (ajRetrato()) y -= 76.0f;
  if (uxTemPadrao(uxOp)) {
    TxtLinha t = uxRodape ? ajTxtT(TXT_V2_SEG, "Restaurar padrão") : ajTxt(TXT_V2_SEG, "Restaurar padrão");
    float w = 24 + 24 + 10 + t.w + 24;
    if (ajPontEditor) ponteiro_alvo(m.x + 52, y, w, 64, ajPontEdRodape, NULL, 0, 0);
    if (uxRodape) ajPilulaAcento((GfxRect){ m.x + 52, y, w, 64 }, 32, 1);
    else ajNeutro((GfxRect){ m.x + 52, y, w, 64 }, 32, 0.08f, AJ_CHIP_SOL, 1);
    if (uxRodape) ajIconeT("aj_rotate-ccw-clock", m.x + 52 + 24, y + 20, 24, 1);
    else ajIcone("aj_rotate-ccw-clock", m.x + 52 + 24, y + 20, 24, 0.85f);
    txt_desenhar_alpha(t, m.x + 52 + 58, y + (64 - t.h) * 0.5f, uxRodape ? 1.0f : 0.85f);
  }
#ifdef NV_TOUCH_UI
  if (ajPontEditor && (ajTelaCheia() || uxOp == AJ_INTERFACE_MODO)) return;
#endif
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
static float aj2EditorBase(GfxRect m) {
  return m.y + m.h - 40.0f - 64.0f - 24.0f - (ajRetrato() ? 76.0f : 0.0f);
}
static void aj2EditorNumero(GfxRect m) {
  const Opcao *o = &OPCOES[uxOp];
  float y, lx, lw, ar, ag, ab, pw = 520.0f;
  char v[64], salvo[64], uni[64];
  // Android: sem faixa vigiada no trilho (a folha de memoria virou dica).
  int risco = uxOp == AJ_FIL_LIMITE && fil_limite_pede_aviso(FIL_LIMITE_MIN, FIL_LIMITE_MAX) ? FIL_LIMITE_SEGURO : -1;
  int ilimitado = uxOp == AJ_FIL_LIMITE && fil_limite_ilimitado(uxPendente);
  ajustes_acento(&ar, &ag, &ab);
  y = aj2CabecalhoEditor(m);
  if (m.w - 104 - pw - 56 < 560) pw = 0;
  lx = m.x + 52; lw = pw > 0 ? m.w - 104 - pw - 56 : m.w - 104;
  snprintf(v, sizeof v, "%d", uxPendente);
  uxValorTexto(uxOp, uxOriginal, salvo, sizeof salvo);
  if (ilimitado) uni[0] = 0;
  else if (uxOp == AJ_FIL_LIMITE) snprintf(uni, sizeof uni, "%s", i18n("fileiras"));
  else snprintf(uni, sizeof uni, "%s", o->sufixo ? o->sufixo + (o->sufixo[0] == ' ') : "");
  { GfxRect r = { lx, y, lw, 34 + 150 + 40 + 10 + 14 + 40 + 34 };
    float fr = ajMovFracao(uxOp, uxPendente), fp = ajPadraoVisual(uxOp) >= 0 ? ajFracao(uxOp, ajPadraoVisual(uxOp)) : -1;
    float tx = r.x + 40, tw = r.w - 80, ty = r.y + 34 + 150 + 40, cy = r.y + 34 + 75;
    TxtLinha big, u = ajTxt(TXT_V2_36B, uni), ls;
    char sv[96];
    if (uxOp == AJ_HERO_TRAILER_ESPERA) snprintf(v, sizeof v, "%d,%d", uxPendente / 10, uxPendente % 10);
    // "Ilimitado" nao cabe no corpo de 150 dos numeros: vai no titulo de 36.
    big = ilimitado ? ajTxt(TXT_V2_36B, i18n("Ilimitado")) : ajTxt(ajRetrato() ? TXT_V2_TIT : TXT_V2_NUM150, v);
    if (ajVid()) gfx_cor(r, 30.0f / r.h, 1, 1, 1, uxRodape ? 0.0f : 0.12f);
    else ajFocoLinha(r, 30, uxRodape ? 0 : 1, 1);
    txt_desenhar(big, tx - 4, cy - big.h * 0.5f);
    snprintf(sv, sizeof sv, i18n("salvo: %s"), salvo);
    ls = ajTxt(TXT_V2_26, sv);
    { float bx = tx - 4 + big.w + 30, bh = 43.6f + 6 + 29;
      if (ajRetrato()) {
        float livre = fmaxf(1.0f, r.x + r.w - 168.0f - bx);
        u = ajTxtC(TXT_V2_36B, uni, livre); ls = ajTxtC(TXT_V2_26, sv, livre);
      }
      txt_desenhar(u, bx, cy - bh * 0.5f + (43.6f - u.h) * 0.5f);
      txt_desenhar_alpha(ls, bx, cy - bh * 0.5f + 43.6f + 6 + (29 - ls.h) * 0.5f, 0.5f); }
    if (ajPontEditor) {
      ponteiro_alvo(r.x, r.y, r.w, r.h, ajPontEdNumero, NULL, 0, 0);
      ponteiro_alvo(r.x + r.w - 40 - 48, cy - 28, 56, 56, NULL, ajPontEdPasso, 1, 0);
      ponteiro_alvo(r.x + r.w - 40 - 104, cy - 28, 56, 56, NULL, ajPontEdPasso, -1, 0);
    }
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
      if (uxOp == AJ_FIL_LIMITE && fil_limite_ilimitado(o->max)) snprintf(t, sizeof t, "%s", i18n("Ilimitado"));
      else snprintf(t, sizeof t, "%d", o->max);
      l = ajTxt(TXT_V2_24, t); txt_desenhar_alpha(l, tx + tw - l.w, lc - l.h * 0.5f, 0.5f);
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
#ifdef NV_ANDROID
    // Dica, nao alerta: no Android o limite e "ilimitado" (dono, 2.0.4).
    snprintf(t, sizeof t, "%s", i18n("Ilimitado mostra todos os catálogos ligados. Com muitas fileiras a Home leva mais tempo para ficar completa."));
    y += 22; y += aj2Ef("aj_info", t, lx, y, lw, 1);
#else
    snprintf(t, sizeof t, i18n("Mais fileiras usam mais memória e rede: em TV com 1 GB a Home pode ficar lenta ou fechar. Acima de %d a TV pede confirmação."), FIL_LIMITE_SEGURO);
    y += 22; y += aj2Ef("aj_triangle-alert", t, lx, y, lw, 1);
#endif
  }
  { const char *ef = uxOp == AJ_FIL_LIMITE ? NULL : efeitoOpcao(uxOp);
    if (ef && ef[0] && y + 12 + 80 <= aj2EditorBase(m) + 4.0f) {
      y += 12; y += aj2Ef("aj_info", i18n(ef), lx, y, lw, 1); } }
  if (pw > 0) aj2PreviaPendente(m.x + m.w - 52 - pw, aj2CabecalhoEditor(m), pw);
  { AjDica d[3] = { { "← →", "Ajustar" }, { "OK", uxOp==AJ_GUIA_CATEGORIAS?"Selecionar":"Confirmar" }, { "Voltar", "Cancelar" } };
    aj2RodapeEditor(m, lw, d, 3); }
}
static void aj2EditorEscolha(GfxRect m) {
  const Opcao *o = &OPCOES[uxOp];
  int n = nValores(uxOp), i, primeiro, vis;
  float y, lx, lw, pw = 520.0f, y0;
  float passo=100.0f;
  if(uxOp==AJ_GUIA_CATEGORIAS) {
    passo=fmaxf(passo,(float)ajTxt(TXT_V2_ROT,"Mg").h+ajTxt(TXT_V2_24,"Mg").h+36.0f);
    pw=0;
  }
  y = aj2CabecalhoEditor(m);
  y0 = y;
  if (m.w - 104 - pw - 56 < 560) pw = 0;
  lx = m.x + 52; lw = pw > 0 ? m.w - 104 - pw - 56 : m.w - 104;
  vis = (int)((aj2EditorBase(m) - y) / passo);
  if (vis < (uxOp==AJ_GUIA_CATEGORIAS?1:3)) vis = uxOp==AJ_GUIA_CATEGORIAS?1:3;
  primeiro = uxPendente - vis / 2;
  if (primeiro + vis > n) primeiro = n - vis;
  if (primeiro < 0) primeiro = 0;
#ifdef NV_TOUCH_UI
  if (ajTelaCheia()) { float base = aj2EditorBase(m);
    if (ajPontEditor) {
      float off = ajToqueRegistrar(AJT_EDITOR, (GfxRect){lx, y0, lw, base - y0}, &ajToqueOffset[AJT_EDITOR],
                                  n * passo - (base - y0), primeiro * passo, uxOp);
      primeiro = 0; vis = n; y = y0 - off;
    }
    gfx_recorte(lx, y0, lw, base - y0);
  }
#endif
  for (i = primeiro; i < n && i < primeiro + vis; i++) {
#ifdef NV_TOUCH_UI
    if (ajTelaCheia() && (y + passo - 4 < y0 || y >= aj2EditorBase(m))) { y += passo; continue; }
#endif
    // Servico desligado nao e oferta, e so linha de ponteiro: como o valor nao
    // aparece na lista do editor, clicar nele nao pode dar passo algum.
    if (uxOp == AJ_SALVOS_DEST && !ajustes_fonte_oferecida(i, trakt_ativo(), simkl_ativo())) { y += 100; continue; }
    char t[160];
    int foco = !uxRodape && i == uxPendente;
    GfxRect r = { lx, y, lw, passo-4.0f };
    int fv;
    uxValorTexto(uxOp, i, t, sizeof t);
    // Acima do que a RAM desta TV suporta: o rotulo fica puro e apagado, com o motivo a direita.
    if (uxOp == AJ_TEX_MB && texAcima(i)) snprintf(t, sizeof t, "%s", i18n(o->valores[i]));
    if (ajPontEditor) {
      PonteiroFn ac = uxOp == AJ_TEX_MB && texAcima(i) ? NULL : ajPontEdValor;
      PonteiroFn ativar = uxOp==AJ_GUIA_CATEGORIAS || uxOp==AJ_IPTV_CONTA || ajIptvMenu(uxOp) ? ajIptvToqueEscolher : NULL;
#ifdef NV_TOUCH_UI
      if (ajTelaCheia())
        ponteiro_alvo_faixa(r.x, r.y, r.w, r.h, y0, aj2EditorBase(m), ac, ativar, i, 0);
      else
#endif
        ponteiro_alvo(r.x, r.y, r.w, r.h, ac, ativar, i, 0);
    }
    if (foco) ajFocoLinha(r, 28, 1, 1);
    if(uxOp==AJ_GUIA_CATEGORIAS && i<uxNCategorias) {
      // A dedicated trailing checkbox cannot overlap the two text lines.
      float textoX=r.x+30,limite=fmaxf(1,r.w-104);
      TxtLinha nome=ajTxtC(TXT_V2_ROT,t,limite);
      TxtLinha conta=ajTxtC(TXT_V2_24,uxCategorias[i].conta,limite);
      float yy=r.y+(r.h-nome.h-conta.h-4)*0.5f;
      txt_desenhar_alpha(nome,textoX,yy,foco?1.0f:0.88f);
      txt_desenhar_alpha(conta,textoX,yy+nome.h+4,0.55f);
      GfxRect box={r.x+r.w-56,r.y+(r.h-26)*0.5f,26,26};
      gfx_anel_fora(box,0.18f,0,2,0.65f,0.67f,0.72f,0.75f);
      if(uxCategorias[i].oculta) {
        float ar,ag,ab;ajustes_acento(&ar,&ag,&ab);
        gfx_cor(box,0.18f,ar,ag,ab,1);
        ajIconeT("aj_check",box.x,box.y,26,1);
      }
      y+=passo;continue;
    }
    fv = marcaDoValor(uxOp, o->valores && i < o->n ? o->valores[i] : NULL);
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
      int indisp = uxOp == AJ_TEX_MB && texAcima(i);
      TxtLinha l = ajTxtC(TXT_V2_ROT, t, lw - 210 - (textoX - r.x));
      txt_desenhar_alpha(l, textoX, r.y + (96 - l.h) * 0.5f, indisp ? 0.4f : foco ? 1.0f : 0.88f);
      if (indisp) {
        char mot[96];
        TxtLinha m;
        float mx = r.x + r.w - 30;
        snprintf(mot, sizeof mot, i18n("acima do máximo desta TV (%d MB)"), tex_teto_ram_mb());
        m = ajTxtC(TXT_V2_28, mot, r.w - 60 - (textoX - r.x) - l.w - 24 - (i == uxOriginal ? 200 : 0));
        if (i == uxOriginal) mx -= 170;
        txt_desenhar_alpha(m, mx - m.w, r.y + (96 - m.h) * 0.5f, 0.5f);
      }
    }
    if (uxOp!=AJ_GUIA_CATEGORIAS && i == uxOriginal && uxOp!=AJ_IPTV_ACOES && uxOp!=AJ_IPTV_EXCLUIR && (!ajIptvGerenciador(uxOp) || ajIptvDoTipo(ajIptvTipo(uxOp),i)==iptv_conta_indice())) {
      TxtLinha s = ajTxt(TXT_V2_28, "Salvo");
      float sx = r.x + r.w - 30 - s.w;
      txt_desenhar_alpha(s, sx, r.y + (96 - s.h) * 0.5f, 0.55f);
      ajIcone("aj_check", sx - 12 - 26, r.y + 35, 26, 0.55f);
    }
    y += passo;
  }
#ifdef NV_TOUCH_UI
  if (ajTelaCheia()) gfx_sem_recorte();
#endif
  if (pw > 0 && uxOp == AJ_P2P_DESTINO) ajUsbPainel(m.x + m.w - 52 - pw, y0, pw, 1);
  else if (pw > 0) aj2PreviaPendente(m.x + m.w - 52 - pw, y0, pw);
  { AjDica d[3] = { { "↑ ↓", "Escolher" }, { "OK", uxOp==AJ_GUIA_CATEGORIAS?"Selecionar":"Confirmar" }, { "Voltar", "Cancelar" } };
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
// PONTEIRO (#99) NA LISTA "DIFERENTES DO PADRAO": a linha sob o cursor ganha o
// foco (uxDifFoco, a variavel das setas).
static void ajPontDif(int i, int b) {
  (void)b;
  if (uxEditor || uxIndice != 1 || i < 0 || i >= uxNDifs || (!focoIndice && i == uxDifFoco)) return;
  focoIndice = 0; uxDifFoco = i;
}
static void aj2DesenharDiferencas(void) {
  GfxRect fr = aj2ListaR();
  float topo = AJ_TOPO, base = AJ_BASE + AJ_A3_RODAPE, y = topo + 8.0f;
  int primeiro = uxDifFoco > 9 ? uxDifFoco - 9 : 0, i;
#ifdef NV_TOUCH_UI
  if (ajTelaCheia() && ajPontLista) {
    float off = ajToqueRegistrar(AJT_DIFS, (GfxRect){fr.x, topo, fr.w, base - topo}, &ajToqueOffset[AJT_DIFS],
                                uxNDifs * 78.0f - (base - topo), primeiro * 78.0f, 0);
    primeiro = 0; y = topo + 8.0f - off;
  }
#endif
  float x = fr.x + 18, w = fr.w - 36;
  ajPaginaIlha(fr, 40.0f, 1);
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
#ifdef NV_TOUCH_UI
    if (ajTelaCheia() && y + 76 < topo) { y += 78; continue; }
#endif
    float xd = x + w - 22, a = ajMascara(y + 38, topo, base), yc = y + 38;
    TxtLinha la, lp, lr, ls;
    if (y > base) break;
    if (ajPontLista) ponteiro_alvo_faixa(r.x, r.y, r.w, r.h, topo, base, ajPontDif, NULL, i, 0);
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
  if (ajRetrato()) {
    if (!uxNDifs) return;
    int op = uxDifs[uxDifFoco], salvo = valor[op];
    float w = fminf(560.0f, ir.w), x = ir.x + (ir.w - w) * 0.5f, cw = (w - 16.0f) * 0.5f;
    gfx_recorte(ir.x, ir.y, ir.w, ir.h);
    for (int i = 0; i < 2; i++) {
      valor[op] = i == 0 ? uxValorPadrao(op) : salvo;
      ajPreviaCompacta(op, x + i * (cw + 16.0f), ir.y + 4.0f, cw);
      TxtLinha t = ajTxtC(TXT_V3_SUB, i == 0 ? "Padrão" : "Atual", cw);
      txt_desenhar_alpha(t, x + i * (cw + 16.0f), ir.y + 4.0f + cw * 9.0f / 16.0f + 10.0f, 0.65f);
    }
    valor[op] = salvo;
    float y = ir.y + 4.0f + cw * 9.0f / 16.0f + 52.0f;
    TxtLinha t = ajTxtC(TXT_V3_LN_B, rotuloOpcao(op), ir.w);
    txt_desenhar_alpha(t, ir.x, y, 1);
    ajBloco(TXT_V3_SUB, uxResumo(op), ir.x, y + 40.0f, ir.w, 30.0f, 0.70f, 2);
    gfx_sem_recorte();
    return;
  }
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
  return (uxEditor && (uxRestaurar || uxAvisoRisco || uxEditor == 2)) || riscoFolha || frAberta || hcAberta || ajVinculoAtivo() ||
         pfTela || teclado_aberto() || registro_envio_aberto() || apoioAberto;
}
#ifdef NV_TOUCH_UI
// Inline pointer editing (Confirm/Cancel footer, Mobile hit targets) is Mobile only.
static int ajInlineAtivo(int modal, const Aj2Pose *p) {
  return layout_modo_mobile() && !modal && uxOp >= 0 && ajEditandoLinha(uxOp) &&
         p->lA > 0.997f && fabsf(p->lS - 1.0f) < 0.002f && fabsf(p->lDx) < 0.5f;
}
#endif
static void ajDesenharTela(void) {
  int modal = ajModalAberto();
  Aj2Pose p = aj2PoseAgora();
  float g0;
#ifdef NV_TOUCH_UI
  ajPontInline = 0;
#endif
  ajFundoPagina();
  // Pagina assentada sem vidro: o cabecalho e a lista sao opacos e vem logo,
  // entao o Frost pendente so e pintado fora deles (ajIlhaMaterial faz o mesmo
  // com uma ilha so; aqui sao duas antes da primeira).
  if (!ajVid() && !modal && !filAberta && !uxEditor && p.rA <= 0.003f && uxIndice >= 1 &&
      p.lA >= 0.999f && fabsf(p.lS - 1.0f) < 0.002f && fabsf(p.lDx) < 0.5f) {
    GfxRect f[2] = { ajIlhaMiolo0(aj2PaginaR(), 36.0f), ajIlhaMiolo0(aj2ListaR(), 40.0f) };
    gfx_ambiente_descarregar_fora(f, 2);
  }
  if (filAberta) return;   // a folha de fileiras ocupa a tela (desenhada depois)
#ifdef NV_TOUCH_UI
  ajPontInline = ajInlineAtivo(modal, &p);
  if (ajPontInline) { ponteiro_camada(); ajToqueCamada(); }
#endif
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
  ajPontLista = !modal && !uxEditor && p.lA > 0.997f && fabsf(p.lS - 1.0f) < 0.002f && fabsf(p.lDx) < 0.5f;
#ifdef NV_TOUCH_UI
  if (ajPontInline) ajPontLista = 1;
#endif
  // #205: a edicao na linha (Mobile e TV) tem o rodape com os chips clicaveis.
  // A camada dos alvos comeca aqui: as linhas registraram alvos no quadro
  // assentado e um clique fora nao pode cair no OK de camada vazia.
#ifdef NV_TOUCH_UI
  if (ajPontInline) { ponteiro_camada(); ajToqueCamada(); }
  else if (uxEditor && !modal)
#else
  if (uxEditor && !modal)
#endif
    { ponteiro_camada();
#ifdef NV_TOUCH_UI
      ajToqueCamada();
#endif
    }
  if (p.lA > 0.003f && uxIndice >= 1) {
    GfxRect fr = aj2ListaR();
    float oy = p.lOy > 0 ? p.lOy : aj2GradeOrigemY(uxIndice);
    gfx_opacidade_grupo = g0 * p.lA;
    gfx_transformar(fr.x, oy, p.lS, p.lDx, 0);
    aj2TextoLista = p.lTxt;
    if (uxIndice >= 2) ajDesenharFolha(); else aj2DesenharDiferencas();
    gfx_sem_transformar();
  }
  // O editor e os modais tem o teclado: os alvos de tras deixam de valer e,
  // sem alvo proprio, o clique e o OK (ponteiro.h, CAMADAS).
  if (modal || (uxEditor
#ifdef NV_TOUCH_UI
                && !ajPontInline && !ajEditandoLinha(uxOp)
#endif
               )) { ponteiro_camada();
#ifdef NV_TOUCH_UI
    ajToqueCamada();
#endif
  }
  if (aj2EditorPainel()) aj2EdVisto = 1;
  else if (p.eA <= 0.003f) aj2EdVisto = 0;
  ajPontEditor = aj2EditorPainel() && !(uxRestaurar || uxAvisoRisco) && !riscoFolha && !frAberta &&
                 p.eA > 0.997f && fabsf(p.eS - 1.0f) < 0.002f && fabsf(p.eDx) < 0.5f;
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
  // #205: os modais de digitacao/aviso limparam os alvos do rodape. Na edicao
  // na linha o rodape da lista continua na tela: repete o MESMO rodape sem
  // desenhar (a funcao escolhe de novo quais chips cabem com o aviso, senao
  // um alvo de OK/Voltar ficaria debaixo do "Ajuste salvo") para registrar os
  // chips sobre um anteparo. No Mobile (ajPontInline) a camada do quadro ja e
  // a da folha e o rodape tem os botoes de toque proprios — nao pode zerar
  // aqui. TV e desktop sem toque tambem: o clique do ponteiro cancela.
#ifdef NV_TOUCH_UI
  if (ajEditandoLinha(uxOp) && !ajPontInline && !modal) { ponteiro_camada(); ajToqueCamada();
#else
  if (ajEditandoLinha(uxOp) && !modal) { ponteiro_camada();
#endif
    ponteiro_alvo(0, 0, (float)NV_TELA_W, (float)NV_TELA_H, NULL, NULL, 0, 0);
    AJ_ESCALA_INI();
    { AjDica d[4]; aj2RodapeA3(aj2ListaR(), d, ajDicasDe(focoIndice ? -1 : focoOp, d), 1.0f, 2); }
    AJ_ESCALA_FIM(); }
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
#ifdef NV_TOUCH_UI
  ajToqueCamada();
  ajSubToqueChave = 1;
#endif
  AJ_ESCALA_INI();
  ajDesenharAddons(foco);
  AJ_ESCALA_FIM();
}
// --- 2.0.3: AS SUBLISTAS (ADDONS, PLUGINS) NO MODELO NOVO DOS AJUSTES ---------------
// Mesma pagina da categoria (cabecalho de ponta a ponta, a cena a esquerda, a
// lista com as linhas novas e as pilulas com o estado escrito a direita). Antes
// era o indice antigo na coluna da esquerda e o chip "Ligado" velho.
typedef struct { const char *nome, *apoio, *icone; int pilula, chevron, traduz; } AjSubLinha;
static void ajSubCabecalho(const char *ic, const char *kick, const char *titulo, const char *valor, const char *sub) {
  GfxRect h = aj2PaginaR();
  float ar, ag, ab, tinta = ajTinta() / 255.0f, tile = 76.0f, x = h.x + 28.0f, direita = h.x + h.w - 36.0f;
  ajustes_acento(&ar, &ag, &ab);
  ajPaginaIlha(h, 36.0f, 1);
  gfx_cor(h, ajTelaCheia() ? 0.0f : 36.0f / h.h, ar, ag, ab, 0.10f);
  { GfxRect tr = { x, h.y + (h.h - tile) * 0.5f, tile, tile };
    gfx_cor(tr, 22.0f / tile, ar, ag, ab, 1);
    gfx_icone((GfxRect){ tr.x + 19, tr.y + 19, 38, 38 }, ic, tinta, tinta, tinta, 1); }
  x += tile + 26.0f;
  { TxtLinha lv = ajTxtC(TXT_V2_28, valor, h.w * 0.30f), lq = ajTxtC(TXT_V2_24, sub, h.w * 0.30f);
    if (valor[0]) txt_desenhar_alpha(lv, direita - lv.w, h.y + h.h * 0.5f - lv.h - (sub[0] ? 0 : -lv.h * 0.5f), 0.85f);
    if (sub[0]) txt_desenhar_alpha(lq, direita - lq.w, h.y + h.h * 0.5f + 4.0f, 0.5f);
    direita -= (lv.w > lq.w ? lv.w : lq.w) + 40.0f; }
  ajKickerAcento(kick, x + 2.0f, h.y + h.h * 0.5f - 44.0f, 1);
  { TxtLinha tt = ajTxtC(TXT_V2_TIT, titulo, direita - x); txt_desenhar(tt, x, h.y + h.h * 0.5f - 18.0f); }
}
#define AJ_SUB_PASSO 96.0f
// PONTEIRO (#99) NAS SUBLISTAS (Addons, Plugins): a linha sob o cursor ganha o
// foco pelo `focar` da tela dona da lista (addonsui/pluginsui, a mesma variavel
// das setas); o OK do clique segue o caminho de sempre (ligar, abrir).
static void ajSubLista(const AjSubLinha *L, int n, int foco, const AjDica *d, int nd, const char *aviso,
                       PonteiroFn focar) {
  GfxRect fr = aj2ListaR();
  float topo = fr.y + 30.0f, base = fr.y + fr.h - AJ_A3_RODAPE - 8.0f, y, alvo;
  int i;
  ajPaginaIlha(fr, 40.0f, 1);
  alvo = foco * AJ_SUB_PASSO - (base - topo - AJ_SUB_PASSO) * 0.5f;
  if (alvo > n * AJ_SUB_PASSO - (base - topo)) alvo = n * AJ_SUB_PASSO - (base - topo);
  if (alvo < 0) alvo = 0;
#ifdef NV_TOUCH_UI
  if (ajTelaCheia()) {
    { static int anterior = -1, tipoAnterior = -1;
      int tipo = focar == addonsui_ponteiro ? (addonsui_ao_vivo() ? 3 : 1) : 2;
      if (tipo != tipoAnterior || foco != anterior) ajToqueLivre[AJT_SUB] = 0;
      anterior = foco; tipoAnterior = tipo;
    }
    alvo = ajToqueRegistrar(AJT_SUB, (GfxRect){fr.x, topo, fr.w, base - topo}, &ajToqueOffset[AJT_SUB],
                            n * AJ_SUB_PASSO - (base - topo), alvo, ajSubToqueChave);
  }
#endif
  gfx_recorte(fr.x, topo, fr.w, base - topo);
  y = topo - alvo;
  for (i = 0; i < n; i++, y += AJ_SUB_PASSO) {
    GfxRect r = { fr.x + 18.0f, y, fr.w - 36.0f, AJ_SUB_PASSO - 6.0f };
    float a = ajMascara(y + r.h * 0.5f, topo, base), cw = 0, tx = r.x + 22.0f + 52.0f + 18.0f, tw, yc = y + r.h * 0.5f;
    int f = i == foco;
    TxtLinha t, c;
    if (y + r.h < topo || y > base) continue;
    if (focar) ponteiro_alvo_faixa(r.x, r.y, r.w, r.h, topo, base, focar, NULL, i, 0);
    if (f) { float ar, ag, ab; ajFocoLinha(r, 18.0f, 1, a); ajustes_acento(&ar, &ag, &ab); gfx_cor(r, 18.0f / r.h, ar, ag, ab, 0.14f * a); }
    if (L[i].icone) {
      ajNeutro((GfxRect){ r.x + 22, yc - 26, 52, 52 }, 15, 0.09f, 0.141f, 0.149f, 0.173f, a);
      ajIcone(L[i].icone, r.x + 22 + 14, yc - 12, 24, 0.85f * a);
    } else ajRosto(L[i].nome, r.x + 22, yc - 26, 52, 15, 0.09f, 0.85f * a, TXT_ILHA_INICIAL);
    if (L[i].pilula >= 0) cw = aj2PilulaEstado(-1, r.x + r.w - 22.0f, yc, L[i].pilula, a) + 16.0f;
    if (L[i].chevron) {
      float dir = r.x + r.w - 22.0f - cw;
      aj2Chevron(dir, yc, (f ? 0.7f : 0.32f) * a);
#ifdef NV_TOUCH_UI
      if (ajTelaCheia() && focar == pluginsui_ponteiro)
        ponteiro_alvo_faixa(dir - 38.0f, r.y, 54.0f, r.h, topo, base,
                            pluginsui_ponteiro, pluginsui_detalhes, i, 0);
#endif
      cw += 34.0f;
    }
    tw = r.x + r.w - 22.0f - cw - tx - 12.0f;
    // O nome do addon/plugin e do usuario: nao passa por i18n.
    t = L[i].traduz ? ajTxtC(f ? TXT_V3_LN_B : TXT_V3_LN, L[i].nome, tw)
                    : txt_linha_corta(f ? TXT_V3_LN_B : TXT_V3_LN, L[i].nome, AJ_TX, tw);
    c = ajTxtC(TXT_V3_BEN, L[i].apoio ? L[i].apoio : "", tw);
    { float bh = t.h + (c.w ? 4 + c.h : 0), ty = yc - bh * 0.5f;
      txt_desenhar_alpha(t, tx, ty, (f ? 1.0f : 0.88f) * a);
      if (c.w) txt_desenhar_alpha(c, tx, ty + t.h + 4, 0.55f * a); }
  }
  gfx_sem_recorte();
  if (aviso && aviso[0]) {
    snprintf(uxAviso, sizeof uxAviso, "%s", aviso);
    uxAvisoAte = SDL_GetTicks() + 1000;
  }
  aj2RodapeA3(fr, d, nd, 1, 1);
}
// A coluna da esquerda: a cena gerada da linha (a mesma da categoria) e o
// kicker, o titulo e o texto da linha em foco.
static float ajSubInspetor(int op, const char *kick, const char *titulo, const char *texto) {
  GfxRect ir = aj2InspR();
  if (ajRetrato()) {
    if (!aj2TemInsp()) return ir.y + ir.h;
    float pw = fminf(560.0f, ir.w), px = ir.x + (ir.w - pw) * 0.5f, y = ir.y + 4.0f;
    gfx_recorte(ir.x, ir.y, ir.w, ir.h);
    ajCenaTroca(AJ_TROCA_INSPETOR, uxSecaoDe(op), op, px, y, pw, 0);
    y += pw * 9.0f / 16.0f + 16.0f;
    TxtLinha t = ajTxtC(TXT_V3_LN_B, titulo, ir.w);
    txt_desenhar_alpha(t, ir.x, y, 1);
    if (texto && texto[0]) ajBloco(TXT_V3_SUB, texto, ir.x, y + 40.0f, ir.w, 30.0f, 0.70f, 2);
    gfx_sem_recorte();
    return ir.y + ir.h;
  }
  float x = ir.x, w = ir.w, y = ir.y + 4.0f;
  y += ajCenaTroca(AJ_TROCA_INSPETOR, uxSecaoDe(op), op, x, y, w, 0) + 30.0f;
  ajKickerAcento(kick, x + 2.0f, y, 1);
  y += 34.0f;
  y += ajBloco(TXT_V2_A3TIT, titulo, x, y, w, 56.0f, 1, 2) + 10.0f;
  if (texto && texto[0]) y += ajBloco(TXT_V3_INSP, texto, x, y, w, 37.0f, 0.70f, 4) + 14.0f;
  return y;
}
static const char *ajSyncAddonsApoio(void) {
  if (!sessao_logada()) return "Entre na conta Nuvio para sincronizar os addons.";
  if (sync_manual_addons_rodando()) return "Sincronizando…";
  return "Busca agora os addons da sua conta, sem esperar a volta automática.";
}
static void ajDesenharAddons(int foco) {
  static AjSubLinha L[130];
  int n = addons_n(), i, lig = 0, live = addonsui_ao_vivo();
  char sub[96], val[64], k[160];
  montarTela();
  if (n > 128) n = 128;
  for (i = 0; i < n; i++) if (live ? addonsui_addon_ao_vivo(i) : addons_ativo(i)) lig++;
  ajFundoPagina();
  snprintf(val, sizeof val, i18n("%d de %d"), lig, n);
  snprintf(sub, sizeof sub, "%s", n ? i18n(live ? "TV ao vivo" : "Ligados nesta TV") : i18n("Nenhum addon nesta conta."));
  ajSubCabecalho("aj_puzzle", "Fontes e addons", live ? "TV ao vivo" : "Addons", n ? val : "", sub);
  if (foco > (live ? n - 1 : n + 1)) foco = live ? n - 1 : n + 1;
  if (foco < 0) foco = 0;
  { static char apoio[128][96];
    for (i = 0; i < n; i++) {
      snprintf(apoio[i], sizeof apoio[i], "%s", ajAddonFornece(i));   // ajAddonFornece devolve um buffer so
      L[i] = (AjSubLinha){ addons_nome(i), apoio[i], NULL, live ? addonsui_addon_ao_vivo(i) : addons_ativo(i), 0, 0 };
    } }
  L[n] = (AjSubLinha){ "Sincronizar addons", ajSyncAddonsApoio(), "aj_rotate-cw", -1, 0, 1 };
  L[n + 1] = (AjSubLinha){ "TV ao vivo", "Os addons selecionados ignoram os filtros de fontes.", "aj_tv-minimal-play", -1, 0, 1 };
  { AjDica d[3] = { { "OK", !live && foco == n ? "Sincronizar addons" : "Mudar" }, { "↑ ↓", "Navegar" }, { "Voltar", "Fontes e addons" } };
    ajSubLista(L, live ? n : n + 2, foco, d, 3, NULL, addonsui_ponteiro); }
  snprintf(k, sizeof k, "%s · %s", i18n("Fontes e addons"), i18n("Addons"));
  if (live) ajSubInspetor(AJ_ADDONS, k, n ? addons_nome(foco) : "TV ao vivo", i18n("Os addons selecionados ignoram os filtros de fontes."));
  else if (foco == n + 1) ajSubInspetor(AJ_ADDONS, k, "TV ao vivo", i18n("Os addons selecionados ignoram os filtros de fontes."));
  else if (foco == n) ajSubInspetor(AJ_ADDONS, k, "Sincronizar addons", i18n(ajSyncAddonsApoio()));
  else if (n > 0) {
    const char *base0 = addons_base(foco);
    char host[160] = "", forn[200];
    GfxRect ir = aj2InspR();
    float y;
    if (base0) {
      const char *p = strstr(base0, "://");
      p = p ? p + 3 : base0;
      snprintf(host, sizeof host, "%.*s", (int)strcspn(p, "/"), p);
    }
    snprintf(forn, sizeof forn, "%s", ajAddonFornece(foco));
    y = ajSubInspetor(AJ_ADDONS, k, addons_nome(foco), i18n("Os addons vêm da sua conta Nuvio. Ligar e desligar aqui vale só nesta TV, até a próxima sincronização."));
    if (y + 3 * 57.0f <= ir.y + ir.h) {
      y += aj2Meta(i18n("Fornece"), forn, ir.x, y, ir.w, 1);
      y += aj2Meta(i18n("Nesta TV"), i18n(addons_ativo(foco) ? "Ligado" : "Desligado"), ir.x, y, ir.w, 1);
      aj2Meta(i18n("Endereço"), host, ir.x, y, ir.w, 1);
    }
  } else ajSubInspetor(AJ_ADDONS, k, "Addons", i18n("Nenhum addon nesta conta."));
}

// PONTEIRO (#99) NAS FOLHAS "PERMITIDOS" E "ORDEM": a linha (e, na ordem, o
// botao subir/descer) sob o cursor ganha o foco por frFoco/frCol, as variaveis
// das setas; o OK do clique liga/desliga ou move, como o do controle.
static void ajPontFolha(int i, int col) {
  if (!frAberta || teclado_aberto() || i < 0 || i > frN) return;
  if ((frAberta == 3 || frAberta == 7) && i > 0) { if (col < 0 || col > 1) return; frCol = col; }
  frFoco = i;
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
  ponteiro_camada();
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
#ifdef NV_TOUCH_UI
  if (ajTelaCheia()) {
    ajToqueCamada();
    alvo = ajToqueRegistrar(AJT_FONTES, (GfxRect){m.x, topo, m.w, base - topo}, &ajToqueOffset[AJT_FONTES],
                            n * 84.0f - (base - topo), alvo, frAberta);
  }
#endif
  gfx_recorte(m.x, topo, m.w, base - topo);
  y = topo - alvo;
  for (i = 0; i < n; i++, y += 84) {
    GfxRect r = { m.x + 18, y, m.w - 36, 80 };
    int f = i == frFoco, lig;
    const char *nome, *apoio;
    float cw, tw;
    if (y + 80 < topo || y > base) continue;
    ponteiro_alvo_faixa(r.x, r.y, r.w, r.h, topo, base, ajPontFolha, NULL, i, -1);
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

// "Catálogos do destaque": busca no topo, fontes fixas, Sugeridos e os
// catálogos por addon. Mesmo molde da folha de permitidos; o dedo foca a linha.
static void ajPontHeroCat(int i, int col) {
  (void)col;
  if (hcAberta && !teclado_aberto() && hcFocavel(i)) hcFoco = i;
}
static void desenhaFolhaHeroCat(void) {
  GfxRect m = { 460, 90, 1000, 900 };
  float topo, base, y, alvo, total = 0, yFoco = 0;
  const char *atual = fil_hero_fonte();
  int i;
  char sub[200];
  hcMontar();
  ponteiro_camada();
  ajVeuModal();
  ajCentraModal(&m);
  ajIlha(m, 36, 1, 1);
  snprintf(sub, sizeof sub, "%s", i18n("Escolha de onde vêm os títulos do destaque. Busque pelo nome do catálogo ou do addon."));
  { char k[160];
    snprintf(k, sizeof k, "%s · %s", i18n("Ajustes"), i18n("Destaque"));
    topo = ajCabecalhoFolha(m, k, "Catálogos do destaque", sub, 0, 1); }
  base = m.y + m.h - 96.0f;
  for (i = 0; i < hcN; i++) {
    float h = hcL[i].tipo == HC_CAB ? 48.0f : 84.0f;
    if (i == hcFoco) yFoco = total + h * 0.5f;
    total += h;
  }
  alvo = yFoco - (base - topo) * 0.5f;
  if (alvo > total - (base - topo)) alvo = total - (base - topo);
  if (alvo < 0) alvo = 0;
#ifdef NV_TOUCH_UI
  ajToqueCamada();
  alvo = ajToqueRegistrar(AJT_FONTES, (GfxRect){m.x, topo, m.w, base - topo}, &ajToqueOffset[AJT_FONTES],
                          total - (base - topo), alvo, 100 + (hcBusca[0] != 0));
#endif
  gfx_recorte(m.x, topo, m.w, base - topo);
  y = topo - alvo;
  for (i = 0; i < hcN; i++) {
    int t = hcL[i].tipo, f = i == hcFoco;
    float h = t == HC_CAB ? 48.0f : 84.0f;
    GfxRect r = { m.x + 18, y, m.w - 36, h - 4 };
    const char *nome = hcL[i].txt, *apoio = "";
    char ap[200] = "";
    int marcado = 0;
    float tw = r.w - 22 - 64 - 90;
    if (y + h >= topo && y <= base) {
      if (t == HC_CAB) {
        TxtLinha c = ajTxtC(TXT_ILHA_GENERO, hcL[i].idx ? nome : i18n(nome), r.w - 44);
        txt_desenhar_alpha(c, r.x + 22, y + 22, 0.55f);
      } else if (t == HC_VAZIO) {
        TxtLinha c = ajTxtC(TXT_AJ_ROTULO, i18n(nome), r.w - 44);
        txt_desenhar_alpha(c, r.x + 22, y + 24, 0.6f);
      } else {
        ponteiro_alvo_faixa(r.x, r.y, r.w, r.h, topo, base, ajPontHeroCat, NULL, i, -1);
        if (f) ajFocoLinha(r, 22, 1, 1);
        if (t == HC_BUSCA) {
          nome = hcBusca[0] ? hcBusca : i18n("Buscar catálogo");
          apoio = hcBusca[0] ? i18n("OK para mudar a busca") : i18n("OK para digitar");
          ajNeutro((GfxRect){ r.x + 22, y + 16, 48, 48 }, 14, 0.09f, 0.141f, 0.149f, 0.173f, 1);
          ajIcone("aj_search", r.x + 22 + 14, y + 16 + 14, 20, 0.85f);
        } else if (t == HC_FIXA) {
          nome = i18n(nome);
          marcado = hcL[i].idx ? (atual[0] == '*' && !atual[1]) : !atual[0];
          ajNeutro((GfxRect){ r.x + 22, y + 16, 48, 48 }, 14, 0.09f, 0.141f, 0.149f, 0.173f, 1);
          ajIcone("aj_film", r.x + 22 + 14, y + 16 + 14, 20, 0.85f);
        } else {
          int ix = hcL[i].idx;
          nome = fil_titulo(ix);
          snprintf(ap, sizeof ap, "%s%s%s", fil_linha_addon(ix), fil_linha_addon(ix)[0] && fil_linha_conteudo(ix)[0] ? " · " : "",
                   fil_linha_conteudo(ix));
          apoio = ap;
          marcado = !strcmp(fil_chave(ix), atual);
          ajRosto(nome, r.x + 22, y + 16, 48, 14, 0.09f, 0.85f, TXT_ILHA_INICIAL);
        }
        { TxtLinha tl = t == HC_CAT ? txt_linha_corta(TXT_AJ_ROTULO, nome, AJ_TX, tw) : ajTxtC(TXT_AJ_ROTULO, nome, tw);
          TxtLinha c = ajTxtC(TXT_ILHA_GENERO, apoio, tw);
          float ty = y + 40 - (tl.h + (apoio[0] ? 4 + c.h : 0)) * 0.5f;
          txt_desenhar_alpha(tl, r.x + 22 + 48 + 16, ty, f ? 1.0f : 0.86f);
          if (apoio[0]) txt_desenhar_alpha(c, r.x + 22 + 48 + 16, ty + tl.h + 4, 0.42f); }
        if (marcado) ajChipLiga(r.x + r.w - 22, y + 40, 1, 1);
      }
    }
    y += h;
  }
  gfx_sem_recorte();
  { AjDica d[2] = { { "OK", "Escolher" }, { "Voltar", "Fechar" } };
    ajDicas(d, 2, m.x + 44, m.y + m.h - 36, m.w - 88, 1); }
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
  ponteiro_camada();
  ajVeuModal();
  ajCentraModal(&m);
  ajIlha(m, 36, 1, 1);
  snprintf(sub, sizeof sub, "%s", frAberta == 7
    ? i18n("Esquerda e direita escolhem subir ou descer; OK move.")
    : i18n("Esquerda e direita escolhem subir ou descer; OK move. Quem está fora da ordem toca depois de todos."));
  snprintf(k, sizeof k, "%s · %s", i18n("Fontes e addons"), i18n("Escolha da fonte"));
  topo = ajCabecalhoFolha(m, k, frAberta == 7 ? "Ordem das notas na linha do título" : "Ordem dos add-ons", sub, 0, 1);
  base = m.y + m.h - 96.0f;
  alvo = frFoco * 84.0f - (base - topo - 80.0f) * 0.5f;
  if (alvo > n * 84.0f - (base - topo)) alvo = n * 84.0f - (base - topo);
  if (alvo < 0) alvo = 0;
#ifdef NV_TOUCH_UI
  if (ajTelaCheia()) {
    ajToqueCamada();
    alvo = ajToqueRegistrar(AJT_FONTES, (GfxRect){m.x, topo, m.w, base - topo}, &ajToqueOffset[AJT_FONTES],
                            n * 84.0f - (base - topo), alvo, frAberta);
  }
#endif
  gfx_recorte(m.x, topo, m.w, base - topo);
  y = topo - alvo;
  for (i = 0; i < n; i++, y += 84) {
    GfxRect r = { m.x + 18, y, m.w - 36, 80 };
    int f = i == frFoco;
    if (y + 80 < topo || y > base) continue;
    ponteiro_alvo_faixa(r.x, r.y, r.w, r.h, topo, base, ajPontFolha, NULL, i, frCol);
    if (f) ajFocoLinha(r, 22, 1, 1);
    if (i == 0) {
      TxtLinha t = ajTxtC(TXT_AJ_ROTULO, i18n(frAberta == 7 ? "Padrão" : "Sem ordem"), r.w - 120);
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
        ponteiro_alvo_faixa(b.x, b.y, b.w, b.h, topo, base, ajPontFolha, NULL, i, c);
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

// Same reorder/number-editor interaction as the add-on order sheet, with
// explicit targets in TV and Mobile. Labels never identify persisted rows.
static void faPont(int i, int col) {
  if (frAberta < 4 || teclado_aberto() || i < 0 || i >= faN || col < 0 || col > (frAberta==8?1:2)) return;
  faFoco = i; faCol = col;
}
static void faContaFocar(int i,int b) { (void)b;faFoco=i; }
static void faFechar(int a, int b) { (void)a; (void)b; if (frAberta >= 4) frAberta = 0; }
static void desenhaFolhaFalha(void) {
  GfxRect m = {460, 90, 1000, 900}; float topo, base, y, alvo;
  if(frAberta==8 && !nv_perfil_sessao_atual(faIptvSessao)) { frAberta=0;return; }
  int contas=frAberta==8;
  faMontar(); ponteiro_camada(); ajVeuModal(); ajCentraModal(&m); ajIlha(m, 36, 1, 1);
  topo = ajCabecalhoFolha(m, contas?"TV ao vivo":"Fontes e addons", contas?"Contas de TV ao vivo":frAberta == 4 ? "Ordem e limites dos provedores" : "Ordem e limites dos add-ons",
                         contas?"":i18n("Limites contam links distintos, incluindo o primeiro. Use as setas para ordenar e o número para ajustar."), 0, 1);
  base = m.y + m.h - 96.0f;
  alvo = faFoco * 84.0f - (base - topo - 80.0f) * 0.5f;
  if (alvo > faN * 84.0f - (base - topo)) alvo = faN * 84.0f - (base - topo);
  if (alvo < 0) alvo = 0;
#ifdef NV_TOUCH_UI
  if (ajTelaCheia()) {
    ajToqueCamada();
    alvo = ajToqueRegistrar(AJT_FONTES, (GfxRect){m.x, topo, m.w, base - topo}, &ajToqueOffset[AJT_FONTES],
                            faN * 84.0f - (base - topo), alvo, frAberta);
  }
#endif
  gfx_recorte(m.x, topo, m.w, base - topo); y = topo - alvo;
  for (int i = 0; i < faN; i++, y += 84.0f) {
    if (y + 80.0f < topo || y > base) continue;
    GfxRect r = {m.x + 18.0f, y, m.w - 36.0f, 80.0f};
    int f = i == faFoco; float bw = 56.0f, bx = r.x + r.w - 22.0f - (contas?2.0f:3.0f) * bw - 16.0f;
    float tw = fmaxf(1.0f, bx - (r.x + 36.0f) - 24.0f);
    // Tapping the row edits its limit; reorder always has an explicit arrow.
    ponteiro_alvo_faixa(r.x, r.y, r.w, r.h, topo, base, faPont, contas?faContaFocar:NULL, i, contas?0:2);
    if (f) ajFocoLinha(r, 22.0f, 1.0f, 1.0f);
    TxtLinha t = txt_linha_corta(TXT_AJ_ROTULO, faItem[i].nome, AJ_TX, tw);
    TxtLinha ap = ajTxtC(TXT_ILHA_GENERO, faItem[i].apoio, tw);
    float ty = y + 40.0f - (t.h + (faItem[i].apoio[0] ? 4.0f + ap.h : 0.0f)) * 0.5f;
    txt_desenhar_alpha(t, r.x + 36.0f, ty, f ? 1.0f : 0.86f);
    if (faItem[i].apoio[0]) txt_desenhar_alpha(ap, r.x + 36.0f, ty + t.h + 4.0f, 0.42f);
    for (int col = 0; col < (contas?2:3); col++) {
      GfxRect b = {bx + col * (bw + 8.0f), y + 12.0f, bw, bw};
      int bf = f && faCol == col, sem = (col == 0 && i == 0) || (col == 1 && i + 1 == faN);
      ponteiro_alvo_faixa(b.x, b.y, b.w, b.h, topo, base, faPont, NULL, i, col);
      if (bf) ajPilulaAcento(b, 28.0f, 1.0f); else ajNeutro(b, 28.0f, 0.08f, AJ_CHIP_SOL, 1.0f);
      if (col < 2) {
        if (bf) ajIconeT(col ? "aj_arrow-down" : "aj_arrow-up", b.x + 14.0f, b.y + 14.0f, 28.0f, sem ? 0.3f : 1.0f);
        else ajIcone(col ? "aj_arrow-down" : "aj_arrow-up", b.x + 14.0f, b.y + 14.0f, 28.0f, sem ? 0.3f : 0.85f);
      } else {
        char n[8]; snprintf(n, sizeof n, "%d", faLimite(i));
        TxtLinha num = bf ? ajTxtT(TXT_V2_SEG, n) : ajTxt(TXT_V2_SEG, n);
        txt_desenhar_alpha(num, b.x + (b.w - num.w) * 0.5f, b.y + (b.h - num.h) * 0.5f, 1.0f);
      }
    }
  }
  if (!faN) {
    TxtLinha empty = ajTxtC(TXT_AJ_ROTULO, contas?"Nenhuma":"Nenhum add-on instalado.", m.w - 88.0f);
    txt_desenhar_alpha(empty, m.x + 44.0f, topo + 24.0f, 0.7f);
  }
  gfx_sem_recorte();
  TxtLinha close = ajTxt(TXT_V2_SEG, "Fechar");
  GfxRect fechar = {m.x + m.w - close.w - 84.0f, m.y + m.h - 76.0f, close.w + 40.0f, 48.0f};
  ajNeutro(fechar, 24.0f, 0.10f, AJ_CHIP_SOL, 1.0f);
  txt_desenhar_alpha(close, fechar.x + 20.0f, fechar.y + (fechar.h - close.h) * 0.5f, 0.9f);
  ponteiro_alvo(fechar.x, fechar.y, fechar.w, fechar.h, NULL, faFechar, 0, 0);
  if (!ajTelaCheia()) {
    AjDica d[2] = {{"OK", contas?"Mover":"Editar"}, {"Voltar", "Fechar"}};
    ajDicasLinha(d, 2, m.x + 44.0f, m.y + m.h - 66.0f, 1.0f);
  }  if (uxAviso[0]) {
    TxtLinha aviso = ajTxtC(TXT_AJ_SUB, uxAviso, NV_VTELA_W - 80.0f);
    txt_desenhar_alpha(aviso, (NV_VTELA_W - aviso.w) * 0.5f, NV_VTELA_H - 50.0f, 0.8f);
  }
}

// Shared source exclusions. Checkmarks mean excluded, in both TV and Mobile;
// this uses the same clipped rows and pointer dispatch as the source sheets.
static void ffPont(int i, int categoria) {
  if (frAberta != 6 || teclado_aberto() || categoria != ffCategoria) return;
  int n = ffCategoria >= 0 ? fontefiltro_opcoes_n(ffCategoria) : 9;
  if (i < 0 || i >= n) return;
  if (ffCategoria >= 0) ffValorFoco = i; else ffFoco = i;
}
static void ffPontVoltar(int a, int b) { (void)a; (void)b; ffVoltar(); }
static void desenhaFolhaFiltros(void) {
  GfxRect m = {460.0f, 90.0f, 1000.0f, 900.0f};
  float topo, base, alvo, y;
  int n = ffCategoria >= 0 ? fontefiltro_opcoes_n(ffCategoria) : 9;
  int foco = ffCategoria >= 0 ? ffValorFoco : ffFoco;
  FonteFiltroConfig cfg; fontefalha_filtro(&cfg);
  ponteiro_camada(); ajVeuModal(); ajCentraModal(&m); ajIlha(m, 36.0f, 1.0f, 1.0f);
  topo = ajCabecalhoFolha(m, ffCategoria >= 0 ? "Filtros das fontes" : "Fontes e addons",
                         ffCategoria >= 0 ? fontefiltro_rotulo(ffCategoria) : "Filtros das fontes",
                         i18n(ffCategoria >= 0
                           ? "Marcado = filtrado. OK ou toque marca/desmarca e salva na hora."
                           : "Selecione os valores que deseja filtrar. As alterações são salvas na hora e valem antes dos limites de links."),
                         0, 1);
  base = m.y + m.h - 96.0f;
  alvo = foco * 84.0f - (base - topo - 80.0f) * 0.5f;
  if (alvo > n * 84.0f - (base - topo)) alvo = n * 84.0f - (base - topo);
  if (alvo < 0.0f) alvo = 0.0f;
#ifdef NV_TOUCH_UI
  ajToqueCamada();
  alvo = ajToqueRegistrar(AJT_FONTES, (GfxRect){m.x, topo, m.w, base - topo}, &ajToqueOffset[AJT_FONTES],
                          n * 84.0f - (base - topo), alvo, 600 + ffCategoria);
#endif
  gfx_recorte(m.x, topo, m.w, base - topo); y = topo - alvo;
  for (int i = 0; i < n; i++, y += 84.0f) {
    GfxRect r = {m.x + 18.0f, y, m.w - 36.0f, 80.0f};
    if (y + r.h < topo || y > base) continue;
    int f = i == foco;
    ponteiro_alvo_faixa(r.x, r.y, r.w, r.h, topo, base, ffPont, NULL, i, ffCategoria);
    if (f) ajFocoLinha(r, 22.0f, 1.0f, 1.0f);
    if (ffCategoria >= 0) {
      int excluido = (cfg.excluir[ffCategoria] & fontefiltro_bit(ffCategoria, i)) != 0;
      GfxRect caixa = {r.x + r.w - 66.0f, y + 20.0f, 40.0f, 40.0f};
      TxtLinha nome = ajTxtC(TXT_AJ_ROTULO, fontefiltro_opcao(ffCategoria, i), r.w - 122.0f);
      txt_desenhar_alpha(nome, r.x + 36.0f, y + (r.h - nome.h) * 0.5f, f ? 1.0f : 0.86f);
      if (excluido) {
        ajPilulaAcento(caixa, 12.0f, 1.0f);
        ajIconeT("aj_check", caixa.x + 8.0f, caixa.y + 8.0f, 24.0f, 1.0f);
      } else ajNeutro(caixa, 12.0f, 0.10f, AJ_CHIP_SOL, 1.0f);
    } else {
      TxtLinha nome = ajTxtC(TXT_AJ_ROTULO, ffNome(i), r.w - 100.0f);
      // Values are already translated, and user-entered release groups remain literal.
      TxtLinha valorFiltro = txt_linha_corta(TXT_ILHA_GENERO, ffValor(i), AJ_TX, r.w - 100.0f);
      float ty = y + (r.h - nome.h - 4.0f - valorFiltro.h) * 0.5f;
      txt_desenhar_alpha(nome, r.x + 36.0f, ty, f ? 1.0f : 0.86f);
      txt_desenhar_alpha(valorFiltro, r.x + 36.0f, ty + nome.h + 4.0f, 0.52f);
      ajIcone("aj_chevron-right", r.x + r.w - 54.0f, y + 28.0f, 24.0f, f ? 1.0f : 0.65f);
    }
  }
  gfx_sem_recorte();
  TxtLinha fechar = ajTxt(TXT_V2_SEG, ffCategoria >= 0 ? "Voltar" : "Fechar");
  GfxRect botao = {m.x + m.w - fechar.w - 84.0f, m.y + m.h - 76.0f, fechar.w + 40.0f, 48.0f};
  ajNeutro(botao, 24.0f, 0.10f, AJ_CHIP_SOL, 1.0f);
  txt_desenhar_alpha(fechar, botao.x + 20.0f, botao.y + (botao.h - fechar.h) * 0.5f, 0.9f);
  ponteiro_alvo(botao.x, botao.y, botao.w, botao.h, NULL, ffPontVoltar, 0, 0);
  if (!ajTelaCheia()) {
    AjDica d[2] = {{"OK", ffCategoria >= 0 ? "Filtrar" : "Editar"}, {"Voltar", ffCategoria >= 0 ? "Voltar" : "Fechar"}};
    ajDicasLinha(d, 2, m.x + 44.0f, m.y + m.h - 66.0f, 1.0f);
  }
  if (uxAviso[0]) {
    TxtLinha aviso = ajTxtC(TXT_AJ_SUB, uxAviso, NV_VTELA_W - 80.0f);
    txt_desenhar_alpha(aviso, (NV_VTELA_W - aviso.w) * 0.5f, NV_VTELA_H - 50.0f, 0.8f);
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
#ifdef NV_TOUCH_UI
  ajToqueCamada();
  ajSubToqueChave = 2 + v->nivel * 400 + v->repo;
#endif
  AJ_ESCALA_INI();
  ajDesenharPlugins(v);
  AJ_ESCALA_FIM();
}
static void ajDesenharPlugins(const AjPluginsVista *v) {
  static AjSubLinha L[96];
  static char nomes[96][128], apoios[96][160];
  int disp = plugins_disponivel(), nr = plugins_n_repos(), n, i, foco = v->foco;
  char sub[160], val[64], k[200], linha[200];
  PlugRepo rp;
  montarTela();
  ajFundoPagina();
  memset(&rp, 0, sizeof rp);
  if (v->nivel == 1) plugins_repo(v->repo, &rp);
  val[0] = 0;
  if (!disp) snprintf(sub, sizeof sub, "%s", i18n("Indisponível nesta plataforma"));
  else if (v->nivel == 0) {
    int ns = 0, q;
    for (q = 0; q < nr; q++) ns += plugins_repo_scrapers(q, NULL);
    snprintf(sub, sizeof sub, i18n("%d repositórios · %d scrapers"), nr, ns);
    snprintf(val, sizeof val, "%s", i18n(plugins_ligado() ? "Ligado" : "Desligado"));
  } else {
    int lig, tot = plugins_repo_scrapers(v->repo, &lig);
    snprintf(sub, sizeof sub, i18n("%d scrapers · %d ligados"), tot, lig);
  }
  ajSubCabecalho("aj_plug", v->nivel == 1 ? "Plugins" : "Fontes e addons",
                 v->nivel == 1 ? (rp.nome[0] ? rp.nome : rp.url) : "Plugins", val, sub);
  n = !disp ? 1 : v->nivel == 0 ? 2 + nr : 1 + plugins_repo_scrapers(v->repo, NULL);
  if (n > 96) n = 96;
  if (foco >= n) foco = n - 1;
  for (i = 0; i < n; i++) {
    AjSubLinha *l = &L[i];
    *l = (AjSubLinha){ "", "", NULL, -1, 0, 1 };
    if (!disp) { l->nome = "Plugins"; l->apoio = "O motor de plugins não roda neste aparelho."; l->icone = "aj_plug"; }
    else if (v->nivel == 0 && i == 0) {
      l->nome = "Plugins"; l->apoio = "Scrapers dos repositórios Nuvio como mais uma fonte, ao lado dos add-ons"; l->icone = "aj_plug"; l->pilula = plugins_ligado();
    } else if (v->nivel == 0 && i == 1) {
      l->nome = "Adicionar repositório"; l->apoio = "Digite a URL do manifest.json"; l->icone = "aj_plus";
    } else if (v->nivel == 0) {
      PlugRepo q; int lig, tot;
      plugins_repo(i - 2, &q);
      tot = plugins_repo_scrapers(i - 2, &lig);
      snprintf(nomes[i], sizeof nomes[i], "%s", q.nome[0] ? q.nome : q.url);
      if (plugins_repo_estado(i - 2) == 1) snprintf(apoios[i], sizeof apoios[i], i18n("%d scrapers · %d ligados"), tot, lig);
      else snprintf(apoios[i], sizeof apoios[i], "%s", i18n(ajPlugEstado(i - 2)));
      l->nome = nomes[i]; l->apoio = apoios[i]; l->pilula = q.ativo; l->chevron = 1; l->traduz = 0;
    } else if (i == 0) {
      l->nome = "Remover repositório"; l->icone = "aj_x";
      l->apoio = v->armado ? "OK de novo remove o repositório" : "OK para remover";
    } else {
      PlugScraper sc;
      if (plugins_scraper(v->repo, i - 1, &sc)) {
        snprintf(nomes[i], sizeof nomes[i], "%s", sc.nome);
        snprintf(apoios[i], sizeof apoios[i], "%s", i18n(sc.filme && sc.serie ? "Filmes e séries" : sc.filme ? "Filmes" : "Séries"));
        l->nome = nomes[i]; l->apoio = apoios[i]; l->pilula = sc.ativo; l->traduz = 0;
      }
    }
  }
  if (!disp) { AjDica d[1] = { { "Voltar", "Fontes e addons" } }; ajSubLista(L, n, foco, d, 1, v->aviso, pluginsui_ponteiro); }
  else if (v->nivel == 0) {
    AjDica d[3] = { { "OK", foco == 1 ? "Adicionar" : "Mudar" }, { "→", "Scrapers" }, { "Voltar", "Fontes e addons" } };
    if (foco < 2) d[1] = d[2];
    ajSubLista(L, n, foco, d, foco >= 2 ? 3 : 2, v->aviso, pluginsui_ponteiro);
  } else {
    AjDica d[2] = { { "OK", foco == 0 ? "Remover" : "Mudar" }, { "Voltar", "Repositórios" } };
    ajSubLista(L, n, foco, d, 2, v->aviso, pluginsui_ponteiro);
  }
  snprintf(k, sizeof k, "%s · %s", i18n("Fontes e addons"), i18n("Plugins"));
  { GfxRect ir = aj2InspR();
    float y;
    char host[160] = "";
    if (disp && v->nivel == 0 && foco >= 2 && plugins_repo(foco - 2, &rp)) {
      int lig, tot = plugins_repo_scrapers(foco - 2, &lig);
      ajPlugHost(rp.url, host, sizeof host);
      y = ajSubInspetor(AJ_PLUGINS, k, rp.nome[0] ? rp.nome : host, "");
      snprintf(linha, sizeof linha, i18n("%d scrapers · %d ligados"), tot, lig);
      if (y + 3 * 57.0f <= ir.y + ir.h) {
        y += aj2Meta(i18n("Estado"), i18n(ajPlugEstado(foco - 2)), ir.x, y, ir.w, 1);
        y += aj2Meta(i18n("Scrapers"), linha, ir.x, y, ir.w, 1);
        aj2Meta(i18n("Endereço"), host, ir.x, y, ir.w, 1);
      }
    } else if (disp && v->nivel == 1 && foco >= 1) {
      PlugScraper sc;
      if (plugins_scraper(v->repo, foco - 1, &sc)) {
        y = ajSubInspetor(AJ_PLUGINS, k, sc.nome, "");
        if (ajRetrato()) return;
        y += aj2Meta(i18n("Nesta TV"), i18n(sc.ativo ? "Ligado" : "Desligado"), ir.x, y, ir.w, 1);
        aj2Meta(i18n("Tipo"), i18n(sc.filme && sc.serie ? "Filmes e séries" : sc.filme ? "Filmes" : "Séries"), ir.x, y, ir.w, 1);
      }
    } else if (disp && v->nivel == 1) {
      ajSubInspetor(AJ_PLUGINS, k, "Remover repositório", "Remove o repositório desta TV e da sua conta. Remover o último deixa a lista vazia também na conta.");
    } else if (disp && foco == 1) {
      ajSubInspetor(AJ_PLUGINS, k, "Adicionar repositório", "A URL do manifest.json. Ex.: raw.githubusercontent.com/usuario/repo/main");
    } else {
      ajSubInspetor(AJ_PLUGINS, k, "Plugins", disp
        ? "Scrapers em JavaScript dos repositórios de plugins do Nuvio, como mais uma fonte na lista, ao lado dos add-ons. Os repositórios vêm da sua conta. Desligado por padrão."
        : "O motor de plugins não roda neste aparelho.");
    } }
}

// --- KIT DAS ILHAS PARA AS TELAS VIZINHAS (diagnostico.c, livetvdiag.c) -----------
// As mesmas pecas de Ajustes, para o diagnostico, o teste de velocidade e o
// diagnostico da Live TV ornarem com a tela de onde saem. Ver ajustes.h.
void ajustes_ui_fundo(void) { ajFundo(); }
void ajustes_ui_arte(int n) { ajArteFundoN = n; }
void ajustes_ui_ilha(GfxRect r, float raioPx, int modal) { ajIlha(r, raioPx, 1, modal); }
void ajustes_ui_ilha_a(GfxRect r, float raioPx, int modal, float a) { ajIlha(r, raioPx, a, modal); }
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

