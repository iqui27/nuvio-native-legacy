/* AUDIO FROM ANOTHER SOURCE on the Samsung .tpk (donor, owner 15/11/2026).
 *
 * Plays the picture of one source with audio tracks taken from another source
 * of the same title: a release whose audio the TV plays as it is (E-AC3, AC3)
 * instead of converting the file's DTS/TrueHD, or a dub the file lacks.
 *
 * How (proven on the S95C with TorBox burn-in, branch subtitle-rewrite,
 * src/dts/tbaudio_tpk.c): a local HTTP server on 127.0.0.1 serves the TV a
 * master playlist whose video is the local DTS converter's playlist in
 * video-only mode (dts_tpk_abrir with DTS_REMUX_NO_AUDIO: its segments carry
 * no audio, which the TV needs to play a rendition) and whose audio tracks are
 * HLS renditions, 2 s segments made on request from the playing file and from
 * the donor. The TV lists the renditions as its audio tracks and plays the
 * chosen one in sync with the video.
 *
 * Audio by codec: AAC / AC3 / E-AC3 / Opus / MP3 copied into MPEG-TS; DTS,
 * TrueHD/MLP converted to AC3 (5.1 640 kb/s, 2.0 192 kb/s); FLAC copied into
 * fragmented MP4 (or converted, `flac`).
 *
 * Timeline: the converter writes every packet at its source time +
 * DTS_REMUX_TS_OFFSET_S. The file's audio goes there as it is; the donor's
 * at its own time + (the file's video start - the donor's) + doador_ms.
 *
 * Connections: one audio connection by default. The source the player is not
 * listening to gives its connection back (dts_engine_release: its header and
 * index stay; taking it again is one Range request). The converter holds one
 * more for the picture. Nothing here logs a URL.
 *
 * Only the .tpk built with FFmpeg has the server; elsewhere the calls are
 * stubs (doa_disponivel() == 0). Host test: tests/dts_tpk_servidor.sh. */
#ifndef NV_DOADOR_TPK_H
#define NV_DOADOR_TPK_H
#include <stddef.h>

/* Donor tracks are keyed DOA_DOADOR(k) (k = its audio track, 0-based, in
 * container order); the file's keep their ordinal. */
#define DOA_DOADOR(k) (1000 + (k))
int  doa_disponivel(void);
typedef struct {
  const char *video;          /* the local converter's URL (dts_tpk_abrir in video-only mode) */
  const char *orig, *cab;     /* the playing file and the add-on's raw header lines */
  int audio;                  /* the file's audio track to start on (its ordinal) */
  int flac;                   /* 1: convert FLAC (else copied into fMP4) */
  const char *doador, *doador_cab, *doador_nome;   /* NULL/"": no donor; nome: the source's label (no URL) */
  const char *doador_lingua;  /* the donor track to start on: these languages in order, comma-separated ("": its first) */
  int doador_faixa;           /* the donor's audio stream picked by hand (container stream index + 1; 0 none): first */
  /* Series memory: the donor track picked in the last episode. The donor's
   * track in that language most like it (title, codec, channels) wins over
   * the language list. NULL/"" none. */
  const char *dica_lingua, *dica_codec, *dica_titulo;
  int dica_canais;
  int doador_padrao;          /* 1: start on the donor's track; 0: on `audio` */
  int doador_ms;              /* the donor's offset, ms, positive = later */
  int duas_conexoes;          /* test: both audio sources keep their connections */
  /* The donor's speed against the file, ppm (0 the same; a PAL version
   * +42708): its audio is played slower or faster to match (pitch follows,
   * as a PAL slowdown) and so always converted (AC3 / AAC), never copied.
   * 0 with frame rates that name a known film/TV ratio and lengths that agree
   * within 1 %: that speed by itself (doa_diferenca's ppm says it). */
  int doador_ppm;
  int seguir;                 /* another cut: follow it through the film after a find (doa_seguir_evento) */
  /* Master playlist form (owner 17/11: 4K remux sources never asked for an
   * audio segment). 0 = as proven on the S95C; 1 = the retry's: CODECS on the
   * STREAM-INF (video + every rendition) and BANDWIDTH capped at 60 Mbit/s. */
  int variante;
  /* At most this many audio renditions in the master (the starting track
   * always among them); 0 = all. The 4K test row (owner 17/11). */
  int rend_limite;
  /* TORBOX BURN-IN (owner 26/11/2026): `video` is TorBox's burned video
   * (tbaudio_tpk.h, tba_abrir_video), not the converter's. The audio goes
   * onto TorBox's timeline (its first video PTS) instead of the converter's,
   * the master takes TorBox's bandwidth and resolution, and the audio match
   * is off (it reads the file's audio from the converter, which is closed);
   * the subtitle match and the edit map work as before. */
  int tb_video;
} DoaAbrir;
/* New session (replaces any previous one): writes the URL the TV player opens.
 * The sources are read in a thread; the player's requests wait for it. 0 =
 * cannot (no FFmpeg, no server). */
