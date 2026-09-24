// A MONTAGEM DE "CONTINUAR ASSISTINDO" OBEDECE A ORDENACAO (issue #127).
// Parte de tests/cwordem.sh: inclui src/descoberta.c, como tests/cwremover.c,
// com um "Trakt" falso que devolve pausados e "a seguir" com estreia passada e
// futura. Cobra, por modo, a ordem que montarContinuar entrega, quem ele
// publica como futuro (cwo_e_futuro, que a home usa para partir a fileira) e o
// `showUnairedNextUp` desligado tirando os futuros de vez.
//
//   bash tests/cwordem.sh
// Duples do estado da home (merge do Codex): sem snapshot valido aqui, que e
// o caso de um primeiro arranque — a remocao e o que este teste cobra.
#include "../src/homeestado.h"
unsigned homeestado_geracao(void) { return 1; }
int homeestado_contexto_valido(void) { return 0; }
int homeestado_tem_fileira(const char *chave) { (void)chave; return 0; }
int homeestado_ordem_fileira(const char *chave) { (void)chave; return -1; }
int homeestado_salvar_se_geracao(const CatFileira *fils, int n, unsigned g) { (void)fils; (void)n; (void)g; return 1; }
int homeestado_identidade_geracao(unsigned g, char *dono, unsigned tamDono, int *perfil) {
  (void)g; if (dono && tamDono) dono[0] = 0; if (perfil) *perfil = 0; return 0; }
int arte_reserva_episodios(const char *imdb, const char *corpo) { (void)imdb; (void)corpo; return 0; }

#include "../src/descoberta.c"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>

// --- DUBLES: so fazem descoberta.c linkar (o conjunto de tests/cateps.c) -----
int         ajustes_idioma_ingles(void) { return 0; }
const char *i18n(const char *s)         { return s; }
const char *dados_dir(void)             { return ""; }
const char *sessao_usuario(void)        { return ""; }
int         perfis_ativo(void)          { return 1; }
char *dados_ler(const char *nome)                 { (void)nome; return NULL; }
int   dados_gravar(const char *nome, const char *c) { (void)nome; (void)c; return 1; }
int   dados_apagar(const char *nome)              { (void)nome; return 1; }
void  SDL_Delay(Uint32 ms)                 { usleep(ms * 1000); }
int   ajustes_cw_fonte(void)               { return 0; }   // AJ_CWF_AMBAS
int   ajustes_tmdb_ligado(void)            { return 0; }
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
// Dubles da escolha da cota (#126): nada escolhido na TV, e o registro dos
// catalogos fora da cota nao interessa a este teste.
int fil_escolhida(const char *c) { (void)c; return -1; }
void fil_registrar_se_couber(const char *c, const char *t, const char *a,
                             const char *tp) { (void)c; (void)t; (void)a; (void)tp; }
void  fil_registrar(const char *c, const char *t, const char *a,
                    const char *tp, int itens) {
  (void)c; (void)t; (void)a; (void)tp; (void)itens;
}
// Contexto em partes (homeestado.h, 1.4.5): constante aqui, entao nada muda
// no meio da montagem e o fim dela segue o caminho de sempre.
void homeestado_contexto(HomeContexto *c) { *c = (HomeContexto){0}; c->perfil = 1; }
int homeestado_mudancas(const HomeContexto *a, const HomeContexto *b) { (void)a; (void)b; return 0; }
const char *homeestado_mudancas_texto(int m, char *b, unsigned t) { (void)m; if (b && t) b[0] = 0; return b; }
int   fil_tem_ordem(void)                  { return 0; }
int   fil_unir(const char *const *c, int n, int *s, int m) {
  int i; (void)c; for (i = 0; i < n && i < m; i++) s[i] = i; return i;
}
void  marco(const char *n)                 { (void)n; }
int   simkl_ativo(void)                    { return 0; }
int   simkl_continuar(CatItem *s, int m)   { (void)s; (void)m; return 0; }
int   simkl_plantowatch(CatItem *s, int m) { (void)s; (void)m; return 0; }
int   ajustes_salvos_no_simkl(void)        { return 0; }
int   trakt_enfeitar_lote(CatItem *s, int n) { (void)s; return n; }
int   trakt_lista(const char *q, CatItem *s, int m) { (void)q; (void)s; (void)m; return 0; }
int   trakt_social(CatItem *s, int m)      { (void)s; (void)m; return 0; }
const char *nuvem_trakt_cliente(void)      { return ""; }
int   addons_n(void)                       { return 0; }
const char *addons_base(int i)             { (void)i; return ""; }
const char *addons_id_manifesto(int i)     { (void)i; return ""; }
const char *addons_nome(int i)             { (void)i; return "addon"; }
unsigned addons_versao(void)               { return 1; }
const char *addons_base_por_id(const char *id) { (void)id; return ""; }
void  addons_manifesto_lido(int i, const char *corpo) { (void)i; (void)corpo; }
char *rede_baixar(const char *u, int t)    { (void)u; (void)t; return NULL; }
char *rede_baixar_com(const char *u, int t, const char *const *c) {
  (void)c; return rede_baixar(u, t); }
int   arte_reserva_registrar(const char *url, const char *imdb, int poster) {
  (void)url; (void)imdb; (void)poster; return 1; }

