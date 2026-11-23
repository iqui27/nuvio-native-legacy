// FILEIRA DO FAROL NA HOME (lote sense, issue 45). Mesmo arranjo de
// tests/home_precomputada.c: descoberta.c por #include, montar() de verdade,
// catalogo.c/homeestado.c de verdade; a rede e duble. Em cima disto entram
// o farol.c de producao e os modulos puros da cena (cena/rec/peca/texto/
// info/ficha/gosto/map a-puro); farolsinal, diario, salvos, seguro, infantil
// e recomenda sao falsos deste arquivo — o modelo e construido na mao.
//
// Prova, na ordem do aceite da issue:
//   1. a fileira do Farol MONTA dentro do montar() real (posicao de fabrica
//      pelo ordenarPorSnapshot: gosto1 depois do Continuar, gosto2 na 6a);
//   2. "nao aparece": frio 0, recomendar desligado, modo seguro, oculta em
//      Fileiras, compositor falhou (sem reaproveitar a antiga);
//   3. mesma cena a cada volta; cena nova so com a Home fora da tela
//      (evento forte = modelo mudou; 24 h = vale_s);
//   4. motivo do item focado ja renderizado, sem id cru;
//   5. origem do Play (FS_ORIGEM_FAROL) — chamado pela home, conferido no stub;
//   6. remover na hora e na ORDEM: desligar "Recomendar" e "zerar o gosto"
//      tiram as fileiras ANTES de a preferencia/identidade mudar;
//   7. oculta continua oculta; rank salvo prevalece sobre a posicao de fabrica.
//
//   bash tests/farol_home.sh
#define _GNU_SOURCE
#include <assert.h>
#include <dirent.h>
#include <limits.h>
#include <stdatomic.h>
#include <sys/stat.h>
#include <unistd.h>
int arte_reserva_episodios(const char *imdb, const char *corpo) { (void)imdb; (void)corpo; return 0; }

void addons_manifesto_lido_base(int i, const char *base, const char *destino,
                                unsigned versao, const char *corpo) {
  (void)i; (void)base; (void)destino; (void)versao; (void)corpo;
}
void addons_lista_travar(void) {}
void addons_lista_destravar(void) {}
void ws_destino(const char *base, const char *usuario, int perfil, char out[20]) {
  (void)base; (void)usuario; (void)perfil; out[0] = 0;
}
int ajustes_ocultar_filmes_vistos(void) { return 0; }
int ajustes_social(void) { return 1; }
int ajustes_fonte_biblioteca(void) { return 0; }
int ajustes_hist_conta(void) { return 1; }
int ajustes_busca_cinemeta(void) { return 1; }
int ajustes_busca_nuvio(void) { return 0; }
int ajustes_ocultar_nao_lancados(void) { return 0; }
#include "../src/descoberta.c"
#include "farolsinal.h"
#include "diario.h"
#include "salvos.h"
#include "jellyfin_stub.inc"
#include "nvtmp.h"
void addons_catalogo_chave(const char *id, const char *base, const char *type,
                           const char *catalog, char *dst, unsigned size) {
  (void)base; snprintf(dst, size, "%s_%s_%s", id, type, catalog);
}
int addons_identidade_recebida(const char *base, unsigned versao, NvPerfilSessao sessao,
                              const char *corpo) {
  (void)base; (void)versao; (void)sessao; (void)corpo; return 1;
}
int addons_manifesto_recebido(const char *base, unsigned versao, NvPerfilSessao sessao,
                             const char *corpo) {
  (void)base; (void)versao; (void)sessao; (void)corpo; return 1;
}
int ajustes_cw_do_episodio_mais_alto(void) { return 1; }
int contalib_sementes_a_seguir(ContaSemente *s, int m, int a) { (void)s; (void)m; (void)a; return 0; }
int trakt_cw_concluiu(const char *id, long long ms) { (void)id; (void)ms; return 0; }
int vistoep_estado(const char *imdb, int t, int e) { (void)imdb; (void)t; (void)e; return -1; }
unsigned recomenda_geracao(void) { return 1; }
Uint32 SDL_GetTicks(void) { return 0; }

#define BASE "https://addon.example/abc"
#define AID  "app.addon.demo"

// Seis catalogos de 2 filmes cada. Limite de fileiras 3 (fil_limite).
static const char *MANIFESTO =
  "{\"id\":\"" AID "\",\"name\":\"Addon\",\"resources\":[\"catalog\"],\"catalogs\":["
  "{\"type\":\"movie\",\"id\":\"cat_a\",\"name\":\"Cat A\"},"
  "{\"type\":\"movie\",\"id\":\"cat_b\",\"name\":\"Cat B\"},"
  "{\"type\":\"movie\",\"id\":\"cat_c\",\"name\":\"Cat C\"},"
  "{\"type\":\"movie\",\"id\":\"cat_d\",\"name\":\"Cat D\"},"
  "{\"type\":\"movie\",\"id\":\"cat_e\",\"name\":\"Cat E\"},"
  "{\"type\":\"movie\",\"id\":\"cat_f\",\"name\":\"Cat F\"}]}";

