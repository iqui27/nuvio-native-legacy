// FAROL — erro nativo normalizado (PLANO.md secao 2.3).
//
// Cada backend entrega a falha do jeito que a TV fala: o uMS da LG manda
// "<errorCode> <errorText>" ("40403 server error:40403"), o Media3 manda
// "%d player error:%d", o AVPlay do .wgt manda o nome do erro
// ("PLAYER_ERROR_CONNECTION_FAILED") e o host .tpk manda o codigo CAPI
// ("Samsung player error 0xfe6c0026") ou uma linha "erro <NomeDoEnum>".
// Sem um codigo estavel, playbook e fingerprint teriam que conhecer quatro
// dialetos. farolerro_ler traduz todos para (classe, http, nativo, cod).
//
// Extensao pura de fontesonda_causa: o que nao casa com uma assinatura da
// plataforma cai no classificador generico, entao os dois nunca discordam de
// classe (FS_CAUSA_NENHUMA = "sem opiniao": o Farol pode ainda assim
// classificar pelo contexto da plataforma — ex.: uMS 100 vira FC_FORMATO).
//
// TABELA DE ASSINATURAS — lista real do D1 pendente (dono). Sem acesso ao
// D1 remoto nesta etapa: a tabela saiu do codigo e de tests/fontesonda_causa.c.
// Os nomes PLAYER_ERROR_* do AVPlay alem dos de conexao e os codigos Media3
// fora das familias medidas NAO sao de registros reais: teste em TV pendente.
// O que nao casar devolve 0 com cod "<plataforma>_outro" e o numero nativo em
// `nativo` — e exatamente isso que o registro existe para capturar.
//
//   LG (texto = "<errorCode> <errorText>", video.c:1615)
//     "40403 server error:40403" (40000+HTTP)   HTTP      ums_<codigo>
//     errorCode 300                              REDE      ums_300
//     errorCode 200 "Audio Codec Not Supported"  AUDIO     ums_200
//     errorCode 202/203/204/210, "100 Playing error"  FORMATO  ums_<codigo>
//     "… is not running"                         SISTEMA   ums_pipeline
//     hub LS2 negou (video_registro_negado)      SISTEMA   ls2_permissao
//     "Cannot safely identify the selected native audio track"  AUDIO  ums_faixa_audio
//     "network recovery timeout" (video.c:2970)  REDE      ums_rede
//   Android (texto = "%d player error:%d", video_android.c:804)
//     2000/2001/2002/2008                        REDE      m3_<codigo>
//     2004-2006                                  HTTP      m3_<codigo>  (2005=404, 2006=403)
//     3000-4999                                  FORMATO   m3_<codigo>
//     "… stalled download" / "… recovery timeout"  PARADO   m3_parado
//     "… audio sem decoder" (EV_AUDIO_SEM_DECODER vira texto do produtor)  AUDIO  m3_audio
//   .wgt AVPlay (texto = S.erro do onerror, video_tizen.c:1153)
//     CONNECTION_FAILED / NETWORK / TIMEOUT / TIMED_OUT,
//     "network recovery timeout" (:1362)         REDE      av_conexao
//     demais PLAYER_ERROR_<nome>                 FORMATO   av_<nome em minusculas>
//                                              (AUDIO se o nome tiver "audio")
//   .tpk (texto = "Samsung player error 0x%08x" ou linha "erro <Enum>" do host)
//     0xFE6C0026 / "erro ConnectionFailed"       REDE      tpk_conexao
//     0xFE6C002E / "(NotSupported)AudioCodec(NotSupported)"  AUDIO  tpk_audio
//     "interrompido: ResourceConflict"           SISTEMA   tpk_conflito
//     relogio congelado 15 s (TPK_PRESO_MS)      PARADO    tpk_preso
//
// `codigoDecoder` e o codigo nativo quando nao ha texto (o que
// video_erro_decoder_codigo devolve, ou o int do evento de erro do host).
// Convencoes: LG: LSR_PERMISSION (-1027) / LSR_NOT_PRIVILEGED (-1031) ->
// ls2_permissao; TPK: o int do EV_ERRO (0xFE6C00xx) e -2 -> tpk_preso
// (relogio congelado, recuperarPlayer).
#ifndef NV_FAROLERRO_H
#define NV_FAROLERRO_H

typedef enum { FP_WEBOS, FP_TIZEN_WGT, FP_TIZEN_TPK, FP_ANDROID,
               FP_OUTRA } FarolPlat;
typedef enum { FC_NENHUMA, FC_REDE, FC_HTTP, FC_FORMATO, FC_AUDIO,
               FC_SISTEMA, FC_PARADO } FarolClasse;

typedef struct {
  FarolClasse classe;  // FC_NENHUMA = nao reconhecido
  int http;            // status HTTP quando a classe e FC_HTTP (0 = nao sei)
  int nativo;          // codigo nativo do erro (0 = nenhum)
  char cod[24];        // token estavel para playbook/fingerprint
} FarolErro;

// Le o erro nativo. Devolve 1 quando reconheceu (classe preenchida). Devolve 0
// com classe=FC_NENHUMA e cod="<plataforma>_outro" quando nao reconheceu — o
// numero nativo segue em `nativo`, para o registro capturar a assinatura nova.
int farolerro_ler(FarolPlat p, const char *texto, int codigoDecoder, FarolErro *e);

// "ums", "av", "tpk", "m3" ou "gen" — prefixo dos cod da plataforma.
const char *farolerro_prefixo(FarolPlat p);
// "nenhuma", "rede", "http", "formato", "audio", "sistema" ou "parado".
const char *farolerro_classe_nome(FarolClasse c);

// Lista fechada dos `cod` fixos que farolerro_ler emite, em `cod` —
// o redator do Farol 4 inclui este par em vez de manter copia. Alem destes
// tokens ha tres familias geradas na hora: ums_<codigo do uMS>,
// m3_<codigo do Media3> e av_<nome do enum AVPlay em minusculas>.
extern const char *const farolerro_codigos[];
extern const int farolerro_codigos_n;

#endif
