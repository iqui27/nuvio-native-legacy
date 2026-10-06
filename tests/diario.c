// O DIARIO (diario.c) sozinho: fusao das fontes na janela de 6 h, rever,
// nota/flags/tags, consultas por titulo/periodo/lista, ouvintes, arquivo por
// perfil e o CSV do Letterboxd. Disco e perfil sao trocados por dubles.
#include "diario.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

// ---- dubles de dados.c e perfis.c
static char pasta[256];
static int perfilAtivo = 1;
int perfis_ativo(void) { return perfilAtivo; }
char *dados_ler(const char *nome) {
  char c[512]; FILE *f; long n; char *b;
  snprintf(c, sizeof c, "%s/%s", pasta, nome);
  f = fopen(c, "rb"); if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)n + 1); fread(b, 1, (size_t)n, f); b[n] = 0; fclose(f);
  return b;
}
int dados_gravar(const char *nome, const char *conteudo) {
  char c[512]; FILE *f;
  snprintf(c, sizeof c, "%s/%s", pasta, nome);
  f = fopen(c, "wb"); if (!f) return 0;
  fputs(conteudo, f); fclose(f); return 1;
}
extern long long diario_relogio_teste;
int diario_nuvem_lote(int perfil, DiarioEntrada *v, int max);
void diario_nuvem_confirmar(int perfil, const DiarioEntrada *v, int n);

static DiarioEntrada ent(const char *id, char orig, long long q) {
  DiarioEntrada e; memset(&e, 0, sizeof e);
  snprintf(e.id, sizeof e.id, "%s", id); e.origem = orig; e.quando = q; e.hora = -1;
  return e;
}

static int evs[8], evN;
static void ouvinte(int perfil, int ev, const DiarioEntrada *e, void *ctx) {
  (void)perfil; (void)e; (*(int *)ctx)++;
  if (ev >= 1 && ev <= 4) evs[ev]++;
  evN++;
}

static int conta(const DiarioEntrada *e, void *c) { (void)e; (*(int *)c)++; return 1; }

