// RECOMENDAR UM TITULO A UM AMIGO — o lado cliente.
//
// POR QUE EXISTE E POR QUE CONTRA UM SERVIDOR PROPRIO: o Supabase do Nuvio nao
// tem nada social (medido em 15/09/2026 no indice OpenAPI do PostgREST: 13
// tabelas, 52 RPC, nenhuma de amigo, recomendacao ou aviso) e este repositorio
// e um fork nao oficial — criar tabela la nao e uma decisao nossa. O servico
// vive em servidor/recomendacoes/ e nao toca em nada do Nuvio. Ver
// PLANO-SOCIAL-RECOMENDACOES.md.
//
// A DISCIPLINA E A DE atualizacao.c: um fio proprio para a rede, todo o estado
// atras de um mutex, e o laco de desenho so LE. Nenhuma funcao daqui bloqueia,
// com uma excecao anotada (recomenda_enviar apenas ENFILEIRA).
//
// SEM URL COMPILADA O MODULO INTEIRO NAO EXISTE. recomenda_ativo() devolve 0,
// nenhuma funcao abre conexao, e quem desenha esconde a aba e o item de menu.
// O dono publica builds sem NUVIO_REC_URL em local.properties, e um botao que
// so dá erro e pior que botao nenhum.
//
// REALTIME AQUI E SONDAGEM. Este app so tem HTTP (rede.c carrega a libcurl do
// aparelho por dlopen; nao ha WebSocket nem SSE, e no Tizen o mesmo codigo roda
// em WebAssembly). A sondagem e de 60 s com o app aberto, e o caso comum custa
// um 304 sem corpo porque o cliente devolve o ETag que recebeu.
#ifndef NV_RECOMENDA_H
#define NV_RECOMENDA_H

#include "catalogo.h"
#include "gfx.h"
#include <SDL2/SDL.h>
#include <stddef.h>

// Teto da lista guardada. 60 x ~900 bytes = ~54 KB estaticos, pela mesma razao
// de SALVOS_MAX: esta lista e percorrida por quadro com o painel aberto, e o
// alvo Tizen ja bateu no teto de memoria do WebAssembly (TIZEN-MEMORIA.md).
// Sessenta recomendacoes sao mais do que qualquer pessoa real recebe antes de
// a retencao de 90 dias do servidor apagar as velhas.
#define REC_MAX           60
#define REC_CONTATOS_MAX  40
// Teto da lista de sugestoes — o MESMO SUG_MAX do servidor, que ja corta em 20.
// Este numero aqui nao e a regra, e a garantia de que uma resposta maior que a
// combinada nao escreve fora do vetor.
#define REC_SUGESTOES_MAX 20
// Quantos modelos prontos existem. Texto livre (modelo -1) e etapa posterior.
#define REC_MODELOS        6

typedef struct {
  long long id;        // id no servidor; e tambem o cursor
  long long criado;    // epoch em segundos
  char de[96];         // "trakt:<slug>" | "nuvio:<sub>"
  char deNome[64];     // nome de exibicao de quem mandou
  // FOTO DE PERFIL DE QUEM MANDOU. Vazio e estado NORMAL, e nao falha: a conta
  // Nuvio nao expoe foto na verificacao de identidade (ver idNuvio no servidor)
  // e quem entrou por codigo costuma cair nesse caso. Vazio desenha a inicial
  // num disco colorido, como perfilsel.c ja faz nos perfis sem foto.
  //
  // 256 E NAO 512 como o `poster`: sao 60 linhas guardadas, e cada byte aqui
  // custa 60 vezes na memoria estatica do modulo (a nota de REC_MAX explica por
  // que isso importa no alvo Tizen). O avatar do Trakt e uma URL de
  // walter.trakt.tv ou do Gravatar — as duas familias ficam bem abaixo disso.
  char deAvatar[256];
  char imdb[24];
  char tipo[8];        // "movie" | "series"
  char titulo[160];
  char poster[512];
  char ano[16];
  int  modelo;         // indice do template; -1 = texto livre
  char texto[72];      // so quando modelo == -1
  // NOTA DO IMDb EM CENTESIMOS (83 = 8,3), a MESMA unidade do `nota` do
  // CatItem — la ela nasce de `imdbRating * 10` (descoberta.c) e e desenhada
  // como `nota/10 , nota%10`. 0 = desconhecida, e o selo some.
  //
  // VIAJA COM A RECOMENDACAO, e nao e procurada no catalogo de quem recebe: o
  // titulo recomendado pode nao estar no catalogo dele, que e precisamente o
  // caso que uma recomendacao existe para cobrir.
  int  nota;
  int  visto;          // 0 = ainda conta para o selo
} RecItem;

