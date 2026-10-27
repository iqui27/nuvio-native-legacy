#ifndef NV_PERFILREGRAS_H
#define NV_PERFILREGRAS_H
/* Local parental policy. Values are per profile; no credentials are stored. */
enum { PR_TEMPO, PR_IDADE, PR_BUSCA_LIVRE, PR_COMPRAS, PR_N };
int perfil_regra(int profile,int rule);
int perfil_regra_definir(int profile,int rule,int value);
int perfil_regra_classif(const char *certificate);
int perfil_regra_busca(const char *certificate);
int perfil_regra_compras(void);
/* Accumulate visible app use against the local calendar day, returns 1 at limit. */
int perfil_regra_tempo(float seconds);
void perfil_regras_esquecer(void);
#endif
