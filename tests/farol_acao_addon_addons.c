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
//   - addons_lento_isolavel escolhe o mudo e deixa de escolher quem ja esta de lado;
//   - os Ajustes (ajAddonApoio/ajAddonNestaTv, extraidos de ajustes_ux_ilha.inc
//     pelo farol_acao_addon.sh) mostram "De lado ate amanha" na linha e no painel;
//   - dois fios com barreira e 2 add-ons de fontes: 1 FAAD_OK, 1 FAAD_UNICO;
//   - a consulta capturada (Agenda, preparo do proximo) segue a quarentena de
//     AGORA: addons_consulta_atual cai e filme/consultar pulam (ou voltam a
//     perguntar) depois de isolar, vencer as 86400 s ou Desfazer; com todos de
//     lado, as duas rotas usam o ultimo recurso.
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
static pthread_mutex_t farolTrava = PTHREAD_MUTEX_INITIALIZER;   // os fios da secao 8
int  farolAcoes, farolResultados, farolUltimoOk = -1;
void farol_registrar_acao(int acao) {
  (void)acao;
  pthread_mutex_lock(&farolTrava); farolAcoes++; pthread_mutex_unlock(&farolTrava);
}
void farol_registrar_resultado(int acao, int ok) {
  (void)acao;
  pthread_mutex_lock(&farolTrava); farolResultados++; farolUltimoOk = ok; pthread_mutex_unlock(&farolTrava);
}
void ilhaacao_feita(const IlhaAcao *a) { (void)a; }

// A consulta capturada sem debrid: toda fonte com URL serve.
void debrid_contexto_capturar(DebridContexto *k) { if (k) memset(k, 0, sizeof *k); }
void debrid_contexto_entrar(const DebridContexto *k) { (void)k; }
void debrid_contexto_sair(void) {}
int stream_fonte_disponivel(const Stream *f) { return f && f->url[0]; }

// Os Ajustes de verdade: as duas funcoes que a linha e o painel usam.
static const char *ajAddonFornece(int i) { (void)i; return "Fontes"; }
#include "ajustes_addon_estado.inc"

static time_t agoraFalso = 1800000000;
static time_t relogio(void) { return agoraFalso; }

#define N_HOSTS 8
static int pedidos[N_HOSTS];   // streams pedidos a hostN.test
static pthread_mutex_t pedidosTrava = PTHREAD_MUTEX_INITIALIZER;   // consulta capturada usa fios
static char *rede_resposta(const char *url) {
  if (strstr(url, "/manifest.json"))
    return strdup("{\"resources\":[{\"name\":\"stream\",\"types\":[\"movie\"],\"idPrefixes\":[\"tt\"]}]}");
  for (int i = 0; i < N_HOSTS; i++) {
    char host[24];
    snprintf(host, sizeof host, "host%d.test", i);
    if (strstr(url, host)) {
      pthread_mutex_lock(&pedidosTrava); pedidos[i]++; pthread_mutex_unlock(&pedidosTrava);
      break;
    }
  }
  return strdup("{\"streams\":[{\"url\":\"https://x/movie.mp4\"}]}");
}
char *rede_baixar(const char *url, int seconds) { (void)seconds; return rede_resposta(url); }
char *rede_baixar_medido_controle(const char *url, int segundos, const char *const *cabecalhos,
                                  const RedeControle *controle, RedeMedida *medida) {
  (void)segundos; (void)cabecalhos; (void)controle;
  memset(medida, 0, sizeof *medida);
  medida->status = 200;
  return rede_resposta(url);
}
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
  return farolacao_addon_isolar(c);
}
static void desfaz(int i) {
  char c[32];
  chave(i, c);
  assert(farolacao_addon_desfazer(c) == 1);
}

// Barreira de dois (pthread_barrier_t nao existe no macOS).
static pthread_mutex_t barTrava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t barSinal = PTHREAD_COND_INITIALIZER;
static int barChegou, barVolta;
static void barreira(void) {
  pthread_mutex_lock(&barTrava);
  int v = barVolta;
  if (++barChegou == 2) { barChegou = 0; barVolta++; pthread_cond_broadcast(&barSinal); }
  else while (v == barVolta) pthread_cond_wait(&barSinal, &barTrava);
  pthread_mutex_unlock(&barTrava);
}
typedef struct { int i, rc; } Corrida;
static void *corrida(void *u) {
  Corrida *k = u;
  barreira();
  k->rc = isola(k->i);
  return NULL;
}

