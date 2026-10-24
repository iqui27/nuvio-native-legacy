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
  char imdb[16], poster[NV_ARTE_URL_MAX];
} Atividade;
typedef struct {
  unsigned geracao; CatItem pessoa; char bio[640], local[160];
  SocialEstado estado; int n; Atividade atividades[20];
} SocialDados;
typedef struct { unsigned geracao; CatItem pessoa; } SocialTarefa;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static SocialDados dados, pronto;
static unsigned geracao;
static int temPronto, sair, selecionado, escolhido = -1;
static float entrada, tempo, rolagem, velocidade, focoAnim[20];

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
    obj=strstr(p,"\"show\""); if(!obj||obj>=f)obj=strstr(p,"\"movie\"");
    if(!obj||obj>=f){p=js_prox(f);continue;} obj=strchr(obj,'{'); if(!obj||obj>=f){p=js_prox(f);continue;}
    {const char *fo=js_fim(obj);js_texto(obj,fo,"title",a->titulo,sizeof a->titulo);js_texto(obj,fo,"imdb",imdb,sizeof imdb);}
    ep=strstr(p,"\"episode\"");
    if(ep&&ep<f){const char *eo=strchr(ep,'{'),*ef=eo?js_fim(eo):NULL;int t=eo?(int)js_num(eo,ef,"season",0):0,e=eo?(int)js_num(eo,ef,"number",0):0;char nome[100]="";
      if(eo&&ef)js_texto(eo,ef,"title",nome,sizeof nome);snprintf(a->detalhe,sizeof a->detalhe,i18n("T%dE%d%s%s"),t,e,nome[0]?" · ":"",nome);
    } else snprintf(a->detalhe,sizeof a->detalhe,"Filme");   /* chave inteira: text.c traduz no desenho */
    if(!imdb[0]){p=js_prox(f);continue;} snprintf(a->imdb,sizeof a->imdb,"%s",imdb);
    snprintf(a->poster,sizeof a->poster,"https://images.metahub.space/poster/medium/%s/img",imdb); n++; p=js_prox(f);
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
      d->n++;
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
void social_abrir(const CatItem *pessoa) { entrada=tempo=rolagem=velocidade=0; memset(focoAnim,0,sizeof focoAnim); abrirInterno(pessoa,0); }
int social_quer_sair(void){if(sair && entrada < .002f){sair=0;return 1;}return 0;}
SocialEstado social_estado(void){return dados.estado;}
void social_evento(const SDL_Event *e){SDL_Keycode k;if(!e||e->type!=SDL_KEYDOWN)return;k=e->key.keysym.sym;
  if(nv_tecla_voltar(e)||k==SDLK_LEFT){sair=1;return;}if(k==SDLK_UP&&selecionado>0)selecionado--;if(k==SDLK_DOWN&&selecionado+1<dados.n)selecionado++;
  if(k==SDLK_r && dados.estado!=SOCIAL_CARREGANDO && dados.estado!=SOCIAL_ATUALIZANDO){abrirInterno(&dados.pessoa,1);return;}
  if(k==SDLK_RETURN||k==SDLK_KP_ENTER){if(dados.estado==SOCIAL_PRIVADO||dados.estado==SOCIAL_INDISPONIVEL||dados.estado==SOCIAL_DESCONECTADO||dados.estado==SOCIAL_STALE){abrirInterno(&dados.pessoa,1);return;}if((dados.estado==SOCIAL_PRONTO||dados.estado==SOCIAL_ATUALIZANDO)&&selecionado>=0&&selecionado<dados.n&&dados.atividades[selecionado].imdb[0])escolhido=selecionado;}
}
int social_item_selecionado(SocialItemSelecionado *saida){if(escolhido<0||escolhido>=dados.n)return 0;if(saida){snprintf(saida->imdb,sizeof saida->imdb,"%s",dados.atividades[escolhido].imdb);snprintf(saida->titulo,sizeof saida->titulo,"%s",dados.atividades[escolhido].titulo);}escolhido=-1;return 1;}
void social_atualizar(float dt,Uint32 agora){(void)agora;
  int red=ajustes_animacoes_reduzidas();
  tempo += dt; entrada = red ? !sair : anim_mola(entrada, !sair, dt, NV_MOLA_TELA);
  for(int i=0;i<20;i++) focoAnim[i]=red ? (i==selecionado&&!sair) : anim_mola(focoAnim[i],i==selecionado&&!sair,dt,NV_MOLA_FOCO);
  rolagem=anim_mola2_reduzida(&velocidade,rolagem,fmaxf(0,selecionado-3)*154,dt,NV_MOLA2_SCROLL,red);pthread_mutex_lock(&trava);if(temPronto){SocialDados novo=pronto;if((novo.estado==SOCIAL_PRIVADO||novo.estado==SOCIAL_INDISPONIVEL||novo.estado==SOCIAL_DESCONECTADO)&&dados.n>0){novo.n=dados.n;memcpy(novo.atividades,dados.atividades,sizeof novo.atividades);novo.estado=SOCIAL_STALE;}dados=novo;temPronto=0;if(selecionado>=dados.n)selecionado=dados.n?dados.n-1:0;}pthread_mutex_unlock(&trava);}
