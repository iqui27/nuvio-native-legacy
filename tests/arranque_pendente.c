// Relato de queda pendente grande nao pode cortar o relato NOVO. Ver o .sh.
#define NV_WEBOS 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Stubs do que arranque.c usa de queda.c, avisos.c e ajustes.c.
static size_t tamRelatoNovo = 3000;
void queda_armar(const char *a) { (void)a; }
void queda_espelho(const char *a) { (void)a; }
int queda_relatar_em(const char *a, FILE *saida) {
  size_t i;
  (void)a;
  fputs("INICIO-RELATO-NOVO\n", saida);
  for (i = 0; i < tamRelatoNovo; i++) fputc('a' + (int)(i % 26), saida);
  fputs("\nFIM-RELATO-NOVO\n", saida);
  return 1;
}
void avisos_tv_linha(char *linha, size_t cap, char *modelo, size_t capModelo) {
  snprintf(linha, cap, "[tv] modelo=TESTE"); snprintf(modelo, capModelo, "TESTE");
}
int avisos_enviar_arranque(const char *relato, const char *tv, int segundos) {
  (void)relato; (void)tv; (void)segundos; return 0;
}
int ajustes_envio_auto(void) { return 0; }

#include "../src/arranque.c"

static int falhas;
#define CONFERE(c, msg) do { if (!(c)) { printf("FALHOU: %s\n", msg); falhas++; } else printf("ok: %s\n", msg); } while (0)

static char *lerTudo(const char *caminho, size_t *n) {
  FILE *f = fopen(caminho, "rb"); char *b; long t;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); t = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)t + 1);
  *n = fread(b, 1, (size_t)t, f); b[*n] = 0; fclose(f);
  return b;
}

static void rodar(size_t tamAntigo, const char *log, int esperaCorte) {
  FILE *f;
  size_t i, n = 0, nl = 0;
  char *pend, *saida, msg[160];
  int fdOut = dup(1);
  f = fopen(ARQ_ANTES, "w"); fputs("rede t=1\n", f); fclose(f);
  f = fopen(ARQ_PEND, "w");
  fputs("CABECA-RELATO-VELHO\n", f);
  for (i = 0; i < tamAntigo; i++) fputc(i % 80 == 79 ? '\n' : 'v', f);
  fputs("\nCAUDA-RELATO-VELHO\n", f);
  fclose(f);
  fflush(stdout);
  if (!freopen(log, "w", stdout)) { perror("freopen"); exit(2); }
  arranque_relatar();
  fflush(stdout);
  dup2(fdOut, 1); close(fdOut);
  pend = lerTudo(ARQ_PEND, &n);
  saida = lerTudo(log, &nl);
  snprintf(msg, sizeof msg, "antigo=%zu: relato novo inteiro no pendente (inicio)", tamAntigo);
  CONFERE(pend && strstr(pend, "INICIO-RELATO-NOVO"), msg);
  snprintf(msg, sizeof msg, "antigo=%zu: relato novo inteiro no pendente (fim)", tamAntigo);
  CONFERE(pend && strstr(pend, "FIM-RELATO-NOVO"), msg);
  snprintf(msg, sizeof msg, "antigo=%zu: relatoPendente == arquivo", tamAntigo);
  CONFERE(pend && relatoPendente && strcmp(pend, relatoPendente) == 0, msg);
  snprintf(msg, sizeof msg, "antigo=%zu: rastro da etapa presente", tamAntigo);
  CONFERE(pend && strstr(pend, "[arranque] ultima etapa: rede"), msg);
  snprintf(msg, sizeof msg, "antigo=%zu: algo do relato velho continua", tamAntigo);
  CONFERE(pend && strstr(pend, "--- relato anterior sem envio ---"), msg);
  if (esperaCorte) {
    snprintf(msg, sizeof msg, "antigo=%zu: log diz que o relato velho foi cortado", tamAntigo);
    CONFERE(saida && strstr(saida, "[arranque] relato anterior cortado"), msg);
  } else {
    snprintf(msg, sizeof msg, "antigo=%zu: relato velho pequeno vai inteiro", tamAntigo);
    CONFERE(pend && strstr(pend, "CABECA-RELATO-VELHO") && strstr(pend, "CAUDA-RELATO-VELHO"), msg);
    snprintf(msg, sizeof msg, "antigo=%zu: sem aviso de corte", tamAntigo);
    CONFERE(saida && !strstr(saida, "relato anterior cortado"), msg);
  }
  free(pend); free(saida); free(relatoPendente); relatoPendente = NULL;
}

int main(int argc, char **argv) {
  const char *log = argc > 1 ? argv[1] : "/tmp/nuvio-arranque-pendente-log.txt";
  rodar(500, log, 0);        // pequeno: tudo cabe, nada muda
  rodar(7000, log, 1);       // cabia no antigo[8192], mas estourava nrq+2048
  rodar(40000, log, 1);      // maior que o buffer antigo
  unlink(ARQ_PEND); unlink(ARQ_ANTES); unlink(ARQ_ETAPA); unlink(log);
  printf(falhas ? "arranque_pendente: %d falha(s)\n" : "arranque_pendente: ok\n", falhas);
  return falhas ? 1 : 0;
}
