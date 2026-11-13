#define _POSIX_C_SOURCE 200809L
// The audio-source state (audioext.c): marks per title, menu choice requests,
// offset, and the series memory that carries a choice to the next episodes.
#include "../src/audioext.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
// A slow header read: blocks until released or cancelled, counting how many
// run at once (the hold-OK menu fired once per focus move: unbounded threads).
static pthread_mutex_t lentoLock = PTHREAD_MUTEX_INITIALIZER;
static int lentoVivos, lentoMax, lentoChamadas, lentoSolto, lentoCancelado;
static int lentoSonda(const char *url, const char *cab, AudioExtFaixa *out, int max) {
  struct timespec ts = {0, 1000000};
  int cancelado;
  (void)cab; (void)max;
  pthread_mutex_lock(&lentoLock);
  lentoChamadas++; if (++lentoVivos > lentoMax) lentoMax = lentoVivos;
  lentoCancelado = 0;
  pthread_mutex_unlock(&lentoLock);
  for (;;) {
    pthread_mutex_lock(&lentoLock);
    cancelado = lentoCancelado; int solto = lentoSolto;
    pthread_mutex_unlock(&lentoLock);
    if (cancelado || solto) break;
    nanosleep(&ts, NULL);
  }
  pthread_mutex_lock(&lentoLock); lentoVivos--; pthread_mutex_unlock(&lentoLock);
  if (cancelado) return -1;
  memset(out, 0, sizeof *out); out->stream = strstr(url, "b39") ? 39 : 1;
  return 1;
}
static void lentoCancelar(void) { pthread_mutex_lock(&lentoLock); lentoCancelado = 1; pthread_mutex_unlock(&lentoLock); }
static void esperarOcioso(void) {
  struct timespec ts = {0, 1000000};
  for (int i = 0; i < 3000 && audioext_sonda_ocupada(); i++) nanosleep(&ts, NULL);
  assert(!audioext_sonda_ocupada());
}
static int fakeSonda(const char *url, const char *cab, AudioExtFaixa *out, int max) {
  (void)cab; if (strstr(url, "bad")) return -1;
  for (int i = 0; i < 2 && i < max; i++) { memset(&out[i], 0, sizeof out[i]); out[i].stream = i + 1; snprintf(out[i].idioma, sizeof out[i].idioma, "%s", i ? "eng" : "pol"); }
  return 2;
}
static int esperarSonda(unsigned g, AudioExtFaixa *f) {
  int n = -2; struct timespec ts = {0, 2000000};
  for (int i = 0; i < 1000 && (n = audioext_sonda_resultado(g, f, AUDIOEXT_FAIXAS_MAX)) == -2; i++) nanosleep(&ts, NULL);
  return n;
}

