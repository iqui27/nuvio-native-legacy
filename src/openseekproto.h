// OPENSEEK (servidor proprio de miniaturas): a parte PURA do cliente. Sem rede,
// sem SDL, sem GL, sem fios — para os testes rodarem com `cc` sozinhos
// (tests/openseek*.sh). O contrato e docs/api/CONTRACT-v1.md do OpenSeek;
// o comportamento na tela, docs/design/SCRUBBING.md. O resto mora em openseek.c.
//
// SEGURANCA (SR-3/SR-4): a URL do video e os cabecalhos so saem no CORPO JSON
// de um POST com TLS fixado (pino). Nada daqui imprime coisa alguma; quem chama
// tambem nao pode imprimir URL, token, pino, codigo nem URL de capacidade.
#ifndef NV_OPENSEEKPROTO_H
#define NV_OPENSEEKPROTO_H
#include <stddef.h>
#include "seekrvtt.h"
#include "rede.h"

#define OS_PORTA_PADRAO 8787
#define OS_BASE_MAX 200
#define OS_TOKEN_MAX 64
#define OS_PINO_MAX 64            // "sha256//" + 44
#define OS_TILE_BYTES_MAX (4u * 1024u * 1024u)
#define OS_TILE_LADO_MAX 3200
#define OS_TILE_ALTO_MAX 1800
#define OS_PRAZO_API_MS 5000u     // status / prioridade / criar / parear
#define OS_PRAZO_DADOS_MS 10000u  // VTT, quadros, folhas

// Os mesmos numeros de SEEKR_* (seekr.h); openseek.c confere com _Static_assert.
enum {
  OS_SEEKR_DESLIGADO = 0, OS_SEEKR_BUSCANDO, OS_SEEKR_PRONTO, OS_SEEKR_SEM_PREVIA,
  OS_SEEKR_CHAVE_RECUSADA, OS_SEEKR_LIMITE_LOCAL, OS_SEEKR_LIMITE_PROVEDOR,
  OS_SEEKR_REDE_INDISPONIVEL
};
// Por que (so para o rotulo; o estado de cima decide o desenho).
enum {
  OSM_NENHUM = 0, OSM_REDE, OSM_PINO_MUDOU, OSM_SEM_PINO, OSM_RECUSADO,
  OSM_OCUPADO, OSM_FONTE, OSM_EXPIRADO, OSM_PREPARANDO, OSM_NAO_PAREADO,
  OSM_NAO_COMPARTILHADO, OSM_COMPARTILHAMENTO_OFF, OSM_CONTRIBUINDO, OSM_OCUPADO_CONTRIB
};
enum { OS_PREPARANDO = 1, OS_PRONTO, OS_COMPLETO, OS_FALHOU };

// "nas.local", "192.168.1.5:8787", "https://nas:8787/", "[fe80::1]:9000" ->
// "https://host:porta" (porta padrao 8787). http://, usuario@, caminho, query
// ou caractere fora de [A-Za-z0-9.-] no host: 0.
int  openseek_normalizar_endereco(const char *entrada, char *base, size_t n);
// 1 quando `url` e "<base>/..." (mesmo esquema, host e porta).
int  openseek_na_origem(const char *url, const char *base);
// Chave de cache de quadro/folha: a URL sem t=, sem exp= e sem fragmento. A
// mesma imagem com capacidade renovada (exp novo) ou pts diferente (t) tem a
// mesma chave: a troca de VTT nao perde texturas (SCRUBBING §4).
int  openseek_chave(const char *url, char *dst, size_t n);
// exp= (Unix s) da URL, 0 sem.
long long openseek_url_exp(const char *url);
// Largura/altura do SOF de um JPEG, ANTES de decodificar. 1 se achou.
int  openseek_jpeg_dimensoes(const unsigned char *b, size_t n, int *w, int *h);

