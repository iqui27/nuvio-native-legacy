// GUIA DAS NOVIDADES DA 2.0 — ver novidades20.h.
//
// MEDIDAS: as do mockup aprovado (05-whatsnew-20.html), em px de 1080p. O
// palco do mockup tem 1920x1080; a cena da esquerda e uma tela virtual de
// 1280x720 mostrada a 78,125% (1000x562), como o .peek .scn do mockup.
//
// CUSTO NA C9. O fundo e o "Borrada" de fundo.c (a luz assada de 320x180, ja
// em cache por paleta — nenhum desfoque por quadro) com um veu so
// (gfx_veu_css_base: uma camada de tela cheia). A cena e desenhada num alvo
// proprio (gfx_mini_*) e CONGELA: so e redesenhada quando muda (capitulo,
// estado, material, acento) e ate as artes e os textos dela chegarem. Na
// troca, a cena anterior fica no segundo alvo e as duas cruzam por 0,35 s.
// Os dois alvos (2 x 2,2 MB) so existem com o guia aberto.
//
// AS CENAS sao desenhadas com pecas simples (ilha, chip, linha, cartaz, barra)
// e a arte que o pacote ja traz (art/NN.jpg, art/poster/NN.jpg): nao chamam
// home.c, detail.c nem streams.c — aqueles modulos sao estado global da tela.
//
// FOCO SEM CONTORNO (regra do Glass UI): o botao em foco e a pilula cheia no
// acento; o resto e neutro. Material (vidro/solido) e acento seguem Ajustes.
#include "novidades20.h"
#include "novidadesfila.h"
#include "ajustes.h"
#include "anim.h"
#include "corviva.h"
#include "dados.h"
#include "fundo.h"
#include "gfx.h"
#include "idioma.h"
#include "idiomacod.h"
#include "ilha.h"
#include "layout.h"
#include "teclavoltar.h"
#include "logoapp.h"
#include "perfiltv.h"
#include "ponteiro.h"
#include "tex_cache.h"
#include "text.h"
#include "telefonecartao.h"
#include "escala.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ------------------------------------------------------------------ dados
enum { DA = 1, DL = 2, DS = 4, DW = 8, TODOS = 15, ALS = DA | DL | DS };
enum { TG_NADA = 0, TG_ACENTO, TG_AMARELO, TG_CINZA };
typedef struct { const char *t, *tag; int tipo, so; } Linha;
typedef struct {
  int g;
  const char *ic, *nome;
  int ess, exp;
  const char *arte;
  Linha l[3];
  Linha e[6];
  const char *st[3];
  const char *sum;
} Cap;

static const char *const GRUPO[6] = { "Visual", "Navegação", "Reprodução", "Conta e conteúdo", "Experimental", "Sistema" };

static const Cap CAP[N20_NCAP] = {
  { 0, "aj_panel-top", "Visual novo", 1, 0, "00.jpg",
    { { "Todo painel virou uma ilha, igual ao relógio. Vidro ou sólido, você escolhe em <b>Ajustes › Aparência</b>.", 0, 0, 0 },
      { "18 cores de destaque, e o fundo pode ser arte, arte borrada ou Frost.", 0, 0, 0 },
      { "Logo novo (o Clássico continua lá) e três jeitos de abrir o app: Padrão, Só esmaece e Direto.", 0, 0, 0 } },
    { { "Cartão de atualização que cresce da ilha", 0, 0, 0 }, { "Registro do app em painel, com código de envio", 0, 0, 0 },
      { "Textura", 0, 0, 0 }, { "Opacidade do vidro", "Em teste", TG_AMARELO, 0 }, { "Vidro fosco", "Em teste", TG_AMARELO, 0 } },
    { "Vidro · ciano", "Sólido · âmbar", "Vidro · lilás" },
    "Painéis em ilhas, 18 cores, logo novo." },
  { 0, "aj_moon", "Tela de descanso", 1, 0, "05.jpg",
    { { "Com a TV parada, a tela escurece e entra o <b>descanso</b>: Vitrine, Relógio ou Só escurecer.", 0, 0, 0 },
      { "A Vitrine passa títulos do seu catálogo em tela cheia; <b>OK</b> abre o que está na tela.", 0, 0, 0 },
      { "Nada fica parado no mesmo lugar, e o descanso nunca entra com o filme tocando.", 0, 0, 0 } },
    { { "Relógio com a próxima estreia da Agenda", 0, 0, 0 }, { "Vitrine do catálogo ou da sua lista", 0, 0, 0 },
      { "Padrão de 2 min, dá pra desligar", 0, 0, 0 }, { "Brilho dos controles do player", 0, 0, 0 } },
    { "Vitrine", "Relógio", "Só escurecer" },
    "Vitrine e relógio quando a TV fica parada." },
  { 1, "aj_house", "Home e menu", 1, 0, "03.jpg",
    { { "Na Moderna o menu é um trilho flutuante; na Padrão a barra vai de cima a baixo da tela.", 0, 0, 0 },
      { "A Dinâmica mostra um pedaço da fileira de cima, pra você saber que dá pra subir.", 0, 0, 0 },
      { "Biblioteca com setas laterais, Busca com fileira de Pessoas e Agenda em calendário do mês.", 0, 0, 0 } },
    { { "Editor de fileiras com a arte de verdade", 0, 0, 0 }, { "Salvar voa para a ilha e tem Desfazer", 0, 0, 0 },
      { "Esquerda na borda abre o menu", 0, 0, 0 }, { "Menu do cartão (segurar OK)", 0, 0, 0 },
      { "Canais ficam no Guia, não na Home", 0, 0, 0 }, { "Carregamento da Home num painel compacto", 0, 0, 0 } },
    { "Moderna · trilho", "Padrão · barra", NULL },
    "Menu em trilho, Dinâmica, Biblioteca e Agenda novas." },
  { 1, "aj_film", "Página do título", 1, 0, "13.jpg",
    { { "Filme e série no Glass UI: logo, botões, informações e Notas num cartão só.", 0, 0, 0 },
      { "Botão <b>Assistir trailer</b> e uma ilha com os amigos que já viram ou curtiram.", 0, 0, 0 },
      { "Abre na hora com o que o clique já sabia, sem esperar a ficha inteira chegar.", 0, 0, 0 } },
    { { "← → troca de temporada na fileira de episódios", 0, 0, 0 }, { "Logos das produtoras de volta", 0, 0, 0 },
      { "Gráficos de temporada nas séries", 0, 0, 0 }, { "Guia parental sai da ilha do relógio", 0, 0, 0 },
      { "Zoom do trailer", "Em teste · Samsung .tpk", TG_AMARELO, DS }, { "Explorar: climas e títulos parecidos", 0, 0, 0 } },
    { "Filme", "Série · episódios", NULL },
    "Página nova, trailer, amigos e Notas." },
  { 2, "aj_captions", "Player e legendas", 1, 0, "08.jpg",
    { { "Áudio e Legendas abrem da ilha do canto, sem cobrir o filme.", 0, 0, 0 },
      { "Segunda legenda independente e <b>AutoSync</b>: Rápido, Minucioso ou Desfazer.", 0, 0, 0 },
      { "Avançar ficou previsível: segurar acelera aos poucos, toque rápido é sempre 10 s.", 0, 0, 0 } },
    { { "AutoSync pelo áudio", "Só Android", TG_CINZA, DA }, { "Legendas em árabe com RTL", 0, 0, 0 },
      { "Próximo episódio acha os créditos pelo capítulo do MKV", 0, 0, 0 }, { "Cápsula do Seekr ao arrastar", 0, 0, 0 },
      { "Legenda automática corrigida", 0, 0, 0 }, { "TV ao vivo com folhas que saem do relógio", 0, 0, 0 } },
    { "Seletor de legendas", "AutoSync", "Arrastando a barra" },
    "Legendas pela ilha, AutoSync, seek melhor." },
  { 2, "aj_zap", "Fontes e desempenho", 1, 0, "05.jpg",
    { { "Folha de Fontes com <b>Melhor para esta TV</b> primeiro, grupos por qualidade e filtros Só MP4, Em cache e Dublado.", 0, 0, 0 },
      { "A escolha automática pesa HDR e Dolby Vision, e não prefere mais MP4 de plugin a cache de debrid.", 0, 0, 0 },
      { "O StreamFit mede a rede da TV e só rebaixa a qualidade quando tem certeza.", 0, 0, 0 } },
    { { "Prioridade e HDR em Ajustes", 0, 0, 0 }, { "Pacotes de selos", 0, 0, 0 }, { "Home abre mais rápido", 0, 0, 0 },
      { "Catálogo em disco: 26,9 → 1,9 MB", 0, 0, 0 }, { "Cache de busca 256/512/1024 MB", "Só Android", TG_CINZA, DA },
      { "Volume até 200%", "Só Android", TG_CINZA, DA } },
    { "Folha de Fontes", "Memória de imagens", NULL },
    "Fontes por TV, StreamFit, Home e catálogo mais leves." },
  { 3, "aj_users", "Social e amigos", 1, 0, "21.jpg",
    { { "Você vira uma pessoa só: Trakt, Simkl e Letterboxd ligados à mesma pessoa, sem amigo nem atividade em dobro.", 0, 0, 0 },
      { "Quem já viu um título aparece no pôster, no hero e na página do título.", 0, 0, 0 },
      { "Recomende, responda <b>Já assisti</b>, e receba enquete no relógio (dá pra desligar).", 0, 0, 0 } },
    { { "Pedido de amizade avisa no relógio", 0, 0, 0 }, { "Recusar não avisa a pessoa", 0, 0, 0 },
      { "Assistidas sincronizam sozinhas", 0, 0, 0 }, { "Lista com capa maior", 0, 0, 0 },
      { "Painel Social abre com o Azul", 0, 0, 0 }, { "Enquetes", "Depende do servidor", TG_CINZA, 0 } },
    { "Painel Social", "Amigos no pôster · enquete", NULL },
    "Pessoa única, amigos nos pôsteres, enquetes." },
  { 3, "aj_user-round", "Perfis 2.0", 1, 0, "00.jpg",
    { { "Fundo novo: <b>Filmes</b>, uma parede com os cartazes do que cada perfil assistiu.", 0, 0, 0 },
      { "Mostra o <b>Continuar assistindo</b> de quem está em foco. Perfil com PIN não mostra.", 0, 0, 0 },
      { "Plugins, enquetes e servidor pessoal acompanham o perfil que você escolheu.", 0, 0, 0 } },
    { { "Troca de perfil sem tela em branco", 0, 0, 0 }, { "Preferências de desempenho ficam por aparelho", 0, 0, 0 },
      { "Fundos Luz e Projetor", 0, 0, 0 } },
    { "Perfil 1", "Perfil 2", "Perfil com PIN" },
    "Parede de filmes e Continuar de cada perfil." },
  { 3, "aj_server", "Servidores pessoais", 0, 0, "13.jpg",
    { { "<b>Jellyfin</b> (login ou Quick Connect), <b>Emby</b> e <b>Plex</b> (login por PIN) como fontes da sua casa.", 0, 0, 0 },
      { "Suas bibliotecas viram fileiras na Home, com detalhes e retomar de onde parou.", 0, 0, 0 },
      { "O que você assiste volta pro servidor: o visto e o ponto onde parou.", 0, 0, 0 } },
    { { "Quick Connect no Jellyfin", 0, 0, 0 }, { "Emby usa o mesmo protocolo do Jellyfin", 0, 0, 0 },
      { "Plex: lista de servidores da conta", 0, 0, 0 } },
    { "Jellyfin · Quick Connect", "Emby", "Plex · PIN" },
    "Jellyfin, Emby e Plex na Home." },
  { 4, "aj_puzzle", "Plugins e P2P", 0, 1, "08.jpg",
    { { "<b>Plugins</b>: repositórios de scrapers do Nuvio rodam como mais uma fonte, ao lado dos addons.", "Experimental", TG_AMARELO, ALS },
      { "<b>P2P</b>: o motor embutido toca o torrent que você escolher à mão. Nunca entra na escolha automática.", "Experimental", TG_AMARELO, ALS },
      { "Os dois vêm <b>desligados</b>. Liga em Ajustes, com limite de disco e de memória.", "Desligado", TG_AMARELO, 0 } },
    { { "Repositórios CloudStream ficam na conta, mas não rodam aqui", 0, 0, 0 },
      { "P2P some no .wgt / Tizen 4–5", "Indisponível", TG_CINZA, 0 },
      { "Para quando falta disco: o app avisa o motivo", 0, 0, 0 } },
    { "Plugins", "P2P", NULL },
    "Plugins e P2P, desligados por padrão." },
  { 5, "aj_sliders-horizontal", "Ajustes", 1, 0, "03.jpg",
    { { "Menu compacto, categorias agrupadas, busca e <b>prévia de verdade</b> em cada opção.", 0, 0, 0 },
      { "Um único <b>Mostrar opções avançadas</b> no lugar de um em cada seção.", 0, 0, 0 },
      { "Opção inativa explica por quê. E o <b>Guia de uso</b> mora aqui dentro.", 0, 0, 0 } },
    { { "Gráfico de memória das imagens", 0, 0, 0 }, { "Fileiras da Home", 0, 0, 0 }, { "Tamanho padrão 80%", 0, 0, 0 },
      { "Textos faltantes traduzidos nos 28 idiomas", 0, 0, 0 }, { "Registro com consentimento", 0, 0, 0 },
      { "Teclado novo, com QR para digitar no celular", 0, 0, 0 } },
    { "Índice com prévia", "Memória e avançado", NULL },
    "Ajustes mais curtos, com prévia e Guia de uso." },
};

// ------------------------------------------------------------------ estado
static int aberto, decidido, novo, pedido, essencial, devForcado = -1;
static int lista[N20_NCAP + 3], nLista, idx;
static int est[N20_NCAP];
static int focoHero, focoFim, saindo, focoSair;
static float entrada, troca = 1.0f, sairA;
static char dirArte[512] = "deploy/app/art";
#ifdef NV_TOUCH_UI
static TelefoneCartao n20Telefone;
#endif

// ------------------------------------------------------------------ aparelho
static int devBuild(void) {
#if defined(NV_TPK40)
  return N20_DEV_WGT;
#else
  switch (ptv_plataforma()) {
    case PTV_ANDROID: return N20_DEV_ANDROID;
    case PTV_TPK: return N20_DEV_TPK;
    case PTV_TIZEN: return N20_DEV_WGT;
    default: return N20_DEV_LG;
  }
#endif
}
static int dev(void) { return devForcado >= 0 && devForcado < N20_NDEV ? devForcado : devBuild(); }
static int temAqui(int so) { return !so || (so & (1 << dev())); }
static const char *devNome(void) {
  switch (dev()) {
    case N20_DEV_ANDROID: return "Android TV";
    case N20_DEV_TPK: return "Samsung Tizen 6+";
    case N20_DEV_WGT:
#if defined(NV_TPK40)
      return "Samsung Tizen 4–5";
#else
      return "Samsung .wgt";
#endif
    default: return "LG webOS";
  }
}

