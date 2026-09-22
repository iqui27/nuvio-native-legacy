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
#include "ajustes.h"
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
// A RESPOSTA SOBRE APARECER PARA OS OUTROS, em arquivo PROPRIO e nao numa
// coluna de `recomendacoes-eu.txt`. Os dois tem o mesmo tempo de vida, mas nao
// o mesmo dono: `eu` e devolvido pelo servidor e reescrito a cada arranque,
// isto e uma escolha da PESSOA e so ela escreve. Um campo a mais no arquivo do
// servidor seria uma linha de codigo a menos e uma chance a mais de a resposta
// dela ser sobrescrita por uma resposta de rede.
#define REC_ARQ_APARECER "recomendacoes-aparecer.txt"

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
// Disco da foto de quem mandou, no cartao. 56 e o menor em que a INICIAL ainda
// se le a 3 m — a mesma conta que PS_AV_MIN faz em perfilsel.c, so que ali o
// disco e a tela inteira e aqui ele divide a linha com o nome.
#define RC_AVATAR     56.0f
#define RC_AVATAR_GAP 18.0f

// REC_SELO_H e REC_SELO_GAP moram em recomenda.h: quem desenha precisa deles
// para posicionar a linha dos selos.
// A marca amarela do IMDb, na mesma proporcao do selo de detail.c (60x30)
// reduzida para caber na coluna de 568px do painel. As medidas moram em
// recomenda.h — ver o comentario la.

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

// APARECER PARA OUTRAS PESSOAS. `aparecer` e um dos tres REC_APARECER_*;
// `aparecerPendente` e -1 quando nao ha nada a dizer ao servidor, ou 0/1 para
// enviar. Sao dois campos e nao um porque "a resposta dela" e "o que falta
// avisar" tem tempos de vida diferentes: a resposta e definitiva no disco no
// mesmo instante em que ela aperta OK, o aviso pode levar tres ciclos de rede.
static int aparecer;
static int aparecerPendente = -1;

static RecSugestao sugestoes[REC_SUGESTOES_MAX];
static int         nSugestoes;
// Um de cada vez, como `removerId` e pela mesma razao: e sempre um OK numa tela
// que fica esperando a resposta.
static char        sugAdicionar[96];
// Pede a lista de sugestoes fora da hora (ao abrir a aba Social). Fora disso
// ela acompanha o relogio dos contatos — ver REC_CONTATOS_MS.
static int         pedirSugestoes;

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
  int  nota;                  // centesimos, como o `nota` do CatItem
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
//
// v1: id criado visto modelo tipo ano de deNome imdb poster texto | titulo
// v2: ... os mesmos, mais NOTA e DEAVATAR                          | titulo
//
// AS COLUNAS NOVAS ENTRAM ANTES DO TITULO porque o titulo e o unico campo que
// pode conter qualquer coisa e por isso e lido como "o resto da linha".
//
// QUEM DECIDE A VERSAO E A CONTAGEM DE TABS DA PROPRIA LINHA, e nao o cabecalho
// "# nuvio recomendacoes vN". Sao 11 tabs na v1 e 13 na v2, sempre — campo
// vazio ainda gasta o seu separador, e semTab garante que nenhum VALOR contem
// tabulacao. Pelo cabecalho, um arquivo truncado na primeira linha (ou copiado
// sem ela) seria lido com o deslocamento errado e o titulo viraria a nota; pela
// contagem, cada linha se explica sozinha. O cabecalho continua sendo escrito
// como v2, para quem abrir o arquivo com o olho.
#define REC_TABS_V1 11
#define REC_TABS_V2 13

static int contaTabs(const char *s) {
  int n = 0;
  for (; *s; s++) if (*s == '\t') n++;
  return n;
}

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
  // 1400 E NAO 1000 desde a v2: poster (512) + deAvatar (256) + titulo (160) +
  // de (96) + deNome (64) + o resto ja passam de 1100 bytes numa linha cheia, e
  // com o teto antigo a ultima linha sairia cortada no meio de uma URL.
  size_t cap = (size_t)REC_MAX * 1400u + 64u;
  char *buf = (char *)malloc(cap);
  size_t k = 0;
  int i;
  if (!buf) return;
  k += (size_t)snprintf(buf + k, cap - k, "# nuvio recomendacoes v2\n");
  for (i = 0; i < nItens && k + 1 < cap; i++)
    k += (size_t)snprintf(buf + k, cap - k,
                          "%lld\t%lld\t%d\t%d\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t"
                          "%d\t%s\t%s\n",
                          itens[i].id, itens[i].criado, itens[i].visto,
                          itens[i].modelo, itens[i].tipo, itens[i].ano,
                          itens[i].de, itens[i].deNome, itens[i].imdb,
                          itens[i].poster, itens[i].texto,
                          itens[i].nota, itens[i].deAvatar, itens[i].titulo);
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

