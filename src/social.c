#include "arteurl.h"
#include "social.h"
#include "horafmt.h"
#include "idioma.h"
#include "trakt.h"
#include "recomenda.h"
#include "rede.h"
#include "js.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "layout.h"
#include "ajustes.h"
#define NV_TXT_TELA
#include "tela21.h"
#include "svdesenho.h"
#include "revela.h"
#include "teclavoltar.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define SOCIAL_CARREGANDO SOCIAL_ESTADO_CARREGANDO
#define SOCIAL_ATUALIZANDO SOCIAL_ESTADO_ATUALIZANDO
#define SOCIAL_PRONTO SOCIAL_ESTADO_PRONTO
#define SOCIAL_STALE SOCIAL_ESTADO_STALE
#define SOCIAL_SEM_ATIVIDADE SOCIAL_ESTADO_SEM_ATIVIDADE
#define SOCIAL_PRIVADO SOCIAL_ESTADO_PRIVADO
#define SOCIAL_DESCONECTADO SOCIAL_ESTADO_DESCONECTADO
#define SOCIAL_INDISPONIVEL SOCIAL_ESTADO_INDISPONIVEL
typedef struct {
  char pessoa[96], acao[64], titulo[160], detalhe[160], horario[32];
  char imdb[16], poster[NV_ARTE_URL_MAX], fundo[NV_ARTE_URL_MAX];
  time_t quando; int avaliacao;   // avaliacao: rating rows (the "Avaliações" tab)
} Atividade;
typedef struct {
  unsigned geracao; CatItem pessoa; char bio[640], local[160];
  SocialEstado estado; int n; Atividade atividades[20];
} SocialDados;
typedef struct { unsigned geracao; CatItem pessoa; } SocialTarefa;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static SocialDados dados, pronto;
static unsigned geracao;
static int temPronto, sair, selecionado, escolhido = -1, filtro, zona = 1;
static float entrada, tempo, rolagem, velocidade, focoAnim[20], segSel, segFoco, revT;

