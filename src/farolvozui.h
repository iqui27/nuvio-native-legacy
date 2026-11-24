// CAMADA DE ENTRADA E REVISAO DE FRASE DO FAROL (ADENDO-2.1.md #31 e #32).
//
// Fluxo por plataforma:
//   - Android (voz == 1): botao "Falar" (FT_ACAO_FALAR) -> st_voz_iniciar ->
//     FV_OUVINDO -> ST_FIM -> FV_REVISAO.
//   - Samsung .tpk/.wgt e WASM (voz == 0 && ime == 1): botao "Teclado da TV"
//     (FT_ACAO_TECLADO_TV) -> st_ime_tipo(ST_IME_TEXTO) + st_ime_abrir ->
//     FV_DIGITANDO -> ST_FIM -> FV_REVISAO.
//   - LG webOS / QR celular (voz == 0 && cel == 1, ou alternancia quando
//     ime == 1 && cel == 1): botao "Digitar pelo celular"
//     (FT_ACAO_DIGITAR_CELULAR) -> celular_abrir_digitar(FT_DIGITE_CELULAR) +
//     QR real (qr_gerar / qrtex.h) -> FV_QR -> celular_pegar -> FV_REVISAO.
//
// Em todos os modos, a frase aparece em FV_REVISAO ANTES de qualquer POST HTTP:
//   - OK confirma (ignorando e->key.repeat != 0) -> farolbusca_pedir
//   - Voltar descarta com 0 pedidos HTTP, sem sujar a busca confirmada da Tela 7.
//   - Em FV_BUSCANDO, Voltar cancela via farolbusca_cancelar().
//   - Ao concluir, confere info.geracao (R8) e abre a Tela 7 ("Achados").
#ifndef NV_FAROLVOZUI_H
#define NV_FAROLVOZUI_H

#include <SDL2/SDL.h>

typedef enum {
  FV_INATIVO = 0,
  FV_OUVINDO,
  FV_DIGITANDO,
  FV_QR,
  FV_REVISAO,
  FV_BUSCANDO,
  FV_AVISO
} FarolVozEstado;

typedef enum {
  FV_MODO_NENHUM = 0,
  FV_MODO_VOZ,
  FV_MODO_IME,
  FV_MODO_QR
} FarolVozModo;

enum {
  FV_FOCO_CONFIRMAR = 300,
  FV_FOCO_FALAR,
  FV_FOCO_ALT,
  FV_FOCO_VOLTAR
};

int            farolvozui_disponivel(void);
const char    *farolvozui_rotulo_botao(void);
const char    *farolvozui_icone_botao(void);
FarolVozModo   farolvozui_modo_atual(void);
int            farolvozui_ativo(void);
FarolVozEstado farolvozui_estado(void);
int            farolvozui_foco(void);
int            farolvozui_volta_fase(void);
const char    *farolvozui_frase(void);
const char    *farolvozui_parcial(void);
const char    *farolvozui_aviso(void);

// Gancho deterministico para testes (-1 = deteccao real da plataforma; 0/1 = override).
void farolvozui_teste_modos(int voz, int ime, int cel);

void farolvozui_reset(void);
int  farolvozui_abrir(int voltaFase);
int  farolvozui_abrir_texto(const char *texto, int voltaFase);
void farolvozui_fechar(void);

void farolvozui_evento(const SDL_Event *e, int *pediuVoltar, int *abriuAchados);
void farolvozui_atualizar(float dt, Uint32 agora, int *abriuAchados, int *pediuVoltar);
void farolvozui_desenhar(float ox, float oy, float alpha, Uint32 agora);

#endif