// ------------------------------------------------------------------ material
#define TX 243, 242, 239
static int sol;   // forca o solido (a cena "Sólido" do capitulo Visual)
static float acR = -1, acG, acB;   // acento da cena (-1 = o de Ajustes)
static int vid(void) { return ajustes_vidro() && !sol; }
static void acento(float *r, float *g, float *b) {
  if (acR >= 0) { *r = acR; *g = acG; *b = acB; return; }
  ajustes_acento(r, g, b);
}
static int tintaAc(void) {
  float r, g, b;
  if (acR < 0) return ajustes_tinta_foco();
  acento(&r, &g, &b);
  return 0.2126f * r + 0.7152f * g + 0.0722f * b > 0.55f ? 16 : 250;
}
static void acento8(int *r, int *g, int *b) {
  float ar, ag, ab;
  if (acR >= 0) acento(&ar, &ag, &ab);
  else ajustes_acento_marca(&ar, &ag, &ab);
  *r = (int)(ar * 255.0f + 0.5f); *g = (int)(ag * 255.0f + 0.5f); *b = (int)(ab * 255.0f + 0.5f);
}
// Ilha: o material de Ajustes (sombra curta, miolo vidro/solido, luz do canto).
static int ilhaModal;   // 1 = a ilha por cima de outra tela (o dialogo): miolo quase opaco
static void ilhaM(GfxRect r, float raioPx, float a) {
  float raio = raioPx / r.h;
  if (a <= 0.003f) return;
  gfx_sombra_sob((GfxRect){ r.x - 20, r.y - 6, r.w + 40, r.h + 46 }, 1.0f, 0.0f, 0.5f, 0, 0, 0,
                 (vid() ? 0.36f : 0.45f) * a, r, raioPx, !vid() && a >= 0.999f ? 1.0f : 0.0f);
  if (vid()) {
    float m = (ilhaModal ? 1.17f : 0.80f) * gfx_vidro_opacidade();
    gfx_vidro_miolo(r, raio, 0.055f, 0.059f, 0.071f, (m > 0.97f ? 0.97f : m) * a, a);
    gfx_luz_canto(r, raio, r.w * 0.22f, -r.h * 0.40f, r.h * 0.62f, 1, 1, 1, 0.10f * a);
  } else gfx_cor(r, raio, 0.082f, 0.086f, 0.102f, a);
}
// Neutro claro: branco a `vidA` no vidro, o cinza opaco no solido.
static void neutro(GfxRect r, float raioPx, float vidA, float a) {
  if (vid()) gfx_cor(r, raioPx / r.h, 1, 1, 1, vidA * a);
  else gfx_cor(r, raioPx / r.h, 0.141f, 0.149f, 0.173f, a);
}
static void pilulaAc(GfxRect r, float a) {
  float ar, ag, ab;
  acento(&ar, &ag, &ab);
  gfx_rect((GfxRect){ r.x - 14, r.y - 2, r.w + 28, r.h + 34 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, ar, ag, ab, 0.34f * a);
  gfx_cor(r, 0.5f, ar, ag, ab, a);
}
static TxtLinha txt(TxtEstilo e, const char *s) { return txt_linha(e, s, TX, 255); }
static TxtLinha txtC(TxtEstilo e, const char *s, float w) { return txt_linha_corta(e, s, TX, 255, w); }
static TxtLinha txtT(TxtEstilo e, const char *s) { int t = tintaAc(); return txt_linha(e, s, t, t, t, 255); }
static TxtLinha txtA(TxtEstilo e, const char *s) { int r, g, b; acento8(&r, &g, &b); return txt_linha(e, s, r, g, b, 255); }
static float caps(TxtEstilo e, const char *s, float track, float x, float y, int r, int g, int b, float a) {
  char up[300];
  idioma_maiusc_em(ajustes_idioma(), up, sizeof up, i18n(s));
  return txt_tracking(e, up, r, g, b, x, y, a, track);
}
static float capsA(TxtEstilo e, const char *s, float track, float x, float y, float a) {
  int r, g, b;
  acento8(&r, &g, &b);
  return caps(e, s, track, x, y, r, g, b, a);
}
static float capsLarg(TxtEstilo e, const char *s, float track) { return caps(e, s, track, -1, 0, TX, 0); }
static void icone(const char *n, GfxRect r, float a) { gfx_icone(r, n, 0.953f, 0.949f, 0.937f, a); }
static void iconeA(const char *n, GfxRect r, float a) { float ar, ag, ab; acento(&ar, &ag, &ab); gfx_icone(r, n, ar, ag, ab, a); }
static void iconeT(const char *n, GfxRect r, float a) { float k = tintaAc() / 255.0f; gfx_icone(r, n, k, k, k, a); }
static void txtMeio(TxtLinha l, float x, float y, float h, float a) { txt_desenhar_alpha(l, x, y + (h - l.h) * 0.5f, a); }

// ------------------------------------------------------------------ arte
static int faltas;   // texturas pedidas e ainda nao prontas (a cena nao congela)
static char caminho[10][600];
static int nCaminho;
static const char *arte(const char *rel) {
  char *b = caminho[nCaminho++ % 10];
  snprintf(b, sizeof caminho[0], "%s/%s", dirArte, rel);
  return b;
}
static void img(const char *rel, GfxRect r, float raioPx, float a) {
  const char *c = arte(rel);
  GLuint t = tex_obter_larg(c, r.w > r.h * 1.78f ? r.w : r.h * 1.78f);
  if (!t) {
    gfx_cor(r, raioPx / r.h, 0.10f, 0.11f, 0.13f, a);
    if (!tex_falhou(c)) faltas++;
    return;
  }
  gfx_tex_aspect_atual = tex_aspecto(c);
  gfx_card_forcar_cover_atual = 1.0f;
  gfx_rect(r, t, GFX_CARD, 1.0f, 0, 0, raioPx / r.h, 0, 0, 0, a);
  gfx_card_forcar_cover_atual = 0.0f;
  gfx_tex_aspect_atual = 0.0f;
}
// Imagem de marca (logo do app) inteira em maxW x maxH; devolve a largura.
static float marca(const char *c, float x, float y, float maxW, float maxH, int centro, float a) {
  GLuint t = tex_obter_larg(c, maxW);
  float asp, w, h;
  if (!t) { if (!tex_falhou(c)) faltas++; return 0; }
  asp = tex_aspecto(c);
  if (asp <= 0.0f) asp = 1.0f;
  w = maxW; h = w / asp;
  if (h > maxH) { h = maxH; w = h * asp; }
  if (centro) x -= w * 0.5f;
  gfx_tex_aspect_atual = asp;
  gfx_rect((GfxRect){ x, y, w, h }, t, GFX_ARTE, 0, 0, 0, 0, 1, 1, 1, a);
  gfx_tex_aspect_atual = 0.0f;
  return w;
}
// O "Borrada" de fundo.c precisa da PALETA da arte, e a paleta so e anotada
// quando a arte e decodificada como heroi (tex_cache.c). A tela cheia do guia
// pede a arte do capitulo assim (uma de cada vez, despejavel); a cena so
// espera a paleta chegar (sem ela, a arte nitida com veu, e a cena nao congela).
static void fundoArte(const char *rel, GfxRect r, float raioPx, float a) {
  const char *c = arte(rel);
  CorvivaPaleta p;
  int tela = r.w >= NV_TELA_W - 1.0f, tem = corviva_paleta(c, &p);
  if (!(tela ? tex_obter_hero(c) : tex_obter_larg(c, 640.0f)) && !tex_falhou(c)) faltas++;
  if (tem && p.ok) fundo_desenhar_modo(FUNDO_BORRADA, r, raioPx, c, a);
  // Arte sem cor (quase preta ou cinza): o Frost, que e a luz do acento.
  else if (tem || tex_falhou(c)) fundo_desenhar_modo(FUNDO_FROST, r, raioPx, NULL, a);
  else { faltas++; gfx_cor(r, raioPx / r.h, 0.039f, 0.043f, 0.055f, a); }
}
static void pedirArtes(void) {
  static const char *const A[] = { "00.jpg", "03.jpg", "05.jpg", "08.jpg", "13.jpg", "21.jpg" };
  int i;
  for (i = 0; i < 6; i++) tex_obter_larg(arte(A[i]), 640.0f);
}

// ------------------------------------------------------------------ texto rico
// Frase com trechos em <b>...</b>, quebrada em `w`. Desenha (a >= 0) ou so
// mede; devolve a altura e, em *fimX, onde a ultima linha terminou (para a
// etiqueta que vem logo depois). Cada linha vira no maximo um punhado de
// trechos (regular/negrito alternados), nao uma textura por palavra.
typedef struct { char s[240]; int neg; float w, gap; } Trecho;
// Largura de um espaco no estilo: o texto nao desenha espaco na ponta da linha.
static float espacoW(TxtEstilo e) { return (float)(txt_largura(e, "a a") - txt_largura(e, "aa")); }
static float rico(const char *s, TxtEstilo reg, TxtEstilo neg, float x, float y, float w, float lead,
                  float aReg, float aNeg, float *fimX, int maxL) {
  Trecho tr[8];
  int nt = 0, b = 0, linhas = 0, espaco = 0;
  float usado = 0;
  const char *p = s;
  if (fimX) *fimX = x;
  memset(tr, 0, sizeof tr);
#define FECHA_LINHA() do { int k; float xx = x; \
    if (aReg >= 0 && (maxL <= 0 || linhas < maxL)) for (k = 0; k < nt; k++) { \
      TxtLinha l = tr[k].neg ? txt_linha(neg, tr[k].s, 255, 255, 255, 255) : txt_linha(reg, tr[k].s, TX, 255); \
      xx += tr[k].gap; \
      txt_desenhar_alpha(l, xx, y + linhas * lead + (lead - l.h) * 0.5f, tr[k].neg ? aNeg : aReg); xx += tr[k].w; } \
    if (fimX) *fimX = x + usado; linhas++; nt = 0; usado = 0; } while (0)
  while (*p) {
    size_t n;
    if (*p == ' ' || *p == '\n') { espaco = 1; p++; continue; }
    if (!strncmp(p, "<b>", 3)) { b = 1; p += 3; continue; }
    if (!strncmp(p, "</b>", 4)) { b = 0; p += 4; continue; }
    n = txt_token_tam(p);
    if (!n) n = 1;
    { const char *lt = memchr(p, '<', n);
      if (lt && lt > p) n = (size_t)(lt - p);
      else if (lt == p) n = 1; }
    {
      char cand[260];
      float wc;
      int novo = !nt || tr[nt - 1].neg != b;
      const char *base = novo ? "" : tr[nt - 1].s;
      float gap = novo && nt && espaco ? espacoW(b ? neg : reg) : 0.0f;
      snprintf(cand, sizeof cand, "%s%s%.*s", base, (!novo && espaco) ? " " : "", (int)n, p);
      wc = (float)txt_largura(b ? neg : reg, cand);
      if (nt && usado - (novo ? 0 : tr[nt - 1].w) + gap + wc > w) {
        FECHA_LINHA();
        snprintf(cand, sizeof cand, "%.*s", (int)n, p);
        wc = (float)txt_largura(b ? neg : reg, cand);
        novo = 1;
        gap = 0;
      }
      if (novo) {
        if (nt < 8) { snprintf(tr[nt].s, sizeof tr[nt].s, "%s", cand); tr[nt].neg = b; tr[nt].w = wc; tr[nt].gap = gap; nt++; usado += wc + gap; }
      } else {
        usado += wc - tr[nt - 1].w;
        snprintf(tr[nt - 1].s, sizeof tr[nt - 1].s, "%s", cand);
        tr[nt - 1].w = wc;
      }
    }
    espaco = 0;
    p += n;
  }
  if (nt) FECHA_LINHA();
#undef FECHA_LINHA
  if (maxL > 0 && linhas > maxL) linhas = maxL;
  return linhas * lead;
}

// ------------------------------------------------------------------ pecas das cenas
// Tudo em unidades da tela virtual 1280x720 da cena.
static void sIlha(GfxRect r, float raio) {
  if (vid()) {
    gfx_cor(r, raio / r.h, 0.06f, 0.065f, 0.08f, 0.42f);
    gfx_cor(r, raio / r.h, 1, 1, 1, 0.08f);
    gfx_luz_canto(r, raio / r.h, r.w * 0.22f, -r.h * 0.4f, r.h * 0.6f, 1, 1, 1, 0.08f);
  } else gfx_cor(r, raio / r.h, 0.118f, 0.125f, 0.149f, 1);
}
static float sCaps(const char *s, float x, float y, float a) { return caps(TXT_AJ_CAPS13, s, 2.2f, x, y, TX, 0.5f * a); }
// Chip: 0 neutro, 1 no acento, 2 contornado (vazado).
static float sChip(const char *s, float x, float y, float h, int modo, TxtEstilo e) {
  TxtLinha t = modo == 1 ? txtT(e, s) : txt(e, s);
  float w = t.w + h * 0.86f;
  float ar, ag, ab;
  acento(&ar, &ag, &ab);
  if (modo == 1) gfx_cor((GfxRect){ x, y, w, h }, 0.5f, ar, ag, ab, 1);
  else if (modo == 2) {
    gfx_cor((GfxRect){ x, y, w, h }, 0.5f, 1, 1, 1, 0.22f);
    gfx_cor((GfxRect){ x + 1.5f, y + 1.5f, w - 3, h - 3 }, 0.5f, 0.07f, 0.075f, 0.09f, vid() ? 0.55f : 1.0f);
  } else gfx_cor((GfxRect){ x, y, w, h }, 0.5f, 1, 1, 1, 0.10f);
  txtMeio(t, x + h * 0.43f, y, h, 1);
  return w;
}
static void sRowFoco(GfxRect r, float raio) { gfx_cor(r, raio / r.h, 1, 1, 1, vid() ? 0.14f : 0.10f); }
static void sTog(float x, float y, int on) {
  float ar, ag, ab;
  acento(&ar, &ag, &ab);
  if (on) gfx_cor((GfxRect){ x, y, 64, 36 }, 0.5f, ar, ag, ab, 1);
  else gfx_cor((GfxRect){ x, y, 64, 36 }, 0.5f, 1, 1, 1, 0.2f);
  if (on) { float k = tintaAc() / 255.0f; gfx_cor((GfxRect){ x + 32, y + 4, 28, 28 }, 0.5f, k, k, k, 1); }
  else gfx_cor((GfxRect){ x + 4, y + 4, 28, 28 }, 0.5f, 1, 1, 1, 1);
}
static void sBar(GfxRect r, float f, float aBar) {
  float ar, ag, ab;
  acento(&ar, &ag, &ab);
  gfx_cor(r, 0.5f, 1, 1, 1, 0.2f);
  if (f > 0) gfx_cor((GfxRect){ r.x, r.y, r.w * f, r.h }, 0.5f, ar, ag, ab, aBar);
}
static void sPoster(const char *n, float x, float y, float w) {
  char rel[40];
  snprintf(rel, sizeof rel, "poster/%s.jpg", n);
  img(rel, (GfxRect){ x, y, w, w * 1.5f }, 14, 1);
}
static void sLand(const char *n, GfxRect r, int foco) {
  char rel[40];
  float ar, ag, ab;
  acento(&ar, &ag, &ab);
  if (foco) gfx_cor((GfxRect){ r.x - 6, r.y - 6, r.w + 12, r.h + 12 }, 22.0f / (r.h + 12), ar, ag, ab, 1);
  snprintf(rel, sizeof rel, "%s.jpg", n);
  img(rel, r, 16, 1);
}
static void sAv(float r, float g, float b, const char *l, float x, float y, float d, int presenca) {
  TxtLinha t = txt_linha(TXT_ILHA_INICIAL, l, 16, 18, 22, 255);
  gfx_cor((GfxRect){ x, y, d, d }, 0.5f, r, g, b, 1);
  txt_desenhar(t, x + (d - t.w) * 0.5f, y + (d - t.h) * 0.5f);
  if (presenca) {
    gfx_cor((GfxRect){ x + d - 15, y + d - 15, 16, 16 }, 0.5f, 0.106f, 0.114f, 0.133f, 1);
    gfx_cor((GfxRect){ x + d - 12, y + d - 12, 10, 10 }, 0.5f, 0.325f, 0.878f, 0.545f, 1);
  }
}
static void sRelogio(void) {
  TxtLinha t = txt(TXT_ILHA_ITEM, "21:40");
  float w = 24 + 10 + 12 + t.w + 24, ar, ag, ab;
  GfxRect r = { 1280 - 34 - w, 28, w, 54 };
  acento(&ar, &ag, &ab);
  sIlha(r, 27);
  gfx_cor((GfxRect){ r.x + 24, r.y + 22, 10, 10 }, 0.5f, ar, ag, ab, 1);
  txtMeio(t, r.x + 46, r.y, 54, 1);
}
static void sTag(const char *s, float x, float y, int modo) {   // 0 cinza, 1 acento, 2 amarelo
  TxtLinha t = modo == 1 ? txtT(TXT_AJ_CAPS13, s) : modo == 2 ? txt_linha(TXT_AJ_CAPS13, s, 36, 26, 2, 255) : txt(TXT_AJ_CAPS13, s);
  float ar, ag, ab;
  acento(&ar, &ag, &ab);
  if (modo == 1) gfx_cor((GfxRect){ x, y, t.w + 16, 24 }, 7.0f / 24, ar, ag, ab, 1);
  else if (modo == 2) gfx_cor((GfxRect){ x, y, t.w + 16, 24 }, 7.0f / 24, 0.949f, 0.804f, 0.392f, 1);
  else gfx_cor((GfxRect){ x, y, t.w + 16, 24 }, 7.0f / 24, 1, 1, 1, 0.16f);
  txtMeio(t, x + 8, y, 24, 1);
}
static void sVeu(GfxRect r, int borda, float a) { gfx_veu_css(r, borda, 1.0f, 1.0f, a); }
static void sTxt(TxtEstilo e, const char *s, float x, float y, float a) { txt_desenhar_alpha(txt(e, s), x, y, a); }

