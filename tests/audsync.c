// Sincronia de legenda por AUDIO (F06): VAD + alinhamento com PCM sintetico,
// anel limitado, sessao inteira (audsync.c -> legsync.c -> autosync.c),
// cancelamentos, passthrough e plataforma sem PCM.
//   bash tests/audsync.sh     SANITIZE=1 / SANITIZE=thread
#include "audsync.h"
#include "audvad.h"
#include "legsync.h"
#include "autosync.h"
#include "rede.h"
#include "audsync_sint.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>

// --- stubs (legsyncui.c / legref.c / legenda.c sem SDL nem rede) -------------------
#include "legendasui.h"
void legendasui_definir_sync(const LegendasSyncProvider *p) { (void)p; }
const char *i18n(const char *s) { return s; }
void plrui_decimal(char *s) { (void)s; }
const char *player_id_canal(void) { return ""; }
unsigned rede_pedido_capacidades(void) { return 0; }   // sem coletor embutido: so audio
int rede_pedir(const RedePedido *p, RedeResposta *r) { (void)p; memset(r, 0, sizeof *r); r->erro = REDE_INDISPONIVEL; return 0; }
void rede_resposta_limpar(RedeResposta *r) { free(r->corpo); free(r->cabecalhos); r->corpo = r->cabecalhos = NULL; }
static char *corpoSrt;
char *rede_baixar_bin(const char *url, int segundos, long *n) {
  (void)url; (void)segundos;
  if (!corpoSrt) return NULL;
  if (n) *n = (long)strlen(corpoSrt);
  return strdup(corpoSrt);
}

// --- backend falso: registra o que o "Kotlin" recebeu ------------------------------------
static atomic_int tapLigado, chamadasTap;
static void ligarTap(int on) { atomic_store(&tapLigado, on); atomic_fetch_add(&chamadasTap, 1); }

static int casos;
#define CASO(nome) do { casos++; fprintf(stderr, "-- %s\n", nome); } while (0)

// --- 1. VAD ----------------------------------------------------------------------------
static AudVad vad;
static void vadDe(int tipo, double offset, double de, double ate) {
  int16_t b[960]; double t;
  audvad_iniciar(&vad);
  for (t = de; t < ate; t += 0.06) { sint_pcm(b, 960, t, offset, tipo); audvad_pcm(&vad, b, 960, (int64_t)llround(t * 1e6)); }
  audvad_fechar(&vad);
}

static void testeVad(void) {
  int i, j, casou = 0, total = 0;
  CASO("vad: speech bursts in noise -> one segment per line, edges within 100 ms");
  sint_timeline(0, 130, 7);
  vadDe(SINT_FALA | SINT_RUIDO, 0, 0, 120);
  for (i = 1; i < sintN; i++) {      // the first line comes before the floor exists
    if (sintCues[i].fim > 119) continue;
    total++;
    for (j = 0; j < vad.nseg; j++)
      if (fabs(vad.seg[j].inicio - sintCues[i].ini) < 0.10 && fabs(vad.seg[j].fim - sintCues[i].fim) < 0.10) { casou++; break; }
  }
  assert(total > 20 && casou == total);
  assert(vad.nseg <= total + 2);
  CASO("vad: noise only / music only / silence -> no speech");
  vadDe(SINT_RUIDO, 0, 0, 60); assert(vad.nseg == 0);
  vadDe(SINT_MUSICA | SINT_RUIDO, 0, 0, 60); assert(vad.nseg == 0);
  vadDe(0, 0, 0, 30); assert(vad.nseg == 0);
}