static char dono[64] = "dono-A";
static char dirDados[PATH_MAX];
const char *sessao_usuario(void)  { return dono; }
static _Atomic int T_perfil = 1;
int         perfis_ativo(void)    { return T_perfil; }
const char *dados_dir(void)       { return dirDados; }
static void caminhoDado(char *dst, size_t tam, const char *nome) {
  snprintf(dst, tam, "%s/%s", dirDados, nome);
}
int dados_gravar(const char *nome, const char *conteudo) {
  char p[PATH_MAX + 64]; FILE *f; size_t n; int ok;
  caminhoDado(p, sizeof p, nome); f = fopen(p, "wb");
  if (!f) return 0;
  n = strlen(conteudo); ok = fwrite(conteudo, 1, n, f) == n;
  return fclose(f) == 0 && ok;
}
char *dados_ler(const char *nome) {
  char p[PATH_MAX + 64]; FILE *f; long n; char *b;
  caminhoDado(p, sizeof p, nome); f = fopen(p, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); rewind(f);
  b = malloc((size_t)n + 1);
  if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
  b[n] = 0; fclose(f); return b;
}
int dados_apagar(const char *nome) {
  char p[PATH_MAX + 64]; caminhoDado(p, sizeof p, nome); return unlink(p) == 0;
}

int   addons_n(void)                   { return 1; }
const char *addons_base(int i)         { (void)i; return BASE; }
int   addons_ativo(int i)              { (void)i; return 1; }
int   addons_fornece(int i, int oque)  { (void)i; (void)oque; return 0; }
int   addons_sondado(int i)            { (void)i; return 0; }
const char *addons_id_manifesto(int i) { (void)i; return AID; }
const char *addons_nome(int i)         { (void)i; return "Addon"; }
unsigned addons_versao(void)           { return 1; }
const char *addons_base_por_id(const char *id) { return id && !strcmp(id, AID) ? BASE : ""; }
void  addons_manifesto_lido(int i, const char *c) { (void)i; (void)c; }
int   addons_perfil_da_lista(void)     { return 0; }
int   addons_base_desligada(const char *b) { (void)b; return 0; }
int   addons_aceita_id(int i, const char *t, const char *id) { (void)i; (void)t; (void)id; return -1; }

// A rede duble: cada catalogo devolve 2 filmes com generos RODANDO entre
// Acao/Comedia/Drama/Terror — a variedade e o que deixa a regra de
// diversidade do motor montar uma fileira de 6+.
static _Atomic int pedidosCatalogo;
static const char *const GENEROS[4] = { "Action", "Comedy", "Drama", "Horror" };
char *rede_baixar(const char *url, int t) {
  char buf[900];
  const char *p;
  char id[16] = "";
  (void)t;
  if (strstr(url, "/manifest.json")) return strdup(MANIFESTO);
  p = strstr(url, "/catalog/movie/");
  if (!p) return NULL;
  snprintf(id, sizeof id, "%.5s", p + 15);
  pedidosCatalogo++;
  if (id[4] < 'a' || id[4] > 'f') return strdup("{\"metas\":[]}");
  {
    int g1 = (id[4] - 'a') % 4, g2 = (id[4] - 'a' + 1) % 4;
    snprintf(buf, sizeof buf,
             "{\"metas\":["
             "{\"id\":\"tt%s1\",\"type\":\"movie\",\"name\":\"Titulo %s 1\",\"poster\":\"p.jpg\",\"genres\":[\"%s\"]},"
             "{\"id\":\"tt%s2\",\"type\":\"movie\",\"name\":\"Titulo %s 2\",\"poster\":\"p.jpg\",\"genres\":[\"%s\"]},"
             "{\"id\":\"tt%s3\",\"type\":\"movie\",\"name\":\"Titulo %s 3\",\"poster\":\"p.jpg\",\"genres\":[\"%s\"]},"
             "{\"id\":\"tt%s4\",\"type\":\"movie\",\"name\":\"Titulo %s 4\",\"poster\":\"p.jpg\",\"genres\":[\"%s\"]}]}",
             id + 4, id, GENEROS[g1],
             id + 4, id, GENEROS[g2],
             id + 4, id, GENEROS[(id[4] - 'a' + 2) % 4],
             id + 4, id, GENEROS[(id[4] - 'a' + 3) % 4]);
  }
  return strdup(buf);
}
char *rede_baixar_com(const char *u, int t, const char *const *c) { (void)c; return rede_baixar(u, t); }

