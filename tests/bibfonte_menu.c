// Inspect the real context-menu action, including a missing-action failure.
#include "../src/ctxmenu.c"
void bibfonte_menu_toggle(int index) {
  ctx_abrir(index);
  for (int i = 0; i < nOps; i++) {
    if (ops[i].acao == OP_LISTA) { foco = i; aplicar(); break; }
  }
  aberto = 0;
}
const char *bibfonte_menu_label(int index) {
  ctx_abrir(index);
  for (int i = 0; i < nOps; i++) {
    if (ops[i].acao == OP_LISTA) { aberto = 0; return ops[i].rot; }
  }
  aberto = 0;
  return "";
}
