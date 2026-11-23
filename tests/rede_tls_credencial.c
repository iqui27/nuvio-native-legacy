// Pedido que leva credencial — no cabecalho OU na query — verifica o TLS e
// nao segue redirect; a midia publica dos addons (GET sem credencial)
// continua como sempre. Artefato de release baixa por rede_baixar_bin_seguro,
// que verifica sempre e so segue redirect no prefixo oficial de anexos; o
// rede_baixar_bin publico (legenda, cartaz, fonte) nao muda. Usa a
// implementacao real de rede.c com uma libcurl falsa que registra as opcoes
// no momento do perform.
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef NV_TPK40
void nv_tpk40_etapa(const char *s) { (void)s; }
#endif
#include "../src/rede.c"
typedef struct { long peer, host, follow; int pPeer, pHost, pFollow; } Fake;
static Fake ultimo;
static void *init(void) { return calloc(1, sizeof(Fake)); }
static int option(void *c, int opt, ...) {
  Fake *f = c; va_list ap; va_start(ap, opt);
  if (opt == OPT_SSL_VERIFYPEER) { f->peer = va_arg(ap, long); f->pPeer = 1; }
  else if (opt == OPT_SSL_VERIFYHOST) { f->host = va_arg(ap, long); f->pHost = 1; }
  else if (opt == OPT_FOLLOWLOCATION) { f->follow = va_arg(ap, long); f->pFollow = 1; }
  va_end(ap); return 0;
}
static int perform(void *c) { ultimo = *(Fake *)c; return 0; }
static int info(void *c, int opt, ...) {
  (void)c; va_list ap; va_start(ap, opt);
  if (opt == INFO_RESPONSE_CODE) *va_arg(ap, long *) = 200;
  va_end(ap); return 0;
}
static void cleanup(void *c) { free(c); }
static void *slAppend(void *l, const char *s) { (void)s; return l ? l : (void *)1; }
static void slFree(void *l) { (void)l; }

