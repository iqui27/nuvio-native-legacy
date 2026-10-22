// Regra das Conquistas (src/conquistasregra.c): niveis, arquivo por perfil, fila de avisos.
#include "../src/conquistasregra.h"
#include <stdio.h>
#include <string.h>

static int falhas;
static void ok(int c, const char *m) { printf("%s %s\n", c ? "ok   " : "FALHA", m); if (!c) falhas++; }

int main(void) {
  static CqEstado s, t;
  static char buf[4096];

  // Chave: o mesmo regex do servidor.
  ok(cqregra_chave_valida("trilha:oscar-melhor-filme") && cqregra_chave_valida("dir:nm0000229"),
     "chaves boas");
  ok(cqregra_chave_valida("top:trakt/250") && cqregra_chave_valida("ator:a.b_c-1"), "barra, ponto e traco");
  ok(!cqregra_chave_valida(NULL) && !cqregra_chave_valida("") && !cqregra_chave_valida("trilha:"),
     "NULL, vazia e sem corpo");
  ok(!cqregra_chave_valida("mov:nouvelle-vague") && !cqregra_chave_valida("trilha:a b") &&
     !cqregra_chave_valida("trilha:a|b") && !cqregra_chave_valida("trilha:a\nb"), "prefixo e caracteres fora");
  {
    char longa[200];
    memset(longa, 'a', sizeof longa);
    memcpy(longa, "dir:", 4);
    longa[4 + 80] = 0;
    ok(cqregra_chave_valida(longa), "80 caracteres ainda passa");
    longa[4 + 81] = 0; longa[4 + 80] = 'a';
    ok(!cqregra_chave_valida(longa), "81 nao passa");
  }

  // Niveis de uma trilha de 98 filmes.
  ok(cqregra_nivel(0, 98, 0) == 0 && cqregra_nivel(1, 98, 0) == 1, "0 vistos nada; 1 visto e o primeiro passo");
  ok(cqregra_nivel(48, 98, 0) == 1 && cqregra_nivel(49, 98, 0) == 2, "metade exata de 98 = 49");
  ok(cqregra_nivel(73, 98, 0) == 2 && cqregra_nivel(74, 98, 0) == 3, "75% de 98: 73,5 -> 74");
  ok(cqregra_nivel(97, 98, 0) == 3 && cqregra_nivel(98, 98, 0) == 4, "todos = nivel 4");
  ok(cqregra_nivel(98, 98, 1) == 5 && cqregra_nivel(60, 98, 1) == 2, "em ordem so vale com todos");
  ok(cqregra_nivel(120, 98, 0) == 4 && cqregra_nivel(5, 0, 1) == 0 && cqregra_nivel(5, -1, 0) == 0,
     "vistos acima do total fecha; total zero ou negativo nao da nivel");
  ok(cqregra_nivel(1, 1, 0) == 4, "trilha de um filme so");
  ok(cqregra_nivel_top(24) == 0 && cqregra_nivel_top(25) == 1 && cqregra_nivel_top(100) == 3 &&
     cqregra_nivel_top(250) == 4, "Top 250: 25/50/100/250");

  // Raridade.
  ok(cqregra_faixa(500) == 0 && cqregra_faixa(300) == 1 && cqregra_faixa(100) == 1 &&
     cqregra_faixa(99) == 2 && cqregra_faixa(10) == 2 && cqregra_faixa(9) == 3 && cqregra_faixa(0) == 3,
     "faixas nos cortes");

  // Registro: so sobe.
  memset(&s, 0, sizeof s);
  ok(cqregra_registrar(&s, "trilha:oscar", CQ_FAM_PREMIO, "Melhor Filme", 1, 1, 98, 1000) == 1, "primeiro nivel sobe");
  ok(cqregra_registrar(&s, "trilha:oscar", CQ_FAM_PREMIO, NULL, 1, 2, 98, 2000) == 0 && s.e[0].ganho == 1000,
     "mesmo nivel: nada novo, data intacta");
  ok(s.e[0].vistos == 2 && !strcmp(s.e[0].nome, "Melhor Filme"), "vistos acompanham; nome fica quando vem NULL");
  ok(cqregra_registrar(&s, "trilha:oscar", CQ_FAM_PREMIO, "", 0, 0, 98, 3000) == 0 && s.e[0].nivel == 1,
     "nivel nunca desce (historico apagado)");
  ok(cqregra_registrar(&s, "trilha:oscar", CQ_FAM_PREMIO, NULL, 9, 50, 98, 4000) == CQ_NIVEIS, "nivel limita em 5");
  ok(cqregra_registrar(&s, "xx:oscar", 0, "x", 1, 1, 1, 0) == -1 && s.n == 1, "chave ruim recusada");

  // Fila de avisos.
  ok(cqregra_pendentes(&s) == 1 && cqregra_proximo_aviso(&s) == 0, "subida fica pendente");
  cqregra_registrar(&s, "dir:nm1", CQ_FAM_DIRETOR, "Kenji", 2, 3, 6, 5000);
  ok(cqregra_pendentes(&s) == 2 && cqregra_proximo_aviso(&s) == 1, "dois pendentes; o mais recente primeiro");
  cqregra_marcar_avisados(&s);
  ok(cqregra_pendentes(&s) == 0 && cqregra_proximo_aviso(&s) == -1, "marcar avisados zera a fila");
  cqregra_registrar(&s, "dir:nm1", CQ_FAM_DIRETOR, NULL, 3, 5, 6, 6000);
  ok(cqregra_pendentes(&s) == 1 && s.e[1].avisado == 2, "nova subida depois de avisar volta a pendente");

  // Ida e volta pelo arquivo.
  ok(cqregra_serializar(&s, buf, sizeof buf) > 0, "serializa");
  ok(cqregra_ler(buf, &t) == 2 && !memcmp(&s.e[0], &t.e[0], sizeof s.e[0]) &&
     !memcmp(&s.e[1], &t.e[1], sizeof s.e[1]), "ida e volta identica");
  ok(cqregra_serializar(&s, buf, 20) == 0 && buf[0] == 0, "nao coube: 0 e vazio");

  // Lixo e linhas ruins.
  ok(cqregra_ler(NULL, &t) == 0 && cqregra_ler("", &t) == 0 && cqregra_ler("lixo\n\n|||\n", &t) == 0,
     "NULL, vazio e lixo");
  cqregra_ler("trilha:a|1|9|9|3|10|50|Boa\r\n"
              "xx:ruim|1|1|1|1|1|1|Nao\n"
              "trilha:a|2|1|0|1|1|1|Repetida\n"
              "trilha:b|99|2|7|-4|10|-9|Nome|com barra\n", &t);
  ok(t.n == 2 && t.e[0].nivel == CQ_NIVEIS && t.e[0].avisado == CQ_NIVEIS && !strcmp(t.e[0].nome, "Boa"),
     "CRLF, chave ruim e repetida ignoradas, nivel limitado");
  ok(t.e[1].familia == CQ_FAM_N - 1 && t.e[1].avisado == 2 && t.e[1].vistos == 0 && t.e[1].ganho == 0,
     "familia, aviso, vistos e data fora do intervalo");
  ok(!strcmp(t.e[1].nome, "Nomecom barra"), "barra extra no nome nao quebra a linha");

  // Nome com separador ou quebra de linha nao corrompe o arquivo.
  memset(&s, 0, sizeof s);
  cqregra_registrar(&s, "saga:x", CQ_FAM_TRILHA, "A|B\nC", 1, 1, 2, 1);
  cqregra_serializar(&s, buf, sizeof buf);
  ok(cqregra_ler(buf, &t) == 1 && !strcmp(t.e[0].nome, "ABC"), "| e quebra de linha saem do nome");
  {
    char longo[200];
    memset(longo, 0, sizeof longo);
    for (int i = 0; i < 40; i++) strcat(longo, "é");   // 80 bytes
    cqregra_registrar(&s, "saga:y", CQ_FAM_TRILHA, longo, 1, 1, 2, 1);
    ok(strlen(s.e[1].nome) <= CQ_NOME_MAX && strlen(s.e[1].nome) % 2 == 0, "corta sem partir UTF-8");
  }

  // Estado cheio.
  memset(&s, 0, sizeof s);
  for (int i = 0; i < CQ_MAX; i++) {
    char k[32];
    snprintf(k, sizeof k, "trilha:t%d", i);
    cqregra_registrar(&s, k, CQ_FAM_TRILHA, "t", 1, 1, 2, 1);
  }
  ok(s.n == CQ_MAX && cqregra_registrar(&s, "trilha:extra", CQ_FAM_TRILHA, "t", 1, 1, 2, 1) == -1,
     "cheio recusa chave nova");
  ok(cqregra_registrar(&s, "trilha:t0", CQ_FAM_TRILHA, NULL, 2, 2, 2, 9) == 2, "cheio ainda atualiza chave existente");

  printf("%s\n", falhas ? "FALHOU" : "conquistas: tudo ok");
  return falhas ? 1 : 0;
}
