// Real application entry and three production sync motors, deterministic clock.
#define NV_REC_URL "http://fixture.invalid"
#include "../src/juntos.c"
#include <assert.h>

static int playing, prepared, pauses, starts, lobby;
static double position, now = 10000;
static char sent[1024];
int dws_enviar(DiscordWs *w, const char *j) { (void)w; (void)j; return 0; }
const char *i18n(const char *s) { return s; }
void player_toast_ex(const char *s, unsigned ms, const char *ic, int c) { (void)s; (void)ms; (void)ic; (void)c; }
int video_velocidade_suportada(void) { return 1; }
int video_velocidade_bloqueada(void) { return 0; }
int recomenda_ativo(void) { return 1; }
const char *recomenda_meu_id(void) { return "host"; }
const char *recomenda_vinculo_nome(void) { return "Host"; }
int player_aberto(void) { return 1; }
int player_pausado(void) { return !playing; }
int video_pronto(void) { return prepared; }
int player_juntos_preparado(void) { return prepared; }
int video_tocando(void) { return playing; }
double video_duracao(void) { return 7200; }
unsigned video_bufferando_ms(void) { return 0; }
double video_pos_amostra(Uint64 *t) { *t = now; return position; }
void player_juntos_pausar(int p) { playing = !p; if (p) pauses++; else starts++; }
void player_juntos_buscar(double p) { position = p; }
void video_taxa_sync(int v) { (void)v; }
void juntosui_sala_abriu(void) { lobby++; }
static int send(void *u, const char *json) { (void)u; snprintf(sent, sizeof sent, "%s", json); return 0; }

static void snapshot(JsMotor *m, int n, int seq, const char *state, double t0) {
  char json[4096], members[2048] = "";
  for (int i = 0; i < n; i++) {
    char member[512];
    snprintf(member, sizeof member, "%s{\"id\":\"%s\",\"nome\":\"P%d\",\"anf\":%s,\"on\":true,\"st\":\"carregando\",\"pct\":42,\"stats\":1,\"rtt\":24,\"offset\":-12,\"drift\":8,\"altura\":2160}", i ? "," : "", i == 0 ? "host" : i == 1 ? "guest1" : "guest2", i, i ? "false" : "true");
    strcat(members, member);
  }
  snprintf(json, sizeof json, "{\"t\":\"sala\",\"seq\":%d,\"s\":%.0f,\"rel\":{\"estado\":\"%s\",\"pos0\":0,\"t0\":%.0f},\"membros\":[%s]}", seq, now, state, t0, members);
  jsm_receber(m, json, now);
}

int main(void) {
  juntos_preparar("tt1", 0, 0, "Film", "", NULL, 0, 0);
  assert(fase == JF_PREPARADO && temMotor && juntos_precarregando());
  assert(motor.sala.n == 1 && !strcmp(motor.sala.m[0].st, "carregando"));
  assert(abrirPedido && !strcmp(abrirImdb, "tt1") && !escolhendo);
  assert(juntos_ao_alternar(0) == 1 && starts == 0);
  prepared = 1;
  motor.enviar = send; motor.conectado = 1;
  snapshot(&motor, 1, 1, "lobby", now);
  assert(pauses > 0 && !playing);
  JsMotor guests[2];
  for (int i = 0; i < 2; i++) {
    jsm_iniciar(&guests[i], i ? "guest2" : "guest1", "mac", "", 'A', &motor.p, send, NULL);
    guests[i].conectado = 1;
    snapshot(&motor, i + 2, 1, "lobby", now);
    snapshot(&guests[i], i + 2, 1, "lobby", now);
    assert(motor.sala.n == i + 2 && starts == 0);
    assert(jsm_cmd(&guests[i], "comecar", -1, NULL, now) == 0);
  }
  assert(motor.sala.m[2].pct == 42 && motor.sala.m[2].rtt == 24);
  assert(motor.sala.m[2].offset == -12 && motor.sala.m[2].altura == 2160);
  motor.rel.tem = 1; motor.rel.melhorRtt = 24; motor.rel.offset = 0;
  motor.altura = 2160;
  jsm_passo(&motor, now);
  assert(strstr(sent, "\"stats\":1") && strstr(sent, "\"altura\":2160"));
  jsm_receber(&motor, "{\"t\":\"membro\",\"id\":\"guest2\",\"on\":true,\"st\":\"pronto\",\"stats\":1,\"rtt\":30,\"offset\":-9}", now);
  assert(!strcmp(motor.sala.m[2].st, "pronto") && motor.sala.m[2].rtt == 30);
  assert(jsm_cmd(&motor, "play", -1, NULL, now) == 0);
  assert(juntos_ao_alternar(0) == 1 && lobby == 1 && starts == 0);
  assert(juntos_comecar() == 1 && strstr(sent, "comecar"));
  for (int i = 0; i < 3; i++) {
    JsMotor *m = i ? &guests[i - 1] : &motor;
    m->sync.latPlayMs = 0;
    snapshot(m, 3, 2, "tocando", now + 3000);
    assert(m->playArmado);
    jsm_passo(m, now + 2900);
    assert(starts == 0);
  }
  now += 3000;
  for (int i = 0; i < 3; i++) jsm_passo(i ? &guests[i - 1] : &motor, now);
  assert(starts == 3 && playing);
  assert(!juntos_precarregando());
  puts("PASS: lobby preload gate; host + two guests; live stats; lobby-only start; shared countdown");
}
