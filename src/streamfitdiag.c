#include "streamfitdiag.h"
#include "streamfit.h"
#include "redemarca.h"
#include "vazao.h"
#include <ctype.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

/* Android: the player pulls progressive files over 4 connections at once
 * (ParaleloDataSource.kt) because the OS caps each connection's TCP window, so
 * the speed test must too (v1.7.4 did; 2.0 regressed to one: 50 -> 18 Mbps on
 * the owner's TCL). The extra connections read distant ranges of the same
 * file and their per-second rates are summed. LG/Samsung play on one. */
#ifndef STREAMFITDIAG_CONEXOES
#ifdef NV_ANDROID
#define STREAMFITDIAG_CONEXOES 4
#else
#define STREAMFITDIAG_CONEXOES 1
#endif
#endif
#define STREAMFITDIAG_SALTO (256L * 1024L * 1024L)
typedef struct {
  const StreamfitDiagControle *controle;
  int n, invalida;
  uint64_t bytes;
  int kbps[STREAMFIT_AMOSTRAS_MAX];
} Medida;
static int parar(void *arg) {
  Medida *m = arg;
  return !m->controle->rede || redemarca_atual() != m->controle->rede ||
      (m->controle->cancelado && m->controle->cancelado(m->controle->usuario));
}
static void intervalo(const RedeIntervalo *v, void *arg) {
  Medida *m = arg;
  uint64_t taxa;
  m->bytes += v->bytes;
  if (!v->completa || v->ms < 1000) return; /* final partial interval */
  if (m->n >= STREAMFIT_AMOSTRAS_MAX || v->bytes > UINT64_MAX / 8) { m->invalida = 1; return; }
  taxa = v->bytes * 8 / v->ms; /* bits/ms == kbps; actual interval duration */
  if (taxa > 10000000) { m->invalida = 1; return; }
  m->kbps[m->n++] = (int)taxa; /* an actual zero-byte stall is evidence */
}
/* 0 = not a media body, 1 = loose (MIME absent or generic binary, body does not
 * look like text), 2 = strict (a video/ MIME). The speed test measures at 1 or
 * 2; only 2 feeds the stream-fit observation, which must be provably video.
 * Debrid CDNs often serve files as application/octet-stream (#203): the strict
 * gate alone left the Settings speed test with "sem medida" on every source. */
