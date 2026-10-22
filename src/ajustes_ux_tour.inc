// O QUE O NUVIO FAZ? (2.1, M5): o tour por FUNCAO. Cada cartao mostra a previa
// animada da opcao (ajCenaGerada, so ilustra: nada de chave ou controle na
// imagem), duas frases e dois botoes: "Ligar agora" leva a opcao ja em edicao,
// "Ver onde fica" so poe o foco nela. Incluido por ajustes_ux_desenho.inc.
typedef struct { int op; const char *ic, *tit, *txt; int novo; } TourCartao;
#define TOUR_NOVO_DIAS 30
static const TourCartao TOUR[] = {
  { AJ_QUALIDADE, "aj_gauge", "Qualidade de vídeo", "Escolha até que resolução o Nuvio busca. Ele escolhe a melhor fonte dentro desse teto.", 0 },
  { AJ_FONTE_HDR, "aj_film", "Dolby Vision e HDR", "Fontes com HDR ou Dolby Vision vêm na frente. Funciona se o arquivo e a TV aceitarem.", 0 },
  { AJ_ATMOS, "aj_volume-2", "Som Dolby Atmos", "Fontes com Atmos passam na frente da fila. O som final depende da sua TV ou caixa.", 0 },
  { AJ_TAM_MAX, "aj_hard-drive", "Limite de tamanho", "Fontes maiores que o limite vão para o fim da fila. Bom para quem tem internet com franquia.", 0 },
  { AJ_FONTE_PRIORIDADE, "aj_zap", "Escolha da fonte", "Decida o que vem primeiro: equilíbrio, qualidade ou começar rápido. O Nuvio escolhe sozinho.", 0 },
  { AJ_LEG_LINGUA2, "aj_captions", "Segunda legenda", "Mostre duas legendas ao mesmo tempo, no alto da tela. Ótimo para estudar um idioma.", 1 },
#ifdef NV_ANDROID
  { AJ_LEG_SYNC_AUDIO, "aj_audio-lines", "Acertar a legenda", "O Nuvio compara as falas com a legenda e corrige o atraso. Vale só nesta TV.", 0 },
#endif
  { AJ_HOME_LAYOUT, "aj_layout-dashboard", "Layout da Home", "Moderna, Padrão ou Dinâmica, no estilo Apple TV. Troque quando quiser, sem perder nada.", 0 },
  { AJ_HERO_TRAILER, "aj_clapperboard", "Trailer no destaque", "Com o foco parado no destaque, o trailer toca no lugar da arte. Mexeu o foco, a arte volta.", 1 },
  { AJ_TEMA, "aj_palette", "Cor de destaque", "Troque a cor do botão em foco e das marcas de estado. O texto sempre fica legível.", 0 },
  { AJ_JF_LIGADO, "aj_server", "Seu servidor de mídia", "Veja na Home os filmes e séries do seu Jellyfin, Emby ou Plex. Ainda é experimental.", 1 },
  { AJ_XTREAM_SERVIDOR, "aj_audio-lines", "Canais de TV ao vivo", "Coloque o servidor que o provedor mandou. Os canais entram no Guia de TV com tudo preenchido.", 0 },
};
#define TOUR_N ((int)(sizeof TOUR / sizeof TOUR[0]))
// Geometria (tela virtual 1920x1080): 4 colunas x 2 linhas visiveis, rola por linha.
#define TOUR_COLS 4
#define TOUR_CW 438.0f
#define TOUR_CH 380.0f
#define TOUR_GAP 24.0f
#define TOUR_X0 48.0f
#define TOUR_Y0 230.0f

static int uxAbrirEdicao;   // "Ligar agora": a opcao de uxAbrirOp abre ja em edicao
static int tourPend, tourFoco, tourBtn, tourTopo;
static float tourRol, tourRolV, tourRolAlvo;
static long tourInicio;   // quando o tour abriu pela primeira vez (a janela do "Novo")

