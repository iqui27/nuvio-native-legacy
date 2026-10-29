// Optional linkage for the existing isolated module fixtures. Every complete
// app links perfis.c; fixtures that omit account infrastructure retain their
// original behaviour. Tests of profile ownership must link the real module.
#ifndef NV_PERFISESSAO_H
#define NV_PERFISESSAO_H
#include <stdint.h>
#if defined(__APPLE__) && (defined(__GNUC__) || defined(__clang__))
// Mach-O distinguishes an optional undefined import from a weak definition.
// A complete app supplies the strong producer; isolated fixtures may omit it.
#define NV_PERFIL_OPCIONAL __attribute__((weak_import))
#elif defined(__GNUC__) || defined(__clang__)
#define NV_PERFIL_OPCIONAL __attribute__((weak))
#else
#define NV_PERFIL_OPCIONAL
#endif
#if (defined(__GNUC__) || defined(__clang__)) && !defined(NV_PERFIL_SESSAO_FORTE)
extern uint64_t perfis_sessao_geracao(void) NV_PERFIL_OPCIONAL;
extern int perfis_sessao_atual(uint64_t) NV_PERFIL_OPCIONAL;
extern int perfis_sessao_travar(uint64_t) NV_PERFIL_OPCIONAL;
extern void perfis_sessao_destravar(void) NV_PERFIL_OPCIONAL;
extern int contapend_lista_pendente(const char *) NV_PERFIL_OPCIONAL;
static inline uint64_t nv_perfil_sessao_ler(void) {
  return perfis_sessao_geracao ? perfis_sessao_geracao() : 0;
}
static inline int nv_perfil_sessao_atual(uint64_t g) {
  return perfis_sessao_atual ? perfis_sessao_atual(g) : g == 0;
}
static inline int nv_perfil_sessao_travar(uint64_t g) {
  return perfis_sessao_travar ? perfis_sessao_travar(g) : g == 0;
}
static inline void nv_perfil_sessao_destravar(void) {
  if (perfis_sessao_destravar) perfis_sessao_destravar();
}
// Mach-O: an undefined weak_import still fails a static link on this
// toolchain, so the modules that must self-satisfy in isolated fixtures set
// NV_DUBLE_* and emit weak DEFAULTS. The strong producer (perfis.c,
// contapend.c) or a fixture stub overrides them in the final link.
#ifdef NV_DUBLE_SESSAO
__attribute__((weak)) uint64_t perfis_sessao_geracao(void) { return 0; }
__attribute__((weak)) int perfis_sessao_atual(uint64_t g) { return g == 0; }
__attribute__((weak)) int perfis_sessao_travar(uint64_t g) { return g == 0; }
__attribute__((weak)) void perfis_sessao_destravar(void) {}
#endif
#ifdef NV_DUBLE_CONTAPEND
__attribute__((weak)) int contapend_estado(const char *id, const char *tipo, int t, int e, long long ms, int remoto) {
  (void)id; (void)tipo; (void)t; (void)e; (void)ms; return remoto;
}
__attribute__((weak)) int contapend_lista_pendente(const char *id) {
  (void)id; return 0;
}
#endif
#else
#include "perfis.h"
#define nv_perfil_sessao_ler perfis_sessao_geracao
#define nv_perfil_sessao_atual perfis_sessao_atual
#define nv_perfil_sessao_travar perfis_sessao_travar
#define nv_perfil_sessao_destravar perfis_sessao_destravar
#endif
typedef uint64_t NvPerfilSessao;
#endif
