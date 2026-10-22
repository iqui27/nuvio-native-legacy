// #255: conta com 7 colecoes e ~300 pastas (o Xperience do relator: "Rows 20,
// Collections 7, Folders 296, Sources 598" na foto do configurador) e uma
// ordem de fileiras com mais de 768 itens, quase todos desligados. O log 2.0.0
// dele dizia "1 alem do teto de 256", "256 pastas vindas da conta" e
// "768 na ordem da conta, 700 desligadas" — os dois tetos batidos.
//
// Monta a Home de verdade (home.c via o arnes de cwordem_home.c), sem rede.
#define main cwordem_home_fixture_main
#include "cwordem_home.c"
#undef main
#include "../src/colfileiras.h"
#include <stdarg.h>

Uint32 SDL_GetTicks(void) { return 1000; }
int addons_n(void) { return 0; }
int addons_base_desligada(const char *b) { (void)b; return 0; }   // 203-desligados
const char *addons_base(int i) { (void)i; return ""; }
const char *addons_id_manifesto(int i) { (void)i; return ""; }
const char *sessao_usuario(void) { return "account-255"; }

#define ADDON "app.xperience.t"
// Base com o tamanho da do Xperience (~370 caracteres, JWT no caminho).
static char base[400];

static char *buf; static size_t nBuf, capBuf;
static void put(const char *fmt, ...) {
  va_list ap; va_start(ap, fmt);
  char tmp[1024]; int n = vsnprintf(tmp, sizeof tmp, fmt, ap); va_end(ap);
  assert(n > 0 && (size_t)n < sizeof tmp);
  if (nBuf + (size_t)n + 1 > capBuf) {
    capBuf = (capBuf + (size_t)n + 1) * 2; buf = realloc(buf, capBuf); assert(buf);
  }
  memcpy(buf + nBuf, tmp, (size_t)n + 1); nBuf += (size_t)n;
}

// Colecoes na ordem da conta. A ultima e "Directors", com Wes Anderson e Tim
// Burton: as fileiras soltas da foto da TV do relator.
static const char *NOMES[] = { "Streaming", "Genres", "Awards", "Decades", "Studios", "Moods", "Directors" };
static const int PASTAS[] = { 40, 60, 40, 50, 50, 26, 30 };   // 296
#define NCOL 7
static const char *DIRETORES[] = { "Wes Anderson", "Tim Burton", "Quentin Tarantino", "Christopher Nolan" };

static void catId(char *d, size_t n, int c, int p, const char *tipo) {
  snprintf(d, n, "c%d_f%d_%s", c + 1, p, tipo);
}

static int fontesDa(int c, int p) { return (c == 1 && p < 6) ? 3 : 2; }   // 598

static void montarColecoes(int nCol) {
  nBuf = 0; put("{\"collections\":[");
  for (int c = 0; c < nCol; c++) {
    put("%s{\"id\":\"c%d\",\"title\":\"%s\",\"folders\":[", c ? "," : "", c + 1, NOMES[c]);
    for (int p = 0; p < PASTAS[c]; p++) {
      char titulo[64];
      if (c == 6 && p < 4) snprintf(titulo, sizeof titulo, "%s", DIRETORES[p]);
      else snprintf(titulo, sizeof titulo, "%s %d", NOMES[c], p + 1);
      put("%s{\"id\":\"c%d_f%d\",\"title\":\"%s\",\"coverImageUrl\":\"https://cdn.invalid/c%d/f%d.webp\",\"sources\":[",
          p ? "," : "", c + 1, p, titulo, c + 1, p);
      static const char *TIPOS[] = { "movie", "series", "anime" };
      for (int s = 0; s < fontesDa(c, p); s++) {
        char id[64]; catId(id, sizeof id, c, p, TIPOS[s]);
        put("%s{\"provider\":\"addon\",\"addonId\":\"" ADDON "\",\"addonBaseUrl\":\"%s\",\"type\":\"%s\",\"catalogId\":\"%s\",\"title\":\"%s - %s\"}",
            s ? "," : "", base, s == 1 ? "series" : "movie", id, titulo, TIPOS[s]);
      }
      put("]}");
    }
    put("]}");
  }
  put("]}");
}