// ================================================================== CENAS
static void cenaVisual(int s) {
  static const float AC[3][3] = { { 0.533f, 0.878f, 0.965f }, { 0.949f, 0.804f, 0.392f }, { 0.780f, 0.733f, 0.941f } };
  static const unsigned SW[18] = { 0x88e0f6, 0x7ab8ff, 0x9aa6ff, 0xc7bbf0, 0xe3a8f0, 0xfeb5b1, 0xff9c8f, 0xf6a870, 0xf2cd64,
                                   0xd8e66b, 0xa8e66b, 0x83e5bc, 0x6fe0d0, 0x6bc8e8, 0xb0b6bf, 0xe8e2d4, 0xffd3a6, 0xffb0d0 };
  static const int SEL[3] = { 0, 8, 3 };
  GfxRect il = { 46, 116, 690, 560 };
  float y, x;
  int i;
  sol = s == 1;
  acR = AC[s][0]; acG = AC[s][1]; acB = AC[s][2];
  fundoArte("00.jpg", (GfxRect){ 0, 0, 1280, 720 }, 0, 1);
  marca(logoapp_caminho(logoapp_atual(), LOGO_F_HORIZONTAL), 46, 34, 260, 50, 0, 1);
  sRelogio();
  sIlha(il, 40);
  sCaps("Ajustes › Aparência", il.x + 34, il.y + 30, 1);
  y = il.y + 76; x = il.x + 34;
#define ROT(s_) sTxt(TXT_ILHA_ITEM, s_, x, y + 8, 1)
  ROT("Material");
  { float cx = x + 210; cx += sChip("Vidro", cx, y, 42, s == 1 ? 2 : 1, TXT_G18M) + 12; sChip("Sólido", cx, y, 42, s == 1 ? 1 : 2, TXT_G18M); }
  y += 72;
  ROT("Cor de destaque");
  for (i = 0; i < 18; i++) {
    float sx = x + 210 + (i % 9) * 44, sy = y - 4 + (i / 9) * 44;
    float r = ((SW[i] >> 16) & 255) / 255.0f, g = ((SW[i] >> 8) & 255) / 255.0f, b = (SW[i] & 255) / 255.0f;
    if (i == SEL[s]) gfx_cor((GfxRect){ sx - 3, sy - 3, 42, 42 }, 0.5f, 1, 1, 1, 1);
    gfx_cor((GfxRect){ sx, sy, 36, 36 }, 0.5f, r, g, b, 1);
  }
  y += 110;
  ROT("Fundo");
  { float cx = x + 210; cx += sChip("Arte", cx, y, 42, 2, TXT_G18M) + 12; cx += sChip("Borrada", cx, y, 42, 1, TXT_G18M) + 12; sChip("Frost", cx, y, 42, 2, TXT_G18M); }
  y += 72;
  ROT("Logo");
  { float cx = x + 210; cx += sChip("Novo", cx, y, 42, 1, TXT_G18M) + 12; sChip("Clássico", cx, y, 42, 2, TXT_G18M); }
  y += 72;
  ROT("Abertura");
  { float cx = x + 210; cx += sChip("Padrão", cx, y, 42, 1, TXT_G18M) + 12; cx += sChip("Só esmaece", cx, y, 42, 2, TXT_G18M) + 12; sChip("Direto", cx, y, 42, 2, TXT_G18M); }
#undef ROT
  { GfxRect a = { 1280 - 34 - 420, 100, 420, 176 };
    sIlha(a, 34);
    sCaps("Atualização", a.x + 26, a.y + 22, 1);
    sTxt(TXT_G26B, "Nuvio 2.0 pronta", a.x + 26, a.y + 44, 1);
    sTxt(TXT_G18R, "Cresce da ilha do relógio", a.x + 26, a.y + 82, 0.55f);
    sChip("Instalar agora", a.x + 26, a.y + 116, 42, 1, TXT_G18M); }
  { GfxRect p = { 1280 - 34 - 420, 300, 420, 376 };
    sIlha(p, 34);
    sCaps("Prévia", p.x + 26, p.y + 24, 1);
    sPoster("02", p.x + 26, p.y + 56, 104); sPoster("12", p.x + 144, p.y + 56, 104); sPoster("15", p.x + 262, p.y + 56, 104);
    sBar((GfxRect){ p.x + 26, p.y + 236, 368, 8 }, 0.58f, 1);
    sTxt(TXT_G18R, "Continuar assistindo · 58%", p.x + 26, p.y + 258, 0.55f); }
}

static void cenaDescanso(int s) {
  if (s == 0) {   // Vitrine: arte inteira, logo/titulo e meta a esquerda
    img("05.jpg", (GfxRect){ 0, 0, 1280, 720 }, 0, 1);
    sVeu((GfxRect){ 0, 300, 1280, 420 }, 0, 0.75f);
    gfx_cor((GfxRect){ 0, 0, 1280, 720 }, 0, 0, 0, 0, 0.18f);
    sCaps("No seu catálogo", 70, 410, 1);
    sTxt(TXT_W20_HERO, "Título", 66, 440, 1);
    sTxt(TXT_G18R, "Filme", 70, 590, 0.6f);
    { TxtLinha ok = txt(TXT_ILHA_APOIO, "OK para abrir");
      GfxRect r = { 1280 - 70 - ok.w - 52, 620, ok.w + 52, 44 };
      sIlha(r, 22); txtMeio(ok, r.x + 26, r.y, 44, 1); }
  } else {
    gfx_cor((GfxRect){ 0, 0, 1280, 720 }, 0, 0, 0, 0, 1);
    if (s == 1) {   // Relogio: numeral fino, data e a proxima estreia
      TxtLinha h = txt(TXT_DESC_HORA, "21:47"), d = txt(TXT_V2_26, "domingo"), p = txt(TXT_G18R, "Série · T1 E3");
      txt_desenhar_alpha(h, 640 - h.w * 0.5f, 170, 0.62f);
      txt_desenhar_alpha(d, 640 - d.w * 0.5f, 180 + h.h, 0.45f);
      sCaps("Próxima estreia", 640 - capsLarg(TXT_AJ_CAPS13, "Próxima estreia", 2.2f) * 0.5f, 250 + h.h, 0.6f);
      txt_desenhar_alpha(p, 640 - p.w * 0.5f, 280 + h.h, 0.45f);
    } else {        // So escurecer: quase tudo apagado
      TxtLinha h = txt(TXT_G26B, "21:47");
      txt_desenhar_alpha(h, 880, 560, 0.35f);
    }
  }
}

static void cenaHome(int s) {
  int mod = s == 0, i;
  float L = mod ? 0 : 96, X = L + (mod ? 150 : 50), H = mod ? 520 : 400;
  static const char *const LS[4] = { "21", "13", "05", "08" };
  static const char *const IC[5] = { "aj_house", "aj_search", "aj_film", "aj_bookmark", "aj_calendar" };
  gfx_cor((GfxRect){ 0, 0, 1280, 720 }, 0, 0.039f, 0.043f, 0.055f, 1);
  img("03.jpg", (GfxRect){ L, 0, 1280 - L, H }, 0, 1);
  sVeu((GfxRect){ L, 0, (1280 - L) * 0.6f, H }, 2, 0.85f);
  sVeu((GfxRect){ L, H * 0.4f, 1280 - L, H * 0.6f + 1 }, 0, 1.0f);
  sRelogio();
  { float y = mod ? 200 : 140;
    sCaps("Série · 2 temporadas", X, y, 1.4f);
    sTxt(TXT_AJ_NUM64, "Série em destaque", X, y + 22, 1);
    sTxt(TXT_G20M, "2024 · 8 episódios · 4K HDR", X, y + 104, 0.6f);
    { float cx = X;
      cx += sChip("Assistir", cx, y + 140, 54, 1, TXT_G21B) + 12;
      sChip("Mais informações", cx, y + 140, 54, 0, TXT_G21B); } }
  { float y = mod ? 536 : 420;
    sCaps(mod ? "Continuar assistindo" : "Em alta agora", X, y, 1);
    for (i = 0; i < 4; i++) sLand(LS[i], (GfxRect){ X + i * 292, y + 26, 276, 155 }, i == 0); }
  if (!mod) {
    gfx_recorte(146, 372, 1280 - 146, 26);
    for (i = 0; i < 6; i++) { static const char *const P[6] = { "02", "12", "15", "19", "05", "08" }; sPoster(P[i], 146 + i * 134, 372 - 150, 120); }
    gfx_sem_recorte();
    { TxtLinha t = txt(TXT_G18R, "Um pedaço da fileira de cima aparece aqui");
      GfxRect r = { 520, 300, t.w + 44, 44 };
      sIlha(r, 22); txtMeio(t, r.x + 22, r.y, 44, 1); }
    gfx_cor((GfxRect){ 0, 0, 96, 720 }, 0, 0.039f, 0.043f, 0.055f, 0.86f);
    gfx_cor((GfxRect){ 95, 0, 1, 720 }, 0, 1, 1, 1, 0.10f);
    for (i = 0; i < 5; i++) {
      GfxRect r = { 33, 360 - 2.5f * 56 + i * 56 - 15, 30, 30 };
      if (i == 0) iconeA(IC[i], r, 1); else icone(IC[i], r, 0.65f);
    }
  } else {
    GfxRect rail = { 26, 150, 84, 420 };
    sIlha(rail, 42);
    for (i = 0; i < 5; i++) {
      GfxRect b = { rail.x + 16, rail.y + 22 + i * 78, 52, 52 };
      if (i == 0) { float ar, ag, ab; acento(&ar, &ag, &ab); gfx_cor(b, 0.5f, ar, ag, ab, 1); iconeT(IC[i], (GfxRect){ b.x + 13, b.y + 13, 26, 26 }, 1); }
      else icone(IC[i], (GfxRect){ b.x + 13, b.y + 13, 26, 26 }, 0.7f);
    }
  }
}

static void cenaTitulo(int s) {
  int ser = s == 1, i;
  img("13.jpg", (GfxRect){ 0, 0, 1280, 720 }, 0, 1);
  sVeu((GfxRect){ 0, 0, 1280 * 0.7f, 720 }, 2, 0.94f);
  sVeu((GfxRect){ 0, 720 * 0.45f, 1280, 720 * 0.55f + 1 }, 0, 0.95f);
  sRelogio();
  sTxt(TXT_AJ_NUM64, "TÍTULO", 58, 72, 1);
  sTxt(TXT_G20M, ser ? "2 temporadas · 2024 · TV-MA" : "2h 14min · 2023 · 14 anos", 60, 156, 0.6f);
  { float cx = 60;
    cx += sChip(ser ? "Continuar T1 E3" : "Assistir", cx, 196, 54, 1, TXT_G21B) + 12;
    cx += sChip("Assistir trailer", cx, 196, 54, 0, TXT_G21B) + 12;
    gfx_cor((GfxRect){ cx, 196, 54, 54 }, 0.5f, 1, 1, 1, 0.10f);
    icone("aj_bookmark", (GfxRect){ cx + 16, 212, 22, 22 }, 1); }
  { GfxRect n = { 60, 274, 520, 112 };
    static const char *const V[3] = { "8,6", "93%", "84" }, *const K[3] = { "IMDb", "Tomatoes", "TMDB" };
    sIlha(n, 30);
    sCaps("Notas", n.x + 26, n.y + 18, 1);
    for (i = 0; i < 3; i++) { sTxt(TXT_LOG_N44, V[i], n.x + 26 + i * 150, n.y + 34, 1); sTxt(TXT_ILHA_APOIO, K[i], n.x + 28 + i * 150, n.y + 84, 0.55f); } }
  { GfxRect f = { 1280 - 34 - 360, 100, 360, 82 };
    static const float C[3][3] = { { 0.996f, 0.710f, 0.694f }, { 0.533f, 0.878f, 0.965f }, { 0.949f, 0.804f, 0.392f } };
    sIlha(f, 30);
    for (i = 0; i < 3; i++) sAv(C[i][0], C[i][1], C[i][2], i == 0 ? "A" : i == 1 ? "B" : "C", f.x + 22 + i * 32, f.y + 18, 46, 0);
    sTxt(TXT_ILHA_ITEM, "3 amigos", f.x + 150, f.y + 14, 1);
    sTxt(TXT_G18R, "já viram esse", f.x + 150, f.y + 42, 0.55f); }
  if (ser) {
    static const char *const E[4] = { "21", "00", "05", "03" };
    float cx = 60;
    cx += sChip("←", cx, 410, 34, 0, TXT_ILHA_APOIO) + 10;
    cx += sChip("Temporada 1", cx, 410, 34, 1, TXT_ILHA_APOIO) + 10;
    sChip("→", cx, 410, 34, 0, TXT_ILHA_APOIO);
    for (i = 0; i < 4; i++) {
      char t[48];
      sLand(E[i], (GfxRect){ 60 + i * 296, 458, 280, 158 }, i == 2);
      snprintf(t, sizeof t, "%d. %s", i + 1, i18n("Episódio"));
      sTxt(TXT_ILHA_ITEM, t, 60 + i * 296, 626, 1);
    }
  } else {
    static const char *const ES[3] = { "Estúdio A", "Estúdio B", "Estúdio C" };
    float cx = 60;
    sCaps("Estúdios e elenco", 60, 470, 1);
    for (i = 0; i < 3; i++) {
      TxtLinha t = txt(TXT_G20B, ES[i]);
      GfxRect r = { cx, 500, t.w + 52, 60 };
      sIlha(r, 20); txtMeio(t, r.x + 26, r.y, 60, 1);
      cx += r.w + 14;
    }
    sPoster("08", cx, 500, 96); sPoster("10", cx + 110, 500, 96); sPoster("17", cx + 220, 500, 96);
  }
}

static void cenaPlayer(int s) {
  img("08.jpg", (GfxRect){ 0, 0, 1280, 720 }, 0, 1);
  sVeu((GfxRect){ 0, 720 * 0.6f, 1280, 720 * 0.4f + 1 }, 0, 0.85f);
  sVeu((GfxRect){ 0, 0, 1280, 180 }, 1, 0.55f);
  sRelogio();
  if (s == 0) {
    GfxRect r = { 1280 - 34 - 520, 96, 520, 330 };
    float y;
    sIlha(r, 36);
    { float cx = r.x + 26; cx += sChip("Áudio", cx, r.y + 24, 42, 2, TXT_G18M) + 10; sChip("Legendas", cx, r.y + 24, 42, 1, TXT_G18M); }
    y = r.y + 82;
    sRowFoco((GfxRect){ r.x + 16, y, r.w - 32, 64 }, 20);
    sTxt(TXT_AJ_TEXTO, "Português (BR)", r.x + 36, y + 20, 1);
    sTag("em uso", r.x + r.w - 130, y + 20, 1);
    y += 68;
    sTxt(TXT_AJ_TEXTO, "English", r.x + 36, y + 20, 1); sTag("SRT", r.x + r.w - 90, y + 20, 0);
    y += 68;
    sTxt(TXT_AJ_TEXTO, "Segunda legenda", r.x + 36, y + 20, 0.55f);
    { TxtLinha t = txt(TXT_AJ_TEXTO, "English"); txt_desenhar_alpha(t, r.x + r.w - 36 - t.w, y + 20, 0.55f); }
    y += 72;
    sCaps("AutoSync", r.x + 36, y + 14, 1);
    { TxtLinha a = txt(TXT_G18M, "Minucioso");
      float w2 = a.w + 36, xx = r.x + r.w - 26 - w2;
      sChip("Minucioso", xx, y, 40, 2, TXT_G18M);
      { TxtLinha b = txt(TXT_G18M, "Rápido"); sChip("Rápido", xx - 10 - (b.w + 34.4f), y, 40, 1, TXT_G18M); } }
  } else if (s == 1) {
    GfxRect r = { 1280 - 34 - 520, 96, 520, 236 };
    sIlha(r, 36);
    sCaps("AutoSync · Minucioso", r.x + 26, r.y + 24, 1);
    sTxt(TXT_G26B, "Alinhando com o áudio…", r.x + 26, r.y + 46, 1);
    sBar((GfxRect){ r.x + 26, r.y + 94, r.w - 52, 8 }, 0.64f, 1);
    sTxt(TXT_G18R, "Lendo a referência do arquivo por HTTP Range", r.x + 26, r.y + 116, 0.55f);
    { float cx = r.x + 26; cx += sChip("Cancelar", cx, r.y + 166, 42, 2, TXT_G18M) + 10; sChip("Desfazer", cx, r.y + 166, 42, 0, TXT_G18M); }
    { TxtLinha t = txt(TXT_G18R, "sincronizada +0,40 s");
      GfxRect p = { 640 - (t.w + 70) * 0.5f, 30, t.w + 70, 48 };
      float ar, ag, ab; acento(&ar, &ag, &ab);
      sIlha(p, 24); gfx_cor((GfxRect){ p.x + 24, p.y + 19, 10, 10 }, 0.5f, ar, ag, ab, 1); txtMeio(t, p.x + 46, p.y, 48, 1); }
  } else {
    TxtLinha t = txt(TXT_G18R, "Seekr · prévia pronta"), m = txt(TXT_G18R, "+ 2:40");
    GfxRect p = { 640 - (t.w + m.w + 92) * 0.5f, 720 - 150 - 52, t.w + m.w + 92, 52 };
    float ar, ag, ab; acento(&ar, &ag, &ab);
    sIlha(p, 26); gfx_cor((GfxRect){ p.x + 24, p.y + 21, 10, 10 }, 0.5f, ar, ag, ab, 1);
    txtMeio(t, p.x + 46, p.y, 52, 1); txtMeio(m, p.x + 58 + t.w, p.y, 52, 0.55f);
  }
  { const char *f = s == 1 ? "Ela já sabia disso." : "Você não precisava vir.";
    TxtLinha t = txt(TXT_G30M, f);
    gfx_cor((GfxRect){ 50, 720 - 140 - t.h - 12, t.w + 32, t.h + 12 }, 12.0f / (t.h + 12), 0, 0, 0, 0.35f);
    txt_desenhar(t, 66, 720 - 140 - t.h - 6); }
  { TxtLinha a = txt(TXT_G18R, "Título · T1 E3"), b = txt(TXT_G18R, s == 2 ? "44:10 / 1:12:00" : "38:20 / 1:12:00");
    float y = 720 - 36 - 50 - 30;
    txt_desenhar_alpha(a, 50, y, 1); txt_desenhar_alpha(b, 1280 - 50 - b.w, y, 0.55f);
    sBar((GfxRect){ 50, y + 30, 1180, s == 2 ? 14.0f : 8.0f }, s == 2 ? 0.61f : 0.53f, 1);
    icone("aj_captions", (GfxRect){ 50, y + 56, 28, 28 }, 0.8f);
    icone("aj_play", (GfxRect){ 98, y + 56, 28, 28 }, 0.8f);
    icone("aj_list-video", (GfxRect){ 146, y + 56, 28, 28 }, 0.8f); }
}