typedef struct {
  const char *url;          // SEGREDO: a URL do video (video_url_atual)
  const char *cabecalhos;   // "Nome: valor\n" — so User-Agent/Referer/Origin saem
  long durMs;
  const char *imdb; int temporada, episodio;
  unsigned long long tamanho;
  const char *videoHash, *arquivo;
  long playheadMs, atMs;    // atMs < 0 = null
} OpenSeekFonte;
// Corpos JSON. Devolvem o tamanho (0 = nao coube ou entrada invalida).
size_t openseek_json_previa(char *dst, size_t n, const OpenSeekFonte *f);
// --- FONTE ALTERNATIVA PARA MINIATURAS (opcional; desligada por padrao) ---------
// So METADADOS dos candidatos vao no POST /v1/previews (bloco "alt"): nenhuma URL,
// nenhum cabecalho. O servidor pede UM candidato (alt.want); so entao a URL dele
// segue, uma vez, em POST /v1/previews/{id}/alt-source. O video em reproducao
// nunca e trocado. Contrato: docs/api/CONTRACT-v1.md do OpenSeek (alt).
#define OS_ALT_MAX 24
typedef struct {
  char cid[24];             // "s<indice na lista de fontes>"; opaco para o servidor
  char titleKey[64];        // "tt123:temporada:episodio": igualdade decide "mesmo titulo"
  int  altura;              // 2160, 1080, 720...
  long long durMs;          // 0 = desconhecida
  unsigned long long tamanho;
  const char *codec, *container, *nome;   // nome = release (so le palavras de edicao no servidor)
  int  emCache;             // debrid diz que ja esta pronto: nada a baixar
  int  resolvida;           // o cliente ja tem um link utilizavel
} OpenSeekAltCand;
// Corpo do POST de criacao + bloco "alt" (a base e a de openseek_json_previa).
size_t openseek_json_previa_alt(char *dst, size_t n, const OpenSeekFonte *f,
                                const OpenSeekAltCand *play, const OpenSeekAltCand *c, int nc);
// Corpo de POST /alt-source: o link do UNICO candidato pedido (mesma lista de cabecalhos permitidos).
size_t openseek_json_alt_fonte(char *dst, size_t n, const char *cid, const char *url, const char *cabecalhos);
// Corpo de POST /telemetry: saude da reproducao (bufferMs < 0 = desconhecido).
size_t openseek_json_telemetria(char *dst, size_t n, long bufferMs, int rebuffering, const char *host, int conns);
// Le o bloco "alt" da resposta de status. 1 se presente; want = cid pedido ("" = nada).
int  openseek_ler_alt(const char *json, char *estado, size_t nEstado, char *want, size_t nWant);
size_t openseek_json_prioridade(char *dst, size_t n, long atMs, long playheadMs);
size_t openseek_json_claim(char *dst, size_t n, const char *pairId,
                           const char *device, const char *prova);

typedef struct { char pairId[33], salt[33], pino[OS_PINO_MAX]; long iter; } OpenSeekStart;
// pair_id/salt 32 hex, spki_pin bem formado (rede_pino_valido), iterations.
int  openseek_ler_start(const char *json, OpenSeekStart *s);
typedef struct { char deviceId[17], token[OS_TOKEN_MAX], prova[65], papel[12]; } OpenSeekClaim;   // papel: owner | guest
int  openseek_ler_claim(const char *json, OpenSeekClaim *c);

typedef struct {
  char id[33];
  int  estado;              // OS_PREPARANDO..OS_FALHOU
  char erro[40];
  char vtt[2300];           // so na origem pareada
  long versao, refreshMs;
  long long exp;            // do vtt_url
} OpenSeekPrevia;
// Resposta 200/202 do POST /v1/previews ou do GET /v1/previews/{id}.
int  openseek_ler_previa(const char *json, const char *base, OpenSeekPrevia *p);
// {"vtt_version":N,"state":"..."} do /priority. 1 se leu a versao.
int  openseek_ler_versao(const char *json, long *versao, int *estado);

