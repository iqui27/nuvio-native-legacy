// O CARD DE "CONTINUAR ASSISTINDO" MOSTRA O STILL DO EPISODIO (relato da
// Shield TV Pro 2019, teste 318.3: "a miniatura do episodio mostra o fundo da
// serie"). O ajuste "Miniatura do episodio" (useEpisodeThumbnailsInCw,
// AJ_CW_THUMB) existia, com ajuda e tudo, mas ajustes_cw_thumb_episodio() nao
// tinha nenhum chamador: o card pedia arte_por_identidade(idx, 1), que e o
// fundo do titulo (arte_por_formato), e o still so ia para o destaque (2).
// O codigo 3 de arte_por_identidade e o card da retomada: still do episodio
// com o ajuste ligado, fundo do titulo com ele desligado, em filme, ou quando
// o cache ja sabe que o still nao vem (404 do metahub).
//
// Sem janela, rede ou TV: inclui src/home.c, como tests/heroidentidade_home.c.
#include <assert.h>
void cachearte_marcar_grupo(int grupo, const char *url, int variante, int essencial, int emUso) {
  (void)grupo; (void)url; (void)variante; (void)essencial; (void)emUso; }
void cachearte_limpar_referencias_grupo(int grupo) { (void)grupo; }
void cachearte_estatisticas_pedir(void) {}
void tex_cache_marcar_larg(int grupo, const char *url, float larg, int essencial, int emUso) {
  (void)grupo; (void)url; (void)larg; (void)essencial; (void)emUso; }
#include "../src/home.c"
void desc_sinopse_hero(const int *idx, int n) { (void)idx; (void)n; }
int  tex_largura_fonte(const char *u) { (void)u; return 0; }
const char *tex_arquivo(const char *u) { (void)u; return NULL; }
int  player_aberto(void) { return 0; }
int  detail_aberto(void) { return 0; }
int  trailer_aberto(void) { return 0; }
int  trailer_tocando(void) { return 0; }
void ctx_abrir(int indice) { (void)indice; }
void ctx_fileira(const char *c, const char *t) { (void)c; (void)t; }
void ctx_dispensar_retomar(int on) { (void)on; }
void ctx_abrir_fileira(const char *c, const char *t) { (void)c; (void)t; }
void vertudo_abrir(const char *b, const char *t, const char *c, const char *ti) {
  (void)b; (void)t; (void)c; (void)ti;
}
void vertudo_colecao(const ColFolder *f) { (void)f; }
const char *i18n(const char *s) { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }
static const char *stillFalhou;   // URL que o cache diz que nao vem
int  tex_falhou(const char *u) { return stillFalhou && u && !strcmp(u, stillFalhou); }
char *dados_ler(const char *nome) { (void)nome; return NULL; }
int  dados_gravar(const char *nome, const char *c) { (void)nome; (void)c; return 1; }
void dados_marcar_sujo(int leve) { (void)leve; }
const char *ling_legenda(void) { return ""; }
const char *ling_audio(void) { return ""; }
const char *ling_conta_legenda_valor(void) { return ""; }
const char *ling_conta_legenda2_valor(void) { return ""; }
const char *ling_conta_audio_valor(void) { return ""; }
void ling_conta_legenda(const char *v) { (void)v; }
void ling_conta_legenda2(const char *v) { (void)v; }
void ling_conta_audio(const char *v) { (void)v; }
int  perfis_ativo(void) { return 1; }

int main(void) {
  static CatItem it[2];
  static CatFileira fs[1];
  const char *a;
  int i;
  ajustes_aplicar_blob("{\"continueWatchingEnabled\":true,\"useEpisodeThumbnailsInCw\":true}");
  for (i = 0; i < 2; i++) {
    snprintf(it[i].imdb, sizeof it[i].imdb, "tt%07d", 1000 + i);
    snprintf(it[i].titulo, sizeof it[i].titulo, "T%d", i);
    snprintf(it[i].tipo, sizeof it[i].tipo, i == 0 ? "series" : "movie");
    snprintf(it[i].poster, sizeof it[i].poster, "https://p.invalid/%d.jpg", i);
    snprintf(it[i].backdrop, sizeof it[i].backdrop, "https://b.invalid/%d.jpg", i);
    it[i].progresso = 40;
  }
  it[0].temporada = 1; it[0].episodio = 3;
  snprintf(fs[0].chave, sizeof fs[0].chave, "continue_watching");
  snprintf(fs[0].titulo, sizeof fs[0].titulo, "Continuar assistindo");
  fs[0].ini = 0; fs[0].n = 2;
  cat_definir_tudo(it, 2, fs, 1);

  a = arte_por_identidade(0, 3);
  if (!a || !strstr(a, "episodes.metahub.space/tt0001000/1/3/")) {
    fprintf(stderr, "card da retomada com miniatura ligada: %s (esperado o still T1E3)\n",
            a ? a : "(nulo)");
    return 1;
  }
  puts("ok  miniatura ligada: o card do episodio usa o still dele");

  stillFalhou = a;
  { char falhou[512];
    snprintf(falhou, sizeof falhou, "%s", a);
    stillFalhou = falhou;
    a = arte_por_identidade(0, 3);
    assert(a && !strstr(a, "episodes.metahub.space"));
    stillFalhou = NULL; }
  puts("ok  still 404: cai na arte do titulo");

  a = arte_por_identidade(1, 3);
  assert(a && !strstr(a, "episodes.metahub.space"));
  puts("ok  filme: arte do titulo");

  ajustes_aplicar_blob("{\"useEpisodeThumbnailsInCw\":false}");
  assert(!ajustes_cw_thumb_episodio());
  a = arte_por_identidade(0, 3);
  assert(a && !strstr(a, "episodes.metahub.space"));
  puts("ok  miniatura desligada: arte da serie");
  puts("cwthumb: tudo ok");
  return 0;
}

// Sem textura nesta fixture de identidade.
float tex_aspecto(const char *url) { (void)url; return 0.0f; }
