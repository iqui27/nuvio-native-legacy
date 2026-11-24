// Teste integrado e unitario de src/farolcompor.c (issue #52).
// Linka farolcompor.c, farolgosto.c, farolpeca.c, faroltexto.c, js.c, rede.c,
// redeurl.c sem SDL, contra servidor HTTP local (tests/farol_compor_server.py).
#include "farolcompor.h"
#include "farolgosto.h"
#include "farolpeca.h"
#define FarolMotivo FarolRecMotivo
#include "farolrec.h"
#undef FarolMotivo
#include "rede.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// Estado simulado dos ganchos do app (sobrescrevem os fracos de farolcompor.c).
// Atomicos: o worker revalida os bloqueios (player/infantil/perfil...) no
// proprio fio, logo antes do POST, enquanto o teste os muda no principal.
static _Atomic int        S_offline = 0;
static _Atomic int        S_infantil = 0;
static _Atomic int        S_player = 0;
static _Atomic int        S_seguro = 0;
static _Atomic int        S_recomendar = 1;
static _Atomic int        S_perfil = 1;
static _Atomic uint64_t   S_sessao = 1;
static char               S_usuario[128] = "conta_canario_1";
static _Atomic int        S_homeVis = 0;
static _Atomic int        S_traktRecusadas = 0;
static _Atomic long long  S_relogio = 1795000000LL; // 2026-11-18 ~ 11:06 UTC

// recomenda_cabecalhos de verdade e chamado quando nao ha override de auth
// (farolcompor_definir_auth(NULL...)). No app ele pode renovar o JWT por HTTP
// (recomenda.c -> sessao.c); aqui uma BARREIRA o segura para provar que o
// POST nao sai fora do contexto que o autorizou (FAROL52-r1, P1 #2).
static pthread_mutex_t B_mx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  B_cv = PTHREAD_COND_INITIALIZER;
static int B_armada, B_dentro, B_liberar, B_chamadas;
int recomenda_cabecalhos(const char **cab, char *aut, size_t na, char *via, size_t nv,
                         char *perfil, size_t np) {
  pthread_mutex_lock(&B_mx);
  B_chamadas++;
  if (B_armada) {
    B_dentro = 1;
    pthread_cond_broadcast(&B_cv);
    while (!B_liberar) pthread_cond_wait(&B_cv, &B_mx);
  }
  pthread_mutex_unlock(&B_mx);
  // Como o app: o cabecalho de perfil sai do perfil ATIVO no fim da resolucao.
  snprintf(aut, na, "Authorization: Bearer tok_segredo_compor_99");
  snprintf(via, nv, "X-Nuvio-Auth: nuvio");
  snprintf(perfil, np, "X-Nuvio-Perfil: %d", (int)S_perfil);
  if (cab) { cab[0] = aut; cab[1] = via; cab[2] = perfil; cab[3] = NULL; }
  return 1;
}
static void barreiraArmar(void) {
  pthread_mutex_lock(&B_mx);
  B_armada = 1; B_dentro = 0; B_liberar = 0; B_chamadas = 0;
  pthread_mutex_unlock(&B_mx);
}
static void barreiraEsperarDentro(void);
static void barreiraLiberar(void) {
  pthread_mutex_lock(&B_mx);
  B_liberar = 1; B_armada = 0;
  pthread_cond_broadcast(&B_cv);
  pthread_mutex_unlock(&B_mx);
}

int rede_saude_offline(void) { return S_offline; }
int perfil_infantil_ativo(void) { return S_infantil; }
int player_aberto(void) { return S_player; }
int seguro_perfil_ativo(void) { return S_seguro; }
int farolsinal_recomendar(void) { return S_recomendar; }
int perfis_ativo(void) { return S_perfil; }
uint64_t perfis_sessao_geracao(void) { return S_sessao; }
const char *sessao_usuario(void) { return S_usuario; }
const char *ajustes_tmdb_idioma(void) { return "pt-BR"; }
void trakt_http_recusada(void) { S_traktRecusadas++; }

static long long relogioTeste(void) { return S_relogio; }
static int homeVisTeste(void) { return S_homeVis; }

static long long agoraMs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (long long)t.tv_sec * 1000LL + (long long)(t.tv_nsec / 1000000L);
}

static void srvConfig(const char *base, const char *scenario, double delayS, int resetHits) {
  char url[512], body[256];
  RedePedido req;
  RedeResposta res;
  snprintf(url, sizeof url, "%s/__scenario", base);
  snprintf(body, sizeof body, "{\"scenario\":\"%s\",\"delay_s\":%.2f,\"reset_hits\":%s}",
           scenario, delayS, resetHits ? "true" : "false");
  memset(&req, 0, sizeof req);
  memset(&res, 0, sizeof res);
  req.metodo = "POST";
  req.url = url;
  req.corpo = body;
  req.n_corpo = strlen(body);
  req.prazo_ms = 2000;
  req.max_bytes = 4096;
  rede_pedir(&req, &res);
  assert(res.erro == REDE_OK && res.status == 200);
  rede_resposta_limpar(&res);
}

static int srvHits(const char *base) {
  char url[512];
  RedePedido req;
  RedeResposta res;
  int hits = -1;
  snprintf(url, sizeof url, "%s/__hits", base);
  memset(&req, 0, sizeof req);
  memset(&res, 0, sizeof res);
  req.metodo = "GET";
  req.url = url;
  req.prazo_ms = 2000;
  req.max_bytes = 4096;
  rede_pedir(&req, &res);
  assert(res.erro == REDE_OK && res.status == 200 && res.corpo);
  const char *p = strstr(res.corpo, "\"hits\":");
  assert(p != NULL);
  hits = atoi(p + 7);
  rede_resposta_limpar(&res);
  return hits;
}

static void barreiraEsperarDentro(void) {
  long long ini = agoraMs();
  for (;;) {
    int d;
    pthread_mutex_lock(&B_mx);
    d = B_dentro;
    pthread_mutex_unlock(&B_mx);
    if (d) return;
    assert(agoraMs() - ini < 3000 && "recomenda_cabecalhos nunca foi chamado");
    usleep(2000);
  }
}

// GET simples ao servidor de teste; devolve corpo alocado (ou NULL).
static char *srvGet(const char *base, const char *caminho) {
  char url[512];
  RedePedido req;
  RedeResposta res;
  char *out = NULL;
  snprintf(url, sizeof url, "%s%s", base, caminho);
  memset(&req, 0, sizeof req);
  memset(&res, 0, sizeof res);
  req.metodo = "GET";
  req.url = url;
  req.prazo_ms = 2000;
  req.max_bytes = 65536;
  rede_pedir(&req, &res);
  assert(res.erro == REDE_OK && res.status == 200);
  out = (char *)malloc(res.n_corpo + 1);
  assert(out);
  if (res.n_corpo) memcpy(out, res.corpo, res.n_corpo);
  out[res.n_corpo] = '\0';
  rede_resposta_limpar(&res);
  return out;
}

