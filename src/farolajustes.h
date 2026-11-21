// FAROL (#26): lista branca FECHADA dos ajustes que a secao "ajustes" do
// registro tecnico pode devolver (PLANO §5: diferenca por lista branca, ~25
// itens). O criterio e so ajudar a reproduzir uma falha: fonte, video, audio,
// buffer e o que muda o comportamento do player. Nenhum dado do usuario.
//
// REGRAS da lista:
//   - so opcao de escolha (BOOL/ENUM) ou NUMERO; texto livre nunca entra e
//     nem existe no valor[] de quem entra — texto fica fora por construcao;
//   - nenhuma chave que ajlog_chave_sensivel marcaria (URL, credencial, nome,
//     email, PIN, regex...); tests/farol_ajustes.sh prova isso com erro de
//     COMPILACAO, rodando o predicado de verdade sobre cada linha;
//   - `chave` e o nome gravado no ajustes.txt (a CHAVE[] de ajustes.c); a
//     resolucao e por texto na hora da chamada, entao a tabela nao depende
//     das posicoes do enum, que mudam por #ifdef e por append-only;
//   - `nome` e o identificador estavel no pacote: [a-z0-9_.:+-], ate 39
//     caracteres, nunca a chave crua quando a semantica divergiria.
//
// Leitura por tipo:
//   BOOL: numero = 1 quando o indice gravado e o `ligado` (V_LIGA: 0).
//   NUM:  numero = num[indice] quando a linha tem tabela (ms, GB, horas);
//         sem tabela, o proprio indice gravado.
//   ENUM: token = rot[indice]; indice fora da tabela vira "outro" — nunca
//         texto livre e nunca o rotulo em portugues da tela.
#ifndef FAROL_AJUSTES_H
#define FAROL_AJUSTES_H

#include <stddef.h>   // NULL nas linhas sem tabela

typedef enum { FAROL_AJ_BOOL = 1, FAROL_AJ_NUM, FAROL_AJ_ENUM } FarolAjTipo;

typedef struct {
  const char *chave;            // nome gravado no ajustes.txt que a linha cobre
  const char *nome;             // nome estavel e seguro na secao "ajustes"
  unsigned char tipo;           // FAROL_AJ_*
  unsigned char ligado;         // BOOL: indice gravado que significa Ligado
  const char *const *rot;       // ENUM: token por indice gravado
  const int *num;               // NUM: valor por indice; NULL = o proprio indice
  unsigned char n;              // tamanho de rot/num
} FarolAjItem;

// V_QUALIDADE (indice gravado).
static const char *const FA_QUALIDADE[] = { "auto", "4k", "1080p", "720p" };
// V_FONTE_AUTO.
static const char *const FA_FONTE_AUTO[] = { "melhor", "primeira" };
// V_FONTE_TEXTO.
static const char *const FA_FONTE_TEXTO[] = { "nuvio", "addon", "logo" };
// V_FONTE_PRIORIDADE / V_FONTE_HDR / V_FONTE_ORDEM_USO / V_FONTE_ORDEM_ADDON.
static const char *const FA_FONTE_PRIO[] = { "equilibrio", "qualidade_max", "rapido", "addon" };
static const char *const FA_FONTE_HDR[]  = { "preferir", "indiferente", "evitar" };
static const char *const FA_ORDEM_USO[]  = { "nao", "desempate", "estrita" };
static const char *const FA_ORDEM[]      = { "qualidade", "addon" };
// Escopo do autoplay (streamAutoPlaySource).
static const char *const FA_ESCOPO[]     = { "todas", "instalados", "plugins" };
// V_DV_AUDIO: Original / Estereo (AAC) / 5.1 (Dolby Digital).
static const char *const FA_DV_AUDIO[]   = { "original", "aac", "ac3" };
// V_PLAYER_EXTERNO (num app real do Android vira "outro").
static const char *const FA_PLAYER_EXT[] = { "interno", "externo", "perguntar" };
// V_PROPORCAO: 0 = o ultimo modo usado; 1..8 = PlrAspecto + 1.
static const char *const FA_PROPORCAO[]  = { "ultimo", "original", "recortar",
  "esticar", "zoom_leve", "zoom_cinema", "zoom_ultra", "altura", "largura" };