static void cenaFontes(int s) {
  sol = 1;
  acR = 0.514f; acG = 0.898f; acB = 0.737f;
  fundoArte("05.jpg", (GfxRect){ 0, 0, 1280, 720 }, 0, 1);
  sRelogio();
  if (s == 0) {
    GfxRect r = { 300, 36, 680, 648 };
    static const char *const F[4] = { "Todas", "Só MP4", "Em cache", "Dublado" };
    float y = r.y + 26, cx = r.x + 24;
    int i;
    sIlha(r, 40);
    sCaps("Fontes", r.x + 30, y, 1);
    y += 26;
    for (i = 0; i < 4; i++) cx += sChip(F[i], cx, y, 36, i == 0 ? 1 : 2, TXT_ILHA_APOIO) + 8;
    y += 52;
    { static const struct { const char *gb, *t[3], *badge, *grupo; } L[4] = {
        { "7,8 GB", { "4K", "HDR", "Dolby Vision" }, "Melhor para esta TV", NULL },
        { "12,1 GB", { "4K", "HDR10", NULL }, NULL, "4K" },
        { "3,2 GB", { "1080p", "Dublado", NULL }, NULL, "1080p" },
        { "2,4 GB", { "1080p", "MP4", NULL }, NULL, NULL } };
      for (i = 0; i < 4; i++) {
        float ry, tx;
        int k;
        if (L[i].grupo) { sCaps(L[i].grupo, r.x + 30, y + 12, 1); y += 34; }
        if (i == 0) sRowFoco((GfxRect){ r.x + 16, y, r.w - 32, 108 }, 22);
        ry = y + 14;
        if (L[i].badge) { capsA(TXT_AJ_MINI12, L[i].badge, 2.0f, r.x + 40, ry, 1); ry += 22; }
        sTxt(TXT_ILHA_ITEM, "Série", r.x + 40, ry, 1);
        sTxt(TXT_ILHA_APOIO, "T1 E3", r.x + 40 + txt_largura(TXT_ILHA_ITEM, "Série") + 12, ry + 4, 0.55f);
        { TxtLinha g = txt(TXT_G18R, L[i].gb); txt_desenhar_alpha(g, r.x + r.w - 40 - g.w, ry + 2, 0.6f); }
        tx = r.x + 40;
        for (k = 0; k < 3 && L[i].t[k]; k++) { sTag(L[i].t[k], tx, ry + 36, k == 0 ? 1 : 0); tx += txt_largura(TXT_AJ_CAPS13, i18n(L[i].t[k])) + 24; }
        y += i == 0 ? 116 : 92;
      } }
  } else {
    GfxRect r = { 300, 36, 680, 648 };
    float x = r.x + 34, y = r.y + 30;
    sIlha(r, 40);
    sCaps("Memória para imagens agora", x, y, 1);
    sTxt(TXT_AJ_NUM64, ajustes_idioma_ingles() ? "151.4" : "151,4", x, y + 22, 1);
    { char d[48]; snprintf(d, sizeof d, i18n("de %d MB"), 240);
      sTxt(TXT_AJ_TEXTO, d, x + txt_largura(TXT_AJ_NUM64, "151,4") + 14, y + 60, 0.55f); }
    { char p[48]; TxtLinha t; snprintf(p, sizeof p, i18n("%d%% do teto"), 63); t = txtA(TXT_G18R, p); txt_desenhar(t, r.x + r.w - 34 - t.w, y + 62); }
    y += 110;
    sBar((GfxRect){ x, y, r.w - 68, 10 }, 0.63f, 1);
    y += 30;
    ajustes_ui_grafico_exemplo(x, y, r.w - 68, 170);
    y += 200;
    { static const char *const K[3] = { "Na tela agora", "StreamFit · rede da TV", "Teto" };
      char v[3][64];
      int i;
      snprintf(v[0], sizeof v[0], i18n("%d imagens · %s MB"), 38, ajustes_idioma_ingles() ? "96.2" : "96,2");
      snprintf(v[1], sizeof v[1], i18n("%d Mbps medido"), 38);
      snprintf(v[2], sizeof v[2], "240 MB · %s", i18n("pela RAM da TV"));
      for (i = 0; i < 3; i++) {
        TxtLinha k = txt(TXT_G18R, K[i]), l = txt(TXT_G18R, v[i]);
        gfx_cor((GfxRect){ x, y, r.w - 68, 1 }, 0, 1, 1, 1, 0.08f);
        txtMeio(k, x, y, 56, 0.55f); txtMeio(l, r.x + r.w - 34 - l.w, y, 56, 0.9f);
        y += 56;
      } }
  }
}

static void cenaSocial(int s) {
  static const float C[4][3] = { { 0.533f, 0.878f, 0.965f }, { 0.996f, 0.710f, 0.694f }, { 0.949f, 0.804f, 0.392f }, { 0.780f, 0.733f, 0.941f } };
  fundoArte("21.jpg", (GfxRect){ 0, 0, 1280, 720 }, 0, 1);
  if (s == 0) {
    GfxRect r = { 110, 36, 960, 648 };
    float y = r.y + 28, cx = r.x + 34;
    sRelogio();
    sIlha(r, 44);
    cx += sChip("Amigos", cx, y, 42, 1, TXT_G18M) + 10;
    cx += sChip("Atividade", cx, y, 42, 2, TXT_G18M) + 10;
    sChip("Salvos", cx, y, 42, 2, TXT_G18M);
    y += 62;
    gfx_cor((GfxRect){ r.x + 34, y, r.w - 68, 92 }, 22.0f / 92, 1, 1, 1, 0.07f);
    sAv(C[0][0], C[0][1], C[0][2], "V", r.x + 54, y + 23, 46, 0);
    sTxt(TXT_ILHA_NOME, "Você", r.x + 116, y + 18, 1);
    sTxt(TXT_ILHA_APOIO, "Uma pessoa, contas ligadas", r.x + 116, y + 52, 0.55f);
    { float tx = r.x + r.w - 54;
      static const char *const S[3] = { "Letterboxd", "Simkl", "Trakt" };
      int i;
      for (i = 0; i < 3; i++) { tx -= txt_largura(TXT_AJ_CAPS13, S[i]) + 16 + 8; sTag(S[i], tx + 8, y + 34, 1); } }
    y += 104;
    { static const char *const N[3] = { "Ana", "Bruno", "Carla" }, *const T[3] = { "assistiu agora", "visto há 2 h", "recomendou um filme" };
      int i;
      for (i = 0; i < 3; i++) {
        if (i == 0) sRowFoco((GfxRect){ r.x + 34, y, r.w - 68, 80 }, 22);
        sAv(C[i + 1][0], C[i + 1][1], C[i + 1][2], i == 0 ? "A" : i == 1 ? "B" : "C", r.x + 54, y + 17, 46, i < 2);
        sTxt(TXT_ILHA_NOME, N[i], r.x + 116, y + 26, 1);
        { TxtLinha t = txt(TXT_G18R, T[i]); float xr = r.x + r.w - 54 - (i == 2 ? 150 : 0);
          txt_desenhar_alpha(t, xr - t.w, y + 28, 0.55f); }
        if (i == 2) { TxtLinha j = txtT(TXT_ILHA_APOIO, "Já assisti"); sChip("Já assisti", r.x + r.w - 54 - j.w - 31, y + 22, 36, 1, TXT_ILHA_APOIO); }
        y += 84;
      } }
    sTxt(TXT_ILHA_APOIO, "Pedido de amizade: Recusar não avisa a pessoa.", r.x + 34, y + 14, 0.55f);
  } else {
    static const char *const P[3] = { "12", "15", "19" };
    int i;
    for (i = 0; i < 3; i++) {
      float x = 60 + i * 240;
      sPoster(P[i], x, 120, 214);
      gfx_cor((GfxRect){ x + 214 - 34 - 3, 120 + 321 - 34 - 3, 50, 50 }, 0.5f, 0.09f, 0.094f, 0.114f, 1);
      sAv(C[i + 1][0], C[i + 1][1], C[i + 1][2], i == 0 ? "A" : i == 1 ? "B" : "C", x + 214 - 34, 120 + 321 - 34, 44, 1);
    }
    sTxt(TXT_AJ_TEXTO, "Ana e Bruno viram · no pôster e na linha do hero", 60, 470, 0.8f);
    { GfxRect r = { 1280 - 34 - 470, 28, 470, 300 };
      float ar, ag, ab, y = r.y + 22;
      acento(&ar, &ag, &ab);
      sIlha(r, 36);
      gfx_cor((GfxRect){ r.x + 26, y + 8, 10, 10 }, 0.5f, ar, ag, ab, 1);
      sTxt(TXT_ILHA_ITEM, "21:40", r.x + 46, y, 1);
      sTxt(TXT_ILHA_APOIO, "Tem uma enquete para você", r.x + 46 + txt_largura(TXT_ILHA_ITEM, "21:40") + 12, y + 4, 0.55f);
      y += 42;
      sTxt(TXT_G23B, "Qual vai ser a sessão de hoje?", r.x + 26, y, 1);
      y += 44;
      sTxt(TXT_G18R, "Filme", r.x + 26, y, 1); { TxtLinha t = txt(TXT_G18R, "58%"); txt_desenhar_alpha(t, r.x + r.w - 26 - t.w, y, 0.55f); }
      sBar((GfxRect){ r.x + 26, y + 28, r.w - 52, 6 }, 0.58f, 1);
      y += 46;
      sTxt(TXT_G18R, "Série", r.x + 26, y, 1); { TxtLinha t = txt(TXT_G18R, "42%"); txt_desenhar_alpha(t, r.x + r.w - 26 - t.w, y, 0.55f); }
      sBar((GfxRect){ r.x + 26, y + 28, r.w - 52, 6 }, 0.42f, 0.5f);
      y += 50;
      txt_bloco(TXT_ILHA_APOIO, "Não quer? Ajustes › Notificações › Receber enquetes", TX, r.x + 26, y, r.w - 52, 22, 0.55f, 2); }
  }
}

static void cenaPerfis(int s) {
  static const char *const A[3] = { "00.jpg", "13.jpg", "05.jpg" }, *const N[3] = { "Perfil 1", "Perfil 2", "Perfil 3" };
  static const float C[3][3] = { { 0.533f, 0.878f, 0.965f }, { 0.949f, 0.804f, 0.392f }, { 0.780f, 0.733f, 0.941f } };
  int i;
  acR = C[s][0]; acG = C[s][1]; acB = C[s][2];
  (void)A;
  // Fundo Filmes (psestilos.c): a parede de cartazes de quem esta em foco,
  // girada como na tela de verdade, colunas alternadas. O de PIN nao tem parede.
  gfx_cor((GfxRect){ 0, 0, 1280, 720 }, 0, 0.035f, 0.04f, 0.05f, 1);
  if (s != 2) {
    int col, lin;
    gfx_girar(-0.16f, 640, 360);
    for (col = 0; col < 9; col++)
      for (lin = 0; lin < 4; lin++) {
        char n[4];
        snprintf(n, sizeof n, "%02d", (col * 4 + lin + s * 13) % 30);
        sPoster(n, -170 + col * 186.0f, -230 + lin * 268.0f + (col & 1 ? 120.0f : 0.0f), 170);
      }
    gfx_sem_girar();
  }
  gfx_cor((GfxRect){ 0, 0, 1280, 720 }, 0, 0.02f, 0.024f, 0.03f, 0.62f);
  sVeu((GfxRect){ 0, 360, 1280, 360 }, 0, 0.6f);
  { float w = capsLarg(TXT_AJ_CAPS13, "Quem está assistindo?", 2.2f); sCaps("Quem está assistindo?", 640 - w * 0.5f, 90, 1.6f); }
  for (i = 0; i < 3; i++) {
    float d = i == s ? 168 : 150, cx = 640 + (i - 1) * 206.0f, y = 150 - (d - 150) * 0.5f;
    float al = i == s ? 1.0f : 0.55f;
    char n[4];
    TxtLinha t;
    if (i == s) gfx_cor((GfxRect){ cx - d * 0.5f - 10, y - 10, d + 20, d + 20 }, 0.5f, 1, 1, 1, 1);
    if (i == s) gfx_cor((GfxRect){ cx - d * 0.5f - 6, y - 6, d + 12, d + 12 }, 0.5f, 0.05f, 0.05f, 0.06f, 1);
    gfx_cor((GfxRect){ cx - d * 0.5f, y, d, d }, 0.5f, C[i][0], C[i][1], C[i][2], al);
    snprintf(n, sizeof n, "%d", i + 1);
    t = txt_linha(TXT_AJ_NUM64, n, 16, 18, 22, 255);
    txt_desenhar_alpha(t, cx - t.w * 0.5f, y + (d - t.h) * 0.5f, al);
    t = txt(TXT_ILHA_ITEM, N[i]);
    txt_desenhar_alpha(t, cx - t.w * 0.5f, 150 + 150 + 22, al);
    if (i == 2) { TxtLinha p = txt(TXT_ILHA_APOIO, "PIN"); icone("aj_shield", (GfxRect){ cx - (p.w + 20) * 0.5f, 354, 14, 14 }, 0.55f);
      txt_desenhar_alpha(p, cx - (p.w + 20) * 0.5f + 20, 350, 0.55f); }
  }
  { GfxRect r = { 640 - 320, 430, 640, s == 2 ? 104 : 178 };
    sIlha(r, 34);
    if (s == 2) {
      icone("aj_shield", (GfxRect){ r.x + 28, r.y + 34, 34, 34 }, 1);
      sTxt(TXT_ILHA_NOME, "Continuar assistindo", r.x + 84, r.y + 22, 1);
      sTxt(TXT_G18R, "Protegido por PIN. Digite o PIN para ver.", r.x + 84, r.y + 56, 0.55f);
    } else {
      char t[64];
      snprintf(t, sizeof t, i18n("Continuar de %s"), i18n(N[s]));
      sCaps(t, r.x + 26, r.y + 22, 1);
      img(s ? "08.jpg" : "21.jpg", (GfxRect){ r.x + 26, r.y + 44, 220, 124 }, 16, 1);
      sTxt(TXT_ILHA_NOME, "Série · T1 E3", r.x + 264, r.y + 56, 1);
      sBar((GfxRect){ r.x + 264, r.y + 98, r.w - 290, 8 }, s ? 0.36f : 0.64f, 1);
      sTxt(TXT_ILHA_APOIO, "só deste dispositivo, nada sai da TV", r.x + 264, r.y + 118, 0.55f);
    } }
  { TxtLinha a = txt(TXT_ILHA_APOIO, "← → perfil"), b = txt(TXT_ILHA_APOIO, "OK entra"), c = txt(TXT_ILHA_APOIO, "Voltar sai");
    float w = a.w + b.w + c.w + 40 + 52;
    GfxRect r = { 640 - w * 0.5f, 720 - 34 - 44, w, 44 };
    sIlha(r, 22);
    txtMeio(a, r.x + 26, r.y, 44, 1); txtMeio(b, r.x + 46 + a.w, r.y, 44, 1); txtMeio(c, r.x + 66 + a.w + b.w, r.y, 44, 0.55f); }
}

