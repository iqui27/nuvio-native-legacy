// Tela de detalhe do titulo.
//
// A transicao e o ponto: no Apple TV o card NAO some para dar lugar a uma tela
// nova — ele cresce ate virar o hero do detalhe, e o resto entra depois. Por
// isso detail_abrir recebe o retangulo de origem real do card na tela, e nao
// apenas o titulo: e esse rect que da continuidade ao movimento.
#ifndef NV_DETAIL_H
#define NV_DETAIL_H
#include <SDL2/SDL.h>
#include "home.h"

void detail_abrir(const HomeItem *item);
int  detail_aberto(void);
// 0..1 de quanto o detalhe tomou a tela; a home usa para descer as fileiras.
float detail_progresso(void);        // 1 enquanto a tela existe, inclusive saindo
// 1 quando o cartao ja cobre a tela inteira e desenhar a home por baixo e
// trabalho jogado fora. Medido: a home custa o hero em tela cheia mais ~20
// cards, e sem este corte a pagina de detalhe rodava a 20fps.
int  detail_cobre_tela(void);
// 1 quando o cartao ja parou no lugar e nao esta esticado: nesse estado a home
// atras so aparece pela moldura.
int  detail_assentado(void);

// Qual titulo do catalogo esta em cena, e os pedidos que a tela nao resolve
// sozinha: reproduzir e marcar na lista. O detalhe nao chama o player nem a
// biblioteca direto — quem conhece as outras telas e o roteador.
int  detail_indice(void);
// Temporada e episodio em foco (1 = ha episodio; 0 = titulo sem episodios).
int  detail_ep_foco(int *temporada, int *episodio);
int  detail_pediu_reproduzir(void);   // consome o pedido
void detail_pedir_reproduzir(void);   // arma o pedido (OK no card de retomada)

// Indice do titulo que a tela pediu para ABRIR no lugar do atual, ou -1.
// Consome o pedido. Nasce de dois lugares: um credito na filmografia de um ator
// e um item da aba "Mais como este" — os dois trazem um IMDb id, e quem sabe
// traduzir isso em indice e o catalogo. Quem TROCA de titulo e o roteador em
// app.c, nao esta tela: reabrir a si mesma no meio do proprio desenho e o tipo
// de coisa que quebra em silencio.
int  detail_pediu_abrir(void);

// O botao do olho: marcar o titulo como ASSISTIDO. Nao e o mesmo que
// detail_pediu_marcar, que e "adicionar a lista" — o olho caia no mesmo `else`
// do botao de fontes e nunca marcou nada.
int  detail_pediu_assistido(void);
int  detail_pediu_marcar(void);       // botao "+"
int  detail_pediu_fontes(void);       // OK segurado, ou o botao "..."
// Botao secundario "Reproduzir desde o inicio", que so existe quando ha
// progresso. Hoje ele tambem marca `detail_pediu_reproduzir`, porque o roteador
// ainda nao sabe abrir o player ignorando o ponto salvo.
int  detail_pediu_do_inicio(void);
// --- Pagina do titulo (abaixo da dobra) -------------------------------------
// TUDO daqui para baixo foi medido no app web LOGADO, em 1920x1080, na serie
// "Silo" (getBoundingClientRect / getComputedStyle), em 2026-08-31. Os numeros
// nao vem do CSS: a folha declara `clamp(440px, 29vw, 540px)` para o card de
// episodio e o que a tela desenha e 640 — ler a folha erra por 100px.
//
// Vivem aqui e nao em layout.h porque layout.h esta sendo mexido por outros
// agentes nesta mesma sessao.
//
// O modelo e o do web: UM documento rolavel de 2144px de altura, do qual a tela
// mostra 1080. O hero ocupa 0..1080 e ROLA junto — nao ha cabecalho fixo, nem
// logo centralizado no topo (isso era do app da Apple TV). As secoes ficam em
// coordenadas ABSOLUTAS de documento, e rolar e so subtrair scrollY.
#define NV_DETP_X             96.0f   // gutter das fileiras (--tv-safe-gutter-wide)
// Fim do documento rolavel. NAO e onde o elenco termina (2024): e o que o web
// tem de altura rolavel, porque abaixo do elenco ele ainda monta as secoes de
// comentarios e de produtoras, que este port nao tem.
//
// O numero sai da medida, nao da conta: com o elenco focado o web para em
// scrollTop 1393, e para o topo do grupo (1749) cair nos 33% da tela (356) o
// documento precisa ter pelo menos 1393 + 1080 = 2473. Com os 2144 da conta
// "elenco + padding" a rolagem batia no teto em 1064 e a fileira de elenco
// ficava em y=693 em vez de y=364 — meio ecra fora do lugar, e foi assim que
// apareceu na primeira captura do aparelho.
#define NV_DETP_FIM         2473.0f
// Regra de rolagem do web, achada no fonte e conferida com quatro medidas:
// o topo do GRUPO focado vai para 33% da altura util (40% nas abas). Constantes
// DETAIL_ROW_FOCUS_TARGET / DETAIL_TAB_FOCUS_TARGET de metaDetailsScreen.js.
// Conferido: temporadas -> scrollTop 724, episodios -> 838, abas -> 1248,
// elenco -> 1393. Os quatro batem na casa do pixel.
#define NV_DETP_ALVO_FILEIRA  0.33f
#define NV_DETP_ALVO_ABAS     0.40f
// Vao entre itens de uma linha de meta. O flex declara 24 e o que se mede e 38
// (borda a borda) nas SEIS ocorrencias: genero->ponto, ponto->ano, ano->IMDb,
// selo->duracao, duracao->pais, pais->idioma. Mede-se, nao se le.
#define NV_DETP_SEP           38.0f

