// Explorar 2.0: climas na entrada, a "toca do coelho" por titulo.
//
// TRES NIVEIS, um por vez na tela:
//   CLIMAS     o portal (grade C do mockup): 11 climas editoriais, na ordem de
//              afinidade com o que a pessoa viu. OK abre o clima.
//   CLIMA      os titulos de um clima em duas fileiras: o que ela ainda nao viu
//              e o que ja viu. OK num cartaz abre a vizinhanca dele.
//   VIZINHANCA (variacao B): o titulo em foco a esquerda e ate quatro grupos de
//              elo a direita — pessoa, tema, recomendados, amigos —, cada cartaz
//              com o seu "porque". OK num cartaz DESCE: ele vira o foco, e o fio
//              pelo qual ela desceu (a mesma pessoa, o mesmo tema) continua
//              sendo o primeiro grupo. A trilha no alto mostra o caminho; Voltar
//              sobe um degrau. O botao "Abrir título" leva a pagina do titulo.
//
// ENTRADA PELO DETALHE: o circular "Explorar" da pagina do titulo chama
// explorar_abrir_titulo e a toca comeca nele; Voltar no primeiro degrau devolve
// a pagina do titulo (com a grade de climas por baixo).
//
// OS DADOS sao de mapa.c: o retrato local sai na hora (catalogo, sem rede), e o
// TMDB, quando ha chave, completa num fio proprio. Esta tela so copia o retrato
// quando a revisao muda; nada aqui faz rede nem disco.
//
// REMOTO: so D-pad. Esquerda na borda esquerda devolve a barra lateral, como
// nas outras telas; Voltar sobe um nivel (e, no topo, sai).
#include "menu.h"
#include "explorar.h"
#include "mapa.h"
#include "posterprov.h"
#include "catalogo.h"
#include "descoberta.h"
#include "tex_cache.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "ajustes.h"
#include "ponteiro.h"
#include "rolagemtoque.h"
#include "telefoneui.h"
#include "idioma.h"
#include "idiomacod.h"
#include "layout.h"
#include "focoprof.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef NV_TOUCH_UI
static float explorarDireita(void) {
  float margem = NV_TELA_H > NV_TELA_W ? ajustes_conteudo_x() : 80.0f;
  return NV_TELA_W - margem;
}
#define EX_DIR          explorarDireita()
#else
#define EX_DIR          1840.0f     // borda direita do conteudo
#endif
#define EX_TOPO         232.0f
#define EX_DICA_Y       1012.0f
#define EX_TEX_LARG     220.0f
#define EX_TRILHA_MAX   12

// Grade de climas.
#define EX_CL_COLS      4
#define EX_CL_H         250.0f
#define EX_CL_GAP       18.0f
#define EX_CL_TOPO      212.0f

// Clima aberto: duas fileiras de cartazes.
#define EX_CA_W         200.0f
#define EX_CA_H         300.0f
#define EX_CB_W         140.0f
#define EX_CB_H         210.0f
#define EX_CAR_GAP      26.0f

// Vizinhanca.
#define EX_HERO_W       360.0f
#define EX_VZ_PW        84.0f
#define EX_VZ_PH        126.0f
#define EX_VZ_CARD      246.0f
#define EX_VZ_GAP       12.0f
#define EX_VZ_LINHA     196.0f

enum { MODO_CLIMAS = 0, MODO_CLIMA, MODO_VIZ };
enum { ORIGEM_NADA = 0, ORIGEM_CLIMA, ORIGEM_DETALHE };

typedef struct {
  MapaObra obra;
  long pessoa;            // o fio pelo qual ela chegou aqui (mapa_vizinhos_pedir)
  char tema[48];
  char via[64];           // o "porque" do passo, para a trilha
  int linha, coluna;      // onde estava o foco QUANDO desceu daqui
#ifdef NV_TOUCH_UI
  float pagina;
#endif
} ExPasso;

static int modo;
static int sair, pediuAbrir, pediuIndice, abrindo, pediuRelogio;
static float tempo, entrada, focoT;

static MapaClimas climas;
static unsigned climasRev;
static int clFoco;                    // posicao na grade
static int clAberto = -1;             // posicao do clima aberto (climas.c[])
static int caLinha, caCol[2];         // clima aberto: 0 = descobrir, 1 = vistos
static int caIdx[2][MAPA_CLIMA_ITENS], caN[2];
static float caRolar[2];
#ifdef NV_TOUCH_UI
static ToqueRolagem toqueCl;
static float clRolar;
static int climaColunas(void);
static void climasRetomarFoco(void);
static ToqueRolagem toqueCa[2];
static int toqueCaAtiva = -1;
static ToqueRolagem toqueVz[MAPA_VIZ_GRUPOS];
static float toqueVzOffset[MAPA_VIZ_GRUPOS];
static int toqueVzAtiva = -1, toqueVzUltima = -1;
static ToqueRolagem toquePagina;
static float exPagina, caPagina;
static float exPaginaDesejada;
static int exPaginaRestaurar;
static int toquePaginaAtiva, exNavegar;
static float telefoneCardPasso(void);
static float telefoneClimaPasso(void);
static void telefoneRetomarFoco(void);
static void desenharClimaTelefone(void);
static void desenharVizTelefone(void);
static int toqueExplorarRolar(const PonteiroRolagem *e) {
  if (e->fase == PONT_ROL_INICIO) {
    toqueCaAtiva = toqueVzAtiva = -1;
    toquePaginaAtiva = 0;
    if(modo!=MODO_CLIMAS && telefoneui_ativo() && e->eixoY &&
       e->x>=toquePagina.regiao.x*toquePagina.escala &&
       e->x<(toquePagina.regiao.x+toquePagina.regiao.w)*toquePagina.escala &&
       e->y>=toquePagina.regiao.y*toquePagina.escala &&
       e->y<(toquePagina.regiao.y+toquePagina.regiao.h)*toquePagina.escala)exPaginaRestaurar=0;
    if (modo != MODO_CLIMAS && telefoneui_ativo() && toquerol_evento(&toquePagina, e)) {
      toquePaginaAtiva = 1; return 1;
    }
    if (modo == MODO_CLIMAS) {
      return toquerol_evento(&toqueCl,e);
    } else if (modo == MODO_CLIMA) {
      for (int i = 0; i < 2; i++) if (toquerol_evento(&toqueCa[i], e)) { toqueCaAtiva = i; exPaginaRestaurar=0; return 1; }
    } else if (modo == MODO_VIZ) {
      for (int i = 0; i < MAPA_VIZ_GRUPOS; i++) if (toquerol_evento(&toqueVz[i], e)) {
        toqueVzAtiva = toqueVzUltima = i; exPaginaRestaurar=0; return 1;
      }
    }
    return 0;
  }
  int r = toquePaginaAtiva ? toquerol_evento(&toquePagina,e)
        : modo == MODO_CLIMAS ? toquerol_evento(&toqueCl,e)
        : toqueCaAtiva >= 0 ? toquerol_evento(&toqueCa[toqueCaAtiva], e)
        : toqueVzAtiva >= 0 ? toquerol_evento(&toqueVz[toqueVzAtiva], e) : 0;
  if (e->fase == PONT_ROL_FIM || e->fase == PONT_ROL_CANCELAR) {
    toqueCaAtiva = toqueVzAtiva = -1; toquePaginaAtiva = 0;
  }
  return r;
}
static void explorarAlvoFileira(float x, float y, float w, float h, float x0, float x1,
                               PonteiroFn focar, int linha, int col) {
  if (!layout_modo_mobile()) { ponteiro_alvo(x, y, w, h, focar, NULL, linha, col); return; }
  float fim = fminf(x + w, x1);
  x = fmaxf(x, x0);
  if (fim > x) ponteiro_alvo(x, y, fim - x, h, focar, NULL, linha, col);
}
#endif

static MapaVizinhos viz;
static unsigned vizRev;
static ExPasso trilha[EX_TRILHA_MAX];
static int nTrilha;
static int origem;
static int vzLinha = -1, vzCol;       // -1 = botao "Abrir titulo"

// --- utilidades -----------------------------------------------------------------

static float suave(float t) {
  t = anim_clamp(t, 0.0f, 1.0f);
  t = 1.0f - t;
  return 1.0f - t * t * t * t;
}

static float vzX(void) { return ajustes_conteudo_x() + EX_HERO_W + 60.0f; }

static void tintaAcento(int *r, int *g, int *b) {
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  *r = (int)((ar * 0.55f + 0.45f) * 255.0f);
  *g = (int)((ag * 0.55f + 0.45f) * 255.0f);
  *b = (int)((ab * 0.55f + 0.45f) * 255.0f);
}

static void rotuloEspacado(const char *s, float x, float y, float a) {
  int tr, tg, tb;
  tintaAcento(&tr, &tg, &tb);
  txt_tracking(TXT_CAPTION2, s, tr, tg, tb, x, y, a, 2.4f);
}

static void painel(GfxRect r, float raio, float a) {
  if (ajustes_vidro()) gfx_vidro_painel(r, raio, 0.55f, a);
  else gfx_cor(r, raio, 0.14f, 0.15f, 0.17f, a);
}

// Painel com foco: no vidro o miolo clareia por cima; sem vidro o anel e um
// retangulo cheio que vai por baixo do painel (opaco).
static void painelComFoco(GfxRect r, float raio, float f, float a) {
  if (!ajustes_vidro() && f > 0.01f) foco_anel(r, raio, f, a);
  painel(r, raio, a);
  if (ajustes_vidro() && f > 0.01f) gfx_vidro_foco(r, raio, f, a);
}

static GfxRect crescer(GfxRect r, float s) {
  return (GfxRect){ r.x - r.w * s * 0.5f, r.y - r.h * s * 0.5f, r.w * (1.0f + s), r.h * (1.0f + s) };
}

// Cartaz com esqueleto enquanto a arte nao chega; `f` e o foco (0..1).
static void cartaz(const MapaObra *obra, GfxRect r, float f, float a) {
  const char *url = posterprov_card(obra->imdb, obra->tmdb, obra->tipo, obra->poster);
  float largura=EX_TEX_LARG;
#ifdef NV_TOUCH_UI
  if(telefoneui_ativo())largura=fmaxf(largura,r.w*gfx_escala());
#endif
  GLuint t = url && url[0] ? tex_obter_larg(url, largura) : 0;
  float raio = 10.0f / r.w;
  if (f > 0.01f)
    gfx_rect((GfxRect){ r.x - 40.0f, r.y - 20.0f, r.w + 80.0f, r.h + 80.0f }, 0,
             GFX_SOMBRA, 1.0f, 0, 0, 0.5f, 0, 0, 0, 0.5f * f * a);
  // O anel vai POR BAIXO: sem vidro, foco_anel e um retangulo cheio na cor do
  // realce um pouco maior que o cartaz, e o cartaz cobre o miolo.
  foco_anel(r, raio, f, a);
  if (t) {
    gfx_tex_aspect_atual = tex_aspecto(url);
    gfx_rect(r, t, GFX_CARD, f, 0, 0, raio, 1, 1, 1, a);
    gfx_tex_aspect_atual = 0.0f;
  } else {
    gfx_cor(r, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, a);
  }
}

