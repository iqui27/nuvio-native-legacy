#include "imdbnota.h"
#include "vertudo.h"
#include "servidores.h"
#include "posterprov.h"
#include "idioma.h"
#include "badges.h"
#include "descoberta.h"
#include "catalogo.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
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
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>

// MEDIDAS do web (catalogSeeAllScreen, .seeall-card): cartaz de 248 de largura
// e raio 12. A 1920 cabem 5 colunas com o gutter da tela dos dois lados.
#define VT_COLS      (timeline ? 1 : 5)
#define VT_CARD_W  248.0f
#define VT_CARD_H  (timeline ? 236.0f : VT_CARD_W * 1.5f)
#define VT_GAP_X    16.0f                 // .seeall-grid: gap 20px 16px
#define VT_GAP_Y    (20.0f + 40.0f)       // gap + a linha de titulo sob o cartaz
#define VT_TOPO    (servidorAtivo ? 402.0f : collection ? 332.0f : 244.0f)

// PAINEL DE DETALHE, a direita. Medidas do web (.seeall-detail), que as ancora
// explicitamente na tela real de 1920x1080: top 170, right 104, largura 336.
//
// FIXO e nao rolando junto: o proprio CSS registra que `position: sticky` nao
// existe no Chromium 53 da TV e que sem `fixed` o painel sumia assim que o dono
// descia. Aqui nao ha fluxo nenhum — ele so nao recebe scrollY.
#define VT_PAN_W   336.0f
#define VT_PAN_X   (NV_TELA_W - 104.0f - VT_PAN_W)
#define VT_PAN_Y   VT_TOPO
#define VT_PAN_ART_H 330.0f
#define VT_PAN_ART_W 220.0f

static int   aberta, foco, pedAbrir = -1;
static float anim, scrollY, velY;
static char  titulo[96];
static const ColFolder *collection;
static ColFolder servidor;
static ColSource servidorFontes[SRV_VISAO_MAX];
static char servidorRaiz[96];
static char servidorChaves[SRV_VISAO_MAX][96];
static int servidorAtivo, servidorFiltro, servidorOrdem, servidorControle;
static int servidorMapa[VT_MAX], servidorN = -1, servidorLidos = -1;
static const char *const SRV_FILTROS[] = { "Todos", "Filmes", "Séries", "Não assistido", "Fonte HDR", "4K" };
static const char *const SRV_ORDENS[] = { "Recentes", "Nome (A–Z)", "Ano", "Nota" };
static int source, tabFocus, tabCursor, timeline, ranked;
static float tabAnim[COL_SOURCE_MAX];
static int order[VT_MAX], orderN=-1;
static char catalogId[96];
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
  if (timeline) return "Filmografia em ordem cronológica";
  if (grupo("Awards")) return ranked ? "Ranking na ordem original" : "Lista de prêmios configurada pelo usuário";
  if (grupo("Genres")) return "Títulos do gênero no seu catálogo";
  if (grupo("Themes")) return "Uma curadoria por tema e atmosfera";
  if (grupo("Streaming")) return "Catálogo organizado por serviço";
  return ranked ? "Ordem original do ranking" : "Seleção do seu catálogo";
}

