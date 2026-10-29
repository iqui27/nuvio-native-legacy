#include "arranque.h"
#include "avisos.h"
#include "queda.h"
#include "saidaandroid.h"
#include "ilha.h"
#include "ilhasalvar.h"
#define AV_ILHA_CHAVE "avisos"   // o "N avisos novos" da central na ilha (ilha.c junta)
#include "dados.h"
#include "rede.h"
#include "js.h"
#include "gfx.h"
#include "botoes.h"
#include "seguro.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "segurar.h"
#include "idioma.h"
#include "ajustes.h"
#include "recomenda.h"
#include "atualizacao.h"
#include "salvosintro.h"
#include "registro.h"
#include "regcodigo.h"
#include "redesaude.h"
#include <math.h>
#include "agenda.h"
#include "perfis.h"
#include "catalogo.h"
#include "salvos.h"
#include "sessao.h"
#include "trakt.h"
#include "marco.h"
#include "avisodisp.h"
#define NV_ESCALA_TELA   // o arquivo inteiro mede pela tela virtual (escala.h)
#include "escala.h"
#include "ponteiro.h"
#include "rolagemtoque.h"
#include "telefonecartao.h"
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
#if defined(NV_TPK) || defined(NV_ANDROID)
// No .tpk (e no Android) o /tmp nao e do app: o anterior fica na pasta de dados (tpk.c).
#define AV_LOG_ANTERIOR (getenv("NUVIO_LOG_ANTERIOR"))
#else
#define AV_LOG_ANTERIOR "/tmp/nuvio-anterior.log"
#endif
#define AV_REGISTRO_MAX (200 * 1024)
// Envio AUTOMATICO (sessao anterior): no maximo 64 KB, cabeca + cauda (#203).
#define AV_REGISTRO_AUTO_MAX (64 * 1024)
// 20 s, nao 6 (dono, 20/09/2026: "teria que ficar mais tempo"). Quem esta
// olhando um card do outro lado da tela leva um tempo para notar o canto.
#define AV_TOAST_MS   20000.0f

