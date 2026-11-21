// FICHA DO TITULO — o que o modelo de gosto precisa saber de cada titulo
// visto ou candidato (PLANO-GOSTO secao 2.1, issue 36).
//
// A ficha e o minimo de cada obra: quem ela e, quanto dura e os ids do TMDB
// das dimensoes que o FarolGosto pontua (genero, pessoa, tema). Nao guarda
// titulo, sinopse nem arte: isso o catalogo e o mapa ja tem; a ficha so
// precisa responder "de que esta historia e feita".
//
// DE ONDE VEM
//   TMDB      a MESMA resposta que o mapa pede (/movie|tv/<id>?append_to_
//             response=keywords,credits,recommendations), lida por
//             mapa_ler_ids — o parser e um so, extraido de mapa_ler_detalhe.
//             Vem da fila lenta da secao 2.1, um pedido por vez; aqui so
//             entra o corpo ja baixado.
//   CatItem   sem chave do TMDB (ou sem rede), genero/direcao/elenco do
//             item do catalogo viram ids NEGATIVOS estaveis pelo
//             mapa_hash_nome — a mesma convencao do MapaFio na reserva.
//             `genero` aceita os dois formatos do catalogo: com o rotulo do
//             tipo na frente (pacote/Trakt) e a lista crua (Jellyfin/Plex).
//
// MEMORIA. A ficha tem 96 bytes cravados (_Static_assert): cabe no vetor do
// modelo sem malloc e sem fragmentar. O cache e um vetor fixo de 600 em
// ordem LRU, gravado inteiro em farol-fichas.bin (~58 KB) pelo caminho que
// o chamador passa (dados_dir() na integracao — o modulo e puro e nao
// conhece dados.h). Nenhum titulo, nome de pessoa ou perfil entra na ficha
// nem em log algum daqui: so ids e numeros.
//
// FIOS. Sem estado proprio: o cache mora no chamador, que segura um por fio
// ou serializa o acesso (a fila lenta e um fio so). FarolFichaTmp e a area
// de rascunho da leitura do JSON, tambem uma por fio.
#ifndef NV_FAROLFICHA_H
#define NV_FAROLFICHA_H
#include <stdint.h>
#include "catalogo.h"          // CatItem (so a struct; nenhuma funcao)
#include "mapa.h"              // MapaDetalhe (o rascunho de mapa_ler_ids)

#define FAROL_FICHA_MAX    600   // tamanho do cache (secao 2.1)
#define FAROL_FICHA_GEN    4
#define FAROL_FICHA_PESSOA 6
#define FAROL_FICHA_TEMA   8

enum { FF_FILME = 1, FF_SERIE = 2 };
enum { FF_DE_NADA = 0, FF_DE_TMDB = 1, FF_DE_CAT = 2 };

typedef struct {
  int32_t  tmdb;                     // id do titulo no TMDB; 0 = so catalogo
  uint8_t  tipo;                     // FF_FILME | FF_SERIE
  uint8_t  nota;                     // vote_average * 10 (0..100)
  uint16_t ano;                      // 0 = desconhecido
  uint16_t idioma;                   // ISO 639-1 empacotado: 'p'<<8|'t'; 0 = ?
  uint16_t minutos;                  // duracao; na serie, do episodio
  uint16_t votos;                    // vote_count, teto 65535 (media bayesiana)
  uint8_t  nGenero, nPessoa, nTema;  // quantas das vagas estao cheias
  uint8_t  fonte;                    // FF_DE_*
  int32_t  genero[FAROL_FICHA_GEN];  // ids do TMDB; < 0 = hash do nome (CatItem)
  int32_t  pessoa[FAROL_FICHA_PESSOA]; // direcao/criacao primeiro; idem
  int32_t  tema[FAROL_FICHA_TEMA];   // ids das palavras-chave do TMDB
  int32_t  reserva;                  // sem TMDB: bit30=id do item | titulo+ano
} FarolFicha;                        // 96 bytes, sem ponteiro nem texto