static int yearOf(const CatItem *it) {
  for(const char *s=it->meta;*s;s++)if((*s=='1'||*s=='2')&&strlen(s)>=4&&s[1]>='0'&&s[1]<='9'&&s[2]>='0'&&s[2]<='9'&&s[3]>='0'&&s[3]<='9')return atoi(s);
  return 9999;
}
static int viewItem(int i,CatItem *out) {
  if (servidorAtivo) return i >= 0 && i < servidorN && desc_vertudo_item(servidorMapa[i], out);
  return desc_vertudo_item(timeline&&i<orderN?order[i]:i,out);
}
static void servidorOrdenar(void) {
  int n = desc_vertudo_n();
  CatItem *it;
  if (!servidorAtivo || (n == servidorLidos && servidorN >= 0)) return;
  it = calloc(2, sizeof *it);
  if (!it) return;
  servidorN = 0; servidorLidos = n;
  for (int i = 0; i < n; i++) {
    SrvAchado info = {0};
    int tem;
    if (!desc_vertudo_item(i, it)) continue;
    tem = servidores_info(it->imdb, it->tipo, &info);
    if (strcmp(it->tipo, "group") && strcmp(it->tipo, "list") &&
        ((servidorFiltro == 1 && strcmp(it->tipo, "movie")) ||
        (servidorFiltro == 2 && strcmp(it->tipo, "series")) ||
        (servidorFiltro == 3 && (!tem || info.visto)) ||
        (servidorFiltro == 4 && (!tem || !info.hdr)) ||
        (servidorFiltro == 5 && (!tem || info.altura < 2160)))) continue;
    servidorMapa[servidorN++] = i;
  }
  if (servidorOrdem) for (int i = 1; i < servidorN; i++) {
    int j = i, v = servidorMapa[i];
    desc_vertudo_item(v, it);
    while (j > 0) {
      int troca;
      desc_vertudo_item(servidorMapa[j - 1], it + 1);
      troca = servidorOrdem == 1 ? strcasecmp(it->titulo, it[1].titulo) < 0
            : servidorOrdem == 2 ? yearOf(it) > yearOf(it + 1) : it->nota > it[1].nota;
      if (!troca) break;
      servidorMapa[j] = servidorMapa[j - 1]; j--;
    }
    servidorMapa[j] = v;
  }
  free(it);
  if (foco >= servidorN) foco = servidorN > 0 ? servidorN - 1 : 0;
  if (servidorN < 10 && !desc_vertudo_fim()) desc_vertudo_mais();
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

static void openSource(void) {
  const ColSource *s=&collection->sources[source];
  const char *base=baseDaFonte(s);
  snprintf(catalogId,sizeof catalogId,"%s",s->catId);
  ranked=strstr(s->catId,"top100")||strstr(s->catId,"top250")||strstr(s->catId,"top10");
  foco=0;scrollY=velY=0;orderN=-1;servidorN=servidorLidos=-1;armarOnda();
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
  servidorAtivo=0;
  collection=folder;source=tabCursor=0;tabFocus=folder->nSources>1;aberta=1;pedAbrir=-1;
  timeline=!strcmp(folder->group,"Directors");
  // COMECAR NA PRIMEIRA FONTE QUE TEM ENDERECO, e nao teimosamente na fonte 0.
  // Numa pasta com varias abas e comum que so parte dos addons esteja instalada
  // aqui; abrir na aba morta faz a pasta inteira parecer vazia.
  for (i = 0; i < folder->nSources; i++)
    if (folder->sources[i].prov[0] || baseDaFonte(&folder->sources[i])[0])
      { source = tabCursor = i; break; }
  snprintf(titulo,sizeof titulo,"%s",folder->title);openSource();
}

void vertudo_abrir(const char *base, const char *tipo, const char *catId,
                   const char *tit) {
  if (base && !strcmp(base, "servidores")) { vertudo_servidores(catId); return; }
  servidorAtivo=0;
  aberta = 1; foco = 0; scrollY = 0.0f; velY = 0.0f; pedAbrir = -1;
  memset(tabAnim, 0, sizeof tabAnim);
  snprintf(titulo, sizeof titulo, "%s", tit ? tit : "");
  collection=col_por_catalogo(base,tipo,catId);timeline=collection&&!strcmp(collection->group,"Directors");
  source=tabCursor=tabFocus=0;orderN=-1;
  if(collection) {
    for(int i=0;i<collection->nSources;i++)if(!strcmp(collection->sources[i].catId,catId)&&!strcmp(collection->sources[i].type,tipo)){source=tabCursor=i;break;}
    snprintf(titulo,sizeof titulo,"%s",collection->title);
  }
  snprintf(catalogId,sizeof catalogId,"%s",catId);
  ranked=strstr(catId,"top100")||strstr(catId,"top250")||strstr(catId,"top10");
  armarOnda();
  desc_vertudo_abrir(base, tipo, catId);
}

void vertudo_servidores(const char *chave) {
  CatFileira fils[SRV_VISAO_MAX];
  int n = servidores_bibliotecas(fils, SRV_VISAO_MAX), escolhido = 0;
  if (!n) return;
  memset(&servidor, 0, sizeof servidor);
  memset(servidorFontes, 0, sizeof servidorFontes);
  servidor.sources = servidorFontes;
  servidor.nSources = n;
  snprintf(servidor.title, sizeof servidor.title, "%s", i18n("Servidores pessoais"));
  for (int i = 0; i < n; i++) {
    ColSource *s = &servidorFontes[i];
    snprintf(s->base, sizeof s->base, "servidores");
    snprintf(s->catId, sizeof s->catId, "%s", fils[i].chave);
    snprintf(servidorChaves[i], sizeof servidorChaves[i], "%s", fils[i].chave);
    snprintf(s->type, sizeof s->type, "%s", fils[i].tipo);
    snprintf(s->title, sizeof s->title, "%s", fils[i].titulo);
    if (chave && !strcmp(chave, fils[i].chave)) escolhido = i;
  }
  collection = &servidor; timeline = ranked = 0;
  aberta = 1; pedAbrir = -1;
  memset(tabAnim, 0, sizeof tabAnim);
  snprintf(titulo, sizeof titulo, "%s", servidor.title);
  servidorAtivo = 1;
  servidorFiltro = servidorOrdem = servidorControle = 0;
  source = tabCursor = escolhido;
  snprintf(servidorRaiz, sizeof servidorRaiz, "%s", servidorFontes[escolhido].catId);
  tabFocus = n > 1;
  openSource();
}

int vertudo_aberta(void) { return aberta; }
void vertudo_fechar_seco(void) { aberta = 0; anim = 0.0f; }
int vertudo_pediu_abrir(void) { int v = pedAbrir; pedAbrir = -1; return v; }
// ESQUERDA na coluna 0 da grade (ou na primeira aba da colecao): a barra
// lateral abre POR CIMA da lista (dono, 03/10). Antes a coluna 0 voltava para
// o ultimo da linha de cima. app.c le no mesmo evento.
static int pedMenu;
int vertudo_pediu_menu(void) { int v = pedMenu; pedMenu = 0; return v; }

static int nItens(void) { return servidorAtivo ? (servidorN > 0 ? servidorN : 0) : desc_vertudo_n(); }

void vertudo_evento(const SDL_Event *e) {
  int n = nItens(), k;
  if (!aberta || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      e->key.keysym.scancode == NV_SCANCODE_BACK) {
    if (servidorAtivo && strcmp(catalogId, servidorRaiz)) {
      snprintf(servidorFontes[source].catId, sizeof servidorFontes[source].catId, "%s", servidorRaiz);
      openSource(); return;
    }
    aberta = 0; return;
  }
  if (servidorAtivo && tabFocus == 2) {
    if (k == SDLK_LEFT && servidorControle > 0) servidorControle--;
    if (k == SDLK_RIGHT && servidorControle < 1) servidorControle++;
    if (k == SDLK_UP) tabFocus = 1;
    if (k == SDLK_DOWN) tabFocus = 0;
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
      if (!servidorControle) servidorFiltro = (servidorFiltro + 1) % 6;
      else servidorOrdem = (servidorOrdem + 1) % 4;
      servidorLidos = -1; foco = 0; scrollY = 0;
      servidorOrdenar();
    }
    return;
  }
  if(collection&&tabFocus) {
    if(k==SDLK_LEFT){ if(tabCursor>0)tabCursor--; else pedMenu=1; }
    if(k==SDLK_RIGHT&&tabCursor+1<collection->nSources)tabCursor++;
    if(k==SDLK_RETURN||k==SDLK_KP_ENTER){source=tabCursor;
      if (servidorAtivo) {
        snprintf(servidorFontes[source].catId, sizeof servidorFontes[source].catId, "%s", servidorChaves[source]);
        snprintf(servidorRaiz, sizeof servidorRaiz, "%s", servidorChaves[source]);
        servidorFiltro = 0;
      }
      openSource();tabFocus=0;}
    if(k==SDLK_DOWN&&(n>0||servidorAtivo))tabFocus=servidorAtivo?2:0;
    return;
  }
  if(k==SDLK_UP&&foco<VT_COLS&&collection){tabFocus=servidorAtivo?2:1;tabCursor=source;return;}
  if((k==SDLK_RETURN||k==SDLK_KP_ENTER)&&desc_vertudo_erro()){desc_vertudo_mais();return;}
  if (k == SDLK_LEFT && (n < 1 || foco % VT_COLS == 0)) { pedMenu = 1; return; }
  if (n < 1) return;
  if (k == SDLK_RIGHT && foco + 1 < n) foco++;
  else if (k == SDLK_LEFT) foco--;
  else if (k == SDLK_DOWN) { if (foco + VT_COLS < n) foco += VT_COLS;
                             else foco = n - 1; }
  else if (k == SDLK_UP) { if (foco >= VT_COLS) foco -= VT_COLS; }
  else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
    // O item da grade NAO esta no catalogo global — ele veio de uma pagina que
    // so esta tela leu. Entra por cat_acrescentar para que a tela de titulo
    // possa abri-lo por indice, que e como todo o app trabalha.
    CatItem it;
    if (viewItem(foco, &it)) {
      if (servidorAtivo && (!strcmp(it.tipo, "group") || !strcmp(it.tipo, "list"))) {
        char tag[JFID_TAG + 1], item[JFID_ITEM + 1], chave[96];
        const char *pre = !strncmp(catalogId, "plex_", 5) ? "plex_" : !strncmp(catalogId, "emby_", 5) ? "emby_" : "jellyfin_";
        const char *ip = pre[0] == 'p' ? JFID_PREFIXO_PLEX : pre[0] == 'e' ? JFID_PREFIXO_EMBY : JFID_PREFIXO;
        if (jfid_partes_p(ip, it.imdb, tag, item)) {
          snprintf(chave, sizeof chave, "%s%s%s", pre, !strcmp(it.tipo, "list") ? "list_" : pre[0] == 'p' ? "group_" : "", item);
          snprintf(servidorFontes[source].catId, sizeof servidorFontes[source].catId, "%s", chave);
          openSource();
        }
        return;
      }
      // Item de fonte TMDB (issue #44): o id e "tmdb:<n>", nao imdb. Quem
      // resolve e o caminho sob-demanda ja usado pela filmografia de elenco:
      // external_ids -> imdb -> meta do Cinemeta -> cat_acrescentar, e o
      // desc_titulo_pronto do loop principal abre o detalhe.
      if (!strncmp(it.imdb, "tmdb:", 5)) {
        desc_pedir_titulo_tmdb(atol(it.imdb + 5),
                               !strcmp(it.tipo, "series") ? "tv" : "movie");
      } else {
        int idx = it.imdb[0] ? cat_indice_por_imdb(it.imdb) : -1;
        if (idx < 0) idx = cat_acrescentar(&it);
        if (idx >= 0) {
          if (servidorAtivo && !strcmp(it.tipo, "series") && it.temporada > 0 && it.episodio > 0) {
            const CatItem *ci = cat_item(idx);
            if (ci) {
              CatItem selected = *ci;
              selected.temporada = it.temporada; selected.episodio = it.episodio;
              selected.progresso = it.progresso; selected.restanteMin = it.restanteMin;
              snprintf(selected.nomeEpisodio, sizeof selected.nomeEpisodio, "%s", it.nomeEpisodio);
              cat_atualizar_item(idx, &selected);
            }
          }
          pedAbrir = idx;
        } // conserva a lista e a posição ao voltar
      }
    }
  }
  // Chegando perto do fim, pede a proxima pagina. Antes de o dono ver o vazio,
  // e nao quando ele ja esta olhando para ele.
  if (foco >= n - VT_COLS * 2) desc_vertudo_mais();
}

