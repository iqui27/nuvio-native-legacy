// Busca HTTP(S) simples, para memoria.
//
// Usa a libcurl DO APARELHO por dlopen. Escrever TLS a mao estava fora de
// cogitacao e o SDK nao traz libcurl para linkar — mas /usr/lib/libcurl.so.5
// existe na TV, e os addons so falam https. No Mac usa a libcurl do sistema.
#ifndef NV_REDE_H
#define NV_REDE_H

#include <stddef.h>
#include <stdint.h>

/* API aditiva para workers/plugins. Nao usa o estado por fio dos wrappers.
 * O pedido BLOQUEIA; executa-lo fora do desenho. Strings, corpo e callback
 * ficam validos ate retornar. rede_resposta_limpar libera as duas alocacoes.
 * max_bytes/max_cabecalhos zero usam limites seguros, nunca "ilimitado".
 * TLS e verificado; ca_arquivo sobrescreve o bundle confiavel registrado no
 * arranque (rede_discord_ca), ou usa trust do sistema se nao houver bundle.
 * Redirects so HTTP(S), sem downgrade HTTPS. Cabecalhos do dono e corpo nao
 * atravessam origem; 307/308 autenticados entre origens sao recusados.
 * O XHR sincrono WGT nao oferece este contrato: retorna INDISPONIVEL sem
 * iniciar I/O, exceto GET publico HTTPS com receber_corpo, sem redirects,
 * cabecalhos, job ou intervalo (fetch em fluxo). Os wrappers antigos continuam
 * disponiveis com seus limites.
 */
typedef struct RedeGrupo RedeGrupo;
typedef struct RedeJob RedeJob;
typedef enum {
  REDE_OK = 0, REDE_ENTRADA, REDE_INDISPONIVEL, REDE_MEMORIA,
  REDE_TRANSPORTE, REDE_PRAZO, REDE_CANCELADO, REDE_GERACAO,
  REDE_LIMITE_CORPO, REDE_LIMITE_CABECALHOS, REDE_REDIRECT
} RedeErro;
#define REDE_CORPO_PADRAO (8u * 1024u * 1024u)
#define REDE_CORPO_MAXIMO (32u * 1024u * 1024u)
#define REDE_CAB_PADRAO 16384u
#define REDE_CAB_MAXIMO 65536u
#define REDE_CAP_CORPO 1u
#define REDE_CAP_JOB 2u
#define REDE_CAP_REDIRECT 4u
#define REDE_CAP_INTERVALO 8u

typedef struct {
  uint64_t bytes;       /* payload no fio, antes da descompressao */
  unsigned ms;         /* duracao real; nunca inventa janelas de 1 s */
  unsigned inicio_ms;  /* relativo ao primeiro byte do corpo */
  int completa;        /* >=1 s; amostra final curta vale 0 */
} RedeIntervalo;
typedef struct {
  const char *metodo;   /* NULL = GET; GET/HEAD/POST/PUT/PATCH/DELETE */
  const char *url;
  const char *const *cabecalhos;
  const void *corpo;
  size_t n_corpo;       /* binario, nao strlen */
  unsigned prazo_ms;    /* total, inclusive redirects; 0 = 15 s, max 300 s */
  size_t max_bytes, max_cabecalhos;
  int seguir;          /* 0 = devolver 3xx; 1 = ate 5 redirects */
  const char *ca_arquivo;
  /* #290: 1 = servidor PESSOAL configurado pela pessoa (Jellyfin/Emby/Plex),
   * que costuma ter certificado autoassinado ou de rede local. O certificado
   * deixa de ser verificado SO enquanto o pedido estiver na MESMA origem
   * (esquema+host+porta) de `url`; um redirect para outra origem volta a
   * verificar. Todo o resto do app continua verificando. */
  int tls_pessoal;
  RedeJob *job;
  void (*intervalo)(const RedeIntervalo *, void *);
  void *intervalo_usuario;
  /* Optional streamed/discarded GET: keeps only a 512-byte prefix. Stops at
   * window or cap, never allocates the media body. A byte cap does not prove
   * a completed time window. Zero keeps the normal buffered contract. */
  unsigned janela_corpo_ms;  /* from first body byte; max 60 s */
  uint64_t max_descartado;    /* required with window; max 2 GiB */
  int (*parar)(void *);       /* additional caller cancellation, no UI work */
  void *parar_usuario;
  /* Decoded body chunks, without allocating corpo; 0 aborts the request.
   * Only final HTTP 2xx chunks reach this callback; max_bytes still applies. */
  int (*receber_corpo)(const void *, size_t, void *);
  void *corpo_usuario;
  /* 1 = media Range read: follow an https->http redirect like the player does
   * (debrid links). Cross-origin hops already drop every caller header. */
  int rebaixar_midia;
} RedePedido;
typedef struct {
  int status, curl_erro;
  RedeErro erro;        /* HTTP 4xx/5xx e uma resposta, nao erro de transporte */
  char *corpo, *cabecalhos;
  size_t n_corpo, n_cabecalhos;
  char final[4096], host[256], mime[128]; /* final pode ser privado: nunca logar */
  int retry_after_s;
  unsigned ms, primeiro_byte_ms, corpo_ms, intervalos_completos;
  uint64_t bytes_fio;   /* separado de n_corpo (descomprimido) */
  unsigned char prefixo[512];
  unsigned n_prefixo;
  int fim_janela, fim_teto; /* intentional discard stops, not transport EOF */
} RedeResposta;
unsigned rede_pedido_capacidades(void);
int rede_pedir(const RedePedido *pedido, RedeResposta *resposta);
void rede_resposta_limpar(RedeResposta *resposta);