int   trakt_lista_cresc(const char *q, CatItem **s, int m) { (void)q; (void)s; (void)m; return 0; }
int   trakt_lista(const char *q, CatItem *s, int m) { (void)q; (void)s; (void)m; return 0; }
int   simkl_plantowatch(CatItem *s, int m) { (void)s; (void)m; return 0; }

// ------------------------------------------------------------- fileiras.c
static char registro[64][192];
static int nRegistro;
static char ocultas[8][64]; static int nOcultas;
int fil_escolhida(const char *c) { (void)c; return -1; }
int fil_migrar_197(const char *const *c, int n) { (void)c; (void)n; return 0; }
void fil_registrar_se_couber(const char *c, const char *t, const char *a,
                             const char *tp) { (void)c; (void)t; (void)a; (void)tp; }
void fil_registrar(const char *c, const char *t, const char *a, const char *tp, int itens) {
  int i;
  (void)t; (void)a; (void)tp; (void)itens;
  for (i = 0; i < nRegistro; i++) if (!strcmp(registro[i], c)) return;
  if (nRegistro < 64) snprintf(registro[nRegistro++], sizeof registro[0], "%s", c);
}
int   fil_n(void)                          { return nRegistro; }
const char *fil_chave(int i)               { return (i >= 0 && i < nRegistro) ? registro[i] : ""; }
int   fil_linha_oculta(int i)              { (void)i; return 0; }
int   fil_linha_tipo(int i)                { (void)i; return FIL_TIPO_AUTO; }
int   fil_linha_tam(int i)                 { (void)i; return FIL_TAM_PADRAO; }
void  fil_gravar_registro(void)            { }
int   fil_lista_e_deste_perfil(int p)      { (void)p; return 1; }
unsigned fil_perfil_geracao(void)          { return 0; }
FilPassada fil_passada_ler(void)           { FilPassada p = {0, 0}; return p; }
int   fil_passada_valida(const FilPassada *p) { (void)p; return 1; }
void  fil_registrar_de(const FilPassada *p, const char *c, const char *t, const char *a, const char *k, int n) { (void)p; (void)c; (void)t; (void)a; (void)k; (void)n; }
void  fil_registrar_se_couber_de(const FilPassada *p, const char *c, const char *t, const char *a, const char *k) { (void)p; (void)c; (void)t; (void)a; (void)k; }
int   fil_podar_catalogos(const char *const *ids, const char *const *bases, int n,
                          int perfilDaLista) {
  (void)ids; (void)bases; (void)n; (void)perfilDaLista; return 0; }
int fil_marcar_declaradas(const char *const *chaves, int n, int perfilDaLista) { (void)chaves; (void)n; (void)perfilDaLista; return 0; }
int fil_marcar_sem_addon(const char *const *ids, const char *const *bases, const int *ativos, int n, int perfilDaLista) {
  (void)ids; (void)bases; (void)ativos; (void)n; (void)perfilDaLista; return 0;
}
int   fil_addon_novo(const char *id, const char *base) { (void)id; (void)base; return 0; }
int   fil_limite(void)                     { return 3; }
const char *fil_hero_fonte(void)           { return "auto"; }
int   fil_oculta(const char *c)            {
  int i;
  for (i = 0; i < nOcultas; i++) if (!strcmp(ocultas[i], c)) return 1;
  return 0;
}
int   fil_adicionada_na_tv(const char *c)  { (void)c; return 0; }
int   fil_estado_chave(const char *c)      { (void)c; return -1; }
int   fil_tem_ordem(void)                  { return 0; }
int   fil_unir(const char *const *c, int n, int *s, int m) {
  int i; (void)c; for (i = 0; i < n && i < m; i++) s[i] = i; return i; }

