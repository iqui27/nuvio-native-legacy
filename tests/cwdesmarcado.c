// O "A SEGUIR" DO CONTINUAR ASSISTINDO RESPEITA O QUE A PESSOA DESMARCOU
// (Silo, tt14688458, TCL, 08/10/2026). O remoto (Trakt / Simkl / conta Nuvio)
// diz "ultimo visto T2E10", mas T2E7..T2E10 foram desmarcados NESTA TV: o
// proximo da fileira tem de ser o T2E7, nao o T3E1 (ou T2E11) do remoto.
//
// Como tests/cwordem_desc.c: inclui src/descoberta.c com Trakt/Simkl/conta
// FALSOS (sem rede), mas com o src/vistonao.c de VERDADE guardando as
// desmarcacoes. NO COMMIT PAI (sem o gancho desc_lapides_primeira) este
// arquivo COMPILA e FALHA nas assercoes: e a prova do defeito.
//
//   bash tests/cwdesmarcado.sh
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
#include "../src/vistonao.h"
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
static int proximoTeste = 1;
int   ajustes_cw_proximo(void) { return proximoTeste; }


// --- FONTES FALSAS -----------------------------------------------------------
#define SILO "tt14688458"
#define DIA (24LL * 60 * 60 * 1000)
static long long agoraMs, relogioNao;
static long long relogioVN(void) { return relogioNao; }
int sessao_logada(void) { return 0; }

// Trakt: um "a seguir" (tkId) OU um pausado (tkProg > 0), com o instante do
// ultimo visto (tkMs). Simkl: um "a seguir". Conta: uma semente.
static const char *tkId, *skId, *ctId;
static int tkProg, ctT, ctE;
static long long tkMs, skMs, ctMs;
static int simklAtivoTeste, traktAtivoTeste;
static int regSet(const char *id, const char *a) { return a && !strcmp(id, a); }
int trakt_ativo(void) { return traktAtivoTeste; }
int simkl_ativo(void) { return simklAtivoTeste; }
int trakt_progresso_ocultar(const char *i, int o) { (void)i; (void)o; return 0; }
int trakt_e_a_seguir(const char *id) { return !tkProg && regSet(id, tkId); }
int simkl_e_a_seguir(const char *id) { return regSet(id, skId); }
int trakt_continuar_falhou(void) { return 0; }
static void item(CatItem *s, const char *id, int prog, long long ms) {
  memset(s, 0, sizeof *s);
  snprintf(s->imdb, sizeof s->imdb, "%s", id);
  snprintf(s->tipo, sizeof s->tipo, "series");
  sscanf(strchr(id, ':') + 1, "%d:%d", &s->temporada, &s->episodio);
  s->progresso = prog;
  s->retomadoMs = ms;
  snprintf(s->nomeEpisodio, sizeof s->nomeEpisodio, "Episodio de %s", id);
  if (!prog) cwo_marcar_estreia(id, 1);
}
int trakt_continuar(CatItem *s, int m) {
  if (!tkId || m < 1) return 0;
  item(&s[0], tkId, tkProg, tkMs);
  return 1;
}
int simkl_continuar(CatItem *s, int m) {
  if (!skId || m < 1) return 0;
  item(&s[0], skId, 0, skMs);
  return 1;
}
int ajustes_cw_do_episodio_mais_alto(void) { return 1; }
int vistoep_estado(const char *imdb, int t, int e) { (void)imdb; (void)t; (void)e; return -1; }
int contalib_sementes_a_seguir(ContaSemente *s, int m, int a) {
  (void)a;
  if (!ctId || m < 1) return 0;
  snprintf(s[0].id, sizeof s[0].id, "%s", ctId);
  s[0].temporada = ctT; s[0].episodio = ctE; s[0].vistoMs = ctMs;
  return 1;
}
int trakt_enfeitar_lote(CatItem *s, int n) { (void)s; return n; }

static int falhas;
static void confere(const char *o_que, int ok) {
  printf("  %-72s %s\n", o_que, ok ? "ok" : "FALHOU");
  if (!ok) falhas++;
}
static long long relogioProg(void) { return 1000000; }

// Desmarca T2E<de>..T2E<ate> da Silo AGORA (relogio da desmarcacao = relogioNao).
static void desmarcar(int de, int ate) {
  VistoPar p[16];
  int n = 0, e;
  for (e = de; e <= ate; e++) { p[n].temporada = 2; p[n].episodio = (short)e; n++; }
  vistonao_gesto(SILO, p, n, 0);
}
static void limpar(void) {
  vistonao_esquecer();
  tkId = skId = ctId = NULL; tkProg = 0;
}
// Monta a fileira e devolve o 1o item (ou NULL se vazia).
static CatItem *primeiro(void) {
  static CatItem lote[CONT_MAX];
  return montarContinuar(lote, CONT_MAX) > 0 ? &lote[0] : NULL;
}
static int eh(CatItem *c, int t, int e) {
  return c && c->temporada == t && c->episodio == e && c->progresso == 0;
}