static int modoTeste = CWO_PADRAO, naoExibidosTeste = 1;
int ajustes_cw_ordem(void)                { return modoTeste; }
int ajustes_cw_mostrar_nao_exibidos(void) { return naoExibidosTeste; }

// --- O "TRAKT" FALSO ---------------------------------------------------------
// Por instante (o mais recente primeiro depois da ordenacao da montagem):
//   ttA:1:3   a seguir, estreia daqui a 10 dias  (futuro)
//   tt1       pausado
//   ttB:2:1   a seguir, estreia amanha           (futuro)
//   ttC:1:8   a seguir, foi ao ar ontem
//   tt2       pausado
#define DIA (24LL * 60 * 60 * 1000)
static long long agoraMs;
static const struct { const char *id; int seguir, prog; long long quando, estreiaDias; } FALSO[] = {
  { "ttA:1:3", 1,  0, 900500,  10 },
  { "tt1",     0, 40, 900400,   0 },
  { "ttB:2:1", 1,  0, 900300,   1 },
  { "ttC:1:8", 1,  0, 900200,  -1 },
  { "tt2",     0, 60, 900100,   0 },
};
#define NFALSO ((int)(sizeof FALSO / sizeof *FALSO))
int trakt_e_a_seguir(const char *id) {
  int i;
  for (i = 0; i < NFALSO; i++) if (!strcmp(FALSO[i].id, id)) return FALSO[i].seguir;
  return 0;
}
int simkl_e_a_seguir(const char *id) { (void)id; return 0; }
int trakt_continuar(CatItem *s, int m) {
  int i;
  for (i = 0; i < NFALSO && i < m; i++) {
    memset(&s[i], 0, sizeof s[i]);
    snprintf(s[i].imdb, sizeof s[i].imdb, "%s", FALSO[i].id);
    snprintf(s[i].tipo, sizeof s[i].tipo, "%s", FALSO[i].seguir ? "series" : "movie");
    if (FALSO[i].seguir) sscanf(strchr(FALSO[i].id, ':') + 1, "%d:%d", &s[i].temporada, &s[i].episodio);
    s[i].progresso = FALSO[i].prog;
    s[i].retomadoMs = FALSO[i].quando;
    // O que trakt.c faz no enfeite, com o `released` do Cinemeta.
    if (FALSO[i].seguir) cwo_marcar_estreia(FALSO[i].id, agoraMs + FALSO[i].estreiaDias * DIA);
  }
  return i;
}

static long long relogioProg(void) { return 1000000; }

static void conferir(const char *rotulo, const char *const *esperado, int n) {
  CatItem lote[CONT_MAX];
  int k, nc = montarContinuar(lote, CONT_MAX);
  if (nc != n) { fprintf(stderr, "%s: %d itens, esperado %d\n", rotulo, nc, n); exit(1); }
  for (k = 0; k < n; k++)
    if (strcmp(lote[k].imdb, esperado[k])) {
      fprintf(stderr, "%s: posicao %d = %s, esperado %s\n", rotulo, k, lote[k].imdb, esperado[k]);
      exit(1);
    }
}

int main(void) {
  prog_definir_relogio(relogioProg);
  agoraMs = (long long)time(NULL) * 1000LL;

  { static const char *const e[] = { "ttA:1:3", "tt1", "ttB:2:1", "ttC:1:8", "tt2" };
    modoTeste = CWO_PADRAO;
    conferir("padrao", e, 5);
    assert(!cwo_e_futuro("ttA:1:3") && !cwo_e_futuro("ttB:2:1"));
    puts("ok  padrao: pelo instante, nenhum futuro publicado"); }

  { static const char *const e[] = { "tt1", "ttC:1:8", "tt2", "ttB:2:1", "ttA:1:3" };
    modoTeste = CWO_STREAMING;
    conferir("streaming", e, 5);
    puts("ok  streaming: futuros no fim, a estreia mais proxima primeiro");
    modoTeste = CWO_SEPARAR;
    conferir("separar", e, 5);
    assert(cwo_e_futuro("ttA:1:3") && cwo_e_futuro("ttB:2:1"));
    assert(!cwo_e_futuro("ttC:1:8") && !cwo_e_futuro("tt1") && !cwo_e_futuro("tt2"));
    puts("ok  separar: mesma ordem, futuros publicados para a home"); }

  // Fileira curta: os futuros e que ficam de fora do corte.
  { CatItem lote[CONT_MAX];
    int nc = montarContinuar(lote, 4);
    assert(nc == 4 && !strcmp(lote[3].imdb, "ttB:2:1"));
    assert(cwo_e_futuro("ttB:2:1") && !cwo_e_futuro("ttA:1:3"));
    puts("ok  corte da fileira leva os futuros primeiro"); }

  { static const char *const e[] = { "tt1", "ttC:1:8", "tt2" };
    naoExibidosTeste = 0;
    conferir("nao exibidos desligado", e, 3);
    assert(!cwo_e_futuro("ttA:1:3") && !cwo_e_futuro("ttB:2:1"));
    modoTeste = CWO_PADRAO;
    conferir("nao exibidos desligado (padrao)", e, 3);
    puts("ok  nao exibidos desligado: futuros saem em qualquer modo"); }
  puts("cwordem_desc: tudo ok");
  return 0;
}
