// A regra da Sessao: entrada, voto, unanimidade, maioria e o que o visitante
// consegue (ou nao) fazer com texto malformado. Ver src/sala.h.
#include "sala.h"
#include <stdio.h>
#include <string.h>

static int falhas;
#define CONFERE(c) do { if (!(c)) { printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #c); falhas++; } } while (0)

#define ANA "aaaa11111111"
#define BIA "bbbb22222222"
#define CAU "cccc33333333"

static int voto(Sala *s, const char *quem, int c, int sim) {
  char m[64];
  snprintf(m, sizeof m, "v:%s:%d:%d", quem, c, sim);
  return sala_mensagem(s, m);
}

int main(void) {
  Sala s;
  char j[4096];

  // ENTRADA
  sala_iniciar(&s, 5);
  CONFERE(!sala_comecar(&s));                       // sala vazia nao comeca
  CONFERE(sala_mensagem(&s, "j:" ANA ":Ana") == SALA_OK);
  CONFERE(sala_mensagem(&s, "j:" ANA ":Outro nome") == SALA_IGUAL);
  CONFERE(!strcmp(s.g[0].nome, "Ana"));
  CONFERE(sala_mensagem(&s, "j:aaaa99999999:Impostor") == SALA_ESTRANHO);  // id publico ja usado
  CONFERE(voto(&s, ANA, 0, 1) == SALA_FORA_DE_HORA); // ninguem vota antes de comecar
  CONFERE(sala_mensagem(&s, "j:" BIA ":  Bia  ") == SALA_OK);
  CONFERE(!strcmp(s.g[1].nome, "Bia"));
  CONFERE(s.nGente == 2);

  // TEXTO MALFORMADO
  CONFERE(sala_mensagem(&s, NULL) == SALA_INVALIDO);
  CONFERE(sala_mensagem(&s, "") == SALA_INVALIDO);
  CONFERE(sala_mensagem(&s, "j:AAAA11111111:x") == SALA_INVALIDO);   // so hex minusculo
  CONFERE(sala_mensagem(&s, "j:aaaa1111111:xx") == SALA_INVALIDO);
  CONFERE(sala_mensagem(&s, "x:" ANA ":1:1") == SALA_INVALIDO);
  CONFERE(sala_comecar(&s));
  CONFERE(voto(&s, ANA, 5, 1) == SALA_INVALIDO);     // cartao fora do baralho
  CONFERE(sala_mensagem(&s, "v:" ANA ":1:2") == SALA_INVALIDO);
  CONFERE(sala_mensagem(&s, "v:" ANA ":1:1x") == SALA_INVALIDO);
  CONFERE(sala_mensagem(&s, "v:" ANA ":-1:1") == SALA_INVALIDO);
  CONFERE(sala_mensagem(&s, "v:" ANA "::1") == SALA_INVALIDO);
  CONFERE(sala_mensagem(&s, "v:" ANA ":4294967296:1") == SALA_INVALIDO);
  CONFERE(voto(&s, CAU, 0, 1) == SALA_ESTRANHO);     // quem nao entrou nao vota
  CONFERE(sala_mensagem(&s, "v:aaaa00000000:0:1") == SALA_ESTRANHO);  // segredo errado
  CONFERE(s.fase == SALA_VOTANDO && s.g[0].visto == 0);

  // O PRIMEIRO EM QUE TODOS DISSERAM SIM (no tempo, nao no indice)
  CONFERE(voto(&s, ANA, 0, 0) == SALA_OK);
  CONFERE(voto(&s, ANA, 0, 1) == SALA_IGUAL);        // o primeiro voto vale
  CONFERE(sala_sins(&s, 0) == 0);
  CONFERE(voto(&s, ANA, 3, 1) == SALA_OK);
  CONFERE(voto(&s, ANA, 1, 1) == SALA_OK);
  CONFERE(s.fase == SALA_VOTANDO);
  CONFERE(voto(&s, BIA, 0, 1) == SALA_OK);
  CONFERE(voto(&s, BIA, 3, 1) == SALA_OK);           // 3 fecha antes do 1
  CONFERE(s.fase == SALA_DECIDIDO && s.vence == 3 && s.unanime == 1);
  CONFERE(voto(&s, BIA, 1, 1) == SALA_FORA_DE_HORA);
  CONFERE(sala_mensagem(&s, "j:" CAU ":Cau") == SALA_FORA_DE_HORA);

  // ATRASADO CONTA PARA A UNANIMIDADE
  sala_iniciar(&s, 3);
  sala_mensagem(&s, "j:" ANA ":Ana");
  sala_comecar(&s);
  CONFERE(sala_mensagem(&s, "j:" BIA ":Bia") == SALA_OK);
  CONFERE(voto(&s, ANA, 0, 1) == SALA_OK);
  CONFERE(s.fase == SALA_VOTANDO);                   // Bia ainda nao disse sim
  CONFERE(voto(&s, BIA, 0, 1) == SALA_OK);
  CONFERE(s.fase == SALA_DECIDIDO && s.vence == 0 && s.unanime);

  // SEM UNANIMIDADE: MAIORIA, EMPATE NO MENOR INDICE
  sala_iniciar(&s, 3);
  sala_mensagem(&s, "j:" ANA ":Ana"); sala_mensagem(&s, "j:" BIA ":Bia"); sala_mensagem(&s, "j:" CAU ":Cau");
  sala_comecar(&s);
  voto(&s, ANA, 0, 0); voto(&s, ANA, 1, 1); voto(&s, ANA, 2, 1);
  voto(&s, BIA, 0, 1); voto(&s, BIA, 1, 1); voto(&s, BIA, 2, 0);
  voto(&s, CAU, 0, 0); voto(&s, CAU, 1, 0);
  CONFERE(s.fase == SALA_VOTANDO);
  CONFERE(voto(&s, CAU, 2, 1) == SALA_OK);           // todos viram tudo
  CONFERE(s.fase == SALA_DECIDIDO && s.vence == 1 && s.unanime == 0);
  CONFERE(sala_votos(&s, 2) == 3 && sala_sins(&s, 1) == 2 && sala_sins(&s, 2) == 2);

  // NINGUEM QUIS NADA
  sala_iniciar(&s, 2);
  sala_mensagem(&s, "j:" ANA ":Ana");
  sala_comecar(&s);
  voto(&s, ANA, 0, 0); voto(&s, ANA, 1, 0);
  CONFERE(s.fase == SALA_SEM_ACORDO && s.vence == -1);

  // A TV ENCERRA COM OS VOTOS DE AGORA
  sala_iniciar(&s, 4);
  sala_mensagem(&s, "j:" ANA ":Ana"); sala_mensagem(&s, "j:" BIA ":Bia");
  sala_encerrar(&s);
  CONFERE(s.fase == SALA_ENTRANDO);                  // so vale votando
  sala_comecar(&s);
  voto(&s, ANA, 2, 1);
  sala_encerrar(&s);
  CONFERE(s.fase == SALA_DECIDIDO && s.vence == 2 && !s.unanime);

  // SALA CHEIA, BARALHO LIMITADO
  sala_iniciar(&s, 99);
  CONFERE(s.nCartas == SALA_CARTAS_MAX);
  { int i; char m[64];
    for (i = 0; i < SALA_GENTE_MAX; i++) {
      snprintf(m, sizeof m, "j:%04x%08x:P%d", i, i, i);
      CONFERE(sala_mensagem(&s, m) == SALA_OK);
    }
    CONFERE(sala_mensagem(&s, "j:ffff00000000:Sobra") == SALA_CHEIA); }
  sala_comecar(&s);
  CONFERE(sala_mensagem(&s, "v:000000000000:23:1") == SALA_OK);
  CONFERE(s.g[0].sim == 1u << 23);

  // NOME DE VISITANTE: tamanho, controles, UTF-8 e o que vai para a pagina
  sala_iniciar(&s, 1);
  CONFERE(sala_mensagem(&s, "j:" ANA ":") == SALA_OK);
  CONFERE(!strcmp(s.g[0].nome, "?"));
  CONFERE(sala_mensagem(&s, "j:" BIA ":Jo\xC3\xA3o\x01\n</script><b>\"\\ 0123456789 abcdef") == SALA_OK);
  CONFERE(strlen(s.g[1].nome) <= SALA_NOME_MAX);
  CONFERE(!strchr(s.g[1].nome, '\n') && !strchr(s.g[1].nome, 1));
  CONFERE(sala_mensagem(&s, "j:" CAU ":aaaaaaaaaaaaaaaaaaaaaaa\xC3\xA3zz") == SALA_OK);
  CONFERE(!strcmp(s.g[2].nome, "aaaaaaaaaaaaaaaaaaaaaaa")); // nao parte o caractere de 2 bytes
  CONFERE(sala_mensagem(&s, "j:dddd44444444:\xFF\xC3") == SALA_OK);
  CONFERE(!strcmp(s.g[3].nome, "??"));
  CONFERE(sala_estado_json(&s, j, sizeof j) > 0);
  CONFERE(!strchr(j, '<') && !strchr(j, '>') && !strchr(j, '\n'));
  CONFERE(strstr(j, "\"i\":\"aaaa\"") && !strstr(j, "11111111")); // id publico sim, segredo nunca
  CONFERE(strstr(j, "{\"f\":0,\"n\":1,\"v\":-1,\"u\":0,\"g\":[") == j);
  CONFERE(strstr(j, "Jo\xC3\xA3o\\u003c/script\\u003e"));
  CONFERE(sala_estado_json(&s, j, 40) == 0);          // nao coube: nada pela metade
  { char t[64];
    CONFERE(sala_json_texto(t, sizeof t, "a\"b\\c") == 15 && !strcmp(t, "a\\u0022b\\u005cc"));
    CONFERE(sala_json_texto(t, 4, "abcd") == 0);
    CONFERE(sala_json_texto(t, 5, "abcd") == 4); }

  if (falhas) { printf("sala: %d falha(s)\n", falhas); return 1; }
  puts("sala: OK");
  return 0;
}
