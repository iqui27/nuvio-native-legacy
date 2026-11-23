#include <assert.h>
#include <string.h>
#include "farolanel.h"

#if defined(TEST_ADDON)
#include "../src/addons.c"
void addonstats_registrar(const char *chave, unsigned ms, int respondeu) {
  (void)chave; (void)ms; (void)respondeu;
}
int main(void) {
  FarolEvento e[4];
  espDecisaoMs = 2000;
  registrarLatencia("https://privado/segredo?token=abc", 100, 1);
  assert(!farolanel_ler(e, 4, 0));
  registrarLatencia("https://privado/segredo?token=abc", 2100, 1);
  registrarLatencia("Filme privado", 100, 0);
  assert(farolanel_ler(e, 4, 0) == 2);
  assert(e[0].ev == FE_ADDON_LENTO && e[0].a == 2100 && e[0].b == 0);
  assert(e[1].ev == FE_ADDON_LENTO && e[1].b == 1);
  // "h:<8 hex>": o pseudonimo com sal que o redator do Farol 4 aceita.
  assert(!strncmp(e[0].tok, "h:", 2) && strlen(e[0].tok) == 10);
  assert(strspn(e[0].tok + 2, "0123456789abcdef") == 8);
  assert(!strncmp(e[1].tok, "h:", 2) && strcmp(e[0].tok, e[1].tok));
  return 0;
}
#elif defined(TEST_FONTE)
// streams.c importa perfis_sessao_* por perfisessao.h; o ld64 reprova a
// importacao fraca indefinida se alguma referencia sobreviver ao
// -dead_strip, entao o fixture emite os defaults fracos do proprio
// modulo (a convencao NV_DUBLE_SESSAO — sem stub forte aqui para
// conflitar).
#define NV_DUBLE_SESSAO
#include "../src/streams.c"
int main(void) {
  Stream s = {0};
  FarolEvento e[4];
  s.altura = 2160; s.dolbyVision = 1; s.tamanhoMB = 1024;
  s.badges = badges_bit("v-hdr10") | badges_bit("a-ddp");
  strcpy(s.rotulo, "Filme privado");
  strcpy(s.url, "https://privado/segredo?token=abc");
  farolFonteEscolhida(&s, 1234);
  assert(farolanel_ler(e, 4, 0) == 1);
  assert(e[0].ev == FE_FONTE_ESCOLHIDA && e[0].a == 1234 && e[0].b == 10);
  assert(!strcmp(e[0].tok, "2160p_hdr_dv_eac3"));
  return 0;
}
#elif defined(TEST_DEBRID)
#include "../src/debrid.c"
int main(void) {
  DebridContexto c;
  FarolEvento e[4];
  atomic_store(&habilitado, 1);
  debrid_contexto_capturar(&c);
  debrid_contexto_entrar(&c);
  assert(semPlanoMarcar(0, 1, 403));
  assert(!semPlanoMarcar(0, 1, 403));
  assert(semPlanoMarcar(1, 2, 401));
  assert(farolanel_ler(e, 4, 0) == 2);
  assert(!strcmp(e[0].tok, "sem_plano") && e[0].b == 0 && e[0].a == 403);
  assert(!strcmp(e[1].tok, "recusa") && e[1].b == 1 && e[1].a == 401);
  c.busca++;
  assert(!semPlanoMarcar(2, 1, 403));
  assert(farolanel_ler(e, 4, 0) == 2);
  debrid_contexto_sair();
  return 0;
}
#endif
