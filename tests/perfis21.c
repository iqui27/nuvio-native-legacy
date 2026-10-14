// 2.1 — CRIAR, EDITAR E REMOVER PERFIS DA CONTA, sem SDL e sem rede.
//
// O contrato e o do app oficial (ProfileRepository.kt, lido no app mobile, que
// e quem ja cria e edita perfis):
//
//   criar/editar: RPC sync_push_profiles, corpo
//     { "p_client_max_profiles": <teto do cliente>,
//       "p_profiles": [ { "profile_index", "name", "avatar_color_hex",
//                         "uses_primary_addons", "uses_primary_plugins",
//                         "avatar_id", "avatar_url" } ] }
//     — a lista e INTEIRA: os que ja existem viajam junto com o novo.
//   remover: RPC sync_delete_profile_data, corpo { "p_profile_id": N }.
//
// O que este teste prova:
//   1. perfis_indice_livre: o menor buraco 1..8 e preenchido antes de crescer
//      (a mesma conta do app oficial); lista cheia devolve 0.
//   2. perfis_push_corpo: o corpo sai como o servidor espera — p_client_max_
//      profiles, a lista inteira, avatar_id null e a URL ja resolvida.
//   3. perfis_push_lista / perfis_remover: 1 quando o servidor aceita, -1 sem
//      rede (e o aviso da tela, nao um crash).
//
//   bash tests/perfis21.sh
#include "perfis.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nvtmp.h"
void servidores_perfil_trocou(void) {}
void plugins_perfil_mudou(void) {}

// --- pasta de dados de mentira (o mesmo molde do tests/perfilsel.c) ----------
static const char *pasta(void) {
  const char *p = getenv("NUVIO_TESTE_DIR");
  return p ? p : nvt_dir();
}
static void caminho(char *dst, size_t tam, const char *nome) {
  snprintf(dst, tam, "%s/%s", pasta(), nome);
}
char *dados_ler(const char *nome) {
  char c[512]; long z; char *b; FILE *f;
  caminho(c, sizeof c, nome);
  f = fopen(c, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); z = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)z + 1);
  if (!b) { fclose(f); return NULL; }
  if (fread(b, 1, (size_t)z, f) != (size_t)z) { free(b); fclose(f); return NULL; }
  b[z] = 0; fclose(f); return b;
}
int dados_gravar(const char *nome, const char *conteudo) {
  char c[512]; FILE *f;
  caminho(c, sizeof c, nome);
  f = fopen(c, "wb");
  if (!f) return 0;
  fputs(conteudo, f); fclose(f); return 1;
}
int dados_apagar(const char *nome) {
  char c[512]; caminho(c, sizeof c, nome); return remove(c) == 0;
}
void fil_definir_perfil(int p) { (void)p; }
void fontepref_definir_perfil(int p) { (void)p; }
void legmem_definir_perfil(int p) { (void)p; }
void arteesc_definir_perfil(int p) { (void)p; }
static int addonsDoPrincipal = 1;
int ajustes_addons_do_principal(void) { return addonsDoPrincipal; }

const char *sessao_usuario(void) { return "usuario-de-teste"; }
const char *nuvem_url(void) { return "https://exemplo.invalido"; }
char *nuvem_rpc_com(const char *f, const char *c, const char *b, int *s) {
  (void)f; (void)c; (void)b; if (s) *s = 0; return NULL;
}

// O "servidor" da vez. pushCorpo guarda o corpo de sync_push_profiles e
// removerCorpo o de sync_delete_profile_data, para o teste ler o que o app
// escreveu — corpo malformado (aspa sem escape, campo faltando) falha aqui.
static char pushCorpo[8192];
static char removerCorpo[256];
// A lista que o "servidor" tem AGORA: o push anterior valeu.
static char pushResposta[2048];
static int redeCaida;

char *sessao_rpc(const char *funcao, const char *corpoJson, int *status) {
  const char *r = NULL;
  if (status) *status = 200;
  if (redeCaida) { if (status) *status = 0; return NULL; }
  if (!strcmp(funcao, "get_sync_owner"))
    r = "\"441bf572-0000-4000-8000-000000000000\"";
  else if (!strcmp(funcao, "sync_pull_profiles")) {
    // A lista que o "servidor" tem AGORA: o push anterior valeu.
    return strdup(pushResposta);
  } else if (!strcmp(funcao, "sync_push_profiles")) {
    snprintf(pushCorpo, sizeof pushCorpo, "%s", corpoJson ? corpoJson : "");
    r = "[]";
  } else if (!strcmp(funcao, "sync_delete_profile_data")) {
    snprintf(removerCorpo, sizeof removerCorpo, "%s", corpoJson ? corpoJson : "");
    r = "[]";
  } else if (!strcmp(funcao, "sync_pull_profile_locks")) {
    r = "[]";
  }
  return r ? strdup(r) : NULL;
}