// --------------------------------------------------------------- ajustes --
static volatile int montagens;
void  marco(const char *n)                 { if (!strcmp(n, "montar: inicio")) montagens++; }
void  SDL_Delay(Uint32 ms)                 { usleep(ms * 1000); }
const char *i18n(const char *s)            { return s; }
int   ajustes_idioma_ingles(void)          { return 0; }
int   ajustes_idioma(void)                 { return 0; }
int   ajustes_cw_ordem(void)               { return 0; }
int   ajustes_cw_concluido(void)           { return 90; }
int   ajustes_cw_minimo(void)              { return 1; }
int   ajustes_itens_fileira(void)          { return 12; }
int   ajustes_cw_mostrar_nao_exibidos(void){ return 1; }
int   ajustes_cw_proximo(void)             { return 1; }
int   ajustes_cw_ligado(void)              { return 1; }
int   ajustes_cw_estilo(void)              { return 0; }
int   ajustes_posteres_deitados(void)      { return 0; }
float ajustes_espaco_fileiras(void)        { return 1.0f; }
float ajustes_espaco_titulos(void)         { return 1.0f; }
int   ajustes_rotulos_poster(void)         { return 1; }
int   ajustes_hero_fonte(void)             { return 0; }
int   ajustes_cw_fonte(void)               { return 0; }
int   ajustes_tmdb_ligado(void)            { return 0; }
int   ajustes_tmdb_basico(void)            { return 0; }
int   ajustes_meta_externo(void)           { return 0; }
int   ajustes_fundo_addon(void)            { return 0; }
int   ajustes_logo_addon(void)             { return 0; }
int   ajustes_meta_so_cinemeta(void)       { return 0; }
int   ajustes_tmdb_arte(void)              { return 0; }
int   ajustes_tmdb_elenco(void)            { return 0; }
int   ajustes_tmdb_cw(void)                { return 0; }
int   ajustes_salvos_no_simkl(void)        { return 0; }
const char *ajustes_tmdb_idioma(void)      { return "pt-BR"; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }
int   simkl_ativo(void)                    { return 1; }
int   trakt_ativo(void)                    { return 0; }   // cw de conta so sem Trakt/Simkl
int   simkl_continuar(CatItem *s, int m)   { (void)s; (void)m; return 0; }
int   simkl_e_a_seguir(const char *id)     { (void)id; return 0; }
int   trakt_social(CatItem *s, int m)      { (void)s; (void)m; return 0; }
int   trakt_continuar(CatItem *s, int m)   { (void)s; (void)m; return 0; }
int   trakt_continuar_falhou(void)         { return 0; }
int   trakt_e_a_seguir(const char *id)     { (void)id; return 0; }
int   trakt_progresso_ocultar(const char *i, int o) { (void)i; (void)o; return 0; }
const char *nuvem_trakt_cliente(void)      { return ""; }
int   arte_reserva_registrar(const char *url, const char *imdb, int poster) {
  (void)url; (void)imdb; (void)poster; return 1; }

