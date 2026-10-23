#include "backupstores.h"
#include "backupcrypto.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {const char *id,*nome;unsigned flags;} Tipo;
#define P BS_PROFILE
#define C BS_CONNECTION
#define H BS_HARDWARE
#define M BS_MULTIPLE
static const Tipo tipos[BS_N]={
  {"home-rows","fileirasui-p%d.txt",P},
  {"library-ui","bibliotecaui-p%d.txt",P},
  {"collection-ui","colecao-exibicao-p%d.txt",P},
  {"saved-ui","salvos-org-p%d.txt",P},
  {"saved-tabs","salvos-abas-p%d.txt",P},
  {"central-ui","central-p%d.txt",P},
  {"guide-favorites","guia-fav-p%d.txt",P},
  {"guide-mode","guia-modo-p%d.txt",P},
  {"guide-preview","guia-preview-p%d.txt",P},
  {"guide-addons","guia-addons-p%d.txt",P},
  {"guide-categories","guia-categorias-p%d.txt",P},
  {"guide-reminders","guia-lembretes-p%d.txt",P},
  {"guide-category-aliases","guia-categorias-origem-p%d.txt",P},
  {"source-order","fonteregra-p%d.txt",P},
  {"badge-choice","selos-p%d.txt",P},
  {"badge-custom","selos-p%d-%u.json",P|M},
  {"poster-connection","posteres.txt",C},
  {"fanart-key","fanart.txt",C},
  {"seekr-key","seekr.txt",C},
  {"p2p-connection","p2p.txt",C},
  {"curation","curadoria.txt",0},
  {"debrid-shared","debrid.txt",C},
  {"debrid-profile",NULL,P|C},
  {"provider-scope",NULL,P|C},
  {"iptv-accounts","livetv-contas-p%d.txt",P|C},
  {"xtream","xtream-p%d-s%u.txt",P|C|M},
  {"m3u","m3u-p%d-s%u.txt",P|C|M},
  {"stalker","stalker-p%d-s%u.txt",P|C|M},
  {"jellyfin","jellyfin-p%d.txt",P|C},
  {"emby","emby-p%d.txt",P|C},
  {"plex","plex-p%d.txt",P|C},
  {"imported-font-choices","fontes-legenda.txt",0},
  {"pinned-lists","listas-p%d.txt",P},
  {"agenda-reminders","lembretes-p%d.txt",P},
  {"plugins-shared",NULL,P},
  {"plugins-profile",NULL,P},
  {"pip","pip-modo.txt",H},
  {"stats-size","estatvivo-tamanho.txt",0},
  {"stats-tiles","estatvivo-tiles-p%d-%u.txt",P|M},
  {"subtitle-memory","legmemoria-p%d.txt",P},
  {"audio-memory","audiomemoria-p%d.txt",P},
  {"art-memory","arte-escolhida-p%d.txt",P},
  {"manual-source-memory","fontepref-p%d.txt",P},
  {"external-player","player-externo.txt",H},
  {"live-urls","ltv-urls-p%d.txt",P|C},
  {"live-source-preferences","ltv-fontes-p%d.txt",P},
  {"live-favorites","guia-ltv-fav-p%d.txt",P},
  {"live-epg-mapping","ltv-epgmapa-p%d.txt",P},
};
#undef P
#undef C
#undef H
#undef M
struct BackupStores {
  struct {int tipo,existe;uint32_t sub;char *texto;size_t n;} item[BACKUP_STORES_MAX];
  int n;size_t bytes;
};
static int subValido(int tipo,uint32_t sub) {
  if(tipo<0 || tipo>=BS_N || sub==UINT32_MAX)return 0;
  if(!(tipos[tipo].flags&BS_MULTIPLE))return sub==0;
  if(tipo==BS_BADGES_CUSTOM)return sub<3; /* SELOS_MAX_PACOTES */
  if(tipo==BS_STATS_TILES)return sub<3; /* EV_TAM_N */
  return 1;
}
BackupStores *backupstores_novo(void){return calloc(1,sizeof(BackupStores));}
void backupstores_liberar(BackupStores *b) {
  if(!b)return;
  for(int i=0;i<b->n;i++){backupcrypto_limpar(b->item[i].texto,b->item[i].n);free(b->item[i].texto);}
  backupcrypto_limpar(b,sizeof *b);free(b);
}
const char *backupstores_tipo(int tipo){return tipo>=0 && tipo<BS_N?tipos[tipo].id:NULL;}
unsigned backupstores_flags(int tipo){return tipo>=0 && tipo<BS_N?tipos[tipo].flags:0;}
int backupstores_n(const BackupStores *b){return b?b->n:0;}
int backupstores_item(const BackupStores *b,int i,BackupStore *out) {
  if(!b || !out || i<0 || i>=b->n)return 0;
  *out=(BackupStore){b->item[i].tipo,b->item[i].existe,b->item[i].sub,b->item[i].texto,b->item[i].n};return 1;
}
int backupstores_buscar(const BackupStores *b,int tipo,uint32_t sub,BackupStore *out) {
  if(!b)return 0;
  for(int i=0;i<b->n;i++)if(b->item[i].tipo==tipo && b->item[i].sub==sub)return backupstores_item(b,i,out);
  return 0;
}
int backupstores_adicionar(BackupStores *b,int tipo,uint32_t sub,int existe,const char *texto,size_t n) {
  BackupStore outro;
  if(!b || !subValido(tipo,sub) || (existe!=0 && existe!=1) || b->n>=BACKUP_STORES_MAX ||
     n>BACKUP_STORE_BYTES || n>BACKUP_STORES_BYTES-b->bytes ||
     (!existe && n) || (n && (!texto || memchr(texto,0,n))) || backupstores_buscar(b,tipo,sub,&outro))return 0;
  char *s=malloc(n+1u);if(!s)return 0;
  if(n)memcpy(s,texto,n);
  s[n]=0;
  int i=b->n++;b->item[i].tipo=tipo;b->item[i].sub=sub;b->item[i].existe=existe;
  b->item[i].texto=s;b->item[i].n=n;b->bytes+=n;return 1;
}
int backupstores_remover(BackupStores *b,int tipo,uint32_t sub) {
  if(!b)return 0;
  for(int i=0;i<b->n;i++)if(b->item[i].tipo==tipo && b->item[i].sub==sub) {
    backupcrypto_limpar(b->item[i].texto,b->item[i].n);free(b->item[i].texto);b->bytes-=b->item[i].n;
    memmove(&b->item[i],&b->item[i+1],(size_t)(b->n-i-1)*sizeof b->item[0]);b->n--;return 1;
  }
  return 1;
}
int backupstores_definir(BackupStores *b,int tipo,uint32_t sub,int existe,const char *texto,size_t n) {
  BackupStore antigo;
  if(!backupstores_buscar(b,tipo,sub,&antigo))return backupstores_adicionar(b,tipo,sub,existe,texto,n);
  if((existe!=0 && existe!=1) || n>BACKUP_STORE_BYTES ||
     n>BACKUP_STORES_BYTES-(b->bytes-antigo.n) || (!existe && n) || (n && (!texto || memchr(texto,0,n))))return 0;
  char *copia=malloc(n+1u);if(!copia)return 0;
  if(n)memcpy(copia,texto,n);
  copia[n]=0;
  for(int i=0;i<b->n;i++)if(b->item[i].tipo==tipo && b->item[i].sub==sub) {
    backupcrypto_limpar(b->item[i].texto,b->item[i].n);free(b->item[i].texto);
    b->bytes=b->bytes-b->item[i].n+n;b->item[i].texto=copia;b->item[i].n=n;b->item[i].existe=existe;return 1;
  }
  free(copia);return 0;
}
BackupStores *backupstores_copiar(const BackupStores *b) {
  if(!b)return NULL;
  BackupStores *out=backupstores_novo();if(!out)return NULL;
  for(int i=0;i<b->n;i++)if(!backupstores_adicionar(out,b->item[i].tipo,b->item[i].sub,b->item[i].existe,b->item[i].texto,b->item[i].n)) {
    backupstores_liberar(out);return NULL;
  }
  return out;
}
int backupstores_exportar(const BackupStores *b,char **saida,size_t *n) {
  if(saida)*saida=NULL;
  if(n)*n=0;
  if(!b || !saida || !n)return 0;
  size_t cap=b->bytes+(size_t)b->n*100u+80u;
  if(cap>BACKUP_STORES_BYTES+BACKUP_STORES_MAX*100u+80u)return 0;
  char *out=malloc(cap);if(!out)return 0;
  size_t pos=(size_t)snprintf(out,cap,"NUVIO-LOCAL-STORES 1\ncount %d\n",b->n);
  for(int i=0;i<b->n;i++) {
    int k=snprintf(out+pos,cap-pos,"file %s %u %d %zu\n",tipos[b->item[i].tipo].id,
      b->item[i].sub,b->item[i].existe,b->item[i].n);
    if(k<0 || (size_t)k>=cap-pos || b->item[i].n+1u>=cap-pos-(size_t)k)goto falha;
    pos+=(size_t)k;memcpy(out+pos,b->item[i].texto,b->item[i].n);pos+=b->item[i].n;out[pos++]='\n';
  }
  out[pos]=0;*saida=out;*n=pos;return 1;
falha:
  backupcrypto_limpar(out,cap);free(out);return 0;
}
static int numero(const char *s,uint64_t max,uint64_t *out) {
  uint64_t n=0;if(!s || !*s)return 0;
  for(;*s;s++) {if(*s<'0' || *s>'9' || (unsigned)(*s-'0')>max || n>(max-(unsigned)(*s-'0'))/10u)return 0;n=n*10u+(unsigned)(*s-'0');}
  *out=n;return 1;
}
int backupstores_ler(const char *s,size_t n,BackupStores **saida) {
  if(saida)*saida=NULL;
  static const char cab[]="NUVIO-LOCAL-STORES 1\ncount ";
  if(!saida || !s || n<sizeof cab || n>BACKUP_STORES_BYTES+BACKUP_STORES_MAX*100u+80u ||
     memchr(s,0,n) || memcmp(s,cab,sizeof cab-1u))return 0;
  const char *p=s+sizeof cab-1u,*fim=s+n,*e=memchr(p,'\n',(size_t)(fim-p));
  char linha[128];uint64_t count;
  if(!e || e-p>6)return 0;
  memcpy(linha,p,(size_t)(e-p));linha[e-p]=0;
  if(!numero(linha,BACKUP_STORES_MAX,&count))return 0;
  BackupStores *b=backupstores_novo();if(!b)return 0;p=e+1;
  for(uint64_t i=0;i<count;i++) {
    e=memchr(p,'\n',(size_t)(fim-p));if(!e || (size_t)(e-p)>=sizeof linha)goto falha;
    memcpy(linha,p,(size_t)(e-p));linha[e-p]=0;
    char *tokens[5],*v=linha;
    for(int k=0;k<5;k++) {
      tokens[k]=v;char *space=strchr(v,' ');
      if(k<4){if(!space || space==v)goto falha;*space=0;v=space+1;}
      else if(space || !*v)goto falha;
    }
    int tipo=-1;for(int k=0;k<BS_N;k++)if(!strcmp(tokens[1],tipos[k].id))tipo=k;
    uint64_t sub,existe,bytes;
    if(strcmp(tokens[0],"file") || !numero(tokens[2],UINT32_MAX-1u,&sub) ||
       !numero(tokens[3],1,&existe) || !numero(tokens[4],BACKUP_STORE_BYTES,&bytes))goto falha;
    p=e+1;if(bytes>=(uint64_t)(fim-p) || p[bytes]!='\n' ||
       !backupstores_adicionar(b,tipo,(uint32_t)sub,(int)existe,p,(size_t)bytes))goto falha;
    p+=bytes+1u;
  }
  if(p!=fim)goto falha;
  *saida=b;return 1;
falha:
  backupstores_liberar(b);return 0;
}
int backupstores_iguais(const BackupStores *a,const BackupStores *b) {
  if(!a || !b)return 0;
  for(int lado=0;lado<2;lado++) {
    const BackupStores *x=lado?b:a,*y=lado?a:b;
    for(int i=0;i<x->n;i++)if(x->item[i].existe) {
      BackupStore v;
      if(!backupstores_buscar(y,x->item[i].tipo,x->item[i].sub,&v) || !v.existe ||
         v.n!=x->item[i].n || memcmp(v.texto,x->item[i].texto,v.n))return 0;
    }
  }
  return 1;
}
int backupstores_nome(int tipo,uint32_t sub,const char *dono,int perfil,char *out,size_t cap) {
  if(out && cap)*out=0;
  if(!out || !cap || !subValido(tipo,sub) || !dono || !*dono || strlen(dono)>80 || perfil<1)return 0;
  for(const unsigned char *p=(const unsigned char *)dono;*p;p++)
    if(!((*p>='a' && *p<='z') || (*p>='A' && *p<='Z') || (*p>='0' && *p<='9') || *p=='_' || *p=='-'))return 0;
  int n;
  if(tipo==BS_PLUGINS_SHARED || tipo==BS_PLUGINS_PROFILE) {
    uint64_t h=1469598103934665603ULL;
    for(const unsigned char *p=(const unsigned char *)dono;*p;p++)h=(h^*p)*1099511628211ULL;
    n=snprintf(out,cap,"plugins-%08llx-p%d.txt",(unsigned long long)(h&0xffffffffULL),tipo==BS_PLUGINS_SHARED?1:perfil);
  } else if(tipo==BS_DEBRID_PROFILE || tipo==BS_PROVIDER_SCOPE)
    n=snprintf(out,cap,"%s-%s-p%d.txt",tipo==BS_DEBRID_PROFILE?"debrid":"provider-scope",dono,perfil);
  else if((tipo==BS_XTREAM || tipo==BS_M3U || tipo==BS_STALKER) && !sub)
    n=snprintf(out,cap,"%s-p%d.txt",tipo==BS_XTREAM?"xtream":tipo==BS_M3U?"m3u":"stalker",perfil);
  else if(tipos[tipo].flags&BS_MULTIPLE)n=snprintf(out,cap,tipos[tipo].nome,perfil,sub);
  else if(tipos[tipo].flags&BS_PROFILE)n=snprintf(out,cap,tipos[tipo].nome,perfil);
  else n=snprintf(out,cap,"%s",tipos[tipo].nome);
  if(n<0 || (size_t)n>=cap){*out=0;return 0;}return 1;
}

int backupstores_reconhecer(const char *nome,const char *dono,int perfil,int *tipo,uint32_t *sub) {
  if(!nome || !tipo || !sub)return 0;
  char esperado[160];
  for(int t=0;t<BS_N;t++) {
    if(backupstores_nome(t,0,dono,perfil,esperado,sizeof esperado) && !strcmp(nome,esperado)){*tipo=t;*sub=0;return 1;}
    if(!(tipos[t].flags&BS_MULTIPLE))continue;
    const char *fim=strrchr(nome,'.'),*p=fim;
    if(!fim)continue;
    while(p>nome && p[-1]>='0' && p[-1]<='9')p--;
    if(p==fim || (size_t)(fim-p)>10)continue;
    char numeroTexto[16];memcpy(numeroTexto,p,(size_t)(fim-p));numeroTexto[fim-p]=0;
    uint64_t id;
    if(!numero(numeroTexto,UINT32_MAX-1u,&id))continue;
    if(backupstores_nome(t,(uint32_t)id,dono,perfil,esperado,sizeof esperado) && !strcmp(nome,esperado)){*tipo=t;*sub=(uint32_t)id;return 1;}
  }
  return 0;
}