// Chamar com o mutex TOMADO. `dados_gravar` e nao `dados_gravar_leve`: esta e
// uma resposta da PESSOA e nao um dado re-obtivel. Perder a lista de posters
// custa um download; perder um "nao" custa perguntar de novo a alguem que ja
// tinha respondido — e uma pergunta de consentimento que reaparece ensina a
// responder sem ler.
static void gravarAparecer(void) {
  char s[32];
  snprintf(s, sizeof s, "%d\n", aparecer);
  dados_gravar(REC_ARQ_APARECER, s);
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
    int tabs;
    prox = fim ? fim + 1 : NULL;
    if (fim) *fim = 0;
    if (linha[0] == '#' || !linha[0]) continue;
    // ANTES de campo(), que troca cada TAB por um terminador.
    tabs = contaTabs(linha);
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
    // v1 NAO TEM ESTES DOIS, e nao e um arquivo corrompido: e o cache gravado
    // pela versao instalada hoje na TV do dono. Sem este desvio, o titulo dele
    // seria lido como a nota e a lista abriria com quatro linhas sem nome.
    if (tabs >= REC_TABS_V2) {
      r->nota = atoi(campo(&p));
      snprintf(r->deAvatar, sizeof r->deAvatar, "%s", campo(&p));
    }
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
  // A RESPOSTA SOBRE APARECER VEM DO DISCO E NAO DA REDE, e por isso ela e lida
  // aqui e nao no primeiro ciclo: a aba Social pode abrir no primeiro segundo,
  // e um estado "nao perguntado" por falta de resposta do servidor mostraria a
  // tela de consentimento de novo a quem ja respondeu.
  aparecer = REC_APARECER_NAO_PERGUNTADO;
  b = dados_ler(REC_ARQ_APARECER);
  if (b) {
    int v = atoi(b);
    if (v == REC_APARECER_NAO || v == REC_APARECER_SIM) aparecer = v;
    free(b);
  }
  printf("[recomenda] %d na lista local, cursor %lld, aparecer %d\n",
         nItens, cursor, aparecer);
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

int recomenda_aparecer(void) {
  int v;
  // SEM SERVICO NAO HA PERGUNTA. Um pacote sem NUVIO_REC_URL nao tem aba
  // Social, e devolver "nao perguntado" aqui faria a tela de consentimento
  // existir num app onde ela nao pode levar a lugar nenhum.
  if (!recomenda_ativo() || !mtx) return REC_APARECER_NAO;
  SDL_LockMutex(mtx); v = aparecer; SDL_UnlockMutex(mtx);
  return v;
}

void recomenda_responder_aparecer(int sim) {
  if (!recomenda_ativo()) return;
  if (!mtx) mtx = SDL_CreateMutex();
  SDL_LockMutex(mtx);
  aparecer = sim ? REC_APARECER_SIM : REC_APARECER_NAO;
  aparecerPendente = sim ? 1 : 0;
  gravarAparecer();
  SDL_UnlockMutex(mtx);
  // PEDE UM CICLO AGORA porque um "sim" so vale quando o servidor souber, e o
  // efeito visivel dele (as sugestoes) vem no mesmo ciclo.
  recomenda_pedir_agora();
}

int recomenda_n_sugestoes(void) {
  int n;
  if (!recomenda_ativo() || !mtx) return 0;
  SDL_LockMutex(mtx); n = nSugestoes; SDL_UnlockMutex(mtx);
  return n;
}

int recomenda_sugestao(int i, RecSugestao *saida) {
  int ok = 0;
  if (!recomenda_ativo() || !mtx || !saida) return 0;
  SDL_LockMutex(mtx);
  if (i >= 0 && i < nSugestoes) { *saida = sugestoes[i]; ok = 1; }
  SDL_UnlockMutex(mtx);
  return ok;
}

int recomenda_adicionar_sugerido(const char *id) {
  if (!recomenda_ativo() || !id || !id[0]) return 0;
  if (!mtx) mtx = SDL_CreateMutex();
  SDL_LockMutex(mtx);
  if (sugAdicionar[0]) { SDL_UnlockMutex(mtx); return 0; }
  snprintf(sugAdicionar, sizeof sugAdicionar, "%s", id);
  // TIRA DA LISTA LOCAL NA HORA, como recomenda_remover_contato faz com o
  // contato removido e pela mesma razao: sem isto o nome continua sugerido por
  // ate 200 ms, que e tempo de sobra para um segundo OK mandar o mesmo pedido.
  { int i, k = 0;
    for (i = 0; i < nSugestoes; i++)
      if (strcmp(sugestoes[i].id, id)) sugestoes[k++] = sugestoes[i];
    nSugestoes = k; }
  SDL_UnlockMutex(mtx);
  recomenda_pedir_agora();
  return 1;
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
              dados_apagar(REC_ARQ_CARTAO); dados_apagar(REC_ARQ_EU);
              dados_apagar(REC_ARQ_APARECER); return; }
  SDL_LockMutex(mtx);
  geracao++;
  nItens = 0;
  nContatos = 0;
  nSugestoes = 0;
  sugAdicionar[0] = 0;
  pedirSugestoes = 0;
  // A RESPOSTA VOLTA A "NAO PERGUNTADO", e nao a "nao". Quem entra na conta
  // depois nao respondeu nada, e herdar o "nao" de quem saiu seria esconder a
  // pergunta de alguem que nunca a viu. Herdar o "sim" seria pior ainda. O
  // servidor guarda a resposta POR IDENTIDADE, entao a de quem saiu continua
  // valendo para ela, e a de quem entrar e relida no primeiro registro.
  aparecer = REC_APARECER_NAO_PERGUNTADO;
  aparecerPendente = -1;
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
  dados_apagar(REC_ARQ_APARECER);
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
  int st = 0, desc = 0;
  url("/v1/eu");
  r = rede_postar_st(fioUrl, REC_TEMPO_REDE, cab, "", &st);
  if (r && st >= 200 && st < 300) {
    // O QUE O SERVIDOR GUARDOU SOBRE APARECER. Ele e a autoridade por
    // IDENTIDADE, e este aparelho pode ser o segundo da mesma pessoa.
    desc = (int)js_num(r, r + strlen(r), "descobrivel", 0.0) ? 1 : 0;
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
  // RECONCILIACAO EM UM SO SENTIDO, e o sentido importa.
  //
  // Se este aparelho nunca perguntou e o servidor ja diz 1, a pessoa respondeu
  // SIM em outra TV: adotar isso e a resposta certa, e e o unico caminho pelo
  // qual `aparecer` vira SIM sem alguem apertar OK aqui. O contrario NAO vale:
  // servidor em 0 com este aparelho em "nao perguntado" continua "nao
  // perguntado", porque 0 e tambem o estado de quem nunca respondeu nada e
  // adota-lo como "nao" apagaria a pergunta sem ela ter sido feita.
  //
  // Quando este aparelho TEM uma resposta e o servidor discorda, quem manda e a
  // resposta daqui — ela e mais nova por construcao: ou foi dada nesta TV, ou
  // veio de um /v1/eu anterior.
  if (aparecer == REC_APARECER_NAO_PERGUNTADO) {
    if (desc) { aparecer = REC_APARECER_SIM; gravarAparecer(); }
  } else if (desc != (aparecer == REC_APARECER_SIM)) {
    aparecerPendente = (aparecer == REC_APARECER_SIM) ? 1 : 0;
  }
  SDL_UnlockMutex(mtx);
  printf("[recomenda] registrado como %s, codigo %s, descobrivel %d\n",
         id, codigo[0] ? codigo : "?", desc);
  fflush(stdout);
  return 1;
}

// Monta `{"slugs":[...]}` com quem o dono segue no Trakt, e devolve quantos
// entraram. 0 quando nao ha Trakt ligado ou a lista voltou vazia — e nesse caso
// `corpo` fica com um `{}` valido, porque as duas rotas que o usam aceitam
// corpo sem slugs (a de sugestoes ainda tem o ramo de amigo-de-amigo).
//
// A LISTA E CACHEADA POR REC_CONTATOS_MS. Ela e pedida por DUAS rotas agora
// (sugestoes e o vinculo manual do Trakt), e a aba Social pede um ciclo toda
// vez que abre: sem o cache, abrir e fechar o painel tres vezes custaria tres
// downloads de `users/me/following` ao Trakt, que tem limite de requisicao por
// aplicativo — nao por aparelho.
static char     fioSlugs[6000];
static int      fioSlugsN;
static Uint32   fioSlugsMs;
static int      fioSlugsTem;

static int corpoSlugsTrakt(char *corpo, size_t tam) {
  const char *tcab[4];
  char aut[3200], chave[160], *lista;
  const char *p;
  size_t k;
  int n = 0;
  if (fioSlugsTem && (Sint32)(SDL_GetTicks() - fioSlugsMs) < 0) {
    snprintf(corpo, tam, "%s", fioSlugs);
    return fioSlugsN;
  }
  snprintf(corpo, tam, "{}");
  if (!trakt_ativo()) return 0;
  if (!trakt_cabecalhos(tcab, aut, sizeof aut, chave, sizeof chave)) return 0;
  lista = rede_baixar_com("https://api.trakt.tv/users/me/following", 10, tcab);
  if (!lista) return 0;
  k = (size_t)snprintf(corpo, tam, "{\"slugs\":[");
  p = strchr(lista, '[');
  p = p ? p + 1 : NULL;
  while (p && *p && n < 200 && k + 80 < tam) {
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
      k += (size_t)snprintf(corpo + k, tam - k, "%s\"%s\"", n ? "," : "", esc);
      n++;
    }
    p = js_prox(f);
  }
  free(lista);
  snprintf(corpo + k, tam - k, "]}");
  if (!n) { snprintf(corpo, tam, "{}"); return 0; }
  snprintf(fioSlugs, sizeof fioSlugs, "%s", corpo);
  fioSlugsN = n;
  fioSlugsTem = 1;
  fioSlugsMs = SDL_GetTicks() + REC_CONTATOS_MS;
  return n;
}