enum { AV_REC, AV_AGENDA, AV_UPDATE, AV_CANAL, AV_CRASH, AV_PEDIDO };
typedef struct {
  char id[72];
  int  tipo;
  char titulo[160];
  char texto[420];
  char alvo[24];        // imdb (agenda) ou versao (update)
  int  visto;
  int  anunciar;        // novo: a ilha ainda nao disse o assunto (anunciarNaIlha)
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
static Uint32 okDesde;              // OK afundado numa linha da central (soltarOk)
static void soltarOk(int longo);
static float entrada, rol;
#ifdef NV_TOUCH_UI
static ToqueRolagem toqueAvisos;
static int toqueAvisosRolar(const PonteiroRolagem *e);
#endif
static int   toastN;
static long long recAnunciada = -1;   // id da ultima recomendacao dita na ilha
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
// O RECIBO E O MOTIVO do ultimo envio MANUAL (o automatico so atualiza
// envAuto*): o painel de envio do registro (registro.c) mostra o codigo, o
// HTTP, o tamanho e, na falha, por que falhou — servidor, TV sem internet ou
// prazo. Escritos pelo fio de envio, lidos pelo de desenho; inteiros e
// strings curtas atras de `trava`.
static int    envMotivo, envHttp, envLinhas, envPendenteRede;
static long   envBytes;
static time_t envQuando, envAutoQuando;
static int    envAutoHttp;
static char   envCodigo[8];
static Uint32 envAutoProximo;
static pthread_t fioEnvio;

// O CARTAO DO CRASH, na reabertura: uma pergunta, dois botoes. Abre uma vez
// por crash (a marca e o id do item, gravada em vistos ao fechar) quando a
// home esta de pe e nenhum outro cartao esta na frente, como agendaviso.c.
static int   cartao;            // 1 = aberto
#ifdef NV_TOUCH_UI
static void crashPhoneReiniciar(void);
#endif
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
  char chave[72];
  int i;
  int privado = !strcmp(id, "rec") || !strncmp(id, "agenda:", 7) ||
                !strncmp(id, "pedidos:", 8);
  if (privado) {
    snprintf(chave, sizeof chave, "p%d:%s", perfis_ativo(), id);
    for (i = 0; i < nVistos; i++) if (!strcmp(vistos[i], chave)) return 1;
    // As marcas antigas pertenciam ao perfil principal.
    if (perfis_ativo() != 1) return 0;
  }
  for (i = 0; i < nVistos; i++) if (!strcmp(vistos[i], id)) return 1;
  return 0;
}
static void marcarVisto(const char *id) {
  if (foiVisto(id)) return;
  if (nVistos >= 120) { memmove(vistos[0], vistos[1], sizeof vistos[0] * 119); nVistos = 119; }
  if (!strcmp(id, "rec") || !strncmp(id, "agenda:", 7) || !strncmp(id, "pedidos:", 8))
    snprintf(vistos[nVistos++], sizeof vistos[0], "p%d:%s", perfis_ativo(), id);
  else snprintf(vistos[nVistos++], sizeof vistos[0], "%s", id);
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
  // DISPENSADO NAO VOLTA (avisodisp.h): nem na lista, nem na ilha, nem depois
  // de reiniciar. A chave e o evento; um evento novo tem outra chave.
  if (avisodisp_tem(id)) return 0;
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
  itens[n].anunciar = !itens[n].visto;
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

void avisos_idioma_definido(const char *codigo, const char *texto) {
  char id[72];
  int novo;
  snprintf(id, sizeof id, "idioma:%s", codigo ? codigo : "");
  pthread_mutex_lock(&trava);
  novo = por(id, AV_CANAL, i18n("Idioma"), texto ? texto : "", NULL);
  pthread_mutex_unlock(&trava);
  toast(novo);
}

int avisos_sessao_anterior_caiu(void) { return crashDetectado; }

void avisos_modo_seguro(const char *id, const char *titulo, const char *texto) {
  int novo;
  pthread_mutex_lock(&trava);
  novo = por(id, AV_CANAL, titulo ? titulo : "", texto ? texto : "", NULL);
  pthread_mutex_unlock(&trava);
  toast(novo);
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
  char tok[3000];
  if (trakt_ativo() && trakt_cabecalhos(tcab, aut, nAut, chave, nChave)) {
    snprintf(via, nVia, "X-Nuvio-Auth: trakt");
  } else if (sessao_token_copiar(tok, sizeof tok)) {
    // Pela COPIA (#203): a renovacao reescreve o token em outro fio.
    snprintf(aut, nAut, "Authorization: Bearer %s", tok);
    memset(tok, 0, sizeof tok);
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
#ifdef NV_VIDAA
           "vidaa",
#else
           "tizen",
#endif
#elif defined(NV_TPK)
           "tizen-tpk",
#elif defined(NV_ANDROID)
           "android",
#elif defined(NV_LINUX_DESKTOP)
           "linux-desktop",
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
#include "avisos_corte.inc"
static unsigned ultimoHashAuto;   // so muda depois de um envio que o servidor aceitou
static void *enviarRegistro(void *u) {
  unsigned hAuto = 0;
  static char aut[2200], via[40], chave[160];
  const char *cab[5];
  char *texto = NULL, *corpo, *resp;
  size_t nTexto = 0;
  int status = 0;
  int manual = (u == &ATUAL || u == &AUTO_ATUAL);
  int automatico = (u == &AUTO_ATUAL || u == &AUTO_ANTERIOR);
  int comAnterior = (u == &ATUAL);
#ifdef __EMSCRIPTEN__
  { const char *fonte = manual ? logAtual : logAnterior;
    if (fonte) { texto = strdup(fonte); if (texto) nTexto = strlen(texto); } }
#else
  { const char *arq = manual ? registro_arquivo() : (AV_LOG_ANTERIOR && AV_LOG_ANTERIOR[0] ? AV_LOG_ANTERIOR : "");
    FILE *f = (arq && arq[0]) ? fopen(arq, "rb") : NULL;
    if (f) {
      size_t max = automatico ? AV_REGISTRO_AUTO_MAX : AV_REGISTRO_MAX;
      FILE *ant = NULL;
      char *cauda = NULL;
      size_t nCauda = 0;
      if (comAnterior) ant = fopen(AV_LOG_ANTERIOR, "rb");
      if (ant) {
        cauda = malloc(max / 4 + 1);
        if (cauda) nCauda = lerRegistroCabecaCauda(ant, cauda, max / 4);
        fclose(ant);
        if (nCauda && !strstr(cauda, "[player]") && !strstr(cauda, "[video]") &&
            !strstr(cauda, "[queda]")) { free(cauda); cauda = NULL; nCauda = 0; }
      }
      texto = malloc(max + nCauda + 64);
      if (texto) nTexto = lerRegistroCabecaCauda(f, texto, max);
      if (texto && nCauda && nTexto + 32 + nCauda < max + nCauda + 64) {
        int m = snprintf(texto + nTexto, 48, "\n--- sessao anterior ---\n");
        if (m > 0) {
          memcpy(texto + nTexto + m, cauda, nCauda);
          nTexto += (size_t)m + nCauda;
          texto[nTexto] = 0;
        }
      }
      free(cauda);
      fclose(f);
    } }
#endif
  if (automatico && texto) nTexto = cortarCabecaCauda(texto, nTexto, AV_REGISTRO_AUTO_MAX);
  // ENVIO AUTOMATICO SEM NOVIDADE NAO SOBE (#203). Parado no menu o log nao
  // cresce, e a TV reenviava os mesmos 200 KB a cada 5 min: o D1 recebia
  // ~2 GB/dia e as escritas atrasavam as outras rotas. Mesmo texto da ultima
  // vez que subiu com sucesso = nada a fazer.
  if (automatico && texto) {
    size_t k;
    hAuto = 2166136261u;
    for (k = 0; k < nTexto; k++) hAuto = (hAuto ^ (unsigned char)texto[k]) * 16777619u;
    if (hAuto == ultimoHashAuto) { free(texto); envioEstado = 0; return NULL; }
  }
  if (!idHead(cab, aut, sizeof aut, via, sizeof via, chave, sizeof chave)) {
    if (!automatico) { pthread_mutex_lock(&trava); envMotivo = AVISOS_ENVIO_CONTA; envHttp = 0; pthread_mutex_unlock(&trava); }
    free(texto); envioEstado = automatico ? 0 : 3; return NULL;
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
#ifdef NV_VIDAA
             "vidaa",
#else
             "tizen",
#endif
#elif defined(NV_TPK)
             "tizen-tpk",
#elif defined(NV_ANDROID)
             "android",
#elif defined(NV_LINUX_DESKTOP)
           "linux-desktop",
#elif defined(__APPLE__)
             "mac",
#else
             "webos",
#endif
             u == &AUTO_ATUAL ? agoraTextoAuto("auto") :
             u == &AUTO_ANTERIOR ? agoraTextoAuto("anterior") :
             manual ? agoraTexto() : crashQuando, esc);
    free(esc); }
  { long linhas = 0; size_t k;
    for (k = 0; texto && k < nTexto; k++) if (texto[k] == '\n') linhas++;
    if (!automatico) { pthread_mutex_lock(&trava); envBytes = (long)nTexto; envLinhas = (int)linhas; pthread_mutex_unlock(&trava); } }
  free(texto);
  { char url[300];
    Uint32 t0 = SDL_GetTicks();
    snprintf(url, sizeof url, "%s/v1/registro", NV_REC_URL);
    resp = rede_postar_repetindo(url, 30, cab, corpo, &status, 3);
    if (automatico && status >= 200 && status < 300) ultimoHashAuto = hAuto;
    if (!automatico) {
      char id[32], cod[8] = "";
      int ok = status >= 200 && status < 300;
      // O codigo: o do recibo quando o servidor ja manda ("codigo"), senao o
      // derivado do registro_id (regcodigo.h) — os dois dao o mesmo.
      if (ok && resp) {
        const char *c = strstr(resp, "\"codigo\"");
        if (c && (c = strchr(c, ':')) != NULL) {
          int j = 0;
          c++;
          while (*c == ' ' || *c == '"') c++;
          while (j < 6 && ((*c >= '0' && *c <= '9') || (*c >= 'A' && *c <= 'Z'))) cod[j++] = *c++;
          cod[j] = 0;
          if (j != 6) cod[0] = 0;
        }
        if (!cod[0] && extrairRegistroId(resp, id, sizeof id)) regcodigo_de_id(id, cod);
      }
      pthread_mutex_lock(&trava);
      envHttp = status;
      snprintf(envCodigo, sizeof envCodigo, "%s", cod);
      envMotivo = ok ? AVISOS_ENVIO_OK
                : status > 0 ? AVISOS_ENVIO_SERVIDOR
                : rede_saude_offline() ? AVISOS_ENVIO_OFFLINE
                : SDL_GetTicks() - t0 >= 29000u ? AVISOS_ENVIO_PRAZO : AVISOS_ENVIO_CONEXAO;
      envPendenteRede = envMotivo == AVISOS_ENVIO_OFFLINE;
      pthread_mutex_unlock(&trava);
      if (cod[0]) {
        char linha[48];
        snprintf(linha, sizeof linha, "%s %ld\n", cod, (long)time(NULL));
        dados_gravar("registro-codigo.txt", linha);
      }
    } else {
      pthread_mutex_lock(&trava); envAutoQuando = time(NULL); envAutoHttp = status; pthread_mutex_unlock(&trava);
    } }
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
      // Todo idioma que nao o portugues le o texto em ingles: o canal so tem pt e en, e
      // o ingles e o que mais gente entende. So o portugues (o do Brasil e o de
      // Portugal) le o portugues.
      int ingles = ajustes_idioma() != IDIOMA_PT && ajustes_idioma() != IDIOMA_PTPT, ok = 1;
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
#ifdef NV_VIDAA
      if (plat[0] && strcmp(plat, "todas") && strcmp(plat, "vidaa")) ok = 0;
#else
      if (plat[0] && strcmp(plat, "todas") && strcmp(plat, "tizen")) ok = 0;
#endif
#elif defined(NV_TPK)
      // O .tpk tambem e Samsung: vale o "tizen" de sempre, mais o "tizen-tpk"
      // so dele. O anuncio do proprio preview (id com "tpk-preview") nao faz
      // sentido dentro dele.
      if (plat[0] && strcmp(plat, "todas") && strcmp(plat, "tizen") && strcmp(plat, "tizen-tpk")) ok = 0;
      if (strstr(id, "tpk-preview")) ok = 0;
#elif defined(NV_ANDROID)
      if (plat[0] && strcmp(plat, "todas") && strcmp(plat, "android")) ok = 0;
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
#ifdef NV_WEBOS
// "chave":"valor" de um JSON raso do nyx, sem depender de js.c: 1 se achou.
static int nyxCampo(const char *txt, const char *chave, char *out, size_t cap) {
  char alvo[64];
  const char *p, *f;
  snprintf(alvo, sizeof alvo, "\"%s\"", chave);
  out[0] = 0;
  if (!txt || !(p = strstr(txt, alvo))) return 0;
  p = strchr(p + strlen(alvo), '"');
  if (!p || !(f = strchr(++p, '"')) || (size_t)(f - p) >= cap) return 0;
  memcpy(out, p, (size_t)(f - p)); out[f - p] = 0;
  return 1;
}
// A LINHA [tv] DO webOS. Android e .tpk ja mandam modelo e sistema; a LG nao
// mandava nada, e a issue #265 (50NANO80ASA, travas e quedas na 2.0.0) nao
// tinha como ser casada com log nenhum. Os dois arquivos sao do nyx; se a
// jaula do app nao deixar ler, a linha diz isso em vez de sumir.
static int tvWebosLinha(char *linha, size_t cap, char *modeloOut, size_t capModelo) {
  static const char *const arqs[] = { "/var/run/nyx/device_info.json", "/var/run/nyx/os_info.json" };
  char buf[2][4096] = { "", "" }, modelo[64], placa[64], versao[48], build[64];
  for (int i = 0; i < 2; i++) {
    FILE *f = fopen(arqs[i], "rb");
    if (!f) continue;
    buf[i][fread(buf[i], 1, sizeof buf[i] - 1, f)] = 0;
    fclose(f);
  }
  if (!buf[0][0] && !buf[1][0]) { snprintf(linha, cap, "[tv] webos: /var/run/nyx ilegivel"); if (modeloOut) snprintf(modeloOut, capModelo, "webos"); return 0; }
  if (!nyxCampo(buf[0], "product_id", modelo, sizeof modelo)) nyxCampo(buf[0], "device_name", modelo, sizeof modelo);
  nyxCampo(buf[0], "hardware_id", placa, sizeof placa);
  nyxCampo(buf[1], "webos_release", versao, sizeof versao);
  nyxCampo(buf[1], "webos_manufacturing_version", build, sizeof build);
  snprintf(linha, cap, "[tv] modelo=%s host=webos-%s placa=%s fw=%s app=%s", modelo[0] ? modelo : "?",
           versao[0] ? versao : "?", placa[0] ? placa : "?", build[0] ? build : "?", NV_VERSAO);
  if (modeloOut) snprintf(modeloOut, capModelo, "%s", modelo[0] ? modelo : "webos");
  return 1;
}
static void tvWebos(void) {
  char linha[256];
  tvWebosLinha(linha, sizeof linha, NULL, 0);
  printf("%s\n", linha);
}
// Para o relato de arranque (arranque.c), que roda antes de avisos_iniciar.
void avisos_tv_linha(char *linha, size_t cap, char *modelo, size_t capModelo) {
  tvWebosLinha(linha, cap, modelo, capModelo);
}
// RELATO DE FALHA DE ARRANQUE (#317). Uma TV que cai antes de a home existir
// nao vive o bastante para o envio automatico (e talvez nem tenha conta): na
// abertura seguinte o arranque manda UMA vez o rastro e o relato de queda, por
// /v1/registro/arranque (sem conta, com tetos no servidor; o mesmo que o vigia
// do Android usa). SINCRONO e curto: `segundos` de teto, sem laco. Quem decide
// SE manda (ajuste "Enviar registros sozinho") e arranque.c. Devolve 1 se o
// servidor confirmou.
int avisos_enviar_arranque(const char *relato, const char *tv, int segundos) {
  char url[300], *esc, *corpo, *resp;
  const char *cab[2] = { "Content-Type: application/json", NULL };
  int status = 0, ok = 0;
  size_t n;
  if (!relato || !*relato || !NV_REC_URL[0]) return 0;
  n = strlen(relato);
  esc = malloc(n * 2 + 8);
  corpo = malloc(n * 2 + 512);
  if (!esc || !corpo) { free(esc); free(corpo); return 0; }
  jsonEsc(esc, n * 2 + 8, relato);
  snprintf(corpo, n * 2 + 512,
           "{\"versao\":\"%s\",\"plataforma\":\"webos\",\"quando\":\"arranque queda\",\"tv\":\"%s\",\"texto\":\"%s\"}",
           NV_VERSAO, tv ? tv : "webos", esc);
  snprintf(url, sizeof url, "%s/v1/registro/arranque", NV_REC_URL);
  resp = rede_postar_st(url, segundos, cab, corpo, &status);
  ok = status >= 200 && status < 300;
  printf("[arranque] relato de falha de arranque: HTTP %d\n", status);
  fflush(stdout);
  free(resp); free(corpo); free(esc);
  return ok;
}
#endif

void avisos_iniciar(void) {
  char *m;
#ifdef NV_WEBOS
  tvWebos();
  // Antes de tudo: o relato da queda anterior entra no log desta sessao (e
  // o que o envio automatico leva) e o registrador volta a ficar armado.
  { char qd[700];
    if (dados_caminho(qd, sizeof qd, "queda.txt")) { queda_relatar(qd); queda_armar(qd);
#ifdef NV_WEBOS
      arranque_espelhar_queda();   // o relato continua tambem em /tmp (#317)
#endif
    } }
#endif
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
#ifdef NV_ANDROID
  // MARCA PRESENTE NAO E CRASH quando o proprio Android diz que matou o
  // processo por atualizacao, parada forcada ou o app arrastado para fora dos
  // recentes (saidaandroid.h). Sem isto essas saidas acendiam o aviso de queda
  // e, abaixo de 60 s, empurravam o modo seguro.
  if (m && saida_android_nao_foi_queda(getenv("NUVIO_SAIDA_ANTERIOR"))) {
    printf("[avisos] marca da sessao anterior presente, mas o Android a encerrou sem queda: nao e crash\n");
    fflush(stdout);
    free(m); m = NULL;
  }
#endif
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
#ifdef NV_TOUCH_UI
  crashPhoneReiniciar();
#endif
}
int avisos_cartao_aberto(void) { return cartao; }

static void cartaoFechar(void) {
  cartao = 0;
#ifdef NV_TOUCH_UI
  crashPhoneReiniciar();
#endif
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

// PONTEIRO (#99). O cartao e uma camada; os botoes poem o foco pela MESMA
// variavel das setas (cartaoFoco) e o OK do clique faz o resto. Depois do
// envio so ha "Fechar", que qualquer OK aciona: o focar nao tem o que mexer.
static void ponteiroCartao(int i, int b) {
  (void)b;
  if (!cartao || envioEstado != 0 || i < 0 || i > 1 || cartaoFoco == i) return;
  cartaoFoco = i;
}

#ifdef NV_TOUCH_UI
static TelefoneCartao crashPhone;
static void crashPhoneReiniciar(void) { telefonecartao_limpar(&crashPhone); }
static int crashPhoneRolar(const PonteiroRolagem *e) {
  return cartao ? toquerol_evento(&crashPhone.rolagem, e) : 0;
}
static void crashPhoneAcao(int i, int b) {
  (void)b;
  if (!cartao || cartaoA <= .99f) return;
  if (envioEstado == 0 && i == 0) { cartaoFoco = 0; enviarAgora(); }
  else { cartaoFoco = 1; cartaoFechar(); }
}
static float crashPhoneCorpo(float x, float y, float w, float a) {
  float ini = y; char txt[300];
  y += telefonecartao_titulo("O app fechou sozinho", x, y, w, a);
  snprintf(txt, sizeof txt, i18n("Em %s o Nuvio parou sem avisar. O registro daquela sessão (os últimos 200 KB do log, sem senhas nem chaves) ajuda a achar a causa. Quer enviar?"), crashQuando);
  y += txt_bloco_corta(TXT_BODY, txt, 200, 203, 210, x, y, w, 34, a, 0) + 24;
  if (envioEstado > 0) y += telefonecartao_texto(TXT_BODY, envioEstado == 1 ? "Enviando…"
                                     : envioEstado == 2 ? "Registro enviado. Obrigado." : "Não foi possível enviar agora.", x, y, w, 34, a);
  return y - ini;
}
static void cartaoPhoneDesenhar(float a) {
  telefonecartao_medir(&crashPhone, NV_TELA_W, NV_TELA_H, envioEstado == 0 ? 2 : 1);
  GfxRect r = crashPhone.corpo;
  float h = crashPhoneCorpo(r.x, r.y, r.w, 0);
  telefonecartao_comecar(&crashPhone, h, envioEstado, cartao && a > .99f, crashPhoneRolar, a);
  crashPhoneCorpo(r.x, r.y - crashPhone.offset, r.w, a);
  gfx_sem_recorte();
  if (envioEstado == 0) {
    telefonecartao_botao(&crashPhone, 0, "Enviar registro", cartaoFoco == 0, NULL, crashPhoneAcao, 0, a);
    telefonecartao_botao(&crashPhone, 1, "Agora não", cartaoFoco == 1, NULL, crashPhoneAcao, 0, a);
  } else telefonecartao_botao(&crashPhone, 0, "Fechar", 1, NULL, crashPhoneAcao, 1, a);
}
#endif

static void cartaoDesenhar(void) {
  const float W = 980.0f, H = 336.0f;
  float a = cartaoA, x = (NV_TELA_W - W) * 0.5f, y = (NV_TELA_H - H) * 0.5f + (1.0f - a) * 30.0f;
  float ar, ag, ab, bx;
  int alvos = cartao && a > 0.99f;
  char txt[300];
  if (cartao) ponteiro_camada();
  if (a < 0.01f) return;
#ifdef NV_TOUCH_UI
  if (telefoneui_ativo()) { cartaoPhoneDesenhar(a); return; }
#endif
  ajustes_acento(&ar, &ag, &ab);
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.70f * a);
  // Mantem a luz ambiente do cartao para dar profundidade sem tornar o estado
  // de foco da acao um bloco saturado.
  gfx_vidro_ou_solido((GfxRect){ x, y, W, H }, 28.0f / H, GFX_VIDRO_DENS_MODAL, a, 0.055f, 0.058f, 0.068f, 0.94f);
  if (!ajustes_vidro()) gfx_luz_canto((GfxRect){ x, y, W, H }, 28.0f / H, W * 0.05f, -W * 0.15f, W * 0.5f, ar, ag, ab, 0.22f * a);
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
    { float bw = botao_largura(i18n("Fechar"), NULL, 1), by = y + H - 56.0f - BOTAO_H_PRIMARIO;
      if (alvos) ponteiro_alvo(x + W - 56.0f - bw, by, bw, BOTAO_H_PRIMARIO, ponteiroCartao, NULL, 0, 0);
      botao(x + W - 56.0f - bw, by, i18n("Fechar"), 1, a, ar, ag, ab, 1); }
  } else {
    float by = y + H - 56.0f - BOTAO_H_PRIMARIO, bw;
    if (alvos) ponteiro_alvo(bx, by, botao_largura(i18n("Enviar registro"), NULL, 1), BOTAO_H_PRIMARIO,
                             ponteiroCartao, NULL, 0, 0);
    bx += botao(bx, by, i18n("Enviar registro"), cartaoFoco == 0, a, ar, ag, ab, 1) + BOTAO_GAP;
    bw = botao_largura(i18n("Agora não"), NULL, 0);
    // O secundario e mais baixo e alinha pela base (ver botao).
    if (alvos) ponteiro_alvo(bx, by + BOTAO_H_PRIMARIO - BOTAO_H_SECUNDARIO, bw, BOTAO_H_SECUNDARIO,
                             ponteiroCartao, NULL, 1, 0);
    botao(bx, by, i18n("Agora não"), cartaoFoco == 1, a, ar, ag, ab, 0);
  }
}

void avisos_encerrar(void) {
  dados_despedida_fim();   // no Tizen: sincrono, vale mesmo se o apagar abaixo nao chegar ao disco
  dados_apagar(AV_MARCA_ARQ);
  vistosGravar();
  seguro_encerrar();   // confirma o que estava em prova e fecha a sessao no diario
}

// Fontes que o app ja tem: um item por estado, atualizado a cada volta.
static void colherLocais(void) {
  int novos = 0;
  pthread_mutex_lock(&trava);
  // Recomendacoes
  { int k = recomenda_ativo() ? recomenda_n_novas() : 0;
    if (k > 0) {
      char txt[200];
      RecItem r;
      snprintf(txt, sizeof txt, k == 1 ? i18n("%d recomendação nova de um amigo. Abra Salvos para ver.")
                                       : i18n("%d recomendações novas de amigos. Abra Salvos para ver."), k);
      novos += por("rec", AV_REC, i18n("Recomendação de amigo"), txt, NULL);
      // UMA VEZ POR RECOMENDACAO, nao por item da lista: o item "rec" e um so
      // (a contagem troca no lugar), mas cada recomendacao nova que chega e um
      // assunto novo para a ilha ("Ana recomendou Fallout"). Guarda o id da
      // ultima anunciada; a mais nova diferente dela e ainda nao vista anuncia.
      if (recomenda_item(0, &r) && !r.visto && r.id != recAnunciada) {
        int i;
        recAnunciada = r.id;
        for (i = 0; i < n; i++) if (!strcmp(itens[i].id, "rec")) {
          if (!itens[i].anunciar) { itens[i].anunciar = 1; novos++; }
        }
      }
    } else tirar("rec"); }
  // PEDIDOS DE AMIZADE: uma linha so, com a contagem, que leva a aba Amigos.
  // A chave e o pedido mais novo: um pedido novo acende o ponto de "novo" de
  // novo; o mesmo pedido relido nao. A ilha ja disse o assunto
  // (ilhasinais.c), entao a linha nao vai para a ilha (anunciar = 0).
  { int k = recomenda_ativo() ? recomenda_n_pedidos() : 0, i;
    char id[40] = "";
    RecPessoa p;
    if (k > 0 && recomenda_pedido(0, &p) && p.pub[0]) snprintf(id, sizeof id, "pedidos:%s", p.pub);
    for (i = n - 1; i >= 0; i--)
      if (!strncmp(itens[i].id, "pedidos:", 8) && strcmp(itens[i].id, id)) tirar(itens[i].id);
    if (id[0]) {
      char txt[200];
      snprintf(txt, sizeof txt, k == 1 ? i18n("%d pessoa quer ser sua amiga.")
                                       : i18n("%d pessoas querem ser suas amigas."), k);
      (void)por(id, AV_PEDIDO, i18n("Pedidos de amizade"), txt, NULL);
      for (i = 0; i < n; i++) if (!strcmp(itens[i].id, id)) itens[i].anunciar = 0;
    } }
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

  // O limiar do OK longo na central, com o dedo ainda no botao.
  if (aberto && segurar_venceu(SDL_GetTicks(), okDesde, "avisos")) soltarOk(1);
  if (!aberto) okDesde = 0;
  if (vistosSujos && !aberto) vistosGravar();
  avisos_envio_auto_passo(agora);
}

int  avisos_aberto(void) { return aberto; }
// A CENTRAL ABERTA LE TUDO: os avisos dela saem da ilha (os que diziam o
// assunto e o "N avisos novos" que juntou o resto).
void avisos_abrir(void)  {
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toqueAvisos);
#endif
  aberto = 1; foco = 0; rol = 0.0f; toastN = 0; ilha_retirar_grupo();
}
const char *avisos_pediu_abrir(void) {
  static char saida[24];
  if (!pediuAbrir[0]) return NULL;
  snprintf(saida, sizeof saida, "%s", pediuAbrir);
  pediuAbrir[0] = 0;
  return saida;
}
int avisos_pediu(void) { int c = pediuCodigo; pediuCodigo = 0; return c; }

void avisos_abrir_sessao(void) {
  pthread_mutex_lock(&trava);
  // As marcas ja possuem o dono no id; gravar nao as transfere para o novo perfil.
  vistosGravar();
  for (int i = n - 1; i >= 0; i--)
    if (itens[i].tipo == AV_REC || itens[i].tipo == AV_AGENDA || itens[i].tipo == AV_PEDIDO)
      tirar(itens[i].id);
  aberto = foco = 0; entrada = rol = 0.0f; okDesde = 0;
  toastN = toastPendente = 0; recAnunciada = -1;
  pediuAbrir[0] = 0; pediuCodigo = 0;
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toqueAvisos);
#endif
  pthread_mutex_unlock(&trava);
}

