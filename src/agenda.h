// AGENDA: quando sai o proximo episodio das series que o dono acompanha, e o
// lembrete para o dia em que sair.
//
// ---------------------------------------------------------------------------
// DE ONDE VEM A DATA, e por que nao custa pedido novo na pagina de detalhe
//
// O corpo de /tv/<id> do TMDB — o MESMO que extras.c ja baixa para montar redes,
// "mais como este" e a lista de temporadas — sempre traz tres campos que o
// parse antigo lia e jogava fora:
//
//   "status": "Returning Series" | "Ended" | "Canceled" | "In Production" | ...
//   "next_episode_to_air": { season_number, episode_number, name, air_date }
//   "last_episode_to_air": { ... }
//
// Ou seja: na tela de titulo a agenda e DE GRACA, pelo mesmo argumento que
// extras.h ja registra para a ficha tecnica do filme ("Nenhum pedido de rede
// novo"). O unico ajuste foi tirar a serie da guarda de toggles: ver a nota em
// extras.c.
//
// POR QUE TMDB E NAO TRAKT. O Trakt tem /shows/<id>/next_episode, e o app ja
// fala com ele por IMDb id sem traducao. Perde em tres pontos medidos:
//   1. E um pedido A MAIS — o corpo do TMDB ja esta na mao.
//   2. Sem `?extended=full` ele nao devolve a data; com, sao dois pedidos
//      (um para o episodio, outro para o `status` da serie).
//   3. `first_aired` do Trakt e instante UTC; a data de estreia de um episodio
//      e uma DATA de calendario. Converter UTC->local com o fuso do aparelho
//      (que numa TV nem sempre esta certo) desloca a estreia em um dia.
// O `status` do Trakt continua sendo lido por extras.c quando existe, e este
// modulo aceita as duas grafias (o Trakt escreve em minusculas).
//
// ---------------------------------------------------------------------------
// O LEMBRETE NUMA TV SEM PUSH
//
// Nao ha notificacao. Nem webOS nem Tizen entregam nada a um app fechado, e
// este app nao tem servico de fundo. Dizer "avisamos voce" seria mentira.
//
// O que o lembrete FAZ, e e real:
//   - grava a intencao em lembretes-p<N>.txt, por perfil, pela mesma
//     dados_gravar atomica da sessao (padrao de fileirasui-p<N>.txt);
//   - a serie sobe para o topo da tela Agenda, com o dia marcado;
//   - quando o dia CHEGA, agenda_devidos() devolve os lembretes vencidos e
//     ainda nao avisados, uma unica vez por episodio — a mesma disciplina de
//     marca por conteudo de atualizacao.c e recintro.c. Quem abre o app ve o
//     aviso; quem nao abre nao ve nada, e a interface nao promete o contrario.
//
// ---------------------------------------------------------------------------
// QUE LISTA E "AS SERIES QUE EU SIGO"
//
// Duas fontes, unidas:
//   1. CatItem.naLista — a marca UNICA em que ja caem as tres superficies de
//      "quero ver" (salvos.h local, watchlist do Trakt, biblioteca da conta).
//      Ver a nota em salvos.h: existe UM destino de leitura, de proposito.
//   2. progresso.h — series com posicao guardada no PERFIL ATIVO. Quem esta no
//      meio da 3a temporada segue a serie mesmo sem nunca ter apertado "+".
// Filme fica de fora: nao tem proximo episodio.
#ifndef NV_AGENDA_H
#define NV_AGENDA_H

#include <stddef.h>

#define AG_MAX 120

// Situacao da serie. A ordem importa para o agrupamento da tela.
typedef enum {
  AG_DESCONHECIDA = 0,
  AG_VOLTANDO,      // "Returning Series" / "returning series"
  AG_PRODUCAO,      // "In Production", "Planned", "Pilot"
  AG_ENCERRADA,     // "Ended"
  AG_CANCELADA      // "Canceled" / "Cancelled"
} AgSituacao;

