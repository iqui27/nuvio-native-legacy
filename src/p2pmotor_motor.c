// Fronteira com o nuvio-engine real (API C, NUVIO_ENGINE_API_VERSION 4: o 0.1.3
// mais tools/p2p-motor/janela-334.patch, janela de streaming do #334).
// So compila o motor com -DNV_P2P_MOTOR e as bibliotecas ligadas
// (tools/p2p-motor/). Sem isso, p2pmotor_ops_reais() devolve NULL e o resto do
// app ve "sem motor neste pacote".
//
// Android e LG: o motor e ESTATICO dentro do binario. .tpk (Tizen 6+):
// -DNV_P2P_MOTOR_DLOPEN. A libnuvio_engine.so vai no pacote ao lado da
// libnuvio.so e e aberta por dlopen NA PRIMEIRA PERGUNTA (nao no arranque).
// Ligar direto (NEEDED) quebraria a auto-atualizacao do .tpk: uma
// libnuvio.so nova encenada em data/ num pacote antigo, sem a .so do motor,
// nao carregaria e o app nao abriria. Com dlopen, sem a .so = "sem motor".
// dladdr/Dl_info (carregar, abaixo) so existem com _GNU_SOURCE no glibc. No .tpk
// o -include src/tpk.h puxa os headers do sistema antes deste arquivo, entao o
// define aqui chega tarde: tools/tpk.sh passa -D_GNU_SOURCE a ESTE arquivo.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "p2pmotor.h"
#include <stddef.h>

static const char *erroCarga = "";
const char *p2pmotor_motor_erro(void) { return erroCarga; }

#ifndef NV_P2P_MOTOR
const P2pmOps *p2pmotor_ops_reais(void) { return NULL; }
#else

#include "nuvio_engine/nuvio_engine.h"
#include <stdio.h>
#include <string.h>

#ifdef NV_P2P_MOTOR_DLOPEN
#include <dlfcn.h>
#include <pthread.h>
#include <stdlib.h>
static struct {
  __typeof__(nuvio_engine_api_version) *api_version;
  __typeof__(nuvio_engine_version_string) *version_string;
  __typeof__(nuvio_engine_protocol_backend_version) *protocol_backend_version;
  __typeof__(nuvio_engine_status_message) *status_message;
  __typeof__(nuvio_engine_config_init_sized) *config_init_sized;
  __typeof__(nuvio_engine_torrent_request_init_sized) *torrent_request_init_sized;
  __typeof__(nuvio_engine_event_init_sized) *event_init_sized;
  __typeof__(nuvio_engine_file_init_sized) *file_init_sized;
  __typeof__(nuvio_engine_stream_request_init_sized) *stream_request_init_sized;
  __typeof__(nuvio_engine_stats_init_sized) *stats_init_sized;
  __typeof__(nuvio_engine_create) *create;
  __typeof__(nuvio_engine_destroy) *destroy;
  __typeof__(nuvio_engine_add_torrent) *add_torrent;
  __typeof__(nuvio_engine_poll_event) *poll_event;
  __typeof__(nuvio_engine_get_file_count) *get_file_count;
  __typeof__(nuvio_engine_get_file) *get_file;
  __typeof__(nuvio_engine_prepare_stream) *prepare_stream;
  __typeof__(nuvio_engine_remove_torrent) *remove_torrent;
  __typeof__(nuvio_engine_stop_stream) *stop_stream;
  __typeof__(nuvio_engine_get_stats) *get_stats;
} F;
static int carregado;
static pthread_once_t umaVez = PTHREAD_ONCE_INIT;
static void *abrir(const char *pasta) {
  char p[700];
  if (!pasta || !pasta[0]) return NULL;
  snprintf(p, sizeof p, "%s/libnuvio_engine.so", pasta);
  return dlopen(p, RTLD_NOW | RTLD_LOCAL);
}
static void carregar(void) {
  Dl_info di;
  void *h = NULL;
  char pasta[600];
  // 1) ao lado desta libnuvio.so (o caso normal: <pacote>/lib)
  if (dladdr((void *)carregar, &di) && di.dli_fname) {
    char *b;
    snprintf(pasta, sizeof pasta, "%s", di.dli_fname);
    b = strrchr(pasta, '/');
    if (b) { *b = 0; h = abrir(pasta); }
  }
  // 2) libnuvio.so encenada em data/: <pacote>/res/art/../../lib
  if (!h && getenv("NUVIO_TPK_ARTE")) {
    snprintf(pasta, sizeof pasta, "%s/../../lib", getenv("NUVIO_TPK_ARTE"));
    h = abrir(pasta);
  }
  if (!h) { erroCarga = "libnuvio_engine.so missing from the package"; printf("[p2p-motor] %s\n", erroCarga); return; }
#define PEGA(c) if (!(*(void **)&F.c = dlsym(h, "nuvio_engine_" #c))) { erroCarga = "libnuvio_engine.so lacks " #c; printf("[p2p-motor] %s\n", erroCarga); return; }
  PEGA(api_version) PEGA(version_string) PEGA(protocol_backend_version) PEGA(status_message)
  PEGA(config_init_sized) PEGA(torrent_request_init_sized) PEGA(event_init_sized)
  PEGA(file_init_sized) PEGA(stream_request_init_sized) PEGA(stats_init_sized)
  PEGA(create) PEGA(destroy) PEGA(add_torrent) PEGA(poll_event) PEGA(get_file_count)
  PEGA(get_file) PEGA(prepare_stream) PEGA(remove_torrent) PEGA(stop_stream) PEGA(get_stats)
#undef PEGA
  if (F.api_version() != NUVIO_ENGINE_API_VERSION) {
    erroCarga = "libnuvio_engine.so has another API version";
    printf("[p2p-motor] %s (%u)\n", erroCarga, F.api_version());
    return;
  }
  carregado = 1;
  printf("[p2p-motor] libnuvio_engine.so carregada de %s\n", pasta);
}
// Daqui para baixo o mesmo codigo do motor estatico, pelos ponteiros.
#define nuvio_engine_api_version (*F.api_version)
#define nuvio_engine_version_string (*F.version_string)
#define nuvio_engine_protocol_backend_version (*F.protocol_backend_version)
#define nuvio_engine_status_message (*F.status_message)
#define nuvio_engine_config_init_sized (*F.config_init_sized)
#define nuvio_engine_torrent_request_init_sized (*F.torrent_request_init_sized)
#define nuvio_engine_event_init_sized (*F.event_init_sized)
#define nuvio_engine_file_init_sized (*F.file_init_sized)
#define nuvio_engine_stream_request_init_sized (*F.stream_request_init_sized)
#define nuvio_engine_stats_init_sized (*F.stats_init_sized)
#define nuvio_engine_create (*F.create)
#define nuvio_engine_destroy (*F.destroy)
#define nuvio_engine_add_torrent (*F.add_torrent)
#define nuvio_engine_poll_event (*F.poll_event)
#define nuvio_engine_get_file_count (*F.get_file_count)
#define nuvio_engine_get_file (*F.get_file)
#define nuvio_engine_prepare_stream (*F.prepare_stream)
#define nuvio_engine_remove_torrent (*F.remove_torrent)
#define nuvio_engine_stop_stream (*F.stop_stream)
#define nuvio_engine_get_stats (*F.get_stats)
#endif

