// AS FAIXAS DE EPISODIO SOBREVIVEM AO CATALOGO CRESCER.
//
// Os episodios nao vivem num vetor por titulo: ha um vetor unico e cada titulo
// guarda uma janela (epIni, epQtd) sobre ele — a mesma forma das fileiras, e a
// mesma armadilha. Quem acrescenta um titulo no FIM nao muda o indice de
// ninguem, entao nada ali pode invalidar a janela dos outros.
//
// Era o defeito: garantirFaixas() zerava o vetor inteiro e era chamada tambem
// por cat_acrescentar e pelo append em lote. Na TV isso apagava os episodios da
// serie ABERTA assim que a descoberta acrescentava qualquer titulo, e a secao
// de episodios desaparecia da pagina de detalhe — o D-pad passava de
// "Temporadas" direto para as abas, porque secao com zero colunas e
// intransponivel.
//
// O QUE ESTE TESTE PROVA:
//   1. acrescentar UM titulo nao mexe nos episodios de quem ja estava;
//   2. acrescentar um LOTE tambem nao;
//   3. o titulo NOVO nasce com zero episodios, e nao com lixo do realloc;
//   4. trocar o catalogo inteiro (cat_definir_tudo) INVALIDA tudo, porque ai os
//      indices mudaram de verdade — o contrario dos casos acima.
//
//   bash tests/cateps.sh
// Alem das faixas, o teste da NOTA DE EPISODIO do TMDB (issue #87) vive aqui:
// desc_tmdb_notas_temporada e pura sobre JSON + CatEp, e o jeito de linka-la e
// o mesmo de tests/colfileiras.c — descoberta.c INTEIRO entra por #include e
// os simbolos que ele pede (addons, rede, trakt...) viram dubles abaixo. Os
// cat_* NAO viram duble: este arquivo existe exatamente para exercitar os de
// verdade do catalogo.c.
#include "../src/descoberta.c"
#include "../src/progresso.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>

// --- DUBLES: nenhum participa da regra, so fazem descoberta.c linkar ---------
// (o conjunto e o de tests/colfileiras.c, menos os cat_* — que catalogo.c ja
// traz — e mais os que este teste ja tinha)
int         ajustes_idioma_ingles(void) { return 0; }
const char *i18n(const char *s)         { return s; }
const char *dados_dir(void)             { return ""; }
const char *sessao_usuario(void)        { return ""; }
int         perfis_ativo(void)          { return 1; }
unsigned homeestado_geracao(void) { return 1; }
int homeestado_contexto_valido(void) { return 0; }
int homeestado_tem_fileira(const char *chave) { (void)chave; return 0; }
int homeestado_ordem_fileira(const char *chave) { (void)chave; return -1; }
int homeestado_salvar_se_geracao(const CatFileira *f, int n, unsigned g) {
  (void)f; (void)n; return g == 1;
}
int homeestado_identidade_geracao(unsigned g, char *d, unsigned z, int *p) {
  if (g != 1) return 0;
  if (d && z) d[0] = 0;
  if (p) *p = 1;
  return 1;
}
int arte_reserva_episodios(const char *imdb, const char *corpo) { (void)imdb; (void)corpo; return 0; }
// Contexto em partes (homeestado.h, 1.4.5): constante aqui, entao nada muda
// no meio da montagem e o fim dela segue o caminho de sempre.
void homeestado_contexto(HomeContexto *c) { *c = (HomeContexto){0}; c->perfil = 1; }
int homeestado_mudancas(const HomeContexto *a, const HomeContexto *b) { (void)a; (void)b; return 0; }
const char *homeestado_mudancas_texto(int m, char *b, unsigned t) { (void)m; if (b && t) b[0] = 0; return b; }
int prog_ler(ProgRegistro *saida, int max) { (void)saida; (void)max; return 0; }
int prog_gravar_local(const char *imdb, int t, int e, double p, double d) {
  (void)imdb; (void)t; (void)e; (void)p; (void)d; return 0;
}

