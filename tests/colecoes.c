// Colecoes da conta no shape do web -> ColFolder, e a chave de fileira por id.
#include "../src/colecoes.h"
#include "nvtmp.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
const char *addons_base_por_id(const char *id) { return id && !strcmp(id, "org.x") ? "https://resolvido" : ""; }
int main(void) {
  const char *web =
    "[{\"collections_json\":{\"collections\":[{\"id\":\"c1\",\"title\":\"Streaming\",\"backdropImageUrl\":\"https://img/bg.jpg\","
    "\"folders\":[{\"id\":\"f1\",\"title\":\"Netflix\",\"coverImageUrl\":\"https://img/nf.jpg\",\"titleLogoUrl\":\"https://img/nf.png\",\"hideTitle\":true,"
    "\"sources\":[{\"provider\":\"addon\",\"addonId\":\"x\",\"addonBaseUrl\":\"https://addon/abc/manifest.json\",\"type\":\"movie\",\"catalogId\":\"nf_movies\",\"title\":\"Movies\",\"genre\":\"None\"},"
    "{\"provider\":\"tmdb\",\"tmdbSourceType\":\"DISCOVER\"},"
    "{\"addonBaseUrl\":\"https://addon/abc\",\"type\":\"series\",\"catalogId\":\"nf_series\",\"catalogName\":\"Series\"}]},"
    "{\"id\":\"f2\",\"title\":\"Vazia\",\"sources\":[]}]}]}}]";
  assert(col_definir_json(web) == 1);
  const ColFolder *f = col_folder(0);
  assert(f && !strcmp(f->group, "Streaming") && !strcmp(f->groupId, "c1") && !strcmp(f->id, "f1"));
  assert(!strcmp(f->cover, "https://img/nf.jpg") && f->hideTitle == 1);
  // BANNER na ordem do web: heroBackdropUrl > coverImageUrl > backdropImageUrl
  // da colecao. A pasta nao trouxe heroBackdropUrl, entao o hero fica vazio e o
  // backdrop da colecao e so o ultimo recurso.
  assert(!f->hero[0] && !strcmp(f->groupBackdrop, "https://img/bg.jpg"));
  assert(!strcmp(col_banner(f), "https://img/nf.jpg") && !strcmp(col_capa(f), "https://img/nf.jpg"));
  assert(f->nSources == 3);
  assert(!strcmp(f->sources[0].base, "https://addon/abc") && !strcmp(f->sources[0].catId, "nf_movies") && !f->sources[0].genre[0]);
  // A fonte tmdb ENTRA (issue #44): antes ela era descartada e uma pasta so
  // de fontes tmdb/trakt sumia inteira — a colecao "instalada no site" que
  // nunca aparecia na TV.
  assert(!strcmp(f->sources[1].prov, "tmdb") && !strcmp(f->sources[1].tmdbTipo, "DISCOVER"));
  assert(!strcmp(f->sources[2].title, "Series") && !strcmp(f->sources[2].type, "series"));
  // Cadeia de arte da pasta, na ordem do web, ate o ultimo campo.
  assert(col_definir_json("{\"collections\":[{\"id\":\"cc\",\"title\":\"C\",\"backdropImageUrl\":\"https://img/grupo.jpg\",\"folders\":["
    "{\"id\":\"a\",\"title\":\"So grupo\",\"sources\":[{\"addonId\":\"a\",\"type\":\"movie\",\"catalogId\":\"k\"}]},"
    "{\"id\":\"b\",\"title\":\"Com hero\",\"heroBackdropUrl\":\"https://img/h.jpg\",\"coverImageUrl\":\"https://img/c.jpg\","
      "\"sources\":[{\"addonId\":\"a\",\"type\":\"movie\",\"catalogId\":\"k\"}]},"
    "{\"id\":\"n\",\"title\":\"Nada\",\"sources\":[{\"addonId\":\"a\",\"type\":\"movie\",\"catalogId\":\"k\"}]}]},"
    "{\"id\":\"cd\",\"title\":\"D\",\"folders\":[{\"id\":\"z\",\"title\":\"Zero\","
      "\"sources\":[{\"addonId\":\"a\",\"type\":\"movie\",\"catalogId\":\"k\"}]}]}]}") == 4);
  assert(!strcmp(col_capa(col_folder(0)), "https://img/grupo.jpg") && !strcmp(col_banner(col_folder(0)), "https://img/grupo.jpg"));
  assert(!strcmp(col_banner(col_folder(1)), "https://img/h.jpg") && !strcmp(col_capa(col_folder(1)), "https://img/c.jpg"));
  assert(!col_banner(col_folder(3))[0] && !col_capa(col_folder(3))[0]);   // sem campo nenhum: nada inventado
  puts("ok  banner: heroBackdropUrl > coverImageUrl > backdropImageUrl da colecao; capa: cover > backdrop; vazio fica vazio");
  assert(col_definir_json(web) == 1);
  puts("ok  shape do web: linha da RPC, manifest.json cortado, fonte tmdb entra, genre None vazio");

  char chave[192];
  col_chave_grupo("Streaming", chave, sizeof chave); assert(!strcmp(chave, "collection_c1"));
  col_chave_grupo("Outro", chave, sizeof chave);     assert(!strcmp(chave, "collection_Outro"));
  puts("ok  chave por id da colecao, nome quando nao ha id");

  // Fonte trakt do editor do site: lista publica por id, com ordenacao.
  assert(col_definir_json("{\"collections\":[{\"id\":\"ct\",\"title\":\"T\",\"folders\":[{\"id\":\"g\",\"title\":\"G\",\"sources\":["
    "{\"provider\":\"trakt\",\"traktListId\":1234,\"mediaType\":\"MOVIE\",\"sortBy\":\"rank\",\"sortHow\":\"asc\",\"title\":\"Top filmes\"},"
    "{\"provider\":\"tmdb\",\"tmdbSourceType\":\"PERSON\",\"tmdbId\":6384,\"mediaType\":\"MOVIE\",\"filters\":{\"withGenres\":\"28\"}},"
    "{\"provider\":\"desconhecido\",\"catalogId\":\"x\"}]}]}]}") == 1);
  f = col_folder(0);
  assert(f->nSources == 2);   // provedor desconhecido continua fora
  assert(!strcmp(f->sources[0].prov, "trakt") && f->sources[0].traktLista == 1234
         && !strcmp(f->sources[0].ordenar, "rank") && !strcmp(f->sources[0].ordem, "asc"));
  assert(!strcmp(f->sources[1].prov, "tmdb") && !strcmp(f->sources[1].tmdbTipo, "PERSON")
         && f->sources[1].tmdbId == 6384 && strstr(f->sources[1].filtros, "withGenres"));
  puts("ok  fontes trakt e tmdb entram com id, ordenacao e filtros");

  // string escapada, como parseRemoteCollectionsPayload aceita
  const char *esc = "{\"collections_json\":\"{\\\"collections\\\":[{\\\"id\\\":\\\"c9\\\",\\\"title\\\":\\\"T\\\",\\\"folders\\\":[{\\\"id\\\":\\\"g\\\",\\\"title\\\":\\\"G\\\",\\\"sources\\\":[{\\\"addonBaseUrl\\\":\\\"https://a\\\",\\\"type\\\":\\\"movie\\\",\\\"catalogId\\\":\\\"k\\\"}]}]}]}\"}";
  assert(col_definir_json(esc) == 1 && !strcmp(col_folder(0)->groupId, "c9"));
  puts("ok  collections_json como string escapada");

  // Only an explicit complete empty snapshot deletes old account collections.
  unsigned rev = col_revisao();
  const char *invalidas[] = {
    "{\"collections\":[] garbage}",
    "{\"collections\":[],\"broken\":}",
    "{\"collections\":[],\"broken\":[}",
    "{\"collections\":[],}",
    "{\"collections\":[],\"broken\":01}",
    "{\"collections\":[],\"broken\":1e+}",
    "{\"collections\":[],\"broken\":\"\\q\"}",
    "{\"collections\":[],\"broken\":\"\\u123\"}",
    "{\"collections\":[],\"broken\":truex}",
    "{\"collections_json\":\"{\\\"collections\\\":[],\\\"broken\\\":}\"}",
    "{\"collections_json\":\"{\\\"collections\\\":[]}\\u0000garbage\"}",
    "{\"collections_json\":\"{\\\"collections\\\":[],\\\"x\\\":\\\"\\n\\\"}\"}",
    "{\"collections_json\":\"{\\\"collections\\\":[],\\\"x\\\":\\\"\\u000A\\\"}\"}",
    "{\"collections_json\":\"{\\\"collections\\\":[],\\\"x\\\":\\\"\\u000D\\\"}\"}",
    "{\"collections_json\":\"{\\\"collections\\\":[],\\\"x\\\":\\\"\\u0009\\\"}\"}",
    "{\"collections_json\":\"{\\\"collections\\\":[],\\\"x\\\":\\\"\\u0001\\\"}\"}",
    "{\"collections_json\":\"{\\\"collections\\\":[],\\\"x\\\":\\\"\\uD83D\\uDE\"}"
  };
  for (size_t i = 0; i < sizeof invalidas / sizeof invalidas[0]; i++) {
    assert(!col_resposta_valida(invalidas[i]));
    assert(col_definir_json(invalidas[i]) == 0 && col_n() == 1 && col_revisao() == rev);
  }
  assert(col_resposta_valida(" \n{\"collections\":[],\"other\":[-1.25e+3,true,false,null,\"\\u00e1\\n\",{}]}\t"));
  assert(col_resposta_valida("{\"collections_json\":\"\\n{\\\"collections\\\":[],\\\"x\\\":\\\"\\u00e1\\uD83D\\uDE00\\\"}\\t\"}"));
  assert(col_resposta_valida("{\"collections_json\":\"\\u000A\\u000D{\\\"collections\\\":[]}\\u0009\"}"));
  puts("ok  malformed envelope and encoded snapshot retain last-good collections; complete JSON values accepted");
  assert(col_definir_json("[]") == 0 && col_n() == 1 && col_revisao() == rev);
  assert(col_definir_json("{\"collections_json\":null}") == 0 && col_n() == 1);
  assert(col_definir_json("{\"collections\":[{\"id\":\"cut\"") == 0 && col_n() == 1);
  assert(col_definir_json("{\"collections\":[]}") == 0 && col_n() == 0 && col_revisao() != rev);
  rev = col_revisao();
  assert(col_definir_json("{\"collections\":[]}") == 0 && col_revisao() == rev);
  puts("ok  explicit empty clears; missing/null/truncated retain; unchanged revision is stable");
  // fonte so com addonId (como a conta manda): entra, e a base resolve no acesso
  assert(col_definir_json("{\"collections\":[{\"id\":\"c\",\"title\":\"T\",\"folders\":[{\"id\":\"g\",\"title\":\"G\",\"sources\":[{\"provider\":\"addon\",\"addonId\":\"org.x\",\"type\":\"movie\",\"catalogId\":\"k\"}]}]}]}") == 1);
  assert(!strcmp(col_folder(0)->sources[0].base, "https://resolvido"));
  puts("ok  addonId sem URL resolve pela sonda");
  // a RPC real: collections_json e o array direto
  assert(col_definir_json("[{\"profile_id\":1,\"collections_json\":[{\"id\":\"r\",\"title\":\"R\",\"folders\":[{\"id\":\"g\",\"title\":\"G\",\"sources\":[{\"addonId\":\"a\",\"type\":\"movie\",\"catalogId\":\"k\"}]}]}],\"updated_at\":\"x\"}]") == 1);
  assert(!strcmp(col_folder(0)->groupId, "r"));
  puts("ok  linha da RPC com o array direto");
  // GIF DE FOCO (#29). O pacote nao guarda URL nenhuma — ele converte o GIF em
  // 001.jpg..090.jpg na importacao —, entao a URL so chega por aqui. Sem esta
  // leitura as colecoes da conta nunca animam, que e o issue inteiro.
  assert(col_definir_json("{\"collections\":[{\"id\":\"c\",\"title\":\"T\",\"folders\":["
    "{\"id\":\"g1\",\"title\":\"Com GIF\",\"focusGifUrl\":\"https://cdn/a.gif\","
      "\"sources\":[{\"addonId\":\"a\",\"type\":\"movie\",\"catalogId\":\"k\"}]},"
    "{\"id\":\"g2\",\"title\":\"Sem campo\","
      "\"sources\":[{\"addonId\":\"a\",\"type\":\"movie\",\"catalogId\":\"k\"}]},"
    "{\"id\":\"g3\",\"title\":\"Desligado\",\"focusGifUrl\":\"https://cdn/c.gif\",\"focusGifEnabled\":false,"
      "\"sources\":[{\"addonId\":\"a\",\"type\":\"movie\",\"catalogId\":\"k\"}]}]}]}") == 3);
  assert(!strcmp(col_folder(0)->focusGif, "https://cdn/a.gif"));
  assert(!col_folder(1)->focusGif[0]);   // campo ausente e o caso comum
  assert(!col_folder(2)->focusGif[0]);   // focusGifEnabled:false desliga
  puts("ok  focusGifUrl entra, campo ausente fica vazio, focusGifEnabled:false desliga");
  // pacote + conta com o mesmo id: fica a arte local, grupo/titulo da conta
  { char dir[NVT_MAX]; nvt_set(dir, sizeof dir, "/nuvio-col-XXXXXX"); char caminho[300]; FILE *f;
    assert(mkdtemp(dir));
    snprintf(caminho, sizeof caminho, "%s/collections.json", dir); f = fopen(caminho, "w");
    fputs("{\"groups\":[{\"id\":\"c1\",\"title\":\"Streaming\",\"folders\":[{\"id\":\"f1\",\"title\":\"Netflix\",\"cover\":\"collections/f1/cover.jpg\",\"hero\":\"collections/f1/hero.jpg\",\"frames\":12,\"sources\":[{\"title\":\"Movies\",\"base\":\"https://addon/abc\",\"type\":\"movie\",\"catId\":\"nf_movies\"}]}]}]}", f); fclose(f);
    assert(col_carregar(dir) == 1 && col_folder(0)->local && col_folder(0)->frames == 12);
    assert(col_definir_json("{\"collections\":[{\"id\":\"c1\",\"title\":\"Streaming Renomeado\",\"folders\":[{\"id\":\"f1\",\"title\":\"Netflix\",\"coverImageUrl\":\"https://cdn/nf.webp\",\"sources\":[{\"addonId\":\"x\",\"type\":\"movie\",\"catalogId\":\"nf_movies\"}]}]},{\"id\":\"c2\",\"title\":\"Nova\",\"folders\":[{\"id\":\"f9\",\"title\":\"Nova pasta\",\"coverImageUrl\":\"https://cdn/n.webp\",\"sources\":[{\"addonId\":\"x\",\"type\":\"movie\",\"catalogId\":\"k\"}]}]}]}") == 2);
    assert(strstr(col_folder(0)->hero, "/collections/f1/hero.jpg") && col_folder(0)->frames == 12 && col_folder(0)->local);
    assert(!strcmp(col_folder(0)->group, "Streaming Renomeado") &&
           !strcmp(col_folder(0)->sources[0].addonId, "x") && !col_folder(0)->sources[0].base[0]);
    assert(!strcmp(col_folder(1)->cover, "https://cdn/n.webp") && !col_folder(1)->local);
    puts("ok  pasta do pacote guarda arte e quadros; a conta da grupo, titulo e pastas novas"); }
  // "ARTE DAS PASTAS DA CONTA": desligado e o pacote (acima); ligado, capa,
  // fundo e logo da conta vencem campo a campo, e desligar de novo devolve a
  // arte curada na hora (o casamento e refeito com a ultima resposta).
  { char dir[NVT_MAX]; nvt_set(dir, sizeof dir, "/nuvio-colarte-XXXXXX"); char caminho[300]; FILE *f;
    const char *conta =
      "{\"collections\":[{\"id\":\"c1\",\"title\":\"S\",\"folders\":["
      "{\"id\":\"f1\",\"title\":\"Netflix\",\"coverImageUrl\":\"https://cdn/nf.webp\","
        "\"heroBackdropUrl\":\"https://cdn/nf-hero.jpg\",\"titleLogoUrl\":\"https://cdn/nf-logo.png\","
        "\"focusGifUrl\":\"https://cdn/nf.gif\","
        "\"sources\":[{\"addonId\":\"x\",\"type\":\"movie\",\"catalogId\":\"k\"}]},"
      "{\"id\":\"f2\",\"title\":\"Prime\",\"titleLogoUrl\":\"https://cdn/pv-logo.png\","
        "\"sources\":[{\"addonId\":\"x\",\"type\":\"movie\",\"catalogId\":\"k\"}]}]}]}";
    assert(mkdtemp(dir));
    snprintf(caminho, sizeof caminho, "%s/collections.json", dir); f = fopen(caminho, "w");
    fputs("{\"groups\":[{\"id\":\"c1\",\"title\":\"S\",\"folders\":["
          "{\"id\":\"f1\",\"title\":\"Netflix\",\"cover\":\"c/f1.jpg\",\"hero\":\"c/f1h.jpg\",\"logo\":\"c/f1l.png\",\"frames\":12,"
            "\"sources\":[{\"title\":\"M\",\"base\":\"https://a\",\"type\":\"movie\",\"catId\":\"k\"}]},"
          "{\"id\":\"f2\",\"title\":\"Prime\",\"cover\":\"c/f2.jpg\",\"hero\":\"c/f2h.jpg\",\"logo\":\"c/f2l.png\","
            "\"sources\":[{\"title\":\"M\",\"base\":\"https://a\",\"type\":\"movie\",\"catId\":\"k\"}]}]}]}", f);
    fclose(f);
    col_arte_conta(0);
    assert(col_carregar(dir) == 2);
    assert(col_definir_json(conta) == 2);
    assert(strstr(col_folder(0)->cover, "c/f1.jpg") && col_folder(0)->frames == 12);
    assert(strstr(col_folder(1)->logo, "c/f2l.png"));
    col_arte_conta(1);   // refaz na hora, sem nova resposta da conta
    assert(col_n() == 2 && col_folder(0)->local && col_folder(1)->local);
    assert(!strcmp(col_folder(0)->cover, "https://cdn/nf.webp") && col_folder(0)->frames == 0 &&
           !strcmp(col_folder(0)->focusGif, "https://cdn/nf.gif"));
    assert(!strcmp(col_folder(0)->hero, "https://cdn/nf-hero.jpg") && !col_folder(0)->editorial);
    assert(!strcmp(col_folder(0)->logo, "https://cdn/nf-logo.png"));
    // So o logo na conta: capa e fundo continuam os do pacote.
    assert(strstr(col_folder(1)->cover, "c/f2.jpg") && strstr(col_folder(1)->hero, "c/f2h.jpg") &&
           !strcmp(col_folder(1)->logo, "https://cdn/pv-logo.png"));
    assert(col_definir_json(conta) == 2);   // ciclo de sync com o ajuste ligado: estavel
    assert(!strcmp(col_folder(0)->cover, "https://cdn/nf.webp"));
    col_arte_conta(0);   // desligar devolve a arte curada
    assert(strstr(col_folder(0)->cover, "c/f1.jpg") && col_folder(0)->frames == 12 &&
           strstr(col_folder(0)->hero, "c/f1h.jpg") && strstr(col_folder(1)->logo, "c/f2l.png"));
    remove(caminho); rmdir(dir);
    puts("ok  arte das pastas da conta: desligado o pacote, ligado a conta campo a campo, volta na hora"); }
  // O GIF DA CONTA CONTRA A ARTE DO PACOTE. A regra e "a versao local fica
  // inteira", e o GIF nao a contradiz: o pacote nao TEM focusGif para perder.
  //   frames > 0  — ja anima pela sequencia curada; a URL da conta e ignorada.
  //   frames == 0 — nao ha animacao nenhuma; a URL entra, senao a pasta ficaria
  //                 parada tendo GIF disponivel.
  { char dir[NVT_MAX]; nvt_set(dir, sizeof dir, "/nuvio-colgif-XXXXXX"); char caminho[300]; FILE *f;
    const char *conta =
      "{\"collections\":[{\"id\":\"c1\",\"title\":\"G\",\"folders\":["
      "{\"id\":\"comq\",\"title\":\"Com quadros\",\"focusGifUrl\":\"https://cdn/q.gif\","
        "\"sources\":[{\"addonId\":\"a\",\"type\":\"movie\",\"catalogId\":\"k\"}]},"
      "{\"id\":\"semq\",\"title\":\"Sem quadros\",\"focusGifUrl\":\"https://cdn/s.gif\","
        "\"sources\":[{\"addonId\":\"a\",\"type\":\"movie\",\"catalogId\":\"k\"}]}]}]}";
    assert(mkdtemp(dir));
    snprintf(caminho, sizeof caminho, "%s/collections.json", dir); f = fopen(caminho, "w");
    fputs("{\"groups\":[{\"id\":\"c1\",\"title\":\"G\",\"folders\":["
          "{\"id\":\"comq\",\"title\":\"Com quadros\",\"frames\":12,"
            "\"sources\":[{\"title\":\"M\",\"base\":\"https://a\",\"type\":\"movie\",\"catId\":\"k\"}]},"
          "{\"id\":\"semq\",\"title\":\"Sem quadros\",\"frames\":0,"
            "\"sources\":[{\"title\":\"M\",\"base\":\"https://a\",\"type\":\"movie\",\"catId\":\"k\"}]}]}]}", f);
    fclose(f);
    assert(col_carregar(dir) == 2);
    assert(!col_folder(0)->focusGif[0] && !col_folder(1)->focusGif[0]);
    assert(col_definir_json(conta) == 2);
    assert(col_folder(0)->frames == 12 && !col_folder(0)->focusGif[0]);
    assert(col_folder(1)->frames == 0 && !strcmp(col_folder(1)->focusGif, "https://cdn/s.gif"));
    assert(col_folder(0)->local && col_folder(1)->local);
    remove(caminho); rmdir(dir);
    puts("ok  o GIF da conta so entra na pasta local que nao tem sequencia de quadros"); }
  // OS QUATRO NIVEIS DE col_diagnostico. O que este teste guarda nao e a
  // funcao e sim a CAPACIDADE DE SEPARAR causas: os quatro casos abaixo
  // produzem hoje o mesmo sintoma na tela (fileira solta na home, #18) e o
  // nivel e a unica coisa que diz qual deles aconteceu. Se dois deles voltarem
  // a devolver o mesmo numero, o diagnostico volta a ser inutil e o relator
  // volta a mandar log que nao decide nada.
  { char g[64];
    assert(col_definir_json("{\"collections\":[{\"id\":\"c\",\"title\":\"Streaming\","
           "\"folders\":[{\"id\":\"g\",\"title\":\"G\",\"sources\":["
           "{\"addonBaseUrl\":\"https://x\",\"type\":\"movie\",\"catalogId\":\"top\"}]}]}]}") == 1);
    assert(col_diagnostico("https://outro", "movie", "top", g, sizeof g) == 0);
    assert(col_diagnostico("https://x", "series", "top", g, sizeof g) == 1);
    assert(col_diagnostico("https://x", "movie", "imdbRating", g, sizeof g) == 2);
    assert(col_diagnostico("https://x", "movie", "top", g, sizeof g) == 3);
    // O grupo sai junto: sem ele o nivel 3 diz "casou" e nao diz ONDE, e o
    // nivel 3 e exatamente o caso em que a pessoa precisa ir desocultar algo.
    assert(!strcmp(g, "Streaming"));
    // Base vazia nao casa com nada. Sem esta guarda uma fileira sem base
    // casaria com toda fonte ainda nao resolvida — o falso positivo que
    // col_por_catalogo ja evita, repetido aqui porque sao duas varreduras.
    assert(col_diagnostico("", "movie", "top", g, sizeof g) == 0);
    puts("ok  col_diagnostico separa os quatro motivos de nao engolir"); }

  // FORMA DA PASTA (tileShape do web): POSTER, LANDSCAPE/WIDE, e o resto —
  // inclusive ausente — quadrado. A fileira usa a da maioria.
  { const char *f = "\"sources\":[{\"addonBaseUrl\":\"https://a/x\",\"type\":\"movie\",\"catalogId\":\"k\"}]";
    char js[2000];
    snprintf(js, sizeof js,
      "{\"collections\":[{\"id\":\"cf\",\"title\":\"Formas\",\"folders\":["
      "{\"id\":\"p\",\"title\":\"P\",\"tileShape\":\"POSTER\",%s},"
      "{\"id\":\"w\",\"title\":\"W\",\"tileShape\":\"wide\",%s},"
      "{\"id\":\"l\",\"title\":\"L\",\"posterShape\":\"LANDSCAPE\",%s},"
      "{\"id\":\"s\",\"title\":\"S\",%s}]}]}", f, f, f, f);
    assert(col_definir_json(js) == 4);
    assert(col_folder(0)->forma == COL_FORMA_POSTER);
    assert(col_folder(1)->forma == COL_FORMA_PAISAGEM);
    assert(col_folder(2)->forma == COL_FORMA_PAISAGEM);
    assert(col_folder(3)->forma == COL_FORMA_QUADRADO);
    assert(col_grupo_forma("Formas") == COL_FORMA_PAISAGEM);
    assert(col_grupo_forma("nao existe") == COL_FORMA_PAISAGEM);
    assert(col_forma_texto("square") == COL_FORMA_QUADRADO);
    puts("ok  tileShape da pasta vira a forma do cartao, na regra do web"); }
  // Empty account snapshots remove package rows too. A later nonempty profile
  // snapshot can still recover the curated artwork without reviving old rows.
  { char dir[NVT_MAX], arq[700]; nvt_set(dir, sizeof dir, "/nuvio-col-perfil-XXXXXX");
    FILE *f;
    int i, achou = 0;
    assert(mkdtemp(dir));
    snprintf(arq, sizeof arq, "%s/collections.json", dir);
    f = fopen(arq, "w"); assert(f);
    fputs("{\"groups\":[{\"title\":\"G\",\"id\":\"g1\",\"folders\":[{\"id\":\"f1\",\"title\":\"Netflix\","
          "\"cover\":\"c.jpg\",\"sources\":[{\"base\":\"https://a\",\"type\":\"movie\",\"catId\":\"m\"}]}]}]}", f);
    fclose(f);
    assert(col_carregar(dir) == 1);
    assert(col_definir_json("[{\"collections_json\":[]}]") == 0 && col_n() == 0);
    col_esquecer_perfil();
    assert(col_n() == 0);
    assert(col_definir_json("[{\"collections_json\":[]}]") == 0 && col_n() == 0);
    assert(col_definir_json("[{\"collections_json\":{\"collections\":[]}}]") == 0 && col_n() == 0);
    assert(col_definir_json(web) == 1);
    for (i = 0; i < col_n(); i++)
      if (!strcmp(col_folder(i)->id, "f1")) {
        achou = 1;
        assert(col_folder(i)->local == 1 && strstr(col_folder(i)->cover, "/c.jpg"));
      }
    assert(achou);
    unlink(arq); rmdir(dir);
    puts("ok  troca de perfil: colecoes do anterior saem, arte do pacote volta"); }

  { ColSource s; char n[128];
    memset(&s, 0, sizeof s);
    snprintf(s.catId, sizeof s.catId, "streaming_netflix_movies");
    snprintf(s.title, sizeof s.title, "streaming_netflix_movies");   // conta sem titulo: colecoes.c copia o catId
    col_nome_fonte(&s, "", n, sizeof n); assert(!n[0]);
    col_nome_fonte(&s, NULL, n, sizeof n); assert(!n[0]);
    col_nome_fonte(&s, "streaming_netflix_movies", n, sizeof n); assert(!n[0]);
    col_nome_fonte(&s, "outro_id_cru", n, sizeof n); assert(!n[0]);
    col_nome_fonte(&s, "Netflix", n, sizeof n); assert(!strcmp(n, "Netflix"));
    snprintf(s.title, sizeof s.title, "Originais");
    col_nome_fonte(&s, "Netflix", n, sizeof n); assert(!strcmp(n, "Originais"));
    snprintf(s.title, sizeof s.title, "mdblist.13914"); snprintf(s.catId, sizeof s.catId, "mdblist.13914");
    col_nome_fonte(&s, "Top 2024", n, sizeof n); assert(!strcmp(n, "Top 2024"));
    puts("ok  nome da aba: titulo da conta, senao manifesto, nunca id cru"); }

  puts("colecoes: tudo ok");
  return 0;
}