int main(void) {
  int st; char *b; long nb;
  const char *cabAut[] = { "Authorization: Bearer x", NULL };
  const char *cabKey[] = { "apikey: k", "Accept: */*", NULL };
  const char *cabNeutro[] = { "Accept: */*", "Range: bytes=0-9", NULL };
  const char *cabVazio[] = { NULL };
  pronto = 1; curl_init = init; curl_setopt_f = option; curl_perform = perform;
  curl_cleanup = cleanup; curl_getinfo = info; slist_append = slAppend; slist_free = slFree;

  // POST de API (login/token/sync): verificado.
  memset(&ultimo, 0, sizeof ultimo);
  b = rede_postar_st("https://api.example.test/token", 5, cabAut, "{}", &st);
  assert(b && st == 200); free(b);
  assert(ultimo.peer == 1 && ultimo.host == 2);

  // GET com credencial: verificado e SEM redirect.
  memset(&ultimo, 0, sizeof ultimo);
  b = rede_baixar_com("https://api.example.test/me", 5, cabAut); free(b);
  assert(ultimo.peer == 1 && ultimo.host == 2 && ultimo.pFollow && ultimo.follow == 0);
  memset(&ultimo, 0, sizeof ultimo);
  b = rede_baixar_com("https://api.example.test/me", 5, cabKey); free(b);
  assert(ultimo.peer == 1 && ultimo.follow == 0);

  // GET sem credencial (midia/imagem publica dos addons): comportamento de sempre.
  memset(&ultimo, 0, sizeof ultimo);
  b = rede_baixar_com("https://cdn.example.test/img.jpg", 5, cabNeutro); free(b);
  assert(ultimo.pPeer && ultimo.peer == 0 && ultimo.follow == 1);
  memset(&ultimo, 0, sizeof ultimo);
  b = rede_baixar("https://cdn.example.test/img.jpg", 5); free(b);
  assert(ultimo.pPeer && ultimo.peer == 0 && ultimo.follow == 1);

  // DELETE autenticado (Trakt /sync/playback): TLS verificado, sem redirect.
  memset(&ultimo, 0, sizeof ultimo);
  b = rede_apagar("https://api.example.test/sync/playback/1", 5, cabAut, &st);
  free(b);
  assert(ultimo.peer == 1 && ultimo.host == 2 && ultimo.pFollow && ultimo.follow == 0);

  // Credencial NA QUERY (a forma que o TorBox exige no requestdl, sem
  // cabecalho): TLS verificado e sem redirect, igual ao cabecalho.
  memset(&ultimo, 0, sizeof ultimo);
  b = rede_baixar_st("https://api.torbox.app/v1/api/torrents/requestdl?token=fixture&torrent_id=1&file_id=2&redirect=false",
                     5, cabVazio, &st); free(b);
  assert(ultimo.peer == 1 && ultimo.host == 2 && ultimo.pFollow && ultimo.follow == 0);
  memset(&ultimo, 0, sizeof ultimo);
  b = rede_baixar_st("https://api.torbox.app/v1/api/usenet/requestdl?token=fixture&usenet_id=1&file_id=2&redirect=false",
                     5, cabVazio, &st); free(b);
  assert(ultimo.peer == 1 && ultimo.host == 2 && ultimo.pFollow && ultimo.follow == 0);
  memset(&ultimo, 0, sizeof ultimo);
  b = rede_baixar("https://api.themoviedb.org/3/movie/1/watch/providers?api_key=fixture", 5); free(b);
  assert(ultimo.peer == 1 && ultimo.host == 2 && ultimo.pFollow && ultimo.follow == 0);
  memset(&ultimo, 0, sizeof ultimo);
  b = rede_baixar("https://api.mdblist.com/imdb/tt1/movie?apikey=fixture", 5); free(b);
  assert(ultimo.peer == 1 && ultimo.host == 2 && ultimo.pFollow && ultimo.follow == 0);
  memset(&ultimo, 0, sizeof ultimo);
  b = rede_baixar_st("https://painel.example.test/player_api.php?username=u&password=fixture",
                     5, cabVazio, &st); free(b);
  assert(ultimo.peer == 1 && ultimo.host == 2 && ultimo.pFollow && ultimo.follow == 0);
  // No POST e no DELETE a credencial na query tambem desliga o redirect.
  memset(&ultimo, 0, sizeof ultimo);
  b = rede_postar_st("https://api.example.test/x?token=fixture", 5, cabNeutro, "{}", &st); free(b);
  assert(ultimo.peer == 1 && ultimo.host == 2 && ultimo.pFollow && ultimo.follow == 0);
  memset(&ultimo, 0, sizeof ultimo);
  b = rede_apagar("https://api.example.test/sync/playback/1?token=fixture", 5, cabNeutro, &st);
  free(b);
  assert(ultimo.peer == 1 && ultimo.host == 2 && ultimo.pFollow && ultimo.follow == 0);

  // Nome de parametro que so lembra segredo nao conta: GET publico inalterado.
  memset(&ultimo, 0, sizeof ultimo);
  b = rede_baixar("https://cdn.example.test/x?monkey=1&tokenize=2&keynote=3", 5); free(b);
  assert(ultimo.pPeer && ultimo.peer == 0 && ultimo.follow == 1);

  // rede_baixar_bin_seguro: caminho verificado dos artefatos de release
  // (.so do .tpk, .apk). Redirect so no prefixo oficial de anexos (o 302 do
  // GitHub para o CDN de assets); fora dele nao segue.
  memset(&ultimo, 0, sizeof ultimo);
  free(rede_baixar_bin_seguro("https://github.com/iqui27/nuvio-native-legacy/releases/download/v9.9.9/Nuvio-9.9.9-tpk-arm.so",
                              5, &nb));
  assert(ultimo.peer == 1 && ultimo.host == 2 && ultimo.follow == 1);
  memset(&ultimo, 0, sizeof ultimo);
  free(rede_baixar_bin_seguro("https://cdn.example.test/legendas/x.srt", 5, &nb));
  assert(ultimo.peer == 1 && ultimo.host == 2 && ultimo.pFollow && ultimo.follow == 0);
  // Binario seguro com credencial na query: a regra do GET com segredo precede.
  memset(&ultimo, 0, sizeof ultimo);
  free(rede_baixar_bin_seguro("https://cdn.example.test/x.bin?token=fixture", 5, &nb));
  assert(ultimo.peer == 1 && ultimo.host == 2 && ultimo.pFollow && ultimo.follow == 0);

  // rede_baixar_bin publico (legenda, cartaz, fonte) NAO muda: GET publico
  // como sempre, TLS aberto e redirect ligado.
  memset(&ultimo, 0, sizeof ultimo);
  free(rede_baixar_bin("https://cdn.example.test/legendas/x.srt", 5, &nb));
  assert(ultimo.pPeer && ultimo.peer == 0 && ultimo.follow == 1);

  // Credencial em http:// nao muda nada aqui (nao ha TLS a verificar); segue como era.
  memset(&ultimo, 0, sizeof ultimo);
  b = rede_baixar_com("http://127.0.0.1/x", 5, cabAut); free(b);
  assert(ultimo.peer == 0 && ultimo.follow == 1);
  puts("rede_tls_credencial: ok");
  return 0;
}
