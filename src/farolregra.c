#include "farolregra.h"
#include "dts_tv.h"
#include "faroltexto.h"
#include "memlog.h"
#include <limits.h>
#include <string.h>

static uint8_t plat_bit(FarolPlat p) {
  if (p < FP_WEBOS || p > FP_OUTRA) p = FP_OUTRA;
  return (uint8_t)(1u << (unsigned)p);
}

static int erro_recente(const FarolFatos *f) {
  return f->erro.classe != FC_NENHUMA &&
         f->erroIdadeS >= 0 && f->erroIdadeS <= RES_REJEITA_S;
}

static int eh_erro_negou(const FarolFatos *f) {
  return erro_recente(f) && f->erro.classe == FC_SISTEMA &&
         (!strcmp(f->erro.cod, "ls2_permissao") || !strcmp(f->erro.cod, "tpk_conflito"));
}

static int taxa_mbps(const FarolFatos *f) {
  int64_t t;
  if (f->fonteMbps < 0 || f->fonteGBx10 < 0) return 0;
  if (f->fonteMbps > 0) return f->fonteMbps;
  if (f->fonteGBx10 > 0) {
    t = ((int64_t)f->fonteGBx10 + 8) / 9;
    return t > INT_MAX ? INT_MAX : (int)t;
  }
  return 0;
}

static int vazao_boa(const FarolFatos *f) {
  int t;
  if (f->vazaoMbps <= 0 || f->fonteMbps < 0 || f->fonteGBx10 < 0) return 0;
  t = taxa_mbps(f);
  return t > 0 ? f->vazaoMbps >= t : f->vazaoMbps >= 10;
}

static int dts_sem_suporte(const FarolFatos *f) {
  if (f->faixaDts <= 0) return 0;
  switch (f->plat) {
    case FP_WEBOS:
      return f->osMaior > 0 && !nv_dts_tv_decodifica(f->osMaior);
    case FP_TIZEN_WGT:
    case FP_TIZEN_TPK:
      return 1;
    default:
      return 0;
  }
}

static int sem_saida_audio(const FarolFatos *f) {
  if (f->audioRecusado > 0) return 1;
  if (erro_recente(f) && f->erro.classe == FC_AUDIO) return 1;
  return dts_sem_suporte(f);
}

static int casa_rede_fora(const FarolFatos *f) {
  return f->offline > 0;
}

static int casa_addon_nenhum(const FarolFatos *f) {
  return f->addonsQtd == 0;
}

static int casa_addon_lento_3(const FarolFatos *f) {
  int64_t s;
  if (f->sintoma != RES_S_TRAVA || f->addonsLentos < 0 || f->addonsMudos < 0) return 0;
  s = (int64_t)f->addonsLentos + (int64_t)f->addonsMudos;
  return s >= 3;
}

static int casa_addon_lento(const FarolFatos *f) {
  int64_t s;
  if (f->sintoma != RES_S_TRAVA || f->addonsLentos < 0 || f->addonsMudos < 0) return 0;
  s = (int64_t)f->addonsLentos + (int64_t)f->addonsMudos;
  return s >= 1;
}

static int casa_debrid_sem_plano(const FarolFatos *f) {
  return f->debridSemPlano > 0;
}

static int casa_debrid_desconectou(const FarolFatos *f) {
  if (f->debridRecusa > 0) return 1;
  if (f->fonteDebrid <= 0 || !erro_recente(f)) return 0;
  return f->erro.classe == FC_HTTP && (f->erro.http == 401 || f->erro.http == 403);
}

static int casa_rede_nao_chega(const FarolFatos *f) {
  return f->offline == 0 && erro_recente(f) && f->erro.classe == FC_REDE;
}

static int casa_fonte_recusou(const FarolFatos *f) {
  if (!erro_recente(f) || f->erro.classe != FC_HTTP) return 0;
  return f->erro.http == 0 || (f->erro.http >= 400 && f->erro.http <= 599);
}

static int casa_fonte_sem_range(const FarolFatos *f) {
  return f->sondaRange == 200;
}

static int casa_rede_curta(const FarolFatos *f) {
  int t;
  if (f->paradas5min < 2 || f->vazaoMbps <= 0) return 0;
  t = taxa_mbps(f);
  return t > 0 && f->vazaoMbps < t;
}

static int casa_tv_negou(const FarolFatos *f) {
  if (f->tvNegou > 0) return 1;
  return eh_erro_negou(f);
}