static int contarSub(const char *s, const char *sub) {
  int n = 0;
  size_t k = strlen(sub);
  for (s = strstr(s, sub); s; s = strstr(s + k, sub)) n++;
  return n;
}

// Prova 1-para-1: o corpo tem exatamente nCand candidatos, e "c<k>" do JSON
// e o MESMO id que a lista adotada devolve em cand[k].
static void conferirCnContraCorpo(const char *corpo, const FarolComporCand *cand, int nCand) {
  int k;
  assert(contarSub(corpo, "{\"c\":\"c") == nCand);
  for (k = 0; k < nCand; k++) {
    char tag[64];
    snprintf(tag, sizeof tag, "{\"c\":\"c%d\",\"id\":\"%s\"", k, cand[k].imdb);
    if (!strstr(corpo, tag)) {
      fprintf(stderr, "c%d -> %s nao bate com o corpo enviado\n", k, cand[k].imdb);
      assert(0 && "cN do corpo nao corresponde ao candidato adotado");
    }
  }
}

static void esperarWorker(long maxMs, FarolComporInfo *out) {
  long long ini = agoraMs();
  for (;;) {
    FarolComporInfo inf;
    farolcompor_estado(&inf);
    if (!farolcompor_em_voo()) {
      if (out) *out = inf;
      return;
    }
    assert(agoraMs() - ini < maxMs);
    usleep(10000);
  }
}

static void resetAmbiente(const char *base, const char *dir) {
  farolcompor_teste_zera();
  farolcompor_definir_url(base);
  farolcompor_definir_auth("Authorization: Bearer tok_segredo_compor_99",
                           "X-Nuvio-Auth: nuvio", "X-Nuvio-Perfil: 1");
  farolcompor_relogio(relogioTeste);
  farolcompor_pasta(dir);
  farolcompor_esquecer();
  farolcompor_teste_home_visivel(homeVisTeste);
  farolcompor_definir_consentimento(1);
  S_offline = 0; S_infantil = 0; S_player = 0; S_seguro = 0; S_recomendar = 1;
  S_perfil = 1; S_sessao = 1;
  snprintf(S_usuario, sizeof S_usuario, "conta_canario_1");
  S_homeVis = 0; S_traktRecusadas = 0;
  S_relogio = 1795000000LL;
}

static void popularCandBasicos(FarolComporCand *cand, int n) {
  int i;
  for (i = 0; i < n; i++) {
    memset(&cand[i], 0, sizeof cand[i]);
    snprintf(cand[i].imdb, sizeof cand[i].imdb, "tt%07d", 1000000 + i);
    cand[i].tmdb = 500 + i;
    cand[i].tipo = (i % 4 == 0) ? 1 : 0;
    snprintf(cand[i].titulo, sizeof cand[i].titulo, "Titulo Publico %d", i);
    cand[i].ano = 2020 + (i % 5);
    cand[i].minutos = 88 + (i % 30);
    cand[i].generos[0] = 53;
    cand[i].generos[1] = -98765; // hash local negativo NAO deve ir ao servidor
    cand[i].nGeneros = 2;
    cand[i].nota10 = 78;
    cand[i].pontos = 12000 - i * 100;
    cand[i].motivos[0].tipo = FMO_AMIGO;
    cand[i].motivos[0].a = 999888; // DEVE ser zerado/anonimizado no JSON
    cand[i].motivos[0].b = 777666;
    cand[i].motivos[1].tipo = FMO_TERMINOU;
    cand[i].motivos[1].a = 0;      // refere ancora[0] (o teste puro varia)
    cand[i].nMotivos = 2;
    cand[i].cabeHoje = 1;
  }
}

static void seedGostoRico(FarolGosto *g, uint16_t dia) {
  int i;
  farolgosto_iniciar(g, 1);
  g->diaBase = dia;
  g->titulos = 12; // nivel 2
  for (i = 0; i < 10; i++) {
    g->genero[i].id = (i == 0) ? -424242 : (20 + i); // negativo local deve ser filtrado
    g->genero[i].peso = (int16_t)(900 - i * 40);
    g->genero[i].n = 2;
    g->genero[i].dia = dia;
    g->tema[i].id = (i == 1) ? -515151 : (400 + i);
    g->tema[i].peso = (int16_t)(800 - i * 30);
    g->tema[i].n = 2;
    g->tema[i].dia = dia;
  }
  for (i = 0; i < 8; i++) {
    g->pessoa[i].id = (i == 0) ? -616161 : (1000 + i);
    g->pessoa[i].peso = (int16_t)(850 - i * 50);
    g->pessoa[i].n = 2;
    g->pessoa[i].dia = dia;
  }
}

static const char *lookupPessoaCanario(int32_t id, void *u) {
  (void)u;
  static char buf[48];
  if (id <= 0) return "HASH_NEGATIVO_PROIBIDO";
  snprintf(buf, sizeof buf, "Pessoa_%d", (int)id);
  return buf;
}

static const char *lookupTemaCanario(int32_t id, void *u) {
  (void)u;
  static char buf[48];
  if (id <= 0) return "TEMA_NEGATIVO_PROIBIDO";
  snprintf(buf, sizeof buf, "tema_%d", (int)id);
  return buf;
}

