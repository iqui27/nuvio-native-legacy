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
#include "badges.h"
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
#define REC_CONTATO_IDS    3

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
  // O APELIDO que o amigo escolheu ("" = nunca escolheu, ou servidor antigo) e
  // o SELO que o servidor deu a ele ("criador" | ""). O selo nunca e deduzido
  // aqui: qualquer um pode escolher o apelido de quem fez o app.
  char apelido[32];
  char selo[12];
  // IDENTIDADES LIGADAS (F08, servidor com migracao 008): "trakt:<slug>" que
  // este amigo provou ser dele e deixou os amigos verem. E por elas que o feed
  // do Trakt dele se junta ao nosso. Servidor antigo: nIds = 0.
  char ids[REC_CONTATO_IDS][80];
  int  nIds;
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
  char selo[12];       // ver RecContato.selo
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
// O mesmo disco com o ESTILO da letra escolhido por quem chama (-1 = a regra
// de rec_avatar). A ilha do relogio usa: rosto de 36 com a inicial em negrito
// pequeno, e o de 150 do modal com a letra grande (mockup de 02/10).
void rec_avatar_estilo(GfxRect a, const char *url, const char *nome, const char *id,
                       float alfa, int estilo);

// "Amigo #12" e a RESERVA de quem nao tem nome (o servidor e rec_nome_exibicao
// a criam), nao um nome nem um apelido: quem desenha a pessoa pela identidade
// trata como vazio. 1 quando `s` e essa reserva (em qualquer idioma: so olha o
// "#<numero>" no fim).
int  rec_nome_reserva(const char *s);
// AS DUAS LINHAS DE QUEM E A PESSOA: o nome em cima e "@apelido" embaixo. Sem
// um dos dois sobe o outro sozinho (sem buraco); sem nenhum, "Sem nome". `l2`
// sai "" quando nao ha segunda linha.
void rec_identidade(const char *nome, const char *apelido, char *l1, size_t n1,
                    char *l2, size_t n2);
// O SELO DE QUEM FEZ O APP ("Criador"), ao lado do nome: pilula de BADGE_H no
// acento a 18 %, o icone de verificado e o texto no tom do acento. Desenha so
// com `selo` == "criador" (o servidor decide); devolve a largura, 0 sem selo.
float rec_selo_pessoa_largura(const char *selo);
float rec_selo_pessoa(float x, float y, const char *selo, float alfa);

// Altura unica dos dois selos, e o vao entre eles. Ficam aqui porque quem
// desenha a linha precisa deles para centrar o texto ao lado.
// DESDE 21/09/2026 SAO OS DA TABELA UNICA (badges.h): 28 px, e nao 30 — o
// selo do IMDb da home, do card de Continuar, da aba Social e do detalhe
// passaram a ser o MESMO desenho (badge_imdb), e as medidas moram la.
#define REC_SELO_H    BADGE_H
#define REC_SELO_GAP  BADGE_GAP
// Largura da marca amarela do IMDb e o vao ate o numero. Tambem no header
// porque quem ancora o selo pela DIREITA (o card de Continuar assistindo,
// issue #87) precisa da largura total antes de desenhar.
#define REC_IMDB_W    BADGE_IMDB_W
#define REC_IMDB_GAP  BADGE_IMDB_GAP

// Selo de tipo ("Filme" / "Série") em `x,y`. Devolve a largura desenhada.
// `escuro` inverte as cores para o fundo claro do foco.
float rec_selo_tipo(float x, float y, const char *tipo, int escuro, float alfa);

// Selo do IMDb com a nota em centesimos. Devolve a largura, ou 0 com nota <= 0
// — quem chama nao precisa perguntar antes.
float rec_selo_imdb(float x, float y, int nota, int escuro, float alfa);

