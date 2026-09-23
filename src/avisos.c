#include "avisos.h"
#include "dados.h"
#include "rede.h"
#include "js.h"
#include "gfx.h"
#include "botoes.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "idioma.h"
#include "ajustes.h"
#include "recomenda.h"
#include "atualizacao.h"
#include "salvosintro.h"
#include "registro.h"
#include <math.h>
#include "agenda.h"
#include "sessao.h"
#include "trakt.h"
#include "marco.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef NV_VERSAO
#define NV_VERSAO "dev"
#endif
#ifndef NV_REC_URL
#define NV_REC_URL ""
#endif

// O canal do dono: um arquivo no proprio repositorio. Sem servidor novo e sem
// segredo: qualquer um pode ler, e e essa a intencao de um aviso.
// -DAV_CANAL_HOST=192.168.1.181:8793 na compilacao aponta para outro canal
// (http://<host>/avisos.json): e como se olha um aviso antes de publica-lo para
// todo mundo — previa no Mac ou na TV do dono. Host e nao URL porque a URL
// inteira nao atravessa o docker do tools/arm.sh (as aspas nao chegam) e um
// "//" num -D vira comentario para o pre-processador.
#define NV_STR2(x) #x
#define NV_STR(x) NV_STR2(x)
#ifdef AV_CANAL_HOST
#define AV_CANAL_URL "http://" NV_STR(AV_CANAL_HOST) "/avisos.json"
#else
#define AV_CANAL_URL "https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/master/avisos.json"
#endif
#define AV_CANAL_INTERVALO_MS (30u * 60u * 1000u)
#define AV_MAX        40
#define AV_VISTOS_ARQ "avisos-vistos.txt"
#define AV_MARCA_ARQ  "sessao-viva.txt"
#define AV_LOG_ANTERIOR "/tmp/nuvio-anterior.log"
#define AV_REGISTRO_MAX (200 * 1024)
// 20 s, nao 6 (dono, 20/09/2026: "teria que ficar mais tempo"). Quem esta
// olhando um card do outro lado da tela leva um tempo para notar o canto.
#define AV_TOAST_MS   20000.0f

enum { AV_REC, AV_AGENDA, AV_UPDATE, AV_CANAL, AV_CRASH };
typedef struct {
  char id[72];
  int  tipo;
  char titulo[160];
  char texto[420];
  char alvo[24];        // imdb (agenda) ou versao (update)
  int  visto;
} Aviso;

static Aviso itens[AV_MAX];
static int   n;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

// Ids ja vistos, um por linha em avisos-vistos.txt. Um id que ja foi visto nao
// dispara toast de novo — e o que impede o aviso do dono de aparecer a cada
// arranque durante as duas semanas em que ele esta no ar.
static char vistos[120][72];
static int  nVistos, vistosLidos;

// Painel e toast.
static int   aberto, foco;
static float entrada, rol;
static float toastAte;          // SDL_GetTicks em que o toast some; 0 = sem toast
static int   toastN;
static float toastA;
static int   vistosSujos;

// Acoes entregues a app.c.
static char pediuAbrir[24];
static int  pediuCodigo;

// Crash da sessao anterior e envio do registro.
static int   crashDetectado;
static char  crashQuando[40];
#ifdef __EMSCRIPTEN__
// O LOG DA SESSAO ANTERIOR NO TIZEN. Nao ha arquivo: tools/tizen-shell.html
// guarda as linhas do Module.print em localStorage (nv-log) e, no arranque
// seguinte, gira para nv-log-anterior. E lido AQUI, no fio principal (o
// unico com localStorage), em avisos_iniciar; enviarRegistro roda num
// pthread e so pode usar o que ja esta na memoria. Ate a 1.3.3 o crash da
// Samsung chegava com 0 bytes de texto — os 5 de #72 nao disseram nada.
static char *logAnterior;
#include <emscripten.h>
static void lerLogAnterior(void) {
  char *js = (char *)EM_ASM_PTR({
    try {
      var t = localStorage.getItem('nv-log-anterior') || '';
      if (t.length > $0) t = t.slice(t.length - $0);
      // TextEncoder e nao stringToUTF8: os helpers do runtime nao estao
      // exportados neste build (so PThread), e com ASSERTIONS chamar um
      // deles aborta.
      var b = new TextEncoder().encode(t);
      var p = _malloc(b.length + 1);
      if (!p) return 0;
      HEAPU8.set(b, p);
      HEAPU8[p + b.length] = 0;
      return p;
    } catch (e) { return 0; }
  }, AV_REGISTRO_MAX);
  logAnterior = js;
  if (logAnterior && !logAnterior[0]) { free(logAnterior); logAnterior = NULL; }
  printf("[avisos] log da sessao anterior: %u bytes\n",
         logAnterior ? (unsigned)strlen(logAnterior) : 0u);
}
#endif
static int   envioEstado;       // 0 nada, 1 enviando, 2 ok, 3 falhou
static pthread_t fioEnvio;

// O CARTAO DO CRASH, na reabertura: uma pergunta, dois botoes. Abre uma vez
// por crash (a marca e o id do item, gravada em vistos ao fechar) quando a
// home esta de pe e nenhum outro cartao esta na frente, como agendaviso.c.
static int   cartao;            // 1 = aberto
static int   cartaoFoco;        // 0 = Enviar, 1 = Agora nao
static float cartaoA;
static int   cartaoPendente;    // ha crash nao perguntado nesta sessao
static char  cartaoId[72];

// Canal do dono.
static pthread_t fioCanal;
static int   canalVivo;
static Uint32 canalProximo;
static char  canalEtag[80];

// --- vistos ---------------------------------------------------------------------
static void vistosLer(void) {
  char *t, *p;
  if (vistosLidos) return;
  vistosLidos = 1;
  t = dados_ler(AV_VISTOS_ARQ);
  if (!t) return;
  p = t;
  while (*p && nVistos < 120) {
    char *fim = strchr(p, '\n');
    size_t len = fim ? (size_t)(fim - p) : strlen(p);
    if (len > 0 && len < sizeof vistos[0]) { memcpy(vistos[nVistos], p, len); vistos[nVistos][len] = 0; nVistos++; }
    if (!fim) break;
    p = fim + 1;
  }
  free(t);
}
static int foiVisto(const char *id) {
  int i;
  for (i = 0; i < nVistos; i++) if (!strcmp(vistos[i], id)) return 1;
  return 0;
}
static void marcarVisto(const char *id) {
  if (foiVisto(id)) return;
  if (nVistos >= 120) { memmove(vistos[0], vistos[1], sizeof vistos[0] * 119); nVistos = 119; }
  snprintf(vistos[nVistos++], sizeof vistos[0], "%s", id);
  vistosSujos = 1;
}
static void vistosGravar(void) {
  char buf[120 * 72 + 8];
  size_t u = 0;
  int i;
  if (!vistosSujos) return;
  vistosSujos = 0;
  buf[0] = 0;
  for (i = 0; i < nVistos && u + 74 < sizeof buf; i++)
    u += (size_t)snprintf(buf + u, sizeof buf - u, "%s\n", vistos[i]);
  dados_gravar(AV_VISTOS_ARQ, buf);
}

