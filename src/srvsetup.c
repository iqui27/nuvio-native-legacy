// Approved personal-server Library setup. Network work belongs to one cancellable worker.
#include "srvsetup.h"
#include "srvprefs.h"
#include "servidores.h"
#include "biblioteca.h"
#include "ajustes.h"
#include "teclado.h"
#include "perfis.h"
#include "idioma.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "qr.h"
#include "js.h"
#include "jsraiz.h"
#include "jsw.h"
#include "dados.h"
#include "descoberta.h"
#include "fileiras.h"
#define NV_TXT_TELA
#include "tela21.h"
#include "botoes.h"
#include "revela.h"
#include "teclavoltar.h"
#include "ponteiro.h"
#include "focus.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { CHOOSE, DISCOVER, ADDRESS, LOGIN, USERS, PASSWORD, CODE, CONNECT_CODE, CONNECT_PASSWORD,
       CONNECT_SERVERS, PLEX_SERVERS, LIBRARIES, READY, MANAGE, REMOVE, ERROR_SCREEN, ROWS };
enum { JOB_DISCOVER = 1, JOB_USERS, JOB_CONNECT_PIN, JOB_CONNECT_PASSWORD, JOB_CONNECT_SERVERS,
       JOB_CONNECT_EXCHANGE, JOB_PLEX_SERVERS, JOB_PLEX_CHOOSE, JOB_PLEX_MANUAL, JOB_LIBRARIES, JOB_CATALOG, JOB_ROW_CONTENT };