// Selo "visto" no canto do cartaz: palavra e nao so cor.
static void seloVisto(GfxRect r, float a) {
  TxtLinha l = txt_linha(TXT_CAPTION2, i18n("Visto"), 20, 20, 24, 255);
  GfxRect p = { r.x + 6.0f, r.y + 6.0f, l.w + 18.0f, l.h + 6.0f };
  gfx_cor(p, 0.5f, 0.92f, 0.93f, 0.95f, 0.92f * a);
  txt_desenhar_alpha(l, p.x + 9.0f, p.y + 3.0f, a);
}

static void metaObra(const MapaObra *o, char *dst, size_t n) {
  char nota[24] = "";
  const char *tipo = i18n(!strcmp(o->tipo, "series") ? "Série" : "Filme");
  if (o->nota > 0) {
    snprintf(nota, sizeof nota, "   ·   TMDB %d.%d", o->nota / 10, o->nota % 10);
    idioma_decimal_texto(nota, ajustes_idioma());
  }
  if (o->ano) snprintf(dst, n, "%s   ·   %d%s", tipo, o->ano, nota);
  else snprintf(dst, n, "%s%s", tipo, nota);
}

static int indicePorImdb(const char *imdb) {
  int i, n = cat_n();
  if (!imdb || !imdb[0]) return -1;
  for (i = 0; i < n; i++) {
    const CatItem *ci = cat_item(i);
    if (ci && !strcmp(ci->imdb, imdb)) return i;
  }
  return -1;
}

// A pagina do titulo: do catalogo quando ele esta la; senao a descoberta busca
// o meta num fio e o roteador (app.c) abre quando chegar.
static void abrirPagina(const MapaObra *o) {
  int idx;
  if (!o || !o->titulo[0]) return;
  idx = indicePorImdb(o->imdb);
  if (idx < 0 && o->catIndice >= 0) {
    const CatItem *ci = cat_item(o->catIndice);
    if (ci && !strcmp(ci->titulo, o->titulo)) idx = o->catIndice;
  }
  if (idx >= 0) { pediuIndice = idx; pediuAbrir = 1; return; }
  if (desc_titulo_buscando()) return;
  if (o->tmdb > 0) desc_pedir_titulo_tmdb(o->tmdb, !strcmp(o->tipo, "series") ? "tv" : "movie");
  else if (o->imdb[0]) desc_pedir_titulo(o->imdb);
  else return;
  abrindo = 1;
}

static void trocarModo(int m) {
#ifdef NV_TOUCH_UI
  exPagina = m == MODO_CLIMA ? caPagina : 0;
  exPaginaDesejada=exPagina;exPaginaRestaurar=m==MODO_CLIMA && caPagina>0;
  toquePagina = (ToqueRolagem){0}; toquePaginaAtiva = exNavegar = 0;
#endif
  modo = m;
  entrada = 0.0f;
  focoT = 0.0f;
  abrindo = 0;
}

// --- climas ---------------------------------------------------------------------

static void climaSeparar(void) {
  const MapaClima *c;
  int i;
  caN[0] = caN[1] = 0;
  if (clAberto < 0 || clAberto >= climas.n) return;
  c = &climas.c[clAberto];
  for (i = 0; i < c->n; i++) {
    int k = c->visto[i] ? 1 : 0;
    caIdx[k][caN[k]++] = i;
  }
  for (i = 0; i < 2; i++) if (caCol[i] >= caN[i]) caCol[i] = caN[i] > 0 ? caN[i] - 1 : 0;
  if (caN[caLinha] == 0 && caN[!caLinha] > 0) caLinha = !caLinha;
}

static void abrirClima(int pos) {
  if (pos < 0 || pos >= climas.n) return;
  clAberto = pos;
  caLinha = 0;
  caCol[0] = caCol[1] = 0;
  caRolar[0] = caRolar[1] = 0.0f;
#ifdef NV_TOUCH_UI
  caPagina = 0;
  for (int i = 0; i < 2; i++) toquerol_limpar(&toqueCa[i]);
#endif
  climaSeparar();
  mapa_clima_abrir(climas.c[pos].id);
  trocarModo(MODO_CLIMA);
}

static const MapaObra *climaObraFocada(void) {
  if (clAberto < 0 || clAberto >= climas.n || caN[caLinha] <= 0) return NULL;
  return &climas.c[clAberto].itens[caIdx[caLinha][caCol[caLinha]]];
}

// --- vizinhanca -----------------------------------------------------------------

static void pedirPasso(const ExPasso *p) {
#ifdef NV_TOUCH_UI
  exPagina = 0; toquePagina = (ToqueRolagem){0}; toquePaginaAtiva = exNavegar = 0;
  exPaginaRestaurar=0;
  for (int i = 0; i < MAPA_VIZ_GRUPOS; i++) {
    toquerol_limpar(&toqueVz[i]); toqueVz[i].offset = NULL; toqueVzOffset[i] = 0.0f;
  }
  toqueVzAtiva = toqueVzUltima = -1;
#endif
  mapa_vizinhos_pedir(&p->obra, p->pessoa, p->tema);
}

// Os grupos que existem neste retrato, na ordem da tela.
static int gruposVisiveis(int *g) {
  int i, n = 0;
  for (i = 0; i < MAPA_VIZ_GRUPOS; i++) if (viz.g[i].n > 0) g[n++] = i;
  return n;
}

// O que ja esta na trilha nao volta como vizinho: descer de A para B e ver A
// de novo em B so faria a toca andar em circulo (Voltar e quem sobe).
static void vizFiltrarTrilha(void) {
  int g, i, k, n;
  for (g = 0; g < MAPA_VIZ_GRUPOS; g++) {
    MapaVizGrupo *gr = &viz.g[g];
    for (i = n = 0; i < gr->n; i++) {
      int naTrilha = 0;
      for (k = 0; k < nTrilha - 1 && !naTrilha; k++)
        if (!strcmp(trilha[k].obra.titulo, gr->itens[i].obra.titulo)) naTrilha = 1;
      if (!naTrilha) gr->itens[n++] = gr->itens[i];
    }
    gr->n = n;
  }
}

static void vizAjustarFoco(void) {
  int g[MAPA_VIZ_GRUPOS], n = gruposVisiveis(g);
  if (vzLinha >= n) vzLinha = n - 1;
  if (vzLinha >= 0 && vzCol >= viz.g[g[vzLinha]].n) vzCol = viz.g[g[vzLinha]].n - 1;
  if (vzCol < 0) vzCol = 0;
}
#ifdef NV_TOUCH_UI
static float vizRolarFileira(int grupo, int linha, float largura) {
  float max = fmaxf(0.0f, viz.g[grupo].n * (EX_VZ_CARD + EX_VZ_GAP) - EX_VZ_GAP - largura);
  float *offset = &toqueVzOffset[grupo];
  if (!toquerol_livre(&toqueVz[grupo]) && vzLinha == linha) {
    float x = vzCol * (EX_VZ_CARD + EX_VZ_GAP);
    if (x < *offset) *offset = x;
    if (x + EX_VZ_CARD > *offset + largura) *offset = x + EX_VZ_CARD - largura;
  }
  *offset = anim_clamp(*offset, 0.0f, max);
  return max;
}
#endif

static void comecarToca(const MapaObra *o, int org) {
#ifdef NV_TOUCH_UI
  if (modo == MODO_CLIMA) caPagina = exPagina;
#endif
  memset(trilha, 0, sizeof trilha);
  trilha[0].obra = *o;
  nTrilha = 1;
  origem = org;
  vizRev = 0;           // copia mesmo quando o titulo ja era o publicado
  pedirPasso(&trilha[0]);
  if (mapa_vizinhos_copiar(&viz, &vizRev)) vizFiltrarTrilha();
  vzLinha = 0; vzCol = 0;
  vizAjustarFoco();
  trocarModo(MODO_VIZ);
}

// O "porque" de um cartaz da vizinhanca, numa frase curta.
static void porque(const MapaVizGrupo *g, const MapaVizItem *it, char *dst, size_t n) {
  dst[0] = 0;
  switch (g->tipo) {
    case MAPA_GR_PESSOA: snprintf(dst, n, i18n("Também com %s"), it->motivo); break;
    case MAPA_GR_EPOCA:
      if (it->obra.ano) snprintf(dst, n, i18n("Também de %d"), it->obra.ano);
      else snprintf(dst, n, i18n("Também dos anos %s"), it->motivo);
      break;
    case MAPA_GR_TEMA:   snprintf(dst, n, i18n("Também fala de %s."), i18n(it->motivo)); break;
    case MAPA_GR_GENERO: snprintf(dst, n, i18n("No mesmo gênero: %s"), i18n(it->motivo)); break;
    case MAPA_GR_REC:
      snprintf(dst, n, "%s", i18n(viz.remoto ? "Recomendado pelo TMDB" : "Gênero e época parecidos"));
      break;
    case MAPA_GR_AMIGOS: snprintf(dst, n, i18n("%s gostou"), it->motivo); break;
  }
}

static void cabecalhoGrupo(const MapaVizGrupo *g, char *dst, size_t n) {
  switch (g->tipo) {
    case MAPA_GR_PESSOA: snprintf(dst, n, i18n("O fio de %s"), g->sub); break;
    case MAPA_GR_EPOCA:  snprintf(dst, n, i18n("Da mesma década: anos %s"), g->sub); break;
    case MAPA_GR_TEMA:   snprintf(dst, n, i18n("Também sobre %s"), i18n(g->sub)); break;
    case MAPA_GR_GENERO: snprintf(dst, n, i18n("No mesmo gênero: %s"), i18n(g->sub)); break;
    case MAPA_GR_REC:
      snprintf(dst, n, "%s", i18n(viz.remoto ? "Quem viu este também viu" : "Parecidos no seu catálogo"));
      break;
    case MAPA_GR_AMIGOS: snprintf(dst, n, "%s", i18n("Seus amigos gostaram")); break;
    default: dst[0] = 0;
  }
}