// PONTEIRO (#99): a linha sob o cursor vira o foco, pela MESMA variavel das
// setas. Trocar de linha solta um OK afundado, como a seta faz.
static void ponteiroLinha(int i, int b) {
  (void)b;
  if (!aberto || cartao || i == foco || i < 0 || i >= avisos_lista_linhas()) return;
  foco = i; okDesde = 0;
}
#ifdef NV_TOUCH_UI
static int toqueAvisosRolar(const PonteiroRolagem *e) {
  int r;
  if (!aberto || cartao) return 0;
  r = toquerol_evento(&toqueAvisos, e);
  if (r) okDesde = 0;
  return r;
}
#endif
int avisos_teste_foco(void) { return aberto ? foco : -1; }

static void fechar(void) {
  avisos_marcar_lidos();
  aberto = 0;
}

// SEGURAR OK NUMA LINHA DA CENTRAL DISPENSA (06/10). O painel proprio fica por
// cima de tudo e nao hospeda o menu do cartaz (ctxmenu.c so se desenha sobre o
// painel de Salvos): aqui o gesto longo e a acao direta, e a dica na linha em
// foco diz isso. O toque curto continua sendo o OK de sempre, decidido na
// soltura. NV_HOLD_MS e a mesma medida da home e de Salvos.
static void soltarOk(int longo) {
  okDesde = 0;
  if (longo) {
    avisos_lista_dispensar(foco);
    if (foco >= avisos_lista_linhas()) foco = avisos_lista_linhas() > 0 ? avisos_lista_linhas() - 1 : 0;
  } else if (avisos_lista_ok(foco)) fechar();
  else if (foco >= avisos_lista_linhas()) foco = 0;
}

