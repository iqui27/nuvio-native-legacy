// Envio do registro do Farol (issue 18) contra o servidor local
// tests/farol_envio_server.py e, com --worker, contra o worker real
// (servidor/recomendacoes) servido por tests/farol_envio.sh.
//
// Cobre: 200 com codigo, 200 com `conhecido` corrigido (sem POST), conhecido
// nao-corrigido, 429 (preserva o codigo), 422, 5xx, 200 sem codigo, GET 500,
// prazo/sem resposta, cancelamento, sem conta, sem servidor, offline ->
// farol-pendente.json -> rede volta -> reenvio e pendente apagado, falha de
// transporte com espera, reenvio manual, expiracao de 24 h, pendente lixo,
// 422/503 no reenvio, opt-in do log, os 256 eventos mais recentes e nenhum
// I/O de arquivo no fio principal.
#include "farolenvio.h"
#include "farol.h"
#include "farolanel.h"
#include "farolperfil.h"
#include "rede.h"
#include "resolverregra.h"
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FALHA %s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)

// ------------------------------------------------------------ estado fake --
static pthread_mutex_t tv = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  tvCond = PTHREAD_COND_INITIALIZER;
static pthread_t       fioPrincipal;
static char            dir[512];
static int             temConta = 1, offline, logPedidos, envios, ultimoHttp, dadosNoPrincipal;
static int             barreiraAuthArm, barreiraAuthDentro, barreiraAuthLib;
static int             barreiraOffArm, barreiraOffDentro, barreiraOffLib;
static int             barreiraRelArm, barreiraRelDentro, barreiraRelLib;
static int             barreiraGravArm, barreiraGravDentro, barreiraGravLib;
static long long       epoch = 1800000000LL;
static uint32_t        anelMs = 1000;

static long long relogioFake(void) {
  long long e;
  pthread_mutex_lock(&tv);
  if (barreiraRelArm && !pthread_equal(pthread_self(), fioPrincipal)) {
    barreiraRelArm = 0;
    barreiraRelDentro = 1;
    pthread_cond_broadcast(&tvCond);
    while (!barreiraRelLib) pthread_cond_wait(&tvCond, &tv);
  }
  e = epoch;
  pthread_mutex_unlock(&tv);
  return e;
}
static void avancar(long long s) { pthread_mutex_lock(&tv); epoch += s; pthread_mutex_unlock(&tv); }
static uint32_t anelRelogio(void) { uint32_t v; pthread_mutex_lock(&tv); v = anelMs; pthread_mutex_unlock(&tv); return v; }
static void setOffline(int v) { pthread_mutex_lock(&tv); offline = v; pthread_mutex_unlock(&tv); }
static void setConta(int v) { pthread_mutex_lock(&tv); temConta = v; pthread_mutex_unlock(&tv); }
static int lerInt(int *p) { int v; pthread_mutex_lock(&tv); v = *p; pthread_mutex_unlock(&tv); return v; }

static void armarBarreira(int *arm, int *dentro, int *lib) {
  pthread_mutex_lock(&tv); *arm = 1; *dentro = 0; *lib = 0; pthread_mutex_unlock(&tv);
}
static void esperarBarreira(int *dentro) {
  pthread_mutex_lock(&tv);
  while (!*dentro) pthread_cond_wait(&tvCond, &tv);
  pthread_mutex_unlock(&tv);
}
static void liberarBarreira(int *lib) {
  pthread_mutex_lock(&tv); *lib = 1; pthread_cond_broadcast(&tvCond); pthread_mutex_unlock(&tv);
}

static void notarIo(void) {
  if (pthread_equal(pthread_self(), fioPrincipal)) {
    pthread_mutex_lock(&tv); dadosNoPrincipal++; pthread_mutex_unlock(&tv);
  }
}

// ---------------------------------------------- ganchos do app (fortes) --
int rede_saude_offline(void) {
  int off;
  pthread_mutex_lock(&tv);
  if (barreiraOffArm && !pthread_equal(pthread_self(), fioPrincipal)) {
    barreiraOffDentro = 1;
    pthread_cond_broadcast(&tvCond);
    while (!barreiraOffLib) pthread_cond_wait(&tvCond, &tv);
    barreiraOffArm = 0;
  }
  off = offline;
  pthread_mutex_unlock(&tv);
  return off;
}
void avisos_enviar_registro_atual(void) {
  CHECK(pthread_equal(pthread_self(), fioPrincipal));
  pthread_mutex_lock(&tv); logPedidos++; pthread_mutex_unlock(&tv);
}
int avisos_registro_cabecalhos(const char **cab, char *aut, size_t nAut,
                               char *via, size_t nVia, char *chave, size_t nChave) {
  (void)chave; (void)nChave;
  CHECK(!pthread_equal(pthread_self(), fioPrincipal));   // a copia do token pode renovar por HTTP
  pthread_mutex_lock(&tv);
  if (barreiraAuthArm) {
    barreiraAuthDentro = 1;
    pthread_cond_broadcast(&tvCond);
    while (!barreiraAuthLib) pthread_cond_wait(&tvCond, &tv);
    barreiraAuthArm = 0;
  }
  pthread_mutex_unlock(&tv);
  if (!lerInt(&temConta)) return 0;
  snprintf(aut, nAut, "Authorization: Bearer tok-farol-18");
  snprintf(via, nVia, "X-Nuvio-Auth: nuvio");
  cab[0] = aut; cab[1] = via; cab[2] = "Content-Type: application/json"; cab[3] = NULL;
  return 1;
}
void farol_registrar_envio(int http) {
  int ok = http >= 200 && http < 300;
  pthread_mutex_lock(&tv); envios++; ultimoHttp = http; pthread_mutex_unlock(&tv);
  farol_ev(FM_FAROL, FE_FAROL_ENVIO, ok ? 0 : 2, http, 0, ok ? "ok" : "erro");
}
int farol_eventos(FarolEvento *dst, int max) {
  if (!dst || max <= 0) return 0;
  return farolanel_ler_ultimos(dst, max > FAROL_ANEL_N ? FAROL_ANEL_N : max, 0);
}

