// #244: Continuar assistindo mostra cada obra UMA vez (Trakt x Simkl x conta, chave de
// episodio x chave de serie, varias gravacoes), a fonte escolhida vale, e marcar como
// assistido tira a obra de vez.
#include "../src/homeestado.h"
unsigned homeestado_geracao(void) { return 1; }
unsigned recomenda_geracao(void) { return 1; }
int homeestado_contexto_valido(void) { return 0; }
int homeestado_tem_fileira(const char *chave) { (void)chave; return 0; }
int homeestado_ordem_fileira(const char *chave) { (void)chave; return -1; }
int homeestado_salvar_se_geracao(const CatFileira *fils, int n, unsigned g) { (void)fils; (void)n; (void)g; return 1; }
int homeestado_identidade_geracao(unsigned g, char *dono, unsigned tamDono, int *perfil) {
  (void)g; if (dono && tamDono) dono[0] = 0; if (perfil) *perfil = 0; return 0; }
int arte_reserva_episodios(const char *imdb, const char *corpo) { (void)imdb; (void)corpo; return 0; }

int ajustes_social(void) { return 1; }
int ajustes_hist_conta(void) { return 1; }
int ajustes_busca_cinemeta(void) { return 1; }
int ajustes_busca_nuvio(void) { return 0; }   // #311: Primeiro (padrao)
#include "../src/descoberta.c"
Uint32 SDL_GetTicks(void) { return 0; }
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include "jellyfin_stub.inc"

// --- DUBLES: so fazem descoberta.c linkar (o conjunto de tests/cateps.c) -----
int         ajustes_idioma_ingles(void) { return 0; }
int ajustes_idioma(void) { return 0; }
int   ajustes_cw_ordem(void)               { return 0; }   // Padrao (issue #127)
int   ajustes_cw_concluido(void)           { return 90; }  // Percentual assistido de fabrica
int   ajustes_cw_mostrar_nao_exibidos(void) { return 1; }
int   ajustes_cw_proximo(void) { return 1; }
const char *i18n(const char *s)         { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }
const char *dados_dir(void)             { return ""; }
const char *sessao_usuario(void)        { return ""; }
int         perfis_ativo(void)          { return 1; }
char *dados_ler(const char *nome)                 { (void)nome; return NULL; }
int   dados_gravar(const char *nome, const char *c) { (void)nome; (void)c; return 1; }
int   dados_apagar(const char *nome)              { (void)nome; return 1; }
void  SDL_Delay(Uint32 ms)                 { usleep(ms * 1000); }
static int traktLigado = 1;
static int fonteTeste = 0;
int   ajustes_cw_fonte(void)               { return fonteTeste; }
int   ajustes_tmdb_ligado(void)            { return 0; }
int   ajustes_tmdb_basico(void)            { return 0; }
int   ajustes_meta_externo(void)           { return 0; }
int   ajustes_meta_so_cinemeta(void)        { return 0; }
int   ajustes_fundo_addon(void)            { return 0; }
int   ajustes_logo_addon(void)             { return 0; }
int   addons_aceita_id(int i, const char *t, const char *id) { (void)i; (void)t; (void)id; return -1; }
int   ajustes_tmdb_arte(void)              { return 0; }
int   ajustes_tmdb_elenco(void)            { return 0; }
int   ajustes_tmdb_cw(void)                { return 0; }
const char *ajustes_tmdb_idioma(void)      { return "pt-BR"; }
const char *ajustes_tmdb_chave(void)       { return ""; }
void  fil_gravar_registro(void)            { }
int   fil_lista_e_deste_perfil(int p)       { (void)p; return 1; }
unsigned fil_perfil_geracao(void)          { return 0; }
FilPassada fil_passada_ler(void)          { FilPassada p = {0, 0}; return p; }
int   fil_passada_valida(const FilPassada *p) { (void)p; return 1; }
void  fil_registrar_de(const FilPassada *p, const char *c, const char *t, const char *a, const char *k, int n) { (void)p; (void)c; (void)t; (void)a; (void)k; (void)n; }
void  fil_registrar_se_couber_de(const FilPassada *p, const char *c, const char *t, const char *a, const char *k) { (void)p; (void)c; (void)t; (void)a; (void)k; }
int   fil_podar_catalogos(const char *const *ids, const char *const *bases, int n,
                          int perfilDaLista) {
  (void)ids; (void)bases; (void)n; (void)perfilDaLista; return 0; }
