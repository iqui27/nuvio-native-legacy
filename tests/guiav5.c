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
  assert(!strcmp(items[0].rating,"8.1"));
  G5Item metadata;
  const char *info="{\"stream_id\":21,\"name\":\"Artwork\",\"year\":\"2026\",\"genre\":\"Adventure\",\"rating\":8.4,\"backdrop_path\":[\"https://img.test/back.jpg\"]}";
  assert(g5_parse_xtream_item(info,info+strlen(info),G5_MOVIE,&metadata));
  assert(!strcmp(metadata.year,"2026")&&!strcmp(metadata.genre,"Adventure")&&!strcmp(metadata.rating,"8.4"));
  assert(!strcmp(metadata.backdrop,"https://img.test/back.jpg")&&metadata.age==-1);
  assert(items[1].adult);
  assert(g5_parse_xtream("{\"error\":\"denied\"}", G5_MOVIE, items, 4, &total) == -1);
  assert(g5_parse_xtream("[]", G5_MOVIE, items, 4, &total) == 0 && total == 0);
  assert(g5_parse_xtream_page(movies,G5_MOVIE,items,2,2,&total)==1&&!strcmp(items[0].id,"19")&&total==3);
  assert(g5_parse_xtream("[{\"stream_id\":1}", G5_MOVIE, items, 4, &total) == -1);
  G5Playlist p = {0};
  g5_playlist_init(&p, items, 4);
  const char *a = "#EXTM3U\n#EXTINF:-1 tvg-id=\"globo\" group-title=\"News\",Globo\nhttps://tv.test/live\n#EXTINF:-1 group-title=\"Movies\",Film\nhttps://tv.test/movie.mp4\n";
  for (size_t i = 0; a[i]; i++) assert(g5_playlist_feed(&p, a+i, 1));
  assert(g5_playlist_finish(&p) == 2);
  assert(items[0].kind == G5_LIVE && items[1].kind == G5_MOVIE);
  assert(!strcmp(items[0].epg, "globo"));
  const char *mixed="#EXTM3U\n#EXTINF:-1 tvg-id=\"hbo\" group-title=\"Filmes\",HBO\nhttps://tv.test/live/1.ts\n#EXTINF:-1 group-title=\"VOD\",Film\nhttps://tv.test/play/2\n#EXTINF:-1,Show S01E02\nhttps://tv.test/play/3\n#EXTINF:7200,Recorded film\nhttps://tv.test/play/4\n";
  g5_playlist_init(&p,items,4);assert(g5_playlist_feed(&p,mixed,strlen(mixed)));assert(g5_playlist_finish(&p)==4&&p.total==4);
  assert(items[0].kind==G5_LIVE&&items[1].kind==G5_MOVIE&&items[2].kind==G5_EPISODE&&items[3].kind==G5_MOVIE);
  assert(g5_parse_xtream("[{\"stream_id\":1,\"name\":\"Film\",\"stream_type\":\"movie\"}]",G5_LIVE,items,4,&total)==1&&items[0].kind==G5_MOVIE);
  g5_playlist_init(&p,items,4);assert(g5_playlist_feed(&p,a,strlen(a)));assert(g5_playlist_finish(&p)==2);
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
  strcpy(rules.blocked_channels[0],"17");rules.channel_count=1;rules.max_age=18;rules.ask_unrated=0;
  memset(&lease,0,sizeof lease);assert(g5_restricted(&rules,&lease,&item,1,101));
  strcpy(item.id,"18");assert(!g5_restricted(&rules,&lease,&item,1,101));
  strcpy(rules.blocked_groups[0],"Cinema");rules.group_count=1;strcpy(item.group,"Cinema");assert(g5_restricted(&rules,&lease,&item,1,101));
  assert(g5_live_slots(4,1,2,0)==1);
  assert(g5_live_slots(4,4,2,1)==1);
  assert(g5_live_slots(4,4,0,0)==0);
  assert(!strcmp(g5_item_category(items,0),"News"));
  items[0].group[0]=0;
  assert(!strcmp(g5_item_category(items,0),"Globo"));

  /* MULTIVIEW layout: every slot keeps 16:9 (+/-1%) in every mode, inside the
   * area, letterboxed (never squashed). */
  for (int m = 2; m <= 4; m++) {
    G5MvRect r[4];
    assert(g5_mv_layout(m, 48, 240, 1824, 816, 24, r) == m);
    for (int i = 0; i < m; i++) {
      float a = r[i].w / r[i].h;
      assert(a > 16.0f/9*0.99f && a < 16.0f/9*1.01f);
      assert(r[i].x >= 47.9f && r[i].y >= 239.9f && r[i].x + r[i].w <= 1872.1f && r[i].y + r[i].h <= 1056.1f);
    }
  }
  /* Narrow area: still 16:9, still inside. */
  { G5MvRect r[4]; g5_mv_layout(4, 0, 0, 900, 800, 16, r);
    for (int i = 0; i < 4; i++) { float a = r[i].w / r[i].h; assert(a > 16.0f/9*0.99f && a < 16.0f/9*1.01f); } }

  /* Per-source caps: Xtream honors max_connections (default 1), Stalker 1,
   * M3U/addons unlimited; usage counts per source, independently. */
  assert(g5_mv_source_cap("xt", 3) == 3 && g5_mv_source_cap("xt", 0) == 1 && g5_mv_source_cap("xt", -1) == 1);
  assert(g5_mv_source_cap("st", 9) == 1);
  assert(g5_mv_source_cap("mabcdef12", 1) == -1 && g5_mv_source_cap("a00ff00", 1) == -1 && g5_mv_source_cap("g5m3u", 1) == -1);
  { char keys[4][24] = {{"xt"},{"xt"},{"mabcdef12"},{""}};
    assert(g5_mv_source_use(keys, 4, "xt") == 2 && g5_mv_source_use(keys, 4, "mabcdef12") == 1);
    /* two channels from two different sources both fit: caps do not mix */
    assert(g5_mv_source_use(keys, 4, "st") == 0); }

  /* Slot controller: 120 simulated seconds with a periodic data refresh every
   * 30 s and EPG ticks per second — zero re-opens of a playing slot. */
  { G5MvCtl c; memset(&c, 0, sizeof c);
    g5_mv_ctl_set(&c, 0, "xtream:11", "xt", 0);
    g5_mv_ctl_set(&c, 1, "g5live:22", "mabcdef12", 0);
    int opens = 0;
    for (unsigned t = 0; t <= 120000; t += 50) {
      if (t % 30000 == 0) g5_mv_ctl_refresh(&c);   /* the 30 s data refresh */
      if (t == 300) g5_mv_ctl_pronto(&c, 0);
      if (t == 400) g5_mv_ctl_pronto(&c, 1);
      opens += g5_mv_ctl_abrir(&c, 0, t) + g5_mv_ctl_abrir(&c, 1, t);
    }
    assert(opens == 2);   /* one open per slot, ever */
    /* Real error: backoff 1/3/6/10 s, then gives up until a user action. */
    g5_mv_ctl_erro(&c, 0, 120000);
    assert(!g5_mv_ctl_abrir(&c, 0, 120500));
    assert(g5_mv_ctl_abrir(&c, 0, 121000));
    g5_mv_ctl_erro(&c, 0, 121000);
    assert(g5_mv_ctl_abrir(&c, 0, 124000) && !g5_mv_ctl_abrir(&c, 0, 124100));
    g5_mv_ctl_erro(&c, 0, 124000); assert(g5_mv_ctl_abrir(&c, 0, 130000));
    g5_mv_ctl_erro(&c, 0, 130000); assert(g5_mv_ctl_abrir(&c, 0, 140000));
    g5_mv_ctl_erro(&c, 0, 140000);   /* 4 retries used */
    assert(!g5_mv_ctl_abrir(&c, 0, 300000));
    /* User action (channel change) reopens immediately. */
    g5_mv_ctl_set(&c, 0, "xtream:99", "xt", 300000);
    assert(g5_mv_ctl_abrir(&c, 0, 300000));
    /* The other slot was never touched. */
    assert(c.aberturas == 2 + 4 + 1);
  }
  puts("PASS: bounded native catalogs, streamed playlist, scoped PIN leases, decoder/provider caps");
}
