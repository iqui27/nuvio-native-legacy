// FAROLCOMPOR — compositor remoto FarolCena v1 (PLANO-GOSTO §5.3, §5.4, issue 52).
//
// Consulta POST /v1/farol/compor em fio proprio (prazo rigido de 6 s, cancelavel
// em < 300 ms, sem SDL nem _Thread_local), valida a resposta contra o contrato
// estrito FarolCena v1 (`farolpeca_ler` <= 8 KB, vinculacao `c<n>`->candidato e
// resolucao de referencias), aplica privacidade/supressao e ADOTA somente no fio
// principal (`farolcompor_passo()`), com cache por perfil de 24 h em
// `farol-cena-p<n>.json` e no maximo 1 chamada Home por dia local por perfil.
//
// PRIVACIDADE E SUPRESSAO:
//   - Suprime pedido e adocao quando: player aberto, perfil infantil, falta
//     consentimento (allowlist #53), modo seguro ou "Recomendar" desligado.
//   - Corpo HTTP <= 12288 B, estritamente limitado ao schema de /v1/farol/compor:
//     v, modo, idioma, hora (balde grosseiro 0..7), gosto (<=6 generos, <=6 temas,
//     <=4 pessoas com ids publicos TMDB > 0 e nomes, <=3 ancoras publicas),
//     <=24 candidatos (id/tipo/titulo/ano/generos/minutos/motivos estruturados
//     com motivo de amigo SEMPRE anonimizado), fatos opcionais (incluindo frase
//     ditada sanitizada quando aplicavel), infantil e consentiu.
//   - Credenciais (que podem renovar JWT por HTTP) sao resolvidas DENTRO do
//     fio do job, ja registrado e cancelavel, no mesmo prazo de 6 s; logo
//     antes do POST os bloqueios sao conferidos de novo (job vivo, mesma
//     geracao/perfil/sessao/conta, player fechado, nao infantil, consentimento)
//     e qualquer mudanca aborta com zero bytes enviados.
//   - NUNCA envia Diario cru, notas/datas, nome ou ID de perfil, amigos,
//     addons/fontes ou vetor completo de gosto.
//   - Falha de rede, offline, 429, 5xx, timeout, JSON invalido, referencia nao
//     resolvida ou {"origem":"regras"} preservam a cena deterministica calado.
#ifndef NV_FAROLCOMPOR_H
#define NV_FAROLCOMPOR_H

#include <stddef.h>
#include <stdint.h>
#include "farolgosto.h"
#include "farolpeca.h"

#define FAROL_COMPOR_PRAZO_MS    6000u
#define FAROL_COMPOR_CORPO_MAX   12288u
#define FAROL_COMPOR_CACHE_TTL_S 86400LL
#define FAROL_COMPOR_GEN_MAX     6
#define FAROL_COMPOR_TEMA_MAX    6
#define FAROL_COMPOR_PESSOA_MAX  4
#define FAROL_COMPOR_ANCORA_MAX  3
#define FAROL_COMPOR_CAND_MAX    24
#define FAROL_COMPOR_MOT_MAX     4
#define FAROL_COMPOR_FATOS_MAX   8
#define FAROL_COMPOR_FRASE_MAX   120

typedef enum {
  FC_MODO_HOME = 0,
  FC_MODO_COMPARAR,
  FC_MODO_DICA,
  FC_MODO_RETOMADA,
  FC_MODO_N
} FarolComporModo;

typedef enum {
  FC_OCIOSO = 0,
  FC_CARREGANDO,
  FC_PRONTO,
  FC_REGRAS,
  FC_CANCELADO,
  FC_SUPRIMIDO
} FarolComporEstado;

typedef enum {
  FC_MOT_NENHUM = 0,
  FC_MOT_OK,
  FC_MOT_CACHE,
  FC_MOT_REGRAS,
  FC_MOT_OFFLINE,
  FC_MOT_PRAZO,
  FC_MOT_LIMITE,
  FC_MOT_ERRO,
  FC_MOT_CANCELADO,
  FC_MOT_SUPRIMIDO
} FarolComporMotivo;

typedef struct {
  int32_t id;        // somente id publico TMDB (> 0) vai ao fio
  int16_t peso;      // Q8.8
  char    nome[64];  // opcional, nome publico sanitizado
} FarolComporAfin;

