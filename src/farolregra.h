#ifndef NV_FAROLREGRA_H
#define NV_FAROLREGRA_H

#include "farolerro.h"
#include "resolverregra.h"
#include <stdint.h>

// "Cores estranhas": o sintoma vive em resolverregra.h com valor proprio
// (era 6 fixo e colidia com RES_S_GERAL — IMAGEM_DV disparava sem o sintoma).
#define FAROL_S_CORES RES_S_CORES

#define FAROL_P_WEBOS      (1u << FP_WEBOS)
#define FAROL_P_TIZEN_WGT  (1u << FP_TIZEN_WGT)
#define FAROL_P_TIZEN_TPK  (1u << FP_TIZEN_TPK)
#define FAROL_P_ANDROID    (1u << FP_ANDROID)
#define FAROL_P_OUTRA      (1u << FP_OUTRA)
#define FAROL_P_TODAS \
  (FAROL_P_WEBOS | FAROL_P_TIZEN_WGT | FAROL_P_TIZEN_TPK | FAROL_P_ANDROID | FAROL_P_OUTRA)

#define FAROL_CODIGOS(X) \
  X(NENHUM) \
  X(REDE_FORA) \
  X(REDE_NAO_CHEGA) \
  X(ADDON_NENHUM) \
  X(ADDON_LENTO) \
  X(DEBRID_SEM_PLANO) \
  X(DEBRID_DESCONECTOU) \
  X(FONTE_RECUSOU) \
  X(FONTE_SEM_RANGE) \
  X(REDE_CURTA) \
  X(TV_SEM_MEMORIA) \
  X(TV_NEGOU_PLAYER) \
  X(AUDIO_DTS_SEM_SAIDA) \
  X(AUDIO_OUTRA_FAIXA) \
  X(AUDIO_SEM_SAIDA) \
  X(IMAGEM_DV) \
  X(TV_FORMATO) \
  X(TV_FORMATO_REPETE) \
  X(PLAYER_PAROU) \
  X(NADA_ACHADO)

enum {
#define FAROL_DEF_C(k) FAROL_C_##k,
  FAROL_CODIGOS(FAROL_DEF_C)
#undef FAROL_DEF_C
  FAROL_C_N
};

enum {
  FAROL_A_NENHUMA = 0,
  FAROL_A_TENTAR_DE_NOVO,
  FAROL_A_RECONECTAR_REDE,
  FAROL_A_ABRIR_FONTES,
  FAROL_A_ISOLAR_ADDON,
  FAROL_A_ABRIR_DEBRID,
  FAROL_A_RECONECTAR_DEBRID,
  FAROL_A_PROXIMA_FONTE,
  FAROL_A_FONTE_MENOR,
  FAROL_A_FONTE_SEM_DV,
  FAROL_A_FONTE_SEM_DTS,
  FAROL_A_FONTE_OUTRO_FORMATO,
  FAROL_A_CONVERTER_AUDIO,
  FAROL_A_TROCAR_FAIXA,
  FAROL_A_ALIVIAR_REABRIR,
  FAROL_A_LIMPAR_CACHE,
  FAROL_A_BUFFER_MAIOR,
  FAROL_A_DESLIGAR_HDR,
  FAROL_A_PLAYER_EXTERNO,
  FAROL_A_ENVIAR_REGISTRO,
  FAROL_A_N
};

enum {
  FAROL_ESC_NUNCA = 0,
  FAROL_ESC_SE_FALHAR = 1,
  FAROL_ESC_JUNTO = 2
};

typedef struct {
  int offline, addonsQtd;
  int addonsMotivo;    // 1 = a ultima busca dos add-ons veio com motivo de falha
  int debridSemPlano, debridRecusa;
  int tvNegou;
  int haFalha;         // 1 = falha de reproducao recente, mesmo de causa nao lida
  FarolPlat plat;
  int osMaior;
  FarolErro erro;
  int erroIdadeS;   // 0..RES_REJEITA_S (6 h) = recente; < 0 = nao sei; > RES_REJEITA_S = vencido
  int falhasSeguidas;
  int paradas5min;
  int addonsLentos, addonsMudos;
  int audioRecusado, faixaDts, conversaoPossivel, outraFaixa;
  int fonteDv, fonteHdr, tvDv, tvHdr;
  int fonteGBx10, vazaoMbps;
  int memLivreMB;
  int temOutraFonte, temFonteMenor, temFonteSemDv, temFonteSemDts;
  int externoDisponivel;
  int sondaRange;
  int versaoNova;
  int fonteMbps;
  int fonteDebrid;
  int sintoma;
} FarolFatos;

typedef struct {
  uint16_t codigo;
  uint8_t  estagio;
  uint8_t  plats;
  int    (*casa)(const FarolFatos *);
  const char *l1, *l2;
  uint8_t  acao, acaoAlt;
  uint8_t  escala;
} FarolRegra;

typedef struct {
  const FarolRegra *regra;
  int estado[RES_T_N];
} FarolVeredito;

void farolregra_fatos_iniciar(FarolFatos *f);
void farolregra_julgar(int sintoma, const FarolFatos *f, FarolVeredito *v);
const FarolRegra *farolregra_tabela(int *n);
const char *farolregra_codigo_nome(int codigo);
const char *farolregra_acao_texto(int acao);
const char *farolregra_acao_token(int acao);

#endif
