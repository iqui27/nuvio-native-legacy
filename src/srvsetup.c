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
static void change(int next) { screen = next; selected = 0; message[0] = 0; }
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
  teclado_contexto("Biblioteca"); teclado_abrir_com(label, "", f == 1 ? 400 : f == 2 ? 120 : 250, alphabet,
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
int srvsetup_aberto(void) { return opened; }
void srvsetup_abrir(int p) {
  if (opened) srvsetup_fechar();
  ajustes_definir_servidores(1);
  profile = perfis_ativo(); provider = p >= 0 && p < 3 ? p : 0;
  syncTimes(); recoveryAuth = 0;
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
      if (!selected && provider == 1 && jellyfin_qc_permitido() == 1) enterCode();
      else if (selected == 2 && provider == 2) { change(CONNECT_CODE); codeSince = SDL_GetTicks(); makeQR("https://emby.media/pin"); startWorker(JOB_CONNECT_PIN); }
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
      } else if (screen == CONNECT_PASSWORD) { change(CONNECT_CODE); makeQR("https://emby.media/pin"); startWorker(JOB_CONNECT_PIN); }
      else change(LOGIN); break;
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
      } else if (selected == nLibraries + 1) { change(ROWS); librariesLoad(); }
      else change(REMOVE); break;
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
    case ADDRESS: case LOGIN: case CONNECT_CODE: return 3;
    case ERROR_SCREEN: return recoveryAuth ? 3 : 4;
    case CODE: case PASSWORD: case CONNECT_PASSWORD: return 4;
    case PLEX_SERVERS: return (worker.n > 0 ? worker.n : 0) + 2;
    case CONNECT_SERVERS: return (worker.n > 0 ? worker.n : 0) + 1;
    case LIBRARIES: return nLibraries + 3;
    case ROWS: return nLibraries + 5;
    default: return 2;
  }
}
void srvsetup_evento(const SDL_Event *e) {
  if (!opened) return;
  if (teclado_aberto()) { teclado_evento(e); return; }
  if (e->type != SDL_KEYDOWN || e->key.repeat) return;
  SDL_Keycode k = e->key.keysym.sym;
  if (k == SDLK_ESCAPE || k == SDLK_BACKSPACE) {
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
  txt_desenhar(txt_linha_corta(style, i18n(s), tone, tone, tone, 255, width), x, y);
}
static void paragraph(const char *s, float x, float y, float width) {
  txt_bloco(TXT_ILHA_TEXTO, i18n(s), 155, 155, 155, x, y, width, 30, 1, 3);
}
static void panel(float x, float y, float w, float h, int focus) {
  GfxRect r = {x,y,w,h};
  gfx_vidro_folha(r, 30.0f / h, 1);
  if(focus)gfx_cor(r,30.0f/h,1,1,1,.13f);
}
static void logo(int p, float x, float y, float size) {
  GLuint t = tex_obter_larg(marks[p], size);
  if (t) { float a = tex_aspecto(marks[p]); if (a <= 0) a = 1; gfx_rect((GfxRect){x,y + (size - size / a) / 2,size,size / a}, t, GFX_TEXTO, 0,0,0,0,1,1,1,1); }
}
static void button(int i, const char *label, float x, float y, float w) {
  GfxRect r = {x,y,w,56};
  if (selected == i) gfx_vidro_pilula_cheia(r, .5f, 1, 1); else gfx_cor(r, .5f, 1, 1, 1, .08f);
  int cr = 243, cg = 242, cb = 239; if (selected == i) cr = cg = cb = ajustes_tinta_foco();
  TxtLinha t = txt_linha_corta(TXT_G21B, i18n(label), cr,cg,cb,255,w-30);
  txt_desenhar(t,x+(w-t.w)/2,y+(56-t.h)/2);
}
static void row(int i, const char *title, const char *sub, float y, int p) {
  if (selected == i) panel(220,y,710,86,1);
  if (p >= 0) logo(p,242,y+21,40);
  text(TXT_ILHA_NOME,title,p>=0?304:248,y+12,243,580);
  text(TXT_ILHA_SUB,sub,p>=0?304:248,y+48,145,580);
}
static void syncLabel(int p, char *out, size_t n) {
  if(!lastSync[p]) { snprintf(out,n,"%s",i18n("sem sincronizar"));return; }
  time_t stamp=(time_t)lastSync[p];struct tm local;char date[40];
  localtime_r(&stamp,&local);strftime(date,sizeof date,"%d/%m %H:%M",&local);
  snprintf(out,n,i18n("Sincronizada: %s"),date);
}
static void drawQR(void) {
  if (!qr.lado) return;
  float x=1052,y=418,side=246,unit=side/(qr.lado+8);
  gfx_cor((GfxRect){x,y,side,side},.08f,1,1,1,1);
  for (int j=0;j<qr.lado;j++) for(int i=0;i<qr.lado;i++) if(qr_modulo(&qr,i,j))
    gfx_cor((GfxRect){x+(i+4)*unit,y+(j+4)*unit,unit+.1f,unit+.1f},0,0,0,0,1);
}
void srvsetup_desenhar(Uint32 now) {
  if (!opened) return;
  gfx_cor((GfxRect){160,0,1760,1080},0,.035f,.038f,.044f,1);
  gfx_luz_canto((GfxRect){160,0,1760,850},0,1400,0,1700,.17f,.32f,.34f,.35f);
  const char *title = screen == CHOOSE ? "Sua mídia, na sua Biblioteca" : screen == MANAGE ? "Seus servidores" :
    screen == DISCOVER ? "Seus servidores" : screen == ADDRESS ? "Endereço" :
    screen == CODE ? provider ? "Entrar com código" : "Entrar com código" : screen == CONNECT_CODE ? "Emby Connect" :
    screen == USERS ? "Quem está assistindo?" : screen == PASSWORD || screen == CONNECT_PASSWORD ? "Entrar" :
    screen == LIBRARIES ? "Escolher" : screen == READY ? "Pronto" : screen == REMOVE ? "Remover" :
    screen == PLEX_SERVERS || screen == CONNECT_SERVERS ? "Seus servidores" : screen == ERROR_SCREEN ? recoveryAuth?"Entrar na sua conta":"Não consegui falar com o servidor." : screen == ROWS ? "Conteúdo da Home" : "Seus servidores";
  if(screen!=ROWS) {
    char context[160]; snprintf(context,sizeof context,"%s › %s",i18n("Biblioteca"),i18n("Seus servidores"));
    if (screen != CHOOSE && screen != MANAGE) { logo(provider,200,124,34); text(TXT_ILHA_APOIO,context,252,132,130,1500); }
    else text(TXT_ILHA_APOIO,context,200,132,130,1500);
    text(TXT_V2_TIT,title,200,180,243,1600);
    const char *intro=screen==USERS?"Cada perfil tem sua própria lista e seu progresso.":
      screen==ADDRESS?"IP e porta do servidor, como 192.168.1.5:8096, ou o endereço com https://":
      screen==ERROR_SCREEN?recoveryAuth?"O que você assiste volta pro servidor: o visto e o ponto onde parou.":"Confira o endereço e se o servidor está ligado, em Ajustes > Avançado.":
      screen==CODE||screen==CONNECT_CODE?"Abra este endereço no celular e digite o código:":
      screen==PASSWORD||screen==CONNECT_PASSWORD?"Usuário e senha":"Suas bibliotecas viram fileiras na Home, com detalhes e retomar de onde parou.";
    paragraph(intro,200,250,1540);
    const char *steps[] = {"Serviço","Conectar","Entrar","Biblioteca","Pronto"};
    int step=screen==CHOOSE?0:screen==LIBRARIES?3:screen==READY?4:screen==DISCOVER||screen==ADDRESS?1:2;
    float x=200;
    for(int i=0;i<5;i++) {
      char label[120]; snprintf(label,sizeof label,"%d  %s",i+1,i18n(steps[i]));
      if(i<step)gfx_icone((GfxRect){x,330,17,17},"check",.6f,.6f,.6f,1);
      text(TXT_ILHA_APOIO,label,x+24,328,i==step?243:112,170); x+=195;
    }
  }
  if (screen == CHOOSE) {
    const char *desc[] = {"Mostra um código de 4 letras: digite em plex.tv/link no celular. Nada é digitado nesta TV; só um token fica aqui.","Usa o Quick Connect se o servidor permitir: aprove o código em outro app do Jellyfin. Senão, pede usuário e senha.","Usuário e senha"};
    for(int i=0;i<3;i++) { float x=200+i*550; panel(x,405,523,274,selected==i); logo(i,x+34,438,66); text(TXT_G30B,providers[i],x+34,540,243,450); paragraph(desc[i],x+34,592,455); }
    paragraph("Este perfil nesta TV",200,714,1500);
  } else if (screen == CODE || screen == CONNECT_CODE) {
    panel(200,386,750,390,0); panel(1020,386,804,390,0);
    const char *url = screen==CONNECT_CODE?"emby.media/pin":provider?"Quick Connect":"plex.tv/link";
    text(TXT_ILHA_SUB,url,232,422,150,680);
    text(TXT_LOG_COD,code[0]?code:"…",232,470,243,680);
    unsigned elapsed=(now-codeSince)/1000, left=elapsed<300?300-elapsed:0; char remaining[128];
    snprintf(remaining,sizeof remaining,i18n("Aguardando aprovação · %02d:%02d"),left/60,left%60);
    text(TXT_ILHA_TEXTO,remaining,232,580,155,690);
    button(0,"Continuar",232,630,270); button(1,"Novo código",514,630,200);
    button(2,screen==CONNECT_CODE?"Usuário e senha":provider?"Usuário e senha":"Buscar",232,704,300);
    if(screen==CODE) button(3,"Endereço",1052,690,300);
    drawQR(); text(TXT_G28B,"Abrir no celular",1332,494,243,450);
    paragraph("Abra este endereço no celular e digite o código:",1332,548,430);
  } else if (screen == MANAGE) {
    panel(200,386,1624,470,0);
    for(int i=0;i<3;i++) { char label[192],stamp[96],sub[160]; snprintf(label,sizeof label,"%s · %s",providers[i],serverName(i)); syncLabel(i,stamp,sizeof stamp);snprintf(sub,sizeof sub,"%s · %s",i18n(connected(i)?"Conectado":"Não configurado"),stamp);row(i,label,sub,410+i*92,i); button(i+4,"Remover",1100,425+i*92,310); }
    button(3,"Seus servidores",232,738,320);
  } else if (screen == USERS) {
    panel(200,386,750,402,0);
    int total=!worker.alive&&worker.n>0?worker.n:0,first=selected<total?(selected/3)*3:0;
    if(worker.alive)paragraph("Carregando…",232,440,680);
    for(int j=0;j<3 && first+j<total;j++) {
      int i=first+j;float x=232+j*182;
      if(selected==i)panel(x,442,150,202,1);
      GLuint avatar=worker.users[i].imagem[0]?tex_obter_larg(worker.users[i].imagem,100):0;
      if(avatar)gfx_rect((GfxRect){x+25,458,100,100},avatar,GFX_AVATAR,0,0,0,.5f,1,1,1,1);
      else { gfx_cor((GfxRect){x+25,458,100,100},.5f,1,1,1,.08f); gfx_icone((GfxRect){x+53,486,44,44},"user",.6f,.6f,.6f,1); }
      text(TXT_G21B,worker.users[i].nome,x+10,582,243,130);
    }
    button(total,"Trocar de usuário",232,674,270);
    button(total+1,provider==1?"Quick Connect":"Emby Connect",514,674,300);
    text(TXT_G30B,"Perfil",1020,412,243,680);
    paragraph("Cada perfil tem sua própria lista e seu progresso.",1020,470,680);
    paragraph("Este perfil nesta TV",1020,566,680);
  } else if (screen == DISCOVER || screen == PLEX_SERVERS || screen == CONNECT_SERVERS) {
    panel(200,386,750,500,0);
    int total=!worker.alive && worker.n>0?worker.n:0, n=total>4?4:total;
    int start=selected>3?selected-3:0; if(start+n>total)start=total-n; if(start<0)start=0;
    if(worker.alive) paragraph(screen==DISCOVER?"Procurando…":"Carregando…",232,420,680);
    else if(!n) paragraph(screen==DISCOVER?"Nada encontrado":"Não configurado",232,420,680);
    for(int j=0;j<n;j++) {
      int i=start+j;
      const char *name=screen==DISCOVER?worker.servers[i].nome:screen==USERS?worker.users[i].nome:screen==PLEX_SERVERS?worker.plex[i].nome:worker.connect[i].nome;
      row(i,name,"Servidor",410+j*88,screen==USERS?-1:provider);
      if (screen == USERS && worker.users[i].imagem[0]) {
        GLuint avatar = tex_obter_larg(worker.users[i].imagem, 64);
        if (avatar) gfx_rect((GfxRect){850,421+j*88,64,64},avatar,GFX_AVATAR,0,0,0,.5f,1,1,1,1);
      }
    }
    float y= n?430+n*88:600;
    button(total,screen==USERS?"Trocar de usuário":screen==CONNECT_SERVERS?"Usuário e senha":screen==PLEX_SERVERS?"Buscar":"Endereço",232,y,330);
    if(screen!=CONNECT_SERVERS) button(total+1,screen==USERS?provider==1?"Quick Connect":"Emby Connect":screen==PLEX_SERVERS?"Endereço":"Buscar de novo",574,y,330);
    text(TXT_G30B,"Biblioteca",1020,412,243,750);
    paragraph("Suas bibliotecas viram fileiras na Home, com detalhes e retomar de onde parou.",1020,470,680);
    paragraph("Este perfil nesta TV",1020,620,680);
    if (screen == DISCOVER && provider == 2) button(total+2,"Emby Connect",1020,700,330);
  } else if (screen == ADDRESS || screen == PASSWORD || screen == CONNECT_PASSWORD) {
    panel(200,386,750,430,0); panel(1020,386,804,430,0);
    if(screen==ADDRESS) { button(0,addressText[0]?addressText:"Endereço",232,440,680); button(1,"Conectar",232,580,260); button(2,"Buscar",510,580,300); }
    else { button(0,username[0]?username:provider==2?"Usuário do Emby":"Usuário do Jellyfin",232,420,680); button(1,password[0]?"••••••••":"Senha",232,500,680); button(2,"Entrar",232,620,260); button(3,screen==CONNECT_PASSWORD?"Emby Connect":"Voltar",510,620,300); }
    text(TXT_G30B,"Teclado",1052,420,243,680);
    paragraph("Em campos de texto, o botão do celular mostra um QR. Você digita no telefone e o texto chega na TV.",1052,480,680);
    if (screen != ADDRESS) text(TXT_ILHA_SUB,"Senha",1052,620,155,680);
  } else if(screen==ROWS) {
    gfx_cor((GfxRect){160,0,1760,1080},0,0,0,0,.3f);
    text(TXT_ILHA_SUB,"Home",200,570,150,510);
    text(TXT_G30B,"Conteúdo da Home",200,614,243,510);
    for(int i=0;i<3;i++) {
      const CatItem *it=cat_item(i); if(!it)break;
      GLuint poster=tex_obter_larg(it->poster,160);
      if(poster)gfx_rect((GfxRect){200+i*180,710,160,240},poster,GFX_TEXTO,0,0,0,.05f,1,1,1,1);
    }
    panel(780,24,1116,1032,0);
    text(TXT_ILHA_APOIO,"Adicionar à Home",820,64,145,1010);
    text(TXT_ILHA_TITULO,"Conteúdo da Home",820,105,243,1010);
    paragraph("Uma lista boa fica fixada na Biblioteca, ou vira uma fileira na tela inicial.",820,171,990);
    text(TXT_ILHA_SUB,"Servidores pessoais",820,252,225,1010); logo(provider,820,300,32);
    text(TXT_G21B,providers[provider],870,304,243,900);
    int shown=nLibraries>4?4:nLibraries;
    int first=selected<nLibraries&&selected>3?selected-3:rowChoice>3?rowChoice-3:0;
    if(first+shown>nLibraries)first=nLibraries-shown;
    if(first<0)first=0;
    for(int j=0;j<shown;j++) {
      int i=first+j;float y=366+j*100;
      if(selected==i)panel(820,y,1036,92,1);
      logo(provider,844,y+25,34);text(TXT_ILHA_NOME,libraries[i].titulo,902,y+14,243,900);
      text(TXT_ILHA_SUB,!strcmp(libraries[i].tipo,"group")?"Coleções":!strcmp(libraries[i].tipo,"list")?"Listas":"Biblioteca",902,y+50,145,900);
    }
    if(nLibraries && rowChoice<nLibraries) {
      const char *key=libraries[rowChoice].chave;char label[180];
      snprintf(label,sizeof label,"%s · %s",i18n("Estilo"),i18n(fil_estilo_rotulo(key,fil_tipo(key))));
      button(nLibraries,label,820,794,325);
      snprintf(label,sizeof label,"%s · %s",i18n("Ordem"),i18n(fil_servidor_ordem(key)?"Título":"Padrão"));
      button(nLibraries+1,label,1157,794,310);
      snprintf(label,sizeof label,"%s · %s",i18n("Ocultar vistos"),i18n(fil_servidor_ocultar_vistos(key)?"Ligado":"Desligado"));
      button(nLibraries+2,label,1479,794,365);
    }
    button(nLibraries+3,"Adicionar à Home",820,882,330);button(nLibraries+4,"Cancelar",1162,882,260);
    if(message[0])text(TXT_ILHA_SUB,message,820,968,215,1000);
  } else if(screen==LIBRARIES) {
    panel(200,386,750,500,0);
    if(!nLibraries) paragraph(worker.alive?"Carregando…":"Não configurado",232,420,680);
    int shown=nLibraries>4?4:nLibraries;
    int start=selected>3?selected-3:0; if(start+shown>nLibraries)start=nLibraries-shown; if(start<0)start=0;
    for(int j=0;j<shown;j++){int i=start+j; char label[192]; snprintf(label,sizeof label,"%s  %s",screen==ROWS?"":checked[i]?"[x]":"[ ]",libraries[i].titulo); row(i,label,libraries[i].tipo,410+j*88,provider);}
    button(nLibraries,"Continuar",232,800,220); button(nLibraries+1,"Adicionar à Home",464,800,260); button(nLibraries+2,"Remover",1020,740,310);
    text(TXT_G30B,"Biblioteca",1020,412,243,700);
    paragraph("Suas bibliotecas viram fileiras na Home, com detalhes e retomar de onde parou.",1020,470,680);
    paragraph("No seu servidor",1020,600,680);
  } else if(screen==LOGIN) {
    panel(200,386,750,400,0); panel(1020,386,804,400,0);
    text(TXT_G30B,providers[provider],232,420,243,680);
    button(0,provider==1&&jellyfin_qc_permitido()==1?"Quick Connect":"Usuário e senha",232,500,330);
    button(1,"Trocar de usuário",232,580,330);
    if(provider==2)button(2,"Emby Connect",232,660,330); else button(2,"Usuário e senha",232,660,330);
    paragraph("Este perfil nesta TV",1052,440,680);
  } else if(screen==READY) {
    panel(200,386,750,430,0); text(TXT_G30B,"Biblioteca",232,420,243,680);
    const char *labels[]={"Biblioteca","Em andamento","Fontes"};
    for(int i=0;i<3;i++)text(TXT_ILHA_NOME,labels[i],250,486+i*65,225,650);
    button(0,"Abrir a Biblioteca",232,730,300); button(1,"Adicionar à Home",544,730,300);
    paragraph("No seu servidor",1020,440,680);
  } else if(screen==REMOVE) {
    panel(200,386,750,430,0); paragraph("Remove a conexão e o catálogo deste perfil. Não apaga arquivos do servidor.",232,430,680);
    paragraph("Este perfil nesta TV",232,540,680);
    button(0,"Voltar",232,700,300); button(1,"Remover",544,700,300);
  } else {
    panel(200,386,750,360,0);
    text(TXT_G30B,recoveryAuth?"Entrar na sua conta":"Servidor",232,420,243,680);
    paragraph(message[0]?message:recoveryAuth?"Usuário e senha":"O servidor não respondeu dentro do prazo.",232,480,680);
    button(0,recoveryAuth?provider==0?"Entrar no Plex":provider==1?"Quick Connect":"Emby Connect":"Tentar de novo",232,610,300);
    button(1,recoveryAuth?"Usuário e senha":"Endereço",544,610,330);
    text(TXT_G30B,"Biblioteca",1020,412,243,680);
    char stamp[96];syncLabel(provider,stamp,sizeof stamp);text(TXT_ILHA_SUB,stamp,1020,474,155,680);
    button(2,"Abrir a Biblioteca",1020,550,380);
    if(!recoveryAuth)button(3,"Seus servidores",1020,628,380);
  }
  if(message[0] && screen!=ERROR_SCREEN && screen!=ROWS) text(TXT_ILHA_TEXTO,message,200,920,215,1624);
  text(TXT_ILHA_APOIO,"Escolher",200,1028,145,180);
  text(TXT_ILHA_APOIO,"Voltar",410,1028,145,180);
  text(TXT_ILHA_APOIO,"Este perfil nesta TV",1160,1028,145,660);
  if(teclado_aberto()) teclado_desenhar(now);
}

static int originRows(int index, const char **name, int *count) {
  if (index < 0 || index > 3) return 0;
  *name=index<3?providers[index]:i18n("Seus servidores");
  *count=index<3?libraryProviderCounts[index]:0;
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