// A consulta capturada (addons_consulta_*), contando quem foi perguntado.
static AddonsConsulta *captura(void) {
  AddonsConsulta *k = addons_consulta_capturar();
  assert(k);
  return k;
}
static void ampla(const AddonsConsulta *k, int p0, int p1) {
  Stream *out = NULL;
  memset(pedidos, 0, sizeof pedidos);
  assert(addons_consulta_consultar(k, "tt1", "movie", 2, NULL, NULL, &out) > 0);
  free(out);
  assert(pedidos[0] == p0 && pedidos[1] == p1);
}
static void filme(const AddonsConsulta *k, int p0, int p1) {
  memset(pedidos, 0, sizeof pedidos);
  assert(addons_consulta_filme(k, "tt1", NULL, NULL) == 1);
  assert(pedidos[0] == p0 && pedidos[1] == p1);
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
  // os Ajustes: a linha diz o estado no lugar do que fornece; o painel, no "Nesta TV".
  assert(!strcmp(ajAddonApoio(1), FT_ADDON_DE_LADO) && !strcmp(ajAddonApoio(0), "Fontes"));
  assert(!strcmp(ajAddonNestaTv(1), FT_ADDON_DE_LADO) && !strcmp(ajAddonNestaTv(0), i18n("Ligado")));
  puts("ok  Ajustes: linha e painel do isolado dizem \"De lado ate amanha\"");

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

  // --- 8. dois fios, duas fontes: nunca as duas de lado -------------------------
  lista(2, 2);
  for (int volta = 0; volta < 100; volta++) {
    pthread_t th[2];
    Corrida cr[2];
    addonstats_isolar_zerar();
    assert(addons_fontes_vivas_n() == 2);
    for (int i = 0; i < 2; i++) { cr[i].i = i; assert(pthread_create(&th[i], NULL, corrida, &cr[i]) == 0); }
    for (int i = 0; i < 2; i++) pthread_join(th[i], NULL);
    assert((cr[0].rc == FAAD_OK && cr[1].rc == FAAD_UNICO) || (cr[0].rc == FAAD_UNICO && cr[1].rc == FAAD_OK));
    assert(addons_fontes_vivas_n() == 1);
  }
  addonstats_isolar_zerar();
  puts("ok  dois fios com 2 fontes: 1 FAAD_OK, 1 FAAD_UNICO, 1 fonte viva");

  // --- 9. consulta capturada segue a quarentena de agora ---------------------------
  lista(2, 2);
  {
    AddonsConsulta *k = captura();
    assert(addons_consulta_atual(k));
    assert(isola(0) == FAAD_OK);
    assert(!addons_consulta_atual(k));                 // isolar depois da captura a deixa velha
    ampla(k, 0, 1);
    filme(k, 0, 1);
    addons_consulta_liberar(k);
    k = captura();                                     // capturada durante a quarentena
    assert(addons_consulta_atual(k));
    ampla(k, 0, 1);
    filme(k, 0, 1);
    agoraFalso += FAAD_ISOLA_S;                        // as 86400 s vencem
    assert(!addons_consulta_atual(k));
    ampla(k, 1, 1);
    filme(k, 1, 0);
    addons_consulta_liberar(k);
    assert(isola(0) == FAAD_OK);
    k = captura();
    desfaz(0);                                         // Desfazer depois da captura
    assert(!addons_consulta_atual(k));
    ampla(k, 1, 1);
    filme(k, 1, 0);
    addons_consulta_liberar(k);
    k = captura();
    assert(isola(0) == FAAD_OK);
    desfaz(0);                                         // ida e volta: o efetivo nao mudou
    assert(addons_consulta_atual(k));
    addons_consulta_liberar(k);
    // todos de lado: as duas rotas usam o ultimo recurso (melhor perguntar que calar)
    chave(0, c); assert(addonstats_isolar(c, FAAD_ISOLA_S));
    chave(1, c); assert(addonstats_isolar(c, FAAD_ISOLA_S));
    k = captura();
    ampla(k, 1, 1);
    filme(k, 1, 0);
    addons_consulta_liberar(k);
    addonstats_isolar_zerar();
  }
  puts("ok  consulta capturada: isolar, vencer em 86400 s e Desfazer valem na hora");

  puts("farol_acao_addon_addons: tudo ok");
  return 0;
}
