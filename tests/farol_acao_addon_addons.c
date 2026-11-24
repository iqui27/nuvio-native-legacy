// FAROL #17 — a quarentena de 24 h dentro do addons.c de verdade.
//
// O que se confere, com a lista real de add-ons, a busca real de fontes
// (consultar) e a rede trocada por um duble que anota quem foi perguntado:
//   - add-on de lado NAO e consultado nas fontes; segue na lista (ligado);
//   - o prazo vence sozinho (relogio injetado em addonstats_relogio);
//   - Desfazer volta a consultar na hora;
//   - a lista de add-ons diz "De lado ate amanha" (FT_ADDON_DE_LADO) e so ali;
//   - teto de 3 e nunca o unico add-on de fontes (politica de farolacao_addon);
//   - ultimo recurso: se SO restassem add-ons de lado, pergunta a eles;
//   - addons_lento_isolavel escolhe o mudo e deixa de escolher quem ja esta de lado.
#define rede_baixar_st fixture_download_st
#define rede_baixar fixture_download
#define NV_TEST_REDE_PEDIR_OVERLAY 1
#define NV_TEST_REDE_BAIXAR_ST_OVERLAY 1
#define NV_TEST_REDE_BAIXAR_MEDIDO_OVERLAY 1
#define main addonslista_fixture_main
#include "addonslista.c"
#undef main
#undef rede_baixar
#undef rede_baixar_st
char *rede_baixar(const char *url, int seconds);
char *rede_baixar_st(const char *url, int seconds, const char *const *headers, int *status);
#include "rede.h"
int rede_pedir(const RedePedido *p, RedeResposta *r) {
  memset(r, 0, sizeof *r);
  r->corpo = rede_baixar(p->url, (int)(p->prazo_ms / 1000u));
  r->n_corpo = r->corpo ? strlen(r->corpo) : 0;
  r->status = r->corpo ? 200 : 0;
  r->erro = r->corpo ? REDE_OK : REDE_TRANSPORTE;
  return 0;
}
void rede_resposta_limpar(RedeResposta *r) { free(r->corpo); r->corpo = NULL; }
char *rede_baixar_medido(const char *url, int seconds, const char *const *headers, RedeMedida *m) {
  int st = 0; char *r = rede_baixar_st(url, seconds, headers, &st);
  *m = (RedeMedida){ .status = st };
  return r;
}
#include "../src/addons.c"
#include "farolacao_addon.h"
#include "ilhaacao.h"
#include "resolver.h"
#include <assert.h>

// ------------------------------------------------------------------ dubles
char *dados_ler(const char *nome) { (void)nome; return NULL; }
int  farolAcoes, farolResultados, farolUltimoOk = -1;
void farol_registrar_acao(int acao) { (void)acao; farolAcoes++; }
void farol_registrar_resultado(int acao, int ok) { (void)acao; farolResultados++; farolUltimoOk = ok; }
void ilhaacao_feita(const IlhaAcao *a) { (void)a; }

static time_t agoraFalso = 1800000000;
static time_t relogio(void) { return agoraFalso; }

#define N_HOSTS 8
static int pedidos[N_HOSTS];   // streams pedidos a hostN.test
static char *rede_resposta(const char *url) {
  if (strstr(url, "/manifest.json"))
    return strdup("{\"resources\":[{\"name\":\"stream\",\"types\":[\"movie\"],\"idPrefixes\":[\"tt\"]}]}");
  for (int i = 0; i < N_HOSTS; i++) {
    char host[24];
    snprintf(host, sizeof host, "host%d.test", i);
    if (strstr(url, host)) { pedidos[i]++; break; }
  }
  return strdup("{\"streams\":[{\"url\":\"https://x/movie.mp4\"}]}");
}
char *rede_baixar(const char *url, int seconds) { (void)seconds; return rede_resposta(url); }
char *rede_baixar_st(const char *url, int seconds, const char *const *headers, int *status) {
  (void)seconds; (void)headers;
  *status = 200;
  return rede_resposta(url);
}

static int geracao;   // URL nova a cada lista: addons_definir_lista ignora lista igual
static void lista(int n, int comFonte) {
  AddonRemoto a[N_HOSTS];
  assert(n <= N_HOSTS);
  memset(a, 0, sizeof a);
  for (int i = 0; i < n; i++) {
    snprintf(a[i].nome, sizeof a[i].nome, "Add-on %d", i);
    snprintf(a[i].url, sizeof a[i].url, "https://host%d.test/g%d/manifest.json", i, ++geracao);
    a[i].ativo = 1;
  }
  assert(addons_definir_lista(a, n));
  for (int i = 0; i < n; i++)
    addons_manifesto_lido(i, i < comFonte
      ? "{\"resources\":[{\"name\":\"stream\",\"types\":[\"movie\"],\"idPrefixes\":[\"tt\"]}]}"
      : "{\"resources\":[{\"name\":\"catalog\",\"types\":[\"movie\"]}],\"catalogs\":[{\"type\":\"movie\",\"id\":\"c\"}]}");
}
static void busca(void) {
  Stream *out = NULL;
  memset(pedidos, 0, sizeof pedidos);
  consultar("tt1", "movie", NULL, 1, NULL, NULL, &out, NULL, 1, perfis_sessao_geracao());
  free(out);
}
static void chave(int i, char c[32]) { assert(addons_chave_estatistica(i, c)); }
static int isola(int i) {
  char c[32];
  chave(i, c);
  return farolacao_addon_isolar(c, addons_fontes_vivas_n());
}
static void desfaz(int i) {
  char c[32];
  chave(i, c);
  assert(farolacao_addon_desfazer(c) == 1);
}