// O fio que continua quando ela desce por um grupo.
static void descer(const MapaVizGrupo *g, const MapaVizItem *it) {
  ExPasso *p;
  // Cheia: o degrau mais antigo sai. Voltar ate o fim ainda devolve a origem.
  if (nTrilha >= EX_TRILHA_MAX) {
    memmove(&trilha[0], &trilha[1], sizeof trilha[0] * (EX_TRILHA_MAX - 1));
    nTrilha--;
  }
  trilha[nTrilha - 1].linha = vzLinha;
  trilha[nTrilha - 1].coluna = vzCol;
#ifdef NV_TOUCH_UI
  trilha[nTrilha - 1].pagina = exPagina;
#endif
  p = &trilha[nTrilha++];
  memset(p, 0, sizeof *p);
  p->obra = it->obra;
  if (g->tipo == MAPA_GR_PESSOA) p->pessoa = g->ref;
  else if (g->tipo == MAPA_GR_TEMA) snprintf(p->tema, sizeof p->tema, "%s", g->kw);
  else if (g->tipo == MAPA_GR_GENERO) snprintf(p->tema, sizeof p->tema, "%s", g->sub);
  switch (g->tipo) {
    case MAPA_GR_PESSOA: case MAPA_GR_AMIGOS: snprintf(p->via, sizeof p->via, "%s", it->motivo); break;
    case MAPA_GR_EPOCA:  snprintf(p->via, sizeof p->via, i18n("anos %s"), g->sub); break;
    case MAPA_GR_TEMA: case MAPA_GR_GENERO: snprintf(p->via, sizeof p->via, "%s", i18n(g->sub)); break;
    default:             snprintf(p->via, sizeof p->via, "%s", i18n("recomendado")); break;
  }
  pedirPasso(p);
  vzLinha = 0; vzCol = 0;
  entrada = 0.0f;
  focoT = 0.0f;
  abrindo = 0;
}

static void subir(void) {
  if (nTrilha > 1) {
    nTrilha--;
    pedirPasso(&trilha[nTrilha - 1]);
#ifdef NV_TOUCH_UI
    exPagina = trilha[nTrilha - 1].pagina; toquePagina.livre = 1;
    exPaginaDesejada=exPagina;exPaginaRestaurar=1;
#endif
    // O foco volta para onde estava; vizAjustarFoco corrige quando o retrato
    // chegar (de memoria, no proximo quadro).
    vzLinha = trilha[nTrilha - 1].linha;
    vzCol = trilha[nTrilha - 1].coluna;
    entrada = 0.0f;
    abrindo = 0;
    return;
  }
  // O primeiro degrau: de volta a origem.
  nTrilha = 0;
  if (origem == ORIGEM_CLIMA && clAberto >= 0) { trocarModo(MODO_CLIMA); return; }
  if (origem == ORIGEM_DETALHE && trilha[0].obra.catIndice >= 0) {
    pediuIndice = trilha[0].obra.catIndice;
    pediuAbrir = 1;
  }
  trocarModo(MODO_CLIMAS);
}

// --- eventos --------------------------------------------------------------------

static int ehOk(SDL_Keycode k) { return k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE; }

static void eventoClimas(SDL_Keycode k) {
  int cols = EX_CL_COLS;
#ifdef NV_TOUCH_UI
  cols = climaColunas();
#endif
  int col = clFoco % cols;
  if (climas.n <= 0) { if (k == SDLK_LEFT) sair = 1; else if (k == SDLK_UP) pediuRelogio = 1; return; }
  if (k == SDLK_LEFT)  { if (col == 0) { sair = 1; return; } clFoco--; }
  else if (k == SDLK_RIGHT) { if (col < cols - 1 && clFoco + 1 < climas.n) clFoco++; }
  else if (k == SDLK_UP)    { if (clFoco >= cols) clFoco -= cols; else if (ajustes_menu_topo()) sair = 1; else pediuRelogio = 1; }
  else if (k == SDLK_DOWN)  {
    if (clFoco + cols < climas.n) clFoco += cols;
    else if ((clFoco / cols) < (climas.n - 1) / cols) clFoco = climas.n - 1;
  }
  else if (ehOk(k)) { abrirClima(clFoco); return; }
  else return;
  focoT = 0.0f;
}

static void eventoClima(SDL_Keycode k) {
  int *c = &caCol[caLinha];
  if (k == SDLK_LEFT)  { if (*c == 0) { sair = 1; return; } (*c)--; }
  else if (k == SDLK_RIGHT) { if (*c + 1 < caN[caLinha]) (*c)++; }
  else if (k == SDLK_UP)    { if (caLinha == 1 && caN[0] > 0) caLinha = 0; else if (ajustes_menu_topo()) sair = 1; else pediuRelogio = 1; }
  else if (k == SDLK_DOWN)  { if (caLinha == 0 && caN[1] > 0) caLinha = 1; }
  else if (ehOk(k)) {
    const MapaObra *o = climaObraFocada();
    if (o) comecarToca(o, ORIGEM_CLIMA);
    return;
  }
  else return;
  focoT = 0.0f;
}

static void eventoViz(SDL_Keycode k) {
  int g[MAPA_VIZ_GRUPOS], n = gruposVisiveis(g);
  if (vzLinha < 0) {
    if (k == SDLK_UP) { if (ajustes_menu_topo()) sair = 1; else pediuRelogio = 1; return; }
    if (k == SDLK_LEFT) { sair = 1; return; }
    if (k == SDLK_RIGHT && n > 0) { vzLinha = 0; vzCol = 0; }
    else if (ehOk(k)) { abrirPagina(&viz.foco); return; }
    else return;
  } else {
    if (k == SDLK_LEFT)  { if (vzCol == 0) vzLinha = -1; else vzCol--; }
    else if (k == SDLK_RIGHT) { if (vzCol + 1 < viz.g[g[vzLinha]].n) vzCol++; }
    else if (k == SDLK_UP)    { if (vzLinha > 0) vzLinha--; else if (ajustes_menu_topo()) sair = 1; else pediuRelogio = 1; }
    else if (k == SDLK_DOWN)  { if (vzLinha + 1 < n) vzLinha++; }
    else if (ehOk(k)) {
      const MapaVizGrupo *gr = &viz.g[g[vzLinha]];
      if (vzCol < gr->n) descer(gr, &gr->itens[vzCol]);
      return;
    }
    else return;
    vizAjustarFoco();
  }
  focoT = 0.0f;
  abrindo = 0;
}

void explorar_evento(const SDL_Event *e) {
  pediuRelogio = 0;
  if (!e || e->type != SDL_KEYDOWN) return;
#ifdef NV_TOUCH_UI
  if (toquerol_navegacao(e)) {
    if (modo != MODO_CLIMAS && telefoneui_ativo()) {
      telefoneRetomarFoco(); toquerol_limpar(&toquePagina); exNavegar = 1;
      exPaginaRestaurar=0;
    }
    if (modo == MODO_CLIMAS) climasRetomarFoco();
    for (int i = 0; i < 2; i++) {
      if (modo == MODO_CLIMA && toquerol_livre(&toqueCa[i]) && caN[i] > 0) {
        float passo = telefoneui_ativo() ? telefoneClimaPasso()
                    : (i == 1 && caN[0] > 0 ? EX_CB_W : EX_CA_W) + EX_CAR_GAP;
        int c = (int)(caRolar[i] / passo);
        caCol[i] = c < caN[i] ? c : caN[i] - 1;
      }
      toquerol_limpar(&toqueCa[i]);
    }
    if (modo == MODO_VIZ && toqueVzUltima >= 0 && toquerol_livre(&toqueVz[toqueVzUltima])) {
      int g[MAPA_VIZ_GRUPOS], n = gruposVisiveis(g);
      for (int i = 0; i < n; i++) if (g[i] == toqueVzUltima) {
        vzLinha = i; vzCol = (int)(toqueVzOffset[g[i]] / (telefoneui_ativo() ? telefoneCardPasso() : EX_VZ_CARD + EX_VZ_GAP));
        vizAjustarFoco(); break;
      }
    }
    for (int i = 0; i < MAPA_VIZ_GRUPOS; i++) toquerol_limpar(&toqueVz[i]);
  }
#endif
  SDL_Keycode k;
  k = e->key.keysym.sym;
  if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE || k == SDLK_DELETE) {
    if (e->key.repeat) return;
    if (modo == MODO_VIZ) subir();
    else if (modo == MODO_CLIMA) { clFoco = clAberto >= 0 ? clAberto : 0; trocarModo(MODO_CLIMAS); }
    else sair = 1;
    return;
  }
  // OK segurado repete o KEYDOWN: so o primeiro toque vale (#187).
  if (ehOk(k) && e->key.repeat) return;
  if (modo == MODO_CLIMAS) eventoClimas(k);
  else if (modo == MODO_CLIMA) eventoClima(k);
  else eventoViz(k);
}

// PONTEIRO (#99). Mesmas variaveis das setas; o OK do clique segue o caminho
// de sempre (abrir clima, puxar o fio, descer pela toca). So zera a animacao
// do foco quando ele de fato troca.
static void ponteiroClimas(int i, int b) {
  (void)b;
  if (modo != MODO_CLIMAS || i < 0 || i >= climas.n || i == clFoco) return;
  clFoco = i; focoT = 0.0f;
}
static void ponteiroClima(int linha, int col) {
#ifdef NV_TOUCH_UI
  if (linha >= 0 && linha < 2) toquerol_limpar(&toqueCa[linha]);
#endif
  if (modo != MODO_CLIMA || linha < 0 || linha > 1 || col < 0 || col >= caN[linha]) return;
  if (linha == caLinha && caCol[linha] == col) return;
  caLinha = linha; caCol[linha] = col; focoT = 0.0f;
}
static void ponteiroViz(int linha, int col) {
#ifdef NV_TOUCH_UI
  int g[MAPA_VIZ_GRUPOS], n = gruposVisiveis(g);
  if (linha >= 0 && linha < n) toquerol_limpar(&toqueVz[g[linha]]);
#endif
  if (modo != MODO_VIZ || (linha == vzLinha && (linha < 0 || col == vzCol))) return;
  vzLinha = linha; vzCol = linha < 0 ? 0 : col;
  if (linha >= 0) vizAjustarFoco();
  focoT = 0.0f; abrindo = 0;
}

// --- desenho: comum ----------------------------------------------------------------

