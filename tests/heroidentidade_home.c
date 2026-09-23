// A ARTE DO DESTAQUE E DO TITULO EM FOCO, NAO DO ANTERIOR (issue #118, 1.4.2).
//
// O desenho do hero pede, no MESMO quadro, a arte do atual (arteA) e a do
// anterior (arteB, para o esvanecimento). As duas saem de arte_por_identidade,
// que no modo automatico devolve artehero_url_episodio ou artehero_url — e as
// duas escreviam num `static char buf[512]` unico. O segundo pedido reescrevia
// o texto para o qual o primeiro ainda apontava: arteA virava a arte do
// ANTERIOR. Na 1.4.1 o desenho usava o item (arteDoItem) e escondia isso; na
// 1.4.2 desenhaArteHero passou a desenhar o `path` (ce3e15e), e o hero de
// Continuar assistindo mostrava o still do titulo de antes — ou "Carregando
// arte" para sempre, quando o still do anterior tinha dado 404 (C9, 23/09:
// Brothers depois de One Piece S23E4).
//
// Sem janela, rede ou TV: inclui src/home.c, como tests/cwremover_home.c.
#include <assert.h>
void cachearte_marcar_grupo(int grupo, const char *url, int variante, int essencial, int emUso) {
  (void)grupo; (void)url; (void)variante; (void)essencial; (void)emUso; }
void cachearte_limpar_referencias_grupo(int grupo) { (void)grupo; }
void cachearte_estatisticas_pedir(void) {}
void tex_cache_marcar_larg(int grupo, const char *url, float larg, int essencial, int emUso) {
  (void)grupo; (void)url; (void)larg; (void)essencial; (void)emUso; }
#include "../src/home.c"
int  tex_largura_fonte(const char *u) { (void)u; return 0; }
const char *tex_arquivo(const char *u) { (void)u; return NULL; }
int  player_aberto(void) { return 0; }
int  detail_aberto(void) { return 0; }
int  trailer_aberto(void) { return 0; }
int  trailer_tocando(void) { return 0; }
void ctx_abrir(int indice) { (void)indice; }
void vertudo_abrir(const char *b, const char *t, const char *c, const char *ti) {
  (void)b; (void)t; (void)c; (void)ti;
}
void vertudo_colecao(const ColFolder *f) { (void)f; }
const char *i18n(const char *s) { return s; }
int  tex_falhou(const char *u) { (void)u; return 0; }
char *dados_ler(const char *nome) { (void)nome; return NULL; }
void dados_marcar_sujo(int leve) { (void)leve; }
const char *ling_legenda(void) { return ""; }
const char *ling_audio(void) { return ""; }
void ling_conta_legenda(const char *v) { (void)v; }
void ling_conta_legenda2(const char *v) { (void)v; }
void ling_conta_audio(const char *v) { (void)v; }
int  perfis_ativo(void) { return 1; }

int main(void) {
  static CatItem it[4];
  static CatFileira fs[1];
  const char *a, *b;
  char esperadoA[512];
  int i;
  ajustes_aplicar_blob("{\"continueWatchingEnabled\":true}");
  // Dois episodios (Continuar assistindo) e dois filmes sem backdrop, que caem
  // no fundo do metahub montado pelo id — os dois caminhos do buffer unico.
  for (i = 0; i < 4; i++) {
    snprintf(it[i].imdb, sizeof it[i].imdb, "tt%07d", 1000 + i);
    snprintf(it[i].titulo, sizeof it[i].titulo, "T%d", i);
    snprintf(it[i].tipo, sizeof it[i].tipo, i < 2 ? "series" : "movie");
    snprintf(it[i].poster, sizeof it[i].poster, "https://p.invalid/%d.jpg", i);
    it[i].progresso = 40;
  }
  it[0].temporada = 1; it[0].episodio = 3;
  it[1].temporada = 5; it[1].episodio = 4;
  snprintf(fs[0].chave, sizeof fs[0].chave, "continue_watching");
  snprintf(fs[0].titulo, sizeof fs[0].titulo, "Continuar assistindo");
  fs[0].ini = 0; fs[0].n = 4;
  cat_definir_tudo(it, 4, fs, 1);

  // A ordem do desenho do hero: atual, depois anterior, e so entao o uso.
  a = arte_por_identidade(0, 2);
  assert(a && strstr(a, "tt0001000/1/3"));
  snprintf(esperadoA, sizeof esperadoA, "%s", a);
  b = arte_por_identidade(1, 2);
  assert(b && strstr(b, "tt0001001/5/4"));
  if (strcmp(a, esperadoA)) {
    fprintf(stderr, "arteA virou a do anterior: %s (esperado %s)\n", a, esperadoA);
    return 1;
  }
  puts("ok  episodio: a arte do atual sobrevive ao pedido do anterior");

  a = arte_por_identidade(2, 2);
  assert(a && strstr(a, "tt0001002"));
  snprintf(esperadoA, sizeof esperadoA, "%s", a);
  b = arte_por_identidade(3, 2);
  assert(b && strstr(b, "tt0001003"));
  if (strcmp(a, esperadoA)) {
    fprintf(stderr, "arteA virou a do anterior: %s (esperado %s)\n", a, esperadoA);
    return 1;
  }
  puts("ok  fundo pelo id: a arte do atual sobrevive ao pedido do anterior");
  puts("heroidentidade: tudo ok");
  return 0;
}
