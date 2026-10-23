#define _POSIX_C_SOURCE 200809L
#include "backupdisco.h"
#include "backupplano_interno.h"
#include "backupcrypto.h"
#include "dados.h"
#include <stdlib.h>
#include <string.h>
static int atual(void *ctx,const char *d,int p,uint64_t s) {
  (void)ctx;return !strcmp(d,"user-1") && p==7 && s==33;
}
int backup_wasm_seed(void) {
  return dados_gravar("player.txt","old-player") && dados_backup_sincronizar();
}
int backup_wasm_restore(void) {
  BackupDisco *d=backupdisco_abrir(dados_dir(),atual,NULL);if(!d)return -99;
  BackupPlano p={.perfil=7,.sessao=33,.criado=1792330000};strcpy(p.dono,"user-1");
  p.parte[BACKUP_PLAYER].usada=1;p.parte[BACKUP_PLAYER].existia=1;
  p.parte[BACKUP_PLAYER].antes="old-player";p.parte[BACKUP_PLAYER].nAntes=10;
  p.parte[BACKUP_PLAYER].depois="new-player";p.parte[BACKUP_PLAYER].nDepois=10;
  BackupIO io=backupdisco_io(d);int r=backuprestauro_aplicar(&p,&io);backupdisco_fechar(d);return r;
}
int backup_wasm_recover(void) {return backupdisco_recuperar_arranque(dados_dir());}
int backup_wasm_crypto(void) {
  const char *plain="portable-settings",*password="fixture-password";unsigned char *sealed=NULL,*opened=NULL;size_t ns=0,no=0;
  int ok=backupcrypto_selar(plain,strlen(plain),password,strlen(password),"user-1:p7:settings",&sealed,&ns) &&
    backupcrypto_abrir(sealed,ns,password,strlen(password),"user-1:p7:settings",&opened,&no) &&
    no==strlen(plain) && !memcmp(opened,plain,no);
  backupcrypto_limpar(sealed,ns);free(sealed);backupcrypto_limpar(opened,no);free(opened);return ok;
}
