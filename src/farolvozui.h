// CAMADA DE DITADO E REVISAO DE FRASE DO FAROL (ADENDO-2.1.md #31).
//
// Fluxo:
//   1. Botao de microfone (exibido quando `st_voz_disponivel()` == 1) abre
//      `farolvozui_abrir(voltaFase)`: chama `st_voz_iniciar(ST_BUSCA)`, exibe
//      "Ouvindo..." (FT_OUVINDO) e texto parcial via `st_ler()`, cancelavel
//      com Voltar (`st_fechar`).
//   2. Ao concluir a fala (`ST_FIM`), ou pelo gancho `farolvozui_abrir_texto`
//      (#32: teclado Samsung, QR celular, WASM), o texto aparece na tela ANTES
//      de qualquer pedido HTTP:
//        - OK confirma (ignorando `e->key.repeat != 0`) -> `farolbusca_pedir`
//        - Voltar descarta o texto com 0 pedidos HTTP
//   3. Enquanto busca em voo, exibe estado de espera cancelavel com Voltar via
//      `farolbusca_cancelar()`.
//   4. Ao concluir, confere `info.geracao` antes e depois de
//      `farolbusca_resolver()` (R8) e abre a Tela 7 ("Achados").
#ifndef NV_FAROLVOZUI_H
#define NV_FAROLVOZUI_H

#include <SDL2/SDL.h>

typedef enum {
  FV_INATIVO = 0,
  FV_OUVINDO,
  FV_REVISAO,
  FV_BUSCANDO,
  FV_AVISO
} FarolVozEstado;

enum {
  FV_FOCO_CONFIRMAR = 300,
  FV_FOCO_FALAR,
  FV_FOCO_VOLTAR
};

int            farolvozui_disponivel(void);
int            farolvozui_ativo(void);
FarolVozEstado farolvozui_estado(void);
int            farolvozui_foco(void);
int            farolvozui_volta_fase(void);
const char    *farolvozui_frase(void);
const char    *farolvozui_parcial(void);
const char    *farolvozui_aviso(void);

void farolvozui_reset(void);
int  farolvozui_abrir(int voltaFase);
int  farolvozui_abrir_texto(const char *texto, int voltaFase);
void farolvozui_fechar(void);

void farolvozui_evento(const SDL_Event *e, int *pediuVoltar, int *abriuAchados);
void farolvozui_atualizar(float dt, Uint32 agora, int *abriuAchados, int *pediuVoltar);
void farolvozui_desenhar(float ox, float oy, float alpha, Uint32 agora);

#endif
