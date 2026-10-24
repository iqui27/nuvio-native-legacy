// Production transport/publication, with the existing addon fixture's fake
// network. Duplicate display names must keep independent instance identities.
#define main addonslista_legados_main
#define stream_definir_lista addonslista_definir_lista
#define stream_definir_lista_idade addonslista_definir_lista_idade
#define stream_lista_acrescentar addonslista_lista_acrescentar
#include "addonslista.c"
#undef main
#undef stream_definir_lista
#undef stream_definir_lista_idade
#undef stream_lista_acrescentar
#include <assert.h>

static Stream publicadas[8];
static int nPublicadas;
void stream_definir_lista(const Stream *l, int n) {
  assert(n >= 0 && n <= 8);
  nPublicadas = n;
  if (n) memcpy(publicadas, l, sizeof *l * (size_t)n);
}
void stream_definir_lista_idade(const Stream *l, int n, Uint32 idade) {
  (void)idade; stream_definir_lista(l, n);
}
void stream_lista_acrescentar(const Stream *l, int n, int ordem) {
  assert(nPublicadas + n <= 8);
  for (int i = 0; i < n; i++) assert(l[i].addonOrdem == ordem);
  memcpy(publicadas + nPublicadas, l, sizeof *l * (size_t)n);
  nPublicadas += n;
}
#include "../src/addons.c"

static int pluginFixture(const char *id, const char *tipo, int (*cancelado)(void *),
                         void *ctx, OrigemAviso aviso, void *avisoU, void *saida) {
  (void)id; (void)tipo; (void)cancelado; (void)ctx;
  Stream *l = calloc(3, sizeof *l);
  assert(l);
  strcpy(l[0].bingeGroup, "nuvio-plugin|scraper-a|1080");
  strcpy(l[1].bingeGroup, "nuvio-plugin|scraper-a|2160");
  strcpy(l[2].bingeGroup, "nuvio-plugin|scraper-b|720");
  for (int i = 0; i < 3; i++) strcpy(l[i].provedor, "Same display name");
  aviso(avisoU, 7, "Same display name", "scraper-a", 1, NULL, 0);
  aviso(avisoU, 3, "Same display name", "scraper-b", 1, NULL, 0);
  aviso(avisoU, 7, "Same display name", "scraper-a", 2, l, 2);
  aviso(avisoU, 3, "Same display name", "scraper-b", 2, l + 2, 1);
  *(Stream **)saida = l;
  return 3;
}

