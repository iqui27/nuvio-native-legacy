// #360 MAO UNICA (11/10/2026): a TV so PUXA os addons da conta, nunca envia.
//
// Mesmo arnes de tests/contaoffline.c (fio, sync_iniciar/sync_passo/
// sync_periodico de verdade; disco numa pasta de verdade; servidor dublado).
// O servidor dublado guarda a lista de addons da conta em servidor.txt e conta
// as chamadas a sync_push_addons — que nunca podem acontecer.
//
// Cenarios (um processo por sessao; "reiniciar" = processo novo, mesmo disco):
//   periodico   o ciclo de 5 min nao roda com o player aberto (app.c), mas a
//               primeira chamada depois de fechar o player ja parte o ciclo
//               atrasado — nao espera mais 5 minutos.
//   maounica    edicao local (instalar, desligar, reordenar) nao gera
//               pendencia nem push, e o proximo pull sobrescreve com a conta.
//               O botao "Sincronizar addons" puxa a lista nova na hora.
//   duplicata   a conta com o mesmo addon duas vezes (heranca do push antigo)
//               e aplicada uma vez so; o pull repetido nao reinstala.
//   gravar      a lista aplicada vira snapshot em disco (contacache,
//               superficie "addons-aplicados").
//   reabrir     reinicio com o snapshot ja em disco: o pull identico nao
//               reaplica (e o que cura o "refreshes the addon at every
//               startup" do relato).
//   migrar      pendencias conta-addons-pend-*.txt da versao anterior (de
//               qualquer conta/perfil, inclusive .tmp) sao apagadas na
//               primeira execucao, sem nunca serem enviadas.
#include "sync.h"
#include "nvtmp.h"
#include "addons.h"
#include "catordem.h"
#include "contacache.h"
#include "traktauth.h"
#include "perfis.h"
#include "debrid.h"
#include "stub_perfisessao.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// ------------------------------------------------------------ disco dublado

static const char *pasta(void) {
  const char *d = getenv("NV_T_DIR");
  return d && *d ? d : nvt_dir();
}
static void caminho(char *dst, size_t tam, const char *nome) {
  snprintf(dst, tam, "%s/%s", pasta(), nome);
}
const char *dados_dir(void) { return pasta(); }
void dados_uuid(char *dst, unsigned tam) {
  static unsigned seq;
  snprintf(dst, tam, "%08x-0000-4000-8000-%012x", (unsigned)getpid(), ++seq);
}
char *dados_ler(const char *nome) {
  char c[600];
  FILE *f;
  long n;
  char *b;
  caminho(c, sizeof c, nome);
  f = fopen(c, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)n + 1);
  n = (long)fread(b, 1, (size_t)n, f);
  b[n] = 0;
  fclose(f);
  return b;
}
int dados_gravar(const char *nome, const char *conteudo) {
  char c[600];
  FILE *f;
  caminho(c, sizeof c, nome);
  f = fopen(c, "wb");
  if (!f) return 0;
  fputs(conteudo ? conteudo : "", f);
  fclose(f);
  return 1;
}
int dados_apagar(const char *nome) {
  char c[600];
  caminho(c, sizeof c, nome);
  return remove(c) == 0;
}
const char *dados_cliente_id(void) { return "tv-teste"; }

// ------------------------------------------------------------ perfis dublados

int  perfis_ativo(void)          { return 1; }
int  perfis_ativo_addons(void)   { return 1; }
int  perfis_n(void)              { return 1; }
int  perfis_precisa_escolher(void) { return 0; }
const char *perfis_dono(void)    { return "dono-a"; }
void perfis_esquecer(void)       { }
const ContaPerfil *perfis_item(int i) { (void)i; return NULL; }
const char *i18n(const char *s) { return s; }
int ajustes_social(void) { return 1; }
int ajustes_hist_conta(void) { return 1; }
void ajustes_perfil_guardar(int perfil) { (void)perfil; }
int  ajustes_perfil_restaurar(int perfil) { (void)perfil; return 0; }
void ajustes_perfil_esquecer(void) { }
void arteesc_esquecer(void) { }
void sorg_esquecer(void) { }
int  perfis_puxar(void) { return 1; }

