// Recomendacoes entre amigos — ver a nota longa em recomenda.h para o porque
// do servico proprio e por que "realtime" aqui e sondagem.
//
// O QUE ESTE ARQUIVO GUARDA EM DISCO, e por que nao so o id: `recomendacoes.txt`
// leva titulo, poster, quem mandou e a frase. A aba Social precisa desenhar no
// PRIMEIRO quadro do arranque, antes de existir catalogo e antes de a rede
// responder — exatamente a razao pela qual salvos.c guarda titulo e poster em
// vez de uma lista de ids nus (ver a nota em salvos.h). O cursor e o ETag ficam
// em `recomendacoes-cursor.txt`: sem persistir o ETag, a primeira sondagem
// depois de cada arranque baixaria a lista inteira para descobrir que nada
// mudou.
//
// O QUE ELE NAO GUARDA: token. A identidade e montada a cada ciclo a partir de
// trakt.c ou de sessao.c e vai so no cabecalho do pedido.
#include "recomenda.h"
#include "dados.h"
#include "rede.h"
#include "js.h"
#include "trakt.h"
#include "sessao.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "anim.h"
#include "layout.h"
#include "idioma.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// VAZIO E O PADRAO, e nao um esquecimento: tools/env.sh so emite -DNV_REC_URL
// quando NUVIO_REC_URL existe em local.properties, e o dono publica builds sem
// ele. Ver recomenda_ativo().
#ifndef NV_REC_URL
#define NV_REC_URL ""
#endif

#define REC_ARQ        "recomendacoes.txt"
#define REC_ARQ_CURSOR "recomendacoes-cursor.txt"
#define REC_ARQ_CARTAO "recomendacoes-cartao.txt"
// Quem eu sou para o servico: id estavel e codigo de pareamento. Separado
// dos outros dois porque tem outro tempo de vida — a lista e o cursor mudam a
// cada ciclo, isto muda uma vez na vida da conta.
#define REC_ARQ_EU     "recomendacoes-eu.txt"

#define REC_INTERVALO_MS  60000u   // sondagem com o app aberto
#define REC_ESPERA_MS      2000u   // sem identidade ainda: tentar de novo logo
#define REC_CONTATOS_MS  600000u   // a lista de contatos muda devagar
#define REC_TEMPO_REDE       12    // segundos por requisicao

// Cartao de abertura, com a mesma pegada do de atualizacao.c.
// LARGO E COM O CARTAZ MENOR, e a medida saiu da primeira captura: com 1120 de
// largura e um cartaz de 260, a coluna de texto ficava com 704px e
// "Um Sonho de Liberdade" saia cortado em "Um Sonho de...". O titulo e o unico
// dado que a pessoa precisa ler daqui — se ele nao cabe, o cartao nao serve.
#define RC_W        1280.0f
#define RC_H         470.0f
#define RC_X        ((NV_TELA_W - RC_W) * 0.5f)
#define RC_Y        ((NV_TELA_H - RC_H) * 0.5f)
#define RC_PAD        56.0f
#define RC_POSTER_W  220.0f
#define RC_POSTER_H  330.0f
#define RC_ABRIR_MS  280.0f
#define RC_FECHAR_MS 160.0f

// Os modelos prontos. O TEXTO FINAL passa por i18n na hora de desenhar, como
// todo o resto; o que viaja ao servidor e o INDICE, nao a frase — assim a
// mesma recomendacao chega em portugues numa TV e em ingles na outra.
static const char *MODELOS[REC_MODELOS] = {
  "Assiste isso hoje",
  "Melhor do ano",
  "Confia em mim",
  "Você vai chorar",
  "Dá pra ver junto?",
  "Terminei, sua vez"
};

// --- ESTADO, TODO ATRAS DO MUTEX ---------------------------------------------
static SDL_mutex *mtx;
static SDL_Thread *fio;
static int fioLigado, fioParar;

static RecItem    itens[REC_MAX];
static int        nItens;
static RecContato contatos[REC_CONTATOS_MAX];
static int        nContatos;
static long long  cursor;
static char       etagRec[96];
static int        registrado;
static char       meuId[96];
static char       meuCodigo[16];

// PEDIDOS DE CONTATO. Sao tres e nenhum e uma fila: vincular por codigo,
// remover alguem e revarrer o Trakt acontecem um por vez, disparados por um OK
// numa tela que fica esperando a resposta. Um vetor aqui seria estrutura sem
// uso — a mesma conta que a nota da `fila` de envio ja faz.
static char vincCodigo[16];
static char vincNome[64];
static int  vincEstado;
static char removerId[96];
static int  pedirTrakt, traktEstado, traktAchados;

// Fila de envio: UMA de cada vez, de proposito. O fluxo e "escolho amigo,
// escolho frase, confirmo" — nao ha como o dono disparar dois antes de ver o
// resultado do primeiro, e uma fila de verdade seria estrutura sem uso.
static struct {
  char imdb[24], tipo[8], titulo[160], poster[512], ano[16];
  char para[96], texto[72];
  int  modelo;
  int  cheia;
} fila;
static int envioEstado;
// Ids a confirmar como vistos no servidor. Cabe uma sondagem inteira (o
// servidor devolve no maximo 50 por vez).
static long long vistoFila[REC_MAX];
static int       nVistoFila;

static int      pedidoAgora;
static Uint32   proximoMs, contatosMs;

// GERACAO, incrementada por recomenda_esquecer. Um ciclo que ja estava no ar
// quando alguem saiu da conta voltaria com a lista de quem saiu e a GRAVARIA de
// volta em disco — apagar tudo e ver reaparecer segundos depois e o pior tipo
// de defeito, porque quem viu nao consegue reproduzir. O fio le a geracao antes
// do pedido e joga fora a resposta se ela mudou.
static unsigned geracao;

// Cartao de abertura (so o fio principal toca nisto).
static int   cartaoAberto, cartaoMostrado;
static float cartaoEntrada;
static RecItem cartaoItem;
static char  pedido[24];
static int   temPedido;

int recomenda_ativo(void) { return NV_REC_URL[0] != 0; }
int recomenda_aberta(void) { return cartaoAberto; }

const char *recomenda_modelo(int i) {
  return (i >= 0 && i < REC_MODELOS) ? MODELOS[i] : NULL;
}

const char *recomenda_pediu_abrir(void) {
  if (!temPedido) return NULL;
  temPedido = 0;
  return pedido;
}

// --- ARQUIVO -----------------------------------------------------------------
//
// Uma linha por recomendacao, campos separados por TAB e o TITULO por ULTIMO —
// o mesmo formato e a mesma razao de salvos.c: TAB porque titulo de filme tem
// ";" e "|" e nunca tabulacao, e o ultimo campo nao precisa de separador
// depois dele. Campo que faltar na leitura vira vazio e a linha continua
// valida, para que uma versao futura com mais colunas nao invalide o arquivo
// de quem ja usa o app.

