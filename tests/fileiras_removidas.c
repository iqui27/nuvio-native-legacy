// #504/#365: a lista completa da conta, a Home em cache e as duas abas de
// Ajustes. fileiras.c e real; o script inclui os blocos reais de Ajustes e
// descoberta. Somente a conta e o armazenamento temporario sao dubles.
#include "fileiras.h"
#include "colecoes.h"
#include "colfileiras.h"
#include "addons.h"
#include "perfisessao.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <stdatomic.h>

// Fronteira da sessao da conta: esta fixture nao liga perfis.c. O registro
// e suas geracoes de perfil sao reais; a sessao externa capturada e sempre 0.
uint64_t perfis_sessao_geracao(void) { return 0; }
int perfis_sessao_atual(uint64_t geracao) { return geracao == 0; }
int perfis_sessao_travar(uint64_t geracao) { return geracao == 0; }
void perfis_sessao_destravar(void) {}

static const char *diretorio;
static int gravacoes;
int dados_gravar(const char *nome, const char *texto) {
  char caminho[1024]; FILE *f;
  snprintf(caminho, sizeof caminho, "%s/%s", diretorio, nome);
  f = fopen(caminho, "wb"); assert(f);
  assert(fwrite(texto, 1, strlen(texto), f) == strlen(texto));
  assert(!fclose(f)); gravacoes++; return 1;
}
char *dados_caminho(char *saida, unsigned tam, const char *nome) {
  snprintf(saida, tam, "%s/%s", diretorio, nome); return saida;
}
static char *arquivo(int perfil) {
  char caminho[1024], *s; long n; FILE *f;
  snprintf(caminho, sizeof caminho, "%s/fileirasui-p%d.txt", diretorio, perfil);
  f = fopen(caminho, "rb"); assert(f); assert(!fseek(f, 0, SEEK_END));
  n = ftell(f); assert(n >= 0); rewind(f);
  s = calloc((size_t)n + 1, 1); assert(s);
  assert(fread(s, 1, (size_t)n, f) == (size_t)n); fclose(f); return s;
}
static int indice(const char *chave) {
  for (int i = 0; i < fil_n(); i++) if (!strcmp(fil_chave(i), chave)) return i;
  return -1;
}
static int filAba, filLista[FIL_MAX + 1], filListaN, filSep, filFoco;
static int filForaAgrupada = 1;
static int fileirasBusca;
void sync_proteger_ajustes_locais(void) {}
void addons_catalogo_chave(const char *id, const char *base, const char *type,
                           const char *catalog, char *dst, unsigned size) {
  (void)base; snprintf(dst, size, "%s_%s_%s", id, type, catalog);
}
#define AJ_FIL_DESTAQUE (-2)
static int refreshes, remounts;
static const char *loaded[] = { "continue_watching", "social_activity", "vivo_movie_a", "vivo_movie_b" };
static void desc_remontar_fileiras(void) {
  int order[4];
  remounts++;
  assert(fil_unir(loaded, 4, order, 4) == 4);
  fil_espelhar_ordem(loaded, NULL, 4);
}
static void desc_repetir(void) { refreshes++; }
#include "ajustes_fileiras.inc"
#define DECL_MAX 1024
static struct { char chave[FIL_CHAVE]; } decls[DECL_MAX];
static int nDecl, numeroAddons, perfilLista = 1, ativo[ADDONS_MAX];
static unsigned versaoManifestos = 1, versaoLista = 1;
static const char *ids[ADDONS_MAX], *bases[ADDONS_MAX];
int addons_n(void) { return numeroAddons; }
const char *addons_id_manifesto(int i) { return ids[i]; }
const char *addons_base(int i) { return bases[i]; }
int addons_ativo(int i) { return ativo[i]; }
unsigned addons_versao(void) { return versaoLista; }
int addons_perfil_da_lista(void) { return perfilLista; }
// Mesma copia e trava de addons.c, com a lista da conta como fronteira.
static pthread_mutex_t addonTrava = PTHREAD_MUTEX_INITIALIZER;
static struct { char id[96], base[NV_ADDON_URL_MAX]; int ativo; } addon[ADDONS_MAX];
#define nAddon numeroAddons
#include "addons_identidades.inc"
#undef nAddon
static NvPerfilSessao descSessao(void) { return 0; }
static void reconciliar(void) {
  for (int i = 0; i < numeroAddons; i++) {
    snprintf(addon[i].id, sizeof addon[i].id, "%s", ids[i] ? ids[i] : "");
    snprintf(addon[i].base, sizeof addon[i].base, "%s", bases[i] ? bases[i] : "");
    addon[i].ativo = ativo[i];
  }
#include "descoberta_fileiras.inc"
}
static void conta(int instalado, int ligado) {
  numeroAddons = instalado ? 2 : 1;
  ids[0] = "vivo"; bases[0] = "https://vivo.invalid"; ativo[0] = 1;
  ids[1] = "removido"; bases[1] = "https://removido.invalid"; ativo[1] = ligado;
}
static int naAba(const char *chave, int aba) {
  filAba = aba; filMontarLista();
  for (int i = 0; i < filListaN; i++)
    if (filLista[i] >= 0 && !strcmp(fil_chave(filLista[i]), chave)) return 1;
  return 0;
}
static void visivel(const char *chave, int aba) {
  assert(naAba(chave, aba) && !naAba(chave, !aba));
}
static void ausente(const char *chave) {
  assert(!naAba(chave, 0) && !naAba(chave, 1));
}
static void preferencias(const char *antes, int perfil, int escritas) {
  char *depois = arquivo(perfil); assert(!strcmp(antes, depois)); free(depois);
  assert(gravacoes == escritas);
}
static int contar(const char *prefixo, int aba) {
  int n = 0; filAba = aba; filMontarLista();
  for (int i = 0; i < filListaN; i++)
    if (filLista[i] >= 0 && !strncmp(fil_chave(filLista[i]), prefixo, strlen(prefixo))) n++;
  return n;
}

