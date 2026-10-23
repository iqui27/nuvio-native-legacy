// OS ARQUIVOS DE UMA FONTE, E O FILME EM PARTES (dono, 16/10/2026).
//
// O addon responde UM link por titulo. Um filme que saiu cortado em episodios
// (Kaguya-sama "The First Kiss That Never Ends": 96 min ou 4 x 25 min) vem
// num torrent so, e a fonte mostrava apenas a parte 1. Segurar OK numa fonte
// (folha de Fontes) abre "Arquivos desta fonte": os videos do item na conta
// TorBox da pessoa, na ordem do torrent. Escolher um toca aquele arquivo.
//
// ACHAR O ITEM DA FONTE NO TORBOX (dono, 16/10: "nunca mostrar arquivo de
// outro lancamento"; o log diz qual passo serviu):
//   1. HASH: o do addon (infoHash / clientResolve.infoHash) ou 40 hex no link.
//      Vence tudo e e o unico caso sem "Escolher outra copia".
//   2. LEMBRADO: a copia que a pessoa ja confirmou para esta fonte (disco).
//   3. NOME DO ARQUIVO (behaviorHints.filename) num arquivo de algum item; com
//      o tamanho do addon (videoSize), so o item cujo arquivo tem esse tamanho.
//   4. TAMANHO EXATO do arquivo (videoSize) num arquivo de algum item.
//   5. FORA DA CONTA: abre o link do addon como o Play abre (o TorBox poe o
//      torrent na conta), le a pagina 1 fresca e tenta 3 e 4 de novo, com o
//      tamanho que o link responde. A modal diz "Adicionando ao seu TorBox…".
//   6. NENHUM: as copias do titulo na conta (pelo nome) viram a lista "Escolha
//      uma copia", em ordem de pontos (estilo smart detect do AIOStreams:
//      tamanho do episodio, temporada, grupo, resolucao, codec, audio, origem);
//      o titulo nunca escolhe sozinho, salvo grupo + temporada + tamanho a 2%.
// Com o hash do addon e fora da conta: o torrent entra SO se ja estiver em
// cache no TorBox (o mesmo que o addon faz ao tocar).
//
// AS ABAS: Episodios (com separador por temporada; "Partes" num filme),
// Especiais (OVA/OAD/ONA/SP/S00/Movie/Recap) e Extras (NCOP/NCED/menu/PV/CM/
// trailer/sample, ou bem menor que os episodios). A sequencia de partes e o
// autoplay andam so pela primeira.
//
// O FILME EM PARTES: tocando um arquivo da lista que tem duas partes ou mais
// (extras como NCOP/NCED/sample ficam de fora da sequencia), a SESSAO DE
// PARTES fica ligada para aquele alvo:
//   - no fim de uma parte a seguinte toca sozinha (o link e pedido 60 s antes);
//   - o progresso que vai ao Continuar assistindo, ao Trakt e a conta e o do
//     FILME INTEIRO, estimado pelo tamanho dos arquivos (partes_virtual);
//   - a parte e o ponto dentro dela ficam guardados no disco por conta e
//     perfil: o Retomar volta na mesma parte, no mesmo ponto.
// Escolher outra fonte para o titulo desliga a sessao e esquece o ponto.
//
// Nunca vai para log: link do addon, link do CDN, chave.
#ifndef NV_PARTES_H
#define NV_PARTES_H
#include <stddef.h>
#include <stdint.h>
#include "streams.h"

#define PARTES_COPIAS 12
#define PARTES_CASA_ARQUIVO "partescasa.txt"
#define PARTES_CASA_MAX 200
#define PARTES_NOME 192
#define PARTES_ARQUIVO "partes.txt"
#define PARTES_GUARDADAS 50
// Um ponto guardado vale 30 dias. O TorBox apaga itens parados depois de um
// tempo; vencido ou sumido, o Retomar cai na escolha normal.
#define PARTES_VALIDADE_S (30L * 24 * 3600)

enum {
  PARTES_VAZIO = 0, PARTES_BUSCANDO, PARTES_PRONTO,
  PARTES_SEM_CHAVE, PARTES_SEM_PLANO, PARTES_NAO_ACHOU, PARTES_ERRO,
  PARTES_ADICIONANDO,      // abrindo o link do addon para o TorBox por na conta
  PARTES_ESCOLHER          // sem casamento certo: a lista de copias (copias[])
};
enum { PARTE_EP = 0, PARTE_ESPECIAL = 1, PARTE_EXTRA = 2 };

typedef struct {
  int id;                  // file_id do TorBox
  char nome[PARTES_NOME];  // so o nome, sem as pastas
  uint64_t bytes;
  int extra;               // fora da sequencia: tipo != PARTE_EP
  int tipo;                // PARTE_EP / PARTE_ESPECIAL / PARTE_EXTRA
  int temporada;           // 0 = o nome nao diz, ou temporada zero explicita
  int temporadaZero;       // explicit season zero, independent of multipart classification
  int episodio;            // 0 = o nome nao diz
} ParteArq;