/* Grupo vive fora do request: avancar ao trocar conta/perfil/titulo invalida
 * todos os jobs anteriores. Jobs retidos impedem UAF; nenhuma geracao volta
 * a ser valida. Cancelar um job nao cancela seus irmaos. Soltar o grupo do
 * dono nao o cancela; cancelar explicitamente no encerramento. O chamador
 * deve conferir rede_job_estado tambem ao publicar numa fila da UI: mudar
 * a geracao DEPOIS do retorno nao consegue retirar um resultado ja entregue.
 * Sem scheduler/pool: integra os workers atuais. Sem TLS de compilador.
 */
RedeGrupo *rede_grupo_criar(void);
void rede_grupo_soltar(RedeGrupo *grupo);
uint64_t rede_grupo_avancar(RedeGrupo *grupo);
void rede_grupo_cancelar(RedeGrupo *grupo);
RedeJob *rede_job_criar(RedeGrupo *grupo);
void rede_job_reter(RedeJob *job);
void rede_job_soltar(RedeJob *job);
void rede_job_cancelar(RedeJob *job);
RedeErro rede_job_estado(RedeJob *job);

/* Bounded, validated byte ranges for the local DTS demuxer. Unlike the old
 * long-offset API this works past 2 GiB on ARM. Only matching HTTP 206 bodies
 * are accepted; total is -1 when unknown. Caller frees the returned buffer.
 * Raw headers use one Name: value per line. Credentials are stripped when a
 * redirect changes origin. Cancelling also interrupts an idle transfer. */
/* cross: 0 = same origin, 1 = public headers only, 2 = no caller headers
 * after an HTTPS downgrade (including subsequent HTTPS hops). */
char *rede_range_headers(const char *raw, int public_only); /* caller frees */
char *rede_baixar_trecho64_final(const char *url, const char *headers,
                              int64_t start, int64_t end, long *size,
                              int64_t *total, int *status,
                              volatile int *cancelled, char *final, int *cross);
/* Optional guard for an actively awaited range. Atomic urgent flag is owned
 * by the caller for this synchronous call. Last retry can omit this guard. */
char *rede_baixar_trecho64_vigiado(const char *url,const char *headers,
    int64_t start,int64_t end,long *size,int64_t *total,int *status,
    volatile int *cancelled,char *final,int *cross,volatile int *urgent,
    unsigned idle_ms,int *stalled);
