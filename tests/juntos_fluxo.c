// Real application entry and three production sync motors, deterministic clock.
#define NV_REC_URL "http://fixture.invalid"
#include "../src/juntos.c"
#include <assert.h>

static int playing, prepared, pauses, starts, lobby, failedVideo, roomErrors;
static int selectedStream = 0, socketPending;
#ifdef JUNTOS_AUTH
static int authAvailable, authCalls;
#endif
int video_falhou(void) { return failedVideo; }
int player_tem_video(void) { return prepared; }
int video_velocidade_atual(void) { return 100; }
void video_velocidade(int v) { (void)v; }
void video_estatisticas(VideoStats *s) { memset(s, 0, sizeof *s); }
int dws_estado(DiscordWs *w) { (void)w; return DWS_CONECTANDO; }
int dws_codigo_fechamento(DiscordWs *w) { (void)w; return 0; }
char *dws_receber(DiscordWs *w) { (void)w; return NULL; }
void juntosui_evento(const JsEvento *e) { (void)e; }
void juntosui_fonte_diferente(int v, int pal) { (void)v; (void)pal; }
static Stream testStream;
const Stream *stream_item(int i) { return i == 0 ? &testStream : NULL; }
int stream_atual(void) { return prepared ? selectedStream : -1; }
static double position, now = 10000;
double video_pos(void) { return position; }
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
int player_juntos_preparado(void) { return prepared && !failedVideo; }
int video_tocando(void) { return playing; }
double video_duracao(void) { return 7200; }
unsigned video_bufferando_ms(void) { return 0; }
double video_pos_amostra(Uint64 *t) { *t = now; return position; }
void player_juntos_pausar(int p) { playing = !p; if (p) pauses++; else starts++; }
void player_juntos_buscar(double p) { position = p; }
void video_taxa_sync(int v) { (void)v; }
void juntosui_sala_abriu(void) { lobby++; }
// Network/socket edges of the room replies: never reached (worker held, no ticket).
void juntosui_sala_fechou(void) {}
static char lastRoomError[32];
void juntosui_erro(const char *k) { snprintf(lastRoomError, sizeof lastRoomError, "%s", k); roomErrors++; }
void juntosui_convite(const JuntosConvite *c) { (void)c; }
void atividade_junto(const char *a, const char *b, const char *c, const char *d, int t, int e, int s) { (void)a; (void)b; (void)c; (void)d; (void)t; (void)e; (void)s; }
DiscordWs *dws_abrir(const char *u) { (void)u; return socketPending ? (DiscordWs *)&socketPending : NULL; }
void dws_fechar(DiscordWs *w) { (void)w; }
int perfil_infantil_ativo(void) { return 0; }
const char *rede_ultimo_erro(void) { return "fixture"; }
int recomenda_cabecalhos(const char **c, char *a, size_t na, char *v, size_t nv, char *p, size_t np) { (void)c; (void)a; (void)na; (void)v; (void)nv; (void)p; (void)np;
#ifdef JUNTOS_AUTH
  authCalls++;
  if (!authAvailable) return 0;
  assert(na >= 3200); // The social JWT can exceed the old 400-byte buffer.
  memset(a, 'x', 900); a[900] = 0; c[0] = a; c[1] = NULL;
  return 1;
#endif
  return 0; }
char *rede_postar_st(const char *u, int s, const char *const *c, const char *b, int *st) { (void)u; (void)s; (void)c; (void)b; (void)st;
#ifdef JUNTOS_AUTH
  assert(strlen(c[0]) == 900);
  *st = 200;
  return strdup("{\"id\":\"fixture-room\",\"codigo\":\"K7M2\"}");
#endif
  return NULL; }
char *rede_baixar_st(const char *u, int s, const char *const *c, int *st) { (void)u; (void)s; (void)c; (void)st; return NULL; }
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
#ifdef JUNTOS_TCL
  // Previous ExoPlayer failure survives exit; no source belongs to this player yet.
  failedVideo = 1;
  juntos_passo(10001);
  assert(fase == JF_PREPARADO && abrirPedido && temMotor);
  char imdb[24], title[160], poster[512]; int season, episode;
  assert(juntos_pediu_abrir(imdb, sizeof imdb, &season, &episode,
                            title, sizeof title, poster, sizeof poster));
  juntos_passo(10002); // player opened, still waiting for the new source
  assert(fase == JF_PREPARADO && temMotor);
  failedVideo = 0;
#endif
  prepared = 1;
#ifdef JUNTOS_TCL
  // Duration + paused readiness must now reach room creation (worker held).
  fioVivo = 1;
  snprintf(testStream.rotulo, sizeof testStream.rotulo, "Android fixture");
  juntos_passo(10003);
  assert(fase == JF_CRIANDO && nFila == 1 && fila[0].tipo == OP_CRIAR);
  free(fila[0].corpo); nFila = 0; fioVivo = 0;