// Topo de cada GRUPO focavel, em coordenada de documento.
#define NV_DETP_G_TEMP      1080.0f
#define NV_DETP_G_EP        1194.0f
#define NV_DETP_G_ABAS      1680.0f
#define NV_DETP_G_ELENCO    1749.0f

// Abas de temporada: 269x80 em x=96, passo 321 (gap 52), raio 40, fonte 32/500.
// A largura sai do texto + padding, e nao e constante: "Especiais" mede 219.
#define NV_DETP_TEMP_Y      1160.0f
#define NV_DETP_TEMP_H        83.0f   // MEDIDO na referencia (era 80)
#define NV_DETP_TEMP_PADX     40.0f
#define NV_DETP_TEMP_GAP      52.0f
// Base do resumo da temporada ("12 episódios · 5 assistidos") ao TOPO das
// pilulas. A folga vive na vaga do cabecalho "Temporadas" que foi retirado: o
// grupo comeca em NV_DETP_G_TEMP (1080) e a pilula so em 1160, entao ha 80 px
// livres e uma linha de 23 cabe com 20 acima e 22 abaixo. Menos que isto e a
// linha gruda na pilula e le como parte dela.
#define NV_DETP_TEMP_RESUMO_DY 22.0f

// Episodio: card 640x422 em x=96, passo 726, raio 32. A diferenca estrutural
// com o port anterior (que era do app da Apple TV) e que o TEXTO FICA DENTRO da
// miniatura, sobre um degrade vertical, e nao abaixo dela.
#define NV_DETP_EP_Y        1286.0f
// AS MEDIDAS DO CARD FORAM REFEITAS NO APARELHO (TCL, 1920x1080, serie
// "Furious", 2026-09-01), porque as do web erravam em quase todas: o passo era
// 726 contra os 671 medidos (o card ficava com 86px de vao em vez de 31) e o
// bloco de texto vinha 30px acima do lugar, encostado no meio da miniatura.
//
// Referencia lida: card focado com anel de 4px em x=94..737 e y=245..662, ou
// seja caixa 96..735 x 247..660 — 640x414. O card seguinte comeca em x=767.
#define NV_DETP_EP_W         640.0f
#define NV_DETP_EP_H         414.0f   // a caixa E a miniatura: o texto fica dentro
#define NV_DETP_EP_PASSO     672.0f   // 767 - 96 = 671, arredondado para 640+32
#define NV_DETP_EP_THUMB_H   414.0f
#define NV_DETP_EP_RAIO       32.0f
#define NV_DETP_EP_PAD        32.0f   // margem do texto dentro da miniatura
#define NV_DETP_EP_TEXTO_W   576.0f
// Selo "EPISÓDIO n": caixa 152x43 em (32,155) dentro do card, tinta do texto
// em 147..266 — 19 de folga a esquerda. Medido no card 1 de "Furious".
#define NV_DETP_EP_SELO_Y    124.0f
#define NV_DETP_EP_SELO_H     38.0f
#define NV_DETP_EP_SELO_PADX  18.0f
// Folga entre o selo do numero e o selo de "ainda nao foi ao ar", quando ele
// existe. 12 e a mesma distancia que separa o relogio da duracao no rodape do
// card (8) mais a meia-folga de um selo — abaixo disso os dois preenchimentos
// leem como uma peca so a 3 metros, e acima deles o par se descola do canto.
#define NV_DETP_EP_SELO_GAP   12.0f
// Os tres offsets abaixo sao o TOPO DA CAIXA da linha, nao o topo da tinta: a
// tinta medida (caixa alta do titulo em +221, sinopse em +274, rodape em +349)
// fica alguns pixels abaixo do topo da caixa que o SDL_ttf devolve, e o
// desconto e a diferenca entre a ascendente e a altura de caixa alta da Inter
// no corpo de cada linha.
#define NV_DETP_EP_TIT_Y     174.0f   // titulo separado do selo
#define NV_DETP_EP_SIN_Y     224.0f   // tres linhas antes do rodape
#define NV_DETP_EP_LD_SIN     32.0f
#define NV_DETP_EP_META_Y    344.0f   // relogio + duracao + data, tinta em +349
#define NV_DETP_EP_ICONE      28.0f
#define NV_DETP_EP_BARRA_Y   390.0f   // barra 576x8, raio 999
#define NV_DETP_EP_BARRA_H     8.0f
#define NV_DETP_EP_STATUS     48.0f   // circulo tracejado de "nao assistido"
// Anel de foco. O web usa 2px na miniatura do episodio e 4px nos botoes do
// hero; aqui fica 4 nos dois, porque a folha declara pixels de CSS e a tela
// desta pagina roda com um fator de escala de ~1.6 sobre os `clamp` — ou seja,
// o anel de 2px do episodio chega perto de 3px reais, e 4 e o valor inteiro que
// mais se aproxima sem sumir na TV.
#define NV_DETP_ANEL           4.0f

