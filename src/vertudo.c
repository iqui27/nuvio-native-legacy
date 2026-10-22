#include "imdbnota.h"
#include "vertudo.h"
#include "posterprov.h"
#include "idioma.h"
#include "badges.h"
#include "descoberta.h"
#include "catalogo.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "perfiltv.h"
#include "layout.h"
#include "anim.h"
#include "revela.h"
#include "ajustes.h"
#include "diretor.h"
#include "addons.h"
#include "nuvem.h"
#include "fundo.h"
#include "focoprof.h"
#include "corviva.h"
#include "ctxlista.h"
#include "ctxmenu.h"
#include "ponteiro.h"
#include "rolagemtoque.h"
#include "telefoneui.h"
#include "escala.h"
#include "anos.h"
#include "salvos.h"
#include "dados.h"
#include "home.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

// MEDIDAS do web (catalogSeeAllScreen, .seeall-card): cartaz de 248 de largura
// e raio 12. A 1920 cabem 5 colunas com o gutter da tela dos dois lados.
// #359: com a rail fixa da Moderna o x0 vai a ~218 e a 5a coluna entrava
// ~42 px por baixo do painel da direita. Quantas colunas cabem sai de
// vtColunas (x0 de ajustes_conteudo_x ate o painel); recolhida e Padrao
// continuam com 5, como antes.
static int vtColunas(void);
static float vtCardLargura(void);
static float vtCardAltura(void);
#define VT_COLS      vtColunas()
#define VT_COLS_MAX  5
#define VT_CARD_BASE_W 248.0f
#define VT_CARD_W   vtCardLargura()
#define VT_CARD_H   vtCardAltura()
#define VT_GAP_X    16.0f                 // .seeall-grid: gap 20px 16px
#define VT_GAP_Y    (20.0f + 40.0f)       // gap + a linha de titulo sob o cartaz
#define VT_TOPO    (collection ? 332.0f : 244.0f)
// LARGURA UTIL DA GRADE: as 5 colunas de sempre (5 x 248 + 4 x 16). Toda
// exibicao cabe nela, e o painel de detalhe a direita nunca e invadido.
#define VT_CW      (5.0f * VT_CARD_W + 4.0f * VT_GAP_X)
// A linha da filmografia (exibicao "Linha do tempo"): cartao deitado de 236.
#define VT_LINHA_H 236.0f

// PAINEL DE DETALHE, a direita. Medidas do web (.seeall-detail), que as ancora
// explicitamente na tela real de 1920x1080: top 170, right 104, largura 336.
//
// FIXO e nao rolando junto: o proprio CSS registra que `position: sticky` nao
// existe no Chromium 53 da TV e que sem `fixed` o painel sumia assim que o dono
// descia. Aqui nao ha fluxo nenhum — ele so nao recebe scrollY.
#define VT_PAN_W   336.0f
#define VT_PAN_X   (NV_TELA_W - 104.0f - VT_PAN_W)
#define VT_PAN_VAO  24.0f   // vao minimo entre a ultima coluna e o painel
#define VT_PAN_Y   VT_TOPO
#define VT_PAN_ART_H 330.0f
#define VT_PAN_ART_W 220.0f
static int   aberta, foco, pedAbrir = -1;
static float anim, scrollY, velY;
#ifdef NV_TOUCH_UI
static int toqueLivre;
static ToqueRolagem toqueAbas;
static int toqueNasAbas;
#endif
static char  titulo[96];
static const ColFolder *collection;
static int source, tabFocus, tabCursor, timeline, ranked;
static float tabRol;
static int vtTelefone(void) {
  return telefoneui_ativo();
}
static float vtEscala(void) { return vtTelefone() ? gfx_escala_ui() : 1.0f; }
/* Keep measurements valid outside drawing too (focus, gestures and menus).
 * Ordinary TV/tablet pages keep their original unscaled viewport. */
#undef NV_TELA_W
#undef NV_TELA_H
#define NV_TELA_W (NV_LAYOUT_REAL_W / vtEscala())
#define NV_TELA_H (NV_LAYOUT_REAL_H / vtEscala())
static int vtTimelineRetrato(void) { return timeline && vtTelefone() && NV_TELA_H > NV_TELA_W; }
static float vtInicio(void) {
  return vtTelefone() ? 48.0f + ajustes_rail_largura_fixa() / vtEscala() : ajustes_conteudo_x();
}
static float vtFim(void) { return vtTelefone() ? NV_TELA_W - 48.0f : VT_PAN_X - VT_PAN_VAO; }
static float vtCabecalhoLargura(float x, float original) {
  if (!vtTelefone()) return original;
  float fim = NV_TELA_W - 48.0f;
  if (vtTimelineRetrato()) fim -= 180.0f;
  return fminf(original, fim - x);
}
static GfxRect vtTimelineCard(float cy) {
  float x = vtInicio() + (vtTimelineRetrato() ? 96.0f : 158.0f);
  float w = 1000.0f;
  if (vtTelefone()) w = fminf(w, vtFim() - x);
  return (GfxRect){x, cy, w, VT_CARD_H};
}
static GfxRect vtTimelineArte(GfxRect card) {
  float w = vtTelefone() ? fminf(376.0f, card.w * 0.36f) : 376.0f;
  return (GfxRect){card.x + 12.0f, card.y + 12.0f, w, w * 212.0f / 376.0f};
}
// Colunas que cabem entre o inicio do conteudo (a rail fixa entra por
// ajustes_conteudo_x, a fonte unica do recuo) e o painel, com VT_PAN_VAO de
// folga. Tira colunas, nao encolhe o cartaz (regra de ajustes_area_conteudo).
static int vtColunas(void) {
  if (timeline) return 1;
  if (vtTelefone()) {
    if (NV_TELA_H > NV_TELA_W) return 3;
    int n = (int)((vtFim() - vtInicio() + VT_GAP_X) / (VT_CARD_BASE_W + VT_GAP_X));
    return n < 1 ? 1 : n;
  }
  float livre = VT_PAN_X - VT_PAN_VAO - ajustes_conteudo_x() + VT_GAP_X;
  int n = (int)(livre / (VT_CARD_BASE_W + VT_GAP_X) + 0.001f);
  return n < 1 ? 1 : n > VT_COLS_MAX ? VT_COLS_MAX : n;
}
static float vtCardLargura(void) {
  if (!vtTelefone() || timeline) return VT_CARD_BASE_W;
  return fmaxf(1.0f, (vtFim() - vtInicio() - (VT_COLS - 1) * VT_GAP_X) / VT_COLS);
}
static float vtCardAltura(void) { return timeline ? 236.0f : VT_CARD_W * 1.5f; }
static float tabAnim[COL_SOURCE_MAX];
static int order[VT_MAX], orderN=-1;
static char catalogId[96];

// --- EXIBICAO DA COLECAO ------------------------------------------------------
//
// Pedido do dono (06/10): "nas colecoes vamos colocar tipos diferentes onde a
// pessoa pode escolher, igual temos o dos diretores que e um timeline". A
// filmografia em linha do tempo era a UNICA variacao, e so os Diretores a
// tinham. Agora toda colecao escolhe a sua, no botao "Exibição" ao lado das
// abas, e a escolha fica gravada por perfil e por pasta.
//
// AUTOMATICO (layAuto): Diretores = linha do tempo; todas as outras pastas
// usam Grade. As outras exibicoes so entram pela escolha no botao.
//
// TODAS AS EXIBICOES SAO A MESMA TELA: uma tabela de celulas (posicao e caixa
// de cada titulo, relativas ao inicio da grade) montada uma vez por mudanca, e
// a navegacao anda nela por LINHAS (celLin) e pelo centro mais proximo. Nenhuma
// conta de "colunas" sobrou no desenho nem no D-pad.
//
// CUSTO NA TV: nenhuma passada de blur, nenhuma textura nova por quadro. As
// artes saem do mesmo tex_cache por largura; Mural pede a arte deitada, que o
// cache ja tem das fileiras da home.
typedef enum {
  VT_LAY_AUTO = 0, VT_LAY_GRADE, VT_LAY_LINHA, VT_LAY_RANKING,
  VT_LAY_SAGA, VT_LAY_DECADAS, VT_LAY_MURAL, VT_LAY_VITRINE, VT_LAY_N
} VtLayout;
static const char *LAY_NOME[VT_LAY_N] = {
  "Automático", "Grade", "Linha do tempo", "Ranking", "Saga", "Décadas", "Mural", "Vitrine"
};
static int layEscolha;            // gravada (VT_LAY_AUTO = nunca escolheu)
static int semVitrineAuto;        // aberta por vertudo_abrir (ver layAuto)
static int lay = VT_LAY_GRADE;    // a que esta na tela
static float pillAnim;            // foco no botao "Exibição"
static float celX[VT_MAX], celY[VT_MAX], celW[VT_MAX], celH[VT_MAX];
static short celLin[VT_MAX], celCol[VT_MAX];
static int   celN = -1, celLay = -1;
static float celFim;              // altura do conteudo inteiro
// DECADAS: o cabecalho de cada secao (ano da decada, quantos titulos, y).
#define VT_DEC_MAX 40
#define VT_DEC_H   96.0f
static int   decN, decAno[VT_DEC_MAX], decQtd[VT_DEC_MAX];
static float decY[VT_DEC_MAX];
// SAGA: a trilha de capitulos acima de cada linha de cartoes.
#define VT_SAGA_TRILHA 44.0f
static float x0Atual;
// SAGA: quem ja foi visto, conferido na montagem e a cada 1,5 s (a volta do
// player marca um capitulo). Nunca por quadro: cada consulta copia o CatItem.
static unsigned char celVisto[VT_MAX];
static int sagaVistos;
static Uint32 vistosEm;             // ajustes_conteudo_x() do quadro (desenho)

static int ordenaPorAno(void) {
  return lay == VT_LAY_LINHA || lay == VT_LAY_SAGA || lay == VT_LAY_DECADAS;
}
// MICRO-ANIMACOES (revela.h): a arte chegando por cartaz e a onda da grade.
// A onda e ARMADA quando uma grade nova e pedida (abrir, trocar de fonte) e
// dispara no primeiro quadro que tem itens — rolar ou voltar do detalhe nao
// a repete.
static RevelaArte revArte[VT_MAX];
static Uint32 ondaEm;
static int ondaArmada;
static void armarOnda(void) {
  ondaArmada = 1; ondaEm = 0;
  memset(revArte, 0, sizeof revArte);
}
// A fonte escolhida NAO TEM ENDERECO ainda: o addon dela nao esta instalado
// nesta TV, ou a sonda de manifesto ainda nao respondeu. Ver openSource.
static int semFonte;
static int grupo(const char *nome) {
  return collection && !strcmp(collection->group, nome);
}
static void corColecao(float *r,float *g,float *b) {
  col_cor(collection,r,g,b);
  // Serviços preservam sua marca. Famílias editoriais sem marca própria usam
  // cor apenas em seleção/estado, mantendo as superfícies neutras.
  if (grupo("Genres")) {*r=.075f;*g=.34f;*b=.285f;}
  else if (grupo("Themes")) {*r=.34f;*g=.13f;*b=.38f;}
  else if (grupo("Film Collections") || grupo("TV Collections"))
    {*r=.12f;*g=.27f;*b=.40f;}
}

// As abas de fonte pertencem ao mesmo fluxo de selecao da folha de fontes.
// Mantem o acento reconhecivel, mas o mistura ao fundo para nao virar uma
// faixa azul/branca solta sobre a arte. A excecao para tons quase brancos
// preserva a leitura da tinta escura nos temas Branco e Grafite.
// Desde os acentos de 03/10 e o proprio acento (ver botao_cor_foco).
static void corFocoFonte(float *r, float *g, float *b) {
  ajustes_acento(r, g, b);
}

static void focoAbaFonte(GfxRect r, float f, float a) {
  float ar, ag, ab, sr, sg, sb;
  GfxRect luz;
  if (f <= 0.01f) return;
  ajustes_acento(&ar, &ag, &ab);
  luz = (GfxRect){r.x - r.h * 0.45f, r.y - r.h * 0.45f,
                  r.w + r.h * 0.90f, r.h * 2.0f};
  gfx_rect(luz, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f,
           ar, ag, ab, 0.16f * f * a);
  corFocoFonte(&sr, &sg, &sb);
  gfx_cor(r, NV_RAIO_PILL, sr, sg, sb, f * a);
  gfx_rect(r, 0, GFX_BRILHO_TOPO, NV_RAIO_PILL, 0.24f, 0, 0.5f,
           1, 1, 1, 0.10f * f * a);
}
static const char *rotuloGrupo(void) {
  if (grupo("Directors")) return "FILMOGRAFIA";
  if (grupo("Awards")) return "PRÊMIOS E CÂNONE";
  if (grupo("Genres")) return "GÊNERO";
  if (grupo("Themes")) return "TEMA";
  if (grupo("Streaming")) return "STREAMING";
  if (ranked) return "RANKING";
  return "COLEÇÃO";
}
static const char *legendaGrupo(void) {
  if (lay == VT_LAY_LINHA) return grupo("Directors") ? "Filmografia em ordem cronológica"
                                                    : "Em ordem cronológica";
  if (lay == VT_LAY_SAGA) return "Saga em ordem de lançamento";
  if (lay == VT_LAY_DECADAS) return "Organizado por década";
  if (grupo("Awards")) return ranked ? "Ranking na ordem original" : "Lista de prêmios configurada pelo usuário";
  if (grupo("Genres")) return "Títulos do gênero no seu catálogo";
  if (grupo("Themes")) return "Uma curadoria por tema e atmosfera";
  if (grupo("Streaming")) return "Catálogo organizado por serviço";
  return ranked ? "Ordem original do ranking" : "Seleção do seu catálogo";
}

