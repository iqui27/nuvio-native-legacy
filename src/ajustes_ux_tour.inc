// Functional tour: settings routes reuse the v4 focus and draft editor.
typedef struct { int op; const char *tit, *txt; int novo; } TourCartao;
#define TOUR_NOVO_DIAS 30
static const TourCartao TOUR[] = {
#ifdef NV_ANDROID
  { AJ_LEG_SYNC_AUDIO, "Legendas que se acertam sozinhas", "Baixa e sincroniza a legenda pelo áudio, sem mexer no atraso.", 0 },
#else
  { AJ_LEG_SYNC_AUTO, "Legendas que se acertam sozinhas", "Baixa e sincroniza a legenda pelo áudio, sem mexer no atraso.", 0 },
#endif
  { AJ_LEG_LINGUA2, "Duas legendas ao mesmo tempo", "Para aprender idioma ou assistir com quem lê outra língua.", 1 },
  { AJ_SEEKR_LIGADO, "Miniatura ao avançar", "Veja a cena antes de soltar o botão. Precisa de uma chave grátis do Seekr.", 0 },
  { AJ_RELOGIO, "Avisos e Salvos no CH+ ou AZUL", "A pílula do canto guarda o episódio pela metade, estreias e enquetes.", 0 },
  { AJ_FONTE_PRIORIDADE, "Fonte escolhida do seu jeito", "Só os add-ons que você quer e um limite de tamanho para poupar internet.", 0 },
  { AJ_DTS_AC3, "Dolby Vision e DTS nesta TV", "Na LG: MKV em Dolby Vision e DTS convertido para 5.1. Na Samsung, veja o que dá.", 1 },
  { AJ_PERFIL_ATIVO, "Cada pessoa com o seu Nuvio", "Perfis com ajustes próprios, perfil infantil com trava e cópia de ajustes.", 0 },
  { AJ_XTREAM_SERVIDOR, "TV ao vivo com guia", "Canais do provedor, grade de programação e zapping com CH+ e CH−.", 1 },
  { AJ_PERFIL_PESQ, "Meu perfil público", "Amigos do Nuvio além do Trakt. Perfil público só se você ligar.", 0 },
  { AJ_TAM_MAX, "Economizar dados", "Fontes maiores que o limite vão para o fim da fila. Bom para quem tem internet com franquia.", 0 },
  { AJ_ESMAECER, "Tela de descanso", "Vitrine do seu catálogo, Relógio com a próxima estreia, ou só escurecer. Nunca entra com algo tocando.", 0 },
  { -1, "Assistir Juntos", "Escolha um título na Biblioteca. Abra Assistir Juntos na página do título.", 1 },
  { AJ_DV_MKV, "Dolby Vision em MKV nas TVs LG (experimental)", "Deixa tocar fontes em Dolby Vision quando a TV aceita.", 0 },
  { AJ_JF_LIGADO, "Seu servidor de mídia", "Veja na Home os filmes e séries do seu Jellyfin, Emby ou Plex. Ainda é experimental.", 0 },
  { AJ_QUALIDADE, "Qualidade de vídeo", "Escolha até que resolução o Nuvio busca. Ele escolhe a melhor fonte dentro desse teto.", 0 },
  { AJ_ATMOS, "Som Dolby Atmos", "Fontes com Atmos passam na frente da fila. O som final depende da sua TV ou caixa.", 0 },
  { AJ_HOME_LAYOUT, "Layout da Home", "Moderna, Padrão ou Dinâmica, no estilo Apple TV. Troque quando quiser, sem perder nada.", 0 },
  { AJ_HERO_TRAILER, "Trailer no destaque", "Com o foco parado no destaque, o trailer toca no lugar da arte. Mexeu o foco, a arte volta.", 0 },
  { AJ_TEMA, "Cor de destaque", "Troque a cor do botão em foco e das marcas de estado. O texto sempre fica legível.", 0 },
};
#define TOUR_N ((int)(sizeof TOUR / sizeof TOUR[0]))
#define TOUR_COLS 4
#define TOUR_CW 432.0f
#define TOUR_CH 382.0f
#define TOUR_GAP 24.0f
#define TOUR_X0 48.0f
#define TOUR_Y0 230.0f

static int uxAbrirEdicao;
static int tourFoco, tourBtn, tourTopo;
static float tourRol, tourRolV, tourRolAlvo;
// Springs: page visibility (enter and the retained exit over v4), each card's
// focus and the focus sliding between the two pills of the focused card.
static float tourVis, tourBtnA, tourFocoA[TOUR_N];
static long tourInicio;

