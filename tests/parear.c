// Regras do codigo de 6 digitos do login (src/parear.c). bash tests/parear.sh
#include "parear.h"
#include <stdio.h>
#include <string.h>

static int falhas;
#define OK(c) do { if (!(c)) { printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #c); falhas++; } } while (0)

int main(void) {
  char d[32], cod[16], pag[64];
  int v = -7;

  OK(parear_codigo_valido("483921"));
  OK(parear_codigo_valido("000000"));
  OK(!parear_codigo_valido("48392"));
  OK(!parear_codigo_valido("4839210"));
  OK(!parear_codigo_valido("48392a"));
  OK(!parear_codigo_valido("483 921"));
  OK(!parear_codigo_valido(""));
  OK(!parear_codigo_valido(NULL));

  parear_formatar("483921", d, sizeof d); OK(!strcmp(d, "483 921"));
  parear_formatar("000042", d, sizeof d); OK(!strcmp(d, "000 042"));
  strcpy(d, "lixo"); parear_formatar("12345", d, sizeof d); OK(!d[0]);
  strcpy(d, "lixo"); parear_formatar(NULL, d, sizeof d); OK(!d[0]);
  parear_formatar("483921", d, 4); OK(strlen(d) < 4);          // nao estoura

  OK(parear_endereco_ok("https://conta.exemplo/tv-login?code=0123456789abcdef0123456789abcdef"));
  OK(parear_endereco_ok("http://localhost:8080/tv?code=a"));
  OK(!parear_endereco_ok("ftp://x/y"));
  OK(!parear_endereco_ok("https://"));
  OK(!parear_endereco_ok("https:///x"));
  OK(!parear_endereco_ok("https://a b"));
  OK(!parear_endereco_ok("https://a/\"x"));
  OK(!parear_endereco_ok(""));
  OK(!parear_endereco_ok(NULL));
  { char longo[600]; memset(longo, 'a', sizeof longo - 1); longo[sizeof longo - 1] = 0;
    memcpy(longo, "https://", 8); OK(!parear_endereco_ok(longo)); }

  OK(parear_ler_resposta(200, "{\"ok\":1,\"codigo\":\"483921\",\"validade\":600,\"pagina\":\"https://w.test/c\"}",
                         cod, sizeof cod, pag, sizeof pag, &v));
  OK(!strcmp(cod, "483921") && !strcmp(pag, "https://w.test/c") && v == 600);
  // Nada e escrito quando a resposta nao serve.
  strcpy(cod, "antes"); v = -7;
  OK(!parear_ler_resposta(200, "{\"codigo\":\"48392\",\"validade\":600,\"pagina\":\"https://w.test/c\"}", cod, sizeof cod, pag, sizeof pag, &v));
  OK(!strcmp(cod, "antes") && v == -7);
  OK(!parear_ler_resposta(200, "{\"codigo\":\"483921\",\"validade\":0,\"pagina\":\"https://w.test/c\"}", cod, sizeof cod, pag, sizeof pag, &v));
  OK(!parear_ler_resposta(200, "{\"codigo\":\"483921\",\"validade\":600}", cod, sizeof cod, pag, sizeof pag, &v));
  // validade fora da faixa ou nao inteira: recusada ANTES do cast para int.
  OK(!parear_ler_resposta(200, "{\"codigo\":\"483921\",\"validade\":1e300,\"pagina\":\"https://w.test/c\"}", cod, sizeof cod, pag, sizeof pag, &v));
  OK(!parear_ler_resposta(200, "{\"codigo\":\"483921\",\"validade\":-1e300,\"pagina\":\"https://w.test/c\"}", cod, sizeof cod, pag, sizeof pag, &v));
  OK(!parear_ler_resposta(200, "{\"codigo\":\"483921\",\"validade\":3601,\"pagina\":\"https://w.test/c\"}", cod, sizeof cod, pag, sizeof pag, &v));
  OK(!parear_ler_resposta(200, "{\"codigo\":\"483921\",\"validade\":0.5,\"pagina\":\"https://w.test/c\"}", cod, sizeof cod, pag, sizeof pag, &v));
  OK(!parear_ler_resposta(200, "{\"codigo\":\"483921\",\"validade\":59.5,\"pagina\":\"https://w.test/c\"}", cod, sizeof cod, pag, sizeof pag, &v));
  OK(!parear_ler_resposta(200, "{\"codigo\":\"4839210\",\"validade\":600,\"pagina\":\"https://w.test/c\"}", cod, sizeof cod, pag, sizeof pag, &v));
  OK(parear_ler_resposta(200, "{\"codigo\":\"483921\",\"validade\":3600,\"pagina\":\"https://w.test/c\"}", cod, sizeof cod, pag, sizeof pag, &v) && v == 3600);
  OK(!parear_ler_resposta(200, "{\"codigo\":\"483921\",\"validade\":600,\"pagina\":\"javascript:x\"}", cod, sizeof cod, pag, sizeof pag, &v));
  OK(!parear_ler_resposta(200, "{\"codigo\":\"483921\",\"validade\":600,\"pagina\":\"https://w.test/c\"}", cod, 6, pag, sizeof pag, &v));
  OK(!parear_ler_resposta(200, "{\"codigo\":\"483921\",\"validade\":600,\"pagina\":\"https://w.test/c\"}", cod, sizeof cod, pag, 8, &v));
  OK(!parear_ler_resposta(503, "{\"erro\":\"parear desligado\"}", cod, sizeof cod, pag, sizeof pag, &v));
  OK(!parear_ler_resposta(200, "<html>", cod, sizeof cod, pag, sizeof pag, &v));
  OK(!parear_ler_resposta(0, NULL, cod, sizeof cod, pag, sizeof pag, &v));
  OK(parear_ler_resposta(200, "{\"codigo\":\"000001\",\"validade\":45,\"pagina\":\"https://w.test/c\"}", cod, sizeof cod, pag, sizeof pag, NULL));

  parear_pagina_curta("https://nuvio.exemplo/c", d, sizeof d);  OK(!strcmp(d, "nuvio.exemplo/c"));
  parear_pagina_curta("https://www.nuvio.exemplo/c/", d, sizeof d); OK(!strcmp(d, "nuvio.exemplo/c"));
  parear_pagina_curta("http://localhost:8799/c", d, sizeof d);  OK(!strcmp(d, "localhost:8799/c"));
  parear_pagina_curta(NULL, d, sizeof d); OK(!d[0]);

  // Servidor sem a rota ou com o recurso desligado: nao insiste.
  OK(parear_espera(404, 1) == -1);
  OK(parear_espera(400, 1) == -1);
  OK(parear_espera(503, 1) == -1);
  // Rede fora ou 5xx passageiro: tenta de novo, com folga crescente, ate 3.
  OK(parear_espera(0, 1) == 5);
  OK(parear_espera(500, 2) == 20);
  OK(parear_espera(0, 3) == -1);
  OK(parear_espera(429, 1) == 60);
  OK(parear_espera(200, 1) == 5);

  OK(!parear_vencendo(1000, 1600));
  OK(!parear_vencendo(1570, 1600));
  OK(parear_vencendo(1571, 1600));
  OK(parear_vencendo(1700, 1600));

  if (falhas) { printf("%d falha(s)\n", falhas); return 1; }
  printf("parear: ok\n");
  return 0;
}
