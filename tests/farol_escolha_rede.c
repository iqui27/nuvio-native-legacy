// tests/farol_escolha_rede.c — testes completos da issue Farol #54
// ("Assistente por frase, Android primeiro" — farolescolha_rede + UI P5 + FAROL50-r2).
//
// Dois modos de compilacao:
//   -DFER_SO_REDE : modulo de rede + motor puro (sem SDL) — roda em GCC/Clang
//                   normal, ASan+UBSan e Clang TSan (concorrencia, auth lenta,
//                   prazo 6 s, cancelamento, geracao, auditoria de chaves/canario,
//                   privacidade/consentimento, validacao estrita, "Mistério").
//   padrao        : app inteiro (menos main.c) — fluxo real no Resolver/P5:
//                   botao microfone, "Ouvindo…" + parciais, previa com 0 POSTs,
//                   Voltar descarta com 0 POSTs, key.repeat=1 ignorado, caso
//                   canonico, falta priorizada, P4 com guarda todosImdb, cartaz
//                   anti-TOCTOU, "Nada exato…", falhas/429/503/regras, privacidade,
//                   farolescolhaui_aplicar_frase e capturas PNG 1920x1080.
#include "farolescolha.h"
#include "farolescolha_rede.h"
#include "farolconsent.h"
#include "farolgosto.h"
#include "rede.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int gFalhas;
#define CHECK(c) do { if (!(c)) { \
  fprintf(stderr, "%s:%d: FALHA: %s\n", __FILE__, __LINE__, #c); \
  gFalhas++; exit(1); \
} } while (0)

static long long monoMs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (long long)t.tv_sec * 1000LL + (long long)(t.tv_nsec / 1000000L);
}

static int lerContador(const char *path) {
  FILE *f = fopen(path, "rb");
  int v = 0;
  if (!f) return 0;
  if (fscanf(f, "%d", &v) != 1) v = 0;
  fclose(f);
  return v;
}

#ifdef FER_SO_REDE
static void esperarRedeTerminar(int maxMs) {
  long long lim = monoMs() + (long long)maxMs;
  while (monoMs() < lim) {
    FarolEscRedeInfo info;
    FarolEscRedeEstado st = farolescolha_rede_estado(&info);
    if (st != FER_BUSCANDO && !farolescolha_rede_em_voo()) return;
    usleep(5000);
  }
}
#endif

// ------------------------------------------------ prova FAROL50-r2 ("Mistério")
static void provaMisterio(void) {
  FarolEscFatos f;
  FarolEscCand c;
  memset(&f, 0, sizeof f);
  f.minutos = 95;
  f.ano = 2021;
  f.genero = "Filme · Mistério";
  farolescolha_cand_de(&c, &f);
  CHECK((c.m[FESC_HUMOR] & (1u << FESC_TENSAO)) != 0);
  CHECK((c.m[FESC_PESO] & (1u << FESC_INTENSO)) != 0);

  f.genero = "MISTÉRIO";
  farolescolha_cand_de(&c, &f);
  CHECK((c.m[FESC_HUMOR] & (1u << FESC_TENSAO)) != 0);
  CHECK((c.m[FESC_PESO] & (1u << FESC_INTENSO)) != 0);

  f.genero = "Série · Misterio · Drama";
  farolescolha_cand_de(&c, &f);
  CHECK((c.m[FESC_HUMOR] & (1u << FESC_TENSAO)) != 0);
  CHECK((c.m[FESC_PESO] & (1u << FESC_INTENSO)) != 0);
  puts("ok  misterio: Filme · Mistério e MISTÉRIO casam tensao e intenso");
}

#ifdef FER_SO_REDE
// ======================================================================
// MODO 1: Modulo de rede + motor puro (sem SDL; roda em GCC/Clang ASan e TSan)
// ======================================================================
#include <pthread.h>

static int gInfantil;
static int gRecomendar = 1;
static int gConsent = FAROL_CONSENT_SIM;
static int gPrivado;
static int gPlayer;
static int gOffline;
static int gAuthAtrasoMs;
static int gAuthCancelou;
static int gAuthFalhar;
static int gAuthChamadas;
static int gTemGosto;
static FarolGosto gModelo;

int perfil_infantil_ativo(void) { return __atomic_load_n(&gInfantil, __ATOMIC_ACQUIRE); }
int farolsinal_recomendar(void) { return __atomic_load_n(&gRecomendar, __ATOMIC_ACQUIRE); }
int farolconsent_estado(void) { return __atomic_load_n(&gConsent, __ATOMIC_ACQUIRE); }
int farolconsent_llm_ativo(void) {
  return __atomic_load_n(&gConsent, __ATOMIC_ACQUIRE) == FAROL_CONSENT_SIM &&
         !__atomic_load_n(&gInfantil, __ATOMIC_ACQUIRE) &&
         __atomic_load_n(&gRecomendar, __ATOMIC_ACQUIRE);
}
int seguro_perfil_ativo(void) { return __atomic_load_n(&gPrivado, __ATOMIC_ACQUIRE); }
int player_aberto(void) { return __atomic_load_n(&gPlayer, __ATOMIC_ACQUIRE); }
int rede_saude_offline(void) { return __atomic_load_n(&gOffline, __ATOMIC_ACQUIRE); }
const char *ajustes_tmdb_idioma(void) { return "pt-BR"; }
int farolsinal_modelo(FarolGosto *dst) {
  if (!__atomic_load_n(&gTemGosto, __ATOMIC_ACQUIRE) || !dst) return 0;
  *dst = gModelo;
  return 1;
}

// Simula recomenda_cabecalhos com atraso controlado e respeito a rede_parar_trocar.
int recomenda_cabecalhos(const char **cab, char *aut, size_t na,
                         char *via, size_t nv, char *perfil, size_t np) {
  int atraso = __atomic_load_n(&gAuthAtrasoMs, __ATOMIC_ACQUIRE);
  int falhar = __atomic_load_n(&gAuthFalhar, __ATOMIC_ACQUIRE);
  long long fim;
  (void)cab;
  __atomic_add_fetch(&gAuthChamadas, 1, __ATOMIC_RELAXED);
  if (atraso > 0) {
    fim = monoMs() + (long long)atraso;
    while (monoMs() < fim) {
      if (rede_deve_parar()) {
        __atomic_store_n(&gAuthCancelou, 1, __ATOMIC_RELEASE);
        return 0;
      }
      usleep(10000);
    }
  }
  if (falhar) return 0;
  snprintf(aut, na, "Authorization: Bearer tok_segredo_esc54_auth");
  snprintf(via, nv, "X-Nuvio-Auth: nuvio");
  snprintf(perfil, np, "X-Nuvio-Perfil: 1");
  return 1;
}

