#include "androidcache.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
static void file(const char *p, long bytes, time_t age) {
  FILE *f = fopen(p, "wb"); assert(f);
  assert(!ftruncate(fileno(f), bytes)); assert(!fclose(f));
  struct utimbuf t = {age, age}; assert(!utime(p, &t));
}
static void durable(const char *data) {
  const char *keep[]={"ltv-urls.txt","ltv-urls-p2.txt","ltv-fontes.txt",
    "ltv-fontes-p2.txt","ltv-fav.txt","ltv-fav-p2.txt","ltv-epgmapa.txt",
    "ltv-epgmapa-p2.txt","profiles.txt","accounts.txt","progress.txt",
    "addons.txt","plugins-p1.txt","epg-mappings.txt","plugins-config.txt"};
  char p[1200];
  for (unsigned i=0;i<sizeof keep/sizeof *keep;i++) {
    snprintf(p,sizeof p,"%s/%s",data,keep[i]); file(p,123,1);
  }
  nv_android_cache_migrate(data);
  assert(nv_android_cache_clear());
  for (unsigned i=0;i<sizeof keep/sizeof *keep;i++) {
    struct stat st;
    snprintf(p,sizeof p,"%s/%s",data,keep[i]);
    assert(!stat(p,&st) && st.st_size==123);
    assert(!nv_android_cache_name(keep[i]));
  }
  puts("Durable storage survives migration and clear PASS");
}
static void apk(const char *root) {
  char p[1200], other[1200]; struct stat st;
  snprintf(p,sizeof p,"%s/atualizacao",root); assert(!mkdir(p,0700));
  snprintf(p,sizeof p,"%s/atualizacao/Nuvio-2.0.7.apk",root);
  file(p,86975102,1); /* fclose completes the download before installation. */
  nv_android_cache_trim(NV_ANDROID_CACHE_BYTES);
  assert(!stat(p,&st) && st.st_size==86975102);
  snprintf(other,sizeof other,"%s/other.tmp",root); file(other,90L<<20,2);
  nv_android_cache_trim(NV_ANDROID_CACHE_BYTES);
  assert(!stat(p,&st) && st.st_size==86975102);
  assert(access(other,F_OK));
  FILE *f=fopen(p,"rb"); assert(f); assert(!fclose(f)); /* Installer can open it. */
  assert(nv_android_cache_clear()); assert(access(p,F_OK));
  puts("APK larger than cache cap survives until install PASS");
}
int main(int argc, char **argv) {
  (void)argc;
  char root[1024], data[1024], p[1200], old[1200], hot[1200];
  snprintf(root,sizeof root,"%s/cache",argv[1]); mkdir(root,0700);
  snprintf(data,sizeof data,"%s/data",argv[1]); mkdir(data,0700);
  setenv("NUVIO_CACHE",root,1);
  if (argc>2) {
    if (!strcmp(argv[2],"durable")) durable(data);
    else apk(root);
    return 0;
  }
  const char *keep[]={"profiles.txt","accounts.txt","progress.txt","plugins-p1.txt","addons.txt","ajustes.txt"};
  for(unsigned i=0;i<sizeof keep/sizeof *keep;i++) { snprintf(p,sizeof p,"%s/%s",data,keep[i]); file(p,123,1); }
  snprintf(old,sizeof old,"%s/epg-old",root); file(old,70L<<20,1);
  snprintf(hot,sizeof hot,"%s/plugins-c-hot",root); file(hot,60L<<20,2);
  nv_android_cache_touch(old);
  assert(nv_android_cache_size() > (100L<<20));
  nv_android_cache_trim(100L<<20);
  assert(nv_android_cache_size() <= (100L<<20));
  assert(!access(old,F_OK)); assert(access(hot,F_OK));
  snprintf(p,sizeof p,"%s/trailer",root); mkdir(p,0700);
  snprintf(p,sizeof p,"%s/trailer/video",root); file(p,90L<<20,3);
  nv_android_cache_trim(NV_ANDROID_CACHE_BYTES);
  assert(nv_android_cache_size() <= NV_ANDROID_CACHE_BYTES);
  snprintf(p,sizeof p,"%s/outside",data); file(p,17,1);
  snprintf(hot,sizeof hot,"%s/link",root); assert(!symlink(data,hot));
  assert(nv_android_cache_clear()); assert(nv_android_cache_size()==0);
  assert(!access(p,F_OK));
  for(unsigned i=0;i<sizeof keep/sizeof *keep;i++) { snprintf(p,sizeof p,"%s/%s",data,keep[i]); struct stat st; assert(!stat(p,&st)&&st.st_size==123); }
  assert(nv_android_cache_name("plugins-c-123.txt"));
  assert(nv_android_cache_name("tendencia/123.txt"));
  assert(nv_android_cache_name("atualizacao"));
  assert(nv_android_cache_name("atualizacao/Nuvio-2.0.7.apk"));
  assert(!nv_android_cache_name("atualizacao/../accounts.txt"));
  assert(!nv_android_cache_name("atualizacao-other/Nuvio.apk"));
  snprintf(p,sizeof p,"%s/atualizacao",root); assert(!mkdir(p,0700));
  snprintf(p,sizeof p,"%s/atualizacao/Nuvio-2.0.7.apk",root); file(p,123,1);
  nv_android_cache_migrate(data); assert(!access(p,F_OK));
  assert(!nv_android_cache_name("plugins-p1.txt"));
  assert(!nv_android_cache_name("guia-favoritos-p1.txt"));
  assert(!nv_android_cache_name("../accounts.txt"));
  snprintf(p,sizeof p,"%s/plugins-c-old.txt",data); file(p,22,1);
  nv_android_cache_migrate(data); assert(access(p,F_OK));
  snprintf(p,sizeof p,"%s/profiles.txt",data); assert(!access(p,F_OK));
  snprintf(p,sizeof p,"%s/abandoned.tmp",root); file(p,90L<<20,1);
  nv_android_cache_trim(NV_ANDROID_CACHE_BYTES);
  assert(nv_android_cache_size() <= NV_ANDROID_CACHE_BYTES);
  puts("Android storage: LRU budget, recursive clear, symlinks and user state PASS");
}