static void caminho(char *dst, size_t n, const char *nome) { snprintf(dst, n, "%s/%s", dir, nome); }
char *dados_ler(const char *nome) {
  char c[700]; FILE *f; long n; char *b;
  notarIo();
  caminho(c, sizeof c, nome);
  f = fopen(c, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = (char *)malloc((size_t)n + 1);
  if (!b) { fclose(f); return NULL; }
  if (n > 0 && fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
  b[n] = 0;
  fclose(f);
  return b;
}
int dados_gravar(const char *nome, const char *conteudo) {
  char c[700], t[720]; FILE *f; int ok;
  notarIo();
  caminho(c, sizeof c, nome);
  snprintf(t, sizeof t, "%s.tmp", c);
  f = fopen(t, "wb");
  if (!f) return 0;
  fputs(conteudo, f);
  fclose(f);
  ok = rename(t, c) == 0;
  pthread_mutex_lock(&tv);
  if (barreiraGravArm && !strcmp(nome, FAROLENVIO_PENDENTE_ARQ) &&
      !pthread_equal(pthread_self(), fioPrincipal)) {
    barreiraGravArm = 0;
    barreiraGravDentro = 1;
    pthread_cond_broadcast(&tvCond);
    while (!barreiraGravLib) pthread_cond_wait(&tvCond, &tv);
  }
  pthread_mutex_unlock(&tv);
  return ok;
}
int dados_apagar(const char *nome) { char c[700]; notarIo(); caminho(c, sizeof c, nome); return unlink(c) == 0; }

// --------------------------------------------------------------- ajudas --
static long tamArq(const char *nome) {
  char c[700]; struct stat st;
  caminho(c, sizeof c, nome);
  return stat(c, &st) == 0 ? (long)st.st_size : -1;
}
static char *lerArq(const char *nome) {
  char c[700]; FILE *f; long n; char *b;
  caminho(c, sizeof c, nome);
  f = fopen(c, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = (char *)malloc((size_t)n + 1);
  CHECK(b && fread(b, 1, (size_t)n, f) == (size_t)n);
  b[n] = 0; fclose(f);
  return b;
}
static void gravarArq(const char *nome, const char *s) {
  char c[700]; FILE *f; caminho(c, sizeof c, nome);
  f = fopen(c, "wb"); CHECK(f); fputs(s, f); fclose(f);
}

static void dormir(int ms) { struct timespec t = { ms / 1000, (long)(ms % 1000) * 1000000L }; nanosleep(&t, NULL); }
static long long msAgora(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000LL + t.tv_nsec / 1000000; }

static FarolEnvioEstado esperar(FarolEnvioInfo *i, int maxMs) {
  long long fim = msAgora() + maxMs;
  while (farolenvio_em_voo()) { CHECK(msAgora() < fim); dormir(5); }
  return farolenvio_info(i);
}

static char baseUrl[256];
static void usar(const char *prefixo) {
  char u[400];
  snprintf(u, sizeof u, "%s%s", baseUrl, prefixo);
  farolenvio_definir_url(u);
}

// GET /stats?p=<prefixo> e le um campo inteiro/string do JSON.
static char statsBuf[4096];
static void stats(const char *prefixo) {
  char u[400]; RedePedido rq; RedeResposta rs;
  snprintf(u, sizeof u, "%s/stats?p=%s", baseUrl, prefixo);
  memset(&rq, 0, sizeof rq); memset(&rs, 0, sizeof rs);
  rq.metodo = "GET"; rq.url = u; rq.prazo_ms = 5000; rq.max_bytes = 4096; rq.max_cabecalhos = 4096;
  rede_pedir(&rq, &rs);
  CHECK(rs.status == 200 && rs.corpo);
  snprintf(statsBuf, sizeof statsBuf, "%.*s", (int)rs.n_corpo, rs.corpo);
  rede_resposta_limpar(&rs);
}
static int sInt(const char *k) {
  char pad[64]; const char *p;
  snprintf(pad, sizeof pad, "\"%s\":", k);
  p = strstr(statsBuf, pad);
  CHECK(p);
  return atoi(p + strlen(pad));
}
static const char *sStr(const char *k) {
  static char out[128]; char pad[64]; const char *p, *q;
  snprintf(pad, sizeof pad, "\"%s\": \"", k);
  p = strstr(statsBuf, pad);
  CHECK(p);
  p += strlen(pad);
  q = strchr(p, '"');
  CHECK(q);
  snprintf(out, sizeof out, "%.*s", (int)(q - p), p);
  return out;
}

// ------------------------------------------------------------ cenario --
static FarolPerfil          gPerfil;
static FarolPacoteTentativa gTentou[2];

static void entrada(FarolPacoteEntrada *in, char *codigoMut) {
  farolperfil_padrao(&gPerfil);
  strcpy(gPerfil.aparelho.plataforma, "webos");
  strcpy(gPerfil.aparelho.so, "4.9");
  gPerfil.aparelho.so_maior = 4;
  strcpy(gPerfil.aparelho.modelo, "OLED55C9PSA");
  strcpy(gPerfil.cliente.versao, "2.1.0");
  strcpy(gPerfil.cliente.commit, "c18c21756");
  strcpy(gPerfil.cliente.build, "webos-ipk");
  strcpy(gPerfil.cliente.backend, "starfish");
  strcpy(gPerfil.cliente.idioma, "pt-BR");
  gTentou[0] = (FarolPacoteTentativa){ "proxima_fonte", 0, 6400 };
  gTentou[1] = (FarolPacoteTentativa){ "reconexao", 0, 13000 };
  farolpacote_entrada_padrao(in);
  in->gatilho = FAROL_G_MANUAL;
  in->sintoma = FAROL_PS_NAO_COMECA;
  strcpy(codigoMut, "TV_FORMATO_REPETE");
  in->codigo = codigoMut;          // a copia profunda e conferida mexendo nele depois
  in->estagio = RES_T_TV;
  in->erro.classe = FC_FORMATO;
  in->erro.nativo = 204;
  strcpy(in->erro.cod, "ums_204");
  in->tentou = gTentou;
  in->n_tentou = 2;
  in->perfil = &gPerfil;
  in->midia.container = "mkv";
  in->midia.video = "hevc10";
  in->midia.hdr = "dv";
  in->midia.res = "2160p";
  in->midia.audio = "eac3_51";
  in->midia.entrega = "debrid_cache";
  in->eventos = NULL;
  in->agora_ms = 0;
  in->janela_ms = FAROL_PACOTE_JANELA_MS;
}

static FarolEnvioEstado enviar(const char *prefixo, const FarolEnvioOpcoes *op, FarolEnvioInfo *i) {
  FarolPacoteEntrada in;
  char cod[48];
  if (prefixo) usar(prefixo);
  entrada(&in, cod);
  CHECK(farol_enviar(&in, op) == 1);
  strcpy(cod, "LIXO_MUTADO");      // o fio ja tem a copia
  gPerfil.aparelho.so_maior = 77;
  return esperar(i, 20000);
}

static void semeiaAnel(int n) {
  int k;
  farolanel_zerar();
  for (k = 0; k < n; k++) {
    pthread_mutex_lock(&tv); anelMs += 10; pthread_mutex_unlock(&tv);
    farol_ev(FM_VIDEO, FE_ERRO_NATIVO, 2, 204, k, "ums_204");
  }
}

static int portaFechada(void) {
  struct sockaddr_in a; socklen_t n = sizeof a; int s = socket(AF_INET, SOCK_STREAM, 0), p;
  CHECK(s >= 0);
  memset(&a, 0, sizeof a); a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  CHECK(bind(s, (struct sockaddr *)&a, sizeof a) == 0);
  CHECK(getsockname(s, (struct sockaddr *)&a, &n) == 0);
  p = ntohs(a.sin_port);
  close(s);
  return p;
}

// ------------------------------------------------------------ testes --
static void t_basico(void) {
  FarolEnvioOpcoes o;
  FarolEnvioInfo i;
  farolenvio_opcoes_padrao(&o);
  CHECK(o.incluir_log == 0);                   // opt-in: log nasce desligado
  CHECK(o.res_sintoma == RES_S_GERAL);
  CHECK(farol_enviar(NULL, NULL) == 0);
  CHECK(farolenvio_info(&i) == FAROLENVIO_OCIOSO);
  CHECK(!i.temPendente && i.codigo[0] == 0);
  CHECK(FAROLENVIO_PRAZO_MS == 30000u);
  CHECK(!strcmp(FAROLENVIO_PENDENTE_ARQ, "farol-pendente.json"));
  CHECK(!strcmp(FAROLENVIO_ENVIADOS_ARQ, "resolver-enviados.txt"));
}

static void t_sem_servidor_e_sem_conta(void) {
  FarolEnvioInfo i;
  farolenvio_definir_url("");
  CHECK(enviar(NULL, NULL, &i) == FAROLENVIO_FALHOU && i.motivo == FAROLENVIO_M_INDISPONIVEL);
  setConta(0);
  CHECK(enviar("/conta", NULL, &i) == FAROLENVIO_FALHOU && i.motivo == FAROLENVIO_M_CONTA);
  stats("/conta");
  CHECK(sInt("get") == 0 && sInt("post") == 0);
  setConta(1);
  CHECK(tamArq(FAROLENVIO_PENDENTE_ARQ) < 0);
  CHECK(lerInt(&envios) == 0);
}

static void t_ok(void) {
  FarolEnvioInfo i;
  FarolEvento ult[2];
  char *env;
  semeiaAnel(300);                              // anel guarda os 256 mais novos
  CHECK(enviar("/ok", NULL, &i) == FAROLENVIO_OK);
  CHECK(i.motivo == FAROLENVIO_M_OK && i.http == 200 && i.codigoNovo == 1 && !i.doPendente);
  CHECK(!strcmp(i.codigo, "4F7K92") && !strcmp(i.codigoExibir, "NV-4F7K-92"));
  CHECK(i.codigoQuando == relogioFake());
  CHECK(i.conhecidoEstado[0] == 0);
  stats("/ok");
  CHECK(sInt("get") == 1 && sInt("post") == 1);
  CHECK(sInt("auth_ok") == 1 && sInt("v_ok") == 1 && sInt("fp_ok") == 1);
  CHECK(!strcmp(sStr("fp_query"), i.fp) && !strcmp(sStr("fp_body"), i.fp));
  CHECK(!strcmp(sStr("c"), "TV_FORMATO_REPETE"));   // copia, nao o buffer mutado
  CHECK(!strcmp(sStr("pl"), "webos") && !strcmp(sStr("ver"), "2.1.0") && !strcmp(sStr("so"), "4"));
  CHECK(sInt("bytes") == (int)i.bytes && i.bytes <= 35000);
  // Os 256 mais recentes: a janela tem os 256 do anel; o corte (se houver) e
  // dos mais antigos e o mais novo (b=299) esta no pacote.
  CHECK(i.linhaEscritos + i.linhaCortados == 256);
  CHECK(sInt("linha_n") == i.linhaEscritos && i.linhaEscritos > 0);
  CHECK(sInt("linha_ult_b") == 299);
  printf("farol_envio: pacote %zu B, linha %d (+%d cortados)\n", i.bytes, i.linhaEscritos, i.linhaCortados);
  // Anel: FE_FAROL_ENVIO com o http.
  CHECK(lerInt(&envios) == 1 && lerInt(&ultimoHttp) == 200);
  CHECK(farolanel_ler_ultimos(ult, 1, 0) == 1 && ult[0].ev == FE_FAROL_ENVIO && ult[0].a == 200);
  // Lista de enviados.
  env = lerArq(FAROLENVIO_ENVIADOS_ARQ);
  CHECK(env && strstr(env, "4F7K92 1800000000 6"));
  free(env);
  CHECK(tamArq(FAROLENVIO_PENDENTE_ARQ) < 0);
  farolenvio_passo();
  CHECK(lerInt(&logPedidos) == 0);              // incluir_log 0: nada de log
}

static void t_corrigido(void) {
  FarolEnvioInfo i;
  int antes = lerInt(&envios);
  CHECK(enviar("/corrigido", NULL, &i) == FAROLENVIO_JA_CORRIGIDO);
  CHECK(i.motivo == FAROLENVIO_M_CORRIGIDO && !strcmp(i.conhecidoEstado, "corrigido"));
  CHECK(!strcmp(i.conhecidoVersao, "2.1.3"));
  CHECK(!strcmp(i.notaPt, "Corrigido na vers\xc3\xa3o 2.1.3 \xe2\x80\x94 atualize"));
  CHECK(!strcmp(i.notaEn, "Fixed in 2.1.3"));
  CHECK(!strcmp(i.codigo, "4F7K92") && i.codigoNovo == 0);
  stats("/corrigido");
  CHECK(sInt("get") == 1 && sInt("post") == 0);   // o registro NAO foi enviado
  CHECK(lerInt(&envios) == antes);
}

static void t_conhecido_segue(void) {
  FarolEnvioInfo i;
  char *env;
  CHECK(enviar("/okcon", NULL, &i) == FAROLENVIO_OK);
  CHECK(!strcmp(i.codigo, "8HJ3MX") && !strcmp(i.conhecidoEstado, "contorno") && !strcmp(i.notaPt, "Use AC3"));
  stats("/okcon");
  CHECK(sInt("get") == 1 && sInt("post") == 1);
  env = lerArq(FAROLENVIO_ENVIADOS_ARQ);
  CHECK(env && strstr(env, "8HJ3MX") && strstr(env, "4F7K92"));
  CHECK(strstr(env, "8HJ3MX") < strstr(env, "4F7K92"));    // o mais novo no topo
  free(env);
  // GET 500 no conhecido e melhor esforco: o POST segue.
  CHECK(enviar("/conget500", NULL, &i) == FAROLENVIO_OK && !strcmp(i.codigo, "4F7K92"));
  stats("/conget500");
  CHECK(sInt("get") == 1 && sInt("post") == 1);
}

static void t_erros(void) {
  FarolEnvioInfo i;
  CHECK(enviar("/r429", NULL, &i) == FAROLENVIO_FALHOU);
  CHECK(i.motivo == FAROLENVIO_M_LIMITE && i.http == 429);
  CHECK(!strcmp(i.codigo, "4F7K92") && !strcmp(i.codigoExibir, "NV-4F7K-92") && i.codigoNovo == 0);
  CHECK(lerInt(&ultimoHttp) == 429);
  CHECK(i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) > 0);
  CHECK(enviar("/r422", NULL, &i) == FAROLENVIO_FALHOU && i.motivo == FAROLENVIO_M_RECUSADO && i.http == 422);
  CHECK(!i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) < 0);
  CHECK(enviar("/r500", NULL, &i) == FAROLENVIO_FALHOU && i.motivo == FAROLENVIO_M_SERVIDOR && i.http == 503);
  CHECK(i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) > 0);
  CHECK(enviar("/semcod", NULL, &i) == FAROLENVIO_FALHOU && i.motivo == FAROLENVIO_M_SERVIDOR && i.http == 200);
  CHECK(!strcmp(i.codigo, "4F7K92"));
  CHECK(i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) > 0);
  CHECK(farolenvio_descartar_pendente() == 1);
  esperar(&i, 5000);
  CHECK(!i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) < 0);
}

