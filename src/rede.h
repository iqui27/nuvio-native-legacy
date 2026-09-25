// Busca HTTP(S) simples, para memoria.
//
// Usa a libcurl DO APARELHO por dlopen. Escrever TLS a mao estava fora de
// cogitacao e o SDK nao traz libcurl para linkar — mas /usr/lib/libcurl.so.5
// existe na TV, e os addons so falam https. No Mac usa a libcurl do sistema.
#ifndef NV_REDE_H
#define NV_REDE_H

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
char *rede_baixar_trecho_st(const char *url, int segundos, long ini, long fim,
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

// Teto de bytes da transferencia corrente (0 = sem teto). E interno ao modulo;
// esta exposto so porque rede_baixar_trecho o usa. Nao mexer de fora.
#if defined(__GNUC__)
extern _Thread_local long rede_teto;
#else
extern long rede_teto;
#endif

// Segue os redirecionamentos e devolve o endereco FINAL, sem baixar o corpo.
// Serve para saber se um link de debrid leva ao arquivo ou a um video de aviso
// ("downloading.mp4", "slate.mp4") — que TOCA NORMALMENTE e por isso nao da
// erro nenhum. 1 se conseguiu resolver.
int rede_url_final(const char *url, int segundos, char *dst, unsigned tam);

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

// GET com cabecalhos E codigo HTTP. Pelo mesmo motivo do POST acima: a leitura
// de tabela do Supabase precisa distinguir "tabela nao existe" (404 com
// PGRST205 no corpo) de "sem rede", porque a primeira significa cair para a
// tabela seguinte e a segunda significa nao mexer em nada.
//
// ATENCAO: ao contrario de rede_baixar_com, este NAO transforma 4xx em NULL.
// O corpo de erro e justamente o que o chamador quer ler.
char *rede_baixar_st(const char *url, int segundos, const char *const *cabecalhos,
                     int *status);

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
// NO TIZEN nao ha como contar bytes enquanto chegam (XHR sincrono entrega o
// corpo inteiro de uma vez): la sao pedidos de Range em pedacos crescentes,
// cada um descartado no proprio JavaScript e cronometrado, e cada pedaco
// vira tantas amostras quantos segundos ele levou. Um CDN sem CORS para a
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

// Registra quem OUVE os 401. Sem isto um token de sessao vencido era so uma
// linha no log — o Trakt continuava "conectado" na tela enquanto toda
// resposta voltava 401. O callback recebe a URL e decide se a recusa e dele.
void rede_avisar_401(void (*f)(const char *url));

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
const char *rede_url_publica(const char *url, char *dst, unsigned tam);

#endif