static void resetHooks(const char *base) {
  farolescolha_rede_teste_zera();
  __atomic_store_n(&gInfantil, 0, __ATOMIC_RELEASE);
  __atomic_store_n(&gRecomendar, 1, __ATOMIC_RELEASE);
  __atomic_store_n(&gConsent, FAROL_CONSENT_SIM, __ATOMIC_RELEASE);
  __atomic_store_n(&gPrivado, 0, __ATOMIC_RELEASE);
  __atomic_store_n(&gPlayer, 0, __ATOMIC_RELEASE);
  __atomic_store_n(&gOffline, 0, __ATOMIC_RELEASE);
  __atomic_store_n(&gAuthAtrasoMs, 0, __ATOMIC_RELEASE);
  __atomic_store_n(&gAuthCancelou, 0, __ATOMIC_RELEASE);
  __atomic_store_n(&gAuthFalhar, 0, __ATOMIC_RELEASE);
  __atomic_store_n(&gAuthChamadas, 0, __ATOMIC_RELEASE);
  __atomic_store_n(&gTemGosto, 0, __ATOMIC_RELEASE);
  memset(&gModelo, 0, sizeof gModelo);
  farolescolha_rede_definir_url(base);
  farolescolha_rede_definir_auth("Authorization: Bearer tok_segredo_esc54_99",
                                 "X-Nuvio-Auth: nuvio", NULL);
}

static void montarGostoComCanarios(FarolGosto *g) {
  FarolGostoFicha f;
  farolgosto_iniciar(g, 3);
  memset(&f, 0, sizeof f);
  f.tmdb = 603;
  f.tipo = FG_TIPO_FILME;
  f.ano = 2022;
  f.minutos = 95;
  f.genero[0] = 35;      // Comedia (publico > 0)
  f.genero[1] = 10751;   // Familia (publico > 0)
  f.genero[2] = -98765;  // negativo local -> NUNCA vai
  f.pessoa[0] = 31;      // publico > 0
  f.pessoa[1] = -424242; // hash negativo -> NUNCA vai
  f.tema[0] = 9715;      // publico > 0
  f.tema[1] = -77777;    // negativo -> NUNCA vai
  farolgosto_aplicar(g, &f, FG_D_GOSTOU, 10, 2, 1);
  f.tmdb = 604;
  farolgosto_aplicar(g, &f, FG_D_GOSTOU, 10, 2, 1);
  f.tmdb = 605;
  farolgosto_aplicar(g, &f, FG_D_GOSTOU, 10, 2, 1);
  farolgosto_recusar(g, 0xdeadbeefu);
  farolgosto_mostrado(g, 0xcafebabeu, 10);
}

static void provaNormalizacaoECorpo(void) {
  char out[FAROL_ESCOLHA_FRASE_BYTES];
  char longa[1024];
  char corpo[FAROL_ESCOLHA_CORPO_MAX + 1];
  int pergs[3] = { FESC_TEMPO, FESC_PESO, FESC_COMPANHIA };
  int i, cps = 0;
  const unsigned char *p;
  FarolGosto g;

  // Trim e colapso de espacos.
  CHECK(farolescolha_rede_normalizar_frase("  algo   leve\tde\n uma hora  ", out, sizeof out));
  CHECK(!strcmp(out, "algo leve de uma hora"));

  // Vazia, controles, URLs -> rejeitadas.
  CHECK(!farolescolha_rede_normalizar_frase(NULL, out, sizeof out));
  CHECK(!farolescolha_rede_normalizar_frase("", out, sizeof out));
  CHECK(!farolescolha_rede_normalizar_frase("   \t\n  ", out, sizeof out));
  CHECK(!farolescolha_rede_normalizar_frase("algo \x07 leve", out, sizeof out));
  CHECK(!farolescolha_rede_normalizar_frase("algo \x7f leve", out, sizeof out));
  CHECK(!farolescolha_rede_normalizar_frase("veja http://exemplo.com", out, sizeof out));
  CHECK(!farolescolha_rede_normalizar_frase("HTTPS://site.org/x", out, sizeof out));
  CHECK(!farolescolha_rede_normalizar_frase("entra em www.teste.com", out, sizeof out));
  CHECK(!farolescolha_rede_normalizar_frase("WWW.SITE.BR", out, sizeof out));
  // UTF-8 invalido -> rejeitado.
  CHECK(!farolescolha_rede_normalizar_frase("abc\xFF\xFE", out, sizeof out));

  // > 200 chars UTF-8 com acentos multibyte (2 bytes cada: 'é' = C3 A9):
  // trunca em exatamente 200 codepoints sem cortar multibyte!
  for (i = 0; i < 230; i++) {
    longa[i * 2] = (char)0xC3;
    longa[i * 2 + 1] = (char)0xA9;
  }
  longa[460] = '\0';
  CHECK(farolescolha_rede_normalizar_frase(longa, out, sizeof out));
  CHECK(strlen(out) == 400); // 200 * 2 bytes, nenhum byte partido
  for (p = (const unsigned char *)out; *p; p++) {
    if ((*p & 0xC0) != 0x80) cps++;
  }
  CHECK(cps == FAROL_ESCOLHA_FRASE_MAX);

  // Corpo estrito + gosto sem IDs negativos nem campos proibidos.
  montarGostoComCanarios(&g);
  CHECK(farolescolha_rede_montar_corpo("algo leve de uma hora para ver com meu filho",
                                       "pt-BR", pergs, 3, &g, corpo, sizeof corpo) > 0);
  CHECK(strstr(corpo, "\"v\":1") != NULL);
  CHECK(strstr(corpo, "\"idioma\":\"pt-BR\"") != NULL);
  CHECK(strstr(corpo, "\"infantil\":0") != NULL);
  CHECK(strstr(corpo, "\"consentiu\":1") != NULL);
  CHECK(strstr(corpo, "\"perguntas\":[\"tempo\",\"peso\",\"companhia\"]") != NULL);
  CHECK(strstr(corpo, "-98765") == NULL);
  CHECK(strstr(corpo, "-424242") == NULL);
  CHECK(strstr(corpo, "-77777") == NULL);
  CHECK(strstr(corpo, "horaResto") == NULL);
  CHECK(strstr(corpo, "recusado") == NULL);
  CHECK(strstr(corpo, "perfil") == NULL);

  // Frase com URL no montar_corpo devolve 0.
  CHECK(farolescolha_rede_montar_corpo("veja https://exemplo.com", "pt-BR",
                                       pergs, 3, &g, corpo, sizeof corpo) == 0);
  puts("ok  normalizacao e corpo: <=200 UTF-8 sem corte, URLs barradas, zero ID negativo");
}