static void cenaServidores(int s) {
  static const char *const N[3] = { "Jellyfin", "Emby", "Plex" };
  static const char *const MODO[3] = { "Quick Connect", "Usuário e senha", "Login por PIN" };
  static const char *const DICA[3] = { "Abra Painel › Quick Connect e digite", "Mesmo protocolo do Jellyfin", "Abra plex.tv/link e digite" };
  static const char *const COD[3] = { "482 910", "Conectado", "K7QX" };
  static const float C[3][3] = { { 0.545f, 0.486f, 1.0f }, { 0.322f, 0.761f, 0.420f }, { 0.949f, 0.694f, 0.204f } };
  int i;
  acR = C[s][0]; acG = C[s][1]; acB = C[s][2];
  fundoArte("13.jpg", (GfxRect){ 0, 0, 1280, 720 }, 0, 1);
  sRelogio();
  for (i = 0; i < 3; i++) {
    GfxRect r = { 46, 110 + i * 98, 260, 84 };
    char l[2] = { N[i][0], 0 };
    TxtLinha t = txt_linha(TXT_G23B, l, 16, 18, 22, 255);
    sIlha(r, 26);
    if (i == s) sRowFoco(r, 26);
    gfx_cor((GfxRect){ r.x + 20, r.y + 20, 44, 44 }, 14.0f / 44, C[i][0], C[i][1], C[i][2], 1);
    txt_desenhar(t, r.x + 42 - t.w * 0.5f, r.y + 42 - t.h * 0.5f);
    sTxt(TXT_ILHA_NOME, N[i], r.x + 80, r.y + 28, 1);
  }
  { GfxRect r = { 340, 110, 520, 300 };
    char t[64];
    TxtLinha c;
    sIlha(r, 38);
    sCaps(MODO[s], r.x + 30, r.y + 28, 1);
    snprintf(t, sizeof t, i18n("%s da sua casa"), N[s]);
    sTxt(TXT_G26B, t, r.x + 30, r.y + 52, 1);
    sTxt(TXT_G18R, DICA[s], r.x + 30, r.y + 94, 0.55f);
    c = txtA(s == 1 ? TXT_G30B : TXT_AJ_NUM64, COD[s]);
    txt_desenhar(c, r.x + 30, r.y + 136); }
  { GfxRect r = { 890, 110, 350, 300 };
    static const char *const L[4] = { "Fileiras na Home", "Detalhes e episódios", "Retomar de onde parou", "Visto volta pro servidor" };
    sIlha(r, 38);
    sCaps("Depois de entrar", r.x + 26, r.y + 26, 1);
    for (i = 0; i < 4; i++) {
      iconeA("aj_check", (GfxRect){ r.x + 26, r.y + 62 + i * 54 + 4, 22, 22 }, 1);
      sTxt(TXT_G18R, L[i], r.x + 58, r.y + 62 + i * 54 + 4, 1);
    } }
  { char t[64];
    static const char *const P[6] = { "08", "10", "17", "19", "05", "02" };
    snprintf(t, sizeof t, "%s · %s", i18n("Na sua Home"), N[s]);
    sCaps(t, 340, 450, 1);
    for (i = 0; i < 6; i++) sPoster(P[i], 340 + i * 164, 476, 150); }
}

static void cenaPlugins(int s) {
  GfxRect r = { 230, 40, 820, 640 };
  float y = r.y + 28, x = r.x + 32, w = r.w - 64;
  sol = 1;
  acR = 0.949f; acG = 0.804f; acB = 0.392f;
  fundoArte("08.jpg", (GfxRect){ 0, 0, 1280, 720 }, 0, 1);
  sRelogio();
  sIlha(r, 44);
  sCaps(s == 0 ? "Ajustes › Plugins" : "Ajustes › Reprodução", x, y, 1);
  y += 36;
  sRowFoco((GfxRect){ x, y, w, 72 }, 22);
  { const char *n = s == 0 ? "Plugins" : "Servidor P2P";
    sTxt(TXT_ILHA_NOME, n, x + 20, y + 22, 1);
    sTag("EXPERIMENTAL", x + 32 + txt_largura(TXT_ILHA_NOME, n), y + 24, 2);
    { TxtLinha d = txt(TXT_G18R, "Desligado"); txt_desenhar_alpha(d, x + w - 20 - 64 - 16 - d.w, y + 26, 0.55f); }
    sTog(x + w - 20 - 64, y + 18, 0); }
  y += 80;
  if (s == 0) {
    sTxt(TXT_AJ_TEXTO, "Repositório", x + 20, y + 24, 0.55f);
    { TxtLinha t = txt(TXT_G18R, "Adicionar por URL…"); txt_desenhar_alpha(t, x + w - 20 - t.w, y + 26, 0.55f); }
    y += 76;
    sTxt(TXT_AJ_TEXTO, "Meu repositório", x + 20, y + 24, 1);
    { TxtLinha t = txt(TXT_G18R, "3 scrapers"); txt_desenhar_alpha(t, x + w - 20 - 64 - 16 - t.w, y + 26, 0.55f); }
    sTog(x + w - 20 - 64, y + 18, 1);
    y += 76;
    sTxt(TXT_AJ_TEXTO, "Remover repositório", x + 20, y + 24, 1);
    { TxtLinha t = txt(TXT_G18R, "OK duas vezes"); txt_desenhar_alpha(t, x + w - 20 - t.w, y + 26, 0.55f); }
    y += 96;
    txt_bloco(TXT_G18R, "Cada scraper vira mais uma origem na folha de Fontes, rodando junto com os addons. Limites globais de memória e de pedidos.",
              TX, x, y, w, 27, 0.55f, 4);
  } else {
    sCaps("Na folha de Fontes", x, y + 12, 1);
    y += 40;
    gfx_cor((GfxRect){ x, y, w, 84 }, 22.0f / 84, 1, 1, 1, 0.05f);
    sTxt(TXT_ILHA_NOME, "Torrent · 1080p", x + 20, y + 14, 1);
    sTxt(TXT_ILHA_APOIO, "escolhido por você", x + 20, y + 48, 0.55f);
    sTag("P2P", x + w - 70, y + 30, 2);
    y += 110;
    txt_bloco(TXT_G18R, "Motor embutido, sem servidor na rede. A escolha automática nunca usa P2P. Limite de disco até 1,5 GB, memória 32 MB, para se faltar espaço e diz por quê.",
              TX, x, y, w, 27, 0.55f, 5);
  }
}

static void cenaAjustes(int s) {
  static const char *const CI[6] = { "aj_panel-top", "aj_captions", "aj_house", "aj_zap", "aj_users", "aj_book-open" };
  static const char *const CN[6] = { "Aparência", "Reprodução", "Home", "Desempenho", "Contas e serviços", "Sobre e ajuda" };
  int i;
  sol = 1;
  gfx_cor((GfxRect){ 0, 0, 1280, 720 }, 0, 0.051f, 0.055f, 0.071f, 1);
  img("00.jpg", (GfxRect){ 0, 0, 520, 720 }, 0, 1);
  sVeu((GfxRect){ 0, 0, 520, 720 }, 3, 0.9f);
  { GfxRect r = { 40, 150, 400, 250 };
    sIlha(r, 30);
    sCaps("Prévia da opção", r.x + 22, r.y + 22, 1);
    sPoster("02", r.x + 22, r.y + 50, 90); sPoster("12", r.x + 122, r.y + 50, 90); sPoster("15", r.x + 222, r.y + 50, 90);
    sTxt(TXT_ILHA_APOIO, s == 0 ? "Cada opção mostra como vai ficar" : "Memória e opções avançadas", r.x + 22, r.y + 200, 0.55f); }
  if (s == 0) {
    float x = 560, w = 1280 - 34 - 560, y = 100;
    GfxRect b = { x, y, w, 64 };
    sIlha(b, 32);
    icone("aj_search", (GfxRect){ x + 22, y + 21, 22, 22 }, 1);
    sTxt(TXT_AJ_TEXTO, "Buscar nos Ajustes", x + 58, y + 20, 0.55f);
    y += 80;
    for (i = 0; i < 6; i++) {
      if (i == 5) sRowFoco((GfxRect){ x, y, w, 64 }, 22);
      icone(CI[i], (GfxRect){ x + 20, y + 19, 26, 26 }, 1);
      sTxt(TXT_ILHA_ITEM, CN[i], x + 62, y + 20, 1);
      if (i == 5) { char t[96]; TxtLinha l; snprintf(t, sizeof t, "%s · %s", i18n("Guia de uso"), i18n("Novidades 2.0"));
        l = txt(TXT_ILHA_APOIO, t); txt_desenhar_alpha(l, x + w - 20 - l.w, y + 24, 0.55f); }
      y += 68;
    }
  } else {
    float x = 560, w = 1280 - 34 - 560, y = 100;
    sCaps("Desempenho desta TV", x, y, 1);
    y += 28;
    sRowFoco((GfxRect){ x, y, w, 72 }, 22);
    icone("aj_sliders-horizontal", (GfxRect){ x + 20, y + 23, 26, 26 }, 1);
    sTxt(TXT_AJ_TEXTO, "Mostrar opções avançadas", x + 62, y + 24, 1);
    sTog(x + w - 20 - 64, y + 18, 1);
    y += 76;
    icone("aj_zap", (GfxRect){ x + 20, y + 23, 26, 26 }, 1);
    sTxt(TXT_AJ_TEXTO, "Memória para imagens", x + 62, y + 24, 1);
    { TxtLinha t = txt(TXT_G18R, "Automático"); txt_desenhar_alpha(t, x + w - 20 - t.w, y + 26, 0.55f); }
    y += 76;
    icone("aj_captions", (GfxRect){ x + 20, y + 23, 26, 26 }, 0.55f);
    sTxt(TXT_AJ_TEXTO, "AutoSync pelo áudio", x + 62, y + 24, 0.55f);
    { TxtLinha t = txt(TXT_ILHA_APOIO, "Só no Android"); txt_desenhar_alpha(t, x + w - 20 - t.w, y + 28, 0.55f); }
    y += 92;
    { GfxRect m = { x, y, w, 150 };
      sIlha(m, 28);
      sCaps("Memória agora", m.x + 24, m.y + 22, 1);
      sTxt(TXT_LOG_N44, ajustes_idioma_ingles() ? "151.4" : "151,4", m.x + 24, m.y + 42, 1);
      { char d[48]; snprintf(d, sizeof d, i18n("de %d MB"), 240); sTxt(TXT_G18R, d, m.x + 34 + txt_largura(TXT_LOG_N44, "151,4"), m.y + 62, 0.55f); }
      sBar((GfxRect){ m.x + 24, m.y + 112, m.w - 48, 8 }, 0.63f, 1); }
  }
}

typedef void (*CenaFn)(int s);
static const CenaFn CENA[N20_NCAP] = { cenaVisual, cenaDescanso, cenaHome, cenaTitulo, cenaPlayer, cenaFontes,
                                      cenaSocial, cenaPerfis, cenaServidores, cenaPlugins, cenaAjustes };
static int nEst(int c) { return CAP[c].st[2] ? 3 : 2; }

// ------------------------------------------------------------------ a lista
static void montarLista(void) {
  int c, cur = idx >= 0 && idx < nLista ? lista[idx] : N20_HERO;
  nLista = 0;
  lista[nLista++] = N20_HERO;
  for (c = 0; c < N20_NCAP; c++) if (!essencial || CAP[c].ess) lista[nLista++] = c;
  lista[nLista++] = N20_RESUMO;
  lista[nLista++] = N20_FIM;
  for (idx = 0; idx < nLista; idx++) if (lista[idx] == cur) return;
  idx = 0;
}
static int tipo(void) { return lista[idx]; }
static void irPara(int i) {
  if (i < 0) i = 0;
  if (i >= nLista) i = nLista - 1;
  if (i == idx) return;
  idx = i;
  troca = ajustes_animacoes_reduzidas() ? 1.0f : 0.0f;
  focoFim = 0;
}
static int nCapsVis(void) { return nLista - 3; }
static int posCap(int c) { int i; for (i = 1; i < nLista - 2; i++) if (lista[i] == c) return i; return 0; }

// ------------------------------------------------------------------ API simples
void novidades20_dir(const char *d) { if (d && d[0]) snprintf(dirArte, sizeof dirArte, "%s", d); }
int  novidades20_aberto(void) { return aberto; }
int  novidades20_visivel(void) { return aberto || entrada > 0.002f; }
int  novidades20_pedido(void) { int p = pedido; pedido = N20_PEDIU_NADA; return p; }
void novidades20_aparelho(int d) { devForcado = d; }
int  novidades20_dev(void) { return dev(); }
int  novidades20_telas(void) { return nLista; }
int  novidades20_tela(void) { return idx; }
int  novidades20_tela_tipo(int i) { return i >= 0 && i < nLista ? lista[i] : N20_HERO; }
int  novidades20_estado(int c) { return c >= 0 && c < N20_NCAP ? est[c] : 0; }
int  novidades20_estados(int c) { return c >= 0 && c < N20_NCAP ? nEst(c) : 0; }
int  novidades20_essencial(void) { return essencial; }
int  novidades20_saindo(void) { return saindo; }
int  novidades20_foco(void) { return saindo ? focoSair : tipo() == N20_HERO ? focoHero : focoFim; }
const char *novidades20_cap_nome(int c) { return c >= 0 && c < N20_NCAP ? CAP[c].nome : ""; }
int  novidades20_disponivel(int c, int item) {
  if (c < 0 || c >= N20_NCAP || item < 0 || item >= 9) return 0;
  if (item < 3) return temAqui(CAP[c].l[item].so);
  return CAP[c].e[item - 3].t ? temAqui(CAP[c].e[item - 3].so) : 0;
}
void novidades20_ir(int t) { if (t >= 0 && t < nLista) { idx = t; troca = 1.0f; focoFim = 0; } }

void novidades20_abrir(int n) {
  aberto = 1;
  decidido = 1;
  novo = n ? 1 : 0;
  essencial = 0;
  idx = 0;
  montarLista();
  idx = 0;
  memset(est, 0, sizeof est);
  focoHero = 0; focoFim = 0; saindo = 0; focoSair = 0; sairA = 0;
  troca = 1.0f;
  entrada = ajustes_animacoes_reduzidas() ? 1.0f : 0.0f;
#ifdef NV_TOUCH_UI
  telefonecartao_limpar(&n20Telefone);
#endif
}

void novidades20_primeira_vez(void) {
  char *s;
  int f;
  if (decidido) return;
  decidido = 1;
  s = dados_ler(N20_ARQ);
  if (s) { free(s); return; }
  // A fila antiga ve tudo como visto; a marca da 1.8.0 tambem, para o cartao
  // dela nao aparecer depois deste guia.
  f = novidadesfila_preparar();
  dados_gravar(NF_ARQ_180, "1\n");
  dados_gravar(N20_ARQ, "1\n");
  novidades20_abrir(f == NF_NOVA);
}

void novidades20_suprimir(void) {
  char *s;
  if (decidido) return;
  decidido = 1;
  s = dados_ler(N20_ARQ);
  if (s) { free(s); return; }
  // O COMECAR da 2.1 (comecar.h) tomou o primeiro uso: a fila antiga fica
  // vista do mesmo jeito, mas o guia NAO abre sozinho — quem oferece o tour
  // e o Comecar, e o guia inteiro continua em Ajustes > Sobre e ajuda.
  novidadesfila_preparar();
  dados_gravar(NF_ARQ_180, "1\n");
  dados_gravar(N20_ARQ, "1\n");
}

static void fechar(const char *aviso, int oQue) {
  aberto = 0;
  saindo = 0;
  pedido = oQue;
  dados_gravar(N20_ARQ, "1\n");
  if (aviso) ilha_avisar("novidades20", ILHA_OK, "aj_book-open", i18n(aviso), 4500, 0);
}
#define AV_GUARDADO "Guardado em Ajustes › Sobre e ajuda › Novidades 2.0"

