// O download verificado de artefato de release (rede_baixar_bin_seguro) segue
// o 302 do GitHub para o CDN — mas a cadeia tem regra propria (F89): saltos
// so em HTTPS, so para hosts oficiais (github.com e os CDNs de assets) e com
// teto. Cada Location e conferida pelo recebedor de cabecalho ANTES de ser
// seguida: fora da lista a linha volta curta e a libcurl aborta. E a
// credencial so conta na QUERY de verdade: ?token= dentro do fragmento
// (#...) nao e credencial, entao a midia publica segue como sempre.
//
//   bash tests/rede_release_redirect.sh [--sanitize]
//
// Libcurl falsa que registra as opcoes no perform; o script de cabecalhos e
// alimentado DENTRO do perform, quando o CacaCab da chamada ainda esta vivo —
// como a libcurl de verdade faz com cada linha de resposta.
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef NV_TPK40
void nv_tpk40_etapa(const char *s) { (void)s; }
#endif
#include "../src/rede.c"

// Na base do PR estas constantes nao existiam em rede.c; o teste precisa
// compilar la para provar que falha (sem REDIR/MAXREDIRS e sem o recebedor).
#ifndef OPT_REDIR_PROTOCOLS
#define OPT_REDIR_PROTOCOLS 182
#endif
#ifndef OPT_MAXREDIRS
#define OPT_MAXREDIRS 68
#endif

typedef struct {
  long peer, host, follow, redirProto, maxRedirs;
  int pPeer, pHost, pFollow, pRedir, pMax;
  void *hfn, *hdata;
} Fake;
static Fake ultimo;
static const char *scriptCab[8];   // linhas alimentadas durante o perform
static void *init(void) { return calloc(1, sizeof(Fake)); }
static int option(void *c, int opt, ...) {
  Fake *f = c; va_list ap; va_start(ap, opt);
  if (opt == OPT_SSL_VERIFYPEER) { f->peer = va_arg(ap, long); f->pPeer = 1; }
  else if (opt == OPT_SSL_VERIFYHOST) { f->host = va_arg(ap, long); f->pHost = 1; }
  else if (opt == OPT_FOLLOWLOCATION) { f->follow = va_arg(ap, long); f->pFollow = 1; }
  else if (opt == OPT_REDIR_PROTOCOLS) { f->redirProto = va_arg(ap, long); f->pRedir = 1; }
  else if (opt == OPT_MAXREDIRS) { f->maxRedirs = va_arg(ap, long); f->pMax = 1; }
  else if (opt == OPT_HEADERFUNCTION) f->hfn = va_arg(ap, void *);
  else if (opt == OPT_HEADERDATA) f->hdata = va_arg(ap, void *);
  va_end(ap); return 0;
}
static size_t ultimaLinha;   // retorno da ultima linha alimentada no receberCab
static int perform(void *c) {
  Fake *f = c; int i;
  ultimo = *f;
  ultimaLinha = 0;
  for (i = 0; scriptCab[i] && f->hfn; i++) {
    size_t (*fn)(void *, size_t, size_t, void *) = f->hfn;
    size_t n = strlen(scriptCab[i]);
    ultimaLinha = fn((void *)scriptCab[i], 1, n, f->hdata);
    // Linha curta = a libcurl aborta a transferencia com CURLE_WRITE_ERROR.
    if (ultimaLinha != n) return 23;
  }
  return 0;
}
static int info(void *c, int opt, ...) {
  (void)c; va_list ap; va_start(ap, opt);
  if (opt == INFO_RESPONSE_CODE) *va_arg(ap, long *) = 200;
  va_end(ap); return 0;
}
static void cleanup(void *c) { free(c); }
static void *slAppend(void *l, const char *s) { (void)s; return l ? l : (void *)1; }
static void slFree(void *l) { (void)l; }

static char *baixa(const char *url, int segundos, long *nb, const char *location) {
  memset(&ultimo, 0, sizeof ultimo);
  scriptCab[0] = location; scriptCab[1] = NULL;
  return rede_baixar_bin_seguro(url, segundos, nb);
}
// Mesmo pedido, mas o roteiro tem VARIAS linhas de cabecalho — para o
// Location dobrado (a 1a linha dele seguida da continuacao).
static char *baixaSeq(const char *url, int segundos, long *nb,
                      const char *const *linhas) {
  int i;
  memset(&ultimo, 0, sizeof ultimo);
  for (i = 0; linhas[i] && i < 7; i++) scriptCab[i] = linhas[i];
  scriptCab[i] = NULL;
  return rede_baixar_bin_seguro(url, segundos, nb);
}

