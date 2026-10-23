#include "backupconnections.h"
#include "backupcategories.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void put(BackupStores *b,int t,unsigned id,const char *s){assert(backupstores_definir(b,t,id,1,s,strlen(s)));}
static void eq(const BackupStores *b,int t,unsigned id,const char *s){BackupStore v;assert(backupstores_buscar(b,t,id,&v) && v.existe && !strcmp(v.texto,s));}
int main(void) {
  BackupStores *a=backupstores_novo(),*b=backupstores_novo(),*out=NULL,*again=NULL,*portable=NULL;
  assert(a && b);
  put(a,BS_IPTV_REGISTRY,0,"next\t2\nselected\t0\naccount\t1\t2\t1\tExisting M3U\n");
  put(a,BS_M3U,1,"lista\thttps://existing.invalid/private\nepg\thttps://existing.invalid/epg\n");
  put(a,BS_M3U,2,"lista\thttps://orphan.invalid/keep-but-dont-export\n");
  put(a,BS_SOURCE_ORDER,0,"regex account-managed\naddon official\nordem Old\n");
  put(a,BS_JELLYFIN,0,"host\thttps://existing-server.invalid\nkey\texisting-key\n");
  put(b,BS_IPTV_REGISTRY,0,"next\t3\nselected\t0\naccount\t1\t2\t1\tImported M3U\naccount\t2\t1\t0\tImported Xtream\n");
  put(b,BS_M3U,1,"lista\thttps://new.invalid/private\nepg\thttps://new.invalid/epg\n");
  put(b,BS_XTREAM,2,"servidor\thttps://new-xtream.invalid\nusuario\tnew-user\nsenha\tnew-password\n");
  put(b,BS_GUIDE_FAVORITES,0,"nvm3u:@00000001:channel\nxtream:@00000002:42\n");
  put(b,BS_SOURCE_ORDER,0,"ordem New\n");
  put(b,BS_JELLYFIN,0,"host\thttps://other-server.invalid\nkey\tother-key\n");
  put(b,BS_LIBRARY_UI,0,"layout 2\n");
  char category[32];snprintf(category,sizeof category,"%016llx\n",(unsigned long long)backupcategorias_chave(2,1,"","Sports"));
  put(b,BS_GUIDE_CATEGORIES,0,category);
  assert(backupconnections_mesclar(a,b,&out));
  BackupCategorias *badCats=NULL;
  assert(!backupcategorias_ler("group\nmap 2 4294967297 3\nhash 0000000000000001\n",&badCats));
  assert(!backupcategorias_ler("group\nmap 2 -2 3\nhash 0000000000000001\n",&badCats));
  BackupStore aliases;assert(backupstores_buscar(out,BS_GUIDE_CATEGORY_ALIASES,0,&aliases));
  BackupCategorias *categorias=NULL;assert(backupcategorias_ler(aliases.texto,&categorias));
  assert(backupcategorias_oculta(categorias,2,3,"","Sports")==1);
  assert(!backupcategorias_oculta(categorias,2,1,"","Sports"));
  assert(!backupcategorias_oculta(categorias,2,3,"","Movies"));
  backupcategorias_liberar(categorias);

  eq(out,BS_M3U,1,"lista\thttps://existing.invalid/private\nepg\thttps://existing.invalid/epg\n");
  eq(out,BS_M3U,2,"lista\thttps://orphan.invalid/keep-but-dont-export\n");
  eq(out,BS_M3U,3,"lista\thttps://new.invalid/private\nepg\thttps://new.invalid/epg\n");
  eq(out,BS_GUIDE_FAVORITES,0,"nvm3u:@00000003:channel\nxtream:@00000002:42\n");
  eq(out,BS_SOURCE_ORDER,0,"regex account-managed\naddon official\nordem Old\n");
  eq(out,BS_JELLYFIN,0,"host\thttps://existing-server.invalid\nkey\texisting-key\n");
  eq(out,BS_LIBRARY_UI,0,"layout 2\n");
  BackupStore reg;assert(backupstores_buscar(out,BS_IPTV_REGISTRY,0,&reg));
  assert(strstr(reg.texto,"account\t1\t2\t1\tExisting M3U\n") && strstr(reg.texto,"account\t3\t2\t1\tImported M3U\n"));
  assert(backupconnections_mesclar(out,b,&again));
  assert(backupstores_iguais(out,again)); /* repeated restore never adds duplicates */
  assert(backupconnections_capturar(out,&portable));
  BackupStore unused;assert(!backupstores_buscar(portable,BS_M3U,2,&unused));
  assert(!backupstores_buscar(portable,BS_SOURCE_ORDER,0,&reg));
  put(a,BS_HOME_ROWS,0,"limite 7\nordem 0\n");
  put(b,BS_HOME_ROWS,0,"limite 200\nordem 1\n");
  put(b,BS_EXTERNAL_PLAYER,0,"device-player");
  BackupStores *selected=NULL,*hardwareMerged=NULL;
  assert(backupconnections_selecionar(b,0,&selected));
  assert(!backupstores_buscar(selected,BS_EXTERNAL_PLAYER,0,&unused));
  eq(selected,BS_HOME_ROWS,0,"ordem 1\n");
  assert(backupconnections_mesclar(a,selected,&hardwareMerged));
  eq(hardwareMerged,BS_HOME_ROWS,0,"ordem 0\nlimite 7\n");
  backupstores_liberar(selected);backupstores_liberar(hardwareMerged);
  // Current upstream URL accounts: append missing connections, preserve
  // names/credentials, avoid duplicates, and reject full/malformed imports.
  put(a,BS_LTV_URLS,0,"m3u\tExisting\thttps://existing.invalid/list\n");
  put(b,BS_LTV_URLS,0,"m3u\tRename ignored\thttps://existing.invalid/list\nxmltv\tGuide\thttps://guide.invalid/epg\n");
  BackupStores *urls=NULL,*twice=NULL;
  assert(backupconnections_mesclar(a,b,&urls));
  eq(urls,BS_LTV_URLS,0,"m3u\tExisting\thttps://existing.invalid/list\nxmltv\tGuide\thttps://guide.invalid/epg\n");
  assert(backupconnections_mesclar(urls,b,&twice) && backupstores_iguais(urls,twice));
  backupstores_liberar(urls);backupstores_liberar(twice);
  put(b,BS_LTV_URLS,0,"m3u\tBad\tfile:///private/file\n");
  assert(!backupconnections_capturar(b,&urls) && !urls);
  backupstores_remover(b,BS_LTV_URLS,0);
  put(b,BS_IPTV_REGISTRY,0,"next\t4294967296\nselected\t0\n");
  BackupStores *invalid=(void *)1;assert(!backupconnections_mesclar(a,b,&invalid) && !invalid);
  put(b,BS_IPTV_REGISTRY,0,"next\t2\nselected\t0\naccount\t-2\t2\t1\tBad signed id\n");
  assert(!backupconnections_mesclar(a,b,&invalid) && !invalid);
  put(b,BS_IPTV_REGISTRY,0,"next\t3\nselected\t0\naccount\t1\t2\t1\tMissing config\naccount\t19\t3\t1\tInvalid\n");
  BackupStores *bad=(void *)1;assert(!backupconnections_mesclar(a,b,&bad) && !bad);
  backupstores_liberar(a);backupstores_liberar(b);backupstores_liberar(out);backupstores_liberar(again);backupstores_liberar(portable);
  puts("PASS: preserve existing connections, collision/orphan-safe IPTV IDs, favourite remapping, source/cloud ownership, duplicate-free repeated restore and reject incomplete account bundles");
}
