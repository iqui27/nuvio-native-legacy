// Anel de eventos do Farol (issue 2, secao 2.2 do PLANO). Sem SDL nem rede:
// linka so src/farolanel.c. Cobre: volta do anel, leitura em ordem, filtro por
// janela (com virada do relogio de 32 bits), farolanel_contar, saneamento do
// token (URL e titulo viram "x"), limites da gravacao e 4 fios gravando
// 200 000 eventos sem corrida (TSan).
//
//   bash tests/farol_anel.sh                  (ASan+UBSan, o padrao)
//   SANITIZE=thread bash tests/farol_anel.sh  (TSan)
//   SANITIZE=0 bash tests/farol_anel.sh       (so os asserts)
#include "../src/farolanel.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Relogio injetado: o teste controla t_ms ao milissegundo.
static uint32_t tAgora;
static uint32_t relogioFalso(void) { return tAgora; }

static int tokOk(const char *t) {
  size_t i, n = strlen(t);
  if (n > FAROL_TOKEN_N - 1) return 0;
  for (i = 0; i < n; i++) {
    char c = t[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
          c == '_' || c == '.' || c == ':' || c == '+' || c == '-'))
      return 0;
  }
  return 1;
}

static void tokenDeveSer(const char *entrada, const char *esperado) {
  char out[FAROL_TOKEN_N];
  farolanel_token(entrada, out);
  assert(strcmp(out, esperado) == 0);
}

// --- fios ------------------------------------------------------------------
#define FIOS    4
#define POR_FIO 50000
static void *produtor(void *p) {
  int id = (int)(intptr_t)p, i;
  char tok[FAROL_TOKEN_N];
  snprintf(tok, sizeof tok, "fio%d", id);
  for (i = 0; i < POR_FIO; i++)
    farol_ev((FarolMod)(id % FM_N), FE_ABRIR + (i % 4), i % 3, id, i, tok);
  return NULL;
}

static void testaVoltaEOrdem(void) {
  FarolEvento buf[300];
  int i, n;

  farolanel_zerar();
  farolanel_relogio(relogioFalso);

  // 1 000 eventos: ficam os 256 ultimos, do mais antigo para o mais novo.
  for (i = 0; i < 1000; i++) {
    tAgora = 1000u + (uint32_t)i;
    farol_ev(FM_VIDEO, FE_ERRO_NATIVO, 2, i, i * 2, "ums_204");
  }
  n = farolanel_ler(buf, 300, 0);
  assert(n == FAROL_ANEL_N);
  for (i = 0; i < n; i++) {
    assert(buf[i].a == 744 + i);
    assert(buf[i].b == (744 + i) * 2);
    assert(strcmp(buf[i].tok, "ums_204") == 0);
    if (i) assert(buf[i].t_ms >= buf[i - 1].t_ms);
  }
  assert(buf[0].t_ms == 1744 && buf[FAROL_ANEL_N - 1].t_ms == 1999);

  // max < disponiveis: os max mais antigos, em ordem.
  n = farolanel_ler(buf, 10, 0);
  assert(n == 10 && buf[0].a == 744 && buf[9].a == 753);

  // Filtro por janela: desde_ms corta pelo carimbo.
  n = farolanel_ler(buf, 300, 1800);
  assert(n == 200 && buf[0].a == 800 && buf[199].a == 999);

  // Virada do relogio de 32 bits (SDL_GetTicks/milissegundos viram em ~49 dias):
  // t_ms perto do fim + eventos depois da virada nao podem "sumir".
  farolanel_zerar();
  for (i = 0; i < 20; i++) {
    tAgora = 0xFFFFFFF0u + (uint32_t)i;   // vira em i == 16
    farol_ev(FM_VIDEO, FE_ERRO_NATIVO, 1, i, 0, "wrap");
  }
  n = farolanel_ler(buf, 300, 0xFFFFFFF0u);
  assert(n == 20 && buf[0].a == 0 && buf[19].a == 19);
  n = farolanel_ler(buf, 300, 0xFFFFFFFEu);   // desde antes da virada
  assert(n == 6 && buf[0].a == 14 && buf[5].a == 19);
  n = farolanel_ler(buf, 300, 0xFFFFFFFAu);
  assert(n == 10 && buf[0].a == 10 && buf[9].a == 19);
  n = farolanel_ler(buf, 300, 5);             // tudo e "mais antigo" que 5
  assert(n == 0);
}

static void testaContar(void) {
  farolanel_zerar();
  tAgora = 8000;  farol_ev(FM_PLAYER, FE_PARADO, 2, 0, 0, "a");
  tAgora = 9000;  farol_ev(FM_PLAYER, FE_PARADO, 2, 0, 0, "a");
  tAgora = 9500;  farol_ev(FM_PLAYER, FE_PARADO, 2, 0, 0, "a");
  tAgora = 9600;  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, 0, 0, "b");
  tAgora = 20000; farol_ev(FM_PLAYER, FE_PARADO, 2, 0, 0, "a");

  assert(farolanel_contar(FE_PARADO, 5000, 10000) == 3);   // 8000, 9000, 9500
  assert(farolanel_contar(FE_PARADO, 500, 10000) == 1);    // so 9500
  assert(farolanel_contar(FE_PARADO, 0, 10000) == 4);      // sem janela: todos
  assert(farolanel_contar(FE_ERRO_FONTE, 5000, 10000) == 1);
  assert(farolanel_contar(FE_PRONTO, 0, 10000) == 0);
  assert(farolanel_contar(FE_PARADO, 5000, 8500) == 1);    // futuro nao conta
  assert(farolanel_contar(FE_PARADO, 5000, 500) == 0);
}

