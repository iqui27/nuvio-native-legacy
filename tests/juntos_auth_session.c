// Account edges omitted by the isolated Trakt refresh fixture.
#include <stddef.h>
int sessao_identidade_local_copiar(char *user, size_t nu, char *jwt, size_t nj) {
  if (nu) user[0] = 0;
  if (nj) jwt[0] = 0;
  return 0;
}
const char *sessao_usuario(void) { return ""; }
