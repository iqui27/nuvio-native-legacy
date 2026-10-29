// Shared parser fixtures: both server shapes, sidecars, extraction and item identity.
#include "jellyfin.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
void testarEmbyBib(void) {
  const char *body = "{\"Type\":\"Episode\",\"ProviderIds\":{\"Imdb\":\"tt1234567\",\"Tmdb\":\"42\"},\"ParentIndexNumber\":2,\"IndexNumber\":3,\"MediaSources\":[{\"Id\":\"mediasource_1001\",\"MediaStreams\":[{\"Type\":\"Audio\",\"Index\":1},{\"Type\":\"Subtitle\",\"Index\":2,\"Codec\":\"subrip\",\"Language\":\"por\",\"DisplayTitle\":\"Portuguese\"},{\"Type\":\"Subtitle\",\"Index\":5,\"Codec\":\"ass\",\"IsExternal\":true,\"Language\":\"eng\",\"Path\":\"/private/Show.en.ass\"},{\"Type\":\"Subtitle\",\"Index\":6,\"Codec\":\"srt\",\"IsExternal\":true,\"Path\":\"/private/Show.srt\"},{\"Type\":\"Subtitle\",\"Index\":7,\"Codec\":\"pgssub\"}]}]}";
  Legenda *subs = calloc(LEG_MAX, sizeof *subs); assert(subs);
  for (int kind = 0; kind < 2; kind++) {
    JfConta c = {0}; char id[64]; c.tipo = kind;
    strcpy(c.base, "http://fixture.invalid/media"); strcpy(c.token, "fixture-secret");
    assert(jf_ler_legendas(&c, "1001", "mediasource_1001", body, strlen(body), subs, LEG_MAX) == 3);
    assert(strstr(subs[0].url, "/Videos/1001/mediasource_1001/Subtitles/2/Stream.srt"));
    assert(strstr(subs[1].url, "/Subtitles/5/Stream.ass"));
    assert(strstr(subs[2].url, "/Subtitles/6/Stream.srt"));
    assert(!strcmp(subs[1].arquivo, "Show.en.ass") && !strstr(subs[1].arquivo, "/private"));
    assert(strstr(subs[0].url, kind ? "/emby/Videos" : "/media/Videos"));
    assert(jf_ler_legendas(&c, "1001", "other-version", body, strlen(body), subs, LEG_MAX) == 0);
    assert(jf_legenda_id(body, strlen(body), id, sizeof id) && !strcmp(id, "tt1234567:2:3"));
    const char *tmdb = "{\"Type\":\"Movie\",\"ProviderIds\":{\"Tmdb\":\"42\"}}";
    assert(jf_legenda_id(tmdb, strlen(tmdb), id, sizeof id) && !strcmp(id, "tmdb:42"));
  }
  free(subs);
}
