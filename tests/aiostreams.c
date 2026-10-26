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
  assert(s.detalhes>=7);
  for(int i=0;i<s.detalhes;i++) assert(!strstr(s.detalhe[i].valor,"SECRET"));
  aio_configuracao(&s,"{\"data\":{\"userData\":null}}");
  assert(!s.configurada && !s.detalhes);
  aio_resultados(&s,"{\"streams\":[{\"url\":\"https://media.test/v\",\"streamData\":{\"cached\":true,\"service\":{\"cached\":false}}}]}");
  assert(s.resultado[0].cache==1);
  aio_legendas(&s,"{\"subtitles\":[{\"lang\":\"por\",\"label\":\"Portuguese SDH\",\"hearingImpaired\":true}]}");
  assert(s.legenda[0].sdh==1 && !strcmp(s.legenda[0].nome,"Portuguese SDH"));
  aio_configuracao(&s,"{\"data\":{\"userData\":{\"requiredEncodes\":[\"HEVC\"],\"resultLimits\":{\"global\":20,\"addon\":5},\"sortCriteria\":{\"global\":[{\"key\":\"size\",\"direction\":\"desc\"}]},\"formatter\":{\"id\":\"custom\",\"definitions\":{\"custom\":{\"name\":\"SECRET\"}}}}}}");
  assert(s.detalhes>=6 && strstr(s.resumo[5],"desc"));
  for(int i=0;i<s.detalhes;i++) assert(!strstr(s.detalhe[i].valor,"SECRET"));
  // Full public API envelope, synthetic values only. Exercise fields beyond the old 128-row cutoff.
  FILE *fixture=fopen("tests/fixtures/aiostreams-full.json","rb"); assert(fixture);
  char full[65536]; size_t bytes=fread(full,1,sizeof full-1,fixture); fclose(fixture); full[bytes]=0;
  aio_configuracao(&s,full);
  const char *sections[]={"requiredResolutions","includedQualities","excludedVisualTags","preferredAudioTags",
    "preferredLanguages","requiredSubtitles","size.resolution.576p.movies","size.resolution.Unknown.series",
    "bitrate.useMetadataRuntime","requiredSeederRange","excludeAgeRange","excludeCachedFromServices",
    "preferredRegexPatterns","requiredKeywords","rankedStreamExpressions","sortCriteria.cachedAnime",
    "services","presets","formatter.definitions.saved","resultLimits.mode","proxy.proxiedServices",
    "appliedTemplates","digitalReleaseFilter","titleMatching","deduplicator","catalogModifications",
    "autoPlay","failover","precacheSelector","linkedAccounts"};
  for(size_t k=0;k<sizeof sections/sizeof *sections;k++) {
    int found=0;
    for(int i=0;i<s.detalhes;i++) if(!strncmp(s.detalhe[i].nome,sections[k],strlen(sections[k]))) found=1;
    if(!found) fprintf(stderr,"missing section: %s\n",sections[k]);
    assert(found);
  }
  assert(s.detalhes>128 && s.servicos==2 && s.addons==4);
  int lastLanguage=0,regex=0,direction=0;
  for(int i=0;i<s.detalhes;i++) {
    if(strstr(s.detalhe[i].nome,"preferredLanguages") && strstr(s.detalhe[i].valor,"Czech")) lastLanguage=1;
    if(strstr(s.detalhe[i].nome,"RegexPatterns") && strstr(s.detalhe[i].valor,"(?i)")) regex=1;
    if(strstr(s.detalhe[i].nome,"sortCriteria") && strstr(s.detalhe[i].valor,"asc")) direction=1;
  }
  assert(lastLanguage && regex && direction && !s.detalhesOmitidos);
  aio_configuracao(&s,"{\"success\":true,\"data\":{\"userData\":{\"uuid\":\"PRIVATE\",\"encryptedPassword\":\"PRIVATE\",\"services\":[{\"id\":\"torbox\",\"credentials\":{\"apiKey\":\"PRIVATE\"}}],\"presets\":[{\"type\":\"custom\",\"enabled\":true,\"options\":{\"arbitraryKey\":\"PRIVATE\",\"url\":\"PRIVATE\"}}],\"proxy\":{\"credentials\":\"PRIVATE\",\"publicUrl\":\"PRIVATE\"}}}}");
  for(int i=0;i<s.detalhes;i++) assert(!strstr(s.detalhe[i].valor,"PRIVATE"));
  // Jellyfin locks can contain PINs or password hashes, including persona locks.
  aio_configuracao(&s,"{\"data\":{\"userData\":{\"jellyfin\":{\"primary\":{\"lock\":\"7391\"},\"personas\":[{\"lock\":\"$2b$12$synthetic-hash\"}]}}}}");
  int locks=0;
  for(int i=0;i<s.detalhes;i++) {
    assert(!strstr(s.detalhe[i].valor,"7391") && !strstr(s.detalhe[i].valor,"synthetic-hash"));
    if(strstr(s.detalhe[i].nome,".lock")) { assert(!strcmp(s.detalhe[i].valor,"hidden")); locks++; }
  }
  assert(locks==2);
  aio_configuracao(&s,"{\"success\":false,\"data\":{\"userData\":{\"preferredLanguages\":[\"stale\"]}}}");
  assert(!s.configurada && !s.detalhes && !s.resumo[2][0]);
  // Capacity ceiling must be explicit, never silently lose late fields.
  char many[65536]="{\"data\":{\"userData\":{\"requiredKeywords\":[";
  for(int i=0;i<2200;i++) strcat(many,i?",\"sample\"":"\"sample\"");
  strcat(many,"]}}}"); aio_configuracao(&s,many);
  assert(s.detalhes==AIO_DETAIL_MAX && s.detalhesOmitidos==2200-AIO_DETAIL_MAX);
  Stream *playable=NULL;
  assert(stream_extrair("{\"streams\":[{\"externalUrl\":\"https://external.test/page\"},{\"url\":\"https://media.test/video.mp4\"}]}","AIOStreams",&playable)==1);
  free(playable);
  puts("aiostreams: URL, authentication, manifest, config and returned inventory passed");
  return 0;
}