int tourN(void) { return TOUR_N; }
int tourOp(int i) { return TOUR[i].op; }
const char *tourTitulo(int i) { return i18n(TOUR[i].tit); }
const char *tourFrase(int i) { return i18n(TOUR[i].txt); }
int tourDisponivel(int i) { return ajOpcaoDisponivel(TOUR[i].op); }
int tourNovoJanela(long inicio, long agora) { return agora < inicio + (long)TOUR_NOVO_DIAS * 86400L; }
static int tourNovo(int i) { return TOUR[i].novo && tourNovoJanela(tourInicio, (long)time(NULL)); }
// Ligar agora / Ver onde fica: o foco vai para a opcao depois que o tour sai
// (tourAtualizar), com o editor aberto no primeiro caso.
static void tourIr(int i, int editar) {
  if (!tourDisponivel(i)) return;
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
  tourAberto = 1; tourFoco = 0; tourBtn = 0; tourTopo = 0;
  tourRol = tourRolV = tourRolAlvo = 0;
}
static void tourSair(void) { tourAberto = 0; focoIndice = 1; }   // volta ao cartao da grade

static void tourAtualizar(float dt) {
  if (tourPend && !tourAberto) {
    int op = uxAbrirOp;
    tourPend = 0; uxAbrirOp = -1;
    focarOpcao(op);
    if (uxAbrirEdicao) { uxAbrirEdicao = 0; uxAbrirEditor(op); }
    return;
  }
  if (!tourAberto) return;
  tourRol = ajustes_animacoes_reduzidas() ? tourRolAlvo : anim_mola2(&tourRolV, tourRol, tourRolAlvo, dt, 14.0f);
}

