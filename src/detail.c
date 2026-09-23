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
#include "episodios.h"
#include "fontepref.h"
#include "idioma.h"
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
#include "layout.h"
#include "catalogo.h"
#include "artehero.h"
#include "recomenda.h"
#include "recenviar.h"
#include "serieaud.h"
#include "seriefrases.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

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
#define REL_CARD_W   212.0f
#define REL_CARD_H   318.0f
#define REL_CARD_GAP  32.0f

// Cartao de produtora/rede: o dobro aproximado do `.detail-company-card` do web
// (180x70 em px de CSS) — na TV 1080p os cards medem em torno disto.
#define EST_CARD_W   240.0f
#define EST_CARD_H   100.0f
#define EST_GAP       18.0f
#define EST_TITULO_H  46.0f   // linha do titulo proprio na serie
// MEDIDO na referencia (TCL, 1920x1080): cartao 722x466, vao 25, canto 20.
// 722x466 era o MEDIDO na TCL; o dono, olhando a C9 em 19/09/2026, mandou
// encolher ("ta gigante") — o mesmo veredito dos botoes do detalhe na 1.3.2.
// 600x340 mantem a proporcao e as seis linhas de texto com leading de 30.
#define COM_CARD_W   600.0f
#define COM_CARD_H   340.0f
#define COM_PAD       24.0f
#define COM_CARD_GAP  22.0f
// Altura da CHAMADA das duas secoes sob demanda: a linha do titulo (TXT_HEADLINE,
// 38), a da procedencia e a do custo (TXT_DET_META2, 23), com os mesmos vaos
// que o cabecalho dos proprios modulos usa. Constante e nao medida por txt_linha
// porque ela entra em alturaSecao, que roda no empilhamento de todo quadro.
#define CHAMADA_H    124.0f
// Vao entre os tres graficos de audiencia. O mesmo NV_DETF_SEC_GAP que separa
// secoes de filme: os tres paineis tem cabecalho proprio e leem como tres
// blocos, nao como um bloco de tres partes.
#define AUD_GAP       64.0f
// Divisao do par frases | ficha. 1040 e a largura do bloco de texto do heroi
// (NV_DETW2_TEXTO_W) e dos "Detalhes do Filme", e os 592 que sobram sao
// exatamente a largura em que a ficha de producao foi desenhada e julgada (ver
// a nota no fim de seriefrases.c). 1040 + 96 + 592 = 1728, a faixa inteira.
#define FR_COL_W    1040.0f
#define FR_COL_GAP    96.0f

#define N_SECOES    13
#define N_ELENCO    6

static HomeItem item;
static int  aberto = 0, saindo = 0;
static int  idx = 0;                 // titulo atual dentro do acervo
// A IDENTIDADE do titulo aberto, e uma copia dele. Ver revalidarIdx.
static char idxImdb[24];
static CatItem idxCopia;
static int  idxTemCopia;
// Ultima revisao do catalogo que esta pagina ja tratou. Ver detail_atualizar.
static unsigned revistaVista;
static float t = 0.0f;               // 0 = card na home, 1 = tela cheia
// Dois estados, nao tres: o hero (nivel 0) e a pagina rolada (nivel 1). O
// nivel intermediario "cartao vira tela cheia" so fazia sentido enquanto havia
// cartao; no web a tela ja nasce cheia.
static int  nivel = 0;
// Ficha da pessoa por cima da tela de titulo. Nao e um `nivel` a mais porque
// nao e um estado da MESMA pagina: e outra tela, que aparece e sai inteira.
static int  pessoaAberta;
static int  pessoaFoco;
// Primeira LINHA visivel da filmografia. A grade tem 6 por linha e cabem duas
// linhas na tela; sem isto o resto dos creditos era cortado sem aviso.
static int  pessoaLinha;
static int  pedAbrir = -1;
// Foco DENTRO da aba "Mais como este", que e uma lista vertical propria e nao
// uma das fileiras horizontais do focus.c.
static int  relFoco;
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

static int  botao = 0;      // botao em foco no hero
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
static int    pediuMenu = 0;   // ESQUERDA na borda: fechar E abrir a barra (ver app.c)
static float  trailerFade = 0.0f;
// MODO CINEMA DO TRAILER (dono, 21/09/2026, com a foto do outro app: "quando
// comecar a tocar o trailer descer a arte do titulo e deixar assim"). Com o
// trailer tocando, o bloco de texto do heroi desce e apaga e so o logo fica,
// pequeno, no canto inferior esquerdo. Qualquer tecla traz o bloco de volta
// (o trailer continua); Voltar fecha o trailer sem sair da pagina.
static float  trailerCopy = 0.0f;      // 0 = bloco no lugar, 1 = so o logo embaixo
static int    trailerCopyOculta = 0;   // a intencao; trailerCopy e a mola
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
static int trailerSemFonteLogado = 0;
static const char *trailerFonte(int k, int *qual) {
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
#ifndef __EMSCRIPTEN__
    c.imdb = trailerimdb_url(ci->imdb, NULL);
    c.imdbRespondeu = trailerimdb_respondeu(ci->imdb);
#endif
  } else c.appleRespondeu = c.imdbRespondeu = 1;   // sem id nao ha o que esperar
#ifdef __EMSCRIPTEN__
  if (k >= 0 && k < extras_n_trailers() && extras_trailer_yt(k)[0]) c.youtube = extras_trailer_yt(k);
  else if (extras_n_trailers() > 0 && extras_trailer_yt(0)[0]) c.youtube = extras_trailer_yt(0);
#else
  (void)k;
#endif
  c.youtubeRespondeu = !extras_carregando();
  d = trailerfonte_escolher(aj, tz, &c, &u, &q);
  if (d == TRF_ABRE) { if (qual) *qual = q; return u; }
  if (d == TRF_NENHUMA && !trailerSemFonteLogado) {
    trailerSemFonteLogado = 1;
    printf("[trailer] detalhe: sem trailer (ajuste %d, apple %s, imdb %s, youtube %s)\n", aj,
           c.apple ? "tem" : c.appleRespondeu ? "sem" : "?",
           tz ? "n/a" : c.imdb ? "tem" : c.imdbRespondeu ? "sem" : "?",
           !tz ? "n/a" : c.youtube ? "tem" : "sem");
    fflush(stdout);
  }
  return NULL;
}
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
// Altura MEDIDA no ultimo desenho. Os dois modulos so dizem quanto ocuparam
// DEPOIS de desenhar (e o valor de retorno de cada painel), e o empilhamento
// precisa do numero antes. Um quadro de atraso e o preco, e ele so aparece nas
// duas trocas de estado que existem (chamada -> carregando -> pronto), nunca
// por quadro: a secao de frases e a ultima do documento (mexe so no docFim) e a
// de audiencia so muda de altura com o foco DENTRO dela, onde a rolagem ja mira
// o topo dela, que nao se move.
static float audAlt[3] = { 380.0f, 402.0f, 484.0f }, frasesAlt = 520.0f;
// PISO DE ALTURA DE CADA BANDA, e ele nao e cosmetico — e o que impede o unico
// defeito grave que a altura-medida-no-ultimo-desenho pode causar.
//
// MEDIDO na captura (tests/detail_secoes_shot.sh, 1920x1080): com a temporada
// inteira na mao o arco ocupa 380, o radar 402 e a impressao digital 484. Os
// tres sao dominados por constantes fixas do modulo (a caixa do grafico mede
// 210, 200 e 200) — o que varia com o conteudo sao poucas linhas de rodape, e
// sempre PARA MAIS.
//
// O DEFEITO QUE ISTO CORRIGE apareceu na captura do estado "carregando": no
// quadro em que a banda troca de vazia para cheia, o empilhamento ainda usa a
// altura do quadro anterior (~140, a do aviso de vazio) e o CABECALHO DO RADAR
// era desenhado POR CIMA da curva do arco. E o mesmo acidente que a secao do
// Trakt ja teve contra os avatares do elenco, e um quadro dele ja e o suficiente
// para a pessoa ver dois textos sobrepostos ao entrar na secao.
//
// Com o piso, a banda vazia RESERVA o espaco que o grafico vai ocupar — que e a
// mesma disciplina dos esqueletos de episodio e de elenco deste arquivo
// ("ocupa exatamente as coordenadas finais, para a resposta so preencher e nao
// deslocar a pagina"). A medida so pode fazer a banda CRESCER.
static const float AUD_PISO[3] = { 380.0f, 402.0f, 484.0f };
// 1 quando a fileira `r` e uma das tres bandas de audiencia. Vira indice em
// audAlt com `r - SEC_AUD_ARCO`.
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
typedef enum { SEC_TEMPORADAS, SEC_EPISODIOS, SEC_ABAS_INFO, SEC_ELENCO,
               SEC_AUD_ARCO, SEC_AUD_RADAR, SEC_AUD_DIGITAL,
               SEC_TRAILERS, SEC_RELACIONADOS, SEC_COMENTARIOS,
               SEC_ESTUDIOS, SEC_FRASES, SEC_DETALHES } TipoSecao;
// Definida adiante, junto do resto das consultas ao catalogo; declarada aqui
// porque recalcularLayout, cabecalhoDe e nAvaliaveis, todas acima dela,
// precisam separar serie de filme.
static int ehSerie(void);
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
static float docFim = NV_DETP_FIM;

