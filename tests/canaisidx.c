// #37: canal (FrostView, tipo Stremio "channel") clicava e abria OUTRO canal,
// com "source error" na sequencia. A causa mora em mesmoTitulo() (catalogo.c):
// ela compara ids so ATE O PRIMEIRO ':', regra pensada para "tt1234567:2:1"
// (temporada:episodio). Id de canal e "cs:channel:<hash>" — o ':' cai logo
// depois do prefixo de DUAS letras, entao TODO canal truncava para "cs" e
// cat_indice_por_imdb() devolvia sempre o PRIMEIRO canal do catalogo,
// nao importa qual foi clicado.
//
// Este teste prova as duas pontas exigidas pelo relato:
//   (a) dois canais com ids DISTINTOS, sem prefixo "tt" (logo sem a regra de
//       temporada:episodio), NAO colidem em cat_indice_por_imdb — cada um
//       acha o SEU proprio indice.
//   (b) o comportamento de id de IMDb com sufixo temporada:episodio, que a
//       funcao existe para servir, continua intacto.
//
// So src/catalogo.c e linkado, com dubles para o resto — mesma razao de
// tests/catcache.c e tests/catordem.c: cache e progresso nao interessam aqui.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/catalogo.h"
#include "../src/progresso.h"

int ajustes_idioma_ingles(void) { return 0; }
const char *i18n(const char *s) { return s; }
const char *dados_dir(void)     { return "/tmp"; }
const char *sessao_usuario(void){ return "teste"; }
int         perfis_ativo(void)  { return 1; }
const char *desc_genero_pt(const char *g) { return g; }
int prog_ler(ProgRegistro *saida, int max) { (void)saida; (void)max; return 0; }
int prog_gravar_local(const char *imdb, int t, int e, double p, double d) {
  (void)imdb; (void)t; (void)e; (void)p; (void)d; return 0;
}

static int falhas;
static void confere(const char *o_que, int ok) {
  printf("  %-66s %s\n", o_que, ok ? "ok" : "FALHOU");
  if (!ok) falhas++;
}

static void semear(CatItem *itens, int n, const char *imdbs[]) {
  CatFileira fil;
  int i;
  memset(itens, 0, sizeof(CatItem) * (size_t)n);
  memset(&fil, 0, sizeof fil);
  for (i = 0; i < n; i++) {
    snprintf(itens[i].imdb, sizeof itens[i].imdb, "%s", imdbs[i]);
    snprintf(itens[i].tipo, sizeof itens[i].tipo, "channel");
    snprintf(itens[i].titulo, sizeof itens[i].titulo, "Canal %d", i);
  }
  snprintf(fil.titulo, sizeof fil.titulo, "Canais");
  fil.n = n;
  cat_definir_tudo(itens, n, &fil, 1);
}

int main(void) {
  CatItem itens[4];
  // Tres canais do MESMO addon FrostView: prefixo "cs:channel:" identico,
  // hash distinto depois. E exatamente a forma medida no manifesto real.
  // imdb[16] no catalogo (14 uteis + NUL): "cs:channel:" ja leva 11, sobra so
  // 1 byte para diferenciar — o suficiente para provar a colisao, que e
  // exatamente onde ela mordia.
  const char *canais[] = {
    "cs:channel:a",
    "cs:channel:b",
    "cs:channel:c",
  };
  semear(itens, 3, canais);

  int i0 = cat_indice_por_imdb(canais[0]);
  int i1 = cat_indice_por_imdb(canais[1]);
  int i2 = cat_indice_por_imdb(canais[2]);
  confere("canal 0 acha o proprio indice", i0 == 0);
  confere("canal 1 acha o proprio indice (nao o 0)", i1 == 1);
  confere("canal 2 acha o proprio indice (nao o 0 nem o 1)", i2 == 2);
  confere("os tres indices sao DIFERENTES entre si",
          i0 != i1 && i1 != i2 && i0 != i2);

  // A regra de temporada:episodio, que mesmoTitulo() existe para servir,
  // continua funcionando para id de IMDb de verdade.
  {
    CatItem series[2];
    const char *ids[] = { "tt1234567:2:1", "tt7654321:1:1" };
    semear(series, 2, ids);
    int achado = cat_indice_por_imdb("tt1234567");
    confere("id de IMDb ainda casa sem o sufixo temporada:episodio",
            achado == 0);
  }

  if (falhas) { printf("FALHOU: %d checagem(ns)\n", falhas); return 1; }
  puts("PASS: canais com ids distintos nao colidem em cat_indice_por_imdb; "
       "id de IMDb com temporada:episodio continua casando.");
  return 0;
}