// 9999 sem ano: na ordem cronologica o titulo sem data vai para o fim.
static int yearOf(const CatItem *it) {
  int a = nv_ano_meta(it->meta);
  return a ? a : 9999;
}
static int viewItem(int i,CatItem *out) {return desc_vertudo_item(ordenaPorAno()&&i<orderN?order[i]:i,out);}

// --- A ESCOLHA, por perfil e por pasta ---------------------------------------
// Arquivo proprio ("<id da colecao>\t<exibicao>" por linha), gravado por
// dados_gravar como o resto: no Tizen e o que leva a escolha para o IndexedDB.
// Nunca vai para a conta — e escolha deste aparelho, como as fileiras.
#define VT_ESC_MAX 160
static const char *arquivoEscolha(void) {
  static char nome[48];
  snprintf(nome, sizeof nome, "colecao-exibicao-p%d.txt", salvos_perfil_atual());
  return nome;
}
static void chavePasta(const ColFolder *f, char *dst, size_t n) {
  snprintf(dst, n, "%s/%s", f->groupId[0] ? f->groupId : f->group,
           f->id[0] ? f->id : f->title);
  for (char *p = dst; *p; p++) if (*p == '\t' || *p == '\n') *p = ' ';
}
// Le o arquivo inteiro: `chaves`/`vals` recebem ate VT_ESC_MAX linhas.
static int lerEscolhas(char (*chaves)[160], int *vals) {
  char cam[600], buf[300];
  int n = 0;
  FILE *f;
  if (!dados_caminho(cam, sizeof cam, arquivoEscolha())) return 0;
  if (!(f = fopen(cam, "r"))) return 0;
  while (n < VT_ESC_MAX && fgets(buf, sizeof buf, f)) {
    char *tab = strrchr(buf, '\t');
    if (!tab || buf[0] == '#') continue;
    *tab = 0;
    snprintf(chaves[n], 160, "%s", buf);
    vals[n] = atoi(tab + 1);
    if (vals[n] > VT_LAY_AUTO && vals[n] < VT_LAY_N) n++;
  }
  fclose(f);
  return n;
}
static int escolhaGravada(const ColFolder *pasta) {
  static char chaves[VT_ESC_MAX][160];
  int vals[VT_ESC_MAX], n, i;
  char k[160];
  if (!pasta) return VT_LAY_AUTO;
  chavePasta(pasta, k, sizeof k);
  n = lerEscolhas(chaves, vals);
  for (i = 0; i < n; i++) if (!strcmp(chaves[i], k)) return vals[i];
  return VT_LAY_AUTO;
}
static void gravarEscolha(const ColFolder *pasta, int v) {
  static char chaves[VT_ESC_MAX][160];
  int vals[VT_ESC_MAX], n, i, achou = 0;
  char k[160], *txt;
  size_t cap = (size_t)(VT_ESC_MAX + 2) * 172, used = 0;
  if (!pasta) return;
  chavePasta(pasta, k, sizeof k);
  n = lerEscolhas(chaves, vals);
  for (i = 0; i < n; i++) if (!strcmp(chaves[i], k)) { vals[i] = v; achou = 1; }
  // Cheio: a mais antiga sai (a lista e um historico, nao um cadastro).
  if (!achou) {
    if (n == VT_ESC_MAX) { memmove(chaves, chaves + 1, sizeof chaves[0] * (VT_ESC_MAX - 1));
                           memmove(vals, vals + 1, sizeof vals[0] * (VT_ESC_MAX - 1)); n--; }
    snprintf(chaves[n], 160, "%s", k); vals[n++] = v;
  }
  if (!(txt = malloc(cap))) return;
  used = (size_t)snprintf(txt, cap, "# Exibicao de cada colecao neste perfil (vertudo.c).\n");
  for (i = 0; i < n && used < cap; i++)
    used += (size_t)snprintf(txt + used, cap - used, "%s\t%d\n", chaves[i], vals[i]);
  if (used < cap) dados_gravar(arquivoEscolha(), txt);
  free(txt);
}

static int contem(const char *s, const char *termo) {
  size_t n = strlen(termo);
  for (; s && *s; s++) if (!strncasecmp(s, termo, n)) return 1;
  return 0;
}
// AUTOMATICO, pelo conteudo da pasta. Ver o cabecalho de VtLayout.
static int layAuto(void) {
  if (!collection) return VT_LAY_GRADE;
  if (grupo("Directors")) return VT_LAY_LINHA;
  // O PADRAO E O DA 2.0.3: Grade para toda pasta (com ou sem abas); so o
  // Diretor vai para a Linha do tempo, como ja ia. Ranking, Saga, Decadas, Mural
  // e Vitrine so entram quando o dono escolhe no botao "Exibição".
  return VT_LAY_GRADE;
}
static void resolverLayout(void) {
  int novo = layEscolha > VT_LAY_AUTO && layEscolha < VT_LAY_N ? layEscolha : layAuto();
  if (novo != lay) { lay = novo; celLay = -1; orderN = -1; }
  timeline = lay == VT_LAY_LINHA;
}
// O ENDERECO DA FONTE, que pode nao existir ainda.
//
// Uma pasta que veio da CONTA guarda o addonId (o "id" do manifesto) e, muitas
// vezes, nenhuma URL: quem resolve e o mapa de addons, e ele so conhece o id
// depois que a sonda leu o manifesto daquele addon. Uma pasta que veio do
// PACOTE ja traz a base escrita.
static const char *baseDaFonte(const ColSource *s) {
  if (s->base[0]) return s->base;
  return s->addonId[0] ? addons_base_por_id(s->addonId) : "";
}

static void vitIniciar(void);
static void carregarFonte(void);
static void openSource(void) {
  const ColSource *s=&collection->sources[source];
  snprintf(catalogId,sizeof catalogId,"%s",s->catId);
  ranked=strstr(s->catId,"top100")||strstr(s->catId,"top250")||strstr(s->catId,"top10");
  foco=0;scrollY=velY=0;orderN=-1;celLay=-1;armarOnda();
  resolverLayout();
  // VITRINE: nao ha "a fonte aberta"; cada aba vira uma fileira e vitIniciar
  // as carrega uma a uma.
  if (lay == VT_LAY_VITRINE) { vitIniciar(); return; }
  carregarFonte();
}
// Pede ao carregador da descoberta o catalogo de `source` (sem mexer na tela).
static void carregarFonte(void) {
  const ColSource *s=&collection->sources[source];
  const char *base=baseDaFonte(s);
  snprintf(catalogId,sizeof catalogId,"%s",s->catId);
#ifdef NV_TOUCH_UI
  toqueLivre = 0;
  toquerol_limpar(&toqueAbas);
#endif
  // FONTE NAO-ADDON (issue #44): "tmdb"/"trakt" vinda do site. Nao tem base
  // de catalogo — o conteudo e pedido direto ao servico pelo
  // desc_vertudo_fonte. "Sem fonte" aqui quer dizer servico nao configurado
  // nesta TV (sem chave do TMDB / sem client id do Trakt), nao addon
  // faltando.
  if (s->prov[0]) {
    semFonte = !strcmp(s->prov,"tmdb") ? !desc_chave_tmdb()[0]
                                      : !nuvem_trakt_cliente()[0];
    snprintf(catalogId,sizeof catalogId,"%s",s->title);
    if (semFonte) {
      printf("[vertudo] fonte %s sem servico configurado: %s\n",
             s->prov, s->title);
      fflush(stdout);
      return;
    }
    desc_vertudo_fonte(s);
    return;
  }
  // BASE VAZIA NAO VIRA PEDIDO.
  //
  // desc_vertudo_filtro so recusa ponteiro NULO, e `base` e um vetor dentro da
  // ColSource — nunca nulo, as vezes vazio. Com ele vazio a URL montada virava
  // "/catalog/movie/<id>.json", um caminho RELATIVO: no Tizen o XHR resolve
  // isso contra a origem do proprio widget, a resposta e o index.html e nada
  // decodifica. O resultado na tela era a colecao abrindo sem um unico cartaz e
  // sem explicacao (issue #10). Agora a tela DIZ o que falta.
  semFonte = !base[0];
  if (semFonte) {
    printf("[vertudo] fonte sem endereco: addonId=\"%s\" cat=%s/%s\n",
           s->addonId, s->type, s->catId);
    fflush(stdout);
    return;
  }
  desc_vertudo_filtro(base,s->type,s->catId,s->genre);
}

void vertudo_colecao(const ColFolder *folder) {
  int i;
  if(!folder||!folder->nSources)return;
  memset(tabAnim, 0, sizeof tabAnim);
#ifdef NV_TOUCH_UI
  tabRol = 0.0f;
#endif
  collection=folder;source=tabCursor=0;tabFocus=folder->nSources>1;aberta=1;pedAbrir=-1;
  pillAnim=0;layEscolha=escolhaGravada(folder);semVitrineAuto=0;
  // COMECAR NA PRIMEIRA FONTE QUE TEM ENDERECO, e nao teimosamente na fonte 0.
  // Numa pasta com varias abas e comum que so parte dos addons esteja instalada
  // aqui; abrir na aba morta faz a pasta inteira parecer vazia.
  for (i = 0; i < folder->nSources; i++)
    if (folder->sources[i].prov[0] || baseDaFonte(&folder->sources[i])[0])
      { source = tabCursor = i; break; }
  snprintf(titulo,sizeof titulo,"%s",folder->title);openSource();
  if(lay==VT_LAY_VITRINE)tabFocus=0;   // sem barra de abas: o foco nasce nas fileiras
}

void vertudo_abrir(const char *base, const char *tipo, const char *catId,
                   const char *tit) {
  aberta = 1; foco = 0; scrollY = 0.0f; velY = 0.0f; pedAbrir = -1;
#ifdef NV_TOUCH_UI
  toqueLivre = 0;
  toquerol_limpar(&toqueAbas);
  tabRol = 0.0f;
#endif
  memset(tabAnim, 0, sizeof tabAnim);
  snprintf(titulo, sizeof titulo, "%s", tit ? tit : "");
  collection=col_por_catalogo(base,tipo,catId);
  source=tabCursor=tabFocus=0;orderN=-1;celLay=-1;pillAnim=0;
  layEscolha=escolhaGravada(collection);semVitrineAuto=1;
  if(layEscolha==VT_LAY_VITRINE)layEscolha=VT_LAY_AUTO;   // "Ver tudo" e uma fileira so
  if(collection) {
    for(int i=0;i<collection->nSources;i++)if(!strcmp(collection->sources[i].catId,catId)&&!strcmp(collection->sources[i].type,tipo)){source=tabCursor=i;break;}
    snprintf(titulo,sizeof titulo,"%s",collection->title);
  }
  snprintf(catalogId,sizeof catalogId,"%s",catId);
  ranked=strstr(catId,"top100")||strstr(catId,"top250")||strstr(catId,"top10");
  armarOnda();
  // Sem pasta (o "Ver tudo" de uma fileira de catalogo) a tela e a grade de
  // sempre: a escolha de exibicao e coisa de colecao.
  resolverLayout();
  desc_vertudo_abrir(base, tipo, catId);
}

int vertudo_aberta(void) { return aberta; }
int vertudo_foco(void) { return foco; }
void vertudo_fechar_seco(void) { aberta = 0; anim = 0.0f; }
int vertudo_pediu_abrir(void) { int v = pedAbrir; pedAbrir = -1; return v; }
// ESQUERDA na coluna 0 da grade (ou na primeira aba da colecao): a barra
// lateral abre POR CIMA da lista (dono, 03/10). Antes a coluna 0 voltava para
// o ultimo da linha de cima. app.c le no mesmo evento.
static int pedMenu;
int vertudo_pediu_menu(void) { int v = pedMenu; pedMenu = 0; return v; }

static int nItens(void) { return desc_vertudo_n(); }

// O OK NUM CARTAO: abre a pagina do titulo (o toque curto) ou, SEGURANDO, o menu
// do cartaz (ctxlista.h). O item da grade NAO esta no catalogo global — ele veio
// de uma pagina que so esta tela leu. Entra por cat_acrescentar para que a tela
// de titulo possa abri-lo por indice, que e como todo o app trabalha.
static CtxHold hold;

static void abrirItem(const CatItem *src);
static void abrirFocado(void) {
  CatItem it;
  if (viewItem(foco, &it)) abrirItem(&it);
}

// Retangulo e arte do cartao em foco, na tela virtual: o menu nasce ao lado e
// o cartaz volta por cima do veu, como na home. Le a tabela de celulas, a mesma
// do desenho, entao vale para toda exibicao.
static GfxRect celulaRect(int i, const CatItem *it, const char **arte) {
  float x0 = vtInicio();
  float cx = x0 + celX[i], cy = VT_TOPO + celY[i] - scrollY;
  if (lay == VT_LAY_LINHA) {
    *arte = it->backdrop[0] ? it->backdrop : it->poster;
    return vtTimelineArte(vtTimelineCard(cy));
  }
  { const char *pp = posterprov_card_addon(it->origem, it->imdb, it->tmdb, it->tipo, it->poster);
    *arte = pp[0] ? pp : it->backdrop; }
  return (GfxRect){ cx, cy, celW[i], celH[i] };
}

// 1 se o menu abriu. Titulo fora do catalogo e sem como entrar nele (id TMDB
// que nenhum item conhece) devolve 0 — o KEYUP vira toque, como sempre foi.
static int menuNoFocado(void) {
  CatItem it;
  GfxRect r;
  const char *arte = "";
  int idx;
  if (!viewItem(foco, &it)) return 0;
  if (!strncmp(it.imdb, "tmdb:", 5)) idx = ctxlista_indice(it.imdb, it.tmdb);
  else {
    idx = it.imdb[0] ? cat_indice_por_imdb(it.imdb) : -1;
    if (idx < 0 && it.imdb[0]) idx = cat_acrescentar(&it);
  }
  r = celulaRect(foco, &it, &arte);
  return ctxlista_abrir(idx, r, arte);
}

