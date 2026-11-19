// Real Home assembly and real profile storage, with synthetic catalogue rows.
#define HOME_DADOS_REAIS 1
#define main home_layout_main_unused
#include "fv_cat_756_home_layout.c"
#undef main
#include "dados.h"

static const char *keys[] = {"ordinary_a", "ordinary_b", "srv:01234567:lib:11"};
static void build(int count, int changed_key) {
  CatItem items[9] = {{0}};
  CatFileira rows[3] = {{0}};
  for (int i = 0; i < count; i++) {
    snprintf(rows[i].chave, sizeof rows[i].chave, "%s",
             i == 2 && changed_key ? "srv:01234567:lib:22" : keys[i]);
    snprintf(rows[i].titulo, sizeof rows[i].titulo, "Row %d", i);
    if (i != 2) snprintf(rows[i].base, sizeof rows[i].base, "https://example.invalid/addon");
    snprintf(rows[i].tipo, sizeof rows[i].tipo, "movie");
    rows[i].ini = i * 3; rows[i].n = 3;
  }
  cat_definir_tudo(items, 9, rows, count);
  sincronizarFileiras();
}
static void expect(const char *const *expected, int n) {
  assert(nFileiras == n);
  for (int i = 0; i < n; i++) assert(!strcmp(fileiras[i].chave, expected[i]));
}
int main(int argc, char **argv) {
  assert(argc == 2);
  dados_iniciar(NULL);
  assert(getenv("NUVIO_DADOS") && !strcmp(dados_dir(), getenv("NUVIO_DADOS")));
  fil_definir_perfil(!strcmp(argv[1], "other") ? 2 : 1);
  homeCom(HOME_LAYOUT_MODERNA, 0);
  const char *initial[] = {keys[0], "social_activity", keys[1], keys[2]};
  if (!strcmp(argv[1], "write")) {
    build(3, 0); expect(initial, 4);
    int i;
    for (i = 0; i < fil_n() && strcmp(fil_chave(i), keys[2]); i++);
    assert(i < fil_n());
    while (i > 0) { int next = fil_mover(i, -1); assert(next >= 0 && next < i); i = next; }
    const char *ordered[] = {keys[2], keys[0], "social_activity", keys[1]};
    build(3, 0); expect(ordered, 4);
  } else if (!strcmp(argv[1], "read")) {
    const char *partial[] = {keys[0], "social_activity", keys[1]};
    const char *ordered[] = {keys[2], keys[0], "social_activity", keys[1]};
    build(2, 0); expect(partial, 3);
    build(3, 0); expect(ordered, 4);
    build(3, 1);
    const char *changed[] = {keys[0], "social_activity", keys[1], "srv:01234567:lib:22"};
    expect(changed, 4);
  } else {
    assert(!strcmp(argv[1], "other"));
    build(3, 0); expect(initial, 4);
  }
  puts("issue756 scenario passed");
  return 0;
}