// A ordem da conta: as 20 fileiras e as 7 colecoes ligadas primeiro (o
// "Arrange home 27 items"), depois os catalogos de pasta e outros, todos
// DESLIGADOS — 830 itens, acima dos 768 que o 2.0.0 lia. Os de "Directors"
// ficam no fim, depois do item 768.
static int nItens;
static void montarOrdem(void) {
  int ordem = 0;
  nBuf = 0; put("{\"items\":[");
  for (int r = 0; r < 20; r++)
    put("%s{\"addon_id\":\"" ADDON "\",\"type\":\"movie\",\"catalog_id\":\"row%d\",\"enabled\":true,\"order\":%d}",
        ordem ? "," : "", r, ordem), ordem++;
  // Colecoes em ordem DIFERENTE da do array da conta: a Home tem de seguir esta.
  for (int c = NCOL - 1; c >= 0; c--)
    put(",{\"is_collection\":true,\"collection_id\":\"c%d\",\"enabled\":true,\"order\":%d}", c + 1, ordem), ordem++;
  for (int k = 0; k < 205; k++)
    put(",{\"addon_id\":\"" ADDON "\",\"type\":\"movie\",\"catalog_id\":\"extra%d\",\"enabled\":false,\"order\":%d}", k, ordem), ordem++;
  for (int c = 0; c < NCOL; c++)
    for (int p = 0; p < PASTAS[c]; p++)
      for (int s = 0; s < fontesDa(c, p); s++) {
        static const char *TIPOS[] = { "movie", "series", "anime" };
        char id[64]; catId(id, sizeof id, c, p, TIPOS[s]);
        put(",{\"addon_id\":\"" ADDON "\",\"type\":\"%s\",\"catalog_id\":\"%s\",\"enabled\":false,\"order\":%d}",
            s == 1 ? "series" : "movie", id, ordem), ordem++;
      }
  put("]}");
  nItens = ordem;
}

// A pergunta que descoberta.c faz a cada catalogo declarado (desligada() e
// dentroDeColecaoVisivelBase, que e static la): engolido por colecao visivel
// ou desligado pela conta. Nenhum dos dois = fileira solta na Home.
static int solta(int c, int p, int s) {
  static const char *TIPOS[] = { "movie", "series", "anime" };
  char id[64], chave[192];
  const char *tipo = s == 1 ? "series" : "movie";
  catId(id, sizeof id, c, p, TIPOS[s]);
  snprintf(chave, sizeof chave, ADDON "_%s_%s", tipo, id);
  const ColFolder *f = col_por_catalogo(base, tipo, id);
  if (f) {
    char g[192]; col_chave_pasta(f, g, sizeof g);
    if (!fil_oculta(g) && !catordem_oculta(g, g)) return 0;
  }
  return !catordem_oculta(chave, chave);
}

