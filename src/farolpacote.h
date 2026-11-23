// FAROL — montador do pacote FarolRegistro v1 (PLANO.md §2.5, §2.6, §5 issue 11).
//
// Junta o anel de eventos (farolanel), o perfil do aparelho/cliente (farolperfil),
// o veredito, os metadados tecnicos da midia e a cadeia de 6 passos num JSON
// fechado pela lista branca do redator (farolredator), com `fingerprint` legivel
// e `fp` (16 primeiros hex do SHA-256 dele).
//
// Modulo puro: sem SDL, sem rede, sem dados.h, sem alocacao dinamica e sem
// _Thread_local (seguro no carregador ELF proprio do TPK40).
//
// Teto: <= FAROL_JSON_MAX (35 000 bytes). Quando o anel cheio ultrapassaria o
// teto (ou o buffer do chamador), descarta os eventos mais antigos de `linha`
// mantendo os mais recentes em ordem cronologica e preservando `privacidade`.
#ifndef NV_FAROLPACOTE_H
#define NV_FAROLPACOTE_H

#include "farolanel.h"
#include "farolerro.h"
#include "farolperfil.h"
#include "farolredator.h"
#include "resolverregra.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FAROL_PACOTE_JANELA_MS   300000u /* 5 minutos */
#define FAROL_PACOTE_TENTOU_MAX  64      /* mesmo teto LISTA_MAX do servidor */
#define FAROL_PACOTE_FP_MAX      160     /* limite de erro.fingerprint */
#define FAROL_PACOTE_FP_HEX_N    17      /* 16 hex + NUL */

typedef enum {
  FAROL_G_FALHA_SEM_CONSERTO = 0,
  FAROL_G_MANUAL,
  FAROL_G_FALHA_REPETIDA
} FarolGatilho;

typedef enum {
  FAROL_PS_NAO_COMECA = 0,
  FAROL_PS_TRAVA,
  FAROL_PS_SEM_SOM,
  FAROL_PS_LEGENDA,
  FAROL_PS_QUALIDADE,
  FAROL_PS_OUTRO
} FarolPacoteSintoma;

enum {
  FAROL_EST_NAO_TESTADO = RES_NAO_TESTADO,
  FAROL_EST_OK          = RES_OK,
  FAROL_EST_FALHOU      = RES_FALHOU,
  FAROL_EST_TESTANDO    = 3
};

typedef struct {
  const char *acao;  /* enum de erro.tentou[].acao (ex.: "proxima_fonte") */
  int         ok;    /* 0 ou 1 */
  int         ms;    /* duracao em ms */
} FarolPacoteTentativa;

typedef struct {
  const char *container; /* mkv|mp4|ts|hls|dash|avi|webm|outro|x */
  const char *video;     /* h264|hevc|hevc10|av1|vp9|mpeg2|outro|x */
  const char *hdr;       /* sdr|hdr10|hdr10plus|hlg|dv|x */
  const char *res;       /* 2160p|1080p|720p|480p|outra|x */
  double      fps;       /* >= 0.0; < 0 omite */
  double      mbps;      /* >= 0.0; < 0 omite */
  double      gb;        /* >= 0.0; < 0 omite */
  const char *audio;     /* aac|aac_20|ac3|ac3_51|eac3|eac3_51|eac3_atmos|dts|dts_51|truehd|truehd_atmos|mp3|flac|outro|x */
  const char *legenda;   /* srt_embutida|srt_externa|ass_embutida|ass_externa|pgs|nenhuma|outra|x */
  const char *entrega;   /* debrid_cache|debrid|http|torrent|local|outro|x */
  int         http;      /* >= 0; < 0 omite */
  int         ttfb_ms;   /* >= 0; < 0 omite */
} FarolPacoteMidia;

typedef struct {
  int estado;  /* FAROL_EST_* (compativel com RES_NAO_TESTADO/RES_OK/RES_FALHOU) */
  int ms;      /* >= 0 emite `ms`; < 0 omite */
  int http;    /* > 0 emite `http`; <= 0 omite */
} FarolPacotePasso;