// --- itens ------------------------------------------------------------------------
// Mantem um item por id, atualizando titulo/texto quando ja existe. Devolve 1
// se e NOVO (para o toast).
static int por(const char *id, int tipo, const char *titulo, const char *texto, const char *alvo) {
  int i;
  for (i = 0; i < n; i++) if (!strcmp(itens[i].id, id)) {
    snprintf(itens[i].titulo, sizeof itens[i].titulo, "%s", titulo);
    snprintf(itens[i].texto, sizeof itens[i].texto, "%s", texto);
    return 0;
  }
  if (n >= AV_MAX) { memmove(&itens[0], &itens[1], sizeof itens[0] * (AV_MAX - 1)); n = AV_MAX - 1; }
  memset(&itens[n], 0, sizeof itens[n]);
  snprintf(itens[n].id, sizeof itens[n].id, "%s", id);
  itens[n].tipo = tipo;
  snprintf(itens[n].titulo, sizeof itens[n].titulo, "%s", titulo);
  snprintf(itens[n].texto, sizeof itens[n].texto, "%s", texto);
  if (alvo) snprintf(itens[n].alvo, sizeof itens[n].alvo, "%s", alvo);
  itens[n].visto = foiVisto(id);
  n++;
  return !itens[n - 1].visto;
}
static void tirar(const char *id) {
  int i;
  for (i = 0; i < n; i++) if (!strcmp(itens[i].id, id)) {
    memmove(&itens[i], &itens[i + 1], sizeof itens[0] * (size_t)(n - i - 1));
    n--; return;
  }
}
// O RELOGIO DO TOAST COMECA NO PRIMEIRO QUADRO EM QUE ELE PODE SER VISTO, e
// nao quando o aviso chega: o do crash nasce no arranque, com a tela de perfil
// na frente, e com o relogio correndo dali ele ja tinha expirado quando a home
// aparecia (medido na previa do Mac: 32 s de arranque, toast de 6 s).
static int toastPendente;
static int demoAviso(const char *id, int tipo, const char *t, const char *x, const char *alvo) { return por(id, tipo, t, x, alvo); }
static void toast(int novos) {
  if (novos <= 0) return;
  toastN += novos;
  toastPendente = 1;
}

int avisos_n_novos(void) {
  int i, k = 0;
  for (i = 0; i < n; i++) if (!itens[i].visto) k++;
  return k;
}

// --- crash e registro ---------------------------------------------------------
// A MARCA TEM DUAS LINHAS. A primeira ("1.4.1 2026-09-22 18:40") e a de
// sempre: existir na abertura seguinte quer dizer que a sessao nao passou por
// avisos_encerrar. A segunda e o ULTIMO SINAL DE VIDA — evento de janela,
// segundos de sessao e rss — e existe porque "nao se despediu" sozinho nao
// separava nada: 17 registros de 6 pessoas na 1.4.0 e nenhuma linha dizendo se
// a TV matou o app em segundo plano (ultimo=oculto), se ele caiu no meio do uso
// (ultimo=vivo com rss alto) ou se a TV foi desligada. O relatorio 22b pedia
// exatamente isto.
static char marcaCab[64];
static char marcaSinal[96];
static void marcaEscrever(int leve) {
  char buf[180];
  snprintf(buf, sizeof buf, "%s\n%s\n", marcaCab, marcaSinal);
  if (leve) dados_gravar_leve(AV_MARCA_ARQ, buf);
  else      dados_gravar(AV_MARCA_ARQ, buf);
}
static void marcaGravar(void) {
  time_t t = time(NULL);
  struct tm tmv;
  localtime_r(&t, &tmv);
  snprintf(marcaCab, sizeof marcaCab, "%s %04d-%02d-%02d %02d:%02d", NV_VERSAO,
           tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday, tmv.tm_hour, tmv.tm_min);
  snprintf(marcaSinal, sizeof marcaSinal, "ultimo=arranque t=0s");
  marcaEscrever(0);
}
// `evento` NULL = batida periodica: so reescreve a cada 60 s e pela gravacao
// LEVE (no Tizen, o relogio de 15 s do IDBFS em vez do de 700 ms). Evento de
// janela grava na hora e pela gravacao normal: e o sinal que mais importa e o
// que tem menos tempo para chegar ao disco antes de a TV matar o processo.
void avisos_sinal(const char *evento, float rssMb) {
  static Uint32 ultBatida;
  Uint32 agora = SDL_GetTicks();
  if (!marcaCab[0]) return;                    // antes de avisos_iniciar
  if (!evento) {
    if (ultBatida && agora - ultBatida < 60000) return;
    ultBatida = agora;
  }
  snprintf(marcaSinal, sizeof marcaSinal, "ultimo=%s t=%us rss=%.0fMB",
           evento ? evento : "vivo", (unsigned)(agora / 1000), rssMb);
  marcaEscrever(evento == NULL);
}

static int idHead(const char **cab, char *aut, size_t nAut, char *via, size_t nVia, char *chave, size_t nChave) {
  const char *tcab[4];
  if (trakt_ativo() && trakt_cabecalhos(tcab, aut, nAut, chave, nChave)) {
    snprintf(via, nVia, "X-Nuvio-Auth: trakt");
  } else if (sessao_token()[0]) {
    snprintf(aut, nAut, "Authorization: Bearer %s", sessao_token());
    snprintf(via, nVia, "X-Nuvio-Auth: nuvio");
  } else return 0;
  cab[0] = aut; cab[1] = via; cab[2] = "Content-Type: application/json"; cab[3] = NULL;
  return 1;
}

static void jsonEsc(char *dst, size_t tam, const char *s) {
  size_t u = 0;
  for (; *s && u + 7 < tam; s++) {
    unsigned char c = (unsigned char)*s;
    if (c == '"' || c == '\\') { dst[u++] = '\\'; dst[u++] = (char)c; }
    else if (c == '\n') { dst[u++] = '\\'; dst[u++] = 'n'; }
    else if (c == '\r') continue;
    else if (c < 0x20) { u += (size_t)snprintf(dst + u, tam - u, "\\u%04x", c); }
    else dst[u++] = (char)c;
  }
  dst[u] = 0;
}

static int extrairRegistroId(const char *json, char *dst, unsigned tam) {
  const char *p, *q;
  size_t n;
  if (!dst || tam < 2) return 0;
  dst[0] = 0;
  if (!json) return 0;
  p = strstr(json, "registro_id");
  if (!p) p = strstr(json, "registroId");
  if (!p) p = strstr(json, "\\\"id\\\"");
  if (!p) return 0;
  p = strchr(p, ':');
  if (!p) return 0;
  p++;
  while (*p == ' ' || *p == '\t' || *p == '"') p++;
  q = p;
  while (*q && *q != '"' && *q != ',' && *q != '}' &&
         (unsigned char)*q > 0x20) q++;
  n = (size_t)(q - p);
  if (!n || n >= tam) return 0;
  // `"registro_id": null` NAO e recibo: e o Worker dizendo que nao gravou. Sem
  // isto o "null" era copiado como id e o diagnostico mostrava "enviado".
  if (n == 4 && !strncmp(p, "null", 4)) return 0;
  memcpy(dst, p, n);
  dst[n] = 0;
  return 1;
}