// Tira TAB e quebra de linha do que veio da rede. O servidor ja limita tamanho
// e o texto livre a a-z0-9, mas o NOME de exibicao vem do Trakt e do Supabase
// sem essa limpeza — um nome com tabulacao partiria a linha em duas.
static void semTab(char *s) {
  for (; *s; s++) if (*s == '\t' || *s == '\n' || *s == '\r') *s = ' ';
}

static char *campo(char **p) {
  char *ini = *p, *t;
  if (!ini) return (char *)"";
  t = strchr(ini, '\t');
  if (t) { *t = 0; *p = t + 1; } else { *p = NULL; }
  return ini;
}

// Chamar com o mutex TOMADO.
static void gravar(void) {
  size_t cap = (size_t)REC_MAX * 1000u + 64u;
  char *buf = (char *)malloc(cap);
  size_t k = 0;
  int i;
  if (!buf) return;
  k += (size_t)snprintf(buf + k, cap - k, "# nuvio recomendacoes v1\n");
  for (i = 0; i < nItens && k + 1 < cap; i++)
    k += (size_t)snprintf(buf + k, cap - k,
                          "%lld\t%lld\t%d\t%d\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
                          itens[i].id, itens[i].criado, itens[i].visto,
                          itens[i].modelo, itens[i].tipo, itens[i].ano,
                          itens[i].de, itens[i].deNome, itens[i].imdb,
                          itens[i].poster, itens[i].texto, itens[i].titulo);
  dados_gravar(REC_ARQ, buf);
  free(buf);
}

// Chamar com o mutex TOMADO. O ETag vai junto do cursor porque os dois so
// fazem sentido em par: um ETag guardado com o cursor errado pede um 304 para
// uma pergunta diferente da que foi feita.
static void gravarCursor(void) {
  char s[160];
  snprintf(s, sizeof s, "%lld\t%s\n", cursor, etagRec);
  dados_gravar(REC_ARQ_CURSOR, s);
}

// Chamar com o mutex TOMADO.
static void gravarEu(void) {
  char t[160];
  snprintf(t, sizeof t, "%s\t%s\n", meuId, meuCodigo);
  dados_gravar(REC_ARQ_EU, t);
}

void recomenda_iniciar(void) {
  char *b, *linha, *prox;
  if (!recomenda_ativo()) return;
  if (!mtx) mtx = SDL_CreateMutex();
  SDL_LockMutex(mtx);
  nItens = 0;
  b = dados_ler(REC_ARQ);
  for (linha = b; linha && *linha && nItens < REC_MAX; linha = prox) {
    char *p, *fim = strchr(linha, '\n');
    RecItem *r;
    prox = fim ? fim + 1 : NULL;
    if (fim) *fim = 0;
    if (linha[0] == '#' || !linha[0]) continue;
    p = linha;
    r = &itens[nItens];
    memset(r, 0, sizeof *r);
    r->id     = atoll(campo(&p));
    r->criado = atoll(campo(&p));
    r->visto  = atoi(campo(&p));
    r->modelo = atoi(campo(&p));
    snprintf(r->tipo,   sizeof r->tipo,   "%s", campo(&p));
    snprintf(r->ano,    sizeof r->ano,    "%s", campo(&p));
    snprintf(r->de,     sizeof r->de,     "%s", campo(&p));
    snprintf(r->deNome, sizeof r->deNome, "%s", campo(&p));
    snprintf(r->imdb,   sizeof r->imdb,   "%s", campo(&p));
    snprintf(r->poster, sizeof r->poster, "%s", campo(&p));
    snprintf(r->texto,  sizeof r->texto,  "%s", campo(&p));
    snprintf(r->titulo, sizeof r->titulo, "%s", p ? p : "");
    if (r->id <= 0 || strncmp(r->imdb, "tt", 2)) continue;   // linha inutil
    nItens++;
  }
  free(b);
  b = dados_ler(REC_ARQ_CURSOR);
  if (b) {
    char *p = b, *fimLinha = strchr(b, '\n');
    if (fimLinha) *fimLinha = 0;
    cursor = atoll(campo(&p));
    snprintf(etagRec, sizeof etagRec, "%s", p ? p : "");
    free(b);
  }
  // O CODIGO VEM DO DISCO ANTES DA REDE. `registrado` continua 0 de proposito:
  // o ciclo ainda chama /v1/eu (e ele que atualiza o `visto` da pessoa no
  // servidor). O que este bloco evita e a tela de amigos abrir com o campo do
  // codigo em branco por um ou dois segundos toda vez que a TV liga.
  b = dados_ler(REC_ARQ_EU);
  if (b) {
    char *p = b, *fimLinha = strchr(b, '\n');
    if (fimLinha) *fimLinha = 0;
    snprintf(meuId, sizeof meuId, "%s", campo(&p));
    snprintf(meuCodigo, sizeof meuCodigo, "%s", p ? p : "");
    free(b);
  }
  printf("[recomenda] %d na lista local, cursor %lld\n", nItens, cursor);
  fflush(stdout);
  SDL_UnlockMutex(mtx);
}

int recomenda_n(void) {
  int n;
  if (!recomenda_ativo() || !mtx) return 0;
  SDL_LockMutex(mtx); n = nItens; SDL_UnlockMutex(mtx);
  return n;
}

int recomenda_n_novas(void) {
  int i, n = 0;
  if (!recomenda_ativo() || !mtx) return 0;
  SDL_LockMutex(mtx);
  for (i = 0; i < nItens; i++) if (!itens[i].visto) n++;
  SDL_UnlockMutex(mtx);
  return n;
}

int recomenda_item(int i, RecItem *saida) {
  int ok = 0;
  if (!recomenda_ativo() || !mtx || !saida) return 0;
  SDL_LockMutex(mtx);
  if (i >= 0 && i < nItens) { *saida = itens[i]; ok = 1; }
  SDL_UnlockMutex(mtx);
  return ok;
}

int recomenda_contatos(RecContato *saida, int max) {
  int i, n;
  if (!recomenda_ativo() || !mtx || !saida || max < 1) return 0;
  SDL_LockMutex(mtx);
  n = nContatos < max ? nContatos : max;
  for (i = 0; i < n; i++) saida[i] = contatos[i];
  SDL_UnlockMutex(mtx);
  return n;
}

