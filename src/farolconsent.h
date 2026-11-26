// FAROLCONSENT — consentimento explicito por perfil para o compositor LLM
// (PLANO-GOSTO.md §2.4, §5.3, §6 issue 53, §7 decisao 2).
//
// Modulo puro: sem SDL, sem rede, sem _Thread_local.
//
// Tres estados por perfil (mesmo padrao de REC_APARECER_* em src/recomenda.h):
//   - FAROL_CONSENT_NAO_PERGUNTADO (0): padrao seguro, LLM desligado.
//   - FAROL_CONSENT_SO_TV          (1): "So nesta TV", LLM desligado, nao
//                                       volta a perguntar.
//   - FAROL_CONSENT_SIM            (2): LLM ligado para o perfil, exceto se
//                                       perfil_infantil_ativo().
//
// Fonte unica da verdade ("lista da tela == lista do corpo"):
//   farolconsent_espelho_grupos(), farolconsent_vai_junto(),
//   farolconsent_nunca_vai() e farolconsent_campo_autorizado() compartilham a
//   mesma tabela fechada com src/farolcompor.c.
#ifndef NV_FAROLCONSENT_H
#define NV_FAROLCONSENT_H

#include "faroltexto.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Aliases de compatibilidade (testes antigos usam FAROLCONSENT_*): a fonte
// unica das frases esta em src/faroltexto.h (FT_GOSTO_*).
#define FAROLCONSENT_KICKER          FT_GOSTO_CONSENT_KICKER
#define FAROLCONSENT_TITULO          FT_GOSTO_CONSENT_TITULO
#define FAROLCONSENT_SUB             FT_GOSTO_CONSENT_SUB
#define FAROLCONSENT_BTN_PERMITIR    FT_GOSTO_CONSENT_PERMITIR
#define FAROLCONSENT_NOTA_SO_TV      FT_GOSTO_CONSENT_NOTA_SO_TV
#define FAROLCONSENT_RODAPE          FT_GOSTO_CONSENT_RODAPE

#define FAROLCONSENT_VAI_IDIOMA      FT_GOSTO_VAI_IDIOMA
#define FAROLCONSENT_VAI_HORA        FT_GOSTO_VAI_HORA
#define FAROLCONSENT_VAI_GOSTO       FT_GOSTO_VAI_GOSTO
#define FAROLCONSENT_VAI_CANDIDATOS  FT_GOSTO_VAI_CANDIDATOS
#define FAROLCONSENT_VAI_ANCORAS     FT_GOSTO_VAI_ANCORAS
#define FAROLCONSENT_VAI_FRASE       FT_GOSTO_VAI_FRASE

#define FAROLCONSENT_NUNCA_DIARIO    FT_GOSTO_NUNCA_DIARIO
#define FAROLCONSENT_NUNCA_NOTAS     FT_GOSTO_NUNCA_NOTAS
#define FAROLCONSENT_NUNCA_PERFIS    FT_GOSTO_NUNCA_PERFIS
#define FAROLCONSENT_NUNCA_AMIGOS    FT_GOSTO_NUNCA_AMIGOS
#define FAROLCONSENT_NUNCA_ADDONS    FT_GOSTO_NUNCA_ADDONS
#define FAROLCONSENT_NUNCA_VETOR     FT_GOSTO_NUNCA_VETOR
#define FAROLCONSENT_NUNCA_INFANTIL  FT_GOSTO_NUNCA_INFANTIL

enum {
  FAROL_CONSENT_NAO_PERGUNTADO = 0,
  FAROL_CONSENT_SO_TV = 1,
  FAROL_CONSENT_SIM = 2
};

enum {
  FAROLCONSENT_VAI_N = 6,
  FAROLCONSENT_NUNCA_N = 7
};

typedef struct {
  const char *prefixo;
  const char *frase;
} FarolConsentGrupo;

int  farolconsent_presente(void);
int  farolconsent_estado(void);
int  farolconsent_estado_perfil(int perfil);
int  farolconsent_llm_ativo(void);
int  farolconsent_deve_perguntar(void);
int  farolconsent_definir(int estado);
int  farolconsent_definir_perfil(int perfil, int estado);
void farolconsent_trocar_perfil(int perfil);
void farolconsent_esquecer(void);

// Sincronizacao interna usada por farolcompor.c ao carregar cache/trocar perfil.
void farolconsent_sincronizar_ativo(void);
void farolconsent_invalidar(void);

// Fonte unica da verdade ("Vai junto / Nunca vai" == campos de farolcompor.c).
const FarolConsentGrupo *farolconsent_espelho_grupos(size_t *n);
const char *const       *farolconsent_vai_junto(size_t *n);
const char *const       *farolconsent_nunca_vai(size_t *n);
const char              *farolconsent_espelho_para(const char *caminho);
int                      farolconsent_campo_autorizado(const char *caminho);

// Ganchos de teste (NULL volta ao padrao de dados_dir()).
void farolconsent_pasta(const char *dir);
void farolconsent_teste_zera(void);

#ifdef __cplusplus
}
#endif

#endif // NV_FAROLCONSENT_H
