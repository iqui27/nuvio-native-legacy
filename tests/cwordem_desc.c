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
const char *i18n(const char *s)         { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }
const char *dados_dir(void)             { return ""; }
const char *sessao_usuario(void)        { return ""; }
int         perfis_ativo(void)          { return 1; }
char *dados_ler(const char *nome)                 { (void)nome; return NULL; }
int   dados_gravar(const char *nome, const char *c) { (void)nome; (void)c; return 1; }
int   dados_apagar(const char *nome)              { (void)nome; return 1; }
void  SDL_Delay(Uint32 ms)                 { usleep(ms * 1000); }
static int fonteTeste;          // 0 = AJ_CWF_AMBAS, 2 = so o Trakt
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
int   addons_fornece(int i, int oque)     { (void)i; (void)oque; return 0; }
int   addons_sondado(int i)              { (void)i; return 0; }
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
static int simklAtivoTeste;
int   simkl_ativo(void)                    { return simklAtivoTeste; }
int   simkl_continuar(CatItem *s, int m)   { (void)s; (void)m; return 0; }
int   simkl_plantowatch(CatItem *s, int m) { (void)s; (void)m; return 0; }
int   ajustes_salvos_no_simkl(void)        { return 0; }
// O enfeite falso: para o "a seguir" DA CONTA (#199) faz o que trakt.c faz —
// anota a estreia e descarta a serie que acabou (ver CONTA abaixo).
int   trakt_enfeitar_lote(CatItem *s, int n);
int   trakt_lista(const char *q, CatItem *s, int m) { (void)q; (void)s; (void)m; return 0; }
int   trakt_lista_cresc(const char *q, CatItem **s, int m) { (void)q; (void)s; (void)m; return 0; }
// O servico social proprio (recomenda.c) fica fora deste teste: a uniao e so o que o Trakt trouxe.
int   recomenda_social_mesclar(CatItem *i, int nTrakt, int max) { (void)i; (void)max; return nTrakt; }
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
static int concluidoTeste = 90;         // Percentual assistido; 90 de fabrica
int   ajustes_cw_concluido(void)           { return concluidoTeste; }
int ajustes_cw_mostrar_nao_exibidos(void) { return naoExibidosTeste; }
int   ajustes_cw_proximo(void) { return 1; }