int avisos_evento(const SDL_Event *e) {
#ifdef NV_TOUCH_UI
  if (toquerol_navegacao(e)) toquerol_limpar(&toqueAvisos);
#endif
  SDL_Keycode k;
  int sc;
  if (cartao) return cartaoEvento(e);
  if (e->type == SDL_KEYUP && aberto && okDesde) {
    k = e->key.keysym.sym;
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE)
      soltarOk(ponteiro_ok_longo() || SDL_GetTicks() - okDesde >= NV_HOLD_MS);
    return 1;
  }
  if (e->type != SDL_KEYDOWN) return aberto;
  k = e->key.keysym.sym; sc = e->key.keysym.scancode;
  if (!aberto) {
    // O AVISO DA CENTRAL NA ILHA e a unica hora em que AZUL/CH+ vem para ca:
    // fora dela as duas teclas continuam sendo o que sempre foram (Salvos,
    // secao do guia). Aviso com modal proprio (recomendacao, versao nova) ja
    // foi atendido antes, em ilha_evento: a pilula cresce para ele.
    if (ilha_tecla_central() && !e->key.repeat &&
        (k == SDLK_s || sc == NV_SCANCODE_BLUE || sc == NV_SCANCODE_CH_UP || k == SDLK_PAGEUP)) {
      avisos_abrir();
      return 1;
    }
    return 0;
  }
  if (e->key.repeat && k != SDLK_UP && k != SDLK_DOWN) return 1;
  if (k != SDLK_RETURN && k != SDLK_KP_ENTER && k != SDLK_SPACE) okDesde = 0;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE || sc == NV_SCANCODE_BACK ||
      k == SDLK_s || sc == NV_SCANCODE_BLUE) { fechar(); return 1; }
  if (k == SDLK_UP)   { if (foco > 0) foco--; return 1; }
  if (k == SDLK_DOWN) { if (foco + 1 < avisos_lista_linhas()) foco++; return 1; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    // Linha de aviso: decide na soltura (curto abre, longo dispensa). A
    // ultima, "Dispensar todos", nao tem o que segurar.
    if (foco < avisos_lista_n()) { okDesde = SDL_GetTicks(); if (!okDesde) okDesde = 1; }
    else soltarOk(0);
    return 1;
  }
  return 1;
}

// --- desenho -----------------------------------------------------------------------
#include "telefoneui.h"
#define AVP_W    telefoneui_largura(760.0f, NV_TELA_W, 24.0f)
#define AVP_X    (NV_TELA_W - AVP_W - (telefoneui_ativo() ? 24.0f : 0.0f))
#define AVP_MARG  48.0f
#define AVP_TOPO 176.0f

static const char *icone(int tipo) {
  switch (tipo) {
    case AV_REC:    return "recomendar";
    case AV_AGENDA: return "lembrete";
    case AV_UPDATE: return "avancar";
    case AV_CRASH:  return "fluxo";
    case AV_PEDIDO: return "aj_users";
    default:        return "menu_settings";
  }
}