typedef struct {
  FarolGatilho                gatilho;
  FarolPacoteSintoma          sintoma;
  const char                 *codigo;      /* codigo do playbook (ex.: "TV_FORMATO_REPETE") */
  int                         estagio;     /* RES_T_INTERNET..RES_T_AUDIO, ou -1 para omitir */
  FarolErro                   erro;        /* ultimo erro normalizado */
  const FarolPacoteTentativa *tentou;      /* pode ser NULL quando n_tentou == 0 */
  int                         n_tentou;    /* 0..FAROL_PACOTE_TENTOU_MAX */
  const FarolPerfil          *perfil;      /* NULL = usa snapshot de farolperfil_obter() */
  FarolPacoteMidia            midia;
  FarolPacotePasso            cadeia[RES_T_N];
  int                         tem_cadeia;  /* 1 = emite os 6 passos; 0 = emite cadeia vazia */
  const FarolEvento          *eventos;     /* NULL = le do anel global farolanel_ler() */
  int                         n_eventos;   /* usado apenas quando eventos != NULL */
  uint32_t                    agora_ms;    /* relogio de referencia; 0 = t_ms do evento mais novo */
  uint32_t                    janela_ms;   /* janela da linha em ms (0 = sem filtro de janela) */
} FarolPacoteEntrada;

typedef struct {
  size_t   bytes;                      /* tamanho do JSON gerado sem NUL; 0 em erro */
  unsigned campos_recusados;           /* total recusado pelo redator */
  int      linha_total;                /* eventos elegiveis na janela antes do corte */
  int      linha_escritos;             /* eventos escritos em `linha` */
  int      linha_cortados;             /* eventos mais antigos descartados pelo teto */
  char     fingerprint[FAROL_PACOTE_FP_MAX];
  char     fp[FAROL_PACOTE_FP_HEX_N];
} FarolPacoteRes;

/* Espelho na tela (§2.5 item 6): cada grupo/prefixo do schema de farolredator
 * mapeia para uma linha FT_VAI_* de faroltexto.h. As colunas "Vai junto" e
 * "Nunca vai" saem desta tabela unica. */
typedef struct {
  const char *prefixo;  /* prefixo do caminho no schema (ex.: "aparelho", "cliente.ajustes") */
  const char *frase;    /* constante FT_VAI_* de faroltexto.h */
} FarolEspelhoGrupo;

/* Inicializa structs com desconhecidos seguros (-1 / "x") e janela de 5 min. */
void farolpacote_midia_padrao(FarolPacoteMidia *m);
void farolpacote_entrada_padrao(FarolPacoteEntrada *e);

/* Converte sintoma de resolverregra.h (RES_S_*) para FarolPacoteSintoma. */
FarolPacoteSintoma farolpacote_sintoma_de_resolver(int res_sintoma);

/* Monta `fingerprint` (<plat>-<so_maior>-<backend>-<nativo>-<cont>-<vid>-<hdr>-<aud>)
 * e `fp` (16 primeiros hex do SHA-256). Deterministico e fechado. Devolve 1 em sucesso. */
int farolpacote_fingerprint(const FarolPerfil *perfil,
                            const FarolErro *erro,
                            const FarolPacoteMidia *midia,
                            char fp_texto[FAROL_PACOTE_FP_MAX],
                            char fp_hex[FAROL_PACOTE_FP_HEX_N]);

/* Monta o JSON FarolRegistro v1 em `buf` (capado em FAROL_JSON_MAX = 35 000 B).
 * Devolve 1 em sucesso, 0 se erro de argumento ou buffer insuficiente. */
int farolpacote_montar(const FarolPacoteEntrada *in,
                       char *buf, size_t tam,
                       FarolPacoteRes *res);

/* Tabelas do espelho na tela ("Vai junto" / "Nunca vai", §2.5 item 6). */
const FarolEspelhoGrupo *farolpacote_espelho_grupos(size_t *n);
const char *farolpacote_espelho_para(const char *caminho);
const char *const *farolpacote_vai_junto(size_t *n);
const char *const *farolpacote_nunca_vai(size_t *n);

#ifdef __cplusplus
}
#endif

#endif /* NV_FAROLPACOTE_H */