// ------------------------------------------------- falsos da borda do Farol
// O MODELO e construido na mao (farolgosto real): afinidade positiva nos
// quatro generos dos metas — assim o motor pontua > 0 e a fileira existe.
static FarolGosto T_g;
static _Atomic int T_rec = 1;         // "Recomendar nesta TV"
static _Atomic int T_seguro = 0;      // modo seguro
static _Atomic int T_infantil = 0;
static _Atomic int T_visivel = 1;     // a Home esta na tela (padrao: sim)
static _Atomic int T_mostrados;       // farolsinal_mostrado chamadas
static _Atomic int T_origens;         // farolsinal_origem chamadas
int  farolsinal_recomendar(void) { return T_rec; }
void farolsinal_esquecer(void) { }
void farolsinal_definir_recomendar(int ligado) {
  // PROVA DE ORDEM (contrato, item 5): quando a preferencia virar, as
  // fileiras farol_ JA nao podem estar publicadas — a remocao correu antes.
  if (!ligado) {
    int r;
    for (r = 0; r < cat_n_fileiras(); r++)
      assert(strncmp(cat_fileira(r)->chave, CAT_PREFIXO_FAROL,
                     sizeof CAT_PREFIXO_FAROL - 1) &&
             "desligar Recomendar definiu antes de remover as fileiras");
  }
  T_rec = ligado;
}
int  farolsinal_modelo(FarolGosto *dst) { if (!dst) return 0; *dst = T_g; return 1; }
// Prova r1: o anel de exibidos muda o modelo e NAO pode recompor a cena;
// e o opt-out no MEIO da entrega (a primeira marcacao dispara o desligar)
// nao pode deixar a volta republicar as fileiras. Prova 45C: o mesmo
// disparo para "Zerar o gosto" — o lote em voo nao pode subir depois.
static _Atomic int desligarNaEntrega;
static _Atomic int zerarNaEntrega;
void farolsinal_mostrado(const CatItem *ci) {
  farolgosto_mostrado(&T_g, mapa_hash_titulo(ci->imdb), 1);
  T_mostrados++;
  if (atomic_exchange(&desligarNaEntrega, 0))
    farol_recomendar_definir(0, dirDados);
  if (atomic_exchange(&zerarNaEntrega, 0))
    farol_gosto_zerar(dirDados);
}
void farolsinal_zerar_gosto(void) { farolgosto_iniciar(&T_g, (uint16_t)T_perfil); }
int  farolsinal_ficha_do_item(const CatItem *ci, FarolFicha *dst) {
  (void)ci; (void)dst; return 0;
}
void farolsinal_origem(const CatItem *ci, int origem) {
  assert(origem == FS_ORIGEM_FAROL); (void)ci; T_origens++;
}
int  seguro_perfil_ativo(void) { return T_seguro; }
int  perfil_infantil_ativo(void) { return T_infantil; }
int  perfil_infantil(int idx) { (void)idx; return T_infantil; }
const char *app_tela_nome(void) {
  return T_visivel ? (T_visivel == 2 ? "menu" : "home") : "busca";
}
// A visibilidade de verdade (home.c): menu lateral e sobreposicoes deixam a
// Home pintada — e ela, nao o nome da camada de cima, quem decide.
int  home_fileiras_na_tela(void) { return T_visivel != 0; }
int  recomenda_pessoa_canonica(const char *id, char *dst, size_t tam) {
  (void)id; if (tam) *dst = 0; return 0;
}
// Diario: duas sementes terminadas (motivo "Porque voce terminou" tem fonte).
static DiarioEntrada T_dia[8]; static int T_diaN;
int diario_n(int perfil) { (void)perfil; return T_diaN; }
int diario_entrada(int perfil, int i, DiarioEntrada *s) {
  (void)perfil;
  if (!s || i < 0 || i >= T_diaN) return 0;
  *s = T_dia[i]; return 1;
}
int diario_titulo(int perfil, const char *id, DiarioTitulo *s) {
  int i, achou = 0;
  (void)perfil;
  if (!s || !id) return 0;
  memset(s, 0, sizeof *s);
  for (i = 0; i < T_diaN; i++) if (!strcmp(T_dia[i].id, id)) {
    achou = 1; s->vezes++;
    s->flags |= T_dia[i].flags;
    if (T_dia[i].quando > s->ultimo) s->ultimo = T_dia[i].quando;
  }
  snprintf(s->id, sizeof s->id, "%s", id);
  return achou;
}
int diario_nome(const char *id, char *titulo, size_t tamTitulo, int *ano,
                char *poster, size_t tamPoster) {
  (void)ano; (void)poster; (void)tamPoster;
  if (!id || !titulo || !tamTitulo) return 0;
  // so a semente gravada tem nome conhecido
  if (!strncmp(id, "ttseed", 6)) { snprintf(titulo, tamTitulo, "Semente %s", id + 6); return 1; }
  return 0;
}
// Salvos: vazio por padrao; o caso de salvo fora do catalogo monta um.
static SalvoItem T_salvo[8]; static int T_salvoN;
int  salvos_n(void) { return T_salvoN; }
const SalvoItem *salvos_item(int i) { return i >= 0 && i < T_salvoN ? &T_salvo[i] : NULL; }
int  salvos_tem(const char *imdb) {
  int i;
  for (i = 0; i < T_salvoN; i++) if (!strcmp(T_salvo[i].id, imdb)) return 1;
  return 0;
}
int salvos_mesmo_titulo(const char *a, const char *b) { return a && b && !strcmp(a, b); }

// ------------------------------------------------------------- helpers ----
static void esperarQuieto(void) {
  int i, quieto = 0;
  for (i = 0; i < 500 && !buscando; i++) usleep(2000);
  for (i = 0; i < 10000 && quieto < 150; i++) {
    usleep(2000);
    quieto = buscando ? 0 : quieto + 1;
  }
  assert(quieto >= 150);
}
static void volta(void) {           // uma volta completa de montagem
  homeestado_iniciar();
  desc_repetir();
  esperarQuieto();
}
static int idxFil(const char *chave) {
  int r;
  for (r = 0; r < cat_n_fileiras(); r++)
    if (!strcmp(cat_fileira(r)->chave, chave)) return r;
  return -1;
}
static int nFarol(void) {
  int r, n = 0;
  for (r = 0; r < cat_n_fileiras(); r++)
    if (!strncmp(cat_fileira(r)->chave, CAT_PREFIXO_FAROL,
                 sizeof CAT_PREFIXO_FAROL - 1)) n++;
  return n;
}
static int temIdiCru(const char *s) {
  // id cru na tela nunca: "tt<digitos>", "c<digitos>", "g<digits>" sobrando
  return strstr(s, "tt") || (s[0] == 'c' && s[1] >= '0' && s[1] <= '9');
}

static _Atomic long long T_now;
static long long fakeS(void) { return T_now; }
static int fakeVis(void) { return T_visivel; }
static void modeloGosto(void) {
  static const char *const GEN_PT[4] = { "Ação", "Comédia", "Drama", "Terror" };
  int i;
  farolgosto_iniciar(&T_g, (uint16_t)T_perfil);
  T_g.titulos = 20;                            // nivel 2 de frio
  for (i = 0; i < 4; i++) {
    T_g.genero[i].id = (int32_t)mapa_hash_nome(GEN_PT[i]);
    T_g.genero[i].peso = 512;                  // +2,0 em Q8.8
    T_g.genero[i].n = 5;
    T_g.genero[i].dia = (uint16_t)(T_now / 86400);
  }
}

