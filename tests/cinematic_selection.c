#include <assert.h>
#include "nvtmp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "colecoes.h"

static void put(const char *root,const char *name,const char *data) {
  char path[700];snprintf(path,sizeof path,"%s/%s",root,name);
  FILE *f=fopen(path,"w");assert(f);fputs(data,f);fclose(f);
}
int main(void) {
  char root[NVT_MAX]; nvt_set(root, sizeof root, "/nuvio-cinematic-test-XXXXXX");assert(mkdtemp(root));
  char path[700];snprintf(path,sizeof path,"%s/cinematic",root);assert(!mkdir(path,0700));
  snprintf(path,sizeof path,"%s/editorial",root);assert(!mkdir(path,0700));
  put(root,"collections.json","{\"groups\":[{\"title\":\"Directors\",\"folders\":[{\"id\":\"example\",\"title\":\"Example\",\"hero\":\"original.png\",\"sources\":[{\"base\":\"https://example.invalid\",\"type\":\"movie\",\"catId\":\"example\"}]}]}]}");
  assert(col_carregar(root)==1);assert(col_folder(0)->editorial==0);
  put(root,"editorial/example-home.png","");put(root,"editorial/example-detail.png","");
  assert(col_carregar(root)==1);assert(col_folder(0)->editorial==1);
  put(root,"cinematic/example-home.png","");
  assert(col_carregar(root)==1);assert(col_folder(0)->editorial==1);
  put(root,"cinematic/example-detail.png","");
  assert(col_carregar(root)==1);const ColFolder *f=col_folder(0);
  assert(f->editorial==2);assert(strstr(f->hero,"/cinematic/example-home.png"));
  assert(strstr(f->detailHero,"/cinematic/example-detail.png"));
  const char *files[]={"collections.json","editorial/example-home.png","editorial/example-detail.png","cinematic/example-home.png","cinematic/example-detail.png"};
  for(unsigned i=0;i<sizeof files/sizeof files[0];i++){snprintf(path,sizeof path,"%s/%s",root,files[i]);assert(!unlink(path));}
  snprintf(path,sizeof path,"%s/editorial",root);assert(!rmdir(path));
  snprintf(path,sizeof path,"%s/cinematic",root);assert(!rmdir(path));assert(!rmdir(root));
  puts("cinematic selection: PASS (complete pairs override; incomplete pair preserves fallback)");
}
