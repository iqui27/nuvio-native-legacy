// "DIGITAR PELO CELULAR" (celular.h): o servidor de verdade, falado por um
// cliente HTTP de verdade (curl), em 127.0.0.1 e no IP da LAN que o QR mostra.
// Prova as regras de seguranca uma a uma: token errado, Host/Origin de fora,
// metodo, corpo grande, uso unico, expiracao, bloqueio por tentativas.
// O texto de teste NAO e chave de ninguem.
#include "celular.h"
#include "posterprov.h"
#include "nvtmp.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Dublês: so o que celular.c usa de fora.
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
  snprintf(cmd, sizeof cmd, "curl -s -m 5 -o '%s/nv-celular-corpo.txt' -w '%%{http_code}' %s", nvt_dir(), args);
  f = popen(cmd, "r");
  if (!f) return -1;
  if (fscanf(f, "%d", &cod) != 1) cod = 0;
  pclose(f);
  f = fopen(nvt("/nv-celular-corpo.txt"), "r");
  saida[0] = 0;
  if (f) { n = fread(saida, 1, sizeof saida - 1, f); saida[n] = 0; fclose(f); }
  return cod;
}

int main(void) {
  char a[512], txt[8192], local[160], tok[16];
  const char *u, *barra;
  int p, i;

  assert(celular_disponivel());
  assert(celular_abrir("Chave do Seekr"));
  u = celular_url();
  p = celular_porta();
  assert(!strncmp(u, "http://", 7) && p > 0);
  barra = strrchr(u, '/');
  snprintf(tok, sizeof tok, "%s", barra + 1);
  assert(strlen(tok) == 8);
  snprintf(local, sizeof local, "http://127.0.0.1:%d/%s", p, tok);
  printf("url no QR: http://<ip-lan>:%d/<token> (%zu caracteres)\n", p, strlen(u));

  // Pagina pelo IP DA LAN (o caminho do celular) e por 127.0.0.1.
  snprintf(a, sizeof a, "'%s'", u);
  assert(curl(a) == 200 && strstr(saida, "<textarea") && strstr(saida, "Chave do Seekr"));
  assert(strstr(saida, "lang='pt-BR'") || strstr(saida, "lang='pt"));
  // O maxlength da textarea segue o teto do modelo de URL (PP_MODELO_MAX):
  // modelos longos colados pelo celular nao podem ser cortados pelo navegador.
  { char ml[40]; snprintf(ml, sizeof ml, "maxlength=%d", PP_MODELO_MAX - 1);
    assert(strstr(saida, ml)); }
  snprintf(a, sizeof a, "-I '%s'", local);
  assert(curl(a) == 200);
  // Sem CORS: nenhum Access-Control na resposta.
  snprintf(a, sizeof a, "-D - '%s'", local);
  { char cmd[600]; FILE *f; char l[512]; int cors = 0;
    snprintf(cmd, sizeof cmd, "curl -s -D - -o /dev/null '%s'", local);
    f = popen(cmd, "r");
    while (f && fgets(l, sizeof l, f)) if (strcasestr(l, "access-control")) cors = 1;
    if (f) pclose(f);
    assert(!cors); }

  // Token errado: 404. Host de fora (rebinding): 403. Origin de fora: 403.
  snprintf(a, sizeof a, "'http://127.0.0.1:%d/aaaaaaaa'", p);
  assert(curl(a) == 404);
  snprintf(a, sizeof a, "-H 'Host: evil.example:%d' '%s'", p, local);
  assert(curl(a) == 403);
  snprintf(a, sizeof a, "-H 'Origin: http://evil.example' --data-urlencode 't=x' '%s'", local);
  assert(curl(a) == 403);
  // A pagina tem de pedir ao navegador o Origin de verdade no POST: com
  // Referrer-Policy no-referrer o Chrome manda "Origin: null" (recusado abaixo)
  // e o envio do formulario falhava.
  { char cmd[600]; FILE *f; char l[512]; int ok = 0;
    snprintf(cmd, sizeof cmd, "curl -s -D - -o /dev/null '%s'", local);
    f = popen(cmd, "r");
    while (f && fgets(l, sizeof l, f)) if (strcasestr(l, "referrer-policy: same-origin")) ok = 1;
    if (f) pclose(f);
    assert(ok); }
  snprintf(a, sizeof a, "-H 'Origin: null' --data-urlencode 't=x' '%s'", local);
  assert(curl(a) == 403);
  // Metodo: so GET/HEAD/POST.
  snprintf(a, sizeof a, "-X PUT '%s'", local);
  assert(curl(a) == 405);
  // Corpo acima do teto: 413, e o token continua valido.
  snprintf(a, sizeof a, "-H 'Content-Type: application/x-www-form-urlencoded' --data-binary @'%s/nv-celular-grande.txt' '%s'", nvt_dir(), local);
  { FILE *g = fopen(nvt("/nv-celular-grande.txt"), "w"); fputs("t=", g);
    for (i = 0; i < CEL_CORPO_MAX * 3 + 10; i++) fputc('a', g); fclose(g); }
  assert(curl(a) == 413);
  assert(celular_estado() == CEL_ESPERANDO && !celular_pegar(txt, sizeof txt));

  // O ENVIO: texto de teste com espaco nas pontas e quebra de linha.
  snprintf(a, sizeof a, "-H 'Origin: http://127.0.0.1:%d' --data-urlencode 't=  TEXTO_de-TESTE_123 nao-e-chave  \n' '%s'", p, u);
  assert(curl(a) == 200 && strstr(saida, "Enviado"));
  assert(celular_pegar(txt, sizeof txt));
  assert(!strcmp(txt, "TEXTO_de-TESTE_123 nao-e-chave"));
  assert(!celular_pegar(txt, sizeof txt));   // consumido na leitura
  assert(celular_estado() == CEL_RECEBIDO && !celular_url()[0]);
  // USO UNICO: o servidor ja fechou.
  usleep(100 * 1000);
  snprintf(a, sizeof a, "--data-urlencode 't=segundo' '%s'", local);
  assert(curl(a) == 0);
  celular_fechar();
  assert(celular_estado() == CEL_PARADO);

  // Token novo a cada abertura; fechar derruba o servidor na hora.
  assert(celular_abrir("Busca"));
  assert(strcmp(strrchr(celular_url(), '/') + 1, tok));
  p = celular_porta();
  celular_fechar();
  snprintf(a, sizeof a, "'http://127.0.0.1:%d/'", p);
  assert(curl(a) == 0);

  // BLOQUEIO: CEL_ERROS_MAX caminhos errados fecham o servidor.
  assert(celular_abrir("Busca"));
  p = celular_porta();
  for (i = 0; i < CEL_ERROS_MAX; i++) {
    snprintf(a, sizeof a, "'http://127.0.0.1:%d/chute%03d'", p, i);
    assert(curl(a) == 404);
  }
  usleep(100 * 1000);
  assert(celular_estado() == CEL_FALHOU);
  celular_fechar();

  // Envio vazio nao conta: o token continua valendo.
  assert(celular_abrir("Busca"));
  snprintf(local, sizeof local, "http://127.0.0.1:%d%s", celular_porta(), strrchr(celular_url(), '/'));
  snprintf(a, sizeof a, "--data-urlencode 't=   ' '%s'", local);
  assert(curl(a) == 200 && strstr(saida, "<textarea"));
  assert(celular_estado() == CEL_ESPERANDO && !celular_pegar(txt, sizeof txt));
  // UTF-8 inteiro (busca do guia aceita acento no alfabeto do campo? o filtro e
  // do teclado.c; aqui o servidor entrega o que veio).
  snprintf(a, sizeof a, "--data-urlencode 't=ação' '%s'", local);
  assert(curl(a) == 200);
  assert(celular_pegar(txt, sizeof txt) && !strcmp(txt, "ação"));
  celular_fechar();

  puts("PASS: celular (servidor, token, Host/Origin, metodo, teto, uso unico, bloqueio)");
  return 0;
}
