// DESMARCAR UM EPISODIO TEM DE GANHAR DE QUALQUER FONTE (Silo, tt14688458).
//
// O relato do dono na TCL: desmarca T2E7..E10 (quer a T2 vista so ate o E6) e
// os episodios VOLTAM marcados. O mapa de vistos (vistoep.c) e reescrito por
// tres leitores, e nenhum deles sabia do gesto depois de um tempo:
//   - o Trakt (/shows/<id>/progress/watched): escreve `completed` cru. Se o
//     Trakt ainda diz "visto" (remove recusado, sem rede, corrida com o POST do
//     gesto, outro servico re-enviando), a marca volta ao abrir a pagina;
//   - a conta Nuvio (sync_pull_watched_items): respeitava o jornal
//     (contapend.c), mas o jornal e PODADO assim que a conta confirma o delete
//     — uma linha que ainda venha no pull seguinte re-marcava;
//   - nada disso sobrevivia a fechar o app.
//
// FONTES FALSAS, sem rede: o corpo do Trakt e montado aqui com a forma real
// (seasons[].episodes[].completed/last_watched_at), a conta e o mesmo servidor
// de mentira de tests/contapend.c, e o "disco" e um mapa em memoria.
//
// ESTE ARQUIVO COMPILA NO COMMIT PAI de proposito (so API que ja existia) e
// la ele FALHA nas assercoes — e a prova de que o defeito existe. Com
// src/vistonao.c presente, o .sh liga TEM_VISTONAO e o juiz entra.
#include "vistoep.h"
#include "contapend.h"
#include "contalib.h"
#include "catalogo.h"
#include "js.h"
#ifdef TEM_VISTONAO
#include "vistonao.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SILO "tt14688458"

static int falhas;
static void confere(const char *o_que, int obtido, int esperado) {
  int ok = obtido == esperado;
  printf("  %-62s %s (obtido %d, esperado %d)\n", o_que, ok ? "ok    " : "FALHOU",
         obtido, esperado);
  if (!ok) falhas++;
}

// ---------------------------------------------------------------- dubles
static int perfil = 1, logada = 0;
static long long agora = 1759000000000LL;   // 27/09/2025 19:06:40 UTC
static long long relogio(void) { return agora; }
int perfis_ativo(void) { return perfil; }
int sessao_logada(void) { return logada; }
const char *sessao_usuario(void) { return logada ? "u-123" : ""; }
const char *dados_cliente_id(void) { return "cliente-teste"; }

// "Disco": poucos arquivos por nome (o jornal da conta e o das desmarcacoes).
#define DISCO_MAX 6
static struct { char nome[128]; char *txt; } disco[DISCO_MAX];
static int discoAcha(const char *n) {
  int i;
  for (i = 0; i < DISCO_MAX; i++) if (disco[i].txt && !strcmp(disco[i].nome, n)) return i;
  return -1;
}
char *dados_ler(const char *n) {
  int i = discoAcha(n);
  return i < 0 ? NULL : strdup(disco[i].txt);
}
int dados_gravar(const char *n, const char *c) {
  int i = discoAcha(n);
  if (i < 0) for (i = 0; i < DISCO_MAX && disco[i].txt; i++) {}
  if (i >= DISCO_MAX) return 0;
  snprintf(disco[i].nome, sizeof disco[i].nome, "%s", n);
  free(disco[i].txt);
  disco[i].txt = strdup(c);
  return 1;
}
int dados_apagar(const char *n) {
  int i = discoAcha(n);
  if (i >= 0) { free(disco[i].txt); disco[i].txt = NULL; }
  return 1;
}

