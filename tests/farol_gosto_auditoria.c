// Auditoria completa de privacidade (canarios), contadores locais, RAM e
// concorrencia para farol-56. Compilado com os modulos reais sem SDL.
#include "farolacha.h"
#include "farolanel.h"
#include "farolbusca.h"
#include "farolbuscaparse.h"
#include "farolcompor.h"
#include "farolconsent.h"
#include "farolerro.h"
#include "farolescolha.h"
#include "farolescolha_rede.h"
#include "farolficha.h"
#include "farolgosto.h"
#include "farolpacote.h"
#include "farolpeca.h"
#include "farolperfil.h"
#define FarolMotivo FarolRecMotivo
#include "farolrec.h"
#undef FarolMotivo
#include "farolredator.h"
#include "farolsinal.h"
#include "faroltexto.h"
#include "cacavisto.h"
#include "diario.h"
#include "dormiu.h"
#include "fontesonda.h"
#include "idbase.h"
#include "mapa.h"
#include "progresso.h"
#include "rede.h"
#include "salvos.h"
#include "visto.h"

#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef AUDITORIA_RAM
#include <malloc.h>
#endif

static const char *CAN_TITULO = "CANARIO_TITULO_902";
static const char *CAN_PERFIL = "CANARIO_PERFIL_901";
static const char *CAN_FRASE  = "quero filme curto CANARIO_FRASE_903";
static const char *CAN_TOKEN  = "CANARIO_TOKEN_904";
static const char *CAN_URL    = "https://canario905.example/segredo.mkv";

static char          G_dir[512];
static _Atomic int   G_perfil = 1;
static _Atomic int   G_infantil = 0;
static _Atomic int   G_seguro = 0;
static _Atomic int   G_offline = 0;
static _Atomic int   G_player = 0;
static _Atomic int   G_travado = 0;
static _Atomic long long G_now = 1795000000LL;
static _Atomic uint32_t  G_ms = 10000u;

static long long relS(void) { return G_now; }
static uint32_t  relMs(void) { return G_ms; }

// ---- Ganchos do app (simulados com valores canarios) ----
const char *dados_dir(void) { return G_dir; }
int perfis_ativo(void) { return G_perfil; }
const char *sessao_usuario(void) { return CAN_PERFIL; }
uint64_t perfis_sessao_geracao(void) { return 7u; }
int perfil_infantil_ativo(void) { return G_infantil; }
int seguro_perfil_ativo(void) { return G_seguro; }
int rede_saude_offline(void) { return G_offline; }
int player_aberto(void) { return G_player; }
int perfil_ajustes_travados(int idx) { (void)idx; return G_travado; }
const char *ajustes_tmdb_idioma(void) { return "pt-BR"; }
int trakt_ativo(void) { return 0; }
int recomenda_cabecalhos(const char **cab, char *aut, size_t na, char *via, size_t nv,
                         char *perfil, size_t np) {
  snprintf(aut, na, "Authorization: Bearer %s", CAN_TOKEN);
  snprintf(via, nv, "X-Nuvio-Auth: nuvio");
  snprintf(perfil, np, "X-Nuvio-Perfil: %d", (int)G_perfil);
  if (cab) { cab[0] = aut; cab[1] = via; cab[2] = perfil; cab[3] = NULL; }
  return 1;
}