int main(void) {
  char c[32];
  addonstats_relogio(relogio);
  assert(addonstats_agora() == agoraFalso);

  // --- 1. add-on de lado: sai da busca, fica na lista --------------------
  lista(3, 3);
  busca();
  assert(pedidos[0] == 1 && pedidos[1] == 1 && pedidos[2] == 1);
  assert(addons_fontes_vivas_n() == 3);
  assert(isola(1) == FAAD_OK);
  assert(addons_isolado_s(1) == FAAD_ISOLA_S);
  assert(addons_fontes_vivas_n() == 2);
  chave(1, c);
  assert(naoSegura(c));                                      // tambem nao e esperado na decisao
  busca();
  assert(pedidos[0] == 1 && pedidos[1] == 0 && pedidos[2] == 1);
  assert(addons_n() == 3 && addons_ativo(1) == 1);          // continua na lista, ligado
  assert(!strcmp(addons_nome(1), "Add-on 1"));
  puts("ok  add-on de lado nao e consultado nem esperado nas fontes e segue na lista");

  // --- 2. a lista de add-ons diz "de lado ate amanha" ---------------------
  assert(!strcmp(addons_estado_texto(1), FT_ADDON_DE_LADO));
  assert(!strcmp(addons_estado_texto(1), "De lado até amanhã"));
  assert(!addons_estado_texto(0)[0] && !addons_estado_texto(2)[0]);
  assert(!addons_estado_texto(-1)[0] && !addons_estado_texto(99)[0]);
  puts("ok  consulta da lista: \"De lado ate amanha\" so para o isolado");

  // --- 3. o prazo vence sozinho ------------------------------------------
  agoraFalso += FAAD_ISOLA_S - 1;
  assert(addons_isolado_s(1) == 1);
  busca();
  assert(pedidos[1] == 0);
  agoraFalso += 1;
  assert(addons_isolado_s(1) == 0 && !addons_estado_texto(1)[0]);
  busca();
  assert(pedidos[0] == 1 && pedidos[1] == 1 && pedidos[2] == 1);
  assert(addonstats_isolados_n() == 0);
  puts("ok  vence em 86400 s e o add-on volta a ser consultado");

  // --- 4. Desfazer volta na hora ------------------------------------------
  assert(isola(2) == FAAD_OK);
  busca();
  assert(pedidos[2] == 0);
  desfaz(2);
  busca();
  assert(pedidos[2] == 1 && addons_isolado_s(2) == 0);
  puts("ok  Desfazer devolve o add-on a busca");

  // --- 5. teto de 3 e unico de fontes -------------------------------------
  lista(5, 5);
  assert(isola(0) == FAAD_OK && isola(1) == FAAD_OK && isola(2) == FAAD_OK);
  assert(isola(3) == FAAD_TETO);
  assert(addons_isolado_s(3) == 0 && addons_fontes_vivas_n() == 2);
  busca();
  assert(pedidos[0] == 0 && pedidos[1] == 0 && pedidos[2] == 0 && pedidos[3] == 1 && pedidos[4] == 1);
  desfaz(0);
  assert(isola(3) == FAAD_OK);                         // a vaga voltou
  agoraFalso += FAAD_ISOLA_S;
  assert(addonstats_isolados_n() == 0);
  lista(2, 2);
  assert(isola(0) == FAAD_OK);
  assert(isola(1) == FAAD_UNICO);                      // o ultimo de fontes nunca sai
  assert(addons_isolado_s(1) == 0 && addons_fontes_vivas_n() == 1);
  busca();
  assert(pedidos[0] == 0 && pedidos[1] == 1);
  desfaz(0);
  // add-on so de catalogo nao conta como "de fontes": 1 de fontes + 1 catalogo.
  lista(2, 1);
  assert(addons_fontes_vivas_n() == 1);
  assert(isola(0) == FAAD_UNICO);
  assert(farolAcoes >= 1 && farolResultados == farolAcoes && farolUltimoOk == 0);
  puts("ok  teto de 3 e nunca o unico add-on de fontes");

  // --- 6. ultimo recurso: nada para perguntar --------------------------------
  lista(2, 2);
  chave(0, c); assert(addonstats_isolar(c, FAAD_ISOLA_S));
  chave(1, c); assert(addonstats_isolar(c, FAAD_ISOLA_S));
  busca();
  assert(pedidos[0] == 1 && pedidos[1] == 1);          // so de lado: melhor perguntar que calar
  addonstats_isolar_zerar();
  puts("ok  se so restassem add-ons de lado, a busca ainda pergunta");

  // --- 7. quem a Farol isolaria ----------------------------------------------
  lista(3, 3);
  assert(addons_lento_isolavel() == -1);               // ninguem mudo ainda
  chave(2, c);
  addonstats_registrar(c, 30000, 0);
  addonstats_registrar(c, 30000, 0);
  assert(addons_lento_isolavel() == 2);
  assert(isola(2) == FAAD_OK);
  assert(addons_lento_isolavel() == -1);               // quem ja esta de lado nao volta a ser escolhido
  puts("ok  addons_lento_isolavel escolhe o mudo e ignora o que ja esta de lado");

  puts("farol_acao_addon_addons: tudo ok");
  return 0;
}