int avisos_enviar_diagnostico(const char *execucao_id, const char *relatorio,
                              char *registro_id, unsigned tam_registro_id) {
  char aut[2200], via[40], chave[160], *esc = NULL, *corpo = NULL, *resp = NULL;
  const char *cab[5];
  char url[300];
  int status = 0, ok = 0;
  size_t n;
  if (registro_id && tam_registro_id) registro_id[0] = 0;
  if (!execucao_id || !*execucao_id || !relatorio || !NV_REC_URL[0]) return 0;
  if (!idHead(cab, aut, sizeof aut, via, sizeof via, chave, sizeof chave)) return 0;
  n = strlen(relatorio);
  esc = malloc(n * 2 + 8);
  corpo = malloc(n * 2 + 640);
  if (!esc || !corpo) goto fim;
  jsonEsc(esc, n * 2 + 8, relatorio);
  snprintf(corpo, n * 2 + 640,
           "{\"versao\":\"%s\",\"plataforma\":\"%s\",\"execucao_id\":\"%s\",\"texto\":\"%s\"}",
           NV_VERSAO,
#ifdef __EMSCRIPTEN__
           "tizen",
#elif defined(__APPLE__)
           "mac",
#else
           "webos",
#endif
           execucao_id, esc);
  snprintf(url, sizeof url, "%s/v1/registro", NV_REC_URL);
  resp = rede_postar_st(url, 30, cab, corpo, &status);
  if (status >= 200 && status < 300 && resp &&
      extrairRegistroId(resp, registro_id, tam_registro_id)) ok = 1;
  printf("[diagnostico] envio %s: HTTP %d%s\n", ok ? "confirmado" : "falhou", status,
         ok ? "" : " (sem recibo desta execucao)");
  fflush(stdout);
fim:
  free(resp);
  free(corpo);
  free(esc);
  return ok;
}

// ENVIO MANUAL, pelos Ajustes (dono, 20/09/2026): o mesmo caminho do crash,
// com o log DESTA sessao. `u` == &ATUAL escolhe a origem. No Tizen o log
// atual e lido do localStorage no fio principal antes de o fio de envio
// nascer (avisos_enviar_registro_atual); no LG o arquivo e o de sempre.
static const int ATUAL = 1;
// ENVIO AUTOMATICO (ajuste "Enviar registros sozinho", 20/09/2026): os mesmos
// dois envios, sem botao. Nao mexem em envioEstado ao terminar, para a linha
// "Enviar registro" dos Ajustes nao dizer "enviado" por algo que a pessoa
// nao apertou.
static const int AUTO_ATUAL = 2, AUTO_ANTERIOR = 3;
static const char *agoraTexto(void) {
  static char buf[40];
  time_t t = time(NULL);
  struct tm tmv;
  localtime_r(&t, &tmv);
  snprintf(buf, sizeof buf, "%04d-%02d-%02d %02d:%02d (manual)", tmv.tm_year + 1900, tmv.tm_mon + 1,
           tmv.tm_mday, tmv.tm_hour, tmv.tm_min);
  return buf;
}
static const char *agoraTextoAuto(const char *marca) {
  static char buf[48];
  time_t t = time(NULL);
  struct tm tmv;
#ifdef _WIN32
  localtime_s(&tmv, &t);
#else
  localtime_r(&t, &tmv);
#endif
  snprintf(buf, sizeof buf, "%04d-%02d-%02d %02d:%02d (%s)", tmv.tm_year + 1900,
           tmv.tm_mon + 1, tmv.tm_mday, tmv.tm_hour, tmv.tm_min, marca);
  return buf;
}
#ifdef __EMSCRIPTEN__
static char *logAtual;
#endif
static void *enviarRegistro(void *u) {
  static char aut[2200], via[40], chave[160];
  const char *cab[5];
  char *texto = NULL, *corpo, *resp;
  size_t nTexto = 0;
  int status = 0;
  int manual = (u == &ATUAL || u == &AUTO_ATUAL);
  int automatico = (u == &AUTO_ATUAL || u == &AUTO_ANTERIOR);
#ifdef __EMSCRIPTEN__
  { const char *fonte = manual ? logAtual : logAnterior;
    if (fonte) { texto = strdup(fonte); if (texto) nTexto = strlen(texto); } }
#else
  { const char *arq = manual ? registro_arquivo() : AV_LOG_ANTERIOR;
    FILE *f = arq ? fopen(arq, "rb") : NULL;
    if (f) {
      long tam;
      fseek(f, 0, SEEK_END); tam = ftell(f);
      if (tam > AV_REGISTRO_MAX) fseek(f, tam - AV_REGISTRO_MAX, SEEK_SET); else rewind(f);
      texto = malloc(AV_REGISTRO_MAX + 1);
      if (texto) { nTexto = fread(texto, 1, AV_REGISTRO_MAX, f); texto[nTexto] = 0; }
      fclose(f);
    } }
#endif
  if (!idHead(cab, aut, sizeof aut, via, sizeof via, chave, sizeof chave)) {
    free(texto); envioEstado = 3; return NULL;
  }
  corpo = malloc(nTexto * 2 + 512);
  if (!corpo) { free(texto); envioEstado = 3; return NULL; }
  { char *esc = malloc(nTexto * 2 + 8);
    if (!esc) { free(corpo); free(texto); envioEstado = 3; return NULL; }
    jsonEsc(esc, nTexto * 2 + 8, texto ? texto : "");
    snprintf(corpo, nTexto * 2 + 512,
             "{\"versao\":\"%s\",\"plataforma\":\"%s\",\"quando\":\"%s\",\"texto\":\"%s\"}",
             NV_VERSAO,
#ifdef __EMSCRIPTEN__
             "tizen",
#elif defined(__APPLE__)
             "mac",
#else
             "webos",
#endif
             u == &AUTO_ATUAL ? agoraTextoAuto("auto") :
             u == &AUTO_ANTERIOR ? agoraTextoAuto("anterior") :
             manual ? agoraTexto() : crashQuando, esc);
    free(esc); }
  free(texto);
  { char url[300];
    snprintf(url, sizeof url, "%s/v1/registro", NV_REC_URL);
    resp = rede_postar_st(url, 30, cab, corpo, &status); }
  free(corpo);
  free(resp);
  envioEstado = automatico ? 0 : (status >= 200 && status < 300) ? 2 : 3;
  printf("[avisos] registro enviado%s: HTTP %d\n", automatico ? " (sozinho)" : "", status);
  fflush(stdout);
  return NULL;
}

