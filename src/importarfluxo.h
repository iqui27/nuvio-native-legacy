// Onboarding import flows: Stremio (auth link) and AIOManager (link typed on the phone).
// Workers fill static buffers; importarfluxo_passo() (main thread, once per frame) applies
// them, so addons/salvos are never touched off-thread. Never exposes a token or addon URL.
#ifndef NV_IMPORTARFLUXO_H
#define NV_IMPORTARFLUXO_H

typedef enum { IMPF_PARADO = 0, IMPF_ESPERA, IMPF_TRABALHA, IMPF_OK, IMPF_ERRO } ImpfEstado;
enum { IMPF_NENHUM = 0, IMPF_STREMIO, IMPF_AIOM };

void importarfluxo_comecar(int fonte);
void importarfluxo_passo(void);
void importarfluxo_cancelar(void);
ImpfEstado  importarfluxo_estado(void);
int         importarfluxo_fonte(void);
const char *importarfluxo_codigo(void);   // Stremio: code to type at the link page
const char *importarfluxo_link(void);     // Stremio: page that takes the code (also the QR)
const char *importarfluxo_host(void);     // AIOManager: host only
int         importarfluxo_addons(void);   // imported counts
int         importarfluxo_titulos(void);

#ifdef AJUSTES_TESTE
void importarfluxo_teste(int fonte, ImpfEstado e, const char *codigo, const char *link, int nAddons, int nTitulos);
#endif

#endif
