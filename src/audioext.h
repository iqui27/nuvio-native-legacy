#ifndef NV_AUDIOEXT_H
#include <stddef.h>
#include <stdint.h>
#define NV_AUDIOEXT_H
// AUDIO FROM ANOTHER VERSION OF THE TITLE (test build, external-audio-test).
//
// In the source sheet the BLUE key on a source marks it as the audio source:
// the next source played for the same title/episode keeps its own picture and
// takes the audio of the marked one (a WEB-DL dub over a remux, for example).
// The converter reads only the audio of the marked file, over its own small
// ring (dts_engine_external_audio). BLUE on the marked source again clears it.
// While it plays, CH+/CH- move the audio 50 ms later/earlier.
//
// Main thread only. Nothing is saved: it lasts until another title or episode
// opens its source list, or the app closes.

// The source list now belongs to this target ("tt123", "tt123:1:2"). A
// different target than the marked one forgets the mark.
void audioext_alvo(const char *alvo);
// BLUE on a source of the current target: marks it, or clears the mark when it
// already is the marked one. 1 = marked now, 0 = cleared, -1 = refused (no URL).
int audioext_alternar(const char *url, const char *cabecalhos, const char *nome);
void audioext_limpar(void);
// From the audio menu while playing: use this source's audio now (`idioma` is
// the language it was listed under, tried first in that file). The webOS
// player reopens the converter at the current position (audioext_pedido_geracao).
void audioext_escolher(const char *url, const char *cabecalhos, const char *nome, const char *idioma);
// Other copies of the same file (other add-ons), best first, set right after
// audioext_escolher: when the chosen link fails the next one is tried before
// the session falls back to the file's own audio. A new choice drops them.
void audioext_reserva(const char *url, const char *cabecalhos);
// The debrid service's placeholder clip (a short warning video instead of the
// file: not cached, or not ready yet). The converter's failure text carries
// "(placeholder clip)". A version said to be cached is tried again after
// AUDIOEXT_ESPERA_MS, `n` times (set right after audioext_escolher; 0 for the
// others); meanwhile audioext_preparando() is 1 and the player says
// "preparing" once per try (audioext_preparando_consumir). Links still not
// ready after that come out of audioext_negada_consumir, to be marked not
// cached in the source list.
#define AUDIOEXT_TENTATIVAS 3
#define AUDIOEXT_ESPERA_MS 5000
#define AUDIOEXT_SONDA_PLACEHOLDER (-3)   /* audioext_sonda_resultado: the placeholder clip */
void audioext_tentativas(int n);
int audioext_motivo_placeholder(const char *motivo);
int audioext_preparando(void);
int audioext_preparando_consumir(int *tentativa, int *total);
int audioext_negada_consumir(char *url, size_t n);
// Tests: the clock in ms (NULL: the monotonic one).
void audioext_definir_relogio(uint64_t (*fn)(void));
// SERIES MEMORY (Settings > Remember for the next episodes): what identifies
// the chosen version across episodes. The add-on's binge group when it sends
// one ("the same release"), else the same add-on and version name (nome:
// the source row's name, simplified by audioext_nome_versao), else add-on +
// language + resolution + codec.
typedef struct {
  char grupo[128], addon[96], idioma[8], nome[96];
  int altura, codec;
} AudioExtIdent;
// A source row's name as the series memory compares it across episodes:
// lowercase letters and digits, one space between words; emoji (cache marks
// change between episodes), brackets and episode tags (S10E12, E12, 10x12)
// left out. "🇵🇱 ☁️ [TB⚡️] ST Store 1080p" -> "tb st store 1080p".
void audioext_nome_versao(const char *rotulo, char *dst, size_t n);
// Called with the identity right after audioext_escolher, in a series
// episode ("tt123:1:2"): the next episodes look for that version.
void audioext_lembrar_serie(const AudioExtIdent *id);
// The episode on screen ("tt123:1:3") belongs to the remembered series and
// has no audio chosen yet: the source list may pick the version (streams.c).
int audioext_serie_procura(const char *alvo, AudioExtIdent *id);
// streams.c found the remembered version in this episode's list: mark it with
// the remembered sync, as if chosen in the menu.
void audioext_serie_achou(const char *url, const char *cabecalhos, const char *nome);
void audioext_serie_esquecer(void);
// Preferences the UI copies here every frame (the player has no settings).
void audioext_preferencias(int soAudio, int serie);
int audioext_so_audio(void);
// From the audio menu: back to this file's own audio, reopened the same way.
void audioext_original(void);
// The chosen version stopped answering while it played (the converter gave
// up after its reconnections): this file's own audio again, the same way, but
// the series memory stays (a CDN hiccup is not a bad pick).
void audioext_sem_resposta(void);
unsigned audioext_pedido_geracao(void);
// From the audio menu while the other version plays: its audio stream
// `stream` (demux index in that file) instead of the language pick; the webOS
// player reopens with it. Forgotten with the mark. -1 = by language.
void audioext_escolher_faixa(int stream);
int audioext_faixa(void);
// The same choice before playing (hold-OK menu): no reopen, nothing pending.
void audioext_definir_faixa(int stream);
// TRACKS OF A VERSION for the hold-OK menu. The platform registers a blocking
// header reader (webOS: the converter's); audioext_sondar runs it on a thread
// and returns the request's generation; audioext_sonda_resultado says -2 still
// reading, -1 failed, else the count copied to `out`.
#define AUDIOEXT_FAIXAS_MAX 16
typedef struct { int stream, canais; char codec[24], idioma[16], titulo[64]; } AudioExtFaixa;
// SERIES MEMORY, the track (owner 09/11): the other version's track picked by
// hand (two English: the one chosen) goes with the remembered version to the
// next episodes. The player passes the hint on; the engine looks there for
// the track most like it (name, codec, channels, number; the same language).
void audioext_lembrar_faixa_serie(const AudioExtFaixa *f);
int audioext_faixa_dica(AudioExtFaixa *out);   // 1: this episode has one
// 1: this episode's offset came from the series memory (owner 11/11: an
// automatic match at start that fails keeps it without a message).
int audioext_offset_lembrado(void);
void audioext_definir_sonda(int (*fn)(const char *url, const char *cab, AudioExtFaixa *out, int max));
int audioext_sonda_disponivel(void);
unsigned audioext_sondar(const char *url, const char *cab);
int audioext_sonda_resultado(unsigned ger, AudioExtFaixa *out, int max);
int audioext_tem_marca(void);
const char *audioext_idioma(void);
// The source with this URL is the marked one (for the sheet's label).
int audioext_marcada(const char *url);
// The marked file on this very row: two add-ons can list one file under two
// names (owner 17/11, Torrentio and BestTorrents), and only the row picked
// shows the mark. Where the file itself matters, audioext_marcada.
int audioext_marcada_nome(const char *url, const char *nome);
// 1 when a mark exists for the current target and it is not the video itself.
int audioext_pedido(const char *urlVideo);
const char *audioext_url(void);
const char *audioext_cabecalhos(void);
const char *audioext_nome(void);
// Offset of the external audio in ms (positive = later), kept with the mark.
int audioext_offset_ms(void);
// Adds `ms` / sets it, bounded to +-60 min (an extended cut against the
// theatrical one); returns the new offset.
#define AUDIOEXT_OFFSET_MAX 3600000
// An automatic match that moves the offset by less than this keeps the
// current one: nobody notices it, and applying it means a new connection to
// the other file (owner's log 14/11: a 1 ms correction landed on a slow one).
#define AUDIOEXT_AUTO_MIN_MS 40
#define AUDIOEXT_DICA_MAX 600000   /* the lengths' hint: within 10 min only */
int audioext_offset_somar(int ms);
int audioext_offset_definir(int ms);
// Bumped by every offset change; the webOS player applies the latest one.
unsigned audioext_offset_geracao(void);
// 1 when the last offset change came from a check made while playing: the
// converter makes it at a quiet moment of the other version's audio.
int audioext_offset_suave(void);
// The playback position (s), so the series memory keeps the offset of the
// start of an episode: one found later (versions whose gaps differ) would put
// the next episode's start off.
void audioext_posicao(double segundos);
#define AUDIOEXT_SERIE_INICIO_S 600
// The webOS player says whether the session on screen plays the external
// audio (the player screen uses it for CH+/CH-), and why it could not.
void audioext_tocando_definir(int sim);
// The webOS player is opening the other version's audio (from the choice until
// it plays or fails): the player shows a loading card instead of black.
void audioext_carregando_definir(int sim);
int audioext_carregando(void);
// "Match automatically" (audio menu, on click only, while the other version
// plays): the webOS player takes the request, works ~30 s and reports; a found
// offset replaces the current one (applied live like CH+/CH-).
void audioext_dopasuj_pedir(void);
int audioext_dopasuj_pedido_consumir(void);
int audioext_dopasuj_ativo(void);
// The outcome (dts_engine.h DtsMatchInfo, in the app's words): ms the offset
// (found), ppm the other version's speed, como how it was found (1 near,
// 2 length hint, 3 +-60 s, 4 +-5 min, 5 speed, measured later on its own),
// dica_ms the offset the lengths suggest and pesado the wide search skipped
// for its download (not found).
// pozniej: found by a check made while playing (the sync checked all through,
// DtsMatchInfo.later): applied at a quiet moment, said nothing, and it does not
// become the series memory's offset.
typedef struct { int ms, ppm, como, dica_ms, pesado, nkand, kand_ms[3], kand_pct[3], automatico, pozniej; } AudioExtDopasuj;
void audioext_dopasuj_resultado(int ok, const AudioExtDopasuj *r);
// Once, for a toast: 1 found, -1 not trusted, 0 nothing.
int audioext_dopasuj_aviso_consumir(AudioExtDopasuj *out);
// The other version's speed from a match (ppm; 0 the same): the webOS player
// opens that version at it.
int audioext_velocidade(void);
void audioext_velocidade_auto(int ppm);   // the speed the converter took from the frame rates
// The offset the two lengths suggest (another cut), 0 when they are close or
// beyond the sync's reach (10 min).
int audioext_dica_ms(void);
// The best places the last match found (offset ms, score %), for the sync
// editor to try; count, kept until another version or title.
int audioext_kandydatos(int *ms, int *pct, int max);
// The playing file's own audio language (its chosen track), kept while another
// version plays: a version that also has it matches surely (the same language
// is compared, owner's log 07/11).
void audioext_idioma_principal_definir(const char *idioma);
// A jump the player is about to make (an intro skip, seconds): the webOS
// converter fetches its bytes ahead. Consumed once.
void audioext_salto_previsto(double segundos);
double audioext_salto_previsto_consumir(void);
// The converter reconnected a slow source instead of giving up (once, for a toast).
void audioext_reconexao_marcar(void);
int audioext_reconexao_consumir(void);
// The other version cannot keep up with the picture: 1 reopened once by
// itself, 2 still too slow after that, 3 it stopped answering (back to this
// file's own audio). Once each, for a toast.
void audioext_lento_marcar(int etapa);
int audioext_lento_consumir(void);
// Following another cut (Settings): an event of the webOS converter. tipo 1
// this file's own audio (a scene the other lacks), 2 the other resumed at
// `ms` (the offset is taken), 3 lost it. Once, for a toast.
void audioext_seguir_evento(int tipo, int ms);
int audioext_seguir_aviso_consumir(int *ms);
const char *audioext_idioma_principal(void);
// The step of a sync key: `base` (Settings) for a tap; held (repeticoes =
// key repeats so far) 100 ms, then 1 s, then 10 s.
int audioext_passo(int base, int repeticoes);
// "+690 ms", or with the (translated) formats for seconds ("%c%u.%03u s":
// sign, s, ms) and minutes ("%c%u min %u.%03u s": sign, min, s, ms).
void audioext_formatar_offset(int ms, char *dst, unsigned tam, const char *fmtSeg, const char *fmtMin);
int audioext_tocando(void);
void audioext_falhou(const char *motivo);
// Once: the last failure ("" none), for a toast on the player screen.
int audioext_falha_consumir(char *dst, unsigned tam);
// A failure clears the mark. The webOS player calls this when a playback
// starts: a start with the mark that never reached audioext_tocando_definir(1)
// (it hung, or the person backed out while it loaded) makes this next start
// clear the mark and play the file's own audio.
void audioext_inicio_reproducao(const char *urlVideo);
// Once: the last start fell back to the file's own audio (player toast).
int audioext_voltou_padrao_consumir(void);
// The webOS player, when the other version starts: its length minus this
// file's (seconds) and whether its speed differs. Over 10 s or another speed
// is a different cut: the player says once that the sync may drift.
void audioext_diferenca(int corteSeg, int velocidade);
int audioext_diferenca_consumir(int *corteSeg, int *velocidade);
#endif
