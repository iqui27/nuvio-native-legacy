// Ordered trails: versioned Wikidata CC0 catalog, actual watched history.
#include "trilhas.h"
#include "ajustes.h"
#include "artemetahub.h"
#include "botoes.h"
#include "catalogo.h"
#include "conquistasdados.h"
#include "idioma.h"
#include "plrui.h"
#include "ponteiro.h"
#include "tex_cache.h"
#include "text.h"
#include "visto.h"
#define NV_ESCALA_TELA_ATIVA
#include "escala.h"
#include <stdio.h>
#include <string.h>

#define Q_W 1920.0f
#define Q_H 1080.0f
#define MG 148.0f
#define CARD_W 202.0f
#define GAP 30.0f
#define VISIVEIS 7

enum { B_VOLTAR = 1, B_RETOMAR, B_BADGE, B_VER, B_VISTO, P_PASSO0 = 100 };
static int aberto, foco, passo, primeiro, nvistos, pedidoTitulo, quer, sobreTitulo;
static const CqTrilha *tr;
static unsigned char visto[CQ_TRILHA_MAX];
static float ox, oy;

int trilhas_aberta(void) { return aberto; }
int trilhas_foco(void) { return foco; }
const char *trilhas_titulo(void) { return tr ? tr->nome : ""; }
int trilhas_passo(void) { return tr ? passo : -1; }
int trilhas_passos(void) { return tr ? tr->n : 0; }
int trilhas_total(void) { return trilhas_passos(); }
int trilhas_vistos(void) { return nvistos; }
int trilhas_primeiro_visivel(void) { return primeiro; }
void trilhas_teste_esquecer(void) {
  aberto = sobreTitulo = pedidoTitulo = quer = nvistos = primeiro = 0;
  foco = B_VOLTAR; passo = -1; tr = NULL;
}
static int reler(void) {
  int n = cqd_trilha_vistos(tr, visto, CQ_TRILHA_MAX);
  nvistos = 0;
  for (int i = 0; i < n; i++) nvistos += visto[i];
  return cqd_trilha_proximo(tr, visto, n);
}
static void enquadrar(void) {
  if (passo < primeiro) primeiro = passo;
  if (passo >= primeiro + VISIVEIS) primeiro = passo - VISIVEIS + 1;
}
int trilhas_abrir(const char *chave) {
  const CqTrilha *nova = cqd_trilha_buscar(chave);
  if (!nova || nova->n < 1 || nova->n > CQ_TRILHA_MAX) return 0;
  tr = nova;
  passo = reler();
  if (passo < 0) passo = tr->n - 1;
  aberto = 1; pedidoTitulo = quer = sobreTitulo = primeiro = 0;
  foco = P_PASSO0 + passo; enquadrar();
  return 1;
}
int trilhas_abrir_titulo(const char *imdb, const char *tipo) {
  const CqTrilha *ts[1];
  if (!cqd_trilhas_de(imdb, tipo, ts, 1) || !trilhas_abrir(ts[0]->id)) return 0;
  for (int i = 0; i < tr->n; i++) if (!strcmp(tr->p[i].imdb, imdb)) { passo = i; break; }
  foco = P_PASSO0 + passo; enquadrar();
  return 1;
}
int trilhas_pediu_titulo(CqPasso *dst) {
  if (!pedidoTitulo || !tr || !dst) return 0;
  *dst = tr->p[passo]; pedidoTitulo = 0; return 1;
}
void trilhas_voltar_do_titulo(void) {
  if (!sobreTitulo || !tr) return;
  sobreTitulo = 0; aberto = 1; reler(); enquadrar();
}
int trilhas_pediu_conquistas(char *chave, size_t tam) {
  if (!quer || !tr || !chave || !tam) return 0;
  snprintf(chave, tam, "trilha:%s", tr->id); quer = 0; return 1;
}
static void abrir_titulo(void) { pedidoTitulo = sobreTitulo = 1; aberto = 0; }
static void ok(void) {
  if (foco == B_VOLTAR) { aberto = 0; return; }
  if (foco == B_BADGE) { quer = 1; aberto = 0; return; }
  if (foco == B_RETOMAR) {
    int proximo = reler();
    if (proximo >= 0) passo = proximo;
    enquadrar(); abrir_titulo(); return;
  }
  if (foco == B_VISTO) {
    const char *id = tr->p[passo].imdb;
    int novo = !visto[passo];
    if (!visto_titulo(id, "movie", NULL, 0, novo, visto_destinos())) return;
    cat_historico_definir_id(id, "movie", novo);
    cqd_atualizar_local(); reler(); return;
  }
  if (foco == B_VER || foco >= P_PASSO0) abrir_titulo();
}
static void focar(int id, int b) {
  (void)b; foco = id;
  if (id >= P_PASSO0) { passo = id - P_PASSO0; enquadrar(); }
}
static void ativar(int id, int b) { focar(id, b); ok(); }
void trilhas_evento(const SDL_Event *e) {
  if (!aberto || !e || e->type != SDL_KEYDOWN) return;
  SDL_Keycode k = e->key.keysym.sym;
  if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE || k == SDLK_DELETE ||
      e->key.keysym.scancode == NV_SCANCODE_BACK) { aberto = 0; return; }
  if ((k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) && !e->key.repeat) { ok(); return; }
  if (foco >= P_PASSO0) {
    if (k == SDLK_LEFT && passo > 0) passo--;
    else if (k == SDLK_RIGHT && passo + 1 < tr->n) passo++;
    else if (k == SDLK_UP) { foco = B_RETOMAR; return; }
    else if (k == SDLK_DOWN) { foco = B_VER; return; }
    foco = P_PASSO0 + passo; enquadrar(); return;
  }
  if ((k == SDLK_UP && foco >= B_VER) || (k == SDLK_DOWN && foco <= B_BADGE)) {
    foco = P_PASSO0 + passo; return;
  }
  if (k == SDLK_LEFT) {
    if (foco == B_VISTO) foco = B_VER;
    else if (foco == B_BADGE) foco = B_RETOMAR;
    else if (foco == B_RETOMAR) foco = B_VOLTAR;
  } else if (k == SDLK_RIGHT) {
    if (foco == B_VER) foco = B_VISTO;
    else if (foco == B_VOLTAR) foco = B_RETOMAR;
    else if (foco == B_RETOMAR) foco = B_BADGE;
  }
}
static GfxRect em(float x, float y, float w, float h) { return (GfxRect){ox+x,oy+y,w,h}; }
static void texto(TxtEstilo est, const char *s, float x, float y, float w, int tom) {
  TxtLinha l = txt_linha_corta(est, s, tom, tom, tom, 255, w);
  txt_desenhar(l, ox+x, oy+y);
}
static void alvo(GfxRect r, int id) { ponteiro_alvo(r.x,r.y,r.w,r.h,focar,ativar,id,0); }
static void botao(float x, float y, const char *rotulo, const char *icone, int id) {
  float w = plrui_botao(x+ox,y+oy,rotulo,icone,foco == id,1);
  alvo(em(x,y,w,60),id);
}
int trilhas_poster(const CqPasso *p, char *url, size_t tam) {
  CatItem ci={0};
  if (!p || !url || !tam) return 0;
  cat_copiar_por_id(p->imdb,"movie",&ci);
  snprintf(ci.imdb,sizeof ci.imdb,"%s",p->imdb);
  arte_metahub_preencher(&ci);
  snprintf(url,tam,"%s",ci.poster);
  return url[0]!=0;
}
static void cartao(int i) {
  const CqPasso *p = &tr->p[i];
  float x = MG + 12 + (i-primeiro)*(CARD_W+GAP);
  float cr,cg,cb;
  ajustes_acento(&cr,&cg,&cb);
  GfxRect d=em(x+(CARD_W-44)/2,330,44,44), r=em(x,390,CARD_W,294);
  gfx_cor(d,.5f, visto[i]?cr:.09f,visto[i]?cg:.09f,visto[i]?cb:.10f,1);
  if (visto[i]) {
    float r,g,b;
    float tinta = ajustes_acento_tinta(&r,&g,&b);
    gfx_icone((GfxRect){d.x+10,d.y+10,24,24},"aj_check",tinta,tinta,tinta,1);
  }
  else { char b[12]; snprintf(b,sizeof b,"%d",i+1); texto(TXT_ILHA_NOME,b,x+(CARD_W-24)/2,338,40,230); }
  if (foco == P_PASSO0+i) plrui_linha_foco((GfxRect){r.x-8,r.y-8,r.w+16,r.h+16},18,1);
  char url[NV_ARTE_URL_MAX];
  trilhas_poster(p,url,sizeof url);
  GLuint tex=tex_obter_larg(url,CARD_W);
  if (tex) gfx_rect(r,tex,GFX_CARD,0,0,0,12/r.h,1,1,1,1);
  else {
    gfx_cor(r,12/r.h,.071f,.075f,.086f,1);
    gfx_icone((GfxRect){r.x+63,r.y+82,48,48},"aj_clapperboard",.45f,.47f,.50f,1);
  }
  texto(TXT_ILHA_SUB,p->titulo,x,688,CARD_W, visto[i]?160:239);
  char ano[12]; snprintf(ano,sizeof ano,"%d",p->ano);
  texto(TXT_V2_18,ano,x,718,CARD_W,150);
  alvo(r,P_PASSO0+i);
}
void trilhas_desenhar(Uint32 agora) {
  (void)agora;
  if (!aberto || !tr) return;
  ESCALA_REAL_INI();
  ox=(NV_TELA_W-Q_W)*.5f; oy=(NV_TELA_H-Q_H)*.5f;
  gfx_cor((GfxRect){0,0,NV_TELA_W,NV_TELA_H},0,.051f,.051f,.051f,1);
  botao(MG,32,i18n("Voltar"),"aj_corner-up-left",B_VOLTAR);
  texto(TXT_V2_KICK,i18n("Biblioteca › Trilhas"),MG,118,1100,150);
  const char *nome = !strcmp(tr->id,"oscar-melhor-filme") ? i18n("Os vencedores do Oscar de Melhor Filme") : i18n("Oscar de Melhor Filme Internacional");
  texto(TXT_V2_TIT,nome,MG,154,1624,243);
  texto(TXT_V2_24,i18n("Em ordem cronológica"),MG,222,1000,186);
  botao(1250,256,i18n("CONTINUAR"),"aj_play",B_RETOMAR);
  botao(1510,256,i18n("Ver o emblema"),"aj_star",B_BADGE);
  gfx_cor(em(MG+12,351,1590,3),0,1,1,1,.12f);
  for (int i=primeiro;i<tr->n && i<primeiro+VISIVEIS;i++) cartao(i);
  // Warm the next row after visible requests in the bounded async queue.
  for (int i=primeiro+VISIVEIS;i<tr->n && i<primeiro+VISIVEIS+3;i++) {
    char url[NV_ARTE_URL_MAX];
    trilhas_poster(&tr->p[i],url,sizeof url);
    tex_obter_larg(url,CARD_W);
  }
  GfxRect sobre=em(1348,766,424,266), painel=em(MG,766,1168,266);
  plrui_material_plano(painel,28,1); plrui_material_plano(sobre,28,1);
  const CqPasso *p=&tr->p[passo]; char b[256];
  snprintf(b,sizeof b,i18n("Passo %d · %s · %d"),passo+1,p->titulo,p->ano);
  texto(TXT_V2_KICK,b,MG+28,792,1110,186);
  txt_bloco_corta(TXT_V2_24,i18n("Filme que você já viu antes de abrir a trilha conta para os níveis 1 a 4."),
      186,192,204,ox+MG+28,oy+838,1110,30,1,2);
  botao(MG+28,946,i18n("Reproduzir"),"aj_play",B_VER);
  botao(MG+300,946,i18n(visto[passo]?"Desmarcar como assistido":"Marcar como visto"),"aj_check",B_VISTO);
  texto(TXT_V2_KICK,i18n("Como conta"),1376,792,368,186);
  snprintf(b,sizeof b,i18n("%d de %d filmes vistos"),nvistos,tr->n);
  texto(TXT_V2_24,b,1376,838,368,239);
  snprintf(b,sizeof b,i18n("Em andamento · nível %d de 5"),cqregra_nivel(nvistos,tr->n,0));
  texto(TXT_V2_24,b,1376,884,368,186);
  ESCALA_REAL_FIM();
}