const char *recomenda_meu_codigo(void) {
  // ESTATICO E COPIADO, e nao um ponteiro para `meuCodigo`: o fio de rede
  // reescreve aquele vetor quando /v1/eu responde, e quem desenha guardaria um
  // ponteiro para memoria que muda debaixo dele. Sao 16 bytes.
  static char copia[16];
  if (!recomenda_ativo() || !mtx) return "";
  SDL_LockMutex(mtx);
  snprintf(copia, sizeof copia, "%s", meuCodigo);
  SDL_UnlockMutex(mtx);
  return copia;
}

int recomenda_vincular(const char *codigo) {
  char limpo[16];
  size_t k = 0;
  if (!recomenda_ativo() || !codigo) return 0;
  // A LIMPEZA E AQUI, e nao no servidor: o teclado da TV so entrega a-z0-9,
  // mas um codigo ditado por telefone chega com espaco no meio e com a letra
  // maiuscula de quem leu de uma foto. Seis caracteres exatos ou nada — assim o
  // 400 de "codigo invalido" nunca chega a sair do aparelho.
  for (; *codigo && k + 1 < sizeof limpo; codigo++) {
    char c = *codigo;
    if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) limpo[k++] = c;
  }
  limpo[k] = 0;
  if (k != 6) return 0;
  if (!mtx) mtx = SDL_CreateMutex();
  SDL_LockMutex(mtx);
  if (vincCodigo[0] || vincEstado == REC_VINC_INDO) { SDL_UnlockMutex(mtx); return 0; }
  snprintf(vincCodigo, sizeof vincCodigo, "%s", limpo);
  vincNome[0] = 0;
  vincEstado = REC_VINC_INDO;
  SDL_UnlockMutex(mtx);
  recomenda_pedir_agora();
  return 1;
}

int recomenda_vinculo_estado(void) {
  int e;
  if (!mtx) return REC_VINC_NADA;
  SDL_LockMutex(mtx); e = vincEstado; SDL_UnlockMutex(mtx);
  return e;
}

const char *recomenda_vinculo_nome(void) {
  static char copia[64];
  if (!mtx) return "";
  SDL_LockMutex(mtx);
  snprintf(copia, sizeof copia, "%s", vincNome);
  SDL_UnlockMutex(mtx);
  return copia;
}

void recomenda_vinculo_limpar(void) {
  if (!mtx) return;
  SDL_LockMutex(mtx);
  if (vincEstado != REC_VINC_INDO) { vincEstado = REC_VINC_NADA; vincNome[0] = 0; }
  SDL_UnlockMutex(mtx);
}

int recomenda_procurar_trakt(void) {
  if (!recomenda_ativo()) return 0;
  if (!mtx) mtx = SDL_CreateMutex();
  SDL_LockMutex(mtx);
  if (traktEstado == REC_TRAKT_INDO) { SDL_UnlockMutex(mtx); return 0; }
  pedirTrakt = 1;
  traktEstado = REC_TRAKT_INDO;
  traktAchados = 0;
  SDL_UnlockMutex(mtx);
  recomenda_pedir_agora();
  return 1;
}

int recomenda_trakt_estado(void) {
  int e;
  if (!mtx) return REC_TRAKT_NADA;
  SDL_LockMutex(mtx); e = traktEstado; SDL_UnlockMutex(mtx);
  return e;
}

int recomenda_trakt_achados(void) {
  int n;
  if (!mtx) return 0;
  SDL_LockMutex(mtx); n = traktAchados; SDL_UnlockMutex(mtx);
  return n;
}

void recomenda_trakt_limpar(void) {
  if (!mtx) return;
  SDL_LockMutex(mtx);
  if (traktEstado != REC_TRAKT_INDO) traktEstado = REC_TRAKT_NADA;
  SDL_UnlockMutex(mtx);
}

int recomenda_remover_contato(const char *id) {
  if (!recomenda_ativo() || !id || !id[0]) return 0;
  if (!mtx) mtx = SDL_CreateMutex();
  SDL_LockMutex(mtx);
  if (removerId[0]) { SDL_UnlockMutex(mtx); return 0; }
  snprintf(removerId, sizeof removerId, "%s", id);
  // TIRA DA LISTA LOCAL NA HORA. A confirmacao do servidor chega no proximo
  // ciclo e a lista e relida depois dele; sem isto o nome removido continua na
  // tela ate 200 ms depois, que e tempo suficiente para a pessoa apertar OK de
  // novo e mandar a mesma remocao duas vezes.
  { int i, k = 0;
    for (i = 0; i < nContatos; i++)
      if (strcmp(contatos[i].id, id)) contatos[k++] = contatos[i];
    nContatos = k; }
  SDL_UnlockMutex(mtx);
  recomenda_pedir_agora();
  return 1;
}

void recomenda_marcar_vistas(void) {
  int i, mudou = 0;
  if (!recomenda_ativo() || !mtx) return;
  SDL_LockMutex(mtx);
  for (i = 0; i < nItens; i++) {
    if (itens[i].visto) continue;
    itens[i].visto = 1;
    mudou = 1;
    if (nVistoFila < REC_MAX) vistoFila[nVistoFila++] = itens[i].id;
  }
  if (mudou) { gravar(); pedidoAgora = 1; }
  SDL_UnlockMutex(mtx);
}

int recomenda_envio_estado(void) {
  int e;
  if (!mtx) return REC_ENVIO_NADA;
  SDL_LockMutex(mtx); e = envioEstado; SDL_UnlockMutex(mtx);
  return e;
}

void recomenda_envio_limpar(void) {
  if (!mtx) return;
  SDL_LockMutex(mtx);
  if (envioEstado != REC_ENVIO_INDO) envioEstado = REC_ENVIO_NADA;
  SDL_UnlockMutex(mtx);
}

void recomenda_esquecer(void) {
  if (!mtx) { dados_apagar(REC_ARQ); dados_apagar(REC_ARQ_CURSOR);
              dados_apagar(REC_ARQ_CARTAO); dados_apagar(REC_ARQ_EU); return; }
  SDL_LockMutex(mtx);
  geracao++;
  nItens = 0;
  nContatos = 0;
  nVistoFila = 0;
  cursor = 0;
  etagRec[0] = 0;
  meuId[0] = 0;
  meuCodigo[0] = 0;
  registrado = 0;
  fila.cheia = 0;
  envioEstado = REC_ENVIO_NADA;
  // OS PEDIDOS DE CONTATO TAMBEM MORREM AQUI. Um vinculo por codigo no ar
  // quando alguem sai da conta voltaria vinculando a pessoa ERRADA — a
  // identidade do cabecalho ja e a da conta seguinte.
  vincCodigo[0] = 0; vincNome[0] = 0; vincEstado = REC_VINC_NADA;
  removerId[0] = 0;
  pedirTrakt = 0; traktEstado = REC_TRAKT_NADA; traktAchados = 0;
  SDL_UnlockMutex(mtx);
  dados_apagar(REC_ARQ);
  dados_apagar(REC_ARQ_CURSOR);
  dados_apagar(REC_ARQ_CARTAO);
  dados_apagar(REC_ARQ_EU);
  cartaoAberto = 0;
  cartaoMostrado = 0;
}