static void t_log_opt_in(void) {
  FarolEnvioOpcoes o;
  FarolEnvioInfo i;
  farolenvio_opcoes_padrao(&o);
  o.incluir_log = 1;
  o.res_sintoma = RES_S_SOM;
  CHECK(enviar("/log", &o, &i) == FAROLENVIO_OK && i.incluirLog == 1);
  CHECK(lerInt(&logPedidos) == 0);              // so no passo (fio da UI)
  farolenvio_passo();
  CHECK(lerInt(&logPedidos) == 1);
  farolenvio_passo();
  CHECK(lerInt(&logPedidos) == 1);
  // Falhou: nada de log.
  CHECK(enviar("/r500", &o, &i) == FAROLENVIO_FALHOU);
  farolenvio_passo();
  CHECK(lerInt(&logPedidos) == 1);
}

static void t_prazo_e_cancelar(void) {
  FarolEnvioInfo i;
  FarolPacoteEntrada in;
  char cod[48];
  long long t0;
  farolenvio_teste_prazos(1500, 400);
  t0 = msAgora();
  CHECK(enviar("/lento", NULL, &i) == FAROLENVIO_PENDENTE);
  CHECK(i.motivo == FAROLENVIO_M_PRAZO && i.http == 0);
  CHECK(msAgora() - t0 < 4000);
  CHECK(i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) > 0);
  CHECK(lerInt(&ultimoHttp) == 0);
  CHECK(farolenvio_descartar_pendente() == 1);
  esperar(&i, 5000);
  CHECK(!i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) < 0);

  // Cancelar: encerra rapido, sem pendente; um segundo pedido em voo e recusado.
  farolenvio_teste_prazos(20000, 15000);
  usar("/lento");
  entrada(&in, cod);
  CHECK(farol_enviar(&in, NULL) == 1);
  CHECK(farol_enviar(&in, NULL) == 0);
  CHECK(farolenvio_info(&i) == FAROLENVIO_ENVIANDO);
  dormir(150);
  t0 = msAgora();
  farolenvio_cancelar();
  CHECK(esperar(&i, 3000) == FAROLENVIO_CANCELADO && i.motivo == FAROLENVIO_M_CANCELADO);
  CHECK(msAgora() - t0 < 1500);
  CHECK(!i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) < 0);
  farolenvio_teste_prazos(0, 0);
}