static const char *providers[] = { "Plex", "Jellyfin", "Emby" };
static const char *marks[] = { "deploy/app/art/marcas/plex.png", "deploy/app/art/marcas/jellyfin.png", "deploy/app/art/marcas/emby.png" };
static const char *prefix[] = { "plex_", "jellyfin_", "emby_" };
static const char *alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .,:;!?\"'()[]{}+-_=/*@&#%\\|~`$^<>";
static int opened, screen, provider, selected, profile, field, restartPin, enterManual;
// Steps the user walked through; Back reopens the top one. Automatic hops
// (code approved, login done) replace instead of pushing, so Back never lands
// on a step that would bounce forward again.
static struct { int screen, selected; } history[16];
static int nHistory, keepHistory, backward, pageDir = 1, freshSetup, libraryRetry;
static unsigned libraryVersion;
static int authReturned, navigationCount;
static GfxRect navigationRects[SRV_VISAO_MAX + 8];
static int navigationIds[SRV_VISAO_MAX + 8];
static float entry, page, pageSeconds, focusAnim[SRV_VISAO_MAX + 8], scroll, scrollVelocity, stepAnim = -1;
static Uint32 motionTime;
static float contentX, contentW;
static GfxMini previousPage;
static float bodyScroll, bodyVelocity, bodyTarget, userScroll, userVelocity, userTarget;
static int bodyDrawing;
static float lastButtonY, nextButtonX;
static float sy(float y) {
  return bodyDrawing ? ANC(386) + y - 386 - bodyScroll : ANC(y);
}
static float px(float x) { return contentX + (x - 200) * contentW / 1624; }
static float pw(float w) { return w * contentW / 1624; }
static float focusValue(int i) { return i >= 0 && i < SRV_VISAO_MAX + 8 ? focusAnim[i] : 0; }
static char username[128], password[256], addressText[512], message[192], code[24], qrUrl[640];
static int rowChoice, rowStyle, recoveryAuth, keyboardUpper;
enum { KEY_FIRST = 4, KEY_SHIFT = 44, KEY_DELETE, KEY_DONE };
static const char *inlineKeys(int secret) {
  return secret ? "1234567890qwertyuiopasdfghjkl@zxcvbnm._-" : "1234567890qwertyuiopasdfghjkl:zxcvbnm./-";
}
static void inlineKey(int id) {
  int secret = screen == PASSWORD, index = id - KEY_FIRST;
  char *value = secret ? password : addressText;
  size_t size = secret ? sizeof password : sizeof addressText, n = strlen(value);
  if (id == KEY_DONE) { selected = secret ? 2 : 1; return; }
  if (id == KEY_DELETE) {
    if (n) { do { n--; } while(n && ((unsigned char)value[n]&0xc0)==0x80); value[n]=0; }
  } else if (id == KEY_SHIFT) {
    if (secret) keyboardUpper = !keyboardUpper;
    else if (n + 8 < size) memcpy(value+n,"https://",9);
  } else if (index >= 0 && index < 40 && n + 1 < size) {
    char c=inlineKeys(secret)[index];
    if (keyboardUpper && c>='a' && c<='z') c -= 'a'-'A';
    value[n]=c; value[n+1]=0;
  }
}
static long long lastSync[3];
static int libraryProfile = -999;
static unsigned libraryCatalogRevision;
static int libraryProviderCounts[3];
static Uint32 codeSince;
static Qr qr;
static CatFileira libraries[SRV_VISAO_MAX];
static int nLibraries, checked[SRV_VISAO_MAX];
static struct {
  pthread_t thread;
  int alive, done, kind, result, provider, profile, n, cached;
  RedeGrupo *group;
  RedeJob *job;
  JfConta jf;
  PxConta px;
  unsigned generation;
  SrvRede servers[SRV_SETUP_MAX];
  SrvUsuario users[SRV_SETUP_MAX];
  EmConnectServidor connect[SRV_SETUP_MAX];
  PxServidor plex[PX_SERVIDORES_MAX];
  CatFileira libraries[SRV_VISAO_MAX];
  CatItem *items;
  char token[128], user[64], pin[24], input[512], username[128], password[256];
} worker;
static void wipe(void *p, size_t n) { volatile unsigned char *v = p; while (n--) *v++ = 0; }
static int connected(int p) { return p == 0 ? plex_conectado() : p == 1 ? jellyfin_conectado() : emby_conectado(); }
static const char *serverName(int p) { return p == 0 ? plex_servidor_nome() : p == 1 ? jellyfin_servidor_curto() : emby_servidor_curto(); }
static void cancelLogin(void) {
  if (connected(provider)) return;
  if (provider == 0) plex_cancelar_entrada(); else if (provider == 1) jellyfin_cancelar_entrada(); else emby_cancelar_entrada();
}
static void stopWorker(void) {
  if (!worker.alive) return;
  rede_job_cancelar(worker.job);
  // Cancellation does not join on the render thread. update reaps the finished worker.
}
static int cancelled(void) { return rede_job_estado(worker.job) != REDE_OK; }
static void delayPoll(void) {
  for (int i = 0; i < 20 && !cancelled(); i++) { struct timespec t = {0, 100000000}; nanosleep(&t, NULL); }
}
static void clearReply(RedeResposta *r) { if (r->corpo) wipe(r->corpo, r->n_corpo); rede_resposta_limpar(r); }
static void *work(void *unused) {
  (void)unused; int e = JF_OK; RedeResposta r;
  switch (worker.kind) {
    case JOB_CATALOG: {
      PxBiblioteca px[PX_FIL_MAX]; JfBiblioteca jf[JF_FIL_MAX];
      int nl = worker.provider == 0 ? px_bibliotecas(&worker.px, worker.job, px, PX_FIL_MAX) : jf_bibliotecas(&worker.jf, worker.job, jf, JF_FIL_MAX);
      worker.items = calloc(CAT_MAX, sizeof *worker.items); worker.n = 0;
      if (!worker.items) { e = JF_ERR_REDE; break; }
      if (nl < 0) { worker.cached=1; worker.n=servidores_cache_ler(worker.profile,worker.provider,worker.provider?worker.jf.servidorId:worker.px.servidorId,worker.items,CAT_MAX); e=worker.n?JF_OK:nl; break; }
      for (int i = 0; i < nl && !cancelled(); i++) {
        const char *id = worker.provider == 0 ? px[i].id : jf[i].id;
        if (!srv_biblioteca_selecionada(worker.provider ? worker.jf.bibliotecas : worker.px.bibliotecas, id)) continue;
        int offset = 0, total = 1;
        while (offset < total && worker.n < CAT_MAX && !cancelled()) {
          int max = CAT_MAX - worker.n; if (max > 100) max = 100;
          int got = worker.provider == 0 ? px_itens(&worker.px, worker.job, id, offset, max, worker.items + worker.n, max, &total) : jf_itens(&worker.jf, worker.job, id, offset, max, worker.items + worker.n, max, &total);
          if (got <= 0) { if (got < 0) e = got; break; }
          worker.n += got; offset += got;
        }
      }
      if (!cancelled() && worker.n == 0 && e != JF_OK) { worker.cached=1; worker.n=servidores_cache_ler(worker.profile,worker.provider,worker.provider?worker.jf.servidorId:worker.px.servidorId,worker.items,CAT_MAX); if(worker.n)e=JF_OK; }
      break;
    }
    case JOB_ROW_CONTENT: {
      worker.n = servidores_bibliotecas(worker.libraries, SRV_VISAO_MAX);
      int w = 0;
      for (int i = 0; i < worker.n; i++) {
        CatFileira f = worker.libraries[i];
        if (strncmp(f.chave, prefix[worker.provider], strlen(prefix[worker.provider])) ||
            !strcmp(f.tipo,"group") || !strcmp(f.tipo,"list") || strstr(f.chave,"_nextup")) continue;
        worker.libraries[w++] = f;
      }
      CatItem *items = calloc(8, sizeof *items);
      for (int kind = 0; items && kind < 2 && !cancelled(); kind++) {
        char key[96]; snprintf(key,sizeof key,"%s%s",prefix[worker.provider],kind?"playlists":"collections");
        int total=0,raw=0,n=servidores_pagina(key,0,8,items,8,&total,&raw);
        for (int i=0;i<n && w<SRV_VISAO_MAX;i++) {
          char tag[JFID_TAG+1],id[JFID_ITEM+1];
          const char *ip=worker.provider==0?JFID_PREFIXO_PLEX:worker.provider==1?JFID_PREFIXO:JFID_PREFIXO_EMBY;
          if (!jfid_partes_p(ip,items[i].imdb,tag,id)) continue;
          CatFileira *f=&worker.libraries[w++]; memset(f,0,sizeof *f);
          snprintf(f->chave,sizeof f->chave,"%s%s%s",prefix[worker.provider],kind?"list_":worker.provider==0?"group_":"",id);
          snprintf(f->titulo,sizeof f->titulo,"%s",items[i].titulo);
          snprintf(f->tipo,sizeof f->tipo,"%s",kind?"list":"group");
        }
      }
      free(items);
      if(w>0 && w<SRV_VISAO_MAX) {
        CatFileira first=worker.libraries[0],*recent=&worker.libraries[w++];memset(recent,0,sizeof *recent);
        snprintf(recent->chave,sizeof recent->chave,"%srecent_%s",prefix[worker.provider],first.chave+strlen(prefix[worker.provider]));
        snprintf(recent->titulo,sizeof recent->titulo,"%s · %s",i18n("Mais recentes"),first.titulo);
        snprintf(recent->tipo,sizeof recent->tipo,"%s",first.tipo);
      }
      worker.n=w; break;
    }
    case JOB_LIBRARIES: {
      PxBiblioteca px[PX_FIL_MAX]; JfBiblioteca jf[JF_FIL_MAX];
      worker.n = worker.provider == 0 ? px_bibliotecas(&worker.px, worker.job, px, PX_FIL_MAX) : jf_bibliotecas(&worker.jf, worker.job, jf, JF_FIL_MAX);
      if (worker.n < 0) { e = worker.n; break; }
      for (int i = 0; i < worker.n; i++) {
        CatFileira *f = &worker.libraries[i]; memset(f, 0, sizeof *f);
        snprintf(f->chave, sizeof f->chave, "%s%s", prefix[worker.provider], worker.provider == 0 ? px[i].id : jf[i].id);
        snprintf(f->titulo, sizeof f->titulo, "%s", worker.provider == 0 ? px[i].nome : jf[i].nome);
        snprintf(f->tipo, sizeof f->tipo, "%s", worker.provider == 0 ? px[i].tipo : jf[i].tipo);
      }
      break;
    }
    case JOB_DISCOVER: worker.n = srv_descobrir(worker.provider, worker.job, worker.servers, SRV_SETUP_MAX); break;
    case JOB_USERS: worker.n = srv_usuarios(&worker.jf, worker.job, worker.users, SRV_SETUP_MAX); if (worker.n < 0) e = worker.n; break;
    case JOB_PLEX_SERVERS:
      worker.n = px_recursos(&worker.px, worker.job, worker.plex, PX_SERVIDORES_MAX); if (worker.n < 0) e = worker.n; break;
    case JOB_PLEX_CHOOSE: e = px_escolher(&worker.px, worker.job, &worker.plex[worker.n]); break;
    case JOB_PLEX_MANUAL: e = px_setup_manual(&worker.px, worker.input, worker.job); break;
    case JOB_CONNECT_PASSWORD: {
      Jsw w; jsw_iniciar(&w); jsw_obj_ini(&w); jsw_cs(&w, "nameOrEmail", worker.username); jsw_cs(&w, "rawpw", worker.password); jsw_obj_fim(&w);
      e = em_connect_pedir("user/authenticate", jsw_texto_final(&w), NULL, worker.job, &r);
      if (!e) { if (!em_connect_ler_conta(r.corpo, worker.token, sizeof worker.token, worker.user, sizeof worker.user)) e = JF_ERR_FORMATO; clearReply(&r); }
      if (w.p) wipe(w.p, w.n); jsw_livre(&w); wipe(worker.password, sizeof worker.password); break;
    }
    case JOB_CONNECT_PIN: {
      Jsw w; jsw_iniciar(&w); jsw_obj_ini(&w); jsw_cs(&w, "deviceId", worker.jf.dispositivoId); jsw_obj_fim(&w);
      e = em_connect_pedir("pin", jsw_texto_final(&w), NULL, worker.job, &r); jsw_livre(&w);
      if (e) break;
      char pin[24] = "", device[128] = "";
      js_texto_raiz(r.corpo, "Pin", pin, sizeof pin); clearReply(&r);
      if (!pin[0] || strspn(pin, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") != strlen(pin)) { e = JF_ERR_FORMATO; break; }
      snprintf(device, sizeof device, "%s", worker.jf.dispositivoId);
      if (strspn(device, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_") != strlen(device)) { e = JF_ERR_FORMATO; break; }
      // Only display code is published while the worker polls. No credential leaves the worker.
      snprintf(worker.pin, sizeof worker.pin, "%s", pin); __atomic_store_n(&worker.done, 2, __ATOMIC_RELEASE);
      unsigned start = SDL_GetTicks();
      for (;;) {
        if (cancelled()) { e = JF_ERR_CANCELADO; break; }
        if (SDL_GetTicks() - start > 300000) { e = JF_ERR_EXPIRADO; break; }
        delayPoll(); char path[256]; snprintf(path, sizeof path, "pin?deviceId=%s&pin=%s", device, pin);
        e = em_connect_pedir(path, NULL, NULL, worker.job, &r);
        if (e) break;
        int confirmed = boolRaiz(r.corpo, NULL, "IsConfirmed", 0);
        clearReply(&r); if (!confirmed) continue;
        jsw_iniciar(&w); jsw_obj_ini(&w); jsw_cs(&w, "deviceId", device); jsw_cs(&w, "pin", pin); jsw_obj_fim(&w);
        e = em_connect_pedir("pin/authenticate", jsw_texto_final(&w), NULL, worker.job, &r); jsw_livre(&w);
        if (!e) { if (!em_connect_ler_conta(r.corpo, worker.token, sizeof worker.token, worker.user, sizeof worker.user)) e = JF_ERR_FORMATO; clearReply(&r); }
        break;
      }
      wipe(pin, sizeof pin); break;
    }
    case JOB_CONNECT_SERVERS:
      worker.n = em_connect_servidores(worker.token, worker.user, worker.job, worker.connect, SRV_SETUP_MAX); if (worker.n < 0) e = worker.n; break;
    case JOB_CONNECT_EXCHANGE: e = em_connect_trocar(&worker.jf, &worker.connect[worker.n], worker.user, worker.job); break;
  }
  worker.result = e; __atomic_store_n(&worker.done, 1, __ATOMIC_RELEASE); return NULL;
}
static int startWorker(int kind) {
  if (worker.alive) return 0;
  worker.kind = kind; worker.provider = provider; worker.profile = profile; worker.done = 0; worker.result = 0; worker.cached = 0;
  if (!worker.group) worker.group = rede_grupo_criar();
  worker.job = rede_job_criar(worker.group);
  if (!worker.job) return 0;
  if (provider == 0) worker.generation = px_setup_conta(&worker.px, &worker.profile);
  else worker.generation = jf_setup_conta(provider == 2, &worker.jf, &worker.profile);
  worker.alive = pthread_create(&worker.thread, NULL, work, NULL) == 0;
  if (!worker.alive) { rede_job_soltar(worker.job); worker.job = NULL; }
  return worker.alive;
}
static void change(int next) {
  if (opened && entry > .99f && !gfx_mini_ativo() &&
      !ajustes_animacoes_reduzidas() && gfx_mini_alvo(&previousPage, 1920, 1080)) {
    gfx_mini_comecar(&previousPage, 0, 0, 1);
    srvsetup_desenhar(SDL_GetTicks());
    gfx_mini_terminar();
  }
  int restore = -1;
  if (next != screen && !keepHistory) {
    int i = 0;
    while (i < nHistory && history[i].screen != next) i++;
    if (i < nHistory) { restore = history[i].selected; nHistory = i; }
    else if (nHistory < 16) {
      history[nHistory].screen = screen; history[nHistory++].selected = selected;
    }
  }
  pageDir = backward ? -1 : 1;
  screen = next; keyboardUpper = 0; message[0] = 0; libraryRetry = 0;
  authReturned = navigationCount = 0;
  // Focus the obvious next action: Connect for a known address, the inline
  // keys for a password whose user is already chosen.
  selected = restore >= 0 ? restore : next == ADDRESS ? (addressText[0] ? 1 : KEY_FIRST) : next == PASSWORD && username[0] ? KEY_FIRST : 0;
  if(next==LOGIN && !worker.alive) { worker.kind=0;worker.n=0; }
  bodyScroll = bodyVelocity = bodyTarget = userScroll = userVelocity = userTarget = 0;
  page = pageSeconds = scroll = scrollVelocity = 0;
  memset(focusAnim, 0, sizeof focusAnim);
}
static void replaceScreen(int next) { keepHistory = 1; change(next); keepHistory = 0; }
// Signed in: the login steps are spent, Back goes to where setup began.
static void connectedScreen(int next) { freshSetup = 1; if (nHistory > 1) nHistory = 1; replaceScreen(next); }
static void failed(int error) { recoveryAuth = error == JF_ERR_AUTH || error == JF_ERR_EXPIRADO; replaceScreen(ERROR_SCREEN); }
// A setup step that fails stays put and says why; only a saved connection
// that stops answering opens the offline/sign-in-again page.
static void stepError(const char *text, int focus) {
  if (!strcmp(message, i18n(text))) return;
  snprintf(message, sizeof message, "%s", i18n(text)); selected = focus;
}
static void syncTimes(void) {
  for (int i=0;i<3;i++) {
    int p; JfConta jf; PxConta px;
    if(i) jf_setup_conta(i==2,&jf,&p); else px_setup_conta(&px,&p);
    lastSync[i]=servidores_cache_instante(profile,i,i?jf.servidorId:px.servidorId);
    wipe(&jf,sizeof jf); wipe(&px,sizeof px);
  }
}
static void makeQR(const char *url) { snprintf(qrUrl, sizeof qrUrl, "%s", url); qr_gerar(&qr, url); }
static void keyboard(int f) {
  field = f;
  const char *label = f == 1 ? "Endereço" : f == 2 ? provider == 2 ? "Usuário do Emby" : "Usuário do Jellyfin" : "Senha";
  teclado_contexto("Biblioteca"); teclado_apresentacao(1);
  teclado_abrir_com(label, "", f == 1 ? 400 : f == 2 ? 120 : 250, alphabet,
    f == 1 ? addressText : f == 2 ? username : NULL);
  if (f == 3) teclado_tipo(TECLADO_TIPO_SENHA);
}
static void enterCode(void) {
  cancelLogin(); code[0] = 0; codeSince = SDL_GetTicks();
  change(CODE);
  if (!provider) { makeQR("https://plex.tv/link"); if (!plex_entrar()) snprintf(message, sizeof message, "%s", i18n("Falhou")); }
  else { char url[640]; if (jellyfin_qc_url(url, sizeof url)) makeQR(url); if (!jellyfin_entrar_quick_connect()) { replaceScreen(LOGIN); stepError("Se Quick Connect estiver desativado, use usuário e senha.", 4); } }
}
static void connectCode(void) { change(CONNECT_CODE); codeSince = SDL_GetTicks(); makeQR("https://emby.media/pin"); startWorker(JOB_CONNECT_PIN); }
static void librariesLoad(void) {
  nLibraries = servidores_bibliotecas(libraries, SRV_VISAO_MAX);
  int w = 0;
  for (int i = 0; i < nLibraries; i++) if (!strncmp(libraries[i].chave, prefix[provider], strlen(prefix[provider])) &&
      strcmp(libraries[i].tipo, "group") && strcmp(libraries[i].tipo, "list") && !strstr(libraries[i].chave, "_nextup")) {
    libraries[w] = libraries[i]; checked[w++] = 1;
  }
  nLibraries = w;
  if (screen == LIBRARIES && !selected) selected = nLibraries;   // libraries arrive checked: Continue is next
  if (!worker.alive && screen == LIBRARIES) startWorker(JOB_LIBRARIES);
  else if (!worker.alive && screen == ROWS) startWorker(JOB_ROW_CONTENT);
}
void srvsetup_fechar(void) {
  if (opened) cancelLogin(); stopWorker(); opened = 0; field = 0;
  wipe(password, sizeof password); teclado_esquecer();
  if (!worker.alive) { wipe(worker.token, sizeof worker.token); wipe(worker.user, sizeof worker.user); wipe(worker.connect, sizeof worker.connect); wipe(worker.plex, sizeof worker.plex); }
}
int srvsetup_aberto(void) { return opened || entry > .002f; }
void srvsetup_abrir(int p) {
  if (opened) srvsetup_fechar();
  ajustes_definir_servidores(1);
  profile = perfis_ativo(); provider = p >= 0 && p < 3 ? p : 0;
  syncTimes(); recoveryAuth = freshSetup = 0;
  gfx_mini_liberar(&previousPage);
  entry = page = pageSeconds = 0; motionTime = 0; stepAnim = -1; nHistory = 0;
  opened = 1; rowChoice = rowStyle = 0; restartPin = enterManual = 0; username[0] = password[0] = addressText[0] = code[0] = qrUrl[0] = 0; qr.lado = 0;
  replaceScreen(p == -2 ? MANAGE : p < 0 ? CHOOSE : connected(provider) ? LIBRARIES : provider ? DISCOVER : CODE);
  if (screen == DISCOVER) startWorker(JOB_DISCOVER);
  else if (screen == CODE) enterCode(); else if (screen == LIBRARIES) librariesLoad();
}
static void discover(void) { cancelLogin(); change(DISCOVER); worker.n = 0; startWorker(JOB_DISCOVER); }
static void userLogin(void) { cancelLogin(); change(USERS); worker.n = 0; startWorker(JOB_USERS); }
static void back(void) {
  cancelLogin(); stopWorker(); wipe(password, sizeof password);
  if (!nHistory) { srvsetup_fechar(); return; }
  int previous = --nHistory, focus = history[previous].selected;
  keepHistory = backward = 1; change(history[previous].screen); keepHistory = backward = 0;
  selected = focus; authReturned = 1; restartPin = 0;
}
static void activate(void) {
  if ((screen == ADDRESS || screen == PASSWORD) && selected >= KEY_FIRST) { inlineKey(selected); return; }
  if (worker.alive && screen == DISCOVER) {
    if (selected==1) { stopWorker();change(ADDRESS); }
    else if (selected==(provider==2?3:2)) srvsetup_fechar();
    return;
  }
  if (worker.alive && screen != CODE && screen != CONNECT_CODE) return;
  if (worker.alive && screen == CONNECT_CODE && selected == 0) {
    snprintf(message, sizeof message, "%s", i18n("Carregando…")); return;
  }
  if (authReturned && screen == CODE && selected == 0) {
    if (!connected(provider)) enterCode(); else authReturned = 0;
    return;
  }
  if (screen == CODE && selected == 0) {
    snprintf(message, sizeof message, "%s", i18n("Carregando…")); return;
  }
  if (screen == LOGIN && (provider == 1 ? jellyfin_estado(NULL, 0) : emby_estado(NULL, 0)) == JF_EST_VERIFICANDO) return;
  switch (screen) {
    case CHOOSE:
      provider = selected; if (connected(provider)) { freshSetup = 0; change(LIBRARIES); librariesLoad(); }
      else if (!provider) enterCode(); else discover(); break;
    case MANAGE:
      if (selected < 3) { provider = selected; freshSetup = 0; change(connected(provider) ? LIBRARIES : provider ? DISCOVER : CODE); if (screen == LIBRARIES) librariesLoad(); else if (screen == DISCOVER) startWorker(JOB_DISCOVER); else enterCode(); }
      else if (selected == 3) change(CHOOSE);
      else { provider = selected - 4; change(REMOVE); } break;
    case DISCOVER:
      if (selected < worker.n && worker.n > 0) {
        snprintf(addressText, sizeof addressText, "%s", worker.servers[selected].url);
        if (!provider) { change(ADDRESS); }
        else if (provider == 1 ? jellyfin_definir_servidor(addressText) : emby_definir_servidor(addressText)) change(LOGIN);
        else stepError("Verifique o servidor", selected);
      } else if (selected == (worker.n > 0 ? worker.n : 0)) discover();
      else if (selected == (worker.n > 0 ? worker.n : 0)+1) change(ADDRESS);
      else if (provider == 2 && selected == (worker.n > 0 ? worker.n : 0) + 2) connectCode();
      else srvsetup_fechar(); break;
    case ADDRESS:
      if (!selected) keyboard(1);
      else if (selected == 1) {
        if (!addressText[0]) { selected = KEY_FIRST; break; }
        if (!provider) { if (connected(0)) { snprintf(worker.input, sizeof worker.input, "%s", addressText); startWorker(JOB_PLEX_MANUAL); } else { enterManual = 1; enterCode(); } }
        else if (provider == 1 ? jellyfin_definir_servidor(addressText) : emby_definir_servidor(addressText)) change(LOGIN);
        else stepError("Verifique o servidor", 0);
      } else discover(); break;
    case LOGIN:
      if(selected>=5 && selected-5<worker.n) { snprintf(username,sizeof username,"%s",worker.users[selected-5].nome);change(PASSWORD); }
      else if (selected == 1) { if (provider == 2) connectCode(); else enterCode(); }
      else if (selected == 2) discover();
      else if (selected == 3) change(ADDRESS);
      else if (selected == 4) { username[0] = 0; change(PASSWORD); keyboard(2); }
      else userLogin(); break;
    case USERS:
      if (selected < worker.n && worker.n > 0) { snprintf(username, sizeof username, "%s", worker.users[selected].nome); change(PASSWORD); }
      else if (selected == (worker.n > 0 ? worker.n : 0)) { username[0] = 0; change(PASSWORD); keyboard(2); }
      else if (provider == 1) enterCode(); else connectCode(); break;
    case PASSWORD: case CONNECT_PASSWORD:
      if (!selected) keyboard(2); else if (selected == 1) keyboard(3);
      else if (selected == 2) {
        if (!username[0]) { keyboard(2); break; }
        if (screen == CONNECT_PASSWORD) {
          snprintf(worker.username, sizeof worker.username, "%s", username); snprintf(worker.password, sizeof worker.password, "%s", password);
          if (startWorker(JOB_CONNECT_PASSWORD)) wipe(password, sizeof password);
        } else { authReturned=0;int ok = provider == 1 ? jellyfin_entrar_senha(username, password) : emby_entrar_senha(username, password); if (!ok) snprintf(message, sizeof message, "%s", i18n("Falhou")); }
      } else if (screen == CONNECT_PASSWORD) { change(LOGIN); }
      else if (provider == 1) enterCode();
      else connectCode(); break;
    case CODE:
      if (!selected) { /* Existing worker already polls; OK checks its published state on next update. */ }
      else if (selected == 1) enterCode(); else if (selected == 2) { cancelLogin(); if (provider) userLogin(); else discover(); }
      else { cancelLogin(); change(ADDRESS); } break;
    case CONNECT_CODE:
      if (selected == 1) { stopWorker(); code[0] = 0; restartPin = 1; }
      else if (selected == 2) { stopWorker(); change(CONNECT_PASSWORD); }
      break;
    case PLEX_SERVERS:
      if (selected < worker.n && worker.n > 0) {
        if (!worker.plex[selected].presente) { snprintf(message, sizeof message, i18n("%s está offline"), worker.plex[selected].nome); break; }
        worker.n = selected; startWorker(JOB_PLEX_CHOOSE);
      }
      else if (selected == (worker.n > 0 ? worker.n : 0)) discover(); else change(ADDRESS); break;
    case CONNECT_SERVERS:
      if (selected < worker.n && worker.n > 0) { worker.n = selected; startWorker(JOB_CONNECT_EXCHANGE); }
      else change(CONNECT_PASSWORD); break;
    case LIBRARIES:
      if (libraryRetry && selected == nLibraries) {
        if (startWorker(JOB_LIBRARIES)) { libraryRetry = 0; message[0] = 0; }
      }
      else if (selected < nLibraries) checked[selected] = !checked[selected];
      else if (selected == nLibraries) {
        int n = 0; for (int i = 0; i < nLibraries; i++) n += checked[i];
        if (!n) { snprintf(message, sizeof message, "%s", i18n("Escolha suas bibliotecas")); break; }
        // Keep library visibility in the existing per-profile Home row store.
        for (int i = 0; i < nLibraries; i++) {
          fil_registrar(libraries[i].chave, libraries[i].titulo, providers[provider], libraries[i].tipo, libraries[i].n);
        }
        fil_gravar_registro();
        JfConta jf; PxConta px; int p; unsigned g;
        if (!provider) g = px_setup_conta(&px, &p); else g = jf_setup_conta(provider == 2, &jf, &p);
        char *selection = provider ? jf.bibliotecas : px.bibliotecas; selection[0] = 0;
        for (int i = 0; i < nLibraries; i++) if (checked[i]) {
          size_t k = strlen(selection); const char *id = libraries[i].chave + strlen(prefix[provider]);
          if (k + strlen(id) + 2 < 256) snprintf(selection + k, 256 - k, "%s%s", k ? "\n" : "", id);
        }
        int ok = provider ? jf_setup_aceitar(provider == 2, &jf, p, g) : px_setup_aceitar(&px, p, g);
        wipe(&jf, sizeof jf); wipe(&px, sizeof px);
        if (ok) change(READY); else snprintf(message, sizeof message, "%s", i18n("Falhou"));
      } else back(); break;
    case READY:
      if (!selected) { srvsetup_fechar(); biblioteca_servidor_filtrar(provider); }
      else { change(ROWS); librariesLoad(); } break;
    case ROWS:
      if (selected < nLibraries) { rowChoice = selected; rowStyle = 0; }
      else if (selected == nLibraries) {
        if (nLibraries && rowChoice < nLibraries) {
          int types[FIL_TIPO_N]; const char *labels[FIL_TIPO_N];
          fil_registrar(libraries[rowChoice].chave,libraries[rowChoice].titulo,providers[provider],libraries[rowChoice].tipo,libraries[rowChoice].n);
          int count=fil_estilos(libraries[rowChoice].chave,types,labels,FIL_TIPO_N);
          int current=fil_tipo(libraries[rowChoice].chave);rowStyle=0;
          for(int i=0;i<count;i++)if(types[i]==current)rowStyle=i;
          rowStyle=count?(rowStyle+1)%count:0;
          if(count)fil_definir_tipo(libraries[rowChoice].chave,types[rowStyle]);
        }
      } else if ((selected == nLibraries+1 || selected == nLibraries+2) && nLibraries && rowChoice < nLibraries) {
        const char *key = libraries[rowChoice].chave;
        fil_registrar(key,libraries[rowChoice].titulo,providers[provider],libraries[rowChoice].tipo,libraries[rowChoice].n);
        int order=fil_servidor_ordem(key),hide=fil_servidor_ocultar_vistos(key);
        fil_definir_servidor(key,selected==nLibraries+1?!order:order,selected==nLibraries+2?!hide:hide);
      } else if (selected == nLibraries+3 && nLibraries && rowChoice < nLibraries) {
        fil_registrar(libraries[rowChoice].chave, libraries[rowChoice].titulo, providers[provider], libraries[rowChoice].tipo, libraries[rowChoice].n);
        fil_gravar_registro();
        for (int i = 0; i < fil_n(); i++) if (!strcmp(fil_chave(i), libraries[rowChoice].chave)) { int state; fil_adicionar(i, &state); break; }
        desc_repetir_silencioso(); snprintf(message, sizeof message, "%s", i18n("Adicionada à Home"));
      } else change(READY); break;
    case REMOVE:
      if (!selected) change(MANAGE);
      else { servidores_cache_apagar(profile,provider); if (!provider) plex_esquecer(); else if (provider == 1) jellyfin_esquecer(); else emby_esquecer(); desc_repetir_silencioso(); change(MANAGE); } break;
    case ERROR_SCREEN:
      if (!selected) {
        if (recoveryAuth) { if(provider==2) connectCode(); else enterCode(); }
        else { change(LIBRARIES); startWorker(JOB_LIBRARIES); }
      } else if (selected == 1) {
        if(recoveryAuth && provider) userLogin(); else change(ADDRESS);
      } else if(selected==2) { srvsetup_fechar(); biblioteca_servidor_filtrar(provider); }
      else change(MANAGE); break;
  }
}
static int targets(void) {
  if (worker.alive && screen == DISCOVER) return provider==2?4:3;
  if (worker.alive && screen != CODE && screen != CONNECT_CODE) return 0;
  switch (screen) {
    case CHOOSE: return 3;
    case MANAGE: return 7;
    case DISCOVER: return (worker.n > 0 ? worker.n : 0) + (provider == 2 ? 4 : 3);
    case USERS: return (worker.n > 0 ? worker.n : 0) + 2;
    case ADDRESS: return KEY_DONE + 1;
    case LOGIN: return 5+(worker.n>3?3:worker.n>0?worker.n:0);
    case ERROR_SCREEN: return recoveryAuth ? 3 : 4;
    case CODE: return provider ? 3 : 4;
    case PASSWORD: return KEY_DONE + 1;
    case CONNECT_PASSWORD: return 4;
    case CONNECT_CODE: return 3;
    case PLEX_SERVERS: return (worker.n > 0 ? worker.n : 0) + 2;
    case CONNECT_SERVERS: return (worker.n > 0 ? worker.n : 0) + 1;
    case LIBRARIES: return nLibraries + 2;
    case ROWS: return nLibraries + 5;
    default: return 2;
  }
}
void srvsetup_evento(const SDL_Event *e) {
  if (!opened) return;
  if (teclado_aberto()) { teclado_evento(e); return; }
  if (e->type != SDL_KEYDOWN || e->key.repeat) return;
  SDL_Keycode k = e->key.keysym.sym;
  if (nv_tecla_voltar(e)) back();
  else if (k == SDLK_LEFT || k == SDLK_UP || k == SDLK_RIGHT || k == SDLK_DOWN) {
    int total = screen==USERS || screen==DISCOVER || screen==PLEX_SERVERS || screen==CONNECT_SERVERS ? (!worker.alive && worker.n>0 ? worker.n : 0) :
      screen==LIBRARIES || screen==ROWS ? nLibraries : 0;
    int step = screen==USERS ? (k==SDLK_RIGHT)-(k==SDLK_LEFT) : (k==SDLK_DOWN)-(k==SDLK_UP);
    int adjacent = selected + step, visible = 0;
    for (int i=0;i<navigationCount;i++) if (navigationIds[i]==adjacent) visible=1;
    // Reveal the next item before geometry can skip it for a visible footer.
    if (step && selected<total && adjacent>=0 && adjacent<total && !visible) { selected=adjacent; return; }
    int current = -1;
    for (int i=0;i<navigationCount;i++) if (navigationIds[i]==selected) current=i;
    int next = focus_rect_nearest(navigationRects,navigationCount,current,
      (k==SDLK_RIGHT)-(k==SDLK_LEFT),(k==SDLK_DOWN)-(k==SDLK_UP));
    if (next>=0) selected=navigationIds[next];
  }
  else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) activate();
}
static int stepOf(int s) {
  return s==CHOOSE?0:s==LIBRARIES?3:s==READY?4:s==DISCOVER||s==ADDRESS||s==LOGIN||s==PLEX_SERVERS?1:2;
}
void srvsetup_atualizar(Uint32 now) {
  float dt = motionTime ? fminf((Uint32)(now - motionTime) / 1000.0f, .05f) : 1.0f / 60;
  motionTime = now;
  int reduced = ajustes_animacoes_reduzidas();
  entry = t21_entrada(entry, dt, opened);
  page = reduced ? 1 : anim_rampa(page, 1, dt, T21_ENTER_MS);
  // The step rail walks to the new step instead of relabelling in place.
  stepAnim = reduced || stepAnim < 0 ? stepOf(screen) : anim_mola(stepAnim, stepOf(screen), dt, NV_MOLA_TELA);
  if (page >= 1) gfx_mini_liberar(&previousPage);
  bodyScroll = anim_mola2_reduzida(&bodyVelocity, bodyScroll, bodyTarget, dt, NV_MOLA2_SCROLL, reduced);
  if (screen == USERS && !worker.alive && selected < worker.n) userTarget = (float)(selected / 3 * 3);
  userScroll = anim_mola2_reduzida(&userVelocity, userScroll, userTarget, dt, NV_MOLA2_SCROLL, reduced);
  pageSeconds += dt;
  for (int i = 0; i < SRV_VISAO_MAX + 8; i++) {
    float target = opened && selected == i ? 1 : 0;
    focusAnim[i] = reduced ? target : anim_mola(focusAnim[i], target, dt,
      target > focusAnim[i] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  }
  float targetScroll = 0;
  int total = screen == LIBRARIES || screen == ROWS ? nLibraries : worker.n;
  int visibleRows = screen==LIBRARIES?3:4;
  if ((screen == DISCOVER || screen == PLEX_SERVERS || screen == CONNECT_SERVERS || screen == LIBRARIES || screen == ROWS) && total > visibleRows) {
    int choice = selected < total ? selected : total - 1;
    targetScroll = fminf(fmaxf(choice-visibleRows+1,0),total-visibleRows);
  }
  scroll = anim_mola2_reduzida(&scrollVelocity, scroll, targetScroll, dt, NV_MOLA2_SCROLL, reduced);
  if (opened && profile != perfis_ativo()) srvsetup_fechar();
  if (worker.alive && worker.profile != perfis_ativo()) stopWorker();
  int done = __atomic_load_n(&worker.done, __ATOMIC_ACQUIRE);
  if (opened && worker.alive && done == 2 && screen == CONNECT_CODE && !code[0]) {
    snprintf(code, sizeof code, "%s", worker.pin);
  }
  if (worker.alive && done == 1) {
    pthread_join(worker.thread, NULL); worker.alive = 0;
    int valid = (opened || worker.kind == JOB_CATALOG) && worker.profile == perfis_ativo() && !cancelled() && provider == worker.provider;
    if(valid) {
      int p;JfConta jf;PxConta px;unsigned g=worker.provider?jf_setup_conta(worker.provider==2,&jf,&p):px_setup_conta(&px,&p);
      valid=p==worker.profile && g==worker.generation;wipe(&jf,sizeof jf);wipe(&px,sizeof px);
    }
    rede_job_soltar(worker.job); worker.job = NULL;
    if (valid) {
      if (worker.kind == JOB_CATALOG) {
        int p; JfConta jf; PxConta px;
        unsigned g = worker.provider ? jf_setup_conta(worker.provider == 2, &jf, &p) : px_setup_conta(&px, &p);
        if (p == worker.profile && g == worker.generation && (worker.result == JF_OK || worker.n>0)) {
          if (!worker.cached) servidores_cache_gravar(worker.profile,worker.provider,worker.provider?worker.jf.servidorId:worker.px.servidorId,worker.items,worker.n);
          syncTimes();
          int w = 0;
          for (int i = 0; i < worker.n; i++) if (cat_indice_por_imdb(worker.items[i].imdb) < 0) {
            int j; for (j = 0; j < w && strcmp(worker.items[j].imdb, worker.items[i].imdb); j++) {}
            if (j == w) worker.items[w++] = worker.items[i];
          }
          if (w) cat_acrescentar_lote(worker.items, w, NULL);
          biblioteca_alternar_lista(-1);
        }
        wipe(&jf, sizeof jf); wipe(&px, sizeof px);
      }
      else if (worker.result < 0) {
        const char *why = worker.result == JF_ERR_AUTH ? "Usuário ou senha incorretos" : worker.result == JF_ERR_EXPIRADO ? "o código expirou — OK pede outro" : "O servidor não respondeu dentro do prazo.";
        if (worker.kind == JOB_LIBRARIES && freshSetup) { libraryRetry = 1; stepError(why, nLibraries); }
        else if (worker.kind == JOB_LIBRARIES || worker.kind == JOB_ROW_CONTENT) failed(worker.result);
        else if (worker.kind == JOB_CONNECT_PIN) stepError(why, 1);
        else stepError(why, selected);
      }
      else if (worker.kind == JOB_ROW_CONTENT) {
        nLibraries = worker.n; memcpy(libraries,worker.libraries,sizeof *libraries*nLibraries); rowChoice = rowStyle = 0;
      }
      else if (worker.kind == JOB_USERS && screen == LOGIN && !selected) selected = worker.n > 0 ? 5 : 4;
      else if (worker.kind == JOB_LIBRARIES) {
        libraryRetry = 0; message[0] = 0;
        if (selected >= nLibraries) selected = worker.n;
        nLibraries = worker.n; memcpy(libraries, worker.libraries, sizeof *libraries * nLibraries);
        const char *selection = provider ? worker.jf.bibliotecas : worker.px.bibliotecas;
        for (int i = 0; i < nLibraries; i++) checked[i] = srv_biblioteca_selecionada(selection, libraries[i].chave + strlen(prefix[provider]));
      }
      else if (worker.kind == JOB_CONNECT_PIN || worker.kind == JOB_CONNECT_PASSWORD) { replaceScreen(CONNECT_SERVERS); startWorker(JOB_CONNECT_SERVERS); }
      else if (worker.kind == JOB_CONNECT_EXCHANGE) { if (jf_setup_aceitar(1, &worker.jf, worker.profile, worker.generation)) { connectedScreen(LIBRARIES); librariesLoad(); } }
      else if (worker.kind == JOB_PLEX_CHOOSE || worker.kind == JOB_PLEX_MANUAL) { if (px_setup_aceitar(&worker.px, worker.profile, worker.generation)) { connectedScreen(LIBRARIES); librariesLoad(); } }
    }
    if (worker.kind == JOB_CATALOG && worker.items) { free(worker.items); worker.items = NULL; }
    if (!worker.alive) { wipe(&worker.jf, sizeof worker.jf); wipe(&worker.px, sizeof worker.px); wipe(worker.password, sizeof worker.password); }
  }
  if (!opened) {
    if (!worker.alive) { wipe(worker.token, sizeof worker.token); wipe(worker.user, sizeof worker.user); wipe(worker.connect, sizeof worker.connect); wipe(worker.plex, sizeof worker.plex); }
    return;
  }
  if (restartPin && !worker.alive && screen == CONNECT_CODE) { restartPin = 0; codeSince = now; startWorker(JOB_CONNECT_PIN); }
  if (field) {
    teclado_atualizar(1.0f / 60, now);
    if (!teclado_aberto()) {
      int r = teclado_resultado(), f = field; field = 0;
      if (r == TECLADO_PRONTO) {
        snprintf(f == 1 ? addressText : f == 2 ? username : password, f == 1 ? sizeof addressText : f == 2 ? sizeof username : sizeof password, "%s", teclado_texto());
        teclado_esquecer(); selected = f == 1 ? 1 : f == 2 ? 1 : 2;
      }
    }
  }
  if (screen == CODE) {
    char detail[160] = "";
    int state = !provider ? plex_estado(detail, sizeof detail) : jellyfin_estado(detail, sizeof detail);
    if ((!provider && state == PX_EST_CODIGO) || (provider == 1 && state == JF_EST_QC_CODIGO)) snprintf(code, sizeof code, "%s", detail);
    if (connected(provider) && !authReturned) {
      if (!provider) {
        if (enterManual) { enterManual = 0; snprintf(worker.input, sizeof worker.input, "%s", addressText); connectedScreen(ADDRESS); startWorker(JOB_PLEX_MANUAL); }
        else { connectedScreen(PLEX_SERVERS); startWorker(JOB_PLEX_SERVERS); }
      }
      else { connectedScreen(LIBRARIES); librariesLoad(); }
    } else if (!connected(provider) && ((!provider && (state == PX_EST_ERRO || state == PX_EST_EXPIROU)) || (provider && (state == JF_EST_ERRO || state == JF_EST_EXPIROU)))) stepError(state==(provider?JF_EST_EXPIROU:PX_EST_EXPIROU)?"o código expirou — OK pede outro":"O servidor não respondeu dentro do prazo.", 1);
  }
  if (!authReturned && (screen == PASSWORD || screen == LOGIN) && connected(provider)) { connectedScreen(LIBRARIES); librariesLoad(); }
  if (screen == LIBRARIES && !nLibraries && !worker.alive && !message[0]) {
    snprintf(message, sizeof message, "%s", i18n("Não configurado"));
  }
  if (screen == PASSWORD) {
    int state = provider == 1 ? jellyfin_estado(NULL, 0) : emby_estado(NULL, 0);
    if (state == JF_EST_ERRO) snprintf(message, sizeof message, "%s", i18n("Usuário ou senha incorretos"));
  }
  if (screen == LOGIN && !authReturned) {
    if(!worker.alive && worker.kind!=JOB_USERS) { worker.n=0;startWorker(JOB_USERS); }
    int state = provider == 1 ? jellyfin_estado(NULL, 0) : emby_estado(NULL, 0);
    if (state == JF_EST_VERIFICANDO) snprintf(message, sizeof message, "%s", i18n("Conferindo o servidor…"));
    else if (state == JF_EST_ERRO) stepError("O servidor não respondeu dentro do prazo.", 2);
    else if (!strcmp(message, i18n("Conferindo o servidor…"))) message[0] = 0;
  }
}

static void text(int style, const char *s, float x, float y, int tone, float width) {
  txt_desenhar(txt_linha_corta(style, i18n(s), tone, tone, tone, 255, pw(width)), px(x), sy(y));
}
static void paragraph(const char *s, float x, float y, float width) {
  txt_bloco(TXT_ILHA_CORPO, i18n(s), 155, 155, 155, px(x), sy(y), pw(width), 32, 1, 3);
}
static void pointFocus(int i, int unused) { (void)unused; selected = i; }
static void pointActivate(int i, int unused) { (void)unused; selected = i; activate(); }
// Magic Remote target at the rect as drawn now: entry rise, body scroll and
// the horizontal clip of the user group are applied, so it follows the motion.
static float targetX0 = -1e9f, targetX1 = 1e9f;
static void pointerTarget(GfxRect r, int id) {
  if (!opened) return;
  float x0 = fmaxf(r.x, targetX0), x1 = fminf(r.x + r.w, targetX1);
  if (x1 <= x0) return;
  r.x = x0; r.w = x1 - x0;
  r.y += (1 - anim_suave(entry)) * NV_ENTRA_DY;
  if (id >= 0 && navigationCount < SRV_VISAO_MAX + 8) {
    navigationIds[navigationCount] = id;
    navigationRects[navigationCount++] = r;
  }
  if (bodyDrawing) ponteiro_alvo_faixa(r.x, r.y, r.w, r.h, ANC(386), ANC(978), pointFocus, pointActivate, id, 0);
  else {
    if (gfx_escala_txt() > 1) r.y -= bodyScroll;
    ponteiro_alvo(r.x, r.y, r.w, r.h, pointFocus, pointActivate, id, 0);
  }
}
static void panel(float x, float y, float w, float h, int id) {
  GfxRect r = {px(x), sy(y), pw(w), bodyDrawing ? h : ANC(h)};
  if (id < 0) {
    // Setup sheets use the approved quiet island, without the player's rim.
    gfx_sombra_sob((GfxRect){r.x-16,r.y-4,r.w+32,r.h+30},1,0,.5f,0,0,0,.25f,r,30,1);
    gfx_cor(r,30/fminf(r.w,r.h),.055f,.059f,.071f,ajustes_vidro()?.88f:.98f);
    gfx_luz_canto(r,30/fminf(r.w,r.h),r.w*.2f,-r.h*.4f,r.w*.65f,1,1,1,.035f);
  }
  else { plrui_linha_foco(r, T21_R_ROW, focusValue(id)); pointerTarget(r, id); }
}
static void logo(int p, float x, float y, float size) {
  x = px(x); y = sy(y);
  GLuint t = tex_obter_larg(marks[p], size);
  if (t) { float a = tex_aspecto(marks[p]); if (a <= 0) a = 1; gfx_rect((GfxRect){x,y + (size - size / a) / 2,size,size / a}, t, GFX_TEXTO, 0,0,0,0,1,1,1,1); }
}
static void button(int i, const char *label, float x, float y, float w) {
  const char *localized = i18n(label);
  float bx = px(x), by = sy(y), width = plrui_botao_largura(localized, NULL);
  if (fabsf(by - lastButtonY) < 1) bx = fmaxf(bx, nextButtonX);
  if (bx + width > contentX + contentW) { bx = px(screen == ROWS ? 820 : 232); by += 72; }
  GfxRect r = {bx, by, fmaxf(width, fminf(pw(w), contentX + contentW - bx)), 60};
  float reveal = ajustes_animacoes_reduzidas() ? 1 :
    revela_entra(1, revela_onda_atraso(i < 8 ? i : 7, 0), 1 + (Uint32)(pageSeconds * 1000));
  float buttonY=r.y+(1-reveal)*NV_ENTRA_DY, focus=focusValue(i);
  if(focus>.005f) plrui_botao(r.x,buttonY,localized,NULL,focus,reveal);
  else {
    gfx_cor((GfxRect){r.x,buttonY,width,60},.5f,1,1,1,.07f*reveal);
    TxtLinha label=txt_linha(TXT_G21B,localized,243,243,243,255);
    txt_desenhar_alpha(label,r.x+28,buttonY+(60-label.h)/2,reveal);
  }
  pointerTarget((GfxRect){r.x, r.y + (1 - reveal) * NV_ENTRA_DY, width, r.h}, i);
  lastButtonY = by; nextButtonX = bx + width + 12;
  if (selected == i && bodyDrawing) {
    float low = ANC(386), high = ANC(966);
    if (r.y < low) bodyTarget = fmaxf(0, bodyScroll + r.y - low);
    else if (r.y + r.h > high) bodyTarget = bodyScroll + r.y + r.h - high;
  }
}
static void input(int i, const char *label, const char *value, float y) {
  GfxRect r={px(232),sy(y),pw(686),90};
  gfx_cor(r,16/fminf(r.w,r.h),1,1,1,.045f+.025f*focusValue(i));
  pointerTarget(r,i);
  text(TXT_ILHA_SUB_L,label,254,y+12,145,642);
  text(TXT_ILHA_CORPO,value,254,y+44,243,642);
}
static void inlineKeyboard(int passwordField) {
  plrui_kicker(passwordField?"Senha · Teclado da TV":"Endereço · Teclado da TV",px(1052),sy(422),130,130,130,1);
  const char *keys=inlineKeys(passwordField);
  for(int i=0;keys[i];i++) {
    GfxRect r={px(1052+(i%10)*75),sy(456+(i/10)*62),pw(67),54};
    float focus=focusValue(KEY_FIRST+i);
    float cr,cg,cb; ajustes_acento(&cr,&cg,&cb);
    gfx_cor(r,12/fminf(r.w,r.h),anim_mistura(1,cr,focus),anim_mistura(1,cg,focus),anim_mistura(1,cb,focus),anim_mistura(.05f,1,focus));
    char key[2]={keys[i],0}; if(keyboardUpper && key[0]>='a' && key[0]<='z')key[0]-='a'-'A';
    int ink=(int)anim_mistura(225,ajustes_tinta_foco(),focus);
    TxtLinha line=txt_linha(TXT_ILHA_CORPO,key,ink,ink,ink,255);
    txt_desenhar(line,r.x+(r.w-line.w)/2,r.y+(r.h-line.h)/2);
    pointerTarget(r,KEY_FIRST+i);
  }
  const char *labels[]={passwordField?"⇧ Maiúsculas":"https://","⌫ Apagar","Concluir"};
  float x=px(1052);
  for(int i=0;i<3;i++) {
    float focus=focusValue(KEY_SHIFT+i),cr,cg,cb;
    int ink=(int)anim_mistura(225,ajustes_tinta_foco(),focus);
    TxtLinha line=txt_linha(TXT_ILHA_SUB_L,i18n(labels[i]),ink,ink,ink,255);
    GfxRect r={x,sy(722),line.w+40,52};
    ajustes_acento(&cr,&cg,&cb);
    gfx_cor(r,.5f,anim_mistura(1,cr,focus),anim_mistura(1,cg,focus),anim_mistura(1,cb,focus),anim_mistura(.07f,1,focus));
    txt_desenhar(line,r.x+20,r.y+(r.h-line.h)/2);
    pointerTarget(r,KEY_SHIFT+i); x+=r.w+12;
  }
}
static void row(int i, const char *title, const char *sub, float y, int p) {
  float group = gfx_opacidade_grupo;
  float reveal = ajustes_animacoes_reduzidas() ? 1 : revela_entra(1, revela_onda_atraso(i, 0), 1 + (Uint32)(pageSeconds * 1000));
  gfx_opacidade_grupo *= reveal;
  y += (1 - reveal) * NV_ENTRA_DY;
  if (bodyDrawing && selected == i) {
    float top = sy(y), low = ANC(386), high = ANC(966);
    if (top < low) bodyTarget = fmaxf(0, bodyScroll + top - low);
    else if (top + 86 > high) bodyTarget = bodyScroll + top + 86 - high;
  }
  panel(220,y,710,86,i);
  if (p >= 0) logo(p,242,y+21,40);
  text(TXT_ILHA_NOME,title,p>=0?304:248,y+12,243,580);
  text(TXT_ILHA_SUB_L,sub,p>=0?304:248,y+48,186,580);
  gfx_opacidade_grupo = group;
}
static void plexRow(int i, float y) {
  const PxServidor *s=&worker.plex[i];
  char sub[192];
  if(s->presente) {
    int local=0; for(int j=0;j<s->nCon;j++) if(s->con[j].local && !s->con[j].relay) local=1;
    snprintf(sub,sizeof sub,"%s · %s",i18n(s->dono?"Seu servidor":"Compartilhado"),i18n(local?"Conexão local":"Conexão remota"));
  } else if(s->visto) {
    time_t stamp=(time_t)s->visto; struct tm tm; char date[40];
    localtime_r(&stamp,&tm); strftime(date,sizeof date,"%d/%m %H:%M",&tm);
    snprintf(sub,sizeof sub,i18n("Visto por último %s"),time(NULL)/86400-s->visto/86400==1?i18n("ontem"):date);
  } else snprintf(sub,sizeof sub,"%s",i18n("Offline"));
  panel(232,y,686,104,i); logo(0,254,y+32,34);
  TxtLinha state=txt_linha(TXT_ILHA_SUB_L,i18n(s->presente?"Conectado":"Desconectado"),155,155,155,255);
  float right=px(896), width=right-state.w-px(308)-24;
  txt_desenhar(state,right-state.w,sy(y+40));
  txt_desenhar(txt_linha_corta(TXT_ILHA_NOME,s->nome,243,243,243,255,width),px(308),sy(y+20));
  txt_desenhar(txt_linha_corta(TXT_ILHA_SUB_L,sub,155,155,155,255,width),px(308),sy(y+60));
}
static float dicaChip(const char *key, const char *label, float x, float y) {
  const char *localized = i18n(label);
  return x + (plrui_dicas(&key, &localized, 1, px(x), ANC(y) + 15, 0, 1) + 22) * 1624 / contentW;
}
static void syncLabel(int p, char *out, size_t n) {
  if(!lastSync[p]) { snprintf(out,n,"%s",i18n("sem sincronizar"));return; }
  time_t stamp=(time_t)lastSync[p];struct tm local;char date[40];
  localtime_r(&stamp,&local);strftime(date,sizeof date,"%d/%m %H:%M",&local);
  snprintf(out,n,i18n("Sincronizada: %s"),date);
}
static void setupDiagram(float x, float y, Uint32 now, int done) {
  float r,g,b;ajustes_acento(&r,&g,&b);
  const char *icons[]={"aj_smartphone","aj_server","aj_tv"};
  for(int i=0;i<3;i++) {
    gfx_icone((GfxRect){px(x+i*160),sy(y),44,44},done?"aj_check":icons[i],done?r:.72f,done?g:.73f,done?b:.75f,1);
    if(i<2)gfx_icone((GfxRect){px(x+80+i*160),sy(y+10),24,24},"aj_chevron-right",r,g,b,
      done||ajustes_animacoes_reduzidas()?.65f:.35f+.4f*(.5f+.5f*sinf(now/350.0f-i)));
  }
}
static void librarySummary(float x,float y) {
  int chosen=0,titles=0;for(int i=0;i<nLibraries;i++)if(checked[i]){chosen++;titles+=libraries[i].n;}
  char value[32];snprintf(value,sizeof value,"%d / %d",chosen,nLibraries);
  text(TXT_ILHA_TITULO,value,x,y,243,320);text(TXT_ILHA_SUB_L,"Bibliotecas",x,y+52,155,320);
  snprintf(value,sizeof value,i18n("%d títulos"),titles);
  text(TXT_ILHA_NOME,value,x+370,y+10,243,350);
  text(TXT_ILHA_SUB_L,"Servidor",x+370,y+52,155,350);
}
static void drawQR(void) {
  if (!qr.lado) return;
  float x=px(1052),y=sy(418),side=fminf(246,pw(246)),unit=side/(qr.lado+8);
  gfx_cor((GfxRect){x,y,side,side},.08f,1,1,1,1);
  for (int j=0;j<qr.lado;j++) for(int i=0;i<qr.lado;i++) if(qr_modulo(&qr,i,j))
    gfx_cor((GfxRect){x+(i+4)*unit,y+(j+4)*unit,unit+.1f,unit+.1f},0,0,0,0,1);
}
void srvsetup_desenhar(Uint32 now) {
  if (!srvsetup_aberto()) return;
  navigationCount = 0;
  TXT_INI();
  t21_inicio(&contentX, &contentW);
  lastButtonY = -1000; nextButtonX = 0;
  float group = gfx_opacidade_grupo;
  gfx_opacidade_grupo *= anim_suave(entry);
  GfxRect background = {0,0,NV_TELA_W,NV_TELA_H};
  gfx_cor(background,0,.051f,.051f,.051f,1);
  gfx_luz_canto(background,0,NV_TELA_W*.85f,NV_TELA_H*.25f,NV_TELA_W*.7f,.16f,.23f,.25f,.75f);
  gfx_luz_canto(background,0,NV_TELA_W*.3f,NV_TELA_H*.05f,NV_TELA_W*.5f,.21f,.17f,.23f,.55f);
  if (!previousPage.tex) gfx_opacidade_grupo *= anim_saida(page);
  // A step change slides the new page in from the side it comes from while the
  // old one drifts out the other way (forward: right to left; Back: reversed).
  float slide = (1 - anim_saida(page)) * NV_ENTRA_DY * 2 * pageDir;
  gfx_transformar(0, 0, 1, slide, (1 - anim_suave(entry)) * NV_ENTRA_DY);
  const char *srv = serverName(provider)[0] ? serverName(provider) : providers[provider];
  char title[192];
  if (screen == CHOOSE) snprintf(title, sizeof title, "%s", i18n("Sua mídia na Biblioteca"));
  else if (screen == MANAGE) snprintf(title, sizeof title, "%s", i18n("Gerenciar servidores"));
  else if (screen == DISCOVER) snprintf(title, sizeof title, "%s", i18n(worker.alive ? "Buscando servidores" : worker.n ? "Escolher servidor" : "Nenhum servidor encontrado"));
  else if (screen == ADDRESS) snprintf(title, sizeof title, "%s", i18n("Informar endereço do servidor"));
  else if (screen == LOGIN) snprintf(title, sizeof title, "%s", provider == 2 ? i18n("Conectar ao Emby") : i18n("Seus servidores"));
  else if (screen == USERS) snprintf(title, sizeof title, "%s", i18n("Quem está assistindo?"));
  else if (screen == PASSWORD) snprintf(title, sizeof title, i18n("Entrar em %s"), srv);
  else if (screen == CONNECT_PASSWORD) snprintf(title, sizeof title, "%s", i18n("Entrar com Emby Connect"));
  else if (screen == CODE) snprintf(title, sizeof title, "%s", provider ? i18n("Autorizar esta TV") : i18n("Vincular conta Plex"));
  else if (screen == CONNECT_CODE) snprintf(title, sizeof title, "%s", i18n("Vincular Emby Connect"));
  else if (screen == PLEX_SERVERS) snprintf(title, sizeof title, "%s", i18n("Escolher servidor Plex"));
  else if (screen == CONNECT_SERVERS) snprintf(title, sizeof title, "%s", i18n("Escolha um servidor compartilhado"));
  else if (screen == LIBRARIES) snprintf(title, sizeof title, "%s", i18n("Escolher bibliotecas"));
  else if (screen == READY) snprintf(title, sizeof title, i18n(provider == 2 ? "%s conectado" : "%s conectada"), srv);
  else if (screen == REMOVE) snprintf(title, sizeof title, i18n("Remover %s deste perfil?"), srv);
  else if (screen == ERROR_SCREEN) { if (recoveryAuth) snprintf(title, sizeof title, "%s", i18n("Entrar novamente")); else snprintf(title, sizeof title, i18n("%s está offline"), srv); }
  else snprintf(title, sizeof title, "%s", i18n("Seus servidores"));
  if(screen!=ROWS) {
    char context[160];
    if (screen == CHOOSE) snprintf(context, sizeof context, "%s › %s", i18n("Biblioteca"), i18n("Adicionar origem"));
    else if (screen == MANAGE) snprintf(context, sizeof context, "%s › %s", i18n("Biblioteca"), i18n("Origens"));
    else snprintf(context, sizeof context, "%s › %s %s", i18n("Biblioteca"), i18n("Conectar"), providers[provider]);
    if (screen != CHOOSE && screen != MANAGE) logo(provider,200,124,34);
    plrui_kicker(context,px(screen == CHOOSE || screen == MANAGE ? 200 : 252),ANC(132),130,130,130,1);
    text(TXT_NOV_TITULO,title,200,180,243,1600);
    char intro[192];
    if (screen == CHOOSE) snprintf(intro, sizeof intro, "%s", i18n("Conecte um serviço. Seus filmes e séries ficam na mesma Biblioteca, listas e fontes."));
    else if (screen == MANAGE) snprintf(intro, sizeof intro, "%s", i18n("Conexões salvas só nesta TV"));
    else if (screen == USERS) snprintf(intro, sizeof intro, i18n("%s · Escolha um usuário visível ou entre com outro nome."), srv);
    else if (screen == ADDRESS) snprintf(intro, sizeof intro, "%s", i18n("Use o endereço completo, com porta ou caminho se necessário."));
    else if (screen == PASSWORD || screen == CONNECT_PASSWORD) snprintf(intro, sizeof intro, "%s", screen == CONNECT_PASSWORD ? i18n("Use sua conta Emby Connect para buscar servidores compartilhados.") : i18n("Use sua conta do servidor. Sua senha fica oculta nesta tela."));
    else if (screen == CODE && provider) snprintf(intro,sizeof intro,"%s · Quick Connect",srv);
    else if (screen == CODE || screen == CONNECT_CODE) snprintf(intro, sizeof intro, "%s", i18n("Use celular ou computador para autorizar esta TV."));
    else if (screen == LOGIN) snprintf(intro,sizeof intro,"%s",i18n("Escolha o servidor e use Emby Connect ou uma conta local."));
    else if (screen == DISCOVER || screen == PLEX_SERVERS || screen == CONNECT_SERVERS) snprintf(intro, sizeof intro, "%s", screen == PLEX_SERVERS ? i18n("Conta vinculada · Escolha um servidor ou busque na rede.") : i18n("Conecte um servidor da rede local ou informe o endereço."));
    else if (screen == LIBRARIES) snprintf(intro, sizeof intro, i18n("%s · Acesso como %s. Escolha o que aparece no Nuvio."), srv, username[0] ? username : "Marina");
    else if (screen == READY) { int k=0; for(int i2=0;i2<nLibraries;i2++) k+=checked[i2]; snprintf(intro, sizeof intro, i18n("%d bibliotecas selecionadas · Tudo pronto para assistir."), k); }
    else if (screen == REMOVE) snprintf(intro, sizeof intro, i18n("Isso desconecta o %s · %s desta TV."), providers[provider], srv);
    else if (screen == ERROR_SCREEN) { char stamp[96]; syncLabel(provider, stamp, sizeof stamp); if (recoveryAuth) snprintf(intro, sizeof intro, i18n("%s não aceita mais o login desta TV."), srv); else snprintf(intro, sizeof intro, i18n("Última conexão %s. O catálogo salvo continua disponível."), stamp); }
    else snprintf(intro, sizeof intro, "%s", i18n("Seus servidores"));
    paragraph(intro,200,250,screen==CHOOSE?740:1540);
    const char *steps[] = {i18n("Escolher serviço"),i18n("Conectar"),i18n("Entrar"),i18n("Bibliotecas"),i18n("Pronto")};
    if (screen != MANAGE) {
      float x = px(200), y = ANC(screen==CHOOSE?347:328), at = stepAnim < 0 ? stepOf(screen) : stepAnim, markX[5], markW[5];
      for (int i=0;i<5;i++) {
        // Done steps settle to grey with a check, the current one lights up.
        float done=fminf(fmaxf(at-i,0),1), lit=fmaxf(0,1-fabsf(at-i));
        int tone=(int)anim_mistura(anim_mistura(105,165,done),243,lit);
        char label[128];
        if (done>.5f) snprintf(label,sizeof label,"✓  %s",steps[i]);
        else snprintf(label,sizeof label,"%d  %s",i+1,steps[i]);
        TxtLinha line=txt_linha(TXT_ILHA_SUB_L,label,tone,tone,tone,255);
        txt_desenhar(line,x,y); markX[i]=x; markW[i]=line.w; x+=line.w+24;
        if(i<4) { line=txt_linha(TXT_ILHA_SUB_L,"›",105,105,105,255);txt_desenhar(line,x,y);x+=line.w+24; }
      }
      // Progress mark under the current step slides with the rail spring.
      int a=(int)floorf(at); if(a>3)a=3; if(a<0)a=0;
      float t=fminf(fmaxf(at-a,0),1), cr,cg,cb; ajustes_acento(&cr,&cg,&cb);
      gfx_cor((GfxRect){anim_mistura(markX[a],markX[a+1],t),y+34,anim_mistura(markW[a],markW[a+1],t),3},.5f,cr,cg,cb,.9f);
    }
  }
  bodyDrawing = screen != ROWS;
  if (bodyDrawing) gfx_recorte(contentX, ANC(386), contentW, ANC(592));
  else if (gfx_escala_txt() > 1) {
    float target = selected >= nLibraries ? fmaxf(0, (1040 + 60) - ANC(966)) : 0;
    bodyTarget = target;
    gfx_transformar(0, 0, 1, slide, -bodyScroll + (1 - anim_suave(entry)) * NV_ENTRA_DY);
  }
  if (screen == CHOOSE) {
    const char *desc[] = {"Vincule sua conta com um código. Escolha um servidor da conta ou rede.","Busque o servidor. Use Quick Connect ou entre com seu usuário.","Busque o servidor. Use Emby Connect ou entre diretamente."};
    for(int i=0;i<3;i++) {
      float reveal = ajustes_animacoes_reduzidas() ? 1 : revela_entra(1, revela_onda_atraso(i, 0), 1 + (Uint32)(pageSeconds * 1000));
      float old = gfx_opacidade_grupo, x=200+i*550;
      gfx_opacidade_grupo *= reveal;
      panel(x,405,523,274,-1);
      gfx_cor((GfxRect){px(x),sy(405)+(1-reveal)*NV_ENTRA_DY,pw(523),274},26/274.f,1,1,1,.14f*focusValue(i));
      pointerTarget((GfxRect){px(x),sy(405),pw(523),274},i);
      logo(i,x+34,428,66);
      text(TXT_V2_GRUPO,providers[i],x+34,520,243,450); paragraph(desc[i],x+34,568,455);
      gfx_opacidade_grupo = old;
    }
    paragraph("Seu servidor continua seu. O Nuvio conecta esta TV e perfil a ele.",200,714,1500);
  } else if (screen == CODE || screen == CONNECT_CODE) {
    panel(200,386,750,410,-1); panel(1020,386,804,410,-1);
    const char *url = screen==CONNECT_CODE?"emby.media/pin":provider?"Quick Connect":"plex.tv/link";
    char eyebrow[160]; snprintf(eyebrow,sizeof eyebrow,i18n("Acesse %s e digite"),url);
    plrui_kicker(provider==1?"No app Jellyfin conectado, abra Quick Connect":eyebrow,px(232),sy(422),130,130,130,1);
    char displayCode[32];
    if(strlen(code)==6) snprintf(displayCode,sizeof displayCode,"%.3s %.3s",code,code+3);
    else snprintf(displayCode,sizeof displayCode,"%s",code);
    txt_tracking(TXT_LOG_COD,displayCode,243,243,243,px(232),sy(470),1,6);
    unsigned elapsed=(now-codeSince)/1000, left=elapsed<300?300-elapsed:0; char remaining[128];
    if (screen==CONNECT_CODE) snprintf(remaining,sizeof remaining,"%s",i18n("Aguardando aprovação · Mantenha esta tela aberta."));
    else if (provider) snprintf(remaining,sizeof remaining,i18n("Aguardando aprovação · restam %02d:%02d"),left/60,left%60);
    else snprintf(remaining,sizeof remaining,i18n("Aguardando aprovação · O código expira em %02d:%02d"),left/60,left%60);
    if(provider==1) text(TXT_ILHA_CORPO,"Digite este código para autorizar o Nuvio.",232,568,155,690);
    text(TXT_ILHA_CORPO,remaining,232,provider==1?600:580,155,690);
    button(0,screen==CONNECT_CODE||provider?"Já autorizei esta TV":"Já vinculei esta TV",232,provider==1?658:630,260); button(1,"Novo código",504,provider==1?658:630,200);
    if(provider) button(2,screen==CONNECT_CODE?"Usar usuário e senha do Emby Connect":"Entrar com usuário e senha",232,provider==1?732:704,330);
    else { button(2,"Buscar na minha rede",1052,704,230);button(3,"Informar endereço do servidor",1294,704,330); }
    if(!provider) text(TXT_ILHA_SUB_L,"Não compartilhe este código.",232,720,155,690);
    drawQR(); text(TXT_V2_GRUPO,provider==1?"Abrir seu servidor":screen==CONNECT_CODE?"Abrir Emby Connect":"Leia para abrir o Plex",1332,494,243,450);
    paragraph(screen==CONNECT_CODE?"Entre e digite o código desta TV.":provider==1?"Entre e digite o código no Quick Connect.":"Entre no Plex e digite o código mostrado nesta TV.",1332,548,430);
    if(provider) text(TXT_ILHA_SUB_L,provider==1?"Se Quick Connect estiver desativado, use usuário e senha.":"Depois, escolha um servidor compartilhado e as bibliotecas.",1052,690,155,740);
    setupDiagram(1052,790,now,0);
    text(TXT_ILHA_SUB_L,"Após entrar, escolha um servidor da sua conta.",1052,852,155,740);
    text(TXT_ILHA_SUB_L,"Escolher bibliotecas",1052,886,145,740);
  } else if (screen == MANAGE) {
    panel(200,386,1624,470,-1);
    for(int i=0;i<3;i++) { char label[192],stamp[96],sub[160]; snprintf(label,sizeof label,"%s · %s",providers[i],serverName(i)[0]?serverName(i):providers[i]); syncLabel(i,stamp,sizeof stamp);
      if(!connected(i)) snprintf(sub,sizeof sub,"%s",i18n("Não configurado"));
      else if(libraryProviderCounts[i]) snprintf(sub,sizeof sub,i18n("%d títulos · %s"),libraryProviderCounts[i],stamp);
      else snprintf(sub,sizeof sub,"%s · %s",i18n("Catálogo salvo"),stamp);
      row(i,label,sub,410+i*92,i);
      text(TXT_ILHA_SUB_L,connected(i)?i18n("Online"):i18n("Offline"),1620,438+i*92,connected(i)?168:236,180); }
    button(3,"+ Conectar um servidor",232,738,330); button(4,"Remover selecionado",574,738,310);
    text(TXT_ILHA_SUB_L,"Remover a conexão não apaga arquivos do servidor.",232,800,155,1500);
  } else if (screen == USERS) {
    panel(200,386,750,402,-1);
    int total=!worker.alive&&worker.n>0?worker.n:0;
    if(worker.alive)paragraph("Carregando…",232,440,680);
    // Groups of three slide as one strip: the outgoing group leaves while the next enters.
    targetX0=px(222);targetX1=px(756);
    gfx_recorte(targetX0,ANC(386),targetX1-targetX0,ANC(592));
    for(int i=(int)floorf(userScroll);i<total&&i<(int)floorf(userScroll)+5;i++) {
      if(i<0)continue;
      float x=232+(i-userScroll)*182;
      panel(x,442,150,202,i);
      GLuint avatar=worker.users[i].imagem[0]?tex_obter_larg(worker.users[i].imagem,100):0;
      if(avatar)gfx_rect((GfxRect){px(x+25),sy(458),100,100},avatar,GFX_AVATAR,0,0,0,.5f,1,1,1,1);
      else { gfx_cor((GfxRect){px(x+25),sy(458),100,100},.5f,1,1,1,.08f); gfx_icone((GfxRect){px(x+53),sy(486),44,44},"aj_user-round",.6f,.6f,.6f,1); }
      text(TXT_ILHA_NOME,worker.users[i].nome,x+10,582,243,130);
      if(worker.users[i].senha) text(TXT_ILHA_SUB_L,"Exige senha",x+10,616,145,130);
    }
    targetX0=-1e9f;targetX1=1e9f;
    gfx_recorte(contentX, ANC(386), contentW, ANC(592));
    button(total,"Outro usuário",232,674,200);
    button(total+1,provider==1?"Usar Quick Connect":"Usar Emby Connect",444,674,300);
    text(TXT_V2_GRUPO,"Seu perfil no servidor",1020,412,243,780);
    paragraph("Só usuários visíveis são exibidos. Usuários ocultos podem digitar seu nome.",1020,470,780);
    text(TXT_ILHA_SUB_L,"Esta conexão pertence ao perfil Nuvio atual.",1020,548,145,780);
  } else if (screen == DISCOVER || screen == PLEX_SERVERS || screen == CONNECT_SERVERS) {
    int total=!worker.alive && worker.n>0?worker.n:0, n=total>4?4:total;
    char found[64];snprintf(found,sizeof found,"%s · %d",i18n("Seus servidores"),total);
    text(TXT_ILHA_SUB_L,worker.alive?i18n("Carregando…"):found,1020,820,155,740);
    panel(200,386,750,n?(screen==PLEX_SERVERS?182:210)+n*112:410,-1);
    plrui_kicker(screen==PLEX_SERVERS?"Da sua conta Plex":"Sua rede",px(232),sy(422),130,130,130,1);
    int start=(int)floorf(scroll); if(start+n>total)start=total-n; if(start<0)start=0;
    float scrollPart=scroll-start;
    if(worker.alive) {
      gfx_icone((GfxRect){px(232),sy(474),64,64},"aj_loader",.55f,.55f,.55f,1);
      text(TXT_V2_GRUPO,"Buscando na sua rede…",232,582,243,680);
      paragraph("Isso pode levar alguns segundos.",232,640,680);
    } else if(!n) { char vazio[160]; snprintf(vazio,sizeof vazio,i18n("Nenhum servidor %s na rede"),providers[provider]); gfx_icone((GfxRect){px(232),sy(474),44,44},"aj_search",.55f,.55f,.55f,1); text(TXT_V2_GRUPO,vazio,232,566,243,680); paragraph("Confira se o servidor está ligado e na mesma rede desta TV.",232,624,680); }
    if(total) gfx_recorte(px(220), fmaxf(ANC(386), sy(456)), pw(710), fminf(448, ANC(978) - fmaxf(ANC(386), sy(456))));
    int visible = total - start; if (visible > 5) visible = 5;
    for(int j=0;j<visible;j++) {
      int i=start+j;
      const char *name=screen==DISCOVER?worker.servers[i].nome:screen==USERS?worker.users[i].nome:screen==PLEX_SERVERS?worker.plex[i].nome:worker.connect[i].nome;
      char sub[160]; snprintf(sub,sizeof sub,"%s · %s · %s",providers[provider],i18n("Online"),i18n("Rede local"));
      int usuarios = screen == USERS;
      if(screen==PLEX_SERVERS) plexRow(i,456+(j-scrollPart)*112);
      else row(i,name,usuarios?i18n("Senha necessária"):screen==DISCOVER?sub:screen==CONNECT_SERVERS?i18n("Compartilhado com você · Emby Connect"):i18n("Servidor"),456+(j-scrollPart)*112,usuarios?-1:provider);
      if (usuarios) {
        GLuint avatar = worker.users[i].imagem[0] ? tex_obter_larg(worker.users[i].imagem, 64) : 0;
        if (avatar) gfx_rect((GfxRect){px(242),sy(421+j*88),64,64},avatar,GFX_AVATAR,0,0,0,.5f,1,1,1,1);
        panel(220,410+j*88,710,86,i);
      }
    }
    gfx_recorte(contentX, ANC(386), contentW, ANC(592));
    float y= n?474+n*112:worker.alive?700:682;
    button(total,screen==CONNECT_SERVERS?"Usuário e senha":screen==PLEX_SERVERS?"Buscar na rede local":"Buscar novamente",232,y,250);
    if(screen!=CONNECT_SERVERS) button(total+1,"Informar endereço",494,y,250);
    if(screen==PLEX_SERVERS) {
      text(TXT_V2_GRUPO,total?worker.plex[selected<total?selected:0].nome:providers[provider],1020,412,243,750);
      paragraph("Seus filmes e séries usam a rede local quando disponível.",1020,470,780);
      text(TXT_ILHA_SUB_L,"Bibliotecas privadas ainda exigem acesso pela conta.",1020,530,145,780);
      text(TXT_ILHA_SUB_L,"A reprodução remota segue o acesso da sua conta Plex.",1020,562,145,780);
    } else {
      text(TXT_V2_GRUPO,"Tudo em um só lugar",1020,412,243,750);
      paragraph("Filmes, séries e progresso do servidor entram na sua Biblioteca.",1020,470,780);
      text(TXT_ILHA_SUB_L,"Servidor remoto? Use o endereço completo.",1020,530,145,780);
      text(TXT_ILHA_SUB_L,"Você pode incluir uma porta ou caminho.",1020,562,145,780);
    }
    if(screen==DISCOVER) button(total+(provider==2?3:2),"Cancelar configuração",1020,598,250);
    if (screen == DISCOVER && provider == 2) button(total+2,"Usar Emby Connect",232,y+78,300);
  } else if (screen == ADDRESS || screen == PASSWORD || screen == CONNECT_PASSWORD) {
    panel(200,386,750,430,-1); panel(1020,386,804,430,-1);
    if(screen==ADDRESS) {
      input(0,"Endereço do servidor",addressText,430);
      button(1,"Conectar",232,548,155); button(2,"Buscar na rede",399,548,230);
      char exemplo[192]; snprintf(exemplo,sizeof exemplo,i18n("Por exemplo: https://media.example.test/%s"),provider==1?"jellyfin":provider==2?"emby":"");
      text(TXT_ILHA_SUB_L,exemplo,232,630,155,680);
      text(TXT_ILHA_SUB_L,"Após conectar, entre neste servidor.",232,686,145,680);
    } else {
      input(0,screen==CONNECT_PASSWORD?"Usuário ou e-mail do Emby Connect":"Usuário",username,430);
      input(1,"Senha",password[0]?"••••••••":"",532);
      if(password[0]) text(TXT_ILHA_SUB_L,"Oculto",830,578,155,90);
      button(2,"Entrar",232,650,125); button(3,screen==CONNECT_PASSWORD?"Usar conta do servidor":provider==1?"Quick Connect":"Emby Connect",369,650,300);
      if(screen==PASSWORD) text(TXT_ILHA_SUB_L,"Senha incorreta? Tente novamente ou escolha outro usuário.",232,732,155,680);
    }
    if(screen==CONNECT_PASSWORD) {
      text(TXT_V2_GRUPO,"Escolher servidor compartilhado",1052,420,243,740);
      logo(2,1074,512,34);
      text(TXT_ILHA_NOME,worker.n>0&&worker.connect[0].nome[0]?worker.connect[0].nome:serverName(2)[0]?serverName(2):"Emby",1128,498,243,570);
      text(TXT_ILHA_SUB_L,"Compartilhado com você · Emby Connect",1128,538,155,570);
      text(TXT_ILHA_SUB_L,"›",1764,518,155,28);
      paragraph("Após entrar, escolha um servidor da sua conta.",1052,600,740);
      text(TXT_ILHA_SUB_L,"Emby Connect encontra o servidor; a TV se conecta diretamente a ele.",1052,654,145,740);
    } else inlineKeyboard(screen==PASSWORD);
  } else if(screen==ROWS) {
    gfx_cor((GfxRect){0,0,NV_TELA_W,NV_TELA_H},0,0,0,0,T21_VEIL);
    text(TXT_ILHA_SUB_L,"Home",200,570,150,510);
    text(TXT_V2_GRUPO,"Conteúdo da Home",200,614,243,510);
    for(int i=0;i<3;i++) {
      const CatItem *it=cat_item(i); if(!it)break;
      GLuint poster=tex_obter_larg(it->poster,160);
      if(poster)gfx_rect((GfxRect){px(200+i*180),ANC(710),160,240},poster,GFX_TEXTO,0,0,0,.05f,1,1,1,1);
    }
    panel(780,24,1116,1032,-1);
    text(TXT_ILHA_SUB_L,"Home › Fileiras › Adicionar fileira",820,64,145,1010);
    text(TXT_ILHA_TITULO,"Escolha o conteúdo da fileira",820,105,243,1010);
    paragraph("Use o mesmo editor de coleções, listas e servidores.",820,171,990);
    // The approved editor groups catalogs, lists and servers before choosing
    // the row content. Only the server section is actionable here.
    { const char *tabs[] = {i18n("Catálogos"), i18n("Listas"), i18n("Servidores pessoais")};
      plrui_seg(tabs, NULL, 3, 2, 0, px(820), ANC(236), 1); }
    text(TXT_ILHA_SUB_L,"Servidores pessoais",820,292,225,1010); logo(provider,820,332,32);
    text(TXT_ILHA_NOME,providers[provider],870,336,243,900);
    int shown=nLibraries>4?4:nLibraries;
    int first=(int)floorf(scroll);
    if(first+shown>nLibraries)first=nLibraries-shown;
    if(first<0)first=0;
    gfx_recorte(px(820), ANC(390), pw(1036), ANC(406));
    int visible=nLibraries-first;if(visible>5)visible=5;
    for(int j=0;j<visible;j++) {
      int i=first+j;float y=394+(i-scroll)*100;
      panel(820,y,1036,92,i);
      logo(provider,844,y+25,34);text(TXT_ILHA_NOME,libraries[i].titulo,902,y+14,243,900);
      text(TXT_ILHA_SUB_L,!strcmp(libraries[i].tipo,"group")?"Coleções":!strcmp(libraries[i].tipo,"list")?"Listas":"Biblioteca",902,y+50,145,900);
    }
    gfx_sem_recorte();
    int stacked = gfx_escala_txt() > 1;
    if(nLibraries && rowChoice<nLibraries) {
      const char *key=libraries[rowChoice].chave;char label[180];
      snprintf(label,sizeof label,"%s · %s",i18n("Estilo"),i18n(fil_estilo_rotulo(key,fil_tipo(key))));
      button(nLibraries,label,820,824,325);
      snprintf(label,sizeof label,"%s · %s",i18n("Ordem"),i18n(fil_servidor_ordem(key)?"Título":"Padrão"));
      button(nLibraries+1,label,stacked?820:1157,stacked?896:824,310);
      snprintf(label,sizeof label,"%s · %s",i18n("Ocultar vistos"),i18n(fil_servidor_ocultar_vistos(key)?"Ligado":"Desligado"));
      button(nLibraries+2,label,stacked?820:1479,stacked?968:824,365);
    }
    button(nLibraries+3,"Adicionar à Home",820,stacked?1040:912,330);button(nLibraries+4,"Cancelar",1162,stacked?1040:912,260);
    text(TXT_ILHA_SUB_L,"Só filmes e séries encontrados no catálogo aparecem na Home.",820,982,145,1010);
    if(message[0])text(TXT_ILHA_SUB_L,message,820,1000,215,1000);
  } else if(screen==LIBRARIES) {
    panel(200,386,750,540,-1);
    text(TXT_V2_GRUPO,"Bibliotecas",232,420,243,680);
    if(!nLibraries) paragraph(worker.alive?"Carregando…":"Não configurado",232,460,680);
    int shown=nLibraries>3?3:nLibraries;
    int start=(int)floorf(scroll); if(start+shown>nLibraries)start=nLibraries-shown; if(start<0)start=0;
    gfx_recorte(px(220), fmaxf(ANC(386), sy(484)), pw(710), fminf(336, ANC(978) - fmaxf(ANC(386), sy(484))));
    int visible=nLibraries-start;if(visible>4)visible=4;
    for(int j=0;j<visible;j++) {
      int i=start+j;float y=484+(i-scroll)*112;
      panel(232,y,686,104,i);
      GfxRect check={px(254),sy(y+28),34,34};
      gfx_cor(check,10.0f/34,1,1,1,checked[i]?.14f:.07f);
      if(checked[i]) gfx_icone(check,"aj_check",.9f,.9f,.9f,1);
      text(TXT_ILHA_NOME,libraries[i].titulo,308,y+12,243,590);
      char sub[128];snprintf(sub,sizeof sub,i18n("%d títulos · %s"),libraries[i].n,i18n(!strcmp(libraries[i].tipo,"series")?"TV":!strcmp(libraries[i].tipo,"musicvideo")?"Videoclipes":"Filmes"));
      text(TXT_ILHA_SUB_L,sub,308,y+50,155,590);
    }
    gfx_recorte(contentX, ANC(386), contentW, ANC(592));
    button(nLibraries,libraryRetry?"Tentar novamente":"Continuar",232,832,160); button(nLibraries+1,"Voltar",404,832,120);
    text(TXT_V2_GRUPO,"Parte da sua Biblioteca",1020,412,243,700);
    paragraph("As bibliotecas aparecem junto ao Trakt, Simkl e seus títulos salvos.",1020,470,780);
    text(TXT_ILHA_NOME,"Continuar assistindo",1040,544,243,620);
    text(TXT_ILHA_SUB_L,"Unir progresso dos títulos identificados",1040,584,155,620);
    text(TXT_ILHA_SUB_L,"Ativado",1740,562,155,100);
    text(TXT_ILHA_NOME,"Estado de assistido",1040,658,243,620);
    text(TXT_ILHA_SUB_L,"Sincronizar nos dois sentidos",1040,698,155,620);
    text(TXT_ILHA_SUB_L,"Ativado",1740,676,155,100);
    text(TXT_ILHA_SUB_L,"Coleções e playlists aparecem em Listas.",1020,762,145,780);
    text(TXT_ILHA_SUB_L,"Adicione fileiras no editor de fileiras.",1020,794,145,780);
    librarySummary(1020,844);
  } else if(screen==LOGIN) {
    panel(200,386,750,470,-1); panel(1020,386,804,470,-1);
    plrui_kicker("Na sua rede",px(232),sy(422),130,130,130,1);
    row(0,srv,i18n("Rede local · Conectado"),456,provider);
    button(2,"Buscar novamente",232,588,250);button(3,"Informar endereço",494,588,250);
    button(1,provider==2?"Usar Emby Connect":"Usar Quick Connect",232,674,330);
    text(TXT_V2_GRUPO,"Entrar neste servidor",1052,420,243,740);
    for(int i=0;!worker.alive&&i<worker.n&&i<3;i++) {
      float x=1084+i*182;GLuint avatar=tex_obter_larg(worker.users[i].imagem,100);
      if(avatar) gfx_rect((GfxRect){px(x),sy(498),100,100},avatar,GFX_AVATAR,0,0,0,.5f,1,1,1,1);
      text(TXT_ILHA_NOME,worker.users[i].nome,x,616,243,150);
      if(worker.users[i].senha) text(TXT_ILHA_SUB_L,"Exige senha",x,650,145,150);
      pointerTarget((GfxRect){px(x),sy(498),150,184},5+i);
    }
    button(4,"Usuário e senha",1052,710,260);
    text(TXT_ILHA_SUB_L,"Só usuários visíveis são exibidos. Usuários ocultos podem digitar seu nome.",1052,796,145,740);
  } else if(screen==READY) {
    panel(200,386,750,492,-1); text(TXT_V2_GRUPO,"Tudo em seu lugar",232,420,243,680);
    const char *labels[]={"Biblioteca","Continuar assistindo","Fontes"};
    char origin[192]; snprintf(origin,sizeof origin,i18n("Filmes, séries, coleções e playlists com origem no %s."),providers[provider]);
    const char *descs[]={origin,i18n("Progresso do servidor unido aos títulos identificados."),i18n("A cópia do servidor aparece primeiro, quando disponível.")};
    for(int i=0;i<3;i++) {
      char number[8];snprintf(number,sizeof number,"%02d",i+1);
      txt_desenhar(txt_linha(TXT_ILHA_CORPO,number,145,145,145,255),px(232),sy(486+i*99));
      text(TXT_ILHA_NOME_L,labels[i],280,486+i*99,243,620);
      txt_bloco(TXT_ILHA_SUB_L,descs[i],155,155,155,px(280),sy(530+i*99),pw(638),27,1,2);
    }
    button(0,"Abrir Biblioteca",232,782,215); button(1,"Adicionar fileira ao Início",459,782,300);
    text(TXT_V2_GRUPO,"Personalize seu Início",1020,406,243,680);
    paragraph("Adicione biblioteca, coleção, playlist ou Adicionados recentemente pelo editor.",1020,470,780);
    txt_bloco(TXT_ILHA_SUB_L,i18n("Títulos não identificados ficam na Biblioteca do servidor. O Início mostra filmes e séries identificados."),155,155,155,px(1020),sy(562),pw(780),28,1,3);
    text(TXT_ILHA_SUB_L,"Assistidos: sincronização nos dois sentidos.",1020,640,145,780);
    text(TXT_ILHA_SUB_L,"Gerencie esta conexão em Biblioteca › Origens.",1020,672,145,780);
    librarySummary(1020,730);
    setupDiagram(1020,846,now,1);
  } else if(screen==REMOVE) {
    panel(200,386,750,430,-1); text(TXT_V2_GRUPO,"O que muda",232,420,243,680);
    paragraph("A conexão e o catálogo salvos deste servidor saem deste perfil. As fileiras da Home deixam de carregar.",232,474,680);
    paragraph("Os arquivos do servidor e a sua conta são mantidos.",232,560,680);
    button(0,"Manter conexão",232,680,300); button(1,"Remover conexão",544,680,300);
    text(TXT_V2_GRUPO,"Sua Biblioteca continua sua",1020,420,243,680);
    paragraph("Títulos salvos via Trakt, Simkl ou conta Nuvio ficam na Biblioteca.",1020,478,680);
  } else {
    panel(200,386,750,recoveryAuth?274:430,-1);
    if(recoveryAuth) panel(1020,386,804,274,-1);
    text(TXT_V2_GRUPO,recoveryAuth?"Sua conexão precisa de aprovação":"Verifique o servidor",232,420,243,680);
    paragraph(recoveryAuth?"Suas escolhas estão salvas. Entre novamente para sincronizar e assistir.":"Ligue o servidor e confira a conexão de rede, depois tente de novo.",232,474,680);
    button(0,recoveryAuth?provider==0?"Entrar no Plex":provider==1?"Usar Quick Connect":"Entrar com Emby Connect":"Tentar de novo",232,recoveryAuth?536:590,260);
    button(1,recoveryAuth?"Usuário e senha":"Trocar endereço",504,recoveryAuth?536:590,260);
    if(!recoveryAuth) paragraph("O catálogo salvo continua disponível na Biblioteca filtrada.",232,660,680);
    text(TXT_V2_GRUPO,recoveryAuth?"Continuar no Nuvio":"Continuar navegando",recoveryAuth?1052:1020,420,243,740);
    if(recoveryAuth) paragraph("Títulos salvos continuam na Biblioteca. Fontes de complementos continuam disponíveis.",1052,478,740);
    button(2,recoveryAuth?"Voltar à Biblioteca":"Abrir a Biblioteca",recoveryAuth?1052:1020,recoveryAuth?566:470,260);
    if(!recoveryAuth) button(3,"Seus servidores",1020,548,380);

  }
  if(message[0] && screen!=ERROR_SCREEN && screen!=ROWS) text(TXT_ILHA_CORPO,message,200,920,215,1624);
  if (bodyDrawing) gfx_sem_recorte();
  bodyDrawing = 0;
  if (screen != ROWS) {
    float hx = dicaChip("OK","Escolher",200,1014);
    dicaChip("Voltar","Voltar",hx,1014);
    TxtLinha nota = txt_linha(TXT_ILHA_SUB_L, i18n("Conexão salva só nesta TV e neste perfil"), 145,145,145,255);
    txt_desenhar(nota,px(1824)-nota.w,ANC(1018));
  }
  gfx_sem_transformar();
  if (previousPage.tex && !gfx_mini_ativo())
    gfx_mini_desenhar(&previousPage, (GfxRect){-anim_saida(page) * NV_ENTRA_DY * 2 * pageDir,0,NV_TELA_W,NV_TELA_H}, 0, 1 - anim_saida(page));
  gfx_opacidade_grupo = group;
  TXT_FIM();
  if(teclado_aberto()) teclado_desenhar(now);
}

static int originRows(int index, const char **name, const char **sub, int *count) {
  static char label[3][96];
  if (index < 0 || index > 3) return 0;
  *name=index<3?providers[index]:i18n("Seus servidores");
  *count=index<3?libraryProviderCounts[index]:0;
  if (index < 3) {
    const char *server = serverName(index);
    if (connected(index) && server[0]) snprintf(label[index], sizeof label[index], "%s · %s", server, i18n("Conectado"));
    else if (connected(index)) snprintf(label[index], sizeof label[index], "%s", i18n("Conectado"));
    else snprintf(label[index], sizeof label[index], "%s", i18n("Não configurado"));
    *sub = label[index];
  } else *sub = NULL;
  return 1;
}
static void originSelect(int i) {
  if(i<3 && connected(i)) {
    biblioteca_servidor_filtrar(i);
    if (!worker.alive) { provider = i; profile = perfis_ativo(); startWorker(JOB_CATALOG); }
  } else srvsetup_abrir(i<3?i:-2);
}
void srvsetup_ligar_biblioteca(void) {
  biblioteca_origens_servidores(originRows,originSelect);
  biblioteca_servidores_contem(servidores_contem_item);
  unsigned v = servidores_fileiras_versao(); int p = perfis_ativo();
  unsigned revision = cat_revisao();
  if (v == libraryVersion && p == libraryProfile && revision == libraryCatalogRevision) return;
  libraryVersion = v; libraryProfile = p; libraryCatalogRevision = revision;
  libraryProviderCounts[0] = plex_indice_n(); libraryProviderCounts[1] = jellyfin_indice_n(); libraryProviderCounts[2] = emby_indice_n();
  CatItem *items = calloc(SRV_ITENS_MAX, sizeof *items);
  if (!items) return;
  int n = servidores_catalogo_copiar(items, SRV_ITENS_MAX), w = 0;
  for (int i = 0; i < n; i++) {
    if (!items[i].imdb[0] || !strcmp(items[i].tipo, "group") || !strcmp(items[i].tipo, "list")) continue;
    int existing = cat_indice_por_imdb(items[i].imdb);
    if (existing >= 0) {
      const CatItem *old = cat_item(existing);
      if (old && items[i].retomadoMs > old->retomadoMs) {
        CatItem updated = *old; updated.retomadoMs = items[i].retomadoMs;
        updated.progresso = items[i].progresso; cat_atualizar_item(existing, &updated);
      }
      continue;
    }
    int j; for (j = 0; j < w && strcmp(items[j].imdb, items[i].imdb); j++) {}
    if (j == w) items[w++] = items[i];
  }
  if (w) cat_acrescentar_lote(items, w, NULL);
  free(items);
  biblioteca_alternar_lista(-1);
}