int dormiu_suspeita(const char *id, DiarioEntrada *saida) {
  (void)id; (void)saida; return 0;
}
static DiarioOuvinte D_fn; static void *D_ctx;
int diario_ouvir(DiarioOuvinte fn, void *ctx) { D_fn = fn; D_ctx = ctx; return 1; }
void diario_parar_de_ouvir(DiarioOuvinte fn, void *ctx) {
  if (D_fn == fn && D_ctx == ctx) D_fn = NULL;
}
int diario_percorrer(int perfil, const DiarioFiltro *f, DiarioVisita cb, void *ctx) {
  (void)perfil; (void)f; (void)cb; (void)ctx; return 0;
}
int diario_carregado(int perfil) { (void)perfil; return 1; }
void diario_separar_id(const char *src, char *id, size_t tam, int *t, int *e) {
  size_t n = src ? strlen(src) : 0;
  if (tam) {
    if (n >= tam) n = tam - 1;
    if (n) memcpy(id, src, n);
    id[n] = 0;
  }
  if (t) *t = 0;
  if (e) *e = 0;
}
int diario_por_titulo(int perfil, const char *id, DiarioEntrada *saida, int max) {
  (void)perfil; (void)id; (void)saida; (void)max; return 0;
}
int diario_apagar(int perfil, const char *id, int t, int e, long long quando) {
  (void)perfil; (void)id; (void)t; (void)e; (void)quando; return 0;
}
int diario_episodio(int perfil, const char *id, int t, int e, long long *quando) {
  (void)perfil; (void)id; (void)t; (void)e; (void)quando; return 0;
}
int diario_ultimo_episodio(int perfil, const char *id, int *t, int *e, long long *quando) {
  (void)perfil; (void)id; (void)t; (void)e; (void)quando; return 0;
}

static CatItem C_it[32];
static int C_n;
static unsigned C_rev = 1;
static pthread_mutex_t C_mtx = PTHREAD_MUTEX_INITIALIZER;
int cat_n(void) { int r; pthread_mutex_lock(&C_mtx); r = C_n; pthread_mutex_unlock(&C_mtx); return r; }
unsigned cat_revisao(void) { return C_rev; }
const CatItem *cat_item(int i) {
  const CatItem *r;
  pthread_mutex_lock(&C_mtx);
  r = (i >= 0 && i < C_n) ? &C_it[i] : NULL;
  pthread_mutex_unlock(&C_mtx);
  return r;
}
int cat_indice_por_imdb(const char *imdb) {
  int i, r = -1;
  if (!imdb) return -1;
  pthread_mutex_lock(&C_mtx);
  for (i = 0; i < C_n; i++) if (!strcmp(C_it[i].imdb, imdb)) { r = i; break; }
  pthread_mutex_unlock(&C_mtx);
  return r;
}
int cat_n_episodios(int i) { (void)i; return 0; }
const CatEp *cat_episodio(int i, int k) { (void)i; (void)k; return NULL; }
int cat_acrescentar(const CatItem *item) {
  int i;
  if (!item) return -1;
  pthread_mutex_lock(&C_mtx);
  if (C_n >= 32) { pthread_mutex_unlock(&C_mtx); return -1; }
  i = C_n++;
  C_it[i] = *item;
  C_rev++;
  pthread_mutex_unlock(&C_mtx);
  return i;
}
void salvos_ao_definir(void (*f)(const CatItem *, int)) { (void)f; }
void visto_ao_marcar(VistoOuvinte fn) { (void)fn; }
void prog_ao_retirar(void (*fn)(const char *)) { (void)fn; }
int prog_ler(ProgRegistro *saida, int max) { (void)saida; (void)max; return 0; }
int cacavisto_filme(const char *imdb, long tmdb, long long *quando) {
  (void)imdb; (void)tmdb;
  if (quando) *quando = 0;
  return 0;
}

static void esperarCompor(void) {
  int lim = 300;
  while (farolcompor_em_voo() && lim-- > 0) usleep(10000);
}
static void esperarBusca(void) {
  int lim = 300;
  while (farolbusca_em_voo() && lim-- > 0) usleep(10000);
}
static void esperarEscolha(void) {
  int lim = 300;
  while (farolescolha_rede_em_voo() && lim-- > 0) usleep(10000);
}
static void esperarSinal(void) {
  int lim = 300;
  while (farolsinal_teste_trabalho() && lim-- > 0) {
    farolsinal_passo(1);
    usleep(5000);
  }
}