static void t_transporte_e_reenvio(void) {
  FarolEnvioInfo i;
  char u[128], *pend;
  long tam;
  snprintf(u, sizeof u, "http://127.0.0.1:%d", portaFechada());
  farolenvio_definir_url(u);
  CHECK(enviar(NULL, NULL, &i) == FAROLENVIO_PENDENTE && i.motivo == FAROLENVIO_M_CONEXAO);
  CHECK(i.temPendente && i.pendenteCriado == relogioFake());
  CHECK(i.pendenteVence == i.pendenteCriado + 24 * 3600);
  tam = tamArq(FAROLENVIO_PENDENTE_ARQ);
  CHECK(tam > 0 && tam <= 35 * 1024);
  pend = lerArq(FAROLENVIO_PENDENTE_ARQ);
  CHECK(pend && !strncmp(pend, "{\"v\":1,\"criado\":", 16) && strstr(pend, "\"pacote\":{\"v\":1"));
  CHECK(!strstr(pend, "tok-farol-18"));          // token nunca vai para o arquivo
  free(pend);
  // Rede "no ar", mas a espera de 60 s ainda nao venceu.
  usar("/re1");
  farolenvio_passo();
  CHECK(!farolenvio_em_voo());
  avancar(61);
  farolenvio_passo();
  CHECK(esperar(&i, 20000) == FAROLENVIO_OK && i.doPendente == 1 && !strcmp(i.codigo, "4F7K92"));
  CHECK(!i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) < 0);
  stats("/re1");
  CHECK(sInt("get") == 1 && sInt("post") == 1 && sInt("fp_ok") == 1 && sInt("v_ok") == 1);
  farolenvio_passo();
  CHECK(!farolenvio_em_voo());
}

