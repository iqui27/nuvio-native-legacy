// FRASES E FICHA DE PRODUCAO, para SERIE E FILME. E o segundo pedido do dono
// sobre a pagina de titulo, e a parte dele que mais dependia de pesquisa: quase
// tudo que parece existir para "curiosidades" e "frases famosas" nao tem fonte
// consultavel.
//
// O QUE FOI MEDIDO ANTES DE ESCREVER ISTO (16/09/2026) — leia antes de propor
// qualquer outra fonte:
//
//   IMDb        tem trivia e quotes, e NAO TEM API publica. Raspar esta fora.
//   TMDB        NAO tem trivia nem quotes. Conferido contra a api de verdade:
//               `append_to_response=trivia,quotes` e aceito e IGNORADO em
//               silencio, e /movie/<id> nao devolve nenhuma dessas chaves.
//   Wikiquote   TEM, e livre, e tem api (MediaWiki). E a fonte das frases.
//   Wikidata    TEM os fatos de producao em campo estruturado (orcamento,
//               bilheteria, local de filmagem, premio, emissora, episodios) e
//               TEM o vinculo do imdb id ate a pagina do Wikiquote. E a fonte
//               da ficha, e o unico caminho honesto de "imdb id -> Wikiquote".
//
// COBERTURA MEDIDA do Wikiquote (nao suposta — 30 titulos consultados):
//   FILME: 11 de 14 tinham pagina com falas (Matrix, Duna 2, Dark Knight, Good
//     Will Hunting, Pulp Fiction, Forrest Gump, Barbie, Knives Out, EEAAO, Kung
//     Fu Panda 4, HTTYD 2025). Sem pagina: The Holdovers, The Menu, Planeta
//     Terra II (documentario).
//   SERIE: 4 de 12 tinham falas na pagina PRINCIPAL (Severance 62, The Bear
//     153, Rick and Morty 21, Breaking Bad 11). Friends, Avatar, Game of
//     Thrones, The Sopranos, The Office e House of Cards tem pagina SEM falas —
//     sao indices que apontam para subpaginas por temporada, e O NOME DESSAS
//     SUBPAGINAS NAO E DEDUZIVEL: "Stranger Things/Season 1" existe, "Breaking
//     Bad/Season 1" nao. Por isso este modulo NAO persegue subpagina: seria uma
//     viagem de rede que falha na maioria das vezes.
//
// Consequencia aceita e dita na tela: em serie, a secao muitas vezes nao
// aparece. Nao ha frase inventada para preencher.
//
// --- O PULO PARA A MINUTAGEM: MEDIDO E DESCARTADO -----------------------------
//
// O dono perguntou se da para ligar a fala ao minuto do filme e abrir direto
// ali. O caminho tecnico existe (legenda.h ja converte SRT/VTT em cue com
// tempo, e o addon de legenda ja responde por imdb id), e ele FOI MEDIDO:
//
//   casando as falas do Wikiquote contra a legenda inglesa do OpenSubtitles,
//   normalizando pontuacao e caixa:
//     The Dark Knight     80 falas testadas -> 21 exatas + 16 aproximadas = 46%
//     Forrest Gump        55                -> 13 + 14                    = 49%
//     Pulp Fiction        78                -> 21 + 23                    = 56%
//     Good Will Hunting   79                ->  6 +  5                    = 13%
//     The Matrix          NAO HA legenda em ingles no addon (27 em outros
//                         idiomas) — nem da para tentar.
//
// Ou seja: metade das falas nao casa, e mesmo as que casam trazem um tempo que
// vale para AQUELE arquivo de legenda — outra remontagem, outra codificacao,
// com ou sem "anteriormente em", e o tempo anda de segundos a minutos. Um botao
// que erra a cena em metade dos casos e pior que nenhum botao.
//
// Entao ficam SO AS FRASES na pagina de detalhe, que foi o que o dono disse
// para fazer se nao desse ("se nao der deixe so as frase tb na pagina de
// detalhes"). O que esta medido acima e o que precisaria mudar para isso voltar
// a ser discutivel: uma fonte de falas JA COM tempo, por lancamento.
#ifndef NV_SERIEFRASES_H
#define NV_SERIEFRASES_H
#include "gfx.h"

#define SF_FRASE_MAX 6
#define SF_FATO_MAX  6

// Abre a secao. SO CHAMAR QUANDO A PESSOA ENTRA NELA. `imdb` aceita
// "tt1234567" e a chave composta "tt1234567:2:4". Serve a filme e a serie — a
// consulta e por obra, nao por episodio.
void seriefrases_abrir(const char *imdb);
void seriefrases_fechar(void);
int  seriefrases_carregando(void);

// --- FRASES ------------------------------------------------------------------
int  seriefrases_n(void);
const char *seriefrases_quem(int i);    // personagem; "" quando a pagina nao diz
const char *seriefrases_texto(int i);
// Nome da pagina do Wikiquote de onde vieram, para o credito na tela.
const char *seriefrases_pagina(void);
// 1 quando a pagina lida e a portuguesa; 0 quando e a inglesa. Muda o rotulo:
// frase em ingles sob um app em portugues precisa dizer que esta em ingles.
int  seriefrases_em_portugues(void);

// --- FICHA DE PRODUCAO -------------------------------------------------------
// NAO se chama "Curiosidades". Sao campos do Wikidata, cada um com rotulo
// proprio; nao ha nada aqui reescrito de sinopse.
int  seriefrases_n_fatos(void);
const char *seriefrases_fato_rotulo(int i);
const char *seriefrases_fato_valor(int i);

void seriefrases_selecionar(int i);
int  seriefrases_selecionado(void);

// --- DESENHO -----------------------------------------------------------------
float seriefrases_desenhar(GfxRect r);        // painel de frases
float seriefrases_desenhar_fatos(GfxRect r);  // painel da ficha

#endif
