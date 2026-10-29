#ifndef NV_LEGFONTES_H
#define NV_LEGFONTES_H
#include <stddef.h>

/* Local fonts for subtitles and the interface. Only the SDL/UI thread reads or changes this registry.
 * IDs, never document URIs or user paths, are persisted with each selection. */
#define LEGFONTES_MAX 8
#define LEGFONTE_MAX_BYTES (32u * 1024u * 1024u)
#define LEGFONTE_PRINCIPAL 0
#define LEGFONTE_SECUNDARIA 1
#define LEGFONTE_INTERFACE 2
#define LEGFONTES_SLOTS 3
void legfontes_dir(const char *dir);
int legfontes_n(void);
const char *legfontes_nome(int indice);
int legfontes_selecionada(int slot); /* -1 = bundled font / inherit */
int legfontes_escolher(int slot, int indice); /* -1 clears custom selection */
/* A related atomic preference write runs only after renderer admission. The
 * callback must not call this registry or the renderer, and must keep its own
 * persisted values unchanged on failure. */
int legfontes_escolher_transacao(int slot, int indice, int (*confirmar)(void *), void *u);
int legfontes_remover(int indice);
const char *legfontes_caminho(int slot);
const char *legfontes_nome_ass(int slot);
int legfontes_nome_sfnt(const void *dados, size_t n, char *out, size_t tam);
unsigned legfontes_revisao(void);
/* Pure bounded SFNT container validation; FreeType validates the face too. */
int legfontes_sfnt_valido(const void *dados, size_t n);
/* Imports only an owned entrada-*.tmp inside this module's private directory.
 * The staging file is removed on both success and failure. */
int legfontes_importar(int slot, const char *arquivo);
int legfontes_pedir_importacao(int slot);
/* Poll once per UI tick. Returns a translatable status string, or NULL. */
const char *legfontes_atualizar(void);
#endif
