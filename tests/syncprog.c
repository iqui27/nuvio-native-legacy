// Contrato do progresso com a conta: o corpo do push tem o formato do web, o
// pull aceita os formatos que o servidor devolve, e o cenario de rollback
// (pull antigo aplicado por cima do local recem-assistido) NAO regride.
// Sem rede: sessao_rpc e um duble que grava o pedido e devolve o combinado.
#include "syncprog.h"
#include "progresso.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nvtmp.h"

// ---- dublês ----------------------------------------------------------------
static char *arquivo;
static int   perfil = 1;
static long long agora = 1757000000000LL;

char *dados_ler(const char *n) { (void)n; return arquivo ? strdup(arquivo) : NULL; }
int   dados_gravar(const char *n, const char *c) { (void)n; free(arquivo); arquivo = strdup(c); return 1; }
int   dados_apagar(const char *n) { (void)n; free(arquivo); arquivo = NULL; return 1; }
const char *dados_cliente_id(void) { return "cliente-teste-0001"; }
int   perfis_ativo(void) { return perfil; }
uint64_t perfis_sessao_geracao(void) { return 0; }
int perfis_sessao_atual(uint64_t g) { return !g; }
int perfis_sessao_travar(uint64_t g) { return !g; }
void perfis_sessao_destravar(void) {}
static long long relogio(void) { return agora; }

static char ultimaFuncao[64], ultimoCorpo[4096];
static int  chamadas, proximoStatus = 200;
static const char *proximaResposta = "[]";
static void (*duranteRpc)(void);
char *sessao_rpc(const char *funcao, const char *corpo, int *status) {
  snprintf(ultimaFuncao, sizeof ultimaFuncao, "%s", funcao);
  snprintf(ultimoCorpo, sizeof ultimoCorpo, "%s", corpo);
  chamadas++;
  if (duranteRpc) duranteRpc();
  if (status) *status = proximoStatus;
  return strdup(proximaResposta);
}
char *nuvem_rpc_com(const char *fn, const char *corpo, const char *jwt, int *st) {
  (void)jwt;
  return sessao_rpc(fn, corpo, st);
}
int sessao_identidade_local_copiar(char *u, size_t nu, char *j, size_t nj) {
  snprintf(u, nu, "usuario-teste");
  snprintf(j, nj, "token-teste");
  return 1;
}
int sessao_token_copiar(char *j, size_t nj) { snprintf(j, nj, "token-teste"); return 1; }
void sessao_token_recusado(const char *t) { (void)t; }
const char *sessao_usuario(void) { return "usuario-teste"; }

// Catalogo: so tt1234567 e tt7654321 existem.
static int aplicadosNoCatalogo;
static double ultimaPosAplicada;
int cat_indice_por_imdb(const char *imdb) {
  if (!strcmp(imdb, "tt1234567")) return 0;
  if (!strcmp(imdb, "tt7654321")) return 1;
  return -1;
}
void cat_aplicar_progresso(int i, double pos, double dur, int t, int e) {
  (void)i; (void)dur; (void)t; (void)e;
  aplicadosNoCatalogo++;
  ultimaPosAplicada = pos;
}

static void zerar(void) {
  free(arquivo); arquivo = NULL;
  prog_invalidar();
  syncprog_esquecer();
  chamadas = 0; proximoStatus = 200; proximaResposta = "[]";
  duranteRpc = NULL;
  aplicadosNoCatalogo = 0; ultimaPosAplicada = -1;
  ultimoCorpo[0] = 0; ultimaFuncao[0] = 0;
}

static int tem(const char *trecho) { return strstr(ultimoCorpo, trecho) != NULL; }

static ProgRegistro porChave(const char *chave) {
  ProgRegistro c; memset(&c, 0, sizeof c);
  prog_por_chave(chave, &c);
  return c;
}

// ---- casos -----------------------------------------------------------------