int main(void) {
  AudioExtIdent id, got;
  audioext_preferencias(0, 1);
  audioext_alvo("tt1:1:1");
  unsigned g = audioext_pedido_geracao();
  audioext_escolher("https://a/pl.mkv", "X: 1", "WEB-DL PL", "pl");
  assert(audioext_pedido_geracao() == g + 1 && audioext_pedido("https://a/remux.mkv"));
  assert(!audioext_pedido("https://a/pl.mkv"));          // the picture's own file is not "another"
  memset(&id, 0, sizeof id);
  snprintf(id.grupo, sizeof id.grupo, "aio|webdl-pl"); snprintf(id.idioma, sizeof id.idioma, "pl");
  audioext_lembrar_serie(&id);
  audioext_offset_somar(150);
  // The same episode never looks for itself.
  assert(!audioext_serie_procura("tt1:1:1", NULL));
  // Raw title id stamped before the episode list: same title, mark kept.
  audioext_alvo("tt1");
  assert(audioext_tem_marca());
  // Next episode: the mark goes, the memory stays and is looked for.
  audioext_alvo("tt1:1:2");
  assert(!audioext_tem_marca());
  assert(audioext_serie_procura("tt1:1:2", &got) && !strcmp(got.grupo, "aio|webdl-pl"));
  g = audioext_pedido_geracao();
  audioext_serie_achou("https://a/pl-e2.mkv", "X: 1", "WEB-DL PL E2");
  assert(audioext_pedido_geracao() == g + 1);
  assert(audioext_offset_ms() == 150 && !strcmp(audioext_idioma(), "pl"));
  assert(audioext_offset_lembrado());   // from the series memory (owner 11/11)
  assert(audioext_pedido("https://a/remux-e2.mkv"));
  assert(!audioext_serie_procura("tt1:1:2", NULL));      // already chosen here
  // No track remembered yet: no hint.
  assert(!audioext_faixa_dica(NULL));
  // Episode 2: the second English track chosen by hand (owner 09/11).
  { AudioExtFaixa f = { 2, 2, "ac3", "eng", "Commentary" }, h;
    audioext_escolher_faixa(f.stream);
    audioext_lembrar_faixa_serie(&f);
    assert(!audioext_faixa_dica(NULL));                   // chosen here: forced, not a hint
    // A new offset in episode 2 is what episode 3 gets, and the track with it.
    audioext_offset_somar(-50);
    audioext_alvo("tt1:1:3");
    assert(audioext_serie_procura("tt1:1:3", NULL));
    audioext_serie_achou("https://a/pl-e3.mkv", "", "E3");
    assert(audioext_offset_ms() == 100 && audioext_faixa() == -1);
    assert(audioext_faixa_dica(&h) && h.stream == 2 && !strcmp(h.titulo, "Commentary") && !strcmp(h.codec, "ac3"));
    // Another track picked in episode 3: no hint any more (forced).
    audioext_escolher_faixa(1);
    assert(!audioext_faixa_dica(NULL)); }
  // Another series does not inherit it.
  audioext_alvo("tt2:1:1");
  assert(!audioext_serie_procura("tt2:1:1", NULL));
  // Back to the series, then "This file's own audio": memory forgotten.
  audioext_alvo("tt1:1:4");
  assert(audioext_serie_procura("tt1:1:4", NULL));
  audioext_original();
  assert(!audioext_serie_procura("tt1:1:4", NULL));
  // Setting off: nothing remembered.
  audioext_alvo("tt3:1:1");
  audioext_escolher("https://b/pl.mkv", "", "PL", "pl");
  audioext_preferencias(1, 0);
  audioext_lembrar_serie(&id);
  audioext_alvo("tt3:1:2");
  assert(!audioext_serie_procura("tt3:1:2", NULL) && audioext_so_audio());
  // Movies (no episode) are never remembered as a series.
  audioext_preferencias(0, 1);
  audioext_alvo("tt9");
  audioext_escolher("https://c/pl.mkv", "", "PL", "pl");
  audioext_lembrar_serie(&id);
  assert(!audioext_serie_procura("tt9", NULL));
  // SAFETY NET: a start whose audio never played makes the next start of the
  // title use the file's own audio.
  audioext_alvo("tt20");
  assert(audioext_alternar("https://d/pl.mkv", "", "PL") == 1);
  audioext_inicio_reproducao("https://d/remux.mkv");          // first start: tries it
  assert(audioext_pedido("https://d/remux.mkv"));
  audioext_inicio_reproducao("https://d/remux.mkv");          // backed out while loading
  assert(!audioext_tem_marca() && audioext_voltou_padrao_consumir());
  assert(!audioext_voltou_padrao_consumir());
  // A start whose audio played keeps the mark for the next one.
  assert(audioext_alternar("https://d/pl.mkv", "", "PL") == 1);
  audioext_inicio_reproducao("https://d/remux.mkv");
  audioext_tocando_definir(1);
  audioext_tocando_definir(0);
  audioext_inicio_reproducao("https://d/remux.mkv");
  assert(audioext_pedido("https://d/remux.mkv") && !audioext_voltou_padrao_consumir());
  // Playing the marked source itself is not an attempt.
  audioext_tocando_definir(1); audioext_tocando_definir(0);
  audioext_inicio_reproducao("https://d/pl.mkv");
  audioext_inicio_reproducao("https://d/pl.mkv");
  assert(audioext_tem_marca());
  // A reported failure clears the mark at once.
  audioext_falhou("no audio");
  assert(!audioext_tem_marca());
  { char f[32]; assert(audioext_falha_consumir(f, sizeof f) && !strcmp(f, "no audio")); }
  // A track of the other version picked in the menu: reopens with it, and is
  // forgotten with the mark or a new version.
  audioext_alvo("tt21");
  assert(audioext_alternar("https://e/pl.mkv", "", "PL") == 1 && audioext_faixa() == -1);
  { unsigned g = audioext_pedido_geracao();
    audioext_escolher_faixa(2);
    assert(audioext_faixa() == 2 && audioext_pedido_geracao() == g + 1); }
  audioext_escolher("https://e/pl.mkv", "", "PL", "pl");   // same version: the track stays
  assert(audioext_faixa() == 2);
  audioext_escolher("https://e/other.mkv", "", "PL2", "pl");
  assert(audioext_faixa() == -1);
  audioext_escolher_faixa(1); audioext_limpar();
  assert(audioext_faixa() == -1);
  audioext_escolher_faixa(1);   // no mark: nothing to pick from
  assert(audioext_faixa() == -1);
  // Tracks of a version for the hold-OK menu: read on a thread, by request.
  { AudioExtFaixa f[AUDIOEXT_FAIXAS_MAX];
    assert(!audioext_sonda_disponivel());
    assert(esperarSonda(audioext_sondar("https://x/a.mkv", ""), f) == -1);   // no reader
    audioext_definir_sonda(fakeSonda);
    unsigned g = audioext_sondar("https://x/a.mkv", "");
    assert(esperarSonda(g, f) == 2 && f[0].stream == 1 && !strcmp(f[1].idioma, "eng"));
    assert(esperarSonda(audioext_sondar("https://x/bad.mkv", ""), f) == -1);
    assert(audioext_sonda_resultado(g, f, AUDIOEXT_FAIXAS_MAX) == -1); }  // an older request
  // The track picked before playing: set, nothing reopened, nothing pending.
  audioext_alvo("tt22");
  assert(audioext_alternar("https://e/v.mkv", "", "V") == 1);
  { unsigned g = audioext_pedido_geracao();
    audioext_definir_faixa(3);
    assert(audioext_faixa() == 3 && audioext_pedido_geracao() == g);
    audioext_inicio_reproducao("https://e/main.mkv");
    assert(audioext_tem_marca() && !audioext_voltou_padrao_consumir()); }
  // "Match automatically" (test 5): +-10 min offsets, the speed that goes with
  // a find, the lengths' hint, and a second look arriving on its own.
  {
    AudioExtDopasuj r = { 0 }, got;
    char v[48];
    audioext_alvo("tt9");
    audioext_escolher("https://a/cut.mkv", "", "Extended", "pl");
    assert(audioext_offset_definir(4000000) == AUDIOEXT_OFFSET_MAX && AUDIOEXT_OFFSET_MAX == 3600000);
    assert(audioext_offset_somar(-8000000) == -AUDIOEXT_OFFSET_MAX);
    audioext_offset_definir(0);
    audioext_diferenca(100, 0);
    assert(audioext_dica_ms() == -100000);
    audioext_diferenca(-4, 0);
    assert(audioext_dica_ms() == 0);
    audioext_tocando_definir(1);
    audioext_dopasuj_pedir();
    assert(audioext_dopasuj_ativo() && audioext_dopasuj_pedido_consumir());
    unsigned g = audioext_pedido_geracao();
    r.ms = -40000; r.como = 2;
    audioext_dopasuj_resultado(1, &r);
    assert(!audioext_dopasuj_ativo() && audioext_offset_ms() == -40000 && audioext_velocidade() == 0);
    assert(audioext_pedido_geracao() == g);            // same speed: applied live, no swap
    assert(audioext_dopasuj_aviso_consumir(&got) == 1 && got.ms == -40000 && got.como == 2);
    // The second look: another speed, the other version is reopened at it.
    r.ms = -39500; r.ppm = 250; r.como = 5;
    audioext_dopasuj_resultado(1, &r);
    assert(audioext_velocidade() == 250 && audioext_offset_ms() == -39500 && audioext_pedido_geracao() == g + 1);
    // A late not-asked result (not a second look) is ignored.
    r.como = 3; r.ms = 1;
    audioext_dopasuj_resultado(1, &r);
    assert(audioext_offset_ms() == -39500);
    // Not found, with the lengths' hint, for the toast.
    audioext_dopasuj_pedir();
    AudioExtDopasuj f = { 0, 0, 0, -100000, 0, 2, { -350, 41900 }, { 13, 12 }, 0 };
    audioext_dopasuj_resultado(0, &f);
    assert(audioext_dopasuj_aviso_consumir(&got) == -1 && got.dica_ms == -100000 && audioext_offset_ms() == -39500);
    { int km[3], kp[3];   // its candidates stay for the sync editor
      assert(audioext_kandydatos(km, kp, 3) == 2 && km[0] == -350 && kp[1] == 12); }
    audioext_diferenca(2998, 0);   // an extended cut 50 min longer: no hint
    assert(audioext_dica_ms() == 0);
    audioext_diferenca(-100, 0);
    // The follow's own match (no click) is taken; its events move the offset.
    { AudioExtDopasuj au = { 0 }; int ms;
      au.ms = 2500; au.como = 1; au.automatico = 1;
      audioext_dopasuj_resultado(1, &au);
      assert(audioext_offset_ms() == 2500 && audioext_dopasuj_aviso_consumir(&got) == 1);
      audioext_seguir_evento(1, 2500);
      assert(audioext_seguir_aviso_consumir(&ms) == 1 && audioext_offset_ms() == 2500);
      unsigned g2 = audioext_offset_geracao();
      audioext_seguir_evento(2, 27500);
      assert(audioext_seguir_aviso_consumir(&ms) == 2 && ms == 27500 && audioext_offset_ms() == 27500 &&
             audioext_offset_geracao() == g2 + 1);
      assert(audioext_seguir_aviso_consumir(&ms) == 0);
      audioext_offset_definir(-39500); }
    // A coming jump and a reconnection: each consumed once.
    audioext_salto_previsto(825.25);
    assert(audioext_salto_previsto_consumir() == 825.25 && audioext_salto_previsto_consumir() == 0);
    audioext_reconexao_marcar();
    assert(audioext_reconexao_consumir() == 1 && audioext_reconexao_consumir() == 0);
    // Held keys: 100 ms, 1 s, 10 s.
    assert(audioext_passo(50, 0) == 50 && audioext_passo(50, 3) == 100 && audioext_passo(50, 12) == 1000 && audioext_passo(50, 40) == 10000);
    audioext_formatar_offset(690, v, sizeof v, "%c%u.%03u s", "%c%u min %u.%03u s"); assert(!strcmp(v, "+690 ms"));
    audioext_formatar_offset(-40250, v, sizeof v, "%c%u.%03u s", "%c%u min %u.%03u s"); assert(!strcmp(v, "-40.250 s"));
    audioext_formatar_offset(100005, v, sizeof v, "%c%u,%03u s", "%c%u min %u,%03u s"); assert(!strcmp(v, "+1 min 40,005 s"));
    // A pre-play mark starts neutral; live swaps are covered below.
    audioext_tocando_definir(0); audioext_carregando_definir(0);
    audioext_escolher("https://a/other.mkv", "", "Other", "pl");
    assert(audioext_velocidade() == 0 && audioext_offset_ms() == 0);
    { int km[3], kp[3]; assert(audioext_kandydatos(km, kp, 3) == 0); }
    audioext_limpar();
  }
  { // BOUNDED PROBES (P1): 40 focus moves = one worker, at most one waiting;
    // only the newest request publishes; cancel/profile/title drop the rest.
    AudioExtFaixa f[AUDIOEXT_FAIXAS_MAX];
    char u[32];
    unsigned g = 0, g0;
    audioext_definir_sonda(lentoSonda);
    audioext_definir_sonda_cancelar(lentoCancelar);
    audioext_alvo("tt9");
    g0 = audioext_sondar("https://x/b0.mkv", "");
    for (int i = 1; i < 40; i++) { snprintf(u, sizeof u, "https://x/b%d.mkv", i); g = audioext_sondar(u, ""); }
    assert(audioext_sonda_resultado(g0, f, AUDIOEXT_FAIXAS_MAX) == -1);   // superseded
    pthread_mutex_lock(&lentoLock); lentoSolto = 1; pthread_mutex_unlock(&lentoLock);
    assert(esperarSonda(g, f) == 1 && f[0].stream == 39);
    esperarOcioso();
    assert(lentoMax == 1);                 // never two reads at once
    assert(lentoChamadas <= 2);            // the stale one (interrupted) + the newest
    // Cancel while reading: nothing published, the worker exits, no thread left.
    pthread_mutex_lock(&lentoLock); lentoSolto = 0; pthread_mutex_unlock(&lentoLock);
    g = audioext_sondar("https://x/b9.mkv", "");
    audioext_sonda_cancelar();
    assert(audioext_sonda_resultado(g, f, AUDIOEXT_FAIXAS_MAX) == -1);
    esperarOcioso();
    // Another title while reading drops it too.
    g = audioext_sondar("https://x/b9.mkv", "");
    audioext_alvo("tt10");
    assert(audioext_sonda_resultado(g, f, AUDIOEXT_FAIXAS_MAX) == -1);
    esperarOcioso();
    // PROFILE ISOLATION: another profile forgets the mark, the series memory
    // and the read in flight; the request bump makes a playing session retire it.
    audioext_perfil("owner:1");
    audioext_alvo("tt11:1:1");
    audioext_escolher("https://a/pl.mkv", "", "PL", "pl");
    { AudioExtIdent id2; memset(&id2, 0, sizeof id2); snprintf(id2.idioma, sizeof id2.idioma, "pl"); audioext_lembrar_serie(&id2); }
    g = audioext_sondar("https://x/b9.mkv", "");
    unsigned pg = audioext_pedido_geracao();
    audioext_perfil("owner:2");
    assert(!audioext_tem_marca() && audioext_pedido_geracao() != pg);
    assert(audioext_sonda_resultado(g, f, AUDIOEXT_FAIXAS_MAX) == -1);
    assert(!audioext_serie_procura("tt11:1:2", NULL));   // profile 2 never inherits profile 1's series
    esperarOcioso();
    audioext_definir_sonda_cancelar(NULL);
  }
  // Switching candidates in the player keeps the live timing/mode. A forced
  // track belongs to one donor only; the next donor starts with its language.
  audioext_limpar();
  audioext_alvo("tt50:1:1");
  audioext_preferencias(1, 1);
  audioext_escolher("fixture:donor-a", "", "A", "en");
  audioext_definir_faixa(7);
  audioext_tocando_definir(1);
  { AudioExtDopasuj r = { .ms = -2300, .ppm = 41708, .como = 5 };
    audioext_dopasuj_resultado(1, &r); }
  g = audioext_pedido_geracao();
  audioext_escolher("fixture:donor-b", "", "B", "pl");
  assert(audioext_pedido_geracao() == g + 1);
  assert(!strcmp(audioext_url(), "fixture:donor-b") && !strcmp(audioext_idioma(), "pl"));
  assert(audioext_faixa() == -1 && audioext_offset_ms() == -2300);
  assert(audioext_velocidade() == 41708 && audioext_so_audio());
  audioext_escolher_faixa(3);
  assert(audioext_faixa() == 3 && audioext_offset_ms() == -2300 && audioext_velocidade() == 41708);
  audioext_falhou("candidate refused");
  { char m[160];
    assert(audioext_falha_consumir(m, sizeof m) && !audioext_falha_consumir(m, sizeof m)); }
  audioext_inicio_reproducao("fixture:video");
  assert(!audioext_tem_marca() && !audioext_pedido("fixture:video"));
  puts("audioext: marks, menu requests, offsets, series memory and matching passed");
  return 0;
}