// ---------- 1. Testes puros de privacidade, limites, canary e validacao ----
static void testarPurosPrivacidadeEContrato(void) {
  FarolGosto g;
  FarolComporAfin gen[FAROL_COMPOR_GEN_MAX], tem[FAROL_COMPOR_TEMA_MAX], pes[FAROL_COMPOR_PESSOA_MAX];
  int ng = 0, nt = 0, np = 0, i;
  FarolComporCand cand[32];
  FarolComporAncora anc[5];
  FarolComporFato fatos[4];
  FarolComporPedido ped;
  char corpo[FAROL_COMPOR_CORPO_MAX + 256];
  size_t nb;

  // Simulacao de Diario canario local que JAMAIS pode aparecer no corpo HTTP:
  const char *canaryDiarioTitle = "CANARIO_DIARIO_SEGREDO_991";
  const char *canaryPerfil = "Perfil_Privado_Canario";
  const char *canaryAmigo = "Amigo_Canario_Joao";
  const char *canaryAddon = "https://addon-privado.example/jwt_tok_canario";
  (void)canaryDiarioTitle; (void)canaryPerfil; (void)canaryAmigo;

  seedGostoRico(&g, 100);
  farolcompor_extrair_gosto(&g, 100, lookupPessoaCanario, lookupTemaCanario, NULL,
                            gen, &ng, tem, &nt, pes, &np);
  assert(ng == FAROL_COMPOR_GEN_MAX);
  assert(nt == FAROL_COMPOR_TEMA_MAX);
  assert(np == FAROL_COMPOR_PESSOA_MAX);
  for (i = 0; i < ng; i++) assert(gen[i].id > 0);
  for (i = 0; i < nt; i++) assert(tem[i].id > 0);
  for (i = 0; i < np; i++) assert(pes[i].id > 0);

  popularCandBasicos(cand, 32);
  for (i = 0; i < 32; i++) cand[i].motivos[1].a = i % 5; // cita as 5 ancoras
  for (i = 0; i < 5; i++) {
    snprintf(anc[i].id, sizeof anc[i].id, "tt%07d", 9000000 + i);
    snprintf(anc[i].titulo, sizeof anc[i].titulo, "Ancora %d", i);
  }
  memset(fatos, 0, sizeof fatos);
  snprintf(fatos[0].chave, sizeof fatos[0].chave, "perfil"); // bloqueado pela lista de privacidade
  snprintf(fatos[0].valor, sizeof fatos[0].valor, "%s", canaryPerfil);
  snprintf(fatos[1].chave, sizeof fatos[1].chave, "duracao_min");
  fatos[1].inteiro = 92;
  fatos[1].temInteiro = 1;
  snprintf(fatos[2].chave, sizeof fatos[2].chave, "addon");
  snprintf(fatos[2].valor, sizeof fatos[2].valor, "%s", canaryAddon); // URL bloqueada por textoSeguroLimpar

  memset(&ped, 0, sizeof ped);
  ped.modo = FC_MODO_HOME;
  ped.idioma = "pt-BR";
  ped.hora = 2;
  ped.fimSemana = 0;
  ped.nivel = 2;
  ped.infantil = 0;
  ped.consentiu = 1;
  ped.generos = gen; ped.nGeneros = ng;
  ped.temas = tem; ped.nTemas = nt;
  ped.pessoas = pes; ped.nPessoas = np;
  ped.ancoras = anc; ped.nAncoras = 5;       // teto 3
  ped.candidatos = cand; ped.nCandidatos = 32; // teto 24
  ped.fatos = fatos; ped.nFatos = 3;
  ped.frase = "algo leve sem drama";

  nb = farolcompor_montar_corpo(&ped, corpo, sizeof corpo);
  assert(nb > 0 && nb <= FAROL_COMPOR_CORPO_MAX);
  assert(strstr(corpo, "\"c\":\"c23\"") != NULL);
  assert(strstr(corpo, "\"c\":\"c24\"") == NULL);
  assert(strstr(corpo, "tt9000000") != NULL);
  assert(strstr(corpo, "tt9000002") != NULL);
  assert(strstr(corpo, "tt9000003") == NULL); // somente <= 3 ancoras
  assert(strstr(corpo, "tt9000004") == NULL);
  assert(strstr(corpo, "999888") == NULL);    // id do amigo NUNCA vaza
  assert(strstr(corpo, "777666") == NULL);
  assert(strstr(corpo, "-98765") == NULL);
  assert(strstr(corpo, canaryPerfil) == NULL);
  assert(strstr(corpo, canaryAddon) == NULL);
  assert(strstr(corpo, "HASH_NEGATIVO") == NULL);
  assert(strstr(corpo, "{\"frase\":\"algo leve sem drama\"}") != NULL);

  // Sem consentimento ou infantil: NUNCA serializa.
  ped.consentiu = 0;
  assert(farolcompor_montar_corpo(&ped, corpo, sizeof corpo) == 0);
  ped.consentiu = 1;
  ped.infantil = 1;
  assert(farolcompor_montar_corpo(&ped, corpo, sizeof corpo) == 0);

  // Validacao de c<n>-binding e deteccao de {"origem":"regras"}
  {
    FarolCena cena;
    int ehRegras = 0;
    assert(farolcompor_validar_resposta(FC_MODO_HOME, "{\"origem\":\"regras\"}",
                                        strlen("{\"origem\":\"regras\"}"),
                                        cand, 24, &cena, &ehRegras) == 0);
    assert(ehRegras == 1);
    assert(farolcompor_validar_resposta(FC_MODO_HOME,
        "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"fileira\",\"id\":\"gosto1\","
        "\"titulo\":{\"m\":\"FT_FIL_HORA\"},"
        "\"forma\":\"cartaz\",\"itens\":[\"c0\",\"c1\",\"c2\",\"c3\",\"c4\",\"c99\"]}]}",
        strlen("{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"fileira\",\"id\":\"gosto1\","
               "\"titulo\":{\"m\":\"FT_FIL_HORA\"},"
               "\"forma\":\"cartaz\",\"itens\":[\"c0\",\"c1\",\"c2\",\"c3\",\"c4\",\"c99\"]}]}"),
        cand, 24, &cena, &ehRegras) < 0);
  }
}

