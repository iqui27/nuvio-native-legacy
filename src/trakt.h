// Continue Assistindo vindo do Trakt.
//
// Antes o historico era um retrato exportado do app web (watchProgressItems do
// localStorage) e congelava no momento da exportacao. O dono ja usa o Trakt
// como fonte de progresso (traktSettings.watchProgressSource = "trakt"), entao
// perguntar direto a ele e o que mantem a fileira viva — e o que faz o app
// nativo concordar com o app web sem os dois se escreverem.
//
// CREDENCIAIS ficam em art/trakt.txt ("token<TAB>clientId"), arquivo do dono:
// tratar como segredo, nao versionar. Sem o arquivo o modulo simplesmente nao
// faz nada e a fileira cai no que veio no pacote.
#include "vistoep.h"
#ifndef NV_TRAKT_H
#define NV_TRAKT_H
#include "catalogo.h"
#include "perfil.h"
#include <stddef.h>

int  trakt_carregar(const char *dirArte);   // 1 quando ha credencial

// Monta os tres cabecalhos que TODO pedido ao Trakt exige (token, versao da
// api e a chave do aplicativo) em `cab`, que precisa ter 4 posicoes — a
// ultima recebe NULL. Devolve 0 quando nao ha credencial carregada.
//
// Existe porque este mesmo bloco estava copiado em cada funcao do trakt.c, e o
// extras.c seria a quarta copia. Os buffers `aut` e `chave` sao de quem chama:
// os cabecalhos apontam para eles e precisam viver ate o fim do pedido.
int  trakt_cabecalhos(const char **cab, char *aut, size_t nAut,
                      char *chave, size_t nChave);
int  trakt_ativo(void);

// Credencial vinda da CONTA, no lugar do arquivo. O token sai de
// sync_pull_provider_credentials (provider "trakt"); o clientId e do
// APLICATIVO, nao da pessoa, e vem compilado (-DNV_TRAKT_CLIENT_ID, gerado de
// local.properties). Enquanto isto nao existia, o vinculo Trakt do app nativo
// era o do dono do pacote — para todo mundo que instalasse.
int  trakt_definir(const char *token, const char *clientId);

// Esquece a credencial. Chamado ao SAIR: um token de Trakt que sobrevive ao
// logout continua ESCREVENDO (trakt_marcar) na conta de quem saiu, com o que a
// proxima pessoa assistir.
void trakt_esquecer(void);

// Completa itens que ja tem imdb, tipo e progresso com arte, sinopse, meta e
// minutos restantes (Cinemeta), em paralelo; compacta os que o Cinemeta nao
// conhece e devolve quantos sobraram. Nao depende de credencial Trakt.
// BLOQUEIA — chamar do fio de descoberta.
int  trakt_enfeitar_lote(CatItem *saida, int n);

// Preenche ate `max` itens do "continue assistindo", ja com arte resolvida.
// BLOQUEIA — chamar do fio de descoberta. Devolve quantos preencheu.
// Remove um item da barra de retomada do Trakt (DELETE /sync/playback/<id>).
// `imdb` e a chave COMPOSTA que trakt_continuar montou. Devolve 0 quando nao ha
// id conhecido — o item veio do progresso da conta, nao do Trakt.
int trakt_playback_remover(const char *imdb);
// Marca (visto=1) ou desmarca (0) um lote de episodios, numa requisicao so.
// SINCRONO: quem chamar do fio de desenho tem de mandar para um fio proprio.
int trakt_episodios_marcar(const char *imdb, const VistoPar *pares, int qtd,
                           int visto);
int  trakt_continuar(CatItem *saida, int max);

// Atividade recente dos AMIGOS do dono. Usa o feed social oficial do Trakt
// (/users/me/friends/activities), mantendo no CatItem o titulo/arte normais e
// os dados sociais nos campos de apresentacao: `pais` = nome do amigo,
// `provNome` = acao e `direcao` = contexto do episodio. BLOQUEIA.
int  trakt_social(CatItem *saida, int max);

// Snapshot mensal usado pela tela Perfil e Stats. Faz as chamadas no fio de
// trabalho do app e nunca deve ser executado no laco de desenho.
int  trakt_perfil(PerfilDados *saida);

// Informa onde o dono parou. `imdb` pode trazer episodio ("tt123:4:9").
// Ate agora o app so LIA o Trakt; sem isto, assistir aqui nao mexia no
// "continue assistindo" dos outros aparelhos dele. Nao bloqueia: sai num fio.
void trakt_marcar(const char *imdb, double posSeg, double durSeg);

// Watchlist ("Minha Lista") e colecao ("Comprados") do dono. `qual` e
// "watchlist" ou "collection". BLOQUEIA — chamar do fio de descoberta.
int  trakt_lista(const char *qual, CatItem *saida, int max);

// Acrescenta ou tira o titulo da WATCHLIST do dono. Nao bloqueia. O estado de
// leitura ja vem em CatItem.naLista, preenchido por trakt_lista na descoberta —
// o que faltava era escrever de volta: o botao "+" so mexia num vetor local e a
// lista nos outros aparelhos nunca sabia.
void trakt_watchlist(const char *imdb, int adicionar);

// Marca (ou desmarca) o titulo como ASSISTIDO em /sync/history.
//
// NAO confundir com trakt_marcar, que e /scrobble/pause ("parei aqui") e serve
// ao player. O botao do olho quer dizer "ja vi este", e usava trakt_marcar com
// duracao 1.0 — valor que a propria guarda daquela funcao rejeita, entao nada
// chegava ao Trakt.
void trakt_assistido(const char *imdb, int marcar);

#endif