// O TOAST MORA NA ILHA (01/10/2026, pedido do dono), e desde 02/10 ele DIZ O
// ASSUNTO (mockup aprovado): em vez de "1 aviso novo", "Ana recomendou
// Fallout", "Saiu T2E5 de The Bear", "Versão 1.7.2 chegou". Cada item novo
// entra na ilha como um aviso da CENTRAL (grupo = 1, chave "av:<id>"); quando
// chegam varios juntos, ilha.c deixa o primeiro dizer o seu e junta os que
// esperam num "N avisos novos" que abre esta lista — o mesmo toast de antes,
// so que para o resto. A regra de "quando" continua aqui: so com a central
// fechada e sem o cartao do crash na frente (avisos_desenhar).
//
// Os que tem para onde ir levam o MODAL da ilha (a pilula cresce, AZUL/CH+):
// recomendacao (Ver / Salvar / Dispensar), versao nova (Atualizar / Depois),
// queda (Enviar registro / Agora não / Dispensar) e o aviso do dono (o texto
// inteiro; Fechar / Dispensar). Voltar recolhe sem mudar nada; "Dispensar"
// tira o item da lista de vez (avisos_dispensar, avisodisp.h). O
// episodio novo abre o modal do cartao de estreia (ilhacart.c), quando ele
// esta na pilula. Quem executa o botao e avisos_ilha_acao.
static RecItem recDaIlha;              // a recomendacao que o modal mostra
static void anunciarItem(const Aviso *it) {
  char chave[96], txt[240], f1[200], f2[200];
  IlhaAvisoEx e;
  static IlhaModal m;
  memset(&e, 0, sizeof e);
  memset(&m, 0, sizeof m);
  snprintf(chave, sizeof chave, "av:%s", it->id);
  e.chave = chave; e.grupo = 1; e.texto = txt;
  switch (it->tipo) {
    case AV_REC: {
      RecItem *r = &recDaIlha;
      int idx;
      if (!recomenda_item(0, r) || !r->titulo[0]) { snprintf(txt, sizeof txt, "%s", it->texto); e.tipo = ILHA_ACENTO; e.icone = "recomendar"; e.tecla = 1; e.ms = 8000u; break; }
      snprintf(txt, sizeof txt, i18n("%s recomendou %s"),
               ilha_forte(f1, sizeof f1, r->deNome[0] ? r->deNome : "?"), ilha_forte(f2, sizeof f2, r->titulo));
      e.tipo = ILHA_ACENTO; e.ms = 8000u;
      e.rosto = r->deAvatar; e.rostoNome = r->deNome[0] ? r->deNome : "?"; e.capa = r->poster;
      snprintf(m.kicker, sizeof m.kicker, i18n("Recomendação de %s"), r->deNome[0] ? r->deNome : "?");
      snprintf(m.titulo, sizeof m.titulo, "%s", r->titulo);
      { char nota[24] = "";
        if (r->nota > 0) snprintf(nota, sizeof nota, " · IMDb %d,%d", r->nota / 10, r->nota % 10);
        snprintf(m.linha, sizeof m.linha, "%s%s%s%s", i18n(!strncmp(r->tipo, "series", 6) ? "Série" : "Filme"),
                 r->ano[0] ? " · " : "", r->ano, nota); }
      { const char *fr = rec_frase(r);
        if (fr && fr[0]) snprintf(m.fala, sizeof m.fala, "\xe2\x80\x9c%s\xe2\x80\x9d", fr); }
      if (r->criado > 0) rec_quando_texto(m.estado, sizeof m.estado, r->criado);
      // A ARTE 16:9 do titulo quando ele esta no catalogo; senao o cartaz.
      idx = r->imdb[0] ? cat_indice_por_imdb(r->imdb) : -1;
      { const CatItem *ci = idx >= 0 ? cat_item(idx) : NULL;
        snprintf(m.arte, sizeof m.arte, "%s", ci && ci->backdrop[0] ? ci->backdrop : r->poster); }
      snprintf(m.rosto, sizeof m.rosto, "%s", r->deAvatar);
      snprintf(m.rostoNome, sizeof m.rostoNome, "%s", r->deNome[0] ? r->deNome : "?");
      m.salvos = 1; m.nBotoes = 3;
      snprintf(m.botao[0], sizeof m.botao[0], "%s", i18n("Ver"));
      snprintf(m.botaoIcone[0], sizeof m.botaoIcone[0], "play");
      snprintf(m.botao[1], sizeof m.botao[1], "%s", i18n("Salvar"));
      snprintf(m.botaoIcone[1], sizeof m.botaoIcone[1], "aj_bookmark");
      snprintf(m.botao[2], sizeof m.botao[2], "%s", i18n("Dispensar"));
      e.modal = &m;
      break; }
    case AV_AGENDA: {
      const AgItem *ag = it->alvo[0] ? agenda_registro(it->alvo) : NULL;
      const char *tit = ag && ag->titulo[0] ? ag->titulo : it->texto;
      e.tipo = ILHA_ACENTO;
      // ESTREIA HOJE (o episodio vai ao ar hoje) e SAIU (ja foi): o mesmo
      // aviso de agenda, com a frase do dia. O de hoje e mais curto e nao
      // leva a tecla: ainda nao ha o que assistir.
      if (ag && agenda_dias(ag->dataProx) == 0) {
        snprintf(txt, sizeof txt, i18n("%s estreia hoje"), ilha_forte(f1, sizeof f1, tit));
        e.icone = "aj_calendar"; e.ms = 6000u;
      } else {
        if (ag && ag->temporada > 0 && ag->episodio > 0)
          snprintf(txt, sizeof txt, i18n("Saiu T%dE%d de %s"), ag->temporada, ag->episodio, ilha_forte(f1, sizeof f1, tit));
        else snprintf(txt, sizeof txt, i18n("Episódio novo de %s"), ilha_forte(f1, sizeof f1, tit));
        e.icone = "aj_tv-minimal-play"; e.ms = 8000u;
        e.cartao = ILHA_ESTREIA + 1;
        e.tecla = 1;
      }
      break; }
    case AV_UPDATE:
      // GLASS UI v2 (mockup ajustes-v2 "v2-upd-aviso", 03/10): o aviso de duas
      // linhas, e a tecla nao abre mais um modal da ilha — o proprio CARTAO DA
      // ATUALIZACAO nasce da pilula (atualizacao.c), pela acao do aviso.
      snprintf(txt, sizeof txt, "%s", it->texto);
      e.titulo = it->titulo;
      e.tipo = ILHA_ACENTO; e.icone = "aj_download"; e.ms = 10000u;
      e.acao = 1; e.dica = i18n("Ver o que mudou");
      break;
    case AV_CRASH:
      // O DONO TIROU O CARTAO DO ARRANQUE em 23/09 ("tira a mensagem de enviar
      // o log quando entra no app, ja temos os logs") e aprovou este aviso em
      // 02/10. As duas coisas cabem juntas assim: com o envio automatico
      // ligado o registro ja foi (ou vai) sozinho e a ilha nao pergunta nada;
      // desligado, ela diz uma vez, sem cartao, e AZUL leva ao envio.
      if (ajustes_envio_auto()) return;
      snprintf(txt, sizeof txt, "%s", i18n("O app fechou sozinho da última vez"));
      e.tipo = ILHA_ERRO; e.prior = ILHA_P2; e.icone = "aj_triangle-alert"; e.ms = 9000u;
      // O titulo do modal e a frase inteira (mockup do registro, quadro 14).
      snprintf(m.titulo, sizeof m.titulo, "%s", txt);
      snprintf(m.texto, sizeof m.texto, "%s", it->texto);
      snprintf(m.icone, sizeof m.icone, "aj_triangle-alert");
      m.tipo = ILHA_ERRO; m.nBotoes = 2;
      snprintf(m.botao[0], sizeof m.botao[0], "%s", i18n("Enviar registro"));
      snprintf(m.botaoIcone[0], sizeof m.botaoIcone[0], "aj_send");
      snprintf(m.kicker, sizeof m.kicker, "%s", i18n("Aviso"));
      m.cabecalho = 1;
      snprintf(m.botao[1], sizeof m.botao[1], "%s", i18n("Agora não"));
      m.nBotoes = 3;
      snprintf(m.botao[2], sizeof m.botao[2], "%s", i18n("Dispensar"));
      snprintf(m.botaoIcone[2], sizeof m.botaoIcone[2], "aj_x");
      e.modal = &m;
      break;
    default:
      // AV_CANAL: o aviso do dono (e os de idioma e modo seguro, que usam o
      // mesmo tipo). A pilula diz o titulo; o texto inteiro fica no modal.
      snprintf(txt, sizeof txt, "%s", it->titulo);
      e.tipo = ILHA_INFO; e.icone = "aj_megaphone"; e.ms = 10000u;
      if (it->texto[0]) {
        snprintf(m.kicker, sizeof m.kicker, "%s", i18n("Aviso"));
        snprintf(m.titulo, sizeof m.titulo, "%s", it->titulo);
        snprintf(m.texto, sizeof m.texto, "%s", it->texto);
        snprintf(m.icone, sizeof m.icone, "aj_megaphone");
        // Fechar = lido (a linha fica); Dispensar = some e nao volta (avisodisp.h).
        m.nBotoes = 2;
        snprintf(m.botao[0], sizeof m.botao[0], "%s", i18n("Fechar"));
        snprintf(m.botao[1], sizeof m.botao[1], "%s", i18n("Dispensar"));
        snprintf(m.botaoIcone[1], sizeof m.botaoIcone[1], "aj_x");
        e.modal = &m;
      }
      break;
  }
  ilha_avisar_ex(&e);
}

// Os itens com `anunciar` vao para a ilha, cada um uma vez. Sem nenhum
// marcado e com contagem (o toast(N) dos testes), o "N avisos novos" de antes.
//
// LIMITE DE RUIDO (06/10): UMA VEZ POR SESSAO POR CHAVE. Um item que sai e
// volta a lista (o "rec" quando a contagem zera e sobe, a versao nova depois de
// uma lista cheia) entrava de novo com `anunciar` e passava outra vez pela
// pilula. Ja dito nesta sessao (avisodisp_sessao_*) nao passa de novo; um
// evento novo e outra chave e passa. A recomendacao nova e a excecao: o item e
// um so ("rec") e cada recomendacao e um assunto novo (recAnunciada).
static void anunciarNaIlha(void) {
  static Aviso copia[AV_MAX];
  int i, k = 0;
  pthread_mutex_lock(&trava);
  for (i = 0; i < n; i++) if (itens[i].anunciar) {
    char ch[96];
    itens[i].anunciar = 0;
    snprintf(ch, sizeof ch, "av:%s", itens[i].id);
    if (strcmp(itens[i].id, "rec") && avisodisp_sessao_tem(ch)) continue;
    avisodisp_sessao_por(ch);
    copia[k++] = itens[i];
  }
  pthread_mutex_unlock(&trava);
  for (i = 0; i < k; i++) anunciarItem(&copia[i]);
  if (!k && toastN > 0) {
    char txt[120];
    snprintf(txt, sizeof txt, toastN == 1 ? i18n("%d aviso novo") : i18n("%d avisos novos"), toastN);
    { IlhaAvisoEx e;
      memset(&e, 0, sizeof e);
      e.chave = AV_ILHA_CHAVE; e.tipo = ILHA_ACENTO; e.icone = "sino"; e.texto = txt;
      e.ms = (unsigned)AV_TOAST_MS; e.tecla = 1; e.grupo = 1;
      ilha_avisar_ex(&e); }
  }
  toastN = 0;
}