// --- canal do dono ------------------------------------------------------------
static int versaoMaior(const char *a, const char *b) {   // a > b ?
  int ma = 0, na = 0, pa = 0, mb = 0, nb = 0, pb = 0;
  sscanf(a, "%d.%d.%d", &ma, &na, &pa);
  sscanf(b, "%d.%d.%d", &mb, &nb, &pb);
  if (ma != mb) return ma > mb;
  if (na != nb) return na > nb;
  return pa > pb;
}
static void hojeIso(char *dst, size_t tam) {
  time_t t = time(NULL);
  struct tm tmv;
  localtime_r(&t, &tmv);
  snprintf(dst, tam, "%04d-%02d-%02d", tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);
}
static void *fioCanalFn(void *u) {
  const char *cab[3];
  char cabEtag[100];
  char *corpo;
  (void)u;
  cab[0] = NULL;
  if (canalEtag[0]) { snprintf(cabEtag, sizeof cabEtag, "If-None-Match: %s", canalEtag); cab[0] = cabEtag; cab[1] = NULL; }
  corpo = rede_baixar_com(AV_CANAL_URL, 15, cab);
  if (corpo && strchr(corpo, '[')) {
    const char *p = js_raiz_array(corpo);
    char hoje[12];
    int novos = 0;
    hojeIso(hoje, sizeof hoje);
    pthread_mutex_lock(&trava);
    while (p) {
      const char *f = js_fim(p);
      char id[72] = "", desde[12] = "", ate[12] = "", plat[12] = "", ateV[16] = "";
      char tit[160] = "", titEn[160] = "", txt[420] = "", txtEn[420] = "";
      int ingles = ajustes_idioma_ingles(), ok = 1;
      js_texto(p, f, "id", id, sizeof id);
      js_texto(p, f, "desde", desde, sizeof desde);
      js_texto(p, f, "ate", ate, sizeof ate);
      js_texto(p, f, "plataforma", plat, sizeof plat);
      js_texto(p, f, "ate_versao", ateV, sizeof ateV);
      js_texto(p, f, "titulo", tit, sizeof tit);
      js_texto(p, f, "titulo_en", titEn, sizeof titEn);
      js_texto(p, f, "texto", txt, sizeof txt);
      js_texto(p, f, "texto_en", txtEn, sizeof txtEn);
      if (!id[0] || !tit[0]) ok = 0;
      if (desde[0] && strcmp(hoje, desde) < 0) ok = 0;
      if (ate[0] && strcmp(hoje, ate) > 0) ok = 0;
      if (ateV[0] && versaoMaior(NV_VERSAO, ateV)) ok = 0;
#ifdef __EMSCRIPTEN__
      if (plat[0] && strcmp(plat, "todas") && strcmp(plat, "tizen")) ok = 0;
#else
      if (plat[0] && strcmp(plat, "todas") && strcmp(plat, "lg")) ok = 0;
#endif
      { char cid[72];
        snprintf(cid, sizeof cid, "canal:%s", id);
        if (ok) novos += por(cid, AV_CANAL, ingles && titEn[0] ? titEn : tit,
                             ingles && txtEn[0] ? txtEn : txt, NULL);
        else tirar(cid); }
      p = js_prox(f);
    }
    pthread_mutex_unlock(&trava);
    toast(novos);
    printf("[avisos] canal lido: %d aviso(s) novo(s)\n", novos);
    fflush(stdout);
  }
  free(corpo);
  canalVivo = 0;
  return NULL;
}

// --- ciclo ---------------------------------------------------------------------------
void avisos_iniciar(void) {
  char *m;
  vistosLer();
  m = dados_ler(AV_MARCA_ARQ);
  // MARCA PRESENTE NAO E CRASH quando a sessao anterior se despediu por fora
  // do IDBFS (issue #120; ver dados_despedida_ler). No Tizen a remocao da marca
  // pode nao chegar ao IndexedDB antes de o processo morrer; e a TV fechar o
  // app escondido (Home, Exit, desligar) nao e o app cair.
  { int desp = dados_despedida_ler();
    if (m && desp) {
      printf("[avisos] marca da sessao anterior presente, mas ela se despediu (%s): nao e crash\n",
             desp == 1 ? "saida pelo app" : "pagina escondida, a TV fechou em segundo plano");
      fflush(stdout);
      free(m); m = NULL;
    } }
  if (m) {
    char v[24] = "", sinal[96] = "";
    const char *nl;
    // "1.3.1 2026-09-19 18:40" e, desde a 1.4.1, uma segunda linha com o
    // ultimo sinal de vida (ver marcaGravar). Marca antiga nao tem a segunda.
    sscanf(m, "%23s %39[^\n]", v, crashQuando);
    nl = strchr(m, '\n');
    if (nl && nl[1]) sscanf(nl + 1, "%95[^\n]", sinal);
    crashDetectado = 1;
    free(m);
    printf("[avisos] a sessao anterior (%s, %s) nao se despediu: marca presente; %s\n",
           v, crashQuando, sinal[0] ? sinal : "no last signal (old mark)");
    fflush(stdout);
#ifdef __EMSCRIPTEN__
    lerLogAnterior();
#endif
    { char id[72], tit[160], txt[420];
      snprintf(id, sizeof id, "crash:%s", crashQuando);
      snprintf(tit, sizeof tit, "%s", i18n("O app fechou sozinho"));
      snprintf(txt, sizeof txt, i18n("Em %s o Nuvio parou sem avisar. Se quiser, envie o registro daquela sessão para ajudar a encontrar a causa."), crashQuando);
      // SEM CARTAO E SEM TOAST NO ARRANQUE (dono, 23/09/2026: "tira a
      // mensagem de enviar o log quando entra no app, ja temos os logs"). A
      // queda fica so na lista de Avisos, em silencio; quem ligou o envio
      // automatico continua mandando o registro anterior (avisos_envio_auto_passo).
      pthread_mutex_lock(&trava);
      (void)por(id, AV_CRASH, tit, txt, NULL);
      pthread_mutex_unlock(&trava); }
  }
  marcaGravar();
  canalProximo = SDL_GetTicks() + 8000;   // depois da home, nao junto com ela
  // DEMONSTRACAO: NUVIO_AVISOS_DEMO=1 poe um item de cada tipo na lista, para
  // olhar o painel sem esperar amigo, estreia, versao nova ou crash de verdade.
  // So na previa; a TV nao tem ambiente.
  { const char *demo = getenv("NUVIO_AVISOS_DEMO");
    if (demo && demo[0] == '1') {
      int novos = 0;
      pthread_mutex_lock(&trava);
      // DADO DE MENTIRA, nao rotulo: os textos abaixo imitam o que a rede
      // traria (nome de amigo, titulo, aviso do dono), por isso passam por
      // demoAviso e nao por i18n — a varredura de i18n sabe disso.
      novos += demoAviso("demo:rec", AV_REC, i18n("Recomendação de amigo"),
                   "Gustavo recomendou \"The Gentlemen\": \"vale cada minuto\". Abra Salvos para ver.", NULL);
      novos += demoAviso("demo:agenda", AV_AGENDA, i18n("Episódio novo"),
                   "Outlander — T7E9 · Unfinished Business", "tt3006802");
      novos += demoAviso("demo:update", AV_UPDATE, i18n("Atualização disponível"),
                   "Versão 1.3.2 disponível. Você está na 1.3.1.", "1.3.2");
      novos += demoAviso("demo:canal", AV_CANAL, "Guia de TV demorando",
                   "Na 1.3.1 o guia espera a rede a cada abertura. A 1.3.2 corrige. — Henrique", NULL);
      novos += demoAviso("demo:crash", AV_CRASH, i18n("O app fechou sozinho"),
                   "Em 2026-09-19 18:57 o Nuvio parou sem avisar. Se quiser, envie o registro daquela sessão para ajudar a encontrar a causa.", NULL);
      pthread_mutex_unlock(&trava);
      toast(novos);
      cartaoPendente = 1; snprintf(cartaoId, sizeof cartaoId, "demo:crash");
      if (!crashQuando[0]) snprintf(crashQuando, sizeof crashQuando, "2026-09-19 18:57");
    } }
}

void avisos_mostrar_se_houver(void) {
  if (!cartaoPendente || cartao || aberto) return;
  cartaoPendente = 0;
  cartao = 1; cartaoFoco = 0;
}
int avisos_cartao_aberto(void) { return cartao; }

static void cartaoFechar(void) {
  cartao = 0;
  pthread_mutex_lock(&trava);
  marcarVisto(cartaoId);
  { int i; for (i = 0; i < n; i++) if (!strcmp(itens[i].id, cartaoId)) itens[i].visto = 1; }
  pthread_mutex_unlock(&trava);
  vistosSujos = 1;
}

static void enviarAgora(void) {
  if (envioEstado == 1) return;
  if (!NV_REC_URL[0]) { envioEstado = 3; return; }
  envioEstado = 1;
  if (pthread_create(&fioEnvio, NULL, enviarRegistro, NULL) == 0) pthread_detach(fioEnvio);
  else envioEstado = 3;
}