static void provaValidacaoEstrita(void) {
  int pergs[4] = { FESC_TEMPO, FESC_PESO, FESC_COMPANHIA, FESC_CONTINUAR };
  FarolEscRedeResp r;
  int ehRegras = 0;

  // Valida: origem=="llm", respostas validas, falta null.
  {
    const char *j = "{\"origem\":\"llm\",\"respostas\":{\"tempo\":\"t100\",\"peso\":\"leve\",\"companhia\":\"crianca\"},\"falta\":null}";
    CHECK(farolescolha_rede_validar_resposta(j, strlen(j), pergs, 4, &r, &ehRegras));
    CHECK(!ehRegras && r.nRespostas == 3 && !r.temFalta && r.faltaPerg == -1);
  }
  // Valida: falta antes de respostas + compatibilidade "continuar":"continuar".
  {
    const char *j = "{\"falta\":\"peso\",\"origem\":\"llm\",\"respostas\":{\"continuar\":\"continuar\"}}";
    CHECK(farolescolha_rede_validar_resposta(j, strlen(j), pergs, 4, &r, &ehRegras));
    CHECK(r.nRespostas == 1 && r.respostas[0].perg == FESC_CONTINUAR &&
          r.respostas[0].opc == FESC_CONT_SIM && !strcmp(r.respostas[0].opcaoId, "sim"));
    CHECK(r.temFalta && r.faltaPerg == FESC_PESO && !strcmp(r.faltaId, "peso"));
  }
  // Rejeitada: origem:"regras".
  {
    const char *j = "{\"origem\":\"regras\"}";
    CHECK(!farolescolha_rede_validar_resposta(j, strlen(j), pergs, 4, &r, &ehRegras));
    CHECK(ehRegras == 1);
  }
  // Rejeitada: falta repetida em respostas.
  {
    const char *j = "{\"origem\":\"llm\",\"respostas\":{\"tempo\":\"t45\"},\"falta\":\"tempo\"}";
    CHECK(!farolescolha_rede_validar_resposta(j, strlen(j), pergs, 4, &r, &ehRegras));
  }
  // Rejeitada: pergunta fora das enviadas (formato nao estava em pergs[4]).
  {
    const char *j = "{\"origem\":\"llm\",\"respostas\":{\"formato\":\"filme\"},\"falta\":null}";
    CHECK(!farolescolha_rede_validar_resposta(j, strlen(j), pergs, 4, &r, &ehRegras));
  }
  // Rejeitada: opcao invalida.
  {
    const char *j = "{\"origem\":\"llm\",\"respostas\":{\"tempo\":\"t30\"},\"falta\":null}";
    CHECK(!farolescolha_rede_validar_resposta(j, strlen(j), pergs, 4, &r, &ehRegras));
  }
  // Rejeitada: pergunta desconhecida.
  {
    const char *j = "{\"origem\":\"llm\",\"respostas\":{\"xpto\":\"sim\"},\"falta\":null}";
    CHECK(!farolescolha_rede_validar_resposta(j, strlen(j), pergs, 4, &r, &ehRegras));
  }
  // Rejeitada: resposta vazia.
  {
    const char *j = "{\"origem\":\"llm\",\"respostas\":{},\"falta\":null}";
    CHECK(!farolescolha_rede_validar_resposta(j, strlen(j), pergs, 4, &r, &ehRegras));
  }
  // Rejeitada: JSON torto / chave desconhecida.
  {
    const char *j1 = "{\"origem\":\"llm\",\"respostas\":{\"tempo\":\"t45\"},\"extra\":1}";
    const char *j2 = "{\"origem\":\"llm\",\"respostas\":{";
    CHECK(!farolescolha_rede_validar_resposta(j1, strlen(j1), pergs, 4, &r, &ehRegras));
    CHECK(!farolescolha_rede_validar_resposta(j2, strlen(j2), pergs, 4, &r, &ehRegras));
  }
  puts("ok  validacao estrita: llm/falta/continuar aceitos; regras/fora/invalida/vazia/torto barrados");
}

