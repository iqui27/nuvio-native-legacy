#include "visto.h"
#include "perfisessao.h"
#include "trakt.h"
#include "simkl.h"
#include "contapend.h"
#include "sessao.h"
#include "ajustes.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Optional imports belong to consumers: public declarations keep producers strong.
extern int trakt_destino_capturar(TraktDestino *) NV_PERFIL_OPCIONAL;
extern int trakt_episodios_marcar_destino(const TraktDestino *, const char *, const VistoPar *, int, int) NV_PERFIL_OPCIONAL;
extern int trakt_episodios_marcar_alvos_destino(const TraktDestino *, const char *, const TraktEpisodioAlvo *, int, int) NV_PERFIL_OPCIONAL;
extern int cat_episodio_tvdb(int, const char *, int, int) NV_PERFIL_OPCIONAL;
extern unsigned cat_versao_episodios(void) NV_PERFIL_OPCIONAL;
extern int simkl_destino_capturar(SimklDestino *) NV_PERFIL_OPCIONAL;
extern int simkl_episodios_marcar_destino(const SimklDestino *, const char *, const VistoPar *, int, int) NV_PERFIL_OPCIONAL;
extern int simkl_titulo_marcar_destino(const SimklDestino *, const char *, const char *, const int *, int, int) NV_PERFIL_OPCIONAL;

// Teto do lote: o mesmo do Simkl, que e o menor dos tres (Trakt aceita 256
// desde este mesmo conserto; a conta nao tem teto no cliente).
#define VT_LOTE SMK_LOTE_MAX
#define VT_TEMPS 64

int visto_destinos(void) {
  return (trakt_ativo() ? VISTO_TRAKT : 0) |
         (simkl_ativo() ? VISTO_SIMKL : 0) |
         // "Enviar histórico para a conta Nuvio" (por perfil): desligado, o visto
         // deste perfil so vai para o Trakt e o Simkl que ele tiver vinculados.
         (sessao_logada() && ajustes_hist_conta() ? VISTO_CONTA : 0);
}

// Retry uses the same synchronous service writers as the first attempt.
int visto_reenviar(const char *imdb, const char *tipo, const VistoPar *pares, int n,
                   int visto, int destinos, const int *temps, int nt,
                   const int *tvdbEps) {
  int ok = 0;
  if (destinos & VISTO_TRAKT) {
    TraktDestino d;
    if (trakt_destino_capturar && trakt_destino_capturar(&d)) {
      int enviado = 0, tentouExato = 0;
      if (n && tvdbEps) {
        TraktEpisodioAlvo alvos[VT_LOTE];
        int temExato = 0;
        for (int i = 0; i < n; i++) {
          alvos[i].par = pares[i];
          alvos[i].tvdbEp = tvdbEps[i] > 0 ? tvdbEps[i] : 0;
          if (alvos[i].tvdbEp) temExato = 1;
        }
        if (temExato) {
          tentouExato = 1;
          if (trakt_episodios_marcar_alvos_destino)
            enviado = trakt_episodios_marcar_alvos_destino(&d, imdb, alvos, n, visto);
        }
      }
      if (!tentouExato)
        enviado = n ? trakt_episodios_marcar_destino(&d, imdb, pares, n, visto)
                    : trakt_titulo_marcar_destino(&d, imdb, tipo, visto);
      if (enviado) ok |= VISTO_TRAKT;
    }
  }
  if (destinos & VISTO_SIMKL) {
    SimklDestino d;
    if (simkl_destino_capturar && simkl_destino_capturar(&d) &&
        (n ? simkl_episodios_marcar_destino(&d, imdb, pares, n, visto)
           : simkl_titulo_marcar_destino(&d, imdb, tipo, temps, nt, visto))) ok |= VISTO_SIMKL;
  }
  return ok;
}

void visto_iniciar(void) { contapend_reenviar_com(visto_reenviar); }

int visto_episodios_ja(const char *imdb, const char *tipo, const VistoPar *pares,
                       int n, int visto, int destinos) {
  int ok = 1;
  if (!imdb || !imdb[0] || !pares || n < 1) return 0;
  if (n > VT_LOTE) n = VT_LOTE;
  NvPerfilSessao sessao = nv_perfil_sessao_ler();
  if (!nv_perfil_sessao_travar(sessao)) return 0;
  long long ms = contapend_gesto(imdb, tipo, pares, n, visto, destinos, NULL, 0);
  // Ordem: Trakt, Simkl, conta. Nenhum depende do outro; a falha de um nao
  // impede os seguintes (cada um diz no log o que aconteceu).
  int trackers = destinos & (VISTO_TRAKT | VISTO_SIMKL);
  int enviados = visto_reenviar(imdb, tipo, pares, n, visto, trackers, NULL, 0, NULL);
  contapend_confirmar_gesto(imdb, pares, n, ms, enviados);
  ok &= enviados == trackers;
  nv_perfil_sessao_destravar();
  // A CONTA PASSA PELO JORNAL (contapend.c): o gesto fica em disco antes da
  // rede e so sai de la com 2xx. Era uma RPC solta — sem rede, a marca se
  // perdia. Quem ja registrou (visto_episodios) manda VISTO_CONTA_JORNAL.
  if (destinos & VISTO_CONTA) {
    ok &= contapend_enviar() >= 0 ? 1 : 0;
  }
  return ok;
}

