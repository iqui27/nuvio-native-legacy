// A ordem das fileiras da home vinda da conta chega com o sentido certo?
//
// Este teste existe porque TODO defeito desta area e mudo. Uma ordem que nao e
// aplicada nao da erro: a pessoa so acha que a TV ignora o que ela arrumou no
// app web. E o defeito oposto e pior — aplicar a ordem remota CRUA remove os
// catalogos que passaram a existir depois de ela ter sido gravada, e o sintoma
// medido na OLED65C9 foi `{"localItems":54,"remoteItems":43}` em todo boot, com
// a home reescrita e reinvalidada sem nunca convergir.
//
// Os casos abaixo sao o contrato do Anexo C1, na ordem em que ele o descreve.
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "catordem.h"

static int falhas;

static void confere(const char *o_que, int obtido, int esperado) {
  int ok = obtido == esperado;
  printf("  %-56s %s (obtido %d, esperado %d)\n", o_que, ok ? "ok    " : "FALHOU",
         obtido, esperado);
  if (!ok) falhas++;
}

static void confereTexto(const char *o_que, const char *obtido, const char *esperado) {
  int ok = !strcmp(obtido, esperado);
  printf("  %-56s %s (obtido \"%s\", esperado \"%s\")\n", o_que,
         ok ? "ok    " : "FALHOU", obtido, esperado);
  if (!ok) falhas++;
}

// A uniao como a descoberta a aplica: chaves locais na ordem em que ja estavam,
// devolvidas na ordem final. Escreve o resultado como "a,b,c" para poder ser
// comparado de uma vez — meia ordem certa nao e ordem certa.
static void unir(const char *const *locais, int n, char *dst, size_t tam) {
  int saida[16], q, i;
  size_t k = 0;
  q = catordem_unir(locais, n, saida, 16);
  dst[0] = 0;
  for (i = 0; i < q; i++)
    k += (size_t)snprintf(dst + k, tam - k, i ? ",%s" : "%s", locais[saida[i]]);
}

// #203: leitor em outro fio enquanto catordem_ler reescreve. Cada chave lida
// tem de ser inteira (uma das duas formas), nunca metade de uma e outra.
static volatile int rodando;
static volatile int corrompido;
static void *leitor(void *u) {
  (void)u;
  while (rodando) {
    int n = catordem_n(), i;
    for (i = 0; i < n; i++) {
      const char *c = catordem_chave(i);
      if (c[0] && strncmp(c, "aaa_movie_", 10) && strncmp(c, "bbb_movie_", 10)) corrompido = 1;
    }
  }
  return NULL;
}