typedef struct {
  char imdb[24];
  char titulo[160];
  char poster[512];
  int  situacao;          // AgSituacao
  int  temporada, episodio;
  char nomeEp[120];
  char dataProx[12];      // ISO "2026-09-18"; "" = nao ha data
  char dataUlt[12];       // ISO do ultimo episodio que foi ao ar
  int  lembrete;          // 1 = o dono pediu para ser lembrado
  // --- O QUE VINHA NO MESMO CORPO E ERA JOGADO FORA -------------------------
  //
  // Os cinco campos abaixo saem do MESMO /tv/<id> que este modulo ja baixa
  // para ler `status` e `next_episode_to_air`. Zero pedido novo — a mesma conta
  // que a nota do topo deste arquivo ja faz para a data. Foram lidos porque a
  // tela Agenda precisava de mais do que "titulo + dia" e a alternativa era
  // inventar (noticia, citacao) ou pagar por consulta nova.
  //
  // Vazio/zero = o TMDB nao tem o campo para esta serie, e quem desenha OMITE a
  // linha. Nunca um valor de reserva: ver PRODUCT.md.
  char sinopse[400];      // overview do proximo episodio; do ULTIMO quando nao ha proximo
  char tipoEp[24];        // episode_type cru: "premiere", "finale", "mid_season", "standard"
  char rede[64];          // networks[0].name — "Apple TV+", "HBO"
  int  duracao;           // minutos do episodio; 0 = nao ha
  int  temporadas;        // number_of_seasons
  // Primeiro genero (TMDB genres[0].name ou Cinemeta genres[0]). A tela so usa
  // quando o catalogo nao tem o item: com catalogo, o genero dele manda.
  char genero[48];
} AgItem;

// Le o cache e os lembretes do perfil ativo. Chamar depois de dados_iniciar.
// Repetir com o mesmo perfil nao custa nada; com outro perfil, recarrega.
void agenda_iniciar(void);

// Classifica o `status` cru do TMDB OU do Trakt. Reconhece as duas grafias
// porque extras.c sobrescreve o status do TMDB com o do Trakt quando ha token.
AgSituacao agenda_situacao_de(const char *cru);

// Guarda o que o parse leu. Seguro entre fios. `titulo`/`poster` podem vir
// vazios: o que ja estava guardado nao e apagado por um campo vazio.
void agenda_registrar(const char *imdb, const char *titulo, const char *poster,
                      const char *statusCru, int temporada, int episodio,
                      const char *nomeEp, const char *dataProx,
                      const char *dataUlt);

// O resto do corpo /tv/<id>, gravado no MESMO registro. Separado de
// agenda_registrar de proposito: extras.c chama so o primeiro (a pagina de
// titulo ja tem estes dados em outras secoes) e o fio deste modulo chama os
// dois. Campo vazio ou zero nao apaga o que ja estava.
void agenda_registrar_extra(const char *imdb, const char *sinopse,
                            const char *tipoEp, const char *rede,
                            const char *genero, int duracao, int temporadas);

// Registro guardado, ou NULL. O ponteiro vale ate a proxima escrita — copie
// se for atravessar um quadro.
const AgItem *agenda_registro(const char *imdb);

// UMA LINHA para a pagina de titulo, ja traduzida e pronta para desenhar.
// Devolve 0 e escreve "" quando nao ha nada honesto a dizer (serie sem
// registro, ou registro sem data e sem situacao conhecida) — a linha entao
// SOME, em vez de mostrar um valor de reserva. Ver PRODUCT.md.
int agenda_frase(const char *imdb, char *dst, size_t tam);

// 1 quando existe data FUTURA (ou de hoje) para lembrar. E a condicao do botao
// "Lembrar-me": serie encerrada ou sem data nao ganha botao nenhum.
int agenda_pode_lembrar(const char *imdb);

int agenda_lembrete(const char *imdb);            // 1 = ligado
// Liga/desliga e GRAVA. Devolve o estado novo. Ignora quem nao pode lembrar.
int agenda_alternar_lembrete(const char *imdb);

// --- calendario -------------------------------------------------------------
// Remonta a lista das series seguidas a partir do catalogo, do progresso e do
// cache. Devolve quantas. FIO PRINCIPAL.
int agenda_montar(void);
int agenda_n(void);
const AgItem *agenda_lista(int i);

// --- datas ------------------------------------------------------------------
// "hoje" em ISO ("2026-09-16"). Substituivel para teste; passar NULL ou ""
// devolve o relogio do sistema.
void agenda_definir_hoje(const char *iso);
const char *agenda_hoje(void);

#define AG_SEM_DATA (-100000)
// Dias de hoje ate `iso`. 0 = hoje, 1 = amanha, negativo = ja passou.
// AG_SEM_DATA quando a entrada nao e uma data ISO.
int agenda_dias(const char *iso);

// "hoje", "amanhã", "em 6 dias", "18 de setembro de 2026" — a forma curta
// quando esta perto, a data por extenso (desc_data_extenso, a mesma do resto
// do app) quando esta longe. Vazia para entrada invalida.
void agenda_quando(const char *iso, char *dst, size_t tam);

