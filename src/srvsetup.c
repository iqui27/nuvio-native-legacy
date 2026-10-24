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
static unsigned libraryVersion;
static float entry, page, pageSeconds, focusAnim[SRV_VISAO_MAX + 8], scroll, scrollVelocity;
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
static int rowChoice, rowStyle, recoveryAuth;
static long long lastSync[3];
static int libraryProfile = -999;
static unsigned libraryCatalogRevision;
static int libraryProviderCounts[3];
static char username[128], password[256], addressText[512], message[192], code[24], qrUrl[640];
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
  screen = next; selected = 0; message[0] = 0;
  bodyScroll = bodyVelocity = bodyTarget = userScroll = userVelocity = userTarget = 0;
  page = pageSeconds = scroll = scrollVelocity = 0;
  memset(focusAnim, 0, sizeof focusAnim);
}
static void failed(int error) { recoveryAuth = error == JF_ERR_AUTH || error == JF_ERR_EXPIRADO; change(ERROR_SCREEN); }
static int providerError(void) { return provider==0?plex_ultimo_erro():provider==1?jellyfin_ultimo_erro():emby_ultimo_erro(); }
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
  else { char url[640]; if (jellyfin_qc_url(url, sizeof url)) makeQR(url); if (!jellyfin_entrar_quick_connect()) change(LOGIN); }
}
static void librariesLoad(void) {
  nLibraries = servidores_bibliotecas(libraries, SRV_VISAO_MAX);
  int w = 0;
  for (int i = 0; i < nLibraries; i++) if (!strncmp(libraries[i].chave, prefix[provider], strlen(prefix[provider])) &&
      strcmp(libraries[i].tipo, "group") && strcmp(libraries[i].tipo, "list") && !strstr(libraries[i].chave, "_nextup")) {
    libraries[w] = libraries[i]; checked[w++] = 1;
  }
  nLibraries = w;
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
  syncTimes(); recoveryAuth = 0;
  gfx_mini_liberar(&previousPage);
  entry = page = pageSeconds = 0; motionTime = 0;
  opened = 1; rowChoice = rowStyle = 0; restartPin = enterManual = 0; username[0] = password[0] = addressText[0] = code[0] = qrUrl[0] = 0; qr.lado = 0;
  change(p == -2 ? MANAGE : p < 0 ? CHOOSE : connected(provider) ? LIBRARIES : provider ? DISCOVER : CODE);
  if (screen == DISCOVER) startWorker(JOB_DISCOVER);
  else if (screen == CODE) enterCode(); else if (screen == LIBRARIES) librariesLoad();
}
static void discover(void) { cancelLogin(); change(DISCOVER); worker.n = 0; startWorker(JOB_DISCOVER); }
static void userLogin(void) { cancelLogin(); change(USERS); worker.n = 0; startWorker(JOB_USERS); }
static void activate(void) {
  if (worker.alive && screen != CODE && screen != CONNECT_CODE) return;
  if (worker.alive && screen == CONNECT_CODE && selected == 0) {
    snprintf(message, sizeof message, "%s", i18n("Carregando…")); return;
  }
  if (screen == CODE && selected == 0) {
    snprintf(message, sizeof message, "%s", i18n("Carregando…")); return;
  }
  if (screen == LOGIN && (provider == 1 ? jellyfin_estado(NULL, 0) : emby_estado(NULL, 0)) == JF_EST_VERIFICANDO) return;
  switch (screen) {
    case CHOOSE:
      provider = selected; if (connected(provider)) { change(LIBRARIES); librariesLoad(); }
      else if (!provider) enterCode(); else discover(); break;
    case MANAGE:
      if (selected < 3) { provider = selected; change(connected(provider) ? LIBRARIES : provider ? DISCOVER : CODE); if (screen == LIBRARIES) librariesLoad(); else if (screen == DISCOVER) startWorker(JOB_DISCOVER); else enterCode(); }
      else if (selected == 3) change(CHOOSE);
      else { provider = selected - 4; change(REMOVE); } break;
    case DISCOVER:
      if (selected < worker.n && worker.n > 0) {
        snprintf(addressText, sizeof addressText, "%s", worker.servers[selected].url);
        if (!provider) { change(ADDRESS); }
        else { int ok = provider == 1 ? jellyfin_definir_servidor(addressText) : emby_definir_servidor(addressText); if (ok) change(LOGIN); }
      } else if (selected == (worker.n > 0 ? worker.n : 0)) { change(ADDRESS); keyboard(1); }
      else if (provider == 2 && selected == (worker.n > 0 ? worker.n : 0) + 2) {
        change(CONNECT_CODE); codeSince = SDL_GetTicks(); makeQR("https://emby.media/pin"); startWorker(JOB_CONNECT_PIN);
      } else discover(); break;
    case ADDRESS:
      if (!selected) keyboard(1);
      else if (selected == 1) {
        if (!addressText[0]) { keyboard(1); break; }
        if (!provider) { if (connected(0)) { snprintf(worker.input, sizeof worker.input, "%s", addressText); startWorker(JOB_PLEX_MANUAL); } else { enterManual = 1; enterCode(); } }
        else if (provider == 1 ? jellyfin_definir_servidor(addressText) : emby_definir_servidor(addressText)) change(LOGIN);
      } else discover(); break;
    case LOGIN:
      if (selected == 1 && provider == 2) { change(CONNECT_CODE); codeSince = SDL_GetTicks(); makeQR("https://emby.media/pin"); startWorker(JOB_CONNECT_PIN); }
      else if (selected == 2 && provider == 1) enterCode();
      else userLogin(); break;
    case USERS:
      if (selected < worker.n && worker.n > 0) { snprintf(username, sizeof username, "%s", worker.users[selected].nome); change(PASSWORD); keyboard(3); }
      else if (selected == (worker.n > 0 ? worker.n : 0)) { change(PASSWORD); keyboard(2); }
      else if (provider == 1) enterCode(); else { change(CONNECT_CODE); codeSince = SDL_GetTicks(); makeQR("https://emby.media/pin"); startWorker(JOB_CONNECT_PIN); } break;
    case PASSWORD: case CONNECT_PASSWORD:
      if (!selected) keyboard(2); else if (selected == 1) keyboard(3);
      else if (selected == 2) {
        if (!username[0]) { keyboard(2); break; }
        if (screen == CONNECT_PASSWORD) {
          snprintf(worker.username, sizeof worker.username, "%s", username); snprintf(worker.password, sizeof worker.password, "%s", password);
          if (startWorker(JOB_CONNECT_PASSWORD)) wipe(password, sizeof password);
        } else { int ok = provider == 1 ? jellyfin_entrar_senha(username, password) : emby_entrar_senha(username, password); if (!ok) snprintf(message, sizeof message, "%s", i18n("Falhou")); }
      } else if (screen == CONNECT_PASSWORD) { change(LOGIN); }
      else if (provider == 1) enterCode();
      else { change(CONNECT_CODE); codeSince = SDL_GetTicks(); makeQR("https://emby.media/pin"); startWorker(JOB_CONNECT_PIN); } break;
    case CODE:
      if (!selected) { /* Existing worker already polls; OK checks its published state on next update. */ }
      else if (selected == 1) enterCode(); else if (selected == 2) { cancelLogin(); if (provider) userLogin(); else discover(); }
      else { cancelLogin(); change(ADDRESS); keyboard(1); } break;
    case CONNECT_CODE:
      if (selected == 1) { stopWorker(); code[0] = 0; restartPin = 1; }
      else if (selected == 2) { stopWorker(); change(CONNECT_PASSWORD); }
      break;
    case PLEX_SERVERS:
      if (selected < worker.n && worker.n > 0) { worker.n = selected; startWorker(JOB_PLEX_CHOOSE); }
      else if (selected == (worker.n > 0 ? worker.n : 0)) discover(); else { change(ADDRESS); keyboard(1); } break;
    case CONNECT_SERVERS:
      if (selected < worker.n && worker.n > 0) { worker.n = selected; startWorker(JOB_CONNECT_EXCHANGE); }
      else change(CONNECT_PASSWORD); break;
    case LIBRARIES:
      if (selected < nLibraries) checked[selected] = !checked[selected];
      else if (selected == nLibraries) {
        int n = 0; for (int i = 0; i < nLibraries; i++) n += checked[i];
        if (!n) { snprintf(message, sizeof message, "%s", i18n("Escolher")); break; }
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
      } else change(MANAGE); break;
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
        if (recoveryAuth) { if(provider==2) { change(CONNECT_CODE); makeQR("https://emby.media/pin"); startWorker(JOB_CONNECT_PIN); } else enterCode(); }
        else { change(LIBRARIES); startWorker(JOB_LIBRARIES); }
      } else if (selected == 1) {
        if(recoveryAuth && provider) userLogin(); else { change(ADDRESS); keyboard(1); }
      } else if(selected==2) { srvsetup_fechar(); biblioteca_servidor_filtrar(provider); }
      else change(MANAGE); break;
  }
}
static int targets(void) {
  if (worker.alive && screen != CODE && screen != CONNECT_CODE) return 0;
  switch (screen) {
    case CHOOSE: return 3;
    case MANAGE: return 7;
    case DISCOVER: return (worker.n > 0 ? worker.n : 0) + (provider == 2 ? 3 : 2);
    case USERS: return (worker.n > 0 ? worker.n : 0) + 2;
    case ADDRESS: case LOGIN: return 3;
    case ERROR_SCREEN: return recoveryAuth ? 3 : 4;
    case CODE: return 3;
    case PASSWORD: case CONNECT_PASSWORD: return 4;
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
  if (nv_tecla_voltar(e)) {
    cancelLogin(); stopWorker(); wipe(password, sizeof password);
    if (screen == CHOOSE || screen == MANAGE) srvsetup_fechar();
    else if (screen == REMOVE) change(MANAGE);
    else if (screen == LIBRARIES) change(MANAGE);
    else if (screen == READY) { change(LIBRARIES); librariesLoad(); }
    else if (screen == ROWS) change(READY);
    else if (screen == ADDRESS || screen == DISCOVER || screen == CODE) change(CHOOSE);
    else if (screen == LOGIN) change(CHOOSE);
    else change(LOGIN);
  } else if (k == SDLK_LEFT || k == SDLK_UP) { if (selected > 0) selected--; }
  else if (k == SDLK_RIGHT || k == SDLK_DOWN) { if (selected + 1 < targets()) selected++; }
  else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) activate();
}
void srvsetup_atualizar(Uint32 now) {
  float dt = motionTime ? fminf((Uint32)(now - motionTime) / 1000.0f, .05f) : 1.0f / 60;
  motionTime = now;
  int reduced = ajustes_animacoes_reduzidas();
  entry = t21_entrada(entry, dt, opened);
  page = reduced ? 1 : anim_rampa(page, 1, dt, 180);
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
  if ((screen == DISCOVER || screen == PLEX_SERVERS || screen == CONNECT_SERVERS || screen == LIBRARIES || screen == ROWS) && total > 4) {
    int choice = selected < total ? selected : total - 1;
    targetScroll = fminf(fmaxf(choice - 3, 0), total - 4);
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
      else if (worker.result < 0) { failed(worker.result); snprintf(message, sizeof message, "%s", i18n(worker.result == JF_ERR_AUTH ? "Usuário ou senha incorretos" : worker.result == JF_ERR_EXPIRADO ? "o código expirou — OK pede outro" : "O servidor não respondeu dentro do prazo.")); }
      else if (worker.kind == JOB_ROW_CONTENT) {
        nLibraries = worker.n; memcpy(libraries,worker.libraries,sizeof *libraries*nLibraries); rowChoice = rowStyle = 0;
      }
      else if (worker.kind == JOB_LIBRARIES) {
        nLibraries = worker.n; memcpy(libraries, worker.libraries, sizeof *libraries * nLibraries);
        const char *selection = provider ? worker.jf.bibliotecas : worker.px.bibliotecas;
        for (int i = 0; i < nLibraries; i++) checked[i] = srv_biblioteca_selecionada(selection, libraries[i].chave + strlen(prefix[provider]));
      }
      else if (worker.kind == JOB_CONNECT_PIN || worker.kind == JOB_CONNECT_PASSWORD) { change(CONNECT_SERVERS); startWorker(JOB_CONNECT_SERVERS); }
      else if (worker.kind == JOB_CONNECT_EXCHANGE) { if (jf_setup_aceitar(1, &worker.jf, worker.profile, worker.generation)) { change(LIBRARIES); librariesLoad(); } }
      else if (worker.kind == JOB_PLEX_CHOOSE || worker.kind == JOB_PLEX_MANUAL) { if (px_setup_aceitar(&worker.px, worker.profile, worker.generation)) { change(LIBRARIES); librariesLoad(); } }
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
    if (connected(provider)) {
      if (!provider) {
        if (enterManual) { enterManual = 0; snprintf(worker.input, sizeof worker.input, "%s", addressText); change(ADDRESS); startWorker(JOB_PLEX_MANUAL); }
        else { change(PLEX_SERVERS); startWorker(JOB_PLEX_SERVERS); }
      }
      else { change(LIBRARIES); librariesLoad(); }
    } else if ((!provider && (state == PX_EST_ERRO || state == PX_EST_EXPIROU)) || (provider && (state == JF_EST_ERRO || state == JF_EST_EXPIROU))) failed(state==(provider?JF_EST_EXPIROU:PX_EST_EXPIROU)?JF_ERR_EXPIRADO:providerError());
  }
  if ((screen == PASSWORD || screen == LOGIN) && connected(provider)) { change(LIBRARIES); librariesLoad(); }
  if (screen == LIBRARIES && !nLibraries && !worker.alive) {
    snprintf(message, sizeof message, "%s", i18n("Não configurado"));
  }
  if (screen == PASSWORD) {
    int state = provider == 1 ? jellyfin_estado(NULL, 0) : emby_estado(NULL, 0);
    if (state == JF_EST_ERRO) snprintf(message, sizeof message, "%s", i18n("Usuário ou senha incorretos"));
  }
  if (screen == LOGIN) {
    int state = provider == 1 ? jellyfin_estado(NULL, 0) : emby_estado(NULL, 0);
    if (state == JF_EST_VERIFICANDO) snprintf(message, sizeof message, "%s", i18n("Conferindo o servidor…"));
    else if (state == JF_EST_ERRO) failed(providerError());
    else message[0] = 0;
  }
}

