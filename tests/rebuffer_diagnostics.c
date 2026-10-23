// Exercise BUFFERING transitions in the real backend, including exclusions.
#define main cacheboost_tests_main
#include "video_android_cacheboost.c"
#undef main
int main(void) {
  redeKbps = 52800;
  relogio = 10000; bufferando = 1; prontoLoad = 0; tocandoDesde = 0;
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(&env, 0, EV_BUFFER, 0, 0);
  assert(redeFit.rebufs == 0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeStats(&env, 0, 0, 1200, 50000, 1, 1, 0, 0, NULL, 50000, 80, 1);
  for (int i = 1; i <= 10; i++)
    Java_space_nuvio_nativelegacy_NvPlayer_nativeStats(&env, 0, i * 5000000LL, 1200, 50000, 1, 1, 0, 0, NULL, 50000, 80, 1);
  prontoLoad = tocandoDesde = 1; acaoMs = relogio;
  bufferando = 0;
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(&env, 0, EV_BUFFER, 0, 0);
  assert(redeFit.rebufs == 0);
  relogio += 5000; bufferando = 0;
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(&env, 0, EV_BUFFER, 0, 0);
  assert(redeFit.rebufs == 1);
}
