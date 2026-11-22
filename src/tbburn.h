#ifndef NUVIO_TBBURN_H
#define NUVIO_TBBURN_H
// TORBOX BURN-IN (owner, 12/11/2026). An embedded ASS/SSA track on a TorBox
// source plays as TorBox's transcoded HLS stream with the track burned into
// the picture: frame-exact, and the TV's picture processing applies to it.
// TorBox transcodes for Pro accounts only, so it needs Settings > Debrid >
// "TorBox plan" = Pro, "Burn in ASS with TorBox" On (default) and a TorBox key.
//
// The playing URL stays the ORIGINAL file for everything else (sync, resume,
// chapters, thumbnails: video_url_atual() in video_tpk.c); only the URL handed
// to the TV player changes. This module finds the file in the account, asks
// TorBox for the stream and logs everything with the key redacted.
//
// TorBox's web player URL is .../stream/<id>/<resolution>/<subtitle>/<audio>/
// playlist.m3u8 (owner's screenshot, 12/11): each index counts only the tracks
// of its own kind, from 0, in file order.
#include <stdint.h>

// TBB_ESPERANDO (owner 19/11/2026): the file was gone from the account and
// Nuvio re-added it by its hash (cached only); TorBox may need minutes to
// register it, so the request keeps asking (every ~20 s, up to 3 min) while
// the original plays with Nuvio's subtitles. Ends in TBB_PRONTO or TBB_FALHOU.
enum { TBB_NADA = 0, TBB_BUSCANDO, TBB_PRONTO, TBB_FALHOU, TBB_ESPERANDO };

// The source the player opened (player.c, after video_tocar). `url` is the
// URL the player plays (video_url_atual()); the rest comes from the add-on.
void tbburn_fonte(const char *url, const char *hash, const char *arquivo, uint64_t bytes);

// The exact TorBox file of `url` when it is already known (a file picked in
// "files of the source": video_tpk_cdn_arquivo). Skips the account lookup.
void tbburn_ids(const char *url, int usenet, int item, int arquivo);

// The saved last link about to play (app.c, "Reuse last link"): its file name,
// size and hash, used when the player opens that URL without the add-on list.
void tbburn_fonte_guardada(const char *url, const char *hash, const char *arquivo, uint64_t bytes);

// EARLY LOOKUP (owner 18/11). The account lookup (partes_achar_exato: TorBox's
// list has no hash filter, 0.7-3.6 s) starts before the file's header is read:
// when a source has been focused 1 s in the source list, and on the frame after
// Play (tbburn_cedo_bombear, video_tpk.c). Only with burn-in possible
// (tbburn_pode) and a file that may be MKV. One lookup at a time; the newest
// request waits for it (older waiting ones are dropped). A hit is kept 30 min
// per file (hash + file name + size); a miss only stops new early lookups of
// that file for 1 min: the worker still looks up again (owner 14/11). The
// worker waits for a running early lookup of its file instead of asking twice.
void tbburn_procurar_cedo(const char *url, const char *hash, const char *arquivo, uint64_t bytes, const char *porque);
void tbburn_cedo_bombear(void);

// Settings On + plan Pro + a TorBox key, not refused by TorBox this app
// session, and not switched Off for this playback.
int  tbburn_pode(void);
// The in-playback "Burn in" row: -1 = follow Settings, 0 = Off, 1 = On. Lasts
// for the session (Next episodes too); tbburn_sessao_nova clears it.
int  tbburn_escolha(void);
void tbburn_definir_escolha(int v);
void tbburn_sessao_nova(void);
// The next close is the step to the next episode: keep the playback choice.
void tbburn_manter_proximo(void);
// The row is worth showing: plan Pro, a key, and the file is on TorBox (found)
// or not looked up yet. Set by video_tpk.c when the file has an embedded ASS.
int  tbburn_linha_visivel(void);
void tbburn_arquivo_tem_ass(int sim);

// Asks TorBox for the burned stream of `url` with subtitle `sub` and audio
// `audio` (indices among tracks of their kind). Async; one at a time.
void tbburn_pedir(const char *url, int sub, int audio);
// TBB_*; on TBB_PRONTO `m3u8` holds the playlist URL (with TorBox's token: never
// log it raw). `motivo` (may be NULL) says why on TBB_FALHOU.
int  tbburn_resultado(char *m3u8, unsigned n, char *motivo, unsigned mn);
void tbburn_cancelar(void);
// TorBox refused the plan on the last request (not Pro there). Owner
// 19/11/2026: burn-in never goes away on Pro; that request draws, the next
// pick or file asks again.
int  tbburn_recusado(void);