// ---------- 1b. FAROL52-r1 P1 #1/#3 (puros): ancoras so referenciadas e cN compactado
static void testarPurosAncorasECnCompactado(void) {
  FarolComporCand cand[24];
  FarolComporAncora anc[3];
  FarolComporPedido ped;
  char corpo[FAROL_COMPOR_CORPO_MAX + 256];
  int mapa[FAROL_COMPOR_CAND_MAX], nMapa = -1, i, k;
  size_t nb;

  // Ancora 0 = canario de titulo privado sem referencia; 1 referenciada;
  // 2 so citada por candidato que a sanitizacao descarta.
  popularCandBasicos(cand, 6);
  snprintf(anc[0].id, sizeof anc[0].id, "tt2999919");
  snprintf(anc[0].titulo, sizeof anc[0].titulo, "CANARIO_DIARIO_TITULO_NAO_REFERENCIADO");
  snprintf(anc[1].id, sizeof anc[1].id, "tt9100001");
  snprintf(anc[1].titulo, sizeof anc[1].titulo, "Ancora Publica");
  snprintf(anc[2].id, sizeof anc[2].id, "tt9100002");
  snprintf(anc[2].titulo, sizeof anc[2].titulo, "Ancora Do Descartado");
  for (i = 0; i < 6; i++) cand[i].motivos[1].a = 1;
  cand[3].motivos[1].a = 2;
  snprintf(cand[3].titulo, sizeof cand[3].titulo, "Filme www.exemplo.com");

  memset(&ped, 0, sizeof ped);
  ped.modo = FC_MODO_HOME;
  ped.idioma = "pt-BR";
  ped.hora = 2; ped.fimSemana = 0; ped.nivel = 2; ped.consentiu = 1;
  ped.ancoras = anc; ped.nAncoras = 3;
  ped.candidatos = cand; ped.nCandidatos = 6;
  nb = farolcompor_montar_corpo_mapa(&ped, corpo, sizeof corpo, mapa, &nMapa);
  assert(nb > 0);
  assert(strstr(corpo, "CANARIO_DIARIO_TITULO_NAO_REFERENCIADO") == NULL);
  assert(strstr(corpo, "tt2999919") == NULL);
  assert(strstr(corpo, "\"ancoras\":[{\"id\":\"tt9100001\",\"titulo\":\"Ancora Publica\"}]") != NULL);
  assert(strstr(corpo, "tt9100002") == NULL);   // so o descartado citava
  assert(strstr(corpo, "www.exemplo.com") == NULL);
  assert(nMapa == 5);
  assert(mapa[0] == 0 && mapa[1] == 1 && mapa[2] == 2 && mapa[3] == 4 && mapa[4] == 5);
  {
    FarolComporCand comp[FAROL_COMPOR_CAND_MAX];
    for (k = 0; k < nMapa; k++) comp[k] = cand[mapa[k]];
    conferirCnContraCorpo(corpo, comp, nMapa);
  }
  // Nenhum motivo cita ancora -> zero ancoras no corpo.
  for (i = 0; i < 6; i++) cand[i].nMotivos = 1;
  nb = farolcompor_montar_corpo_mapa(&ped, corpo, sizeof corpo, mapa, &nMapa);
  assert(nb > 0 && strstr(corpo, "\"ancoras\"") == NULL);

  // Teto de 12 KB: 24 candidatos grandes + 1 descartado por sanitizacao.
  for (i = 0; i < 24; i++) {
    int g;
    memset(&cand[i], 0, sizeof cand[i]);
    snprintf(cand[i].imdb, sizeof cand[i].imdb, "tt30%05d", i + 1);
    cand[i].tmdb = 700 + i;
    // ~118 bytes de titulo publico
    snprintf(cand[i].titulo, sizeof cand[i].titulo,
             "Titulo Grande %02d Lorem ipsum dolor sit amet consectetur adipiscing elit sed do eiusmod tempor incididunt ut lab", i);
    cand[i].ano = 2021;
    cand[i].minutos = 101;
    for (g = 0; g < 8; g++) cand[i].generos[g] = 1000000000 + g;
    cand[i].nGeneros = 8;
    cand[i].nota10 = 77;
    cand[i].pontos = 30000 - i;
    cand[i].motivos[0].tipo = FMO_GENERO;  cand[i].motivos[0].a = 10402;
    cand[i].motivos[1].tipo = FMO_PESSOA;  cand[i].motivos[1].a = 1234567;
    cand[i].motivos[2].tipo = FMO_TEMA;    cand[i].motivos[2].a = 7654321;
    cand[i].motivos[3].tipo = FMO_TERMINOU; cand[i].motivos[3].a = (i < 23) ? 1 : 2;
    cand[i].nMotivos = 4;
    cand[i].faltam = 3; cand[i].minNoite = 40; cand[i].cabeHoje = 1;
  }
  snprintf(cand[2].titulo, sizeof cand[2].titulo, "Filme http://exemplo.com");
  ped.candidatos = cand; ped.nCandidatos = 24;
  nb = farolcompor_montar_corpo_mapa(&ped, corpo, sizeof corpo, mapa, &nMapa);
  assert(nb > 0 && nb <= FAROL_COMPOR_CORPO_MAX);
  assert(nMapa > 6 && nMapa < 23);                 // o teto cortou
  assert(mapa[0] == 0 && mapa[1] == 1 && mapa[2] == 3); // o sanitizado saiu
  for (k = 1; k < nMapa; k++) assert(mapa[k] > mapa[k - 1] && mapa[k] != 2);
  {
    FarolComporCand comp[FAROL_COMPOR_CAND_MAX];
    char tag[32];
    for (k = 0; k < nMapa; k++) comp[k] = cand[mapa[k]];
    conferirCnContraCorpo(corpo, comp, nMapa);
    snprintf(tag, sizeof tag, "\"c\":\"c%d\"", nMapa);
    assert(strstr(corpo, tag) == NULL);
  }
  // O unico que citava a ancora 2 (o ultimo) ficou de fora pelo teto: ela sai.
  assert(mapa[nMapa - 1] < 23);
  assert(strstr(corpo, "tt9100002") == NULL);
  assert(strstr(corpo, "tt9100001") != NULL);
}

// ---------- 2. Allowlist #53 e supressoes (player/child/consent/safe) ----
static int allowlistSeam(int perfil, void *u) {
  int *permitido = (int *)u;
  return perfil == *permitido;
}

static void testarSupressaoEAllowlist(const char *base, const char *dir) {
  FarolGosto g;
  FarolComporCand cand[8];
  FarolComporAncora anc[1];
  int perfilPermitido = 1;
  FarolComporInfo inf;

  popularCandBasicos(cand, 8);
  snprintf(anc[0].id, sizeof anc[0].id, "tt9000000");
  snprintf(anc[0].titulo, sizeof anc[0].titulo, "Ancora 0");
  seedGostoRico(&g, 100);

  resetAmbiente(base, dir);
  srvConfig(base, "ok_home", 0.0, 1);

  // 2a. Sem consentimento -> suprimido, 0 hits
  farolcompor_definir_consentimento(0);
  assert(farolcompor_pedir_home(&g, cand, 8, anc, 1, lookupPessoaCanario, NULL, 111) == 0);
  assert(farolcompor_estado(&inf) == FC_SUPRIMIDO);
  assert(inf.preservaRegra == 1);
  assert(srvHits(base) == 0);

  // 2b. Seam #53: permite perfil 1, nega perfil 2
  farolcompor_definir_allowlist(allowlistSeam, &perfilPermitido);
  assert(farolcompor_consentimento() == 1);
  S_perfil = 2;
  assert(farolcompor_consentimento() == 0);
  assert(farolcompor_pedir_home(&g, cand, 8, anc, 1, lookupPessoaCanario, NULL, 111) == 0);
  assert(srvHits(base) == 0);
  S_perfil = 1;
  farolcompor_definir_allowlist(NULL, NULL);
  farolcompor_definir_consentimento(1);

  // 2c. Infantil -> suprimido, 0 hits
  S_infantil = 1;
  assert(farolcompor_pedir_home(&g, cand, 8, anc, 1, lookupPessoaCanario, NULL, 111) == 0);
  assert(farolcompor_estado(&inf) == FC_SUPRIMIDO);
  assert(srvHits(base) == 0);
  S_infantil = 0;

  // 2d. Player aberto -> suprime novo pedido
  S_player = 1;
  assert(farolcompor_pedir_home(&g, cand, 8, anc, 1, lookupPessoaCanario, NULL, 111) == 0);
  assert(farolcompor_estado(&inf) == FC_SUPRIMIDO);
  assert(srvHits(base) == 0);
  S_player = 0;

  // 2e. Player abre NO MEIO do voo -> resposta recebida NAO pode ser adotada no passo!
  srvConfig(base, "ok_home", 0.25, 1);
  assert(farolcompor_pedir_home(&g, cand, 8, anc, 1, lookupPessoaCanario, NULL, 111) == 1);
  usleep(30000);
  S_player = 1;
  assert(farolcompor_passo() == 0);
  esperarWorker(3000, &inf);
  assert(farolcompor_passo() == 0);
  assert(farolcompor_cena_adotada(NULL, NULL, NULL, 0, NULL) == 0);
  S_player = 0;

  // 2f. Modo seguro e Recomendar desligado tambem suprimem
  resetAmbiente(base, dir);
  srvConfig(base, "ok_home", 0.0, 1);
  S_seguro = 1;
  assert(farolcompor_pedir_home(&g, cand, 8, anc, 1, lookupPessoaCanario, NULL, 111) == 0);
  assert(farolcompor_estado(&inf) == FC_SUPRIMIDO);
  S_seguro = 0;
  S_recomendar = 0;
  assert(farolcompor_pedir_home(&g, cand, 8, anc, 1, lookupPessoaCanario, NULL, 111) == 0);
  assert(farolcompor_estado(&inf) == FC_SUPRIMIDO);
  assert(srvHits(base) == 0);
  S_recomendar = 1;
}