void  SDL_Delay(Uint32 ms)                 { usleep(ms * 1000); }
int   ajustes_cw_fonte(void)               { return 0; }
int   ajustes_tmdb_ligado(void)            { return 1; }
int   ajustes_tmdb_basico(void)            { return 0; }
int   ajustes_tmdb_arte(void)              { return 0; }
int   ajustes_tmdb_elenco(void)            { return 0; }
int   ajustes_tmdb_cw(void)                { return 0; }
const char *ajustes_tmdb_idioma(void)      { return "pt-BR"; }
const char *ajustes_tmdb_chave(void)       { return ""; }
void  fil_gravar_registro(void)            { }
int   fil_podar_catalogos(const char *const *ids, const char *const *bases, int n) {
  (void)ids; (void)bases; (void)n; return 0; }
int   fil_limite(void)                     { return 16; }
int   fil_oculta(const char *c)            { (void)c; return 0; }
void  fil_registrar(const char *c, const char *t, const char *a,
                    const char *tp, int itens) {
  (void)c; (void)t; (void)a; (void)tp; (void)itens;
}
int   fil_tem_ordem(void)                  { return 0; }
int   fil_unir(const char *const *c, int n, int *s, int m) {
  int i; (void)c; for (i = 0; i < n && i < m; i++) s[i] = i; return i;
}
void  marco(const char *n)                 { (void)n; }
void  prog_chave(char *d, unsigned n, const char *c, int t, int e) {
  (void)c; (void)t; (void)e; if (n) d[0] = 0;
}
void  prog_content_id(char *d, unsigned n, const char *i, int *t, int *e) {
  (void)i; (void)t; (void)e; if (n) d[0] = 0;
}
int   prog_por_chave(const char *c, ProgRegistro *s) { (void)c; (void)s; return 0; }
// "Tirar de Continuar assistindo" (desc_tirar_continuar, tests/cwremover.sh).
void  prog_remover(const char *c)          { (void)c; }
void  prog_marcar_removido(const char *i)  { (void)i; }
int   prog_removido_vence(const char *i, long long ms) { (void)i; (void)ms; return 0; }
int   trakt_continuar(CatItem *s, int m)   { (void)s; (void)m; return 0; }
// Simkl (issue #110): sem vinculo nos testes de fileira, como o Trakt acima.
int   simkl_ativo(void)                    { return 0; }
int   simkl_continuar(CatItem *s, int m)   { (void)s; (void)m; return 0; }
int   simkl_e_a_seguir(const char *id)     { (void)id; return 0; }
int   simkl_plantowatch(CatItem *s, int m) { (void)s; (void)m; return 0; }
int   ajustes_salvos_no_simkl(void)        { return 0; }
int   trakt_enfeitar_lote(CatItem *s, int n) { (void)s; (void)n; return 0; }
int   trakt_lista(const char *q, CatItem *s, int m) { (void)q; (void)s; (void)m; return 0; }
int   trakt_social(CatItem *s, int m)      { (void)s; (void)m; return 0; }
const char *nuvem_trakt_cliente(void)      { return ""; }
int   addons_n(void)                       { return 0; }
const char *addons_base(int i)             { (void)i; return ""; }
const char *addons_id_manifesto(int i)     { (void)i; return ""; }
const char *addons_nome(int i)            { (void)i; return "addon"; }
unsigned addons_versao(void)             { return 1; }   // estatico no teste
const char *addons_base_por_id(const char *id) { (void)id; return ""; }
void  addons_manifesto_lido(int i, const char *corpo) { (void)i; (void)corpo; }
char *rede_baixar(const char *u, int t)    { (void)u; (void)t; return NULL; }
char *rede_baixar_com(const char *u, int t, const char *const *c) {
  (void)c; return rede_baixar(u, t); }

