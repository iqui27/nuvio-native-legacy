// "O que eu perdi?": corte sem spoiler, pedido do Gemini e leitura da resposta.
#include "perdi.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static LegendaCue cue(double ini, const char *t) {
  LegendaCue c;
  memset(&c, 0, sizeof c);
  c.inicio = ini; c.fim = ini + 2.0;
  snprintf(c.texto, sizeof c.texto, "%s", t);
  return c;
}

int main(void) {
  char k[64], r[PERDI_RESUMO_MAX];
  int falas = 0;

  // Chave: so o alfabeto das chaves do Google; nada de quebra de linha.
  assert(perdi_chave_limpar(" AIzaSy-Ab_12\r\nX-Evil: 1 ", k, sizeof k) == 19);
  assert(!strcmp(k, "AIzaSy-Ab_12X-Evil1"));
  assert(perdi_chave_limpar(NULL, k, sizeof k) == 0 && !k[0]);

  // Corte: o que comeca depois do instante atual NAO entra.
  LegendaCue v[6] = {
    cue(10, "Bom dia,\nAna."), cue(20, "Bom dia,\nAna."), cue(30, "Cadê o Rui?"),
    cue(95, "Ele saiu ontem."), cue(100.5, "O ASSASSINO É O MORDOMO"), cue(4000, "Fim.")
  };
  char *t = perdi_trecho(v, 6, 100.0, 0, PERDI_TRECHO_MAX, &falas);
  assert(t && falas == 3);
  assert(!strcmp(t, "[0:00:10]\nBom dia, Ana.\nCadê o Rui?\n[0:01:35]\nEle saiu ontem.\n"));
  assert(!strstr(t, "MORDOMO") && !strstr(t, "Fim."));
  free(t);
  // O atraso da legenda anda junto com o que esta na tela.
  t = perdi_trecho(v, 6, 100.0, 600, PERDI_TRECHO_MAX, &falas);
  assert(t && falas == 4 && strstr(t, "MORDOMO") && !strstr(t, "Fim."));
  free(t);
  t = perdi_trecho(v, 6, 100.0, -6000, PERDI_TRECHO_MAX, &falas);
  assert(t && falas == 2 && !strstr(t, "saiu"));
  free(t);
  assert(!perdi_trecho(v, 6, 5.0, 0, PERDI_TRECHO_MAX, &falas) && falas == 0);
  assert(!perdi_trecho(NULL, 0, 5.0, 0, PERDI_TRECHO_MAX, NULL));
  // Teto: cai o comeco, em fronteira de linha; o recente fica.
  t = perdi_trecho(v, 6, 100.0, 0, 40, &falas);
  assert(t && strlen(t) <= 40 && strstr(t, "Ele saiu ontem.") && !strstr(t, "Bom dia"));
  assert(t[0] != '\n' && t[strlen(t) - 1] == '\n');
  free(t);

  // Pedido: JSON com escape, sem a chave e sem o que vem depois.
  t = perdi_trecho(v, 6, 100.0, 0, PERDI_TRECHO_MAX, NULL);
  char *p = perdi_pedido("A \"Casa\"", "T1E2", 100.0, "pt-BR", t);
  assert(p && strstr(p, "\"systemInstruction\"") && strstr(p, "\"contents\"") && strstr(p, "pt-BR"));
  assert(strstr(p, "A \\\"Casa\\\" - T1E2") && strstr(p, "0:01:40") && strstr(p, "Ele saiu ontem.\\n"));
  assert(!strstr(p, "MORDOMO") && !strstr(p, "AIza") && !strstr(p, "key"));
  free(p); free(t);
  assert(!perdi_pedido("x", NULL, 1.0, "en", ""));

  // Resposta boa: partes juntas, "thought" fora, markdown fora.
  const char *ok = "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"pensando\",\"thought\":true},"
                   "{\"text\":\"**Ana** procura o Rui.\\n\"},{\"text\":\"Ele saiu ontem. caf\\u00e9\"}],\"role\":\"model\"},"
                   "\"finishReason\":\"STOP\"}],\"usageMetadata\":{\"totalTokenCount\":9}}";
  assert(perdi_resposta(200, ok, r, sizeof r) == PERDI_OK);
  assert(!strcmp(r, "Ana procura o Rui. Ele saiu ontem. café"));
  // Buffer curto: corta sem partir um caractere UTF-8.
  { char c[40]; assert(perdi_resposta(200, ok, c, sizeof c) == PERDI_OK); assert(strlen(c) < sizeof c);
    for (size_t i = 0; c[i]; i++) if (((unsigned char)c[i] & 0xE0) == 0xC0) assert(((unsigned char)c[i + 1] & 0xC0) == 0x80); }
  // Erros.
  assert(perdi_resposta(400, "{\"error\":{\"code\":400,\"message\":\"API key not valid. Please pass a valid API key.\",\"status\":\"INVALID_ARGUMENT\"}}", r, sizeof r) == PERDI_CHAVE);
  assert(perdi_resposta(403, "{}", r, sizeof r) == PERDI_CHAVE);
  assert(perdi_resposta(429, "{\"error\":{\"code\":429}}", r, sizeof r) == PERDI_COTA);
  assert(perdi_resposta(0, NULL, r, sizeof r) == PERDI_REDE);
  assert(perdi_resposta(500, "{}", r, sizeof r) == PERDI_ERRO);
  assert(perdi_resposta(400, "{\"error\":{\"message\":\"bad model\"}}", r, sizeof r) == PERDI_ERRO);
  assert(perdi_resposta(200, "{\"promptFeedback\":{\"blockReason\":\"SAFETY\"}}", r, sizeof r) == PERDI_BLOQUEADO);
  assert(perdi_resposta(200, "{\"candidates\":[{\"finishReason\":\"RECITATION\"}]}", r, sizeof r) == PERDI_BLOQUEADO);
  assert(perdi_resposta(200, "{\"candidates\":[]}", r, sizeof r) == PERDI_ERRO && !r[0]);
  { // muitas partes num buffer pequeno: trunca sem estourar
    char pq[16];
    assert(perdi_resposta(200, "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"aaaaaaaaaaaaaaaaaaaaaaaa\"},{\"text\":\"bbbb\"},{\"text\":\"cccc\"},{\"text\":\"dddd\"}]}}]}", pq, sizeof pq) == PERDI_OK);
    assert(strlen(pq) < sizeof pq);
  }
  puts("perdi ok");
  return 0;
}