// --- REDE --------------------------------------------------------------------
//
// Tudo daqui para baixo roda NO FIO, com uma excecao anotada. O mutex e tomado
// so para ler a identidade e para publicar o resultado — nunca durante a
// requisicao, senao o desenho travaria pelo tempo da rede.

// Buffers do fio. ESTATICOS e nao na pilha porque o token do Supabase tem ate
// 3000 caracteres e a pilha de um fio no webOS nao e lugar para isso; so o fio
// de rede os toca, e ele e um so.
static char fioAut[3200];
static char fioVia[32];
static char fioUrl[600];

// Monta os dois cabecalhos que TODA rota do servico exige. Preferencia pelo
// Trakt quando os dois existem: e a unica identidade que ja tem lista de
// amigos pronta (users/me/following), entao ela vira contatos sem o dono
// digitar codigo nenhum. 0 quando nao ha identidade — e "nao ha" e um estado
// normal, nao um erro: o app pode estar no primeiro segundo do arranque.
static int identidade(const char **cab) {
  const char *tcab[4];
  char chave[160];
  if (trakt_ativo() && trakt_cabecalhos(tcab, fioAut, sizeof fioAut,
                                        chave, sizeof chave)) {
    snprintf(fioVia, sizeof fioVia, "X-Nuvio-Auth: trakt");
  } else if (sessao_token()[0]) {
    snprintf(fioAut, sizeof fioAut, "Authorization: Bearer %s", sessao_token());
    snprintf(fioVia, sizeof fioVia, "X-Nuvio-Auth: nuvio");
  } else {
    return 0;
  }
  cab[0] = fioAut;
  cab[1] = fioVia;
  cab[2] = NULL;
  return 1;
}

static void url(const char *caminho) {
  snprintf(fioUrl, sizeof fioUrl, "%s%s", NV_REC_URL, caminho);
}

// Escapa para dentro de uma string JSON. So o necessario: aspas, barra e os
// controles. Um gerador completo seria mais codigo do que o app inteiro usa.
static void jsonEsc(char *dst, size_t tam, const char *s) {
  size_t k = 0;
  if (!s) s = "";
  for (; *s && k + 7 < tam; s++) {
    unsigned char c = (unsigned char)*s;
    if (c == '"' || c == '\\') { dst[k++] = '\\'; dst[k++] = (char)c; }
    else if (c < 0x20)         { k += (size_t)snprintf(dst + k, tam - k, "\\u%04x", c); }
    else                        dst[k++] = (char)c;
  }
  dst[k] = 0;
}

// POST /v1/eu. Cria a pessoa na primeira chamada de cada TV e devolve o id
// estavel. Sem ele nenhuma outra rota tem a quem responder.
static int registrar(const char **cab) {
  char *r;
  char id[96] = "", codigo[16] = "";
  int st = 0;
  url("/v1/eu");
  r = rede_postar_st(fioUrl, REC_TEMPO_REDE, cab, "", &st);
  if (r && st >= 200 && st < 300) {
    js_texto_raiz(r, "id", id, sizeof id);
    // O CODIGO SEMPRE VEM NESTA RESPOSTA e o cliente o ignorava. Era o unico
    // ponto onde ele existe: nao ha rota para perguntar "qual e o meu codigo?"
    // depois, porque /v1/eu ja e ela.
    js_texto_raiz(r, "codigo", codigo, sizeof codigo);
  }
  free(r);
  if (!id[0]) {
    printf("[recomenda] /v1/eu nao respondeu (HTTP %d)\n", st);
    fflush(stdout);
    return 0;
  }
  SDL_LockMutex(mtx);
  snprintf(meuId, sizeof meuId, "%s", id);
  if (codigo[0]) snprintf(meuCodigo, sizeof meuCodigo, "%s", codigo);
  registrado = 1;
  gravarEu();
  SDL_UnlockMutex(mtx);
  printf("[recomenda] registrado como %s, codigo %s\n",
         id, codigo[0] ? codigo : "?");
  fflush(stdout);
  return 1;
}

// POST /v1/contatos/trakt com os slugs de quem o dono segue.
//
// O AMIGO DO TRAKT JA E CONTATO POR CONSTRUCAO: os dois se seguem la. Obrigar
// a parear de novo por codigo seria pedir duas vezes a mesma coisa. O servidor
// vincula so quem JA usa o servico — quem nunca abriu o app nao vira contato
// porque nao ha ninguem para receber.
static int vincularTrakt(const char **cab) {
  const char *tcab[4];
  char aut[3200], chave[160], *lista;
  char corpo[6000];
  const char *p;
  size_t k;
  int n = 0;
  if (!trakt_ativo()) return 0;
  if (!trakt_cabecalhos(tcab, aut, sizeof aut, chave, sizeof chave)) return 0;
  lista = rede_baixar_com("https://api.trakt.tv/users/me/following", 10, tcab);
  if (!lista) return 0;
  k = (size_t)snprintf(corpo, sizeof corpo, "{\"slugs\":[");
  p = strchr(lista, '[');
  p = p ? p + 1 : NULL;
  while (p && *p && n < 200 && k + 80 < sizeof corpo) {
    const char *f, *u;
    char slug[96] = "";
    while (*p && (unsigned char)*p <= ' ') p++;
    if (*p != '{') break;
    f = js_fim(p);
    u = strstr(p, "\"user\"");
    if (u && u < f) {
      const char *ui = strchr(u, '{');
      if (ui) js_texto(ui, js_fim(ui), "slug", slug, sizeof slug);
    }
    if (slug[0]) {
      char esc[128];
      jsonEsc(esc, sizeof esc, slug);
      k += (size_t)snprintf(corpo + k, sizeof corpo - k, "%s\"%s\"",
                            n ? "," : "", esc);
      n++;
    }
    p = js_prox(f);
  }
  free(lista);
  snprintf(corpo + k, sizeof corpo - k, "]}");
  if (!n) return 0;
  url("/v1/contatos/trakt");
  { int st = 0, vinculados = 0;
    char *r = rede_postar_st(fioUrl, REC_TEMPO_REDE, cab, corpo, &st);
    // QUANTOS DELES JA USAM O SERVICO, que e a unica resposta que interessa a
    // tela: "2 seguidos oferecidos" e trabalho do cliente, "0 vinculados" e o
    // que a pessoa precisa ler para entender por que a lista continua vazia.
    if (r && st >= 200 && st < 300)
      vinculados = (int)js_num(r, r + strlen(r), "vinculados", 0.0);
    printf("[recomenda] %d seguidos do Trakt oferecidos, %d vinculados (HTTP %d)\n",
           n, vinculados, st);
    fflush(stdout);
    free(r);
    return vinculados; }
}