char *contacache_ler(const char *s, int p, const char *u, long *q) {
  (void)s; (void)p; (void)u; (void)q; return NULL;
}
void cat_historico_definir_id(const char *i, const char *t, int v) { (void)i; (void)t; (void)v; }
// O catalogo, que contalib.c pede para a BIBLIOTECA (nao usada aqui).
const char *i18n(const char *s) { return s; }
int cat_n(void) { return 0; }
const CatItem *cat_item(int i) { (void)i; return NULL; }
int cat_indice_por_imdb(const char *imdb) { (void)imdb; return -1; }
void cat_definir_na_lista(int i, int n) { (void)i; (void)n; }
int cat_acrescentar_lote(const CatItem *v, int q, int *s) { (void)v; (void)q; (void)s; return 0; }

// A CONTA: aceita tudo com 200 e conta os deletes. O que ela DEVOLVE no pull e
// o teste que decide (corpo passado a contalib_ler_vistos) — e assim que se
// monta "a conta confirmou o delete e a linha ainda veio".
static int nDelVis;
char *sessao_rpc(const char *fn, const char *corpo, int *st) {
  (void)corpo;
  *st = 200;
  if (!strcmp(fn, "sync_delete_watched_items")) nDelVis++;
  return strdup("[]");
}

// ---------------------------------------------------------------- fontes

// ISO-8601 de um instante em ms, como o Trakt manda em last_watched_at.
static const char *iso(long long ms, char *b, size_t tam) {
  time_t s = (time_t)(ms / 1000);
  struct tm tm;
  gmtime_r(&s, &tm);
  strftime(b, tam, "%Y-%m-%dT%H:%M:%S.000Z", &tm);
  return b;
}

// /shows/tt14688458/progress/watched com a forma real: 3 temporadas de 10, a
// T1 e a T2 inteiras vistas e a T3 ate o E8 — os "30 no mapa (28 vistos)" do
// log da TCL. `vistoEm` e o last_watched_at de todo episodio visto;
// (tNovo, eNovo, novoEm) troca o de UM episodio (0 = nenhum); semData tira o
// campo (fonte que nao diz quando).
static char prog[16384];
static const char *progresso(long long vistoEm, int tNovo, int eNovo, long long novoEm,
                             int semData) {
  size_t u = 0;
  int t, e;
  char b[40];
  u += (size_t)snprintf(prog + u, sizeof prog - u, "{\"aired\":30,\"completed\":28,\"seasons\":[");
  for (t = 1; t <= 3; t++) {
    u += (size_t)snprintf(prog + u, sizeof prog - u,
                          "%s{\"number\":%d,\"aired\":10,\"completed\":%d,\"episodes\":[",
                          t > 1 ? "," : "", t, t < 3 ? 10 : 8);
    for (e = 1; e <= 10; e++) {
      int visto = t < 3 || e <= 8;
      long long em = (t == tNovo && e == eNovo) ? novoEm : vistoEm;
      u += (size_t)snprintf(prog + u, sizeof prog - u, "%s{\"number\":%d,\"completed\":%s",
                            e > 1 ? "," : "", e, visto ? "true" : "false");
      if (visto && !semData)
        u += (size_t)snprintf(prog + u, sizeof prog - u, ",\"last_watched_at\":\"%s\"",
                              iso(em, b, sizeof b));
      else
        u += (size_t)snprintf(prog + u, sizeof prog - u, ",\"last_watched_at\":null");
      u += (size_t)snprintf(prog + u, sizeof prog - u, "}");
    }
    u += (size_t)snprintf(prog + u, sizeof prog - u, "]}");
  }
  snprintf(prog + u, sizeof prog - u, "],\"next_episode\":{\"season\":3,\"number\":9}}");
  return prog;
}

// O GESTO DA PESSOA, como episodios.c faz: vistoep_aplicar (local, na hora) e
// depois os destinos. Aqui so o da conta importa (o jornal); Trakt e Simkl sao
// POSTs que nao voltam ao mapa.
static int gesto(const VistoPar *p, int n, int visto, int comConta) {
  VistoPar envio[32];
  int ja = 0, k = vistoep_aplicar(SILO, p, n, visto, envio, &ja);
  if (comConta && k) contapend_episodios(SILO, "series", envio, k, visto);
  return k;
}