/* One streaming read "bytes=start-" for the Samsung converter: the body goes to
 * sink(opaque, data, n) as it arrives (return 0 to stop; it may block). Same
 * redirect and header rules as rede_baixar_trecho64_final. No limit on the
 * whole transfer; it aborts after stall_ms without a byte (time inside the sink
 * does not count). Returns 0 when the body ended, 1 when the sink stopped it,
 * -1 on error (*status = HTTP code or 0, *bytes = body bytes delivered). */
int rede_fluxo64(const char *url, const char *headers, int64_t start,
                 int (*sink)(void *opaque, const char *data, size_t n), void *opaque,
                 int64_t *total, int *status, volatile int *cancelled,
                 char *final, int *cross, long stall_ms, int64_t *bytes);
char *rede_baixar_trecho64_cab(const char *url, const char *headers,
                              int64_t start, int64_t end, long *size,
                              int64_t *total, int *status,
                              volatile int *cancelled);

typedef struct {
  int status;          // 0 = transporte sem resposta
  long bytes;          // corpo recebido; -1 quando nao medido
  unsigned long ms;    // tempo total da requisicao
  int limitado;        // 1 = o teto por requisicao foi atingido
  int cancelado;       // 1 = o cancelamento interrompeu a transferencia
} RedeMedida;

typedef struct {
  long max_bytes;             // 0 = sem teto adicional
  volatile int *cancelado;    // opcional; lido durante o recebimento
} RedeControle;

// Baixa `url` inteiro para um buffer novo (terminado em NUL) e devolve-o; o
// chamador libera com free(). NULL em qualquer falha. BLOQUEIA — chamar de um
// fio proprio, nunca do laco de desenho.
char *rede_baixar(const char *url, int segundos);

// Igual, mas para conteudo BINARIO: devolve o tamanho em *n. A versao acima
// termina em NUL e serve para JSON; imagem tem zeros no meio e strlen mentiria.
char *rede_baixar_bin(const char *url, int segundos, long *n);
// GET EM FLUXO: o corpo vai em pedacos para `cb` (que devolve 0 para cortar a
// transferencia de proposito, o que conta como sucesso) e nada fica na memoria.
// `maxBytes` 0 = sem teto. 1 = terminou (2xx); 0 = falha (*status = HTTP, 0 sem
// resposta). BLOQUEIA. Usado pelo XMLTV e pela M3U do guia de TV.
int rede_baixar_fluxo(const char *url, int segundos, const char *const *cab,
                      int (*cb)(const char *dados, size_t n, void *u), void *u,
                      long long maxBytes, int *status, long long *bytes);


#ifdef __EMSCRIPTEN__
// POST de corpo text/plain com resposta BINARIA (tamanho em *n; 4xx/5xx viram
// NULL, como em rede_baixar_bin). So existe no Tizen, para o proxy do Xtream
// (#112): a url do icone vai no CORPO, que a plataforma do worker nao
// registra, e volta uma imagem com zeros no meio — rede_postar_st nao da o
// tamanho. Ver tex_cache.c e servidor/recomendacoes/src/xtream.js.
char *rede_postar_bin(const char *url, int segundos, const char *corpo, long *n);
#endif

// Com cabecalhos. `cabecalhos` e um vetor terminado em NULL de linhas prontas
// ("Authorization: Bearer x"). Existe por causa do Trakt, que exige token e
// chave de aplicativo em cabecalho — nao ha como passar por URL.
char *rede_baixar_com(const char *url, int segundos, const char *const *cabecalhos);

// Baixa SO UM TRECHO, por cabecalho Range. Devolve o tamanho em *tam.
//
// Existe para ler o cabecalho de um MKV sem puxar o arquivo inteiro: o
// pipeline da LG devolve "language":"(null)" em TODA faixa de legenda (medido
// num arquivo de 43 legendas — o audio vem com idioma, a legenda nao), e a
// unica forma de saber o idioma e ler o proprio container, que e o que o
// navegador faz por conta propria no app web.
//
// Servidor que ignora o Range devolve o arquivo inteiro; por isso quem chama
// tem de estar preparado para receber MAIS do que pediu, e parar de ler quando
// achou o que queria.
char *rede_baixar_trecho(const char *url, int segundos, long ini, long fim,
                         long *tam);