static void pushNoFormatoDoWeb(void) {
  ProgRegistro r[8];
  zerar();
  agora = 1757000100000LL;
  assert(prog_gravar_local("tt1234567", 4, 9, 1432, 2640, -1));      // serie de catalogo
  assert(prog_gravar_local("tt7654321", 0, 0, 5400, 7200, -1));      // filme
  assert(prog_gravar_local("tt5550000", 0, 0, 10, 30, -1));          // dur < 60s: nao vai
  assert(prog_pendentes(r, 8) == 3);

  assert(syncprog_empurrar() == 2);
  assert(chamadas == 1 && !strcmp(ultimaFuncao, "sync_push_watch_progress"));
  assert(tem("\"p_profile_id\":1"));
  assert(tem("\"p_origin_client_id\":\"cliente-teste-0001\""));
  // Serie: chave do web, tipo certo, episodio separado, video_id sintetico, ms.
  assert(tem("\"content_id\":\"tt1234567\""));
  assert(tem("\"content_type\":\"series\""));
  assert(tem("\"video_id\":\"__nuvio_episode__:4:9\""));
  assert(tem("\"season\":4") && tem("\"episode\":9"));
  assert(tem("\"position\":1432000") && tem("\"duration\":2640000"));
  assert(tem("\"last_watched\":1757000100000"));
  assert(tem("\"progress_key\":\"tt1234567_s4e9\""));
  // Filme: season/episode nulos, video_id = content_id, chave = content_id.
  assert(tem("\"content_type\":\"movie\""));
  assert(tem("\"video_id\":\"tt7654321\""));
  assert(tem("\"season\":null") && tem("\"episode\":null"));
  assert(tem("\"progress_key\":\"tt7654321\""));
  // O antigo formato NAO aparece.
  assert(!tem("tt1234567:4:9"));
  assert(!tem("tt5550000"));
  // 2xx: as duas enviadas deixam de ser pendentes; a curta continua.
  assert(prog_pendentes(r, 8) == 1 && !strcmp(r[0].chave, "tt5550000"));
  // Sem pendentes que valham, nao ha pedido.
  chamadas = 0;
  assert(syncprog_empurrar() == 0 && chamadas == 0);
  puts("ok  push: formato do web, so pendentes, marca empurrados em 2xx");
}

static void pushFalhoMantemPendente(void) {
  ProgRegistro r[8];
  zerar();
  assert(prog_gravar_local("tt1234567", 4, 9, 1432, 2640, -1));
  proximoStatus = 500;
  assert(syncprog_empurrar() == -1);
  assert(prog_pendentes(r, 8) == 1 && r[0].pendente);
  puts("ok  push com erro: continua pendente");
}

static void pullAceitaOsFormatosDoServidor(void) {
  ProgRegistro s;
  zerar();
  agora = 1757000200000LL;
  proximaResposta =
    "[{\"content_id\":\"tt1234567\",\"content_type\":\"series\",\"season\":4,\"episode\":9,"
    "  \"position_ms\":900000,\"duration_ms\":2640000,\"position\":900000,\"duration\":2640000,"
    "  \"updated_at\":\"2026-09-04T18:00:00.250+00:00\",\"progress_key\":\"tt1234567_s4e9\"},"
    " {\"content_id\":\"tt7654321\",\"content_type\":\"movie\",\"season\":null,\"episode\":null,"
    "  \"position\":5400000,\"duration\":7200000,\"last_watched\":1757000150000},"
    " {\"content_id\":\"tt5550000:2:3\",\"position\":1000,\"duration\":100000,\"last_watched\":1757000150},"
    " {\"content_id\":\"tt9990000\",\"position\":10,\"duration\":500}]";
  assert(syncprog_puxar() == 3);            // a de dur <= 1s fica de fora
  assert(!strcmp(ultimaFuncao, "sync_pull_watch_progress") && tem("\"p_profile_id\":1"));
  assert(syncprog_puxadas() == 3);
  { int casaram = -1;
    assert(syncprog_aplicar(&casaram) == 3);
    assert(casaram == 2 && aplicadosNoCatalogo == 2);   // tt5550000 nao esta no catalogo
    assert(syncprog_puxadas() == 0); }
  s = porChave("tt1234567_s4e9");
  assert(s.chave[0] && s.posSeg == 900 && s.durSeg == 2640 && !s.pendente);
  assert(s.lastWatchedMs == 1788544800250LL);        // 2026-09-04T18:00:00.250Z
  s = porChave("tt7654321");
  assert(s.chave[0] && s.posSeg == 5400 && s.lastWatchedMs == 1757000150000LL && !strcmp(s.tipo, "movie"));
  // Cliente antigo mandou id composto: chave recalculada, segundos virados em ms.
  s = porChave("tt5550000_s2e3");
  assert(s.chave[0] && s.temporada == 2 && s.episodio == 3 && !strcmp(s.contentId, "tt5550000"));
  assert(s.lastWatchedMs == 1757000150000LL);
  assert(!porChave("tt9990000").chave[0]);
  puts("ok  pull: ISO e numero, ms e s, id composto, retem sem catalogo");
}

