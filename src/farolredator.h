#ifndef FAROLREDATOR_H
#define FAROLREDATOR_H
#include <stddef.h>
#include <stdint.h>

#define FAROL_JSON_N 40000
#define FAROL_JSON_MAX 35000
#define FAROL_JSON_PROF 8
#define FAROL_REDATOR_TOKEN_N 40

typedef enum { FT_INT, FT_NUM1, FT_BOOL, FT_TOKEN, FT_ENUM, FT_VERSAO, FT_HASH,
               FT_OBJETO, FT_ARRAY } FarolTipo;
typedef struct {
  const char *caminho;
  FarolTipo tipo;
  const char *valores;
} FarolCampo;
typedef struct {
  char caminho[96];
  uint64_t vistos[4];
  unsigned itens;
  int array;
} FarolJsonNivel;
typedef struct FarolJson {
  char *buf;
  size_t tam, n;
  unsigned campos_recusados;
  int prof, fechado;
  FarolJsonNivel nivel[FAROL_JSON_PROF];
} FarolJson;

int farolredator_abrir(FarolJson *j, char *buf, size_t tam);
int farolredator_campo(FarolJson *j, const char *chave, FarolTipo t, const void *valor);
int farolredator_objeto(FarolJson *j, const char *chave);
int farolredator_array(FarolJson *j, const char *chave);
int farolredator_fechar(FarolJson *j);
void farolredator_token(const char *s, char out[FAROL_REDATOR_TOKEN_N]);
void farolredator_hash_host(const char *url_ou_host, const unsigned char sal[16], char out[12]);
int farolredator_host_publico(const char *host);
const FarolCampo *farolredator_schema(size_t *n);
#endif
