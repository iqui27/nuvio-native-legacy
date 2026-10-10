/* Sem internet: doubles de curl exercitam os caminhos reais de rede.c. */
#include <assert.h>
#include <stdarg.h>
#include "../src/rede.c"
#define OLD "https://api.themoviedb.org/3/movie/1?api_key=x"
#define NEW "https://api.tmdb.org/3/movie/1?api_key=x"
typedef struct { char url[4096]; size_t (*write)(void *, size_t, size_t, void *); void *data; } Fake;
static int erro = 35, http, chamadas, alternativos, falhaAlt;
static void *init(void) { return calloc(1, sizeof(Fake)); }
static void cleanup(void *c) { free(c); }
static int option(void *c, int o, ...) {
  va_list ap; va_start(ap, o);
  if (o == OPT_URL) snprintf(((Fake *)c)->url, 4096, "%s", va_arg(ap, const char *));
  if (o == OPT_WRITEFUNCTION) ((Fake *)c)->write = va_arg(ap, size_t (*)(void *, size_t, size_t, void *));
  if (o == OPT_WRITEDATA) ((Fake *)c)->data = va_arg(ap, void *);
  va_end(ap); return 0;
}
static int perform(void *c) {
  chamadas++;
  if (strstr(((Fake *)c)->url, "https://api.tmdb.org/")) {
    assert(!strcmp(((Fake *)c)->url, NEW)); alternativos++;
    if (!falhaAlt && ((Fake *)c)->write) ((Fake *)c)->write("{}", 1, 2, ((Fake *)c)->data);
    return falhaAlt ? 35 : 0;
  }
  return erro;
}
static int info(void *c, int o, ...) {
  va_list ap; va_start(ap, o);
  if (o == INFO_URL_FINAL) *va_arg(ap, char **) = ((Fake *)c)->url;
  else if (o == INFO_RESPONSE_CODE) *va_arg(ap, long *) = strstr(((Fake *)c)->url, "api.tmdb.org") && !falhaAlt ? 200 : http;
  else if (o == INFO_NUM_CONNECTS) *va_arg(ap, long *) = 1;
  else if (o == INFO_SIZE_DOWNLOAD) *va_arg(ap, double *) = 0;
  va_end(ap); return 0;
}
static void *append(void *l, const char *s) { (void)s; return l ? l : malloc(1); }
static void *outro(void *p) {
  int st; char *r = rede_postar_st(OLD, 5, NULL, "{}", &st);
  assert(r && st == 200); free(r); (void)p; return NULL;
}
int main(void) {
  int st, i; long n; char *r; pthread_t fio;
  pronto = 1; curl_init = init; curl_cleanup = cleanup; curl_setopt_f = option;
  curl_perform = perform; curl_getinfo = info; slist_append = append; slist_free = free;
  /* Outros hosts e respostas HTTP nunca ativam a troca. */
  r = rede_baixar_st("https://other.org/3/movie/1?api_key=x", 5, NULL, &st); free(r);
  r = rede_baixar_st("https://api.themoviedb.org.evil/3/movie/1?api_key=x", 5, NULL, &st); free(r);
  for (i = 0; i < 3; i++) {
    int codigos[] = {401,404,429}; erro = 0; http = codigos[i];
    r = rede_baixar_st(OLD, 5, NULL, &st); assert(st == http); free(r);
  }
  /* Mesmo curl de transporte acompanhado de HTTP nao pode trocar. */
  erro = 56; http = 401; r = rede_baixar_st(OLD, 5, NULL, &st); free(r);
  assert(alternativos == 0);
  memset(neg, 0, sizeof neg); nNeg = 0; memset(recuo, 0, sizeof recuo);
  /* Uma tentativa no alias que tambem falha nao gruda nem entra em loop. */
  { int codigos[] = {35,7,28,56,52};
    http = 0; falhaAlt = 1;
    for (i = 0; i < 5; i++) {
      chamadas = alternativos = 0; erro = codigos[i];
      r = rede_postar_st(OLD, 5, NULL, "{}", &st); assert(!r);
      assert(chamadas == 2 && alternativos == 1);
    }
    chamadas = alternativos = 0; erro = 60; /* certificado: nao contornar */
    r = rede_postar_st(OLD, 5, NULL, "{}", &st); assert(!r);
    assert(chamadas == 1 && alternativos == 0);
    falhaAlt = 0;
  }
  erro = 35; http = 0; chamadas = alternativos = 0;
  r = rede_baixar_bin(OLD, 5, &n); assert(r); free(r);
  assert(chamadas == 3 && alternativos == 1); /* repeticao antiga, depois alias */
  pthread_create(&fio, NULL, outro, NULL); pthread_join(fio, NULL);
  { RedePedido q = {.url=OLD, .prazo_ms=5000}; RedeResposta resp;
    assert(rede_pedir(&q, &resp)); assert(resp.status == 200);
    assert(!strcmp(resp.final, NEW)); rede_resposta_limpar(&resp); }
  assert(chamadas == 5 && alternativos == 3);
  { Fake c = {0};
    const char *outros[] = {"https://other.org/?x=api.themoviedb.org",
      "https://api.themoviedb.org.evil/", "https://api.themoviedb.org:pass@evil/",
      "https://image.tmdb.org/t/p/w500/1.jpg"};
    for (i = 0; i < 4; i++) {
      assert(!urlSetopt(&c, outros[i])); assert(!strcmp(c.url, outros[i]));
    }
    assert(!urlSetopt(&c, "https://api.themoviedb.org:443/3/movie/1?api_key=x"));
    assert(!strcmp(c.url, "https://api.tmdb.org:443/3/movie/1?api_key=x"));
  }
  puts("rede_tmdb: ok"); return 0;
}