typedef struct {
  char id[20];       // "tt<digitos>"
  char titulo[100];  // titulo publico
} FarolComporAncora;

typedef struct {
  uint8_t tipo;      // FMO_* (farolrec.h); FMO_AMIGO/FMO_RECOMENDADO sempre anonimizados
  int32_t a;
  int32_t b;
} FarolComporMotivoItem;

typedef struct {
  char    imdb[24];  // "tt<digitos>" ou ""
  int32_t tmdb;      // > 0 ou 0
  uint8_t tipo;      // 0 = movie, 1 = series
  char    titulo[128];
  int16_t ano;       // 1880..2100 (0 = omite)
  int16_t minutos;   // 1..2000 (0 = omite)
  int32_t generos[8];
  int     nGeneros;  // somente ids publicos TMDB (> 0) sao serializados
  int16_t nota10;    // 0..100 -> 0.0..10.0 (< 0 = omite)
  int32_t pontos;    // Q15 opcional (0 = omite)
  FarolComporMotivoItem motivos[FAROL_COMPOR_MOT_MAX];
  int     nMotivos;
  int16_t faltam;    // > 0 opcional (comparar/retomada)
  int16_t minNoite;  // > 0 opcional
  int8_t  cabeHoje;  // 1 = sim, -1 = nao, 0 = omite
} FarolComporCand;

typedef struct {
  char    chave[32];
  char    valor[160];
  int32_t inteiro;
  int     temInteiro;
  char    alvo[20];  // "c<n>" ou "tt<digitos>" opcional
} FarolComporFato;

typedef struct {
  FarolComporModo            modo;
  const char                *idioma;     // ex: "pt-BR" (NULL -> ajustes_tmdb_idioma)
  int                        hora;       // balde grosseiro 0..7 (< 0 -> calcula local)
  int                        fimSemana;  // 0 | 1 (< 0 -> calcula local)
  int                        nivel;      // 0..2
  int                        infantil;   // 0 | 1
  int                        consentiu;  // 0 | 1
  uint32_t                   hModelo;    // hash do modelo na coleta
  const FarolComporAfin     *generos;    int nGeneros;
  const FarolComporAfin     *temas;      int nTemas;
  const FarolComporAfin     *pessoas;    int nPessoas;
  const FarolComporAncora   *ancoras;    int nAncoras;
  const FarolComporCand     *candidatos; int nCandidatos;
  const FarolComporFato     *fatos;      int nFatos;
  const char                *frase;      // frase ditada opcional quando aplicavel
  // modo retomada (issue 57): o episodio JA VISTO (entrada real no Diario) e
  // a sinopse DAQUELE episodio. Fora do modo retomada nunca vai ao corpo; no
  // modo retomada com algum deles preenchido e os tres nao validos, o corpo nao
  // e montado (zero POST). Os tres zerados: pedido de cena da #52, sem objeto.
  int                        retTemporada; // >= 1
  int                        retEpisodio;  // >= 1
  const char                *retSinopse;   // CatEp.sinopse do (t, e) visto
} FarolComporPedido;

#define FAROL_COMPOR_SINOPSE_MAX 400u  // bytes UTF-8 da sinopse no corpo

typedef struct {
  FarolComporEstado estado;
  FarolComporMotivo motivo;
  FarolComporModo   modo;
  int               http;
  int               perfil;
  int               doCache;
  int               preservaRegra;
  uint64_t          geracao;
  uint64_t          sessao;
} FarolComporInfo;

// Costura de allowlist e tabela unica de campos de consentimento (#53).
typedef int (*FarolComporAllowlistFn)(int perfil, void *usuario);

void farolcompor_definir_consentimento(int consentiu);
void farolcompor_definir_allowlist(FarolComporAllowlistFn fn, void *usuario);
int  farolcompor_consentimento(void);

// Funcoes puras (serializacao, extracao do gosto e validacao da resposta):
size_t farolcompor_montar_corpo(const FarolComporPedido *ped, char *dst, size_t cap);
// Igual a farolcompor_montar_corpo, e devolve o mapa EXATO do corpo:
// mapaOut[k] = indice em ped->candidatos do candidato escrito como "c<k>"
// (sanitizacao e teto de 12 KB descartam candidatos e compactam os cN).
// `gosto.ancoras` so leva ancora citada por FMO_TERMINOU de candidato
// efetivamente escrito. mapaOut precisa de FAROL_COMPOR_CAND_MAX posicoes.
size_t farolcompor_montar_corpo_mapa(const FarolComporPedido *ped, char *dst, size_t cap,
                                     int *mapaOut, int *nMapaOut);
