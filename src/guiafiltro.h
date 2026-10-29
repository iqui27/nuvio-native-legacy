// FILTRO DA LIVE TV: qualidade, categorias e canais escondidos.
//
// Qualidade: Todas as versoes (padrao), So 4K, ou Preferir 4K (um por canal:
// a 4K se houver, senao a maior resolucao). "O mesmo canal" e o nome-base de
// livetv_regras.h dentro da mesma fonte, inclusive entre grupos. Categoria e canal
// escondidos ficam por perfil, como os add-ons escondidos (guiaaddons.h).
//
// Channel visibility uses source keys plus exact IDs in every quality mode; names and categories
// stored with hidden IDs are panel metadata, never sibling visibility rules.
//
// Modulo sem rede nem tela: o guia chama guiafiltro_avaliar ANTES do teto de
// canais, entao o que sai por aqui nao gasta vaga.
#ifndef NV_GUIAFILTRO_H
#define NV_GUIAFILTRO_H
#include <stddef.h>
#include <stdint.h>

enum { GF_TODAS = 0, GF_SO_4K, GF_PREFERIR_4K, GF_MODOS };

#define GF_NOME 256
#define GF_ID 256
#define GF_FONTE 24
#define GF_CATS_MAX 256
#define GF_CANAIS_MAX 512

typedef struct {
  int modo;
  int nCat;  char cat[GF_CATS_MAX][GF_NOME];
  int nCan;  struct { char id[GF_ID], nome[GF_NOME], cat[GF_NOME], fonte[GF_FONTE]; int visivel; } can[GF_CANAIS_MAX];
} GuiaFiltro;

// Lines: Q<mode>, C<category>, S<source>\t<id>\t<hidden>\t<name>\t<category>.
// Legacy H<id>\t<name>\t<category> migrates to wildcard source "*" on save.
// Linha vazia, repetida, desconhecida ou alem do teto e ignorada.
void   guiafiltro_ler(GuiaFiltro *f, const char *texto);
size_t guiafiltro_texto(const GuiaFiltro *f, char *dst, size_t n);
void   guiafiltro_modo_definir(GuiaFiltro *f, int modo);
int    guiafiltro_cat_oculta(const GuiaFiltro *f, const char *cat);
// Invertem o estado; devolvem 1 se agora esta ESCONDIDO. Lista cheia: nao esconde.
int    guiafiltro_cat_alternar(GuiaFiltro *f, const char *cat);
int    guiafiltro_canal_oculto(const GuiaFiltro *f, const char *id);
int    guiafiltro_canal_alternar(GuiaFiltro *f, const char *id, const char *nome, const char *cat);
// Source-qualified rules override legacy bare IDs; explicit show survives migration.
int    guiafiltro_canal_oculto_fonte(const GuiaFiltro *f, const char *fonte, const char *id);
int    guiafiltro_canal_alternar_fonte(GuiaFiltro *f, const char *fonte, const char *id, const char *nome, const char *cat);
// "guia-filtro-p<perfil>.txt" — por perfil, como o cache do guia.
const char *guiafiltro_arquivo(int perfil, char *dst, size_t n);

// --- admissao -----------------------------------------------------------------
// Estado de uma carga: onde ficou o canal escolhido de cada "mesmo canal".
typedef struct { uint64_t chave; int idx, res; } GuiaFiltroSlot;
typedef struct { GuiaFiltroSlot *t; int cap, n; } GuiaFiltroDup;   // comeca zerado

enum {
  GF_ENTRA,      // entra como canal novo (depois chame guiafiltro_dup_registrar)
  GF_FORA,       // escondido, ou nao e 4K no modo So 4K
  GF_FORA_DUP,   // outra versao do mesmo canal ja ficou melhor (*idxVelho = quem ficou)
  GF_TROCA       // melhor que a que ficou: ocupa a vaga *idxVelho (sem gastar outra)
};
int  guiafiltro_avaliar(const GuiaFiltro *f, GuiaFiltroDup *d, int fonte, const char *grupo,
                        const char *id, const char *nome, const char *origem, int *idxVelho);
void guiafiltro_dup_registrar(const GuiaFiltro *f, GuiaFiltroDup *d, int fonte, const char *grupo,
                              const char *nome, const char *origem, int idx);
void guiafiltro_dup_zerar(GuiaFiltroDup *d);   // libera e zera

#endif
