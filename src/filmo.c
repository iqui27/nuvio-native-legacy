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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
// Approved prototype: header, four tabs, chapters/year rail, nine-column grid.
static int aba,zona,col,cap,ano,ruido,topo,camada,op,selecionado=-1,pedido=-1,bioPagina;
static int indices[FM_MAX],ni,anos[FM_MAX],na;
static FmFiltro filtro;
static FmQuiz quiz;
static int como;
static void preparar(void);
static int gradeCols(void);
static float gradePasso(void);
static float alpha, entrada, tempo, scroll, scrollV, anoScroll, anoV, abaSel, abaFoco;
static int folhaAtiva;
static int saindo, ultimaAba = -1, ultimoAno, folhaAnterior;
static long pessoaLembrada = -1;
static float focoBtns[6][FM_MAX], focoCred[FM_MAX], focoAnos[FM_MAX];
static int drawZona, drawItem;
static int atualizar(float dt) {
  preparar();
  int red = ajustes_animacoes_reduzidas();
  entrada = red ? !saindo : anim_mola(entrada, !saindo, dt, NV_MOLA_TELA);
  if (camada) folhaAnterior = camada;
  if (ultimaAba != aba || ultimoAno != ano) { tempo = 0; ultimaAba = aba; ultimoAno = ano; }
  tempo += dt;
  scroll = anim_mola2_reduzida(&scrollV, scroll, topo * gradePasso(), dt, NV_MOLA2_SCROLL, red);
  int ai = 0; while (ai < na && anos[ai] != ano) ai++;
  int start = ai - 10; if (start < 0) start = 0;
  if (start > na - 21) start = na > 21 ? na - 21 : 0;
  anoScroll = anim_mola2_reduzida(&anoV, anoScroll, start * 81, dt, NV_MOLA2_SCROLL, red);
  abaSel = red ? aba : anim_mola(abaSel, aba, dt, NV_MOLA_FOCO);
  abaFoco = red ? zona == 1 && !camada : anim_mola(abaFoco, zona == 1 && !camada, dt, NV_MOLA_FOCO);
  for (int z = 0; z < 6; z++) for (int i = 0; i < FM_MAX; i++)
    focoBtns[z][i] = red ? (!saindo && (camada ? z == 5 && i == op : z == zona && i == col)) : anim_mola(focoBtns[z][i], !saindo && (camada ? z == 5 && i == op : z == zona && i == col), dt, NV_MOLA_FOCO);
  for (int i = 0; i < FM_MAX; i++) {
    focoCred[i] = red ? (!camada && zona == 4 && col < ni && indices[col] == i) : anim_mola(focoCred[i], !camada && zona == 4 && col < ni && indices[col] == i, dt, NV_MOLA_FOCO);
    focoAnos[i] = red ? (zona == 3 && i < na && anos[i] == ano) : anim_mola(focoAnos[i], zona == 3 && i < na && anos[i] == ano, dt, NV_MOLA_FOCO);
  }
  if(saindo && entrada < .002f) { folhaAtiva=0; return 1; }
  return 0;
}
int filmo_atualizar(float dt) {
  TXT_INI(); int saiu = atualizar(dt); TXT_FIM(); return saiu;
}
static float paginaX(void) { float x; t21_inicio(&x, NULL); return x - 72.0f; }
// Reflow authored columns and row anchors; text keeps the selected scale.
static float paginaW(void) { float w; t21_inicio(NULL, &w); return w; }
static float colunaK(void) { return paginaW() / 1788.0f; }
static float linhaK(void) { return fminf(1, (ANC(T21_SAFE_B) - 60) / 970.0f); }
static float px(float x) { return 72 + (x - 72) * colunaK(); }
static float py(float y) { return 60 + (y - 60) * linhaK(); }
static GfxRect area(GfxRect r) { return (GfxRect){px(r.x), py(r.y), r.w * colunaK(), r.h * linhaK()}; }
static void material(GfxRect r,float radius,int modal,float a) { plrui_material(area(r),radius,modal,a); }
static void trilho(GfxRect r,float v,float cr,float cg,float cb,float a) { plrui_trilho(area(r),v,cr,cg,cb,a); }
static int gradeCols(void) { return (int)((paginaW() + 24) / (220 * gfx_poster_k() + 24)); }
static float gradePasso(void) { return 330 * gfx_poster_k() + 100; }
static int gradeLinhas(void) { return aba==1 && zona==4 && ANC(T21_SAFE_B) > 330 + 2 * gradePasso() ? 2 : 1; }
static const char *abas[]={"História","Filmografia","Sobre","Conquistas"};
static void texto(const char *s,float x,float y,float w,TxtEstilo font,int cor){TxtLinha l=txt_linha_corta(font,i18n(s),cor,cor,cor,255,w*colunaK());txt_desenhar_alpha(l,px(x),py(y),alpha);}
static void bloco(const char *s,float x,float y,float w,int n){txt_bloco(TXT_ILHA_SUB,i18n(s),190,195,205,px(x),py(y),w*colunaK(),29,alpha,n);}
static void botao(const char *s,float x,float y,float w,int foco) {
  (void)foco;
  float f = focoBtns[drawZona][drawItem++];
  GfxRect r = {px(x), py(y), w * colunaK(), T21_CTRL_H};
  plrui_botao_repouso(r, alpha);
  plrui_pilula_foco(r, alpha * f);
  TxtLinha l = txt_linha_corta(TXT_DET_BOTAO, i18n(s), f >= .5f ? plrui_tinta() : 230,
    f >= .5f ? plrui_tinta() : 230, f >= .5f ? plrui_tinta() : 230, 255, r.w - 40);
  txt_desenhar_alpha(l, r.x + 20, r.y + (r.h - l.h) / 2, alpha);
}
static void preparar(void){fd_atualizar();const FdFicha *d=fd_ficha();na=0;for(int i=0;i<d->n;i++){const FmCredito *c=d->c+i;if((!ruido&&c->ruido)||!c->ano)continue;int j=0;while(j<na&&anos[j]!=c->ano)j++;if(j==na)anos[na++]=c->ano;}for(int i=1;i<na;i++){int a=anos[i],j=i;while(j&&anos[j-1]>a){anos[j]=anos[j-1];j--;}anos[j]=a;}if(na&&(!ano||ano<anos[0]))ano=anos[0];
  int all[FM_MAX];int n=fm_filtrar(d->c,d->st,d->n,&filtro,all,FM_MAX);ni=0;for(int i=0;i<n;i++){int k=all[i];if(!ruido&&d->c[k].ruido)continue;if(aba==0&&d->c[k].ano!=ano)continue;indices[ni++]=k;}if(zona==4&&col>=ni)col=ni?ni-1:0;
}
void filmo_abrir(void) {
  if (pessoaLembrada != pessoa_id()) {
    aba=col=cap=ano=ruido=topo=bioPagina=0; zona=3;
    memset(&filtro,0,sizeof filtro); pessoaLembrada=pessoa_id();
  }
  if(folhaAtiva && saindo) { saindo=0; return; }
  camada=0; selecionado=pedido=-1; saindo=0; entrada=tempo=0;
  folhaAtiva=1;
  memset(focoBtns,0,sizeof focoBtns); memset(focoCred,0,sizeof focoCred);
  scroll=topo*gradePasso(); scrollV=anoV=0; fd_abrir();
}
int filmo_titulo(void){int p=pedido;pedido=-1;return p;}
int filmo_foco(void){return zona*1000+col;}
int filmo_aba(void){return aba;}
int filmo_camada(void){return camada;}
static void folha(int i){if(i<0)return;selecionado=i;camada=1;op=0;fd_enriquecer(i);}
static int evento(const SDL_Event *e){if(e->type!=SDL_KEYDOWN||e->key.repeat)return 0;preparar();const FdFicha *d=fd_ficha();SDL_Keycode k=e->key.keysym.sym;int back=nv_tecla_voltar(e);int ok=k==SDLK_RETURN||k==SDLK_KP_ENTER;
  if(conquistas_aviso_evento(e))return 0;
  if(back){if(camada){camada=0;op=0;trailer_fechar();return 0;}if(zona!=1){zona=1;col=aba;return 0;}saindo=1;return 0;}
  if(camada){
    if(camada==5){if(k==SDLK_DOWN)bioPagina++;if(k==SDLK_UP&&bioPagina)bioPagina--;if(ok)camada=0;return 0;}
    int max=camada==1?5:camada==2?6:camada==3?3:3;
    if(k==SDLK_DOWN&&op+1<max)op++;if(k==SDLK_UP&&op)op--;
    if(!ok)return 0;
    if(camada==1){if(op==0){pedido=selecionado;camada=0;}else if(op==1){if(fd_ficha()->st[selecionado].estado!=FM_EM_BREVE&&fd_preparado(selecionado)&&fd_tentativas(selecionado)){como=FM_COMO_DIRETO;op=0;camada=fd_quiz(selecionado,&quiz)?2:3;}}else if(op==2){const FmCredito *c=d->c+selecionado;int i=cat_indice_por_imdb(c->imdb);if(i>=0){const CatItem *ci=cat_item(i);salvos_definir(ci,!ci->naLista);cat_definir_na_lista(i,!ci->naLista);}}else if(op==3){if(fd_marcar(selecionado,0,0,1))camada=0;}else camada=0;}
    else if(camada==2){if(op<4){fm_quiz_responder(&quiz,op);op=0;if(quiz.resultado==1){como=FM_COMO_QUIZ;camada=3;}else if(quiz.resultado==2){fd_falhou(selecionado);camada=4;}}else if(op==4){fm_quiz_trocar(&quiz);op=0;}else{como=FM_COMO_DIRETO;camada=3;op=0;}}
    else if(camada==3){if(op==2)camada=0;else if(fd_marcar(selecionado,como,op==1,0))camada=0;}
    else if(camada==4){if(op==0){como=FM_COMO_DIRETO;camada=3;op=0;}else if(op==1&&fd_tentativas(selecionado)&&fd_quiz(selecionado,&quiz)){camada=2;op=0;}else camada=0;}
    return 0;
  }
  if(k==SDLK_UP){if(zona==4){int cols=aba==0?5:gradeCols();if(col>=cols){col-=cols;topo=col/cols;}else{zona=aba==0?3:2;col=0;}}else if(zona>0){zona--;col=zona==1?aba:0;}return 0;}
  if(k==SDLK_DOWN){if(zona==0){zona=1;col=aba;}else if(zona==1){zona=2;col=0;}else if(aba==0&&zona==2){zona=3;col=0;}else if((aba==0||aba==1)&&zona<4){zona=4;col=0;}else if(zona==4){int cols=aba==0?5:gradeCols();if(col+cols<ni){col+=cols;if(col/cols>=topo+gradeLinhas())topo=col/cols-gradeLinhas()+1;}}return 0;}
  if(k==SDLK_LEFT||k==SDLK_RIGHT){int step=k==SDLK_RIGHT?1:-1;if(zona==1){aba=(aba+4+step)%4;col=aba;topo=0;filtro.ano=0;if(aba==2)fd_sobre();}else if(zona==3&&aba==0){int j=0;while(j<na&&anos[j]!=ano)j++;if(j+step>=0&&j+step<na)ano=anos[j+step];}else{int max=zona==0?3:zona==4?ni:aba==1?5:aba==0?6:1;if(col+step>=0&&col+step<max)col+=step;}return 0;}
  if(ok){if(zona==0){if(col==1)fd_seguir();else if(col==2){int dir=!strcmp(pessoa_area(),"Direção"),i;caca_previa(dir?CACA_DIRECAO:CACA_ATUACAO,d->pessoa,pessoa_nome(),pessoa_foto());i=caca_previa_cacar();if(i>=0){CacaInfo c;if(caca_info(i,&c)){IlhaAcao ac;char frase[160];snprintf(frase,sizeof frase,i18n("Caçando %s"),c.nome);memset(&ac,0,sizeof ac);ac.icone="aj_crosshair";ac.frase=frase;ac.titulo=c.nome;ac.onde=i18n("Perfil · Caça");ac.thumb=c.foto;ac.voar=c.foto[0]!=0;ac.tipo=ILHA_ACENTO;ilhaacao_feita(&ac);}}}else{FmConquista cq[4];int reason,level,n=fm_conquistas(d->pessoa,pessoa_nome(),d->c,d->st,d->n,cq);folha(fm_proximo(d->c,d->st,d->n,FM_ATUA,cq,n,&reason,&level));}}else if(zona==4&&ni){if(aba==0&&col>=5){aba=1;filtro.ano=ano;zona=2;col=0;}else folha(indices[col]);}else if(zona==2){if(aba==1){if(col<4)fm_girar(&filtro,col);else ruido=!ruido;topo=0;}else if(aba==0){FmMarco m[10];FmCap caps[6];int nm=fm_marcos(d->c,d->n,m,10),nc=fm_capitulos(d->c,d->n,m,nm,caps,6);if(col<nc){cap=col;ano=caps[cap].de;zona=3;}}else if(aba==2){camada=5;bioPagina=0;}}}
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
  float e=ajustes_animacoes_reduzidas()?1:revela_entra(1,
    revela_onda_atraso(pos%cols,pos/cols-topo),1+(Uint32)(tempo*1000));
  w=(aba==0?212:220)*gfx_poster_k(); h=(aba==0?322:330)*gfx_poster_k();
  GfxRect r=area((GfxRect){x,y+(1-e)*NV_ENTRA_DY,w,h});
  r.w=w; r.h=h;
  CatItem ci={0}; snprintf(ci.titulo,sizeof ci.titulo,"%s",c->titulo);
  snprintf(ci.poster,sizeof ci.poster,"%s",c->poster);
  float f=focoCred[idx];
  GfxRect zoom=foco_zoom(r,f);
  // Preserve the user's configured poster-border mode, as Home does.
  foco_anel(zoom,ajustes_raio_poster_px()/zoom.h,f,alpha*e);
  home_vitrine_cartao(&ci,zoom,zoom,alpha*e,0,f);
  r=zoom;
  char b[100];
  if(s->estado==FM_EM_BREVE)texto("Em breve",x+12,y+15,w-24,TXT_ILHA_SUB,220);
  else if(s->estado==FM_ANDA)trilho((GfxRect){x+8,y+h-12,w-16,5},s->pct/100.f,-1,-1,-1,alpha);
  TxtLinha label=txt_linha_corta(TXT_ILHA_NOME,c->titulo,235,235,235,255,r.w);
  txt_desenhar_alpha(label,r.x,r.y+r.h+12,alpha*e);
  fm_rotulo(c,b,sizeof b,i18n);
  label=txt_linha_corta(TXT_ILHA_SUB,b,150,150,150,255,r.w);
  txt_desenhar_alpha(label,r.x,r.y+r.h+42,alpha*e);
}
static void header(const FdFicha *d){drawZona=0;drawItem=0;FmResumo r;fm_resumo(d->c,d->st,d->n,&r);int compact=aba==1&&zona==4;char s[250];float x=compact?72:250;
  if(!compact){GfxRect photo=area((GfxRect){72,60,140,210});GLuint t=pessoa_foto()[0]?tex_obter_larg(pessoa_foto(),140):0;if(t){gfx_tex_aspect_atual=tex_aspecto(pessoa_foto());gfx_rect(photo,t,GFX_CARD,0,0,0,.06f,0,0,0,alpha);gfx_tex_aspect_atual=0;}else gfx_cor(photo,.06f,.12f,.13f,.15f,alpha);texto(pessoa_area(),x,62,1150,TXT_V2_KICK,160);texto(pessoa_lugar(),x,230,950,TXT_ILHA_SUB,155);snprintf(s,sizeof s,"%s%s%s",pessoa_nascimento(),pessoa_morte()[0]?" · ":"",pessoa_morte());texto(s,x,196,950,TXT_ILHA_SUB,185);}
  texto(pessoa_nome(),x,compact?40.0f-fmaxf(0,gfx_escala_txt()-1)*100.0f:100,1200,TXT_TITULO1,245);
  if(!compact){snprintf(s,sizeof s,i18n("Você já viu %d de %d"),r.visto,r.total);texto(s,1420,90,400,TXT_V2_GRUPO,235);trilho((GfxRect){1420,137,400,6},r.total?r.visto/(float)r.total:0,-1,-1,-1,alpha);snprintf(s,sizeof s,i18n("Atuação %d/%d · Direção %d/%d · Roteiro %d/%d"),r.papelVisto[0],r.papelTotal[0],r.papelVisto[1],r.papelTotal[1],r.papelVisto[2],r.papelTotal[2]);texto(s,1420,170,420,TXT_ILHA_SUB,160);FmConquista cq[4];int reason,level,nc=fm_conquistas(d->pessoa,pessoa_nome(),d->c,d->st,d->n,cq);int next=fm_proximo(d->c,d->st,d->n,FM_ATUA,cq,nc,&reason,&level);botao(next>=0?d->c[next].titulo:"Próximo a ver",250,265,420,zona==0&&col==0);if(next>=0){snprintf(s,sizeof s,"%s · %d%%",i18n("Próximo a ver"),d->st[next].pct);texto(s,250,335,620,TXT_ILHA_SUB,165);}botao(d->seguindo?"Seguindo":"Seguir",690,265,180,zona==0&&col==1);botao(caca_achar(!strcmp(pessoa_area(),"Direção")?CACA_DIRECAO:CACA_ATUACAO,d->pessoa,"")>=0?"Na caçada":"Caçar",890,265,160,zona==0&&col==2);}
  const char *labels[4]; for(int i=0;i<4;i++) labels[i]=i18n(abas[i]);
  plrui_seg(labels,NULL,4,abaSel,abaFoco,px(72),py(compact?130:365),alpha);
}
static void historia(const FdFicha *d){drawZona=2;drawItem=0;FmMarco m[10];FmCap caps[6];int nm=fm_marcos(d->c,d->n,m,10),nc=fm_capitulos(d->c,d->n,m,nm,caps,6);for(int i=0;i<nc;i++)botao(fm_capitulo_nome(caps[i].tipo),72+i*290,465,270,zona==2&&col==i);GfxRect rail={72,550,1750,95};material(rail,25,0,alpha);int start=0;for(int i=0;i<na;i++)if(anos[i]==ano)start=i-10;if(start<0)start=0;if(start>na-21)start=na>21?na-21:0;for(int i=start;i<na&&i<start+21;i++){char b[16];snprintf(b,sizeof b,"%d",anos[i]);float x=100+i*81-anoScroll;
    plrui_linha_foco(area((GfxRect){x-6,563,76,70}),20,alpha*focoAnos[i]);
    texto(b,x,579,76,TXT_ILHA_NOME,anos[i]==ano?255:135);int volume=0,vistos=0;for(int k=0;k<d->n;k++)if(d->c[k].ano==anos[i]&&!d->c[k].ruido){volume++;vistos+=d->st[k].estado==FM_VISTO;}snprintf(b,sizeof b,"%d/%d",vistos,volume);texto(b,x,609,76,TXT_ILHA_SUB,185);for(int k=0;k<nm;k++)if(m[k].ano==anos[i])gfx_icone(area((GfxRect){x+53,566,16,16}),"aj_star",1,1,1,alpha);}
  char b[150];snprintf(b,sizeof b,"%d",ano);texto(b,72,690,430,TXT_TITULO1,245);const char *frase="Trabalhos deste ano";for(int i=0;i<nm;i++)if(m[i].ano==ano)frase=fm_marco_nome(m[i].tipo);texto(frase,72,775,400,TXT_V2_GRUPO,225);snprintf(b,sizeof b,i18n("%d títulos neste ano"),ni);texto(b,72,850,400,TXT_ILHA_SUB,155);int vistos=0;for(int i=0;i<ni;i++)vistos+=d->st[indices[i]].estado==FM_VISTO;snprintf(b,sizeof b,i18n("%d de %d vistos"),vistos,ni);texto(b,72,892,400,TXT_ILHA_SUB,185);for(int i=0;i<ni&&i<5;i++)cartaz(indices[i],530+i*(212*gfx_poster_k()+24),670,212,322,zona==4&&col==i);if(ni>5)botao("Ver todos",1640,970,200,zona==4&&col>=5);
}
static void grade(const FdFicha *d) {
  (void)d;drawZona=2;drawItem=0;
  const char *sort[]={"Mais recentes","Mais antigos","Mais votados"},*type[]={"Todos","Filmes","Séries"},*role[]={"Todas","Atuação","Direção","Roteiro","Voz"},*status[]={"Todos","Vistos","Não vistos"};
  const char *labels[]={sort[filtro.ordem],type[filtro.tipo],role[filtro.papel],status[filtro.status],ruido?"Participações visíveis":"Participações ocultas"};
  float y=zona==4?230:465;
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
static void sobre(const FdFicha *d){drawZona=2;drawItem=0;material((GfxRect){72,465,660,550},28,0,alpha);material((GfxRect){756,465,500,550},28,0,alpha);material((GfxRect){1280,465,550,550},28,0,alpha);texto("Biografia · TMDB",100,495,600,TXT_ILHA_ITEM,235);char *bio=pessoa_bio();bloco(bio&&bio[0]?bio:"O TMDB ainda não tem biografia.",100,545,600,7);free(bio);botao("Ler biografia completa",100,775,580,zona==2);if(pessoa_bio_ingles())bloco("Sem biografia em português no TMDB; esta é a versão em inglês.",100,850,600,2);texto(pessoa_nascimento(),100,930,560,TXT_ILHA_SUB,165);texto(pessoa_outros(),100,965,560,TXT_ILHA_SUB,150);texto("Parcerias frequentes",785,495,440,TXT_ILHA_ITEM,235);int y=545;for(int i=0;i<d->npar;i++){texto(d->parcerias[i].nome,785,y,440,TXT_ILHA_NOME,220);y+=45;}if(!d->npar)bloco("Nenhuma parceria repetida ainda.",785,y,440,2);texto("Família · Wikidata",785,710,440,TXT_ILHA_ITEM,235);y=760;for(int i=0;i<d->nf&&i<4;i++){texto(d->familia[i].nome,785,y,440,TXT_ILHA_NOME,220);texto(d->familia[i].rel,785,y+30,440,TXT_ILHA_SUB,155);y+=65;}if(!d->nf)bloco("Sem dados no Wikidata.",785,y,440,2);texto("Prêmios · Wikidata",1310,495,480,TXT_ILHA_ITEM,235);y=545;for(int i=0;i<d->np&&i<5;i++){texto(d->premios[i].nome,1310,y,480,TXT_ILHA_NOME,225);texto(d->premios[i].rel,1310,y+30,480,TXT_ILHA_SUB,160);y+=80;}if(!d->np)bloco("Nenhum prêmio no Wikidata.",1310,y,480,2);}
static void conquistas(const FdFicha *d){FmConquista cq[4];int n=fm_conquistas(d->pessoa,pessoa_nome(),d->c,d->st,d->n,cq),tr=0;for(int i=0;i<n;i++){FmConquista *c=cq+i;if(!c->trilha)continue;float x=72+tr++*580;material((GfxRect){x,465,550,550},28,0,alpha);texto(c->nome,x+28,495,500,TXT_V2_GRUPO,230);char b[180];snprintf(b,sizeof b,i18n("%d de %d vistos"),c->visto,c->total);texto(b,x+28,545,500,TXT_ILHA_SUB,170);trilho((GfxRect){x+28,590,494,5},c->total?c->visto/(float)c->total:0,.85f,.69f,.35f,alpha);for(int nv=1;nv<=5;nv++){snprintf(b,sizeof b,"%d  %s",nv,i18n(fm_nivel_nome(nv)));texto(b,x+28,625+(nv-1)*68,490,TXT_ILHA_ITEM,c->nivel>=nv?235:140);}}
  float x=tr>1?1250:680;material((GfxRect){x,465,580,550},28,0,alpha);texto("Como conta",x+28,495,510,TXT_V2_GRUPO,235);bloco("Vale o histórico do Nuvio, do Trakt e do Simkl.",x+28,550,510,3);bloco("Já vi manual só conta se você passou nas perguntas.",x+28,650,510,3);bloco("Série conta quando você viu todos os episódios em que a pessoa aparece.",x+28,750,510,3);bloco("Em ordem pede datas reais. Participações e lançamentos futuros ficam fora.",x+28,860,510,3);}
static void overlay(const FdFicha *d){drawZona=5;drawItem=0;float x=1130,w=690;const FmCredito *c=selecionado>=0?d->c+selecionado:NULL;texto(camada==5?pessoa_nome():c?c->titulo:"",x,100,w,TXT_TITULO3,245);
  if(camada==5){char *bio=pessoa_bio();if(!bio)return;float height=txt_bloco(TXT_ILHA_SUB,bio,190,195,205,px(x),py(180),w*colunaK(),29,0,0);int pages=(int)(height/(696*linhaK()));if(bioPagina>pages)bioPagina=pages;GfxRect clip=area((GfxRect){x,180,w,696});gfx_recorte(clip.x,clip.y,clip.w,clip.h);txt_bloco(TXT_ILHA_SUB,bio,190,195,205,clip.x,clip.y-bioPagina*clip.h,clip.w,29,alpha,0);gfx_sem_recorte();free(bio);botao("Fechar",x,935,w,1);return;}
  if(camada==1){texto(c?c->personagem:"",x,170,w,TXT_ILHA_ITEM,180);const FmEstado *s=d->st+selecionado;char b[180];snprintf(b,sizeof b,i18n("%d de %d episódios"),s->epsVistos,c->eps);texto(c->tv?b:s->estado==FM_VISTO?"Visto":"Ainda não visto",x,215,w,TXT_ILHA_SUB,165);const char *opts[]={"Abrir título",s->manual&&s->como==FM_COMO_DIRETO?"Responder para contar nas conquistas":"Já vi","Quero ver","Desfazer Já vi","Fechar"};for(int i=0;i<5;i++)botao(opts[i],x,300+i*90,w,op==i);bloco("Passou nas perguntas, conta para as conquistas; sem responder, só aparece como visto.",x,795,w,4);if(!fd_preparado(selecionado))texto("Carregando…",x,935,w,TXT_ILHA_SUB,175);}
  else if(camada==2){const FmPergunta *q=fm_quiz_pergunta(&quiz);const char *questions[]={"Qual personagem esta pessoa interpreta?","Quem também está no elenco?","Qual é um tema deste título?","Qual cena pertence a este título?"};if(!q)return;bloco(questions[q->tipo],x,180,w,3);for(int i=0;i<4;i++)botao(q->opc[i],x,300+i*90,w,op==i);botao("Não lembro desta",x,700,w,op==4);botao("Marcar sem responder",x,790,w,op==5);bloco("Sem spoiler e sem tempo. Sair não gasta tentativa.",x,900,w,3);}
  else if(camada==3){texto("Quando você viu?",x,190,w,TXT_TITULO3,235);bloco("Vai para o histórico do Nuvio, do Trakt e do Simkl com esta data.",x,270,w,3);botao("Hoje",x,400,w,op==0);botao("Não lembro",x,530,w,op==1);char b[180];snprintf(b,sizeof b,i18n("Usa a data de lançamento: %s"),c->lancamento);bloco(b,x,605,w,3);botao("Agora não",x,730,w,op==2);bloco(como==FM_COMO_QUIZ?"Conta para conquistas: você passou nas perguntas.":"Não conta para conquistas: marcado sem as perguntas.",x,850,w,4);}
  else{texto("Tudo bem, às vezes faz tempo.",x,210,w,TXT_V2_GRUPO,230);bloco("Nada foi marcado. Ninguém mais vê isso.",x,280,w,3);botao("Marcar como visto mesmo assim",x,430,w,op==0);botao("Tentar com outras perguntas",x,550,w,op==1);botao("Fechar",x,670,w,op==2);}}
// The clock island owns the sheet's material, shape, veil and retained closing body.
static void corpoFilmo(GfxRect r,float a,void *u) {
  (void)u;
  TXT_INI();
  int atual=camada; float pa=alpha;
  camada=folhaAnterior; alpha=a*entrada;
  gfx_transformar(0,0,1,r.x-px(1090),r.y-py(50));
  overlay(fd_ficha()); gfx_sem_transformar();
  camada=atual; alpha=pa; TXT_FIM();
}
static void toqueFoco(int z,int c){if(z==5){op=c;return;}zona=z;col=c;if(z==1&&aba!=c){aba=c;topo=0;if(aba==2)fd_sobre();}if(z==3)ano=c;}
static float gesto;
static int toqueRolar(const PonteiroRolagem *e){if(camada==5){if(e->fase==PONT_ROL_MOVER&&e->delta){bioPagina+=e->delta<0?1:-1;if(bioPagina<0)bioPagina=0;}return 1;}if(camada||!(aba==0||aba==1))return 0;if(e->fase==PONT_ROL_INICIO){gesto=0;return e->eixoY==(aba==1)&&e->y>=550;}if(e->fase==PONT_ROL_MOVER||e->fase==PONT_ROL_INERCIA){gesto-=e->delta;int step=(int)(gesto/(aba==1?350:-81));if(step){gesto-=step*(aba==1?350:81);if(aba==1){int max=(ni+gradeCols()-1)/gradeCols()-gradeLinhas();if(max<0)max=0;topo+=step;if(topo<0)topo=0;if(topo>max)topo=max;zona=4;col=topo*gradeCols();}else{int j=0;while(j<na&&anos[j]!=ano)j++;j+=step;if(j<0)j=0;if(j>=na)j=na-1;if(j>=0)ano=anos[j];}}return 1;}return 1;}
static void alvoToque(float x,float y,float w,float h,int z,int c){GfxRect r=area((GfxRect){x,y,w,h});if(h==60)r.h=60;ponteiro_alvo(r.x+paginaX(),r.y,r.w,r.h,toqueFoco,NULL,z,c);}
static void registrarToque(void){if(camada){ponteiro_camada();if(camada==5){ponteiro_rolagem(toqueRolar);alvoToque(1130,935,690,60,5,0);return;}int n=camada==1?5:camada==2?6:3;for(int i=0;i<n;i++){float y=camada==1?300+i*90:camada==2?(i<4?300+i*90:i==4?700:790):camada==3?(i==0?400:i==1?530:730):(i==0?430:i==1?550:670);alvoToque(1130,y,690,60,5,i);}return;}int compact=aba==1&&zona==4;if(!compact){alvoToque(250,265,420,60,0,0);alvoToque(690,265,180,60,0,1);alvoToque(890,265,160,60,0,2);}float tabX=72;for(int i=0;i<4;i++){float tw=txt_largura(TXT_ILHA_SEG,i18n(abas[i]))+40;alvoToque(tabX,compact?130:365,tw/colunaK(),54/linhaK(),1,i);tabX+=(tw+4)/colunaK();}if(aba==0){FmMarco m[10];FmCap caps[6];const FdFicha *d=fd_ficha();int nm=fm_marcos(d->c,d->n,m,10),nc=fm_capitulos(d->c,d->n,m,nm,caps,6);for(int i=0;i<nc;i++)alvoToque(72+i*290,465,270,60,2,i);int start=0;for(int i=0;i<na;i++)if(anos[i]==ano)start=i-10;if(start<0)start=0;if(start>na-21)start=na>21?na-21:0;for(int i=start;i<na&&i<start+21;i++)alvoToque(100+(i-start)*81,550,76,95,3,anos[i]);for(int i=0;i<ni&&i<5;i++)alvoToque(530+i*(212*gfx_poster_k()+24),670,212*gfx_poster_k()/colunaK(),322*gfx_poster_k()/linhaK(),4,i);if(ni>5)alvoToque(1640,970,200,60,4,5);ponteiro_rolagem(toqueRolar);}else if(aba==1){float y=compact?230:465;for(int i=0;i<5;i++)alvoToque(72+i*345,y,325,60,2,i);for(int i=topo*gradeCols();i<ni&&i<(topo+gradeLinhas())*gradeCols();i++)alvoToque(72+(i%gradeCols())*(220*gfx_poster_k()+24)/colunaK(),60+(py(y)+100+(i/gradeCols())*gradePasso()-scroll-60)/linhaK(),220*gfx_poster_k()/colunaK(),330*gfx_poster_k()/linhaK(),4,i);ponteiro_rolagem(toqueRolar);}else if(aba==2)alvoToque(100,775,580,60,2,0);}
void filmo_desenhar(float a){TXT_INI();alpha=a*entrada;preparar();const FdFicha *d=fd_ficha();gfx_cor((GfxRect){0,0,NV_TELA_W,NV_TELA_H},0,.043f,.047f,.055f,alpha);gfx_transformar(0,0,1,paginaX(),0);header(d);if(!d->pronto)t21_estado(area((GfxRect){72,465,1750,430}),d->erro?T21_ERROR:T21_LOADING,"aj_user",i18n(d->erro?"Serviço indisponível":"Carregando…"),NULL,NULL,tempo,alpha);else if(aba==0)historia(d);else if(aba==1)grade(d);else if(aba==2)sobre(d);else conquistas(d);if(camada){GfxRect sheet=area((GfxRect){1090,50,770,980});ilha_corpo(sheet.w,sheet.h,corpoFilmo,NULL);}texto(fd_aviso(),72,1000,1150,TXT_ILHA_SUB,180);const char *keys[]={"OK","Voltar"},*labels[]={"Abrir opções","Voltar"};plrui_dicas(keys,labels,2,px(1830),ANC(T21_SAFE_B),1,alpha);gfx_sem_transformar();registrarToque();TXT_FIM();conquistas_aviso_desenhar();}