// ------------------------------------------------------------------ teclas
static int nBotoesFim(void) { return novo ? 3 : 2; }
// Botoes da tela final: (novo) Abrir o Guia de uso, Concluir, Rever do começo.
enum { BF_GUIA = 0, BF_CONCLUIR, BF_REVER };
static int botaoFim(int i) { return novo ? i : i + 1; }
static void okFim(int b) {
  if (b == BF_GUIA) fechar(NULL, N20_PEDIU_GUIA);
  else if (b == BF_CONCLUIR) fechar("Pronto. O guia ficou em Sobre e ajuda.", N20_PEDIU_NADA);
  else irPara(0);
}
static void okHero(int b) {
  if (b == 0) { essencial = 0; montarLista(); irPara(1); }
  else if (b == 1) { essencial = 1; montarLista(); irPara(1); }
  else fechar(AV_GUARDADO, N20_PEDIU_NADA);
}
static void okSair(int b) {
  if (b == 0) saindo = 0;
  else if (b == 1) fechar(AV_GUARDADO, N20_PEDIU_NADA);
  else fechar("Guia fechado. Dá pra reabrir quando quiser.", N20_PEDIU_NADA);
}
static int ehAzul(const SDL_Event *e) {
  SDL_Keycode k = e->key.keysym.sym;
  int sc = e->key.keysym.scancode;
  return k == SDLK_s || sc == NV_SCANCODE_BLUE || sc == NV_SCANCODE_CH_UP || k == SDLK_PAGEUP;
}
void novidades20_evento(const SDL_Event *e) {
  SDL_Keycode k;
  int t, ok;
  if (!aberto || !e || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  ok = k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE;
  t = tipo();
  if (saindo) {
    if (k == SDLK_LEFT && focoSair > 0) focoSair--;
    else if (k == SDLK_RIGHT && focoSair < 2) focoSair++;
    else if (ok) okSair(focoSair);
    else if (nv_tecla_voltar(e)) saindo = 0;
    return;
  }
  if (nv_tecla_voltar(e)) { saindo = 1; focoSair = 0; sairA = ajustes_animacoes_reduzidas() ? 1.0f : 0.0f; return; }
  if (ehAzul(e)) {
    if (e->key.repeat) return;
    if (idx >= nLista - 2) irPara(0); else irPara(nLista - 2);
    return;
  }
  if (t == N20_HERO) {
    if (k == SDLK_DOWN) focoHero = (focoHero + 1) % 3;
    else if (k == SDLK_UP) focoHero = (focoHero + 2) % 3;
    else if (k == SDLK_RIGHT) okHero(0);
    else if (ok) okHero(focoHero);
    return;
  }
  if (t == N20_FIM) {
    if (k == SDLK_LEFT) { if (focoFim > 0) focoFim--; else irPara(idx - 1); }
    else if (k == SDLK_RIGHT) { if (focoFim < nBotoesFim() - 1) focoFim++; }
    else if (ok) okFim(botaoFim(focoFim));
    return;
  }
  if (k == SDLK_LEFT) irPara(idx - 1);
  else if (k == SDLK_RIGHT) irPara(idx + 1);
  else if (ok) {
    if (t >= 0) est[t] = (est[t] + 1) % nEst(t);
    else irPara(idx + 1);
  }
}

// ------------------------------------------------------------------ a cena (alvos)
static GfxMini mini[2];
static int miniCur, miniKey[2] = { -1, -1 }, miniPronto[2], miniVoltas[2];
static float miniCruza = 1.0f;
#define PEEK_X 96.0f
#define PEEK_Y 150.0f
#define PEEK_W 1000.0f
#define PEEK_H 562.0f
static int chaveCena(int c) {
  float r, g, b;
  ajustes_acento(&r, &g, &b);
  return ((c * 4 + est[c]) * 2 + ajustes_vidro()) * 4096 + (int)(r * 15) * 256 + (int)(g * 15) * 16 + (int)(b * 15);
}
static void renderCena(int m, int c) {
  int pend0 = txt_pendentes;
  if (!gfx_mini_alvo(&mini[m], (int)PEEK_W, (int)PEEK_H)) { miniPronto[m] = 0; return; }
  faltas = 0;
  gfx_mini_comecar(&mini[m], 0, 0, PEEK_W / 1280.0f);
  gfx_cor((GfxRect){ 0, 0, 1280, 720 }, 0, 0.039f, 0.043f, 0.055f, 1);
  sol = 0; acR = -1;
  CENA[c](est[c]);
  sol = 0; acR = -1;
  gfx_mini_terminar();
  miniVoltas[m]++;
  miniPronto[m] = faltas == 0 && txt_pendentes == pend0 && miniVoltas[m] >= 2;
  if (miniVoltas[m] > 240) miniPronto[m] = 1;   // arte que nunca chega: congela assim mesmo
}
static void peek(int c, float a) {
  int k = chaveCena(c);
  GfxRect r = { PEEK_X, PEEK_Y, PEEK_W, PEEK_H };
  if (miniKey[miniCur] != k) {
    if (miniKey[miniCur] >= 0) {
      miniCur ^= 1;
      miniCruza = ajustes_animacoes_reduzidas() ? 1.0f : 0.0f;
    }
    miniKey[miniCur] = k;
    miniPronto[miniCur] = 0;
    miniVoltas[miniCur] = 0;
  }
  if (!miniPronto[miniCur]) renderCena(miniCur, c);
  gfx_rect((GfxRect){ r.x - 40, r.y, r.w + 80, r.h + 90 }, 0, GFX_SOMBRA, 1, 0, 0, 0.5f, 0, 0, 0, 0.5f * a);
  if (!mini[miniCur].tex) { gfx_cor(r, 40.0f / r.h, 0.06f, 0.065f, 0.08f, a); return; }
  if (miniCruza < 1.0f && mini[miniCur ^ 1].tex && miniKey[miniCur ^ 1] >= 0) {
    gfx_mini_desenhar(&mini[miniCur ^ 1], r, 40, a);
    gfx_mini_desenhar(&mini[miniCur], r, 40, a * anim_suave(miniCruza));
  } else gfx_mini_desenhar(&mini[miniCur], r, 40, a);
}
static void liberarMinis(void) {
  int i;
  for (i = 0; i < 2; i++) { if (mini[i].fbo) gfx_mini_liberar(&mini[i]); miniKey[i] = -1; miniPronto[i] = 0; }
}

void novidades20_atualizar(float dt, Uint32 agora) {
  int red = ajustes_animacoes_reduzidas();
  (void)agora;
  if (!aberto && entrada < 0.002f) {
    entrada = 0.0f;
    if (mini[0].fbo || mini[1].fbo) liberarMinis();
    return;
  }
  if (aberto) pedirArtes();
  if (red) { entrada = aberto ? 1.0f : 0.0f; troca = 1.0f; miniCruza = 1.0f; sairA = saindo ? 1.0f : 0.0f; return; }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt, aberto ? 320.0f : 180.0f);
  if (troca < 1.0f) { troca += dt / 0.42f; if (troca > 1.0f) troca = 1.0f; }
  if (miniCruza < 1.0f) { miniCruza += dt / 0.35f; if (miniCruza > 1.0f) miniCruza = 1.0f; }
  sairA = anim_rampa(sairA, saindo ? 1.0f : 0.0f, dt, saindo ? 300.0f : 160.0f);
}

// ================================================================== DESENHO
static float botao(const char *rot, const char *ic, float x, float y, float h, TxtEstilo e, int foco, float a) {
  TxtLinha t = foco ? txtT(e, rot) : txt(e, rot);
  float pad = h * 0.5f, isz = h * 0.33f, w = pad + (ic ? isz + 14 : 0) + t.w + pad;
  GfxRect r = { x, y, w, h };
  if (foco) pilulaAc(r, a);
  else neutro(r, h * 0.5f, 0.085f, a);
  if (ic) { GfxRect ir = { x + pad, y + (h - isz) * 0.5f, isz, isz }; if (foco) iconeT(ic, ir, a); else icone(ic, ir, 0.9f * a); }
  txtMeio(t, x + pad + (ic ? isz + 14 : 0), y, h, (foco ? 1.0f : 0.92f) * a);
  return w;
}
static float botaoLarg(const char *rot, const char *ic, float h, TxtEstilo e) {
  return h + (ic ? h * 0.33f + 14 : 0) + (float)txt_largura(e, rot);
}
// Tecla das dicas: a pilula da tecla e o rotulo. `azul` = a tecla AZUL.
static float tecla(const char *k, const char *rot, float x, float y, int azul, float a) {
  TxtLinha kt = azul ? txt_linha(TXT_AJ_16B, k, 255, 255, 255, 255) : txt(TXT_AJ_16B, k), rt = txt(TXT_G18R, rot);
  float kw = kt.w + 20 < 42 ? 42 : kt.w + 20;
  if (azul) gfx_cor((GfxRect){ x, y, kw, 38 }, 12.0f / 38, 0.169f, 0.424f, 0.941f, a);
  else neutro((GfxRect){ x, y, kw, 38 }, 12, 0.14f, a);
  txtMeio(kt, x + (kw - kt.w) * 0.5f, y, 38, a);
  if (rot && rot[0]) txtMeio(rt, x + kw + 10, y, 38, 0.6f * a);
  return kw + (rot && rot[0] ? 10 + rt.w : 0) + 26;
}
static void ponteiroNada(int a, int b) { (void)a; (void)b; }
static void ptHeroFoco(int b, int z) { (void)z; focoHero = b; }
static void ptHeroOk(int b, int z) { (void)z; focoHero = b; okHero(b); }
static void ptFimFoco(int b, int z) { (void)z; focoFim = b; }
static void ptFimOk(int b, int z) { (void)z; focoFim = b; okFim(botaoFim(b)); }
static void ptSairFoco(int b, int z) { (void)z; focoSair = b; }
static void ptSairOk(int b, int z) { (void)z; focoSair = b; okSair(b); }
static void ptIr(int i, int z) { (void)z; irPara(i); }
static void ptLater(int a, int z) { (void)a; (void)z; fechar(AV_GUARDADO, N20_PEDIU_NADA); }
static void ptAcao(int a, int z) {
  (void)z;
  if (a == 0) irPara(idx - 1);
  else if (a == 1) irPara(idx + 1);
  else if (a == 2) { int t = tipo(); if (t >= 0) est[t] = (est[t] + 1) % nEst(t); else irPara(idx + 1); }
  else if (a == 3) { saindo = 1; focoSair = 0; }
  else if (a == 4) { if (idx >= nLista - 2) irPara(0); else irPara(nLista - 2); }
}

static void fundo(const char *rel, float a) {
  GfxRect t = { 0, 0, NV_TELA_W, NV_TELA_H };
  fundoArte(rel, t, 0, a * (0.55f + 0.45f * anim_suave(troca)));
  gfx_veu_css_base(t, 0, 1.0f, 1.0f, 0.45f * a, 0.35f * a);
}

// Topo: logo, "2.0", pontos e "n / N".
static void topo(float a) {
  float x = 96, y = 44, xd;
  int i;
  x += marca(logoapp_caminho(logoapp_atual(), LOGO_F_HORIZONTAL), x, y + 9, 230, 46, 0, a) + 28;
  { TxtLinha t = txtA(TXT_AJ_KBD, N20_VERSAO);
    float w = t.w + 30, ar, ag, ab;
    acento(&ar, &ag, &ab);
    gfx_cor((GfxRect){ x, y + 14, w, 36 }, 0.5f, ar, ag, ab, a);
    gfx_cor((GfxRect){ x + 1.5f, y + 15.5f, w - 3, 33 }, 0.5f, 0.05f, 0.055f, 0.07f, a);
    txtMeio(t, x + 15, y + 14, 36, a); }
  { char n[24];
    TxtLinha c;
    snprintf(n, sizeof n, "%d / %d", idx + 1, nLista);
    c = txt(TXT_AJ_16B, n);
    xd = 1824 - (c.w > 90 ? c.w : 90);
    txt_desenhar_alpha(c, 1824 - c.w, y + 32 - c.h * 0.5f, 0.6f * a);
    xd -= 24; }
  { float w = 0, dx, ar, ag, ab;
    acento(&ar, &ag, &ab);
    for (i = 0; i < nLista; i++) w += (i == idx ? 44 : 12) + (i ? 10 : 0) + (i == nLista - 2 ? 14 : 0);
    dx = xd - w;
    for (i = 0; i < nLista; i++) {
      float dw = i == idx ? 44 : 12;
      if (i) dx += 10;
      if (i == nLista - 2) dx += 14;
      if (i == idx) gfx_cor((GfxRect){ dx, y + 26, dw, 12 }, 0.5f, ar, ag, ab, a);
      else gfx_cor((GfxRect){ dx, y + 26, dw, 12 }, 0.5f, 1, 1, 1, (i < idx ? 0.6f : 0.22f) * a);
      if (aberto && !saindo) ponteiro_alvo(dx - 4, y + 18, dw + 8, 28, ponteiroNada, ptIr, i, 0);
      dx += dw;
    } }
}

static void dicas(float a) {
  int t = tipo();
  float x = 96, y = 1080 - 22 - 48;
  if (t == N20_HERO) {
    x += tecla("↑ ↓", "botão", x, y, 0, a);
    x += tecla("OK", "escolher", x, y, 0, a);
    x += tecla("→", "começar", x, y, 0, a);
    tecla("Voltar", "sair", x, y, 0, a);
    return;
  }
  { float x0 = x;
    x += tecla("←", "anterior", x, y, 0, a);
    if (aberto && !saindo) ponteiro_alvo(x0, y, x - x0 - 26, 38, ponteiroNada, ptAcao, 0, 0);
    if (t != N20_FIM) { x0 = x; x += tecla("→", "próximo", x, y, 0, a); if (aberto && !saindo) ponteiro_alvo(x0, y, x - x0 - 26, 38, ponteiroNada, ptAcao, 1, 0); }
    x0 = x;
    x += tecla("OK", t >= 0 ? "trocar estado" : t == N20_FIM ? "escolher" : "continuar", x, y, 0, a);
    if (aberto && !saindo) ponteiro_alvo(x0, y, x - x0 - 26, 38, ponteiroNada, ptAcao, 2, 0);
    x0 = x;
    x += tecla("Voltar", "sair", x, y, 0, a);
    if (aberto && !saindo) ponteiro_alvo(x0, y, x - x0 - 26, 38, ponteiroNada, ptAcao, 3, 0); }
  { const char *rot = idx >= nLista - 2 ? "rever do início" : "pular para o resumo";
    TxtLinha lt = txt(TXT_G18R, "Ver depois");
    float wl = t >= 0 ? lt.w + 44 : 0, wb, xd = 1824;
    if (t >= 0) {
      GfxRect r = { xd - wl, y - 2, wl, 42 };
      neutro(r, 21, 0.085f, a);
      txtMeio(lt, r.x + 22, r.y, 42, a);
      if (aberto && !saindo) ponteiro_alvo(r.x, r.y, r.w, r.h, ponteiroNada, ptLater, 0, 0);
      xd -= wl + 22;
    }
    wb = tecla("Azul", rot, -10000, y, 1, 0) - 26;
    tecla("Azul", rot, xd - wb, y, 1, a);
    if (aberto && !saindo) ponteiro_alvo(xd - wb, y, wb, 38, ponteiroNada, ptAcao, 4, 0); }
}

// Texto quebrado em `w` e CENTRADO em cx, linha a linha (ate maxL).
static float centrado(const char *s, TxtEstilo e, float cx, float y, float w, float lead, float a, int maxL) {
  const char *p = i18n(s);
  int nl = 0;
  while (*p && nl < maxL) {
    const char *q = p, *corte = NULL;
    char tmp[600];
    TxtLinha l;
    while (*q) {
      size_t n = txt_token_tam(q);
      const char *fim = q + (n ? n : 1);
      snprintf(tmp, sizeof tmp, "%.*s", (int)(fim - p), p);
      if (corte && txt_largura(e, tmp) > w) break;
      corte = fim;
      q = fim;
      while (*q == ' ') q++;
    }
    if (!corte) break;
    if (nl == maxL - 1) corte = p + strlen(p);
    snprintf(tmp, sizeof tmp, "%.*s", (int)(corte - p), p);
    l = txtC(e, tmp, w);
    txt_desenhar_alpha(l, cx - l.w * 0.5f, y + nl * lead, a);
    nl++;
    p = corte;
    while (*p == ' ') p++;
  }
  return nl * lead;
}

// --- HERO
static void telaHero(float a, float dy) {
  float y;
  int i;
  fundo("21.jpg", a);
  gfx_veu_css((GfxRect){ 0, 0, NV_TELA_W, 260 }, 1, 1.0f, 1.0f, 0.5f * a);
  marca(logoapp_caminho(logoapp_atual(), LOGO_F_HORIZONTAL), 96, 53 + dy, 230, 46, 0, a);
  // O simbolo, com a luz violeta do mockup por tras.
  gfx_rect((GfxRect){ 960 - 260, 70 + dy, 520, 420 }, 0, GFX_SOMBRA, 1, 0, 0, 0.5f, 0.47f, 0.31f, 1.0f, 0.32f * a);
  marca(logoapp_caminho(logoapp_atual(), LOGO_F_SIMBOLO), 960, 92 + dy, 320, 330, 1, a);
  { TxtLinha n = txt(TXT_W20_HERO, "Nuvio "), v = txt_linha(TXT_W20_HERO, N20_VERSAO, 138, 122, 255, 255);
    float x = 960 - (n.w + v.w) * 0.5f;
    txt_desenhar_alpha(n, x, 446 + dy, a);
    txt_desenhar_alpha(v, x + n.w, 446 + dy, a); }
  y = 610 + dy;
  centrado(novo ? "Boas-vindas ao Nuvio. Este tour mostra o que a 2.0 trouxe; o resto do app está no Guia de uso. Dá uns quatro minutos."
                 : "Visual novo, servidores da sua casa e um monte de coisa que a gente tirou do caminho. Dá uns quatro minutos.",
           TXT_G28R, 960, y, 1100, 42, 0.82f * a, 3);
  { static const char *const R[3] = { "Começar o guia", "Só o que muda pra mim", "Ver depois" };
    static const char *const IC[3] = { "aj_play", NULL, NULL };
    float w = 0, x;
    for (i = 0; i < 3; i++) w += botaoLarg(R[i], IC[i], 78, TXT_G26B) + (i ? 20 : 0);
    x = 960 - w * 0.5f;
    for (i = 0; i < 3; i++) {
      float bw = botao(R[i], IC[i], x, 740 + dy, 78, TXT_G26B, focoHero == i && !saindo, a);
      if (aberto && !saindo) ponteiro_alvo(x, 740 + dy, bw, 78, ptHeroFoco, ptHeroOk, i, 0);
      x += bw + 20;
    } }
  { TxtLinha k = txt(TXT_G18R, "Seu aparelho"), d = txt_linha(TXT_AJ_16B, devNome(), 16, 18, 22, 255);
    float w = k.w + 14 + d.w + 36, x = 960 - w * 0.5f;
    txtMeio(k, x, 856 + dy, 40, 0.6f * a);
    gfx_cor((GfxRect){ x + k.w + 14, 856 + dy, d.w + 36, 40 }, 0.5f, 1, 1, 1, 0.9f * a);
    txtMeio(d, x + k.w + 32, 856 + dy, 40, a); }
  dicas(a);
}

