// SEND THE LOG TO THE CLOUD: when the TV's own diagnostics address cannot be
// opened (no local address, another network, a phone that cannot reach the
// TV), the same masked snapshot that the local page serves is gzipped and
// uploaded to OUR diagnostics worker (servidor/recomendacoes,
// POST /v1/loglink, stored with a 72 h TTL). The card shows the link as a QR
// code and as text, and the person sends that link to whoever asked.
// (Adapted from Matis94, which posted to an external temporary file host.)
//
// Only on an explicit OK/RIGHT in the "Download log" card; never automatic.
// The snapshot is the logexport one: credentials, URLs and personal
// identifiers are already masked.
#ifndef NV_LOGCLOUD_H
#define NV_LOGCLOUD_H

enum { LOGCLOUD_PARADO = 0, LOGCLOUD_ENVIANDO, LOGCLOUD_PRONTO, LOGCLOUD_FALHOU };

/* Starts one upload on a thread (a running one is left alone). 1 = started. */
int logcloud_enviar(void);
int logcloud_estado(void);
/* Copies, valid once PRONTO / FALHOU. */
void logcloud_url(char *dst, int n);
void logcloud_motivo(char *dst, int n);
/* The card closed: forget the result (a running upload finishes quietly). */
void logcloud_esquecer(void);

/* Pure helpers (tests): the request body (identity of the gzipped snapshot)
 * and the worker's {"ok":1,"url":"..."} JSON answer. */
char *logcloud_corpo(const unsigned char *gz, unsigned long n, const char *fronteira, unsigned long *tam);
int logcloud_resposta_url(const char *corpo, char *dst, int n);
#endif
