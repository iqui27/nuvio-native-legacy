#include "guiav5.h"
#include <assert.h>
#include <string.h>
int main(void){
  G5Item out[2];
  const char *json="{\"episodes\":{\"1\":[{\"id\":\"111\",\"title\":\"Pilot\",\"episode_num\":1,\"container_extension\":\"mkv\"}],\"2\":[{\"id\":222,\"title\":\"Return\",\"episode_num\":1}]}}";
  assert(g5_parse_episodes(json,1,out,2)==1);
  assert(!strcmp(out[0].id,"111")&&out[0].kind==G5_EPISODE&&out[0].season==1);
  assert(g5_parse_episodes(json,2,out,2)==1&&!strcmp(out[0].id,"222"));
  assert(g5_parse_episodes(json,3,out,2)==0);
}
