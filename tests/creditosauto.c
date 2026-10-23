#include "creditosauto.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  CreditosAuto s;
  creditosauto_zerar(&s);
  assert(!creditosauto_aguardar(&s, 0, 1, 0, 0));
  creditosauto_buscar(&s, 1440);
  assert(creditosauto_aguardar(&s, 1440, 1, 0, 0));
  assert(creditosauto_aguardar(&s, 1450, 0, 0, 0)); // pausa nao confirma progresso
  assert(creditosauto_aguardar(&s, NAN, 1, 0, 0));
  assert(!creditosauto_aguardar(&s, 1450, 1, 0, 0));
  creditosauto_buscar(&s, 1500);
  assert(creditosauto_aguardar(&s, 1500, 1, 0, 0));
  assert(!creditosauto_aguardar(&s, 1500, 0, 1, 0)); // fim nativo

  creditosauto_recuar(&s, 1490, 1480);
  assert(creditosauto_aguardar(&s, 1480, 1, 0, 0));
  assert(creditosauto_aguardar(&s, 1490, 1, 1, 0)); // amostra/fim antigos
  assert(!creditosauto_aguardar(&s, 1480.5, 1, 0, 0));
  creditosauto_recuar(&s, 1450, 1440);
  creditosauto_recuar(&s, 1440, 1430); // recuo continuo acompanha todo o gesto
  assert(s.fimBusca == 1430 && s.inicioBusca == 1440);
  assert(creditosauto_aguardar(&s, 1440, 1, 1, 0));
  assert(!creditosauto_aguardar(&s, 1430.5, 1, 0, 0));
  creditosauto_recuar(&s, 1430, 1440); // avanco nao arma recuo
  creditosauto_recuar(&s, NAN, 1400);
  creditosauto_recuar(&s, 1430, INFINITY);
  assert(!s.buscaPendente);
  creditosauto_recuar(&s, 1430, 1400);
  creditosauto_zerar(&s);
  assert(!s.buscaPendente && !s.buscaRecuo);
  creditosauto_recuar(&s, 1490, 1480); // recuo substituido por avanco: nunca pousa
  creditosauto_destino(&s, 1480);      // o proprio destino nao desarma
  assert(s.buscaPendente);
  creditosauto_destino(&s, 1500);
  assert(!s.buscaPendente);
  assert(!creditosauto_aguardar(&s, 1500, 1, 0, 0));
  creditosauto_recuar(&s, 1490, 1480); // seekTo falho: so o tempo libera
  assert(creditosauto_aguardar(&s, 1490, 1, 0, 100));
  assert(creditosauto_aguardar(&s, 1490, 1, 0, 100 + CREDITOSAUTO_LIMITE_MS - 1));
  assert(!creditosauto_aguardar(&s, 1490, 1, 0, 100 + CREDITOSAUTO_LIMITE_MS));
  // Recuo menor que a folga de progresso: o relogio pode ficar congelado
  // ate o novo fim nativo. Mesmo parado no fim, a busca precisa expirar.
  creditosauto_recuar(&s, 1500, 1499.9);
  assert(creditosauto_aguardar(&s, 1499.9, 1, 0, 100));
  assert(creditosauto_aguardar(&s, 1500, 0, 1, 1600));
  assert(creditosauto_aguardar(&s, 1500, 0, 1, 100 + CREDITOSAUTO_LIMITE_MS - 1));
  assert(!creditosauto_aguardar(&s, 1500, 0, 1, 100 + CREDITOSAUTO_LIMITE_MS));
  // Pausa de verdade, sem fim nativo, continua sem gastar o prazo.
  creditosauto_recuar(&s, 1500, 1499.9);
  assert(creditosauto_aguardar(&s, 1499.9, 0, 0, 100));
  assert(creditosauto_aguardar(&s, 1499.9, 0, 0, 100 + CREDITOSAUTO_LIMITE_MS));
  creditosauto_zerar(&s);
  creditosauto_fonte(&s, "https://example.test/corte-a");
  creditosauto_fonte(&s, "https://example.test/corte-a");
  assert(!s.fonteMudou);
  creditosauto_fonte(&s, "https://example.test/corte-b");
  assert(s.fonteMudou);
  creditosauto_zerar(&s);
  assert(!s.fonteMudou && !s.temFonte);
  { static char longa[5000]; // URL >= 4096 bytes sem troca real nao muda a fonte
    memset(longa, 'a', sizeof longa - 1);
    creditosauto_fonte(&s, longa); creditosauto_fonte(&s, longa);
    assert(!s.fonteMudou);
    longa[4500] = 'b'; creditosauto_fonte(&s, longa);
    assert(s.fonteMudou); }
  puts("creditosauto: progresso real, fim nativo, recuo continuo e amostras antigas OK; pulo em intro_auto");
  return 0;
}
