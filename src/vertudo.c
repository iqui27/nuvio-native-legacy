#include "vertudo.h"
#include "idioma.h"
#include "badges.h"
#include "descoberta.h"
#include "catalogo.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "layout.h"
#include "anim.h"
#include "ajustes.h"
#include "diretor.h"
#include "addons.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

// MEDIDAS do web (catalogSeeAllScreen, .seeall-card): cartaz de 248 de largura
// e raio 12. A 1920 cabem 5 colunas com o gutter da tela dos dois lados.
#define VT_COLS      (timeline ? 1 : 5)
#define VT_CARD_W  248.0f
#define VT_CARD_H  (timeline ? 236.0f : VT_CARD_W * 1.5f)
#define VT_GAP_X    16.0f                 // .seeall-grid: gap 20px 16px
#define VT_GAP_Y    (20.0f + 40.0f)       // gap + a linha de titulo sob o cartaz
#define VT_TOPO    (collection ? 332.0f : 244.0f)

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
static int source, tabFocus, tabCursor, timeline, ranked;
static float tabAnim[COL_SOURCE_MAX];
static int order[VT_MAX], orderN=-1;
static char catalogId[96];
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
static int viewItem(int i,CatItem *out) {return desc_vertudo_item(timeline&&i<orderN?order[i]:i,out);}
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
  foco=0;scrollY=velY=0;orderN=-1;
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
  collection=folder;source=tabCursor=0;tabFocus=folder->nSources>1;aberta=1;pedAbrir=-1;
  timeline=!strcmp(folder->group,"Directors");
  // COMECAR NA PRIMEIRA FONTE QUE TEM ENDERECO, e nao teimosamente na fonte 0.
  // Numa pasta com varias abas e comum que so parte dos addons esteja instalada
  // aqui; abrir na aba morta faz a pasta inteira parecer vazia.
  for (i = 0; i < folder->nSources; i++)
    if (baseDaFonte(&folder->sources[i])[0]) { source = tabCursor = i; break; }
  snprintf(titulo,sizeof titulo,"%s",folder->title);openSource();
}

void vertudo_abrir(const char *base, const char *tipo, const char *catId,
                   const char *tit) {
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
  desc_vertudo_abrir(base, tipo, catId);
}

int vertudo_aberta(void) { return aberta; }
int vertudo_pediu_abrir(void) { int v = pedAbrir; pedAbrir = -1; return v; }

static int nItens(void) { return desc_vertudo_n(); }

void vertudo_evento(const SDL_Event *e) {
  int n = nItens(), k;
  if (!aberta || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      e->key.keysym.scancode == NV_SCANCODE_BACK) { aberta = 0; return; }
  if(collection&&tabFocus) {
    if(k==SDLK_LEFT&&tabCursor>0)tabCursor--;
    if(k==SDLK_RIGHT&&tabCursor+1<collection->nSources)tabCursor++;
    if(k==SDLK_RETURN||k==SDLK_KP_ENTER){source=tabCursor;openSource();tabFocus=0;}
    if(k==SDLK_DOWN&&n>0)tabFocus=0;
    return;
  }
  if(k==SDLK_UP&&foco<VT_COLS&&collection){tabFocus=1;tabCursor=source;return;}
  if((k==SDLK_RETURN||k==SDLK_KP_ENTER)&&desc_vertudo_erro()){desc_vertudo_mais();return;}
  if (n < 1) return;
  if (k == SDLK_RIGHT && foco + 1 < n) foco++;
  else if (k == SDLK_LEFT && foco > 0) foco--;
  else if (k == SDLK_DOWN) { if (foco + VT_COLS < n) foco += VT_COLS;
                             else foco = n - 1; }
  else if (k == SDLK_UP) { if (foco >= VT_COLS) foco -= VT_COLS; }
  else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
    // O item da grade NAO esta no catalogo global — ele veio de uma pagina que
    // so esta tela leu. Entra por cat_acrescentar para que a tela de titulo
    // possa abri-lo por indice, que e como todo o app trabalha.
    CatItem it;
    if (viewItem(foco, &it)) {
      int idx = it.imdb[0] ? cat_indice_por_imdb(it.imdb) : -1;
      if (idx < 0) idx = cat_acrescentar(&it);
      if (idx >= 0) { pedAbrir = idx; } // conserva a lista e a posição ao voltar
    }
  }
  // Chegando perto do fim, pede a proxima pagina. Antes de o dono ver o vazio,
  // e nao quando ele ja esta olhando para ele.
  if (foco >= n - VT_COLS * 2) desc_vertudo_mais();
}