// Conta e ordem sao fronteiras; registro, fontes e publicacao sao reais.
const char *addons_base_por_id(const char *id) {
  return !strcmp(id, "vivo") ? "https://vivo.invalid" :
         !strcmp(id, "removido") ? "https://removido.invalid" : "";
}
const char *sessao_usuario(void) { return "conta-teste"; }
int catordem_n(void) { return 0; }
const char *catordem_chave(int i) { (void)i; return ""; }
int catordem_oculta(const char *chave, const char *outra) { (void)chave; (void)outra; return 0; }
int catordem_unir(const char *const *v, int n, int *saida, int max) {
  (void)v; if (n > max) n = max; for (int i = 0; i < n; i++) saida[i] = i; return n;
}
static void apagado(const char *chave) {
  assert(indice(chave) < 0); ausente(chave);
}
static char *semRemovido(const char *texto) {
  char *saida = calloc(strlen(texto) + 1, 1); const char *p = texto; size_t n = 0;
  assert(saida);
  while (*p) {
    const char *fim = strchr(p, '\n'); size_t tam = fim ? (size_t)(fim - p + 1) : strlen(p);
    const char *achou = strstr(p, "removido_");
    if (!achou || achou >= p + tam) { memcpy(saida + n, p, tam); n += tam; }
    p += tam;
  }
  return saida;
}
static void soRemoveuProprio(const char *antes, int perfil) {
  char *esperado = semRemovido(antes), *depois = arquivo(perfil);
  assert(!strcmp(esperado, depois)); free(esperado); free(depois);
}