// Uma copia do titulo na conta, para "Escolher outra copia".
typedef struct {
  int item;
  char nome[160];          // o nome do item (a pasta do torrent)
  uint64_t epBytes;        // o arquivo deste episodio nela (0 = nao achou)
  int nArq;                // videos
  int temporada;           // do arquivo do episodio (0 = nao diz)
  int pontos;
  int tamanho;             // 2 = mesmo tamanho do arquivo do addon, 1 = a menos de 2%
  int grupo;               // mesmo grupo do lancamento
  char porque[72];         // resolucao, codec, audio da copia ("1080p, x264, FLAC")
} ParteCopia;

typedef struct {
  int estado;              // PARTES_*
  int usenet;              // 1 = item de usenet, 0 = torrent
  int item;                // id do item no TorBox
  int n;
  ParteArq *arq;            // owned videos, in natural path order
  int atual;               // o arquivo que a fonte toca (-1 = nao se sabe)
  char como[24];           // "hash", "filename", "link hash", "added", "size", "remembered", "chosen"
  int exato;               // casou por hash: sem "Escolher outra copia"
  char hash[41];           // o hash do item no TorBox (minusculas), para lembrar a copia
  int nCopias;
  ParteCopia copias[PARTES_COPIAS];   // as copias do titulo, melhor primeiro
  int airlock;             // 1 = o item esta no Airlock do TorBox (campo `airlocked` do mylist; ausente = 0)
} PartesLista;

// Lists must be zero-initialized; clear before discarding, copy instead of assignment.
void partes_lista_limpar(PartesLista *l);
int partes_lista_copiar(PartesLista *dst, const PartesLista *src);

// ---- puro (testes) --------------------------------------------------------
// Hashes de 40 hex soltos no texto, em minusculas. Devolve quantos.
int  partes_hashes_texto(const char *texto, char saida[][41], int max);
// Le um item do mylist (o objeto que comeca em `item`) para `l`: id, videos
// ordenados, extras marcados. 1 se tem ao menos um video.
int  partes_ler_item(const char *item, const char *fim, int usenet, PartesLista *l);
// Procura no corpo de um mylist (data = lista de itens, ou um item so) o item
// da fonte. `hashes` (n >= 0) sao comparados com o hash de cada item;
// `arquivo` (pode ser vazio) com o nome de cada arquivo; `bytes` (0 = nao se
// sabe) desempata. 1 achou, com `l` preenchida e `atual` no arquivo da fonte.
int  partes_achar(const char *corpo, int usenet, char hashes[][41], int nHashes,
                  int hashesConfiaveis, const char *arquivo, uint64_t bytes, PartesLista *l);
// O mesmo, dizendo quantos itens o corpo tinha (pagina cheia = ha outra).
int  partes_achar_n(const char *corpo, int usenet, char hashes[][41], int nHashes,
                    int hashesConfiaveis, const char *arquivo, uint64_t bytes, PartesLista *l, int *itens);
// O mesmo, e se nada casar, pelo nome do TITULO (o da folha) no nome dos itens.
int  partes_achar_t(const char *corpo, int usenet, char hashes[][41], int nHashes,
                    int hashesConfiaveis, const char *arquivo, uint64_t bytes, const char *titulo,
                    PartesLista *l, int *itens);
// Pontua as copias do titulo no corpo (puro, testes): preenche l->copias.
// `fonteTexto` = nome do arquivo + rotulo + descricao da fonte. Devolve quantas.
int  partes_copias(const char *corpo, const char *titulo, const char *arquivo, uint64_t bytes,
                   const char *fonteTexto, PartesLista *l);
// Temporada e episodio no nome de um arquivo (0 = nao diz).
void partes_temporada_episodio(const char *nome, int *temporada, int *episodio);
// Texto para comparar nomes: " palavra palavra " em minusculas (ver partes.c).
void partes_normalizar(const char *s, char *d, size_t n);
// Quantas partes de verdade (nao extras) a lista tem.
int  partes_principais(const PartesLista *l);
// A parte de verdade seguinte a `k`, ou -1.
int  partes_seguinte(const PartesLista *l, int k);
// Numero (1..) de `k` entre as partes de verdade; 0 se `k` e extra.
int  partes_numero(const PartesLista *l, int k);
// A posicao no FILME INTEIRO, pelo tamanho dos arquivos: tudo o que vem antes
// de `k` mais a fracao tocada de `k`. `catDur` (0 = nao se sabe) e a duracao
// do catalogo; sem ela, a estimativa e a duracao desta parte escalada pelo
// tamanho. 1 = *vpos/*vdur validos.
int  partes_conta_virtual(const PartesLista *l, int k, double pos, double dur, double catDur,
                          double *vpos, double *vdur);