static void t_offline(void) {
  FarolEnvioInfo i;
  setOffline(1);
  CHECK(enviar("/off", NULL, &i) == FAROLENVIO_PENDENTE && i.motivo == FAROLENVIO_M_OFFLINE);
  CHECK(i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) > 0);
  stats("/off");
  CHECK(sInt("get") == 0 && sInt("post") == 0);   // sem internet nem tenta
  farolenvio_passo();
  CHECK(!farolenvio_em_voo());
  avancar(120);
  farolenvio_passo();                              // ainda offline: nao tenta
  CHECK(!farolenvio_em_voo());
  setOffline(0);                                   // a rede voltou
  farolenvio_passo();
  CHECK(esperar(&i, 20000) == FAROLENVIO_OK && i.doPendente == 1);
  CHECK(!i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) < 0);
  stats("/off");
  CHECK(sInt("get") == 1 && sInt("post") == 1 && sInt("fp_ok") == 1);

  // Reenvio manual ("Tentar de novo"), sem esperar o passo.
  setOffline(1);
  CHECK(enviar("/man", NULL, &i) == FAROLENVIO_PENDENTE);
  setOffline(0);
  CHECK(farolenvio_retentar() == 1);
  CHECK(esperar(&i, 20000) == FAROLENVIO_OK && i.doPendente == 1 && tamArq(FAROLENVIO_PENDENTE_ARQ) < 0);
  CHECK(farolenvio_retentar() == 0);              // nada pendente

  // 503 no reenvio mantem o pendente; 422 apaga.
  setOffline(1);
  CHECK(enviar("/r500", NULL, &i) == FAROLENVIO_PENDENTE);
  setOffline(0);
  CHECK(farolenvio_retentar() == 1);
  CHECK(esperar(&i, 20000) == FAROLENVIO_FALHOU && i.motivo == FAROLENVIO_M_SERVIDOR);
  CHECK(i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) > 0);
  usar("/r422");
  CHECK(farolenvio_retentar() == 1);
  CHECK(esperar(&i, 20000) == FAROLENVIO_FALHOU && i.motivo == FAROLENVIO_M_RECUSADO);
  CHECK(!i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) < 0);
}

static void t_expira_e_arranque(void) {
  FarolEnvioInfo i;
  long criado;
  setOffline(1);
  CHECK(enviar("/exp", NULL, &i) == FAROLENVIO_PENDENTE);
  criado = i.pendenteCriado;
  // Arranque novo: a memoria some, o arquivo fica e e redescoberto em fio.
  farolenvio_teste_esquecer();
  CHECK(farolenvio_info(&i) == FAROLENVIO_OCIOSO && !i.temPendente);
  farolenvio_iniciar();
  esperar(&i, 5000);
  CHECK(i.temPendente && i.pendenteCriado == criado && i.estado == FAROLENVIO_OCIOSO);
  // 23 h: ainda vale (e offline, nada sai).
  avancar(23 * 3600);
  farolenvio_passo();
  CHECK(!farolenvio_em_voo() && tamArq(FAROLENVIO_PENDENTE_ARQ) > 0);
  // 24 h: o passo pede ao fio que apague, mesmo offline.
  avancar(3600);
  farolenvio_passo();
  CHECK(esperar(&i, 5000) == FAROLENVIO_FALHOU && i.motivo == FAROLENVIO_M_EXPIROU);
  CHECK(!i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) < 0);
  stats("/exp");
  CHECK(sInt("get") == 0 && sInt("post") == 0);

  // Arranque com pendente vencido: apaga calado.
  CHECK(enviar("/exp2", NULL, &i) == FAROLENVIO_PENDENTE);
  farolenvio_teste_esquecer();
  avancar(24 * 3600 + 5);
  farolenvio_iniciar();
  esperar(&i, 5000);
  CHECK(!i.temPendente && i.estado == FAROLENVIO_OCIOSO && tamArq(FAROLENVIO_PENDENTE_ARQ) < 0);

  // Arquivo lixo / grande demais: apagado no arranque.
  gravarArq(FAROLENVIO_PENDENTE_ARQ, "{\"v\":1,\"criado\":\"x\"}");
  farolenvio_iniciar();
  esperar(&i, 5000);
  CHECK(!i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) < 0);
  {
    char *big = (char *)malloc(40000);
    CHECK(big);
    memset(big, ' ', 39999); big[39999] = 0;
    memcpy(big, "{\"v\":1,\"criado\":1800000000,", 27);
    gravarArq(FAROLENVIO_PENDENTE_ARQ, big);
    free(big);
  }
  farolenvio_iniciar();
  esperar(&i, 5000);
  CHECK(!i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) < 0);
  setOffline(0);
}

