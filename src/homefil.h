// Ver homefil.c.
#ifndef NV_HOMEFIL_H
#define NV_HOMEFIL_H
#include "colecoes.h"

// --- "vistos ocultos" por fileira (#366) --------------------------------------
#define OV_MAX 128
int      ov_ativa(const char *chave);
int      ov_alternar(const char *chave);      // devolve o estado NOVO
unsigned ov_revisao(void);
void     ov_definir_perfil(int perfil);
void     ov_esquecer(void);

// --- colecao fixada: vira fileira propria -------------------------------------
// Chave da fileira de uma pasta fixada: "colpin_<id da pasta>".
#define CF_MAX 16
#define CF_PREFIXO "colpin_"
int          cf_fixada(const char *pastaId);
int          cf_alternar(const ColFolder *pasta);   // devolve o estado NOVO
unsigned     cf_revisao(void);
int          cf_n(void);
const ColFolder *cf_pasta(int i);   // NULL se a conta nao a trouxe mais
const char  *cf_titulo(int i);
// Titulos das janelas locais, na ordem das fontes e dos catalogos, sem
// duplicatas. 0 se alguma fonte nao esta disponivel ou exige consulta propria:
// a Home conserva o atalho para "Ver tudo", sem buscar dados novos.
int          cf_itens(const ColFolder *pasta, int *indicesCat, int max);
void         cf_definir_perfil(int perfil);
void         cf_esquecer(void);

// --- ocultar / mover uma fileira pela chave (a mesma escolha de Ajustes) -----
int homefil_indice(const char *chave);          // -1 = fora do registro
int homefil_ocultar(const char *chave);         // 1 = ocultou
int homefil_mover(const char *chave, int dir);  // 1 = mudou de lugar

// --- de onde vem a fileira ------------------------------------------------------
// Origem ao lado do titulo de "Continuar assistindo" / "Proximos episodios"
// (home.html, `.org`), em PARTES que quem desenha junta com " · ": "Trakt",
// "Simkl" (nomes proprios) e a chave "Este aparelho" (passa por i18n). `trakt`
// e `simkl` = vinculo ativo nesta TV. O progresso do aparelho sempre existe;
// os proximos episodios so existem com algum rastreador ligado. Devolve quantas
// partes pos em `partes` (precisa de 3 posicoes); 0 = fileira sem origem.
int homefil_origem(const char *chave, int trakt, int simkl, const char *partes[3]);

#endif