// cat_acrescentar publica a reserva junto com o item. Este espião deixa o
// teste provar os dois campos sem depender de rede ou de uma implementação de
// TMDB: o registry próprio é exercitado em tests/artereserva.c.
static int reservaN;
static char reservaUrls[8][512];
static char reservaIds[8][24];
static int reservaTipos[8];
int arte_reserva_registrar(const char *url, const char *imdb, int poster) {
  if (reservaN < 8) {
    snprintf(reservaUrls[reservaN], sizeof reservaUrls[reservaN], "%s", url);
    snprintf(reservaIds[reservaN], sizeof reservaIds[reservaN], "%s", imdb);
    reservaTipos[reservaN] = poster;
  }
  reservaN++;
  return 1;
}

static CatItem itens[3];

static void montarCatalogo(void) {
  int i;
  memset(itens, 0, sizeof itens);
  for (i = 0; i < 3; i++) {
    snprintf(itens[i].titulo, sizeof itens[i].titulo, "T%d", i);
    snprintf(itens[i].imdb, sizeof itens[i].imdb, "tt%d", i);
    snprintf(itens[i].tipo, sizeof itens[i].tipo, "series");
  }
  cat_definir_tudo(itens, 3, NULL, 0);
}

// Tres episodios com nome reconhecivel, para a assercao ser sobre CONTEUDO e
// nao so sobre a contagem: um vetor com o tamanho certo e os dados de outra
// serie passaria por um teste que so contasse.
static void publicar(int alvo, const char *prefixo, int quantos) {
  CatEp eps[4];
  int i;
  memset(eps, 0, sizeof eps);
  for (i = 0; i < quantos; i++) {
    eps[i].temporada = 1;
    eps[i].episodio = i + 1;
    snprintf(eps[i].nome, sizeof eps[i].nome, "%s-E%d", prefixo, i + 1);
  }
  cat_definir_episodios(alvo, eps, quantos);
}

static int temEpisodios(int alvo, const char *prefixo, int quantos) {
  int i;
  if (cat_n_episodios(alvo) != quantos) return 0;
  for (i = 0; i < quantos; i++) {
    char esperado[64];
    const CatEp *e = cat_episodio(alvo, i);
    snprintf(esperado, sizeof esperado, "%s-E%d", prefixo, i + 1);
    if (!e || strcmp(e->nome, esperado)) return 0;
  }
  return 1;
}