// Abas "Criador e elenco | Avaliacoes | Mais como este | Trailer": fonte 32/500,
// selecionada branca, as outras #808080; o divisor "|" e 32/700 #808080, com 20
// de folga de cada lado.
#define NV_DETP_ABA_Y       1758.0f
#define NV_DETP_ABA_H         51.0f
#define NV_DETP_ABA_SEP       20.0f

// Elenco: card de 220 de largura, passo 270; avatar 140x140 ALINHADO A
// ESQUERDA do card (nao centralizado); nome 26/500 rgb(179,179,179) e papel
// 21/400 rgb(128,128,128) abaixo.
#define NV_DETP_EL_Y        1817.0f
#define NV_DETP_EL_W         220.0f
#define NV_DETP_EL_PASSO     270.0f
#define NV_DETP_EL_AVATAR    140.0f
#define NV_DETP_EL_NOME_DY    10.0f   // base do avatar -> topo do nome
#define NV_DETP_EL_PAPEL_DY   43.0f   // topo do nome -> topo do papel
// Altura de uma linha de texto do cartao de elenco (nome ou papel), e o vao
// MEDIDO entre a base do elenco e o topo do wordmark do Trakt na captura da
// referencia (~105 px em 1920). Existem para o empilhamento da secao de
// comentarios na SERIE nao precisar chutar onde o elenco termina — usar
// NV_DETF_EL_ALT, que e a altura do elenco do FILME, punha a secao POR CIMA
// dos avatares.
#define NV_DETP_EL_LINHA      34.0f
#define NV_DETP_EL_GAP_TRAKT 105.0f

// Selos da pilha de meta do HERO, medidos na mesma sessao.
#define NV_DETW_IMDB_W       109.0f   // logo 60x60 + folga + nota 20.7/400
#define NV_DETW_IMDB_H        60.0f
#define NV_DETW_SELO_H        45.0f   // .detail-meta-badge, raio 8, borda 1px
#define NV_DETW_SELO_PADX     10.0f
// Botao secundario "Reproduzir desde o inicio": 345x96, raio 64, fundo #222,
// texto branco; focado vira #f5f5f5 com texto #111 e o anel de 4px.
#define NV_DETW_BTN2_PADX     28.0f   // 34 na medida do web; 28 acompanha a pilula de 72

