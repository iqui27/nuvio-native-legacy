// Exercise the actual settings toggle after a guide has been published.
#include "../src/guia.c"
#include <assert.h>
#include <sys/stat.h>

int main(void) {
  const char *dir = getenv("NUVIO_DADOS");
  assert(dir);
  mkdir(dir, 0700);
  dados_iniciar(NULL);
  ajustes_dir(dir);
  int op = ajustes_rapido_op("liveTvSugerirLocal");
  assert(op >= 0);
  // Keep the loader foreground-only: model an active load, then verify cancellation.
  sTeto = 100;
  int f = sFonte(LTV_M3U, "https://list.test", "List", "");
  GCru c = {0}; c.id = "one"; c.nome = "Aurora"; c.grupo = "Outros"; c.sug = 1;
  assert(sCanalAdd(f, &c));
  adotarStaging(); estado = G_PRONTO;
  assert(nCanais == 1);
  fioVivo = 1;
  for (int i = 0; i < 2; i++) {
    int before = ajustes_livetv_sugerir();
    cargaCancelada = recarregarPend = 0;
    assert(ajustes_rapido_passo(op, 1));
    assert(ajustes_livetv_sugerir() != before);
    assert(cargaCancelada && recarregarPend);
    fioVivo = 0; estado = G_PRONTO;
    assert(ajustes_rapido_passo(op, 1));
    assert(estado == G_PARADO); // Reopening now starts a fresh load.
    fioVivo = 1;
  }
  fioVivo = 0;
  puts("PASS: actual setting toggle cancels stale load and queues refresh after publication, both directions");
  return 0;
}