// =============================================================================
// ENTRE AMIGOS ALEM DO TRAKT: perfil publico opcional, busca, pedidos de
// amizade, bloqueio e atividade dos amigos.
//
// O modelo de privacidade inteiro esta em docs/SOCIAL-PRIVACIDADE.md e e imposto
// no servidor (servidor/recomendacoes/src/amigos.js); o que este lado faz e
// NAO MANDAR NADA que a pessoa nao ligou. As tres coisas que saem, e so quando
// ela liga cada uma:
//   perfil publico   apelido, bio curta, generos, foto (opcional), "vistos
//                    recentemente" (opcional) — para quem PROCURA por ela;
//   atividade        o que assistiu (e, no nivel 2, "assistindo agora") —
//                    so para AMIGOS MUTUOS;
//   nada mais        nunca e-mail, id de conta, addon, IP, aparelho.
// Tudo nasce DESLIGADO, e desligar apaga do servidor na hora.
// =============================================================================

// Ids de genero aceitos pelo servidor (lista fechada). O rotulo passa por i18n
// em quem desenha; o id e o que viaja.
#define REC_GENEROS_N 14
const char *rec_genero_id(int i);
const char *rec_genero_rotulo(int i);   // chave de i18n em portugues

#define REC_APELIDO_MAX 20
#define REC_BIO_MAX     80

// MEU perfil, como esta neste aparelho. `publicado` e o "Perfil pesquisavel".
typedef struct {
  int  publicado;             // 0 = ninguem me acha (padrao)
  char apelido[REC_APELIDO_MAX + 4];
  char bio[REC_BIO_MAX + 4];
  unsigned generos;           // mascara: bit i = rec_genero_id(i)
  int  foto;                  // 1 = mostrar a foto da conta
  int  recentes;              // 1 = "vistos recentemente" no cartao publico
  int  ativ;                  // atividade para AMIGOS: 0 nada, 1 assistiu, 2 + agora
} RecPerfil;

int  recomenda_perfil(RecPerfil *saida);
// Grava no aparelho NA HORA e enfileira o aviso ao servidor (o "sim" so vale
// quando ele souber; a falha de rede repete no proximo ciclo). Exige apelido
// de 2+ letras — devolve 0 sem apelido. Publicar tambem liga o "aparecer".
int  recomenda_perfil_publicar(const RecPerfil *p);
// Despublica: o servidor apaga apelido/bio/generos/foto e sorteia outro handle.
void recomenda_perfil_despublicar(void);
// So o nivel de atividade para amigos (0/1/2), sem mexer no perfil publico.
void recomenda_atividade_nivel(int nivel);
// "Apagar meus dados sociais": perfil, atividade e pedidos enviados. Nao mexe
// em contatos nem recomendacoes.
void recomenda_apagar_dados_sociais(void);
// 1 quando o perfil esta publicado neste aparelho.
int  recomenda_pesquisavel(void);

// UMA PESSOA VISTA POR ESTRANHO: so o que ela escolheu mostrar.
typedef struct {
  char pub[16];               // handle opaco; NUNCA o id da conta
  char apelido[REC_APELIDO_MAX + 12];
  char avatar[256];
  char bio[REC_BIO_MAX + 12];
  unsigned generos;
  char relacao[12];           // "" | "amigo" | "enviado" | "recebido"
  int  emComum;               // titulos em comum (so nas sugestoes de gosto)
  // Ultimo dos "vistos recentemente" PUBLICOS (so na lista da comunidade, e so
  // de quem ligou "Mostrar o que assisti recentemente"). Nunca "assistindo
  // agora": isso e so para amigo.
  char vendo[80];
  // O NOME so vem de quem ja e AMIGO (servidor: amigos.js, cartao); para
  // estranho ele chega vazio e a linha mostra so o apelido. Servidor antigo:
  // vazio sempre. `selo` = "criador" ou "" (ver RecContato.selo).
  char nome[64];
  char selo[12];
} RecPessoa;

#define REC_BUSCA_MAX    10
// A lista da comunidade vem em paginas de 20 (servidor); o aparelho guarda ate
// duas paginas e oferece "Ver mais" enquanto o servidor disser que ha.
#define REC_COMUNIDADE_MAX 40
#define REC_PEDIDOS_MAX  20
#define REC_BLOQ_MAX     20