int main(void) {
  long nb = 0;
  const char *oficial = "https://github.com/iqui27/nuvio-native-legacy/releases/download/v9.9.9/Nuvio-9.9.9-tpk-arm.so";
  pronto = 1; curl_init = init; curl_setopt_f = option; curl_perform = perform;
  curl_cleanup = cleanup; curl_getinfo = info; slist_append = slAppend; slist_free = slFree;

  // Prefixo oficial: TLS verificado e redirect ligado, MAS com protocolo
  // https nos saltos, teto de cadeia e o recebedor de cabecalho instalado.
  free(baixa(oficial, 5, &nb, NULL));
  assert(ultimo.peer == 1 && ultimo.host == 2 && ultimo.follow == 1);
  assert(ultimo.pRedir && ultimo.redirProto == 2);   // CURLPROTO_HTTPS
  assert(ultimo.pMax && ultimo.maxRedirs > 0 && ultimo.maxRedirs <= 10);
  assert(ultimo.hfn && ultimo.hdata);

  // Location para http:// ou para host fora da lista: a linha volta CURTA
  // no receberCab (ultimaLinha == 0), o perform sai com write-error e o
  // download e recusado.
  { static const char *const ruins[] = {
      "Location: http://github.com/iqui27/x\r\n",
      "Location: https://cdn.example.test/x\r\n",
      "Location: https://github.com.evil.test/x\r\n",
      "Location: https://release-assets.githubusercontent.com.evil.test/x\r\n",
      "Location: /releases/download/v9.9.9/x\r\n",
      "Location: https://user@github.com/x\r\n",
      "Location: https://user:senha@github.com/x\r\n",
      "Location: https://github.com@evil.test/x\r\n",   // oficial@evil
      "Location: https://github.com:443@evil.test/x\r\n", NULL };
    int i;
    for (i = 0; ruins[i]; i++) {
      assert(baixa(oficial, 5, &nb, ruins[i]) == NULL);
      assert(ultimaLinha == 0);
    }
  }

  // Destinos oficiais passam — a linha volta inteira e o perform segue: o
  // CDN de assets, o proprio github.com e o CDN de objects, com caixa
  // diferente no nome do cabecalho e porta explicita.
  { static const char *const bons[] = {
      "Location: https://release-assets.githubusercontent.com/github-production-release-asset/x\r\n",
      "location: https://github.com/iqui27/outro\r\n",
      "Location: https://objects.githubusercontent.com/x\r\n",
      "Location: https://release-assets.githubusercontent.com:443/x\r\n",
      "Content-Length: 4\r\n", NULL };
    int i;
    for (i = 0; bons[i]; i++) {
      free(baixa(oficial, 5, &nb, bons[i]));
      assert(ultimaLinha == strlen(bons[i]));
    }
  }

  // F107: host oficial na autoridade nao basta — espaco, tab ou controle no
  // VALOR faria a libcurl seguir mesmo assim (o caminho sai codificado no
  // segundo GET). Recusado como qualquer destino fora da lista.
  { static const char *const ruins[] = {
      "Location: https://github.com/final com espaco\r\n",
      "Location: https://github.com/final\tcom\ttab\r\n",
      "Location: https://github.com/fin\x01" "al\r\n",
      "Location: https://github.com/fin\x7f" "al\r\n", NULL };
    int i;
    for (i = 0; ruins[i]; i++) {
      assert(baixa(oficial, 5, &nb, ruins[i]) == NULL);
      assert(ultimaLinha == 0);
    }
  }

  // E o Location DOBRADO (obs-fold): a 1a linha sozinha pareceria aceitavel,
  // mas a continuacao estende o valor — a libcurl seguiria o destino dela ou
  // a costura das duas. A resposta e recusada na linha de continuacao.
  { static const char *const dobrados[][4] = {
      { "Location: https://github.com/final\r\n", " continuacao\r\n", NULL },
      { "Location: https://github.com/final\r\n", "\tcontinuacao\r\n", NULL },
      { "Location: https://github.com/final\r\n",
        " https://objects.githubusercontent.com/costura\r\n", NULL } };
    int i;
    for (i = 0; i < 3; i++) {
      assert(baixaSeq(oficial, 5, &nb, dobrados[i]) == NULL);
      assert(ultimaLinha == 0);
    }
  }

  // A dobra nao se prova de qual cabecalho e — no caminho verificado
  // QUALQUER continuacao recusa a resposta, mesmo aparentada a outro
  // cabecalho inofensivo.
  { static const char *const eTagDobrado[] = {
      "ETag: \"abc\r\n", " def\"\r\n",
      "Location: https://github.com/final\r\n", NULL };
    assert(baixaSeq(oficial, 5, &nb, eTagDobrado) == NULL);
    assert(ultimaLinha == 0);
  }

  // F101: a autoridade tem de ser conferida INTEIRA ou o salto e recusado.
  // Esta linha e longa demais para a copia interna do receberCab: o prefixo
  // cortado le "github.com + porta decimal" e passaria no filtro, mas a
  // autoridade completa continua com '@' — userinfo que aponta a libcurl
  // para outro host. Aceitar seria autorizar um destino diferente do lido.
  { char grande[1400];
    int i = snprintf(grande, sizeof grande, "Location: https://github.com:");
    memset(grande + i, '4', 1100);
    strcpy(grande + i + 1100, "@evil.test/x\r\n");
    assert(baixa(oficial, 5, &nb, grande) == NULL);
    assert(ultimaLinha == 0);
  }

  // Location longo que CABE inteiro na copia segue conferido e aceito — o
  // limite so derruba o que nao pode ser lido por completo.
  { char ok[1200];
    size_t i = snprintf(ok, sizeof ok, "Location: https://github.com/iqui27/");
    memset(ok + i, 'a', 1100);
    strcpy(ok + i + 1100, "\r\n");
    free(baixa(oficial, 5, &nb, ok));
    assert(ultimaLinha == strlen(ok));
  }

  // E o que nao cabe nem na copia inteira e recusado por tamanho, sem
  // validar um pedaco (mesmo teto da reserva segura, alvo[2048]).
  { char enorme[2300];
    int i = snprintf(enorme, sizeof enorme, "Location: https://github.com/");
    memset(enorme + i, 'a', 2100);
    strcpy(enorme + i + 2100, "x\r\n");
    assert(baixa(oficial, 5, &nb, enorme) == NULL);
    assert(ultimaLinha == 0);
  }

  // Fora do prefixo oficial: redirect desligado, sem regra de saltos e sem o
  // recebedor de cabecalho.
  free(baixa("https://cdn.example.test/x.bin", 5, &nb, NULL));
  assert(ultimo.peer == 1 && ultimo.host == 2 && ultimo.pFollow && ultimo.follow == 0);
  assert(!ultimo.pRedir && !ultimo.pMax && ultimo.hfn == NULL);
  // Binario seguro com credencial na query: a regra do segredo precede —
  // redirect desligado e nenhum Location e conferido (nem instalado).
  free(baixa("https://github.com/iqui27/nuvio-native-legacy/releases/download/v9.9.9/x?token=fixture",
             5, &nb, "Location: https://cdn.example.test/x\r\n"));
  assert(ultimo.peer == 1 && ultimo.host == 2 && ultimo.follow == 0);
  assert(!ultimo.pRedir && ultimo.hfn == NULL);

  // ?token= SO NO FRAGMENTO nao e credencial (F89): a query e o que vem antes
  // do #. Direto e pelo caminho publico — TLS aberto e redirect ligado.
  assert(credencialNaQuery("https://cdn.example.test/x#help?token=fixture") == 0);
  assert(credencialNaQuery("https://cdn.example.test/x?s=1#frag?token=fixture") == 0);
  assert(credencialNaQuery("https://cdn.example.test/x?token=fixture#frag") == 1);
  assert(credencialNaQuery("https://cdn.example.test/x") == 0);
  memset(&ultimo, 0, sizeof ultimo);
  free(rede_baixar("https://cdn.example.test/legendas/x.srt#help?token=fixture", 5));
  assert(ultimo.pPeer && ultimo.peer == 0 && ultimo.follow == 1);

  puts("rede_release_redirect: ok");
  return 0;
}
