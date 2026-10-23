// AJUSTES V2 — O MENU GRANDE, O RESUMO E A PILHA QUE DESLIZA (ajustes-v2.html,
// inventory-v2.md, aprovado pelo dono em 03/10/2026). Incluido por
// ajustes_ux_desenho.inc, depois das previas e antes da ilha (que desenha a
// lista, o inspetor e os editores com estas pecas).
//
// TRES DEGRAUS: 0 = o menu de categorias sozinho, com o resumo da categoria a
// direita; 1 = OK numa categoria: a lista cresce por cima do menu (que recua
// para a coluna de icones) e o resumo vira o inspetor; 2 = OK numa opcao que
// abre editor: o editor entra do mesmo jeito e a lista recua para a faixa.
// Cada degrau e uma mola de segunda ordem (sem quique) e rampas de alfa; o
// Voltar desfaz com a mola mais rapida. "Animacoes: Reduzidas" = sem deslize
// nem escala, so o fade de 120 ms.

// --- MOLA COM AMORTECIMENTO -----------------------------------------------------------
// A anim_mola2 e criticamente amortecida (zeta 1); a tabela do mockup pede
// zeta 0,86 e 0,9 para a lista e o inspetor. Passos fixos de 1/240 s (semi-
// implicito), estaveis para qualquer dt.
static float aj2Mola(float *v, float x, float alvo, float dt, float w, float z) {
  if (anim_politica_reduzida || ajustes_animacoes_reduzidas()) { *v = 0.0f; return alvo; }
  if (dt > 0.05f) dt = 0.05f;
  while (dt > 0.0f) {
    float h = dt > (1.0f / 240.0f) ? (1.0f / 240.0f) : dt;
    *v += (-2.0f * z * w * *v - w * w * (x - alvo)) * h;
    x += *v * h;
    dt -= h;
  }
  if (fabsf(x - alvo) < 0.0005f && fabsf(*v) < 0.005f) { x = alvo; *v = 0.0f; }
  return x;
}
static float aj2Rampa(float p, float alvo, float dt, float ms) {
  float passo = dt * (1000.0f / (ajustes_animacoes_reduzidas() ? 120.0f : ms));
  if (alvo > p) { p += passo; if (p > alvo) p = alvo; }
  else          { p -= passo; if (p < alvo) p = alvo; }
  return p;
}

// --- O ESTADO DA PILHA ----------------------------------------------------------------
static float aj2V1, aj2N2, aj2V2;     // degraus 1 e 2 (molas); aj2N1 mora na geometria (desenho.inc)
static float aj2Rot = 1.0f;                  // rotulos do menu (some em 120 ms)
static float aj2Miolo = 1.0f;                // ilha do menu: 1, 0,42, 0,16
static float aj2ListaA, aj2EdA, aj2ResumoA = 1.0f;
static float aj2Insp, aj2VI, aj2InspEspera;  // o inspetor chega 60 ms depois
// Pose: o que o desenho aplica. ajustes_teste_pose fixa uma (o quadro do meio
// da transicao do mockup).
typedef struct {
  float mS, mDx, mA, mRot;             // menu: escala, deslize, alfa, rotulos
  float lS, lDx, lOy, lA, lTxt;        // lista: escala, deslize, origem y, alfa, texto
  float iDx, iA;                       // inspetor
  float rA;                            // resumo (so no degrau 0)
  float eS, eDx, eA;                   // editor
} Aj2Pose;
#ifdef AJUSTES_TESTE
static int aj2PoseFixa;
static Aj2Pose aj2PoseTeste;
#endif
static int aj2EditorPainel(void);
static int aj2Degrau(void) {
  if (focoIndice || uxIndice == 0) return 0;
  return aj2EditorPainel() ? 2 : 1;
}

// --- O MENU ---------------------------------------------------------------------------
// menuV2: ilha de 660 em x 48, y 112, raio 40, padding 18; linhas de 76 (gap
// 4), rotulo 36/500 (600 na ativa), icone 40; a linha em foco abre a
// descricao (24, linha 1,35, ate 2 linhas): padding 18 28 20. Fio de 1 px com
// margem 8 24 depois de "Diferentes do padrao". Esvanece nos 70 px de baixo.
#define AJ2_MENU_PAD   18.0f
// Medidas do indice (04/10): a linha, o vao entre linhas e o respiro entre grupos
// se ajustam a altura da ilha (aj2MenuMedir) — a 80% tudo cabe sem rolar; a 100% ou
// mais ficam nas medidas cheias e a lista rola.
#define AJ2_MENU_LN    aj2Ln
#define AJ2_MENU_GAP   aj2Gap
#define AJ2_MENU_CHIP  62.0f   // as tres pilulas do alto (Buscar, Diferentes do padrao, Avancadas); era 56
#define AJ2_MENU_CHIP_GAP 8.0f // vao entre as fileiras de pilulas, quando elas quebram
#define AJ2_MENU_TOPOL aj2ChipsTopo()   // onde comeca a lista: pilulas + 10 + fio 1 + 10
#define AJ2_MENU_GT    30.0f   // titulo de grupo (Telas, Assistir, Conta e sistema)
#define AJ2_MENU_GT_EX aj2GtEx // respiro extra antes do 2o e do 3o grupo
#define AJ2_MENU_FADE  aj2Fade
#define AJ2_MENU_ICO   34.0f
#define AJ2_MENU_ICO_GAP 18.0f
#define AJ2_MENU_RESERVA 80.0f // o que a linha aberta (descricao de 2 linhas) soma a uma fechada
#define AJ2_NMENU (AJ_MAX_SECOES + 2)
static float aj2MH[AJ2_NMENU], aj2MV[AJ2_NMENU], aj2MDesc[AJ2_NMENU], aj2MTempo[AJ2_NMENU];
static float aj2MRol, aj2MRolV, aj2SupY, aj2SupYv, aj2SupH, aj2SupHv;
static int aj2MIni;
static float aj2Ln = 58.0f, aj2Gap = 3.0f, aj2GtEx = 18.0f, aj2Fade = 70.0f;