static void texto(TxtEstilo estilo,const char *s,float x,float y,float w,int cor){txt_desenhar(txt_linha_corta(estilo,s?s:"",cor,cor,cor,255,w),x,y);}
static const char *estadoTexto(SocialEstado estado){switch(estado){case SOCIAL_CARREGANDO:return "Carregando atividade…";case SOCIAL_ATUALIZANDO:return "Atualizando · atividade anterior preservada";case SOCIAL_STALE:return "Atualização indisponível · mostrando atividade anterior";case SOCIAL_SEM_ATIVIDADE:return "Sem atividade pública por enquanto";case SOCIAL_PRIVADO:return "Atividade privada ou não compartilhada";case SOCIAL_DESCONECTADO:return "Trakt desconectado";case SOCIAL_INDISPONIVEL:return "Serviço indisponível";default:return "Atividade recente";}}
// A RAIL FIXA EMPURRA A TELA (26/09). As duas colunas foram medidas para a
// tela inteira (perfil em 96, atividade em 656 ate 1800); com a rail presa a
// coluna do perfil nascia embaixo dela. Tudo anda `R` para a direita e a
// coluna da atividade ENCOLHE pela esquerda: a borda direita fica em 1800, e o
// titulo + detalhe (a terceira coluna) se estreita na mesma proporcao.
void social_desenhar(Uint32 agora) {
  (void)agora;
  TXT_INI();
  float lx,w; t21_inicio(&lx,&w);
  float rx=lx+560,rw=w-560,tx=rx+116+rw*.34f,tw=rw*.46f,cw=rw*.34f;
  float a=entrada, og=gfx_opacidade_grupo; gfx_opacidade_grupo=og*a;
  gfx_cor((GfxRect){0,0,NV_TELA_W,NV_TELA_H},0,NV_COR_FUNDO_R,NV_COR_FUNDO_G,NV_COR_FUNDO_B,1);
  texto(TXT_V2_KICK,"ENTRE AMIGOS",lx,ANC(60),520,179);
  svd_avatar((GfxRect){lx,ANC(132),176,176},dados.pessoa.socialAvatar,dados.pessoa.socialNome,dados.pessoa.socialSlug,1);
  texto(TXT_NOV_TITULO,dados.pessoa.socialNome[0]?dados.pessoa.socialNome:i18n("Perfil indisponível"),lx,ANC(346),500,245);
  if(dados.pessoa.socialSlug[0]){char u[160];snprintf(u,sizeof u,"@%s",dados.pessoa.socialSlug);texto(TXT_CALLOUT,u,lx,ANC(416),500,179);}
  if(dados.local[0])texto(TXT_CAPTION,dados.local,lx,ANC(470),500,179);
  if(dados.bio[0])txt_bloco(TXT_CAPTION,dados.bio,210,210,210,lx,ANC(524),480,32,1,8);
  texto(TXT_CAPTION,"Trakt · perfil público",lx,ANC(900),480,179);
  t21_dica("Voltar",i18n("Voltar"),lx,ANC(990),1);
  texto(TXT_ILHA_TITULO,"Atividade recente",rx,ANC(64),rw,245);
  texto(TXT_CAPTION,"Pessoa · ação · título · horário",rx,ANC(122),rw,179);
  if(dados.estado!=SOCIAL_PRONTO&&dados.estado!=SOCIAL_ATUALIZANDO&&dados.estado!=SOCIAL_STALE) {
    int loading=dados.estado==SOCIAL_CARREGANDO;
    t21_estado((GfxRect){rx,ANC(210),rw,fminf(460,ANC(920)-ANC(210))},loading?T21_LOADING:T21_EMPTY,
      "aj_users",i18n(estadoTexto(dados.estado)),NULL,
      !loading&&dados.estado!=SOCIAL_SEM_ATIVIDADE?i18n("Tentar de novo"):NULL,tempo,1);
  } else {
    gfx_recorte(rx-16,ANC(184),rw+32,ANC(970)-ANC(184));
    for(int i=0;i<dados.n;i++) {
      Atividade *at=&dados.atividades[i];
      float e=ajustes_animacoes_reduzidas()?1:anim_atraso(tempo,i,28,320),y=ANC(196)+i*154-rolagem+(1-e)*22;
      if(y+144<ANC(184)||y>ANC(970))continue;
      plrui_linha_foco((GfxRect){rx-16,y-12,rw+16,144},T21_R_ROW,focoAnim[i]*e);
      svd_poster((GfxRect){rx,y,96,134},at->poster,8,e);
      texto(TXT_ILHA_APOIO,at->pessoa,rx+116,y+4,cw,179);
      texto(TXT_CALLOUT,at->acao,rx+116,y+36,cw,235);
      texto(TXT_CAPTION,at->titulo,tx,y+4,tw,245);
      texto(TXT_CAPTION,at->detalhe,tx,y+42,tw,179);
      texto(TXT_ILHA_APOIO,at->horario,tx,y+82,tw,179);
    }
    gfx_sem_recorte();
    texto(TXT_ILHA_APOIO,dados.estado==SOCIAL_STALE?"Atualização indisponível · mostrando atividade anterior · OK: tentar novamente":"OK · abrir título   ·   Voltar · fechar",rx,ANC(1000),rw,190);
  }
  gfx_opacidade_grupo=og;
  TXT_FIM();
}