int visto_titulo_ja(const char *imdb, const char *tipo, const int *temporadas,
                    int nt, int visto, int destinos) {
  int ok = 1;
  if (!imdb || !imdb[0]) return 0;
  NvPerfilSessao sessao = nv_perfil_sessao_ler();
  if (!nv_perfil_sessao_travar(sessao)) return 0;
  long long ms = contapend_gesto(imdb, tipo, NULL, 0, visto, destinos, temporadas, nt);
  int trackers = destinos & (VISTO_TRAKT | VISTO_SIMKL);
  int enviados = visto_reenviar(imdb, tipo, NULL, 0, visto, trackers,
                                temporadas, nt, NULL);
  contapend_confirmar_gesto(imdb, NULL, 0, ms, enviados);
  if (enviados) ok &= contapend_reenviar_overrides(imdb, ms) >= 0;
  ok &= enviados == trackers;
  nv_perfil_sessao_destravar();
  if (destinos & VISTO_CONTA) {
    ok &= contapend_enviar() >= 0 ? 1 : 0;
  }
  return ok;
}

// A COPIA E DO FIO: quem chamou pode ter o CatItem trocado de bloco pela
// descoberta antes de o fio ler (mesma razao do Envio de episodios.c).
typedef struct {
  char imdb[24], tipo[12];
  VistoPar pares[VT_LOTE];
  TraktEpisodioAlvo alvosTrakt[VT_LOTE];
  int temps[VT_TEMPS];
  int n, nt, visto, destinos, titulo, temAlvoTrakt, confirmarSemEnvio;
  long long ms;
  int perfil;
  NvPerfilSessao sessao; TraktDestino trakt; SimklDestino simkl;
} Envio;

static void *fio(void *u) {
  Envio *e = (Envio *)u;
  // The UI/journal already received the gesture before this worker started.
  // Serialize tracker writes with retries through confirmation.
  // ponytail: profile-wide gate; use per-item gates if write throughput matters.
  if (!nv_perfil_sessao_travar(e->sessao)) { free(e); return NULL; }
  // Keep only journal entries still owned by this queued gesture.
  int n = 0, temAlvoTrakt = 0;
  for (int i = 0; i < e->n; i++)
    if (contapend_gesto_atual(e->imdb, e->pares[i].temporada, e->pares[i].episodio, e->ms)) {
      e->pares[n] = e->pares[i];
      e->alvosTrakt[n] = e->alvosTrakt[i];
      if (e->alvosTrakt[n].tvdbEp > 0) temAlvoTrakt = 1;
      n++;
    }
  e->n = n;
  if (e->temAlvoTrakt) e->temAlvoTrakt = temAlvoTrakt;
  if (e->titulo ? !contapend_gesto_atual(e->imdb, 0, 0, e->ms) : !n) {
    nv_perfil_sessao_destravar(); free(e); return NULL;
  }
  // A catalog revision during identity capture makes this Trakt batch unsafe.
  // Mark that destination handled in the retry journal without sending a
  // potentially wrong numeric pair later; independent destinations proceed.
  int enviados = e->confirmarSemEnvio;
  if (e->destinos & VISTO_TRAKT) {
    int ok = e->titulo ? trakt_titulo_marcar_destino(&e->trakt, e->imdb, e->tipo, e->visto)
           : e->temAlvoTrakt && trakt_episodios_marcar_alvos_destino
             ? trakt_episodios_marcar_alvos_destino(&e->trakt, e->imdb, e->alvosTrakt, e->n, e->visto)
           : trakt_episodios_marcar_destino
             ? trakt_episodios_marcar_destino(&e->trakt, e->imdb, e->pares, e->n, e->visto)
             : trakt_episodios_marcar(e->imdb, e->pares, e->n, e->visto);
    if (ok) enviados |= VISTO_TRAKT;
  }
  if (e->destinos & VISTO_SIMKL) {
    int ok;
    if (e->titulo)
      ok = simkl_titulo_marcar_destino ? simkl_titulo_marcar_destino(&e->simkl, e->imdb, e->tipo, e->temps, e->nt, e->visto)
           : simkl_titulo_marcar(e->imdb, e->tipo, e->temps, e->nt, e->visto);
    else
      ok = simkl_episodios_marcar_destino ? simkl_episodios_marcar_destino(&e->simkl, e->imdb, e->pares, e->n, e->visto)
           : simkl_episodios_marcar(e->imdb, e->pares, e->n, e->visto);
    if (ok) enviados |= VISTO_SIMKL;
  }
  contapend_confirmar_gesto(e->imdb, e->pares, e->n, e->ms, enviados);
  if (e->titulo && enviados) contapend_reenviar_overrides(e->imdb, e->ms);
  nv_perfil_sessao_destravar();
  if (e->destinos & VISTO_CONTA) contapend_enviar(); // each journal record owns its account/profile
  memset(e, 0, sizeof *e);
  free(e);
  return NULL;
}

