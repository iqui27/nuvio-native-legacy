// FAROL — coletor do aparelho e do cliente (PLANO.md §2.6, §5 issue 8).
//
// Preenche as secoes `aparelho` e `cliente` do FarolRegistro v1 com parsers
// puros para cada plataforma (Android NUVIO_TV_INFO, linha [tv] do TPK,
// /proc/meminfo, casca WASM e webOS), filtro fechado de modelo (<= 32 chars,
// [A-Za-z0-9._ -], qualquer valor suspeito vira "?") e coleta assincrona em
// fio proprio: com cache, nunca bloqueia o fio principal. No TPK a linha [tv]
// do log pode tardar ~3 s (ou chegar alem do prefixo): a leitura em blocos
// cobre o arquivo inteiro e, sem a linha, o fio rele ate um prazo — e o
// proximo iniciar() recoleta em vez de congelar a leitura sem [tv].
//
// Modulo puro: sem SDL, sem rede, sem dados.h, sem _Thread_local (seguro no
// carregador ELF proprio do TPK40). Depende apenas de farolredator.h para a
// escrita no JSON por lista branca.
#ifndef NV_FAROLPERFIL_H
#define NV_FAROLPERFIL_H

#include "farolredator.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FAROL_PERFIL_MODELO_MAX 32
#define FAROL_PERFIL_SO_MAX     23
#define FAROL_PERFIL_VER_MAX    23
#define FAROL_PERFIL_COMMIT_MAX 12

typedef struct {
  int w, h;                    /* pixels; -1 = desconhecido */
  int hdr, dv, hevc, av1;      /* 1 = sim, 0 = nao, -1 = desconhecido */
} FarolPerfilTela;

typedef struct {
  int dts, truehd, eac3, ac3;  /* 1 = sim, 0 = nao, -1 = desconhecido */
} FarolPerfilAudio;

typedef struct {
  int    offline;              /* 0 = online, 1 = offline, -1 = desconhecido */
  int    hosts_com_falha;      /* >= 0, -1 = desconhecido */
  double vazao_mbps;           /* >= 0.0, < 0 = desconhecido */
} FarolPerfilRede;

typedef struct {
  char plataforma[16];         /* webos|tizen_wgt|tizen_tpk|android|outra */
  char so[24];                 /* "4.9", "6.5", "14", ou "?" */
  int  so_maior;               /* versao maior; -1 = desconhecido */
  int  sdk;                    /* Android API level; -1 = desconhecido */
  char modelo[FAROL_PERFIL_MODELO_MAX + 1]; /* filtrado <= 32, ou "?" */
  int  ram_total_mb;           /* MiB; -1 = desconhecido */
  int  ram_livre_mb;           /* MiB; -1 = desconhecido */
  FarolPerfilTela  tela;
  FarolPerfilAudio audio;
  FarolPerfilRede  rede;
} FarolPerfilAparelho;

typedef struct {
  int    glass_ui;             /* -1 = desconhecido, 0/1 */
  char   qualidade_max[16];    /* auto|4k|1080p|720p|480p ou "" */
  double tamanho_max_gb;       /* < 0 = desconhecido */
  char   dts[16];              /* auto|ac3|aac|direto|desligado ou "" */
  int    dv_mkv;               /* -1 = desconhecido, 0/1 */
  int    legenda_auto_sync;    /* -1 = desconhecido, 0/1 */
} FarolPerfilAjustes;

enum {
  FAROL_ADDON_CATALOGO = 1u << 0,
  FAROL_ADDON_FONTES   = 1u << 1,
  FAROL_ADDON_LEGENDAS = 1u << 2,
  FAROL_ADDON_META     = 1u << 3
};

enum {
  FAROL_DEBRID_TORBOX     = 1u << 0,
  FAROL_DEBRID_REALDEBRID = 1u << 1,
  FAROL_DEBRID_ALLDEBRID  = 1u << 2,
  FAROL_DEBRID_PREMIUMIZE = 1u << 3,
  FAROL_DEBRID_DEBRIDLINK = 1u << 4,
  FAROL_DEBRID_EASYDEBRID = 1u << 5
};

enum {
  FAROL_CONTA_TRAKT = 1u << 0,
  FAROL_CONTA_SIMKL = 1u << 1
};

typedef struct {
  int      total;              /* -1 = desconhecido */
  int      ativos;             /* -1 = desconhecido */
  int      isolados;           /* -1 = desconhecido */
  unsigned tipos_mask;         /* FAROL_ADDON_* */
} FarolPerfilAddons;

typedef struct {
  char               versao[24];   /* "2.1.0", ou "?" */
  char               commit[16];   /* 7..12 hex, ou "?" */
  char               build[16];    /* webos-ipk|tizen-wgt|tizen-tpk*|android-apk|debug */
  char               backend[16];  /* starfish|avplay|tpk|media3|outro */
  char               idioma[12];   /* "pt-BR", "en", ... ou "?" */
  int                aberto_s;     /* segundos; -1 = desconhecido */
  char               gpu[16];      /* auto:alto|...|desconhecido */
  FarolPerfilAjustes ajustes;
  FarolPerfilAddons  addons;
  unsigned           debrid_mask;  /* FAROL_DEBRID_* */
  unsigned           contas_mask;  /* FAROL_CONTA_* */
} FarolPerfilCliente;

typedef struct {
  FarolPerfilAparelho aparelho;
  FarolPerfilCliente  cliente;
} FarolPerfil;

