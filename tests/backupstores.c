#include "backupstores.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void recusar(const char *s) {
  BackupStores *out=(void *)1;
  assert(!backupstores_ler(s,strlen(s),&out) && !out);
}
int main(void) {
  BackupStores *a=backupstores_novo(),*b=backupstores_novo(),*decoded=NULL;
  assert(a && b);
  assert(backupstores_adicionar(a,BS_M3U,41,1,"lista\thttps://fixture.invalid/private\n",38));
  assert(backupstores_adicionar(a,BS_HOME_ROWS,0,1,"",0));
  assert(backupstores_adicionar(a,BS_XTREAM,42,0,NULL,0));
  assert(!backupstores_adicionar(a,BS_M3U,41,1,"duplicate",9));
  assert(!backupstores_adicionar(a,BS_M3U,UINT32_MAX,1,"",0));
  assert(!backupstores_adicionar(a,BS_BADGES_CUSTOM,3,1,"",0));
  assert(!backupstores_adicionar(a,BS_STATS_TILES,3,1,"",0));
  assert(!backupstores_adicionar(a,BS_HOME_ROWS,1,1,"",0));
  assert(!backupstores_adicionar(a,BS_HOME_ROWS,0,2,"",0));
  assert(!backupstores_adicionar(a,BS_STALKER,2,0,"x",1));
  assert(!backupstores_adicionar(a,BS_STALKER,2,1,"\0",1));
  assert(!backupstores_adicionar(a,BS_STALKER,2,1,"x",BACKUP_STORE_BYTES+1u));
  char *text=NULL;size_t n;
  assert(backupstores_exportar(a,&text,&n));
  assert(backupstores_ler(text,n,&decoded) && backupstores_iguais(a,decoded));
  assert(backupstores_adicionar(b,BS_HOME_ROWS,0,1,"",0));
  BackupStore item;assert(backupstores_buscar(a,BS_M3U,41,&item));
  assert(backupstores_adicionar(b,item.tipo,item.sub,item.existe,item.texto,item.n));
  assert(backupstores_iguais(a,b)); /* omitted vs explicit tombstone */
  assert(backupstores_adicionar(b,BS_XTREAM,42,1,"",0));
  assert(!backupstores_iguais(a,b)); /* existing empty is not absent */
  char path[160];
  assert(backupstores_nome(BS_DEBRID_PROFILE,0,"user-1",7,path,sizeof path));
  assert(!strcmp(path,"debrid-user-1-p7.txt"));
  assert(backupstores_nome(BS_M3U,41,"user-1",7,path,sizeof path));
  assert(!strcmp(path,"m3u-p7-s41.txt"));
  assert(backupstores_nome(BS_M3U,0,"user-1",8,path,sizeof path));
  assert(!strcmp(path,"m3u-p8.txt"));
  assert(!backupstores_nome(BS_M3U,41,"../escape",7,path,sizeof path) && !*path);
  assert(!backupstores_nome(BS_M3U,41,"user-1",0,path,sizeof path));
  assert(!backupstores_nome(BS_M3U,41,"user-1",7,path,2) && !*path);
  assert(!backupstores_nome(-1,0,"user-1",7,path,sizeof path));
  /* Login files cannot enter the typed bundle through native capture, nor be
   * injected by a downloaded backup. Preferences use their own scalar keys. */
  const char *logins[]={"trakt-p7.txt","discord-p7.txt","trakt-fluxo-p7.txt",
    "sessao.txt","google-drive-user-1.txt","simkl-p7.txt"};
  for(size_t i=0;i<sizeof logins/sizeof *logins;i++) {
    int kind=-1;uint32_t sub=0;
    assert(!backupstores_reconhecer(logins[i],"user-1",7,&kind,&sub));
  }
  recusar("NUVIO-LOCAL-STORES 1\ncount 1\nfile trakt 0 1 6\nsecret\n");
  recusar("NUVIO-LOCAL-STORES 1\ncount 1\nfile discord 0 1 6\nsecret\n");
  backupstores_liberar(decoded);decoded=NULL;
  assert(!backupstores_ler(text,n-1,&decoded));
  text[n-1]='x';assert(!backupstores_ler(text,n,&decoded));text[n-1]='\n';
  recusar("NUVIO-LOCAL-STORES 1\ncount 1\nfile ../../sessions.txt 0 1 1\nx\n");
  recusar("NUVIO-LOCAL-STORES 1\ncount 1\nfile m3u 4294967295 1 1\nx\n");
  recusar("NUVIO-LOCAL-STORES 1\ncount 1\nfile m3u 1 0 1\nx\n");
  recusar("NUVIO-LOCAL-STORES 1\ncount 1\nfile m3u 1 1 99999999999999999999\nx\n");
  recusar("NUVIO-LOCAL-STORES 1\ncount 2\nfile m3u 1 1 1\nx\nfile m3u 1 1 1\ny\n");
  recusar("NUVIO-LOCAL-STORES 1\ncount 1\nfile home-rows 0 1 0\n\ntrailing");
  recusar("NUVIO-LOCAL-STORES 1\ncount 257\n");
  free(text);backupstores_liberar(a);backupstores_liberar(b);backupstores_liberar(decoded);
  puts("PASS: bounded typed stores, exact framing, tombstones, duplicate/overflow/path rejection, authenticated owner/profile path mapping");
}
