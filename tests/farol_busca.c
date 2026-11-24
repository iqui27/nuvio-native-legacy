// Teste integrado do cliente assincrono src/farolbusca.c contra servidor HTTP
// local (tests/farol_busca_server.py): ok, palavras/422, ids vazio, 429, erro,
// offline, atraso > 6 s (prazo -> offline), cancelamento (< 300 ms), novo
// pedido sobre pedido lento, perfil privado, consentiu:0 explicito, infantil,
// consentiu:1, contrato busca_pediu_abrir e invalidacao por cat_revisao().
//
// Nenhum titulo real, nome de perfil ou credencial real entra neste teste.
#include "farolbusca.h"
#include "cacavisto.h"
#include "faroltexto.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define CAT_CAP 64

static CatItem  C_itens[CAT_CAP];
static int      C_n;
static unsigned C_rev = 1;
static int      R_offline;
static int      R_infantil;
static int      R_acrescentados;

int cat_n(void) { return C_n; }
unsigned cat_revisao(void) { return C_rev; }
const CatItem *cat_item(int i) { return (i >= 0 && i < C_n) ? &C_itens[i] : NULL; }

int cat_acrescentar(const CatItem *item) {
  if (!item || C_n >= CAT_CAP) return -1;
  C_itens[C_n] = *item;
  C_rev++;
  R_acrescentados++;
  return C_n++;
}

int cacavisto_filme(const char *imdb, long tmdb, long long *quando) {
  (void)imdb; (void)tmdb;
  if (quando) *quando = 0;
  return 0;
}

int rede_saude_offline(void) { return R_offline; }
int perfil_infantil_ativo(void) { return R_infantil; }

static long agoraMs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000L + t.tv_nsec / 1000000L;
}

static void resetCatalogo(void) {
  memset(C_itens, 0, sizeof C_itens);
  C_n = 0;
  C_rev++;
  R_acrescentados = 0;
}

static int addItem(const char *imdb, long tmdb, const char *tipo,
                   const char *cod, const char *genero, const char *meta,
                   const char *poster) {
  int i = C_n++;
  assert(i < CAT_CAP);
  memset(&C_itens[i], 0, sizeof C_itens[i]);
  snprintf(C_itens[i].imdb, sizeof C_itens[i].imdb, "%s", imdb ? imdb : "");
  C_itens[i].tmdb = tmdb;
  snprintf(C_itens[i].tipo, sizeof C_itens[i].tipo, "%s", tipo ? tipo : "movie");
  snprintf(C_itens[i].titulo, sizeof C_itens[i].titulo, "%s", cod ? cod : "");
  snprintf(C_itens[i].genero, sizeof C_itens[i].genero, "%s", genero ? genero : "");
  snprintf(C_itens[i].meta, sizeof C_itens[i].meta, "%s", meta ? meta : "");
  snprintf(C_itens[i].poster, sizeof C_itens[i].poster, "%s", poster ? poster : "");
  C_itens[i].nota = 82;
  return i;
}

static void popularCatalogoBase(void) {
  resetCatalogo();
  // Indice 0: filme no catalogo (casara por imdb tt0000101).
  addItem("tt0000101", 550, "movie", "obra_101",
          "Filme · Suspense", "2024 · 92 min",
          "https://img.invalid/p101.jpg");
  // Indice 1: serie no catalogo (casara por tmdb 1396 mesmo com imdb diferente/ausente no servidor).
  addItem("tt0000102", 1396, "series", "obra_102",
          "Programa de TV · Drama", "2023 · 48 min",
          "https://img.invalid/p102.jpg");
  // Indice 2: filme curto de suspense para fallback local.
  addItem("tt0000103", 777, "movie", "obra_103",
          "Filme · Suspense", "2025 · 86 min",
          "https://img.invalid/p103.jpg");
  // Indice 3: serie em andamento para FA_RETOMAR.
  {
    int idx = addItem("tt0000104", 888, "series", "obra_104",
                      "Programa de TV · Crime", "2024 · 50 min",
                      "https://img.invalid/p104.jpg");
    C_itens[idx].progresso = 45;
    C_itens[idx].retomadoMs = 1700000000000LL;
  }
}