static int cartaoEvento(const SDL_Event *e) {
  SDL_Keycode k = e->key.keysym.sym;
  int sc = e->key.keysym.scancode;
  if (e->type != SDL_KEYDOWN) return 1;
  if (e->key.repeat) return 1;
  if (k == SDLK_LEFT)  { cartaoFoco = 0; return 1; }
  if (k == SDLK_RIGHT) { cartaoFoco = 1; return 1; }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE || sc == NV_SCANCODE_BACK) { cartaoFechar(); return 1; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    if (envioEstado == 2 || envioEstado == 3) { cartaoFechar(); return 1; }   // "Fechar" depois do envio
    if (cartaoFoco == 0) enviarAgora();
    else cartaoFechar();
    return 1;
  }
  return 1;
}

// Botao do cartao: a PILULA DA TABELA (botoes.h). "Enviar registro" e o
// primario (72 px, superficie cheia), "Agora nao" o secundario (56 px, so
// contorno) — a hierarquia e escala e peso, nao uma segunda cor. Os dois se
// alinham pela BASE. `ar/ag/ab` sobraram da assinatura antiga; a tabela le o
// realce sozinha.
static float botao(float x, float y, const char *rot, int foco, float a, float ar, float ag, float ab, int primario) {
  float h = primario ? BOTAO_H_PRIMARIO : BOTAO_H_SECUNDARIO;
  GfxRect r = { x, y + (BOTAO_H_PRIMARIO - h), botao_largura(rot, NULL, primario), h };
  (void)ar; (void)ag; (void)ab;
  botao_pilula(r, rot, NULL, foco ? 1.0f : 0.0f, primario, 0, a);
  return r.w;
}

static void cartaoDesenhar(void) {
  const float W = 980.0f, H = 336.0f;
  float a = cartaoA, x = (NV_TELA_W - W) * 0.5f, y = (NV_TELA_H - H) * 0.5f + (1.0f - a) * 30.0f;
  float ar, ag, ab, bx;
  char txt[300];
  if (a < 0.01f) return;
  ajustes_acento(&ar, &ag, &ab);
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.70f * a);
  // Mantem a luz ambiente do cartao para dar profundidade sem tornar o estado
  // de foco da acao um bloco saturado.
  gfx_cor((GfxRect){ x, y, W, H }, 28.0f / H, 0.055f, 0.058f, 0.068f, 0.94f * a);
  gfx_luz_canto((GfxRect){ x, y, W, H }, 28.0f / H, W * 0.05f, -W * 0.15f, W * 0.5f, ar, ag, ab, 0.22f * a);
  gfx_cor((GfxRect){ x + 56.0f, y + 56.0f, 56.0f, 56.0f }, 0.5f, 0.16f, 0.17f, 0.20f, a);
  gfx_icone((GfxRect){ x + 69.0f, y + 69.0f, 30.0f, 30.0f }, "fluxo", 0.62f, 0.80f, 0.96f, a);
  { TxtLinha t = txt_linha(TXT_TITULO3, i18n("O app fechou sozinho"), 246, 247, 252, 255);
    txt_desenhar_alpha(t, x + 136.0f, y + 52.0f, a); }
  snprintf(txt, sizeof txt, i18n("Em %s o Nuvio parou sem avisar. O registro daquela sessão (os últimos 200 KB do log, sem senhas nem chaves) ajuda a achar a causa. Quer enviar?"), crashQuando);
  txt_bloco(TXT_BODY, txt, 200, 203, 210, x + 56.0f, y + 132.0f, W - 112.0f, 34.0f, a, 3);
  bx = x + 56.0f;
  if (envioEstado == 1) {
    TxtLinha t = txt_linha(TXT_BODY, i18n("Enviando…"), 200, 203, 210, 255);
    txt_desenhar_alpha(t, bx, y + H - 56.0f - BOTAO_H_PRIMARIO + 22.0f, a);
  } else if (envioEstado == 2 || envioEstado == 3) {
    TxtLinha t = txt_linha(TXT_BODY, envioEstado == 2 ? i18n("Registro enviado. Obrigado.") : i18n("Não foi possível enviar agora."),
                           envioEstado == 2 ? 120 : 237, envioEstado == 2 ? 200 : 77, envioEstado == 2 ? 140 : 77, 255);
    txt_desenhar_alpha(t, bx, y + H - 56.0f - BOTAO_H_PRIMARIO + 22.0f, a);
    botao(x + W - 56.0f - botao_largura(i18n("Fechar"), NULL, 1), y + H - 56.0f - BOTAO_H_PRIMARIO, i18n("Fechar"), 1, a, ar, ag, ab, 1);
  } else {
    bx += botao(bx, y + H - 56.0f - BOTAO_H_PRIMARIO, i18n("Enviar registro"), cartaoFoco == 0, a, ar, ag, ab, 1) + BOTAO_GAP;
    botao(bx, y + H - 56.0f - BOTAO_H_PRIMARIO, i18n("Agora não"), cartaoFoco == 1, a, ar, ag, ab, 0);
  }
}

void avisos_encerrar(void) {
  dados_despedida_fim();   // no Tizen: sincrono, vale mesmo se o apagar abaixo nao chegar ao disco
  dados_apagar(AV_MARCA_ARQ);
  vistosGravar();
}

// Fontes que o app ja tem: um item por estado, atualizado a cada volta.
static void colherLocais(void) {
  int novos = 0;
  pthread_mutex_lock(&trava);
  // Recomendacoes
  { int k = recomenda_ativo() ? recomenda_n_novas() : 0;
    if (k > 0) {
      char txt[200];
      snprintf(txt, sizeof txt, k == 1 ? i18n("%d recomendação nova de um amigo. Abra Salvos para ver.")
                                       : i18n("%d recomendações novas de amigos. Abra Salvos para ver."), k);
      novos += por("rec", AV_REC, i18n("Recomendação de amigo"), txt, NULL);
    } else tirar("rec"); }
  // Atualizacao
  { const char *v = atualizacao_nova();
    if (v && v[0]) {
      char id[72], txt[200];
      snprintf(id, sizeof id, "update:%s", v);
      snprintf(txt, sizeof txt, i18n("Versão %s disponível. Você está na %s."), v, NV_VERSAO);
      novos += por(id, AV_UPDATE, i18n("Atualização disponível"), txt, v);
    } }
  // Agenda: lembretes vencidos (agendaviso.c continua abrindo o cartao dele;
  // aqui fica o rastro na lista para quem fechou o cartao sem ler).
  { const AgItem *dev[8];
    int q = agenda_devidos(dev, 8), i;
    for (i = 0; i < q; i++) {
      char id[72], txt[300];
      snprintf(id, sizeof id, "agenda:%s:%s", dev[i]->imdb, dev[i]->dataProx);
      if (dev[i]->temporada > 0 && dev[i]->episodio > 0)
        snprintf(txt, sizeof txt, i18n("%s — T%dE%d%s%s"), dev[i]->titulo, dev[i]->temporada, dev[i]->episodio,
                 dev[i]->nomeEp[0] ? " · " : "", dev[i]->nomeEp);
      else snprintf(txt, sizeof txt, "%s", dev[i]->titulo);
      novos += por(id, AV_AGENDA, i18n("Episódio novo"), txt, dev[i]->imdb);
    } }
  pthread_mutex_unlock(&trava);
  toast(novos);
}