static void preencherGostoCheio(FarolGosto *g, uint16_t dia) {
  int i;
  farolgosto_iniciar(g, 1);
  g->titulos = 40;
  g->diaBase = dia;
  for (i = 0; i < FG_GENERO_N; i++) {
    g->genero[i].id = 100 + i;
    g->genero[i].n = 5;
    g->genero[i].peso = 220;
    g->genero[i].dia = dia;
  }
  for (i = 0; i < FG_PESSOA_N; i++) {
    g->pessoa[i].id = 1000 + i;
    g->pessoa[i].n = 4;
    g->pessoa[i].peso = 200;
    g->pessoa[i].dia = dia;
  }
  for (i = 0; i < FG_TEMA_N; i++) {
    g->tema[i].id = 5000 + i;
    g->tema[i].n = 4;
    g->tema[i].peso = 190;
    g->tema[i].dia = dia;
  }
  for (i = 0; i < FG_RECUS_N; i++) g->recusado[i] = (uint32_t)(9000 + i);
  for (i = 0; i < FG_MOSTR_N; i++) {
    g->mostrado[i].h = (uint32_t)(12000 + i);
    g->mostrado[i].vezes = 2;
    g->mostrado[i].dia = (uint8_t)(dia & 0xff);
  }
}

static void rodarCanariosRede(const char *baseUrl) {
  FarolGosto g;
  FarolComporPedido ped;
  FarolComporAfin genA;
  FarolComporCand cands[6];
  CatItem cCan, cNorm;
  FarolUsoContadores u;
  char *buf;
  size_t len;
  int i;

  farolsinal_relogio(relS, relMs);
  farolsinal_iniciar();
  farolconsent_pasta(G_dir);
  farolcompor_pasta(G_dir);
  farolcompor_definir_url(baseUrl);
  farolcompor_relogio(relS);
  farolbusca_definir_url(baseUrl);
  farolescolha_rede_definir_url(baseUrl);
  farolsinal_passo(1);
  esperarSinal();

  // Satura contadores locais com sentinelas magicos para garantir que NUNCA
  // viajam pela rede nem em corpo de LLM nem em FarolRegistro.
  memset(&cCan, 0, sizeof cCan);
  snprintf(cCan.imdb, sizeof cCan.imdb, "tt9990001");
  snprintf(cCan.tipo, sizeof cCan.tipo, "movie");
  snprintf(cCan.titulo, sizeof cCan.titulo, "%s", CAN_TITULO);
  snprintf(cCan.poster, sizeof cCan.poster, "%s/p.jpg", CAN_URL);
  cat_acrescentar(&cCan);

  for (i = 0; i < 7; i++) {
    farolsinal_uso_cena(i & 1);
    farolsinal_uso_vista(1);
  }
  farolsinal_uso_aberta(&cCan, 1);
  farolsinal_uso_tocada(&cCan);
  farolsinal_saida("tt9990001", 'm', 0, 0, 0, 96.0, 100.0, 1);
  memset(&cNorm, 0, sizeof cNorm);
  snprintf(cNorm.imdb, sizeof cNorm.imdb, "tt9990002");
  snprintf(cNorm.tipo, sizeof cNorm.tipo, "movie");
  farolsinal_recusar(&cNorm);
  farolsinal_uso_obter(&u);
  assert(u.vistas == 7 && u.abertas == 1 && u.tocadas == 1 &&
         u.terminadas == 1 && u.recusas == 1);

  preencherGostoCheio(&g, farolsinal_dia_atual());
  memset(cands, 0, sizeof cands);
  for (i = 0; i < 6; i++) {
    snprintf(cands[i].imdb, sizeof cands[i].imdb, "tt100%d", i);
    cands[i].minutos = 95;
    cands[i].ano = 2023;
    cands[i].nota10 = 80;
    cands[i].nGeneros = 1;
    cands[i].generos[0] = 18;
    // Candidatos do catalogo nunca carregam URL/perfil/token/frase.
    snprintf(cands[i].titulo, sizeof cands[i].titulo, "Obra_%d", i);
  }
  memset(&genA, 0, sizeof genA);
  genA.id = 18; genA.peso = 512; snprintf(genA.nome, sizeof genA.nome, "Drama");

  // Sem consentimento -> 0 POSTs.
  farolconsent_definir(FAROL_CONSENT_SO_TV);
  memset(&ped, 0, sizeof ped);
  ped.modo = FC_MODO_HOME;
  ped.idioma = "pt-BR";
  ped.nivel = 2;
  ped.generos = &genA;
  ped.nGeneros = 1;
  ped.candidatos = cands;
  ped.nCandidatos = 6;
  farolcompor_pedir(&ped);
  esperarCompor();

  // Com consentimento -> POST real para /v1/farol/compor.
  farolconsent_definir(FAROL_CONSENT_SIM);
  farolcompor_definir_consentimento(1);
  ped.consentiu = 1;
  farolcompor_pedir(&ped);
  esperarCompor();

  // Frase com URL -> bloqueada (0 POSTs); depois frase limpa -> POST real.
  {
    const char *hist[1] = { CAN_TITULO };
    const char *vis[1] = { "tt9990001" };
    FarolBuscaCtx bctx;
    int pergs[2] = { FESC_TEMPO, FESC_HUMOR };
    memset(&bctx, 0, sizeof bctx);
    bctx.consentiu = 1;
    bctx.idioma = "pt-BR";
    bctx.historico = hist; bctx.nHistorico = 0;
    bctx.vistos = vis; bctx.nVistos = 1;

    farolbusca_pedir_com(CAN_URL, &bctx);
    esperarBusca();
    farolbusca_pedir_com(CAN_FRASE, &bctx);
    esperarBusca();

    farolescolha_rede_pedir_perguntas(CAN_URL, pergs, 2, &g);
    esperarEscolha();
    farolescolha_rede_pedir_perguntas(CAN_FRASE, pergs, 2, &g);
    esperarEscolha();
  }

  // FarolRegistro v1 com URL/perfil/titulo/token injetados -> zero canarios no JSON.
  buf = malloc(FAROL_JSON_MAX + 64);
  assert(buf != NULL);
  farolanel_zerar();
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 0, 403, 0, CAN_URL);
  {
    FarolPerfil perfil;
    FarolPacoteEntrada in;
    FarolPacoteRes res;
    farolperfil_padrao(&perfil);
    snprintf(perfil.aparelho.modelo, sizeof perfil.aparelho.modelo, "%s", CAN_PERFIL);
    farolpacote_entrada_padrao(&in);
    in.perfil = &perfil;
    in.codigo = CAN_TOKEN;
    in.agora_ms = G_ms;
    assert(farolpacote_montar(&in, buf, FAROL_JSON_MAX + 64, &res) == 1);
    len = strlen(buf);
    assert(len > 0);
    assert(strstr(buf, CAN_TITULO) == NULL);
    assert(strstr(buf, CAN_PERFIL) == NULL);
    assert(strstr(buf, CAN_FRASE) == NULL);
    assert(strstr(buf, CAN_TOKEN) == NULL);
    assert(strstr(buf, CAN_URL) == NULL);
    assert(strstr(buf, "cenasTv") == NULL);
    assert(strstr(buf, "farol-medida") == NULL);
  }
  free(buf);
}

