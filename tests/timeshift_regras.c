// tests/timeshift_regras.sh: a conta do timeshift local (src/timeshift_regras.h).
#include "../src/timeshift_regras.h"
#include <stdio.h>
#include <stdlib.h>
static int falhas;
#define OK(c, m) do { if (c) printf("ok  %s\n", m); else { printf("FALHOU %s\n", m); falhas++; } } while (0)
#define MB (1ull << 20)
int main(void) {
  TsrIndice *x = malloc(sizeof *x);
  unsigned id = 0, off_id = 0, fora = 0;
  uint64_t pos, off, resto;
  int pulou, tem, i;

  // orcamento
  OK(tsr_teto_bytes(0) == 0 && tsr_teto_bytes(700 * MB) == 0, "pouco espaco livre: desligado");
  OK(tsr_teto_bytes(768 * MB) == 256 * MB, "livre menos a reserva de 512 MB");
  OK(tsr_teto_bytes(64ull << 30) == TSR_TETO_BYTES, "teto automatico de 4 GB");
  OK(tsr_janela_estimada_s(4ull << 30, 8000) == 4294 && tsr_janela_estimada_s(1, 0) == 0, "4 GB a 8 Mbps: 71 min");

  // segmentos: abre no primeiro byte, num marco depois de 6 s, ou a forca por tamanho
  tsr_iniciar(x, 30 * MB);
  OK(tsr_precisa_novo(x, 0, 1000), "indice vazio pede segmento");
  OK(tsr_abrir(x, 1000, &fora, &tem) == 0 && !tem, "primeiro segmento, id 0");
  tsr_somar(x, 10 * MB);
  OK(!tsr_precisa_novo(x, 1, 5000), "marco antes de 6 s nao corta");
  OK(!tsr_precisa_novo(x, 0, 9000), "sem marco nao corta");
  OK(tsr_precisa_novo(x, 1, 7000), "marco depois de 6 s corta");
  OK(tsr_abrir(x, 7000, &fora, &tem) == 1, "segundo segmento, id 1");
  tsr_somar(x, 10 * MB);
  tsr_abrir(x, 13000, &fora, &tem); tsr_somar(x, 10 * MB);
  OK(x->fim == 30 * MB && tsr_usado(x) == 30 * MB && !tsr_expurgar(x, &id), "no orcamento: nada sai");
  OK(tsr_janela_ms(x, 20000) == 19000, "janela: desde o inicio do mais antigo");

  // volta: comeco do ultimo segmento que comecou ate o alvo
  OK(tsr_pos_atras(x, 0, 20000) == 20 * MB, "0 s atras: o segmento em curso");
  OK(tsr_pos_atras(x, 8000, 20000) == 10 * MB, "8 s atras: o segmento do meio");
  OK(tsr_pos_atras(x, 999000, 20000) == 0, "alem da janela: o mais antigo");

  // localizar
  pos = 10 * MB + 188;
  OK(tsr_localizar(x, &pos, &id, &off, &resto, &pulou) && id == 1 && off == 188 &&
     resto == 10 * MB - 188 && !pulou, "posicao no meio do segmento 1");
  pos = 30 * MB;
  OK(!tsr_localizar(x, &pos, &id, &off, &resto, &pulou), "na ponta: esperar");

  // expurgo: acima do teto sai o mais antigo, nunca o que esta em escrita
  tsr_abrir(x, 19000, &fora, &tem); tsr_somar(x, 5 * MB);
  OK(tsr_expurgar(x, &id) && id == 0 && !tsr_expurgar(x, &id), "35 MB em 30: sai so o id 0");
  OK(tsr_inicio(x) == 10 * MB && tsr_janela_ms(x, 20000) == 13000, "janela encolhe com o expurgo");
  pos = 5 * MB;
  OK(tsr_localizar(x, &pos, &id, &off, &resto, &pulou) && pulou && pos == 10 * MB && id == 1 && off == 0,
     "posicao apagada pula para o mais antigo");
  tsr_iniciar(x, 1 * MB);
  tsr_abrir(x, 0, &fora, &tem); tsr_somar(x, 50 * MB);
  OK(!tsr_expurgar(x, &id), "um segmento so, acima do teto: fica");
  OK(!tsr_precisa_novo(x, 0, 1) , "50 MB ainda cabe num segmento");
  tsr_somar(x, 14 * MB);
  OK(tsr_precisa_novo(x, 0, 1), "64 MB sem marco: corta a forca");

  // indice cheio: o mais antigo sai para o novo entrar
  tsr_iniciar(x, 1ull << 40);
  for (i = 0; i < TSR_SEG_MAX; i++) { tsr_abrir(x, i * 6000, &fora, &tem); tsr_somar(x, 188); }
  off_id = tsr_abrir(x, (int64_t)TSR_SEG_MAX * 6000, &fora, &tem);
  OK(tem && fora == 0 && off_id == TSR_SEG_MAX && x->n == TSR_SEG_MAX && tsr_inicio(x) == 188,
     "indice cheio descarta o id 0");

  // passo do botao
  OK(tsr_voltar(0, 100) == 30 && tsr_voltar(60, 100) == 90 && tsr_voltar(90, 100) == 90, "voltar 30 s ate a janela");
  OK(tsr_voltar(0, 20) == 0, "janela menor que o passo: nao volta");
  free(x);
  printf(falhas ? "\n%d FALHA(S)\n" : "\ntimeshift_regras: tudo certo\n", falhas);
  return falhas ? 1 : 0;
}
