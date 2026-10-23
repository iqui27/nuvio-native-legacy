// A folha do perfil (2.1): lista de acoes, escolha de perfil, checklist da copia,
// confirmacao e o bloqueio de perfil infantil. Ver ajustes_ux_perfil.inc.
#define PF_LH 56.0f
#define PF_GAP 6.0f

static void pfTextos(char *kick, size_t nk, const char *tit[1], char *corpo, size_t nc) {
  const char *nome = pfNome(perfis_ativo());
  static char ed[96];
  corpo[0] = 0;
  snprintf(ed, sizeof ed, "%s", pfNome(ajustes_edicao_perfil()));
  switch (pfTela) {
    case PF_MENU:
      snprintf(kick, nk, "%s", ajustes_edicao_perfil() ? i18n("Editando outro perfil") : pfEhAdmin() ? i18n("Perfil · Administrador") : perfil_infantil_ativo() ? i18n("Perfil · Infantil") : i18n("Perfil"));
      tit[0] = ajustes_edicao_perfil() ? ed : nome[0] ? nome : i18n("Perfil");
      if (ajustes_edicao_perfil()) snprintf(corpo, nc, "%s", i18n("Você está mudando os ajustes de outro perfil. O seu fica guardado e volta quando terminar."));
      else if (perfil_infantil_ativo()) snprintf(corpo, nc, "%s", i18n("Este é um perfil infantil: as ações de administrador não ficam disponíveis aqui."));
      else snprintf(corpo, nc, "%s", i18n("Cada perfil guarda os próprios ajustes de aparência, player e home. O que descreve a TV (4K, memória, rede, idioma da interface) vale para todos."));
      break;
    case PF_EDITAR:
      snprintf(kick, nk, "%s", i18n("Perfil · Administrador"));
      tit[0] = i18n("Editar ajustes de outro perfil");
      snprintf(corpo, nc, "%s", i18n("Escolha o perfil. O PIN de administrador é pedido uma vez por visita. Enquanto edita, nada muda no seu perfil."));
      break;
    case PF_COPIAR:
      snprintf(kick, nk, "%s", i18n("Perfil · Administrador"));
      tit[0] = i18n("Copiar meus ajustes para…");
      snprintf(corpo, nc, "%s", i18n("Copia aparência, player, home e legendas. Não copia Jellyfin, IPTV, chaves de API, histórico nem o que é da TV."));
      break;
    case PF_INFANTIL:
      snprintf(kick, nk, "%s", i18n("Perfil · Administrador"));
      tit[0] = i18n("Perfil infantil e trava de ajustes");
      snprintf(corpo, nc, "%s", i18n("OK muda entre Comum, Infantil e Infantil com ajustes travados. Infantil aplica padrões seguros. Travado só abre os Ajustes com o PIN de administrador."));
      break;
    case PF_CONFIRMA:
      snprintf(kick, nk, "%s", i18n("Perfil"));
      if (pfConfirmaTipo == PL_COPIAR) {
        char nomes[160] = "";
        int i, k = 0;
        for (i = 0; i < perfis_n(); i++) {
          const ContaPerfil *p = perfis_item(i);
          if (!p || p->indice == perfis_ativo()) continue;
          if (pfMarca[k++]) { size_t l = strlen(nomes); snprintf(nomes + l, sizeof nomes - l, "%s%s", l ? ", " : "", p->nome); }
        }
        tit[0] = i18n("Copiar os seus ajustes?");
        snprintf(corpo, nc, i18n("Os ajustes de %s serão trocados pelos seus. Eles podem mudar de novo depois."), nomes);
      } else {
        tit[0] = i18n("Restaurar o padrão deste perfil?");
        snprintf(corpo, nc, "%s", i18n("As opções do perfil voltam ao padrão de fábrica. O que é da TV, as contas e o histórico não mudam."));
      }
      break;
    default:   /* PF_BLOQUEIO */
      snprintf(kick, nk, "%s", i18n("Perfil infantil"));
      tit[0] = i18n("Ajustes bloqueados");
      snprintf(corpo, nc, "%s", i18n("O administrador travou os ajustes deste perfil. Digite o PIN de administrador para abrir."));
      break;
  }
}

