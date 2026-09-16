// Audiencia da serie: leitura do /stats, a conta da retencao (inclusive a
// armadilha do episodio com MAIS gente que o E1), temporada com buracos, e o
// cache de disco sobrevivendo a um arranque.
//
//   bash tests/serieaud.sh
//
// Inclui o .c de proposito, como tests/atualizacao.c e tests/recomenda.c: a
// leitura do JSON e o fio sao estaticos, e expo-los so para o teste seria API a
// mais. O .sh compila tudo MENOS src/serieaud.c.
//
// A REDE E O TRAKT SAO INTERCEPTADOS POR #define, antes do include. As
// respostas sao CORPOS REAIS, copiados da api em 16/09/2026 — inventar um
// corpo de mentira e como testar o parser contra ele mesmo.
//
// ONDE ELE ESCREVE: exclusivamente em NUVIO_DADOS, e ele CONFERE isso antes de
// qualquer escrita. Um teste desta pasta ja sobrescreveu os dados de verdade do
// dono uma vez; a trava abaixo existe por causa disso.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define rede_baixar_com   teste_rede_com
#define trakt_cabecalhos  teste_trakt_cab

#include "../src/serieaud.c"

// --- DUBLE DA REDE -----------------------------------------------------------

static int  nPedidos;
static char ultimaUrl[300];
// Respostas por episodio: indice = numero do episodio. NULL = o servidor
// respondeu nada (o que rede_baixar_com devolve em 4xx e em falha de rede).
static const char *respEp[40];
static const char *respSerie;

int teste_trakt_cab(const char **cab, char *aut, size_t nAut, char *chave,
                    size_t nChave) {
  snprintf(aut, nAut, "Authorization: Bearer teste");
  snprintf(chave, nChave, "trakt-api-key: teste");
  cab[0] = aut; cab[1] = "trakt-api-version: 2"; cab[2] = chave; cab[3] = NULL;
  return 1;
}

static char *dup(const char *s) {
  char *r;
  if (!s) return NULL;
  r = (char *)malloc(strlen(s) + 1);
  if (r) strcpy(r, s);
  return r;
}

char *teste_rede_com(const char *url, int seg, const char *const *cab) {
  const char *p;
  (void)seg; (void)cab;
  nPedidos++;
  snprintf(ultimaUrl, sizeof ultimaUrl, "%s", url);
  p = strstr(url, "/episodes/");
  if (p) {
    int ep = atoi(p + 10);
    if (ep >= 0 && ep < 40) return dup(respEp[ep]);
    return NULL;
  }
  return dup(respSerie);
}

// --- CONFERENCIAS ------------------------------------------------------------

static int falhas;
#define CONFERE(c, ...) do { if (!(c)) { falhas++; printf("FALHA: " __VA_ARGS__); \
                                         printf("\n"); } } while (0)

// Espera o fio terminar. O duble da rede e instantaneo, entao isto sai em
// poucos milissegundos; o teto existe para o teste falhar em vez de travar.
static void esperar(void) {
  int k;
  for (k = 0; k < 2000 && serieaud_carregando(); k++) {
    struct timespec t = { 0, 2000000 };  /* 2 ms */
    nanosleep(&t, NULL);
  }
  CONFERE(!serieaud_carregando(), "o fio nao terminou em 4 s");
}

// Zera o modulo como se o processo tivesse acabado de nascer, SEM TOCAR NO
// DISCO — e assim que se prova "o cache sobrevive ao arranque" sem precisar de
// um segundo processo.
static void simularArranque(void) {
  memset(eps, 0, sizeof eps);
  nEps = 0;
  imdbAtual[0] = 0;
  imdbPedido[0] = 0;
  temporadaAtual = tempPedida = 0;
  playsSerie = watchersSerie = -1;
  truncada = 0;
  selecionado = 0;
  abandonar = 0;
}

static void limparRespostas(void) {
  memset(respEp, 0, sizeof respEp);
  respSerie = NULL;
  nPedidos = 0;
}

// --- CORPOS REAIS ------------------------------------------------------------
// Copiados da api.trakt.tv em 16/09/2026, com APENAS o client id no cabecalho
// (sem Authorization) — foi assim que se descobriu que /stats nao exige token.