static const char *versao(void) {
  static char v[64];
  if (!v[0]) snprintf(v, sizeof v, "%s / libtorrent %s", nuvio_engine_version_string(),
                      nuvio_engine_protocol_backend_version());
  return v;
}

static int criar(const P2pmConfig *c, void **m) {
  nuvio_engine_config k;
  nuvio_engine *e = NULL;
  nuvio_engine_status st;
  *m = NULL;
  // Biblioteca de outra versao (a .so do .tpk e separada): recusa em vez de
  // passar structs com tamanho errado.
  if (nuvio_engine_api_version() != NUVIO_ENGINE_API_VERSION) {
    printf("[p2p-motor] API do motor %u, esperada %u\n", nuvio_engine_api_version(),
           NUVIO_ENGINE_API_VERSION);
    return -1;
  }
  nuvio_engine_config_init(&k);
  k.data_directory = c->dados;
  k.cache_directory = c->cache;
  k.memory_cache_capacity_bytes = c->ram;
  k.disk_cache_capacity_bytes = c->disco;
  k.upload_mode = NUVIO_ENGINE_UPLOAD_LIMITED;
  k.upload_limit_bytes_per_second = c->upload_bps;
  k.torrent_profile = NUVIO_ENGINE_TORRENT_PROFILE_BALANCED;
  k.streaming_window_bytes = c->janela;   // #334; 0 = arquivo inteiro, como antes
  st = nuvio_engine_create(&k, &e);
  if (st != NUVIO_ENGINE_STATUS_OK || !e) {
    printf("[p2p-motor] create: %s\n", nuvio_engine_status_message(st));
    return -1;
  }
  *m = e;
  return 0;
}

static void destruir(void *m) { nuvio_engine_destroy((nuvio_engine *)m); }

