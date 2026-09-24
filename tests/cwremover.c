// "TIRAR DE CONTINUAR ASSISTINDO" SAI NA HORA E NAO VOLTA COM O REMOTO VELHO.
//
// Pedido do dono (22/09): "na samsung o remove watching nao ta atualizando a
// fila na hora". Duas metades, uma por binario (ver tests/cwremover.sh):
//
//   ESTE ARQUIVO — a fileira PUBLICADA e a refacao. descoberta.c inteiro entra
//   por #include (como tests/cateps.c), com catalogo.c e progresso.c DE
//   VERDADE; so a rede e duble: um "Trakt" falso cujo /sync/playback o teste
//   controla, com o paused_at que quiser.
//     1. remover -> a janela "continue_watching" publicada nao tem mais o
//        item, as outras fileiras nao mudam de conteudo e cat_revisao sobe
//        (sem ela a home do guarda curto nao remonta; tests/cwremover_home.c);
//     2. uma refacao que recebe do Trakt o item com paused_at MAIS VELHO que a
//        remocao (o DELETE ainda nao chegou la) nao o traz de volta;
//     3. idem para a refacao que montou ANTES da remocao e publica DEPOIS
//        (fio em voo) e para o ciclo completo (cat_definir_tudo);
//     4. o retrato da ultima montagem (desc_remontar_fileiras, a cada sync)
//        nao devolve a janela velha;
//     5. com paused_at MAIS NOVO (assistiu de novo em outro aparelho) ele
//        volta — e tambem quando o registro novo e LOCAL.
//
//   bash tests/cwremover.sh
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
int   ajustes_cw_ordem(void)               { return 0; }   // Padrao (issue #127)
int   ajustes_cw_mostrar_nao_exibidos(void) { return 1; }
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
int   simkl_e_a_seguir(const char *id)     { (void)id; return 0; }
int   simkl_plantowatch(CatItem *s, int m) { (void)s; (void)m; return 0; }
int   ajustes_salvos_no_simkl(void)        { return 0; }
int   trakt_enfeitar_lote(CatItem *s, int n) { (void)s; return n; }
int   trakt_lista(const char *q, CatItem *s, int m) { (void)q; (void)s; (void)m; return 0; }
int   trakt_social(CatItem *s, int m)      { (void)s; (void)m; return 0; }
int   trakt_e_a_seguir(const char *id)     { (void)id; return 0; }
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
static long long pausadoTt2 = 900000;
int trakt_continuar(CatItem *s, int m) {
  static const char *ids[3] = { "tt1", "tt2", "tt3" };
  int i;
  for (i = 0; i < 3 && i < m; i++) {
    memset(&s[i], 0, sizeof s[i]);
    snprintf(s[i].imdb, sizeof s[i].imdb, "%s", ids[i]);
    snprintf(s[i].titulo, sizeof s[i].titulo, "Filme %s", ids[i]);
    snprintf(s[i].tipo, sizeof s[i].tipo, "movie");
    s[i].progresso = 40;
    s[i].retomadoMs = i == 1 ? pausadoTt2 : 900000 - i;
  }
  return i;
}

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