#ifdef AUDITORIA_RAM
static void medirRamGosto(void) {
  struct mallinfo2 miAntes, miDepois;
  FarolGosto *g;
  FarolFicha fb[60];
  FarolRecCand cand[60];
  FarolRecItem out[12];
  FarolRecCtx ctx;
  FarolUsoContadores u;
  char txt[FAROL_GOSTO_TXT_MAX + 1];
  int i, n;
  size_t estatica, picoHeap, total;

  // Mede o bloco de estado vivo residente do modelo + recomendacao + contadores.
  miAntes = mallinfo2();
  g = calloc(1, sizeof *g);
  assert(g != NULL);
  preencherGostoCheio(g, 200);
  n = (int)farolgosto_escrever(g, txt, sizeof txt);
  assert(n > 0);

  for (i = 0; i < 60; i++) {
    memset(&fb[i], 0, sizeof fb[i]);
    fb[i].tmdb = 50000 + i;
    fb[i].tipo = FF_FILME;
    fb[i].ano = 2020;
    fb[i].minutos = 95;
    fb[i].nota = 85;
    fb[i].votos = 5000;
    fb[i].nGenero = 1;
    fb[i].genero[0] = (int32_t)(100 + (i % 20));
    memset(&cand[i], 0, sizeof cand[i]);
    cand[i].chave = fb[i].tmdb;
    cand[i].h = (uint32_t)(70000 + i);
    cand[i].ficha = &fb[i];
    cand[i].semente = FRC_SEM_SEMENTE;
    cand[i].flags = FRC_FL_TEM_IMDB;
    cand[i].fontes = FRC_F_CATALOGO;
  }
  memset(&ctx, 0, sizeof ctx);
  ctx.g = g;
  ctx.dia = 200;
  ctx.naHome = 1;
  farolsinal_uso_obter(&u);
  n = farolrec_escolher(&ctx, cand, 60, out, 12);
  assert(n == 12);
  miDepois = mallinfo2();

  picoHeap = (miDepois.uordblks > miAntes.uordblks)
               ? (size_t)(miDepois.uordblks - miAntes.uordblks)
               : 0;
  estatica = sizeof(FarolGosto) + sizeof(FarolUsoContadores) +
             sizeof(FarolRecCtx) + sizeof(FarolRecItem) * 12 +
             sizeof(FarolConsentGrupo) * FAROLCONSENT_VAI_N;
  total = estatica + picoHeap;
  printf("AUDITORIA_RAM_BYTES: estatica=%zu heap=%zu total=%zu teto=%zu (sizeof_gosto=%zu sizeof_uso=%zu)\n",
         estatica, picoHeap, total, (size_t)(96 * 1024),
         sizeof(FarolGosto), sizeof(FarolUsoContadores));
  free(g);
  assert(total <= 96u * 1024u);
}
#endif