void avisos_ilha_acao(const char *chave, int botao) {
  const char *id;
  if (!chave || strncmp(chave, "av:", 3)) return;
  id = chave + 3;
  if (!strcmp(id, "rec")) {
    if (botao == 1 && recDaIlha.imdb[0]) snprintf(pediuAbrir, sizeof pediuAbrir, "%s", recDaIlha.imdb);
    else if (botao == 2 && recDaIlha.imdb[0]) {
      // SALVAR: o titulo do catalogo quando existe, senao o que a
      // recomendacao trouxe (o mesmo minimo que salvos.c guarda).
      int idx = cat_indice_por_imdb(recDaIlha.imdb);
      const CatItem *ci = idx >= 0 ? cat_item(idx) : NULL;
      static CatItem tmp;
      if (!ci) {
        memset(&tmp, 0, sizeof tmp);
        snprintf(tmp.imdb, sizeof tmp.imdb, "%s", recDaIlha.imdb);
        snprintf(tmp.tipo, sizeof tmp.tipo, "%s", recDaIlha.tipo);
        snprintf(tmp.titulo, sizeof tmp.titulo, "%s", recDaIlha.titulo);
        snprintf(tmp.poster, sizeof tmp.poster, "%s", recDaIlha.poster);
        ci = &tmp;
      }
      // Primeira vez: a ilha pergunta onde o + salva e grava depois (ilhasalvar.c).
      if (!ilhasalvar_perguntar(ci, 1)) {
        salvos_definir(ci, 1);
        // O Trakt so se a pessoa pediu (o mesmo criterio do "+", app.c).
        if (ajustes_salvos_no_trakt()) trakt_watchlist(ci->imdb, 1);
        ilhasalvar_aviso(ci, 1);
      }
    }
    // As tres respondem a recomendacao: o selo apaga (o mesmo que abrir Salvos).
    recomenda_marcar_vistas();
    if (botao == 3) { avisos_dispensar(id); return; }
  } else if (!strncmp(id, "update:", 7)) {
    if (botao == 1) pediuCodigo = AVISOS_ABRIR_ATUALIZACAO;
  } else if (!strncmp(id, "crash:", 6)) {
    if (botao == 1) enviarAgora();
    else if (botao == 3) { avisos_dispensar(id); return; }
  } else if (botao == 2) {
    // O aviso do dono (e idioma, modo seguro): Fechar | Dispensar.
    avisos_dispensar(id);
    return;
  }
  avisos_marcar_visto(id);
}

// A LISTA, desenhada dentro de qualquer caixa: o painel proprio usa, e a aba
// AVISOS do painel de Salvos tambem (pedido do dono: abrir quando quiser, sem
// depender do toast). `foco` e de quem chama; -1 = nenhuma linha em foco.
// GLASS UI (mockup "ilha" tela 3, 02/10): a linha do aviso e a LINHA DA ILHA
// do painel Social (salvospainel.c) — sem caixa em repouso, superficie um
// degrau mais clara so no foco, raio 22, recuo 18/22, o disco do icone de 52,
// titulo 24 semibold, texto 19 a 62 % (duas linhas de 25) e a acao em 15 a
// 38 %. 18 + 29 + 4 + 50 + 4 + 18 + 18 = 141. Era um cartao escuro por linha
// e o foco num bloco cheio de acento: na aba Avisos, ao lado das outras
// tres, lia como outro aplicativo.
#define AVL_ROW 142.0f
#define AVL_PADX 22.0f
// A LINHA EM FOCO DE UM AVISO DO CANAL CRESCE para o texto inteiro (20/09/2026,
// visto na previa do aviso da 1.3.4-rc1: duas linhas cortavam justamente o
// "onde baixar"). As outras ficam em AVL_ROW. A altura expandida e medida no
// desenho (txt_bloco devolve o que ocupou) e vale a partir do quadro seguinte —
// a mola do hospedeiro engole o quadro de diferenca.
#define AVL_LINHAS_CANAL 8
static float alturaCanalFoco = AVL_ROW + 4.0f * 27.0f;
static int ehCanalExpansivel(int i) { return i >= 0 && i < n && itens[i].tipo == AV_CANAL; }
// A ALTURA DE CADA LINHA SAI DO QUE ELA TEM: texto de uma ou de duas linhas,
// e a linha da acao so quando o aviso tem acao. Com AVL_ROW fixo, um aviso de
// "modo seguro" (uma frase, sem acao) ganhava 40 px de superficie vazia
// embaixo no foco — a captura da aba Avisos mostrou. A largura do texto e a
// do ultimo desenho (todo hospedeiro desenha antes de rolar); antes do
// primeiro, a da aba Avisos do painel Social.
static float larguraTextoAviso = 610.0f;
static int temAcaoAviso(const Aviso *av) {
  return av->tipo == AV_REC || av->tipo == AV_AGENDA || av->tipo == AV_UPDATE || av->tipo == AV_CRASH ||
         av->tipo == AV_PEDIDO;
}
// A LINHA EM FOCO SEMPRE TEM A LINHA DE ACAO (06/10): e onde mora "Segure OK
// para dispensar". Fora do foco so quem tem acao a desenha.
static float alturaAvisoF(int i, int focado) {
  const Aviso *av = &itens[i];
  float h = 18.0f + 29.0f + 18.0f;
  if (av->texto[0])
    h += 4.0f + ((float)txt_largura(TXT_ILHA_SUB, av->texto) > larguraTextoAviso ? 50.0f : 25.0f);
  if (temAcaoAviso(av) || focado) h += 4.0f + 18.0f;
  return h < 92.0f ? 92.0f : h;
}
static float alturaAviso(int i) { return alturaAvisoF(i, 0); }
// "DISPENSAR TODOS": a ultima linha da lista, so quando ha o que dispensar.
#define AVL_TODOS_H 84.0f
int avisos_lista_linhas(void) { int k; pthread_mutex_lock(&trava); k = n ? n + 1 : 0; pthread_mutex_unlock(&trava); return k; }
float avisos_lista_altura_linha(int linha, int focoLinha) {
  float h = AVL_ROW;
  pthread_mutex_lock(&trava);
  if (linha == focoLinha && ehCanalExpansivel(linha)) h = alturaCanalFoco;
  else if (linha >= 0 && linha < n) h = alturaAvisoF(linha, linha == focoLinha);
  else if (linha == n && n > 0) h = AVL_TODOS_H;
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
  { float h = 0.0f;
    int i;
    for (i = 0; i < n; i++) h += alturaAviso(i);
    if (n == 0) h = 60.0f;
    else h += AVL_TODOS_H;
    pthread_mutex_unlock(&trava); return h; }
}
int avisos_lista_n(void) { int k; pthread_mutex_lock(&trava); k = n; pthread_mutex_unlock(&trava); return k; }

