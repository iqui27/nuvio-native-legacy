// AIOStreams Stremio output: transformer copies stream.size to videoSize.
// https://github.com/Viren070/AIOStreams/blob/main/packages/core/src/transformers/stremio.ts
static const char chipfonte2_json[] =
  "{\"streams\":[{\"name\":\"⚡ 4K | Dolby Vision | Atmos 5.1\","
  "\"description\":\"Project Hail Mary\","
  "\"url\":\"https://example.invalid/aio/play/movie.mp4\","
  "\"behaviorHints\":{\"videoSize\":19756849562,"
  "\"filename\":\"Project.Hail.Mary.2160p.DV.Atmos.5.1.mkv\","
  "\"bingeGroup\":\"aiostreams-real-debrid\"},"
  "\"streamData\":{\"size\":19756849562,\"addon\":\"Torrentio\","
  "\"service\":{\"id\":\"realdebrid\",\"cached\":true}}}]}";

static Stream chipfonte2_source(void) {
  Stream *parsed = NULL;
  assert(stream_extrair(chipfonte2_json,"AIOStreams Stable",&parsed)==1);
  Stream source = parsed[0];
  free(parsed);
  assert(source.tamanhoBytes==UINT64_C(19756849562));
  return source;
}