// --- BLOCO DO HERO, MEDIDO NO APARELHO -------------------------------------
//
// Tudo com o prefixo NV_DETW2_ foi medido PIXEL A PIXEL em capturas do app de
// referencia rodando na TCL em 1920x1080 (adb exec-out screencap), numa SERIE
// ("Lioness") e num FILME ("Ma"), em 2026-09-01. As duas capturas foram lidas
// por um decodificador de PNG proprio, nao a olho.
//
// POR QUE UM SEGUNDO CONJUNTO, e nao corrigir NV_DETW_*: aqueles vieram do app
// WEB (getBoundingClientRect sobre .series-detail-shell). O web e a TV sao dois
// aplicativos diferentes e divergem em quase tudo que esta aqui — a coluna
// comeca em 96 e nao em 72, o botao primario mede 94 de altura e nao 96, o foco
// e ESCALA e nao anel, e o selo do IMDb tem 60x30 e nao 109x60. Onde os dois
// discordam manda o aparelho, que e o que se ve. Os NV_DETW_* continuam vivos
// porque outras partes da tela ainda os usam.
//
// Vivem em detail.h e nao em layout.h porque layout.h esta sendo mexido por
// outros agentes nesta mesma sessao.
#define NV_DETW2_X            96.0f   // coluna do conteudo (era 72, do web)
// Base da pilha: a borda de baixo do selo de classificacao cai em 1047/1048 nas
// DUAS capturas, com sinopses de tamanhos diferentes. E o mesmo valor que
// NV_DETW_BASE ja tinha, e e o que ancora a pilha inteira.
#define NV_DETW2_BASE       1048.0f

// LINHA DE ACOES. Pilula em repouso 321x94 (x=96..417, y=512..606 na serie),
// circulos de 96 com 24 de vao entre vizinhos (centros em 488,5 e 608,5, passo
// 120). A largura da pilula sai do rotulo: 54 + 28 + 21 + texto + 54 = 319 para
// "Assistir T1:E1", contra os 321 medidos.
//
// REDUZIDOS EM 19/09/2026, pedido do dono olhando a TV: "botoes muito grandes
// perto do restante da UI". A medida do web (94/96) foi feita para uma pagina
// em que o botao e o unico elemento grande; aqui ele fica a 40 px de uma linha
// de 22 px e de uma sinopse de 25, e pesava quase quatro vezes o texto ao
// lado. 72 e a altura que o hero da home ja usa para "Ver titulo" (68 + 4 de
// respiro do rotulo de 25 px), entao as duas telas passam a ter o mesmo botao.
// Padding, icone e vao descem na mesma proporcao (~0,75); o rotulo fica em 25,
// que e o piso de leitura desta interface. O foco continua por ESCALA, com os
// mesmos fatores medidos: 72 vira 83 focado.
#define NV_DETW2_BTN_H        72.0f
#define NV_DETW2_BTN_PADX     38.0f
#define NV_DETW2_BTN_ICONE_W  22.0f   // triangulo 22x24, centrado na vertical
#define NV_DETW2_BTN_ICONE_H  24.0f
#define NV_DETW2_BTN_GAPI     16.0f   // fim do triangulo -> tinta do rotulo
#define NV_DETW2_CIRC         72.0f
#define NV_DETW2_BTN_GAP      18.0f
// Glifo dentro do circular: 32 num circulo de 96 em repouso e 36 num de 110
// focado — 0,333 do diametro nos dois. Estava 0,45, que vinha de uma captura
// solta do dono e engordava o "+" ate quase encostar na borda.
#define NV_DETW2_CIRC_GLIFO    0.333f
// FOCO: o aparelho NAO desenha anel. O item focado CRESCE, com o centro parado,
// e o circular escuro ainda troca de cor (#222 -> #f5f5f5, glifo branco -> #111).
// A pilula branca fica branca nos dois estados: e o tamanho que diz o foco.
//
// Os dois fatores sao medidos e NAO sao iguais, o que surpreende mas se repete:
// a pilula vai de 321x94 para 357,6x107,8 (1,114 em x, 1,147 em y) e o circulo
// de 96 para 110 (1,146 nos dois eixos). Escrever um so fator faria a pilula
// crescer 10px a mais do que a referencia; ficam os dois, como medidos.
#define NV_DETW2_FOCO_SX       1.114f
#define NV_DETW2_FOCO_SY       1.147f