int fil_marcar_sem_addon(const char *const *ids, const char *const *bases, const int *ativos, int n, int perfilDaLista) {
  (void)ids; (void)bases; (void)ativos; (void)n; (void)perfilDaLista; return 0;
}
int   fil_addon_novo(const char *id, const char *base) { (void)id; (void)base; return 0; }
int   addons_perfil_da_lista(void)         { return 0; }
int addons_base_desligada(const char *b) { (void)b; return 0; }   // 203-desligados
int   addons_ativo(int i)                  { (void)i; return 1; }
int   fil_limite(void)                     { return 16; }
int   fil_oculta(const char *c)            { (void)c; return 0; }
int   fil_adicionada_na_tv(const char *c) { (void)c; return 0; }
int   fil_estado_chave(const char *c) { (void)c; return -1; }
const char *fil_hero_fonte(void) { return ""; }
// Dubles da escolha da cota (#126): nada escolhido na TV, e o registro dos
// catalogos fora da cota nao interessa a este teste.
int fil_escolhida(const char *c) { (void)c; return -1; }
int fil_migrar_197(const char *const *c, int n) { (void)c; (void)n; return 0; }
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
static int simklLigado = 0;
int   simkl_ativo(void)                    { return simklLigado; }
static void um(CatItem *c, const char *imdb, const char *tipo, int t, int e, int prog, long long ms) {
  memset(c, 0, sizeof *c);
  snprintf(c->imdb, sizeof c->imdb, "%s", imdb);
  snprintf(c->titulo, sizeof c->titulo, "Titulo %s", imdb);
  snprintf(c->tipo, sizeof c->tipo, "%s", tipo);
  c->temporada = t; c->episodio = e; c->progresso = prog; c->retomadoMs = ms;
}
int   simkl_continuar(CatItem *s, int m) {
  if (m < 3) return 0;
  um(&s[0], "tt500:1:8", "series", 1, 8, 30, 800000);   // a mesma obra do Trakt
  um(&s[1], "tt600", "series", 1, 2, 10, 700000);
  um(&s[2], "tt600:1:2", "series", 1, 2, 10, 700001);   // duplicata dentro do Simkl
  return 3;
}
int   simkl_e_a_seguir(const char *id)     { (void)id; return 0; }
int   simkl_plantowatch(CatItem *s, int m) { (void)s; (void)m; return 0; }
int   ajustes_salvos_no_simkl(void)        { return 0; }
int   trakt_enfeitar_lote(CatItem *s, int n) { (void)s; return n; }
int   trakt_lista(const char *q, CatItem *s, int m) { (void)q; (void)s; (void)m; return 0; }
int   trakt_lista_cresc(const char *q, CatItem **s, int m) { (void)q; (void)s; (void)m; return 0; }
// O servico social proprio (recomenda.c) fica fora deste teste: a uniao e so o que o Trakt trouxe.
int   recomenda_social_mesclar(CatItem *i, int nTrakt, int max) { (void)i; (void)max; return nTrakt; }
int   trakt_social(CatItem *s, int m)      { (void)s; (void)m; return 0; }
int   trakt_e_a_seguir(const char *id)     { (void)id; return 0; }
int   trakt_progresso_ocultar(const char *i, int o) { (void)i; (void)o; return 0; }
// "A seguir" da conta (#199): sem vistos aqui, e com o Trakt "no ar" o caminho
// nem roda. So para linkar.
int   trakt_ativo(void)                    { return traktLigado; }
int   ajustes_cw_do_episodio_mais_alto(void) { return 1; }
int   contalib_sementes_a_seguir(ContaSemente *s, int m, int a) { (void)s; (void)m; (void)a; return 0; }
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

// --- O "TRAKT" FALSO ---------------------------------------------------------
// /sync/playback: tres filmes pausados. O teste mexe no paused_at de tt2.
int trakt_continuar(CatItem *s, int m) {
  int i;
  if (m < 6 || !traktLigado) return 0;
  // 5 gravacoes do mesmo episodio pausado (chave de episodio) + a serie por chave pura
  for (i = 0; i < 3; i++) um(&s[i], "tt500:1:8", "series", 1, 8, 30, 900000 - i);
  um(&s[3], "tt500", "series", 1, 8, 30, 899000);       // chave da serie
  um(&s[4], "tt700", "movie", 0, 0, 40, 600000);
  return 5;
}
int   trakt_continuar_falhou(void)        { return 0; }

static long long agora = 1000000;
static long long relogioTeste(void) { return agora; }

