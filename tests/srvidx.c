// srvidx: the personal-server matching index (pure; no network).
#include "srvidx.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// dados.c stubs: one in-memory file.
static char *arquivo;
static char nomeArq[64];
int dados_gravar_leve(const char *nome, const char *conteudo) {
  free(arquivo); arquivo = strdup(conteudo); snprintf(nomeArq, sizeof nomeArq, "%s", nome); return 1; }
char *dados_ler(const char *nome) { return arquivo && !strcmp(nome, nomeArq) ? strdup(arquivo) : NULL; }
int dados_apagar(const char *nome) { if (!strcmp(nome, nomeArq)) { free(arquivo); arquivo = NULL; } return 1; }

static SrvIdxItem item(char tipo, const char *id, const char *imdb, long tmdb, long tvdb, const char *titulo, int ano) {
  SrvIdxItem e;
  memset(&e, 0, sizeof e);
  e.tipo = tipo;
  snprintf(e.item, sizeof e.item, "%s", id);
  if (imdb) snprintf(e.imdb, sizeof e.imdb, "%s", imdb);
  e.tmdb = tmdb; e.tvdb = tvdb;
  e.tituloHash = srvidx_titulo_hash(titulo);
  e.ano = (short)ano;
  return e;
}

int main(void) {
  SrvIdx x, y;
  SrvIdxItem e;
  char imdb[16], *t;
  long tmdb;
  int temp, ep, i;

  // ---- titles: case, accents, punctuation, one leading article
  assert(srvidx_titulo_hash("The Office") == srvidx_titulo_hash("office"));
  assert(srvidx_titulo_hash("Spider-Man: No Way Home") == srvidx_titulo_hash("spider man no way home"));
  assert(srvidx_titulo_hash("Amélie") == srvidx_titulo_hash("AMELIE"));
  assert(srvidx_titulo_hash("O Auto da Compadecida") == srvidx_titulo_hash("Auto da Compadecida"));
  assert(srvidx_titulo_hash("A") != 0 && srvidx_titulo_hash("The") != 0);   // an article alone is the title
  assert(srvidx_titulo_hash("") == 0 && srvidx_titulo_hash(NULL) == 0 && srvidx_titulo_hash("!!") == 0);
  assert(srvidx_titulo_hash("Alien") != srvidx_titulo_hash("Aliens"));
  assert(srvidx_titulo_hash("Ç") == srvidx_titulo_hash("c"));

  // ---- ids, dates, years, hosts
  assert(srvidx_id_titulo("tt0944947:2:5", imdb, &tmdb, &temp, &ep) && !strcmp(imdb, "tt0944947") && temp == 2 && ep == 5);
  assert(srvidx_id_titulo("tmdb:1399:1:3", imdb, &tmdb, &temp, &ep) && tmdb == 1399 && !imdb[0] && ep == 3);
  assert(!srvidx_id_titulo("kitsu:1", imdb, &tmdb, NULL, NULL) && !srvidx_id_titulo("jf:0f1e2d3c.ab", imdb, &tmdb, NULL, NULL));
  assert(srvidx_iso("1970-01-02T00:00:00.0000000Z") == 86400);
  assert(srvidx_iso("2026-10-01T12:34:56Z") == 1790858096LL);
  assert(srvidx_iso("garbage") == 0 && srvidx_iso(NULL) == 0);
  { char iso[32];
    srvidx_iso_de(1790858096LL, iso, sizeof iso);
    assert(!strcmp(iso, "2026-10-01T12:34:56Z") && srvidx_iso(iso) == 1790858096LL);
    srvidx_iso_de(951782400LL, iso, sizeof iso);                     // leap day
    assert(!strcmp(iso, "2000-02-29T00:00:00Z")); }
  assert(srvidx_ano_de("2019 · 2 temporadas") == 2019 && srvidx_ano_de("120 min") == 0 && srvidx_ano_de("") == 0);
  assert(srvidx_ano_de("1080p 1999") == 1999);
  assert(srvidx_host_local("http://192.168.1.5:8096") && srvidx_host_local("http://10.0.0.2/jf"));
  assert(srvidx_host_local("http://172.20.1.1:32400") && !srvidx_host_local("http://172.40.1.1:32400"));
  assert(srvidx_host_local("http://nas:8096") && srvidx_host_local("http://media.local:8096"));
  assert(srvidx_host_local("http://localhost:1") && srvidx_host_local("http://100.77.1.2:8096"));
  assert(srvidx_host_local("https://10-0-0-5.abcdef.plex.direct:32400"));
  assert(!srvidx_host_local("https://54-1-2-3.abcdef.plex.direct:32400"));
  assert(!srvidx_host_local("https://jellyfin.example.com") && !srvidx_host_local("http://8.8.8.8"));

  // ---- match rules
  srvidx_iniciar(&x);
  e = item('m', "aaa1", "tt0111161", 278, 0, "The Shawshank Redemption", 1994); assert(srvidx_por(&x, &e));
  e = item('m', "aaa2", NULL, 680, 0, "Pulp Fiction", 1994); assert(srvidx_por(&x, &e));
  e = item('s', "bbb1", "tt0944947", 1399, 121361, "Game of Thrones", 2011); assert(srvidx_por(&x, &e));
  e = item('s', "bbb2", NULL, 0, 81189, "Breaking Bad", 2008); assert(srvidx_por(&x, &e));
  e = item('m', "ccc1", NULL, 0, 0, "Home Video", 2015); assert(srvidx_por(&x, &e));
  e = item('m', "ccc2", NULL, 0, 0, "Dune", 1984); assert(srvidx_por(&x, &e));
  e = item('m', "ccc3", NULL, 0, 0, "Dune", 2021); assert(srvidx_por(&x, &e));
  e = item('m', "ccc4", NULL, 0, 0, "Twin", 2000); assert(srvidx_por(&x, &e));
  e = item('m', "ccc5", NULL, 0, 0, "Twin", 2001); assert(srvidx_por(&x, &e));
  assert(x.n == 9 && srvidx_n_tipo(&x, 's') == 2);
  assert(srvidx_achar(&x, "tt0111161", 0, 0, 'm') == 0);
  assert(srvidx_achar(&x, "tt0111161", 0, 0, 's') == -1);          // kind matters
  assert(srvidx_achar(&x, NULL, 680, 0, 'm') == 1);
  assert(srvidx_achar(&x, "tt9999999", 680, 0, 'm') == 1);          // imdb miss, tmdb hit
  assert(srvidx_achar(&x, NULL, 0, 81189, 's') == 3);
  assert(srvidx_achar(&x, NULL, 0, 81189, 'm') == -1);
  assert(srvidx_achar(&x, NULL, 0, 0, 'm') == -1);
  assert(srvidx_achar_titulo(&x, srvidx_titulo_hash("home video"), 2016, 'm') == 4);   // ±1 year
  assert(srvidx_achar_titulo(&x, srvidx_titulo_hash("home video"), 2018, 'm') == -1);
  assert(srvidx_achar_titulo(&x, srvidx_titulo_hash("Dune"), 2021, 'm') == 6);         // remake: the year decides
  assert(srvidx_achar_titulo(&x, srvidx_titulo_hash("Twin"), 2000, 'm') == -2);        // ambiguous: no match
  assert(srvidx_achar_titulo(&x, srvidx_titulo_hash("Dune"), 0, 'm') == -1);           // no year: no guess
  // upsert: same kind+item replaces, never duplicates
  e = item('m', "aaa1", "tt0111161", 278, 0, "The Shawshank Redemption", 1994);
  e.altura = 2160; e.hdr = SRVIDX_HDR_DV; e.atualizado = 500;
  assert(srvidx_por(&x, &e) && x.n == 9 && x.v[0].altura == 2160 && x.marca == 500);
  // episodes
  for (i = 1; i <= 3; i++) {
    char id[16];
    snprintf(id, sizeof id, "ep%d", i);
    e = item('e', id, NULL, 0, 0, NULL, 0);
    snprintf(e.serie, sizeof e.serie, "bbb1");
    e.temporada = 1; e.episodio = (short)i;
    assert(srvidx_por(&x, &e));
  }
  assert(srvidx_achar_ep(&x, "bbb1", 1, 2) >= 0 && !strcmp(x.v[srvidx_achar_ep(&x, "bbb1", 1, 2)].item, "ep2"));
  assert(srvidx_achar_ep(&x, "bbb1", 2, 1) == -1 && srvidx_achar_ep(&x, "bbb2", 1, 1) == -1);
  assert(srvidx_n_eps(&x, "bbb1") == 3);

  // ---- text round trip; another server's file is ignored
  x.total = 7;
  t = srvidx_serializar(&x, "0f1e2d3c");
  assert(t && !strstr(t, "Shawshank") && strstr(t, "tt0111161"));   // ids only, never a title
  srvidx_iniciar(&y);
  assert(srvidx_desserializar(&y, t, "deadbeef") == 0 && y.n == 0);
  assert(srvidx_desserializar(&y, t, "0f1e2d3c") == x.n && y.n == x.n && y.total == 7 && y.marca == 500);
  assert(srvidx_achar(&y, "tt0111161", 0, 0, 'm') >= 0 && y.v[srvidx_achar(&y, "tt0111161", 0, 0, 'm')].hdr == SRVIDX_HDR_DV);
  assert(srvidx_achar_ep(&y, "bbb1", 1, 3) >= 0);
  assert(srvidx_achar_titulo(&y, srvidx_titulo_hash("Dune"), 1984, 'm') >= 0);
  free(t);
  // A damaged line is skipped, the rest is read.
  srvidx_limpar(&y); srvidx_iniciar(&y);
  assert(srvidx_desserializar(&y, "srvidx 1 t 0 0\nm\tok1\t-\t-\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\n"
                                  "m\tbad/../x\t-\t-\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\nzz\n"
                                  "s\tok2\t-\ttt1\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0", "t") == 2);
  assert(!srvidx_desserializar(&y, "garbage", NULL));
  // disk
  assert(srvidx_gravar(&x, 3, "jf", "0f1e2d3c") && !strcmp(nomeArq, "srvidx-p3-jf.txt"));
  srvidx_limpar(&y); srvidx_iniciar(&y);
  assert(srvidx_ler(&y, 3, "jf", "0f1e2d3c") == x.n);
  assert(srvidx_ler(&y, 3, "px", "0f1e2d3c") == 0);
  srvidx_apagar(3, "jf");
  srvidx_limpar(&y); srvidx_iniciar(&y);
  assert(srvidx_ler(&y, 3, "jf", "0f1e2d3c") == 0);
  // merge (incremental pass): rows of the newer pass win
  { SrvIdx d; srvidx_iniciar(&d);
    e = item('m', "aaa2", NULL, 680, 0, "Pulp Fiction", 1994); e.visto = 1; e.atualizado = 900;
    srvidx_por(&d, &e);
    e = item('m', "new1", "tt0000001", 0, 0, "New", 2026); srvidx_por(&d, &e);
    srvidx_juntar(&x, &d);
    assert(x.n == 13 && x.v[1].visto && x.marca == 900 && srvidx_achar(&x, "tt0000001", 0, 0, 'm') == 12);
    srvidx_limpar(&d); }
  // big: upsert stays fast and correct
  srvidx_limpar(&y); srvidx_iniciar(&y);
  for (i = 0; i < 30000; i++) {
    char id[16];
    snprintf(id, sizeof id, "%x", i);
    e = item(i % 2 ? 'm' : 's', id, NULL, i + 1, 0, NULL, 0);
    assert(srvidx_por(&y, &e));
  }
  for (i = 0; i < 30000; i += 7) {
    char id[16];
    snprintf(id, sizeof id, "%x", i);
    e = item(i % 2 ? 'm' : 's', id, NULL, i + 1, 0, NULL, 0);
    assert(srvidx_por(&y, &e));
  }
  assert(y.n == 30000 && srvidx_achar(&y, NULL, 29999, 0, 's') == 29998);
  srvidx_limpar(&x); srvidx_limpar(&y);
  free(arquivo);
  printf("srvidx: all ok\n");
  return 0;
}
