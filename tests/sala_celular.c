// A SESSAO PELA REDE LOCAL: o servidor de celular.h em modo sessao com as
// opcoes da sala (validade propria, cartazes https) e a regra de sala.h,
// falados por um cliente HTTP de verdade (curl). Ver src/salaui.h.
#include "celular.h"
#include "sala.h"
#include "nvtmp.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

const char *i18n(const char *s) { return s; }
int ajustes_idioma(void) { return 0; }
void ajustes_acento(float *r, float *g, float *b) { *r = 0.96f; *g = 0.55f; *b = 0.2f; }
int ajustes_tinta_foco(void) { return 18; }

static char saida[16384];
static int curl(const char *args) {
  char cmd[2048];
  FILE *f;
  size_t n;
  int cod = 0;
  snprintf(cmd, sizeof cmd, "curl -s -m 5 -D '%s/nv-sala-cab.txt' -o '%s/nv-sala-corpo.txt' -w '%%{http_code}' %s",
           nvt_dir(), nvt_dir(), args);
  f = popen(cmd, "r");
  if (!f) return -1;
  if (fscanf(f, "%d", &cod) != 1) cod = 0;
  pclose(f);
  f = fopen(nvt("/nv-sala-corpo.txt"), "r");
  saida[0] = 0;
  if (f) { n = fread(saida, 1, sizeof saida - 1, f); saida[n] = 0; fclose(f); }
  return cod;
}
static int cabecalhoTem(const char *s) {
  char cab[4096];
  size_t n;
  FILE *f = fopen(nvt("/nv-sala-cab.txt"), "r");
  if (!f) return 0;
  n = fread(cab, 1, sizeof cab - 1, f); cab[n] = 0; fclose(f);
  return strstr(cab, s) != NULL;
}

static Sala sala;
static char local[160];
// O que salaui.c faz a cada quadro: um pedido do servidor para a regra.
static int quadro(void) {
  char m[160], j[2048];
  int r;
  if (!celular_pegar(m, sizeof m)) return -1;
  r = sala_mensagem(&sala, m);
  if (r == SALA_OK && sala_estado_json(&sala, j, sizeof j)) celular_status(j);
  return r;
}
static int manda(const char *msg) {
  char a[512];
  snprintf(a, sizeof a, "--data-urlencode 't=%s' '%s'", msg, local);
  return curl(a);
}
static const char *estado(void) {
  char a[256];
  snprintf(a, sizeof a, "'%s/s'", local);
  assert(curl(a) == 200);
  return saida;
}

int main(void) {
  char a[512], j[2048];
  const char *tok;

  sala_iniciar(&sala, 3);
  celular_sessao_opcoes(1800, 1);
  assert(celular_abrir_sessao("Nuvio", "<html><h1>sala</h1></html>"));
  assert(sala_estado_json(&sala, j, sizeof j)); celular_status(j);
  tok = strrchr(celular_url(), '/') + 1;
  snprintf(local, sizeof local, "http://127.0.0.1:%d/%s", celular_porta(), tok);

  // A pagina da sala pode carregar cartazes https, e so isso de fora.
  snprintf(a, sizeof a, "'%s'", local);
  assert(curl(a) == 200 && strstr(saida, "sala"));
  assert(cabecalhoTem("img-src https:; ") && cabecalhoTem("default-src 'none'") && cabecalhoTem("connect-src 'self'"));
  assert(cabecalhoTem("Referrer-Policy: same-origin"));
  assert(strstr(estado(), "{\"f\":0,\"n\":3,\"v\":-1,\"u\":0,\"g\":[]}"));

  // Dois visitantes entram; um terceiro pedido antes de a TV ler espera (409).
  assert(manda("j:aaaa11111111:Ana") == 200);
  assert(manda("j:bbbb22222222:Bia") == 409);
  assert(quadro() == SALA_OK);
  assert(manda("j:bbbb22222222:Bia <script>") == 200 && quadro() == SALA_OK);
  assert(quadro() == -1);
  assert(strstr(estado(), "\"i\":\"aaaa\",\"n\":\"Ana\"") && strstr(saida, "\\u003cscript\\u003e"));
  assert(!strstr(saida, "11111111") && !strstr(saida, "22222222") && !strchr(saida, '<'));

  // Votar antes de a TV comecar, com segredo errado, ou lixo: nada muda.
  assert(manda("v:aaaa11111111:0:1") == 200 && quadro() == SALA_FORA_DE_HORA);
  assert(sala_comecar(&sala));
  assert(manda("v:aaaa00000000:0:1") == 200 && quadro() == SALA_ESTRANHO);
  assert(manda("oi") == 200 && quadro() == SALA_INVALIDO);
  assert(sala.g[0].visto == 0);

  // O primeiro titulo com sim dos dois encerra, e todos os celulares leem isso.
  assert(manda("v:aaaa11111111:0:1") == 200 && quadro() == SALA_OK);
  assert(manda("v:bbbb22222222:0:0") == 200 && quadro() == SALA_OK);
  assert(manda("v:bbbb22222222:2:1") == 200 && quadro() == SALA_OK);
  assert(strstr(estado(), "\"f\":1,") && strstr(saida, "\"i\":\"bbbb\"") && strstr(saida, "\"k\":2}"));
  assert(manda("v:aaaa11111111:2:1") == 200 && quadro() == SALA_OK);
  assert(sala.fase == SALA_DECIDIDO && sala.vence == 2 && sala.unanime);
  assert(strstr(estado(), "{\"f\":2,\"n\":3,\"v\":2,\"u\":1,"));
  assert(celular_url()[0]);   // a sala continua de pe depois dos pedidos

  // Fechou: nada responde; e a proxima abertura comum NAO herda os cartazes.
  celular_fechar();
  snprintf(a, sizeof a, "'%s/s'", local);
  assert(curl(a) != 200);
  assert(celular_abrir_sessao("Guia TV", "<html>guia</html>"));
  tok = strrchr(celular_url(), '/') + 1;
  snprintf(a, sizeof a, "'http://127.0.0.1:%d/%s'", celular_porta(), tok);
  assert(curl(a) == 200 && !cabecalhoTem("img-src"));
  celular_fechar();
  assert(celular_abrir("Busca"));
  tok = strrchr(celular_url(), '/') + 1;
  snprintf(a, sizeof a, "'http://127.0.0.1:%d/%s'", celular_porta(), tok);
  assert(curl(a) == 200 && !cabecalhoTem("img-src"));
  celular_fechar();
  puts("PASS: sala_celular (CSP da sala, entrada, 409, votos, unanimidade, estado sem segredo)");
  return 0;
}