enum { REC_SOC_NADA = 0, REC_SOC_INDO, REC_SOC_OK,
       REC_SOC_FALHA,
       REC_SOC_LIMITE,         // 429: muitas buscas/pedidos, tente mais tarde
       REC_SOC_NAO_ACHOU,      // 404
       REC_SOC_SEM_APELIDO,    // 409: pedir amizade exige um apelido
       REC_SOC_CURTA,          // busca com menos de 3 letras (nem sai do aparelho)
       REC_SOC_NEGADO };       // explicit 403; different from unavailable source/404

// UMA OPERACAO SOCIAL POR VEZ (a mesma disciplina de vincular/remover): todas
// devolvem 1 quando entraram na fila e o resultado sai em recomenda_soc_estado().
int  recomenda_buscar(const char *texto);        // apelido (3+) ou codigo de 6
int  recomenda_sugeridos_gosto(void);            // ver nota em recomenda.c
// COMUNIDADE NUVIO NATIVE: todo mundo que publicou o perfil (servidor:
// POST /v1/perfis/comunidade). `pagina` 0 recomeca a lista; as seguintes
// acrescentam ao fim. So responde algo para quem tambem e pesquisavel.
int  recomenda_comunidade(int pagina);
int  recomenda_comunidade_mais(void);            // 1 = o servidor tem mais pessoas
int  recomenda_comunidade_pagina(void);          // ultima pagina lida
int  recomenda_comunidade_fechada(void);         // 1 = servidor disse "ligue o perfil"
int  recomenda_ver_perfil(const char *pub);
int  recomenda_pedir_amizade(const char *pub);
int  recomenda_aceitar(const char *pub);
int  recomenda_recusar(const char *pub);
int  recomenda_cancelar_pedido(const char *pub);
int  recomenda_bloquear(const char *pubOuId);
int  recomenda_desbloquear(const char *pub);
int  recomenda_listar_pedidos(void);
int  recomenda_listar_bloqueados(void);
int  recomenda_soc_estado(void);
void recomenda_soc_limpar(void);

int  recomenda_n_achados(void);
int  recomenda_achado(int i, RecPessoa *saida);
// 1 = veio de uma busca, 2 = das sugestoes de gosto, 3 = da comunidade. Serve
// para o titulo da lista.
int  recomenda_achados_origem(void);
int  recomenda_cartao(RecPessoa *saida);         // o ultimo perfil aberto
// Titulos "vistos recentemente" do cartao aberto (so vem se a pessoa ligou).
int  recomenda_cartao_n_recentes(void);
int  recomenda_cartao_recente(int i, char *titulo, size_t tam);
int  recomenda_n_pedidos(void);                  // pedidos RECEBIDOS pendentes
int  recomenda_pedido(int i, RecPessoa *saida);
int  recomenda_n_bloqueados(void);
int  recomenda_bloqueado(int i, char *pub, size_t tp, char *nome, size_t tn);

// --- ATIVIDADE ---------------------------------------------------------------
//
// O player avisa (recomenda_atividade_passo / _fim) e ESTE MODULO decide se
// algo sai: sem o interruptor ligado nao ha nem fila. O envio e por titulo
// (imdb, titulo, tipo, ano, nota) — sem episodio, sem tempo, sem fonte.
void recomenda_atividade_passo(const CatItem *ci, int tocando);
void recomenda_atividade_fim(const CatItem *ci, int concluiu);

// A FILEIRA "ENTRE AMIGOS". Recebe os `nTrakt` itens que o Trakt ja montou em
// `itens` (capacidade `max`) e devolve a lista UNIDA: os amigos Nuvio (nosso
// servico, so amigos mutuos que ligaram a atividade) entram com o nome e a foto
// do amigo, cada titulo aparece UMA vez (o mais recente) e as duas fontes se
// alternam. Sincrona e de rede — so chamar de dentro do fio da descoberta.
// Atividade que o amigo Nuvio `id` (o socialSlug do item) compartilhou, da ultima
// leitura do feed — sem rede. Devolve quantos copiou em `titulos`.
typedef struct { char imdb[24]; char titulo[160]; char tipo[8]; int agora; long long criado; } RecAtivAmigo;
int  recomenda_amigo_atividades(const char *id, RecAtivAmigo *saida, int max);
int  recomenda_social_mesclar(CatItem *itens, int nTrakt, int max);
// Funde `novos` (n) em `itens`(nTrakt) SEM rede: a regra de uniao, separada
// para o teste. Devolve o novo total.
int  rec_social_unir(CatItem *itens, int nTrakt, const CatItem *nuvio, int nNuvio, int max);

