// LOG DE MUDANCA DE AJUSTE (ajlog.h + gancho em gravar()): formato, redacao de
// segredo e juncao de rajadas. Roda sem SDL na tela.
#include "../src/ajustes.c"
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

static char linhas[32][240];
static int nl;
static void guarda(const char *l) { if (nl < 32) snprintf(linhas[nl++], sizeof linhas[0], "%s", l); }

int main(void) {
  char dir[] = "/tmp/nuvio-ajlog-XXXXXX";
  int i, op = AJ_RELOGIO;
  ajlog_saida(guarda);

  // 1. Formato e juncao de rajada (tempo controlado).
  ajlog_mudou("tamanhoUiLocal", 1, 2, AJLOG_AJUSTES, 1000);
  ajlog_mudou("tamanhoUiLocal", 2, 3, AJLOG_AJUSTES, 1500);   // segurar a seta
  ajlog_mudou("tamanhoUiLocal", 3, 4, AJLOG_AJUSTES, 2900);   // ainda a mesma rajada
  assert(nl == 0);
  ajlog_vazar(3500); assert(nl == 0);                         // calmo ha so 600 ms
  ajlog_vazar(4900);
  assert(nl == 1);
  assert(!strcmp(linhas[0], "[ajustes] mudou tamanhoUiLocal: 1 -> 4 (origem=ajustes)"));
  // Passou de 2 s: duas linhas.
  nl = 0;
  ajlog_mudou("relogioTelaLocal", 1, 0, AJLOG_CENTRAL, 10000);
  ajlog_mudou("relogioTelaLocal", 0, 1, AJLOG_CENTRAL, 12500);
  assert(nl == 1 && strstr(linhas[0], "relogioTelaLocal: 1 -> 0 (origem=central)"));
  ajlog_vazar_tudo();
  assert(nl == 2 && strstr(linhas[1], "relogioTelaLocal: 0 -> 1"));
  // Voltou ao valor de antes dentro da rajada: nada mudou, nada impresso.
  nl = 0;
  ajlog_mudou("esmaecerLocal", 3, 4, AJLOG_AJUSTES, 20000);
  ajlog_mudou("esmaecerLocal", 4, 3, AJLOG_AJUSTES, 20100);
  ajlog_vazar_tudo(); assert(nl == 0);
  // Chaves diferentes nao se misturam; origem "primeira".
  ajlog_mudou("salvosDestino", 0, 1, AJLOG_PRIMEIRA, 30000);
  ajlog_mudou("brilhoPlayerLocal", 1, 2, AJLOG_AJUSTES, 30010);
  ajlog_vazar_tudo(); assert(nl == 2);
  assert(strstr(linhas[0], "(origem=primeira)"));

  // 2. Redacao: nada de texto livre nem segredo.
  nl = 0;
  ajlog_mudou_texto("seekrChave", "", "sk-SEGREDO-123", AJLOG_AJUSTES, 40000);
  ajlog_mudou_texto("tmdbChave", "abc", "def", AJLOG_AJUSTES, 40000);
  ajlog_mudou_texto("nome", "Ana", "", AJLOG_AJUSTES, 40000);
  ajlog_mudou("pinLocal", 1234, 4321, AJLOG_AJUSTES, 40000);
  ajlog_mudou("fonteRegexLocal", 0, 2, AJLOG_AJUSTES, 40000);
  ajlog_mudou("perfilPin", 1, 2, AJLOG_AJUSTES, 40000);
  ajlog_vazar_tudo();
  assert(nl == 6);
  for (i = 0; i < nl; i++) {
    assert(!strstr(linhas[i], "SEGREDO")); assert(!strstr(linhas[i], "abc"));
    assert(!strstr(linhas[i], "def"));     assert(!strstr(linhas[i], "Ana"));
    assert(!strstr(linhas[i], "1234"));    assert(!strstr(linhas[i], "4321"));
  }
  assert(strstr(linhas[0], "seekrChave: <vazio> -> <texto>"));
  assert(strstr(linhas[2], "nome: <texto> -> <vazio>"));
  assert(strstr(linhas[3], "pinLocal: <oculto> -> <oculto>"));
  assert(ajlog_chave_sensivel("debridKey") && ajlog_chave_sensivel("-xtreamSenha"));
  assert(!ajlog_chave_sensivel("mapping") && !ajlog_chave_sensivel("tamanhoUiLocal"));
  assert(!ajlog_chave_sensivel("mdblist_enabled"));

  // 3. Integracao: so a mudanca da pessoa sai; carga e conta nao.
  assert(mkdtemp(dir));
  ajustes_dir(dir);
  nl = 0;
  ajustes_definir_envio_auto(0);                 // central
  ajustes_definir_envio_auto(1);
  ajlog_vazar_tudo();
  assert(nl == 1 || nl == 0);                     // voltou ao inicio: nada, ou uma linha
  nl = 0;
  { int antes = valor[op];
    assert(ajustes_rapido_passo(op, 1));         // central (painel rapido)
    ajlog_vazar_tudo();
    assert(nl == 1);
    { char esperado[200];
      snprintf(esperado, sizeof esperado, "[ajustes] mudou %s: %d -> %d (origem=central)", CHAVE[op], antes, valor[op]);
      assert(!strcmp(linhas[0], esperado)); } }
  // Pela tela de Ajustes: origem=ajustes.
  nl = 0;
  { int antes = valor[AJ_ESMAECER];
    assert(definirValorDireto(AJ_ESMAECER, (antes + 1) % 3));
    ajlog_vazar_tudo();
    assert(nl == 1 && strstr(linhas[0], "(origem=ajustes)")); }
  // Valor vindo da conta ou do disco: silencio.
  nl = 0;
  valor[AJ_ESMAECER] = (valor[AJ_ESMAECER] + 1) % 5;
  ajOrigem = 0; gravar();
  ajlog_vazar_tudo();
  assert(nl == 0);
  ajustes_dir(dir);                               // recarga: sombra igual ao disco
  assert(ajSombra[op] == valor[op]);
  printf("ajustes_log: ok\n");
  return 0;
}