// --- CAPITULO
static float etiqueta(const char *s, int tipoTag, float x, float y, float a) {
  TxtLinha t;
  float w;
  char up[120];
  idioma_maiusc_em(ajustes_idioma(), up, sizeof up, i18n(s));
  if (tipoTag == TG_AMARELO) t = txt_linha(TXT_AJ_CAPS13, up, 36, 26, 2, 255);
  else if (tipoTag == TG_ACENTO) t = txtT(TXT_AJ_CAPS13, up);
  else t = txt_linha(TXT_AJ_CAPS13, up, 255, 255, 255, 255);
  w = t.w + 20;
  if (a > 0) {
    float ar, ag, ab;
    acento(&ar, &ag, &ab);
    if (tipoTag == TG_AMARELO) gfx_cor((GfxRect){ x, y, w, 26 }, 0.5f, 0.949f, 0.804f, 0.392f, a);
    else if (tipoTag == TG_ACENTO) gfx_cor((GfxRect){ x, y, w, 26 }, 0.5f, ar, ag, ab, a);
    else gfx_cor((GfxRect){ x, y, w, 26 }, 0.5f, 1, 1, 1, 0.16f * a);
    txtMeio(t, x + 10, y, 26, a);
  }
  return w;
}
static float faixaItem(int k, int c) {
  return k == c ? 20 + 22 + 9 + (float)txt_largura(TXT_AJ_16B, CAP[k].nome) + 20 : 50;
}
static void faixa(int c, float a) {
  GfxRect r = { 96, 858, 1728, 104 };
  float x = r.x + 20;
  int i = 1;
  ilhaM(r, 34, a);
  while (i < nLista - 2) {
    int g = CAP[lista[i]].g, j, ini = i;
    float wi = 0, wl = capsLarg(TXT_AJ_MINI12, GRUPO[g], 2.2f) + 4, gx = x;
    for (j = i; j < nLista - 2 && CAP[lista[j]].g == g; j++) wi += faixaItem(lista[j], c) + (j > i ? 6 : 0);
    caps(TXT_AJ_MINI12, GRUPO[g], 2.2f, x + 4, r.y + 14, TX, 0.5f * a);
    for (j = ini; j < nLista - 2 && CAP[lista[j]].g == g; j++) {
      int k = lista[j];
      float y = r.y + 44, w = faixaItem(k, c);
      if (j > ini) gx += 6;
      if (k == c) {
        TxtLinha t = txtT(TXT_AJ_16B, CAP[k].nome);
        pilulaAc((GfxRect){ gx, y, w, 48 }, a);
        iconeT(CAP[k].ic, (GfxRect){ gx + 20, y + 13, 22, 22 }, a);
        txtMeio(t, gx + 51, y, 48, a);
      } else icone(CAP[k].ic, (GfxRect){ gx + 14, y + 13, 22, 22 }, 0.62f * a);
      if (aberto && !saindo) ponteiro_alvo(gx, y, w, 48, ponteiroNada, ptIr, j, 0);
      gx += w;
    }
    x += (wi > wl ? wi : wl) + 26;
    i = j;
  }
}
static void telaCap(int c, float a, float dx) {
  const Cap *C = &CAP[c];
  int st = est[c] % nEst(c), i;
  float y, x = 1150 + dx, w = 690, a0;
  fundo(C->arte, a);
  topo(a);
  peek(c, a);
  // Legenda da cena: OK e o nome do estado, os pontos a direita.
  { float px = 96, py = 730, kw;
    TxtLinha k = txt(TXT_AJ_16B, "OK"), s = txt(TXT_ILHA_SUB, C->st[st]);
    kw = 18 + 14 + 8 + k.w + 18;
    neutro((GfxRect){ px, py, kw, 40 }, 20, 0.14f, a);
    icone("aj_play", (GfxRect){ px + 18, py + 13, 14, 14 }, a);
    txtMeio(k, px + 40, py, 40, a);
    txtMeio(s, px + kw + 14, py, 40, 0.6f * a);
    { float ar, ag, ab, sx = 96 + 1000 - nEst(c) * 20 + 8;
      acento(&ar, &ag, &ab);
      for (i = 0; i < nEst(c); i++) {
        if (i == st) gfx_cor((GfxRect){ sx + i * 20, py + 14, 12, 12 }, 0.5f, ar, ag, ab, a);
        else gfx_cor((GfxRect){ sx + i * 20, py + 14, 12, 12 }, 0.5f, 1, 1, 1, 0.25f * a);
      } } }
  // Coluna da direita.
  a0 = a;
  a *= anim_suave(troca);
  y = 136;
  { char k[96];
    float kx;
    snprintf(k, sizeof k, i18n("Capítulo %d de %d"), posCap(c), nCapsVis());
    kx = x + capsA(TXT_AJ_16B, k, 3.4f, x, y, a);
    kx += 10;
    { TxtLinha d = txt(TXT_AJ_16B, "·"); txt_desenhar_alpha(d, kx, y - 2, 0.5f * a); kx += d.w + 10; }
    kx += capsA(TXT_AJ_16B, GRUPO[C->g], 3.4f, kx, y, a);
    if (C->exp) etiqueta("Experimental", TG_AMARELO, kx + 14, y - 4, a); }
  y += 32;
  { TxtLinha t = txtC(TXT_V2_TIT, C->nome, w);
    txt_desenhar_alpha(t, x, y, a);
    y += t.h + 22; }
  for (i = 0; i < 3; i++) {
    const Linha *l = &C->l[i];
    int tem = temAqui(l->so);
    float la = tem ? 1.0f : 0.42f, fimX, h, lead = 33;
    float ar, ag, ab;
    acento(&ar, &ag, &ab);
    gfx_cor((GfxRect){ x, y + 4, 30, 30 }, 0.5f, ar, ag, ab, la * a);
    iconeT("aj_check", (GfxRect){ x + 6, y + 10, 18, 18 }, la * a);
    h = rico(i18n(l->t), TXT_V2_24, TXT_W20_24B, x + 46, y, w - 46, lead, 0.9f * la * a, la * a, &fimX, 0);
    { const char *tg = tem ? l->tag : "Não neste aparelho";
      int tt = tem ? l->tipo : TG_CINZA;
      if (tg) {
        float ew = etiqueta(tg, tt, 0, 0, 0);
        if (fimX + 10 + ew <= x + w) etiqueta(tg, tt, fimX + 10, y + h - lead + 3.5f, la * a);
        else { etiqueta(tg, tt, x + 46, y + h + 2, la * a); h += lead; }
      } }
    y += h + 14;
  }
  // "Também": chips em fluxo.
  y += 6;
  { float cx = x, ch = 38;
    TxtLinha lb;
    char up[60];
    idioma_maiusc_em(ajustes_idioma(), up, sizeof up, i18n("Também"));
    lb = txt(TXT_AJ_KBD, up);
    cx += txt_tracking(TXT_AJ_KBD, up, TX, cx, y + (ch - lb.h) * 0.5f, 0.55f * a, 2.0f) + 14;
    for (i = 0; i < 6 && C->e[i].t; i++) {
      const Linha *e = &C->e[i];
      int tem = temAqui(e->so);
      char s[200];
      TxtLinha t, tg = { 0 };
      float cw, sp;
      snprintf(s, sizeof s, "%s", i18n(e->t));
      t = txt(TXT_ILHA_APOIO, s);
      if (e->tag) { char tt[100]; snprintf(tt, sizeof tt, "· %s", i18n(tem ? e->tag : "Não neste aparelho")); tg = txt(TXT_ILHA_APOIO, tt); }
      else if (!tem) { char tt[100]; snprintf(tt, sizeof tt, "· %s", i18n("Não neste aparelho")); tg = txt(TXT_ILHA_APOIO, tt); }
      sp = tg.w ? espacoW(TXT_ILHA_APOIO) : 0;
      cw = 16 + t.w + sp + tg.w + 16;
      if (cx + cw > x + w) { cx = x; y += ch + 8; }
      neutro((GfxRect){ cx, y, cw, ch }, ch * 0.5f, 0.085f, (tem ? 1.0f : 0.4f) * a);
      txtMeio(t, cx + 16, y, ch, (tem ? 0.85f : 0.4f) * a);
      if (tg.w) txtMeio(tg, cx + 16 + t.w + sp, y, ch, (tem ? 0.6f : 0.35f) * a);
      if (!tem) gfx_cor((GfxRect){ cx + 14, y + ch * 0.5f, t.w + sp + tg.w + 4, 1.5f }, 0, 1, 1, 1, 0.45f * a);
      cx += cw + 8;
    } }
  faixa(c, a0);
  dicas(a0);
}

// --- RESUMO
static void item3(const char *b, const char *s, int marcaT, float x, float y, float w, float a) {   // 0 check, 1 "!", 2 "–"
  float ar, ag, ab;
  acento(&ar, &ag, &ab);
  if (marcaT == 0) { gfx_cor((GfxRect){ x, y + 3, 28, 28 }, 0.5f, ar, ag, ab, a); iconeT("aj_check", (GfxRect){ x + 6, y + 9, 16, 16 }, a); }
  else if (marcaT == 1) { TxtLinha t = txt_linha(TXT_AJ_KBD, "!", 36, 26, 2, 255); gfx_cor((GfxRect){ x, y + 3, 28, 28 }, 0.5f, 0.949f, 0.804f, 0.392f, a); txtMeio(t, x + 14 - t.w * 0.5f, y + 3, 28, a); }
  else { TxtLinha t = txt(TXT_AJ_KBD, "–"); gfx_cor((GfxRect){ x, y + 3, 28, 28 }, 0.5f, 1, 1, 1, 0.14f * a); txtMeio(t, x + 14 - t.w * 0.5f, y + 3, 28, a); }
  { TxtLinha t1 = txt_linha_corta(TXT_G21B, b, 255, 255, 255, 255, w - 42), t2 = txtC(TXT_ILHA_GENERO, s, w - 42);
    txt_desenhar_alpha(t1, x + 42, y + 2, a);
    txt_desenhar_alpha(t2, x + 42, y + 30, 0.6f * a); }
}
static void telaResumo(float a, float dy) {
  float y = 150 + dy, cw[3], cx, x = 96;
  int i;
  float a0 = a;
  fundo("13.jpg", a);
  topo(a);
  a *= anim_suave(troca);
  { TxtLinha t = txt(TXT_AJ_NUM64, "O que muda pra você"); txt_desenhar_alpha(t, x, y, a); y += t.h + 10; }
  { char l[300];
    TxtLinha t;
    snprintf(l, sizeof l, "%s%s%s", essencial ? i18n("Só o essencial.") : "", essencial ? " " : "",
             i18n("Tudo isso já vem na 2.0. O que for experimental fica desligado até você ligar."));
    t = txtC(TXT_V2_26, l, 1728);
    txt_desenhar_alpha(t, x, y, 0.6f * a);
    y += t.h + 34; }
  cw[0] = (1728 - 52) * 1.15f / 3.15f; cw[1] = cw[2] = (1728 - 52) / 3.15f;
  cx = x;
  for (i = 0; i < 3; i++) {
    GfxRect r = { cx, y, cw[i], 584 };
    float iy = y + 30, ix = cx + 34, iw = cw[i] - 68;
    ilhaM(r, 36, a);
    if (i == 0) {
      int c;
      capsA(TXT_AJ_KBD, "Você vai notar logo", 3.0f, ix, iy, a);
      iy += 40;
      int n = 0;
      float passo;
      for (c = 0; c < N20_NCAP; c++) if (!CAP[c].exp) n++;
      passo = n > 9 ? (584.0f - 70.0f - 20.0f) / (float)n : 55.0f;   // 9 cabiam a 55
      for (c = 0; c < N20_NCAP; c++) if (!CAP[c].exp) { item3(CAP[c].nome, CAP[c].sum, 0, ix, iy, iw, a); iy += passo; }
    } else if (i == 1) {
      static const char *const B[4] = { "Plugins", "Servidor P2P", "Opacidade do vidro / Vidro fosco", "Receber enquetes" };
      static const char *const S[4] = { "Desligado, experimental", "Desligado, experimental", "Opções de teste", "Ligado, dá pra desligar" };
      int k;
      capsA(TXT_AJ_KBD, "Vem desligado ou em teste", 3.0f, ix, iy, a);
      iy += 40;
      for (k = 0; k < 4; k++) { item3(B[k], S[k], 1, ix, iy, iw, a); iy += 64; }
    } else {
      static const char *const B[6] = { "Plugins", "P2P", "Opacidade do vidro e Vidro fosco", "Zoom do trailer (Tizen .tpk)", "AutoSync pelo áudio", "Cache de busca e volume até 200%" };
      static const int SO[6] = { ALS, ALS, 0, DS, DA, DA };
      char h[96];
      int k;
      snprintf(h, sizeof h, i18n("No seu aparelho · %s"), devNome());
      capsA(TXT_AJ_KBD, h, 3.0f, ix, iy, a);
      iy += 40;
      for (k = 0; k < 6; k++) {
        int ok = temAqui(SO[k]);
        item3(B[k], ok ? "Disponível" : "Não neste aparelho", ok ? 0 : 2, ix, iy, iw, (ok ? 1.0f : 0.5f) * a);
        iy += 64;
      }
    }
    cx += cw[i] + 26;
  }
  dicas(a0);
}

// --- FIM
static void telaFim(float a, float dy) {
  GfxRect p = { 96, 150 + dy, 880, 700 };
  float y, x;
  int i;
  float a0 = a;
  fundo("00.jpg", a);
  topo(a);
  a *= anim_suave(troca);
  { int s0 = sol; sol = 1; ilhaM(p, 40, a); sol = s0; }
  x = p.x + 34; y = p.y + 34;
  caps(TXT_AJ_KBD, "Ajustes", 3.0f, x + 8, y + 6, TX, 0.55f * a);
  y += 38;
  { static const char *const IC[4] = { "aj_panel-top", "aj_captions", "aj_zap", "aj_users" };
    static const char *const N[4] = { "Aparência", "Reprodução", "Desempenho desta TV", "Contas e serviços" };
    for (i = 0; i < 4; i++) {
      icone(IC[i], (GfxRect){ x + 24, y + 24, 30, 30 }, 0.85f * a);
      txtMeio(txt(TXT_V2_24, N[i]), x + 72, y, 78, 0.85f * a);
      y += 80;
    } }
  gfx_cor((GfxRect){ x, y, p.w - 68, 78 }, 26.0f / 78, 1, 1, 1, 0.12f * a);
  icone("aj_book-open", (GfxRect){ x + 24, y + 24, 30, 30 }, a);
  txtMeio(txt(TXT_W20_24B, "Sobre e ajuda"), x + 72, y, 78, a);
  { TxtLinha t = txt(TXT_G18R, "›"); txtMeio(t, x + p.w - 68 - 24 - t.w, y, 78, 0.6f * a); }
  y += 92;
  caps(TXT_AJ_KBD, "Sobre e ajuda", 3.0f, x + 8, y, TX, 0.55f * a);
  y += 28;
  icone("aj_star", (GfxRect){ x + 58, y + 25, 28, 28 }, 0.85f * a);
  txtMeio(txt(TXT_V2_24, "Novidades 2.0"), x + 104, y, 78, 0.85f * a);
  { TxtLinha t = txt(TXT_G18R, "este guia"); txtMeio(t, x + p.w - 68 - 24 - t.w, y, 78, 0.6f * a); }
  y += 82;
  { GfxRect r = { x + 34, y, p.w - 68 - 34, 78 };
    TxtLinha t = txtT(TXT_W20_24B, "Guia de uso"), s = txtT(TXT_G18R, "tudo explicado");
    pilulaAc(r, a);
    iconeT("aj_book-open", (GfxRect){ r.x + 24, y + 25, 28, 28 }, a);
    txtMeio(t, r.x + 70, y, 78, a);
    txtMeio(s, r.x + r.w - 24 - s.w, y, 78, 0.8f * a); }
  // A direita.
  x = 1040; y = 150 + dy;
  capsA(TXT_AJ_16B, "Fim do guia", 3.4f, x, y, a);
  y += 34;
  y += txt_bloco(TXT_AJ_NUM64, "Dúvida depois? O Guia de uso fica à mão.", TX, x, y, 790, 68, a, 3) + 26;
  { static const char *const C[3] = { "Ajustes", "Sobre e ajuda", "Guia de uso" };
    float cx = x, ar, ag, ab;
    acento(&ar, &ag, &ab);
    for (i = 0; i < 3; i++) {
      TxtLinha t = i == 2 ? txtT(TXT_V2_24, C[i]) : txt(TXT_V2_24, C[i]);
      float w = t.w + 40;
      if (cx + w > x + 790) { cx = x; y += 60; }
      if (i == 2) gfx_cor((GfxRect){ cx, y, w, 50 }, 0.5f, ar, ag, ab, a);
      else neutro((GfxRect){ cx, y, w, 50 }, 25, 0.14f, a);
      txtMeio(t, cx + 20, y, 50, a);
      cx += w + 10;
      if (i < 2) { TxtLinha s = txt(TXT_V2_24, "›"); txtMeio(s, cx, y, 50, 0.55f * a); cx += s.w + 10; }
    }
    y += 50 + 28; }
  { static const char *const L[2] = { "O Guia de uso explica o que cada coisa faz, com busca. Dá pra abrir direto no recurso.",
                                      "Quer rever este tour? Está em <b>Sobre e ajuda › Novidades 2.0</b>." };
    float ar, ag, ab;
    acento(&ar, &ag, &ab);
    for (i = 0; i < 2; i++) {
      gfx_cor((GfxRect){ x, y + 4, 30, 30 }, 0.5f, ar, ag, ab, a);
      iconeT("aj_check", (GfxRect){ x + 6, y + 10, 18, 18 }, a);
      y += rico(i18n(L[i]), TXT_V2_24, TXT_W20_24B, x + 46, y, 744, 33, 0.9f * a, a, NULL, 0) + 14;
    } }
  y += 22;
  { static const char *const R[3] = { "Abrir o Guia de uso", "Concluir", "Rever do começo" };
    static const char *const IC[3] = { "aj_book-open", NULL, "aj_rotate-ccw-clock" };
    float bx = x;
    for (i = 0; i < nBotoesFim(); i++) {
      int b = botaoFim(i);
      float bw = botao(R[b], IC[b], bx, y, 72, TXT_ILHA_ITEM, focoFim == i && !saindo, a);
      if (aberto && !saindo) ponteiro_alvo(bx, y, bw, 72, ptFimFoco, ptFimOk, i, 0);
      bx += bw + 14;
    } }
  dicas(a0);
}

