// PAUSA COM COMENTARIOS: comentarios publicos do Trakt para o painel de pausa,
// que os mostra em rodizio enquanto a pessoa esta com o video parado.
//
// Um pedido por chave (filme ou episodio) e um fio de rede por vez. Trocar de
// titulo descarta o que ja chegou; resposta de chave antiga nunca aparece. Sem
// chave do Trakt, ou sem rede, a lista fica vazia e nada e dito na tela.
#ifndef NV_PAUSACOM_H
#define NV_PAUSACOM_H
#include <stddef.h>

#define PC_MAX   8     // comentarios guardados por chave
#define PC_TEXTO 288   // 280 bytes de texto + "..." + NUL
#define PC_AUTOR 64    // username do Trakt + NUL

// episodio <= 0 => filme (temporada 0 + episodio > 0 = especial). Mesma chave repetida nao faz nada, salvo
// repetir um pedido que falhou: depois de 30 s, no maximo 3 pedidos de rede por chave (sem credencial espera
// os mesmos 30 s, mas nao conta no limite). Chave nova descarta os resultados antigos e dispara um fio de busca.
void pausacom_pedir(const char *imdb, int temporada, int episodio);

// Comentarios prontos para a chave atual; 0 enquanto carrega, falha ou nao ha.
int  pausacom_n(void);

typedef struct {
  char texto[PC_TEXTO];
  char autor[PC_AUTOR];
  int  nota;        // user_rating do Trakt 1-10, 0 = sem nota
  int  curtidas;    // likes do comentario
  char lingua[4];   // "en", "pt"... ("" = o Trakt nao disse)
} PcComentario;

// 1 quando `i` existe e preenche `*out`; 0 zera `*out`.
int  pausacom_obter(int i, PcComentario *out);

// Idioma da interface ("pt", "en"): os comentarios escritos nele vem primeiro,
// como na pagina do titulo (comentordem.h). Vale para os pedidos seguintes.
void pausacom_definir_idioma(const char *iso);

void pausacom_limpar(void);

// para teste

// Le o corpo de /comments/likes e devolve quantos comentarios valem (no maximo
// `max` e PC_MAX). Corpo malformado devolve 0.
int pausacom_interpretar(const char *json, PcComentario *out, int max);

// Relogio monotonico em ms para os 30 s entre tentativas. NULL volta ao relogio real.
void pausacom_relogio_teste(long long (*ms)(void));

#endif
