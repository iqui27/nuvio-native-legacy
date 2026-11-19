#ifndef NV_NVBACKEND_H
#define NV_NVBACKEND_H

/* URL do servidor Nuvio (recomendações, social): compile-time NV_REC_URL ou Ajustes. */
const char *nv_rec_url(void);
int nvbackend_definir_url(const char *u);
/* GET origin/.well-known/nuvio -> backend_url; grava se válido. */
int nvbackend_descobrir(const char *origin);

#endif
