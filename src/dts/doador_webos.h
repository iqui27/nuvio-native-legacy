/* AUDIO FROM ANOTHER SOURCE on LG (webOS), the Samsung way (owner 26/11):
 * the calls of doador_tpk.h that the audio menu and the subtitle match
 * (doadorleg.c) ask, answered over the LG converter (dts_engine.c, the HLS
 * remux of dts_remux.c) instead of the .tpk's donor server.
 *
 * The converter tells this file when it opened the playing file and the other
 * version (dts_engine_set_external_open_hook): the start of every indexed
 * block of their text subtitle tracks is copied here (their seek index, no
 * byte more is downloaded), with the file's own audio tracks and what the two
 * containers say about lengths and frame rates. video.c runs the match and
 * hands its answer to the converter as an offset. */
#ifndef NV_DOADOR_WEBOS_H
#define NV_DOADOR_WEBOS_H

/* The hook (worker thread): both AVFormatContexts as void *. `hls` 1 for the
 * HLS remux (its donor base scales with the speed), 0 for the converter. */
void doaw_fontes_abertas(void *mainFmt, void *donorFmt, int hls);
/* Bumped by each opening the hook saw (0: none yet this session). */
unsigned doaw_geracao(void);
void doaw_limpar(void);
/* The app's offset (ms, positive = the other audio later) for the match's
 * line file = k * donor + desloc (container times). 0 before an opening. */
int  doaw_ms(double k, double desloc, int *ms);
/* The file's audio stream (container index) named by Scan again's pick
 * (audioext_match_faixa), -1 none / Auto. */
int  doaw_faixa_si(const char *lingua, const char *codec, int canais);

#endif