// Apaga cache, cursor e marca do cartao do aparelho. Chamar de
// sync_esquecer_usuario: recomendacao e tao pessoal quanto a lista de salvos.
void recomenda_esquecer(void);
// Changes when the account/profile is forgotten. UI caches consume it on their thread.
unsigned recomenda_geracao(void);

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


// =============================================================================
// REDESENHO DO SOCIAL (02/10/2026) — API para quem desenha. Contrato completo
// com o servidor em docs/social-contrato.md; fontes em docs/social-fontes.md.
//
// TUDO AQUI SEGUE A DISCIPLINA DO RESTO DO MODULO: nenhuma funcao bloqueia,
// pedidos so ENFILEIRAM e acordam o fio de rede, leituras COPIAM de tras do
// mutex. Sem NUVIO_REC_URL tudo devolve 0/vazio.
//
// IDENTIDADE: cada PERFIL Nuvio e uma pessoa. O fio manda `X-Nuvio-Perfil` com
// o profile_index quando o perfil ativo NAO e o principal (o principal continua
// `nuvio:<sub>`). Trocar de perfil = trocar de pessoa: o modulo esquece tudo,
// como em sair da conta. Em /v1/eu vao o nome e a foto do perfil ativo.
// =============================================================================

// Nome que a TV escreve para uma pessoa. `nome` quando ha; senao o slug (id
// "trakt:<slug>"); senao "Amigo #<n>" (n curto, derivado do id). NUNCA o id
// cru — era assim que um amigo por codigo aparecia como "5269539e-...".
void rec_nome_exibicao(char *dst, size_t tam, const char *nome, const char *id);

// O meu nome como os amigos o veem (resposta do servidor; "" antes do 1o ciclo).
const char *recomenda_meu_nome(void);
// O id social deste perfil ("nuvio:<sub>" ou "nuvio:<sub>:<n>"), "" antes do
// primeiro /v1/eu. O Watch Together reconhece a propria cadeira na sala por ele.
const char *recomenda_meu_id(void);
// O que eu digitei como nome de exibicao ("" = uso o nome do perfil).
const char *recomenda_minha_exibicao(void);
// Enfileira POST /v1/eu/nome. "" volta ao nome do perfil. 1 = enfileirou.
// Resultado: recomenda_meu_nome() muda no ciclo seguinte.
int  recomenda_definir_nome(const char *nome);

// QUEM VE O QUE EU ASSISTO. Tres respostas e o "nao perguntado", pela mesma
// razao dos tres estados de REC_APARECER_*: a tela pergunta enquanto for
// REC_ALCANCE_NAO_PERGUNTADO, e nada sai da TV ate a pessoa responder.
//   NINGUEM (0)   nada e enviado; o servidor apaga o que havia.
//   AMIGOS (1)    contatos veem atividade, "assistindo agora", agregados e
//                 reacoes; "gosto parecido" exige 1+ dos dois lados.
//   AMIGOS2 (2)   tambem amigos de amigos (sem foto e sem o id da conta).
// E SEPARADO do "aparecer"/descobrivel (quem pode me ACHAR) e do antigo
// RecPerfil.ativ (rota velha /v1/amigos/atividade, mantida para TVs no ar).
enum { REC_ALCANCE_NAO_PERGUNTADO = -1, REC_ALCANCE_NINGUEM = 0,
       REC_ALCANCE_AMIGOS = 1, REC_ALCANCE_AMIGOS2 = 2 };
int  recomenda_alcance(void);
// Grava no aparelho NA HORA e enfileira POST /v1/alcance. Descer para 0 limpa
// a fila de atividade que ainda nao saiu.
void recomenda_responder_alcance(int nivel);


