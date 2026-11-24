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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// Estado simulado dos ganchos do app (sobrescrevem os fracos de farolcompor.c).
static int        S_offline = 0;
static int        S_infantil = 0;
static int        S_player = 0;
static int        S_seguro = 0;
static int        S_recomendar = 1;
static int        S_perfil = 1;
static uint64_t   S_sessao = 1;
static char       S_usuario[128] = "conta_canario_1";
static int        S_homeVis = 0;
static int        S_traktRecusadas = 0;
static long long  S_relogio = 1795000000LL; // 2026-11-18 ~ 11:06 UTC

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
    cand[i].motivos[1].a = 0;      // refere ancora[0]
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
  assert(strstr(corpo, "tt9000002") != NULL);
  assert(strstr(corpo, "tt9000003") == NULL); // somente <= 3 ancoras
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

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "uso: %s <base_url> <pasta_dados>\n", argv[0]);
    return 2;
  }
  const char *base = argv[1];
  const char *dir = argv[2];

  testarPurosPrivacidadeEContrato();
  testarSupressaoEAllowlist(base, dir);
  testarAdocaoCacheECotaDiaria(base, dir);
  testarFallbacksECancelamento(base, dir);
  testarModosExtrasEVisibilidade(base, dir);

  farolcompor_teste_zera();
  return 0;
}