static void pfDesenhar(void) {
  PfLinha L[PF_MAX];
  char kick[160], corpo[400];
  const char *tit[1] = { "" };
  GfxRect m = { 460, 0, 1000, 0 };
  float x, y, hTxt;
  int n = pfLinhas(L), i;
  pfTextos(kick, sizeof kick, tit, corpo, sizeof corpo);
  hTxt = txt_bloco(TXT_AJ_TEXTO, corpo, 0, 0, 0, 0, -4000, m.w - 88, 30, 0, 4);
  m.h = 44 + 22 + 8 + 44 + 10 + hTxt + 22 + (float)n * (PF_LH + PF_GAP) + 18 + 44;
  m.y = (NV_VTELA_H - m.h) * 0.5f;
  ajCentraModal(&m);
  ajVeuModal();
  if (teclado_aberto()) return;   // a modal de PIN fica por cima; a folha so some atras
  ajIlha(m, 36, 1, 1);
  x = m.x + 44; y = m.y + 44;
  if (pfTela == PF_BLOQUEIO) { gfx_icone((GfxRect){ x, y - 2, 22, 22 }, "aj_shield", 0.961f, 0.784f, 0.298f, 1); txt_desenhar(ajTxt(TXT_G22M, kick), x + 34, y); }
  else txt_desenhar(ajTxt(TXT_G22M, kick), x, y);
  y += 22 + 8;
  { TxtLinha t = ajTxtC(TXT_ILHA_PERGUNTA, tit[0], m.w - 88); txt_desenhar(t, x, y); y += 44 + 10; }
  y += ajBloco(TXT_AJ_TEXTO, corpo, x, y, m.w - 88, 30, 0.62f, 4) + 22;
  for (i = 0; i < n; i++) {
    const PfLinha *l = &L[i];
    int foco = i == pfFoco;
    GfxRect r = { x, y, m.w - 88, PF_LH };
    float al = (l->id == PL_COPIAR_AGORA && !l->ativa) ? 0.45f : 1.0f, tx = r.x + 26;
    TxtLinha t;
    char rot[128];
    snprintf(rot, sizeof rot, "%s", l->id == PL_PERFIL ? l->rot : i18n(l->rot));
    if (foco) ajPilulaAcento(r, PF_LH * 0.5f, al); else ajNeutro(r, PF_LH * 0.5f, 0.07f, AJ_CHIP_SOL, al);
    if (pfTela == PF_COPIAR && l->id == PL_PERFIL) {
      GfxRect c = { tx, r.y + (PF_LH - 28) * 0.5f, 28, 28 };
      if (l->marca) { if (foco) ajIconeT("aj_circle-check", c.x, c.y, 28, 1); else ajIcone("aj_circle-check", c.x, c.y, 28, 1); }
      else ajNeutro(c, 14, 0.18f, 0.35f, 0.36f, 0.4f, 1);
      tx += 28 + 16;
    } else if (l->id == PL_PERFIL) {
      if (foco) ajIconeT("aj_user-round", tx, r.y + (PF_LH - 26) * 0.5f, 26, 1); else ajIcone("aj_user-round", tx, r.y + (PF_LH - 26) * 0.5f, 26, 0.85f);
      tx += 26 + 14;
    }
    t = foco ? ajTxtT(TXT_ILHA_ITEM, rot) : ajTxt(TXT_ILHA_ITEM, rot);
    txt_desenhar_alpha(t, tx, r.y + (PF_LH - t.h) * 0.5f, al);
    if (l->apoio[0]) {
      char ap[160];
      float lim = r.x + r.w - 26 - (tx + t.w + 24);
      TxtLinha a;
      snprintf(ap, sizeof ap, "%s", l->id == PL_PERFIL ? l->apoio : i18n(l->apoio));
      a = foco ? ajTxtT(TXT_V2_24, ap) : ajTxt(TXT_V2_24, ap);
      if (a.w <= lim) txt_desenhar_alpha(a, r.x + r.w - 26 - a.w, r.y + (PF_LH - a.h) * 0.5f, (foco ? 0.78f : 0.5f) * al);
    }
    y += PF_LH + PF_GAP;
  }
  y += 12;
  { AjDica d[2] = { { "OK", pfTela == PF_COPIAR ? "Marcar" : "Escolher" }, { "Voltar", pfTela == PF_BLOQUEIO ? "Sair dos ajustes" : "Fechar" } };
    aj2DicasLinha(d, 2, x, y + 20, TXT_G22M, 40, 44, 9, TXT_V2_26, 10, 28, 1, 1); }
  if (uxAviso[0]) {
    TxtLinha t = ajTxtC(TXT_AJ_SUB, uxAviso, 1100);
    txt_desenhar_alpha(t, (NV_VTELA_W - t.w) * 0.5f, NV_VTELA_H - 70, 0.8f);
  }
}

// Enquanto a edicao de outro perfil esta aberta, uma faixa no alto diz de quem
// sao os ajustes que se mexe.
static void pfFaixaEdicao(void) {
  char t[160];
  TxtLinha l;
  GfxRect r;
  int ed = ajustes_edicao_perfil();
  if (!ed) return;
  snprintf(t, sizeof t, i18n("Editando os ajustes de %s"), pfNome(ed));
  l = ajTxtT(TXT_V2_SEG, t);
  r = (GfxRect){ (NV_VTELA_W - (l.w + 112)) * 0.5f, 22, l.w + 112, 52 };
  ajPilulaAcento(r, 26, 0.96f);
  ajIconeT("aj_user-round", r.x + 24, r.y + 13, 26, 1);
  txt_desenhar(l, r.x + 24 + 26 + 14, r.y + (52 - l.h) * 0.5f);
}
