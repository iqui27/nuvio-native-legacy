#include <assert.h>
#include <time.h>
#include "../src/homeassistiu.c"

static CatItem catalogo[24];
static unsigned versao = 1;
static uint64_t sessao = 1;
static int adulto = 1, ligado = 1, rede, nRegs;
static ProgRegistro regs[2];
static char visto[16];

int cat_n(void) { return sizeof catalogo / sizeof *catalogo; }
const CatItem *cat_item(int i) { return &catalogo[i]; }
unsigned cat_revisao_itens(void) { return versao; }
int cat_indice_por_imdb_tipo(const char *id, const char *tipo) {
  for (int i = 0; i < cat_n(); i++)
    if (!strcmp(id, catalogo[i].imdb) && !strcmp(tipo, catalogo[i].tipo)) return i;
  return -1;
}
int cat_historico_estado_tempo(const char *id, const char *tipo, long long *quando) {
  (void)tipo;
  if (strcmp(id, visto)) return -1;
  *quando = 20;
  return 1;
}
int cat_visto(const CatItem *c) { return !strcmp(visto, c->imdb); }
int prog_ler(ProgRegistro *saida, int max) {
  assert(max >= nRegs);
  memcpy(saida, regs, (size_t)nRegs * sizeof *regs);
  return nRegs;
}
uint64_t perfis_sessao_geracao(void) { return __atomic_load_n(&sessao, __ATOMIC_ACQUIRE); }
int perfis_sessao_atual(uint64_t g) { return g == perfis_sessao_geracao(); }
int farolsinal_recomendar(void) { return ligado; }
int perfil_infantil_ativo(void) { return !adulto; }
const char *desc_chave_tmdb(void) { return rede ? "fake-key" : ""; }
const char *desc_tmdb_idioma(void) { return "pt-BR"; }
int mapa_recomendacoes_tmdb(const char *id, long tmdb, int serie, const char *chave,
                            const char *idioma, long *ids, int max) {
  assert(!strcmp(id, "tt900") && !tmdb && !serie && max >= 1);
  assert(!strcmp(chave, "fake-key") && !strcmp(idioma, "pt-BR"));
  ids[0] = 108;
  return 1;
}

static void entrada(int i, int id, const char *gen, int nota) {
  CatItem *c = &catalogo[i];
  snprintf(c->imdb, sizeof c->imdb, "tt%d", id);
  snprintf(c->titulo, sizeof c->titulo, "Teste %d", id);
  snprintf(c->tipo, sizeof c->tipo, "movie");
  snprintf(c->genero, sizeof c->genero, "Filme · %s", gen);
  snprintf(c->poster, sizeof c->poster, "poster-%d", i);
  snprintf(c->classificacao, sizeof c->classificacao, "L");
  c->nota = nota;
}

int main(void) {
  int out[HOME_ASSISTIU_MAX];
  entrada(0, 900, "Drama", 60);
  entrada(1, 901, "Comédia", 99);
  entrada(2, 902, "Drama", 80);
  entrada(3, 902, "Drama", 70);
  entrada(4, 904, "Drama", 99);
  entrada(5, 905, "Drama", 99);
  entrada(6, 906, "Drama", 99);
  entrada(7, 900, "Drama", 99);
  entrada(8, 908, "Ação", 50);
  catalogo[8].tmdb = 108;
  catalogo[5].progresso = 22;
  catalogo[6].poster[0] = 0;
  strcpy(visto, "tt904");

  regs[0] = (ProgRegistro){ .contentId = "tt901", .tipo = "movie",
                          .durSeg = 100, .posSeg = 99, .tocou = 0 };
  regs[1] = (ProgRegistro){ .contentId = "tt900", .tipo = "movie",
                          .durSeg = 100, .posSeg = 91, .tocou = 1 };
  nRegs = 2;
  unsigned r = home_assistiu_passo(100);
  assert(home_assistiu_itens(out) == 1 && out[0] == 2);
  assert(home_assistiu_passo(101) == r);
  puts("ok  fallback local, sem vistos, em andamento, seek, cartaz ausente ou duplicatas");

  rede = 1;
  home_assistiu_passo(102);
  const struct timespec pausa = { .tv_nsec = 1000000 };
  for (int i = 0; i < 1000 && !__atomic_load_n(&remotoRev, __ATOMIC_ACQUIRE); i++)
    nanosleep(&pausa, NULL);
  home_assistiu_passo(103);
  assert(home_assistiu_itens(out) == 2 && out[0] == 8 && out[1] == 2);
  rede = 0;
  home_assistiu_passo(104);
  assert(home_assistiu_itens(out) == 1 && out[0] == 2);
  puts("ok  TMDB assincrono prioriza correspondencias; desligar restaura fallback");

  adulto = 0;
  strcpy(catalogo[2].classificacao, "16");
  strcpy(catalogo[3].classificacao, "16");
  home_assistiu_passo(105);
  assert(home_assistiu_itens(out) == 0);
  adulto = 1;
  nRegs = 0;
  strcpy(visto, "tt900");
  for (int i = 9; i < cat_n(); i++) entrada(i, 900 + i, "Drama", 60);
  versao++;
  home_assistiu_passo(106);
  assert(home_assistiu_itens(out) == HOME_ASSISTIU_MAX && out[0] == 4);
  puts("ok  restricao infantil, historico sem progresso e teto de 12");

  visto[0] = 0;
  __atomic_add_fetch(&sessao, 1, __ATOMIC_RELEASE);
  home_assistiu_passo(107);
  assert(home_assistiu_itens(out) == 0);
  puts("ok  trocar de perfil descarta a fileira anterior");
  return 0;
}
