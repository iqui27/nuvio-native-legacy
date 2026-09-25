#include "social.h"
#include "idioma.h"
#include "trakt.h"
#include "rede.h"
#include "js.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "layout.h"
#include "ajustes.h"
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
  char imdb[16], poster[512];
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
  time_t stamp=isoParaTime(iso); struct tm local;
  if (stamp==(time_t)-1 || !localtime_r(&stamp,&local)) { dst[0]=0; return; }
  strftime(dst,tam,"%d/%m/%Y · %H:%M",&local);
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
void social_abrir(const CatItem *pessoa) { abrirInterno(pessoa,0); }
int social_quer_sair(void){int v=sair;sair=0;return v;}
SocialEstado social_estado(void){return dados.estado;}
void social_evento(const SDL_Event *e){SDL_Keycode k;if(!e||e->type!=SDL_KEYDOWN)return;k=e->key.keysym.sym;
  if(k==SDLK_ESCAPE||k==SDLK_AC_BACK||k==SDLK_BACKSPACE||k==SDLK_LEFT){sair=1;return;}if(k==SDLK_UP&&selecionado>0)selecionado--;if(k==SDLK_DOWN&&selecionado+1<dados.n)selecionado++;
  if(k==SDLK_r && dados.estado!=SOCIAL_CARREGANDO && dados.estado!=SOCIAL_ATUALIZANDO){abrirInterno(&dados.pessoa,1);return;}
  if(k==SDLK_RETURN||k==SDLK_KP_ENTER){if(dados.estado==SOCIAL_PRIVADO||dados.estado==SOCIAL_INDISPONIVEL||dados.estado==SOCIAL_DESCONECTADO||dados.estado==SOCIAL_STALE){abrirInterno(&dados.pessoa,1);return;}if((dados.estado==SOCIAL_PRONTO||dados.estado==SOCIAL_ATUALIZANDO)&&selecionado>=0&&selecionado<dados.n&&dados.atividades[selecionado].imdb[0])escolhido=selecionado;}
}
int social_item_selecionado(SocialItemSelecionado *saida){if(escolhido<0||escolhido>=dados.n)return 0;if(saida){snprintf(saida->imdb,sizeof saida->imdb,"%s",dados.atividades[escolhido].imdb);snprintf(saida->titulo,sizeof saida->titulo,"%s",dados.atividades[escolhido].titulo);}escolhido=-1;return 1;}
void social_atualizar(float dt,Uint32 agora){(void)dt;(void)agora;pthread_mutex_lock(&trava);if(temPronto){SocialDados novo=pronto;if((novo.estado==SOCIAL_PRIVADO||novo.estado==SOCIAL_INDISPONIVEL||novo.estado==SOCIAL_DESCONECTADO)&&dados.n>0){novo.n=dados.n;memcpy(novo.atividades,dados.atividades,sizeof novo.atividades);novo.estado=SOCIAL_STALE;}dados=novo;temPronto=0;if(selecionado>=dados.n)selecionado=dados.n?dados.n-1:0;}pthread_mutex_unlock(&trava);}
static void texto(TxtEstilo estilo,const char *s,float x,float y,float w,int cor){txt_desenhar(txt_linha_corta(estilo,s?s:"",cor,cor,cor,255,w),x,y);}
static const char *estadoTexto(SocialEstado estado){switch(estado){case SOCIAL_CARREGANDO:return "Carregando atividade…";case SOCIAL_ATUALIZANDO:return "Atualizando · atividade anterior preservada";case SOCIAL_STALE:return "Atualização indisponível · mostrando atividade anterior";case SOCIAL_SEM_ATIVIDADE:return "Sem atividade pública por enquanto";case SOCIAL_PRIVADO:return "Atividade privada ou não compartilhada";case SOCIAL_DESCONECTADO:return "Trakt desconectado";case SOCIAL_INDISPONIVEL:return "Serviço indisponível";default:return "Atividade recente";}}
// A RAIL FIXA EMPURRA A TELA (26/09). As duas colunas foram medidas para a
// tela inteira (perfil em 96, atividade em 656 ate 1800); com a rail presa a
// coluna do perfil nascia embaixo dela. Tudo anda `R` para a direita e a
// coluna da atividade ENCOLHE pela esquerda: a borda direita fica em 1800, e o
// titulo + detalhe (a terceira coluna) se estreita na mesma proporcao.
void social_desenhar(Uint32 agora){(void)agora;float R=ajustes_rail_largura_fixa(),lx=96+R,rx=656+R,rw=1120-R,k=(1008-R)/1008,tx=rx+116+438*k,tw=500*k,cw=430*k;
  gfx_cor((GfxRect){0,0,NV_TELA_W,NV_TELA_H},0,NV_COR_FUNDO_R,NV_COR_FUNDO_G,NV_COR_FUNDO_B,1);texto(TXT_CAPTION,"ENTRE AMIGOS",lx,56,550,179);
  {GfxRect av={lx,132,176,176};GLuint tex=dados.pessoa.socialAvatar[0]?tex_obter_larg(dados.pessoa.socialAvatar,220):0;gfx_cor(av,.5f,.15f,.16f,.18f,1);if(tex){gfx_tex_aspect_atual=tex_aspecto(dados.pessoa.socialAvatar);gfx_rect(av,tex,GFX_AVATAR,0,0,0,0,1,1,1,1);gfx_tex_aspect_atual=0;}else gfx_icone((GfxRect){lx+52,184,72,72},"menu_profile",.8f,.81f,.83f,1);}
  if(dados.pessoa.socialNome[0])texto(TXT_TITULO2,dados.pessoa.socialNome,lx,346,480,245);else if(dados.estado==SOCIAL_INDISPONIVEL||dados.estado==SOCIAL_DESCONECTADO)texto(TXT_TITULO2,"Perfil indisponível",lx,346,480,245);if(dados.pessoa.socialSlug[0]){char u[160];snprintf(u,sizeof u,"@%s",dados.pessoa.socialSlug);texto(TXT_CALLOUT,u,lx,416,480,179);}if(dados.local[0])texto(TXT_CAPTION,dados.local,lx,470,480,179);if(dados.bio[0])txt_bloco(TXT_CAPTION,dados.bio,210,210,210,lx,524,480,32,1,8);
  texto(TXT_CAPTION,"Trakt · perfil público",lx,900,480,179);texto(TXT_CAPTION,"← Voltar",lx,970,480,235);texto(TXT_TITULO3,"Atividade recente",rx,64,rw+30,245);texto(TXT_CAPTION,"Pessoa · ação · título · horário",rx,122,rw+30,179);
  if(dados.estado==SOCIAL_CARREGANDO){for(int i=0;i<4;i++)gfx_cor((GfxRect){rx,210+i*170,rw,134},.06f,.12f,.125f,.14f,1);texto(TXT_CAPTION,estadoTexto(dados.estado),rx+24,248,rw-70,210);return;}
  if(dados.estado!=SOCIAL_PRONTO&&dados.estado!=SOCIAL_ATUALIZANDO&&dados.estado!=SOCIAL_STALE){texto(TXT_TITULO3,estadoTexto(dados.estado),rx,236,rw,235);if(dados.estado==SOCIAL_PRIVADO||dados.estado==SOCIAL_INDISPONIVEL||dados.estado==SOCIAL_DESCONECTADO)texto(TXT_CALLOUT,"OK ou R · tentar novamente",rx,310,rw,235);return;}
  { int inicio = selecionado > 3 ? selecionado - 3 : 0;
    for (int i = inicio; i < dados.n && i < inicio + 5; i++) {
      Atividade *a=&dados.atividades[i]; float y=196+(i-inicio)*154;
      if(i==selecionado)gfx_cor((GfxRect){rx-16,y-12,rw+40,144},.07f,.18f,.185f,.20f,1);
      if(a->poster[0]){GLuint p=tex_obter_larg(a->poster,96);if(p){gfx_tex_aspect_atual=tex_aspecto(a->poster);gfx_rect((GfxRect){rx,y,96,134},p,GFX_CARD,0,0,0,.055f,0,0,0,1);gfx_tex_aspect_atual=0;}}
      texto(TXT_MINI,a->pessoa,rx+116,y+4,cw,179);texto(TXT_CALLOUT,a->acao,rx+116,y+36,cw,235);texto(TXT_CAPTION,a->titulo,tx,y+4,tw,245);texto(TXT_CAPTION,a->detalhe,tx,y+42,tw,179);if(a->horario[0])texto(TXT_MINI,a->horario,tx,y+82,tw,179);
    }
  }
  texto(TXT_MINI,dados.estado==SOCIAL_STALE?"Atualização indisponível · mostrando atividade anterior · OK: tentar novamente":"OK · abrir título   ·   Voltar · fechar",rx,1000,rw,190);
}
