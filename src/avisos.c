#include "avisos.h"
#include "dados.h"
#include "rede.h"
#include "js.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "idioma.h"
#include "ajustes.h"
#include "recomenda.h"
#include "atualizacao.h"
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
#define AV_CANAL_URL "https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/master/avisos.json"
#define AV_CANAL_INTERVALO_MS (30u * 60u * 1000u)
#define AV_MAX        40
#define AV_VISTOS_ARQ "avisos-vistos.txt"
#define AV_MARCA_ARQ  "sessao-viva.txt"
#define AV_LOG_ANTERIOR "/tmp/nuvio-anterior.log"
#define AV_REGISTRO_MAX (200 * 1024)
#define AV_TOAST_MS   6000.0f

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
static int   envioEstado;       // 0 nada, 1 enviando, 2 ok, 3 falhou
static pthread_t fioEnvio;

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
static void marcaGravar(void) {
  char buf[96];
  time_t t = time(NULL);
  struct tm tmv;
  localtime_r(&t, &tmv);
  snprintf(buf, sizeof buf, "%s %04d-%02d-%02d %02d:%02d\n", NV_VERSAO,
           tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday, tmv.tm_hour, tmv.tm_min);
  dados_gravar(AV_MARCA_ARQ, buf);
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

static void *enviarRegistro(void *u) {
  static char aut[2200], via[40], chave[160];
  const char *cab[5];
  char *texto = NULL, *corpo, *resp;
  size_t nTexto = 0;
  int status = 0;
  (void)u;
  { FILE *f = fopen(AV_LOG_ANTERIOR, "rb");
    if (f) {
      long tam;
      fseek(f, 0, SEEK_END); tam = ftell(f);
      if (tam > AV_REGISTRO_MAX) fseek(f, tam - AV_REGISTRO_MAX, SEEK_SET); else rewind(f);
      texto = malloc(AV_REGISTRO_MAX + 1);
      if (texto) { nTexto = fread(texto, 1, AV_REGISTRO_MAX, f); texto[nTexto] = 0; }
      fclose(f);
    } }
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
             crashQuando, esc);
    free(esc); }
  free(texto);
  { char url[300];
    snprintf(url, sizeof url, "%s/v1/registro", NV_REC_URL);
    resp = rede_postar_st(url, 30, cab, corpo, &status); }
  free(corpo);
  free(resp);
  envioEstado = (status >= 200 && status < 300) ? 2 : 3;
  printf("[avisos] registro enviado: HTTP %d\n", status);
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
  if (m) {
    char v[24] = "";
    // "1.3.1 2026-09-19 18:40"
    sscanf(m, "%23s %39[^\n]", v, crashQuando);
    crashDetectado = 1;
    free(m);
    printf("[avisos] a sessao anterior (%s, %s) nao se despediu: marca presente\n", v, crashQuando);
    fflush(stdout);
    { char id[72], tit[160], txt[420];
      snprintf(id, sizeof id, "crash:%s", crashQuando);
      snprintf(tit, sizeof tit, "%s", i18n("O app fechou sozinho"));
      snprintf(txt, sizeof txt, i18n("Em %s o Nuvio parou sem avisar. Se quiser, envie o registro daquela sessão para ajudar a encontrar a causa."), crashQuando);
      pthread_mutex_lock(&trava);
      toast(por(id, AV_CRASH, tit, txt, NULL));
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
      novos += por("demo:rec", AV_REC, i18n("Recomendação de amigo"),
                   "Gustavo recomendou \"The Gentlemen\": \"vale cada minuto\". Abra Salvos para ver.", NULL);
      novos += por("demo:agenda", AV_AGENDA, i18n("Episódio novo"),
                   "Outlander — T7E9 · Unfinished Business", "tt3006802");
      novos += por("demo:update", AV_UPDATE, i18n("Atualização disponível"),
                   "Versão 1.3.2 disponível. Você está na 1.3.1.", "1.3.2");
      novos += por("demo:canal", AV_CANAL, "Guia de TV demorando",
                   "Na 1.3.1 o guia espera a rede a cada abertura. A 1.3.2 corrige. — Henrique", NULL);
      novos += por("demo:crash", AV_CRASH, i18n("O app fechou sozinho"),
                   "Em 2026-09-19 18:57 o Nuvio parou sem avisar. Se quiser, envie o registro daquela sessão para ajudar a encontrar a causa.", NULL);
      pthread_mutex_unlock(&trava);
      toast(novos);
    } }
}

