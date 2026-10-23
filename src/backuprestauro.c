#include "backuprestauro.h"
#include "backupplano_interno.h"
#include "backupstores.h"
#include "backupfonts.h"
#include "backupcrypto.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>
#define BJ_MAX (BACKUP_LOCAL_MAX+128u)
static int ioValido(const BackupIO *io){return io && io->ler && io->gravar && io->apagar;}
static int gravarDiario(const BackupIO *io,const char *s,size_t n){return io->gravar(io->ctx,NULL,0,BACKUP_IO_DIARIO,s,n);}
static int limpar(const BackupIO *io){return io->apagar(io->ctx,NULL,0,BACKUP_IO_DIARIO);}
static int planoValido(const BackupPlano *p) {
  if(!p)return 0;
  BackupLocal *b=backuplocal_novo(p->dono,p->perfil,p->criado);if(!b)return 0;
  backuplocal_liberar(b);int n=0;
  for(int i=0;i<BACKUP_PARTES_N;i++)if(p->parte[i].usada) {
    n++;
    if(!p->parte[i].depois || p->parte[i].nDepois>BACKUP_PARTE_MAX ||
       memchr(p->parte[i].depois,0,p->parte[i].nDepois) ||
       (p->parte[i].existia!=0 && p->parte[i].existia!=1) ||
       p->parte[i].nAntes>BACKUP_PARTE_MAX-2u ||
       (!p->parte[i].existia && (p->parte[i].antes || p->parte[i].nAntes)) ||
       (p->parte[i].existia && (!p->parte[i].antes || memchr(p->parte[i].antes,0,p->parte[i].nAntes))))return 0;
  }
  return n>0;
}
static char *diario(const BackupPlano *p,char estado,size_t *n) {
  BackupLocal *b=backuplocal_novo(p->dono,p->perfil,p->criado);if(!b)return NULL;
  int ok=1;
  for(int i=0;i<BACKUP_PARTES_N && ok;i++)if(p->parte[i].usada) {
    size_t k=p->parte[i].nAntes;char *s=malloc(k+2);if(!s){ok=0;break;}
    s[0]=p->parte[i].existia?'1':'0';s[1]='\n';
    if(k)memcpy(s+2,p->parte[i].antes,k);
    ok=backuplocal_adicionar(b,i,s,k+2);free(s);
  }
  char *corpo=NULL;size_t tamanho=0;
  if(ok)ok=backuplocal_exportar(b,&corpo,&tamanho);
  backuplocal_liberar(b);if(!ok)return NULL;
  char *out=malloc(tamanho+128);if(!out){free(corpo);return NULL;}
  int cab=snprintf(out,128,"NUVIO-RESTORE-JOURNAL 1 %c\n",estado);
  if(cab<0 || cab>=128){free(corpo);free(out);return NULL;}
  memcpy(out+cab,corpo,tamanho);free(corpo);
  size_t total=(size_t)cab+tamanho;
  unsigned long crc=(unsigned long)crc32(0L,(const Bytef *)out,(uInt)total);
  int k=snprintf(out+total,128-(size_t)cab,"crc32 %08lx\n",crc);
  if(k!=15){free(out);return NULL;}*n=total+15;return out;
}
static int antigoIgual(const BackupPlano *p,const BackupIO *io,int tipo) {
  char *s=NULL;size_t n=0;int estado=io->ler(io->ctx,p->dono,p->perfil,tipo,&s,&n);
  int ok=estado==p->parte[tipo].existia;
  if(estado==0)ok=ok && !s && !n;
  else if(estado==1) {
    if(tipo==BACKUP_LOCAIS) {
      BackupStores *a=NULL,*b=NULL;
      ok=ok && s && backupstores_ler(s,n,&a) && backupstores_ler(p->parte[tipo].antes,p->parte[tipo].nAntes,&b) && backupstores_iguais(a,b);
      backupstores_liberar(a);backupstores_liberar(b);
    } else if(tipo==BACKUP_FONTES_IMPORTADAS)ok=ok && s && backupfonts_iguais(s,p->parte[tipo].antes);
    else ok=ok && s && n==p->parte[tipo].nAntes && (!n || !memcmp(s,p->parte[tipo].antes,n));
  }
  else ok=0;
  backupcrypto_limpar(s,n);free(s);return ok;
}
static int desfazer(const BackupPlano *p,const BackupIO *io) {
  // Rewrite PREPARED durably first: a failed commit-marker write may have
  // replaced the marker even though its final durability barrier failed.
  size_t n=0;char *s=diario(p,'P',&n);if(!s)return BACKUP_RESTORE_RECUPERAR;
  int ok=gravarDiario(io,s,n);free(s);if(!ok)return BACKUP_RESTORE_RECUPERAR;
  for(int i=0;i<BACKUP_PARTES_N;i++)if(p->parte[i].usada) {
    ok=p->parte[i].existia ? io->gravar(io->ctx,p->dono,p->perfil,i,p->parte[i].antes,p->parte[i].nAntes)
                          : io->apagar(io->ctx,p->dono,p->perfil,i);
    if(!ok)return BACKUP_RESTORE_RECUPERAR;
  }
  return limpar(io)?BACKUP_RESTORE_FALHOU:BACKUP_RESTORE_RECUPERAR;
}
int backuprestauro_aplicar(const BackupPlano *p,const BackupIO *io) {
  if(!ioValido(io) || !io->atual || !planoValido(p))return BACKUP_RESTORE_INVALIDO;
  if(!io->atual(io->ctx,p->dono,p->perfil,p->sessao))return BACKUP_RESTORE_CONTEXTO;
  char *pendente=NULL;size_t tamanho=0;int existe=io->ler(io->ctx,NULL,0,BACKUP_IO_DIARIO,&pendente,&tamanho);
  int temTexto=pendente!=NULL;free(pendente);if(existe!=0 || tamanho || temTexto)return BACKUP_RESTORE_RECUPERAR;
  for(int i=0;i<BACKUP_PARTES_N;i++)if(p->parte[i].usada && !antigoIgual(p,io,i))return BACKUP_RESTORE_CONTEXTO;
  size_t np=0,nc=0;char *preparado=diario(p,'P',&np),*confirmado=diario(p,'C',&nc);
  if(!preparado || !confirmado){free(preparado);free(confirmado);return BACKUP_RESTORE_INVALIDO;}
  if(!gravarDiario(io,preparado,np)){free(preparado);free(confirmado);return BACKUP_RESTORE_RECUPERAR;}
  free(preparado);int ok=1;
  for(int i=0;i<BACKUP_PARTES_N && ok;i++)if(p->parte[i].usada) {
    ok=io->atual(io->ctx,p->dono,p->perfil,p->sessao) &&
       io->gravar(io->ctx,p->dono,p->perfil,i,p->parte[i].depois,p->parte[i].nDepois);
  }
  if(ok)ok=io->atual(io->ctx,p->dono,p->perfil,p->sessao) && gravarDiario(io,confirmado,nc);
  free(confirmado);if(!ok)return desfazer(p,io);
  // COMMITTED is durable. A leftover committed journal must never undo success.
  return limpar(io)?BACKUP_RESTORE_OK:BACKUP_RESTORE_LIMPEZA;
}
static int crcValido(const char *s,size_t n) {
  if(n<15 || memcmp(s+n-15,"crc32 ",6) || s[n-1]!='\n')return 0;
  unsigned long v=0;
  for(size_t i=n-9;i<n-1;i++) {
    unsigned char c=(unsigned char)s[i];unsigned d;
    if(c>='0' && c<='9')d=c-'0';else if(c>='a' && c<='f')d=c-'a'+10;else return 0;
    v=(v<<4)|d;
  }
  return crc32(0L,(const Bytef *)s,(uInt)(n-15))==v;
}
static BackupPlano *lerDiario(const char *s,size_t n,int *confirmado) {
  if(n>BJ_MAX || n<15 || memchr(s,0,n) || !crcValido(s,n))return NULL;
  const char *fim=memchr(s,'\n',n-15);if(!fim || (size_t)(fim-s)>63)return NULL;
  char cab[64];size_t k=(size_t)(fim-s);memcpy(cab,s,k);cab[k]=0;
  if(!strcmp(cab,"NUVIO-RESTORE-JOURNAL 1 C"))*confirmado=1;
  else if(!strcmp(cab,"NUVIO-RESTORE-JOURNAL 1 P"))*confirmado=0;
  else return NULL;
  const char *corpo=fim+1;size_t nc=n-15-(size_t)(corpo-s);char dono[81];int perfil;
  if(!backuplocal_identidade(corpo,nc,dono,sizeof dono,&perfil))return NULL;
  BackupLocal *b=NULL;if(!backuplocal_ler(corpo,nc,dono,perfil,&b))return NULL;
  BackupPlano *p=calloc(1,sizeof *p);if(!p){backuplocal_liberar(b);return NULL;}
  memcpy(p->dono,dono,strlen(dono)+1);p->perfil=perfil;p->criado=backuplocal_criado(b);
  for(int i=0;i<BACKUP_PARTES_N;i++) {
    size_t tam=0;const char *v=backuplocal_parte(b,i,&tam);if(!v)continue;
    if(tam<2 || v[1]!='\n' || (v[0]!='0' && v[0]!='1') || (v[0]=='0' && tam!=2))goto falha;
    p->parte[i].usada=1;p->parte[i].existia=v[0]=='1';p->parte[i].nAntes=tam-2;
    if(p->parte[i].existia) {
      char *antigo=malloc(tam-1);if(!antigo)goto falha;
      memcpy(antigo,v+2,tam-2);antigo[tam-2]=0;p->parte[i].antes=antigo;
    }
  }
  backuplocal_liberar(b);return p;
falha:
  backuplocal_liberar(b);backupplano_liberar(p);return NULL;
}
int backuprestauro_recuperar(const BackupIO *io) {
  if(!ioValido(io))return BACKUP_RESTORE_INVALIDO;
  char *s=NULL;size_t n=0;int existe=io->ler(io->ctx,NULL,0,BACKUP_IO_DIARIO,&s,&n);
  if(existe==0 && !s && !n)return BACKUP_RESTORE_NADA;
  if(existe!=1 || !s){free(s);return BACKUP_RESTORE_RECUPERAR;}
  int confirmado=0;BackupPlano *p=lerDiario(s,n,&confirmado);free(s);
  if(!p)return BACKUP_RESTORE_RECUPERAR;
  int resultado;
  if(confirmado)resultado=limpar(io)?BACKUP_RESTORE_OK:BACKUP_RESTORE_LIMPEZA;
  else {resultado=desfazer(p,io);if(resultado==BACKUP_RESTORE_FALHOU)resultado=BACKUP_RESTORE_OK;}
  backupplano_liberar(p);return resultado;
}