static void lerContatos(const char **cab) {
  char *r;
  const char *p;
  RecContato novos[REC_CONTATOS_MAX];
  int n = 0, st = 0;
  url("/v1/contatos");
  r = rede_baixar_st(fioUrl, REC_TEMPO_REDE, cab, &st);
  if (!r || st < 200 || st >= 300) { free(r); return; }
  p = js_array(r, NULL, "contatos");
  while (p && *p == '{' && n < REC_CONTATOS_MAX) {
    const char *f = js_fim(p);
    memset(&novos[n], 0, sizeof novos[n]);
    js_texto(p, f, "id",     novos[n].id,     sizeof novos[n].id);
    js_texto(p, f, "nome",   novos[n].nome,   sizeof novos[n].nome);
    js_texto(p, f, "origem", novos[n].origem, sizeof novos[n].origem);
    semTab(novos[n].nome);
    // Contato sem nome nao e contato quebrado: quem nunca preencheu o perfil
    // no Trakt aparece so com o slug, e o slug e o que o dono reconhece.
    if (!novos[n].nome[0] && novos[n].id[0]) {
      const char *dp = strchr(novos[n].id, ':');
      snprintf(novos[n].nome, sizeof novos[n].nome, "%s", dp ? dp + 1 : novos[n].id);
    }
    if (novos[n].id[0]) n++;
    p = js_prox(f);
  }
  free(r);
  SDL_LockMutex(mtx);
  memcpy(contatos, novos, sizeof(RecContato) * (size_t)n);
  nContatos = n;
  SDL_UnlockMutex(mtx);
}

// Insere `r` na posicao certa por id DECRESCENTE. Chamar com o mutex TOMADO.
//
// NAO E "SEMPRE NO TOPO", e a diferenca aparece na primeira leitura: o servidor
// devolve a pagina em ordem DECRESCENTE (ORDER BY r.id DESC), entao empilhar
// cada item no topo entregaria a pagina invertida — a recomendacao mais velha
// em cima, que e exatamente a linha que ninguem quer ver primeiro.
//
// Duplicata (mesmo id) nao entra duas vezes: o cursor evita o caso comum, mas
// um cache de disco antigo com o cursor zerado nao.
static int inserir(const RecItem *r) {
  int i, k;
  for (i = 0; i < nItens; i++) if (itens[i].id == r->id) return 0;
  for (k = 0; k < nItens && itens[k].id > r->id; k++) { }
  // Lista cheia e o recem-chegado e mais velho que todos: ele nao entra. A que
  // sai por baixo quando ele entra e sempre a mais velha, e nao a ultima lida.
  if (k >= REC_MAX) return 0;
  if (nItens < REC_MAX) nItens++;
  if (k < nItens - 1)
    memmove(&itens[k + 1], &itens[k],
            sizeof(RecItem) * (size_t)(nItens - 1 - k));
  itens[k] = *r;
  return 1;
}

// Le UM item do JSON. Separada de lerRecs porque e a unica parte que o teste
// consegue exercitar sem rede — ver tests/recomenda.c.
static void lerItem(const char *p, const char *f, RecItem *r) {
  memset(r, 0, sizeof *r);
  r->id     = (long long)js_num(p, f, "id", 0.0);
  r->criado = (long long)js_num(p, f, "criado", 0.0);
  r->modelo = (int)js_num(p, f, "modelo", 0.0);
  r->visto  = (int)js_num(p, f, "visto", 0.0);
  js_texto(p, f, "de",     r->de,     sizeof r->de);
  js_texto(p, f, "deNome", r->deNome, sizeof r->deNome);
  js_texto(p, f, "imdb",   r->imdb,   sizeof r->imdb);
  js_texto(p, f, "tipo",   r->tipo,   sizeof r->tipo);
  js_texto(p, f, "titulo", r->titulo, sizeof r->titulo);
  js_texto(p, f, "poster", r->poster, sizeof r->poster);
  js_texto(p, f, "ano",    r->ano,    sizeof r->ano);
  js_texto(p, f, "texto",  r->texto,  sizeof r->texto);
  semTab(r->deNome);
  semTab(r->titulo);
  semTab(r->texto);
  if (!r->tipo[0]) snprintf(r->tipo, sizeof r->tipo, "movie");
  if (!r->deNome[0] && r->de[0]) {
    const char *dp = strchr(r->de, ':');
    snprintf(r->deNome, sizeof r->deNome, "%s", dp ? dp + 1 : r->de);
  }
}

// GET /v1/rec?desde=<cursor>, com If-None-Match. Devolve 1 quando falou com o
// servidor (inclusive no 304, que e a resposta NORMAL e nao uma falha).
static int lerRecs(const char **cab) {
  const char *cabs[4];
  char cabEtag[160];
  char etagNovo[96] = "";
  char *r;
  const char *p;
  long long desde;
  unsigned ger;
  int st = 0, novos = 0;

  SDL_LockMutex(mtx);
  ger = geracao;
  desde = cursor;
  cabs[0] = cab[0]; cabs[1] = cab[1]; cabs[2] = NULL; cabs[3] = NULL;
  if (etagRec[0]) {
    snprintf(cabEtag, sizeof cabEtag, "If-None-Match: %s", etagRec);
    cabs[2] = cabEtag;
  }
  SDL_UnlockMutex(mtx);

  // MONTADA EM DOIS PASSOS, e nao num snprintf so. "desde" e o nome do
  // parametro que o servidor espera, e tambem uma palavra em portugues: num
  // formato unico a varredura de i18n (tools/varredura-i18n.py) acusa esta URL
  // como frase nao traduzida. Separar o texto fixo do numero tira o falso
  // positivo sem inventar um nome de parametro que o servidor nao conhece.
  url("/v1/rec?desde=");
  { size_t k = strlen(fioUrl);
    snprintf(fioUrl + k, sizeof fioUrl - k, "%lld", desde); }
  r = rede_baixar_etag(fioUrl, REC_TEMPO_REDE, cabs, &st,
                       etagNovo, sizeof etagNovo);
  // 304: nada mudou desde a ultima vez, e e o caso comum. Sem corpo, sem
  // trabalho, e o ETag guardado continua valendo.
  if (st == 304) { free(r); return 1; }
  if (!r || st < 200 || st >= 300) {
    free(r);
    return st != 0;
  }
  p = js_array(r, NULL, "itens");
  while (p && *p == '{') {
    const char *f = js_fim(p);
    RecItem it;
    lerItem(p, f, &it);
    if (it.id > 0 && it.imdb[0]) {
      SDL_LockMutex(mtx);
      if (ger == geracao) {
        if (inserir(&it)) novos++;
        if (it.id > cursor) cursor = it.id;
      }
      SDL_UnlockMutex(mtx);
    }
    p = js_prox(f);
  }
  SDL_LockMutex(mtx);
  // Saiu da conta enquanto isto estava no ar: nada do que voltou e desta
  // pessoa, e gravar seria desfazer o logout.
  if (ger != geracao) { SDL_UnlockMutex(mtx); free(r); return 1; }
  snprintf(etagRec, sizeof etagRec, "%s", etagNovo);
  if (novos) gravar();
  gravarCursor();
  SDL_UnlockMutex(mtx);
  free(r);
  if (novos) {
    printf("[recomenda] %d nova(s); cursor %lld\n", novos, cursor);
    fflush(stdout);
  }
  return 1;
}