// --- BIBLIOTECA COMPARTILHADA (convidado) -------------------------------------------
// Um aparelho convidado NUNCA envia URL de video nem cria trabalho: ele pergunta ao servidor do dono
// se ha miniaturas de OUTRA versao do mesmo titulo (POST /v1/lookup, so ids + duracao) e le o VTT
// que o servidor ja tem. Toda correspondencia e APROXIMADA ate prova em contrario; o ajuste manual
// e a rejeicao valem so para este aparelho. Contrato: docs/design/OPENSEEK-2.md do OpenSeek.
enum { OS_PAPEL_DONO = 0, OS_PAPEL_CONVIDADO = 1 };
int  openseek_papel_de(const char *texto);           // "guest" -> convidado; qualquer outra coisa -> dono
// O que a pessoa colou/digitou ao entrar numa biblioteca. forma: 0 invalido, 1 codigo de 8 (janela do dono),
// 2 convite (string openseek://join/1?... ou codigo digitado de 16).
typedef struct {
  int  forma;
  char base[OS_BASE_MAX];       // "https://host:porta" quando a string de convite traz h=; senao ""
  char pino[OS_PINO_MAX];       // p=, quando traz
  char convite[40];             // i= (id de 32 hex) ou a etiqueta de 4 letras do codigo digitado
  char segredo[16];             // o segredo de 12 (ou o codigo de 8 da janela do dono)
} OpenSeekEntrada;
int  openseek_ler_entrada(const char *texto, OpenSeekEntrada *e);
// Corpo do POST /v1/pair/start: {} (janela do dono) ou {"invite":"<id|etiqueta>"}.
size_t openseek_json_start(char *dst, size_t n, const char *convite);
// Corpo do POST /v1/lookup. imdb "tt..." obrigatorio (0 sem ele: nada a procurar). tamanho 0 = omitido.
size_t openseek_json_procura(char *dst, size_t n, const char *imdb, int temporada, int episodio,
                             long durMs, unsigned long long tamanho);
// Corpo de PUT /v1/previews/{id}/offset.
size_t openseek_json_offset(char *dst, size_t n, long offsetMs);
typedef struct {
  char id[33];
  char vtt[2300];           // so na origem pareada
  long long exp;
  char tipo[24];            // same_length | speed_adjusted | length_differs | user_adjusted
  int  aproximado;          // SEMPRE 1 ate o servidor provar o contrario ("verified")
  int  verificado;
  double a, b;              // t_cache = a * t_convidado + b (ms)
  long fonteMs, tiles, nTiles;
  int  nCandidatos;
} OpenSeekCompart;
// Resposta 200 do POST /v1/lookup: o primeiro candidato. `aproximado` so e 0 quando verified=true E approximate=false.
int  openseek_ler_procura(const char *json, const char *base, OpenSeekCompart *c);
// Legenda curta, honesta, para a tela (nunca chama de "verificada" o que e so metadado).
const char *openseek_rotulo_compart(const OpenSeekCompart *c);
// CONTRIBUINTE (guest com permissao do dono): quando o titulo NAO esta na biblioteca, o aparelho entrega o link do video
// ao servidor do dono (POST /v1/contribute, TLS fixado) para que as miniaturas sejam geradas uma vez para todos. Corpo:
// source{url,headers (so User-Agent/Referer/Origin)}, imdb_id, season, episode, duration_ms. 0 se faltar id tt ou duracao.
size_t openseek_json_contribuir(char *dst, size_t n, const OpenSeekFonte *f);
// 1 quando GET /v1/pair/status diz que ESTE aparelho pode contribuir agora ("contribute":true).
int  openseek_ler_pode_contribuir(const char *json);
// Novo valor de b (ms) ao deslocar as miniaturas `deslocaMs` para MAIS TARDE na linha do tempo do convidado
// (positivo = depois). Limitado a +-6 h.
long openseek_offset_apos(double b, long deslocaMs);