// ---------- 3. Adocao no fio principal, cache 24h por perfil e cota 1/dia --
static void testarAdocaoCacheECotaDiaria(const char *base, const char *dir) {
  FarolGosto g;
  FarolComporCand cand[8], outCand[FAROL_COMPOR_CAND_MAX];
  FarolComporAncora anc[1];
  FarolCena cena;
  FarolComporInfo inf;
  uint32_t hm = 0;
  int nOut = 0;
  char arqP1[512], arqMeta1[512], arqP2[512];
  struct stat st;

  popularCandBasicos(cand, 8);
  snprintf(anc[0].id, sizeof anc[0].id, "tt9000000");
  snprintf(anc[0].titulo, sizeof anc[0].titulo, "Ancora 0");
  seedGostoRico(&g, 100);

  resetAmbiente(base, dir);
  srvConfig(base, "ok_home", 0.0, 1);

  unsigned gAntes = farolcompor_home_geracao();
  assert(farolcompor_pedir_home(&g, cand, 8, anc, 1, lookupPessoaCanario, NULL, 222) == 1);
  esperarWorker(3000, &inf);
  // ANTES de farolcompor_passo() (fio principal), NADA foi adotado ainda!
  assert(farolcompor_cena_adotada(NULL, NULL, NULL, 0, NULL) == 0);
  assert(farolcompor_home_geracao() == gAntes);

  // farolcompor_passo() adota na hora no arranque e grava farol-cena-p1.json.
  assert(farolcompor_passo() == 1);
  assert(farolcompor_home_geracao() == gAntes + 1);
  assert(farolcompor_cena_adotada(&cena, outCand, &nOut, FAROL_COMPOR_CAND_MAX, &hm) == 1);
  assert(hm == 222 && nOut == 8 && cena.nPecas >= 2);
  assert(strcmp(outCand[0].imdb, "tt1000000") == 0);
  assert(srvHits(base) == 1);

  snprintf(arqP1, sizeof arqP1, "%s/farol-cena-p1.json", dir);
  snprintf(arqMeta1, sizeof arqMeta1, "%s/farol-compor-p1.txt", dir);
  assert(stat(arqP1, &st) == 0 && st.st_size > 20);
  assert(stat(arqMeta1, &st) == 0);

  // Reinicio simulado dentro de 24 h: recarrega cache do perfil 1 SEM hit HTTP.
  farolcompor_encerrar();
  assert(farolcompor_cena_adotada(NULL, NULL, NULL, 0, NULL) == 0);
  assert(farolcompor_carregar_cache_home() == 1);
  assert(farolcompor_cena_adotada(&cena, outCand, &nOut, FAROL_COMPOR_CAND_MAX, NULL) == 1);
  assert(cena.pecas[0].tipo == FP_FILEIRA && cena.pecas[0].u.fileira.itens[0].c == -1);
  assert(strcmp(cena.pecas[0].u.fileira.itens[0].s, "tt1000000") == 0);
  assert(srvHits(base) == 1);

  // Trocar para perfil 2: cache p1 NAO vaza; perfil 2 faz 1 chamada propria.
  S_perfil = 2; S_sessao = 2;
  farolcompor_trocar_perfil();
  assert(farolcompor_cena_adotada(NULL, NULL, NULL, 0, NULL) == 0);
  assert(farolcompor_carregar_cache_home() == 0);
  srvConfig(base, "ok_home_2", 0.0, 0);
  assert(farolcompor_pedir_home(&g, cand, 8, anc, 1, lookupPessoaCanario, NULL, 333) == 1);
  esperarWorker(3000, NULL);
  assert(farolcompor_passo() == 1);
  assert(srvHits(base) == 2);
  snprintf(arqP2, sizeof arqP2, "%s/farol-cena-p2.json", dir);
  assert(stat(arqP2, &st) == 0);

  // Voltar para perfil 1 depois de > 24 h (proximo dia local): permite nova chamada.
  S_perfil = 1; S_sessao = 3;
  farolcompor_trocar_perfil();
  S_relogio += 86405LL; // +24h00m05s -> cache expira e dia local muda
  assert(farolcompor_carregar_cache_home() == 0); // expirado e apagado
  srvConfig(base, "ok_home", 0.0, 0);
  assert(farolcompor_pedir_home(&g, cand, 8, anc, 1, lookupPessoaCanario, NULL, 222) == 1);
  esperarWorker(3000, NULL);
  assert(farolcompor_passo() == 1);
  assert(srvHits(base) == 3);

  // Simular falha que nao gravou cache (apagar farol-cena-p1.json), mas no MESMO dia local:
  // cota de 1 chamada Home/dia bloqueia a 2a chamada!
  unlink(arqP1);
  farolcompor_encerrar();
  assert(farolcompor_pedir_home(&g, cand, 8, anc, 1, lookupPessoaCanario, NULL, 222) == 0);
  assert(farolcompor_estado(&inf) == FC_REGRAS && inf.motivo == FC_MOT_LIMITE);
  assert(srvHits(base) == 3);

  // Cache corrompido ou com perfil trocado em farol-cena-p1.json e rejeitado com seguranca
  {
    FILE *fp = fopen(arqP1, "wb");
    assert(fp != NULL);
    fputs("{corrompido", fp);
    fclose(fp);
    assert(farolcompor_carregar_cache_home() == 0);
    assert(stat(arqP1, &st) != 0);

    fp = fopen(arqP1, "wb");
    assert(fp != NULL);
    fprintf(fp,
      "{\"v\":1,\"tela\":\"home\",\"perfil\":2,\"feita\":%lld,\"vale_s\":86400,"
      "\"pecas\":[{\"t\":\"fileira\",\"id\":\"gosto1\",\"titulo\":{\"m\":\"FT_FIL_HORA\"},"
      "\"forma\":\"cartaz\",\"itens\":[\"tt1000000\",\"tt1000001\",\"tt1000002\",\"tt1000003\",\"tt1000004\",\"tt1000005\"]}]}",
      S_relogio - 60);
    fclose(fp);
    assert(farolcompor_carregar_cache_home() == 0);
    unlink(arqP1);
  }

  farolcompor_esquecer(); // limpa p1 e p2 enquanto pastaOverride esta ativa
  assert(stat(arqP2, &st) != 0);
  farolcompor_teste_zera();
}

