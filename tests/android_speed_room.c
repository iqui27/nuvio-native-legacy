// Reuse the fake JNI, compiling the entire real video_android.c backend.
#define main cacheboost_regression
#include "video_android_cacheboost.c"
#undef main
#include "juntossync.h"
#include "room_gate.h"

static int failures;
#define CHECK(c, message) do { if (!(c)) { puts("FAIL: " message); failures++; } } while (0)

int main(void) {
  // A supported bridge must allow personal speed even with encoded audio.
  mVelocidade = (jmethodID)(uintptr_t)15;
  Java_space_nuvio_nativelegacy_NvPlayer_nativeAudioEstado(&env, NULL, 2);
  CHECK(cacheboost_ganho_estado() == CB_GANHO_PASSTHROUGH, "passthrough fixture");
  video_velocidade(150);
  CHECK(!video_velocidade_bloqueada(), "PV-1 passthrough must not lock speed row");
  CHECK(video_velocidade_atual() == 150, "PV-1 requested speed must survive passthrough");
  CHECK(video_taxa_efetiva() == 150, "PV-1 pipeline must receive personal speed");

  // Room-only drift must never request 95/105 (or a subsequent 100 reset)
  // from the bridge that reselects PCM via ExoPlayer stop/prepare.
  video_velocidade(100);
  CHECK(nivelDaTv() == 'B', "PV-2 Android rooms must use seek/hold corrections");
  Java_space_nuvio_nativelegacy_NvPlayer_nativeAudioEstado(&env, NULL, 1);
  CHECK(nivelDaTv() == 'B', "PV-2 PCM must not enable room speed corrections");
  for (int sign = -1; sign <= 1; sign += 2) {
    JsSync sync;
    jss_zerar(&sync, nivelDaTv());
    for (int ms = 0; ms <= 600000; ms += 500) {
      double target = 20 + ms / 1000.0;
      JsAcao action = jss_decidir(&sync, ms, target + sign * 0.8, target, 1);
      if (action.tipo == JSA_VELOCIDADE) {
        CHECK(0, "PV-2 room correction reaches PCM stop/prepare speed path");
        break;
      }
      CHECK(video_taxa_efetiva() == 100, "PV-2 room-only playback stays at 1x");
    }
  }
  video_velocidade_recusada();
  CHECK(video_velocidade_atual() == 100 && !video_velocidade_suportada(),
        "measured speed refusal must still fall back to 1x");
  if (failures) return 1;
  // Also exercise opens, JNI speed delivery, restoration and refusal.
  velRecusada = 0;
  if (cacheboost_regression()) return 1;
  puts("android_speed_room: PASS (real Android speed gates; 10-minute room drift replay)");
  return 0;
}