typedef struct {
  char id[96];
  char nome[64];
  char avatar[256];    // vazio = desenhar a inicial; ver RecItem.deAvatar
  char origem[8];      // "trakt" | "nuvio"
} RecContato;

// GENTE QUE A PESSOA TALVEZ CONHECA, e como o servico chegou ate ela.
//
// AS DUAS FONTES E O QUE ELAS NAO PRECISAM. Nenhuma das duas manda um dado novo
// para fora da TV:
//   "trakt" — quem ela ja segue no Trakt e tambem usa o servico. A lista de
//             slugs que vai no pedido e a MESMA que /v1/contatos/trakt ja
//             recebia; o proprio Trakt a publica.
//   "amigo" — alcancavel por um contato que ela ja tem. E um JOIN no servidor
//             sobre a tabela de contatos; o cliente nao manda nada e nao recebe
//             a lista de amigos de ninguem — so o NOME de um intermediario.
//
// EM AMBAS, SO APARECE QUEM ACEITOU APARECER. A tela de consentimento promete
// isso com todas as letras, e uma excecao aqui faria daquela frase uma mentira.
typedef struct {
  char id[96];
  char nome[64];
  char avatar[256];
  char origem[8];      // "trakt" | "amigo"
  char viaNome[64];    // nome do contato em comum; so em "amigo"
} RecSugestao;

// 0 quando o pacote saiu SEM NUVIO_REC_URL. Nesse caso nada mais aqui faz
// coisa alguma, e quem desenha deve esconder a superficie inteira.
int  recomenda_ativo(void);

// Le cursor e cache do disco. Chamar UMA vez no arranque, depois de
// dados_iniciar. Nao abre conexao.
void recomenda_iniciar(void);

// Liga a sondagem. A PRIMEIRA chamada cria o fio e faz um ciclo imediato; as
// seguintes NAO FAZEM NADA.
//
// Ela e idempotente porque quem a chama e o bloco `homePronta` de app.c, que
// roda UMA VEZ POR QUADRO — a mesma posicao de atualizacao_verificar(), e pela
// mesma razao. Sem essa garantia a sondagem de 60 s viraria uma a cada 200 ms,
// que e o passo do laco do fio. Para pedir um ciclo fora de hora, use
// recomenda_pedir_agora.
void recomenda_verificar(void);

// Pede um ciclo AGORA, mesmo com o fio ja de pe: ao abrir a aba Social (para
// nao mostrar lista velha) e ao entrar na tela de escolher o amigo. Nao
// bloqueia — so acorda o fio, que responde em ate 200 ms.
void recomenda_pedir_agora(void);

// Quantas recomendacoes ainda nao vistas — o numero do selo.
int  recomenda_n_novas(void);

int  recomenda_n(void);
// Copia da linha `i` (mais nova primeiro) para `saida`. 1 quando copiou.
//
// COPIA E NAO PONTEIRO de proposito: a lista vive atras de um mutex que o fio
// de rede reescreve, e devolver ponteiro para dentro dela seria entregar ao
// desenho memoria que muda debaixo dele — o mesmo defeito que derrubou
// salvospainel.c quando ele apontava para dentro do CatItem.
int  recomenda_item(int i, RecItem *saida);

// Marca TODAS como vistas: apaga o selo na hora, grava, e enfileira o aviso ao
// servidor para os outros aparelhos concordarem. Chamar quando a aba Social
// abre.
void recomenda_marcar_vistas(void);

// Enfileira um envio; o fio de rede o despacha. 1 quando entrou na fila (ha
// espaco e os dados sao suficientes), 0 quando nem isso. O resultado de
// verdade sai em recomenda_envio_estado().
int  recomenda_enviar(const CatItem *ci, const char *paraId, int modelo,
                      const char *texto);

enum { REC_ENVIO_NADA = 0, REC_ENVIO_INDO, REC_ENVIO_OK, REC_ENVIO_FALHA };
int  recomenda_envio_estado(void);
void recomenda_envio_limpar(void);

// Copia ate `max` contatos. Devolve quantos copiou.
int  recomenda_contatos(RecContato *saida, int max);