// --- O "TRAKT" FALSO ---------------------------------------------------------
// Por instante (o mais recente primeiro depois da ordenacao da montagem):
//   ttA:1:3   a seguir, estreia daqui a 10 dias  (futuro)
//   tt1       pausado
//   ttB:2:1   a seguir, estreia amanha           (futuro)
//   ttC:1:8   a seguir, foi ao ar ontem
//   tt2       pausado
#define DIA (24LL * 60 * 60 * 1000)
static long long agoraMs;
typedef struct { const char *id; int seguir, prog; long long quando, estreiaDias; } Falso;
static const Falso FALSO[] = {
  { "ttA:1:3", 1,  0, 900500,  10 },
  { "tt1",     0, 40, 900400,   0 },
  { "ttB:2:1", 1,  0, 900300,   1 },
  { "ttC:1:8", 1,  0, 900200,  -1 },
  { "tt2",     0, 60, 900100,   0 },
};
// BROTHERS (C9 do dono, 24/09): o Trakt manda 10 itens, TODOS "a seguir", e o
// mais recente e Brothers S1E6 — que estreia em 21/10. A conta tem 12 em
// andamento, um deles Brothers S1E4 (mais velho que o S1E6 do Trakt). Sao 21
// candidatos para 12 lugares. Com o slice simples o futuro era sempre o
// cortado: "Separar futuros" dava "0 futuro(s)" e Brothers sumia da home.
static const Falso BROTHERS[] = {
  { "tt6773088:1:6",  1, 0, 900900,  27 },
  { "tt32260680:1:2", 1, 0, 900800,  -2 },
  { "tt10541088:1:2", 1, 0, 900700,  -3 },
  { "tt11691774:1:2", 1, 0, 900600,  -4 },
  { "tt31091039:5:4", 1, 0, 900500,  -5 },
  { "tt31937954:1:3", 1, 0, 900400, -183 },
  { "tt2304589:1:2",  1, 0, 900300,  -6 },
  { "tt13111078:1:2", 1, 0, 900200,  -7 },
  { "tt10986410:1:2", 1, 0, 900100,  -8 },
  { "tt8599532:1:2",  1, 0, 900000,  -9 },
};
static const Falso *tabela = FALSO;
static int semDataPrimeiro;   // 1 = o primeiro da tabela vem sem `released`
static int nTabela = (int)(sizeof FALSO / sizeof *FALSO);
int   trakt_progresso_ocultar(const char *i, int o) { (void)i; (void)o; return 0; }
#define NFALSO nTabela
int trakt_e_a_seguir(const char *id) {
  int i;
  for (i = 0; i < NFALSO; i++) if (!strcmp(tabela[i].id, id)) return tabela[i].seguir;
  return 0;
}
int simkl_e_a_seguir(const char *id) { (void)id; return 0; }
static int traktFalhouTeste;    // #151: o Trakt respondeu HTTP 500
int trakt_continuar(CatItem *s, int m) {
  int i;
  if (traktFalhouTeste) return 0;
  for (i = 0; i < NFALSO && i < m; i++) {
    const Falso *f = &tabela[i];
    memset(&s[i], 0, sizeof s[i]);
    snprintf(s[i].imdb, sizeof s[i].imdb, "%s", f->id);
    // Id composto e episodio de serie, "a seguir" ou pausado.
    snprintf(s[i].tipo, sizeof s[i].tipo, "%s", strchr(f->id, ':') ? "series" : "movie");
    if (strchr(f->id, ':')) sscanf(strchr(f->id, ':') + 1, "%d:%d", &s[i].temporada, &s[i].episodio);
    s[i].progresso = f->prog;
    s[i].retomadoMs = f->quando;
    // O que trakt.c faz no enfeite, com o `released` do Cinemeta.
    if (f->seguir)
      cwo_marcar_estreia(f->id, semDataPrimeiro && i == 0 ? CWO_SEM_DATA
                                                         : agoraMs + f->estreiaDias * DIA);
  }
  return i;
}
int   trakt_continuar_falhou(void)        { return traktFalhouTeste; }