// --- 2. alinhamento -------------------------------------------------------------------------
static void testeAlinhamento(void) {
  static const double offs[] = { 2.5, -1.2, 12.0, -25.0, 0.0 };
  AudSeg cues[SINT_MAX];
  int i, k;
  sint_timeline(0, 420, 99);
  for (i = 0; i < sintN; i++) { cues[i].inicio = sintCues[i].ini; cues[i].fim = sintCues[i].fim; }
  for (k = 0; k < (int)(sizeof offs / sizeof *offs); k++) {
    AudAlign a;
    CASO("align: known offset recovered within 50 ms");
    vadDe(SINT_FALA | SINT_RUIDO, offs[k], 60, 360);
    a = audalign_estimar(vad.seg, vad.nseg, cues, sintN, 60, 360, 30000);
    fprintf(stderr, "   off %.1f -> %d ms score %.3f alt %.3f (%s)\n", offs[k], a.offsetMs, a.score, a.alternativo, audalign_motivo(a.motivo));
    assert(a.motivo == AUDALIGN_OK && abs(a.offsetMs - (int)lround(offs[k] * 1000)) <= 50);
  }
  CASO("align: music-only window rejected (no speech)");
  vadDe(SINT_MUSICA | SINT_RUIDO, 0, 60, 360);
  assert(audalign_estimar(vad.seg, vad.nseg, cues, sintN, 60, 360, 30000).motivo == AUDALIGN_SEM_FALA);
  CASO("align: speech that matches no subtitle activity -> low confidence/ambiguous");
  {
    AudSeg falsos[SINT_MAX]; int n = 0; AudAlign a;
    sint_timeline(0, 420, 4242);     // a different film
    vadDe(SINT_FALA | SINT_RUIDO, 0, 60, 360);
    for (i = 0; i < vad.nseg && n < SINT_MAX; i++) falsos[n++] = vad.seg[i];
    a = audalign_estimar(falsos, n, cues, SINT_MAX > 0 ? sintN : 0, 60, 360, 30000);
    (void)a;
    sint_timeline(0, 420, 99);
    for (i = 0; i < sintN; i++) { cues[i].inicio = sintCues[i].ini; cues[i].fim = sintCues[i].fim; }
    a = audalign_estimar(falsos, n, cues, sintN, 60, 360, 30000);
    fprintf(stderr, "   unrelated: %s score %.3f\n", audalign_motivo(a.motivo), a.score);
    assert(a.motivo == AUDALIGN_CONFIANCA || a.motivo == AUDALIGN_AMBIGUO);
  }
}

// --- sessao inteira ---------------------------------------------------------------------------
static unsigned agora = 1000;
static const char *URL = "https://cdn.example/filme.mkv";
static void passo(int sensivel) { agora += 50; legsync_passo(URL, 100.0, 60.0, sensivel, agora); }

// Feeds [de, ate) of media time like the tap: 60 ms chunks, waiting for room in
// the ring as the real-time pace would. Calls the UI step every simulated second.
static void tocar(double de, double ate, double offset, int tipo) {
  int16_t b[960]; double t; int k = 0;
  for (t = de; t < ate; t += 0.06, k++) {
    sint_pcm(b, 960, t, offset, tipo);
    while (audsync_teste_livre() < 960) usleep(100);
    audsync_pcm(b, 960, (int64_t)llround(t * 1e6));
    if (k % 16 == 0) passo(0);
  }
}

static LegSyncVisao esperar(LegSyncFase f) {
  LegSyncVisao v;
  int i;
  for (i = 0; i < 20000; i++) { passo(0); v = legsync_visao(0); if (v.fase == f) return v; usleep(1000); }
  fprintf(stderr, "esperava fase %d, ficou %d motivo %d\n", f, v.fase, v.motivo);
  assert(!"timeout");
  return v;
}

// New player session with the external subtitle (cues of seed 99) active.
static void abrirComExterna(void) {
  legsync_teste_auto(0);   // R4: este teste exercita as acoes manuais
  legsync_iniciar(URL);
  legsync_audio_habilitar(1);
  legsync_primaria_externa("ext://legenda.srt", "en", "OpenSubtitles");
  esperar(LEGSYNC_PRONTA);
}