// --- as pecas do calendario -------------------------------------------------
// A tela Agenda desenha uma LINHA DO TEMPO: dia em numeral, mes como cabecalho
// de secao e dia da semana como apoio. Nenhuma dessas tres pecas sai de
// agenda_quando (que devolve a frase inteira), e formatar data em dois lugares
// e como este repositorio ja perdeu uma tarde: ver a nota sobre desc_data_extenso
// logo acima. Entao as pecas saem daqui.
int agenda_dia(const char *iso);      // 1..31; 0 = entrada invalida
int agenda_mes(const char *iso);      // 1..12; 0 = invalida
int agenda_ano(const char *iso);      // 2026; 0 = invalida
// 0 = domingo ... 6 = sabado. -1 quando a entrada nao e data.
int agenda_semana(const char *iso);
// Nome do mes e abreviacao do dia da semana, ja como CHAVE de traducao — quem
// desenha passa direto a txt_linha, que traduz (ver idioma.h). Os nomes dos
// meses sao os MESMOS de desc_data_extenso, de proposito: duas tabelas de mes
// divergem na primeira revisao de traducao.
const char *agenda_mes_nome(int mes);
const char *agenda_semana_nome(int dia);

// Quanto FALTA, sempre em forma curta, para qualquer distancia: "hoje",
// "amanhã", "em 6 dias", "em 7 semanas", "em 5 meses". Diferente de
// agenda_quando, que a partir de uma semana devolve a data por extenso — na
// linha do tempo a data ja esta desenhada no numeral ao lado, e repeti-la por
// extenso e a tinta que este redesenho gastava sem dizer nada novo.
void agenda_falta(const char *iso, char *dst, size_t tam);

// O MARCO do episodio: "Estreia da temporada", "Final da temporada", "Final da
// meia temporada". Sai do `episode_type` do TMDB, que e um campo do mesmo corpo
// e diz exatamente isto — nao e inferido do numero do episodio, que erraria em
// toda serie com temporada de tamanho irregular.
// Devolve 0 e escreve "" quando o tipo e "standard" ou desconhecido: um
// episodio comum nao tem marco, e escrever "episodio" ali seria ruido.
int agenda_marco(const AgItem *it, char *dst, size_t tam);

// A LINHA DE APOIO da tela Agenda: rede, duracao e numero de temporadas, na
// ordem, separados por "·", pulando o que nao existe. "" quando nao ha nenhum
// dos tres — e ai a linha some, em vez de mostrar separadores vazios.
void agenda_apoio(const AgItem *it, char *dst, size_t tam);

// --- lembretes vencidos -----------------------------------------------------
// Lembretes cuja data ja chegou e que ainda nao foram avisados. Preenche
// `saida` com ponteiros para a lista montada. Chamar agenda_montar antes.
int agenda_devidos(const AgItem **saida, int max);
// Marca como avisado e GRAVA: o mesmo episodio nao avisa duas vezes.
void agenda_marcar_avisado(const char *imdb);

// --- atualizacao das seguidas ----------------------------------------------
// Sobe um fio que busca as series seguidas cujo registro esta faltando ou
// velho (> 12 h). ISTO E PEDIDO DE REDE NOVO, e e o unico do modulo: sem ele o
// calendario so conhece as series que o dono abriu.
//
// TRES FONTES, nesta ordem, e a seguinte so preenche o que a anterior deixou
// vazio (ver a nota em agenda.c, "AS TRES FONTES"):
//   1. TMDB /tv/<id>             — so com a chave (ajuste TMDB ligado);
//   2. Trakt next/last_episode   — so com o Trakt vinculado (trakt_ativo);
//   3. Cinemeta /meta/series     — sempre: sem chave, e o que sobra para quem
//                                  chega do app web com o TMDB desligado.
// Antes desta revisao era so o TMDB, e sem chave a funcao voltava na hora.
void agenda_atualizar_seguidas(void);
int  agenda_atualizando(void);
// Sobe 1 a cada fio que TERMINA. A tela guarda o numero que viu e remonta
// quando ele muda — contador e nao borda de agenda_atualizando(), porque um
// fio que acaba antes do primeiro quadro (tudo em cache de rede, ou tudo
// falhou rapido) nao deixaria borda nenhuma para ver.
int  agenda_versao(void);

// SO PARA TESTE: troca o GET do fio. Mesma forma de rede_baixar_st (corpo +
// codigo HTTP em *status). NULL volta para a rede de verdade. Existe porque
// tests/agenda_shot.c compila src/agenda.c inteiro e abre a tela, que dispara
// o fio — sem isto a captura sairia para a internet.
typedef char *(*AgBaixar)(const char *url, int segundos,
                          const char *const *cab, int *status);
void agenda_rede_teste(AgBaixar f);

// Apaga cache e lembretes de todos os perfis. Para o logout.
void agenda_esquecer(void);

#endif
