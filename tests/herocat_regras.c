// tests/herocat_regras.sh: sugeridos e filtro do seletor "Catálogos do destaque".
#include "../src/herocat_regras.h"
static int falhas;
#define OK(c, m) do { if (c) printf("ok  %s\n", m); else { printf("FALHOU %s\n", m); falhas++; } } while (0)
int main(void) {
  const char *ch[] = { "cin_movie_top", "trakt_trending_movies", "cin_series_last", "x_upcoming", "y_desktop", "z_popular",
                       "w_novidades", "q_em_alta", "k_new", "m_plain", "n_top_series" };
  const char *ti[] = { "Popular", "Trending Movies", "Latest", "Upcoming", "Desktop", "Top Rated",
                       "Lançamentos", "Em Alta", "New", "Plain", "Top Séries" };
  int out[HC_SUG_MAX], n, i;
  OK(hc_classe("a", "Tendências da semana") == HC_TRENDING, "tendencias = trending");
  OK(hc_classe("a", "Mais vistos") == HC_TOP, "mais vistos = top");
  OK(hc_classe("a", "Desktop") == -1, "desktop nao e top");
  OK(hc_classe("a", "Em breve") == HC_ANTICIPATED, "em breve = anticipated");
  OK(hc_classe("tmdb_new_releases", "X") == HC_LATEST, "id tambem casa");
  n = hc_sugeridos(11, ch, ti, "", out, HC_SUG_MAX);
  OK(n == HC_SUG_MAX, "teto de sugeridos");
  OK(out[0] == 1 && out[1] == 7, "trending primeiro (Trending, Em Alta)");
  OK(hc_classe(ch[out[2]], ti[out[2]]) == HC_LATEST, "depois latest");
  for (i = 1; i < n; i++) OK(hc_classe(ch[out[i-1]], ti[out[i-1]]) <= hc_classe(ch[out[i]], ti[out[i]]), "ordem por classe");
  n = hc_sugeridos(11, ch, ti, "trakt_trending_movies", out, HC_SUG_MAX);
  OK(out[0] == 7, "ja escolhido sai dos sugeridos");
  n = hc_sugeridos(1, ch + 4, ti + 4, "", out, HC_SUG_MAX);
  OK(n == 0, "nenhum casa = vazio");
  OK(hc_filtro("", "Qualquer", "Addon"), "busca vazia casa tudo");
  OK(hc_filtro("lancamentos", "Lançamentos", "Cinemeta"), "sem acento");
  OK(hc_filtro("CINE top", "Top Rated", "Cinemeta"), "palavras no titulo+addon");
  OK(!hc_filtro("zzz", "Top", "Cinemeta"), "sem resultado");
  return falhas ? 1 : 0;
}