// LIGACAO COM O MODULO DO PLAYER (src/atividade.c, branch agente/reacao): ele so
// envia com atividade_definir_permitido(nivel) > 0. No merge, uma linha no
// arranque: recomenda_ao_mudar_alcance(atividade_definir_permitido). O aviso
// chama na hora com o nivel atual e de novo a cada mudanca (resposta da pessoa,
// adocao do servidor, sair da conta), SEMPRE fora do mutex deste modulo.
// "Nao perguntado" chega como 0.
void recomenda_ao_mudar_alcance(void (*fn)(int nivel));

// Os cabecalhos do servico para OUTRO modulo que fale com ele (atividade.c):
// Authorization, X-Nuvio-Auth e, num perfil que nao e o principal,
// X-Nuvio-Perfil — sem este, o evento cairia na pessoa do perfil principal.
// `cab` precisa de 4 posicoes; os buffers sao do chamador. 0 = sem identidade.
int  recomenda_cabecalhos(const char **cab, char *aut, size_t na, char *via, size_t nv,
                          char *perfil, size_t np);

// --- ATIVIDADE DO PLAYER (contrato com o agente do player) --------------------
//
// O player preenche e chama recomenda_atividade(); o modulo decide se sai (so
// com alcance >= 1). "progresso" seguidos do mesmo titulo se fundem na fila
// (somando `seg`), entao chamar a cada minuto e barato.
typedef struct {
  char ev[12];        // "inicio" | "progresso" | "fim" | "abandono" | "reacao" | "salvo"
  char imdb[24];      // "tt..." (obrigatorio)
  char midia[8];      // "movie" | "series"
  char titulo[160];
  char poster[512];   // so sai no feed se for de host conhecido (servidor)
  int  temporada, episodio;
  int  pct;           // 0-100
  int  seg;           // segundos assistidos NESTE trecho (desde o evento anterior)
  int  reacao;        // 1 | 0 | -1, so em ev "reacao"
  long long rec;      // id da recomendacao de origem (RecItem.id) ou 0
} RecAtiv;
int  recomenda_atividade(const RecAtiv *a);   // 1 = enfileirou

// --- FEED UNIFICADO ------------------------------------------------------------
enum { REC_FONTE_NUVIO = 1, REC_FONTE_TRAKT, REC_FONTE_SIMKL, REC_FONTE_LETTERBOXD };
enum { REC_ACAO_INICIO = 1,     // comecou (nosso) / "assistindo agora" (Trakt)
       REC_ACAO_FIM,            // terminou / "assistiu"
       REC_ACAO_ABANDONO,
       REC_ACAO_REACAO,         // `reacao` 1/0/-1
       REC_ACAO_SALVO,
       REC_ACAO_NOTA,           // nota de tracker (`nota` 0-100)
       REC_ACAO_JUNTO };        // assistiram juntos (Watch Together); `com` = os outros
typedef struct {
  int  fonte, acao;
  int  agora;           // explicit watching response; a check-in alone is not live
  char pessoa[96];      // "nuvio:..", "trakt:<slug>" ou "pub:<handle>" (amigo de amigo)
  char pessoaNome[64];  // ja pronto (rec_nome_exibicao)
  char pessoaAvatar[256];
  int  grau;            // 1 amigo, 2 amigo de amigo
  char via[64];         // grau 2: nome do amigo em comum
  char imdb[24];
  char midia[8];        // "movie" | "series"
  char titulo[160];
  char poster[512];
  int  temporada, episodio, pct;
  long long quando;     // epoch s; 0 = desconhecido (vai para o fim)
  int  reacao;          // so REC_ACAO_REACAO
  int  nota;            // so REC_ACAO_NOTA
  long long id;         // id do evento no nosso servidor (0 nas outras fontes)
  char com[96];         // so REC_ACAO_JUNTO: "Rafa, Fabi" (quem tambem aparece)
} RecEvento;
#define REC_FEED_MAX 50

// Pede GET /v1/feed agora (ETag/304). O feed tambem e relido a cada 10 min com
// a lista de contatos e fica em disco: a tela abre com o ultimo no 1o quadro.
void recomenda_feed_pedir(void);
int  recomenda_feed_n(void);
int  recomenda_feed_item(int i, RecEvento *saida);   // copia; mais novo primeiro

