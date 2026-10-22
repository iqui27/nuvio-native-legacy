// A arte-chave que a busca da Apple ja traz (trailerapple.c, escolherNaBusca),
// contra a resposta real de /search?searchTerm=Dune%20Part%20Two (23/09).
#define static
#include "../src/trailerapple.c"
#undef static
#include <assert.h>

char *dados_caminho(char *dst, unsigned tam, const char *nome) { (void)nome; if (tam) dst[0] = 0; return NULL; }
void dados_marcar_sujo(int leve) { (void)leve; }
int ajustes_trailer_qualidade(void) { return 0; }
static const char *resposta;
static int pedidos, falhas;
char *rede_baixar_com(const char *u, int s, const char *const *c) {
  (void)u; (void)s; (void)c; pedidos++;
  return resposta ? strdup(resposta) : NULL;
}
static void check(int ok, const char *msg) {
  printf("%s %s\n", ok ? "PASS" : "FAIL", msg);
  if (!ok) falhas++;
}

int main(int argc, char **argv) {
  char c[600], id[80], arte[600];
  FILE *f;
  long n;
  char *b;
  snprintf(c, sizeof c, "%s/apple_busca_dune_part_two.json", argc > 1 ? argv[1] : "tests/fixtures/arte");
  f = fopen(c, "rb");
  assert(f);
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)n + 1);
  assert(fread(b, 1, (size_t)n, f) == (size_t)n); b[n] = 0; fclose(f);
  // O mesmo filme aparece em duas prateleiras: continua sendo UM candidato.
  assert(escolherNaBusca(b, "Dune: Part Two", 2024, 0, id, sizeof id, arte, sizeof arte) == 1);
  assert(!strcmp(id, "umc.cmc.363aycnv6vy9qgekvew6fveb9"));
  assert(!strcmp(arte, "https://is1-ssl.mzstatic.com/image/thumb/CjKP9J_FPtPin1VBVU4-mg/{w}x{h}.{f}"));
  printf("ok  apple: Dune: Part Two 2024 -> shelfImageBackground\n");
  // Dois "Dune": o ano separa (2021 x 1984), e cada um tem a sua arte.
  assert(escolherNaBusca(b, "Dune", 2021, 0, id, sizeof id, arte, sizeof arte) == 1);
  assert(strstr(arte, "oYA7X4qfGYv2Mgr1tqNlzg"));
  assert(escolherNaBusca(b, "Dune", 1984, 0, id, sizeof id, arte, sizeof arte) == 1);
  assert(strstr(arte, "3PLzKi40X457Kce0jDUi8Q"));
  printf("ok  apple: mesmo nome, o ano escolhe a arte certa\n");
  // Serie com o mesmo nome nao casa com filme; ano errado nao casa.
  assert(escolherNaBusca(b, "Dune: Part Two", 2024, 1, id, sizeof id, arte, sizeof arte) == 0 && !arte[0]);
  assert(escolherNaBusca(b, "Dune: Part Two", 2019, 0, id, sizeof id, arte, sizeof arte) == 0);
  printf("ok  apple: tipo e ano errados nao inventam arte\n");
  // Homonimos sinteticos: Movie/Season nao sao a serie; dois Shows sao ambiguos.
  const char *homonimos = "{\"shelves\":[{\"items\":["
    "{\"type\":\"Movie\",\"id\":\"filme\",\"title\":\"Silo\",\"releaseDate\":1672531200000},"
    "{\"type\":\"Season\",\"id\":\"temporada\",\"title\":\"Silo\",\"releaseDate\":1672531200000},"
    "{\"type\":\"Show\",\"id\":\"serie\",\"title\":\"Silo\",\"releaseDate\":1672531200000}]}]}";
  check(escolherNaBusca(homonimos, "Silo", 2023, 1, id, sizeof id, arte, sizeof arte) == 1 && !strcmp(id, "serie"),
        "Silo: Show vence Movie/Season homonimos");
  const char *ambiguos = "{\"shelves\":[{\"items\":["
    "{\"type\":\"Show\",\"id\":\"serie1\",\"title\":\"Silo\",\"releaseDate\":1672531200000},"
    "{\"type\":\"Show\",\"id\":\"serie2\",\"title\":\"Silo\",\"releaseDate\":1672531200000}]}]}";
  check(escolherNaBusca(ambiguos, "Silo", 2023, 1, id, sizeof id, arte, sizeof arte) == 0,
        "Silo: dois Shows do mesmo ano recusados");
  // Transporte simulado; exercita o resolvedor publico, nao so o seletor.
  resposta = b; pedidos = 0;
  check(trailerapple_arte("tt15239678", "Dune: Part Two", 2024, 1, arte, sizeof arte) == 0 && pedidos == 1,
        "arte nao tenta filme quando o pedido e serie");
  check(trailerapple_arte("tt15239678", "Dune: Part Two", 2024, 0, arte, sizeof arte) == 1,
        "cache negativo de serie nao bloqueia filme");
  check(trailerapple_arte("tt15239678", "Dune: Part Two", 2019, 0, arte, sizeof arte) == 0,
        "cache por IMDb nao ignora ano do pedido");
  check(trailerapple_arte("tt15239678", "Dune: Part Two", 2024, 1, arte, sizeof arte) == 0,
        "cache positivo de filme nao fornece arte para serie");
  free(b);
  puts(falhas ? "trailerapple-arte: FALHOU" : "trailerapple-arte: tudo ok");
  return falhas ? 1 : 0;
}
