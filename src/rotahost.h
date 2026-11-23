#ifndef NV_ROTAHOST_H
#define NV_ROTAHOST_H

#define ROTAHOST_MAX 64
#define ROTAHOST_TENTATIVAS 3
#define ROTAHOST_TETO_CANAL 8

typedef struct { int modo, rota; } RotaTentativa;
typedef struct {
  const char *url;
  int podeProxy, proxyForcado, proxyPadrao;
  int modoForcado, modoPadrao, variaModo;
} RotaPlanoEntrada;

void rotahost_definir_arquivo(char *(*ler)(const char *),
                             int (*gravar)(const char *, const char *),
                             int (*apagar)(const char *));
void rotahost_funcionou(const char *url, int rota, int modo);
void rotahost_falhou(const char *url, int rota);
int rotahost_rota(const char *url);
int rotahost_modo(const char *url);
void rotahost_limpar(void);
int rotahost_plano(const RotaPlanoEntrada *e, RotaTentativa *saida, int max);

#endif