static const VistoPar T2E7_10[4] = { {2, 7}, {2, 8}, {2, 9}, {2, 10} };

static int t2Vistos(void) {
  int e, k = 0;
  for (e = 1; e <= 10; e++) k += vistoep_estado(SILO, 2, e) == 1;
  return k;
}

// "Fechou e abriu o app": a memoria vai embora, o disco fica.
static void reabrir(void) {
  vistoep_esquecer();
  contapend_esquecer();
#ifdef TEM_VISTONAO
  vistonao_esquecer();
#endif
}

// Reprodutor de hipoteses para o relato de desmarcacao NO SITE. Nao afirma
// qual delas ocorreu na TV: falta o episodio e a resposta remota daquele gesto.
static int site(void) {
  const char *antes =
    "[{\"content_id\":\"tt14688458\",\"season\":2,\"episode\":6},"
    "{\"content_id\":\"tt14688458\",\"season\":2,\"episode\":5}]";
  const char *depois = "[{\"content_id\":\"tt14688458\",\"season\":2,\"episode\":5}]";
  const char *traktNao =
    "{\"seasons\":[{\"number\":2,\"episodes\":[{\"number\":6,\"completed\":false}]}]}";
  VistoPar par = {2, 6};
  logada = 1;
  contapend_relogio(relogio);

  puts("SITE: conta sem Trakt, snapshot perdeu T2E6 (nenhum gesto local)");
  contalib_ler_vistos(antes);
  contalib_aplicar_vistos();
  contalib_ler_vistos(depois);
  contalib_aplicar_vistos();
  confere("ausente do snapshot deixa de contar como visto",
          vistoep_estado(SILO, 2, 6) == 1, 0);

  puts("SITE: Trakt explicita false, sem jornal: remove do mapa");
  vistoep_ler_progresso(SILO, traktNao);
  confere("Trakt false remove o visto", vistoep_estado(SILO, 2, 6), 0);

  puts("SITE: gesto local ainda nao enviado deve sobreviver");
  vistoep_definir(SILO, 2, 6, 1);
  contapend_episodios(SILO, "series", &par, 1, 1);
  vistoep_ler_progresso(SILO, traktNao);
  contapend_aplicar_local();
  confere("jornal preserva visto local pendente", vistoep_estado(SILO, 2, 6), 1);
  confere("pendente continua na fila", contapend_pendentes(), 1);

  puts("SITE: gesto confirmado na conta, depois Trakt false, antes da poda");
  contapend_enviar();
  confere("nao ha envio pendente", contapend_pendentes(), 0);
  vistoep_ler_progresso(SILO, traktNao);
  contapend_aplicar_local();
  confere("confirmado antigo nao devolve visto retirado no Trakt",
          vistoep_estado(SILO, 2, 6), 0);
  printf("SITE: %d falha(s) de reconciliacao reproduzida(s); nao e captura da TV\n", falhas);
  return falhas ? 1 : 0;
}