static void provaPrivacidadeZeroPosts(const char *base, const char *countFile) {
  int pergs[3] = { FESC_TEMPO, FESC_PESO, FESC_COMPANHIA };
  FarolEscRedeInfo info;
  int antes = lerContador(countFile);

  // 1. Infantil -> 0 POSTs.
  resetHooks(base);
  __atomic_store_n(&gInfantil, 1, __ATOMIC_RELEASE);
  CHECK(!farolescolha_rede_autorizado());
  CHECK(farolescolha_rede_pedir_perguntas("algo leve", pergs, 3, NULL) == 0);
  CHECK(farolescolha_rede_estado(&info) == FER_FALLBACK && info.motivo == FER_MOT_SUPRIMIDO);

  // 2. Sem consentimento (NAO_PERGUNTADO = 0) -> 0 POSTs.
  resetHooks(base);
  __atomic_store_n(&gConsent, FAROL_CONSENT_NAO_PERGUNTADO, __ATOMIC_RELEASE);
  CHECK(!farolescolha_rede_autorizado());
  CHECK(farolescolha_rede_pedir_perguntas("algo leve", pergs, 3, NULL) == 0);
  CHECK(farolescolha_rede_estado(&info) == FER_FALLBACK && info.motivo == FER_MOT_SUPRIMIDO);

  // 3. SO_TV (1) -> 0 POSTs.
  resetHooks(base);
  __atomic_store_n(&gConsent, FAROL_CONSENT_SO_TV, __ATOMIC_RELEASE);
  CHECK(!farolescolha_rede_autorizado());
  CHECK(farolescolha_rede_pedir_perguntas("algo leve", pergs, 3, NULL) == 0);
  CHECK(farolescolha_rede_estado(&info) == FER_FALLBACK && info.motivo == FER_MOT_SUPRIMIDO);

  // 4. recomendar = 0 -> 0 POSTs.
  resetHooks(base);
  __atomic_store_n(&gRecomendar, 0, __ATOMIC_RELEASE);
  CHECK(!farolescolha_rede_autorizado());
  CHECK(farolescolha_rede_pedir_perguntas("algo leve", pergs, 3, NULL) == 0);
  CHECK(farolescolha_rede_estado(&info) == FER_FALLBACK && info.motivo == FER_MOT_SUPRIMIDO);

  // 5. Privado (seguro_perfil_ativo = 1) -> 0 POSTs.
  resetHooks(base);
  __atomic_store_n(&gPrivado, 1, __ATOMIC_RELEASE);
  CHECK(!farolescolha_rede_autorizado());
  CHECK(farolescolha_rede_pedir_perguntas("algo leve", pergs, 3, NULL) == 0);
  CHECK(farolescolha_rede_estado(&info) == FER_FALLBACK && info.motivo == FER_MOT_SUPRIMIDO);

  // 6. Player aberto -> 0 POSTs.
  resetHooks(base);
  __atomic_store_n(&gPlayer, 1, __ATOMIC_RELEASE);
  CHECK(farolescolha_rede_pedir_perguntas("algo leve", pergs, 3, NULL) == 0);
  CHECK(farolescolha_rede_estado(&info) == FER_FALLBACK && info.motivo == FER_MOT_SUPRIMIDO);

  // 7. Offline -> 0 POSTs.
  resetHooks(base);
  __atomic_store_n(&gOffline, 1, __ATOMIC_RELEASE);
  CHECK(farolescolha_rede_pedir_perguntas("algo leve", pergs, 3, NULL) == 0);
  CHECK(farolescolha_rede_estado(&info) == FER_FALLBACK && info.motivo == FER_MOT_OFFLINE);

  // 8. Sem URL -> 0 POSTs.
  resetHooks("");
  CHECK(farolescolha_rede_pedir_perguntas("algo leve", pergs, 3, NULL) == 0);
  CHECK(farolescolha_rede_estado(&info) == FER_FALLBACK && info.motivo == FER_MOT_OFFLINE);

  // 9. Frase com URL -> 0 POSTs.
  resetHooks(base);
  CHECK(farolescolha_rede_pedir_perguntas("abre http://mal.example", pergs, 3, NULL) == 0);
  CHECK(farolescolha_rede_estado(&info) == FER_FALLBACK && info.motivo == FER_MOT_FRASE_INVALIDA);
  CHECK(farolescolha_rede_pedir_perguntas("veja www.mal.example", pergs, 3, NULL) == 0);
  CHECK(farolescolha_rede_estado(&info) == FER_FALLBACK && info.motivo == FER_MOT_FRASE_INVALIDA);

  CHECK(lerContador(countFile) == antes);
  puts("ok  privacidade e guardas: infantil/sem-consent/SO_TV/recomendar=0/privado/player/offline/sem-url/URL = 0 POSTs");
}

static void provaRedeResilienciaETSan(const char *base, const char *countFile) {
  char url[512];
  int pergs[5] = { FESC_TEMPO, FESC_PESO, FESC_COMPANHIA, FESC_FORMATO, FESC_HUMOR };
  FarolEscRedeInfo info;
  FarolEscRedeResp resp;
  uint64_t g1, g2;
  long long t0, dt;

  // 1. Caso canonico com gosto (inclui canarios que NAO podem vazar no JSON).
  snprintf(url, sizeof url, "%s/canonico", base);
  resetHooks(url);
  montarGostoComCanarios(&gModelo);
  __atomic_store_n(&gTemGosto, 1, __ATOMIC_RELEASE);
  t0 = monoMs();
  g1 = farolescolha_rede_pedir_perguntas("algo leve de uma hora para ver com meu filho",
                                         pergs, 5, NULL);
  dt = monoMs() - t0;
  CHECK(g1 > 0 && dt < 100);
  esperarRedeTerminar(3000);
  CHECK(farolescolha_rede_estado(&info) == FER_PRONTO && info.geracao == g1 && info.http == 200);
  CHECK(farolescolha_rede_resposta(g1, &resp));
  CHECK(resp.nRespostas == 3 && !resp.temFalta);
  CHECK(!farolescolha_rede_resposta(g1 + 99, &resp)); // geracao errada recusada

  // 2. Auth lenta: UI volta em < 100 ms e cancelamento interrompe auth com 0 POSTs!
  snprintf(url, sizeof url, "%s/canonico", base);
  resetHooks(url);
  farolescolha_rede_definir_auth(NULL, NULL, NULL); // usa recomenda_cabecalhos()
  __atomic_store_n(&gAuthAtrasoMs, 2500, __ATOMIC_RELEASE);
  {
    int antes = lerContador(countFile);
    t0 = monoMs();
    g1 = farolescolha_rede_pedir_perguntas("algo leve", pergs, 5, NULL);
    dt = monoMs() - t0;
    CHECK(g1 > 0 && dt < 100); // NUNCA bloqueou a UI na auth lenta!
    usleep(80000);
    farolescolha_rede_cancelar();
    farolescolha_rede_encerrar();
    CHECK(__atomic_load_n(&gAuthChamadas, __ATOMIC_ACQUIRE) >= 1);
    CHECK(__atomic_load_n(&gAuthCancelou, __ATOMIC_ACQUIRE) == 1);
    CHECK(lerContador(countFile) == antes); // cancelou durante auth -> 0 POSTs!
  }

  // 3. 429 -> FER_FALLBACK / FER_MOT_LIMITE.
  snprintf(url, sizeof url, "%s/429", base);
  resetHooks(url);
  g1 = farolescolha_rede_pedir_perguntas("algo leve", pergs, 5, NULL);
  CHECK(g1 > 0);
  esperarRedeTerminar(3000);
  CHECK(farolescolha_rede_estado(&info) == FER_FALLBACK &&
        info.motivo == FER_MOT_LIMITE && info.http == 429);

  // 4. 503 -> FER_FALLBACK / FER_MOT_ERRO.
  snprintf(url, sizeof url, "%s/503", base);
  resetHooks(url);
  g1 = farolescolha_rede_pedir_perguntas("algo leve", pergs, 5, NULL);
  CHECK(g1 > 0);
  esperarRedeTerminar(3000);
  CHECK(farolescolha_rede_estado(&info) == FER_FALLBACK &&
        info.motivo == FER_MOT_ERRO && info.http == 503);

  // 5. JSON invalido -> FER_FALLBACK / FER_MOT_ERRO.
  snprintf(url, sizeof url, "%s/json-torto", base);
  resetHooks(url);
  g1 = farolescolha_rede_pedir_perguntas("algo leve", pergs, 5, NULL);
  CHECK(g1 > 0);
  esperarRedeTerminar(3000);
  CHECK(farolescolha_rede_estado(&info) == FER_FALLBACK && info.motivo == FER_MOT_ERRO);

  // 6. origem:"regras" -> FER_FALLBACK / FER_MOT_REGRAS.
  snprintf(url, sizeof url, "%s/regras", base);
  resetHooks(url);
  g1 = farolescolha_rede_pedir_perguntas("algo leve", pergs, 5, NULL);
  CHECK(g1 > 0);
  esperarRedeTerminar(3000);
  CHECK(farolescolha_rede_estado(&info) == FER_FALLBACK && info.motivo == FER_MOT_REGRAS);

  // 7. pergunta/opcao invalida -> FER_FALLBACK / FER_MOT_ERRO.
  snprintf(url, sizeof url, "%s/opcao-invalida", base);
  resetHooks(url);
  g1 = farolescolha_rede_pedir_perguntas("algo leve", pergs, 5, NULL);
  CHECK(g1 > 0);
  esperarRedeTerminar(3000);
  CHECK(farolescolha_rede_estado(&info) == FER_FALLBACK && info.motivo == FER_MOT_ERRO);

  // 8. Geracao antiga descartada: pedido lento g1 seguido de pedido rapido g2.
  snprintf(url, sizeof url, "%s/lento-300ms", base);
  resetHooks(url);
  g1 = farolescolha_rede_pedir_perguntas("primeiro pedido lento", pergs, 5, NULL);
  CHECK(g1 > 0);
  usleep(20000);
  snprintf(url, sizeof url, "%s/falta", base);
  farolescolha_rede_definir_url(url);
  g2 = farolescolha_rede_pedir_perguntas("segundo pedido rapido", pergs, 5, NULL);
  CHECK(g2 > g1);
  esperarRedeTerminar(3000);
  CHECK(farolescolha_rede_estado(&info) == FER_PRONTO && info.geracao == g2);
  CHECK(farolescolha_rede_resposta(g2, &resp));
  CHECK(resp.nRespostas == 2 && resp.temFalta && resp.faltaPerg == FESC_HUMOR);

  // 9. Prazo rigido de 6 s (servidor trava 9 s -> corta em ~6 s com FER_MOT_PRAZO).
  snprintf(url, sizeof url, "%s/timeout", base);
  resetHooks(url);
  t0 = monoMs();
  g1 = farolescolha_rede_pedir_perguntas("algo que demora demais", pergs, 5, NULL);
  CHECK(g1 > 0);
  esperarRedeTerminar(8500);
  dt = monoMs() - t0;
  CHECK(dt >= 5500 && dt < 7800);
  CHECK(farolescolha_rede_estado(&info) == FER_FALLBACK && info.motivo == FER_MOT_PRAZO);

  farolescolha_rede_encerrar();
  puts("ok  resiliencia e TSan: auth lenta (<100ms + cancel), 429, 503, JSON torto, regras, opcao invalida, geracao, prazo 6s");
}

