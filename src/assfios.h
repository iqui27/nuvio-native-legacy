// NAMES OF THE SUBTITLE THREADS (.tpk, Linux). The libass worker is
// "nv-assrender" and libass's blur helper threads "nv-assblur", so they can be
// told apart from the UI in top/ps on the TV. Elsewhere these do nothing.
#ifndef NV_ASSFIOS_H
#define NV_ASSFIOS_H

// Name of the calling thread (Linux comm, up to 15 letters). Threads it starts
// afterwards inherit it: that is how libass's blur threads get theirs.
void assfios_nomear(const char *nome);
// Rename only for the length of a call: `antigo` (16 bytes) keeps the current one.
void assfios_nome_trocar(const char *nome, char *antigo);
void assfios_nome_voltar(const char *antigo);

#endif