// --- O QUE O TESTE OLHA ------------------------------------------------------
static const CatFileira *fileiraPorChave(const char *chave) {
  int r;
  for (r = 0; r < cat_n_fileiras(); r++)
    if (!strcmp(cat_fileira(r)->chave, chave)) return cat_fileira(r);
  return NULL;
}
static int naContinuar(const char *imdb) {
  const CatFileira *f = fileiraPorChave("continue_watching");
  int i;
  if (!f) return 0;
  for (i = 0; i < f->n; i++)
    if (!strcmp(cat_item(f->ini + i)->imdb, imdb)) return 1;
  return 0;
}
static int nContinuar(void) {
  const CatFileira *f = fileiraPorChave("continue_watching");
  return f ? f->n : 0;
}
// A fileira de baixo tem de continuar mostrando OS MESMOS titulos: e o titulo
// que a pessoa ve, nao o indice.
static void conferirLista(void) {
  const CatFileira *f = fileiraPorChave("lista");
  assert(f && f->n == 2);
  assert(!strcmp(cat_item(f->ini)->imdb, "tt90"));
  assert(!strcmp(cat_item(f->ini + 1)->imdb, "tt91"));
}

// Publica como montar() publica: a fileira de retomada em ini=0 e uma fileira
// de catalogo depois dela. Guarda o retrato em filsMontadas, como montar().
static void cicloCompleto(void) {
  CatItem lote[CONT_MAX + 2];
  int nc = montarContinuar(lote, CONT_MAX);
  memset(&lote[nc], 0, sizeof lote[0] * 2);
  snprintf(lote[nc].imdb, sizeof lote[nc].imdb, "tt90");
  snprintf(lote[nc + 1].imdb, sizeof lote[nc + 1].imdb, "tt91");
  memset(filsMontadas, 0, sizeof filsMontadas[0] * 2);
  snprintf(filsMontadas[0].chave, sizeof filsMontadas[0].chave, "continue_watching");
  filsMontadas[0].ini = 0; filsMontadas[0].n = nc;
  snprintf(filsMontadas[1].chave, sizeof filsMontadas[1].chave, "lista");
  snprintf(filsMontadas[1].base, sizeof filsMontadas[1].base, "https://addon.invalid");
  snprintf(filsMontadas[1].catId, sizeof filsMontadas[1].catId, "top");
  filsMontadas[1].ini = nc; filsMontadas[1].n = 2;
  nFileirasMontadas = 2;
  cat_definir_tudo(lote, nc + 2, filsMontadas, 2);
}


static int quantos(const char *obra) {
  const CatFileira *f = fileiraPorChave("continue_watching");
  int i, k = 0;
  size_t L = strlen(obra);
  if (!f) return 0;
  for (i = 0; i < f->n; i++) {
    const char *im = cat_item(f->ini + i)->imdb;
    if (!strncmp(im, obra, L) && (im[L] == 0 || im[L] == ':')) k++;
  }
  return k;
}
static void ciclo(void) {
  CatItem lote[CONT_MAX];
  int nc = montarContinuar(lote, CONT_MAX);
  cat_definir_tudo(lote, nc, (CatFileira[]){ { .chave = "continue_watching", .ini = 0, .n = nc } }, 1);
}
int main(void) {
  prog_definir_relogio(relogioTeste);
  // 1. Fonte "Ambas": cada obra uma vez, mesmo com varias gravacoes e chaves de episodio/serie.
  simklLigado = 1; fonteTeste = 0;
  ciclo();
  assert(quantos("tt500") == 1);
  assert(quantos("tt600") == 1);
  assert(quantos("tt700") == 1);
  assert(nContinuar() == 3);
  // 2. Fonte so Trakt: nada do Simkl.
  fonteTeste = 2; ciclo();
  assert(quantos("tt500") == 1 && quantos("tt700") == 1 && quantos("tt600") == 0 && nContinuar() == 2);
  // 3. Fonte so Simkl: nada do Trakt, e a duplicata interna some.
  fonteTeste = 3; ciclo();
  assert(quantos("tt500") == 1 && quantos("tt600") == 1 && quantos("tt700") == 0 && nContinuar() == 2);
  // 4. Fonte so conta Nuvio: sem registro local, vazia.
  fonteTeste = 1; ciclo();
  assert(nContinuar() == 0);
  // 5. "Marcar como assistido" (ctxmenu): tombstone + tirar da fileira = a obra sai de
  //    todas as chaves e NAO volta quando o remoto ainda a devolve.
  fonteTeste = 0; ciclo();
  assert(quantos("tt500") == 1);
  prog_marcar_removido("tt500:1:8");
  assert(cat_tirar_continuar("tt500:1:8") >= 1);
  assert(quantos("tt500") == 0);
  agora += 10; ciclo();
  assert(quantos("tt500") == 0 && quantos("tt600") == 1);
  puts("cwdup244: tudo ok");
  return 0;
}