// O mesmo, dizendo POR QUE falhou (#92). `*status` recebe o codigo HTTP da
// resposta final, depois dos redirecionamentos (0 = nenhuma resposta), e
// `*erro` o codigo da libcurl (28 = prazo, 7 = conexao recusada; 0 no Tizen,
// que nao tem libcurl). O corpo so volta em 2xx: um 403/429/416 e NULL como
// antes, mas quem chama sabe separar "o CDN recusou esta conexao" de "a rede
// caiu" — o mkvass trata um como freio e o outro como falha passageira.
// `final` (opcional, `tamFinal` bytes) recebe o endereco depois dos
// redirecionamentos — o mesmo que rede_url_final daria, sem pedido a mais.
// Qualquer ponteiro pode ser NULL.
//
// CORPO CORTADO (#92): um 206 que fecha antes do fim (curl 18) nao e mais
// falha — o que veio fica e o resto e pedido do byte seguinte, ate completar
// ou um pedaco nao trazer nada (ai NULL, com o codigo). O host que corta ganha
// um teto de pedido lembrado a sessao inteira (rede_corte_host), e os trechos
// seguintes ja saem em pedacos desse tamanho. `segundos` e o prazo do trecho
// INTEIRO, nao de cada pedaco. rede_baixar_trecho passa pelo mesmo laco.
// `ini`/`fim` em 64 bits: Range alem de 2 GiB num ARM de 32 bits (#269).
char *rede_baixar_trecho_st(const char *url, int segundos, long long ini, long long fim,
                            long *tam, int *status, int *erro,
                            char *final, unsigned tamFinal);

// Teto de pedido aprendido para o host de `url` (esquema, host e porta), em
// bytes; 0 = o host nunca cortou um Range nesta sessao. Um host que corta e
// tambem o candidato a derrubar conexao a mais: o mkvass passa a uma so.
long rede_corte_host(const char *url);

// 1 quando o ULTIMO rede_baixar_trecho_st DESTE FIO falhou porque um pedaco
// seguinte (o resto, depois de um corte ou de um pedaco do teto) voltou com
// zero bytes e sem estourar o prazo: o servidor recusou continuar (#92, v1.4.7:
// o Real-Debrid, com o video tocando). Ler logo depois da chamada, no mesmo fio.
int rede_resto_recusado(void);

// LEITURA LATERAL AO VIDEO (#385). O fio que chama rede_lateral(1) e uma
// leitura AO LADO do player no mesmo arquivo (pre-busca e colheita do mkvass,
// sonda do MKV, capitulos): nunca o proprio video nem o proxy dele. Nesses
// fios um Range cuja CONEXAO falha (curl 6, 7 ou 35: o no do CDN recusou,
// sumiu ou derrubou o aperto de mao) nao repete "em conexao nova", e o host
// entra em PAUSA (10 s, dobrando ate 60 s, como o #308): ate ela vencer,
// todo Range lateral a ele volta na hora com o mesmo codigo, sem abrir
// conexao. O primeiro Range lateral que da certo solta a pausa. Vale para o
// fio inteiro: chamar no comeco de um fio proprio.
void rede_lateral(int sim);

// Teto de bytes da transferencia corrente (0 = sem teto). E interno ao modulo;
// esta exposto so porque rede_baixar_trecho o usa. Nao mexer de fora.
//
// NV_TPK40 (libnuvio do pacote Tizen 4/5, tools/tpk.sh): SEM _Thread_local.
// Nessas TVs a .so pode entrar pelo carregador de ELF proprio do host
// (Program40.cs), que nao monta TLS de compilador — o primeiro acesso a uma
// variavel _Thread_local seria o fim do processo. O estado por fio vive numa
// struct por pthread_key (rede.c); o nome continua um lvalue por macro.
#if defined(NV_TPK40) || defined(NV_UM_FIO)
long *rede_teto_ptr(void);
#define rede_teto (*rede_teto_ptr())
#elif defined(__GNUC__)
extern _Thread_local long rede_teto;
#else
extern long rede_teto;
#endif

