// AIOStreams + TorBox (#205): a stream the add-on marks cached (name "⚡" or
// clientResolve.isCached:true) must not turn "not cached" because the
// description carries a stray "⏳" (age/duration), "download]" or "uncached".
// Shapes anonymized from a real LG webOS 2.0.6 log (850 kept, none usable).
#include "streams.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(void) {
  Stream *v = NULL;
  int n = stream_extrair(
    "{\"streams\":["
    // 0: name marks cached, description has "⏳ 2d" (age)
    "{\"name\":\"[TB\xe2\x9a\xa1] AIO\\n1080p\",\"url\":\"https://aio.invalid/p/0\","
    "\"description\":\"\xf0\x9f\x93\xa6 4.2 GB \xe2\x8f\xb3 2d \xf0\x9f\x91\xa4 12\\nMovie.2024.1080p.WEB-DL.mkv\"},"
    // 1: explicit clientResolve.isCached:true, description says "not cached" elsewhere
    "{\"name\":\"AIO 2160p\",\"description\":\"Movie 2160p (not cached on other service)\","
    "\"clientResolve\":{\"type\":\"debrid\",\"service\":\"torbox\",\"infoHash\":\"aa11\",\"isCached\":true}},"
    // 2: label itself says ⏳ -> still not cached
    "{\"name\":\"\xe2\x8f\xb3 TB 1080p\",\"url\":\"https://aio.invalid/p/2\"},"
    // 3: isCached:false torrent -> not cached
    "{\"name\":\"AIO 1080p\",\"clientResolve\":{\"service\":\"torbox\",\"infoHash\":\"bb22\",\"isCached\":false}},"
    // 4: label says ⏳ and ⚡ both elsewhere -> label wins, not cached
    "{\"name\":\"\xe2\x8f\xb3 TB\",\"description\":\"\xe2\x9a\xa1 seeds\",\"url\":\"https://aio.invalid/p/4\"},"
    // 5: neutral label, description says "Not cached" and "⚡" only in the filename -> not cached
    "{\"name\":\"AIOStreams 1080p\",\"url\":\"https://aio.invalid/p/5\","
    "\"description\":\"[TB download] Not cached | Movie.\xe2\x9a\xa1.1080p.mkv\"},"
    // 6: stale "⚡" name + URL, but clientResolve.isCached:false -> explicit status wins
    "{\"name\":\"[TB\xe2\x9a\xa1] AIO 1080p\",\"url\":\"https://aio.invalid/p/6\",\"description\":\"Not cached\","
    "\"clientResolve\":{\"service\":\"torbox\",\"isCached\":false}},"
    // 7: negative name status beyond both the 191-byte label and 1023-byte name buffers wins
    "{\"name\":\"[TB+] "
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    " Not cached\",\"url\":\"https://aio.invalid/p/7\",\"description\":\"Not cached\"}"
    "]}", "AIO | TB Instant", &v);
  assert(n == 8);
  assert(v[0].emCache && !v[0].foraCache);
  assert(v[1].emCache && !v[1].foraCache);
  assert(!v[2].emCache && v[2].foraCache);
  assert(!v[3].emCache && v[3].foraCache && v[3].cacheNegado);
  assert(!v[4].emCache && v[4].foraCache);
  assert(!v[5].emCache && v[5].foraCache);
  assert(!v[6].emCache && v[6].foraCache && v[6].cacheNegado);
  assert(!v[7].emCache && v[7].foraCache);
  free(v);
  puts("torbox_aio: cached marks beat stray description text");
  return 0;
}