void avisos_atualizar(float dt, Uint32 agora) {
  static Uint32 ultColheita;
  entrada = anim_mola(entrada, aberto ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  cartaoA = anim_mola(cartaoA, cartao ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  if (agora - ultColheita > 2000) { ultColheita = agora; colherLocais(); }
#ifdef NV_LEVE
  canalProximo = agora + AV_CANAL_INTERVALO_MS;   // build de diagnostico: sem canal
#endif
  if (agora >= canalProximo && !canalVivo) {
    canalVivo = 1;
    canalProximo = agora + AV_CANAL_INTERVALO_MS;
    if (pthread_create(&fioCanal, NULL, fioCanalFn, NULL) == 0) pthread_detach(fioCanal);
    else canalVivo = 0;
  }
  { float alvo = (toastAte > 0.0f && (float)agora < toastAte && !aberto) ? 1.0f : 0.0f;
    toastA = ajustes_animacoes_reduzidas() ? alvo : anim_mola(toastA, alvo, dt, NV_MOLA_TELA);
    if (alvo == 0.0f && toastA < 0.01f && toastAte > 0.0f && (float)agora >= toastAte) { toastAte = 0.0f; toastN = 0; } }
  if (vistosSujos && !aberto) vistosGravar();
  avisos_envio_auto_passo(agora);
}

int  avisos_aberto(void) { return aberto; }
void avisos_abrir(void)  { aberto = 1; foco = 0; rol = 0.0f; toastAte = 0.0f; toastN = 0; }
const char *avisos_pediu_abrir(void) {
  static char saida[24];
  if (!pediuAbrir[0]) return NULL;
  snprintf(saida, sizeof saida, "%s", pediuAbrir);
  pediuAbrir[0] = 0;
  return saida;
}
int avisos_pediu(void) { int c = pediuCodigo; pediuCodigo = 0; return c; }

static void fechar(void) {
  avisos_marcar_lidos();
  aberto = 0;
}

int avisos_evento(const SDL_Event *e) {
  SDL_Keycode k;
  int sc;
  if (cartao) return cartaoEvento(e);
  if (e->type != SDL_KEYDOWN) return aberto;
  k = e->key.keysym.sym; sc = e->key.keysym.scancode;
  if (!aberto) {
    // O TOAST NA TELA e a unica hora em que AZUL/CH+ vem para ca: fora dela
    // as duas teclas continuam sendo o que sempre foram (Salvos, secao do guia).
    if (toastA > 0.5f && !e->key.repeat &&
        (k == SDLK_s || sc == NV_SCANCODE_BLUE || sc == NV_SCANCODE_CH_UP || k == SDLK_PAGEUP)) {
      avisos_abrir();
      return 1;
    }
    return 0;
  }
  if (e->key.repeat && k != SDLK_UP && k != SDLK_DOWN) return 1;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE || sc == NV_SCANCODE_BACK ||
      k == SDLK_s || sc == NV_SCANCODE_BLUE) { fechar(); return 1; }
  if (k == SDLK_UP)   { if (foco > 0) foco--; return 1; }
  if (k == SDLK_DOWN) { if (foco + 1 < n) foco++; return 1; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    if (avisos_lista_ok(foco)) fechar();
    return 1;
  }
  return 1;
}

// --- desenho -----------------------------------------------------------------------
#define AVP_W    760.0f
#define AVP_X    (NV_TELA_W - AVP_W)
#define AVP_MARG  48.0f
#define AVP_TOPO 176.0f

static const char *icone(int tipo) {
  switch (tipo) {
    case AV_REC:    return "recomendar";
    case AV_AGENDA: return "lembrete";
    case AV_UPDATE: return "avancar";
    case AV_CRASH:  return "fluxo";
    default:        return "menu_settings";
  }
}

static void desenharToast(Uint32 agora) {
  // AVISO COM HIERARQUIA: icone + contexto + contagem + acao. A versao de uma
  // linha era funcional, mas parecia uma legenda pequena perdida no canto da
  // TV. O bloco agora tem uma leitura em dois tempos: "Central de avisos" como
  // contexto, depois a contagem em corpo maior, e por fim a tecla desenhada.
  // A tecla continua sendo o disco azul da LG ou o rocker CH+ da Samsung, nao
  // texto — "AZUL" e uma cor a procurar entre quatro.
  //
  // PULSA na cor de acento (dono: "meio que piscar com a cor pra chamar
  // atencao"): a luz difusa e o ponto respiram a ~1 Hz. Sem piscar de verdade
  // — ligar/desligar num canto de TV le como defeito; a respiracao le como
  // "tem algo aqui".
  char txt[120];
  TxtLinha cab, t1, t2;
  float w, h = 104.0f, x, y, ar, ag, ab, pulso, lado = 42.0f;
  float tinta;
  GfxRect bloco;
  if (toastA < 0.01f) return;
  tinta = ajustes_acento_tinta(&ar, &ag, &ab);
  pulso = ajustes_animacoes_reduzidas() ? 0.5f :
          0.5f + 0.5f * sinf((float)agora * (2.0f * 3.14159265f / 1100.0f));
  snprintf(txt, sizeof txt, toastN == 1 ? i18n("%d aviso novo") : i18n("%d avisos novos"), toastN);
  cab = txt_linha(TXT_CAPTION2, i18n("Central de avisos"), 156, 160, 172, 255);
  t1 = txt_linha(TXT_BODY, txt, (int)(tinta * 255.0f + 0.5f),
                 (int)(tinta * 255.0f + 0.5f), (int)(tinta * 255.0f + 0.5f), 255);
  t2 = txt_linha(TXT_CAPTION2, i18n("abre"), 178, 181, 190, 255);
  w = 24.0f + 56.0f + 18.0f + (cab.w > t1.w ? cab.w : t1.w) +
      30.0f + lado + 10.0f + t2.w + 24.0f;
  // CANTO SUPERIOR DIREITO: o aviso sai da cena e nao compete com as fileiras
  // de conteudo. A entrada vem de cima, com distancia suficiente para ser lida
  // como um componente e nao como texto que piscou no canto.
  x = NV_TELA_W - 64.0f - w;
  y = 40.0f - (1.0f - toastA) * 32.0f;
  bloco = (GfxRect){ x, y, w, h };
  // Luz difusa de acento respirando POR TRAS da pilula, no lugar do anel de
  // 2 px: a linguagem nova do app nao tem aneis (dono, 21/09/2026), e uma
  // mancha que cresce e apaga chama tanto quanto o anel sem desenhar borda.
  gfx_rect((GfxRect){ x - h * 0.7f, y - h * 0.7f, w + h * 1.4f, h * 2.4f }, 0, GFX_SOMBRA,
           1.0f, 0, 0, 0.5f, ar, ag, ab, (0.07f + 0.13f * pulso) * toastA);
  gfx_cor(bloco, 28.0f / h, 0.055f, 0.058f, 0.068f, 0.98f * toastA);
  // Icone de notificacao: SO O SINO, sem disco (dono, 23/09/2026: "deixar so
  // o sininho sem fundo"). O sino leva a cor de realce e cresce para ocupar o
  // lugar do disco; o ponto que pulsa acompanha na mesma cor.
  { GfxRect ic = { x + 24.0f, y + 24.0f, 56.0f, 56.0f };
    float d = 5.0f + 3.0f * pulso;
    // (23/09) Sino PREENCHIDO do Phosphor (bell-fill, MIT) em art/icones/
    // sino.png, no lugar do GFX_SINO de contorno fino — o dono pediu "tirar o
    // contorno do sino e usar um sino mais bonito".
    gfx_icone((GfxRect){ ic.x + 6.0f, ic.y + 6.0f, 44.0f, 44.0f }, "sino",
              ar, ag, ab, toastA);
    gfx_cor((GfxRect){ ic.x + ic.w - d - 1.0f, ic.y - d * 0.5f, d, d },
            0.5f, ar, ag, ab, 0.90f * toastA); }
  txt_desenhar_alpha(cab, x + 98.0f, y + 18.0f, toastA);
  txt_desenhar_alpha(t1, x + 98.0f, y + 48.0f, toastA);
  { float ax = x + w - 24.0f - lado - 10.0f - t2.w;
    sintro_tecla_atalho(ax, y + 25.0f, lado, toastA);
    txt_desenhar_alpha(t2, ax + lado + 10.0f, y + 25.0f + (lado - t2.h) * 0.5f, toastA); }
}

// A LISTA, desenhada dentro de qualquer caixa: o painel proprio usa, e a aba
// AVISOS do painel de Salvos tambem (pedido do dono: abrir quando quiser, sem
// depender do toast). `foco` e de quem chama; -1 = nenhuma linha em foco.
#define AVL_ROW 164.0f
// A LINHA EM FOCO DE UM AVISO DO CANAL CRESCE para o texto inteiro (20/09/2026,
// visto na previa do aviso da 1.3.4-rc1: duas linhas cortavam justamente o
// "onde baixar"). As outras ficam em AVL_ROW. A altura expandida e medida no
// desenho (txt_bloco devolve o que ocupou) e vale a partir do quadro seguinte —
// a mola do hospedeiro engole o quadro de diferenca.
#define AVL_LINHAS_CANAL 8
static float alturaCanalFoco = AVL_ROW + 4.0f * 27.0f;
static int ehCanalExpansivel(int i) { return i >= 0 && i < n && itens[i].tipo == AV_CANAL; }
float avisos_lista_altura_linha(int linha, int focoLinha) {
  float h = AVL_ROW;
  pthread_mutex_lock(&trava);
  if (linha == focoLinha && ehCanalExpansivel(linha)) h = alturaCanalFoco;
  pthread_mutex_unlock(&trava);
  return h;
}
float avisos_lista_y(int linha, int focoLinha) {
  float y = 0.0f;
  int i;
  for (i = 0; i < linha; i++) y += avisos_lista_altura_linha(i, focoLinha);
  return y;
}
float avisos_lista_altura(void) {
  pthread_mutex_lock(&trava);
  { float h = n > 0 ? (float)n * AVL_ROW : 60.0f; pthread_mutex_unlock(&trava); return h; }
}
int avisos_lista_n(void) { int k; pthread_mutex_lock(&trava); k = n; pthread_mutex_unlock(&trava); return k; }

void avisos_lista_desenhar(float x, float y0, float w, float a, int focoLinha) {
  float ar, ag, ab;
  int i;
  // Tinta sobre o realce: branca, a nao ser que o realce seja branco
  // (ajustes_acento_tinta). `tf` e o texto principal, `ts` o secundario.
  ajustes_acento(&ar, &ag, &ab);
  int tf = ajustes_tinta_foco(), ts = ajustes_tinta_foco2();
  pthread_mutex_lock(&trava);
  if (n == 0) {
    TxtLinha t = txt_linha(TXT_CAPTION, i18n("Nada por enquanto."), 150, 153, 162, 255);
    txt_desenhar_alpha(t, x, y0, a);
  }
  { float y = y0;
  for (i = 0; i < n; i++) {
    const Aviso *av = &itens[i];
    int f = (i == focoLinha);
    int expande = f && av->tipo == AV_CANAL;
    float rowH = expande ? alturaCanalFoco : AVL_ROW;
    GfxRect row = { x, y, w, rowH - 10.0f };
    const char *acao = NULL;
    // O cartao selecionado usa fill accent opaco; o halo reduzido fica atras
    // dele e nao vaza para os avisos vizinhos.
    if (f) {
      botao_luz(row, .4f, a);
      gfx_cor(row, 14.0f / row.h, ar, ag, ab, a);
    } else {
      gfx_cor(row, 14.0f / row.h, .062f, .066f, .079f, .92f * a);
    }
    gfx_cor((GfxRect){ x + 20.0f, y + 22.0f, 52.0f, 52.0f }, 0.5f,
            0.12f, 0.13f, 0.15f, a);
    gfx_icone((GfxRect){ x + 32.0f, y + 34.0f, 28.0f, 28.0f }, icone(av->tipo),
              f ? tf / 255.0f : 0.62f, f ? tf / 255.0f : 0.80f,
              f ? tf / 255.0f : 0.96f, a);
    // NOVO = um ponto na cor de acento colado ao icone, e nao uma pilula com
    // palavra: a palavra competia com o titulo e o ponto e o vocabulario que
    // a aba Social ja usa para "qual delas e nova".
    if (!av->visto && !f) gfx_cor((GfxRect){ x + 62.0f, y + 18.0f, 14.0f, 14.0f }, 0.5f, ar, ag, ab, a);
    { TxtLinha t = txt_linha_corta(TXT_BODY, av->titulo, f ? tf : 240, f ? tf : 241, f ? tf : 245, 255, w - 116.0f);
      txt_desenhar_alpha(t, x + 92.0f, y + 16.0f, a); }
    if (expande) {
      float h = txt_bloco(TXT_CAPTION, av->texto, 60, 62, 70, x + 92.0f, y + 50.0f, w - 116.0f, 27.0f, a, AVL_LINHAS_CANAL);
      float nova = 50.0f + h + 34.0f;
      if (nova < AVL_ROW) nova = AVL_ROW;
      alturaCanalFoco = nova;
    }
    else if (f) txt_bloco(TXT_CAPTION, av->texto, ts, ts, ts, x + 92.0f, y + 50.0f, w - 116.0f, 27.0f, a, 2);
    else        txt_bloco(TXT_CAPTION, av->texto, 150, 153, 162, x + 92.0f, y + 50.0f, w - 116.0f, 27.0f, a, 2);
    switch (av->tipo) {
      case AV_REC:    acao = i18n("OK abre Salvos"); break;
      case AV_AGENDA: acao = i18n("OK abre o título"); break;
      case AV_UPDATE: acao = i18n("OK abre a atualização"); break;
      case AV_CRASH:  acao = envioEstado == 1 ? i18n("Enviando…") : envioEstado == 2 ? i18n("Registro enviado. Obrigado.")
                           : envioEstado == 3 ? i18n("Não foi possível enviar. OK tenta de novo.") : i18n("OK envia o registro"); break;
      default: break;
    }
    if (acao) {
      TxtLinha t = txt_linha(TXT_CAPTION2, acao, f ? ts : 168,
                             f ? ts : 172, f ? ts : 182, 255);
      txt_desenhar_alpha(t, x + 92.0f, y + row.h - 34.0f, a * 0.95f);
    }
    y += rowH;
  } }
  pthread_mutex_unlock(&trava);
}

// OK numa linha, para quem hospeda a lista (o painel proprio ou a aba de
// Salvos). Devolve 1 quando a linha pediu para o hospedeiro FECHAR (a acao
// abre outra coisa); 0 quando a lista continua (canal, envio de registro).
int avisos_lista_ok(int linha) {
  Aviso a;
  int ha = 0;
  pthread_mutex_lock(&trava);
  if (linha >= 0 && linha < n) { a = itens[linha]; ha = 1; }
  pthread_mutex_unlock(&trava);
  if (!ha) return 0;
  switch (a.tipo) {
    case AV_REC:    pediuCodigo = AVISOS_ABRIR_SALVOS; return 1;
    case AV_UPDATE: pediuCodigo = AVISOS_ABRIR_ATUALIZACAO; return 1;
    case AV_AGENDA: snprintf(pediuAbrir, sizeof pediuAbrir, "%s", a.alvo); return 1;
    case AV_CRASH:
      if (envioEstado == 0 || envioEstado == 3) {
        if (!NV_REC_URL[0]) { envioEstado = 3; return 0; }
        envioEstado = 1;
        if (pthread_create(&fioEnvio, NULL, enviarRegistro, NULL) == 0) pthread_detach(fioEnvio);
        else envioEstado = 3;
      }
      return 0;
    default: return 0;
  }
}

// Tudo lido: o hospedeiro chama ao fechar (o painel proprio e a aba).
void avisos_marcar_lidos(void) {
  int i;
  pthread_mutex_lock(&trava);
  for (i = 0; i < n; i++) { itens[i].visto = 1; marcarVisto(itens[i].id); }
  pthread_mutex_unlock(&trava);
}

void avisos_desenhar(Uint32 agora) {
  float a = anim_clamp(entrada, 0.0f, 1.0f), dx;
  if (toastPendente && !aberto && !cartao) { toastPendente = 0; toastAte = (float)agora + AV_TOAST_MS; }
  if (!cartao) desenharToast(agora);
  if (a < 0.01f) { cartaoDesenhar(); return; }
  dx = (1.0f - a) * 80.0f;
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.45f * a);
  // O painel continua com luz ambiente suave; os cartoes e controles adotam
  // a nova superficie tonal sem retirar a profundidade aprovada da camada.
  { float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
    GfxRect p = { AVP_X + dx, 24.0f, AVP_W, NV_TELA_H - 48.0f };
    gfx_cor(p, 28.0f / AVP_W, 0.055f, 0.058f, 0.068f, 0.94f * a);
    gfx_luz_canto(p, 28.0f / AVP_W, AVP_W * 0.9f, -AVP_W * 0.1f, AVP_W * 0.65f, ar, ag, ab, 0.22f * a); }
  { TxtLinha t = txt_linha(TXT_HEADLINE, i18n("Avisos"), 240, 242, 248, 255);
    txt_desenhar_alpha(t, AVP_X + dx + AVP_MARG, 64.0f, a); }
  txt_bloco(TXT_CAPTION, i18n("Recomendações, estreias, versões novas, avisos de quem faz o app e o que aconteceu com ele."),
            150, 153, 162, AVP_X + dx + AVP_MARG, 118.0f, AVP_W - 2 * AVP_MARG, 28.0f, a, 2);
  gfx_recorte(AVP_X + dx, AVP_TOPO - 8.0f, AVP_W, NV_TELA_H - 80.0f - AVP_TOPO + 8.0f);
  { float areaH = NV_TELA_H - 80.0f - AVP_TOPO;
    float fim = avisos_lista_y(foco, foco) + avisos_lista_altura_linha(foco, foco);
    float alvo = fim > areaH ? fim - areaH : 0.0f;
    rol += (alvo - rol) * 0.25f; }
  avisos_lista_desenhar(AVP_X + dx + AVP_MARG, AVP_TOPO - rol, AVP_W - 2 * AVP_MARG, a, foco);
  gfx_sem_recorte();
  { TxtLinha t = txt_linha_corta(TXT_CAPTION2, i18n("↑ ↓ escolher · OK agir · Voltar fecha e marca tudo como lido"),
                                 140, 144, 154, 255, AVP_W - 2 * AVP_MARG);
    txt_desenhar_alpha(t, AVP_X + dx + AVP_MARG, NV_TELA_H - 62.0f, a * 0.85f); }
  cartaoDesenhar();
}

