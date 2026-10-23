#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "backupconnections.h"
#include "backupcrypto.h"
#include "backupcategories.h"
// Backup wire layout, independent of the optional native multi-account UI.
#define IPTV_MAX_CONTAS 32
enum { IPTV_XTREAM=1, IPTV_M3U, IPTV_STALKER };
typedef struct {unsigned id;int tipo,ativa;char nome[96];} IptvConta;
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {IptvConta contas[IPTV_MAX_CONTAS];int n,selecionada;unsigned next;} Contas;
typedef BackupCategoriaMapa Mapa;
static int backend(int tipo){return tipo==IPTV_XTREAM?BS_XTREAM:tipo==IPTV_M3U?BS_M3U:BS_STALKER;}
static int copiarItem(BackupStores *b,const BackupStore *v){return backupstores_definir(b,v->tipo,v->sub,v->existe,v->texto,v->n);}
static int tem(const BackupStores *b,int tipo,unsigned sub,BackupStore *v){return backupstores_buscar(b,tipo,sub,v) && v->existe;}
static int configValida(int tipo,const char *s,size_t n) {
  if(n>8192)return 0;
  const char *campos[5];
  if(tipo==IPTV_XTREAM){campos[0]="servidor";campos[1]="usuario";campos[2]="senha";campos[3]=NULL;}
  else if(tipo==IPTV_M3U){campos[0]="lista";campos[1]="epg";campos[2]=NULL;}
  else {campos[0]="portal";campos[1]="mac";campos[2]="device";campos[3]="serial";campos[4]=NULL;}
  unsigned vistos=0;
  for(const char *p=s;*p;) {
    const char *e=strchr(p,'\n');if(!e)e=p+strlen(p);
    if(e==p || *p=='#' || (e==p+1 && *p=='\r')){p=*e?e+1:e;continue;}
    const char *tab=memchr(p,'\t',(size_t)(e-p));if(!tab)return 0;
    int k=0;while(campos[k] && (strlen(campos[k])!=(size_t)(tab-p) || memcmp(campos[k],p,(size_t)(tab-p))))k++;
    if(!campos[k] || vistos&(1u<<k) || e-tab>1200)return 0;
    vistos|=1u<<k;
    for(const char *v=tab+1;v<e;v++)if((unsigned char)*v<32 && *v!='\r')return 0;
    p=*e?e+1:e;
  }
  return 1;
}
static int numeroConta(const char *s,unsigned max,unsigned *out) {
  unsigned v=0;if(!s || !*s)return 0;
  for(;*s;s++){unsigned n=(unsigned char)*s-'0';if(n>9 || n>max || v>(max-n)/10u)return 0;v=v*10u+n;}
  *out=v;return 1;
}
static int lerContas(const BackupStores *b,Contas *c) {
  memset(c,0,sizeof *c);c->next=1;BackupStore reg;
  if(tem(b,BS_IPTV_REGISTRY,0,&reg)) {
    if(reg.n>8192)return 0;
    char *s=strdup(reg.texto);if(!s)return 0;
    int ok=1,viuNext=0,viuSel=0;
    for(char *p=s;*p && ok;) {
      char *e=strchr(p,'\n');if(e)*e++=0;else e=p+strlen(p);
      if(!*p || *p=='#'){p=e;continue;}
      char *campos[6]={p};int nc=1;
      for(char *v=p;*v;v++)if(*v=='\t') {if(nc>=6){ok=0;break;}*v=0;campos[nc++]=v+1;}
      if(!ok)break;
      unsigned id,tipo,ativa;
      if(nc==2 && !strcmp(campos[0],"next")) {
        ok=!viuNext++ && numeroConta(campos[1],UINT_MAX-1u,&id) && id>0;if(ok)c->next=id;
      } else if(nc==2 && !strcmp(campos[0],"selected")) {
        ok=!viuSel++ && numeroConta(campos[1],IPTV_MAX_CONTAS-1u,&id);if(ok)c->selecionada=(int)id;
      } else if(nc==5 && !strcmp(campos[0],"account")) {
        ok=c->n<IPTV_MAX_CONTAS && numeroConta(campos[1],UINT_MAX-1u,&id) && numeroConta(campos[2],IPTV_STALKER,&tipo) &&
          tipo>=IPTV_XTREAM && numeroConta(campos[3],1,&ativa) && *campos[4] && strlen(campos[4])<96;
        for(const unsigned char *v=(const unsigned char *)campos[4];*v;v++)if(*v<32 || *v==127)ok=0;
        if(ok)for(int i=0;i<c->n;i++)if(c->contas[i].tipo==(int)tipo && c->contas[i].id==id)ok=0;
        BackupStore cfg;
        if(ok)ok=tem(b,backend((int)tipo),id,&cfg) && configValida((int)tipo,cfg.texto,cfg.n);
        if(ok){c->contas[c->n]=(IptvConta){.id=id,.tipo=(int)tipo,.ativa=(int)ativa};snprintf(c->contas[c->n++].nome,96,"%s",campos[4]);}
      } else ok=0;
      p=e;
    }
    backupcrypto_limpar(s,reg.n);free(s);
    if(!ok || (c->n?c->selecionada>=c->n:c->selecionada!=0))return 0;
  } else {
    for(int t=IPTV_XTREAM;t<=IPTV_STALKER;t++) {
      BackupStore v;
      if(tem(b,backend(t),0,&v) && v.n) {
        if(!configValida(t,v.texto,v.n))return 0;
        c->contas[c->n]=(IptvConta){.tipo=t,.ativa=1};
        snprintf(c->contas[c->n++].nome,96,"%s",t==IPTV_XTREAM?"Xtream":t==IPTV_M3U?"M3U":"Stalker");
      }
    }
  }
  return 1;
}
static int salvarContas(BackupStores *b,const Contas *c) {
  char s[8192];size_t n=(size_t)snprintf(s,sizeof s,"next\t%u\nselected\t%d\n",c->next,c->selecionada);
  for(int i=0;i<c->n;i++) {
    const IptvConta *v=&c->contas[i];int k=snprintf(s+n,sizeof s-n,"account\t%u\t%d\t%d\t%s\n",v->id,v->tipo,v->ativa,v->nome);
    if(k<0 || (size_t)k>=sizeof s-n)return 0;
    n+=(size_t)k;
  }
  return backupstores_definir(b,BS_IPTV_REGISTRY,0,1,s,n);
}
static int campo(const char *s,const char *nome,const char **out,size_t *n) {
  size_t k=strlen(nome);
  for(const char *p=s;*p;) {
    const char *e=strchr(p,'\n');if(!e)e=p+strlen(p);
    if((size_t)(e-p)>k && !memcmp(p,nome,k) && p[k]=='\t') {
      *out=p+k+1;*n=(size_t)(e-*out);if(*n && (*out)[*n-1]=='\r')--*n;return 1;
    }
    p=*e?e+1:e;
  }
  *out="";*n=0;return 0;
}
static int mesmoCampo(const char *a,const char *b,const char *nome,int obrigatorio) {
  const char *x,*y;size_t nx,ny;campo(a,nome,&x,&nx);campo(b,nome,&y,&ny);
  return (!obrigatorio || nx) && nx==ny && !memcmp(x,y,nx);
}
static int mesmaConta(int tipo,const char *a,const char *b) {
  return tipo==IPTV_M3U?mesmoCampo(a,b,"lista",1):
    tipo==IPTV_XTREAM?(mesmoCampo(a,b,"servidor",1) && mesmoCampo(a,b,"usuario",1)):
    (mesmoCampo(a,b,"portal",1) && mesmoCampo(a,b,"mac",1));
}
static char *remapear(const char *s,const Mapa *mapa,int nm) {
  size_t n=strlen(s),cap=n+(size_t)nm*16u+128u,pos=0;
  char *out=malloc(cap);if(!out)return NULL;
  for(const char *p=s;*p;) {
    int feito=0;
    for(int i=0;i<nm && !feito;i++) {
      if(mapa[i].antes==mapa[i].depois)continue;
      const char *prefixo=mapa[i].tipo==IPTV_XTREAM?"xtream:":mapa[i].tipo==IPTV_M3U?"nvm3u:":"stalker:";
      char antigo[40],novo[40];
      if(mapa[i].antes)snprintf(antigo,sizeof antigo,"%s@%08x:",prefixo,mapa[i].antes);else snprintf(antigo,sizeof antigo,"%s",prefixo);
      if(mapa[i].depois)snprintf(novo,sizeof novo,"%s@%08x:",prefixo,mapa[i].depois);else snprintf(novo,sizeof novo,"%s",prefixo);
      size_t k=strlen(antigo),l=strlen(novo);
      if(!strncmp(p,antigo,k) && (mapa[i].antes || p[k]!='@')) {
        if(pos+l+2>=cap){size_t nc=cap*2;char *tmp=realloc(out,nc);if(!tmp){free(out);return NULL;}out=tmp;cap=nc;}
        memcpy(out+pos,novo,l);pos+=l;p+=k;feito=1;
      }
    }
    if(!feito) {
      if(pos+2>=cap){size_t nc=cap*2;char *tmp=realloc(out,nc);if(!tmp){free(out);return NULL;}out=tmp;cap=nc;}
      out[pos++]=*p++;
    }
  }
  out[pos]=0;return out;
}
static char *ordemLocal(const char *s,int manterOrdem) {
  size_t n=strlen(s),pos=0;char *out=malloc(n+2);if(!out)return NULL;
  for(const char *p=s;*p;) {
    const char *e=strchr(p,'\n');if(!e)e=p+strlen(p);
    int ordem=(size_t)(e-p)>=6 && !memcmp(p,"ordem ",6);
    if(ordem==manterOrdem){size_t k=(size_t)(e-p);memcpy(out+pos,p,k);pos+=k;out[pos++]='\n';}
    p=*e?e+1:e;
  }
  out[pos]=0;return out;
}
static char *limiteLocal(const char *s,int incluir) {
  size_t pos=0;char *out=malloc(strlen(s)+2u);if(!out)return NULL;
  for(const char *p=s;*p;) {
    const char *e=strchr(p,'\n');if(!e)e=p+strlen(p);
    int limite=(size_t)(e-p)>=7 && !memcmp(p,"limite ",7);
    if(limite==incluir){size_t k=(size_t)(e-p);memcpy(out+pos,p,k);pos+=k;out[pos++]='\n';}
    p=*e?e+1:e;
  }
  out[pos]=0;return out;
}
int backupconnections_selecionar(const BackupStores *todos,int hardware,BackupStores **saida) {
  if(saida)*saida=NULL;
  if(!todos || !saida)return 0;
  BackupStores *b=backupstores_novo();if(!b)return 0;
  for(int i=0;i<backupstores_n(todos);i++) {
    BackupStore v;backupstores_item(todos,i,&v);
    if(!hardware && (backupstores_flags(v.tipo)&BS_HARDWARE))continue;
    if(!hardware && v.tipo==BS_HOME_ROWS) {
      char *s=limiteLocal(v.texto,0);if(!s)goto falha;
      int ok=backupstores_adicionar(b,v.tipo,v.sub,v.existe,s,strlen(s));free(s);if(!ok)goto falha;
    } else if(!copiarItem(b,&v))goto falha;
  }
  *saida=b;return 1;
falha:backupstores_liberar(b);return 0;
}
static char *pluginsLocais(const char *s,int local) {
  size_t n=strlen(s),pos=0;char *out=malloc(n+2);if(!out)return NULL;
  for(const char *p=s;*p;) {
    const char *e=strchr(p,'\n');if(!e)e=p+strlen(p);
    int proprio=((size_t)(e-p)>=7 && !memcmp(p,"ligado\t",7)) || ((size_t)(e-p)>=8 && !memcmp(p,"scraper\t",8));
    if(proprio==local){size_t k=(size_t)(e-p);memcpy(out+pos,p,k);pos+=k;out[pos++]='\n';}
    p=*e?e+1:e;
  }
  out[pos]=0;return out;
}