// O FEED DA TELA: o nosso (cache) + os itens que o Trakt ja montou
// (trakt_social), num formato so, sem duplicata, mais novo primeiro. Sem rede.
// `quandoTrakt` e o epoch de cada item do Trakt ou NULL (trakt_social ainda nao
// le `watched_at`: esses vao para o fim). Devolve quantos copiou.
int  recomenda_feed_unido(RecEvento *saida, int max, const CatItem *trakt,
                          const long long *quandoTrakt, int nTrakt);
// As pecas, publicas para o teste e para fontes futuras (Simkl/Letterboxd):
// converte um item de trakt_social; 0 se nao serve (sem imdb/pessoa).
int  rec_evento_de_trakt(const CatItem *ci, long long quando, RecEvento *saida);
// Funde `src` em `dst` (n itens, capacidade max): DEDUPE por pessoa + imdb +
// acao + midia + temporada/episodio (INICIO e FIM NAO se fundem) com |dt| <= 1 h — ou
// qualquer dt quando um dos dois nao tem hora. Na fusao fica o do NOSSO
// servidor (tem reacao, grau, capa), completado com o que faltar. Ordena por
// `quando` decrescente (0 por ultimo). Devolve o novo n.
int  rec_eventos_unir(RecEvento *dst, int n, const RecEvento *src, int nsrc, int max);

// --- PERFIL DO AMIGO (GET /v1/amigo?id=) ---------------------------------------
enum { REC_REC_ENTREGUE = 0, REC_REC_ABERTA, REC_REC_COMECOU, REC_REC_TERMINOU,
       REC_REC_REAGIU };
#define REC_AMIGO_GOSTOU 10
#define REC_AMIGO_RECS   20
typedef struct {
  char id[96];          // o que foi pedido (contato) ou "pub:<handle>"
  char nome[64];
  char avatar[256];     // vazio para amigo de amigo
  int  grau;            // 1 contato, 2 amigo de amigo
  char via[64];         // grau 2: amigo em comum
  long long desde;      // contato desde (epoch s); 0 em grau 2
  char origem[12];      // "codigo" | "trakt" | "sugestao" | "pedido" | ""
  int  compartilha;     // 0: a pessoa nao compartilha atividade comigo
  // agregados do mes corrente (so com compartilha)
  int  temMes; char mes[8]; long long seg; int filmes, series;
  // assistindo agora (some 15 min sem evento)
  int  temAgora; RecEvento agora;
  int  nGostou; RecEvento gostou[REC_AMIGO_GOSTOU];
  // recs que EU mandei para ela, mais nova primeiro (so grau 1)
  int  nRecs;
  struct { long long id, criado; char imdb[24], tipo[8], titulo[160], poster[512];
           int estado, temReacao, reacao, terminou;
           // resposta DIRETA de quem recebeu (POST /v1/rec/resposta): a mensagem
           // curta ("" = nenhuma) e o epoch em que respondeu (0 = nao respondeu).
           char resposta[64]; long long respondido; } recs[REC_AMIGO_RECS];
  // gosto parecido: % de titulos com a MESMA reacao (so se os dois compartilham)
  int  temGosto, gostoTotal, gostoIguais, gostoPct;
  // F08: o detalhe da comparacao. temCmp = 0 quando o servidor nao o mandou
  // (antigo) — a tela mostra "desconhecido", nunca zero.
  int  temCmp, filmesTotal, filmesIguais, seriesTotal, seriesIguais;
  int  comumFilmes, comumSeries, cobEu, cobEle;
} RecAmigo;

// Enfileira a leitura. Se o cache em disco for desta pessoa, ele ja fica
// disponivel em recomenda_amigo() antes da rede. Estado em recomenda_amigo_estado.
// Funde o estado de assistida/resposta de um corpo de GET /v1/rec em recresp.h.
// E o que a rede faz a cada sondagem; exposta para o teste.
void recomenda_fundir_respostas(const char *corpo);
int  recomenda_amigo_pedir(const char *id);
int  recomenda_amigo(RecAmigo *saida);    // 1 = ha dados (cache ou novos)
int  recomenda_amigo_estado(void);        // REC_SOC_* (NADA/INDO/OK/FALHA/NAO_ACHOU/NEGADO)