void vertudo_atualizar(float dt, Uint32 agora) {
  float alvo, maxY;
  int n, linhas;
  servidorOrdenar();
  n = nItens();
  (void)agora;
  anim = anim_mola(anim, aberta ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  if (!aberta) return;
  // A SONDA PODE CHEGAR DEPOIS DA TELA. Enquanto a fonte nao tem endereco,
  // reconferir por quadro custa uma varredura de poucas strings (addons_n e
  // dezenas, nao milhares) e SO acontece no estado de erro — assim a colecao se
  // preenche sozinha quando o manifesto responde, em vez de exigir sair e
  // entrar de novo.
  if (servidorAtivo && (!ajustes_jellyfin_ligado() || !servidores_conectado())) {
    aberta = 0;
    return;
  }
  if (semFonte && collection) {
    const ColSource *s = &collection->sources[source];
    if (baseDaFonte(s)[0]) openSource();
  }
  for (int i = 0; i < COL_SOURCE_MAX; i++) {
    float alvoTab = collection && tabFocus == 1 && i == tabCursor ? 1.0f : 0.0f;
    tabAnim[i] = anim_mola(tabAnim[i], alvoTab, dt,
                           alvoTab > tabAnim[i] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  }
  if(timeline&&n!=orderN) {
    int old=orderN>0&&foco<orderN?order[foco]:-1;int years[VT_MAX];
    for(int i=0;i<n;i++){CatItem it;order[i]=i;years[i]=desc_vertudo_item(i,&it)?yearOf(&it):9999;}
    for(int i=1;i<n;i++){int v=order[i],j=i;while(j>0&&years[order[j-1]]>years[v]){order[j]=order[j-1];j--;}order[j]=v;}
    orderN=n;if(old>=0)for(int i=0;i<n;i++)if(order[i]==old){foco=i;break;}
  }
  linhas = (n + VT_COLS - 1) / VT_COLS;
  // Mira a linha focada a 30% da altura util, como o resto do app faz.
  alvo = VT_TOPO + (float)(foco / VT_COLS) * (VT_CARD_H + VT_GAP_Y)
       - NV_TELA_H * 0.30f;
  if(tabFocus)alvo=0;
  maxY = VT_TOPO + (float)linhas * (VT_CARD_H + VT_GAP_Y) - NV_TELA_H + 120.0f;
  if (maxY < 0.0f) maxY = 0.0f;
  if (alvo < 0.0f) alvo = 0.0f;
  if (alvo > maxY) alvo = maxY;
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
      TxtLinha falta = txt_linha_corta(servidorAtivo ? TXT_V2_24 : TXT_HERO_META, "Sem pôster", 185, 191, 204, 255, r.w-24);
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
    TxtLinha t = txt_linha_corta(servidorAtivo ? TXT_V2_24 : TXT_CAPTION, it.genero, 220, 231, 244, 255,
                                 VT_PAN_W);
    txt_desenhar_alpha(t, VT_PAN_X, y, a * 0.72f);
    y += t.h + 6.0f;
  }
  // Pastilha da nota, no amarelo do IMDb que o web usa (245,197,24).
  int imdbRating = imdbnota_obter(it.imdb, it.nota, !strcmp(it.tipo,"series"));
  if (imdbRating > 0) {
    char n[16];
    snprintf(n, sizeof n, "%.1f", imdbRating / 10.0f); idioma_decimal_texto(n, ajustes_idioma());
    { TxtLinha t = txt_linha(servidorAtivo ? TXT_V2_24 : TXT_CAPTION, n, 23, 19, 10, 255);
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
      GfxRect header={0,0,1920,320};
      if(collection->editorial==2&&tex_aspecto(collection->detailHero)>0) {
        float aspect=tex_aspecto(collection->detailHero);
        header.w=fminf(1920,header.h*aspect);header.h=header.w/aspect;
        header.x=1920-header.w;
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

static void themeHeader(float a,float x0) {
  float r,g,b;corColecao(&r,&g,&b);
  TxtLinha eyebrow=txt_linha(servidorAtivo ? TXT_V2_24 : TXT_HERO_META,servidorAtivo ? "Servidores pessoais" : rotuloGrupo(),197,202,211,255);
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
    float w=560.0f,h=w/aspect;if(h>108){h=108;w=h*aspect;}
    gfx_rect((GfxRect){x0,83,w,h},logo,tex_marca_escura(collection->logo)?GFX_MARCA:GFX_TEXTO,0,0,0,0,.96f,.97f,.98f,a);
  } else {TxtLinha title=txt_linha_corta(TXT_TITULO1,titulo,242,243,247,255,940);txt_desenhar_alpha(title,x0,80,a);}
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
  else snprintf(caption,sizeof caption,i18n("%d títulos%s  ·  %s"),n,desc_vertudo_fim()?"":i18n(" carregados"),i18n(servidorAtivo ? "Servidores pessoais" : legendaGrupo()));
  TxtLinha sub=txt_linha_corta(servidorAtivo ? TXT_V2_24 : TXT_DET_META2,caption,196,202,213,255,960);txt_desenhar_alpha(sub,x0,192,a);
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
    const float H=52.0f,PAD=22.0f,GAP=12.0f,W=NV_TELA_W-x0-90;
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
      larg[i]=txt_linha_corta(servidorAtivo ? TXT_V2_SEG : TXT_HERO_META,rot[i],176,176,176,255,420).w+PAD*2;pos[i]=px;px+=larg[i]+GAP;
    }
    // ROLAGEM PELO CURSOR, nao por indice: as abas tem larguras diferentes,
    // entao "seis por vez" nao existe mais. A aba com o cursor (ou a aberta,
    // quando o cursor esta na grade) entra inteira; a rolagem so anda o que
    // precisa, e o resto da faixa fica onde estava.
    static float rol; int alvo=tabFocus == 1 ? tabCursor : source;
    float ini=pos[alvo],fim=pos[alvo]+larg[alvo];
    if(fim-rol>W)rol=fim-W; if(ini-rol<0)rol=ini;
    if(rol<0)rol=0;
    gfx_recorte(x0-6,244,W+12,H+12);
    for(int i=0;i<nAbas;i++) {
      float x=x0+pos[i]-rol;
      if(x+larg[i]<x0-6||x>x0+W+6)continue;
      int f=tabFocus == 1 && tabCursor==i,selecionada=source==i;
      float fa=tabAnim[i];
      GfxRect pill={x,253,larg[i],H};
      if (selecionada && servidorAtivo) {
        gfx_cor(pill, NV_RAIO_PILL, 1, 1, 1, .13f * a);
      } else if (selecionada) {
        float sr, sg, sb; corFocoFonte(&sr, &sg, &sb);
        gfx_cor(pill, NV_RAIO_PILL, sr, sg, sb,
                a * (0.62f + 0.38f * fa));
      } else gfx_cor(pill,NV_RAIO_PILL,1,1,1,.04f*a*(1-fa));
      if(fa>.001f) focoAbaFonte(pill, fa, a);
      int cor=f?ajustes_tinta_foco():selecionada?(servidorAtivo?242:ajustes_tinta_foco2()):176;
      TxtLinha t=txt_linha_corta(servidorAtivo ? TXT_V2_SEG : TXT_HERO_META,rot[i],cor,cor,cor,255,420);
      txt_desenhar_alpha(t,x+PAD,pill.y+(H-t.h)*.5f,a);
    }gfx_sem_recorte();
  }
  if (servidorAtivo) {
    const char *rot[2] = { SRV_FILTROS[servidorFiltro], SRV_ORDENS[servidorOrdem] };
    float x = x0;
    if (ajustes_vidro()) gfx_vidro_painel((GfxRect){x0 - 18, 323, 650, 62}, 28, .8f, a);
    else gfx_cor((GfxRect){x0 - 18, 323, 650, 62}, .45f, .071f, .075f, .086f, a);
    for (int i = 0; i < 2; i++) {
      int f = tabFocus == 2 && servidorControle == i;
      TxtLinha l = txt_linha_corta(TXT_V2_SEG, i18n(rot[i]), 242, 243, 247, 255, 290);
      GfxRect r = {x, 328, l.w + 50, 52};
      if (f) { float ar, ag, ab; ajustes_acento(&ar, &ag, &ab); gfx_cor(r, .5f, ar, ag, ab, a);
        int c = ajustes_tinta_foco(); l = txt_linha_corta(TXT_V2_SEG, i18n(rot[i]), c, c, c, 255, 290); }
      txt_desenhar_alpha(l, x + 25, r.y + (r.h - l.h) * .5f, a);
      x += r.w + 18;
    }
  }
}

static void timelineCard(int i,float cy,float a,float x0) {
  CatItem it;if(!viewItem(i,&it))return;
  int sel=i==foco&&!tabFocus;float r,g,b;corColecao(&r,&g,&b);
  // A linha organiza a cronologia; não é uma borda decorativa de card.
  gfx_cor((GfxRect){x0+109,cy-30,2,VT_CARD_H+VT_GAP_Y},0,.48f,.47f,.46f,a*.6f);
  gfx_cor((GfxRect){x0+102,cy+24,16,16},.5f,sel?.95f:r,sel?.95f:g,sel?.97f:b,a);
  char year[16];int y=yearOf(&it);if(y==9999)snprintf(year,sizeof year,"—");else snprintf(year,sizeof year,"%d",y);
  TxtLinha yr=txt_linha(TXT_CW_TITULO,year,219,210,195,255);txt_desenhar_alpha(yr,x0,cy+14,a);
  float x=x0+158;GfxRect card={x,cy,1000,VT_CARD_H};
  if(sel)gfx_cor((GfxRect){x-4,cy-4,1008,VT_CARD_H+8},.045f,.94f,.95f,.97f,a);
  gfx_cor(card,.04f,.09f,.095f,.105f,a);
  const char *art=it.backdrop[0]?it.backdrop:it.poster;GLuint tex=art[0]?tex_obter_larg(art,390):0;
  if(tex){gfx_tex_aspect_atual=tex_aspecto(art);gfx_rect((GfxRect){x+12,cy+12,376,212},tex,GFX_CARD,0,0,0,.04f,0,0,0,a);gfx_tex_aspect_atual=0;}
  else { gfx_cor((GfxRect){x+12,cy+12,376,212},.04f,.16f,.16f,.18f,a);
         txt_desenhar_alpha(txt_linha(TXT_MINI,"Sem arte",184,188,198,255),
                            x+158,y+104,a*.9f); }
  TxtLinha name=txt_linha_corta(TXT_CW_TITULO,it.titulo,242,243,247,255,550);txt_desenhar_alpha(name,x+418,cy+22,a);
  TxtLinha genre=txt_linha_corta(TXT_HERO_META,it.genero,187,194,207,255,550);txt_desenhar_alpha(genre,x+418,cy+64,a);
  txt_bloco(TXT_DET_META2,it.sinopse,209,214,225,x+418,cy+106,545,30,a,3);
}

void vertudo_desenhar(Uint32 agora) {
  float a = anim, x0 = ajustes_conteudo_x();
  int n = nItens(), i, lin0;
  if (a < 0.01f) return;
  if (ondaArmada && n > 0) { ondaEm = agora ? agora : 1u; ondaArmada = 0; }
  if (ondaEm && revela_onda_fim(ondaEm, agora)) ondaEm = 0;
  // A onda conta a fileira VISIVEL: a grade que abre rolada (a volta de uma
  // colecao) entra pela primeira fileira da tela, nao pela zero.
  lin0 = (int)(scrollY / (VT_CARD_H + VT_GAP_Y));
  { GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    gfx_cor(tela, 0.0f, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, a); }
  themeBackground(a);

  // A GRADE E RECORTADA ABAIXO DO CABECALHO.
  //
  // O cabecalho ja era desenhado em posicao fixa, mas os cartazes passavam POR
  // TRAS dele ao rolar — o titulo ficava sobre imagem em movimento e virava
  // "fundo". O recorte resolve sem precisar de faixa opaca: o que sobe alem do
  // topo simplesmente nao e desenhado.
  gfx_recorte(0.0f, VT_TOPO - 12.0f, NV_TELA_W, NV_TELA_H - VT_TOPO + 12.0f);
  for (i = 0; i < n; i++) {
    float cx = x0 + (float)(i % VT_COLS) * (VT_CARD_W + VT_GAP_X);
    float cy = VT_TOPO + (float)(i / VT_COLS) * (VT_CARD_H + VT_GAP_Y) - scrollY;
    CatItem it;
    GLuint t;
    const char *arteCard;
    // MESMO raio dos cartazes da home: `posterCardCornerRadiusDp` (12dp x 2 =
    // 24px), fracao do MENOR lado porque o SDF do shader e normalizado. O
    // NV_RAIO_CARD fixo que estava aqui dava um canto diferente do resto do
    // app, e a grade lia como outra tela.
    float raio = ajustes_raio_poster_px() / VT_CARD_W;
    int sel = (i == foco);
    if (cy > NV_TELA_H || cy + VT_CARD_H + 40.0f < VT_TOPO - 12.0f) continue;
    // ONDA: cada cartaz sobe alguns pixels e ganha opacidade com atraso pela
    // coluna e pela fileira visiveis (revela.h). So na primeira aparicao.
    float entra = ondaEm ? revela_entra(ondaEm, revela_onda_atraso(i % VT_COLS,
                                         i / VT_COLS - lin0), agora) : 1.0f;
    float ac = a * entra;
    if (ac < 0.005f) continue;
    cy += (1.0f - entra) * NV_ENTRA_DY;
    if(timeline){timelineCard(i,cy,ac,x0);continue;}
    if (!viewItem(i, &it)) continue;
    { GfxRect r0 = { cx, cy, VT_CARD_W, VT_CARD_H }, r = r0;
      float aArte;
      // SEM ANEL: o foco e o crescimento (focoprof.h), como nas outras grades,
      // sem halo colorido. Antes o anel branco fixo ignorava os Ajustes.
      if (sel && !tabFocus && !ajustes_borda_foco()) {
        r = foco_zoom(r0, 1.0f);
      }
      if (sel && !tabFocus && ajustes_borda_foco()) {
        // O ANEL DO APP: no acento e so com "Foco no cartaz" ligado (era branco
        // fixo, a unica grade do app que nao seguia os Ajustes).
        GfxRect anel = { cx - NV_ANEL_FOCO, cy - NV_ANEL_FOCO,
                         VT_CARD_W + 2 * NV_ANEL_FOCO, VT_CARD_H + 2 * NV_ANEL_FOCO };
        float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
        if (ajustes_vidro()) gfx_vidro_cartao(r, ajustes_raio_poster_px() / VT_CARD_W, 1.0f, ac);
        else gfx_cor(anel, (ajustes_raio_poster_px() + NV_ANEL_FOCO) / (VT_CARD_W + 2 * NV_ANEL_FOCO), ar, ag, ab, ac);
      }
      { const char *pp = posterprov_card_addon(it.origem, it.imdb, it.tmdb, it.tipo, it.poster);
        arteCard = pp[0] ? pp : it.backdrop; }
      t = arteCard[0] ? tex_obter_larg(arteCard, VT_CARD_W) : 0;
      // Arte chegando esvanece sobre o esqueleto (revela.h), como na home.
      aArte = i < VT_MAX ? revela_arte(&revArte[i], t != 0, agora) : 1.0f;
      if (!t || aArte < 0.999f)
        // Esqueleto enquanto a arte nao chega — a mesma cor do resto do app.
        gfx_cor(r, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G,
                NV_COR_ESQUELETO_B, ac);
      if (t) {
        gfx_tex_aspect_atual = tex_aspecto(arteCard);
        gfx_rect(r, t, GFX_CARD, sel ? 1.0f : 0.0f, 0, 0, raio, 0, 0, 0, ac * aArte);
        gfx_tex_aspect_atual = 0.0f;
      } }
    { int c = sel ? 255 : 214;
      TxtLinha l = txt_linha_corta(servidorAtivo ? TXT_V2_24 : TXT_DET_META2, it.titulo, c, c, c, 255,
                                   VT_CARD_W);
      txt_desenhar_alpha(l, cx, cy + VT_CARD_H + 10.0f, ac * (sel ? 1.0f : 0.86f)); }
    if(ranked) {
      char rank[8];snprintf(rank,sizeof rank,"%d",i+1);
      TxtLinha edge=txt_linha(TXT_RANK,rank,234,236,241,255),ink=txt_linha(TXT_RANK,rank,17,18,22,255);
      float x=cx-10,y=cy+VT_CARD_H-edge.h;
      for(int dx=-2;dx<=2;dx+=2)for(int dy=-2;dy<=2;dy+=2)txt_desenhar_alpha(edge,x+dx,y+dy,ac);
      txt_desenhar_alpha(ink,x,y,ac);
    }
  }
  if(!n&&desc_vertudo_carregando())for(int i=0;i<5;i++)
    gfx_cor((GfxRect){x0+i*264,VT_TOPO,248,372},.06f,.12f,.13f,.15f,a);
  gfx_sem_recorte();

  // CABECALHO por cima do recorte, entao ele nunca compete com a arte.
  themeHeader(a,x0);

  if (n > 0) painel(a);
}