static void testeSessao(void) {
  LegSyncVisao v;
  char txt[300];
  sint_timeline(0, 600, 99);
  corpoSrt = sint_srt();

  CASO("platform without PCM: no 'Por audio', the reason is shown");
  audsync_backend(ligarTap); audsync_formato(AUDSYNC_FMT_PCM);
  abrirComExterna();                 // waits for the download with audio available
  audsync_backend(NULL);
  v = legsync_visao(0);
  assert(!(v.acoes & LEGSYNC_ACAO_AUDIO) && v.motivoAudio == LEGSYNC_M_AUD_PLATAFORMA);
  legsync_texto(&v, txt, sizeof txt);
  assert(strstr(txt, "indispon\xc3\xadvel nesta plataforma"));
  assert(!legsync_acao(LEGSYNC_ACAO_AUDIO));

  CASO("toggle off (default): nothing about audio");
  legsync_audio_habilitar(0);
  v = legsync_visao(0);
  assert(!(v.acoes & LEGSYNC_ACAO_AUDIO) && v.motivoAudio == LEGSYNC_M_NENHUM);

  audsync_backend(ligarTap);
  CASO("passthrough: reported, never switched off");
  legsync_audio_habilitar(1);
  audsync_formato(AUDSYNC_FMT_BITSTREAM);
  v = legsync_visao(0);
  assert(!(v.acoes & LEGSYNC_ACAO_AUDIO) && v.motivoAudio == LEGSYNC_M_AUD_PASSTHROUGH);
  legsync_texto(&v, txt, sizeof txt);
  assert(strstr(txt, "passthrough"));
  assert(atomic_load(&chamadasTap) == 0);   // the tap was never even armed

  CASO("accepted: +2.5 s found by audio, applied once, undo keeps manual");
  audsync_formato(AUDSYNC_FMT_PCM);
  v = legsync_visao(0);
  assert(v.acoes & LEGSYNC_ACAO_AUDIO);
  assert(legsync_acao(LEGSYNC_ACAO_AUDIO));
  passo(0);
  assert(atomic_load(&tapLigado) == 1);
  v = legsync_visao(0); assert(v.fase == LEGSYNC_OUVINDO && v.acoes == LEGSYNC_ACAO_PARAR);
  tocar(100, 100 + AUDSYNC_ALVO_SEG + 1, 2.5, SINT_FALA | SINT_RUIDO);
  v = esperar(LEGSYNC_ACEITA);
  fprintf(stderr, "   accepted %d ms\n", v.offsetAutoMs);
  assert(v.audio && abs(v.offsetAutoMs - 2500) <= 100);
  assert(!(v.acoes & LEGSYNC_ACAO_OUTRA) && (v.acoes & LEGSYNC_ACAO_DESFAZER));
  assert(atomic_load(&tapLigado) == 0);     // window done: tap disarmed
  assert(legsync_offset_ms(300) == -300 + v.offsetAutoMs);   // manual +300 = later
  assert(legsync_acao(LEGSYNC_ACAO_DESFAZER));
  assert(legsync_offset_ms(300) == -300);
  assert(audsync_pcm((int16_t[4]){ 0 }, 4, 0) == 0);   // late PCM after the window: ignored

  CASO("accepted: -1.2 s, with a seek in the middle (window restarts)");
  assert(legsync_acao(LEGSYNC_ACAO_AUDIO));
  tocar(100, 140, -1.2, SINT_FALA | SINT_RUIDO);
  passo(1);                                   // seek
  v = legsync_visao(0); assert(v.fase == LEGSYNC_PAUSADA);
  passo(0);
  tocar(200, 212, -1.2, SINT_FALA | SINT_RUIDO);
  { int i; for (i = 0; i < 500 && audsync_teste_livre() < AUDSYNC_RING; i++) usleep(1000); }
  v = legsync_visao(0);
  assert(v.fase == LEGSYNC_OUVINDO && v.progresso <= 5);   // the 40 s before the seek do not count
  tocar(212, 200 + AUDSYNC_ALVO_SEG + 1, -1.2, SINT_FALA | SINT_RUIDO);
  v = esperar(LEGSYNC_ACEITA);
  assert(abs(v.offsetAutoMs + 1200) <= 100);

  CASO("music only: refused, nothing changed");
  legsync_acao(LEGSYNC_ACAO_DESFAZER);
  assert(legsync_acao(LEGSYNC_ACAO_AUDIO));
  tocar(100, 100 + AUDSYNC_ALVO_SEG + 1, 0, SINT_MUSICA | SINT_RUIDO);
  v = esperar(LEGSYNC_RECUSADA);
  assert(v.motivo == LEGSYNC_M_AUD_SEM_FALA && v.offsetAutoMs == 0 && legsync_offset_ms(0) == 0);
  legsync_texto(&v, txt, sizeof txt);
  assert(strstr(txt, "sem falas claras"));
  assert(v.acoes & LEGSYNC_ACAO_AUDIO);       // can listen again

  CASO("cancel: player close");
  assert(legsync_acao(LEGSYNC_ACAO_AUDIO)); tocar(100, 110, 0, SINT_FALA | SINT_RUIDO);
  legsync_encerrar(); passo(0);
  assert(audsync_status().fase == AUDSYNC_PARADO && atomic_load(&tapLigado) == 0);
  assert(audsync_pcm((int16_t[4]){ 0 }, 4, 0) == 0);

  CASO("cancel: source change mid-listen");
  abrirComExterna();
  assert(legsync_acao(LEGSYNC_ACAO_AUDIO)); tocar(100, 110, 0, SINT_FALA | SINT_RUIDO);
  agora += 50; legsync_passo("https://cdn.example/outra.mkv", 0, 60, 0, agora);
  assert(audsync_status().fase == AUDSYNC_PARADO && atomic_load(&tapLigado) == 0);
  URL = "https://cdn.example/outra.mkv";

  CASO("cancel: subtitle track change (embedded chosen)");
  legsync_primaria_externa("ext://legenda.srt", "en", "OpenSubtitles"); esperar(LEGSYNC_PRONTA);
  assert(legsync_acao(LEGSYNC_ACAO_AUDIO)); tocar(100, 110, 0, SINT_FALA | SINT_RUIDO);
  legsync_primaria_outra(1); passo(0);
  assert(audsync_status().fase == AUDSYNC_PARADO && atomic_load(&tapLigado) == 0);

  CASO("cancel: audio track change");
  legsync_primaria_externa("ext://legenda.srt", "en", "OpenSubtitles"); esperar(LEGSYNC_PRONTA);
  assert(legsync_acao(LEGSYNC_ACAO_AUDIO)); tocar(100, 110, 0, SINT_FALA | SINT_RUIDO);
  legsync_audio_trocou(); passo(0);
  assert(audsync_status().fase == AUDSYNC_PARADO && atomic_load(&tapLigado) == 0);
  v = legsync_visao(0); assert(v.fase == LEGSYNC_PRONTA);

  CASO("cancel: toggle switched off mid-listen");
  assert(legsync_acao(LEGSYNC_ACAO_AUDIO)); tocar(100, 110, 0, SINT_FALA | SINT_RUIDO);
  legsync_audio_habilitar(0); passo(0);
  assert(audsync_status().fase == AUDSYNC_PARADO && atomic_load(&tapLigado) == 0);
  legsync_audio_habilitar(1);

  CASO("passthrough starts mid-listen: unavailable, tap disarmed");
  assert(legsync_acao(LEGSYNC_ACAO_AUDIO)); tocar(100, 110, 0, SINT_FALA | SINT_RUIDO);
  audsync_formato(AUDSYNC_FMT_BITSTREAM);
  v = esperar(LEGSYNC_INDISPONIVEL);
  assert(v.motivo == LEGSYNC_M_AUD_PASSTHROUGH); passo(0);
  assert(atomic_load(&tapLigado) == 0);
  audsync_formato(AUDSYNC_FMT_PCM);

  CASO("Parar stops listening");
  assert(legsync_acao(LEGSYNC_ACAO_AUDIO)); tocar(100, 105, 0, SINT_FALA | SINT_RUIDO);
  assert(legsync_acao(LEGSYNC_ACAO_PARAR)); passo(0);
  assert(audsync_status().fase == AUDSYNC_PARADO && atomic_load(&tapLigado) == 0);
}