static int celulaAceitaMenu(void) {
  CatItem it;
  if (collection && tabFocus) return 0;
  if (lay == VT_LAY_VITRINE) return 0;   // a Vitrine trata o OK sozinha (vitEvento)
  if (nItens() < 1 || desc_vertudo_erro()) return 0;
  return viewItem(foco, &it);
}
static int vistoItem(const CatItem *it) { return cat_visto(it) || it->progresso >= 90; }
static void contarVistos(int n) {
  sagaVistos = 0;
  for (int i = 0; i < n && i < VT_MAX; i++) {
    CatItem it;
    celVisto[i] = viewItem(i, &it) && vistoItem(&it);
    sagaVistos += celVisto[i];
  }
}

// --- A TABELA DE CELULAS ------------------------------------------------------
static void cel(int i, float x, float y, float w, float h, int lin, int col) {
  celX[i] = x; celY[i] = y; celW[i] = w; celH[i] = h;
  celLin[i] = (short)lin; celCol[i] = (short)col;
  if (y + h > celFim) celFim = y + h;
}
// A ordem cronologica (Linha do tempo, Saga, Decadas). Insercao estavel: dois
// titulos do mesmo ano ficam na ordem que o catalogo deu.
static void ordenar(int n) {
  static int years[VT_MAX];
  int old = orderN > 0 && foco < orderN ? order[foco] : (orderN < 0 ? -1 : foco);
  for (int i = 0; i < n; i++) { CatItem it; order[i] = i;
    years[i] = desc_vertudo_item(i, &it) ? yearOf(&it) : 9999; }
  for (int i = 1; i < n; i++) { int v = order[i], j = i;
    while (j > 0 && years[order[j - 1]] > years[v]) { order[j] = order[j - 1]; j--; }
    order[j] = v; }
  orderN = n;
  if (old >= 0) for (int i = 0; i < n; i++) if (order[i] == old) { foco = i; break; }
}
static int decadaDe(int i) {
  CatItem it;
  int y = viewItem(i, &it) ? yearOf(&it) : 9999;
  return y == 9999 ? 0 : y / 10 * 10;
}
// MONTA a tabela da exibicao atual. Barato (uma passada pelos itens), e so
// roda quando o numero de itens ou a exibicao mudam — nunca por quadro.
static void montar(int n) {
  int i;
  if (n > VT_MAX) n = VT_MAX;
  celFim = 0.0f; decN = 0;
  if (ordenaPorAno() && n != orderN) ordenar(n);
  switch (lay) {
    case VT_LAY_LINHA:
      for (i = 0; i < n; i++) cel(i, 0, i * (VT_LINHA_H + VT_GAP_Y), 1158.0f, VT_LINHA_H, i, 0);
      break;
    case VT_LAY_RANKING: {
      // Quatro por linha: o numeral mora no vao a ESQUERDA do cartaz, como o
      // Top 10 da Dinamica, e o cartaz cobre a ponta direita dele.
      const float faixa = 100.0f, w = 218.0f, h = w * 1.5f, passo = VT_CW / 4.0f + 4.0f;
      for (i = 0; i < n; i++)
        cel(i, (i % 4) * passo + faixa, (i / 4) * (h + VT_GAP_Y), w, h, i / 4, i % 4);
      break; }
    case VT_LAY_SAGA: {
      // Tres capitulos deitados por linha, com a trilha de progresso por cima.
      const float w = 424.0f, h = 238.0f, passoY = VT_SAGA_TRILHA + h + 108.0f;
      for (i = 0; i < n; i++)
        cel(i, (i % 3) * (w + VT_GAP_X), VT_SAGA_TRILHA + (i / 3) * passoY, w, h, i / 3, i % 3);
      break; }
    case VT_LAY_DECADAS: {
      // Uma secao por decada: o cabecalho e depois seis cartazes por linha.
      const float w = (VT_CW - 5.0f * VT_GAP_X) / 6.0f, h = w * 1.5f, passoY = h + 56.0f;
      float y = 0.0f;
      int lin = -1, col = 0, dec = -1;
      for (i = 0; i < n; i++) {
        int d = decadaDe(i);
        if (i == 0 || d != dec) {
          if (i) y += passoY;              // fecha a ultima linha da secao anterior
          dec = d; col = 0; lin++;
          if (decN < VT_DEC_MAX) { decAno[decN] = d; decQtd[decN] = 0; decY[decN] = y; decN++; }
          y += VT_DEC_H;
        } else if (col == 6) { col = 0; lin++; y += passoY; }
        if (decN) decQtd[decN - 1]++;
        cel(i, col * (w + VT_GAP_X), y, w, h, lin, col);
        col++;
      }
      break; }
    case VT_LAY_MURAL: {
      // Blocos de cinco: um grande (2x2) e quatro pequenos, o grande trocando
      // de lado a cada bloco. Linha de cima: o grande e os dois pequenos do
      // alto; linha de baixo: os outros dois.
      const float bw = 640.0f, bh = 360.0f, sw = 312.0f, sh = 172.0f, g = VT_GAP_X;
      for (i = 0; i < n; i++) {
        int b = i / 5, k = i % 5;
        float y = b * (bh + g + 24.0f);
        if (!(b & 1)) {
          if (k == 0) cel(i, 0, y, bw, bh, 2 * b, 0);
          else { int q = k - 1; cel(i, bw + g + (q & 1) * (sw + g), y + (q >> 1) * (sh + g), sw, sh,
                                   2 * b + (q >> 1), 1 + (q & 1)); }
        } else {
          if (k == 2) cel(i, 2 * (sw + g), y, bw, bh, 2 * b, 2);
          else { int q = k < 2 ? k : k - 1;
                 cel(i, (q & 1) * (sw + g), y + (q >> 1) * (sh + g), sw, sh, 2 * b + (q >> 1), q & 1); }
        }
      }
      break; }
    default: {  // GRADE: preserva o recuo da rail fixa (#359).
      int cols = VT_COLS;
      for (i = 0; i < n; i++)
        cel(i, (i % cols) * (VT_CARD_W + VT_GAP_X), (i / cols) * (VT_CARD_H + VT_GAP_Y),
            VT_CARD_W, VT_CARD_H, i / cols, i % cols);
      break; }
  }
  celN = n; celLay = lay;
  if (foco >= n) foco = n ? n - 1 : 0;
  if (lay == VT_LAY_SAGA) { contarVistos(n); vistosEm = SDL_GetTicks(); }
}
// O vizinho na linha `lin` (acima ou abaixo) com o centro mais perto do
// centro do item em foco. As linhas sao contiguas em indice em toda exibicao.
static int vizinhoNaLinha(int n, int lin) {
  float cx = celX[foco] + celW[foco] * 0.5f, melhor = 1e9f;
  int achou = -1;
  for (int i = 0; i < n; i++) {
    float d;
    if (celLin[i] != lin) { if (celLin[i] > lin) break; continue; }
    d = fabsf(celX[i] + celW[i] * 0.5f - cx);
    if (d < melhor) { melhor = d; achou = i; }
  }
  return achou;
}
// O botao "Exibição" mora na faixa das abas: cursor = nAbas.
static int nAbas(void) {
  return collection && collection->nSources > 1 && lay != VT_LAY_VITRINE ? collection->nSources : 0;
}

// --- VITRINE ------------------------------------------------------------------
//
// Pedido de um usuario (Reddit): "can the collection view be like a home screen
// with a hero poster? Tabs are not the best view". A Vitrine e a Home em
// miniatura: o destaque do titulo em foco no alto (a mesma arte, logo, meta e
// sinopse do destaque da Home) e, abaixo, UMA FILEIRA POR ABA da colecao, com
// os cartazes das fileiras da Home. Sem abas, as fileiras saem do proprio
// catalogo: por tipo (filmes e series misturados) ou por decada.
//
// DADOS. O carregador da descoberta guarda UM catalogo por vez, entao as abas
// sao lidas em fila: abre a aba 0, espera a primeira pagina, guarda os
// primeiros VIT_ITENS titulos na fileira dela e passa a proxima. Nada de rede
// nova: e o mesmo caminho da grade, repetido. O que sobra e uma copia de ate
// VIT_LIN_MAX x VIT_ITENS CatItems (alocada so quando a Vitrine abre pela
// primeira vez), de modo que mudar de fileira nao refaz pedido nenhum.
//
// CUSTO NA TV: o mesmo da Home com o foco nas fileiras. UMA arte de tela cheia
// (a do destaque, na camada GFX_HERO_CHEIO) e os cartazes; nenhuma passada de
// blur. Durante os 250 ms da troca de destaque a arte anterior fica por baixo
// (duas camadas, so ali, como a Home faz).
#define VIT_LIN_MAX 12
#define VIT_ITENS   20   // teto por fileira (o espaco que o buffer reserva)
#define VIT_ITENS_FRACA 12   // TV com pouca RAM ou GPU fraca: buffer ~40% menor
// Itens por fileira EM VIGOR, fixado quando o buffer nasce (vitIniciar).
static int vitCap = VIT_ITENS;
enum { VIT_PEND = 0, VIT_CARREGANDO, VIT_PRONTA, VIT_VAZIA };
typedef struct { char nome[96]; int n, estado, semFonte; } VitLin;
static VitLin vitLin[VIT_LIN_MAX];
static CatItem *vitBuf;
static int vitN, vitMulti, vitCarga, vitFR, vitFC, vitMexeu, vitUnicoN = -1;
static int vitColLemb[VIT_LIN_MAX];
static Uint32 vitT0, vitNomeEm, vitReleEm;
static float vitScroll, vitScrollV, vitSx[VIT_LIN_MAX], vitSv[VIT_LIN_MAX];
// O destaque: o que esta na tela, o que saiu, e o que o foco pede.
static int vitHeroAtual = -1, vitHeroAnt = -1, vitHeroDes = -1;
static Uint32 vitHeroDesde;
static float vitHeroEntra = 1.0f;

static CatItem *vitItem(int r, int c) {
  if (!vitBuf || r < 0 || r >= vitN || c < 0 || c >= vitLin[r].n) return NULL;
  return &vitBuf[r * vitCap + c];
}
static CatItem *vitChave(int k) { return k < 0 ? NULL : vitItem(k / vitCap, k % vitCap); }
static int vitTemItens(void) {
  for (int r = 0; r < vitN; r++) if (vitLin[r].n > 0) return 1;
  return 0;
}
// A fileira com itens mais proxima de `r` na direcao `d` (+1 / -1); -1 se nao ha.
static int vitLinhaVizinha(int r, int d) {
  for (r += d; r >= 0 && r < vitN; r += d) if (vitLin[r].n > 0) return r;
  return -1;
}
static void vitFocoInicial(void) {
  if (vitLin[vitFR < vitN ? vitFR : 0].n > 0 && vitFR < vitN) return;
  for (int r = 0; r < vitN; r++) if (vitLin[r].n > 0) { vitFR = r; vitFC = 0; return; }
}

// O texto de cada aba: o mesmo da barra de abas (ver a nota em themeHeader).
static void rotulosAbas(char (*rot)[180]) {
  static char nomeAba[COL_SOURCE_MAX][128];
  int n = collection->nSources;
  if (n > COL_SOURCE_MAX) n = COL_SOURCE_MAX;
  for (int i = 0; i < n; i++) {
    const ColSource *s = &collection->sources[i];
    col_nome_fonte(s, desc_nome_catalogo(baseDaFonte(s), s->type, s->catId), nomeAba[i], sizeof nomeAba[i]);
    if (!strcasecmp(nomeAba[i], collection->title)) nomeAba[i][0] = 0;
  }
  for (int i = 0; i < n; i++) {
    const ColSource *s = &collection->sources[i];
    const char *tipo = i18n(!strcmp(s->type, "series") ? "Séries" : "Filmes");
    int repete = 0;
    for (int j = 0; j < n && nomeAba[i][0]; j++) if (j != i && !strcmp(nomeAba[i], nomeAba[j])) repete = 1;
    if (!nomeAba[i][0]) snprintf(rot[i], 180, "%s", tipo);
    else if (repete) snprintf(rot[i], 180, "%s · %s", nomeAba[i], tipo);
    else snprintf(rot[i], 180, "%s", nomeAba[i]);
  }
}
static void vitNomes(void) {
  static char rot[COL_SOURCE_MAX][180];
  if (!vitMulti || !collection) return;
  rotulosAbas(rot);
  for (int r = 0; r < vitN; r++) snprintf(vitLin[r].nome, sizeof vitLin[r].nome, "%s", rot[r]);
}

static void vitIniciar(void) {
  if (!vitBuf) {
    // RAM <= 1,5 GB (MemTotal/deviceMemory) ou GPU fraca: 12 itens por fileira.
    long memMb = 0; int mb = 0, fixo = 0, slots = 0;
    tex_orcamento_info(&mb, &memMb, &fixo, &slots);
    vitCap = (memMb > 0 && memMb <= 1536) || ptv_gpu_fraca_atual() ? VIT_ITENS_FRACA : VIT_ITENS;
    vitBuf = calloc((size_t)VIT_LIN_MAX * vitCap, sizeof(CatItem));
  }
  memset(vitLin, 0, sizeof vitLin);
  memset(vitColLemb, 0, sizeof vitColLemb);
  memset(vitSx, 0, sizeof vitSx); memset(vitSv, 0, sizeof vitSv);
  vitN = 0; vitCarga = 0; vitFR = vitFC = 0; vitMexeu = 0; vitUnicoN = -1;
  vitScroll = vitScrollV = 0.0f;
  vitHeroAtual = vitHeroAnt = vitHeroDes = -1; vitHeroEntra = 1.0f;
  vitNomeEm = 0; vitT0 = 0;
  if (!vitBuf || !collection) { lay = VT_LAY_GRADE; celLay = -1; carregarFonte(); return; }
  vitMulti = collection->nSources > 1;
  if (vitMulti) {
    vitN = collection->nSources < VIT_LIN_MAX ? collection->nSources : VIT_LIN_MAX;
    vitNomes();
  } else carregarFonte();
}