// Segue os redirecionamentos com GET Range de 64 bytes e devolve o endereco
// FINAL somente em HTTP2xx, sem truncar o destino. Nativo descarta o corpo e
// aborta ao passar de 64 bytes; WGT fetch aborts after response headers,
// without reading or copying the media body.
// Serve para saber se um link de debrid leva ao arquivo ou a um video de aviso
// ("downloading.mp4", "slate.mp4") — que TOCA NORMALMENTE e por isso nao da
// erro nenhum. 1 se conseguiu resolver.
int rede_url_final(const char *url, int segundos, char *dst, unsigned tam);
// Igual, com os cabecalhos exigidos pelo addon e status HTTP final (0 quando
// nao houve resposta). Falha de transporte, HTTP nao2xx ou destino pequeno
// deixam dst vazio. Quem chama no WGT deve distinguir recusa por cabecalho
// controlado pelo navegador de fonte morta: AVPlay pode enviar esse cabecalho.
int rede_url_final_cab(const char *url, int segundos, const char *const *cabecalhos,
                       char *dst, unsigned tam, int *status);
// O MESMO PEDIDO, devolvendo tambem o que a resposta final diz ser: `mime`
// (Content-Type sem parametros, minusculo; vazio quando a resposta nao disse)
// e `corpo` (bytes do corpo quando a resposta foi um 200 completo de ate 64 B,
// ou -1 quando nao da para saber: 206, corpo cortado). Qualquer ponteiro de
// saida pode ser NULL. Nenhum pedido a mais: sao dados da mesma sonda. Serve a
// naovideo.h (link de aviso que responde HTML/JSON em vez de video).
int rede_url_final_tipo(const char *url, int segundos, const char *const *cabecalhos,
                        char *dst, unsigned tam, int *status,
                        char *mime, unsigned mimeTam, long *corpo);

#ifdef NV_VIDAA
// VARIANTE PARA A VIDAA. rede_url_final so devolve 0/1, e na VIDAA um 0 pode
// significar duas coisas bem diferentes: "resolvi, e o destino respondeu
// erro" (fonte morta de verdade) ou "a requisicao nem saiu" (bloqueio de
// conteudo misto ou de CORS na pagina https — ver o comentario de
// vidaaNuncaProxiar/vidaaPareceVideo em rede.c: URL de video nunca passa
// pelo /v1/proxy, entao esse bloqueio e ROTINEIRO ali, nao uma prova de nada).
// Devolve 1 = resolveu, 0 = resolveu e o destino falhou, -1 = desconhecido —
// quem chama decide o que fazer com "desconhecido" (streams.c trata como
// "presuma viva", a mesma politica que playlistVazia ja usa para "nao
// baixei").
int rede_url_final_vidaa(const char *url, int segundos, char *dst, unsigned tam);
#endif

// POST de JSON. Existe para o Trakt, que so aceita escrita por POST.
char *rede_postar(const char *url, int segundos, const char *const *cabecalhos,
                  const char *corpo);

// Igual, mas devolve o CODIGO HTTP em *status (0 quando a requisicao nem saiu).
// Existe por causa do Supabase: la o 401 nao e falha, e a instrucao para
// renovar o token e repetir. Sem o codigo na mao, "sessao vencida" e
// "servidor fora do ar" chegam identicos — como NULL — e o app ou perde a
// sessao a toa ou entra em laco de retry contra um erro que nao passa.
// O corpo do erro tambem volta: o PostgREST explica no corpo qual funcao ou
// tabela nao existe, e essa string e o que distingue "servidor antigo" de
// "parametro errado".
// DELETE com corpo de resposta (que costuma ser vazio: 204). Devolve NULL so em
// falha de TRANSPORTE; um 4xx volta com corpo e o status em `*status`, como em
// rede_postar_st. Existe porque o Trakt remove um item da barra de retomada por
// DELETE /sync/playback/:id e responde 404 ao mesmo caminho em POST.
char *rede_apagar(const char *url, int segundos, const char *const *cabecalhos,
                  int *status);
char *rede_postar_st(const char *url, int segundos, const char *const *cabecalhos,
                     const char *corpo, int *status);
// POST with the response Retry-After (same parsing as rede_baixar_st_retry).
char *rede_postar_st_retry(const char *url, int segundos, const char *const *cabecalhos,
                           const char *corpo, int *status, int *retryAfter);
