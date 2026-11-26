#ifndef NUVIO_TBABRIR_H
#define NUVIO_TBABRIR_H
// OPENING A TORBOX FILE (owner 25/11/2026, from subtitle-rewrite's burn-in
// start, without the transcoding). Shared C: .tpk, LG and Android alike.
//
// EARLY LOOKUP. The file of a TorBox source is looked up in the account
// (partes_achar_exato: TorBox's list has no hash filter, 0.7-3.6 s) before it
// is needed: when a source has been focused 1 s in the source list
// (stream_foco_demorado) and when it starts opening (Play). One lookup at a
// time; the newest request waits for the running one (older waiting ones are
// dropped). A hit is kept 30 min per file (hash + file name + size); a miss
// only stops new early lookups of that file for 1 min.
//
// STUCK ITEM RECOVERY. A TorBox file that never shows a picture (player error
// before the first frame, or the opening deadline) is often an item TorBox
// keeps listing but no longer serves: removing it on the dashboard and adding
// the hash again made it play (owner, log 19/20). Once per file per playback,
// the app holds the source and this module, on its own thread:
// - finds the item (the early lookup's hit, else a lookup);
// - torrent with a hash still cached on TorBox (checkcached first: a file no
//   longer cached is never taken out of the account, it only gets a fresh
//   link): deletes it, adds the hash again (cached only, never a real download), drops every account list asked for before the add, looks
//   again 3.5 s later in fresh lists (and once more 2 s after that); the app
//   holds the source 12 s at most (direct files need no transcoder wait);
// - not in the account but a hash is known: adds it, same looks;
// - usenet download (no hash to add): only a fresh download link;
// - found: a fresh download link (requestdl) for the app to reopen at the
//   same point. Anything else: the app's usual failover / error.
// An added torrent listed without the playing file (the hash was another
// torrent) is deleted again; a hash whose torrent is listed without the file
// is never added.
#include <stdint.h>
#include "streams.h"

// ALLDEBRID AND REAL-DEBRID (owner 25/11/2026): the same early lookup (the
// account list by the torrent's hash, read at most once a minute) and hold.
// - AllDebrid has no separate cache check, so the hash is uploaded first and
//   its `ready` is the check: only then is the stuck magnet deleted (a new
//   ready magnet replaces it, or the same one is deleted and uploaded again);
//   not confirmed: nothing is deleted, a fresh unlock of the file only. A
//   magnet that is not ready is never deleted.
// - Real-Debrid has no cache check at all: nothing is ever deleted or added;
//   the torrent in the account gives a fresh unrestricted link only.
// - Premiumize needs none of this: its directdl hands a fresh link of the
//   cached file per call and nothing sits in the account to get stuck.

// The service a source plays from, for all of the above: DEBRID_TB,
// DEBRID_AD or DEBRID_RD (resolved by Nuvio with it, or the add-on's
// link/label says it) when its key can use the API; -1 otherwise.
int  tbabrir_servico(const Stream *s);
// tbabrir_servico(s) == DEBRID_TB.
int  tbabrir_fonte_torbox(const Stream *s);

// The early lookup (see above). `porque` goes to the log.
void tbabrir_procurar_cedo(const Stream *s, const char *porque);

// The recovery key of a source (hash + file name + size, else the URL): the
// app keeps it to try once per file per playback.
void tbabrir_chave(char *out, unsigned n, const char *url, const char *hash, const char *arquivo, uint64_t bytes);

// Starts the recovery for `s` (async, one at a time; a new one cancels the
// old). 0 = nothing to try (no hash and not found is only known later: then
// tbabrir_resultado answers -1).
int  tbabrir_recuperar(const Stream *s, const char *urlAtual);
// 0 = working, 1 = done (`url` = the fresh link), -1 = failed (`motivo`
// says why), -2 = nothing asked. 1 and -1 are consumed by the read.
int  tbabrir_resultado(char *url, unsigned n, char *motivo, unsigned mn);
void tbabrir_cancelar(void);

// Pure (tests): the 40-hex hash to re-add, lower case, from the add-on's hash,
// else the item's hash from TorBox's list, else a 40-hex string in the link.
// 1 = `out` filled.
int  tbabrir_hash_para_readicionar(const char *hashFonte, const char *hashItem, const char *url, char out[41]);

#endif
