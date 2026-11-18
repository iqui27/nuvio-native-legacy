#define _POSIX_C_SOURCE 200809L
// The audio-source state (audioext.c): marks per title, menu choice requests,
// offset, and the series memory that carries a choice to the next episodes.
#include "../src/audioext.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
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

static uint64_t agoraTeste;
static uint64_t relogioTeste(void) { return agoraTeste; }

int main(void) {
  AudioExtIdent id, got;
  // Version names as the series memory compares them (owner's log 16/11).
  { char v[96];
    audioext_nome_versao("\xf0\x9f\x87\xb5\xf0\x9f\x87\xb1 \xe2\x98\x81\xef\xb8\x8f [TB\xe2\x9a\xa1\xef\xb8\x8f] ST Store 1080p", v, sizeof v);
    assert(!strcmp(v, "tb st store 1080p"));
    audioext_nome_versao("\xf0\x9f\x87\xb5\xf0\x9f\x87\xb1 [TB\xe2\x9a\xa1\xef\xb8\x8f] ST Store 1080p", v, sizeof v);
    assert(!strcmp(v, "tb st store 1080p"));   // the cache mark gone: the same name
    audioext_nome_versao("\xf0\x9f\x87\xb5\xf0\x9f\x87\xb1 [TB\xe2\x9a\xa1\xef\xb8\x8f] MF|Midnight 1080p", v, sizeof v);
    assert(!strcmp(v, "tb mf midnight 1080p"));
    audioext_nome_versao("Chicago.Fire.S10E12.PL.1080p E12 10x12 S10", v, sizeof v);
    assert(!strcmp(v, "chicago fire pl 1080p"));
    audioext_nome_versao("Seventies x264 E", v, sizeof v);
    assert(!strcmp(v, "seventies x264 e"));
    audioext_nome_versao("abcdefghij klmnop", v, 12);
    assert(!strcmp(v, "abcdefghij"));
    audioext_nome_versao(NULL, v, sizeof v);
    assert(!v[0]); }
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
  // The first pick of an episode: remembered before the mark is set too
  // (the episode on screen), so the next episode finds it.
  audioext_preferencias(0, 1);
  audioext_limpar();
  audioext_alvo("tt4:2:5");
  audioext_lembrar_serie(&id);
  assert(audioext_serie_procura("tt4:2:6", NULL) && !audioext_serie_procura("tt4:2:5", NULL));
  // Versions whose gaps differ (owner's log 16/11, Chicago Fire): the next
  // episode starts with the offset of this one's start, not one set after its
  // first 10 minutes or found by a check made while playing.
  audioext_preferencias(0, 1);
  audioext_limpar();
  audioext_alvo("tt6:10:9");
  audioext_escolher("https://f/pl-e9.mkv", "", "PL E9", "pl");
  audioext_lembrar_serie(&id);
  audioext_posicao(70.0);
  audioext_offset_definir(640);                         // set near the start: remembered
  { AudioExtDopasuj la = { 0 };
    audioext_tocando_definir(1);
    audioext_posicao(400.0);
    la.ms = 1500; la.como = 1; la.automatico = 1; la.pozniej = 1;
    audioext_dopasuj_resultado(1, &la);                 // a check while playing: applied, not remembered
    assert(audioext_offset_ms() == 1500 && audioext_offset_suave());
    la.ms = 1530; audioext_dopasuj_resultado(1, &la);   // under 40 ms: kept
    assert(audioext_offset_ms() == 1500);
    audioext_posicao(2445.0);
    audioext_offset_definir(3500);                      // set late: not remembered either
    assert(!audioext_offset_suave()); }
  audioext_alvo("tt6:10:10");
  assert(audioext_serie_procura("tt6:10:10", NULL));
  audioext_serie_achou("https://f/pl-e10.mkv", "", "PL E10");
  assert(audioext_offset_ms() == 640 && !audioext_offset_suave());
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
  // From another source: the other copies of the same file are tried, in
  // order, before the failure is said; a new choice drops them.
  audioext_alvo("tt22");
  audioext_escolher("https://f/a.mkv", "", "PL", "pl");
  audioext_reserva("https://f/b.mkv", "H: 1");
  audioext_reserva("https://f/a.mkv", "");   // the chosen one itself: ignored
  audioext_reserva("https://f/c.mkv", "");
  { unsigned g = audioext_pedido_geracao();
    audioext_falhou("http 404");
    assert(audioext_tem_marca() && !strcmp(audioext_url(), "https://f/b.mkv") && !strcmp(audioext_cabecalhos(), "H: 1"));
    assert(!strcmp(audioext_nome(), "PL") && !strcmp(audioext_idioma(), "pl"));
    assert(audioext_pedido_geracao() == g + 1 && !audioext_falha_consumir(NULL, 0));
    audioext_falhou("http 403");
    assert(!strcmp(audioext_url(), "https://f/c.mkv"));
    audioext_falhou("timeout");
    assert(!audioext_tem_marca());
    { char f[32]; assert(audioext_falha_consumir(f, sizeof f) && !strcmp(f, "timeout")); } }
  // The debrid's placeholder clip on a version said to be cached: copies
  // first, then the chosen link again AUDIOEXT_ESPERA_MS later, n times; then
  // every link tried comes out as "not ready". Not cached: said at once.
  audioext_definir_relogio(relogioTeste);
  agoraTeste = 1000;
  audioext_escolher("https://p/a.mkv", "H: a", "PL", "pl");
  audioext_reserva("https://p/b.mkv", "");
  audioext_tentativas(2);
  { unsigned g = audioext_pedido_geracao(); int t = 0, tot = 0;
    char u[64];
    audioext_falhou("External audio: x (placeholder clip)");          // -> copy b
    assert(!strcmp(audioext_url(), "https://p/b.mkv") && audioext_pedido_geracao() == g + 1);
    audioext_falhou("External audio: x (placeholder clip)");          // -> wait, then a again
    assert(audioext_tem_marca() && audioext_preparando() && !strcmp(audioext_url(), "https://p/a.mkv"));
    assert(!strcmp(audioext_cabecalhos(), "H: a"));
    assert(audioext_preparando_consumir(&t, &tot) && t == 1 && tot == 2 && !audioext_preparando_consumir(NULL, NULL));
    assert(audioext_pedido_geracao() == g + 1);                        // not yet
    agoraTeste += AUDIOEXT_ESPERA_MS;
    assert(audioext_pedido_geracao() == g + 2 && !audioext_preparando());
    assert(!audioext_falha_consumir(NULL, 0) && !audioext_negada_consumir(u, sizeof u));
    audioext_falhou("External audio: x (placeholder clip)");          // a -> b
    audioext_falhou("External audio: x (placeholder clip)");          // try 2
    agoraTeste += AUDIOEXT_ESPERA_MS;
    assert(audioext_pedido_geracao() == g + 4);
    audioext_falhou("External audio: x (placeholder clip)");          // a -> b
    audioext_falhou("External audio: x (placeholder clip)");          // done
    assert(!audioext_tem_marca() && !audioext_preparando());
    { char f[96]; assert(audioext_falha_consumir(f, sizeof f) && audioext_motivo_placeholder(f)); }
    assert(audioext_negada_consumir(u, sizeof u) && !strcmp(u, "https://p/a.mkv"));
    assert(audioext_negada_consumir(u, sizeof u) && !strcmp(u, "https://p/b.mkv"));
    assert(!audioext_negada_consumir(u, sizeof u)); }
  // Not said to be cached (no tries): at once, marked not ready.
  audioext_escolher("https://q/a.mkv", "", "PL", "pl");
  audioext_falhou("External audio: x (placeholder clip)");
  assert(!audioext_tem_marca());
  { char u[64]; assert(audioext_negada_consumir(u, sizeof u) && !strcmp(u, "https://q/a.mkv")); }
  audioext_falha_consumir(NULL, 0);
  // Another failure (not the placeholder) on a cached version: no tries.
  audioext_escolher("https://r/a.mkv", "", "PL", "pl");
  audioext_tentativas(3);
  audioext_falhou("http 404");
  assert(!audioext_tem_marca() && !audioext_preparando());
  { char u[64]; assert(!audioext_negada_consumir(u, sizeof u)); }
  audioext_falha_consumir(NULL, 0);
  audioext_definir_relogio(NULL);
  audioext_escolher("https://g/a.mkv", "", "PL", "pl");
  audioext_reserva("https://g/b.mkv", "");
  audioext_escolher("https://g/other.mkv", "", "EN", "en");   // another version: no copies
  audioext_falhou("x");
  assert(!audioext_tem_marca());
  audioext_falha_consumir(NULL, 0);
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
    { int c, v;   // a few seconds apart: nothing said (the sync is followed quietly)
      assert(audioext_diferenca_consumir(&c, &v) == 0);
      audioext_diferenca(0, 0);
      assert(audioext_diferenca_consumir(&c, &v) == 0); }
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
    AudioExtDopasuj f = { 0, 0, 0, -100000, 0, 2, { -350, 41900 }, { 13, 12 }, 0, 0 };
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
    // An automatic match a few ms away keeps the offset (no new connection);
    // 40 ms or more, or a click, moves it.
    { AudioExtDopasuj au = { 0 };
      unsigned g3 = audioext_offset_geracao();
      au.ms = -39500 + 39; au.automatico = 1; au.como = 1;
      audioext_dopasuj_resultado(1, &au);
      assert(audioext_offset_ms() == -39500 && audioext_offset_geracao() == g3);
      au.ms = -39500 - 40;
      audioext_dopasuj_resultado(1, &au);
      assert(audioext_offset_ms() == -39540 && audioext_offset_geracao() == g3 + 1);
      audioext_dopasuj_pedir();
      r.ms = -39530; r.ppm = 250; r.como = 1; r.automatico = 0;
      audioext_dopasuj_resultado(1, &r);
      assert(audioext_offset_ms() == -39530);
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
    // A new mark starts at the same speed again.
    audioext_escolher("https://a/other.mkv", "", "Other", "pl");
    assert(audioext_velocidade() == 0 && audioext_offset_ms() == 0);
    { int km[3], kp[3]; assert(audioext_kandydatos(km, kp, 3) == 0); }
    audioext_limpar();
  }
  puts("audioext: marks, menu requests, offsets, series memory and matching passed");
  return 0;
}
