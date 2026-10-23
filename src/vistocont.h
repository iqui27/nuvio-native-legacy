// "VOCE VIU N DE M" da filmografia. Puro: quem chama diz, por credito, o que as
// fontes (Nuvio, Trakt, Simkl) ja concluiram em cat_visto; aqui so se deduplica
// o mesmo titulo que chegou por ids diferentes (tt, tmdb, id de episodio).
#ifndef NV_VISTOCONT_H
#define NV_VISTOCONT_H

typedef struct {
  const char *imdb;   // "tt123", "tt123:1:2" ou vazio
  long        tmdb;   // 0 = desconhecido
  const char *tipo;   // "movie" | "series"
  int         visto;  // 1 = alguma fonte diz visto
} VcEntrada;

// Devolve M (titulos distintos) e *vistos = N. Mesmo titulo = mesmo imdb base
// OU mesmo (tmdb, tipo); entrada sem identidade conta por si so.
int vistocont_contar(const VcEntrada *e, int n, int *vistos);
// 0..100, arredondado para baixo; total 0 devolve 0.
int vistocont_pct(int vistos, int total);

#endif
