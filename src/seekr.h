// MINIATURAS DA BARRA DE TEMPO pelo Seekr (seekr.tv).
//
// O QUE E: uma API de "seek preview" — dado o id do IMDb (ou TMDB) e a
// DURACAO do video, devolve um WebVTT que aponta, para cada ~10 s, um recorte
// de 320x180 numa folha JPEG. E o quadro que aparece em cima da barra
// enquanto a pessoa procura um ponto no filme. Nao e fonte de video, catalogo
// nem addon Stremio: so a miniatura.
//
// A personal key entered in Settings stays on this TV, masked, and takes
// precedence over the package's default key. Removing it restores the default.
// Keys are sent only to /sprites or /v1/keys/validate; signed VTT/JPEG hops
// never receive them. Neither key values nor signed URLs belong in logs.
//
// This app additionally limits /sprites to 50 dispatches/day per installation,
// across all accounts, profiles and keys. The provider's limits are separate.
#ifndef NV_SEEKR_H
#define NV_SEEKR_H
#include "gl_compat.h"
#include "seekrquota.h"
#include <stddef.h>

enum {
  SEEKR_DESLIGADO = 0,  // sem chave, ajuste desligado ou nada pedido
  SEEKR_BUSCANDO,
  SEEKR_PRONTO,         // ha cues
  SEEKR_SEM_PREVIA,     // titulo sem preview; network failure has its own state
  SEEKR_CHAVE_RECUSADA, // 401/403: chave invalida ou revogada
  SEEKR_LIMITE_LOCAL,
  SEEKR_LIMITE_PROVEDOR,
  SEEKR_REDE_INDISPONIVEL,
  SEEKR_ARMAZENAMENTO_INDISPONIVEL,
  SEEKR_RELOGIO_INDISPONIVEL
};

typedef struct {
  int usadas, restantes, limite;
  int relogioAtrasado, persistente;
  long long reinicioUtc, retryUtc;
  int http;
  unsigned latenciaMs;
  int chavePropria; // chave pessoal: sem teto local, vale o plano do Seekr
} SeekrUso;
// Chave pessoal nao paga o orcamento local de 50 consultas (que e da chave
// padrao compartilhada do aplicativo). Chamar junto de seekr_definir_chave.
void seekr_chave_propria(int propria);
void seekr_uso(SeekrUso *uso);
// Stable UI labels (Portuguese i18n keys); no key, URL or response body.
const char *seekr_estado_rotulo(int estado);
// Format an absolute reset/retry instant using this TV's local timezone.
// Returns 0 when the instant cannot be represented; never changes the UTC ledger.
int seekr_horario_local(long long utc, char *saida, size_t tamanho);
// Explicit retry/refresh still reserves another call. No automatic request
// occurs while the provider's Retry-After is active.
void seekr_tentar_novamente(void);

// Chave lida/gravada pelos Ajustes. "" apaga.
void seekr_definir_chave(const char *chave);
int  seekr_tem_chave(void);
// Pede as miniaturas do titulo. `t`/`e` zero = filme. `durMs` e a duracao
// REAL do video (o servico escolhe a versao da folha por ela). Assincrono; a
// mesma combinacao pedida de novo nao refaz a consulta.
void seekr_pedir(const char *imdb, int t, int e, long durMs);
void seekr_desligar(void);
int  seekr_estado(void);
// FIO DE DESENHO: textura 320x180 da miniatura de `posSeg` (0 enquanto nao ha
// nenhuma) e, em *cueSeg, o instante em que aquele quadro foi tirado — e o que
// a etiqueta deve mostrar, nao a posicao crua. Pede a folha sozinho; enquanto
// a nova nao chega devolve a anterior (melhor que piscar).
GLuint seekr_quadro(double posSeg, double *cueSeg);
// A FITA (anterior, atual, seguinte), a sugestao da documentacao para deixar a
// grade de ~10 s explicita. n = 1 e o mesmo que seekr_quadro; n = 3 enche
// texs[0..2] e cueSeg[0..2] nessa ordem (0 / -1 onde nao ha). 1 quando ha a
// atual (ou a anterior a ela, enquanto a nova nao chega).
int seekr_quadros(double posSeg, int n, GLuint *texs, double *cueSeg);
// 1 quando a atual do ultimo seekr_quadros e a ULTIMA USADA, no lugar da que
// ainda esta chegando (a tela mostra que esta carregando).
int seekr_quadro_velho(void);
#ifdef NV_SHOT_HOOKS
// Capturas: `est` (SEEKR_*), os quadros anterior/atual/seguinte fixos e se a
// atual e a "ultima usada".
void seekr_shot(int est, const GLuint *t, const double *cue, int velho);
#endif
// Fim do avanco: solta a folha DECODIFICADA (3200x1800, ~22 MB medidos) e fica
// so o JPEG, para a proxima busca na mesma folha nao ir a rede.
void seekr_ocioso(void);
// SINCRONIA DA MINIATURA, em ms, somada a posicao antes da escolha. Existe
// porque a folha foi tirada de UMA versao do titulo: noutra versao (corte
// regional, abertura mais longa) a miniatura fica deslocada por igual no
// filme todo. A documentacao pede um ajuste manual e desaconselha deduzir o
// numero da diferenca de duracao — por isso nao ha nada automatico aqui.
void seekr_definir_ajuste_ms(long ms);
// BLOQUEIA: confere a chave em /v1/keys/validate. 1 valida, 0 recusada,
// -1 sem resposta. Chamar fora do fio de desenho.
int  seekr_validar(const char *chave);
#endif