// --- ring bounds ----------------------------------------------------------------------------
static void testeAnel(void) {
  int16_t b[960];
  int aceitos = 0, i;
  LegendaDocumentoInfo info = { .sessao = 77, .flags = LEGENDA_DOC_COMPLETO };
  LegendaDocumento *d;
  CASO("ring: bounded, producer never waits, overflow drops and restarts the window");
  sint_timeline(0, 100, 5);
  d = legenda_documento_criar(corpoSrt, &info);
  assert(d);
  audsync_backend(ligarTap); audsync_formato(AUDSYNC_FMT_PCM);
  audsync_teste_travar(1);
  assert(audsync_pedir(77, d, 60, 30000));
  memset(b, 0, sizeof b);
  for (i = 0; i < 400; i++) {
    aceitos += audsync_pcm(b, 960, (int64_t)i * 60000);
    assert(audsync_teste_livre() >= 0 && audsync_teste_livre() <= AUDSYNC_RING);
  }
  assert(aceitos == AUDSYNC_RING / 960);           // exactly what fits
  assert(audsync_status().descartados == 400 - aceitos);
  assert(audsync_pcm(b, 4096 + 1, 0) == 0);         // larger than one delivery: refused
  audsync_teste_travar(0);
  for (i = 0; i < 2000 && audsync_teste_livre() < AUDSYNC_RING; i++) usleep(1000);
  assert(audsync_teste_livre() == AUDSYNC_RING);
  // next chunk carries the break: the window restarts
  { int r0 = audsync_status().reinicios;
    for (i = 0; i < 5; i++) { while (audsync_teste_livre() < 960) usleep(100); audsync_pcm(b, 960, 50000000 + (int64_t)i * 60000); }
    for (i = 0; i < 2000 && audsync_teste_livre() < AUDSYNC_RING; i++) usleep(1000);
    assert(audsync_status().reinicios > r0); }
  audsync_cancelar();
  legenda_documento_liberar(d);
}

int main(void) {
  testeVad();
  testeAlinhamento();
  testeSessao();
  testeAnel();
  legsync_destruir();
  fprintf(stderr, "audsync: %d casos ok\n", casos);
  free(corpoSrt);
  return 0;
}