// --- APARECER PARA OUTRAS PESSOAS -------------------------------------------
//
// O PADRAO E NAO, E O PADRAO NAO E "NAO": sao TRES estados, e a diferenca entre
// os dois primeiros e a tela inteira. "Nao perguntado" e o que faz a aba Social
// abrir com a pergunta; "nao" e uma resposta que a pessoa deu e que nao se
// pergunta de novo. Um sinalizador de dois valores confundiria "ela recusou"
// com "ela ainda nao viu", e a segunda leitura autoriza perguntar toda vez —
// que e como um consentimento vira um obstaculo a ser clicado sem ler.
//
// O EFEITO NO SERVIDOR E UM SO: se a pessoa pode APARECER na lista de sugestoes
// de outra gente. Ele nao governa receber recomendacao, nao governa o codigo de
// pareamento e nao governa os contatos que ela ja tem. Recusar nao tira nada
// dela — e por isso a tela pode dizer isso sem ressalva.
enum { REC_APARECER_NAO_PERGUNTADO = 0, REC_APARECER_NAO, REC_APARECER_SIM };
int  recomenda_aparecer(void);

// Grava a resposta no aparelho NA HORA e enfileira o aviso ao servidor. Grava
// primeiro de proposito: a pergunta nao pode voltar na proxima abertura so
// porque a rede estava fora, e um "sim" que nao chegou ao servidor e uma pessoa
// que nao apareceu — que e o lado seguro do erro.
void recomenda_responder_aparecer(int sim);

// --- SUGESTOES ---------------------------------------------------------------
int  recomenda_n_sugestoes(void);
// Copia da sugestao `i`. 1 quando copiou. COPIA e nao ponteiro, pelo mesmo
// motivo de recomenda_item.
int  recomenda_sugestao(int i, RecSugestao *saida);

// Enfileira `POST /v1/contatos/sugerido`: vira contato nos dois sentidos. Tira
// da lista local na hora (a resposta chega no proximo ciclo) e devolve 1 quando
// entrou na fila. O servidor RECALCULA as sugestoes antes de vincular — um id
// que nao esta nelas volta 403, entao esta rota nao e "vincule-me a qualquer
// um".
int  recomenda_adicionar_sugerido(const char *id);

// Frase de origem JA PRONTA de uma sugestao ("Segue no Trakt", "Amigo de
// Gustavo"). Nunca NULL. Mora aqui, e nao em quem desenha, porque a regra de
// "origem amigo sem viaNome" tem de ter UMA resposta.
void rec_sugestao_origem(char *dst, size_t tam, const RecSugestao *s);

// --- QUEM SOU EU, E COMO UM AMIGO ME ACHA -----------------------------------
//
// O CODIGO DE PAREAMENTO E A UNICA PORTA PARA QUEM NAO USA TRAKT, e ele nao
// existia no cliente: `POST /v1/eu` sempre devolveu `{id, nome, codigo}` e o
// cliente lia so o `id`. Sem o codigo na mao, a tela de "adicionar amigo" nao
// tem o que ditar no telefone e o servico fica preso aos seguidos do Trakt que
// JA instalaram o app — que em 15/09/2026 eram zero.
//
// Ele tambem vai para o disco (`recomendacoes-eu.txt`), pela mesma razao que a
// lista de recomendacoes vai: a tela abre no primeiro quadro, antes de o fio de
// rede ter falado com o servidor, e "seu codigo: ......" piscando por 2 s le
// como defeito.
const char *recomenda_meu_codigo(void);

// Enfileira `POST /v1/contatos` com o codigo de um amigo (6 chars a-z0-9; o que
// nao for e descartado aqui, nao no servidor). 1 quando entrou na fila. O
// resultado sai em recomenda_vinculo_estado().
int  recomenda_vincular(const char *codigo);
enum { REC_VINC_NADA = 0, REC_VINC_INDO, REC_VINC_OK,
       REC_VINC_NAO_ACHOU,     // 404: ninguem tem esse codigo
       REC_VINC_EU_MESMO,      // 400: e o meu proprio
       REC_VINC_FALHA };
int  recomenda_vinculo_estado(void);
// Nome de quem acabou de virar contato. "" fora do estado REC_VINC_OK.
const char *recomenda_vinculo_nome(void);
void recomenda_vinculo_limpar(void);