int main(int argc, char **argv) {
  const char *base = argc > 1 ? argv[1] : "";
  const char *countFile = argc > 2 ? argv[2] : "";
  CHECK(base[0] && countFile[0]);
  provaMisterio();
  provaNormalizacaoECorpo();
  provaValidacaoEstrita();
  provaPrivacidadeZeroPosts(base, countFile);
  provaRedeResilienciaETSan(base, countFile);
  CHECK(!gFalhas);
  puts("farol_escolha_rede (rede pura): ok");
  return 0;
}

#else
// ======================================================================
// MODO 2: App inteiro (menos main.c) — UI P5 + Resolver + Voz + PNG 1920x1080
// ======================================================================
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include "ajustes.h"
#include "catalogo.h"
#include "dados.h"
#include "farolacha.h"
#include "farolcompara.h"
#include "farolescolhaui.h"
#include "farolsinal.h"
#include "gfx.h"
#include "perfilinfantil.h"
#include "perfis.h"
#include "resolver.h"
#include "seguro.h"
#include "sistexto.h"
#include "tex_cache.h"
#include "text.h"

#ifdef NV_LINUX_DESKTOP
#include <GLES2/gl2.h>
#endif

static int gPlayer;
static int playerTeste(void) { return gPlayer; }

static void teclaR(SDL_Keycode k, int repeat) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  e.key.repeat = (Uint8)repeat;
  resolver_evento(&e);
  resolver_atualizar(0.016f, 1000);
}
static void tecla(SDL_Keycode k) { teclaR(k, 0); }

static void esperarUiEspera(int maxMs) {
  long long lim = monoMs() + (long long)maxMs;
  while (monoMs() < lim) {
    resolver_atualizar(0.016f, 1500);
    if (farolescolhaui_modo() != FESCUI_ESPERA) return;
    usleep(5000);
  }
}

static CatItem CAT[48];

static void montarCatCanonico(void) {
  int i;
  // 0..3: filmes leves ~60-85 min para ver com crianca (animacao/familia), IMDb.
  // 4..47: mix de outros generos/duracoes (suspense, terror, longos, series).
  static const char *const G_OUTROS[] = {
    "Filme · Suspense · Crime",
    "Filme · Terror",
    "Série · Drama · Mistério",
    "Filme · Ação · Guerra",
    "Série · Comédia",
    "Filme · Documentário",
  };
  for (i = 0; i < 48; i++) {
    CatItem *c = &CAT[i];
    memset(c, 0, sizeof *c);
    snprintf(c->imdb, sizeof c->imdb, "tt54%05d", i + 1);
    snprintf(c->titulo, sizeof c->titulo, "Obra %02d", i + 1);
    snprintf(c->classificacao, sizeof c->classificacao, "%s", i < 6 ? "L" : "14");
    if (i < 4) {
      snprintf(c->tipo, sizeof c->tipo, "movie");
      snprintf(c->genero, sizeof c->genero, "%s",
               (i % 2 == 0) ? "Filme · Animação · Família · Comédia"
                            : "Filme · Família · Aventura · Drama");
      snprintf(c->meta, sizeof c->meta, "%d · %d min", (i < 2) ? 2022 : 1995, 65 + i * 5);
      c->nota = 90 - i * 2;
    } else {
      int serie = (i % 3 == 0);
      snprintf(c->tipo, sizeof c->tipo, "%s", serie ? "series" : "movie");
      snprintf(c->genero, sizeof c->genero, "%s", G_OUTROS[i % 6]);
      snprintf(c->meta, sizeof c->meta, "%d · %d min", (i % 2) ? 2021 : 1994,
               serie ? 42 : 125 + (i % 4) * 10);
      c->nota = 60 + (i % 25);
    }
  }
  cat_definir(CAT, 48);
}