// --- "A SEGUIR" DA CONTA (issue #199) -----------------------------------------
// Sem Trakt/Simkl no ar, as sementes vem dos vistos da conta. Por semente: o
// episodio sugerido, quando o episodio-ancora foi visto, a estreia (dias a
// partir de agora) e se o Cinemeta confirma que ele existe.
//   ttT:4:10  Ted Lasso, estreia daqui a 6 dias          (futuro)
//   ttM:1:4   MobLand, foi ao ar ontem
//   tt7000001:1:9  serie JA PAUSADA na conta              (fica fora)
//   ttX:9:1   serie que acabou: o Cinemeta nao tem         (fica fora)
static int traktAtivoTeste = 1, contaSementesTeste;
int trakt_ativo(void) { return traktAtivoTeste; }
int ajustes_cw_do_episodio_mais_alto(void) { return 1; }
typedef struct { const char *id; int t, e; long long visto, estreiaDias; int existe; } SemFalsa;
static const SemFalsa CONTA[] = {
  { "ttT",       4, 10, 900950,  6, 1 },
  { "ttM",       1,  4, 900940, -1, 1 },
  { "tt7000001", 1,  9, 900930, -2, 1 },
  { "ttX",       9,  1, 900920,  0, 0 },
};
#define NCONTA ((int)(sizeof CONTA / sizeof *CONTA))
// contaSementesTeste: 0 = vistos lidos e sem semente; 1 = as quatro acima;
// -1 = os vistos da conta NAO foram puxados (ou estao velhos): contrato de
// contalib_sementes_a_seguir. contaBaseMs soma ao instante de cada ancora: 0
// deixa os "de 1970" dos testes antigos; os do vinculo (Trakt no ar) usam um
// instante real, que e o que a janela de 60 dias mede.
static long long contaBaseMs;
static const char *contaAntigaId;     // semente cuja ancora foi ha 61 dias
static const char *vistoEmOutraTeste; // episodio "id:t:e" que outra fonte viu
int vistoep_estado(const char *imdb, int t, int e) {
  char k[48];
  snprintf(k, sizeof k, "%s:%d:%d", imdb, t, e);
  return vistoEmOutraTeste && !strcmp(k, vistoEmOutraTeste) ? 1 : -1;
}
int contalib_sementes_a_seguir(ContaSemente *s, int m, int a) {
  int i;
  (void)a;
  if (contaSementesTeste < 0) return -1;
  if (!contaSementesTeste) return 0;
  for (i = 0; i < NCONTA && i < m; i++) {
    snprintf(s[i].id, sizeof s[i].id, "%s", CONTA[i].id);
    s[i].temporada = CONTA[i].t;
    s[i].episodio = CONTA[i].e;
    s[i].vistoMs = contaBaseMs + CONTA[i].visto;
    if (contaAntigaId && !strcmp(contaAntigaId, CONTA[i].id))
      s[i].vistoMs -= 61LL * 24 * 3600 * 1000;
  }
  return i;
}
static int consultadas;
int trakt_enfeitar_lote(CatItem *s, int n) {
#ifdef CWLOCAL_ENFEITAR
  return cwlocal_enfeitar_lote(s, n);
#else
  int r, w = 0, k;
  for (r = 0; r < n; r++) {
    int fica = 1;
    if (cwo_conta_a_seguir(s[r].imdb)) {
      consultadas++;
      for (k = 0; k < NCONTA; k++) {
        char id[40];
        snprintf(id, sizeof id, "%s:%d:%d", CONTA[k].id, CONTA[k].t, CONTA[k].e);
        if (strcmp(id, s[r].imdb)) continue;
        fica = CONTA[k].existe;
        if (fica) cwo_marcar_estreia(id, agoraMs + CONTA[k].estreiaDias * DIA);
      }
    }
    if (fica) s[w++] = s[r];
  }
  return w;
#endif
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
    puts("ok  fileira curta: o futuro fica com o lugar reservado, o outro sai"); }

  { static const char *const e[] = { "tt1", "ttC:1:8", "tt2" };
    naoExibidosTeste = 0;
    conferir("nao exibidos desligado", e, 3);
    assert(!cwo_e_futuro("ttA:1:3") && !cwo_e_futuro("ttB:2:1"));
    modoTeste = CWO_PADRAO;
    conferir("nao exibidos desligado (padrao)", e, 3);
    puts("ok  nao exibidos desligado: futuros saem em qualquer modo"); }

  // --- BROTHERS: fileira cheia, o futuro e o mais recente ---------------------
  { int k;
    tabela = BROTHERS;
    nTabela = (int)(sizeof BROTHERS / sizeof *BROTHERS);
    for (k = 0; k < 12; k++) {
      ProgRegistro r;
      memset(&r, 0, sizeof r);
      if (k == 0) {             // Brothers S1E4 na conta, MAIS VELHO que o S1E6 do Trakt
        snprintf(r.contentId, sizeof r.contentId, "tt6773088");
        r.temporada = 1; r.episodio = 4;
      } else snprintf(r.contentId, sizeof r.contentId, "tt70000%02d", k);
      r.posSeg = 1200; r.durSeg = 3000;       // 40%
      r.lastWatchedMs = 800000 - k * 100;
      assert(prog_aplicar_remoto(&r));
    }
    naoExibidosTeste = 1;

    // Padrao: pelo instante, Brothers (o mais recente) primeiro.
    { CatItem lote[CONT_MAX];
      int nc;
      modoTeste = CWO_PADRAO;
      nc = montarContinuar(lote, CONT_MAX);
      assert(nc == 12 && !strcmp(lote[0].imdb, "tt6773088:1:6"));
      for (k = 1; k < nc; k++) assert(strncmp(lote[k].imdb, "tt6773088", 9));
      assert(!cwo_e_futuro("tt6773088:1:6"));
      puts("ok  brothers padrao: o S1E6 do Trakt na frente, o S1E4 da conta fora"); }

    // Separar futuros: Brothers FICA na lista (no fim) e e publicado como futuro.
    { CatItem lote[CONT_MAX];
      int nc;
      modoTeste = CWO_SEPARAR;
      nc = montarContinuar(lote, CONT_MAX);
      assert(nc == 12);
      assert(!strcmp(lote[11].imdb, "tt6773088:1:6"));
      assert(!strcmp(lote[0].imdb, "tt32260680:1:2"));
      assert(cwo_e_futuro("tt6773088:1:6"));
      assert(!cwo_e_futuro("tt32260680:1:2") && !cwo_e_futuro("tt31937954:1:3"));
      puts("ok  brothers separar: S1E6 publicado como futuro, nao cortado");
      modoTeste = CWO_STREAMING;
      nc = montarContinuar(lote, CONT_MAX);
      assert(nc == 12 && !strcmp(lote[11].imdb, "tt6773088:1:6"));
      puts("ok  brothers streaming: S1E6 no fim da fileira"); }

    // showUnairedNextUp desligado: Brothers sai, e a fileira continua cheia.
    { CatItem lote[CONT_MAX];
      int nc;
      naoExibidosTeste = 0;
      modoTeste = CWO_SEPARAR;
      nc = montarContinuar(lote, CONT_MAX);
      assert(nc == 12);
      for (k = 0; k < nc; k++) assert(strncmp(lote[k].imdb, "tt6773088", 9));
      assert(!cwo_e_futuro("tt6773088:1:6"));
      puts("ok  brothers nao exibidos desligado: S1E6 escondido, fileira cheia"); }

    // Sem data (o Cinemeta nao deu `released`): conta como exibido, fica na
    // frente pelo instante e nao e publicado como futuro.
    { CatItem lote[CONT_MAX];
      int nc;
      naoExibidosTeste = 1;
      modoTeste = CWO_SEPARAR;
      semDataPrimeiro = 1;
      nc = montarContinuar(lote, CONT_MAX);
      semDataPrimeiro = 0;
      assert(nc == 12 && !strcmp(lote[0].imdb, "tt6773088:1:6"));
      assert(!cwo_e_futuro("tt6773088:1:6"));
      puts("ok  brothers sem data: conta como exibido (e o log diz)"); }
    naoExibidosTeste = 1; }

  // #151: TRAKT SEM RESPOSTA NAO ESVAZIA A FILEIRA. Com a fonte so no Trakt a
  // lista nova sai vazia; o que estava publicado fica. Com o Trakt de volta e
  // de verdade vazio, a fileira esvazia (o guarda e so para "nao sei").
  { CatItem lote[CONT_MAX], velho[3];
    int nc, k;
    memset(velho, 0, sizeof velho);
    for (k = 0; k < 3; k++) {
      snprintf(velho[k].imdb, sizeof velho[k].imdb, "ttV%d", k);
      snprintf(velho[k].tipo, sizeof velho[k].tipo, "movie");
    }
    cat_trocar_continuar(velho, 3);
    fonteTeste = 2;
    traktFalhouTeste = 1;
    nc = montarContinuar(lote, CONT_MAX);
    assert(nc == 3 && !strcmp(lote[0].imdb, "ttV0") && !strcmp(lote[2].imdb, "ttV2"));
    puts("ok  trakt sem resposta: a fileira publicada fica (#151)");
    traktFalhouTeste = 0;
    nTabela = 0;
    nc = montarContinuar(lote, CONT_MAX);
    assert(nc == 0);
    puts("ok  trakt respondeu vazio: a fileira esvazia");
    fonteTeste = 0; }

  // PAUSADO VELHO DO TRAKT x EPISODIO MAIS NOVO VISTO AQUI. ttP: o Trakt ainda
  // tem o S1E3 pausado (40%), mas esta TV terminou o S1E5 depois: o item vira
  // a semente S1E5 terminada (o card passa ao proximo), e o S1E3 nao volta.
  // ttQ: o Trakt esta ADIANTE (S2E1) do S1E9 visto aqui: fica como veio.
  { static const Falso NOVO[] = {
      { "ttP:1:3", 0, 40, 700000, 0 },
      { "ttQ:2:1", 0, 30, 690000, 0 },
    };
    CatItem lote[CONT_MAX];
    ProgRegistro r;
    int nc;
    memset(&r, 0, sizeof r);
    snprintf(r.contentId, sizeof r.contentId, "ttP");
    r.temporada = 1; r.episodio = 5; r.posSeg = 3000; r.durSeg = 3000;
    r.lastWatchedMs = 950000;
    assert(prog_aplicar_remoto(&r));
    snprintf(r.contentId, sizeof r.contentId, "ttQ");
    r.temporada = 1; r.episodio = 9; r.lastWatchedMs = 940000;
    assert(prog_aplicar_remoto(&r));
    tabela = NOVO;
    nTabela = 2;
    fonteTeste = 2;
    modoTeste = CWO_PADRAO;
    nc = montarContinuar(lote, CONT_MAX);
    assert(nc == 2);
    assert(!strcmp(lote[0].imdb, "ttP:1:5") && lote[0].temporada == 1 &&
           lote[0].episodio == 5 && lote[0].progresso == 100);
    assert(!strcmp(lote[1].imdb, "ttQ:2:1") && lote[1].progresso == 30);
    puts("ok  trakt velho: S1E3 nao volta por cima do S1E5 visto aqui; o adiante fica");
    fonteTeste = 0; nTabela = 0; }

  // PERCENTUAL ASSISTIDO: 92% conta como assistido com o padrao (90) e como em
  // andamento com 95.
  { CatItem lote[CONT_MAX];
    ProgRegistro r;
    int nc, k, achou;
    memset(&r, 0, sizeof r);
    snprintf(r.contentId, sizeof r.contentId, "ttR");
    r.temporada = 1; r.episodio = 1; r.posSeg = 2760; r.durSeg = 3000;  // 92%
    r.lastWatchedMs = 960000;
    assert(prog_aplicar_remoto(&r));
    fonteTeste = 1;
    nc = montarContinuar(lote, CONT_MAX);
    for (k = 0, achou = 0; k < nc; k++) if (!strncmp(lote[k].imdb, "ttR", 3)) achou = 1;
    assert(!achou);
    concluidoTeste = 95;
    nc = montarContinuar(lote, CONT_MAX);
    assert(nc > 0 && !strcmp(lote[0].imdb, "ttR:1:1") && lote[0].progresso == 92);
    puts("ok  percentual assistido: 92% sai com 90 e fica com 95");
    concluidoTeste = 90; fonteTeste = 0; }

  // #199: SO A CONTA, SEM TRAKT NEM SIMKL. As 12 pausadas da conta (Brothers
  // S1E4 e tt7000001..11) continuam no disco; os vistos da conta semeiam o "a
  // seguir". Os dois mais novos entram no lugar dos pausados mais velhos.
  { CatItem lote[CONT_MAX];
    int nc, k;
    fonteTeste = 1;               // AJ_CWF_CONTA
    traktAtivoTeste = 0;
    contaSementesTeste = 1;
    modoTeste = CWO_PADRAO;
    naoExibidosTeste = 1;
    consultadas = 0;
    nc = montarContinuar(lote, CONT_MAX);
    assert(nc == 12);
    assert(!strcmp(lote[0].imdb, "ttT:4:10") && !strcmp(lote[1].imdb, "ttM:1:4"));
    assert(lote[0].progresso == 0 && lote[0].temporada == 4 && lote[0].episodio == 10);
    // A semente da serie ja pausada nem vai ao Cinemeta; a que acabou vai e sai.
    assert(consultadas == 3);
    for (k = 0; k < nc; k++)
      assert(strncmp(lote[k].imdb, "ttX", 3) && strcmp(lote[k].imdb, "tt7000001:1:9"));
    assert(cwo_conta_a_seguir("ttT:4:10") && cwo_conta_a_seguir("ttM:1:4"));
    assert(!cwo_e_futuro("ttT:4:10"));
    puts("ok  #199 conta: a seguir dos vistos entra, pausado e serie acabada ficam fora");

    // Ordenacao "Separar futuros": Ted Lasso (estreia em 6 dias) vira futuro.
    modoTeste = CWO_SEPARAR;
    nc = montarContinuar(lote, CONT_MAX);
    assert(nc == 12 && !strcmp(lote[11].imdb, "ttT:4:10"));
    assert(cwo_e_futuro("ttT:4:10") && !cwo_e_futuro("ttM:1:4"));
    puts("ok  #199 conta separar: o nao lancado vai para Proximos episodios");

    // "Mostrar nao exibidos" desligado: o futuro sai, o que ja foi ao ar fica.
    naoExibidosTeste = 0;
    modoTeste = CWO_PADRAO;
    nc = montarContinuar(lote, CONT_MAX);
    assert(nc == 12 && !strcmp(lote[0].imdb, "ttM:1:4"));
    for (k = 0; k < nc; k++) assert(strncmp(lote[k].imdb, "ttT", 3));
    puts("ok  #199 conta nao exibidos desligado: so o nao lancado sai");
    naoExibidosTeste = 1;

    // Fonte AMBAS com o Trakt no ar: o "a seguir" e dele, a conta nao semeia.
    traktAtivoTeste = 1;
    fonteTeste = 0;
    consultadas = 0;
    nc = montarContinuar(lote, CONT_MAX);
    assert(consultadas == 0 && !cwo_conta_a_seguir("ttT:4:10"));
    for (k = 0; k < nc; k++) assert(strncmp(lote[k].imdb, "ttT", 3) && strncmp(lote[k].imdb, "ttM", 3));
    puts("ok  #199 AMBAS com Trakt no ar: os vistos da conta nao semeiam");

    // FONTE = CONTA com o Trakt no ar: a conta e quem da o "a seguir".
    fonteTeste = 1;
    contaBaseMs = agoraMs - 3600LL * 1000;
    consultadas = 0;
    nc = montarContinuar(lote, CONT_MAX);
    assert(cwo_conta_a_seguir("ttT:4:10") && cwo_conta_a_seguir("ttM:1:4"));
    puts("ok  conta + Trakt vinculado: a conta semeia o a seguir");

    // (a) vistos nao puxados / velhos: nenhuma semente.
    contaSementesTeste = -1;
    consultadas = 0;
    nc = montarContinuar(lote, CONT_MAX);
    assert(consultadas == 0 && !cwo_conta_a_seguir("ttT:4:10") && !cwo_conta_a_seguir("ttM:1:4"));
    contaSementesTeste = 1;
    puts("ok  conta sem vistos puxados: nao semeia");

    // (b) outra fonte (Trakt/Simkl/local) ja viu o episodio: nao volta.
    vistoEmOutraTeste = "ttM:1:4";
    nc = montarContinuar(lote, CONT_MAX);
    assert(!cwo_conta_a_seguir("ttM:1:4") && cwo_conta_a_seguir("ttT:4:10"));
    for (k = 0; k < nc; k++) assert(strcmp(lote[k].imdb, "ttM:1:4"));
    vistoEmOutraTeste = NULL;
    puts("ok  conta + Trakt: episodio visto em outra fonte nao volta");

    // (c) serie parada ha 61 dias: nao ressuscita quando ha outra fonte...
    contaAntigaId = "ttM";
    nc = montarContinuar(lote, CONT_MAX);
    assert(!cwo_conta_a_seguir("ttM:1:4") && cwo_conta_a_seguir("ttT:4:10"));
    // ...mas sozinha (sem Trakt/Simkl) a conta mantem o comportamento da #199.
    traktAtivoTeste = 0;
    nc = montarContinuar(lote, CONT_MAX);
    assert(cwo_conta_a_seguir("ttM:1:4"));
    contaAntigaId = NULL;
    traktAtivoTeste = 1;
    puts("ok  conta + Trakt: serie parada ha 61 dias nao ressuscita");
    contaBaseMs = 0;
    contaSementesTeste = 0;
    fonteTeste = 0; }
  puts("cwordem_desc: tudo ok");
  return 0;
}