// ---------- 4. Falhas (regras, 429, 500, 401, offline, malformado, >8KB, c99, slow, cancel)
static void testarFallbacksECancelamento(const char *base, const char *dir) {
  FarolGosto g;
  FarolComporCand cand[8];
  FarolComporPedido ped;
  FarolComporInfo inf;
  const char *cenariosRuins[] = {
    "regras", "malformed", "oversize", "wrong_tela", "bad_ref", "status_429", "status_500"
  };
  size_t i;

  popularCandBasicos(cand, 8);
  seedGostoRico(&g, 100);
  resetAmbiente(base, dir);

  memset(&ped, 0, sizeof ped);
  ped.modo = FC_MODO_DICA; // sem limite diario da Home
  ped.idioma = "pt-BR";
  ped.hora = 2; ped.fimSemana = 0; ped.nivel = 2; ped.consentiu = 1;
  ped.candidatos = cand; ped.nCandidatos = 8;

  for (i = 0; i < sizeof cenariosRuins / sizeof *cenariosRuins; i++) {
    srvConfig(base, cenariosRuins[i], 0.0, 0);
    assert(farolcompor_pedir(&ped) == 1);
    esperarWorker(3000, &inf);
    assert(inf.estado == FC_REGRAS);
    assert(inf.preservaRegra == 1);
    assert(farolcompor_passo() == 0);
  }

  // 401 com via=trakt chama trakt_http_recusada
  farolcompor_definir_auth("Authorization: Bearer tok_segredo_compor_99",
                           "X-Nuvio-Auth: trakt", "X-Nuvio-Perfil: 1");
  srvConfig(base, "status_401", 0.0, 0);
  assert(farolcompor_pedir(&ped) == 1);
  esperarWorker(3000, &inf);
  assert(inf.http == 401 && inf.preservaRegra == 1);
  assert(S_traktRecusadas == 1);
  farolcompor_definir_auth("Authorization: Bearer tok_segredo_compor_99",
                           "X-Nuvio-Auth: nuvio", "X-Nuvio-Perfil: 1");

  // Offline preserva determinismo sem chamar servidor
  {
    int antes = srvHits(base);
    S_offline = 1;
    assert(farolcompor_pedir(&ped) == 0);
    farolcompor_estado(&inf);
    assert(inf.motivo == FC_MOT_OFFLINE && inf.preservaRegra == 1);
    assert(srvHits(base) == antes);
    S_offline = 0;
  }

  // Cancelamento rapido (< 300 ms) durante resposta lenta
  srvConfig(base, "ok_dica", 4.0, 0);
  assert(farolcompor_pedir(&ped) == 1);
  usleep(40000);
  {
    long long t0 = agoraMs();
    farolcompor_cancelar();
    esperarWorker(1000, &inf);
    assert(agoraMs() - t0 < 300);
    assert(inf.estado == FC_CANCELADO && inf.preservaRegra == 1);
  }

  // Prazo rigido de 6 s contra servidor lento (> 7 s)
  srvConfig(base, "ok_dica", 7.2, 0);
  {
    long long t0 = agoraMs();
    assert(farolcompor_pedir(&ped) == 1);
    esperarWorker(8000, &inf);
    long long dt = agoraMs() - t0;
    assert(dt >= 5700 && dt <= 6800);
    assert(inf.estado == FC_REGRAS && inf.motivo == FC_MOT_PRAZO && inf.preservaRegra == 1);
  }
}

// ---------- 5. Comparar, Dica, Retomada, UI visibility defer e zerar -----
static void testarModosExtrasEVisibilidade(const char *base, const char *dir) {
  FarolComporCand cand[8];
  FarolComporPedido ped;
  FarolCena cena;
  uint64_t gOut = 0;
  int nCand = 0;

  popularCandBasicos(cand, 8);
  resetAmbiente(base, dir);

  // Comparar com frase ditada
  srvConfig(base, "ok_comparar", 0.0, 1);
  memset(&ped, 0, sizeof ped);
  ped.modo = FC_MODO_COMPARAR;
  ped.idioma = "pt-BR";
  ped.hora = 2; ped.fimSemana = 0; ped.nivel = 2; ped.consentiu = 1;
  ped.candidatos = cand; ped.nCandidatos = 8;
  ped.frase = "quero um filme rapido de suspense";
  assert(farolcompor_pedir(&ped) == 1);
  esperarWorker(3000, NULL);
  assert(farolcompor_passo() == 1);
  assert(farolcompor_cena_modo(FC_MODO_COMPARAR, &cena, NULL, &nCand, 0, &gOut) == 1);
  assert(cena.pecas[0].tipo == FP_COMPARAR && cena.pecas[0].u.comparar.escolha == 0);

  // Retomada
  srvConfig(base, "ok_retomada", 0.0, 0);
  ped.modo = FC_MODO_RETOMADA;
  ped.frase = NULL;
  assert(farolcompor_pedir(&ped) == 1);
  esperarWorker(3000, NULL);
  assert(farolcompor_passo() == 1);
  assert(farolcompor_cena_modo(FC_MODO_RETOMADA, &cena, NULL, NULL, 0, NULL) == 1);
  assert(cena.pecas[0].tipo == FP_PORQUE && cena.pecas[1].tipo == FP_INFO);

  // Adocao Home adiada enquanto homeVisivel() == 1 quando ja existe Home adotada
  srvConfig(base, "ok_home", 0.0, 0);
  ped.modo = FC_MODO_HOME;
  ped.hModelo = 555;
  unsigned g0 = farolcompor_home_geracao();
  assert(farolcompor_pedir(&ped) == 1);
  esperarWorker(3000, NULL);
  assert(farolcompor_passo() == 1);
  assert(farolcompor_home_geracao() == g0 + 1);

  // Simula proximo dia e Home na tela: carrega resposta, mas passo() NAO troca ainda
  S_relogio += 86405LL;
  S_homeVis = 1;
  srvConfig(base, "ok_home_2", 0.0, 0);
  assert(farolcompor_pedir(&ped) == 1);
  esperarWorker(3000, NULL);
  assert(farolcompor_passo() == 0); // nao troca cartaz com o usuario olhando!
  assert(farolcompor_home_geracao() == g0 + 1);
  S_homeVis = 0;
  assert(farolcompor_passo() == 1); // Home saiu -> promove!
  assert(farolcompor_home_geracao() == g0 + 2);

  // farolcompor_zerar_gosto apaga cache do perfil ativo e invalida adotada
  {
    char arqP1[512];
    struct stat st;
    snprintf(arqP1, sizeof arqP1, "%s/farol-cena-p1.json", dir);
    assert(stat(arqP1, &st) == 0);
    farolcompor_zerar_gosto();
    assert(stat(arqP1, &st) != 0);
    assert(farolcompor_cena_adotada(NULL, NULL, NULL, 0, NULL) == 0);
  }
}