int tourN(void) { return TOUR_N; }
int tourDisponivel(int i);
static int tourLigada(int i) {
  int op = TOUR[i].op;
  if (!tourDisponivel(i)) return 0;
  switch (op) {
    case AJ_LEG_LINGUA2: return ling_legenda2()[0] && strcmp(ling_legenda2(), "none") != 0;
    case AJ_ESMAECER: case AJ_TAM_MAX: return uxValorAtual(op) > 0;
    case AJ_DTS_AC3:
#ifdef NV_TPK
      return ajustes_dts_tpk_modo() != DTSTPK_DESLIGADO;
#else
      return ajustes_dts_ac3();
#endif
    case AJ_PERFIL_ATIVO: return perfis_n() > 1;
    case AJ_PERFIL_PESQ: return recomenda_pesquisavel();
    case AJ_XTREAM_SERVIDOR: {
      IptvConta contas[IPTV_MAX_CONTAS];
      int n = iptv_contas(contas, IPTV_MAX_CONTAS);
      for (int j = 0; j < n; j++) if (contas[j].tipo == IPTV_XTREAM && contas[j].ativa) return 1;
      return 0;
    }
    case AJ_FONTE_PRIORIDADE: return ajustes_fonte_prioridade() != 3;
    default: return op < 0 || OPCOES[op].valores != V_LIGA || lig(op);
  }
}
static int tourLigadas(void) {
  int n = 0;
  for (int i = 0; i < TOUR_N; i++) n += tourLigada(i);
  return n;
}
int tourOp(int i) { return TOUR[i].op; }
const char *tourTitulo(int i) { return i18n(TOUR[i].tit); }
const char *tourFrase(int i) { return i18n(TOUR[i].txt); }
int tourDisponivel(int i) {
  return i >= 0 && i < TOUR_N && (TOUR[i].op < 0 || aj4Disponivel(TOUR[i].op == AJ_XTREAM_SERVIDOR ? AJ_IPTV_LISTA_XTREAM : TOUR[i].op));
}
int tourNovoJanela(long inicio, long agora) {
  return inicio > 0 && agora >= inicio && agora - inicio < (long)TOUR_NOVO_DIAS * 86400L;
}
static int tourNovo(int i) { return TOUR[i].novo && tourNovoJanela(tourInicio, (long)time(NULL)); }
static void tourSair(void);
static void tourIr(int i, int editar) {
  if (!tourDisponivel(i)) return;
  if (TOUR[i].op < 0) { pediuTela = GT_BIBLIOTECA; tourSair(); return; }
  uxAbrirOp = TOUR[i].op; uxAbrirEdicao = editar;
  tourAberto = 0; tourPend = 1;
}
void tourLigar(int i) { tourIr(i, 1); }
void tourOnde(int i) { tourIr(i, 0); }

static void tourAbrir(void) {
  char *s = dados_ler("tour-visto.txt");
  tourInicio = s ? atol(s) : 0;
  free(s);
  if (tourInicio <= 0) {
    char b[24];
    tourInicio = (long)time(NULL);
    snprintf(b, sizeof b, "%ld", tourInicio);
    dados_gravar("tour-visto.txt", b);
  }
  uxCancelar();
  tourPend = uxAbrirEdicao = 0;
  tourAberto = 1; tourFoco = 0; tourBtn = 0; tourTopo = 0;
  tourRol = tourRolV = tourRolAlvo = 0;
  tourVis = tourBtnA = 0;
  memset(tourFocoA, 0, sizeof tourFocoA);
}
static void tourSair(void) { tourAberto = 0; focoIndice = 1; aj4Linha = 7; aj4Topo = -1; }