static void enviarFila(const char **cab) {
  char corpo[2200];
  char t[400], po[1100], ti[64], an[48], pa[260], tx[200];
  char *r;
  int st = 0, ok;
  SDL_LockMutex(mtx);
  if (!fila.cheia) { SDL_UnlockMutex(mtx); return; }
  jsonEsc(t,  sizeof t,  fila.titulo);
  jsonEsc(po, sizeof po, fila.poster);
  jsonEsc(ti, sizeof ti, fila.tipo);
  jsonEsc(an, sizeof an, fila.ano);
  jsonEsc(pa, sizeof pa, fila.para);
  jsonEsc(tx, sizeof tx, fila.texto);
  snprintf(corpo, sizeof corpo,
           "{\"para\":\"%s\",\"imdb\":\"%s\",\"tipo\":\"%s\",\"titulo\":\"%s\","
           "\"poster\":\"%s\",\"ano\":\"%s\",\"modelo\":%d,\"texto\":\"%s\"}",
           pa, fila.imdb, ti, t, po, an, fila.modelo, tx);
  fila.cheia = 0;
  envioEstado = REC_ENVIO_INDO;
  SDL_UnlockMutex(mtx);

  url("/v1/rec");
  r = rede_postar_st(fioUrl, REC_TEMPO_REDE, cab, corpo, &st);
  ok = r && st >= 200 && st < 300 && strstr(r, "\"ok\"");
  printf("[recomenda] envio HTTP %d%s\n", st, ok ? "" : " -- falhou");
  fflush(stdout);
  free(r);
  SDL_LockMutex(mtx);
  envioEstado = ok ? REC_ENVIO_OK : REC_ENVIO_FALHA;
  SDL_UnlockMutex(mtx);
}

// OS TRES PEDIDOS DE CONTATO, num ciclo so: vincular por codigo, remover
// alguem e revarrer os seguidos do Trakt.
//
// A LISTA E RELIDA DEPOIS DE CADA UM, e nao remendada aqui: o servidor e quem
// sabe o nome de quem entrou e a origem dele, e uma copia montada de memoria
// divergiria da lista de verdade na primeira diferenca de nome. Custa um GET
// que so acontece quando alguem apertou OK numa tela de amigos.
static void tratarContatos(const char **cab) {
  char codigo[16], remover[96];
  int querTrakt, mudou = 0;

  SDL_LockMutex(mtx);
  snprintf(codigo,  sizeof codigo,  "%s", vincCodigo);  vincCodigo[0] = 0;
  snprintf(remover, sizeof remover, "%s", removerId);   removerId[0] = 0;
  querTrakt = pedirTrakt; pedirTrakt = 0;
  SDL_UnlockMutex(mtx);

  if (codigo[0]) {
    char corpo[64], nome[64] = "", *r;
    int st = 0, estado;
    snprintf(corpo, sizeof corpo, "{\"codigo\":\"%s\"}", codigo);
    url("/v1/contatos");
    r = rede_postar_st(fioUrl, REC_TEMPO_REDE, cab, corpo, &st);
    if (st >= 200 && st < 300 && r) {
      // "nome" so existe dentro de "contato" nesta resposta, entao a primeira
      // ocorrencia e a certa.
      js_texto(r, r + strlen(r), "nome", nome, sizeof nome);
      estado = REC_VINC_OK;
      mudou = 1;
    } else if (st == 404) {
      estado = REC_VINC_NAO_ACHOU;
    } else if (st == 400) {
      // O SERVIDOR DA 400 PARA DOIS CASOS e o corpo e o unico jeito de separar:
      // "codigo invalido" (que recomenda_vincular ja impede de sair daqui) e
      // "esse codigo e seu". Dizer "codigo nao encontrado" para quem digitou o
      // proprio codigo manda a pessoa conferir uma coisa que esta certa.
      estado = (r && strstr(r, "seu")) ? REC_VINC_EU_MESMO : REC_VINC_FALHA;
    } else {
      estado = REC_VINC_FALHA;
    }
    printf("[recomenda] vincular por codigo HTTP %d\n", st);
    fflush(stdout);
    free(r);
    SDL_LockMutex(mtx);
    vincEstado = estado;
    snprintf(vincNome, sizeof vincNome, "%s", nome);
    SDL_UnlockMutex(mtx);
  }

  if (remover[0]) {
    char corpo[160], esc[120];
    int st = 0;
    jsonEsc(esc, sizeof esc, remover);
    snprintf(corpo, sizeof corpo, "{\"id\":\"%s\"}", esc);
    url("/v1/contatos/remover");
    free(rede_postar_st(fioUrl, REC_TEMPO_REDE, cab, corpo, &st));
    printf("[recomenda] remover contato HTTP %d\n", st);
    fflush(stdout);
    mudou = 1;
  }

  if (querTrakt) {
    int achados = 0, estado;
    if (!trakt_ativo()) {
      // SEM TRAKT NAO HA O QUE PROCURAR, e isto nao e falha: quem entrou so com
      // conta Nuvio nao tem lista de seguidos em lugar nenhum. A tela diz isso
      // em vez de girar para sempre.
      estado = REC_TRAKT_SEM_CONTA;
    } else {
      achados = vincularTrakt(cab);
      estado = REC_TRAKT_PRONTO;
      mudou = 1;
    }
    SDL_LockMutex(mtx);
    traktEstado = estado;
    traktAchados = achados;
    SDL_UnlockMutex(mtx);
  }

  if (mudou) lerContatos(cab);
}

