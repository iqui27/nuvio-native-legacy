// AJUSTES NO GLASS UI — ILHAS (mockup aprovado pelo dono em 03/10/2026,
// "perfeito": ajustes-mockup.html, DESIGN.md). Tres ilhas sobre a arte do
// titulo: categorias (o ponto no acento marca a categoria aberta), a lista da
// categoria e o inspetor com a previa DE VERDADE da opcao em foco — arte do
// catalogo, marcas de formato e os graficos que a versao anterior tinha, no
// lugar da foto fixa "Exemplo ilustrativo".
//
// Medidas: as do CSS do mockup, em px de 1080p. Os nomes seguem as classes de
// la (ilha, idx, folha, ln, tg, seg, insp, dicas) para que a comparacao lado a
// lado ache cada peca.

#include "home.h"
// A linha e o inspetor do "Guia de uso" moram com o guia (ajustes_ux_guia.inc).
static void guiaInspetorAjustes(GfxRect ir);
static int guiaTotal(void);
static char uxDirArte[1024];
void ajustes_recursos(const char *dirArte) {
  snprintf(uxDirArte, sizeof uxDirArte, "%s", dirArte ? dirArte : "");
  logoapp_iniciar(dirArte);
}
static const char *uxResumo(int op) {
  if (op == AJ_HOME_LAYOUT && uxIndice == 1) return i18n("Escolha como o destaque e as fileiras se organizam na Tela inicial.");
  return ajudaOpcao(op);
}
static const char *uxCaminho(int op) {
  int i;
  for (i = 0; i < AJ_N_TELA; i++)
    if (TELA[i].tipo == IT_OPC && TELA[i].op == op) return TELA[secIni[secDoItem[i]]].titulo;
  return "Ajustes";
}
static int uxSecaoDe(int op) {
  int i;
  for (i = 0; i < AJ_N_TELA; i++)
    if (TELA[i].tipo == IT_OPC && TELA[i].op == op) return secDoItem[i];
  return 0;
}
// O bloco (ROT) da opcao, "" sem bloco.
static const char *uxBloco(int op) {
  int i, j;
  for (i = 0; i < AJ_N_TELA; i++)
    if (TELA[i].tipo == IT_OPC && TELA[i].op == op) {
      for (j = i; j > secIni[secDoItem[i]]; j--)
        if (TELA[j].tipo == IT_ROT) return TELA[j].titulo;
      return "";
    }
  return "";
}

// --- ARTE ---------------------------------------------------------------------
// A arte das previas: a do CATALOGO quando ele ja chegou (os titulos da conta,
// os mesmos da Home), e a amostra embarcada (deploy/app/art) antes disso. `n`
// e o indice da amostra que o mockup usa; com catalogo vira o titulo n.
enum { AJA_FUNDO = 0, AJA_POSTER, AJA_LOGO };
static int ajArteFundoN = 12;
static int ajArteSoAmostra;   // 1 = ignora o catalogo e usa so a amostra embarcada   // titulo atras da tela (ajustes_teste_arte)
static const char *ajArte(int tipo, int n) {
  static char buf[8][1100];
  static int k;
  char *b;
  if (n < 0) n = 0;
  if (cat_n() > 0 && !ajArteSoAmostra) {
    const CatItem *c = cat_item(n % cat_n());
    if (c) {
      const char *s = tipo == AJA_POSTER ? c->poster : tipo == AJA_LOGO ? c->logo : c->backdrop;
      if (s[0]) return s;
      if (tipo == AJA_LOGO) return "";
    }
  }
  if (!uxDirArte[0]) return "";
  k = (k + 1) % 8;
  b = buf[k];
  snprintf(b, sizeof buf[0], "%s/%s%02d.%s", uxDirArte,
           tipo == AJA_POSTER ? "poster/" : tipo == AJA_LOGO ? "logo/" : "", n % 40,
           tipo == AJA_LOGO ? "png" : "jpg");
  return b;
}
// Imagens pedidas e ainda nao prontas durante o desenho de uma cena: a troca
// de cenas (ajustes_ux_secoes.inc) segura a cena anterior enquanto este numero
// sobe. A que ja falhou (404, arquivo que nao existe) nao conta: nunca vem.
static int ajCenaFaltas;
// A cena de um quadro (0 = resumo do indice, 1 = coluna da arte) com a troca
// suave; devolve a altura da cena PEDIDA. Ver ajustes_ux_secoes.inc.
enum { AJ_TROCA_RESUMO = 0, AJ_TROCA_INSPETOR, AJ_TROCA_N };
static float ajCenaTroca(int quadro, int sec, int op, float x, float y, float w, float corte);
static void ajCenaTrocaAtualizar(float dt);
// Arte em "cover" num retangulo de cantos `raioPx`. Devolve 1 se desenhou.
static int ajImg(const char *c, GfxRect r, float raioPx, float a) {
  GLuint t;
  if (!c || !c[0] || r.w < 1.0f || r.h < 1.0f || a <= 0.003f) return 0;
  t = tex_obter_larg(c, r.w > r.h * 1.78f ? r.w : r.h * 1.78f);
  if (!t) {
    gfx_cor(r, raioPx / r.h, 0.16f, 0.165f, 0.18f, 0.6f * a);
    if (!tex_falhou(c)) ajCenaFaltas++;
    return 0;
  }
  gfx_tex_aspect_atual = tex_aspecto(c);
  gfx_rect(r, t, GFX_ARTE, 0, 0, 0, raioPx / r.h, 1, 1, 1, a);
  gfx_tex_aspect_atual = 0;
  return 1;
}
// Logo do titulo inteiro dentro de maxW x maxH, alinhado a esquerda e em cima.
static float ajLogo(const char *c, float x, float y, float maxW, float maxH, float a) {
  GLuint t;
  float asp, w, h;
  if (!c || !c[0]) return 0.0f;
  t = tex_obter_larg(c, maxW);
  if (!t) { if (!tex_falhou(c)) ajCenaFaltas++; return 0.0f; }
  asp = tex_aspecto(c);
  if (asp <= 0.0f) asp = 3.0f;
  w = maxW; h = w / asp;
  if (h > maxH) { h = maxH; w = h * asp; }
  gfx_tex_aspect_atual = asp;
  gfx_rect((GfxRect){ x, y, w, h }, t, GFX_ARTE, 0, 0, 0, 0, 1, 1, 1, a);
  gfx_tex_aspect_atual = 0;
  return w;
}