static int lancar(Envio *e) {
  pthread_t t;
  if (pthread_create(&t, NULL, fio, e) != 0) { free(e); return 0; }
  pthread_detach(t);
  return 1;
}

static void capturarDestinos(Envio *e) { // caller holds the profile opening gate
  if ((e->destinos & VISTO_TRAKT) && trakt_destino_capturar && !trakt_destino_capturar(&e->trakt)) e->destinos &= ~VISTO_TRAKT;
  if ((e->destinos & VISTO_SIMKL) && simkl_destino_capturar && !simkl_destino_capturar(&e->simkl)) e->destinos &= ~VISTO_SIMKL;
}
static int episodiosEnviar(int indice, const char *imdb, const char *tipo,
                           const VistoPar *pares, int n, int visto, int destinos) {
  if (!imdb || !imdb[0] || !pares || n < 1) return 0;
  if (n > VT_LOTE) n = VT_LOTE;
  NvPerfilSessao sessao = nv_perfil_sessao_ler();
  if (!nv_perfil_sessao_travar(sessao)) return 0;
  int tvdbEps[VT_LOTE] = {0}, temAlvoTrakt = 0, confirmarSemEnvio = 0;
  int destinosFio = destinos;
  if (indice >= 0 && (destinos & VISTO_TRAKT) &&
      (!tipo || !tipo[0] || !strcmp(tipo, "series")) &&
      cat_episodio_tvdb) {
    unsigned versao = cat_versao_episodios ? cat_versao_episodios() : 0;
    for (int i = 0; i < n; i++) {
      tvdbEps[i] = cat_episodio_tvdb(indice, imdb,
          pares[i].temporada, pares[i].episodio);
      if (tvdbEps[i] > 0) temAlvoTrakt = 1;
    }
    if (cat_versao_episodios && cat_versao_episodios() != versao) {
      memset(tvdbEps, 0, sizeof tvdbEps);
      temAlvoTrakt = 0; destinosFio &= ~VISTO_TRAKT;
      confirmarSemEnvio |= VISTO_TRAKT;
      printf("[visto] %s: lista de episodios mudou; lote Trakt nao enviado\n", imdb);
      fflush(stdout);
    }
  }
  long long ms = contapend_gesto_alvos(imdb, tipo, pares, tvdbEps, n, visto,
                                       destinos, NULL, 0);
  if (confirmarSemEnvio)
    contapend_confirmar_gesto(imdb, pares, n, ms, confirmarSemEnvio);
  Envio *e = calloc(1, sizeof *e);
  if (!e) { nv_perfil_sessao_destravar(); return 0; }
  snprintf(e->imdb, sizeof e->imdb, "%s", imdb);
  snprintf(e->tipo, sizeof e->tipo, "%s", tipo && tipo[0] ? tipo : "series");
  memcpy(e->pares, pares, sizeof(VistoPar) * (size_t)n);
  e->n = n; e->visto = visto; e->destinos = destinosFio;
  e->sessao = sessao; e->ms = ms; e->temAlvoTrakt = temAlvoTrakt;
  e->confirmarSemEnvio = confirmarSemEnvio;
  for (int i = 0; i < n; i++) {
    e->alvosTrakt[i].par = e->pares[i];
    e->alvosTrakt[i].tvdbEp = tvdbEps[i];
  }
  capturarDestinos(e);
  int ok = lancar(e);
  nv_perfil_sessao_destravar(); return ok;
}
int visto_episodios(const char *imdb, const char *tipo, const VistoPar *pares,
                    int n, int visto, int destinos) {
  return episodiosEnviar(-1, imdb, tipo, pares, n, visto, destinos);
}
int visto_episodios_item(int indice, const char *imdb, const char *tipo,
                        const VistoPar *pares, int n, int visto, int destinos) {
  return episodiosEnviar(indice, imdb, tipo, pares, n, visto, destinos);
}
int visto_titulo(const char *imdb, const char *tipo, const int *temporadas,
                 int nt, int visto, int destinos) {
  if (!imdb || !imdb[0]) return 0;
  NvPerfilSessao sessao = nv_perfil_sessao_ler();
  if (!nv_perfil_sessao_travar(sessao)) return 0;
  vistoep_titulo_gesto(imdb, visto);
  long long ms = contapend_gesto(imdb, tipo, NULL, 0, visto, destinos, temporadas, nt);
  Envio *e = calloc(1, sizeof *e);
  if (!e) { nv_perfil_sessao_destravar(); return 0; }
  snprintf(e->imdb, sizeof e->imdb, "%s", imdb);
  snprintf(e->tipo, sizeof e->tipo, "%s", tipo && tipo[0] ? tipo : "movie");
  if (temporadas && nt > 0) {
    if (nt > VT_TEMPS) nt = VT_TEMPS;
    memcpy(e->temps, temporadas, sizeof(int) * (size_t)nt); e->nt = nt;
  }
  e->visto = visto; e->destinos = destinos; e->titulo = 1; e->sessao = sessao; e->ms = ms;
  capturarDestinos(e);
  int ok = lancar(e);
  nv_perfil_sessao_destravar(); return ok;
}
