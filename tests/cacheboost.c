// F07: seek cache and volume boost state (src/cacheboost.c): option mapping,
// backend reports, one-shot notice per session, volume limits and steps, the
// passthrough cap and the dB math. A reporter thread runs against the UI
// reads so `SANITIZE=thread bash tests/cacheboost.sh` can see races.
#include "../src/cacheboost.h"
#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

static atomic_int parar;
static void *reporter(void *u) {
  int i = 0;
  (void)u;
  while (!atomic_load(&parar)) {
    cacheboost_cache_relato(CB_CACHE_ATIVO, 512, 512, i % 600);
    cacheboost_ganho_relato(i % 3 == 0 ? CB_GANHO_PCM : CB_GANHO_DESCONHECIDO);
    i++;
  }
  return NULL;
}

int main(void) {
  char b[64];
  CbCache c;
  int i, v;

  // Platform: this host build has no backend; captures may pretend.
  assert(!cacheboost_suportado());
  cacheboost_simular_suporte(1); assert(cacheboost_suportado());
  cacheboost_simular_suporte(-1); assert(!cacheboost_suportado());

  // Settings option -> MB.
  assert(cacheboost_cache_mb(0) == 0 && cacheboost_cache_mb(1) == 256);
  assert(cacheboost_cache_mb(2) == 512 && cacheboost_cache_mb(3) == 1024);
  assert(cacheboost_cache_mb(-1) == 0 && cacheboost_cache_mb(CB_CACHE_OPCOES) == 0);

  // Reports: limits only mean something while active; usage never exceeds
  // the limit; invalid states are ignored.
  cacheboost_sessao();
  c = cacheboost_cache();
  assert(c.estado == CB_CACHE_DESLIGADO && !c.limiteMb && !c.usadoMb);
  cacheboost_cache_texto(b, sizeof b); assert(!strcmp(b, "Desligado"));
  cacheboost_cache_relato(CB_CACHE_ATIVO, 1024, 512, 120);
  c = cacheboost_cache();
  assert(c.estado == CB_CACHE_ATIVO && c.pedidoMb == 1024 && c.limiteMb == 512 && c.usadoMb == 120);
  cacheboost_cache_texto(b, sizeof b); assert(!strcmp(b, "120 / 512 MB"));
  cacheboost_cache_relato(CB_CACHE_ATIVO, 512, 512, 9999);
  assert(cacheboost_cache().usadoMb == 512);
  cacheboost_cache_relato(CB_CACHE_ATIVO, 512, -5, -5);
  assert(cacheboost_cache().limiteMb == 0 && cacheboost_cache().usadoMb == 0);
  cacheboost_cache_relato(99, 1, 1, 1);
  assert(cacheboost_cache().estado == CB_CACHE_ATIVO);
  cacheboost_cache_relato(CB_CACHE_NAO_SE_APLICA, 512, 512, 10);
  c = cacheboost_cache();
  assert(c.estado == CB_CACHE_NAO_SE_APLICA && !c.limiteMb && !c.usadoMb);
  cacheboost_cache_texto(b, sizeof b); assert(!strcmp(b, "Não se aplica"));
  assert(!cacheboost_cache_aviso());   // not applicable is not a notice

  // ENOSPC mid-session: one notice, then silence; repeated reports do not
  // re-arm it; a new session does.
  cacheboost_cache_relato(CB_CACHE_ATIVO, 512, 512, 300);
  cacheboost_cache_relato(CB_CACHE_DISCO_CHEIO, 512, 0, 0);
  assert(!strcmp(cacheboost_cache_aviso(), "Cache de seek desligado: o disco encheu"));
  assert(!cacheboost_cache_aviso());
  cacheboost_cache_relato(CB_CACHE_DISCO_CHEIO, 512, 0, 0);
  cacheboost_cache_relato(CB_CACHE_POUCO_ESPACO, 512, 0, 0);
  assert(!cacheboost_cache_aviso());
  cacheboost_cache_texto(b, sizeof b); assert(!strcmp(b, "Pouco espaço"));
  cacheboost_sessao();
  assert(!cacheboost_cache_aviso());
  cacheboost_cache_relato(CB_CACHE_POUCO_ESPACO, 256, 0, 0);
  assert(!strcmp(cacheboost_cache_aviso(), "Cache de seek desligado: pouco espaço livre"));
  assert(!cacheboost_cache_aviso());
  cacheboost_cache_relato(CB_CACHE_FALHOU, 256, 0, 0);
  cacheboost_cache_texto(b, sizeof b); assert(!strcmp(b, "Falhou"));
  cacheboost_cache_texto(NULL, 0);

  // Volume: 100 at session start; 10% steps; 0..200; reset by a new session.
  cacheboost_sessao();
  assert(cacheboost_volume() == 100 && cacheboost_volume_teto() == 200 && !cacheboost_volume_acima());
  for (i = 0; i < 15; i++) v = cacheboost_volume_passo(1);
  assert(v == 200 && cacheboost_volume() == 200 && cacheboost_volume_acima());
  assert(cacheboost_volume_passo(1) == 200);
  for (i = 0; i < 25; i++) v = cacheboost_volume_passo(-1);
  assert(v == 0 && cacheboost_volume_passo(-1) == 0);
  assert(cacheboost_volume_passo(0) == 0);
  for (i = 0; i < 13; i++) cacheboost_volume_passo(1);
  assert(cacheboost_volume() == 130);
  cacheboost_sessao();
  assert(cacheboost_volume() == 100 && cacheboost_ganho_estado() == CB_GANHO_DESCONHECIDO);

  // Passthrough: the boost is unavailable; a boosted session is capped at
  // 100 and cannot go above it; PCM again lifts the cap (not the value).
  cacheboost_volume_passo(1); cacheboost_volume_passo(1);
  assert(cacheboost_volume() == 120);
  cacheboost_ganho_relato(CB_GANHO_PASSTHROUGH);
  assert(cacheboost_volume() == 100 && cacheboost_volume_teto() == 100);
  assert(cacheboost_volume_passo(1) == 100);
  assert(cacheboost_volume_passo(-1) == 90);
  cacheboost_ganho_relato(CB_GANHO_PCM);
  assert(cacheboost_volume_teto() == 200 && cacheboost_volume() == 90);
  cacheboost_ganho_relato(7);
  assert(cacheboost_ganho_estado() == CB_GANHO_PCM);

  // dB: 200% of amplitude is +6.02 dB, 50% is -6.02 dB, 0 is the floor.
  assert(fabs(cacheboost_volume_db(200) - 6.0206) < 1e-3);
  assert(fabs(cacheboost_volume_db(100)) < 1e-9);
  assert(fabs(cacheboost_volume_db(50) + 6.0206) < 1e-3);
  assert(cacheboost_volume_db(0) == -120.0 && cacheboost_volume_db(-3) == -120.0);

  // The non-Android hooks are no-ops (link + call).
  cacheboost_backend_cache(512);
  cacheboost_backend_ganho(150);

  // Concurrent reporter vs UI reads (TSan build).
  { pthread_t t;
    assert(!pthread_create(&t, NULL, reporter, NULL));
    for (i = 0; i < 200000; i++) {
      c = cacheboost_cache();
      assert(c.usadoMb <= c.limiteMb || c.estado != CB_CACHE_ATIVO);
      (void)cacheboost_volume_passo(i & 1 ? 1 : -1);
      (void)cacheboost_volume_teto();
      (void)cacheboost_cache_aviso();
    }
    atomic_store(&parar, 1);
    pthread_join(t, NULL); }
  puts("cacheboost: options, reports, one notice per session, volume 0-200, passthrough cap, dB ok");
  return 0;
}
