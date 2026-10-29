// logrede_limpar (PR #640): a pagina /log da rede de casa nunca expoe
// credencial, URL de addon/debrid ou titulo. Cada caso abaixo falhava antes
// do conserto do saneador.
#include "logrede.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void caso(const char *in, const char *esperado) {
  char b[512];
  snprintf(b, sizeof b, "%s", in);
  logrede_limpar(b);
  assert(!strcmp(b, esperado));
}

int main(void) {
  // userinfo (usuario:senha@) some da autoridade, host fica.
  caso("falhou https://user:senha123@provedor.com/stream/abc.m3u8",
       "falhou https://provedor.com/…");
  // userinfo + query: host fica, resto vira "…".
  caso("get http://u:p@host.test:8080/get.php?user=u&pass=p&type=m3u",
       "get http://host.test:8080/…");
  // magnet: xt (hash), dn (titulo) e tr (rastreador) somem juntos.
  caso("magnet:?xt=urn:btih:abc123DEF456&dn=Meu.Filme.2024.1080p&tr=udp://tracker.test:1337/ann",
       "magnet:…");
  // token longo so de letras (sem digito nenhum): mascara igual.
  caso("key AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA fim", "key AAAA… fim");
  // hex longo (hash de rastreador): mascara.
  caso("info_hash=abcdef0123456789abcdef0123456789", "info…");
  // base64 longo com +/= : mascara.
  caso("tok dGVzdGV0ZXN0ZXRlc3RldGVzdGV0ZXN0ZXRlc3RlPQ== ok", "tok dGVz… ok");
  // host sem caminho nao muda; URL com caminho continua cortada.
  caso("host http://exemplo.test respondeu", "host http://exemplo.test respondeu");
  caso("url https://exemplo.test/caminho/longo/com/segredo", "url https://exemplo.test/…");
  // texto normal passa intacto.
  caso("[fonte] tocando agora, 12.5 MB/s", "[fonte] tocando agora, 12.5 MB/s");
  puts("logrede: ok");
  return 0;
}