// --- MATERIAL -------------------------------------------------------------------
static int ajVid(void) { return ajustes_vidro(); }
// A sombra da ilha. `opaca` = o miolo vem por cima com alfa 1 (o solido): a
// parte da mancha que ele cobre nao e desenhada (gfx_sombra_vazada, o furo e a
// ilha sem a faixa do canto arredondado). No vidro o miolo e translucido e a
// mancha inteira aparece por baixo dele.
static void ajIlhaSombra(GfxRect r, float raioPx, float al, int opaca) {
  gfx_sombra_sob((GfxRect){ r.x - 20, r.y - 6, r.w + 40, r.h + 46 }, 1.0f, 0.0f, 0.5f, 0, 0, 0, al,
                 r, raioPx, opaca ? 1.0f : 0.0f);
}
// Ilha: sombra curta, miolo (vidro 80% / solido #15161A) e a luz larga do canto
// de cima, sem contorno. `modal` = miolo a 86%, para a ilha por cima de outra.
static void ajIlha(GfxRect r, float raioPx, float a, int modal) {
  float raio = raioPx / r.h;
  if (a <= 0.003f) return;
  ajIlhaSombra(r, raioPx, (ajVid() ? 0.36f : 0.45f) * a, !ajVid() && a >= 0.999f);
  if (ajVid()) {
    // Teste do vidro: opacidade (78% = 0.80/0.86 de sempre) e fosco.
    float miolo = (modal ? 0.86f : 0.80f) * gfx_vidro_opacidade();
    gfx_vidro_fosco(r, raio, a);
    gfx_vidro_miolo(r, raio, 0.055f, 0.059f, 0.071f, (miolo > 1.0f ? 1.0f : miolo) * a, a);
    gfx_luz_canto(r, raio, r.w * 0.22f, -r.h * 0.40f, r.h * 0.62f, 1, 1, 1, 0.10f * a);
  } else {
    gfx_cor(r, raio, 0.082f, 0.086f, 0.102f, a);
  }
}
// Ilha com o miolo do vidro a `miolo` (o editor da v2: 88%).
static void ajIlhaMiolo(GfxRect r, float raioPx, float miolo) {
  float raio = raioPx / r.h;
  ajIlhaSombra(r, raioPx, ajVid() ? 0.36f : 0.45f, !ajVid());
  if (ajVid()) {
    float m = miolo * gfx_vidro_opacidade();
    gfx_vidro_fosco(r, raio, 1);
    gfx_vidro_miolo(r, raio, 0.055f, 0.059f, 0.071f, m > 1.0f ? 1.0f : m, 1);
    gfx_luz_canto(r, raio, r.w * 0.22f, -r.h * 0.40f, r.h * 0.62f, 1, 1, 1, 0.10f);
  } else gfx_cor(r, raio, 0.082f, 0.086f, 0.102f, 1);
}
// Superficie de FOCO de linha: um degrau mais clara (vidro 12%, solido #2B2D34).
static void ajFocoLinha(GfxRect r, float raioPx, float f, float a) {
  if (f <= 0.01f) return;
  if (ajVid()) {
    float ar, ag, ab;
    gfx_cor(r, raioPx / r.h, 1, 1, 1, 0.12f * f * a);
    ajustes_acento(&ar, &ag, &ab);
    gfx_vidro_aro(r, raioPx / r.h, 2.0f, ar, ag, ab, 0.96f * f * a);
  }
  else {
    gfx_rect((GfxRect){ r.x - 10, r.y + 2, r.w + 20, r.h + 26 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, 0, 0, 0, 0.30f * f * a);
    gfx_cor(r, raioPx / r.h, 0.169f, 0.176f, 0.204f, f * a);
  }
}
// Neutro claro sobre a ilha: branco a `vidA` no vidro, o cinza opaco no solido.
static void ajNeutro(GfxRect r, float raioPx, float vidA, float sr, float sg, float sb, float a) {
  if (ajVid()) gfx_cor(r, raioPx / r.h, 1, 1, 1, vidA * a);
  else gfx_cor(r, raioPx / r.h, sr, sg, sb, a);
}
#define AJ_CHIP_SOL 0.141f, 0.149f, 0.173f   // #24262C
// Pilula cheia no acento (botao/chip/tecla em foco), com a luz do acento.
static void ajPilulaAcento(GfxRect r, float raioPx, float a) {
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  gfx_rect((GfxRect){ r.x - 14, r.y - 2, r.w + 28, r.h + 30 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, ar, ag, ab, 0.32f * a);
  gfx_cor(r, raioPx / r.h, ar, ag, ab, a);
}
static int ajTinta(void) { return ajustes_tinta_foco(); }
// O acento como TEXTO sobre a ilha: a marca (ajustes_acento_marca), que nos
// profundos e a versao clara do matiz.
static void ajAcento8(int *r, int *g, int *b) {
  float ar, ag, ab;
  ajustes_acento_marca(&ar, &ag, &ab);
  *r = (int)(ar * 255.0f + 0.5f); *g = (int)(ag * 255.0f + 0.5f); *b = (int)(ab * 255.0f + 0.5f);
}
// O branco do texto do mockup (#F3F2EF). A opacidade vai no DESENHO
// (txt_desenhar_alpha), nao na rasterizacao: uma textura por opacidade
// encheria o cache de linhas.
#define AJ_TX 243, 242, 239, 255
static TxtLinha ajTxt(TxtEstilo e, const char *s) { return txt_linha(e, s, AJ_TX); }
static TxtLinha ajTxtC(TxtEstilo e, const char *s, float maxW) { return txt_linha_corta(e, s, AJ_TX, maxW); }
static TxtLinha ajTxtT(TxtEstilo e, const char *s) { int t = ajTinta(); return txt_linha(e, s, t, t, t, 255); }
// Caixa alta espacada (kicker): i18n antes, maiusculas (latim + acentos) e tracking.
static float ajCapsRGB(TxtEstilo e, const char *s, float track, float x, float y, float al, int cr, int cg, int cb) {
  char up[200];
  // 2.0.3: a caixa alta da lingua (cirilico e grego tambem), nao so ASCII/latim.
  idioma_maiusc_em(ajustes_idioma(), up, sizeof up, i18n(s));
  return txt_tracking(e, up, cr, cg, cb, x, y, al, track);
}
static float ajCaps(TxtEstilo e, const char *s, float track, float x, float y, float al) {
  return ajCapsRGB(e, s, track, x, y, al, 243, 242, 239);
}
// Como ajCaps, mas cabe em `maxW`: o que passa e cortado por caractere UTF-8 e
// termina em "…". O kicker da folha de fileiras junta titulo, addon, tipo e
// contagem; em alemao/russo (ou num titulo longo de addon) passava da borda.
static float ajCapsC(TxtEstilo e, const char *s, float track, float x, float y, float al, float maxW) {
  char up[240];
  size_t n;
  idioma_maiusc_em(ajustes_idioma(), up, sizeof up, i18n(s));
  if (maxW > 0 && txt_tracking(e, up, 243, 242, 239, -1, 0, 1, track) > maxW) {
    if (txt_tracking(e, up, 243, 242, 239, -1, 0, 1, track * 0.6f) <= maxW) track *= 0.6f;   // aperta o tracking antes de cortar
    else {
    n = strlen(up);
    while (n > 0) {
      do n--; while (n > 0 && ((unsigned char)up[n] & 0xC0) == 0x80);
      up[n] = 0;
      { char t[250];
        snprintf(t, sizeof t, "%s\xe2\x80\xa6", up);
        if (txt_tracking(e, t, 243, 242, 239, -1, 0, 1, track) <= maxW) { snprintf(up, sizeof up, "%s", t); break; } }
    } }
  }
  return txt_tracking(e, up, 243, 242, 239, x, y, al, track);
}
// Kicker padrao: 15/700, tracking .14em, branco a 45%. Devolve a largura.
static float ajKicker(const char *s, float x, float y, float a) {
  return ajCaps(TXT_MINI, s, 2.1f, x, y, 0.45f * a);
}
// Titulo de grupo do indice: o branco com um toque do acento (a marca clara
// do matiz, 30%), mais legivel que o kicker comum (65% em vez de 45%).
static float ajKickerAcento(const char *s, float x, float y, float a) {
  int r, g, b;
  ajAcento8(&r, &g, &b);
  r = (243 * 6 + r * 4) / 10; g = (242 * 6 + g * 4) / 10; b = (239 * 6 + b * 4) / 10;
  return ajCapsRGB(TXT_V2_KICK, s, 2.5f, x, y, 0.70f * a, r, g, b);
}
static float ajKickerC(const char *s, float x, float y, float a, float maxW) {
  return ajCapsC(TXT_MINI, s, 2.1f, x, y, 0.45f * a, maxW);
}
// Icone Lucide na cor do texto do mockup.
static void ajIcone(const char *nome, float x, float y, float lado, float al) {
  gfx_icone((GfxRect){ x, y, lado, lado }, nome, 0.953f, 0.949f, 0.937f, al);
}
static void ajIconeT(const char *nome, float x, float y, float lado, float a) {
  float t = ajTinta() / 255.0f;
  gfx_icone((GfxRect){ x, y, lado, lado }, nome, t, t, t, a);
}
// Bloco de texto com a cor do mockup e o alfa no desenho.
static float ajBloco(TxtEstilo e, const char *s, float x, float y, float w, float lh, float al, int maxL) {
  return txt_bloco(e, s, 243, 242, 239, x, y, w, lh, al, maxL);
}

// --- FUNDO ----------------------------------------------------------------------
// Aparencia › Fundo (Ajustes v2, ajustes_fundo): o que fica atras das ilhas.
// O desenho mora em fundo.c (a pagina do titulo usa o mesmo); resumo:
//   Arte: a arte do titulo nitida com o veu do mockup (preto ~50%, chapado: veu
//         de cor unica nao faz faixa no painel de 8 bits).
//   Arte borrada: a arte do titulo DESFOCADA (gfx_desfocado, assada num
//         quadro de 320x180). Nenhum desfoque por quadro: o assado so refaz
//         quando a arte muda.
//   Frost: sem arte. Cinza frio com tres luzes largas no acento (58%, 38% e
//         14%); todo degrade pelo nv_dither (GFX_VEU_CSS e GFX_LUZ).
#include "corviva.h"
#include "fundo.h"
static const char *ajFundoArte(void) {
  HomeItem hi;
  return (cat_n() > 0 && home_item_focado(&hi) && hi.indice >= 0)
           ? ajArte(AJA_FUNDO, hi.indice) : ajArte(AJA_FUNDO, ajArteFundoN);
}
// 2.0.2 (dono, na TCL): "o fundo do ajustes tem que ser sempre frost que fica
// melhor de ler". Os Ajustes desenham SEMPRE o Frost, qualquer que seja a
// Aparencia › Fundo escolhida (o valor salvo nao muda; a escolha continua
// valendo no resto do app e aparece nos cartoes da propria opcao). E o mesmo
// caminho do Frost do app (fundo_desenhar_modo): a luz assada no canal, sem
// fosco a mais, e as mesmas quedas nos niveis de GPU menores.
static void ajFundo(void) {
  fundo_desenhar_modo(FUNDO_FROST, (GfxRect){ 0, 0, NV_VTELA_W, NV_VTELA_H }, 0.0f, ajFundoArte(), 1.0f);
}

// --- GEOMETRIA ------------------------------------------------------------------
#define AJI_TOPO        (112.0f / ajustes_tamanho_ajustes())   // indice: logo abaixo da ilha do relogio
#define AJI_MARGEM       (40.0f / ajustes_tamanho_ajustes())   // folha e inspetor: 40 das bordas
#define AJI_IDX_W       360.0f
#define AJI_INSP_X     1276.0f
#define AJI_LN_H         76.0f
#define AJI_LN_GAP        4.0f
static float ajX0(void) { return (48.0f + ajustes_rail_largura_fixa()) / ajustes_tamanho_ajustes(); }
static float ajFolhaX(void) { return ajX0() + AJI_IDX_W + 24.0f; }
// TAMANHO DA INTERFACE (escala.h): na tela virtual menor as tres ilhas nao
// cabem com as larguras de 1080p. A folha fica com pelo menos AJI_FOLHA_MIN (o
// segmentado de 460 e o rotulo ao lado) e a proporcao de 1080p (820:604); o
// inspetor fica com o resto, e SAI quando o resto e menor que AJI_INSP_MIN —
// a folha ocupa o lugar dele. Em 100% e a conta fixa de sempre (1276).
#define AJI_FOLHA_MIN  600.0f
#define AJI_INSP_MIN   440.0f   // as dicas do rodape do inspetor nao cabem em menos
static float ajInspX(void) {
  float total, folha;
  if (ajustes_tamanho_ajustes() == 1.0f) return AJI_INSP_X;
  total = NV_VTELA_W - AJI_MARGEM - ajFolhaX() - 24.0f;
  folha = total * (820.0f / 1424.0f);
  if (folha < AJI_FOLHA_MIN) folha = AJI_FOLHA_MIN;
  if (total - folha < AJI_INSP_MIN) return NV_VTELA_W;   // sem inspetor
  return ajFolhaX() + folha + 24.0f;
}
static int ajTemInspetor(void) { return ajInspX() < NV_VTELA_W; }
static float ajFolhaW(void) {
  return ajTemInspetor() ? ajInspX() - 24.0f - ajFolhaX() : NV_VTELA_W - AJI_MARGEM - ajFolhaX();
}
static GfxRect ajFolhaR(void) { return (GfxRect){ ajFolhaX(), AJI_MARGEM, ajFolhaW(), NV_VTELA_H - 2 * AJI_MARGEM }; }
static GfxRect ajInspR(void) { return (GfxRect){ ajInspX(), AJI_MARGEM, NV_VTELA_W - AJI_MARGEM - ajInspX(), NV_VTELA_H - 2 * AJI_MARGEM }; }

// --- GEOMETRIA DA V2 (ajustes-v2.html, aprovado em 03/10) --------------------------
// A pilha de tres degraus: o MENU grande sozinho (com o resumo da categoria a
// direita); no OK a LISTA cresce por cima dele a partir de x = 172 e o menu
// recua para a coluna de icones, com o INSPETOR ao lado; o EDITOR entra do
// mesmo jeito em x = 300. Tudo na tela virtual (escala.h), desenhado a 100%.
// The Settings scale changes content size while frame anchors remain at
// 48/172/300 px and screen margins at 40 px. The global UI zoom is independent.
#define AJ2_S          ajustes_tamanho_ajustes()
#define AJ2_TOPO       (112.0f / AJ2_S)
#define AJ2_MARGEM     (40.0f / AJ2_S)
// 770 (era 660): os nomes inteiros em Montserrat, a fonte mais larga que a TV
// oferece — "Languages and subtitles" em 44/600 mede 563 e a linha tira 198.
#define AJ2_MENU_W     730.0f
// Recolhida (degrau 1 e 2), a ilha do menu encolhe ate a coluna de icones: os
// dois chips de icone lado a lado (18 + 64 + 12 + 64 + 18). A lista da A3 fica
// a direita e a arte da opcao ocupa o lugar que o menu largou.
#define AJ2_RAIL_W     176.0f
#define AJ2_LISTA_DX   (124.0f / AJ2_S)   // 172 com a ilha do menu em 48
#define AJ2_LISTA_W   1000.0f
#define AJ2_EDITOR_DX  (252.0f / AJ2_S)   // 300
static float aj2X0(void) { return (48.0f + ajustes_rail_largura_fixa()) / AJ2_S; }
// A3: linhas compactas (58 + 2 de vao, rotulo 24), o dobro de linhas na tela.
#define AJ2_LN_H        60.0f   // 2.0.3: 48 px no mockup v3 a 80%
#define AJ2_LN_GAP       2.0f
#define AJ2_LN_SUB      76.0f   // linha com a segunda linha ("Depende de: ...")
#define AJ2_LN_LINHA1   60.0f   // a primeira linha de uma linha que abre
#define AJ2_LN_BEN      31.0f   // cada linha da frase de beneficio (2.0.3)
static float aj2N1;
static GfxRect aj2MenuR(void) {
  float n = aj2N1 < 0 ? 0 : aj2N1 > 1 ? 1 : aj2N1;
  (void)n;   // 2.0.2: o indice e a grade; o menu so aparece como trilho
  return (GfxRect){ aj2X0(), AJ2_TOPO, AJ2_RAIL_W, NV_VTELA_H - AJ2_TOPO - AJ2_MARGEM };
}
// #339: "Lista" tira a coluna da arte: a lista toma a largura e a explicacao
// da opcao em foco abre embaixo da linha (o caminho de 120%, que ja era assim).
static int aj2TemInsp(void) { return ajustes_tamanho_ajustes() < 1.19f && !ajustes_layout_lista(); }
// A LARGURA DA LISTA DA V2 (1000 em 80-100%). Nao desenha mais nada: e a regua
// de ajSegmentada (quem e segmentado e quem abre editor), mantida para que a
// A3 nao mude a navegacao de nenhuma opcao.
static float aj2ListaWRef(void) {
  float x = aj2X0() + AJ2_LISTA_DX;
  float w = aj2TemInsp() ? AJ2_LISTA_W : NV_VTELA_W - AJ2_MARGEM - x;
  if (aj2TemInsp() && x + w + 24.0f + 440.0f > NV_VTELA_W - AJ2_MARGEM) w = NV_VTELA_W - AJ2_MARGEM - 464.0f - x;
  return w;
}
// A3 (dono, 04/10, ajustes-agenda-tpl.html): dentro da categoria a ARTE da
// opcao em foco fica a esquerda, grande, com kicker, titulo e descricao
// embaixo (sem ilha); a LISTA compacta numa ilha a direita. A coluna da arte
// comeca depois da ilha do menu recolhida; arte 44% do que sobra, 36 de vao.
#define AJ2_CORPO (AJ2_TOPO + AJ_CAB_PAGINA)   // 2.0.2: abaixo do cabecalho da pagina
static float aj2ArteX(void) { return aj2X0(); }
static float aj2ArteW(void) { return (NV_VTELA_W - AJ2_MARGEM - aj2ArteX()) * 0.44f; }
static GfxRect aj2ListaR(void) {
  float x = ajustes_layout_lista() ? aj2ArteX() : aj2ArteX() + aj2ArteW() + 36.0f;
  return (GfxRect){ x, AJ2_CORPO, NV_VTELA_W - AJ2_MARGEM - x, NV_VTELA_H - AJ2_CORPO - AJ2_MARGEM };
}
// O "inspetor" da A3 e a coluna da arte (sem ilha).
static GfxRect aj2InspR(void) {
  return (GfxRect){ aj2ArteX(), AJ2_CORPO, aj2ArteW(), NV_VTELA_H - AJ2_CORPO - AJ2_MARGEM };
}
static GfxRect aj2EditorR(void) {
  float x = aj2X0() + AJ2_EDITOR_DX;
  return (GfxRect){ x, AJ2_CORPO, NV_VTELA_W - AJ2_MARGEM - x, NV_VTELA_H - AJ2_CORPO - AJ2_MARGEM };
}
// Onde a lista comeca (o fim do cabecalho: 42 + kicker 21,8 + 4 + titulo 62,9
// + 8 + sub 29 + 18) e onde termina (a margem; os ultimos 90 px esvanecem).
static float aj2TextoLista = 1.0f;   // o texto da lista (0 com o editor por cima)
#define AJ2_LISTA_TOPO (AJ2_TOPO + 185.7f)
#define AJ2_LISTA_BASE (NV_VTELA_H - AJ2_MARGEM)

// --- CLASSIFICACAO DAS LINHAS -----------------------------------------------------
enum { AJL_TOG = 0, AJL_ESC, AJL_NUM, AJL_ACAO, AJL_LER };
static int ajTipo(int op) {
  if (ehInterruptor(op)) return AJL_TOG;
  switch (OPCOES[op].tipo) {
    case OP_NUMERO: return AJL_NUM;
    case OP_ACAO: return AJL_ACAO;
    case OP_LEITURA: return AJL_LER;
    default: return AJL_ESC;
  }
}
// O valor i no segmentado: traduzido e sem o aposto entre parenteses
// ("Dinâmica (Apple TV)" vira "Dinâmica"; o nome inteiro fica no inspetor).
static const char *ajRotSeg(int op, int i) {
  static char b[3][96];
  char *p;
  snprintf(b[i % 3], sizeof b[0], "%s", i18n(OPCOES[op].valores[i]));
  p = strstr(b[i % 3], " (");
  if (p && p != b[i % 3]) *p = 0;
  return b[i % 3];
}
// Escolha que cabe na linha: ate tres valores curtos viram o segmentado.
static float aj2SegLargura(int op, int n);
static int ajSegmentada(int op) {
  int n;
  if (op < 0 || ajTipo(op) != AJL_ESC || op == AJ_LEG_LINGUA || op == AJ_LEG_LINGUA2 || op == AJ_AUD_LINGUA || op == AJ_FUNDO ||
      op == AJ_LOGO_APP /* abre o seletor com miniaturas de 64 px, como o icone do app */) return 0;
  n = nValores(op);
  if (n < 2 || n > 3 || !OPCOES[op].valores) return 0;
  // O segmentado so fica na linha se o nome da opcao cabe ao lado dele;
  // senao a escolha abre no editor.
  { float w = aj2SegLargura(op, n);
    if (w + (float)txt_largura(TXT_V2_ROT, rotuloOpcao(op)) + 60.0f + 20.0f > aj2ListaWRef() - 44.0f) return 0;
    return w < 560.0f; }
}
// O editor desta opcao acontece NA LINHA (segmentado, interruptor, cor, fundo),
// e nao no painel do editor. Vindo da lista de diferencas a linha nao esta na
// tela: la e o editor.
static int ajEmLinha(int op) {
  if (op < 0 || uxIndice < 2) return 0;
  return ajTipo(op) == AJL_TOG || ajSegmentada(op) || op == AJ_TEMA || op == AJ_FUNDO;
}
static int ajEditandoLinha(int op) {
  return uxEditor == 1 && !uxRestaurar && !uxAvisoRisco && uxOp == op && ajEmLinha(op);
}
// AS LINHAS QUE ABREM (v2): a primeira linha fica com 70,4 (16 + 38,4 + 16),
// 20 de vao (o gap do flex que quebra) e o que vem embaixo — a regua da cor
// de destaque editando, os tres cartoes do Fundo em foco e, sem inspetor
// (120% ou mais), a ajuda da opcao em foco.
// A REGUA (acentos-mockup.html, quadro 1): quatro fileiras de 72 — claros,
// profundos, dinamicas e a Textura —, o mockup a 1,15x (a linha da v2 e
// mais larga que a do mockup das cores).
#define AJ_REGUA_FILA 72.0f
#define AJ_REGUA_H (4.0f * AJ_REGUA_FILA + 4.0f)
#define AJ2_FUNDO_BAIXO 237.75f   // 4 + cartao (10 + 150,75 + 10 + 29 + 10) + 24
static int aj2FocoNaLista(int i) { return i == focoItem && !focoIndice && !uxCabLayout && uxIndice >= 2; }
// A REGUA EM FILEIRAS: a ordem da TELA (ajustes.c, REGUA_*); o valor gravado
// continua o indice de V_TEMA. ← → anda na fileira, ↑ ↓ troca de fileira na
// mesma coluna.
static const int *ajReguaFila(int f, int *n) {
  static const int TX[2] = { AJ_TEMA_TEXTURA, AJ_TEMA_TEXTURA_SUTIL };
  switch (f) {
    case 0:  *n = 9; return REGUA_CLAROS;
    case 1:  *n = 9; return REGUA_PROFUNDOS;
    case 2:  *n = 3; return REGUA_DIN;
    default: *n = 2; return TX;
  }
}
static int ajReguaOnde(int v, int *col) {
  int f, k, n;
  for (f = 0; f < 4; f++) {
    const int *L = ajReguaFila(f, &n);
    for (k = 0; k < n; k++) if (L[k] == v) { *col = k; return f; }
  }
  *col = 0; return 0;
}
static int ajReguaMover(int v, int dx, int dy) {
  int col, f = ajReguaOnde(v, &col), n;
  const int *L;
  f += dy;
  if (f < 0) f = 0;
  if (f > 3) f = 3;
  L = ajReguaFila(f, &n);
  col += dx;
  if (col < 0) col = 0;
  if (col >= n) col = n - 1;
  return L[col];
}
static int ajTemaNovo(int v) { return v >= 16 && v <= 21; }
// Bolinha de um acento fixo: 56 (o mockup a 1,15x); a em foco 1,18x com o
// anel escuro de 3 e o claro de 2 por fora; a gravada leva o visto na tinta.
static void aj2Bolinha(int v, float cx, float cy, int foco, int gravada, float a) {
  float r, g, b, d = foco ? 66.0f : 56.0f;
  corDeHex(TEMA_ACENTO[v].fill, &r, &g, &b);
  if (foco) {
    gfx_rect((GfxRect){ cx - d * 0.5f - 14, cy - d * 0.5f, d + 28, d + 34 }, 0, GFX_SOMBRA, 1, 0, 0, 0.5f, 0, 0, 0, 0.5f * a);
    gfx_cor((GfxRect){ cx - d * 0.5f - 6, cy - d * 0.5f - 6, d + 12, d + 12 }, 0.5f, 0.953f, 0.949f, 0.937f, 0.92f * a);
    gfx_cor((GfxRect){ cx - d * 0.5f - 3.5f, cy - d * 0.5f - 3.5f, d + 7, d + 7 }, 0.5f, 0.055f, 0.059f, 0.071f, a);
  }
  gfx_cor((GfxRect){ cx - d * 0.5f, cy - d * 0.5f, d, d }, 0.5f, r, g, b, a);
  if (gravada) {
    float k = TEMA_ACENTO[v].fam == 'c' ? 18.0f / 255.0f : 1.0f;
    gfx_icone((GfxRect){ cx - 12, cy - 12, 24, 24 }, "aj_check", k, k, k, a);
  }
  if (ajTemaNovo(v)) {   // as seis novas: o ponto claro no canto
    float px = cx + d * 0.5f * 0.707f, py = cy - d * 0.5f * 0.707f;
    gfx_cor((GfxRect){ px - 8.5f, py - 8.5f, 17, 17 }, 0.5f, 0.055f, 0.059f, 0.071f, 0.95f * a);
    gfx_cor((GfxRect){ px - 6, py - 6, 12, 12 }, 0.5f, 0.953f, 0.949f, 0.937f, a);
  }
}
// Cor que a dinamica `v` tiraria da arte `url` agora (a paleta, se ja ha);
// sem paleta, a da cena.
static void aj2CorDin(int v, const char *url, float c1[3], float c2[3]) {
  CorvivaPaleta p;
  if (url && url[0] && corviva_paleta(url, &p) && p.ok) {
    memcpy(c1, p.acento, sizeof p.acento);
    memcpy(c2, v == AJ_TEMA_GRADIENTE ? p.grad[2] : p.acento, sizeof p.acento);
    return;
  }
  memcpy(c1, nv_acento_viva, sizeof nv_acento_viva);
  memcpy(c2, v == AJ_TEMA_GRADIENTE ? nv_grad_viva[2] : nv_acento_viva, sizeof nv_acento_viva);
}
static void aj2Regua(GfxRect ln, float y, float a) {
  static const char *const KICK[4][2] = {
    { "Claros", "Tinta escura" }, { "Profundos", "Tinta branca" },
    { "Dinâmicas", "Só nesta TV" }, { "Textura", "Do título" } };
  int sel = valor[AJ_TEMA], alvo = uxPendente, f, k, n;
  // A coluna dos rotulos cresce ate 176 quando a linha deixa (Montserrat:
  // "ONLY ON THIS TV" pede ~170); as nove bolinhas (9 x 72) cabem do lado.
  float kcol = ln.w - 30.0f - 9 * 72.0f - 8.0f;
  float kx = ln.x + 30.0f, bx;
  if (kcol > 176.0f) kcol = 176.0f;
  if (kcol < 150.0f) kcol = 150.0f;
  bx = kx + kcol;
  for (f = 0; f < 4; f++) {
    const int *L = ajReguaFila(f, &n);
    float cy = y + f * AJ_REGUA_FILA + 32.0f, px = bx;
    // Em ingles/Montserrat "ONLY ON THIS TV" passava dos 150 da coluna e
    // entrava no primeiro chip: o tracking aperta antes (ajCapsC).
    ajCapsC(TXT_AJ_CAPS13, KICK[f][0], 1.82f, kx, cy - 18.0f, 0.45f * a, kcol - 8.0f);
    ajCapsC(TXT_AJ_CAPS13, KICK[f][1], 1.82f, kx, cy + 1.0f, 0.45f * a, kcol - 8.0f);
    // Largura do nome nas pilulas (filas 2 e 3): a natural quando a fila cabe
    // ate a borda do bloco; senao o teto desce, e so os nomes mais longos
    // encolhem (em partes iguais "From the art" cortava com folga do lado).
    float maxT = 9999.0f;
    if (f >= 2) {
      float cabe = ln.x + ln.w - 18.0f - bx - (n - 1) * 10.0f - n * (7 + 42 + 10 + 18), tw[5], soma = 0;
      int passo;
      for (k = 0; k < n; k++) { tw[k] = ajTxt(TXT_V2_SEG, V_TEMA[L[k]]).w; soma += tw[k]; }
      for (passo = 0; soma > cabe && passo < 40; passo++) {
        float maior = 0;
        for (k = 0; k < n; k++) if (tw[k] > maior) maior = tw[k];
        maxT = maior - 6.0f;
        for (soma = 0, k = 0; k < n; k++) { if (tw[k] > maxT) tw[k] = maxT; soma += tw[k]; }
      }
    }
    for (k = 0; k < n; k++) {
      int v = L[k];
      if (f < 2) { aj2Bolinha(v, bx + 28.0f + k * 72.0f, cy, v == alvo, v == sel && v != alvo, a); continue; }
      // Pilula da dinamica: disco com o DESENHO do que ela faz, tingido na cor
      // que ela tiraria da arte agora, e o nome. Antes eram a mesma miniatura
      // da arte em todas (nao dizia qual era qual) mais um ponto de cor, e a
      // fileira passava da borda do bloco com Montserrat ou em outro idioma:
      // o nome agora encolhe para a largura que sobra (maxT).
      { static const char *const IC[5] = { "aj_pipette", "aj_blend", "aj_spotlight", "aj_paint-roller", "aj_feather" };
        const char *nome = V_TEMA[v], *url = ajFundoArte();
        const char *ic = v == AJ_TEMA_DINAMICA ? IC[0] : v == AJ_TEMA_GRADIENTE ? IC[1] : v == AJ_TEMA_IMERSIVA ? IC[2] :
                         v == AJ_TEMA_TEXTURA ? IC[3] : IC[4];
        TxtLinha t = ajTxtC(TXT_V2_SEG, nome, maxT);
        float w = 7 + 42 + 10 + t.w + 18, c1[3], c2[3];
        GfxRect p = { px, cy - 28.0f, w, 56 };
        int foc = v == alvo;
        if (foc) {
          gfx_cor(p, 0.5f, 1, 1, 1, 0.16f * a);
          gfx_anel_fora(p, 28.0f / 56.0f, 2.0f, 3.0f, 0.953f, 0.949f, 0.937f, 0.92f * a);
        } else ajNeutro(p, 28, 0.07f, AJ_CHIP_SOL, a);
        aj2CorDin(v, url, c1, c2);
        gfx_cor((GfxRect){ px + 7, cy - 21, 42, 42 }, 0.5f, 0.055f, 0.059f, 0.071f, 0.85f * a);
        if (v == sel && !foc) gfx_icone((GfxRect){ px + 16, cy - 12, 24, 24 }, "aj_check", 1, 1, 1, a);
        else {
          if (v == AJ_TEMA_GRADIENTE) c1[0] = (c1[0] + c2[0]) * 0.5f, c1[1] = (c1[1] + c2[1]) * 0.5f, c1[2] = (c1[2] + c2[2]) * 0.5f;
          gfx_icone((GfxRect){ px + 16, cy - 12, 24, 24 }, ic, c1[0], c1[1], c1[2], a);
        }
        txt_desenhar_alpha(t, px + 59, cy - t.h * 0.5f, (foc ? 1.0f : 0.78f) * a);
        px += w + 10.0f; }
    }
  }
}
// A FRASE DE BENEFICIO da linha em foco (2.0.3): ate duas linhas, medida na
// largura da linha.
static const char *uxBeneficio(int op);
static float ajBeneficioLargura(void) { return aj2ListaR().w - 36.0f - 44.0f; }
static float ajBeneficioH(int op) {
  float h;
  if (op < 0) return 0;
  h = txt_bloco(TXT_V3_BEN, uxBeneficio(op), 0, 0, 0, 0, -4000, ajBeneficioLargura(), AJ2_LN_BEN, 0, 2);
  return h < 1.0f ? 0.0f : h > 2 * AJ2_LN_BEN ? 2 * AJ2_LN_BEN : h;
}
// SEM INSPETOR (#339 "Lista", ou 120%): o "o que muda" da opcao em foco abre
// embaixo da propria linha — a ajuda inteira (ate 4 linhas, numa medida de
// leitura, nao a largura toda) e, quando ha padrao de fabrica, "Recomendado"
// com o valor e o check no acento, e o custo ao lado.
#define AJ_AJUDA_META 40.0f
static float ajAjudaLinhaW(void) { float w = aj2ListaR().w - 36.0f - 44.0f - 80.0f; return w > 1260.0f ? 1260.0f : w; }
static float ajAjudaLinhaTxtH(int op) {
  float h = txt_bloco(TXT_V3_BEN, uxResumo(op), 0, 0, 0, 0, -4000, ajAjudaLinhaW(), AJ2_LN_BEN, 0, 4);
  return h < 1.0f ? 0.0f : h > 4 * AJ2_LN_BEN ? 4 * AJ2_LN_BEN : h;
}
static float ajAjudaLinhaH(int op) {
  return ajAjudaLinhaTxtH(op) + (uxTemPadrao(op) && op != AJ_TEMA ? 10.0f + AJ_AJUDA_META : 0.0f);
}
static int ajCusto(int op);
static const char *ajCustoTexto(int c);
static void ajAjudaLinhaDesenhar(int op, float x, float y, float a) {
  float h = ajAjudaLinhaTxtH(op);
  txt_bloco(TXT_V3_BEN, uxResumo(op), 243, 242, 239, x, y, ajAjudaLinhaW(), AJ2_LN_BEN, 0.70f * a, 4);
  if (uxTemPadrao(op) && op != AJ_TEMA) {
    char padrao[160];
    float yc = y + h + 10.0f + AJ_AJUDA_META * 0.5f, ar, ag, ab, xx = x;
    TxtLinha lk = ajTxt(TXT_V2_24, "Recomendado"), lv;
    uxValorTexto(op, uxValorPadrao(op), padrao, sizeof padrao);
    lv = ajTxtC(TXT_V2_24, padrao, ajAjudaLinhaW() - lk.w - 60.0f);
    ajustes_acento_marca(&ar, &ag, &ab);
    gfx_icone((GfxRect){ xx, yc - 11.0f, 22, 22 }, uxDiferente(op) ? "aj_star" : "aj_check", ar, ag, ab, 0.9f * a);
    xx += 32.0f;
    txt_desenhar_alpha(lk, xx, yc - lk.h * 0.5f, 0.45f * a);
    xx += lk.w + 10.0f;
    txt_desenhar_alpha(lv, xx, yc - lv.h * 0.5f, 0.85f * a);
    xx += lv.w;
    if (ajCusto(op)) {
      TxtLinha lc = ajTxtC(TXT_V2_24, ajCustoTexto(ajCusto(op)), ajAjudaLinhaW() - (xx - x) - 40.0f);
      if (lc.w > 40.0f) {
        gfx_cor((GfxRect){ xx + 16.0f, yc - 2.0f, 4, 4 }, 0.5f, 1, 1, 1, 0.35f * a);
        txt_desenhar_alpha(lc, xx + 32.0f, yc - lc.h * 0.5f, 0.55f * a);
      }
    }
  }
}
// --- 2.0.3 (M3): FILHAS RECUADAS, DEPENDENCIA NA LINHA, CUSTO E SELOS ---------------
// A linha que depende da de cima (ou que so vale quando ela vale) fica recuada
// com um canto "└", como no mockup v3. Alem do requisito (uxRequisito), umas
// filhas de sentido: o que o Nuvio prioriza e quanto espera so contam quando ele
// escolhe a fonte; o padrao e o modelo sao do filtro; o estilo da segunda
// legenda e dela.
static int uxPaiVisual(int op) {
  switch (op) {
    case AJ_FONTE_PRIORIDADE: case AJ_FONTE_PRAZO: return AJ_FONTE_MANUAL;
    case AJ_FONTE_REGEX_PADRAO: case AJ_FONTE_REGEX_MODELO: return AJ_FONTE_REGEX;
    case AJ_FONTE_ADDONS_PERM: case AJ_FONTE_PLUGINS_PERM: return AJ_FONTE_ESCOPO;
    case AJ_LEG2_POS: case AJ_LEG2_TAMANHO: case AJ_LEG2_COR: case AJ_LEG2_FUNDO: case AJ_LEG2_BORDA:
      return AJ_LEG_LINGUA2;
    case AJ_DESCANSO_ESTILO: case AJ_DESCANSO_FONTE: return AJ_ESMAECER;
    case AJ_CW_BLUR_PROX: return AJ_CW_THUMB;
    case AJ_HERO_CHEIO: return -1;   // depende do layout, que nao e a linha de cima
    default: return uxRequisito(op);
  }
}
static int uxDescende(int op, int pai) {
  int k;
  for (k = 0; k < 4 && op >= 0; k++) { op = uxPaiVisual(op); if (op == pai) return 1; }
  return 0;
}
// Recuada se, subindo pelas linhas visiveis do mesmo bloco, so ha irmas (ou
// netas) ate chegar na mae.
static int ajLinhaFilha(int i) {
  int op = TELA[i].op, pai = uxPaiVisual(op), j;
  if (pai < 0) return 0;
  for (j = i - 1; j > secIni[secDoItem[i]]; j--) {
    if (TELA[j].tipo != IT_OPC) return 0;
    if (!visivel(j)) continue;
    if (TELA[j].op == pai) return 1;
    if (!uxDescende(TELA[j].op, pai)) return 0;
  }
  return 0;
}
// O aviso de dependencia aparece na propria linha: na linha em foco, e nas
// filhas recuadas (a mae esta logo acima, a frase explica o apagado).
static int ajLinhaAviso(int i) {
  int op = TELA[i].op;
  return inativa(op) && uxRequisito(op) >= 0 && (aj2FocoNaLista(i) || ajLinhaFilha(i));
}
static int uxLigarRequisitos(int op, int aplicar);
static void ajAvisoTexto(int op, char *b, size_t n) {
  int req = uxRequisito(op);
  const char *nome = req >= 0 ? i18n(rotuloOpcao(req)) : "";
  int liga = uxLigarRequisitos(op, 0);
  if (liga == 2) snprintf(b, n, i18n("Precisa ligar \"%s\" — OK liga as duas"), nome);
  else if (liga == 1) snprintf(b, n, i18n("Precisa ligar \"%s\" — OK liga"), nome);
  else snprintf(b, n, i18n("Depende de \"%s\" — OK mostra"), nome);
}
// CUSTO (mockup v3, tela 07): pesa na TV (zap), usa internet (globe), precisa
// de chave (key-round). Bits: 1, 2, 4.
static int ajCusto(int op) {
  switch (op) {
    case AJ_PROF: case AJ_PROF_BORDA: case AJ_PROF_BRILHO: case AJ_PROF_COBERTURA: case AJ_PROF_POSTERS:
    case AJ_PROF_CW: case AJ_PROF_EPS: case AJ_PROF_ELENCO: case AJ_PROF_TRAILERS:
    case AJ_VIDRO: case AJ_VIDRO_FOSCO: case AJ_GPU_EFEITOS: case AJ_TEX_MB: case AJ_ITENS_FILEIRA:
    case AJ_QUALIDADE_IMG: case AJ_RESOLUCAO: case AJ_DTS_AC3: case AJ_CACHE_SEEK: case AJ_FOCO_TRAILER:
      return 1;
    case AJ_MANTER_VIDEO: case AJ_HERO_TRAILER: case AJ_FIL_LIMITE: return 1 | 2;
    case AJ_QUALIDADE: case AJ_TMDB_LIGADO: case AJ_MDB_LIGADO: case AJ_FONTE_AQUECER:
    case AJ_FONTE_CONFERIR_VARIAS: case AJ_FONTE_PREPARAR: case AJ_TRAILER_QUAL: case AJ_DET_TRAILER_AUTO:
      return 2;
    case AJ_SEEKR_LIGADO: return 2 | 4;
    case AJ_P2P_LIGADO: case AJ_FANART_CHAVE: case AJ_SEEKR_CHAVE: case AJ_MDB_CHAVE: case AJ_POSTER_CHAVE:
    case AJ_DEBRID_RD: case AJ_DEBRID_TB: case AJ_DEBRID_PM: case AJ_DEBRID_AD:
      return 4;
    default: return 0;
  }
}
static const char *ajCustoTexto(int c) {
  return (c & 1) ? "pesa na TV" : (c & 2) ? "usa mais internet" : (c & 4) ? "precisa de chave" : "";
}
// Desenha os icones de custo terminando em xDir; devolve a largura usada.
static float ajCustoIcones(int op, float xDir, float yc, float a) {
  static const char *const IC[3] = { "aj_zap", "aj_globe", "aj_key-round" };
  int c = ajCusto(op), k;
  float x = xDir;
  for (k = 2; k >= 0; k--) if (c & (1 << k)) {
    x -= 24.0f;
    ajIcone(IC[k], x, yc - 12.0f, 24.0f, 0.5f * a);
    x -= 8.0f;
  }
  return xDir - x;
}
// O SELO depois do rotulo: a plataforma ("SO LG", "ANDROID") ganha de
// "AVANCADO", que ganha do escopo ("ESTA TV" / "ESTE PERFIL", mockup v3 tela
// 03). Acoes e leituras nao tem escopo para mostrar.
static const char *ajSelo(int op) {
#if !defined(NV_TPK) && !defined(NV_ANDROID)
  if (op == AJ_DV_MKV) return "Só LG";
#endif
  if (op == AJ_LEG_SYNC_AUDIO) return "Android";
  if (uxAvancada(op)) return "Avançado";
  if (!uxTemPadrao(op)) return NULL;
  return !strcmp(uxEscopo(op), "Conta/perfil") ? "Este perfil" : "Esta TV";
}
static float ajLinhaH(int i) {
  int op = TELA[i].op;
  float h = AJ2_LN_H, baixo = 0;
  // 2.0.3: a linha dependente diz na propria linha o que falta ("Precisa ligar
  // X — OK liga as duas"), no lugar da modal.
  if (op == AJ_GUIA) h = AJ2_LN_SUB;
  else if (ajLinhaAviso(i)) return AJ2_LN_LINHA1 - 6.0f + AJ2_LN_BEN + 10.0f;
  // A regua de cores abre NA LINHA, com e sem inspetor: o inspetor mostra a
  // previa viva e as medidas da cor (aj2PreviaCor).
  if (op == AJ_TEMA && ajEditandoLinha(op) && aj2FocoNaLista(i)) baixo += AJ_REGUA_H;
  if (op == AJ_FUNDO && aj2FocoNaLista(i)) baixo += AJ2_FUNDO_BAIXO;
  if (baixo > 0) return AJ2_LN_LINHA1 + 20.0f + baixo;
  if (!aj2TemInsp() && aj2FocoNaLista(i) && op != AJ_GUIA) return AJ2_LN_LINHA1 - 6.0f + ajAjudaLinhaH(op) + 18.0f;
  // 2.0.3: a linha em foco ganha a frase de beneficio embaixo do rotulo.
  if (h == AJ2_LN_H && aj2TemInsp() && aj2FocoNaLista(i) && op != AJ_GUIA) {
    float b = ajBeneficioH(op);
    if (b > 0) h = AJ2_LN_LINHA1 - 6.0f + b + 12.0f;
  }
  return h;
}

// --- INDICE (ilha de categorias) -------------------------------------------------
static void ajDesenharIndice(void) {
  GfxRect ilha = { ajX0(), AJI_TOPO, AJI_IDX_W, NV_VTELA_H - AJI_TOPO - AJI_MARGEM };
  float y = ilha.y + 14.0f;
  int j, indRecorte = 0;
  float indFim = 0.0f;
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  ajIlha(ilha, 32.0f, 1, 0);
  // TELA VIRTUAL MENOR (escala.h): as categorias nao cabem acima do rodape do
  // perfil. A lista vira uma janela recortada que acompanha a categoria
  // escolhida; em 100% tudo cabe e nada disso acontece.
  { float topoL = ilha.y + 8.0f, fimL = ilha.y + ilha.h - 14.0f - 6.0f - 21.0f - 20.0f - 12.0f;
    float tot = (float)(nSecoes + 2) * 62.0f + 21.0f + 14.0f;
    if (ajustes_tamanho_ajustes() != 1.0f && tot > fimL - topoL) {
      float jan = fimL - topoL, sy = 14.0f + uxIndice * 62.0f + (uxIndice >= 2 ? 21.0f : 0.0f) + 29.0f;
      float off = sy - jan * 0.5f;
      if (off > tot - jan) off = tot - jan;
      if (off < 0.0f) off = 0.0f;
      y -= off;
      indRecorte = 1; indFim = fimL;
      gfx_recorte(ilha.x, topoL, ilha.w, jan);
    } }
  for (j = 0; j < nSecoes + 2; j++) {
    int sel = j == uxIndice, foco = sel && focoIndice;
    const char *titulo = j == 0 ? "Buscar ajuste" : j == 1 ? "Diferentes do padrão" : TELA[secIni[j - 2]].titulo;
    const char *icone = j == 0 ? "aj_search" : j == 1 ? "aj_rotate-ccw-clock" : TELA[secIni[j - 2]].icone;
    GfxRect r;
    float al = (foco || sel) ? 1.0f : 0.66f;
    if (j == 2) {
      gfx_cor((GfxRect){ ilha.x + 32.0f, y + 8.0f, ilha.w - 64.0f, 1 }, 0, 1, 1, 1, 0.08f);
      y += 21.0f;
    }
    r = (GfxRect){ ilha.x + 14.0f, y, ilha.w - 28.0f, 58.0f };
    // Na janela recortada so entram categorias inteiras: meia linha cortada
    // pelo recorte parece defeito, nao rolagem.
    if (indRecorte && (r.y < ilha.y + 7.0f || r.y + 58.0f > indFim + 1.0f)) { y += 62.0f; continue; }
    if (foco) ajNeutro(r, 29.0f, 0.14f, 0.169f, 0.176f, 0.204f, 1);
    else if (sel) gfx_cor((GfxRect){ r.x + 7.0f, r.y + 26.5f, 5, 5 }, 0.5f, ar, ag, ab, 1);
    ajIcone(icone, r.x + 20.0f, r.y + 16.0f, 26.0f, al);
    { TxtLinha t = ajTxtC(TXT_AJ_ITEM, titulo, r.w - 82.0f);
      txt_desenhar_alpha(t, r.x + 62.0f, r.y + (58.0f - t.h) * 0.5f, al); }
    y += 62.0f;
  }
  if (indRecorte) gfx_sem_recorte();
  // Rodape: quem esta usando (as preferencias "Conta/perfil" sao dele).
  { const ContaPerfil *p = perfis_item_ativo();
    const char *nome = p && p->nome[0] ? p->nome : NULL;
    char ini[8], nb[80];
    float cy = ilha.y + ilha.h - 14.0f - 6.0f - 21.0f;
    float ax = ilha.x + 30.0f;
    float cr = 0.486f, cg = 0.361f, cb = 1.0f;
    TxtLinha l, n, s;
    if (nome) {
      unsigned char c0 = (unsigned char)nome[0];
      int nb0 = c0 >= 0xF0 ? 4 : c0 >= 0xE0 ? 3 : c0 >= 0xC0 ? 2 : 1;
      snprintf(ini, sizeof ini, "%.*s", nb0, nome);
      if (ini[0] >= 'a' && ini[0] <= 'z') ini[0] = (char)(ini[0] - 32);
      snprintf(nb, sizeof nb, "%s", nome);
    } else {
      snprintf(ini, sizeof ini, "%d", perfis_ativo());
      snprintf(nb, sizeof nb, i18n("Perfil %d"), perfis_ativo());
    }
    if (p && p->corHex[0] == '#' && strlen(p->corHex) >= 7) {
      unsigned v = (unsigned)strtoul(p->corHex + 1, NULL, 16);
      cr = ((v >> 16) & 255) / 255.0f; cg = ((v >> 8) & 255) / 255.0f; cb = (v & 255) / 255.0f;
    }
    gfx_cor((GfxRect){ ax, cy - 20.0f, 40, 40 }, 0.5f, cr, cg, cb, 1);
    l = txt_linha(TXT_ILHA_INICIAL, ini, 255, 255, 255, 255);
    txt_desenhar(l, ax + (40 - l.w) * 0.5f, cy - l.h * 0.5f);
    n = txt_linha_corta(TXT_AJ_SEG, nb, AJ_TX, ilha.w - 110.0f);
    s = ajTxt(TXT_ILHA_APOIO, "Conta/perfil");
    { float ty = cy - (n.h + 2 + s.h) * 0.5f;
      txt_desenhar(n, ax + 54.0f, ty);
      txt_desenhar_alpha(s, ax + 54.0f, ty + n.h + 2, 0.45f); } }
}

// --- PECAS DA LINHA ---------------------------------------------------------------
static void ajChevron(float xDir, float yc, float al) {
  ajIcone("aj_chevron-right", xDir - 24.0f, yc - 12.0f, 24.0f, al);
}
// Interruptor: o chip "Ligado" no acento a 22% (e estado), "Desligado" neutro.
static float ajChipLiga(float xDir, float yc, int ligado, float a) {
  TxtLinha t;
  float w, ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  if (ligado) {
    t = txt_linha(TXT_AJ_CHIP, "Ligado", (int)(ar * 255), (int)(ag * 255), (int)(ab * 255), 255);
    w = 18 + 8 + 9 + t.w + 18;
    gfx_cor((GfxRect){ xDir - w, yc - 20, w, 40 }, 0.5f, ar, ag, ab, 0.22f * a);
    gfx_cor((GfxRect){ xDir - w + 18, yc - 4, 8, 8 }, 0.5f, ar, ag, ab, a);
    txt_desenhar_alpha(t, xDir - w + 35, yc - t.h * 0.5f, a);
  } else {
    t = ajTxt(TXT_AJ_CHIP, "Desligado");
    w = 18 + t.w + 18;
    ajNeutro((GfxRect){ xDir - w, yc - 20, w, 40 }, 20, 0.07f, AJ_CHIP_SOL, a);
    txt_desenhar_alpha(t, xDir - w + 18, yc - t.h * 0.5f, 0.5f * a);
  }
  return w;
}
// Segmentado: trilho 6% (solido #1D1E23), selecionado 14% (#34363E); editando,
// o item que as setas movem fica cheio no acento, como botao em foco.
static float ajSegLargura(const char *const *rot, const char *const *num, int n) {
  float w = 10.0f;
  int i;
  for (i = 0; i < n; i++) {
    w += (float)txt_largura(TXT_AJ_SEG, i18n(rot[i])) + 40.0f + (i ? 4.0f : 0.0f);
    if (num && num[i]) w += 9.0f + (float)txt_largura(TXT_ILHA_GENERO, num[i]);
  }
  return w;
}
static float ajSeg(const char *const *rot, const char *const *num, int n, int sel, int ed,
                   float x, float yc, float h, float a) {
  float w = ajSegLargura(rot, num, n), ih = h - 10.0f;
  int i;
  ajNeutro((GfxRect){ x, yc - h * 0.5f, w, h }, h * 0.5f, 0.06f, 0.114f, 0.118f, 0.137f, a);
  x += 5.0f;
  for (i = 0; i < n; i++) {
    int ac = i == sel && ed;
    TxtLinha t = ac ? ajTxtT(TXT_AJ_SEG, rot[i]) : ajTxt(TXT_AJ_SEG, rot[i]), q = { 0 };
    float iw = t.w + 40.0f, al = 0.55f;
    if (num && num[i]) { q = ac ? ajTxtT(TXT_ILHA_GENERO, num[i]) : ajTxt(TXT_ILHA_GENERO, num[i]); iw += 9.0f + q.w; }
    if (ac) { ajPilulaAcento((GfxRect){ x, yc - ih * 0.5f, iw, ih }, ih * 0.5f, a); al = 1; }
    else if (i == sel) { ajNeutro((GfxRect){ x, yc - ih * 0.5f, iw, ih }, ih * 0.5f, 0.14f, 0.204f, 0.212f, 0.243f, a); al = 1; }
    txt_desenhar_alpha(t, x + 20, yc - t.h * 0.5f, al * a);
    if (q.w) txt_desenhar_alpha(q, x + 20 + t.w + 9, yc - q.h * 0.5f + 1, (ac ? 0.6f : 0.35f) * a);
    x += iw + 4.0f;
  }
  return w;
}
static float ajFracao(int op, int v) {
  const Opcao *o = &OPCOES[op];
  return o->max > o->min ? (float)(v - o->min) / (float)(o->max - o->min) : 0.0f;
}
// O padrao que a tela MOSTRA. O limite de fileiras mora em fileiras.c e nao
// tem "Restaurar padrao" (uxTemPadrao), mas o risco do 7 no trilho ainda diz
// onde o valor de fabrica fica.
static int ajPadraoVisual(int op) {
  if (uxTemPadrao(op)) return uxValorPadrao(op);
  if (op == AJ_FIL_LIMITE) return FIL_LIMITE_PADRAO;
  return -1;
}
// Pressao da memoria de imagens: usado sobre o teto.
static void corPressao(float t, float *r, float *g, float *b);
#ifdef AJUSTES_TESTE
static int ajMemFixa;   // capturas: os numeros do mockup, sem depender do cache
#endif
static void ajMemUso(long *bytes, long *teto) {
  *bytes = 0; *teto = tex_orcamento_bytes();
  tex_estatisticas(NULL, NULL, bytes, NULL, NULL);
#ifdef AJUSTES_TESTE
  if (ajMemFixa) { *bytes = (long)(151.4 * 1048576); *teto = 240L << 20; }
#endif
}
static float ajPressao(void) {
  long bytes, teto;
  ajMemUso(&bytes, &teto);
  if (teto <= 0) teto = 1;
  return (float)bytes / (float)teto;
}
// O estado curto de uma acao leva o ponto no acento quando e um VINCULO ativo.
static int ajEstadoLigado(int op) {
  return (op == AJ_TRAKT && traktauth_estado() == TRA_LIGADO) ||
         (op == AJ_SIMKL && simklauth_estado() == SMK_LIGADO) ||
         (op == AJ_DISCORD && discord_estado() == DIS_LIGADO);
}
static const char *ajEstadoAcao(int op) {
  static char b[64];
  if (op == AJ_FIL_ORDEM) {
    int home = fil_n_na_home(), fila = fil_n_fila();
    if (fila) snprintf(b, sizeof b, i18n("%d na Home · %d na fila"), home, fila);
    else snprintf(b, sizeof b, i18n("%d na Home"), home);
    return b;
  }
  if (op == AJ_GUIA) { snprintf(b, sizeof b, i18n("%d recursos"), guiaTotal()); return b; }
  if (op == AJ_APOIAR) {
    int i, u = 0;
    b[0] = 0;
    for (i = 0; i < apoio_n() && u < (int)sizeof b; i++)
      u += snprintf(b + u, sizeof b - (size_t)u, "%s%s", i ? " · " : "", apoio_nome(apoio_qual(i)));
    return b;
  }
  return textoLeitura(op);
}

// --- A LINHA (v2) -------------------------------------------------------------------
// So a linha em foco ganha superficie. Cada tipo tem a sua cauda: interruptor
// = chip; escolha = valor + seta, ou o segmentado na linha; numero = valor +
// trilho + seta; acao = estado + seta; leitura = valor apagado, sem seta.
// Medidas do lnV2 do mockup: 96 de altura, raio 28, padding 0 30, rotulo
// 32/500, valor 28, chip 48/22, segmentado 24/600, seta de 30.
static void aj2Chevron(float xDir, float yc, float al) {
  ajIcone("aj_chevron-right", xDir - 22.0f, yc - 11.0f, 22.0f, al);
}
// Estado visual por opcao; persistencia usa sempre o candidato inteiro.
static float ajMovValor[AJ_N];
static unsigned char ajMovPronto[AJ_N];
static void ajMovReiniciar(void) { memset(ajMovPronto, 0, sizeof ajMovPronto); }
static float ajMov(int op, float alvo) {
  if (!ajMovPronto[op] || ajustes_animacoes_reduzidas()) {
    ajMovValor[op] = alvo; ajMovPronto[op] = 1;
  }
  return ajMovValor[op];
}
static float ajMovFracao(int op, int alvo) {
  const Opcao *o = &OPCOES[op];
  float v = ajMov(op, alvo);
  return o->max > o->min ? (v-o->min)/(o->max-o->min) : 0.0f;
}
static void ajMovAtualizar(float dt) {
  int op;
  float k = 1.0f - expf(-22.0f * fmaxf(0.0f, dt));
  for (op = 0; op < AJ_N; op++) if (ajMovPronto[op]) {
    float alvo = uxEditor == 1 && uxOp == op ? uxPendente : uxValorAtual(op);
    if (ajustes_animacoes_reduzidas() || fabsf(alvo-ajMovValor[op]) < 0.001f) ajMovValor[op] = alvo;
    else ajMovValor[op] += (alvo-ajMovValor[op])*k;
  }
}
// A3: o interruptor da linha compacta (64 x 34, botao de 26).
// 2.0.2: sem o "Ligado"/"Desligado" escrito ao lado. O interruptor ja diz o
// estado e o texto repetido em toda linha era o ruido da lista; ligado, o
// trilho fica cheio no acento e o botao na tinta dele (claro ou escuro), para
// ler de longe sem depender do texto.
static float aj2ChipLiga(int op, float xDir, float yc, int ligado, float a) {
  float ar,ag,ab, on = 1.0f-ajMov(op, valor[op]), k = ajTinta() / 255.0f;
  float kr = 1.0f + (k - 1.0f) * on;
  (void)ligado;
  ajustes_acento(&ar,&ag,&ab);
  ajNeutro((GfxRect){xDir-64,yc-17,64,34},17,0.12f,AJ_CHIP_SOL,a);
  gfx_cor((GfxRect){xDir-64,yc-17,64,34},0.5f,ar,ag,ab,0.92f*on*a);
  gfx_cor((GfxRect){xDir-60+30*on,yc-13,26,26},0.5f,kr,kr,kr,(0.6f+0.4f*on)*a);
  return 64.0f;
}
// 2.0.3 (decisao do dono): o interruptor e uma PILULA COM O ESTADO ESCRITO.
// Ligado = pilula clara cheia, ponto escuro, texto escuro. Desligado = so o
// contorno, ponto vazado, texto cinza. Nunca a bolinha sozinha.
static float aj2PilulaEstado(int op, float xDir, float yc, int ligado, float a) {
  const char *rot = ligado ? "Ligado" : "Desligado";
  TxtLinha t = ligado ? txt_linha(TXT_V3_PILL, rot, 22, 22, 26, 255) : ajTxt(TXT_V3_PILL, rot);
  float h = 50.0f, w = 20.0f + 12.0f + 10.0f + t.w + 22.0f;
  GfxRect r = { xDir - w, yc - h * 0.5f, w, h }, d = { r.x + 20.0f, yc - 6.0f, 12.0f, 12.0f };
  (void)op;
  if (ligado) {
    gfx_cor(r, 0.5f, 0.953f, 0.949f, 0.937f, a);
    gfx_cor(d, 0.5f, 0.086f, 0.086f, 0.102f, a);
    txt_desenhar_alpha(t, d.x + 22.0f, yc - t.h * 0.5f, a);
  } else {
    gfx_anel(r, 0.5f, 2.0f, 1, 1, 1, 0.28f * a);
    gfx_anel(d, 0.5f, 2.0f, 1, 1, 1, 0.55f * a);
    txt_desenhar_alpha(t, d.x + 22.0f, yc - t.h * 0.5f, 0.62f * a);
  }
  return w;
}
static float aj2SegLargura(int op, int n) {
  float w = 10.0f;
  int i;
  for (i = 0; i < n; i++) w += (float)txt_largura(TXT_V2_SEG, ajRotSeg(op, i)) + 44.0f + (i ? 4.0f : 0.0f);
  return w;
}
// Segmentado da v2: trilho 6% (solido #1D1E23) com padding 5, itens 24/600 com
// padding 12 22; o selecionado 14% (#34363E), editando cheio no acento.
static float aj2Seg(int op, int n, int sel, int ed, float xDir, float yc, float a) {
  float w = aj2SegLargura(op, n), h = 63.0f, ih = h - 10.0f, x = xDir - w;
  int i;
  ajNeutro((GfxRect){ x, yc - h * 0.5f, w, h }, h * 0.5f, 0.06f, 0.114f, 0.118f, 0.137f, a);
  x += 5.0f;
  { float pos = ajMov(op, sel), left = x, pw = 0, off = 0;
    int lo = (int)floorf(pos), hi;
    if(lo<0) lo=0;
    if(lo>=n) lo=n-1;
    hi=lo+1<n?lo+1:lo;
    for(i=0;i<lo;i++) off += txt_largura(TXT_V2_SEG,ajRotSeg(op,i))+48.0f;
    pw=txt_largura(TXT_V2_SEG,ajRotSeg(op,lo))+44.0f;
    left += off+(pos-lo)*(pw+4.0f);
    pw += (pos-lo)*(txt_largura(TXT_V2_SEG,ajRotSeg(op,hi))+44.0f-pw);
    if(ed) ajPilulaAcento((GfxRect){left,yc-ih*0.5f,pw,ih},ih*0.5f,a);
    else ajNeutro((GfxRect){left,yc-ih*0.5f,pw,ih},ih*0.5f,0.14f,0.204f,0.212f,0.243f,a);
  }
  for (i = 0; i < n; i++) {
    int ac = i == sel && ed;
    TxtLinha t = ac ? ajTxtT(TXT_V2_SEG, ajRotSeg(op, i)) : ajTxt(TXT_V2_SEG, ajRotSeg(op, i));
    float iw = t.w + 44.0f, al = 0.55f;
    if (i == sel) al = 1;
    txt_desenhar_alpha(t, x + 22, yc - t.h * 0.5f, al * a);
    x += iw + 4.0f;
  }
  return w;
}
static void aj2Trilho(float x, float yc, float w, float frac, int foco, float a) {
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  if (frac < 0) frac = 0;
  if (frac > 1) frac = 1;
  gfx_cor((GfxRect){ x, yc - 3, w, 6 }, 0.5f, 1, 1, 1, 0.14f * a);
  if (frac * w >= 1.0f) {
    if (foco) gfx_cor((GfxRect){ x, yc - 3, w * frac, 6 }, 0.5f, ar, ag, ab, a);
    else gfx_cor((GfxRect){ x, yc - 3, w * frac, 6 }, 0.5f, 0.953f, 0.949f, 0.937f, 0.55f * a);
  }
}
static void aj2MiniFundo(int modo, float x, float y, float w, float a);
// Os tres cartoes do Fundo dentro da linha em foco: a tela de Ajustes em
// miniatura em cada fundo, a escolhida em superficie com o check.
static void aj2CartoesFundo(GfxRect ln, float y, float a) {
  static const char *const N[3] = { "Arte", "Arte borrada", "Frost" };
  int i, sel = ajEditandoLinha(AJ_FUNDO) ? uxPendente : valor[AJ_FUNDO];
  float gap = 16.0f, cw = (ln.w - 60.0f - 2 * gap) / 3.0f;
  y += 4.0f;
  for (i = 0; i < 3; i++) {
    float cx = ln.x + 30.0f + i * (cw + gap), mw = cw - 20.0f, mh = mw * 9.0f / 16.0f;
    TxtLinha t = ajTxtC(TXT_V2_SEG, N[i], cw - 50.0f);
    if (i == sel) ajNeutro((GfxRect){ cx, y, cw, 10 + mh + 10 + 29 + 10 }, 20, 0.12f, 0.204f, 0.212f, 0.243f, a);
    aj2MiniFundo(i, cx + 10, y + 10, mw, (i == sel ? 1.0f : 0.7f) * a);
    if (i == sel) {
      ajIcone("aj_check", cx + 10, y + 10 + mh + 10 + (29 - 22) * 0.5f, 22, a);
      txt_desenhar_alpha(t, cx + 10 + 30, y + 10 + mh + 10 + (29 - t.h) * 0.5f, a);
    } else txt_desenhar_alpha(t, cx + 10, y + 10 + mh + 10 + (29 - t.h) * 0.5f, 0.6f * a);
  }
}
static void ajDesenharLinha(int item, float y, float f, float a) {
  int op = TELA[item].op, tipo = ajTipo(op);
  float h = ajLinhaH(item);
  GfxRect fr = aj2ListaR();
  GfxRect ln = { fr.x + 18.0f, y, fr.w - 36.0f, h };
  int desligada = inativa(op), emFoco = f > 0.5f, ed = ajEditandoLinha(op);
  int sub = op == AJ_GUIA, aviso = ajLinhaAviso(item), filha = ajLinhaFilha(item);
  int abre = h > AJ2_LN_SUB + 0.5f && !aviso && !(aj2TemInsp() && aj2FocoNaLista(item) && op != AJ_FUNDO && !(op == AJ_TEMA && ed));
  int beneficio = !sub && !aviso && aj2TemInsp() && aj2FocoNaLista(item) && op != AJ_FUNDO && !(op == AJ_TEMA && ed) && h > AJ2_LN_H + 0.5f;
  float recuo = filha ? 34.0f : 0.0f;
  float aT = a * (desligada ? 0.45f : 1.0f), aTx = a * aj2TextoLista;
  float xDir = ln.x + ln.w - 22.0f, yc = (abre || beneficio || aviso) ? y + AJ2_LN_LINHA1 * 0.5f : y + h * 0.5f;
  float cauda = 0;
  // O valor usa o que o rotulo deixa (A3: a linha e larga; cortar o valor com
  // espaco sobrando era o "Back to home (minimize..." da captura em Montserrat).
  float rotFim = ln.x + 22.0f + recuo + (float)txt_largura(emFoco ? TXT_V3_LN_B : TXT_V3_LN, rotuloOpcao(op)) + 28.0f;
#define AJ_VMAX(xx) ((xx) - rotFim < 160.0f ? 160.0f : (xx) - rotFim)
  if (a <= 0.004f) return;
  // O texto da lista some quando o editor entra por cima (a lista fica so
  // superficie): aTx leva esse alfa; a superficie de foco fica.
  aT *= aj2TextoLista;
  ajFocoLinha(ln, 18.0f, f, aTx);
  // O toque leve do acento na linha em foco (o mesmo do indice).
  if (f > 0.01f) { float ar, ag, ab; ajustes_acento(&ar, &ag, &ab); gfx_cor(ln, 18.0f / ln.h, ar, ag, ab, 0.14f * f * aTx); }
  if (aTx <= 0.004f) return;
  // A3: o segmentado da opcao em foco fica sobre a arte (ajDesenharInspetor);
  // a linha mostra o valor, como as outras escolhas.
  // Sem a arte (#339 "Lista", 120%), o segmentado da escolha em foco fica na linha.
  if ((tipo == AJL_TOG && ed) || (!aj2TemInsp() && ajSegmentada(op) && !desligada && (ed || emFoco))) {
    int n = nValores(op);
    if (n > 3) n = 3;
    cauda = aj2Seg(op, n, ed ? uxPendente : valor[op], ed, xDir, yc, aTx);
  } else if (tipo == AJL_TOG && desligada) {
    // Apagada: o estado escrito, sem pilula (mockup v3, tela 03).
    TxtLinha t = ajTxt(TXT_V3_VAL, lig(op) ? "Ligado" : "Desligado");
    txt_desenhar_alpha(t, xDir - t.w, yc - t.h * 0.5f, 0.72f * aT);
    cauda = t.w;
  } else if (tipo == AJL_TOG) {
    cauda = aj2PilulaEstado(op, xDir, yc, lig(op), aT);
    cauda += ajCustoIcones(op, xDir - cauda - 14.0f, yc, aT) + (ajCusto(op) ? 14.0f : 0.0f);
  } else if (tipo == AJL_ESC || tipo == AJL_NUM) {
    char vb[96];
    const char *v = textoValor(op);
    float x = xDir;
    if (op == AJ_FUNDO && ed) { uxValorTexto(op, uxPendente, vb, sizeof vb); v = vb; }
    if (ajSegmentada(op) && ed) { uxValorTexto(op, uxPendente, vb, sizeof vb); v = vb; }
    if (!desligada && !ajSegmentada(op)) { aj2Chevron(x, yc, (emFoco ? 0.7f : 0.32f) * aT); x -= 22.0f + 12.0f; }
    // A estrela vazada no valor que e o recomendado (o padrao de fabrica).
    if (!desligada && uxTemPadrao(op) && !uxDiferente(op) && !(ed && uxPendente != uxValorPadrao(op))) {
      ajIcone("aj_star", x - 24.0f, yc - 12.0f, 24.0f, (emFoco ? 0.75f : 0.45f) * aT);
      x -= 24.0f + 12.0f;
    }
    if (tipo == AJL_NUM) {
      aj2Trilho(x - 120.0f, yc, 120.0f, ajMovFracao(op, valor[op]), emFoco, aT);
      x -= 120.0f + 14.0f;
    }
    { int fv = marcaDoValor(op, v);
      if (fv >= 0) {
        float mw = marca_formato_largura((FormatoMarca)fv, 30.0f);
        marca_formato((FormatoMarca)fv, x - mw, yc - 15.0f, 30.0f, 1, 1, 1, (emFoco ? 0.85f : 0.6f) * aT);
        x -= mw;
      } else {
        TxtLinha t = ajTxtC(TXT_V3_VAL, v, AJ_VMAX(x) - (op == AJ_TEMA ? 30.0f : 0.0f) - (ajCusto(op) ? 40.0f : 0.0f));
        txt_desenhar_alpha(t, x - t.w, yc - t.h * 0.5f, (emFoco ? 0.95f : 0.72f) * aT);
        x -= t.w;
        if (ajCusto(op) && !desligada) x -= ajCustoIcones(op, x - 10.0f, yc, aT) + 10.0f;
        if (op == AJ_TEMA) {
          float sr, sg, sb;
          ajustes_acento(&sr, &sg, &sb);
          gfx_cor((GfxRect){ x - 10 - 20, yc - 10, 20, 20 }, 0.5f, sr, sg, sb, aT);
          x -= 30.0f;
        }
      } }
    cauda = xDir - x;
  } else if (tipo == AJL_ACAO) {
    const char *st = ajEstadoAcao(op);
    float x = xDir;
    if (!desligada) { aj2Chevron(x, yc, (emFoco ? 0.7f : 0.32f) * aT); x -= 22.0f + 12.0f; }
    if (st && st[0]) {
      TxtLinha t = ajTxtC(TXT_V3_VAL, st, AJ_VMAX(x) - (ajEstadoLigado(op) ? 18.0f : 0.0f));
      txt_desenhar_alpha(t, x - t.w, yc - t.h * 0.5f, (emFoco ? 0.9f : 0.62f) * aT);
      x -= t.w;
      if (ajEstadoLigado(op)) {
        float ar, ag, ab;
        ajustes_acento(&ar, &ag, &ab);
        gfx_cor((GfxRect){ x - 10 - 8, yc - 4, 8, 8 }, 0.5f, ar, ag, ab, aT);
        x -= 18.0f;
      }
    }
    cauda = xDir - x;
  } else {
    float x = xDir;
    char vb[96];
    const char *v = textoValor(op);
    if (op == AJ_ESPACO) {
      float t = ajPressao(), pr, pg, pb;
      long bytes, teto;
      ajMemUso(&bytes, &teto);
      if (t > 1) t = 1;
      corPressao(t, &pr, &pg, &pb);
      gfx_cor((GfxRect){ x - 110, yc - 3, 110, 6 }, 0.5f, 1, 1, 1, 0.12f * aT);
      if (t * 110 > 1) gfx_cor((GfxRect){ x - 110, yc - 3, 110 * t, 6 }, 0.5f, pr, pg, pb, 0.85f * aT);
      x -= 124.0f;
      snprintf(vb, sizeof vb, i18n("%d de %d MB"), (int)(bytes / 1048576), (int)(teto / 1048576));
      v = vb;
    }
    { TxtLinha t = ajTxtC(TXT_V3_VAL, v, AJ_VMAX(x));
      txt_desenhar_alpha(t, x - t.w, yc - t.h * 0.5f, (emFoco ? 0.85f : 0.5f) * aT);
      x -= t.w; }
    cauda = xDir - x;
  }
  // ROTULO, marca do formato, ponto de "diferente do padrao", "AVANCADO".
  if (filha) {   // o canto "└" que liga a filha a mae
    float cx = ln.x + 28.0f;
    gfx_cor((GfxRect){ cx, y + 10.0f, 2.0f, yc - y - 10.0f }, 0, 1, 1, 1, 0.30f * aTx);
    gfx_cor((GfxRect){ cx, yc - 1.0f, 14.0f, 2.0f }, 0, 1, 1, 1, 0.30f * aTx);
  }
  { float tx = ln.x + 22.0f + recuo, maxW = ln.w - 44.0f - recuo - cauda - 16.0f, tyc = yc;
    TxtLinha r = ajTxtC(emFoco ? TXT_V3_LN_B : TXT_V3_LN, rotuloOpcao(op), maxW);
    float rx;
    if (sub) tyc = y + 12.0f + 16.0f;
    if (sub) {
      char t[200];
      TxtLinha s;
      if (op == AJ_GUIA) snprintf(t, sizeof t, "%s", i18n("Tudo o que o app faz, por assunto"));
      else snprintf(t, sizeof t, emFoco ? i18n("Depende de: %s · OK mostra") : i18n("Depende de: %s"),
                    i18n(rotuloOpcao(uxRequisito(op))));
      s = ajTxtC(TXT_V2_18, t, maxW);
      txt_desenhar_alpha(s, tx, y + 12.0f + 32.0f + 4.0f + (22.0f - s.h) * 0.5f, 0.5f * aT);
    }
    txt_desenhar_alpha(r, tx, tyc - r.h * 0.5f, (emFoco ? 1.0f : 0.88f) * aT);
    rx = tx + r.w;
    { int fm = marcaDaOpcao(op);
      if (fm >= 0) {
        float mw = marca_formato_largura((FormatoMarca)fm, 30.0f);
        if (rx + 12 + mw < tx + maxW) {
          marca_formato((FormatoMarca)fm, rx + 12, tyc - 15, 30, 1, 1, 1, (emFoco ? 0.85f : 0.55f) * aT);
          rx += 12 + mw;
        }
      } }
    if (uxDiferente(op) && rx + 20 < tx + maxW) {
      gfx_cor((GfxRect){ rx + 12, tyc - 4, 8, 8 }, 0.5f, 0.953f, 0.949f, 0.937f, 0.45f * aT);
      rx += 20;
    }
    // "NOVO" no acento (e estado) ate a pessoa abrir o registro uma vez.
    if (op == AJ_VER_REGISTRO && !registro_ja_aberto()) {
      int r8, g8, b8;
      float w = ajCaps(TXT_G16B, "Novo", 1.9f, -1, 0, 0);
      ajAcento8(&r8, &g8, &b8);
      if (rx + 16 + w < tx + maxW) {
        char up[40];
        idioma_maiusc_em(ajustes_idioma(), up, sizeof up, i18n("Novo"));
        txt_tracking(TXT_G16B, up, r8, g8, b8, rx + 14, tyc - 10, aT, 1.9f);
      }
    }
    { const char *selo = ajSelo(op);
      if (selo) {
        float w = ajCaps(TXT_V3_SELO, selo, 2.2f, -1, 0, 0);
        if (rx + 18 + w < tx + maxW) ajCaps(TXT_V3_SELO, selo, 2.2f, rx + 18, tyc - 11, 0.42f * aT);
      } }
  }
  // 2.0.3: o que falta para a linha valer, na propria linha (ambar).
  if (aviso) {
    char t[300];
    float tx = ln.x + 22.0f + recuo;
    TxtLinha s;
    ajAvisoTexto(op, t, sizeof t);
    s = txt_linha_corta(TXT_V3_BEN, t, 232, 176, 92, 255, ln.w - 44.0f - recuo - 34.0f);
    gfx_icone((GfxRect){ tx, y + AJ2_LN_LINHA1 - 8.0f + (AJ2_LN_BEN - 24.0f) * 0.5f, 24, 24 }, "aj_triangle-alert", 0.91f, 0.69f, 0.36f, aTx);
    txt_desenhar_alpha(s, tx + 32.0f, y + AJ2_LN_LINHA1 - 8.0f + (AJ2_LN_BEN - s.h) * 0.5f, aTx);
  }
  // 2.0.3: a frase de beneficio (mockup v3: "Ate onde a fonte pode ir...").
  if (beneficio)
    txt_bloco_corta(TXT_V3_BEN, uxBeneficio(op), 243, 242, 239, ln.x + 22.0f + recuo, y + AJ2_LN_LINHA1 - 8.0f,
                    ajBeneficioLargura(), AJ2_LN_BEN, 0.62f * aT, 2);
  // O QUE VEM EMBAIXO numa linha que abre.
  if (abre) {
    float by = y + AJ2_LN_LINHA1 + 20.0f;
    if (op == AJ_FUNDO && aj2FocoNaLista(item)) aj2CartoesFundo(ln, by, aTx);
    else if (!aj2TemInsp() && aj2FocoNaLista(item) && !(op == AJ_TEMA && ed))
      ajAjudaLinhaDesenhar(op, ln.x + 22.0f + recuo, y + AJ2_LN_LINHA1 - 8.0f, aTx);
  }
  // A REGUA DA COR DE DESTAQUE (editando): claros, profundos, dinamicas, Textura.
  if (op == AJ_TEMA && abre && ed) aj2Regua(ln, y + AJ2_LN_LINHA1 + 20.0f, a);
#undef AJ_VMAX
}

// Cabecalho de grupo (grpV2): nome 26/800, contagem 22 a direita, fio de 1 px.
// margin 26 0 6, padding 0 30 12.
// As margens do mockup colapsam: a linha seguinte (margin-top 4) fica a 6 do
// fio, nao a 10 — o grupo leva 2 e a linha os seus 4.
// A3: margem 20 e 8 embaixo (era 26 e 12); o nome continua 26/800 a 100%.
#define AJ2_GRUPO_H (20.0f + 31.2f + 8.0f + 2.0f + 2.0f)
// 2.0.2: sem a contagem a direita (so a nota de um ROTS, "para quem faz o app").
static void ajDesenharGrupo(int item, float y, float a) {
  GfxRect fr = aj2ListaR();
  float x = fr.x + 18.0f, w = fr.w - 36.0f, yc = y + 20.0f + 15.6f;
  TxtLinha t;
  a *= aj2TextoLista;
  if (a <= 0.004f) return;
  { int sec = secDoItem[item];
    // Depois do MAIS(): "Mais opcoes · Escolha da fonte" (mockup v3, tela 07).
    if (maisItem[sec] >= 0 && item > maisItem[sec]) {
      char b[200];
      snprintf(b, sizeof b, i18n("Mais opções · %s"), i18n(TELA[item].titulo));
      t = ajTxtC(TXT_V3_GRP, b, w - 44.0f);
    } else t = ajTxt(TXT_V3_GRP, TELA[item].titulo); }
  txt_desenhar_alpha(t, x + 22, yc - t.h * 0.5f, a);
  if (TELA[item].sub && TELA[item].sub[0]) {
    TxtLinha c = ajTxtC(TXT_V2_24, TELA[item].sub, w - 44.0f - t.w - 24.0f);
    txt_desenhar_alpha(c, x + w - 22 - c.w, yc - c.h * 0.5f + 1.0f, 0.60f * a);
  }
  gfx_cor((GfxRect){ x, y + 20.0f + 31.2f + 8.0f, w, 2 }, 0, 1, 1, 1, 0.18f * a);
}

// Cabecalho da folha: kicker + titulo 40/700 + sub. Devolve onde a lista comeca.
static float ajCabecalhoFolha(GfxRect fr, const char *kick, const char *titulo, const char *sub,
                              float reservaDir, float a) {
  float x = fr.x + 44.0f, y = fr.y + 38.0f;
  ajKicker(kick, x, y + 1, a);
  y += 22.0f;
  { TxtLinha t = ajTxtC(TXT_ILHA_TITULO, titulo, fr.w - 88.0f - reservaDir);
    txt_desenhar_alpha(t, x, y + (46.0f - t.h) * 0.5f, a); }
  y += 46.0f + 8.0f;
  if (sub && sub[0]) {
    float hh = ajBloco(TXT_AJ_SUB, sub, x, y + 2, fr.w - 84.0f, 26.6f, 0.55f * a, 2);
    y += hh > 26.6f ? hh : 26.6f;
  }
  return y + 18.0f;
}

// --- DICAS ---------------------------------------------------------------------------
typedef struct { const char *k, *l; } AjDica;
static float ajDicasLinha(const AjDica *d, int n, float x, float y, float a) {
  int i;
  float x0 = x;
  for (i = 0; i < n; i++) {
    TxtLinha k = ajTxt(TXT_AJ_KBD, d[i].k), l = ajTxt(TXT_ILHA_GENERO, d[i].l);
    float kw = k.w + 18.0f < 34.0f ? 34.0f : k.w + 18.0f;
    ajNeutro((GfxRect){ x, y, kw, 30 }, 15, 0.09f, AJ_CHIP_SOL, a);
    txt_desenhar_alpha(k, x + (kw - k.w) * 0.5f, y + (30 - k.h) * 0.5f, 0.82f * a);
    txt_desenhar_alpha(l, x + kw + 9, y + (30 - l.h) * 0.5f, 0.5f * a);
    x += kw + 9 + l.w + 20.0f;
  }
  return x - x0 - 20.0f;
}
static float ajDicasLargura(const AjDica *d, int n) {
  float w = 0;
  int i;
  for (i = 0; i < n; i++) {
    float kw = (float)txt_largura(TXT_AJ_KBD, i18n(d[i].k)) + 18.0f;
    if (kw < 34.0f) kw = 34.0f;
    w += kw + 9 + (float)txt_largura(TXT_ILHA_GENERO, i18n(d[i].l)) + (i ? 20.0f : 0.0f);
  }
  return w;
}
// Rodape de dicas de uma ilha: fio de 7% e as teclas, com o fundo em yBase.
static void ajDicas(const AjDica *d, int n, float x, float yBase, float w, float a) {
  gfx_cor((GfxRect){ x, yBase - 30.0f - 18.0f, w, 1 }, 0, 1, 1, 1, 0.07f * a);
  ajDicasLinha(d, n, x + 6.0f, yBase - 30.0f, a);
}

// Como ajDicas, mas quebra em mais de uma linha quando as teclas nao cabem em
// `w` (alemao, russo): a ultima linha fica rente a yBase e as outras acima.
static void ajDicasQuebra(const AjDica *d, int n, float x, float yBase, float w, float a) {
  int ini[8], nl = 0, i = 0, r;
  while (i < n && nl < 8) {
    int j = i + 1;
    ini[nl++] = i;
    while (j < n && ajDicasLargura(d + i, j - i + 1) <= w - 12.0f) j++;
    i = j;
  }
  ini[nl] = n;
  gfx_cor((GfxRect){ x, yBase - 30.0f - 18.0f - (nl - 1) * 40.0f, w, 1 }, 0, 1, 1, 1, 0.07f * a);
  for (r = 0; r < nl; r++)
    ajDicasLinha(d + ini[r], (r + 1 < nl ? ini[r + 1] : n) - ini[r], x + 6.0f, yBase - 30.0f - (nl - 1 - r) * 40.0f, a);
}

// Linhas "rotulo ........ valor" com fio de 7% em cima (o .meta do mockup).
static float ajMeta(const char *k, const char *v, float x, float y, float w, float a) {
  TxtLinha lk = ajTxt(TXT_AJ_ESTADO, k);
  TxtLinha lv = ajTxtC(TXT_AJ_ESTADO, v, w - lk.w - 24);
  gfx_cor((GfxRect){ x, y, w, 1 }, 0, 1, 1, 1, 0.07f * a);
  txt_desenhar_alpha(lk, x, y + 13, 0.45f * a);
  txt_desenhar_alpha(lv, x + w - lv.w, y + 13, 0.85f * a);
  return 13 + 20.5f + 13;
}
// Caixa de "o que muda" (.ef): icone e texto corrido sobre 5% (solido #1B1C21).
static float ajEf(const char *icone, const char *s, float x, float y, float w, float a) {
  float tw = w - 32 - 20 - 12, h;
  int linhas = 0;
  { // altura: mede quebrando sem desenhar (alfa 0)
    h = txt_bloco(TXT_AJ_ESTADO, s, 0, 0, 0, x, -4000, tw, 24.65f, 0, 6); (void)linhas; }
  ajNeutro((GfxRect){ x, y, w, h + 28 }, 18, 0.05f, 0.106f, 0.110f, 0.129f, a);
  ajIcone(icone, x + 16, y + 16, 20, 0.55f * a);
  ajBloco(TXT_AJ_ESTADO, s, x + 16 + 20 + 12, y + 14, tw, 24.65f, 0.72f * a, 6);
  return h + 28;
}
#include "ajcena.inc"              // o kit das previas geradas (2.0.2)
#include "ajustes_ux_visual.inc"
#include "ajustes_ux_previa.inc"
#include "ajustes_ux_cenas.inc"     // qual previa gerada cada opcao mostra
#include "ajustes_ux_v2.inc"
#include "ajustes_ux_ilha.inc"
#include "ajustes_ux_fileiras.inc"
#include "ajustes_ux_guia.inc"
#include "ajustes_ux_apoio.inc"
#include "ajustes_ux_secoes.inc"
