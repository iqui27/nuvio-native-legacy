#include "stub_perfisessao.h"
// Stalker (src/stalker.c): normalizacao do portal e varredura de rotas (#237),
// sem rede e sem disco. O prefixo de caminho colado ("/meuportal/c/") tem de
// ficar; "/c/" e barras finais, nao.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "stalker.h"

static char disco[2048]; static int temDisco;
char *dados_ler(const char *nome) { (void)nome; return temDisco ? strdup(disco) : NULL; }
int dados_gravar(const char *nome, const char *c) { (void)nome; snprintf(disco, sizeof disco, "%s", c); temDisco = 1; return 1; }
void dados_apagar(const char *nome) { (void)nome; temDisco = 0; }
static int perfil = 1;
int perfis_ativo(void) { return perfil; }
uint64_t perfis_sessao_geracao(void) { return 0; }
int perfis_sessao_atual(uint64_t g) { return g == 0; }
int perfis_sessao_travar(uint64_t g) { return g == 0; }
void perfis_sessao_destravar(void) {}

static const char *okPrefixo;   // url que "responde" ao handshake
static char urls[32][300]; static int nUrls; static char ultimoRef[300], primeiroRef[300];
char *rede_baixar_com(const char *url, int segundos, const char *const *cab) {
  int i;
  (void)segundos;
  if (nUrls < 32) snprintf(urls[nUrls++], sizeof urls[0], "%s", url);
  for (i = 0; cab && cab[i]; i++)
    if (!strncmp(cab[i], "Referer: ", 9)) { snprintf(ultimoRef, sizeof ultimoRef, "%s", cab[i]); if (nUrls == 1) snprintf(primeiroRef, sizeof primeiroRef, "%s", cab[i]); }
  if (okPrefixo && !strncmp(url, okPrefixo, strlen(okPrefixo)))
    return strdup("{\"js\":1,\"token\":\"TKN\"}");
  return strdup("<html>nao e portal</html>");
}

static void confere(const char *entrada, const char *curto) {
  stalker_definir_portal(entrada);
  if (strcmp(stalker_portal_curto(), curto)) {
    fprintf(stderr, "FALHOU: '%s' -> '%s' (esperava '%s')\n", entrada, stalker_portal_curto(), curto);
    exit(1);
  }
}

int main(void) {
  StalkerCanal sai[4];
  stalker_definir_portal("meu.portal.tv:8080");
  confere("meu.portal.tv", "meu.portal.tv");
  confere("meu.portal.tv:8080", "meu.portal.tv:8080");
  confere("http://meu.portal.tv:8080/", "meu.portal.tv:8080");
  confere("https://meu.portal.tv:8080/c/", "meu.portal.tv:8080");
  confere("meu.portal.tv:8080/c", "meu.portal.tv:8080");
  confere("  meu.portal.tv:8080/c/  ", "meu.portal.tv:8080");
  confere("meu.portal.tv/stalker_portal/c/", "meu.portal.tv/stalker_portal");
  confere("http://meu.portal.tv/custom/c/", "meu.portal.tv/custom");
  confere("http://meu.portal.tv:80/a/b///", "meu.portal.tv:80/a/b");
  confere("meu.portal.tv/meuportal?x=1", "meu.portal.tv/meuportal");
  confere("host.tv:8080/c/index.html", "host.tv:8080");
  confere("host.tv/stalker_portal/c/index.php", "host.tv/stalker_portal");
  confere("host.tv/custom/c/", "host.tv/custom");
  confere("host.tv/cursos", "host.tv/cursos");
  puts("ok  normaliza host, porta, /c/, prefixo, esquema e espacos");

  // Valor antigo (so host, gravado antes) segue valendo.
  perfil = 2;                 // outro perfil: forca a releitura do "disco"
  snprintf(disco, sizeof disco, "portal\thttp://velho.tv:8080\nmac\t00:1a:79:00:00:01\n");
  stalker_carregar();
  assert(stalker_configurado() && !strcmp(stalker_portal_curto(), "velho.tv:8080"));
  puts("ok  cadastro antigo carrega");

  perfil = 1; temDisco = 0;
  // Rotas: com prefixo, ele vem antes; stalker_portal nao duplica.
  stalker_definir_mac("00:1a:79:00:00:01");
  stalker_definir_portal("host.tv/meuportal/c/");
  nUrls = 0;
  stalker_canais(sai, 4);
  assert(nUrls >= 3);
  assert(!strncmp(urls[0], "http://host.tv/meuportal/server/load.php?", 41));
  assert(!strncmp(urls[1], "http://host.tv/meuportal/portal.php?", 36));
  assert(!strncmp(urls[2], "http://host.tv/meuportal/stalker_portal/server/load.php?", 56));
  assert(!strcmp(primeiroRef, "Referer: http://host.tv/meuportal/c/"));
  stalker_definir_portal("host.tv/stalker_portal/c/");
  nUrls = 0;
  stalker_canais(sai, 4);
  { int i; for (i = 0; i < nUrls; i++) assert(!strstr(urls[i], "stalker_portal/stalker_portal")); }
  assert(nUrls >= 2);
  assert(!strncmp(urls[0], "http://host.tv/stalker_portal/server/load.php?", 46));
  assert(!strncmp(urls[1], "http://host.tv/stalker_portal/portal.php?", 41));
  stalker_definir_portal("host.tv:8080");
  nUrls = 0;
  stalker_canais(sai, 4);
  assert(nUrls >= 3 && strstr(urls[2], "http://host.tv:8080/stalker_portal/server/load.php?") == urls[2]);
  puts("ok  rotas com prefixo, sem duplicar stalker_portal");
  // Fallback: prefixo nao responde, a raiz sim -> sessao inteira na raiz.
  stalker_definir_portal("host.tv:8080/whatever/c/");
  okPrefixo = "http://host.tv:8080/server/load.php?";
  nUrls = 0; ultimoRef[0] = 0;
  stalker_canais(sai, 4);
  assert(nUrls >= 4);
  assert(strstr(urls[0], "http://host.tv:8080/whatever/server/load.php?") == urls[0]);
  assert(strstr(urls[2], "http://host.tv:8080/whatever/stalker_portal/") == urls[2]);
  assert(strstr(urls[3], "http://host.tv:8080/server/load.php?") == urls[3]);
  assert(!strcmp(ultimoRef, "Referer: http://host.tv:8080/c/"));
  { int i, naRaiz = 0; for (i = 4; i < nUrls; i++) { assert(!strstr(urls[i], "/whatever")); naRaiz++; } assert(naRaiz >= 1); }
  okPrefixo = NULL;
  // Host so: nada de varredura extra na raiz (sao as mesmas tres rotas).
  stalker_definir_portal("solo.tv");
  nUrls = 0;
  stalker_canais(sai, 4);
  assert(strstr(urls[0], "http://solo.tv/server/load.php?") == urls[0]);
  assert(strstr(urls[2], "http://solo.tv/stalker_portal/server/load.php?") == urls[2]);
  assert(strstr(urls[3], "http://solo.tv/server/load.php?") == urls[3]);   // 2a tentativa (renovacao)
  puts("ok  fallback para a raiz; host so inalterado");
  puts("stalker_portal: tudo ok");
  return 0;
}
