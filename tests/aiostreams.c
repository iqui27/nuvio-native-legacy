// Offline contract: private paths never become display text; root fields only.
#include "aiostreams.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  AioSnapshot s = {0}; char out[4096], auth[4096];
  assert(aio_url("https://host.test/stremio/uuid/token/manifest.json?secret=yes", out, sizeof out));
  assert(!strcmp(out, "https://host.test"));
  assert(!aio_url("https://user:password@host.test/manifest.json", out, sizeof out));
  assert(!aio_url("file:///tmp/manifest.json", out, sizeof out));
  assert(!aio_url("https://host.test/\nsecret", out, sizeof out));
  assert(aio_autorizacao("https://host.test/stremio/uuid/token/manifest.json", auth, sizeof auth));
  assert(!strcmp(auth, "Authorization: Basic dXVpZDp0b2tlbg=="));
  assert(!aio_autorizacao("https://host.test/stremio/alias/manifest.json", auth, sizeof auth));
  assert(aio_manifesto(&s, "{\"catalogs\":[{\"extra\":[{\"name\":\"search\"}],\"id\":\"popular\",\"type\":\"movie\",\"name\":\"Cinema\"}],\"id\":\"aiostreams.test\",\"name\":\"AIOStreams\",\"resources\":[\"catalog\",{\"name\":\"stream\"},\"subtitles\"],\"behaviorHints\":{\"configurationRequired\":false}}"));
  assert(s.catalogos == 1 && !strcmp(s.catalogo[0].nome, "Cinema"));
  assert(s.recursos == (AIO_CATALOGO | AIO_STREAM | AIO_LEGENDA));
  assert(!aio_manifesto(&s, "{\"id\":\"not-aiostreams\",\"name\":\"Other\"}"));
  assert(!aio_manifesto(&s, "{\"id\":\"aiostreams\",\"behaviorHints\":{\"configurationRequired\":true}}"));
  assert(!aio_manifesto(&s, "not json"));
  aio_configuracao(&s, "{\"data\":{\"userData\":{\"services\":[{\"id\":\"torbox\",\"enabled\":true,\"apiKey\":\"SECRET\"}],\"presets\":[{\"type\":\"torrentio\",\"options\":{\"name\":\"SECRET\"},\"name\":\"Torrentio\",\"enabled\":true}],\"excludedResolutions\":[\"2160p\"],\"sortCriteria\":{\"global\":[{\"key\":\"cached\"}]},\"unknownFuture\":{\"apiKey\":\"SECRET\"}}}}" );
  assert(s.configurada && s.servicos == 1 && s.addons == 1);
  assert(!strcmp(s.servico[0].nome, "torbox"));
  assert(!strcmp(s.addon[0].nome, "torrentio"));
  assert(s.regras[0] == 1);
  assert(!strstr((char *)&s, "SECRET"));
  aio_resultados(&s, "{\"streams\":[{\"name\":\"Video\",\"url\":\"https://media.test/secret\"},{\"name\":\"External\",\"externalUrl\":\"https://external.test/private\"},{\"infoHash\":\"abcdef\"},{\"nzbUrl\":\"https://nzb.test/file\"}]}" );
  assert(s.resultados == 4 && s.links == 2 && s.p2p == 1);
  assert(!strcmp(s.resultado[0].tipo, "HTTP"));
  assert(!strcmp(s.resultado[1].tipo, "Externo"));
  assert(!strcmp(s.resultado[2].tipo, "P2P"));
  assert(s.resultado[0].cache == -1);
  assert(!strstr(s.resultado[0].nome, "secret"));
  puts("aiostreams: URL, authentication, manifest, config and returned inventory passed");
  return 0;
}