int  doa_abrir(const DoaAbrir *o, char *out, size_t n);
void doa_fechar(void);
int  doa_ativo(void);
int  doa_pronto(void);
/* The session failed to prepare (the playing file did not open): 1 with the reason (no URL). */
int  doa_falhou(char *motivo, size_t n);
/* The renditions in the master's order (= the TV's audio list): count, up to max. */
typedef struct { int chave, fonte, ord, si, canais; char lingua[16], nome[96], codec[24], codecFonte[24]; } DoaRendicao;
/* si: the track's stream index in its container; codecFonte: its codec there
 * (codec is what the rendition carries: "ac3" for a converted DTS). */
int  doa_rendicoes(DoaRendicao *out, int max);
/* The rendition of a key (index in the master's list), -1 not offered. */
int  doa_rendicao_de(int chave);
/* The track the next playlist/segments use (the DEFAULT=YES rendition), and the current one's key. */
void doa_definir_audio(int chave);
int  doa_audio(void);
/* New donor offset (ms, positive = later): the next segments carry it. The app
 * then seeks in place, so the player drops the segments it holds. */
void doa_desloc(int ms);
int  doa_desloc_ms(void);
/* 1 the donor's tracks are offered, 0 no donor (or still preparing), -1 it
 * failed to open (reason in `motivo`, no URL): the file's own tracks go on. */
int  doa_estado(char *motivo, size_t n);
/* A summary line for the 10 s log: segments, connections to the files' hosts,
 * reader memory, which source holds a connection, the last reconnection. */
void doa_log(void);
/* Language comparison for the donor track to start on (the app passes
 * ling_casa: "pl" matches "pol"); NULL or unset compares case-insensitively. */
void doa_definir_casa(int (*casa)(const char *codigo, const char *pref));
/* The donor against the file, once both opened (1; 0 not yet / no donor):
 * corte = the donor's length minus the file's (s, another cut when large),
 * velocidade = 1 when their frame rates differ (a sped-up version), ppm =
 * the donor's speed against the file's from the frame rates (0 unknown). */
int  doa_diferenca(int *corte, int *velocidade, int *ppm);
/* The audio tracks of a file (the audio menu's list for another version):
 * blocking, opens its header. Returns the count, -1 it did not open, -3 the
 * debrid's placeholder clip (under two minutes). */
typedef struct { int si, canais; char codec[24], lingua[16], titulo[64]; } DoaSonda;
int  doa_sondar(const char *url, const char *cab, DoaSonda *out, int max);
/* PRE-OPEN (owner 18/11, log 13: a pick reopened the player at once and it
 * waited for the donor, 3.6-13.1 s without a picture; a cold debrid node took
 * 10.0 s and the 10 s deadline dropped it). Opens the donor's header the way
 * the session will (warms the debrid node, DNS and TLS) while the audio in
 * force keeps playing; blocking, run it in a thread. Past `prazo_ms` its
 * reader is cancelled. 1 = it answered, -3 = the debrid placeholder clip,
 * 0 = no answer / error (`motivo`). */
int  doa_preabrir(const char *url, const char *cab, int prazo_ms, char *motivo, size_t n);
/* MATCH AUTOMATICALLY (as the webOS engine, dts_match.inc): ~40 s of the
 * file's audio (taken from what the picture's reader downloads anyway, the
 * converter's tap) and of the donor's playing track (what its segments read)
 * become band envelopes; the offset is where their onsets correlate best,
 * first near the current offset, then (reading the donor on its own) where
 * the lengths suggest, +-60 s and +-5 min. Only while the donor's track
 * plays. Result: ms the donor offset (doa_desloc's), ppm its speed, como 1
 * near / 2 lengths / 3 +-60 s / 4 +-5 min, dica_ms the lengths' hint and
 * pesado a wide search skipped for its download (not found). */
