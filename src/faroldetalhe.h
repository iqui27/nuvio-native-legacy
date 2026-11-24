// FAROLDETALHE — bloco "Por que voce vai gostar" (P3) + informacao relevante
// (P6) na pagina do titulo (lote sense, issue 46; PLANO-GOSTO secoes 4.2-4.5).
//
// Consome farolcena (44), farolinfo (47), farolpeca (39), farolrec (41) e
// faroltexto (35) sem altera-los. Quando nao ha motivo nem informacao,
// faroldetalhe_altura devolve exatamente 0.0f e nada e desenhado — o layout
// da pagina fica identico ao de hoje, pixel a pixel.
#ifndef NV_FAROLDETALHE_H
#define NV_FAROLDETALHE_H

#include <SDL2/SDL.h>
#include <stddef.h>
#include <stdint.h>
#include "farolficha.h"
#include "farolgosto.h"
#include "farolinfo.h"
#include "ponteiro.h"

#define FD_PRAZO_QUALIDADE_MS 8000u
#define FD_FADE_MS            220.0f

enum {
  FD_ACAO_NENHUMA = 0,
  FD_ACAO_PESSOA,
  FD_ACAO_EXPLORAR
};

void  faroldetalhe_abrir(int catIndice, Uint32 agoraMs);
void  faroldetalhe_fechar(void);
void  faroldetalhe_atualizar(int catIndice, int epTemp, int epNum,
                             float dt, Uint32 agoraMs);

float faroldetalhe_altura(float larg);
void  faroldetalhe_desenhar(float x, float y, float larg, float a,
                            PonteiroFn ptrFn, int ptrRow);

int   faroldetalhe_n_selos(void);
int   faroldetalhe_focado(void);
int   faroldetalhe_selo_atual(void);
int   faroldetalhe_focar(int entrar);
void  faroldetalhe_ponteiro(int col);
int   faroldetalhe_tecla(SDL_Keycode k, int *pediuMenu);
// seloTipo/seloFio: tipo FP_SELO_* e fio cru (kw ou genero local) do selo
// ativado quando a acao e FD_ACAO_EXPLORAR (-1/"" em pessoa ou nenhuma).
// FD_TEM_SELO_FIO marca a assinatura nova (FAROL46B) para quem compila contra
// a revisao anterior (testes dirigidos base-vs-ponta).
#define FD_TEM_SELO_FIO 1
int   faroldetalhe_ativar(long *tmdb, int *elencoCol,
                          char *nome, size_t tamNome,
                          char *foto, size_t tamFoto,
                          int *seloTipo, char *seloFio, size_t tamFio);
// Variante que devolve tambem o id do selo (id TMDB > 0 ou hash local < 0).
int   faroldetalhe_ativar_id(long *tmdb, int *elencoCol,
                             char *nome, size_t tamNome,
                             char *foto, size_t tamFoto,
                             int *seloTipo, char *seloFio, size_t tamFio,
                             int32_t *seloId);

// --- inspecao e ganchos de teste deterministico (sem rede) -------------------
int   faroldetalhe_tem_porque(void);
int   faroldetalhe_tem_info(void);
int   faroldetalhe_tem_qualidade(void);
float faroldetalhe_alpha_qualidade(void);
float faroldetalhe_alpha_bloco(void);
int   faroldetalhe_linha_porque(int i, char *dst, size_t cap);
int   faroldetalhe_linha_info(int i, char *dst, size_t cap);
int   faroldetalhe_selo_rotulo(int i, int *tipo, int32_t *id,
                               char *dst, size_t cap);

void  faroldetalhe_teste_reset(void);
void  faroldetalhe_teste_relogio(long long (*epochS)(void), Uint32 (*ms)(void));
void  faroldetalhe_teste_modelo(const FarolGosto *g);
void  faroldetalhe_teste_ficha(const char *imdb, const FarolFicha *f);
void  faroldetalhe_teste_semente(const char *imdb, const char *titulo,
                                 const FarolFicha *f, int terminou);
void  faroldetalhe_teste_vocab(int tipo, int32_t id, const char *nome,
                               const char *foto, int elencoCol);
void  faroldetalhe_teste_fontes(const char *alvo,
                                const FarolInfoFonte *f, int n,
                                const FarolInfoCaps *caps);
int   faroldetalhe_teste_json(const char *json, int nCand, Uint32 agoraMs);

#endif