void avisos_encerrar(void) {
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
  if (agora - ultColheita > 2000) { ultColheita = agora; colherLocais(); }
  if (agora >= canalProximo && !canalVivo) {
    canalVivo = 1;
    canalProximo = agora + AV_CANAL_INTERVALO_MS;
    if (pthread_create(&fioCanal, NULL, fioCanalFn, NULL) == 0) pthread_detach(fioCanal);
    else canalVivo = 0;
  }
  { float alvo = (toastAte > 0.0f && (float)agora < toastAte && !aberto) ? 1.0f : 0.0f;
    toastA = anim_mola(toastA, alvo, dt, NV_MOLA_TELA);
    if (alvo == 0.0f && toastA < 0.01f && toastAte > 0.0f && (float)agora >= toastAte) { toastAte = 0.0f; toastN = 0; } }
  if (vistosSujos && !aberto) vistosGravar();
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

static void desenharToast(void) {
  // UMA LINHA, sem icone grande: "1 aviso novo · AZUL abre" com um ponto na
  // cor de acento. A primeira versao tinha um disco de 48 px e duas linhas e
  // pesava mais que o card ao lado (o dono: "deixa mais elegante").
  char txt[120], dica[40];
  TxtLinha t1, t2;
  float w, h = 58.0f, x, y, ar, ag, ab;
  if (toastA < 0.01f) return;
  ajustes_acento(&ar, &ag, &ab);
  snprintf(txt, sizeof txt, toastN == 1 ? i18n("%d aviso novo") : i18n("%d avisos novos"), toastN);
#ifdef __EMSCRIPTEN__
  snprintf(dica, sizeof dica, "%s", i18n("CH+ abre"));
#else
  snprintf(dica, sizeof dica, "%s", i18n("AZUL abre"));
#endif
  t1 = txt_linha(TXT_CAPTION, txt, 240, 242, 247, 255);
  t2 = txt_linha(TXT_CAPTION, dica, 150, 153, 162, 255);
  w = 24.0f + 10.0f + 14.0f + t1.w + 18.0f + t2.w + 24.0f;
  x = NV_TELA_W - 80.0f - w;
  y = NV_TELA_H - 72.0f - h + (1.0f - toastA) * 24.0f;
  gfx_cor((GfxRect){ x, y, w, h }, 0.5f, 0.106f, 0.110f, 0.122f, 0.94f * toastA);
  gfx_cor((GfxRect){ x + 24.0f, y + (h - 10.0f) * 0.5f, 10.0f, 10.0f }, 0.5f, ar, ag, ab, toastA);
  txt_desenhar_alpha(t1, x + 48.0f, y + (h - t1.h) * 0.5f, toastA);
  txt_desenhar_alpha(t2, x + 48.0f + t1.w + 18.0f, y + (h - t2.h) * 0.5f, toastA);
}

// A LISTA, desenhada dentro de qualquer caixa: o painel proprio usa, e a aba
// AVISOS do painel de Salvos tambem (pedido do dono: abrir quando quiser, sem
// depender do toast). `foco` e de quem chama; -1 = nenhuma linha em foco.
#define AVL_ROW 164.0f
float avisos_lista_altura(void) {
  pthread_mutex_lock(&trava);
  { float h = n > 0 ? (float)n * AVL_ROW : 60.0f; pthread_mutex_unlock(&trava); return h; }
}
int avisos_lista_n(void) { int k; pthread_mutex_lock(&trava); k = n; pthread_mutex_unlock(&trava); return k; }

void avisos_lista_desenhar(float x, float y0, float w, float a, int focoLinha) {
  float ar, ag, ab;
  int i;
  ajustes_acento(&ar, &ag, &ab);
  pthread_mutex_lock(&trava);
  if (n == 0) {
    TxtLinha t = txt_linha(TXT_CAPTION, i18n("Nada por enquanto."), 150, 153, 162, 255);
    txt_desenhar_alpha(t, x, y0, a);
  }
  for (i = 0; i < n; i++) {
    const Aviso *av = &itens[i];
    float y = y0 + (float)i * AVL_ROW;
    int f = (i == focoLinha);
    GfxRect row = { x, y, w, AVL_ROW - 10.0f };
    const char *acao = NULL;
    gfx_cor(row, 14.0f / row.h, NV_COR_FOCO_R, NV_COR_FOCO_G, NV_COR_FOCO_B, 0.34f * a);
    if (f) gfx_cor(row, 14.0f / row.h, ar, ag, ab, a);
    gfx_cor((GfxRect){ x + 20.0f, y + 22.0f, 52.0f, 52.0f }, 0.5f, f ? 0.11f : 0.16f, f ? 0.115f : 0.17f, f ? 0.13f : 0.20f, a);
    gfx_icone((GfxRect){ x + 32.0f, y + 34.0f, 28.0f, 28.0f }, icone(av->tipo),
              f ? 0.96f : 0.62f, f ? 0.96f : 0.80f, f ? 0.97f : 0.96f, a);
    // NOVO = um ponto na cor de acento colado ao icone, e nao uma pilula com
    // palavra: a palavra competia com o titulo e o ponto e o vocabulario que
    // a aba Social ja usa para "qual delas e nova".
    if (!av->visto && !f) gfx_cor((GfxRect){ x + 62.0f, y + 18.0f, 14.0f, 14.0f }, 0.5f, ar, ag, ab, a);
    { TxtLinha t = txt_linha_corta(TXT_BODY, av->titulo, f ? 20 : 240, f ? 21 : 241, f ? 25 : 245, 255, w - 116.0f);
      txt_desenhar_alpha(t, x + 92.0f, y + 16.0f, a); }
    if (f) txt_bloco(TXT_CAPTION, av->texto, 60, 62, 70, x + 92.0f, y + 50.0f, w - 116.0f, 27.0f, a, 2);
    else   txt_bloco(TXT_CAPTION, av->texto, 150, 153, 162, x + 92.0f, y + 50.0f, w - 116.0f, 27.0f, a, 2);
    switch (av->tipo) {
      case AV_REC:    acao = i18n("OK abre Salvos"); break;
      case AV_AGENDA: acao = i18n("OK abre o título"); break;
      case AV_UPDATE: acao = i18n("OK abre a atualização"); break;
      case AV_CRASH:  acao = envioEstado == 1 ? i18n("Enviando…") : envioEstado == 2 ? i18n("Registro enviado. Obrigado.")
                           : envioEstado == 3 ? i18n("Não foi possível enviar. OK tenta de novo.") : i18n("OK envia o registro"); break;
      default: break;
    }
    if (acao) {
      TxtLinha t = txt_linha(TXT_CAPTION2, acao, f ? 40 : 120, f ? 42 : 124, f ? 50 : 134, 255);
      txt_desenhar_alpha(t, x + 92.0f, y + row.h - 34.0f, a * 0.95f);
    }
  }
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
  if (toastPendente && !aberto) { toastPendente = 0; toastAte = (float)agora + AV_TOAST_MS; }
  desenharToast();
  if (a < 0.01f) return;
  dx = (1.0f - a) * 80.0f;
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.45f * a);
  gfx_cor((GfxRect){ AVP_X + dx, 0, AVP_W, NV_TELA_H }, 0.0f, 0.106f, 0.110f, 0.122f, 0.98f * a);
  { TxtLinha t = txt_linha(TXT_HEADLINE, i18n("Avisos"), 240, 242, 248, 255);
    txt_desenhar_alpha(t, AVP_X + dx + AVP_MARG, 64.0f, a); }
  txt_bloco(TXT_CAPTION, i18n("Recomendações, estreias, versões novas, avisos de quem faz o app e o que aconteceu com ele."),
            150, 153, 162, AVP_X + dx + AVP_MARG, 118.0f, AVP_W - 2 * AVP_MARG, 28.0f, a, 2);
  gfx_recorte(AVP_X + dx, AVP_TOPO - 8.0f, AVP_W, NV_TELA_H - 80.0f - AVP_TOPO + 8.0f);
  { float areaH = NV_TELA_H - 80.0f - AVP_TOPO;
    float alvo = (foco + 1) * AVL_ROW > areaH ? (foco + 1) * AVL_ROW - areaH : 0.0f;
    rol += (alvo - rol) * 0.25f; }
  avisos_lista_desenhar(AVP_X + dx + AVP_MARG, AVP_TOPO - rol, AVP_W - 2 * AVP_MARG, a, foco);
  gfx_sem_recorte();
  { TxtLinha t = txt_linha_corta(TXT_CAPTION2, i18n("↑ ↓ escolher · OK agir · Voltar fecha e marca tudo como lido"),
                                 140, 144, 154, 255, AVP_W - 2 * AVP_MARG);
    txt_desenhar_alpha(t, AVP_X + dx + AVP_MARG, NV_TELA_H - 62.0f, a * 0.85f); }
}