static void t_farol18b_cancel_barreiras(void) {
  FarolEnvioInfo i;
  FarolPacoteEntrada in;
  char cod[48];

  // Caso 1A: cancelamento na barreira de auth (avisos_registro_cabecalhos).
  usar("/cancel_auth");
  entrada(&in, cod);
  armarBarreira(&barreiraAuthArm, &barreiraAuthDentro, &barreiraAuthLib);
  CHECK(farol_enviar(&in, NULL) == 1);
  esperarBarreira(&barreiraAuthDentro);
  farolenvio_cancelar();
  liberarBarreira(&barreiraAuthLib);
  CHECK(esperar(&i, 5000) == FAROLENVIO_CANCELADO && i.motivo == FAROLENVIO_M_CANCELADO);
  CHECK(!farolenvio_em_voo() && !i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) < 0);
  stats("/cancel_auth");
  CHECK(sInt("get") == 0 && sInt("post") == 0);

  // Caso 1B: cancelamento na barreira de rede_saude_offline().
  usar("/cancel_off");
  entrada(&in, cod);
  setOffline(1);
  armarBarreira(&barreiraOffArm, &barreiraOffDentro, &barreiraOffLib);
  CHECK(farol_enviar(&in, NULL) == 1);
  esperarBarreira(&barreiraOffDentro);
  farolenvio_cancelar();
  liberarBarreira(&barreiraOffLib);
  CHECK(esperar(&i, 5000) == FAROLENVIO_CANCELADO && i.motivo == FAROLENVIO_M_CANCELADO);
  CHECK(!farolenvio_em_voo() && !i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) < 0);
  setOffline(0);
  avancar(120);
  farolenvio_passo();
  CHECK(!farolenvio_em_voo());
  stats("/cancel_off");
  CHECK(sInt("get") == 0 && sInt("post") == 0);
}

static void t_farol18b_503_manual(void) {
  FarolEnvioInfo i;
  FarolEnvioOpcoes op;
  int postAntes, logAntes;

  // Caso 2A: 503 novo guarda para retentativa manual; passo() nao reenvia sozinho.
  stats("/r500");
  postAntes = sInt("post");
  farolenvio_opcoes_padrao(&op);
  op.incluir_log = 1;
  CHECK(enviar("/r500", &op, &i) == FAROLENVIO_FALHOU && i.motivo == FAROLENVIO_M_SERVIDOR && i.http == 503);
  CHECK(i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) > 0 && i.incluirLog == 1);
  avancar(61);
  farolenvio_passo();
  CHECK(!farolenvio_em_voo());
  avancar(1000);
  farolenvio_passo();
  CHECK(!farolenvio_em_voo());
  stats("/r500");
  CHECK(sInt("post") == postAntes + 1);
  // Retentativa manual funciona e preserva incluir_log.
  logAntes = lerInt(&logPedidos);
  usar("/ok_ret503");
  CHECK(farolenvio_retentar() == 1);
  CHECK(esperar(&i, 20000) == FAROLENVIO_OK && i.doPendente == 1 && i.incluirLog == 1);
  CHECK(!i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) < 0);
  farolenvio_passo();
  CHECK(lerInt(&logPedidos) == logAntes + 1);

  // Caso 2B: pendencia offline volta online -> 503 -> passo() apos 61 s nao reenvia sozinho.
  stats("/r500");
  postAntes = sInt("post");
  setOffline(1);
  CHECK(enviar("/r500", NULL, &i) == FAROLENVIO_PENDENTE && i.temPendente);
  setOffline(0);
  farolenvio_passo();
  CHECK(esperar(&i, 20000) == FAROLENVIO_FALHOU && i.motivo == FAROLENVIO_M_SERVIDOR && i.http == 503);
  stats("/r500");
  CHECK(sInt("post") == postAntes + 1);
  avancar(61);
  farolenvio_passo();
  CHECK(!farolenvio_em_voo());
  avancar(1000);
  farolenvio_passo();
  CHECK(!farolenvio_em_voo());
  setOffline(1);
  farolenvio_passo();
  setOffline(0);
  farolenvio_passo();
  CHECK(!farolenvio_em_voo());
  stats("/r500");
  CHECK(sInt("post") == postAntes + 1);
  CHECK(farolenvio_descartar_pendente() == 1);
  esperar(&i, 5000);
  CHECK(!i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) < 0);
}

static void t_farol18b_arranque_codigo_e_log(void) {
  FarolEnvioInfo i;
  FarolEnvioOpcoes op;
  int logAntes;

  // P2: no arranque, carrega ultimo codigo de resolver-enviados.txt para preservar em 429.
  farolenvio_teste_esquecer();
  farolenvio_iniciar();
  esperar(&i, 5000);
  CHECK(!strcmp(i.codigo, "4F7K92") && !strcmp(i.codigoExibir, "NV-4F7K-92") && i.codigoNovo == 0);
  CHECK(enviar("/r429", NULL, &i) == FAROLENVIO_FALHOU && i.motivo == FAROLENVIO_M_LIMITE && i.http == 429);
  CHECK(!strcmp(i.codigo, "4F7K92") && !strcmp(i.codigoExibir, "NV-4F7K-92") && i.codigoNovo == 0);
  CHECK(farolenvio_descartar_pendente() == 1);
  esperar(&i, 5000);

  // P2: incluir_log sobrevive a reinicio em farol-pendente.json e retentativa manual.
  farolenvio_opcoes_padrao(&op);
  op.incluir_log = 1;
  setOffline(1);
  CHECK(enviar("/ok_reinicio", &op, &i) == FAROLENVIO_PENDENTE && i.incluirLog == 1);
  setOffline(0);
  farolenvio_teste_esquecer();
  farolenvio_iniciar();
  esperar(&i, 5000);
  CHECK(i.temPendente && i.incluirLog == 1);
  logAntes = lerInt(&logPedidos);
  usar("/ok_reinicio");
  CHECK(farolenvio_retentar() == 1);
  CHECK(esperar(&i, 20000) == FAROLENVIO_OK && i.doPendente == 1 && i.incluirLog == 1);
  farolenvio_passo();
  CHECK(lerInt(&logPedidos) == logAntes + 1);
}

