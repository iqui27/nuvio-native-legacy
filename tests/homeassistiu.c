#include <assert.h>
#include <time.h>
#include "../src/homeassistiu.c"
#include "fileiras.h"

static CatItem catalogo[HIST_LOTE + 2 + HOME_ASSISTIU_MAX];
static int nCatalogo = 24, consultasHistorico;
static unsigned versao = 1;
static uint64_t sessao = 1;
static int perfil = 1;
static int adulto = 1, ligado = 1, rede, nRegs;
static ProgRegistro regs[2];
static char visto[16];
static long resposta = 108;
static int pedidos;
static const char *respostaJson;
static int lotes;

int cat_n(void) { return nCatalogo; }
const CatItem *cat_item(int i) { return &catalogo[i]; }
unsigned cat_revisao_itens(void) { return versao; }
int cat_acrescentar_lote(const CatItem *v, int qtd, int *saida) {
  assert(nCatalogo + qtd <= (int)(sizeof catalogo / sizeof *catalogo));
  for (int i = 0; i < qtd; i++) {
    saida[i] = nCatalogo;
    catalogo[nCatalogo++] = v[i];
  }
  lotes++;
  versao++;
  return qtd;
}
int cat_indice_por_imdb_tipo(const char *id, const char *tipo) {
  for (int i = 0; i < cat_n(); i++)
    if (!strcmp(id, catalogo[i].imdb) && !strcmp(tipo, catalogo[i].tipo)) return i;
  return -1;
}
int cat_historico_estado_tempo(const char *id, const char *tipo, long long *quando) {
  (void)tipo;
  consultasHistorico++;
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
int perfis_sessao_travar(uint64_t g) { return perfis_sessao_atual(g); }
void perfis_sessao_destravar(void) {}
int perfis_ativo(void) { return perfil; }
const char *sessao_usuario(void) { return "conta-teste"; }
int farolsinal_recomendar(void) { return ligado; }
int perfil_infantil_ativo(void) { return !adulto; }
const char *desc_chave_tmdb(void) { return rede ? "fake-key" : ""; }
const char *desc_tmdb_idioma(void) { return "pt-BR"; }
const char *i18n(const char *s) { return s; }
char *dados_caminho(char *dst, unsigned tam, const char *nome) {
  (void)dst; (void)tam; (void)nome; return NULL;
}
int dados_gravar(const char *nome, const char *conteudo) {
  (void)nome; (void)conteudo; return 1;
}
int mapa_recomendacoes_tmdb(const char *id, long tmdb, int serie, const char *chave,
                            const char *idioma, MapaRecomendacoes *saida) {
  assert(!strcmp(id, "tt900") && !tmdb && !serie);
  assert(!strcmp(chave, "fake-key") && !strcmp(idioma, "pt-BR"));
  pedidos++;
  if (respostaJson) return mapa_ler_recomendacoes(respostaJson, serie, saida) >= 0;
  memset(saida, 0, sizeof *saida);
  if (resposta) {
    saida->n = 1;
    saida->obras[0].obra.tmdb = resposta;
  }
  return resposta != 0;
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

static void aguardarResposta(unsigned anterior) {
  const struct timespec pausa = { .tv_nsec = 1000000 };
  for (int i = 0; i < 1000 && __atomic_load_n(&remotoRev, __ATOMIC_ACQUIRE) == anterior; i++)
    nanosleep(&pausa, NULL);
  assert(__atomic_load_n(&remotoRev, __ATOMIC_ACQUIRE) == anterior + 1);
}

static void conferirRotulo(const char *esperado) {
  char rotulo[96];
  home_assistiu_rotulo(rotulo, sizeof rotulo);
  assert(!strcmp(rotulo, esperado));
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
  conferirRotulo("Parecidos no seu catálogo");
  assert(home_assistiu_passo(101) == r);
  puts("ok  fallback local, sem vistos, em andamento, seek, cartaz ausente ou duplicatas");

  rede = 1;
  home_assistiu_passo(102);
  aguardarResposta(0);
  home_assistiu_passo(103);
  assert(home_assistiu_itens(out) == 2 && out[0] == 8 && out[1] == 2);
  conferirRotulo("Porque você assistiu Teste 900");
  strcpy(catalogo[0].titulo, "Semente do catálogo substituído");
  CatItem troca = catalogo[8];
  catalogo[8] = catalogo[2];
  catalogo[2] = troca;
  versao++;
  home_assistiu_passo(103);
  assert(home_assistiu_itens(out) == 2 && out[0] == 2 && out[1] == 8);
  conferirRotulo("Porque você assistiu Semente do catálogo substituído");
  troca = catalogo[8];
  catalogo[8] = catalogo[2];
  catalogo[2] = troca;
  versao++;
  puts("ok  troca completa do catálogo invalida índices e título");
  rede = 0;
  home_assistiu_passo(104);
  assert(home_assistiu_itens(out) == 1 && out[0] == 2);
  conferirRotulo("Parecidos no seu catálogo");
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
  perfil++;
  __atomic_add_fetch(&sessao, 1, __ATOMIC_RELEASE);
  home_assistiu_passo(107);
  assert(home_assistiu_itens(out) == 0);
  puts("ok  trocar de perfil descarta a fileira anterior");

  nCatalogo = HIST_LOTE + 2;
  for (int i = 24; i < cat_n(); i++) entrada(i, 900 + i, "Drama", 60);
  strcpy(visto, catalogo[nCatalogo - 1].imdb);
  versao++;
  consultasHistorico = 0;
  home_assistiu_passo(108);
  assert(consultasHistorico <= HIST_LOTE && home_assistiu_itens(out) == 0);
  home_assistiu_passo(109);
  assert(home_assistiu_itens(out) == HOME_ASSISTIU_MAX);
  int consultas = consultasHistorico;
  home_assistiu_passo(110);
  assert(consultasHistorico == consultas);
  puts("ok  histórico examinado em lotes e reaproveitado sem nova revisão");

  visto[0] = 0;
  nRegs = 2;
  versao++;
  rede = 1;
  resposta = 0;
  uint32_t agora = 102 + REDE_INTERVALO_MS;
  home_assistiu_passo(agora);
  aguardarResposta(1);
  home_assistiu_passo(agora + 1);
  assert(pedidos == 2);
  conferirRotulo("Parecidos no seu catálogo");
  home_assistiu_passo(agora + REDE_INTERVALO_MS - 1);
  assert(!ocupada && pedidos == 2);
  agora += REDE_INTERVALO_MS;
  home_assistiu_passo(agora);
  aguardarResposta(2);
  home_assistiu_passo(agora + 1);
  home_assistiu_passo(agora + 2 * REDE_INTERVALO_MS - 1);
  assert(!ocupada && pedidos == 3);
  agora += 2 * REDE_INTERVALO_MS;
  resposta = 999;
  home_assistiu_passo(agora);
  aguardarResposta(3);
  home_assistiu_passo(agora + 1);
  assert(pedidos == 4 && home_assistiu_itens(out) > 0);
  conferirRotulo("Parecidos no seu catálogo");
  puts("ok  falha retenta com backoff; resultado TMDB sem correspondência não muda o rótulo local");

  catalogo[0].genero[0] = 0;
  strcpy(catalogo[2].titulo, "The original: title");
  strcpy(catalogo[2].meta, "2022 · 2h");
  catalogo[2].tmdb = 0;
  strcpy(catalogo[8].titulo, "The original title");
  strcpy(catalogo[8].meta, "1980");
  catalogo[8].tmdb = 0;
  respostaJson = "{\"recommendations\":{\"results\":["
                 "{\"id\":108,\"title\":\"Título localizado\",\"original_title\":\"The Original Title\","
                 "\"release_date\":\"2022-01-01\",\"poster_path\":\"/test.jpg\"}]}}";
  agora += REDE_INTERVALO_MS;
  perfil++;
  __atomic_add_fetch(&sessao, 1, __ATOMIC_RELEASE);
  versao++;
  home_assistiu_passo(agora);
  aguardarResposta(4);
  home_assistiu_passo(agora + 1);
  assert(home_assistiu_itens(out) == 1 && out[0] == 2 && lotes == 0);
  conferirRotulo("Porque você assistiu Semente do catálogo substituído");
  puts("ok  catálogo só com IMDb casa pelo título original e ano, sem confundir homônimos");

  respostaJson = "{\"genres\":[{\"id\":18,\"name\":\"Drama\"}],"
                 "\"recommendations\":{\"results\":[]}}";
  agora += REDE_INTERVALO_MS;
  perfil++;
  __atomic_add_fetch(&sessao, 1, __ATOMIC_RELEASE);
  home_assistiu_passo(agora);
  aguardarResposta(5);
  home_assistiu_passo(agora + 1);
  assert(home_assistiu_itens(out) > 0 && lotes == 0);
  conferirRotulo("Parecidos no seu catálogo");
  puts("ok  gêneros da semente no TMDB alimentam o fallback local");

  respostaJson = "{\"recommendations\":{\"results\":["
                 "{\"id\":502,\"title\":\"Fora do catálogo\",\"release_date\":\"2024-01-01\","
                 "\"poster_path\":\"/remote.jpg\"}]}}";
  agora += REDE_INTERVALO_MS;
  perfil++;
  __atomic_add_fetch(&sessao, 1, __ATOMIC_RELEASE);
  home_assistiu_passo(agora);
  aguardarResposta(6);
  home_assistiu_passo(agora + 1);
  assert(home_assistiu_itens(out) == 1 && lotes == 1);
  assert(!strcmp(catalogo[out[0]].imdb, "tmdb:502") && catalogo[out[0]].tmdb == 502);
  assert(!strcmp(catalogo[out[0]].meta, "2024"));
  home_assistiu_passo(agora + 2);
  assert(home_assistiu_itens(out) == 1 && lotes == 1);
  puts("ok  sem candidatos locais, publica card TMDB navegável uma única vez");

  nCatalogo--;
  versao++;
  home_assistiu_passo(agora + 3);
  assert(home_assistiu_itens(out) == 1 && lotes == 2 && pedidos == 7);
  puts("ok  substituir só o catálogo republica o card sem pedir TMDB");

  int nAntes = nCatalogo - 1;
  __atomic_add_fetch(&sessao, 1, __ATOMIC_RELEASE);
  nCatalogo = 0;
  versao++;
  home_assistiu_passo(agora + 4);
  assert(home_assistiu_itens(out) == 0);
  nCatalogo = nAntes;
  versao++;
  home_assistiu_passo(agora + 5);
  assert(home_assistiu_itens(out) == 1 && lotes == 3 && pedidos == 7);
  assert(!strcmp(catalogo[out[0]].imdb, "tmdb:502"));
  home_assistiu_passo(agora + REDE_INTERVALO_MS + 1);
  assert(home_assistiu_itens(out) == 1 && lotes == 3 && pedidos == 7 && !ocupada);
  puts("ok  reabrir a sessão do mesmo perfil reaproveita TMDB sem duplicar pedidos");

  const char *const chaves[] = { "continue_watching", HOME_ASSISTIU_CHAVE, "farol_gosto1" };
  int ordem[3];
  for (int i = 0; i < 3; i++) fil_registrar(chaves[i], chaves[i], "", "", 1);
  fil_espelhar_ordem(chaves, NULL, 3);
  assert(fil_unir(chaves, 3, ordem, 3) == 3 && ordem[0] == 0 && ordem[1] == 1 && ordem[2] == 2);
  assert(fil_linha_origem(1) == FIL_ORIGEM_APP && !strcmp(fil_chave(1), HOME_ASSISTIU_CHAVE));
  assert(fil_mover(1, -1) == 0);
  fil_espelhar_ordem(chaves, NULL, 3);
  assert(fil_unir(chaves, 3, ordem, 3) == 3 && ordem[0] == 1 && ordem[1] == 0 && ordem[2] == 2);
  puts("ok  ordem padrão e posição definida em Ajustes para porque_assistiu");
  {
    int recomendados[] = { 1, 2, 3, 4, 5 };
    int farol[] = { 3, 20, 21 };
    nCatalogo = 22;
    for (int indice = 1; indice <= 5; indice++) {
      memset(&catalogo[indice], 0, sizeof catalogo[indice]);
      entrada(indice, 700 + indice, "Drama", 7);
    }
    memset(&catalogo[20], 0, 2 * sizeof catalogo[20]);
    entrada(20, 701, "Drama", 7);
    entrada(21, 799, "Drama", 7);
    snprintf(catalogo[21].imdb, sizeof catalogo[21].imdb, "tmdb:702");
    catalogo[2].tmdb = catalogo[4].tmdb = 702;
    snprintf(catalogo[4].tipo, sizeof catalogo[4].tipo, "series");
    assert(home_assistiu_sem_farol(recomendados, 5, farol, 0) == 5);
    assert(home_assistiu_sem_farol(recomendados, 5, farol, 3) == 2);
    assert(recomendados[0] == 4 && recomendados[1] == 5);
    assert(home_assistiu_sem_farol(farol, 3, farol, 3) == 0);
  }
  puts("ok  dedupe porque_assistiu x farol_gosto por indice, IMDb e TMDB do mesmo tipo");
  return 0;
}
