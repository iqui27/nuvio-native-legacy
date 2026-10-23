#include "backupplano_interno.h"
#include "ajustes.h"
#include "fontefalha.h"
#include "backupstores.h"
#include "backupconnections.h"
#include "backupcrypto.h"
#include "backupfonts.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
void backupplano_liberar(BackupPlano *p) {
  if(!p)return;
  for(int i=0;i<BACKUP_PARTES_N;i++){backupcrypto_limpar(p->parte[i].antes,p->parte[i].nAntes);backupcrypto_limpar(p->parte[i].depois,p->parte[i].nDepois);free(p->parte[i].antes);free(p->parte[i].depois);}
  free(p);
}
static char *copiar(const char *s,size_t n) {char *p=malloc(n+1);if(p){memcpy(p,s,n);p[n]=0;}return p;}
static int validar(int tipo,const char *texto,size_t n,char **nativo) {
  if(tipo==BACKUP_FONTES_IMPORTADAS) {
    char *antes=NULL,*depois=NULL;
    int ok=backupfonts_plano("NUVIO-FONT-FILES 1\n",texto,&antes,&depois);
    free(antes);if(ok)*nativo=depois;return ok;
  }
  if(tipo==BACKUP_LOCAIS) {
    BackupStores *b=NULL,*vazio=backupstores_novo(),*validado=NULL;size_t len;
    int ok=vazio && backupstores_ler(texto,n,&b) && backupconnections_mesclar(vazio,b,&validado) && backupstores_exportar(validado,nativo,&len);
    backupstores_liberar(b);backupstores_liberar(vazio);backupstores_liberar(validado);return ok;
  }
  if(tipo==BACKUP_AJUSTES_APARELHO || tipo==BACKUP_AJUSTES_PERFIL || tipo==BACKUP_AJUSTES_HARDWARE)
    return ajustes_backup_mesclar(tipo==BACKUP_AJUSTES_HARDWARE?AJ_BACKUP_HARDWARE:tipo==BACKUP_AJUSTES_PERFIL,"",texto,nativo);
  return 0;
}
int backupplano_criar(const BackupLocal *backup,const char *dono,int perfil,uint64_t sessao,
                      BackupLerAtual ler,void *ctx,BackupPlano **saida) {
  if(saida)*saida=NULL;
  if(!saida || !ler || !backuplocal_compativel(backup,dono,perfil))return BACKUP_PLANO_CONTEXTO;
  if(!backuplocal_quantidade(backup))return BACKUP_PLANO_INVALIDO;
  BackupPlano *p=calloc(1,sizeof *p);if(!p)return BACKUP_PLANO_INVALIDO;
  snprintf(p->dono,sizeof p->dono,"%s",dono);p->perfil=perfil;p->sessao=sessao;p->criado=backuplocal_criado(backup);
  int erro=BACKUP_PLANO_INVALIDO;
  for(int i=0;i<BACKUP_PARTES_N;i++) {
    size_t n=0;const char *texto=backuplocal_parte(backup,i,&n);if(!texto)continue;
    // The old reserved row part is unsupported; row settings now belong to
    // the validated local-store bundle inside the encrypted transport.
    if(i==BACKUP_FILEIRAS){erro=BACKUP_PLANO_NAO_SUPORTADO;goto falha;}
    if(!validar(i,texto,n,&p->parte[i].depois)){printf("[backup] invalid restore section: type=%d\n",i);goto falha;}
    p->parte[i].nDepois=strlen(p->parte[i].depois);p->parte[i].usada=i!=BACKUP_AJUSTES_HARDWARE;
  }
  // Hardware and ordinary device settings share ajustes.txt. Merge both into
  // one journalled disk operation, never two writes based on the same old file.
  const char *hardware=backuplocal_parte(backup,BACKUP_AJUSTES_HARDWARE,NULL);
  if(hardware && !p->parte[BACKUP_AJUSTES_APARELHO].usada) {
    p->parte[BACKUP_AJUSTES_APARELHO].usada=1;
    p->parte[BACKUP_AJUSTES_APARELHO].depois=copiar("",0);
    if(!p->parte[BACKUP_AJUSTES_APARELHO].depois)goto falha;
  }
  // Upstream profile files now also hold some optional display choices. Keep
  // their selected hardware values in the same journal as the device file.
  if(hardware && !p->parte[BACKUP_AJUSTES_PERFIL].usada) {
    p->parte[BACKUP_AJUSTES_PERFIL].usada=1;
    p->parte[BACKUP_AJUSTES_PERFIL].depois=copiar("",0);
    if(!p->parte[BACKUP_AJUSTES_PERFIL].depois)goto falha;
  }
  // Every payload is valid before reading the selected current stores.
  for(int i=0;i<BACKUP_PARTES_N;i++)if(p->parte[i].usada) {
    char *atual=NULL;size_t n=0;int estado=ler(ctx,i,&atual,&n);
    if(estado<0 || estado>1 || (estado==0 && (atual || n)) ||
       (estado==1 && (!atual || n>BACKUP_PARTE_MAX-2u || memchr(atual,0,n)))) {
      printf("[backup] current store read failed: type=%d status=%d\n",i,estado);
      free(atual);erro=BACKUP_PLANO_LEITURA;goto falha;
    }
    p->parte[i].existia=estado;p->parte[i].antes=atual;p->parte[i].nAntes=n;
    if(i==BACKUP_FONTES_IMPORTADAS) {
      char *antes=NULL,*depois=NULL;size_t ns=0;const char *entrada=backuplocal_parte(backup,i,&ns);
      if(!backupfonts_plano(atual?atual:"NUVIO-FONT-FILES 1\n",entrada,&antes,&depois)){free(antes);free(depois);goto falha;}
      free(p->parte[i].antes);free(p->parte[i].depois);
      p->parte[i].antes=antes;p->parte[i].nAntes=strlen(antes);p->parte[i].existia=1;
      p->parte[i].depois=depois;p->parte[i].nDepois=strlen(depois);
    }
    if(i==BACKUP_LOCAIS) {
      BackupStores *antigo=NULL,*entrada=NULL,*junto=NULL;
      size_t ns=0;const char *s=backuplocal_parte(backup,i,&ns);
      int ok=(atual?backupstores_ler(atual,n,&antigo):!!(antigo=backupstores_novo())) &&
        backupstores_ler(s,ns,&entrada) && backupconnections_mesclar(antigo,entrada,&junto);
      // Journal every new path as explicitly absent before the first write,
      // so rollback can remove new account files after a partial bundle write.
      for(int j=0;ok && j<backupstores_n(junto);j++) {
        BackupStore item,velho;backupstores_item(junto,j,&item);
        if(!backupstores_buscar(antigo,item.tipo,item.sub,&velho))
          ok=backupstores_adicionar(antigo,item.tipo,item.sub,0,NULL,0);
      }
      char *antes=NULL,*depois=NULL;size_t na=0,nd=0;
      if(ok)ok=backupstores_exportar(antigo,&antes,&na) && backupstores_exportar(junto,&depois,&nd);
      backupstores_liberar(antigo);backupstores_liberar(entrada);backupstores_liberar(junto);
      if(!ok){printf("[backup] local store merge failed\n");free(antes);free(depois);goto falha;}
      backupcrypto_limpar(p->parte[i].antes,p->parte[i].nAntes);free(p->parte[i].antes);free(p->parte[i].depois);
      p->parte[i].antes=antes;p->parte[i].nAntes=na;p->parte[i].existia=1;
      p->parte[i].depois=depois;p->parte[i].nDepois=nd;
    }
    if(i==BACKUP_AJUSTES_APARELHO || i==BACKUP_AJUSTES_PERFIL) {
      char *entrada=atual?copiar(atual,n):copiar("",0),*mesclado=NULL;
      size_t tamanho=0;const char *snapshot=backuplocal_parte(backup,i,&tamanho);(void)tamanho;
      if(!snapshot)snapshot=i==BACKUP_AJUSTES_PERFIL?
        "NUVIO-LOCAL-SETTINGS 1\nscope profile\n":"NUVIO-LOCAL-SETTINGS 1\nscope device\n";
      if(!entrada || !ajustes_backup_mesclar(i==BACKUP_AJUSTES_PERFIL,entrada,snapshot,&mesclado)) {
        free(entrada);goto falha;
      }
      free(entrada);
      if(i==BACKUP_AJUSTES_APARELHO && hardware) {
        char *junto=NULL;
        if(!ajustes_backup_mesclar(AJ_BACKUP_HARDWARE,mesclado,hardware,&junto)){free(mesclado);goto falha;}
        free(mesclado);mesclado=junto;
      }
      if(i==BACKUP_AJUSTES_PERFIL && hardware) {
        char *junto=NULL;
        if(!ajustes_backup_hardware_perfil(mesclado,hardware,&junto)){free(mesclado);goto falha;}
        free(mesclado);mesclado=junto;
      }
      free(p->parte[i].depois);p->parte[i].depois=mesclado;p->parte[i].nDepois=strlen(mesclado);
    }
  }
  *saida=p;return BACKUP_PLANO_OK;
falha:
  backupplano_liberar(p);return erro;
}
