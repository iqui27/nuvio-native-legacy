// Xtream (src/xtream.c): cadastro, lista de canais e URL, sem rede e sem disco.
//
// rede_baixar e dados_* sao DUBLES: a lista vem de um corpo canned no formato
// que os paineis Xtream Codes emitem (category_id ora "5" ora 5, stream_id
// numero), e o cadastro vive num buffer em memoria.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "xtream.h"

// --- dubles ----------------------------------------------------------------
static char disco[2048]; static int temDisco;
char *dados_ler(const char *nome) { (void)nome; return temDisco ? strdup(disco) : NULL; }
int dados_gravar(const char *nome, const char *c) { (void)nome; snprintf(disco, sizeof disco, "%s", c); temDisco = 1; return 1; }
int dados_apagar(const char *nome) { (void)nome; temDisco = 0; disco[0] = 0; return 1; }
int perfis_ativo(void) { return 1; }

static const char *respCats =
  "[{\"category_id\":\"5\",\"category_name\":\"Esportes\",\"parent_id\":0},"
  " {\"category_id\":7,\"category_name\":\"Not\\u00edcias\",\"parent_id\":0}]";
static const char *respStreams =
  "[{\"num\":1,\"name\":\"ESPN HD\",\"stream_type\":\"live\",\"stream_id\":101,"
  "  \"stream_icon\":\"http://x/espn.png\",\"epg_channel_id\":\"espn.br\",\"category_id\":\"5\"},"
  " {\"num\":2,\"name\":\"CNN\",\"stream_id\":\"202\",\"stream_icon\":\"\",\"category_id\":7},"
  " {\"num\":3,\"name\":\"\",\"stream_id\":303,\"category_id\":\"5\"},"
  " {\"num\":4,\"name\":\"Sem categoria\",\"stream_id\":404}]";
static const char *respAuth0 = "{\"user_info\":{\"auth\":0,\"status\":\"Disabled\"}}";
static int recusar, mudo;
static char ultimaUrl[1200];
char *rede_baixar(const char *url, int segundos) {
  (void)segundos;
  snprintf(ultimaUrl, sizeof ultimaUrl, "%s", url);
  if (mudo) return NULL;
  if (recusar) return strdup(respAuth0);
  if (strstr(url, "action=get_live_categories")) return strdup(respCats);
  if (strstr(url, "action=get_live_streams")) return strdup(respStreams);
  return NULL;
}

int main(void) {
  XtreamCanal c[16];
  char url[600];
  int n;

  assert(!xtream_configurado());
  assert(xtream_canais(c, 16) == 0);          // sem cadastro: nada, sem rede

  xtream_definir_servidor(" http://meu.servidor.tv:8080/ ");
  xtream_definir_usuario("joao@x");
  xtream_definir_senha("s&nha 1");
  assert(xtream_configurado());
  assert(!strcmp(xtream_servidor_curto(), "meu.servidor.tv:8080"));
  assert(!strcmp(xtream_usuario(), "joao@x"));
  assert(strstr(xtream_senha_mascarada(), "\xe2\x80\xa2") && !strstr(xtream_senha_mascarada(), "nha"));
  // Recarrega do "disco": o que foi gravado e o que volta.
  assert(strstr(disco, "servidor\thttp://meu.servidor.tv:8080\n"));
  assert(strstr(disco, "# CREDENCIAL"));
  puts("ok  cadastro: normaliza, mascara, grava com aviso");

  n = xtream_canais(c, 16);
  assert(n == 3);                              // o de nome vazio fica de fora
  assert(xtream_ultima_falha() == XT_OK);
  assert(!strcmp(c[0].id, "xtream:101") && !strcmp(c[0].nome, "ESPN HD"));
  assert(!strcmp(c[0].categoria, "Esportes") && !strcmp(c[0].epgId, "espn.br"));
  printf("  c[1]: id=%s cat=%s\n", c[1].id, c[1].categoria);
  assert(!strcmp(c[1].id, "xtream:202") && !strcmp(c[1].categoria, "Not\xc3\xad" "cias"));
  assert(!strcmp(c[2].id, "xtream:404") && !strcmp(c[2].categoria, "Outros"));
  assert(strstr(ultimaUrl, "username=joao%40x&password=s%26nha%201&action=get_live_streams"));
  puts("ok  canais: id numero ou texto, categoria por id, sem categoria = Outros");

  assert(xtream_e_id("xtream:101") && !xtream_e_id("stalker:1") && !xtream_e_id(NULL));
  assert(xtream_url("xtream:101", url, sizeof url));
  assert(!strcmp(url, "http://meu.servidor.tv:8080/live/joao%40x/s%26nha%201/101.m3u8"));
  assert(!xtream_url("tt123", url, sizeof url));
  puts("ok  url: servidor/live/usuario/senha/id.m3u8, com escape");

  recusar = 1;
  assert(xtream_canais(c, 16) == 0);           // auth 0 nao vira lista
  assert(xtream_ultima_falha() == XT_RECUSOU);
  recusar = 0;
  puts("ok  credencial recusada: zero canais, sem lixo");

  // #112: servidor que nao responde (o "falhou em http://..." do registro
  // 1647) e distinto de credencial recusada, e a proxima resposta boa limpa.
  mudo = 1;
  assert(xtream_canais(c, 16) == 0);
  assert(xtream_ultima_falha() == XT_SEM_RESPOSTA);
  mudo = 0;
  assert(xtream_canais(c, 16) == 3 && xtream_ultima_falha() == XT_OK);
  puts("ok  falha da lista: sem resposta, recusada e recuperada sao distintas");

  xtream_esquecer();
  assert(!xtream_configurado() && !temDisco);
  assert(!xtream_url("xtream:101", url, sizeof url));
  puts("ok  esquecer: apaga o arquivo e a URL deixa de existir");

  xtream_definir_servidor("https://seguro.tv");
  assert(!strcmp(xtream_servidor_curto(), "seguro.tv"));
  assert(strstr(disco, "servidor\thttps://seguro.tv\n"));
  puts("ok  https preservado quando digitado");
  puts("xtream: tudo ok");
  return 0;
}