static int midia(const RedeResposta *r) {
  char prefixo[513];
  unsigned i = 0, n = r->n_prefixo;
  int forte;
  if (r->status != 200 && r->status != 206) return 0;
  if (r->mime[0] && (!strncasecmp(r->mime, "text/", 5) || strstr(r->mime, "mpegurl") ||
      strstr(r->mime, "json") || strstr(r->mime, "xml") || strstr(r->mime, "html") ||
      strstr(r->mime, "dash"))) return 0;
  if (!n) return 0;
  for (; i < n; i++) prefixo[i] = (char)tolower(r->prefixo[i]);
  prefixo[n] = 0;
  i = 0;
  while (i < n && isspace((unsigned char)prefixo[i])) i++;
  if (prefixo[i] == '<' || prefixo[i] == '{' || prefixo[i] == '[' ||
      !strncmp(prefixo + i, "#extm3u", 7) || strstr(prefixo, "<!doctype") || strstr(prefixo, "<html")) return 0;
  if (vazao_url_aviso(r->final)) return 0;
  forte = !strncasecmp(r->mime, "video/", 6);
  return forte ? 2 : 1;
}
typedef struct {
  const char *url; const char *const *cab; int segundos; long inicio;
  uint64_t maxBytes; const StreamfitDiagControle *ctl;
  Medida m; int status, ok;
} Extra;
static void *extraFio(void *a) {
  Extra *x = a;
  RedePedido p = {0}; RedeResposta r;
  const char *vet[66]; char faixa[64]; int nc = 0;
  x->m.controle = x->ctl;
  if (x->cab) for (; nc < 64 && x->cab[nc]; nc++) vet[nc] = x->cab[nc];
  snprintf(faixa, sizeof faixa, "Range: bytes=%ld-", x->inicio); vet[nc++] = faixa; vet[nc] = NULL;
  p.url = x->url; p.cabecalhos = vet; p.seguir = 1;
  p.prazo_ms = x->ctl->prazo_ms ? x->ctl->prazo_ms : (unsigned)x->segundos * 1000 + 8000;
  p.janela_corpo_ms = (unsigned)x->segundos * 1000; p.max_descartado = x->maxBytes;
  p.intervalo = intervalo; p.intervalo_usuario = &x->m;
  p.parar = parar; p.parar_usuario = &x->m; p.ca_arquivo = x->ctl->ca_arquivo;
  x->ok = rede_pedir(&p, &r);
  x->status = r.status;
  rede_resposta_limpar(&r);
  return NULL;
}
int streamfitdiag_medir(const char *url, const char *const *cab, int segundos,
    long inicio, uint64_t maxBytes, const StreamfitDiagControle *ctl,
    int *kbps, int nMax, RedeVazao *res, char *final, unsigned tamFinal) {
  Medida m = {0}; RedePedido p = {0}; RedeResposta r;
  const char *vet[66]; char faixa[64];
  int i, nc = 0, ok, n = 0, usada = 0, tipo;
  uint64_t extraBytes = 0;
  if (res) memset(res, 0, sizeof *res);
  if (final && tamFinal) final[0] = 0;
  if (!ctl || !ctl->rede || segundos < 1 || segundos > 60 || !maxBytes || !kbps || nMax < 1)
    return 0;
  m.controle = ctl;
  if (cab) for (; nc < 64 && cab[nc]; nc++) vet[nc] = cab[nc];
  if (cab && nc == 64 && cab[nc]) return 0;
  if (inicio > 0) { snprintf(faixa, sizeof faixa, "Range: bytes=%ld-", inicio); vet[nc++] = faixa; }
  vet[nc] = NULL;
  p.url = url; p.cabecalhos = vet; p.seguir = 1;
  p.prazo_ms = ctl->prazo_ms ? ctl->prazo_ms : (unsigned)segundos * 1000 + 8000;
  p.janela_corpo_ms = (unsigned)segundos * 1000; p.max_descartado = maxBytes;
  p.intervalo = intervalo; p.intervalo_usuario = &m;
  p.parar = parar; p.parar_usuario = &m; p.ca_arquivo = ctl->ca_arquivo;
  {
    Extra ex[STREAMFITDIAG_CONEXOES > 1 ? STREAMFITDIAG_CONEXOES - 1 : 1];
    pthread_t fio[STREAMFITDIAG_CONEXOES > 1 ? STREAMFITDIAG_CONEXOES - 1 : 1];
    int vivo[STREAMFITDIAG_CONEXOES > 1 ? STREAMFITDIAG_CONEXOES - 1 : 1];
    int e;
    for (e = 0; e < STREAMFITDIAG_CONEXOES - 1; e++) {
      memset(&ex[e], 0, sizeof ex[e]);
      ex[e].url = url; ex[e].cab = cab; ex[e].segundos = segundos;
      ex[e].inicio = inicio + (long)(e + 1) * STREAMFITDIAG_SALTO;
      ex[e].maxBytes = maxBytes; ex[e].ctl = ctl;
      vivo[e] = pthread_create(&fio[e], NULL, extraFio, &ex[e]) == 0;
    }
    ok = rede_pedir(&p, &r);
    for (e = 0; e < STREAMFITDIAG_CONEXOES - 1; e++) {
      int j;
      if (!vivo[e]) continue;
      pthread_join(fio[e], NULL);
      /* Only a real 206 adds; 200 would be the file from byte 0 again. */
      if (!ok || ex[e].status != 206 || ex[e].m.invalida) continue;
      for (j = 0; j < ex[e].m.n && j < STREAMFIT_AMOSTRAS_MAX; j++) {
        if (j < m.n) m.kbps[j] += ex[e].m.kbps[j];
        else if (j == m.n) { m.kbps[j] = ex[e].m.kbps[j]; m.n++; }
      }
      extraBytes += ex[e].m.bytes;
    }
  }
  if (final && tamFinal) snprintf(final, tamFinal, "%s", r.final);
  if (res) {
    res->status = r.status;
    res->erro = ok ? 0 : r.curl_erro ? r.curl_erro : (int)r.erro;
    res->bytes = (long long)(r.bytes_fio + extraBytes);
    res->ms = r.corpo_ms; res->esperaMs = r.primeiro_byte_ms;
    res->cancelado = r.erro == REDE_CANCELADO || r.erro == REDE_GERACAO;
  }
  tipo = ok && !r.fim_teto && !m.invalida && !parar(&m) ? midia(&r) : 0;
  if (tipo) {
    n = m.n < nMax ? m.n : nMax;
    for (i = 0; i < n; i++) kbps[i] = m.kbps[i];
    if (tipo == 2) usada = streamfit_diagnostico(ctl->rede, r.final, m.kbps, m.n, streamfit_agora_ms());
  }
  /* No URL, title, host path, network identifier or credentials in the log. */
  /* MIME type only (e.g. video/x-matroska); never the URL. */
  printf("[stream_fit] diagnostic accepted=%d intervals=%d http=%d error=%d midia=%d mime=%.40s\n",
         usada, m.n, r.status, (int)r.erro, tipo, r.mime);
  rede_resposta_limpar(&r);
  return n;
}