int main(void) {
  const long long T0 = 1790000000LL;   // out/2026
  char linha[512];
  DiarioEntrada e, v[16];
  DiarioTitulo t;
  int n, t2, e2, ctx = 0;
  long long q;

  snprintf(pasta, sizeof pasta, "%s/nuvio-diario-teste-%d",
           getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (int)getpid());
  snprintf(linha, sizeof linha, "rm -rf '%s' && mkdir -p '%s'", pasta, pasta);
  assert(system(linha) == 0);
  diario_relogio_teste = T0 + 100000;
  diario_iniciar();
  assert(diario_ouvir(ouvinte, &ctx));

  // separar id
  { char id[32]; diario_separar_id("tt123:2:5", id, sizeof id, &t2, &e2);
    assert(!strcmp(id, "tt123") && t2 == 2 && e2 == 5);
    diario_separar_id("kitsu:44:1:3", id, sizeof id, &t2, &e2);
    assert(!strcmp(id, "kitsu:44") && t2 == 1 && e2 == 3);
    diario_separar_id("tmdb:99", id, sizeof id, &t2, &e2);
    assert(!strcmp(id, "tmdb:99") && t2 == 0 && e2 == 0); }

  // regras puras
  assert(diario_nota_da_reacao(1) == 8 && diario_nota_da_reacao(0) == 5 && diario_nota_da_reacao(-1) == 2);
  assert(diario_regra_confianca('p') > diario_regra_confianca('t'));
  assert(diario_regra_confianca('t') > diario_regra_confianca('c'));
  assert(diario_regra_confianca('c') > diario_regra_confianca('l'));

  // 1. FUSAO: conta (c) e player (p) do mesmo filme em 2 h = uma vez; o player manda.
  e = ent("tt0001", 'c', T0); e.flags = DIARIO_F_DATA_APROX; e.nota10 = 7;
  assert(diario_adicionar(0, &e) == 1);
  e = ent("tt0001", 'p', T0 + 7200); e.seg = 5400; e.hora = 5 * 24 + 22; e.cor = 0x112233;
  assert(diario_adicionar(0, &e) == 0);
  assert(diario_n(0) == 1);
  assert(diario_entrada(0, 0, &e));
  assert(e.origem == 'p' && e.quando == T0 + 7200 && e.seg == 5400 && e.nota10 == 7);
  assert(!(e.flags & DIARIO_F_DATA_APROX) && e.cor == 0x112233 && e.hora == 5 * 24 + 22);
  // Trakt depois (menos confiavel) nao tira a data do player.
  e = ent("tt0001", 't', T0 + 3600);
  assert(diario_adicionar(0, &e) == 0);
  assert(diario_entrada(0, 0, &e) && e.origem == 'p' && e.quando == T0 + 7200);

  // 2. REVER: 7 h depois e outra vez.
  e = ent("tt0001", 'p', T0 + 7200 + DIARIO_JANELA_S + 1);
  assert(diario_adicionar(0, &e) == 1);
  assert(diario_n(0) == 2);
  assert(diario_entrada(0, 1, &e) && (e.flags & DIARIO_F_REVER));
  assert(diario_entrada(0, 0, &e) && !(e.flags & DIARIO_F_REVER));
  // Uma vez ANTERIOR chegando depois (import) vira a primeira; a outra vira rever.
  e = ent("tt0001", 'l', T0 - 86400 * 30);
  assert(diario_adicionar(0, &e) == 1);
  assert(diario_entrada(0, 0, &e) && e.quando == T0 - 86400 * 30 && !(e.flags & DIARIO_F_REVER));
  assert(diario_entrada(0, 1, &e) && (e.flags & DIARIO_F_REVER));
  assert(diario_titulo(0, "tt0001", &t) && t.vezes == 3 && t.nota10 == 7 && t.tipo == 'm');
  assert(t.primeiro == T0 - 86400 * 30);

  // 3. Episodios: id com :T:E e tipo serie.
  e = ent("tt0900:1:1", 'p', T0 + 1000); assert(diario_adicionar(0, &e) == 1);
  e = ent("tt0900:1:2", 'p', T0 + 4000); assert(diario_adicionar(0, &e) == 1);
  e = ent("tt0900", 'c', T0 + 5000); e.temporada = 2; e.episodio = 1; assert(diario_adicionar(0, &e) == 1);
  assert(diario_titulo(0, "tt0900", &t) && t.tipo == 's' && t.episodios == 3);
  assert(diario_episodio(0, "tt0900", 1, 2, &q) == 1 && q == T0 + 4000);
  assert(diario_episodio(0, "tt0900", 1, 3, &q) == 0);
  assert(diario_ultimo_episodio(0, "tt0900:9:9", &t2, &e2, &q) && t2 == 2 && e2 == 1);
  assert(diario_por_titulo(0, "tt0900", NULL, 0) == 3);
  assert(diario_por_titulo(0, "tt0900", v, 2) == 3 && v[0].episodio == 1 && v[1].episodio == 2);

  // 4. Periodo e lista ("por pessoa").
  n = diario_por_periodo(0, T0, T0 + 6000, v, 16);
  assert(n == 3);   // tt0900 x3; o tt0001 foi para T0+7200 na fusao
  { const char *ids[] = { "tt0001", "tt7777", "tt0900" }; DiarioTitulo s[3];
    assert(diario_por_lista(0, ids, 3, s) == 2 && s[0].vezes == 3 && s[1].vezes == 0 && s[2].vezes == 3); }
  { DiarioFiltro f; memset(&f, 0, sizeof f); f.tipo = 's'; int c = 0;
    assert(diario_percorrer(0, &f, conta, &c) == 3 && c == 3);
    memset(&f, 0, sizeof f); f.comFlags = DIARIO_F_REVER; c = 0;
    assert(diario_percorrer(0, &f, conta, &c) == 2); }
  assert(diario_n_titulos(0) == 2);

  // 5. Nota, flags, tags, data, apagar.
  assert(diario_avaliar(0, "tt0001", 'm', 9));
  assert(diario_titulo(0, "tt0001", &t) && t.nota10 == 9);
  assert(diario_avaliar(0, "tt5555", 'm', 6));             // sem entrada: cria 'm' aproximada
  assert(diario_titulo(0, "tt5555", &t) && t.vezes == 1 && (t.flags & DIARIO_F_DATA_APROX));
  assert(diario_definir_flags(0, "tt0001", 0, 0, 0, DIARIO_F_CORACAO | DIARIO_F_REVER, 0));
  assert(diario_por_titulo(0, "tt0001", v, 16) == 3 && (v[2].flags & DIARIO_F_CORACAO));
  assert(v[2].flags & DIARIO_F_REVER);   // REVER nao e mexido por fora
  assert(diario_definir_tags(0, "tt0001", 0, 0, 0, (1u << DIARIO_TAG_CHOREI) | (1u << DIARIO_TAG_FINAL)));
  assert(!diario_definir_nota(0, "tt0001", 0, 0, 0, 11));
  assert(diario_definir_data(0, "tt0900", 2, 1, T0 + 5000, T0 + 9000));
  assert(diario_episodio(0, "tt0900", 2, 1, &q) == 1 && q == T0 + 9000);
  assert(diario_apagar(0, "tt5555", 0, 0, 0));
  assert(!diario_titulo(0, "tt5555", &t));
  assert(!diario_apagar(0, "tt5555", 0, 0, 0));

  // 6. Sono PARCIAL nao conta como vez nem como episodio.
  e = ent("tt0900:3:1", 'p', T0 + 20000); e.flags = DIARIO_F_PARCIAL | DIARIO_F_DORMIU_SUS; e.dormiuSeg = 1500;
  assert(diario_adicionar(0, &e) == 1);
  assert(diario_episodio(0, "tt0900", 3, 1, NULL) == 0);
  assert(diario_ultimo_episodio(0, "tt0900", &t2, &e2, NULL) && t2 == 2);
  // ...ate a pessoa assistir de verdade dentro de 6 h: a vez completa ganha.
  e = ent("tt0900:3:1", 'p', T0 + 22000);
  assert(diario_adicionar(0, &e) == 0);
  assert(diario_episodio(0, "tt0900", 3, 1, NULL) == 1);
  assert(diario_por_titulo(0, "tt0900:3:1", NULL, 0) == 4);

  // 7. Ouvintes: so no diario_passo.
  ctx = 0; evN = 0; memset(evs, 0, sizeof evs);
  diario_passo();
  assert(ctx > 0 && evs[DIARIO_EV_NOVA] > 0 && evs[DIARIO_EV_APAGADA] == 1);
  ctx = 0; diario_passo(); assert(ctx == 0);

  // 8. Lote = um RECARREGADO.
  { DiarioEntrada lote[3]; unsigned g = diario_geracao(0);
    lote[0] = ent("tt2001", 'l', T0 - 1000); lote[0].flags = DIARIO_F_IMPORTADO; lote[0].nota10 = 10;
    lote[1] = ent("tt2002", 'l', T0 - 2000);
    lote[2] = ent("tt2001", 'l', T0 - 900);   // mesma vez do [0]
    assert(diario_adicionar_lote(0, lote, 3) == 2);
    assert(diario_geracao(0) != g);
    ctx = 0; memset(evs, 0, sizeof evs);
    diario_passo();
    assert(evs[DIARIO_EV_RECARREGADO] == 1 && evs[DIARIO_EV_NOVA] == 0); }

  // 9. Perfis separados.
  perfilAtivo = 2;
  assert(diario_n(0) == 0 && diario_n(1) > 0);
  e = ent("tt0001", 'p', T0); e.comPerfis = 1; e.flags = DIARIO_F_JUNTOS;
  assert(diario_adicionar(0, &e) == 1);
  assert(diario_n(2) == 1);
  perfilAtivo = 1;

  // 10. Arquivo: grava, esquece, rele igual.
  { int n1 = diario_n(1), n2 = diario_n(2); DiarioEntrada a, b;
    assert(diario_entrada(1, 3, &a));
    diario_gravar_agora();
    diario_esquecer();
    assert(diario_n(1) == n1 && diario_n(2) == n2);
    assert(diario_entrada(1, 3, &b) && !memcmp(&a, &b, sizeof a)); }

  // 11. Linha e CSV.
  e = ent("tt0111161", 'p', T0); e.nota10 = 9; e.tmdb = 278; e.flags = DIARIO_F_REVER;
  e.tags = (1u << DIARIO_TAG_FINAL); e.tipo = 'm';
  assert(diario_linha(&e, linha, sizeof linha) > 0);
  { DiarioEntrada b; assert(diario_ler_linha(linha, &b) && !memcmp(&e, &b, sizeof e)); }
  assert(!diario_ler_linha("lixo", &e) && !diario_ler_linha("# x", &e));
  e = ent("tt0111161", 'p', T0); e.nota10 = 9; e.tmdb = 278; e.flags = DIARIO_F_REVER;
  e.tags = (1u << DIARIO_TAG_FINAL);
  assert(diario_csv_letterboxd(&e, "Um \"Sonho\", de Liberdade", 1994, 1, linha, sizeof linha) > 0);
  assert(!strncmp(linha, "Title,Year,imdbID,tmdbID,WatchedDate,Rating10,Rewatch,Tags\n", 59));
  assert(strstr(linha, "\"Um \"\"Sonho\"\", de Liberdade\",1994,tt0111161,278,2026-"));
  assert(strstr(linha, ",9,true,ending\n"));

  // 12. Cofre: ligar marca tudo; confirmar limpa; editar marca de novo.
  diario_nuvem_definir(1, DIARIO_NUVEM_SIM);
  assert(diario_nuvem_estado(1) == DIARIO_NUVEM_SIM);
  n = diario_nuvem_pendentes(1);
  assert(n == diario_n(1));
  { DiarioEntrada lote[200]; int k = diario_nuvem_lote(1, lote, 200);
    diario_nuvem_confirmar(1, lote, k); }
  assert(diario_nuvem_pendentes(1) == 0);
  assert(diario_definir_nota(1, "tt0001", 0, 0, 0, 4));
  assert(diario_nuvem_pendentes(1) == 1);
  diario_esquecer();
  assert(diario_nuvem_estado(1) == DIARIO_NUVEM_SIM && diario_nuvem_pendentes(1) == 1);

  // 13. Teto.
  perfilAtivo = 3;
  { static DiarioEntrada muitos[DIARIO_MAX + 50]; int i;
    for (i = 0; i < DIARIO_MAX + 50; i++) { muitos[i] = ent("tt3000", 'p', T0 + (long long)i * DIARIO_JANELA_S * 2); }
    diario_adicionar_lote(0, muitos, DIARIO_MAX + 50);
    assert(diario_n(0) == DIARIO_MAX);
    assert(diario_entrada(0, 0, &e) && e.quando == T0 + 50LL * DIARIO_JANELA_S * 2);
    assert(!(e.flags & DIARIO_F_REVER)); }

  // 14. Importacao grande do Trakt (mais nova primeiro, titulos distintos):
  // tem de ser O(n log n), nao O(n^2). 20 mil entradas em menos de 1 s.
  perfilAtivo = 4;
  { static DiarioEntrada muitos[DIARIO_MAX]; int i; clock_t c0 = clock();
    for (i = 0; i < DIARIO_MAX; i++) {
      char id[32]; snprintf(id, sizeof id, "tt%07d:%d:%d", i / 10, 1 + i % 3, 1 + i % 10);
      muitos[i] = ent(id, 't', T0 - (long long)i * 600);
    }
    diario_adicionar_lote(0, muitos, DIARIO_MAX);
    assert(diario_n(0) == DIARIO_MAX);
    assert(diario_n_titulos(0) == DIARIO_MAX / 10);
    assert((double)(clock() - c0) / CLOCKS_PER_SEC < 1.0); }
  perfilAtivo = 1;

  snprintf(linha, sizeof linha, "rm -rf '%s'", pasta);
  system(linha);
  printf("diario: ok\n");
  return 0;
}