int main(int argc, char **argv) {
  const long long DIA = 86400000LL;
  long long antes = agora - 7 * DIA;      // quando o Trakt diz que foi visto
  long long gestoMs;

  if (argc == 2 && !strcmp(argv[1], "--site")) return site();

  contapend_relogio(relogio);
  contapend_sem_fio(1);
  contalib_filtros(contapend_lista_oculta, contapend_visto_oculto);
#ifdef TEM_VISTONAO
  vistonao_relogio(relogio);
  vistoep_lapides(vistonao_barra, vistonao_gesto);   // o que app.c liga
#endif

  // ------------------------------------------------------------ Trakt
  printf("\nTrakt ainda diz 'visto' depois de a pessoa desmarcar (sem conta Nuvio):\n");
  confere("o progresso do Trakt entra inteiro (30 episodios)",
          vistoep_ler_progresso(SILO, progresso(antes, 0, 0, 0, 0)), 30);
  confere("28 vistos, como no log da TCL", vistoep_contar(SILO), 28);
  confere("desmarcar T2E7..E10 muda 4", gesto(T2E7_10, 4, 0, 0), 4);
  gestoMs = agora;
  confere("local na hora: T2 vista ate o E6", t2Vistos(), 6);
  agora += 60000;   // a pessoa reabre a pagina um minuto depois
  vistoep_ler_progresso(SILO, progresso(antes, 0, 0, 0, 0));
  confere("o MESMO corpo do Trakt nao re-marca: T2 continua em 6", t2Vistos(), 6);
  confere("e a serie fica com 24 vistos, nao 28", vistoep_contar(SILO), 24);
  confere("o que nao foi desmarcado segue visto (T2E6)", vistoep_estado(SILO, 2, 6), 1);
  confere("desmarcado e 0 (sabe-se que NAO), e nao -1", vistoep_estado(SILO, 2, 7), 0);

  printf("\nfonte SEM data (last_watched_at nulo): a desmarcacao ganha:\n");
  vistoep_ler_progresso(SILO, progresso(antes, 0, 0, 0, 1));
  confere("T2 continua em 6", t2Vistos(), 6);

  printf("\nrelogio do servidor um pouco a frente do da TV nao e 'visto mais novo':\n");
  vistoep_ler_progresso(SILO, progresso(antes, 2, 9, gestoMs + 30000, 0));
  confere("visto 30 s 'depois' do gesto continua barrado (T2E9)", vistoep_estado(SILO, 2, 9), 0);

  printf("\ntocar um episodio POSTERIOR (T3E9) nao marca os anteriores:\n");
  vistoep_definir(SILO, 3, 9, 1);   // o que player.c faz ao concluir: so o episodio
  confere("T3E9 marcado", vistoep_estado(SILO, 3, 9), 1);
  confere("T2 continua em 6", t2Vistos(), 6);
  vistoep_ler_progresso(SILO, progresso(antes, 0, 0, 0, 0));
  confere("e o Trakt, relido depois disso, ainda nao re-marca", t2Vistos(), 6);

  printf("\nfechar e abrir o app: a desmarcacao esta em disco:\n");
  reabrir();
  vistoep_ler_progresso(SILO, progresso(antes, 0, 0, 0, 0));
  confere("T2 continua em 6 depois de reabrir", t2Vistos(), 6);

  printf("\ne por PERFIL: o outro perfil ve o que o Trakt dele diz:\n");
  perfil = 2;
  vistoep_esquecer();
  vistoep_ler_progresso(SILO, progresso(antes, 0, 0, 0, 0));
  confere("perfil 2: T2 inteira vista", t2Vistos(), 10);
  perfil = 1;
  vistoep_esquecer();
  vistoep_ler_progresso(SILO, progresso(antes, 0, 0, 0, 0));
  confere("de volta ao perfil 1: T2 em 6", t2Vistos(), 6);

  printf("\nvisto de novo em OUTRO aparelho, DEPOIS do gesto: o remoto mais novo ganha:\n");
  vistoep_ler_progresso(SILO, progresso(antes, 2, 7, gestoMs + DIA, 0));
  confere("T2E7 (visto um dia depois) volta a marcado", vistoep_estado(SILO, 2, 7), 1);
  confere("T2E8..E10 continuam desmarcados", t2Vistos(), 7);
  vistoep_ler_progresso(SILO, progresso(antes, 0, 0, 0, 0));
  confere("e T2E7 nao e barrado de novo por uma leitura mais velha", vistoep_estado(SILO, 2, 7), 1);

  printf("\nmarcar de novo na TV desfaz a desmarcacao:\n");
  { VistoPar um = { 2, 8 };
    confere("marcar T2E8 muda 1", gesto(&um, 1, 1, 0), 1); }
  vistoep_definir(SILO, 2, 8, 0);   // um leitor qualquer zera sem ser gesto
  vistoep_ler_progresso(SILO, progresso(antes, 0, 0, 0, 0));
  confere("T2E8 volta com o Trakt (nao ha mais desmarcacao)", vistoep_estado(SILO, 2, 8), 1);
  confere("T2E9 e E10 seguem desmarcados", t2Vistos(), 8);

  // ------------------------------------------------------------ conta
  printf("\nconta Nuvio: a linha volta DEPOIS de o delete ser confirmado e podado:\n");
  reabrir();
  logada = 1;
  { static const char *CONTA_ANTES =
      "[{\"content_id\":\"tt14688458\",\"content_type\":\"series\",\"season\":2,\"episode\":5,\"watched_at\":1758000000000},"
      "{\"content_id\":\"tt14688458\",\"content_type\":\"series\",\"season\":2,\"episode\":6,\"watched_at\":1758000000000}]";
    static char contaNova[400];
    static const VistoPar T2E5_6[2] = { {2, 5}, {2, 6} };
    static const VistoPar T2E6[1] = { {2, 6} };
    confere("a conta entrega 2 linhas", contalib_ler_vistos(CONTA_ANTES), 2);
    contalib_aplicar_vistos();
    confere("T2E5 e T2E6 vistos pela conta",
            (vistoep_estado(SILO, 2, 5) == 1) + (vistoep_estado(SILO, 2, 6) == 1), 2);
    (void)T2E5_6;
    confere("desmarcar T2E6 muda 1", gesto(T2E6, 1, 0, 1), 1);
    confere("o delete sobe e a conta confirma", contapend_enviar(), 1);
    confere("um sync_delete_watched_items saiu", nDelVis, 1);
    // Ainda no jornal: o pull e barrado por ele (ja era assim).
    contalib_ler_vistos(CONTA_ANTES);
    contalib_aplicar_vistos();
    confere("com a entrada no jornal, a linha nao re-marca", vistoep_estado(SILO, 2, 6), 0);
    // O ciclo seguinte poda o que foi confirmado antes dele (sync.c).
    agora += 10 * 60000;
    contapend_podar(agora);
    confere("o jornal ja nao segura a linha (entrada podada)",
            contapend_visto_oculto(SILO, 2, 6, 1758000000000LL), 0);
    // ... e a linha AINDA vem (copia guardada, pagina velha, delete que o
    // servidor aceitou e nao aplicou).
    contalib_ler_vistos(CONTA_ANTES);
    contalib_aplicar_vistos();
    confere("a linha velha da conta NAO re-marca T2E6", vistoep_estado(SILO, 2, 6), 0);
    confere("T2E5, que ninguem desmarcou, segue visto", vistoep_estado(SILO, 2, 5), 1);
    // Reaplicar o jornal (sync.c faz a cada ciclo) tambem nao mexe.
    contapend_aplicar_local();
    confere("e o jornal reaplicado nao muda isso", vistoep_estado(SILO, 2, 6), 0);
    // Visto de novo no celular DEPOIS do gesto: watched_at mais novo ganha.
    snprintf(contaNova, sizeof contaNova,
             "[{\"content_id\":\"tt14688458\",\"content_type\":\"series\",\"season\":2,\"episode\":6,\"watched_at\":%lld}]",
             agora + DIA);
    contalib_ler_vistos(contaNova);
    contalib_aplicar_vistos();
    confere("linha da conta MAIS NOVA que o gesto volta a marcar", vistoep_estado(SILO, 2, 6), 1);
  }

#ifdef TEM_VISTONAO
  // ------------------------------------------------------------ titulo
  printf("\ngesto na SERIE inteira (visto_titulo -> vistoep_titulo_gesto):\n");
  { static const VistoPar T1E2[1] = { {1, 2} };
    vistoep_esquecer();
    vistoep_fonte("tt7000002", 1, 1, 1, antes, NULL);
    vistoep_fonte("tt7000002", 1, 2, 1, antes, NULL);
    vistoep_fonte("tt7000002", 1, 3, 0, 0, NULL);
    vistoep_titulo_gesto("tt7000002", 0);           // desmarcar a serie
    confere("cada episodio visto ganha desmarcacao (T1E1)",
            vistonao_barra("tt7000002", 1, 1, antes), 1);
    confere("(T1E2)", vistonao_barra("tt7000002", 1, 2, antes), 1);
    confere("o que nao estava visto nao ganha (T1E3)",
            vistonao_barra("tt7000002", 1, 3, antes), 0);
    vistoep_fonte("tt7000002", 1, 1, 1, antes, NULL);
    confere("e a fonte que nao aplicou o remove nao traz de volta",
            vistoep_estado("tt7000002", 1, 1), 0);
    vistonao_gesto("tt7000002", T1E2, 1, 0);
    vistoep_titulo_gesto("tt7000002", 1);           // marcar a serie
    confere("marcar a serie solta todas (T1E1)", vistonao_barra("tt7000002", 1, 1, 0), 0);
    confere("(T1E2)", vistonao_barra("tt7000002", 1, 2, 0), 0);
  }

  // ------------------------------------------------------------ relogio
  printf("\nTV ligada sem relogio (1970): a desmarcacao nao perde para tudo:\n");
  { static const VistoPar T1E1[1] = { {1, 1} };
    long long certo = agora;
    agora = 86400000LL;                 // 02/01/1970
    vistonao_gesto("tt7000001", T1E1, 1, 0);
    confere("visto 'mais novo' que 1970 nao derruba o gesto",
            vistonao_barra("tt7000001", 1, 1, certo - DIA), 1);
    agora = certo;                      // a rede voltou, o relogio acertou
    confere("com o relogio certo, visto de ontem continua barrado",
            vistonao_barra("tt7000001", 1, 1, certo - DIA), 1);
    confere("e um visto de amanha (depois do gesto) ganha",
            vistonao_barra("tt7000001", 1, 1, certo + DIA), -1);
    confere("ganhou: a desmarcacao caiu", vistonao_barra("tt7000001", 1, 1, 0), 0);
  }

  // ------------------------------------------------------------ teto
  printf("\no arquivo de desmarcacoes tem teto e nao cresce sem limite:\n");
  { VistoPar cem[100];
    int lote, e, lotes = VISTONAO_MAX / 100 + 3;
    for (e = 0; e < 100; e++) { cem[e].temporada = 1; cem[e].episodio = (short)(e + 1); }
    for (lote = 0; lote < lotes; lote++) {   // 3 lotes a mais que o teto
      char id[16];
      snprintf(id, sizeof id, "tt%07d", 9000000 + lote);
      agora += 1000;
      vistonao_gesto(id, cem, 100, 0);
    }
    confere("nunca passa do teto", vistonao_n() <= VISTONAO_MAX, 1);
    { char id[16];
      snprintf(id, sizeof id, "tt%07d", 9000000 + lotes - 1);
      confere("a mais NOVA ficou", vistonao_barra(id, 1, 100, 0), 1); }
    confere("a mais VELHA saiu", vistonao_barra("tt9000000", 1, 1, 0), 0);
    reabrir();
    confere("e o arquivo relido tambem respeita o teto", vistonao_n() <= VISTONAO_MAX, 1);
    confere("com o conteudo de antes", vistonao_n() > VISTONAO_MAX - 100, 1);
  }
#endif

  if (falhas) { printf("\n%d FALHA(S)\n", falhas); return 1; }
  printf("\nok  desmarcar ganha do Trakt, da conta e de fechar o app\n");
  return 0;
}