static void *fioEstresseUso(void *arg) {
  int id = (int)(intptr_t)arg;
  int i;
  CatItem ci;
  FarolUsoContadores u;
  memset(&ci, 0, sizeof ci);
  snprintf(ci.imdb, sizeof ci.imdb, "tt888%d", id);
  snprintf(ci.tipo, sizeof ci.tipo, "movie");
  for (i = 0; i < 120; i++) {
    farolsinal_uso_cena(i & 1);
    farolsinal_uso_vista(i & 1);
    farolsinal_uso_aberta(&ci, i & 1);
    farolsinal_uso_tocada(&ci);
    farolsinal_saida(ci.imdb, 'm', 0, 0, 0, 95.0, 100.0, 1);
    farolsinal_uso_obter(&u);
  }
  return NULL;
}

static void rodarConcorrenciaFios(void) {
  pthread_t t[4];
  int i;
  G_infantil = 0;
  G_perfil = 1;
  for (i = 0; i < 4; i++)
    assert(pthread_create(&t[i], NULL, fioEstresseUso, (void *)(intptr_t)i) == 0);
  for (i = 0; i < 20; i++) {
    G_perfil = 1 + (i & 1);
    farolsinal_passo(1);
    if (i == 10) farolsinal_zerar_gosto();
    usleep(1000);
  }
  for (i = 0; i < 4; i++) pthread_join(t[i], NULL);
  G_perfil = 1;
  farolsinal_passo(1);
  esperarSinal();
}

int main(int argc, char **argv) {
  const char *baseUrl = argc > 1 ? argv[1] : "http://127.0.0.1:1";
  const char *dir = argc > 2 ? argv[2] : "/tmp";
  snprintf(G_dir, sizeof G_dir, "%s", dir);
  farolsinal_pasta(G_dir);

#ifdef AUDITORIA_RAM
  if (argc > 1 && !strcmp(argv[1], "--ram")) {
    medirRamGosto();
    return 0;
  }
#endif

  rodarCanariosRede(baseUrl);
  rodarConcorrenciaFios();
  farolcompor_encerrar();
  farolbusca_encerrar();
  farolescolha_rede_encerrar();
  farolsinal_esquecer();
  farolsinal_sair();
  puts("auditoria-c: tudo ok");
  return 0;
}