// PILHA DE TEXTO. As duas capturas dao as mesmas coordenadas absolutas para o
// que esta ABAIXO da sinopse (selo do IMDb em y=938, selo de classificacao em
// y=999) e a sinopse cresce PARA CIMA — 5 linhas na serie, 4 no filme, e a
// ULTIMA linha cai no mesmo y nas duas. Por isso a pilha e montada de baixo
// para cima, e nao do logo para baixo.
#define NV_DETW2_LD_SIN       40.0f   // passo entre linhas da sinopse (medido)
#define NV_DETW2_SIN_LINHAS       5   // o maximo visto na referencia
#define NV_DETW2_TEXTO_W    1040.0f   // sinopse e linha de apoio (96..1136)
// Distancia entre TOPOS DE CAIXA de linhas vizinhas, ja descontada a metrica da
// fonte: entre a tinta a referencia da 62 do alto do "R" de "Roteirista" ao
// alto do "C" da sinopse, e as duas linhas usam o mesmo corpo.
#define NV_DETW2_GAP_SUP      62.0f
// Fim da caixa da sinopse -> topo do selo do IMDb. A tinta da ultima linha
// termina em 890 e o selo comeca em 938; o resto e a descida da fonte.
#define NV_DETW2_GAP_SIN      33.0f
// Base da pilula em repouso (606) -> topo da caixa da linha de apoio. A tinta
// do "R" comeca em 649, e a caixa comeca ~6 acima dela no corpo de 26.
#define NV_DETW2_GAP_ACOES    37.0f

// META LINHA 1: generos, data e o selo do IMDb, tudo em rgb(179,179,179).
// A altura da linha e a do selo (30), que e o item mais alto dela.
#define NV_DETW2_M1_H         30.0f
#define NV_DETW2_META_GAP     31.0f   // 999 - 968
// Selo do IMDb: retangulo amarelo #f6c700 de 60x30, raio ~4, com "IMDb" preto
// dentro; a nota vem 8px depois, no mesmo cinza do resto da linha. NAO e o
// 109x60 do web — este e menor e a marca ocupa o selo inteiro.
#define NV_DETW2_IMDB_W       60.0f
#define NV_DETW2_IMDB_H       30.0f
#define NV_DETW2_IMDB_R        4.0f
#define NV_DETW2_IMDB_GAP      8.0f
// Ponto separador. Sao DOIS pontos diferentes e a diferenca e so a cor: entre
// generos ele e rgb(179,179,179) com 11 de folga de cada lado, e entre GRUPOS
// (generos | data | nota) e rgb(128,128,128) com 30. Os dois medem 6x7.
#define NV_DETW2_PONTO_D       6.0f
#define NV_DETW2_SEP          30.0f
#define NV_DETW2_BULLET_SEP   11.0f

// META LINHA 2: um selo de CONTORNO com classificacao e status juntos, depois
// duracao (so em filme) e pais. Caixa de 49 de altura em y=999, raio 8, borda
// de 2px rgb(107,107,107) — contorno de verdade, sem miolo pintado.
#define NV_DETW2_SELO_H       49.0f
#define NV_DETW2_SELO_R        8.0f
#define NV_DETW2_SELO_PADX    16.0f
#define NV_DETW2_SELO_BORDA    2.0f
// Divisoria interna: barra de 2x24 na mesma cor da borda, com 18 de folga de
// cada lado. E ela que separa "TV-MA" de "RENOVADA" dentro do mesmo selo.
#define NV_DETW2_DIV_W         2.0f
#define NV_DETW2_DIV_H        24.0f
#define NV_DETW2_DIV_PAD      18.0f

// --- Pagina do titulo tipo FILME, abaixo da dobra ---------------------------
//
// Filme NAO reaproveita as coordenadas absolutas da serie. Aquelas foram
// medidas numa pagina de serie (temporadas + episodios) e, num filme, deixavam
// 600px de buraco. Aqui as secoes se EMPILHAM: cada uma sabe a propria altura e
// a seguinte comeca onde a anterior acabou.
//
// ORIGEM DESTAS MEDIDAS, e vale ficar escrito porque sao DUAS fontes:
//
// TAMANHOS (card, avatar, tipografia) — do app WEB, medidos com
// getBoundingClientRect em 1920x1080. E a referencia de TV, feita para ver de
// longe, e por isso ela manda aqui.
//
// ESTRUTURA (secoes empilhadas, cada uma com cabecalho proprio) — do app
// NATIVO DE MAC, /Applications/Nuvio.app, com.nuvio.media.desktop 1.1.22, feito
// em Compose Multiplatform. Foi de la que sairam as capturas de referencia, e
// as classes confirmam o desenho: DetailCastSectionKt, DetailTrailersSectionKt,
// DetailAdditionalInfoSectionKt, DetailProductionSectionKt. O app WEB nao tem
// isso — la e uma fileira de abas que troca o conteudo no lugar.
//
// Os tamanhos daquele app NAO foram copiados: e um app de mesa (breakpoints
// 600/840/1200dp) e no maior deles o avatar de elenco mede 100dp contra os 140
// da TV. So a proporcao interna da tabela de detalhes veio de la, por nao haver
// outra fonte — ver a nota em NV_DETF_DET_*.
//
// OS QUATRO VAOS ABAIXO continuam sendo proporcao tirada das capturas, nao
// medida: o Mac parametriza o espacamento por chamada
// (DetailSectionContainer(horizontalPadding, contentMaxWidth, bottomPadding)),
// entao nao ha uma constante unica para ler no bytecode.
#define NV_DETF_HERO_FIM     1080.0f   // o hero ocupa 0..1080, igual a serie
#define NV_DETF_CAB_H          46.0f   // linha do cabecalho (TXT_HEADLINE, 38)
#define NV_DETF_CAB_GAP        20.0f   // cabecalho -> conteudo
#define NV_DETF_SEC_GAP        64.0f   // fim de uma secao -> cabecalho da proxima
#define NV_DETF_PAD_FIM       130.0f   // padding-bottom do scroller (clamp(116,12vh,168))

