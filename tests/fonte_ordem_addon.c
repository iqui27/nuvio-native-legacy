// #400: duas respostas fora de ordem; exercita a montagem real da folha.
#include <assert.h>
#include "../src/streams.c"
#include "stub_player_source_actions.h"

int ajustes_fonte_prioridade(void) { return 0; }
int fontefalha_addon_ao_vivo(const char *key) { (void)key; return 0; }
int xtream_url_limitada(const char *url) { (void)url; return 0; }
static int modo;
static int negativa, consultas;
int debrid_resolver_cache(const char *h, int f, char *u, unsigned t, int *fora) {
  (void)h; (void)f; (void)u; (void)t; consultas++; *fora = negativa; return 0;
}
int rede_url_final_tipo(const char *u, int s, const char *const *c, char *d,
                        unsigned t, int *st, char *m, unsigned mt, long *b) {
  (void)u; (void)s; (void)c; (void)d; (void)t; (void)st; (void)m; (void)mt; (void)b;
  assert(!"esta fixture so consulta torrent"); return 0;
}
char *rede_baixar_st(const char *u, int s, const char *const *c, int *st) {
  (void)u; (void)s; (void)c; (void)st; assert(!"sem rede nesta fixture"); return NULL;
}
int ajustes_selos_coloridos(void) { return 0; }
int ajustes_idioma(void) { return IDIOMA_EN; }
int fontefalha_filtro_ativo(void) { return 0; }
void fontefalha_filtro(FonteFiltroConfig *saida) { memset(saida, 0, sizeof *saida); }
int perfis_sessao_atual(uint64_t geracao) { (void)geracao; return 1; }
void selospacote_colorido(int ligado) { (void)ligado; }
int selospacote_embutidos_ok(void) { return 0; }
const SeloFiltro *selospacote_filtro(unsigned short id) { (void)id; return NULL; }
const char *i18n(const char *s) { return s; }
int vazao_url_aviso(const char *url) { (void)url; return 0; }
int ajustes_fonte_ordem_addon(void) { return modo; }
int ajustes_fonte_texto_addon(void) { return 0; }
int debrid_ativo(void) { return 1; }
int p2p_ativo(void) { return 0; }
const char *ajustes_p2p_url(void) { return ""; }
float gfx_escala_ui(void) { return 1; }
float txt_bloco(TxtEstilo e, const char *s, int r, int g, int b,
                float x, float y, float w, float leading, float alpha, int max) {
  (void)e; (void)s; (void)r; (void)g; (void)b;
  (void)x; (void)y; (void)w; (void)leading; (void)alpha; (void)max;
  abort(); // This ordering fixture has no discarded-torrent footer to measure.
}
int selospacote_ativo(void) { return -1; }
unsigned selospacote_versao(void) { return 1; }
int selospacote_casar(const char *const *p, int q, unsigned short *o, int m) {
  (void)p; (void)q; (void)o; (void)m; return 0;
}
int ondever_n(const char *id) { (void)id; return 0; }
Uint32 SDL_GetTicks(void) { return 1000; }
char *dados_ler(const char *nome) { (void)nome; return NULL; }
int dados_gravar(const char *nome, const char *s) { (void)nome; (void)s; return 1; }

