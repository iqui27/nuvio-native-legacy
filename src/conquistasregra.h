// Regra das Conquistas (2.1): niveis, estado gravado por perfil e fila de avisos.
// Mockup: design/2.1/conquistas.html; catalogo e perguntas em NOTAS-CONQUISTAS.md.
//
// SEPARADA DA TELA de proposito: nada aqui depende de SDL, rede ou desenho, entao
// o teste (tests/conquistas.sh) linka so este arquivo.
//
// O QUE ESTA AQUI: a conta dos cinco niveis de uma trilha, o formato do arquivo
// por perfil (conquistas-p<N>.txt), o registro de progresso (so sobe, nunca
// desce) e a fila do aviso "depois dos creditos". O QUE NAO ESTA: de onde vem
// `vistos` e `total`. O app nao guarda ano, pais nem diretor de um titulo; quem
// souber (trilhas, listas, Caca) chama cqregra_registrar com os numeros.
// Ninguem chama hoje fora dos testes, e a tela mostra o vazio honesto.
//
// Premia descoberta, nunca tempo de tela: nao ha contagem de horas, sequencia
// nem prazo (NOTAS-CONQUISTAS.md, principios 1 e 2).
#ifndef NV_CONQUISTASREGRA_H
#define NV_CONQUISTASREGRA_H
#include <stddef.h>

enum {
  CQ_FAM_TRILHA = 0, CQ_FAM_DIRETOR, CQ_FAM_EXPLORADOR, CQ_FAM_PREMIO,
  CQ_FAM_COMUNIDADE, CQ_FAM_TEMPORADA, CQ_FAM_N
};

#define CQ_NIVEIS 5          // 1 primeiro passo .. 5 em ordem
#define CQ_CHAVE_MAX 85      // "trilha:" + 80, como o regex do servidor
#define CQ_NOME_MAX 60
#define CQ_MAX 128

typedef struct {
  char chave[CQ_CHAVE_MAX + 1];   // "dir:...", "trilha:..." (cqregra_chave_valida)
  char nome[CQ_NOME_MAX + 1];
  int  familia;                   // CQ_FAM_*
  int  nivel;                     // 0..CQ_NIVEIS; so sobe
  int  avisado;                   // maior nivel ja mostrado no aviso; < nivel = pendente
  int  vistos, total;
  long ganho;                     // time_t do ultimo nivel; 0 = nunca
} CqEntrada;

typedef struct {
  CqEntrada e[CQ_MAX];
  int n;
} CqEstado;

// Mesmo regex do servidor: ^(dir|ator|saga|top|trakt|trilha):[A-Za-z0-9._/-]{1,80}$
int  cqregra_chave_valida(const char *chave);

// Nivel de uma trilha: 1 = ao menos um filme, 2 = metade, 3 = 75%, 4 = todos,
// 5 = todos na ordem do curador (`emOrdem`). `total` <= 0 nao da nivel.
int  cqregra_nivel(int vistos, int total, int emOrdem);
// Top 250 do Trakt: 25, 50, 100 e 250 filmes viram os niveis 1 a 4.
int  cqregra_nivel_top(int vistos);

// Texto de conquistas-p<N>.txt -> estado. NULL ou lixo caem no vazio; linhas
// ruins sao ignoradas uma a uma. Formato por linha:
//   chave|familia|nivel|avisado|vistos|total|ganho|nome
int  cqregra_ler(const char *txt, CqEstado *s);
// Estado -> texto. Devolve o tamanho, ou 0 se nao coube.
int  cqregra_serializar(const CqEstado *s, char *dst, size_t tam);

// Anota o progresso de uma chave. O nivel so sobe; vistos e total acompanham.
// Devolve o novo nivel quando subiu (o aviso fica pendente), senao 0. -1 se a
// chave e invalida ou o estado esta cheio.
int  cqregra_registrar(CqEstado *s, const char *chave, int familia, const char *nome,
                       int nivel, int vistos, int total, long agora);
const CqEntrada *cqregra_buscar(const CqEstado *s, const char *chave);

// Fila do aviso depois dos creditos. Pendente = nivel > avisado.
int  cqregra_pendentes(const CqEstado *s);
// Indice da proxima entrada pendente (a mais recente primeiro), ou -1.
int  cqregra_proximo_aviso(const CqEstado *s);
// Marca tudo como mostrado (o aviso agrupa varias conquistas de uma vez).
void cqregra_marcar_avisados(CqEstado *s);

// Raridade pela parte de 1000 de pessoas que tem o emblema:
// 0 Comum (>30%), 1 Incomum (10-30%), 2 Rara (1-10%), 3 Raríssima (<1%).
int  cqregra_faixa(int permil);

#endif
