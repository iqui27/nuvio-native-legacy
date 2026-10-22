// Exercise actual app adapters and retry preference without a player or network.
#include "../src/juntos.c"
#include <assert.h>

static Stream sources[3];
static int preferred = -1, rate;
static unsigned char excluded[3];
int stream_n(void) { return 3; }
const Stream *stream_item(int i) { return i >= 0 && i < 3 ? &sources[i] : NULL; }
int stream_automatico_excluida(int i) { return excluded[i]; }
void stream_preferir(int i) { preferred = i; }
double video_pos_amostra(Uint64 *t) { *t = 4294967400ULL; return 12.5; }
void video_taxa_sync(int r) { rate = r; }

int main(void) {
  double stamp;
  assert(pPos(NULL, &stamp) == 12.5 && stamp == 4294967400.0);
  pVel(NULL, 95); assert(rate == 950);
  pVel(NULL, 100); assert(rate == 1000);
  pVel(NULL, 105); assert(rate == 1050);
  strcpy(impSala.hash, "abcdef0123456789abcdef0123456789abcdef01");
  impSala.idx = 2; impSala.altura = 1080;
  strcpy(sources[0].infoHash, impSala.hash); sources[0].fileIdx = 2;
  strcpy(sources[1].infoHash, impSala.hash); sources[1].fileIdx = 2;
  sources[2].altura = 1080;
  // No room: preserve normal automatic selection.
  juntos_preferir_reserva(); assert(preferred == -1);
  temMotor = motor.temSala = 1;
  juntos_preferir_reserva(); assert(preferred == 0);
  excluded[0] = 1;
  juntos_preferir_reserva(); assert(preferred == 1);
  excluded[1] = 1;
  salaDurConferida = 1;
  juntos_preferir_reserva(); assert(preferred == 2 && !salaDurConferida);
  excluded[2] = 1;
  juntos_preferir_reserva(); assert(preferred == -1);
  puts("juntos_cliente: native timestamp, sync units, room retry exclusions OK");
  return 0;
}