static void tourAtualizar(float dt) {
  tourVis = anim_mola(tourVis, tourAberto ? 1.0f : 0.0f, dt, 14.0f);
  if (!tourAberto && tourVis < 0.01f) tourVis = 0;
  if (tourPend && !tourAberto) {
    int op = uxAbrirOp == AJ_XTREAM_SERVIDOR ? AJ_IPTV_LISTA_XTREAM : uxAbrirOp, editar = uxAbrirEdicao;
    tourPend = uxAbrirEdicao = 0; uxAbrirOp = -1;
    aj4Util = aj4Review = 0; uxVeioBusca = 0;
    focarOpcao(op);
    if (editar) {
      if (op == AJ_PERFIL_ATIVO) aj4AbrirPerfil();
      else uxAbrirEditor(op);
    }
    return;
  }
  if (!tourAberto) return;
  tourRol = ajustes_animacoes_reduzidas() ? tourRolAlvo : anim_mola2(&tourRolV, tourRol, tourRolAlvo, dt, 14.0f);
  tourBtnA = anim_mola(tourBtnA, (float)tourBtn, dt, 18.0f);
  for (int i = 0; i < TOUR_N; i++) tourFocoA[i] = anim_mola(tourFocoA[i], i == tourFoco, dt, 18.0f);
}
static void tourMirar(void) {
  int r = tourFoco / TOUR_COLS;
  if (r < tourTopo) tourTopo = r;
  else if (r > tourTopo + 1) tourTopo = r - 1;
  tourRolAlvo = tourTopo * (TOUR_CH + TOUR_GAP);
}
static void tourEvento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!e || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (guiaVoltar(k, e)) { tourSair(); return; }
  if (uxOk(k)) { if (tourBtn == 0) tourLigar(tourFoco); else tourOnde(tourFoco); return; }
  if (k == SDLK_RIGHT) { if (tourBtn == 0) tourBtn = 1; else if (tourFoco < TOUR_N - 1) { tourFoco++; tourBtn = 0; } }
  else if (k == SDLK_LEFT) { if (tourBtn == 1) tourBtn = 0; else if (tourFoco > 0) { tourFoco--; tourBtn = 1; } }
  else if (k == SDLK_DOWN && tourFoco + TOUR_COLS < TOUR_N) tourFoco += TOUR_COLS;
  else if (k == SDLK_UP && tourFoco >= TOUR_COLS) tourFoco -= TOUR_COLS;
  tourMirar();
}
static void tourPontFoco(int i, int b) {
  if (!tourAberto || i < 0 || i >= TOUR_N) return;
  tourFoco = i; tourBtn = b;
}
static void tourPontAcao(int i, int b) {
  tourPontFoco(i, b);
  if (b == 0) tourLigar(i); else tourOnde(i);
}

static void tourLegenda(const char *s, float y, int secundaria) {
  float px = secundaria ? 27 : 30;
  float w = fminf(ajcLargura(s, px, 1), 580);
  if (!secundaria) ajc_ret(320 - w * .5f - 24, y - 8, w + 48, 54, 10, 0, 0, 0, .85f);
  ajTextoMini(s, ajc_px(px), 1, ajc_x(320 - w * .5f), ajc_y(y),
              243, secundaria ? 216 : 242, secundaria ? 121 : 239, C.a);
}