int main(void) {
  CatItem novo;
  CatItem lote[2];

  // A) UM TITULO ACRESCENTADO no fim nao apaga os episodios de quem ja estava.
  //    Este e o caso que a TV mostrava: a serie aberta perdia a secao inteira
  //    quando a descoberta trazia mais um titulo.
  montarCatalogo();
  publicar(0, "A", 3);
  publicar(2, "C", 2);
  assert(temEpisodios(0, "A", 3));
  memset(&novo, 0, sizeof novo);
  snprintf(novo.titulo, sizeof novo.titulo, "novo");
  { int i = cat_acrescentar(&novo);
    assert(i == 3);
    assert(temEpisodios(0, "A", 3));
    assert(temEpisodios(2, "C", 2));
    // E o recem-chegado nasce VAZIO. realloc nao inicializa o que cresceu: um
    // epQtd de lixo aqui faria cat_episodio ler fora do vetor de episodios.
    assert(cat_n_episodios(i) == 0); }
  puts("ok  acrescentar um titulo preserva as faixas e o novo nasce vazio");

  // B) O MESMO PARA O LOTE, que e outro caminho de codigo.
  montarCatalogo();
  publicar(1, "B", 4);
  memset(lote, 0, sizeof lote);
  snprintf(lote[0].titulo, sizeof lote[0].titulo, "L0");
  snprintf(lote[1].titulo, sizeof lote[1].titulo, "L1");
  { int idx[2];
    assert(cat_acrescentar_lote(lote, 2, idx) == 2);
    assert(temEpisodios(1, "B", 4));
    assert(cat_n_episodios(idx[0]) == 0 && cat_n_episodios(idx[1]) == 0); }
  puts("ok  acrescentar em lote preserva as faixas");

  // C) DOIS APPENDS SEGUIDOS. A cauda zerada da primeira vez nao pode ser
  //    zerada de novo por cima de episodios publicados depois dela.
  montarCatalogo();
  { int i = cat_acrescentar(&novo);
    publicar(i, "D", 2);
    assert(cat_acrescentar(&novo) >= 0);
    assert(temEpisodios(i, "D", 2)); }
  puts("ok  publicar depois de crescer e crescer de novo mantem o que foi publicado");

  // D) TROCAR O CATALOGO INTEIRO INVALIDA, e tem de invalidar: os indices
  //    passam a apontar para outros titulos. E o oposto exato de (A), e por
  //    isso os dois estao no mesmo arquivo — quem mexer num vai ler o outro.
  montarCatalogo();
  publicar(0, "A", 3);
  assert(temEpisodios(0, "A", 3));
  cat_definir_tudo(itens, 3, NULL, 0);
  assert(cat_n_episodios(0) == 0);
  puts("ok  trocar o catalogo inteiro invalida as faixas");

  // E) APPEND DE ARTE: o caminho unitário precisa registrar poster e fundo
  // exatamente como o lote e a publicação completa. Repetir o mesmo item é
  // permitido pelo catálogo, mas deve reusar a mesma chave no registry (a
  // deduplicação da chave é coberta pelo teste de arte); aqui garantimos que
  // não existe um caminho unitário que simplesmente esqueça a arte.
  montarCatalogo();
  { CatItem arte;
    int antes;
    memset(&arte, 0, sizeof arte);
    snprintf(arte.imdb, sizeof arte.imdb, "tt9000001");
    snprintf(arte.poster, sizeof arte.poster, "https://addon/poster/one.jpg");
    snprintf(arte.backdrop, sizeof arte.backdrop, "https://addon/background/one.jpg");
    reservaN = 0;
    assert(cat_acrescentar(&arte) == 3);
    assert(reservaN == 2);
    assert(!strcmp(reservaUrls[0], arte.poster) && !strcmp(reservaIds[0], arte.imdb) && reservaTipos[0] == 1);
    assert(!strcmp(reservaUrls[1], arte.backdrop) && !strcmp(reservaIds[1], arte.imdb) && reservaTipos[1] == 0);
    antes = reservaN;
    assert(cat_acrescentar(&arte) == 4);
    assert(reservaN == antes + 2);
  }
  puts("ok  append unitario registra poster/fundo em novo e repetido item");

  // F) NOTA TMDB POR EPISODIO (issue #87). JSON de temporada sintetico com
  //    tres episodios — um SEM vote_average — mais um de OUTRA temporada na
  //    lista, que nao pode ser tocado. Casa por episode_number; devolve
  //    quantos ganharam nota.
  { CatEp eps2[4];
    const char *json =
      "{\"episodes\":["
      "{\"episode_number\":1,\"vote_average\":8.3},"
      "{\"episode_number\":2},"
      "{\"episode_number\":3,\"vote_average\":7.0}]}";
    memset(eps2, 0, sizeof eps2);
    eps2[0].temporada = 1; eps2[0].episodio = 1;
    eps2[1].temporada = 1; eps2[1].episodio = 2;
    eps2[2].temporada = 1; eps2[2].episodio = 3;
    eps2[3].temporada = 2; eps2[3].episodio = 1;
    assert(desc_tmdb_notas_temporada(json, eps2, 4, 1) == 2);
    assert(eps2[0].nota == 83);          // 8.3 x10
    assert(eps2[1].nota == 0);           // sem vote_average: continua zero
    assert(eps2[2].nota == 70);
    assert(eps2[3].nota == 0);           // temporada 2 nao e da resposta
    // Pedindo a temporada errada nao preenche nada — e nao devolve nada.
    memset(eps2, 0, sizeof eps2);
    eps2[0].temporada = 1; eps2[0].episodio = 1;
    assert(desc_tmdb_notas_temporada(json, eps2, 1, 2) == 0);
    assert(eps2[0].nota == 0); }
  puts("ok  nota TMDB por episodio casa numero/temporada e ignora o que falta");

  puts("cateps: tudo ok");
  return 0;
}
