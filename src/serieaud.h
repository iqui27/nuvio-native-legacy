// AUDIENCIA DA SERIE: tres paineis de grafico para a pagina de titulo, todos
// desenhados com gfx_* (geometria), nenhum com imagem.
//
// O QUE CADA PAINEL DIZ, E DE ONDE VEM O NUMERO. Esta lista e a parte que nao
// pode ser "melhorada" depois sem conferir a fonte: o PRODUCT.md proibe meta-
// dado inventado, e grafico e o lugar mais facil de inventar — basta um rotulo
// generoso sobre um numero honesto.
//
//   1. ARCO DE QUALIDADE — nota do TRAKT por episodio da temporada.
//      NAO E IMDb. O mockup dizia "Nota do IMDb"; o app nao tem nota do IMDb
//      por episodio em lugar nenhum (o mdbList devolve a nota da OBRA, nao do
//      episodio), e o que esta em memoria e o `rating` que o Trakt manda em
//      /shows/<id>/seasons?extended=episodes,full — a mesma chamada que
//      extras.c ja faz para as pastilhas. CUSTO: ZERO pedidos. Quem chama
//      passa as notas que extras.h ja expoe (extras_ep_nota, em decimos).
//
//   2. RADAR DE DESISTENCIA — watchers(E_n) / watchers(E_1).
//      Um pedido /stats POR EPISODIO. E o painel caro; o orcamento esta na
//      nota de serieaud.c. O rotulo tem de dizer que conta QUEM MARCA NO
//      TRAKT, nao "a audiencia": sao dezenas de milhares de pessoas, nao os
//      milhoes que assistiram.
//
//   3. IMPRESSAO DIGITAL DO EPISODIO — quatro eixos empilhados por episodio.
//      SUBSTITUI o "DNA da serie" do mockup, que media TOM e GENERO por
//      episodio. Isso nao existe em fonte nenhuma que este app consulta:
//      genero e da OBRA (TMDB e Trakt), nunca do episodio. Os quatro eixos
//      daqui variam de verdade por episodio e ja estao na mao:
//        nota      = rating do Trakt
//        retencao  = watchers / watchers(E1)          } do mesmo /stats
//        rever     = plays / watchers                 } do painel 2 —
//        conversa  = comments + votes                 } CUSTO ZERO a mais.
//
// O INDICE DE REVISITA DA SERIE (plays/watchers do /shows/<id>/stats) entra
// como UM numero no rodape do painel 3, dito pelo que ele e: reproducoes por
// espectador SOMANDO TODOS OS EPISODIOS. O mockup abria isso por ano ("1o ano
// 68%, 2o 54%") — o Trakt nao publica nada por ano, e nenhuma conta com o que
// temos chega la. Esse trecho do mockup NAO E DESENHADO.
#ifndef NV_SERIEAUD_H
#define NV_SERIEAUD_H
#include "gfx.h"

// Teto de episodios POR TEMPORADA, que e tambem o teto de pedidos /stats.
// Ver a conta do orcamento em serieaud.c.
#define SA_EP_MAX 24

// --- CICLO DE VIDA -----------------------------------------------------------

// Abre a secao. SO CHAMAR QUANDO A PESSOA ENTRA NELA — nunca ao abrir a
// pagina: o custo e um pedido por episodio, e a maioria das visitas a uma
// serie nunca rola ate aqui.
//
// `imdb` aceita "tt1234567" e tambem o "tt1234567:2:4" da lista de episodios
// (o que vem depois do primeiro ':' e ignorado). `epNum` e `notaDecimos` sao
// os vetores que extras.h ja tem para a temporada (nota em decimos, 0 = sem
// nota); podem ser NULL, e ai o painel 1 fica vazio e os outros dois seguem.
//
// Repetir com a MESMA serie e temporada nao refaz nada.
void serieaud_abrir(const char *imdb, int temporada, const int *epNum,
                    const int *notaDecimos, int n);

// A pessoa saiu. O fio para na proxima fronteira de episodio e descarta o que
// vier depois. Nao bloqueia.
void serieaud_fechar(void);

// 1 enquanto ha pedido no ar. Quem desenha usa para mostrar "carregando" em
// vez de "sem informacao" — os dois estados sao a lista vazia.
int  serieaud_carregando(void);

// 1 quando o E1 ja chegou, que e o minimo para a retencao existir (ele e o
// denominador). Antes disso o painel 2 nao tem o que dizer.
int  serieaud_pronto(void);

// --- DADOS CRUS --------------------------------------------------------------
// Existem para o teste e para quem quiser o numero sem o desenho. Todos os
// indices sao 0..serieaud_n()-1, na ordem dos episodios.

int  serieaud_n(void);
int  serieaud_temporada(void);
int  serieaud_ep(int i);            // numero do episodio
int  serieaud_nota(int i);          // decimos (83 = 8.3); 0 = sem nota
int  serieaud_tem_stats(int i);     // 0 quando o /stats nao veio ou falhou
long serieaud_watchers(int i);
long serieaud_plays(int i);
int  serieaud_comentarios(int i);
int  serieaud_votos(int i);

// RETENCAO em MILESIMOS de E1 (1000 = 100%). -1 quando falta o dado.
//
// PODE PASSAR DE 1000, e isso NAO e erro a ser aparado: quando o piloto e um
// episodio que muita gente pula (episodio duplo contado como um, recapitulacao,
// estreia que saiu fora de ordem), o E2 tem mais watchers que o E1. MEDIDO no
// Trakt em 16/09/2026: star-trek-deep-space-nine T1 tem E1=25164 e E2=25245
// watchers — 100,3%. Grampear em 100% esconderia exatamente o caso
// interessante, e ainda faria a curva mentir sobre o inicio da temporada.
int  serieaud_retencao(int i);

// REVER = plays/watchers em CENTESIMOS (119 = 1,19 reproducoes por pessoa).
// -1 quando falta o dado. Sempre >= 100 quando existe.
int  serieaud_rever(int i);

// O mesmo indice para a SERIE INTEIRA (/shows/<id>/stats), em centesimos.
// ATENCAO ao que este numero e: soma as reproducoes de TODOS os episodios
// sobre os espectadores da SERIE, entao ele cresce com o tamanho da serie
// (Breaking Bad da 5962, ou 59,6x — sao 62 episodios). Nao e comparavel com o
// de um episodio e nao deve ser desenhado como se fosse. -1 = nao chegou.
int  serieaud_rever_serie(void);

int  serieaud_nota_media(void);     // decimos; 0 = nao da para calcular
int  serieaud_melhor(void);         // indice do melhor episodio; -1
int  serieaud_pior(void);

// --- SELECAO -----------------------------------------------------------------
// O painel 3 destaca UM episodio. O vocabulario e o desta base: selecionado =
// SUPERFICIE CLARA PREENCHIDA com texto ESCURO, sem contorno (layout.h,
// NV_COR_FOCO).
void serieaud_selecionar(int i);
int  serieaud_selecionado(void);

// --- DESENHO -----------------------------------------------------------------
// Cada um desenha DENTRO de `r` e devolve a altura usada, para quem empilha.
// Sao so primitivas gfx_* e texto: nenhuma textura, nenhum byte do orcamento
// de imagens (NV_TEX_ORCAMENTO_MB ja vive encostado no teto na TV do dono).
float serieaud_arco(GfxRect r);
float serieaud_radar(GfxRect r);
float serieaud_digital(GfxRect r);

#endif