#define BB_E1 "{\"watchers\":387823,\"plays\":460619,\"collectors\":621573," \
              "\"comments\":29,\"lists\":867,\"votes\":4935}"
#define BB_E7 "{\"watchers\":367883,\"plays\":426886,\"collectors\":634202," \
              "\"comments\":12,\"lists\":264,\"votes\":3532}"
#define BB_SERIE "{\"collectors\":848887,\"comments\":568,\"favorited\":25383," \
                 "\"lists\":270394,\"plays\":24126034,\"votes\":72017," \
                 "\"watchers\":404730}"
// star-trek-deep-space-nine T1: o E2 tem MAIS watchers que o E1. E o caso que
// quebra qualquer codigo que grampeie a retencao em 100%.
#define DS9_E1 "{\"watchers\":25164,\"plays\":38252,\"collectors\":176555," \
               "\"comments\":13,\"lists\":358,\"votes\":715}"
#define DS9_E2 "{\"watchers\":25245,\"plays\":31669,\"collectors\":175071," \
               "\"comments\":4,\"lists\":205,\"votes\":574}"
#define DS9_E3 "{\"watchers\":24902,\"plays\":31149,\"collectors\":174379," \
               "\"comments\":4,\"lists\":201,\"votes\":539}"

int main(void) {
  const char *dir = getenv("NUVIO_DADOS");
  int epNum[8], nota[8];
  int i;

  // A TRAVA. Sem ela este teste grava serieaud-*.txt dentro do ~/.nuvio de quem
  // o executa.
  if (!dir || !dir[0]) {
    printf("serieaud: NUVIO_DADOS nao esta no ambiente; recusando rodar\n");
    return 2;
  }
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) {
    printf("serieaud: dados_dir() e \"%s\" e NUVIO_DADOS e \"%s\"; recusando\n",
           dados_dir(), dir);
    return 2;
  }

  // --- 1. LEITURA DE UM /stats DE VERDADE -----------------------------------
  { SaEp d;
    memset(&d, 0, sizeof d);
    CONFERE(lerStats(BB_E1, &d) == 1, "o corpo real do E1 nao foi lido");
    CONFERE(d.watchers == 387823, "watchers %ld", d.watchers);
    CONFERE(d.plays == 460619, "plays %ld", d.plays);
    CONFERE(d.comentarios == 29, "comments %d", d.comentarios);
    CONFERE(d.votos == 4935, "votes %d", d.votos);
    // Corpo vazio, corpo de erro e NULL nao podem virar "0 espectadores": zero
    // e um numero, e um grafico com zero diz uma coisa que ninguem mediu.
    memset(&d, 0, sizeof d);
    CONFERE(lerStats(NULL, &d) == 0, "NULL virou dado");
    CONFERE(lerStats("{\"error\":\"not found\"}", &d) == 0, "erro virou dado");
    CONFERE(d.tem == 0, "marcou tem=1 sem watchers"); }

  // --- 2. TEMPORADA COMPLETA, CURVA DESCENDENTE -----------------------------
  limparRespostas();
  respSerie = BB_SERIE;
  respEp[1] = BB_E1;
  respEp[7] = BB_E7;
  for (i = 0; i < 7; i++) { epNum[i] = i + 1; nota[i] = 80 + i; }
  serieaud_abrir("tt0903747", 1, epNum, nota, 7);
  esperar();
  // 1 pedido da serie + 7 dos episodios.
  CONFERE(nPedidos == 8, "pedidos %d, esperava 8", nPedidos);
  CONFERE(serieaud_pronto(), "E1 chegou e serieaud_pronto() e falso");
  CONFERE(serieaud_retencao(0) == 1000, "retencao do E1 %d", serieaud_retencao(0));
  // 367883/387823 = 0.9486 -> 949 milesimos.
  CONFERE(serieaud_retencao(6) == 949, "retencao do E7 %d", serieaud_retencao(6));
  // 460619/387823 = 1.1877 -> 119 centesimos.
  CONFERE(serieaud_rever(0) == 119, "rever do E1 %d", serieaud_rever(0));
  // 24126034/404730 = 59.61 -> 5961 centesimos.
  CONFERE(serieaud_rever_serie() == 5961, "rever da serie %d", serieaud_rever_serie());
  CONFERE(serieaud_nota_media() == 83, "media %d", serieaud_nota_media());
  CONFERE(serieaud_melhor() == 6 && serieaud_pior() == 0,
          "melhor %d pior %d", serieaud_melhor(), serieaud_pior());

  // --- 3. TEMPORADA COM BURACOS ---------------------------------------------
  // Os episodios 2..6 nao responderam. O que NAO pode acontecer: virarem zero,
  // contaminarem a media, ou fazerem a retencao do E7 mudar.
  CONFERE(serieaud_tem_stats(0) == 1, "E1 sem stats");
  CONFERE(serieaud_tem_stats(1) == 0, "E2 devia estar sem stats");
  CONFERE(serieaud_retencao(1) == -1, "E2 sem dado devolveu %d", serieaud_retencao(1));
  CONFERE(serieaud_rever(1) == -1, "rever do E2 sem dado devolveu %d", serieaud_rever(1));
  CONFERE(serieaud_watchers(1) == 0, "E2 sem dado tem watchers %ld",
          serieaud_watchers(1));
  CONFERE(serieaud_retencao(6) == 949, "buraco mexeu na retencao do E7");

  // --- 4. A ARMADILHA: E2 COM MAIS GENTE QUE O E1 ---------------------------
  // MEDIDO no Trakt (star-trek-deep-space-nine T1, 16/09/2026). Grampear em
  // 100% aqui esconderia o unico caso em que a curva tem algo a dizer sobre o
  // piloto.
  limparRespostas();
  respEp[1] = DS9_E1; respEp[2] = DS9_E2; respEp[3] = DS9_E3;
  for (i = 0; i < 3; i++) { epNum[i] = i + 1; nota[i] = 75; }
  serieaud_abrir("tt0106145", 1, epNum, nota, 3);
  esperar();
  CONFERE(serieaud_retencao(1) == 1003, "retencao do E2 %d, esperava 1003 (>100%%)",
          serieaud_retencao(1));
  CONFERE(serieaud_retencao(2) == 990, "retencao do E3 %d", serieaud_retencao(2));

  // --- 5. O CACHE SOBREVIVE AO ARRANQUE -------------------------------------
  // O arquivo foi gravado no passo 4. Um "arranque" zera a memoria e reabre a
  // MESMA temporada: nao pode sair pedido nenhum, e os numeros tem de voltar
  // identicos.
  simularArranque();
  limparRespostas();
  for (i = 0; i < 3; i++) { epNum[i] = i + 1; nota[i] = 75; }
  serieaud_abrir("tt0106145", 1, epNum, nota, 3);
  esperar();
  CONFERE(nPedidos == 0, "a segunda visita gastou %d pedidos", nPedidos);
  CONFERE(serieaud_watchers(0) == 25164, "E1 do cache %ld", serieaud_watchers(0));
  CONFERE(serieaud_retencao(1) == 1003, "retencao do cache %d", serieaud_retencao(1));
  CONFERE(serieaud_rever_serie() == -1 || serieaud_rever_serie() > 0,
          "indice da serie incoerente");

  // --- 6. O CACHE NAO SERVE PARA OUTRA TEMPORADA NEM PARA OUTRA SERIE -------
  // Um arquivo da temporada errada desenharia um grafico com a cara certa e os
  // numeros de outra coisa.
  simularArranque();
  limparRespostas();
  respEp[1] = BB_E1;
  for (i = 0; i < 3; i++) { epNum[i] = i + 1; nota[i] = 75; }
  serieaud_abrir("tt0106145", 2, epNum, nota, 3);  // temporada 2, sem cache
  esperar();
  CONFERE(nPedidos == 4, "T2 sem cache gastou %d pedidos, esperava 4", nPedidos);

  // --- 7. CACHE VENCIDO ------------------------------------------------------
  // Um arquivo com data velha tem de ser ignorado: watchers de serie nova ainda
  // sobe rapido, e um grafico de duas semanas atras apresentado como atual e
  // numero errado com cara de certo.
  { char nome[80], buf[400];
    FILE *f;
    char caminho[600];
    nomeArquivo(nome, sizeof nome, "tt0106145", 3);
    snprintf(buf, sizeof buf,
             "%s\ntt0106145\t3\t%lld\t100\t10\n1\t25164\t38252\t13\t715\n",
             SA_ARQ_V,
             (long long)time(NULL) - (long long)(SA_VALIDADE_DIAS + 1) * 86400);
    dados_caminho(caminho, sizeof caminho, nome);
    f = fopen(caminho, "w");
    CONFERE(f != NULL, "nao consegui gravar o cache vencido em %s", caminho);
    if (f) { fputs(buf, f); fclose(f); }
    simularArranque();
    limparRespostas();
    respEp[1] = DS9_E1;
    for (i = 0; i < 2; i++) { epNum[i] = i + 1; nota[i] = 75; }
    serieaud_abrir("tt0106145", 3, epNum, nota, 2);
    esperar();
    CONFERE(nPedidos == 3, "cache vencido foi usado (pedidos %d, esperava 3)",
            nPedidos); }

  // --- 7b. CACHE PARCIAL: A VISITA SEGUINTE COMPLETA, NAO REFAZ -------------
  // Uma visita interrompida (a pessoa saiu no meio) ou com /stats falhando
  // deixa o arquivo com PARTE da temporada. O que nao pode acontecer: nem
  // refazer tudo, nem congelar os buracos ate o cache vencer — os episodios que
  // faltam pareceriam "sem dado" por duas semanas por causa de uma queda.
  { simularArranque();
    limparRespostas();
    respSerie = BB_SERIE;
    respEp[1] = BB_E1;              /* so o E1 responde */
    for (i = 0; i < 3; i++) { epNum[i] = i + 1; nota[i] = 75; }
    serieaud_abrir("tt4444444", 1, epNum, nota, 3);
    esperar();
    CONFERE(nPedidos == 4, "1a visita parcial: %d pedidos", nPedidos);
    simularArranque();
    limparRespostas();
    respEp[2] = BB_E7; respEp[3] = BB_E7;   /* agora os outros respondem */
    for (i = 0; i < 3; i++) { epNum[i] = i + 1; nota[i] = 75; }
    serieaud_abrir("tt4444444", 1, epNum, nota, 3);
    esperar();
    CONFERE(nPedidos == 2, "2a visita: %d pedidos, esperava so os 2 que faltavam",
            nPedidos);
    CONFERE(serieaud_watchers(0) == 387823, "o E1 do cache se perdeu (%ld)",
            serieaud_watchers(0));
    CONFERE(serieaud_tem_stats(1) && serieaud_tem_stats(2),
            "os episodios que faltavam nao foram completados"); }

  // --- 8. TETO DE EPISODIOS --------------------------------------------------
  // Uma temporada maior que SA_EP_MAX nao pode virar SA_EP_MAX+1 pedidos nem
  // fingir que a temporada acaba no teto.
  { int muitos[40], notas[40];
    simularArranque();
    limparRespostas();
    for (i = 0; i < 40; i++) { muitos[i] = i + 1; notas[i] = 70; }
    serieaud_abrir("tt9999999", 1, muitos, notas, 40);
    esperar();
    CONFERE(serieaud_n() == SA_EP_MAX, "n %d, esperava o teto %d",
            serieaud_n(), SA_EP_MAX);
    CONFERE(nPedidos == SA_EP_MAX + 1, "pedidos %d, esperava %d",
            nPedidos, SA_EP_MAX + 1);
    CONFERE(truncada == 1, "a temporada truncada nao ficou marcada"); }

  // --- 9. ABANDONO -----------------------------------------------------------
  // Sair da secao no meio precisa PARAR os pedidos. Aqui o abandono e levantado
  // antes do fio comecar, que e o caso limite: zero pedidos.
  { simularArranque();
    limparRespostas();
    pthread_mutex_lock(&trava);
    /* estado de "ja ha um fio vivo" para serieaud_abrir nao criar outro */
    pthread_mutex_unlock(&trava);
    for (i = 0; i < 3; i++) { epNum[i] = i + 1; nota[i] = 70; }
    serieaud_abrir("tt8888888", 1, epNum, nota, 3);
    serieaud_fechar();
    esperar();
    CONFERE(nPedidos <= 4, "abandono nao limitou os pedidos (%d)", nPedidos); }

  printf(falhas ? "serieaud: %d falha(s)\n" : "serieaud: ok\n", falhas);
  return falhas ? 1 : 0;
}