// The same generated-scene kit, without switches or simulated setting controls.
// Only the focused illustration moves; the other seven remain still.
static void tourPrevia(int op, GfxRect r, int foco, float a) {
  gfx_recorte(r.x, r.y, r.w, r.h);
  ajc_comecar(r.x, r.y, r.w, a);
  gfx_recorte(r.x, r.y, r.w, r.h);
  if (!foco) { C.red = 1; C.t = 0; }
  switch (op) {
    case AJ_LEG_SYNC_AUTO: case AJ_LEG_SYNC_AUDIO: case AJ_LEG_LINGUA2:
      tourLegenda(i18n(op == AJ_LEG_LINGUA2 ? "Eu disse que ia voltar." : "Legenda no tempo certo"), 190, 0);
      if (op == AJ_LEG_LINGUA2) tourLegenda("I told you I'd come back.", 120, 1);
      else {
        ajc_barra(33, 33, 163, .4f);
        ajc_progresso(33, 136, 574, .4f + .15f * ajc_onda(3), 1);
      }
      break;
    case AJ_SEEKR_LIGADO:
      ajc_ret(207, 42, 222, 124, 15, .95f, .95f, .93f, 1);
      ajc_video(211, 46, 214, 116, 11, 1);
      ajc_progresso(33, 199, 574, .4f + .15f * ajc_onda(3), 1);
      break;
    case AJ_RELOGIO:
      ajc_vidro(420, 20, 190, 65, 30, 1);
      ajcTexto("21:42", 32, 1, 448, 34, 1);
      ajc_vidro(220, 110, 390, 120, 24, 1);
      ajc_icone("aj_bell", 270, 170, 42, .8f);
      ajc_barra(320, 150, 200, .8f); ajc_barra(320, 185, 240, .4f);
      break;
    case AJ_DTS_AC3: case AJ_DV_MKV:
      ajc_neutro(33, 98, 200, 44, 9, .18f);
      ajcTexto("Dolby Vision", 28, 1, 48, 101, 1);
      if (op == AJ_DTS_AC3) {
        ajc_neutro(245, 98, 155, 44, 9, .18f);
        ajcTexto("DTS → 5.1", 28, 1, 260, 101, 1);
      }
      ajc_barra(45, 175, 320, .6f); ajc_barra(45, 210, 190, .3f);
      break;
    case AJ_PERFIL_ATIVO: case AJ_PERFIL_PESQ:
      for (int j = 0; j < 3; j++) {
        float cr, cg, cb; ajc_matiz(j * 2 + 1, &cr, &cg, &cb);
        ajc_bolinha(130 + j * 180, 120, 96, cr, cg, cb, 1);
        ajc_icone("aj_user-round", 130 + j * 180, 120, 44, .8f);
      }
      break;
    case AJ_XTREAM_SERVIDOR:
      for (int j = 0; j < 8; j++) {
        ajc_neutro(35 + j % 4 * 145, 45 + j / 4 * 75, 130, 60, 9, .8f);
        if (j == ajc_passo(2, 8)) ajc_barra(50 + j % 4 * 145, 70 + j / 4 * 75, 90, 1);
      }
      break;
    case AJ_FONTE_PRIORIDADE: case AJ_TAM_MAX:
      for (int j = 0; j < 3; j++) {
        ajc_cartaz(45 + j * 195, 35, 160, 175, j * 2 + 1, 1);
        if (j == (op == AJ_TAM_MAX ? 0 : ajc_passo(2, 3))) ajc_icone("aj_check", 125 + j * 195, 235, 34, 1);
      }
      break;
    case -1:
      ajcTexto(i18n("Página do título"), 32, 1, 160, 5, 1);
      ajcTexto(i18n("Convide um amigo"), 32, 0, 40, 220, 1);
      ajc_tv(30, 50, 240, 130, 1); ajc_cartaz(36, 56, 228, 118, 2, 1);
      ajc_tv(370, 50, 240, 130, 1); ajc_cartaz(376, 56, 228, 118, 2, 1);
      ajc_progresso(40, 195, 220, .4f + .15f * ajc_onda(4), 1);
      ajc_progresso(380, 195, 220, .4f + .15f * ajc_onda(4), 1);
      break;
    case AJ_ESMAECER:
      ajc_cartaz(80 + 30 * ajc_onda(5), 15, 480, 240, 4, .65f);
      ajcTexto("21:42", 38, 1, 260, 155, 1);
      break;
    case AJ_JF_LIGADO: ajcCenaServico(op, "aj_server"); break;
    case AJ_QUALIDADE:
      ajc_sub(88, 0, .72f);
      ajcCenaQualidade(op);
      ajc_sub_fim();
      break;
    case AJ_ATMOS:
      ajcTexto("Dolby Atmos", 36, 1, 200, 30, 1);
      for (int j = 0; j < 7; j++) {
        float h = 40 + 70 * fabsf(sinf(j + ajc_t() * 2));
        ajc_neutro(155 + j * 45, 190 - h, 24, h, 8, .9f);
      }
      break;
    default:
      { AjcHome h = ajcHomePadrao(); ajcHome(&h); }
      break;
  }
  ajc_fim();
}
// Shared Detail card material with the shared row focus surface on top (the
// spring fades it in); the preview scene covers the top, text stays light.
static void tourCartao(int i, float x, float y, float clipY, float a) {
  GfxRect c = { x, y, TOUR_CW, TOUR_CH }, pl = { x, y, TOUR_CW, 176 };
  int ativo = tourDisponivel(i);
  float f = tourFocoA[i], al = ativo ? a : .4f * a;
  gfx_recorte(0, clipY, NV_VTELA_W, 2 * TOUR_CH + TOUR_GAP);
  plrui_cartao(c, 16, a);
  plrui_linha_foco(c, 16, f * a);
  tourPrevia(TOUR[i].op, pl, i == tourFoco, a);
  gfx_recorte(0, clipY, NV_VTELA_W, 2 * TOUR_CH + TOUR_GAP);
  if (tourNovo(i)) {
    float ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    char up[32]; idioma_maiusc(up, sizeof up, i18n("NOVO"));
    TxtLinha n = txt_linha_corta(TXT_ILHA_HORA, up, (int)(ar * 255), (int)(ag * 255), (int)(ab * 255), 255, 200);
    GfxRect nr = { x + 14, y + 14, n.w + 24, 28 };
    plrui_botao_repouso(nr, a);
    txt_desenhar_alpha(n, nr.x + 12, nr.y + (nr.h - n.h) * .5f, a);
  }
  txt_bloco_corta(TXT_ILHA_NOME, tourTitulo(i), 243, 242, 239, x + 22, y + 190, TOUR_CW - 44, 28, a, 2);
  txt_bloco_corta(TXT_ILHA_TEXTO, tourFrase(i), 243, 242, 239, x + 22, y + 248, TOUR_CW - 44, 22, (.62f + .13f * f) * a, 3);
  { const char *rot = TOUR[i].op < 0 ? "Escolher" : TOUR[i].op == AJ_PERFIL_ATIVO ? "Criar perfil" :
      TOUR[i].op == AJ_XTREAM_SERVIDOR ? "Conectar" : TOUR[i].op == AJ_FONTE_PRIORIDADE ? "Configurar" :
      TOUR[i].op == AJ_RELOGIO ? "Ver como" : "Ligar agora";
    float bw = fminf(botao_largura(rot, NULL, 1), (TOUR_CW - 56) * .5f);
    float bw1 = fminf(botao_largura("Onde fica", NULL, 0), TOUR_CW - 56 - bw);
    GfxRect b0 = { x + 22, y + 320, bw, 44 }, b1 = { x + 34 + bw, y + 320, bw1, 44 };
    botao_pilula(b0, rot, NULL, f * (1 - tourBtnA), 1, 0, al);
    botao_pilula(b1, "Onde fica", NULL, f * tourBtnA, 0, 0, al);
    if (ativo && tourAberto) {
      ponteiro_alvo(b0.x, b0.y, b0.w, b0.h, tourPontFoco, tourPontAcao, i, 0);
      ponteiro_alvo(b1.x, b1.y, b1.w, b1.h, tourPontFoco, tourPontAcao, i, 1);
    }
  }
}
// tourVis < 1: entering or the retained exit, drawn over the v4 page.
static void tourDesenhar(void) {
  float a = tourVis, dy = (1 - a) * 28;
  GfxRect h = { 48, 112 + dy, 1824, 92 };
  TxtLinha t = ajTxt(TXT_ILHA_TITULO, "O que o Nuvio faz?");
  char q[96], mais[96];
  snprintf(mais, sizeof mais, i18n("Mais %d funções"), TOUR_N > 2 * TOUR_COLS ? TOUR_N - 2 * TOUR_COLS : 0);
  AjDica d[3] = { { "OK", "Ligar agora" }, { "→", mais }, { "Voltar", "Ajustes" } };
  int novos = 0;
  // The page's own backdrop dissolves in with it, so v4 text never shows through the cards.
  fundo_desenhar_modo(FUNDO_FROST, (GfxRect){ 0, 0, NV_VTELA_W, NV_VTELA_H }, 0.0f, ajFundoArte(), a);
  ajIlha(h, 26, a, 0);
  plrui_material_plano((GfxRect){ h.x + 22, h.y + 16, 60, 60 }, 14, a);
  gfx_icone((GfxRect){ h.x + 36, h.y + 30, 32, 32 }, "aj_sparkles", .95f, .95f, .93f, a);
  plrui_kicker("Ajustes", h.x + 106, h.y + 14, 158, 158, 158, a);
  txt_desenhar_alpha(t, h.x + 106, h.y + 32, a);
  for (int i = 0; i < TOUR_N; i++) novos += tourNovo(i);
  snprintf(q, sizeof q, i18n("%d de %d funções ligadas"), tourLigadas(), TOUR_N);
  { TxtLinha c = ajTxt(TXT_ILHA_CORPO, q); txt_desenhar_alpha(c, h.x + h.w - 30 - c.w, h.y + 22, .9f * a); }
  snprintf(q, sizeof q, i18n("%d novas na 2.0.2"), novos);
  { TxtLinha c = ajTxt(TXT_ILHA_TEXTO, q); txt_desenhar_alpha(c, h.x + h.w - 30 - c.w, h.y + 56, .55f * a); }
  gfx_recorte(0, TOUR_Y0 + dy - 2, NV_VTELA_W, 2 * TOUR_CH + TOUR_GAP + 4);
  for (int i = 0; i < TOUR_N; i++) {
    float y = TOUR_Y0 + dy + (i / TOUR_COLS) * (TOUR_CH + TOUR_GAP) - tourRol;
    if (y + TOUR_CH < TOUR_Y0 + dy || y > TOUR_Y0 + dy + 2 * TOUR_CH + TOUR_GAP) continue;
    tourCartao(i, TOUR_X0 + (i % TOUR_COLS) * (TOUR_CW + TOUR_GAP), y, TOUR_Y0 + dy, a);
  }
  gfx_sem_recorte();
  ajDicasLinha(d, 3, NV_VTELA_W - 48 - ajDicasLargura(d, 3), 1030, a);
}
