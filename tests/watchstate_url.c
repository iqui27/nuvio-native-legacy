// URL do push: query preservada depois do caminho, sem vazar URLs no teste.
#include <stdio.h>
#include <string.h>
#include "watchstate.h"

int main(void) {
  WsEvento e = {0};
  char url[256];
  e.ev = WS_STOP; strcpy(e.tipo, "movie"); strcpy(e.videoId, "tt0133093");
  e.atMs = e.iniMs = 1000;
  if (!ws_url("https://addon.test/cfg?apikey=fixture", &e, url, sizeof url) ||
      strcmp(url, "https://addon.test/cfg/watch_state/push/movie/tt0133093.json?apikey=fixture")) {
    puts("watchstate_url: query failure"); return 1;
  }
  if (!ws_url("https://addon.test/cfg///?apikey=fixture", &e, url, sizeof url) ||
      strcmp(url, "https://addon.test/cfg/watch_state/push/movie/tt0133093.json?apikey=fixture") ||
      ws_url("https://addon.test/cfg?apikey=fixture", &e, url, 20)) {
    puts("watchstate_url: slash or capacity failure"); return 1;
  }
  puts("watchstate_url: ok"); return 0;
}
