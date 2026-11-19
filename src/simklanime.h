// SIMKL ANIME ANSWERS ON DISK: one file for the whole TV.
//
// Continue watching asks Simkl for an anime's details (/anime/{id}) and its
// episode list (/anime/episodes/{id}). These are public catalog data, the same
// for every account, and Simkl asks apps to cache them
// (api.simkl.org/api-rules.md, "Cache aggressively"). They used to live in the
// shared 64-slot metadata cache (fichameta.c), where keys picked by hash
// overwrote each other and the card metadata: the same anime was asked again
// on every rebuild.
//
// Here every answer keeps its own entry. The file is read once, kept in
// memory, and rewritten when an answer is saved. Callers store only the
// fields they read (simkl.c trims the answers), so entries stay small; past
// SIMKLANIME_MAX_BYTES the oldest are dropped. A damaged file or one of
// another version is ignored and rebuilt: everything in it can be asked again.
#ifndef NV_SIMKLANIME_H
#define NV_SIMKLANIME_H

#define SIMKLANIME_ARQ "simkl-anime.txt"
#define SIMKLANIME_MAX_BYTES (1024 * 1024)

// A copy (free) of the answer saved for `chave` at most `ttlSeg` seconds ago,
// or NULL when there is none or it is older.
char *simklanime_ler(const char *chave, long ttlSeg);

// Saves `corpo` for `chave` now, replacing what was there, and writes the
// file. A body larger than the cap is not kept.
void simklanime_gravar(const char *chave, const char *corpo);
// The same, saved as of `quando` (epoch seconds): tests age an answer.
void simklanime_gravar_em(const char *chave, const char *corpo, long long quando);

// Forgets what is in memory; the next call reads the file again (tests).
void simklanime_descarregar(void);

#endif
