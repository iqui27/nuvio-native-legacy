// FAROL — telas 4, 5 e 6 + estados §3.3 do envio do registro (issue 22).
//
// Modulo de UI do fluxo de envio ligado a farolenvio.h:
//   - Tela 4 ("Vai junto / Nunca vai"): espelho de farolvai.h + toggle opt-in
//     "Incluir o registro de texto" (sempre nasce 0 e volta para 0 ao reabrir)
//     + Mandar / Agora nao.
//   - Enviando: FT_ENVIANDO em fio proprio, cancelavel com Voltar.
//   - Tela 5 ("Enviado"): codigo NV-XXXX-XX em corpo grande + QR a direita
//     apontando para <BASE_URL>/v1/farol/r/<codigo> + Pronto.
//   - Tela 6 ("Ja corrigido"): quando GET /v1/farol/conhecido devolve
//     corrigido, nada e enviado; exibe versao corrigida + Atualizar agora / Fechar.
//   - Estados §3.3: Sem internet/Pendente (farol-pendente.json 24 h),
//     Servidor recusou (Tentar de novo / Fechar), Limite 429 (ultimo codigo
//     valido), Sem conta (abre Contas via RESOLVER_PEDIU_CONTAS).
#ifndef NV_FAROLENVIOUI_H
#define NV_FAROLENVIOUI_H

#include "farolregra.h"
#include "farolenvio.h"
#include <SDL2/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  FEUI_FECHADO = 0,
  FEUI_TELA4_CONFIRMA,
  FEUI_ENVIANDO,
  FEUI_TELA5_ENVIADO,
  FEUI_TELA6_CORRIGIDO,
  FEUI_PENDENTE,
  FEUI_FALHOU_RECUSOU,
  FEUI_FALHOU_429,
  FEUI_FALHOU_SEM_CONTA
} FarolEnvioUiTela;

enum {
  FEUI_B_MANDAR = 100,
  FEUI_B_AGORA_NAO,
  FEUI_B_TOGGLE_LOG,
  FEUI_B_PRONTO,
  FEUI_B_VOLTAR,
  FEUI_B_ATUALIZAR,
  FEUI_B_FECHAR,
  FEUI_B_TENTAR,
  FEUI_B_CONTAS
};

int  farolenvioui_presente(void);
int  farolenvioui_ativo(void);
void farolenvioui_abrir(int sintoma, int voltaFase, int doFarol,
                        const FarolVeredito *vered, const FarolFatos *ff,
                        const int *estados, const char *ultCodigo);
void farolenvioui_fechar(void);
void farolenvioui_esquecer(void);
void farolenvioui_evento(const SDL_Event *e);
/* Roda por quadro (inclusive quando fechado): chama farolenvio_passo() e
 * devolve 1 se ha acao/transicao para resolver.c processar. */
int  farolenvioui_passo(float dt, int *outPedido, int *outFechar,
                        int *outVolta, int *outRecarregar,
                        int *outDoFarolOk);
void farolenvioui_desenhar(float ox, float oy, float a);

/* Inspecao / testes */
FarolEnvioUiTela farolenvioui_tela(void);
int              farolenvioui_foco(void);
int              farolenvioui_incluir_log(void);
const char      *farolenvioui_qr_url(void);
int              farolenvioui_qr_ativo(void);
const char      *farolenvioui_codigo_exibir(void);
void             farolenvioui_definir_url(const char *baseUrl);

#ifdef __cplusplus
}
#endif

#endif /* NV_FAROLENVIOUI_H */
