// Regras puras das fontes de nota: escala nativa -> 0..100, texto, resumo,
// cores e o encaixe da linha do titulo. Sem GL.
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../src/notasfontes.c"

static void texto(int f, int cru, int vg, int longo, const char *esp) {
  char b[24];
  nf_texto(f, cru, vg, longo, b, sizeof b);
  if (strcmp(b, esp)) { fprintf(stderr, "texto(%d,%d): '%s' != '%s'\n", f, cru, b, esp); assert(0); }
}

int main(void) {
  // IMDb: 0..10 and 0..100 provider payloads both give 8.0; others unchanged.
  assert(nf_cru(EX_IMDB, 8.0) == 80 && nf_cru(EX_IMDB, 80.0) == 80);
  texto(EX_IMDB, nf_cru(EX_IMDB, 80.0), 0, 0, "8.0");
  texto(EX_IMDB, nf_cru(EX_IMDB, 8.0), 0, 0, "8.0");
  assert(nf_cru(EX_TRAKT, 80.0) == 800);

  int i;
  // --- normalizacao: cada escala nativa vira 0..100 -------------------------
  assert(nf_norm100(EX_IMDB, 78) == 78);          // 7.8 / 10
  assert(nf_norm100(EX_IMDB, 62) == 62);
  assert(nf_norm100(EX_TOMATOES, 870) == 87);     // 87%
  assert(nf_norm100(EX_TRAKT, 660) == 66);
  assert(nf_norm100(EX_METACRITIC, 720) == 72);
  assert(nf_norm100(EX_LETTERBOXD, 39) == 78);    // 3.9 / 5
  assert(nf_norm100(EX_LETTERBOXD, 50) == 100);
  assert(nf_norm100(EX_EBERT, 35) == 88);         // 3.5 / 4 (87.5 arredonda)
  assert(nf_norm100(EX_MAL, 86) == 86);
  assert(nf_norm100(EX_METAUSER, 74) == 74);
  // acima da escala nativa = a api mandou porcentagem
  assert(nf_norm100(EX_LETTERBOXD, 780) == 78);
  assert(nf_norm100(EX_EBERT, 880) == 88);
  assert(nf_norm100(EX_MAL, 860) == 86);
  // sem nota e valores absurdos
  assert(nf_norm100(EX_IMDB, 0) == 0);
  assert(nf_norm100(EX_IMDB, -5) == 0);
  assert(nf_norm100(99, 50) == 0);
  assert(nf_norm100(EX_TOMATOES, 1000) == 100);
  // monotonica em toda escala
  for (i = 0; i < EX_NFONTES; i++) {
    int c, ant = 0;
    for (c = 1; c <= 1000; c++) {
      int n = nf_norm100(i, c);
      assert(n >= 0 && n <= 100);
      // so a passagem da escala nativa para "porcentagem" pode recuar
      if (c != nf_escala_max(i) * 10 + nf_escala_max(i) / 2 + 1 || nf_escala_max(i) == 100)
        assert(n >= ant || nf_escala_max(i) != 100);
      ant = n;
    }
  }

  // --- texto na escala nativa -------------------------------------------------
  texto(EX_IMDB, 78, 0, 0, "7.8");
  texto(EX_IMDB, 78, 1, 0, "7,8");
  texto(EX_TOMATOES, 870, 0, 0, "87%");
  texto(EX_TRAKT, 664, 0, 0, "66%");
  texto(EX_METACRITIC, 720, 0, 0, "72");
  texto(EX_MDBSCORE, 815, 0, 0, "82");
  texto(EX_LETTERBOXD, 39, 0, 0, "3.9");
  texto(EX_LETTERBOXD, 39, 0, 1, "3.9/5");
  texto(EX_EBERT, 35, 1, 1, "3,5/4");
  texto(EX_LETTERBOXD, 780, 0, 1, "78%");           // cru fora da escala
  texto(EX_IMDB, 0, 0, 0, "");

  // --- posicoes e prioridades: permutacoes completas ----------------------------
  { int vistoP[EX_NFONTES] = {0}, vistoR[EX_NFONTES] = {0};
    for (i = 0; i < EX_NFONTES; i++) {
      int p = nf_posicao(i), r = nf_prioridade(i);
      assert(p >= 0 && p < EX_NFONTES && !vistoP[p]); vistoP[p] = 1;
      assert(r >= 0 && r < EX_NFONTES && !vistoR[r]); vistoR[r] = 1;
      assert(nf_na_posicao(p) == i);
    } }
  // a linha de fabrica de hoje: IMDb, Rotten Tomatoes, Trakt, nessa ordem
  assert(nf_posicao(EX_IMDB) < nf_posicao(EX_TOMATOES));
  assert(nf_posicao(EX_TOMATOES) < nf_posicao(EX_TRAKT));
  assert(nf_padrao_titulo(EX_IMDB) && nf_padrao_titulo(EX_TOMATOES) && nf_padrao_titulo(EX_TRAKT));
  assert(!nf_padrao_titulo(EX_TMDB) && !nf_padrao_titulo(EX_MAL) && !nf_padrao_titulo(EX_AUDIENCE));
  assert(!nf_precisa_mdblist(EX_IMDB) && !nf_precisa_mdblist(EX_TRAKT));
  assert(nf_precisa_mdblist(EX_TOMATOES) && nf_precisa_mdblist(EX_MAL));
  // IMDb e a ultima a sair
  assert(nf_prioridade(EX_IMDB) == 0);

  // --- cores --------------------------------------------------------------------
  { float r, g, b, t, r2, g2, b2;
    // a rampa cresce em luminancia (uniforme perceptual): monotonica
    float lumAnt = -1.0f;
    for (i = 0; i <= 100; i++) {
      nf_viridis(i / 100.0f, &r, &g, &b);
      { float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        assert(lum >= lumAnt - 1e-4f); lumAnt = lum; }
      assert(r >= 0 && r <= 1 && g >= 0 && g <= 1 && b >= 0 && b <= 1);
    }
    nf_viridis(-3.0f, &r, &g, &b);  nf_viridis(0.0f, &r2, &g2, &b2);
    assert(r == r2 && g == g2 && b == b2);
    nf_viridis(9.0f, &r, &g, &b);   nf_viridis(1.0f, &r2, &g2, &b2);
    assert(r == r2 && g == g2 && b == b2);
    // numero legivel: claro no escuro, escuro no amarelo
    nf_cor_nota(30, 30, &r, &g, &b, &t); assert(t == 1.0f);
    nf_cor_nota(100, 30, &r, &g, &b, &t); assert(t == 0.0f);
    // piso: tudo abaixo trava na mesma cor
    nf_cor_nota(5, 30, &r, &g, &b, &t); nf_cor_nota(30, 30, &r2, &g2, &b2, &t);
    assert(r == r2 && g == g2 && b == b2);
    // metacritic: as tres faixas do site
    nf_cor_metacritic(61, &r, &g, &b); assert(g > r && g > b);
    nf_cor_metacritic(40, &r, &g, &b); assert(r == 1.0f && g > 0.7f);
    nf_cor_metacritic(39, &r, &g, &b); assert(r == 1.0f && g == 0.0f);
    // episodio
    assert(nf_cor_episodio(0, &r, &g, &b) == 0);
    assert(nf_cor_episodio(85, &r, &g, &b) == 1);
    nf_cor_episodio(30, &r, &g, &b); nf_cor_episodio(50, &r2, &g2, &b2);
    assert(r == r2 && g == g2 && b == b2);
  }

  // --- resumo -----------------------------------------------------------------
  { int f[5] = { EX_IMDB, EX_TOMATOES, EX_AUDIENCE, EX_METACRITIC, EX_MDBSCORE };
    int n[5] = { 78, 90, 70, 62, 40 };
    NfResumo r;
    nf_resumo(f, n, 5, &r);
    assert(r.n == 4);                     // a agregada nao entra
    assert(r.media == 75);                // (78+90+70+62)/4 = 75
    assert(r.min == 62 && r.fonteMin == EX_METACRITIC);
    assert(r.max == 90 && r.fonteMax == EX_TOMATOES);
    assert(r.criticos == 76 && r.publico == 74);   // (90+62)/2 e (78+70)/2
    assert(r.temDiff && r.diff == -2);
    // so critica
    { int f2[1] = { EX_TOMATOES }, n2[1] = { 80 };
      nf_resumo(f2, n2, 1, &r);
      assert(r.n == 1 && r.media == 80 && r.criticos == 80 && r.publico == -1 && !r.temDiff); }
    // vazio
    nf_resumo(f, n, 0, &r);
    assert(r.n == 0 && r.media == 0 && !r.temDiff);
    // notas zeradas sao ignoradas
    { int f3[2] = { EX_IMDB, EX_TMDB }, n3[2] = { 0, 60 };
      nf_resumo(f3, n3, 2, &r); assert(r.n == 1 && r.media == 60); } }

  // --- encaixe da linha ---------------------------------------------------------
  { float w[5] = { 100, 100, 100, 100, 100 };
    int p[5] = { 0, 1, 4, 3, 2 };
    unsigned char m[5];
    // cabe tudo
    assert(nf_encaixar(w, p, 5, 500, m) == 5);
    // aperto: some quem tem MAIOR numero de prioridade (indice 2, depois 3)
    assert(nf_encaixar(w, p, 5, 450, m) == 4 && !m[2] && m[0] && m[1] && m[3] && m[4]);
    assert(nf_encaixar(w, p, 5, 350, m) == 3 && !m[2] && !m[3]);
    // nunca estoura: com espaco para menos que um item, nenhum
    assert(nf_encaixar(w, p, 5, 99, m) == 0);
    assert(nf_encaixar(w, p, 0, 10, m) == 0);
    // largura desigual: tira pela prioridade, nao pelo tamanho
    { float w2[3] = { 300, 40, 40 }; int p2[3] = { 0, 1, 2 };
      assert(nf_encaixar(w2, p2, 3, 345, m) == 2 && m[0] && m[1] && !m[2]); }
    // invariante: soma dos mantidos <= disp, para varios disp
    { float d;
      for (d = 0; d < 520; d += 7.5f) {
        float s = 0; int k, q = nf_encaixar(w, p, 5, d, m), c = 0;
        for (k = 0; k < 5; k++) if (m[k]) { s += w[k]; c++; }
        assert(s <= d + 0.001f && c == q);
      } } }

  // --- grade de episodios ---------------------------------------------------------
  { float cw, ch;
    assert(nf_grade_celula(2, 13, 1700, 400, &cw, &ch) && cw == 64.0f && ch == 34.0f);
    assert(nf_grade_celula(27, 25, 1700, 500, &cw, &ch));
    assert(cw <= 64.0f && ch >= 8.0f && 27 * ch + 26 * 4 <= 500.01f);
    assert(!nf_grade_celula(64, 30, 1700, 200, &cw, &ch));    // nem o piso cabe
    assert(!nf_grade_celula(0, 10, 1700, 400, &cw, &ch));
    assert(nf_grade_celula(1, 30, 700, 400, &cw, &ch) && 30 * cw + 29 * 4 <= 700.01f); }

  puts("notasfontes: PASS (escalas, texto, ordem, cores, resumo, encaixe, grade)");
  return 0;
}