static int slugValido(const char *s) {
  return s && s[0] && strspn(s, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_") == strlen(s);
}
static int campoDentro(const char *ini, const char *fim, const char *chave,
                       char *dst, size_t tam) {
  char busca[48];
  const char *p;
  snprintf(busca, sizeof busca, "\"%s\"", chave);
  p = strstr(ini, busca);
  return p && p < fim && js_texto(p, fim, chave, dst, tam);
}
static time_t isoParaTime(const char *iso) {
  int y, m, d, h, mi, s; struct tm t;
  if (!iso || sscanf(iso, "%d-%d-%dT%d:%d:%d", &y, &m, &d, &h, &mi, &s) != 6) return (time_t)-1;
  memset(&t, 0, sizeof t); t.tm_year=y-1900; t.tm_mon=m-1; t.tm_mday=d;
  t.tm_hour=h; t.tm_min=mi; t.tm_sec=s; return timegm(&t);
}
static void horarioLocal(const char *iso, char *dst, size_t tam) {
  time_t stamp=isoParaTime(iso); struct tm local; char h[16];
  if (stamp==(time_t)-1 || !localtime_r(&stamp,&local)) { dst[0]=0; return; }
  // HORA DE TELA (horafmt.h): a linha "02/10/2026 · 21:10" da atividade do
  // amigo seguia em 24 h fixo; agora obedece Ajustes > Formato do relogio.
  hora_tela(h, sizeof h, &local);
  snprintf(dst,tam,"%02d/%02d/%04d · %s",local.tm_mday,local.tm_mon+1,local.tm_year+1900,h);
}
// O VERBO DA ATIVIDADE PASSA POR i18n AQUI, e nao em quem desenha.
//
// O resultado e copiado para `a->acao` com snprintf e so muito depois vira
// texto na tela — a varredura de i18n nao cobre valor de retorno de funcao, e
// por isso estas cinco frases atravessaram a tabela sem ninguem notar. Apareceu
// na foto: "Kevin · assistiu" no meio de uma home inteira em ingles.
//
// A traducao acontece na LEITURA e nao no desenho porque o feed e montado num
// fio de rede e a linha ja e guardada pronta; traduzir no desenho obrigaria a
// guardar a chave e a traduzir a cada quadro.
static const char *rotuloAcao(const char *acao, const char *timestamp) {
  if (!strcmp(acao,"watching")) {
    time_t stamp=isoParaTime(timestamp);
    if (stamp!=(time_t)-1 && difftime(time(NULL),stamp)<=10.0*60.0) return i18n("assistindo agora");
    return i18n("assistiu");
  }
  if (!strcmp(acao,"watch")||!strcmp(acao,"watched")||!strcmp(acao,"scrobble")) return i18n("assistiu");
  if (!strcmp(acao,"rating")||!strcmp(acao,"rated")) return i18n("avaliou");
  if (!strcmp(acao,"checkin")||!strcmp(acao,"check-in")) return i18n("fez check-in");
  return i18n("atividade recente");
}
static int extrairAtividades(const char *corpo, SocialDados *d, const char *acaoPadrao) {
  const char *p=strchr(corpo?corpo:"",'['); int n=0; p=p?p+1:NULL;
  while (p&&*p&&n<20) {
    const char *f,*obj,*ep; Atividade *a; char acao[32]="",timestamp[40]="",imdb[24]="";
    while(*p&&(unsigned char)*p<=' ')p++; if(*p!='{')break; f=js_fim(p);
    a=&d->atividades[n]; memset(a,0,sizeof *a);
    snprintf(a->pessoa,sizeof a->pessoa,"%s",d->pessoa.socialNome[0]?d->pessoa.socialNome:d->pessoa.socialSlug);
    if(!campoDentro(p,f,"action",acao,sizeof acao)||!acao[0])snprintf(acao,sizeof acao,"%s",acaoPadrao?acaoPadrao:"watch");
    if(!campoDentro(p,f,"timestamp",timestamp,sizeof timestamp))campoDentro(p,f,"watched_at",timestamp,sizeof timestamp);
    snprintf(a->acao,sizeof a->acao,"%s",rotuloAcao(acao,timestamp)); horarioLocal(timestamp,a->horario,sizeof a->horario);
    a->quando=isoParaTime(timestamp); a->avaliacao=!strcmp(acao,"rating")||!strcmp(acao,"rated");
    obj=strstr(p,"\"show\""); if(!obj||obj>=f)obj=strstr(p,"\"movie\"");
    if(!obj||obj>=f){p=js_prox(f);continue;} obj=strchr(obj,'{'); if(!obj||obj>=f){p=js_prox(f);continue;}
    {const char *fo=js_fim(obj);js_texto(obj,fo,"title",a->titulo,sizeof a->titulo);js_texto(obj,fo,"imdb",imdb,sizeof imdb);}
    ep=strstr(p,"\"episode\"");
    if(ep&&ep<f){const char *eo=strchr(ep,'{'),*ef=eo?js_fim(eo):NULL;int t=eo?(int)js_num(eo,ef,"season",0):0,e=eo?(int)js_num(eo,ef,"number",0):0;char nome[100]="";
      if(eo&&ef)js_texto(eo,ef,"title",nome,sizeof nome);snprintf(a->detalhe,sizeof a->detalhe,i18n("T%dE%d%s%s"),t,e,nome[0]?" · ":"",nome);
    } else snprintf(a->detalhe,sizeof a->detalhe,"Filme");   /* chave inteira: text.c traduz no desenho */
    if(!imdb[0]){p=js_prox(f);continue;} snprintf(a->imdb,sizeof a->imdb,"%s",imdb);
    snprintf(a->poster,sizeof a->poster,"https://images.metahub.space/poster/medium/%s/img",imdb);
    snprintf(a->fundo,sizeof a->fundo,"https://images.metahub.space/background/medium/%s/img",imdb); n++; p=js_prox(f);
  }
  return n;
}
static void *carregar(void *arg) {
  SocialTarefa *t=arg; SocialDados *d=calloc(1,sizeof *d); const char *cab[4]; char aut[200],chave[140],url[512],*body;
  if(!d){free(t);return NULL;} d->geracao=t->geracao;d->pessoa=t->pessoa;d->estado=SOCIAL_INDISPONIVEL;
  // AMIGO DO NUVIO (nosso servico, nao o Trakt): o id vem em socialSlug com o
  // prefixo "nuvio:" (recomenda.c, lerFeedCorpo). O que ele compartilhou ja veio
  // no feed da fileira, entao esta tela responde SEM rede — e sem Trakt, que e
  // justamente o caso de quem so tem conta Nuvio. Nao ha bio nem cidade: o
  // amigo nao publicou isso para amigos, so a atividade.
  if(!strncmp(d->pessoa.socialSlug,"nuvio:",6)) {
    RecAtivAmigo *rec=calloc(20,sizeof *rec); int q,nr=rec?recomenda_amigo_atividades(d->pessoa.socialSlug,rec,20):0;
    for(q=0;q<nr&&d->n<20;q++){
      Atividade *a=&d->atividades[d->n]; time_t t=(time_t)rec[q].criado; struct tm local; char h[16];
      memset(a,0,sizeof *a);
      snprintf(a->pessoa,sizeof a->pessoa,"%s",d->pessoa.socialNome);
      snprintf(a->acao,sizeof a->acao,"%s",i18n(rec[q].agora?"assistindo agora":"assistiu"));
      snprintf(a->titulo,sizeof a->titulo,"%s",rec[q].titulo);
      snprintf(a->detalhe,sizeof a->detalhe,"%s",!strcmp(rec[q].tipo,"series")?"Série":"Filme");
      if(t>0&&localtime_r(&t,&local)){ hora_tela(h,sizeof h,&local);
        snprintf(a->horario,sizeof a->horario,"%02d/%02d/%04d · %s",local.tm_mday,local.tm_mon+1,local.tm_year+1900,h); }
      snprintf(a->imdb,sizeof a->imdb,"%s",rec[q].imdb);
      snprintf(a->poster,sizeof a->poster,"https://images.metahub.space/poster/medium/%s/img",rec[q].imdb);
      snprintf(a->fundo,sizeof a->fundo,"https://images.metahub.space/background/medium/%s/img",rec[q].imdb);
      a->quando=t>0?t:0; d->n++;
    }
    free(rec);
    d->estado=d->n?SOCIAL_PRONTO:SOCIAL_SEM_ATIVIDADE;
  } else
  if(!slugValido(d->pessoa.socialSlug)||!trakt_cabecalhos(cab,aut,sizeof aut,chave,sizeof chave))d->estado=!trakt_ativo()?SOCIAL_DESCONECTADO:SOCIAL_INDISPONIVEL;
  else {
    snprintf(url,sizeof url,"https://api.trakt.tv/users/%s?extended=full",d->pessoa.socialSlug); body=rede_baixar_com(url,10,cab);
    if(!body)d->estado=SOCIAL_INDISPONIVEL;
    else {js_texto(body,NULL,"name",d->pessoa.socialNome,sizeof d->pessoa.socialNome);js_texto(body,NULL,"about",d->bio,sizeof d->bio);js_texto(body,NULL,"location",d->local,sizeof d->local);
      {const char *av=strstr(body,"\"avatar\"");if(av)js_texto(av,NULL,"full",d->pessoa.socialAvatar,sizeof d->pessoa.socialAvatar);}free(body);
      snprintf(url,sizeof url,"https://api.trakt.tv/users/%s/activities?limit=20&extended=full",d->pessoa.socialSlug);body=rede_baixar_com(url,12,cab);
      if(body&&strchr(body,'[')){d->n=extrairAtividades(body,d,"watch");d->estado=d->n?SOCIAL_PRONTO:SOCIAL_SEM_ATIVIDADE;free(body);}
      else {free(body);snprintf(url,sizeof url,"https://api.trakt.tv/users/%s/history?limit=20&extended=full",d->pessoa.socialSlug);body=rede_baixar_com(url,12,cab);
        if(body&&strchr(body,'[')){d->n=extrairAtividades(body,d,"watch");d->estado=d->n?SOCIAL_PRONTO:SOCIAL_SEM_ATIVIDADE;}else d->estado=SOCIAL_PRIVADO;free(body);}
    }
  }
  pthread_mutex_lock(&trava);if(d->geracao==geracao){pronto=*d;temPronto=1;}pthread_mutex_unlock(&trava);free(d);free(t);return NULL;
}
static void abrirInterno(const CatItem *pessoa, int preservar) {
  SocialTarefa *t; pthread_t fio; CatItem copia; memset(&copia,0,sizeof copia);if(pessoa)copia=*pessoa;
  SocialDados anterior=dados;
  pthread_mutex_lock(&trava);geracao++;temPronto=0;
  if(preservar)dados=anterior;else dados=(SocialDados){0};
  dados.geracao=geracao;dados.pessoa=copia;dados.estado=preservar?SOCIAL_ATUALIZANDO:SOCIAL_CARREGANDO;pthread_mutex_unlock(&trava);
  sair=0;selecionado=0;escolhido=-1;t=malloc(sizeof *t);if(!t){dados.estado=SOCIAL_INDISPONIVEL;return;}t->geracao=geracao;t->pessoa=copia;
  if(pthread_create(&fio,NULL,carregar,t)==0)pthread_detach(fio);else{free(t);dados.estado=SOCIAL_INDISPONIVEL;}
}
void social_abrir(const CatItem *pessoa) { entrada=tempo=rolagem=velocidade=segSel=segFoco=revT=0; filtro=0; zona=1; memset(focoAnim,0,sizeof focoAnim); abrirInterno(pessoa,0); }
int social_quer_sair(void){if(sair && entrada < .002f){sair=0;return 1;}return 0;}
SocialEstado social_estado(void){return dados.estado;}

// O MOCKUP APROVADO ("Glass UI — ilha", tela 3, design/glass-ilha): a atividade
// e a ilha de 760 a 40 px da direita, do topo e da base; kicker + titulo, o
// segmentado e as linhas com rosto, tres andares de texto e a capa a direita,
// agrupadas por dia. Sem o disco de fechar (dono, 03/10: Voltar/esquerda fecham).
// A identidade do amigo fica a esquerda, sobre a arte, na gramatica do Perfil
// (tela 11). Medidas em px de 1080p relativas a ilha.
#define SO_W 760.0f
#define SO_RAIO 36.0f
#define SO_LISTA 225.0f   // topo da primeira secao ("Hoje")
#define SO_LINHA 115.0f
#define SO_SECAO 43.0f    // rotulo 22 + 10 + fio + 6 ate a linha
#define SO_SECAO_AR 22.0f
static GfxRect ilhaRect(void){float y=ANC(40);return (GfxRect){ANC(1880)-SO_W,y,SO_W,ANC(1040)-y};}
static int ativVisiveis(int *v){int n=0;for(int i=0;i<dados.n;i++)if(!filtro||(filtro==2)==dados.atividades[i].avaliacao)v[n++]=i;return n;}
static int grupoDia(time_t q,time_t agora){struct tm a,b;if(q<=0||!localtime_r(&agora,&a)||!localtime_r(&q,&b))return 2;
  if(a.tm_year==b.tm_year&&a.tm_yday==b.tm_yday)return 0;return difftime(agora,q)<7*86400.0?1:2;}
// Topo de cada linha (e da secao que abre antes dela) dentro da lista; devolve a altura toda.
static float montarLista(const int *v,int nv,time_t agora,float *ry,int *sec,float *sy){
  float y=0;int g=-1;
  for(int i=0;i<nv;i++){int gi=grupoDia(dados.atividades[v[i]].quando,agora);sec[i]=-1;
    if(gi!=g){if(g>=0)y+=SO_SECAO_AR;sec[i]=gi;sy[i]=y;y+=SO_SECAO;g=gi;}
    ry[i]=y;y+=SO_LINHA;}
  return y;
}
static float listaAltura(void){GfxRect r=ilhaRect();return r.h-SO_LISTA-26;}

void social_evento(const SDL_Event *e){SDL_Keycode k;int v[20],nv;if(!e||e->type!=SDL_KEYDOWN)return;k=e->key.keysym.sym;nv=ativVisiveis(v);
  if(nv_tecla_voltar(e)){sair=1;return;}
  if(k==SDLK_r && dados.estado!=SOCIAL_CARREGANDO && dados.estado!=SOCIAL_ATUALIZANDO){abrirInterno(&dados.pessoa,1);return;}
  if(k==SDLK_RETURN||k==SDLK_KP_ENTER){if(dados.estado==SOCIAL_PRIVADO||dados.estado==SOCIAL_INDISPONIVEL||dados.estado==SOCIAL_DESCONECTADO||dados.estado==SOCIAL_STALE){abrirInterno(&dados.pessoa,1);return;}
    if(zona==0){if(nv)zona=1;return;}
    if((dados.estado==SOCIAL_PRONTO||dados.estado==SOCIAL_ATUALIZANDO)&&selecionado>=0&&selecionado<nv&&dados.atividades[v[selecionado]].imdb[0])escolhido=v[selecionado];return;}
  if(zona==0){
    if(k==SDLK_LEFT){if(filtro>0){filtro--;selecionado=0;revT=tempo;}else sair=1;}
    else if(k==SDLK_RIGHT&&filtro<2){filtro++;selecionado=0;revT=tempo;}
    else if(k==SDLK_DOWN&&nv)zona=1;
    return;}
  if(k==SDLK_LEFT){sair=1;return;}
  if(k==SDLK_UP){if(selecionado>0)selecionado--;else zona=0;}
  if(k==SDLK_DOWN&&selecionado+1<nv)selecionado++;
}
int social_item_selecionado(SocialItemSelecionado *saida){if(escolhido<0||escolhido>=dados.n)return 0;if(saida){snprintf(saida->imdb,sizeof saida->imdb,"%s",dados.atividades[escolhido].imdb);snprintf(saida->titulo,sizeof saida->titulo,"%s",dados.atividades[escolhido].titulo);}escolhido=-1;return 1;}
void social_atualizar(float dt,Uint32 agora){(void)agora;
  int red=ajustes_animacoes_reduzidas_usuario(),v[20],sec[20],nv;float ry[20],sy[20],alto,alvo;
  TXT_INI();
  tempo += dt; entrada = t21_entrada(entrada, dt, !sair);
  pthread_mutex_lock(&trava);if(temPronto){SocialDados novo=pronto;if((novo.estado==SOCIAL_PRIVADO||novo.estado==SOCIAL_INDISPONIVEL||novo.estado==SOCIAL_DESCONECTADO)&&dados.n>0){novo.n=dados.n;memcpy(novo.atividades,dados.atividades,sizeof novo.atividades);novo.estado=SOCIAL_STALE;}dados=novo;temPronto=0;}pthread_mutex_unlock(&trava);
  nv=ativVisiveis(v);if(selecionado>=nv)selecionado=nv?nv-1:0;if(!nv)zona=0;
  for(int i=0;i<20;i++) focoAnim[i]=red ? (i==selecionado&&zona&&!sair) : anim_mola(focoAnim[i],i==selecionado&&zona&&!sair,dt,NV_MOLA_FOCO);
  segFoco=red?(zona==0&&!sair):anim_mola(segFoco,zona==0&&!sair,dt,NV_MOLA_FOCO);
  segSel=red?filtro:anim_mola(segSel,(float)filtro,dt,NV_MOLA_FOCO);
  alto=montarLista(v,nv,time(NULL),ry,sec,sy);
  alvo=nv?fminf(fmaxf(0,ry[selecionado]+SO_LINHA-listaAltura()*.62f),fmaxf(0,alto-listaAltura())):0;
  rolagem=anim_mola2_reduzida(&velocidade,rolagem,alvo,dt,NV_MOLA2_SCROLL,red);
  TXT_FIM();
}
static void texto(TxtEstilo estilo,const char *s,float x,float y,float w,int cor){txt_desenhar(txt_linha_corta(estilo,s?s:"",cor,cor,cor,255,w),x,y);}
static float textoA(TxtEstilo estilo,const char *s,float x,float y,float w,float a){TxtLinha l=txt_linha_corta(estilo,s?s:"",243,242,239,255,w);txt_desenhar_alpha(l,x,y,a);return l.w;}
static const char *estadoTexto(SocialEstado estado){switch(estado){case SOCIAL_CARREGANDO:return "Carregando atividade…";case SOCIAL_ATUALIZANDO:return "Atualizando · atividade anterior preservada";case SOCIAL_STALE:return "Atualização indisponível · mostrando atividade anterior";case SOCIAL_SEM_ATIVIDADE:return "Sem atividade pública por enquanto";case SOCIAL_PRIVADO:return "Atividade privada ou não compartilhada";case SOCIAL_DESCONECTADO:return "Trakt desconectado";case SOCIAL_INDISPONIVEL:return "Serviço indisponível";default:return "Atividade recente";}}
static void quandoTexto(const Atividade *at,time_t agora,char *d,size_t n){
  double s=at->quando>0?difftime(agora,at->quando):-1;
  if(s<0)snprintf(d,n,"%s",at->horario);
  else if(s<60)snprintf(d,n,"%s",i18n("agora"));
  else if(s<3600)snprintf(d,n,i18n("há %d min"),(int)(s/60));
  else if(!grupoDia(at->quando,agora))snprintf(d,n,i18n("há %d h"),(int)(s/3600));
  else if(s<2*86400)snprintf(d,n,"%s",i18n("ontem"));
  else if(s<7*86400)snprintf(d,n,i18n("há %d dias"),(int)(s/86400));
  else snprintf(d,n,"%s",at->horario);
}
// Fundo: a arte (backdrop) do titulo mais recente sob o veu do mockup (.veu2).
static void fundo(float a){
  GfxRect tela={0,0,NV_TELA_W,NV_TELA_H};const char *u=dados.n?dados.atividades[0].fundo:"";GLuint t=u[0]?tex_obter_hero(u):0;
  gfx_cor(tela,0,NV_COR_FUNDO_R,NV_COR_FUNDO_G,NV_COR_FUNDO_B,a);
  if(t){gfx_tex_aspect_atual=tex_aspecto(u);gfx_rect(tela,t,GFX_CARD,0,0,0,0,1,1,1,a);gfx_tex_aspect_atual=0;}
  // + a sombra de leitura da esquerda, sob a identidade do amigo.
  gfx_veu_css_base(tela,2,0,.62f,.55f*a,(ajustes_vidro()?.30f:.62f)*a);
}
// A linha do mockup: superficie de 708 (26 das bordas), rosto 52, nome + verbo,
// titulo, quando, e a capa 52x76 a direita.
static void linha(GfxRect r,const Atividade *at,float f,float e,time_t agora){
  char nome[96],t2[340],q[64];size_t k;float w=r.w-92-92,x=r.x+92,nw;
  snprintf(nome,sizeof nome,"%s",at->pessoa);k=strcspn(nome," ");nome[k]=0;
  plrui_linha_foco(r,22,f*e);
  svd_avatar((GfxRect){r.x+22,r.y+31.5f,52,52},dados.pessoa.socialAvatar,nome,dados.pessoa.socialSlug,e);
  nw=textoA(TXT_ILHA_NOME,nome,x,r.y+18,w,(.88f+.12f*f)*e);
  textoA(TXT_ILHA_CORPO,at->acao,x+nw+7,r.y+18,w-nw-7,.55f*e);
  if(at->detalhe[0]&&strcmp(at->detalhe,"Filme")&&strcmp(at->detalhe,"Série"))snprintf(t2,sizeof t2,"%s · %s",at->titulo,at->detalhe);else snprintf(t2,sizeof t2,"%s",at->titulo);
  textoA(TXT_ILHA_SUB,t2,x,r.y+51,w,.62f*e);
  quandoTexto(at,agora,q,sizeof q);textoA(TXT_ILHA_HORA,q,x,r.y+78,w,.38f*e);
  svd_poster((GfxRect){r.x+r.w-22-52,r.y+20,52,76},at->poster,9.0f/76.0f,e);
}
void social_desenhar(Uint32 agora) {
  (void)agora;
  TXT_INI();
  float lx,w; t21_inicio(&lx,&w);
  float a=anim_suave(entrada), og=gfx_opacidade_grupo; gfx_opacidade_grupo=og*a;
  GfxRect il=ilhaRect(); il.x+=(1-a)*48;
  time_t now=time(NULL); int v[20],sec[20],nv=ativVisiveis(v); float ry[20],sy[20];
  fundo(1);
  // Identidade (tela 11 do mockup): rosto 120 no anel do acento, nome 52, @ e cidade.
  {float ar,ag,ab,ax=lx,ay=ANC(56),tx=lx+149,tw=fmaxf(200,il.x-64-tx),bw=fminf(720,il.x-64-lx);char u[200];
    ajustes_acento(&ar,&ag,&ab);
    gfx_anel_fora((GfxRect){ax,ay,120,120},.5f,5,3,ar,ag,ab,1);
    svd_avatar((GfxRect){ax,ay,120,120},dados.pessoa.socialAvatar,dados.pessoa.socialNome,dados.pessoa.socialSlug,1);
    texto(TXT_NOV_TITULO,dados.pessoa.socialNome[0]?dados.pessoa.socialNome:i18n("Perfil indisponível"),tx,ay+16,tw,245);
    if(dados.pessoa.socialSlug[0]&&strncmp(dados.pessoa.socialSlug,"nuvio:",6))snprintf(u,sizeof u,dados.local[0]?"@%s · %s":"@%s",dados.pessoa.socialSlug,dados.local);else snprintf(u,sizeof u,"%s",dados.local);
    if(u[0])textoA(TXT_ILHA_SUB_L,u,tx,ay+86,tw,.55f);
    float by=dados.bio[0]?txt_bloco(TXT_ILHA_SUB_L,dados.bio,243,242,239,lx,ay+164,bw,32,.62f,5)+20:0;
    if(strncmp(dados.pessoa.socialSlug,"nuvio:",6))textoA(TXT_ILHA_APOIO,"Trakt · perfil público",lx,ay+164+by,bw,.45f);
    {static const char *const T[2]={"OK","Voltar"},*const R[2]={"Abrir título","Fechar"};int lista=nv&&zona&&(dados.estado==SOCIAL_PRONTO||dados.estado==SOCIAL_ATUALIZANDO||dados.estado==SOCIAL_STALE);plrui_dicas(T+!lista,R+!lista,2-!lista,lx,ANC(1000),0,1);}}
  // A ilha.
  t21_ilha_folha(il,SO_RAIO,1);
  plrui_kicker("Entre amigos",il.x+48,il.y+40,115,115,115,1);
  textoA(TXT_ILHA_TITULO,"Atividade recente",il.x+48,fmaxf(il.y+58,il.y+40+txt_linha(TXT_MINI,"AG",255,255,255,255).h),il.w-96,1);
  {int c[3]={0,0,0};for(int i=0;i<dados.n;i++){c[0]++;c[dados.atividades[i].avaliacao?2:1]++;}
    static const char *const ROT[3]={"Tudo","Assistidos","Avaliações"};
    int ok=dados.estado==SOCIAL_PRONTO||dados.estado==SOCIAL_ATUALIZANDO||dados.estado==SOCIAL_STALE;
    if(!ok)c[0]=c[1]=c[2]=-1;
    plrui_seg_ex(ROT,c,3,filtro,segSel,segFoco,il.x+42,il.y+138,1,0);}
  if(dados.estado!=SOCIAL_PRONTO&&dados.estado!=SOCIAL_ATUALIZANDO&&dados.estado!=SOCIAL_STALE) {
    int loading=dados.estado==SOCIAL_CARREGANDO;float x=il.x+48,y=il.y+SO_LISTA+16;
    if(loading)for(int i=0;i<3;i++)gfx_cor((GfxRect){x+i*18,y+12,8,8},.5f,1,1,1,ajustes_animacoes_reduzidas_usuario()?.7f:.4f+.3f*(1+sinf(tempo*6-i*1.2f)));
    else gfx_icone((GfxRect){x,y,32,32},"aj_users",1,1,1,.62f);
    y+=56+txt_bloco(TXT_ILHA_NOME,i18n(estadoTexto(dados.estado)),243,242,239,x,y+56,il.w-96,32,1,2)+24;
    if(!loading&&dados.estado!=SOCIAL_SEM_ATIVIDADE){TxtLinha l=txt_linha(T21_CHIP,i18n("Tentar de novo"),255,255,255,255);GfxRect b={x,y,l.w+48,T21_CTRL_H};int ink=ajustes_tinta_foco();
      plrui_pilula_foco(b,1);l=txt_linha(T21_CHIP,i18n("Tentar de novo"),ink,ink,ink,255);txt_desenhar(l,b.x+(b.w-l.w)*.5f,b.y+(b.h-l.h)*.5f);}
  } else {
    float ly=il.y+SO_LISTA,bot=il.y+il.h-26;
    montarLista(v,nv,now,ry,sec,sy);
    gfx_recorte(il.x,ly-6,il.w,bot-ly+6);
    for(int i=0;i<nv;i++) {
      float e=ajustes_animacoes_reduzidas_usuario()?1:revela_entra(1,revela_onda_atraso(0,i),1+(Uint32)((tempo-revT)*1000)),dy=(1-e)*NV_ENTRA_DY-rolagem;
      if(sec[i]>=0){float y=ly+sy[i]+dy;static const char *const G[3]={"Hoje","Esta semana","Mais antigos"};
        textoA(TXT_ILHA_SECAO,G[sec[i]],il.x+48,y,il.w-96,e);gfx_cor((GfxRect){il.x+26,y+36,il.w-52,1},0,1,1,1,.08f*e);}
      GfxRect r={il.x+26,ly+ry[i]+dy,il.w-52,SO_LINHA};
      if(r.y+r.h<ly-6||r.y>bot)continue;
      linha(r,&dados.atividades[v[i]],focoAnim[i],e,now);
    }
    gfx_sem_recorte();
    if(dados.estado==SOCIAL_STALE)textoA(TXT_ILHA_APOIO,"Atualização indisponível · mostrando atividade anterior · OK: tentar novamente",il.x+48,il.y+il.h-44,il.w-96,.62f);
  }
  gfx_opacidade_grupo=og;
  TXT_FIM();
}