static void irParaEscolha(void) {
  int i;
  for (i = 0; i < 40 && resolver_foco() != RESOLVER_B_ACHAR_ESCOLHA; i++) {
    int f = resolver_foco();
    if ((f >= RESOLVER_B_ACHAR_CURTO && f <= RESOLVER_B_ACHAR_SURPRESA) ||
        f == RESOLVER_B_ACHAR_VOZ)
      tecla(SDLK_DOWN);
    else tecla(SDLK_RIGHT);
  }
  CHECK(resolver_foco() == RESOLVER_B_ACHAR_ESCOLHA);
}

static void abrirP5(void) {
  resolver_abrir();
  resolver_atualizar(0.016f, 1000);
  CHECK(resolver_aberto() && resolver_fase() == RESOLVER_FASE_INICIO);
  irParaEscolha();
  tecla(SDLK_RETURN);
  CHECK(resolver_aberto() && resolver_fase() == RESOLVER_FASE_ESCOLHA);
  CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA);
}

static void focarMic(void) {
  CHECK(farolescolhaui_voz_visivel() == 1);
  tecla(SDLK_DOWN);
  CHECK(farolescolhaui_foco() == FESCUI_FOCO_VOLTAR);
  tecla(SDLK_RIGHT);
  CHECK(farolescolhaui_foco() == FESCUI_FOCO_VOZ);
}

static void configurarAppAutorizado(const char *url) {
  farolescolha_rede_teste_zera();
  farolescolha_rede_definir_url(url);
  farolescolha_rede_definir_auth("Authorization: Bearer tok_segredo_ui54",
                                 "X-Nuvio-Auth: nuvio", NULL);
  farolescolhaui_teste_hora(20);
  farolescolhaui_teste_infantil(0);
  CHECK(perfil_infantil_definir(perfis_ativo(), 0));
  farolsinal_definir_recomendar(1);
  seguro_iniciar(0, 1000, NULL);
  CHECK(seguro_perfil_ativo() == 0);
  CHECK(farolconsent_definir(FAROL_CONSENT_SIM));
  CHECK(farolescolha_rede_autorizado() == 1);
}

