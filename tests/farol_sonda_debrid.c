// Casos da sonda de revisao do Farol 10 sobre a fixture real de tests/debrid.c:
// UM evento por transicao de estado da conta debrid.
//   rd-chave: RD devolve 401 na consulta de conta -> 1 "recusa" (HTTP 401).
//   rd-plano: conta RD gratuita -> 1 "sem_plano" (HTTP 200).
//   ad:       AllDebrid 200 com AUTH_BAD_APIKEY -> 1 "recusa" (HTTP 200),
//             nao a dupla marcacao semPlano + recusado.
//   fora-cache: um evento por busca, com rearme (ja passava; fica de guarda).
#define main teste_debrid_original
#include "tests/debrid.c"
#undef main
#include "farolanel.h"

static int conferir(int servico, const char *token, int http) {
  char url[4096] = "", estado[96] = "";
  FarolEvento eventos[16];
  int *pedidos = servico == DEBRID_RD ? &contaPedidos[DEBRID_RD] : &pedidosServico[servico];
  int pedidosAntes = *pedidos;
  unsigned n;
  int ocorrencias = 0, camposCorretos = 1;
  assert(!debrid_resolver_cache("DEADBEEF", -1, url, sizeof url, NULL));
  assert(!url[0]);
  n = farolanel_ler(eventos, 16, 0);
  for (unsigned i = 0; i < n; i++) {
    FarolEvento *ev = &eventos[i];
    assert(ev->mod == FM_DEBRID && ev->ev == FE_DEBRID);
    assert(!strstr(ev->tok, "CHAVE") && !strstr(ev->tok, "http") && !strstr(ev->tok, "DEADBEEF"));
    printf("  evento: HTTP=%d servico=%d token=%s\n", ev->a, ev->b, ev->tok);
    if (!strcmp(ev->tok, token) && ev->b == servico) {
      ocorrencias++;
      if (ev->a != http) camposCorretos = 0;
    }
  }
  if (servico == DEBRID_RD) {
    assert(!debrid_testar_conta(servico, estado, sizeof estado, NULL, 0));
    assert(!debrid_servico_disponivel(servico));
    assert(!strcmp(estado, !strcmp(token, "recusa") ? "chave recusada" : "conta sem premium"));
  }
  assert(*pedidos == pedidosAntes + 1);
  // O mesmo pedido de novo nao consulta nem repete evento (TTL da conta).
  assert(!debrid_resolver_cache("DEADBEEF", -1, url, sizeof url, NULL));
  assert(farolanel_ler(eventos, 16, 0) == n && *pedidos == pedidosAntes + 1);
  printf("%s/%s: pedidos=1 estado=%s eventos=%u ocorrencias=%d HTTP=%d\n",
         debrid_servico_id(servico), token, estado, n, ocorrencias, http);
  return ocorrencias != 1 || !camposCorretos || n != 1;
}

static int foraCache(void) {
  FarolEvento eventos[8];
  char url[4096];
  int fora, n = 0;
  debrid_definir_chave("alldebrid", K_AD);
  for (int busca = 0; busca < 2; busca++) {
    debrid_nova_busca();
    for (int tentativa = 0; tentativa < 2; tentativa++) {
      adZera(1, 0, -1, -1);
      assert(!debrid_resolver_servico_cache(DEBRID_AD, "DEADBEEF", -1, url, sizeof url, &fora));
      assert(fora && !url[0] && debrid_fora_de_cache() == tentativa + 1);
      n = farolanel_ler(eventos, 8, 0);
      if (n != busca + 1) return 1;
    }
  }
  for (int i = 0; i < n; i++) {
    assert(eventos[i].ev == FE_DEBRID && eventos[i].b == DEBRID_AD && eventos[i].a == 0);
    assert(!strcmp(eventos[i].tok, "fora_cache"));
  }
  printf("fora_cache: 2 buscas, 2 pedidos por busca, %d eventos\n", n);
  return 0;
}

int main(int argc, char **argv) {
  assert(argc == 2);
  debrid_esquecer();
  farolanel_zerar();
  if (!strcmp(argv[1], "fora-cache")) return foraCache();
  if (!strcmp(argv[1], "ad")) {
    debrid_definir_chave("alldebrid", K_AD);
    adUploadModo = 2;
    return conferir(DEBRID_AD, "recusa", 200);
  }
  debrid_definir_chave("real-debrid", K_RD);
  if (!strcmp(argv[1], "rd-chave")) {
    contaModo = 2;
    return conferir(DEBRID_RD, "recusa", 401);
  }
  assert(!strcmp(argv[1], "rd-plano"));
  contaModo = 5;
  return conferir(DEBRID_RD, "sem_plano", 200);
}