static int addMagnet(void *m, const char *magnet, uint64_t *rid) {
  nuvio_engine_torrent_request q;
  nuvio_engine_torrent_request_init(&q);
  q.magnet_uri = magnet;
  q.source_type = NUVIO_ENGINE_TORRENT_SOURCE_MAGNET;
  return nuvio_engine_add_torrent((nuvio_engine *)m, &q, rid) == NUVIO_ENGINE_STATUS_OK ? 0 : -1;
}

static int evPoll(void *m, P2pmEvento *ev) {
  nuvio_engine_event e;
  nuvio_engine_event_init(&e);
  if (nuvio_engine_poll_event((nuvio_engine *)m, &e) != NUVIO_ENGINE_STATUS_OK) return 0;
  memset(ev, 0, sizeof *ev);
  switch (e.type) {
    case NUVIO_ENGINE_EVENT_TORRENT_ADDED:          ev->tipo = P2PM_EV_ADDED; break;
    case NUVIO_ENGINE_EVENT_TORRENT_METADATA_READY: ev->tipo = P2PM_EV_META; break;
    case NUVIO_ENGINE_EVENT_TORRENT_ERROR:          ev->tipo = P2PM_EV_ERRO; break;
    case NUVIO_ENGINE_EVENT_STREAM_PREPARED:        ev->tipo = P2PM_EV_PREPARADO; break;
    case NUVIO_ENGINE_EVENT_TORRENT_REMOVED:        ev->tipo = P2PM_EV_REMOVIDO; break;
    case NUVIO_ENGINE_EVENT_STREAM_STOPPED:         ev->tipo = P2PM_EV_PARADO; break;
    default:                                        ev->tipo = P2PM_EV_OUTRO; break;
  }
  if (e.dropped_events) printf("[p2p-motor] fila do motor perdeu %llu eventos\n",
                               (unsigned long long)e.dropped_events);
  ev->rid = e.request_id;
  snprintf(ev->tid, sizeof ev->tid, "%s", e.torrent_id);
  snprintf(ev->sid, sizeof ev->sid, "%s", e.stream_id);
  snprintf(ev->url, sizeof ev->url, "%s", e.stream_url);
  snprintf(ev->msg, sizeof ev->msg, "%s", e.message);
  return 1;
}

static int qtdArquivos(void *m, const char *tid, size_t *n) {
  *n = 0;
  return nuvio_engine_get_file_count((nuvio_engine *)m, tid, n) == NUVIO_ENGINE_STATUS_OK ? 0 : -1;
}

static int arquivo(void *m, const char *tid, size_t i, char *path, unsigned np, uint64_t *tam) {
  nuvio_engine_file f;
  nuvio_engine_file_init(&f);
  if (nuvio_engine_get_file((nuvio_engine *)m, tid, i, &f) != NUVIO_ENGINE_STATUS_OK) return -1;
  f.path[sizeof f.path - 1] = 0;
  snprintf(path, np, "%s", f.path);
  *tam = f.size;
  return 0;
}

static int preparar(void *m, const char *tid, uint32_t idx, uint64_t *rid) {
  nuvio_engine_stream_request s;
  nuvio_engine_stream_request_init(&s);
  s.torrent_id = tid;
  s.file_index = idx;
  return nuvio_engine_prepare_stream((nuvio_engine *)m, &s, rid) == NUVIO_ENGINE_STATUS_OK ? 0 : -1;
}

static void pararStream(void *m, const char *sid) {
  uint64_t rid;
  nuvio_engine_stop_stream((nuvio_engine *)m, sid, &rid);
}

static void remover(void *m, const char *tid) {
  uint64_t rid;
  nuvio_engine_remove_torrent((nuvio_engine *)m, tid, &rid);
}

static int stats(void *m, P2pmStats *s) {
  nuvio_engine_stats st;
  nuvio_engine_stats_init(&st);
  if (nuvio_engine_get_stats((nuvio_engine *)m, &st) != NUVIO_ENGINE_STATUS_OK) return -1;
  s->ram_usada = st.memory_cache_used_bytes;
  s->disco_motor = st.disk_cache_used_bytes;
  s->baixando_bps = st.download_rate_bytes_per_second;
  s->baixado = st.total_payload_download_bytes;
  s->pares = st.connected_peers;
  s->janela_apagado = st.window_freed_bytes;
  s->janela_rebaixadas = st.window_refetched_pieces;
  s->janela_sem_suporte = st.window_unsupported != 0;
  return 0;
}

static const P2pmOps REAIS = {
  versao, criar, destruir, addMagnet, evPoll, qtdArquivos, arquivo, preparar,
  pararStream, remover, stats
};
const P2pmOps *p2pmotor_ops_reais(void) {
#ifdef NV_P2P_MOTOR_DLOPEN
  pthread_once(&umaVez, carregar);
  if (!carregado) return NULL;
#endif
  return &REAIS;
}
#endif
