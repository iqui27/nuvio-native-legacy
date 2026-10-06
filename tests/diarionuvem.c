// O COFRE DO DIARIO, parte pura: a chave do servidor, o corpo do lote e a
// leitura da restauracao (o formato de servidor/recomendacoes/src/diario.js).
// Rede, sessao, perfil e ilha sao dubles: nada sai daqui.
#include "diarionuvem.h"
#include "ilha.h"
#include "perfis.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int perfis_ativo(void) { return 1; }
const ContaPerfil *perfis_item_ativo(void) { return NULL; }
char *dados_ler(const char *n) { (void)n; return NULL; }
int dados_gravar(const char *n, const char *c) { (void)n; (void)c; return 1; }
const char *i18n(const char *s) { return s; }
int sessao_logada(void) { return 0; }
const char *sessao_token(void) { return ""; }
char *rede_baixar_st(const char *u, int s, const char *const *c, int *st) { (void)u; (void)s; (void)c; *st = 0; return NULL; }
char *rede_postar_st(const char *u, int s, const char *const *c, const char *b, int *st) { (void)u; (void)s; (void)c; (void)b; *st = 0; return NULL; }
void ilha_avisar_ex(const IlhaAvisoEx *a) { (void)a; }
void ilha_avisar(const char *c, int t, const char *i, const char *x, unsigned ms, int k) { (void)c; (void)t; (void)i; (void)x; (void)ms; (void)k; }
int ilha_tem(const char *c) { (void)c; return 0; }

int main(void) {
  DiarioEntrada v[3], w[4];
  char chave[64], corpo[8192];
  int mais; long long ate;
  memset(v, 0, sizeof v);
  snprintf(v[0].id, sizeof v[0].id, "tt0111161"); v[0].origem = 'p'; v[0].quando = 1790000000; v[0].nota10 = 9;
  v[0].flags = DIARIO_F_REVER | DIARIO_F_CORACAO; v[0].hora = 118; v[0].seg = 8000; v[0].cor = 0x112233;
  snprintf(v[1].id, sizeof v[1].id, "tt0900"); v[1].origem = 'c'; v[1].temporada = 2; v[1].episodio = 5;
  v[1].quando = 1790001000; v[1].hora = -1; v[1].flags = DIARIO_F_DATA_APROX;
  diarionuvem_chave(&v[0], chave, sizeof chave); assert(!strcmp(chave, "tt0111161"));
  diarionuvem_chave(&v[1], chave, sizeof chave); assert(!strcmp(chave, "tt0900_s2e5"));
  diario_nome_definir("tt0111161", "Um \"Sonho\" de Liberdade", 1994, "");
  assert(diarionuvem_json_lote(v, 2, corpo, sizeof corpo) > 0);
  assert(strstr(corpo, "\"itens\":[{\"chave\":\"tt0111161\",\"quando\":1790000000,\"nota10\":9,\"flags\":5"));
  assert(strstr(corpo, "\"titulo\":\"Um \\\"Sonho\\\" de Liberdade\",\"ano\":1994"));
  assert(strstr(corpo, "\"chave\":\"tt0900_s2e5\"") && strstr(corpo, "\"origem\":\"c\"") && strstr(corpo, "\"hora\":-1"));
  assert(!diarionuvem_json_lote(v, 2, corpo, 40));     // nao cabe: 0, nunca meio JSON

  { const char *r = "{\"itens\":[{\"chave\":\"tt0900_s2e5\",\"quando\":1790001000,\"nota10\":0,\"flags\":32,"
      "\"tags\":0,\"cor\":0,\"seg\":0,\"hora\":-1,\"origem\":\"c\",\"tmdb\":0,\"dormiu\":0,\"dur\":0,\"com\":0,"
      "\"titulo\":\"Ruptura\",\"ano\":2022,\"link\":\"\",\"atualizado\":5},"
      "{\"chave\":\"tt0111161\",\"quando\":1790000000,\"nota10\":9,\"flags\":133,\"tags\":16,\"cor\":1122867,"
      "\"seg\":8000,\"hora\":118,\"origem\":\"p\",\"tmdb\":278,\"dormiu\":0,\"dur\":8500,\"com\":0,"
      "\"titulo\":\"\",\"ano\":0,\"link\":\"https://boxd.it/abc\",\"atualizado\":7},"
      "{\"chave\":\"kitsu:1_sX\",\"quando\":1790002000,\"nota10\":99,\"origem\":\"l\"}],\"mais\":1,\"ate\":7}";
    char t[64]; int ano;
    assert(diarionuvem_ler_itens(r, w, 4, &mais, &ate) == 3 && mais == 1 && ate == 7);
    assert(!strcmp(w[0].id, "tt0900") && w[0].temporada == 2 && w[0].episodio == 5 && w[0].tipo == 's');
    assert(w[0].flags == DIARIO_F_DATA_APROX && w[0].hora == -1 && w[0].origem == 'c');
    assert(!strcmp(w[1].id, "tt0111161") && w[1].tmdb == 278 && w[1].cor == 0x112233 && w[1].durSeg == 8500);
    assert((w[1].flags & DIARIO_F_REVIEW) && w[1].tags == 16 && w[1].nota10 == 9);
    assert(!strcmp(w[2].id, "kitsu:1_sX") && w[2].temporada == 0 && w[2].nota10 == 0);   // nota invalida zera
    assert(diario_nome("tt0900", t, sizeof t, &ano, NULL, 0) && !strcmp(t, "Ruptura") && ano == 2022); }
  assert(diarionuvem_ler_itens("{\"erro\":\"cofre\"}", w, 4, &mais, &ate) == -1);
  assert(!diarionuvem_disponivel());     // sem sessao: nada sai
  printf("diarionuvem: ok\n");
  return 0;
}