// --- "SAIR DO GUIA?" — cresce da ilha do relogio (canto de cima a direita).
static void dialogo(void) {
  float s = anim_suave(sairA), ix, iy, iw, ih;
  GfxRect alvo = { 960 - 440, 0, 880, 0 }, r;
  float y;
  int i;
  if (s <= 0.003f) return;
  // A altura: titulo + texto (ate 3 linhas) + botoes.
  { float h = rico(i18n("Você pode terminar depois. O guia fica guardado em <b>Ajustes › Sobre e ajuda › Novidades 2.0</b> e não abre sozinho de novo."),
                   TXT_V2_24, TXT_W20_24B, 0, 0, 880 - 96, 33, -1, -1, NULL, 0);
    alvo.h = 44 + 52 + 14 + h + 32 + 68 + 44;
    alvo.y = 540 - alvo.h * 0.5f; }
  if (!ilha_rect(&ix, &iy, &iw, &ih) || iw < 10) { iw = 240; ih = 64; ix = 1824 - iw; iy = 44; }
  r.x = ix + (alvo.x - ix) * s; r.y = iy + (alvo.y - iy) * s;
  r.w = iw + (alvo.w - iw) * s; r.h = ih + (alvo.h - ih) * s;
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0, 0.02f, 0.024f, 0.03f, 0.62f * s);
  ponteiro_camada();
  ilhaModal = 1;
  ilhaM(r, 44, s);
  ilhaModal = 0;
  if (s < 0.6f) return;
  { float a = (s - 0.6f) / 0.4f;
    float x = r.x + 48;
    y = r.y + 44;
    sTxt(TXT_V2_INSP, "Sair do guia?", x, y, a);
    y += 52 + 14;
    y += rico(i18n("Você pode terminar depois. O guia fica guardado em <b>Ajustes › Sobre e ajuda › Novidades 2.0</b> e não abre sozinho de novo."),
              TXT_V2_24, TXT_W20_24B, x, y, r.w - 96, 33, 0.6f * a, 0.95f * a, NULL, 0) + 32;
    { static const char *const R[3] = { "Continuar", "Ver depois", "Sair mesmo assim" };
      float bx = x;
      for (i = 0; i < 3; i++) {
        float bw = botao(R[i], NULL, bx, y, 68, TXT_ILHA_ITEM, focoSair == i, a);
        if (saindo) ponteiro_alvo(bx, y, bw, 68, ptSairFoco, ptSairOk, i, 0);
        bx += bw + 14;
      } } }
}

#ifdef NV_TOUCH_UI
static int n20TelefoneRolar(const PonteiroRolagem *e) { return aberto && toquerol_evento(&n20Telefone.rolagem, e); }
static void n20TelefoneAcao(int i, int acao) {
  (void)i;
  if (aberto && !saindo && acao >= 0 && acao <= 4) ptAcao(acao, 0);
}
static void n20TelefoneEstado(int s, int c) {
  if (!aberto || saindo || tipo() != c || c < 0 || c >= N20_NCAP || s < 0 || s >= nEst(c)) return;
  est[c] = s;
}
static float n20TelefoneRico(const char *s, float x, float y, float w, float a) {
  char texto[2048]; size_t n = 0;
  const char *p = i18n(s);
  // Keep the existing words. Bold markers belong to the TV rich-text parser;
  // phone text wraps and bounds even a single overlong translated word.
  while (*p && n + 1 < sizeof texto) {
    if (!strncmp(p, "<b>", 3)) p += 3;
    else if (!strncmp(p, "</b>", 4)) p += 4;
    else texto[n++] = *p++;
  }
  texto[n] = 0;
  return telefonecartao_texto(TXT_V2_24, texto, x, y, w, 33, a);
}
static float n20TelefoneControle(const char *rot, float x, float y, float w, int f,
                                 PonteiroFn ativar, int i, int b, float a) {
  if (a > 0) telefonecartao_botao_em(&n20Telefone, (GfxRect){x, y, w, 76}, rot, f,
                                      NULL, ativar, i, b, a, 1);
  return 88;
}
static void n20TelefonePrevia(int c, GfxRect r, float a) {
  int k = chaveCena(c);
  if (miniKey[miniCur] != k) {
    miniKey[miniCur] = k; miniPronto[miniCur] = 0; miniVoltas[miniCur] = 0;
  }
  if (!miniPronto[miniCur]) renderCena(miniCur, c);
  if (mini[miniCur].tex) gfx_mini_desenhar(&mini[miniCur], r, 20, a);
}
static float n20TelefoneCapitulos(float x, float y, float w, float a) {
  float inicio = y;
  y += telefonecartao_texto(TXT_AJ_SEG, "Capítulos", x, y, w, 30, .6f * a) + 16;
  for (int i = 0; i < nLista; i++) {
    int c = lista[i];
    const char *rot = c == N20_HERO ? "Rever do começo" : c == N20_RESUMO ? "O que muda pra você" :
                      c == N20_FIM ? "Fim do guia" : CAP[c].nome;
    y += n20TelefoneControle(rot, x, y, w, i == idx, ptIr, i, 0, a);
  }
  return y - inicio;
}
static float n20TelefoneConteudo(float x, float y, float w, float a) {
  float inicio = y; int t = tipo();
  if (saindo) {
    y += telefonecartao_titulo("Sair do guia?", x, y, w, a);
    y += n20TelefoneRico("Você pode terminar depois. O guia fica guardado em <b>Ajustes › Sobre e ajuda › Novidades 2.0</b> e não abre sozinho de novo.", x, y, w, .7f * a);
    return y - inicio + 24;
  }
  if (t == N20_HERO) {
    y += telefonecartao_titulo("Nuvio 2.0", x, y, w, a);
    y += telefonecartao_texto(TXT_G28R,
       novo ? "Boas-vindas ao Nuvio. Este tour mostra o que a 2.0 trouxe; o resto do app está no Guia de uso. Dá uns quatro minutos."
            : "Visual novo, servidores da sua casa e um monte de coisa que a gente tirou do caminho. Dá uns quatro minutos.",
       x, y, w, 42, .82f * a) + 24;
    y += telefonecartao_texto(TXT_G18R, "Seu aparelho", x, y, w, 28, .6f * a) + 8;
    y += telefonecartao_texto(TXT_AJ_16B, devNome(), x, y, w, 28, a) + 24;
  } else if (t >= 0) {
    const Cap *C = &CAP[t]; char capitulo[96];
    snprintf(capitulo, sizeof capitulo, i18n("Capítulo %d de %d"), posCap(t), nCapsVis());
    y += telefonecartao_texto(TXT_AJ_16B, capitulo, x, y, w, 28, .6f * a) + 8;
    y += telefonecartao_texto(TXT_AJ_SEG, GRUPO[C->g], x, y, w, 30, .6f * a) + 12;
    y += telefonecartao_titulo(C->nome, x, y, w, a);
    if (C->exp) y += telefonecartao_texto(TXT_AJ_SEG, "Experimental", x, y, w, 30, .7f * a) + 12;
    float ph = fminf(320, n20Telefone.corpo.h * .5f), pw = ph * PEEK_W / PEEK_H;
    if (pw > w) { ph *= w / pw; pw = w; }
    if (a > 0) n20TelefonePrevia(t, (GfxRect){x + (w - pw) * .5f, y, pw, ph}, a);
    y += ph + 16;
    for (int s = 0; s < nEst(t); s++) y += n20TelefoneControle(C->st[s], x, y, w, est[t] == s, n20TelefoneEstado, s, t, a);
    y += 12;
    for (int i = 0; i < 3; i++) {
      const Linha *l = &C->l[i]; int tem = temAqui(l->so);
      y += n20TelefoneRico(l->t, x, y, w, (tem ? .9f : .42f) * a) + 12;
      const char *tag = tem ? l->tag : "Não neste aparelho";
      if (tag) y += telefonecartao_texto(TXT_AJ_SEG, tag, x, y, w, 30, .6f * a) + 12;
      y += 12;
    }
    y += telefonecartao_texto(TXT_AJ_SEG, "Também", x, y, w, 30, .6f * a) + 16;
    for (int i = 0; i < 6 && C->e[i].t; i++) {
      const Linha *l = &C->e[i]; int tem = temAqui(l->so);
      y += n20TelefoneRico(l->t, x, y, w, (tem ? .85f : .4f) * a) + 8;
      const char *tag = tem ? l->tag : "Não neste aparelho";
      if (tag) y += telefonecartao_texto(TXT_AJ_SEG, tag, x, y, w, 30, .6f * a) + 8;
      y += 16;
    }
    y += n20TelefoneControle("pular para o resumo", x, y, w, 0, ptAcao, 4, 0, a);
    y += n20TelefoneControle("Ver depois", x, y, w, 0, ptLater, 0, 0, a);
  } else if (t == N20_RESUMO) {
    static const char *const B[4] = { "Plugins", "Servidor P2P", "Opacidade do vidro / Vidro fosco", "Receber enquetes" };
    static const char *const S[4] = { "Desligado, experimental", "Desligado, experimental", "Opções de teste", "Ligado, dá pra desligar" };
    static const char *const AP[6] = { "Plugins", "P2P", "Opacidade do vidro e Vidro fosco", "Zoom do trailer (Tizen .tpk)", "AutoSync pelo áudio", "Cache de busca e volume até 200%" };
    static const int SO[6] = { ALS, ALS, 0, DS, DA, DA };
    y += telefonecartao_titulo("O que muda pra você", x, y, w, a);
    if (essencial) y += telefonecartao_texto(TXT_V2_26, "Só o essencial.", x, y, w, 36, .6f * a) + 8;
    y += telefonecartao_texto(TXT_V2_26, "Tudo isso já vem na 2.0. O que for experimental fica desligado até você ligar.", x, y, w, 36, .6f * a) + 24;
    y += telefonecartao_texto(TXT_AJ_SEG, "Você vai notar logo", x, y, w, 30, .7f * a) + 16;
    for (int c = 0; c < N20_NCAP; c++) if (!CAP[c].exp) y += telefonecartao_item("aj_check", CAP[c].nome, CAP[c].sum, x, y, w, a);
    y += telefonecartao_texto(TXT_AJ_SEG, "Vem desligado ou em teste", x, y, w, 30, .7f * a) + 16;
    for (int i = 0; i < 4; i++) y += telefonecartao_item(NULL, B[i], S[i], x, y, w, a);
    char aparelho[96]; snprintf(aparelho, sizeof aparelho, i18n("No seu aparelho · %s"), devNome());
    y += telefonecartao_texto(TXT_AJ_SEG, aparelho, x, y, w, 30, .7f * a) + 16;
    for (int i = 0; i < 6; i++) {
      int tem = temAqui(SO[i]);
      y += telefonecartao_item(NULL, AP[i], tem ? "Disponível" : "Não neste aparelho", x, y, w, (tem ? 1 : .5f) * a);
    }
    y += n20TelefoneControle("rever do início", x, y, w, 0, ptAcao, 4, 0, a);
  } else {
    y += telefonecartao_texto(TXT_AJ_SEG, "Fim do guia", x, y, w, 30, .7f * a) + 16;
    y += telefonecartao_titulo("Dúvida depois? O Guia de uso fica à mão.", x, y, w, a);
    char trilha[256];
    snprintf(trilha, sizeof trilha, "%s › %s › %s", i18n("Ajustes"), i18n("Sobre e ajuda"), i18n("Guia de uso"));
    y += telefonecartao_texto(TXT_V2_24, trilha, x, y, w, 33, .85f * a) + 24;
    y += n20TelefoneRico("O Guia de uso explica o que cada coisa faz, com busca. Dá pra abrir direto no recurso.", x, y, w, .9f * a) + 24;
    y += n20TelefoneRico("Quer rever este tour? Está em <b>Sobre e ajuda › Novidades 2.0</b>.", x, y, w, .9f * a) + 24;
    y += n20TelefoneControle("anterior", x, y, w, 0, ptAcao, 0, 0, a);
  }
  y += 24 + n20TelefoneCapitulos(x, y + 24, w, a);
  return y - inicio + 24;
}
static void n20DesenharTelefone(void) {
  int t = tipo(), n = saindo || t == N20_HERO ? 3 : t == N20_FIM ? nBotoesFim() : 3;
  float a = anim_suave(entrada);
  telefonecartao_medir(&n20Telefone, NV_TELA_W / gfx_escala(), NV_TELA_H / gfx_escala(), n);
  float total = n20TelefoneConteudo(0, 0, n20Telefone.corpo.w, 0);
  telefonecartao_comecar(&n20Telefone, total, idx + (saindo ? 100 : 0), aberto, n20TelefoneRolar, a);
  n20TelefoneConteudo(n20Telefone.corpo.x, n20Telefone.corpo.y - n20Telefone.offset, n20Telefone.corpo.w, a);
  gfx_sem_recorte();
  if (saindo) {
    static const char *const R[3] = { "Continuar", "Ver depois", "Sair mesmo assim" };
    for (int i = 0; i < 3; i++) telefonecartao_botao(&n20Telefone, i, R[i], focoSair == i, ptSairFoco, ptSairOk, 0, a);
  } else if (t == N20_HERO) {
    static const char *const R[3] = { "Começar o guia", "Só o que muda pra mim", "Ver depois" };
    for (int i = 0; i < 3; i++) telefonecartao_botao(&n20Telefone, i, R[i], focoHero == i, ptHeroFoco, ptHeroOk, 0, a);
  } else if (t == N20_FIM) {
    static const char *const R[3] = { "Abrir o Guia de uso", "Concluir", "Rever do começo" };
    for (int i = 0; i < n; i++) telefonecartao_botao(&n20Telefone, i, R[botaoFim(i)], focoFim == i, ptFimFoco, ptFimOk, 0, a);
  } else {
    telefonecartao_botao(&n20Telefone, 0, "anterior", 0, NULL, n20TelefoneAcao, 0, a);
    telefonecartao_botao(&n20Telefone, 1, t == N20_RESUMO ? "continuar" : "próximo", 1, NULL, n20TelefoneAcao, 1, a);
    telefonecartao_botao(&n20Telefone, 2, "sair", 0, NULL, n20TelefoneAcao, 3, a);
  }
}
#endif

void novidades20_desenhar(Uint32 agora) {
  float a = anim_suave(entrada), dy = (1.0f - anim_suave(troca)) * 18.0f;
  int t;
  (void)agora;
  if (entrada < 0.002f) return;
#ifdef NV_TOUCH_UI
  if (telefoneui_ativo()) { ESCALA_INI(); n20DesenharTelefone(); ESCALA_FIM(); return; }
#endif
  if (aberto) ponteiro_camada();
  t = tipo();
  if (t == N20_HERO) telaHero(a, 0);
  else if (t == N20_RESUMO) telaResumo(a, dy);
  else if (t == N20_FIM) telaFim(a, dy);
  else telaCap(t, a, dy * 1.6f);
  if (aberto) dialogo();
}