typedef struct { int ms, ppm, como, dica_ms, pesado, ncand, cand_ms[3], cand_pct[3]; } DoaDopasuj;
void doa_dopasuj(int automatico);
/* 0 nothing new, 1 found, -1 not found (once per request). */
int  doa_dopasuj_resultado(DoaDopasuj *out, int *automatico);
int  doa_dopasuj_ativo(void);
/* FOLLOWING ANOTHER CUT (DoaAbrir.seguir, lengths over 10 s apart): after a
 * find, the same near search every 2 minutes of playback; a shift over
 * 250 ms is applied by itself (4, the new offset in ms), two misses in a row
 * say it is lost (3, once). Only for a donor the subtitles could not place
 * (owner 18/11: with subtitles the edit map does it; doa_aplicar_legendas
 * turns it off). 5: the playhead entered another stretch of the edit map
 * (ms its offset; already applied, nothing to say). 0 nothing new. */
int  doa_seguir_evento(int *ms);
/* The donor's health, each said once: 1 it stopped answering (three of its
 * segments failed in a row: the app goes back to the file's own audio);
 * *lento = 1 it downloads slower than the picture plays (two 2 s segments
 * that took over 10 s). */
int  doa_doador_saude(int *lento);
/* Requests the TV made to this session so far: audio segments (and fMP4
 * inits), and every request. The app's no-audio check (video_tpk.c). */
int  doa_pedidos_audio(void);
int  doa_pedidos(void);

/* SUBTITLE MATCH, the first level of the sync (owner 17/11): the embedded
 * TEXT subtitle tracks of the file (fonte 0) and of the donor (fonte 1) as
 * their seek index lists them (the start of each indexed block, in that
 * source's own time). Read from the index prepFonte loads anyway: no request
 * more. Tracks with fewer than 20 points are left out. */
#define DOA_LEG_MAX 32   /* every text track of a file (owner 17/11: the French one was the 13th) */
/* n: index points; falas: the lines they make once a burst (an ASS line drawn
 * in pieces, karaoke, a typeset sign: starts within 0.15 s; 0.02 s for the
 * others) counts as one, what the match compares (0 = not counted: n). */
typedef struct { char lingua[16], nome[64], codec[16]; int forcada, n, falas; } DoaLegInfo;
#define DOA_SURTO_ASS 0.15
#define DOA_SURTO_SRT 0.02
static inline int doa_falas(const double *t, int n, int ass) {
  int i, m = 0;
  double ult = -1e9;
  for (i = 0; i < n; i++) {
    if (t[i] < 0) continue;
    if (m && t[i] - ult < (ass ? DOA_SURTO_ASS : DOA_SURTO_SRT)) continue;
    ult = t[i]; m++;
  }
  return m;
}
int  doa_legendas(int fonte, DoaLegInfo *out, int max);
/* Start times (s, sorted) of track `k` of doa_legendas; returns the count. */
int  doa_legenda_tempos(int fonte, int k, double *t, int max);
/* The subtitle match's result: the file's time = k * the donor's time + desloc
 * (container times). Handed to the app like a match (doa_dopasuj_resultado,
 * como 6, automatico 1): ms and ppm against the session's base. */
void doa_aplicar_legendas(double k, double desloc, double conf);
/* THE EDIT MAP (owner 18/11, "scan the subs and know where the major
 * differences are and adjust for them"): where two releases differ (an
 * extra/missing intro, recap, preview, eyecatch), each stretch of the file
 * has its own offset. Built once per donor from the subtitles (doadorleg.c),
 * applied by the donor server per segment: the offset of a segment is the
 * map's at its start (the file's time), so an edit is a donor seek between
 * two segments and the TV player never seeks for it. Stretch i covers the
 * file's [ini, fim) (the first from -inf, the last to +inf); file = k *
 * donor + desloc there. At most DOA_MAPA_MAX stretches (owner: more is a
 * wrong title or a broken pair). */