/* Resultados dos parsers puros por plataforma */
typedef struct {
  char modelo[FAROL_PERFIL_MODELO_MAX + 1]; /* "?" se desconhecido/rejeitado */
  int  sdk;                                 /* 1..999, -1 se desconhecido */
  char so[24];                              /* "14", "11.0.1", ou "?" */
  int  so_maior;                            /* >= 1, -1 se desconhecido */
  char versao[24];                          /* "2.1.0", ou "?" */
} FarolTvInfoAndroid;

typedef struct {
  char modelo[FAROL_PERFIL_MODELO_MAX + 1]; /* "?" se desconhecido/rejeitado */
  char so[24];                              /* "6.5", "4.0", ou "?" */
  int  so_maior;                            /* >= 1, -1 se desconhecido */
  int  tela_w, tela_h;                      /* pixels; -1 se desconhecido */
} FarolTvInfoTpk;

typedef struct {
  char modelo[FAROL_PERFIL_MODELO_MAX + 1]; /* "?" se desconhecido/rejeitado */
  char so[24];                              /* "6.5", ou "?" */
  int  so_maior;                            /* >= 1, -1 se desconhecido */
} FarolTvInfoWgt;

/* Preenche `out` com valores desconhecidos (-1 / "?") e metadados de build. */
void farolperfil_padrao(FarolPerfil *out);

/* Parsers puros (sem I/O, sem estado global, sem alocacao). */
/* Filtra modelo: <= 32 bytes, [A-Za-z0-9._ -], rejeita URLs/IPs/tokens -> "?".
 * Devolve 1 se valido, 0 se virou "?". Seguro quando entrada == out. */
int farolperfil_filtrar_modelo(const char *entrada, char out[FAROL_PERFIL_MODELO_MAX + 1]);

/* NUVIO_TV_INFO do Android: "<MFR> <MODEL>|<SDK_INT>|<RELEASE>|<versionName>". */
int farolperfil_parse_android_info(const char *env, FarolTvInfoAndroid *out);

/* Linha [tv] do TPK: "[tv] modelo=<M> tizen=<V> host=<H> dotnet=<D> tela=<W>x<H>". */
int farolperfil_parse_tpk_tv(const char *texto, FarolTvInfoTpk *out);

/* /proc/meminfo: MemTotal e MemAvailable em MiB (-1 quando ausente/invalido). */
int farolperfil_parse_meminfo(const char *texto, int *total_mb, int *livre_mb);

/* Dimensao "WxH" (ex.: NUVIO_PANEL_SIZE="3840x2160"). */
int farolperfil_parse_tela(const char *texto, int *w, int *h);

/* Saida da casca WASM (tools/tizen-shell.html): "<modelo>|<versao>". */
int farolperfil_parse_wgt_info(const char *texto, FarolTvInfoWgt *out);

/* webOS: /var/run/nyx/os_info.json e /etc/starfish-release. */
int farolperfil_parse_webos_nyx(const char *json, char so[24], int *so_maior);
int farolperfil_parse_webos_starfish(const char *texto, char so[24], int *so_maior);

/* Coleta assincrona em fio proprio (com cache; sem [tv] no TPK, o proximo
 * farolperfil_iniciar() recoleta). farolperfil_iniciar() e
 * farolperfil_obter() NUNCA bloqueiam em I/O. */
void farolperfil_iniciar(void);
int  farolperfil_obter(FarolPerfil *out); /* 1 = cache pronto, 0 = ainda lendo */
int  farolperfil_pronto(void);
void farolperfil_encerrar(void);          /* junta o fio se ativo */

/* Receptores em tempo de execucao (seguros entre fios).
 * LG: a issue 7 obtem o modelo via configd e a issue 14 chama este receptor. */
int  farolperfil_definir_modelo(const char *texto);
void farolperfil_definir_tela(const FarolPerfilTela *tela);
void farolperfil_definir_audio(const FarolPerfilAudio *audio);
void farolperfil_definir_rede(const FarolPerfilRede *rede);
void farolperfil_definir_cliente(const FarolPerfilCliente *cliente);

/* Escreve `aparelho` e `cliente` no FarolJson ja aberto (lista branca §2.6). */
int farolperfil_escrever_aparelho(FarolJson *j, const FarolPerfilAparelho *a);
int farolperfil_escrever_cliente(FarolJson *j, const FarolPerfilCliente *c);
int farolperfil_escrever(FarolJson *j, const FarolPerfil *p);

#ifdef FAROL_PERFIL_TESTE
/* Injecao de fontes apenas para testes deterministas (sem mutar ambiente global). */
typedef struct {
  const char *plataforma;       /* NULL = padrao de compilacao */
  const char *meminfo_caminho;  /* NULL = padrao ("/proc/meminfo" no Linux) */
  const char *tpk_log_caminho;  /* NULL = nao ler arquivo de log */
  const char *nyx_caminho;      /* NULL = padrao */
  const char *starfish_caminho; /* NULL = padrao */
  const char *env_android_info; /* NULL = nao usar (ou usar real se usar_env_real) */
  const char *env_wgt_info;
  const char *env_tv_model;
  const char *env_tizen_ver;
  const char *env_panel_size;
  int         usar_env_real;
  void      (*antes_de_ler)(void); /* gancho opcional no fio trabalhador (prova nao bloqueio) */
} FarolPerfilTesteFontes;

void farolperfil_teste_reiniciar(void);
void farolperfil_teste_configurar(const FarolPerfilTesteFontes *cfg);
int  farolperfil_teste_esperar(int timeout_ms);
int  farolperfil_teste_leituras(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* NV_FAROLPERFIL_H */