void avisos_lista_desenhar(float x, float y0, float w, float a, int focoLinha) {
  avisos_lista_desenhar_ptr(x, y0, w, a, focoLinha, NULL, 0.0f, 0.0f);
}
// PONTEIRO (#99): com `focar`, cada linha (e "Dispensar todos", indice n)
// vira alvo recortado a [clipY0, clipY1). O focar e de QUEM HOSPEDA a lista:
// o foco e dele (a central ou a aba Avisos de Salvos).
void avisos_lista_desenhar_ptr(float x, float y0, float w, float a, int focoLinha,
                               void (*focar)(int, int), float clipY0, float clipY1) {
  float ar, ag, ab;
  int i;
  const float fr = 243.0f / 255.0f, fg = 242.0f / 255.0f, fb = 239.0f / 255.0f;
  ajustes_acento(&ar, &ag, &ab);
  pthread_mutex_lock(&trava);
  if (n == 0) {
    TxtLinha t = txt_linha(TXT_ILHA_SUB, i18n("Nada por enquanto."), 243, 242, 239, 255);
    txt_desenhar_alpha(t, x + AVL_PADX, y0, a * 0.5f);
  }
  { float y = y0;
  for (i = 0; i < n; i++) {
    const Aviso *av = &itens[i];
    int f = (i == focoLinha);
    int expande = f && av->tipo == AV_CANAL;
    float tx = x + AVL_PADX + 52.0f + 18.0f, tw = w - (tx - x) - AVL_PADX;
    float rowH;
    larguraTextoAviso = tw;
    rowH = expande ? alturaCanalFoco : alturaAvisoF(i, f);
    GfxRect row = { x, y, w, rowH };
    const char *acao = NULL;
    if (focar) ponteiro_alvo_faixa(x, y, w, rowH, clipY0, clipY1, focar, NULL, i, 0);
    if (f) {
      if (gfx_foco_placa(row, 22.0f / row.h, 1.0f, a)) { /* Shared accent plate. */ }
      else if (ajustes_vidro()) gfx_cor(row, 22.0f / row.h, 1, 1, 1, .12f * a);
      else {
        gfx_rect((GfxRect){ row.x - 14.0f, row.y - 2.0f, row.w + 28.0f, row.h + 30.0f }, 0,
                 GFX_SOMBRA, 1.0f, 0, 0, 0.5f, 0, 0, 0, .30f * a);
        gfx_cor(row, 22.0f / row.h, .169f, .176f, .204f, a);
      }
    }
    // O disco do icone: branco a 8 % (o ".dsc" do mockup), o icone a 85 %.
    if (ajustes_vidro()) gfx_cor((GfxRect){ x + AVL_PADX, y + 18.0f, 52.0f, 52.0f }, 0.5f, 1, 1, 1, .08f * a);
    else gfx_cor((GfxRect){ x + AVL_PADX, y + 18.0f, 52.0f, 52.0f }, 0.5f, .141f, .149f, .173f, a);
    gfx_icone((GfxRect){ x + AVL_PADX + 13.0f, y + 31.0f, 26.0f, 26.0f }, icone(av->tipo),
              fr * .85f, fg * .85f, fb * .85f, a);
    // NOVO = um ponto na cor de acento colado ao icone, e nao uma pilula com
    // palavra: a palavra competia com o titulo e o ponto e o vocabulario que
    // a aba Social ja usa para "qual delas e nova". Agora tambem no foco: a
    // superficie do foco nao e mais o acento, entao o ponto nao some nela.
    if (!av->visto) gfx_cor((GfxRect){ x + AVL_PADX + 40.0f, y + 16.0f, 14.0f, 14.0f }, 0.5f, ar, ag, ab, a);
    { TxtLinha t = txt_linha_corta(TXT_ILHA_NOME, av->titulo, f ? 255 : 243, f ? 255 : 242,
                                   f ? 255 : 239, 255, tw);
      txt_desenhar_alpha(t, tx, y + 18.0f, a * (f ? 1.0f : 0.88f)); }
    if (expande) {
      float h = txt_bloco(TXT_ILHA_SUB, av->texto, 243, 242, 239, tx, y + 51.0f, tw, 25.0f, a * 0.62f,
                          AVL_LINHAS_CANAL);
      float nova = 51.0f + h + 4.0f + 18.0f + 18.0f;   // em foco: sempre a linha de acao
      if (nova < alturaAviso(i)) nova = alturaAviso(i);
      alturaCanalFoco = nova;
    }
    else txt_bloco(TXT_ILHA_SUB, av->texto, 243, 242, 239, tx, y + 51.0f, tw, 25.0f, a * 0.62f, 2);
    switch (av->tipo) {
      case AV_REC:    acao = i18n("OK abre Salvos"); break;
      case AV_PEDIDO: acao = i18n("OK abre Amigos para aceitar"); break;
      case AV_AGENDA: acao = i18n("OK abre o título"); break;
      case AV_UPDATE: acao = i18n("OK abre a atualização"); break;
      case AV_CRASH:  acao = envioEstado == 1 ? i18n("Enviando…") : envioEstado == 2 ? i18n("Registro enviado. Obrigado.")
                           : envioEstado == 3 ? i18n("Não foi possível enviar. OK tenta de novo.") : i18n("OK envia o registro"); break;
      default: break;
    }
    // Em foco, a acao ganha o segundo gesto: segurar OK dispensa (o menu na
    // aba Avisos de Salvos, direto no painel proprio da central).
    { char linhaAcao[200];
      if (f) snprintf(linhaAcao, sizeof linhaAcao, acao ? "%s  ·  %s" : "%s%s",
                      acao ? acao : "", i18n("Segure OK para dispensar"));
      else snprintf(linhaAcao, sizeof linhaAcao, "%s", acao ? acao : "");
      if (linhaAcao[0]) {
        TxtLinha t = txt_linha_corta(TXT_ILHA_HORA, linhaAcao, 243, 242, 239, 255, tw);
        txt_desenhar_alpha(t, tx, y + rowH - 18.0f - 18.0f, a * (f ? 0.62f : 0.38f));
      } }
    y += rowH;
  }
  // DISPENSAR TODOS: a mesma linha da ilha, mais baixa, com o X no disco.
  if (n > 0) {
    int f = focoLinha == n;
    GfxRect row = { x, y, w, AVL_TODOS_H };
    if (focar) ponteiro_alvo_faixa(x, y, w, AVL_TODOS_H, clipY0, clipY1, focar, NULL, n, 0);
    if (f) {
      if (gfx_foco_placa(row, 22.0f / row.h, 1.0f, a)) { /* Shared accent plate. */ }
      else if (ajustes_vidro()) gfx_cor(row, 22.0f / row.h, 1, 1, 1, .12f * a);
      else gfx_cor(row, 22.0f / row.h, .169f, .176f, .204f, a);
    }
    if (ajustes_vidro()) gfx_cor((GfxRect){ x + AVL_PADX, y + 16.0f, 52.0f, 52.0f }, 0.5f, 1, 1, 1, .08f * a);
    else gfx_cor((GfxRect){ x + AVL_PADX, y + 16.0f, 52.0f, 52.0f }, 0.5f, .141f, .149f, .173f, a);
    gfx_icone((GfxRect){ x + AVL_PADX + 13.0f, y + 29.0f, 26.0f, 26.0f }, "aj_x",
              fr * .85f, fg * .85f, fb * .85f, a);
    { TxtLinha t = txt_linha_corta(TXT_ILHA_NOME, i18n("Dispensar todos"), 243, 242, 239, 255,
                                   w - 2.0f * AVL_PADX - 70.0f);
      txt_desenhar_alpha(t, x + AVL_PADX + 70.0f, y + (AVL_TODOS_H - (float)t.h) * 0.5f, a * (f ? 1.0f : 0.72f)); }
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
  if (!ha) {
    // A ultima linha: "Dispensar todos". A lista fica (vazia) no hospedeiro.
    if (linha == avisos_lista_n() && linha > 0) avisos_dispensar_todos();
    return 0;
  }
  switch (a.tipo) {
    case AV_REC:    pediuCodigo = AVISOS_ABRIR_SALVOS; return 1;
    // 2 = o painel fica aberto (so troca de aba); a central fecha como sempre.
    case AV_PEDIDO: pediuCodigo = AVISOS_ABRIR_AMIGOS; return 2;
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

// A ESTREIA AINDA NAO LIDA mais recente (ilhacart.c poe na ilha). Os itens
// entram no fim da lista, entao o mais novo e o de indice maior.
int avisos_estreia_pendente(char *id, size_t tamId, char *imdb, size_t tamImdb) {
  int i, achou = 0;
  pthread_mutex_lock(&trava);
  for (i = n - 1; i >= 0; i--)
    if (itens[i].tipo == AV_AGENDA && !itens[i].visto && itens[i].alvo[0] &&
        !avisodisp_sessao_tem(itens[i].id)) {   // "Depois" no modal: so na proxima sessao
      snprintf(id, tamId, "%s", itens[i].id);
      snprintf(imdb, tamImdb, "%s", itens[i].alvo);
      achou = 1;
      break;
    }
  pthread_mutex_unlock(&trava);
  return achou;
}

void avisos_marcar_visto(const char *id) {
  int i;
  if (!id || !id[0]) return;
  pthread_mutex_lock(&trava);
  for (i = 0; i < n; i++) if (!strcmp(itens[i].id, id)) itens[i].visto = 1;
  marcarVisto(id);
  pthread_mutex_unlock(&trava);
}

// Tudo lido: o hospedeiro chama ao fechar (o painel proprio e a aba).
void avisos_marcar_lidos(void) {
  int i;
  pthread_mutex_lock(&trava);
  for (i = 0; i < n; i++) { itens[i].visto = 1; marcarVisto(itens[i].id); }
  pthread_mutex_unlock(&trava);
}

// --- dispensar (06/10) ---------------------------------------------------------------
// Ver avisodisp.h. A recomendacao e a excecao: o item "rec" e a CONTAGEM das
// novas, e gravar "rec" calaria toda recomendacao futura. Dispensar responde a
// recomendacao (o mesmo que o "Dispensar" do modal sempre fez): a contagem zera
// e o item sai sozinho na proxima colheita; uma recomendacao nova o traz de
// volta. A estreia tambem marca o lembrete daquele episodio como avisado
// (agenda.c), para o cartao "Estreou hoje" nao abrir por ela — o lembrete
// continua ligado e o proximo episodio avisa.
void avisos_dispensar(const char *id) {
  char ch[96], imdb[24] = "";
  int i, tipo = -1;
  if (!id || !id[0]) return;
  snprintf(ch, sizeof ch, "%s", id);   // id pode apontar para dentro de itens[]
  pthread_mutex_lock(&trava);
  for (i = 0; i < n; i++) if (!strcmp(itens[i].id, ch)) {
    tipo = itens[i].tipo;
    snprintf(imdb, sizeof imdb, "%s", itens[i].alvo);
    break;
  }
  pthread_mutex_unlock(&trava);
  if (!strcmp(ch, "rec")) recomenda_marcar_vistas();
  else avisodisp_por(ch);
  if ((tipo == AV_AGENDA || (tipo < 0 && !strncmp(ch, "agenda:", 7))) && imdb[0]) agenda_marcar_avisado(imdb);
  pthread_mutex_lock(&trava);
  marcarVisto(ch);
  tirar(ch);
  pthread_mutex_unlock(&trava);
  { char chIlha[100];
    snprintf(chIlha, sizeof chIlha, "av:%s", ch);
    ilha_retirar(chIlha); }
  printf("[avisos] dispensado: %s\n", ch);
}

void avisos_dispensar_todos(void) {
  char ids[AV_MAX][72];
  int i, k;
  pthread_mutex_lock(&trava);
  for (k = 0; k < n; k++) snprintf(ids[k], sizeof ids[k], "%s", itens[k].id);
  pthread_mutex_unlock(&trava);
  for (i = 0; i < k; i++) avisos_dispensar(ids[i]);
  ilha_retirar(AV_ILHA_CHAVE);
}

int avisos_lista_dispensar(int linha) {
  char id[72] = "";
  pthread_mutex_lock(&trava);
  if (linha >= 0 && linha < n) snprintf(id, sizeof id, "%s", itens[linha].id);
  pthread_mutex_unlock(&trava);
  if (!id[0]) return 0;
  avisos_dispensar(id);
  return 1;
}

int avisos_lista_item(int linha, char *id, size_t tamId, char *titulo, size_t tamTit,
                      char *imdbLembrete, size_t tamImdb) {
  int ok = 0;
  if (imdbLembrete && tamImdb) imdbLembrete[0] = 0;
  pthread_mutex_lock(&trava);
  if (linha >= 0 && linha < n) {
    const Aviso *a = &itens[linha];
    if (id && tamId) snprintf(id, tamId, "%s", a->id);
    if (titulo && tamTit) snprintf(titulo, tamTit, "%s", a->tipo == AV_AGENDA && a->texto[0] ? a->texto : a->titulo);
    if (imdbLembrete && tamImdb && a->tipo == AV_AGENDA && a->alvo[0])
      snprintf(imdbLembrete, tamImdb, "%s", a->alvo);
    ok = 1;
  }
  pthread_mutex_unlock(&trava);
  // So oferece "Remover lembrete" quando ele ainda esta ligado.
  if (ok && imdbLembrete && imdbLembrete[0] && agenda_lembrete(imdbLembrete) != 1) imdbLembrete[0] = 0;
  return ok;
}

static void avisos_desenharCorpo_(Uint32 agora);
// Camada ampliada (escala.h): o corpo desenha na tela virtual.
void avisos_desenhar(Uint32 agora) {
  ESCALA_INI();
  avisos_desenharCorpo_(agora);
  ESCALA_FIM();
}
static void avisos_desenharCorpo_(Uint32 agora) {
  (void)agora;
  float a = anim_clamp(entrada, 0.0f, 1.0f), dx;
  if (toastPendente && !aberto && !cartao) {
    toastPendente = 0;
    anunciarNaIlha();
  }
  // A CENTRAL ABERTA E UMA CAMADA (app.c nao chama camada para ela): o que
  // esta atras nao recebe o ponteiro, nem durante a entrada.
  if (aberto) ponteiro_camada();
  if (a < 0.01f) { cartaoDesenhar(); return; }
  dx = (1.0f - a) * 80.0f;
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.45f * a);
  // O painel continua com luz ambiente suave; os cartoes e controles adotam
  // a nova superficie tonal sem retirar a profundidade aprovada da camada.
  { float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
    GfxRect p = { AVP_X + dx, 24.0f, AVP_W, NV_TELA_H - 48.0f };
    gfx_vidro_ou_solido(p, 28.0f / AVP_W, GFX_VIDRO_DENS_MODAL, a, 0.055f, 0.058f, 0.068f, 0.94f);
    if (!ajustes_vidro()) gfx_luz_canto(p, 28.0f / AVP_W, AVP_W * 0.9f, -AVP_W * 0.1f, AVP_W * 0.65f, ar, ag, ab, 0.22f * a); }
  { TxtLinha t = txt_linha(TXT_HEADLINE, i18n("Avisos"), 240, 242, 248, 255);
    txt_desenhar_alpha(t, AVP_X + dx + AVP_MARG, 64.0f, a); }
  txt_bloco(TXT_CAPTION, i18n("Recomendações, estreias, versões novas, avisos de quem faz o app e o que aconteceu com ele."),
            150, 153, 162, AVP_X + dx + AVP_MARG, 118.0f, AVP_W - 2 * AVP_MARG, 28.0f, a, 2);
  gfx_recorte(AVP_X + dx, AVP_TOPO - 8.0f, AVP_W, NV_TELA_H - 80.0f - AVP_TOPO + 8.0f);
  { float areaH = NV_TELA_H - 80.0f - AVP_TOPO;
    float fim = avisos_lista_y(foco, foco) + avisos_lista_altura_linha(foco, foco);
    float alvo = fim > areaH ? fim - areaH : 0.0f;
#ifdef NV_TOUCH_UI
    int nl = avisos_lista_linhas();
    float total = nl > 0 ? avisos_lista_y(nl - 1, foco) + avisos_lista_altura_linha(nl - 1, foco) : 0;
    toquerol_vincular(&toqueAvisos, (GfxRect){AVP_X + dx, AVP_TOPO, AVP_W, areaH}, gfx_escala(),
                     0, total - areaH, 1, &rol);
    if (aberto && !cartao && a > .99f) ponteiro_rolagem(toqueAvisosRolar);
    if (!toquerol_livre(&toqueAvisos))
#endif
    rol += (alvo - rol) * 0.25f; }
  // A linha da ilha tem 22 de recuo proprio: a caixa dela sai 22 para fora,
  // e o texto continua na prumada do titulo do painel.
  // PONTEIRO (#99): as linhas so com o painel assentado, recortadas a janela
  // da lista, com o focar DA CENTRAL.
  avisos_lista_desenhar_ptr(AVP_X + dx + AVP_MARG - 22.0f, AVP_TOPO - rol, AVP_W - 2 * AVP_MARG + 44.0f, a, foco,
                            aberto && !cartao && a > 0.99f ? ponteiroLinha : NULL,
                            AVP_TOPO - 8.0f, NV_TELA_H - 80.0f);
  gfx_sem_recorte();
  { TxtLinha t = txt_linha_corta(TXT_CAPTION2, i18n("↑ ↓ escolher · OK agir · Voltar fecha e marca tudo como lido"),
                                 140, 144, 154, 255, AVP_W - 2 * AVP_MARG);
    txt_desenhar_alpha(t, AVP_X + dx + AVP_MARG, NV_TELA_H - 62.0f, a * 0.85f); }
  cartaoDesenhar();
}

// --- envio manual (Ajustes) ---------------------------------------------------
int avisos_envio_estado(void) { return envioEstado; }
// PASSO DO ENVIO AUTOMATICO, do laco principal. Com o ajuste ligado: uma vez
// por sessao, o registro da sessao ANTERIOR (a que acabou, inclusive a que caiu),
// com no maximo 64 KB. Nunca dois envios ao mesmo tempo, e nunca por cima de um
// envio manual em curso.
void avisos_envio_auto_passo(Uint32 agora) {
  static int anteriorFeito;
  // "O registro fica guardado e sai assim que a rede voltar" (painel de envio,
  // TV sem internet): o envio manual que caiu por falta de rede sai sozinho
  // no primeiro quadro com a rede de volta, com ou sem o envio automatico.
  if (envPendenteRede && envioEstado != 1 && !rede_saude_offline()) {
    envPendenteRede = 0;
    avisos_enviar_registro_atual();
    return;
  }
  if (!ajustes_envio_auto() || !NV_REC_URL[0] || envioEstado == 1) return;
  if (!anteriorFeito) {
    anteriorFeito = 1;
    (void)agora;
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
  // UMA VEZ POR SESSAO (#203, dono). O registro desta sessao NAO sobe mais a
  // cada poucos minutos: o log da sessao que acabou e o que sobe, uma vez, no
  // proximo arranque (acima) — fim limpo, queda e TV desligada caem todos nesse
  // caminho. "Enviar registro" (Ajustes) e o recibo de queda continuam.
}

void avisos_enviar_registro_atual(void) {
  if (envioEstado == 1) return;
  envQuando = time(NULL);
  envCodigo[0] = 0; envHttp = 0;
  if (!NV_REC_URL[0]) { envMotivo = AVISOS_ENVIO_INDISPONIVEL; envioEstado = 3; return; }
  // Sem internet (redesaude.h) nem tenta: diz o porque e espera a rede voltar.
  if (rede_saude_offline()) {
    envMotivo = AVISOS_ENVIO_OFFLINE; envPendenteRede = 1; envioEstado = 3;
    { FILE *f; const char *arq = registro_arquivo();
      envBytes = 0; envLinhas = 0;
      if (arq && (f = fopen(arq, "rb")) != NULL) {
        fseek(f, 0, SEEK_END); envBytes = ftell(f); fclose(f);
        if (envBytes > AV_REGISTRO_MAX) envBytes = AV_REGISTRO_MAX;
      } }
    return;
  }
  envMotivo = 0; envPendenteRede = 0;
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

int avisos_envio_info(AvisosEnvio *o) {
  char *s;
  memset(o, 0, sizeof *o);
  o->disponivel = NV_REC_URL[0] != 0;
  pthread_mutex_lock(&trava);
  o->estado = envioEstado; o->motivo = envMotivo; o->http = envHttp;
  o->bytes = envBytes; o->linhas = envLinhas; o->quando = envQuando;
  o->autoQuando = envAutoQuando; o->autoHttp = envAutoHttp;
  o->pendenteRede = envPendenteRede;
  snprintf(o->codigo, sizeof o->codigo, "%s", envCodigo);
  pthread_mutex_unlock(&trava);
  o->autoProximoMs = envAutoProximo;
  s = dados_ler("registro-codigo.txt");
  if (s) {
    long t = 0;
    char c[8] = "";
    if (sscanf(s, "%7s %ld", c, &t) == 2 && strlen(c) == 6) {
      snprintf(o->ultimoCodigo, sizeof o->ultimoCodigo, "%s", c);
      o->ultimoCodigoQuando = (time_t)t;
    }
    free(s);
  }
  return o->estado;
}

#ifdef AVISOS_TESTE_ENVIO
// A queda da sessao anterior na ilha, como o arranque a anunciaria.
void avisos_teste_queda(const char *texto) {
  Aviso a;
  memset(&a, 0, sizeof a);
  snprintf(a.id, sizeof a.id, "teste:crash");
  a.tipo = AV_CRASH;
  snprintf(a.titulo, sizeof a.titulo, "%s", i18n("O app fechou sozinho"));
  snprintf(a.texto, sizeof a.texto, "%s", texto);
  anunciarItem(&a);
}
void avisos_teste_envio(int estado, int motivo, int http, const char *codigo, long bytes, int linhas) {
  envioEstado = estado; envMotivo = motivo; envHttp = http; envBytes = bytes; envLinhas = linhas;
  envQuando = time(NULL);
  snprintf(envCodigo, sizeof envCodigo, "%s", codigo ? codigo : "");
  envPendenteRede = motivo == AVISOS_ENVIO_OFFLINE;
}
void avisos_teste_envio_auto(long haSeg, int http) { envAutoQuando = time(NULL) - haSeg; envAutoHttp = http; envAutoProximo = SDL_GetTicks() + 240000u; }
#endif
