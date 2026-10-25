// Marca "Perfil infantil", por perfil, nesta TV.
//
// A conta Nuvio nao tem coluna de perfil infantil (sync_pull_profiles nao a
// traz), entao a marca e LOCAL: um arquivo na pasta de dados com os
// profile_index marcados. Sai junto com a conta (perfis_esquecer -> sync.c
// chama perfil_infantil_esquecer). Quem a consome hoje e o Assistir Juntos,
// que manda `infantil:1` ao servidor; o servidor aplica as regras.
#ifndef NV_PERFILINFANTIL_H
#define NV_PERFILINFANTIL_H

// 1 se o perfil `idx` (profile_index) esta marcado como infantil. Padrao 0.
int  perfil_infantil(int idx);
// Atalho: o perfil ativo (perfis_ativo()).
int  perfil_infantil_ativo(void);
// Marca/desmarca e grava. Devolve 1 se gravou (ou ja estava assim).
int  perfil_infantil_definir(int idx, int ligado);
// Apaga todas as marcas (sair da conta).
void perfil_infantil_esquecer(void);

// 2.1: o menu Ajustes pode ser TRAVADO num perfil infantil (so abre com o PIN de
// administrador). So vale quando o perfil tambem e infantil. Arquivo proprio
// (perfil-ajustes-travados.txt), mesma regra de sair da conta.
int  perfil_ajustes_travados(int idx);
int  perfil_ajustes_travar(int idx, int travado);

#endif
