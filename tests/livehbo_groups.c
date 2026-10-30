/* Exercise the setup picker with more than 96 live provider categories. */
#include "../src/guiav5ui.c"
#include <assert.h>
int perfis_ativo(void) { return 1; }
char *dados_ler(const char *name) { (void)name; return NULL; }
int main(void) {
  count=160;inventory=calloc((size_t)count,sizeof *inventory);assert(inventory);
  for(int i=0;i<count;i++){
    inventory[i].kind=G5_LIVE;
    snprintf(inventory[i].group,sizeof inventory[i].group,"Category %03d",i);
  }
  strcpy(inventory[159].group,"HBO");
  build_groups();
  assert(group_count==160);
  assert(!strcmp(groups[159],"HBO")&&group_totals[159]==1&&group_allowed("HBO"));
  free(inventory);
  puts("PASS: live setup picker retains all 160 categories including HBO");
  return 0;
}