// POST /v1/contatos/trakt com os slugs de quem o dono segue.
//
// SO PELO BOTAO "PROCURAR AMIGOS DO TRAKT", e nao mais no primeiro ciclo. Ver a
// nota longa em ciclo().
static int vincularTrakt(const char **cab) {
  char corpo[6000];
  int n = corpoSlugsTrakt(corpo, sizeof corpo);
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
    js_texto(p, f, "avatar", novos[n].avatar, sizeof novos[n].avatar);
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

// POST /v1/descobrivel. So sai quando ha resposta a dar: `aparecerPendente` e
// -1 no caso comum e o ciclo inteiro custa um teste de inteiro.
//
// FALHA NAO DESFAZ A RESPOSTA NO DISCO, e por isso o pendente SO e limpo com o
// servidor confirmando. Com a rede fora, a escolha continua gravada aqui e o
// aviso sai no proximo ciclo; a pessoa nao e perguntada de novo e tambem nao
// fica com a tela dizendo "sim" enquanto o servidor pensa "nao".
static void enviarAparecer(const char **cab) {
  char corpo[48];
  char *r;
  int quer, st = 0;
  SDL_LockMutex(mtx);
  quer = aparecerPendente;
  SDL_UnlockMutex(mtx);
  if (quer < 0) return;
  snprintf(corpo, sizeof corpo, "{\"descobrivel\":%d}", quer ? 1 : 0);
  url("/v1/descobrivel");
  r = rede_postar_st(fioUrl, REC_TEMPO_REDE, cab, corpo, &st);
  printf("[recomenda] aparecer=%d HTTP %d\n", quer, st);
  fflush(stdout);
  free(r);
  if (st >= 200 && st < 300) {
    SDL_LockMutex(mtx);
    // SO LIMPA SE NINGUEM MUDOU DE IDEIA NO MEIO. A pessoa pode ter apertado o
    // interruptor de novo enquanto este pedido estava no ar; limpar cegamente
    // perderia a segunda resposta, que e a que vale.
    if (aparecerPendente == quer) aparecerPendente = -1;
    SDL_UnlockMutex(mtx);
  }
}

// POST /v1/sugestoes. O corpo leva os slugs do Trakt (cacheados) e nada mais —
// o ramo de amigo-de-amigo e um JOIN do servidor sobre a tabela de contatos,
// que este aparelho nao tem e nao vai ter.
static void lerSugestoes(const char **cab) {
  char corpo[6000];
  char *r;
  const char *p;
  RecSugestao novos[REC_SUGESTOES_MAX];
  unsigned ger;
  int n = 0, st = 0;
  SDL_LockMutex(mtx);
  ger = geracao;
  SDL_UnlockMutex(mtx);
  corpoSlugsTrakt(corpo, sizeof corpo);
  url("/v1/sugestoes");
  r = rede_postar_st(fioUrl, REC_TEMPO_REDE, cab, corpo, &st);
  if (!r || st < 200 || st >= 300) { free(r); return; }
  p = js_array(r, NULL, "sugestoes");
  while (p && *p == '{' && n < REC_SUGESTOES_MAX) {
    const char *f = js_fim(p);
    memset(&novos[n], 0, sizeof novos[n]);
    js_texto(p, f, "id",      novos[n].id,      sizeof novos[n].id);
    js_texto(p, f, "nome",    novos[n].nome,    sizeof novos[n].nome);
    js_texto(p, f, "avatar",  novos[n].avatar,  sizeof novos[n].avatar);
    js_texto(p, f, "origem",  novos[n].origem,  sizeof novos[n].origem);
    js_texto(p, f, "viaNome", novos[n].viaNome, sizeof novos[n].viaNome);
    semTab(novos[n].nome);
    semTab(novos[n].viaNome);
    // Sem nome, o slug. Mesma regra de lerContatos: quem nunca preencheu o
    // perfil no Trakt aparece so com o slug, e o slug e o que se reconhece.
    if (!novos[n].nome[0] && novos[n].id[0]) {
      const char *dp = strchr(novos[n].id, ':');
      snprintf(novos[n].nome, sizeof novos[n].nome, "%s", dp ? dp + 1 : novos[n].id);
    }
    if (novos[n].id[0]) n++;
    p = js_prox(f);
  }
  free(r);
  SDL_LockMutex(mtx);
  // Saiu da conta enquanto isto estava no ar: sugestao de outra pessoa na tela
  // de quem acabou de entrar seria o pior tipo de vazamento deste recurso.
  if (ger == geracao) {
    memcpy(sugestoes, novos, sizeof(RecSugestao) * (size_t)n);
    nSugestoes = n;
  }
  SDL_UnlockMutex(mtx);
}

// POST /v1/contatos/sugerido. Devolve 1 quando vinculou — quem chama releia a
// lista de contatos depois.
static int adicionarSugerido(const char **cab, const char *id) {
  char corpo[6200], slugs[6000], esc[120];
  char *r;
  int st = 0, ok;
  jsonEsc(esc, sizeof esc, id);
  // OS SLUGS VAO JUNTO porque o servidor RECALCULA as sugestoes antes de
  // vincular, e sem eles o ramo do Trakt nao existe naquele recalculo — um
  // seguido do Trakt seria recusado com 403 na hora de adicionar, depois de ter
  // aparecido na tela. O ramo de amigo-de-amigo nao precisa de nada.
  corpoSlugsTrakt(slugs, sizeof slugs);
  { const char *interno = strchr(slugs, '[');
    if (interno) {
      const char *fim = strrchr(slugs, ']');
      size_t tam = fim && fim > interno ? (size_t)(fim - interno + 1) : 0;
      snprintf(corpo, sizeof corpo, "{\"id\":\"%s\",\"slugs\":%.*s}",
               esc, (int)tam, interno);
    } else {
      snprintf(corpo, sizeof corpo, "{\"id\":\"%s\"}", esc);
    } }
  url("/v1/contatos/sugerido");
  r = rede_postar_st(fioUrl, REC_TEMPO_REDE, cab, corpo, &st);
  ok = r && st >= 200 && st < 300 && strstr(r, "\"ok\"") != NULL;
  printf("[recomenda] adicionar sugerido HTTP %d%s\n", st, ok ? "" : " -- falhou");
  fflush(stdout);
  free(r);
  return ok;
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
  r->nota   = (int)js_num(p, f, "nota", 0.0);
  r->visto  = (int)js_num(p, f, "visto", 0.0);
  // FORA DA FAIXA VIRA 0 e nao um selo com "25,5": o servidor ja recusa, mas o
  // cache em disco pode ter vindo de uma versao futura, e desenhar lixo e pior
  // que nao desenhar.
  if (r->nota < 0 || r->nota > 100) r->nota = 0;
  js_texto(p, f, "de",     r->de,     sizeof r->de);
  js_texto(p, f, "deNome", r->deNome, sizeof r->deNome);
  js_texto(p, f, "deAvatar", r->deAvatar, sizeof r->deAvatar);
  js_texto(p, f, "imdb",   r->imdb,   sizeof r->imdb);
  js_texto(p, f, "tipo",   r->tipo,   sizeof r->tipo);
  js_texto(p, f, "titulo", r->titulo, sizeof r->titulo);
  js_texto(p, f, "poster", r->poster, sizeof r->poster);
  js_texto(p, f, "ano",    r->ano,    sizeof r->ano);
  js_texto(p, f, "texto",  r->texto,  sizeof r->texto);
  semTab(r->deNome);
  semTab(r->deAvatar);
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
           "\"poster\":\"%s\",\"ano\":\"%s\",\"modelo\":%d,\"nota\":%d,"
           "\"texto\":\"%s\"}",
           pa, fila.imdb, ti, t, po, an, fila.modelo, fila.nota, tx);
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
  char codigo[16], remover[96], sugerido[96];
  int querTrakt, mudou = 0;

  SDL_LockMutex(mtx);
  snprintf(codigo,   sizeof codigo,   "%s", vincCodigo);   vincCodigo[0] = 0;
  snprintf(remover,  sizeof remover,  "%s", removerId);    removerId[0] = 0;
  snprintf(sugerido, sizeof sugerido, "%s", sugAdicionar); sugAdicionar[0] = 0;
  querTrakt = pedirTrakt; pedirTrakt = 0;
  SDL_UnlockMutex(mtx);

  // ADICIONAR UMA SUGESTAO USA O MESMO ESTADO DE "VINCULAR POR CODIGO"
  // (REC_VINC_*), e nao um par de enums parecido: as duas acoes terminam do
  // mesmo jeito para quem esta olhando — "fulano virou contato" ou "nao deu" —
  // e duas maquinas de estado para uma frase so divergiriam na primeira
  // correcao, como o desenho da frase do modelo divergiu antes de rec_frase.
  if (sugerido[0]) {
    int ok = adicionarSugerido(cab, sugerido);
    SDL_LockMutex(mtx);
    vincEstado = ok ? REC_VINC_OK : REC_VINC_FALHA;
    vincNome[0] = 0;
    // RELE AS SUGESTOES NO MESMO CICLO, e nao daqui a dez minutos. Quem entrou
    // como contato tem de sair da lista de sugeridos, e quem o servidor recusou
    // (a pessoa revogou entre a tela e o OK) tem de sair tambem.
    pedirSugestoes = 1;
    SDL_UnlockMutex(mtx);
    mudou = 1;
  }

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
  int reg, querSug;
  if (!identidade(cab)) return 0;
  SDL_LockMutex(mtx);
  reg = registrado;
  SDL_UnlockMutex(mtx);
  if (!reg) {
    if (!registrar(cab)) return 1;   // servidor fora; tentar de novo no proximo
    // O VINCULO AUTOMATICO DOS SEGUIDOS DO TRAKT SAIU DAQUI, e a troca e
    // deliberada. Ele transformava em contato, sem ninguem apertar nada, toda
    // pessoa que o dono segue no Trakt e que tambem usa o servico — dos dois
    // lados, e a cada arranque do app. O pedido que originou esta mudanca diz
    // "mostrar os que instalaram o app ... e adicionar como amigo": mostrar e
    // adicionar sao dois passos, e o segundo e de quem esta olhando.
    //
    // O QUE NAO MUDOU: a rota /v1/contatos/trakt continua existindo e continua
    // vinculando na hora — ela e o botao "procurar amigos do Trakt" da tela de
    // amigos, que e uma acao que alguem toma. O que deixou de existir e a
    // varredura silenciosa no arranque. Reverter e recolocar uma linha aqui.
    lerContatos(cab);
    lerSugestoes(cab);
    contatosMs = SDL_GetTicks() + REC_CONTATOS_MS;
  } else if ((Sint32)(SDL_GetTicks() - contatosMs) >= 0) {
    lerContatos(cab);
    lerSugestoes(cab);
    contatosMs = SDL_GetTicks() + REC_CONTATOS_MS;
  }
  enviarAparecer(cab);
  enviarFila(cab);
  tratarContatos(cab);
  confirmarVistas(cab);
  SDL_LockMutex(mtx);
  querSug = pedirSugestoes; pedirSugestoes = 0;
  SDL_UnlockMutex(mtx);
  // FORA DO RELOGIO DE DEZ MINUTOS so quando alguem pediu: abrir a aba Social
  // ou aceitar uma sugestao. A sondagem de 60 s NAO pede sugestao — a lista de
  // quem a pessoa talvez conheca nao muda de minuto em minuto, e cada pedido
  // custa um JOIN no servidor e, no ramo do Trakt, uma leitura do cache.
  if (querSug) lerSugestoes(cab);
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
  // AS SUGESTOES ACOMPANHAM O PEDIDO MANUAL, e nao a sondagem de 60 s. Quem
  // chama esta funcao e sempre uma acao de quem esta na sala — abrir a aba
  // Social, vincular alguem, responder a pergunta do consentimento — e depois
  // de qualquer uma delas a lista de sugeridos pode ter mudado. A sondagem
  // periodica continua sem pedir nada disso.
  pedirSugestoes = 1;
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
  // A NOTA SAI DAQUI, do CatItem de quem MANDA, e nao do catalogo de quem
  // recebe: o titulo recomendado pode nao existir no catalogo do amigo — e esse
  // e justamente o caso em que uma recomendacao e util. A unidade e a do
  // CatItem (centesimos, 83 = 8,3); fora da faixa vira 0 e o selo some.
  fila.nota = (ci->nota >= 0 && ci->nota <= 100) ? ci->nota : 0;
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

// "Segue no Trakt" / "Amigo de Gustavo". A FRASE INTEIRA passa por i18n como
// FORMATO, e nao montada de "Amigo de" + nome: em ingles a preposicao e a ordem
// mudam, e a chave da tabela e sempre uma string inteira.
void rec_sugestao_origem(char *dst, size_t tam, const RecSugestao *s) {
  if (!dst || tam < 2) return;
  dst[0] = 0;
  if (!s) return;
  if (!strcmp(s->origem, "trakt")) {
    // "VOCE SEGUE", com o sujeito. `users/me/following` e quem o dono segue, e
    // nao quem o segue — "Segue no Trakt" sozinho le como o contrario, e a
    // frase existe justamente para a pessoa decidir se conhece o sugerido.
    snprintf(dst, tam, "%s", i18n("Você segue no Trakt"));
    return;
  }
  // SEM O NOME DO INTERMEDIARIO ainda ha o que dizer, e dizer importa: a frase
  // e a unica coisa na linha que explica por que um estranho esta sendo
  // sugerido. Vazio acontece quando quem faz a ponte nunca preencheu o perfil.
  if (s->viaNome[0]) snprintf(dst, tam, i18n("Amigo de %s"), s->viaNome);
  else               snprintf(dst, tam, "%s", i18n("Amigo de um contato seu"));
}

// Frase da recomendacao: o modelo traduzido, ou o texto livre como veio.
const char *rec_frase(const RecItem *r) {
  if (!r) return "";
  if (r->modelo >= 0 && r->modelo < REC_MODELOS) return i18n(MODELOS[r->modelo]);
  return r->texto;
}

// --- AS TRES MARCAS DA LINHA -------------------------------------------------

// Primeiro CARACTERE, e nao primeiro byte: "Álvaro" tem dois bytes na primeira
// letra e cortar no byte produz um glifo invalido. Copiada de perfilsel.c de
// proposito — la ela e estatica, e exportar uma funcao de uma tela de perfil
// para um modulo de rede seria a dependencia errada.
static void recInicial(const char *nome, char *dst, size_t tam) {
  size_t z = 1;
  if (tam < 5) { if (tam) dst[0] = 0; return; }
  if (!nome || !nome[0]) { dst[0] = '?'; dst[1] = 0; return; }
  while (z < 4 && (nome[z] & 0xc0) == 0x80) z++;
  memcpy(dst, nome, z);
  dst[z] = 0;
}

// A COR DO DISCO SAI DO ID, e nao de um acaso nem do acento do aparelho.
//
// perfilsel.c usa a cor que a CONTA guarda para cada perfil; um contato deste
// servico nao tem cor nenhuma no servidor, e inventar uma nova a cada quadro
// faria o mesmo amigo mudar de cor entre duas linhas. Um hash do id estavel
// resolve os dois: a cor e sempre a mesma para a mesma pessoa, e duas pessoas
// diferentes quase sempre caem em discos diferentes.
//
// As seis cores sao as da paleta de acentos que ajustes.c ja oferece, e nao um
// arco-iris novo: elas ja foram escolhidas para ter contraste contra o #0D0D0D
// do fundo e para a letra branca se ler por cima.
static void recCorDoId(const char *id, float *r, float *g, float *b) {
  static const float PALETA[6][3] = {
    { 0.180f, 0.490f, 0.910f },   // azul
    { 0.400f, 0.733f, 0.416f },   // verde
    { 0.855f, 0.420f, 0.290f },   // coral
    { 0.560f, 0.420f, 0.850f },   // violeta
    { 0.910f, 0.650f, 0.200f },   // ambar
    { 0.180f, 0.680f, 0.700f },   // turquesa
  };
  unsigned h = 2166136261u;       // FNV-1a: oito linhas a menos que um md5 e
  const char *p = id ? id : "";   // com a unica propriedade que interessa aqui
  for (; *p; p++) { h ^= (unsigned char)*p; h *= 16777619u; }
  h %= 6u;
  *r = PALETA[h][0]; *g = PALETA[h][1]; *b = PALETA[h][2];
}

void rec_avatar(GfxRect a, const char *url, const char *nome, const char *id,
                float alfa) {
  GLuint foto = (url && url[0]) ? tex_obter_larg(url, a.w) : 0;
  // SOQUETE ESCURO POR BAIXO SEMPRE, como perfilsel.c: enquanto a foto nao
  // chega da rede, o lugar dela e um disco e nao um buraco com o fundo do
  // painel aparecendo — e um buraco redondo le como defeito.
  gfx_rect(a, 0, GFX_DISCO, 0, 0, 0, 0, 0.09f, 0.09f, 0.10f, alfa);
  if (foto) {
    gfx_tex_aspect_atual = tex_aspecto(url);
    gfx_rect(a, foto, GFX_AVATAR, 0, 0, 0, 0, 1, 1, 1, alfa);
    gfx_tex_aspect_atual = 0.0f;
    return;
  }
  { float cr, cg, cb;
    char ini[8];
    TxtLinha l;
    recCorDoId(id && id[0] ? id : nome, &cr, &cg, &cb);
    gfx_rect(a, 0, GFX_DISCO, 0, 0, 0, 0, cr, cg, cb, alfa);
    recInicial(nome, ini, sizeof ini);
    l = txt_linha(a.w >= 48.0f ? TXT_CALLOUT : TXT_CAPTION2, ini, 255, 255, 255, 255);
    txt_desenhar_alpha(l, a.x + (a.w - l.w) * 0.5f, a.y + (a.h - l.h) * 0.5f, alfa); }
}

float rec_selo_tipo(float x, float y, const char *tipo, int escuro, float alfa) {
  // "Série" e "Filme" JA SAO CHAVES DA TABELA — as mesmas que metaTexto usa na
  // aba Salvos. Reaproveita-las e o que faz a aba Social e a aba Salvos dizerem
  // "Série" com a mesma palavra em qualquer idioma.
  //
  // O DESENHO E O DA TABELA UNICA (badges.h, 21/09/2026): neutro sobre o
  // painel, e a variante SOBRE_REALCE quando a linha esta em foco — a pilula
  // clara do foco engolia um preenchimento branco a 0.10, entao `escuro`
  // troca o estilo e nao so a cor.
  int serie = tipo && !strncmp(tipo, "series", 6);
  return badge_desenhar(x, y, serie ? "Série" : "Filme",
                        escuro ? BADGE_SOBRE_REALCE : BADGE_NEUTRO, alfa);
}

float rec_selo_imdb(float x, float y, int nota, int escuro, float alfa) {
  // A MARCA da tabela unica: amarelo #F5C518 e "IMDb" preto em todo lugar
  // (home, card de Continuar, detalhe, aqui). Separador decimal pelo idioma
  // fica dentro de badge_imdb.
  return badge_imdb(x, y, nota, escuro, alfa);
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
  y = RC_Y + dy + 66.0f;
  { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("RECOMENDAÇÃO DE UM AMIGO"),
                           150, 154, 165, 255);
    txt_desenhar_alpha(t, x, y, a * 0.92f); y += t.h + 14.0f; }
  // A CARA DE QUEM MANDOU AO LADO DO NOME. Era so o nome, e um nome sozinho num
  // cartao que aparece no arranque nao diz de quem e ate a pessoa LER — a foto
  // diz antes. Sem foto (conta Nuvio) o disco leva a inicial, exatamente como
  // perfilsel.c faz com perfil sem foto.
  { GfxRect av = { x, y + 2.0f, RC_AVATAR, RC_AVATAR };
    float tx2 = x + RC_AVATAR + RC_AVATAR_GAP;
    rec_avatar(av, cartaoItem.deAvatar, cartaoItem.deNome, cartaoItem.de, a);
    // Formato inteiro em i18n: em ingles o nome vem antes do verbo e depois do
    // objeto, e uma frase remendada aqui sairia na ordem errada.
    snprintf(buf, sizeof buf, i18n("%s te recomendou"), cartaoItem.deNome);
    { TxtLinha t = txt_linha_corta(TXT_CALLOUT, buf, 200, 204, 214, 255,
                                   RC_W - (tx2 - RC_X) - RC_PAD);
      txt_desenhar_alpha(t, tx2, y + (RC_AVATAR - t.h) * 0.5f, a * 0.95f); }
    y += RC_AVATAR + 14.0f; }
  { TxtLinha t = txt_linha_corta(TXT_TITULO2, cartaoItem.titulo, 246, 247, 252,
                                 255, RC_W - (x - RC_X) - RC_PAD);
    txt_desenhar_alpha(t, x, y, a); y += t.h + 16.0f; }
  { const char *frase = rec_frase(&cartaoItem);
    if (frase[0]) {
      snprintf(buf, sizeof buf, "\xe2\x80\x9c%s\xe2\x80\x9d", frase);
      y += txt_bloco(TXT_BODY, buf, 214, 218, 228, x, y,
                     RC_W - (x - RC_X) - RC_PAD, 38.0f, a * 0.96f, 2) + 16.0f;
    } }
  // FILME OU SÉRIE, A NOTA E O QUANDO, NA MESMA LINHA. Sao as tres coisas que
  // se olham de relance e nenhuma delas merece uma linha propria — juntas elas
  // custam os mesmos 30px que o "há 2 h" sozinho custava.
  { float sx = x;
    sx += rec_selo_tipo(sx, y, cartaoItem.tipo, 0, a * 0.95f) + REC_SELO_GAP;
    { float w = rec_selo_imdb(sx, y, cartaoItem.nota, 0, a * 0.95f);
      if (w > 0.0f) sx += w + REC_SELO_GAP + 6.0f; }
    rec_quando_texto(buf, sizeof buf, cartaoItem.criado);
    if (buf[0]) {
      TxtLinha t = txt_linha(TXT_CAPTION, buf, 150, 154, 165, 255);
      txt_desenhar_alpha(t, sx, y + (REC_SELO_H - t.h) * 0.5f, a * 0.85f);
    } }

  y = RC_Y + dy + RC_H - 70.0f;
  { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("OK para abrir"), 200, 204, 214, 255);
    txt_desenhar_alpha(t, x, y, a * 0.9f); }
  { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("Voltar para depois"),
                           150, 154, 165, 255);
    txt_desenhar_alpha(t, RC_X + RC_W - RC_PAD - t.w, y, a * 0.85f); }
  gfx_sem_recorte();
}