// ELENCO. Card 220x193, passo 270, avatar 140 alinhado a ESQUERDA do card.
// Medido no web (.movie-cast-card / .movie-cast-track).
#define NV_DETF_EL_ALT        193.0f
#define NV_DETF_EL_MAX           18    // .slice(0, 18) do web

// TRAILERS. Card 520 de largura, miniatura 520x292 raio 24, passo 582.
// O selo de play e um circulo de 96 a rgba(0,0,0,.48) com o triangulo de 44.
#define NV_DETF_TR_W          520.0f
#define NV_DETF_TR_PASSO      582.0f
#define NV_DETF_TR_VIDEO_H    292.0f
#define NV_DETF_TR_RAIO        24.0f
#define NV_DETF_TR_NOME_DY    302.0f   // topo do card -> nome (28/500 branco)
#define NV_DETF_TR_TIPO_DY    344.6f   // topo do card -> subrotulo (24/400 cinza)
#define NV_DETF_TR_ALT        377.0f
#define NV_DETF_TR_PLAY_D      96.0f

// DETALHES DO FILME. Tabela de duas colunas com divisoria por linha.
//
// A ESTRUTURA E REAL, e foi conferida: o app nativo de Mac (/Applications/
// Nuvio.app, com.nuvio.media.desktop 1.1.22, Compose Multiplatform) tem as
// classes DetailCastSectionKt, DetailTrailersSectionKt e
// DetailAdditionalInfoSectionKt — ou seja, secoes EMPILHADAS com cabecalho
// proprio, e nao a fileira de abas do NuvioWeb-0.3.38-beta. Foi desse app que
// vieram as capturas de referencia.
//
// OS TAMANHOS NAO SAO COPIADOS DE LA, e isso e decisao. Aquele e um app de
// mesa, com breakpoints em 600/840/1200dp; no maior deles o avatar de elenco
// mede 100dp, contra os 140 medidos no app web de TV. Copiar o dp do Mac
// encolheria a tela de quem ve de longe. O que se aproveita e a PROPORCAO
// interna, que nao tinha fonte nenhuma antes:
//
//   Mac DetailInfoRow: coluna do rotulo 176dp num conteudo de 720dp = 24,4%
//
// A largura vem da medida de TV (o bloco de texto do hero, 1040), e a coluna do
// valor comeca nos mesmos 24,4% dela. O passo de linha de 68 bate com a conta
// do Mac reescalada para o corpo de 25 desta tela (25*1.4 + 2*15,6 = 66).
//
// A chave alinha a esquerda em NV_DETP_X; o valor comeca numa coluna FIXA, e
// nao depois do texto da chave — senao a segunda coluna serrilha de linha em
// linha.
#define NV_DETF_DET_LINHA      68.0f   // passo vertical de uma linha
#define NV_DETF_DET_W        1040.0f   // largura da tabela e da divisoria
#define NV_DETF_DET_CHAVE_W   254.0f   // 24,4% de NV_DETF_DET_W (proporcao do Mac)
#define NV_DETF_DET_MAXL          6    // Status, Lancamento, Duracao, Classif., Pais

void detail_evento(const SDL_Event *e);
void detail_atualizar(float dt, Uint32 agora);
void detail_desenhar(Uint32 agora);   // desenhe DEPOIS da home: ele cobre

#endif