// Refaz a varredura dos seguidos do Trakt (`POST /v1/contatos/trakt`) fora do
// arranque. Ela ja roda sozinha no primeiro ciclo; isto e o "procurar agora"
// para quando um amigo instalou o app depois. 1 quando enfileirou.
int  recomenda_procurar_trakt(void);
enum { REC_TRAKT_NADA = 0, REC_TRAKT_INDO, REC_TRAKT_PRONTO,
       REC_TRAKT_SEM_CONTA };   // nao ha Trakt ligado neste aparelho
int  recomenda_trakt_estado(void);
// Quantos viraram contato na ultima varredura. So faz sentido em REC_TRAKT_PRONTO.
int  recomenda_trakt_achados(void);
void recomenda_trakt_limpar(void);

// Enfileira `POST /v1/contatos/remover`. Apaga o vinculo NOS DOIS SENTIDOS e
// tambem as recomendacoes nao lidas que a pessoa mandou — e o "bloquear" deste
// servico. 1 quando entrou na fila.
int  recomenda_remover_contato(const char *id);

// Frase do modelo `i` em portugues (a chave de i18n). NULL fora da faixa.
const char *recomenda_modelo(int i);

// Frase JA PRONTA de uma recomendacao: o modelo traduzido, ou o texto livre
// como veio. Nunca NULL. Compartilhada com quem desenha a aba Social, para as
// duas superficies nunca divergirem na regra de "modelo -1 = texto livre".
const char *rec_frase(const RecItem *r);

// "há 2 h" — a frase INTEIRA passa por i18n como formato, nao montada de
// pedacos (mesma regra do "Salvo há 2 horas" de salvospainel.c).
void rec_quando_texto(char *dst, size_t tam, long long quandoS);

// --- AS TRES MARCAS DA LINHA, compartilhadas com quem desenha ----------------
//
// MORAM AQUI, e nao em salvospainel.c, porque as MESMAS tres aparecem em duas
// superficies: a linha da aba Social e o cartao que abre com o app. Duplicar o
// desenho faria as duas divergirem na primeira correcao — foi o que aconteceu
// com a frase do modelo antes de rec_frase existir.

// Disco do avatar em `a`: a foto quando ha URL, senao a INICIAL do nome sobre
// uma cor derivada do id. Mesma receita de perfilsel.c (soquete escuro, cor por
// cima, foto ou letra), sem a parte de GIF — a foto do Trakt nao anima.
void rec_avatar(GfxRect a, const char *url, const char *nome, const char *id,
                float alfa);

// Altura unica dos dois selos, e o vao entre eles. Ficam aqui porque quem
// desenha a linha precisa deles para centrar o texto ao lado.
#define REC_SELO_H    30.0f
#define REC_SELO_GAP  12.0f
// Largura da marca amarela do IMDb e o vao ate o numero. Tambem no header
// porque quem ancora o selo pela DIREITA (o card de Continuar assistindo,
// issue #87) precisa da largura total antes de desenhar.
#define REC_IMDB_W    52.0f
#define REC_IMDB_GAP   8.0f

// Selo de tipo ("Filme" / "Série") em `x,y`. Devolve a largura desenhada.
// `escuro` inverte as cores para o fundo claro do foco.
float rec_selo_tipo(float x, float y, const char *tipo, int escuro, float alfa);

// Selo do IMDb com a nota em centesimos. Devolve a largura, ou 0 com nota <= 0
// — quem chama nao precisa perguntar antes.
float rec_selo_imdb(float x, float y, int nota, int escuro, float alfa);

// Apaga cache, cursor e marca do cartao do aparelho. Chamar de
// sync_esquecer_usuario: recomendacao e tao pessoal quanto a lista de salvos.
void recomenda_esquecer(void);

// --- CARTAO DE ABERTURA ------------------------------------------------------
//
// Mesmo formato e mesmas regras do cartao de atualizacao.c: abre uma vez por
// recomendacao, so com a home de pe, e NUNCA por cima de quem esta assistindo.
void recomenda_mostrar_se_houver(void);
int  recomenda_aberta(void);
void recomenda_evento(const SDL_Event *e);
void recomenda_atualizar(float dt, Uint32 agora);
void recomenda_desenhar(Uint32 agora);

// IMDb que o dono pediu para abrir (OK no cartao), ou NULL. Consumido uma vez.
// Quem resolve o id e abre o detalhe e o roteador (app.c) — este modulo nao
// conhece detail.c, pela mesma razao que salvospainel.c nao conhece.
const char *recomenda_pediu_abrir(void);

#endif
