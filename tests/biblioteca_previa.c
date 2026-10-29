// Session previews must belong to a list and disappear on profile changes.
#include "../src/listas.c"
#include <assert.h>
int main(void) {
  LstLista a = { .fonte = LST_TRAKT, .traktId = 77 };
  LstLista b = { .fonte = LST_TRAKT, .traktId = 88 };
  CatItem item = {0};
  strcpy(item.poster, "portrait.jpg");
  LstPrevia out;
  assert(!lst_previa(&a, &out));
  lst_previa_registrar(&a, &item, 1);
  assert(lst_previa(&a, &out) && out.n == 1);
  assert(!strcmp(out.poster[0], "portrait.jpg"));
  assert(!lst_previa(&b, &out));
  lst_previa_limpar();
  assert(!lst_previa(&a, &out));
  puts("library previews: PASS");
}