// Guarda a primeira pagina do catalogo aberto na fileira `r`.
static void vitGuardar(int r) {
  int n = nItens(), k = 0;
  if (n > vitCap) n = vitCap;
  for (int i = 0; i < n; i++)
    if (desc_vertudo_item(i, &vitBuf[r * vitCap + k])) k++;
  vitLin[r].n = k;
  vitLin[r].estado = k ? VIT_PRONTA : VIT_VAZIA;
  vitLin[r].semFonte = semFonte;
}

// SEM ABAS: as fileiras saem dos titulos lidos. Tipos misturados = uma fileira
// por tipo; senao uma por decada (a mais nova primeiro, "sem data" por ultimo).
static void vitAgrupar(int n) {
  static short chave[VT_MAX];
  static CatItem tmp;
  int chaves[VIT_LIN_MAX + 1], nc = 0, filmes = 0, series = 0;
  if (n > VT_MAX) n = VT_MAX;
  for (int i = 0; i < n; i++)
    if (desc_vertudo_item(i, &tmp)) { if (!strcmp(tmp.tipo, "series")) series = 1; else filmes = 1; }
  for (int i = 0; i < n; i++) {
    int k;
    chave[i] = -1;
    if (!desc_vertudo_item(i, &tmp)) continue;
    if (filmes && series) k = !strcmp(tmp.tipo, "series") ? 1 : 0;
    else { int a = nv_ano_meta(tmp.meta); k = a ? a / 10 * 10 : 0; }
    chave[i] = (short)k;
    { int j; for (j = 0; j < nc && chaves[j] != k; j++) {}
      if (j == nc && nc < VIT_LIN_MAX) chaves[nc++] = k; }
  }
  // Ordem das fileiras: tipos filmes, series; decadas da mais nova a mais velha.
  for (int i = 1; i < nc; i++) {
    int v = chaves[i], j = i;
    int mistos = filmes && series;
    for (; j > 0; j--) {
      int a = chaves[j - 1], antes = mistos ? a > v : (v == 0 ? 0 : (a == 0 ? 1 : a < v));
      if (!antes) break;
      chaves[j] = chaves[j - 1];
    }
    chaves[j] = v;
  }
  memset(vitLin, 0, sizeof vitLin);
  vitN = nc;
  for (int r = 0; r < nc; r++) {
    VitLin *l = &vitLin[r];
    if (filmes && series) snprintf(l->nome, sizeof l->nome, "%s", i18n(chaves[r] ? "Séries" : "Filmes"));
    else if (chaves[r]) snprintf(l->nome, sizeof l->nome, i18n("Anos %d"), chaves[r]);
    else snprintf(l->nome, sizeof l->nome, "%s", i18n("Sem data"));
    for (int i = 0; i < n && l->n < vitCap; i++)
      if (chave[i] == chaves[r] && desc_vertudo_item(i, &vitBuf[r * vitCap + l->n])) l->n++;
    l->estado = VIT_PRONTA;
  }
  if (vitFR >= vitN) vitFR = vitN ? vitN - 1 : 0;
  if (vitN && vitFC >= vitLin[vitFR].n) vitFC = vitLin[vitFR].n ? vitLin[vitFR].n - 1 : 0;
}

// Um passo da carga, por quadro: a fila de abas, as ultimas paginas do catalogo
// unico e a releitura das fontes cujo addon ainda nao tinha endereco.
static void vitCarregar(Uint32 agora) {
  if (!vitBuf) return;
  if (!vitMulti) {
    int n = nItens();
    if (n != vitUnicoN) { vitUnicoN = n; if (n > 0) vitAgrupar(n); }
    // O catalogo traz 100 por pagina: pede mais ate haver o que agrupar.
    if (n > 0 && n < 300 && !desc_vertudo_carregando() && !desc_vertudo_fim() &&
        !desc_vertudo_erro()) desc_vertudo_mais();
    if (semFonte && baseDaFonte(&collection->sources[source])[0]) vitIniciar();
    return;
  }
  if (vitCarga < vitN) {
    VitLin *l = &vitLin[vitCarga];
    if (l->estado == VIT_PEND) {
      source = vitCarga; carregarFonte();
      l->estado = VIT_CARREGANDO; vitT0 = agora ? agora : 1u;
    } else if (l->estado == VIT_CARREGANDO && !desc_vertudo_carregando() &&
               (nItens() > 0 || desc_vertudo_fim() || desc_vertudo_erro() || semFonte ||
                (Sint32)(agora - vitT0) > 2500)) {
      vitGuardar(vitCarga);
      vitCarga++;
    }
  }
  // Fonte sem endereco que ganhou um (a sonda do manifesto respondeu): reabre.
  if ((Sint32)(agora - vitReleEm) > 1500) {
    vitReleEm = agora;
    vitNomes();
    for (int r = 0; r < vitN && vitCarga >= vitN; r++)
      if (vitLin[r].estado == VIT_VAZIA && vitLin[r].semFonte &&
          baseDaFonte(&collection->sources[r])[0]) { vitLin[r].estado = VIT_PEND; vitCarga = r; break; }
  }
}

// ABRIR UM TITULO (a grade e a Vitrine). O item NAO esta no catalogo global —
// veio de uma pagina que so esta tela leu. Entra por cat_acrescentar para que a
// tela de titulo possa abri-lo por indice, que e como todo o app trabalha.
static void abrirItem(const CatItem *src) {
  CatItem it = *src;
  // Item de fonte TMDB (issue #44): o id e "tmdb:<n>", nao imdb. Quem resolve e
  // o caminho sob-demanda ja usado pela filmografia de elenco: external_ids ->
  // imdb -> meta do Cinemeta -> cat_acrescentar, e o desc_titulo_pronto do loop
  // principal abre o detalhe.
  if (!strncmp(it.imdb, "tmdb:", 5)) {
    desc_pedir_titulo_tmdb(atol(it.imdb + 5), !strcmp(it.tipo, "series") ? "tv" : "movie");
  } else {
    int idx = it.imdb[0] ? cat_indice_por_imdb(it.imdb) : -1;
    if (idx < 0) idx = cat_acrescentar(&it);
    if (idx >= 0) pedAbrir = idx;
  }
}

static int primeiraFonte(void) {
  for (int i = 0; i < collection->nSources; i++)
    if (collection->sources[i].prov[0] || baseDaFonte(&collection->sources[i])[0]) return i;
  return 0;
}
// A PROXIMA EXIBICAO, e ela fica gravada para esta pasta (o botao "Exibição").
static void alternarExibicao(void) {
  int antes = lay, prox = lay % (VT_LAY_N - 1) + 1;
  layEscolha = prox; gravarEscolha(collection, prox);
  resolverLayout(); foco = 0; scrollY = velY = 0; armarOnda();
  if (lay != antes && (lay == VT_LAY_VITRINE || antes == VT_LAY_VITRINE)) {
    // A Vitrine nao tem aba aberta e as outras exibicoes precisam de uma: a
    // fonte volta para a primeira que tem endereco, e o cursor para o botao.
    source = primeiraFonte(); openSource();
  }
  tabCursor = nAbas();
}

static void vitEvento(int k) {
  int enter = k == SDLK_RETURN || k == SDLK_KP_ENTER;
  VitLin *l;
  vitMexeu = 1;
  if (tabFocus) {   // o botao "Exibição", acima do destaque
    if (k == SDLK_LEFT || (k == SDLK_UP && ajustes_menu_topo())) pedMenu = 1;
    else if (k == SDLK_DOWN && vitTemItens()) tabFocus = 0;
    else if (enter) alternarExibicao();
    return;
  }
  if (vitFR >= vitN) return;
  l = &vitLin[vitFR];
  if (k == SDLK_LEFT) { if (vitFC > 0) vitFC--; else pedMenu = 1; }
  else if (k == SDLK_RIGHT) { if (vitFC + 1 < l->n) vitFC++; }
  else if (k == SDLK_DOWN) {
    int r = vitLinhaVizinha(vitFR, 1);
    if (r >= 0) { vitColLemb[vitFR] = vitFC; vitFR = r; vitFC = vitColLemb[r]; }
  } else if (k == SDLK_UP) {
    int r = vitLinhaVizinha(vitFR, -1);
    if (r >= 0) { vitColLemb[vitFR] = vitFC; vitFR = r; vitFC = vitColLemb[r]; }
    else tabFocus = 1, tabCursor = nAbas();
  } else if (enter) {
    const CatItem *it = vitItem(vitFR, vitFC);
    if (it) abrirItem(it);
  }
  if (vitFR < vitN && vitFC >= vitLin[vitFR].n) vitFC = vitLin[vitFR].n ? vitLin[vitFR].n - 1 : 0;
}

// Medidas das fileiras: as da Home (home_vitrine_medida). O topo da primeira e
// o da Home Moderna; a fileira em foco sobe para ele e a seguinte espia embaixo.
#define VIT_TITULO_H 46.0f
static float vitPasso(void) {
  float w, h, p, rot;
  home_vitrine_medida(&w, &h, &p, &rot);
  return VIT_TITULO_H + h + rot + 40.0f;
}

static void vitAtualizar(float dt, Uint32 agora) {
  float w, h, passo, rot, alvo;
  vitCarregar(agora);
  if (!vitMexeu) vitFocoInicial();
  // Destaque: o item sob o foco; so troca depois de 140 ms parado e com a arte
  // nova decodificada (ou 700 ms de espera), e a arte nova entra em 250 ms.
  { int des = !vitItem(vitFR, vitFC) ? -1 : vitFR * vitCap + vitFC;
    CatItem *it = vitChave(des);
    if (des != vitHeroDes) { vitHeroDes = des; vitHeroDesde = agora; }
    if (it && des != vitHeroAtual) {
      Uint32 parado = agora - vitHeroDesde;
      if (vitHeroAtual < 0 || (parado >= 140 && (home_vitrine_arte_pronta(it) || parado >= 700))) {
        vitHeroAnt = vitHeroAtual; vitHeroAtual = des;
        vitHeroEntra = vitHeroAnt < 0 ? 1.0f : 0.0f;
      }
    } else if (!it) { vitHeroAtual = vitHeroAnt = -1; vitHeroEntra = 1.0f; }
    if (vitHeroEntra < 1.0f) {
      vitHeroEntra += dt / 0.25f;
      if (vitHeroEntra >= 1.0f) { vitHeroEntra = 1.0f; vitHeroAnt = -1; }
    } }
  home_vitrine_medida(&w, &h, &passo, &rot);
  alvo = (float)vitFR * vitPasso();
  vitScroll = anim_mola2(&vitScrollV, vitScroll, alvo, dt, NV_MOLA2_SCROLL);
  for (int r = 0; r < vitN; r++) {
    // A fileira em foco anda ate o cartaz caber com um vizinho a vista.
    if (r == vitFR)
      vitSx[r] = anim_mola2(&vitSv[r], vitSx[r], vitFC > 1 ? (vitFC - 1) * passo : 0.0f,
                            dt, NV_MOLA2_SCROLL);
  }
}

#ifdef NV_TOUCH_UI
static float toqueVertudoMax(void) {
  return fmaxf(0.0f, VT_TOPO + celFim + VT_GAP_Y - NV_TELA_H + 120.0f);
}
static int toqueVertudoRolar(const PonteiroRolagem *e) {
  float escala = vtEscala();
  if (e->fase == PONT_ROL_INICIO) {
    toqueNasAbas = 0;
    if (toquerol_evento(&toqueAbas, e)) { toqueNasAbas = 1; ctxhold_cancelar(&hold); return 1; }
    float x = e->x / escala, y = e->y / escala;
    if (!e->eixoY || x < vtInicio() || x >= (vtTelefone() ? vtFim() : VT_PAN_X) || y < VT_TOPO - 12.0f || y >= NV_TELA_H || nItens() < 1) return 0;
    toqueLivre = 1; velY = 0.0f; ctxhold_cancelar(&hold);
    return 1;
  }
  if (toqueNasAbas) return toquerol_evento(&toqueAbas, e);
  if (e->fase == PONT_ROL_MOVER || e->fase == PONT_ROL_INERCIA) {
    float antes = scrollY;
    scrollY = anim_clamp(scrollY - e->delta / escala, 0.0f, toqueVertudoMax());
    if (scrollY + NV_TELA_H >= VT_TOPO + celFim - 2.0f * (VT_CARD_H + VT_GAP_Y)) desc_vertudo_mais();
    return fabsf(scrollY - antes) > 0.001f;
  }
  return 1;
}
static void toqueVertudoRetomarFoco(void) {
  if (toqueLivre && nItens() > 0) {
    float alvo = scrollY + NV_TELA_H * 0.30f - VT_TOPO, distancia = 1e9f;
    int coluna = celCol[foco];
    for (int i = 0; i < celN; i++) {
      float d = fabsf(celY[i] - alvo);
      if (d < distancia || (d == distancia && celCol[i] == coluna)) {
        distancia = d; foco = i;
      }
    }
    tabFocus = 0;
  }
  toqueLivre = 0;
}
#endif

