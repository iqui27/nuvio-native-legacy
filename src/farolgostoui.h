// FAROLGOSTOUI — folhas de gosto do Farol (issue 51):
//   1. "O que o Farol aprendeu" (top 5 generos, pessoas e temas por extenso)
//   2. Confirmacao de "Zerar o gosto" ("Apagar o que o Farol aprendeu nesta TV?")
//   3. Controle LLM / "So nesta TV" com gancho __attribute__((weak)) para #53.
//
// Sem armazenamento por thread; zero log com titulo, perfil ou gosto.
#ifndef NV_FAROLGOSTOUI_H
#define NV_FAROLGOSTOUI_H

#include <SDL2/SDL.h>

// Top 5 por extenso (espelho de dados da folha "O que o Farol aprendeu").
#define FGUI_TOP_N 5
typedef struct {
  int  nGeneros, nPessoas, nTemas;
  char generos[FGUI_TOP_N][64];
  char pessoas[FGUI_TOP_N][64];
  char temas[FGUI_TOP_N][64];
  int  vazio;
} FarolGostoResumo;

// Preenche o resumo a partir do modelo ativo (e catalogo/cache de nomes).
void farolgostoui_resumo(FarolGostoResumo *dst);

// Abre a folha "O que o Farol aprendeu".
void farolgostoui_abrir_aprendeu(void);
// Abre o modal de confirmacao "Zerar o gosto" (nao abre se trava parental ativa).
void farolgostoui_abrir_zerar(void);
void farolgostoui_fechar(void);
void farolgostoui_abrir_sessao(void);

int  farolgostoui_aberto(void);
int  farolgostoui_modo(void);   // 0 fechada, 1 aprendeu, 2 confirmar zerar
int  farolgostoui_foco(void);
void farolgostoui_evento(const SDL_Event *e);
void farolgostoui_atualizar(float dt, Uint32 agora);
void farolgostoui_desenhar(Uint32 agora);

// Consentimento do LLM para gosto / "So nesta TV":
// 0 = So nesta TV (padrao seguro, LLM desligado); 1 = consentimento ativo.
// Bloqueado em perfil infantil e sob trava parental.
int  farolgostoui_llm_ativo(void);
int  farolgostoui_llm_definir(int ligado);

#endif
