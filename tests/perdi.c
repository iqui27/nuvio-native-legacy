// "O que eu perdi?": corte sem spoiler, pedido do Gemini e leitura da resposta.
#include "perdi.h"
#include <assert.h>
#include <math.h>
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

  // LEGENDA EMBUTIDA (Android/Media3): sem documento, as falas exibidas viram
  // o historico; o corte sem spoiler continua valendo sobre ele.
  {
    PerdiHist h;
    memset(&h, 0, sizeof h);
    assert(perdi_hist_anotar(&h, 10.0, "Bom dia, Ana.") == 1);
    assert(perdi_hist_anotar(&h, 10.2, "Bom dia, Ana.") == 0);   // ainda na tela
    assert(perdi_hist_anotar(&h, 11.0, "") == 0);                // tela vazia
    assert(perdi_hist_anotar(&h, 11.5, "Bom dia, Ana.") == 0);   // repetida a < 2 s
    assert(perdi_hist_anotar(&h, 30.0, "Cadê o Rui?") == 1);
    assert(perdi_hist_anotar(&h, 95.0, "Ele saiu ontem.") == 1);
    assert(perdi_hist_anotar(&h, 100.5, "O ASSASSINO É O MORDOMO") == 1);
    assert(perdi_hist_anotar(&h, NAN, "x") == 0 && perdi_hist_anotar(&h, -1.0, "x") == 0);
    // Voltou no video: a fala entra na ordem de tempo; rever nao duplica.
    assert(perdi_hist_anotar(&h, 20.0, "Onde ele está?") == 1);
    assert(perdi_hist_anotar(&h, 30.5, "Cadê o Rui?") == 0);
    assert(h.n == 5);
    for (int i = 1; i < h.n; i++) assert(h.v[i - 1].inicio <= h.v[i].inicio);
    // Voltou para 96 s: o MORDOMO (100,5 s) ja anotado NAO entra no pedido.
    t = perdi_trecho(h.v, h.n, 96.0, 0, PERDI_TRECHO_MAX, &falas);
    assert(t && falas == 4 && strstr(t, "Ele saiu ontem.") && !strstr(t, "MORDOMO"));
    free(t);
    perdi_hist_zerar(&h);
    assert(!h.v && !h.n && !h.cap);
    // Teto: cai o comeco, a ordem fica.
    char linha[32];
    for (int i = 0; i < PERDI_HIST_MAX + 10; i++) {
      snprintf(linha, sizeof linha, "fala %d", i);
      assert(perdi_hist_anotar(&h, 3.0 * i, linha) == 1);
    }
    assert(h.n <= PERDI_HIST_MAX && !strcmp(h.v[h.n - 1].texto, "fala 3009"));
    for (int i = 1; i < h.n; i++) assert(h.v[i - 1].inicio < h.v[i].inicio);
    perdi_hist_zerar(&h);
  }

  // Qual cartao: com a embutida ligada e sem documento, NUNCA o "ative uma
  // legenda" (a regressao do Android: a pessoa ja tinha ligado a legenda).
  assert(perdi_modo(10, 0, 0) == PERDI_MODO_PEDIR_DOC);
  assert(perdi_modo(10, 1, 50) == PERDI_MODO_PEDIR_DOC);       // documento manda
  assert(perdi_modo(2, 0, 0) == PERDI_MODO_POUCO);
  assert(perdi_modo(0, 1, 0) == PERDI_MODO_POUCO);
  assert(perdi_modo(-1, 0, 0) == PERDI_MODO_SEM_LEGENDA);
  assert(perdi_modo(-1, 1, 0) == PERDI_MODO_EMBUTIDA_SEM_TEXTO);
  assert(perdi_modo(-1, 1, 2) == PERDI_MODO_EMBUTIDA_POUCO);
  assert(perdi_modo(-1, 1, PERDI_FALAS_MIN) == PERDI_MODO_PEDIR_HIST);
  // Regra pura: historico com falas manda mesmo sem embutida ligada. No player
  // isso nao acontece (perdiColher zera o historico ao trocar ou desligar a faixa).
  assert(perdi_modo(-1, 0, PERDI_FALAS_MIN) == PERDI_MODO_PEDIR_HIST);
  for (int e = 0; e <= 1; e++)
    for (int f = 0; f < 8; f++)
      if (e || f) assert(perdi_modo(-1, e, f) != PERDI_MODO_SEM_LEGENDA);

  // UTF-8: corte em buffer fixo nunca deixa meio caractere no fim.
  {
    char u[16];
    strcpy(u, "ab\xc3\xa9"); assert(perdi_utf8_aparar(u) == 4);            // "abé" inteiro
    strcpy(u, "ab\xc3"); assert(perdi_utf8_aparar(u) == 2 && !strcmp(u, "ab"));
    strcpy(u, "a\xe2\x80"); assert(perdi_utf8_aparar(u) == 1 && !strcmp(u, "a"));
    strcpy(u, "a\xf0\x9f\x98"); assert(perdi_utf8_aparar(u) == 1);
    strcpy(u, "a\xf0\x9f\x98\x80"); assert(perdi_utf8_aparar(u) == 5);   // emoji inteiro
    strcpy(u, ""); assert(perdi_utf8_aparar(u) == 0);
    assert(perdi_utf8_aparar(NULL) == 0);
    // Fala maior que o teto de LegendaCue.texto, feita de "é": o historico e o
    // trecho guardam so caracteres inteiros, e a mesma fala longa nao reentra.
    PerdiHist h;
    char longa[2000];
    memset(&h, 0, sizeof h);
    for (int i = 0; i + 2 < (int)sizeof longa; i += 2) { longa[i] = (char)0xC3; longa[i + 1] = (char)0xA9; }
    longa[sizeof longa - 2] = 0;
    longa[767 - 1] = 0;                       // 766 bytes: o teto (767) cairia no meio
    char tmp[2100];
    snprintf(tmp, sizeof tmp, "xy%s", longa); // 768 bytes: o teto de 767 parte o ultimo "é"
    strcat(tmp, "\xc3\xa9\xc3\xa9");        // passa do teto: o corte cai no meio de um "é"
    assert(perdi_hist_anotar(&h, 1.0, tmp) == 1);
    assert(perdi_hist_anotar(&h, 1.1, tmp) == 0);          // a mesma, ainda na tela
    size_t L = strlen(h.v[0].texto);
    assert(L < sizeof h.v[0].texto && ((unsigned char)h.v[0].texto[L - 1] == 0xA9));
    char *tr = perdi_trecho(h.v, h.n, 5.0, 0, PERDI_TRECHO_MAX, &falas);
    assert(tr && falas == 1);
    size_t T = strlen(tr);
    assert(tr[T - 1] == '\n' && (unsigned char)tr[T - 2] == 0xA9);
    free(tr);
    perdi_hist_zerar(&h);
  }
  puts("perdi ok");
  return 0;
}