static void provaUiFluxoCompleto(const char *base, const char *countFile) {
  char url[512];
  int antes;

  // 1. Sem voz (st_teste_ligar(0)): botao de microfone oculto.
  montarCatCanonico();
  snprintf(url, sizeof url, "%s/canonico", base);
  configurarAppAutorizado(url);
  st_teste_ligar(0);
  abrirP5();
  CHECK(farolescolhaui_voz_visivel() == 0);
  tecla(SDLK_DOWN);
  tecla(SDLK_RIGHT);
  CHECK(farolescolhaui_foco() == FESCUI_FOCO_VOLTAR); // nao foca mic invisivel
  resolver_fechar();
  resolver_atualizar(1.0f, 2000);

  // 2. Com voz: escuta -> parcial -> Voltar cancela (0 POSTs);
  //    escuta -> ST_FIM -> previa (0 POSTs) -> repeat ignorado -> Voltar descarta (0 POSTs).
  st_teste_ligar(1);
  abrirP5();
  focarMic();
  antes = lerContador(countFile);
  teclaR(SDLK_RETURN, 1); // repeat ignorado
  CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA);
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_OUVINDO);
  st_teste_evento("Palgo leve de uma");
  resolver_atualizar(0.016f, 1100);
  CHECK(!strcmp(farolescolhaui_parcial(), "algo leve de uma"));
  teclaR(SDLK_ESCAPE, 1); // repeat ignorado em Voltar no estado ouvindo
  CHECK(farolescolhaui_modo() == FESCUI_OUVINDO);
  tecla(SDLK_ESCAPE);     // cancela escuta -> volta intacto as perguntas
  CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA);
  CHECK(st_estado() == ST_PARADO);
  CHECK(lerContador(countFile) == antes);

  // Ditado ate ST_FIM -> previa obrigatoria com 0 POSTs antes do OK!
  focarMic();
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_OUVINDO);
  st_teste_evento("Valgo leve de uma hora para ver com meu filho");
  resolver_atualizar(0.016f, 1200);
  CHECK(farolescolhaui_modo() == FESCUI_PREVIA);
  CHECK(!strcmp(farolescolhaui_frase(), "algo leve de uma hora para ver com meu filho"));
  CHECK(farolescolhaui_foco() == FESCUI_FOCO_CONFIRMAR);
  CHECK(lerContador(countFile) == antes); // 0 POSTs antes do OK!

  // Repeat ignorado em OK e Voltar na previa!
  teclaR(SDLK_RETURN, 1);
  CHECK(farolescolhaui_modo() == FESCUI_PREVIA);
  CHECK(lerContador(countFile) == antes);
  teclaR(SDLK_ESCAPE, 1);
  CHECK(farolescolhaui_modo() == FESCUI_PREVIA);
  CHECK(lerContador(countFile) == antes);

  // Voltar descarta com 0 POSTs e volta intacto as perguntas.
  tecla(SDLK_ESCAPE);
  CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA);
  CHECK(farolescolha_feitas(farolescolhaui_motor()) == 0);
  CHECK(farolescolhaui_motor()->nPre == 0);
  CHECK(lerContador(countFile) == antes);

  // 3. Caso canonico confirmado com OK -> 3 pre-respostas (tempo=t100, peso=leve,
  //    companhia=crianca) -> 0 perguntas restantes -> abre P4 Comparar e fecha Farol!
  farolcompara_teste_reset();
  CHECK(farolescolhaui_aplicar_frase("algo leve de uma hora para ver com meu filho") == 1);
  CHECK(farolescolhaui_modo() == FESCUI_PREVIA);
  CHECK(lerContador(countFile) == antes); // aplicar_frase tambem exige previa (0 POSTs)
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_ESPERA || farolescolhaui_modo() == FESCUI_COMPARAR);
  esperarUiEspera(3000);
  CHECK(lerContador(countFile) == antes + 1);
  CHECK(farolescolha_resposta(farolescolhaui_motor(), FESC_TEMPO) == FESC_T100);
  CHECK(farolescolha_resposta(farolescolhaui_motor(), FESC_PESO) == FESC_LEVE);
  CHECK(farolescolha_resposta(farolescolhaui_motor(), FESC_COMPANHIA) == FESC_CRIANCA);
  CHECK(farolescolhaui_modo() == FESCUI_COMPARAR);
  CHECK(!resolver_aberto());
  CHECK(farolcompara_aberto());
  CHECK(farolcompara_n_colunas() >= 2 && farolcompara_n_colunas() <= 3);
  farolcompara_fechar();
  farolcompara_teste_reset();
  resolver_atualizar(1.0f, 2000);

  // 4. 2 pre-respostas + falta priorizada ("humor") -> 1 pergunta restante (de <= 3)!
  snprintf(url, sizeof url, "%s/falta-epoca", base);
  configurarAppAutorizado(url);
  abrirP5();
  CHECK(farolescolhaui_aplicar_frase("filme leve para ver com meu filho") == 1);
  tecla(SDLK_RETURN);
  esperarUiEspera(3000);
  CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA);
  CHECK(farolescolha_pergunta(farolescolhaui_motor()) == FESC_EPOCA); // falta priorizada!
  CHECK(farolescolha_de(farolescolhaui_motor()) == 1);                // 2 pre + 1 = <= 3
  CHECK(farolescolhaui_voz_visivel() == 0);                           // ja houve pre-resposta
  resolver_fechar();
  resolver_atualizar(1.0f, 2000);

  // 5. Guarda todosImdb do #50 mantida: vencedor so-TMDB fica nos cartazes, NAO vai ao P4!
  snprintf(url, sizeof url, "%s/canonico", base);
  configurarAppAutorizado(url);
  CAT[0].imdb[0] = '\0';
  CAT[0].tmdb = 540001;
  cat_definir(CAT, 48);
  abrirP5();
  CHECK(farolescolhaui_aplicar_frase("algo leve de uma hora para ver com meu filho") == 1);
  tecla(SDLK_RETURN);
  esperarUiEspera(3000);
  CHECK(farolescolhaui_modo() == FESCUI_RESULTADO);
  CHECK(!farolcompara_aberto());
  CHECK(farolescolhaui_n_resultados() >= 2);
  resolver_fechar();
  resolver_atualizar(1.0f, 2000);

  // 6. 1 vencedor -> cartaz anti-TOCTOU; 0 vencedores -> "Nada exato…".
  montarCatCanonico();
  // So 1 filme leve/crianca <=100 min -> 1 resultado.
  snprintf(CAT[1].genero, sizeof CAT[1].genero, "Filme · Terror");
  snprintf(CAT[2].genero, sizeof CAT[2].genero, "Filme · Terror");
  snprintf(CAT[3].genero, sizeof CAT[3].genero, "Filme · Terror");
  cat_definir(CAT, 48);
  abrirP5();
  CHECK(farolescolhaui_aplicar_frase("algo leve de uma hora para ver com meu filho") == 1);
  tecla(SDLK_RETURN);
  esperarUiEspera(3000);
  CHECK(farolescolhaui_modo() == FESCUI_RESULTADO && farolescolhaui_n_resultados() == 1);
  CHECK(farolescolhaui_saida() == FESC_SAIDA_ACHADOS);
  resolver_fechar();
  resolver_atualizar(1.0f, 2000);

  // 0 vencedores -> "Nada exato…".
  snprintf(CAT[0].genero, sizeof CAT[0].genero, "Filme · Terror");
  cat_definir(CAT, 48);
  abrirP5();
  CHECK(farolescolhaui_aplicar_frase("algo leve de uma hora para ver com meu filho") == 1);
  tecla(SDLK_RETURN);
  esperarUiEspera(3000);
  CHECK(farolescolhaui_modo() == FESCUI_RESULTADO && farolescolhaui_n_resultados() == 0);
  CHECK(farolescolhaui_saida() == FESC_SAIDA_NADA);
  CHECK(farolescolhaui_foco() == FESCUI_FOCO_VOLTAR);
  resolver_fechar();
  resolver_atualizar(1.0f, 2000);

  // 7. Falhas (429, 503, origem:"regras") e cancelamento em ESPERA -> fallback limpo
  //    para o banco normal de perguntas sem travar nem perder foco.
  montarCatCanonico();
  {
    static const char *const ROTAS[] = { "/429", "/503", "/regras", "/json-torto" };
    int r;
    for (r = 0; r < 4; r++) {
      snprintf(url, sizeof url, "%s%s", base, ROTAS[r]);
      configurarAppAutorizado(url);
      abrirP5();
      CHECK(farolescolhaui_aplicar_frase("algo leve") == 1);
      tecla(SDLK_RETURN);
      esperarUiEspera(3000);
      CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA);
      CHECK(farolescolhaui_foco() == 0);
      CHECK(farolescolha_feitas(farolescolhaui_motor()) == 0);
      resolver_fechar();
      resolver_atualizar(1.0f, 2000);
    }
  }
  // Cancelar durante ESPERA (ignorando repeat=1).
  snprintf(url, sizeof url, "%s/timeout", base);
  configurarAppAutorizado(url);
  abrirP5();
  CHECK(farolescolhaui_aplicar_frase("algo demorado") == 1);
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_ESPERA);
  teclaR(SDLK_ESCAPE, 1); // repeat ignorado
  CHECK(farolescolhaui_modo() == FESCUI_ESPERA);
  tecla(SDLK_ESCAPE);
  CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA && farolescolhaui_foco() == 0);
  farolescolha_rede_encerrar();
  resolver_fechar();
  resolver_atualizar(1.0f, 2000);

  // 8. Guardas de privacidade/consentimento na UI -> 0 POSTs e volta as perguntas!
  snprintf(url, sizeof url, "%s/canonico", base);
  configurarAppAutorizado(url);
  antes = lerContador(countFile);
  // 8a. Sem consentimento (SO_TV).
  CHECK(farolconsent_definir(FAROL_CONSENT_SO_TV));
  abrirP5();
  CHECK(farolescolhaui_aplicar_frase("algo leve") == 1);
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA);
  CHECK(lerContador(countFile) == antes);
  resolver_fechar();
  // 8b. recomendar = 0.
  configurarAppAutorizado(url);
  farolsinal_definir_recomendar(0);
  abrirP5();
  CHECK(farolescolhaui_aplicar_frase("algo leve") == 1);
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA);
  CHECK(lerContador(countFile) == antes);
  resolver_fechar();
  // 8c. Privado (seguro_perfil_ativo = 1 via 2 quedas rapidas).
  configurarAppAutorizado(url);
  seguro_iniciar(1, 1100, NULL);
  seguro_iniciar(1, 1200, NULL);
  CHECK(seguro_perfil_ativo() == 1);
  abrirP5();
  CHECK(farolescolhaui_aplicar_frase("algo leve") == 1);
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA);
  CHECK(lerContador(countFile) == antes);
  resolver_fechar();
  seguro_iniciar(0, 1300, NULL);
  CHECK(seguro_perfil_ativo() == 0);
  // 8d. Infantil.
  configurarAppAutorizado(url);
  farolescolhaui_teste_infantil(1);
  abrirP5();
  CHECK(farolescolhaui_aplicar_frase("algo leve") == 1);
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA);
  CHECK(lerContador(countFile) == antes);
  resolver_fechar();
  // 8e. URL na frase -> aplicar_frase recusa com 0 POSTs.
  configurarAppAutorizado(url);
  abrirP5();
  CHECK(farolescolhaui_aplicar_frase("veja https://exemplo.com") == 0);
  CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA);
  CHECK(lerContador(countFile) == antes);
  resolver_fechar();
  resolver_atualizar(1.0f, 2000);

  puts("ok  UI P5 completa: mic, ouvindo+parcial, previa 0 POSTs, repeat, canonico, falta, todosImdb, 1/0, falhas e privacidade");
}