static void text(int style, const char *s, float x, float y, int tone, float width) {
  txt_desenhar(txt_linha_corta(style, i18n(s), tone, tone, tone, 255, pw(width)), px(x), sy(y));
}
static void paragraph(const char *s, float x, float y, float width) {
  txt_bloco(TXT_ILHA_CORPO, i18n(s), 186, 192, 204, px(x), sy(y), pw(width), 32, 1, 3);
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
  if (bodyDrawing) ponteiro_alvo_faixa(r.x, r.y, r.w, r.h, ANC(386), ANC(978), pointFocus, pointActivate, id, 0);
  else {
    if (gfx_escala_txt() > 1) r.y -= bodyScroll;
    ponteiro_alvo(r.x, r.y, r.w, r.h, pointFocus, pointActivate, id, 0);
  }
}
static void panel(float x, float y, float w, float h, int id) {
  GfxRect r = {px(x), sy(y), pw(w), bodyDrawing ? h : ANC(h)};
  if (id < 0) t21_ilha(r, T21_R_SHEET, 1);
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
  plrui_botao(r.x, r.y + (1 - reveal) * NV_ENTRA_DY, localized, NULL, focusValue(i), reveal);
  pointerTarget((GfxRect){r.x, r.y + (1 - reveal) * NV_ENTRA_DY, width, r.h}, i);
  lastButtonY = by; nextButtonX = bx + r.w + 12;
  if (selected == i && bodyDrawing) {
    float low = ANC(386), high = ANC(966);
    if (r.y < low) bodyTarget = fmaxf(0, bodyScroll + r.y - low);
    else if (r.y + r.h > high) bodyTarget = bodyScroll + r.y + r.h - high;
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
  text(TXT_ILHA_SUB,sub,p>=0?304:248,y+48,186,580);
  gfx_opacidade_grupo = group;
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
static void drawQR(void) {
  if (!qr.lado) return;
  float x=px(1052),y=sy(418),side=fminf(246,pw(246)),unit=side/(qr.lado+8);
  gfx_cor((GfxRect){x,y,side,side},.08f,1,1,1,1);
  for (int j=0;j<qr.lado;j++) for(int i=0;i<qr.lado;i++) if(qr_modulo(&qr,i,j))
    gfx_cor((GfxRect){x+(i+4)*unit,y+(j+4)*unit,unit+.1f,unit+.1f},0,0,0,0,1);
}
void srvsetup_desenhar(Uint32 now) {
  if (!srvsetup_aberto()) return;
  TXT_INI();
  t21_inicio(&contentX, &contentW);
  lastButtonY = -1000; nextButtonX = 0;
  float group = gfx_opacidade_grupo;
  gfx_opacidade_grupo *= anim_suave(entry);
  gfx_cor((GfxRect){0,0,NV_TELA_W,NV_TELA_H},0,.035f,.038f,.044f,1);
  if (!previousPage.tex) gfx_opacidade_grupo *= anim_saida(page);
  gfx_transformar(0, 0, 1, 0, (1 - anim_suave(entry)) * NV_ENTRA_DY);
  const char *srv = serverName(provider)[0] ? serverName(provider) : providers[provider];
  char title[192];
  if (screen == CHOOSE) snprintf(title, sizeof title, "%s", i18n("Sua mídia, na sua Biblioteca"));
  else if (screen == MANAGE) snprintf(title, sizeof title, "%s", i18n("Gerenciar servidores"));
  else if (screen == DISCOVER) snprintf(title, sizeof title, "%s", i18n(worker.alive ? "Procurando servidores" : worker.n ? "Escolha seu servidor" : "Nenhum servidor encontrado"));
  else if (screen == ADDRESS) snprintf(title, sizeof title, "%s", i18n("Digite o endereço do servidor"));
  else if (screen == LOGIN) snprintf(title, sizeof title, "%s", provider == 2 ? i18n("Conectar ao Emby") : i18n("Seus servidores"));
  else if (screen == USERS) snprintf(title, sizeof title, "%s", i18n("Quem está assistindo?"));
  else if (screen == PASSWORD) snprintf(title, sizeof title, i18n("Entrar em %s"), srv);
  else if (screen == CONNECT_PASSWORD) snprintf(title, sizeof title, "%s", i18n("Entrar com o Emby Connect"));
  else if (screen == CODE) snprintf(title, sizeof title, "%s", provider ? i18n("Aprove esta TV") : i18n("Vincule sua conta Plex"));
  else if (screen == CONNECT_CODE) snprintf(title, sizeof title, "%s", i18n("Vincular o Emby Connect"));
  else if (screen == PLEX_SERVERS) snprintf(title, sizeof title, "%s", i18n("Escolha seu servidor Plex"));
  else if (screen == CONNECT_SERVERS) snprintf(title, sizeof title, "%s", i18n("Escolha um servidor compartilhado"));
  else if (screen == LIBRARIES) snprintf(title, sizeof title, "%s", i18n("Escolha suas bibliotecas"));
  else if (screen == READY) snprintf(title, sizeof title, i18n("%s conectado"), srv);
  else if (screen == REMOVE) snprintf(title, sizeof title, i18n("Remover %s deste perfil?"), srv);
  else if (screen == ERROR_SCREEN) { if (recoveryAuth) snprintf(title, sizeof title, "%s", i18n("Entrar de novo")); else snprintf(title, sizeof title, i18n("%s está offline"), srv); }
  else snprintf(title, sizeof title, "%s", i18n("Seus servidores"));
  if(screen!=ROWS) {
    char context[160];
    if (screen == CHOOSE) snprintf(context, sizeof context, "%s › %s", i18n("Biblioteca"), i18n("Adicionar origem"));
    else if (screen == MANAGE) snprintf(context, sizeof context, "%s › %s", i18n("Biblioteca"), i18n("Origens"));
    else snprintf(context, sizeof context, "%s › %s %s", i18n("Biblioteca"), i18n("Conectar"), providers[provider]);
    if (screen != CHOOSE && screen != MANAGE) { logo(provider,200,124,34); text(TXT_ILHA_APOIO,context,252,132,130,1500); }
    else text(TXT_V2_KICK,context,200,132,130,1500);
    text(TXT_NOV_TITULO,title,200,180,243,1600);
    char intro[192];
    if (screen == CHOOSE) snprintf(intro, sizeof intro, "%s", i18n("Conecte um serviço. Seus filmes e séries entram na mesma Biblioteca, listas e fontes."));
    else if (screen == MANAGE) snprintf(intro, sizeof intro, "%s", i18n("Conexões salvas só nesta TV"));
    else if (screen == USERS) snprintf(intro, sizeof intro, i18n("%s · Escolha um usuário visível ou entre com outro nome."), srv);
    else if (screen == ADDRESS) snprintf(intro, sizeof intro, "%s", i18n("Use o endereço completo, com porta ou caminho, se o servidor precisar."));
    else if (screen == PASSWORD || screen == CONNECT_PASSWORD) snprintf(intro, sizeof intro, "%s", screen == CONNECT_PASSWORD ? i18n("Use sua conta Emby Connect para achar os servidores compartilhados com você.") : i18n("Use sua conta do servidor. Sua senha fica oculta nesta tela."));
    else if (screen == CODE || screen == CONNECT_CODE) snprintf(intro, sizeof intro, "%s", screen == CODE && provider ? i18n("Quick Connect") : i18n("Use o celular ou o computador para aprovar esta TV."));
    else if (screen == DISCOVER || screen == PLEX_SERVERS || screen == CONNECT_SERVERS) snprintf(intro, sizeof intro, "%s", screen == PLEX_SERVERS ? i18n("Conta vinculada · Escolha um servidor disponível ou busque na sua rede.") : i18n("Conecte a um servidor da sua rede ou digite o endereço."));
    else if (screen == LIBRARIES) snprintf(intro, sizeof intro, i18n("%s · Conectado como %s. Escolha o que aparece no Nuvio."), srv, username[0] ? username : "Marina");
    else if (screen == READY) { int k=0; for(int i2=0;i2<nLibraries;i2++) k+=checked[i2]; snprintf(intro, sizeof intro, i18n("%d bibliotecas selecionadas · Pronto para a sua próxima sessão."), k); }
    else if (screen == REMOVE) snprintf(intro, sizeof intro, i18n("Isso desconecta o %s · %s desta TV."), providers[provider], srv);
    else if (screen == ERROR_SCREEN) { char stamp[96]; syncLabel(provider, stamp, sizeof stamp); if (recoveryAuth) snprintf(intro, sizeof intro, i18n("%s não aceita mais o login desta TV."), srv); else snprintf(intro, sizeof intro, i18n("Última conexão %s. O catálogo salvo continua disponível."), stamp); }
    else snprintf(intro, sizeof intro, "%s", i18n("Seus servidores"));
    paragraph(intro,200,250,1540);
    const char *steps[] = {i18n("Serviço"),i18n("Conectar"),i18n("Entrar"),i18n("Biblioteca"),i18n("Pronto")};
    int step=screen==CHOOSE?0:screen==LIBRARIES?3:screen==READY?4:screen==DISCOVER||screen==ADDRESS?1:2;
    if (screen != MANAGE) t21_passos((GfxRect){px(200), ANC(320), pw(1624), 5}, 5, step, steps, 1);
  }
  bodyDrawing = screen != ROWS;
  if (bodyDrawing) gfx_recorte(contentX, ANC(386), contentW, ANC(592));
  else if (gfx_escala_txt() > 1) {
    float target = selected >= nLibraries ? fmaxf(0, (1040 + 60) - ANC(966)) : 0;
    bodyTarget = target;
    gfx_transformar(0, 0, 1, 0, -bodyScroll + (1 - anim_suave(entry)) * NV_ENTRA_DY);
  }
  if (screen == CHOOSE) {
    const char *desc[] = {"Vincule sua conta com um código. Escolha um servidor da sua conta ou da sua rede.","Encontre seu servidor. Use o Quick Connect ou entre com seu usuário do servidor.","Encontre seu servidor. Use o Emby Connect ou entre diretamente."};
    for(int i=0;i<3;i++) {
      float reveal = ajustes_animacoes_reduzidas() ? 1 : revela_entra(1, revela_onda_atraso(i, 0), 1 + (Uint32)(pageSeconds * 1000));
      float old = gfx_opacidade_grupo, x=200+i*550;
      gfx_opacidade_grupo *= reveal;
      t21_ilha((GfxRect){px(x), sy(405) + (1-reveal)*NV_ENTRA_DY, pw(523), 274}, T21_R_SHEET, 1);
      panel(x,405,523,274,i); logo(i,x+34,428,66);
      text(TXT_V2_GRUPO,providers[i],x+34,520,243,450); paragraph(desc[i],x+34,568,455);
      gfx_opacidade_grupo = old;
    }
    paragraph("Seu servidor continua seu. O Nuvio conecta esta TV e este perfil a ele.",200,714,1500);
  } else if (screen == CODE || screen == CONNECT_CODE) {
    panel(200,386,750,456,-1); panel(1020,386,804,456,-1);
    const char *url = screen==CONNECT_CODE?"emby.media/pin":provider?"Quick Connect":"plex.tv/link";
    char eyebrow[160]; snprintf(eyebrow,sizeof eyebrow,i18n("Vá em %s e digite"),url);
    text(TXT_ILHA_SUB,eyebrow,232,422,150,680);
    text(TXT_LOG_COD,code[0]?code:"…",232,470,243,680);
    unsigned elapsed=(now-codeSince)/1000, left=elapsed<300?300-elapsed:0; char remaining[128];
    if (screen==CONNECT_CODE) snprintf(remaining,sizeof remaining,"%s",i18n("Aguardando aprovação · Mantenha esta tela aberta."));
    else if (provider) snprintf(remaining,sizeof remaining,i18n("Aguardando aprovação · restam %02d:%02d"),left/60,left%60);
    else snprintf(remaining,sizeof remaining,i18n("Aguardando aprovação · O código expira em %02d:%02d"),left/60,left%60);
    text(TXT_ILHA_TEXTO,remaining,232,580,155,690);
    button(0,screen==CONNECT_CODE||provider?"Já aprovei esta TV":"Já vinculei esta TV",232,630,300); button(1,"Novo código",544,630,200);
    button(2,screen==CONNECT_CODE||provider?"Usuário e senha":"Buscar na rede",232,716,330);
    text(TXT_ILHA_APOIO,screen==CONNECT_CODE||provider?"Digite este código para aprovar o Nuvio.":"Não compartilhe este código.",232,798,155,690);
    drawQR(); text(TXT_V2_GRUPO,provider==1?"Abra seu servidor":screen==CONNECT_CODE?"Abra o Emby Connect":"Escaneie para abrir o Plex",1332,494,243,450);
    paragraph(screen==CONNECT_CODE?"Entre e digite o código desta TV.":provider==1?"Entre e digite o código no Quick Connect.":"Entre no Plex e digite o código mostrado nesta TV.",1332,548,430);
    text(TXT_ILHA_SUB,provider==1?"Se o Quick Connect estiver desativado, use usuário e senha.":screen==CONNECT_CODE?"Depois: escolha um servidor compartilhado e suas bibliotecas.":"Buscar na minha rede · Digitar endereço",1332,650,155,430);
  } else if (screen == MANAGE) {
    panel(200,386,1624,470,-1);
    for(int i=0;i<3;i++) { char label[192],stamp[96],sub[160]; snprintf(label,sizeof label,"%s · %s",providers[i],serverName(i)[0]?serverName(i):providers[i]); syncLabel(i,stamp,sizeof stamp);
      if(!connected(i)) snprintf(sub,sizeof sub,"%s",i18n("Não configurado"));
      else if(libraryProviderCounts[i]) snprintf(sub,sizeof sub,i18n("%d títulos · %s"),libraryProviderCounts[i],stamp);
      else snprintf(sub,sizeof sub,"%s · %s",i18n("Catálogo salvo"),stamp);
      row(i,label,sub,410+i*92,i);
      text(TXT_ILHA_APOIO,connected(i)?i18n("Online"):i18n("Offline"),1620,438+i*92,connected(i)?168:236,180); }
    button(3,"+ Conectar um servidor",232,738,330); button(4,"Remover selecionado",574,738,310);
    text(TXT_ILHA_APOIO,"Remover a conexão não apaga arquivos do servidor.",232,800,155,1500);
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
    }
    targetX0=-1e9f;targetX1=1e9f;
    gfx_recorte(contentX, ANC(386), contentW, ANC(592));
    button(total,"Trocar de usuário",232,674,270);
    button(total+1,provider==1?"Quick Connect":"Emby Connect",514,674,300);
    text(TXT_V2_GRUPO,"Perfil",1020,412,243,680);
    paragraph("Cada perfil tem sua própria lista e seu progresso.",1020,470,680);
    paragraph("Este perfil nesta TV",1020,566,680);
  } else if (screen == DISCOVER || screen == PLEX_SERVERS || screen == CONNECT_SERVERS) {
    panel(200,386,750,500,-1);
    int total=!worker.alive && worker.n>0?worker.n:0, n=total>4?4:total;
    int start=(int)floorf(scroll); if(start+n>total)start=total-n; if(start<0)start=0;
    float scrollPart=scroll-start;
    if(worker.alive) paragraph(screen==DISCOVER?"Procurando na sua rede… Isso pode levar alguns segundos.":"Carregando…",232,420,680);
    else if(!n) { char vazio[160]; snprintf(vazio,sizeof vazio,i18n("Nenhum servidor %s por perto"),providers[provider]); gfx_icone((GfxRect){px(232),ANC(414),44,44},"aj_search",.4f,.4f,.4f,1); text(TXT_ILHA_NOME,vazio,232,472,243,680); paragraph("Confira se o servidor está ligado e na mesma rede desta TV.",232,520,680); }
    gfx_recorte(px(220), fmaxf(ANC(386), sy(400)), pw(710), fminf(370, ANC(978) - fmaxf(ANC(386), sy(400))));
    int visible = total - start; if (visible > 5) visible = 5;
    for(int j=0;j<visible;j++) {
      int i=start+j;
      const char *name=screen==DISCOVER?worker.servers[i].nome:screen==USERS?worker.users[i].nome:screen==PLEX_SERVERS?worker.plex[i].nome:worker.connect[i].nome;
      char sub[160]; snprintf(sub,sizeof sub,"%s · %s · %s",providers[provider],i18n("Online"),i18n("Rede local"));
      int usuarios = screen == USERS;
      row(i,name,usuarios?i18n("Senha necessária"):screen==DISCOVER?sub:screen==CONNECT_SERVERS?i18n("Compartilhado com você · Emby Connect"):screen==PLEX_SERVERS?i18n("Da sua conta Plex"):i18n("Servidor"),410+(j-scrollPart)*88,usuarios?-1:provider);
      if (usuarios) {
        GLuint avatar = worker.users[i].imagem[0] ? tex_obter_larg(worker.users[i].imagem, 64) : 0;
        if (avatar) gfx_rect((GfxRect){px(242),sy(421+j*88),64,64},avatar,GFX_AVATAR,0,0,0,.5f,1,1,1,1);
        panel(220,410+j*88,710,86,i);
      }
    }
    gfx_recorte(contentX, ANC(386), contentW, ANC(592));
    float y= n?430+n*88:600;
    button(total,screen==USERS?"Outro usuário":screen==CONNECT_SERVERS?"Usuário e senha":screen==PLEX_SERVERS?"Buscar na rede local":n?"Digitar endereço":"Buscar de novo",232,y,330);
    if(screen!=CONNECT_SERVERS) button(total+1,screen==USERS?(provider==1?"Usar Quick Connect":"Usar Emby Connect"):"Digitar endereço",574,y,330);
    text(TXT_V2_GRUPO,"Tudo no mesmo lugar",1020,412,243,750);
    paragraph("Filmes, séries e progresso do seu servidor entram na sua Biblioteca.",1020,470,680);
    paragraph("Servidor remoto? Use o endereço completo. Você pode incluir porta ou caminho.",1020,580,680);
    if (screen == DISCOVER && provider == 2) button(total+2,"Emby Connect",1020,700,330);
  } else if (screen == ADDRESS || screen == PASSWORD || screen == CONNECT_PASSWORD) {
    panel(200,386,750,430,-1); panel(1020,386,804,430,-1);
    if(screen==ADDRESS) {
      text(TXT_ILHA_APOIO,"Endereço do servidor",232,428,155,680);
      button(0,addressText[0]?addressText:"http://…",232,456,680); button(1,"Conectar",232,580,260); button(2,"Buscar na rede",510,580,300);
      char exemplo[192]; snprintf(exemplo,sizeof exemplo,i18n("Por exemplo: https://media.example.test/%s"),provider==1?"jellyfin":provider==2?"emby":"");
      text(TXT_ILHA_APOIO,exemplo,232,652,155,680);
    } else {
      text(TXT_ILHA_APOIO,screen==CONNECT_PASSWORD?"Usuário ou e-mail do Emby Connect":"Usuário",232,408,155,680);
      button(0,username[0]?username:"",232,436,680);
      text(TXT_ILHA_APOIO,"Senha",232,514,155,680);
      button(1,password[0]?"••••••••":"",232,542,680);
      if(password[0]) text(TXT_ILHA_APOIO,"Oculta",830,560,155,90);
      button(2,"Entrar",232,660,260); button(3,screen==CONNECT_PASSWORD?"Usar conta do servidor":provider==1?"Quick Connect":"Emby Connect",510,660,330);
    }
    text(TXT_V2_GRUPO,"Teclado",1052,420,243,680);
    paragraph("No campo, OK abre o teclado da TV; o celular também mostra um QR. Você digita e o texto chega na TV.",1052,480,680);
  } else if(screen==ROWS) {
    gfx_cor((GfxRect){0,0,NV_TELA_W,NV_TELA_H},0,0,0,0,T21_VEIL);
    text(TXT_ILHA_SUB,"Home",200,570,150,510);
    text(TXT_V2_GRUPO,"Conteúdo da Home",200,614,243,510);
    for(int i=0;i<3;i++) {
      const CatItem *it=cat_item(i); if(!it)break;
      GLuint poster=tex_obter_larg(it->poster,160);
      if(poster)gfx_rect((GfxRect){px(200+i*180),ANC(710),160,240},poster,GFX_TEXTO,0,0,0,.05f,1,1,1,1);
    }
    panel(780,24,1116,1032,-1);
    text(TXT_ILHA_APOIO,"Home › Fileiras › Adicionar fileira",820,64,145,1010);
    text(TXT_ILHA_TITULO,"Escolha o conteúdo da fileira",820,105,243,1010);
    paragraph("Use o mesmo editor de coleções, listas e servidores.",820,171,990);
    // The approved editor groups catalogs, lists and servers before choosing
    // the row content. Only the server section is actionable here.
    { const char *tabs[] = {i18n("Catálogos"), i18n("Listas"), i18n("Servidores pessoais")};
      plrui_seg(tabs, NULL, 3, 2, 0, px(820), ANC(236), 1); }
    text(TXT_ILHA_SUB,"Servidores pessoais",820,292,225,1010); logo(provider,820,332,32);
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
      text(TXT_ILHA_SUB,!strcmp(libraries[i].tipo,"group")?"Coleções":!strcmp(libraries[i].tipo,"list")?"Listas":"Biblioteca",902,y+50,145,900);
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
    text(TXT_ILHA_APOIO,"Só filmes e séries encontrados no catálogo aparecem na Home.",820,982,145,1010);
    if(message[0])text(TXT_ILHA_SUB,message,820,1000,215,1000);
  } else if(screen==LIBRARIES) {
    panel(200,386,750,500,-1);
    text(TXT_V2_GRUPO,"Biblioteca",232,416,243,680);
    if(!nLibraries) paragraph(worker.alive?"Carregando…":"Não configurado",232,460,680);
    int shown=nLibraries>4?4:nLibraries;
    int start=(int)floorf(scroll); if(start+shown>nLibraries)start=nLibraries-shown; if(start<0)start=0;
    gfx_recorte(px(220), fmaxf(ANC(386), sy(448)), pw(710), fminf(364, ANC(978) - fmaxf(ANC(386), sy(448))));
    int visible=nLibraries-start;if(visible>5)visible=5;
    for(int j=0;j<visible;j++){int i=start+j; char sub[96]; snprintf(sub,sizeof sub,i18n("%d títulos"),libraries[i].n); row(i,libraries[i].titulo,sub,452+(i-scroll)*88,provider); if(checked[i]) gfx_icone((GfxRect){px(880),sy(482+(i-scroll)*88),28,28},"aj_check",.9f,.9f,.9f,1); else gfx_cor((GfxRect){px(880),sy(482+(i-scroll)*88),28,28},10.0f/28,1,1,1,.10f);}
    gfx_recorte(contentX, ANC(386), contentW, ANC(592));
    button(nLibraries,"Continuar",232,822,240); button(nLibraries+1,"Voltar",486,822,200);
    text(TXT_V2_GRUPO,"Parte da sua Biblioteca",1020,412,243,700);
    paragraph("As bibliotecas escolhidas aparecem junto de Trakt, Simkl e dos seus títulos salvos.",1020,470,680);
    text(TXT_ILHA_NOME,"Continuar assistindo",1020,566,243,680);
    paragraph("Une o progresso dos títulos encontrados",1020,606,680);
    text(TXT_ILHA_NOME,"Marcas de visto",1020,654,243,680);
    paragraph("Sincroniza nos dois sentidos",1020,694,680);
    paragraph("Coleções e playlists aparecem em Listas. As fileiras da Home são adicionadas no editor de fileiras.",1020,742,680);
  } else if(screen==LOGIN) {
    panel(200,386,750,400,-1); panel(1020,386,804,400,-1);
    text(TXT_ILHA_APOIO,"Na sua rede",232,416,155,680);
    button(0,worker.n?worker.servers[0].nome:providers[provider],232,444,680);
    button(1,provider==2?"Usar o Emby Connect":"Usuário e senha",232,566,330);
    text(TXT_V2_GRUPO,"Entrar neste servidor",1020,412,243,680);
    paragraph("Só aparecem os usuários compartilhados pelo servidor. Usuários ocultos entram com o nome.",1020,470,680);
  } else if(screen==READY) {
    panel(200,386,750,430,-1); text(TXT_V2_GRUPO,"Tudo no seu lugar",232,420,243,680);
    const char *labels[]={"Biblioteca","Continuar assistindo","Fontes"};
    const char *descs[]={"Filmes, séries, coleções e playlists, com o servidor como origem.","O progresso do servidor se junta aos títulos encontrados.","A cópia do seu servidor aparece primeiro quando disponível."};
    for(int i=0;i<3;i++){ text(TXT_ILHA_APOIO,labels[i],250,486+i*85,145,650); text(TXT_ILHA_NOME,descs[i],250,514+i*85,243,650); }
    button(0,"Abrir a Biblioteca",232,730,300); button(1,"Adicionar à Home",544,730,300);
    text(TXT_V2_GRUPO,"Deixe a Home com a sua cara",1020,440,243,680);
    paragraph("Adicione uma biblioteca, coleção, playlist ou os Mais recentes pelo editor de fileiras.",1020,498,680);
    paragraph("Títulos sem correspondência no catálogo ficam na Biblioteca filtrada pelo servidor. A Home mostra filmes e séries encontrados.",1020,610,680);
    paragraph("Sincronia de vistos: nos dois sentidos. Gerencie esta conexão em Biblioteca › Origens.",1020,730,680);
  } else if(screen==REMOVE) {
    panel(200,386,750,430,-1); text(TXT_V2_GRUPO,"O que muda",232,420,243,680);
    paragraph("A conexão e o catálogo salvos deste servidor saem deste perfil. As fileiras da Home deixam de carregar.",232,474,680);
    paragraph("Os arquivos do servidor e a sua conta são mantidos.",232,560,680);
    button(0,"Manter conexão",232,680,300); button(1,"Remover conexão",544,680,300);
    text(TXT_V2_GRUPO,"Sua Biblioteca continua sua",1020,420,243,680);
    paragraph("Títulos salvos via Trakt, Simkl ou conta Nuvio ficam na Biblioteca.",1020,478,680);
  } else {
    panel(200,386,750,430,-1);
    text(TXT_V2_GRUPO,recoveryAuth?"Sua conexão precisa de aprovação":"Verifique o servidor",232,420,243,680);
    paragraph(recoveryAuth?"Suas escolhas de servidor e bibliotecas estão salvas. Entre de novo para voltar a sincronizar e reproduzir.":"Ligue o servidor e confira a conexão de rede, depois tente de novo.",232,474,680);
    button(0,recoveryAuth?provider==0?"Entrar no Plex":provider==1?"Usar Quick Connect":"Entrar com o Emby Connect":"Tentar de novo",232,590,330);
    button(1,recoveryAuth?"Usuário e senha":"Trocar endereço",574,590,330);
    paragraph(recoveryAuth?"As outras fontes disponíveis continuam na lista de fontes do título.":"O catálogo salvo continua disponível na Biblioteca filtrada.",232,660,680);
    text(TXT_V2_GRUPO,"Continuar navegando",1020,412,243,680);
    button(2,"Abrir a Biblioteca",1020,470,380);
    if(!recoveryAuth) button(3,"Seus servidores",1020,548,380);
    else { char stamp2[96];syncLabel(provider,stamp2,sizeof stamp2);text(TXT_ILHA_APOIO,stamp2,1020,548,155,680); }
  }
  if(message[0] && screen!=ERROR_SCREEN && screen!=ROWS) text(TXT_ILHA_TEXTO,message,200,920,215,1624);
  if (bodyDrawing) gfx_sem_recorte();
  bodyDrawing = 0;
  if (screen != ROWS) {
    float hx = dicaChip("OK","Escolher",200,1014);
    dicaChip("Voltar","Voltar",hx,1014);
    TxtLinha nota = txt_linha(TXT_ILHA_APOIO, i18n("Conexão salva só nesta TV e neste perfil"), 145,145,145,255);
    txt_desenhar(nota,px(1824)-nota.w,ANC(1018));
  }
  gfx_sem_transformar();
  if (previousPage.tex && !gfx_mini_ativo())
    gfx_mini_desenhar(&previousPage, (GfxRect){0,0,NV_TELA_W,NV_TELA_H}, 0, 1 - anim_saida(page));
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
