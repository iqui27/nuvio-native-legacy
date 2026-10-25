#include "filmo.h"
#include "filmodados.h"
#include "pessoa.h"
#include "plrui.h"
#include "text.h"
#include "tex_cache.h"
#include "focoprof.h"
#include "idioma.h"
#include "layout.h"
#define NV_TXT_TELA
#include "tela21.h"
#include "conquistas.h"
#include "caca.h"
#include "ilha.h"
#include "ilhaacao.h"
#include "salvos.h"
#include "catalogo.h"
#include "trailer.h"
#include "ponteiro.h"
#include "home.h"
#include "revela.h"
#include "svdesenho.h"
#include "teclavoltar.h"
#include "botoes.h"
#include "corviva.h"
#include "descoberta.h"
#include "idiomacod.h"
#include "badges.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
// Approved prototype: header, four tabs, chapters/year rail, nine-column grid.
static int aba,zona,col,cap,ano,ruido,topo,camada,op,selecionado=-1,pedido=-1,bioPagina;
static int indices[FM_MAX],ni,anos[FM_MAX],na;
static FmFiltro filtro;
static FmQuiz quiz;
static int como;
static void preparar(void);
static int gradeCols(void);
static float gradePasso(void);
static float anoPasso(void);
static float alpha, entrada, tempo, scroll, scrollV, anoScroll, anoV, abaSel, abaFoco;
static int folhaAtiva;
static int saindo, ultimaAba = -1, folhaAnterior;
static float anosAlpha[FM_MAX];
static long pessoaLembrada = -1;
static float focoBtns[6][FM_MAX], focoCred[FM_MAX], focoAnos[FM_MAX];
static int drawZona, drawItem;
static float compactoT;
// Hero: kicker, name, meta, action row, tabs (authored y at 1080p).
#define HERO_KICK 92
#define HERO_NOME 112
#define HERO_META 214
#define HERO_ACOES 274
#define HERO_ABAS 372
#define CONTEUDO_Y 448
#define ANO_S 78.0f
#define HIST_POSTER_W 184.0f
#define HIST_POSTER_H 276.0f
#define FOLHA_Y 222
#define FOLHA_P 86
static float aberto, tAba, anoSel = -1, anoFoco;
static int abaVista = -1;
static GfxRect acoesR[3];
static char contTxt[4][16];
static float chipX[8], chipW[8];
static int atualizar(float dt) {
  preparar();
  int red = ajustes_animacoes_reduzidas();
  entrada = red ? !saindo : anim_mola(entrada, !saindo, dt, NV_MOLA_TELA);
  if (camada) folhaAnterior = camada;
  if (ultimaAba != aba) {
    tempo = 0; ultimaAba = aba;
    for (int i = 0; i < na; i++) anosAlpha[i] = anos[i] == ano;
  }
  // Retarget every visible year without discarding outgoing text or artwork.
  for (int i = 0; i < na; i++)
    anosAlpha[i] = red ? anos[i] == ano : anim_mola(anosAlpha[i], anos[i] == ano, dt, NV_MOLA_PAGINA);
  if (abaVista != aba) { tAba = 0; abaVista = aba; }
  tempo += dt; tAba += dt; aberto += dt;
  scroll = anim_mola2_reduzida(&scrollV, scroll, topo * gradePasso(), dt, NV_MOLA2_SCROLL, red);
  int ai = 0; while (ai < na && anos[ai] != ano) ai++;
  int start = ai - 10; if (start < 0) start = 0;
  if (start > na - 21) start = na > 21 ? na - 21 : 0;
  anoScroll = anim_mola2_reduzida(&anoV, anoScroll, start * anoPasso(), dt, NV_MOLA2_SCROLL, red);
  anoSel = red || anoSel < 0 ? ai : anim_mola(anoSel, ai, dt, NV_MOLA_FOCO);
  anoFoco = red ? zona == 3 && !camada : anim_mola(anoFoco, zona == 3 && !camada, dt, NV_MOLA_FOCO);
  abaSel = red ? aba : anim_mola(abaSel, aba, dt, NV_MOLA_FOCO);
  abaFoco = red ? zona == 1 && !camada : anim_mola(abaFoco, zona == 1 && !camada, dt, NV_MOLA_FOCO);
  for (int z = 0; z < 6; z++) for (int i = 0; i < FM_MAX; i++)
    focoBtns[z][i] = red ? (!saindo && (camada ? z == 5 && i == op : z == zona && i == col)) : anim_mola(focoBtns[z][i], !saindo && (camada ? z == 5 && i == op : z == zona && i == col), dt, NV_MOLA_FOCO);
  for (int i = 0; i < FM_MAX; i++) {
    focoCred[i] = red ? (!camada && zona == 4 && col < ni && indices[col] == i) : anim_mola(focoCred[i], !camada && zona == 4 && col < ni && indices[col] == i, dt, NV_MOLA_FOCO);
    focoAnos[i] = red ? (zona == 3 && i < na && anos[i] == ano) : anim_mola(focoAnos[i], zona == 3 && i < na && anos[i] == ano, dt, NV_MOLA_FOCO);
  }
  { float q = aba == 1 && zona == 4; compactoT = red ? q : anim_mola(compactoT, q, dt, NV_MOLA_FOCO); }
  if(saindo && entrada < .002f) { folhaAtiva=0; return 1; }
  return 0;
}
int filmo_atualizar(float dt) {
  TXT_INI(); int saiu = atualizar(dt); TXT_FIM(); return saiu;
}
static float paginaX(void) { float x; t21_inicio(&x, NULL); return aba==0?ANC(120)-72.0f:x-72.0f; }
// Reflow authored columns and row anchors; text keeps the selected scale.
static float paginaW(void) { float w; t21_inicio(NULL, &w); return w; }
static float colunaK(void) { return aba==0?ANC(1):paginaW()/1788.0f; }
static float linhaK(void) { return fminf(1, (ANC(T21_SAFE_B) - 60) / 970.0f); }
static float px(float x) { return 72 + (x - 72) * colunaK(); }
static float anoPasso(void) { return na>1&&na<=21?1590.0f/(na-1):ANO_S; }
static float py(float y) { return aba==0 ? ANC(y) : 60+(y-60)*linhaK(); }
static float compacta(float full,float small){return full+(small-full)*compactoT;}
static GfxRect area(GfxRect r) { return (GfxRect){px(r.x), py(r.y), r.w * colunaK(), r.h * linhaK()}; }
static void material(GfxRect r,float radius,int modal,float a) { plrui_material(area(r),radius,modal,a); }
static void trilho(GfxRect r,float v,float cr,float cg,float cb,float a) { plrui_trilho(area(r),v,cr,cg,cb,a); }
static int gradeCols(void) { return (int)((paginaW() + 24) / (220 * gfx_poster_k() + 24)); }
static float gradePasso(void) { return 330 * gfx_poster_k() + 100; }
static int gradeLinhas(void) { return aba==1 && zona==4 && ANC(T21_SAFE_B) > 330 + 2 * gradePasso() ? 2 : 1; }
static const char *abas[]={"História","Filmografia","Sobre","Conquistas"};
static void texto(const char *s,float x,float y,float w,TxtEstilo font,int cor){TxtLinha l=txt_linha_corta(font,i18n(s),cor,cor,cor,255,w*colunaK());txt_desenhar_alpha(l,px(x),py(y),alpha);}
static void bloco(const char *s,float x,float y,float w,int n){txt_bloco(TXT_ILHA_SUB,i18n(s),190,195,205,px(x),py(y),w*colunaK(),29,alpha,n);}
// Shared borderless glass, with the same accent focus spring as other pills.
static void repouso(GfxRect r, float a) {
  if (ajustes_vidro()) gfx_vidro_painel_sem_aro(r, .5f, .55f, a);
  else plrui_botao_repouso(r, a);
}
// Chip at NV_CTRL_H; w <= 0 measures the label. Returns the authored width.
static float botao(const char *s,float x,float y,float w,int foco) {
  (void)foco;
  float f = focoBtns[drawZona][drawItem++];
  if (w <= 0) w = botao_largura(i18n(s), NULL, 0) / colunaK();
  GfxRect r = {px(x), py(y), w * colunaK(), T21_CTRL_H};
  repouso(r, alpha);
  plrui_pilula_foco(r, alpha * f);
  TxtLinha l = txt_linha_corta(TXT_DET_BOTAO, i18n(s), f >= .5f ? plrui_tinta() : 230,
    f >= .5f ? plrui_tinta() : 230, f >= .5f ? plrui_tinta() : 230, 255, r.w - 2 * BOTAO_PAD_X2 + 2);
  txt_desenhar_alpha(l, r.x + BOTAO_PAD_X2, r.y + (r.h - l.h) / 2, alpha);
  return w;
}
// Chip like the mock's .chip: accent dot when on, dim prefix before the label; dir = x is the right edge.
static float chip(const char *v,const char *s,int on,float x,float y,int dir){
  float f=focoBtns[drawZona][drawItem++],ar,ag,ab;int fo=f>=.5f,ink=fo?plrui_tinta():on?255:230;
  TxtLinha lv={0},ls=txt_linha(TXT_ILHA_SEG,s,ink,ink,ink,255);if(v)lv=txt_linha(TXT_ILHA_SUB,v,ink,ink,ink,fo?190:150);
  float w=2*BOTAO_PAD_X2+(on?17:0)+(lv.w?lv.w+10:0)+ls.w;
  GfxRect r={px(x)-(dir?w:0),py(y),w,T21_CTRL_H};
  repouso(r,alpha);plrui_pilula_foco(r,alpha*f);
  float xx=r.x+BOTAO_PAD_X2,yc=r.y+r.h/2;
  if(on){if(fo)ar=ag=ab=ink/255.f;else ajustes_acento(&ar,&ag,&ab);gfx_cor((GfxRect){xx,yc-3.5f,7,7},.5f,ar,ag,ab,alpha);xx+=17;}
  if(lv.w){txt_desenhar_alpha(lv,xx,yc-lv.h/2,alpha);xx+=lv.w+10;}
  txt_desenhar_alpha(ls,xx,yc-ls.h/2,alpha);
  return w/colunaK();
}
// Sheet action: the official primary pill, labels aligned like the poster menu.
static void acao(const char *s,const char *icone,float x,float y,float w) {
  float f = focoBtns[drawZona][drawItem++];
  botao_pilula_sem_aro((GfxRect){px(x), py(y), w * colunaK(), BOTAO_H_PRIMARIO}, i18n(s), icone, f, 1, 1, alpha);
}
static float sobe(float t,int k){return ajustes_animacoes_reduzidas()?1:revela_entra(1,revela_onda_atraso(0,k),1+(Uint32)(t*1000));}
static void preparar(void){fd_atualizar();const FdFicha *d=fd_ficha();na=0;for(int i=0;i<d->n;i++){const FmCredito *c=d->c+i;if((!ruido&&c->ruido)||!c->ano)continue;int j=0;while(j<na&&anos[j]!=c->ano)j++;if(j==na)anos[na++]=c->ano;}for(int i=1;i<na;i++){int a=anos[i],j=i;while(j&&anos[j-1]>a){anos[j]=anos[j-1];j--;}anos[j]=a;}if(na&&(!ano||ano<anos[0]))ano=anos[0];
  int all[FM_MAX];int n=fm_filtrar(d->c,d->st,d->n,&filtro,all,FM_MAX);ni=0;for(int i=0;i<n;i++){int k=all[i];if(!ruido&&d->c[k].ruido)continue;if(aba==0&&d->c[k].ano!=ano)continue;indices[ni++]=k;}if(zona==4&&col>=ni)col=ni?ni-1:0;
}
void filmo_abrir(void) {
  if (pessoaLembrada != pessoa_id()) {
    aba=col=cap=ano=ruido=topo=bioPagina=0; zona=3;
    memset(&filtro,0,sizeof filtro); pessoaLembrada=pessoa_id(); anoSel=-1; selecionado=-1;
  }
  if(folhaAtiva && saindo) { saindo=0; return; }
  camada=0; pedido=-1; saindo=0; entrada=tempo=aberto=0; abaVista=ultimaAba=-1;
  folhaAtiva=1;
  memset(focoBtns,0,sizeof focoBtns); memset(focoCred,0,sizeof focoCred);
  compactoT=aba==1&&zona==4;
  scroll=topo*gradePasso(); scrollV=anoV=0; fd_abrir(); fd_sobre();
}
int filmo_titulo(void){int p=pedido;pedido=-1;return p;}
int filmo_foco(void){return zona*1000+col;}
int filmo_aba(void){return aba;}
int filmo_camada(void){return camada;}
static void folha(int i){if(i<0)return;selecionado=i;camada=1;op=0;fd_enriquecer(i);}
static int capitulos(const FdFicha *d,FmCap *caps){FmMarco m[10];int nm=fm_marcos(d->c,d->n,m,10);return fm_capitulos(d->c,d->n,m,nm,caps,6);}
static int ruidoN(const FdFicha *d){int n=0;for(int i=0;i<d->n;i++)n+=d->c[i].ruido!=0;return n;}
// Sheet options for the selected title, prototype rules: "Já vi" while unseen, "Desfazer" only on a manual mark.
static int opcoes(int *o){const FmEstado *s=fd_ficha()->st+selecionado;int n=0;o[n++]=0;if(s->estado!=FM_VISTO||(s->manual&&s->como==FM_COMO_DIRETO))o[n++]=1;o[n++]=2;if(s->manual)o[n++]=3;o[n++]=4;return n;}
static int evento(const SDL_Event *e){if(e->type!=SDL_KEYDOWN||e->key.repeat)return 0;preparar();const FdFicha *d=fd_ficha();SDL_Keycode k=e->key.keysym.sym;int back=nv_tecla_voltar(e);int ok=k==SDLK_RETURN||k==SDLK_KP_ENTER;
  if(conquistas_aviso_evento(e))return 0;
  if(back){if(camada){camada=0;op=0;trailer_fechar();return 0;}if(zona!=1){zona=1;col=aba;return 0;}saindo=1;return 0;}
  if(camada){
    if(camada==5){if(k==SDLK_DOWN)bioPagina++;if(k==SDLK_UP&&bioPagina)bioPagina--;if(ok)camada=0;return 0;}
    int o[5],no=camada==1?opcoes(o):0,max=camada==1?no:camada==2?6:camada==3?3:3;
    if(k==SDLK_DOWN&&op+1<max)op++;if(k==SDLK_UP&&op)op--;
    if(!ok)return 0;
    if(camada==1){int id=o[op];if(id==0){pedido=selecionado;camada=0;}else if(id==1){if(fd_ficha()->st[selecionado].estado!=FM_EM_BREVE&&fd_preparado(selecionado)&&fd_tentativas(selecionado)){como=FM_COMO_DIRETO;op=0;camada=fd_quiz(selecionado,&quiz)?2:3;}}else if(id==2){const FmCredito *c=d->c+selecionado;int i=cat_indice_por_imdb(c->imdb);if(i>=0){const CatItem *ci=cat_item(i);salvos_definir(ci,!ci->naLista);cat_definir_na_lista(i,!ci->naLista);}}else if(id==3){if(fd_marcar(selecionado,0,0,1))camada=0;}else camada=0;}
    else if(camada==2){if(op<4){fm_quiz_responder(&quiz,op);op=0;if(quiz.resultado==1){como=FM_COMO_QUIZ;camada=3;}else if(quiz.resultado==2){fd_falhou(selecionado);camada=4;}}else if(op==4){fm_quiz_trocar(&quiz);op=0;}else{como=FM_COMO_DIRETO;camada=3;op=0;}}
    else if(camada==3){if(op==2)camada=0;else if(fd_marcar(selecionado,como,op==1,0))camada=0;}
    else if(camada==4){if(op==0){como=FM_COMO_DIRETO;camada=3;op=0;}else if(op==1&&fd_tentativas(selecionado)&&fd_quiz(selecionado,&quiz)){camada=2;op=0;}else camada=0;}
    return 0;
  }
  if(k==SDLK_UP){if(zona==4){int cols=aba==0?5:gradeCols();if(col>=cols){col-=cols;topo=col/cols;}else{zona=aba==0?3:2;col=0;}}else if(zona>0){zona--;col=zona==1?aba:0;}return 0;}
  if(k==SDLK_DOWN){if(zona==0){zona=1;col=aba;}else if(zona==1){zona=2;col=0;}else if(aba==0&&zona==2){zona=3;col=0;}else if((aba==0||aba==1)&&zona<4){zona=4;col=0;}else if(zona==4){int cols=aba==0?5:gradeCols();if(col+cols<ni){col+=cols;if(col/cols>=topo+gradeLinhas())topo=col/cols-gradeLinhas()+1;}}return 0;}
  if(aba==0&&(k==SDLK_PAGEUP||k==SDLK_PAGEDOWN)){FmCap caps[6];int n=capitulos(d,caps),i=0;while(i+1<n&&caps[i+1].de<=ano)i++;i+=k==SDLK_PAGEDOWN?1:-1;if(i>=0&&i<n){ano=caps[i].de;zona=3;col=0;}return 0;}
  if(k==SDLK_LEFT||k==SDLK_RIGHT){int step=k==SDLK_RIGHT?1:-1;if(zona==1){aba=(aba+4+step)%4;col=aba;topo=0;filtro.ano=0;if(aba==2)fd_sobre();}else if(zona==3&&aba==0){int j=0;while(j<na&&anos[j]!=ano)j++;if(j+step>=0&&j+step<na)ano=anos[j+step];}else{FmCap caps[6];int max=zona==0?3:zona==4?ni:aba==1?5:aba==0?capitulos(d,caps)+(ruidoN(d)>0):1;if(col+step>=0&&col+step<max)col+=step;}return 0;}
  if(ok){if(zona==0){if(col==1)fd_seguir();else if(col==2){int dir=!strcmp(pessoa_area(),"Direção"),i;caca_previa(dir?CACA_DIRECAO:CACA_ATUACAO,d->pessoa,pessoa_nome(),pessoa_foto());i=caca_previa_cacar();if(i>=0){CacaInfo c;if(caca_info(i,&c)){IlhaAcao ac;char frase[160];snprintf(frase,sizeof frase,i18n("Caçando %s"),c.nome);memset(&ac,0,sizeof ac);ac.icone="aj_crosshair";ac.frase=frase;ac.titulo=c.nome;ac.onde=i18n("Perfil · Caça");ac.thumb=c.foto;ac.voar=c.foto[0]!=0;ac.tipo=ILHA_ACENTO;ilhaacao_feita(&ac);}}}else{FmConquista cq[4];int reason,level,n=fm_conquistas(d->pessoa,pessoa_nome(),d->c,d->st,d->n,cq);int nx=fm_proximo(d->c,d->st,d->n,FM_ATUA,cq,n,&reason,&level);if(nx<0){aba=1;zona=4;col=topo=0;}else folha(nx);}}else if(zona==4&&ni){if(aba==0&&col>=5){aba=1;filtro.ano=ano;zona=2;col=0;}else folha(indices[col]);}else if(zona==2){if(aba==1){if(col<4)fm_girar(&filtro,col);else ruido=!ruido;topo=0;}else if(aba==0){FmCap caps[6];int nc=capitulos(d,caps);if(col<nc){cap=col;ano=caps[cap].de;zona=3;}else ruido=!ruido;}else if(aba==2){camada=5;bioPagina=0;}}}
  return 0;
}
int filmo_evento(const SDL_Event *e) {
  TXT_INI(); int saiu = evento(e); TXT_FIM(); return saiu;
}
static void cartaz(int idx,float x,float y,float w,float h,int foco) {
  (void)foco;
  const FdFicha *d=fd_ficha(); const FmCredito *c=d->c+idx; const FmEstado *s=d->st+idx;
  int pos=0; while(pos<ni && indices[pos]!=idx) pos++;
  int cols=aba==0?5:gradeCols();
  float e=aba==0?sobe(tAba,2):ajustes_animacoes_reduzidas()?1:revela_entra(1,
    revela_onda_atraso(pos%cols,pos/cols-topo),1+(Uint32)(tempo*1000));
  w=aba==0?HIST_POSTER_W:220*gfx_poster_k(); h=aba==0?HIST_POSTER_H:330*gfx_poster_k();
  GfxRect r=area((GfxRect){x,y+(1-e)*NV_ENTRA_DY,w,h});
  r.w=w; r.h=h;
  CatItem ci={0}; snprintf(ci.titulo,sizeof ci.titulo,"%s",c->titulo);
  snprintf(ci.poster,sizeof ci.poster,"%s",c->poster);
  float f=focoCred[idx];
  GfxRect zoom=foco_zoom(r,f);
  // Optional "Foco no cartaz" border, drawn exactly like the Home poster row.
  if(f>.01f&&ajustes_borda_foco()){
    float raio=ajustes_raio_poster_px()/zoom.h,menor=zoom.w<zoom.h?zoom.w:zoom.h,ar,ag,ab;ajustes_acento(&ar,&ag,&ab);
    if(ajustes_vidro())gfx_vidro_cartao(zoom,raio,f,alpha*e);
    else gfx_cor((GfxRect){zoom.x-NV_ANEL_FOCO,zoom.y-NV_ANEL_FOCO,zoom.w+2*NV_ANEL_FOCO,zoom.h+2*NV_ANEL_FOCO},
      (raio*menor+NV_ANEL_FOCO)/(menor+2*NV_ANEL_FOCO),ar,ag,ab,f*alpha*e);
  }
  home_vitrine_cartao(&ci,zoom,zoom,alpha*e,0,f);
  if(s->estado==FM_VISTO)badges_check_cartao(zoom,0,alpha*e);
  r=zoom;
  char b[100];
  if(s->estado==FM_EM_BREVE)texto("Em breve",x+12,y+15,w-24,TXT_ILHA_SUB,220);
  else if(s->estado==FM_ANDA)trilho((GfxRect){x+8,y+h-12,w-16,5},s->pct/100.f,-1,-1,-1,alpha);
  float legenda=28;
  TxtLinha label=txt_linha_corta(TXT_ILHA_NOME,c->titulo,235,235,235,255,r.w);
  if(aba==0)legenda=txt_bloco(TXT_LOG_19B,c->titulo,235,235,235,r.x,r.y+r.h+12,r.w,18,alpha*e,2);
  else txt_desenhar_alpha(label,r.x,r.y+r.h+12,alpha*e);
  char papel[80];fm_rotulo(c,papel,sizeof papel,i18n);
  if(c->ano)snprintf(b,sizeof b,"%d · %s",c->ano,papel);else snprintf(b,sizeof b,"%s",papel);
  label=txt_linha_corta(aba==0?TXT_ILHA_HORA:TXT_ILHA_SUB,b,150,150,150,255,r.w);
  txt_desenhar_alpha(label,r.x,r.y+r.h+14+legenda,alpha*e);
}
// Mock's layered hero, tinted by the portrait palette (theme accent until it decodes).
static void heroi(float a){
  float r,g,b;ajustes_acento(&r,&g,&b);
  const char *f=pessoa_foto();CorvivaPaleta p;
  if(f[0]){tex_obter_larg(f,140);tex_publicar_paleta(f);if(corviva_paleta(f,&p)&&p.ok){r=p.acento[0];g=p.acento[1];b=p.acento[2];}}
  GfxRect h={0,0,NV_TELA_W,NV_TELA_H};
  float hi=fmaxf(r,fmaxf(g,b)),lo=fminf(r,fminf(g,b)),delta=hi-lo,hue=0;
  if(delta>.001f){hue=hi==r?(g-b)/delta:hi==g?2+(b-r)/delta:4+(r-g)/delta;hue/=6;if(hue<0)hue+=1;}
  gfx_rect(h,0,GFX_FILMO_FUNDO,0,hue,0,0,r,g,b,a);
}
// Continue pill: play, poster with progress, title and state, on the primary surface.
static float continuar(float x,float y,int next,const FdFicha *d,float f){
  const FmCredito *c=d->c+next;const FmEstado *st=d->st+next;char s[120],b[40];
  float prog=st->estado==FM_VISTO?1:c->tv&&c->eps?st->epsVistos/(float)c->eps:st->pct/100.f;
  if(c->tv&&c->eps)snprintf(b,sizeof b,i18n("%d de %d"),st->epsVistos,c->eps);else snprintf(b,sizeof b,"%d%%",st->pct);
  snprintf(s,sizeof s,"%s · %s",i18n(st->estado==FM_ANDA?"Continuar":"Próximo a ver"),b);
  TxtLinha t=txt_linha(TXT_DET_BOTAO,c->titulo,235,235,235,255),u=txt_linha(TXT_ILHA_SUB,s,235,235,235,255);
  float w=BOTAO_PAD_X2+BOTAO_ICONE+BOTAO_ICONE_GAP+36+16+t.w+12+u.w+BOTAO_PAD_X;
  GfxRect r={x,y,w,BOTAO_H_PRIMARIO};repouso(r,alpha);plrui_pilula_foco(r,alpha*f);
  int ink=f>=.5f?plrui_tinta():235;float k=ink/255.f,xx=x+BOTAO_PAD_X2;
  gfx_icone((GfxRect){xx,y+(r.h-BOTAO_ICONE)/2,BOTAO_ICONE,BOTAO_ICONE},"play",k,k,k,alpha);xx+=BOTAO_ICONE+BOTAO_ICONE_GAP;
  GfxRect pr={xx,y+6,36,54};GLuint tx=c->poster[0]?tex_obter_larg(c->poster,36):0;
  if(tx){gfx_tex_aspect_atual=tex_aspecto(c->poster);gfx_rect(pr,tx,GFX_CARD,0,0,0,.12f,0,0,0,alpha);gfx_tex_aspect_atual=0;}
  plrui_trilho((GfxRect){pr.x,pr.y+pr.h+3,pr.w,4},prog,-1,-1,-1,alpha);xx+=36+16;
  t=txt_linha(TXT_DET_BOTAO,c->titulo,ink,ink,ink,255);txt_desenhar_alpha(t,xx,y+(r.h-t.h)/2,alpha);xx+=t.w+12;
  u=txt_linha(TXT_ILHA_SUB,s,ink,ink,ink,150);txt_desenhar_alpha(u,xx,y+(r.h-u.h)/2,alpha);
  return w;
}
static void header(const FdFicha *d){drawZona=0;drawItem=0;FmResumo r;fm_resumo(d->c,d->st,d->n,&r);float c=compactoT,pa=alpha,fade=pa*(1-c),e;char s[320],t[160];
  if(c<.999f){
    e=sobe(aberto,0);alpha=fade*e;int core=0;for(int i=0;i<d->n;i++)core+=!d->c[i].ruido;snprintf(t,sizeof t,i18n("%d títulos"),core);
    s[0]=0;const char *roles[]={"Atuação","Direção","Roteiro"};
    for(int i=0;i<3;i++)if(r.papelTotal[i])snprintf(s+strlen(s),sizeof s-strlen(s),"%s%s",s[0]?" · ":"",i18n(roles[i]));
    snprintf(s+strlen(s),sizeof s-strlen(s),"%s%s · %s",pessoa_lugar()[0]?" · ":"",pessoa_lugar(),t);
    if(na)snprintf(s+strlen(s),sizeof s-strlen(s),na>1?" · %d–%d":" · %d",anos[0],anos[na-1]);
    plrui_kicker(s,px(72),py(HERO_KICK+(1-e)*NV_ENTRA_DY),205,205,215,alpha);
  }
  e=sobe(aberto,1);alpha=pa*e;
  { char up[200];idioma_maiusc(up,sizeof up,pessoa_nome());
    float y=py(compacta(HERO_NOME,40.0f-fmaxf(0,gfx_escala_txt()-1)*100.0f)+(1-e)*NV_ENTRA_DY);
    if(txt_tracking(TXT_LOG_COD,up,245,245,245,-1,0,0,3.52f)<=1500*colunaK())
      txt_tracking(TXT_LOG_COD,up,245,245,245,px(72),y,alpha,3.52f);
    else txt_desenhar_alpha(txt_linha_corta(TXT_LOG_COD,up,245,245,245,255,1500*colunaK()),px(72),y,alpha); }
  if(c<.999f){
    e=sobe(aberto,2);alpha=fade*e;
    // Meta: birth (and death), place-free; most voted two titles.
    int top[2]={-1,-1};for(int i=0;i<d->n;i++){if(d->c[i].ruido)continue;if(top[0]<0||d->c[i].votos>d->c[top[0]].votos){top[1]=top[0];top[0]=i;}else if(top[1]<0||d->c[i].votos>d->c[top[1]].votos)top[1]=i;}
    s[0]=0;if(pessoa_nascimento()[0]){
      int y=0,m=0,day=0,ey=0,em=0,ed=0;char born[80];time_t now=time(NULL);struct tm today;localtime_r(&now,&today);
      ey=today.tm_year+1900;em=today.tm_mon+1;ed=today.tm_mday;
      if(pessoa_morte()[0])sscanf(pessoa_morte(),"%d-%d-%d",&ey,&em,&ed);
      desc_data_extenso(pessoa_nascimento(),born,sizeof born);
      if(sscanf(pessoa_nascimento(),"%d-%d-%d",&y,&m,&day)==3)
        snprintf(s,sizeof s,i18n("Nascimento: %s · %d anos"),born,ey-y-(em<m||(em==m&&ed<day)));
      else snprintf(s,sizeof s,"%s",born);
    }
    if(pessoa_morte()[0]){desc_data_extenso(pessoa_morte(),t,sizeof t);snprintf(s+strlen(s),sizeof s-strlen(s)," – %s",t);}
    if(top[0]>=0)snprintf(s+strlen(s),sizeof s-strlen(s),"%s%s: %s%s%s",s[0]?"   ·   ":"",i18n("Mais votados"),d->c[top[0]].titulo,top[1]>=0?", ":"",top[1]>=0?d->c[top[1]].titulo:"");
    { TxtLinha l=txt_linha_corta(TXT_DET_META,s,215,215,222,255,1640*colunaK());txt_desenhar_alpha(l,px(72),py(HERO_META+(1-e)*NV_ENTRA_DY),alpha);
      FmConquista cq[FM_CQ_MAX];int n=fm_conquistas(d->pessoa,pessoa_nome(),d->c,d->st,d->n,cq),best=-1;
      for(int i=0;i<n;i++)if(cq[i].trilha&&cq[i].nivel&&(best<0||cq[i].nivel>cq[best].nivel))best=i;
      if(best>=0){const char *role=strstr(cq[best].nome," · ");snprintf(t,sizeof t,"%s · %s",i18n(fm_nivel_nome(cq[best].nivel)),role?role+4:cq[best].nome);
        char up[320];idioma_maiusc(up,sizeof up,t);
        float bx=px(72)+l.w+24,w=txt_tracking(TXT_AJ_CAPS13,up,207,148,61,-1,0,0,1.56f);
        if(bx+15+w<px(72)+paginaW()){
          TxtLinha label=txt_linha(TXT_AJ_CAPS13,up,207,148,61,255);
          float yc=py(HERO_META+(1-e)*NV_ENTRA_DY)+l.h/2;
          gfx_cor((GfxRect){bx,yc-3.5f,7,7},.5f,207/255.f,148/255.f,61/255.f,alpha);
          txt_tracking(TXT_AJ_CAPS13,up,207,148,61,bx+15,yc-label.h/2,alpha,1.56f);
        }}
    }
    e=sobe(aberto,3);alpha=fade*e;float y=py(HERO_ACOES+(1-e)*NV_ENTRA_DY),x=px(72);
    FmConquista cq[4];int reason,level,nc=fm_conquistas(d->pessoa,pessoa_nome(),d->c,d->st,d->n,cq);int next=fm_proximo(d->c,d->st,d->n,FM_ATUA,cq,nc,&reason,&level);
    float w=next>=0?continuar(x,y,next,d,focoBtns[0][0]):botao_largura(i18n("Ver todos"),NULL,1);
    if(next<0)botao_pilula_sem_aro((GfxRect){x,y,w,BOTAO_H_PRIMARIO},i18n("Ver todos"),NULL,focoBtns[0][0],1,0,alpha);
    acoesR[0]=(GfxRect){x,y,w,BOTAO_H_PRIMARIO};x+=w+BOTAO_GAP;
    const char *seg=i18n(d->seguindo?"Seguindo":"Seguir"),*ic=d->seguindo?"visto":"mais";w=botao_largura(seg,ic,1);
    botao_pilula_sem_aro((GfxRect){x,y,w,BOTAO_H_PRIMARIO},seg,ic,focoBtns[0][1],1,0,alpha);acoesR[1]=(GfxRect){x,y,w,BOTAO_H_PRIMARIO};x+=w+BOTAO_GAP;
    const char *cac=i18n(caca_achar(!strcmp(pessoa_area(),"Direção")?CACA_DIRECAO:CACA_ATUACAO,d->pessoa,"")>=0?"Na caçada":"Caçar");w=botao_largura(cac,"aj_crosshair",1);
    botao_pilula_sem_aro((GfxRect){x,y,w,BOTAO_H_PRIMARIO},cac,"aj_crosshair",focoBtns[0][2],1,0,alpha);acoesR[2]=(GfxRect){x,y,w,BOTAO_H_PRIMARIO};x+=w+32;
    // Watched progress beside the actions.
    float yc=y+BOTAO_H_PRIMARIO/2;plrui_trilho((GfxRect){x,yc-2,170,4},r.total?r.visto/(float)r.total:0,-1,-1,-1,alpha);x+=190;
    snprintf(s,sizeof s,i18n("Você já viu %d de %d"),r.visto,r.total);TxtLinha l=txt_linha(TXT_ILHA_SUB,s,240,240,245,255);
    snprintf(t,sizeof t,i18n("Atuação %d/%d · Direção %d/%d · Roteiro %d/%d"),r.papelVisto[0],r.papelTotal[0],r.papelVisto[1],r.papelTotal[1],r.papelVisto[2],r.papelTotal[2]);
    float fim=px(72)+paginaW();TxtLinha l2=txt_linha(TXT_ILHA_APOIO,t,165,165,175,255);
    // The mock's single line when it fits; at large text the breakdown stacks under the count, never cut.
    if(x+l.w+18+l2.w<=fim){txt_desenhar_alpha(l,x,yc-l.h/2,alpha);txt_desenhar_alpha(l2,x+l.w+18,yc-l2.h/2,alpha);}
    else{float y0=yc-(l.h+l2.h+2)/2;txt_desenhar_alpha(l,x,y0,alpha);l2=txt_linha_corta(TXT_ILHA_APOIO,t,165,165,175,255,fim-x);txt_desenhar_alpha(l2,x,y0+l.h+2,alpha);}
  }
  alpha=pa*sobe(aberto,4);
  const char *labels[4]; for(int i=0;i<4;i++) labels[i]=i18n(abas[i]);
  { FmConquista cq[4];int nq=fm_conquistas(d->pessoa,pessoa_nome(),d->c,d->st,d->n,cq),lv=0;for(int i=0;i<nq;i++)lv+=cq[i].nivel>0;
    int core=0;for(int i=0;i<d->n;i++)core+=ruido||!d->c[i].ruido;
    snprintf(contTxt[1],sizeof contTxt[1],"%d",core);if(nq)snprintf(contTxt[3],sizeof contTxt[3],"%d/%d",lv,nq);else contTxt[3][0]=0; }
  const char *cont[4]={NULL,contTxt[1],NULL,contTxt[3]};
  plrui_seg_txt(labels,cont,4,abaSel,abaFoco,px(72),py(compacta(HERO_ABAS,130)),alpha);
  alpha=pa;
}
// Year rail: one dot per year (white seen, grey unseen, halo milestone); selected year rides a sliding pill.
static void regua(const FdFicha *d,const FmMarco *m,int nm,float a){
  float yc=py(556);int start=(int)(anoScroll/anoPasso()+.5f),fim=start+21<na?start+21:na;
  if(na<1)return;
  float x0=px(170+start*anoPasso()-anoScroll),x1=px(170+(fim-1)*anoPasso()-anoScroll);
  gfx_cor((GfxRect){x0,yc-1,x1-x0,2},.5f,1,1,1,.18f*a);
  for(int i=start;i<fim;i++){
    float cx=px(170+i*anoPasso()-anoScroll),f=focoAnos[i];int vistos=0,marco=0;
    for(int k=0;k<d->n;k++)if(d->c[k].ano==anos[i]&&!d->c[k].ruido)vistos+=d->st[k].estado==FM_VISTO;
    for(int k=0;k<nm;k++)marco|=m[k].ano==anos[i];
    if(marco)gfx_cor((GfxRect){cx-14,yc-14,28,28},.5f,1,1,1,.16f*a);
    float dd=14*(1+.3f*f),l=vistos?.96f:.5f;gfx_cor((GfxRect){cx-dd/2,yc-dd/2,dd,dd},.5f,l,l,l,a);
  }
  char b[16];snprintf(b,sizeof b,"%d",anos[start]);TxtLinha l=txt_linha(TXT_DET_META2,b,165,165,175,255);txt_desenhar_alpha(l,x0-28-l.w,yc-l.h/2,a);
  snprintf(b,sizeof b,"%d",anos[fim-1]);l=txt_linha(TXT_DET_META2,b,165,165,175,255);txt_desenhar_alpha(l,x1+28,yc-l.h/2,a);
  int volume=0;for(int k=0;k<d->n;k++)volume+=d->c[k].ano==ano&&!d->c[k].ruido;
  char v[48];snprintf(v,sizeof v,i18n("%d títulos"),volume);snprintf(b,sizeof b,"%d",ano);
  int ink=anoFoco>=.5f?plrui_tinta():245;TxtLinha ly=txt_linha(TXT_ILHA_NOME,b,ink,ink,ink,255),lv=txt_linha(TXT_ILHA_SUB,v,ink,ink,ink,190);
  float w=ly.w+10+lv.w+44,cx=px(170+anoSel*anoPasso()-anoScroll);GfxRect r={cx-w/2,yc-26,w,52};
  repouso(r,a);plrui_pilula_foco(r,a*anoFoco);
  txt_desenhar_alpha(ly,r.x+22,yc-ly.h/2,a);txt_desenhar_alpha(lv,r.x+22+ly.w+10,yc-lv.h/2+1,a);
}
static void capitulo(const FdFicha *d,const FmMarco *m,int nm,const FmCap *caps,int nc,int anoCap){
  char b[900],k[160];int ci=0,lista[FM_MAX],n=0;
  for(int i=0;i<nc;i++)if(caps[i].de<=anoCap&&anoCap<=caps[i].ate)ci=i;
  for(int i=0;i<d->n;i++)if((ruido||!d->c[i].ruido)&&d->c[i].ano==anoCap)lista[n++]=i;
  float pa=alpha,e=sobe(tAba,2);alpha*=e;float dy=(1-e)*NV_ENTRA_DY;
  if(nc){snprintf(k,sizeof k,i18n("Capítulo %d de %d"),ci+1,nc);snprintf(b,sizeof b,"%s · %s",k,i18n(fm_capitulo_nome(caps[ci].tipo)));plrui_kicker(b,px(72),py(626+dy),190,190,200,alpha);}
  snprintf(b,sizeof b,"%d",anoCap);txt_tracking(TXT_FILMO_ANO,b,245,245,245,px(72),py(636+dy),alpha,1.68f);
  b[0]=0;
  for(int i=0;i<nm;i++)if(m[i].ano==anoCap){
    const FmCredito *c=d->c+m[i].idx;size_t off=strlen(b);
    if(m[i].tipo==FM_M_MAIS_VOTADO)snprintf(b+off,sizeof b-off,i18n("%s vira o trabalho mais votado da carreira (%d votos no TMDB)."),c->titulo,c->votos);
    else snprintf(b+off,sizeof b-off,"%s%s: %s%s%s. ",off?" ":"",i18n(fm_marco_nome(m[i].tipo)),c->titulo,c->personagem[0]?" · ":"",c->personagem);
  }
  if(!b[0]&&n){const FmCredito *c=d->c+lista[0];snprintf(b,sizeof b,"%s%s%s.",c->titulo,c->personagem[0]?" · ":"",c->personagem);}
  for(int i=0;i<d->np;i++)if(d->premios[i].ano==anoCap)snprintf(b+strlen(b),sizeof b-strlen(b)," %s: %s.",d->premios[i].rel[0]?d->premios[i].rel:i18n("Prêmios"),d->premios[i].nome);
  float h=txt_bloco(TXT_BODY,b,225,225,232,px(72),py(742+dy),560*colunaK(),30,alpha,4);
  float tx=px(72),ty=py(742+dy)+h+14;int tags=0;
  for(int i=0;i<nm;i++)if(m[i].ano==anoCap){char up[80];idioma_maiusc(up,sizeof up,i18n(fm_marco_nome(m[i].tipo)));TxtLinha tl=txt_linha(TXT_V2_KICK,up,235,235,240,255);float tw=tl.w+28;
    GfxRect tr={tx,ty,tw,tl.h+12};repouso(tr,alpha);txt_desenhar_alpha(tl,tx+14,ty+6,alpha);tx+=tw+10;tags=1;}
  for(int i=0;i<d->np;i++)if(d->premios[i].ano==anoCap){const char *label=d->premios[i].rel[0]?d->premios[i].rel:i18n("Prêmios");tx+=badge_desenhar(tx,ty,label,BADGE_NEUTRO,alpha)+BADGE_GAP;tags=1;}
  int vistos=0,no=0;
  if(nc){for(int i=0;i<d->n;i++){if((!ruido&&d->c[i].ruido)||d->c[i].ano<caps[ci].de||d->c[i].ano>caps[ci].ate)continue;no++;vistos+=d->st[i].estado==FM_VISTO;}snprintf(b,sizeof b,i18n("Neste capítulo você viu %d de %d."),vistos,no);}
  else{for(int i=0;i<n;i++)vistos+=d->st[lista[i]].estado==FM_VISTO;snprintf(b,sizeof b,i18n("%d de %d vistos"),vistos,n);}
  {TxtLinha l=txt_linha(TXT_DET_META2,b,160,160,170,255);txt_desenhar_alpha(l,px(72),ty+(tags?54:0),alpha);}
  alpha=pa;
  for(int i=0;i<n&&i<5;i++)cartaz(lista[i],676+i*(HIST_POSTER_W+24)*gfx_escala_txt(),622,HIST_POSTER_W,HIST_POSTER_H,zona==4&&col==i);
}
static void historia(const FdFicha *d){drawZona=2;drawItem=0;FmMarco m[10];FmCap caps[6];int nm=fm_marcos(d->c,d->n,m,10),nc=fm_capitulos(d->c,d->n,m,nm,caps,6),nr=ruidoN(d);float pa=alpha,e;
  char b[260],k[120];int ci=0;for(int i=0;i<nc;i++)if(caps[i].de<=ano&&ano<=caps[i].ate)ci=i;
  e=sobe(tAba,0);alpha=pa*e;float x=72,cy=CONTEUDO_Y+(1-e)*NV_ENTRA_DY;
  // The current chapter's chip carries its years; cameos sit right-aligned.
  for(int i=0;i<nc;i++){if(caps[i].ate>caps[i].de)snprintf(k,sizeof k,"%d–%d",caps[i].de,caps[i].ate);else snprintf(k,sizeof k,"%d",caps[i].de);
    chipX[i]=x;chipW[i]=chip(i==ci?k:NULL,i18n(fm_capitulo_nome(caps[i].tipo)),i==ci,x,cy,0);x+=chipW[i]+12/colunaK();}
  if(nr){snprintf(b,sizeof b,i18n(ruido?"Visíveis · %d":"Ocultas · %d"),nr);chipW[nc]=chip(i18n("Participações e entrevistas"),b,0,1800,cy,1);chipX[nc]=1800-chipW[nc];}
  alpha=pa*sobe(tAba,1);regua(d,m,nm,alpha);
  for(int i=0;i<na;i++)if(anosAlpha[i]>.002f){alpha=pa*anosAlpha[i];capitulo(d,m,nm,caps,nc,anos[i]);}
  alpha=pa;if(ni>5)botao("Ver todos",1640,970,0,zona==4&&col>=5);
}
static void grade(const FdFicha *d) {
  (void)d;drawZona=2;drawItem=0;
  const char *sort[]={"Mais recentes","Mais antigos","Mais votados"},*type[]={"Todos","Filmes","Séries"},*role[]={"Todas","Atuação","Direção","Roteiro","Voz"},*status[]={"Todos","Vistos","Não vistos"};
  const char *labels[]={sort[filtro.ordem],type[filtro.tipo],role[filtro.papel],status[filtro.status],ruido?"Participações visíveis":"Participações ocultas"};
  float y=compacta(CONTEUDO_Y,230);
  for(int i=0;i<5;i++)botao(labels[i],72+i*345,y,325,zona==2&&col==i);
  float cy=py(y)+100, step=gradePasso(),pw=220*gfx_poster_k();int cols=gradeCols();
  gfx_recorte(px(72)-12,cy-12,paginaW()+24,ANC(T21_SAFE_B)-cy);
  for(int i=0;i<ni;i++) {
    float yy=cy+(i/cols)*step-scroll;
    if(yy+step<cy||yy>ANC(T21_SAFE_B))continue;
    float authoredX=72+(i%cols)*(pw+24)/colunaK();
    float authoredY=60+(yy-60)/linhaK();
    cartaz(indices[i],authoredX,authoredY,pw,330*gfx_poster_k(),zona==4&&col==i);
  }
  gfx_sem_recorte();
  if(!ni)texto("Nenhum título com esses filtros. Mude um filtro acima.",72,y+170,1600,TXT_V2_GRUPO,185);
}
static void sobre(const FdFicha *d){drawZona=2;drawItem=0;material((GfxRect){72,465,660,520},28,0,alpha);material((GfxRect){756,465,500,520},28,0,alpha);material((GfxRect){1280,465,550,520},28,0,alpha);texto("Biografia · TMDB",100,495,600,TXT_ILHA_ITEM,235);char *bio=pessoa_bio();bloco(bio&&bio[0]?bio:"O TMDB ainda não tem biografia.",100,545,600,7);free(bio);botao("Ler biografia completa",100,775,580,zona==2);if(pessoa_bio_ingles())bloco("Sem biografia em português no TMDB; esta é a versão em inglês.",100,850,600,2);texto(pessoa_nascimento(),100,905,560,TXT_ILHA_SUB,165);texto(pessoa_outros(),100,940,560,TXT_ILHA_SUB,150);texto("Parcerias frequentes",785,495,440,TXT_ILHA_ITEM,235);int y=545;for(int i=0;i<d->npar;i++){texto(d->parcerias[i].nome,785,y,440,TXT_ILHA_NOME,220);y+=45;}if(!d->npar)bloco("Nenhuma parceria repetida ainda.",785,y,440,2);texto("Família · Wikidata",785,710,440,TXT_ILHA_ITEM,235);y=760;for(int i=0;i<d->nf&&i<4;i++){texto(d->familia[i].nome,785,y,440,TXT_ILHA_NOME,220);texto(d->familia[i].rel,785,y+30,440,TXT_ILHA_SUB,155);y+=65;}if(!d->nf)bloco("Sem dados no Wikidata.",785,y,440,2);texto("Prêmios · Wikidata",1310,495,480,TXT_ILHA_ITEM,235);y=545;for(int i=0;i<d->np&&i<5;i++){texto(d->premios[i].nome,1310,y,480,TXT_ILHA_NOME,225);texto(d->premios[i].rel,1310,y+30,480,TXT_ILHA_SUB,160);y+=80;}if(!d->np)bloco("Nenhum prêmio no Wikidata.",1310,y,480,2);}
static void conquistas(const FdFicha *d){FmConquista cq[4];int n=fm_conquistas(d->pessoa,pessoa_nome(),d->c,d->st,d->n,cq),tr=0;for(int i=0;i<n;i++){FmConquista *c=cq+i;if(!c->trilha)continue;float x=72+tr++*580;material((GfxRect){x,465,550,520},28,0,alpha);texto(c->nome,x+28,495,500,TXT_V2_GRUPO,230);char b[180];snprintf(b,sizeof b,i18n("%d de %d vistos"),c->visto,c->total);texto(b,x+28,545,500,TXT_ILHA_SUB,170);trilho((GfxRect){x+28,590,494,5},c->total?c->visto/(float)c->total:0,.85f,.69f,.35f,alpha);for(int nv=1;nv<=5;nv++){snprintf(b,sizeof b,"%d  %s",nv,i18n(fm_nivel_nome(nv)));texto(b,x+28,625+(nv-1)*68,490,TXT_ILHA_ITEM,c->nivel>=nv?235:140);}}
  float x=tr>1?1250:680;material((GfxRect){x,465,580,520},28,0,alpha);texto("Como conta",x+28,495,510,TXT_V2_GRUPO,235);bloco("Vale o histórico do Nuvio, do Trakt e do Simkl.",x+28,550,510,3);bloco("Já vi manual só conta se você passou nas perguntas.",x+28,650,510,3);bloco("Série conta quando você viu todos os episódios em que a pessoa aparece.",x+28,750,510,3);bloco("Em ordem pede datas reais. Participações e lançamentos futuros ficam fora.",x+28,860,510,3);}