int main(void) {
  unsigned rev;
  prog_definir_relogio(relogioTeste);

  cicloCompleto();
  assert(nContinuar() == 3 && naContinuar("tt2"));
  conferirLista();

  // 1. REMOVER: a fileira publicada ja nao tem o item, no mesmo passo.
  rev = cat_revisao();
  assert(desc_tirar_continuar("tt2", 0, 0) == 1);
  assert(!naContinuar("tt2"));
  assert(nContinuar() == 2 && naContinuar("tt1") && naContinuar("tt3"));
  conferirLista();
  assert(cat_revisao() != rev);
  puts("ok  remover tira o card da fileira publicada e sobe a revisao");

  // 2. REFACAO com o Trakt ainda devolvendo tt2, paused_at de ANTES da
  //    remocao (900000 < 1000000): nao volta.
  agora = 1000500;
  fioContinuar(NULL);
  assert(!naContinuar("tt2") && nContinuar() == 2);
  conferirLista();
  puts("ok  refacao com paused_at mais velho que a remocao nao traz de volta");

  // 3a. FIO EM VOO: montou antes da remocao (tt2 no lote), publica depois.
  { CatItem lote[CONT_MAX]; int k;
    memset(lote, 0, sizeof lote);
    for (k = 0; k < 3; k++) {
      snprintf(lote[k].imdb, sizeof lote[k].imdb, "tt%d", k + 1);
      lote[k].progresso = 40; lote[k].retomadoMs = 900000 - k;
    }
    cat_trocar_continuar(lote, 3);
    assert(!naContinuar("tt2") && nContinuar() == 2);
    conferirLista(); }
  // 3b. CICLO COMPLETO que montou a fileira antes e publica depois.
  { CatItem lote[5]; CatFileira fs[2]; int k;
    memset(lote, 0, sizeof lote); memset(fs, 0, sizeof fs);
    for (k = 0; k < 3; k++) {
      snprintf(lote[k].imdb, sizeof lote[k].imdb, "tt%d", k + 1);
      lote[k].progresso = 40; lote[k].retomadoMs = 900000 - k;
    }
    snprintf(lote[3].imdb, sizeof lote[3].imdb, "tt90");
    snprintf(lote[4].imdb, sizeof lote[4].imdb, "tt91");
    snprintf(fs[0].chave, sizeof fs[0].chave, "continue_watching"); fs[0].n = 3;
    snprintf(fs[1].chave, sizeof fs[1].chave, "lista"); fs[1].ini = 3; fs[1].n = 2;
    cat_definir_tudo(lote, 5, fs, 2);
    assert(!naContinuar("tt2") && nContinuar() == 2);
    conferirLista(); }
  puts("ok  publicacao montada antes da remocao (refacao e ciclo) nao traz de volta");

  // 4. O RETRATO DA MONTAGEM (desc_remontar_fileiras roda a cada sync) e o
  //    de ANTES: continue_watching n=3 e "lista" em ini=3. cat_trocar_continuar
  //    e a remocao deslocaram as janelas publicadas sem tocar filsMontadas;
  //    republicar o retrato poria "lista" um titulo adiante e a retomada com
  //    o primeiro da lista dentro. A janela publicada vence.
  cicloCompleto();                 // retrato novo ja sem tt2 (n=2)
  filsMontadas[0].n = 3;           // o retrato velho de antes da remocao
  filsMontadas[1].ini = 3;
  desc_remontar_fileiras();
  assert(!naContinuar("tt2") && nContinuar() == 2);
  conferirLista();
  puts("ok  remontar sem rede nao devolve a janela velha");

  // 5a. ASSISTIU DE NOVO EM OUTRO APARELHO: paused_at depois da remocao.
  pausadoTt2 = 1000200;
  agora = 1000800;
  fioContinuar(NULL);
  assert(naContinuar("tt2") && nContinuar() == 3);
  conferirLista();
  puts("ok  paused_at mais novo que a remocao traz de volta");

  // 5b. ASSISTIU DE NOVO AQUI: o Trakt ainda com o paused_at velho, mas o
  //     player gravou local depois da remocao.
  assert(desc_tirar_continuar("tt3", 0, 0) == 1 && !naContinuar("tt3"));
  agora = 1001000;
  fioContinuar(NULL);
  assert(!naContinuar("tt3"));
  agora = 1002000;
  assert(prog_gravar_local("tt3", 0, 0, 600.0, 6000.0));
  fioContinuar(NULL);
  assert(naContinuar("tt3"));
  conferirLista();
  puts("ok  registro local mais novo que a remocao traz de volta");

  puts("cwremover: tudo ok");
  return 0;
}