static void t_farol18c_cancel_relogio_e_gravacao(void) {
  FarolEnvioInfo i;
  FarolPacoteEntrada in;
  char cod[48];

  // Caso P1-1A: barreira em agoraS() no ramo offline -> cancelar e liberar.
  usar("/cancel_relogio");
  entrada(&in, cod);
  setOffline(1);
  armarBarreira(&barreiraRelArm, &barreiraRelDentro, &barreiraRelLib);
  CHECK(farol_enviar(&in, NULL) == 1);
  esperarBarreira(&barreiraRelDentro);
  farolenvio_cancelar();
  liberarBarreira(&barreiraRelLib);
  CHECK(esperar(&i, 5000) == FAROLENVIO_CANCELADO && i.motivo == FAROLENVIO_M_CANCELADO);
  CHECK(!farolenvio_em_voo() && !i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) < 0);
  setOffline(0);
  avancar(61);
  farolenvio_passo();
  CHECK(!farolenvio_em_voo());
  stats("/cancel_relogio");
  CHECK(sInt("get") == 0 && sInt("post") == 0);

  // Caso P1-1B: cancelamento durante pendenteGravar() desfaz o arquivo gravado.
  usar("/cancel_grav");
  entrada(&in, cod);
  setOffline(1);
  armarBarreira(&barreiraGravArm, &barreiraGravDentro, &barreiraGravLib);
  CHECK(farol_enviar(&in, NULL) == 1);
  esperarBarreira(&barreiraGravDentro);
  farolenvio_cancelar();
  liberarBarreira(&barreiraGravLib);
  CHECK(esperar(&i, 5000) == FAROLENVIO_CANCELADO && i.motivo == FAROLENVIO_M_CANCELADO);
  CHECK(!farolenvio_em_voo() && !i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) < 0);
  setOffline(0);
  avancar(61);
  farolenvio_passo();
  CHECK(!farolenvio_em_voo());
  stats("/cancel_grav");
  CHECK(sInt("get") == 0 && sInt("post") == 0);
}

static void t_farol18c_ok_outro_preserva_pendente(void) {
  FarolEnvioInfo i;
  FarolPacoteEntrada in;
  char cod[48], fpA[32], tmpDir[720];

  // Caso P1-2: A salvo offline (ums_204); online envia B (ums_205) com HTTP 200;
  // apos 61 s, farolenvio_passo() reenvia A automaticamente e remove farol-pendente.json.
  setOffline(1);
  usar("/p18c_a");
  entrada(&in, cod);
  strcpy(in.erro.cod, "ums_204");
  CHECK(farol_enviar(&in, NULL) == 1);
  CHECK(esperar(&i, 5000) == FAROLENVIO_PENDENTE && i.motivo == FAROLENVIO_M_OFFLINE);
  CHECK(i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) > 0);
  snprintf(fpA, sizeof fpA, "%s", i.fp);

  setOffline(0);
  usar("/p18c_b");
  entrada(&in, cod);
  in.erro.nativo = 205;
  strcpy(in.erro.cod, "ums_205");
  CHECK(farol_enviar(&in, NULL) == 1);
  CHECK(esperar(&i, 20000) == FAROLENVIO_OK && i.http == 200 && !i.doPendente);
  CHECK(strcmp(i.fp, fpA) != 0);
  CHECK(i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) > 0);
  stats("/p18c_b");
  CHECK(sInt("get") == 1 && sInt("post") == 1);

  usar("/p18c_a");
  avancar(61);
  farolenvio_passo();
  CHECK(esperar(&i, 20000) == FAROLENVIO_OK && i.doPendente == 1 && !strcmp(i.fp, fpA));
  CHECK(!i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) < 0);
  stats("/p18c_a");
  CHECK(sInt("get") == 1 && sInt("post") == 1 && !strcmp(sStr("fp_body"), fpA));

  // Cancelar envio B antes de pendenteGravar nao apaga pendencia A pre-existente de outro fp.
  setOffline(1);
  usar("/p18c_a");
  entrada(&in, cod);
  strcpy(in.erro.cod, "ums_204");
  CHECK(farol_enviar(&in, NULL) == 1);
  CHECK(esperar(&i, 5000) == FAROLENVIO_PENDENTE && i.temPendente);

  usar("/p18c_b");
  entrada(&in, cod);
  in.erro.nativo = 205;
  strcpy(in.erro.cod, "ums_205");
  armarBarreira(&barreiraRelArm, &barreiraRelDentro, &barreiraRelLib);
  CHECK(farol_enviar(&in, NULL) == 1);
  esperarBarreira(&barreiraRelDentro);
  farolenvio_cancelar();
  liberarBarreira(&barreiraRelLib);
  CHECK(esperar(&i, 5000) == FAROLENVIO_CANCELADO && i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) > 0);

  setOffline(0);
  avancar(61);
  farolenvio_passo();
  CHECK(esperar(&i, 20000) == FAROLENVIO_OK && i.doPendente == 1 && !strcmp(i.fp, fpA));
  CHECK(!i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) < 0);

  // Caso P2: se regravacao para auto=0 falhar (.tmp obstruido) apos 503 em reenvio,
  // remove farol-pendente.json para nao reenviar sozinho apos reinicio.
  setOffline(1);
  CHECK(enviar("/r500", NULL, &i) == FAROLENVIO_PENDENTE && i.temPendente);
  setOffline(0);
  caminho(tmpDir, sizeof tmpDir, FAROLENVIO_PENDENTE_ARQ ".tmp");
  CHECK(mkdir(tmpDir, 0700) == 0);
  farolenvio_passo();
  CHECK(esperar(&i, 20000) == FAROLENVIO_FALHOU && i.motivo == FAROLENVIO_M_SERVIDOR && i.http == 503);
  CHECK(rmdir(tmpDir) == 0);
  CHECK(!i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) < 0);
  farolenvio_teste_esquecer();
  farolenvio_iniciar();
  esperar(&i, 5000);
  CHECK(!i.temPendente);
}

