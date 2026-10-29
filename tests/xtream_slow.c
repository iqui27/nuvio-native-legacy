#include "rede.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static struct timespec began;
static long elapsed_ms(void){struct timespec now;clock_gettime(CLOCK_MONOTONIC,&now);return (now.tv_sec-began.tv_sec)*1000+(now.tv_nsec-began.tv_nsec)/1000000;}
static int stop(void *unused){(void)unused;return elapsed_ms()>=100;}
int main(int argc,char **argv){
  assert(argc==2);char url[160];int status=0;
  snprintf(url,sizeof url,"http://127.0.0.1:%d/player_api.php?action=get_vod_streams",atoi(argv[1]));
  char *body=rede_baixar_st(url,15,NULL,&status);
  assert(body&&status==200&&strstr(body,"[]"));free(body);
  snprintf(url,sizeof url,"http://127.0.0.1:%d/idle",atoi(argv[1]));
  clock_gettime(CLOCK_MONOTONIC,&began);rede_parar_quando(stop,NULL);
  body=rede_baixar_st(url,120,NULL,&status);
  assert(!body&&elapsed_ms()<3000&&"idle 120-second request must abort promptly");
  clock_gettime(CLOCK_MONOTONIC,&began);
  body=rede_postar_st(url,120,NULL,"fixture",&status);
  assert(!body&&elapsed_ms()<3000&&"idle proxy POST must abort promptly");
  rede_parar_quando(NULL,NULL);return 0;
}
