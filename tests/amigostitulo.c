// O INDICE DE AMIGOS POR TITULO (amigostitulo.h), sem GL: dedupe por amigo,
// "gostou" antes de "so assistiu", teto, privacidade (so o que foi publicado) e
// as frases do destaque e da ilha.
#include "amigostitulo.h"
#include "idioma.h"
#include "ajustes.h"
#include "dados.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static SvEvento ev(const char *pid, const char *nome, int acao, int reacao, int nota,
                   const char *imdb, int t, int e) {
  SvEvento v;
  memset(&v, 0, sizeof v);
  snprintf(v.pessoaId, sizeof v.pessoaId, "%s", pid);
  snprintf(v.pessoaNome, sizeof v.pessoaNome, "%s", nome);
  v.acao = acao; v.reacao = reacao; v.nota = nota;
  snprintf(v.imdb, sizeof v.imdb, "%s", imdb);
  v.temporada = t; v.episodio = e; v.pct = -1; v.restanteMin = -1;
  return v;
}

int main(void) {
  AmigosTitulo a;
  char linha[200];
  SvEvento fe[40];
  int n = 0, i;
  char cam[600];
  FILE *f;

  // Frases em portugues: o idioma da interface vem do ajustes.txt da pasta temporaria.
  ajustes_iniciar();
  dados_iniciar("deploy/app/art");
  snprintf(cam, sizeof cam, "%s/ajustes.txt", dados_dir());
  f = fopen(cam, "w"); assert(f); fputs("idioma 0\n", f); fclose(f);
  ajustes_dir(dados_dir());
  assert(ajustes_idioma() == 0);

  // 1. Dedupe: o mesmo amigo com varios eventos aparece UMA vez; o episodio mais
  //    adiantado vence; reacao positiva faz "gostou" e "gostou" vem antes de "viu".
  fe[n++] = ev("nuvio:rafa", "rafa cine", SV_FIM, SV_REAC_NADA, 0, "tt1", 1, 3);
  fe[n++] = ev("nuvio:fabi", "fabi", SV_FIM, SV_REAC_NADA, 0, "tt1", 1, 8);
  fe[n++] = ev("nuvio:fabi", "fabi", SV_INICIO, SV_REAC_NADA, 0, "tt1", 1, 5);
  fe[n++] = ev("nuvio:mari", "Mari", SV_REACAO, SV_REAC_GOSTOU, 0, "tt1", 0, 0);
  fe[n++] = ev("nuvio:rafa", "rafa cine", SV_REACAO, SV_REAC_NAO, 0, "tt1", 0, 0);
  assert(amigostitulo_montar(fe, n) == 1);
  assert(amigostitulo_obter("tt1", &a));
  assert(a.total == 3 && a.n == 3 && a.nGostou == 1 && a.nViu == 2);
  assert(!strcmp(a.a[0].id, "nuvio:mari") && a.a[0].gostou);       // gostou primeiro
  assert(!strcmp(a.a[1].id, "nuvio:rafa") && !a.a[1].gostou && a.a[1].viu);   // reacao negativa so viu
  assert(!strcmp(a.a[2].id, "nuvio:fabi") && a.a[2].episodio == 8);  // o mais adiantado
  assert(!amigostitulo_obter("tt404", NULL));

  // 2. Nota de tracker: >= 70 gosta, abaixo so viu; salvo/abandono/mandou nao contam.
  n = 0;
  fe[n++] = ev("trakt:a", "Ana", SV_AVALIOU, SV_REAC_NADA, 70, "tt2", 0, 0);
  fe[n++] = ev("trakt:b", "Bia", SV_AVALIOU, SV_REAC_NADA, 60, "tt2", 0, 0);
  fe[n++] = ev("trakt:c", "Caio", SV_SALVO, SV_REAC_NADA, 0, "tt2", 0, 0);
  fe[n++] = ev("trakt:d", "Davi", SV_ABANDONO, SV_REAC_NADA, 0, "tt2", 0, 0);
  fe[n++] = ev("trakt:e", "Eva", SV_MANDOU, SV_REAC_NADA, 0, "tt2", 0, 0);
  fe[n++] = ev("trakt:f", "Fred", SV_ATIVIDADE, SV_REAC_NADA, 0, "tt3", 0, 0);   // titulo sem ninguem que conte
  amigostitulo_montar(fe, n);
  assert(amigostitulo_obter("tt2", &a));
  // Eva me MANDOU o titulo: entra, e na frente (recomendou), sem contar como "viu".
  assert(a.total == 3 && a.nGostou == 1 && a.nViu == 1 && a.nRecomendou == 1);
  assert(!strcmp(a.a[0].id, "trakt:e") && a.a[0].recomendou && !a.a[0].viu);
  assert(!strcmp(a.a[1].id, "trakt:a") && !strcmp(a.a[2].id, "trakt:b"));
  assert(!amigostitulo_obter("tt3", NULL));    // sem viu/gostou: nem entra no indice

  // 3. Teto: 12 amigos viram AMT_MAX guardados, mas `total` diz a verdade.
  n = 0;
  for (i = 0; i < 12; i++) {
    char id[32], nome[32];
    snprintf(id, sizeof id, "nuvio:%d", i); snprintf(nome, sizeof nome, "amigo%d", i);
    fe[n++] = ev(id, nome, SV_FIM, SV_REAC_NADA, 0, "tt4", 0, 0);
  }
  amigostitulo_montar(fe, n);
  assert(amigostitulo_obter("tt4", &a));
  assert(a.n == AMT_MAX && a.total == 12);

  // 4. Frases (sem traducao carregada, o texto e o da chave em portugues).
  n = 0;
  fe[n++] = ev("nuvio:fabi", "fabi cine", SV_REACAO, SV_REAC_GOSTOU, 0, "tt5", 0, 0);
  fe[n++] = ev("nuvio:rafa", "rafa", SV_FIM, SV_REAC_NADA, 0, "tt5", 1, 8);
  fe[n++] = ev("nuvio:mari", "Mari", SV_FIM, SV_REAC_NADA, 0, "tt5", 1, 2);
  amigostitulo_montar(fe, n);
  assert(amigostitulo_obter("tt5", &a));
  amigostitulo_linha_destaque(&a, linha, sizeof linha);
  assert(!strcmp(linha, "Fabi gostou  \xc2\xb7  Rafa e Mari assistiram"));
  amigostitulo_linha_ilha(&a, linha, sizeof linha);
  assert(!strcmp(linha, "Fabi gostou  \xc2\xb7  Rafa viu até o E8  \xc2\xb7  +1"));
  fe[n++] = ev("nuvio:leo", "leo", SV_FIM, SV_REAC_NADA, 0, "tt5", 0, 0);
  amigostitulo_montar(fe, n);
  amigostitulo_obter("tt5", &a);
  amigostitulo_linha_destaque(&a, linha, sizeof linha);
  assert(!strcmp(linha, "Fabi gostou  \xc2\xb7  Rafa, Mari e mais 1 assistiram"));

  // 6. A ILHA DA PAGINA DO TITULO: recomendou > opinioes > progresso > viu,
  //    e a lista de cada amigo (parou ha mais de 21 dias, terminou a serie).
  { const long long T0 = 2000000000LL;
    char l2[220], st[240], ex[240];
    n = 0;
    fe[n] = ev("nuvio:ana", "Ana", SV_MANDOU, SV_REAC_NADA, 0, "tt7", 0, 0);
    snprintf(fe[n].texto, sizeof fe[n].texto, "Imperdível"); n++;
    fe[n] = ev("nuvio:bia", "Bia", SV_REACAO, SV_REAC_GOSTOU, 0, "tt7", 2, 5); fe[n].quando = T0 - 3000; n++;
    fe[n] = ev("nuvio:bia", "Bia", SV_FIM, SV_REAC_NADA, 0, "tt7", 2, 5); fe[n].quando = T0 - 3600; n++;
    fe[n] = ev("nuvio:caio", "Caio", SV_AVALIOU, SV_REAC_NADA, 40, "tt7", 0, 0); fe[n].quando = T0 - 7200; n++;
    fe[n] = ev("nuvio:duda", "Duda", SV_FIM, SV_REAC_NADA, 0, "tt7", 1, 3); fe[n].quando = T0 - 30 * 86400LL; n++;
    fe[n] = ev("nuvio:edu", "Edu", SV_FIM, SV_REAC_NADA, 0, "tt7", 3, 10); fe[n].quando = T0 - 86400; n++;
    for (i = 0; i < n; i++) snprintf(fe[i].tipo, sizeof fe[i].tipo, "series");
    amigostitulo_montar(fe, n);
    assert(amigostitulo_obter("tt7", &a));
    assert(a.total == 5 && a.nRecomendou == 1 && a.nOpiniao == 2 && a.nGostou == 1 && a.nNaoGostou == 1);
    assert(!strcmp(a.a[0].id, "nuvio:ana") && !strcmp(a.a[1].id, "nuvio:bia") && !strcmp(a.a[2].id, "nuvio:caio"));
    amigostitulo_resumo(&a, 3, 10, T0, linha, sizeof linha, l2, sizeof l2);
    printf("ilha: %s | %s\n", linha, l2);
    assert(!strcmp(linha, "Ana recomendou para você"));
    assert(!strcmp(l2, "5 amigos: 1 gostou  \xc2\xb7  Bia: T2E5  \xc2\xb7  +2"));
    amigostitulo_frase_amigo(&a.a[0], 3, 10, T0, st, sizeof st, ex, sizeof ex);
    assert(!st[0] && !strcmp(ex, "Recomendou para você  \xc2\xb7  \xe2\x80\x9cImperdível\xe2\x80\x9d"));
    amigostitulo_frase_amigo(&a.a[1], 3, 10, T0, st, sizeof st, ex, sizeof ex);
    assert(!strcmp(st, "Gostou  \xc2\xb7  Viu até T2E5"));
    amigostitulo_frase_amigo(&a.a[2], 3, 10, T0, st, sizeof st, ex, sizeof ex);
    assert(!strcmp(st, "Nota 4/10"));
    amigostitulo_frase_amigo(&a.a[3], 3, 10, T0, st, sizeof st, ex, sizeof ex);
    printf("duda: %s\n", st);
    assert(!strcmp(st, "Parou no T1E3  \xc2\xb7  há 30 dias"));
    amigostitulo_frase_amigo(&a.a[4], 3, 10, T0, st, sizeof st, ex, sizeof ex);
    assert(!strcmp(st, "Terminou a série"));
    // Sem saber o ultimo episodio, "terminou a série" nunca e afirmado.
    amigostitulo_frase_amigo(&a.a[4], 0, 0, T0, st, sizeof st, ex, sizeof ex);
    assert(!strcmp(st, "Viu até T3E10"));
    // Sem recomendacao: as opinioes viram a manchete ("4 amigos: 1 gostou").
    amigostitulo_montar(fe + 1, n - 1);
    amigostitulo_obter("tt7", &a);
    amigostitulo_resumo(&a, 3, 10, T0, linha, sizeof linha, l2, sizeof l2);
    printf("ilha sem rec: %s | %s\n", linha, l2);
    assert(!strcmp(linha, "4 amigos: 1 gostou"));
    // So progresso: "Duda parou no T1E3".
    amigostitulo_montar(fe + 4, 1);
    amigostitulo_obter("tt7", &a);
    amigostitulo_resumo(&a, 3, 10, T0, linha, sizeof linha, l2, sizeof l2);
    assert(!strcmp(linha, "Duda parou no T1E3"));
    assert(!strcmp(l2, "Abrir para ver o que acharam")); }

  // 5. Sem amigos: nada.
  assert(amigostitulo_montar(NULL, 0) == 0);
  amigostitulo_linha_destaque(NULL, linha, sizeof linha);
  assert(!linha[0]);
  printf("amigostitulo: ok\n");
  return 0;
}