// PUT de JSON, mesmo contrato de rede_postar_st (NULL so em falha de transporte;
// 4xx volta com corpo). Existe para o TorBox, que edita um item so por PUT.
char *rede_colocar_st(const char *url, int segundos, const char *const *cabecalhos,
                      const char *corpo, int *status);
#ifdef __EMSCRIPTEN__
// Authenticated JSON PATCH through the legacy WGT HTTP bridge.
char *rede_patch_st(const char *url, int segundos, const char *const *cabecalhos,
                    const char *corpo, int *status);
#endif
// POST que repete (espera 1 s, 2 s, ...) so quando nao houve resposta (status 0).
char *rede_postar_repetindo(const char *url, int segundos, const char *const *cabecalhos,
                            const char *corpo, int *status, int tentativas);

// GET com cabecalhos E codigo HTTP. Pelo mesmo motivo do POST acima: a leitura
// de tabela do Supabase precisa distinguir "tabela nao existe" (404 com
// PGRST205 no corpo) de "sem rede", porque a primeira significa cair para a
// tabela seguinte e a segunda significa nao mexer em nada.
//
// ATENCAO: ao contrario de rede_baixar_com, este NAO transforma 4xx em NULL.
// O corpo de erro e justamente o que o chamador quer ler.
char *rede_baixar_st(const char *url, int segundos, const char *const *cabecalhos,
                     int *status);
// O mesmo, e o Retry-After em segundos (delta ou HTTP-date; 0 = ausente,
// invalido, vencido ou oculto pelo CORS no WGT). Para os 429 dos provedores.
char *rede_baixar_st_retry(const char *url, int segundos, const char *const *cabecalhos,
                           int *status, int *retryAfter);

// GET com medicao por requisicao. Nao partilha teto, estado ou acumuladores
// com outros pedidos; a sonda de diagnostico pode chamar isto em serie ou em
// fios diferentes sem misturar os numeros.
char *rede_baixar_medido(const char *url, int segundos,
                         const char *const *cabecalhos, RedeMedida *medida);
char *rede_baixar_medido_controle(const char *url, int segundos,
                                  const char *const *cabecalhos,
                                  const RedeControle *controle,
                                  RedeMedida *medida);
char *rede_baixar_bin_medido_controle(const char *url, int segundos,
                                      const char *const *cabecalhos,
                                      const RedeControle *controle,
                                      long *tam, RedeMedida *medida);

// O mesmo, e ainda COPIA O ETag DA RESPOSTA para `etag` (vazio quando o
// servidor nao mandou nenhum).
//
// POR QUE E UMA FUNCAO A MAIS. Nenhum outro caminho deste app precisava ler um
// cabecalho de RESPOSTA — status bastava. O servico de recomendacoes sonda uma
// vez por minuto com o app aberto, e sem ETag cada sondagem traria a lista
// inteira de volta para descobrir que nada mudou. Com ele o caso comum e um
// 304 sem corpo: o `etag` devolvido aqui e o que volta no `If-None-Match` do
// pedido seguinte, e o conteudo do valor e OPACO para o cliente — ele so
// devolve o que recebeu.
//
// Num 304 o corpo e NULL e `*status` vale 304; distinguir isso de falha de
// transporte (`*status` == 0) e por conta de quem chama.
char *rede_baixar_etag(const char *url, int segundos,
                       const char *const *cabecalhos, int *status,
                       char *etag, unsigned tamEtag);