// Resultado de um pedido de API -> estado SEEKR_* (OS_SEEKR_*) e motivo.
// -1 quando nao muda nada (cancelado/obsoleto). `p` so com 200/202 lido.
int  openseek_mapear(int http, int redeErro, int curlErro, const OpenSeekPrevia *p,
                     int nCues, int *motivo);

// --- Pedido de rede ----------------------------------------------------------
// Monta TODO pedido do OpenSeek num lugar so (testado): https, pino
// obrigatorio (exceto o primeiro contato /v1/pair/start, sem segredo nenhum),
// seguir = 0, tls_pessoal (certificado autoassinado; o pino continua valendo),
// prazo curto. `cab` recebe "Authorization: Bearer ..." quando ha token.
typedef struct {
  char url[2400];
  char auth[96];
  const char *cabs[3];
  RedePedido p;
} OpenSeekPedido;
// metodo NULL = GET. pino NULL/"" so com semPino = 1 (pair/start). 0 = recusado.
int  openseek_pedido(OpenSeekPedido *o, const char *metodo, const char *url,
                     const char *token, const char *pino, int semPino,
                     const void *corpo, size_t nCorpo, unsigned prazoMs);

// --- Cache LRU por chave (texturas e JPEGs) ----------------------------------
#define OS_LRU_MAX 1024
typedef struct { char chave[240]; unsigned uso; int ocupado; } OsLruItem;
typedef struct { OsLruItem it[OS_LRU_MAX]; int n; unsigned relogio; } OsLru;
void openseek_lru_ini(OsLru *l, int n);           // n <= OS_LRU_MAX
int  openseek_lru_tem(const OsLru *l, const char *chave); // indice ou -1 (sem marcar uso)
int  openseek_lru_achar(OsLru *l, const char *chave);     // indice ou -1 (marca uso)
// Indice para guardar `chave` (o livre ou o menos usado fora de `proteger`).
int  openseek_lru_lugar(OsLru *l, const char *chave, const char *const *proteger, int np);

// Tolerancia maxima (ms) para exibir uma cue vizinha da grade esparsa (60s/30s)
// enquanto o quadro exato (±5s) ainda esta sendo refinado: cobre metade do
// passo de 60s (30s) + ±5s de jitter de keyframe. Acima disto devolve -1.
#define OS_SPARSE_TOL_MS 35000L

// Indice da cue visivel para `posMs`: prefere a cue estrita [ini, fim)
// (*esparso = 0); nos buracos da grade hierarquica (60s -> 30s -> 10s), escolhe
// a cue com rotulo/centro mais proximo de `posMs` ate OS_SPARSE_TOL_MS
// (*esparso = 1). Fora desse alcance devolve -1 (*esparso = 0).
int  openseek_cue_visivel(const SeekrVtt *v, long posMs, int *esparso);

// Fila priorizada de indices de cues para busca/pre-recorte: a cue visivel em
// `posMs` e vizinhas na direcao `dir` (+1/-1), seguidas da cue visivel em
// `alvoMs` (quando o avanco visual ainda alcanca o alvo) e lookahead direcional.
int  openseek_fila_busca(const SeekrVtt *v, long posMs, long alvoMs, int dir,
                         int maxOut, int *out);

// Escolhe a proxima folha (`0 .. v->nFolhas - 1`) ainda fora de `jl` para
// prefetch em segundo plano, ordenada pela proximidade a `anchorMs` com vies
// para a direcao `dir` (+1/-1). Devolve -1 quando todas as folhas validas ja
// estao em cache.
int  openseek_escolher_prefetch(const SeekrVtt *v, const char (*folhaChaves)[240],
                                const OsLru *jl, const char (*falhas)[240], int nFalhas,
                                long anchorMs, int dir);
#endif
