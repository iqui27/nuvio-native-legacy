#include "guiav5.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  G5Item items[4];
  int total = 0;
  const char *movies = "[{\"stream_id\":17,\"name\":\"Filme\",\"category_id\":\"4\",\"container_extension\":\"mkv\",\"rating\":\"8.1\",\"stream_icon\":\"https://img.test/a.jpg\"},"
    "{\"stream_id\":18,\"name\":\"Adult\",\"is_adult\":1},{\"name\":\"Missing id\"},"
    "{\"stream_id\":19,\"name\":\"Third\"}]";
  assert(g5_parse_xtream(movies, G5_MOVIE, items, 2, &total) == 2);
  assert(total == 3 && !strcmp(items[0].id, "17") && !strcmp(items[0].group, "4"));
  assert(items[0].age == -1); /* provider review score is not an age rating */
  assert(items[1].adult);
  assert(g5_parse_xtream("{\"error\":\"denied\"}", G5_MOVIE, items, 4, &total) == -1);
  assert(g5_parse_xtream("[]", G5_MOVIE, items, 4, &total) == 0 && total == 0);
  assert(g5_parse_xtream("[{\"stream_id\":1}", G5_MOVIE, items, 4, &total) == -1);
  G5Playlist p = {0};
  g5_playlist_init(&p, items, 4);
  const char *a = "#EXTM3U\n#EXTINF:-1 tvg-id=\"globo\" group-title=\"News\",Globo\nhttps://tv.test/live\n#EXTINF:-1 group-title=\"Movies\",Film\nhttps://tv.test/movie.mp4\n";
  for (size_t i = 0; a[i]; i++) assert(g5_playlist_feed(&p, a+i, 1));
  assert(g5_playlist_finish(&p) == 2);
  assert(items[0].kind == G5_LIVE && items[1].kind == G5_MOVIE);
  assert(!strcmp(items[0].epg, "globo"));
  G5Rules rules = { .hide_adult = 1, .max_age = 12, .ask_unrated = 1 };
  G5Lease lease = {0};
  G5Item item = {.kind=G5_MOVIE, .age=16}; strcpy(item.id,"17"); strcpy(item.source,"xtream");
  assert(g5_restricted(&rules,&lease,&item,1,100));
  g5_grant(&lease,1,"xtream","17",100,3600,0);
  assert(g5_restricted(&rules,&lease,&item,1,100)); /* failed PIN never grants */
  g5_grant(&lease,1,"xtream","17",100,3600,1);
  assert(!g5_restricted(&rules,&lease,&item,1,101));
  assert(g5_restricted(&rules,&lease,&item,2,101));
  strcpy(item.source,"m3u"); assert(g5_restricted(&rules,&lease,&item,1,101));
  strcpy(item.source,"xtream"); assert(g5_restricted(&rules,&lease,&item,1,3700));
  assert(g5_live_slots(4,1,2,0)==1);
  assert(g5_live_slots(4,4,2,1)==1);
  assert(g5_live_slots(4,4,0,0)==0);
  assert(!strcmp(g5_item_category(items,0),"News"));
  items[0].group[0]=0;
  assert(!strcmp(g5_item_category(items,0),"Globo"));
  puts("PASS: bounded native catalogs, streamed playlist, scoped PIN leases, decoder/provider caps");
}