_Static_assert(sizeof(FarolFicha) == 96, "FarolFicha fora dos 96 bytes da secao 2.1");

// Area de rascunho de farolficha_ler_tmdb: a resposta do TMDB decodificada
// antes do recorte de 96 bytes. Uma por fio que monta fichas.
typedef MapaDetalhe FarolFichaTmp;

// A chave do cache: identidade da ficha, nao do ponteiro.
//   com TMDB     (tipo << 32) | tmdb            — positiva
//   sem TMDB     -((tipo << 32) | reserva)      — negativa, como os ids de nome
// A reserva tem dois espacos separados pelo bit 30: ligado = hash do id do
// item (imdb ou jf:/em:/px: do servidor); apagado = hash de titulo+ano.
// 0 = a ficha nao identifica titulo nenhum (nao vale guardar).
int64_t farolficha_chave(const FarolFicha *f);

// Preenche a ficha a partir do corpo de /movie|tv/<id>?append_to_response=
// keywords,credits,recommendations (o mesmo pedido do mapa). `serie` escolhe
// os nomes de campo de TV. `tmp` e o rascunho; pode ser reutilizado na
// proxima chamada do mesmo fio. Resposta truncada devolve a ficha do que deu
// para ler. 1 = a resposta era um detalhe com conteudo.
int farolficha_ler_tmdb(const char *json, int serie, FarolFichaTmp *tmp, FarolFicha *f);

// Preenche a ficha a partir do item do catalogo: o caminho sem TMDB.
// Genero, direcao e elenco viram ids negativos estaveis (mapa_hash_nome);
// elenco que ja tem id do TMDB guarda o id de verdade. 1 = montou.
int farolficha_ler_item(const CatItem *ci, FarolFicha *f);

// --- cache LRU de 600, com arquivo ----------------------------------------

typedef struct {
  FarolFicha f[FAROL_FICHA_MAX];   // f[0] e a mais recente; f[n-1] a mais velha
  uint16_t   n;
  uint16_t   sujo;                 // 1 = gravacao pendente
} FarolFichaCache;                 // 57 604 bytes; quem chama guarda uma estatica

void farolficha_cache_iniciar(FarolFichaCache *c);
// 1 = achou (e copia em `dst`, se nao NULL): a ficha sobe para a frente.
int  farolficha_obter(FarolFichaCache *c, int64_t chave, FarolFicha *dst);
// Insere ou atualiza pela chave da propria ficha, que vira a mais recente.
// Cheio: sai a mais velha. Ficha sem identidade (chave 0) nao entra.
void farolficha_guardar(FarolFichaCache *c, const FarolFicha *f);

// Le o arquivo no formato "FFICHA01" e substitui o cache. 0 = arquivo
// ausente OU corrompido — o corrompido e apagado e o cache fica vazio,
// nunca trava nem lê pela metade. 1 = lido.
int  farolficha_ler_arquivo(FarolFichaCache *c, const char *caminho);
// Grava atomico (arquivo .tmp + rename) so as `n` fichas. 1 = gravou.
int  farolficha_gravar(FarolFichaCache *c, const char *caminho);

// O mesmo formato "FFICHA01" em memoria, para quem serializa/grava a partir
// de copia fora de trava (correcao FAROL38D). escrever devolve os bytes
// escritos (0 = nao coube); o binario inteiro cabe em FICHA_BIN_MAX.
#define FAROL_FICHA_BIN_MAX (12 + FAROL_FICHA_MAX * 96)
size_t farolficha_escrever(const FarolFichaCache *c, unsigned char *dst,
                           size_t cap);
// Le o buffer no lugar do arquivo: mesma rejeicao total a corrupcao, sem
// apagar nada (nao ha arquivo). 1 = cache substituido.
int  farolficha_ler_mem(FarolFichaCache *c, const unsigned char *buf, size_t n);

#endif