#define DOA_MAPA_MAX 12
typedef struct { double ini, fim, desloc; } DoaTrecho;
typedef struct { int n; double k; DoaTrecho t[DOA_MAPA_MAX]; } DoaMapa;
/* Installs a map of 2+ stretches for the session's donor (n < 2 clears it:
 * one offset, as before). `pos`: the playhead. The stretch there takes the
 * offset in force when a refine or a hand change moved it since the subtitle
 * answer (the whole map shifts with it). */
void doa_mapa_definir(const DoaMapa *m, double pos);
/* The playhead, once a second from the app: the server tells the app when it
 * enters another stretch (doa_seguir_evento 5: the offset shown is that
 * stretch's; nothing to apply). */
void doa_posicao(double pos);
/* The stretch at `pos`: its index (-1 no map), its span, whether its audio
 * refine already ran (1 done, -1 tried and not found, 0 not yet). */
int  doa_mapa_trecho(double pos, double *ini, double *fim, int *refinado);
/* A stretch's own audio refine (+-1 s around its offset, ~40 s captured while
 * playing). Its result changes only that stretch's offset in the server (no
 * seek, no reopen; not handed to the app as a match result). */
void doa_dopasuj_fino_trecho(int trecho);
/* SECOND LEVEL: the audio match refining the offset in force (+-1 s, about
 * 40 s of playback, no wide search). como 7. After a subtitle match its
 * candidates are scored audio + subtitles (owner 18/11): within 0.5 s of the
 * subtitles' offset (their own precision) the audio decides; farther, the
 * subtitles' share fades. */
void doa_dopasuj_fino(void);
/* WHICH AUDIO THE MATCH COMPARES (owner 18/11). The tracks of one file are
 * in sync with each other, so any pair of one language places the donor.
 * Auto: the title's ORIGINAL language on both sides (jpn <-> jpn for anime:
 * the track the picture was made with); a donor without it is compared with
 * the file's track in its own language AND the file's original-language
 * track, each candidate offset scored by both (a wrong dub in the file is
 * outvoted); then the playing track's language, a shared language, any.
 * `orig`: the title's original language ("ja"; "" unknown). `pedida*`: the
 * file track picked in Audio sync > Scan again (language, codec, channels;
 * lingua "" = Auto). `leve`: one file track only (Tizen 4-5.5, low memory;
 * the original language first). Read when a match starts. */
void doa_match_definir(const char *orig, const char *pedidaLingua, const char *pedidaCodec, int pedidaCanais, int leve);
/* The playing file's own audio tracks in this session (offered or not), for
 * the Scan again list: count, up to max. */
int  doa_faixas_arquivo(DoaRendicao *out, int max);
/* The choice above, pure (the host test): fx the session's audio tracks
 * (fonte 0 the file, 1 the donor; ok = a decoder exists), tocando the donor
 * track playing (index in fx). Fills siA (the file's track), siA2 (a second
 * file track scored with it, -1 none), siD (the donor's track when not the
 * playing one, -1 the playing one) and why. 0 = nothing to compare. */
typedef struct { int fonte, si, ok, canais; char lingua[16], codec[24]; } DoaMatchFaixa;
typedef struct { int siA, siA2, siD, como; char porque[160]; } DoaMatchEscolha;
enum { DOA_MC_LINGUA = 0, DOA_MC_ORIGINAL, DOA_MC_DUAS, DOA_MC_PEDIDA, DOA_MC_COMUM, DOA_MC_OUTRA, DOA_MC_ORIG_LEVE };
int  doa_match_escolher(const DoaMatchFaixa *fx, int n, int tocando, const char *orig, const char *pLingua, const char *pCodec,
                        int pCanais, int leve, int (*casa)(const char *, const char *), DoaMatchEscolha *o);
/* Two comparisons of the donor (against two file tracks): every candidate
 * offset (within tol s of each other = one) gets both scores (0 where one
 * comparison lacks it) and their mean; best mean first. Returns the count. */
int  doa_match_somar(const double *oa, const double *sa, int na, const double *ob, const double *sb, int nb, double tol,
                     double *o, double *media, double *scA, double *scB, int max);
#endif
