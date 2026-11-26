// Sobe a porta de "Peca como fala" em 127.0.0.1 para o tests/pecaserv.sh falar
// com ela por curl. Protocolo no stdin: "p" imprime o pedido pendente, "k" a
// chave, "a" o indice de abrir, "e <json>" publica o estado, "q" sai.
#include "pecaserv.h"
#include "peca.h"
#include <stdio.h>
#include <string.h>

int main(void) {
  char tok[32], l[2048], b[1100];
  int i;
  if (!pecaserv_gerar_token(tok, sizeof tok) || !peca_token_valido(tok)) return 2;
  if (!pecaserv_iniciar(tok, "<!doctype html><p>pagina")) return 3;
  printf("URL %s\n", pecaserv_url()); fflush(stdout);
  while (fgets(l, sizeof l, stdin)) {
    l[strcspn(l, "\n")] = 0;
    if (l[0] == 'q') break;
    else if (l[0] == 'p') printf("P %s\n", pecaserv_pegar_pedido(b, sizeof b) ? b : "-");
    else if (l[0] == 'k') printf("K %s\n", pecaserv_pegar_chave(b, sizeof b) ? b : "-");
    else if (l[0] == 'a') { if (pecaserv_pegar_abrir(&i)) printf("A %d\n", i); else puts("A -"); }
    else if (l[0] == 'e') pecaserv_estado(l + 2);
    else if (l[0] == 'v') printf("V %d\n", pecaserv_visto_ha() >= 0);
    fflush(stdout);
  }
  pecaserv_parar();
  return 0;
}