void vertudo_atualizar(float dt, Uint32 agora) {
  float alvo, maxY;
  int n = nItens(), linhas;
  (void)agora;
  anim = anim_mola(anim, aberta ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  if (!aberta) return;
  // A SONDA PODE CHEGAR DEPOIS DA TELA. Enquanto a fonte nao tem endereco,
  // reconferir por quadro custa uma varredura de poucas strings (addons_n e
  // dezenas, nao milhares) e SO acontece no estado de erro — assim a colecao se
  // preenche sozinha quando o manifesto responde, em vez de exigir sair e
  // entrar de novo.
  if (semFonte && collection) {
    const ColSource *s = &collection->sources[source];
    if (baseDaFonte(s)[0]) openSource();
  }
  for (int i = 0; i < COL_SOURCE_MAX; i++) {
    float alvoTab = collection && tabFocus && i == tabCursor ? 1.0f : 0.0f;
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
      TxtLinha falta = txt_linha_corta(TXT_HERO_META, "Sem pôster", 185, 191, 204, 255, r.w-24);
      txt_desenhar_alpha(falta, r.x+(r.w-falta.w)*.5f, r.y+(r.h-falta.h)*.5f, a);
    } }
  y += VT_PAN_ART_H + 18.0f;
  float badgeW=badges_desenhar(badges_provedor(it.provNome),VT_PAN_X,y,VT_PAN_W,28,a);
  if(badgeW>0)y+=40;

  // LOGO no lugar do titulo quando existe (max 264x82 no web); o nome escrito
  // com a fonte da interface so quando nao ha logo.
  { GLuint tl = it.logo[0] ? tex_obter_larg(it.logo, 264.0f) : 0;
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
  if (it.nota > 0) {
    char n[16];
    snprintf(n, sizeof n, "%.1f", it.nota / 10.0f);
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
static void themeBackground(float a) {
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
      const char *art=collection->hero[0]?collection->hero:collection->cover;
      GLuint tex=art[0]?tex_obter_hero(art):0;
      if(tex) {
        gfx_tex_aspect_atual=tex_aspecto(art);
        gfx_rect((GfxRect){0,0,NV_TELA_W,620},tex,GFX_HERO_CHEIO,
                 0,0,0,0,0,0,0,a*.38f);
        gfx_tex_aspect_atual=0;
      }
    }
  }
}

static void themeHeader(float a,float x0) {
  float r,g,b;corColecao(&r,&g,&b);
  TxtLinha eyebrow=txt_linha(TXT_HERO_META,rotuloGrupo(),197,202,211,255);
  txt_desenhar_alpha(eyebrow,x0,40,a);
  int ehDiretor=collection&&!strcasecmp(collection->group,"Directors");
  // O wordmark de uma coleção de diretores pode conter cabeça ou lettering
  // composto. No cabeçalho da filmografia, o nome textual e o retrato limpo
  // deixam a identidade legível sem duplicar a mesma informação visual.
  GLuint logo=!ehDiretor&&collection&&!collection->editorial&&collection->logo[0]
             ?tex_obter_larg(collection->logo,560):0;
  float aspect=logo?tex_aspecto(collection->logo):0;
  if(logo&&aspect>0) {
    // Wordmark oficial, grande o bastante para leitura a distancia. O PNG
    // transparente e importado em ate 800px, portanto 560px nao interpola para
    // cima nem perde a silhueta original da marca.
    float w=560.0f,h=w/aspect;if(h>108){h=108;w=h*aspect;}
    gfx_rect((GfxRect){x0,83,w,h},logo,tex_marca_escura(collection->logo)?GFX_MARCA:GFX_TEXTO,0,0,0,0,.96f,.97f,.98f,a);
  } else {TxtLinha title=txt_linha_corta(TXT_TITULO1,titulo,242,243,247,255,940);txt_desenhar_alpha(title,x0,80,a);}
  char caption[180];int n=nItens();
  if(semFonte)snprintf(caption,sizeof caption,"%s",
    "O addon desta coleção não está instalado nesta TV.");
  else if(desc_vertudo_erro())snprintf(caption,sizeof caption,"Não foi possível carregar. OK para tentar novamente.");
  else if(!n)snprintf(caption,sizeof caption,"%s",desc_vertudo_carregando()?"Carregando títulos…":"Nenhum título nesta lista.");
  else snprintf(caption,sizeof caption,i18n("%d títulos%s  ·  %s"),n,desc_vertudo_fim()?"":i18n(" carregados"),i18n(legendaGrupo()));
  TxtLinha sub=txt_linha_corta(TXT_DET_META2,caption,196,202,213,255,960);txt_desenhar_alpha(sub,x0,192,a);
  if(collection&&collection->nSources>1) {
    int first=tabCursor>3?tabCursor-3:0;
    gfx_recorte(x0-6,244,NV_TELA_W-x0-90,72);
    for(int i=first;i<collection->nSources&&i<first+6;i++) {
      float x=x0+(i-first)*322.0f;const ColSource *s=&collection->sources[i];
      int f=tabFocus&&tabCursor==i, selecionada=source==i;
      float fa=tabAnim[i], escala=1.0f+.025f*fa;
      GfxRect pill={x-(304*escala-304)*.5f,250-(58*escala-58)*.5f,
                    304*escala,58*escala};
      gfx_cor(pill,.28f,f?.94f:selecionada?r*.82f:.09f,
              f?.95f:selecionada?g*.82f:.10f,
              f?.97f:selecionada?b*.82f:.12f,a);
      if(!f) gfx_rect(pill,0,GFX_ANEL,0,1.5f/pill.h,0,.28f,
                      selecionada?.86f:.36f,selecionada?.88f:.38f,
                      selecionada?.92f:.43f,a*(selecionada?.72f:.35f));
      if(selecionada&&!f)
        gfx_cor((GfxRect){pill.x+18,pill.y+pill.h-4,pill.w-36,3},.5f,
                .84f+r*.16f,.84f+g*.16f,.84f+b*.16f,a);
      char label[180];snprintf(label,sizeof label,"%s · %s",s->title,i18n(!strcmp(s->type,"series")?"Séries":"Filmes"));
      TxtLinha t=txt_linha_corta(TXT_HERO_META,label,f?22:238,f?24:240,f?28:245,255,276);
      txt_desenhar_alpha(t,pill.x+(pill.w-t.w)*.5f,pill.y+(pill.h-t.h)*.5f,a);
    }gfx_sem_recorte();
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
  int n = nItens(), i;
  (void)agora;
  if (a < 0.01f) return;
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
    // MESMO raio dos cartazes da home: `posterCardCornerRadiusDp` (12dp x 2 =
    // 24px), fracao do MENOR lado porque o SDF do shader e normalizado. O
    // NV_RAIO_CARD fixo que estava aqui dava um canto diferente do resto do
    // app, e a grade lia como outra tela.
    float raio = ajustes_raio_poster_px() / VT_CARD_W;
    int sel = (i == foco);
    if (cy > NV_TELA_H || cy + VT_CARD_H + 40.0f < VT_TOPO - 12.0f) continue;
    if(timeline){timelineCard(i,cy,a,x0);continue;}
    if (!viewItem(i, &it)) continue;
    { GfxRect r = { cx, cy, VT_CARD_W, VT_CARD_H };
      if (sel && !tabFocus) {
        GfxRect anel = { cx - 4, cy - 4, VT_CARD_W + 8, VT_CARD_H + 8 };
        gfx_cor(anel, ajustes_raio_poster_px() / (VT_CARD_W + 8.0f), 1, 1, 1, a);
      }
      t = it.poster[0] ? tex_obter_larg(it.poster, VT_CARD_W)
        : (it.backdrop[0] ? tex_obter_larg(it.backdrop, VT_CARD_W) : 0);
      if (t) {
        gfx_tex_aspect_atual = tex_aspecto(it.poster[0] ? it.poster : it.backdrop);
        gfx_rect(r, t, GFX_CARD, sel ? 1.0f : 0.0f, 0, 0, raio, 0, 0, 0, a);
        gfx_tex_aspect_atual = 0.0f;
      } else {
        // Esqueleto enquanto a arte nao chega — a mesma cor do resto do app.
        gfx_cor(r, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G,
                NV_COR_ESQUELETO_B, a);
      } }
    { int c = sel ? 255 : 214;
      TxtLinha l = txt_linha_corta(TXT_DET_META2, it.titulo, c, c, c, 255,
                                   VT_CARD_W);
      txt_desenhar_alpha(l, cx, cy + VT_CARD_H + 10.0f, a * (sel ? 1.0f : 0.86f)); }
    if(ranked) {
      char rank[8];snprintf(rank,sizeof rank,"%d",i+1);
      TxtLinha edge=txt_linha(TXT_RANK,rank,234,236,241,255),ink=txt_linha(TXT_RANK,rank,17,18,22,255);
      float x=cx-10,y=cy+VT_CARD_H-edge.h;
      for(int dx=-2;dx<=2;dx+=2)for(int dy=-2;dy<=2;dy+=2)txt_desenhar_alpha(edge,x+dx,y+dy,a);
      txt_desenhar_alpha(ink,x,y,a);
    }
  }
  if(!n&&desc_vertudo_carregando())for(int i=0;i<5;i++)
    gfx_cor((GfxRect){x0+i*264,VT_TOPO,248,372},.06f,.12f,.13f,.15f,a);
  gfx_sem_recorte();

  // CABECALHO por cima do recorte, entao ele nunca compete com a arte.
  themeHeader(a,x0);

  if (n > 0) painel(a);
}