// ---------- 6. FAROL52-r1 P1 #2: barreira em recomenda_cabecalhos ----------
typedef struct {
  const FarolGosto       *g;
  const FarolComporCand  *cand;
  int                     nCand;
  const FarolComporAncora *anc;
  int                     nAnc;
  int                     rc;
} PedidoFio;

static void *fioMontagemHome(void *u) {
  PedidoFio *pf = (PedidoFio *)u;
  // A montagem da Home roda fora do fio principal no app (descoberta.c).
  pf->rc = farolcompor_pedir_home(pf->g, pf->cand, pf->nCand, pf->anc, pf->nAnc,
                                  lookupPessoaCanario, NULL, 444);
  return NULL;
}

static int permitePerfil1(int perfil, void *u) { (void)u; return perfil == 1; }

// Arma a barreira, dispara a montagem num fio e espera a resolucao das
// credenciais travar; `mudar` roda com ela segura; depois libera.
static void rodarComBarreira(const char *base, const char *dir, void (*mudar)(void),
                             int esperaHits, int esperaMot, long segurarMs) {
  FarolGosto g;
  FarolComporCand cand[8];
  FarolComporAncora anc[1];
  FarolComporInfo inf;
  PedidoFio pf;
  pthread_t t;

  popularCandBasicos(cand, 8);
  snprintf(anc[0].id, sizeof anc[0].id, "tt9000000");
  snprintf(anc[0].titulo, sizeof anc[0].titulo, "Ancora 0");
  seedGostoRico(&g, 100);

  resetAmbiente(base, dir);
  farolcompor_definir_auth(NULL, NULL, NULL);   // usa recomenda_cabecalhos
  srvConfig(base, "ok_home", 0.0, 1);
  barreiraArmar();
  memset(&pf, 0, sizeof pf);
  pf.g = &g; pf.cand = cand; pf.nCand = 8; pf.anc = anc; pf.nAnc = 1; pf.rc = -1;
  assert(pthread_create(&t, NULL, fioMontagemHome, &pf) == 0);
  barreiraEsperarDentro();
  if (mudar) mudar();
  if (segurarMs > 0) usleep((useconds_t)segurarMs * 1000);
  {
    long long t0 = agoraMs();
    barreiraLiberar();
    pthread_join(t, NULL);
    esperarWorker(3000, &inf);
    if (segurarMs > 0) assert(agoraMs() - t0 < 1000); // nao abre POST tardio
  }
  if (srvHits(base) != esperaHits) {
    fprintf(stderr, "barreira: esperava %d POST, vieram %d\n", esperaHits, srvHits(base));
    assert(0 && "POST fora do contexto autorizado");
  }
  if (esperaMot >= 0) assert((int)inf.motivo == esperaMot);
  assert(inf.preservaRegra == 1 || esperaHits == 1);
  if (esperaHits == 0) {
    assert(farolcompor_passo() == 0);
    assert(farolcompor_cena_adotada(NULL, NULL, NULL, 0, NULL) == 0);
  }
}

static void mudarAbrePlayerEPasso(void) {
  S_player = 1;
  assert(farolcompor_passo() == 0);
}
static void mudarAbrePlayerSemPasso(void) { S_player = 1; }
static void mudarPerfilInfantilSemConsent(void) {
  S_perfil = 2; S_sessao = 2; S_infantil = 1;
  farolcompor_definir_allowlist(permitePerfil1, NULL); // perfil 2 sem consentimento
  farolcompor_trocar_perfil();
  assert(farolcompor_passo() == 0);
}
static void mudarPerfilSemAviso(void) {
  // Troca crua (sem trocar_perfil/passo): so a revalidacao do worker segura.
  S_perfil = 2; S_sessao = 2; S_infantil = 1;
  farolcompor_definir_allowlist(permitePerfil1, NULL);
}

static void barreiraControle(const char *base, const char *dir) {
  // Controle positivo: nada muda -> 1 POST com o perfil certo e adocao.
  rodarComBarreira(base, dir, NULL, 1, FC_MOT_OK, 0);
  {
    char *pf = srvGet(base, "/__last_perfil");
    assert(!strcmp(pf, "1"));
    free(pf);
  }
  assert(farolcompor_passo() == 1);
  assert(B_chamadas == 1);
  puts("ok  barreira: sem mudanca, 1 POST com X-Nuvio-Perfil: 1");
}
static void barreiraA(const char *base, const char *dir) {
  // Caso A: player abre e o fio principal roda farolcompor_passo().
  rodarComBarreira(base, dir, mudarAbrePlayerEPasso, 0, -1, 0);
  S_player = 0;
  puts("ok  barreira caso A: player aberto + passo -> 0 POST");
}
static void barreiraA2(const char *base, const char *dir) {
  // Caso A': player abre sem passo -> a revalidacao no worker segura.
  rodarComBarreira(base, dir, mudarAbrePlayerSemPasso, 0, FC_MOT_SUPRIMIDO, 0);
  S_player = 0;
  puts("ok  barreira caso A': player aberto sem passo -> 0 POST");
}
static void barreiraB(const char *base, const char *dir) {
  // Caso B: perfil 1 -> perfil 2 infantil e sem consentimento.
  rodarComBarreira(base, dir, mudarPerfilInfantilSemConsent, 0, -1, 0);
  farolcompor_definir_allowlist(NULL, NULL);
  puts("ok  barreira caso B: troca para infantil sem consentimento -> 0 POST");
}
static void barreiraB2(const char *base, const char *dir) {
  // Caso B': a troca crua, sem trocar_perfil nem passo.
  rodarComBarreira(base, dir, mudarPerfilSemAviso, 0, FC_MOT_SUPRIMIDO, 0);
  farolcompor_definir_allowlist(NULL, NULL);
  puts("ok  barreira caso B': troca de perfil crua -> 0 POST");
}
static void barreiraPrazo(const char *base, const char *dir) {
  // Orcamento: a resolucao das credenciais conta no prazo de 6 s.
  rodarComBarreira(base, dir, NULL, 0, FC_MOT_PRAZO, 6150);
  puts("ok  barreira: credenciais acima do prazo de 6 s -> 0 POST, FC_MOT_PRAZO");
  farolcompor_teste_zera();
}