int main(void) {
  memset(base, 'x', sizeof base - 1);
  memcpy(base, "https://xp.invalid/", 19);
  base[370] = 0;
  fil_definir_limite(40);
  ajustes_aplicar_blob("{\"continueWatchingEnabled\":true}");

  montarOrdem();
  catordem_ler(buf);
  int lidos = catordem_n();
  printf("[teste] ordem: %d itens na conta, %d lidos\n", nItens, lidos);

  montarColecoes(NCOL);
  printf("[teste] colecoes: %zu bytes de JSON\n", nBuf);
  int recebidas = colfileiras_receber(buf);
  colfileiras_sincronizar();

  // Toda pasta da conta, de toda colecao, com as fontes inteiras.
  int fontes = 0, diretores, idx[64];
  for (int i = 0; i < col_n(); i++) fontes += col_folder(i)->nSources;
  diretores = col_grupo_chave("collection_c7", idx, 64);
  printf("[teste] pastas=%d fontes=%d pastas de Directors=%d\n", col_n(), fontes, diretores);

  // Nenhum catalogo de pasta vira fileira solta (Wes Anderson / Tim Burton).
  int soltas = 0;
  for (int c = 0; c < NCOL; c++)
    for (int p = 0; p < PASTAS[c]; p++)
      for (int s = 0; s < fontesDa(c, p); s++)
        if (solta(c, p, s)) {
          if (!soltas) printf("[teste] primeira fonte solta: colecao %s, pasta %d\n", NOMES[c], p);
          soltas++;
        }
  printf("[teste] catalogos de pasta soltos na Home: %d\n", soltas);

  // As 7 colecoes na Home, na ordem da CONTA (c7 primeiro), nenhuma faltando.
  sincronizarFileiras();
  int naTela = 0, emOrdem = 1;
  for (int c = 0; c < NCOL; c++) {
    char k[32]; snprintf(k, sizeof k, "collection_c%d", c + 1);
    if (naHome(k)) naTela++;
    if (c && naHome(k)) { char a[32]; snprintf(a, sizeof a, "collection_c%d", c);
      if (naHome(a) && posicao(k) > posicao(a)) emOrdem = 0; }
  }
  printf("[teste] colecoes na Home: %d de %d, ordem da conta %s\n", naTela, NCOL, emOrdem ? "sim" : "nao");

  assert(nItens == 830 && lidos == nItens);
  assert(recebidas == 296 && col_n() == 296 && fontes == 598 && diretores == 30);
  assert(!strcmp(col_folder(idx[0])->title, "Wes Anderson"));
  assert(soltas == 0);
  assert(naTela == NCOL && emOrdem);
  // Um segundo pull igual nao muda nada (nem remonta a Home).
  { unsigned rev = col_revisao();
    assert(colfileiras_receber(buf) == 296 && col_revisao() == rev); }
  // UMA PASTA GUARDADA (vertudo guarda o endereco) sobrevive a um pull menor:
  // aponta para fontes zeradas, nao para o bloco liberado (ASan acusa se nao).
  { const ColFolder *guardada = col_folder(290);
    montarColecoes(3);
    assert(colfileiras_receber(buf) == 140 && col_n() == 140);
    assert(guardada->nSources == 0 && !guardada->sources[COL_SOURCE_MAX - 1].catId[0]); }

  // TETO DE SEGURANCA: uma colecao que nao cabe sai INTEIRA, a seguinte entra.
  { nBuf = 0; put("{\"collections\":[{\"id\":\"big\",\"title\":\"Gigante\",\"folders\":[");
    for (int p = 0; p < 140; p++) {
      put("%s{\"id\":\"g%d\",\"title\":\"G %d\",\"sources\":[", p ? "," : "", p, p);
      for (int k = 0; k < COL_SOURCE_MAX; k++)
        put("%s{\"addonId\":\"" ADDON "\",\"addonBaseUrl\":\"https://g.invalid\",\"type\":\"movie\",\"catalogId\":\"g%d_%d\"}",
            k ? "," : "", p, k);
      put("]}");
    }
    put("]},{\"id\":\"small\",\"title\":\"Pequena\",\"folders\":[{\"id\":\"s1\",\"title\":\"S\",\"sources\":["
        "{\"addonId\":\"" ADDON "\",\"addonBaseUrl\":\"https://g.invalid\",\"type\":\"movie\",\"catalogId\":\"s\"}]}]}]}");
    assert(colfileiras_receber(buf) == 1 && col_n() == 1);
    assert(!strcmp(col_folder(0)->group, "Pequena"));
    int idx[4]; assert(col_grupo_chave("collection_big", idx, 4) == 0); }

  puts("ok #255: 7 colecoes, 296 pastas e 598 fontes inteiras; ordem de 830 itens; nenhuma fonte de pasta solta");
  free(buf);
  return 0;
}