int main(void) {
  CatItem *c;
  prog_definir_relogio(relogioProg);
  agoraMs = (long long)time(NULL) * 1000LL;
  relogioNao = agoraMs - 3 * DIA;           // quando a pessoa desmarcou
  vistonao_relogio(relogioVN);
#ifdef TEM_AJUSTE
  desc_lapides_primeira(vistonao_primeira);
#endif

  // ---------------- TRAKT (fonte so Trakt) ----------------
  fonteTeste = 2; traktAtivoTeste = 1;
  limpar(); desmarcar(7, 10);
  tkId = SILO ":3:1"; tkMs = relogioNao - DIA;      // visto ANTES do gesto
  c = primeiro();
  confere("trakt: remoto T3E1, T2E7..E10 desmarcados -> T2E7", eh(c, 2, 7));
  confere("trakt: o id do card acompanha (tt:2:7)", c && !strcmp(c->imdb, SILO ":2:7"));
  confere("trakt: o nome do episodio do remoto nao fica no card", c && !c->nomeEpisodio[0]);
  proximoTeste = 0;
  confere("trakt: o ajustado ainda e 'a seguir' (Proximo no Continuar desligado o tira)",
          primeiro() == NULL);
  proximoTeste = 1;

  limpar(); desmarcar(7, 10);
  tkId = SILO ":2:11"; tkMs = relogioNao - DIA;
  confere("trakt: remoto T2E11 -> T2E7", eh(primeiro(), 2, 7));

  limpar(); desmarcar(9, 10);
  tkId = SILO ":3:1"; tkMs = relogioNao - DIA;
  confere("trakt: so E9 e E10 desmarcados -> T2E9", eh(primeiro(), 2, 9));

  limpar();
  tkId = SILO ":3:1"; tkMs = relogioNao - DIA;
  confere("trakt: SEM desmarcacao -> T3E1 como veio", eh(primeiro(), 3, 1));
  limpar();
  tkId = SILO ":2:11"; tkMs = relogioNao - DIA;
  confere("trakt: SEM desmarcacao -> T2E11 como veio", eh(primeiro(), 2, 11));

  limpar(); desmarcar(7, 10);
  tkId = SILO ":3:1"; tkMs = relogioNao + 3600LL * 1000;   // visto 1 h DEPOIS
  confere("trakt: visto remoto genuinamente mais novo (1 h) -> nao ajusta", eh(primeiro(), 3, 1));
  limpar(); desmarcar(7, 10);
  tkId = SILO ":3:1"; tkMs = relogioNao + 60LL * 1000;     // dentro da folga de 2 min
  confere("trakt: visto 1 min depois (folga de 2 min) -> ainda ajusta", eh(primeiro(), 2, 7));

  limpar(); desmarcar(7, 10);
  tkId = SILO ":2:10"; tkProg = 40; tkMs = relogioNao - DIA;
  c = primeiro();
  confere("trakt: pausado a 40% nao e 'a seguir': fica como veio",
          c && c->temporada == 2 && c->episodio == 10 && c->progresso == 40);

  // ---------------- SIMKL ----------------
  fonteTeste = 3; simklAtivoTeste = 1; traktAtivoTeste = 0;
  limpar(); desmarcar(7, 10);
  skId = SILO ":3:1"; skMs = relogioNao - DIA;
  confere("simkl: remoto T3E1, T2E7..E10 desmarcados -> T2E7", eh(primeiro(), 2, 7));
  limpar();
  skId = SILO ":3:1"; skMs = relogioNao - DIA;
  confere("simkl: SEM desmarcacao -> T3E1 como veio", eh(primeiro(), 3, 1));
  limpar(); desmarcar(7, 10);
  skId = SILO ":3:1"; skMs = relogioNao + 3600LL * 1000;
  confere("simkl: visto remoto mais novo -> nao ajusta", eh(primeiro(), 3, 1));

  // ---------------- CONTA NUVIO ----------------
  fonteTeste = 1; simklAtivoTeste = 0; traktAtivoTeste = 0;;
  limpar(); desmarcar(7, 10);
  ctId = SILO; ctT = 2; ctE = 11; ctMs = relogioNao - DIA;
  confere("conta: semente T2E11, T2E7..E10 desmarcados -> T2E7", eh(primeiro(), 2, 7));
  limpar();
  ctId = SILO; ctT = 2; ctE = 11; ctMs = relogioNao - DIA;
  confere("conta: SEM desmarcacao -> T2E11 como veio", eh(primeiro(), 2, 11));
  limpar(); desmarcar(7, 10);
  ctId = SILO; ctT = 2; ctE = 11; ctMs = relogioNao + 3600LL * 1000;
  confere("conta: visto da conta mais novo -> nao ajusta", eh(primeiro(), 2, 11));

  printf("%s\n", falhas ? "FALHOU" : "tudo ok");
  return falhas ? 1 : 0;
}