// POST /v1/rec/visto. O selo ja sumiu localmente quando a aba abriu; isto e so
// para os OUTROS aparelhos da mesma pessoa concordarem. Falhar aqui nao
// desfaz nada — a marca local e a que manda na tela.
static void confirmarVistas(const char **cab) {
  char corpo[900];
  size_t k;
  int i, n;
  long long copia[REC_MAX];
  SDL_LockMutex(mtx);
  n = nVistoFila;
  for (i = 0; i < n; i++) copia[i] = vistoFila[i];
  nVistoFila = 0;
  SDL_UnlockMutex(mtx);
  if (n < 1) return;
  k = (size_t)snprintf(corpo, sizeof corpo, "{\"ids\":[");
  for (i = 0; i < n && k + 24 < sizeof corpo; i++)
    k += (size_t)snprintf(corpo + k, sizeof corpo - k, "%s%lld",
                          i ? "," : "", copia[i]);
  snprintf(corpo + k, sizeof corpo - k, "]}");
  url("/v1/rec/visto");
  free(rede_postar_st(fioUrl, REC_TEMPO_REDE, cab, corpo, NULL));
}

// Um ciclo completo. 1 quando falou com o servidor (ou tentou); 0 quando nem
// havia identidade para tentar, que e o caso do primeiro segundo do arranque.
static int ciclo(void) {
  const char *cab[3];
  int reg;
  if (!identidade(cab)) return 0;
  SDL_LockMutex(mtx);
  reg = registrado;
  SDL_UnlockMutex(mtx);
  if (!reg) {
    if (!registrar(cab)) return 1;   // servidor fora; tentar de novo no proximo
    (void)vincularTrakt(cab);
    lerContatos(cab);
    contatosMs = SDL_GetTicks() + REC_CONTATOS_MS;
  } else if ((Sint32)(SDL_GetTicks() - contatosMs) >= 0) {
    lerContatos(cab);
    contatosMs = SDL_GetTicks() + REC_CONTATOS_MS;
  }
  enviarFila(cab);
  tratarContatos(cab);
  confirmarVistas(cab);
  lerRecs(cab);
  return 1;
}

static int fioLaco(void *arg) {
  (void)arg;
  while (!fioParar) {
    Uint32 agora = SDL_GetTicks();
    int agir;
    SDL_LockMutex(mtx);
    agir = pedidoAgora || (Sint32)(agora - proximoMs) >= 0;
    pedidoAgora = 0;
    SDL_UnlockMutex(mtx);
    if (agir) {
      int falou = ciclo();
      proximoMs = SDL_GetTicks() + (falou ? REC_INTERVALO_MS : REC_ESPERA_MS);
    }
    // 200 ms e a granularidade de reacao a um envio ou a abertura da aba. Com
    // a sondagem em 60 s, o laco acorda 300 vezes para fazer uma requisicao —
    // e cada despertar e uma leitura de inteiro atras do mutex.
    SDL_Delay(200);
  }
  return 0;
}

void recomenda_verificar(void) {
  if (!recomenda_ativo()) return;
  if (!mtx) mtx = SDL_CreateMutex();
  // SAIR CEDO E O PONTO DESTA FUNCAO, e nao uma micro-otimizacao. app.c a
  // chama no bloco `homePronta`, que roda uma vez POR QUADRO; marcar
  // `pedidoAgora` aqui faria o fio rodar um ciclo a cada 200 ms — 300 vezes a
  // sondagem combinada, contra o mesmo servidor, de todas as TVs.
  if (fioLigado) return;
  fioLigado = 1;
  SDL_LockMutex(mtx);
  pedidoAgora = 1;
  SDL_UnlockMutex(mtx);
  fio = SDL_CreateThread(fioLaco, "nv-recomenda", NULL);
  if (fio) SDL_DetachThread(fio);
  else {
    fioLigado = 0;
    printf("[recomenda] sem fio para falar com o servico\n");
    fflush(stdout);
  }
}

void recomenda_pedir_agora(void) {
  if (!recomenda_ativo()) return;
  recomenda_verificar();            // garante o fio de pe
  if (!mtx) return;
  SDL_LockMutex(mtx);
  pedidoAgora = 1;
  SDL_UnlockMutex(mtx);
}

int recomenda_enviar(const CatItem *ci, const char *paraId, int modelo,
                     const char *texto) {
  if (!recomenda_ativo() || !ci || !ci->imdb[0] || !paraId || !paraId[0])
    return 0;
  if (modelo != -1 && (modelo < 0 || modelo >= REC_MODELOS)) return 0;
  if (!mtx) mtx = SDL_CreateMutex();
  SDL_LockMutex(mtx);
  if (fila.cheia || envioEstado == REC_ENVIO_INDO) { SDL_UnlockMutex(mtx); return 0; }
  memset(&fila, 0, sizeof fila);
  snprintf(fila.imdb,   sizeof fila.imdb,   "%s", ci->imdb);
  snprintf(fila.tipo,   sizeof fila.tipo,   "%s", ci->tipo[0] ? ci->tipo : "movie");
  snprintf(fila.titulo, sizeof fila.titulo, "%s", ci->titulo);
  snprintf(fila.poster, sizeof fila.poster, "%s", ci->poster);
  // O ANO E OS PRIMEIROS QUATRO DIGITOS DO META. O CatItem nao tem campo de ano
  // — ele guarda "2022 · 3 temporadas" em `meta` — e mandar a linha inteira
  // encheria a coluna `ano` do servidor com texto que ninguem le.
  { int i, k = 0;
    for (i = 0; ci->meta[i] && k < 4; i++)
      if (ci->meta[i] >= '0' && ci->meta[i] <= '9') fila.ano[k++] = ci->meta[i];
      else if (k) break;
    fila.ano[k] = 0;
    if (k != 4) fila.ano[0] = 0; }
  snprintf(fila.para,  sizeof fila.para,  "%s", paraId);
  snprintf(fila.texto, sizeof fila.texto, "%s", texto ? texto : "");
  fila.modelo = modelo;
  fila.cheia = 1;
  envioEstado = REC_ENVIO_INDO;
  SDL_UnlockMutex(mtx);
  recomenda_pedir_agora();
  return 1;
}

// --- CARTAO DE ABERTURA ------------------------------------------------------

