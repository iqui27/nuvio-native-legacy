#ifndef NV_CACHE_ARTE_H
#define NV_CACHE_ARTE_H

#include <stddef.h>

/* Variants are deliberately part of the key. A small backdrop is not a valid
 * answer for a hero request, even when both requests came from the same URL. */
enum NvCacheArteVariante {
  NV_CACHE_ARTE_SMALL = 1,
  NV_CACHE_ARTE_MEDIUM = 2
};

enum NvCacheArteGrupo {
  NV_CACHE_ARTE_GRUPO_GLOBAL = 0,
  NV_CACHE_ARTE_GRUPO_HOME = 1,
  NV_CACHE_ARTE_GRUPO_PERFIL = 2
};

typedef struct NvCacheArteStats {
  long itens;
  long bytes;
  /* Number of essential images currently stored in the backend. */
  long essenciais;
  /* Number of unique essential images requested by active groups. */
  long essenciais_esperados;
  long hits;
  long misses;
  long gravacoes;
  long falhas;
} NvCacheArteStats;

/* Opens the independent browser store. This is non-blocking. */
void cachearte_iniciar(void);

/* Worker-only synchronous facade over an asynchronous IndexedDB read.
 * On hit, *bytes is malloc'ed and belongs to the caller. */
int cachearte_buscar(const char *url, int variante,
                     unsigned char **bytes, long *n);

/* Copies the compressed response and queues an asynchronous write. The caller
 * may free `bytes` as soon as this function returns. */
void cachearte_salvar(const char *url, int variante,
                      const unsigned char *bytes, long n, int essencial);
/* Drops a cached body after the real image decoder rejects it. */
void cachearte_invalidar(const char *url, int variante);

/* Essential/in-use flags are metadata only; bodies are never hydrated. */
void cachearte_marcar(const char *url, int variante, int essencial, int emUso);
/* Scoped pins let independently published Home and profile mural snapshots
 * coexist. A profile/account switch should clear every group first. */
void cachearte_marcar_grupo(int grupo, const char *url, int variante,
                            int essencial, int emUso);
void cachearte_limpar_referencias_grupo(int grupo);
/* Clears only transient in-use pins before publishing a new catalogue view.
 * Essential/configured pins remain protected. */
void cachearte_limpar_uso(void);
/* Starts a fresh essential set. Call before republishing a profile/config
 * snapshot, then mark the resources that remain essential. */
void cachearte_limpar_referencias(void);

/* Refreshes the published inventory asynchronously. The next read of `out`
 * returns the most recently published values. */
void cachearte_estatisticas_pedir(void);
void cachearte_estatisticas(NvCacheArteStats *out);

/* Runtime quota for this independent store. Default is 48 MiB on Tizen. */
void cachearte_limite_bytes(long bytes);

/* Native cache keeps its existing tex_cache/cachedisco store. These hooks
 * report that store's events and let its existing pruning callback protect
 * URLs marked essential or in use. */
#ifndef __EMSCRIPTEN__
void cachearte_nativo_hit(void);
void cachearte_nativo_miss(void);
void cachearte_nativo_gravacao(int sucesso);
void cachearte_nativo_inventario(long itens, long bytes, long essenciais_presentes);
void cachearte_nativo_configurar_diretorio(const char *dir);
long cachearte_nativo_essenciais_esperados(void);
int cachearte_nativo_essencial(const char *path);
int cachearte_nativo_protegido(const char *url);
/* Indice em memoria da pasta (nome -> bytes, ultimo uso). Lido do disco uma
 * vez por sessao (construir, num fio de fundo); depois so muda por estes
 * registros. Nenhuma destas funcoes faz I/O com trava segurada. */
#include <stdint.h>
#include "cachedisco.h"
void cachearte_nativo_indice_construir(void);
int cachearte_nativo_indice_pronto(void);
void cachearte_nativo_indice_registrar(const char *path, long bytes);
void cachearte_nativo_indice_remover(const char *path);
void cachearte_nativo_indice_tocar(const char *path);
long cachearte_nativo_indice_bytes(void);
/* Mesma politica de nv_cache_podar, sem varrer a pasta. 0 se o indice ainda
 * nao esta pronto ou outra poda esta rodando. */
long cachearte_nativo_podar(long entrada, long teto, uint64_t reserva, int forcar,
                            NvCacheProtegido protegido, void *ctx);
#endif

#endif