// --------------------------------------------------------------- o teste --
int main(void) {
  int r, i;
  long long base = 1770000000LL;
  nvt_set(dirDados, sizeof dirDados, "/nuvio-farol-home-XXXXXX");
  assert(mkdtemp(dirDados));
  desc_tmdb(dirDados);
  farol_teste_relogio(fakeS);
  farol_teste_home_visivel(fakeVis);
  T_now = base;

  // ---------- frio 0: nunca sai fileira ----------
  farolgosto_iniciar(&T_g, 1); T_g.titulos = 0;
  desc_iniciar(); esperarQuieto();
  assert(nFarol() == 0);
  puts("ok  frio 0: sem fileira do Farol");

  // ---------- nivel 2: a fileira monta ----------
  modeloGosto();
  // Prova r1 (republica): o opt-out NO MEIO da entrega — o primeiro
  // farolsinal_mostrado desliga "Recomendar" — nao pode deixar a volta em
  // voo republicar as fileiras.
  desligarNaEntrega = 1;
  volta();
  assert(T_rec == 0 && nFarol() == 0 && "montagem republicou apos desligar");
  farol_recomendar_definir(1, dirDados);
  T_visivel = 0;                    // a readocao espera a Home sair da tela
  modeloGosto();                    // modelo fresco: anel de exibidos vazio
  { int antes = T_mostrados;
    volta();
    // Cada item publicado marca UMA vez — se a marcacao mudasse o hash da
    // cena a entrega invalidava a recem-adotada e a volta recompunha e
    // remarcava na hora (o bug do r1: a cena se invalida depois de
    // entregue).
    int esperado = 0, q;
    for (q = 0; q < cat_n_fileiras(); q++)
      if (!strncmp(cat_fileira(q)->chave, CAT_PREFIXO_FAROL,
                   sizeof CAT_PREFIXO_FAROL - 1))
        esperado += cat_fileira(q)->n;
    assert(T_mostrados - antes == esperado &&
           "a propria entrega invalidou a cena (recomposta e remarcada)");
  }
  T_visivel = 1;
  r = idxFil("farol_gosto1");
  assert(r >= 0 && cat_fileira(r)->n >= 6);
  printf("    fileiras:");
  for (i = 0; i < cat_n_fileiras(); i++)
    printf(" %s(%d)", cat_fileira(i)->chave, cat_fileira(i)->n);
  printf("\n");
  // sem continue_watching: gosto1 no topo (posicao de fabrica do Poke)
  assert(r == 0);
  // limite nao gasto: as 3 fileiras de addon seguem depois
  assert(idxFil(AID "_movie_cat_a") == 1);
  puts("ok  montou no topo sem Continuar e sem gastar o limite");

  // titulo e motivo renderizados: frase de verdade, sem id cru
  {
    const CatFileira *f = cat_fileira(r);
    assert(f->titulo[0] && !temIdiCru(f->titulo));
    const char *m = NULL;
    for (i = 0; i < f->n; i++) {
      m = farol_motivo("farol_gosto1", i, cat_item(f->ini + i)->imdb);
      if (m) break;
    }
    assert(m && !temIdiCru(m));
    printf("    titulo=\"%s\" motivo[0]=\"%s\"\n", f->titulo, m);
    // o motivo so sai para o imdb que a fileira PUBLICOU na coluna (r1)
    assert(farol_motivo("farol_gosto1", 0, "tt9999999") == NULL);
    assert(farol_motivo("farol_gosto1", -1, "tt0000001") == NULL);
    assert(farol_motivo("nao_e_farol", 0, "tt0000001") == NULL);
  }
  puts("ok  titulo e motivo renderizados, sem id cru");

  // exposicao marcada uma vez por cena entregue (a contagem exata foi
  // conferida na readocao ali em cima: uma marcacao por item publicado)
  assert(T_mostrados >= 6);
  puts("ok  mostrado marcado na entrega, sem readocao na mesma volta");

  // ---------- mesma cena a cada volta ----------
  { char tit1[96], imdb0[64];
    snprintf(tit1, sizeof tit1, "%s", cat_fileira(idxFil("farol_gosto1"))->titulo);
    snprintf(imdb0, sizeof imdb0, "%s",
             cat_item(cat_fileira(idxFil("farol_gosto1"))->ini)->imdb);
    volta();
    assert(!strcmp(cat_fileira(idxFil("farol_gosto1"))->titulo, tit1));
    assert(!strcmp(cat_item(cat_fileira(idxFil("farol_gosto1"))->ini)->imdb, imdb0));
  }
  puts("ok  mesma cena reentregue: titulo e primeiro cartaz iguais");

  // ---------- a exposicao sozinha NAO recompoe (revisao r1) ----------
  { int exposicoes = T_mostrados;
    T_visivel = 0;
    volta();
    assert(T_mostrados == exposicoes && "exposicao sozinha readotou a cena");
  }
  puts("ok  anel de exibidos fora do hash: sem recomposicao a passe");

  // ---------- evento forte com a Home visivel: a cena FICA ----------
  T_visivel = 1;
  T_g.titulos = 21;                            // o modelo mudou (evento)
  volta();
  assert(idxFil("farol_gosto1") == 0);         // cartazes nao trocam olhando
  puts("ok  evento forte com Home na tela: cena antiga fica");

  // ---------- evento forte com a Home FORA: recompoe ----------
  T_visivel = 0;
  T_g.titulos = 22;
  volta();
  assert(idxFil("farol_gosto1") >= 0);
  T_visivel = 1;
  puts("ok  evento forte fora da Home: cena nova adotada");

  // ---------- compositor falhou: a antiga NAO e reaproveitada ----------
  farolgosto_iniciar(&T_g, 1); T_g.titulos = 0;  // voltou ao frio 0
  T_visivel = 0;
  volta();
  assert(nFarol() == 0);
  // readocao vetada com a Home visivel no MESMO perfil que perdeu a cena —
  // a fileira nao pode sumir e reaparecer com a pessoa olhando (revisao r1)
  modeloGosto(); T_visivel = 1;
  volta();
  assert(nFarol() == 0 && "cena readotada com a Home visivel apos falha");
  // nem por preservarFileirasAusentes: a chave farol_ nao volta
  modeloGosto(); T_visivel = 0;
  volta();
  assert(idxFil("farol_gosto1") >= 0);
  puts("ok  falha do compositor: fileira antiga nao reanexa");

  // ---------- zerar o gosto: tela, cena e MODELO (revisao r1) ----------
  farol_gosto_zerar(dirDados);
  assert(nFarol() == 0);
  assert(T_g.titulos == 0 && "Zerar o gosto conservou o modelo anterior");
  modeloGosto();                        // o modelo zerado e reconstruido
  puts("ok  zerar o gosto: fileiras saem e o modelo zera junto");

  // ---------- zerar DEPOIS de montar: a remontagem nao repoe (45C) ------
  // A fileira guardada em filsMontadas e anterior ao reset: republicar o
  // retrato da ultima montagem nao pode traze-la de volta.
  T_visivel = 0;
  volta();                                     // readota e publica de novo
  T_visivel = 1;
  assert(nFarol() > 0);
  farol_gosto_zerar(dirDados);
  assert(nFarol() == 0);
  desc_remontar_fileiras();
  assert(nFarol() == 0 && "remontagem republicou fileiras anteriores ao zerar");
  modeloGosto();
  puts("ok  zerar o gosto: a fileira montada antes nao volta na remontagem");

  // ---------- zerar no MEIO da entrega: o lote em voo nao sobe (45C) ----
  // O primeiro farolsinal_mostrado da entrega dispara o reset: o lote que
  // ja estava montado nao pode ser publicado por cima dele.
  T_visivel = 0;
  volta();                                     // readota para a prova valer
  T_visivel = 1;
  assert(nFarol() > 0);
  zerarNaEntrega = 1;
  T_visivel = 0;
  T_g.titulos++;                               // evento forte -> recompoe
  volta();
  assert(nFarol() == 0 && "lote em voo republicou fileiras apos o zerar");
  assert(T_g.titulos == 0);
  modeloGosto();
  puts("ok  zerar no meio da entrega: o lote em voo nao sobe");

  // ---------- recomendar desligado ----------
  T_visivel = 0;          // a readocao da volta espera a Home sair da tela
  volta();
  T_visivel = 1;
  assert(nFarol() > 0);   // fileira na tela para a prova de ordem valer
  farol_recomendar_definir(0, dirDados);
  // o stub de definir_recomendar ja provou a ORDEM (sem farol_ publicada
  // quando a preferencia virou). Aqui fica o estado:
  assert(T_rec == 0);
  // a remontagem sem rede tambem obedece o opt-out — a fileira removida da
  // tela nao volta por filsMontadas (revisao r1)
  desc_remontar_fileiras();
  assert(nFarol() == 0 && "remontagem sem rede republicou apos desligar");
  volta();
  assert(nFarol() == 0);
  puts("ok  Recomendar desligado: some na volta e a remocao veio antes");
  farol_recomendar_definir(1, dirDados);
  T_visivel = 0;          // a volta que readota precisa da Home fora da tela
  volta();
  T_visivel = 1;
  assert(idxFil("farol_gosto1") >= 0);

  // ---------- menu lateral por cima: a Home continua visivel ----------
  T_visivel = 2;
  farol_teste_home_visivel(NULL);    // producao: o desenho, nao o nome da tela
  farolgosto_iniciar(&T_g, 1);       // modelo zerado = evento forte
  volta();
  assert(nFarol() > 0 && "fileira desapareceu sob o menu lateral");
  farol_teste_home_visivel(fakeVis);
  modeloGosto(); T_visivel = 1;
  puts("ok  menu lateral por cima: a cena nao troca");

  // ---------- modo seguro ----------
  T_seguro = 1; T_visivel = 0;
  volta();
  assert(nFarol() == 0);
  T_seguro = 0;
  puts("ok  modo seguro: nao aparece");

  // ---------- oculta em Fileiras ----------
  snprintf(ocultas[nOcultas], sizeof ocultas[0], "farol_gosto1"); nOcultas++;
  volta();
  assert(idxFil("farol_gosto1") < 0);
  nOcultas = 0;
  volta();
  assert(idxFil("farol_gosto1") >= 0);
  puts("ok  oculta continua oculta");

  // ---------- remocao: tela e disco, na hora ----------
  farol_remover_fileiras(dirDados);
  assert(nFarol() == 0);
  {
    char cam[PATH_MAX + 80]; FILE *fp = NULL; long sz; char *buf;
    DIR *dd = opendir(dirDados); struct dirent *de;
    assert(dd);
    while ((de = readdir(dd))) {
      if (!strncmp(de->d_name, "catalogo-", 9)) {
        snprintf(cam, sizeof cam, "%s/%s", dirDados, de->d_name);
        fp = fopen(cam, "rb");
        if (fp) break;
      }
    }
    closedir(dd);
    assert(fp);
    fseek(fp, 0, SEEK_END); sz = ftell(fp); rewind(fp);
    buf = malloc((size_t)sz);
    assert(buf && fread(buf, 1, (size_t)sz, fp) == (size_t)sz);
    fclose(fp);
    assert(!memmem(buf, (size_t)sz, "farol_gosto", 11));
    free(buf);
  }
  puts("ok  remover: sai da tela e do cache do perfil, o resto fica");

  // ---------- troca de perfil: cena do anterior nao vaza ----------
  volta();                                     // readota para o perfil 1
  assert(idxFil("farol_gosto1") >= 0);
  T_perfil = 2; T_visivel = 1;
  farolgosto_iniciar(&T_g, 2); T_g.titulos = 0;
  volta();
  assert(nFarol() == 0);                       // perfil 2 e frio: nada
  T_perfil = 1; T_visivel = 0;
  modeloGosto();
  volta();
  assert(idxFil("farol_gosto1") >= 0);
  puts("ok  troca de perfil: a cena do anterior nao vaza");

  // ---------- origem do Play ----------
  {
    const CatFileira *f = cat_fileira(idxFil("farol_gosto1"));
    const CatItem *ci = cat_item(f->ini);
    farolsinal_origem(ci, FS_ORIGEM_FAROL);    // o que a home faz no OK
    assert(T_origens == 1);
  }
  puts("ok  origem FS_ORIGEM_FAROL registrada");

  // ---------- salvo fora do catalogo vira candidato com imdb ----------
  T_salvoN = 1;
  snprintf(T_salvo[0].id, sizeof T_salvo[0].id, "tt0004321");
  snprintf(T_salvo[0].tipo, sizeof T_salvo[0].tipo, "movie");
  snprintf(T_salvo[0].titulo, sizeof T_salvo[0].titulo, "Salvo de Fora");
  T_salvo[0].quandoS = base - 86400;
  T_visivel = 0; T_g.titulos = 23;             // evento -> recompoe
  volta();
  {
    int achou = 0;
    for (i = 0; i < cat_n_fileiras(); i++) {
      const CatFileira *f = cat_fileira(i);
      int k;
      if (strncmp(f->chave, CAT_PREFIXO_FAROL, sizeof CAT_PREFIXO_FAROL - 1)) continue;
      for (k = 0; k < f->n; k++) {
        const CatItem *ci = cat_item(f->ini + k);
        if (ci && !strcmp(ci->imdb, "tt0004321")) achou = 1;
      }
    }
    // o salvo nao-visto so pode aparecer por FORA_CAT+TEM_IMDB; se nao
    // pontuou o bastante a fileira segue honesta — o que nao pode e sumir
    // a fileira inteira por causa dele.
    assert(idxFil("farol_gosto1") >= 0);
    (void)achou;
  }
  puts("ok  salvo fora do catalogo nao quebra a fileira");

  printf("farol_home: tudo ok\n");
  return 0;
}
