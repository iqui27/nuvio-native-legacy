// Transaction/failure unit test. Payload validation is separately exercised by
// backupplano.sh with the real module owners; these are synthetic file contents.
#define _POSIX_C_SOURCE 200809L
#include "backuprestauro.h"
#include "backupplano_interno.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#define ARQS (BACKUP_PARTES_N+1)
typedef struct {char *s[ARQS];} Estado;
typedef struct {
  Estado e,fotos[40];int nFotos,tracar,chamadas,falha1,falha2,depois1,depois2,mudarEm;
  uint64_t sessao;
} Mem;
static const char *const antigos[5]={"old-device","old-profile",NULL,"","old-policy"};
static const char *const novos[5]={"new-device","new-profile","new-player","new-shared","new-policy"};
static void soltar(Estado *e){for(int i=0;i<ARQS;i++){free(e->s[i]);e->s[i]=NULL;}}
static void copiar(Estado *dst,const Estado *src){for(int i=0;i<ARQS;i++)dst->s[i]=src->s[i]?strdup(src->s[i]):NULL;}
static void inicializar(Mem *m) {
  memset(m,0,sizeof *m);m->sessao=33;m->tracar=1;
  for(int i=0;i<5;i++)if(antigos[i])m->e.s[i]=strdup(antigos[i]);
}
static void encerrar(Mem *m){soltar(&m->e);for(int i=0;i<m->nFotos;i++)soltar(&m->fotos[i]);}
static void conferir(Mem *m,int novo) {
  for(int i=0;i<5;i++) {
    const char *s=novo?novos[i]:antigos[i];
    assert((s!=NULL)==(m->e.s[i]!=NULL));if(s)assert(!strcmp(s,m->e.s[i]));
  }
}
static int ler(void *ctx,const char *dono,int perfil,int tipo,char **out,size_t *n) {
  Mem *m=ctx;assert(tipo>=0 && tipo<ARQS);
  if(tipo==BACKUP_IO_DIARIO)assert(!dono && !perfil);else assert(!strcmp(dono,"user-1") && perfil==7);
  *out=m->e.s[tipo]?strdup(m->e.s[tipo]):NULL;*n=*out?strlen(*out):0;return *out?1:0;
}
static int falha(Mem *m,int depois) {
  return (m->chamadas==m->falha1 && depois==m->depois1) || (m->chamadas==m->falha2 && depois==m->depois2);
}
static int mudar(Mem *m,int tipo,const char *s,size_t n) {
  m->chamadas++;if(falha(m,0))return 0;
  char *c=NULL;if(s){c=malloc(n+1);assert(c);memcpy(c,s,n);c[n]=0;}
  free(m->e.s[tipo]);m->e.s[tipo]=c;
  if(m->chamadas==m->mudarEm)m->sessao++;
  if(m->tracar){assert(m->nFotos<40);copiar(&m->fotos[m->nFotos++],&m->e);}
  return !falha(m,1);
}
static int gravar(void *ctx,const char *dono,int perfil,int tipo,const char *s,size_t n) {
  if(tipo==BACKUP_IO_DIARIO)assert(!dono && !perfil);else assert(!strcmp(dono,"user-1") && perfil==7);
  return mudar(ctx,tipo,s,n);
}
static int apagar(void *ctx,const char *dono,int perfil,int tipo) {
  if(tipo==BACKUP_IO_DIARIO)assert(!dono && !perfil);else assert(!strcmp(dono,"user-1") && perfil==7);
  return mudar(ctx,tipo,NULL,0);
}
static int atual(void *ctx,const char *dono,int perfil,uint64_t sessao) {
  Mem *m=ctx;return !strcmp(dono,"user-1") && perfil==7 && m->sessao==sessao;
}
static BackupIO ioPara(Mem *m){return (BackupIO){m,ler,gravar,apagar,atual};}
static BackupPlano *plano(void) {
  BackupPlano *p=calloc(1,sizeof *p);assert(p);strcpy(p->dono,"user-1");p->perfil=7;p->sessao=33;p->criado=1792330000;
  for(int i=0;i<5;i++){
    p->parte[i].usada=1;p->parte[i].existia=antigos[i]!=NULL;
    p->parte[i].antes=antigos[i]?strdup(antigos[i]):NULL;p->parte[i].nAntes=antigos[i]?strlen(antigos[i]):0;
    p->parte[i].depois=strdup(novos[i]);p->parte[i].nDepois=strlen(novos[i]);
  }
  return p;
}
static void repararCrc(char *s,size_t n) {
  unsigned long c=(unsigned long)crc32(0L,(const Bytef *)s,(uInt)(n-15));assert(snprintf(s+n-15,16,"crc32 %08lx\n",c)==15);
}
int main(void) {
  BackupPlano *p=plano();Mem m;inicializar(&m);BackupIO io=ioPara(&m);
  assert(backuprestauro_aplicar(p,&io)==BACKUP_RESTORE_OK);conferir(&m,1);assert(!m.e.s[BACKUP_IO_DIARIO]);
  // Every durable boundary is a simulated process/power-loss restart.
  for(int i=0;i<m.nFotos;i++) {
    Mem r={0};copiar(&r.e,&m.fotos[i]);r.sessao=999;BackupIO ri=ioPara(&r);
    char *j=r.e.s[BACKUP_IO_DIARIO];int novo=!j || strstr(j,"JOURNAL 1 C\n")!=NULL;
    assert(backuprestauro_recuperar(&ri)>=0);conferir(&r,novo);assert(!r.e.s[BACKUP_IO_DIARIO]);
    assert(backuprestauro_recuperar(&ri)==BACKUP_RESTORE_NADA);encerrar(&r);
  }
  int operacoes=m.chamadas;assert(operacoes==8);
  // Corrupted or invalid private journals cannot trigger any store writes.
  Mem cor={0};copiar(&cor.e,&m.fotos[0]);cor.sessao=33;BackupIO ci=ioPara(&cor);
  char *j=cor.e.s[BACKUP_IO_DIARIO];size_t nj=strlen(j);j[nj/2]^=1;
  assert(backuprestauro_recuperar(&ci)==BACKUP_RESTORE_RECUPERAR && !cor.chamadas);encerrar(&cor);
  memset(&cor,0,sizeof cor);copiar(&cor.e,&m.fotos[0]);ci=ioPara(&cor);j=cor.e.s[BACKUP_IO_DIARIO];nj=strlen(j);
  char *v=strstr(j,"1\nold-device");assert(v);v[0]='9';
  char *corpo=strchr(j,'\n')+1;repararCrc(corpo,nj-15-(size_t)(corpo-j));repararCrc(j,nj);
  assert(backuprestauro_recuperar(&ci)==BACKUP_RESTORE_RECUPERAR && !cor.chamadas);encerrar(&cor);
  encerrar(&m);
  // Failure before AND after every atomic operation, including commit marker.
  for(int i=1;i<=operacoes;i++)for(int depois=0;depois<2;depois++) {
    inicializar(&m);m.falha1=i;m.depois1=depois;io=ioPara(&m);
    int resultado=backuprestauro_aplicar(p,&io);assert(resultado!=BACKUP_RESTORE_OK);
    int novo=resultado==BACKUP_RESTORE_LIMPEZA;m.falha1=0;
    assert(backuprestauro_recuperar(&io)>=0);conferir(&m,novo);assert(!m.e.s[BACKUP_IO_DIARIO]);encerrar(&m);
  }
  // A failed rollback keeps its journal; a later recovery restores every store.
  for(int segunda=4;segunda<=6;segunda++) {
    inicializar(&m);m.falha1=3;m.falha2=segunda;io=ioPara(&m);
    assert(backuprestauro_aplicar(p,&io)==BACKUP_RESTORE_RECUPERAR);assert(m.e.s[BACKUP_IO_DIARIO]);
    m.falha1=m.falha2=0;assert(backuprestauro_recuperar(&io)==BACKUP_RESTORE_OK);conferir(&m,0);encerrar(&m);
  }
  // Uncertain commit marker + failed re-prepare: durable COMMITTED retains the
  // complete new snapshot on restart instead of reporting a partial rollback.
  inicializar(&m);m.falha1=7;m.depois1=1;m.falha2=8;io=ioPara(&m);
  assert(backuprestauro_aplicar(p,&io)==BACKUP_RESTORE_RECUPERAR);m.falha1=m.falha2=0;
  assert(backuprestauro_recuperar(&io)==BACKUP_RESTORE_OK);conferir(&m,1);encerrar(&m);
  inicializar(&m);m.sessao=34;io=ioPara(&m);
  assert(backuprestauro_aplicar(p,&io)==BACKUP_RESTORE_CONTEXTO && !m.chamadas);conferir(&m,0);encerrar(&m);
  inicializar(&m);m.mudarEm=2;io=ioPara(&m);
  assert(backuprestauro_aplicar(p,&io)==BACKUP_RESTORE_FALHOU);conferir(&m,0);encerrar(&m);
  inicializar(&m);free(m.e.s[1]);m.e.s[1]=strdup("new user edit");io=ioPara(&m);
  assert(backuprestauro_aplicar(p,&io)==BACKUP_RESTORE_CONTEXTO && !m.chamadas && !strcmp(m.e.s[1],"new user edit"));encerrar(&m);
  backupplano_liberar(p);
  puts("PASS: journal commit/rollback, every durable crash boundary, before/after I/O failures, rollback retries, stale sessions and concurrent edits");
}
