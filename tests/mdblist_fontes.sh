#!/bin/bash
# #720 (correcao): o seletor Origens do modo Listas tem 9 fontes. Percorre
# origemDados — a funcao REAL de src/biblioteca.c, extraida do fonte como em
# tests/vertudo_estado.sh — nas 9 linhas sob UBSan, e confere o rotulo MDBList
# de rotuloFonte e a i18n das mensagens novas.
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-mdbfontes.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
python3 - "$tmp/fontes.c" <<'PY'
from pathlib import Path
import sys
s = Path('src/biblioteca.c').read_text()
def function(sig):
    start = s.index('static ', s.index(sig) - 20)
    brace = s.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (s[end] == '{') - (s[end] == '}')
        end += 1
    return s[start:end]
code = r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
#define MODO_LISTAS 7
static int modo = MODO_LISTAS;
static int fonte, serverFilter = -1, nCelulas;
static int contaOrigem[4], contaServidor[3];
static struct { char id[96], nome[96]; int count; } addonOrigens[8];
static int nAddonOrigens;
enum { ORIG_TODAS, ORIG_TRAKT, ORIG_SIMKL, ORIG_NUVIO, BIB_N_ORIG };
static const char *ROT_ORIG[BIB_N_ORIG] = { "Todas", "Trakt", "Simkl", "Conta Nuvio" };
enum { FONTE_TRAKT, FONTE_SIMKL, FONTE_MDBLIST, FONTE_NUVIO, FONTE_FIXADAS, FONTE_PUB, FONTE_PLEX, FONTE_JELLYFIN, FONTE_EMBY, BIB_N_FONTES };
static const char *ROT_FONTE[BIB_N_FONTES] =
  { "Trakt", "Simkl", "MDBList", "Nuvio", "Fixadas", "Públicas", "Plex", "Jellyfin", "Emby" };
