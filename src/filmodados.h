#ifndef NV_FILMODADOS_H
#define NV_FILMODADOS_H
#include "filmomodelo.h"
#include "vistoep.h"
#define FD_EPS 1200
#define FD_LINHAS 12
typedef struct { long id; char nome[96],rel[96]; int ano; } FdLinha;
typedef struct {
  long pessoa;
  int n, pronto, ocupado, erro, seguindo;
  FmCredito c[FM_MAX];
  FmEstado st[FM_MAX];
  FdLinha familia[FD_LINHAS],premios[FD_LINHAS],parcerias[4];
  int nf,np,npar;
} FdFicha;
// Main-loop snapshot; all HTTP runs on the worker.
void fd_abrir(void);
void fd_atualizar(void);
const FdFicha *fd_ficha(void);
void fd_enriquecer(int idx);
int fd_preparado(int idx);
int fd_quiz(int idx,FmQuiz *q);
int fd_tentativas(int idx);
void fd_falhou(int idx);
int fd_marcar(int idx,int como,int naoLembro,int remover);
void fd_seguir(void);
void fd_sobre(void);
const char *fd_aviso(void);
#endif
