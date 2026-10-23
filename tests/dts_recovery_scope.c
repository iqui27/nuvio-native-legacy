/* Preserve non-webOS request and timestamp behavior when the opt-in is absent. */
#define NV_TEST_LEGACY_RECOVERY
#define main timestamp_fixture_main
#include "dts_timestamp_engine.c"
#undef main
int main(void) {
  av_log_set_level(AV_LOG_ERROR);
  boundary(0,0); boundary(4800,0); boundary(-4800,0);
  boundary(4801,1); boundary(24000,1); boundary(-4801,1);
  puts("PASS: without webOS recovery, timestamp gaps retain the original rejection");
  return 0;
}