typedef int (*BibliotecaOrigens)(int index, const char **name, const char **sub, int *count);
static BibliotecaOrigens serverRows;
static int lst_carregando(void) { return 0; }
enum { LST_TRAKT = 0, LST_SIMKL, LST_NUVIO, LST_SERVIDOR, LST_MDB, LST_FONTE_N };
'''
code += function('static const char *rotuloOrigem(int indice, int listas)') + '\n'
code += function('static const char *rotuloFonte(int f)') + '\n'
code += function('static void origemDados(int i,') + '\n'
code += r'''
int main(void) {
  const char *name, *description, *icon, *logo; int count;
  // As nove linhas reais do seletor: nenhuma le fora dos vetores e cada uma
  // devolve o rotulo da sua fonte (Emby era a que abortava no UBSan).
  static const char *rotulos[BIB_N_FONTES] =
    { "Trakt", "Simkl", "MDBList", "Nuvio", "Fixadas", "Públicas", "Plex", "Jellyfin", "Emby" };
  for (int i = 0; i < BIB_N_FONTES; i++) {
    name = description = icon = logo = NULL; count = -2;
    origemDados(i, &name, &description, &count, &icon, &logo);
    assert(name && !strcmp(name, rotulos[i]));
    assert(description && icon);
    if (i >= FONTE_PLEX) assert(logo && strstr(logo, ".png"));
    printf("linha %d: %s / %s / %s\n", i, name, description, icon);
  }
  // Fora dos limites nao le os vetores.
  origemDados(BIB_N_FONTES, &name, &description, &count, &icon, &logo);
  assert(!strcmp(name, "Trakt"));   // rotuloOrigem cai no FONTE_TRAKT
  // O selo da lista aberta e das fixadas: MDBList nao pode sair como NUVIO.
  assert(!strcmp(rotuloFonte(LST_MDB), "MDBLIST"));
  assert(!strcmp(rotuloFonte(LST_TRAKT), "TRAKT"));
  assert(!strcmp(rotuloFonte(LST_NUVIO), "NUVIO"));
  puts("mdblist_fontes: ok");
  return 0;
}
'''
Path(sys.argv[1]).write_text(code)
PY
cc -fsanitize=undefined -fno-sanitize-recover=undefined -O1 -g "$tmp/fontes.c" -o "$tmp/fontes"
"$tmp/fontes"
# prov=mdblist passa pelo portao de credencial proprio, sem client id Trakt:
# carrega a carregarFonte REAL de src/vertudo.c com os servicos duble —
# sem Trakt e sem MDBList a fonte morre; com a chave MDBList ela pede.
python3 - "$tmp/prov.c" <<'PY'
from pathlib import Path
import sys
s = Path('src/vertudo.c').read_text()
def function(sig):
    start = s.index(sig + ' {')
    brace = s.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (s[end] == '{') - (s[end] == '}')
        end += 1
    return s[start:end]
code = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#define NV_ADDON_URL_MAX 1200
typedef struct {
  char title[128], base[600], type[8], catId[96], genre[96], addonId[96];
  char prov[8], tmdbTipo[16]; long tmdbId; char midia[8], ordenar[48], ordem[8];
  long traktLista; int traktPropria; char filtros[1024];
} ColSource;
typedef struct { char id[96], title[128]; ColSource *sources; int nSources; } ColFolder;
// Estado estatico que a funcao toca — so o que o caminho de provedor usa.
static ColSource fonteUnica;
static ColFolder folderUnico = {"pasta", "Pasta", &fonteUnica, 1};
static const ColFolder *collection = &folderUnico;
static int source, vitTudo, vitMulti, ranked, foco, scrollY, velY, orderN,
           servidorN, servidorLidos, celLay, semFonte, lay, vitLin0pag;
static char vitTudoBase[NV_ADDON_URL_MAX], catalogId[96];
static int servidorAtivo, servidorOrdem, servidorGenero;
static char servidorGeneros[8][64];
static int temChaveMdb;
#define VT_LAY_VITRINE 3
static void armarOnda(void) {}
static void genCarregar(const ColSource *s) { (void)s; }
static void limparFocoCatalogo(void) {}
static void resolverLayout(void) {}
static void vitIniciar(void) {}
static int genTem(void) { return 0; }
static const char *genPedido(void) { return ""; }
static int pedidosProvedor, pedidosAddon, paginaPedida;
static const ColSource *pedidoFonte;
static void desc_vertudo_fonte_desde(const ColSource *s, int pagina) {
  pedidosProvedor++; pedidoFonte = s; paginaPedida = pagina;
}
static void desc_vertudo_filtro_desde(const char *b, const char *t, const char *i,
                                      const char *g, int p) {
  (void)b;(void)t;(void)i;(void)g;(void)p; pedidosAddon++;
}
static const char *desc_chave_tmdb(void) { return ""; }
static const char *nuvem_trakt_cliente(void) { return ""; }  // sem Trakt no pacote
static int extras_mdblist_tem_chave(void) { return temChaveMdb; }
static void addons_base_por_id_copiar(const char *id, char *dst, int n) {
  (void)id; if (n) dst[0] = 0;
}
static struct { int pagina; } vitLin[4];
'''
code += function('static const char *baseDaFonte(const ColSource *s)') + '\n'
code += function('static int vitPaginaFonte(void)') + '\n'
code += function('static void carregarFonte(void)') + '\n'
code += r'''
int main(void) {
  memset(&fonteUnica, 0, sizeof fonteUnica);
  snprintf(fonteUnica.prov, sizeof fonteUnica.prov, "mdblist");
  snprintf(fonteUnica.catId, sizeof fonteUnica.catId, "13914");
  snprintf(fonteUnica.title, sizeof fonteUnica.title, "Oscar Winners");
  snprintf(fonteUnica.midia, sizeof fonteUnica.midia, "MOVIE");
  // Sem chave do MDBList a fonte e marcada indisponivel e nao pede —
  // mesmo com o client id do Trakt tambem ausente (o bug: exigia Trakt).
  temChaveMdb = 0;
  carregarFonte();
  assert(semFonte && !pedidosProvedor && !pedidosAddon);
  // Com a chave do MDBList (e AINDA sem client id Trakt) o pedido sai.
  temChaveMdb = 1; semFonte = 0;
  carregarFonte();
  assert(!semFonte && pedidosProvedor == 1 && paginaPedida == 1);
  puts("mdblist_fontes: prov=mdblist sem trakt ok");
  return 0;
}
'''
Path(sys.argv[1]).write_text(code)
PY
cc -O1 -g -Wno-unused-variable "$tmp/prov.c" -o "$tmp/prov"
"$tmp/prov"
# As mensagens novas do caminho MDBList tem chave na tabela mestra.
for k in "Este pacote não tem a chave do MDBList" \
         "A chave do MDBList foi recusada" \
         "Você ainda não criou listas no MDBList" \
         "Esta lista não tem id no MDBList" \
         "O MDBList não está configurado nesta TV."; do
  grep -Fq "{ \"$k\"" src/idioma_tab.h || { echo "i18n ausente: $k" >&2; exit 1; }
done
echo "mdblist_fontes: i18n ok"