static int casa_tv_sem_memoria(const FarolFatos *f) {
  if (f->memLivreMB < 0 || f->memLivreMB >= MEMLOG_PISO_MB) return 0;
  if (f->tvNegou > 0 || eh_erro_negou(f)) return 0;
  return erro_recente(f) &&
         (f->erro.classe == FC_SISTEMA || f->erro.classe == FC_FORMATO);
}

static int casa_imagem_dv(const FarolFatos *f) {
  return f->sintoma == FAROL_S_CORES && f->fonteDv > 0 && f->tvDv == 0;
}

static int casa_tv_formato_rep_ext(const FarolFatos *f) {
  return erro_recente(f) && f->erro.classe == FC_FORMATO &&
         f->falhasSeguidas >= 2 && f->externoDisponivel > 0;
}

static int casa_tv_formato_repete(const FarolFatos *f) {
  return erro_recente(f) && f->erro.classe == FC_FORMATO && f->falhasSeguidas >= 2;
}

static int casa_tv_formato_ext(const FarolFatos *f) {
  return erro_recente(f) && f->erro.classe == FC_FORMATO && f->externoDisponivel > 0;
}

static int casa_tv_formato(const FarolFatos *f) {
  return erro_recente(f) && f->erro.classe == FC_FORMATO;
}

static int casa_player_parou(const FarolFatos *f) {
  return erro_recente(f) && f->erro.classe == FC_PARADO && vazao_boa(f);
}

static int casa_audio_dts(const FarolFatos *f) {
  if (f->conversaoPossivel <= 0 || f->faixaDts <= 0) return 0;
  if (f->plat == FP_TIZEN_TPK && f->osMaior < 6) return 0;
  return sem_saida_audio(f);
}

static int casa_audio_outra_faixa(const FarolFatos *f) {
  return f->outraFaixa > 0 && sem_saida_audio(f);
}

static int casa_audio_sem_saida_ext(const FarolFatos *f) {
  return f->externoDisponivel > 0 && sem_saida_audio(f);
}

static int casa_audio_sem_saida(const FarolFatos *f) {
  return sem_saida_audio(f);
}

static int casa_sempre(const FarolFatos *f) {
  (void)f;
  return 1;
}

#define P_MEM  (FAROL_P_WEBOS | FAROL_P_TIZEN_TPK | FAROL_P_ANDROID)
#define P_NEG  (FAROL_P_WEBOS | FAROL_P_TIZEN_TPK)
#define P_CONV (FAROL_P_WEBOS | FAROL_P_TIZEN_TPK)