// VAZAO DE UM STREAM: baixa `url` a partir do byte `inicio` por ate
// `segundos` (contados do PRIMEIRO BYTE do corpo, nao da conexao) ou
// `maxBytes`, e JOGA OS BYTES FORA — nada e acumulado em memoria, que e o que
// a TV nao tem. Existe para o teste de velocidade do diagnostico (vazao.h).
//
// `kbpsPorSegundo` recebe uma amostra por SEGUNDO INTEIRO de corpo (ate
// `nMax`); um segundo em que nada chegou vale 0, e e justamente o trecho
// ruim que a conta do "otimo" precisa ver. Medida mais curta que 1 s (o
// arquivo acabou) vira uma amostra so. Devolve quantas amostras escreveu.
//
// `cabecalhos` como em rede_baixar_com (os proxyHeaders do addon); `final`
// (opcional) recebe o endereco depois dos redirecionamentos. `cancelado` e
// lido durante o recebimento.
//
// Tizen uses growing Range chunks, discarded in JavaScript and timed.
// Each chunk contributes samples for its elapsed seconds. Fetch stops at
// the chunk limit/deadline even if Range is ignored; bytes/kbps count only
// consumed bytes, not the complete file. A CDN without CORS for the
// origem do app (debrid) aparece como status 0 e erro -1: quem chama diz
// "nao deu para medir pelo navegador", sem inventar numero.
typedef struct {
  int status;               // HTTP da resposta final; 0 = nenhuma resposta
  int erro;                 // libcurl (0 = ok); -1 no Tizen: o navegador recusou
  long long bytes;          // recebidos (e descartados)
  unsigned long ms;         // do primeiro byte ao fim da medida
  unsigned long esperaMs;   // do pedido ao primeiro byte
  int cancelado;
} RedeVazao;
int rede_medir_vazao(const char *url, const char *const *cabecalhos, int segundos,
                     long inicio, long long maxBytes, volatile int *cancelado,
                     int *kbpsPorSegundo, int nMax, RedeVazao *res,
                     char *final, unsigned tamFinal);
// Prazo, em ms, para o PRIMEIRO byte do corpo (DNS, TLS, redirecionamentos e a
// espera do servidor) nas proximas medidas; passado dele a medida desiste. 0
// volta ao padrao (8 s). O ciclo completo baixa varias fontes em fila e uma
// fonte parada nao pode segurar a fila inteira. So o libcurl honra (o XHR
// sincrono do Tizen nao tem prazo).
void rede_vazao_espera(unsigned long ms);

// Registra quem OUVE os 401. Sem isto um token de sessao vencido era so uma
// linha no log — o Trakt continuava "conectado" na tela enquanto toda
// resposta voltava 401. O callback recebe a URL e decide se a recusa e dele.
void rede_avisar_401(void (*f)(const char *url));

// Registra quem OUVE o fim de cada pedido, para a saude da rede (redesaude.h:
// "sem internet" / "internet de volta" na ilha, 02/10). O callback recebe o
// CURLcode (0 = ok; no Tizen-wasm 0 = respondeu, 6 = sem resposta) e a URL, e
// roda no fio do pedido. primeiraMs guarda a primeira falha antes do retry
// (0 = so o resultado final). Ponteiro e nao chamada direta para os testes que
// compilam rede.c sozinho nao precisarem do modulo.
void rede_avisar_saude(void (*f)(int codigo, const char *url, unsigned primeiraMs));
// Cada pedido que terminou, com o host, o CURLcode (0 = transporte ok), o
// HTTP e quantos ms levou (0 = nao medido, no wasm). E de onde a aba Rede do
// painel de registro tira pedidos, falhas e tempo tipico por host
// (redesaude.h: rede_hosts_*). Um ouvinte so; chamado de qualquer fio.
void rede_avisar_host(void (*f)(const char *url, int codigo, int http, unsigned ms));

// Carrega a libcurl AGORA, no fio que chamar. Existe para o arranque fazer isso
// no fio principal, antes de qualquer fio de rede nascer: `curl_global_init`
// nao e seguro entre fios, e a trava interna e a segunda linha de defesa, nao a
// primeira. Chamar mais de uma vez nao custa nada.
void rede_preparar(void);