static void rollbackNaoAcontece(void) {
  ProgRegistro s;
  zerar();
  agora = 1757000300000LL;
  // 1. Assistiu aqui ate 1500s. Pendente.
  assert(prog_gravar_local("tt1234567", 4, 9, 1500, 2640, -1));
  // 2. Ciclo: pull traz o servidor ANTIGO (900s, 5 min atras) ...
  proximaResposta =
    "[{\"content_id\":\"tt1234567\",\"season\":4,\"episode\":9,"
    "  \"position\":900000,\"duration\":2640000,\"last_watched\":1757000000000}]";
  assert(syncprog_puxar() == 1);
  // 3. ... e empurra o novo.
  assert(syncprog_empurrar() == 1);
  assert(tem("\"position\":1500000"));
  // 4. No fio principal, o puxado e aplicado. Antes do conserto, aqui a posicao
  //    voltava para 900 e o arquivo era regravado com ela.
  assert(syncprog_aplicar(NULL) == 0);
  assert(aplicadosNoCatalogo == 0);
  s = porChave("tt1234567_s4e9");
  assert(s.posSeg == 1500 && !s.pendente);           // ja subiu, e continua 1500
  // 5. Proximo ciclo: o servidor agora tem o nosso (ou algo mais novo). Entra.
  proximaResposta =
    "[{\"content_id\":\"tt1234567\",\"season\":4,\"episode\":9,"
    "  \"position\":1600000,\"duration\":2640000,\"last_watched\":1757000400000}]";
  assert(syncprog_puxar() == 1 && syncprog_aplicar(NULL) == 1);
  assert(aplicadosNoCatalogo == 1 && ultimaPosAplicada == 1600);
  assert(porChave("tt1234567_s4e9").posSeg == 1600);
  // 6. Um eco velho depois disso nao regride.
  proximaResposta =
    "[{\"content_id\":\"tt1234567\",\"season\":4,\"episode\":9,"
    "  \"position\":900000,\"duration\":2640000,\"last_watched\":1757000000000}]";
  assert(syncprog_puxar() == 1 && syncprog_aplicar(NULL) == 0);
  assert(porChave("tt1234567_s4e9").posSeg == 1600);
  puts("ok  rollback: pull antigo nao volta a posicao, nem antes nem depois do push");
}

static void migradoSobeComChaveCerta(void) {
  zerar();
  // Arquivo do formato antigo: serie de catalogo (5 colunas) e item do Trakt.
  arquivo = strdup("tt1234567\t1432\t2640\t4\t9\ntt7654321:1:2\t100\t3000\n");
  prog_invalidar();
  agora = 1757000500000LL;
  assert(syncprog_empurrar() == 2);
  assert(tem("\"progress_key\":\"tt1234567_s4e9\"") && tem("\"content_type\":\"series\""));
  assert(tem("\"progress_key\":\"tt7654321_s1e2\"") && tem("\"content_id\":\"tt7654321\""));
  assert(!tem("tt7654321:1:2"));
  // Sem hora conhecida, vai a de agora — nunca 0.
  assert(tem("\"last_watched\":1757000500000"));
  puts("ok  linhas antigas sobem como serie, com a chave do web");
}

static void avancarDurantePush(void) {
  // Mesmo milissegundo: comparar somente a hora ainda perderia este avanco.
  assert(prog_gravar_local("tt1234567", 4, 9, 1500, 2640, -1));
}

static void trocarPerfilDurantePush(void) { perfil = 2; }

