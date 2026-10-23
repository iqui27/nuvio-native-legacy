#include "drivefontes.h"
#include "backupfonts.h"
#include "backupstores.h"
#include "backupcrypto.h"
#include <stdlib.h>
#include <string.h>
static int escolhas(const BackupLocal *b,const BackupFontManifest *m) {
  size_t n;const char *s=backuplocal_parte(b,BACKUP_LOCAIS,&n);if(!s)return 1;
  BackupStores *locais=NULL;if(!backupstores_ler(s,n,&locais))return 0;
  BackupStore v;int ok=1;
  if(backupstores_buscar(locais,BS_IMPORTED_FONT_CHOICES,0,&v) && v.existe) {
    int linhas=0;
    for(const char *p=v.texto;*p && ok;) {
      const char *e=strchr(p,'\n');if(!e)e=p+strlen(p);
      size_t k=(size_t)(e-p);if(k && p[k-1]=='\r')k--;
      if(++linhas>3 || k>20){ok=0;break;}
      if(k) {
        char id[21];memcpy(id,p,k);id[k]=0;int tem=0;
        for(int i=0;i<m->n;i++)if(!strcmp(m->font[i].id,id))tem=1;
        if(!tem)ok=0;
      }
      p=*e?e+1:e;
    }
  }
  backupstores_liberar(locais);return ok;
}
int drivefontes_enviar(const DriveLocal *c,const char *dir,const char *snapshot,size_t n,char **out,size_t *nt) {
  if(out)*out=NULL;
  if(nt)*nt=0;
  BackupLocal *b=NULL;
  if(!c || !out || !nt || !backuplocal_ler(snapshot,n,c->dono,c->perfil,&b))return DRIVE_LOCAL_INVALIDO;
  BackupFontManifest m;int r=DRIVE_LOCAL_INVALIDO;char *manifest=NULL;size_t nm=0;
  if(backuplocal_parte(b,BACKUP_FONTES_IMPORTADAS,NULL) || !backupfonts_enumerar(dir,&m) || !escolhas(b,&m))goto fim;
  for(int i=0;i<m.n;i++)for(int j=0;j<m.font[i].chunks;j++) {
    unsigned char *dados=NULL;size_t nd=0;
    if(!backupfonts_chunk_ler(dir,&m.font[i],j,&dados,&nd))goto fim;
    int enviou=drivelocal_asset_enviar(c,m.font[i].id,j,dados,nd,m.font[i].remoto[j]);free(dados);
    if(enviou!=DRIVE_LOCAL_OK){r=enviou;goto fim;}
  }
  if(!backupfonts_manifest_exportar(&m,&manifest,&nm) || !backuplocal_adicionar(b,BACKUP_FONTES_IMPORTADAS,manifest,nm) || !backuplocal_exportar(b,out,nt))goto fim;
  r=DRIVE_LOCAL_OK;
fim:free(manifest);backuplocal_liberar(b);return r;
}
int drivefontes_baixar(const DriveLocal *c,const char *dir,BackupLocal *b) {
  if(!c || !b || !backuplocal_compativel(b,c->dono,c->perfil))return DRIVE_LOCAL_INVALIDO;
  size_t n;const char *s=backuplocal_parte(b,BACKUP_FONTES_IMPORTADAS,&n);BackupFontManifest m={0};
  if(s && !backupfonts_manifest_ler(s,n,&m))return DRIVE_LOCAL_RESPOSTA;
  if(!escolhas(b,&m))return DRIVE_LOCAL_RESPOSTA;
  if(!m.n)return DRIVE_LOCAL_OK;
  BackupFontStage *stage=backupfonts_stage(dir);if(!stage)return DRIVE_LOCAL_INVALIDO;
  int r=DRIVE_LOCAL_OK;
  for(int i=0;i<m.n && r==DRIVE_LOCAL_OK;i++)for(int j=0;j<m.font[i].chunks;j++) {
    unsigned char *dados=NULL;size_t nd=0;
    r=drivelocal_asset_baixar(c,m.font[i].id,j,m.font[i].remoto[j],&dados,&nd);
    if(r==DRIVE_LOCAL_OK && !backupfonts_stage_chunk(stage,&m.font[i],j,dados,nd))r=DRIVE_LOCAL_RESPOSTA;
    free(dados);if(r!=DRIVE_LOCAL_OK)break;
  }
  if(r==DRIVE_LOCAL_OK && !backupfonts_stage_completo(stage,&m))r=DRIVE_LOCAL_RESPOSTA;
  if(r==DRIVE_LOCAL_OK)backuplocal_fontes_vincular(b,stage);
  backupfonts_soltar(stage);return r;
}
