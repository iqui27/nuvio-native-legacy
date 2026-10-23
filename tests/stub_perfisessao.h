#ifndef NV_TEST_STUB_PERFISESSAO_H
#define NV_TEST_STUB_PERFISESSAO_H
#include "perfis.h"
#define NV_PERFIL_SESSAO_FORTE

// Isolated fixtures keep one profile session throughout each process.
uint64_t perfis_sessao_geracao(void) { return 0; }
int perfis_sessao_atual(uint64_t geracao) { return geracao == 0; }
int perfis_sessao_travar(uint64_t geracao) { return perfis_sessao_atual(geracao); }
void perfis_sessao_destravar(void) {}
#endif