static const FarolRegra tabela[] = {
  { FAROL_C_REDE_FORA, RES_T_INTERNET, FAROL_P_TODAS, casa_rede_fora,
    FT_REDE_FORA_L1, FT_REDE_FORA_L2,
    FAROL_A_TENTAR_DE_NOVO, FAROL_A_RECONECTAR_REDE, FAROL_ESC_NUNCA },

  { FAROL_C_ADDON_NENHUM, RES_T_ADDONS, FAROL_P_TODAS, casa_addon_nenhum,
    FT_ADDON_NENHUM_L1, FT_ADDON_NENHUM_L2,
    FAROL_A_ABRIR_FONTES, FAROL_A_NENHUMA, FAROL_ESC_NUNCA },
  { FAROL_C_ADDON_LENTO, RES_T_ADDONS, FAROL_P_TODAS, casa_addon_lento_3,
    FT_ADDON_LENTO_L1, FT_ADDON_LENTO_L2,
    FAROL_A_ISOLAR_ADDON, FAROL_A_LIMPAR_CACHE, FAROL_ESC_SE_FALHAR },
  { FAROL_C_ADDON_LENTO, RES_T_ADDONS, FAROL_P_TODAS, casa_addon_lento,
    FT_ADDON_LENTO_L1, FT_ADDON_LENTO_L2,
    FAROL_A_ISOLAR_ADDON, FAROL_A_NENHUMA, FAROL_ESC_NUNCA },

  { FAROL_C_DEBRID_SEM_PLANO, RES_T_DEBRID, FAROL_P_TODAS, casa_debrid_sem_plano,
    FT_DEBRID_SEM_PLANO_L1, FT_DEBRID_SEM_PLANO_L2,
    FAROL_A_ABRIR_DEBRID, FAROL_A_NENHUMA, FAROL_ESC_NUNCA },
  { FAROL_C_DEBRID_DESCONECTOU, RES_T_DEBRID, FAROL_P_TODAS, casa_debrid_desconectou,
    FT_DEBRID_DESCONECTOU_L1, FT_DEBRID_DESCONECTOU_L2,
    FAROL_A_RECONECTAR_DEBRID, FAROL_A_NENHUMA, FAROL_ESC_NUNCA },

  { FAROL_C_REDE_NAO_CHEGA, RES_T_FONTE, FAROL_P_TODAS, casa_rede_nao_chega,
    FT_REDE_NAO_CHEGA_L1, FT_REDE_NAO_CHEGA_L2,
    FAROL_A_RECONECTAR_REDE, FAROL_A_PROXIMA_FONTE, FAROL_ESC_SE_FALHAR },
  { FAROL_C_FONTE_RECUSOU, RES_T_FONTE, FAROL_P_TODAS, casa_fonte_recusou,
    FT_FONTE_RECUSOU_L1, FT_FONTE_RECUSOU_L2,
    FAROL_A_PROXIMA_FONTE, FAROL_A_NENHUMA, FAROL_ESC_SE_FALHAR },
  { FAROL_C_FONTE_SEM_RANGE, RES_T_FONTE, FAROL_P_TODAS, casa_fonte_sem_range,
    FT_FONTE_SEM_RANGE_L1, FT_FONTE_SEM_RANGE_L2,
    FAROL_A_PROXIMA_FONTE, FAROL_A_NENHUMA, FAROL_ESC_NUNCA },
  { FAROL_C_REDE_CURTA, RES_T_FONTE, FAROL_P_TODAS, casa_rede_curta,
    FT_REDE_CURTA_L1, FT_REDE_CURTA_L2,
    FAROL_A_FONTE_MENOR, FAROL_A_BUFFER_MAIOR, FAROL_ESC_NUNCA },

  { FAROL_C_TV_SEM_MEMORIA, RES_T_TV, P_MEM, casa_tv_sem_memoria,
    FT_TV_SEM_MEMORIA_L1, FT_TV_SEM_MEMORIA_L2,
    FAROL_A_ALIVIAR_REABRIR, FAROL_A_LIMPAR_CACHE, FAROL_ESC_SE_FALHAR },
  { FAROL_C_TV_NEGOU_PLAYER, RES_T_TV, P_NEG, casa_tv_negou,
    FT_TV_NEGOU_PLAYER_L1, FT_TV_NEGOU_PLAYER_L2,
    FAROL_A_ENVIAR_REGISTRO, FAROL_A_NENHUMA, FAROL_ESC_JUNTO },
  { FAROL_C_IMAGEM_DV, RES_T_TV, FAROL_P_TODAS, casa_imagem_dv,
    FT_IMAGEM_DV_L1, FT_IMAGEM_DV_L2,
    FAROL_A_FONTE_SEM_DV, FAROL_A_DESLIGAR_HDR, FAROL_ESC_NUNCA },
  { FAROL_C_TV_FORMATO_REPETE, RES_T_TV, FAROL_P_ANDROID, casa_tv_formato_rep_ext,
    FT_TV_FORMATO_REPETE_L1, FT_L2_PLAYER_EXTERNO,
    FAROL_A_PLAYER_EXTERNO, FAROL_A_FONTE_OUTRO_FORMATO, FAROL_ESC_JUNTO },
  { FAROL_C_TV_FORMATO_REPETE, RES_T_TV, FAROL_P_TODAS, casa_tv_formato_repete,
    FT_TV_FORMATO_REPETE_L1, FT_TV_FORMATO_REPETE_L2,
    FAROL_A_FONTE_OUTRO_FORMATO, FAROL_A_LIMPAR_CACHE, FAROL_ESC_JUNTO },
  { FAROL_C_TV_FORMATO, RES_T_TV, FAROL_P_ANDROID, casa_tv_formato_ext,
    FT_TV_FORMATO_L1, FT_TV_FORMATO_L2,
    FAROL_A_PROXIMA_FONTE, FAROL_A_PLAYER_EXTERNO, FAROL_ESC_SE_FALHAR },
  { FAROL_C_TV_FORMATO, RES_T_TV, FAROL_P_TODAS, casa_tv_formato,
    FT_TV_FORMATO_L1, FT_TV_FORMATO_L2,
    FAROL_A_PROXIMA_FONTE, FAROL_A_NENHUMA, FAROL_ESC_SE_FALHAR },
  { FAROL_C_PLAYER_PAROU, RES_T_TV, FAROL_P_TODAS, casa_player_parou,
    FT_PLAYER_PAROU_L1, FT_PLAYER_PAROU_L2,
    FAROL_A_BUFFER_MAIOR, FAROL_A_PROXIMA_FONTE, FAROL_ESC_JUNTO },

  { FAROL_C_AUDIO_DTS_SEM_SAIDA, RES_T_AUDIO, P_CONV, casa_audio_dts,
    FT_AUDIO_DTS_L1, FT_AUDIO_DTS_L2,
    FAROL_A_CONVERTER_AUDIO, FAROL_A_FONTE_SEM_DTS, FAROL_ESC_SE_FALHAR },
  { FAROL_C_AUDIO_OUTRA_FAIXA, RES_T_AUDIO, FAROL_P_TODAS, casa_audio_outra_faixa,
    FT_AUDIO_OUTRA_FAIXA_L1, FT_AUDIO_OUTRA_FAIXA_L2,
    FAROL_A_TROCAR_FAIXA, FAROL_A_FONTE_SEM_DTS, FAROL_ESC_NUNCA },
  { FAROL_C_AUDIO_SEM_SAIDA, RES_T_AUDIO, FAROL_P_ANDROID, casa_audio_sem_saida_ext,
    FT_AUDIO_SEM_SAIDA_L1, FT_AUDIO_SEM_SAIDA_L2,
    FAROL_A_FONTE_SEM_DTS, FAROL_A_PLAYER_EXTERNO, FAROL_ESC_SE_FALHAR },
  { FAROL_C_AUDIO_SEM_SAIDA, RES_T_AUDIO, FAROL_P_TODAS, casa_audio_sem_saida,
    FT_AUDIO_SEM_SAIDA_L1, FT_AUDIO_SEM_SAIDA_L2,
    FAROL_A_FONTE_SEM_DTS, FAROL_A_NENHUMA, FAROL_ESC_SE_FALHAR },

  { FAROL_C_NADA_ACHADO, RES_T_AUDIO, FAROL_P_TODAS, casa_sempre,
    FT_NADA_ACHADO_L1, FT_NADA_ACHADO_L2,
    FAROL_A_ENVIAR_REGISTRO, FAROL_A_NENHUMA, FAROL_ESC_JUNTO },
};