// --- IDENTIDADE UNIFICADA (F08) -----------------------------------------------
// Contrato: docs/releases/1.8.0/F08-SOCIAL-IDENTIDADE.md. A pessoa canonica e o
// PERFIL Nuvio; o Trakt e uma identidade LIGADA a ele, por pedido explicito, com
// as duas provas (os dois tokens) no mesmo pedido. Servidor sem o recurso
// ("identidade1" em /v1/eu): tudo aqui fica em REC_IDENT_INDISPONIVEL e a tela
// nao oferece nada.
enum { REC_IDENT_INDISPONIVEL = 0,  // servidor antigo, ou /v1/eu ainda nao respondeu
       REC_IDENT_SEM_TRAKT,         // nao ha as duas contas no aparelho: nada a unir
       REC_IDENT_PODE_UNIR,         // Trakt e conta Nuvio aqui; Trakt ainda nao ligado
       REC_IDENT_UNIDA };           // o Trakt esta ligado a este perfil
enum { REC_IDENT_OP_NADA = 0, REC_IDENT_OP_INDO, REC_IDENT_OP_OK,
       REC_IDENT_OP_CONFLITO,       // essa conta ja e de outro perfil (409)
       REC_IDENT_OP_FALHA,
       REC_IDENT_OP_SEM_SERVICO,    // 501: o servidor nao tem SIMKL_CLIENT_ID
       REC_IDENT_OP_RECUSADO };     // 401/403: o servidor nao aceitou a prova
int  recomenda_identidade_situacao(void);
int  recomenda_identidade_op(void);
const char *recomenda_identidade_trakt(void);   // slug ligado, "" se nao ha
int  recomenda_identidade_unir(void);           // 1 = enfileirou
int  recomenda_identidade_separar(void);        // 1 = enfileirou
void recomenda_identidade_op_limpar(void);

// SIMKL E LETTERBOXD (linhas da aba Amigos). Um servico por vez na fila (um
// pedido unico em voo, qualquer que seja o servico), um resultado por servico.
//  - Simkl: o token e o de Ajustes (nunca pedido de novo, nunca logado). So e
//    oferecido com login do Simkl nesta TV ou se ja ha um ligado (para desligar).
//    501 do servidor = REC_IDENT_E_SEM_SERVICO: a linha some depois do aviso.
//  - Letterboxd: o usuario e DECLARADO; o servidor nao confere e so a propria
//    pessoa o ve.
enum { REC_IDENT_TRAKT = 0, REC_IDENT_SIMKL, REC_IDENT_LETTERBOXD, REC_IDENT_N };
enum { REC_IDENT_E_INDISPONIVEL = 0, // servidor antigo, sem login do Simkl, ou nao registrado
       REC_IDENT_E_SEM_SERVICO,      // o servidor respondeu 501 (so Simkl)
       REC_IDENT_E_PODE,             // da para ligar
       REC_IDENT_E_LIGADO };
int  recomenda_identidade_estado(int prov);      // so SIMKL e LETTERBOXD
int  recomenda_identidade_op_de(int prov);       // REC_IDENT_OP_* do servico
void recomenda_identidade_op_limpar_de(int prov);
const char *recomenda_identidade_usuario_letterboxd(void);  // "" se nao ha
int  recomenda_identidade_simkl_unir(void);      // 1 = enfileirou
int  recomenda_identidade_simkl_separar(void);
// Normaliza (minusculas, a-z 0-9 _) e enfileira. 0 se invalido (2..30) ou ocupado.
int  recomenda_identidade_letterboxd_declarar(const char *usuario);
int  recomenda_identidade_letterboxd_separar(void);
// Id de uma pessoa vinda de outra fonte ("trakt:<slug>") -> o id do CONTATO
// que provou ser ela. 1 quando trocou. Sem ids do servidor (antigo), 0 sempre.
int  recomenda_pessoa_canonica(const char *id, char *dst, size_t tam);
int  rec_contatos_canonica(const RecContato *c, int n, const char *id, char *dst, size_t tam);
// Le "ids":["trakt:x",...] de um contato do servidor em [p,f). Exposta para o teste.
int  rec_contato_ids(const char *p, const char *f, RecContato *c);

#endif
