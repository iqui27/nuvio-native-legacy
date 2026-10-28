#include "rede.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv){
  assert(argc==2);char url[160];int status=0;
  snprintf(url,sizeof url,"http://127.0.0.1:%d/player_api.php?action=get_vod_streams",atoi(argv[1]));
  char *body=rede_baixar_st(url,15,NULL,&status);
  assert(body&&status==200&&strstr(body,"[]"));free(body);return 0;
}
