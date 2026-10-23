// Legenda automatica: normalizacao do idioma do addon, escolha, memoria por titulo.
#include "../src/legauto.c"
#include "../src/linguas.c"
#include <assert.h>
uint64_t legsync_hash_url(const char *u) { uint64_t h = 1469598103934665603ull; while (*u) h = (h ^ (unsigned char)*u++) * 1099511628211ull; return h; }
const char *i18n(const char *t) { return t; }
static Legenda L(const char *lang, const char *url, const char *arq) {
  Legenda l; memset(&l, 0, sizeof l);
  ling_normalizar(lang, l.idioma, sizeof l.idioma);
  snprintf(l.url, sizeof l.url, "%s", url); snprintf(l.arquivo, sizeof l.arquivo, "%s", arq);
  return l;
}
int main(void) {
  char c[16];
  // A CAUSA (foto da TV, 04/10): o AIOStreams manda "PORTUGUESE"; copiado em 8
  // bytes virava "PORTUGU", que nao casa com "pt" nem "por".
  assert(!ling_casa("PORTUGU", "pt"));
  // #429: codigo conjunto servo-croata
  assert(ling_casa("hbs", "hr") && ling_casa("hbs", "sr") && ling_casa("sh", "bs"));
  assert(ling_casa("hr", "hbs") && ling_casa("bos", "bs") && !ling_casa("hbs", "sl") && !ling_casa("hr", "sr"));
  ling_normalizar("PORTUGUESE", c, sizeof c); assert(ling_casa(c, "pt"));
  ling_normalizar("Portuguese (Brazil)", c, sizeof c); assert(ling_casa(c, "pt") && !strcmp(ling_selo(c), "PT-BR"));
  ling_normalizar("por", c, sizeof c); assert(!strcmp(c, "por"));
  ling_normalizar("pt-BR", c, sizeof c); assert(ling_casa(c, "pt"));
  ling_normalizar("ENGLISH", c, sizeof c); assert(ling_casa(c, "en"));
  assert(!strcmp(ling_selo("por"), "PT") && !strcmp(ling_selo("eng"), "EN"));

  // Primeira boa no idioma: ingles na frente nao atrapalha; pt-BR vence pt-PT.
  Legenda v[5];
  v[0] = L("eng", "http://x/a.srt", "a");
  v[1] = L("pt", "http://x/b.srt", "Movie.2020.720p.WEB");
  v[2] = L("pob", "http://x/c.srt", "Movie.2020.1080p.BluRay");
  v[3] = L("PORTUGUESE", "http://x/d.srt", "");
  assert(legauto_escolher(v, 4, "pt", NULL, NULL, 0, 0) == 2);
  // Nenhuma no idioma: -1.
  assert(legauto_escolher(v, 1, "pt", NULL, NULL, 0, 0) == -1);
  // Mesmo grau: o que divide palavras com o arquivo que toca.
  v[2] = L("pob", "http://x/c.srt", "Other.Group.x264");
  v[4] = L("pob", "http://x/e.srt", "Movie.2020.1080p.BluRay.YIFY");
  assert(legauto_escolher(v, 5, "pt", "http://h/Movie.2020.1080p.BluRay.YIFY.mkv", NULL, 0, 0) == 4);
  // O CASO DA TCL (11/10): tres portuguesas com o titulo do filme em comum; a
  // sincronizada divide a fonte e o grupo (WEB-DL, NTb) com o arquivo que toca,
  // a outra so palavras do titulo e do episodio. O URL do debrid e opaco.
  {
    const char *tocando = "The.Grand.Tour.S01E01.Eurocrash.Part.One.1080p.WEB-DL.DDP5.1.H.264-NTb.mkv";
    Legenda w[4];
    w[0] = L("por", "http://x/w0.srt", "The.Grand.Tour.S01E01.Eurocrash.Part.One.720p.HDTV.x264-LOL");
    w[1] = L("por", "http://x/w1.srt", "The.Grand.Tour.S01E01.Eurocrash.Part.One.BluRay.x265-RARBG");
    w[2] = L("por", "http://x/w2.srt", "The.Grand.Tour.S01E01.WEB-DL.NTb");
    w[3] = L("eng", "http://x/w3.srt", "The.Grand.Tour.S01E01.Eurocrash.Part.One.1080p.WEB-DL.NTb");
    // palavras do titulo que todas dividem nao decidem; fonte+grupo decidem
    assert(legauto_escolher(w, 4, "pt", tocando, NULL, 0, 0) == 2);
    // sem nome da fonte, o URL opaco nao diz nada: ordem de chegada
    assert(legauto_escolher(w, 4, "pt", "https://rd.example/d/AbC123xYz", NULL, 0, 0) == 0);
    // com o nome da fonte informado, legauto_midia o prefere ao URL
    legauto_fonte(tocando);
    assert(legauto_escolher(w, 4, "pt", legauto_midia("https://rd.example/d/AbC123xYz"), NULL, 0, 0) == 2);
    legauto_fonte(NULL);
    assert(!strcmp(legauto_midia("u"), "u"));
  }
  // Ja recusada pelo AutoSync fica fora.
  uint64_t ex[1] = { legsync_hash_url(v[4].url) };
  assert(legauto_escolher(v, 5, "pt", "http://h/Movie.2020.1080p.BluRay.YIFY.mkv", ex, 1, 0) == 2);
  // A que ja deu certo neste titulo vence tudo.
  assert(legauto_escolher(v, 5, "pt", "http://h/Movie.2020.1080p.BluRay.YIFY.mkv", NULL, 0, legsync_hash_url(v[1].url)) == 1);
  // Memoria por titulo+idioma (4 entradas, a mais velha sai).
  legauto_lembrar("tt1", "pt", 11); legauto_lembrar("tt2", "pt", 22); legauto_lembrar("tt3", "pt", 33); legauto_lembrar("tt4", "pt", 44);
  assert(legauto_lembrada("tt1", "pt-BR") == 11 && legauto_lembrada("tt1", "en") == 0);
  legauto_lembrar("tt5", "pt", 55);
  assert(legauto_lembrada("tt1", "pt") == 0 && legauto_lembrada("tt2", "pt") == 22 && legauto_lembrada("tt5", "pt") == 55);
  puts("legauto: normalizacao, melhor idioma, release, recusadas, memoria ok");
  return 0;
}
