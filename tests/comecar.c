// Regra do Começar (src/comecarregra.c): quando aparece, o que fica gravado.
#include "../src/comecarregra.h"
#include <stdio.h>
#include <string.h>

static int falhas;
static void ok(int c, const char *m) { printf("%s %s\n", c ? "ok   " : "FALHA", m); if (!c) falhas++; }

int main(void) {
  ComecarSalvo s;
  char buf[256];

  comecarregra_ler(NULL, &s);
  ok(s.estado == COMECAR_ESTADO_NENHUM && s.cor == 0 && !s.nome[0], "NULL cai no padrao");
  ok(comecarregra_decidir(&s, 1, 0) == COMECAR_COMPLETO, "instalacao limpa, login novo: 4 passos");
  ok(comecarregra_decidir(&s, 0, 0) == COMECAR_NADA, "sessao restaurada, nada gravado, sem dados: nada (sem so-tour)");

  // Upgrade: a instalacao ja tinha sessao/perfis/progresso e nao ha comecar.txt.
  ok(comecarregra_decidir(&s, 0, 1) == COMECAR_NADA, "upgrade com dados, sessao restaurada: nunca");
  ok(comecarregra_decidir(&s, 1, 1) == COMECAR_NADA, "upgrade com dados, login novo na mesma instalacao: nunca");

  comecarregra_ler("estado=feito\nnome=Ana\n", &s);
  ok(comecarregra_decidir(&s, 1, 0) == COMECAR_NADA && comecarregra_decidir(&s, 0, 0) == COMECAR_NADA,
     "feito: nunca mais, nem com login novo");

  // "andamento" so existia em builds de teste: nao e mais estado, cai em nenhum.
  comecarregra_ler("estado=andamento\nnome=Marina\ncor=3\ninfantil=1\n", &s);
  ok(s.estado == COMECAR_ESTADO_NENHUM && !strcmp(s.nome, "Marina") && s.cor == 3, "andamento antigo vira nenhum; nome e cor lidos");

  comecarregra_ler("estado=lixo\ncor=99\ncor=-1\n", &s);
  ok(s.estado == COMECAR_ESTADO_NENHUM && s.cor == 0, "estado e cor fora do intervalo caem no padrao");

  comecarregra_ler("estado=feito\r\nnome=Zé\r\ncor=2\r\n", &s);
  ok(s.estado == COMECAR_ESTADO_FEITO && !strcmp(s.nome, "Zé") && s.cor == 2, "CRLF lido");

  // Ida e volta; o arquivo nao fala mais em perfil infantil.
  memset(&s, 0, sizeof s);
  s.estado = COMECAR_ESTADO_FEITO; s.cor = 4;
  snprintf(s.nome, sizeof s.nome, "Lia");
  ok(comecarregra_serializar(&s, buf, sizeof buf) > 0, "serializa");
  ok(!strstr(buf, "infantil") && !strstr(buf, "andamento"), "arquivo sem infantil nem andamento");
  { ComecarSalvo t; comecarregra_ler(buf, &t);
    ok(t.estado == s.estado && t.cor == 4 && !strcmp(t.nome, "Lia"), "ida e volta"); }
  ok(comecarregra_serializar(&s, buf, 8) == 0, "buffer pequeno devolve 0");

  // Nome com quebra de linha nao injeta chave no arquivo.
  s.estado = COMECAR_ESTADO_NENHUM;
  snprintf(s.nome, sizeof s.nome, "A\nestado=feito");
  comecarregra_serializar(&s, buf, sizeof buf);
  { ComecarSalvo t; comecarregra_ler(buf, &t);
    ok(t.estado == COMECAR_ESTADO_NENHUM, "nome com LF nao forja estado=feito"); }

  // Corte de UTF-8: 'é' tem 2 bytes; 31 'a' + 'é' passa de 32 bytes.
  { char in[80], out[COMECAR_NOME_MAX + 1]; int n, i;
    for (i = 0; i < 31; i++) in[i] = 'a';
    in[31] = (char)0xC3; in[32] = (char)0xA9; in[33] = 0;
    n = comecarregra_nome_limpo(in, out, sizeof out);
    ok(n == 31 && out[30] == 'a', "nao parte caractere UTF-8 ao meio");
    for (i = 0; i < 70; i++) in[i] = 'b';
    in[70] = 0;
    n = comecarregra_nome_limpo(in, out, sizeof out);
    ok(n == COMECAR_NOME_MAX, "corta em COMECAR_NOME_MAX"); }

  if (falhas) { printf("comecar: %d falha(s)\n", falhas); return 1; }
  return 0;
}