int main(void) {
  char esperado[2][FONTEFALHA_CHAVE], plugin[2][FONTEFALHA_CHAVE];
  BaldeFonte baldes[2] = {{.idx = 0}, {.idx = 1}};
  Consulta c = { .id = "tt1", .tipo = "movie", .timeout = 1,
                 .baldes = baldes, .nBaldes = 2, .progresso = 1, .rodada = 1 };
  nAddon = 2; memset(addon, 0, sizeof addon);
  for (int i = 0; i < 2; i++) {
    strcpy(addon[i].nome, "AIOStreams");
    addon[i].ativo = addon[i].fonte = 1;
    // Differ only beyond a long shared config prefix, on the SAME host.
    strcpy(addon[i].base, "https://exemplo.test/");
    size_t len = strlen(addon[i].base);
    memset(addon[i].base + len, 'x', 800); len += 800;
    addon[i].base[len++] = (char)('a' + i); addon[i].base[len] = 0;
    fontefalha_addon_chave(addon[i].base, esperado[i], sizeof esperado[i]);
  }
  assert(strcmp(esperado[0], esperado[1]));
  resp[0] = "{\"streams\":[{\"url\":\"https://x/a.mp4\"}]}";
  pthread_mutex_init(&c.trava, NULL);
  progLigado = 1;
  addons_capturar_escopo(&fioEscopo);
  c.sessao = fioEscopo.sessao;
  c.origens = capturarPedidos(c.sessao, &c.nOrigens, &c.versao);
  assert(c.origens && c.nOrigens == 2);
  fioFontes(&c);
  free(c.origens);
  for (int i = 0; i < 2; i++) {
    assert(baldes[i].n == 1);
    assert(!strcmp(baldes[i].achados[0].addonChave, esperado[i]));
    assert(baldes[i].achados[0].addonOrdem == i);
    free(baldes[i].achados);
  }
  pthread_mutex_destroy(&c.trava);
  progDrenar();
  assert(nPublicadas == 2);
  assert(!strcmp(publicadas[0].addonChave, esperado[0]));
  assert(!strcmp(publicadas[1].addonChave, esperado[1]));
  // Reopening the same in-flight target republishes saved partial responses.
  progRepublicar();
  assert(nPublicadas == 2);
  assert(!strcmp(publicadas[0].addonChave, esperado[0]));
  assert(!strcmp(publicadas[1].addonChave, esperado[1]));
  progLimpar(); nPublicadas = 0;

  // Alternate live-TV resource responses take the same stamping route.
  strcpy(addon[0].base, "https://canal.test");
  fontefalha_addon_chave(addon[0].base, esperado[0], sizeof esperado[0]);
  respCanalTv = "{\"streams\":[]}";
  respCanalChannel = "{\"streams\":[{\"url\":\"https://x/channel.m3u8\"}]}";
  memset(&c, 0, sizeof c); memset(baldes, 0, sizeof baldes);
  c.id = "channel1"; c.tipo = "tv";
  c.timeout = 1; c.baldes = baldes; c.nBaldes = 1;
  pthread_mutex_init(&c.trava, NULL);
  c.sessao = perfis_sessao_geracao();
  c.origens = capturarPedidos(c.sessao, &c.nOrigens, &c.versao);
  assert(c.origens);
  fioFontes(&c);
  free(c.origens);
  assert(baldes[0].n == 1 && !strcmp(baldes[0].achados[0].addonChave, esperado[0]));
  assert(baldes[0].achados[0].addonOrdem == 0);
  free(baldes[0].achados); pthread_mutex_destroy(&c.trava);

  // Quality changes do not change a scraper's identity; callback order is
  // preserved in both partial publication and the final/cacheable array.
  PedidoExtra p = { .id = "tt2", .tipo = "movie", .progresso = 1 };
  UmaOrigem o = { .pai = &p, .f = pluginFixture, .indice = 1 };
  fontefalha_addon_chave("plugin:scraper-a", plugin[0], sizeof plugin[0]);
  fontefalha_addon_chave("plugin:scraper-b", plugin[1], sizeof plugin[1]);
  // A revisao de abas sobe a cada estado (anuncio e fim): a folha reavalia
  // mesmo quando a parte terminou sem fontes ou falhou (#205).
  unsigned rev0 = addons_parcial_rev();
  fioUmaOrigem(&o);
  assert(addons_parcial_rev() > rev0);
  assert(o.n == 3 && !p.pendentes);
  assert(!strcmp(o.l[0].addonChave, plugin[0]) && !strcmp(o.l[1].addonChave, plugin[0]));
  assert(!strcmp(o.l[2].addonChave, plugin[1]) && strcmp(plugin[0], plugin[1]));
  assert(o.l[0].addonOrdem == ADD_MAX + 7 && o.l[1].addonOrdem == ADD_MAX + 7);
  assert(o.l[2].addonOrdem == ADD_MAX + 3);
  progDrenar();
  assert(nPublicadas == 3);
  for (int i = 0; i < 3; i++) {
    assert(!strcmp(publicadas[i].addonChave, o.l[i].addonChave));
    assert(publicadas[i].addonOrdem == o.l[i].addonOrdem);
  }
  // Cada scraper tem UMA aba, chaveada pelo id dele (plugin:<id>): as fontes
  // que ele entregou casam com ela e nao abrem uma segunda aba (#205).
  { AddonAba abas[4];
    fioVivo = 1; progLigado = 1; atomic_store(&estado, ADD_BUSCANDO);
    int nab = addons_parcial_abas(abas, 4);
    assert(nab == 2);
    for (int i = 0; i < nab; i++) {
      assert(!strcmp(abas[i].nome, "Same display name"));
      assert(abas[i].estado == ADDON_ABA_PRONTO);
      assert(!strcmp(abas[i].chave, abas[i].ordem == ADD_MAX + 7 ? plugin[0] : plugin[1]));
    }
    fioVivo = 0; atomic_store(&estado, ADD_PARADO);
  }
  free(o.l); progLimpar();
  Stream desconhecida = { .bingeGroup = "plex|Direct Play", .provedor = "AIOStreams" };
  fonteExtraCarimbar(&desconhecida, 1, 2);
  assert(!desconhecida.addonChave[0] && desconhecida.addonOrdem == ADD_MAX + 2);
  puts("addons provenance: duplicate names, full instance base, partial replay, alternate resource and plugin IDs PASS");
  return 0;
}
