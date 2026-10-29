// Exercise actual app adapters and retry preference without a player or network.
#include "../src/juntos.c"
#include <assert.h>

static Stream sources[3];
static int preferred = -1, rate;
static unsigned char excluded[3];
static int sendResult, sends, denied;
static int sendCommand(void *u, const char *json) {
  (void)u;
  assert(strstr(json, "proximo") && strstr(json, "tt123:1:2"));
  sends++;
  return sendResult;
}
const char *i18n(const char *s) { return s; }
void player_toast_ex(const char *text, unsigned ms, const char *icon, int amber) {
  (void)text; (void)ms; (void)icon; (void)amber;
  denied++;
}
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
  // Room navigation must never fall through to a local episode reopen.
  temMotor = 0;
  assert(juntos_ao_proximo("tt123:1:2") == 0);
  temMotor = 1; motor.temSala = 0;
  assert(juntos_ao_proximo("tt123:1:2") == 0);
  motor.temSala = 1;
  motor.enviar = sendCommand;
  for (int hostOnly = 0; hostOnly <= 1; hostOnly++) {
    motor.sala.controleAnf = hostOnly;
    motor.conectado = 0;
    assert(juntos_ao_proximo("tt123:1:2") == 1);
    assert(sends == 0 && abrirPedido == 0);
  }
  motor.conectado = 1;
  assert(juntos_ao_proximo("tt123:1:2") == 1);
  assert(denied == 1 && sends == 0 && abrirPedido == 0);
  motor.sala.controleAnf = 0;
  sendResult = -1;
  assert(juntos_ao_proximo("tt123:1:2") == 1);
  assert(sends == 1 && abrirPedido == 0);
  sendResult = 0;
  assert(juntos_ao_proximo("tt123:1:2") == 1);
  assert(sends == 2 && abrirPedido == 0);
  puts("juntos_cliente: native timestamp, sync units, room retry exclusions, episode authority OK");
  return 0;
}
