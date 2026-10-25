#ifndef NV_FONTEFILTRO_H
#define NV_FONTEFILTRO_H

#include <stdint.h>

#define FONTEFILTRO_MAX_GB 1000

enum {
  FONTEFILTRO_RESOLUCAO, FONTEFILTRO_QUALIDADE, FONTEFILTRO_VISUAL,
  FONTEFILTRO_AUDIO, FONTEFILTRO_CANAIS, FONTEFILTRO_ENCODE,
  FONTEFILTRO_LINGUAS, FONTEFILTRO_N
};

enum {
  FONTEFILTRO_CACHE_TODAS,
  FONTEFILTRO_CACHE_SIM,
  FONTEFILTRO_CACHE_NAO,
  FONTEFILTRO_CACHE_N
};

typedef struct {
  uint64_t excluir[FONTEFILTRO_N];
  unsigned maxGB;                 /* GB decimais; zero = sem limite. */
  unsigned cache;                 /* Todas, somente em cache ou somente fora. */
  char grupos[512];               /* Grupos excluidos, separados por virgula. */
} FonteFiltroConfig;

typedef struct {
  uint64_t valores[FONTEFILTRO_N]; /* Zero = desconhecido; nao soma ao limite. */
  uint64_t tamanhoBytes;          /* Zero = desconhecido. */
  unsigned cache;                 /* Zero = status nao declarado/confirmado. */
  char grupo[80];
  int naoVod;                     /* Request scope; travels with this source. */
} FonteFiltroDados;

typedef struct {
  const char *resolucao, *qualidade, *visual, *audio, *canais, *encode;
  const char *linguas, *grupo, *filename, *texto;
  uint64_t tamanhoBytes;
} FonteFiltroEntrada;

/* IDs de escolha sao persistentes. A ultima escolha de cada categoria e
   Desconhecido. Rotulos e nomes tecnicos nao dependem dos badges de tela. */
int fontefiltro_opcoes_n(int categoria);
const char *fontefiltro_opcao(int categoria, int opcao);
const char *fontefiltro_rotulo(int categoria);
uint64_t fontefiltro_bit(int categoria, int opcao);
uint64_t fontefiltro_desconhecido(int categoria);
uint64_t fontefiltro_valores(int categoria, const char *texto);

int fontefiltro_config_validar(const FonteFiltroConfig *cfg);
/* Converte os sinais de cache do Stream no estado persistente. A confirmacao
   nativa prevalece sobre um marcador antigo fora de cache; P2P e avisos nao
   contam como links confirmados. */
unsigned fontefiltro_cache_estado(int emCache, int cacheNegado, int foraCache,
                                  int cacheNativo, int urlP2P, int naoVideo);
/* Valida e normaliza CSV, preservando saida original em caso de erro. */
int fontefiltro_grupos_normalizar(const char *csv, char *saida, unsigned n);
void fontefiltro_classificar(const FonteFiltroEntrada *entrada, FonteFiltroDados *saida);
/* Linguas: exclui apenas se TODAS as linguas declaradas forem excluidas.
   Demais tags: uma correspondencia basta. Desconhecido e opcao explicita. */
int fontefiltro_permite(const FonteFiltroConfig *cfg, const FonteFiltroDados *dados);
/* A renovacao pertence a uma fonte que ja venceu o filtro. O resolvedor pode
   confirmar outro estado de cache depois da escolha; essa transicao nao deve
   invalidar a propria vencedora, mas os demais metadados atuais continuam
   sujeitos aos filtros. */
int fontefiltro_renovacao_permite(const FonteFiltroConfig *cfg,
                                  const FonteFiltroDados *selecionado,
                                  const FonteFiltroDados *atual);

#endif