// ------------------------------------------------------------ servidor dublado
// A conta: uma letra por addon (A -> https://a.exemplo/manifest.json), em
// servidor.txt para sobreviver ao "reinicio". Minuscula = desligado.

static char conta[32];
static int pushModo = 200, pushes;
static char ultimoPush[4096];
// Cenario "cheio": a conta tem ate 130 addons numerados (https://nNN.exemplo/...),
// fora do mapa de letras. contaN[] guarda os numeros; pushN[] o que o ultimo push levou.
static int cheio, contaN[130], nContaN, pushN[130], nPushN;

static void url(char *dst, size_t t, char l) {
  snprintf(dst, t, "https://%c.exemplo/manifest.json", l | 0x20);
}
static char letra(const char *u) {
  const char *p = strstr(u, "https://");
  return p && p[9] == '.' ? p[8] : '?';
}
static void contaLer(void) {
  char *t = dados_ler("servidor.txt");
  snprintf(conta, sizeof conta, "%s", t ? t : "");
  free(t);
}
static void contaGravar(void) { dados_gravar("servidor.txt", conta); }

char *sessao_tabela(const char *t, const char *q, int *st) {
  char b[4096], u[80];
  size_t k = 0;
  int i;
  (void)q;
  *st = 200;
  if (strcmp(t, "addons")) return strdup("[]");
  if (cheio) {
    static char g[130 * 90];
    size_t kk = 0;
    kk += (size_t)snprintf(g, sizeof g, "[");
    for (i = 0; i < nContaN; i++)
      kk += (size_t)snprintf(g + kk, sizeof g - kk,
        "%s{\"url\":\"https://n%d.exemplo/manifest.json\",\"name\":\"n%d\",\"enabled\":true}", i ? "," : "", contaN[i], contaN[i]);
    snprintf(g + kk, sizeof g - kk, "]");
    return strdup(g);
  }
  k += (size_t)snprintf(b + k, sizeof b - k, "[");
  for (i = 0; conta[i]; i++) {
    url(u, sizeof u, conta[i]);
    k += (size_t)snprintf(b + k, sizeof b - k, "%s{\"url\":\"%s\",\"name\":\"%c\",\"enabled\":%s}",
                          i ? "," : "", u, conta[i] & ~0x20,
                          (conta[i] >= 'A' && conta[i] <= 'Z') ? "true" : "false");
  }
  snprintf(b + k, sizeof b - k, "]");
  return strdup(b);
}
char *sessao_rpc(const char *funcao, const char *corpo, int *st) {
  *st = 200;
  if (!strcmp(funcao, "sync_push_addons")) {
    const char *p;
    char nova[32];
    int n = 0;
    pushes++;
    if (cheio) {
      nPushN = 0;
      for (p = strstr(corpo, "https://n"); p && nPushN < 130; p = strstr(p + 1, "https://n")) pushN[nPushN++] = atoi(p + 9);
      nContaN = nPushN;                        // push SUBSTITUI a lista
      memcpy(contaN, pushN, sizeof contaN);
      return strdup("null");
    }
    snprintf(ultimoPush, sizeof ultimoPush, "%s", corpo);
    *st = pushModo;
    if (pushModo == 0) return NULL;
    if (pushModo != 200) return strdup(pushModo == 400 ? "{\"code\":\"23514\",\"message\":\"check violation\"}" : "error code: 503");
    // SUBSTITUI a lista do perfil (delete + insert), como o web faz.
    for (p = strstr(corpo, "\"url\":\""); p && n < 31; p = strstr(p + 1, "\"url\":\"")) {
      const char *en = strstr(p, "\"enabled\":");
      char l = (char)(letra(p + 7) & ~0x20);
      if (en && !strncmp(en + 10, "false", 5)) l = (char)(l | 0x20);
      nova[n++] = l;
    }
    nova[n] = 0;
    snprintf(conta, sizeof conta, "%s", nova);
    contaGravar();
    return strdup("null");
  }
  return strdup("[]");
}
int  sessao_logada(void)          { return 1; }
const char *sessao_usuario(void)  { return "conta-a"; }
int perfis_puxar_sessao(uint64_t s, const char *u, const char *t) { (void)s; (void)u; (void)t; return perfis_puxar(); }
void perfis_invalidar_sessao(void) {}
int sessao_token_copiar(char *t, size_t n) { snprintf(t, n, "fixture"); return 1; }
int sessao_identidade_local_copiar(char *u, size_t n, char *t, size_t m) {
  snprintf(u, n, "%s", sessao_usuario()); return sessao_token_copiar(t, m);
}
void sessao_token_recusado(const char *t) { (void)t; }
char *nuvem_tabela(const char *t, const char *q, const char *jwt, int *st) {
  (void)jwt; return sessao_tabela(t, q, st);
}
char *nuvem_rpc_com(const char *f, const char *b, const char *jwt, int *st) {
  (void)jwt; return sessao_rpc(f, b, st);
}
#ifndef ADDON_PULL_FIXTURE
char *nuvem_addon_alternar(const char *q, int a, const char *jwt, unsigned prazo_ms, int *st) {
  (void)prazo_ms; (void)q; (void)a; (void)jwt; *st = 503; return NULL;
}
#endif
void ajustes_perfil_padrao(void) {}
void desc_abrir_sessao(void) {}
void desc_sessao_preparada(void) {}
void desc_sessao_passo(void) {}
void extras_abrir_sessao(void) {}
void recomenda_esquecer_conta(void) {}
void debrid_conta_iniciar(DebridConta *c) { memset(c, 0, sizeof *c); }
void debrid_conta_definir(DebridConta *c, const char *s, const char *k) { (void)c; (void)s; (void)k; }
int debrid_aplicar_chaves_conta(const DebridConta *c, uint64_t s) { (void)c; (void)s; return 0; }
int syncprog_puxar_sessao(int p, uint64_t s) { (void)p; (void)s; return 0; }
int syncprog_empurrar_sessao(int p, uint64_t s) { (void)p; (void)s; return 0; }
int  nuvem_freio_ativo(void)      { return 0; }
int  nuvem_erro_ausente(const char *c) { (void)c; return 0; }
const char *nuvem_trakt_cliente(void) { return ""; }
void nuvem_url_escapar(const char *v, char *d, unsigned t) { snprintf(d, t, "%s", v); }