static void testaToken(void) {
  char out[FAROL_TOKEN_N], s[64];
  uint32_t r = 0x9E3779B9u;
  int k, virouX = 0, ficou = 0, i, tam;

  // Casos reais do plano: URL e titulo viram "x"; selos e codigos passam.
  tokenDeveSer("https://cache.exemplo.com/a?key=segredo", "x");
  tokenDeveSer("http://h/t", "x");
  tokenDeveSer("Um Titulo Com Espaco", "x");          // espaco e acento
  tokenDeveSer("T\xc3\xadtulo", "x");                  // byte >= 0x80
  tokenDeveSer("user@host", "x");
  tokenDeveSer("a%20b", "x");
  tokenDeveSer("Bearer abc", "x");
  tokenDeveSer("senha;id=1", "x");
  tokenDeveSer("", "x");
  tokenDeveSer(NULL, "x");
  tokenDeveSer("2160p_hdr_dv_eac3", "2160p_hdr_dv_eac3");
  tokenDeveSer("a.b:c+d_e-f", "a.b:c+d_e-f");
  tokenDeveSer("AUDIOCODECNOTSUPPORTED", "audiocodecnotsupported"); // dobra A-Z
  tokenDeveSer("ums_40403", "ums_40403");

  // Corte em 39: 50 caracteres validos ficam nos 39 primeiros.
  memset(s, 'a', 50); s[50] = '\0';
  farolanel_token(s, out);
  assert(strlen(out) == FAROL_TOKEN_N - 1 && tokOk(out));

  // 100 000 cadeias aleatorias de bytes 1-255: a saida e "x" ou so alfabeto.
  for (k = 0; k < 100000; k++) {
    tam = 1 + (int)(r % 48u);
    for (i = 0; i < tam; i++) {
      r ^= r << 13; r ^= r >> 17; r ^= r << 5;
      s[i] = (char)(r % 255u + 1u);   // byte 1..255, nunca NUL
    }
    s[tam] = '\0';
    farolanel_token(s, out);
    assert(out[0]);
    assert(tokOk(out));
    if (!strcmp(out, "x")) virouX++; else ficou++;
  }
  assert(virouX > 0);   // com bytes 1-255, a grande maioria tem de virar "x"
  (void)ficou;
  printf("token: %d viraram x, %d passaram\n", virouX, ficou);
}

static void testaGravacaoSaneada(void) {
  FarolEvento buf[8];
  int n;

  farolanel_zerar();
  tAgora = 100;

  // O token e saneado NA GRAVACAO: produtor descuidado com URL grava "x".
  farol_ev(FM_FONTE, FE_FONTE_ESCOLHIDA, 0, 9120, 142,
           "magnet:?xt=urn:btih:AAABBB&dn=Titulo");
  // Fora de faixa: enquadrado, nao derruba.
  farol_ev((FarolMod)99, FE_PRONTO, 9, 1, 2, "ok_tok");
  farol_ev(FM_VIDEO, 9999, 1, 3, 4, "ok_tok2");

  n = farolanel_ler(buf, 8, 0);
  assert(n == 3);
  assert(strcmp(buf[0].tok, "x") == 0);
  assert(buf[1].mod == FM_FAROL && buf[1].sev == 2 && buf[1].ev == FE_PRONTO);
  assert(strcmp(buf[1].tok, "ok_tok") == 0);
  assert(buf[2].ev == FE_NENHUM);

  // zerar esvazia de verdade.
  farolanel_zerar();
  assert(farolanel_ler(buf, 8, 0) == 0);
  assert(farolanel_contar(FE_PRONTO, 0, tAgora) == 0);
}

static void testaCorrida(void) {
  FarolEvento buf[FAROL_ANEL_N];
  pthread_t f[FIOS];
  int i, n;

  farolanel_zerar();
  farolanel_relogio(NULL);          // relogio monotonico de verdade
  for (i = 0; i < FIOS; i++)
    assert(pthread_create(&f[i], NULL, produtor, (void *)(intptr_t)i) == 0);
  for (i = 0; i < FIOS; i++)
    assert(pthread_join(f[i], NULL) == 0);

  // Anel cheio, coerente: nenhum slot rasgado, t_ms nao decresce.
  n = farolanel_ler(buf, FAROL_ANEL_N, 0);
  assert(n == FAROL_ANEL_N);
  for (i = 0; i < n; i++) {
    char esperado[8];
    assert(buf[i].mod < FM_N);
    assert(buf[i].sev <= 2);
    assert(buf[i].ev >= FE_ABRIR && buf[i].ev <= FE_ABRIR + 3);
    assert(buf[i].a >= 0 && buf[i].a < FIOS);
    assert(buf[i].b >= 0 && buf[i].b < POR_FIO);
    snprintf(esperado, sizeof esperado, "fio%d", buf[i].a);
    assert(strcmp(buf[i].tok, esperado) == 0);   // tok e a do mesmo slot
    if (i) assert(buf[i].t_ms >= buf[i - 1].t_ms);
  }
}

int main(void) {
  testaToken();
  testaVoltaEOrdem();
  testaContar();
  testaGravacaoSaneada();
  testaCorrida();
  farolanel_relogio(NULL);
  printf("farol_anel: ok\n");
  return 0;
}