int main(void) {
  char linha[400];

  // Tres catalogos que este aparelho realmente tem, na ordem em que os addons
  // os declararam. "cinemeta_movie_top" e o que a conta NAO conhece: ele foi
  // instalado depois de a ordem ter sido gravada.
  static const char *LOCAIS[3] = {
    "xperience_movie_foryou", "cinemeta_movie_top", "cinemeta_series_trending"
  };

  printf("formato moderno (items[]):\n");
  // `order` fora de ordem no array de proposito: quem manda e o campo, nao a
  // posicao. E o `type` vem em MAIUSCULA, que o web normaliza.
  confere("resposta com ordem nova muda o estado", catordem_ler(
    "[{\"settings_json\":{\"items\":["
    "{\"addon_id\":\"cinemeta\",\"type\":\"SERIES\",\"catalog_id\":\"trending\",\"order\":1},"
    "{\"addon_id\":\"xperience\",\"type\":\"movie\",\"catalog_id\":\"foryou\",\"order\":0}"
    "]}}]"), 1);
  confere("duas chaves na ordem da conta", catordem_n(), 2);
  confereTexto("primeira", catordem_chave(0), "xperience_movie_foryou");
  confereTexto("segunda (type normalizado)", catordem_chave(1), "cinemeta_series_trending");

  printf("\nuniao, nao substituicao:\n");
  unir(LOCAIS, 3, linha, sizeof linha);
  confereTexto("remoto primeiro, local desconhecido no FIM", linha,
               "xperience_movie_foryou,cinemeta_series_trending,cinemeta_movie_top");

  printf("\nchave remota que nao existe local e ignorada:\n");
  {
    static const char *SO_UM[1] = { "cinemeta_series_trending" };
    unir(SO_UM, 1, linha, sizeof linha);
    confereTexto("nao inventa fileira que o aparelho nao tem", linha,
                 "cinemeta_series_trending");
  }

  printf("\n`enabled` ausente significa LIGADO:\n");
  catordem_esquecer();
  catordem_ler(
    "[{\"settings_json\":{\"items\":["
    "{\"addon_id\":\"cinemeta\",\"type\":\"movie\",\"catalog_id\":\"top\",\"order\":0},"
    "{\"addon_id\":\"xperience\",\"type\":\"movie\",\"catalog_id\":\"foryou\","
    "\"enabled\":false,\"order\":1}]}}]");
  confere("sem `enabled` fica visivel",
          catordem_oculta("cinemeta_movie_top", ""), 0);
  confere("`enabled:false` fica oculta",
          catordem_oculta("xperience_movie_foryou", ""), 1);

  printf("\ncolecao tem chave propria (collection_<id>):\n");
  catordem_esquecer();
  catordem_ler("[{\"settings_json\":{\"items\":["
               "{\"is_collection\":true,\"collection_id\":\"Awards\",\"order\":0},"
               "{\"is_collection\":true,\"order\":1}]}}]");
  confere("colecao sem id nao entra (nao tem identidade)", catordem_n(), 1);
  confereTexto("chave de colecao", catordem_chave(0), "collection_Awards");

  printf("\nformato legado (arrays de chaves soltas):\n");
  catordem_esquecer();
  // `home_catalog_order` e o SEGUNDO nome da lista: o primeiro nao esta aqui, e
  // o leitor tem de cair nele em vez de desistir. A chave de DESATIVAR e a
  // longa, com base e nome — a mesma que o web grava pela tela de ajustes.
  confere("blob legado e aceito", catordem_ler(
    "[{\"settings_json\":{"
    "\"home_catalog_order\":[\"cinemeta_series_trending\",\"xperience_movie_foryou\"],"
    "\"hidden_catalog_keys\":[\"https://x.tv/manifest.json_movie_foryou_For You\"]"
    "}}]"), 1);
  confere("duas chaves na ordem", catordem_n(), 2);
  unir(LOCAIS, 3, linha, sizeof linha);
  confereTexto("ordem legada aplicada, local novo no fim", linha,
               "cinemeta_series_trending,xperience_movie_foryou,cinemeta_movie_top");
  confere("oculta pela chave de DESATIVAR",
          catordem_oculta("xperience_movie_foryou",
                          "https://x.tv/manifest.json_movie_foryou_For You"), 1);
  confere("outra fileira segue visivel",
          catordem_oculta("cinemeta_movie_top", ""), 0);

  printf("\nresposta vazia NAO destroi a ordem local:\n");
  // O caso que apaga a home de alguem. Blob sem ordem nenhuma e "a pessoa nunca
  // configurou", nao "a pessoa apagou tudo".
  confere("blob sem ordem nao muda nada", catordem_ler("[{\"settings_json\":{}}]"), 0);
  confere("a ordem anterior continua de pe", catordem_n(), 2);
  confere("resposta vazia da RPC nao muda nada", catordem_ler("[]"), 0);
  confere("a ordem anterior continua de pe", catordem_n(), 2);
  unir(LOCAIS, 3, linha, sizeof linha);
  confereTexto("e continua sendo aplicada", linha,
               "cinemeta_series_trending,xperience_movie_foryou,cinemeta_movie_top");

  printf("\nmesma resposta duas vezes nao pede remontagem:\n");
  {
    static const char *BLOB =
      "[{\"settings_json\":{\"items\":["
      "{\"addon_id\":\"cinemeta\",\"type\":\"movie\",\"catalog_id\":\"top\",\"order\":0}]}}]";
    catordem_esquecer();
    confere("primeira leitura muda", catordem_ler(BLOB), 1);
    confere("segunda leitura nao muda", catordem_ler(BLOB), 0);
  }

  printf("\nbooleanos ausentes = \"mantem o local\":\n");
  catordem_esquecer();
  catordem_ler("[{\"settings_json\":{\"catalog_order_keys\":[\"a_movie_b\"]}}]");
  confere("hide_unreleased_content ausente", catordem_tem_ocultar_nao_lancados(), 0);
  confere("hide_catalog_underline ausente", catordem_tem_ocultar_sublinhado(), 0);
  catordem_esquecer();
  catordem_ler("[{\"settings_json\":{\"catalog_order_keys\":[\"a_movie_b\"],"
               "\"hide_unreleased_content\":true,\"hide_catalog_underline\":false}}]");
  confere("hide_unreleased_content presente", catordem_tem_ocultar_nao_lancados(), 1);
  confere("...e vale true",                   catordem_ocultar_nao_lancados(), 1);
  confere("hide_catalog_underline presente",  catordem_tem_ocultar_sublinhado(), 1);
  confere("...e vale false",                  catordem_ocultar_sublinhado(), 0);

  {
    char blob[100000];
    int pos = snprintf(blob, sizeof blob, "{\"items\":[");
    catordem_esquecer();
    for (int k = 0; k < 300; k++)
      pos += snprintf(blob + pos, sizeof blob - pos,
        "%s{\"addon_id\":\"a\",\"type\":\"movie\",\"catalog_id\":\"c%d\",\"order\":%d,\"enabled\":%s}",
        k ? "," : "", k, 299-k, k == 299 ? "false" : "true");
    snprintf(blob + pos, sizeof blob - pos, "]}");
    confere("large account accepted", catordem_ler(blob), 1);
    confere("all 300 catalogues retained", catordem_n(), 300);
    confereTexto("late item sorted first", catordem_chave(0), "a_movie_c299");
    confere("late disabled item hidden", catordem_oculta("a_movie_c299", ""), 1);
    confere("explicit empty items clears remote state", catordem_ler("{\"items\":[]}"), 1);
    confere("remote order cleared", catordem_n(), 0);
    confere("remote hidden state cleared", catordem_oculta("a_movie_c299", ""), 0);
    confere("repeated empty does not rebuild", catordem_ler("{\"items\":[]}"), 0);
  }

  printf("\nlogout esquece a ordem da conta anterior:\n");
  catordem_esquecer();
  confere("sem ordem depois de esquecer", catordem_tem_ordem(), 0);
  unir(LOCAIS, 3, linha, sizeof linha);
  confereTexto("volta a ordem local intacta", linha,
               "xperience_movie_foryou,cinemeta_movie_top,cinemeta_series_trending");

  printf("\nleitura concorrente com catordem_ler:\n");
  { pthread_t t; int k;
    const char *A = "[{\"settings_json\":{\"catalog_order_keys\":[\"aaa_movie_1\",\"aaa_movie_2\",\"aaa_movie_3\"]}}]";
    const char *B = "[{\"settings_json\":{\"catalog_order_keys\":[\"bbb_movie_1\",\"bbb_movie_2\"]}}]";
    rodando = 1;
    pthread_create(&t, NULL, leitor, NULL);
    for (k = 0; k < 20000; k++) catordem_ler(k & 1 ? A : B);
    rodando = 0;
    pthread_join(t, NULL);
    confere("nenhuma chave lida pela metade", corrompido, 0);
  }

  printf("\n%s\n", falhas ? "FALHOU" : "PASSOU");
  return falhas ? 1 : 0;
}