// ------------------------------------------------------------ a TV dublada
// A lista de addons da TV (addons.c): o sync aplica com addons_definir_lista e
// le com addons_exportar.

static AddonRemoto tv[130];
static int nTv, aplicacoes;
int addons_definir_lista(const AddonRemoto *l, int n) {
  int i;
  for (i = 0; i < n && i < 130; i++) tv[i] = l[i];
  nTv = n < 130 ? n : 130;
  aplicacoes++;
  return 1;
}
int addons_exportar(AddonRemoto *s, int m) {
  int i;
  for (i = 0; i < nTv && i < m; i++) s[i] = tv[i];
  return i;
}
static void tvAcrescentar(char l) {
  memset(&tv[nTv], 0, sizeof tv[nTv]);
  url(tv[nTv].url, sizeof tv[nTv].url, l);
  snprintf(tv[nTv].nome, sizeof tv[nTv].nome, "%c", l);
  tv[nTv++].ativo = 1;
}
static void tvLetras(char *dst, size_t t) {
  int i;
  size_t k = 0;
  for (i = 0; i < nTv && k + 1 < t; i++) {
    char l = letra(tv[i].url);
    dst[k++] = tv[i].ativo ? (char)(l & ~0x20) : l;
  }
  dst[k] = 0;
}
static int colConta, bibConta, vistoConta;
int  col_definir_json(const char *j) { if (j && strstr(j, "colecao-conta")) colConta++; return 0; }
int  contalib_ler_biblioteca(const char *j) { if (j && strstr(j, "bib-conta")) bibConta++; return 0; }
int  contalib_ler_vistos(const char *j) { if (j && strstr(j, "visto-conta")) vistoConta++; return 0; }
void desc_remontar_fileiras(void) {}
// 1.8 sync.c also talks to plugins.c, colfileiras.c and colecoes.c revision/validation;
// none of them is under test here, so they are inert: no plugin edit pending, no
// collection-row revision change.
#include "plugins.h"
#include "colecoes.h"
#include "colfileiras.h"
void plugins_retrato(PlugRetrato *s) { memset(s, 0, sizeof *s); }
int  plugins_confirmar(unsigned rev, unsigned g) { (void)rev; (void)g; return 1; }
int  plugins_definir_da_conta(const PlugRepo *l, int n, unsigned g) { (void)l; (void)n; (void)g; return 0; }
int  plugins_ler_conta(const char *j, PlugRepo *o, int max) { (void)j; (void)o; (void)max; return 0; }
void plugins_perfil_mudou(void) {}
void plugins_esquecer(void) {}
int  col_resposta_valida(const char *j) { return j && *j; }
unsigned col_revisao(void) { return 0; }
void colfileiras_sincronizar(void) {}
int  colfileiras_receber(const char *j) { return col_definir_json(j); }   // real one forwards the blob to colecoes.c
void colfileiras_contexto(void) {}
void desc_repetir_silencioso(void) {}
void desc_repetir(void) {}
void desc_repetir_addons(void) {}
void desc_refazer_continuar(void) {}
void desc_loc_apagar(void) {}
void desc_esquecer(void) {}
void desc_tmdb_definir(const char *c) { (void)c; }