int main(void) {
  PerfisPush l[CONTA_PERFIL_MAX];
  int n, i;

  // 1) O MENOR BURACO, nao o proximo numero. Conta com 1, 2 e 4 ganha o 3 —
  //    a mesma conta do app oficial, e o que impede uma conta de chegar no
  //    teto com buracos no meio.
  snprintf(pushResposta, sizeof pushResposta,
    "[{\"profile_index\":1,\"name\":\"Henrique\",\"avatar_color_hex\":\"#1E88E5\","
    "\"avatar_url\":\"https://exemplo.invalido/av1.png\",\"uses_primary_addons\":true},"
    "{\"profile_index\":2,\"name\":\"Álvaro\",\"avatar_color_hex\":\"#E53935\","
    "\"uses_primary_addons\":false},"
    "{\"profile_index\":4,\"name\":\"Infantil\",\"avatar_color_hex\":\"#43A047\","
    "\"uses_primary_addons\":false}]");
  assert(perfis_puxar() == 3);
  assert(perfis_indice_livre() == 3);

  // 3) O CORPO DO PUSH: p_client_max_profiles, a lista inteira, avatar_id
  //    null (a TV so conhece a URL ja resolvida) e os campos com o nome do
  //    servidor.
  n = perfis_push_atuais(l, CONTA_PERFIL_MAX);
  assert(n == 3);
  assert(!strcmp(l[0].nome, "Henrique") && l[0].usaAddons == 1);
  assert(!strcmp(l[0].avatarUrl, "https://exemplo.invalido/av1.png"));
  // Renomear o 4 e o resultado tem de sair no corpo.
  snprintf(l[2].nome, sizeof l[2].nome, "%s", "Kids");
  assert(perfis_push_lista(l, n) == 1);
  assert(strstr(pushCorpo, "\"p_client_max_profiles\":8"));
  assert(strstr(pushCorpo, "\"profile_index\":4"));
  assert(strstr(pushCorpo, "\"name\":\"Kids\""));
  assert(strstr(pushCorpo, "\"avatar_color_hex\":\"#1E88E5\""));
  assert(strstr(pushCorpo, "\"avatar_id\":null"));
  assert(strstr(pushCorpo, "\"avatar_url\":\"https://exemplo.invalido/av1.png\""));
  assert(strstr(pushCorpo, "\"uses_primary_addons\":true"));
  assert(strstr(pushCorpo, "\"uses_primary_plugins\":true"));
  // A lista inteira viaja: 1, 2 e 4.
  { int ha[9] = {0}; const char *p = pushCorpo;
    while ((p = strstr(p, "\"profile_index\":")) != NULL) {
      ha[atoi(p + 16)] = 1; p++;
    }
    assert(ha[1] && ha[2] && ha[4] && !ha[3]); }

  // 2) A LISTA CHEIA nao tem buraco: 0, e a tela nao oferece mais o +.
  { const char *oito[8] = { "1", "2", "3", "4", "5", "6", "7", "8" };
    char cheia[2048] = "[";
    for (i = 0; i < 8; i++) {
      char linha[160];
      snprintf(linha, sizeof linha,
               "{\"profile_index\":%s,\"name\":\"P%s\",\"avatar_color_hex\":\"#1E88E5\"}%s",
               oito[i], oito[i], i < 7 ? "," : "]");
      strcat(cheia, linha);
    }
    snprintf(pushResposta, sizeof pushResposta, "%s", cheia);
    assert(perfis_puxar() == 8);
    assert(perfis_indice_livre() == 0);
    // E o push dos 8 leva os 8 — nenhum fica de fora.
    n = perfis_push_atuais(l, CONTA_PERFIL_MAX);
    assert(n == 8);
    assert(perfis_push_lista(l, n) == 1);
    { int ha[9] = {0}; const char *p = pushCorpo;
      while ((p = strstr(p, "\"profile_index\":")) != NULL) {
        ha[atoi(p + 16)] = 1; p++;
      }
      for (i = 1; i <= 8; i++) assert(ha[i]); } }

  // 4) O AVATAR VAZIO vira null, nao a string "" — o servidor espera null.
  { PerfisPush um[1];
    memset(um, 0, sizeof um);
    um[0].indice = 5; snprintf(um[0].nome, sizeof um[0].nome, "Novo");
    snprintf(um[0].corHex, sizeof um[0].corHex, "#FB8C00");
    assert(perfis_push_lista(um, 1) == 1);
    assert(strstr(pushCorpo, "\"avatar_url\":null")); }

  // 5) REMOVER: o corpo leva p_profile_id com o INDICE do perfil.
  assert(perfis_remover(4) == 1);
  assert(strstr(removerCorpo, "\"p_profile_id\":4"));
  assert(!strstr(removerCorpo, "p_profiles"));

  // 6) SEM REDE: -1, nao crash, nao 0. A tela mostra "Sem conexão".
  redeCaida = 1;
  assert(perfis_push_lista(l, n) == -1);
  assert(perfis_remover(2) == -1);

  printf("perfis21: tudo ok\n");
  return 0;
}