// ---------- 7. FAROL52-r1 P1 #3: cN compactado ate a adocao --------------
static void rodarCnHome(const char *base, const char *dir, FarolComporCand *cand, int n,
                        int esperaN, const char *esperaC0) {
  FarolGosto g;
  FarolComporCand outCand[FAROL_COMPOR_CAND_MAX];
  FarolCena cena;
  FarolComporInfo inf;
  int nOut = 0;
  char *corpo;

  seedGostoRico(&g, 100);
  resetAmbiente(base, dir);
  srvConfig(base, "ok_home", 0.0, 1);
  assert(farolcompor_pedir_home(&g, cand, n, NULL, 0, lookupPessoaCanario, NULL, 555) == 1);
  esperarWorker(3000, &inf);
  assert(inf.motivo == FC_MOT_OK);
  assert(farolcompor_passo() == 1);
  assert(farolcompor_cena_adotada(&cena, outCand, &nOut, FAROL_COMPOR_CAND_MAX, NULL) == 1);
  corpo = srvGet(base, "/__last_raw");
  if (esperaN > 0) assert(nOut == esperaN);
  conferirCnContraCorpo(corpo, outCand, nOut);
  // O item "c0" da cena adotada e o cartaz que a Home renderiza.
  assert(cena.pecas[0].tipo == FP_FILEIRA && cena.pecas[0].u.fileira.itens[0].c == 0);
  if (strcmp(outCand[0].imdb, esperaC0) != 0) {
    fprintf(stderr, "c0 adotado=%s esperado=%s\n", outCand[0].imdb, esperaC0);
    assert(0 && "c0 resolveu para o cartaz errado");
  }
  free(corpo);
  {
    // O cache em disco tambem resolve c0 para o mesmo id.
    char arq[600];
    FILE *fp;
    char buf[8192];
    size_t nb;
    snprintf(arq, sizeof arq, "%s/farol-cena-p1.json", dir);
    fp = fopen(arq, "rb");
    assert(fp);
    nb = fread(buf, 1, sizeof buf - 1, fp);
    fclose(fp);
    buf[nb] = '\0';
    {
      char tag[64];
      snprintf(tag, sizeof tag, "\"itens\":[\"%s\"", esperaC0);
      assert(strstr(buf, tag) != NULL);
    }
  }
}

static void testarCnCompactadoHome(const char *base, const char *dir) {
  FarolComporCand cand[24];
  int i, g;
  // 10 candidatos validos, o 1o com URL no titulo (sanitizacao o descarta).
  for (i = 0; i < 10; i++) {
    memset(&cand[i], 0, sizeof cand[i]);
    snprintf(cand[i].imdb, sizeof cand[i].imdb, "tt20100%02d", i + 1);
    cand[i].tmdb = 800 + i;
    snprintf(cand[i].titulo, sizeof cand[i].titulo, "Filme Publico %d", i + 1);
    cand[i].ano = 2022;
    cand[i].minutos = 95;
    cand[i].generos[0] = 53;
    cand[i].nGeneros = 1;
    cand[i].nota10 = 70;
  }
  snprintf(cand[0].titulo, sizeof cand[0].titulo, "Filme www.exemplo.com");
  rodarCnHome(base, dir, cand, 10, 9, "tt2010002");
  puts("ok  cN: candidato sanitizado sai e c0 adota tt2010002");

  // Teto de 12 KB + sanitizacao no meio.
  for (i = 0; i < 24; i++) {
    memset(&cand[i], 0, sizeof cand[i]);
    snprintf(cand[i].imdb, sizeof cand[i].imdb, "tt40%05d", i + 1);
    cand[i].tmdb = 900 + i;
    snprintf(cand[i].titulo, sizeof cand[i].titulo,
             "Titulo Grande %02d Lorem ipsum dolor sit amet consectetur adipiscing elit sed do eiusmod tempor incididunt ut lab", i);
    cand[i].ano = 2021;
    cand[i].minutos = 101;
    for (g = 0; g < 8; g++) cand[i].generos[g] = 1000000000 + g;
    cand[i].nGeneros = 8;
    cand[i].nota10 = 77;
    cand[i].pontos = 30000 - i;
    cand[i].motivos[0].tipo = FMO_GENERO;  cand[i].motivos[0].a = 10402;
    cand[i].motivos[1].tipo = FMO_PESSOA;  cand[i].motivos[1].a = 1234567;
    cand[i].motivos[2].tipo = FMO_TEMA;    cand[i].motivos[2].a = 7654321;
    cand[i].motivos[3].tipo = FMO_CABE_HOJE;
    cand[i].nMotivos = 4;
    cand[i].faltam = 3; cand[i].minNoite = 40; cand[i].cabeHoje = 1;
  }
  snprintf(cand[0].titulo, sizeof cand[0].titulo, "Filme http://exemplo.com");
  snprintf(cand[5].titulo, sizeof cand[5].titulo, "contato@exemplo.com");
  rodarCnHome(base, dir, cand, 24, -1, "tt4000002");
  {
    FarolComporCand outCand[FAROL_COMPOR_CAND_MAX];
    int nOut = 0;
    assert(farolcompor_cena_adotada(NULL, outCand, &nOut, FAROL_COMPOR_CAND_MAX, NULL) == 1);
    assert(nOut > 6 && nOut < 22);  // o teto cortou de verdade
    assert(!strcmp(outCand[3].imdb, "tt4000005") && !strcmp(outCand[4].imdb, "tt4000007"));
  }
  puts("ok  cN: teto de 12 KB + sanitizacao, 1-para-1 corpo x adocao");
  farolcompor_teste_zera();
}

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "uso: %s <base_url> <pasta_dados>\n", argv[0]);
    return 2;
  }
  const char *base = argv[1];
  const char *dir = argv[2];
  // FAROL_COMPOR_CASO=<nome> roda um grupo so (diagnostico); vazio = todos.
  const char *caso = getenv("FAROL_COMPOR_CASO");
#define CASO(nome) (!caso || !caso[0] || !strcmp(caso, nome))

  if (CASO("puros")) testarPurosPrivacidadeEContrato();
  if (CASO("ancoras_cn")) testarPurosAncorasECnCompactado();
  if (CASO("supressao")) testarSupressaoEAllowlist(base, dir);
  if (CASO("cache")) testarAdocaoCacheECotaDiaria(base, dir);
  if (CASO("falhas")) testarFallbacksECancelamento(base, dir);
  if (CASO("modos")) testarModosExtrasEVisibilidade(base, dir);
  if (CASO("barreira_controle")) barreiraControle(base, dir);
  if (CASO("barreira_A")) barreiraA(base, dir);
  if (CASO("barreira_A2")) barreiraA2(base, dir);
  if (CASO("barreira_B")) barreiraB(base, dir);
  if (CASO("barreira_B2")) barreiraB2(base, dir);
  if (CASO("barreira_prazo")) barreiraPrazo(base, dir);
  if (CASO("cn_home")) testarCnCompactadoHome(base, dir);
#undef CASO

  farolcompor_teste_zera();
  return 0;
}
