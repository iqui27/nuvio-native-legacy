// PoC do motor P2P embutido (nuvio-engine + libtorrent) fora do app.
//
//   poc <infoHash> [pasta] [segundos] [cacheMB]
//
// Adiciona o torrent por magnet (com trackers publicos), espera os metadados,
// escolhe o maior arquivo de video, prepara o stream e imprime a URL local
// (http://127.0.0.1:P/...) que o player toca com Range. A cada 2 s imprime
// pares, taxa, cache em memoria e em disco. Sai depois de `segundos` (0 = nunca).
#include "nuvio_engine/nuvio_engine.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "../../tests/nvtmp.h"

static volatile sig_atomic_t parar;
static void sinal(int s) { (void)s; parar = 1; }

static double agora(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec / 1e9;
}

static int ehVideo(const char *n) {
  const char *e = strrchr(n, '.');
  return e && (!strcasecmp(e, ".mp4") || !strcasecmp(e, ".mkv") ||
               !strcasecmp(e, ".webm") || !strcasecmp(e, ".avi") ||
               !strcasecmp(e, ".mov") || !strcasecmp(e, ".m4v"));
}

int main(int argc, char **argv) {
  const char *hash = argc > 1 ? argv[1] : "dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c";
  char pastaTmp[1100];
  const char *pasta = argv[argc > 2 ? 2 : 0];
  if (argc <= 2) { const char *t = getenv("TMPDIR"); snprintf(pastaTmp, sizeof pastaTmp, "%s/nvp2p", nvt_dir()); pasta = pastaTmp; }
  int limite = argc > 3 ? atoi(argv[3]) : 0;
  int cacheMB = argc > 4 ? atoi(argv[4]) : 512;
  char dados[600], cache[600], magnet[2000], tid[65] = "";
  nuvio_engine_config cfg;
  nuvio_engine *m = NULL;
  nuvio_engine_torrent_request req;
  nuvio_engine_event ev;
  uint64_t rid;
  double t0 = agora(), tMeta = 0, tUrl = 0, ultimo = 0;
  int st, preparado = 0;

  signal(SIGINT, sinal); signal(SIGTERM, sinal);
  snprintf(dados, sizeof dados, "%s/dados", pasta);
  snprintf(cache, sizeof cache, "%s/cache", pasta);
  nuvio_engine_config_init(&cfg);
  cfg.data_directory = dados;
  cfg.cache_directory = cache;
  cfg.memory_cache_capacity_bytes = 16ull << 20;     // TV: 16 MB de RAM de cache
  cfg.disk_cache_capacity_bytes = (uint64_t)cacheMB << 20;
  cfg.upload_mode = NUVIO_ENGINE_UPLOAD_LIMITED;
  cfg.upload_limit_bytes_per_second = 256 * 1024;
  cfg.torrent_profile = NUVIO_ENGINE_TORRENT_PROFILE_BALANCED;
  st = nuvio_engine_create(&cfg, &m);
  if (st) { fprintf(stderr, "create: %s\n", nuvio_engine_status_message(st)); return 1; }
  fprintf(stderr, "[poc] motor %s / %s\n", nuvio_engine_version_string(),
          nuvio_engine_protocol_backend_version());

  snprintf(magnet, sizeof magnet,
           "magnet:?xt=urn:btih:%s"
           "&tr=udp%%3A%%2F%%2Ftracker.opentrackr.org%%3A1337%%2Fannounce"
           "&tr=udp%%3A%%2F%%2Fopen.stealth.si%%3A80%%2Fannounce"
           "&tr=udp%%3A%%2F%%2Ftracker.torrent.eu.org%%3A451%%2Fannounce"
           "&tr=udp%%3A%%2F%%2Fexodus.desync.com%%3A6969%%2Fannounce"
           "&tr=udp%%3A%%2F%%2Ftracker.leechers-paradise.org%%3A6969", hash);
  nuvio_engine_torrent_request_init(&req);
  req.magnet_uri = magnet;
  req.source_type = NUVIO_ENGINE_TORRENT_SOURCE_MAGNET;
  st = nuvio_engine_add_torrent(m, &req, &rid);
  if (st) { fprintf(stderr, "add: %s\n", nuvio_engine_status_message(st)); return 1; }

  while (!parar && (!limite || agora() - t0 < limite)) {
    nuvio_engine_event_init(&ev);
    while (nuvio_engine_poll_event(m, &ev) == NUVIO_ENGINE_STATUS_OK) {
      if (ev.type == NUVIO_ENGINE_EVENT_TORRENT_ADDED) {
        snprintf(tid, sizeof tid, "%s", ev.torrent_id);
        fprintf(stderr, "[poc] %.1fs adicionado %s\n", agora() - t0, tid);
      } else if (ev.type == NUVIO_ENGINE_EVENT_TORRENT_METADATA_READY && !preparado) {
        size_t n = 0, i, melhor = (size_t)-1;
        uint64_t tam = 0;
        nuvio_engine_stream_request sr;
        tMeta = agora() - t0;
        snprintf(tid, sizeof tid, "%s", ev.torrent_id);
        nuvio_engine_get_file_count(m, tid, &n);
        for (i = 0; i < n; i++) {
          nuvio_engine_file f;
          nuvio_engine_file_init(&f);
          if (nuvio_engine_get_file(m, tid, i, &f)) continue;
          fprintf(stderr, "[poc]   arquivo %zu %8.1f MB %s\n", i, f.size / 1048576.0, f.path);
          if (ehVideo(f.path) && f.size > tam) { tam = f.size; melhor = i; }
        }
        fprintf(stderr, "[poc] %.1fs metadados, %zu arquivos, escolhido %zu\n", tMeta, n, melhor);
        if (melhor == (size_t)-1) return 2;
        nuvio_engine_stream_request_init(&sr);
        sr.torrent_id = tid;
        sr.file_index = (uint32_t)melhor;
        st = nuvio_engine_prepare_stream(m, &sr, &rid);
        if (st) { fprintf(stderr, "prepare: %s\n", nuvio_engine_status_message(st)); return 1; }
        preparado = 1;
      } else if (ev.type == NUVIO_ENGINE_EVENT_STREAM_PREPARED) {
        tUrl = agora() - t0;
        fprintf(stderr, "[poc] %.1fs stream pronto (%.1f MB)\n", tUrl, ev.file_size / 1048576.0);
        printf("%s\n", ev.stream_url);
        fflush(stdout);
      } else if (ev.type == NUVIO_ENGINE_EVENT_TORRENT_ERROR) {
        fprintf(stderr, "[poc] erro: %s\n", ev.message);
      } else if (ev.type == NUVIO_ENGINE_EVENT_DISK_CACHE_RECLAIMED) {
        fprintf(stderr, "[poc] cache em disco recolhido: %s\n", ev.message);
      }
      nuvio_engine_event_init(&ev);
    }
    if (agora() - ultimo >= 2) {
      nuvio_engine_stats s;
      ultimo = agora();
      nuvio_engine_stats_init(&s);
      nuvio_engine_get_stats(m, &s);
      fprintf(stderr, "[poc] %5.1fs pares %u (seeds %u) baixa %6.0f KB/s sobe %5.0f KB/s total %6.1f MB"
              " mem %4.1f/%4.1f MB disco %6.1f/%6.1f MB http %u\n",
              agora() - t0, s.connected_peers, s.connected_seeds,
              s.download_rate_bytes_per_second / 1024.0, s.upload_rate_bytes_per_second / 1024.0,
              s.total_payload_download_bytes / 1048576.0,
              s.memory_cache_used_bytes / 1048576.0, s.memory_cache_capacity_bytes / 1048576.0,
              s.disk_cache_used_bytes / 1048576.0, s.disk_cache_capacity_bytes / 1048576.0,
              s.active_http_requests);
    }
    usleep(50 * 1000);
  }
  fprintf(stderr, "[poc] fim: metadados %.1fs, url %.1fs\n", tMeta, tUrl);
  nuvio_engine_destroy(m);
  return 0;
}