#ifdef NV_TPK
// video_tpk.c. _queimada: the burned menu entry (-1 none). _voltou, once:
// 1 = burn-in gave up on this source (toast), 2 = Burn in turned Off; either
// way the ASS tracks are back in Main's list (the caller re-picks Main).
int  video_tpk_queimada(void);
int  video_tpk_queimar_voltou(void);
// Re-added file ready mid-episode (owner 19/11/2026). _aviso, once: 1 = show
// the "Burn-in ready" notice (OK switches), 2 = switched (Main leaves the
// burned ASS track). _tardio_ok: OK with the controls hidden while the notice
// is up opens the burned stream; 1 = the OK was used.
int  video_tpk_queimar_aviso(void);
int  video_tpk_queimar_tardio_ok(void);
#endif

// HDR (owner 13/11): TorBox's transcode would lose HDR, so burn-in starts Off
// on an HDR file (the source's HDR badge, set by player.c before the open, or
// the file's colour info, video_tpk.c). Turning it On in playback is
// remembered for the show (tbburn_hdr_lembrar(1); Off clears it).
void tbburn_hdr_fonte(int sim);
int  tbburn_hdr_da_fonte(void);
int  tbburn_hdr_permitido(void);
void tbburn_hdr_lembrar(int sim);

// ---- the burned layer (owner 13/11) -------------------------------------------
// Burned in is its own layer next to Main and Second. At open: the track
// remembered for the show (or Off), else Signs & Songs, else the dialogue ASS
// track. A burnable track = an embedded ASS/SSA track of the file.
typedef struct {
  char idioma[16], nome[96];
  int  tipo;        // LING_LEG_* (COMUM / FORCADA / LETREIROS)
  int  letreiro;    // the forced/signs flag of the track
} TbFaixa;
// The show of this playback (legmem_id_titulo form: imdb of the series).
void tbburn_titulo(const char *titulo);
// What the show remembers: 0 = nothing, 1 = Off, 2 = a track (*m filled).
int  tbburn_memoria(TbFaixa *m);
// A pick in the Burn in list: `f` NULL = Off. Saved per show and profile.
void tbburn_lembrar(const TbFaixa *f);
// Which of `f[0..n)` to burn: -1 = none. `mem`: 0 nothing / 1 Off / 2 track
// (`m`); `pref`: the subtitle language from Settings ("" none). Pure.
int  tbburn_escolher_auto(const TbFaixa *f, int n, int mem, const TbFaixa *m, const char *pref);

#ifdef NV_TPK
// video_tpk.c. The Burn in list (legendasui.c): `i` = a subtitle menu index of
// a burnable track, -1 = Off. _ligar: On = the automatic choice, Off = -1.
void video_tpk_queimar_faixa(int i);
void video_tpk_queimar_ligar(int ligar);
// 1 = subtitle `i` is a burnable (embedded ASS) track of this file.
int  video_tpk_leg_queimavel(int i);
// 1 = burned in is on (or being asked for) and `i` is burnable: hidden from Main/Second.
int  video_tpk_leg_oculta(int i);
// Audio menu row under the tracks (faixas.c), original audio with burn-in
// (dts/tbaudio_tpk.h): "Original audio: Passthrough | TorBox audio" (saves
// Settings; reopens while burned). One row (k is always 0); FLAC goes as is.
int  video_tpk_tba_linhas(void);
void video_tpk_tba_rotulo(int k, char *nome, unsigned long nn, char *sub, unsigned long ns);
void video_tpk_tba_ok(int k);
// Left/Right on a row: changes the value shown only; OK applies (owner 14/11).
void video_tpk_tba_passo(int k, int dir);
// The sheet opened or closed without OK: back to the current values.
void video_tpk_tba_soltar(void);
#endif

// ---- pure (tests) ----------------------------------------------------------
// The early lookup's key for a file: hash (40 hex, lower case) + file name +
// size; the URL only when none of them is known. "" = nothing to look up by.
void tbburn_chave(char *out, unsigned n, const char *url, const char *hash, const char *arquivo, uint64_t bytes);
// 0 = the file name (else the URL's last path part) ends in a video extension
// other than .mkv (mp4, avi, ts...): no embedded ASS to burn. 1 otherwise.
int  tbburn_parece_mkv(const char *url, const char *arquivo);
// createstream's answer says the item is gone from the account (ITEM_NOT_FOUND,
// or a failed 404): the list it came from was out of date.
int  tbburn_eh_item_sumido(int st, const char *r);
// First http(s) URL ending a path in ".m3u8" anywhere in `corpo` (JSON string
// values unescaped: \/ \" \\). 1 found.
int  tbburn_achar_m3u8(const char *corpo, char *out, unsigned n);
// The string value of the first "chave": "..." anywhere in `corpo`. 1 found.
int  tbburn_json_texto(const char *corpo, const char *chave, char *out, unsigned n);
// Whether TorBox's answer says the account plan can't do this.
int  tbburn_eh_recusa_plano(int st, const char *corpo);
// The createstream route for item/file/sub/audio (no key in it).
void tbburn_rota_criar(char *out, unsigned n, int item, int arquivo, int usenet, int sub, int audio);
// Before a TorBox answer, playlist or URL goes to the log, in place: URLs keep
// only their host, query strings and the value of every "...token" field
// become ***. The text never grows.
void tbburn_log_mascarar(char *s);

#endif