#ifdef NV_LINUX_DESKTOP
static void salvarPng(const char *path, const unsigned char *rgba, int w, int h) {
  int y, pitch = w * 4;
  unsigned char *flip = malloc((size_t)pitch * (size_t)h);
  SDL_Surface *s;
  CHECK(flip != NULL);
  for (y = 0; y < h; y++)
    memcpy(flip + (size_t)y * (size_t)pitch, rgba + (size_t)(h - 1 - y) * (size_t)pitch,
           (size_t)pitch);
  s = SDL_CreateRGBSurfaceFrom(flip, w, h, 32, pitch, 0x000000ffu, 0x0000ff00u,
                               0x00ff0000u, 0xff000000u);
  CHECK(s != NULL);
  CHECK(IMG_SavePNG(s, path) == 0);
  SDL_FreeSurface(s);
  free(flip);
}

static void capturar(const char *dir, const char *nome, unsigned char *px, int W, int H) {
  char path[1024];
  SDL_Surface *lida;
  int i, claros = 0;
  size_t b;
  for (i = 0; i < 25; i++) {
    resolver_atualizar(0.016f, 2000u + (Uint32)i * 16u);
    txt_novo_quadro();
    gfx_novo_quadro();
    glViewport(0, 0, W, H);
    glClearColor(0.04f, 0.05f, 0.07f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    resolver_desenhar(2000u + (Uint32)i * 16u);
    glFinish();
  }
  glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px);
  for (b = 0; b < (size_t)W * (size_t)H * 4u; b += 4)
    if (px[b] > 200 && px[b + 1] > 200 && px[b + 2] > 200) claros++;
  CHECK(claros > 2000);
  snprintf(path, sizeof path, "%s/%s.png", dir, nome);
  salvarPng(path, px, W, H);
  lida = IMG_Load(path);
  CHECK(lida && lida->w == W && lida->h == H);
  SDL_FreeSurface(lida);
  printf("ok  captura %s: %dx%d\n", nome, W, H);
}

static void provaGl(const char *dir, const char *base) {
  const int W = 1920, H = 1080;
  char url[512];
  SDL_Window *win;
  SDL_GLContext gl;
  unsigned char *px = malloc((size_t)W * (size_t)H * 4u);
  CHECK(px != NULL);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  win = SDL_CreateWindow("farol-54", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, W, H,
                         SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  CHECK(win != NULL);
  gl = SDL_GL_CreateContext(win);
  CHECK(gl != NULL);
  glViewport(0, 0, W, H);
  gfx_tamanho_alvo(W, H);
  CHECK(gfx_iniciar());
  { char ic[4096]; if (realpath("deploy/app/art", ic)) gfx_icones_dir(ic); }
  CHECK(txt_iniciar("deploy/app", 1));
  tex_iniciar(16);

  montarCatCanonico();
  snprintf(url, sizeof url, "%s/timeout", base);
  configurarAppAutorizado(url);
  st_teste_ligar(1);
  abrirP5();
  focarMic();
  capturar(dir, "esc54-mic", px, W, H);

  tecla(SDLK_RETURN);
  st_teste_evento("Palgo leve de uma hora");
  capturar(dir, "esc54-ouvindo", px, W, H);

  st_teste_evento("Valgo leve de uma hora para ver com meu filho");
  resolver_atualizar(0.016f, 2600);
  CHECK(farolescolhaui_modo() == FESCUI_PREVIA);
  capturar(dir, "esc54-previa", px, W, H);

  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_ESPERA);
  capturar(dir, "esc54-espera", px, W, H);

  tecla(SDLK_ESCAPE);
  farolescolha_rede_encerrar();
  resolver_fechar();
  resolver_atualizar(1.0f, 4000);

  free(px);
  tex_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(win);
}
#endif

int main(int argc, char **argv) {
  const char *base = argc > 1 ? argv[1] : "";
  const char *countFile = argc > 2 ? argv[2] : "";
  const char *shotDir = argc > 3 ? argv[3] : NULL;
  CHECK(base[0] && countFile[0]);
  provaMisterio();
  CHECK(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG);
  ajustes_iniciar();
  dados_iniciar("deploy/app/art");
  farolcompara_teste_player(playerTeste);
  provaUiFluxoCompleto(base, countFile);
#ifdef NV_LINUX_DESKTOP
  if (shotDir) provaGl(shotDir, base);
#endif
  (void)shotDir;
  farolescolha_rede_encerrar();
  CHECK(!gFalhas);
  CHECK(farolcompara_teste_esperar(3000));
  SDL_Quit();
  puts("farol_escolha_rede (UI): ok");
  return 0;
}
#endif