// V_LIGA_AUTO / V_AUTO_LIGA: Ligado, Desligado, Automatico (vidro, ocultar).
static const char *const FA_LIGA_AUTO[]  = { "on", "off", "auto" };
// V_RESOLUCAO (RES_AUTO..RES_720).
static const char *const FA_RESOLUCAO[]  = { "auto", "1080p", "4k", "720p" };
// V_LEG_MOSTRAR.
static const char *const FA_LEG_MOSTRAR[] = { "idiomas", "todas" };
// V_DTS_SAIDA: a mesma linha da tela grava chaves e escalas distintas por
// plataforma (.tpk liga a conversao e escolhe a saida; LG so escolhe).
#ifdef NV_TPK
static const char *const FA_DTS[] = { "off", "ac3", "aac" };
#define FA_CHAVE_DTS "dtsTpkLocal"
#else
static const char *const FA_DTS[] = { "original", "aac", "ac3", "off" };
#define FA_CHAVE_DTS "dtsLgLocal"
#endif
// Tabelas indice->valor, identicas as de ajustes.c (NUM).
static const int FA_PRAZO_MS[]  = { 0, 3000, 5000, 8000, 15000, 30000, -1 };
static const int FA_TAMANHO_GB[]= { 0, 1, 2, 4, 8, 15, 30 };
static const int FA_LINK_H[]    = { 1, 2, 3, 6, 12, 24, 48, 72, 168 };
static const int FA_BUFFER_MB[] = { 0, 64, 128, 256 };
static const int FA_CONEXOES[]  = { 1, 2, 3, 4 };
static const int FA_MANTER_MIN[]= { 2, 5, 10, 30, 0 };   // 0 = sem limite
static const int FA_SEEK_MB[]   = { 0, 256, 512, 1024 };

#define FA_B(chave_, nome_)         { chave_, nome_, FAROL_AJ_BOOL, 0, NULL, NULL, 0 }
#define FA_N(chave_, nome_)         { chave_, nome_, FAROL_AJ_NUM, 0, NULL, NULL, 0 }
#define FA_NT(chave_, nome_, num_)  { chave_, nome_, FAROL_AJ_NUM, 0, NULL, num_, \
                                      (unsigned char)(sizeof num_ / sizeof *num_) }
#define FA_E(chave_, nome_, rot_)   { chave_, nome_, FAROL_AJ_ENUM, 0, rot_, NULL, \
                                      (unsigned char)(sizeof rot_ / sizeof *rot_) }

