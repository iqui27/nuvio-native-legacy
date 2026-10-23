// Offline contract: private paths never become display text; root fields only.
#include "aiostreams.h"
#include "streams.h"
#include <stdlib.h>
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
  assert(aio_manifesto(&s, "{\"id\":\"com.aiostreams.viren070.user\",\"name\":\"My custom name\",\"resources\":[]}"));
  assert(!aio_manifesto(&s, "{\"id\":\"not-aiostreams\",\"name\":\"Other\"}"));
  assert(!aio_manifesto(&s, "{\"id\":\"aiostreams\",\"behaviorHints\":{\"configurationRequired\":true}}"));
  assert(!aio_manifesto(&s, "not json"));
  assert(!aio_manifesto(&s, "{\"id\":\"aiostreams\""));
  assert(!aio_manifesto(&s, "{\"id\":\"aiostreams\"} trailing"));
  aio_configuracao(&s, "{\"data\":{\"userData\":{\"services\":[{\"id\":\"torbox\",\"enabled\":true,\"apiKey\":\"SECRET\"}],\"presets\":[{\"type\":\"torrentio\",\"options\":{\"name\":\"SECRET\"},\"name\":\"Torrentio\",\"enabled\":true}],\"excludedResolutions\":[\"2160p\"],\"sortCriteria\":{\"global\":[{\"key\":\"cached\"}]},\"unknownFuture\":{\"apiKey\":\"SECRET\"}}}}" );
  assert(s.configurada && s.servicos == 1 && s.addons == 1);
  assert(!strcmp(s.servico[0].nome, "torbox"));
  assert(!strcmp(s.addon[0].nome, "torrentio"));
  assert(s.regras[0] == 1);
  assert(!strcmp(s.resumo[0], "2160p"));
  assert(!strcmp(s.resumo[5], "cached"));
  assert(!strstr((char *)&s, "SECRET"));
  aio_resultados(&s, "{\"streams\":[{\"name\":\"Video\",\"url\":\"https://media.test/secret\"},{\"name\":\"External\",\"externalUrl\":\"https://external.test/private\"},{\"infoHash\":\"abcdef\"},{\"nzbUrl\":\"https://nzb.test/file\"}]}" );
  assert(s.resultados == 4 && s.links == 2 && s.p2p == 1);
  assert(!strcmp(s.resultado[0].tipo, "HTTP"));
  assert(!strcmp(s.resultado[1].tipo, "Externo"));
  assert(!strcmp(s.resultado[2].tipo, "P2P"));
  assert(s.resultado[0].cache == -1);
  assert(!strstr(s.resultado[0].nome, "secret"));
  aio_resultados(&s,"{\"streams\":[{\"name\":\"Mixed\",\"url\":\"https://media.test/v\",\"externalUrl\":\"https://outside.test/private\"}]}");
  assert(s.links==1 && !strcmp(s.resultado[0].tipo,"Externo"));
  assert(!strcmp(s.resultado[0].destino,"https://outside.test/private"));
  aio_resultados(&s,"{\"streams\":[{\"url\":\"https://media.test/v\",\"streamData\":{\"service\":{\"cached\":false},\"parsedFile\":{\"resolution\":\"2160p\"}}}]}");
  assert(s.resultado[0].cache==0 && s.resultado[0].altura==2160);
  aio_status(&s,"{\"data\":{\"version\":\"2.36.0\",\"channel\":\"stable\",\"settings\":{\"searchApiDisabled\":false}}}");
  assert(s.searchAtiva && !strcmp(s.versaoInstancia,"2.36.0"));
  aio_search(&s,"{\"success\":true,\"data\":{\"statistics\":[{},{}],\"errors\":[{}],\"filtered\":3}}");
  assert(s.search && s.estatisticas==2 && s.erros==1 && s.filtrados==3);
  aio_search(&s,"{\"success\":true,\"data\":{\"statistics\":[{\"title\":\"Addon timing\",\"description\":\"https://private.test/token\"}],\"errors\":[{\"title\":\"Timeout\",\"description\":\"12 s\"}]}}");
  assert(s.evidencias==2 && !strcmp(s.evidencia[0].titulo,"Addon timing") && !s.evidencia[0].descricao[0] && s.evidencia[1].erro);
  aio_legendas(&s,"{\"subtitles\":[{\"lang\":\"por\",\"forced\":true,\"sdh\":false},{\"lang\":\"eng\"}]}");
  assert(s.legendas==2 && s.legenda[0].forcada==1 && s.legenda[0].sdh==0 && s.legenda[1].forcada==-1);
  aio_configuracao(&s,"{\"data\":{\"userData\":{\"requiredResolutions\":[\"1080p\"],\"includedResolutions\":[\"2160p\"],\"excludedResolutions\":[\"720p\"],\"bitrate\":{\"global\":{\"series\":[0,20]}},\"proxy\":{\"enabled\":true,\"credentials\":\"SECRET\",\"url\":\"https://private.test/SECRET\"},\"appliedTemplates\":[{\"id\":\"cinema\",\"version\":\"1\"}]}}}");
  assert(s.detalhes==7 && !strstr((char *)s.detalhe,"SECRET"));
  aio_configuracao(&s,"{\"data\":{\"userData\":null}}");
  assert(!s.configurada && !s.detalhes);
  aio_resultados(&s,"{\"streams\":[{\"url\":\"https://media.test/v\",\"streamData\":{\"cached\":true,\"service\":{\"cached\":false}}}]}");
  assert(s.resultado[0].cache==1);
  aio_legendas(&s,"{\"subtitles\":[{\"lang\":\"por\",\"label\":\"Portuguese SDH\",\"hearingImpaired\":true}]}");
  assert(s.legenda[0].sdh==1 && !strcmp(s.legenda[0].nome,"Portuguese SDH"));
  aio_configuracao(&s,"{\"data\":{\"userData\":{\"requiredEncodes\":[\"HEVC\"],\"resultLimits\":{\"global\":20,\"addon\":5},\"sortCriteria\":{\"global\":[{\"key\":\"size\",\"direction\":\"desc\"}]},\"formatter\":{\"id\":\"custom\",\"definitions\":{\"custom\":{\"name\":\"SECRET\"}}}}}}");
  assert(s.detalhes==6 && strstr(s.resumo[5],"desc") && !strstr((char *)s.detalhe,"SECRET"));
  Stream *playable=NULL;
  assert(stream_extrair("{\"streams\":[{\"externalUrl\":\"https://external.test/page\"},{\"url\":\"https://media.test/video.mp4\"}]}","AIOStreams",&playable)==1);
  free(playable);
  puts("aiostreams: URL, authentication, manifest, config and returned inventory passed");
  return 0;
}