static void conferir(const int *esperada, int qtd) {
  montar(-1);
  assert(nOrdem == qtd);
  for (int i = 0; i < qtd; i++) {
    if (ordem[i] != esperada[i]) {
      fprintf(stderr, "FAIL modo=%d linha=%d: indice=%d esperado=%d\n", modo, i, ordem[i], esperada[i]);
      exit(1);
    }
  }
  if (modo) for (int g = 0; g < FOLHA_GRUPOS; g++) assert(secN[g] == 0);
}
int main(void) {
  assert(stream_texto_fora_de_cache("Not cached"));
  assert(!stream_texto_fora_de_cache("cached WEB-DL Download"));
  const char *json = "{\"streams\":["
    "{\"url\":\"https://video.invalid/720.mp4\",\"name\":\"720p\",\"videoSize\":104857600},"
    "{\"url\":\"https://video.invalid/4k-small.mp4\",\"name\":\"2160p\",\"videoSize\":209715200},"
    "{\"url\":\"https://video.invalid/4k-large.mkv\",\"name\":\"2160p\",\"videoSize\":419430400}]}";
  Stream *a = NULL, *b = NULL;
  assert(stream_extrair(json, "Addon A", &a) == 3);
  assert(stream_extrair(json, "Addon B", &b) == 3);
  // B responde primeiro, embora A tenha sido instalado antes.
  stream_lista_acrescentar(b, 3, 1);
  stream_lista_acrescentar(a, 3, 0);
  free(a); free(b);
  filtro = 0; atualizarProvedores(); melhorFolha = -1;
  const int qualidade[] = {5,2,4,1,3,0}, addon[] = {3,4,5,0,1,2};
  modo = 0; conferir(qualidade, 6);
  melhorFolha = 0; const int promovida[] = {0,5,2,4,1,3};
  conferir(promovida, 6);
  modo = 1; conferir(addon, 6);
  // Nem melhor fonte nem StreamFit podem retirar uma fonte da ordem do addon.
  melhorFolha = 2; fitAtualizar(); fitClasses[4] = SF_PESADA;
  conferir(addon, 6);
  filtro = 1; const int apenasA[] = {3,4,5}; conferir(apenasA, 3);
  soMp4 = 1; const int mp4A[] = {3,4}; conferir(mp4A, 2);
  soMp4 = 0; filtro = 0; melhorFolha = -1; fitClasses[4] = SF_DESCONHECIDA;
  modo = 0; conferir(qualidade, 6);
  stream_definir_lista(NULL, 0); modo = 1; conferir(NULL, 0);
  // #402: cache negativo conhecido vai ao fim, desconhecido conserva a ordem.
  const char *mista = "{\"streams\":["
    "{\"infoHash\":\"a\",\"name\":\"⏳ 2160p\"},"
    "{\"url\":\"https://video.invalid/720.mp4\",\"name\":\"720p\"},"
    "{\"infoHash\":\"b\",\"name\":\"1080p\"},"
    "{\"infoHash\":\"c\",\"name\":\"⚡ 1080p cached\"}]}";
  assert(stream_extrair(mista, "NMR | ALL", &a) == 4);
  assert(a[0].foraCache && !a[2].foraCache && !a[3].foraCache);
  stream_definir_lista(a, 4); free(a);
  atualizarProvedores(); melhorFolha = -1;
  // Modo Nuvio: fora do cache no fim do PROPRIO grupo de resolucao (38f3dd35);
  // modo Do addon: no fim da lista inteira.
  const int mistaQ[] = {0,2,3,1}, mistaA[] = {1,2,3,0};
  modo = 0; conferir(mistaQ, 4);
  melhorFolha = 0; conferir(mistaQ, 4); // nem a promocao pode ressuscita-la
  modo = 1; conferir(mistaA, 4);
  assert(!stream_automatico_disponivel(0));
  assert(stream_automatico_disponivel(2) && stream_automatico_disponivel(3));
  assert(temMarca(0, -1)); // selo tambem no modo de texto do addon
  // Campo estruturado sem marca textual: false e diferente de ausente/true.
  assert(stream_extrair("{\"streams\":["
    "{\"name\":\"2160p\",\"clientResolve\":{\"infoHash\":\"x\",\"isCached\":false}},"
    "{\"name\":\"720p\",\"clientResolve\":{\"infoHash\":\"y\",\"isCached\":true}},"
    "{\"name\":\"1080p\",\"infoHash\":\"z\",\"behaviorHints\":{\"bingeGroup\":\"aio|1080p\"}}]}",
    "AIOStreams", &a) == 3);
  assert(a[0].foraCache && !a[1].foraCache && !a[2].foraCache);
  stream_definir_lista(a, 3); free(a); melhorFolha = -1;
  atualizarProvedores();
  const int hintQ[] = {0,2,1}, hintA[] = {1,2,0};
  modo = 0; conferir(hintQ, 3); modo = 1; conferir(hintA, 3);
  Conferencia c = {0}; c.geracao = listaGeracao;
  negativa = 0; assert(!verificarUma(2, &c)); // erro/sem resposta nao e negativa
  assert(!lista[2].foraCache && consultas == 1);
  negativa = 1; assert(!verificarUma(2, &c));
  assert(lista[2].foraCache && consultas == 2 && !stream_automatico_disponivel(2));
  assert(!verificarUma(2, &c) && consultas == 2); // nao consulta de novo
  const int consultadaQ[] = {0,2,1}, consultadaA[] = {1,0,2};
  modo = 0; conferir(consultadaQ, 3); modo = 1; conferir(consultadaA, 3);
  puts("fonte_ordem_addon: PASS (qualidade, addon, chegada inversa, filtros, melhor fonte, StreamFit, troca, vazio)");
}
