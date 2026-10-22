// SEEK-BAR THUMBNAILS IN THE OWNER'S GOOGLE DRIVE (.tpk, LG, Android: miniplat.h).
//
// The TV makes the thumbnails (miniatura.c) and keeps them in RAM only. When a
// title's set is done (or at Back), the set goes to Google Drive as ONE file in
// the hidden app folder (scope drive.appdata: only this app sees it, it uses
// the person's own space). The next time the same file plays, the set is
// downloaded instead of made again. Sets unused for 40 days are deleted.
//
// Sign-in: Google's device flow for TVs (a short code entered at
// google.com/device), like the Trakt link (traktauth.c). The refresh token is
// kept on this TV (gdrive.txt); the thumbnails are not.
//
// A set is found by:
//   1. the content fingerprint (size + hash of the first and last 64 KiB):
//      the same file through any link, debrid or add-on;
//   2. else the file name as the debrid service names it (TorBox file name,
//      also for files opened inside a source) + its exact size.
//
// Build keys: NV_GDRIVE_CLIENT_ID / NV_GDRIVE_CLIENT_SECRET (OAuth client of
// type "TVs and Limited Input devices"), from local.properties via env.sh.
// Empty = the Settings row says it is not available in this build.
#ifndef NV_MINIDRIVE_H
#define NV_MINIDRIVE_H
#include <stddef.h>
#include <stdint.h>

typedef enum {
  GDR_PARADO = 0,
  GDR_PEDINDO,      // asking Google for a code
  GDR_AGUARDANDO,   // code on screen, waiting for the person
  GDR_LIGADO,       // linked
  GDR_ERRO
} GdrEstado;

int  minidrive_disponivel(void);     // the build has a client id
void minidrive_carregar(void);       // reads gdrive.txt at start-up
int  minidrive_ligado(void);
void minidrive_comecar(void);
void minidrive_cancelar(void);
void minidrive_desconectar(void);    // forgets the token (the Drive files stay)
GdrEstado   minidrive_estado(void);
const char *minidrive_codigo(void);
const char *minidrive_url(void);
const char *minidrive_erro(void);

// Metadata stored with each set (Drive appProperties).
typedef struct {
  char fp[65];          // content fingerprint ("" when it could not be read)
  char nk[33];          // name key: hash of the normalised file name + size
  long durMs;
  char imdb[24];
  int temporada, episodio;
  int quadros;          // tiles in the set
} MiniMeta;

// BLOCKING (worker thread). 1 = found: *dados (malloc'd) holds the set and
// `id` the Drive file id. 0 = not found, -1 = Drive unreachable.
int minidrive_buscar(const MiniMeta *m, unsigned char **dados, size_t *n, char *id, size_t nid);
// BLOCKING. Creates (id empty) or replaces the set. 0 ok, -1 failed.
int minidrive_enviar(const char *id, const MiniMeta *m, const unsigned char *dados, size_t n);
// BLOCKING. Marks the set as used today.
void minidrive_tocar(const char *id);
// BLOCKING. At most once a day: deletes sets not used for 40 days.
void minidrive_limpeza(void);

// Name key: first 32 hex of SHA-256(lowercase(file name without folders) + "|" + size).
void minidrive_chave_nome(const char *nome, uint64_t bytes, char saida[33]);
#endif
