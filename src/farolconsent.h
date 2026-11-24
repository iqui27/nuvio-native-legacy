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

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// FALTA_TEXTO: incluir em src/faroltexto.h e nas 29 tabelas src/idioma_*.h:
//   FT_GOSTO_CONSENT_KICKER     = "Gosto"
//   FT_GOSTO_CONSENT_TITULO     = "Recomendações com o servidor?"
//   FT_GOSTO_CONSENT_SUB        = "Você escolhe se o Farol pode montar fileiras e dicas usando o servidor."
//   FT_GOSTO_CONSENT_PERMITIR   = "Permitir"
//   FT_GOSTO_CONSENT_NOTA_SO_TV = "\"Só nesta TV\" mantém tudo calculado só no aparelho e não pergunta de novo."
//   FT_GOSTO_CONSENT_RODAPE     = "Você pode mudar isso depois em Ajustes › Gosto."
//   FT_GOSTO_VAI_IDIOMA         = "Idioma da interface"
//   FT_GOSTO_VAI_HORA           = "Hora local em faixa"
//   FT_GOSTO_VAI_GOSTO          = "Resumo do gosto: até 6 gêneros, 6 temas e 4 pessoas"
//   FT_GOSTO_VAI_CANDIDATOS     = "Até 24 candidatos"
//   FT_GOSTO_VAI_ANCORAS        = "Até 3 títulos-âncora"
//   FT_GOSTO_VAI_FRASE          = "Frase ditada quando houver"
//   FT_GOSTO_NUNCA_DIARIO       = "O Diário"
//   FT_GOSTO_NUNCA_NOTAS        = "Notas e datas"
//   FT_GOSTO_NUNCA_PERFIS       = "Perfis e nomes"
//   FT_GOSTO_NUNCA_AMIGOS       = "Amigos (motivo de amigo vai como \"um amigo\")"
//   FT_GOSTO_NUNCA_ADDONS       = "Add-ons e fontes"
//   FT_GOSTO_NUNCA_VETOR        = "O vetor inteiro"
//   FT_GOSTO_NUNCA_INFANTIL     = "Qualquer coisa de perfil infantil"
#define FAROLCONSENT_KICKER          "Gosto"
#define FAROLCONSENT_TITULO          "Recomendações com o servidor?"
#define FAROLCONSENT_SUB             "Você escolhe se o Farol pode montar fileiras e dicas usando o servidor."
#define FAROLCONSENT_BTN_PERMITIR    "Permitir"
#define FAROLCONSENT_NOTA_SO_TV      "\"Só nesta TV\" mantém tudo calculado só no aparelho e não pergunta de novo."
#define FAROLCONSENT_RODAPE          "Você pode mudar isso depois em Ajustes › Gosto."

#define FAROLCONSENT_VAI_IDIOMA      "Idioma da interface"
#define FAROLCONSENT_VAI_HORA        "Hora local em faixa"
#define FAROLCONSENT_VAI_GOSTO       "Resumo do gosto: até 6 gêneros, 6 temas e 4 pessoas"
#define FAROLCONSENT_VAI_CANDIDATOS  "Até 24 candidatos"
#define FAROLCONSENT_VAI_ANCORAS     "Até 3 títulos-âncora"
#define FAROLCONSENT_VAI_FRASE       "Frase ditada quando houver"

#define FAROLCONSENT_NUNCA_DIARIO    "O Diário"
#define FAROLCONSENT_NUNCA_NOTAS     "Notas e datas"
#define FAROLCONSENT_NUNCA_PERFIS    "Perfis e nomes"
#define FAROLCONSENT_NUNCA_AMIGOS    "Amigos (motivo de amigo vai como \"um amigo\")"
#define FAROLCONSENT_NUNCA_ADDONS    "Add-ons e fontes"
#define FAROLCONSENT_NUNCA_VETOR     "O vetor inteiro"
#define FAROLCONSENT_NUNCA_INFANTIL  "Qualquer coisa de perfil infantil"

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