void vertudo_evento(const SDL_Event *e) {
  int n = nItens(), k;
  if (!aberta) return;
#ifdef NV_TOUCH_UI
  if (toquerol_navegacao(e)) toquerol_limpar(&toqueAbas);
  if (e->type == SDL_KEYDOWN) toqueVertudoRetomarFoco();
#endif
  switch (ctxhold_evento(&hold, e, celulaAceitaMenu())) {
    case CTXH_LONGO: if (!menuNoFocado()) abrirFocado(); return;
    case CTXH_CONSUMIDO: return;
    case CTXH_TOQUE: abrirFocado(); return;
    default: break;
  }
  if (e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      e->key.keysym.scancode == NV_SCANCODE_BACK) { aberta = 0; return; }
  if (n > VT_MAX) n = VT_MAX;
  if (lay == VT_LAY_VITRINE) { vitEvento(k); return; }
  if(collection&&tabFocus) {
    int na = nAbas();
    if(k==SDLK_LEFT||(k==SDLK_UP&&ajustes_menu_topo())){ if(k==SDLK_LEFT&&tabCursor>0)tabCursor--; else pedMenu=1; }
    if(k==SDLK_RIGHT&&tabCursor<na)tabCursor++;
    if(k==SDLK_RETURN||k==SDLK_KP_ENTER) {
      if(tabCursor>=na) {
        // O cursor continua no botao: da para passar por todas as exibicoes
        // de uma vez so com OK.
        alternarExibicao();
      } else {source=tabCursor;openSource();tabFocus=0;}
    }
    if(k==SDLK_DOWN&&n>0)tabFocus=0;
    return;
  }
  if (celLay != lay || celN != n) montar(n);
  if(k==SDLK_UP&&collection&&(n<1||celLin[foco]==0)){tabFocus=1;tabCursor=nAbas()?source:0;return;}
  if((k==SDLK_RETURN||k==SDLK_KP_ENTER)&&desc_vertudo_erro()){desc_vertudo_mais();return;}
  // ESQUERDA no primeiro item de uma linha: a barra lateral (ver pedMenu).
  if (k == SDLK_LEFT && (n < 1 || foco == 0 || celLin[foco - 1] != celLin[foco])) { pedMenu = 1; return; }
  if (n < 1) return;
  if (k == SDLK_RIGHT && foco + 1 < n) foco++;
  else if (k == SDLK_LEFT) foco--;
  else if (k == SDLK_DOWN) { int v = vizinhoNaLinha(n, celLin[foco] + 1);
                             foco = v >= 0 ? v : n - 1; }
  else if (k == SDLK_UP) { int v = vizinhoNaLinha(n, celLin[foco] - 1);
                           if (v >= 0) foco = v; else if (ajustes_menu_topo()) pedMenu = 1; }
  else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) abrirFocado();
  // Chegando perto do fim, pede a proxima pagina. Antes de o dono ver o vazio,
  // e nao quando ele ja esta olhando para ele.
  if (foco >= n - 12) desc_vertudo_mais();
}

