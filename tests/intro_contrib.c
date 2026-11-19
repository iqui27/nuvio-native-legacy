// #494: exercita intro_contrib_registrar de verdade (o codigo do player):
// grava a linha do pulo com o tipo recebido do marcador, recusa janela
// invalida (o bug do fim-como-inicio) e gira o arquivo no teto de tamanho.
// intro.c inteiro vem incluso; as dependencias que o registrador nao usa
// viram stubs (mesmo padrao de tests/intro.c).
#include "../src/intro.c"
#include "../src/rede.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

char *dados_caminho(char *d, unsigned n, const char *name) {
  snprintf(d, n, "%s/%s", getenv("NUVIO_DADOS"), name);
  return d;
}
const char *dados_dir(void) { return getenv("NUVIO_DADOS"); }
void dados_fs_travar(void) {}
void dados_fs_liberar(void) {}
void dados_marcar_sujo(int leve) { (void)leve; }
int rede_pedir(const RedePedido *p, RedeResposta *r) {
  (void)p; memset(r, 0, sizeof *r); r->status = 404; return 0;
}
void rede_resposta_limpar(RedeResposta *r) { (void)r; }
int mkvass_cabecalho(const char *url, unsigned char **buf, long *n) {
  (void)url; (void)buf; (void)n; return 0;
}
char *rede_baixar_trecho_st(const char *url, int s, long long ini, long long fim,
                           long *tam, int *status, int *erro, char *final, unsigned tamFinal) {
  (void)url; (void)s; (void)ini; (void)fim; (void)status; (void)erro; (void)final; (void)tamFinal;
  *tam = 0; return NULL;
}
char *rede_baixar_trecho(const char *url, int s, long ini, long fim, long *tam) {
  (void)url; (void)s; (void)ini; (void)fim; *tam = 0; return NULL;
}
const char *nuvem_simkl_cliente(void) { return ""; }

static FILE *abrir(char *path, size_t n, const char *dir) {
  snprintf(path, n, "%s/skip-contrib-local.jsonl", dir);
  return fopen(path, "r");
}

int main(void) {
  char dir[160], path[256], line[512];
  FILE *f;
  struct stat st;

  snprintf(dir, sizeof dir, "%s/nuvio-contrib-XXXXXX",
           getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
  assert(mkdtemp(dir));
  setenv("NUVIO_DADOS", dir, 1);

  // Pulo de resumo: o caller passa o tipo do marcador (INTRO_RESUMO), nunca o
  // fallback "abertura" de quando o botao nao esta visivel no automatico.
  intro_contrib_registrar("tt123", INTRO_RESUMO, 30000, 90000);
  f = abrir(path, sizeof path, dir);
  assert(f && fgets(line, sizeof line, f));
  assert(strstr(line, "\"tt123\"") && strstr(line, "\"tipo\":2") &&
         strstr(line, "\"ini\":30000") && strstr(line, "\"fim\":90000"));
  fclose(f);

  // A janela invertida do bug (ini = fim devolvido por intro_botao_visivel)
  // continua recusada — agora o caller nao a produz mais.
  { long antes = 0, depois = 0;
    stat(path, &st); antes = st.st_size;
    intro_contrib_registrar("tt123", INTRO_ABERTURA, 90000, 90000);
    intro_contrib_registrar("tt123", INTRO_ABERTURA, 95000, 90000);
    stat(path, &st); depois = st.st_size;
    assert(depois == antes); }

  // Teto de tamanho: um arquivo logo abaixo do limite gira ANTES de estoura-
  // lo — a linha que nao cabe cai inteira no diario novo, sem se perder.
  { long resto;
    assert(stat(path, &st) == 0);
    resto = (INTRO_CONTRIB_MAX - 1) - st.st_size;
    f = fopen(path, "a"); assert(f);
    while (resto-- > 0) fputc('.', f);
    fclose(f);
    assert(stat(path, &st) == 0 && st.st_size == INTRO_CONTRIB_MAX - 1); }
  intro_contrib_registrar("tt999", INTRO_ABERTURA, 1000, 2000);
  { char rot[300]; snprintf(rot, sizeof rot, "%s.1", path);
    assert(stat(rot, &st) == 0 && st.st_size <= INTRO_CONTRIB_MAX); // .1 dentro do teto
    assert(stat(path, &st) == 0 && st.st_size <= INTRO_CONTRIB_MAX); }
  f = fopen(path, "r"); assert(f && fgets(line, sizeof line, f));
  assert(strstr(line, "\"tt999\""));
  fclose(f);

  puts("ok intro_contrib");
  return 0;
}