// Cabecalho de secao: so o filme tem. Na serie o rotulo "Temporadas" e desenhado
// pelo caminho antigo, e "Elenco" ficaria repetindo a aba "Criador e elenco"
// logo acima (ver detail.c:1611).
static const char *cabecalhoDe(int r) {
  if (ehSerie()) return NULL;
  switch (r) {
    case SEC_ELENCO:   return "Elenco";
    case SEC_TRAILERS:     return "Trailers";
    case SEC_RELACIONADOS: return "Recomendações";
    // Sem cabecalho de secao: a propria secao ja abre com "trakt Comentários" e
    // o subtitulo "Avaliações do Trakt". Com os dois saiam DOIS titulos
    // empilhados dizendo a mesma coisa.
    case SEC_COMENTARIOS:  return NULL;
    // Na serie o titulo sai DENTRO da secao (desenhaEstudios), porque as secoes
    // dela nao levam cabecalho — mesmo motivo do "trakt Comentarios".
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

static void recalcularLayout(void) {
  int r;
  if (ehSerie()) {
    // As quatro primeiras vem de MEDIDA ABSOLUTA na referencia; nao sao um
    // empilhamento. As de baixo (comentarios) sim: elas ficam depois do elenco,
    // cuja altura e conhecida.
    static const float G[N_SECOES] = {
      NV_DETP_G_TEMP, NV_DETP_G_EP, NV_DETP_G_ABAS, NV_DETP_G_ELENCO, 0, 0
    };
    float y;
    for (r = 0; r < N_SECOES; r++) {
      topoSec[r] = conteudoSec[r] = G[r];
      alvoSec[r] = (r == SEC_ABAS_INFO) ? NV_DETP_ALVO_ABAS
                                        : NV_DETP_ALVO_FILEIRA;
    }
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
    // AS TRES BANDAS DE AUDIENCIA vem antes dos comentarios, empilhadas pela
    // mesma regra: nascem onde a aba ativa termina e empurram o que vem depois.
    // Altura vinda do ultimo desenho (ver audAlt), porque cada grafico so diz
    // quanto ocupou depois de desenhado.
    for (r = SEC_AUD_ARCO; r <= SEC_AUD_DIGITAL; r++) {
      topoSec[r] = conteudoSec[r] = y;
      if (secaoN(r) <= 0) continue;
      y += alturaSecao(r) + NV_DETF_SEC_GAP;
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
    // ESTUDIOS DEIXOU DE SER A ULTIMA e por isso passou a ADIANTAR `y`. Antes
    // ela so media o proprio fim para o docFim; com a secao de frases embaixo,
    // nao adiantar aqui punha as duas no mesmo topo — o mesmo defeito que a
    // secao do Trakt ja teve contra os avatares do elenco.
    if (secaoN(SEC_ESTUDIOS) > 0) {
      y += alturaSecao(SEC_ESTUDIOS) + NV_DETF_SEC_GAP;
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
    for (r = 0; r < N_SECOES; r++) {
      float h;
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
    if (docFim < NV_TELA_H) docFim = NV_TELA_H; }
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
  return ci ? ci->nota : 0;
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
    case ABA_AVALIACOES:   return notaDe(idx) > 0 || extras_nota_trakt() > 0;
    case ABA_RELACIONADOS: return extras_n_relacionados() > 0;
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
static char arteFixa[512], logoFixo[512], logoCatalogoFixo[512];
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
  // Congelada, MAS NAO PRESA A UM 404: a url da fonte escolhida pode ser
  // virtual (TMDB/Trakt pelo id, artereserva.h) e so se sabe se ela existe
  // depois do download. Falhou, solta e escolhe de novo (cai na seguinte).
  if (i == idx && arteFixa[0] && !tex_falhou(arteFixa)) return arteFixa;
  { const char *u = arteDeViva(i);
    if (i == idx && u) snprintf(arteFixa, sizeof arteFixa, "%s", u);
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
  if (i == idx && arteFixa[0]) return arteFixaPoster;
  { int r = c && !c->backdrop[0] && c->poster[0];
    if (i == idx) arteFixaPoster = r;
    return r; }
}

static void desenhaArteDetalhe(GfxRect alvo, GLuint tex, const char *arte,
                               int poster, float alpha, float pg) {
  if (!tex) {
    gfx_cor(alvo, 0.0f, 0.051f, 0.051f, 0.051f, alpha);
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
      gfx_rect(alvo, 0, GFX_DETALHE, veu * (1.0f - 0.85f * anim_suave(trailerCopy)), 1.0f, 0, 0.0f, 0, 0, 0, alpha * trailerFade);
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

void detail_abrir(const HomeItem *it) {
  pediuMenu = 0;
  marco("detail_abrir");
  // O TITULO ANTERIOR PODE TER DEIXADO FIO NO AR. Trocar de titulo por dentro
  // (um credito de ator, um "Mais como este") reabre esta tela sem passar pelo
  // fechamento, e sem isto os pedidos da serie antiga continuavam saindo para
  // uma pagina que ninguem esta mais vendo. Os dois modulos param na proxima
  // fronteira de episodio e descartam o resto; serieaud_abrir/seriefrases_abrir
  // limpam a marca sozinhos.
  serieaud_fechar();
  seriefrases_fechar();
  audAberta = 0; audTempAberta = -1; audTempVista = -1; frasesAberta = 0;
  item = *it;
  aberto = 1; saindo = 0; nivel = 0; botao = 0;
  t = 0.0f; pg = 0.0f; scrollY = 0.0f; abaInfo = 0; pessoaAberta = 0;
  relFoco = 0; pedAbrir = -1; ratTemp = 0; ratSinc = 0;
  trailer_fechar(); trailerDesde = 0; trailerTentado = 0; trailerFade = 0.0f;
  trailerEtapa = 0; trailerPrazo = 0;
  trailerSemFonteLogado = 0;
  trailerCopy = 0.0f; trailerCopyOculta = 0;
  idx = it->indice;
  revistaVista = cat_revisao();
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
    extras_pedir(ci ? ci->imdb : "", ehSerie(), ci ? ci->tmdb : 0);
    // Pedir agora e o que deixa o trailer pronto quando a pagina assentar.
    // Apple nos dois alvos; IMDb so na LG (exige Referer, que o navegador
    // nao deixa por, e o CORS dele so aceita imdb.com).
    if (trailer_suportado() && ci && ci->imdb[0]) {
      trailerapple_pedir(ci->imdb, ci->titulo, ci->meta, ehSerie());
#ifndef __EMSCRIPTEN__
      trailerimdb_pedir(ci->imdb);
#endif
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
    if (ci0 && ci0->progresso > 0 && ci0->progresso < 90 &&
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
  memset(scrollSec, 0, sizeof scrollSec);
}

int detail_aberto(void) { return aberto; }

// 0..1 de quanto o detalhe ja tomou a tela. A home le isto para DESCER as
// fileiras enquanto ele entra: e o movimento que o dono descreve como "so os
// posters descem". Fica aqui e nao numa variavel compartilhada porque a mola
// que o produz e a mesma do desenho — dois relogios diferentes descasariam.
float detail_progresso(void) { return aberto ? suave(t) : 0.0f; }

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
    if (origem) *origem = 1;
    return 1;
  }
  // Em andamento: o item do "Continuar assistindo" traz temporada e episodio.
  if (ci && ci->progresso > 0 && ci->progresso < 90 && ci->temporada > 0 && ci->episodio > 0 &&
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
  if (min <= 0) { dst[0] = 0; return; }
  if (min < 60) { snprintf(dst, tam, "%dmin", min); return; }
  if (min % 60) snprintf(dst, tam, "%dh %dmin", min / 60, min % 60);
  else          snprintf(dst, tam, "%dh", min / 60);
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

  DET_POE("Status", extras_ficha_status());
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
  DET_POE("País de Origem", v);
  DET_POE("Direção", (ci && ci->direcao[0]) ? ci->direcao : NULL);

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
static float alturaSecao(int r) {
  switch (r) {
    case SEC_TEMPORADAS: return NV_DETP_TEMP_H;
    case SEC_EPISODIOS:  return NV_DETP_EP_H;
    case SEC_ABAS_INFO:  return NV_DETP_ABA_H;
    case SEC_ELENCO:     return NV_DETF_EL_ALT;
    case SEC_TRAILERS:     return NV_DETF_TR_ALT;
    case SEC_RELACIONADOS: return 318.0f + 46.0f;   // cartaz + titulo/ano
    // + o cabecalho: sem ele a secao seguinte ("Detalhes do Filme") era
    // empilhada usando so a altura dos cartoes e saia POR CIMA deles.
    case SEC_COMENTARIOS:  return alturaCabComentarios() + COM_CARD_H;
    // Na serie o titulo "Redes e estudios" sai DENTRO da secao (cabecalhoDe
    // devolve NULL para ela), entao a linha do titulo entra na altura.
    case SEC_ESTUDIOS:     return EST_CARD_H + (ehSerie() ? EST_TITULO_H : 0.0f);
    case SEC_DETALHES:     return nLinhasDetalhe() * NV_DETF_DET_LINHA;
    // As duas sob demanda: a CHAMADA enquanto ninguem entrou (titulo +
    // procedencia + custo, tres linhas) e a altura MEDIDA no ultimo desenho
    // depois disso.
    case SEC_AUD_ARCO:
    case SEC_AUD_RADAR:
    case SEC_AUD_DIGITAL: {
      int b = r - SEC_AUD_ARCO;
      if (!audAberta) return CHAMADA_H;
      return audAlt[b] > AUD_PISO[b] ? audAlt[b] : AUD_PISO[b];
    }
    case SEC_FRASES:    return frasesAberta ? frasesAlt : CHAMADA_H;
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
        case ABA_COLECAO:      n = extras_n_colecao();      break;
        // O cartao de comentario nao se escolhe um a um; o que RECEBE foco sao
        // as duas pilulas do seletor "Série | Episódio". Em filme nao ha
        // episodio: sobra uma coluna so, para o foco poder pousar na fileira e
        // a pagina rolar ate os cartoes.
        case ABA_COMENTARIOS:  n = ehSerie() ? 2 : 1; break;
        default:               n = (ci && ci->nElenco > 0) ? ci->nElenco : 0;
      }
      return n < NV_DETF_EL_MAX ? n : NV_DETF_EL_MAX;
    }
    // Trailers, Recomendacoes, Comentarios e Detalhes so existem em FILME —
    // na serie o mesmo conteudo vive atras das ABAS.
    //
    // Estas duas ultimas eram justamente o que se perdeu ao tirar as abas do
    // filme: os dados sempre estiveram la (o log mostra "coment=8 rel=12"),
    // mas sem aba e sem secao nao havia como chegar neles.
    case SEC_TRAILERS:
      if (ehSerie()) return 0;
      return extras_n_trailers();
    case SEC_RELACIONADOS: {
      int n;
      if (ehSerie()) return 0;
      n = extras_n_relacionados();
      return n < N_ITENS ? n : N_ITENS;
    }
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
    // UMA COLUNA POR EPISODIO da temporada escolhida: e o seletor do painel de
    // impressao digital, que destaca um episodio de cada vez
    // (serieaud_selecionar). A contagem sai de extras.h e nao do modulo de
    // audiencia — ela precisa existir ANTES do primeiro pedido, senao a secao
    // teria zero colunas, focus_mover pularia por cima dela (focus.c) e o
    // pedido nunca poderia ser disparado: a secao ficaria inalcancavel para
    // sempre por depender de si mesma.
    //
    // Sem essa lista a secao NAO EXISTE, e e a mesma condicao que "sem Trakt":
    // as notas por episodio e os numeros dos episodios saem todos do
    // `seasons?extended=episodes,full`. Nao ha aqui uma secao oferecida que nao
    // possa funcionar nesta instalacao.
    // A PRIMEIRA BANDA E A PORTA. Ela existe assim que ha lista de episodios —
    // e por ela que o foco entra e o pedido sai. As outras duas so ganham
    // coluna DEPOIS disso: enquanto ninguem entrou, elas nao tem nada para
    // desenhar, e uma fileira que recebe foco sem desenhar nada e exatamente o
    // que este arquivo ja evita em tres outros lugares.
    //
    // O ARCO e o RADAR sao UMA coluna: o foco pousa, a pagina rola ate o
    // grafico e pronto. Quem tem uma coluna POR EPISODIO e a impressao digital,
    // onde a escolha muda o rodape de numeros crus (serieaud_selecionar).
    case SEC_AUD_ARCO:
      return audTemp() >= 0 ? 1 : 0;
    case SEC_AUD_RADAR:
      return (audAberta && audTemp() >= 0) ? 1 : 0;
    case SEC_AUD_DIGITAL: {
      int t = audTemp(), n;
      if (!audAberta || t < 0) return 0;
      n = extras_n_eps(t);
      return n < N_ITENS ? n : N_ITENS;
    }
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
  // abaixo de 90. Em serie, so quando o episodio-alvo e o "Retomar" — com um
  // episodio em foco na fileira (origem 1) o primario toca AQUELE episodio e
  // nao ha retomada a desfazer.
  if (!ehSerie()) return ci->progresso > 0 && ci->progresso < 90;
  { int t0 = 0, e0 = 0, de = 0;
    return episodioAlvo(&t0, &e0, &de) && de == 2; }
}
// O QUINTO CIRCULAR: "Recomendar a um amigo".
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
static int nBotoes(void) {
  return (ehSerie() ? 3 : 4) + (temInicio() ? 1 : 0) + (temLembrar() ? 1 : 0)
         + (temRecomendar() ? 1 : 0);
}

// Que ACAO esta na posicao `n` da linha. As acoes tem numeros fixos (0
// primario, 1 lista, 2 assistido, 3 fontes, 4 inicio) porque detail_evento
// decide por eles; o que muda com o tipo e quais posicoes existem. Sem esta
// traducao, na serie o segundo circular (que e o de fontes) dispararia
// "marcar assistido". Quando temInicio, a posicao 1 e o secundario de texto
// e os circulares escorregam um para a direita.
enum { ACAO_PRIMARIO = 0, ACAO_LISTA = 1, ACAO_ASSISTIDO = 2, ACAO_FONTES = 3,
       ACAO_INICIO = 4, ACAO_RECOMENDAR = 5, ACAO_LEMBRAR = 6 };
static int acaoEm(int n) {
  if (temInicio()) {
    if (n == 1) return ACAO_INICIO;
    n--;
  }
  // O "Lembrar-me" e o SEGUNDO botao de texto, e o desconto vem antes da conta
  // dos circulares — pela mesma razao que o de temInicio: sem ele a posicao de
  // um circular escorregaria e o OK dispararia a acao do vizinho.
  if (temLembrar()) {
    if (n == 1) return ACAO_LEMBRAR;
    n--;
  }
  // O RECOMENDAR E O ULTIMO DA LINHA e a conferencia vem ANTES do salto da
  // serie: com 3 circulares numa serie, a ultima posicao e n == 3, e a regra
  // de baixo devolveria 4 — que e ACAO_INICIO, o botao de texto. O OK ali
  // abriria "assistir do comeco" a partir de um circular de enviar.
  if (temRecomendar() && n == (ehSerie() ? 3 : 4)) return ACAO_RECOMENDAR;
  if (n >= 2 && ehSerie()) return n + 1;   // serie pula o olho
  return n;
}

void detail_evento(const SDL_Event *e) {
  if (saindo) return;
  // TRAILER EM TELA CHEIA come o teclado: OK pausa, Voltar fecha. O autoplay
  // no fundo nao passa por aqui — ele nao tem teclado, a pagina continua a
  // dela, e qualquer coisa que tire a pagina do topo o fecha (detail_atualizar).
  if (trailer_cheia() && trailer_evento(e)) return;
  // MODO CINEMA: a primeira tecla so devolve o bloco de texto (o trailer
  // segue); Voltar fecha o trailer e fica na pagina.
  if (trailerCopyOculta && e->type == SDL_KEYDOWN && !e->key.repeat) {
    SDL_Keycode kc = e->key.keysym.sym;
    trailerCopyOculta = 0;
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
  if (pessoaAberta) {
    if (e->type != SDL_KEYDOWN) return;
    { int n = pessoa_n_creditos();
      switch (e->key.keysym.sym) {
        case SDLK_LEFT:  if (pessoaFoco > 0) pessoaFoco--; return;
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
        case SDLK_AC_BACK: pessoaAberta = 0; return;
        case SDLK_RETURN:
        case SDLK_KP_ENTER: {
          // Abre o titulo, quando ele for um dos que o catalogo ja tem meta.
          // Quem troca de fato e o roteador (app.c) — daqui so sai o pedido.
          //
          // Um credito que NAO esta no catalogo nao abre nada, de proposito:
          // sem meta nao ha episodios, elenco nem fonte, e uma tela de detalhe
          // vazia e pior que o botao nao responder. Buscar meta sob demanda e
          // trabalho a parte.
          const char *id = pessoa_credito_imdb(pessoaFoco);
          int alvo = id[0] ? cat_indice_por_imdb(id) : -1;
          if (alvo >= 0) { pedAbrir = alvo; pessoaAberta = 0; }
          // Nao esta no catalogo: busca o meta e abre quando chegar. Quem
          // termina o trabalho e o roteador, que ja acompanha o resultado.
          // O credito quase nunca traz imdb_id, entao o caminho normal e pelo
          // id do TMDB.
          else if (id[0]) { desc_pedir_titulo(id); pessoaAberta = 0; }
          else if (pessoa_credito_tmdb(pessoaFoco) > 0) {
            desc_pedir_titulo_tmdb(pessoa_credito_tmdb(pessoaFoco),
                                   pessoa_credito_tipo(pessoaFoco));
            pessoaAberta = 0;
          }
          return; }
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

  // "Mais como este" e "Colecao" sao a MESMA lista vertical, so muda a fonte.
  if (e->type == SDL_KEYDOWN && foco.fileira == SEC_ELENCO && !pessoaAberta &&
      (abaIdDe(abaInfo) == ABA_RELACIONADOS || abaIdDe(abaInfo) == ABA_COLECAO)) {
    int col = (abaIdDe(abaInfo) == ABA_COLECAO);
    int n = col ? extras_n_colecao() : extras_n_relacionados();
    if (n > 7) n = 7;
    switch (e->key.keysym.sym) {
      // "Mais como este" e uma fileira de cartazes: anda na HORIZONTAL. A
      // colecao continua em lista vertical.
      case SDLK_RIGHT: if (!col && relFoco + 1 < n) { relFoco++; return; } break;
      case SDLK_LEFT:  if (!col && relFoco > 0)     { relFoco--; return; } break;
      case SDLK_DOWN: if (col && relFoco + 1 < n) { relFoco++; return; } break;
      case SDLK_UP:   if (col && relFoco > 0)     { relFoco--; return; } break;
      case SDLK_RETURN:
      case SDLK_KP_ENTER: {
        if (col) {
          // A parte da colecao traz so o id do TMDB; o caminho e o mesmo do
          // credito de um ator.
          long t = extras_colecao_tmdb(relFoco);
          if (t > 0) desc_pedir_titulo_tmdb(t, "movie");
        } else {
          const char *id = extras_relacionado_imdb(relFoco);
          // "tmdb:<id>" = recomendacao do TMDB (tmdb_use_more_like_this): nao
          // tem imdb ate a meta chegar, entao abre pelo id do TMDB direto.
          if (!strncmp(id, "tmdb:", 5))
            desc_pedir_titulo_tmdb(atol(id + 5), ehSerie() ? "tv" : "movie");
          else {
            int alvo = cat_indice_por_imdb(id);
            if (alvo >= 0) pedAbrir = alvo;
            else if (id[0]) desc_pedir_titulo(id);
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
    if (nivel == 0) {
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
      } else if (acao == ACAO_RECOMENDAR) {
        // A MESMA MODAL DO MENU DO CARTAZ, e nao uma segunda copia dela: ver
        // recenviar.h. Aberta, ela fica acima desta tela no roteador de app.c e
        // recebe o D-pad ate fechar.
        const CatItem *ci = cat_item(idx);
        if (ci) recenviar_abrir(ci);
      } else {
        pedFontes = 1;
      }
    } else if (foco.fileira == SEC_RELACIONADOS) {
      // FILME: "Mais como este" e secao propria. Mesmo destino do caminho de
      // serie — abre do catalogo quando ja temos meta, senao pede e o roteador
      // termina quando chegar. "tmdb:<id>" = recomendacao do TMDB.
      const char *id = extras_relacionado_imdb(foco.coluna);
      if (!strncmp(id, "tmdb:", 5))
        desc_pedir_titulo_tmdb(atol(id + 5), "movie");
      else {
        int alvo = id[0] ? cat_indice_por_imdb(id) : -1;
        if (alvo >= 0) pedAbrir = alvo;
        else if (id[0]) desc_pedir_titulo(id);
      }
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
      const char *u = trailer_suportado() ? trailerFonte(foco.coluna, NULL) : NULL;
      if (u) {
        GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
        trailerEtapa = 0; trailerPrazo = 0;   // tela cheia: so o teclado fecha
        trailer_abrir(u, tela, trailerfonte_com_som(trailerfonte_tizen()), 1);
      } else extras_trailer_abrir(foco.coluna);
    } else if (foco.fileira == SEC_ESTUDIOS) {
      // OK num logo abre o browse daquela produtora/rede no vertudo — e a
      // mesma pasta sintetica TMDB que as colecoes usam (issue #44), montada
      // na hora. Sem id numerico nao ha endpoint para chamar: o OK nao faz
      // nada em vez de adivinhar pelo nome.
      //
      // vertudo_colecao guarda o PONTEIRO da pasta, nao uma copia — por isso a
      // struct e estatica e nao local.
      static ColFolder pasta;
      long tmdbId = extras_estudio_tmdb(foco.coluna);
      if (tmdbId > 0) {
        memset(&pasta, 0, sizeof pasta);
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
      const CatItem *ci = cat_item(idx);
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
    if (nivel > 0) nivel = 0; else saindo = 1;
    return;
  }
  if (nivel == 0) {
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
      for (int r = 0; r < N_SECOES; r++)
        if (secaoColunas(r) > 0) {
          int alvo = foco.colunaLembrada[r];
          if (alvo >= secaoColunas(r)) alvo = secaoColunas(r) - 1;
          if (alvo < 0) alvo = 0;
          foco.fileira = r; foco.coluna = alvo; nivel = 1;
          break;
        }
    }
    else if (k == SDLK_RIGHT) { if (botao < nBotoes() - 1) botao++; }
    else if (k == SDLK_LEFT)  {
      // ESQUERDA no primeiro botao fecha a pagina e pede a barra lateral
      // (app.c abre quando a mola de saida terminar). Um toque so.
      if (botao > 0) botao--;
      else { saindo = 1; pediuMenu = 1; }
    }
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
  if (k == SDLK_RIGHT)      focus_mover(&foco, 1, 0);
  else if (k == SDLK_LEFT)  {
    if (!focus_mover(&foco, -1, 0)) { saindo = 1; pediuMenu = 1; }
  }
  else if (k == SDLK_DOWN)  focus_mover(&foco, 0, 1);
  else if (k == SDLK_UP)    { if (!focus_mover(&foco, 0, -1)) nivel = 0; }
}

// Largura do item e passo horizontal de cada fileira. Temporada e aba de
// informacao tem largura VARIAVEL (saem do texto), e por isso o passo delas nao
// e uma constante como a do episodio.
static float larguraItem(int r, int c) {
  switch (r) {
    case SEC_TEMPORADAS:  return larguraTemporada(c);
    case SEC_EPISODIOS:   return NV_DETP_EP_W;
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
    case SEC_FRASES:      return NV_TELA_W - NV_DETP_X * 2;
    default:              return NV_DETP_EL_W;
  }
}
// x do item `c` DENTRO da fileira (antes da rolagem horizontal).
static float xItem(int r, int c) {
  float x = NV_DETP_X;
  for (int k = 0; k < c; k++) {
    if (r == SEC_EPISODIOS) { x += NV_DETP_EP_PASSO; continue; }
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
    if (EH_AUD(r) || r == SEC_FRASES) continue;
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
  printf("[detail] catalogo remontou: %s saiu de %d para %d\n",
         idxImdb, idx, novo);
  fflush(stdout);
  idx = novo;
}

void detail_atualizar(float dt, Uint32 agora) {
  // `agora` ficou sem uso quando o repouso da troca de temporada saiu (ver a
  // nota mais abaixo). Fica na assinatura porque ela e a mesma de todas as
  // telas e app.c chama todas do mesmo jeito.
  (void)agora;
  if (!aberto) return;
  // SAINDO: interrompe os dois fios antes mesmo de a mola terminar. Chamar todo
  // quadro nao custa nada (e um flag sob mutex) e evita precisar de uma borda:
  // `saindo` tambem e ligado por caminhos que nao passam pelo Voltar, como o
  // OK num estudio, que abre o vertudo e deixa esta tela para tras.
  if (saindo) { serieaud_fechar(); seriefrases_fechar(); }
  revalidarIdx();
  // TRAILER AUTOMATICO, mudo, no lugar da arte (dono, 20/09/2026: "trailer
  // autoplay direto na interface"). Comeca NV_TRAILER_ESPERA_MS depois de a
  // pagina assentar, uma vez por abertura, e so enquanto a pagina esta no
  // topo com o heroi a mostra: rolar, abrir uma ficha, um menu ou sair fecha
  // o video e a arte volta. Em tela cheia (botao) a regra e outra: so o
  // teclado fecha.
  if (trailer_suportado()) {
    int topo = !saindo && nivel == 0 && !pessoaAberta && !episodios_menu_aberto() &&
               !pedReproduzir && !pedFontes && !player_aberto() &&
               pg < 0.05f && scrollY < 1.0f;
    if (!detail_assentado() || !topo) { if (!trailer_cheia()) trailerDesde = 0; }
    else if (!trailerDesde) trailerDesde = agora;
    if (trailer_aberto() && !trailer_cheia() && !topo) trailer_fechar();
    if (saindo && trailer_aberto()) trailer_fechar();
    if (!trailer_aberto() && !trailerTentado && trailerDesde && topo &&
        agora - trailerDesde >= NV_TRAILER_ESPERA_MS &&
        ajustes_trailer_auto()) {
      int qual = 0;
      const char *u = trailerFonte(0, &qual);
      if (u) {
        GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
        trailerTentado = 1;
        trailer_abrir(u, tela, 0, 0);
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
    // ordem do ajuste, uma vez — em Automatico, Apple -> YouTube; com uma fonte
    // fixa nao ha proxima e fica a arte. Tela cheia nao entra aqui.
    // trailerEtapa: 0 nada, TRF_APPLE/TRF_YOUTUBE a fonte aberta, -1 acabou.
    if (trailerEtapa > 0) {
      int venceu = trailer_aberto() && !trailer_cheia() && !trailer_tocando() &&
                   trailerPrazo && (Sint32)(agora - trailerPrazo) >= 0;
      int errou = !trailer_aberto() && trailer_falhou();
      if (trailer_aberto() && trailer_tocando()) trailerPrazo = 0;
      if ((venceu || errou) && topo) {
        int prox = trailerfonte_depois(trailerfonte_ajuste(), trailerfonte_tizen(), trailerEtapa);
        const char *yt = extras_n_trailers() > 0 ? extras_trailer_yt(0) : "";
        const char *seg = prox == TRF_YOUTUBE && yt[0] ? yt : NULL;
        printf("[trailer] detalhe: %s %d ms (%s, estado %d), %s\n",
               errou ? "erro em" : "sem playing em", NV_TRAILER_PREPARA_MS,
               trailerfonte_nome(trailerEtapa), trailer_estado(),
               seg ? "tenta YouTube" : prox ? "proxima fonte sem trailer, fica a arte" : "fica a arte");
        fflush(stdout);
        if (trailer_aberto()) trailer_fechar();
        if (seg) {
          GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
          trailer_abrir(seg, tela, 0, 0);
          trailerEtapa = TRF_YOUTUBE;
          trailerPrazo = agora + NV_TRAILER_PREPARA_MS;
        } else { trailerEtapa = -1; trailerPrazo = 0; }
      } else if (!trailer_aberto() && !errou) {
        trailerEtapa = 0; trailerPrazo = 0;   // fechou por fim ou por navegacao
      }
    }
#endif
    { float alvo = (trailer_aberto() && trailer_tocando()) ? 1.0f : 0.0f;
      static int tocavaAntes;
      int toca = alvo > 0.5f && !trailer_cheia();
      // Borda de subida: o trailer COMECOU a tocar -> esconde o bloco.
      if (toca && !tocavaAntes) trailerCopyOculta = 1;
      if (!toca) trailerCopyOculta = 0;
      tocavaAntes = toca;
      trailerFade = anim_mola(trailerFade, alvo, dt, NV_MOLA_SCROLL);
      trailerCopy = anim_mola(trailerCopy, trailerCopyOculta ? 1.0f : 0.0f, dt, NV_MOLA_SCROLL); }
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
  { unsigned rev = cat_revisao();
    if (rev != revistaVista) {
      revistaVista = rev;
      if (ehSerie() && cat_n_episodios(idx) < 1) {
        const CatItem *ci = cat_item(idx);
        desc_episodios(idx, ci ? ci->temporada : 0);
      }
    } }
  sincronizarColunas();
  // Solta o pedido de episodios que ficou guardado por ter chegado com outro
  // carregamento em voo.
  desc_episodios_pendente();

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
  if (nivel >= 1 && EH_AUD(foco.fileira)) {
    // AS NOTAS JA ESTAO NA MAO. Vem do mesmo `seasons?extended=episodes,full`
    // que desenha as pastilhas da aba "Avaliações"; o arco nao custa pedido.
    abrirAudiencia();
    // O episodio em destaque no painel 3 e a COLUNA focada — e so na banda da
    // impressao digital, que e a unica com uma coluna por episodio. Nas outras
    // duas a coluna e sempre 0 e mexer na selecao por causa dela apagaria o
    // episodio escolhido toda vez que o foco passasse por cima delas.
    if (foco.fileira == SEC_AUD_DIGITAL) serieaud_selecionar(foco.coluna);
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
  if (nivel >= 1 && foco.fileira == SEC_FRASES) {
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
  pg = anim_mola(pg, nivel >= 1 ? 1.0f : 0.0f, dt, NV_MOLA_PAGINA);
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
    aberto = 0; saindo = 0; t = 0.0f; trailer_fechar(); return;
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
        else if (EH_AUD(r) || r == SEC_FRASES) alvo = 0.0f;
        else if (foco.coluna == 0) alvo = 0.0f;
        else if (x + w > alvo + vista - 24.0f) alvo = x + w - vista + 24.0f;
        else if (x < alvo + 24.0f)             alvo = x - 24.0f;
      }
      if (alvo < 0.0f) alvo = 0.0f;
      scrollSec[r] = anim_mola(scrollSec[r], alvo, dt, NV_MOLA_SCROLL);
    }
    // A fileira de episodios rola ATRAS da aba de temporada mesmo sem o foco:
    // e o que da ao seletor a resposta visual que ele perdeu ao deixar de
    // arrastar o foco junto.
    if (r != SEC_EPISODIOS && secaoN(SEC_EPISODIOS) > 0 &&
        epAncora < foco.nColunas[SEC_EPISODIOS]) {
      float ax = xItem(SEC_EPISODIOS, epAncora) - NV_DETP_X;
      if (ax < 0.0f) ax = 0.0f;
      scrollSec[SEC_EPISODIOS] = anim_mola(scrollSec[SEC_EPISODIOS], ax, dt,
                                           NV_MOLA_SCROLL);
    } }

  // --- rolagem VERTICAL -----------------------------------------------------
  // O topo do grupo focado vai para 33% da altura util (40% nas abas). E a
  // regra do web, e nao um "rola o necessario": conferida nos quatro grupos.
  float alvoY = 0.0f;
  if (nivel >= 1 && foco.fileira >= 0 && foco.fileira < N_SECOES) {
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
  scrollY = anim_mola(scrollY, alvoY, dt, NV_MOLA_SCROLL);
}

// ---------------------------------------------------------------------------
// HERO
// ---------------------------------------------------------------------------
// Nada aqui e sobreposicao num cartao: a tela e full-bleed, a coluna comeca em
// x=72 e a pilha e ancorada na BASE (`.detail-hero-section` e um flex column
// com `justify-content: flex-end`). Empilhar de cima para baixo faz o bloco
// inteiro subir e descer conforme o tamanho da sinopse; no web ele fica preso
// na base e so o topo se move.
// Quanto do titulo ja foi assistido, 0..100. 0 quando nunca comecou.
static int progressoDe(int i) {
  const CatItem *c = cat_item(i);
  return c ? c->progresso : 0;
}

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
  }
  if (circular) {
    float ic;
    // A PELE E A DA TABELA (botoes.h, 21/09/2026): repouso 0.14/0.15/0.17 e
    // nao #222, foco realce + tinta + luz. O tamanho (escala no foco) e o
    // glifo continuam daqui.
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
      gfx_icone(ig, progressoDe(idx) >= 90 ? "visto" : "naovisto", ic, ic, ic, a);
    } else if (icone == ACAO_RECOMENDAR) {
      // AVIAO DE PAPEL — o mesmo vocabulario dos vizinhos: um PNG de
      // deploy/app/art/icones com a forma na alpha, desenhado com GFX_MARCA e
      // colorido daqui. Nao e glifo de fonte e nao e forma montada no shader:
      // o dono ja recusou os desenhados a mao ("usa SVG reais", ver gfx.h).
      gfx_icone(ig, "recomendar", ic, ic, ic, a);
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
    if (focado) { tinta = focoAcento(&fr, &fg, &fb); luzFoco(r, a); }
    gfx_cor(r, NV_RAIO_PILL, fr, fg, fb, a);
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

// Botao secundario: 345x96, raio 64, fundo #222 e texto branco; focado, fundo
// #f5f5f5 e texto #111, com o mesmo anel de 4px. Nao tem icone — no web e so o
// rotulo, e por isso a largura sai de `texto + 2 x 34` e nao da conta do
// primario.
static void desenhaSecundario(GfxRect r, const char *rot, int focado, float a) {
  // A PELE DO SECUNDARIO DA TABELA (botoes.h): contorno de 1,5 px a 22 % em
  // repouso, realce + tinta + luz em foco. A largura e a altura continuam
  // as medidas desta tela.
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
  GLuint texLogo = arqLogo ? tex_obter_larg_qualquer(arqLogo, NV_DETW_LOGO_MAXW) : 0;
  float baseY = NV_TELA_H - 96.0f;
  if (a <= 0.005f) return;
  if (texLogo) {
    float asp = tex_aspecto(arqLogo);
    float h, w;
    if (asp <= 0.0f) asp = 2.5f;
    h = NV_DETW_LOGO_H * 0.62f; w = h * asp;
    if (w > NV_DETW_LOGO_MAXW * 0.5f) { w = NV_DETW_LOGO_MAXW * 0.5f; h = w / asp; }
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

static void heroWeb(float a, float desloc) {
  if (a <= 0.005f) return;
  const CatItem *ci = cat_item(idx);

  char ano[32], dur[64];
  partirMeta(fichaDe(idx), ano, sizeof ano, dur, sizeof dur);

  // Em serie o web escreve "Roteirista:"/"Criador:"; em filme, "Diretor:".
  char sup[192] = "";
  if (ci && ci->direcao[0])
    snprintf(sup, sizeof sup, "%s: %s", i18n(ehSerie() ? "Roteirista" : "Diretor"),
             ci->direcao);

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
  float yMeta2 = NV_DETW2_BASE - NV_DETW2_SELO_H;
  float yMeta1 = yMeta2 - NV_DETW2_META_GAP - NV_DETW2_M1_H;
  float ySin   = yMeta1 - NV_DETW2_GAP_SIN - hSin;
  float ySup   = sup[0] ? ySin - NV_DETW2_GAP_SUP : ySin;
  float yAcoes = ySup - NV_DETW2_GAP_ACOES - NV_DETW2_BTN_H;
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
  if (agLinha[0])      { yEstado -= NV_DETW_RETOM_H; yAgenda = yEstado; }

  // Sobe alguns pixels enquanto entra: continua o movimento da arte em vez de
  // aparecer pronto no lugar. `desloc` e a rolagem do documento.
  float sobe = (1.0f - a) * 26.0f + desloc;
  yMeta2 += sobe; yMeta1 += sobe; ySin += sobe; ySup += sobe;
  yRetom += sobe; yAgenda += sobe; yAcoes += sobe; yEstado += sobe;

  // --- logo -----------------------------------------------------------------
  const char *arqLogo = logoDe(idx);
  // O logo e desenhado com 261 de largura mas a arte de origem costuma vir bem
  // maior; o teto de 960 ja bastaria, mas quando a mesma arte tambem serve ao
  // hero o item e promovido — por isso passa pelo mesmo caminho.
  // tex_obter_larg com a largura que o logo OCUPA, e nao tex_obter (teto
  // generico): com a URL unica de artehero_url_logo_larg o card aberto ja
  // decodificou este mesmo arquivo em ~370 px, e o cache so entrega uma
  // textura menor enquanto reprocessa se ela tiver ao menos metade do
  // pedido. Pedindo o teto, o logo sumia por um instante ao abrir o titulo;
  // pedindo a largura real, aparece na hora e troca pela nitida em seguida.
  GLuint texLogo = arqLogo ? tex_obter_larg_qualquer(arqLogo, NV_DETW_LOGO_MAXW) : 0;
  // O NOME ESCRITO E SO RESERVA. Enquanto o logo ainda esta a caminho a caixa
  // fica vazia, e o logo entra num fade curto — em vez de o nome aparecer e
  // ser trocado pelo logo um instante depois, a cada abertura ("nao ta suave
  // como o fundo do hero", dono, 21/09/2026). O nome so sai quando NAO ha logo
  // para esperar: o titulo nao tem, ou o cache ja tentou e falhou.
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
  int mostraNome = !texLogo && (!arqLogo || tex_falhou(arqLogo));
  if (texLogo) {
    float asp = tex_aspecto(arqLogo);
    if (asp <= 0.0f) asp = 2.5f;
    float h = NV_DETW_LOGO_H, w = h * asp;
    if (w > NV_DETW_LOGO_MAXW) { w = NV_DETW_LOGO_MAXW; h = w / asp; }
    // O logo assenta acima do TOPO DO BLOCO DE ESTADO, seja ele qual for.
    //
    // DEFEITO CORRIGIDO em 16/09, visto pelo dono numa serie encerrada: esta
    // linha olhava so `temRetom`. Com logo na tela, sem progresso e COM linha
    // de agenda ("Show ended · last episode on 13 November 2025"), o logo se
    // ancorava em `yAcoes` enquanto a agenda era desenhada uma linha acima
    // disso — ou seja, DENTRO da caixa do logo. As duas coisas saiam uma por
    // cima da outra.
    //
    // `yEstado` ja e o topo do bloco depois de empilhar retomada e agenda (zero,
    // uma ou as duas), que e exatamente a ancora que o ramo SEM logo, dez linhas
    // abaixo, sempre usou. Os dois ramos agora concordam, que e o que impede de
    // o defeito voltar no proximo estado novo.
    float baseLogo = (temRetom > 0.0f || agLinha[0] ? yEstado : yAcoes)
                     - NV_DETW_LOGO_GAP;
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
      gfx_rect(r, texLogo, m, 0, 0, 0, 0.0f, 1, 1, 1, aLogo); }
  } else if (mostraNome) {
    // Sem logo, o NOME. A altura da caixa continua sendo a do logo, para que a
    // linha de botoes nao pule entre um titulo com logo e outro sem.
    const char *nome = tituloDe(idx);
    if (nome) {
      TxtLinha t2 = txt_linha_corta(TXT_TITULO1, nome, 255, 255, 255, 255,
                                    NV_DETW_LOGO_MAXW);
      // Mesma ancora do logo: acima da retomada quando ha, senao das acoes. A
      // altura da CAIXA continua sendo a do logo, para que a linha de acoes nao
      // pule entre um titulo com logo e outro sem.
      float baseLogo = (temRetom > 0.0f || agLinha[0] ? yEstado : yAcoes)
                       - NV_DETW_LOGO_GAP;
      txt_desenhar_alpha(t2, NV_DETW2_X,
                         baseLogo - NV_DETW_LOGO_H
                                  + (NV_DETW_LOGO_H - t2.h) * 0.5f, a);
    }
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
    else if (ci && ci->progresso > 0) snprintf(rot, sizeof rot, "Retomar");
    else snprintf(rot, sizeof rot, "Reproduzir"); }

  { float cyBtn = yAcoes + NV_DETW2_BTN_H * 0.5f;
    int nb = 0, n = nBotoes();
    // Trocar de titulo com o foco no ultimo circular de um FILME e cair numa
    // serie deixaria `botao` = 3 numa linha de 3 botoes: nenhum apareceria
    // focado e o OK nao acharia acao. Fixa aqui, no desenho, que e por onde
    // todo quadro passa.
    if (botao >= n) botao = n - 1;
    float bx = NV_DETW2_X;
    GfxRect rp = { bx, yAcoes, larguraPrimario(rot), NV_DETW2_BTN_H };
    desenhaBotao(rp, rot, 0, nivel == 0 && botao == nb, a);
    bx += rp.w + NV_DETW2_BTN_GAP; nb++;
    if (temInicio()) {
      // "Assistir do comeco" entre o primario e os circulares (issue #46).
      const char *rotIni = i18n("Assistir do começo");
      GfxRect rs = { bx, yAcoes, larguraSecundario(rotIni), NV_DETW2_BTN_H };
      desenhaSecundario(rs, rotIni, nivel == 0 && botao == nb, a);
      bx += rs.w + NV_DETW2_BTN_GAP; nb++;
    }
    if (temLembrar()) {
      const CatItem *ciL = cat_item(idx);
      GfxRect rs = { bx, cyBtn - NV_DETW2_CIRC * 0.5f,
                     NV_DETW2_CIRC, NV_DETW2_CIRC };
      desenhaLembrete(rs, ciL && agenda_lembrete(ciL->imdb),
                      nivel == 0 && botao == nb, a);
      bx += NV_DETW2_CIRC + NV_DETW2_BTN_GAP; nb++;
    }
    for (; nb < n; nb++) {
      GfxRect rc = { bx, cyBtn - NV_DETW2_CIRC * 0.5f,
                     NV_DETW2_CIRC, NV_DETW2_CIRC };
      desenhaBotao(rc, NULL, acaoEm(nb), nivel == 0 && botao == nb, a);
      bx += NV_DETW2_CIRC + NV_DETW2_BTN_GAP;
    } }

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

  // --- "Roteirista: ..." / "Diretor: ..." ------------------------------------
  // Mesmo CORPO da sinopse, e nao um menor: na referencia o "R" de "Roteirista"
  // e o "C" da sinopse medem os mesmos 20 de altura de caixa alta. Estava em
  // TXT_DET_META (25) contra TXT_DET_SIN (26) por uma medida do web, onde as
  // duas linhas de fato divergem.
  if (sup[0]) {
    TxtLinha l = txt_linha_corta(TXT_DET_SIN, sup, 179, 179, 179, 255,
                                 NV_DETW2_TEXTO_W);
    txt_desenhar_alpha(l, NV_DETW2_X, ySup, a);
  }

  // --- sinopse --------------------------------------------------------------
  if (sin) txt_bloco(TXT_DET_SIN, sin, 255, 255, 255, NV_DETW2_X, ySin,
                     NV_DETW2_TEXTO_W, NV_DETW2_LD_SIN, a, NV_DETW2_SIN_LINHAS);

  // --- meta linha 1: generos • generos  ·  ano  ·  [IMDb] nota ---------------
  //
  // Uma linha so, na ordem do aparelho. O selo do IMDb entra AQUI, no fim dos
  // grupos, e nao encostado na borda direita da tela: ele estava orfao, a mais
  // de 1000px do texto de que faz parte, porque o valor herdado era
  // NV_DETW_DIR.
  //
  // Dois pontos separadores diferentes, e a diferenca de cor e o que agrupa a
  // linha — ver desenhaPonto.
  {
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
    if (ci && ci->nota > 0) {
      if (algo) { desenhaPonto(x + NV_DETW2_SEP, yc, 0.502f, a);
                  x += NV_DETW2_SEP * 2 + NV_DETW2_PONTO_D; }
      x += desenhaSeloImdb(x, yc, ci->nota, a);
    }
    const int fontes[] = { EX_TOMATOES, EX_TRAKT };
    for(int i=0;i<2;i++) {
      int n=extras_nota(fontes[i]);
      if(n<=0) continue;
      // Rotten Tomatoes: tomate FRESCO de 60% para cima, o RESPINGO verde
      // abaixo — e a convencao do proprio site, e o icone e que diz o
      // veredito antes do numero. Trakt: o WORDMARK (nome), nao o icone.
      const char *marca;
      if(fontes[i]==EX_TOMATOES) marca=extras_caminho_marca_nome(n>=600?"tomatoes_fresh":"tomatoes_rotten");
      else marca=extras_caminho_marca_nome("trakt_wordmark");
      GLuint logo=tex_obter(marca);
      char valor[20];snprintf(valor,sizeof valor,"%d%%",n/10);
      TxtLinha lv=txt_linha(TXT_DET_META2,valor,220,220,225,255);
      float mh=fontes[i]==EX_TRAKT?22.0f:32.0f,mw=mh;
      if(logo){float ap=tex_aspecto(marca);if(ap>0)mw=mh*ap;if(mw>110)mw=110;}
      if(x+24+mw+10+lv.w>NV_DETW2_X+NV_HERO_SIN_W)break;
      x+=24;
      // GFX_TEXTO e nao GFX_SNAP: o SNAP ignora o alfa da textura e o tomate saia
      // com um quadrado escuro em volta. O TEXTO preserva o RGB e usa o alfa.
      // O wordmark do Trakt e escuro: vai por GFX_MARCA, que tinge o alfa.
      if(logo){GfxModo m=fontes[i]==EX_TRAKT&&tex_marca_escura(marca)?GFX_MARCA:GFX_TEXTO;
        gfx_rect((GfxRect){x,yc-mh*.5f,mw,mh},logo,m,0,0,0,0,.93f,.94f,.96f,a);}
      else {TxtLinha label=txt_linha(TXT_MINI,extras_fonte_marca(fontes[i]),200,200,205,255);
        txt_desenhar_alpha(label,x,yc-label.h*.5f,a);mw=label.w;}
      x+=mw+10;txt_desenhar_alpha(lv,x,yc-lv.h*.5f,a);x+=lv.w;
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
              <= NV_DETW2_X + NV_HERO_SIN_W) {
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
    float x = NV_DETW2_X, yc = yMeta2 + NV_DETW2_SELO_H * 0.5f;
    int algo = 0;
    const char *status=NULL,*raw=extras_ficha_status();
    if(ehSerie()) {
      if(!strcmp(raw,"canceled")||!strcmp(raw,"Canceled"))status="CANCELADA";
      else if(!strcmp(raw,"ended")||!strcmp(raw,"Ended"))status="FINALIZADA";
      else if(!strcmp(raw,"returning series"))status="EM EXIBIÇÃO";
      else if(!strcmp(raw,"renewed"))status="RENOVADA";
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
      TxtLinha lp = txt_linha(TXT_DET_META2, ci->pais, 255, 255, 255, 255);
      txt_desenhar_alpha(lp, x, yc - lp.h * 0.5f, a);
    }
  }
}

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
static float larguraTemporada(int c) {
  char rot[32]; rotuloTemporada(c, rot, sizeof rot);
  TxtLinha l = txt_linha(TXT_PLR_CORPO, rot, 255, 255, 255, 255);
  return l.w + NV_DETP_TEMP_PADX * 2;
}
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

static void resumoTemporada(float x, float yPilulas, float a) {
  char linha[192];
  int total = 0, vistos = 0, futuros = 0, sabe = 0;
  size_t k = 0;
  contarTemporada(&total, &vistos, &futuros, &sabe);
  if (total <= 0) return;
  // O formato passa por i18n ANTES do snprintf, como a linha de progresso do
  // heroi ja faz: a string montada nunca casaria com uma chave da tabela.
  //
  // COM HISTORICO A LINHA E UMA FRACAO, e nao dois numeros soltos. "24
  // episódios · 11 assistidos" obriga o olho a subtrair para saber onde a
  // pessoa esta; "11 de 24 assistidos" ja E a resposta, e e a MESMA forma que a
  // linha de progresso do heroi usa para a serie inteira ("%d de %d") — a
  // mesma ideia escrita do mesmo jeito nos dois lugares da tela.
  //
  // Sem historico nao ha fracao possivel e sobra o total, que continua sendo
  // informacao nova: quantos episodios esta temporada tem.
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
  // #BEC0C8: o degrau de "rotulo, legenda, valor de referencia" da escada de
  // texto (DESIGN.md secao 2). Nao e o piso de rodape (#9699A2) porque isto e
  // a informacao que a fileira passou a dar, e nao uma nota de procedencia; e
  // nao e branco porque branco aqui competiria com o titulo da obra logo acima
  // e com o rotulo das proprias pilulas logo abaixo.
  { TxtLinha l = txt_linha(TXT_DET_META2, linha, 190, 192, 200, 255);
    txt_desenhar_alpha(l, x, yPilulas - NV_DETP_TEMP_RESUMO_DY - l.h, a); }
}

// Aba de temporada: 80 de altura, raio 40 (pilula), borda de 1px
// rgba(255,255,255,0.16). Tres estados MEDIDOS, e nao dois:
//   normal      #222     texto rgb(179,179,179)
//   escolhida   #2d2d2d  texto branco
//   com foco    #f5f5f5  texto #111, sem borda
// Sem o estado do meio, o usuario perde de vista em que temporada esta assim
// que o foco desce para a lista.
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
  { float br = sel ? fr * 0.6f : 0.133f, bg = sel ? fg * 0.6f : 0.133f,
          bb = sel ? fb * 0.6f : 0.133f;
    gfx_cor(r, raio, br + (fr - br) * f, bg + (fg - bg) * f, bb + (fb - bb) * f, a); }
  // Texto: cinza claro na que nao e nada; sobre o realce, a tinta que
  // contrasta com ele (ti), em degrau no meio da mola.
  { int t = (int)(ti * 255.0f + 0.5f);
    int cor = (sel || f > 0.5f) ? t : 179;
    TxtLinha l = txt_linha(TXT_PLR_CORPO, rot, cor, cor, cor, 255);
    // 500 de peso na Inter Regular: uma segunda passada meio pixel a direita.
    txt_peso(l, r.x + (r.w - l.w) * 0.5f, r.y + (r.h - l.h) * 0.5f, a, 0.5f); }
}

// O degrade do `.series-episode-overlay`: linear vertical de rgba(0,0,0,0.06)
// a 0.95, com paradas em 22% (0.18), 52% (0.62) e 82% (0.86). O shader nao tem
// modo para ele — gfx.c e arquivo de outro agente — e o GFX_VEU que existe
// escurece TAMBEM a esquerda, o que aqui apagaria a metade do card.
//
// Sai em faixas ancoradas na BASE: cada faixa e um retangulo arredondado que
// vai de uma altura ate o fim da miniatura, com o mesmo raio absoluto. Assim os
// cantos de baixo acompanham a miniatura (uma faixa de cantos retos poria dois
// dentes escuros fora do arredondamento) e o empilhamento reproduz a rampa,
// porque compor N camadas de alfa `d` da 1-(1-d)^n.
// O VEU DO CARD, EM UMA PASSADA E SEM FAIXAS.
//
// Eram 14 retangulos empilhados, um por degrau da rampa, cada um um quad de
// largura inteira com SDF. Numa TV de 55" os degraus SE VEEM: o dono mandou a
// foto do card com as faixas contaveis a olho. Subir o numero de degraus nao
// resolve — o olho enxerga a segunda derivada e a emenda entre faixas continua
// aparecendo, que e a mesma razao pela qual GFX_VEU_BAIXO eleva o smoothstep ao
// quadrado.
//
// A rampa nao mudou (as cinco paradas do `linear-gradient` do web); ela so
// passou a ser avaliada no fragmento. Ver GFX_VEU_CARD em gfx.h. De quebra sao
// 14 passadas de preenchimento a menos por card, numa Mali-G71 que ja e o
// gargalo desta tela.
//
// O RAIO SAI DA ALTURA, e nao de min(w,h). O shader normaliza por
// `p = (uv - 0.5) * vec2(w/h, 1.0)`: a meia-extensao VERTICAL e sempre 0,5,
// entao `raio * h` e o raio em pixels e a largura nao entra na conta (ver
// gfx.h). Aqui os dois davam no mesmo por acaso — a miniatura e 640x414, e o
// menor lado E a altura —, mas o idioma errado ja produziu quatro defeitos
// fotografados neste repositorio, e um card mais estreito que alto (se um dia
// a fileira mudar de forma) sairia com a ponta em capsula.
static void veuEpisodio(GfxRect th, float a) {
  float raio = NV_DETP_EP_RAIO / th.h;
  if (raio > 0.5f) raio = 0.5f;
  if (raio > 0.5f * th.w / th.h) raio = 0.5f * th.w / th.h;
  gfx_rect(th, 0, GFX_VEU_CARD, 0, 0, 0, raio, 0, 0, 0, a);
}

// Nota de episodio: provedor pequeno, valor em primeiro plano e um ponto na
// cor de accent. A versao anterior era um retangulo cinza com toda a frase na
// mesma hierarquia; parecia uma legenda solta e nao um dado do card.
static float desenhaNotaEpisodio(float x, float y, const char *fonte,
                                 int nota, float a) {
  char valor[12];
  float ar, ag, ab;
  TxtLinha lf, lv;
  GfxRect selo;
  float ponto = 6.0f, pad = 10.0f, gap = 7.0f;
  if (nota <= 0) return 0.0f;
  snprintf(valor, sizeof valor, ajustes_idioma_ingles() ? "%d.%d" : "%d,%d",
           nota / 10, nota % 10);
  lf = txt_linha(TXT_MINI, fonte, 154, 159, 172, 255);
  lv = txt_linha(TXT_CAPTION2, valor, 242, 245, 250, 255);
  selo = (GfxRect){ x, y, pad + ponto + gap + lf.w + gap + lv.w + pad, 28.0f };
  ajustes_acento(&ar, &ag, &ab);
  // Fundo quase-preto com uma lavagem mínima do accent: a marca continua
  // discreta sobre a foto e deixa de parecer um bloco cinza genérico.
  gfx_cor(selo, 0.5f, .055f + ar * .06f, .062f + ag * .06f,
          .078f + ab * .07f, .94f * a);
  gfx_cor((GfxRect){x + pad, y + (selo.h - ponto) * .5f, ponto, ponto},
          0.5f, ar, ag, ab, a);
  txt_desenhar_alpha(lf, x + pad + ponto + gap,
                     y + (selo.h - lf.h) * .5f + 1.0f, a);
  txt_desenhar_alpha(lv, x + pad + ponto + gap + lf.w + gap,
                     y + (selo.h - lv.h) * .5f, a);
  return selo.w + 12.0f;
}


// Card de episodio: 640x422, com a miniatura de 640x414 e TODO o texto dentro
// dela, sobre o degrade. E a diferenca estrutural com o que estava aqui antes
// (miniatura em cima, texto embaixo, que e o app da Apple TV).
static void desenhaEpisodio(GfxRect r, int c, float f, float a, Uint32 agora) {
  (void)agora;
  const CatEp *ep = cat_episodio(idx, epAbsoluto(c));
  GfxRect th = { r.x, r.y, r.w, NV_DETP_EP_THUMB_H };
  float raioTh = NV_DETP_EP_RAIO / NV_DETP_EP_THUMB_H;

  // Foco: no web e um box-shadow na MINIATURA, nao no card, e nao ha escala
  // nenhuma (`transform: none`). A cor acompanha o accent escolhido — o anel
  // branco fixo fazia esta fileira destoar justamente quando o resto da tela
  // ja seguia o tema.
  if (f > 0.01f) {
    GfxRect anel = { th.x - NV_DETP_ANEL, th.y - NV_DETP_ANEL,
                     th.w + NV_DETP_ANEL * 2, th.h + NV_DETP_ANEL * 2 };
    float ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    gfx_cor(anel, raioTh, ar, ag, ab, f * a);
  }

  const CatItem *serie = cat_item(idx);
  const char *arte = (ep && ep->thumb[0]) ? ep->thumb
                     : (serie && serie->backdrop[0] ? serie->backdrop : NULL);
  GLuint t2 = arte ? tex_obter_larg(arte, th.w) : 0;
  if (t2) {
    gfx_tex_aspect_atual = tex_aspecto(arte);
    gfx_rect(th, t2, GFX_CARD, 0, 0, 0, raioTh, 0, 0, 0, a);
    gfx_tex_aspect_atual = 0.0f;
  } else gfx_cor(th, raioTh, 0.133f, 0.133f, 0.133f, a);
  veuEpisodio(th, a);

  // EPISODIO JA ASSISTIDO, segundo o Trakt: mascara escura sobre a miniatura e
  // um check no canto. Pedido do dono, e resolve uma pergunta que a lista nao
  // respondia — onde ele parou.
  //
  // A mascara vem DEPOIS do veu de texto de proposito: ela precisa cobrir a
  // miniatura inteira, inclusive a parte ja escurecida, senao o card visto e o
  // nao visto ficam parecidos justo em cima do texto.
  //
  // A FONTE E O MAPA (vistoep), e nao mais a matriz de extras.c. Era o defeito
  // que o dono relatou assim: "se eu desmarcar ou marcar como assistido ele nao
  // atualiza os cards". O menu de visto escreve em vistoep_marcar_lote e este
  // card lia extras_ep_visto — duas verdades diferentes, e a que a pessoa
  // acabava de mudar nao era a desenhada.
  //
  // O aviso ja estava escrito, em episodios.c, quando a FOLHA passou pelo mesmo
  // conserto: "desenhar de uma fonte e agir sobre outra faria a linha nao mudar
  // depois do gesto". A folha foi arrumada, esta copia nao — e nada apontava de
  // uma para a outra.
  //
  // A matriz tambem so guarda o "sim": ela nao distingue "nao viu" de "nao
  // sei", e cortava em silencio a temporada 21 e o episodio 40.
  if (ep && serie && serie->imdb[0] &&
      vistoep_estado(serie->imdb, ep->temporada, ep->episodio) == 1) {
    float d = 36.0f;
    GfxRect selo = { th.x + th.w - d - 16.0f, th.y + 16.0f, d, d };
    gfx_cor(th, raioTh, 0.0f, 0.0f, 0.0f, 0.22f * a);
    gfx_cor(selo, 0.5f, 1, 1, 1, 0.92f * a);
    // O check e o icone (art/icones/check.png, o mesmo do card da home), nao
    // mais dois tracos feitos de quadradinhos em degrau — a 36 px isso era o
    // "tick de baixa resolucao" do #74.
    gfx_icone((GfxRect){ selo.x + 7.0f, selo.y + 7.0f, d - 14.0f, d - 14.0f }, "check",
              0.05f, 0.05f, 0.05f, a);
  }

  // NADA DE RESERVA INVENTADA. Aqui as quatro linhas caiam numa tabela de
  // demonstracao (nome, duracao, data e sinopse de "Shrinking"), entao um
  // episodio sem dado nao aparecia vazio: aparecia com o TEXTO DE OUTRA SERIE,
  // indistinguivel de informacao real. Campo ausente agora fica ausente, e o
  // desenho abaixo ja omite cada pedaco que vier vazio.
  const char *epNome = (ep && ep->nome[0])    ? ep->nome    : NULL;
  const char *epDur  = (ep && ep->duracao[0]) ? ep->duracao : NULL;
  const char *epData = (ep && ep->data[0])    ? ep->data    : NULL;
  const char *epSin  = (ep && ep->sinopse[0]) ? ep->sinopse : NULL;
  int epNum = ep ? ep->episodio : c + 1;

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
      TxtLinha l = txt_linha(TXT_CAPTION2, rot, 20, 20, 20, 255);
      float w = l.w + NV_DETP_EP_SELO_PADX * 2;
      GfxRect s = { xs, r.y + NV_DETP_EP_SELO_Y, w, NV_DETP_EP_SELO_H };
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
  if (epSin)
    txt_bloco(TXT_DET_SIN, epSin, 255, 255, 255, tx, r.y + NV_DETP_EP_SIN_Y,
              NV_DETP_EP_TEXTO_W, NV_DETP_EP_LD_SIN, a * 0.9f, 3);

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
      x += desenhaNotaEpisodio(x, y - 3, "Trakt", nota, a);
    // O voto do TMDB entra AO LADO do do Trakt, no mesmo selo compacto
    // (issue #87). Sao fontes diferentes — o rotulo diz qual e qual, e um
    // episodio pode ter uma, a outra ou as duas.
    if (ep && ep->nota > 0)
      x += desenhaNotaEpisodio(x, y - 3, "TMDB", ep->nota, a);
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
      GfxRect at = { tr.x, tr.y, tr.w * (prog / 100.0f), tr.h };
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
static void desenhaTrailer(float x, float y, int c, float a) {
  const char *mini = extras_trailer_miniatura(c);
  GfxRect v = { x, y, NV_DETF_TR_W, NV_DETF_TR_VIDEO_H };
  float raio = NV_DETF_TR_RAIO / NV_DETF_TR_VIDEO_H;   // fracao do MENOR lado
  GLuint tex = (mini && mini[0]) ? tex_obter_larg(mini, NV_DETF_TR_W) : 0;

  if (tex) {
    gfx_tex_aspect_atual = tex_aspecto(mini);
    gfx_rect(v, tex, GFX_CARD, 0, 0, 0, raio, 1, 1, 1, a);
    gfx_tex_aspect_atual = 0.0f;
  } else {
    gfx_cor(v, raio, 0.13f, 0.13f, 0.13f, a);
  }

  // Selo de play: disco escuro e o triangulo por cima, centrados na miniatura.
  { float d = NV_DETF_TR_PLAY_D;
    GfxRect disco = { x + (NV_DETF_TR_W - d) * 0.5f,
                      y + (NV_DETF_TR_VIDEO_H - d) * 0.5f, d, d };
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

  GfxRect av = { x, y, NV_DETP_EL_AVATAR, NV_DETP_EL_AVATAR };
  if (f > 0.01f) {
    GfxRect anel = { av.x - NV_DETP_ANEL, av.y - NV_DETP_ANEL,
                     av.w + NV_DETP_ANEL * 2, av.h + NV_DETP_ANEL * 2 };
    float ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    gfx_cor(anel, 0.5f, ar, ag, ab, f * a);
  }
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
  float yn = y + NV_DETP_EL_AVATAR + NV_DETP_EL_NOME_DY;
  TxtLinha ln = txt_linha_corta(TXT_CALLOUT, nome, 179, 179, 179, 255, NV_DETP_EL_W);
  txt_desenhar_alpha(ln, x, yn, a);
  if (papel && papel[0]) {
    TxtLinha lp = txt_linha_corta(TXT_CAPTION2, papel, 128, 128, 128, 255,
                                  NV_DETP_EL_W);
    txt_desenhar_alpha(lp, x, yn + NV_DETP_EL_PAPEL_DY, a * 0.95f);
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
  gfx_cor(r, raio, 0.082f, 0.094f, 0.118f, 0.92f * a);
  // SEM FIO DE CONTORNO. Havia aqui um anel branco a 14% que existia para
  // "fechar" o cartao sobre a arte de fundo — o dono mandou tirar em 16/09
  // ("tirar esse contorno daqui tb"), e ele estava contra a regra: nesta tela
  // contorno so aparece onde preencher e impossivel, e aqui o preenchimento
  // #151820 ja separa o cartao do fundo sozinho.
}

// Cartao de nota: a MARCA em cima e o valor embaixo, como o .movie-rating-card
// do web (logo 56x28, valor 34/800). As marcas sao os proprios arquivos do app
// web convertidos para PNG em art/marcas — desenhar um retangulo colorido com
// as iniciais, que era o que estava aqui, fica com cara de esboco ao lado de
// componentes que usam arte de verdade.
static void cartaoNota(float x, float y, const char *marca, const char *valor,
                       float a) {
  GfxRect card = { x, y, AVAL_CARD_W, AVAL_CARD_H };
  const char *cam = marca;
  GLuint t;
  moldura(card, 14.0f, a);
  t = tex_obter(cam);
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
      if (nd > 0) snprintf(nv, sizeof nv, "%.1f", nd / 10.0f);
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
    if (extras_fonte_percentual(i))
      snprintf(txt, sizeof txt, "%d%%", (v + 5) / 10);
    else
      snprintf(txt, sizeof txt, "%.1f", v / 10.0f);
    cartaoNota(x + col * (AVAL_CARD_W + AVAL_CARD_GAP), y,
               extras_caminho_marca(i), txt, a);
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
// "Mais como este" aparece por DOIS caminhos e eles nao sao o mesmo estado:
//   SERIE  -> e uma ABA, desenhada no slot de SEC_ELENCO, com foco proprio
//             (`relFoco`), porque a fileira do elenco tem outra contagem.
//   FILME  -> e uma SECAO propria, SEC_RELACIONADOS, e quem manda e `foco.coluna`.
//
// So o primeiro caso estava tratado. No filme a fileira RECEBIA foco (secaoN
// devolve a contagem certa) mas nada acendia e o OK nao respondia — parecia que
// a secao inteira nao existia para o D-pad. Este par resolve os dois de uma vez.
static int relNaLista(void) {
  return foco.fileira == SEC_ELENCO || foco.fileira == SEC_RELACIONADOS;
}
static int relIndice(void) {
  return (foco.fileira == SEC_RELACIONADOS) ? foco.coluna : relFoco;
}

static void desenhaRelacionados(float x, float y, float a) {
  int n = extras_n_relacionados(), i;
  int naLista = relNaLista();
  int foc = relIndice();
  for (i = 0; i < n && i < 7; i++) {
    float cx = x + i * (REL_CARD_W + REL_CARD_GAP);
    GfxRect r = { cx, y, REL_CARD_W, REL_CARD_H };
    int aceso = naLista && i == foc;
    const char *po = extras_relacionado_poster(i);
    GLuint t = po[0] ? tex_obter_larg(po, REL_CARD_W) : 0;
    float raio = raioCartaz(REL_CARD_W, REL_CARD_H);
    if (cx + REL_CARD_W > NV_TELA_W - NV_DETP_X) break;
    if (aceso) {
      GfxRect anel = { r.x - 4, r.y - 4, r.w + 8, r.h + 8 };
      gfx_cor(anel, raio, 1, 1, 1, a);
    }
    if (t) {
      gfx_tex_aspect_atual = tex_aspecto(po);
      gfx_rect(r, t, GFX_CARD, aceso ? 1.0f : 0.0f, 0, 0, raio, 0, 0, 0, a);
      gfx_tex_aspect_atual = 0.0f;
    } else {
      gfx_cor(r, raio, 0.133f, 0.133f, 0.133f, a);
    }
    { int c = aceso ? 255 : 225;
      TxtLinha lt = txt_linha_corta(TXT_DET_META2, extras_relacionado_titulo(i),
                                    c, c, c, 255, REL_CARD_W);
      txt_desenhar_alpha(lt, cx, y + REL_CARD_H + 12.0f, a);
      { const char *ano = extras_relacionado_ano(i);
        if (ano[0]) {
          TxtLinha la = txt_linha(TXT_MINI, ano, 140, 144, 153, 255);
          txt_desenhar_alpha(la, cx, y + REL_CARD_H + 12.0f + lt.h + 6.0f,
                             a * 0.9f);
        } } }
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
// GFX_MARCA, que e o unico jeito de ele nao sumir — e agora ele TROCA DE TINTA
// com o foco, porque a superficie embaixo dele troca de claro para escuro.
//
// FOCO PREENCHIDO, NAO CONTORNADO: era um halo branco a 35% em volta do
// cartao. A regra da casa nao tem excecao para cartao de marca.
static void desenhaEstudio(float x, float y, int i, float f, float a) {
  GfxRect r = { x, y, EST_CARD_W, EST_CARD_H };
  const char *logo = extras_estudio_logo(i);
  int claro = f > 0.5f;          // DEGRAU: linha ja rasterizada nao muda de cor
  GLuint t;
  moldura(r, 14.0f, a);
  if (f > 0.01f)
    gfx_cor(r, 14.0f / EST_CARD_H, 0.961f, 0.961f, 0.961f, a * f);
  t = logo[0] ? tex_obter(logo) : 0;
  if (t) {
    float ap = tex_aspecto(logo), w, h, fr, fg, fb;
    int recorte;
    if (ap <= 0.0f) ap = 3.0f;
    h = 52.0f; w = h * ap;
    if (w > EST_CARD_W - 36.0f) { w = EST_CARD_W - 36.0f; h = w / ap; }
    // RECORTE = a borda da imagem e transparente (tex_cor_fundo devolve 2).
    // E a mesma medida que o guia usa para decidir entre tingir e desenhar, e
    // ela olha o ARQUIVO, nao adivinha pelo nome.
    recorte = tex_cor_fundo(logo, &fr, &fg, &fb) != 1;
    { GfxRect rl = { x + (EST_CARD_W - w) * 0.5f,
                     y + (EST_CARD_H - h) * 0.5f, w, h };
      gfx_tex_aspect_atual = 0.0f;
      if (recorte && tex_marca_escura(logo)) {
        float tom = claro ? 0.09f : 0.93f;
        gfx_rect(rl, t, GFX_MARCA, 0, 0, 0, 0.0f, tom, tom, tom, a);
      } else {
        gfx_rect(rl, t, GFX_ARTE, 0, 0, 0, 8.0f / h, 1, 1, 1, a);
      } }
  } else {
    int c = claro ? 24 : 208;
    TxtLinha ln = txt_linha_corta(TXT_DET_META2, extras_estudio_nome(i),
                                  c, c, claro ? 30 : 220, 255,
                                  EST_CARD_W - 28.0f);
    txt_desenhar_alpha(ln, x + (EST_CARD_W - ln.w) * 0.5f,
                       y + (EST_CARD_H - ln.h) * 0.5f, a * 0.95f);
  }
}

// Aba da COLECAO: as partes da franquia, na ordem que o TMDB devolve. Mesma
// lista vertical de "Mais como este" — o que muda e a fonte e o cabecalho com
// o nome da colecao.
static void desenhaColecao(float x, float y, float a) {
  int n = extras_n_colecao(), i;
  float y0 = y;
  if (extras_colecao_nome()[0]) {
    TxtLinha ln = txt_linha_corta(TXT_DET_META2, extras_colecao_nome(),
                                  150, 154, 163, 255, 900.0f);
    txt_desenhar_alpha(ln, x, y0, a * 0.9f);
    y0 += ln.h + 16.0f;
  }
  for (i = 0; i < n && i < 7; i++) {
    float yl = y0 + i * 52.0f;
    int aceso = (foco.fileira == SEC_ELENCO) && i == relFoco;
    int c = aceso ? 255 : 225;
    if (aceso) {
      GfxRect faixa = { x - 16.0f, yl - 8.0f, 940.0f, 48.0f };
      gfx_cor(faixa, 10.0f / 48.0f, 1, 1, 1, 0.12f * a);
    }
    { TxtLinha lt = txt_linha_corta(TXT_DET_META, extras_colecao_titulo(i),
                                    c, c, c, 255, 900.0f);
      txt_desenhar_alpha(lt, x, yl, a);
      { const char *ano = extras_colecao_ano(i);
        if (ano[0]) {
          TxtLinha la = txt_linha(TXT_DET_META2, ano, 150, 154, 163, 255);
          txt_desenhar_alpha(la, x + lt.w + 18.0f, yl + 2.0f, a * 0.9f);
        } } }
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
  // Elenco e desenhado no proprio NV_DETP_EL_Y; as outras abas em EL_Y + 40
  // (o `yAba` de desenhaSecao). Sao dois pontos de partida diferentes.
  switch (abaIdDe(abaInfo)) {
    case ABA_RELACIONADOS:
      return NV_DETP_EL_Y + 40.0f + REL_CARD_H + 12.0f
           + NV_DETP_EL_LINHA * 2.0f;          // titulo + ano sob o cartaz
    case ABA_AVALIACOES:
      if (ehSerie() && extras_n_temporadas() > 0)
        return NV_DETP_EL_Y + 40.0f + RAT_TEMP_H + 18.0f + RAT_PIL_H;
      return NV_DETP_EL_Y + 40.0f + AVAL_CARD_H;
    case ABA_COLECAO: {
      int n = extras_n_colecao();
      if (n > 7) n = 7;
      return NV_DETP_EL_Y + 40.0f + 30.0f + (float)n * 52.0f;
    }
    default:
      return NV_DETP_EL_Y + NV_DETP_EL_AVATAR + NV_DETP_EL_NOME_DY
           + NV_DETP_EL_PAPEL_DY + NV_DETP_EL_LINHA * 2.0f;
  }
}

static float alturaCabComentarios(void) {
  return 46.0f + 44.0f + (ehSerie() ? COM_PILL_H : 0.0f) + 28.0f;
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
  // Wordmark. A marca ja esta em art/marcas/trakt.png, a mesma que a fileira de
  // notas usa — nao ha texto "trakt" desenhado com fonte, porque o logotipo tem
  // desenho proprio e escrever a palavra sairia diferente da referencia.
  //
  // GFX_CARD e nao GFX_MARCA/GFX_TEXTO, pelo mesmo motivo do cartao de nota: os
  // modos de forma pintam com a cor dada e descartam o RGB da textura, e o
  // wordmark viraria uma silhueta. gfx_icone tambem nao serve — ele monta o
  // caminho a partir de art/icones/, e a marca mora em art/marcas/.
  //
  // art/marcas/trakt_wordmark.png (282x106, com alfa) — o wordmark de verdade,
  // fornecido pelo dono. Antes eu desenhava aqui o LOGOMARK circular
  // (trakt.png, 96x96) esticado ate a largura de um wordmark, e saia um selo
  // vermelho deformado que nao era nem uma coisa nem outra.
  //
  // GFX_MARCA, e nao GFX_CARD: o modo de cartao IGNORA O ALFA da textura e
  // pinta o retangulo inteiro, entao saia uma CAIXA atras das letras — com o
  // arquivo antigo (captura de tela, fundo chapado) e com o vetorial tambem,
  // porque ali o fundo e transparente e o RGB por baixo e preto.
  //
  // GFX_MARCA existe exatamente para isto: a forma vem do ALFA e a cor vem de
  // uCor. Serve porque o wordmark e de UMA COR SO. Nao serviria para o selo do
  // IMDb, que e amarelo e preto e precisa do RGB do arquivo — e por isso o
  // cartao de nota continua em GFX_CARD.
  //
  // A altura manda e a largura sai do aspecto REAL do arquivo — cravar a
  // largura deformaria o desenho se a arte for trocada.
  float larguraMarca = 0.0f;
  { const char *cam = extras_caminho_marca_nome("trakt_wordmark");
    GLuint t = tex_obter_larg(cam, 160.0f);
    if (t) {
      float ap = tex_aspecto(cam);
      float h = 34.0f;
      if (ap <= 0.0f) ap = 282.0f / 106.0f;
      larguraMarca = h * ap;
      { GfxRect m = { x, yy + 6.0f, larguraMarca, h };
        gfx_tex_aspect_atual = 0.0f;
        gfx_rect(m, t, GFX_MARCA, 0, 0, 0, 0.0f, 1, 1, 1, a); }
      larguraMarca += 14.0f;
    } }
  // Sem a palavra "Comentários" ao lado do wordmark: o logo do trakt ja diz de
  // quem sao, e o subtitulo logo abaixo ja diz o que sao. Eram tres rotulos
  // para uma coisa so.
  (void)larguraMarca;
  yy += 46.0f;
  { TxtLinha ls = txt_linha(TXT_DET_META2, "Avaliações do Trakt", 179, 179, 179, 255);
    txt_desenhar_alpha(ls, x, yy, a * 0.95f); }
  yy += 44.0f;

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
        gfx_cor(rc, NV_RAIO_PILL, lum, lum, lum, a);
        if (f > 0.01f) { float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
                         gfx_cor(rc, NV_RAIO_PILL, ar, ag, ab, f * a); }
        else if (sel) { float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
                        gfx_cor(rc, NV_RAIO_PILL, ar * 0.6f, ag * 0.6f,
                                ab * 0.6f, a); }
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
        { TxtLinha l = txt_linha(TXT_PLR_CORPO, rotuloPilulaCom(k), cor, cor, cor, 255);
          txt_peso(l, r.x + (r.w - l.w) * 0.5f, r.y + (r.h - l.h) * 0.5f, a, 0.5f); } }
      px += w + COM_PILL_GAP;
    }
    yy += COM_PILL_H;
  }
  return yy + 28.0f;
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
    if (foc) { float fr, fg, fb; tintaEsc = ajustes_acento_tinta(&fr, &fg, &fb) < 0.5f;
      gfx_cor(card, 20.0f / (COM_CARD_W < COM_CARD_H ? COM_CARD_W
                                                     : COM_CARD_H),
              fr, fg, fb, a); }

    { TxtLinha lu = txt_linha_corta(TXT_ROW_TITULO,
                                    daSerie ? extras_comentario_usuario(i)
                                            : extras_comentario_ep_usuario(i),
                                    foc ? (tintaEsc ? 17 : 255) : 238, foc ? (tintaEsc ? 17 : 255) : 241,
                                    foc ? (tintaEsc ? 20 : 255) : 248, 255, larg);
      txt_desenhar_alpha(lu, px, y + COM_PAD, a); }

    // O texto para ANTES do rodape: sem o teto de linhas ele passava por cima
    // das curtidas. 6 linhas de 30 terminam em 246; o rodape comeca em 288.
    txt_bloco(TXT_DET_META2, daSerie ? extras_comentario_texto(i)
                                     : extras_comentario_ep_texto(i),
              foc ? (tintaEsc ? 45 : 235) : 190, foc ? (tintaEsc ? 47 : 236) : 195, foc ? (tintaEsc ? 52 : 240) : 205,
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
  TxtLinha lt = txt_linha(TXT_HEADLINE, i18n(titulo), 255, 255, 255, 255);
  TxtLinha lf = txt_linha_corta(TXT_DET_META2, i18n(fonte), 150, 153, 162, 255,
                                NV_TELA_W - NV_DETP_X * 2);
  TxtLinha lc = txt_linha_corta(TXT_DET_META2, i18n(custo), 108, 111, 120, 255,
                                NV_TELA_W - NV_DETP_X * 2);
  txt_desenhar_alpha(lt, x, y, a);
  txt_desenhar_alpha(lf, x, y + lt.h + 6.0f, a * 0.95f);
  txt_desenhar_alpha(lc, x, y + lt.h + 6.0f + lf.h + 10.0f, a * 0.9f);
  return lt.h + 6.0f + lf.h + 10.0f + lc.h;
}

// OS TRES GRAFICOS, empilhados. Cada um devolve o que ocupou — e a unica forma
// de saber a altura, porque ela depende do texto que coube e de quantos
// episodios responderam.
//
// O `a` da pagina nao e repassado: os modulos desenham com txt_desenhar e
// gfx_cor de alfa proprio, sem parametro de opacidade. Nao e problema aqui
// porque estas secoes ficam a ~1900 px do topo do documento e so aparecem com
// a pagina ja assentada (pg == 1); a abertura da tela nunca as mostra.
static float desenhaAudiencia(int banda, float x, float y, float a) {
  GfxRect r = { x, y, NV_TELA_W - NV_DETP_X * 2, 0.0f };
  // A CHAMADA fica so na PRIMEIRA banda: as outras duas nem existem antes de
  // alguem entrar (secaoN devolve 0), entao repetir o aviso tres vezes seria
  // tres vezes o mesmo paragrafo sobre a mesma coisa.
  if (!audAberta)
    return desenhaChamada(x, y, "Audiência da temporada",
                          "Quem marcou cada episódio no Trakt — não é a audiência geral",
                          "Uma consulta por episódio: carrega quando você desce até aqui",
                          a);
  if (banda == 0) return serieaud_arco(r);
  if (banda == 1) return serieaud_radar(r);
  return serieaud_digital(r);
}

// FRASES A ESQUERDA, FICHA A DIREITA. Sao duas fontes diferentes (Wikiquote e
// Wikidata) com coberturas MUITO diferentes — 11 de 14 filmes tem frases, 2 de
// 12 series; a ficha do Wikidata quase sempre tem algum campo —, e e por isso
// que elas ficam lado a lado e nao uma sob a outra: a coluna da direita e o que
// impede a secao de ser uma tela inteira com uma linha de "nao tem" no meio.
static float desenhaFrases(float x, float y, float a) {
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
    float yAba = NV_DETP_EL_Y - scrollY + 40.0f;
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
      desenhaRelacionados(NV_DETP_X, yAba, a); return;
    }
    if (r == SEC_ELENCO && aba == ABA_COLECAO) {
      desenhaColecao(NV_DETP_X, yAba, a); return;
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
    case SEC_ABAS_INFO:  y = NV_DETP_ABA_Y;  break;
    // A SECAO DO TRAKT E EMPILHADA, nao medida: ela vem DEPOIS do elenco e a
    // altura do elenco varia (nome comprido quebra em duas linhas). O `default`
    // abaixo mandava ela para NV_DETP_EL_Y, que e o y do PROPRIO elenco — por
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
    case SEC_FRASES:
    case SEC_COMENTARIOS:
    case SEC_ESTUDIOS:    y = conteudoSec[r]; break;
    default:             y = NV_DETP_EL_Y;   break;
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
  { const char *cab = ehSerie() ? NULL : cabecalhoDe(r);
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

  // Na serie a secao de estudios/redes nao tem cabecalho externo — o titulo
  // sai aqui, como o "trakt Comentarios" sai dentro da secao de comentarios.
  if (r == SEC_ESTUDIOS && ehSerie()) {
    TxtLinha lt = txt_linha(TXT_DET_META2, "Redes e estúdios", 150, 154, 163, 255);
    txt_desenhar_alpha(lt, NV_DETP_X, y, a * 0.9f);
  }

  for (int c = 0; c < n && c < N_ITENS; c++) {
    float f = animFoco[r][c];
    float x = xItem(r, c) - scrollSec[r];
    float w = larguraItem(r, c);
    if (x > NV_TELA_W || x + w < -w) continue;
    switch (r) {
      case SEC_TEMPORADAS: {
        GfxRect b = { x, y, w, NV_DETP_TEMP_H };
        desenhaTemporada(b, c, f, a); break;
      }
      case SEC_EPISODIOS: {
        GfxRect b = { x, y, NV_DETP_EP_W, NV_DETP_EP_H };
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
      case SEC_TRAILERS: desenhaTrailer(x, y, c, a); break;
      // Reaproveitam o desenho que ja servia as ABAS da serie: e o mesmo
      // conteudo, so que agora numa secao propria em vez de atras de uma aba.
      case SEC_RELACIONADOS:
        if (c == 0) {
          if (relacionadosCarregando()) desenhaEsqueletoRelacionados(y, a);
          else                          desenhaRelacionados(NV_DETP_X, y, a);
        }
        break;
      case SEC_COMENTARIOS:  desenhaComentarios(NV_DETP_X, y, a); break;
      // UMA VEZ SO, e nao uma por coluna: as colunas destas duas sao posicoes
      // dentro de um desenho unico (o episodio em destaque, a frase em
      // destaque), como os cartazes de "Mais como este" no filme. A altura
      // medida volta para o empilhamento do proximo quadro.
      case SEC_AUD_ARCO:
      case SEC_AUD_RADAR:
      case SEC_AUD_DIGITAL:
        if (c == 0)
          audAlt[r - SEC_AUD_ARCO] =
              desenhaAudiencia(r - SEC_AUD_ARCO, NV_DETP_X, y, a);
        break;
      case SEC_FRASES:
        if (c == 0) frasesAlt = desenhaFrases(NV_DETP_X, y, a);
        break;
      case SEC_ESTUDIOS:
        desenhaEstudio(x, y + (ehSerie() ? EST_TITULO_H : 0.0f), c, f, a);
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
  if (ye < NV_TELA_H && ye + NV_DETP_EP_H > 0) {
    for (c = 0; c < 3; c++) {
      float x = NV_DETP_X + c * NV_DETP_EP_PASSO;
      GfxRect card = { x, ye, NV_DETP_EP_W, NV_DETP_EP_H };
      GfxRect selo = { x + NV_DETP_EP_PAD, ye + NV_DETP_EP_SELO_Y,
                       108.0f, NV_DETP_EP_SELO_H };
      GfxRect titulo = { x + NV_DETP_EP_PAD, ye + NV_DETP_EP_TIT_Y,
                         292.0f, 25.0f };
      GfxRect sin1 = { x + NV_DETP_EP_PAD, ye + NV_DETP_EP_SIN_Y,
                       NV_DETP_EP_TEXTO_W, 18.0f };
      GfxRect sin2 = { sin1.x, sin1.y + NV_DETP_EP_LD_SIN, 420.0f, 18.0f };
      gfx_cor(card, NV_DETP_EP_RAIO / NV_DETP_EP_H,
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
    GfxRect av = { x, y, NV_DETP_EL_AVATAR, NV_DETP_EL_AVATAR };
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
  gfx_cor(tela, 0.0f, 0.051f, 0.051f, 0.051f, a);

  { GLuint t = pessoa_foto()[0] ? tex_obter(pessoa_foto()) : 0;
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
      if (i == pessoaFoco) {
        GfxRect anel = { r.x - 4, r.y - 4, r.w + 8, r.h + 8 };
        gfx_cor(anel, raioCartaz(PES_CARD_W, PES_CARD_H), 1, 1, 1, a);
      }
      if (t) {
        gfx_tex_aspect_atual = tex_aspecto(po);
        gfx_rect(r, t, GFX_CARD, i == pessoaFoco ? 1.0f : 0.0f, 0, 0,
                 raioCartaz(PES_CARD_W, PES_CARD_H), 0, 0, 0, a);
        gfx_tex_aspect_atual = 0.0f;
      } else {
        gfx_cor(r, raioCartaz(PES_CARD_W, PES_CARD_H), 0.13f, 0.13f, 0.13f, a);
      }
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


void detail_desenhar(Uint32 agora) {
  if (!aberto) return;
  float s = suave(t), a2 = fase2();

  if (!detail_cobre_tela()) {
    GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    gfx_cor(tela, 0.0f, 0.051f, 0.051f, 0.051f, s);   // #0d0d0d, o fundo do web
  }
  gfx_sem_recorte();

  // --- backdrop full-bleed --------------------------------------------------
  // A tela de detalhe e uma imagem de 1920x1080 em (0,0) com a vinheta por cima;
  // nao ha cartao, nem moldura, nem titulos vizinhos.
  //
  // O BACKDROP NAO CRESCE A PARTIR DO CARD. Era o ultimo resto do voo do app da
  // Apple: o retangulo saia de item.rect e se abria ate a tela. O dono descreveu
  // o comportamento certo — "so os posters descem e mantem o background, e o
  // background e a arte do filme selecionado" — e voar o retangulo e o oposto
  // disso: a arte entra pequena e cresce, em vez de ja estar la.
  //
  // Agora a arte ocupa a tela desde o primeiro quadro e so ganha opacidade. Quem
  // se move sao as fileiras da home, que descem (ver home_desenhar, que le o
  // detail_progresso).
  // O FUNDO NAO TROCA: ele CONTINUA. O hero da home ja mostrava a arte deste
  // mesmo titulo, entao o backdrop do detalhe nasce no rect exato em que ela
  // estava e cresce dali ate a tela cheia, sem piscar e sem crossfade — com o
  // hero em tela cheia os dois rects sao praticamente o mesmo e o olho nao ve
  // movimento nenhum, so o texto se rearranjando. Antes a arte entrava do zero
  // ganhando opacidade sobre a arte identica que ja estava la, o que dava um
  // clarao no meio da transicao.
  GfxRect alvo; float aEntrada;
  // TRAILER EM TELA CHEIA: a tela inteira e furo, nada da pagina por cima.
  if (trailer_cheia()) {
    GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    gfx_furo(tela);
    return;
  }
  backdropRect(&alvo, &aEntrada);
  const char *arte = arteDe(idx);
  int artePoster = arteDetalheEhPoster(idx);
  // Backdrop em tela cheia: pede o teto de 1920. Com o teto comum de 960 a arte
  // era decodificada com metade da resolucao e ampliada ao dobro na tela.
  GLuint tex = arte ? tex_obter_hero(arte) : 0;
  // Ao rolar, o web NAO desfoca a arte: ele a APAGA. Medido em
  // `.series-detail-shell.detail-scrolled` — o backdrop vai a `opacity: 0.15` e
  // a vinheta a 0, ambos em 0.8s cubic-bezier(.4,0,.2,1).
  //
  // O VEU E DE TELA CHEIA e custava caro numa GPU que ja estava afogada em
  // preenchimento (medido: clr=38,3ms com a CPU ociosa). Mas ele pinta
  // #0d0d0d — que e EXATAMENTE a cor com que main.c limpa o quadro
  // (NV_COR_FUNDO_*). Com a tela ja coberta pelo detalhe, embaixo dele nao ha
  // home nem outra tela: ha o glClear. Pintar #0d0d0d sobre #0d0d0d nao muda
  // um pixel, e a camada inteira sai.
  //
  // Fica quando a tela NAO esta coberta: ai embaixo ha a home, e o veu e o que
  // a apaga.
  if (pg > 0.01f && !detail_cobre_tela()) {
    GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    gfx_cor(tela, 0.0f, 0.051f, 0.051f, 0.051f, pg);
  }
  // 4o parametro = forca da VINHETA, nao "foco". Vai a 0 junto com a rolagem,
  // que e o par que faltava: o web apaga a arte para 15% E some com a vinheta
  // ao mesmo tempo. Poster reserva usa composição contida, sem crop de capa.
  desenhaArteDetalhe(alvo, tex, arte, artePoster,
                     tex ? aEntrada * (1.0f - 0.85f * pg) : 1.0f, pg);


  // O hero ROLA com o documento: ele nao some nem e substituido por um
  // cabecalho fixo. Era isso que fazia a pagina do port parecer outra tela em
  // vez da mesma tela rolada.
  // O conteudo SOBE para o lugar enquanto aparece, no lugar de so surgir: e a
  // contraparte do texto da home, que desce e apaga. Junto, le como um bloco
  // trocando de arranjo, que e o que o dono pediu.
  { float c = anim_suave(trailerCopy);
    // Modo cinema: o bloco desce 220 px enquanto apaga; o logo pequeno entra
    // no canto de baixo. As duas molas sao a mesma, entao o cruzamento e limpo.
    heroWeb(a2 * (1.0f - c), -scrollY + (1.0f - a2) * NV_TELA_H * 0.05f + c * 220.0f);
    if (c > 0.005f) logoCinema(c * a2); }


  if (pg <= 0.01f && scrollY < 1.0f) {
    if (pessoaAberta) desenhaPessoa(s);
    return;
  }
  desenhaEsqueletoEpisodios(pg);
  desenhaEsqueletoElenco(pg);
  for (int r = 0; r < N_SECOES; r++) desenhaSecao(r, pg, agora);
  // POR CIMA de tudo: a ficha e outra tela, nao uma secao desta.
  if (pessoaAberta) desenhaPessoa(s);
  // E o menu de visto por cima da ficha tambem: ele e o ultimo a abrir.
  episodios_menu_desenhar();
}

int detail_indice(void) { return idx; }
int detail_pediu_reproduzir(void) { int v = pedReproduzir; pedReproduzir = 0; return v; }
// Pedido de reproducao vindo de FORA da tela (OK no card de "Continuar
// assistindo", issue #93): o quadro seguinte cai no mesmo ramo de
// detail_pediu_reproduzir do app.c, com a pagina ja aberta por baixo.
void detail_pedir_reproduzir(void) { pedReproduzir = 1; }
int detail_pediu_abrir(void) { int v = pedAbrir; pedAbrir = -1; return v; }
int detail_pediu_assistido(void) { int v = pedAssistido; pedAssistido = 0; return v; }
int detail_pediu_marcar(void)     { int v = pedMarcar;     pedMarcar = 0;     return v; }
int detail_pediu_fontes(void)     { int v = pedFontes;     pedFontes = 0;     return v; }
// "Reproduzir desde o inicio" ainda cai no mesmo caminho do primario: o
// roteador so sabe abrir o player no ponto salvo. Consumir o pedido aqui evita
// que ele fique pendurado.
int detail_pediu_do_inicio(void)  { int v = pedDoInicio;   pedDoInicio = 0;   return v; }

// Uma vez por pedido: a pagina saiu por ESQUERDA e quer a barra lateral no
// lugar. app.c le depois de detail_aberto() virar 0.
int detail_pediu_menu(void) { int v = pediuMenu; pediuMenu = 0; return v; }
