// FAROL — espelho unico "Vai junto / Nunca vai" da Tela 4 (PLANO.md §2.5 item 6,
// §3.2 tela 4, issue 22 + ressalva FAROL11-r1).
//
// Gera as linhas visiveis da coluna "Vai junto" a partir do schema real do
// redator (farolredator_schema) e da tabela base (farolpacote_espelho_*),
// explicitando tambem os grupos presentes no JSON: estado da rede, add-ons
// (contagem e tipos) e servicos ligados (debrid, Trakt, Simkl — sem a conta).
//
// Modulo puro: sem SDL, sem rede, sem dados.h, sem _Thread_local.
#ifndef NV_FAROLVAI_H
#define NV_FAROLVAI_H

#include "farolpacote.h"
#include "farolredator.h"
#include "faroltexto.h"
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// Aliases de compatibilidade (testes antigos usam FAROLVAI_*): a fonte unica
// das frases agora esta em src/faroltexto.h (FT_VAI_*).
#define FAROLVAI_REDE     FT_VAI_REDE
#define FAROLVAI_ADDONS   FT_VAI_ADDONS
#define FAROLVAI_SERVICOS FT_VAI_SERVICOS

enum { FAROLVAI_N_LINHAS = 9 };

static inline const FarolEspelhoGrupo *farolvai_grupos(size_t *n) {
  static const FarolEspelhoGrupo GRUPOS[] = {
    { "aparelho.rede",   FT_VAI_REDE       },
    { "aparelho",        FT_VAI_MODELO     },
    { "cliente.ajustes", FT_VAI_AJUSTES    },
    { "cliente.addons",  FT_VAI_ADDONS     },
    { "cliente.debrid",  FT_VAI_SERVICOS   },
    { "cliente.contas",  FT_VAI_SERVICOS   },
    { "cliente",         FT_VAI_VERSAO     },
    { "midia",           FT_VAI_FORMATO    },
    { "v",               FT_VAI_PONTO      },
    { "gatilho",         FT_VAI_PONTO      },
    { "sintoma",         FT_VAI_PONTO      },
    { "erro",            FT_VAI_PONTO      },
    { "cadeia",          FT_VAI_PONTO      },
    { "privacidade",     FT_VAI_PONTO      },
    { "linha",           FT_VAI_EVENTOS    }
  };
  if (n) *n = sizeof GRUPOS / sizeof *GRUPOS;
  return GRUPOS;
}

static inline const char *farolvai_para(const char *caminho) {
  static const FarolEspelhoGrupo ESPECIFICOS[] = {
    { "aparelho.rede",  FT_VAI_REDE     },
    { "cliente.addons", FT_VAI_ADDONS   },
    { "cliente.debrid", FT_VAI_SERVICOS },
    { "cliente.contas", FT_VAI_SERVICOS }
  };
  size_t i;
  if (!caminho || !*caminho) return NULL;
  for (i = 0; i < sizeof ESPECIFICOS / sizeof *ESPECIFICOS; i++) {
    const char *pref = ESPECIFICOS[i].prefixo;
    size_t plen = strlen(pref);
    if (strncmp(caminho, pref, plen) == 0) {
      char prox = caminho[plen];
      if (prox == '\0' || prox == '.' || prox == '[')
        return ESPECIFICOS[i].frase;
    }
  }
  return farolpacote_espelho_para(caminho);
}

static inline const char *const *farolvai_vai_junto(size_t *n) {
  static const char *const VAI[FAROLVAI_N_LINHAS] = {
    FT_VAI_MODELO,
    FT_VAI_REDE,
    FT_VAI_VERSAO,
    FT_VAI_FORMATO,
    FT_VAI_PONTO,
    FT_VAI_AJUSTES,
    FT_VAI_ADDONS,
    FT_VAI_SERVICOS,
    FT_VAI_EVENTOS
  };
  if (n) *n = FAROLVAI_N_LINHAS;
  return VAI;
}

static inline const char *const *farolvai_nunca_vai(size_t *n) {
  return farolpacote_nunca_vai(n);
}

/* Confere que 100% dos caminhos de farolredator_schema() mapeiam para uma
 * linha visivel de farolvai_vai_junto() e nenhuma linha fica orfa. */
static inline int farolvai_schema_coberto(void) {
  size_t n_sc = 0, n_vai = 0, i, k;
  const FarolCampo *sc = farolredator_schema(&n_sc);
  const char *const *vai = farolvai_vai_junto(&n_vai);
  unsigned hits[FAROLVAI_N_LINHAS] = { 0 };
  if (!sc || !n_sc || !vai || n_vai != FAROLVAI_N_LINHAS) return 0;
  for (i = 0; i < n_sc; i++) {
    const char *frase = farolvai_para(sc[i].caminho);
    int achou = 0;
    if (!frase || !frase[0]) return 0;
    if (!farolpacote_espelho_para(sc[i].caminho)) return 0;
    for (k = 0; k < n_vai; k++) {
      if (strcmp(vai[k], frase) == 0) {
        hits[k]++;
        achou = 1;
        break;
      }
    }
    if (!achou) return 0;
  }
  for (k = 0; k < n_vai; k++) {
    if (!hits[k]) return 0;
  }
  return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* NV_FAROLVAI_H */