static void deslocarCatalogoMaisUm(void) {
  assert(C_n + 1 < CAT_CAP);
  memmove(&C_itens[1], &C_itens[0], sizeof(CatItem) * (size_t)C_n);
  C_n++;
  memset(&C_itens[0], 0, sizeof C_itens[0]);
  snprintf(C_itens[0].imdb, sizeof C_itens[0].imdb, "tt0000999");
  snprintf(C_itens[0].tipo, sizeof C_itens[0].tipo, "movie");
  snprintf(C_itens[0].titulo, sizeof C_itens[0].titulo, "obra_999");
  snprintf(C_itens[0].genero, sizeof C_itens[0].genero, "Filme · Drama");
  snprintf(C_itens[0].meta, sizeof C_itens[0].meta, "2020 · 130 min");
  C_itens[0].progresso = 100; // ja visto
  C_rev++;
}

static FarolBuscaInfo esperarPronto(long maxMs) {
  FarolBuscaInfo info;
  long ini = agoraMs();
  for (;;) {
    FarolBuscaEstado st = farolbusca_estado(&info);
    if (st != FB_BUSCANDO) return info;
    assert(agoraMs() - ini < maxMs);
    usleep(5000);
  }
}

int main(int argc, char **argv) {
  char rota[512];
  FarolBuscaInfo info;
  FarolBuscaItem itens[FAROL_BUSCA_IDS_MAX];
  unsigned rev = 0, rev2 = 0;
  int n, idxAbrir = -1;

  assert(argc == 2);
  farolbusca_definir_auth("Authorization: Bearer sint_tok_segredo", "X-Nuvio-Auth: nuvio");
  farolbusca_definir_semente(42u);
  farolbusca_definir_privado(0);
  farolbusca_definir_consentimento(0);

  // Frase invalida/vazia e rejeitada na hora.
  assert(farolbusca_pedir(NULL) == 0);
  assert(farolbusca_pedir("   ") == 0);
  assert(farolbusca_pedir("https://url-proibida.invalid/x") == 0);

  // 1. OK (LLM): mistura itens no catalogo (por imdb e por tmdb) + fora (cartaz pelo id),
  //    contrato de busca_pediu_abrir e invalidacao por cat_revisao().
  popularCatalogoBase();
  snprintf(rota, sizeof rota, "%s/ok", argv[1]);
  farolbusca_definir_url(rota);
  {
    char fraseMutavel[] = "um suspense curto secreto_canario_frase";
    assert(farolbusca_pedir(fraseMutavel) == 1);
    memset(fraseMutavel, 'x', strlen(fraseMutavel)); // garante copia antes de voltar
  }
  info = esperarPronto(3000);
  assert(info.estado == FB_PRONTO);
  assert(info.motivo == FB_MOTIVO_OK);
  assert(info.origem == FB_ORIG_LLM);
  assert(info.http == 200 && info.llm_status == 200);
  assert(info.usouLocal == 0);
  assert(info.filtro.duracao_max == 100 && info.filtro.decada == 1990);
  assert(info.filtro.nGeneros == 2 && !strcmp(info.filtro.generos[0], "suspense"));

  n = farolbusca_resolver(itens, FAROL_BUSCA_IDS_MAX, &rev);
  assert(n == 4 && rev == C_rev);
  // Item 0: no catalogo por imdb (indice 0).
  assert(itens[0].noCatalogo == 1 && itens[0].indice == 0);
  assert(farolbusca_item(&itens[0], rev) == &C_itens[0]);
  // Item 1: no catalogo por tmdb=1396 (indice 1).
  assert(itens[1].noCatalogo == 1 && itens[1].indice == 1);
  assert(farolbusca_item(&itens[1], rev) == &C_itens[1]);
  // Item 2: fora do catalogo com imdb tt9990001 -> cartaz Metahub pelo id.
  assert(itens[2].noCatalogo == 0 && itens[2].indice == -1);
  assert(!strcmp(itens[2].imdb, "tt9990001") && !strcmp(itens[2].tipo, "movie"));
  assert(strstr(itens[2].poster, "tt9990001") != NULL);
  assert(farolbusca_item(&itens[2], rev) == NULL);
  // Item 3: fora do catalogo so com tmdb=999002 -> id "tmdb:999002".
  assert(itens[3].noCatalogo == 0 && itens[3].indice == -1);
  assert(!strcmp(itens[3].imdb, "tmdb:999002") && itens[3].tmdb == 999002);
  assert(!strcmp(itens[3].tipo, "series"));

  // Selecionar item 0 ANTES de cat_revisao mudar, deslocar catalogo (+1), e
  // consumir busca_pediu_abrir DEPOIS: deve devolver o NOVO indice (1), nunca 0!
  assert(farolbusca_selecionar(&itens[0]) == 1);
  deslocarCatalogoMaisUm();
  // O indice antigo do quadro anterior e recusado na hora por farolbusca_item().
  assert(farolbusca_item(&itens[0], rev) == NULL);
  assert(farolbusca_item(&itens[1], rev) == NULL);
  // Abrir consome uma vez e re-resolve para o indice vivo (1).
  assert(farolbusca_pediu_abrir(&idxAbrir) == 1 && idxAbrir == 1);
  assert(farolbusca_pediu_abrir(&idxAbrir) == 0);

  // Re-resolver na revisao nova entrega indices deslocados (+1).
  n = farolbusca_resolver(itens, FAROL_BUSCA_IDS_MAX, &rev2);
  assert(n == 4 && rev2 == C_rev && rev2 != rev);
  assert(itens[0].noCatalogo == 1 && itens[0].indice == 1);
  assert(itens[1].noCatalogo == 1 && itens[1].indice == 2);

  // Selecionar item FORA do catalogo (itens[2], tt9990001): acrescenta cartaz pelo id.
  {
    int antes = R_acrescentados;
    assert(farolbusca_selecionar(&itens[2]) == 1);
    assert(farolbusca_pediu_abrir(&idxAbrir) == 1 && idxAbrir >= 0);
    assert(R_acrescentados == antes + 1);
    assert(!strcmp(C_itens[idxAbrir].imdb, "tt9990001"));
    assert(!strcmp(C_itens[idxAbrir].tipo, "movie"));
    assert(strstr(C_itens[idxAbrir].poster, "tt9990001") != NULL);
    assert(farolbusca_pediu_abrir(&idxAbrir) == 0);
    // Abrir de novo encontra o item ja acrescentado (nao duplica).
    assert(farolbusca_abrir_item(&itens[2]) == idxAbrir);
    assert(R_acrescentados == antes + 1);
  }

  // 2. Palavras (LLM 422 interno, servidor respondeu por busca de palavras).
  popularCatalogoBase();
  snprintf(rota, sizeof rota, "%s/palavras", argv[1]);
  farolbusca_definir_url(rota);
  assert(farolbusca_pedir("suspense policial") == 1);
  info = esperarPronto(3000);
  assert(info.estado == FB_PRONTO && info.motivo == FB_MOTIVO_OK);
  assert(info.origem == FB_ORIG_PALAVRAS && info.llm_status == 422 && info.usouLocal == 0);

  // 3. Servidor devolveu 200 com ids vazio (ressalva 1 da farol-29) -> cai no local.
  snprintf(rota, sizeof rota, "%s/vazio", argv[1]);
  farolbusca_definir_url(rota);
  assert(farolbusca_pedir("um suspense curto para hoje") == 1);
  info = esperarPronto(3000);
  assert(info.estado == FB_PRONTO && info.motivo == FB_MOTIVO_VAZIO);
  assert(info.origem == FB_ORIG_LOCAL && info.usouLocal == 1);
  assert(!strcmp(info.fraseEstado, FT_BUSCA_FRASE_OFFLINE));
  n = farolbusca_resolver(itens, FAROL_BUSCA_IDS_MAX, &rev);
  assert(n >= 1 && itens[0].noCatalogo == 1);

  // 4. HTTP 429 -> cai na intencao local com FT_BUSCA_FRASE_LIMITE.
  snprintf(rota, sizeof rota, "%s/429", argv[1]);
  farolbusca_definir_url(rota);
  assert(farolbusca_pedir("um suspense curto para hoje") == 1);
  info = esperarPronto(3000);
  assert(info.estado == FB_PRONTO && info.motivo == FB_MOTIVO_LIMITE);
  assert(info.http == 429 && info.origem == FB_ORIG_LOCAL && info.usouLocal == 1);
  assert(info.local.intencao == FA_CURTO && !strcmp(info.local.genero, "Suspense"));
  assert(!strcmp(info.fraseEstado, FT_BUSCA_FRASE_LIMITE));
  n = farolbusca_resolver(itens, FAROL_BUSCA_IDS_MAX, &rev);
  assert(n == 2); // obra_101 (92m) e obra_103 (86m), ambos Suspense <= 100 min
  assert(farolbusca_item(&itens[0], rev) != NULL);
  // cat_revisao muda tambem invalida o fallback local!
  deslocarCatalogoMaisUm();
  assert(farolbusca_item(&itens[0], rev) == NULL);
  n = farolbusca_resolver(itens, FAROL_BUSCA_IDS_MAX, &rev2);
  assert(n == 2 && rev2 == C_rev);
  assert(itens[0].indice == 1 || itens[0].indice == 3);

  // 5. Erro HTTP (500, 400) e JSON malformado -> cai no local com FT_BUSCA_FRASE_OFFLINE.
  popularCatalogoBase();
  {
    const char *rotasErro[] = { "/500", "/400", "/malformado" };
    size_t i;
    for (i = 0; i < 3; i++) {
      snprintf(rota, sizeof rota, "%s%s", argv[1], rotasErro[i]);
      farolbusca_definir_url(rota);
      assert(farolbusca_pedir("continuar de onde parei") == 1);
      info = esperarPronto(3000);
      assert(info.estado == FB_PRONTO && info.motivo == FB_MOTIVO_ERRO);
      assert(info.origem == FB_ORIG_LOCAL && info.usouLocal == 1);
      assert(info.local.intencao == FA_RETOMAR);
      assert(!strcmp(info.fraseEstado, FT_BUSCA_FRASE_OFFLINE));
      n = farolbusca_resolver(itens, FAROL_BUSCA_IDS_MAX, &rev);
      assert(n == 1 && itens[0].indice == 3); // obra_104 em andamento
    }
  }

  // 6. Offline: flag de rede offline E porta fechada (transporte) -> local + FT_BUSCA_FRASE_OFFLINE.
  R_offline = 1;
  snprintf(rota, sizeof rota, "%s/ok", argv[1]);
  farolbusca_definir_url(rota);
  assert(farolbusca_pedir("algo curto") == 1);
  info = esperarPronto(500);
  assert(info.estado == FB_PRONTO && info.motivo == FB_MOTIVO_OFFLINE && info.usouLocal == 1);
  assert(!strcmp(info.fraseEstado, FT_BUSCA_FRASE_OFFLINE));
  R_offline = 0;

  farolbusca_definir_url("http://127.0.0.1:1");
  assert(farolbusca_pedir("algo curto") == 1);
  info = esperarPronto(3000);
  assert(info.estado == FB_PRONTO && info.motivo == FB_MOTIVO_OFFLINE && info.usouLocal == 1);
  assert(!strcmp(info.fraseEstado, FT_BUSCA_FRASE_OFFLINE));

  // 7. Perfil privado: servidor exige EXATAMENTE {"frase":...} e nenhuma outra chave.
  snprintf(rota, sizeof rota, "%s/privado", argv[1]);
  farolbusca_definir_url(rota);
  farolbusca_definir_privado(1);
  farolbusca_definir_consentimento(1);
  assert(farolbusca_pedir("suspense privado") == 1);
  info = esperarPronto(3000);
  assert(info.estado == FB_PRONTO && info.motivo == FB_MOTIVO_OK && info.http == 200);
  {
    const char *gens[] = { "suspense" };
    const char *hist[] = { "tt0000101" };
    FarolBuscaCtx ctx = {0};
    ctx.privado = 1;
    ctx.consentiu = 1;
    ctx.idioma = "pt-BR";
    ctx.generos = gens; ctx.nGeneros = 1;
    ctx.historico = hist; ctx.nHistorico = 1;
    assert(farolbusca_pedir_com("suspense privado ctx", &ctx) == 1);
    info = esperarPronto(3000);
    assert(info.estado == FB_PRONTO && info.motivo == FB_MOTIVO_OK && info.http == 200);
  }
  farolbusca_definir_privado(0);

  // 8. Sem consentimento -> consentiu:0 explicito; infantil -> infantil:1 + consentiu:0;
  //    com consentimento -> consentiu:1 + generos/historico.
  farolbusca_definir_consentimento(0);
  snprintf(rota, sizeof rota, "%s/sem-consent", argv[1]);
  farolbusca_definir_url(rota);
  assert(farolbusca_pedir("drama calmo") == 1);
  info = esperarPronto(3000);
  assert(info.estado == FB_PRONTO && info.motivo == FB_MOTIVO_OK && info.http == 200);

  R_infantil = 1;
  farolbusca_definir_consentimento(1);
  snprintf(rota, sizeof rota, "%s/infantil", argv[1]);
  farolbusca_definir_url(rota);
  assert(farolbusca_pedir("desenho curto") == 1);
  info = esperarPronto(3000);
  assert(info.estado == FB_PRONTO && info.motivo == FB_MOTIVO_OK && info.http == 200);
  R_infantil = 0;

  snprintf(rota, sizeof rota, "%s/com-consent", argv[1]);
  farolbusca_definir_url(rota);
  {
    const char *gens[] = { "suspense" };
    const char *hist[] = { "tt0000101" };
    FarolBuscaCtx ctx = {0};
    ctx.privado = 0;
    ctx.consentiu = 1;
    ctx.idioma = "pt-BR";
    ctx.generos = gens; ctx.nGeneros = 1;
    ctx.historico = hist; ctx.nHistorico = 1;
    assert(farolbusca_pedir_com("suspense anos 90", &ctx) == 1);
    info = esperarPronto(3000);
    assert(info.estado == FB_PRONTO && info.motivo == FB_MOTIVO_OK && info.http == 200);
  }
  farolbusca_definir_consentimento(0);

  // 9. Cancelamento contra servidor lento (10 s) encerra em < 300 ms.
  snprintf(rota, sizeof rota, "%s/slow", argv[1]);
  farolbusca_definir_url(rota);
  assert(farolbusca_pedir("filme lento cancelar") == 1);
  usleep(120000);
  {
    long ini = agoraMs();
    farolbusca_cancelar();
    info = esperarPronto(1000);
    assert(info.estado == FB_CANCELADO && info.motivo == FB_MOTIVO_CANCELADO);
    assert(agoraMs() - ini < 300);
  }

  // Novo pedido sobre pedido lento invalida o antigo e responde normalmente.
  assert(farolbusca_pedir("filme lento substituido") == 1);
  usleep(80000);
  snprintf(rota, sizeof rota, "%s/ok", argv[1]);
  farolbusca_definir_url(rota);
  assert(farolbusca_pedir("suspense rapido") == 1);
  info = esperarPronto(3000);
  assert(info.estado == FB_PRONTO && info.motivo == FB_MOTIVO_OK && info.http == 200);

  // 10. Atraso > 6 s (servidor dorme 10 s): prazo de 6 s estoura e vira offline + local.
  snprintf(rota, sizeof rota, "%s/slow", argv[1]);
  farolbusca_definir_url(rota);
  {
    long ini = agoraMs(), dt;
    assert(farolbusca_pedir("suspense curto demorado") == 1);
    info = esperarPronto(8000);
    dt = agoraMs() - ini;
    assert(dt >= 5700 && dt < 7000);
    assert(info.estado == FB_PRONTO && info.motivo == FB_MOTIVO_PRAZO);
    assert(info.origem == FB_ORIG_LOCAL && info.usouLocal == 1);
    assert(!strcmp(info.fraseEstado, FT_BUSCA_FRASE_OFFLINE));
    n = farolbusca_resolver(itens, FAROL_BUSCA_IDS_MAX, &rev);
    assert(n >= 1);
  }

  farolbusca_encerrar();
  assert(farolbusca_em_voo() == 0);
  puts("farol_busca: cliente ok");
  return 0;
}