static void overlay(const FdFicha *d){drawZona=5;drawItem=0;float x=1130,w=690;const FmCredito *c=selecionado>=0?d->c+selecionado:NULL;
  if(camada==1)plrui_kicker("Opções do título",px(x),py(84),175,178,190,alpha);
  texto(camada==5?pessoa_nome():c?c->titulo:"",x,108,w,T21_SHEET_TITLE,245);
  if(camada==5){char *bio=pessoa_bio();if(!bio)return;float height=txt_bloco(TXT_ILHA_SUB,bio,190,195,205,px(x),py(180),w*colunaK(),29,0,0);int pages=(int)(height/(696*linhaK()));if(bioPagina>pages)bioPagina=pages;GfxRect clip=area((GfxRect){x,180,w,696});gfx_recorte(clip.x,clip.y,clip.w,clip.h);txt_bloco(TXT_ILHA_SUB,bio,190,195,205,clip.x,clip.y-bioPagina*clip.h,clip.w,29,alpha,0);gfx_sem_recorte();free(bio);acao("Fechar",NULL,x,935,w);return;}
  if(camada==1){const FmEstado *s=d->st+selecionado;char b[180],e[80],p[240];snprintf(e,sizeof e,i18n("%d de %d episódios"),s->epsVistos,c->eps);
    snprintf(b,sizeof b,"%d",c->ano);snprintf(p,sizeof p,"%s%s%s%s%s",c->personagem,c->personagem[0]?" · ":"",c->ano?b:"",c->ano?" · ":"",c->tv?e:i18n(s->estado==FM_VISTO?"Visto":"Ainda não visto"));
    texto(p,x,166,w,TXT_DET_META2,175);
    const char *opts[]={"Abrir título",s->manual&&s->como==FM_COMO_DIRETO?"Responder para contar nas conquistas":"Já vi","Quero ver","Desfazer Já vi","Fechar"},*ic[]={"play","visto","mais",NULL,NULL};
    int o[5],n=opcoes(o);for(int i=0;i<n;i++)acao(opts[o[i]],ic[o[i]],x,FOLHA_Y+i*FOLHA_P,w);bloco("Passou nas perguntas, conta para as conquistas; sem responder, só aparece como visto.",x,FOLHA_Y+n*FOLHA_P+24,w,4);}
  else if(camada==2){const FmPergunta *q=fm_quiz_pergunta(&quiz);const char *questions[]={"Qual personagem esta pessoa interpreta?","Quem também está no elenco?","Qual é um tema deste título?","Qual cena pertence a este título?"};if(!q)return;bloco(questions[q->tipo],x,180,w,3);for(int i=0;i<4;i++)acao(q->opc[i],NULL,x,300+i*90,w);acao("Não lembro desta",NULL,x,700,w);acao("Marcar sem responder",NULL,x,790,w);bloco("Sem spoiler e sem tempo. Sair não gasta tentativa.",x,900,w,3);}
  else if(camada==3){texto("Quando você viu?",x,190,w,TXT_TITULO3,235);bloco("Vai para o histórico do Nuvio, do Trakt e do Simkl com esta data.",x,270,w,3);acao("Hoje",NULL,x,400,w);acao("Não lembro",NULL,x,530,w);char b[180];snprintf(b,sizeof b,i18n("Usa a data de lançamento: %s"),c->lancamento);bloco(b,x,605,w,3);acao("Agora não",NULL,x,730,w);bloco(como==FM_COMO_QUIZ?"Conta para conquistas: você passou nas perguntas.":"Não conta para conquistas: marcado sem as perguntas.",x,850,w,4);}
  else{texto("Tudo bem, às vezes faz tempo.",x,210,w,TXT_V2_GRUPO,230);bloco("Nada foi marcado. Ninguém mais vê isso.",x,280,w,3);acao("Marcar como visto mesmo assim",NULL,x,430,w);acao("Tentar com outras perguntas",NULL,x,550,w);acao("Fechar",NULL,x,670,w);}}