static void t_farol18d_cancel_restaura_e_desarma(void) {
  FarolEnvioInfo i;
  FarolPacoteEntrada in;
  char cod[48], fpA[32], *pend;

  // Caso P1-1: pendencia A offline ("auto":1) -> enviar B offline -> barreira em
  // dados_gravar() apos rename -> cancelar -> restaura A em disco e memoria;
  // ao voltar online + 61 s, passo() reenvia A e NAO envia B.
  setOffline(1);
  usar("/p18d_p1a");
  entrada(&in, cod);
  strcpy(in.erro.cod, "ums_204");
  CHECK(farol_enviar(&in, NULL) == 1);
  CHECK(esperar(&i, 5000) == FAROLENVIO_PENDENTE && i.motivo == FAROLENVIO_M_OFFLINE);
  CHECK(i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) > 0);
  snprintf(fpA, sizeof fpA, "%s", i.fp);

  usar("/p18d_p1b");
  entrada(&in, cod);
  in.erro.nativo = 205;
  strcpy(in.erro.cod, "ums_205");
  armarBarreira(&barreiraGravArm, &barreiraGravDentro, &barreiraGravLib);
  CHECK(farol_enviar(&in, NULL) == 1);
  esperarBarreira(&barreiraGravDentro);
  farolenvio_cancelar();
  liberarBarreira(&barreiraGravLib);
  CHECK(esperar(&i, 5000) == FAROLENVIO_CANCELADO && i.motivo == FAROLENVIO_M_CANCELADO);
  CHECK(!farolenvio_em_voo() && i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) > 0);
  pend = lerArq(FAROLENVIO_PENDENTE_ARQ);
  CHECK(pend && strstr(pend, fpA) && strstr(pend, "\"auto\":1") && !strstr(pend, "ums_205"));
  free(pend);

  usar("/p18d_p1a");
  setOffline(0);
  avancar(61);
  farolenvio_passo();
  CHECK(esperar(&i, 20000) == FAROLENVIO_OK && i.doPendente == 1 && !strcmp(i.fp, fpA));
  CHECK(!i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) < 0);
  stats("/p18d_p1a");
  CHECK(sInt("get") == 1 && sInt("post") == 1 && !strcmp(sStr("fp_body"), fpA));
  stats("/p18d_p1b");
  CHECK(sInt("get") == 0 && sInt("post") == 0);

  // Caso P1-2: pendencia A offline ("auto":1) -> online -> farolenvio_retentar() ->
  // barreira na auth -> cancelar -> CANCELADO, zero GET/POST, arquivo intacto,
  // e apos avancar 61 s chamando passo() continua zero GET/POST.
  setOffline(1);
  usar("/p18d_p2");
  entrada(&in, cod);
  strcpy(in.erro.cod, "ums_204");
  CHECK(farol_enviar(&in, NULL) == 1);
  CHECK(esperar(&i, 5000) == FAROLENVIO_PENDENTE && i.motivo == FAROLENVIO_M_OFFLINE);
  CHECK(i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) > 0);

  setOffline(0);
  armarBarreira(&barreiraAuthArm, &barreiraAuthDentro, &barreiraAuthLib);
  CHECK(farolenvio_retentar() == 1);
  esperarBarreira(&barreiraAuthDentro);
  farolenvio_cancelar();
  liberarBarreira(&barreiraAuthLib);
  CHECK(esperar(&i, 5000) == FAROLENVIO_CANCELADO && i.motivo == FAROLENVIO_M_CANCELADO);
  CHECK(!farolenvio_em_voo() && i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) > 0);
  stats("/p18d_p2");
  CHECK(sInt("get") == 0 && sInt("post") == 0);

  avancar(61);
  farolenvio_passo();
  CHECK(!farolenvio_em_voo());
  stats("/p18d_p2");
  CHECK(sInt("get") == 0 && sInt("post") == 0);

  // Retentativa manual posterior continua funcionando e envia A.
  CHECK(farolenvio_retentar() == 1);
  CHECK(esperar(&i, 20000) == FAROLENVIO_OK && i.doPendente == 1);
  CHECK(!i.temPendente && tamArq(FAROLENVIO_PENDENTE_ARQ) < 0);
  stats("/p18d_p2");
  CHECK(sInt("get") == 1 && sInt("post") == 1);
}

static int modoWorker(const char *fase) {
  FarolEnvioInfo i;
  farolenvio_teste_relogio(NULL);
  if (!strcmp(fase, "fase1")) {
    CHECK(enviar("", NULL, &i) == FAROLENVIO_OK);
    CHECK(strlen(i.codigo) == 6 && i.http == 200);
    printf("fp=%s codigo=%s\n", i.fp, i.codigo);
  } else {
    CHECK(enviar("", NULL, &i) == FAROLENVIO_JA_CORRIGIDO);
    CHECK(!strcmp(i.conhecidoEstado, "corrigido") && !strcmp(i.conhecidoVersao, "99.0.0"));
    CHECK(!strcmp(i.notaPt, "Corrigido no 99"));
    printf("estado=corrigido fp=%s\n", i.fp);
  }
  farolenvio_encerrar();
  return 0;
}

int main(int argc, char **argv) {
  int worker = argc >= 2 && !strcmp(argv[1], "--worker");
  if (argc < 3 + worker) { fprintf(stderr, "uso: %s [--worker] <base> <dir> [fase]\n", argv[0]); return 2; }
  fioPrincipal = pthread_self();
  snprintf(baseUrl, sizeof baseUrl, "%s", argv[1 + worker]);
  snprintf(dir, sizeof dir, "%s", argv[2 + worker]);
  farolanel_relogio(anelRelogio);
  farolenvio_teste_relogio(relogioFake);
  if (worker) { semeiaAnel(20); return modoWorker(argc > 4 ? argv[4] : "fase1"); }

  t_basico();
  t_sem_servidor_e_sem_conta();
  t_ok();
  t_corrigido();
  t_conhecido_segue();
  t_erros();
  t_log_opt_in();
  t_prazo_e_cancelar();
  t_transporte_e_reenvio();
  t_offline();
  t_expira_e_arranque();
  t_farol18b_cancel_barreiras();
  t_farol18b_503_manual();
  t_farol18b_arranque_codigo_e_log();
  t_farol18c_cancel_relogio_e_gravacao();
  t_farol18c_ok_outro_preserva_pendente();
  t_farol18d_cancel_restaura_e_desarma();
  farolenvio_encerrar();
  CHECK(lerInt(&dadosNoPrincipal) == 0);          // zero I/O de arquivo no fio da UI
  printf("farol_envio: cenarios locais ok\n");
  return 0;
}