// ------------------------------------------------------------ o resto, mudo

void addons_marcar_da_conta(int perfil) { (void)perfil; }
void addons_esquecer(void) {}
void agenda_esquecer(void) {}
void lembrete_esquecer_todos(void) {}
int  ajustes_aplicar_blob(const char *j) { (void)j; return 0; }
void ajustes_definir_ocultar_nao_lancados(int l) { (void)l; }
int  ajustes_mesclar_blob(const char *b, char **s) { (void)b; *s = NULL; return 0; }
void ajustes_tmdb_idioma_relatar(const char *b) { (void)b; }
void ajustes_idiomas_da_conta(const char *b) { (void)b; }
void buscasrec_esquecer(void) {}
void cachearte_limpar_referencias(void) {}
int  cat_apagar_cache(void) { return 0; }
int  contalib_aplicar_catalogo(void) { return 0; }
int  contalib_aplicar_vistos(void) { return 0; }
void contalib_esquecer(void) {}
unsigned contalib_vistos_revisao(void) { return 0; }   // #199
int  simkl_ativo(void) { return 0; }
void contalib_reconciliar(void) {}
void debrid_definir_chave(const char *s, const char *c) { (void)s; (void)c; }
void debrid_esquecer(void) {}
void extras_definir_chave(const char *c) { (void)c; }
void fontepref_esquecer(void) {}
void legmem_esquecer(void) {}
void audiomem_esquecer(void) {}
void homeestado_esquecer(void) {}
void mapa_esquecer(void) {}
void prog_esquecer_tudo(void) {}
void perfilcont_esquecer(void) {}
void psparede_esquecer(void) {}   // sync.c 2.0 chama no logout; faltava aqui e o teste nao ligava
void recomenda_esquecer(void) {}
void salvos_esquecer(void) {}
void stalker_esquecer(void) {}
int  syncprog_aplicar(int *c) { (void)c; return 0; }
int  syncprog_empurrar(void) { return 0; }
void syncprog_esquecer(void) {}
int  syncprog_puxadas(void) { return 0; }
int  syncprog_puxar(void) { return 0; }
int  trakt_ativo(void) { return 0; }
int  trakt_definir(const char *t, const char *c) { (void)t; (void)c; return 0; }
int  trakt_credencial_igual(const char *t, const char *c) { (void)t; (void)c; return 0; }
void trakt_esquecer(void) {}
TraEstado traktauth_estado(void) { return TRA_LIGADO; }
void vistoep_esquecer(void) {}
void xtream_esquecer(void) {}
void servidores_esquecer_todos(void) {}

