"""Execute the real episode footer with every independent source toggle."""
import pathlib
import subprocess
import tempfile

source = pathlib.Path("src/detail.c").read_text()
start = source.index("    int nota = 0;", source.index("static void desenhaEpisodio("))
block = source[start:source.index("    // A DATA", start)]
with tempfile.TemporaryDirectory() as directory:
    path = pathlib.Path(directory)
    (path / "footer.c").write_text("""
#include <assert.h>
#include <stddef.h>
enum { EX_TRAKT, EX_IMDB, EX_TMDB };
typedef struct { int temporada, episodio, nota, notaImdb; } CatEp;
static int enabled[3], drawn[3];
static int ajustes_nota_episodio(int source) { return enabled[source]; }
static int extras_n_temporadas(void) { return 1; }
static int extras_temporada_numero(int st) { return 2; }
static int extras_n_eps(int st) { return 1; }
static int extras_ep_numero(int st, int ei) { return 3; }
static int extras_ep_nota(int st, int ei) { return 81; }
static float desenhaNotaEpisodio(float x, float y, int source, int rating, float a) {
  assert(rating == (source == EX_IMDB ? 92 : source == EX_TMDB ? 76 : 81));
  drawn[source]++;
  return 40;
}
static void footer(const CatEp *ep) {
  float x = 0, y = 0, a = 1;
BLOCK
}
int main(void) {
  CatEp ep = {2, 3, 76, 92};
  for (int mask = 0; mask < 8; mask++) {
    for (int f = 0; f < 3; f++) { enabled[f] = !!(mask & (1 << f)); drawn[f] = 0; }
    footer(&ep);
    for (int f = 0; f < 3; f++) assert(drawn[f] == enabled[f]);
  }
  ep.nota = ep.notaImdb = 0;
  for (int f = 0; f < 3; f++) { enabled[f] = 1; drawn[f] = 0; }
  footer(&ep);
  assert(!drawn[EX_IMDB] && !drawn[EX_TMDB]);
  ep.temporada = 9;
  for (int f = 0; f < 3; f++) drawn[f] = 0;
  footer(&ep);
  assert(!drawn[EX_TRAKT]);
  footer(NULL);
  assert(!drawn[EX_IMDB] && !drawn[EX_TMDB] && !drawn[EX_TRAKT]);
}
""".replace("BLOCK", block))
    subprocess.run(["cc", str(path / "footer.c"), "-o", str(path / "footer")], check=True)
    subprocess.run([str(path / "footer")], check=True)
print("episode IMDb/TMDB/Trakt source independence: PASS")