int farolcompor_validar_resposta(FarolComporModo modo, const char *json, size_t len,
                                 const FarolComporCand *cand, int nCand,
                                 FarolCena *cenaOut, int *ehRegrasOut);
int farolcompor_extrair_gosto(const FarolGosto *g, uint16_t dia,
                              const char *(*nomePessoaFn)(int32_t, void *),
                              const char *(*nomeTemaFn)(int32_t, void *),
                              void *u,
                              FarolComporAfin *genOut, int *nGen,
                              FarolComporAfin *temaOut, int *nTema,
                              FarolComporAfin *pesOut, int *nPes);

// Disparo assincrono em fio proprio (prazo 6 s, cancelavel).
// Devolve 1 se iniciou pedido (ou carregou cache valido de 24 h na Home),
// 0 se suprimido / invalido / bloqueado por cota diaria Home.
int farolcompor_pedir(const FarolComporPedido *ped);
int farolcompor_pedir_home(const FarolGosto *g,
                           const FarolComporCand *cand, int nCand,
                           const FarolComporAncora *ancoras, int nAncoras,
                           const char *(*nomePessoaFn)(int32_t, void *),
                           void *u, uint32_t hModelo);

// Adocao segura SOMENTE no fio principal (src/app.c):
// valida contexto/perfil/sessao/player/infantil/consentimento/visibilidade,
// grava `farol-cena-p<n>.json` e promove a cena para consumo da Home/UI.
// Devolve 1 quando uma cena nova foi adotada neste passo.
int  farolcompor_passo(void);
void farolcompor_cancelar(void);
void farolcompor_encerrar(void);
void farolcompor_trocar_perfil(void);
void farolcompor_esquecer(void);
void farolcompor_zerar_gosto(void);
int  farolcompor_carregar_cache_home(void);

// RETOMADA SEM SPOILER (issue 57). Pede o resumo de 1 linha do episodio JA
// VISTO (t, e) da serie `imdb`: quem chama garante que (t, e) e o que
// diario_ultimo_episodio devolveu e que `sinopse` e a daquele episodio
// (farolinfo_retomada_sinopse). Os bloqueios da #52 valem todos (player,
// infantil, consentimento, seguro, Recomendar, offline, prazo de 6 s). Nao
// derruba pedido em voo (a Home tem prioridade) e nao repete o mesmo
// (perfil, serie, t, e) na sessao. 1 = pedido iniciado.
int  farolcompor_pedir_retomada(const char *imdb, const char *titulo,
                                int temporada, int episodio,
                                const char *sinopse);
// O resumo adotado para (imdb, t, e): 1 linha validada (farolinfo_retomada_
// linha_ok + faroltexto_aceita), alvo e (t, e) da resposta iguais aos do
// pedido. 0 = nao ha (fica so a linha deterministica de tempo).
int  farolcompor_retomada_resumo(const char *imdb, int temporada, int episodio,
                                 char *dst, size_t cap);

// Leitura segura sob trava (sem I/O nem bloqueio):
FarolComporEstado farolcompor_estado(FarolComporInfo *info);
int      farolcompor_em_voo(void);
unsigned farolcompor_home_geracao(void);
int farolcompor_cena_adotada(FarolCena *cenaOut, FarolComporCand *candOut,
                             int *nCandOut, int maxCand, uint32_t *hModeloOut);
int farolcompor_cena_modo(FarolComporModo modo, FarolCena *cenaOut,
                          FarolComporCand *candOut, int *nCandOut, int maxCand,
                          uint64_t *geracaoOut);

// Ganchos de teste / integracao (NULL volta ao padrao do app):
void farolcompor_definir_url(const char *baseUrl);
void farolcompor_definir_auth(const char *autorizacao, const char *via,
                              const char *perfilCab);
void farolcompor_relogio(long long (*s)(void));
void farolcompor_pasta(const char *dir);
void farolcompor_teste_home_visivel(int (*v)(void));
void farolcompor_teste_zera(void);

#endif // NV_FAROLCOMPOR_H