// The clock island owns the sheet's material, shape, veil and retained closing body.
// Content is laid out in the island's own layer and pinned to its right edge: it keeps its final place while the island grows.
static void toqueFoco(int z,int c);
static int toqueRolar(const PonteiroRolagem *e);
static void alvoFolha(float x,float y,float w,float h,int z,int c,float dx,float dy){GfxRect r=area((GfxRect){x,y,w,h});r.h=BOTAO_H_PRIMARIO;(void)h;ponteiro_alvo(r.x+dx,r.y+dy,r.w,r.h,toqueFoco,NULL,z,c);}
static void alvosFolha(int c,float dx,float dy){
  if(c==5){ponteiro_rolagem(toqueRolar);alvoFolha(1130,935,690,60,5,0,dx,dy);return;}
  int o[5],n=c==1?opcoes(o):c==2?6:3;
  for(int i=0;i<n;i++){float y=c==1?FOLHA_Y+i*FOLHA_P:c==2?(i<4?300+i*90:i==4?700:790):c==3?(i==0?400:i==1?530:730):(i==0?430:i==1?550:670);alvoFolha(1130,y,690,60,5,i,dx,dy);}
}
static void corpoFilmo(GfxRect r,float a,void *u) {
  (void)u;
  if(folhaAnterior!=5&&(selecionado<0||selecionado>=fd_ficha()->n))return;
  GfxRect fim=area((GfxRect){1090,50,770,980}),g=ilha_corpo_rect();
  float dx=r.x+r.w-(fim.x+fim.w),dy=r.y-fim.y;
  /* content appears only once the growing island holds it whole */
  float cob=fminf(g.w/fim.w,g.h/fim.h),rev=fminf(1,fmaxf(0,(cob-.96f)/.04f));
  int atual=camada; float pa=alpha;
  camada=folhaAnterior; alpha=a*entrada*rev;
  gfx_transformar(0,0,1,dx,dy);
  overlay(fd_ficha());
  gfx_sem_transformar();
  if(atual&&rev>.5f)alvosFolha(atual,dx,dy);
  camada=atual; alpha=pa;
}
static void toqueFoco(int z,int c){if(z==5){op=c;return;}zona=z;col=c;if(z==1&&aba!=c){aba=c;topo=0;if(aba==2)fd_sobre();}if(z==3)ano=c;}
static float gesto;
static int toqueRolar(const PonteiroRolagem *e){if(camada==5){if(e->fase==PONT_ROL_MOVER&&e->delta){bioPagina+=e->delta<0?1:-1;if(bioPagina<0)bioPagina=0;}return 1;}if(camada||!(aba==0||aba==1))return 0;if(e->fase==PONT_ROL_INICIO){gesto=0;return e->eixoY==(aba==1)&&e->y>=550;}if(e->fase==PONT_ROL_MOVER||e->fase==PONT_ROL_INERCIA){gesto-=e->delta;int step=(int)(gesto/(aba==1?350:-81));if(step){gesto-=step*(aba==1?350:81);if(aba==1){int max=(ni+gradeCols()-1)/gradeCols()-gradeLinhas();if(max<0)max=0;topo+=step;if(topo<0)topo=0;if(topo>max)topo=max;zona=4;col=topo*gradeCols();}else{int j=0;while(j<na&&anos[j]!=ano)j++;j+=step;if(j<0)j=0;if(j>=na)j=na-1;if(j>=0)ano=anos[j];}}return 1;}return 1;}
static void alvoToque(float x,float y,float w,float h,int z,int c){GfxRect r=area((GfxRect){x,y,w,h});if(h==60)r.h=60;ponteiro_alvo(r.x+paginaX(),r.y,r.w,r.h,toqueFoco,NULL,z,c);}
static void registrarToque(void){if(camada){ponteiro_camada();return;}float c=compactoT;if(c<.5f)for(int i=0;i<3;i++)ponteiro_alvo(acoesR[i].x+paginaX(),acoesR[i].y,acoesR[i].w,acoesR[i].h,toqueFoco,NULL,0,i);float tabX=72;for(int i=0;i<4;i++){float tw=txt_largura(TXT_ILHA_SEG,i18n(abas[i]))+40;if(contTxt[i][0])tw+=9+txt_largura(TXT_ILHA_NUM,contTxt[i]);alvoToque(tabX,compacta(HERO_ABAS,130),tw/colunaK(),54/linhaK(),1,i);tabX+=(tw+4)/colunaK();}if(aba==0){FmMarco m[10];FmCap caps[6];const FdFicha *d=fd_ficha();int nm=fm_marcos(d->c,d->n,m,10),nc=fm_capitulos(d->c,d->n,m,nm,caps,6)+(ruidoN(d)>0);for(int i=0;i<nc;i++)alvoToque(chipX[i],CONTEUDO_Y,chipW[i],60,2,i);int start=(int)(anoScroll/anoPasso()+.5f);for(int i=start;i<na&&i<start+21;i++)alvoToque(170+i*anoPasso()-anoScroll-38,500,76,80,3,anos[i]);for(int i=0;i<ni&&i<5;i++)alvoToque(676+i*(HIST_POSTER_W+24)*gfx_escala_txt(),622,HIST_POSTER_W/colunaK(),HIST_POSTER_H/linhaK(),4,i);if(ni>5)alvoToque(1640,970,botao_largura(i18n("Ver todos"),NULL,0)/colunaK(),60,4,5);ponteiro_rolagem(toqueRolar);}else if(aba==1){float y=compacta(CONTEUDO_Y,230);for(int i=0;i<5;i++)alvoToque(72+i*345,y,325,60,2,i);for(int i=topo*gradeCols();i<ni&&i<(topo+gradeLinhas())*gradeCols();i++)alvoToque(72+(i%gradeCols())*(220*gfx_poster_k()+24)/colunaK(),60+(py(y)+100+(i/gradeCols())*gradePasso()-scroll-60)/linhaK(),220*gfx_poster_k()/colunaK(),330*gfx_poster_k()/linhaK(),4,i);ponteiro_rolagem(toqueRolar);}else if(aba==2)alvoToque(100,775,580,60,2,0);}
void filmo_desenhar(float a){TXT_INI();alpha=a*entrada;preparar();const FdFicha *d=fd_ficha();gfx_cor((GfxRect){0,0,NV_TELA_W,NV_TELA_H},0,.043f,.047f,.055f,alpha);heroi(alpha);gfx_transformar(0,0,1,paginaX(),0);header(d);if(!d->pronto)t21_estado(area((GfxRect){72,465,1750,430}),d->erro?T21_ERROR:T21_LOADING,"aj_user",i18n(d->erro?"Serviço indisponível":"Carregando…"),NULL,NULL,tempo,alpha);else if(aba==0)historia(d);else if(aba==1)grade(d);else if(aba==2)sobre(d);else conquistas(d);if(camada){ESCALA_INI();GfxRect sheet=area((GfxRect){1090,50,770,980});ESCALA_FIM();ilha_corpo(sheet.w,sheet.h,corpoFilmo,NULL);}{TxtLinha l=txt_linha_corta(TXT_ILHA_SUB,i18n(fd_aviso()),180,180,180,255,1150*colunaK());txt_desenhar_alpha(l,px(72),ANC(T21_SAFE_B)-l.h/2,alpha);}const char *keys[]={"← →","PgUp PgDn","OK","Voltar"},*labels[]={i18n("Ano"),i18n("Capítulos"),i18n("Abrir opções"),i18n("Voltar")};plrui_dicas(keys,labels,4,px(72),ANC(T21_SAFE_B),0,alpha);gfx_sem_transformar();registrarToque();TXT_FIM();conquistas_aviso_desenhar();}