// URL SEGURA PARA LOG: so o esquema e o host, com o caminho cortado.
//
// POR QUE ISTO EXISTE, e nao e paranoia. A chave do debrid viaja no CAMINHO das
// URLs de addon ("https://host/manifest/<id>/<jwt>", "https://host/d/<chave>/
// arquivo.mkv"), e o app IMPRIME essas URLs em varios pontos — aqui mesmo, e em
// debrid.c. O destino desse texto nao e mais so um arquivo de desenvolvimento:
//   - no webOS ele vai para /tmp/nuvio.log, que e legivel por qualquer processo;
//   - no Tizen vai para o console do navegador e, com NUVIO_LOG_URL ligado, sai
//     do aparelho pela rede;
//   - e agora ele aparece NA TELA DA TV pelo painel da tecla vermelha.
// O painel corta a URL na exibicao, mas cortar so na exibicao protege a sala e
// nao o arquivo. Cortar na ORIGEM protege os dois, e o host — que e o que
// interessa para diagnosticar qual addon respondeu — continua no log.
//
// Escreve em `dst` e devolve `dst`, para poder ir direto num printf. Texto sem
// "://" e copiado como esta (nao e URL, nao ha caminho a esconder).
// Codigo da falha de transporte do ULTIMO pedido deste fio ("curl 35"), ou
// "" quando ele passou. Sem URL, corpo ou segredo; serve a tela e ao log (#223).
const char *rede_ultimo_erro(void);

// AQUECER CONEXOES (2.0.2). Abre DNS + TCP + TLS de cada origem ("https://host")
// com um HEAD em "/" (sem seguir redirecionamento, sem corpo, nunca um caminho
// de stream) e deixa o handle guardado para o proximo fio que pedir um destes
// hosts. ms[i] recebe o tempo de cada origem (0 = falhou). Devolve quantas
// abriram. Chamar de um fio descartavel; no .wgt nao faz nada (devolve 0).
int rede_aquecer_lote(const char *const *origens, int n, unsigned *ms);
int rede_aquecer_job(const char *const *origens, int n, unsigned *ms, RedeJob *job);
const char *rede_url_publica(const char *url, char *dst, unsigned tam);

// URL DE IMAGEM/META PARA LOG, mais util que rede_url_publica mas igualmente
// segura: mantem esquema, host e os segmentos do caminho que nao podem ser
// credencial (ex.: /t/p/w500/abc.jpg do TMDB), e troca por <redigido> todo
// segmento suspeito (>= 24 chars, uuid, JWT, config serializada) e a query.
// Hosts que levam a config/chave da pessoa NO CAMINHO (btttr.cc do
// BetterPosters/PostersPlus, AioMetadata, *.elfhosted, RPDB, top-posters)
// ficam so com o host e, no fim, o id do titulo. Regra em redeurl.c; teste em
// tests/redeurl_log.sh.
const char *rede_url_log(const char *url, char *dst, unsigned tam);
// O criterio de "segmento suspeito" acima, exposto para o teste.
int rede_segmento_suspeito(const char *s, unsigned n);

// CONEXAO TLS CRUA, sem HTTP. Existe para o websocket do Discord (discordws.c):
// a libcurl das TVs (7.53.1) nao fala websocket, mas abre o TLS e entrega o
// canal com CONNECT_ONLY. `url` e "https://host:porta/" — so host e porta
// contam. Bloqueia ate conectar ou estourar `segundos`. NULL = sem conexao.
// No navegador (Emscripten) sempre NULL.
// Startup only: optional trusted CA bundle for Discord OAuth and Gateway.
// Empty uses libcurl system trust; never disables certificate verification.
void rede_discord_ca(const char *pemPath);
// OAuth POST: verified TLS, dedicated handle, no native redirects.
char *rede_postar_seguro_st(const char *url, int segundos, const char *const *cab,
                            const char *corpo, int *status);
typedef struct RedeTls RedeTls;
RedeTls *rede_tls_abrir(const char *url, int segundos);
// Manda tudo ou falha: 0 ok, -1 erro.
int rede_tls_enviar(RedeTls *t, const void *buf, size_t n);
// One nonblocking send. 0 = success/would-block; *foi is the consumed prefix.
int rede_tls_tentar_enviar(RedeTls *t, const void *buf, size_t n, size_t *foi);
// Ate `n` bytes, esperando no maximo `esperaMs` (0 = nao espera).
// >0 bytes lidos, 0 nada chegou, -1 conexao caiu ou fechou.
int rede_tls_receber(RedeTls *t, void *buf, size_t n, int esperaMs);
void rede_tls_fechar(RedeTls *t);

#endif
