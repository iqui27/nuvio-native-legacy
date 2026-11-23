// Caso da sonda de revisao do Farol 10: busca SO COM PLUGINS (sem add-on
// Stremio instalado) tem de gravar o evento de conclusao FE_ADDON_BUSCA, e o
// plugin mudo gera um FE_ADDON_LENTO com pseudonimo com sal ("h:<8 hex>").
// Reusa a fixture de tests/autoplay_alvo.c (rede/plugin dublados, addons.c real).
#define main teste_autoplay_original
#include "tests/autoplay_alvo.c"
#undef main
#include "farolanel.h"

#define URL_PRIVADA "https://host-privado.invalid/filme.mkv?token=CANARIO_TOKEN"
#define NOME_PRIVADO "CANARIO_TITULO"

static int pluginSintetico(const char *id, const char *tipo,
                           int (*cancelado)(void *), void *ctx,
                           OrigemAviso aviso, void *avisoU, void *saida) {
  Stream *s = calloc(1, sizeof *s);
  (void)tipo;
  assert(s && aviso && !(cancelado && cancelado(ctx)));
  assert(!strcmp(id, "tt9:1:1"));
  aviso(avisoU, 0, URL_PRIVADA, "teste-mudo", 1, NULL, 0);
  aviso(avisoU, 0, URL_PRIVADA, "teste-mudo", 3, NULL, 0);
  aviso(avisoU, 1, NOME_PRIVADO, "teste-fonte", 1, NULL, 0);
  strcpy(s->url, URL_PRIVADA);
  strcpy(s->rotulo, NOME_PRIVADO);
  strcpy(s->provedor, NOME_PRIVADO);
  aviso(avisoU, 1, NOME_PRIVADO, "teste-fonte", 2, s, 1);
  *(Stream **)saida = s;
  atomic_store(&pluginRodou, 1);
  return 1;
}

int main(void) {
  FarolEvento eventos[16];
  int buscas = 0, lentos = 0, k;
  unsigned total;
  farolanel_zerar();
  addons_definir_origem_extra(pluginSintetico, pluginAtivo);
  addons_buscar("tt9:1:1", "series");
  for (k = 0; k < 3000 && addons_estado() == ADD_BUSCANDO; k++) usleep(1000);
  assert(k < 3000 && atomic_load(&pluginRodou));
  assert(nLista == 1 && !strcmp(listaAtiva[0].url, URL_PRIVADA) &&
         !strcmp(listaAtiva[0].rotulo, NOME_PRIVADO));
  addons_encerrar();
  total = farolanel_ler(eventos, 16, 0);
  for (unsigned i = 0; i < total; i++) {
    const char *tok = eventos[i].tok;
    assert(eventos[i].mod == FM_ADDON);
    assert(!strstr(tok, "host-privado") && !strstr(tok, "CANARIO") && !strstr(tok, "://"));
    if (eventos[i].ev == FE_ADDON_BUSCA) buscas++;
    if (eventos[i].ev == FE_ADDON_LENTO) {
      lentos++;
      assert(!strncmp(tok, "h:", 2) && strlen(tok) == 10);
      assert(strspn(tok + 2, "0123456789abcdef") == 8);
    }
    printf("  evento: ev=%d a=%d b=%d token=%s\n", eventos[i].ev, eventos[i].a, eventos[i].b, tok);
  }
  printf("busca so com plugins: fontes=%d lentos=%d buscas=%d\n", nLista, lentos, buscas);
  free(listaAtiva);
  return buscas != 1 || lentos != 1;
}
