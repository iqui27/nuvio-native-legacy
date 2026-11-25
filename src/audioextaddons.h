#ifndef NV_AUDIOEXTADDONS_H
#define NV_AUDIOEXTADDONS_H
// "Addons" of "From another source" (Settings, owner 13/11/2026): the add-ons
// whose versions the player's audio pages list. Saved per profile as the
// keys switched OFF (audioext-addons-p<N>.txt, one key per line), so an
// add-on installed later shows at once. The key is the add-on instance's own
// (fontefalha_addon_chave of its base URL, Stream.addonChave): two installs
// with the same name are told apart. The Sources list for the picture does
// not use it.

#define AUDIOEXT_ADDONS_MAX 48

// 1 unless switched off. A source without a key (plugins, extras) always 1.
int audioext_addon_ligado(const char *chave);
void audioext_addon_definir(const char *chave, int ligado);
// Bumps on every change and on a profile change: the audio pages rebuild.
unsigned audioext_addons_versao(void);

// The installed add-ons that give sources (switched on in the account), for
// the Settings list and its "2 of 4". Count; each with its name, a host to
// tell instances apart, and its key.
typedef struct {
  char nome[96];
  char host[64];
  char chave[40];
} AudioExtAddon;
int audioext_addons_lista(AudioExtAddon *out, int max);
// Of those, how many are on. 1 when some are off (*ligados < *total).
int audioext_addons_resumo(int *ligados, int *total);

#endif