static char *urlsMesclar(const char *dst,const char *src,int preferencias) {
  size_t cap=strlen(dst)+strlen(src)+3u;char *out=calloc(cap,1);if(!out)return NULL;
  const char *listas[]={dst,src};int n=0;
  for(int l=0;l<2;l++)for(const char *p=listas[l];*p;) {
    const char *e=strchr(p,'\n');if(!e)e=p+strlen(p);size_t len=(size_t)(e-p);
    if(!len){p=*e?e+1:e;continue;}
    const char *a=memchr(p,'\t',len),*b=a?memchr(a+1,'\t',(size_t)(e-a-1)):NULL;
    if(len>2300 || !a || (!preferencias && (!b ||
        !((a-p==3 && !memcmp(p,"m3u",3)) || (a-p==5 && !memcmp(p,"xmltv",5))) ||
        b-a>64 || e-b<2 || e-b>1024 ||
        (strncmp(b+1,"http://",7) && strncmp(b+1,"https://",8))))){free(out);return NULL;}
    int repetido=0;
    for(const char *q=out;*q;) {
      const char *fim=strchr(q,'\n');if(!fim)fim=q+strlen(q);
      const char *qa=memchr(q,'\t',(size_t)(fim-q));
      const char *qb=qa?memchr(qa+1,'\t',(size_t)(fim-qa-1)):NULL;
      if(preferencias)repetido=qa && qa-q==a-p && !memcmp(q,p,(size_t)(a-p));
      else repetido=qa && qb && qa-q==a-p && !memcmp(q,p,(size_t)(a-p)) && e-b==fim-qb && !memcmp(b,qb,(size_t)(e-b));
      if(repetido)break;
      q=*fim?fim+1:fim;
    }
    if(!repetido) {
      if(!preferencias && ++n>8){free(out);return NULL;}
      size_t pos=strlen(out);memcpy(out+pos,p,len);out[pos+len]='\n';out[pos+len+1]=0;
    }
    p=*e?e+1:e;
  }
  return out;
}
int backupconnections_capturar(const BackupStores *atual,BackupStores **saida) {
  if(saida)*saida=NULL;
  if(!saida || !atual)return 0;
  Contas c;if(!lerContas(atual,&c))return 0;
  BackupStores *b=backupstores_novo();if(!b)return 0;
  for(int i=0;i<backupstores_n(atual);i++) {
    BackupStore v;backupstores_item(atual,i,&v);int incluir=1;
    if(v.tipo==BS_SOURCE_ORDER)continue; // Nuvio c_settings_v1 owns source rules.
    if(v.tipo==BS_PROVIDER_SCOPE || v.tipo==BS_DEBRID_SHARED || v.tipo==BS_DEBRID_PROFILE)continue; /* effective local overrides added by owner */
    if(v.tipo==BS_XTREAM || v.tipo==BS_M3U || v.tipo==BS_STALKER) {
      incluir=0;for(int j=0;j<c.n;j++)if(backend(c.contas[j].tipo)==v.tipo && c.contas[j].id==v.sub)incluir=1;
    }
    if(!incluir)continue;
    if(v.tipo==BS_LTV_URLS || v.tipo==BS_LTV_PREFS) {
      char *verificado=urlsMesclar("",v.texto,v.tipo==BS_LTV_PREFS);if(!verificado)goto falha;
      int ok=backupstores_adicionar(b,v.tipo,v.sub,v.existe,verificado,strlen(verificado));free(verificado);if(!ok)goto falha;
    } else if(v.tipo==BS_HOME_ROWS) {
      char *limite=limiteLocal(v.texto,1);if(!limite)goto falha;
      int ok=backupstores_adicionar(b,v.tipo,v.sub,v.existe,limite,strlen(limite));free(limite);if(!ok)goto falha;
    } else if(v.tipo==BS_SOURCE_ORDER || v.tipo==BS_PLUGINS_SHARED || v.tipo==BS_PLUGINS_PROFILE) {
      char *ordem=v.tipo==BS_SOURCE_ORDER?ordemLocal(v.texto,1):pluginsLocais(v.texto,1);if(!ordem)goto falha;
      int ok=backupstores_adicionar(b,v.tipo,v.sub,1,ordem,strlen(ordem));free(ordem);if(!ok)goto falha;
    } else if(!copiarItem(b,&v))goto falha;
  }
  if(c.n && !salvarContas(b,&c))goto falha;
  *saida=b;return 1;
falha:backupstores_liberar(b);return 0;
}
int backupconnections_mesclar(const BackupStores *atual,const BackupStores *entrada,BackupStores **saida) {
  if(saida)*saida=NULL;
  if(!atual || !entrada || !saida)return 0;
  Contas dst,src;if(!lerContas(atual,&dst) || !lerContas(entrada,&src))return 0;
  BackupStores *b=backupstores_copiar(atual);if(!b)return 0;
  Mapa mapa[IPTV_MAX_CONTAS];int nm=0;
  for(int i=0;i<src.n;i++) {
    IptvConta v=src.contas[i];BackupStore cfg;tem(entrada,backend(v.tipo),v.id,&cfg);
    unsigned id=v.id;int achou=0;
    for(int j=0;j<dst.n;j++)if(dst.contas[j].tipo==v.tipo) {
      BackupStore outro;if(!tem(b,backend(v.tipo),dst.contas[j].id,&outro))continue;
      if(mesmaConta(v.tipo,cfg.texto,outro.texto)){id=dst.contas[j].id;achou=1;break;}
    }
    if(!achou) {
      if(dst.n>=IPTV_MAX_CONTAS)goto falha; /* never silently drop accounts */
      BackupStore ocupado;
      if(backupstores_buscar(b,backend(v.tipo),id,&ocupado)) {
        id=dst.next;
        while(id<UINT_MAX-1u && backupstores_buscar(b,backend(v.tipo),id,&ocupado))id++;
        if(id>=UINT_MAX-1u)goto falha;
      }
      if(id>=dst.next)dst.next=id+1;
      if(!dst.next || dst.next==UINT_MAX)goto falha;
      v.id=id;dst.contas[dst.n++]=v;
      if(!backupstores_definir(b,backend(v.tipo),id,1,cfg.texto,cfg.n))goto falha;
    }
    mapa[nm++]=(Mapa){v.tipo,src.contas[i].id,id};
  }
  for(int i=0;i<backupstores_n(entrada);i++) {
    BackupStore v,d;backupstores_item(entrada,i,&v);
    if(v.tipo==BS_SOURCE_ORDER)continue;
    if(!v.existe || v.tipo==BS_IPTV_REGISTRY || v.tipo==BS_XTREAM || v.tipo==BS_M3U || v.tipo==BS_STALKER ||
       v.tipo==BS_DEBRID_SHARED)continue;
    if(v.tipo==BS_GUIDE_CATEGORIES || v.tipo==BS_GUIDE_CATEGORY_ALIASES)continue;
    if(v.tipo==BS_LTV_URLS || v.tipo==BS_LTV_PREFS) {
      const char *velho=tem(atual,v.tipo,0,&d)?d.texto:"";
      char *junto=urlsMesclar(velho,v.texto,v.tipo==BS_LTV_PREFS);if(!junto)goto falha;
      int ok=backupstores_definir(b,v.tipo,0,1,junto,strlen(junto));free(junto);if(!ok)goto falha;
    } else if(v.tipo==BS_HOME_ROWS) {
      const char *velho=tem(atual,v.tipo,0,&d)?d.texto:"";
      char *limite=limiteLocal(v.texto,1),*resto=limiteLocal(velho,0);
      if(!limite || !resto){free(limite);free(resto);goto falha;}
      if(!*limite){free(limite);limite=limiteLocal(velho,1);if(!limite){free(resto);goto falha;}}
      size_t n=strlen(resto)+strlen(limite);char *junto=malloc(n+1u);
      if(!junto){free(limite);free(resto);goto falha;}
      strcpy(junto,resto);strcat(junto,limite);
      int ok=backupstores_definir(b,v.tipo,0,1,junto,n);free(junto);free(resto);free(limite);if(!ok)goto falha;
    } else if(v.tipo==BS_SOURCE_ORDER || v.tipo==BS_PLUGINS_SHARED || v.tipo==BS_PLUGINS_PROFILE) {
      const char *velho=tem(atual,v.tipo,0,&d)?d.texto:"";
      char *local=v.tipo==BS_SOURCE_ORDER?ordemLocal(v.texto,1):pluginsLocais(v.texto,1);
      char *resto=v.tipo==BS_SOURCE_ORDER?ordemLocal(velho,0):pluginsLocais(velho,0);
      if(!local || !resto){free(local);free(resto);goto falha;}
      size_t n=strlen(local)+strlen(resto);char *junto=malloc(n+1);
      if(!junto){free(local);free(resto);goto falha;}
      strcpy(junto,resto);strcat(junto,local);int ok=backupstores_definir(b,v.tipo,0,1,junto,n);
      free(local);free(resto);free(junto);if(!ok)goto falha;
    } else if(v.tipo==BS_GUIDE_FAVORITES || v.tipo==BS_GUIDE_REMINDERS) {
      char *s=remapear(v.texto,mapa,nm);if(!s)goto falha;
      int ok=backupstores_definir(b,v.tipo,v.sub,1,s,strlen(s));free(s);if(!ok)goto falha;
    } else if(v.tipo==BS_DEBRID_PROFILE || v.tipo==BS_PROVIDER_SCOPE) {
      if(!copiarItem(b,&v))goto falha;
    } else if((backupstores_flags(v.tipo)&BS_CONNECTION) && tem(atual,v.tipo,v.sub,&d) && d.n)continue;
    else if(!copiarItem(b,&v))goto falha;
  }
  BackupStore srcCats,srcAlias,dstCats,dstAlias;
  if(tem(entrada,BS_GUIDE_CATEGORIES,0,&srcCats) || tem(entrada,BS_GUIDE_CATEGORY_ALIASES,0,&srcAlias)) {
    const char *sc=tem(entrada,BS_GUIDE_CATEGORIES,0,&srcCats)?srcCats.texto:"";
    const char *sa=tem(entrada,BS_GUIDE_CATEGORY_ALIASES,0,&srcAlias)?srcAlias.texto:"";
    const char *dc=tem(atual,BS_GUIDE_CATEGORIES,0,&dstCats)?dstCats.texto:"";
    const char *da=tem(atual,BS_GUIDE_CATEGORY_ALIASES,0,&dstAlias)?dstAlias.texto:"";
    Contas originais;if(!lerContas(atual,&originais))goto falha;
    Mapa destinoMapa[IPTV_MAX_CONTAS];for(int i=0;i<originais.n;i++)destinoMapa[i]=(Mapa){originais.contas[i].tipo,originais.contas[i].id,originais.contas[i].id};
    char *aliases=NULL;int ok=backupcategorias_mesclar(da,dc,destinoMapa,originais.n,sa,sc,mapa,nm,&aliases) &&
      backupstores_definir(b,BS_GUIDE_CATEGORY_ALIASES,0,1,aliases,strlen(aliases)) && backupstores_definir(b,BS_GUIDE_CATEGORIES,0,1,"",0);
    free(aliases);if(!ok)goto falha;
  }
  if(src.n && !salvarContas(b,&dst))goto falha;
  *saida=b;return 1;
falha:backupstores_liberar(b);return 0;
}
