#include "aiostreams.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
  AioSnapshot *s=calloc(1,sizeof *s);
  char *json=malloc(200000); assert(s && json);
  for(int count=248;count<=752;count+=252) {
    size_t used=sprintf(json,"{\"id\":\"aiostreams\",\"catalogs\":[");
    for(int i=0;i<count;i++) used+=sprintf(json+used,"%s{\"id\":\"cat%d\",\"type\":\"movie\",\"name\":\"Catalog %d\"}",i?",":"",i,i);
    strcpy(json+used,"]}");
    assert(aio_manifesto(s,json));
    int stored=count<AIO_CATALOG_MAX?count:AIO_CATALOG_MAX;
    assert(s->catalogTotal==count && s->catalogos==stored && s->catalogOmitted==count-stored);
    char last[32]; sprintf(last,"cat%d",stored-1);
    assert(!strcmp(s->catalogo[stored-1].id,last));
  }
  free(json); free(s);
  puts("AIO catalog inventory: 248 and 500, last IDs reachable PASS");
}