static const FarolAjItem FAROL_AJ_LISTA[] = {
  FA_E("qualidade",                     "qualidade_max",        FA_QUALIDADE),
  FA_B("dolbyVision",                   "dolby_vision"),
  FA_B("dolbyAtmos",                    "dolby_atmos"),
  FA_B("escolherFonteManual",           "fonte_manual"),
  FA_E("fonteAutoLocal",                "fonte_auto",           FA_FONTE_AUTO),
  FA_N("fonteReporLocal",               "fonte_repor"),
  FA_E("fonteTextoLocal",               "fonte_texto",          FA_FONTE_TEXTO),
  FA_NT("fonteEsperaLocal",              "fonte_espera_ms",      FA_PRAZO_MS),
  FA_E("streamAutoPlaySource",          "fonte_escopo",         FA_ESCOPO),
  FA_E("fontePrioridadeLocal",          "fonte_prioridade",     FA_FONTE_PRIO),
  FA_E("fonteHdrLocal",                 "fonte_hdr",            FA_FONTE_HDR),
  FA_B("fonteOutrosLocal",              "fonte_usar_outros"),
  FA_E("fonteOrdemUsoLocal",            "fonte_ordem_uso",      FA_ORDEM_USO),
  FA_E("fonteOrdemLocal",               "fonte_ordem",          FA_ORDEM),
  FA_B("fonteTocarConferindoLocal",     "fonte_conferindo"),
  FA_B("fontePrecarregarLocal",         "fonte_precarregar"),
  FA_B("fonteIdiomaAudioLocal",         "fonte_idioma_audio"),
  FA_B("fonteGrupoLocal",               "fonte_grupo"),
  FA_B("fonteGrupoSessoesLocal",        "fonte_grupo_sessoes"),
  FA_B("linkReusarLocal",               "link_reusar"),
  FA_NT("linkPrazoLocal",                "link_prazo_h",         FA_LINK_H),
  FA_E("fonteOcultarDvLocal",           "fonte_ocultar_dv",     FA_LIGA_AUTO),
  FA_E("fonteOcultarAudioLocal",        "fonte_ocultar_audio",  FA_LIGA_AUTO),
  FA_E("fonteOcultarVideoLocal",        "fonte_ocultar_video",  FA_LIGA_AUTO),
  FA_NT("tamanhoMaxLocal",               "tamanho_max_gb",       FA_TAMANHO_GB),
  FA_NT("tamanhoMinLocal",               "tamanho_min_gb",       FA_TAMANHO_GB),
  FA_E(FA_CHAVE_DTS,                    "dts",                  FA_DTS),
  FA_B("dvMkvLocal",                    "dv_mkv"),
  FA_E("dvAudioFixoLocal",              "dv_audio",             FA_DV_AUDIO),
  FA_B("dv7ConversionLocal",            "dv7"),
  FA_B("hdr10pStripLocal",              "hdr10p_strip"),
  FA_B("legendaSyncAutoLocal",          "legenda_auto_sync"),
  FA_B("legendaForcadaLocal",           "legenda_forcada"),
  FA_B("legendaEvitarSdhLocal",         "legenda_evitar_sdh"),
  FA_E("legendaMostrarLocal",           "legendas",             FA_LEG_MOSTRAR),
  FA_B("audioOriginalLocal",            "audio_original"),
  FA_B("audioNormalizarLocal",          "audio_normalizar"),
  FA_B("debridLigadoLocal",             "debrid_ligado"),
  FA_B("addonsLigadosLocal",            "addons_ligados"),
  FA_NT("bufferPlayerLocal",             "buffer_mb",            FA_BUFFER_MB),
  FA_NT("nativeBufferConnectionsLocal",  "buffer_conn",          FA_CONEXOES),
  FA_NT("conversionBufferConnectionsLocal", "buffer_conv_conn",  FA_CONEXOES),
  FA_B("bufferRamReduzLocal",           "buffer_ram_reduz"),
  FA_NT("cacheSeekLocal",                "cache_seek_mb",        FA_SEEK_MB),
  FA_B("manterVideoLocal",              "manter_video"),
  FA_NT("manterVideoPrazoLocal",         "manter_video_min",     FA_MANTER_MIN),
  FA_B("retomadaMornaLocal",            "retomada_morna"),
  FA_E("playerExternoLocal",            "player_externo",       FA_PLAYER_EXT),
  FA_E("proporcaoPadraoLocal",          "proporcao",            FA_PROPORCAO),
  FA_E("vidroLocal",                    "glass_ui",             FA_LIGA_AUTO),
  FA_E("resolucaoUi",                   "resolucao_ui",         FA_RESOLUCAO),
  FA_B("hdrUiLocal",                    "hdr_ui"),
  FA_B("envioAuto",                     "envio_auto"),
};

#undef FA_B
#undef FA_N
#undef FA_NT
#undef FA_E

#endif // FAROL_AJUSTES_H