static void confirmacaoPreservaEscritaNovaEPerfil(void) {
  ProgRegistro r;
  zerar(); perfil = 1; agora = 1757000600000LL;
  assert(prog_gravar_local("tt1234567", 4, 9, 1432, 2640, -1));
  duranteRpc = avancarDurantePush;
  assert(syncprog_empurrar() == 1 && tem("\"position\":1432000"));
  r = porChave("tt1234567_s4e9");
  assert(r.posSeg == 1500 && r.pendente);
  duranteRpc = NULL;
  assert(syncprog_empurrar() == 1 && tem("\"position\":1500000"));
  assert(!porChave("tt1234567_s4e9").pendente);

  zerar(); perfil = 1;
  assert(prog_gravar_local("tt1234567", 4, 9, 100, 2640, -1));
  perfil = 2;
  assert(prog_gravar_local("tt1234567", 4, 9, 200, 2640, -1));
  perfil = 1; duranteRpc = trocarPerfilDurantePush;
  assert(syncprog_empurrar() == 1 && tem("\"p_profile_id\":1"));
  assert(perfil == 2 && porChave("tt1234567_s4e9").pendente);
  perfil = 1;
  assert(!porChave("tt1234567_s4e9").pendente);
  duranteRpc = NULL;
  puts("ok  confirmacao do push preserva escrita mais nova e perfil diferente");
}

static void pullRecusaNumerosForaDaFaixa(void) {
  zerar(); perfil = 1;
  proximaResposta =
    "[{\"content_id\":\"tt1\",\"season\":1e100,\"episode\":1,\"position\":10000,\"duration\":600000},"
    "{\"content_id\":\"tt2\",\"season\":1,\"episode\":1e100,\"position\":10000,\"duration\":600000},"
    "{\"content_id\":\"tt3\",\"season\":1,\"episode\":1.5,\"position\":10000,\"duration\":600000},"
    "{\"content_id\":\"tt4\",\"position\":10000,\"duration\":1e100},"
    "{\"content_id\":\"tt5\",\"position\":1e100,\"duration\":600000},"
    "{\"content_id\":\"tt6\",\"season\":1,\"episode\":1e309,\"position\":10000,\"duration\":600000},"
    "{\"content_id\":\"tt7\",\"position_ms\":1e309,\"position\":10000,\"duration\":600000},"
    "{\"content_id\":\"tt8\",\"duration_ms\":1e309,\"position\":10000,\"duration\":600000},"
    "{\"content_id\":\"tt9\",\"position_ms\":\"bad\",\"position\":10000,\"duration\":600000},"
    "{\"content_id\":\"tt7654321\",\"season\":null,\"episode\":null,\"position_ms\":null,\"duration_ms\":null,\"position\":20000,\"duration\":600000},"
    "{\"content_id\":\"tt1234567\",\"position\":10000,\"duration\":600000,\"updated_at\":1e100,\"last_watched\":1757000150000}]";
  assert(syncprog_puxar() == 2);
  assert(syncprog_aplicar(NULL) == 2);
  assert(porChave("tt1234567").lastWatchedMs == 1757000150000LL);
  assert(porChave("tt7654321").posSeg == 20);
  assert(!porChave("tt1_s0e1").chave[0]);
  puts("ok  pull rejeita coordenadas e tempos fora da faixa antes dos casts");
}

static void trocarPerfilDurantePull(void) { perfil = 2; }

static void pullDoPerfilAnteriorNaoSeMistura(void) {
  zerar(); perfil = 1;
  proximaResposta = "[{\"content_id\":\"tt1234567\",\"position\":10000,\"duration\":600000}]";
  duranteRpc = trocarPerfilDurantePull;
  assert(syncprog_puxar() == 1 && tem("\"p_profile_id\":1"));
  assert(perfil == 2 && syncprog_aplicar(NULL) == 0);
  assert(!porChave("tt1234567").chave[0]);
  perfil = 1; duranteRpc = NULL;
  assert(syncprog_puxar() == 1);
  perfil = 2; // A troca tambem pode ocorrer depois da resposta e antes da aplicacao.
  assert(syncprog_aplicar(NULL) == 0 && !porChave("tt1234567").chave[0]);
  perfil = 1;
  assert(syncprog_puxar() == 1 && syncprog_aplicar(NULL) == 1);
  assert(porChave("tt1234567").posSeg == 10);
  puts("ok  pull do perfil anterior e descartado depois da troca de perfil");
}

int main(void) {
  prog_definir_relogio(relogio);
  pushNoFormatoDoWeb();
  pushFalhoMantemPendente();
  pullAceitaOsFormatosDoServidor();
  rollbackNaoAcontece();
  migradoSobeComChaveCerta();
  confirmacaoPreservaEscritaNovaEPerfil();
  pullRecusaNumerosForaDaFaixa();
  pullDoPerfilAnteriorNaoSeMistura();
  puts("syncprog: tudo ok");
  return 0;
}
