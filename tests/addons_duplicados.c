// B4: "os addons se replicam (o mesmo aparece varias vezes na lista) e depois
// o app fecha". O MESMO addon chega em variantes de URL — com e sem
// /manifest.json, com barra no fim, host em outra caixa — e cada porta de
// entrada da lista (conta, copia salva, addons.txt, instalacao pela TV) tem
// de reconhecer que e um so. A URL GRANDE (#203, o apelido ~nv) e o caso
// traicoeiro: o apelido vinha do hash da URL CRUA, entao duas variantes
// viravam dois apelidos e passavam pelo dedupe como addons diferentes.
// NAO precisa de rede.
//
//   bash tests/addons_duplicados.sh
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
void wsenvio_podar(void) {}
void wsenvio_podar_addon(const char *base) { (void)base; }
#include "addons.h"
#include "fontecache.h"
void fontecache_vod_limpar(void) {}
const char *rede_url_publica(const char *url, char *dst, unsigned n) { (void)url; snprintf(dst, n, "-"); return dst; }

static int falhas;
static void conferir(const char *caso, int obtido, int esperado) {
  if (obtido != esperado) {
    printf("FALHOU %s: %d (esperado %d)\n", caso, obtido, esperado);
    falhas++;
  }
}

// Como lerAddons (sync.c) guarda a URL da conta: a grande vira apelido.
static AddonRemoto A(const char *url) {
  AddonRemoto a;
  memset(&a, 0, sizeof a);
  snprintf(a.nome, sizeof a.nome, "Dup");
  if (!nv_addon_url_guardar(a.nome, url, a.url, sizeof a.url)) a.url[0] = 0;
  a.ativo = 1;
  return a;
}

int main(void) {
  static AddonRemoto l[8];
  static char grande[3000], g1[3100], g2[3100], g3[3100];
  static char base[2041], m1[2060], m2[2044];
  char dir[512];
  FILE *f;

  // CONTA COM O MESMO ADDON 3x, variantes de URL, mais um outro.
  l[0] = A("https://a.test/cfg/manifest.json");
  l[1] = A("https://A.Test/cfg/");
  l[2] = A("https://a.test/cfg");
  l[3] = A("https://b.test/manifest.json");
  conferir("conta-aplicou", addons_definir_lista(l, 4), 1);
  conferir("conta-n", addons_n(), 2);
  // A mesma lista de novo nao e mudanca (senao cada sync zera as sondas).
  conferir("conta-igual", addons_definir_lista(l, 4), 0);

  // Credenciais distinguem configuracoes; esquema e host continuam sem caixa.
  l[0] = A("https://u:AbC@addon.test/cfg");
  l[1] = A("https://u:abc@addon.test/cfg");
  l[2] = A("HTTPS://u:AbC@ADDON.TEST/cfg/manifest.json");
  l[3] = A("https://U:AbC@addon.test/cfg");
  conferir("credenciais-aplicou", addons_definir_lista(l, 4), 1);
  conferir("credenciais-n", addons_n(), 3);

  // URL GRANDE (AIOLists): tres variantes, tres apelidos crus.
  memset(grande, 'x', sizeof grande - 1);
  memcpy(grande, "https://g.test/", 15);
  snprintf(g1, sizeof g1, "%s/manifest.json", grande);
  snprintf(g2, sizeof g2, "%s/", grande);
  snprintf(g3, sizeof g3, "%s", grande);
  l[0] = A(g1); l[1] = A(g2); l[2] = A(g3); l[3] = A("https://b.test/");
  conferir("grande-aplicou", addons_definir_lista(l, 4), 1);
  conferir("grande-n", addons_n(), 2);

  // INSTALAR PELA TV uma variante do que ja esta na lista.
  conferir("instalar-variante", addons_adicionar("Dup", "HTTPS://B.TEST/manifest.json"), 0);
  conferir("instalar-grande", addons_adicionar("Dup", g2), 0);
  conferir("instalar-n", addons_n(), 2);

  // addons.txt SALVO com o mesmo addon 3x.
  snprintf(dir, sizeof dir, "%s/nuvio-addons-dup-%d", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (int)getpid());
  { char cmd[600]; snprintf(cmd, sizeof cmd, "mkdir -p '%s'", dir); if (system(cmd)) return 2; }
  { char caminho[600]; snprintf(caminho, sizeof caminho, "%s/addons.txt", dir);
    f = fopen(caminho, "w");
    if (!f) return 2;
    fprintf(f, "A\thttps://a.test/cfg/manifest.json\nA\thttps://a.test/cfg/\nA\tHTTPS://A.TEST/cfg\nB\thttps://b.test\n");
    fclose(f);
    conferir("arquivo-n", addons_carregar(dir), 2);
    remove(caminho); }
  rmdir(dir);

  // LIMIAR DE 2048 (revisao B4), por ultimo (definir_lista troca a lista
  // inteira): a base que CABE fica LITERAL e a mesma base com
  // "/manifest.json", que passa do limite, vira APELIDO — e um addon so.
  // A chave da literal era a URL inteira e a do apelido, o hash: dois addons.
  memset(base, 'x', 2040); base[2040] = 0;
  memcpy(base, "https://l.test/", 15);                  // 2040 exatos: literal
  snprintf(m1, sizeof m1, "%s/manifest.json", base);   // 2054: apelido
  snprintf(m2, sizeof m2, "%s/", base);                 // 2041: literal
  l[0] = A(m1); l[1] = A(m2); l[2] = A(base); l[3] = A("https://b.test/");
  conferir("limiar-aplicou", addons_definir_lista(l, 4), 1);
  conferir("limiar-n", addons_n(), 2);

  // A mesma regra vale para URLs grandes guardadas como apelidos.
  memset(grande, 'x', sizeof grande - 1);
  memcpy(grande, "https://u:AbC@addon.test/", sizeof "https://u:AbC@addon.test/" - 1);
  snprintf(g1, sizeof g1, "%s/manifest.json", grande);
  grande[10] = 'a';
  snprintf(g2, sizeof g2, "%s", grande);
  l[0] = A(g1); l[1] = A(g2);
  conferir("credenciais-grandes-aplicou", addons_definir_lista(l, 2), 1);
  conferir("credenciais-grandes-n", addons_n(), 2);

  if (falhas) return 1;
  printf("addons_duplicados: ok\n");
  return 0;
}