// --- AS TRES PILULAS DO ALTO ----------------------------------------------------------
// Buscar, Diferentes do padrao e Avancadas. O ROTULO NUNCA SOME: a largura de
// cada uma vem do texto traduzido, medido na fonte da vez (a Montserrat da TV
// e mais larga que a Inter do Mac), e o arranjo se adapta nesta ordem:
//   1. as tres numa fileira, se cabem;
//   2. Buscar sozinha em cima e as duas de filtro embaixo;
//   3. Buscar + Diferentes em cima e Avancadas embaixo;
//   4. uma por fileira.
// Se nem sozinha a pilula cabe na largura da ilha, o corpo do texto desce um
// degrau (28 > 26 > 25) e, por ultimo, o rotulo e cortado com reticencias —
// cortado, nunca escondido. Com mais de uma fileira, as pilulas de cada
// fileira se esticam ate a largura da ilha (sem "dente" a direita).
// Antes (ate 05/10): fileira unica, e a terceira virava so o icone quando nao
// cabia — em ingles com Montserrat nao cabia nunca (captura do dono).
// Cada pilula tem a sua cor, tirada dos acentos do app (TEMA_ACENTO: Oceano,
// Ambar, Violeta): o `fill` tinge o fundo e a `marca` (a versao clara do
// matiz, feita para o escuro) pinta o icone. Nao dependem do acento escolhido.
#define AJ2_CHIP_ICO 28.0f
static const unsigned char AJ2_CHIP_TEMA[4] = { 2, 5, 3, 4 };   // Oceano, Ambar, Violeta, e o do perfil (2.1)
static struct {
  float x[3], w[3];        // posicao (relativa ao miolo da ilha) e largura abertas
  int lin[3], linhas;      // a fileira de cada uma
  TxtEstilo e;
  int idioma, fonte, pronto;
} aj2Chips = { { 0 }, { 0 }, { 0, 0, 0 }, 1, TXT_V2_SEG, -1, -1, 0 };
static const char *aj2ChipRotulo(int c) {
  return i18n(c == 0 ? "Buscar" : c == 1 ? "Diferentes do padrão" : "Avançadas");
}
static float aj2ChipNatural(int c, TxtEstilo e) {
  return 20.0f + AJ2_CHIP_ICO + 10.0f + (float)txt_largura(e, aj2ChipRotulo(c)) + 22.0f;
}
static void aj2ChipsMedir(void) {
  static const TxtEstilo E[3] = { TXT_ILHA_NOME_L, TXT_V2_LN_B, TXT_V2_SEG };
  static const signed char ARR[4][3] = { { 0, 0, 0 }, { 0, 1, 1 }, { 0, 0, 1 }, { 0, 1, 2 } };
  const float livre = AJ2_MENU_W - 2 * AJ2_MENU_PAD, vao = 10.0f;
  float nat[3];
  int k, a, c, achou = 0;
  if (aj2Chips.pronto && aj2Chips.idioma == ajustes_idioma() && aj2Chips.fonte == valor[AJ_FONTE_UI]) return;
  for (k = 0; k < 3 && !achou; k++) {
    for (c = 0; c < 3; c++) nat[c] = aj2ChipNatural(c, E[k]);
    for (a = 0; a < 4 && !achou; a++) {
      float soma[3] = { 0, 0, 0 };
      int n[3] = { 0, 0, 0 }, cabe = 1, l;
      // No corpo grande so valem os arranjos de ate duas fileiras; tres fileiras
      // e o ultimo recurso, ja no corpo menor.
      if (a == 3 && k < 2) continue;
      for (c = 0; c < 3; c++) { l = ARR[a][c]; soma[l] += nat[c] + (n[l] ? vao : 0); n[l]++; }
      for (l = 0; l < 3; l++) if (soma[l] > livre + 0.5f) cabe = 0;
      if (!cabe && !(a == 3 && k == 2)) continue;
      aj2Chips.e = E[k]; aj2Chips.linhas = ARR[a][2] + 1;
      for (l = 0; l < 3; l++) {
        float x = 0, folga = livre - soma[l];
        for (c = 0; c < 3; c++) {
          float w;
          if (ARR[a][c] != l) continue;
          // Estica so quando quebrou (fileira unica fica a esquerda, como era);
          // a que nao cabe nem sozinha fica na largura da ilha e corta o texto.
          w = nat[c] + (aj2Chips.linhas > 1 && n[l] ? folga / (float)n[l] : 0.0f);
          if (w > livre) w = livre;
          aj2Chips.lin[c] = l; aj2Chips.x[c] = x; aj2Chips.w[c] = w;
          x += w + vao;
        }
      }
      achou = 1;
    }
  }
  for (c = 0; c < 3; c++) uxChipLinha[c] = aj2Chips.lin[c];
  aj2Chips.idioma = ajustes_idioma(); aj2Chips.fonte = valor[AJ_FONTE_UI]; aj2Chips.pronto = 1;
}
// Altura do cabecalho do indice (as pilulas e o fio), que empurra a lista.
static float aj2ChipsTopo(void) {
  aj2ChipsMedir();
  return (float)aj2Chips.linhas * AJ2_MENU_CHIP + (float)(aj2Chips.linhas - 1) * AJ2_MENU_CHIP_GAP + 21.0f;
}
// f = 1: medidas cheias (58 / 3 / 18); f = 0: as compactas (50 / 2 / 10);
// f = -0,35: o piso (47 / 1,65 / 7).
// R6 (04/10): o indice estava "enorme" — linha, icone e corpo menores.
static float aj2MenuNecessario(float f, int nLin) {
  float ln = 50.0f + 8.0f * f, gap = 2.0f + f, ex = 10.0f + 8.0f * f;
  return AJ2_MENU_TOPOL + 3.0f * AJ2_MENU_GT + 2.0f * ex + (float)nLin * (ln + gap) + AJ2_MENU_RESERVA;
}
static void aj2MenuMedir(void) {
  GfxRect m = aj2MenuR();
  float janela = m.h - 2 * AJ2_MENU_PAD, f = 1.0f, n0, n1;
  int nLin = nSecoes > 0 ? nSecoes : 11;
  n0 = aj2MenuNecessario(0.0f, nLin); n1 = aj2MenuNecessario(1.0f, nLin);
  aj2Fade = 70.0f;
  // Com as pilulas em duas fileiras o cabecalho cresceu ~76: a linha pode
  // descer ate 47 (f = -0,35) antes de a lista passar a rolar — a 100% o
  // indice continua inteiro na tela, com as pilulas sempre a vista.
  if (n1 <= janela) aj2Fade = 0.0f;
  else if (n1 > n0 && (janela - n0) / (n1 - n0) >= -0.35f) { f = (janela - n0) / (n1 - n0); aj2Fade = 0.0f; }
  aj2Ln = 50.0f + 8.0f * f; aj2Gap = 2.0f + f; aj2GtEx = 10.0f + 8.0f * f;
}
static const char *aj2MenuTitulo(int j) {
  return j == 0 ? "Buscar ajuste" : j == 1 ? "Diferentes do padrão" : TELA[secIni[j - 2]].titulo;
}
static const char *aj2MenuIcone(int j) {
  return j == 0 ? "aj_search" : j == 1 ? "aj_rotate-ccw-clock" : TELA[secIni[j - 2]].icone;
}
static const char *aj2MenuDesc(int j) {
  return j == 0 ? "Pelo nome ou pelo que quer mudar"
       : j == 1 ? "Valores fora da configuração de fábrica" : TELA[secIni[j - 2]].sub;
}
// Altura da linha aberta: 18 + 42 + 6 + linhas x 32,4 + 20.
static float aj2MenuAberta(int j) {
  float w = AJ2_MENU_W - 2 * AJ2_MENU_PAD - 2 * 28.0f - AJ2_MENU_ICO - AJ2_MENU_ICO_GAP;
  float h = txt_bloco(TXT_V2_26, aj2MenuDesc(j), 0, 0, 0, 0, -4000, w, 32.4f, 0, 2);
  if (h < 32.4f) h = 32.4f;
  return AJ2_MENU_LN - 2.0f + h + 14.0f;
}
static int aj2MenuFocado(void) { return focoIndice ? uxIndice : -1; }
// Os tres grupos do indice: so a apresentacao. As categorias seguem a ordem de
// TELA[] (ajustes_ux_tela.inc, 2.0.3: Assistir = 0-3, Telas = 4-7, o resto em
// Conta e sistema); nada aqui mexe em valor[] nem em CHAVE[].
static int aj2Grupo(int s) { return s < AJS_HOME ? 0 : s < AJS_CONTAS ? 1 : 2; }
static int aj2GrupoDe(int s) { return aj2Grupo(s); }
static int aj2GrupoIni(int s) { return s == 0 || aj2Grupo(s) != aj2Grupo(s - 1); }
static const char *aj2GrupoNome(int g) { return g == 0 ? "Assistir" : g == 1 ? "Telas" : "Conta e sistema"; }
// O VALOR VIVO DE CADA CATEGORIA no indice (2.0.2), como o Ajustes de um
// celular: o que a pessoa mais quer saber dela sem abrir ("Moderna",
// "Automatica", "Portugues", "3 de 5"). So valores que a tela ja mostra nas
// linhas, ja traduzidos; nada de conta ou servidor (TV ao vivo fica vazia).
static void aj2MenuValor(int sec, char *b, size_t n) {
  int op = -1;
  b[0] = 0;
  switch (sec) {
    case AJS_HOME: op = AJ_HOME_LAYOUT; break;
    case AJS_TITULO: {   // as notas ligadas, pelos nomes
      int k, q = 0;
      size_t u = 0;
      for (k = AJ_NT_IMDB; k <= AJ_NT_SCORE && q < 3; k++) {
        const char *r;
        if (!lig(k) || inativa(k)) continue;
        r = i18n(rotuloOpcao(k));
        u += (size_t)snprintf(b + u, u < n ? n - u : 0, "%s%.*s", q ? " · " : "", (int)strcspn(r, " ("), r);
        if (u >= n) break;
        q++;
      }
      return; }
    case AJS_CARTAZES: {   // onde o trailer toca (mockup: "Trailer: so na pagina")
      int home = lig(AJ_HERO_TRAILER) || lig(AJ_FOCO_TRAILER), pag = lig(AJ_DET_TRAILER_AUTO);
      snprintf(b, n, "%s", i18n(home && pag ? "Trailer na home e na página" : pag ? "Trailer: só na página"
                                : home ? "Trailer: só na home" : "Trailer automático desligado"));
      return; }
    case AJS_APARENCIA:   // "Frost · Vidro ligado"
      snprintf(b, n, "%s · %s", i18n(textoValor(AJ_FUNDO)), i18n(lig(AJ_VIDRO) ? "Vidro ligado" : "Sem vidro"));
      return;
    case AJS_REPRODUCAO:   // "4K · Fonte: o Nuvio escolhe"
      snprintf(b, n, "%s · %s", i18n(textoValor(AJ_QUALIDADE)),
               i18n(lig(AJ_FONTE_MANUAL) ? "Fonte: você escolhe" : "Fonte: o Nuvio escolhe"));
      return;
    case AJS_FONTES: snprintf(b, n, "%s", ajEstadoAcao(AJ_ADDONS)); return;
    case AJS_IDIOMAS: {   // "Portugues · Audio: Original do titulo"
      char a[120];
      snprintf(a, sizeof a, i18n("Áudio: %s"), i18n(textoValor(AJ_AUD_LINGUA)));
      snprintf(b, n, "%s · %s", i18n(textoValor(AJ_IDIOMA)), a);
      return; }
    case AJS_TVAOVIVO: {
      int contas = iptv_contas(NULL, 0);
      if (contas) snprintf(b, n, "%s: %d", i18n("Contas de TV ao vivo"), contas);
      else snprintf(b, n, "%s", i18n("Não configurado"));
      return; }
    case AJS_CONTAS: {   // "Henrique · Trakt"
      size_t u = (size_t)snprintf(b, n, "%s", textoLeitura(AJ_PERFIL_ATIVO));
      if (u < n && traktauth_estado() == TRA_LIGADO) u += (size_t)snprintf(b + u, n - u, " · Trakt");
      if (u < n && simklauth_estado() == SMK_LIGADO) snprintf(b + u, n - u, " · Simkl");
      return; }
    case AJS_DESEMPENHO:   // "1080p · Padrao"
      snprintf(b, n, "%s · %s", i18n(textoValor(AJ_RESOLUCAO)), i18n(textoValor(AJ_QUALIDADE_IMG)));
      return;
    case AJS_SOBRE: op = AJ_VERSAO_I; break;
    default: return;
  }
  if (op >= 0) snprintf(b, n, "%s", textoValor(op));
}
static float aj2MenuAltura(int j) { return j < 2 ? AJ2_MENU_CHIP : AJ2_MENU_LN; }
// y do alto da linha j relativo ao topo do conteudo (sem a rolagem). Cada
// pilula fica na sua fileira (aj2Chips).
static float aj2MenuY(int j) {
  float y;
  int k, g;
  if (j < 2) { aj2ChipsMedir(); return (float)aj2Chips.lin[j] * (AJ2_MENU_CHIP + AJ2_MENU_CHIP_GAP); }
  y = AJ2_MENU_TOPOL;
  for (k = 2; k < j; k++) y += aj2MH[k] + AJ2_MENU_GAP;
  g = aj2Grupo(j - 2);
  return y + (float)(g + 1) * AJ2_MENU_GT + (float)g * AJ2_MENU_GT_EX;
}
static void aj2MenuAtualizar(float dt) {
  int j, n = nSecoes + 2, f = aj2MenuFocado(), red = ajustes_animacoes_reduzidas();
  GfxRect m = aj2MenuR();
  float janela = m.h - 2 * AJ2_MENU_PAD, alvo;
  if (n > AJ2_NMENU) n = AJ2_NMENU;
  aj2MenuMedir();
  for (j = 0; j < n; j++) {
    float alvoH = j < 2 ? AJ2_MENU_CHIP : j == f ? aj2MenuAberta(j) : AJ2_MENU_LN;
    if (!aj2MIni || red) { aj2MH[j] = alvoH; aj2MV[j] = 0; }
    else aj2MH[j] = anim_mola2(&aj2MV[j], aj2MH[j], alvoH, dt, 20.0f);
    // descricao: sai em 80 ms; entra 90 ms depois, em 140 ms.
    if (j == f) {
      aj2MTempo[j] += dt;
      if (!aj2MIni) aj2MDesc[j] = 1.0f;
      else if (red) aj2MDesc[j] = aj2Rampa(aj2MDesc[j], 1.0f, dt, 80.0f);
      else if (aj2MTempo[j] >= 0.09f) aj2MDesc[j] = aj2Rampa(aj2MDesc[j], 1.0f, dt, 140.0f);
    } else {
      aj2MTempo[j] = 0;
      aj2MDesc[j] = aj2MIni ? aj2Rampa(aj2MDesc[j], 0.0f, dt, 80.0f) : 0.0f;
    }
  }
  // Rolagem: so quando a linha aberta passaria do degrade (ou subiu para fora).
  { int alvoJ = f >= 0 ? f : uxIndice;
    float ty = aj2MenuY(alvoJ), th = (f >= 0 && alvoJ == f && alvoJ >= 2) ? aj2MenuAberta(alvoJ) : aj2MenuAltura(alvoJ);
    float fim = aj2MenuY(n - 1) + AJ2_MENU_LN;
    alvo = aj2MRol;
    if (ty + th > alvo + janela - AJ2_MENU_FADE) alvo = ty + th - (janela - AJ2_MENU_FADE);
    if (ty < alvo) alvo = ty;
    if (alvo > fim - janela + AJ2_MENU_FADE) alvo = fim - janela + AJ2_MENU_FADE;
    if (alvo < 0) alvo = 0;
#ifdef NV_TOUCH_UI
    if (ajTelaCheia() && ajToqueLivre[AJT_MENU]) aj2MRolV = 0.0f;
    else
#endif
    if (!aj2MIni || red) { aj2MRol = alvo; aj2MRolV = 0; }
    else aj2MRol = anim_mola2(&aj2MRolV, aj2MRol, alvo, dt, 16.0f); }
  // A superficie de foco e uma peca so: segue a linha nova com a mesma mola.
  if (f >= 0) {
    float ty = aj2MenuY(f), th = aj2MH[f];
    if (!aj2MIni || red) { aj2SupY = ty; aj2SupH = th; aj2SupYv = aj2SupHv = 0; }
    else {
      aj2SupY = anim_mola2(&aj2SupYv, aj2SupY, ty, dt, 20.0f);
      aj2SupH = anim_mola2(&aj2SupHv, aj2SupH, th, dt, 20.0f);
    }
  }
  aj2MIni = 1;
}
// Alfa do degrade de baixo, medido no centro da peca.
static float aj2MenuMascara(float yc, float base) {
  float a;
  if (AJ2_MENU_FADE <= 0.5f) return yc < base ? 1.0f : 0.0f;
  a = (base - yc) / AJ2_MENU_FADE;
  return a < 0 ? 0 : a > 1 ? 1 : a;
}
#ifdef NV_TOUCH_UI
static int ajModalAberto(void);
static void aj2MenuToqueFocar(int j, int chip) {
  if (uxEditor) return;
  focoIndice = 1; uxCabLayout = 0; uxTopo = -1; sairArmado = 0; uxChipAv = 0;
  if (chip) uxTopo = AJ2_T_AV;
  else if (j == 0) uxTopo = AJ2_T_BUSCA;
  else if (j == 1) { uxIndice = 1; uxListarDiferencas(); }
  else if (j - 2 < nSecoes) { focarSecao(j - 2); uxIndice = j; }
}
static GfxRect aj2MenuToqueRect(GfxRect r, GfxRect m, const Aj2Pose *p) {
  float cy = m.y + m.h * 0.5f;
  return (GfxRect){m.x + (r.x - m.x) * p->mS + p->mDx,
                   cy + (r.y - cy) * p->mS, r.w * p->mS, r.h * p->mS};
}
#endif
static void aj2DesenharMenu(const Aj2Pose *p) {
  GfxRect m = aj2MenuR();
  int j, n = nSecoes + 2, f = aj2MenuFocado();
  float topo = m.y + AJ2_MENU_PAD, base = m.y + m.h - AJ2_MENU_PAD;
  float ar, ag, ab, grupo = gfx_opacidade_grupo;
  if (n > AJ2_NMENU) n = AJ2_NMENU;
  if (p->mA <= 0.003f) return;
  aj2MenuMedir();
#ifdef NV_TOUCH_UI
  if (ajTelaCheia() && !ajModalAberto() && !uxEditor && p->mA > 0.1f && p->mS > 0.0f) {
    GfxRect r = aj2MenuToqueRect((GfxRect){m.x, topo, m.w, base - topo}, m, p);
    ajToqueRegistrar(AJT_MENU, r, &aj2MRol, aj2MenuY(n - 1) + aj2MH[n - 1] - (base - topo) + AJ2_MENU_FADE, aj2MRol, 0);
    if (ajToqueN) ajToqueRegioes[ajToqueN - 1].escala *= p->mS;
  }
#endif
  ajustes_acento(&ar, &ag, &ab);
  gfx_opacidade_grupo = grupo * p->mA;
  gfx_transformar(m.x, m.y + m.h * 0.5f, p->mS, p->mDx, 0);
  ajIlha(m, 40.0f, 1, 0);
  gfx_recorte(m.x, topo, m.w, base - topo);
  // A superficie de foco (vidro 14%, solido #2B2D34 com sombra).
  if (f >= 2) {
    GfxRect r = { m.x + AJ2_MENU_PAD, topo + aj2SupY - aj2MRol, m.w - 2 * AJ2_MENU_PAD, aj2SupH };
    if (ajVid()) gfx_cor(r, 38.0f / r.h, 1, 1, 1, 0.14f);
    else {
      gfx_rect((GfxRect){ r.x - 10, r.y + 2, r.w + 20, r.h + 26 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, 0, 0, 0, 0.30f);
      gfx_cor(r, 38.0f / r.h, 0.169f, 0.176f, 0.204f, 1);
    }
    gfx_cor(r, 38.0f / r.h, ar, ag, ab, 0.14f);   // um toque leve do acento
  }
  // As tres pilulas do alto (aj2Chips). Recolhido (rotulos a 0) viram so o
  // icone, cada uma no comeco da sua fileira, para caberem na coluna de
  // icones; a terceira some junto com os rotulos.
  { float rot = p->mRot < 0 ? 0 : p->mRot > 1 ? 1 : p->mRot;
    float altura = AJ2_MENU_TOPOL - 21.0f, fio = aj2MenuMascara(topo - aj2MRol + altura + 10.0f, base);
    float xl[3] = { 0, 0, 0 };   // o x corrente de cada fileira (as pilulas encolhem com `rot`)
    int c;
    for (c = 0; c < 3 && n >= 2; c++) {
      const AcentoFixo *cor = &TEMA_ACENTO[AJ2_CHIP_TEMA[c]];
      float fr = ((cor->fill >> 16) & 255) / 255.0f, fg = ((cor->fill >> 8) & 255) / 255.0f, fb = (cor->fill & 255) / 255.0f;
      float mr = ((cor->marca >> 16) & 255) / 255.0f, mg = ((cor->marca >> 8) & 255) / 255.0f, mb = (cor->marca & 255) / 255.0f;
      int l = aj2Chips.lin[c];
      float cy = topo - aj2MRol + (float)l * (AJ2_MENU_CHIP + AJ2_MENU_CHIP_GAP);
      float mkc = aj2MenuMascara(cy + AJ2_MENU_CHIP * 0.5f, base), ac = c == 2 ? rot : 1.0f;
      float wc = AJ2_MENU_CHIP, w = wc + (aj2Chips.w[c] - wc) * rot, cx = m.x + AJ2_MENU_PAD + xl[l];
      int sel = c == uxIndice, fc = c == 2 ? focoIndice && uxChipAv : sel && focoIndice && !uxChipAv;
      int lig_ = c == 2 && lig(AJ_AVANCADAS);
      // LIGADA: a busca/diferencas aberta (a entrada do indice selecionada) ou
      // as avancadas ligadas. E o estado que fica quando o foco sai.
      int ativa = c == 2 ? lig_ : sel;
      GfxRect r = { cx, cy, w, AJ2_MENU_CHIP };
#ifdef NV_TOUCH_UI
      if (ajTelaCheia() && !ajModalAberto() && !uxEditor && p->mA > 0.1f && (c != 2 || rot > 0.003f) && cy + r.h > topo && cy < base) {
        GfxRect hit = r;
        if (hit.y < topo) { hit.h -= topo - hit.y; hit.y = topo; }
        if (hit.y + hit.h > base) hit.h = base - hit.y;
        hit = aj2MenuToqueRect(hit, m, p);
        ponteiro_alvo(hit.x, hit.y, hit.w, hit.h, aj2MenuToqueFocar, NULL, c, c == 2);
      }
#endif
      xl[l] += w + 10.0f;
      if (c == 2 && rot <= 0.003f) continue;   // recolhido: a terceira nao cabe na coluna
      if (mkc > 0.003f) {
        float mc = mkc * ac, raio = AJ2_MENU_CHIP * 0.5f;
        // Foco = preenchimento e elevacao (sem contorno): a sombra, o fundo
        // quase cheio na cor da pilula e o icone branco. Ligada sem foco =
        // o fundo tingido a meio caminho. Em repouso = so um veu da cor.
        if (fc) gfx_rect((GfxRect){ r.x - 10, r.y + 2, r.w + 20, r.h + 26 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, 0, 0, 0, 0.34f * mc);
        ajNeutro(r, raio, 0.10f, AJ_CHIP_SOL, mc);
        gfx_cor(r, raio / r.h, fr, fg, fb, (fc ? 0.78f : ativa ? 0.42f : 0.20f) * mc);
        { const char *ic = c == 0 ? "aj_search" : c == 1 ? "aj_rotate-ccw-clock" : lig_ ? "aj_eye" : "aj_eye-off";
          GfxRect ri = { cx + (wc - AJ2_CHIP_ICO) * 0.5f + (20.0f - (wc - AJ2_CHIP_ICO) * 0.5f) * rot,
                         cy + (AJ2_MENU_CHIP - AJ2_CHIP_ICO) * 0.5f, AJ2_CHIP_ICO, AJ2_CHIP_ICO };
          if (fc) gfx_icone(ri, ic, 1, 1, 1, mc);
          else gfx_icone(ri, ic, mr, mg, mb, (ativa ? 1.0f : 0.92f) * mc); }
        if (rot > 0.003f && w > wc + 4.0f) {
          // O estado das avancadas por extenso ("Ligado"/"Desligado") a direita,
          // quando a pilula esticada tem lugar para ele sem apertar o rotulo.
          float tx = cx + 20.0f + AJ2_CHIP_ICO + 10.0f, tmax = w - (20.0f + AJ2_CHIP_ICO + 10.0f) - 22.0f;
          if (c == 2) {
            TxtLinha st = ajTxt(TXT_V2_24, lig_ ? "Ligado" : "Desligado");
            if (tmax - (float)txt_largura(aj2Chips.e, aj2ChipRotulo(c)) >= st.w + 18.0f) {
              txt_desenhar_alpha(st, cx + w - 22.0f - st.w, cy + (AJ2_MENU_CHIP - st.h) * 0.5f, (lig_ || fc ? 0.92f : 0.6f) * mc * rot);
              tmax -= st.w + 18.0f;
            }
          }
          { TxtLinha tc = ajTxtC(aj2Chips.e, aj2ChipRotulo(c), tmax);
            txt_desenhar_alpha(tc, tx, cy + (AJ2_MENU_CHIP - tc.h) * 0.5f, ((fc || ativa) ? 1.0f : 0.9f) * mc * rot); }
        }
      }
    }
    if (fio > 0.003f) gfx_cor((GfxRect){ m.x + AJ2_MENU_PAD + 24.0f, topo - aj2MRol + altura + 10.0f, m.w - 2 * AJ2_MENU_PAD - 48.0f, 1 }, 0, 1, 1, 1, 0.08f * fio);
  }
  for (j = 0; j < n; j++) {
    float y = topo + aj2MenuY(j) - aj2MRol, h = aj2MH[j];
    int foco = j == f, ativa = j == uxIndice;
    float rx = m.x + AJ2_MENU_PAD, mk, al, lc;
    if (j < 2) continue;
    if (y > base || y + h < topo) continue;
#ifdef NV_TOUCH_UI
    if (ajTelaCheia() && !ajModalAberto() && !uxEditor && p->mA > 0.1f) {
      GfxRect hit = {rx, fmaxf(y, topo), m.w - 2 * AJ2_MENU_PAD, fminf(y + h, base) - fmaxf(y, topo)};
      hit = aj2MenuToqueRect(hit, m, p);
      ponteiro_alvo(hit.x, hit.y, hit.w, hit.h, aj2MenuToqueFocar, NULL, j, 0);
    }
#endif
    if (aj2GrupoIni(j - 2)) {
      float ty = y - AJ2_MENU_GT + 6.0f, mt = aj2MenuMascara(ty + 10.0f, base);
      if (p->mRot > 0.003f && mt > 0.003f) ajKickerAcento(aj2GrupoNome(aj2Grupo(j - 2)), rx + 28.0f, ty, mt * p->mRot);
    }
    mk = aj2MenuMascara(y + (h > AJ2_MENU_LN + 1 ? AJ2_MENU_LN * 0.5f + 1.0f : h * 0.5f), base);
    if (mk <= 0.003f) continue;
    al = (foco || ativa) ? 1.0f : 0.66f;
    // o rotulo fica no alto da linha que abre: centro 38 fechada, 39 aberta
    lc = y + (h > AJ2_MENU_LN + 1 ? AJ2_MENU_LN * 0.5f + (h - AJ2_MENU_LN) / (aj2MenuAberta(j) - AJ2_MENU_LN + 0.001f) : h * 0.5f);
    if (ativa && !foco) gfx_cor((GfxRect){ rx + 9.0f, lc - 4.0f, 8, 8 }, 0.5f, ar, ag, ab, mk);
    // O icone num ladrilho (2.0.2): neutro parado, cheio no acento em foco,
    // com o icone na tinta dele. Le de longe qual categoria e qual.
    { float tl = AJ2_MENU_LN - 10.0f > 48.0f ? 48.0f : AJ2_MENU_LN - 10.0f;
      GfxRect tr = { rx + 28.0f + AJ2_MENU_ICO * 0.5f - tl * 0.5f, lc - tl * 0.5f, tl, tl };
      if (foco) {
        gfx_cor(tr, 14.0f / tl, ar, ag, ab, 0.95f * mk);
        ajIconeT(aj2MenuIcone(j), rx + 28.0f + 5.0f, lc - (AJ2_MENU_ICO - 10.0f) * 0.5f, AJ2_MENU_ICO - 10.0f, mk);
      } else {
        gfx_cor(tr, 14.0f / tl, 1, 1, 1, (ativa ? 0.12f : 0.07f) * mk);
        ajIcone(aj2MenuIcone(j), rx + 28.0f + 5.0f, lc - (AJ2_MENU_ICO - 10.0f) * 0.5f, AJ2_MENU_ICO - 10.0f, al * mk);
      } }
    if (p->mRot > 0.003f) {
      TxtLinha t;
      // Sem a contagem de opcoes a direita (2.0.2): o numero nao ajuda a achar
      // nada e roubava a largura que os nomes longos (alemao, russo) pedem.
      // A contagem continua no resumo ("32 opcoes · 7 avancadas").
      { float livre = AJ2_MENU_W - 2 * AJ2_MENU_PAD - 28 - AJ2_MENU_ICO - AJ2_MENU_ICO_GAP - 20;
        TxtEstilo e = ativa ? TXT_V2_MENU_B : TXT_V2_MENU;
        // Nome que nao cabe inteiro (idiomas longos): um corpo menor antes do
        // corte com reticencias.
        if ((float)txt_largura(e, aj2MenuTitulo(j)) > livre) e = TXT_V2_28;
        t = ajTxtC(e, aj2MenuTitulo(j), livre);
        // O valor vivo a direita, no que o nome deixa (nome sempre primeiro).
        { char vb[160];
          float sobra = livre - t.w - 28.0f;
          aj2MenuValor(j - 2, vb, sizeof vb);
          if (vb[0] && sobra >= 70.0f) {
            TxtLinha v = ajTxtC(TXT_V2_24, vb, sobra);
            float vx = m.x + m.w - AJ2_MENU_PAD - 28.0f - v.w;
            txt_desenhar_alpha(v, vx, lc - v.h * 0.5f, (foco ? 0.8f : 0.5f) * mk * p->mRot);
            if (j - 2 == AJS_APARENCIA && vx - 26.0f > rx + 28 + AJ2_MENU_ICO + AJ2_MENU_ICO_GAP + t.w + 8.0f)
              gfx_cor((GfxRect){ vx - 24.0f, lc - 8.0f, 16, 16 }, 0.5f, ar, ag, ab, mk * p->mRot);
          } } }
      txt_desenhar_alpha(t, rx + 28 + AJ2_MENU_ICO + AJ2_MENU_ICO_GAP, lc - t.h * 0.5f, al * mk * p->mRot);
      if (aj2MDesc[j] > 0.003f && h > AJ2_MENU_LN + 4) {
        float dy = (1.0f - aj2MDesc[j]) * 6.0f;
        ajBloco(TXT_V2_26, aj2MenuDesc(j), rx + 28 + AJ2_MENU_ICO + AJ2_MENU_ICO_GAP, y + AJ2_MENU_LN - 2.0f + dy,
                m.w - 2 * AJ2_MENU_PAD - 2 * 28.0f - AJ2_MENU_ICO - AJ2_MENU_ICO_GAP, 32.4f, 0.7f * aj2MDesc[j] * mk * p->mRot, 2);
      }
    }
  }
  gfx_sem_recorte();
  gfx_sem_transformar();
  gfx_opacidade_grupo = grupo;
}

// --- DICAS GRANDES ------------------------------------------------------------------
// #205: as teclas do rodape sao clicaveis. O chip "OK" concentra o clique (o
// foco ja e o da tela) e o ponteiro entrega o RETURN pelo caminho de sempre; o
// "Voltar" sintetiza a tecla Voltar. Visual de TV intocado; no celular a area
// de toque cresce um pouco. Em transicao (gfx_transformar) o desenho esta
// escalado e os retangulos nao sao os da tela: nao registra.
static void aj2DicaOk(int a, int b) { (void)a; (void)b; }
static void aj2DicaVoltar(int a, int b) {
  (void)a; (void)b;
  { SDL_Event e; SDL_zero(e);
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_AC_BACK; e.key.state = SDL_PRESSED;
    ajustes_evento(&e);
    e.type = SDL_KEYUP; e.key.state = SDL_RELEASED;
    ajustes_evento(&e); }
}
static void aj2DicasAlvo(const char *k, float x, float yc, float kw, float kh) {
  int ok = !strcmp(k, "OK"), voltar = !strcmp(k, "Voltar");
  float m;
  if ((!ok && !voltar) || gfx_transformado()) return;
  m = layout_modo_mobile() ? 12.0f : 4.0f;
  ponteiro_alvo(x - m, yc - kh * 0.5f - m, kw + 2.0f * m, kh + 2.0f * m,
                ok ? aj2DicaOk : NULL, ok ? NULL : aj2DicaVoltar, 0, 0);
}
// kbd com o corpo `ek` e altura `kh`, rotulo `el`; vao tecla-rotulo `g1`,
// entre dicas `g2`. Devolve a largura. `desenha`: 0 so mede (as chamadas de
// medida usam coordenadas de mentira — nada pode registrar), 1 desenha e
// registra os alvos, 2 so registra (o replay do rodape da TV: os chips ja
// foram desenhados pelo rodape de verdade).
static float aj2DicasLinha(const AjDica *d, int n, float x, float yc, TxtEstilo ek, float kh, float kmin,
                           float kpad, TxtEstilo el, float g1, float g2, int desenha, float a) {
  int i;
  float x0 = x;
  for (i = 0; i < n; i++) {
    TxtLinha k = ajTxt(ek, d[i].k), l = ajTxt(el, d[i].l);
    float kw = k.w + 2 * kpad < kmin ? kmin : k.w + 2 * kpad;
    if (desenha == 1) {
      ajNeutro((GfxRect){ x, yc - kh * 0.5f, kw, kh }, kh * 0.5f, 0.09f, AJ_CHIP_SOL, a);
      txt_desenhar_alpha(k, x + (kw - k.w) * 0.5f, yc - k.h * 0.5f, 0.82f * a);
      txt_desenhar_alpha(l, x + kw + g1, yc - l.h * 0.5f, 0.55f * a);
      aj2DicasAlvo(d[i].k, x, yc, kw, kh);
    }
    if (desenha) aj2DicasAlvo(d[i].k, x, yc, kw, kh);
    x += kw + g1 + l.w + g2;
  }
  return x - x0 - g2;
}

static float aj2PreviaCategoria(int sec, float x, float y, float w, float h);

// --- AS PREVIAS DA V2 -----------------------------------------------------------------
// A previa grande de cada categoria no resumo (1068 x 601 a 100%). A Tela
// inicial e a home de verdade em miniatura; Cartazes e arte sao tres cartazes
// no tamanho da home; as outras usam a previa da primeira opcao, recortada.
static float aj2PreviaCategoria(int sec, float x, float y, float w, float h) {
  float cheia = w * 9.0f / 16.0f;
  if (h >= cheia - 0.5f) return ajVisualCena(sec, -1, x, y, w);
  // Sem altura para o 16:9 (Ajustes grandes): o quadro encolhe por cima e por
  // baixo, a cena fica no meio.
  gfx_recorte(x, y, w, h);
  ajVisualCena(sec, -1, x, y - (cheia - h) * 0.5f, w);
  gfx_sem_recorte();
  return h;
}
// Os cartoes da linha Fundo: a tela de cada modo, gerada (2.0.2, sem foto).
static void aj2MiniFundo(int modo, float x, float y, float w, float a) {
  ajcMiniFundo(modo, x, y, w, a);
}
static void aj2Cor8(const float c[3], int *r, int *g, int *b) {
  *r = (int)(c[0] * 255.0f + 0.5f); *g = (int)(c[1] * 255.0f + 0.5f); *b = (int)(c[2] * 255.0f + 0.5f);
}
// As MEDIDAS da cor (o "tok" do mockup): pilula, pilula contra a ilha, marca
// no escuro, chip ligado e "HDR" de grupo, cada uma com a razao de contraste
// e a meta (verde passa, vermelho nao); embaixo, a familia e o OKLCH.
static void aj2Num(char *d, size_t n, const char *fmt, float v) {
  char *p;
  snprintf(d, n, fmt, v);
  if (ajustes_idioma() == IDIOMA_PT || ajustes_idioma() == IDIOMA_PTPT)
    for (p = d; *p; p++) if (*p == '.') *p = ',';
}
static float aj2CorMedidas(float x, float y, float w, float fim, float a) {
  static const float ILHA[3] = { 18 / 255.0f, 19 / 255.0f, 22 / 255.0f };
  float ac[3], mc[3], hd[3], ch[3], tk, tinta[3], lab[3], y0 = y;
  int k;
  char hx[5][12], cr[16], cap[160];
  struct { const char *rot; const float *sw; const char *hex; float c, meta; int aro; } L[5];
  ajustes_acento(&ac[0], &ac[1], &ac[2]);
  ajustes_acento_marca(&mc[0], &mc[1], &mc[2]);
  ajustes_acento_hdr(&hd[0], &hd[1], &hd[2]);
  tk = ajustes_acento_tinta(NULL, NULL, NULL);
  tinta[0] = tinta[1] = tinta[2] = tk;
  if (tk < 0.5f) { tinta[0] = ILHA[0]; tinta[1] = ILHA[1]; tinta[2] = ILHA[2]; }
  for (k = 0; k < 3; k++) ch[k] = ILHA[k] + (ac[k] - ILHA[k]) * 0.22f;
  { const float *C[5] = { ac, ILHA, mc, ch, hd };
    for (k = 0; k < 5; k++) { int r8, g8, b8; aj2Cor8(C[k], &r8, &g8, &b8); snprintf(hx[k], sizeof hx[k], "#%02x%02x%02x", r8, g8, b8); } }
  L[0].rot = tk > 0.5f ? "Pílula · tinta branca" : "Pílula · tinta escura"; L[0].sw = ac; L[0].hex = hx[0];
  L[0].c = corviva_contraste(ac, tinta); L[0].meta = 4.5f; L[0].aro = 0;
  L[1].rot = "Pílula contra a ilha"; L[1].sw = ac; L[1].hex = hx[1]; L[1].c = corviva_contraste(ac, ILHA); L[1].meta = 3.0f; L[1].aro = 1;
  L[2].rot = "Marca no escuro"; L[2].sw = mc; L[2].hex = hx[2]; L[2].c = corviva_contraste(mc, ILHA); L[2].meta = 7.0f; L[2].aro = 0;
  L[3].rot = "Chip ligado (22%)"; L[3].sw = ch; L[3].hex = hx[3]; L[3].c = corviva_contraste(mc, ch); L[3].meta = 4.5f; L[3].aro = 0;
  L[4].rot = "\"HDR\" do grupo"; L[4].sw = hd; L[4].hex = hx[4]; L[4].c = corviva_contraste(hd, ILHA); L[4].meta = 4.5f; L[4].aro = 0;
  for (k = 0; k < 5 && y + 42.0f <= fim; k++) {
    float yc = y + 21.0f;
    TxtLinha t = ajTxtC(TXT_V2_24, L[k].rot, w - 260.0f), hxl, crl;
    int ok = L[k].c >= L[k].meta;
    gfx_cor((GfxRect){ x, y, w, 1 }, 0, 1, 1, 1, 0.07f * a);
    if (L[k].aro) gfx_cor((GfxRect){ x - 4, yc - 15, 30, 30 }, 0.5f, 0.953f, 0.949f, 0.937f, 0.2f * a);
    if (L[k].aro) gfx_cor((GfxRect){ x - 3, yc - 14, 28, 28 }, 0.5f, ILHA[0], ILHA[1], ILHA[2], a);
    gfx_cor((GfxRect){ x, yc - 11, 22, 22 }, 0.5f, L[k].sw[0], L[k].sw[1], L[k].sw[2], a);
    txt_desenhar_alpha(t, x + 34, yc - t.h * 0.5f, 0.5f * a);
    aj2Num(cr, sizeof cr, "%.1f:1", L[k].c);
    crl = txt_linha(TXT_V2_SEG, cr, ok ? 110 : 240, ok ? 210 : 110, ok ? 140 : 110, 255);
    txt_desenhar_alpha(crl, x + w - crl.w, yc - crl.h * 0.5f, a);
    hxl = ajTxt(TXT_V2_24, L[k].hex);
    txt_desenhar_alpha(hxl, x + w - 96.0f - hxl.w, yc - hxl.h * 0.5f, 0.85f * a);
    y += 42.0f;
  }
  if (y + 40.0f <= fim) {
    char Ls[16], Cs[16];
    float Cc, hh;
    corviva_srgb_para_oklab(ac, lab);
    Cc = sqrtf(lab[1] * lab[1] + lab[2] * lab[2]);
    hh = atan2f(lab[2], lab[1]) * 57.29578f;
    if (hh < 0) hh += 360.0f;
    aj2Num(Ls, sizeof Ls, "%.2f", lab[0]);
    aj2Num(Cs, sizeof Cs, "%.3f", Cc);
    snprintf(cap, sizeof cap, "%s · OKLCH L %s · C %s · %.0f°", i18n(tk > 0.5f ? "Profundo" : "Claro"), Ls, Cs, hh);
    { TxtLinha c = ajTxtC(TXT_V2_24, cap, w);
      gfx_cor((GfxRect){ x, y, w, 1 }, 0, 1, 1, 1, 0.07f * a);
      txt_desenhar_alpha(c, x, y + 14.0f, 0.42f * a); }
    y += 40.0f + 14.0f;
  }
  return y - y0;
}

// --- A GRADE (degrau 0, 2.0.3) -------------------------------------------------------
// Mockup v3, tela 01 (aprovado pelo dono em 07/10). Uma ilha so, de ponta a
// ponta. No alto, TRES PILULAS NEUTRAS (o mesmo cinza translucido, sem cor
// propria): Buscar, "Avancadas · Escondidas/Visiveis" (o estado escrito, nao um
// interruptor) e "Perfil: <nome>". Embaixo, os dois cartoes de descoberta: "O
// que o Nuvio faz?" (o guia, por funcao) e "Resolver um problema" (o capitulo
// de problemas comuns). Depois as categorias em tres colunas, pela tarefa:
// Assistir, Telas, Conta e sistema. Cada cartao: o icone num ladrilho de 60, o
// nome e o valor que importa ("4K · Fonte: o Nuvio escolhe"). Foco = o cartao
// cresce, enche no acento com o halo dele e o texto vai para a tinta do acento.
// Custo: UMA ilha e cartoes de cor chapada; nenhum fosco novo.
// Medidas em unidades da camada dos Ajustes (o mockup a 1920 dividido por 0,8,
// o tamanho padrao): pilula 56, cartao de descoberta 88, cartao 136, vaos 24/16.
#define AJ2_G_PAD    40.0f
#define AJ2_G_CHIP   70.0f   // altura das pilulas
#define AJ2_G_DESC  110.0f   // altura dos cartoes de descoberta
#define AJ2_G_VAO    30.0f   // entre colunas
#define AJ2_G_VAO_V  20.0f   // entre cartoes
#define AJ2_G_CAB    71.0f   // cabecalho do grupo (Assistir, Telas, ...)
#define AJ2_G_MAXH  170.0f
static int guiaNovos(void);
static float aj2GF[AJ2_NMENU + 1];   // foco animado de cada categoria (indice uxIndice)
static float aj2CabF;               // #339: foco animado do "Painel | Lista" do cabecalho
static float aj2TF[AJ2_T_N];         // foco animado de cada alvo do alto
static float aj2LRol;
static float aj2LTopo(void);
static float aj2LBase(void);
static float aj2LYRel(int sec, float *total);
static GfxRect aj2GradeR(void) {
  float x = aj2X0();
  return (GfxRect){ x, AJ2_TOPO, NV_VTELA_W - AJ2_MARGEM - x, NV_VTELA_H - AJ2_TOPO - AJ2_MARGEM };
}
static float aj2GradeColW(void) { return (aj2GradeR().w - 2 * AJ2_G_PAD - 2 * AJ2_G_VAO) / 3.0f; }
static int aj2GradeLinha(int s) { int k = 0; while (s - k > 0 && aj2Grupo(s - k - 1) == aj2Grupo(s)) k++; return k; }
static float aj2GradeChipY(void) { return aj2GradeR().y + 45.0f; }
static float aj2GradeDescY(void) { return aj2GradeChipY() + AJ2_G_CHIP + 32.0f + (ajRetrato() ? AJ2_G_CHIP + 20.0f : 0.0f); }
static float aj2GradeCabY(void) { return aj2GradeDescY() + AJ2_G_DESC + 22.0f; }
static float aj2GradeCartaoH(void) {
  GfxRect g = aj2GradeR();
  float util = g.y + g.h - 57.0f - (aj2GradeCabY() + AJ2_G_CAB), h;
  h = (util - 3 * AJ2_G_VAO_V) / 4.0f;
  return h > AJ2_G_MAXH ? AJ2_G_MAXH : h;
}
static GfxRect aj2GradeCartao(int s) {
  GfxRect g = aj2GradeR();
  if (ajRetrato()) return (GfxRect){g.x + AJ2_G_PAD, aj2LTopo() + aj2LYRel(s, NULL) - aj2LRol, g.w - 2 * AJ2_G_PAD, 180.0f};
  float cw = aj2GradeColW(), ch = aj2GradeCartaoH();
  float y0 = aj2GradeCabY() + AJ2_G_CAB;
  return (GfxRect){ g.x + AJ2_G_PAD + (float)aj2Grupo(s) * (cw + AJ2_G_VAO), y0 + (float)aj2GradeLinha(s) * (ch + AJ2_G_VAO_V), cw, ch };
}
// O texto de cada pilula; a da Avancadas e "Avancadas" + " · Escondidas".
static void aj2TopoTexto(int t, char *b, size_t n, char *det, size_t nd) {
  det[0] = 0;
  if (t == AJ2_T_BUSCA) snprintf(b, n, "%s", i18n("Buscar"));
  else if (t == AJ2_T_AV) {
    snprintf(b, n, "%s", i18n("Avançadas"));
    snprintf(det, nd, " · %s", i18n(lig(AJ_AVANCADAS) ? "Visíveis" : "Escondidas"));
  } else if (t == AJ2_T_DIF) {
    snprintf(b, n, "%s", i18n("Diferentes do padrão"));
    if (uxNDifs) snprintf(det, nd, " · %d", uxNDifs);
  } else pfRotuloChip(b, n);
}
static const char *aj2TopoIcone(int t) {
  return t == AJ2_T_BUSCA ? "aj_search" : t == AJ2_T_AV ? (lig(AJ_AVANCADAS) ? "aj_eye" : "aj_eye-off")
       : t == AJ2_T_PERFIL ? "aj_user-round" : t == AJ2_T_DIF ? "aj_rotate-ccw-clock" : t == AJ2_T_TOUR ? "aj_sparkles" : "aj_life-buoy";
}
static float aj2LayoutSegW(TxtEstilo e, float ico);
static float aj2TopoLargura(int t) {
  char b[160], d[96];
  if (t == AJ2_T_LAYOUT) return aj2LayoutSegW(TXT_V3_CHIP_R, 30.0f);
  aj2TopoTexto(t, b, sizeof b, d, sizeof d);
  return 30 + 34 + 16 + (float)txt_largura(t == AJ2_T_BUSCA ? TXT_V3_CHIP_R : TXT_V3_CHIP, b) + (d[0] ? 8 + (float)txt_largura(TXT_V3_CHIP_R, d) : 0) + 32;
}
static GfxRect aj2TopoRetratoR(int t) {
  if (!aj2TopoVisivel(t)) return (GfxRect){0};
  GfxRect g = aj2GradeR();
  float x = g.x + AJ2_G_PAD, y = aj2GradeChipY(), fim = g.x + g.w - AJ2_G_PAD;
  float w = (fim - x - 20.0f) * 0.5f;
  if (t <= AJ2_T_LAYOUT) {
    // 2x2 grid: Search | Advanced, then Profile | Different-from-default. The
    // Panel|List chip is hidden in portrait (Mobile never offers it).
    int col = t == AJ2_T_AV || t == AJ2_T_DIF || t == AJ2_T_LAYOUT;
    int lin = t == AJ2_T_PERFIL || t == AJ2_T_DIF || t == AJ2_T_LAYOUT;
    return (GfxRect){x + col * (w + 20.0f), y + lin * (AJ2_G_CHIP + 20.0f), w, AJ2_G_CHIP};
  }
  return (GfxRect){x, aj2GradeDescY() + (t == AJ2_T_TOUR ? 0.0f : AJ2_G_DESC + 20.0f), fim - x, AJ2_G_DESC};
}
static GfxRect aj2TopoR(int t) {
  if (!aj2TopoVisivel(t)) return (GfxRect){0};
  if (ajRetrato()) return aj2TopoRetratoR(t);
  GfxRect g = aj2GradeR();
  float cw = aj2GradeColW(), x = g.x + AJ2_G_PAD, y = aj2GradeChipY(), fim = g.x + g.w - AJ2_G_PAD;
  float wb = cw + (cw + AJ2_G_VAO) / 3.0f, wa = aj2TopoLargura(AJ2_T_AV), wp = aj2TopoLargura(AJ2_T_PERFIL);
  float wl = ajLayoutSelecionavel() ? aj2TopoLargura(AJ2_T_LAYOUT) : 0.0f, wd = aj2TopoLargura(AJ2_T_DIF);
  float gap = ajLayoutSelecionavel() ? 80.0f : 60.0f;
  // Drop the optional Advanced state before shortening Search or the profile name.
  if (wb + gap + wa + wp + wd + wl > fim - x)
    wa = 30 + 34 + 16 + (float)txt_largura(TXT_V3_CHIP, i18n("Avançadas")) + 32;
  if (wb + gap + wa + wp + wd + wl > fim - x) { wb = fim - x - gap - wa - wp - wd - wl; if (wb < 240) wb = 240; }
  if (wb + gap + wa + wp + wd + wl > fim - x) { float k = (fim - x - gap - wb) / (wa + wp + wd + wl); wa *= k; wp *= k; wd *= k; wl *= k; }
  if (t == AJ2_T_BUSCA) return (GfxRect){ x, y, wb, AJ2_G_CHIP };
  if (t == AJ2_T_AV) return (GfxRect){ x + wb + 20, y, wa, AJ2_G_CHIP };
  if (t == AJ2_T_PERFIL) return (GfxRect){ x + wb + 20 + wa + 20, y, wp, AJ2_G_CHIP };
  if (t == AJ2_T_DIF) return (GfxRect){ x + wb + 20 + wa + 20 + wp + 20, y, wd, AJ2_G_CHIP };
  if (t == AJ2_T_LAYOUT) return (GfxRect){ fim - wl, y, wl, AJ2_G_CHIP };
  if (t == AJ2_T_TOUR) return (GfxRect){ x, aj2GradeDescY(), 2 * cw + AJ2_G_VAO, AJ2_G_DESC };
  return (GfxRect){ x + 2 * (cw + AJ2_G_VAO), aj2GradeDescY(), cw, AJ2_G_DESC };
}
static void aj2GradeAtualizar(float dt) {
  uxLayoutDisponibilidade();
  int j, red = ajustes_animacoes_reduzidas();
  if (focoIndice) uxListarDiferencas();   // a contagem da pilula; aberta, a lista fica estavel
  for (j = 0; j <= AJ2_NMENU; j++) {
    float alvo = focoIndice && uxTopo < 0 && j == uxIndice ? 1.0f : 0.0f;
    aj2GF[j] = red ? alvo : aj2Rampa(aj2GF[j], alvo, dt, 140.0f);
  }
  for (j = 0; j < AJ2_T_N; j++) {
    float alvo = focoIndice && uxTopo == j ? 1.0f : 0.0f;
    aj2TF[j] = red ? alvo : aj2Rampa(aj2TF[j], alvo, dt, 140.0f);
  }
  { float alvo = !focoIndice && uxCabLayout ? 1.0f : 0.0f;
    aj2CabF = red ? alvo : aj2Rampa(aj2CabF, alvo, dt, 140.0f); }
}
static float aj2Suave(float t) { return t * t * (3.0f - 2.0f * t); }
// A superficie de uma peca da grade: o cinza neutro e, em foco, o acento com o
// halo dele por baixo. Devolve a peca ja crescida.
static GfxRect aj2GradeSuperficie(GfxRect r, float raio, float k, float cresce, float halo, int segmentado) {
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  r = (GfxRect){ r.x - cresce * k, r.y - cresce * k, r.w + 2 * cresce * k, r.h + 2 * cresce * k };
  if (k > 0.01f) gfx_rect((GfxRect){ r.x - 16, r.y - 4, r.w + 32, r.h + 40 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, ar, ag, ab, halo * k);
  if (ajVid() && segmentado) gfx_vidro_folha(r, raio / r.h, 1);
  else ajNeutro(r, raio, 0.075f, 0.125f, 0.129f, 0.141f, 1);
  if (k > 0.01f) gfx_cor(r, raio / r.h, ar, ag, ab, k);
  return r;
}
// Texto que troca para a tinta do acento conforme o foco.
static void aj2GradeTexto(TxtEstilo e, const char *s, float x, float yc, float maxW, float alNormal, float k) {
  int ti = ajTinta();
  TxtLinha a = ajTxtC(e, s, maxW), b = txt_linha_corta(e, s, ti, ti, ti, 255, maxW);
  if (k < 0.99f) txt_desenhar_alpha(a, x, yc - a.h * 0.5f, alNormal * (1.0f - k));
  if (k > 0.01f) txt_desenhar_alpha(b, x, yc - b.h * 0.5f, (alNormal > 0.9f ? 1.0f : 0.80f) * k);
}
static void aj2GradeIcone(const char *ic, float x, float y, float lado, float k) {
  float tinta = ajTinta() / 255.0f;
  if (k < 0.99f) ajIcone(ic, x, y, lado, 0.9f * (1.0f - k));
  if (k > 0.01f) gfx_icone((GfxRect){ x, y, lado, lado }, ic, tinta, tinta, tinta, k);
}
// #339: o segmentado "Painel | Lista" (pilula do indice e cabecalho da
// categoria). Fundo neutro e, em foco, o acento com o halo (a linguagem das
// pilulas); o lado escolhido ganha uma pilula clara por dentro. Sem
// animacao: a troca e instantanea, o arranjo inteiro muda junto.
static const char *aj2LayoutRot(int i) { return i ? "Lista" : "Painel"; }
static const char *aj2LayoutIco(int i) { return i ? "aj_layout-list" : "aj_layout-dashboard"; }
static float aj2LayoutSegLado(TxtEstilo e, float ico, int i) { return 22.0f + ico + 10.0f + (float)txt_largura(e, aj2LayoutRot(i)) + 24.0f; }
static float aj2LayoutSegW(TxtEstilo e, float ico) { return 12.0f + aj2LayoutSegLado(e, ico, 0) + aj2LayoutSegLado(e, ico, 1); }
static void aj2LayoutSeg(GfxRect r0, TxtEstilo e, float ico, float k) {
  GfxRect r = aj2GradeSuperficie(r0, r0.h * 0.5f, k, 3.0f, 0.35f, 1);
  int lista = valor[AJ_LAYOUT_AJUSTES] == 1, i;
  float x = r.x + 6.0f, w0 = aj2LayoutSegW(e, ico), esc = r.w / (w0 > 1 ? w0 : 1);
  for (i = 0; i < 2; i++) {
    float lw = aj2LayoutSegLado(e, ico, i) * esc, yc = r.y + r.h * 0.5f, xi = x + 22.0f * esc;
    GfxRect q = { x, r.y + 6.0f, lw, r.h - 12.0f };
    // o lado escolhido: branco sobre o neutro; em foco, a tinta do acento (no
    // acento claro o branco sumiria)
    if (i == lista) {
      float ti = ajTinta() / 255.0f;
      if (k < 0.99f) gfx_cor(q, 0.5f, 1, 1, 1, 0.14f * (1.0f - k));
      if (k > 0.01f) gfx_cor(q, 0.5f, ti, ti, ti, 0.22f * k);
    }
    aj2GradeIcone(aj2LayoutIco(i), xi, yc - ico * 0.5f, ico, k);
    aj2GradeTexto(e, aj2LayoutRot(i), xi + ico + 10.0f, yc, x + lw - (xi + ico + 10.0f) - 12.0f, i == lista ? 1.0f : 0.55f, k);
    x += lw;
  }
}
static void aj2GradePilula(int t) {
  float k = aj2Suave(aj2TF[t]);
  GfxRect r;
  char b[160], d[96];
  float x, yc, livre;
  if (t == AJ2_T_LAYOUT) { aj2LayoutSeg(aj2TopoR(t), TXT_V3_CHIP_R, 30.0f, k); return; }
  r = aj2GradeSuperficie(aj2TopoR(t), AJ2_G_CHIP * 0.5f, k, 3.0f, 0.35f, 0);
  x = r.x + 30; yc = r.y + r.h * 0.5f;
  aj2TopoTexto(t, b, sizeof b, d, sizeof d);
  aj2GradeIcone(aj2TopoIcone(t), x, yc - 17, 34, k);
  x += 34 + 16;
  livre = r.x + r.w - 28 - x;
  { TxtEstilo e = t == AJ2_T_BUSCA ? TXT_V3_CHIP_R : TXT_V3_CHIP;
    float wb = (float)txt_largura(e, b);
    if (wb > livre) wb = livre;
    aj2GradeTexto(e, b, x, yc, livre, t == AJ2_T_BUSCA ? 0.62f : 1.0f, k);
    if (d[0] && livre - wb > 40) aj2GradeTexto(TXT_V3_CHIP_R, d, x + wb + 8, yc, livre - wb - 8, 0.55f, k); }
}
// "O que o Nuvio faz?" e "Resolver um problema": ladrilho, titulo (com o selo
// vermelho de novidades no do guia) e a frase.
static void aj2GradeDescoberta(int t) {
  float k = aj2Suave(aj2TF[t]);
  GfxRect r = aj2GradeSuperficie(aj2TopoR(t), 22.0f, k, 6.0f, 0.42f, 0);
  const char *tit = t == AJ2_T_TOUR ? "O que o Nuvio faz?" : "Resolver um problema";
  const char *sub = t == AJ2_T_TOUR ? "Tour pelas funções, com atalho para ligar cada uma" : "TV lenta, sem som, legenda errada";
  float tile = 65.0f, x = r.x + 25, tw;
  GfxRect tr = { x, r.y + (r.h - tile) * 0.5f, tile, tile };
  gfx_cor(tr, 17.0f / tile, 1, 1, 1, 0.08f + 0.10f * k);
  aj2GradeIcone(aj2TopoIcone(t), tr.x + 16, tr.y + 16, 33, k);
  x += tile + 25;
  tw = r.x + r.w - 28 - x;
  { TxtLinha l = ajTxtC(TXT_V3_DESC, tit, tw);
    float yc = r.y + r.h * 0.5f - 19.0f;
    aj2GradeTexto(TXT_V3_DESC, tit, x, yc, tw, 1.0f, k);
    if (t == AJ2_T_TOUR && guiaNovos() > 0) {
      char nb[48], up[64];
      float bw;
      snprintf(nb, sizeof nb, i18n("%d novas"), guiaNovos());
      idioma_maiusc_em(ajustes_idioma(), up, sizeof up, nb);
      bw = txt_tracking(TXT_V2_KICK, up, 255, 255, 255, -1, 0, 1, 2.0f) + 22;
      if (x + l.w + 18 + bw < r.x + r.w - 20) {
        GfxRect br = { x + l.w + 18, yc - 16, bw, 32 };
        gfx_cor(br, 6.0f / 32.0f, 0.86f, 0.20f, 0.22f, 1);
        txt_tracking(TXT_V2_KICK, up, 255, 255, 255, br.x + 11, br.y + (32 - 23) * 0.5f, 1, 2.0f);
      }
    } }
  aj2GradeTexto(TXT_V3_SUB, sub, x, r.y + r.h * 0.5f + 21.0f, tw, 0.62f, k);
}
// O valor vivo de cada categoria (aj2MenuValor) com a tinta do foco.
static void aj2GradeCartaoDesenhar(int s, float f) {
  float k = aj2Suave(f), tile, tx, tw, y;
  int j = s + 2;
  char vb[200];
  const char *nome = aj2MenuTitulo(j);
  TxtEstilo en = TXT_V3_NOME;
  GfxRect r = aj2GradeSuperficie(aj2GradeCartao(s), 20.0f, k, 7.0f, 0.45f, 0);
  // O ladrilho do icone (60 x 60 no mockup, raio 14).
  tile = r.h - 50.0f > 75.0f ? 75.0f : r.h - 50.0f;
  { GfxRect tr = { r.x + 25.0f, r.y + (r.h - tile) * 0.5f, tile, tile };
    float ico = tile * 0.50f;
    gfx_cor(tr, 17.0f / tile, 1, 1, 1, 0.07f + 0.12f * k);
    aj2GradeIcone(aj2MenuIcone(j), tr.x + (tile - ico) * 0.5f, tr.y + (tile - ico) * 0.5f, ico, k); }
  tx = r.x + 25.0f + tile + 25.0f;
  tw = r.x + r.w - 25.0f - tx;
  aj2MenuValor(s, vb, sizeof vb);
  // O nome inteiro, sempre: numa linha; em duas (alemao, russo) no mesmo corpo.
  { int linhas = (float)txt_largura(en, nome) > tw ? 2 : 1, ti = ajTinta();
    float lineH = fmaxf(44.0f, (float)ajTxt(en, nome).h + 2.0f);
    float nomeH = linhas == 1 ? 50.0f : lineH * 2.0f, valH = vb[0] && linhas == 1 ? 40.0f : 0.0f;
    y = r.y + (r.h - nomeH - (valH > 0 ? 4.0f + valH : 0)) * 0.5f;
    if (linhas == 1) aj2GradeTexto(en, nome, tx, y + 25.0f, tw, 1.0f, k);
    else {
      if (k < 0.99f) txt_bloco(en, nome, 243, 242, 239, tx, y, tw, lineH, 1.0f - k, 2);
      if (k > 0.01f) txt_bloco(en, nome, ti, ti, ti, tx, y, tw, lineH, k, 2);
    }
    if (valH > 0) aj2GradeTexto(TXT_V3_VALOR, vb, tx, y + nomeH + 4.0f + valH * 0.5f, tw, 0.62f, k); }
}
// --- O INDICE EM LISTA (#339, Ajustes › Aparencia › Layout dos Ajustes) -------------
// "La pantalla de ajustes anterior, la de las filas uno abajo de la otra, era muy
// bonita": a 2.0.0 tinha as categorias empilhadas numa coluna. Com "Lista" o
// alto continua o mesmo (pilulas e os dois cartoes de descoberta) e, no lugar
// das tres colunas de cartoes, as categorias descem uma embaixo da outra na
// largura toda: grupo em caixa alta, ladrilho do icone, nome em 36/700, o
// valor de agora a direita com a seta, fios de 1 px entre as linhas. A linha em
// foco ganha o acento (a mesma linguagem do cartao da grade) e abre a frase da
// categoria embaixo do nome, em vez de mandá-la para o pe. Cima/Baixo andam na
// lista inteira; Cima na primeira sobe para "O que o Nuvio faz?".
#define AJ2_L_GH    58.0f   // titulo de grupo: 24 acima, caixa alta 20, 14 abaixo
#define AJ2_L_RMIN  78.0f   // linha fechada, minima (abaixo disso a lista rola)
#define AJ2_L_RMAX  90.0f
#define AJ2_L_DLH   34.0f   // cada linha da frase da categoria em foco
#define AJ2_L_PE    78.0f   // o pe: o que o alvo do alto faz e as teclas
#define AJ2_L_ICO   56.0f
static float aj2LRh = AJ2_L_RMAX;
static int aj2Lista(void) { return ajustes_layout_lista(); }
static float aj2LTopo(void) { return aj2GradeDescY() + AJ2_G_DESC + 8.0f + (ajRetrato() ? AJ2_G_DESC + 20.0f : 0.0f); }
static float aj2LBase(void) { GfxRect g = aj2GradeR(); return g.y + g.h - AJ2_L_PE; }
static float aj2LTextoX(GfxRect r) { return r.x + 22.0f + AJ2_L_ICO + 26.0f; }
static const char *aj2LDesc(int sec) {
  return sec < (int)(sizeof SECAO_AJUDA / sizeof *SECAO_AJUDA) ? SECAO_AJUDA[sec] : TELA[secIni[sec]].sub;
}
// A frase medida na largura que ela tem (ate o valor nao: ela mora embaixo dele).
static float aj2LDescH(int sec) {
  GfxRect g = aj2GradeR();
  float w = g.w - 2 * AJ2_G_PAD - (22.0f + AJ2_L_ICO + 26.0f) - 60.0f;
  float h = txt_bloco(TXT_V3_SUB, aj2LDesc(sec), 0, 0, 0, 0, -4000, w, AJ2_L_DLH, 0, 2);
  return h < 1.0f ? 0.0f : (h > 2 * AJ2_L_DLH ? 2 * AJ2_L_DLH : h) + 6.0f;
}
static float aj2LLinhaH(int sec) {
  if (ajRetrato() && !aj2Lista()) return 200.0f;
  return aj2LRh + aj2Suave(aj2GF[sec + 2]) * aj2LDescH(sec);
}
// y de cada linha medido do topo da lista (sem rolagem) e a altura do conteudo.
static float aj2LYRel(int sec, float *total) {
  int s, n = nSecoes > AJ2_NMENU - 2 ? AJ2_NMENU - 2 : nSecoes;
  float y = 0, ys = 0;
  for (s = 0; s < n; s++) {
    if (aj2GrupoIni(s)) y += AJ2_L_GH;
    if (s == sec) ys = y;
    y += aj2LLinhaH(s);
  }
  if (total) *total = y;
  return ys;
}
static GfxRect aj2LLinhaR(int sec) {
  GfxRect g = aj2GradeR();
  return (GfxRect){ g.x + AJ2_G_PAD, aj2LTopo() + aj2LYRel(sec, NULL) - aj2LRol, g.w - 2 * AJ2_G_PAD, aj2LLinhaH(sec) };
}
// A altura da linha fechada se ajusta ao que sobra (a 80% tudo cabe sem rolar);
// sem lugar, fica na minima e a lista rola mantendo o foco no terco de cima.
static void aj2LAtualizar(float dt) {
  int n = nSecoes > AJ2_NMENU - 2 ? AJ2_NMENU - 2 : nSecoes, s, grupos = 0;
  float jan = aj2LBase() - aj2LTopo(), total, alvo = 0, rh;
  if (n <= 0) return;
  for (s = 0; s < n; s++) grupos += aj2GrupoIni(s);
  rh = (jan - grupos * AJ2_L_GH - 2 * AJ2_L_DLH - 6.0f) / (float)n;
  aj2LRh = ajRetrato() ? 128.0f : rh > AJ2_L_RMAX ? AJ2_L_RMAX : rh < AJ2_L_RMIN ? AJ2_L_RMIN : rh;
  aj2LYRel(0, &total);
  if (total > jan && focoIndice && uxTopo < 0 && uxIndice >= 2) {
    int sec = uxIndice - 2;
    float y = aj2LYRel(sec, NULL);
    alvo = y + aj2LLinhaH(sec) * 0.5f - jan * 0.38f;
    if (alvo > total - jan) alvo = total - jan;
    if (alvo < 0) alvo = 0;
  } else if (total > jan && focoIndice && uxTopo >= 0) alvo = 0;
  else if (total > jan) alvo = aj2LRol;
#ifdef NV_TOUCH_UI
  if (ajTelaCheia() && ajToqueLivre[AJT_INDICE]) aj2LRol = anim_clamp(aj2LRol, 0.0f, fmaxf(0.0f, total - jan));
  else
#endif
  if (ajustes_animacoes_reduzidas()) aj2LRol = alvo;
  else { float k = dt * 14.0f; aj2LRol += (alvo - aj2LRol) * (k > 1 ? 1 : k); }
}
// Alfa da mascara: some nos 40 px das bordas da janela quando a lista rola.
static float aj2LMascara(float yc) {
  float topo = aj2LTopo(), base = aj2LBase(), a = 1.0f;
  if (yc > base - 30.0f) a = (base - yc) / 30.0f;
  if (aj2LRol > 1.0f && yc < topo + 30.0f) { float b = (yc - topo) / 30.0f; if (b < a) a = b; }
  return a < 0 ? 0 : a > 1 ? 1 : a;
}
static void aj2LLinha(int sec, float f, float a) {
  GfxRect r = aj2LLinhaR(sec);
  float k = aj2Suave(f), yc = r.y + aj2LRh * 0.5f, x = aj2LTextoX(r), xDir = r.x + r.w - 26.0f, tinta = ajTinta() / 255.0f;
  float grupo = gfx_opacidade_grupo;
  int j = sec + 2;
  char vb[200];
  if (a <= 0.004f) return;
  gfx_opacidade_grupo = grupo * a;
  // A superficie so na linha em foco: o acento cheio e o halo dele.
  if (k > 0.01f) {
    float ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    gfx_rect((GfxRect){ r.x - 14, r.y - 2, r.w + 28, r.h + 30 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, ar, ag, ab, 0.32f * k);
    gfx_cor(r, 20.0f / r.h, ar, ag, ab, k);
  }
  // o ladrilho do icone
  { GfxRect tr = { r.x + 22.0f, yc - AJ2_L_ICO * 0.5f, AJ2_L_ICO, AJ2_L_ICO };
    gfx_cor(tr, 14.0f / AJ2_L_ICO, 1, 1, 1, 0.07f + 0.13f * k);
    aj2GradeIcone(aj2MenuIcone(j), tr.x + 13, tr.y + 13, AJ2_L_ICO - 26, k); }
  // a direita: o valor de agora e a seta
  { float xv = xDir - 22.0f;
    float wNome = (float)txt_largura(TXT_V3_DESC, aj2MenuTitulo(j));
    if (k < 0.99f) ajIcone("aj_chevron-right", xv, yc - 11.0f, 22.0f, 0.32f * (1.0f - k));
    if (k > 0.01f) gfx_icone((GfxRect){ xv, yc - 11.0f, 22.0f, 22.0f }, "aj_chevron-right", tinta, tinta, tinta, 0.85f * k);
    xv -= 18.0f;
    aj2MenuValor(sec, vb, sizeof vb);
    if (vb[0]) {
      float livre = xv - (x + wNome + 48.0f);
      if (livre > 120.0f) {
        TxtLinha l = ajTxtC(TXT_V3_VALOR, vb, livre);
        aj2GradeTexto(TXT_V3_VALOR, vb, xv - l.w, yc, livre, 0.62f, k);
      }
    }
    aj2GradeTexto(TXT_V3_DESC, aj2MenuTitulo(j), x, yc, xv - x - 20.0f, 1.0f, k); }
  // a frase da categoria, aberta embaixo do nome
  if (k > 0.01f) {
    int ti = ajTinta();
    float w = r.w - (x - r.x) - 60.0f;
    txt_bloco(TXT_V3_SUB, aj2LDesc(sec), ti, ti, ti, x, yc + aj2LRh * 0.5f - 12.0f, w, AJ2_L_DLH, 0.80f * k * k, 2);
  }
  gfx_opacidade_grupo = grupo;
}
static void aj2DesenharIdxLista(void) {
  GfxRect g = aj2GradeR();
  int n = nSecoes > AJ2_NMENU - 2 ? AJ2_NMENU - 2 : nSecoes, s, foco = focoIndice && uxTopo < 0 ? uxIndice - 2 : -1;
  float topo = aj2LTopo(), base = aj2LBase();
  gfx_recorte(g.x, topo, g.w, base - topo);
  for (s = 0; s < n; s++) {
    GfxRect r = aj2LLinhaR(s);
    if (aj2GrupoIni(s)) {
      float gy = r.y - AJ2_L_GH + 24.0f;
      ajCaps(TXT_V2_KICK, aj2GrupoNome(aj2Grupo(s)), 2.52f, r.x + 24.0f, gy, 0.45f * aj2LMascara(gy + 10.0f));
    }
    if (r.y + r.h < topo - 4 || r.y > base + 4) continue;
    // o fio entre duas linhas do mesmo grupo, recuado ate o texto; some perto do foco
    if (s + 1 < n && !aj2GrupoIni(s + 1) && s != foco && s + 1 != foco) {
      float fy = r.y + r.h;
      gfx_cor((GfxRect){ aj2LTextoX(r), fy, r.x + r.w - 26.0f - aj2LTextoX(r), 1 }, 0, 1, 1, 1, 0.07f * aj2LMascara(fy));
    }
    if (s != foco) aj2LLinha(s, aj2GF[s + 2], aj2LMascara(r.y + aj2LRh * 0.5f));
  }
  if (foco >= 0 && foco < n) { GfxRect r = aj2LLinhaR(foco); aj2LLinha(foco, aj2GF[foco + 2], aj2LMascara(r.y + aj2LRh * 0.5f)); }
  gfx_sem_recorte();
  if (ajRetrato()) return;
  // O pe: um fio, o que o alvo do alto faz (a esquerda) e as teclas (a direita).
  { AjDica d[2] = { { "OK", "Abrir" }, { "Voltar", "Sair dos ajustes" } };
    float yc = g.y + g.h - AJ2_L_PE * 0.5f - 4.0f, x = g.x + AJ2_G_PAD + 24.0f, fim = g.x + g.w - AJ2_G_PAD - 24.0f;
    float wd = aj2DicasLinha(d, 2, 0, 0, TXT_V2_KBD20, 40, 44, 9, TXT_V2_26, 10, 28, 0, 1);
    const char *desc = NULL;
    gfx_cor((GfxRect){ g.x + AJ2_G_PAD, g.y + g.h - AJ2_L_PE, g.w - 2 * AJ2_G_PAD, 1 }, 0, 1, 1, 1, 0.07f);
    if (focoIndice) {
      if (uxTopo == AJ2_T_BUSCA) desc = "Pelo nome ou pelo que quer mudar";
      else if (uxTopo == AJ2_T_AV) desc = ajudaOpcao(AJ_AVANCADAS);
      else if (uxTopo == AJ2_T_PERFIL) desc = "Perfil em uso nesta TV e o que vale para cada perfil.";
      else if (uxTopo == AJ2_T_DIF) desc = "Valores fora da configuração de fábrica";
      else if (uxTopo == AJ2_T_LAYOUT) desc = "Painel tem a prévia ao lado; Lista põe tudo um embaixo do outro.";
      else if (uxTopo == AJ2_T_TOUR) desc = "Funções que pouca gente conhece, com OK para ligar cada uma dali mesmo.";
      else if (uxTopo == AJ2_T_RESOLVER) desc = "O que fazer quando algo não funciona, com o ajuste que resolve.";
    }
    if (desc) { TxtLinha l = ajTxtC(TXT_V2_26, desc, fim - wd - 40.0f - x); txt_desenhar_alpha(l, x, yc - l.h * 0.5f, 0.62f); }
    aj2DicasLinha(d, 2, fim - wd, yc, TXT_V2_KBD20, 40, 44, 9, TXT_V2_26, 10, 28, 1, 1); }
}
// O retangulo da categoria no indice: o cartao da grade ou a linha da lista. A
// pagina da categoria nasce dele (aj2DesenharPagina) e o indice cresce em torno.
static GfxRect aj2IdxCartao(int sec) { return aj2Lista() ? aj2LLinhaR(sec) : aj2GradeCartao(sec); }
static int ajModalAberto(void);
// PONTEIRO (#99) NO INDICE. A pilula/cartao/linha sob o cursor ganha o foco
// pelas mesmas variaveis que uxEvento mexe; o OK do clique segue o caminho de
// sempre (abrir a categoria, a busca, alternar as avancadas...). So com a
// grade assentada (a < 1 e a transicao, com gfx_transformar).
static void aj2PonteiroTopo(int t, int b) {
  (void)b;
  if (uxEditor || !aj2TopoVisivel(t) || (focoIndice && uxTopo == t)) return;
  focoIndice = 1; uxCabLayout = 0; uxTopo = t; sairArmado = 0;
}
static void aj2PonteiroSec(int s, int b) {
  (void)b;
  if (uxEditor || s < 0 || s >= nSecoes || (focoIndice && uxTopo < 0 && uxIndice == s + 2)) return;
  focoIndice = 1; uxCabLayout = 0; uxTopo = -1; uxVeioBusca = 0;
  focarSecao(s); uxIndice = s + 2;
}
static void aj2DesenharGrade(float a) {
  GfxRect g = aj2GradeR();
  float grupo = gfx_opacidade_grupo, cw = aj2GradeColW(), x = g.x + AJ2_G_PAD;
  int s, gr, t, red = ajustes_animacoes_reduzidas(), n = nSecoes;
  if (a <= 0.003f) return;
  if (n > AJ2_NMENU - 2) n = AJ2_NMENU - 2;
  gfx_opacidade_grupo = grupo * a;
  // Abrindo/voltando: a grade cresce em torno do cartao da categoria.
  if (!red && a < 0.999f && uxIndice >= 2) {
    GfxRect c = aj2IdxCartao(uxIndice - 2);
    gfx_transformar(c.x + c.w * 0.5f, c.y + c.h * 0.5f, 1.0f + 0.08f * (1.0f - a), 0, 0);
  }
  ajPaginaIlha(g, 40.0f, 1);
  if (a >= 0.999f && !ajModalAberto() && !uxEditor) {
#ifdef NV_TOUCH_UI
    if (ajTelaCheia() && (aj2Lista() || ajRetrato())) {
      float total; aj2LYRel(0, &total);
      ajToqueRegistrar(AJT_INDICE, (GfxRect){g.x, aj2LTopo(), g.w, aj2LBase() - aj2LTopo()},
                       &aj2LRol, total - (aj2LBase() - aj2LTopo()), aj2LRol, 0);
    }
#endif
    for (t = 0; t < AJ2_T_N; t++) if (aj2TopoVisivel(t)) { GfxRect r = aj2TopoR(t); ponteiro_alvo(r.x, r.y, r.w, r.h, aj2PonteiroTopo, NULL, t, 0); }
    if (aj2Lista()) for (s = 0; s < n; s++) {
      GfxRect r = aj2LLinhaR(s);
      ponteiro_alvo_faixa(r.x, r.y, r.w, r.h, aj2LTopo(), aj2LBase(), aj2PonteiroSec, NULL, s, 0);
    }
    else for (s = 0; s < n; s++) {
      GfxRect r = aj2GradeCartao(s);
      if (ajRetrato()) ponteiro_alvo_faixa(r.x, r.y, r.w, r.h, aj2LTopo(), aj2LBase(), aj2PonteiroSec, NULL, s, 0);
      else ponteiro_alvo(r.x, r.y, r.w, r.h, aj2PonteiroSec, NULL, s, 0);
    }
  }
  // As pilulas e os cartoes de descoberta; o em foco por ultimo (halo por cima).
  for (t = 0; t < AJ2_T_N; t++) if (aj2TopoVisivel(t) && (t != uxTopo || !focoIndice)) { if (t <= AJ2_T_LAYOUT) aj2GradePilula(t); else aj2GradeDescoberta(t); }
  if (focoIndice && aj2TopoVisivel(uxTopo)) { if (uxTopo <= AJ2_T_LAYOUT) aj2GradePilula(uxTopo); else aj2GradeDescoberta(uxTopo); }
  if (aj2Lista()) { aj2DesenharIdxLista(); goto fim_grade; }
  if (ajRetrato()) {
    gfx_recorte(g.x, aj2LTopo(), g.w, aj2LBase() - aj2LTopo());
    for (s = 0; s < n; s++) {
      GfxRect r = aj2GradeCartao(s);
      if (aj2GrupoIni(s)) ajCaps(TXT_V2_KICK, aj2GrupoNome(aj2Grupo(s)), 2.52f, r.x + 24.0f, r.y - AJ2_L_GH + 24.0f, 0.62f);
      if (r.y + r.h >= aj2LTopo() && r.y <= aj2LBase()) aj2GradeCartaoDesenhar(s, aj2GF[s + 2]);
    }
    gfx_sem_recorte();
    goto fim_grade;
  }
  // Os grupos: o nome em cima de cada coluna.
  for (gr = 0; gr < 3; gr++) {
    TxtLinha l = ajTxtC(TXT_V3_SUB, aj2GrupoNome(gr), cw);
    txt_desenhar_alpha(l, x + gr * (cw + AJ2_G_VAO), aj2GradeCabY() + 36.0f - l.h * 0.5f, 0.62f);
  }
  // Os cartoes; o em foco por ultimo (o halo e o crescimento por cima dos vizinhos).
  for (s = 0; s < n; s++) if (s + 2 != uxIndice || !focoIndice || uxTopo >= 0) aj2GradeCartaoDesenhar(s, aj2GF[s + 2]);
  if (focoIndice && uxTopo < 0 && uxIndice >= 2 && uxIndice - 2 < n) aj2GradeCartaoDesenhar(uxIndice - 2, aj2GF[uxIndice]);
  // No pe da coluna mais curta: o que o alvo em foco faz, e as teclas.
  { int ult = n - 1, linhas = aj2GradeLinha(ult) + 1;
    GfxRect c = aj2GradeCartao(ult);
    float topo = c.y + c.h + 34.0f, fim = g.y + g.h - 34.0f, ch = aj2GradeCartaoH();
    if (linhas < 4 && fim - topo > ch * 0.6f) {
      AjDica d[2] = { { "OK", "Abrir" }, { "Voltar", "Sair dos ajustes" } };
      const char *desc = NULL;
      if (uxTopo == AJ2_T_BUSCA) desc = "Pelo nome ou pelo que quer mudar";
      else if (uxTopo == AJ2_T_AV) desc = ajudaOpcao(AJ_AVANCADAS);
      else if (uxTopo == AJ2_T_PERFIL) desc = "Perfil em uso nesta TV e o que vale para cada perfil.";
      else if (uxTopo == AJ2_T_DIF) desc = "Valores fora da configuração de fábrica";
      else if (uxTopo == AJ2_T_LAYOUT) desc = "Painel tem a prévia ao lado; Lista põe tudo um embaixo do outro.";
      else if (uxTopo == AJ2_T_TOUR) desc = "Funções que pouca gente conhece, com OK para ligar cada uma dali mesmo.";
      else if (uxTopo == AJ2_T_RESOLVER) desc = "O que fazer quando algo não funciona, com o ajuste que resolve.";
      else if (uxIndice >= 2) {
        int sec = uxIndice - 2;
        desc = sec < (int)(sizeof SECAO_AJUDA / sizeof *SECAO_AJUDA) ? SECAO_AJUDA[sec] : TELA[secIni[sec]].sub;
      }
      float lineH = fmaxf(39.0f, (float)ajTxt(TXT_V3_SUB, desc ? desc : "").h + 2.0f);
      int maxLines = (int)floorf((fim - 64.0f - topo) / lineH);
      if (desc && maxLines > 0) ajBloco(TXT_V3_SUB, desc, c.x, topo, c.w, lineH, 0.70f, maxLines);
      // as duas teclas; so o OK quando a coluna nao comporta as duas (alemao a 100%)
      { int nd = aj2DicasLinha(d, 2, 0, 0, TXT_V2_KBD20, 40, 44, 9, TXT_V2_26, 10, 28, 0, 1) > c.w ? 1 : 2;
        float wd = aj2DicasLinha(d, nd, 0, 0, TXT_V2_KBD20, 40, 44, 9, TXT_V2_26, 10, 28, 0, 1);
        aj2DicasLinha(d, nd, c.x + c.w - wd, fim - 16.0f, TXT_V2_KBD20, 40, 44, 9, TXT_V2_26, 10, 28, 1, 1); }
    }
  }
  fim_grade:
  if (!red && a < 0.999f && uxIndice >= 2) gfx_sem_transformar();
  gfx_opacidade_grupo = grupo;
}
// A lista nasce da altura do cartao aberto.
static float aj2GradeOrigemY(int idx) {
  GfxRect c;
  if (idx < 2) return aj2GradeChipY() + AJ2_G_CHIP * 0.5f;
  c = aj2IdxCartao(idx - 2);
  return c.y + c.h * 0.5f;
}

// --- A PAGINA DA CATEGORIA (degraus 1 e 2, 2.0.2) -------------------------------------
// O dono: "ele fechado ta feio tb pq ele ta no mesmo lugar, inove". Dentro da
// categoria nao ha mais o menu recolhido parado na coluna da esquerda: o
// CARTAO aberto cresce ate virar o cabecalho da pagina, de ponta a ponta (o
// icone no acento, o grupo, o nome em 54/700 e, a direita, o valor e quantas
// opcoes). A arte e a lista ficam embaixo, na largura toda. Voltar (ou a
// Esquerda) encolhe o cabecalho de volta para o cartao. Uma ilha (a do trilho
// saiu), nenhum fosco novo.
static GfxRect aj2PaginaR(void) {
  float x = aj2X0();
  return (GfxRect){ x, AJ2_TOPO, NV_VTELA_W - AJ2_MARGEM - x, AJ_CAB_PAGINA - 22.0f };
}
#ifdef NV_TOUCH_UI
static void aj2PonteiroCabLayout(int a, int b) {
  (void)a; (void)b;
  if (ajLayoutSelecionavel() && !uxEditor && uxIndice >= 2) { focoIndice = 0; uxCabLayout = 1; }
}
#endif
static void aj2DesenharPagina(float t) {
  GfxRect h = aj2PaginaR(), c = uxIndice >= 2 ? aj2IdxCartao(uxIndice - 2) : aj2TopoR(AJ2_T_DIF);
  float k = aj2Suave(t < 0 ? 0 : t > 1 ? 1 : t), ar, ag, ab, tinta = ajTinta() / 255.0f;
  float conteudo = k * k, grupo = gfx_opacidade_grupo, x, tile = 76.0f, direita;
  GfxRect r;
  char vb[160], q[96];
  const char *nome, *ic, *kick;
  int sec = uxIndice - 2;
  if (ajustes_animacoes_reduzidas()) k = conteudo = 1.0f;
  r = (GfxRect){ c.x + (h.x - c.x) * k, c.y + (h.y - c.y) * k, c.w + (h.w - c.w) * k, c.h + (h.h - c.h) * k };
  ajustes_acento(&ar, &ag, &ab);
  gfx_opacidade_grupo = grupo * (t > 1 ? 1 : t);
  ajPaginaIlha(r, 36.0f, 1);
  // um veu do acento, como o cartao em foco de onde ele veio
  gfx_cor(r, ajTelaCheia() ? 0.0f : 36.0f / r.h, ar, ag, ab, 0.10f + 0.55f * (1.0f - k));
  gfx_opacidade_grupo = grupo * conteudo;
  if (uxIndice >= 2) {
    nome = aj2MenuTitulo(uxIndice); ic = aj2MenuIcone(uxIndice); kick = aj2GrupoNome(aj2Grupo(sec));
    aj2MenuValor(sec, vb, sizeof vb);
    // No cabecalho so o valor principal (mockup: "4K", "Portugues").
    if (sec == AJS_REPRODUCAO) snprintf(vb, sizeof vb, "%s", i18n(textoValor(AJ_QUALIDADE)));
    else if (sec == AJS_IDIOMAS) snprintf(vb, sizeof vb, "%s", i18n(textoValor(AJ_IDIOMA)));
    { int i, nOp = 0, nAv = 0;
      for (i = secIni[sec] + 1; i < secFim(sec); i++)
        if (TELA[i].tipo == IT_OPC && ajOpcaoDisponivel(TELA[i].op)) { nOp++; if (uxAvancada(TELA[i].op)) nAv++; }
      // 2.0.3 (mockup v3): "19 opcoes · 11 em Mais opcoes"; com a pilula
      // Avancadas ligada, "19 opcoes · 11 avancadas" e o valor vira o estado.
      { char tmp[80];
        if (!nAv) tmp[0] = 0;
        else if (lig(AJ_AVANCADAS)) snprintf(tmp, sizeof tmp, i18n(nAv == 1 ? "opções · %d avançada" : "opções · %d avançadas"), nAv);
        else snprintf(tmp, sizeof tmp, i18n("opções · %d em Mais opções"), nAv);
        if (!nAv) snprintf(q, sizeof q, i18n("%d opções"), nOp);
        else snprintf(q, sizeof q, "%d %s", nOp, tmp); }
      if (nAv && lig(AJ_AVANCADAS)) snprintf(vb, sizeof vb, "%s", i18n("Avançadas ligadas")); }
  } else {
    nome = aj2ChipRotulo(1); ic = "aj_rotate-ccw-clock"; kick = "Ajustes";
    snprintf(vb, sizeof vb, "%d", uxNDifs); q[0] = 0;
  }
  if(iptvForm.tipo && sec==AJS_TVAOVIVO) {
    nome=iptvForm.nova?ajIptvAdicionarNome(iptvForm.tipo):iptvAlvo.nome;
    vb[0]=q[0]=0;
  }
  x = h.x + 28.0f;
  float meio = h.y + (ajRetrato() ? 57.0f : h.h * 0.5f);
  { GfxRect tr = { x, meio - tile * 0.5f, tile, tile };
    gfx_rect((GfxRect){ tr.x - 12, tr.y, tr.w + 24, tr.h + 30 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, ar, ag, ab, 0.35f);
    gfx_cor(tr, 22.0f / tile, ar, ag, ab, 1);
    gfx_icone((GfxRect){ tr.x + 19, tr.y + 19, 38, 38 }, ic, tinta, tinta, tinta, 1); }
  x += tile + 26.0f;
  // a direita: o valor de agora e quantas opcoes
  direita = h.x + h.w - 36.0f;
  if (!ajRetrato()) { float wq = 0, wv = 0;
    TxtLinha lq = ajTxt(TXT_V2_24, q), lv = ajTxtC(TXT_V2_28, vb, h.w * 0.30f);
    if (q[0]) wq = lq.w;
    if (vb[0]) wv = lv.w;
    if (vb[0]) txt_desenhar_alpha(lv, direita - wv, h.y + h.h * 0.5f - lv.h - (q[0] ? 0 : -lv.h * 0.5f), 0.85f);
    if (q[0]) txt_desenhar_alpha(lq, direita - wq, h.y + h.h * 0.5f + 4.0f, 0.5f);
    direita -= (wq > wv ? wq : wv) + 40.0f; }
  // #339: o "Painel | Lista" no cabecalho, antes do valor. Cima na primeira
  // linha chega nele; OK (ou o lado) troca o arranjo na hora.
  if (uxIndice >= 2 && ajLayoutSelecionavel()) {
    float sw = aj2LayoutSegW(TXT_V2_24, 26.0f), sh = 60.0f;
    GfxRect seg = { direita - sw, ajRetrato() ? h.y + h.h - sh - 10.0f : h.y + (h.h - sh) * 0.5f, sw, sh };
    aj2LayoutSeg(seg, TXT_V2_24, 26.0f, aj2Suave(aj2CabF));
#ifdef NV_TOUCH_UI
    if (ajTelaCheia() && t > 0.997f && !uxEditor)
      ponteiro_alvo(seg.x, seg.y, seg.w, seg.h, aj2PonteiroCabLayout, NULL, 0, 0);
#endif
    if (!ajRetrato()) direita -= sw + 36.0f;
  }
  ajKickerAcento(i18n(kick), x + 2.0f, meio - 44.0f, 1);
  { TxtLinha tt = ajTxtC(TXT_V2_TIT, nome, direita - x);
    txt_desenhar(tt, x, meio - 18.0f); }
  gfx_opacidade_grupo = grupo;
}

static void aj2Atualizar(float dt) {
  int deg = aj2Degrau(), red = ajustes_animacoes_reduzidas();
  float a1 = deg >= 1 ? 1.0f : 0.0f, a2 = deg >= 2 ? 1.0f : 0.0f;
  ajMovAtualizar(dt);
  ajCenaTrocaAtualizar(dt);
  aj2MenuAtualizar(dt);
  aj2GradeAtualizar(dt);
  if (aj2Lista() || ajRetrato()) aj2LAtualizar(dt);
  if (red) { aj2N1 = a1; aj2N2 = a2; aj2V1 = aj2V2 = 0; }
  else {
    aj2N1 = aj2Mola(&aj2V1, aj2N1, a1, dt, a1 > aj2N1 ? 18.0f : 22.0f, a1 > aj2N1 ? 0.86f : 1.0f);
    aj2N2 = aj2Mola(&aj2V2, aj2N2, a2, dt, a2 > aj2N2 ? 18.0f : 22.0f, a2 > aj2N2 ? 0.86f : 1.0f);
  }
  aj2Rot = aj2Rampa(aj2Rot, deg >= 1 ? 0.0f : 1.0f, dt, 120.0f);
  aj2Miolo = aj2Rampa(aj2Miolo, deg == 0 ? 0.0f : deg == 1 ? 0.42f : 0.16f, dt, 200.0f);   // 2.0.2: no degrau 0 a grade toma a tela
  aj2ListaA = aj2Rampa(aj2ListaA, deg >= 1 ? 1.0f : 0.0f, dt, 160.0f);
  aj2TextoLista = aj2Rampa(aj2TextoLista, deg >= 2 ? 0.0f : 1.0f, dt, 120.0f);
  aj2EdA = aj2Rampa(aj2EdA, deg >= 2 ? 1.0f : 0.0f, dt, 160.0f);
  aj2ResumoA = aj2Rampa(aj2ResumoA, deg == 0 ? 1.0f : 0.0f, dt, deg == 0 ? 200.0f : 110.0f);   // a grade: sai rapido, volta macia
  if (deg >= 1) aj2InspEspera += dt; else aj2InspEspera = 0;
  { float alvo = (deg >= 1 && (red || aj2InspEspera >= 0.06f)) ? 1.0f : 0.0f;
    if (red) aj2Insp = aj2Rampa(aj2Insp, alvo, dt, 120.0f), aj2VI = 0;
    else aj2Insp = aj2Mola(&aj2VI, aj2Insp, alvo, dt, alvo > aj2Insp ? 18.0f : 22.0f, alvo > aj2Insp ? 0.9f : 1.0f); }
}
static Aj2Pose aj2PoseAgora(void) {
  Aj2Pose p;
  int red = ajustes_animacoes_reduzidas();
  float n1 = red ? (aj2Degrau() >= 1) : aj2N1, n2 = red ? (aj2Degrau() >= 2) : aj2N2;
#ifdef AJUSTES_TESTE
  if (aj2PoseFixa) return aj2PoseTeste;
#endif
  p.mS = 1.0f - 0.05f * n1 - 0.05f * n2; p.mDx = -10.0f * n1 - 10.0f * n2;
  p.mA = 0; p.mRot = 0;   // 2.0.2: sem trilho; a pagina tem o seu cabecalho (aj2DesenharPagina)
  (void)aj2Miolo;
  p.lS = 0.92f + 0.08f * n1 - 0.05f * n2; p.lDx = 380.0f * (1.0f - n1) - 30.0f * n2;
  p.lA = aj2ListaA * (1.0f - 0.5f * n2); p.lTxt = aj2TextoLista;
  p.lOy = 0;   // origem: a linha da categoria (ajDesenharTela)
  p.iDx = 160.0f * (1.0f - aj2Insp); p.iA = aj2Insp * (1.0f - aj2EdA);   // o editor cobre o inspetor
  p.rA = aj2ResumoA;
  // A grade e a pagina nao se misturam: a categoria so aparece no que a grade
  // ja largou (ela cresce e some em 110 ms; a lista nasce do cartao).
  { float livre = (1.0f - aj2ResumoA) * (1.0f - aj2ResumoA);
    p.lA *= livre; p.iA *= livre; p.mA *= livre; }
  p.eS = 0.92f + 0.08f * n2; p.eDx = 380.0f * (1.0f - n2); p.eA = aj2EdA;
  if (red) { p.mS = p.lS = p.eS = 1.0f; p.mDx = p.lDx = p.eDx = p.iDx = 0.0f; }
  return p;
}