#define TABELA_N ((int)(sizeof tabela / sizeof tabela[0]))
_Static_assert(TABELA_N >= FAROL_C_NADA_ACHADO, "faltam codigos na tabela");

void farolregra_fatos_iniciar(FarolFatos *f) {
  if (!f) return;
  memset(f, 0, sizeof *f);
  f->addonsQtd = 1;
  f->fonteDv = -1;
  f->fonteHdr = -1;
  f->tvDv = -1;
  f->tvHdr = -1;
  f->memLivreMB = -1;
}

static void preencher_estados(int sintoma, const FarolFatos *f, uint8_t pbit,
                              int estado[RES_T_N]) {
  ResolverFatos rf;
  ResolverResultado rr;
  int i, falhou[RES_T_N];
  memset(&rf, 0, sizeof rf);
  memset(falhou, 0, sizeof falhou);
  for (i = 0; i < TABELA_N - 1; i++) {
    if ((tabela[i].plats & pbit) && tabela[i].casa(f))
      falhou[tabela[i].estagio] = 1;
  }
  if (f->offline >= 0 && erro_recente(f) && f->erro.classe == FC_REDE) falhou[RES_T_FONTE] = 1;
  if (f->addonsMotivo > 0) falhou[RES_T_ADDONS] = 1;
  if (f->tvNegou > 0) falhou[RES_T_TV] = 1;
  rf.offline = falhou[RES_T_INTERNET];
  rf.addonsQtd = f->addonsQtd == 0 ? 0 : 1;
  if (falhou[RES_T_ADDONS] && rf.addonsQtd > 0) rf.addonsMotivo[0] = 'x';
  if (falhou[RES_T_DEBRID]) rf.debridMotivo[0] = 'x';
  // A falha generica recente conta mesmo sem causa reconhecida: o filtro de
  // erro_recente() pertence as regras por classe, nao a etapa da cadeia.
  rf.haFalha = falhou[RES_T_FONTE] || f->haFalha > 0;
  if (falhou[RES_T_TV]) rf.tvMotivo[0] = 'x';
  if (falhou[RES_T_AUDIO]) rf.audioMotivo[0] = 'x';
  resolverregra_testar(sintoma, &rf, &rr);
  memcpy(estado, rr.estado, sizeof rr.estado);
}