// ---- busca da lista (fio proprio) -----------------------------------------
// Pede a lista de arquivos da fonte. `titulo` (o da folha) serve ao achado
// pelo nome quando hash e arquivo nao casam. A mesma fonte, ja achada nesta
// sessao do app (as ultimas 16), responde na hora pela memoria.
void partes_pedir(const Stream *fonte, const char *titulo);
// O OK afundou numa fonte: procura so na pagina 1 ja lida, sem rede e sem
// mudar o que a modal mostra; achou, o segurar responde da memoria.
void partes_pedir_cedo(const Stream *fonte, const char *titulo);
// "Escolher outra copia": abre o item `item` como a lista desta fonte e lembra
// a escolha (disco) para os proximos segurares.
void partes_pedir_item(const Stream *fonte, int item);
// Copia o estado da ultima busca. Devolve l->estado.
int  partes_resultado(PartesLista *l);
// O mesmo, so se a ultima busca for DESTA fonte (PARTES_VAZIO se nao for).
int  partes_resultado_de(const Stream *fonte, PartesLista *l);
void partes_cancelar(void);
// A folha de Fontes abriu: le a lista de torrents da conta em segundo plano,
// para o segurar responder da memoria (nada sem chave do TorBox).
void partes_aquecer(void);

// ---- link de um arquivo (fio proprio) -------------------------------------
// Pede o link do CDN de um arquivo. Com `comLista`, le tambem o item (para o
// Retomar, que so tem os ids guardados) e confere que o arquivo ainda existe.
void partes_link_pedir(int usenet, int item, int arquivo, int comLista);
// 0 esperando, 1 pronto (url copiada; a lista, com comLista, em *l), -1 falhou,
// -2 nada pedido. Quem le o 1 ou o -1 consome o resultado.
int  partes_link_estado(char *url, unsigned n, PartesLista *l);
void partes_link_cancelar(void);

// ---- Airlock do item (fio proprio) ----------------------------------------
// Liga (1) ou desliga (0) o Airlock do item `item` (torrent ou usenet). Um
// pedido por vez; durante um em curso, o novo e ignorado.
void partes_airlock_pedir(int usenet, int item, int ligar);
// 0 esperando, 1 feito (*ligado = o estado agora), -1 recusado (`detalhe` = a
// frase do TorBox, vazia se nao houve resposta), -2 nada pedido. Quem le o 1
// ou o -1 consome o resultado.
int  partes_airlock_estado(int *ligado, char *detalhe, unsigned n);

// ---- sessao de partes (fio do desenho) ------------------------------------
void partes_sessao_iniciar(const char *alvo, const PartesLista *l, int k);
void partes_sessao_parar(const char *porque);
int  partes_sessao_de(const char *alvo);
const PartesLista *partes_sessao_lista(void);
int  partes_sessao_indice(void);
void partes_sessao_ir(int k);
// Posicao do filme inteiro (ver partes_conta_virtual) quando ha sessao para
// `alvo`. *ultima = esta e a ultima parte.
int  partes_virtual(const char *alvo, double pos, double dur, double catDur,
                    double *vpos, double *vdur, int *ultima);

// ---- o ponto para o Retomar (disco, por conta e perfil) -------------------
typedef struct { int usenet, item, arquivo; double pos; } PartesPonto;
void partes_ponto_guardar(const char *alvo, const char *conta, int perfil, const PartesPonto *p);
int  partes_ponto_pegar(const char *alvo, const char *conta, int perfil, PartesPonto *p);
void partes_ponto_esquecer(const char *alvo, const char *conta, int perfil);
void partes_ponto_zerar(void);   // testes: o proximo acesso rele o disco

// O arquivo do TorBox que toca agora (fio do desenho): app.c define ao abrir
// um arquivo da lista, uma parte ou o Retomar; player.c le ao fechar para
// guardar o ponto dele. Limpo ao trocar de fonte, de link ou de titulo.
void partes_arquivo_definir(const char *alvo, int usenet, int item, int arquivo);
int  partes_arquivo_de(const char *alvo, PartesPonto *p);
void partes_arquivo_limpar(void);

// 1 onde o gesto existe (so o .tpk, por enquanto).
int  partes_disponivel(void);

// A FOLHA DE FONTES (streams.c): o arquivo escolhido na modal "Arquivos desta
// fonte". 1 uma vez por escolha: `fonte` e a linha da folha, `l` a lista e
// `k` o arquivo. 0 sem escolha, ou se a lista de fontes mudou por baixo.
int  stream_partes_escolheu(int *fonte, PartesLista *l, int *k);

#endif