#endif
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
  // Invite while POST /v1/sala is in flight: queued, then sent once the room id arrives.
  assert(!fioVivo); fioVivo = 1;   // keep requests in the queue, no worker thread
  fase = JF_CRIANDO; salaId[0] = 0; prep.n = 0;
  const char *friend1 = "friend1";
  juntos_convidar(&friend1, 1);
  assert(prep.n == 1 && nFila == 0);
  char reply[] = "{\"id\":\"0123456789abcdef0123456789abcdef\",\"codigo\":\"K7M2\"}";
  Resp created = { .tipo = OP_CRIAR, .status = 200, .txt = reply };
  resposta(&created);
  assert(nFila == 1 && fila[0].tipo == OP_CONVIDAR && strstr(fila[0].corpo, "\"friend1\""));
#ifdef JUNTOS_TCL
  fase = JF_PREPARADO; abrirPedido = 0; failedVideo = 1;
  juntos_passo(10004);
  assert(fase == JF_OCIOSO && !temMotor); // a failure of our own source still exits
#endif
#ifdef JUNTOS_62
  // Saved-source playback deliberately has no selected list item (app.c).
  failedVideo = 0; prepared = 1; selectedStream = -1; playing = 0;
  fioVivo = 1;
  for (int i = 0; i < nFila; i++) free(fila[i].corpo);
  nFila = 0;
  juntos_preparar("tt62", 0, 0, "Saved film", "", NULL, 0, 0);
  abrirPedido = 0; // app consumed the request: open=0, ready=1
  juntos_passo(20000);
  assert(fase == JF_CRIANDO && nFila == 1 && fila[0].tipo == OP_CRIAR);
  assert(strstr(fila[0].corpo, "7200000"));
  free(fila[0].corpo); nFila = 0;
  int errorsBefore = roomErrors, startsBefore = starts;
  Resp denied = { .tipo = OP_CRIAR, .status = 401 };
  resposta(&denied);
  assert(roomErrors == errorsBefore + 1 && temMotor && juntos_precarregando());
  juntos_passo(25000);
  assert(nFila == 0); // no repeated failing requests
  assert(juntos_comecar() == 1);
  assert(playing && starts == startsBefore + 1 && !juntos_ativo());
  // HTTP success must retain the host before any WebSocket snapshot arrives.
  for (socketPending = 0; socketPending <= 1; socketPending++) {
    playing = 0;
    juntos_preparar("tt62", 0, 0, "Saved film", "", NULL, 0, 0);
    abrirPedido = 0;
    juntos_passo(30000 + socketPending);
    assert(fase == JF_CRIANDO && nFila == 1 && fila[0].tipo == OP_CRIAR);
    free(fila[0].corpo); nFila = 0;
    char success[] = "{\"id\":\"0123456789abcdef0123456789abcdef\",\"codigo\":\"K7M2\",\"bilhete\":\"fixture\",\"ws\":\"ws://fixture.invalid\"}";
    Resp accepted = { .tipo = OP_CRIAR, .status = 200, .txt = success };
    resposta(&accepted);
    assert(fase == JF_CONECTANDO && !motor.temSala && !motor.conectado);
    assert(juntos_sou_anfitriao() && motor.sala.rel.estado == JSE_LOBBY);
    unsigned pendingGen = gen;
    assert(juntos_comecar() == 1 && playing && gen != pendingGen);
    assert(roomErrors == errorsBefore + 2 + socketPending);
    assert(!juntos_ativo() && !ws);
    for (int i = 0; i < nFila; i++) free(fila[i].corpo);
    nFila = 0;
  }
  puts("PASS: ready=1 open=0 without stream index; HTTP 401 then solo Start; HTTP 200 with failed/pending socket then solo Start");
#endif
#ifdef JUNTOS_AUTH
  zerar();
  prepared = 1; failedVideo = 0; playing = 0; fioVivo = 1;
  for (int i = 0; i < nFila; i++) free(fila[i].corpo);
  nFila = 0;
  juntos_preparar("ttauth", 0, 0, "Auth film", "", NULL, 0, 0);
  abrirPedido = 0;
  juntos_passo(40000);
  free(fila[0].corpo); nFila = 0;
  Resp missing = { .tipo = OP_CRIAR, .status = 401 };
  resposta(&missing);
  assert(!strcmp(lastRoomError, "auth"));
  juntos_passo(40001);
  assert(motor.sala.m[0].pct == 0 && strcmp(motor.sala.m[0].st, "carregando"));
  juntos_passo(51000);
  assert(fase == JF_CRIANDO && nFila == 1);
  // Exercise the real worker with a JWT larger than 400 bytes.
  authAvailable = 1;
  SDL_Thread *worker = SDL_CreateThread(fioLaco, "auth-fixture", NULL);
  assert(worker);
  SDL_DetachThread(worker);
  for (int i = 0; i < 100; i++) {
    SDL_LockMutex(mtx);
    int done = nCaixa > 0;
    SDL_UnlockMutex(mtx);
    if (done) break;
    SDL_Delay(10);
  }
  SDL_LockMutex(mtx);
  assert(nCaixa == 1 && caixa[0].status == 200);
  Resp recovered = caixa[0]; nCaixa = 0;
  SDL_UnlockMutex(mtx);
  resposta(&recovered); free(recovered.txt);
  assert(authCalls > 0 && !salaFalhou && !strcmp(juntos_codigo(), "K7M2"));
  puts("PASS: missing auth shown, loading cleared, room retried, social-sized JWT creates room");
#endif
  puts("PASS: lobby preload gate; host + two guests; live stats; lobby-only start; shared countdown; invite during room creation");
}