void vertudo_atualizar(float dt, Uint32 agora) {
  float alvo, maxY;
  int n = nItens();
  anim = anim_mola(anim, aberta ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  if (!aberta) { ctxhold_cancelar(&hold); return; }
  // SEGUROU OK NUM CARTAO ATE O LIMIAR: o menu do cartaz, com o dedo ainda no
  // botao (ctxlista.h).
  if (ctxhold_passo(&hold, agora, 1) && menuNoFocado()) ctxhold_cancelar(&hold);
  if (n > VT_MAX) n = VT_MAX;
  // A SONDA PODE CHEGAR DEPOIS DA TELA. Enquanto a fonte nao tem endereco,
  // reconferir por quadro custa uma varredura de poucas strings (addons_n e
  // dezenas, nao milhares) e SO acontece no estado de erro — assim a colecao se
  // preenche sozinha quando o manifesto responde, em vez de exigir sair e
  // entrar de novo.
  if (semFonte && collection && lay != VT_LAY_VITRINE) {
    const ColSource *s = &collection->sources[source];
    if (baseDaFonte(s)[0]) openSource();
  }
  for (int i = 0; i < COL_SOURCE_MAX; i++) {
    float alvoTab = collection && tabFocus && i == tabCursor && i < nAbas() ? 1.0f : 0.0f;
    tabAnim[i] = anim_mola(tabAnim[i], alvoTab, dt,
                           alvoTab > tabAnim[i] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  }
  { float alvoP = collection && tabFocus && tabCursor >= nAbas() ? 1.0f : 0.0f;
    pillAnim = anim_mola(pillAnim, alvoP, dt, alvoP > pillAnim ? NV_MOLA_FOCO : NV_MOLA_DESFOCO); }
  if (lay == VT_LAY_VITRINE) { vitAtualizar(dt, agora); return; }
  if (celLay != lay || celN != n) montar(n);
  else if (lay == VT_LAY_SAGA && n <= 200 && (Sint32)(agora - vistosEm) > 1500)
    { contarVistos(n); vistosEm = agora; }
  // Mira a linha focada a 30% da altura util, como o resto do app faz.
  alvo = n ? VT_TOPO + celY[foco] - NV_TELA_H * 0.30f : 0.0f;
  // Decadas: o primeiro cartaz de uma secao leva o cabecalho junto.
  // Saga: a trilha de capitulos fica acima da linha e entra junto.
  if (lay == VT_LAY_SAGA) alvo -= VT_SAGA_TRILHA;
  if (lay == VT_LAY_DECADAS && n)
    for (int k = 0; k < decN; k++) if (decY[k] + VT_DEC_H == celY[foco]) { alvo -= VT_DEC_H; break; }
  if(tabFocus)alvo=0;
  maxY = VT_TOPO + celFim + VT_GAP_Y - NV_TELA_H + 120.0f;
  if (maxY < 0.0f) maxY = 0.0f;
  if (alvo < 0.0f) alvo = 0.0f;
  if (alvo > maxY) alvo = maxY;
#ifdef NV_TOUCH_UI
  if (toqueLivre) scrollY = anim_clamp(scrollY, 0.0f, maxY);
  else
#endif
    scrollY = anim_mola2(&velY, scrollY, alvo, dt, NV_MOLA2_SCROLL);
}

// PAINEL DA DIREITA: o que a grade sozinha nao diz — sinopse, generos, nota.
//
// Sem ele o dono ve 50 cartazes e nenhuma informacao; era o que faltava para a
// tela deixar de ser so uma parede de imagens.
static void painel(float a) {
  CatItem it;
  float y = VT_PAN_Y;
  if (!viewItem(foco, &it)) return;

  // O contexto lateral usa o cartaz, nunca repete a cena da timeline.
  // Mantém a geometria 2:3 mesmo sem arte para não deslocar os metadados.
  { GfxRect r = { VT_PAN_X, y, VT_PAN_ART_W, VT_PAN_ART_H };
    const char *arte = it.poster[0] ? it.poster : it.backdrop;
    float raio = 12.0f / VT_PAN_ART_W;
    GLuint t = arte[0] ? tex_obter_larg(arte, VT_PAN_ART_W) : 0;
    gfx_cor(r, raio, 1, 1, 1, 0.05f * a);
    if (t) {
      gfx_tex_aspect_atual = tex_aspecto(arte);
      gfx_rect(r, t, GFX_CARD, 0, 0, 0, raio, 0, 0, 0, a);
      gfx_tex_aspect_atual = 0.0f;
    } else {
      TxtLinha falta = txt_linha_corta(TXT_HERO_META, "Sem pôster", 185, 191, 204, 255, r.w-24);
      txt_desenhar_alpha(falta, r.x+(r.w-falta.w)*.5f, r.y+(r.h-falta.h)*.5f, a);
    } }
  y += VT_PAN_ART_H + 18.0f;
  float badgeW=badges_desenhar(badges_provedor(it.provNome),VT_PAN_X,y,VT_PAN_W,28,a);
  if(badgeW>0)y+=40;

  // LOGO no lugar do titulo quando existe (max 264x82 no web); o nome escrito
  // com a fonte da interface so quando nao ha logo.
  { GLuint tl = it.logo[0] ? tex_obter_logo_larg(it.logo, 264.0f) : 0;
    float ap = it.logo[0] ? tex_aspecto(it.logo) : 0.0f;
    if (tl && ap > 0.0f) {
      float wL = 264.0f, hL = wL / ap;
      if (hL > 82.0f) { hL = 82.0f; wL = hL * ap; }
      { GfxRect rl = { VT_PAN_X, y, wL, hL };
        GfxModo m = tex_marca_escura(it.logo) ? GFX_MARCA : GFX_TEXTO;
        gfx_tex_aspect_atual = 0.0f;
        gfx_rect(rl, tl, m, 0, 0, 0, 0.0f, 1, 1, 1, a); }
      y += hL + 4.0f;
    } else {
      TxtLinha t = txt_linha_corta(TXT_HEADLINE, it.titulo, 245, 245, 245, 255,
                                   VT_PAN_W);
      txt_desenhar_alpha(t, VT_PAN_X, y, a);
      y += t.h + 6.0f;
    } }

  if (it.genero[0]) {
    TxtLinha t = txt_linha_corta(TXT_CAPTION, it.genero, 220, 231, 244, 255,
                                 VT_PAN_W);
    txt_desenhar_alpha(t, VT_PAN_X, y, a * 0.72f);
    y += t.h + 6.0f;
  }
  // Pastilha da nota, no amarelo do IMDb que o web usa (245,197,24).
  int imdbRating = imdbnota_obter(it.imdb, it.nota, !strcmp(it.tipo,"series"));
  if (imdbRating > 0) {
    char n[16];
    snprintf(n, sizeof n, "%.1f", imdbRating / 10.0f); idioma_decimal_texto(n, ajustes_idioma());
    { TxtLinha t = txt_linha(TXT_CAPTION, n, 23, 19, 10, 255);
      GfxRect r = { VT_PAN_X, y + 8.0f, t.w + 26.0f, t.h + 8.0f };
      gfx_cor(r, 8.0f / (t.h + 8.0f), 0.961f, 0.773f, 0.094f, 0.92f * a);
      txt_desenhar_alpha(t, VT_PAN_X + 13.0f, y + 12.0f, a);
      y += r.h + 14.0f; }
  }
  if (it.meta[0]) {
    TxtLinha t = txt_linha_corta(TXT_DET_META2, it.meta, 240, 240, 240, 255,
                                 VT_PAN_W);
    txt_desenhar_alpha(t, VT_PAN_X, y, a * 0.92f);
    y += t.h + 12.0f;
  }
  if (it.sinopse[0]) {
    // Ate onde couber sem passar da base util (o web corta em
    // max-height: 100% - 210).
    int linhas = (int)((NV_TELA_H - 48.0f - y) / 34.0f);
    if (linhas > 8) linhas = 8;
    if (linhas > 0)
      txt_bloco(TXT_DET_META2, it.sinopse, 236, 236, 236,
                VT_PAN_X, y, VT_PAN_W, 34.0f, a * 0.86f, linhas);
  }
}

static const char *retratoLocal(const ColFolder *folder) {
  static char caminho[700];
  if (!folder || !folder->frameDir[0]) return "";
  snprintf(caminho, sizeof caminho, "%s/portrait.png", folder->frameDir);
  if (access(caminho, R_OK) == 0) return caminho;
  snprintf(caminho, sizeof caminho, "%s/portrait.jpg", folder->frameDir);
  return access(caminho, R_OK) == 0 ? caminho : "";
}

// Uma única arte full-width, dissolvendo na mesma cor do corpo. Directors usa
// o retrato vertical local quando o pacote ja o tem; o hero horizontal dessa
// colecao e um placeholder neutro e so acrescenta uma camada sem informacao.
// Usa os shaders e o cache existentes, sem blur ou novas texturas por frame.
// FUNDO DA COLECAO: sempre Frost ou a arte borrada da pasta, nunca a arte crua
// ("em colecoes deixa o fundo sempre frost ou blur", 04/10/2026). Ajustes >
// Fundo = Frost fica Frost; senao a arte borrada (fundo.c), e sem a arte
// ainda cai no Frost. A arte de cabecalho (editorial, retrato) vem POR CIMA.
static void fundoColecao(float a) {
  GfxRect tela={0,0,NV_TELA_W,NV_TELA_H};
  GLuint t=0;
  const char *art=collection?(collection->editorial&&collection->detailHero[0]
                              ?collection->detailHero:col_banner(collection)):"";
  int modo=fundo_modo()==FUNDO_FROST?FUNDO_FROST:FUNDO_BORRADA;
  if(art&&art[0])t=tex_obter_hero(art);   /* a Borrada desfoca esta textura */
  if(modo==FUNDO_BORRADA&&!t)modo=FUNDO_FROST;
  fundo_desenhar_modo(modo,tela,0.0f,art,a);
}
static void themeBackground(float a) {
  fundoColecao(a);
  if(collection) {
    if(collection->editorial) {
      GLuint art=tex_obter_hero(collection->detailHero);
      /* The content starts at 332; the separate detail illustration ends at 320. */
      float telaW = vtTelefone() ? NV_TELA_W : 1920.0f;
      GfxRect header={0,0,telaW,320};
      if(collection->editorial==2&&tex_aspecto(collection->detailHero)>0) {
        float aspect=tex_aspecto(collection->detailHero);
        header.w=fminf(telaW,header.h*aspect);header.h=header.w/aspect;
        header.x=telaW-header.w;
      }
      if(art)gfx_rect(header,art,collection->editorial==2?GFX_EDITORIAL:GFX_TEXTO,0,0,0,0,1,1,1,a);
      return;
    }
    if(grupo("Directors")) {
      const char *foto=retratoLocal(collection);
      if(!foto[0]) {
        diretor_pedir(collection->title);
        foto=diretor_foto(collection->title);
      }
      GLuint tp=foto[0]?tex_obter_larg(foto,260.0f):0;
      if(tp) {
        // O painel do item selecionado começa em VT_PAN_Y. O retrato ocupa
        // apenas o cabeçalho e termina antes dele, sem atravessar pôster ou
        // sinopse como uma segunda camada.
        GfxRect rp={1660,18,260,300};
        if (vtTelefone()) rp.x = NV_TELA_W - 48.0f - rp.w;
        if (vtTimelineRetrato()) rp = (GfxRect){NV_TELA_W - 204.0f,18,156,180};
        gfx_tex_aspect_atual=tex_aspecto(foto);
        gfx_rect(rp,tp,GFX_RETRATO,
                 0,0,0,0,0,0,0,a*.88f);
        gfx_tex_aspect_atual=0;
      }
    } else {
      const char *art=col_banner(collection);
      (void)art;   /* o banner agora e o fundo borrado (fundoColecao) */
    }
  }
}

// PONTEIRO (#99). As mesmas variaveis das setas: a aba sob o cursor ganha o
// cursor das abas, o cartaz ganha o foco da grade; o OK do clique segue o
// caminho de sempre (aba abre a fonte, cartaz abre o titulo ou, segurado, o
// menu do cartaz).
static void ponteiroAba(int i, int b) {
  (void)b;
  if (!collection || i < 0 || i >= collection->nSources) return;
#ifdef NV_TOUCH_UI
  toqueLivre = 0;
  toquerol_limpar(&toqueAbas);
#endif
  tabFocus = 1; tabCursor = i;
}
static void ponteiroCartaz(int i, int b) {
  (void)b;
  if (i < 0 || i >= nItens()) return;
#ifdef NV_TOUCH_UI
  toqueLivre = 0;
#endif
  tabFocus = 0; foco = i;
  if (foco >= nItens() - VT_COLS * 2) desc_vertudo_mais();
}

static void themeHeader(float a,float x0) {
  float r,g,b;corColecao(&r,&g,&b);
  TxtLinha eyebrow=vtTelefone()
    ?txt_linha_corta(TXT_HERO_META,rotuloGrupo(),197,202,211,255,vtCabecalhoLargura(x0,960))
    :txt_linha(TXT_HERO_META,rotuloGrupo(),197,202,211,255);
  txt_desenhar_alpha(eyebrow,x0,40,a);
  int ehDiretor=collection&&!strcasecmp(collection->group,"Directors");
  // O wordmark de uma coleção de diretores pode conter cabeça ou lettering
  // composto. No cabeçalho da filmografia, o nome textual e o retrato limpo
  // deixam a identidade legível sem duplicar a mesma informação visual.
  GLuint logo=!ehDiretor&&collection&&!collection->editorial&&collection->logo[0]
             ?tex_obter_logo_larg(collection->logo,560):0;
  float aspect=logo?tex_aspecto(collection->logo):0;
  if(logo&&aspect>0) {
    // Wordmark oficial, grande o bastante para leitura a distancia. O PNG
    // transparente e importado em ate 800px, portanto 560px nao interpola para
    // cima nem perde a silhueta original da marca.
    float w=vtCabecalhoLargura(x0,560),h=w/aspect;if(h>108){h=108;w=h*aspect;}
    gfx_rect((GfxRect){x0,83,w,h},logo,tex_marca_escura(collection->logo)?GFX_MARCA:GFX_TEXTO,0,0,0,0,.96f,.97f,.98f,a);
  } else {TxtLinha title=txt_linha_corta(TXT_TITULO1,titulo,242,243,247,255,vtCabecalhoLargura(x0,940));txt_desenhar_alpha(title,x0,80,a);}
  char caption[180];int n=nItens();
  if(semFonte) {
    const char *pv=collection&&source<collection->nSources?collection->sources[source].prov:"";
    snprintf(caption,sizeof caption,"%s",
      !strcmp(pv,"tmdb")?i18n("O TMDB não está configurado nesta TV."):
      !strcmp(pv,"trakt")?i18n("O Trakt não está configurado nesta TV."):
      i18n("O addon desta coleção não está instalado nesta TV."));
  }
  else if(desc_vertudo_erro())snprintf(caption,sizeof caption,"Não foi possível carregar. OK para tentar novamente.");
  else if(!n)snprintf(caption,sizeof caption,"%s",desc_vertudo_carregando()?"Carregando títulos…":"Nenhum título nesta lista.");
  else snprintf(caption,sizeof caption,i18n("%d títulos%s  ·  %s"),n,desc_vertudo_fim()?"":i18n(" carregados"),i18n(legendaGrupo()));
  // SAGA: quanto da franquia ja foi visto, na mesma linha.
  if(lay==VT_LAY_SAGA&&n>0&&!semFonte&&!desc_vertudo_erro()) {
    char vis[64]; size_t k=strlen(caption);
    snprintf(vis,sizeof vis,i18n("%d de %d assistidos"),sagaVistos,n>VT_MAX?VT_MAX:n);
    snprintf(caption+k,sizeof caption-k,"  ·  %s",vis);
  }
  TxtLinha sub=txt_linha_corta(TXT_DET_META2,caption,196,202,213,255,vtTelefone()?fminf(960.0f,NV_TELA_W-x0-48.0f):960.0f);txt_desenhar_alpha(sub,x0,192,a);
  if(collection&&collection->nSources>1) {
    // ABAS NO PADRAO DO APP (19/09/2026, "deixar mais parecido com o restante").
    // Antes: pilula de 304x58 fixa com anel, sublinhado e escala no foco —
    // tres sinais que o resto do app ja abandonou (ver NV_COR_FOCO em
    // layout.h e as abas do painel de Salvos). Agora e a mesma gramatica das
    // abas de Salvos: largura pelo texto, 52 de altura, FOCO = preenchimento
    // na cor de realce graduada, com tinta contrastante e sem anel; ABERTA sem
    // foco conserva uma marca mais baixa da mesma familia; as outras ficam
    // quase transparentes. `tabAnim` cruza o repouso e o foco em vez de
    // escalar.
    // A faixa termina antes da coluna do painel: ali mora o botao "Exibição".
    const float H=52.0f,PAD=22.0f,GAP=12.0f,W=vtTelefone()?vtFim()-x0:VT_PAN_X-x0-28;
    float margem = vtTelefone() ? 0.0f : 6.0f;
    float textoW = vtTelefone() ? fminf(420.0f, W-PAD*2) : 420.0f;
    float larg[COL_SOURCE_MAX],pos[COL_SOURCE_MAX],px=0;
    static char rot[COL_SOURCE_MAX][180];
    int nAbas=collection->nSources;
    // NOME DA ABA (01/10, pasta Netflix mostrava "streaming_netflix_movies ·
    // Movies"): o nome legivel da fonte (titulo da conta, senao o do
    // manifesto — #76) e NUNCA o id cru. Sem nome, ou com o mesmo nome da
    // pasta ("Netflix" na pasta Netflix), so o tipo: "Filmes" / "Séries". O
    // tipo so acompanha o nome quando duas abas teriam o mesmo texto.
    static char nomeAba[COL_SOURCE_MAX][128];
    for(int i=0;i<nAbas;i++) {
      const ColSource *s=&collection->sources[i];
      col_nome_fonte(s,desc_nome_catalogo(baseDaFonte(s),s->type,s->catId),nomeAba[i],sizeof nomeAba[i]);
      if(!strcasecmp(nomeAba[i],collection->title))nomeAba[i][0]=0;
    }
    for(int i=0;i<nAbas;i++) {
      const ColSource *s=&collection->sources[i];
      const char *tipo=i18n(!strcmp(s->type,"series")?"Séries":"Filmes");
      int repete=0;
      for(int j=0;j<nAbas&&nomeAba[i][0];j++) if(j!=i&&!strcmp(nomeAba[i],nomeAba[j]))repete=1;
      if(!nomeAba[i][0])snprintf(rot[i],sizeof rot[i],"%s",tipo);
      else if(repete)snprintf(rot[i],sizeof rot[i],"%s · %s",nomeAba[i],tipo);
      else snprintf(rot[i],sizeof rot[i],"%s",nomeAba[i]);
      // Medida com a cor de repouso; a cor certa e reaplicada no desenho (o
      // cache de linhas guarda as duas).
      larg[i]=txt_linha_corta(TXT_HERO_META,rot[i],176,176,176,255,textoW).w+PAD*2;pos[i]=px;px+=larg[i]+GAP;
    }
    // ROLAGEM PELO CURSOR, nao por indice: as abas tem larguras diferentes,
    // entao "seis por vez" nao existe mais. A aba com o cursor (ou a aberta,
    // quando o cursor esta na grade) entra inteira; a rolagem so anda o que
    // precisa, e o resto da faixa fica onde estava.
    int alvo=tabFocus?tabCursor:source;
    float ini=pos[alvo],fim=pos[alvo]+larg[alvo];
#ifdef NV_TOUCH_UI
    if (!toqueAbas.livre)
#endif
    { if(fim-tabRol>W)tabRol=fim-W; if(ini-tabRol<0)tabRol=ini; }
    if(tabRol<0)tabRol=0;
#ifdef NV_TOUCH_UI
    tabRol = anim_clamp(tabRol, 0.0f, fmaxf(0.0f, px-GAP-W));
    toquerol_vincular(&toqueAbas, (GfxRect){x0-margem,244,W+2*margem,H+12}, gfx_escala(),
                     0.0f, fmaxf(0.0f, px-GAP-W), 0, &tabRol);
#endif
    gfx_recorte(x0-margem,244,W+2*margem,H+12);
    for(int i=0;i<nAbas;i++) {
      float x=x0+pos[i]-tabRol;
      if(x+larg[i]<=x0-margem||x>=x0+W+margem)continue;
      int f=tabFocus&&tabCursor==i,selecionada=source==i;
      float fa=tabAnim[i];
      GfxRect pill={x,253,larg[i],H};
      ponteiro_alvo(fmaxf(pill.x,x0-margem),pill.y,fminf(pill.x+pill.w,x0+W+margem)-fmaxf(pill.x,x0-margem),pill.h,ponteiroAba,NULL,i,0);
      if (selecionada) {
        float sr, sg, sb; corFocoFonte(&sr, &sg, &sb);
        gfx_cor(pill, NV_RAIO_PILL, sr, sg, sb,
                a * (0.62f + 0.38f * fa));
      } else gfx_cor(pill,NV_RAIO_PILL,1,1,1,.04f*a*(1-fa));
      if(fa>.001f) focoAbaFonte(pill, fa, a);
      int cor=f?ajustes_tinta_foco():selecionada?ajustes_tinta_foco2():176;
      TxtLinha t=txt_linha_corta(TXT_HERO_META,rot[i],cor,cor,cor,255,textoW);
      txt_desenhar_alpha(t,x+PAD,pill.y+(H-t.h)*.5f,a);
    }gfx_sem_recorte();
  }
}

static void timelineCard(int i,float cy,float a,float x0) {
  CatItem it;if(!viewItem(i,&it))return;
  int sel=i==foco&&!tabFocus;float r,g,b;corColecao(&r,&g,&b);
  // A linha organiza a cronologia; não é uma borda decorativa de card.
  float rail = vtTimelineRetrato() ? 64.0f : 109.0f;
  gfx_cor((GfxRect){x0+rail,cy-30,2,VT_CARD_H+VT_GAP_Y},0,.48f,.47f,.46f,a*.6f);
  gfx_cor((GfxRect){x0+rail-7,cy+24,16,16},.5f,sel?.95f:r,sel?.95f:g,sel?.97f:b,a);
  char year[16];int y=yearOf(&it);if(y==9999)snprintf(year,sizeof year,"—");else snprintf(year,sizeof year,"%d",y);
  TxtLinha yr=txt_linha(vtTimelineRetrato()?TXT_DET_META2:TXT_CW_TITULO,year,219,210,195,255);txt_desenhar_alpha(yr,x0,cy+14,a);
  GfxRect card=vtTimelineCard(cy), artRect=vtTimelineArte(card);
  float x=card.x,tx=vtTelefone()?artRect.x+artRect.w+24.0f:x+418.0f;
  float tw=vtTelefone()?card.x+card.w-tx-24.0f:550.0f;
  if(sel)gfx_cor((GfxRect){x-4,cy-4,card.w+8,VT_CARD_H+8},.045f,.94f,.95f,.97f,a);
  gfx_cor(card,.04f,.09f,.095f,.105f,a);
  const char *art=it.backdrop[0]?it.backdrop:it.poster;GLuint tex=art[0]?tex_obter_larg(art,390):0;
  if(tex){gfx_tex_aspect_atual=tex_aspecto(art);gfx_rect(artRect,tex,GFX_CARD,0,0,0,.04f,0,0,0,a);gfx_tex_aspect_atual=0;}
  else { gfx_cor(artRect,.04f,.16f,.16f,.18f,a);
         txt_desenhar_alpha(txt_linha(TXT_MINI,"Sem arte",184,188,198,255),
                            vtTelefone()?artRect.x+artRect.w*.5f-32.0f:x+158,vtTelefone()?artRect.y+artRect.h*.5f-10.0f:y+104,a*.9f); }
  TxtLinha name=txt_linha_corta(TXT_CW_TITULO,it.titulo,242,243,247,255,tw);txt_desenhar_alpha(name,tx,cy+22,a);
  TxtLinha genre=txt_linha_corta(TXT_HERO_META,it.genero,187,194,207,255,tw);txt_desenhar_alpha(genre,tx,cy+64,a);
  txt_bloco(TXT_DET_META2,it.sinopse,209,214,225,tx,cy+106,vtTelefone()?tw:545.0f,30,a,3);
}

// --- PECAS COMUNS DAS EXIBICOES ----------------------------------------------
// Raio do cartao em fracao da ALTURA, que e o que o shader mede (ver raioDe em
// home.c); o teto e metade da largura.
static float raioCard(float w, float h) {
  float r = ajustes_raio_poster_px() / h, teto = 0.5f * w / h;
  return r > teto ? teto : r;
}
// A ARTE DE UM CARTAO, com o foco do app (anel ou crescimento, focoprof.h), o
// esqueleto enquanto a arte nao chega e a chegada esvanecendo (revela.h).
// Devolve a caixa desenhada (maior que r0 quando o foco cresce).
static GfxRect cartaoArte(int i, GfxRect r0, const char *arte, float ac, Uint32 agora, int sel) {
  GfxRect r = r0;
  float raio = raioCard(r0.w, r0.h), aArte;
  GLuint t;
  if (sel) {
    if (ajustes_borda_foco()) foco_anel(r0, ajustes_raio_poster_px() / (r0.w < r0.h ? r0.w : r0.h), 1.0f, ac);
    else r = foco_zoom(r0, 1.0f);
  }
  t = arte && arte[0] ? tex_obter_larg(arte, r0.w) : 0;
  aArte = i < VT_MAX ? revela_arte(&revArte[i], t != 0, agora) : 1.0f;
  if (!t || aArte < 0.999f)
    gfx_cor(r, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, ac);
  if (t) {
    gfx_tex_aspect_atual = tex_aspecto(arte);
    gfx_rect(r, t, GFX_CARD, sel ? 1.0f : 0.0f, 0, 0, raio, 0, 0, 0, ac * aArte);
    gfx_tex_aspect_atual = 0.0f;
  }
  return r;
}
static const char *artePoster(const CatItem *it) {
  const char *pp = posterprov_card_addon(it->origem, it->imdb, it->tmdb, it->tipo, it->poster);
  return pp[0] ? pp : it->backdrop;
}
static const char *arteDeitada(const CatItem *it) {
  return it->backdrop[0] ? it->backdrop : it->poster;
}

// O CARTAZ DA GRADE (Grade e Decadas): arte 2:3, titulo embaixo, e na Grade de
// um ranking o numero pequeno contornado de sempre.
static void cartazGrade(int i, const CatItem *it, float cx, float cy, float w, float h,
                        float ac, Uint32 agora, int sel) {
  int c = sel ? 255 : 214;
  if (lay == VT_LAY_GRADE) {
    // A GRADE DE SEMPRE, pixel por pixel: o mesmo raio e o mesmo foco de antes.
    GfxRect r0 = { cx, cy, w, h }, r = r0;
    float raio = ajustes_raio_poster_px() / VT_CARD_W, aArte;
    const char *arteCard = artePoster(it);
    GLuint t;
    if (sel && !ajustes_borda_foco()) r = foco_zoom(r0, 1.0f);
    if (sel && ajustes_borda_foco()) {
      GfxRect anel = { cx - NV_ANEL_FOCO, cy - NV_ANEL_FOCO,
                       w + 2 * NV_ANEL_FOCO, h + 2 * NV_ANEL_FOCO };
      float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
      if (ajustes_vidro()) gfx_vidro_cartao(r, raio, 1.0f, ac);
      else gfx_cor(anel, (ajustes_raio_poster_px() + NV_ANEL_FOCO) / (w + 2 * NV_ANEL_FOCO), ar, ag, ab, ac);
    }
    t = arteCard[0] ? tex_obter_larg(arteCard, w) : 0;
    aArte = i < VT_MAX ? revela_arte(&revArte[i], t != 0, agora) : 1.0f;
    if (!t || aArte < 0.999f)
      gfx_cor(r, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, ac);
    if (t) {
      gfx_tex_aspect_atual = tex_aspecto(arteCard);
      gfx_rect(r, t, GFX_CARD, sel ? 1.0f : 0.0f, 0, 0, raio, 0, 0, 0, ac * aArte);
      gfx_tex_aspect_atual = 0.0f;
    }
  } else cartaoArte(i, (GfxRect){ cx, cy, w, h }, artePoster(it), ac, agora, sel);
  { TxtLinha l = txt_linha_corta(TXT_DET_META2, it->titulo, c, c, c, 255, w);
    txt_desenhar_alpha(l, cx, cy + h + 10.0f, ac * (sel ? 1.0f : 0.86f)); }
  if (ranked && lay == VT_LAY_GRADE) {
    char rank[8];snprintf(rank,sizeof rank,"%d",i+1);
    TxtLinha edge=txt_linha(TXT_RANK,rank,234,236,241,255),ink=txt_linha(TXT_RANK,rank,17,18,22,255);
    float x=cx-10,y=cy+h-edge.h;
    for(int dx=-2;dx<=2;dx+=2)for(int dy=-2;dy<=2;dy+=2)txt_desenhar_alpha(edge,x+dx,y+dy,ac);
    txt_desenhar_alpha(ink,x,y,ac);
  }
}

// RANKING: numeral grande no vao a esquerda, o cartaz por cima da ponta dele
// (o Top 10 da Dinamica, em pagina inteira). Os tres primeiros no acento.
static void rankingCard(int i, const CatItem *it, float cx, float cy, float w, float h,
                        float ac, Uint32 agora, int sel) {
  char num[8];
  TxtLinha nu;
  snprintf(num, sizeof num, "%d", i + 1);
  nu = txt_linha(TXT_RANK_GRANDE, num, 255, 255, 255, 255);
  if (nu.tex && nu.w > 0) {
    // Cabe no vao (100 px) mais o pedaco que o cartaz cobre; "100" encolhe.
    float vao = 100.0f + 0.22f * w, e = vao / (float)nu.w, nw, nh;
    float ar, ag, ab;
    if (e > h * 0.86f / (float)nu.h) e = h * 0.86f / (float)nu.h;
    nw = nu.w * e; nh = nu.h * e;
    if (i < 3) { ajustes_acento(&ar, &ag, &ab); }
    else { ar = 0.91f; ag = 0.92f; ab = 0.94f; }
    gfx_rect((GfxRect){ cx + 0.22f * w - nw, cy + h - nh * NV_TOP10_NUM_BASE, nw, nh },
             nu.tex, GFX_TEXTO, 0, 0, 0, 0.0f, ar, ag, ab, ac * (i < 3 ? 0.95f : 0.62f));
  }
  cartaoArte(i, (GfxRect){ cx, cy, w, h }, artePoster(it), ac, agora, sel);
  { int c = sel ? 255 : 214;
    TxtLinha l = txt_linha_corta(TXT_DET_META2, it->titulo, c, c, c, 255, w);
    txt_desenhar_alpha(l, cx, cy + h + 10.0f, ac * (sel ? 1.0f : 0.86f)); }
}

// SAGA: o capitulo deitado com o numero dele e, acima da linha, a TRILHA: um
// no por capitulo, cheio no acento quando ja foi visto, e o trecho entre dois
// capitulos vistos aceso. E o "onde eu parei" da franquia sem abrir nada.
static void sagaCard(int i, const CatItem *it, float cx, float cy, float w, float h,
                     float ac, Uint32 agora, int sel, int n) {
  float ar, ag, ab, ty = cy - VT_SAGA_TRILHA * 0.5f - 2.0f;
  int visto = celVisto[i];
  char cap[48];
  ajustes_acento(&ar, &ag, &ab);
  // Trecho ate o proximo capitulo da MESMA linha.
  if (i + 1 < n && celLin[i + 1] == celLin[i]) {
    int aceso = visto && celVisto[i + 1];
    float x1 = cx + w * 0.5f, x2 = x0Atual + celX[i + 1] + celW[i + 1] * 0.5f;
    if (aceso) gfx_cor((GfxRect){ x1, ty - 2.0f, x2 - x1, 4.0f }, 0.5f, ar, ag, ab, ac * 0.9f);
    else gfx_cor((GfxRect){ x1, ty - 1.0f, x2 - x1, 2.0f }, 0.5f, 1, 1, 1, ac * 0.20f);
  }
  { float d = sel ? 22.0f : 16.0f;
    GfxRect no = { cx + w * 0.5f - d * 0.5f, ty - d * 0.5f, d, d };
    if (visto) gfx_cor(no, 0.5f, ar, ag, ab, ac);
    else { gfx_cor(no, 0.5f, 0.12f, 0.13f, 0.15f, ac);
           gfx_rect(no, 0, GFX_ANEL, 0, 2.0f / d, 0, 0.5f, 1, 1, 1, ac * (sel ? 0.95f : 0.45f)); } }
  { GfxRect r = cartaoArte(i, (GfxRect){ cx, cy, w, h }, arteDeitada(it), ac, agora, sel);
    char nn[8];
    TxtLinha l;
    float raio = raioCard(r.w, r.h);
    gfx_veu_base(r, raio, 0.5f, 0.75f * ac);
    snprintf(nn, sizeof nn, "%02d", i + 1);
    l = txt_linha(TXT_TITULO2, nn, 246, 247, 250, 255);
    txt_desenhar_alpha(l, r.x + 20.0f, r.y + r.h - l.h - 14.0f, ac * 0.96f);
    if (visto) {
      TxtLinha v = txt_linha(TXT_CAPTION, "Assistido", 20, 20, 24, 255);
      GfxRect p = { r.x + r.w - v.w - 40.0f, r.y + 14.0f, v.w + 26.0f, v.h + 8.0f };
      gfx_cor(p, 0.5f, ar, ag, ab, 0.95f * ac);
      { int tf = ajustes_tinta_foco(); TxtLinha vt = txt_linha(TXT_CAPTION, "Assistido", tf, tf, tf, 255);
        txt_desenhar_alpha(vt, p.x + 13.0f, p.y + 4.0f, ac); }
    } else if (it->progresso > 0) {
      GfxRect tr = { r.x + 20.0f, r.y + r.h - 10.0f, r.w - 40.0f, 4.0f };
      gfx_cor(tr, 0.5f, 1, 1, 1, 0.25f * ac);
      gfx_cor((GfxRect){ tr.x, tr.y, tr.w * it->progresso / 100.0f, tr.h }, 0.5f, ar, ag, ab, ac);
    } }
  snprintf(cap, sizeof cap, i18n("Capítulo %d"), i + 1);
  { TxtLinha e = txt_linha(TXT_MINI, cap, 170, 176, 188, 255);
    int c = sel ? 255 : 226;
    TxtLinha t = txt_linha_corta(TXT_CW_TITULO, it->titulo, c, c, c, 255, w);
    char ano[16]; int y = yearOf(it);
    txt_desenhar_alpha(e, cx, cy + h + 12.0f, ac * 0.9f);
    txt_desenhar_alpha(t, cx, cy + h + 12.0f + e.h + 4.0f, ac);
    if (y != 9999) {
      TxtLinha a; snprintf(ano, sizeof ano, "%d", y);
      a = txt_linha(TXT_MINI, ano, 170, 176, 188, 255);
      txt_desenhar_alpha(a, cx + w - a.w, cy + h + 12.0f, ac * 0.9f);
    } }
}

// MURAL: ladrilho deitado com o nome por dentro, sobre o veu da base. No
// grande o logo do titulo (quando ha) e a linha de meta.
static void muralTile(int i, const CatItem *it, float cx, float cy, float w, float h,
                      float ac, Uint32 agora, int sel) {
  GfxRect r = cartaoArte(i, (GfxRect){ cx, cy, w, h }, arteDeitada(it), ac, agora, sel);
  int grande = w > 400.0f;
  float pad = grande ? 28.0f : 18.0f, yb = r.y + r.h - pad;
  gfx_veu_base(r, raioCard(r.w, r.h), grande ? 0.55f : 0.62f, 0.85f * ac);
  if (grande && it->meta[0]) {
    TxtLinha m = txt_linha_corta(TXT_HERO_META, it->meta, 222, 226, 234, 255, r.w - 2 * pad);
    yb -= m.h; txt_desenhar_alpha(m, r.x + pad, yb, ac * 0.92f); yb -= 8.0f;
  }
  { GLuint tl = grande && it->logo[0] ? tex_obter_logo_larg(it->logo, 320.0f) : 0;
    float ap = tl ? tex_aspecto(it->logo) : 0.0f;
    if (tl && ap > 0.0f) {
      float wl = 320.0f, hl = wl / ap;
      if (hl > 96.0f) { hl = 96.0f; wl = hl * ap; }
      gfx_rect((GfxRect){ r.x + pad, yb - hl, wl, hl }, tl,
               tex_marca_escura(it->logo) ? GFX_MARCA : GFX_TEXTO, 0, 0, 0, 0, 1, 1, 1, ac);
    } else {
      int c = sel ? 255 : 236;
      TxtLinha t = txt_linha_corta(grande ? TXT_TITULO2 : TXT_DET_META2, it->titulo, c, c, c, 255,
                                   r.w - 2 * pad);
      txt_desenhar_alpha(t, r.x + pad, yb - t.h, ac);
    } }
}

// DECADAS: o cabecalho da secao. Numeral grande, a contagem e um fio ate o fim
// da grade. `fixo` = a copia presa no topo enquanto a secao rola por baixo.
static void cabecalhoDecada(int k, float x0, float y, float a) {
  char ano[24], qtd[48];
  float ar, ag, ab;
  TxtLinha t, q;
  if (decAno[k]) snprintf(ano, sizeof ano, i18n("Anos %d"), decAno[k]);
  else snprintf(ano, sizeof ano, "%s", i18n("Sem data"));
  snprintf(qtd, sizeof qtd, i18n(decQtd[k] == 1 ? "%d título" : "%d títulos"), decQtd[k]);
  ajustes_acento(&ar, &ag, &ab);
  t = txt_linha(TXT_TITULO2, ano, 244, 245, 248, 255);
  q = txt_linha(TXT_HERO_META, qtd, 176, 182, 194, 255);
  gfx_cor((GfxRect){ x0, y + 16.0f, 6.0f, t.h - 14.0f }, 3.0f / (t.h - 14.0f), ar, ag, ab, a);
  txt_desenhar_alpha(t, x0 + 22.0f, y + 8.0f, a);
  txt_desenhar_alpha(q, x0 + 22.0f + t.w + 20.0f, y + 8.0f + (t.h - q.h) * 0.6f, a * 0.9f);
  gfx_cor((GfxRect){ x0 + 22.0f + t.w + q.w + 44.0f, y + 8.0f + t.h * 0.55f,
                     VT_CW - (22.0f + t.w + q.w + 44.0f), 1.0f }, 0, 1, 1, 1, a * 0.16f);
}

// O BOTAO "Exibição": mora na faixa das abas, sobre a coluna do painel. Um
// pequeno simbolo de grade e o nome da exibicao atual; com o foco, o
// preenchimento no acento das abas e a dica de que OK troca.
// SEM ABAS (uma fonte so) ele vai para o inicio da faixa, onde as abas
// estariam: a coluna do painel e do retrato do diretor fica livre.
static void botaoExibicao(float a, float x0) {
  float x = nAbas() ? VT_PAN_X : x0, y = 253.0f, H = 52.0f;
  float maxW = nAbas() && grupo("Directors") ? 172.0f : VT_PAN_W;
  float f = pillAnim, sr, sg, sb;
  int cor = f > 0.5f ? ajustes_tinta_foco() : 214;
  TxtLinha t = txt_linha_corta(TXT_HERO_META, LAY_NOME[lay], cor, cor, cor, 255, maxW - 70.0f);
  // VITRINE: no canto de cima, a direita do nome da colecao.
  if (lay == VT_LAY_VITRINE) { x = NV_TELA_W - 104.0f - (t.w + 70.0f); y = 40.0f; }
  GfxRect pill = { x, y, t.w + 70.0f, H };
  corFocoFonte(&sr, &sg, &sb);
  // Sobre a arte do destaque a pilula precisa de fundo proprio para ler.
  if (lay == VT_LAY_VITRINE) gfx_cor(pill, NV_RAIO_PILL, 0, 0, 0, 0.42f * a * (1.0f - f));
  gfx_cor(pill, NV_RAIO_PILL, 1, 1, 1, 0.07f * a * (1.0f - f));
  if (f > 0.001f) focoAbaFonte(pill, f, a);
  { float gx = x + 22.0f, gy = y + (H - 18.0f) * 0.5f, c = cor / 255.0f;
    for (int q = 0; q < 4; q++)
      gfx_cor((GfxRect){ gx + (q & 1) * 10.0f, gy + (q >> 1) * 10.0f, 8.0f, 8.0f }, 0.25f, c, c, c, a); }
  txt_desenhar_alpha(t, x + 50.0f, y + (H - t.h) * 0.5f, a);
  if (f > 0.05f) {
    TxtLinha d = txt_linha(TXT_MINI, "OK troca a exibição", 186, 192, 204, 255);
    txt_desenhar_alpha(d, x + 4.0f, y + H + 6.0f, a * f * 0.9f);
  }
}

static void vitDesenhar(float a, float x0) {
  float w, h, passo, rot, pas = vitPasso();
  CatItem *hero = vitChave(vitHeroAtual), *ant = vitChave(vitHeroAnt);
  int arte = 0;
  home_vitrine_medida(&w, &h, &passo, &rot);
  // A tela inteira ja e pintada pela arte do destaque (e o clear do quadro): so
  // a abertura/saida, com a Home por baixo, precisa do fundo escuro.
  if (a < 0.99f) { GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    gfx_cor(tela, 0.0f, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, a); }
  if (hero) {
    if (ant && vitHeroEntra < 1.0f) arte |= home_vitrine_arte(ant, a);
    arte |= home_vitrine_arte(hero, a * (ant ? vitHeroEntra : 1.0f));
  }
  if (!arte) fundoColecao(a);
  if (hero) home_vitrine_texto(hero, a);

  // As fileiras, recortadas abaixo do texto do destaque.
  gfx_recorte(0.0f, NV_SHELF_TOP - 8.0f, NV_TELA_W, NV_TELA_H - NV_SHELF_TOP + 8.0f);
  for (int r = 0; r < vitN; r++) {
    float y = NV_SHELF_TOP + r * pas - vitScroll, cy = y + VIT_TITULO_H;
    float ac = a * (r == vitFR ? 1.0f : 0.78f);
    VitLin *l = &vitLin[r];
    TxtLinha tt;
    if (y > NV_TELA_H) break;
    if (y + pas < NV_SHELF_TOP - 8.0f) continue;
    tt = txt_linha_corta(TXT_ROW_TITULO, l->nome[0] ? l->nome : "—", 245, 246, 249, 255, 900);
    txt_desenhar_alpha(tt, x0, y, ac);
    if (l->n > 0) {
      for (int c = 0; c < l->n; c++) {
        float cx = x0 + c * passo - vitSx[r];
        GfxRect base = { cx, cy, w, h }, card = base;
        int sel = r == vitFR && c == vitFC && !tabFocus;
        if (cx + w < 0.0f) continue;
        if (cx > NV_TELA_W) break;
        if (sel) {
          if (ajustes_borda_foco())
            foco_anel(base, ajustes_raio_poster_px() / (w < h ? w : h), 1.0f, a);
          else card = foco_zoom(base, 1.0f);
        }
        home_vitrine_cartao(&vitBuf[r * vitCap + c], base, card, ac);
      }
    } else if (l->estado == VIT_VAZIA) {
      const char *msg = l->semFonte ? i18n("O addon desta coleção não está instalado nesta TV.")
                                    : i18n("Nenhum título nesta lista.");
      TxtLinha m = txt_linha_corta(TXT_DET_META2, msg, 196, 202, 213, 255, 1200);
      txt_desenhar_alpha(m, x0, cy + 12.0f, ac * 0.9f);
    } else {
      // Esqueleto enquanto a fileira espera a vez ou a rede.
      for (int c = 0; c < 8; c++) {
        GfxRect e = { x0 + c * passo, cy, w, h };
        if (e.x > NV_TELA_W) break;
        gfx_cor(e, ajustes_raio_poster_px() / h, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G,
                NV_COR_ESQUELETO_B, ac * 0.7f);
      }
    }
  }
  if (!vitN) {   // nada para repartir em fileiras ainda: diz por que
    const char *pv = collection && source < collection->nSources ? collection->sources[source].prov : "";
    const char *msg = semFonte
      ? (!strcmp(pv, "tmdb") ? i18n("O TMDB não está configurado nesta TV.")
         : !strcmp(pv, "trakt") ? i18n("O Trakt não está configurado nesta TV.")
         : i18n("O addon desta coleção não está instalado nesta TV."))
      : desc_vertudo_erro() ? "Não foi possível carregar. OK para tentar novamente."
      : desc_vertudo_carregando() ? "Carregando títulos…" : "Nenhum título nesta lista.";
    txt_desenhar_alpha(txt_linha_corta(TXT_DET_META2, msg, 196, 202, 213, 255, 1200),
                       x0, NV_SHELF_TOP + 20.0f, a);
  }
  gfx_sem_recorte();

  // A faixa de cima: a colecao a que isto pertence e o botao "Exibição".
  { TxtLinha eb = txt_linha(TXT_HERO_META, rotuloGrupo(), 214, 219, 227, 255);
    TxtLinha tt = txt_linha_corta(TXT_TITULO2, titulo, 246, 247, 250, 255, 900);
    txt_desenhar_alpha(eb, x0, 36.0f, a * 0.85f);
    txt_desenhar_alpha(tt, x0, 62.0f, a); }
  botaoExibicao(a, x0);
}

void vertudo_desenhar(Uint32 agora) {
  float a = anim, x0 = vtInicio();
  int n = nItens(), i, lin0 = -1;
  float corteTopo = VT_TOPO - 12.0f;
  if (a < 0.01f) return;
  float escalaAnterior = gfx_escala();
  if (vtTelefone()) gfx_escala_sair(vtEscala());
#ifdef NV_TOUCH_UI
  toqueAbas.offset = NULL;
  ponteiro_rolagem(toqueVertudoRolar);
#endif
  if (n > VT_MAX) n = VT_MAX;
  if (lay == VT_LAY_VITRINE) {
    vitDesenhar(a, x0);
    if (vtTelefone()) gfx_escala_sair(escalaAnterior);
    return;
  }
  if (celLay != lay || celN != n) montar(n);
  x0Atual = x0;
  if (ondaArmada && n > 0) { ondaEm = agora ? agora : 1u; ondaArmada = 0; }
  if (ondaEm && revela_onda_fim(ondaEm, agora)) ondaEm = 0;
  { GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    gfx_cor(tela, 0.0f, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, a); }
  themeBackground(a);

  // DECADAS: o cabecalho da secao de cima fica PRESO no topo enquanto ela rola;
  // os cartazes passam a ser recortados abaixo dele. A secao seguinte, ao
  // chegar, apaga a copia presa (sem empurrar para cima das abas).
  int presa = -1; float presaA = 0.0f;
  if (lay == VT_LAY_DECADAS && decN) {
    for (int k = 0; k < decN; k++) if (VT_TOPO + decY[k] - scrollY <= VT_TOPO + 0.5f) presa = k;
    if (presa >= 0) {
      float prox = presa + 1 < decN ? VT_TOPO + decY[presa + 1] - scrollY : 1e9f;
      presaA = anim_clamp((prox - VT_TOPO) / VT_DEC_H - 0.25f, 0.0f, 1.0f);
      corteTopo = VT_TOPO + VT_DEC_H - 24.0f;
    }
  }

  // A GRADE E RECORTADA ABAIXO DO CABECALHO.
  //
  // O cabecalho ja era desenhado em posicao fixa, mas os cartazes passavam POR
  // TRAS dele ao rolar — o titulo ficava sobre imagem em movimento e virava
  // "fundo". O recorte resolve sem precisar de faixa opaca: o que sobe alem do
  // topo simplesmente nao e desenhado.
  if (lay == VT_LAY_DECADAS) {
    gfx_recorte(0.0f, VT_TOPO - 12.0f, NV_TELA_W, NV_TELA_H - VT_TOPO + 12.0f);
    for (int k = 0; k < decN; k++) {
      float hy = VT_TOPO + decY[k] - scrollY;
      if (k == presa || hy > NV_TELA_H || hy + VT_DEC_H < VT_TOPO) continue;
      cabecalhoDecada(k, x0, hy, a);
    }
    if (presa >= 0 && presaA > 0.01f) cabecalhoDecada(presa, x0, VT_TOPO, a * presaA);
    gfx_sem_recorte();
  }
  gfx_recorte(0.0f, corteTopo, NV_TELA_W, NV_TELA_H - corteTopo);
  for (i = 0; i < n; i++) {
    float cx = x0 + celX[i], cy = VT_TOPO + celY[i] - scrollY;
    float w = celW[i], h = celH[i];
    CatItem it;
    int sel = (i == foco) && !tabFocus;
    if (cy > NV_TELA_H) { if (lay != VT_LAY_MURAL) break; else continue; }
    if (cy + h + 120.0f < corteTopo) continue;
    if (lin0 < 0) lin0 = celLin[i];
    // ONDA: cada cartaz sobe alguns pixels e ganha opacidade com atraso pela
    // coluna e pela fileira visiveis (revela.h). So na primeira aparicao.
    float entra = ondaEm ? revela_entra(ondaEm, revela_onda_atraso(celCol[i],
                                         celLin[i] - lin0), agora) : 1.0f;
    float ac = a * entra;
    if (ac < 0.005f) continue;
    cy += (1.0f - entra) * NV_ENTRA_DY;
    if (lay == VT_LAY_LINHA) { GfxRect r=vtTimelineCard(cy); ponteiro_alvo_faixa(r.x,r.y,r.w,r.h,VT_TOPO-12.0f,NV_TELA_H,ponteiroCartaz,NULL,i,0); }
    else ponteiro_alvo_faixa(cx,cy,w,h+40.0f,VT_TOPO-12.0f,NV_TELA_H,ponteiroCartaz,NULL,i,0);
    if (lay == VT_LAY_LINHA) { timelineCard(i, cy, ac, x0); continue; }
    if (!viewItem(i, &it)) continue;
    switch (lay) {
      case VT_LAY_RANKING: rankingCard(i, &it, cx, cy, w, h, ac, agora, sel); break;
      case VT_LAY_SAGA:    sagaCard(i, &it, cx, cy, w, h, ac, agora, sel, n); break;
      case VT_LAY_MURAL:   muralTile(i, &it, cx, cy, w, h, ac, agora, sel); break;
      default:             cartazGrade(i, &it, cx, cy, w, h, ac, agora, sel);
    }
  }
  if(!n&&desc_vertudo_carregando())for(int i=0;i<VT_COLS;i++) {
    float w=vtTelefone()&&!timeline?VT_CARD_W:248.0f;
    float h=vtTelefone()&&!timeline?VT_CARD_H:372.0f;
    gfx_cor((GfxRect){x0+i*(w+VT_GAP_X),VT_TOPO,w,h},.06f,.12f,.13f,.15f,a);
  }
  gfx_sem_recorte();

  // CABECALHO por cima do recorte, entao ele nunca compete com a arte.
  themeHeader(a,x0);
  if (collection) botaoExibicao(a, x0);

  if (n > 0 && !vtTelefone()) painel(a);
  if (vtTelefone()) gfx_escala_sair(escalaAnterior);
}