static void tourMirar(void) {
  int r = tourFoco / TOUR_COLS;
  if (r < tourTopo) tourTopo = r;
  else if (r > tourTopo + 1) tourTopo = r - 1;
  tourRolAlvo = tourTopo * (TOUR_CH + TOUR_GAP);
}
static void tourEvento(const SDL_Event *e) {
  SDL_Keycode k;
  int ok;
  if (!e || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  ok = k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE;
  if (guiaVoltar(k, e)) { tourSair(); return; }
  if (ok) { if (tourBtn == 0) tourLigar(tourFoco); else tourOnde(tourFoco); return; }
  if (k == SDLK_RIGHT) { if (tourBtn == 0) tourBtn = 1; else if (tourFoco < TOUR_N - 1) { tourFoco++; tourBtn = 0; } }
  else if (k == SDLK_LEFT) { if (tourBtn == 1) tourBtn = 0; else if (tourFoco > 0) { tourFoco--; tourBtn = 1; } }
  else if (k == SDLK_DOWN && tourFoco + TOUR_COLS < TOUR_N) tourFoco += TOUR_COLS;
  else if (k == SDLK_UP && tourFoco >= TOUR_COLS) tourFoco -= TOUR_COLS;
  tourMirar();
}
static void tourPontAcao(int i, int b) {
  if (!tourAberto || i < 0 || i >= TOUR_N) return;
  tourFoco = i; tourBtn = b;
  if (b == 0) tourLigar(i); else tourOnde(i);
}

// Botao do cartao (52 px): pilula do acento em foco, neutro fora.
static void tourBotao(const char *rot, GfxRect r, int foco, int ativo, float a) {
  TxtLinha t = foco ? ajTxtT(TXT_AJ_SEG, rot) : ajTxt(TXT_AJ_SEG, rot);
  if (!ativo) a *= 0.4f;
  if (foco) ajPilulaAcento(r, 26, a); else ajNeutro(r, 26, 0.08f, AJ_CHIP_SOL, a);
  txt_desenhar_alpha(t, r.x + (r.w - t.w) * 0.5f, r.y + (r.h - t.h) * 0.5f, (foco ? 1.0f : 0.88f) * a);
}
static void tourCartao(int i, float x, float y, int foco) {
  GfxRect c = { x, y, TOUR_CW, TOUR_CH }, pl = { x + 12, y + 12, TOUR_CW - 24, 156 };
  float cw = pl.h * 16.0f / 9.0f, bw = (TOUR_CW - 24 - 10) * 0.5f, ty = y + 12 + 156 + 12;
  int ativo = tourDisponivel(i);
  TxtLinha t = ajTxtC(TXT_AJ_NOME, TOUR[i].tit, TOUR_CW - 24 - 34);
  ajIlha(c, 26, 1, 0);
  if (foco) ajFocoLinha(c, 26, 1, 1);
  ajNeutro(pl, 16.0f, 0.06f, AJ_CHIP_SOL, 1);
  gfx_recorte(pl.x, pl.y, pl.w, pl.h);
  ajCenaGerada(uxSecaoDe(TOUR[i].op), TOUR[i].op, pl.x + (pl.w - cw) * 0.5f, pl.y, cw);
  gfx_sem_recorte();
  if (tourNovo(i)) {
    TxtLinha n = ajTxtT(TXT_AJ_MINI12, "Novo");
    float ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    gfx_cor((GfxRect){ pl.x + 10, pl.y + 10, n.w + 20, 26 }, 0.5f, ar, ag, ab, 1);
    txt_desenhar_alpha(n, pl.x + 20, pl.y + 10 + (26 - n.h) * 0.5f, 1);
  }
  ajIcone(TOUR[i].ic, x + 14, ty + 3, 22, 0.7f);
  txt_desenhar_alpha(t, x + 14 + 34, ty, 1);
  ajBloco(TXT_AJ_TEXTO, TOUR[i].txt, x + 14, ty + 38, TOUR_CW - 28, 25.5f, 0.62f, 3);
  { GfxRect b0 = { x + 12, y + TOUR_CH - 12 - 52, bw, 52 }, b1 = { x + 12 + bw + 10, b0.y, bw, 52 };
    tourBotao("Ligar agora", b0, foco && tourBtn == 0, ativo, 1);
    tourBotao("Ver onde fica", b1, foco && tourBtn == 1, ativo, 1);
    if (ativo) {
      ponteiro_alvo(b0.x, b0.y, b0.w, b0.h, NULL, tourPontAcao, i, 0);
      ponteiro_alvo(b1.x, b1.y, b1.w, b1.h, NULL, tourPontAcao, i, 1);
    } }
}
static void tourDesenhar(void) {
  GfxRect h = { 48, 112, 1824, 92 };
  TxtLinha t = ajTxt(TXT_AJ_TIT28, "O que o Nuvio faz?");
  AjDica d[3] = { { "OK", "Escolher" }, { "↑ ↓ ← →", "Navegar" }, { "Voltar", "Voltar" } };
  char q[48];
  int i;
  ajFundo();
  ajIlha(h, 26, 1, 0);
  ajNeutro((GfxRect){ h.x + 16, h.y + 16, 60, 60 }, 18, 0.08f, AJ_CHIP_SOL, 1);
  ajIcone("aj_sparkles", h.x + 31, h.y + 31, 30, 0.9f);
  ajKicker("Ajustes", h.x + 96, h.y + 17, 1);
  txt_desenhar_alpha(t, h.x + 96, h.y + 40, 1);
  snprintf(q, sizeof q, i18n("%d funções"), TOUR_N);
  { TxtLinha c = ajTxt(TXT_AJ_TEXTO, q);
    txt_desenhar_alpha(c, h.x + h.w - 32 - c.w, h.y + (h.h - c.h) * 0.5f, 0.55f); }
  gfx_recorte(0, TOUR_Y0 - 8, NV_VTELA_W, 2 * TOUR_CH + TOUR_GAP + 16);
  for (i = 0; i < TOUR_N; i++) {
    float y = TOUR_Y0 + (i / TOUR_COLS) * (TOUR_CH + TOUR_GAP) - tourRol;
    if (y + TOUR_CH < TOUR_Y0 - 8 || y > TOUR_Y0 + 2 * TOUR_CH + TOUR_GAP + 8) continue;
    tourCartao(i, TOUR_X0 + (i % TOUR_COLS) * (TOUR_CW + TOUR_GAP), y, i == tourFoco);
  }
  gfx_sem_recorte();
  ajDicasLinha(d, 3, 54, 1030, 1);
}