// O CEU ASSADO NOS EFEITOS LEVES (gpunivel.h, nivel >= 1). O GFX_CEU e um
// hash por pixel em tela cheia: MEDIDO na TCL Smart TV Pro (Mali-G52, GPU
// timer, 06/10/2026) a Explorar parada custava 27,6 ms por quadro (36 fps),
// ~20 ms deles o ceu. Com efeitos leves ele e pintado UMA vez no FBO do
// snapshot (gfx_snap_*) e cada quadro copia o assado: as estrelas param de
// cintilar e de derivar — e uma troca de aparencia, so nesse nivel. Refaz
// quando a cor de acento muda ou quando outro desenho usou o FBO
// (gfx_snap_geracao); dentro de um snapshot (painel de log) desenha direto.
static void desenharFundo(void) {
  float ar, ag, ab;
  static int ceuPronto; static unsigned ceuGer; static float ceuCor[3], ceuTempo;
  ajustes_acento(&ar, &ag, &ab);
  if (gfx_efeitos_leves() && gfx_snap_ok() && !gfx_snap_ativo()) {
    if (!ceuPronto || ceuGer != gfx_snap_geracao() ||
        ceuCor[0] != ar || ceuCor[1] != ag || ceuCor[2] != ab) {
      ceuTempo = tempo;
      gfx_snap_comecar();
      gfx_sem_recorte();
      gfx_rect((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0, GFX_CEU, ceuTempo,
               ceuTempo * 0.35f, 0, 0, ar, ag, ab, 1.0f);
      gfx_snap_terminar();
      ceuGer = gfx_snap_geracao(); ceuPronto = 1;
      ceuCor[0] = ar; ceuCor[1] = ag; ceuCor[2] = ab;
    }
    gfx_snap_desenhar();
    return;
  }
  ceuPronto = 0;
  gfx_rect((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0, GFX_CEU, tempo,
           tempo * 0.35f, 0, 0, ar, ag, ab, 1.0f);
}

static void desenharDica(const char *s) {
  TxtLinha l = txt_linha(TXT_CAPTION2, s, 140, 146, 162, 255);
  txt_desenhar(l, EX_DIR - l.w, EX_DICA_Y);
}

static void desenharTitulo(const char *sub) {
  float x = ajustes_conteudo_x();
  // Layout Dinamica: o nome da tela esta na pilula da barra (menu.h).
  if (!menu_pilula_titulo()) {
    TxtLinha t = txt_linha(TXT_TITULO2, i18n("Explorar"), 246, 246, 248, 255);
    txt_desenhar(t, x, 54.0f);
  }
  if (sub && sub[0]) {
    TxtLinha s = txt_linha_corta(TXT_BODY, sub, 180, 184, 198, 255, EX_DIR - x);
    txt_desenhar(s, x, 128.0f);
  }
}

// --- desenho: grade de climas ----------------------------------------------------

#ifdef NV_TOUCH_UI
static int climaColunas(void) {
  if (!telefoneui_ativo()) return EX_CL_COLS;
  int cols=NV_TELA_H > NV_TELA_W ? 2 : 3;
  float maior=0;
  for(int i=0;i<climas.n;i++) for(int descricao=0;descricao<2;descricao++) {
    const char *p=i18n(descricao ? mapa_clima_descricao(climas.c[i].id) : mapa_clima_nome(climas.c[i].id));
    while(*p) {
      if(*p==' ' || *p=='\n') { p++; continue; }
      char palavra[128]; size_t n=txt_token_tam(p);
      if(!n)break;
      size_t copia=n<sizeof palavra-1 ? n : sizeof palavra-1;
      memcpy(palavra,p,copia); palavra[copia]=0;
      maior=fmaxf(maior,txt_largura(descricao ? TXT_HEADLINE : TXT_TITULO3,palavra));
      p+=n;
    }
  }
  while(cols>1 && (EX_DIR-ajustes_conteudo_x()-EX_CL_GAP*(cols-1))/cols-48<maior)cols--;
  return cols;
}

static float climaBlocoTelefone(TxtEstilo estilo,const char *s,int r,int g,int b,
                               float x,float y,float w,float leading,float a) {
  // Keep a translated compound word complete even when one phone column is
  // too narrow. Insert hard breaks at UTF-8 boundaries; never remove letters.
  char texto[512];size_t usado=0;const char *p=i18n(s);
  while(*p && usado<sizeof texto-1) {
    if(*p==' ' || *p=='\n') {texto[usado++]=*p++;continue;}
    size_t n=txt_token_tam(p), ini=0;
    if(!n)break;
    char palavra[512];
    if(n<sizeof palavra && n<sizeof texto-usado) {
      memcpy(palavra,p,n);palavra[n]=0;
      if(txt_largura(estilo,palavra)<=w) {
        memcpy(texto+usado,p,n);usado+=n;p+=n;continue;
      }
    }
    while(ini<n && usado<sizeof texto-1) {
      char trecho[512];size_t fim=ini, cabe=ini;
      while(fim<n) {
        size_t proximo=fim+1;
        while(proximo<n && ((unsigned char)p[proximo]&0xc0)==0x80)proximo++;
        size_t tam=proximo-ini;
        if(tam>=sizeof trecho)break;
        memcpy(trecho,p+ini,tam);trecho[tam]=0;
        if(txt_largura(estilo,trecho)>w && cabe>ini)break;
        cabe=proximo;fim=proximo;
      }
      if(cabe<=ini)break;
      size_t tam=cabe-ini;
      if(tam>sizeof texto-1-usado)tam=sizeof texto-1-usado;
      memcpy(texto+usado,p+ini,tam);usado+=tam;ini=cabe;
      if(ini<n && usado<sizeof texto-1)texto[usado++]='\n';
    }
    p+=n;
  }
  texto[usado]=0;
  return txt_bloco(estilo,texto,r,g,b,x,y,w,leading,a,0);
}

static float climaConteudoTelefone(const MapaClima *c, float x, float y, float w, float a) {
  float inicio = y;
  char s[96];
  float tituloH=climaBlocoTelefone(TXT_TITULO3,mapa_clima_nome(c->id),246,246,248,x,y,w,58,a);
  y+=tituloH+12;
  float descricaoH=climaBlocoTelefone(TXT_HEADLINE,mapa_clima_descricao(c->id),186,190,202,x,y,w,48,a);
  y+=descricaoH+20;
  float contagemH=0, afinidadeH=0;
  if (c->n > 0) {
    if (a > 0) for (int k=0;k<c->n && k<3;k++)
      cartaz(&c->itens[k],(GfxRect){x+50*k,y,84,126},0,a);
    y += 142;
    snprintf(s,sizeof s,i18n(c->total==1 ? "%d título · você viu %d" : "%d títulos · você viu %d"),c->total,c->vistos);
    contagemH=climaBlocoTelefone(TXT_HEADLINE,s,196,200,212,x,y,w,48,a);y+=contagemH;
    if (c->afinidade > 0) {
      int tr,tg,tb; tintaAcento(&tr,&tg,&tb);
      snprintf(s,sizeof s,i18n("%d%% do que você viu"),c->afinidade);
      afinidadeH=climaBlocoTelefone(TXT_HEADLINE,s,tr,tg,tb,x,y+8,w,48,a);y+=8+afinidadeH;
    }
  } else {
    contagemH=climaBlocoTelefone(TXT_HEADLINE,"Nada deste clima no seu catálogo ainda",150,154,168,x,y,w,48,a);
    y+=contagemH;
  }
  return y-inicio;
}

static int climaGradeTelefone(GfxRect *cards) {
  int cols=climaColunas(), linhas=(climas.n+cols-1)/cols;
  float x=ajustes_conteudo_x(), w=(EX_DIR-x-EX_CL_GAP*(cols-1))/cols, y=EX_CL_TOPO;
  for(int l=0;l<linhas;l++) {
    float h=384;
    for(int c=0;c<cols && l*cols+c<climas.n;c++)
      h=fmaxf(h,48+climaConteudoTelefone(&climas.c[l*cols+c],0,0,w-48,0));
    for(int c=0;c<cols && l*cols+c<climas.n;c++)
      cards[l*cols+c]=(GfxRect){x+c*(w+EX_CL_GAP),y,w,h};
    y+=h+EX_CL_GAP;
  }
  return climas.n;
}

static void climasRetomarFoco(void) {
  if (toquerol_livre(&toqueCl) && telefoneui_ativo() && climas.n>0) {
    GfxRect cards[MAPA_CLIMA_N]; climaGradeTelefone(cards);
    int cols=climaColunas(), col=clFoco%cols, escolhido=clFoco;
    float melhor=1e9f, centro=(EX_CL_TOPO+NV_TELA_H-80)*.5f;
    for(int i=col;i<climas.n;i+=cols) {
      float d=fabsf(cards[i].y-clRolar+cards[i].h*.5f-centro);
      if(d<melhor) { melhor=d; escolhido=i; }
    }
    clFoco=escolhido;
  }
  toquerol_limpar(&toqueCl);
}

static void desenharClimasTelefone(void) {
  GfxRect cards[MAPA_CLIMA_N]; int n=climaGradeTelefone(cards);
  float x=ajustes_conteudo_x(), baixo=NV_TELA_H-80;
  desenharTitulo(i18n("Escolha um clima e puxe o fio de uma história"));
  if(n<=0 || cat_n()<=0) {
    txt_bloco(TXT_TITULO3,i18n("Os climas aparecem quando o catálogo carregar"),236,238,244,x,500,EX_DIR-x,58,1,0);
    return;
  }
  float max=fmaxf(0,cards[n-1].y+cards[n-1].h-baixo);
  if(!toquerol_livre(&toqueCl) && clFoco>=0 && clFoco<n) {
    if(cards[clFoco].y-clRolar<EX_CL_TOPO)clRolar=cards[clFoco].y-EX_CL_TOPO;
    if(cards[clFoco].y+cards[clFoco].h-clRolar>baixo)clRolar=cards[clFoco].y+cards[clFoco].h-baixo;
  }
  clRolar=anim_clamp(clRolar,0,max);
  toquerol_vincular(&toqueCl,(GfxRect){x-10,EX_CL_TOPO-10,EX_DIR-x+20,baixo-EX_CL_TOPO+10},gfx_escala(),0,max,1,&clRolar);
  ponteiro_rolagem(toqueExplorarRolar);
  gfx_recorte(x-10,EX_CL_TOPO-10,EX_DIR-x+20,baixo-EX_CL_TOPO+10);
  for(int i=0;i<n;i++) {
    float e=ajustes_animacoes_reduzidas() ? 1 : suave((entrada-.04f*i)/.45f);
    float f=i==clFoco ? focoT : 0;
    GfxRect r=cards[i]; r.y+=18*(1-e)-clRolar;
    if(e<=.01f || r.y+r.h<EX_CL_TOPO-10 || r.y>=baixo)continue;
    float y0=fmaxf(r.y,EX_CL_TOPO-10), y1=fminf(r.y+r.h,baixo);
    if(y1>y0)ponteiro_alvo(r.x,y0,r.w,y1-y0,ponteiroClimas,NULL,i,0);
    if(i==clFoco)r=crescer(r,.03f*f);
    painelComFoco(r,18/r.h,f,e);
    climaConteudoTelefone(&climas.c[i],r.x+24,r.y+24,r.w-48,e);
  }
  gfx_sem_recorte();
}
#endif

static void desenharClimas(void) {
#ifdef NV_TOUCH_UI
  if(telefoneui_ativo()) { desenharClimasTelefone(); return; }
#endif
  float x0 = ajustes_conteudo_x();
  float w = (EX_DIR - x0 - EX_CL_GAP * (EX_CL_COLS - 1)) / EX_CL_COLS;
  int i, tr, tg, tb;
  tintaAcento(&tr, &tg, &tb);
  desenharTitulo(i18n("Escolha um clima e puxe o fio de uma história"));
  if (climas.n <= 0 || cat_n() <= 0) {
    TxtLinha v = txt_linha(TXT_TITULO3, i18n("Os climas aparecem quando o catálogo carregar"), 236, 238, 244, 255);
    txt_desenhar(v, (NV_TELA_W - v.w) * 0.5f, 500.0f);
    return;
  }
  for (i = 0; i < climas.n; i++) {
    const MapaClima *c = &climas.c[i];
    int col = i % EX_CL_COLS, lin = i / EX_CL_COLS, k;
    float e = ajustes_animacoes_reduzidas() ? 1.0f : suave((entrada - 0.04f * (float)i) / 0.45f);
    float f = i == clFoco ? focoT : 0.0f, a = e;
    GfxRect r = { x0 + col * (w + EX_CL_GAP), EX_CL_TOPO + lin * (EX_CL_H + EX_CL_GAP) + 18.0f * (1.0f - e), w, EX_CL_H };
    float raio = 18.0f / EX_CL_H, ty;
    char s[96];
    if (a <= 0.01f) continue;
    ponteiro_alvo(r.x, r.y, r.w, r.h, ponteiroClimas, NULL, i, 0);
    if (i == clFoco) r = crescer(r, 0.03f * f);
    painelComFoco(r, raio, f, a);
    ty = r.y + 20.0f;
    ty += txt_bloco_corta(TXT_HEADLINE, i18n(mapa_clima_nome(c->id)), 246, 246, 248, r.x + 24.0f, ty,
                          r.w - 48.0f, 40.0f, a, 2) + 8.0f;
    { TxtLinha d = txt_linha_corta(TXT_CAPTION2, i18n(mapa_clima_descricao(c->id)), 186, 190, 202, 255, r.w - 48.0f);
      txt_desenhar_alpha(d, r.x + 24.0f, ty, a); }
    // Tres cartazes do clima, em leque, embaixo a esquerda; os numeros na
    // coluna que sobra a direita.
    for (k = 0; k < c->n && k < 3; k++) {
      GfxRect p = { r.x + 24.0f + 34.0f * (float)k, r.y + r.h - 20.0f - 78.0f, 52.0f, 78.0f };
      cartaz(&c->itens[k], p, 0.0f, a);
    }
    if (c->n == 0) {
      TxtLinha l = txt_linha_corta(TXT_CAPTION2, i18n("Nada deste clima no seu catálogo ainda"), 150, 154, 168, 255, r.w - 48.0f);
      txt_desenhar_alpha(l, r.x + 24.0f, r.y + r.h - 50.0f, a);
    } else {
      float rx = r.x + r.w - 24.0f, ry = r.y + r.h - 20.0f;
      float larg = r.w - 48.0f - (34.0f * (float)((c->n < 3 ? c->n : 3) - 1) + 52.0f) - 16.0f;
      TxtLinha l;
      snprintf(s, sizeof s, i18n(c->total == 1 ? "%d título · você viu %d" : "%d títulos · você viu %d"),
               c->total, c->vistos);
      l = txt_linha_corta(TXT_CAPTION2, s, 196, 200, 212, 255, larg);
      txt_desenhar_alpha(l, rx - l.w, ry - l.h, a);
      if (c->afinidade > 0) {
        snprintf(s, sizeof s, i18n("%d%% do que você viu"), c->afinidade);
        l = txt_linha_corta(TXT_CAPTION2, s, tr, tg, tb, 255, larg);
        txt_desenhar_alpha(l, rx - l.w, ry - 2.0f * l.h - 6.0f, a);
      }
    }
  }
  desenharDica(i18n("Setas   Escolher um clima   ·   OK   Abrir"));
}

// --- desenho: clima aberto -------------------------------------------------------

static void fileiraClima(int linha, float y, float w, float h, float a) {
  const MapaClima *c = &climas.c[clAberto];
  float x0 = ajustes_conteudo_x(), x;
  int i, focada = linha == caLinha;
  if (caN[linha] <= 0) return;
#ifdef NV_TOUCH_UI
  toquerol_vincular(&toqueCa[linha], (GfxRect){x0, y, EX_DIR - x0, h}, gfx_escala(),
                   0.0f, caN[linha] * (w + EX_CAR_GAP) - EX_CAR_GAP - (EX_DIR - x0), 0, &caRolar[linha]);
  ponteiro_rolagem(toqueExplorarRolar);
  if (layout_modo_mobile()) gfx_recorte(x0, 0.0f, EX_DIR - x0, NV_TELA_H);
#endif
  x = x0 - caRolar[linha];
  for (i = 0; i < caN[linha]; i++, x += w + EX_CAR_GAP) {
    const MapaObra *o = &c->itens[caIdx[linha][i]];
    int sel = focada && i == caCol[linha];
    float f = sel ? focoT : 0.0f;
    GfxRect r = { x, y, w, h };
    if (x + w < 0.0f || x > NV_TELA_W) continue;
#ifdef NV_TOUCH_UI
    explorarAlvoFileira(r.x, r.y, r.w, r.h, x0, EX_DIR, ponteiroClima, linha, i);
#else
    ponteiro_alvo(r.x, r.y, r.w, r.h, ponteiroClima, NULL, linha, i);
#endif
    if (sel) r = crescer(r, 0.06f * f);
    cartaz(o, r, f, a);
    if (sel) {
      TxtLinha t = txt_linha_corta(TXT_BODY, o->titulo, 240, 241, 245, 255, w + 120.0f);
      txt_desenhar_alpha(t, x, r.y + r.h + 12.0f, a);
    }
  }
#ifdef NV_TOUCH_UI
  if (layout_modo_mobile()) gfx_sem_recorte();
#endif
}

static void desenharClima(void) {
#ifdef NV_TOUCH_UI
  if (telefoneui_ativo()) { desenharClimaTelefone(); return; }
#endif
  const MapaClima *c;
  float x0 = ajustes_conteudo_x(), a = ajustes_animacoes_reduzidas() ? 1.0f : suave(entrada / 0.4f), caBase = 262.0f;
  char s[120];
  if (clAberto < 0 || clAberto >= climas.n) return;
  c = &climas.c[clAberto];
  desenharTitulo(NULL);
  rotuloEspacado(i18n("CLIMA"), x0, 140.0f, a);
#ifdef NV_TOUCH_UI
  if (telefoneui_ativo()) {
    float y = 168, w = EX_DIR - x0;
    y += txt_bloco_corta(TXT_TITULO3, i18n(mapa_clima_nome(c->id)), 246, 246, 248, x0, y, w, 56, a, 0) + 8;
    y += txt_bloco_corta(TXT_BODY, i18n(mapa_clima_descricao(c->id)), 186, 190, 202, x0, y, w, 36, a, 0) + 24;
    caBase = fmaxf(caBase, y);
  } else
#endif
  { TxtLinha t = txt_linha_corta(TXT_TITULO3, i18n(mapa_clima_nome(c->id)), 246, 246, 248, 255, 1100.0f);
    TxtLinha d;
    txt_desenhar_alpha(t, x0, 168.0f, a);
    d = txt_linha_corta(TXT_BODY, i18n(mapa_clima_descricao(c->id)), 186, 190, 202, 255, EX_DIR - x0 - t.w - 40.0f);
    txt_desenhar_alpha(d, x0 + t.w + 28.0f, 168.0f + (t.h - d.h) * 0.6f, a); }
  if (c->carregando) {
    float pulso = ajustes_animacoes_reduzidas() ? 1.0f : 0.55f + 0.45f * sinf(tempo * 3.0f);
    TxtLinha l = txt_linha(TXT_CAPTION2, i18n("Buscando mais títulos…"), 170, 176, 196, 255);
    txt_desenhar_alpha(l, EX_DIR - l.w, 150.0f, pulso * a);
  }
  if (c->n == 0) {
#ifdef NV_TOUCH_UI
    if (telefoneui_ativo()) {
      txt_bloco_corta(TXT_TITULO3, i18n("Nada deste clima no seu catálogo ainda"), 236, 238, 244,
                      x0, 520, EX_DIR - x0, 56, a, 0);
    } else
#endif
    {
    TxtLinha v = txt_linha(TXT_TITULO3, i18n("Nada deste clima no seu catálogo ainda"), 236, 238, 244, 255);
    txt_desenhar_alpha(v, (NV_TELA_W - v.w) * 0.5f, 520.0f, a);
    }
  }
  if (caN[0] > 0) {
    snprintf(s, sizeof s, "%s   ·   %d", i18n("PARA DESCOBRIR"), caN[0]);
    rotuloEspacado(s, x0, caBase, a);
    fileiraClima(0, caBase + 38.0f, EX_CA_W, EX_CA_H, a);
  }
  if (caN[1] > 0) {
    float y = caN[0] > 0 ? caBase + 428.0f : caBase;
    snprintf(s, sizeof s, "%s   ·   %d", i18n("VOCÊ JÁ VIU"), caN[1]);
    rotuloEspacado(s, x0, y, a);
    fileiraClima(1, y + 38.0f, caN[0] > 0 ? EX_CB_W : EX_CA_W, caN[0] > 0 ? EX_CB_H : EX_CA_H, a);
  }
  desenharDica(i18n("OK   Puxar o fio   ·   Voltar   Climas"));
}

// --- desenho: vizinhanca --------------------------------------------------------

// A trilha: o caminho da raiz ate o foco, com o "porque" de cada degrau entre
// eles. Sem espaco, os degraus mais antigos saem pela esquerda ("…").
static void desenharTrilha(float a) {
  float x0 = ajustes_conteudo_x(), larg = EX_DIR - x0, total = 0.0f, x = x0;
  float wPasso[EX_TRILHA_MAX], wVia[EX_TRILHA_MAX];
  int i, ini = 0, tr, tg, tb;
  char raiz[120] = "";
  tintaAcento(&tr, &tg, &tb);
  if (origem == ORIGEM_CLIMA && clAberto >= 0 && clAberto < climas.n)
    snprintf(raiz, sizeof raiz, "%s", i18n(mapa_clima_nome(climas.c[clAberto].id)));
  for (i = 0; i < nTrilha; i++) {
    float wt = (float)txt_largura(TXT_CAPTION, trilha[i].obra.titulo);
    float wv = i > 0 ? (float)txt_largura(TXT_CAPTION2, trilha[i].via) + 40.0f : 0.0f;
    wPasso[i] = (wt > 300.0f ? 300.0f : wt) + 42.0f;
    wVia[i] = wv > 260.0f ? 260.0f : wv;
    total += wPasso[i] + wVia[i];
  }
  if (raiz[0]) total += (float)txt_largura(TXT_CAPTION2, raiz) + 40.0f;
  while (ini < nTrilha - 1 && total > larg - 60.0f) { total -= wPasso[ini] + wVia[ini + 1]; ini++; }
  if (ini > 0 || raiz[0]) {
    char s[140];
    TxtLinha l;
    snprintf(s, sizeof s, "%s  ›", ini > 0 ? "…" : raiz);
    l = txt_linha(TXT_CAPTION2, s, 170, 176, 196, 255);
    txt_desenhar_alpha(l, x, 136.0f, a);
    x += l.w + 16.0f;
  }
  for (i = ini; i < nTrilha; i++) {
    int atual = i == nTrilha - 1, cor = atual ? 20 : 228;
    TxtLinha l;
    GfxRect p;
    if (i > ini) {
      // O elo do degrau: "Christopher Nolan →" na tinta do acento.
      char s[96];
      snprintf(s, sizeof s, "%s  →", trilha[i].via);
      l = txt_linha_corta(TXT_CAPTION2, s, tr, tg, tb, 255, wVia[i]);
      txt_desenhar_alpha(l, x, 136.0f, a);
      x += l.w + 14.0f;
    }
    l = txt_linha_corta(TXT_CAPTION, trilha[i].obra.titulo, cor, cor, atual ? 24 : 236, 255, 300.0f);
    p = (GfxRect){ x, 128.0f, l.w + 28.0f, l.h + 14.0f };
    if (atual) gfx_cor(p, 0.5f, 0.94f, 0.95f, 0.97f, 0.95f * a);
    else painel(p, 0.5f, a);
    txt_desenhar_alpha(l, x + 14.0f, 135.0f, a);
    x += p.w + 14.0f;
  }
}

static void desenharHero(float a) {
  float x = ajustes_conteudo_x(), y = EX_TOPO;
  const MapaObra *o = &viz.foco;
  char meta[120];
  float f = vzLinha < 0 ? focoT : 0.0f;
  GfxRect p = { x, y, 240.0f, 360.0f };
  cartaz(o, p, 0.0f, a);
  if (viz.focoVisto) seloVisto(p, a);
  y += p.h + 22.0f;
  y += txt_bloco_corta(TXT_TITULO3, o->titulo, 246, 246, 248, x, y, EX_HERO_W, 56.0f, a, 2) + 8.0f;
  metaObra(o, meta, sizeof meta);
  { TxtLinha l = txt_linha_corta(TXT_CAPTION, meta, 186, 190, 202, 255, EX_HERO_W);
    txt_desenhar_alpha(l, x, y, a);
    y += l.h + 8.0f; }
  if (viz.generos[0]) {
    TxtLinha l = txt_linha_corta(TXT_CAPTION2, viz.generos, 160, 166, 182, 255, EX_HERO_W);
    txt_desenhar_alpha(l, x, y, a);
    y += l.h + 8.0f;
  }
  if (viz.carregando) {
    float pulso = ajustes_animacoes_reduzidas() ? 1.0f : 0.55f + 0.45f * sinf(tempo * 3.0f);
    TxtLinha l = txt_linha(TXT_CAPTION2, i18n("Cruzando histórias…"), 170, 176, 196, 255);
    txt_desenhar_alpha(l, x, y + 4.0f, pulso * a);
  }
  // O botao da pagina do titulo, embaixo: a unica acao que SAI da toca.
  { const char *rot = i18n(abrindo ? "Abrindo…" : "Abrir título");
    TxtLinha l = txt_linha(TXT_BODY, rot, 240, 241, 245, 255);
    GfxRect b = { x, 900.0f, l.w + 56.0f, 60.0f };
    int tinta;
    ponteiro_alvo(b.x, b.y, b.w, b.h, ponteiroViz, NULL, -1, 0);
    if (vzLinha < 0) b = crescer(b, 0.05f * f);
    if (ajustes_vidro()) {
      gfx_vidro_painel(b, 0.5f, 0.55f, a);
      gfx_vidro_pilula_cheia(b, 0.5f, vzLinha < 0 ? f : 0.0f, a);
      tinta = gfx_vidro_tinta(vzLinha < 0 ? f : 0.0f);
    } else if (vzLinha < 0) {
      float ar, ag, ab;
      ajustes_acento(&ar, &ag, &ab);
      gfx_cor(b, 0.5f, ar, ag, ab, a);
      tinta = ajustes_tinta_foco();
    } else {
      gfx_cor(b, 0.5f, 0.14f, 0.15f, 0.17f, 0.92f * a);
      tinta = 240;
    }
    l = txt_linha(TXT_BODY, rot, tinta, tinta, tinta, 255);
    txt_desenhar_alpha(l, b.x + (b.w - l.w) * 0.5f, b.y + (b.h - l.h) * 0.5f, a); }
}

static void desenharGrupos(float a) {
  int g[MAPA_VIZ_GRUPOS], n = gruposVisiveis(g), li, i, tr, tg, tb;
  float x0 = vzX(), y = EX_TOPO - 4.0f;
  tintaAcento(&tr, &tg, &tb);
  if (n == 0) {
#ifdef NV_TOUCH_UI
    if (telefoneui_ativo()) {
      txt_bloco_corta(TXT_HEADLINE, i18n("Nenhum vizinho no catálogo ainda"), 220, 222, 230,
                      x0, 420, EX_DIR - x0, 46, a, 0);
    } else
#endif
    {
    TxtLinha v = txt_linha(TXT_HEADLINE, i18n("Nenhum vizinho no catálogo ainda"), 220, 222, 230, 255);
    txt_desenhar_alpha(v, x0, 420.0f, a);
    }
    return;
  }
  for (li = 0; li < n; li++) {
    const MapaVizGrupo *gr = &viz.g[g[li]];
    float e = ajustes_animacoes_reduzidas() ? 1.0f : suave((entrada - 0.06f * (float)li) / 0.4f);
    float ae = a * e, x = x0, posterY = y + 54.0f;
    char cab[140];
    if (ae <= 0.01f) { y += EX_VZ_LINHA; continue; }
    cabecalhoGrupo(gr, cab, sizeof cab);
    { TxtLinha l = txt_linha_corta(TXT_HEADLINE, cab, 236, 238, 244, 255, EX_DIR - x0);
      txt_desenhar_alpha(l, x, y, ae);
      if (gr->tipo == MAPA_GR_AMIGOS && gr->sub[0]) {
#ifdef NV_TOUCH_UI
        if (telefoneui_ativo()) {
          TxtLinha s = txt_linha_corta(TXT_CAPTION2, gr->sub, tr, tg, tb, 255, EX_DIR - x0);
          float sy = y + l.h + 4;
          txt_desenhar_alpha(s, x, sy, ae);
          posterY = fmaxf(posterY, sy + s.h + 12);
        } else
#endif
        {
        TxtLinha s = txt_linha_corta(TXT_CAPTION2, gr->sub, tr, tg, tb, 255, EX_DIR - x0 - l.w - 30.0f);
        txt_desenhar_alpha(s, x + l.w + 18.0f, y + (l.h - s.h) * 0.6f, ae);
        }
      } }
#ifdef NV_TOUCH_UI
    if (layout_modo_mobile()) {
    float largura = EX_DIR - x0;
    float max = vizRolarFileira(g[li], li, largura);
    toquerol_vincular(&toqueVz[g[li]], (GfxRect){x0 - 10.0f, posterY - 10.0f, largura + 14.0f, EX_VZ_PH + 20.0f},
                     gfx_escala(), 0.0f, max, 0, &toqueVzOffset[g[li]]);
    ponteiro_rolagem(toqueExplorarRolar);
    gfx_recorte(x0 - 10.0f, 0.0f, largura + 14.0f, NV_TELA_H);
    x -= toqueVzOffset[g[li]];
    }
#endif
    for (i = 0; i < gr->n; i++, x += EX_VZ_CARD + EX_VZ_GAP) {
      const MapaVizItem *it = &gr->itens[i];
      int sel = vzLinha == li && vzCol == i;
      float f = sel ? focoT : 0.0f, ty;
      GfxRect p = { x, posterY, EX_VZ_PW, EX_VZ_PH };
      char pq[140];
#ifdef NV_TOUCH_UI
      if (layout_modo_mobile()) {
      if (x - 10.0f > EX_DIR + 4.0f) break;
      if (x + EX_VZ_CARD + 4.0f < x0 - 10.0f) continue;
      explorarAlvoFileira(x - 10.0f, p.y - 10.0f, EX_VZ_CARD + 14.0f, EX_VZ_PH + 20.0f,
                         x0 - 10.0f, EX_DIR + 4.0f, ponteiroViz, li, i);
      } else
#endif
      {
      if (x + EX_VZ_CARD > EX_DIR + 4.0f) break;
      ponteiro_alvo(x - 10.0f, p.y - 10.0f, EX_VZ_CARD + 14.0f, EX_VZ_PH + 20.0f, ponteiroViz, NULL, li, i);
      }
      if (sel) {
        GfxRect fundo = { x - 10.0f, p.y - 10.0f, EX_VZ_CARD + 14.0f, EX_VZ_PH + 20.0f };
        painel(fundo, 14.0f / fundo.h, ae * f * 0.8f);
        p = crescer(p, 0.05f * f);
      }
      cartaz(&it->obra, p, f, ae);
      if (it->visto) seloVisto(p, ae);
      ty = p.y + 2.0f;
      ty += txt_bloco_corta(TXT_CAPTION, it->obra.titulo, sel ? 255 : 230, sel ? 255 : 232, sel ? 255 : 238,
                            x + EX_VZ_PW + 14.0f, ty, EX_VZ_CARD - EX_VZ_PW - 18.0f, 31.0f, ae, 2) + 4.0f;
      porque(gr, it, pq, sizeof pq);
      txt_bloco_corta(TXT_CAPTION2, pq, tr, tg, tb, x + EX_VZ_PW + 14.0f, ty,
                      EX_VZ_CARD - EX_VZ_PW - 18.0f, 26.0f, ae * (sel ? 1.0f : 0.85f), 3);
    }
#ifdef NV_TOUCH_UI
    if (layout_modo_mobile()) gfx_sem_recorte();
#endif
    y += EX_VZ_LINHA + posterY - (y + 54.0f);
  }
}

#ifdef NV_TOUCH_UI
typedef struct {
  float escala, x, w, topo, baixo, fim, trilhaH, heroY, textoX, textoW;
  GfxRect poster, botao;
  float y[MAPA_VIZ_GRUPOS], cabH[MAPA_VIZ_GRUPOS], cardH[MAPA_VIZ_GRUPOS];
  float tituloH[MAPA_VIZ_GRUPOS][MAPA_CLIMA_ITENS], motivoH[MAPA_VIZ_GRUPOS][MAPA_CLIMA_ITENS];
  int grupos[MAPA_VIZ_GRUPOS], n;
} ExTelefonePlano;
static ExTelefonePlano exPlano;

static float telefoneClimaPasso(void) {
  float w = (EX_DIR - ajustes_conteudo_x()) / gfx_escala_ui();
  return fminf(260, w * .45f) + EX_CAR_GAP;
}
static float telefoneCardPasso(void) {
  return fminf(620, (EX_DIR - ajustes_conteudo_x()) / gfx_escala_ui()) + 24;
}
static float telefoneTexto(const char *s, float x, float y, float w, float a, int r, int g, int b) {
  return climaBlocoTelefone(TXT_HEADLINE, s, r, g, b, x, y, w, 48, a);
}
static void telefoneVisto(GfxRect r,float a) {
  float h=telefoneTexto("Visto",0,0,r.w-36,0,20,20,24);
  GfxRect selo={r.x+6,r.y+6,r.w-12,h+12};
  gfx_cor(selo,18/selo.h,.92f,.93f,.95f,.92f*a);
  telefoneTexto("Visto",selo.x+12,selo.y+6,selo.w-24,a,20,20,24);
}
static void telefoneTrilha(char *s, size_t n) {
  s[0] = 0;
  int inicio = nTrilha > 2 ? nTrilha - 2 : 0;
  if (inicio > 0) snprintf(s,n,"…  ›  ");
  else if (origem == ORIGEM_CLIMA && clAberto >= 0 && clAberto < climas.n)
    snprintf(s,n,"%s  ›  ",i18n(mapa_clima_nome(climas.c[clAberto].id)));
  for (int i=inicio;i<nTrilha;i++) {
    size_t usado=strlen(s);
    snprintf(s+usado,n-usado,"%s%s%s%s",i>inicio ? "  ›  " : "",
             i>inicio ? trilha[i].via : "",i>inicio && trilha[i].via[0] ? "  ›  " : "",trilha[i].obra.titulo);
  }
}
static float telefoneHeroTexto(ExTelefonePlano *p, float y, float a) {
  float inicio=y; char meta[120];
  float th=climaBlocoTelefone(TXT_TITULO3,viz.foco.titulo,246,246,248,p->textoX,y,p->textoW,58,a);
  y+=th+12;
  metaObra(&viz.foco,meta,sizeof meta);
  float mh=telefoneTexto(meta,p->textoX,y,p->textoW,a,186,190,202);y+=mh+8;
  float gh=0;
  if(viz.generos[0]) {gh=telefoneTexto(viz.generos,p->textoX,y,p->textoW,a,160,166,182);y+=gh+8;}
  if(viz.carregando)y+=telefoneTexto("Cruzando histórias…",p->textoX,y,p->textoW,a,170,176,196)+8;
  return y-inicio;
}
static void telefoneMedir(ExTelefonePlano *p) {
  memset(p,0,sizeof *p);
  p->escala=gfx_escala_ui(); p->x=ajustes_conteudo_x()/p->escala;
  p->w=(EX_DIR-ajustes_conteudo_x())/p->escala;
  p->topo=140; p->baixo=NV_TELA_H/p->escala-40;
  float y=p->topo;
  if(modo==MODO_CLIMA) {
    const MapaClima *c=&climas.c[clAberto];
    y+=telefoneTexto("CLIMA",0,0,p->w,0,170,176,196)+12;
    y+=climaBlocoTelefone(TXT_TITULO3,mapa_clima_nome(c->id),246,246,248,0,0,p->w,58,0)+8;
    y+=telefoneTexto(mapa_clima_descricao(c->id),0,0,p->w,0,186,190,202)+32;
    if(c->carregando)y+=telefoneTexto("Buscando mais títulos…",0,0,p->w,0,170,176,196)+16;
    float pw=telefoneClimaPasso()-EX_CAR_GAP;
    for(int l=0;l<2;l++)if(caN[l]>0) {
      char s[120];snprintf(s,sizeof s,"%s   ·   %d",i18n(l ? "VOCÊ JÁ VIU" : "PARA DESCOBRIR"),caN[l]);
      p->y[l]=y;p->cabH[l]=telefoneTexto(s,0,0,p->w,0,170,176,196);
      p->cardH[l]=pw*1.5f+16;
      for(int i=0;i<caN[l];i++) {
        p->tituloH[l][i]=telefoneTexto(c->itens[caIdx[l][i]].titulo,0,0,pw,0,240,241,245);
        p->cardH[l]=fmaxf(p->cardH[l],pw*1.5f+16+p->tituloH[l][i]);
      }
      y+=p->cabH[l]+16+p->cardH[l]+32; p->n++;
    }
    if(!p->n)y+=climaBlocoTelefone(TXT_TITULO3,"Nada deste clima no seu catálogo ainda",236,238,244,0,0,p->w,58,0);
  } else {
    char trilhaTexto[1024];telefoneTrilha(trilhaTexto,sizeof trilhaTexto);
    p->trilhaH=telefoneTexto(trilhaTexto,0,0,p->w,0,170,176,196);
    y+=p->trilhaH+28;p->heroY=y;
    p->poster=(GfxRect){p->x,y,180,270};p->textoX=p->x+204;p->textoW=p->w-204;
    float textoH=telefoneHeroTexto(p,0,0);
    float botaoH=fmaxf(128,48+telefoneTexto(abrindo ? "Abrindo…" : "Abrir título",0,0,p->textoW-48,0,240,241,245));
    p->botao=(GfxRect){p->textoX,y+textoH+20,p->textoW,botaoH};
    y+=fmaxf(270,textoH+20+botaoH)+40;
    p->n=gruposVisiveis(p->grupos);
    float cw=telefoneCardPasso()-24, tw=cw-204;
    for(int l=0;l<p->n;l++) {
      const MapaVizGrupo *g=&viz.g[p->grupos[l]];char cab[140];cabecalhoGrupo(g,cab,sizeof cab);
      p->y[l]=y;p->cabH[l]=telefoneTexto(cab,0,0,p->w,0,236,238,244);
      if(g->tipo==MAPA_GR_AMIGOS && g->sub[0])p->cabH[l]+=8+telefoneTexto(g->sub,0,0,p->w,0,170,176,196);
      p->cardH[l]=264;
      for(int i=0;i<g->n;i++) {
        char pq[140];porque(g,&g->itens[i],pq,sizeof pq);
        p->tituloH[l][i]=telefoneTexto(g->itens[i].obra.titulo,0,0,tw,0,240,241,245);
        p->motivoH[l][i]=telefoneTexto(pq,0,0,tw,0,170,176,196);
        p->cardH[l]=fmaxf(p->cardH[l],24+p->tituloH[l][i]+12+p->motivoH[l][i]);
      }
      y+=p->cabH[l]+16+p->cardH[l]+32;
    }
    if(!p->n)y+=telefoneTexto("Nenhum vizinho no catálogo ainda",0,0,p->w,0,220,222,230);
  }
  p->fim=y;
}
static void telefoneRetomarFoco(void) {
  if(!toquerol_livre(&toquePagina) || exPlano.n<=0)return;
  float centro=(exPlano.topo+exPlano.baixo)*.5f, melhor=1e9f;int linha=-1;
  for(int l=0;l<MAPA_VIZ_GRUPOS;l++)if(exPlano.cardH[l]>0) {
    float d=fabsf(exPlano.y[l]-exPagina+exPlano.cabH[l]+16+exPlano.cardH[l]*.5f-centro);
    if(d<melhor){melhor=d;linha=l;}
  }
  if(linha>=0) {if(modo==MODO_CLIMA)caLinha=linha;else vzLinha=linha;}
}
static void telefoneAlvo(GfxRect r, PonteiroFn f, int linha, int col) {
  float x0=fmaxf(r.x,exPlano.x),x1=fminf(r.x+r.w,exPlano.x+exPlano.w);
  float y0=fmaxf(r.y,exPlano.topo),y1=fminf(r.y+r.h,exPlano.baixo);
  if(x1>x0 && y1>y0)ponteiro_alvo(x0,y0,x1-x0,y1-y0,f,NULL,linha,col);
}
static void telefoneRecorte(float x,float y,float w,float h) {
  float y0=fmaxf(y,exPlano.topo),y1=fminf(y+h,exPlano.baixo);
  gfx_recorte(x,y0,w,fmaxf(0,y1-y0));
}
static void telefonePaginaVincular(void) {
  float max=fmaxf(0,exPlano.fim-exPlano.baixo);
  if(exPaginaRestaurar) {
    exPagina=exPaginaDesejada;
    int pronto=modo==MODO_CLIMA ? !climas.c[clAberto].carregando :
      nTrilha>0 && !viz.carregando && !strcmp(viz.foco.imdb,trilha[nTrilha-1].obra.imdb) &&
      !strcmp(viz.foco.tipo,trilha[nTrilha-1].obra.tipo) && viz.foco.tmdb==trilha[nTrilha-1].obra.tmdb &&
      !strcmp(viz.foco.titulo,trilha[nTrilha-1].obra.titulo);
    if(pronto)exPaginaRestaurar=0;
  }
  if(exNavegar) {
    int l=modo==MODO_CLIMA ? caLinha : vzLinha;
    float y,h;
    if(l<0){y=exPlano.botao.y;h=exPlano.botao.h;}
    else {y=exPlano.y[l];h=exPlano.cabH[l]+16+exPlano.cardH[l];}
    if(y-exPagina<exPlano.topo)exPagina=y-exPlano.topo;
    if(y+h-exPagina>exPlano.baixo)exPagina=y+h-exPlano.baixo;
    exNavegar=0;
  }
  exPagina=anim_clamp(exPagina,0,max);
  toquerol_vincular(&toquePagina,(GfxRect){exPlano.x,exPlano.topo,exPlano.w,exPlano.baixo-exPlano.topo},
                   exPlano.escala,0,max,1,&exPagina);
  ponteiro_rolagem(toqueExplorarRolar);
}
static float telefoneComecar(void) {
  float anterior=gfx_escala_entrar();telefoneMedir(&exPlano);telefonePaginaVincular();
  if(!menu_pilula_titulo()) {
    TxtLinha titulo=txt_linha(TXT_TITULO2,i18n("Explorar"),246,246,248,255);
    txt_desenhar(titulo,exPlano.x,54);
  }
  return anterior;
}
static void desenharClimaTelefone(void) {
  if(clAberto<0 || clAberto>=climas.n)return;
  float anterior=telefoneComecar(),a=ajustes_animacoes_reduzidas()?1:suave(entrada/.4f);
  const MapaClima *c=&climas.c[clAberto];
  float x=exPlano.x,y=exPlano.topo-exPagina,pw=telefoneClimaPasso()-EX_CAR_GAP,ph=pw*1.5f;
  telefoneRecorte(x,exPlano.topo,exPlano.w,exPlano.baixo-exPlano.topo);
  y+=telefoneTexto("CLIMA",x,y,exPlano.w,a,170,176,196)+12;
  y+=climaBlocoTelefone(TXT_TITULO3,mapa_clima_nome(c->id),246,246,248,x,y,exPlano.w,58,a)+8;
  y+=telefoneTexto(mapa_clima_descricao(c->id),x,y,exPlano.w,a,186,190,202)+32;
  if(c->carregando)y+=telefoneTexto("Buscando mais títulos…",x,y,exPlano.w,a,170,176,196)+16;
  if(!exPlano.n)climaBlocoTelefone(TXT_TITULO3,"Nada deste clima no seu catálogo ainda",236,238,244,x,y,exPlano.w,58,a);
  gfx_sem_recorte();
  for(int l=0;l<2;l++)if(caN[l]>0) {
    char s[120];snprintf(s,sizeof s,"%s   ·   %d",i18n(l ? "VOCÊ JÁ VIU" : "PARA DESCOBRIR"),caN[l]);
    float hy=exPlano.y[l]-exPagina,cy=hy+exPlano.cabH[l]+16;
    if(hy+exPlano.cabH[l]+16+exPlano.cardH[l]<exPlano.topo || hy>exPlano.baixo)continue;
    telefoneRecorte(x,exPlano.topo,exPlano.w,exPlano.baixo-exPlano.topo);
    telefoneTexto(s,x,hy,exPlano.w,a,170,176,196);
    gfx_sem_recorte();
    float max=fmaxf(0,caN[l]*telefoneClimaPasso()-EX_CAR_GAP-exPlano.w);
    caRolar[l]=anim_clamp(caRolar[l],0,max);
    float y0=fmaxf(cy,exPlano.topo),y1=fminf(cy+exPlano.cardH[l],exPlano.baixo);
    if(y1>y0)toquerol_vincular(&toqueCa[l],(GfxRect){x,y0,exPlano.w,y1-y0},exPlano.escala,0,max,0,&caRolar[l]);
    telefoneRecorte(x,cy,exPlano.w,exPlano.cardH[l]);
    for(int i=0;i<caN[l];i++) {
      float px=x+i*telefoneClimaPasso()-caRolar[l];
      if(px+pw<=x || px>=x+exPlano.w)continue;
      const MapaObra *o=&c->itens[caIdx[l][i]];GfxRect r={px,cy,pw,exPlano.cardH[l]};
      telefoneAlvo(r,ponteiroClima,l,i);
      cartaz(o,(GfxRect){px,cy,pw,ph},l==caLinha && i==caCol[l]?focoT:0,a);
      telefoneTexto(o->titulo,px,cy+ph+16,pw,a,240,241,245);
    }
    gfx_sem_recorte();
  }
  gfx_escala_sair(anterior);
}
static void desenharVizTelefone(void) {
  float anterior=telefoneComecar(),a=ajustes_animacoes_reduzidas()?1:suave(entrada/.35f),x=exPlano.x;
  char s[1024];telefoneTrilha(s,sizeof s);
  telefoneRecorte(x,exPlano.topo,exPlano.w,exPlano.baixo-exPlano.topo);
  telefoneTexto(s,x,exPlano.topo-exPagina,exPlano.w,1,170,176,196);
  GfxRect poster=exPlano.poster;poster.y-=exPagina;cartaz(&viz.foco,poster,0,a);
  if(viz.focoVisto)telefoneVisto(poster,a);
  telefoneHeroTexto(&exPlano,exPlano.heroY-exPagina,a);
  GfxRect b=exPlano.botao;b.y-=exPagina;telefoneAlvo(b,ponteiroViz,-1,0);
  float ar,ag,ab;ajustes_acento(&ar,&ag,&ab);
  gfx_cor(b,22/b.h,vzLinha<0?ar:.14f,vzLinha<0?ag:.15f,vzLinha<0?ab:.17f,a);
  const char *rot=abrindo ? "Abrindo…" : "Abrir título";
  float rotH=telefoneTexto(rot,0,0,b.w-48,0,240,241,245);
  telefoneTexto(rot,b.x+24,b.y+(b.h-rotH)*.5f,b.w-48,a,240,241,245);
  gfx_sem_recorte();
  float cw=telefoneCardPasso()-24,tw=cw-204;
  for(int l=0;l<exPlano.n;l++) {
    int grupo=exPlano.grupos[l];const MapaVizGrupo *g=&viz.g[grupo];char cab[140];cabecalhoGrupo(g,cab,sizeof cab);
    float hy=exPlano.y[l]-exPagina,cy=hy+exPlano.cabH[l]+16;
    if(hy+exPlano.cabH[l]+16+exPlano.cardH[l]<exPlano.topo || hy>exPlano.baixo)continue;
    telefoneRecorte(x,exPlano.topo,exPlano.w,exPlano.baixo-exPlano.topo);
    float cabH=telefoneTexto(cab,x,hy,exPlano.w,a,236,238,244);
    if(g->tipo==MAPA_GR_AMIGOS && g->sub[0])telefoneTexto(g->sub,x,hy+cabH+8,exPlano.w,a,170,176,196);
    gfx_sem_recorte();
    float max=fmaxf(0,g->n*telefoneCardPasso()-24-exPlano.w),*offset=&toqueVzOffset[grupo];
    if(!toquerol_livre(&toqueVz[grupo]) && vzLinha==l) {
      float pos=vzCol*telefoneCardPasso();
      if(pos<*offset)*offset=pos;
      if(pos+cw>*offset+exPlano.w)*offset=pos+cw-exPlano.w;
    }
    *offset=anim_clamp(*offset,0,max);
    float y0=fmaxf(cy,exPlano.topo),y1=fminf(cy+exPlano.cardH[l],exPlano.baixo);
    if(y1>y0)toquerol_vincular(&toqueVz[grupo],(GfxRect){x,y0,exPlano.w,y1-y0},exPlano.escala,0,max,0,offset);
    telefoneRecorte(x,cy,exPlano.w,exPlano.cardH[l]);
    for(int i=0;i<g->n;i++) {
      GfxRect r={x+i*telefoneCardPasso()-*offset,cy,cw,exPlano.cardH[l]};
      if(r.x+r.w<=x || r.x>=x+exPlano.w)continue;
      const MapaVizItem *it=&g->itens[i];int sel=l==vzLinha && i==vzCol;
      telefoneAlvo(r,ponteiroViz,l,i);
      if(sel)painel(r,16/r.h,a*.8f);
      GfxRect p={r.x+12,r.y+12,160,240};cartaz(&it->obra,p,sel?focoT:0,a);if(it->visto)telefoneVisto(p,a);
      float th=telefoneTexto(it->obra.titulo,r.x+184,r.y+12,tw,a,240,241,245),ty=r.y+12+th+12;
      char pq[140];porque(g,it,pq,sizeof pq);telefoneTexto(pq,r.x+184,ty,tw,a,170,176,196);
    }
    gfx_sem_recorte();
  }
  if(!exPlano.n) {
    telefoneRecorte(x,exPlano.topo,exPlano.w,exPlano.baixo-exPlano.topo);
    telefoneTexto("Nenhum vizinho no catálogo ainda",x,exPlano.botao.y+exPlano.botao.h+40-exPagina,exPlano.w,a,220,222,230);
    gfx_sem_recorte();
  }
  gfx_escala_sair(anterior);
}
#endif

static void desenharViz(void) {
#ifdef NV_TOUCH_UI
  if (telefoneui_ativo()) { desenharVizTelefone(); return; }
#endif
  float a = ajustes_animacoes_reduzidas() ? 1.0f : suave(entrada / 0.35f);
  desenharTitulo(NULL);
  desenharTrilha(1.0f);
  desenharHero(a);
  desenharGrupos(a);
  desenharDica(i18n(nTrilha > 1 ? "OK   Descer pela toca   ·   Voltar   Subir um degrau"
                                : "OK   Descer pela toca   ·   Voltar   Sair da toca"));
}

// --- ciclo ------------------------------------------------------------------------

void explorar_iniciar(void) {
#ifdef NV_TOUCH_UI
  exPagina = caPagina = 0; toquePagina = (ToqueRolagem){0}; toquePaginaAtiva = exNavegar = 0;
  exPaginaRestaurar=0;exPaginaDesejada=0;
  clRolar=0; toqueCl=(ToqueRolagem){0};
#endif
  sair = pediuAbrir = abrindo = pediuRelogio = 0;
  tempo = 0.0f;
  memset(&climas, 0, sizeof climas);
  climasRev = 0;
  clFoco = 0;
  clAberto = -1;
  nTrilha = 0;
  origem = ORIGEM_NADA;
  trocarModo(MODO_CLIMAS);
  mapa_climas_pedir();
  mapa_climas_copiar(&climas, &climasRev);
}

void explorar_abrir_titulo(const MapaObra *o) {
  pediuRelogio = 0;
  if (!o || !o->titulo[0]) return;
  if (climas.n == 0) { mapa_climas_pedir(); mapa_climas_copiar(&climas, &climasRev); }
  clAberto = -1;
  comecarToca(o, ORIGEM_DETALHE);
}

void explorar_encerrar(void) {
  pediuRelogio = 0;
  pediuAbrir = 0;
  sair = 0;
}

void explorar_atualizar(float dt, Uint32 agora) {
  int reduzida = ajustes_animacoes_reduzidas();
  (void)agora;
  if (mapa_climas_copiar(&climas, &climasRev)) {
#ifdef NV_TOUCH_UI
    if(telefoneui_ativo() && modo!=MODO_VIZ)
      ponteiro_cancelar_alvo(modo==MODO_CLIMAS ? ponteiroClimas : ponteiroClima);
#endif
    if (clFoco >= climas.n) clFoco = climas.n > 0 ? climas.n - 1 : 0;
    // O /discover do clima aberto chegou: as fileiras crescem, o foco fica.
    climaSeparar();
  }
  if (mapa_vizinhos_copiar(&viz, &vizRev)) {
#ifdef NV_TOUCH_UI
    if(telefoneui_ativo() && modo==MODO_VIZ)ponteiro_cancelar_alvo(ponteiroViz);
#endif
    vizFiltrarTrilha(); vizAjustarFoco();
  }
  tempo += reduzida ? 0.0f : dt;
  entrada += dt;
  if (reduzida) entrada = 10.0f;
  focoT = reduzida ? 1.0f : anim_mola(focoT, 1.0f, dt, 14.0f);
  if (abrindo && !desc_titulo_buscando()) abrindo = 0;
  // Rolagem das fileiras do clima: o cartaz em foco sempre inteiro na tela.
  if (modo == MODO_CLIMA) {
    int k;
    for (k = 0; k < 2; k++) {
      float w = (k == 1 && caN[0] > 0) ? EX_CB_W : EX_CA_W;
      float visivel = EX_DIR - ajustes_conteudo_x();
#ifdef NV_TOUCH_UI
      if (telefoneui_ativo()) { w = telefoneClimaPasso() - EX_CAR_GAP; visivel /= gfx_escala_ui(); }
#endif
      float alvo = (float)caCol[k] * (w + EX_CAR_GAP) - visivel * 0.35f;
      float max = (float)caN[k] * (w + EX_CAR_GAP) - EX_CAR_GAP - visivel;
      if (alvo > max) alvo = max;
      if (alvo < 0.0f) alvo = 0.0f;
#ifdef NV_TOUCH_UI
      if (toquerol_livre(&toqueCa[k])) caRolar[k] = anim_clamp(caRolar[k], 0.0f, fmaxf(0.0f, max));
      else
#endif
        caRolar[k] = reduzida ? alvo : anim_mola(caRolar[k], alvo, dt, 10.0f);
    }
  }
}

void explorar_desenhar(Uint32 agora) {
#ifdef NV_TOUCH_UI
  toqueCl.offset=NULL;
  toquePagina.offset=NULL;
  toqueCa[0].offset = toqueCa[1].offset = NULL;
  for (int i = 0; i < MAPA_VIZ_GRUPOS; i++) toqueVz[i].offset = NULL;
#endif
  (void)agora;
  desenharFundo();
  if (modo == MODO_CLIMAS) desenharClimas();
  else if (modo == MODO_CLIMA) desenharClima();
  else desenharViz();
}

int explorar_quer_sair(void) {
  int v = sair;
  sair = 0;
  return v;
}

int explorar_pediu_relogio(void) { int v = pediuRelogio; pediuRelogio = 0; return v; }

int explorar_pediu_abrir(int *indice) {
  if (!pediuAbrir) return 0;
  pediuAbrir = 0;
  if (indice) *indice = pediuIndice;
  return 1;
}
