// Reuse the offline Xtream fixtures; no provider traffic or credentials.
#define main xtream_fixture_main
#include "xtream.c"
#include <unistd.h>
#include "../src/aovivo.c"
int epg_faixa(int i, time_t a, time_t b, EpgProg *p, int n) {
  (void)i; (void)a; (void)b; (void)p; (void)n; return 0;
}
int xtepg_faixa(const char *i, time_t a, time_t b, EpgProg *p, int n) {
  (void)i; (void)a; (void)b; (void)p; (void)n; return 0;
}
#undef main

// Baseline fallbacks express today's behavior when the feature is absent.
__attribute__((weak)) int xtream_conexoes_modo(void) { return 0; }
__attribute__((weak)) void xtream_definir_conexoes_modo(int m) { (void)m; }
__attribute__((weak)) int xtream_conexoes_limitadas(void) { return 0; }
__attribute__((weak)) int xtream_url_limitada(const char *u) { (void)u; return 0; }

int main(void) {
  assert(xtream_conexoes_modo && xtream_definir_conexoes_modo &&
         xtream_conexoes_limitadas && xtream_url_limitada);
  assert(xtream_conexoes_modo() == 0);
  assert(!xtream_conexoes_limitadas()); // Unknown limit is not a one-screen account.
  xtream_definir_servidor("http://127.0.0.1:19876");
  char synthetic[32];
  snprintf(synthetic, sizeof synthetic, "fixture-%ld", (long)getpid());
  xtream_definir_usuario(synthetic);
  xtream_definir_senha(synthetic);
  soTs = 1;
  XtreamConta c;
  assert(xtream_conta_ler(&c));
  assert(xtream_conexoes_limitadas()); // Auto, max_connections=1.
  assert(xtream_url_limitada("http://127.0.0.1:19876/live/fixture/fixture/1.ts"));
  assert(xtream_url_limitada("http://127.0.0.1:19876/movie/fixture/fixture/1.mkv"));
  assert(!xtream_url_limitada("http://127.0.0.1:198760/live/1.ts"));
  assert(!xtream_url_limitada("http://127.0.0.1:19876/player_api.php"));
  assert(!xtream_url_limitada("https://unrelated.invalid/live/1.ts"));
  xtream_definir_conexoes_modo(2);
  assert(!xtream_conexoes_limitadas()); // Off overrides Auto.
  assert(strstr(disco, "conexoesModo\t2\n"));
  xtream_definir_conexoes_modo(1);
  soTs = 0;
  assert(xtream_conta_ler(&c));
  assert(xtream_conexoes_limitadas()); // On overrides a larger allowance.
  xtream_definir_conexoes_modo(0);
  assert(xtream_conexoes_limitadas()); // Preserve the single-screen login decision.
  xtream_definir_servidor("http://127.0.0.1:19876");
  assert(xtream_conta_ler(&c));
  assert(!xtream_conexoes_limitadas()); // New account login with 2/5.
  const char *saved = respConta;
  respConta = "{\"user_info\":{\"auth\":1,\"active_cons\":3,\"max_connections\":3}}";
  assert(xtream_conta_ler(&c));
  assert(xtream_conexoes_limitadas());
  respConta = saved;
  xtream_esquecer();
  assert(xtream_conexoes_modo() == 0 && !xtream_conexoes_limitadas());
  AoVivoZap zap = {0}; int offset = 0;
  aovivo_zap_apertar(&zap, 1, 100);
  assert(!aovivo_zap_pronto(&zap, 150, &offset));
  aovivo_zap_apertar(&zap, 1, 180);
  assert(!aovivo_zap_pronto(&zap, 779, &offset));
  assert(aovivo_zap_pronto(&zap, 780, &offset));
  assert(aovivo_ordem(3, 0, offset) == 2); // A -> B -> C emits only C.
  assert(!aovivo_zap_pronto(&zap, 900, &offset));
  puts("PASS: Auto/On/Off, account persistence/reset, live/VOD boundary, real A/B/C debounce");
}
