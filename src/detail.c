#include "imdbnota.h"
// Tela de detalhe do titulo, no layout do APP WEB (sessao LOGADA).
//
// O port comecou copiando o app da Apple TV, e cada pedaco dessa heranca foi
// sendo devolvido a medida que o web era MEDIDO. O que restava dela ate agora
// era tudo o que fica abaixo da dobra — pilulas de temporada de 236x63, card de
// episodio com o texto ABAIXO da miniatura, secoes "Trailers", "Como assistir"
// e "Sobre". Nada disso existe no web. O que existe, medido em 1920x1080 na
// serie "Silo" com o perfil do dono:
//
//   1. A tela e UM documento rolavel de 2144px de altura. O hero ocupa os
//      primeiros 1080 e ROLA junto: nao ha cabecalho fixo nem logo centralizado
//      no topo. Descer nao "estica" nada — apenas rola.
//   2. As secoes sao quatro: abas de temporada (269x80), fileira de episodios
//      (cards 640x422 com o texto DENTRO da miniatura), abas de informacao
//      ("Criador e elenco | Avaliacoes | Mais como este | Trailer") e a fileira
//      de elenco (avatar 140 redondo).
//   3. Rolar leva o topo do grupo focado a 33% da altura util (40% nas abas de
//      informacao). Isto esta no fonte do web (DETAIL_ROW_FOCUS_TARGET) e foi
//      conferido medindo o scrollTop nos quatro grupos.
//   4. Ao rolar, a arte de fundo NAO desfoca: ela vai a 15% de opacidade em
//      0.8s. O desfoque gaussiano era do app da Apple TV.
#include "detail.h"
#include "focoprof.h"
#include "posterprov.h"
#include "episodios.h"
#include "fontepref.h"
#include "idioma.h"
#include "ctxmenu.h"
#include "ctxlista.h"
#include "ilha.h"
#include "idiomacod.h"
#include "badges.h"
#include "marco.h"
#include "ajustes.h"
#include "home.h"
#include "extras.h"
#include "trailer.h"
#include "trailerimdb.h"
#include "trailerapple.h"
#include "trailerfonte.h"
#include "player.h"
#include "agenda.h"
#include "agendaui.h"
#include "vistoep.h"
#include "pessoa.h"
#include "streams.h"
#include "descoberta.h"
#include "diretor.h"
#include "gfx.h"
#include "botoes.h"
#include "vertudo.h"
#include "text.h"
#include "tex_cache.h"
#include "focus.h"
#include "anim.h"
#include "trailercinema.h"
#include "revela.h"
#include "textogate.h"
#include "layout.h"
#include "corviva.h"
#include "trocaarte.h"
#include "detmais.h"
#include "catalogo.h"
#include "artehero.h"
#include "recomenda.h"
#include "reacao.h"
#include "recenviar.h"
#include "serieaud.h"
#include "seriefrases.h"
#include "fundo.h"
#include "notasui.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include "ponteiro.h"
#include "plrui.h"
#include "svdesenho.h"
#include "amigostitulo.h"
#include "amigostitulo_ui.h"
#include "temporadas_grafico.h"
#include "detrotulo.h"
static void ponteiroDetalhe(int r, int c);   // ponteiro do Magic Remote (#99)
static int moverFileira(int dy);
static void heroReiniciar(void);

// Teto de itens por secao. 24 e nao 8: uma temporada de "Silo" tem 10
// episodios e o vetor de 8 escondia os dois ultimos — a lista parecia menor do
// que a serie e.
// Teto ANTIGO: 24. Como a lista de episodios e UNICA (todas as temporadas
// juntas, ver irParaTemporada), 24 nao cobre nem uma serie media: da terceira
// temporada em diante os episodios sumiam do foco e a aba de temporada nao
// achava para onde ir.
#define N_ITENS    240
// SEIS secoes, mas nenhum titulo usa as seis: serie acende as quatro primeiras
// e filme acende as tres ultimas. As que nao valem para o tipo devolvem 0 em
// secaoN, e focus_mover PULA fileira vazia — entao a ordem do enum ja entrega a
// ordem visual certa nos dois casos, sem tabela de tradusao no meio:
//   serie -> Temporadas, Episodios, Abas, Elenco
//   filme -> Elenco, Trailers, Detalhes
// Cartao de "Recomendacoes" e de "Comentarios". Ficavam junto das funcoes que
// os desenham, la embaixo; subiram porque larguraItem e xItem, no topo,
// precisam deles para posicionar as SECOES novas.
// Glass UI (mockup "Detalhe", "Mais como este"): cartaz 218x327, vao 24.
#define REL_CARD_W   212.0f
#define REL_CARD_H   318.0f
#define REL_CARD_GAP  32.0f

// MINI CARD DA COLECAO (#194, 2a volta). Um cartao UNICO deitado, e nao uma
// fileira de cartazes: o dono viu a lista de nomes e pediu algo que "nao fique
// igual a fileira de recomendados". Largura entre a miniatura de trailer (520)
// e o card de episodio (640x414) da mesma pagina, canto do trailer.
#define COL_CARD_W   760.0f
#define COL_CARD_H   300.0f
#define COL_CARD_RAIO 24.0f
#define COL_CARD_PAD  36.0f
// Cartazes em escada no lado direito do card: o da frente e o primeiro da
// saga, os de tras menores e deslocados para a esquerda.
#define COL_CAPA_H   228.0f
#define COL_CAPA_W   152.0f
#define COL_CAPA_DX   72.0f
// Tela de lista: coluna da colecao a esquerda, partes a direita.
#define COLL_TOPO    120.0f
#define COLL_ESQ_W   420.0f
#define COLL_X       620.0f
#define COLL_LIN_H   236.0f
#define COLL_LIN_GAP  18.0f
#define COLL_PO_W    140.0f
#define COLL_PO_H    210.0f

// Cartao de produtora/rede: o dobro aproximado do `.detail-company-card` do web
// (180x70 em px de CSS) — na TV 1080p os cards medem em torno disto.
// Glass UI (mockup "Detalhe", "Producao"): o .estu e uma pilula de 52 com o
// nome em 18/500 a 80%, padding 22, vao 12, quebrando linha na largura da
// coluna.
#define EST_CARD_W 240.0f
#define EST_CARD_H   100.0f
#define EST_GAP       18.0f
#define EST_PAD       22.0f
// MEDIDO na referencia (TCL, 1920x1080): cartao 722x466, vao 25, canto 20.
// 722x466 era o MEDIDO na TCL; o dono, olhando a C9 em 19/09/2026, mandou
// encolher ("ta gigante") — o mesmo veredito dos botoes do detalhe na 1.3.2.
// 600x340 mantem a proporcao e as seis linhas de texto com leading de 30.
// Glass UI (mockup "Detalhe", "O que estao dizendo"): tres blocos .gl de
// 560x230 com vao de 24, padding 28, raio 26.
#define COM_CARD_W   600.0f
#define COM_CARD_H   340.0f
#define COM_PAD       24.0f
#define COM_CARD_GAP  22.0f
// Altura da CHAMADA das duas secoes sob demanda: a linha do titulo (TXT_HEADLINE,
// 38), a da procedencia e a do custo (TXT_DET_META2, 23), com os mesmos vaos
// que o cabecalho dos proprios modulos usa. Constante e nao medida por txt_linha
// porque ela entra em alturaSecao, que roda no empilhamento de todo quadro.
#define CHAMADA_H    124.0f
// Divisao do par frases | ficha. 1040 e a largura do bloco de texto do heroi
// (NV_DETW2_TEXTO_W) e dos "Detalhes do Filme", e os 592 que sobram sao
// exatamente a largura em que a ficha de producao foi desenhada e julgada (ver
// a nota no fim de seriefrases.c). 1040 + 96 + 592 = 1728, a faixa inteira.
#define FR_COL_W    1040.0f
#define FR_COL_GAP    96.0f

#define N_SECOES    17
// Trilho do segmentado de temporadas (.seg): 5 px de folga em volta dos itens.
#define DET_SEG_PAD   5.0f
#define N_ELENCO    6

static HomeItem item;
static int  aberto = 0, saindo = 0;
static int  idx = 0;                 // titulo atual dentro do acervo
// A IDENTIDADE do titulo aberto, e uma copia dele. Ver revalidarIdx.
// Do tamanho do CatItem.imdb: com [24] o id de canal ao vivo ("pp-live:...",
// mais de 23 caracteres) era cortado, revalidarIdx nunca o achava e reinseria a
// copia da abertura a cada quadro (D1 da 1.7.0: "saiu de 740 para 741"...).
static char idxImdb[sizeof(((CatItem *)0)->imdb)];
static CatItem idxCopia;
static int  idxTemCopia;
// Ultima revisao do catalogo que esta pagina ja tratou. Ver detail_atualizar.
static unsigned revistaVista;
// cat_geracao_episodios() da ultima vez que se conferiu a lista. Zerada na
// abertura (abrirInterno) para o primeiro quadro conferir sempre.
static unsigned epGerVista; static int epGerConferido;
// O tipo (serie ou nao) com que os extras foram pedidos nesta abertura. Item
// de tipo incerto ("anime" de catalogo do AIOMetadata) abre como filme e o
// /meta o resolve como serie depois (descoberta.c, buscarEps): ai os extras
// sao repedidos por /tv e /shows. Ver o laco de atualizacao.
static int extrasSerie;
static float t = 0.0f;               // 0 = card na home, 1 = tela cheia
// Dois estados, nao tres: o hero (nivel 0) e a pagina rolada (nivel 1). O
// nivel intermediario "cartao vira tela cheia" so fazia sentido enquanto havia
// cartao; no web a tela ja nasce cheia.
static int  nivel = 0;
// Ficha da pessoa por cima da tela de titulo. Nao e um `nivel` a mais porque
// nao e um estado da MESMA pagina: e outra tela, que aparece e sai inteira.
static int  pessoaAberta;
// SEGURAR OK NUMA LISTA DA PAGINA = o menu do cartaz (ctxlista.h): filmografia,
// lista da saga e a fileira "Recomendações". Os retangulos sao os do ultimo
// desenho do cartao em foco (antes do zoom do foco).
static CtxHold holdLista;
static GfxRect relFocoRect, colFocoRect, pesFocoRect;
static char    relFocoArte[1024], colFocoArte[1024], pesFocoArte[1024];
static int  pessoaFoco;
// Primeira LINHA visivel da filmografia. A grade tem 6 por linha e cabem duas
// linhas na tela; sem isto o resto dos creditos era cortado sem aviso.
static int  pessoaLinha;
static int  pedAbrir = -1;
// DE ONDE SE VEIO, quando um titulo e aberto de dentro de outro (Recomendacoes,
// filmografia, colecao). Sem isto o Voltar fechava a pagina e caia na Home em
// vez de voltar ao titulo anterior (dono, 05/10/2026). Zerada quando a pagina
// nasce de fora; app.c avisa cada troca por detail_volta_notar.
static int  voltaPilha[8], nVolta, voltando;
// Foco DENTRO da aba "Mais como este", que e uma lista vertical propria e nao
// uma das fileiras horizontais do focus.c.
static int  relFoco;
// TELA DE LISTA DA COLECAO: o OK no mini card abre, por cima da pagina, a saga
// inteira em ordem, uma parte por linha. Mesmo papel da ficha da pessoa: outra
// tela, que come os eventos enquanto esta aberta e sai inteira no Voltar.
static int   colListaAberta, colListaFoco;
static float colListaScroll, colListaVel;
static void abrirListaColecao(void);
static void abrirParteColecao(void);
static void eventoListaColecao(const SDL_Event *e);
static void desenhaListaColecao(float a);
// Temporada escolhida no painel de notas por episodio (indice em extras).
static int  ratTemp;
// 1 depois que ratTemp foi conciliado com a temporada da PAGINA usando a lista
// do Trakt ja carregada. Zero enquanto a lista nao chegou — ver detail_atualizar.
static int  ratSinc;
#define PES_FOTO_W   280.0f
#define PES_FOTO_H   420.0f
#define PES_COL_X    (NV_DETP_X + PES_FOTO_W + 56.0f)
#define PES_CARD_W   212.0f
#define PES_CARD_H   318.0f
#define PES_CARD_GAP  32.0f
#define PES_POR_LINHA  6

static int maisAcoes;
static int  botao = 0;      // botao em foco no hero
static GfxRect maisAncora;  // onde o "..." foi desenhado (ilha detmais.h)
// A ILHA DE AMIGOS (quem assistiu/gostou deste titulo), logo abaixo dos botoes:
// um alvo focavel do nivel 0, entre os botoes e as secoes. Baixo nos botoes cai
// nela, baixo nela entra nas secoes, cima volta aos botoes. OK pede ao roteador
// (app.c) para abrir o painel Social na atividade deste titulo.
static int  focoAmigos = 0;
static int  pedAmigos = 0;
static int  pedExplorar = 0;   // circular "Explorar": o roteador abre a toca neste titulo
#define DET_PTR_AMIGOS 99
#define NV_DETW_AMIGOS_H   76.0f
#define NV_DETW_AMIGOS_GAP 18.0f
static int  amigosDoTitulo(AmigosTitulo *t) {
  const CatItem *c = cat_item(idx);
  return c && c->imdb[0] && amigostitulo_obter(c->imdb, t);
}
// O ULTIMO EPISODIO da serie (a ultima temporada que o extras conhece), para
// "Rafa terminou a série". 0/0 = nao se sabe ainda: a frase nunca e afirmada.
static void ultimoEpisodio(int *t, int *e);
static int  pedReproduzir = 0, pedMarcar = 0, pedFontes = 0;
// Marcar como ASSISTIDO. Separado de pedMarcar, que e "adicionar a lista".
static int  pedAssistido = 0;
static int  pedDoInicio = 0;         // botao "Reproduzir desde o inicio"
static Uint32 okDesceEm = 0;
static float pg = 0.0f;              // 0..1: hero -> pagina rolada
static Foco foco;
static float animFoco[N_SECOES][N_ITENS];
static float scrollSec[N_SECOES];    // rolagem HORIZONTAL de cada fileira
static float scrollY = 0.0f;         // rolagem VERTICAL do documento
// Velocidade da mola de 2a ordem da rolagem (anim_mola2): partida macia e
// cauda exponencial, a MESMA curva que a home mede. A de 1a ordem que estava
// aqui partia na velocidade maxima e o primeiro quadro ja saltava 12%.
static float velSec[N_SECOES], velY = 0.0f;
// Miniatura de episodio e cartaz relacionado chegando: esvanecem sobre o
// esqueleto em vez de trocar num quadro (revela.h). Um registro por coluna.
#define DET_REV_EP 64
static RevelaArte revEp[DET_REV_EP], revRel[EX_REL_MAX];
// TRAILER NO FUNDO (trailer.h). `trailerDesde` e o instante em que a pagina
// assentou, para o autoplay esperar a pessoa ler antes de a arte virar
// video; `trailerTentado` garante uma tentativa por abertura (o video acaba,
// a arte volta e fica); `trailerFade` e a mistura arte -> video.
static Uint32 trailerDesde = 0;
static int    trailerTentado = 0;
// ESCADA COM PRAZO do autoplay na Samsung: a fonte aberta (TRF_APPLE,
// TRF_YOUTUBE de trailerfonte.h), 0 = nada aberto, -1 = acabou a escada. Antes a pagina abria a Apple e esperava para
// sempre — o log 1646 mostra o HLS pedido e nenhuma linha depois. Agora, sem
// `playing` em NV_TRAILER_PREPARA_MS (ou com erro), fecha, loga e tenta o
// proximo degrau uma vez.
static int    trailerEtapa = 0;
static Uint32 trailerPrazo = 0;
// ESQUERDA no comeco de QUALQUER fileira da pagina (e na coluna 0 da
// filmografia, e na lista da colecao): o app abre a barra lateral POR CIMA da
// pagina, que continua viva embaixo — mesmo foco, rolagem e trailer (dono,
// 03/10: "pode abrir por cima"). Antes a tecla ligava `saindo` junto e a
// pagina fechava para a barra abrir sobre a tela de baixo. Voltar/DIREITA na
// barra devolvem o foco aqui; um destino escolhido fecha a pagina (app.c).
static int    pediuSocial = 0; // CIMA no alto da pagina: abrir Salvos/Avisos (ver app.c)
static int    pediuMenu = 0;   // ESQUERDA na borda: abrir a barra por cima (ver app.c)
// A barra esta aberta por cima (detail_sob_menu): o trailer segue tocando,
// MUDO, e o som volta quando ela fecha. `somAntesMenu` lembra se tinha som.
static int    sobMenu = 0, somAntesMenu = 0;
static float  trailerFade = 0.0f;

// --- CARROSSEL DA DINAMICA (layout "Dinâmica (Apple TV)" da home) ----------
//
// Na Dinamica o titulo aberto de uma fileira vira um CARTAO grande, quase da
// tela, com a borda dos vizinhos da mesma fileira espiando dos dois lados; a
// esquerda/direita na ponta da linha de botoes anda pelos titulos da fileira,
// baixo estica o cartao ate a pagina cheia e Voltar devolve a fileira no
// titulo em cena. Referencia: o app de TV da Apple, em video do dono.
//
// O MODELO E ABERTURA, e foi o que o app tvOS do dono provou: a arte NUNCA
// escala. Ela e desenhada em "cover" no quadro da tela cheia e o cartao e
// uma janela arredondada sobre ela (GFX_JANELA); esticar o cartao so aumenta
// a janela. Trocar de titulo e TIRA DE FILME: cada titulo e a mesma janela
// deslocada de (largura + vao), cada um com a sua arte e os seus cantos, e o
// vao passa entre eles. A folha cinza por baixo e um clear de tela cheia (o
// caminho rapido de gfx_cor), e os cartoes sao quads opacos por cima: nada
// de camada de tela cheia misturada no estado de repouso.
//
// Fora da Dinamica, ou aberto de outro lugar que nao uma fileira de titulos,
// `carro` fica 0 e a pagina e a de sempre, byte a byte.
#define CAR_MAX        96
#define CAR_X         130.0f   // margem lateral do cartao (medida no video: ~6,8%)
#define CAR_Y          26.0f
#define CAR_BAIXO      26.0f
#define CAR_VAO        34.0f   // entre um cartao e o vizinho
#define CAR_RAIO       40.0f   // canto do cartao, em pixels
#define CAR_RAIO_HOME  22.0f   // canto do cartaz da fileira, de onde ele abre
#define CAR_TEXTO_PAD  72.0f   // borda do cartao -> coluna do texto
#define CAR_TEXTO_SOBE 64.0f   // o bloco de texto sobe para dentro do cartao
#define CAR_VEU         0.86f  // veu do canto de baixo a esquerda, sob o texto
// Tira de filme: ~0,78 s ate assentar (1-(1+wt)e^-wt chega a 99% em wt ~ 6,6).
#define CAR_MOLA        8.5f
#define CAR_MOLA_PAG    9.0f   // cartao -> pagina cheia e a volta
// A folha: cinza escuro frio, o da Apple TV atras do cartao.
#define CAR_FOLHA_R     0.105f
#define CAR_FOLHA_G     0.110f
#define CAR_FOLHA_B     0.125f
static int   carro;                 // 1 = esta abertura e o carrossel
static int   carN, carIdx[CAR_MAX];  // titulos da fileira (indices do catalogo)
static int   carPos;                // titulo pedido pelo D-pad
static char  carPre[CAR_MAX];       // 1 = vizinho ja pre-buscado (ou nada a buscar) nesta abertura
static int   carAplicado;           // titulo cuja pagina esta montada (= idx)
static int   carBotaoFim;           // chegou pela direita: foco no ultimo botao
static int   carFocou;              // a home ja recebeu o titulo da volta
static int   carEsperaRect;         // quadros de home desenhada para ler o cartaz
static float carOff, carVel;        // posicao da tira, em titulos
static float cartao = 1.0f, cartaoVel;  // 1 = cartao, 0 = pagina cheia
// TELA CHEIA NO TOPO (dono, 01/10): a PRIMEIRA seta para baixo so estica o
// cartao ate a tela inteira — arte e texto vao para as margens, sem rolar;
// a SEGUNDA desce para a pagina (nivel 1). Voltar
// desfaz na ordem inversa: pagina -> tela cheia no topo -> cartao -> fileira.
// `carTxt` e a mola do texto: 1 na posicao do cartao, 0 na da pagina; ela so
// vai a 0 tambem quando o cartao se expande para tela cheia.
static int   carCheia;
static float carTxt = 1.0f, carTxtVel;
static GfxRect carOrigem;           // cartaz da fileira (abrir e fechar)
static float heroDx;                // deslocamento horizontal do bloco do heroi
// MODO CINEMA DO TRAILER (dono, 21/09/2026, com a foto do outro app: "quando
// comecar a tocar o trailer descer a arte do titulo e deixar assim"). Com o
// trailer tocando, o bloco de texto do heroi desce e apaga e so o logo fica,
// pequeno, no canto inferior esquerdo. Qualquer tecla traz o bloco de volta
// (o trailer continua); Voltar fecha o trailer sem sair da pagina.
// A conta (borda de subida, mola, medidas do logo) e de trailercinema.h, que a
// home divide (mesmo modo cinema no destaque).
static TrailerCinema trailerCinema;
// A FONTE do trailer `k` desta pagina, no formato que trailer_abrir espera:
// URL (Apple HLS nos dois alvos, MP4 do IMDb na LG) ou id do YouTube (Samsung,
// lista do TMDB). NULL quando ainda nao ha — ou quando nao vai haver, e ai
// loga uma vez por pagina. `*qual` recebe o TRF_* escolhido.
//
// A ORDEM e do ajuste "Fonte do trailer" (trailerfonte.h). Automatico: Apple,
// e so depois de a Apple RESPONDER a seguinte — senao o IMDb/YouTube ganharia
// sempre por chegar antes. Na Samsung o YouTube e reserva e nao primeiro: o
// embed cai em "Video player configuration error" (wgt de file://, sem
// Referer; #82/#86 na AU7000).
//
// SEM "COM SOM -> YOUTUBE". Ate 965bea2 a tela cheia da Samsung pulava a
// Apple (so video, trailerapple.c varianteMidia) atras do som do YouTube; o
// dono decidiu (22/09/2026) "trailer fica mudo": a tela cheia segue a mesma
// ordem do fundo, e trailer.c forca o mudo nesse alvo.
//
// `cheia` 1 = o botao Trailer (tela cheia, com som onde ha): no .tpk, em
// Automatico, o IMDb vem antes da Apple, porque la a Apple e so video
// (trailerfonte_escolher_cheia, #178). O fundo passa 0 e nao muda.
#ifdef NV_TPK
static char trailerCheiaLogada[32];
#endif
static int trailerSemFonteLogado = 0;
static const char *trailerFonte(int k, int *qual, int cheia) {
  const CatItem *ci = cat_item(idx);
  TrailerCandidatos c;
  const char *u = NULL;
  int q = 0, aj = trailerfonte_ajuste(), tz = trailerfonte_tizen();
  TrailerDecisao d;
  memset(&c, 0, sizeof c);
  if (qual) *qual = 0;
  if (ci && ci->imdb[0]) {
    c.apple = trailerapple_url(ci->imdb);
    c.appleRespondeu = trailerapple_respondeu(ci->imdb);
    // IMDb tambem na Samsung quando a build tem por onde perguntar (#136):
    // MP4 direto no <video> do app, antes do iframe do YouTube.
    if (!tz || trailerfonte_imdb_tizen()) {
      c.imdb = trailerimdb_url(ci->imdb, NULL);
      c.imdbRespondeu = trailerimdb_respondeu(ci->imdb);
    } else c.imdbRespondeu = 1;
  } else c.appleRespondeu = c.imdbRespondeu = 1;   // sem id nao ha o que esperar
#ifdef __EMSCRIPTEN__
  if (k >= 0 && k < extras_n_trailers() && extras_trailer_yt(k)[0]) c.youtube = extras_trailer_yt(k);
  else if (extras_n_trailers() > 0 && extras_trailer_yt(0)[0]) c.youtube = extras_trailer_yt(0);
#else
  (void)k;
#endif
  c.youtubeRespondeu = !extras_carregando();
  d = cheia ? trailerfonte_escolher_cheia(aj, tz, trailerfonte_com_som(tz), &c, &u, &q)
            : trailerfonte_escolher(aj, tz, &c, &u, &q);
  if (d == TRF_ABRE) {
    if (qual) *qual = q;
#ifdef NV_TPK
    // UMA linha por titulo: isto roda a cada quadro, e o log do testador da
    // Samsung (corte de 200 KB) saiu com 1693 copias e nada mais (05/10/2026).
    if (cheia && ci && strcmp(trailerCheiaLogada, ci->imdb)) {
      snprintf(trailerCheiaLogada, sizeof trailerCheiaLogada, "%s", ci->imdb);
      printf("[trailer] detalhe: tela cheia pela fonte %s (ajuste %d)\n", trailerfonte_nome(q), aj); fflush(stdout);
    }
#endif
    return u;
  }
  if (d == TRF_NENHUMA && !trailerSemFonteLogado) {
    trailerSemFonteLogado = 1;
    printf("[trailer] detalhe: sem trailer (ajuste %d, apple %s, imdb %s, youtube %s)\n", aj,
           c.apple ? "tem" : c.appleRespondeu ? "sem" : "?",
           tz && !trailerfonte_imdb_tizen() ? "n/a" : c.imdb ? "tem" : c.imdbRespondeu ? "sem" : "?",
           !tz ? "n/a" : c.youtube ? "tem" : "sem");
    fflush(stdout);
  }
  return NULL;
}
// A URL que a fonte `qual` tem para o titulo agora, ou NULL. E o degrau
// seguinte do prazo do fundo (detail_atualizar): la a ordem ja foi decidida,
// so falta saber se aquela fonte tem o que tocar.
#ifdef __EMSCRIPTEN__
static const char *trailerUrlDaFonte(int qual) {
  const CatItem *ci = cat_item(idx);
  switch (qual) {
    case TRF_APPLE: return ci && ci->imdb[0] ? trailerapple_url(ci->imdb) : NULL;
    case TRF_IMDB:  return ci && ci->imdb[0] && trailerfonte_imdb_tizen() ? trailerimdb_url(ci->imdb, NULL) : NULL;
    case TRF_YOUTUBE:
      return extras_n_trailers() > 0 && extras_trailer_yt(0)[0] ? extras_trailer_yt(0) : NULL;
    default: return NULL;
  }
}
#endif
static int temporada = 0;            // temporada ESCOLHIDA (nao a focada)
// Repouso do foco sobre a fileira de temporadas, para trocar de temporada ao
// PARAR numa pilula em vez de a cada pilula por que se passa.
// Episodio para o qual a aba de temporada APONTA. A rolagem da fileira de
// episodios usa este indice enquanto o foco esta na fileira de temporadas —
// antes a troca de temporada arrastava o FOCO para o episodio, e com isso o
// D-pad saia da fileira de abas: nao dava para passar da segunda temporada.
static int    epAncora = 0;
// Comentarios: 0 = da SERIE, 1 = do EPISODIO. E o seletor que a referencia poe
// sob "Avaliações do Trakt". Em filme nao existe e fica cravado em 0.
static int comentEp = 0;
// O EPISODIO DOS COMENTARIOS: o ultimo que o foco pisou na fileira de
// episodios (temporada/numero), 0 enquanto o foco nao passou por la. O dono
// (20/09/2026): "nao ta seguindo o episodio selecionado" — episodioAlvo() so
// le o foco ENQUANTO ele esta na fileira; ao descer ate os cartoes o foco ja
// saiu de la e a conta caia no "retomar"/"proximo", que e outro episodio.
static int comEpT = 0, comEpE = 0;
static int abaInfo = 0;              // aba de informacao escolhida

// AS DUAS SECOES QUE SO CARREGAM QUANDO ALGUEM ENTRA NELAS.
//
// serieaud custa 1 pedido do /stats da serie MAIS 1 por episodio (teto 24);
// seriefrases custa 2 (um SPARQL no Wikidata, um parse do Wikiquote). Nenhum
// dos dois pode sair na abertura da pagina: a maioria das visitas a um titulo
// nunca rola ate aqui, e quem so quer apertar "Reproduzir" pagaria 25 viagens
// por nada. A disciplina e a que a secao de comentarios ja tem para os
// comentarios de EPISODIO (extras.h): o conteudo que custa so e pedido — e so e
// DESENHADO — quando a pessoa escolhe ver.
//
// "Escolher ver" aqui e o FOCO ENTRAR na secao, e nao ela aparecer na tela.
// A diferenca importa porque nesta pagina nao ha rolagem livre: a rolagem
// persegue a secao focada, entao a secao de baixo fica meia visivel o tempo
// todo em que o foco esta na de cima. Disparar por visibilidade faria os 25
// pedidos so por alguem estar olhando as abas.
//
// Enquanto nao entrou, a secao mostra a CHAMADA (titulo + procedencia + o
// custo) e nada mais. Nao e enfeite: os dois modulos desenham "sem dados" com
// lista vazia e sem fio no ar, e essa frase seria MENTIRA antes do primeiro
// pedido — o app estaria dizendo que o titulo nao tem frases sem nunca ter
// perguntado.
static int audAberta;         // a temporada aberta abaixo ja foi pedida
static int audTempAberta = -1;// NUMERO da temporada que audAberta descreve
// Ultimo NUMERO de temporada que a pagina mostrou. Serve a duas coisas de uma
// vez: fechar a audiencia quando a escolha muda e manter a grade de pastilhas
// da aba "Avaliações" na MESMA temporada dos graficos.
static int audTempVista = -1;
static int frasesAberta;
// Altura MEDIDA no ultimo desenho das frases: o modulo so diz quanto ocupou
// DEPOIS de desenhar. Um quadro de atraso, so na troca chamada -> pronto, e a
// secao e a ultima do documento (mexe so no docFim). Os graficos da serie nao
// precisam disto: o bloco "Numeros da temporada" tem altura fixa.
static float frasesAlt = 520.0f;
// 1 quando a fileira `r` e uma das tres bandas de audiencia (so SEC_AUD_ARCO,
// o bloco "Numeros da temporada", tem colunas hoje).
#define EH_AUD(r) ((r) >= SEC_AUD_ARCO && (r) <= SEC_AUD_DIGITAL)

// As quatro secoes do web, com o topo do GRUPO em coordenada de documento — e
// nao uma pilha de alturas somadas, que era o modelo do app da Apple TV. As
// posicoes sao fixas porque no web tambem sao: o documento tem tamanho
// conhecido e a rolagem so muda o quanto dele se enxerga.
// A AUDIENCIA SAO TRES FILEIRAS E NAO UMA, e isto veio da captura, nao do
// desenho: os tres graficos empilhados medem ~1290 px e a tela tem 1080. A
// rolagem desta pagina persegue o TOPO da fileira focada (33% da altura util) e
// mais nada — dentro de uma fileira o D-pad so anda na horizontal. Com os tres
// numa fileira so, o radar e a impressao digital ficavam ABAIXO DA DOBRA sem
// nenhum jeito de chegar neles: uma secao alcancavel cuja maior parte era
// inalcancavel. Tres fileiras dao ao D-pad o passo que faltava, e cada descida
// traz a proxima para os mesmos 33%. E tambem a gramatica do resto da pagina:
// uma fileira focavel por banda.
//
// SEC_AUDIENCIA e SEC_FRASES entraram DEPOIS e a posicao delas no enum nao e
// arbitraria: a ordem do enum E a ordem do D-pad e a ordem de empilhamento do
// filme (recalcularLayout percorre 0..N_SECOES). Audiencia fica logo abaixo do
// slot das abas porque e a continuacao do que a aba "Avaliações" comeca —
// numeros desta temporada.
//
// FRASES SUBIU ACIMA DE "Detalhes do Filme" em 16/09, pedido do dono olhando a
// tela rodando: "quotes pode ser pra cima do movie details". Ela tinha sido
// posta por ULTIMO com o argumento de ser a unica secao que vale nos dois tipos
// e a unica cuja fonte nao e nem Trakt nem TMDB — procedencia, que e um
// criterio de arquiteto e nao de quem esta assistindo. Pela leitura o argumento
// se inverte: frase celebre e sobre o FILME, e "Detalhes do Filme" e a ficha
// tecnica, o rodape natural de qualquer pagina de titulo. Ficha tecnica nao
// merece vir antes de fala memoravel.
//
// A ordem da SERIE nao muda com isto: aquele caminho empilha secao por secao
// com topoSec explicito em recalcularLayout, e SEC_DETALHES nem existe la.
typedef enum { SEC_TEMPORADAS, SEC_EPISODIOS, SEC_ABAS_INFO, SEC_ELENCO, SEC_RELACIONADOS,
               SEC_AUD_ARCO, SEC_AUD_RADAR, SEC_AUD_DIGITAL,
               // NOTAS: heatmap das fontes + resumo (e, em serie, a grade de
               // episodios). Vem logo depois da audiencia por ser o fecho do
               // que a aba "Avaliacoes" comeca; em filme, onde nao ha abas,
               // e o UNICO lugar em que as notas aparecem alem da linha do
               // titulo.
               SEC_NOTAS, SEC_NOTAS_EP,
               SEC_TRAILERS,
               // COLECAO do filme (#194): logo abaixo das recomendacoes, as
               // duas respondem "o que ver depois deste".
               SEC_COLECAO, SEC_COMENTARIOS,
               SEC_ESTUDIOS, SEC_FRASES, SEC_DETALHES,
               // SEU PROGRESSO (grafico de temporadas, temporadas_grafico.h):
               // so na serie, entre os episodios e as Notas.
               SEC_PROGTEMP } TipoSecao;
// A ORDEM DO FILME no Glass UI (mockup "Detalhe", 03/10): Trailers e extras,
// Elenco, Notas, O que estao dizendo, Mais como este, Colecao, Frases, Ficha
// tecnica e Producao. Ela NAO e a ordem do enum, porque o enum tambem e a
// ordem do D-pad da SERIE (que empilha diferente); no filme o empilhamento
// (recalcularLayout) e o D-pad (moverFileira) seguem esta tabela.
//
// NOTAS (dono, 03/10): no FILME e a PRIMEIRA secao abaixo do hero; na SERIE vem
// logo abaixo dos episodios, antes das abas/elenco e dos graficos da serie. A
// serie tem a propria tabela porque o enum (que a serie usava como ordem do
// D-pad) mantem as Notas depois da audiencia.
static const int ORDEM_FILME[] = {
  SEC_NOTAS, SEC_TEMPORADAS, SEC_EPISODIOS, SEC_ABAS_INFO, SEC_ELENCO, SEC_RELACIONADOS,
  SEC_AUD_ARCO, SEC_AUD_RADAR, SEC_AUD_DIGITAL, SEC_NOTAS_EP,
  SEC_TRAILERS, SEC_COLECAO, SEC_COMENTARIOS,
  SEC_ESTUDIOS, SEC_FRASES, SEC_DETALHES, SEC_PROGTEMP };
// NUMEROS DA TEMPORADA (Glass UI 1.8, mockup "Notas e graficos"): os tres
// graficos da serie viraram UM bloco logo depois das Notas. Ele usa a fileira
// SEC_AUD_ARCO (SEC_NUMEROS); RADAR, DIGITAL e NOTAS_EP ficaram sem colunas.
static const int ORDEM_SERIE[] = {
  SEC_TEMPORADAS, SEC_EPISODIOS, SEC_PROGTEMP, SEC_NOTAS, SEC_AUD_ARCO, SEC_ABAS_INFO, SEC_ELENCO,
  SEC_RELACIONADOS, SEC_AUD_RADAR, SEC_AUD_DIGITAL, SEC_NOTAS_EP,
  SEC_TRAILERS, SEC_COLECAO, SEC_COMENTARIOS,
  SEC_ESTUDIOS, SEC_FRASES, SEC_DETALHES };
#define N_ORDEM ((int)(sizeof ORDEM_FILME / sizeof ORDEM_FILME[0]))
#define SEC_NUMEROS SEC_AUD_ARCO
static int ehSerie(void);
static const int *ordemSecoes(void) { return ehSerie() ? ORDEM_SERIE : ORDEM_FILME; }
// Definida adiante, junto do resto das consultas ao catalogo; declarada aqui
// porque recalcularLayout, cabecalhoDe e nAvaliaveis, todas acima dela,
// precisam separar serie de filme.
static int ehSerie(void);
static int nAbasInfo(void);
static float estAltura(void);
static float yItem(int r, int c);
static int epApple(void);
static float epExtraAltura(void);
static float epAppleExtra(void);
static float notasBloco(void);
static float notasTopoSerie(void);
static float progTopoSerie(void);
static float numerosTopoSerie(void);
static float alturaSecao(int r);
static float epTempY(void);
static float epRowY(void);
static float serieGrupo(int r);
static const NotasSecao *notasDados(void);
static float alturaCabComentarios(void);
static int temporadaEm(int c);
static int epAbsoluto(int c);
static int epVisiveis(void);
static float baseDaAbaAtiva(void);
// A TEMPORADA ESCOLHIDA NA PAGINA, traduzida para o indice de extras.h.
//
// Sao duas contagens diferentes e misturar as duas e o mesmo defeito que o
// tests/detail_eps.sh guarda para a fileira de episodios: `temporada` e a
// posicao da PILULA (0,1,2...) e extras guarda as temporadas na ordem que o
// Trakt devolveu, sem os "Especiais" (extras.c filtra `number > 0`). Numa serie
// com especiais os dois indices divergem em um, e o grafico sairia com a cara
// certa e os episodios de outra temporada.
//
// Devolve -1 quando a temporada escolhida nao esta na lista do Trakt — e ai a
// secao de audiencia nao existe, porque nao ha episodio nenhum para plotar.
static int audTemp(void) {
  int alvo, t, n = extras_n_temporadas();
  if (!ehSerie() || n <= 0) return -1;
  // A TEMPORADA DOS GRAFICOS E A DA ABA "AVALIACOES" (dono, 20/09/2026: "ao
  // mexer no rating logo acima do grafico ele tem que mudar junto"). A aba
  // segue a pagina quando a pagina troca (conciliacao mais abaixo) e pode
  // andar sozinha com esquerda/direita; os graficos seguem a aba, que e o
  // que esta logo acima deles.
  if (ratSinc && ratTemp >= 0 && ratTemp < n) return ratTemp;
  alvo = temporadaEm(temporada);
  for (t = 0; t < n; t++) if (extras_temporada_numero(t) == alvo) return t;
  return -1;
}
// Abre (ou troca) a audiencia para a temporada de audTemp(). As notas ja
// estao em memoria; serieaud_abrir na mesma temporada e barato.
static void abrirAudiencia(void) {
  int t = audTemp();
  const CatItem *ci = cat_item(idx);
  if (ci && ci->imdb[0] && t >= 0) {
    int numeros[EX_EP_MAX], notas[EX_EP_MAX];
    int i, n = extras_n_eps(t);
    if (n > EX_EP_MAX) n = EX_EP_MAX;
    for (i = 0; i < n; i++) {
      numeros[i] = extras_ep_numero(t, i);
      notas[i]   = extras_ep_nota(t, i);
    }
    serieaud_abrir(ci->imdb, extras_temporada_numero(t), numeros, notas, n);
    audAberta = 1;
    audTempAberta = extras_temporada_numero(t);
  }
}
// A fileira de comentarios e definida junto do desenho dela, la embaixo, mas a
// contagem de colunas e a largura de item — que ficam aqui em cima — precisam
// perguntar quantas pilulas e quantos cartoes ela tem.
#define COM_PILL_GAP  16.0f
static const char *COM_ROT[2];
static const char *rotuloPilulaCom(int k);
static int   nPilulasCom(void);
static int   nCartoesCom(void);
static float larguraPilulaCom(const char *rot);
static int  secaoN(int r);
static float alturaSecao(int r);

// LAYOUT DO DOCUMENTO, recalculado a cada quadro.
//
// Duas leis diferentes, e de proposito:
//
// SERIE — coordenadas ABSOLUTAS medidas no aparelho (NV_DETP_G_*). Nao viram
// fluxo. O comentario em detail.h:70 registra o que aconteceu quando alguem
// tentou deduzi-las por soma de alturas: a rolagem batia no teto cedo demais e
// a fileira de elenco parava meio ecra fora do lugar.
//
// FILME — EMPILHADO. As secoes de filme (Elenco, Trailers, Detalhes) tem altura
// que depende do conteudo, e nao ha medida de aparelho para copiar. Aqui o topo
// de cada uma e a soma do que veio antes, que e como o web se comporta de fato:
// o bloco que nao existe nao ocupa altura.
//
// `topoSec` e o topo do GRUPO (a linha do cabecalho). O conteudo comeca em
// `conteudoSec`, e e ELE o alvo da rolagem — o web mira o topo do TRILHO, nao
// o do grupo (focusInList, metaDetailsScreen.js:7936).
static float topoSec[N_SECOES], conteudoSec[N_SECOES], alvoSec[N_SECOES];
// Filme: deslocamento horizontal e largura de cada secao (os pares do mockup
// dividem a linha; o resto ocupa a faixa inteira de 1728).
static float dxSec[N_SECOES], wSec[N_SECOES];
static float docFim = NV_DETP_FIM;

// Cabecalho de secao: so o filme tem. Na serie o rotulo "Temporadas" e desenhado
// pelo caminho antigo, e "Elenco" ficaria repetindo a aba "Criador e elenco"
// logo acima (ver detail.c:1611).
static const char *cabecalhoDe(int r) {
  // TRAILERS E A EXCECAO na serie (#123): a fileira entrou empilhada abaixo
  // das bandas de audiencia e nao ha aba acima dela que diga o que ela e.
  // ESTUDIOS TAMBEM (serie): o titulo era uma linha cinza pequena desenhada
  // DENTRO da secao, e ao lado do "Trailers" e do "Estúdios" do filme parecia
  // de outra pagina. Agora e o mesmo TXT_HEADLINE, com o mesmo vao.
  if (r == SEC_NOTAS) return "Notas";
  if (r == SEC_NOTAS_EP) return "Notas por episódio";
  if (r == SEC_RELACIONADOS) return "Recomendações";
  if (r == SEC_ELENCO && nAbasInfo() <= 1) return "Elenco";
  if (ehSerie()) {
    if (r == SEC_TRAILERS) return "Trailers";
    if (r == SEC_ESTUDIOS) return "Redes e estúdios";
    return NULL;
  }
  switch (r) {
    case SEC_ELENCO:   return "Elenco";
    case SEC_TRAILERS:     return "Trailers";
    case SEC_RELACIONADOS: return "Recomendações";
    case SEC_COLECAO:      return "Coleção";
    // Sem cabecalho de secao: a propria secao ja abre com "trakt Comentários" e
    // o subtitulo "Avaliações do Trakt". Com os dois saiam DOIS titulos
    // empilhados dizendo a mesma coisa.
    case SEC_COMENTARIOS:  return NULL;
    case SEC_ESTUDIOS:     return "Estúdios";
    case SEC_DETALHES:     return "Detalhes do Filme";
    // As duas abaixo trazem o proprio titulo DENTRO do painel (os modulos
    // desenham "Arco de qualidade"/"Frases" com a linha de procedencia logo
    // abaixo, que e a parte que nao pode ser separada do titulo). Um cabecalho
    // externo seria o mesmo nome duas vezes, como ja acontecia com os
    // comentarios.
    case SEC_AUD_ARCO:
    case SEC_AUD_RADAR:
    case SEC_AUD_DIGITAL:
    case SEC_FRASES:       return NULL;
    default:           return NULL;
  }
}

// A ABA DE TEMPORADA VOLTOU A SER UM FILTRO. Issue #35.
//
// A lista era UNICA (todas as temporadas emendadas, ordenadas) e a aba so
// levava o foco ao primeiro episodio daquela temporada. A razao daquilo era a
// demora: trocar de aba pedia a temporada a rede. Essa razao NAO EXISTE MAIS —
// desc_episodios ignora o numero da temporada e traz a serie inteira num
// pedido so ("A lista agora e UNICA e cobre todas as temporadas, entao ter
// qualquer episodio deste titulo ja basta"). Com tudo em memoria, filtrar a
// fileira e trabalho de VISTA: nao ha rede, nao ha remontagem, nao ha demora
// para trazer de volta.
//
// O que o relator descreve e o efeito colateral que sobrou: numa serie de tres
// temporadas a fileira mostrava doze cards seguidos e passar de T1E4 para a
// direita caia em T2E1 sem que a aba dissesse nada. A aba virava enfeite.
//
// COLUNA E RELATIVA, cat_episodio() e ABSOLUTO. Todo mundo que le a fileira
// passa por epAbsoluto(); quem conta, por epVisiveis().
static int epAbsoluto(int c) {
  int alvo = temporadaEm(temporada), n = cat_n_episodios(idx), i, j = 0;
  if (c < 0) return -1;
  // Sem lista de temporadas nao ha filtro possivel: a fileira e a lista crua.
  { const CatItem *ci = cat_item(idx);
    if (!ci || ci->nTemporadas < 1) return c < n ? c : -1; }
  for (i = 0; i < n; i++) {
    const CatEp *e = cat_episodio(idx, i);
    if (e && e->temporada == alvo && j++ == c) return i;
  }
  return -1;
}

static int epVisiveis(void) {
  int alvo = temporadaEm(temporada), n = cat_n_episodios(idx), i, q = 0;
  if (n < 1) return 0;
  { const CatItem *ci = cat_item(idx);
    if (!ci || ci->nTemporadas < 1) return n; }
  for (i = 0; i < n; i++) {
    const CatEp *e = cat_episodio(idx, i);
    if (e && e->temporada == alvo) q++;
  }
  return q;
}

// Trocar de aba agora REINICIA a fileira, porque ela passou a mostrar outra
// coisa. A ancora deixa de ser uma posicao no meio da lista emendada e passa a
// ser sempre o comeco.
static void irParaTemporada(int c, int moverFoco) {
  (void)c;
  epAncora = 0;
  foco.colunaLembrada[SEC_EPISODIOS] = 0;
  if (moverFoco && epVisiveis() > 0) { foco.fileira = SEC_EPISODIOS; foco.coluna = 0; }
}

// OK NUMA COLUNA DO GRAFICO "Seu progresso": a pagina passa para aquela
// temporada e o foco sobe para a fileira de episodios, no primeiro episodio
// que voce ainda nao viu (o primeiro de todos se ja viu a temporada inteira).
static void progAbrirTemporada(int col) {
  const TgDados *d = tgraf_dados(idx);
  const CatItem *ci = cat_item(idx);
  int k, c, q;
  if (!ci || col < 0 || col >= d->n) return;
  for (k = 0; k < ci->nTemporadas; k++) if (ci->temporadas[k] == d->t[col].numero) break;
  if (k >= ci->nTemporadas) return;
  temporada = k;
  foco.colunaLembrada[SEC_TEMPORADAS] = k;
  irParaTemporada(k, 0);
  q = epVisiveis();
  if (q < 1) return;
  for (c = 0; c < q; c++) {
    const CatEp *e = cat_episodio(idx, epAbsoluto(c));
    if (e && vistoep_estado(ci->imdb, e->temporada, e->episodio) != 1) break;
  }
  if (c >= q) c = 0;
  foco.nColunas[SEC_EPISODIOS] = q;
  foco.colunaLembrada[SEC_PROGTEMP] = col;
  foco.fileira = SEC_EPISODIOS; foco.coluna = c;
  epAncora = c; foco.colunaLembrada[SEC_EPISODIOS] = c;
  { const CatEp *ep = cat_episodio(idx, epAbsoluto(c));
    comEpT = ep ? ep->temporada : 0; comEpE = ep ? ep->episodio : 0; }
}

// Troca de temporada pela PONTA da fileira de episodios (dir=+1 direita,
// -1 esquerda). Devolve 1 se trocou. So age quando o foco ja esta na ultima
// (dir>0) ou primeira (dir<0) coluna e existe temporada do outro lado.
static int detail_ep_borda(int dir) {
  const CatItem *ci = cat_item(idx);
  int nt = ci && ci->nTemporadas > 0 ? ci->nTemporadas : 1, nova = temporada + dir, q, col;
  if (nova < 0 || nova >= nt) return 0;
  if (dir > 0 ? foco.coluna < foco.nColunas[SEC_EPISODIOS] - 1 : foco.coluna > 0) return 0;
  temporada = nova;
  irParaTemporada(temporada, 0);
  q = epVisiveis();
  col = dir > 0 || q < 1 ? 0 : q - 1;
  foco.nColunas[SEC_EPISODIOS] = q;
  foco.fileira = SEC_EPISODIOS; foco.coluna = col;
  epAncora = col; foco.colunaLembrada[SEC_EPISODIOS] = col;
  foco.colunaLembrada[SEC_TEMPORADAS] = temporada;
  { const CatEp *ep = cat_episodio(idx, epAbsoluto(col));
    comEpT = ep ? ep->temporada : 0; comEpE = ep ? ep->episodio : 0; }
  return 1;
}

// Filme sem elenco ainda, com o meta em voo. E o unico caso em que uma secao
// vazia ocupa altura (ver recalcularLayout) e recebe esqueleto.
static int elencoCarregando(void) {
  const CatItem *ci = cat_item(idx);
  return !ehSerie() && ci && ci->nElenco == 0 && desc_episodios_carregando(idx);
}

// Mesma ideia para as RECOMENDACOES do filme: a secao existe e esta vazia
// porque o Trakt ainda nao respondeu, nao porque nao ha o que mostrar. Sem
// isto a pagina reservava altura zero e o bloco inteiro nascia do nada quando
// a resposta chegava, empurrando o que estava embaixo sob o controle remoto.
static void desenhaEsqueletoRelacionados(float y, float a);

static int relacionadosCarregando(void) {
  return !ehSerie() && extras_n_relacionados() == 0 && extras_carregando();
}

// O PAR DA LINHA (filme, Glass UI): quem divide a linha com quem.
static int parEsq(int r) { (void)r; return -1; }
static int parDir(int r) { (void)r; return -1; }
static int secaoPresente(int r) {
  return secaoN(r) > 0 || (r == SEC_ELENCO && elencoCarregando()) ||
         (r == SEC_RELACIONADOS && relacionadosCarregando());
}
// O VAO ANTES DE CADA SECAO, medido no quadro "Detalhe" do mockup (as
// posicoes absolutas dos blocos .dt): 87 antes do elenco, 60 antes das notas,
// 44 antes dos comentarios, 83 antes de "Mais como este", 52 antes da linha
// Colecao | Frases e 73 antes da Ficha | Producao.
// Alvo da rolagem de uma secao ALTA (Notas com muitas fontes, Numeros da
// temporada): o topo sobe o bastante para o bloco inteiro caber na tela,
// nunca acima de 10% nem abaixo dos 33% de sempre.
static float alvoQueCabe(float h) {
  float a = (NV_TELA_H - h - 40.0f) / NV_TELA_H;
  if (a > NV_DETP_ALVO_FILEIRA) a = NV_DETP_ALVO_FILEIRA;
  if (a < 0.10f) a = 0.10f;
  return a;
}
static float vaoAntes(int r) {
  switch (r) {
    case SEC_ELENCO:       return 87.0f;
    case SEC_NOTAS:        return 60.0f;
    case SEC_COMENTARIOS:  return 44.0f;
    case SEC_RELACIONADOS: return 83.0f;
    case SEC_COLECAO: case SEC_FRASES:    return 52.0f;
    case SEC_DETALHES: case SEC_ESTUDIOS: return 73.0f;
    default:               return NV_DETF_SEC_GAP;
  }
}

static void recalcularLayout(void) {
  int r;
  for (r = 0; r < N_SECOES; r++) { dxSec[r] = 0; wSec[r] = NV_TELA_W - NV_DETP_X * 2; }
  if (ehSerie()) {
    // As quatro primeiras vem de MEDIDA ABSOLUTA na referencia; nao sao um
    // empilhamento. As de baixo (comentarios) sim: elas ficam depois do elenco,
    // cuja altura e conhecida.
    static const float G[N_SECOES] = {
      NV_DETP_G_TEMP, NV_DETP_G_EP, NV_DETP_G_ABAS, NV_DETP_G_ELENCO, 0, 0
    };
    float y;
    for (r = 0; r < N_SECOES; r++) {
      topoSec[r] = conteudoSec[r] = G[r] +
        (r >= SEC_ABAS_INFO && r <= SEC_ELENCO ? epExtraAltura() : 0);
      // NOTAS na serie: logo abaixo dos episodios, onde antes comecavam as abas
      // (G_ABAS); o bloco inteiro empurra abas e elenco (epExtraAltura).
      if (r == SEC_NOTAS) {
        topoSec[r] = conteudoSec[r] = notasTopoSerie();
        if (secaoN(r) > 0) conteudoSec[r] += NV_DETF_CAB_H + NV_DETF_CAB_GAP;
      }
      // NUMEROS DA TEMPORADA logo depois das Notas; o titulo e do proprio bloco.
      if (r == SEC_NUMEROS) topoSec[r] = conteudoSec[r] = numerosTopoSerie();
      if (r == SEC_PROGTEMP) topoSec[r] = conteudoSec[r] = progTopoSerie();
      alvoSec[r] = (r == SEC_ABAS_INFO) ? NV_DETP_ALVO_ABAS
                                        : NV_DETP_ALVO_FILEIRA;
    }
    alvoSec[SEC_NOTAS] = alvoQueCabe(alturaSecao(SEC_NOTAS));
    alvoSec[SEC_NUMEROS] = alvoQueCabe(alturaSecao(SEC_NUMEROS));
    alvoSec[SEC_PROGTEMP] = alvoQueCabe(alturaSecao(SEC_PROGTEMP));
    docFim = NV_DETP_FIM;
    // SECAO DO TRAKT NA SERIE: empilhada abaixo do elenco, como na referencia.
    // Era o "falta a secao do trakt na de series" — ela existia so em filme.
    //
    // A base do ELENCO sai das medidas da SERIE, nao de NV_DETF_EL_ALT (193),
    // que e a altura da fileira de elenco do FILME — foi o que eu usei antes e
    // por isso a secao do Trakt caiu POR CIMA dos avatares.
    //
    // O empilhamento e: topo da fileira (EL_Y) + avatar + o vao ate o nome + o
    // vao do nome ate o papel + a linha do papel. Mais UMA linha de folga
    // porque nome comprido quebra em duas ("Geneva Robertson-Dworet" na propria
    // captura do dono) e empurra o papel para baixo.
    y = baseDaAbaAtiva() + NV_DETP_EL_GAP_TRAKT;
    // Recommendations keep their own row and focus, directly below cast.
    topoSec[SEC_RELACIONADOS] = conteudoSec[SEC_RELACIONADOS] = y;
    if (secaoN(SEC_RELACIONADOS) > 0) {
      conteudoSec[SEC_RELACIONADOS] += NV_DETF_CAB_H + NV_DETF_CAB_GAP;
      y = conteudoSec[SEC_RELACIONADOS] + alturaSecao(SEC_RELACIONADOS) + NV_DETF_SEC_GAP;
      if (y + NV_DETF_PAD_FIM > docFim) docFim = y + NV_DETF_PAD_FIM;
    }

    // AS TRES BANDAS DE AUDIENCIA vem antes dos comentarios, empilhadas pela
    // mesma regra: nascem onde a aba ativa termina e empurram o que vem depois.
    // Altura vinda do ultimo desenho (ver audAlt), porque cada grafico so diz
    // quanto ocupou depois de desenhado.
    for (r = SEC_AUD_RADAR; r <= SEC_AUD_DIGITAL; r++) {
      topoSec[r] = conteudoSec[r] = y;
      if (secaoN(r) <= 0) continue;
      y += alturaSecao(r) + NV_DETF_SEC_GAP;
      { float fim = y + NV_DETF_PAD_FIM - NV_DETF_SEC_GAP;
        if (fim > docFim) docFim = fim; }
    }
    // NOTAS: heatmap por fonte e grade de episodios, com cabecalho proprio como
    // os trailers logo abaixo (topo do grupo = linha do titulo).
    for (r = SEC_NOTAS_EP; r <= SEC_NOTAS_EP; r++) {
      topoSec[r] = conteudoSec[r] = y;
      if (secaoN(r) <= 0) continue;
      conteudoSec[r] = y + NV_DETF_CAB_H + NV_DETF_CAB_GAP;
      y = conteudoSec[r] + alturaSecao(r) + NV_DETF_SEC_GAP;
      { float fim = y + NV_DETF_PAD_FIM - NV_DETF_SEC_GAP;
        if (fim > docFim) docFim = fim; }
    }
    // TRAILERS NA SERIE (#123), na posicao que o enum ja dava a eles (depois
    // da audiencia, antes dos comentarios — a ordem do D-pad). Com cabecalho,
    // como no filme: o topo do grupo e a linha do titulo, o conteudo abaixo.
    topoSec[SEC_TRAILERS] = conteudoSec[SEC_TRAILERS] = y;
    if (secaoN(SEC_TRAILERS) > 0) {
      conteudoSec[SEC_TRAILERS] = y + NV_DETF_CAB_H + NV_DETF_CAB_GAP;
      y = conteudoSec[SEC_TRAILERS] + alturaSecao(SEC_TRAILERS) + NV_DETF_SEC_GAP;
      { float fim = y + NV_DETF_PAD_FIM - NV_DETF_SEC_GAP;
        if (fim > docFim) docFim = fim; }
    }
    topoSec[SEC_COMENTARIOS] = conteudoSec[SEC_COMENTARIOS] = y;
    if (secaoN(SEC_COMENTARIOS) > 0) {
      y += alturaSecao(SEC_COMENTARIOS) + NV_DETF_SEC_GAP;
      float fim = y + NV_DETF_PAD_FIM - NV_DETF_SEC_GAP;
      if (fim > docFim) docFim = fim;
    }
    // Estudios/redes empilham DEPOIS dos comentarios, como no web
    // (renderCompanySections monta a secao ao fim do corpo da pagina).
    topoSec[SEC_ESTUDIOS] = conteudoSec[SEC_ESTUDIOS] = y;
    // Com cabecalho proprio, como os trailers acima e o filme: o topo do grupo
    // e a linha do titulo e o conteudo fica NV_DETF_CAB_H + NV_DETF_CAB_GAP abaixo.
    if (secaoN(SEC_ESTUDIOS) > 0)
      conteudoSec[SEC_ESTUDIOS] = y + NV_DETF_CAB_H + NV_DETF_CAB_GAP;
    // ESTUDIOS DEIXOU DE SER A ULTIMA e por isso passou a ADIANTAR `y`. Antes
    // ela so media o proprio fim para o docFim; com a secao de frases embaixo,
    // nao adiantar aqui punha as duas no mesmo topo — o mesmo defeito que a
    // secao do Trakt ja teve contra os avatares do elenco.
    if (secaoN(SEC_ESTUDIOS) > 0) {
      y = conteudoSec[SEC_ESTUDIOS] + alturaSecao(SEC_ESTUDIOS) + NV_DETF_SEC_GAP;
      float fim = y + NV_DETF_PAD_FIM - NV_DETF_SEC_GAP;
      if (fim > docFim) docFim = fim;
    }
    topoSec[SEC_FRASES] = conteudoSec[SEC_FRASES] = y;
    if (secaoN(SEC_FRASES) > 0) {
      float fim = y + alturaSecao(SEC_FRASES) + NV_DETF_PAD_FIM;
      if (fim > docFim) docFim = fim;
    }
    return;
  }
  { float y = NV_DETF_HERO_FIM;
    for (int o = 0; o < N_ORDEM; o++) {
      float h;
      r = ORDEM_FILME[o];
      topoSec[r] = conteudoSec[r] = y;
      alvoSec[r] = NV_DETP_ALVO_FILEIRA;
      // Secao ausente nao ocupa altura — salvo o ELENCO enquanto o meta do
      // filme carrega: a fileira reserva o lugar e recebe o esqueleto, para a
      // pagina nao pular quando os atores chegarem.
      if (secaoN(r) <= 0 && !(r == SEC_ELENCO && elencoCarregando()) &&
          !(r == SEC_RELACIONADOS && relacionadosCarregando())) continue;
      if (cabecalhoDe(r)) {
        conteudoSec[r] = y + NV_DETF_CAB_H + NV_DETF_CAB_GAP;
        y = conteudoSec[r];
      }
      h = alturaSecao(r);
      y += h + NV_DETF_SEC_GAP;
    }
    // Fim REAL do documento, nao os 2473 da serie: um filme e bem mais curto e
    // copiar aquele numero deixaria a pagina rolar para muito depois do fim.
    docFim = y - NV_DETF_SEC_GAP + NV_DETF_PAD_FIM;
    if (docFim < NV_TELA_H) docFim = NV_TELA_H;
    alvoSec[SEC_NOTAS] = alvoQueCabe(alturaSecao(SEC_NOTAS)); }
}

// As abas sao DINAMICAS, como no web: renderSeriesInsightSection
// (metaDetailsScreen.js:3751) so acrescenta "Mais como este", "Trailer" e
// "Colecao" quando a lista correspondente tem itens, e esconde a barra inteira
// quando sobra uma aba so. O port cravava as quatro e as tres ultimas caiam
// todas em "Sem informacao para esta aba." — que e exatamente o que o web
// evita nao mostrando a aba.
//
// Aqui existem duas: elenco (sempre) e avaliacoes (quando ha nota). Similares
// e trailer nao tem fonte neste port; quando tiverem, entram nesta tabela.
typedef enum { ABA_ELENCO, ABA_AVALIACOES, ABA_RELACIONADOS, ABA_COLECAO,
               ABA_COMENTARIOS, ABA_NFIXAS } AbaInfoId;
// OS ROTULOS SAO OS DO APARELHO, e nao os do web. Lido na barra da serie
// "Furious" na TCL: "Direção e Elenco | Avaliações | Recomendações | Trailer".
// "Criador e elenco" e "Mais como este" vinham do NuvioWeb e nao existem la.
//
// "Coleção" e "Comentários" ficam: sao dados que este port TEM e que a barra da
// referencia nao mostrava naquele titulo (ela some as abas sem conteudo, e a
// serie medida nao tinha nem colecao nem comentarios). Tirar as duas seria
// esconder o que o app ja sabe mostrar.
static const char *ABA_ROTULO[ABA_NFIXAS] = {
  "Direção e Elenco", "Avaliações", "Recomendações", "Coleção", "Comentários"
};

// Nota do IMDb do titulo aberto, 0 quando nao ha.
static int notaDe(int i) {
  const CatItem *ci = cat_item(i);
  return ci ? imdbnota_obter(ci->imdb, ci->nota, !strcmp(ci->tipo,"series")) : 0;
}
// O que a secao "Notas" e a linha do titulo sabem do titulo aberto: as notas de
// cada fonte (as que a pessoa escondeu em Ajustes ja chegam zeradas, e o IMDb
// cai na reserva do catalogo quando o MDBList nao respondeu) e, em serie, as
// funcoes que entregam as notas por episodio. Barata: e chamada varias vezes
// por quadro (empilhamento, foco, desenho).
static const NotasSecao *notasDados(void) {
  static NotasSecao s;
  int i;
  for (i = 0; i < EX_NFONTES; i++) s.cru[i] = extras_nota(i);
  if (!s.cru[EX_IMDB] && ajustes_mdblist_fonte(EX_IMDB)) s.cru[EX_IMDB] = notaDe(idx);
  s.nTemp = ehSerie() ? extras_n_temporadas() : 0;
  s.tempNum = extras_temporada_numero;
  s.nEps = extras_n_eps;
  s.epNum = extras_ep_numero;
  s.epNota = extras_ep_nota;
  return &s;
}

// Quantos itens a aba de Avaliacoes tem para focar: as temporadas, em serie; os
// cartoes de nota, em filme. Serve so a navegacao — o desenho ja sabe o que
// mostrar em cada caso.
static int nAvaliaveis(void) {
  int i, n = 0;
  if (ehSerie() && extras_n_temporadas() > 0) return extras_n_temporadas();
  for (i = 0; i < EX_NFONTES; i++) {
    int v = extras_nota(i);
    // O fallback do catalogo tambem some quando a fonte esta desligada —
    // "esconder IMDb" quer dizer esconder o cartao, nao so a nota do mdbList.
    if (i == EX_IMDB && !v && ajustes_mdblist_fonte(EX_IMDB)) v = notaDe(idx);
    if (v) n++;
  }
  return n;
}

static int abaDisponivel(int id) {
  switch (id) {
    case ABA_ELENCO:       return 1;
    // Basta UMA das notas para a aba valer a pena; o cartao que faltar mostra
    // "-", que e o que o web faz.
    // Saiu em 30/09 a pedido do dono ("tirar a aba avaliacoes que ja temos o
    // componente novo"): as notas por temporada vivem na secao NOTAS, empilhada
    // logo abaixo da audiencia, e a aba repetia o mesmo numero num segundo lugar.
    case ABA_AVALIACOES:   return 0;
    case ABA_RELACIONADOS: return 0; // Dedicated row below cast, for films and series.
    case ABA_COLECAO:      return extras_n_colecao() > 1;
    // Sem aba de comentarios: na referencia eles sao uma SECAO empilhada, e as
    // abas medidas na TCL sao so Direção e Elenco / Avaliações / Recomendações.
    // Deixar as duas coisas mostraria o mesmo conteudo em dois lugares.
    case ABA_COMENTARIOS:  return 0;
    default:               return 0;
  }
}
// Traduz a posicao visivel `c` para o id da aba.
static int abaIdDe(int c) {
  for (int id = 0, v = 0; id < ABA_NFIXAS; id++)
    if (abaDisponivel(id) && v++ == c) return id;
  return ABA_ELENCO;
}
static int nAbasInfo(void) {
  int n = 0;
  for (int id = 0; id < ABA_NFIXAS; id++) if (abaDisponivel(id)) n++;
  return n;
}


static int  secaoN(int r);
static int  secaoColunas(int r);
static int  temporadaEm(int c);
static float larguraTemporada(int c);
static float larguraAbaInfo(int i);

// NULL quando nao se sabe — nao uma lista de reserva. As listas fixas que
// ficavam aqui existiam para o app rodar so com uma pasta de imagens solta, mas
// o preco era um titulo REAL sem nome carregado aparecer chamado "Ruptura" ou
// "Silo", indistinguivel de dado verdadeiro. Quem desenha omite o que vier NULL.
static const char *tituloDe(int i) {
  const CatItem *c = cat_item(i);
  return (c && c->titulo[0]) ? c->titulo : NULL;
}
static const char *generoDe(int i) {
  const CatItem *c = cat_item(i);
  return (c && c->genero[0]) ? c->genero : NULL;
}
// Vazio quando nao se sabe. A lista de reserva que ficava aqui carimbava
// "2025 · 1 h 54 min" num titulo cujo metadado ainda nao chegou — ano e duracao
// INVENTADOS, na linha de meta do hero, ao lado de dados verdadeiros.
// partirMeta ja lida com string vazia e devolve os dois campos vazios; quem
// desenha omite cada um deles.
static const char *fichaDe(int i) {
  const CatItem *c = cat_item(i);
  return (c && c->meta[0]) ? c->meta : "";
}
static const char *sinopseDe(int i) {
  const CatItem *c = cat_item(i);
  return (c && c->sinopse[0]) ? c->sinopse : NULL;
}
// A ARTE COM QUE O DETALHE ABRIU E A ARTE ATE ELE FECHAR.
//
// Pedido do dono (19/09): "a arte ta boa quando abre o filme, nao tem
// necessidade nenhuma de trocar; alem de ficar feio tudo atualizando o tempo
// todo". O que trocava: a descoberta enriquece o titulo ABERTO num fio (id do
// TMDB, titulo localizado, logo no idioma configurado, fundo w1280) e escreve
// no CatItem — e o detalhe, lendo cat_item() por quadro, redesenhava com o
// logo novo alguns segundos depois de abrir, e com fundo novo quando o
// catalogo trazia so o cartaz. Do sofa isso e a arte "recarregando".
//
// Congelar: no abrir, guarda-se a url do fundo e do logo; enquanto aberto,
// sao essas. O enriquecimento continua acontecendo e fica no catalogo — a
// proxima abertura, e a home, ja veem o logo localizado. Sem logo nenhum ao
// abrir, o que chegar entra (vazio nao e "arte boa").
// arteFixa no teto da textura (#361): quando o titulo nao tem fundo ela e o
// PROPRIO poster, que vai a 1024 — com 512 o cartaz de ~600 caracteres virava
// um 404 so na pagina do titulo. Logo segue o CatItem.logo (512).
static char arteFixa[NV_TEX_URL_MAX], logoFixo[512], logoCatalogoFixo[512];
static int  arteFixaPoster;

static const char *logoDe(int i) {
  const CatItem *c = cat_item(i);
  if (i == idx && logoFixo[0]) return logoFixo;
  if (i == idx) {
    const char *u = artehero_logo_sessao(c);
    if (u) {
      snprintf(logoFixo, sizeof logoFixo, "%s", u);
      if (c && c->logo[0])
        snprintf(logoCatalogoFixo, sizeof logoCatalogoFixo, "%s", c->logo);
    }
    return u;
  }
  // O CATALOGO GUARDA O LOGO EM `original`, que no TMDB e 4127 px de largura
  // para um desenho de no maximo 1000 (NV_DETW_LOGO_MAXW). Igual ao fundo:
  // quem sabe o tamanho do desenho e quem desenha, entao a politica fica em
  // artehero.c e a url do catalogo nao muda.
  if (c && c->logo[0]) return artehero_url_logo(c->logo);
  return NULL;
}
static const char *arteDeViva(int i);
static const char *arteDe(int i) {
  // "TROCAR ARTE" ABERTA (#142): a pagina mostra a previa ja carregada da
  // miniatura em foco — e o que faz a Cor viva e o veu seguirem a escolha
  // antes do OK, sem caminho proprio.
  if (i == idx) { const char *pv = trocaarte_previa_fundo(); if (pv) return pv; }
  // Congelada, MAS NAO PRESA A UM 404: a url da fonte escolhida pode ser
  // virtual (TMDB/Trakt pelo id, artereserva.h) e so se sabe se ela existe
  // depois do download. Falhou, solta e escolhe de novo (cai na seguinte).
  // CONGELADA NO CARTAZ, E O FUNDO CHEGOU: solta. Titulo aberto pelas
  // Recomendacoes nasce de uma SEMENTE (so titulo e cartaz) e a ficha traz o
  // fundo 1-2 s depois; a pagina ficava com o cartaz esticado ate fechar
  // (dono, 05/10/2026). So vale nesse sentido: fundo por fundo nao troca.
  if (i == idx && arteFixa[0] && arteFixaPoster) {
    const CatItem *c = cat_item(i);
    if (c && c->backdrop[0]) { arteFixa[0] = 0; arteFixaPoster = 0; }
  }
  if (i == idx && arteFixa[0] && !tex_falhou(arteFixa)) return arteFixa;
  { const char *u = arteDeViva(i);
    if (i == idx && u) {
      const CatItem *c = cat_item(i);
      snprintf(arteFixa, sizeof arteFixa, "%s", u);
      // Marcado AQUI, junto com o congelamento: arteDetalheEhPoster so marcava
      // se fosse chamada antes desta, e depois dela devolvia o 0 de fabrica.
      arteFixaPoster = c && !c->backdrop[0] && c->poster[0];
    }
    return u; }
}
static const char *arteDeViva(int i) {
  const CatItem *c = cat_item(i);
  // Um detalhe nunca pode herdar a arte de outra posicao do catalogo. Quando
  // o backdrop do proprio titulo falta, o renderer mostra o estado neutro e
  // preserva o layout, aguardando eventual enriquecimento do mesmo item.
  // TELA CHEIA PEDE A ARTE GRANDE. O backdrop guardado no catalogo foi
  // dimensionado para o CARD (o caminho do TMDB entra como w1280); desenhar
  // isso a 1920 amplia 1,5x. artehero_url sobe para a versao grande da mesma
  // arte e, quando nao ha fundo nenhum e o titulo tem id do IMDb, monta a url
  // do metahub — que e fundo de verdade em vez do cartaz esticado.
  // A FONTE E A REGRA DO DESTAQUE (artehero_url_destaque): com "Destaque com
  // outra arte" desligado e a foto do card; ligado, a mesma que o destaque da
  // home mostrava — abrir o titulo nao troca a foto em nenhum dos dois.
  if (c && (c->backdrop[0] || c->imdb[0])) {
    const char *grande = artehero_url_destaque(c, ajustes_hero_fonte(),
                                               ajustes_hero_arte_diferente());
    if (grande) return grande;
  }
  // Poster do próprio título é a reserva segura. O desenho trata-o como arte
  // contida, não como cover 16:9, para preservar rosto, lettering e proporção.
  if (c && c->poster[0]) return c->poster;
  return NULL;
}

static int arteDetalheEhPoster(int i) {
  const CatItem *c = cat_item(i);
  // Congelado junto com a arte: a resposta descreve a url guardada, nao a
  // que o enriquecimento pode ter posto no catalogo depois.
  if (i == idx && trocaarte_previa_fundo()) return 0;   // previa: sempre fundo
  if (i == idx && arteFixa[0]) return arteFixaPoster;
  { int r = c && !c->backdrop[0] && c->poster[0];
    if (i == idx) arteFixaPoster = r;
    return r; }
}

static void desenhaArteDetalhe(GfxRect alvo, GLuint tex, const char *arte,
                               int poster, float alpha, float pg) {
  if (!tex) {
    gfx_cor(alvo, 0.0f, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, alpha);
    return;
  }
  gfx_tex_aspect_atual = tex_aspecto(arte);
  if (!poster) {
    // uFoco = forca da vinheta: cai com a rolagem (pg) e pelo ajuste
    // "Escurecimento do fundo" (0 = arte limpa).
    float veu = (1.0f - pg) * ajustes_detalhe_veu();
    // TRAILER TOCANDO ATRAS DO CANVAS: abre o furo, a arte se apaga por cima
    // dele (trailerFade) e a vinheta fica como veu com alpha, para o texto
    // continuar apoiado no mesmo escuro. Poster reserva nao entra aqui: o
    // furo e a area toda, e o cartaz contido nao a cobre.
    if (trailerFade > 0.005f && trailer_aberto() && !trailer_cheia()) {
      gfx_furo(alvo);
      if (trailerFade < 0.995f)
        gfx_rect(alvo, tex, GFX_DETALHE, veu, 0, 0, 0.0f, 0, 0, 0, alpha * (1.0f - trailerFade));
      // No modo cinema o bloco de texto saiu: o veu sai junto (fica 15%,
      // para o logo pequeno do canto nao flutuar sobre uma cena clara).
      gfx_rect(alvo, 0, GFX_DETALHE, veu * (1.0f - 0.85f * trailercinema_t(&trailerCinema)), 1.0f, 0, 0.0f, 0, 0, 0, alpha * trailerFade);
    } else
      gfx_rect(alvo, tex, GFX_DETALHE, veu, 0, 0, 0.0f, 0, 0, 0, alpha);
  } else {
    float ap = gfx_tex_aspect_atual > 0.05f ? gfx_tex_aspect_atual : (2.0f / 3.0f);
    float h = alvo.h * 0.90f, w = h * ap, maxW = alvo.w * 0.42f;
    if (w > maxW) { w = maxW; h = w / ap; }
    GfxRect r = { alvo.x + alvo.w - w - 72.0f,
                  alvo.y + (alvo.h - h) * 0.5f, w, h };
    gfx_rect(r, tex, GFX_HERO, 0, 0, 0, 0, 0, 0, 0, alpha);
  }
  gfx_tex_aspect_atual = 0.0f;
}
static int ehSerie(void) {
  const CatItem *ci = cat_item(idx);
  if (!ci) return 0;
  if (ci->tipo[0]) return strcmp(ci->tipo, "series") == 0;
  return cat_n_episodios(idx) > 0;
}

static float suave(float x) {
  x = anim_clamp(x, 0.0f, 1.0f);
  return 1.0f - (1.0f - x) * (1.0f - x) * (1.0f - x);
}
static float fase2(void) { return suave((t - 0.45f) / 0.55f); }

// A Inter embarcada so tem Regular, Medium e Bold, e a pagina pede 500, 600 e
// 800 em corpos (32, 26, 21) que so existem em Regular na tabela de text.c —
// que e arquivo de outro agente nesta sessao. Engrossar redesenhando a mesma
// linha com deslocamentos sub-pixel e o que sobra, e e o que os rasterizadores
// chamam de "faux bold": custa uma textura so, porque a linha vem do cache.
static void txt_peso(TxtLinha l, float x, float y, float a, float grossura) {
  txt_desenhar_alpha(l, x, y, a);
  if (grossura > 0.05f) txt_desenhar_alpha(l, x + grossura * 0.5f, y, a);
  if (grossura > 0.9f)  txt_desenhar_alpha(l, x + grossura, y, a);
}

// AUDITORIA (tools/auditoria-release.sh): uma linha quando o numero de
// episodios da pagina aberta muda. E o que o smoke do Mac le para dizer se a
// serie abriu com a lista; a captura de tela nao diz isso sozinha.
static int audIdx = -1, audN = -2;
static void auditoriaEpisodios(void) {
  const CatItem *ci;
  int n;
  if (!aberto || saindo || !ehSerie() || !(ci = cat_item(idx))) return;
  n = cat_n_episodios(idx);
  if (idx == audIdx && n == audN) return;
  audIdx = idx; audN = n;
  printf("[detail] episodios na pagina: %s %d\n", ci->imdb, n);
  fflush(stdout);
}

static void abrirInterno(const HomeItem *it) {
  pediuMenu = 0;
  audIdx = -1;
  marco("detail_abrir");
  // O TITULO ANTERIOR PODE TER DEIXADO FIO NO AR. Trocar de titulo por dentro
  // (um credito de ator, um "Mais como este") reabre esta tela sem passar pelo
  // fechamento, e sem isto os pedidos da serie antiga continuavam saindo para
  // uma pagina que ninguem esta mais vendo. Os dois modulos param na proxima
  // fronteira de episodio e descartam o resto; serieaud_abrir/seriefrases_abrir
  // limpam a marca sozinhos.
  serieaud_fechar();
  seriefrases_fechar();
  trocaarte_fechar();
  audAberta = 0; audTempAberta = -1; audTempVista = -1; frasesAberta = 0;
  item = *it;
  aberto = 1; saindo = 0; nivel = 0; botao = 0; focoAmigos = 0;
  amtui_fechar();
  detmais_zerar();
  heroReiniciar();
  t = 0.0f; pg = 0.0f; scrollY = 0.0f; velY = 0.0f; abaInfo = 0; pessoaAberta = 0;
  relFoco = 0; colListaAberta = 0; colListaFoco = 0;
  colListaScroll = colListaVel = 0.0f; pedAbrir = -1; ratTemp = 0; ratSinc = 0;
  trailerDesde = 0; trailerTentado = 0; trailerFade = 0.0f;
  trailerEtapa = 0; trailerPrazo = 0;
  trailerSemFonteLogado = 0;
  trailercinema_zerar(&trailerCinema);
  idx = it->indice;
  // O TRAILER DO CARTAZ CONTINUA AQUI, COM SOM (pedido do rawldon, canario
  // tpk-janela; so o .tpk, NV_TRAILER_CONTINUA_DETALHE em trailerfonte.h).
  // Se o destaque da home esta tocando o trailer DESTE titulo, a pagina o
  // adota: mesmo player, sem reabrir, tela inteira e som. Respeita "Trailer
  // automatico" (desligado = fecha, como antes). Qualquer outro caso fecha,
  // como sempre. O botao Trailer (tela cheia) nao muda.
  { int adotou = 0;
#if NV_TRAILER_CONTINUA_DETALHE
    const CatItem *ciA = cat_item(idx);
    if (trailer_aberto() && !trailer_cheia() && trailer_tocando() &&
        trailer_dono() == TRAILER_DONO_HOME && ciA && ciA->imdb[0] &&
        !strcmp(trailer_dono_imdb(), ciA->imdb)) {
      if (!ajustes_trailer_auto()) {
        printf("[trailer] detalhe: trailer do cartaz nao continua (Trailer automatico desligado)\n");
        fflush(stdout);
      } else {
        GfxRect telaA = { 0, 0, NV_TELA_W, NV_TELA_H };
        adotou = trailer_continuar(telaA, 1);
        if (adotou) {
          trailer_marcar_dono(TRAILER_DONO_DETALHE, ciA->imdb);
          // Ja tocando: sem esperar a arte apagar, e sem o modo cinema de
          // cara (a pessoa abriu para LER a pagina; o trailer segue atras).
          trailerTentado = 1; trailerFade = 1.0f; trailerCinema.tocavaAntes = 1;
          printf("[trailer] detalhe: continua o trailer do cartaz com som (%s)\n", ciA->imdb);
          fflush(stdout);
        }
      }
    }
#endif
    if (!adotou) trailer_fechar(); }
  revistaVista = cat_revisao();
  // A PAGINA PEDE A PROPRIA LISTA. Quem pedia era so app.c, e so quando o alvo
  // de fontes MUDA: reabrir a serie que esta tocando (pela ilha, pelo mini
  // player) tem o mesmo alvo, e a pagina abria com a faixa vazia e sem pedido
  // nenhum — sem os cartoes de episodio (TCL do dono, 2.0.3).
  epGerConferido = 0;
  // Guarda identidade e copia ANTES de qualquer republicacao. Ver revalidarIdx.
  arteFixa[0] = logoFixo[0] = logoCatalogoFixo[0] = 0;
  arteFixaPoster = 0;   // arte nova por abertura
  { const CatItem *ci0 = cat_item(idx);
    idxImdb[0] = 0; idxTemCopia = 0;
    if (ci0) {
      snprintf(idxImdb, sizeof idxImdb, "%s", ci0->imdb);
      idxCopia = *ci0; idxTemCopia = 1;
    } }
  { const CatItem *ci0 = cat_item(idx);
    artehero_logo_sessao_iniciar(ci0); }
  // Nota do Trakt, comentarios e relacionados. Pedido na ABERTURA e nao no
  // desenho: as abas so aparecem depois que o dado chega, e pedir no desenho
  // faria a barra de abas surgir com o titulo ja na tela.
  //
  // SEM `if (imdb[0])`: titulo sem id tambem passa por extras_pedir, que e
  // quem zera o que o titulo anterior publicou (issue #60, ver extras.c).
  { const CatItem *ci = cat_item(idx);
    extrasSerie = ehSerie();
    extras_pedir(ci ? ci->imdb : "", extrasSerie, ci ? ci->tmdb : 0);
    // Pedir agora e o que deixa o trailer pronto quando a pagina assentar.
    // Apple nos dois alvos; IMDb na LG e, na Samsung, so pelo servico de
    // recomendacoes (a API exige Referer, que o navegador nao deixa por, e o
    // CORS dela so aceita imdb.com — #136).
    if (trailer_suportado() && ci && ci->imdb[0]) {
      trailerapple_pedir(ci->imdb, ci->titulo, ci->meta, ehSerie());
      if (!trailerfonte_tizen() || trailerfonte_imdb_tizen()) trailerimdb_pedir(ci->imdb);
    }
  }
  // A aba marcada tem de ser a da temporada de "Continuar assistindo", nao a
  // do primeiro episodio da serie. Issue #43: abrindo pela fileira com S2E2 em
  // andamento a aba acendia sempre "Temporada 1" (o primeiro episodio
  // carregado) — e o dono, ao descer para a fileira de episodios, via a coluna
  // ZERAR por irParaTemporada (comentario ali: "trocar de aba REINICIA a
  // fileira"), o que empurrava o foco de volta para a temporada em exibicao —
  // que como a aba tinha sido a errada, era a 1, e ele lia isso como
  // "voltou para T1E1". A raiz e a mesma nos dois: nada aqui perguntava pelo
  // progresso.
  //
  // Mesma prioridade do botao "Retomar" (ver episodioAlvo, servida em
  // episodioPadrao): o progresso local em CatItem, que ja chega com o
  // titulo — sem depender do /shows/<id>/progress/watched, que ainda nao
  // respondeu neste instante.
  temporada = 0;
  epAncora = 0;
  comEpT = comEpE = 0;
  { const CatItem *ci0 = cat_item(idx);
    int t = 0, e = 0, achou = 0;
    if (ci0 && ci0->progresso > 0 && ci0->progresso < ajustes_cw_concluido() &&
        ci0->temporada > 0 && ci0->episodio > 0) {
      t = ci0->temporada; e = ci0->episodio; achou = 1;
    } else {
      const CatEp *e0 = cat_episodio(idx, 0);
      if (e0) { t = e0->temporada; e = e0->episodio; achou = 1; }
    }
    if (achou && ci0) {
      int k, n = cat_n_episodios(idx), i, col = 0, achouCol = -1;
      for (k = 0; k < ci0->nTemporadas; k++)
        if (ci0->temporadas[k] == t) { temporada = k; break; }
      for (i = 0; i < n; i++) {
        const CatEp *ep = cat_episodio(idx, i);
        if (!ep || ep->temporada != t) continue;
        if (ep->episodio == e) { achouCol = col; break; }
        col++;
      }
      if (achouCol >= 0) epAncora = achouCol;
    } }
  int cols[N_SECOES]; for (int i = 0; i < N_SECOES; i++) cols[i] = secaoColunas(i);
  focus_iniciar(&foco, N_SECOES, cols);
  // SEM ISTO, descer do hero (foco.coluna = 0, linha mais abaixo) e trocar de
  // aba (irParaTemporada) reescrevem os dois valores acima com zero antes que
  // o usuario mexa em qualquer coisa — colunaLembrada e a MEMORIA que
  // focus_mover consulta ao entrar numa fileira (focus.c:26), e ela nasce
  // zerada pelo memset de focus_iniciar. Semea-la aqui e o unico jeito de a
  // pagina lembrar onde o progresso estava.
  foco.colunaLembrada[SEC_TEMPORADAS] = temporada;
  foco.colunaLembrada[SEC_EPISODIOS]  = epAncora;
  memset(animFoco, 0, sizeof animFoco);
  memset(scrollSec, 0, sizeof scrollSec); memset(velSec, 0, sizeof velSec);
  memset(revEp, 0, sizeof revEp); memset(revRel, 0, sizeof revRel);
}

void detail_fechar(void) {
  if (!aberto || saindo) return;
  pessoaAberta = 0;
  nivel = 0;
  saindo = 1;
}

// SECO: o proximo detail_atualizar ja a encerra (o mesmo fim da mola de
// saida, com o logo restaurado), sem a pagina recolher por cima da home.
void detail_fechar_seco(void) {
  if (!aberto) return;
  pessoaAberta = 0;
  nivel = 0;
  saindo = 1;
  t = 0.0f;
}

void detail_mostrar_pessoa(long tmdb, const char *nome, const char *foto) {
  if (!aberto || tmdb <= 0) return;
  pessoa_pedir(tmdb, nome ? nome : "", foto ? foto : "");
  pessoaAberta = 1;
  pessoaFoco = 0;
  pessoaLinha = 0;
}

void detail_abrir(const HomeItem *it) {
  int replacing = aberto && !saindo;
  if (!replacing) { nVolta = 0; voltando = 0; }
  maisAcoes = 0;
  int pos = -1, n = 0;
  // CARROSSEL: so quando a pagina NASCE de um cartaz de fileira da Dinamica
  // (o item aberto e o focado na home). Trocar de titulo por dentro (credito,
  // "Mais como este") e os outros caminhos de abertura seguem como sempre.
  carro = 0;
  if (!(aberto && !saindo) && it && ajustes_home_layout() == HOME_LAYOUT_DINAMICA)
    n = home_fileira_titulos(carIdx, CAR_MAX, &pos);
  if (n > 0 && pos >= 0 && carIdx[pos] == it->indice) {
    carro = 1; carN = n; carPos = carAplicado = pos; carBotaoFim = 0; carFocou = 0; carEsperaRect = 0;
    carOff = (float)pos; carVel = 0.0f; cartao = 1.0f; cartaoVel = 0.0f;
    memset(carPre, 0, sizeof carPre);
    carCheia = 0; carTxt = 1.0f; carTxtVel = 0.0f;
    carOrigem = it->rect;
    if (carOrigem.w < 8.0f || carOrigem.h < 8.0f)
      carOrigem = (GfxRect){ NV_TELA_W * 0.5f - 124.0f, NV_TELA_H * 0.5f - 186.0f, 248.0f, 372.0f };
    printf("[carrossel] abre %d/%d da fileira\n", pos + 1, n); fflush(stdout);
  }
  abrirInterno(it);
  // Quem abre o detalhe por fora do roteador de app.c (ilha, player, pos-play)
  // tambem fica sem a lista de episodios; pedir aqui e idempotente.
  if (it) desc_episodios_garantir(it->indice);
  if (replacing) t = pg = 1.0f; // Switch titles directly without exposing Home.
}

// Monta a pagina do titulo que a tira deixou em cena. Adiada ate a tira
// quase assentar (ver detail_atualizar): segurar a seta atravessa a fileira
// sem pedir extras, trailer e logo de cada titulo do caminho.
static int nBotoes(void);
static void carAplicar(void) {
  const CatItem *ci;
  HomeItem it;
  float t0 = t;
  if (!carro || carAplicado == carPos) return;
  ci = cat_item(carIdx[carPos]);
  if (!ci) { carPos = carAplicado; return; }
  memset(&it, 0, sizeof it);
  it.indice = carIdx[carPos];
  it.rect = carOrigem;
  it.arte = ci->poster[0] ? ci->poster : ci->backdrop;
  it.titulo = ci->titulo; it.genero = ci->genero; it.meta = ci->meta;
  abrirInterno(&it);
  t = t0;                       // a pagina ja esta aberta: nao reabre
  carAplicado = carPos;
  botao = carBotaoFim ? nBotoes() - 1 : 0;
  if (botao < 0) botao = 0;
  printf("[carrossel] titulo %d/%d: %s\n", carPos + 1, carN, ci->titulo); fflush(stdout);
}
// PRE-BUSCA DOS VIZINHOS (+1, depois -1): meta e lista de episodios prontas
// antes de o foco chegar (C9, 2.0.3: "[meta]" e "[desc]" saiam na chegada).
// So com o titulo em cena assentado e sem pedido proprio em voo; um fio de
// cada vez e uma tentativa por titulo e por abertura (rede fora nao vira laco).
static void carPreBuscar(void) {
  static const int passo[2] = { 1, -1 };
  static int ligada = -1;
  int j;
  // So para medir (A/B na bancada): NUVIO_CARROSSEL_PREBUSCA=0 desliga. Nao e ajuste.
  if (ligada < 0) { const char *e = getenv("NUVIO_CARROSSEL_PREBUSCA"); ligada = !(e && e[0] == '0'); }
  if (!ligada) return;
  if (desc_episodios_carregando(carIdx[carPos])) return;
  for (j = 0; j < 2; j++) {
    int k = carPos + passo[j], r;
    if (k < 0 || k >= carN || carPre[k]) continue;
    r = desc_episodios_precarregar(carIdx[k]);
    if (r < 0) return;
    carPre[k] = 1;
    if (r > 0) {
      const CatItem *ci = cat_item(carIdx[k]);
      printf("[carrossel] pre-busca %d/%d: %s\n", k + 1, carN, ci ? ci->titulo : "");
      fflush(stdout);
      return;
    }
  }
}
static void carPasso(int d) {
  if (carCheia || nivel > 0) return;
  int novo = carPos + d;
  if (novo < 0 || novo >= carN) return;
  carPos = novo;
  carBotaoFim = d > 0;
  // O TRAILER DO CARTAO PARA NA HORA: nada de plano preso atras da tira de
  // filme. O titulo novo espera o seu tempo de novo (abrirInterno zera a
  // tentativa quando a tira chega).
  if (trailer_aberto() && !trailer_cheia()) trailer_fechar();
  trailerDesde = 0; trailerTentado = 0; trailerFade = 0.0f;
}

// A janela do cartao em cena NESTE quadro: cartao <-> tela cheia (cartao) e
// cartaz da fileira <-> cartao (abrir/fechar, t).
static GfxRect carBuraco(float *raioPx) {
  GfxRect c = { CAR_X, CAR_Y, NV_TELA_W - 2.0f * CAR_X, NV_TELA_H - CAR_Y - CAR_BAIXO };
  float k = cartao, s = suave(t), r;
  GfxRect h = { c.x * k, c.y * k, NV_TELA_W + (c.w - NV_TELA_W) * k, NV_TELA_H + (c.h - NV_TELA_H) * k };
  r = CAR_RAIO * k;
  if (s < 0.9999f) {
    h.x = carOrigem.x + (h.x - carOrigem.x) * s;
    h.y = carOrigem.y + (h.y - carOrigem.y) * s;
    h.w = carOrigem.w + (h.w - carOrigem.w) * s;
    h.h = carOrigem.h + (h.h - carOrigem.h) * s;
    r = CAR_RAIO_HOME + (r - CAR_RAIO_HOME) * s;
  }
  if (raioPx) *raioPx = r;
  return h;
}
// O carrossel desenha o fundo neste quadro (folha + cartoes)? Na pagina cheia
// assentada volta o desenho de sempre (desenhaArteDetalhe), que e o mesmo
// pixel com a janela do tamanho da tela.
static float carFolha(void) { return anim_clamp(suave(t) * 3.0f, 0.0f, 1.0f); }
static int carDesenhaFundo(void) {
  return carro && (cartao > 0.002f || suave(t) < 0.999f || saindo);
}

int detail_aberto(void) { return aberto; }
int detail_relogio_oculto(void) { return aberto && carro && !carCheia && nivel == 0; }
int detail_pediu_social(void) { int p = pediuSocial; pediuSocial = 0; return p; }

// 0..1 de quanto o detalhe ja tomou a tela. A home le isto para DESCER as
// fileiras enquanto ele entra: e o movimento que o dono descreve como "so os
// posters descem". Fica aqui e nao numa variavel compartilhada porque a mola
// que o produz e a mesma do desenho — dois relogios diferentes descasariam.
// No carrossel a home NAO desce: o cartaz abre por cima dela, que fica parada
// por baixo da folha ate ser coberta.
float detail_progresso(void) { return aberto && !carro ? suave(t) : 0.0f; }

// Temporada e episodio EM FOCO, para quem for pedir fonte.
//
// Sem isto o addons_buscar recebia so o imdb da serie e cravava ":1:1" — e por
// isso as fontes eram sempre as do episodio 1, qualquer que fosse o escolhido.
// Devolve 0 quando o foco nao esta na fileira de episodios; nesse caso quem
// chama cai no primeiro episodio da temporada em exibicao, que e o que a tela
// mostra em cima.
// ONDE O DONO PAROU, ou onde ele deve comecar.
//
// Tres fontes, nesta ordem:
//   1. o episodio EM FOCO, quando ele esta na fileira de episodios — ali a
//      escolha e explicita e ganha de qualquer historico;
//   2. o episodio em ANDAMENTO (progresso do Trakt no proprio CatItem), que e o
//      "Retomar";
//   3. o PRIMEIRO NAO ASSISTIDO, varrendo as temporadas em ordem com o
//      /shows/<id>/progress/watched que extras.c ja le — o "Proximo".
//
// O comentario que existia aqui dizia que o catalogo nativo "nao guarda quais
// episodios ja foram vistos". Nao guarda mesmo, mas extras_ep_visto() sabe
// desde que o painel de notas por episodio foi feito — a afirmacao ficou velha
// e o botao seguiu apontando para T1E1 em serie ja comecada, que foi o que o
// dono relatou.
//
// `origem` (opcional) devolve 1 = foco, 2 = retomar, 3 = proximo, 0 = primeiro.
static int episodioAlvo(int *temp, int *epis, int *origem) {
  const CatItem *ci = cat_item(idx);
  const CatEp *ep = NULL;
  if (origem) *origem = 0;

  if (foco.fileira == SEC_EPISODIOS) ep = cat_episodio(idx, epAbsoluto(foco.coluna));
  if (ep) {
    if (temp) *temp = ep->temporada;
    if (epis) *epis = ep->episodio;
    // O episodio em foco E o que esta em andamento: e o "Retomar" (com o
    // "Assistir do começo" ao lado), como o mockup "detalhe-retomar".
    if (origem) *origem = (ci && ci->progresso > 0 && ci->progresso < ajustes_cw_concluido() &&
                           ci->temporada == ep->temporada && ci->episodio == ep->episodio) ? 2 : 1;
    return 1;
  }
  // Em andamento: o item do "Continuar assistindo" traz temporada e episodio.
  if (ci && ci->progresso > 0 && ci->progresso < ajustes_cw_concluido() && ci->temporada > 0 && ci->episodio > 0 &&
      !extras_ep_visto(ci->temporada, ci->episodio)) {
    if (temp) *temp = ci->temporada;
    if (epis) *epis = ci->episodio;
    if (origem) *origem = 2;
    return 1;
  }
  // Primeiro nao assistido. So vale quando o Trakt ja respondeu; sem dado
  // nenhum extras_n_temporadas() e 0 e cai no primeiro episodio, como antes.
  { int t, e;
    if (extras_proximo_episodio(&t, &e)) {
      if (temp) *temp = t;
      if (epis) *epis = e;
      if (origem) *origem = 3;
      return 1;
    }
  }
  { int t, i, nt = extras_progresso_pronto() ? extras_n_temporadas() : 0;
    for (t = 0; t < nt; t++) {
      int tn = extras_temporada_numero(t), ne = extras_n_eps(t);
      for (i = 0; i < ne; i++) {
        int en = extras_ep_numero(t, i);
        if (en > 0 && !extras_ep_visto(tn, en)) {
          if (temp) *temp = tn;
          if (epis) *epis = en;
          if (origem) *origem = 3;
          return 1;
        }
      }
    } }
  // O PRIMEIRO DA TEMPORADA EM EXIBICAO, que e o que a nota acima promete —
  // com a lista emendada isto era o primeiro episodio da SERIE, qualquer que
  // fosse a aba aberta.
  ep = cat_episodio(idx, epAbsoluto(0));
  if (!ep) return 0;
  if (temp) *temp = ep->temporada;
  if (epis) *epis = ep->episodio;
  return 1;
}

// Episodio que os COMENTARIOS seguem: o ultimo pisado pelo foco; antes de o
// foco passar pela fileira, o mesmo alvo do botao de reproduzir.
static int episodioDosComentarios(int *temp, int *epis) {
  if (foco.fileira != SEC_EPISODIOS && comEpT > 0 && comEpE > 0) {
    *temp = comEpT; *epis = comEpE; return 1;
  }
  return episodioAlvo(temp, epis, NULL);
}

int detail_ep_foco(int *temp, int *epis) {
  if (!aberto) return 0;
  return episodioAlvo(temp, epis, NULL);
}

int detail_assentado(void) {
  return aberto && !saindo && t > 0.985f && nivel == 0;
}

// Retangulo do backdrop NESTE quadro e a opacidade com que ele sai. Uma conta
// so, usada por detail_cobre_tela e por detail_desenhar — se as duas
// divergirem, a home some um quadro antes de a arte cobrir e a tela pisca.
static void backdropRect(GfxRect *r, float *opac) {
  float s = suave(t);
  GfxRect de;
  home_hero_rect(&de.x, &de.y, &de.w, &de.h);
  r->x = de.x + (0.0f - de.x) * s;
  r->y = de.y + (0.0f - de.y) * s;
  r->w = de.w + (NV_TELA_W - de.w) * s;
  r->h = de.h + (NV_TELA_H - de.h) * s;
  // Sobe RAPIDO (s*3, nao s): a arte por baixo e a mesma, entao a rampa so
  // troca a vinheta do hero pela do detalhe.
  *opac = anim_clamp(s * 3.0f, 0.0f, 1.0f);
}

int detail_cobre_tela(void) {
  // O backdrop e FULL-BLEED: assim que ele termina de crescer, nao sobra um
  // pixel da tela anterior. Desenhar a home por baixo custava um quadro inteiro
  // de preenchimento a toa — medido em 42 ms no pior quadro.
  //
  // A CONDICAO ERA `suave(t) > 0.995`, que so e verdade em t > 0,83: a home
  // continuava sendo desenhada em 83% da abertura, e e nessa janela que estava
  // o jank medido no aparelho (clr=38,3ms com CPU ociosa — GPU afogada por
  // preenchimento, home + fundo chapado + backdrop, tres camadas de tela cheia
  // ou mais).
  //
  // Agora a pergunta e a certa: o retangulo do backdrop ja alcancou as quatro
  // bordas E ja esta opaco? Com o hero em tela cheia ele nasce praticamente do
  // tamanho da tela, entao a resposta chega em t ~ 0,13 — a home sai seis vezes
  // mais cedo. Quando a origem NAO cobre (hero em faixa, ou o detalhe aberto da
  // busca), a conta responde `nao` e a home continua desenhada: e por isso que
  // isto e uma medida de cobertura e nao um limiar novo em `t`.
  if (!aberto) return 0;
  // Carrossel: a folha opaca cobre a tela inteira (ver carFolha). Na SAIDA a
  // home e desenhada sob a folha so nos primeiros quadros: e o desenho dela que
  // diz onde o cartaz do titulo em cena ficou (home_item_focado), e o cartao
  // encolhe ate ele. Desenha-la a saida inteira custava 40-58 ms por quadro na
  // C9 (medido, 01/10): home + folha misturada + cartao.
  if (carro) return !(saindo && carEsperaRect > 0) && carFolha() >= 0.999f;
  { GfxRect r; float opac;
    backdropRect(&r, &opac);
    if (opac < 0.999f) return 0;
    return r.x <= 0.5f && r.y <= 0.5f &&
           r.x + r.w >= NV_TELA_W - 0.5f && r.y + r.h >= NV_TELA_H - 0.5f;
  }
}

// --- tabela "Detalhes do Filme" ---------------------------------------------
//
// Uma linha por campo COM VALOR. Campo vazio nao vira linha com traco: some.
// Essa e a mesma regra que desenhaAvaliacoes ja usa para fonte sem nota, e e o
// que impede a tabela de virar um formulario meio preenchido quando o TMDB nao
// tem o dado.
typedef struct { const char *chave; char valor[168]; } LinhaDet;

// "111" -> "1h 51m"; "47" -> "47min". O TMDB manda minutos crus.
static void duracaoTexto(int min, char *dst, size_t tam) {
  desc_duracao_min(min, dst, tam);   // as tres formas sao chaves da tabela
}

static int montarDetalhes(LinhaDet *o, int max) {
  int n = 0;
  const CatItem *ci = cat_item(idx);
  const char *v;

  #define DET_POE(K, S) do {                                   \
    if ((n) < (max) && (S) && (S)[0]) {                        \
      o[n].chave = (K);                                        \
      snprintf(o[n].valor, sizeof o[n].valor, "%s", (S));      \
      n++;                                                     \
    } } while (0)

  // Status cru do TMDB/Trakt ("Released", "returning series") -> rotulo no
  // idioma da interface; valor que a tabela nao conhece sai como veio.
  // Filme ja lancado: o mockup nao traz a linha (o "Lançado" nao diz nada a
  // quem esta olhando o titulo no catalogo); os outros estados ficam.
  { const char *st = extras_ficha_status();
    const char *k = desc_status_chave(st, ehSerie());
    if (ehSerie() || !st || strcasecmp(st, "Released") != 0)
      DET_POE("Status", k ? i18n(k) : st); }
  { char dt[48]; desc_data_extenso(extras_ficha_lancamento(), dt, sizeof dt);
    DET_POE("Lançamento", dt); }
  { char d[32]; duracaoTexto(extras_ficha_duracao(), d, sizeof d);
    DET_POE("Duração", d); }
  // Classificacao: a da ficha do TMDB e a boa. A do catalogo serve de reserva,
  // e desde que o "14" cravado saiu de descoberta.c ela so tem valor quando
  // veio do arquivo de catalogo, que e dado de verdade.
  v = extras_ficha_classificacao();
  if (!v || !v[0]) v = (ci && ci->classificacao[0]) ? ci->classificacao : NULL;
  DET_POE("Classificação", v);
  // Pais: a lista completa do TMDB quando ha; senao o unico que o Cinemeta da.
  v = extras_ficha_paises();
  if (!v || !v[0]) v = (ci && ci->pais[0]) ? ci->pais : NULL;
  { char pais[168]; desc_pais_txt(v, pais, sizeof pais);   // nomes em ingles -> idioma da UI
    DET_POE("País de Origem", pais); }
  // A direcao ja esta no hero ("Direção David Frankel"), como no mockup.
  // Os campos do Wikidata (orcamento, bilheteria...) entram na ficha quando a
  // consulta das frases ja voltou: no mockup e a mesma tabela.
  { int i, nf = seriefrases_n_fatos();
    for (i = 0; i < nf; i++) DET_POE(seriefrases_fato_rotulo(i), seriefrases_fato_valor(i)); }

  #undef DET_POE
  return n;
}

static int nLinhasDetalhe(void) {
  LinhaDet l[NV_DETF_DET_MAXL];
  return montarDetalhes(l, NV_DETF_DET_MAXL);
}

// Altura do CONTEUDO de uma secao (sem o cabecalho). Serve ao empilhamento do
// filme e ao culling. Antes cada numero destes vivia cravado no meio do
// desenho, e uma secao nova herdava a altura do elenco em silencio.
static int epApple(void) { return ajustes_home_layout() == HOME_LAYOUT_DINAMICA; }
static float epCardW(void) { return epApple() ? 420.0f : NV_DETP_EP_W; }
static float epCardH(void) { return epApple() ? 500.0f : NV_DETP_EP_H; }
static float epCardPasso(void) { return epApple() ? 452.0f : NV_DETP_EP_PASSO; }
static float epThumbH(void) { return epApple() ? 236.0f : NV_DETP_EP_THUMB_H; }
static float epAppleExtra(void) { return epApple() ? epCardH() - 414.0f : 0.0f; }
// Bloco das Notas na serie (cabecalho + fileira + vao), que empurra abas e
// elenco; 0 enquanto as notas nao chegaram.
// Topo do cabecalho: fundo dos cartoes de episodio + o vao antes das notas (60,
// o mesmo de vaoAntes).
// O GRAFICO DE TEMPORADAS ("Seu progresso") vem antes das Notas e as empurra.
static float progTopoSerie(void) { return epRowY() + epCardH() + 60.0f; }
static float progBloco(void) {
  return secaoN(SEC_PROGTEMP) > 0 ? alturaSecao(SEC_PROGTEMP) + 60.0f : 0.0f;
}
static float notasTopoSerie(void) { return progTopoSerie() + progBloco(); }
// Fim das Notas (= topo dos Numeros da temporada, que vem logo depois).
static float numerosTopoSerie(void) {
  float y = notasTopoSerie();
  if (secaoN(SEC_NOTAS) > 0)
    y += NV_DETF_CAB_H + NV_DETF_CAB_GAP + alturaSecao(SEC_NOTAS) + NV_DETF_SEC_GAP;
  return y;
}
// Notas + Numeros da temporada empurram abas e elenco juntos.
static float notasBloco(void) {
  float fim;
  if (!ehSerie() || (secaoN(SEC_NOTAS) <= 0 && secaoN(SEC_NUMEROS) <= 0 &&
                     secaoN(SEC_PROGTEMP) <= 0)) return 0.0f;
  fim = numerosTopoSerie();
  if (secaoN(SEC_NUMEROS) > 0) fim += alturaSecao(SEC_NUMEROS) + NV_DETF_SEC_GAP;
  return fim - NV_DETP_G_ABAS - epAppleExtra();
}
static float epExtraAltura(void) { return epAppleExtra() + notasBloco(); }
static float epTempY(void) { return epApple() ? 1160.0f : NV_DETP_TEMP_Y; }
static float epRowY(void) { return epApple() ? 1286.0f : NV_DETP_EP_Y; }
static float epAbasY(void) { return (epApple() ? 1758.0f : NV_DETP_ABA_Y) + epExtraAltura(); }
static float epElencoY(void) { return (epApple() ? 1817.0f : NV_DETP_EL_Y) + epExtraAltura(); }
static float serieGrupo(int r) {
  static const float glass[] = { NV_DETP_G_TEMP, NV_DETP_G_EP, NV_DETP_G_ABAS, NV_DETP_G_ELENCO };
  static const float apple[] = { 1080.0f, 1194.0f, 1680.0f, 1749.0f };
  return (epApple() ? apple : glass)[r];
}

static float alturaSecao(int r) {
  switch (r) {
    case SEC_TEMPORADAS: return NV_DETP_TEMP_H;
    case SEC_EPISODIOS:  return epCardH();
    case SEC_ABAS_INFO:  return NV_DETP_ABA_H;
    case SEC_ELENCO:     return NV_DETF_EL_ALT;
    case SEC_TRAILERS:     return NV_DETF_TR_ALT;
    case SEC_RELACIONADOS: return REL_CARD_H + 56.0f;   // cartaz + titulo/ano
    // Um mini card so, qualquer que seja o tamanho da saga.
    case SEC_COLECAO:      return COL_CARD_H;
    // + o cabecalho: sem ele a secao seguinte ("Detalhes do Filme") era
    // empilhada usando so a altura dos cartoes e saia POR CIMA deles.
    case SEC_COMENTARIOS:  return alturaCabComentarios() + COM_CARD_H;
    case SEC_ESTUDIOS:     return EST_CARD_H;
    case SEC_DETALHES:     return nLinhasDetalhe() * NV_DETF_DET_LINHA;
    // As duas sob demanda: a CHAMADA enquanto ninguem entrou (titulo +
    // procedencia + custo, tres linhas) e a altura MEDIDA no ultimo desenho
    // depois disso.
    // NUMEROS DA TEMPORADA: altura fixa, carregando ou nao (os cartoes dizem
    // o estado por dentro), entao os dados chegam sem deslocar a pagina.
    case SEC_AUD_ARCO:  return serieaud_bloco_altura();
    case SEC_AUD_RADAR:
    case SEC_AUD_DIGITAL: return 0.0f;
    case SEC_FRASES:    return frasesAberta ? frasesAlt : CHAMADA_H;
    case SEC_NOTAS:     return notasui_fontes_altura(notasDados());
    case SEC_NOTAS_EP:  return 0.0f;
    case SEC_PROGTEMP:  return tgraf_altura();
  }
  return 0.0f;
}

static int secaoN(int r) {
  const CatItem *ci = cat_item(idx);
  switch (r) {
    case SEC_TEMPORADAS:
      // Filme nao tem temporada: a fileira SOME em vez de mostrar abas que nao
      // levam a lugar nenhum. E o que o web faz — a `.series-season-row` so
      // existe no layout de serie.
      if (!ehSerie()) return 0;
      if (ci && ci->nTemporadas > 0)
        return ci->nTemporadas < N_ITENS ? ci->nTemporadas : N_ITENS;
      return 0;
    case SEC_EPISODIOS: {
      int q = epVisiveis();
      if (q <= 0) return 0;
      return q < N_ITENS ? q : N_ITENS;
    }
    // Uma aba so = barra escondida, como o `tabItems.length > 1` do web.
    // FILME NAO TEM ABAS: a pagina de filme empilha as secoes com cabecalho
    // proprio, entao a barra de abas nao entra. Sem esta guarda o filme ficava
    // com as duas coisas ao mesmo tempo — a barra E os cabecalhos.
    case SEC_ABAS_INFO: {
      int n;
      if (!ehSerie()) return 0;
      n = nAbasInfo();
      return n > 1 ? n : 0;
    }
    // A FILEIRA DE BAIXO E A ABA ESCOLHIDA, nao "o elenco". Este slot desenha
    // elenco, cartazes de "Mais como este", cartoes de nota, a colecao ou os
    // comentarios — desenhaSecao troca o conteudo no lugar. Se a contagem
    // continuasse sendo so a do elenco, escolher outra aba deixava a fileira com
    // o numero errado de colunas, e uma guarda no evento BLOQUEAVA descer para
    // ela por completo: dava para mexer nas abas e em mais nada.
    //
    // Sem elenco a secao nao existe — nao ha reserva. O `N_ELENCO` que ficava
    // aqui como padrao enchia a fileira com seis nomes de demonstracao mesmo num
    // titulo que o app nao sabe quem estrela.
    case SEC_ELENCO: {
      int n;
      switch (abaIdDe(abaInfo)) {
        case ABA_AVALIACOES:   n = nAvaliaveis();           break;
        case ABA_RELACIONADOS: n = extras_n_relacionados(); break;
        // O mesmo mini card do filme: uma coluna so, o OK abre a lista.
        case ABA_COLECAO:      n = extras_n_colecao() > 1 ? 1 : 0; break;
        // O cartao de comentario nao se escolhe um a um; o que RECEBE foco sao
        // as duas pilulas do seletor "Série | Episódio". Em filme nao ha
        // episodio: sobra uma coluna so, para o foco poder pousar na fileira e
        // a pagina rolar ate os cartoes.
        case ABA_COMENTARIOS:  n = ehSerie() ? 2 : 1; break;
        default:               n = (ci && ci->nElenco > 0) ? ci->nElenco : 0;
      }
      return n < NV_DETF_EL_MAX ? n : NV_DETF_EL_MAX;
    }
    // Recomendacoes e Detalhes so existem em FILME — na serie o mesmo
    // conteudo vive atras das ABAS. Trailers existem nos dois (#123).
    //
    // Estas duas ultimas eram justamente o que se perdeu ao tirar as abas do
    // filme: os dados sempre estiveram la (o log mostra "coment=8 rel=12"),
    // mas sem aba e sem secao nao havia como chegar neles.
    case SEC_TRAILERS:
#ifdef __EMSCRIPTEN__
      return extras_n_trailers();
#else
      // #417: no nativo nao ha player de YouTube; todo cartao tocava o MESMO
      // trailer do titulo (Apple/IMDb). Um cartao so, que e o que de fato toca.
      return extras_n_trailers() > 0 ? 1 : 0;
#endif
    case SEC_RELACIONADOS: {
      int n;
      n = extras_n_relacionados();
      return n < N_ITENS ? n : N_ITENS;
    }
    // A COLECAO DO FILME NAO TINHA ONDE APARECER (#194). belongs_to_collection
    // so existe em filme, e a aba ABA_COLECAO so vive na barra de abas, que e
    // so da serie (SEC_ABAS_INFO devolve 0 em filme): o pedido saia, o log dizia
    // "colecao ... -> 3" e a pagina nao mostrava nada. Aqui ela e secao propria,
    // como as recomendacoes. UMA coluna: e um mini card so, e o OK abre a
    // lista da saga (desenhaListaColecao). "> 1" pela mesma razao da aba: a
    // colecao inclui o proprio filme, e uma parte so seria ele mesmo.
    case SEC_COLECAO:
      return (!ehSerie() && extras_n_colecao() > 1) ? 1 : 0;
    // Comentario nao se escolhe um a um: UMA coluna, so para o foco pousar e a
    // pagina rolar ate os cartoes.
    // COMENTARIOS EXISTEM NOS DOIS. Na referencia a secao do Trakt fica
    // EMPILHADA abaixo da fileira de elenco tambem na serie — nao e uma aba.
    // Aqui ela so existia em filme, e na serie vivia atras de uma aba que a
    // referencia nao tem; o dono viu isso como "falta a secao do trakt na de
    // series".
    //
    // Colunas: as duas pilulas do seletor "Série | Episódio" na serie; em filme
    // nao ha episodio, entao sobra uma coluna so para o foco pousar.
    case SEC_COMENTARIOS: {
      int nc = nCartoesCom();
      if (nc <= 0 && extras_n_comentarios() <= 0) return 0;
      return nPilulasCom() + nc;
    }
    // Produtoras (filme) e redes+produtoras (serie) vindos do TMDB. Cada logo
    // e uma coluna focavel que abre o browse da entidade — o mesmo caminho das
    // pastas sinteticas TMDB das colecoes.
    case SEC_ESTUDIOS: {
      int n = extras_n_estudios();
      return n < N_ITENS ? n : N_ITENS;
    }
    // A tabela e UMA coluna focavel, nao uma por linha: o D-pad desce ate ela,
    // ela rola para a tela e pronto. Zero colunas faria focus_mover PULA-LA
    // (focus.c:25) e a secao viraria inalcancavel — logo, tambem irrolavel.
    case SEC_DETALHES:
      if (ehSerie()) return 0;
      return nLinhasDetalhe() > 0 ? 1 : 0;
    // NUMEROS DA TEMPORADA: UMA COLUNA POR EPISODIO da temporada escolhida
    // (ate o teto de pedidos do modulo, SA_EP_MAX). Esquerda/direita anda pelos
    // episodios e o mesmo episodio fica em foco nos tres cartoes
    // (serieaud_selecionar). A contagem sai de extras.h e nao do modulo de
    // audiencia: ela precisa existir ANTES do primeiro pedido, senao a fileira
    // teria zero colunas, focus_mover pularia por cima dela e o pedido (que sai
    // quando o foco ENTRA) nunca poderia ser disparado.
    //
    // Sem a lista de episodios do Trakt o bloco NAO EXISTE: as notas por
    // episodio e os numeros dos episodios saem todos do
    // `seasons?extended=episodes,full`.
    case SEC_AUD_ARCO: {
      int t = audTemp(), n;
      if (t < 0) return 0;
      n = extras_n_eps(t);
      return n < SA_EP_MAX ? n : SA_EP_MAX;
    }
    case SEC_AUD_RADAR:
    case SEC_AUD_DIGITAL:
      return 0;
    // UMA COLUNA SO, sempre, e por dois motivos que puxam para o mesmo lado:
    //
    //   as frases sao uma lista VERTICAL (como a aba "Coleção"), entao o que
    //   anda entre elas e cima/baixo tratado em detail_evento, nao a coluna; e
    //
    //   a contagem NAO PODE depender de seriefrases_n(). Ela e 0 antes de
    //   alguem entrar (nada foi pedido) e volta a 0 nos titulos sem pagina no
    //   Wikiquote — que sao a MAIORIA das series (medido: 2 de 12; nos filmes,
    //   11 de 14 tem). Se a contagem caisse a zero depois de carregar, a secao
    //   sumiria DEBAIXO do foco que acabou de entrar nela, que e o pior
    //   desfecho possivel para o D-pad.
    //
    // Entao a secao fica, e quem responde pelo vazio e o desenho: os dois
    // paineis dizem, cada um com a frase do proprio modulo, que a fonte nao tem
    // aquele titulo. Vazio dito e informacao; secao que desaparece e defeito.
    // Ainda assim ela nao existe sem IMDb id: sem ele nao ha o que perguntar
    // nem ao Wikidata nem ao Wikiquote, e a frase de vazio seria sobre uma
    // consulta que nunca aconteceu.
    case SEC_FRASES:
      return (ci && ci->imdb[0]) ? 1 : 0;
    // Sem nenhuma nota a secao nao existe (nada de painel vazio).
    // UM BLOCO POR FONTE na grade de duas colunas (detail_evento anda nela).
    case SEC_NOTAS:
      return notasui_fontes_n(notasDados());
    // A grade de episodios virou o cartao "Notas por episodio" do bloco acima.
    case SEC_NOTAS_EP:
      return 0;
    // SEU PROGRESSO: uma coluna por temporada do grafico; esquerda/direita
    // anda nelas e OK leva a fileira de episodios para aquela temporada.
    case SEC_PROGTEMP: {
      const TgDados *d;
      if (!ehSerie()) return 0;
      d = tgraf_dados(idx);
      return tgraf_existe(d) ? (d->n < N_ITENS ? d->n : N_ITENS) : 0;
    }
  }
  return 0;
}

// O botao primario e UM SO, e ele TROCA DE ROTULO conforme o estado:
// "Reproduzir" quando nunca foi aberto, "Retomar TxEy" quando ha progresso.
//
// O segundo botao ("Assistir do comeco") voltou pela issue #46: aparece so
// quando ha progresso que o player retomaria — primario "Retomar", secundario
// "do comeco". Sem progresso ele nao existe e a linha fica como antes.
// QUANTOS CIRCULARES, e a resposta depende do tipo. MEDIDO nas duas capturas
// do aparelho: o FILME ("Ma") tem tres — mais, olho de "ja assisti" e trailer —
// e a SERIE ("Lioness") tem DOIS, sem o olho. Faz sentido e nao e descuido da
// referencia: "assistido" numa serie e por episodio, e a lista de episodios
// logo abaixo ja marca isso um a um; um olho no hero teria de significar "a
// serie inteira", que nao e coisa que o Trakt guarde por titulo.
//
// Este arquivo desenhava TRES nos dois casos.
static int temInicio(void) {
  const CatItem *ci = cat_item(idx);
  if (!ci) return 0;
  // O MESMO criterio do player (player.c: retomarPct): progresso guardado
  // abaixo do Percentual assistido (ajustes_cw_concluido, 90 de fabrica). Em serie, so quando o episodio-alvo e o "Retomar" — com um
  // episodio em foco na fileira (origem 1) o primario toca AQUELE episodio e
  // nao ha retomada a desfazer.
  if (!ehSerie()) return ci->progresso > 0 && ci->progresso < ajustes_cw_concluido();
  { int t0 = 0, e0 = 0, de = 0;
    return episodioAlvo(&t0, &e0, &de) && de == 2; }
}
// "RECOMENDAR A UM AMIGO" — hoje uma linha da ilha "Mais opcoes" (detmais.h).
//
// SO EXISTE SE O PACOTE TEM O SERVICO. Sem NUVIO_REC_URL compilada,
// recomenda_ativo() e 0 e o botao nao aparece — a mesma regra do item de menu
// do cartaz e da aba Social, e pelo mesmo motivo: o dono publica builds sem a
// URL, e um circular que so da erro e pior que circular nenhum.
//
// E so em filme e serie, como o item de menu: canal e evento nao tem IMDb
// estavel para o amigo abrir do outro lado.
static int temRecomendar(void) {
  const CatItem *ci = cat_item(idx);
  if (!recomenda_ativo() || !ci || !ci->imdb[0]) return 0;
  return !strcmp(ci->tipo, "movie") || !strcmp(ci->tipo, "series");
}
// O BOTAO "LEMBRAR-ME", e as duas condicoes para ele existir.
//
// SO EM SERIE COM DATA FUTURA. Serie encerrada, cancelada ou sem data
// anunciada nao ganha botao: um "Lembrar-me" que nao tem dia para lembrar e
// exatamente a promessa vazia que este trabalho existe para nao fazer.
//
// E UM CIRCULAR, e o rotulo saiu. Pedido do dono depois de ver a captura da
// pilula: "aqui ser so o relogio e quando tiver ativo ele ficar um verdinho
// bonito". A objecao antiga a um circular era que nao havia glifo proprio —
// deixou de valer quando o despertador entrou no pacote de arte, e ele e o
// unico glifo da linha que se MEXE, entao a 3 m ele e o mais facil de achar, e
// nao o mais dificil.
//
// O ESTADO continua dito em palavra, e nao so em cor: com o botao em foco, a
// linha logo acima dele (a mesma que diz "Proximo episodio T3E9 · em 3 dias")
// passa a dizer o nome e o estado do botao. Ver a nota em desenhaLembrete: este
// app nao tem canal de leitura de tela, e todo circular dele e mudo — esta e a
// unica superficie onde um nome cabia sem empurrar a grade medida no aparelho.
static int temLembrar(void) {
  const CatItem *ci = cat_item(idx);
  if (!ehSerie() || !ci || !ci->imdb[0]) return 0;
  return agenda_pode_lembrar(ci->imdb);
}
static const char *rotuloLembrar(void) {
  const CatItem *ci = cat_item(idx);
  return i18n(ci && agenda_lembrete(ci->imdb) ? "Lembrete ativo" : "Lembrar-me");
}
// Instante em que o dono ligou o lembrete, para o despertador tocar inteiro
// no quadro seguinte ao OK. 0 = nao houve troca nesta sessao.
static Uint32 lembreteEm;
// "TROCAR ARTE" (#142): linha da ilha "Mais opcoes" (detmais.h). So
// filme e serie com id — e o id que guarda a escolha (arteescolha.h), e canal
// e evento nao tem backdrop de fonte nenhuma para escolher.
static int temArte(void) {
  const CatItem *ci = cat_item(idx);
  if (!ci || (!ci->imdb[0] && ci->tmdb <= 0)) return 0;
  return !strcmp(ci->tipo, "movie") || !strcmp(ci->tipo, "series");
}
// "ASSISTIR TRAILER" (#234): a primeira linha da ilha "Mais opcoes". So
// existe quando ha trailer para tocar (fonte conhecida na ordem do ajuste, ou
// um trailer do YouTube onde o navegador e o caminho) — sem fonte o botao some
// em vez de prometer o que nao toca. Independe do "Trailer automatico": com o
// autoplay desligado e justamente quando a pessoa o procura.
static int temTrailer(void) {
  const CatItem *ci = cat_item(idx);
  if (!ci || (strcmp(ci->tipo, "movie") && strcmp(ci->tipo, "series"))) return 0;
  if (trailer_suportado() && trailerFonte(0, NULL, 1)) return 1;
  return extras_n_trailers() > 0;
}
// "EXPLORAR" (Explorar 2.0): linha da ilha "Mais opcoes". Abre a toca
// do coelho neste titulo (explorar.h); so filme e serie, que sao o que a
// vizinhanca sabe cruzar.
static int temExplorar(void) {
  const CatItem *ci = cat_item(idx);
  if (!ci || !ci->titulo[0]) return 0;
  return !strcmp(ci->tipo, "movie") || !strcmp(ci->tipo, "series");
}
// "MAIS OPCOES" (dono, 06/10/2026: "muitos botoes na pagina de titulos"): o
// trailer, o Explorar, o Trocar arte e o Recomendar sairam da linha e moram
// na ilha do circular "..." (detmais.h), o ULTIMO da linha. Ele so existe
// quando ha ao menos uma delas — senao seria um botao que abre nada.
static void maisDisponiveis(int d[DMAIS_N]) {
  d[DMAIS_TRAILER] = temTrailer();
  d[DMAIS_EXPLORAR] = temExplorar();
  d[DMAIS_ARTE] = temArte();
  d[DMAIS_RECOMENDAR] = temRecomendar();
}
static int temMais(void) {
  int d[DMAIS_N], i;
  maisDisponiveis(d);
  for (i = 0; i < DMAIS_N; i++) if (d[i]) return 1;
  return 0;
}
static int nBotoesTodos(void) {
  return (ehSerie() ? 3 : 4) + (temInicio() ? 1 : 0) + (temLembrar() ? 1 : 0)
         + (temMais() ? 1 : 0);
}

static int acoesAgrupadas(void) {
  return carro || ajustes_home_layout() == HOME_LAYOUT_DINAMICA ||
         ajustes_home_layout() == HOME_LAYOUT_MODERNA;
}

static int nBotoes(void) {
  return acoesAgrupadas() && !maisAcoes ? 2 + (temInicio() ? 1 : 0) : nBotoesTodos();
}

// Que ACAO esta na posicao `n` da linha. As acoes tem numeros fixos (0
// primario, 1 lista, 2 assistido, 3 fontes, 4 inicio) porque detail_evento
// decide por eles; o que muda com o tipo e quais posicoes existem. Sem esta
// traducao, na serie o segundo circular (que e o de fontes) dispararia
// "marcar assistido". Quando temInicio, a posicao 1 e o secundario de texto
// e os circulares escorregam um para a direita.
enum { ACAO_PRIMARIO = 0, ACAO_LISTA = 1, ACAO_ASSISTIDO = 2, ACAO_FONTES = 3,
       ACAO_INICIO = 4, ACAO_RECOMENDAR = 5, ACAO_LEMBRAR = 6, ACAO_ARTE = 7,
       ACAO_TRAILER = 8, ACAO_EXPLORAR = 9, ACAO_MAIS = 10 };
static int acaoEm(int n) {
  if (n == 0) return ACAO_PRIMARIO;
  if (temInicio()) {
    if (n == 1) return ACAO_INICIO;
    n--;
  }
  if (acoesAgrupadas() && n > 0) {
    if (n == 1) return ACAO_LISTA;
    n--;
  }
  // O "Lembrar-me" e o SEGUNDO botao de texto, e o desconto vem antes da conta
  // dos circulares — pela mesma razao que o de temInicio: sem ele a posicao de
  // um circular escorregaria e o OK dispararia a acao do vizinho.
  if (temLembrar()) {
    if (n == 1) return ACAO_LEMBRAR;
    n--;
  }
  if (acoesAgrupadas() && n > 0) n++; // skip the list action already used as group anchor
  // O "..." E O ULTIMO DA LINHA e a conferencia vem ANTES do salto da serie:
  // com 3 circulares numa serie, a ultima posicao e n == 3, e a regra de
  // baixo devolveria 4 — que e ACAO_INICIO, o botao de texto.
  if (temMais() && n == (ehSerie() ? 3 : 4)) return ACAO_MAIS;
  if (n >= 2 && ehSerie()) return n + 1;   // serie pula o olho
  return n;
}

// Toca o trailer `k` em TELA CHEIA (o OK numa miniatura da fileira e o botao
// "Assistir trailer" da linha de acoes passam por aqui). Voltar fecha o trailer
// (trailer_evento) e a pagina continua com o mesmo foco.
static void tocarTrailerCheio(int k) {
  const char *u = trailer_suportado() ? trailerFonte(k, NULL, 1) : NULL;
  if (u) {
    GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    trailerEtapa = 0; trailerPrazo = 0;   // tela cheia: so o teclado fecha
    trailer_abrir(u, tela, trailerfonte_com_som(trailerfonte_tizen()), 1);
  }
#ifdef __EMSCRIPTEN__
  // SAMSUNG: NUNCA o navegador (#136). O window.open do wgt trocava a
  // pagina do proprio app pelo youtube.com/watch — tocava, mas sem Voltar
  // para o Nuvio. Sem fonte na ordem do ajuste (ex.: "IMDb" fixo e o
  // titulo sem IMDb), o cartao focado ainda e um video do YouTube: toca
  // AQUI, em tela cheia, e o Voltar fecha (trailer_evento).
  else if (trailer_suportado() && k < extras_n_trailers() &&
           extras_trailer_yt(k)[0]) {
    GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    trailerEtapa = 0; trailerPrazo = 0;
    trailer_abrir(extras_trailer_yt(k), tela, 0, 1);
  }
#else
  else extras_trailer_abrir(k);
#endif
}

// ---- segurar OK numa lista da pagina (ctxlista.h) -------------------------
// A celula em foco aceita o menu? Filmografia e saga: sempre que ha obra (o
// titulo fora do catalogo e pedido e o menu abre quando ele chega).
// Recomendacoes: a fileira com o foco nela.
static int listaAceitaMenu(void) {
  if (colListaAberta)
    return colListaFoco >= 0 && colListaFoco < extras_n_colecao() &&
           extras_colecao_tmdb(colListaFoco) > 0;
  if (pessoaAberta)
    return pessoaFoco >= 0 && pessoaFoco < pessoa_n_creditos() &&
           (pessoa_credito_imdb(pessoaFoco)[0] || pessoa_credito_tmdb(pessoaFoco) > 0);
  return nivel >= 1 && foco.fileira == SEC_RELACIONADOS && !focoAmigos &&
         foco.coluna >= 0 && foco.coluna < extras_n_relacionados() &&
         extras_relacionado_imdb(foco.coluna)[0];
}

// 1 se o menu abriu ou foi pedido (abre quando o titulo chegar).
static int menuNaLista(void) {
  if (colListaAberta) {
    long t = extras_colecao_tmdb(colListaFoco);
    int idx = ctxlista_indice(NULL, t);
    if (idx >= 0) return ctxlista_abrir(idx, colFocoRect, colFocoArte);
    return ctxlista_pedir(NULL, t, "movie", extras_colecao_titulo(colListaFoco),
                          extras_colecao_ano(colListaFoco),
                          extras_colecao_poster(colListaFoco), colFocoRect, colFocoArte);
  }
  if (pessoaAberta) {
    const char *id = pessoa_credito_imdb(pessoaFoco);
    long t = pessoa_credito_tmdb(pessoaFoco);
    int idx = ctxlista_indice(id, t);
    if (idx >= 0) return ctxlista_abrir(idx, pesFocoRect, pesFocoArte);
    return ctxlista_pedir(id, t, pessoa_credito_tipo(pessoaFoco),
                          pessoa_credito_titulo(pessoaFoco), pessoa_credito_ano(pessoaFoco),
                          pessoa_credito_poster(pessoaFoco), pesFocoRect, pesFocoArte);
  }
  { const char *id = extras_relacionado_imdb(foco.coluna);
    long t = !strncmp(id, "tmdb:", 5) ? atol(id + 5) : 0;
    int idx = ctxlista_indice(id, t);
    if (idx >= 0) return ctxlista_abrir(idx, relFocoRect, relFocoArte);
    return ctxlista_pedir(t > 0 ? "" : id, t, ehSerie() ? (t > 0 ? "tv" : "series") : "movie",
                          extras_relacionado_titulo(foco.coluna),
                          extras_relacionado_ano(foco.coluna),
                          extras_relacionado_poster(foco.coluna), relFocoRect, relFocoArte);
  }
}

// O OK numa obra da filmografia (toque curto): abre o titulo.
static void abrirCredito(void) {
  // Abre o titulo, quando ele for um dos que o catalogo ja tem meta.
  // Quem troca de fato e o roteador (app.c) — daqui so sai o pedido.
  //
  // Keep the person page visible until the router opens the title.
  const char *id = pessoa_credito_imdb(pessoaFoco);
  int alvo = id[0] ? cat_indice_por_imdb(id) : -1;
  if (alvo >= 0) { pedAbrir = alvo; }
  // Nao esta no catalogo: busca o meta e abre quando chegar. Quem
  // termina o trabalho e o roteador, que ja acompanha o resultado.
  // O credito quase nunca traz imdb_id, entao o caminho normal e pelo
  // id do TMDB.
  else if (id[0]) {
    desc_pedir_titulo_semente(id, 0, pessoa_credito_tipo(pessoaFoco),
                              pessoa_credito_titulo(pessoaFoco), pessoa_credito_ano(pessoaFoco),
                              pessoa_credito_poster(pessoaFoco));
  }
  else if (pessoa_credito_tmdb(pessoaFoco) > 0) {
    desc_pedir_titulo_semente("", pessoa_credito_tmdb(pessoaFoco),
                              pessoa_credito_tipo(pessoaFoco),
                              pessoa_credito_titulo(pessoaFoco), pessoa_credito_ano(pessoaFoco),
                              pessoa_credito_poster(pessoaFoco));
  }
}

// O QUE A ILHA "MAIS OPCOES" ESCOLHEU (detmais.h). Eram os ramos dos quatro
// circulares que sairam da linha; o comportamento de cada um e o mesmo.
static void executarMais(int dm) {
  if (dm == DMAIS_TRAILER) {
    tocarTrailerCheio(0);
  } else if (dm == DMAIS_EXPLORAR) {
    // O roteador (app.c) fecha a pagina e abre a toca neste titulo.
    trailer_fechar();
    pedExplorar = 1;
  } else if (dm == DMAIS_ARTE) {
    // A tela de escolha come o teclado ate fechar (topo de detail_evento).
    trailer_fechar();
    trocaarte_abrir(cat_item(idx));
  } else if (dm == DMAIS_RECOMENDAR) {
    // A MESMA MODAL DO MENU DO CARTAZ, e nao uma segunda copia dela: ver
    // recenviar.h. Aberta, ela fica acima desta tela no roteador de app.c e
    // recebe o D-pad ate fechar.
    const CatItem *ci = cat_item(idx);
    if (ci) recenviar_abrir(ci);
  }
}

void detail_evento(const SDL_Event *e) {
  if (saindo) return;
  // O CARTAO "O QUE ACHOU?" aberto pela pagina e modal: a tecla e dele.
  if (reacao_aberta() && reacao_evento(e, 1)) return;
  // CARROSSEL ANDANDO: as setas laterais continuam andando pela fileira (a
  // pagina do titulo do meio do caminho nem chegou a ser montada); qualquer
  // outra tecla monta a pagina do titulo em cena antes de agir nela.
  if (carro && !carCheia && carAplicado != carPos && e->type == SDL_KEYDOWN && nivel == 0) {
    SDL_Keycode kc = e->key.keysym.sym;
    if (kc == SDLK_RIGHT) { carPasso(1); return; }
    if (kc == SDLK_LEFT)  { carPasso(-1); return; }
    if (!(kc == SDLK_ESCAPE || kc == SDLK_AC_BACK || kc == SDLK_BACKSPACE ||
          kc == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK))
      carAplicar();
  }
  // TRAILER EM TELA CHEIA come o teclado: OK pausa, Voltar fecha. O autoplay
  // no fundo nao passa por aqui — ele nao tem teclado, a pagina continua a
  // dela, e qualquer coisa que tire a pagina do topo o fecha (detail_atualizar).
  if (trailer_cheia() && trailer_evento(e)) return;
  // "TROCAR ARTE" COME TUDO enquanto aberta (#142): e a coisa mais recente na
  // tela, e o Voltar dela fecha so ela.
  if (trocaarte_aberto()) { trocaarte_evento(e); return; }
  // A LISTA DOS AMIGOS (OK na ilha de amigos) e modal enquanto aberta.
  if (amtui_aberta()) { amtui_evento(e); return; }
  // "MAIS OPCOES" aberta come o teclado; o OK nela devolve a acao.
  if (detmais_aberto()) {
    int dm = detmais_evento(e);
    if (dm >= 0) executarMais(dm);
    return;
  }
  // MODO CINEMA: a primeira tecla so devolve o bloco de texto (o trailer
  // segue); Voltar fecha o trailer e fica na pagina.
  if (trailerCinema.oculta && e->type == SDL_KEYDOWN && !e->key.repeat) {
    SDL_Keycode kc = e->key.keysym.sym;
    trailercinema_tecla(&trailerCinema);
    if (kc == SDLK_AC_BACK || kc == SDLK_ESCAPE || kc == SDLK_BACKSPACE || kc == SDLK_DELETE ||
        e->key.keysym.scancode == NV_SCANCODE_BACK) trailer_fechar();
    return;
  }

  // O MENU DE VISTO COME OS EVENTOS. Mesma regra da ficha da pessoa logo
  // abaixo: e a coisa mais recente na tela e e para ela que a pessoa olha.
  if (episodios_menu_aberto()) {
    episodios_menu_evento(e);
    // "Fontes deste episodio" e a porta que a pressao longa tomou do card:
    // antes dela, qualquer OK ali abria as fontes.
    if (episodios_menu_pediu_fontes()) pedFontes = 1;
    return;
  }

  // A FICHA DA PESSOA come os eventos enquanto esta aberta. Ela e outra tela e
  // nao uma secao desta: deixar a tela de titulo continuar respondendo por
  // baixo faria a seta mover duas coisas ao mesmo tempo.
  //
  // O `return` no fim deste bloco e o que faz isso valer. Ele ja existia, mas a
  // chave que o abria englobava TAMBEM os dois blocos abaixo — as setas em
  // "Avaliações" e a navegacao/OK de "Mais como este" e "Coleção" estavam
  // dentro de `if (pessoaAberta)` exigindo `!pessoaAberta`, ou seja, nunca
  // rodavam. Era por isso que nao dava para andar nem abrir nada nas
  // recomendacoes: o codigo estava escrito e era inalcancavel.
  // SEGURAR OK NA FILMOGRAFIA, NA LISTA DA SAGA OU NAS RECOMENDACOES = o menu do
  // cartaz. Filmografia e saga sao telas proprias: o gesto decide na soltura.
  // Nas Recomendacoes o resto da pagina segue como estava (OK no KEYDOWN arma o
  // okDesceEm); aqui so se mede o limiar, que detail_atualizar dispara.
  { int r = ctxhold_evento(&holdLista, e, listaAceitaMenu());
    if (colListaAberta || pessoaAberta) {
      if (r == CTXH_CONSUMIDO) return;
      if (r == CTXH_TOQUE) {
        if (colListaAberta) abrirParteColecao(); else abrirCredito();
        return;
      }
    } }
  if (colListaAberta) { eventoListaColecao(e); return; }
  if (pessoaAberta) {
    if (e->type != SDL_KEYDOWN) return;
    { int n = pessoa_n_creditos();
      switch (e->key.keysym.sym) {
        // Coluna 0 da filmografia: a barra lateral por cima (dono, 03/10);
        // antes a ESQUERDA voltava para o ultimo da linha de cima.
        case SDLK_LEFT:
          if (pessoaFoco % PES_POR_LINHA == 0) pediuMenu = 1;
          else pessoaFoco--;
          return;
        case SDLK_RIGHT: if (pessoaFoco + 1 < n) pessoaFoco++; return;
        case SDLK_UP:
          if (pessoaFoco >= PES_POR_LINHA) pessoaFoco -= PES_POR_LINHA;
          if (pessoaFoco / PES_POR_LINHA < pessoaLinha) pessoaLinha--;
          return;
        case SDLK_DOWN:
          if (pessoaFoco + PES_POR_LINHA < n) pessoaFoco += PES_POR_LINHA;
          // A grade ROLA quando o foco passa da segunda linha visivel. Duas
          // linhas cabem na tela; a terceira em diante entra empurrando.
          if (pessoaFoco / PES_POR_LINHA > pessoaLinha + 1) pessoaLinha++;
          return;
        case SDLK_ESCAPE:
        case SDLK_BACKSPACE:
        case SDLK_DELETE:
        case SDLK_AC_BACK: pessoaAberta = 0; return;
        case SDLK_RETURN:
        case SDLK_KP_ENTER: abrirCredito(); return;
        default: break;
      } }
    if (e->key.keysym.scancode == NV_SCANCODE_BACK) pessoaAberta = 0;
    return;
  }
    // A aba "Mais como este" e uma LISTA VERTICAL dentro da fileira do elenco.
  // Enquanto ela estiver aberta, cima/baixo andam nela em vez de trocar de
  // fileira — e o mesmo que o web faz, onde a lista tem foco proprio.
  // No painel de notas por episodio, esquerda/direita trocam de TEMPORADA.
  if (e->type == SDL_KEYDOWN && foco.fileira == SEC_ELENCO && !pessoaAberta &&
      abaIdDe(abaInfo) == ABA_AVALIACOES && ehSerie() &&
      extras_n_temporadas() > 0) {
    int nt = extras_n_temporadas();
    // Os graficos abaixo trocam junto, na hora, se ja estavam abertos.
    if (e->key.keysym.sym == SDLK_RIGHT && ratTemp + 1 < nt) { ratTemp++; if (audAberta) abrirAudiencia(); return; }
    if (e->key.keysym.sym == SDLK_LEFT  && ratTemp > 0)      { ratTemp--; if (audAberta) abrirAudiencia(); return; }
  }

  // "Colecao" na serie: o MESMO mini card do filme, e o OK abre a lista.
  // "Mais como este" continua a fileira horizontal de cartazes.
  if (e->type == SDL_KEYDOWN && foco.fileira == SEC_ELENCO && !pessoaAberta &&
      (abaIdDe(abaInfo) == ABA_RELACIONADOS || abaIdDe(abaInfo) == ABA_COLECAO)) {
    int col = (abaIdDe(abaInfo) == ABA_COLECAO);
    int n = col ? 1 : extras_n_relacionados();
    if (n > 7) n = 7;
    switch (e->key.keysym.sym) {
      case SDLK_RIGHT: if (!col && relFoco + 1 < n) { relFoco++; return; } break;
      case SDLK_LEFT:  if (!col && relFoco > 0)     { relFoco--; return; } break;
      case SDLK_RETURN:
      case SDLK_KP_ENTER: {
        if (col) {
          abrirListaColecao();
        } else {
          const char *id = extras_relacionado_imdb(relFoco);
          // "tmdb:<id>" = recomendacao do TMDB (tmdb_use_more_like_this): nao
          // tem imdb ate a meta chegar, entao abre pelo id do TMDB direto.
          if (!strncmp(id, "tmdb:", 5))
            desc_pedir_titulo_semente("", atol(id + 5), ehSerie() ? "tv" : "movie",
                                      extras_relacionado_titulo(relFoco), extras_relacionado_ano(relFoco),
                                      extras_relacionado_poster(relFoco));
          else {
            int alvo = cat_indice_por_imdb(id);
            if (alvo >= 0) pedAbrir = alvo;
            else if (id[0]) desc_pedir_titulo_semente(id, 0, ehSerie() ? "series" : "movie", extras_relacionado_titulo(relFoco),
                                                      extras_relacionado_ano(relFoco),
                                                      extras_relacionado_poster(relFoco));
          }
        }
        return; }
      default: break;
    }
    // CIMA no primeiro item e BAIXO no ultimo caem no comportamento normal e
    // saem da lista — senao o foco fica preso nela.
  }



  // AS FRASES SAO UMA LISTA VERTICAL, como a aba "Coleção": cima e baixo andam
  // DENTRO dela. A secao tem uma coluna so de proposito (ver secaoN), entao nao
  // ha o que fazer com esquerda/direita aqui.
  //
  // NAS PONTAS O EVENTO PASSA ADIANTE e o foco sai da secao — a mesma regra da
  // colecao, e o que impede a ultima secao do documento de virar uma armadilha
  // de onde so se sai pelo Voltar.

  if (e->type == SDL_KEYDOWN && nivel >= 1 && foco.fileira == SEC_FRASES &&
      !pessoaAberta && seriefrases_n() > 0) {
    int i = seriefrases_selecionado(), n = seriefrases_n();
    if (e->key.keysym.sym == SDLK_DOWN && i + 1 < n) {
      seriefrases_selecionar(i + 1); return;
    }
    if (e->key.keysym.sym == SDLK_UP && i > 0) {
      seriefrases_selecionar(i - 1); return;
    }
  }

  if (e->type == SDL_KEYDOWN && (e->key.keysym.sym == SDLK_RETURN ||
                                 e->key.keysym.sym == SDLK_KP_ENTER)) {
    if (!okDesceEm) okDesceEm = SDL_GetTicks();
    return;
  }
  if (e->type == SDL_KEYUP && (e->key.keysym.sym == SDLK_RETURN ||
                               e->key.keysym.sym == SDLK_KP_ENTER)) {
    Uint32 dur;
    // SOLTAR sem ter PRESSIONADO nao e clique. Sem esta guarda o detalhe
    // reproduzia sozinho ao ser aberto: o OK apertado na home entrega o KEYDOWN
    // a home (que abre o detalhe) e o KEYUP JA CHEGA AQUI, com nivel 0 e botao
    // 0 — que e exatamente "Reproduzir". Da para ver como o dono descreveu:
    // "clica num titulo e ele ja clica duas vezes e inicia".
    //
    // Antes isto nao aparecia porque o botao morava no nivel 1 e o KEYUP orfao
    // caia em nenhum caso. Passar os botoes para o nivel 0 (que e onde o web os
    // poe) descobriu o defeito que ja existia.
    if (!okDesceEm) return;
    dur = SDL_GetTicks() - okDesceEm;
    okDesceEm = 0;
    if (nivel == 0 && focoAmigos) {
      // A lista do que cada amigo achou, AQUI (dono, 06/10/2026), e nao mais a
      // aba Atividade do painel — que repetia so "viu".
      AmigosTitulo at;
      int ut, ue;
      if (amigosDoTitulo(&at)) {
        ultimoEpisodio(&ut, &ue);
        amtui_abrir(&at, ut, ue, tituloDe(idx));
      }
    } else if (nivel == 0) {
      // Ordem FIXA: primario, adicionar a lista, marcar como visto, fontes.
      //
      // O botao do olho caia no `else` e abria a folha de FONTES — ele nunca
      // marcou nada, apesar do icone. Agora tem pedido proprio.
      int acao = acaoEm(botao);
      if (acao == ACAO_PRIMARIO) {
        if (dur >= NV_HOLD_MS) pedFontes = 1; else pedReproduzir = 1;

      } else if (acao == ACAO_INICIO) {
        // "Assistir do comeco" (issue #46): mesmo caminho do primario, mas o
        // roteador zera a retomada DESTA sessao depois de armar o episodio.
        pedDoInicio = 1;
      } else if (acao == ACAO_LEMBRAR) {
        // Liga/desliga na hora e GRAVA. Nao ha confirmacao nem folha: o estado
        // volta no proprio rotulo do botao, que e o unico lugar onde o dono
        // vai procurar por ele.
        const CatItem *ci = cat_item(idx);
        if (ci && ci->imdb[0]) {
          agenda_alternar_lembrete(ci->imdb);
          lembreteEm = SDL_GetTicks();
        }
      } else if (acao == ACAO_LISTA) {
        pedMarcar = 1;
      } else if (acao == ACAO_ASSISTIDO) {
        pedAssistido = 1;
      } else if (acao == ACAO_MAIS) {
        int d[DMAIS_N];
        maisDisponiveis(d);
        detmais_abrir(d);
      } else {
        pedFontes = 1;
      }
    } else if (foco.fileira == SEC_RELACIONADOS) {
      // Recomendacoes de filme e serie: abre do catalogo quando ja temos
      // meta, senao pede pelo tipo correto e o roteador
      // termina quando chegar. "tmdb:<id>" = recomendacao do TMDB.
      const char *id = extras_relacionado_imdb(foco.coluna);
      if (!strncmp(id, "tmdb:", 5))
        desc_pedir_titulo_semente("", atol(id + 5), ehSerie() ? "tv" : "movie",
                                  extras_relacionado_titulo(foco.coluna), extras_relacionado_ano(foco.coluna),
                                  extras_relacionado_poster(foco.coluna));
      else {
        int alvo = id[0] ? cat_indice_por_imdb(id) : -1;
        if (alvo >= 0) pedAbrir = alvo;
        else if (id[0]) desc_pedir_titulo_semente(id, 0, ehSerie() ? "series" : "movie", extras_relacionado_titulo(foco.coluna),
                                                  extras_relacionado_ano(foco.coluna),
                                                  extras_relacionado_poster(foco.coluna));
      }
    } else if (foco.fileira == SEC_COLECAO) {
      // O mini card abre a LISTA da saga; e la que se escolhe a parte.
      abrirListaColecao();
    } else if (foco.fileira == SEC_PROGTEMP) {
      progAbrirTemporada(foco.coluna);
    } else if (foco.fileira == SEC_TEMPORADAS && dur >= NV_HOLD_MS) {
      // PRESSAO LONGA NA ABA: o menu da temporada (issue #108, "Pressing
      // 'Season' brings up option to mark all as watched"). O toque curto
      // continua trocando de aba — e o gesto de todo dia, e o de marcar a
      // temporada inteira nao pode sair por engano num OK comum. A aba
      // segurada passa a ser a escolhida, para os checks que mudarem estarem
      // na lista que aparece por tras.
      temporada = foco.coluna;
      irParaTemporada(temporada, 0);
      episodios_menu_temporada(idx, temporadaEm(foco.coluna));
    } else if (foco.fileira == SEC_TEMPORADAS) {
      // Trocar de aba BUSCA a temporada. Antes so mudava o realce e a lista
      // continuava a mesma, o que fazia a aba parecer quebrada.
      temporada = foco.coluna;
      irParaTemporada(temporada, 1);
    } else if (foco.fileira == SEC_ELENCO && abaIdDe(abaInfo) == ABA_ELENCO) {
      // OK num rosto abre a FILMOGRAFIA da pessoa. E o `openCastDetail` do web
      // (metaDetailsScreen.js:6165); aqui o OK no elenco nao fazia nada.
      const CatItem *ci = cat_item(idx);
      if (ci && foco.coluna < ci->nElenco && ci->elenco[foco.coluna].tmdb > 0) {
        pessoa_pedir(ci->elenco[foco.coluna].tmdb,
                     ci->elenco[foco.coluna].nome,
                     ci->elenco[foco.coluna].foto);
        pessoaAberta = 1;
        pessoaFoco = 0;
        pessoaLinha = 0;
      }
    } else if (foco.fileira == SEC_ABAS_INFO) {
      abaInfo = foco.coluna;
    } else if (foco.fileira == SEC_COMENTARIOS && foco.coluna < nPilulasCom()) {
      // "Série | Episódio": a pilula escolhe a fonte dos cartoes (ver a nota
      // em detail_atualizar sobre por que nao e ao passar o foco).
      comentEp = foco.coluna;
    } else if (foco.fileira == SEC_TRAILERS) {
      // OK num trailer. Onde ha pagina (Samsung), o trailer toca AQUI, em
      // tela cheia e com som, atras do canvas (trailer.h) — era o #82: o
      // navegador da TV abria por cima e a pessoa nao sabia voltar. Na LG
      // continua o navegador do webOS: o app nativo nao tem onde embutir um
      // player do YouTube.
      //
      // Som: so onde a fonte tem (LG). Na Samsung a tela cheia e muda — a
      // Apple la e so video e o dono nao quer troca para o YouTube por som.
      // No .tpk a tela cheia tem som, e por isso o IMDb (MP4 com audio) vem
      // antes da Apple (so video) em Automatico — trailerFonte(..., 1), #178.
      tocarTrailerCheio(foco.coluna);
    } else if (foco.fileira == SEC_ESTUDIOS) {
      // OK num logo abre o browse daquela produtora/rede no vertudo — e a
      // mesma pasta sintetica TMDB que as colecoes usam (issue #44), montada
      // na hora. Sem id numerico nao ha endpoint para chamar: o OK nao faz
      // nada em vez de adivinhar pelo nome.
      //
      // vertudo_colecao guarda o PONTEIRO da pasta, nao uma copia — por isso a
      // struct e estatica e nao local.
      static ColFolder pasta;
      static ColSource pastaFontes[COL_SOURCE_MAX];   // a pasta so aponta (#255)
      long tmdbId = extras_estudio_tmdb(foco.coluna);
      if (tmdbId > 0) {
        memset(&pasta, 0, sizeof pasta);
        memset(pastaFontes, 0, sizeof pastaFontes);
        pasta.sources = pastaFontes;
        snprintf(pasta.title, sizeof pasta.title, "%s",
                 extras_estudio_nome(foco.coluna));
        snprintf(pasta.sources[0].prov, sizeof pasta.sources[0].prov, "tmdb");
        snprintf(pasta.sources[0].tmdbTipo, sizeof pasta.sources[0].tmdbTipo,
                 "%s", extras_estudio_rede(foco.coluna) ? "NETWORK" : "COMPANY");
        pasta.sources[0].tmdbId = tmdbId;
        snprintf(pasta.sources[0].midia, sizeof pasta.sources[0].midia,
                 "%s", ehSerie() ? "TV" : "MOVIE");
        pasta.nSources = 1;
        vertudo_colecao(&pasta);
        // vertudo vive ABAIXO do detalhe na pilha de telas: os eventos so
        // chegam a ela quando o detalhe nao esta aberto, e o desenho idem.
        // Sem `saindo` a lista abria escondida atras da pagina — e so
        // aparecia quando a pessoa desistia e voltava para a home.
        saindo = 1;
      }
    } else if (foco.fileira == SEC_EPISODIOS) {
      // PRESSAO LONGA ABRE O MENU DE VISTO; o toque curto continua abrindo as
      // fontes, que e o que este card sempre fez.
      //
      // "marcar este / ate aqui / a temporada inteira" existia desde a 1.0.25 e
      // era INALCANCAVEL daqui: o menu so vivia dentro da folha de episodios, e
      // a folha so abre de dentro do player. Quem estava na pagina de detalhe —
      // que e onde qualquer um iria procurar — segurava o card e via as fontes.
      const CatEp *ep = cat_episodio(idx, epAbsoluto(foco.coluna));
      if (dur >= NV_HOLD_MS && ep)
        episodios_menu_visto(idx, ep->temporada, ep->episodio, ep->nome);
      else
        // ISSUE #57, SEGUNDA VOLTA. O toque curto no card do episodio abria a
        // folha de fontes SEMPRE — e este e o card em que uma pessoa aperta OK
        // para retomar uma serie. "Retomar" acabava em "escolha um link de
        // novo", que e a descricao do relator palavra por palavra.
        //
        // A PRIMEIRA CORRECAO SO VALEU PARA METADE DAS PESSOAS, e o relator
        // voltou dizendo que o defeito continuava. Ela tocava direto apenas
        // quando havia fonte LEMBRADA (fontepref_tem), e fontepref_guardar() e
        // chamado num unico lugar: quando a pessoa escolhe a fonte NA MAO, na
        // folha (app.c, em stream_folha_escolheu). O que o automatico escolhe
        // nao vira preferencia, de proposito. Ou seja: quem nunca abriu a folha
        // nunca tinha preferencia, caia no `else` e via a folha de novo — a
        // condicao da correcao excluia exatamente quem mais reclamava.
        //
        // Agora toca sempre, que e o que "Retomar" quer dizer. Se o app sabe
        // escolher fonte sozinho na PRIMEIRA reproducao, sabe escolher na
        // retomada; nao ha nada a perguntar aqui. A lembrada, quando existe,
        // continua indo para a frente da fila de verificacao em app.c.
        //
        // A FOLHA NAO FICOU INALCANCAVEL, que era a razao de ela estar neste
        // toque: o hold neste mesmo card abre o menu do episodio, que tem
        // "Fontes deste episodio" (episodios.c), e o hold no botao primario
        // abre a folha do titulo (ver o ramo de NV_HOLD_MS acima). Quem quer
        // trocar de fonte tem dois caminhos; quem quer continuar vendo tem o
        // toque curto, que e o caso comum.
        pedReproduzir = 1;
    }
    return;
  }

  if (e->type != SDL_KEYDOWN) return;
  SDL_Keycode k = e->key.keysym.sym;

  if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE) {
    if (acoesAgrupadas() && maisAcoes && nivel == 0) { maisAcoes = 0; botao = temInicio() ? 1 : 0; }
    else if (nivel > 0) nivel = 0;
    else if (carro && carCheia) carCheia = 0;   // tela cheia no topo -> cartao
    else if (nVolta > 0) { pedAbrir = voltaPilha[--nVolta]; voltando = 1; }   // volta ao titulo de onde veio
    else { saindo = 1; pediuMenu = 0; }   // Voltar nao abre um menu pedido antes
    return;
  }
  if (nivel == 0) {
    // CIMA na linha de botoes nao fazia nada; com uma reacao pendente ele abre
    // a pergunta (reacao.h). Sem pendencia, continua sem fazer nada.
    // CIMA na ilha de amigos volta ao botao em que estava; BAIXO entra nas secoes;
    // ESQUERDA pede o menu lateral como o primeiro botao.
    if (focoAmigos) {
      if (k == SDLK_UP) { focoAmigos = 0; return; }
      if (k == SDLK_LEFT) { pediuMenu = 1; return; }
      if (k != SDLK_DOWN) return;
      focoAmigos = 0;
    } else if (k == SDLK_DOWN && !(carro && !carCheia)) {
      AmigosTitulo at;
      if (amigosDoTitulo(&at)) { focoAmigos = 1; return; }
    }
    if (k == SDLK_UP && reacao_detalhe_abrir(cat_item(idx))) return;
    if (k == SDLK_UP && !e->key.repeat) { pediuSocial = 1; return; }
    // CARROSSEL: a primeira seta para baixo so estica o cartao (ver carCheia).
    if (k == SDLK_DOWN && carro && !carCheia) { carCheia = 1; return; }
    if (k == SDLK_DOWN) {
      // Descer do hero cai na primeira fileira FOCAVEL. Num filme nao ha
      // temporadas nem episodios, e parar numa fileira vazia deixava o D-pad
      // sem resposta. Tem de ser secaoColunas e nao secaoN: os trailers sao
      // DESENHADOS mas nao aceitam foco, e um filme sem elenco pousaria neles.
      //
      // A COLUNA VEM DA MEMORIA, nao de zero. Issue #43: cravar 0 aqui jogava
      // fora a temporada e o episodio que detail_abrir semeou em
      // colunaLembrada — a fileira de temporadas abria sempre na primeira aba,
      // e ela e quem trocaria a lista de episodios para a temporada errada no
      // proximo quadro (ver o sincronizador em detail_atualizar).
      for (int o = 0; o < N_ORDEM; o++) {
        int r = ordemSecoes()[o];
        if (secaoColunas(r) > 0) {
          int alvo = foco.colunaLembrada[r];
          if (alvo >= secaoColunas(r)) alvo = secaoColunas(r) - 1;
          if (alvo < 0) alvo = 0;
          foco.fileira = r; foco.coluna = alvo; nivel = 1;
          break;
        } }
    }
    else if (k == SDLK_RIGHT) {
      // Carrossel: mais um direita na ponta da linha de botoes e o PROXIMO
      // titulo da fileira (o mesmo gesto do app da Apple).
      if (botao < nBotoes() - 1) botao++;
      else if (carro && !carCheia) carPasso(1);
    }
    else if (k == SDLK_LEFT)  {
      // ESQUERDA no primeiro botao pede a barra lateral, que abre POR CIMA
      // da pagina (app.c). Um toque so. No carrossel ela e o titulo
      // ANTERIOR; so no primeiro da fileira pede a barra.
      if (botao > 0) botao--;
      else if (carro && !carCheia && carPos > 0) carPasso(-1);
      else if (!(carro && carCheia)) pediuMenu = 1;
    }
    if (acoesAgrupadas()) maisAcoes = nivel == 0 && botao >= 1 + (temInicio() ? 1 : 0);
    return;
  }
  // A guarda que existia aqui bloqueava DESCER das abas sempre que a aba
  // escolhida nao fosse "Criador e elenco" — e com isso trancava o acesso a
  // "Mais como este", "Coleção" e "Comentários", cujo codigo de navegacao ja
  // estava escrito logo acima e nunca era alcancado.
  //
  // Nao e mais preciso: secaoN devolve a contagem DA ABA ATIVA, entao a fileira
  // ou tem colunas de verdade (e o foco pousa no que esta desenhado) ou tem
  // zero, e focus_mover pula sozinho.
  // FILEIRA DE EPISODIOS DE SERIE: a fileira e horizontal, entao esquerda/
  // direita ANDAM pelos episodios e, na PONTA, passam para a temporada vizinha
  // (dono, 03/10: trocar de temporada so apertando para os lados em cima do
  // episodio). Direita no ultimo -> primeiro da proxima; esquerda no primeiro
  // -> ultimo da anterior. Na primeira/ultima temporada nada muda (esquerda no
  // primeiro da T1 continua pedindo o menu lateral).
  if (ehSerie() && foco.fileira == SEC_EPISODIOS && (k == SDLK_LEFT || k == SDLK_RIGHT) &&
      detail_ep_borda(k == SDLK_RIGHT ? 1 : -1)) return;
  // NOTAS: os blocos de fonte sao uma grade de DUAS colunas. Cima/baixo anda
  // dentro dela antes de sair da secao; esquerda/direita nao atravessa a linha
  // (esquerda na coluna da esquerda pede o menu, como nas outras fileiras).
  if (foco.fileira == SEC_NOTAS) {
    int nc = foco.nColunas[SEC_NOTAS], c = foco.coluna;
    // Linha de baixo incompleta: da coluna da direita desce para o ultimo bloco.
    if (k == SDLK_DOWN && c / NOTASUI_COLUNAS < (nc - 1) / NOTASUI_COLUNAS) {
      foco.coluna = c + NOTASUI_COLUNAS < nc ? c + NOTASUI_COLUNAS : nc - 1;
      return;
    }
    if (k == SDLK_UP && c >= NOTASUI_COLUNAS) { foco.coluna -= NOTASUI_COLUNAS; return; }
    if (k == SDLK_RIGHT) {
      if (c % NOTASUI_COLUNAS < NOTASUI_COLUNAS - 1 && c + 1 < nc) foco.coluna++;
      return;
    }
    if (k == SDLK_LEFT && c % NOTASUI_COLUNAS > 0) { foco.coluna--; return; }
    if (k == SDLK_LEFT) { pediuMenu = 1; return; }
  }
  if (k == SDLK_RIGHT) {
    if (!focus_mover(&foco, 1, 0) && !ehSerie() && parDir(foco.fileira) >= 0 &&
        foco.nColunas[parDir(foco.fileira)] > 0) {
      foco.colunaLembrada[foco.fileira] = foco.coluna;
      foco.fileira = parDir(foco.fileira); foco.coluna = 0;
    }
  }
  else if (k == SDLK_LEFT)  {
    if (!focus_mover(&foco, -1, 0)) {
      int e2 = ehSerie() ? -1 : parEsq(foco.fileira);
      if (e2 >= 0 && foco.nColunas[e2] > 0) {
        foco.fileira = e2;
        foco.coluna = foco.nColunas[e2] - 1;
      } else pediuMenu = 1;
    }
  }
  else if (k == SDLK_DOWN)  moverFileira(1);
  else if (k == SDLK_UP)    { if (!moverFileira(-1)) nivel = 0; }
}

// Pilulas de "Producao": largura pelo nome; quebram linha na largura da
// coluna da secao (no filme, a metade direita da linha Ficha | Producao).
// Logo da produtora/rede: altura fixa na pilula; a largura sai da proporcao do
// arquivo (3:1 ate baixar) e e limitada. 0 = sem logo, a pilula leva o nome.
#define EST_LOGO_H 30.0f
#define EST_LOGO_WMAX 190.0f
static int estLogoDim(int c, float *w, float *h) {
  const char *logo = extras_estudio_logo(c);
  float ap;
  if (!logo[0]) return 0;
  ap = tex_aspecto(logo);
  if (ap <= 0.0f) ap = 3.0f;
  *h = EST_LOGO_H; *w = *h * ap;
  if (*w > EST_LOGO_WMAX) { *w = EST_LOGO_WMAX; *h = *w / ap; }
  return 1;
}
static float estLargura(int c) {
  float lw, lh;
  int w;
  if (estLogoDim(c, &lw, &lh)) return lw + EST_PAD * 2.0f;
  w = txt_largura(TXT_G18M, extras_estudio_nome(c));
  if (w > 420) w = 420;
  return (float)w + EST_PAD * 2.0f;
}
static float estColuna(void) {
  return ehSerie() ? NV_TELA_W - NV_DETP_X * 2 : wSec[SEC_ESTUDIOS];
}
static void estPosicao(int c, float *x, float *y) {
  float px = 0.0f, py = 0.0f, lim = estColuna();
  for (int k = 0; k <= c; k++) {
    float w = estLargura(k);
    if (px > 0.0f && px + w > lim) { px = 0.0f; py += EST_CARD_H + EST_GAP; }
    if (k == c) break;
    px += w + EST_GAP;
  }
  *x = px; *y = py;
}
static float estAltura(void) {
  int n = extras_n_estudios();
  float x, y;
  if (n <= 0) return 0.0f;
  if (n > N_ITENS) n = N_ITENS;
  estPosicao(n - 1, &x, &y);
  return y + EST_CARD_H;
}
// Deslocamento vertical do item dentro da secao (so as pilulas quebram linha).
static float yItem(int r, int c) { (void)r; (void)c; return 0; }

// Largura do item e passo horizontal de cada fileira. Temporada e aba de
// informacao tem largura VARIAVEL (saem do texto), e por isso o passo delas nao
// e uma constante como a do episodio.
static float larguraItem(int r, int c) {
  switch (r) {
    case SEC_TEMPORADAS:  return larguraTemporada(c);
    case SEC_EPISODIOS:   return epCardW();
    case SEC_ABAS_INFO:   return larguraAbaInfo(c);
    case SEC_TRAILERS:     return NV_DETF_TR_W;
    case SEC_RELACIONADOS: return REL_CARD_W;
    case SEC_COMENTARIOS:
      return (c < nPilulasCom()) ? larguraPilulaCom(rotuloPilulaCom(c)) : COM_CARD_W;
    // A tabela e um bloco so, da largura da divisoria. Cair no `default` daria
    // a ela a largura de um avatar de elenco, e o culling horizontal cortaria
    // a tabela fora da tela.
    case SEC_ESTUDIOS:    return EST_CARD_W;
    case SEC_DETALHES:    return NV_DETF_DET_W;
    // Bloco unico da largura do conteudo: os graficos e os dois paineis de
    // frases ocupam a faixa inteira. Cair no `default` daria a eles a largura
    // de um avatar e o recorte horizontal cortaria tudo fora da tela.
    case SEC_AUD_ARCO:
    case SEC_AUD_RADAR:
    case SEC_AUD_DIGITAL:
    case SEC_NOTAS:
    case SEC_NOTAS_EP:
    case SEC_PROGTEMP:
    case SEC_FRASES:      return NV_TELA_W - NV_DETP_X * 2;
    case SEC_COLECAO:     return COL_CARD_W;
    default:              return NV_DETP_EL_W;
  }
}
// x do item `c` DENTRO da fileira (antes da rolagem horizontal).
static float xItem(int r, int c) {
  float x = NV_DETP_X;
  for (int k = 0; k < c; k++) {
    if (r == SEC_EPISODIOS) { x += epCardPasso(); continue; }
    if (r == SEC_ELENCO)    { x += NV_DETP_EL_PASSO; continue; }
    if (r == SEC_TRAILERS)  { x += NV_DETF_TR_PASSO;  continue; }
    if (r == SEC_RELACIONADOS) { x += REL_CARD_W + REL_CARD_GAP; continue; }
    if (r == SEC_ESTUDIOS)     { x += EST_CARD_W + EST_GAP; continue; }
    if (r == SEC_COMENTARIOS) {
      // As pilulas somam largura + vao; os CARTOES recomecam em NV_DETP_X
      // porque ficam numa LINHA de baixo. xItem deixa de ser monotonico nesta
      // fileira, e nao ha problema: a rolagem horizontal so consulta a coluna
      // FOCADA, nunca a sequencia inteira.
      int np = nPilulasCom();
      if (c <= np) x += larguraPilulaCom(rotuloPilulaCom(k)) + COM_PILL_GAP;
      else if (k >= np) x = NV_DETP_X + (float)(c - np) * (COM_CARD_W + COM_CARD_GAP);
      continue;
    }
    if (r == SEC_DETALHES)  { continue; }   // coluna unica: sempre em NV_DETP_X
    // Audiencia tem uma coluna por EPISODIO, mas elas nao sao itens lado a
    // lado: sao posicoes dentro de um grafico que ocupa a faixa inteira. Todas
    // comecam em NV_DETP_X, e quem marca a escolhida e serieaud_selecionar.
    // Frases idem, com a coluna unica.
    if (EH_AUD(r) || r == SEC_FRASES || r == SEC_NOTAS || r == SEC_NOTAS_EP ||
        r == SEC_PROGTEMP) continue;
    if (r == SEC_TEMPORADAS) x += larguraTemporada(k) + NV_DETP_TEMP_GAP;
    else x += larguraAbaInfo(k) + NV_DETP_ABA_SEP * 2 + 9.0f;  // 9 = largura do "|"
  }
  return x;
}

// Reconta as colunas de cada secao a cada quadro.
//
// O focus_iniciar do detail_abrir congela nColunas com o que EXISTE NA HORA da
// abertura — e os episodios, as temporadas e o elenco chegam DA REDE, segundos
// depois. Com a contagem parada em zero o focus_mover recusa qualquer passo
// lateral (`novo < nColunas[fileira]` nunca passa), que e o defeito relatado:
// "a lista de episodios nao mexe para os lados".
//
// Sai cedo quando nada mudou, entao custa N comparacoes de inteiro. Mesmo
// padrao do sincronizarFileiras() da home, pela mesma razao: quem preenche o
// catalogo e outro fio.
// Quantas colunas da secao aceitam FOCO. Nem sempre e o mesmo que secaoN, que
// diz quantas se DESENHA.
//
// Trailers e o caso: os cards aparecem, mas nao recebem foco. Este port nao tem
// reprodutor de YouTube, e a regra ja escrita duas vezes neste codigo — o botao
// de trailer removido do hero, o glifo do YouTube trocado no terceiro circular
// — e que um controle que promete o que nao cumpre e pior que a ausencia dele.
// Pular a fileira nao esconde nada: ao descer do Elenco para os Detalhes a
// rolagem passa por cima dos trailers e eles ficam visiveis no caminho.
static int secaoColunas(int r) {
  // TRAILERS SAO FOCAVEIS. Eles ficaram fora do foco por um tempo, pelo
  // argumento de que este port nao toca YouTube e um controle que promete o
  // que nao cumpre e pior que a ausencia dele — a mesma regra que tirou o botao
  // de trailer do hero.
  //
  // O dono pediu o contrario, e tem razao no caso: pular a fileira inteira
  // impede ate de PERCORRER os trailers para ler os nomes, e "nao consigo
  // navegar nos trailers" e um defeito maior que um OK sem efeito. O card
  // continua sem acao ao apertar OK enquanto nao houver reprodutor.
  return secaoN(r);
}

static void sincronizarColunas(void) {
  int r, mudou = 0;
  for (r = 0; r < N_SECOES; r++) {
    int n = secaoColunas(r);
    if (foco.nColunas[r] != n) { foco.nColunas[r] = n; mudou = 1; }
  }
  if (!mudou) return;
  // A LISTA VAZIA E TRANSITORIA, E GRAMPEAR NELA PERDIA O EPISODIO (#102).
  // cat_definir_tudo zera nEps de proposito a cada republicacao da descoberta,
  // entao a fileira fica em zero por alguns quadros; grampear a coluna a 0 ai
  // jogava o foco no primeiro episodio da temporada e epAncora copiava o zero
  // logo a seguir — e o carrossel "voltava sozinho para o E1" enquanto a pessoa
  // lia o menu por cima. Enquanto nao ha episodios, a coluna fica onde estava;
  // quando voltam, ela e RELOCALIZADA pelo numero do episodio guardado em
  // comEpT/comEpE, que e estavel e sobrevive a troca do bloco do catalogo.
  if (foco.nColunas[SEC_EPISODIOS] < 1) return;
  if (comEpT > 0 && comEpE > 0) {
    int c, n = foco.nColunas[SEC_EPISODIOS];
    for (c = 0; c < n; c++) {
      const CatEp *e = cat_episodio(idx, epAbsoluto(c));
      if (e && e->temporada == comEpT && e->episodio == comEpE) {
        if (foco.fileira == SEC_EPISODIOS) foco.coluna = c;
        epAncora = c;
        break;
      }
    }
  }
  if (foco.coluna >= foco.nColunas[foco.fileira])
    foco.coluna = foco.nColunas[foco.fileira] > 0
                ? foco.nColunas[foco.fileira] - 1 : 0;
}

// O INDICE NAO E ESTAVEL, E ESTA TELA VIVE MINUTOS.
//
// `idx` e uma posicao no vetor do catalogo, e cat_definir_tudo TROCA O BLOCO
// INTEIRO (tres pontos em descoberta.c). Toda republicacao com esta tela aberta
// fazia o indice apontar para quem passou a ocupar aquela posicao — e o
// resultado era a pagina de titulo trocando sozinha para outro filme.
//
// MEDIDO NO RELATO, e as tres partes dele batem com esta causa: "pisca" e a
// troca do bloco; "abre um titulo diferente" e o novo ocupante; "primeiro abre
// algo de Continuar assistindo" porque aqueles itens ocupam as PRIMEIRAS
// posicoes do catalogo, entao um titulo aberto de la tem indice 0..7, que e
// justamente a faixa que a republicacao reescreve primeiro. E "depois de alguns
// minutos parado" e o sync periodico, que roda a cada cinco.
//
// O player nao sofria disso porque o sync nao roda com ele aberto — pensaram
// nele e nao nesta tela. Issue #16.
//
// A identidade estavel e o imdb. Quando o titulo some do catalogo (o catalogo
// novo pode nao trazer a fileira de onde ele veio), a copia guardada na abertura
// volta por cat_acrescentar: melhor reinseri-lo do que deixar a tela mostrando
// outra obra.
static void revalidarIdx(void) {
  const CatItem *ci;
  int novo;
  if (!idxImdb[0]) return;
  ci = cat_item(idx);
  if (ci && !strcmp(ci->imdb, idxImdb)) return;      // caso comum: nada mudou
  novo = cat_indice_por_imdb(idxImdb);
  if (novo < 0 && idxTemCopia) novo = cat_acrescentar(&idxCopia);
  if (novo < 0) return;                              // sem para onde ir: fica
  // O MESMO TITULO NA MESMA POSICAO, so com o id escrito de outro jeito: o
  // card de serie carrega o episodio ("tt0052520:1:32") e a copia que ficou
  // nesta posicao nao, ou o contrario. cat_indice_por_imdb casa os dois
  // (mesmoTitulo) e devolve o proprio idx — nao houve remontagem nenhuma.
  // Antes, o strcmp acima falhava em TODO quadro e a linha abaixo saia 20 a
  // 40 vezes por segundo: no D1, 3400 linhas a cada 5 min (ids 15821..15897,
  // "tt20285780 saiu de 0 para 0"; 15747, "475 para 475"), o que enchia os
  // 200 KB do registro e apagava todo o resto do log. O id guardado NAO e
  // trocado pelo da tela: entre o cat_item acima e a busca pode ter caido uma
  // republicacao, e adotar o id lido ali seria adotar outro titulo.
  if (novo == idx) return;
  printf("[detail] catalogo remontou: %s saiu de %d para %d\n",
         idxImdb, idx, novo);
  fflush(stdout);
  idx = novo;
}

// O foco esta numa fileira da PRIMEIRA tela? Na serie, temporadas e episodios
// ficam no heroi (mockup "detalhe-retomar"): a pagina fica no topo, com a arte.
static int focoNoTopo(void) { return 0; }

static float blocoAnt[3] = { -1.0f, -1.0f, -1.0f };   // Notas / Numeros / Progresso no quadro anterior

void detail_atualizar(float dt, Uint32 agora) {
  // `agora` ficou sem uso quando o repouso da troca de temporada saiu (ver a
  // nota mais abaixo). Fica na assinatura porque ela e a mesma de todas as
  // telas e app.c chama todas do mesmo jeito.
  (void)agora;
  episodios_menu_atualizar(dt);   // as molas do menu de visto / painel da temporada
  if (!aberto) { blocoAnt[0] = blocoAnt[1] = blocoAnt[2] = -1.0f; return; }
  // SAINDO: interrompe os dois fios antes mesmo de a mola terminar. Chamar todo
  // quadro nao custa nada (e um flag sob mutex) e evita precisar de uma borda:
  // `saindo` tambem e ligado por caminhos que nao passam pelo Voltar, como o
  // OK num estudio, que abre o vertudo e deixa esta tela para tras.
  if (saindo) { serieaud_fechar(); seriefrases_fechar(); trocaarte_fechar(); }
  if (colListaAberta) {
    // A linha em foco mira ~35% da altura, como o resto do app rola.
    float passo = COLL_LIN_H + COLL_LIN_GAP;
    int n = extras_n_colecao();
    float alvo = (float)colListaFoco * passo - (NV_TELA_H * 0.35f - COLL_TOPO);
    float maxY = (float)n * passo - (NV_TELA_H - COLL_TOPO - 60.0f);
    if (maxY < 0.0f) maxY = 0.0f;
    if (alvo > maxY) alvo = maxY;
    if (alvo < 0.0f) alvo = 0.0f;
    colListaScroll = anim_mola2(&colListaVel, colListaScroll, alvo, dt, NV_MOLA2_SCROLL);
  }
  revalidarIdx();
  trocaarte_atualizar(dt);
  // OK NA TELA DE ESCOLHA: a arte congelada na abertura (arteFixa/logoFixo)
  // e justamente a que a pessoa acabou de trocar. Solta e pede de novo — com a
  // escolha gravada, artehero ja devolve a nova.
  if (trocaarte_consumir_mudanca()) {
    arteFixa[0] = logoFixo[0] = logoCatalogoFixo[0] = 0;
    arteFixaPoster = 0;
    artehero_logo_sessao_iniciar(cat_item(idx));
  }
  // TRAILER AUTOMATICO, mudo, no lugar da arte (dono, 20/09/2026: "trailer
  // autoplay direto na interface"). Comeca NV_TRAILER_ESPERA_MS depois de a
  // pagina assentar, uma vez por abertura, e so enquanto a pagina esta no
  // topo com o heroi a mostra: rolar, abrir uma ficha, um menu ou sair fecha
  // o video e a arte volta. Em tela cheia (botao) a regra e outra: so o
  // teclado fecha.
  if (trailer_suportado()) {
    int topo = !saindo && nivel == 0 && !pessoaAberta && !episodios_menu_aberto() &&
               !trocaarte_aberto() &&
               !pedReproduzir && !pedFontes && !player_aberto() &&
               pg < 0.05f && scrollY < 1.0f &&
               // CARROSSEL (dono, 01/10: "coloque pra tocar dentro do card"):
               // o trailer toca no cartao, mas so com a tira parada no titulo
               // montado e a abertura assentada. O plano e de TELA CHEIA, como
               // a arte (GFX_JANELA: a arte nunca escala, o cartao e uma janela
               // sobre ela); o cartao e o furo arredondado (carFundo). Esticar
               // para a tela cheia so aumenta o furo: o plano nao se move.
               !(carro && (carAplicado != carPos || fabsf(carOff - (float)carPos) > 0.02f)) &&
               // Animacoes reduzidas: o cartao fica com a arte (sem video).
               !(carro && cartao > 0.01f && ajustes_animacoes_reduzidas());
    if (!detail_assentado() || !topo) { if (!trailer_cheia()) trailerDesde = 0; }
    else if (!trailerDesde) trailerDesde = agora;
    if (trailer_aberto() && !trailer_cheia() && !topo) trailer_fechar();
    if (saindo && trailer_aberto()) trailer_fechar();
    if (!trailer_aberto() && !trailerTentado && trailerDesde && topo &&
        agora - trailerDesde >= NV_TRAILER_ESPERA_MS &&
        ajustes_trailer_auto()) {
      int qual = 0;
      const char *u = trailerFonte(0, &qual, 0);
      if (u) {
        GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
        trailerTentado = 1;
        trailer_abrir(u, tela, ajustes_trailer_detalhe_som(), 0);
#ifdef __EMSCRIPTEN__
        // A etapa e a FONTE aberta (TRF_*), para o prazo saber qual e a
        // proxima na ordem do ajuste (trailerfonte_depois).
        trailerEtapa = qual;
        trailerPrazo = agora + NV_TRAILER_PREPARA_MS;
#endif
      }
    }
#ifdef __EMSCRIPTEN__
    // O PRAZO. Tocou: o prazo morre (buffering depois de `playing` nao e
    // falha). Nao tocou a tempo, ou o elemento deu erro (trailer_atualizar
    // ja fechou e marcou trailer_falhou): loga e passa para a PROXIMA fonte da
    // ordem do ajuste que tem trailer — em Automatico, Apple -> IMDb ->
    // YouTube (o erro 153 do embed chega como -3 e anda na hora, #136); com
    // uma fonte fixa nao ha proxima e fica a arte. Tela cheia nao entra aqui.
    // trailerEtapa: 0 nada, TRF_* a fonte aberta, -1 acabou.
    if (trailerEtapa > 0) {
      int venceu = trailer_aberto() && !trailer_cheia() && !trailer_tocando() &&
                   trailerPrazo && (Sint32)(agora - trailerPrazo) >= 0;
      int errou = !trailer_aberto() && trailer_falhou();
      if (trailer_aberto() && trailer_tocando()) trailerPrazo = 0;
      if ((venceu || errou) && topo) {
        // A PROXIMA DA ORDEM QUE TEM TRAILER: Apple -> IMDb -> YouTube na
        // Samsung (#136). Uma fonte da ordem sem trailer para o titulo e
        // pulada, nao encerra a fila.
        int prox = trailerfonte_depois(trailerfonte_ajuste(), trailerfonte_tizen(), trailerEtapa);
        const char *seg = NULL;
        while (prox && !(seg = trailerUrlDaFonte(prox)))
          prox = trailerfonte_depois(trailerfonte_ajuste(), trailerfonte_tizen(), prox);
        printf("[trailer] detalhe: %s %d ms (%s, estado %d), %s%s\n",
               errou ? "erro em" : "sem playing em", NV_TRAILER_PREPARA_MS,
               trailerfonte_nome(trailerEtapa), trailer_estado(),
               seg ? "tenta " : "fica a arte", seg ? trailerfonte_nome(prox) : "");
        fflush(stdout);
        if (trailer_aberto()) trailer_fechar();
        if (seg) {
          GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
          trailer_abrir(seg, tela, ajustes_trailer_detalhe_som(), 0);
          trailerEtapa = prox;
          trailerPrazo = agora + NV_TRAILER_PREPARA_MS;
        } else { trailerEtapa = -1; trailerPrazo = 0; }
      } else if (!trailer_aberto() && !errou) {
        trailerEtapa = 0; trailerPrazo = 0;   // fechou por fim ou por navegacao
      }
    }
#endif
    // trailer_mostra_video: no .tpk a arte fica ate o recorte do zoom
    // assentar (#178); na LG e no .wgt e sempre 1.
    { float alvo = (trailer_aberto() && trailer_tocando() && trailer_mostra_video()) ? 1.0f : 0.0f;
      // No cartao o bloco de texto fica: o modo cinema e da tela cheia.
      int toca = alvo > 0.5f && !trailer_cheia() && !(carro && cartao > 0.01f);
      trailerFade = anim_mola(trailerFade, alvo, dt, NV_MOLA_SCROLL);
      trailercinema_passo(&trailerCinema, toca, dt, ajustes_animacoes_reduzidas()); }
  }
  // O CATALOGO TROCOU: OS EPISODIOS FORAM JUNTO, E NINGUEM OS REPEDIA.
  //
  // cat_definir_tudo zera as faixas de episodio de proposito — os indices
  // mudaram e uma faixa antiga apontaria para outro titulo. Mas quem estava com
  // uma serie ABERTA perdia a secao inteira, e nada a reconstruia: o pedido so
  // sai em app.c quando a pagina abre.
  //
  // MEDIDO na C9: "Os Aspones" publica os 7 episodios aos 13,4 s
  // (`[desc] ... 7 episodios publicados`), o ciclo de descoberta termina com
  // `[desc] catalogo montado com 292 titulos`, e dali em diante a secao some —
  // o D-pad pula de "Temporadas" direto para as abas, porque secao com zero
  // colunas e intransponivel (focus.c). Para quem esta olhando, a pagina de uma
  // serie simplesmente nao tem onde ver os episodios.
  //
  // Repedir e barato: a meta ja esta no cache de disco (metaCacheObter), entao
  // isto nao volta a rede. E desc_episodios sai sozinho se a faixa ja existir.
  { const CatItem *ci = cat_item(idx);
    if (ci && ehSerie() != extrasSerie) {
      extrasSerie = ehSerie();
      extras_pedir(ci->imdb, extrasSerie, ci->tmdb);
    } }
  // TAMBEM QUANDO SO A GERACAO DAS FAIXAS ANDOU (cat_geracao_episodios): a
  // volta do vetor de episodios apaga a lista sem subir a revisao. E na
  // abertura (epGerConferido = 0), sem esperar alguem republicar.
  { unsigned rev = cat_revisao(), eg = cat_geracao_episodios();
    if (rev != revistaVista || eg != epGerVista || !epGerConferido) {
      revistaVista = rev; epGerVista = eg; epGerConferido = 1;
      if (ehSerie() && cat_n_episodios(idx) < 1 && !desc_episodios_carregando(idx)) {
        const CatItem *ci = cat_item(idx);
        desc_episodios(idx, ci ? ci->temporada : 0);
      }
    } }
  sincronizarColunas();
  // Solta o pedido de episodios que ficou guardado por ter chegado com outro
  // carregamento em voo.
  desc_episodios_pendente();
  auditoriaEpisodios();

  // TROCA DE TEMPORADA PELO MOVIMENTO DO FOCO, nao pelo OK.
  //
  // A fileira de temporadas e um SELETOR na referencia: andar com o direcional
  // ja troca a lista de episodios. Aqui a troca so acontecia dentro do OK, e o
  // dono, passando pelas pilulas, via a lista NAO mudar — o que ele descreveu
  // como "demora para atualizar quando troca de temporada". Nao demorava: nao
  // acontecia.
  //
  // Com REPOUSO, pela mesma razao do heroi (NV_HERO_REPOUSO_MS): varrer quatro
  // temporadas de ponta a ponta dispararia quatro consultas das quais so a
  // ultima interessa. Espera o foco parar e so entao troca.
  // SEM REPOUSO, e o repouso foi embora junto com a razao dele. Ele existia
  // para nao disparar quatro consultas ao varrer quatro abas de ponta a ponta;
  // hoje trocar de aba nao consulta nada, so muda quais episodios a fileira
  // mostra. Esperar meio segundo para trocar uma VISTA e a propria "demora ao
  // trocar de temporada" que o repouso tentava evitar.
  if (nivel >= 1 && foco.fileira == SEC_TEMPORADAS) {
    if (foco.coluna != temporada) {
      temporada = foco.coluna;
      irParaTemporada(temporada, 0);
    }
  } else if (foco.fileira == SEC_EPISODIOS) {
    // Andar pelos episodios move a ancora junto: voltando para as abas, a
    // fileira nao pula de volta para o episodio de onde a aba a deixou.
    epAncora = foco.coluna;
    { const CatEp *ep = cat_episodio(idx, epAbsoluto(foco.coluna));
      if (ep) { comEpT = ep->temporada; comEpE = ep->episodio; } }
  }

  // "Seu progresso" entra pela coluna da temporada escolhida na pagina.
  if (ehSerie() && foco.fileira != SEC_PROGTEMP) {
    int c = tgraf_coluna(tgraf_dados(idx), temporadaEm(temporada));
    if (c >= 0) foco.colunaLembrada[SEC_PROGTEMP] = c;
  }

  // SELETOR DE COMENTARIOS: a fonte troca no OK, NAO ao passar o foco.
  //
  // Trocava ao passar (#78, Owlphibia: "the TV Show option not being able to
  // scroll through"): as pilulas e os cartoes sao UMA fileira, entao chegar
  // aos cartoes da serie obrigava a atravessar a pilula "Episódio" — que ja
  // trocava a fonte no caminho. Os cartoes da serie eram inalcancaveis. Pior:
  // `comentEp = foco.coluna` tambem corria nos cartoes (coluna 2, 3...) e
  // qualquer valor nao-zero le como "episodio".
  //
  // O pedido do episodio continua saindo a cada quadro em que a fonte e a do
  // episodio: e barato (o modulo sai na hora quando ja tem aquele T/E) e segue
  // o episodio em foco na fileira la de cima sem borda de "mudou".
  if (nivel >= 1 && foco.fileira == SEC_COMENTARIOS && ehSerie()) {
    if (comentEp) {
      const CatItem *ci = cat_item(idx);
      int t = 0, ep = 0;
      if (ci && episodioDosComentarios(&t, &ep) && t > 0 && ep > 0)
        extras_pedir_comentarios_ep(ci->imdb, t, ep);
    }
  }

  // OS DOIS PEDIDOS SOB DEMANDA. Ver a nota em audAberta: saem quando o FOCO
  // ENTRA na secao, nunca na abertura da pagina.
  //
  // Repetir e barato de proposito — os dois modulos saem na hora quando ja
  // estao no mesmo titulo (e na mesma temporada, no caso da audiencia) —, entao
  // chamar por quadro enquanto o foco esta aqui e a forma mais simples de nao
  // precisar de uma borda de "entrou agora" que erra quando o catalogo remonta.
  if (nivel >= 1 && foco.fileira == SEC_NUMEROS) {
    // AS NOTAS JA ESTAO NA MAO. Vem do mesmo `seasons?extended=episodes,full`
    // que desenha as pastilhas da aba "Avaliações"; so a retencao custa pedido.
    abrirAudiencia();
    // O episodio em foco nos tres cartoes e a COLUNA focada.
    serieaud_selecionar(foco.coluna);
  }
  // TROCAR DE TEMPORADA LA EM CIMA MEXE EM DUAS COISAS AQUI EMBAIXO.
  //
  // 1. FECHA a audiencia de volta para a chamada. O modulo guarda uma temporada
  //    por vez; sem isto as bandas continuariam desenhando os graficos da
  //    temporada ANTERIOR sob o titulo da nova — um grafico com a cara certa e
  //    os numeros de outra coisa, que e o defeito que serieaud.h manda evitar
  //    acima de todos.
  //
  // 2. LEVA JUNTO a grade de pastilhas da aba "Avaliações" (`ratTemp`). Ela tem
  //    um seletor de temporada PROPRIO, que nascia sempre na primeira e nunca
  //    ouvia as pilulas do topo da pagina. Isso passou despercebido enquanto
  //    ela era a unica coisa da aba; com os graficos logo abaixo dela viraram
  //    duas leituras da mesma temporada, lado a lado, discordando — visivel na
  //    captura como "T1" aceso na grade e "E1..E13 da T2" no arco. As pilulas do
  //    topo sao a escolha da PAGINA; quem estiver dentro da aba continua livre
  //    para espiar outra temporada com esquerda/direita.
  //
  // E A CONCILIACAO NAO PODE DEPENDER SO DA TROCA. O `agoraT != audTempVista`
  // dispara uma vez, no primeiro quadro da pagina — e nesse instante a lista do
  // Trakt quase sempre AINDA NAO CHEGOU (extras_n_temporadas() = 0), entao
  // audTemp() devolve -1, ratTemp fica em 0 e, como o numero da temporada nao
  // muda mais, a conciliacao nunca mais roda. Com uma serie cuja lista do Trakt
  // comeca em "Especiais" (temporada 0) — ou que simplesmente nao tem a T1 —, o
  // indice 0 aponta para outra temporada, e a grade volta a discordar das
  // pilulas exatamente como antes da correcao acima. A captura escondia isso
  // porque o duble de extras responde no quadro zero; a rede nao.
  //
  // `ratSinc` marca que a conciliacao chegou a acontecer com dado na mao. Quem
  // entra na aba e anda com esquerda/direita continua livre: aquilo escreve
  // ratTemp sem mexer nesta marca, e ela so e rearmada quando a PAGINA troca de
  // temporada.
  if (ehSerie()) {
    int agoraT = temporadaEm(temporada);
    if (agoraT != audTempVista) {
      audTempVista = agoraT;
      if (audTempAberta != agoraT) audAberta = 0;
      ratSinc = 0;
    }
    if (!ratSinc) {
      int t = audTemp();
      if (t >= 0) { ratTemp = t; ratSinc = 1; }
    }
  }
  // No filme a linha Colecao | Frases aparece com a pagina rolando ate ela:
  // a consulta sai quando o foco chega a "Mais como este" ou abaixo.
  if (nivel >= 1 && (foco.fileira == SEC_FRASES ||
      (!ehSerie() && (foco.fileira == SEC_RELACIONADOS || foco.fileira == SEC_COLECAO ||
                      foco.fileira == SEC_DETALHES || foco.fileira == SEC_ESTUDIOS)))) {
    const CatItem *ci = cat_item(idx);
    if (ci && ci->imdb[0]) { seriefrases_abrir(ci->imdb); frasesAberta = 1; }
  }

  // DEPOIS de sincronizarColunas, nao antes: o empilhamento pergunta a secaoN
  // quem tem conteudo, e secaoN olha dados que chegam da rede. Recalcular com a
  // contagem do quadro anterior deixaria o layout um quadro atrasado — visivel
  // como um tranco quando o elenco ou os trailers chegam.
  recalcularLayout();
  t  = anim_mola(t,  saindo ? 0.0f : 1.0f, dt, NV_MOLA_TELA);
  // Rigidez propria: o web leva 0.8s para apagar o backdrop (cubic-bezier
  // .4,0,.2,1), e a mola de NV_MOLA_TELA assenta em ~330ms.
  // Temporadas e episodios da serie moram na PRIMEIRA tela (Glass UI): com o
  // foco neles a pagina nao rola e a arte nao apaga.
  // A VOLTA ao topo (de Notas, Elenco...) reacende a arte no dobro da
  // velocidade: enquanto a pagina e a arte cheia se cruzam, a TV pinta quase
  // cinco telas por quadro e cai para 40 FPS (medido na TCL, 05/10). A ida
  // continua com o tempo do web.
  { float alvoPg = nivel >= 1 && !focoNoTopo() ? 1.0f : 0.0f;
    pg = anim_mola(pg, alvoPg, dt, alvoPg < pg ? NV_MOLA_PAGINA * 2.0f : NV_MOLA_PAGINA); }
  if (carro) {
    int k;
    carOff = anim_mola2(&carVel, carOff, (float)carPos, dt, CAR_MOLA);
    cartao = anim_mola2(&cartaoVel, cartao, (nivel >= 1 || carCheia) ? 0.0f : 1.0f, dt, CAR_MOLA_PAG);
    carTxt = anim_mola2(&carTxtVel, carTxt, (nivel >= 1 || carCheia) ? 0.0f : 1.0f, dt, CAR_MOLA_PAG);
    if (ajustes_animacoes_reduzidas()) { carTxt = (nivel >= 1 || carCheia) ? 0.0f : 1.0f; carTxtVel = 0.0f; }
    // Monta a pagina do titulo novo quando a tira esta chegando: o texto dele
    // entra enquanto o cartao assenta, e nao depois.
    if (carAplicado != carPos && fabsf(carOff - (float)carPos) < 0.25f && !saindo)
      carAplicar();
    // ARTE DOS VIZINHOS PRE-CARREGADA: dois para cada lado, para o passo
    // seguinte ja encontrar a foto na tira. So o titulo em cena e URGENTE: os
    // vizinhos eram pedidos urgentes tambem, e o hero de quem chegava entrava
    // em FIFO atras deles (a chegada o promove e ele fura a fila).
    for (k = carPos - 2; k <= carPos + 2; k++) {
      const char *a = (k >= 0 && k < carN) ? arteDe(carIdx[k]) : NULL;
      if (a) (void)(k == carPos ? tex_obter_hero(a) : tex_obter_hero_quente(a));
    }
    if (carAplicado == carPos && !saindo && !carCheia) carPreBuscar();
    // VOLTA: a fileira recebe o titulo em cena e o cartao encolhe ate o cartaz
    // dele, que a home volta a desenhar (detail_cobre_tela = 0 saindo).
    if (saindo) {
      HomeItem hi;
      if (!carFocou) { home_focar_titulo(carIdx[carPos]); carFocou = 1; carEsperaRect = 3; }
      else if (carEsperaRect > 0) carEsperaRect--;
      if (home_item_focado(&hi) && hi.rect.w > 8.0f && hi.rect.h > 8.0f) carOrigem = hi.rect;
    }
  }
  if (saindo && t < 0.02f) {
    // O fio do TMDB pode trocar ou limpar o logo no catalogo enquanto o detalhe
    // mostra logoFixo; ao voltar, o hero lia o catalogo novo (vazio ou FALHOU)
    // e caia no nome escrito. Restaura o caminho que funcionou na abertura.
    if (logoCatalogoFixo[0] && idxImdb[0]) {
      int i = cat_indice_por_imdb(idxImdb);
      if (i >= 0) {
        const CatItem *c = cat_item(i);
        int ruim = !c || !c->logo[0];
        if (!ruim) {
          const char *u = artehero_url_logo(c->logo);
          ruim = !u || tex_falhou(u);
        }
        if (ruim && c) {
          CatItem e = *c;
          snprintf(e.logo, sizeof e.logo, "%s", logoCatalogoFixo);
          cat_atualizar_item(i, &e);
        }
      }
    }
    aberto = 0; saindo = 0; t = 0.0f; carro = 0; trailer_fechar(); return;
  }

  // SEGURAR OK SEM RESPOSTA NA TELA (dono, 06/10): na home o cartao ganha a
  // barra "Segure para opcoes"; aqui o menu do episodio, o da temporada e as
  // fontes do botao principal so abriam ao soltar, sem nada mostrando que o
  // gesto estava contando. A barra mora na ilha do relogio (ilha_atividade):
  // a pagina nao tem um retangulo de foco unico, e a ilha e o lugar do 2.0
  // para estado em andamento. Os primeiros 150 ms nao contam: e um toque.
  if (okDesceEm && !ctx_aberto()) {
    Uint32 dur = agora - okDesceEm;
    int alvoHold = (nivel >= 1 && (foco.fileira == SEC_EPISODIOS || foco.fileira == SEC_TEMPORADAS)) ||
                   (nivel == 0 && !focoAmigos && acaoEm(botao) == ACAO_PRIMARIO);
    if (alvoHold && dur >= 150u) {
      float p = (float)(dur - 150u) / (float)(NV_HOLD_MS - 150);
      if (p > 1.0f) p = 1.0f;
      ilha_atividade(i18n(p >= 1.0f ? "Solte para abrir opções" : "Segure para opções"), p);
    }
  }

  // SEGUROU OK NUMA LISTA ATE O LIMIAR: o menu do cartaz, com o dedo ainda no
  // botao. okDesceEm zera: o KEYUP vai ao menu, nao a pagina.
  if (!ctx_aberto() && ctxhold_passo(&holdLista, agora, 1) && menuNaLista()) {
    ctxhold_cancelar(&holdLista);
    okDesceEm = 0;
  }

  for (int r = 0; r < N_SECOES; r++)
    for (int c = 0; c < secaoN(r) && c < N_ITENS; c++) {
      float alvo = (nivel >= 1 && focus_indice(&foco, r, c)) ? 1.0f : 0.0f;
      animFoco[r][c] = anim_mola(animFoco[r][c], alvo, dt,
                                 alvo > animFoco[r][c] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
    }

  // --- rolagem HORIZONTAL da fileira focada ---------------------------------
  // Duas regras, as duas do fonte do web (`getHorizontalTrackScrollLeft`): a
  // fileira de episodios ENCOSTA o card focado na margem esquerda; as demais so
  // rolam o necessario, com 24px de folga nas bordas.
  { int r = foco.fileira;
    if (r >= 0 && r < N_SECOES && secaoN(r) > 0) {
      float x = xItem(r, foco.coluna) - NV_DETP_X;
      float w = larguraItem(r, foco.coluna);
      float vista = NV_TELA_W - NV_DETP_X * 2;
      float alvo = scrollSec[r];
      if (r == SEC_EPISODIOS) alvo = x;
      else {
        // Pilula focada: a fileira volta ao inicio. As pilulas nao rolam junto
        // com os cartoes (elas ficam numa linha propria, fixa), entao deixar o
        // scroll de um cartao antigo pendurado esconderia o primeiro cartao
        // assim que o foco subisse para o seletor.
        if (r == SEC_COMENTARIOS && foco.coluna < nPilulasCom()) alvo = 0.0f;
        // As duas secoes de painel NUNCA rolam na horizontal: a coluna e uma
        // posicao DENTRO de um desenho de largura fixa, nao um item que possa
        // sair da tela. Sem esta linha a regra geral abaixo empurrava a secao
        // inteira 24 px para a esquerda assim que o foco saia da coluna 0.
        else if (EH_AUD(r) || r == SEC_FRASES || r == SEC_NOTAS || r == SEC_NOTAS_EP ||
                 r == SEC_PROGTEMP) alvo = 0.0f;
        else if (foco.coluna == 0) alvo = 0.0f;
        else if (x + w > alvo + vista - 24.0f) alvo = x + w - vista + 24.0f;
        else if (x < alvo + 24.0f)             alvo = x - 24.0f;
      }
      if (alvo < 0.0f) alvo = 0.0f;
      scrollSec[r] = anim_mola2(&velSec[r], scrollSec[r], alvo, dt, NV_MOLA2_SCROLL);
    }
    // A fileira de episodios rola ATRAS da aba de temporada mesmo sem o foco:
    // e o que da ao seletor a resposta visual que ele perdeu ao deixar de
    // arrastar o foco junto.
    if (r != SEC_EPISODIOS && secaoN(SEC_EPISODIOS) > 0 &&
        epAncora < foco.nColunas[SEC_EPISODIOS]) {
      float ax = xItem(SEC_EPISODIOS, epAncora) - NV_DETP_X;
      if (ax < 0.0f) ax = 0.0f;
      scrollSec[SEC_EPISODIOS] = anim_mola2(&velSec[SEC_EPISODIOS],
                                            scrollSec[SEC_EPISODIOS], ax, dt,
                                            NV_MOLA2_SCROLL);
    } }

  // --- rolagem VERTICAL -----------------------------------------------------
  // O topo do grupo focado vai para 33% da altura util (40% nas abas). E a
  // regra do web, e nao um "rola o necessario": conferida nos quatro grupos.
  // NOTAS CHEGANDO TARDE (rede) nao pode empurrar o que o foco esta olhando:
  // a secao nasce ACIMA de quase tudo, entao tudo abaixo dela desce o bloco
  // inteiro. Se o foco esta abaixo, a rolagem anda junto (a fileira focada fica
  // onde estava na tela) em vez de a mola arrastar a pagina depois.
  // O MESMO vale para os NUMEROS DA TEMPORADA (serie), que nascem logo abaixo
  // das Notas quando a lista de episodios do Trakt chega.
  { static const int BLOCOS[3] = { SEC_NOTAS, SEC_NUMEROS, SEC_PROGTEMP };
    int kb;
    for (kb = 0; kb < 3; kb++) {
      int sec = BLOCOS[kb];
      float bloco = secaoN(sec) > 0
          ? conteudoSec[sec] - topoSec[sec] + alturaSecao(sec) + NV_DETF_SEC_GAP
          : 0.0f;
      if (blocoAnt[kb] >= 0.0f && bloco != blocoAnt[kb] && nivel >= 1) {
        const int *ordem = ordemSecoes();
        int o, depois = 0, visto = 0;
        for (o = 0; o < N_ORDEM; o++) {
          if (ordem[o] == sec) visto = 1;
          else if (ordem[o] == foco.fileira) { depois = visto; break; }
        }
        if (depois) scrollY += bloco - blocoAnt[kb];
      }
      blocoAnt[kb] = bloco;
    } }
  float alvoY = 0.0f;
  if (nivel >= 1 && foco.fileira >= 0 && foco.fileira < N_SECOES && !focoNoTopo()) {
    // Mira o topo do CONTEUDO (o trilho), nao o do grupo: o cabecalho da secao
    // fica acima e entra na tela junto, de graca. E o que focusInList faz no
    // web — `target.closest(".movie-cast-track, ...")`.
    float maxY = docFim - NV_TELA_H;
    alvoY = conteudoSec[foco.fileira] - NV_TELA_H * alvoSec[foco.fileira];
    // AS FRASES SAO UMA LISTA ALTA DENTRO DE UMA FILEIRA SO, e a rolagem desta
    // pagina so sabe mirar o TOPO da fileira focada. Com seis citacoes o painel
    // passa da altura da tela e as ultimas ficariam abaixo da dobra, escolhidas
    // pelo D-pad e invisiveis — o mesmo defeito que obrigou a audiencia a virar
    // tres fileiras, que aqui nao da para resolver do mesmo jeito porque as
    // citacoes sao uma lista, nao tres blocos com nome proprio.
    //
    // Entao a fileira PANORAMIZA: o alvo desliza do topo ate o fim do painel
    // conforme a escolha desce. Proporcional, e nao por citacao, porque a
    // altura de cada uma depende de quantas linhas a fala ocupou e o unico
    // numero que o modulo devolve e o TOTAL.
    if (foco.fileira == SEC_FRASES && seriefrases_n() > 1) {
      float sobra = frasesAlt - NV_TELA_H * 0.62f;
      if (sobra > 0.0f)
        alvoY += sobra * (float)seriefrases_selecionado()
                       / (float)(seriefrases_n() - 1);
    }
    if (alvoY > maxY) alvoY = maxY;
    if (alvoY < 0.0f) alvoY = 0.0f;
  }
  scrollY = anim_mola2(&velY, scrollY, alvoY, dt, NV_MOLA2_SCROLL);
}

// ---------------------------------------------------------------------------
// HERO
// ---------------------------------------------------------------------------
// Nada aqui e sobreposicao num cartao: a tela e full-bleed, a coluna comeca em
// x=72 e a pilha e ancorada na BASE (`.detail-hero-section` e um flex column
// com `justify-content: flex-end`). Empilhar de cima para baixo faz o bloco
// inteiro subir e descer conforme o tamanho da sinopse; no web ele fica preso
// na base e so o topo se move.






// COR DE FOCO dos botoes do hero: a cor de realce dos Ajustes, e a tinta do
// texto/glifo por cima dela — escura sobre realce claro, branca sobre realce
// escuro (luminancia Rec.709, o mesmo degrau das pilulas de comentario). O
// dono (20/09/2026): "nenhum botao ta ficando com a cor do accent; o texto
// tem que ser branco ou preto dependendo da cor". Os botoes estavam cravados
// em #f5f5f5/#111, a regra de layout.h (foco = preenchimento na cor de
// realce) nao chegava aqui.
static float tintaBotao;   // tinta do rotulo do primario, decidida junto do fundo
static float focoAcento(float *r, float *g, float *b) {
  return ajustes_acento_tinta(r, g, b);
}

// BRILHO DIFUSO POR TRAS DO BOTAO EM FOCO: a luz da pilula do menu lateral
// (21/09/2026) — GFX_SOMBRA na cor de realce com 0,9x a altura de folga e
// alpha 0,35. So o botao focado a tem, entao e uma mancha por tela; para o
// Play sao ~0,07 tela de preenchimento, longe de mudar a conta da pagina.
static void luzFoco(GfxRect r, float a) {
  // A luz da tabela de botoes (botoes.h), para ser a MESMA do menu do cartaz
  // e dos cartoes — o Play nao tem luz propria.
  botao_luz(r, 1.0f, a);
}

static void desenhaBotao(GfxRect r, const char *rot, int icone, int focado, float a) {
  // FOCO E TAMANHO, NAO ANEL.
  //
  // Aqui havia duas marcas de foco empilhadas e as duas falhavam no botao
  // primario: primeiro um gfx_cor 8px maior — que PREENCHE, nao contorna — e
  // logo depois o GFX_ANEL. Num botao que ja e branco a pilula preenchida se
  // funde com ele e o resultado e um botao branco 8px maior, que nao le como
  // "selecionado" e sim como "o botao mudou de forma". Era o defeito relatado.
  //
  // O aparelho resolve por escala: o item focado cresce com o centro parado, os
  // fatores estao em NV_DETW2_FOCO_*, e nao ha anel nenhum em captura alguma.
  // No circular a escala vem acompanhada da inversao de cor, que ja existia.
  int circular = (rot == NULL);
  if (focado) {
    float sx = circular ? NV_DETW2_FOCO_SY : NV_DETW2_FOCO_SX;
    float sy = NV_DETW2_FOCO_SY;
    float cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
    r.w *= sx; r.h *= sy;
    r.x = cx - r.w * 0.5f; r.y = cy - r.h * 0.5f;
    foco_anel(r, NV_RAIO_PILL, 1.0f, a);   // so com "Foco no cartaz" ligado
  }
  if (circular) {
    float ic;
    // A PELE E A DA TABELA (botoes.h, 21/09/2026): repouso 0.14/0.15/0.17 e
    // nao #222, foco realce + tinta + luz. O tamanho (escala no foco) e o
    // glifo continuam daqui.
    if (ajustes_vidro()) {
      // Vidro: o mesmo disco de botao_disco (branco cheio em foco, glifo escuro).
      gfx_vidro_painel(r, NV_RAIO_PILL, 0.55f, a);
      gfx_vidro_pilula_cheia(r, NV_RAIO_PILL, focado ? 1.0f : 0.0f, a);
      ic = focado ? (float)gfx_vidro_tinta(1.0f) / 255.0f : 1.0f;
    } else
    if (focado) { float fr, fg, fb; ic = focoAcento(&fr, &fg, &fb);
                  luzFoco(r, a);
                  gfx_cor(r, NV_RAIO_PILL, fr, fg, fb, a); }
    else { ic = 1.0f; gfx_cor(r, NV_RAIO_PILL, 0.14f, 0.15f, 0.17f, a); }
    // Os tres glifos do web: biblioteca (+), assistido (olho) e trailer
    // (placa do YouTube). Sao SVG la e nao existem na familia embarcada, entao
    // vem do shader — ver GFX_OLHO e GFX_FONTES. Antes eram "+" e dois "...",
    // que nao diziam o que os botoes faziam.
    float cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
    // ICONES DE VERDADE, do art/icones (SVG do app web rasterizados). Antes
    // cada glifo era desenhado a mao no shader — um "+" de dois retangulos, um
    // olho de dois discos, tres barras — e cada um era uma aproximacao do
    // original. Agora e o arquivo, e a cor vem daqui pelo GFX_MARCA.
    //
    // Proporcao glifo/circulo MEDIDA no aparelho: o "+" mede 32 dentro do
    // circulo de 96 em repouso e 36 dentro do de 110 focado — 0,333 nos dois.
    // Estava 0,45, de uma captura solta, e o glifo quase encostava na borda.
    // Sai de `r` (ja escalado) para que o icone cresca junto com o botao.
    float g = r.w * NV_DETW2_CIRC_GLIFO;
    GfxRect ig = { cx - g * 0.5f, cy - g * 0.5f, g, g };
    if (icone == 1) {
      // O botao de lista troca o + por check quando o titulo ja esta salvo;
      // olho fica reservado ao controle separado de assistido logo ao lado.
      // O estado vem de ci->naLista, atualizado pelo mesmo fluxo do botao.
      const CatItem *ci = cat_item(idx);
      gfx_icone(ig, (ci && ci->naLista) ? "check" : "mais", ic, ic, ic, a);
    } else if (icone == 2) {
      // ASSISTIDO: olho aberto quando ja viu, olho riscado quando nao. Antes o
      // icone era sempre o mesmo e nao dizia estado nenhum — era so um enfeite
      // que o dono nao conseguia ler ("avisar o que foi visto").
      // O ESTADO E O DE cat_visto (#212), o mesmo do selo do cartaz: antes era
      // so `progresso >= 90`, e filme visto em outro aparelho (ou marcado pelo
      // menu do cartaz, que zera o progresso) ficava com o olho riscado.
      gfx_icone(ig, cat_visto(cat_item(idx)) ? "visto" : "naovisto", ic, ic, ic, a);
    } else if (icone == ACAO_MAIS) {
      // RETICENCIAS do Lucide: o "tem mais aqui" de qualquer interface.
      gfx_icone(ig, "aj_ellipsis", ic, ic, ic, a);
    } else {
      gfx_icone(ig, "fontes", ic, ic, ic, a);
    }
    return;
  }
  // Primario: branco com texto preto nos DOIS estados. Conferido nas duas
  // capturas do aparelho — o miolo mede (255,255,255) focado e em repouso, e o
  // que muda entre eles e so o tamanho (321x94 -> 357,6x107,8), ja aplicado em
  // `r` la em cima.
  // PILULA BRANCA LIMPA. Aqui o botao INTEIRO era a barra de progresso: a parte
  // que faltava assistir recebia um veu preto a 30%, recortado no ponto do
  // progresso. A intencao era boa e o recorte estava certo, mas o resultado
  // lia como BOTAO DESABILITADO — uma pilula branca com dois tercos apagados
  // parece controle inativo, nao "16% assistido". Foi o que o dono viu: "esse
  // retomar ta muito feio, nem parece o mesmo app".
  //
  // A referencia nao faz isso: o botao e branco limpo e o progresso vive na
  // LINHA DE TEXTO logo acima, que este arquivo ja desenha ("Retomada
  // disponivel  16%  Episodio T2E1"). O veu era redundante alem de feio —
  // dizia com tinta o que a linha ja diz com palavra.
  // Em repouso branco com tinta preta (a referencia); em FOCO a cor de realce
  // com a tinta que contrasta com ela (focoAcento).
  // A PELE E A DA TABELA (botoes.h, 21/09/2026): o primario em repouso e a
  // superficie 0.14/0.15/0.17 com texto 235, e nao a pilula branca da
  // referencia web — branco em repouso e branco em foco (tema padrao) eram
  // o mesmo botao em dois tamanhos, e o Play brigava com o botao em foco.
  { float tinta = 235.0f / 255.0f, fr = 0.14f, fg = 0.15f, fb = 0.17f;
    if (ajustes_vidro()) {
      // Play: vidro com lavagem e aro do realce em repouso; em foco, a pilula
      // cheia na cor do realce (branca no tema padrao).
      gfx_vidro_painel_acento(r, NV_RAIO_PILL, 0.55f, a);
      gfx_vidro_pilula_cheia(r, NV_RAIO_PILL, focado ? 1.0f : 0.0f, a);
      tinta = focado ? (float)gfx_vidro_tinta(1.0f) / 255.0f : tinta;
    } else {
    if (focado) { tinta = focoAcento(&fr, &fg, &fb); luzFoco(r, a); }
    gfx_cor(r, NV_RAIO_PILL, fr, fg, fb, a); }
    tintaBotao = tinta; }

  // Triangulo 28x30 e vao de 21 ate a tinta do rotulo, medidos no aparelho
  // (x=150..177 e rotulo em 200, dentro da pilula 96..417).
  //
  // O grupo icone+rotulo e CENTRADO na pilula em vez de ancorado no padding
  // esquerdo: focada, a pilula cresce e o rotulo nao — SDL_ttf rasteriza num
  // corpo fixo e nao ha estilo de 28pt na tabela de text.c, que e arquivo de
  // outro agente. Ancorado a esquerda, o texto ficaria visivelmente fora de
  // centro no estado focado; centrado, a folga sobra igual dos dois lados.
  { float s = r.h / NV_DETW2_BTN_H;
    float iw = NV_DETW2_BTN_ICONE_W * s, ih = NV_DETW2_BTN_ICONE_H * s;
    int c = (int)(tintaBotao * 255.0f + 0.5f);
    TxtLinha l = txt_linha(TXT_DET_BOTAO, rot, c, c, c, 255);
    float grupo = iw + NV_DETW2_BTN_GAPI * s + l.w;
    float x = r.x + (r.w - grupo) * 0.5f;
    GfxRect tri = { x, r.y + (r.h - ih) * 0.5f, iw, ih };
    gfx_rect(tri, 0, GFX_PLAY, 0, 0, 0, 0.0f, tintaBotao, tintaBotao, tintaBotao, a);
    txt_desenhar_alpha(l, x + iw + NV_DETW2_BTN_GAPI * s,
                       r.y + (r.h - l.h) * 0.5f, a); }
}

// DICA DO CIRCULAR EM FOCO (so o "Assistir trailer"): o circular e mudo, e uma
// acao que abre video nao pode ser adivinhada pelo glifo. Pilula escura com o
// nome logo acima do botao, no mesmo vocabulario da legenda do carrossel.
static void desenhaDicaBotao(GfxRect rc, const char *texto, float a) {
  float ar, ag, ab;
  TxtLinha label = txt_linha_corta(TXT_CAPTION2, texto, 232, 235, 240, 255,
                                   NV_DETW2_TEXTO_W - 28.0f);
  float w = label.w + 28.0f, h = 34.0f;
  GfxRect pill = { rc.x + (rc.w - w) * 0.5f, rc.y - h - 14.0f, w, h };
  if (pill.x < NV_DETW2_X) pill.x = NV_DETW2_X;
  ajustes_acento(&ar, &ag, &ab);
  gfx_cor(pill, .5f, ar, ag, ab, .16f * a);
  gfx_cor((GfxRect){ pill.x + 1, pill.y + 1, pill.w - 2, pill.h - 2 }, .5f,
          .075f, .08f, .095f, .76f * a);
  txt_desenhar_alpha(label, pill.x + 14.0f, pill.y + (h - label.h) * .5f, a);
}

// Botao secundario: 345x96, raio 64, fundo #222 e texto branco; focado, fundo
// #f5f5f5 e texto #111, com o mesmo anel de 4px. Nao tem icone — no web e so o
// rotulo, e por isso a largura sai de `texto + 2 x 34` e nao da conta do
// primario.
static void desenhaSecundario(GfxRect r, const char *rot, int focado, float a) {
  // A PELE DO SECUNDARIO DA TABELA (botoes.h): contorno de 1,5 px a 22 % em
  // repouso, realce + tinta + luz em foco. A largura e a altura continuam
  // as medidas desta tela.
  if (focado) foco_anel(r, NV_RAIO_PILL, 1.0f, a);   // so com "Foco no cartaz" ligado
  botao_pilula(r, rot, NULL, focado ? 1.0f : 0.0f, 0, 0, a);
}

// Largura do botao primario: padding 54 + icone 28 + vao 21 + texto. A conta e
// a mesma do aparelho e fecha na medida: com "Assistir T1:E1" (tinta 162) da
// 319 contra os 321 lidos na captura. Um rotulo maior cresce a pilula em vez de
// estourar por baixo do texto.
static float larguraPrimario(const char *rot) {
  TxtLinha l = txt_linha(TXT_DET_BOTAO, rot, 0, 0, 0, 255);
  return NV_DETW2_BTN_PADX * 2 + NV_DETW2_BTN_ICONE_W + NV_DETW2_BTN_GAPI + l.w;
}
static float larguraSecundario(const char *rot) {
  TxtLinha l = txt_linha(TXT_DET_BOTAO, rot, 255, 255, 255, 255);
  return NV_DETW_BTN2_PADX * 2 + l.w;
}

// O BOTAO DO LEMBRETE — so o relogio, e verde quando esta armado.
//
// Era uma pilula com o despertador e o rotulo "Lembrar-me" / "Lembrete ativo".
// O dono olhou a captura e pediu o contrario: "aqui ser so o relogio e quando
// tiver ativo ele ficar um verdinho bonito". Entao ele passou a ser um CIRCULO
// do mesmo diametro dos vizinhos (96, NV_DETW2_CIRC) — a linha de acoes tem uma
// gramatica so, e um botao de forma propria ali le como peca de outro app.
//
// TRES ESTADOS DE SUPERFICIE, e nenhum deles depende so de cor:
//   desligado, em repouso  circulo #222 e relogio claro, parado, sem ondas
//   LIGADO,    em repouso  circulo ESMERALDA e relogio escuro, tremendo, com
//                          as ondas de som ao lado
//   em foco                circulo claro preenchido e relogio escuro, a mesma
//                          regra dos outros circulares (ver desenhaBotao: o
//                          aparelho marca foco por ESCALA mais inversao de cor,
//                          e nao por anel)
// Quem nao distingue verde de cinza le o estado pelas ONDAS e pelo tremor, que
// so existem com o lembrete ligado. Numa TV a tres metros essa redundancia nao
// e formalidade: o verde e o cinza tem quase a mesma luminancia em quem enxerga
// pouca cor, e ai a unica diferenca que resta e a forma.
//
// O VERDE E O ESMERALDA #66bb6a DA PALETA DE ACENTOS, nao um verde novo — a
// mesma regra que o selo do Social ja segue. Ver agendaui_cor_lembrete: sobre a
// superficie clara do foco ele entra escurecido, porque cheio daria 2,2:1 e
// sumiria.
//
// O NOME DO BOTAO. Este app nao tem canal de leitura de tela (nao ha TTS, nao
// ha voice guidance), e TODOS os circulares dele — mais, assistido, fontes,
// recomendar — sao mudos: foi o que se achou procurando "como os outros icones
// se nomeiam". Em vez de repetir isso, o nome saiu na LINHA DE ESTADO logo
// acima da fileira, que com o botao em foco passa a dizer o nome e o estado.
// E o unico lugar com espaco medido: a grade do hero vem do aparelho e uma
// legenda sob o circulo encostaria na linha de apoio a 4px.
// O GLIFO USA A MEDIDA DA FILEIRA, 0,333, e nao mais um 0,42 proprio.
//
// O 0,42 existia porque o despertador antigo, so de contorno, parecia menor que
// o "+" cheio ao lado. A conta mostrou que a compensacao andava para o lado
// errado: com o icone velho (4762 px de tinta no arquivo de 128, contra 2923 da
// nuvem e 3582 do aviao), 0,42 punha 472 px2 de tinta dentro do circulo de 96,
// contra 182 da nuvem e 223 do aviao. O botao nao estava pequeno, estava
// gritando. Com o desenho novo (4054 px, ver lembrete.svg) o mesmo 0,333 dos
// vizinhos da 253 px2 — dentro da faixa deles, que vai de 118 ("+") a 316
// (olho). Um numero a menos, e o que sobrou e o que a fileira ja usava.
static void desenhaLembrete(GfxRect r, int ligado, int focado, float a) {
  float cr, cg, cb, g;
  int claro = focado || ligado;
  if (focado) {
    float cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
    r.w *= NV_DETW2_FOCO_SY; r.h *= NV_DETW2_FOCO_SY;
    r.x = cx - r.w * 0.5f; r.y = cy - r.h * 0.5f;
  }
  if (focado) {
    // Cor de realce, como os vizinhos. `claro` passa a dizer se a SUPERFICIE
    // e clara — e disso que agendaui_cor_lembrete escolhe o verde escurecido
    // ou o cheio; sem lembrete armado o glifo e a tinta de contraste.
    float fr, fg, fb, t = focoAcento(&fr, &fg, &fb);
    luzFoco(r, a);
    gfx_cor(r, NV_RAIO_PILL, fr, fg, fb, a);
    claro = t < 0.5f;
    // Glifo na tinta que contrasta com o realce, armado ou nao: o estado vai
    // pelas ondas e pelo tremor, nao pela cor.
    cr = cg = cb = t; goto glifo;
  } else if (ligado) {
    // ARMADO SEM FOCO: disco CHEIO na cor de realce, sem contorno, glifo na
    // tinta de contraste — igual ao foco, so que no tamanho de repouso. O
    // anel branco fino que esteve aqui um dia "ficou feio" (dono, 21/09/2026),
    // e o disco verde anterior nao conversava com nenhuma cor da tela. As
    // ondas e o tremor do despertador continuam dizendo o estado.
    float fr, fg, fb, t = focoAcento(&fr, &fg, &fb);
    gfx_cor(r, NV_RAIO_PILL, fr, fg, fb, a);
    cr = cg = cb = t;
    goto glifo;
  } else gfx_cor(r, NV_RAIO_PILL, 0.133f, 0.133f, 0.133f, a);
  agendaui_cor_lembrete(ligado, claro, &cr, &cg, &cb);
glifo:
  g = r.w * NV_DETW2_CIRC_GLIFO;
  { GfxRect ic = { r.x + (r.w - g) * 0.5f, r.y + (r.h - g) * 0.5f, g, g };
    agendaui_despertador(ic, ligado, cr, cg, cb, a, SDL_GetTicks(), lembreteEm); }
}

// Ano solto do campo `meta` ("2025 · 1 h 54 min" -> "2025" e "1 h 54 min").
static void partirMeta(const char *meta, char *ano, size_t na, char *resto, size_t nr) {
  ano[0] = 0; resto[0] = 0;
  if (!meta || !meta[0]) return;
  const char *sep = strstr(meta, "\xc2\xb7");        // U+00B7
  if (!sep) { snprintf(ano, na, "%s", meta); return; }
  size_t n = (size_t)(sep - meta);
  while (n && (meta[n-1] == ' ')) n--;
  if (n >= na) n = na - 1;
  memcpy(ano, meta, n); ano[n] = 0;
  const char *r = sep + 2;
  while (*r == ' ') r++;
  snprintf(resto, nr, "%s", r);
}

// Selo do IMDb: 60x30, raio 4, amarelo #f6c700 com "IMDb" preto dentro; a nota
// vem 8px depois, em rgb(179,179,179) — a MESMA cor do resto da linha, e nao
// branca. Medido nas duas capturas do aparelho (selo em x=628..687 na serie e
// 714..773 no filme, sempre y=938..967).
//
// NAO e o 109x60 que este arquivo trazia do web: la o logo tem 60 de ALTURA e
// aqui o selo inteiro tem 30. Com o valor do web o selo ficava do dobro do
// tamanho da linha em que vive.
//
// A marca continua DESENHADA (retangulo + texto) e nao rasterizada do SVG: o
// app nao empacota SVG e o arquivo nao entra sem reinstalar o ipk. E o unico
// ponto do selo que nao e 1:1.
//
// A nota sai com VIRGULA decimal ("7,8"), como na referencia — o "%.1f" do C
// escreve ponto e a linha inteira e em portugues.
static float desenhaSeloImdb(float x, float yCentro, int nota, float a) {
  if (nota <= 0) return 0.0f;
  return badge_imdb(x, yCentro - BADGE_H * 0.5f, nota, 0, a);
}

// Selo de CONTORNO da segunda linha de meta. Ele carrega DUAS coisas dentro da
// mesma caixa — classificacao indicativa e status da producao —, separadas por
// uma barra vertical: "TV-MA | RENOVADA" na serie, "R | LANÇADO" no filme. A
// classificacao sai em rgb(179,179,179) e o status em BRANCO, o que faz o olho
// ler primeiro o que interessa.
//
// Antes eram duas coisas soltas na linha e o contorno era falsificado com um
// retangulo cheio 1px maior por baixo — que so funciona sobre fundo chapado.
// Aqui e GFX_ANEL, que desenha contorno de verdade e deixa a arte aparecer no
// miolo, como no aparelho.
//
// `dir` pode ser NULL: sem status o selo tem so a classificacao e NENHUMA
// divisoria. Nao ha valor de reserva — o CatItem nao tem campo de status, e
// carimbar "LANÇADO" em tudo seria dado inventado, que ja custou caro aqui.
static float desenhaSeloMeta(float x, float y, const char *esq, const char *dir,
                             float a) {
  if (!dir || !dir[0]) {
    // Classificacao isolada e um badge, nao um contorno de outra altura. O
    // caso composto (classificacao + status) conserva a divisoria propria.
    return badge_desenhar(x, y + (NV_DETW2_SELO_H - BADGE_H) * 0.5f, esq,
                          BADGE_NEUTRO, a);
  }
  TxtLinha le = txt_linha(TXT_DET_META2, esq, 179, 179, 179, 255);
  TxtLinha ld = { 0, 0, 0 };
  float w = NV_DETW2_SELO_PADX * 2 + le.w;
  if (dir && dir[0]) {
    ld = txt_linha(TXT_DET_META2, dir, 255, 255, 255, 255);
    w += NV_DETW2_DIV_PAD * 2 + NV_DETW2_DIV_W + ld.w;
  }
  GfxRect caixa = { x, y, w, NV_DETW2_SELO_H };
  gfx_rect(caixa, 0, GFX_ANEL, 0, NV_DETW2_SELO_BORDA / NV_DETW2_SELO_H, 0,
           NV_DETW2_SELO_R / NV_DETW2_SELO_H, 0.42f, 0.42f, 0.42f, a);
  float cx = x + NV_DETW2_SELO_PADX;
  float cy = y + NV_DETW2_SELO_H * 0.5f;
  txt_desenhar_alpha(le, cx, cy - le.h * 0.5f, a);
  cx += le.w;
  if (dir && dir[0]) {
    GfxRect bar = { cx + NV_DETW2_DIV_PAD, cy - NV_DETW2_DIV_H * 0.5f,
                    NV_DETW2_DIV_W, NV_DETW2_DIV_H };
    gfx_cor(bar, 0.0f, 0.42f, 0.42f, 0.42f, a);
    cx += NV_DETW2_DIV_PAD * 2 + NV_DETW2_DIV_W;
    txt_desenhar_alpha(ld, cx, cy - ld.h * 0.5f, a);
  }
  return w;
}

// Ponto separador: disco de 6, e nao a barrinha de 1x14 do web. Sao dois usos
// com a MESMA forma e cores diferentes, e a diferenca de cor e o que agrupa a
// linha: entre generos ele e rgb(179,179,179) (a cor do proprio texto, porque
// ali ele e um "•" da frase) e entre GRUPOS e rgb(128,128,128), mais apagado.
static void desenhaPonto(float x, float yCentro, float lum, float a) {
  GfxRect pt = { x, yCentro - NV_DETW2_PONTO_D * 0.5f,
                 NV_DETW2_PONTO_D, NV_DETW2_PONTO_D };
  gfx_cor(pt, 0.5f, lum, lum, lum, a);
}

// O LOGO SOZINHO, no canto inferior esquerdo, como o outro app faz enquanto o
// trailer toca: metade do tamanho do logo do heroi, base a 96 px do fundo.
static void logoCinema(float a) {
  const char *arqLogo = logoDe(idx);
  GLuint texLogo = arqLogo ? tex_obter_logo_larg_qualquer(arqLogo, NV_DETW_LOGO_MAXW) : 0;
  float baseY = trailercinema_base();
  if (a <= 0.005f || ajustes_esconder_logo_trailer()) return;
  if (texLogo) {
    float asp = tex_aspecto(arqLogo);
    float h, w;
    trailercinema_logo(asp, &w, &h);
    gfx_tex_aspect_atual = 0.0f;
    { GfxModo m = tex_marca_escura(arqLogo) ? GFX_MARCA : GFX_TEXTO;
      gfx_rect((GfxRect){ NV_DETW2_X, baseY - h, w, h }, texLogo, m, 0, 0, 0, 0.0f, 1, 1, 1, a); }
  } else {
    const CatItem *ci = cat_item(idx);
    const char *nome = ci ? ci->titulo : NULL;
    if (nome && nome[0]) {
      TxtLinha t2 = txt_linha_corta(TXT_TITULO2, nome, 255, 255, 255, 255, NV_DETW_LOGO_MAXW * 0.5f);
      txt_desenhar_alpha(t2, NV_DETW2_X, baseY - t2.h, a);
    }
  }
}

// --- TEXTO INTEIRO NA ABERTURA (#172) ----------------------------------------
//
// COMO A PAGINA ABRIA: detail_abrir zera `t`; o fundo sobe com suave(t) e o
// bloco de texto (heroWeb) com fase2() = suave((t-0.45)/0.55), ou seja, so a
// partir da metade da animacao. So que heroWeb pedia as linhas AQUI, no quadro
// em que ja eram desenhadas: o rasterizador (text.c) tem orcamento por quadro,
// e o que estourava voltava vazio e entrava num quadro depois. Resultado: o
// bloco subia com linhas faltando e o resto ia aparecendo palavra a palavra.
//
// Agora (a) o bloco e DESENHADO desde o primeiro quadro com opacidade quase nula,
// o que rasteriza as linhas enquanto a animacao roda; (b) o portao (textogate.h)
// so revela quando nenhuma linha do bloco ficou pendente — ou em 400 ms, o que
// vier primeiro — e revela TUDO junto, num esvanecimento de 180 ms.
static TextoGate gateHero;
// Sinopse: o que foi desenhado por ultimo, para esvanecer entre o antigo e o
// novo (ingles -> localizado, item raso -> completo) em vez de trocar de uma vez.
static char   sinVisto[900], sinAnt[900];
static int    sinVistoInit;
static Uint32 sinTrocaDesde;
static float  hSinVis;            // altura reservada, suavizada
static int    hSinInit;
static Uint32 hSinTick;
static int    metaEsqVisto;       // o esqueleto da linha de meta foi mostrado
static Uint32 metaChegouEm;

static void heroReiniciar(void) {
  textogate_reiniciar(&gateHero);
  notasui_reiniciar();
  serieaud_bloco_reiniciar();
  tgraf_reiniciar();
  sinVisto[0] = sinAnt[0] = 0; sinVistoInit = 0; sinTrocaDesde = 0;
  hSinVis = 0.0f; hSinInit = 0; hSinTick = 0;
  metaEsqVisto = 0; metaChegouEm = 0;
}

#define DET_SIN_ESQ_LINHAS   3      // linhas que o esqueleto da sinopse reserva
#define DET_TROCA_MS      200.0f
// Barras arredondadas no lugar de um texto que ainda nao chegou. Tom discreto
// (mesma familia do esqueleto das secoes) e o mesmo gfx_esqueleto do resto do
// app, com a luz passando.
static void esqueletoTexto(float x, float y, float larg, float h, float a) {
  if (a <= 0.005f) return;
  gfx_esqueleto((GfxRect){ x, y, larg, h }, 0.5f, 0.30f, 0.30f, 0.33f, 0.50f * a);
}
static void esqueletoSinopse(float x, float y, float a) {
  static const float frac[DET_SIN_ESQ_LINHAS] = { 1.0f, 0.94f, 0.58f };
  for (int i = 0; i < DET_SIN_ESQ_LINHAS; i++)
    esqueletoTexto(x, y + i * NV_DETW2_LD_SIN + 8.0f,
                   NV_DETW2_TEXTO_W * frac[i], 18.0f, a);
}

// O bloco do heroi anda junto com o cartao do carrossel (heroDx; 0 fora dele).
// Todo x do bloco nasce de NV_DETW2_X, entao deslocar a coluna e deslocar o
// bloco inteiro — sem tocar nas medidas do resto da pagina.
#pragma push_macro("NV_DETW2_X")
#undef NV_DETW2_X
#define NV_DETW2_X (96.0f + heroDx)
// Unknown language is not evidence of a mismatch. Keep a loaded logo clean;
// only a confirmed foreign image needs a caption. Missing images keep text.
static int mostrarNomeLogo(const CatItem *ci, const char *url, int loaded,
                           const char *language) {
  if (!url || !url[0]) return !loaded;
  if (!ci || !url || !language || !ci->logoIdiomaUrl[0] ||
      !ci->logoIdioma[0] || !strcmp(ci->logoIdioma, "und")) return 0;
  const char *actual = strrchr(url, '/');
  const char *known = strrchr(ci->logoIdiomaUrl, '/');
  // TMDB size variants share the same image path; other hosts do not.
  int same = !strcmp(url, ci->logoIdiomaUrl) ||
    (!strncmp(url, "https://image.tmdb.org/t/p/", 27) &&
     !strncmp(ci->logoIdiomaUrl, "https://image.tmdb.org/t/p/", 27) &&
     actual && known && !strcmp(actual, known));
  return same && strncmp(ci->logoIdioma, language, 2) != 0;
}
static void heroWeb(float a, float desloc) {
  if (a <= 0.005f) return;
  const CatItem *ci = cat_item(idx);

  char ano[32], dur[64];
  partirMeta(fichaDe(idx), ano, sizeof ano, dur, sizeof dur);
  { char cru[64]; snprintf(cru, sizeof cru, "%s", dur);
    desc_duracao_txt(cru, dur, sizeof dur); }   // "142 min" -> forma do idioma da UI

  const char *sin = sinopseDe(idx);

  // --- ORDEM DA COLUNA, como a referencia do dono -----------------------------
  //
  // De cima para baixo: logo, linha de meta (ano, temporadas, classificacao),
  // generos, quem dirigiu, sinopse, linha de retomada e por fim os BOTOES.
  //
  // Antes os botoes vinham logo abaixo do logo e generos/classificacao caiam no
  // rodape, o que separava a informacao do titulo em dois blocos com a acao no
  // meio. Na referencia tudo que DESCREVE o titulo vem junto e a acao fecha o
  // bloco — foi isso que o dono pediu ao comparar as duas telas.
  //
  // Continua ancorado na BASE: a sinopse muda de altura conforme o texto, e
  // ancorar no topo faria o botao dancar de titulo para titulo.
  // A ACAO VEM LOGO ABAIXO DO LOGO, e todo o texto que DESCREVE o titulo vem
  // junto, embaixo dela.
  //
  // Estava ao contrario: as acoes fechavam o bloco, com ~500 px de texto acima
  // delas — o unico alvo interativo da tela era o mais distante do topo. MEDIDO
  // na referencia (TCL, 1920x1080): logo 311..473, ACOES 509..608, apoio
  // 640..685, sinopse 700..900, meta 938..970, classificacao/pais 1003..1045.
  //
  // Isto NAO reintroduz o defeito que motivou a ordem antiga. Aquele era a
  // informacao PARTIDA EM DOIS — parte acima da acao, parte no rodape. Aqui a
  // acao sobe e o texto desce INTEIRO, num bloco so. As proprias constantes
  // deste arquivo ja descreviam esta ordem (NV_DETW_GAP_ACOES e literalmente
  // "acoes -> Diretor:"); o codigo e que tinha derivado do token sheet.
  //
  // Continua empilhando DE BAIXO PARA CIMA e ancorado na base: a sinopse muda
  // de altura com o texto, e ancorar no topo faria o bloco inteiro dancar de
  // titulo para titulo. Isso ficou CONFIRMADO no aparelho: entre a serie (5
  // linhas de sinopse) e o filme (4) as duas linhas de meta caem exatamente nos
  // mesmos y (938 e 999) e a ULTIMA linha de sinopse tambem — o que cresce para
  // cima e o resto da pilha.
  //
  // A GRADE NOVA, medida na TCL: acoes 512..606, apoio 649..675, sinopse em
  // passo de 40 terminando com a tinta em 890, meta 1 (generos/data/IMDb)
  // 938..968 e meta 2 (selo de contorno + pais) 999..1048.
  //
  // A LINHA DE GENEROS SOLTA DEIXOU DE EXISTIR: no aparelho os generos abrem a
  // primeira linha de meta, e ano e nota vem depois deles, separados por ponto.
  // Aqui eram duas linhas — uma so com generos, outra com ano e duracao — e o
  // selo do IMDb ficava sozinho encostado na borda direita da tela, a meio
  // metro do bloco de texto a que pertence.
  float temRetom = (ci && ci->progresso > 0) ? 1.0f : 0.0f;
  float hSin = 0.0f;
  if (sin) hSin = txt_bloco(TXT_DET_SIN, sin, 255, 255, 255, -1.0f, 0.0f,
                            NV_DETW2_TEXTO_W, NV_DETW2_LD_SIN, 0.0f,
                            NV_DETW2_SIN_LINHAS);
  // DADO A CAMINHO: sinopse e meta chegam da rede depois que a pagina abre.
  // Enquanto o pedido esta em voo desenha-se o ESQUELETO do texto no lugar e
  // RESERVA-SE a altura dele; quando o texto chega ele esvanece por cima e a
  // altura assenta suave — sem isso o logo e os botoes davam um salto do tamanho
  // da sinopse. Teto de 6 s apos a revelacao: um pedido que nunca volta nao
  // deixa a pagina pulsando para sempre.
  Uint32 agoraH = SDL_GetTicks();
  int gateAberto = textogate_aberto(&gateHero);
  // (conta tambem com o portao fechado: a altura ja nasce reservada, senao ela
  // cresceria de 0 durante a propria revelacao)
  int chegando = (!gateAberto || (Uint32)(agoraH - gateHero.pronto) < 6000u) &&
                 (desc_episodios_carregando(idx) || extras_carregando());
  int esqSin = !sin && chegando;
  { // troca de texto -> esvanece
    const char *atual = sin ? sin : "";
    if (sinVistoInit && strcmp(atual, sinVisto) != 0 && gateAberto &&
        !anim_politica_reduzida) {
      snprintf(sinAnt, sizeof sinAnt, "%s", sinVisto);
      sinTrocaDesde = agoraH ? agoraH : 1u;
    }
    if (!sinVistoInit || strcmp(atual, sinVisto) != 0)
      snprintf(sinVisto, sizeof sinVisto, "%s", atual);
    sinVistoInit = 1;
  }
  { float alvo = sin ? hSin : (esqSin ? DET_SIN_ESQ_LINHAS * NV_DETW2_LD_SIN : 0.0f);
    float dt = hSinTick ? (float)(Uint32)(agoraH - hSinTick) : 0.0f;
    hSinTick = agoraH;
    if (dt > 100.0f) dt = 100.0f;
    if (!hSinInit || anim_politica_reduzida || !gateAberto) hSinVis = alvo;
    else {
      hSinVis += (alvo - hSinVis) * (1.0f - expf(-dt / 90.0f));
      if (fabsf(alvo - hSinVis) < 0.5f) hSinVis = alvo;
    }
    hSinInit = 1;
    hSin = hSinVis; }
  const char *arqLogo = logoDe(idx);
  const char *nome = mostrarNomeLogo(ci, arqLogo && tex_falhou(arqLogo) ? NULL : arqLogo,
                                    0, desc_tmdb_idioma()) ? tituloDe(idx) : NULL;
  int nomeAbaixo = nome && arqLogo && !tex_falhou(arqLogo);
  float hNome = nome ? txt_bloco(TXT_DET_META2, nome, 255, 255, 255,
                                -1, 0, NV_DETW_LOGO_MAXW, 34, 0, 0) : 0;
  float hCaption = nomeAbaixo ? hNome + 16.0f : 0;
  float yMeta2 = NV_DETW2_BASE - NV_DETW2_SELO_H;
  float yMeta1 = yMeta2 - NV_DETW2_META_GAP - NV_DETW2_M1_H;
  float ySin   = yMeta1 - NV_DETW2_GAP_SIN - hSin;
  AmigosTitulo amg;
  int temAmg = amigosDoTitulo(&amg);
  if (!temAmg) focoAmigos = 0;
  float hAmg = temAmg ? NV_DETW_AMIGOS_H + NV_DETW_AMIGOS_GAP : 0.0f;
  float yAcoes = ySin - NV_DETW2_GAP_ACOES - NV_DETW2_BTN_H - hCaption - hAmg;
  // BLOCO DE LINHAS DE ESTADO, empilhado de baixo para cima logo acima dos
  // botoes. Sao duas, e a ordem tem razao: a retomada explica o BOTAO e fica
  // colada nele; a agenda ("Próximo episódio T2E5 · em 3 dias") explica o
  // TITULO e fica acima dela.
  //
  // A agenda so ocupa altura quando existe frase. Serie sem registro ainda, ou
  // registro sem data e sem situacao conhecida, nao empurra nada — a linha
  // some, e o hero fica exatamente como era.
  char agLinha[200] = "";
  { const CatItem *ciAg = cat_item(idx);
    if (ehSerie() && ciAg && ciAg->imdb[0])
      agenda_frase(ciAg->imdb, agLinha, sizeof agLinha); }
  float yEstado = yAcoes - NV_DETW_GAP_RETOM;
  float yRetom = yEstado, yAgenda = yEstado;
  if (temRetom > 0.0f) { yEstado -= NV_DETW_RETOM_H; yRetom = yEstado; }
  if (agLinha[0]) { yEstado -= carro ? 46.0f : NV_DETW_RETOM_H; yAgenda = yEstado; }

  // Sobe alguns pixels enquanto entra: continua o movimento da arte em vez de
  // aparecer pronto no lugar. `desloc` e a rolagem do documento.
  float sobe = (1.0f - a) * 26.0f + desloc;
  yMeta2 += sobe; yMeta1 += sobe; ySin += sobe;
  yRetom += sobe; yAgenda += sobe; yAcoes += sobe; yEstado += sobe;

  // --- logo -----------------------------------------------------------------
  // O logo e desenhado com 261 de largura mas a arte de origem costuma vir bem
  // maior; o teto de 960 ja bastaria, mas quando a mesma arte tambem serve ao
  // hero o item e promovido — por isso passa pelo mesmo caminho.
  // tex_obter_larg com a largura que o logo OCUPA, e nao tex_obter (teto
  // generico): com a URL unica de artehero_url_logo_larg o card aberto ja
  // decodificou este mesmo arquivo em ~370 px, e o cache so entrega uma
  // textura menor enquanto reprocessa se ela tiver ao menos metade do
  // pedido. Pedindo o teto, o logo sumia por um instante ao abrir o titulo;
  // pedindo a largura real, aparece na hora e troca pela nitida em seguida.
  GLuint texLogo = arqLogo ? tex_obter_logo_larg_qualquer(arqLogo, NV_DETW_LOGO_MAXW) : 0;
  float baseLogo = ((temRetom > 0.0f || agLinha[0]) ? yEstado : yAcoes)
                   - NV_DETW_LOGO_GAP;
  float espacoLogo = baseLogo - 110.0f - (!nomeAbaixo ? hNome : 0);
  if (espacoLogo < 0) espacoLogo = 0;
  static char  logoVisto[600];
  static Uint32 logoDesde;
  float aLogo = a;
  if (texLogo) {
    if (!arqLogo || strcmp(logoVisto, arqLogo)) {
      snprintf(logoVisto, sizeof logoVisto, "%s", arqLogo ? arqLogo : "");
      logoDesde = SDL_GetTicks();
    }
    { float f = (float)(SDL_GetTicks() - logoDesde) / 260.0f;
      if (f < 1.0f) aLogo *= f < 0.0f ? 0.0f : f; }
  } else logoVisto[0] = 0;
  if (texLogo) {
    float asp = tex_aspecto(arqLogo);
    if (asp <= 0.0f) asp = 2.5f;
    // ALTURA ASSENTA SUAVE: o espaco do logo depende do que esta empilhado
    // embaixo (sinopse, amigos, retomar, agenda). Quando uma dessas linhas
    // aparecia ou sumia, o logo trocava de tamanho num quadro ("cresce do
    // nada", dono, 05/10). Agora persegue o alvo como a altura da sinopse.
    static float hLogoVis;
    static Uint32 hLogoTick;
    static char hLogoDe[600];
    float hAlvo = fminf(NV_DETW_LOGO_H, espacoLogo);
    { Uint32 agoraL = SDL_GetTicks();
      float dtL = hLogoTick ? (float)(Uint32)(agoraL - hLogoTick) : 0.0f;
      hLogoTick = agoraL;
      if (dtL > 100.0f) dtL = 100.0f;
      if (strcmp(hLogoDe, arqLogo) || hLogoVis <= 0.0f || anim_politica_reduzida) hLogoVis = hAlvo;
      else {
        hLogoVis += (hAlvo - hLogoVis) * (1.0f - expf(-dtL / 90.0f));
        if (fabsf(hAlvo - hLogoVis) < 0.5f) hLogoVis = hAlvo;
      }
      snprintf(hLogoDe, sizeof hLogoDe, "%s", arqLogo); }
    float h = hLogoVis, w = h * asp;
    if (w > NV_DETW_LOGO_MAXW) { w = NV_DETW_LOGO_MAXW; h = w / asp; }
    GfxRect r = { NV_DETW2_X, baseLogo - h, w, h };
    gfx_tex_aspect_atual = 0.0f;   // o logo ja vem na proporcao certa
    // LOGO PRETO VIRA BRANCO. O TMDB serve a mesma marca em versao clara e
    // escura e NAO diz qual e qual — nao ha campo para isso, e o ranking do
    // proprio app web ordena so por idioma e nota. Quando cai a escura, ela
    // aparece preta sobre um backdrop escuro e o titulo some da tela: foi o que
    // aconteceu com "The Invite".
    //
    // A decisao e por MEDIDA, nao por regra fixa: tex_luminancia devolve a
    // media dos pixels opacos, calculada uma vez na thread de decode. So a arte
    // realmente escura e tingida; logo claro ou COLORIDO (o dourado, o
    // vermelho) passa intacto pelo GFX_TEXTO, porque chapa-lo de branco seria
    // trocar um defeito por outro.
    //
    // -1 = ainda carregando: trata como clara e nao tinge. Errar para o lado de
    // nao mexer na arte e o certo enquanto nao se sabe.
    { GfxModo m = tex_marca_escura(arqLogo) ? GFX_MARCA : GFX_TEXTO;
      if (r.w > 0 && r.h > 0)
        gfx_rect(r, texLogo, m, 0, 0, 0, 0.0f, 1, 1, 1, aLogo); }
  } else if (nome && !nomeAbaixo) {
    txt_bloco(TXT_DET_META2, nome, 255, 255, 255, NV_DETW2_X,
              baseLogo - fminf(NV_DETW_LOGO_H, espacoLogo) - hNome,
              NV_DETW_LOGO_MAXW, 34, a, 0);
  }

  // --- botoes ---------------------------------------------------------------
  // Em FLUXO, com 24px entre vizinhos — nao os 63 que vinham do web. MEDIDO no
  // aparelho: pilula 96..417, circulos com centro em 488,5 e 608,5 (passo 120,
  // diametro 96), o que da 23,5 e 24 de vao. Os circulos sao 96 e nao 84, e
  // ficam 1px mais altos que a pilula (511..606 contra 512..606), o que na
  // pratica e o mesmo centro vertical — e assim que ficam alinhados aqui.
  //
  // Dois estados do rotulo, medidos: "Retomar T2E3" quando ha progresso,
  // "Reproduzir" quando nao ha. (O web tem um terceiro, "Proximo T2E4", que sai
  // do proximo episodio nao assistido — o catalogo nativo nao guarda quais
  // episodios ja foram vistos, entao esse estado nao tem de onde vir.)
  // TRES estados, como o web: "Retomar TxEy" (em andamento), "Próximo TxEy"
  // (primeiro nao assistido) e "Reproduzir" (nunca aberto). O terceiro estado
  // era dado como impossivel aqui; e possivel desde que extras_ep_visto existe.
  char rot[48];
  { int t = 0, e = 0, de = 0;
    if (ehSerie() && episodioAlvo(&t, &e, &de) && t > 0 && e > 0 && de >= 2)
      // i18n NO FORMATO E NA PALAVRA. A frase montada nunca casa com uma chave
      // (ver idioma.h), entao traduzir so no desenho deixava "Retomar T1E1" em
      // portugues com a interface em ingles — o issue #12. O T/E tambem muda:
      // em ingles a abreviacao e S/E.
      snprintf(rot, sizeof rot, i18n("%s T%dE%d"),
               i18n(de == 2 ? "Retomar" : "Próximo"), t, e);
    else snprintf(rot, sizeof rot, "%s",
                  det_rotulo_primario(ci ? ci->progresso : 0, ajustes_cw_concluido())); }

  float larguraAcoes = 0;
  { float cyBtn = yAcoes + NV_DETW2_BTN_H * 0.5f;
    if (acoesAgrupadas()) {
      int grupo = 1 + (temInicio() ? 1 : 0);
      maisAcoes = nivel == 0 && botao >= grupo;
    }
    int nb = 0, n = nBotoes();
    // Trocar de titulo com o foco no ultimo circular de um FILME e cair numa
    // serie deixaria `botao` = 3 numa linha de 3 botoes: nenhum apareceria
    // focado e o OK nao acharia acao. Fixa aqui, no desenho, que e por onde
    // todo quadro passa.
    if (botao >= n) botao = n - 1;
    float bx = NV_DETW2_X;
    GfxRect rp = { bx, yAcoes, larguraPrimario(rot), NV_DETW2_BTN_H };
    desenhaBotao(rp, rot, 0, nivel == 0 && botao == nb, a);
    if (a > 0.3f) ponteiro_alvo(rp.x, rp.y, rp.w, rp.h, ponteiroDetalhe, NULL, -1, nb);
    bx += rp.w + NV_DETW2_BTN_GAP; nb++;
    if (temInicio()) {
      // "Assistir do comeco" entre o primario e os circulares (issue #46).
      const char *rotIni = i18n("Assistir do começo");
      int selIni = nivel == 0 && botao == nb;
      static float inicioExp;
      static Uint32 inicioTick;
      static int inicioIdx = -1;
      Uint32 tick = SDL_GetTicks();
      float dtIni = inicioTick ? (float)(Uint32)(tick - inicioTick) : 0;
      if (dtIni > 80) dtIni = 80;
      if (inicioIdx != idx) { inicioExp = 0; inicioIdx = idx; }
      inicioTick = tick;
      float alvoIni = selIni ? 1.0f : 0.0f;
      inicioExp = ajustes_animacoes_reduzidas() ? alvoIni :
        inicioExp + (alvoIni-inicioExp)*(1.0f-expf(-dtIni/65.0f));
      float fechado = acoesAgrupadas() ? NV_DETW2_CIRC : larguraSecundario(rotIni);
      float abertoIni = larguraSecundario(rotIni) + 44.0f;
      GfxRect rs = { bx, cyBtn-fechado*.5f,
        acoesAgrupadas() ? fechado+(abertoIni-fechado)*inicioExp : fechado,
        acoesAgrupadas() ? NV_DETW2_CIRC : NV_DETW2_BTN_H };
      rs.y = cyBtn - rs.h*.5f;
      if (acoesAgrupadas()) {
        botao_pilula(rs, "", NULL, selIni ? 1.0f : 0.0f, 0, 0, a);
        float tinta = selIni ? ajustes_acento_tinta(NULL,NULL,NULL) : 235.0f/255.0f;
        gfx_icone((GfxRect){rs.x+22,rs.y+(rs.h-28)*.5f,28,28},
                  "aj_rotate-ccw-clock", tinta,tinta,tinta,a);
        if (rs.w > fechado+10) {
          int cor = (int)(tinta*255);
          TxtLinha label = txt_linha_corta(TXT_DET_BOTAO,rotIni,cor,cor,cor,255,rs.w-86);
          txt_desenhar_alpha(label,rs.x+64,rs.y+(rs.h-label.h)*.5f,a*inicioExp);
        }
      } else desenhaSecundario(rs, rotIni, selIni, a);
      if (a > 0.3f) ponteiro_alvo(rs.x, rs.y, rs.w, rs.h, ponteiroDetalhe, NULL, -1, nb);
      bx += rs.w + NV_DETW2_BTN_GAP; nb++;
    }
    larguraAcoes = larguraPrimario(rot);
    if (temLembrar() && !acoesAgrupadas()) {
      const CatItem *ciL = cat_item(idx);
      GfxRect rs = { bx, cyBtn - NV_DETW2_CIRC * 0.5f,
                     NV_DETW2_CIRC, NV_DETW2_CIRC };
      desenhaLembrete(rs, ciL && agenda_lembrete(ciL->imdb),
                      nivel == 0 && botao == nb, a);
      if (a > 0.3f) ponteiro_alvo(rs.x, rs.y, rs.w, rs.h, ponteiroDetalhe, NULL, -1, nb);
      bx += NV_DETW2_CIRC + NV_DETW2_BTN_GAP; nb++;
    }
    if (acoesAgrupadas()) {
      static float reveal, revealVelocity;
      static Uint32 revealTick;
      static int revealIdx = -1;
      Uint32 now = SDL_GetTicks();
      float elapsed = revealTick ? (float)(Uint32)(now-revealTick) : 0;
      if (elapsed > 80) elapsed = 80;
      if (revealIdx != idx) { reveal = revealVelocity = 0; revealIdx = idx; }
      revealTick = now;
      reveal = ajustes_animacoes_reduzidas() ? (float)maisAcoes :
        anim_mola2(&revealVelocity,reveal,maisAcoes ? 1.0f : 0.0f,elapsed*.001f,16.0f);
      float progress = anim_clamp(reveal,0,1);
      int first = nb;
      GfxRect anchor = {bx,cyBtn-NV_DETW2_CIRC*.5f,NV_DETW2_CIRC,NV_DETW2_CIRC};
      // Draw outgoing circles behind Add, then keep the anchor in front.
      for (int j=nBotoesTodos()-1; j>first; j--) {
        float stagger = anim_clamp((progress-(j-first-1)*.045f)/.78f,0,1);
        if (stagger < .005f) continue;
        GfxRect rc = {bx+(j-first)*(NV_DETW2_CIRC+NV_DETW2_BTN_GAP)*stagger,
          anchor.y,NV_DETW2_CIRC,NV_DETW2_CIRC};
        int action = acaoEm(j), selected = nivel == 0 && botao == j;
        if (action == ACAO_LEMBRAR)
          desenhaLembrete(rc,ci && agenda_lembrete(ci->imdb),selected,a*stagger);
        else desenhaBotao(rc,NULL,action,selected,a*stagger);
        if (action == ACAO_MAIS) maisAncora = rc;
        if (selected && action == ACAO_MAIS && stagger > .85f && !detmais_aberto())
          desenhaDicaBotao(rc, i18n("Mais opções"), a);
        if (maisAcoes && a > .3f && stagger > .85f)
          ponteiro_alvo(rc.x,rc.y,rc.w,rc.h,ponteiroDetalhe,NULL,-1,j);
      }
      desenhaBotao(anchor,NULL,ACAO_LISTA,nivel == 0 && botao == first,a);
      if (a > .3f) ponteiro_alvo(anchor.x,anchor.y,anchor.w,anchor.h,ponteiroDetalhe,NULL,-1,first);
    } else for (; nb < n; nb++) {
      GfxRect rc = { bx, cyBtn - NV_DETW2_CIRC * 0.5f,
                     NV_DETW2_CIRC, NV_DETW2_CIRC };
      desenhaBotao(rc, NULL, acaoEm(nb), nivel == 0 && botao == nb, a);
      if (acaoEm(nb) == ACAO_MAIS) maisAncora = rc;
      if (nivel == 0 && botao == nb && acaoEm(nb) == ACAO_MAIS && !detmais_aberto())
        desenhaDicaBotao(rc, i18n("Mais opções"), a);
      if (a > 0.3f) ponteiro_alvo(rc.x, rc.y, rc.w, rc.h, ponteiroDetalhe, NULL, -1, nb);
      bx += NV_DETW2_CIRC + NV_DETW2_BTN_GAP;
    }
  }

  // --- ILHA DE AMIGOS, logo abaixo dos botoes ----------------------------------
  if (temAmg) {
    static const float ANEL[3] = { 0.075f, 0.08f, 0.095f };
    float y0 = yAcoes + NV_DETW2_BTN_H + hCaption + NV_DETW_AMIGOS_GAP;
    float d = 52.0f, pad = (NV_DETW_AMIGOS_H - d) * 0.5f;
    int foc = nivel == 0 && focoAmigos;
    char l1[200], l2[220];
    TxtLinha t1, t2;
    float pw, larg, tintaF = ajustes_acento_tinta(NULL, NULL, NULL);
    int c1 = foc ? (int)(tintaF * 255.0f) : 255, c2 = foc ? (int)(tintaF * 255.0f * 0.85f) : 179;
    int nr = amg.n < 3 ? amg.n : 3, ut, ue;
    // Informacao UTIL e nao so "viu" (dono, 06/10/2026): quem recomendou, o
    // que acharam, ate onde foram — e embaixo o que nao coube na manchete.
    ultimoEpisodio(&ut, &ue);
    amigostitulo_resumo(&amg, ut, ue, (long long)time(NULL), l1, sizeof l1, l2, sizeof l2);
    t1 = txt_linha_corta(TXT_DET_META2, l1, c1, c1, c1, 255, NV_DETW2_TEXTO_W - 200.0f);
    t2 = txt_linha_corta(TXT_HERO_META, l2, c2, c2, c2, 255, NV_DETW2_TEXTO_W - 200.0f);
    pw = d + d * 0.71f * (float)(nr - 1);
    larg = pad + pw + 18.0f + (t1.w > t2.w ? t1.w : t2.w) + 30.0f;
    { GfxRect ilha = { NV_DETW2_X, y0, larg, NV_DETW_AMIGOS_H };
      if (foc) plrui_pilula_foco(ilha, a);
      else plrui_material(ilha, NV_DETW_AMIGOS_H * 0.5f, 0, a);
      svd_amigos_pilha(ilha.x + pad, y0 + pad, d, &amg, nr, 1, ANEL, a);
      { float tx = ilha.x + pad + pw + 18.0f;
        float bloco = t1.h + 2.0f + t2.h, ty = y0 + (NV_DETW_AMIGOS_H - bloco) * 0.5f;
        txt_desenhar_alpha(t1, tx, ty, a);
        txt_desenhar_alpha(t2, tx, ty + t1.h + 2.0f, a * (foc ? 0.9f : 0.8f)); }
      if (a > 0.3f) ponteiro_alvo(ilha.x, ilha.y, ilha.w, ilha.h, ponteiroDetalhe, NULL, -1, DET_PTR_AMIGOS); } }

  // --- linha da AGENDA ------------------------------------------------------
  // Cor de realce e nao branco: e a unica linha do hero que fala de uma data
  // FUTURA, e a 3 m ela precisa se separar da retomada logo abaixo sem virar
  // um segundo titulo. O texto ja vem traduzido e por extenso de agenda.c —
  // nao ha segundo formatador de data neste arquivo.
  if (agLinha[0]) {
    float ar, ag, ab;
    // COM O DESPERTADOR EM FOCO, esta linha deixa de falar do TITULO e passa a
    // falar do BOTAO — nome, estado e o que ele realmente faz. E a legenda que
    // um circular mudo nao tem onde por, e cai justamente no ponto para onde o
    // olho ja foi: 37px acima do botao que acabou de receber o foco.
    //
    // A frase do mecanismo vem junto de proposito. Numa TV nao ha notificacao —
    // nem webOS nem Tizen entregam nada a um app fechado — e "Lembrar-me"
    // sozinho promete um aviso que nao existe. A tela Agenda ja diz isto uma
    // vez; aqui e onde a duvida nasce.
    char leg[220];
    int emFoco = nivel == 0 && temLembrar() && acaoEm(botao) == ACAO_LEMBRAR;
    int ligado = 0;
    { const CatItem *ciL = cat_item(idx);
      ligado = ciL && ciL->imdb[0] && agenda_lembrete(ciL->imdb); }
    if (emFoco) {
      snprintf(leg, sizeof leg, "%s \xc2\xb7 %s", rotuloLembrar(),
               i18n("o aviso aparece quando você abrir o app no dia"));
    } else {
      snprintf(leg, sizeof leg, "%s", agLinha);
    }
    if (carro && !emFoco) {
      TxtLinha label = txt_linha_corta(TXT_CAPTION2, leg, 232, 235, 240, 255,
                                      NV_DETW2_TEXTO_W - 28.0f);
      float w = label.w + 28.0f, h = 34.0f;
      GfxRect pill = {NV_DETW2_X + (larguraAcoes - w) * .5f, yAgenda, w, h};
      if (pill.x < NV_DETW2_X) pill.x = NV_DETW2_X;
      ajustes_acento(&ar, &ag, &ab);
      gfx_cor(pill, .5f, ar, ag, ab, .16f*a);
      gfx_cor((GfxRect){pill.x+1, pill.y+1, pill.w-2, pill.h-2}, .5f,
              .075f, .08f, .095f, .76f*a);
      txt_desenhar_alpha(label, pill.x + 14.0f,
                        pill.y + (h-label.h)*.5f, a);
    } else {
    // SEMPRE BRANCA (dono, 21/09/2026: "mantenha o texto sempre em branco").
    // Era a cor de realce — rosa sobre cascalho laranja nao se le.
    ar = ag = ab = 1.0f;
    (void)ligado;
    { TxtLinha l = txt_linha_corta(TXT_CAPTION, leg, (int)(ar * 255.0f + 0.5f),
                                   (int)(ag * 255.0f + 0.5f),
                                   (int)(ab * 255.0f + 0.5f), 255,
                                   NV_DETW2_TEXTO_W);
      txt_desenhar_alpha(l, NV_DETW2_X, yAgenda + (NV_DETW_RETOM_H - l.h) * 0.5f, a); }
    }
  }

  // --- linha de retomada ----------------------------------------------------
  if (ci && ci->progresso > 0) {
    char ln[160];
    if (ci->temporada > 0)
      snprintf(ln, sizeof ln, i18n("Retomada disponível   %d%%   Episódio T%dE%d"),
               ci->progresso, ci->temporada, ci->episodio);
    else
      snprintf(ln, sizeof ln, i18n("Retomada disponível   %d%%"), ci->progresso);
    TxtLinha l = txt_linha(TXT_CAPTION, ln, 255, 255, 255, 255);
    txt_desenhar_alpha(l, NV_DETW2_X, yRetom + (NV_DETW_RETOM_H - l.h) * 0.5f,
                       a * 0.82f);
  }

  // A confirmed foreign image keeps its localized caption after actions.
  // Reserve it even before decode; missing artwork uses the complete name
  // in the artwork slot, without truncating it.
  if (nomeAbaixo)
    txt_bloco(TXT_DET_META2, nome, 255, 255, 255, NV_DETW2_X,
               yAcoes + NV_DETW2_BTN_H + 16.0f, NV_DETW_LOGO_MAXW, 34, a, 0);

  // --- sinopse --------------------------------------------------------------
  { float f = 1.0f;
    if (sinTrocaDesde) {
      f = (float)(Uint32)(agoraH - sinTrocaDesde) / DET_TROCA_MS;
      if (f >= 1.0f) { f = 1.0f; sinTrocaDesde = 0; }
      else f = revela_saida(f);
    }
    if (f < 1.0f) {   // o que saiu esvanece por baixo do que entra
      if (sinAnt[0]) txt_bloco(TXT_DET_SIN, sinAnt, 255, 255, 255, NV_DETW2_X, ySin,
                               NV_DETW2_TEXTO_W, NV_DETW2_LD_SIN, a * (1.0f - f),
                               NV_DETW2_SIN_LINHAS);
      else esqueletoSinopse(NV_DETW2_X, ySin, a * (1.0f - f));
    }
    if (sin) txt_bloco(TXT_DET_SIN, sin, 255, 255, 255, NV_DETW2_X, ySin,
                       NV_DETW2_TEXTO_W, NV_DETW2_LD_SIN, a * f,
                       NV_DETW2_SIN_LINHAS);
    else if (esqSin) esqueletoSinopse(NV_DETW2_X, ySin, a * f); }

  // --- meta linha 1: generos • generos  ·  ano  ·  [IMDb] nota ---------------
  //
  // Uma linha so, na ordem do aparelho. O selo do IMDb entra AQUI, no fim dos
  // grupos, e nao encostado na borda direita da tela: ele estava orfao, a mais
  // de 1000px do texto de que faz parte, porque o valor herdado era
  // NV_DETW_DIR.
  //
  // Dois pontos separadores diferentes, e a diferenca de cor e o que agrupa a
  // linha — ver desenhaPonto.
  const float aHero = a;
  float fMeta = 1.0f;
  { const char *g0 = generoDe(idx);
    int vazia = !ano[0] && !(ci && ci->nota > 0) &&
                !(g0 && strstr(g0, "\xc2\xb7"));
    if (vazia && chegando && gateAberto) metaEsqVisto = 1;
    if (!vazia && metaEsqVisto) { metaEsqVisto = 0; metaChegouEm = agoraH ? agoraH : 1u; }
    if (metaChegouEm) {
      fMeta = (float)(Uint32)(agoraH - metaChegouEm) / DET_TROCA_MS;
      if (fMeta >= 1.0f || anim_politica_reduzida) { fMeta = 1.0f; metaChegouEm = 0; }
      else fMeta = revela_saida(fMeta);
    }
    if (vazia && chegando) {
      float yb = yMeta1 + (NV_DETW2_M1_H - 22.0f) * 0.5f;
      esqueletoTexto(NV_DETW2_X, yb, 230.0f, 22.0f, aHero);
      esqueletoTexto(NV_DETW2_X + 254.0f, yb, 96.0f, 22.0f, aHero);
    } else if (fMeta < 1.0f) {
      float yb = yMeta1 + (NV_DETW2_M1_H - 22.0f) * 0.5f;
      esqueletoTexto(NV_DETW2_X, yb, 230.0f, 22.0f, aHero * (1.0f - fMeta));
      esqueletoTexto(NV_DETW2_X + 254.0f, yb, 96.0f, 22.0f, aHero * (1.0f - fMeta));
    } }
  {
    float a = aHero * fMeta;   // o texto de meta entra esvanecendo quando chega
    float x = NV_DETW2_X, yc = yMeta1 + NV_DETW2_M1_H * 0.5f;
    const CatItem *badgeItem=cat_item(idx);
    if(badgeItem)x+=badges_desenhar(badges_provedor(badgeItem->provNome),x,yc-14,150,28,a);
    int algo = 0;
    // GENEROS sem o primeiro campo. `genero` vem do catalogo como
    // "Programa de TV · Ação · Aventura" e o primeiro trecho e sempre o TIPO
    // (ver catalogo.c:539) — a referencia nao o mostra na linha de meta, so os
    // generos. Cada um vira um trecho proprio com o "•" entre eles.
    const char *g = generoDe(idx);
    if (g) {
      const char *p = strstr(g, "\xc2\xb7");
      while (p) {
        char termo[80]; size_t n;
        p += 2; while (*p == ' ') p++;
        const char *fim = strstr(p, "\xc2\xb7");
        n = fim ? (size_t)(fim - p) : strlen(p);
        while (n && p[n-1] == ' ') n--;
        if (n && n < sizeof termo) {
          memcpy(termo, p, n); termo[n] = 0;
          if (algo) {
            desenhaPonto(x + NV_DETW2_BULLET_SEP, yc, 0.702f, a);   // 179
            x += NV_DETW2_BULLET_SEP * 2 + NV_DETW2_PONTO_D;
          }
          TxtLinha lt = txt_linha(TXT_DET_SIN, termo, 179, 179, 179, 255);
          txt_desenhar_alpha(lt, x, yc - lt.h * 0.5f, a);
          x += lt.w; algo = 1;
        }
        p = fim;
      }
    }
    // ANO. Em serie a referencia escreve "2023-" e em filme a data cheia; o
    // catalogo so guarda o ano nos dois casos (descoberta.c corta o travessao
    // da serie de proposito), entao sai o ano. Vazio quando o metadado ainda
    // nao chegou — e ai o grupo inteiro some, sem valor de reserva.
    if (ano[0]) {
      if (algo) { desenhaPonto(x + NV_DETW2_SEP, yc, 0.502f, a);    // 128
                  x += NV_DETW2_SEP * 2 + NV_DETW2_PONTO_D; }
      TxtLinha la = txt_linha(TXT_DET_SIN, ano, 179, 179, 179, 255);
      txt_desenhar_alpha(la, x, yc - la.h * 0.5f, a);
      x += la.w; algo = 1;
    }
    // NOTAS DA LINHA: as fontes que a pessoa escolheu em Ajustes > Notas no
    // titulo, cada uma com a marca e a escala do proprio site (IMDb 7,8;
    // Rotten Tomatoes 87%; Metacritic num quadrado colorido). De fabrica sao as
    // mesmas de sempre: IMDb, Rotten Tomatoes, Trakt. Quando a linha nao cabe
    // saem primeiro as menos importantes (notasfontes.c) — nunca estoura.
    { NotasPlano plano;
      float pontoW = NV_DETW2_SEP * 2 + NV_DETW2_PONTO_D;
      notasui_planejar(&plano, notasDados()->cru,
                       NV_DETW2_X + NV_DETW2_TEXTO_W - x,
                       algo ? pontoW : 0.0f, NULL);
      if (plano.n > 0) {
        if (algo) { desenhaPonto(x + NV_DETW2_SEP, yc, 0.502f, a);
                    x += pontoW; }
        x = notasui_desenhar_linha(&plano, x, yc, a);
        algo = 1;
      }
    }
    // QUANTO DA SERIE VOCE JA VIU, no fim da linha de meta.
    //
    // Fica AQUI e nao no bloco de estado logo acima dos botoes porque aquilo
    // fala do PROXIMO PASSO ("Retomada disponivel 42%", "Proximo episodio
    // T3E9") e isto fala da OBRA, como o ano e as notas ao lado. Sao dois
    // assuntos e o hero ja empilha linhas demais.
    //
    // ZERO PEDIDO A MAIS: os dois contadores vem do topo da mesma resposta de
    // /shows/<id>/progress/watched que a marca de "visto" no card do episodio
    // ja baixa. Ver extras_progresso_serie.
    //
    // NAO APARECE quando o historico ainda nao chegou nem quando a serie nao
    // estreou — 0% ali seria uma afirmacao sobre nada, e o denominador do
    // Trakt ("aired") ja desconta o que ainda vai ao ar. Tambem nao aparece em
    // filme, onde a pergunta e outra e a barra de retomada ja responde.
    if (ehSerie()) {
      int vis = 0, exib = 0;
      if (extras_progresso_serie(&vis, &exib)) {
        char pct[48];
        int p100 = (vis * 200 + exib) / (exib * 2);   // arredonda, sem float
        snprintf(pct, sizeof pct, i18n("%d%% assistido · %d de %d"),
                 p100, vis, exib);
        { TxtLinha lp = txt_linha(TXT_DET_SIN, pct, 179, 179, 179, 255);
          if (x + NV_DETW2_SEP * 2 + NV_DETW2_PONTO_D + lp.w
              <= NV_DETW2_X + NV_DETW2_TEXTO_W) {
            desenhaPonto(x + NV_DETW2_SEP, yc, 0.502f, a);
            x += NV_DETW2_SEP * 2 + NV_DETW2_PONTO_D;
            txt_desenhar_alpha(lp, x, yc - lp.h * 0.5f, a);
            x += lp.w;
          } }
      }
    }
  }

  // --- meta linha 2: [classificacao | status]  ·  duracao  ·  pais -----------
  //
  // A classificacao indicativa e o status da producao vivem DENTRO do mesmo
  // selo de contorno, com uma divisoria entre eles ("TV-MA | RENOVADA",
  // "R | LANÇADO"). Aqui a classificacao era um selo solto na linha de cima.
  //
  // STATUS: o CatItem nao tem o campo. Fica NULL, e o selo sai so com a
  // classificacao — sem divisoria e sem texto de reserva. Carimbar "LANÇADO"
  // em tudo seria repetir o erro que ja tirou a classificacao "14" fixa e o
  // elenco de demonstracao daqui.
  //
  // DURACAO so em FILME. Em serie o segundo campo de `meta` e a contagem de
  // temporadas ("3 temporadas"), e a referencia nao a mostra no hero — quem
  // conta temporadas sao as abas logo abaixo da dobra, que esta tela ja
  // desenha. Repetir a informacao aqui seria acrescentar o que o aparelho
  // tirou.
  {
    float a = aHero * fMeta;
    float x = NV_DETW2_X, yc = yMeta2 + NV_DETW2_SELO_H * 0.5f;
    int algo = 0;
    // STATUS: o do Trakt (fichaStatus, so com conta) ou, na falta, o que o TMDB
    // trouxe na agenda da serie — antes so a grafia minuscula do Trakt casava, e
    // "Returning Series"/"Ended" do TMDB nunca virava selo. Traduz pela tabela e
    // poe em MAIUSCULAS como o selo sempre foi.
    char statusTxt[48] = "";
    const char *status=NULL;
    if(ehSerie()) {
      const char *raw = extras_ficha_status();
      const char *k = desc_status_chave(raw, 1);
      if(!k) k = desc_status_chave(extras_agenda_status(), 1);
      if(k && strcmp(k, "Piloto") && strcmp(k, "Em breve") && strcmp(k, "Planejado")) {
        idioma_maiusc_em(ajustes_idioma(), statusTxt, sizeof statusTxt, i18n(k));
        status = statusTxt;
      }
    }
    if ((ci && ci->classificacao[0]) || status) {
      x += desenhaSeloMeta(x, yMeta2, ci && ci->classificacao[0] ? ci->classificacao : status,
                           ci && ci->classificacao[0] ? status : NULL, a);
      algo = 1;
    }
    if (!ehSerie() && dur[0]) {
      if (algo) { desenhaPonto(x + NV_DETW2_SEP, yc, 0.502f, a);
                  x += NV_DETW2_SEP * 2 + NV_DETW2_PONTO_D; }
      TxtLinha ld = txt_linha(TXT_DET_META2, dur, 255, 255, 255, 255);
      txt_desenhar_alpha(ld, x, yc - ld.h * 0.5f, a);
      x += ld.w; algo = 1;
    }
    if (ci && ci->pais[0]) {
      if (algo) { desenhaPonto(x + NV_DETW2_SEP, yc, 0.502f, a);
                  x += NV_DETW2_SEP * 2 + NV_DETW2_PONTO_D; }
      char paisTxt[128];
      desc_pais_txt(ci->pais, paisTxt, sizeof paisTxt);
      TxtLinha lp = txt_linha(TXT_DET_META2, paisTxt, 255, 255, 255, 255);
      txt_desenhar_alpha(lp, x, yc - lp.h * 0.5f, a);
      x += lp.w; algo = 1;
    }
    if (ci && ci->direcao[0]) {
      float gap = algo ? NV_DETW2_SEP * 2 + NV_DETW2_PONTO_D : 0;
      float remaining = NV_DETW2_X + NV_DETW2_TEXTO_W - x - gap;
      if (remaining > 100.0f) {
        if (algo) desenhaPonto(x + NV_DETW2_SEP, yc, 0.502f, a);
        TxtLinha ld = txt_linha_corta(TXT_DET_META2, ci->direcao,
                                     255, 255, 255, 255, remaining);
        txt_desenhar_alpha(ld, x + gap, yc - ld.h * 0.5f, a);
      }
    }
  }
}
#pragma pop_macro("NV_DETW2_X")

// ---------------------------------------------------------------------------
// PAGINA: temporadas, episodios, abas de informacao, elenco
// ---------------------------------------------------------------------------

// Numero REAL da temporada na posicao `c`. Serie que comeca na 2 (o que
// acontece quando o Cinemeta nao tem a 1) mostrava "Temporada 1" apontando para
// a 2, e a lista abaixo nao batia com o rotulo.
static int temporadaEm(int c) {
  const CatItem *ci = cat_item(idx);
  if (ci && ci->nTemporadas > 0)
    return (c >= 0 && c < ci->nTemporadas) ? ci->temporadas[c] : ci->temporadas[0];
  return c + 1;
}
static void rotuloTemporada(int c, char *dst, size_t n) {
  int s = temporadaEm(c);
  if (s == 0) snprintf(dst, n, "Especiais");
  else snprintf(dst, n, i18n("Temporada %d"), s);
}
// Episodios da temporada na posicao `c` (a contagem do segmentado).
static int epsDaTemporada(int c) {
  int alvo = temporadaEm(c), n = cat_n_episodios(idx), i, q = 0;
  for (i = 0; i < n; i++) {
    const CatEp *e = cat_episodio(idx, i);
    if (e && e->temporada == alvo) q++;
  }
  return q;
}
// Item do SEGMENTADO (.sg do mockup): 19/600 com 20 de cada lado e a
// contagem de episodios em 16 depois de 9 px.
static float larguraTemporada(int c) {
  char rot[32]; rotuloTemporada(c, rot, sizeof rot);
  TxtLinha l = txt_linha(TXT_CALLOUT, rot, 255, 255, 255, 255);
  return l.w + NV_DETP_TEMP_PADX * 2;
}
// As abas da serie no SEGMENTADO do Glass UI (DESIGN.md §5: substitui abas
// com separador), o mesmo das temporadas: 19/600, 20 de cada lado.
static float larguraAbaInfo(int i) {
  TxtLinha l = txt_linha(TXT_PLR_CORPO, ABA_ROTULO[abaIdDe(i)], 255, 255, 255, 255);
  return l.w;
}

// ESTE EPISODIO AINDA NAO FOI AO AR?
//
// O `videos` do Cinemeta lista a temporada INTEIRA, incluindo o que ainda vai
// estrear — e ate agora a lista desenhava esses episodios exatamente como um
// que voce so nao viu. O card convidava a abrir o que nao existe: OK levava a
// uma busca de fontes que nunca acha nada, e o unico sinal na tela era a
// AUSENCIA do selo de nota do Trakt, que e o mesmo estado de uma serie obscura
// que ninguem avaliou. Dois estados diferentes desenhados igual, que e o
// defeito que este arquivo ja corrigiu tres vezes noutros lugares.
//
// A FONTE E `next_episode_to_air` DO TMDB, e ela nao custa pedido nenhum: vem
// no mesmo corpo /tv/<id> que a pagina ja baixa para redes, temporadas e "mais
// como este" (ver a nota de agenda em extras.h). O que ela diz e a POSICAO do
// proximo episodio a estrear; dai para a frente, na ordem (temporada,
// episodio), nada foi ao ar.
//
// POR QUE NAO PELA DATA DO PROPRIO EPISODIO, que seria o caminho obvio:
// CatEp.data ja chega FORMATADA por extenso ("24 de setembro de 2026") porque
// e assim que a referencia a mostra, e o ISO e descartado no parse. Voltar dela
// para uma data exigiria reconhecer nome de mes — em portugues E em ingles,
// porque a formatacao passa por i18n. Comparar posicao de episodio e exato e
// nao depende de idioma.
//
// SEM AGENDA NAO SE AFIRMA NADA. Serie encerrada, TMDB sem o campo ou resposta
// que ainda nao chegou devolvem 0, e ai todo episodio volta a ser um episodio
// comum. Dizer "nao exibido" sem fonte seria inventar metadado, que e o que o
// PRODUCT.md proibe e o que ja tirou daqui a classificacao "14" cravada.
static int epNaoExibido(const CatEp *ep) {
  int t = extras_agenda_temporada(), e = extras_agenda_episodio();
  if (!ep || t <= 0 || e <= 0) return 0;
  if (ep->temporada != t) return ep->temporada > t;
  return ep->episodio >= e;
}

// O QUE A FILEIRA DE TEMPORADAS NAO DIZIA.
//
// As pilulas eram "Temporada 1 | Temporada 2 | Temporada 3" e mais nada. Quem
// chega numa serie de cinco temporadas nao tem como saber, sem descer e contar
// cards, quantos episodios cada uma tem, quanto ja viu de cada uma, nem que a
// atual ainda esta no ar. As tres respostas ja estao NA MEMORIA — o catalogo de
// episodios, o mapa do vistoep e a agenda do TMDB —, e nenhuma custa pedido.
//
// POR QUE UMA LINHA E NAO UM SEGUNDO ANDAR DENTRO DA PILULA: a pilula e 269x83
// MEDIDA na referencia, com uma linha de 32; enfiar um segundo texto la dentro
// obrigaria a crescer a peca (e a fileira inteira com ela) para dizer de cinco
// temporadas o que so interessa da escolhida. E o resumo mudaria de largura a
// cada temporada, fazendo as pilulas vizinhas dancarem quando o foco andasse.
//
// POR QUE ACIMA DAS PILULAS E NAO ENTRE ELAS E OS CARDS: entre as duas fileiras
// sobram 43 px (1243 -> 1286) e o anel de foco do card come 4 deles; uma linha
// de 23 px ali fica a 5 px do anel, espremida entre dois blocos grandes. Acima
// sobram ~130 px — e a vaga do cabecalho "Temporadas" que foi removido por
// repetir o rotulo das pilulas. Este texto nao repete nada.
//
// E ELE ACOMPANHA O FOCO, nao o OK: `temporada` e reescrita a partir da coluna
// focada a cada quadro (detail_atualizar), entao varrer a fileira com o D-pad
// vai contando cada temporada enquanto passa. Era exatamente a informacao que
// faltava na hora de escolher.
static void contarTemporada(int *total, int *vistos, int *futuros, int *sabe) {
  const CatItem *ci = cat_item(idx);
  int alvo = temporadaEm(temporada), n = cat_n_episodios(idx), i;
  *total = *vistos = *futuros = 0;
  // TRI-ESTADO, e e o ponto todo: vistoep separa "nao viu" de "nao sabemos"
  // (ver vistoep.h). Sem mapa desta serie a linha NAO escreve "0 assistidos" —
  // ela omite a clausula, porque zero seria uma afirmacao sobre o que ninguem
  // nos contou. O mesmo motivo pelo qual o card nao desenha um selo de "nao
  // assistido" enquanto o Trakt nao responde.
  *sabe = ci && ci->imdb[0] && vistoep_conhecido(ci->imdb);
  for (i = 0; i < n; i++) {
    const CatEp *e = cat_episodio(idx, i);
    if (!e || e->temporada != alvo) continue;
    (*total)++;
    // Nao exibido GANHA de visto: um episodio que ainda vai ao ar nao pode
    // entrar na conta do que voce assistiu, mesmo que o mapa diga que sim (o
    // Trakt aceita marcar qualquer coisa). Sem esta ordem o mesmo episodio
    // seria contado duas vezes e a soma das clausulas passaria do total.
    if (epNaoExibido(e)) { (*futuros)++; continue; }
    if (*sabe && vistoep_estado(ci->imdb, e->temporada, e->episodio) == 1)
      (*vistos)++;
  }
}

// Largura do trilho inteiro do segmentado (itens + vaos + folga).
static float larguraSegTemporadas(void) {
  int c, n = secaoN(SEC_TEMPORADAS);
  float w = DET_SEG_PAD * 2.0f;
  for (c = 0; c < n; c++) w += larguraTemporada(c) + (c ? NV_DETP_TEMP_GAP : 0.0f);
  return w;
}

// O resumo da temporada escolhida fica acima das pilulas, como na 1.7.4.
// A coluna fixa preserva a leitura mesmo quando as temporadas rolam.
static void resumoTemporada(float x, float yPilulas, float a) {
  char linha[192];
  int total = 0, vistos = 0, futuros = 0, sabe = 0;
  size_t k = 0;
  contarTemporada(&total, &vistos, &futuros, &sabe);
  if (total <= 0) return;
  // Com historico a linha e uma FRACAO ("2 de 8 assistidos"), a mesma forma
  // da linha de progresso do heroi; sem historico sobra o total — zero vistos
  // seria afirmar o que ninguem nos contou (vistoep e tri-estado).
  if (sabe)
    k += (size_t)snprintf(linha + k, sizeof linha - k,
                          i18n("%d de %d assistidos"), vistos, total);
  else
    k += (size_t)snprintf(linha + k, sizeof linha - k,
                          i18n(total == 1 ? "%d episódio" : "%d episódios"),
                          total);
  if (futuros > 0)
    snprintf(linha + k, sizeof linha - k,
             i18n(futuros == 1 ? " · %d ainda não exibido"
                               : " · %d ainda não exibidos"), futuros);
  { TxtLinha l = txt_linha(TXT_DET_META2, linha, 190, 192, 200, 255);
    txt_desenhar_alpha(l, x, yPilulas - NV_DETP_TEMP_RESUMO_DY - l.h, a); }
}

// O TRILHO do segmentado, desenhado uma vez por fileira antes dos itens:
// branco 6% no vidro, #1D1E23 no solido (plrui_seg).
static void trilhoTemporadas(float y, float a) {
  GfxRect tr = { NV_DETP_X - scrollSec[SEC_TEMPORADAS], y - DET_SEG_PAD,
                 larguraSegTemporadas(), NV_DETP_TEMP_H + DET_SEG_PAD * 2.0f };
  if (ajustes_vidro()) gfx_cor(tr, 0.5f, 1, 1, 1, 0.06f * a);
  else gfx_cor(tr, 0.5f, 0.114f, 0.118f, 0.137f, a);
}

// Item do segmentado de temporadas (Glass UI, mockup "detalhe-retomar"). Tres
// estados, nenhum deles contorno: nada (so texto a 55%), ESCOLHIDA (branco
// 14% / #34363E, texto branco) e FOCADA (pilula cheia no acento com a luz,
// tinta de contraste). A contagem de episodios vai ao lado, em 16 a 35%.
static void desenhaTemporada(GfxRect r, int c, float f, float a) {
  char rot[32]; rotuloTemporada(c, rot, sizeof rot);
  int sel = (c == temporada);
  float raio = NV_RAIO_PILL;
  // TODA temporada e um CHIP, escolhida ou nao. Antes so a escolhida tinha
  // container e as outras eram texto solto sobre o fundo — nao liam como um
  // grupo de botoes, e nao havia como adivinhar que eram clicaveis.
  //
  // MEDIDO na referencia: chips #2D2D2D de 285x83; a ESCOLHIDA se distingue
  // pelo TEXTO branco (o fundo continua #2D2D2D), e a FOCADA INVERTE — fundo
  // quase branco, texto escuro, SEM anel.
  //
  // O que estava aqui punha um anel branco em volta e deixava o miolo em
  // #2D2D2D com texto cinza 170: o item focado virava o MAIS APAGADO da
  // fileira, lido na TV como "desabilitado". A inversao e a mesma linguagem de
  // foco dos botoes circulares do heroi, medida na mesma referencia — foco e
  // "escolhido" deixam de colidir sem precisar de dois tons de cinza que a
  // 3 metros ninguem separa.
  // CORRIGIDO em 16/09/2026, o dono olhando a captura: "aqui tem que ser fill
  // quando selecionado e nao o contorno". A escolhida-sem-foco tinha anel
  // #C2C4C9 por cima de um miolo #363636 quase igual ao das outras — o anel
  // fazia todo o trabalho, e anel e o que a regra proibe.
  //
  // Agora sao TRES PREENCHIMENTOS da mesma familia, como a barra de abas da
  // Biblioteca e o seletor Serie|Episodio: #222 na que nao e nada, 60% da
  // superficie de foco (#939393) na escolhida em repouso, #F5F5F5 na focada.
  // Tres degraus bem separados a 3 metros, e nenhum deles e um contorno.
  // Os tres degraus na COR DE REALCE (layout.h): cheia na focada, 60% na
  // escolhida em repouso, #222 na que nao e nada.
  float fr, fg, fb, ti = ajustes_acento_tinta(&fr, &fg, &fb);
  foco_anel(r, raio, f, a);   // so com "Foco no cartaz" ligado (focoprof.h)
  if (ajustes_vidro()) {
    // Vidro: repouso = fio; a escolhida leva a lavagem e o aro do realce; a
    // focada e a pilula cheia no realce. Os tres degraus seguem existindo.
    if (sel) gfx_vidro_painel_acento(r, raio, 0.5f, a); else gfx_vidro_painel(r, raio, 0.4f, a);
    gfx_vidro_pilula_cheia(r, raio, f, a);
  } else
  { float br = sel ? fr * 0.6f : 0.133f, bg = sel ? fg * 0.6f : 0.133f,
          bb = sel ? fb * 0.6f : 0.133f;
    gfx_cor(r, raio, br + (fr - br) * f, bg + (fg - bg) * f, bb + (fb - bb) * f, a); }
  // Texto: cinza claro na que nao e nada; sobre o realce, a tinta que
  // contrasta com ele (ti), em degrau no meio da mola.
  { int t = (int)(ti * 255.0f + 0.5f);
    int cor = (sel || f > 0.5f) ? t : 179;
    if (ajustes_vidro()) cor = f > 0.5f ? t : (sel ? 245 : 179);   // escolhida: superficie escura
    TxtLinha l = txt_linha(TXT_CALLOUT, rot, cor, cor, cor, 255);
    // 500 de peso na Inter Regular: uma segunda passada meio pixel a direita.
    txt_peso(l, r.x + (r.w - l.w) * 0.5f, r.y + (r.h - l.h) * 0.5f, a, 0.5f); }
}

// CARTAO DE EPISODIO NO GLASS UI (mockup "detalhe-retomar", 03/10): a still
// de 400x225 (raio 20) e o texto ABAIXO dela — kicker "EPISÓDIO n · 56min",
// o nome em 24/600 e, so no focado, a sinopse em duas linhas. O FOCO NAO E
// ANEL: o cartao sobe 10 px, a still cresce 4% a partir da base e ganha a
// sombra caida (0 22 50 .6). Assistido e um DISCO de 34 no canto da still; o
// progresso e o trilho de 4 px na base dela, no acento.
static void veuEpisodio(GfxRect th, float a) {
  float raio = NV_DETP_EP_RAIO / th.h;
  if (raio > 0.5f) raio = 0.5f;
  if (raio > 0.5f * th.w / th.h) raio = 0.5f * th.w / th.h;
  gfx_rect(th, 0, GFX_VEU_CARD, 0, 0, 0, raio, 0, 0, 0, a);
}

// O SELO DE NOTA DO EPISODIO: o LOGO da fonte e o numero em NEGRITO (dono,
// 08/10). Era "• Trakt 8.1": a palavra em 15 px cinza e o numero em Regular —
// o que se lia primeiro era o nome do servico, e a nota, que e a informacao,
// vinha depois e mais fraca. O logo diz a fonte sem leitura (o disco vermelho,
// o letreiro azul) e sobra peso para o numero. `fonte` e EX_TRAKT ou EX_TMDB.
static float desenhaNotaEpisodio(float x, float y, int fonte, int nota, float a) {
  char valor[12];
  float ar, ag, ab;
  TxtLinha lv;
  GfxRect selo;
  const float pad = 9.0f, gap = 8.0f, hm = fonte == EX_TMDB ? 19.0f : 20.0f;
  float wm;
  if (nota <= 0) return 0.0f;
  snprintf(valor, sizeof valor, idioma_ponto_decimal(ajustes_idioma()) ? "%d.%d" : "%d,%d",
           nota / 10, nota % 10);
  lv = txt_linha(TXT_G21B, valor, 242, 245, 250, 255);
  wm = notasui_marca_largura(fonte, hm);
  selo = (GfxRect){ x, y, pad + wm + gap + lv.w + pad + 2.0f, 30.0f };
  ajustes_acento(&ar, &ag, &ab);
  // Fundo quase-preto com uma lavagem mínima do accent: a marca continua
  // discreta sobre a foto e deixa de parecer um bloco cinza genérico.
  if (ajustes_vidro()) gfx_vidro_painel(selo, 0.5f, 0.6f, a);
  else
  gfx_cor(selo, 0.5f, .055f + ar * .06f, .062f + ag * .06f,
          .078f + ab * .07f, .94f * a);
  notasui_marca(fonte, x + pad, y + selo.h * .5f, hm, a);
  txt_desenhar_alpha(lv, x + pad + wm + gap, y + (selo.h - lv.h) * .5f, a);
  return selo.w + 12.0f;
}

static void desenhaAssistidoEpisodio(GfxRect th, float a) {
  const float h = 46, pad = 12, icone = 28, gap = 10;
  TxtLinha texto = txt_linha_corta(TXT_CAPTION2, "Assistido", 255, 255, 255, 255,
                                  th.w * .5f - pad * 2 - icone - gap);
  float w = pad * 2 + icone + gap + texto.w;
  GfxRect selo = {th.x + th.w - w - 18, th.y + 18, w, h};
  gfx_cor(selo, .5f, .055f, .068f, .09f, .96f * a);
  gfx_anel(selo, .5f, 1, .68f, .75f, .86f, .30f * a);
  GfxRect disco = {selo.x + pad, selo.y + (h - icone) * .5f, icone, icone};
  gfx_cor(disco, .5f, .451f, .839f, .694f, a);
  gfx_icone((GfxRect){disco.x + 5, disco.y + 5, icone - 10, icone - 10},
             "check", 1, 1, 1, a);
  txt_desenhar_alpha(texto, disco.x + icone + gap,
                     selo.y + (h - texto.h) * .5f, a);
}

static void desenhaEpisodio(GfxRect r, int c, float f, float a, Uint32 agora) {
  const CatEp *ep = cat_episodio(idx, epAbsoluto(c));
  GfxRect th = { r.x, r.y, r.w, epThumbH() };
  float raioTh = NV_DETP_EP_RAIO / epThumbH();

  // Foco: no web e um box-shadow na MINIATURA, nao no card, e nao ha escala
  // nenhuma (`transform: none`). A cor acompanha o accent escolhido — o anel
  // branco fixo fazia esta fileira destoar justamente quando o resto da tela
  // ja seguia o tema.
  // FOCO COMO O DA HOME (focoprof.h): anel so com "Foco no cartaz" ligado;
  // desligado, a miniatura cresce com a animacao do foco. O brilho e o
  // realce de profundidade seguem os mesmos Ajustes (Episodios).
  GfxRect thLayout = th;
  if (!epApple()) { foco_anel(th, raioTh, f, a); th = foco_zoom(th, f); }

  const CatItem *serie = cat_item(idx);
  const char *arte = (ep && ep->thumb[0]) ? ep->thumb
                     : (serie && serie->backdrop[0] ? serie->backdrop : NULL);
  GLuint t2 = arte ? tex_obter_larg(arte, th.w) : 0;
  // STILL QUE NAO EXISTE NEM NO METAHUB NEM NO TMDB (One Piece da 2a
  // temporada em diante, 23/09): o card nao fica cinza, leva o fundo da serie
  // — o numero e o nome do episodio ja estao no texto por cima. O still
  // continua sendo pedido acima, para o recuo do tex_cache tentar de novo.
  if (!t2 && ep && ep->thumb[0] && arte == ep->thumb && tex_falhou(arte) && serie) {
    const char *reserva = serie->backdrop[0] ? serie->backdrop
                        : (serie->poster[0] ? serie->poster : NULL);
    GLuint t3 = reserva ? tex_obter_larg(reserva, th.w) : 0;
    if (t3) { arte = reserva; t2 = t3; }
  }
  float aArte = (c >= 0 && c < DET_REV_EP) ? revela_arte(&revEp[c], t2 != 0, agora) : 1.0f;
  // DESFOCAR NAO ASSISTIDOS (blurUnwatchedEpisodes, #133). O ajuste existia na
  // tela e na conta e nada o lia — o card saia nitido em toda plataforma.
  // "Nao assistido" e tudo que o mapa NAO afirma como visto: inclusive o "nao
  // sei" (-1) de quem nao tem Trakt ou cujo historico ainda nao chegou. Aqui o
  // erro seguro e o contrario do check: desfocar um episodio que a pessoa ja
  // viu custa nada; mostrar nitido o que ela pediu para esconder e o spoiler.
  if (t2 && ajustes_desfocar_nao_assistidos() &&
      !(ep && serie && serie->imdb[0] &&
        vistoep_estado(serie->imdb, ep->temporada, ep->episodio) == 1)) {
    GLuint tb = gfx_desfocado(t2, arte);
    // Copia ainda nao gerada (no maximo duas por quadro): o fundo do card, e
    // nunca a arte nitida por um quadro.
    if (tb) t2 = tb;
    else { t2 = 0; arte = NULL; }
  }
  if (t2) {
    if (aArte < 0.999f) gfx_cor(th, raioTh, 0.133f, 0.133f, 0.133f, a);
    gfx_tex_aspect_atual = tex_aspecto(arte);
    gfx_rect(th, t2, GFX_CARD, f, 0, 0, raioTh, 0, 0, 0, a * aArte);
    gfx_tex_aspect_atual = 0.0f;
  } else if (arte && !tex_falhou(arte))
    gfx_esqueleto(th, raioTh, 0.133f, 0.133f, 0.133f, a);
  else gfx_cor(th, raioTh, 0.133f, 0.133f, 0.133f, a);
  veuEpisodio(th, a);
  if (!epApple()) foco_profundidade(th, raioTh, ajustes_profundidade_episodios(), a);
  if (ajustes_vidro()) gfx_vidro_aro(th, raioTh, 1.5f, 1, 1, 1, 0.14f * a);   // vidro: aro fino na miniatura

  // A mesma fonte que o menu modifica: desconhecido (-1) nao recebe selo.
  // A lavagem e leve para preservar a arte; texto e estado sao desenhados depois.
  if (ep && serie && serie->imdb[0] &&
      vistoep_estado(serie->imdb, ep->temporada, ep->episodio) == 1) {
    gfx_cor(th, raioTh, 0, 0, 0, .12f * a);
    desenhaAssistidoEpisodio(th, a);
  }
  th = thLayout;   // o texto abaixo segue a caixa de repouso

  // NADA DE RESERVA INVENTADA. Aqui as quatro linhas caiam numa tabela de
  // demonstracao (nome, duracao, data e sinopse de "Shrinking"), entao um
  // episodio sem dado nao aparecia vazio: aparecia com o TEXTO DE OUTRA SERIE,
  // indistinguivel de informacao real. Campo ausente agora fica ausente, e o
  // desenho abaixo ja omite cada pedaco que vier vazio.
  const char *epNome = (ep && ep->nome[0])    ? ep->nome    : NULL;
  char epDurTxt[32] = "";
  if (ep && ep->duracao[0]) desc_duracao_txt(ep->duracao, epDurTxt, sizeof epDurTxt);
  const char *epDur  = epDurTxt[0] ? epDurTxt : NULL;
  const char *epData = (ep && ep->data[0])    ? ep->data    : NULL;
  const char *epSin  = (ep && ep->sinopse[0]) ? ep->sinopse : NULL;
  int epNum = ep ? ep->episodio : c + 1;

  if (epApple()) {
    float textX = r.x + 18.0f, textW = r.w - 36.0f;
    float textY = r.y + epThumbH() + 18.0f;
    GfxRect info = {r.x,textY-8,r.w,epCardH()-epThumbH()-18};
    if (f > .005f) {
      gfx_cor(info,20.0f/info.h,.19f,.21f,.23f,.64f*f*a);
      if (ajustes_borda_foco()) gfx_anel(th,20.0f/th.h,1.5f,1,1,1,.18f*f*a);
    }
    float metaY = th.y + th.h - 36.0f;
    int watched = serie && ep &&
      vistoep_estado(serie->imdb, ep->temporada, ep->episodio) == 1;
    int current = serie && ep && serie->temporada == ep->temporada &&
      serie->episodio == ep->episodio;
    int progress = current && !watched ? serie->progresso : 0;
    if (watched)
      gfx_icone((GfxRect){textX,metaY+2,20,20},"aj_rotate-ccw-clock",
                .9f,.92f,.94f,a);
    else
      gfx_rect((GfxRect){textX,metaY+4,14,17},0,GFX_PLAY,0,0,0,0,
               .9f,.92f,.94f,a);
    float durationX = textX+28;
    if (progress > 0 && progress < 100) {
      GfxRect track = {durationX,metaY+10,72,4};
      gfx_cor(track,.5f,1,1,1,.25f*a);
      track.w *= progress/100.0f; gfx_cor(track,.5f,1,1,1,.8f*a);
      durationX += 84;
      if (serie->restanteMin > 0) {
        char remaining[64];
        int h = serie->restanteMin / 60, m = serie->restanteMin % 60;
        if (h && m) snprintf(remaining,sizeof remaining,i18n("%dh %dmin Restantes"),h,m);
        else if (h) snprintf(remaining,sizeof remaining,i18n("%dh Restantes"),h);
        else snprintf(remaining,sizeof remaining,i18n("%dmin Restantes"),m);
        txt_desenhar_alpha(txt_linha_corta(TXT_CAPTION,remaining,235,237,240,255,
                          r.x+r.w-18-durationX),durationX,metaY,a);
      }
    } else if (epDur)
      txt_desenhar_alpha(txt_linha(TXT_CAPTION,epDur,235,237,240,255),durationX,metaY,a);
    char number[32]; snprintf(number,sizeof number,i18n("EPISÓDIO %d"),epNum);
    txt_desenhar_alpha(txt_linha(TXT_CAPTION2,number,164,169,176,255),textX,textY,a);
    char fallback[40]; snprintf(fallback,sizeof fallback,i18n("Episódio %d"),epNum);
    TxtLinha name = txt_linha_corta(TXT_DET_META2,epNome ? epNome : fallback,
                                   246,247,250,255,textW);
    txt_desenhar_alpha(name,textX,textY+30,a);
    float y = textY+64;
    if (epSin) y += txt_bloco_corta(TXT_CAPTION,epSin,171,177,185,
                                   textX,y,textW,29,a,4);
    if (epData) {
      const char *date = epData;
      if (!ajustes_data_completa() && strlen(date)>=4) date += strlen(date)-4;
      txt_desenhar_alpha(txt_linha_corta(TXT_CAPTION2,date,165,171,180,255,textW),
                        textX,y+10,a);
    }
    if (epNaoExibido(ep)) {
      TxtLinha status = txt_linha_corta(TXT_CAPTION2,i18n("NÃO EXIBIDO"),245,199,77,255,textW);
      txt_desenhar_alpha(status,textX,r.y+epCardH()-30,a);
    }
    return;
  }

  float tx = r.x + NV_DETP_EP_PAD;

  // Ausencia de check nao afirma que o historico ja chegou. Evita um selo
  // "nao assistido" inventado enquanto o Trakt ainda esta consultando.

  // Selo "EPISÓDIO n": caixa de 43 de altura, raio 12, fundo escuro
  // translucido, texto em caixa alta.
  //
  // Voltou a ser "EPISÓDIO n" e nao "T2E4". A forma curta entrou por uma
  // captura antiga do dono, mas a referencia no aparelho escreve "EPISÓDIO 1"
  // por extenso — e o argumento de que a forma curta "diz de que temporada e"
  // nao se sustenta: o card so aparece dentro da aba da temporada escolhida,
  // que esta desenhada logo acima dele.
  { char cab[24];
    float xs = tx;
    snprintf(cab, sizeof cab, i18n("EPISÓDIO %d"), epNum);
    { TxtLinha l = txt_linha(TXT_CAPTION2, cab, 255, 255, 255, 255);
      float w = l.w + NV_DETP_EP_SELO_PADX * 2;
      GfxRect s = { xs, r.y + NV_DETP_EP_SELO_Y, w, NV_DETP_EP_SELO_H };
      if (ajustes_vidro()) gfx_vidro_painel(s, 12.0f / NV_DETP_EP_SELO_H, 0.6f, a);
      else
      gfx_cor(s, 12.0f / NV_DETP_EP_SELO_H, 0.05f, 0.05f, 0.06f, 0.78f * a);
      txt_peso(l, s.x + NV_DETP_EP_SELO_PADX,
               s.y + (NV_DETP_EP_SELO_H - l.h) * 0.5f, a, 1.0f);
      xs += w + NV_DETP_EP_SELO_GAP; }

    // AINDA NAO FOI AO AR: um segundo selo, colado no primeiro.
    //
    // POR QUE UM SELO PREENCHIDO EM AMBAR e nao um card apagado: a 3 metros uma
    // diferenca de opacidade entre cards vizinhos nao se le — e nesta tela ela
    // ainda brigaria com o anel de foco, que e a outra coisa que muda o brilho
    // de um card. Superficie preenchida com texto escuro e o mesmo vocabulario
    // de foco desta base (DESIGN.md secao 5), e o ambar #F5C74D e a posicao que
    // a paleta de dado reserva para "atencao sem alarme" — nao e vermelho, que
    // diria erro, nem verde, que diria pronto.
    //
    // A PALAVRA DEPENDE DO QUE HA PARA LER. Com data, o selo diz so "ESTREIA" e
    // quem carrega o quando e a data ja desenhada na direita do rodape, na
    // MESMA coluna de todos os outros cards — repetir a data aqui dentro seria
    // escrever a mesma coisa duas vezes no mesmo card, e a versao longa
    // ("ESTREIA 24 DE SETEMBRO DE 2026") passa de 350 px e briga com o selo do
    // numero dentro dos 576 de texto. Sem data nao ha nada na direita para o
    // "ESTREIA" apontar, e ai o selo precisa se bastar: "NÃO EXIBIDO".
    if (epNaoExibido(ep)) {
      const char *rot = epData ? i18n("ESTREIA") : i18n("NÃO EXIBIDO");
      const int vid = ajustes_vidro();
      // Vidro: o ambar vira lavagem + aro e texto ambar (a cor ainda diz "atencao").
      TxtLinha l = vid ? txt_linha(TXT_CAPTION2, rot, 245, 199, 77, 255)
                       : txt_linha(TXT_CAPTION2, rot, 20, 20, 20, 255);
      float w = l.w + NV_DETP_EP_SELO_PADX * 2;
      GfxRect s = { xs, r.y + NV_DETP_EP_SELO_Y, w, NV_DETP_EP_SELO_H };
      if (vid) { gfx_cor(s, 12.0f / NV_DETP_EP_SELO_H, 0.961f, 0.780f, 0.302f, 0.14f * a);
                 gfx_anel(s, 12.0f / NV_DETP_EP_SELO_H, 1.5f, 0.961f, 0.780f, 0.302f, 0.6f * a); }
      else
      gfx_cor(s, 12.0f / NV_DETP_EP_SELO_H, 0.961f, 0.780f, 0.302f, a);
      // Mesmo peso extra do selo vizinho: sao a mesma peca tipografica, e o
      // texto escuro sobre superficie clara ja parece mais grosso do que e
      // (a regra optica de text.c), entao 1.0 e o teto aqui.
      txt_peso(l, s.x + NV_DETP_EP_SELO_PADX,
               s.y + (NV_DETP_EP_SELO_H - l.h) * 0.5f, a, 1.0f);
    } }

  // Titulo: 32/800. O 800 nao existe na familia embarcada e o 32 so existe em
  // Regular na tabela de estilos, entao vem de tres passadas.
  // Sem nome do episodio, "Episodio N" — que e um rotulo VERDADEIRO, deduzido
  // do numero, e nao o titulo de outra serie.
  { char reserva[32];
    const char *nome = epNome;
    if (!nome) { snprintf(reserva, sizeof reserva, i18n("Episódio %d"), epNum);
                 nome = reserva; }
    TxtLinha l = txt_linha_corta(TXT_PLR_CORPO, nome, 255, 255, 255, 255,
                                 NV_DETP_EP_TEXTO_W);
    txt_peso(l, tx, r.y + NV_DETP_EP_TIT_Y, a, 1.4f); }

  // Sinopse: tres linhas, como a referencia, com truncamento do bloco.
  // Sem sinopse o espaco
  // fica vazio: melhor um card com menos texto que um card com texto errado.
  // TXT_CAPTION (22/400) e nao TXT_DET_SIN (26), dono 08/10: no corpo do card a
  // sinopse da pagina pesava tanto quanto o titulo e empurrava o nome, os logos
  // e as notas em negrito para segundo plano. E o estilo que a sinopse ja usa no
  // card do layout Apple (logo acima), com a entrelinha dele (NV_LD_CAPTION).
  if (epSin)
    txt_bloco_corta(TXT_CAPTION, epSin, 255, 255, 255, tx,
                    r.y + NV_DETP_EP_SIN_Y, NV_DETP_EP_TEXTO_W,
                    NV_DETP_EP_LD_SIN, a * 0.9f, 3);

  // Meta: relogio + duracao + data, 20/400 rgb(179,179,179), com 38 de folga
  // entre os dois blocos.
  { float x = tx, y = r.y + NV_DETP_EP_META_Y;
    // Relogio de 28x28. O glifo do web e um disco CHEIO em rgb(179,179,179) com
    // os ponteiros VAZADOS — o `path` do SVG recorta o L do ponteiro do disco.
    // Aqui o vazado sai pintando os ponteiros de preto por cima: naquele ponto
    // da miniatura o veu ja esta em 0.95, entao o que estaria atras do recorte e
    // praticamente preto. Desenhar os ponteiros na MESMA cor do disco, como
    // estava, some com eles e deixa so uma bolinha cinza.
    // O relogio so entra COM a duracao ao lado. Sozinho ele nao e um icone, e
    // um rotulo sem valor: um disco cinza solto no canto do card, que le como
    // defeito de desenho.
    if (epDur) {
      float cx = x + NV_DETP_EP_ICONE * 0.5f, cy = y + NV_DETP_EP_ICONE * 0.5f;
      GfxRect aro = { x, y, NV_DETP_EP_ICONE, NV_DETP_EP_ICONE };
      GfxRect pv = { cx - 1.5f, cy - 8, 3, 9.5f };
      GfxRect ph = { cx - 1.5f, cy - 1.5f, 8, 3 };
      gfx_rect(aro, 0, GFX_ANEL, 0, 2.0f / NV_DETP_EP_ICONE, 0, 0.5f,
               0.76f, 0.77f, 0.79f, a);
      gfx_cor(pv, 0.0f, 0.76f, 0.77f, 0.79f, a);
      gfx_cor(ph, 0.0f, 0.76f, 0.77f, 0.79f, a);
      x += NV_DETP_EP_ICONE + 8.0f;
      { TxtLinha ld = txt_linha(TXT_CAPTION2, epDur, 179, 179, 179, 255);
        txt_desenhar_alpha(ld, x, y, a); x += ld.w + 16; }
    }
    // Extras fornece avaliacao Trakt por episodio, nao IMDb. Nunca usar
    // a nota da serie ou o selo de outro provedor neste rodape.
    int nota = 0;
    if (ep) for (int st = 0; st < extras_n_temporadas(); st++) {
      if (extras_temporada_numero(st) != ep->temporada) continue;
      for (int ei = 0; ei < extras_n_eps(st); ei++)
        if (extras_ep_numero(st, ei) == ep->episodio) {
          nota = extras_ep_nota(st, ei); break;
        }
      break;
    }
    if (nota > 0)
      x += desenhaNotaEpisodio(x, y - 4, EX_TRAKT, nota, a);
    // O voto do TMDB entra AO LADO do do Trakt, no mesmo selo compacto
    // (issue #87). Sao fontes diferentes — o logo diz qual e qual, e um
    // episodio pode ter uma, a outra ou as duas.
    if (ep && ep->nota > 0)
      x += desenhaNotaEpisodio(x, y - 4, EX_TMDB, ep->nota, a);
    // A DATA vai para a direita do card, como na referencia: a esquerda fica so
    // a duracao, e as duas deixam de disputar a mesma linha corrida.
    if (epData) {
      const char *data = epData;
      size_t nData = strlen(epData);
      if (!ajustes_data_completa() && nData >= 4) data = epData + nData - 4;
      float disponivel = r.x + r.w - NV_DETP_EP_PAD - x;
      if (disponivel > 48) {
        TxtLinha lf = txt_linha_corta(TXT_CAPTION2, data, 179, 179, 179, 255, disponivel);
        txt_desenhar_alpha(lf, r.x + r.w - NV_DETP_EP_PAD - lf.w, y, a);
      }
    } }

  // Barra de progresso: 576x8 a 16px da base da miniatura, trilho
  // rgba(0,0,0,0.45) e preenchimento rgb(158,158,158). So aparece entre 2% e
  // 98% — e o mesmo intervalo do web, e e o que faz um episodio recem-comecado
  // nao ganhar uma barra de largura zero.
  { int prog = 0;
    const CatItem *ci = cat_item(idx);
    if (ci && ci->progresso > 0 && ep && ci->temporada == ep->temporada &&
        ci->episodio == ep->episodio) prog = ci->progresso;
    if (prog > 2 && prog < 98) {
      GfxRect tr = { tx, r.y + NV_DETP_EP_BARRA_Y, NV_DETP_EP_TEXTO_W,
                     NV_DETP_EP_BARRA_H };
      // Anda do valor que esta tela mostrava ate o novo (revela.h): na volta
      // do player a barra do episodio cresce ate onde a pessoa parou.
      float mostra = revela_progresso(ci->imdb, ep->temporada, ep->episodio,
                                      (float)prog, SDL_GetTicks());
      GfxRect at = { tr.x, tr.y, tr.w * (mostra / 100.0f), tr.h };
      gfx_cor(tr, 0.5f, 0, 0, 0, 0.45f * a);
      gfx_cor(at, 0.5f, 0.62f, 0.62f, 0.62f, a);
    } }
}

// Abas de informacao: texto puro, sem pilula. Escolhida (ou focada) em branco,
// as outras em #808080; o divisor "|" e 32/700 #808080. O foco no web e
// `transform: scale(1.03)` — o unico lugar desta tela que escala.
static void desenhaAbaInfo(float x, float y, int i, float f, float a) {
  int sel = (i == abaInfo);
  int base = sel ? 255 : 128;
  int cor = (int)(base + (255 - base) * f);
  TxtLinha l = txt_linha(TXT_PLR_CORPO, ABA_ROTULO[abaIdDe(i)], cor, cor, cor, 255);
  txt_peso(l, x, y + (NV_DETP_ABA_H - l.h) * 0.5f, a, 0.5f + f * 0.6f);
}

// Elenco: avatar redondo de 140 ALINHADO A ESQUERDA do card de 220 (nao
// centralizado, que era o desenho anterior), nome 26/500 rgb(179,179,179) e
// papel 21/400 rgb(128,128,128) abaixo dele.
// --- card de TRAILER ---------------------------------------------------------
//
// Miniatura 520x292 raio 24, selo de play ao centro, nome embaixo e o tipo em
// cinza. A miniatura vem de img.youtube.com por URL previsivel, e tex_obter
// baixa e cacheia sozinho — nao ha codigo de rede aqui.
//
// E focavel e OK abre o video no app nativo da plataforma: navegador do webOS
// (luna-send), aba do Tizen (window.open) ou browser do desktop (open).
//
// FOCO (23/09/2026, o dono: "hoje nao mostra que esta selecionado"): o anel na
// cor de realce em volta da MINIATURA, o mesmo do card de episodio logo acima
// na pagina — e o mesmo tipo de card (video com legenda embaixo), entao o foco
// tem de ser o mesmo. `f` e a animacao do foco da secao (animFoco).
static void desenhaTrailer(float x, float y, int c, float f, float a) {
  const char *mini = extras_trailer_miniatura(c);
  GfxRect v = { x, y, NV_DETF_TR_W, NV_DETF_TR_VIDEO_H };
  float raio = NV_DETF_TR_RAIO / NV_DETF_TR_VIDEO_H;   // fracao do MENOR lado
  GLuint tex = (mini && mini[0]) ? tex_obter_larg(mini, NV_DETF_TR_W) : 0;

  // Foco da Home (focoprof.h): anel so com o ajuste ligado, senao a miniatura
  // cresce; brilho/profundidade seguem os Ajustes (Trailers).
  foco_anel(v, raio, f, a);
  v = foco_zoom(v, f);

  if (tex) {
    gfx_tex_aspect_atual = tex_aspecto(mini);
    gfx_rect(v, tex, GFX_CARD, f, 0, 0, raio, 1, 1, 1, a);
    gfx_tex_aspect_atual = 0.0f;
  } else {
    gfx_cor(v, raio, 0.13f, 0.13f, 0.13f, a);
  }
  foco_profundidade(v, raio, ajustes_profundidade_trailers(), a);

  // Selo de play: disco escuro e o triangulo por cima, centrados na miniatura.
  { float d = NV_DETF_TR_PLAY_D;
    GfxRect disco = { v.x + (v.w - d) * 0.5f,
                      v.y + (v.h - d) * 0.5f, d, d };
    GfxRect tri   = { disco.x + d * 0.34f, disco.y + d * 0.28f,
                      d * 0.36f, d * 0.44f };
    gfx_cor(disco, 0.5f, 0.0f, 0.0f, 0.0f, a * 0.48f);
    gfx_rect(tri, 0, GFX_PLAY, 0, 0, 0, 0.0f, 1, 1, 1, a); }

  { TxtLinha ln = txt_linha_corta(TXT_ROW_TITULO, extras_trailer_nome(c),
                                  245, 248, 255, 255, NV_DETF_TR_W);
    txt_desenhar_alpha(ln, x, y + NV_DETF_TR_NOME_DY, a); }
  { TxtLinha lt = txt_linha(TXT_CAPTION2, "YouTube", 179, 179, 179, 255);
    txt_desenhar_alpha(lt, x, y + NV_DETF_TR_TIPO_DY, a * 0.9f); }
}

// --- tabela "Detalhes do Filme" ---------------------------------------------
//
// Duas colunas: chave em cinza a esquerda, valor em branco numa coluna FIXA.
// A coluna do valor nao segue a largura da chave — se seguisse, cada linha
// comecaria num x diferente e a tabela serrilharia. Divisoria de 1px sob cada
// linha menos a ultima, como na referencia.
//
// Recebe `f` so para saber se a secao esta focada: a tabela nao tem item a
// item, entao o foco nela e a propria secao, e o realce e sutil de proposito —
// nao ha o que escolher aqui, so o que ler.
// GLASS UI (mockup "Detalhe", .ficha): chave 17 a 45% numa coluna de 220,
// valor 20 a 100%, linha de 57 com o fio de 1 px a 7% embaixo de cada uma.
static void desenhaDetalhes(float x, float y, float f, float a) {
  LinhaDet l[NV_DETF_DET_MAXL];
  int n = montarDetalhes(l, NV_DETF_DET_MAXL), i;
  for (i = 0; i < n; i++) {
    float ly = y + i * NV_DETF_DET_LINHA;
    float yc = ly + NV_DETF_DET_LINHA * 0.5f;
    TxtLinha lk = txt_linha(TXT_DET_META2, l[i].chave, 150, 154, 163, 255);
    TxtLinha lv = txt_linha_corta(TXT_DET_META, l[i].valor, 235, 238, 245, 255,
                                  NV_DETF_DET_W - NV_DETF_DET_CHAVE_W);
    txt_desenhar_alpha(lk, x, yc - lk.h * 0.5f, a * 0.9f);
    txt_desenhar_alpha(lv, x + NV_DETF_DET_CHAVE_W, yc - lv.h * 0.5f, a);
    if (i < n - 1) {
      GfxRect d = { x, ly + NV_DETF_DET_LINHA - 1.0f, NV_DETF_DET_W, 1.0f };
      gfx_cor(d, 0.0f, 1, 1, 1, a * (0.10f + 0.06f * f));
    }
  }
}

static void desenhaElenco(float x, float y, int c, float f, float a) {
  const CatItem *ci = cat_item(idx);
  const char *nome = NULL, *papel = NULL, *foto = NULL;
  if (ci && c < ci->nElenco) {
    nome = ci->elenco[c].nome;
    papel = ci->elenco[c].papel;
    if (ci->elenco[c].foto[0]) foto = ci->elenco[c].foto;
  }
  if (ci && ci->nElenco > 0 && c >= ci->nElenco) return;
  // SEM ELENCO NAO SE INVENTA ELENCO. Aqui havia uma reserva cravada
  // (`ELENCO[c % N_ELENCO]`) que preenchia a fileira com o elenco de
  // "Shrinking" — e o resultado era o Homem-Aranha creditando Jason Segel e
  // Harrison Ford, com cara de dado real. Mesmo defeito do "14" que estava
  // cravado em descoberta.c: valor de demonstracao exibido como informacao.
  //
  // A fileira nem chega aqui sem dado, porque secaoN devolve 0 (e focus_mover
  // pula fileira vazia). Este `return` e a segunda tranca.
  if (!nome || !nome[0]) return;

  if (epApple() && f > .005f) {
    float top = y + NV_DETP_EL_AVATAR + NV_DETP_EL_NOME_DY - 8;
    GfxRect info = {x-12,top,NV_DETP_EL_W+24,76};
    gfx_cor(info,18.0f/info.h,.19f,.21f,.23f,.6f*f*a);
  }
  GfxRect av = { x + (NV_DETP_EL_W-NV_DETP_EL_AVATAR)*.5f, y, NV_DETP_EL_AVATAR, NV_DETP_EL_AVATAR };
  // Foco da Home (focoprof.h): anel so com o ajuste ligado, senao o avatar
  // cresce; a profundidade segue o ajuste de Elenco.
  if (epApple()) { if (ajustes_borda_foco() && f > 0.01f) gfx_anel(av,.5f,1.5f,1,1,1,.22f*f*a); }
  else foco_anel(av, 0.5f, f, a);
  av = foco_zoom(av, f);
  GLuint t2 = foto ? tex_obter_larg(foto, NV_DETP_EL_AVATAR) : 0;
  if (t2) {
    gfx_tex_aspect_atual = tex_aspecto(foto);
    // GFX_AVATAR faz o cover e a mascara radial no mesmo passe. GFX_CARD
    // arredondava o retangulo, mas preservava a logica de enquadramento de
    // cartaz — sobravam faixas e a foto nao ocupava o circulo inteiro.
    gfx_rect(av, t2, GFX_AVATAR, 0, 0, 0, 0.0f, 0, 0, 0, a);
    gfx_tex_aspect_atual = 0.0f;
  } else {
    // Sem foto, a inicial sobre #222 (#303030 com foco) — e o que o web faz
    // com `.movie-cast-avatar-fallback`.
    float lum = 0.133f + 0.055f * f;
    gfx_cor(av, 0.5f, lum, lum, lum, a);
    char ini[5] = {0};
    for (int k = 0; k < 4 && nome[k] && (unsigned char)nome[k] >= 0x20; k++) {
      ini[k] = nome[k];
      if ((nome[k] & 0xC0) != 0x80) { if (k) { ini[k] = 0; break; } }
    }
    TxtLinha li = txt_linha(TXT_TITULO3, ini, 210, 212, 220, 255);
    txt_desenhar_alpha(li, av.x + (av.w - li.w) * 0.5f,
                       av.y + (av.h - li.h) * 0.5f, a * 0.9f);
  }
  foco_profundidade(av, 0.5f, ajustes_profundidade_elenco(), a);
  float yn = y + NV_DETP_EL_AVATAR + NV_DETP_EL_NOME_DY;
  TxtLinha ln = txt_linha_corta(TXT_CALLOUT, nome, epApple()?238:179, epApple()?240:179, epApple()?244:179, 255, NV_DETP_EL_W);
  txt_desenhar_alpha(ln, x + (NV_DETP_EL_W-ln.w)*.5f, yn, a);
  if (papel && papel[0]) {
    TxtLinha lp = txt_linha_corta(TXT_CAPTION2, papel, epApple()?166:128, epApple()?172:128, epApple()?180:128, 255,
                                  NV_DETP_EL_W);
    txt_desenhar_alpha(lp, x + (NV_DETP_EL_W-lp.w)*.5f, yn + NV_DETP_EL_PAPEL_DY, a * 0.95f);
  }
}

// Aba "Avaliacoes". No web (metaDetailsScreen.js:3699) sao dois cartoes lado a
// lado, IMDb e TMDB: .movie-rating-card de 160x120, raio 14, fundo
// rgba(18,23,31,.9) com borda de 1px a 16%, logo de 56x28 em cima e o valor em
// 34/800 embaixo; quando o dado falta o cartao mostra "-".
//
// DIVERGENCIA ANOTADA: em SERIE o web troca isto por um painel de avaliacoes
// POR EPISODIO (renderSeriesRatingsPanel), com seletor de temporada. Este port
// nao tem nota por episodio em fonte nenhuma — o Cinemeta nao devolve — entao
// serie mostra os mesmos dois cartoes do filme. Nao e a tela do web; e o que o
// dado permite, e mostrar dois cartoes certos e melhor que uma grade vazia.
#define AVAL_CARD_W  160.0f
#define AVAL_CARD_H  120.0f
#define AVAL_CARD_GAP 16.0f
// Contorno de 1px a 16%, em quatro faixas: nao ha helper de borda no gfx e
// este mesmo desenho serve o cartao de nota e o de comentario.
// `raio` em PIXEIS; a conversao para a fracao do menor lado que o gfx espera e
// feita aqui. Passar 14 direto (o raio do CSS) fazia o SDF saturar e o cartao
// saia de canto reto — o valor do gfx e fracao, nao pixel.
// RAIO DE CARTAZ, em fracao do menor lado — o SDF do shader e normalizado.
//
// Existe porque cinco pontos deste arquivo faziam `NV_RAIO_CARD / largura`, e
// NV_RAIO_CARD JA E UMA FRACAO (0,055). Dividir de novo pela largura dava
// ~0,0003, ou seja canto reto: era por isso que os cartazes de "Recomendações"
// e as fotos da filmografia saiam quadrados enquanto os da home eram
// arredondados. O valor vem do mesmo `posterCardCornerRadiusDp` da home, para
// as duas telas terem o mesmo canto.
// O DIVISOR E A ALTURA, e isto foi conferido no shader, nao deduzido: em
// FS_SDF (gfx.c) o fragmento faz `p = (uv - 0.5) * vec2(asp, 1.0)` e
// `b = vec2(0.5*asp, 0.5) - r`, ou seja a meia-extensao vertical e sempre 0,5 e
// o `r` e medido contra ela. Dividir pelo MENOR LADO, como estava aqui, so
// acerta em retangulo mais largo que alto; num cartaz (retrato) o menor lado e
// a LARGURA, e pedir 24/largura sobre uma altura maior pedia um canto bem mais
// fechado que os 24 px que o comentario dizia garantir. O teto e metade da
// LARGURA medida na mesma escala, senao um retangulo mais largo que alto
// termina com canto reto na horizontal e redondo na vertical.
static float raioCartaz(float w, float h) {
  if (h <= 0.0f) return NV_RAIO_CARD;
  { float r = ajustes_raio_poster_px() / h;
    float teto = 0.5f * w / h;
    if (r > 0.5f)  r = 0.5f;
    if (r > teto)  r = teto;
    return r; }
}

// `raio` chega em PIXEIS e sai em fracao da ALTURA, pela mesma razao escrita
// em raioCartaz: o `r` do SDF e medido contra a meia-altura. Aqui o retangulo e
// deitado, entao o menor lado era a altura e a conta antiga dava o mesmo
// resultado por acidente — o teto pela largura e que faltava.
static void moldura(GfxRect r, float raio, float a) {
  float teto = r.h > 0.0f ? 0.5f * r.w / r.h : 0.5f;
  raio = r.h > 0.0f ? raio / r.h : 0.0f;
  if (raio > 0.5f) raio = 0.5f;
  if (raio > teto) raio = teto;
  // SUPERFICIE ESCURA TINGIDA, um degrau acima do fundo. O cinza #2D2D2D da
  // primeira versao competia com os graficos e fazia esta aba parecer um modal
  // separado; o navy quase-preto mantem a hierarquia sem virar uma placa.
  if (ajustes_vidro()) { gfx_vidro_painel(r, raio, 0.5f, a); return; }
  gfx_cor(r, raio, 0.082f, 0.094f, 0.118f, 0.92f * a);
  // SEM FIO DE CONTORNO. Havia aqui um anel branco a 14% que existia para
  // "fechar" o cartao sobre a arte de fundo — o dono mandou tirar em 16/09
  // ("tirar esse contorno daqui tb"), e ele estava contra a regra: nesta tela
  // contorno so aparece onde preencher e impossivel, e aqui o preenchimento
  // #151820 ja separa o cartao do fundo sozinho.
}

// Disco com a inicial (o .av do mockup), na cor que o nome sorteia.
static void avatarInicial(GfxRect d, const char *nome, TxtEstilo e, float a) {
  static const float COR[6][3] = { { 0.239f, 0.545f, 0.761f }, { 0.761f, 0.239f, 0.420f },
    { 0.478f, 0.498f, 0.569f }, { 0.541f, 0.361f, 0.761f }, { 0.761f, 0.439f, 0.239f },
    { 0.239f, 0.639f, 0.420f } };
  unsigned h = 5381u; const unsigned char *p;
  char ini[8] = "?";
  for (p = (const unsigned char *)(nome ? nome : ""); *p; p++) h = h * 33u + *p;
  if (nome && nome[0]) {
    int n = 1;
    if ((unsigned char)nome[0] >= 0xC0) while (n < 4 && ((unsigned char)nome[n] & 0xC0) == 0x80) n++;
    memcpy(ini, nome, (size_t)n); ini[n] = 0;
    if (n == 1 && ini[0] >= 'a' && ini[0] <= 'z') ini[0] = (char)(ini[0] - 32);
  }
  { const float *c = COR[h % 6u];
    gfx_cor(d, 0.5f, c[0], c[1], c[2], a); }
  { TxtLinha l = txt_linha(e, ini, 255, 255, 255, 255);
    txt_desenhar_alpha(l, d.x + (d.w - (float)l.w) * 0.5f, d.y + (d.h - (float)l.h) * 0.5f, a); }
}

// Cartao de nota: a MARCA em cima e o valor embaixo, como o .movie-rating-card
// do web (logo 56x28, valor 34/800). As marcas sao os proprios arquivos do app
// web convertidos para PNG em art/marcas — desenhar um retangulo colorido com
// as iniciais, que era o que estava aqui, fica com cara de esboco ao lado de
// componentes que usam arte de verdade.
static void cartaoNota(float x, float y, int fonte, const char *marca,
                       const char *valor, float a) {
  GfxRect card = { x, y, AVAL_CARD_W, AVAL_CARD_H };
  const char *cam = marca;
  GLuint t;
  moldura(card, 14.0f, a);
  // Marca desenhada (MyAnimeList, Roger Ebert, MDBList): sem arquivo, `marca`
  // vem NULL e a caixa da marca e preenchida por notasui.
  t = cam ? tex_obter(cam) : 0;
  { TxtLinha lv = txt_linha(TXT_TITULO3, valor, 245, 248, 255, 255);
    float hLogo = 28.0f, hBloco = hLogo + 12.0f + lv.h;
    float yb = y + (AVAL_CARD_H - hBloco) * 0.5f;
    if (t) {
      float ap = tex_aspecto(cam);
      float w;
      if (ap <= 0.0f) ap = 2.0f;
      w = hLogo * ap;
      if (w > 96.0f) { w = 96.0f; hLogo = w / ap; }
      { GfxRect rl = { x + (AVAL_CARD_W - w) * 0.5f, yb, w, hLogo };
        // GFX_CARD e nao GFX_TEXTO: o modo de texto pinta a forma com a COR
        // dada e joga fora o RGB da textura — o logo do IMDb sairia como uma
        // silhueta branca. Aqui a marca tem de manter a cor dela.
        gfx_tex_aspect_atual = 0.0f;
        gfx_rect(rl, t, GFX_CARD, 0, 0, 0, 0.0f, 0, 0, 0, a); }
    }
    if (!cam)
      notasui_marca_cartao(fonte, 0, x + AVAL_CARD_W * 0.5f, yb + 14.0f, 28.0f, a);
    txt_desenhar_alpha(lv, x + (AVAL_CARD_W - lv.w) * 0.5f, yb + 28.0f + 12.0f, a);
  }
}

// A FILEIRA de notas, na ordem do web: trakt, imdb, tmdb, tomatoes, audience,
// metacritic, letterboxd. So entra a fonte que TEM nota — o web faz o mesmo
// (`.filter(([,,value]) => value != null)`), e uma fileira de "-" nao informa
// nada. IMDb vem do catalogo quando o mdbList nao respondeu por ele.
// PASTILHA DE NOTA DE EPISODIO. As cores e as faixas sao as do web
// (ratingToneClass, metaDetailsScreen.js:912, e as regras
// .series-episode-rating-chip.*): >=9 excelente, >=8 otimo, >=7.5 bom,
// >=7 misto, >=6 ruim, >0 pessimo. O numero e a nota do Trakt, nao do IMDb.
static void corDaNota(int notaDec, float *r, float *g, float *b, float *tx) {
  float rr, gg, bb, t;
  if      (notaDec >= 90) { rr=0.078f; gg=0.643f; bb=0.302f; t=0.97f; }  /* #14a44d */
  else if (notaDec >= 80) { rr=0.180f; gg=0.733f; bb=0.404f; t=0.97f; }  /* #2ebb67 */
  else if (notaDec >= 75) { rr=0.243f; gg=0.722f; bb=0.400f; t=0.97f; }  /* #3eb866 */
  else if (notaDec >= 70) { rr=0.906f; gg=0.706f; bb=0.196f; t=0.10f; }  /* #e7b432 */
  else if (notaDec >= 60) { rr=0.906f; gg=0.298f; bb=0.235f; t=0.97f; }  /* #e74c3c */
  else if (notaDec >  0)  { rr=0.388f; gg=0.224f; bb=0.455f; t=0.97f; }  /* #633974 */
  else                    { rr=0.925f; gg=0.816f; bb=0.239f; t=0.09f; }  /* #ecd03d */
  *r = rr; *g = gg; *b = bb; *tx = t;
}

#define RAT_PIL_W    86.0f
#define RAT_PIL_H    62.0f
#define RAT_PIL_GAP  10.0f
#define RAT_TEMP_H   38.0f
#define RAT_TEMP_GAP 10.0f

// Painel de SERIE: fileira de temporadas e a grade de pastilhas por episodio.
// E o renderSeriesRatingsPanel do web, que ate agora nao tinha fonte aqui — a
// aba de serie caia nos mesmos cartoes do filme.
static void desenhaNotasEpisodio(float x, float y, float a) {
  int nt = extras_n_temporadas(), t, i, ne, primeira = 0, cabem;
  if (ratTemp >= nt) ratTemp = 0;
  // JANELA: com mais temporadas do que cabem na faixa (South Park tem 27,
  // #79) a fileira mostra um trecho com a escolhida dentro, em vez de
  // desenhar chips fora da tela.
  cabem = (int)((NV_TELA_W - NV_DETP_X * 2 + RAT_TEMP_GAP) / (58.0f + RAT_TEMP_GAP));
  if (cabem < 1) cabem = 1;
  if (nt > cabem) {
    primeira = ratTemp - cabem / 2;
    if (primeira > nt - cabem) primeira = nt - cabem;
    if (primeira < 0) primeira = 0;
  }
  for (t = primeira; t < nt && t < primeira + cabem; t++) {
    char rot[8];
    float bx = x + (t - primeira) * (58.0f + RAT_TEMP_GAP);
    GfxRect r = { bx, y, 58.0f, RAT_TEMP_H };
    int sel = (t == ratTemp);
    snprintf(rot, sizeof rot, "T%d", extras_temporada_numero(t));
    gfx_cor(r, 0.5f, 1, 1, 1, (sel ? 0.28f : 0.14f) * a);
    { TxtLinha l = txt_linha(TXT_DET_META2, rot, 241, 247, 254, 255);
      txt_desenhar_alpha(l, bx + (58.0f - l.w) * 0.5f,
                         y + (RAT_TEMP_H - l.h) * 0.5f, a); }
  }
  ne = extras_n_eps(ratTemp);
  { float gy = y + RAT_TEMP_H + 18.0f;
    for (i = 0; i < ne; i++) {
      float gx = x + i * (RAT_PIL_W + RAT_PIL_GAP);
      int nd = extras_ep_nota(ratTemp, i);
      float cr, cg, cb, tx;
      char ep[8], nv[8];
      if (gx + RAT_PIL_W > NV_TELA_W - NV_DETP_X) break;
      corDaNota(nd, &cr, &cg, &cb, &tx);
      gfx_cor((GfxRect){ gx, gy, RAT_PIL_W, RAT_PIL_H }, 14.0f / RAT_PIL_H,
              cr, cg, cb, a);
      snprintf(ep, sizeof ep, "E%d", extras_ep_numero(ratTemp, i));
      if (nd > 0) { snprintf(nv, sizeof nv, "%.1f", nd / 10.0f); idioma_decimal_texto(nv, ajustes_idioma()); }
      else        snprintf(nv, sizeof nv, "-");
      // 14/700 no rotulo e 28/800 no valor, do web
      // (.series-episode-rating-ep e .series-episode-rating-val). TXT_TITULO3 e
      // 48 e estourava a pastilha de 62 — o "E1" era empurrado para fora dela.
      { int c = (int)(tx * 255.0f);
        TxtLinha le = txt_linha(TXT_MINI, ep, c, c, c, 255);
        TxtLinha lv = txt_linha(TXT_ROW_TITULO, nv, c, c, c, 255);
        float h = le.h + 2.0f + lv.h;
        float yb = gy + (RAT_PIL_H - h) * 0.5f;
        txt_desenhar_alpha(le, gx + (RAT_PIL_W - le.w) * 0.5f, yb, a);
        txt_desenhar_alpha(lv, gx + (RAT_PIL_W - lv.w) * 0.5f, yb + le.h + 2.0f, a); }
    } }
}

static void desenhaAvaliacoes(float x, float y, float a) {
  int i, col = 0;
  for (i = 0; i < EX_NFONTES; i++) {
    int v = extras_nota(i);
    char txt[8];
    // Sem mdbList o IMDb ainda vem do catalogo, que guarda 0..100; no vetor a
    // escala e "cru x 10", e para o imdb o cru e 0..10.
    if (i == EX_IMDB && !v && ajustes_mdblist_fonte(EX_IMDB)) v = notaDe(idx);
    if (!v) continue;
    // Na escala do site (notasfontes.c): 87%, 7,8, 72 — o Metacritic nao leva
    // "%" e o Letterboxd e de 0 a 5.
    nf_texto(i, v, !ajustes_idioma_ingles(), 0, txt, sizeof txt);
    cartaoNota(x + col * (AVAL_CARD_W + AVAL_CARD_GAP), y, i,
               (i == EX_MAL || i == EX_EBERT || i == EX_MDBSCORE) ? NULL
               : i == EX_METAUSER ? extras_caminho_marca_nome("metacritic")
               : extras_caminho_marca(i), txt, a);
    col++;
  }
}

// Aba "Mais como este": /related do Trakt. Uma coluna de titulos com o ano, e
// nao os posteres do web — o related do Trakt devolve identificador e nome, e
// buscar poster para doze titulos so para pintar esta aba custaria doze
// pedidos de rede a cada abertura. O que a aba precisa responder e "o que mais
// se parece com isto", e o nome responde.
// "Mais como este" em CARTAZES, e nao em lista de texto: e assim que o web
// mostra (renderPreviewRail) e e o que o dono pediu ao ver a lista crua. O
// poster vem do proprio Trakt, com `extended=images` no /related — buscar arte
// noutro servico seria um pedido por titulo so para pintar esta aba.
// Cast and recommendations occupy independent rows for movies and series.
// Only the recommendations row owns its horizontal focus.
static int relNaLista(void) {
  return foco.fileira == SEC_RELACIONADOS;
}
static int relIndice(void) {
  return foco.coluna;
}

static void desenhaRelacionados(float x, float y, float a, int primeiro, int limite) {
  int n = extras_n_relacionados(), i;
  int naLista = relNaLista();
  int foc = relIndice();
  for (i = primeiro; i < n && i < limite && i < EX_REL_MAX; i++) {
    float cx = x + (i - primeiro) * (REL_CARD_W + REL_CARD_GAP);
    GfxRect r = { cx, y, REL_CARD_W, REL_CARD_H };
    int aceso = naLista && i == foc;
    // POSTER PERSONALIZADO (posterprov.h): o relacionado so tem o id (tt ou
    // tmdb:N); o tipo e o do titulo aberto (o Trakt e o TMDB devolvem o mesmo).
    const char *po = posterprov_card(extras_relacionado_imdb(i), 0,
                                     ehSerie() ? "series" : "movie",
                                     extras_relacionado_poster(i));
    const char *ano = extras_relacionado_ano(i);
    GLuint t = po[0] ? tex_obter_larg(po, REL_CARD_W) : 0;
    float raio = raioCartaz(REL_CARD_W, REL_CARD_H);
    if (aceso) { relFocoRect = r; snprintf(relFocoArte, sizeof relFocoArte, "%s", po); }
    // O realce de foco e o da Home (focoprof.h): so apple (Dinamica) tem o
    // painel da legenda. O cartao de vidro/accent que envolvia arte e legenda
    // era um contorno de 8 px que ignorava "Foco no cartaz".
    if (aceso && epApple()) {
      float alturaLegenda = 12.0f + 22.0f + (ano[0] ? 6.0f + 18.0f : 0.0f) + 8.0f;
      GfxRect legend = {cx-10,y+REL_CARD_H+4,REL_CARD_W+20,alturaLegenda+8};
      gfx_cor(legend,16.0f/legend.h,.19f,.21f,.23f,.64f*a);
      if (ajustes_borda_foco()) gfx_anel(r,raio,1.5f,1,1,1,.18f*a);
    }
    // Foco da Home no cartaz (focoprof.h): anel so com o ajuste ligado, senao
    // ele cresce; brilho e profundidade seguem os Ajustes.
    float fRel = (aceso && i < N_ITENS) ? animFoco[SEC_RELACIONADOS][i] : 0.0f;
    if (!epApple()) foco_anel(r, raio, fRel, a);
    r = foco_zoom(r, fRel);
    { float aArte = revela_arte(&revRel[i], t != 0, SDL_GetTicks());
      if (t) {
        if (aArte < 0.999f) gfx_cor(r, raio, 0.133f, 0.133f, 0.133f, a);
        gfx_tex_aspect_atual = tex_aspecto(po);
        gfx_rect(r, t, GFX_CARD, fRel, 0, 0, raio, 0, 0, 0, a * aArte);
        gfx_tex_aspect_atual = 0.0f;
      } else if (po[0] && !tex_falhou(po)) {
        gfx_esqueleto(r, raio, 0.133f, 0.133f, 0.133f, a);
      } else {
        gfx_cor(r, raio, 0.133f, 0.133f, 0.133f, a);
      }
      foco_profundidade(r, raio, ajustes_profundidade_posters(), a); }
    { int c = aceso ? 255 : 225;
      TxtLinha lt = txt_linha_corta(TXT_DET_META2, extras_relacionado_titulo(i),
                                    c, c, c, 255, REL_CARD_W);
      txt_desenhar_alpha(lt, cx, y + REL_CARD_H + 12.0f, a);
      if (ano[0]) {
        int cy = aceso ? 174 : 156;
        TxtLinha la = txt_linha(TXT_MINI, ano, cy, cy + 2, cy + 9, 255);
        txt_desenhar_alpha(la, cx, y + REL_CARD_H + 12.0f + lt.h + 6.0f,
                           a * 0.95f);
      } }
  }
}

// Cartao de produtora/rede: logo TMDB centralizado quando ha, nome quando nao
// — o `.detail-company-card` do web faz a mesma escolha (img ou span). O card
// e FOCAVEL e o OK abre o browse da entidade (mesma pasta sintetica TMDB das
// colecoes).
// O CARTAO DE REDE/ESTUDIO. Tres defeitos numa captura so ("tem logo cagada no
// network and studios"), e os tres tinham a mesma raiz: o logo era desenhado
// com GFX_CARD.
//
// 1. GFX_CARD IGNORA O ALFA DA TEXTURA — ele le so o RGB e recorta pelo SDF.
//    Logo de marca vem quase sempre recortado sobre transparencia, entao o que
//    aparecia era o RGB do fundo transparente (preto, na maioria dos PNG) como
//    se fosse desenho. Era o "quadrado" em volta das letras.
// 2. GFX_CARD faz OVER-SCAN de 3% em cada borda, reserva de parallax que so
//    faz sentido em cartaz. Num wordmark isso come a margem que o proprio
//    arquivo reserva, e a arte sai com as pontas cortadas.
// 3. GFX_CARD ESCURECE a arte em 20% quando `foco` e 0 (`cor *= 0.80`). Logo de
//    marca tem cor propria; escurecer inventa outra.
//
// GFX_ARTE resolve os tres: RGB e alfa inteiros, recortados pela mesma mascara
// arredondada, sem over-scan e sem ganho. O recorte monocromatico continua no
// GFX_MARCA; como a superficie em foco permanece escura, a marca segue clara
// enquanto o cartao recebe o accent configuravel.
static void desenhaEstudio(float x, float y, int i, float f, float a) {
  GfxRect r0 = { x, y, EST_CARD_W, EST_CARD_H };
  GfxRect r = foco_zoom(r0, f);
  const char *logo = extras_estudio_logo(i);
  const float raio = 14.0f / EST_CARD_H;
  GLuint t;
  // MESMO FOCO DOS OUTROS CARTOES DA PAGINA (focoprof.h): anel so com "Foco no
  // cartaz" ligado, senao o cartao cresce; profundidade pelo ajuste de
  // cartazes. Antes o foco aqui era um preenchimento no acento (ou o vidro de
  // realce) que "Mais como este" e o Elenco nao tem — na Moderna o estudio era
  // o unico cartao da pagina com outra linguagem de foco (dono, 03/10/2026).
  // REPOUSO = a mesma superficie neutra dos demais (moldura).
  // Sem anel o foco nao pode ser so o crescimento de 6 % num cartao de logo:
  // sobe para a superficie clara neutra das linhas em foco (plrui_linha_foco),
  // sem acento, sem halo.
  if (!ajustes_vidro()) foco_anel(r0, raio, f, a);
  moldura(r, 14.0f, a);
  if (!ajustes_borda_foco() && f > 0.01f) plrui_linha_foco(r, 14.0f, f * a);
  if (ajustes_vidro()) foco_anel(r0, raio, f, a);
  foco_profundidade(r, raio, ajustes_profundidade_posters(), a);
  // Logo pedido pela largura com que o cartao desenha (h=52, w<=204; cap 256
  // com a folga cobrindo o crescimento do foco) — o 640 unico decodificava um
  // wordmark inteiro para 52 px de altura. Ver tests/artemenor.c.
  t = logo[0] ? tex_obter_larg(logo, EST_CARD_W - 36.0f) : 0;
  if (t) {
    float ap = tex_aspecto(logo), w, h, fr, fg, fb;
    int recorte;
    if (ap <= 0.0f) ap = 3.0f;
    h = 52.0f; w = h * ap;
    if (w > EST_CARD_W - 36.0f) { w = EST_CARD_W - 36.0f; h = w / ap; }
    recorte = tex_cor_fundo(logo, &fr, &fg, &fb) != 1;
    { float k = r.w / EST_CARD_W;
      GfxRect rl = { r.x + (r.w - w * k) * 0.5f,
                     r.y + (r.h - h * k) * 0.5f, w * k, h * k };
      gfx_tex_aspect_atual = 0.0f;
      // Logo recortado sempre branco; logo com fundo proprio fica como veio.
      if (recorte) {
        float tom = 0.93f;
        gfx_rect(rl, t, GFX_MARCA, 0, 0, 0, 0.0f, tom, tom, tom, a);
      } else {
        gfx_rect(rl, t, GFX_ARTE, 0, 0, 0, 8.0f / h, 1, 1, 1, a);
      } }
  } else {
    int c = f > 0.5f ? 245 : 208;
    TxtLinha ln = txt_linha_corta(TXT_DET_META2, extras_estudio_nome(i),
                                  c, c, 220, 255,
                                  EST_CARD_W - 28.0f);
    txt_desenhar_alpha(ln, r.x + (r.w - ln.w) * 0.5f,
                       r.y + (r.h - ln.h) * 0.5f, a * 0.95f);
  }
}

// "1999–2003" (ou so "1999") a partir dos anos das partes; "" sem ano nenhum.
static void anosColecao(char *buf, size_t tam) {
  int n = extras_n_colecao(), i, lo = 0, hi = 0;
  for (i = 0; i < n; i++) {
    int v = atoi(extras_colecao_ano(i));
    if (v <= 0) continue;
    if (!lo || v < lo) lo = v;
    if (v > hi) hi = v;
  }
  if (!lo) buf[0] = 0;
  else if (lo == hi) snprintf(buf, tam, "%d", lo);
  else snprintf(buf, tam, "%d\xe2\x80\x93%d", lo, hi);
}

// "3 filmes  ·  1999–2003". "filmes" ja existe na tabela de idiomas.
static void metaColecao(char *buf, size_t tam) {
  char anos[16];
  anosColecao(anos, sizeof anos);
  snprintf(buf, tam, "%d %s%s%s", extras_n_colecao(), i18n("filmes"),
           anos[0] ? "  \xc2\xb7  " : "", anos);
}

// Parte da colecao que e o titulo aberto, ou -1.
static int parteAtualColecao(void) {
  const CatItem *ci = cat_item(idx);
  int n = extras_n_colecao(), i;
  if (!ci || ci->tmdb <= 0) return -1;
  for (i = 0; i < n; i++)
    if (extras_colecao_tmdb(i) == ci->tmdb) return i;
  return -1;
}

// Cartaz com canto do app, esqueleto enquanto baixa e placa neutra sem arte.
static void cartazColecao(GfxRect r, const char *po, float a) {
  GLuint t = po[0] ? tex_obter_larg(po, r.w) : 0;
  float raio = raioCartaz(r.w, r.h);
  if (t) {
    gfx_tex_aspect_atual = tex_aspecto(po);
    gfx_rect(r, t, GFX_CARD, 1.0f, 0, 0, raio, 0, 0, 0, a);
    gfx_tex_aspect_atual = 0.0f;
  } else if (po[0] && !tex_falhou(po)) {
    gfx_esqueleto(r, raio, 0.133f, 0.133f, 0.133f, a);
  } else {
    gfx_cor(r, raio, 0.133f, 0.133f, 0.133f, a);
  }
}

// MINI CARD DA COLECAO (#194, 2a volta). Era uma lista de nomes soltos; o dono
// pediu "em forma de mini card com artwork", diferente da fileira de
// recomendados. Um cartao so: o BACKDROP da colecao no fundo (veu de leitura a
// esquerda assado no GFX_VITRINE), o nome e "N filmes · anos" a esquerda e os
// cartazes das primeiras partes em ESCADA a direita. Sem backdrop, a escada
// sozinha sobre a superficie neutra ja diz "isto e uma saga". O foco e o da
// miniatura de trailer (anel na cor de realce) ou o contorno do vidro.
// GLASS UI (mockup "Detalhe", "Coleção"): bloco .gl de 300 com a arte da
// colecao por baixo de um veu que escurece a esquerda (110deg, 92% ate 35%),
// o nome em 40/800, a meta a 60%, a pilula "Ver coleção" e dois cartazes de
// 130x195 em leque a direita. Foco: a pilula vira o botao no acento (o OK
// abre a lista da saga) e o bloco sobe um degrau, sem anel.
static void desenhaColecao(float x, float y, float f, float a) {
  GfxRect r = { x, y, COL_CARD_W, COL_CARD_H };
  float raio = COL_CARD_RAIO / COL_CARD_H;
  const char *fundo = extras_colecao_fundo();
  GLuint t = fundo[0] ? tex_obter_larg(fundo, COL_CARD_W) : 0;
  int n = extras_n_colecao(), k, nCapas;
  float capaX = x + COL_CARD_W - COL_CARD_PAD - COL_CAPA_W, textoW;

  // Anel so com "Foco no cartaz" ligado (focoprof.h); vidro usa o cartao de
  // vidro como anel, por cima da arte, logo abaixo.
  if (!ajustes_vidro()) foco_anel(r, raio, f, a);
  if (t) {
    gfx_tex_aspect_atual = tex_aspecto(fundo);
    gfx_rect(r, t, GFX_VITRINE, 1.0f, 0.5f, 0.0f, raio, 0.30f, 0, 0, a);
    gfx_tex_aspect_atual = 0.0f;
  } else {
    moldura(r, COL_CARD_RAIO, a);
    if (fundo[0] && !tex_falhou(fundo)) gfx_esqueleto(r, raio, 0.12f, 0.12f, 0.13f, a * 0.6f);
  }
  if (ajustes_vidro()) foco_anel(r, raio, f, a);

  // Escada: de tras para a frente, para a primeira parte ficar por cima.
  nCapas = n < 3 ? n : 3;
  for (k = nCapas - 1; k >= 0; k--) {
    float esc = 1.0f - 0.10f * (float)k;
    float w = COL_CAPA_W * esc, h = COL_CAPA_H * esc;
    GfxRect c = { capaX - COL_CAPA_DX * (float)k + (COL_CAPA_W - w),
                  y + (COL_CARD_H - h) * 0.5f, w, h };
    GfxRect sombra = { c.x - 3.0f, c.y - 3.0f, c.w + 6.0f, c.h + 6.0f };
    gfx_cor(sombra, raioCartaz(sombra.w, sombra.h), 0, 0, 0, a * 0.45f);
    cartazColecao(c, extras_colecao_poster(k), a * (1.0f - 0.18f * (float)k));
  }

  // Texto a esquerda, centrado na vertical do cartao.
  textoW = capaX - COL_CAPA_DX * (float)(nCapas > 1 ? nCapas - 1 : 0)
         - 28.0f - (x + COL_CARD_PAD);
  { char meta[64];
    TxtLinha lm;
    float yt = y + COL_CARD_PAD + 6.0f;
    metaColecao(meta, sizeof meta);
    lm = txt_linha_corta(TXT_DET_META2, meta, 205, 210, 220, 255, textoW);
    { float h = txt_bloco_corta(TXT_TITULO3, extras_colecao_nome(),
                                250, 251, 255, x + COL_CARD_PAD, yt, textoW,
                                52.0f, a, 2);
      txt_desenhar_alpha(lm, x + COL_CARD_PAD, yt + h + 14.0f, a * 0.95f); } }

  // PILULA "Ver coleção" (mockup Glass UI "Detalhe"): em repouso um vidro
  // neutro, em foco o botao no ACENTO com a tinta de foco — o sinal de foco que
  // existe com o anel ligado ou desligado, e o que diz que o OK abre a saga.
  { float ar, ag, ab, pf = f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
    TxtLinha lr = txt_linha(TXT_DET_META2, i18n("Ver coleção"), 235, 238, 245, 255);
    GfxRect pil = { x + COL_CARD_PAD, y + COL_CARD_H - COL_CARD_PAD - 48.0f,
                    lr.w + 52.0f, 48.0f };
    int tin = ajustes_tinta_foco(), cc;
    ajustes_acento(&ar, &ag, &ab);
    gfx_cor(pil, 0.5f, 1.0f, 1.0f, 1.0f, 0.14f * (1.0f - pf) * a);
    if (pf > 0.01f) gfx_cor(pil, 0.5f, ar, ag, ab, pf * a);
    cc = (int)(235.0f + (float)(tin - 235) * pf);
    lr = txt_linha(TXT_DET_META2, i18n("Ver coleção"), cc, cc, cc, 255);
    txt_desenhar_alpha(lr, pil.x + 26.0f, pil.y + (pil.h - lr.h) * 0.5f, a); }
}

// TELA DE LISTA DA COLECAO. A saga inteira, na ordem de lancamento (extras ja
// ordena), uma parte por linha: cartaz, titulo, ano e nota, e a sinopse curta.
// A parte que e o titulo aberto leva "Você está aqui". Fundo = o backdrop da
// colecao dissolvido no fundo da pagina (o mesmo GFX_VITRINE do destaque da
// home, numa passada so), a coluna da esquerda apresenta a colecao.
static void abrirListaColecao(void) {
  int atual = parteAtualColecao();
  if (extras_n_colecao() < 1) return;
  colListaAberta = 1;
  colListaFoco = atual >= 0 ? atual : 0;
  { float passo = COLL_LIN_H + COLL_LIN_GAP;
    float alvo = (float)colListaFoco * passo - (NV_TELA_H * 0.35f - COLL_TOPO);
    colListaScroll = alvo > 0.0f ? alvo : 0.0f;
    colListaVel = 0.0f; }
}

// O OK numa parte da saga (toque curto). A parte traz so o id do TMDB: o
// caminho e o mesmo do credito de um ator. OK na parte que ja esta aberta so
// fecha a lista.
static void abrirParteColecao(void) {
  long t = extras_colecao_tmdb(colListaFoco);
  if (colListaFoco != parteAtualColecao() && t > 0)
    desc_pedir_titulo_tmdb(t, "movie");
  colListaAberta = 0;
}

static void eventoListaColecao(const SDL_Event *e) {
  int n = extras_n_colecao(), k;
  if (e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      e->key.keysym.scancode == NV_SCANCODE_BACK) { colListaAberta = 0; return; }
  // A lista e vertical: ESQUERDA nao tinha uso e agora pede a barra lateral
  // por cima, como a borda de qualquer fileira da pagina (dono, 03/10).
  if (k == SDLK_LEFT) { pediuMenu = 1; return; }
  if (k == SDLK_DOWN && colListaFoco + 1 < n) colListaFoco++;
  else if (k == SDLK_UP && colListaFoco > 0) colListaFoco--;
  else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) abrirParteColecao();
}

static void desenhaListaColecao(float a) {
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  const char *fundo = extras_colecao_fundo();
  GLuint tf = fundo[0] ? tex_obter(fundo) : 0;
  int n = extras_n_colecao(), atual = parteAtualColecao(), i;
  float lx = COLL_X, lw = NV_TELA_W - NV_DETP_X - COLL_X;
  float passo = COLL_LIN_H + COLL_LIN_GAP;

  // FUNDO DA LISTA: SEMPRE Frost ou arte borrada, nunca a arte crua ("em
  // colecoes deixa o fundo sempre frost ou blur, ta feio assim", 04/10/2026).
  // Ajustes > Fundo = Frost fica Frost; qualquer outro vira a arte borrada da
  // colecao (fundo.c: a arte desfocada), e sem a arte ainda cai no Frost.
  { int modo = fundo_modo() == FUNDO_FROST ? FUNDO_FROST : FUNDO_BORRADA;
    if (modo == FUNDO_BORRADA && (!fundo[0] || !tf))
      modo = FUNDO_FROST;
    gfx_cor(tela, 0.0f, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, a);
    fundo_desenhar_modo(modo, tela, 0.0f, fundo, a);
  }

  // --- coluna da colecao --------------------------------------------------
  { const char *capa = extras_colecao_capa();
    GfxRect rc = { NV_DETP_X, COLL_TOPO, 240.0f, 360.0f };
    float y;
    char meta[64];
    if (!capa[0]) capa = extras_colecao_poster(0);
    cartazColecao(rc, capa, a);
    y = rc.y + rc.h + 30.0f;
    y += txt_bloco_corta(TXT_TITULO3, extras_colecao_nome(), 250, 251, 255,
                         NV_DETP_X, y, COLL_ESQ_W, 56.0f, a, 3) + 10.0f;
    metaColecao(meta, sizeof meta);
    { TxtLinha lm = txt_linha_corta(TXT_DET_META2, meta, 175, 180, 190, 255, COLL_ESQ_W);
      txt_desenhar_alpha(lm, NV_DETP_X, y, a);
      y += lm.h + 20.0f; }
    if (extras_colecao_sinopse()[0])
      txt_bloco_corta(TXT_DET_META2, extras_colecao_sinopse(), 190, 195, 205,
                      NV_DETP_X, y, COLL_ESQ_W, 32.0f, a * 0.9f, 7);
  }

  // --- partes -----------------------------------------------------------
  for (i = 0; i < n; i++) {
    float ly = COLL_TOPO + (float)i * passo - colListaScroll;
    GfxRect lin = { lx, ly, lw, COLL_LIN_H };
    int foc = (i == colListaFoco);
    float tx = lx + 13.0f + COLL_PO_W + 32.0f, tw = lx + lw - 32.0f - tx, ty;
    if (ly + COLL_LIN_H < 0.0f || ly > NV_TELA_H) continue;
    if (foc) {
      plrui_linha_foco(lin, 20.0f, a);   // a mesma linha em foco dos Ajustes/Agenda
    } else {
      moldura(lin, 20.0f, a * 0.55f);
    }
    { GfxRect rp = { lx + 13.0f, ly + (COLL_LIN_H - COLL_PO_H) * 0.5f,
                     COLL_PO_W, COLL_PO_H };
      if (foc) { colFocoRect = rp; snprintf(colFocoArte, sizeof colFocoArte, "%s", extras_colecao_poster(i)); }
      cartazColecao(rp, extras_colecao_poster(i), a); }
    ty = ly + 30.0f;
    { int c = foc ? 255 : 232;
      float wSelo = 0.0f;
      TxtLinha lsel = { 0 };
      if (i == atual) {
        lsel = txt_linha(TXT_CAPTION2, "Você está aqui", 0, 0, 0, 255);
        wSelo = lsel.w + 28.0f + 16.0f;
      }
      { TxtLinha lt = txt_linha_corta(TXT_ROW_TITULO, extras_colecao_titulo(i),
                                      c, c, c, 255, tw - wSelo);
        txt_desenhar_alpha(lt, tx, ty, a);
        if (i == atual) {
          float ar, ag, ab;
          int tin;
          GfxRect pil = { tx + lt.w + 16.0f, ty + (lt.h - 36.0f) * 0.5f,
                          lsel.w + 28.0f, 36.0f };
          ajustes_acento(&ar, &ag, &ab);
          tin = ajustes_tinta_foco();
          gfx_cor(pil, 0.5f, ar, ag, ab, a);
          lsel = txt_linha(TXT_CAPTION2, "Você está aqui", tin, tin, tin, 255);
          txt_desenhar_alpha(lsel, pil.x + 14.0f, pil.y + (pil.h - lsel.h) * 0.5f, a);
        }
        ty += lt.h + 12.0f; } }
    // Ano e nota do TMDB na mesma linha, a estrela no amarelo das notas.
    { const char *ano = extras_colecao_ano(i);
      int nota = extras_colecao_nota(i);
      float mx = tx;
      if (ano[0]) {
        TxtLinha la = txt_linha(TXT_DET_META2, ano, 175, 180, 190, 255);
        txt_desenhar_alpha(la, mx, ty, a);
        mx += la.w + 22.0f;
      }
      if (nota > 0) {
        char nb[8];
        GfxRect ic = { mx, ty + 3.0f, 22.0f, 22.0f };
        TxtLinha ln;
        snprintf(nb, sizeof nb, idioma_ponto_decimal(ajustes_idioma()) ? "%d.%d" : "%d,%d", nota / 10, nota % 10);
        ln = txt_linha(TXT_DET_META2, nb, 175, 180, 190, 255);
        gfx_icone(ic, "aj_star", 0.96f, 0.77f, 0.09f, a);
        txt_desenhar_alpha(ln, mx + 30.0f, ty, a);
      }
      ty += 40.0f; }
    if (extras_colecao_sinopse_parte(i)[0])
      txt_bloco_corta(TXT_DET_META2, extras_colecao_sinopse_parte(i),
                      foc ? 205 : 170, foc ? 210 : 175, foc ? 220 : 185,
                      tx, ty, tw, 31.0f, a, 3);
  }
}

// Aba "Comentarios": /comments/likes do Trakt, os mais curtidos primeiro. Uma
// linha com o usuario e as curtidas, e o texto quebrado embaixo.
// Comentarios em CARTOES lado a lado, com a mesma moldura dos cartoes de nota,
// para nao ficarem como texto solto no meio de uma tela feita de componentes.
// Cartao de comentario NO FORMATO DA REFERENCIA. MEDIDO na TCL: 722x466, vao
// de 25, canto ~20. O que havia aqui era 560x240 com o nome e as curtidas na
// MESMA linha — o cartao cabia tres linhas de texto e cortava o resto, e a nota
// de quem comentou nao aparecia.
//
// A referencia separa em tres blocos, e a ordem importa: NOME sozinho no topo,
// TEXTO no meio ocupando o que sobra, e um rodape "10/10  17 curtidas" colado
// na base. Ler o nome, decidir se interessa e so entao ler — nessa ordem.
// Cabecalho da secao, como na referencia: o WORDMARK do trakt, "Comentários" ao
// lado, "Avaliações do Trakt" abaixo, e o seletor "Série | Episódio".
//
// Nada disto existia — os cartoes apareciam soltos, sem dizer de onde vinham
// nem que havia dois conjuntos. O seletor nao e enfeite: comentario de EPISODIO
// e outra consulta no Trakt, e sem ele metade do conteudo era inalcancavel.
#define COM_PILL_H    64.0f
#define COM_PILL_PAD  34.0f
// Altura do cabecalho, medida do mesmo jeito que cabecalhoComentarios a
// percorre: 46 do titulo + 44 do subtitulo + as pilulas (so em serie) + 28 de
// respiro. Vem de uma funcao e nao de uma constante justamente porque a de
// filme e menor — cravar um numero so faria uma das duas ficar errada.
// BASE DA ABA ATIVA na serie: o y ABSOLUTO onde o conteudo do slot de
// SEC_ELENCO termina.
//
// Existe porque esse slot desenha coisas de alturas MUITO diferentes conforme a
// aba: o elenco tem ~230, mas "Mais como este" tem cartaz de 318 mais o rotulo.
// A secao do Trakt era empilhada a partir da altura do ELENCO sempre, entao ao
// escolher "Recomendações" os cartazes desciam por cima dela. Nao da para usar
// uma altura so: usar a maior afastaria o Trakt do elenco sem motivo, e usar a
// menor e o defeito que o dono viu.
static float baseDaAbaAtiva(void) {
  // Elenco e desenhado no proprio epElencoY(); as outras abas em EL_Y + 40
  // (o `yAba` de desenhaSecao). Sao dois pontos de partida diferentes.
  switch (abaIdDe(abaInfo)) {
    case ABA_RELACIONADOS:
      return epElencoY() + 40.0f + REL_CARD_H + 12.0f
           + NV_DETP_EL_LINHA * 2.0f;          // titulo + ano sob o cartaz
    case ABA_AVALIACOES:
      if (ehSerie() && extras_n_temporadas() > 0)
        return epElencoY() + 40.0f + RAT_TEMP_H + 18.0f + RAT_PIL_H;
      return epElencoY() + 40.0f + AVAL_CARD_H;
    case ABA_COLECAO:
      return epElencoY() + 40.0f + COL_CARD_H;
    default:
      return epElencoY() + NV_DETP_EL_AVATAR + NV_DETP_EL_NOME_DY
           + NV_DETP_EL_PAPEL_DY + NV_DETP_EL_LINHA * 2.0f;
  }
}

static float alturaCabComentarios(void) {
  return NV_DETF_CAB_H + NV_DETF_CAB_GAP + (ehSerie() ? COM_PILL_H + 28.0f : 0.0f);
}

// Rotulos do seletor, em escopo de arquivo: a contagem de colunas e a largura
// de item precisam deles fora do desenho.
static const char *COM_ROT[2] = { "Série", "Episódio" };
// A pilula do episodio DIZ qual episodio: "Episódio · T1E3". Sem isso a pessoa
// nao tem como saber que os cartoes seguem o episodio em foco na fileira la de
// cima (pedido do dono, #78). O texto ja sai traduzido pedaco a pedaco porque
// a linha montada nao esta na tabela de idioma.
static const char *rotuloPilulaCom(int k) {
  static char buf[64];
  int t = 0, ep = 0;
  if (k != 1) return COM_ROT[0];
  if (episodioDosComentarios(&t, &ep) && t > 0 && ep > 0) {
    char te[24];
    snprintf(te, sizeof te, i18n("T%dE%d"), t, ep);
    snprintf(buf, sizeof buf, "%s  \xc2\xb7  %s", i18n(COM_ROT[1]), te);
    return buf;
  }
  return COM_ROT[1];
}

// A fileira de comentarios tem DUAS naturezas em sequencia: as pilulas do
// seletor (so em serie) e, depois delas, os CARTOES.
//
// Os cartoes precisavam virar colunas: eles eram desenhados tres e ponto, sem
// foco, entao os outros cinco que o Trakt manda (EX_COMENT_MAX = 8) eram
// inalcancaveis — foi o "nao tava dando pra navegar nos comentarios".
static int nPilulasCom(void) { return ehSerie() ? 2 : 0; }
static int nCartoesCom(void) {
  int n = (ehSerie() && comentEp) ? extras_n_comentarios_ep()
                                 : extras_n_comentarios();
  return n > EX_COMENT_MAX ? EX_COMENT_MAX : n;
}

static float larguraPilulaCom(const char *rot) {
  TxtLinha l = txt_linha(TXT_PLR_CORPO, rot, 255, 255, 255, 255);
  return l.w + COM_PILL_PAD * 2;
}

// Desenha o cabecalho e devolve o Y onde os CARTOES comecam.
static float cabecalhoComentarios(float x, float y, float a) {
  float yy = y;
  // O cabecalho .dh do mockup: "O que estão dizendo" em 34/700, o complemento
  // "avaliações no Trakt" a 45% e a contagem a direita a 40%. Era o wordmark
  // do Trakt com o subtitulo embaixo — dois titulos para uma coisa so.
  { int daSerie = !(ehSerie() && comentEp);
    int n = daSerie ? extras_n_comentarios() : extras_n_comentarios_ep();
    TxtLinha lc = txt_linha(TXT_LOG_T34, i18n("O que estão dizendo"), 243, 242, 239, 255);
    TxtLinha ls = txt_linha(TXT_G18R, i18n("avaliações no Trakt"), 243, 242, 239, 115);
    float base = yy + NV_DETF_CAB_H, yl = base - (float)lc.h;
    txt_desenhar_alpha(lc, x, yl, a);
    txt_desenhar_alpha(ls, x + (float)lc.w + 16.0f, yl + (float)lc.h - (float)ls.h - 4.0f, a);
    if (n > 0) {
      char b[12]; snprintf(b, sizeof b, "%d", n);
      { TxtLinha ln = txt_linha(TXT_ILHA_NUM, b, 243, 242, 239, 102);
        txt_desenhar_alpha(ln, NV_TELA_W - NV_DETP_X - (float)ln.w,
                           yl + (float)lc.h - (float)ln.h - 4.0f, a); } }
    yy = base + NV_DETF_CAB_GAP; }

  // As duas pilulas. Em FILME so existe a da serie — nao ha episodio —, entao a
  // fileira inteira some em vez de mostrar um controle morto.
  if (ehSerie()) {
    float px = x;
    int k;
    int fileira = SEC_COMENTARIOS;
    for (k = 0; k < 2; k++) {
      float w = larguraPilulaCom(rotuloPilulaCom(k));
      GfxRect r = { px, yy, w, COM_PILL_H };
      // MEDIDO na referencia: a pilula ESCOLHIDA e BRANCA com texto escuro, e a
      // outra e #2D2D2D com texto branco. E o oposto das pilulas de temporada,
      // onde a escolhida continua escura e so o texto embranquece — sao dois
      // componentes com regras proprias, e eu tinha aplicado a regra errada
      // aqui.
      //
      // Por isso o FOCO nao pode ser a inversao: a inversao ja e o estado
      // "escolhida". Fica o anel branco, que e a outra linguagem de foco do app
      // e nao colide com nada.
      float f = (nivel >= 1 && foco.fileira == fileira && foco.coluna == k)
                ? animFoco[fileira][k] : 0.0f;
      int sel = (comentEp == k);
      // FOCO: anel branco na pilula ESCURA, CRESCIMENTO na pilula branca.
      //
      // Anel branco em volta de preenchimento branco deixa uma folga escura
      // entre os dois, e essa folga e o "halo estranho" — dois brancos
      // separados por uma linha preta, que nao le como foco nem como selecao.
      // Na pilula ja invertida o foco se marca pelo TAMANHO, que e a mesma
      // linguagem medida nos botoes circulares do heroi.
      //
      // REVISTO em 16/09/2026 (regra de NV_COR_FOCO, layout.h): o FOCO e a
      // pilula preenchida na cor de realce com texto escuro, sem anel. A
      // pilula ESCOLHIDA sem foco marca-se pela borda #6e6e6e (3,9:1 sobre
      // o fundo) — "onde voce esta" e "onde esta o foco" sao
      // duas coisas, e branco e so a segunda. Antes a escolhida era branca e
      // a focada tinha anel; com o foco tambem branco seriam dois brancos.
      // CORRIGIDO em 16/09/2026, o dono olhando a captura: "aqui tem que ser
      // fill quando selecionado e nao o contorno". A escolhida-sem-foco era
      // contorno #6e6e6e, e contorno e justamente o que a regra proibe.
      //
      // Os TRES estados viraram tres PREENCHIMENTOS da mesma familia, como a
      // barra de abas da Biblioteca: realce cheio na focada, realce a 60% na
      // escolhida em repouso, #2D2D2D na que nao e nenhuma das duas. Sao tres
      // degraus de brilho bem separados, e nenhum deles e um anel.
      { float cresce = 1.0f + 0.07f * f;
        float dw = r.w * (cresce - 1.0f), dh = r.h * (cresce - 1.0f);
        GfxRect rc = { r.x - dw * 0.5f, r.y - dh * 0.5f, r.w + dw, r.h + dh };
        float lum = 0.176f;                     // #2D2D2D
        foco_anel(rc, NV_RAIO_PILL, f, a);      // so com "Foco no cartaz" ligado
        if (ajustes_vidro()) {
          if (sel) gfx_vidro_painel_acento(rc, NV_RAIO_PILL, 0.5f, a);
          else gfx_vidro_painel(rc, NV_RAIO_PILL, 0.4f, a);
          gfx_vidro_pilula_cheia(rc, NV_RAIO_PILL, f, a);
        } else {
        gfx_cor(rc, NV_RAIO_PILL, lum, lum, lum, a);
        if (f > 0.01f) { float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
                         gfx_cor(rc, NV_RAIO_PILL, ar, ag, ab, f * a); }
        else if (sel) { float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
                        gfx_cor(rc, NV_RAIO_PILL, ar * 0.6f, ag * 0.6f,
                                ab * 0.6f, a); }
        }
        r = rc; }
      // A TINTA DO TEXTO E CALCULADA, nao cravada: a cor de realce e escolha
      // da pessoa nos Ajustes, e com um realce escuro o preto sobre os 60%
      // dele seria o mesmo defeito de contraste ao contrario. Luminancia
      // Rec.709 da superficie que o texto vai pisar, em DEGRAU no meio da
      // mola (f > 0.5) porque a linha ja rasterizada nao muda de cor.
      { float ar, ag, ab, ls; int cor;
        ajustes_acento(&ar, &ag, &ab);
        if (f > 0.5f)   ls = 0.2126f*ar + 0.7152f*ag + 0.0722f*ab;
        else if (sel)   ls = (0.2126f*ar + 0.7152f*ag + 0.0722f*ab) * 0.6f;
        else            ls = 0.176f;
        cor = (ls > 0.55f) ? 17 : 255;
        if (f > 0.5f) cor = ajustes_tinta_foco();   // a tinta do acento (claros: escura)
        if (ajustes_vidro()) cor = f > 0.5f ? gfx_vidro_tinta(1.0f) : 255;   // so o foco e cheio
        { TxtLinha l = txt_linha(TXT_PLR_CORPO, rotuloPilulaCom(k), cor, cor, cor, 255);
          txt_peso(l, r.x + (r.w - l.w) * 0.5f, r.y + (r.h - l.h) * 0.5f, a, 0.5f); } }
      px += w + COM_PILL_GAP;
    }
    yy += COM_PILL_H + 28.0f;
  }
  return yy;
}

static void desenhaComentarios(float x, float y, float a) {
  int daSerie = !(ehSerie() && comentEp);
  int n = daSerie ? extras_n_comentarios() : extras_n_comentarios_ep();
  int i;
  y = cabecalhoComentarios(x, y, a);
  // Carregando e "nao ha" sao a MESMA lista vazia; sem separar os dois o
  // episodio parecia nunca ter comentario nenhum.
  if (n == 0) {
    const char *msg = (!daSerie && extras_comentarios_ep_carregando())
                    ? "Carregando comentários…"
                    : "Nenhum comentário ainda.";
    TxtLinha l = txt_linha(TXT_DET_META2, msg, 150, 154, 163, 255);
    txt_desenhar_alpha(l, x, y, a * 0.9f);
    return;
  }
  // TODOS os cartoes, nao tres: os outros que o Trakt manda ficavam
  // inalcancaveis. Quem limita o que aparece e o recorte lateral abaixo, e quem
  // traz os de fora da tela e a rolagem horizontal da fileira.
  for (i = 0; i < n; i++) {
    float cx = x + i * (COM_CARD_W + COM_CARD_GAP) - scrollSec[SEC_COMENTARIOS];
    GfxRect card = { cx, y, COM_CARD_W, COM_CARD_H };
    int foc = (nivel >= 1 && foco.fileira == SEC_COMENTARIOS &&
               foco.coluna - nPilulasCom() == i);
    float px, larg;
    // Fora da tela dos dois lados: nem desenha. Sao ate 8 cartoes de 722 px, e
    // pintar os que ninguem ve custa preenchimento num aparelho onde ele e o
    // recurso escasso.
    if (cx > NV_TELA_W || cx + COM_CARD_W < 0.0f) continue;
    char rodape[64];
    px = cx + COM_PAD; larg = COM_CARD_W - COM_PAD * 2;
    // FOCO PREENCHIDO, NAO CONTORNADO. Era um anel na cor de realce em volta
    // do cartao; o dono mandou tirar o contorno daqui tambem. O cartao em foco
    // vira a superficie clara #F5F5F5 e as tres linhas de texto invertem —
    // mesma lingua do resto do app (folha de fontes, painel lateral, Agenda).
    //
    // A troca e em DEGRAU e nao interpolada: `txt_linha` guarda a linha ja
    // rasterizada na cor pedida, entao uma cor que varia por quadro seria uma
    // entrada nova de cache por quadro.
    moldura(card, 20.0f, a);
    int tintaEsc = 1;   // tinta escura sobre o foco (realce claro); 0 = clara
    // Vidro: o cartao em foco segue translucido (contorno no realce), entao o
    // texto NAO inverte; `focCor` e o "foco cheio" das cores abaixo.
    const int focCor = foc && !ajustes_vidro();
    if (foc && ajustes_vidro())
      gfx_vidro_foco(card, 20.0f / (COM_CARD_W < COM_CARD_H ? COM_CARD_W : COM_CARD_H), 1.0f, a);
    else if (foc) { float fr, fg, fb; tintaEsc = ajustes_acento_tinta(&fr, &fg, &fb) < 0.5f;
      gfx_cor(card, 20.0f / (COM_CARD_W < COM_CARD_H ? COM_CARD_W
                                                     : COM_CARD_H),
              fr, fg, fb, a); }

    // ETIQUETA DE IDIOMA ("EN") quando o comentario NAO esta no idioma da
    // interface: o texto e de uma pessoa e nao se traduz, mas a pessoa fica
    // sabendo por que ele esta em outra lingua. Os do idioma dela vem primeiro
    // (extras.c) e nao levam etiqueta.
    const char *lingCom = daSerie ? extras_comentario_lingua(i)
                                  : extras_comentario_ep_lingua(i);
    float larguraTag = 0.0f;
    if (lingCom[0]) {
      char tag[8]; size_t q;
      for (q = 0; q < sizeof tag - 1 && lingCom[q]; q++)
        tag[q] = (char)((lingCom[q] >= 'a' && lingCom[q] <= 'z') ? lingCom[q] - 32 : lingCom[q]);
      tag[q] = 0;
      { TxtLinha lt = txt_linha(TXT_CAPTION2, tag,
                                foc ? (tintaEsc ? 88 : 215) : 132, foc ? (tintaEsc ? 90 : 216) : 138,
                                foc ? (tintaEsc ? 96 : 222) : 150, 255);
        txt_desenhar_alpha(lt, px + larg - lt.w, y + COM_PAD + 6.0f, a * 0.9f);
        larguraTag = lt.w + 14.0f; }
    }
    { TxtLinha lu = txt_linha_corta(TXT_ROW_TITULO,
                                    daSerie ? extras_comentario_usuario(i)
                                            : extras_comentario_ep_usuario(i),
                                    focCor ? (tintaEsc ? 17 : 255) : 238, focCor ? (tintaEsc ? 17 : 255) : 241,
                                    focCor ? (tintaEsc ? 20 : 255) : 248, 255, larg - larguraTag);
      txt_desenhar_alpha(lu, px, y + COM_PAD, a); }

    // O texto para ANTES do rodape: sem o teto de linhas ele passava por cima
    // das curtidas. 6 linhas de 30 terminam em 246; o rodape comeca em 288.
    txt_bloco(TXT_DET_META2, daSerie ? extras_comentario_texto(i)
                                     : extras_comentario_ep_texto(i),
              focCor ? (tintaEsc ? 45 : 235) : 190, focCor ? (tintaEsc ? 47 : 236) : 195, focCor ? (tintaEsc ? 52 : 240) : 205,
              px, y + COM_PAD + 42.0f, larg, 30.0f, a * 0.95f, 6);

    { int nota = daSerie ? extras_comentario_nota(i)
                         : extras_comentario_ep_nota(i);
      int cur  = daSerie ? extras_comentario_curtidas(i)
                         : extras_comentario_ep_curtidas(i);
      if (nota > 0)
        snprintf(rodape, sizeof rodape, i18n("%d/10   %d curtidas"), nota, cur);
      else
        snprintf(rodape, sizeof rodape, i18n("%d curtidas"), cur);
      { TxtLinha lr = txt_linha(TXT_CAPTION2, rodape,
                                foc ? (tintaEsc ? 88 : 215) : 132, foc ? (tintaEsc ? 90 : 216) : 138,
                                foc ? (tintaEsc ? 96 : 222) : 150, 255);
        txt_desenhar_alpha(lr, px, y + COM_CARD_H - COM_PAD - lr.h, a * 0.9f); } }
  }
}

// --- AS DUAS SECOES SOB DEMANDA ---------------------------------------------
//
// A CHAMADA: o que a secao mostra antes de alguem entrar nela. Tres linhas na
// MESMA tipografia e nos MESMOS vaos que o cabecalho dos dois modulos usa
// (TXT_HEADLINE, depois TXT_DET_META2), para o titulo nao dar um pulo quando o
// painel de verdade nascer por cima dela — so o corpo aparece embaixo.
//
// A terceira linha diz o CUSTO. Nao e desculpa tecnica vazada para a tela: e a
// resposta a pergunta que o desenho levanta sozinho ("por que isto esta vazio
// se tudo o mais ja carregou?"). O mesmo que a secao de comentarios responde
// com o seletor "Série | Episódio", que tambem so busca quando escolhido.
static float desenhaChamada(float x, float y, const char *titulo,
                            const char *fonte, const char *custo, float a) {
  // No Glass UI o titulo e o do cabecalho de secao (34/700) e as duas linhas
  // de apoio em 18 a 55% e 40%.
  TxtLinha lt = txt_linha(TXT_LOG_T34, i18n(titulo), 243, 242, 239, 255);
  TxtLinha lf = txt_linha_corta(TXT_G18R, i18n(fonte), 243, 242, 239, 140,
                                NV_TELA_W - NV_DETP_X * 2);
  TxtLinha lc = txt_linha_corta(TXT_G18R, i18n(custo), 243, 242, 239, 102,
                                NV_TELA_W - NV_DETP_X * 2);
  txt_desenhar_alpha(lt, x, y, a);
  txt_desenhar_alpha(lf, x, y + lt.h + 6.0f, a * 0.95f);
  txt_desenhar_alpha(lc, x, y + lt.h + 6.0f + lf.h + 10.0f, a * 0.9f);
  return lt.h + 6.0f + lf.h + 10.0f + lc.h;
}

// NUMEROS DA TEMPORADA (so serie): os tres cartoes de serieaud_bloco, com o
// episodio em foco = a coluna focada (ou a ultima lembrada, com o foco fora).
static void desenhaNumeros(float x, float y, float a) {
  SaBloco b;
  int t = audTemp();
  if (t < 0) return;
  memset(&b, 0, sizeof b);
  b.temporada = extras_temporada_numero(t);
  b.tempIdx = t;
  b.sel = (nivel >= 1 && foco.fileira == SEC_NUMEROS) ? foco.coluna
                                                      : foco.colunaLembrada[SEC_NUMEROS];
  if (b.sel < 0 || b.sel >= secaoN(SEC_NUMEROS)) b.sel = 0;
  b.selEp = extras_ep_numero(t, b.sel);
  b.aberto = audAberta && audTempAberta == b.temporada;
  b.notas = notasDados();
  serieaud_bloco(x, y, &b, a);
}

// FRASES A ESQUERDA, FICHA A DIREITA. Sao duas fontes diferentes (Wikiquote e
// Wikidata) com coberturas MUITO diferentes — 11 de 14 filmes tem frases, 2 de
// 12 series; a ficha do Wikidata quase sempre tem algum campo —, e e por isso
// que elas ficam lado a lado e nao uma sob a outra: a coluna da direita e o que
// impede a secao de ser uma tela inteira com uma linha de "nao tem" no meio.
static float desenhaFrases(float x, float y, float a) {
  // 1.7.4: o componente antigo (frases | ficha de producao), em filme e serie.
  GfxRect q = { x, y, FR_COL_W, 0.0f };
  GfxRect f = { x + FR_COL_W + FR_COL_GAP, y,
                NV_TELA_W - NV_DETP_X * 2 - FR_COL_W - FR_COL_GAP, 0.0f };
  float hq, hf;
  if (!frasesAberta)
    return desenhaChamada(x, y, "Frases e ficha de produção",
                          "Wikiquote e Wikidata — nada escrito por nós",
                          "Duas consultas: carrega quando você desce até aqui",
                          a);
  hq = seriefrases_desenhar(q);
  hf = seriefrases_desenhar_fatos(f);
  return hq > hf ? hq : hf;
}

// PONTEIRO (#99). r < 0 = botao `c` do hero (nivel 0); senao secao r, coluna
// c (nivel 1) — as mesmas variaveis que as setas mexem em detail_evento.
static void ponteiroDetalhe(int r, int c) {
  if (r < 0 && c == DET_PTR_AMIGOS) { nivel = 0; focoAmigos = 1; return; }
  if (r < 0) {
    focoAmigos = 0;
    if (c < 0 || c >= nBotoes()) return;
    nivel = 0; botao = c;
    return;
  }
  if (r >= N_SECOES || c < 0 || c >= secaoColunas(r)) return;
  nivel = 1;
  foco.fileira = r; foco.coluna = c; foco.colunaLembrada[r] = c;
}
// Altura da area clicavel de um item da secao; 0 = secao que o ponteiro nao
// foca por item (desenho unico, lista interna com foco proprio).
static float alturaAlvo(int r) {
  switch (r) {
    case SEC_TEMPORADAS: return NV_DETP_TEMP_H;
    case SEC_EPISODIOS:  return epCardH();
    case SEC_ABAS_INFO:  return NV_DETP_ABA_H;
    case SEC_TRAILERS:   return NV_DETF_TR_VIDEO_H;
    case SEC_ESTUDIOS:   return EST_CARD_H;
    case SEC_RELACIONADOS: return REL_CARD_H + 72.0f;
    case SEC_COLECAO:    return COL_CARD_H;
    case SEC_ELENCO:     return NV_DETP_EL_AVATAR + 90.0f;
    default:             return 0.0f;
  }
}

// CABECALHO DE SECAO do Glass UI (.dh do mockup): o nome em 34/700, um
// complemento em 17 a 45% logo depois ("e equipe") e a contagem a direita em
// 16 a 40%. `base` e a BASE da linha (o conteudo comeca NV_DETF_CAB_GAP abaixo).
static void cabecalhoSecao(int r, const char *cab, float x, float base, float a) {
  TxtLinha lc = txt_linha(TXT_LOG_T34, cab, 243, 242, 239, 255);
  float yl = base - (float)lc.h;
  const char *comp = r == SEC_ELENCO ? "e equipe" : r == SEC_FRASES ? "Wikiquote" : NULL;
  txt_desenhar_alpha(lc, x, yl, a);
  if (comp) {
    TxtLinha ls = txt_linha(TXT_G18R, i18n(comp), 243, 242, 239, 115);
    txt_desenhar_alpha(ls, x + (float)lc.w + 16.0f, yl + (float)lc.h - (float)ls.h - 4.0f, a);
  }
  { int n = r == SEC_TRAILERS ? secaoN(r) : 0;
    if (n > 0) {
      char b[12]; snprintf(b, sizeof b, "%d", n);
      { TxtLinha ln = txt_linha(TXT_ILHA_NUM, b, 243, 242, 239, 102);
        txt_desenhar_alpha(ln, NV_TELA_W - NV_DETP_X - (float)ln.w,
                           yl + (float)lc.h - (float)ln.h - 4.0f, a); }
    } }
}

// D-pad vertical na pagina: na serie e o enum (focus_mover); no FILME e a
// ORDEM_FILME, com a mesma regra de pular fileira sem coluna e de lembrar a
// coluna de cada fileira. Devolve 1 se moveu.
static int moverFileira(int dy) {
  int o, pos = -1;
  const int *ordem = ordemSecoes();
  for (o = 0; o < N_ORDEM; o++) if (ordem[o] == foco.fileira) pos = o;
  if (pos < 0) return focus_mover(&foco, 0, dy);
  for (o = pos + dy; o >= 0 && o < N_ORDEM; o += dy) {
    int r = ordem[o];
    // O par da mesma linha (Colecao | Frases, Ficha | Producao) e esquerda/
    // direita, nao cima/baixo.
    if (parEsq(r) == foco.fileira || parDir(r) == foco.fileira) continue;
    if (foco.nColunas[r] > 0) {
      int alvo = foco.colunaLembrada[r];
      foco.colunaLembrada[foco.fileira] = foco.coluna;
      if (alvo >= foco.nColunas[r]) alvo = foco.nColunas[r] - 1;
      if (alvo < 0) alvo = 0;
      foco.fileira = r; foco.coluna = alvo;
      return 1;
    }
  }
  return 0;
}

// DE ONDE O MENU DE VISTO SAI (episodios.h): a caixa, neste quadro, do card do
// episodio ou da aba da temporada em foco. O menu se ancora nela e, depois que
// ele pinta o veu (episodio) ou a superficie do painel (temporada), a pagina
// redesenha a peca por cima com a MESMA funcao da fileira — o card ao lado da
// ilha, a aba de cabecalho do painel. Em pixels reais; o menu mede pela tela
// virtual (escala.h) e a conversao e uma divisao.
static GfxRect menuOrigem;
static int     menuOrigemCol;
static float   menuOrigemF, menuOrigemA;
static void desenhaEpisodio(GfxRect r, int c, float f, float a, Uint32 agora);
static void menuDeVisto(Uint32 agora) {
  if (!episodios_menu_visivel()) { episodios_menu_desenhar(); return; }
  if (menuOrigem.w > 8.0f && (foco.fileira == SEC_EPISODIOS || foco.fileira == SEC_TEMPORADAS)) {
    float e = gfx_escala_ui();
    episodios_menu_ancora((GfxRect){ menuOrigem.x / e, menuOrigem.y / e, menuOrigem.w / e, menuOrigem.h / e });
  }
  episodios_menu_desenhar();
  if (menuOrigem.w <= 8.0f) return;
  if (episodios_menu_painel()) desenhaTemporada(menuOrigem, menuOrigemCol, menuOrigemF, menuOrigemA);
  else if (foco.fileira == SEC_EPISODIOS)
    desenhaEpisodio(menuOrigem, menuOrigemCol, menuOrigemF, menuOrigemA * anim_clamp(episodios_menu_anim(), 0.0f, 1.0f), agora);
  menuOrigem.w = 0.0f;   // so vale a caixa do quadro em que a peca foi desenhada
}

static void desenhaSecao(int r, float a, Uint32 agora) {
  int n = secaoN(r);
  // Aba de informacao que nao seja "Criador e elenco": o web TROCA o conteudo
  // da secao (avaliacoes por episodio, fileira de similares, trailer). Nenhum
  // desses dados existe no catalogo nativo, e o web mostra exatamente esta
  // linha quando o dado falta (`.series-insight-empty`).
  //
  // Trocar, e nao sobrepor: na primeira captura do aparelho a mensagem saia POR
  // CIMA dos avatares do elenco, e as duas coisas ficavam ilegiveis.
  { int aba = abaIdDe(abaInfo);
    float yAba = epElencoY() - scrollY + 40.0f;
    if (r == SEC_ELENCO && aba == ABA_AVALIACOES) {
      // Serie com notas por episodio mostra o painel do web; o resto (filme, ou
      // serie sem essa fonte) cai nos cartoes de nota.
      if (ehSerie() && extras_n_temporadas() > 0)
        desenhaNotasEpisodio(NV_DETP_X, yAba, a);
      else
        desenhaAvaliacoes(NV_DETP_X, yAba, a);
      return;
    }
    if (r == SEC_ELENCO && aba == ABA_RELACIONADOS) {
      desenhaRelacionados(NV_DETP_X, yAba, a, 0, 7); return;
    }
    if (r == SEC_ELENCO && aba == ABA_COLECAO) {
      desenhaColecao(NV_DETP_X, yAba,
                     foco.fileira == SEC_ELENCO ? animFoco[SEC_ELENCO][0] : 0.0f, a);
      return;
    }
    if (r == SEC_ELENCO && aba == ABA_COMENTARIOS) {
      desenhaComentarios(NV_DETP_X, yAba, a); return;
    } }
  if (n <= 0) return;
  // FILME le o layout empilhado; SERIE mantem as coordenadas medidas. Note que
  // na serie o topo do GRUPO e o y de DESENHO sao numeros diferentes (o grupo
  // de temporadas comeca em 1080 e a pilula e desenhada em 1160), por isso as
  // duas constantes coexistem em vez de uma sair da outra.
  float y;
  if (!ehSerie()) y = conteudoSec[r];
  else switch (r) {
    case SEC_TEMPORADAS: y = NV_DETP_TEMP_Y; break;
    case SEC_EPISODIOS:  y = NV_DETP_EP_Y;   break;
    case SEC_ABAS_INFO:  y = epAbasY();  break;
    // A SECAO DO TRAKT E EMPILHADA, nao medida: ela vem DEPOIS do elenco e a
    // altura do elenco varia (nome comprido quebra em duas linhas). O `default`
    // abaixo mandava ela para epElencoY(), que e o y do PROPRIO elenco — por
    // isso ela era desenhada por cima dos avatares.
    //
    // Consertar o recalcularLayout nao bastou: aquilo governa foco e rolagem, e
    // este switch e quem escolhe onde DESENHAR. Eram dois numeros para o mesmo
    // lugar, e so um deles tinha sido corrigido.
    // Estudios e empilhado como os comentarios: vem DEPOIS da fileira da aba,
    // e o `default` mandaria ele para o y do elenco — por cima dos avatares.
    case SEC_AUD_ARCO:
    case SEC_AUD_RADAR:
    case SEC_AUD_DIGITAL:
    case SEC_NOTAS:
    case SEC_NOTAS_EP:
    case SEC_PROGTEMP:
    case SEC_TRAILERS:
    case SEC_RELACIONADOS:
    case SEC_FRASES:
    case SEC_COMENTARIOS:
    case SEC_ESTUDIOS:    y = conteudoSec[r]; break;
    default:             y = epElencoY();   break;
  }
  y -= scrollY;

  // TITULO DA SECAO ("Temporadas", "Elenco"), como a referencia. O port nao
  // tinha cabecalho nenhum e as fileiras apareciam soltas, sem dizer o que
  // eram. Fica ACIMA da fileira e some junto com ela na rolagem.
  // So "Temporadas". A fileira de elenco ja e rotulada pela ABA acima dela
  // ("Criador e elenco"), e um cabecalho "Elenco" logo abaixo dela dizia a
  // mesma coisa duas vezes — na primeira tentativa os dois ainda se
  // sobrepunham.
  // Na SERIE so "Temporadas": a fileira de elenco ja e rotulada pela aba
  // "Criador e elenco" logo acima, e um cabecalho "Elenco" abaixo dela dizia a
  // mesma coisa duas vezes. No FILME nao ha abas, entao cada secao carrega o
  // proprio nome — que e o que torna a pagina legivel sem a barra.
  //
  // Na SERIE nao ha cabecalho NENHUM. Havia "Temporadas" acima da fileira de
  // pilulas; a referencia no aparelho nao tem: a pilula "Temporada 1" ja diz o
  // que a fileira e, e o rotulo acima dela repetia a palavra duas vezes em
  // linhas seguidas. No FILME cada secao continua carregando o proprio nome,
  // porque la nao existe a barra de abas para dizer o que e o que.
  { const char *cab = cabecalhoDe(r);   // na serie: Trailers e Estudios
    if (cab) {
      TxtLinha lc = txt_linha(TXT_HEADLINE, cab, 245, 248, 255, 255);
      txt_desenhar_alpha(lc, NV_DETP_X, y - lc.h - NV_DETF_CAB_GAP, a);
    } }
  { float alt = alturaSecao(r);
    if (y > NV_TELA_H || y + alt < -40.0f) return; }

  // O resumo da temporada ESCOLHIDA, acima das pilulas. Depois do culling (ele
  // desenha acima de `y`, dentro da mesma faixa) e antes do laco de colunas,
  // porque nao pertence a nenhuma pilula: e uma linha por FILEIRA. Em filme
  // secaoN(SEC_TEMPORADAS) ja devolveu 0 e nao se chega aqui.
  if (r == SEC_TEMPORADAS) resumoTemporada(NV_DETP_X, y, a);
  // O grafico e UM desenho; as colunas focaveis sao posicoes dentro dele.
  if (r == SEC_PROGTEMP) {
    tgraf_desenhar(tgraf_dados(idx), NV_DETP_X, y, NV_TELA_W - NV_DETP_X * 2,
                   nivel >= 1 && foco.fileira == SEC_PROGTEMP ? foco.coluna : -1,
                   temporadaEm(temporada), a, agora);
    return;
  }

  for (int c = 0; c < n && c < N_ITENS; c++) {
    float f = animFoco[r][c];
    float x = xItem(r, c) - scrollSec[r];
    float w = larguraItem(r, c);
    if (x > NV_TELA_W || x + w < -w) continue;
    if (ponteiro_ativo() && a > 0.3f && c < secaoColunas(r) && alturaAlvo(r) > 0.0f)
      ponteiro_alvo(x, y,
                    w, alturaAlvo(r), ponteiroDetalhe, NULL, r, c);
    switch (r) {
      case SEC_TEMPORADAS: {
        GfxRect b = { x, y, w, NV_DETP_TEMP_H };
        if (foco.fileira == r && c == foco.coluna) { menuOrigem = b; menuOrigemCol = c; menuOrigemF = f; menuOrigemA = a; }
        desenhaTemporada(b, c, f, a); break;
      }
      case SEC_EPISODIOS: {
        GfxRect b = { x, y, epCardW(), epCardH() };
        if (foco.fileira == r && c == foco.coluna) { menuOrigem = b; menuOrigemCol = c; menuOrigemF = f; menuOrigemA = a; }
        desenhaEpisodio(b, c, f, a, agora); break;
      }
      case SEC_ABAS_INFO: {
        desenhaAbaInfo(x, y, c, f, a);
        if (c + 1 < n) {
          TxtLinha d = txt_linha(TXT_PLR_CORPO, "|", 128, 128, 128, 255);
          txt_peso(d, x + w + NV_DETP_ABA_SEP,
                   y + (NV_DETP_ABA_H - d.h) * 0.5f, a, 1.4f);
        }
        break;
      }
      case SEC_TRAILERS: desenhaTrailer(x, y, c, f, a); break;
      // Reaproveitam o desenho que ja servia as ABAS da serie: e o mesmo
      // conteudo, so que agora numa secao propria em vez de atras de uma aba.
      case SEC_RELACIONADOS:
        if (relacionadosCarregando()) {
          if (c == 0) desenhaEsqueletoRelacionados(y, a);
        } else desenhaRelacionados(x, y, a, c, c + 1);
        break;
      case SEC_COLECAO:
        if (c == 0) desenhaColecao(x, y, f, a);
        break;
      case SEC_COMENTARIOS:  desenhaComentarios(NV_DETP_X, y, a); break;
      // UMA VEZ SO, e nao uma por coluna: as colunas destas duas sao posicoes
      // dentro de um desenho unico (o episodio em destaque, a frase em
      // destaque), como os cartazes de "Mais como este" no filme. A altura
      // medida volta para o empilhamento do proximo quadro.
      case SEC_AUD_ARCO:
        if (c == 0) desenhaNumeros(NV_DETP_X, y, a);
        break;
      case SEC_FRASES:
        if (c == 0) frasesAlt = desenhaFrases(NV_DETP_X, y, a);
        break;
      case SEC_NOTAS:
        if (c == 0) notasui_fontes_desenhar(notasDados(), NV_DETP_X, y, a, animFoco[SEC_NOTAS]);
        break;
      case SEC_ESTUDIOS:
        desenhaEstudio(x, y, c, f, a);
        break;
      case SEC_DETALHES: desenhaDetalhes(x, y, f, a); break;
      default: desenhaElenco(x, y, c, f, a); break;
    }
  }

}

// A estrutura da pagina aparece enquanto o Cinemeta responde. Nao entra em
// secaoN(): esqueleto nao recebe foco nem inventa itens. Ele ocupa exatamente
// as coordenadas finais de temporadas/episodios, de modo que a resposta apenas
// preenche os blocos e nao desloca a pagina sob o controle remoto.
static void desenhaEsqueletoEpisodios(float a) {
  int c;
  float yt, ye;
  if (!ehSerie() || cat_n_episodios(idx) > 0 ||
      !desc_episodios_carregando(idx)) return;
  yt = NV_DETP_TEMP_Y - scrollY;
  ye = NV_DETP_EP_Y - scrollY;

  if (yt < NV_TELA_H && yt + NV_DETP_TEMP_H > 0) {
    for (c = 0; c < 3; c++) {
      float w = c == 0 ? 238.0f : 214.0f;
      GfxRect p = { NV_DETP_X + c * 276.0f, yt, w, NV_DETP_TEMP_H };
      gfx_cor(p, 0.5f, 0.17f, 0.18f, 0.20f, a * 0.62f);
    }
  }
  if (ye < NV_TELA_H && ye + epCardH() > 0) {
    for (c = 0; c < 3; c++) {
      float x = NV_DETP_X + c * epCardPasso();
      GfxRect card = { x, ye, epCardW(), epCardH() };
      GfxRect selo = { x + NV_DETP_EP_PAD, ye + NV_DETP_EP_SELO_Y,
                       108.0f, NV_DETP_EP_SELO_H };
      GfxRect titulo = { x + NV_DETP_EP_PAD, ye + NV_DETP_EP_TIT_Y,
                         292.0f, 25.0f };
      GfxRect sin1 = { x + NV_DETP_EP_PAD, ye + NV_DETP_EP_SIN_Y,
                       NV_DETP_EP_TEXTO_W, 18.0f };
      GfxRect sin2 = { sin1.x, sin1.y + NV_DETP_EP_LD_SIN, 420.0f, 18.0f };
      gfx_cor(card, NV_DETP_EP_RAIO / epCardH(),
              0.105f, 0.11f, 0.12f, a * 0.82f);
      gfx_cor(selo, 0.48f, 0.19f, 0.20f, 0.22f, a * 0.70f);
      gfx_cor(titulo, 0.5f, 0.25f, 0.26f, 0.28f, a * 0.62f);
      gfx_cor(sin1, 0.5f, 0.20f, 0.21f, 0.23f, a * 0.52f);
      gfx_cor(sin2, 0.5f, 0.20f, 0.21f, 0.23f, a * 0.52f);
    }
  }
}

// RECOMENDACOES do filme: cinco cartazes e a linha de titulo, nas coordenadas
// finais. Mesma regra do esqueleto de episodios — ocupa exatamente o lugar que
// o conteudo vai ocupar, para a chegada da resposta so preencher.
static void desenhaEsqueletoRelacionados(float y, float a) {
  int c;
  if (y > NV_TELA_H || y + REL_CARD_H < -40.0f) return;
  for (c = 0; c < 5; c++) {
    float x = NV_DETP_X + c * (REL_CARD_W + REL_CARD_GAP);
    GfxRect card = { x, y, REL_CARD_W, REL_CARD_H };
    GfxRect tit  = { x, y + REL_CARD_H + 12.0f, REL_CARD_W * 0.82f, 20.0f };
    GfxRect ano  = { x, y + REL_CARD_H + 40.0f, 62.0f, 15.0f };
    if (x + REL_CARD_W > NV_TELA_W - NV_DETP_X) break;
    gfx_cor(card, raioCartaz(REL_CARD_W, REL_CARD_H), 0.133f, 0.133f, 0.133f, a * 0.82f);
    gfx_cor(tit, 0.5f, 0.22f, 0.23f, 0.25f, a * 0.55f);
    gfx_cor(ano, 0.5f, 0.20f, 0.21f, 0.23f, a * 0.45f);
  }
}

// Mesma ideia para o ELENCO do filme: seis avatares e as duas linhas de texto
// nas coordenadas finais, com o cabecalho "Elenco" no lugar dele.
static void desenhaEsqueletoElenco(float a) {
  if (!elencoCarregando()) return;
  float y = conteudoSec[SEC_ELENCO] - scrollY;
  if (y > NV_TELA_H || y + NV_DETF_EL_ALT < -40.0f) return;
  { TxtLinha lc = txt_linha(TXT_HEADLINE, "Elenco", 245, 248, 255, 255);
    txt_desenhar_alpha(lc, NV_DETP_X, y - lc.h - NV_DETF_CAB_GAP, a); }
  for (int c = 0; c < 6; c++) {
    float x = NV_DETP_X + c * NV_DETP_EL_PASSO;
    GfxRect av = { x + (NV_DETP_EL_W-NV_DETP_EL_AVATAR)*.5f, y, NV_DETP_EL_AVATAR, NV_DETP_EL_AVATAR };
    GfxRect nome = { x, y + NV_DETP_EL_AVATAR + NV_DETP_EL_NOME_DY + 4.0f,
                     c % 2 ? 150.0f : 184.0f, 20.0f };
    GfxRect papel = { x, nome.y + NV_DETP_EL_PAPEL_DY, 110.0f, 16.0f };
    gfx_cor(av, 0.5f, 0.17f, 0.18f, 0.20f, a * 0.62f);
    gfx_cor(nome, 0.5f, 0.22f, 0.23f, 0.25f, a * 0.55f);
    gfx_cor(papel, 0.5f, 0.20f, 0.21f, 0.23f, a * 0.45f);
  }
}

// FICHA DA PESSOA — a tela que o web chama de castDetailScreen. Ocupa a tela
// inteira sobre um fundo opaco, com a foto e a bio a esquerda e a filmografia
// em cartoes de poster a direita. Nao ha layout medido do web para copiar aqui
// (a tela do web e uma pagina rolavel de largura fluida), entao as medidas
// seguem as que esta tela ja usa: gutter de 96, poster de 212x318, cartao com
// o mesmo raio dos outros.

static void desenhaPessoa(float a) {
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  gfx_cor(tela, 0.0f, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, a);

  // Foto pedida pela largura com que desenha (PES_FOTO_W=280, cap 352) — o
  // 640 unico decodificava o retrato inteiro para 280 de cartao. Ver
  // tests/artemenor.c.
  { GLuint t = pessoa_foto()[0] ? tex_obter_larg(pessoa_foto(), PES_FOTO_W) : 0;
    GfxRect r = { NV_DETP_X, 96.0f, PES_FOTO_W, PES_FOTO_H };
    if (t) {
      gfx_tex_aspect_atual = tex_aspecto(pessoa_foto());
      gfx_rect(r, t, GFX_CARD, 0, 0, 0, raioCartaz(PES_FOTO_W, PES_FOTO_H), 0, 0, 0, a);
      gfx_tex_aspect_atual = 0.0f;
    } else {
      gfx_cor(r, raioCartaz(PES_FOTO_W, PES_FOTO_H), 0.13f, 0.13f, 0.13f, a);
    } }

  { float y = 96.0f + PES_FOTO_H + 32.0f;
    TxtLinha ln = txt_linha_corta(TXT_TITULO3, pessoa_nome(), 245, 248, 255, 255,
                                  PES_FOTO_W);
    txt_desenhar_alpha(ln, NV_DETP_X, y, a);
    y += ln.h + 10.0f;
    if (pessoa_area()[0]) {
      TxtLinha la = txt_linha(TXT_DET_META2, pessoa_area(), 150, 154, 163, 255);
      txt_desenhar_alpha(la, NV_DETP_X, y, a * 0.9f);
      y += la.h + 18.0f;
    }
    if (pessoa_bio()[0])
      txt_bloco(TXT_DET_META2, pessoa_bio(), 190, 195, 205, NV_DETP_X, y,
                PES_FOTO_W, 32.0f, a * 0.9f, 6);
  }

  { int n = pessoa_n_creditos(), i;
    float x0 = PES_COL_X;
    TxtLinha lt = txt_linha(TXT_HEADLINE, "Filmografia", 245, 248, 255, 255);
    txt_desenhar_alpha(lt, x0, 96.0f, a);
    for (i = 0; i < n; i++) {
      int col = i % PES_POR_LINHA, lin = i / PES_POR_LINHA;
      float x = x0 + col * (PES_CARD_W + PES_CARD_GAP);
      float y = 96.0f + lt.h + 28.0f + (lin - pessoaLinha) * (PES_CARD_H + 92.0f);
      if (lin < pessoaLinha) continue;
      GfxRect r = { x, y, PES_CARD_W, PES_CARD_H };
      const char *po = pessoa_credito_poster(i);
      GLuint t = po[0] ? tex_obter_larg(po, PES_CARD_W) : 0;
      if (y + PES_CARD_H > NV_TELA_H - 24.0f) break;
      if (i == pessoaFoco) { pesFocoRect = r; snprintf(pesFocoArte, sizeof pesFocoArte, "%s", po); }
      // Foco da Home (focoprof.h): anel so com o ajuste ligado, senao cresce.
      { float fp = i == pessoaFoco ? 1.0f : 0.0f;
        foco_anel(r, raioCartaz(PES_CARD_W, PES_CARD_H), fp, a);
        r = foco_zoom(r, fp); }
      if (t) {
        gfx_tex_aspect_atual = tex_aspecto(po);
        gfx_rect(r, t, GFX_CARD, i == pessoaFoco ? 1.0f : 0.0f, 0, 0,
                 raioCartaz(PES_CARD_W, PES_CARD_H), 0, 0, 0, a);
        gfx_tex_aspect_atual = 0.0f;
      } else {
        gfx_cor(r, raioCartaz(PES_CARD_W, PES_CARD_H), 0.13f, 0.13f, 0.13f, a);
      }
      foco_profundidade(r, raioCartaz(PES_CARD_W, PES_CARD_H), ajustes_profundidade_posters(), a);
      { TxtLinha lc = txt_linha_corta(TXT_DET_META2, pessoa_credito_titulo(i),
                                      230, 234, 242, 255, PES_CARD_W);
        txt_desenhar_alpha(lc, x, y + PES_CARD_H + 12.0f, a);
        { const char *ano = pessoa_credito_ano(i);
          const char *pap = pessoa_credito_papel(i);
          char sub[96];
          snprintf(sub, sizeof sub, "%s%s%s", ano,
                   (ano[0] && pap[0]) ? "  \xc2\xb7  " : "", pap);
          if (sub[0]) {
            TxtLinha ls = txt_linha_corta(TXT_MINI, sub, 140, 144, 153, 255,
                                          PES_CARD_W);
            txt_desenhar_alpha(ls, x, y + PES_CARD_H + 12.0f + lc.h + 6.0f,
                               a * 0.9f);
          } } }
    } }
}


// A ARTE BORRADA NA PAGINA DO TITULO (fundo.c: a arte desfocada) leva um veu
// mais forte que os 28% dos Ajustes: aqui o texto do heroi (titulo, sinopse,
// generos) fica direto sobre ela, e uma arte clara desfocada deixava o texto
// cinza sobre cinza claro (captura de tests/fluidez_perf.sh, cenario borrada).
// Era 55% e na C9 a pagina ficava bem mais escura que os Ajustes com o mesmo
// fundo (dono, 05/10: "nos Ajustes ta melhor"). 36%: um pouco acima dos 28%.
#define DET_VEU_BORRADA 0.36f

// FOLHA + TIRA DE CARTOES do carrossel. A folha e um gfx_cor opaco de tela
// cheia, que gfx_rect transforma em glClear; cada cartao e UM quad opaco
// (GFX_JANELA) com os cantos no SDF. Na abertura a folha sobe sobre a home e
// a janela cresce do cartaz da fileira ate o cartao.
static void carFundo(void) {
  float s = suave(t), raio, passo, veuPag, veu, a;
  GfxRect h = carBuraco(&raio), tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  float expandida = 1.0f - cartao, salvaAmb = nv_ambiente_forca;
  const char *arteAtual = arteDe(idx);
  GLuint texAtual = arteAtual ? tex_obter_hero(arteAtual) : 0;
  int k, modo = fundo_modo();
  int comTrailer = trailerFade > 0.005f && trailer_aberto() && !trailer_cheia();
  int especial = !comTrailer && (modo == FUNDO_FROST ||
      (modo == FUNDO_BORRADA && texAtual && !arteDetalheEhPoster(idx)));
  // A FOLHA FECHA EM UM TERCO DA ABERTURA (e abre so no ultimo terco da
  // volta): e quando ela fica opaca que a home deixa de ser desenhada, e home +
  // folha misturada + cartoes no mesmo quadro passava de 40 ms na C9 (medido,
  // 01/10). O video da Apple faz o mesmo: a home some em dois quadros.
  gfx_cor(tela, 0.0f,
          CAR_FOLHA_R + (NV_COR_FUNDO_R - CAR_FOLHA_R) * expandida,
          CAR_FOLHA_G + (NV_COR_FUNDO_G - CAR_FOLHA_G) * expandida,
          CAR_FOLHA_B + (NV_COR_FUNDO_B - CAR_FOLHA_B) * expandida, carFolha());
  passo = h.w + CAR_VAO;
  a = anim_clamp(s * 3.0f, 0.0f, 1.0f);
  // A folha cinza vira a base da pagina enquanto o cartao cresce. Assim o
  // GFX_JANELA encontra os mesmos pixels sob a arte ao chegar ao detalhe;
  // a pagina assentada continua usando uma unica passada de arte.
  if (expandida > 0.0f) {
    if (especial) {
      fundo_borrada_veu(DET_VEU_BORRADA);
      fundo_desenhar_modo(modo, tela, 0.0f, arteAtual, expandida * a);
      fundo_borrada_veu(-1.0f);
      if (pg > 0.005f) gfx_cor(tela, 0.0f, 0.043f, 0.047f, 0.055f, 0.55f * pg * expandida * a);
      nv_ambiente_forca = 1.0f;
    } else gfx_ambiente(expandida * a);
  }
  veuPag = especial ? 1.0f : (1.0f - pg) * ajustes_detalhe_veu();
  veu = veuPag + (CAR_VEU - veuPag) * cartao;
  gfx_janela_atual[0] = h.x / NV_TELA_W; gfx_janela_atual[1] = h.y / NV_TELA_H;
  gfx_janela_atual[2] = h.w / NV_TELA_W; gfx_janela_atual[3] = h.h / NV_TELA_H;
  for (k = 0; k < carN; k++) {
    GfxRect r = { h.x + ((float)k - carOff) * passo, h.y, h.w, h.h };
    const char *arte;
    GLuint tex;
    if (r.x >= NV_TELA_W || r.x + r.w <= 0.0f) continue;
    arte = arteDe(carIdx[k]);
    tex = arte ? (k == carPos || k == carAplicado ? tex_obter_hero(arte) : tex_obter_hero_quente(arte)) : 0;
    // Os VIZINHOS entram tarde na abertura e saem cedo na volta: por cima da
    // home meio apagada, um cartao grande a meia forca le como borrao.
    { float ak = k == carAplicado ? a : anim_clamp((s - 0.55f) * 2.2f, 0.0f, 1.0f);
      if (ak <= 0.004f) continue;
      // TRAILER NO CARTAO: furo com os cantos do cartao (fora dele a folha
      // opaca fica), a arte apaga por cima com trailerFade quando o primeiro
      // quadro chega, e o veu sozinho (cor r=0) segue por cima do video.
      if (k == carAplicado && trailerFade > 0.005f && trailer_aberto() && !trailer_cheia()) {
        gfx_furo_raio(r, raio / r.h);
        gfx_tex_aspect_atual = tex ? tex_aspecto(arte) : 0.0f;
        if (tex && trailerFade < 0.995f)
          gfx_rect(r, tex, GFX_JANELA, veu, 0.85f * pg, 1.0f - cartao, raio / r.h, 1, 1, 1, ak * (1.0f - trailerFade));
        gfx_rect(r, 0, GFX_JANELA,
                 veu * (1.0f - 0.85f * trailercinema_t(&trailerCinema) * expandida),
                 0.85f * pg, expandida, raio / r.h, 0, 0, 0, ak * trailerFade);
        continue;
      }
      if (!tex) {
        // Frost permanece sem arte; nos demais modos o placeholder converge
        // para o mesmo chao do detalhe, inclusive se a imagem ainda decodifica.
        if (especial) gfx_cor(r, raio / r.h, 0.16f, 0.17f, 0.19f, ak * cartao);
        else gfx_cor(r, raio / r.h,
                     NV_COR_FUNDO_R + (0.16f - NV_COR_FUNDO_R) * cartao,
                     NV_COR_FUNDO_G + (0.17f - NV_COR_FUNDO_G) * cartao,
                     NV_COR_FUNDO_B + (0.19f - NV_COR_FUNDO_B) * cartao, ak);
        continue;
      }
      gfx_tex_aspect_atual = tex_aspecto(arte);
      if (arteDetalheEhPoster(carIdx[k]) && expandida > 0.0f) {
        // O cartaz reserva converge para o mesmo encaixe contido da pagina.
        // Duas artes apenas durante esta transicao; assentado usa o detalhe.
        gfx_rect(r, tex, GFX_JANELA, veu, (especial ? pg : 0.85f * pg), expandida,
                 raio / r.h, 1, 1, 1, ak * cartao);
        desenhaArteDetalhe(r, tex, arte, 1,
                          ak * expandida * (1.0f - (especial ? pg : 0.85f * pg)), pg);
        continue;
      }
      gfx_rect(r, tex, GFX_JANELA, veu, (especial ? pg : 0.85f * pg), expandida,
               raio / r.h, 1, 1, 1, ak * (especial ? 1.0f - 0.002f * expandida : 1.0f)); }
  }
  nv_ambiente_forca = salvaAmb;
  gfx_tex_aspect_atual = 0.0f;
  gfx_janela_atual[0] = gfx_janela_atual[1] = 0.0f;
  gfx_janela_atual[2] = gfx_janela_atual[3] = 1.0f;
}

// "O QUE ACHOU?" AINDA SEM RESPOSTA (reacao.h): uma linha discreta no canto
// inferior direito, so no topo da pagina e sem nada aberto por cima. CIMA na
// linha de botoes abre o cartao (detail_evento); aberto, ele desenha aqui.
static void reacaoPendente(float s) {
  int livre = nivel == 0 && scrollY < 1.0f && !pessoaAberta && !colListaAberta && !amtui_aberta() &&
              !detmais_aberto() &&
              !episodios_menu_aberto() && trocaarte_visivel() < 0.005f;
  if (!livre && !reacao_aberta()) return;
  reacao_detalhe_dica(cat_item(idx), livre ? s : 0.0f);
}

// O FUNDO DA PAGINA DO TITULO, inteiro, numa funcao so.
//
// Tudo o que pinta ATRAS do conteudo passa por aqui: o chao #0D0D0D (quando a
// home ainda aparece por baixo), a arte do titulo com a vinheta, o apagar para
// 15% quando a pagina rola, o furo do trailer de fundo e o carrossel da
// Dinamica. E o caso "Arte" do ajuste Fundo (Arte / Arte borrada / Frost) que
// esta nascendo em outro ramo (fundo.h, fundo_desenhar): quando os dois se
// juntarem, a troca e AQUI e em mais lugar nenhum. O conteudo da pagina nao
// depende da arte nitida para ter contraste — o veu de leitura do heroi
// (veuLeitura) e desenhado por cima, fora desta funcao.
static void detalheFundo(float s) {
  if (!detail_cobre_tela() && !carDesenhaFundo()) {
    GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    gfx_cor(tela, 0.0f, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, s);   // #0d0d0d, o fundo do web
    // Imersiva: a MESMA luz que main.c pinta depois do clear, subindo junto
    // com o fundo. Sem ela, no quadro em que o detalhe passa a cobrir a tela
    // (a home deixa de ser desenhada) a luz de baixo aparecia de uma vez.
    gfx_ambiente(s);
  }
  gfx_sem_recorte();

  // --- backdrop full-bleed --------------------------------------------------
  // A arte ocupa a tela desde o primeiro quadro e so ganha opacidade (o hero
  // da home ja mostrava a mesma arte: o fundo CONTINUA, nao troca). Quem se
  // move sao as fileiras da home, que descem (home_desenhar le
  // detail_progresso).
  //
  // AJUSTES > FUNDO (fundo.h), como no mockup Glass UI (quadro "Detalhe"):
  //   Arte          a arte nitida; ao rolar ela SAI (o mockup rola a arte para
  //                 fora e o resto da pagina fica no #0b0c0e liso);
  //   Arte borrada  a arte desfocada atras de tudo, e a arte nitida no topo
  //                 a direita, vazando nas bordas (a mascara do .hero); ao
  //                 rolar as cores ficam, sob um veu (o .dt-cobre do imersivo);
  //   Frost         a superficie fosca no acento, com a mesma arte no topo.
  // Os tres levam o veu de leitura por cima (veuLeitura, fora daqui).
  GfxRect alvo; float aEntrada;
  backdropRect(&alvo, &aEntrada);
  const char *arte = arteDe(idx);
  if (carDesenhaFundo()) { carFundo(); return; }
  int artePoster = arteDetalheEhPoster(idx);
  int modo = fundo_modo();
  // Com o trailer de fundo tocando, o video e o hero: o caminho de sempre.
  int comTrailer = trailerFade > 0.005f && trailer_aberto() && !trailer_cheia();
  // Backdrop em tela cheia: pede o teto de 1920.
  GLuint tex = arte ? tex_obter_hero(arte) : 0;
  if (pg > 0.01f && !detail_cobre_tela()) {
    GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    gfx_cor(tela, 0.0f, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, pg);
  }
  // Frost independe da textura; a arte e apenas a camada opcional do topo.
  if (modo == FUNDO_ARTE || comTrailer ||
      (modo != FUNDO_FROST && (artePoster || !tex))) {
    desenhaArteDetalhe(alvo, tex, arte, artePoster,
                       tex ? aEntrada * (1.0f - 0.85f * pg) : 1.0f, pg);
    return;
  }
  { GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    fundo_borrada_veu(DET_VEU_BORRADA);
    fundo_desenhar_modo(modo, tela, 0.0f, arte, aEntrada);
    fundo_borrada_veu(-1.0f);
    // Ao rolar: o veu do .dt-cobre do imersivo (50-60%) por cima das cores.
    if (pg > 0.005f) gfx_cor(tela, 0.0f, 0.043f, 0.047f, 0.055f, 0.55f * pg * aEntrada); }
  // A arte nitida do topo, vazando nas bordas (o GFX_DETALHE com o caminho
  // "vazar": alfa = 1 - vinheta, em vez de misturar no #0d0d0d).
  if (tex && (1.0f - pg) * aEntrada > 0.005f) {
    if (artePoster) {
      desenhaArteDetalhe(alvo, tex, arte, 1, aEntrada * (1.0f - pg), pg);
      return;
    }
    float salva = nv_ambiente_forca;
    nv_ambiente_forca = 1.0f;
    gfx_tex_aspect_atual = tex_aspecto(arte);
    // ALFA 1 NO TOPO PARADO: com o fundo (a luz imersiva, ou o Frost adiado,
    // gfx_luz_canal_adiar) ainda intacto embaixo, o GFX_DETALHE le o fundo pelo
    // uAmb e sai OPACO numa passada so — o fundo deixa de ser uma tela pintada
    // e a arte, uma tela misturada por cima (TCL, 2.0.1: topo da pagina a
    // ~30 fps, clr 22 ms + swap 21 ms). O teto de 0,998 que havia aqui
    // obrigava o caminho misturado; sem fundo intacto o caminho continua o
    // misturado (o "vazar"), com alfa 1 em vez de 0,998.
    gfx_rect(alvo, tex, GFX_DETALHE, 1.0f, 0, 0, 0.0f, 0, 0, 0, aEntrada * (1.0f - pg));
    gfx_tex_aspect_atual = 0.0f;
    nv_ambiente_forca = salva;
  }
}

// A ILHA "MAIS OPCOES" por cima da pagina, ancorada no "..." onde ele foi
// desenhado neste quadro. Rolou a pagina (nivel 1) com ela fechando: some.
static void maisDesenhar(void) {
  if (detmais_aberto() && nivel != 0) detmais_fechar();
  detmais_desenhar(maisAncora, 1.0f - trocaarte_visivel());
}

void detail_desenhar(Uint32 agora) {
  if (!aberto) return;
  amigostitulo_atualizar();   // barato: so remonta quando o feed social mudou
  // COR VIVA: a pagina do titulo manda na cor, acima da home que pode estar
  // desenhada por baixo (a prioridade resolve o mesmo quadro). A chave e a
  // MESMA arte que o fundo pede em tela cheia logo abaixo.
  { const char *cv = arteDe(idx), *lg = logoDe(idx);
    if (cv) corviva_definir(cv, CORVIVA_DETALHE);
    if (lg) corviva_definir_logo(lg, CORVIVA_DETALHE); }
  float s = suave(t), a2 = fase2();

  // TRAILER EM TELA CHEIA: a tela inteira e furo, nada da pagina por cima.
  // No .tpk, ate o recorte do zoom assentar (trailer_mostra_video, #178),
  // preto opaco no lugar do furo: o plano ja pode ter o quadro inteiro com
  // tarja, e ele e que nao deve aparecer. Na LG e no .wgt, o furo de sempre.
  if (trailer_cheia()) {
    GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    gfx_sem_recorte();
    if (trailer_mostra_video()) gfx_furo(tela);
    else gfx_cor(tela, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    { const CatItem *ct = cat_item(idx);
      trailer_osd_desenhar(ct ? ct->titulo : "", 1.0f); }
    return;
  }
  detalheFundo(s);
  // O veu de leitura do heroi: fora do fundo, para valer sobre qualquer fundo
  // (arte nitida, borrada ou frost). Sai com a rolagem, com o modo cinema do
  // trailer e no cartao do carrossel (que tem o veu proprio).


  // O hero ROLA com o documento: ele nao some nem e substituido por um
  // cabecalho fixo. Era isso que fazia a pagina do port parecer outra tela em
  // vez da mesma tela rolada.
  // O conteudo SOBE para o lugar enquanto aparece, no lugar de so surgir: e a
  // contraparte do texto da home, que desce e apaga. Junto, le como um bloco
  // trocando de arranjo, que e o que o dono pediu.
  { float c = trailercinema_t(&trailerCinema);
    // Modo cinema: o bloco desce 220 px enquanto apaga; o logo pequeno entra
    // no canto de baixo. As duas molas sao a mesma, entao o cruzamento e limpo.
    // Com "Trocar arte" aberta o texto da pagina sai e fica a arte: o que se
    // escolhe e o fundo, e ele precisa da tela (a tela desenha o logo).
    float ta = trocaarte_visivel();
    // PORTAO (#172): ver gateHero. Ate revelar, o bloco e desenhado com opacidade
    // ~0 so para rasterizar as linhas; `pend` diz quantas o orcamento recusou.
    float gh = textogate_aberto(&gateHero) ? 1.0f : 0.0f;
    { int pend0 = txt_pendentes;
      float k = (1.0f - c) * (1.0f - ta);
      if (gh > 0.0f) {
        float f = textogate_passo(&gateHero, 0, SDL_GetTicks());
        float aCar = 1.0f, dyCar = 0.0f;
        if (carro) {
          // O texto e do titulo MONTADO e anda com o cartao dele na tira:
          // apaga conforme se afasta do centro e entra quando o novo assenta.
          float raio, passo;
          GfxRect hb = carBuraco(&raio);
          passo = hb.w + CAR_VAO;
          // hb.x - CAR_X*cartao e so o voo da abertura (0 assentado); o lugar
          // do texto segue carTxt: expandir leva a coluna para a margem
          // da pagina, junto com a arte.
          heroDx = (hb.x - CAR_X * cartao) + ((float)carAplicado - carOff) * passo +
                   (CAR_X + CAR_TEXTO_PAD - 96.0f) * carTxt;
          dyCar = -CAR_TEXTO_SOBE * carTxt;
          aCar = anim_clamp(1.0f - fabsf(carOff - (float)carAplicado) * 4.0f, 0.0f, 1.0f);
        }
        heroWeb(a2 * k * f * aCar, -scrollY + (1.0f - a2) * NV_TELA_H * 0.05f + c * NV_CINEMA_DESCE + dyCar);
        heroDx = 0.0f;
      } else {
        heroWeb(NV_TXTGATE_AQUECER, -scrollY + NV_TELA_H * 0.05f);
        textogate_passo(&gateHero, txt_pendentes - pend0,
                        SDL_GetTicks());
      } }
    if (c > 0.005f) logoCinema(c * a2);
    if (ta > 0.005f) trocaarte_desenhar(logoDe(idx)); }


  if (!ehSerie() && pg <= 0.01f && scrollY < 1.0f) {
    if (pessoaAberta) { ponteiro_camada(); desenhaPessoa(s); }
    if (colListaAberta) { ponteiro_camada(); desenhaListaColecao(s); }
    if (episodios_menu_aberto()) ponteiro_camada();
    reacaoPendente(s);
    maisDesenhar();
    if (amtui_aberta()) { ponteiro_camada(); amtui_desenhar(agora); }
    return;
  }
  desenhaEsqueletoEpisodios(a2 > pg ? a2 : pg);
  desenhaEsqueletoElenco(pg);
  // As duas fileiras da primeira tela da serie entram com o heroi, e nao com
  // a rolagem (pg fica em 0 com o foco nelas).
  { float aTopo = a2 * (1.0f - trailercinema_t(&trailerCinema)) * (1.0f - trocaarte_visivel());
    for (int r = 0; r < N_SECOES; r++)
      desenhaSecao(r, (ehSerie() && (r == SEC_TEMPORADAS || r == SEC_EPISODIOS))
                      ? (aTopo > pg ? aTopo : pg) : pg, agora); }
  // POR CIMA de tudo: a ficha e outra tela, nao uma secao desta.
  // O ponteiro (#99) nao alcanca a pagina por baixo de nenhuma das duas.
  if (pessoaAberta) { ponteiro_camada(); desenhaPessoa(s); }
  if (colListaAberta) { ponteiro_camada(); desenhaListaColecao(s); }
  // E o menu de visto por cima da ficha tambem: ele e o ultimo a abrir.
  if (episodios_menu_aberto()) ponteiro_camada();
  reacaoPendente(s);
  menuDeVisto(agora);
  maisDesenhar();
  if (amtui_aberta()) { ponteiro_camada(); amtui_desenhar(agora); }
}

static void ultimoEpisodio(int *t, int *e) {
  int nt, ti;
  *t = *e = 0;
  if (!ehSerie()) return;
  nt = extras_n_temporadas();
  for (ti = nt - 1; ti >= 0; ti--) {
    int num = extras_temporada_numero(ti), ne = extras_n_eps(ti);
    if (num <= 0 || ne <= 0) continue;     // especiais (T0) nao fecham a serie
    *t = num; *e = extras_ep_numero(ti, ne - 1);
    return;
  }
}

int detail_indice(void) { return idx; }
int detail_pediu_reproduzir(void) { int v = pedReproduzir; pedReproduzir = 0; return v; }
// Pedido de reproducao vindo de FORA da tela (OK no card de "Continuar
// assistindo", issue #93): o quadro seguinte cai no mesmo ramo de
// detail_pediu_reproduzir do app.c, com a pagina ja aberta por baixo.
void detail_pedir_reproduzir(void) { pedReproduzir = 1; }
int detail_pediu_abrir(void) { int v = pedAbrir; pedAbrir = -1; return v; }
void detail_volta_notar(int novo) {
  if (voltando) { voltando = 0; return; }       // esta troca E a volta: nao empilha
  if (!aberto || saindo || novo == idx || idx < 0) return;
  if (nVolta == (int)(sizeof voltaPilha / sizeof *voltaPilha)) {
    memmove(voltaPilha, voltaPilha + 1, sizeof voltaPilha - sizeof *voltaPilha);
    nVolta--;
  }
  voltaPilha[nVolta++] = idx;
}
int detail_pediu_assistido(void) { int v = pedAssistido; pedAssistido = 0; return v; }
int detail_pediu_marcar(void)     { int v = pedMarcar;     pedMarcar = 0;     return v; }
int detail_pediu_amigos(char *imdb, size_t tam) {
  const CatItem *c = cat_item(idx);
  if (!pedAmigos) return 0;
  pedAmigos = 0;
  if (imdb && tam) snprintf(imdb, tam, "%s", c ? c->imdb : "");
  return 1;
}
int detail_pediu_fontes(void)     { int v = pedFontes;     pedFontes = 0;     return v; }
int detail_pediu_explorar(void)   { int v = pedExplorar;   pedExplorar = 0;   return v; }
// "Reproduzir desde o inicio" ainda cai no mesmo caminho do primario: o
// roteador so sabe abrir o player no ponto salvo. Consumir o pedido aqui evita
// que ele fique pendurado.
int detail_pediu_do_inicio(void)  { int v = pedDoInicio;   pedDoInicio = 0;   return v; }

// Uma vez por pedido: a pagina saiu por ESQUERDA e quer a barra lateral no
// lugar. app.c le depois de detail_aberto() virar 0.
int detail_pediu_menu(void) { int v = pediuMenu; pediuMenu = 0; return v; }
// A barra lateral abriu/fechou por cima da pagina. O trailer no fundo NAO fecha
// (fecharia e so voltaria do comeco, com a espera inteira): fica mudo enquanto
// a barra esta aberta e recupera o som que tinha quando ela fecha.
// trailer_continuar so troca volume/retangulo da MESMA fonte, sem reabrir.
void detail_sob_menu(int sim) {
  sim = sim && aberto;
  if (sim == sobMenu) return;
  sobMenu = sim;
  if (!trailer_aberto() || trailer_cheia()) { if (sim) somAntesMenu = 0; return; }
  if (sim) {
    somAntesMenu = trailer_com_som();
    if (somAntesMenu) trailer_continuar(trailer_retangulo(), 0);
  } else if (somAntesMenu) {
    trailer_continuar(trailer_retangulo(), 1);
    somAntesMenu = 0;
  }
}
int detail_sob_menu_ativo(void) { return sobMenu; }