// ------------------------------------------------------------ roteiro

static int falhas;
static unsigned agora = 1000;
static void confere(const char *d, int ok) {
  printf("  %-70s %s\n", d, ok ? "ok" : "FALHOU");
  if (!ok) falhas++;
}
static void ateTerminar(void) {
  int i;
  for (i = 0; i < 4000 && sync_estado() == SYNC_RODANDO; i++) { sync_passo(agora += 16); usleep(1000); }
  sync_passo(agora += 16);
  sync_passo(agora += 16);
}
static void proximoCiclo(void) {
  int r = sync_periodico(agora += SYNC_INTERVALO_MS + 1);
  if (!r) printf("  (sync_periodico nao partiu)\n");
  ateTerminar();
}
static int existe(const char *nome) {
  char *b = dados_ler(nome);
  int ok = b != NULL;
  free(b);
  return ok;
}
#define PEND "conta-addons-pend-636f6e74612d61-p1.txt"

int main(int argc, char **argv) {
  const char *cen = argc > 1 ? argv[1] : "";
  char l[40];
  setvbuf(stdout, NULL, _IOLBF, 0);
  if (argc > 2) pushModo = atoi(argv[2]);
  contaLer();
  printf("-- cenario %s%s%s (conta no inicio: %s)\n", cen, argc > 2 ? " " : "", argc > 2 ? argv[2] : "", conta);

  if (!strcmp(cen, "cheio")) {
    int i, total = argc > 2 ? atoi(argv[2]) : 100;
    cheio = 1; nContaN = total;
    for (i = 0; i < total; i++) contaN[i] = i;
    sync_iniciar(); ateTerminar();
    confere("conta aceita ate 128 addons", nTv == (total > 128 ? 128 : total));
    confere("ordem e ultimo addon preservados", nTv > 0 && strstr(tv[nTv - 1].url, total > 128 ? "n127.exemplo" : "n99.exemplo"));
    confere("nenhum push", pushes == 0);
  } else if (!strcmp(cen, "manual")) {
    snprintf(conta, sizeof conta, "AB");
    sync_iniciar(); // Click while the first cycle is already running.
    confere("manual request accepted", sync_manual_addons() == 1);
    confere("duplicate click does not enqueue", sync_manual_addons() == 1);
    confere("manual request reports busy", sync_manual_addons_rodando());
    ateTerminar();
    ateTerminar();
    confere("manual request completes", !sync_manual_addons_rodando());
    confere("manual result reports success", sync_manual_addons_estado(NULL, NULL, NULL) == 2);
    confere("manual result is consumed once", sync_manual_addons_estado(NULL, NULL, NULL) == 0);
    tvLetras(l, sizeof l);
    confere("manual request applied account addons", !strcmp(l, "AB"));
    sync_manual_addons();
    sync_esquecer_usuario();
    confere("logout clears pending manual request", !sync_manual_addons_rodando());
    confere("logout clears manual result", sync_manual_addons_estado(NULL, NULL, NULL) == 0);
  } else if (!strcmp(cen, "periodico")) {
    unsigned fim;
    snprintf(conta, sizeof conta, "AB");
    sync_iniciar(); ateTerminar();
    fim = sync_ultimo_ok();
    confere("primeiro ciclo terminou bem", fim != 0);
    confere("antes de 5 min o periodico nao parte", sync_periodico(fim + SYNC_INTERVALO_MS - 1) == 0);
    // O player ficou aberto 2 h: app.c nao chamou sync_periodico nesse tempo.
    // Na PRIMEIRA chamada depois de fechar, o ciclo atrasado parte na hora.
    agora = fim + 2u * 3600u * 1000u;
    confere("1o quadro depois de 2 h de player: o ciclo atrasado parte JA", sync_periodico(agora) == 1);
    ateTerminar();
    confere("e o relogio volta a contar a partir dele", sync_ultimo_ok() >= agora - 64);
  } else if (!strcmp(cen, "maounica")) {
    snprintf(conta, sizeof conta, "AB"); contaGravar();
    sync_iniciar(); ateTerminar();
    int aplicou = aplicacoes;
    proximoCiclo();
    confere("pull identico nao reaplica", aplicacoes == aplicou);
    tvAcrescentar('D'); sync_sujar_addons(NULL);
    confere("edicao local nao cria pendencia", !existe(PEND));
    proximoCiclo(); tvLetras(l, sizeof l);
    confere("conta inalterada e nenhum push", pushes == 0 && !strcmp(conta, "AB"));
    confere("pull sobrescreve edicao local mesmo com conta igual", !strcmp(l, "AB"));
    tv[0].ativo = 0; sync_sujar_addons(tv[0].url);
    proximoCiclo(); tvLetras(l, sizeof l);
    confere("pull preserves explicit local disable", !strcmp(l, "aB"));
    AddonRemoto tmp = tv[0]; tv[0] = tv[1]; tv[1] = tmp;
    sync_sujar_addons(NULL); proximoCiclo(); tvLetras(l, sizeof l);
    confere("pull restores account order and preserves local disable", !strcmp(l, "aB"));
    snprintf(conta, sizeof conta, "A");
    sync_manual_addons(); ateTerminar(); tvLetras(l, sizeof l);
    confere("manual pull removes addon and preserves local disable", !strcmp(l, "a") && !pushes);
  } else if (!strcmp(cen, "duplicata")) {
    snprintf(conta, sizeof conta, "ABB");
    sync_iniciar(); ateTerminar();
    confere("dedupe da conta mantem primeiro URL e estado", nTv == 2 && tv[0].ativo);
    int aplicou = aplicacoes;
    proximoCiclo();
    confere("duplicata repetida nao reinstala", aplicacoes == aplicou && !pushes);
  } else if (!strcmp(cen, "gravar") || !strcmp(cen, "reabrir")) {
    snprintf(conta, sizeof conta, "AB");
    // Simula a lista local restaurada no boot. O pull nao deve reinstala-la.
    if (!strcmp(cen, "reabrir")) { tvAcrescentar('A'); tvAcrescentar('B'); }
    sync_iniciar(); ateTerminar();
    confere("snapshot nao reaplica no proximo arranque", aplicacoes == (!strcmp(cen, "gravar") ? 1 : 0));
    proximoCiclo();
    confere("pull repetido nao reinstala", aplicacoes == (!strcmp(cen, "gravar") ? 1 : 0));
  } else if (!strcmp(cen, "migrar")) {
    dados_gravar(PEND, "{\"owner\":\"conta-a\",\"profile\":1,\"edit\":\"old\",\"addons\":[{\"url\":\"https://d.exemplo/manifest.json\",\"enabled\":true}]}");
    dados_gravar(PEND ".tmp", "interrompido");
    dados_gravar("conta-addons-pend-636f6e74612d62-p2.txt", "outra conta");
    dados_gravar("conta-outro.txt", "preservar");
    snprintf(conta, sizeof conta, "AB");
    sync_iniciar(); ateTerminar(); tvLetras(l, sizeof l);
    confere("pendencias antigas e temporarios removidos sem enviar", !existe(PEND) && !existe(PEND ".tmp") && !existe("conta-addons-pend-636f6e74612d62-p2.txt") && pushes == 0);
    confere("migracao segue conta e preserva outros arquivos", !strcmp(l, "AB") && existe("conta-outro.txt"));
  } else {
    printf("cenario desconhecido\n");
    return 2;
  }
  printf("%s\n", falhas ? "FALHOU" : "PASSOU");
  return falhas ? 1 : 0;
}

void cat_historico_contexto(const char *u, int p) { (void)u; (void)p; }

__attribute__((weak)) int addons_perfil_da_lista(void) { return 0; }
