// "PECA COMO FALA", PELO CELULAR (pesquisa de mercado 4.5), primeira etapa.
//
// O celular pareado (QR, endereco que nao muda: #798) manda um pedido em texto
// livre ("algo curto e engracado para ver com minha mae"); a TV pergunta ao
// Gemini, COM A CHAVE DO PROPRIO USUARIO (decisao do dono), confere cada
// sugestao no Cinemeta e mostra TRES titulos, nao uma lista.
//
// Este modulo e a REGRA PURA (sem SDL, sem rede, sem fio, sem socket): a rota
// do pedido HTTP do celular, o corpo do pedido ao Gemini, a leitura da
// resposta, a escolha do titulo certo no resultado do Cinemeta e o estado que
// a pagina le. tests/peca.sh cobre tudo. Quem escuta a rede local e
// pecaserv.c; quem fala com o Google e pecafio.c; quem desenha e pecaui.c.
//
// A CHAVE nao entra em nada que este modulo monta (url ou corpo): vai so no
// cabecalho x-goog-api-key, direto ao Google. Nenhum servidor nosso no caminho.
// O PEDIDO e o historico sao DADO para o modelo, nunca instrucao.
#ifndef NV_PECA_H
#define NV_PECA_H
#include <stddef.h>

// Alias mantido pelo Google para o Flash corrente (generativelanguage v1beta).
#define PECA_MODELO "gemini-flash-latest"
#define PECA_URL "https://generativelanguage.googleapis.com/v1beta/models/" PECA_MODELO ":generateContent"
#define PECA_CINEMETA "https://v3-cinemeta.strem.io"

#define PECA_PEDIDO_MAX  400   // bytes do pedido (uma frase falada)
#define PECA_CORPO_MAX   1024  // corpo de qualquer POST do celular
#define PECA_TOKEN_N     12    // simbolos do token do endereco
#define PECA_N           3     // titulos na TV
#define PECA_CANDIDATOS  6     // pedidos ao modelo: sobra para o que nao existir
#define PECA_CTX_MAX     40    // titulos por grupo do historico
#define PECA_ERROS_MAX   30    // caminhos errados antes de a porta se calar
#define PECA_CASTIGO_S   600
#define PECA_LEITURA_MS  4000  // prazo total para ler um pedido (conexao lenta solta a porta)

typedef struct {
  char titulo[160];    // no idioma da pessoa (o que a TV mostra)
  char original[160];  // como esta no IMDb (o que o Cinemeta entende)
  char tipo[8];        // "movie" | "series"
  int  ano;
  char motivo[220];    // uma frase: por que serve ao pedido
  // Preenchidos por peca_cinemeta_escolher:
  char id[24];         // tt...
  char poster[512];
} PecaSugestao;

typedef struct {
  const char *vistos[PECA_CTX_MAX];    int nVistos;
  const char *andamento[PECA_CTX_MAX]; int nAndamento;
  const char *lista[PECA_CTX_MAX];     int nLista;
} PecaContexto;

typedef enum {
  PECA_OK = 0,
  PECA_SEM_CHAVE,   // nenhuma chave salva
  PECA_CHAVE,       // chave recusada (400 API key / 401 / 403)
  PECA_COTA,        // 429
  PECA_BLOQUEADO,   // o modelo recusou
  PECA_REDE,        // sem resposta
  PECA_VAZIO,       // respondeu, mas nenhum titulo confirmado
  PECA_INFANTIL,    // perfil infantil: nao atende
  PECA_ERRO
} PecaResultado;

// Chave digitada -> so [A-Za-z0-9_-]; uma linha torta nunca vira cabecalho.
size_t peca_chave_limpar(const char *txt, char *dst, size_t tam);
// Pedido do celular -> uma linha, sem controles, UTF-8 inteiro. Tamanho.
size_t peca_pedido_limpar(const char *txt, char *dst, size_t tam);
int    peca_token_valido(const char *t);

// --- o celular ------------------------------------------------------------------
typedef enum {
  PECA_R_INVALIDO = 0,  // 400
  PECA_R_NEGADO,        // 403: Host/Origin de outro lugar
  PECA_R_NADA,          // 404: conta como tentativa errada
  PECA_R_GRANDE,        // 413
  PECA_R_PAGINA,        // GET  /<token>
  PECA_R_ESTADO,        // GET  /<token>/s
  PECA_R_PEDIR,         // POST /<token>/p   corpo = o pedido
  PECA_R_CHAVE,         // POST /<token>/k   corpo = a chave do Gemini
  PECA_R_ABRIR          // POST /<token>/a   corpo = "0".."2"
} PecaRota;
// `req` = cabecalhos + corpo (terminado em 0). `host` = "ip:porta" que o QR
// mostra. *corpo aponta para dentro de `req`. Regras: caminho exato do token;
// Host tem de ser o do QR (ou 127.0.0.1:porta); Origin, se vier, o mesmo
// (sem CORS, sem DNS rebinding); POST com Content-Length <= PECA_CORPO_MAX.
PecaRota peca_rota(const char *req, const char *token, const char *host,
                   const char **corpo, size_t *nCorpo);

// --- o Gemini -------------------------------------------------------------------
// Corpo JSON do generateContent (resposta em JSON com esquema). malloc.
char *peca_pedido(const char *texto, const char *idioma, const PecaContexto *ctx);
// Le a resposta: ate `max` sugestoes em v. Sem titulo/tipo validos, a
// sugestao cai. PECA_OK com *n >= 1.
PecaResultado peca_resposta(int http, const char *corpo, PecaSugestao *v, int max, int *n);
// 1 se `titulo` esta entre os vistos do contexto (comparacao normalizada).
int peca_ja_visto(const PecaContexto *ctx, const PecaSugestao *s);

// --- o Cinemeta -----------------------------------------------------------------
// .../catalog/<tipo>/top/search=<nome>.json. `base` NULL = PECA_CINEMETA.
int peca_cinemeta_url(const char *base, const PecaSugestao *s, int original, char *dst, size_t tam);
// Preenche s->id e s->poster com o resultado de MESMO nome (normalizado; o
// original ou o traduzido) e ano a no maximo 1 de distancia. Nome parecido
// nao serve: titulo errado na TV e pior que um titulo a menos. 1 = achou.
int peca_cinemeta_escolher(const char *corpo, PecaSugestao *s);

// --- o estado que a pagina le (GET /<token>/s) -----------------------------------
// {"e":"livre|pensando|pronto|erro","r":<PecaResultado>,"chave":0|1,
//  "pedido":"...","t":[{"n":"...","a":1999,"m":"..."}]}
char *peca_estado_json(const char *estado, PecaResultado r, int temChave,
                       const char *pedido, const PecaSugestao *v, int n);
#endif