void farolregra_julgar(int sintoma, const FarolFatos *f, FarolVeredito *v) {
  FarolFatos ff;
  uint8_t pbit;
  int i;
  if (!v) return;
  if (f) {
    ff = *f;
  } else {
    farolregra_fatos_iniciar(&ff);
  }
  ff.sintoma = sintoma;
  pbit = plat_bit(ff.plat);
  v->regra = &tabela[TABELA_N - 1];
  for (i = 0; i < TABELA_N; i++) {
    if ((tabela[i].plats & pbit) && tabela[i].casa(&ff)) {
      v->regra = &tabela[i];
      break;
    }
  }
  preencher_estados(sintoma, &ff, pbit, v->estado);
}

const FarolRegra *farolregra_tabela(int *n) {
  if (n) *n = TABELA_N;
  return tabela;
}

const char *farolregra_codigo_nome(int codigo) {
  static const char *const nomes[FAROL_C_N] = {
#define FAROL_STR_C(k) #k,
    FAROL_CODIGOS(FAROL_STR_C)
#undef FAROL_STR_C
  };
  return codigo >= 0 && codigo < FAROL_C_N ? nomes[codigo] : "?";
}

const char *farolregra_acao_texto(int acao) {
  switch (acao) {
    case FAROL_A_TENTAR_DE_NOVO:      return FT_ACAO_TENTAR;
    case FAROL_A_RECONECTAR_REDE:     return FT_ACAO_RECONECTAR_REDE;
    case FAROL_A_ABRIR_FONTES:        return FT_ACAO_ABRIR_FONTES;
    case FAROL_A_ISOLAR_ADDON:        return FT_ACAO_ISOLAR;
    case FAROL_A_ABRIR_DEBRID:        return FT_ACAO_ABRIR_DEBRID;
    case FAROL_A_RECONECTAR_DEBRID:   return FT_ACAO_DEBRID_CELULAR;
    case FAROL_A_PROXIMA_FONTE:       return FT_ACAO_PROXIMA_FONTE;
    case FAROL_A_FONTE_MENOR:         return FT_ACAO_FONTE_MENOR;
    case FAROL_A_FONTE_SEM_DV:        return FT_ACAO_FONTE_SEM_DV;
    case FAROL_A_FONTE_SEM_DTS:       return FT_ACAO_FONTE_SEM_DTS;
    case FAROL_A_FONTE_OUTRO_FORMATO: return FT_ACAO_FONTE_FORMATO;
    case FAROL_A_CONVERTER_AUDIO:     return FT_ACAO_CONVERTER;
    case FAROL_A_TROCAR_FAIXA:        return FT_ACAO_TROCAR_FAIXA;
    case FAROL_A_ALIVIAR_REABRIR:     return FT_ACAO_ALIVIAR;
    case FAROL_A_LIMPAR_CACHE:        return FT_ACAO_LIMPAR_CACHE;
    case FAROL_A_BUFFER_MAIOR:        return FT_ACAO_BUFFER_MAIOR;
    case FAROL_A_DESLIGAR_HDR:        return FT_ACAO_DESLIGAR_HDR;
    case FAROL_A_PLAYER_EXTERNO:      return FT_ACAO_PLAYER_EXTERNO;
    case FAROL_A_ENVIAR_REGISTRO:     return FT_ACAO_MANDAR_REGISTRO;
    default:                          return "";
  }
}

const char *farolregra_acao_token(int acao) {
  switch (acao) {
    case FAROL_A_TENTAR_DE_NOVO:      return "reconexao";
    case FAROL_A_RECONECTAR_REDE:     return "reconectar_rede";
    case FAROL_A_ABRIR_FONTES:        return "login_addon";
    case FAROL_A_ISOLAR_ADDON:        return "isolar_addon";
    case FAROL_A_ABRIR_DEBRID:
    case FAROL_A_RECONECTAR_DEBRID:   return "login_debrid";
    case FAROL_A_PROXIMA_FONTE:
    case FAROL_A_FONTE_OUTRO_FORMATO: return "proxima_fonte";
    case FAROL_A_FONTE_MENOR:         return "fonte_menor";
    case FAROL_A_FONTE_SEM_DV:        return "sem_dv";
    case FAROL_A_FONTE_SEM_DTS:       return "sem_dts";
    case FAROL_A_CONVERTER_AUDIO:     return "converter_ac3";
    case FAROL_A_TROCAR_FAIXA:        return "sem_dts";
    case FAROL_A_ALIVIAR_REABRIR:     return "aliviar_memoria";
    case FAROL_A_LIMPAR_CACHE:        return "limpar_cache";
    case FAROL_A_BUFFER_MAIOR:        return "buffer_maior";
    case FAROL_A_DESLIGAR_HDR:        return "desligar_hdr";
    case FAROL_A_PLAYER_EXTERNO:      return "player_externo";
    default:                          return "";
  }
}