// --- envio manual (Ajustes) ---------------------------------------------------
int avisos_envio_estado(void) { return envioEstado; }
// Le o nv-log do localStorage para logAtual (fio principal; so no Tizen).
static void lerLogAtual(void) {
#ifdef __EMSCRIPTEN__
  free(logAtual);
  logAtual = (char *)EM_ASM_PTR({
    try {
      var t = localStorage.getItem('nv-log') || '';
      if (t.length > $0) t = t.slice(t.length - $0);
      var b = new TextEncoder().encode(t);
      var p = _malloc(b.length + 1);
      if (!p) return 0;
      HEAPU8.set(b, p);
      HEAPU8[p + b.length] = 0;
      return p;
    } catch (e) { return 0; }
  }, AV_REGISTRO_MAX);
#endif
}

// PASSO DO ENVIO AUTOMATICO, do laco principal. Com o ajuste ligado: uma vez,
// o registro da sessao ANTERIOR (e o da sessao que travou, quando travou);
// depois o desta sessao a cada minuto. Nunca dois envios ao mesmo tempo, e
// nunca por cima de um envio manual em curso.
void avisos_envio_auto_passo(Uint32 agora) {
  static int anteriorFeito;
  static Uint32 proximo;
  if (!ajustes_envio_auto() || !NV_REC_URL[0] || envioEstado == 1) return;
  if (!anteriorFeito) {
    anteriorFeito = 1;
    proximo = agora + 60000;
    { int tem;
#ifdef __EMSCRIPTEN__
      if (!logAnterior) lerLogAnterior();
      tem = logAnterior && logAnterior[0];
#else
      { FILE *f = fopen(AV_LOG_ANTERIOR, "rb"); tem = f != NULL; if (f) fclose(f); }
#endif
    if (tem) {
      envioEstado = 1;
      if (pthread_create(&fioEnvio, NULL, enviarRegistro, (void *)&AUTO_ANTERIOR) == 0) pthread_detach(fioEnvio);
      else envioEstado = 0;
    } }
    return;
  }
  if (agora < proximo) return;
  // 1 min no Tizen (e la que falta dado); 5 min no LG — o registro do LG com
  // o player aberto enche os 200 KB em menos de um minuto (haylereader,
  // 20/09: 200 KB por minuto de eventos [video]).
#ifdef __EMSCRIPTEN__
  proximo = agora + 60000;
#else
  proximo = agora + 300000;
#endif
  lerLogAtual();
  fflush(stdout);
  envioEstado = 1;
  if (pthread_create(&fioEnvio, NULL, enviarRegistro, (void *)&AUTO_ATUAL) == 0) pthread_detach(fioEnvio);
  else envioEstado = 0;
}

void avisos_enviar_registro_atual(void) {
  if (envioEstado == 1) return;
  if (!NV_REC_URL[0]) { envioEstado = 3; return; }
#ifdef __EMSCRIPTEN__
  // Fio principal: e o unico com localStorage. O shell grava nv-log a cada
  // 10 s, entao o que vai e o log ate a ultima gravacao.
  free(logAtual);
  logAtual = (char *)EM_ASM_PTR({
    try {
      var t = localStorage.getItem('nv-log') || '';
      if (t.length > $0) t = t.slice(t.length - $0);
      var b = new TextEncoder().encode(t);
      var p = _malloc(b.length + 1);
      if (!p) return 0;
      HEAPU8.set(b, p);
      HEAPU8[p + b.length] = 0;
      return p;
    } catch (e) { return 0; }
  }, AV_REGISTRO_MAX);
#endif
  fflush(stdout);   // o que este fio ja imprimiu entra no arquivo antes da leitura
  envioEstado = 1;
  if (pthread_create(&fioEnvio, NULL, enviarRegistro, (void *)&ATUAL) == 0) pthread_detach(fioEnvio);
  else envioEstado = 3;
}