void recomenda_mostrar_se_houver(void) {
  char *visto;
  long long ultimo = 0;
  int i, achou = -1;
  if (!recomenda_ativo() || !mtx) return;
  if (cartaoMostrado || cartaoAberto) return;
  SDL_LockMutex(mtx);
  for (i = 0; i < nItens; i++)
    if (!itens[i].visto) { achou = i; break; }   // a lista ja vem da mais nova
  if (achou >= 0) cartaoItem = itens[achou];
  SDL_UnlockMutex(mtx);
  if (achou < 0) return;
  cartaoMostrado = 1;
  // UMA VEZ POR RECOMENDACAO. A marca guarda o maior id ja anunciado; uma
  // recomendacao mais velha que ela nunca volta a abrir cartao.
  visto = dados_ler(REC_ARQ_CARTAO);
  if (visto) { ultimo = atoll(visto); free(visto); }
  if (cartaoItem.id <= ultimo) return;
  { char s[48];
    snprintf(s, sizeof s, "%lld\n", cartaoItem.id);
    dados_gravar(REC_ARQ_CARTAO, s); }
  cartaoAberto = 1;
}

void recomenda_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!cartaoAberto || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    cartaoAberto = 0;
    snprintf(pedido, sizeof pedido, "%s", cartaoItem.imdb);
    temPedido = 1;
    recomenda_marcar_vistas();
    return;
  }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    // FECHAR NAO E LER. O selo continua na aba; quem dispensou o cartao nao
    // disse que ja viu a recomendacao, disse que nao quer decidir agora.
    cartaoAberto = 0;
  }
}

void recomenda_atualizar(float dt, Uint32 agora) {
  (void)agora;
  if (!cartaoAberto && cartaoEntrada < 0.002f) { cartaoEntrada = 0.0f; return; }
  cartaoEntrada = anim_rampa(cartaoEntrada, cartaoAberto ? 1.0f : 0.0f, dt,
                             cartaoAberto ? RC_ABRIR_MS : RC_FECHAR_MS);
}

// "há 2 h". A FRASE INTEIRA passa por i18n como FORMATO, nao montada de
// pedacos: "há" e "atrás" trocam de lugar na traducao (mesma regra de
// salvospainel.c).
void rec_quando_texto(char *dst, size_t tam, long long quandoS) {
  long long d = (long long)time(NULL) - quandoS;
  if (quandoS <= 0) { dst[0] = 0; return; }
  if (d < 0) d = 0;
  // "agora mesmo" e nao "agora": uma chave de UMA palavra comum e um risco real
  // neste sistema, porque a chave da tabela e o proprio portugues e qualquer
  // texto dinamico identico a ela tambem seria traduzido (ver a nota em
  // idioma.h). Duas palavras tornam a colisao improvavel.
  if (d < 90)          snprintf(dst, tam, "%s", i18n("agora mesmo"));
  else if (d < 5400)   snprintf(dst, tam, i18n("há %d min"), (int)(d / 60));
  else if (d < 172800) snprintf(dst, tam, i18n("há %d h"),   (int)(d / 3600));
  else                 snprintf(dst, tam, i18n("há %d dias"), (int)(d / 86400));
}

// Frase da recomendacao: o modelo traduzido, ou o texto livre como veio.
const char *rec_frase(const RecItem *r) {
  if (!r) return "";
  if (r->modelo >= 0 && r->modelo < REC_MODELOS) return i18n(MODELOS[r->modelo]);
  return r->texto;
}

void recomenda_desenhar(Uint32 agora) {
  float a, dy, x, y;
  char buf[320];
  (void)agora;
  if (cartaoEntrada < 0.002f) return;
  a = anim_suave(cartaoEntrada);
  dy = (1.0f - a) * 36.0f;

  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0,
          0.72f * cartaoEntrada);
  { GfxRect c = { RC_X, RC_Y + dy, RC_W, RC_H };
    gfx_cor(c, 0.030f, 0.075f, 0.078f, 0.088f, 0.98f * a); }
  gfx_recorte(RC_X, RC_Y + dy, RC_W, RC_H);

  { GfxRect p = { RC_X + RC_PAD, RC_Y + dy + (RC_H - RC_POSTER_H) * 0.5f,
                  RC_POSTER_W, RC_POSTER_H };
    GLuint tex = cartaoItem.poster[0] ? tex_obter(cartaoItem.poster) : 0;
    if (tex) {
      gfx_tex_aspect_atual = tex_aspecto(cartaoItem.poster);
      gfx_rect(p, tex, GFX_CARD, 0.0f, 0.0f, 0.0f, 0.06f, 0, 0, 0, a);
      gfx_tex_aspect_atual = 0.0f;
    } else {
      gfx_cor(p, 0.06f, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G,
              NV_COR_ESQUELETO_B, a);
    } }

  x = RC_X + RC_PAD + RC_POSTER_W + 44.0f;
  y = RC_Y + dy + 76.0f;
  { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("RECOMENDAÇÃO DE UM AMIGO"),
                           150, 154, 165, 255);
    txt_desenhar_alpha(t, x, y, a * 0.92f); y += t.h + 16.0f; }
  // Formato inteiro em i18n: em ingles o nome vem antes do verbo e depois do
  // objeto, e uma frase remendada aqui sairia na ordem errada.
  snprintf(buf, sizeof buf, i18n("%s te recomendou"), cartaoItem.deNome);
  { TxtLinha t = txt_linha_corta(TXT_CALLOUT, buf, 200, 204, 214, 255,
                                 RC_W - (x - RC_X) - RC_PAD);
    txt_desenhar_alpha(t, x, y, a * 0.95f); y += t.h + 10.0f; }
  { TxtLinha t = txt_linha_corta(TXT_TITULO2, cartaoItem.titulo, 246, 247, 252,
                                 255, RC_W - (x - RC_X) - RC_PAD);
    txt_desenhar_alpha(t, x, y, a); y += t.h + 22.0f; }
  { const char *frase = rec_frase(&cartaoItem);
    if (frase[0]) {
      snprintf(buf, sizeof buf, "\xe2\x80\x9c%s\xe2\x80\x9d", frase);
      y += txt_bloco(TXT_BODY, buf, 214, 218, 228, x, y,
                     RC_W - (x - RC_X) - RC_PAD, 38.0f, a * 0.96f, 2) + 12.0f;
    } }
  rec_quando_texto(buf, sizeof buf, cartaoItem.criado);
  if (buf[0]) {
    TxtLinha t = txt_linha(TXT_CAPTION, buf, 150, 154, 165, 255);
    txt_desenhar_alpha(t, x, y, a * 0.85f);
  }

  y = RC_Y + dy + RC_H - 70.0f;
  { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("OK para abrir"), 200, 204, 214, 255);
    txt_desenhar_alpha(t, x, y, a * 0.9f); }
  { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("Voltar para depois"),
                           150, 154, 165, 255);
    txt_desenhar_alpha(t, RC_X + RC_W - RC_PAD - t.w, y, a * 0.85f); }
  gfx_sem_recorte();
}