int main(int argc, char **argv) {
  char *antes; int escritas, salvo; FilPassada antiga;
  assert(argc == 2); diretorio = argv[1];
  fil_definir_perfil(1); conta(1, 1);
  fil_podar_catalogos(ids, bases, numeroAddons, 1);
  fil_conta_dono(sessao_usuario());
  fil_registrar("vivo_movie_top", "Vivo", "Vivo", "movie", 5);
  fil_registrar("removido_movie_top", "Escolhido", "Removido", "movie", 5);
  fil_registrar("removido_series_top", "Oculto", "Removido", "series", 5);
  fil_registrar("continue_watching", "Continuar", "", "", 5);
  fil_registrar("collection_pasta", "Pasta", "", "", 5);
  salvo = indice("removido_movie_top");
  assert(fil_definir_tipo("removido_movie_top", FIL_TIPO_RANKING));
  fil_ciclar_tam(salvo); fil_remover(indice("removido_series_top"));
  fil_definir_hero_fonte("removido_movie_top");
  assert(fil_mover(salvo, -1) >= 0); fil_gravar_registro(); reconciliar();
  antes = arquivo(1); escritas = gravacoes;

  conta(1, 1); ids[0] = ""; reconciliar();
  visivel("removido_movie_top", 0); visivel("removido_series_top", 1);
  conta(0, 1); versaoLista++; reconciliar();
  visivel("removido_movie_top", 0); versaoLista = versaoManifestos;
  perfilLista = 2; reconciliar(); visivel("removido_movie_top", 0);
  perfilLista = 0; reconciliar(); visivel("removido_movie_top", 0);
  perfilLista = 1; preferencias(antes, 1, escritas);
  puts("ok  lista incompleta, geracao antiga e outro perfil nao apagam catalogos");

  conta(1, 0); reconciliar();
  visivel("removido_movie_top", 1); visivel("removido_series_top", 1);
  ids[1] = ""; reconciliar();
  assert(indice("removido_movie_top") >= 0 && indice("removido_series_top") >= 0);
  preferencias(antes, 1, escritas);
  conta(1, 1); reconciliar(); visivel("removido_movie_top", 0);
  preferencias(antes, 1, escritas);
  puts("ok  addon desligado ou sem identidade conserva suas escolhas e acesso");

  conta(0, 1); reconciliar();
  apagado("removido_movie_top"); apagado("removido_series_top");
  assert(fil_n() == 3 && !fil_hero_fonte()[0]);
  visivel("vivo_movie_top", 0); visivel("continue_watching", 0); visivel("collection_pasta", 0);
  soRemoveuProprio(antes, 1); free(antes); antes = arquivo(1); escritas = gravacoes;
  puts("ok  remocao apaga linhas, preferencias e destaque do arquivo e das duas abas");

  antiga = fil_passada_ler();
  fil_registrar_de(&antiga, "removido_movie_top", "Antigo", "Removido", "movie", 5);
  fil_registrar("removido_series_top", "Antigo", "Removido", "series", 5);
  fil_registrar("removido_movie_nunca_registrado", "Antigo", "Removido", "movie", 5);
  assert(fil_n() == 3); apagado("removido_movie_top");
  assert(fil_n_capacidade() == 1); preferencias(antes, 1, escritas);
  puts("ok  Home em cache nao recria catalogos removidos nem consome suas vagas");

  fil_definir_perfil(2); perfilLista = 2;
  { const char *outro[] = { "segundo" }, *base[] = { "https://segundo.invalid" };
    fil_podar_catalogos(outro, base, 1, 2); }
  fil_registrar("segundo_movie_top", "Segundo", "Segundo", "movie", 5);
  fil_registrar_de(&antiga, "removido_movie_top", "Antigo", "Antigo", "movie", 5);
  assert(fil_n() == 1); visivel("segundo_movie_top", 0);
  fil_definir_perfil(1); perfilLista = 1;
  // O arquivo apagado nao restaura as escolhas no reinicio. A autoridade
  // recarregada rejeita o cache; nao bloqueamos fonte saudavel enquanto falta manifesto.
  assert(fil_n() == 3); apagado("removido_movie_top");
  reconciliar();
  fil_registrar("removido_movie_top", "Antigo", "Removido", "movie", 5);
  fil_registrar_de(&antiga, "atrasado_movie_top", "Antigo", "Antigo", "movie", 5);
  assert(fil_n() == 3); apagado("removido_movie_top"); apagado("atrasado_movie_top");
  { char *depois = arquivo(1); assert(!strcmp(antes, depois)); free(depois); }
  free(antes);
  conta(1, 1); reconciliar();
  fil_registrar("removido_movie_top", "Novo", "Removido", "movie", 5);
  fil_registrar("removido_series_top", "Novo", "Removido", "series", 5);
  salvo = indice("removido_movie_top");
  assert(salvo >= 0 && fil_linha_tipo(salvo) == FIL_TIPO_AUTO && fil_linha_tam(salvo) == FIL_TAM_PADRAO);
  assert(!fil_hero_fonte()[0]); visivel("removido_series_top", 0);
  puts("ok  reinicio nao restaura fantasmas; reinstalacao cria preferencias novas");

  // Tabela cheia: dez entradas realmente saem do arquivo, liberando dez vagas.
  {
    size_t cap = (size_t)FIL_MAX * 180 + 128, n = 0; char *texto = malloc(cap);
    assert(texto);
    n += (size_t)snprintf(texto + n, cap - n, "limite %d\nordem 1\nmigracao 197\n", FIL_LIMITE_MAX);
    for (int i = 0; i < FIL_MAX; i++)
      n += (size_t)snprintf(texto + n, cap - n, "linha %s_movie_%d\t%d\t%d\t1\t0\tTitulo %d\n",
                            i < 10 ? "removido" : "vivo", i, i % 2,
                            i < 10 ? FIL_TIPO_RANKING : FIL_TIPO_AUTO, i);
    assert(n < cap); dados_gravar("fileirasui-p3.txt", texto); free(texto);
    fil_definir_perfil(3); perfilLista = 3; conta(1, 1); reconciliar();
    assert(fil_n() == FIL_MAX); fil_definir_hero_fonte("removido_movie_0"); antes = arquivo(3);
    conta(0, 1); reconciliar();
    assert(fil_n() == FIL_MAX - 10 && !fil_hero_fonte()[0]);
    assert(contar("removido_", 0) == 0 && contar("removido_", 1) == 0);
    assert(contar("vivo_", 0) == (FIL_MAX - 10) / 2 && contar("vivo_", 1) == (FIL_MAX - 10) / 2);
    soRemoveuProprio(antes, 3); free(antes);
    for (int i = 0; i < 10; i++) { char chave[64]; snprintf(chave, sizeof chave, "vivo_series_novo%d", i);
      fil_registrar(chave, "Novo", "Vivo", "series", 5); }
    assert(fil_n() == FIL_MAX);
  }
  puts("ok  registro cheio apaga dez entradas, conserva 758 e aceita dez novas");

  // Fontes com dono explicito saem; colecao mista conserva o outro addon e TMDB.
  {
    const char *json = "{\"collections\":["
      "{\"id\":\"exclusiva\",\"title\":\"So removido\",\"folders\":[{\"id\":\"f1\",\"title\":\"F1\",\"sources\":[{\"addonId\":\"removido\",\"type\":\"movie\",\"catalogId\":\"top\"}]}]},"
      "{\"id\":\"mista\",\"title\":\"Mista\",\"folders\":[{\"id\":\"f2\",\"title\":\"F2\",\"sources\":[{\"addonId\":\"removido\",\"type\":\"movie\",\"catalogId\":\"top\"},{\"addonId\":\"vivo\",\"type\":\"movie\",\"catalogId\":\"top\"},{\"provider\":\"tmdb\",\"tmdbSourceType\":\"DISCOVER\"}]}]},"
      "{\"id\":\"trakt\",\"title\":\"Trakt\",\"folders\":[{\"id\":\"f3\",\"title\":\"F3\",\"sources\":[{\"provider\":\"trakt\",\"traktListId\":1234,\"mediaType\":\"MOVIE\"}]}]}]}";
    fil_definir_perfil(4); perfilLista = 4; conta(1, 1); reconciliar();
    colfileiras_receber(json); colfileiras_sincronizar(); assert(col_n() == 3);
    fil_definir_hero_fonte("collection_exclusiva");
    assert(indice("collection_exclusiva") >= 0);
    conta(0, 1); reconciliar(); colfileiras_sincronizar();
    assert(col_n() == 2 && !fil_hero_fonte()[0]); apagado("collection_exclusiva");
    visivel("collection_mista", 0); visivel("collection_trakt", 0);
    assert(col_folder(0)->nSources == 2);
    for (int i = 0; i < col_n(); i++) for (int j = 0; j < col_folder(i)->nSources; j++)
      assert(strcmp(col_folder(i)->sources[j].addonId, "removido"));
    antes = arquivo(4); assert(!strstr(antes, "collection_exclusiva")); free(antes);
    // Mesmo corpo reaplicado e o rebuild direto nao restauram a fonte apagada.
    colfileiras_receber(json); assert(col_n() == 2); colfileiras_sincronizar();
    assert(col_definir_json(json) == 3); colfileiras_sincronizar(); assert(col_n() == 2);
    apagado("collection_exclusiva");
    fil_definir_perfil(5); fil_definir_perfil(4); perfilLista = 4; conta(0, 1); reconciliar();
    colfileiras_receber(json); colfileiras_sincronizar();
    assert(col_n() == 2); apagado("collection_exclusiva"); assert(!fil_hero_fonte()[0]);
  }
  puts("ok  colecoes removem fontes e pastas vazias; cache e reinicio nao as recriam");
  // Uma lista com 128 donos nao e um retrato truncado dos primeiros dezesseis.
  {
    static char nomes[ADDONS_MAX][24], urls[ADDONS_MAX][64];
    fil_definir_perfil(6); perfilLista = 6; numeroAddons = ADDONS_MAX;
    versaoLista++; versaoManifestos = versaoLista;
    for (int i = 0; i < ADDONS_MAX; i++) {
      snprintf(nomes[i], sizeof nomes[i], "addon%d", i);
      snprintf(urls[i], sizeof urls[i], "https://addon%d.invalid", i);
      ids[i] = nomes[i]; bases[i] = urls[i]; ativo[i] = 1;
    }
    fil_registrar("addon127_movie_top", "Ultimo", "Ultimo", "movie", 5);
    assert(fil_definir_tipo("addon127_movie_top", FIL_TIPO_RANKING));
    fil_definir_hero_fonte("addon127_movie_top"); fil_gravar_registro();
    antes = arquivo(6); escritas = gravacoes; reconciliar();
    assert(indice("addon127_movie_top") >= 0);
    assert(fil_linha_tipo(indice("addon127_movie_top")) == FIL_TIPO_RANKING);
    assert(!strcmp(fil_hero_fonte(), "addon127_movie_top"));
    preferencias(antes, 6, escritas); free(antes);
    assert(colfileiras_receber("{\"collections\":[{\"id\":\"ultimo\",\"title\":\"Ultimo\",\"folders\":[{\"id\":\"f\",\"title\":\"Pasta\",\"sources\":[{\"addonId\":\"addon127\",\"type\":\"movie\",\"catalogId\":\"top\"}]}]}]}") == 1);
    colfileiras_sincronizar(); assert(col_n() == 1);
    assert(col_folder(0)->nSources == 1 && !strcmp(col_folder(0)->sources[0].addonId, "addon127"));
    AddonIdentidade *copia = calloc(ADDONS_MAX, sizeof *copia); assert(copia);
    assert(addons_copiar_identidades(copia, 16, versaoLista, 6) == -1);
    assert(addons_copiar_identidades(copia, ADDONS_MAX, versaoLista, 6) == ADDONS_MAX);
    assert(!strcmp(copia[ADDONS_MAX - 1].id, "addon127")); free(copia);
    unsigned velha = versaoLista++;
    assert(!addons_travar_lista(velha, 6));
    versaoManifestos = versaoLista; ativo[ADDONS_MAX - 1] = 0; reconciliar();
    assert(indice("addon127_movie_top") >= 0 && fil_estado_chave("addon127_movie_top") == FIL_FORA);
    visivel("addon127_movie_top", 1); assert(col_n() == 1);
  }
  puts("ok  lista completa de 128 addons conserva ultimo dono, escolhas e colecao");

  // Autoridade antiga nao e autoridade sobre uma lista nova com manifesto pendente.
  {
    const char *json = "{\"collections\":[{\"id\":\"novo\",\"title\":\"Novo\",\"folders\":[{\"id\":\"f\",\"title\":\"Pasta\",\"sources\":[{\"addonId\":\"removido\",\"type\":\"movie\",\"catalogId\":\"top\"}]}]}]}";
    fil_definir_perfil(7); perfilLista = 7; conta(0, 1);
    versaoLista++; versaoManifestos = versaoLista; reconciliar();
    assert(fil_addon_ausente("removido", ""));
    conta(1, 1); versaoLista++; versaoManifestos = versaoLista; ids[1] = "";
    fil_registrar("removido_movie_top", "Fonte saudavel", "Novo", "movie", 5);
    assert(indice("removido_movie_top") >= 0);
    colfileiras_receber(json); colfileiras_sincronizar(); assert(col_n() == 1);
    reconciliar(); // manifesto de um addon falhou; nao apaga o outro que respondeu
    assert(!fil_addon_ausente("removido", ""));
    assert(indice("removido_movie_top") >= 0 && col_n() == 1);
    conta(1, 1); reconciliar();
    assert(fil_definir_tipo("removido_movie_top", FIL_TIPO_RANKING));
    conta(0, 1); versaoLista++; versaoManifestos = versaoLista; reconciliar();
    colfileiras_sincronizar(); apagado("removido_movie_top"); assert(col_n() == 0);
    conta(1, 1); versaoLista++; versaoManifestos = versaoLista; ids[1] = "";
    fil_registrar("removido_movie_top", "Reinstalado", "Novo", "movie", 5);
    assert(indice("removido_movie_top") >= 0);
    assert(fil_linha_tipo(indice("removido_movie_top")) == FIL_TIPO_AUTO);
    colfileiras_receber(json); colfileiras_sincronizar(); assert(col_n() == 1);
    reconciliar(); assert(indice("removido_movie_top") >= 0 && col_n() == 1);
    conta(1, 1); reconciliar(); assert(col_n() == 1);
    // Identidade ainda desconhecida, inclusive desligada: nunca prova exclusao.
    ids[1] = ""; ativo[1] = 0; reconciliar();
    assert(!fil_addon_ausente("nao_identificado", ""));
    fil_registrar("nao_identificado_movie_top", "Saudavel", "Novo", "movie", 5);
    assert(indice("nao_identificado_movie_top") >= 0);
  }
  puts("ok  troca de lista invalida autoridade antiga sem bloquear manifesto pendente");
  // Fixed rows shift raw indices beyond the catalog quota. Explicit Add must
  // still refresh an eligible missing catalog.
  fil_definir_perfil(8); perfilLista = 8;
  fil_esquecer();
  conta(0, 1); reconciliar();
  fil_definir_limite(3);
  fil_registrar("continue_watching", "Continue", "", "", 9);
  fil_registrar("social_activity", "Friends", "", "", 6);
  fil_registrar("vivo_movie_a", "A", "Vivo", "movie", 5);
  fil_registrar("vivo_movie_b", "B", "Vivo", "movie", 5);
  {
    const char *declared[] = { "vivo_movie_a", "vivo_movie_b" };
    int state, qi;
    fil_marcar_declaradas(declared, 2, 8);
    fil_registrar_se_couber("vivo_movie_quota", "Quota", "Vivo", "movie");
    qi = fil_adicionar(indice("vivo_movie_quota"), &state);
    assert(qi >= fil_limite() && state == FIL_NA_HOME);
    assert(fil_escolhida("vivo_movie_quota") == 2);
    fileirasReagir();
    assert(remounts == 1 && refreshes == 0 && fileirasBusca == 1);
  }
  puts("ok  explicit Add refreshes beyond fixed rows");
  puts("fileiras_removidas: tudo ok"); return 0;
}
