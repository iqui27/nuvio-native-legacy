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
#include "idioma.h"
#include "badges.h"
#include "marco.h"
#include "ajustes.h"
#include "home.h"
#include "extras.h"
#include "vistoep.h"
#include "pessoa.h"
#include "streams.h"
#include "descoberta.h"
#include "diretor.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "focus.h"
#include "anim.h"
#include "layout.h"
#include "catalogo.h"
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
// MEDIDO na referencia (TCL, 1920x1080): cartao 722x466, vao 25, canto 20.
#define COM_CARD_W   722.0f
#define COM_CARD_H   466.0f
#define COM_PAD       28.0f
#define COM_CARD_GAP  25.0f   // MEDIDO

#define N_SECOES    8
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
static int temporada = 0;            // temporada ESCOLHIDA (nao a focada)
// Repouso do foco sobre a fileira de temporadas, para trocar de temporada ao
// PARAR numa pilula em vez de a cada pilula por que se passa.
static int    tempPend = 0;
// Episodio para o qual a aba de temporada APONTA. A rolagem da fileira de
// episodios usa este indice enquanto o foco esta na fileira de temporadas —
// antes a troca de temporada arrastava o FOCO para o episodio, e com isso o
// D-pad saia da fileira de abas: nao dava para passar da segunda temporada.
static int    epAncora = 0;
static Uint32 tempDesde = 0;
// Comentarios: 0 = da SERIE, 1 = do EPISODIO. E o seletor que a referencia poe
// sob "Avaliações do Trakt". Em filme nao existe e fica cravado em 0.
static int comentEp = 0;
static int abaInfo = 0;              // aba de informacao escolhida

// As quatro secoes do web, com o topo do GRUPO em coordenada de documento — e
// nao uma pilha de alturas somadas, que era o modelo do app da Apple TV. As
// posicoes sao fixas porque no web tambem sao: o documento tem tamanho
// conhecido e a rolagem so muda o quanto dele se enxerga.
typedef enum { SEC_TEMPORADAS, SEC_EPISODIOS, SEC_ABAS_INFO, SEC_ELENCO,
               SEC_TRAILERS, SEC_RELACIONADOS, SEC_COMENTARIOS,
               SEC_DETALHES } TipoSecao;
// Definida adiante, junto do resto das consultas ao catalogo; declarada aqui
// porque recalcularLayout, cabecalhoDe e nAvaliaveis, todas acima dela,
// precisam separar serie de filme.
static int ehSerie(void);
static float alturaCabComentarios(void);
static int temporadaEm(int c);
static float baseDaAbaAtiva(void);
// A fileira de comentarios e definida junto do desenho dela, la embaixo, mas a
// contagem de colunas e a largura de item — que ficam aqui em cima — precisam
// perguntar quantas pilulas e quantos cartoes ela tem.
#define COM_PILL_GAP  16.0f
static const char *COM_ROT[2];
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
    case SEC_DETALHES:     return "Detalhes do Filme";
    default:           return NULL;
  }
}

// A ABA DE TEMPORADA E UM ATALHO DE ROLAGEM, nao um filtro.
//
// Proposta do dono, e melhor que o que estava: a lista de episodios passou a
// ser UNICA (todas as temporadas, ordenadas), e a aba apenas leva o foco ao
// PRIMEIRO episodio daquela temporada. Nao ha recarga, nao ha rede, nao ha
// reconstrucao — a demora ao trocar de aba deixa de existir porque a troca
// deixa de acontecer.
static void irParaTemporada(int c, int moverFoco) {
  int alvo = temporadaEm(c), n = cat_n_episodios(idx), i;
  if (alvo <= 0 || n < 1) return;
  if (n > foco.nColunas[SEC_EPISODIOS]) n = foco.nColunas[SEC_EPISODIOS];
  for (i = 0; i < n; i++) {
    const CatEp *e = cat_episodio(idx, i);
    if (e && e->temporada == alvo) {
      // A ancora move a ROLAGEM sempre; o FOCO so quando o dono confirma com
      // OK ou desce para a fileira. Puxar o foco no simples passar por cima da
      // pilula tirava o dono da fileira de temporadas e prendia a navegacao
      // nas duas primeiras abas.
      epAncora = i;
      foco.colunaLembrada[SEC_EPISODIOS] = i;
      if (moverFoco) { foco.fileira = SEC_EPISODIOS; foco.coluna = i; }
      return;
    }
  }
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
    topoSec[SEC_COMENTARIOS] = conteudoSec[SEC_COMENTARIOS] = y;
    if (secaoN(SEC_COMENTARIOS) > 0) {
      float fim = y + alturaSecao(SEC_COMENTARIOS) + NV_DETF_PAD_FIM;
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
    if (i == EX_IMDB && !v) v = notaDe(idx);
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
static const char *logoDe(int i) {
  const CatItem *c = cat_item(i);
  return (c && c->logo[0]) ? c->logo : NULL;
}
static const char *arteDe(int i) {
  const CatItem *c = cat_item(i);
  // Um detalhe nunca pode herdar a arte de outra posicao do catalogo. Quando
  // o backdrop do proprio titulo falta, o renderer mostra o estado neutro e
  // preserva o layout, aguardando eventual enriquecimento do mesmo item.
  if (c && c->backdrop[0]) return c->backdrop;
  // Poster do próprio título é a reserva segura. O desenho trata-o como arte
  // contida, não como cover 16:9, para preservar rosto, lettering e proporção.
  if (c && c->poster[0]) return c->poster;
  return NULL;
}

static int arteDetalheEhPoster(int i) {
  const CatItem *c = cat_item(i);
  return c && !c->backdrop[0] && c->poster[0];
}

static void desenhaArteDetalhe(GfxRect alvo, GLuint tex, const char *arte,
                               int poster, float alpha, float pg) {
  if (!tex) {
    gfx_cor(alvo, 0.0f, 0.051f, 0.051f, 0.051f, alpha);
    return;
  }
  gfx_tex_aspect_atual = tex_aspecto(arte);
  if (!poster) {
    gfx_rect(alvo, tex, GFX_DETALHE, 1.0f - pg, 0, 0, 0.0f, 0, 0, 0,
             alpha);
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
  marco("detail_abrir");
  item = *it;
  aberto = 1; saindo = 0; nivel = 0; botao = 0;
  t = 0.0f; pg = 0.0f; scrollY = 0.0f; abaInfo = 0; pessoaAberta = 0;
  relFoco = 0; pedAbrir = -1; ratTemp = 0;
  idx = it->indice;
  revistaVista = cat_revisao();
  // Guarda identidade e copia ANTES de qualquer republicacao. Ver revalidarIdx.
  { const CatItem *ci0 = cat_item(idx);
    idxImdb[0] = 0; idxTemCopia = 0;
    if (ci0) {
      snprintf(idxImdb, sizeof idxImdb, "%s", ci0->imdb);
      idxCopia = *ci0; idxTemCopia = 1;
    } }
  // Nota do Trakt, comentarios e relacionados. Pedido na ABERTURA e nao no
  // desenho: as abas so aparecem depois que o dado chega, e pedir no desenho
  // faria a barra de abas surgir com o titulo ja na tela.
  { const CatItem *ci = cat_item(idx);
    if (ci && ci->imdb[0]) extras_pedir(ci->imdb, ehSerie(), ci->tmdb); }
  // A aba marcada tem de ser a da temporada que os episodios trazem. Comecando
  // sempre em 0, uma serie cujo primeiro episodio carregado e da 4 abria com
  // "Temporada 1" aceso — o rotulo desmentia a lista logo abaixo.
  temporada = 0;
  { const CatEp *e0 = cat_episodio(idx, 0);
    const CatItem *ci0 = cat_item(idx);
    if (e0 && ci0) {
      int k;
      for (k = 0; k < ci0->nTemporadas; k++)
        if (ci0->temporadas[k] == e0->temporada) { temporada = k; break; }
    } }
  tempPend = temporada; tempDesde = 0;
  epAncora = 0;
  int cols[N_SECOES]; for (int i = 0; i < N_SECOES; i++) cols[i] = secaoColunas(i);
  focus_iniciar(&foco, N_SECOES, cols);
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

  if (foco.fileira == SEC_EPISODIOS) ep = cat_episodio(idx, foco.coluna);
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
  ep = cat_episodio(idx, 0);
  if (!ep) return 0;
  if (temp) *temp = ep->temporada;
  if (epis) *epis = ep->episodio;
  return 1;
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
    case SEC_DETALHES:     return nLinhasDetalhe() * NV_DETF_DET_LINHA;
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
      int q = cat_n_episodios(idx);
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
    // A tabela e UMA coluna focavel, nao uma por linha: o D-pad desce ate ela,
    // ela rola para a tela e pronto. Zero colunas faria focus_mover PULA-LA
    // (focus.c:25) e a secao viraria inalcancavel — logo, tambem irrolavel.
    case SEC_DETALHES:
      if (ehSerie()) return 0;
      return nLinhasDetalhe() > 0 ? 1 : 0;
  }
  return 0;
}

// O botao primario e UM SO, e ele TROCA DE ROTULO conforme o estado:
// "Reproduzir" quando nunca foi aberto, "Retomar TxEy" quando ha progresso.
//
// Havia um segundo botao ("Reproduzir desde o inicio") que aparecia junto da
// linha de retomada. Saiu por decisao do dono: "quando ja tiver comecado nao use
// outro botao para resumir, use o mesmo botao de reproduzir, so troque ele". E o
// que a referencia mostra tambem — primario + TRES circulares (+, ja assisti,
// trailer), sem segundo botao de texto.
// QUANTOS CIRCULARES, e a resposta depende do tipo. MEDIDO nas duas capturas
// do aparelho: o FILME ("Ma") tem tres — mais, olho de "ja assisti" e trailer —
// e a SERIE ("Lioness") tem DOIS, sem o olho. Faz sentido e nao e descuido da
// referencia: "assistido" numa serie e por episodio, e a lista de episodios
// logo abaixo ja marca isso um a um; um olho no hero teria de significar "a
// serie inteira", que nao e coisa que o Trakt guarde por titulo.
//
// Este arquivo desenhava TRES nos dois casos.
static int nBotoes(void) { return ehSerie() ? 3 : 4; }

// Que ACAO esta na posicao `n` da linha. As acoes tem numeros fixos (0
// primario, 1 lista, 2 assistido, 3 fontes) porque detail_evento decide por
// eles; o que muda com o tipo e quais posicoes existem. Sem esta traducao, na
// serie o segundo circular (que e o de fontes) dispararia "marcar assistido".
enum { ACAO_PRIMARIO = 0, ACAO_LISTA = 1, ACAO_ASSISTIDO = 2, ACAO_FONTES = 3 };
static int acaoEm(int n) {
  if (n >= 2 && ehSerie()) return n + 1;   // serie pula o olho
  return n;
}

void detail_evento(const SDL_Event *e) {
  if (saindo) return;

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
    if (e->key.keysym.sym == SDLK_RIGHT && ratTemp + 1 < nt) { ratTemp++; return; }
    if (e->key.keysym.sym == SDLK_LEFT  && ratTemp > 0)      { ratTemp--; return; }
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
          int alvo = cat_indice_por_imdb(id);
          if (alvo >= 0) pedAbrir = alvo;
          else if (id[0]) desc_pedir_titulo(id);
        }
        return; }
      default: break;
    }
    // CIMA no primeiro item e BAIXO no ultimo caem no comportamento normal e
    // saem da lista — senao o foco fica preso nela.
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
      } else if (acao == ACAO_LISTA) {
        pedMarcar = 1;
      } else if (acao == ACAO_ASSISTIDO) {
        pedAssistido = 1;
      } else {
        pedFontes = 1;
      }
    } else if (foco.fileira == SEC_RELACIONADOS) {
      // FILME: "Mais como este" e secao propria. Mesmo destino do caminho de
      // serie — abre do catalogo quando ja temos meta, senao pede e o roteador
      // termina quando chegar.
      const char *id = extras_relacionado_imdb(foco.coluna);
      int alvo = id[0] ? cat_indice_por_imdb(id) : -1;
      if (alvo >= 0) pedAbrir = alvo;
      else if (id[0]) desc_pedir_titulo(id);
    } else if (foco.fileira == SEC_TEMPORADAS) {
      // Trocar de aba BUSCA a temporada. Antes so mudava o realce e a lista
      // continuava a mesma, o que fazia a aba parecer quebrada.
      temporada = foco.coluna;
      tempPend = temporada; tempDesde = 0;
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
    } else if (foco.fileira == SEC_EPISODIOS) {
      // PRESSAO LONGA ABRE O MENU DE VISTO; o toque curto continua abrindo as
      // fontes, que e o que este card sempre fez.
      //
      // "marcar este / ate aqui / a temporada inteira" existia desde a 1.0.25 e
      // era INALCANCAVEL daqui: o menu so vivia dentro da folha de episodios, e
      // a folha so abre de dentro do player. Quem estava na pagina de detalhe —
      // que e onde qualquer um iria procurar — segurava o card e via as fontes.
      const CatEp *ep = cat_episodio(idx, foco.coluna);
      if (dur >= NV_HOLD_MS && ep)
        episodios_menu_visto(idx, ep->temporada, ep->episodio, ep->nome);
      else
        // No web e `openEpisodeStreams`. Aqui a folha de fontes ainda e a do
        // titulo: `stream_folha_abrir()` nao recebe episodio. Melhor abrir a
        // folha que existe do que nao responder ao OK.
        pedFontes = 1;
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
      for (int r = 0; r < N_SECOES; r++)
        if (secaoColunas(r) > 0) { foco.fileira = r; foco.coluna = 0; nivel = 1; break; }
    }
    else if (k == SDLK_RIGHT) { if (botao < nBotoes() - 1) botao++; }
    else if (k == SDLK_LEFT)  { if (botao > 0) botao--; }
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
  else if (k == SDLK_LEFT)  focus_mover(&foco, -1, 0);
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
      return (c < nPilulasCom()) ? larguraPilulaCom(COM_ROT[c]) : COM_CARD_W;
    // A tabela e um bloco so, da largura da divisoria. Cair no `default` daria
    // a ela a largura de um avatar de elenco, e o culling horizontal cortaria
    // a tabela fora da tela.
    case SEC_DETALHES:    return NV_DETF_DET_W;
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
    if (r == SEC_COMENTARIOS) {
      // As pilulas somam largura + vao; os CARTOES recomecam em NV_DETP_X
      // porque ficam numa LINHA de baixo. xItem deixa de ser monotonico nesta
      // fileira, e nao ha problema: a rolagem horizontal so consulta a coluna
      // FOCADA, nunca a sequencia inteira.
      int np = nPilulasCom();
      if (c <= np) x += larguraPilulaCom(COM_ROT[k]) + COM_PILL_GAP;
      else if (k >= np) x = NV_DETP_X + (float)(c - np) * (COM_CARD_W + COM_CARD_GAP);
      continue;
    }
    if (r == SEC_DETALHES)  { continue; }   // coluna unica: sempre em NV_DETP_X
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
  // A coluna corrente pode ter ficado fora da faixa (a lista encolheu ao trocar
  // de temporada). Puxar para dentro evita desenhar foco em item inexistente.
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
  if (!aberto) return;
  revalidarIdx();
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
  if (nivel >= 1 && foco.fileira == SEC_TEMPORADAS) {
    if (foco.coluna != tempPend) { tempPend = foco.coluna; tempDesde = agora; }
    else if (tempPend != temporada && tempDesde &&
             agora - tempDesde >= NV_HERO_REPOUSO_MS) {
      temporada = tempPend;
      irParaTemporada(temporada, 0);
      tempDesde = 0;
    }
  } else {
    tempPend = temporada;
    tempDesde = 0;
    // Andar pelos episodios move a ancora junto: voltando para as abas, a
    // fileira nao pula de volta para o episodio de onde a aba a deixou.
    if (foco.fileira == SEC_EPISODIOS) epAncora = foco.coluna;
  }

  // SELETOR DE COMENTARIOS, pela mesma regra: mover o foco ja troca a fonte.
  // Sem repouso — sao duas pilulas, e a da serie ja esta em memoria; so a do
  // episodio custa uma viagem, e ela e disparada uma vez por episodio.
  if (nivel >= 1 && foco.fileira == SEC_COMENTARIOS && ehSerie()) {
    if (foco.coluna != comentEp) comentEp = foco.coluna;
    if (comentEp) {
      const CatItem *ci = cat_item(idx);
      int t = 0, ep = 0;
      if (ci && detail_ep_foco(&t, &ep) && t > 0 && ep > 0)
        extras_pedir_comentarios_ep(ci->imdb, t, ep);
    }
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
  if (saindo && t < 0.02f) { aberto = 0; saindo = 0; t = 0.0f; return; }

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
    float lum = focado ? 0.961f : 0.133f;   // #f5f5f5 / #222
    gfx_cor(r, NV_RAIO_PILL, lum, lum, lum, a);
    // Os tres glifos do web: biblioteca (+), assistido (olho) e trailer
    // (placa do YouTube). Sao SVG la e nao existem na familia embarcada, entao
    // vem do shader — ver GFX_OLHO e GFX_FONTES. Antes eram "+" e dois "...",
    // que nao diziam o que os botoes faziam.
    float ic = focado ? 0.067f : 1.0f;      // #111 com foco, branco sem
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
      // O botao MOSTRA O ESTADO: com o titulo ja na watchlist o "+" some e
      // entra o olho aberto — nao adianta convidar a adicionar o que ja esta la.
      // O estado vem de ci->naLista, que a descoberta preenche com a lista de
      // verdade do Trakt.
      const CatItem *ci = cat_item(idx);
      gfx_icone(ig, (ci && ci->naLista) ? "visto" : "mais", ic, ic, ic, a);
    } else if (icone == 2) {
      // ASSISTIDO: olho aberto quando ja viu, olho riscado quando nao. Antes o
      // icone era sempre o mesmo e nao dizia estado nenhum — era so um enfeite
      // que o dono nao conseguia ler ("avisar o que foi visto").
      gfx_icone(ig, progressoDe(idx) >= 90 ? "visto" : "naovisto", ic, ic, ic, a);
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
  gfx_cor(r, NV_RAIO_PILL, 1, 1, 1, a);

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
    TxtLinha l = txt_linha(TXT_DET_BOTAO, rot, 0, 0, 0, 255);
    float grupo = iw + NV_DETW2_BTN_GAPI * s + l.w;
    float x = r.x + (r.w - grupo) * 0.5f;
    GfxRect tri = { x, r.y + (r.h - ih) * 0.5f, iw, ih };
    gfx_rect(tri, 0, GFX_PLAY, 0, 0, 0, 0.0f, 0, 0, 0, a);
    txt_desenhar_alpha(l, x + iw + NV_DETW2_BTN_GAPI * s,
                       r.y + (r.h - l.h) * 0.5f, a); }
}

// Botao secundario: 345x96, raio 64, fundo #222 e texto branco; focado, fundo
// #f5f5f5 e texto #111, com o mesmo anel de 4px. Nao tem icone — no web e so o
// rotulo, e por isso a largura sai de `texto + 2 x 34` e nao da conta do
// primario.
static void desenhaSecundario(GfxRect r, const char *rot, int focado, float a) {
  if (focado) {
    GfxRect anel = { r.x - NV_DETW_ANEL, r.y - NV_DETW_ANEL,
                     r.w + NV_DETW_ANEL * 2, r.h + NV_DETW_ANEL * 2 };
    gfx_cor(anel, NV_RAIO_PILL, 1, 1, 1, a);
  }
  float lum = focado ? 0.961f : 0.133f;
  gfx_cor(r, NV_RAIO_PILL, lum, lum, lum, a);
  int cor = focado ? 17 : 255;
  TxtLinha l = txt_linha(TXT_DET_BOTAO, rot, cor, cor, cor, 255);
  txt_desenhar_alpha(l, r.x + (r.w - l.w) * 0.5f, r.y + (r.h - l.h) * 0.5f, a);
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
  char txt[8];
  snprintf(txt, sizeof txt, "%d,%d", nota / 10, nota % 10);
  TxtLinha l = txt_linha(TXT_DET_SIN, txt, 179, 179, 179, 255);
  GfxRect marca = { x, yCentro - NV_DETW2_IMDB_H * 0.5f,
                    NV_DETW2_IMDB_W, NV_DETW2_IMDB_H };
  gfx_cor(marca, NV_DETW2_IMDB_R / NV_DETW2_IMDB_H,
          0.965f, 0.780f, 0.0f, a);                       // #f6c700
  TxtLinha lm = txt_linha(TXT_MINI, "IMDb", 10, 10, 10, 255);
  txt_peso(lm, marca.x + (marca.w - lm.w) * 0.5f,
           marca.y + (marca.h - lm.h) * 0.5f, a, 0.8f);
  txt_desenhar_alpha(l, x + NV_DETW2_IMDB_W + NV_DETW2_IMDB_GAP,
                     yCentro - l.h * 0.5f, a);
  return NV_DETW2_IMDB_W + NV_DETW2_IMDB_GAP + l.w;
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
  // A linha de retomada explica o BOTAO, entao fica colada nele — logo acima.
  float yRetom = yAcoes - NV_DETW_GAP_RETOM - NV_DETW_RETOM_H;

  // Sobe alguns pixels enquanto entra: continua o movimento da arte em vez de
  // aparecer pronto no lugar. `desloc` e a rolagem do documento.
  float sobe = (1.0f - a) * 26.0f + desloc;
  yMeta2 += sobe; yMeta1 += sobe; ySin += sobe; ySup += sobe;
  yRetom += sobe; yAcoes += sobe;

  // --- logo -----------------------------------------------------------------
  const char *arqLogo = logoDe(idx);
  // O logo e desenhado com 261 de largura mas a arte de origem costuma vir bem
  // maior; o teto de 960 ja bastaria, mas quando a mesma arte tambem serve ao
  // hero o item e promovido — por isso passa pelo mesmo caminho.
  GLuint texLogo = arqLogo ? tex_obter(arqLogo) : 0;
  if (texLogo) {
    float asp = tex_aspecto(arqLogo);
    if (asp <= 0.0f) asp = 2.5f;
    float h = NV_DETW_LOGO_H, w = h * asp;
    if (w > NV_DETW_LOGO_MAXW) { w = NV_DETW_LOGO_MAXW; h = w / asp; }
      // O logo assenta acima do que vier primeiro: a linha de retomada quando ha
    // progresso, senao a propria linha de acoes.
    float baseLogo = (temRetom ? yRetom : yAcoes) - NV_DETW_LOGO_GAP;
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
      gfx_rect(r, texLogo, m, 0, 0, 0, 0.0f, 1, 1, 1, a); }
  } else {
    // Sem logo, o NOME. A altura da caixa continua sendo a do logo, para que a
    // linha de botoes nao pule entre um titulo com logo e outro sem.
    const char *nome = tituloDe(idx);
    if (nome) {
      TxtLinha t2 = txt_linha_corta(TXT_TITULO1, nome, 255, 255, 255, 255,
                                    NV_DETW_LOGO_MAXW);
      // Mesma ancora do logo: acima da retomada quando ha, senao das acoes. A
      // altura da CAIXA continua sendo a do logo, para que a linha de acoes nao
      // pule entre um titulo com logo e outro sem.
      float baseLogo = (temRetom ? yRetom : yAcoes) - NV_DETW_LOGO_GAP;
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
    for (; nb < n; nb++) {
      GfxRect rc = { bx, cyBtn - NV_DETW2_CIRC * 0.5f,
                     NV_DETW2_CIRC, NV_DETW2_CIRC };
      desenhaBotao(rc, NULL, acaoEm(nb), nivel == 0 && botao == nb, a);
      bx += NV_DETW2_CIRC + NV_DETW2_BTN_GAP;
    } }

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
  { float base = sel ? 0.21f : 0.133f;
    float lum  = base + (0.961f - base) * f;   // -> #F5F5F5 no foco
    gfx_cor(r, raio, lum, lum, lum, a); }
  if (sel && f < 0.99f)
    gfx_rect(r, 0, GFX_ANEL, 0, 1.5f / r.h, 0, raio,
             0.76f, 0.77f, 0.79f, 0.5f * (1 - f) * a);
  // Texto: cinza quando so existe, branco quando escolhido, ESCURO quando
  // focado. Interpolado por `f` para acompanhar a mola em vez de estalar.
  { float claro = sel ? 255.0f : 179.0f;
    float v = claro + (17.0f - claro) * f;     // -> #111 no foco
    int cor = (int)(v + 0.5f);
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
static void veuEpisodio(GfxRect th, float a) {
  float raio = NV_DETP_EP_RAIO / (th.w < th.h ? th.w : th.h);
  if (raio > 0.5f) raio = 0.5f;
  gfx_rect(th, 0, GFX_VEU_CARD, 0, 0, 0, raio, 0, 0, 0, a);
}

// Card de episodio: 640x422, com a miniatura de 640x414 e TODO o texto dentro
// dela, sobre o degrade. E a diferenca estrutural com o que estava aqui antes
// (miniatura em cima, texto embaixo, que e o app da Apple TV).
static void desenhaEpisodio(GfxRect r, int c, float f, float a, Uint32 agora) {
  (void)agora;
  const CatEp *ep = cat_episodio(idx, c);
  GfxRect th = { r.x, r.y, r.w, NV_DETP_EP_THUMB_H };
  float raioTh = NV_DETP_EP_RAIO / NV_DETP_EP_THUMB_H;

  // Anel de foco: no web e um box-shadow na MINIATURA, nao no card, e nao ha
  // escala nenhuma (`transform: none`).
  if (f > 0.01f) {
    GfxRect anel = { th.x - NV_DETP_ANEL, th.y - NV_DETP_ANEL,
                     th.w + NV_DETP_ANEL * 2, th.h + NV_DETP_ANEL * 2 };
    gfx_cor(anel, raioTh, 1, 1, 1, f * a);
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
    // O check e feito de dois tracos; sem rotacao no gfx, dois retangulos finos
    // em degraus dao a mesma leitura no tamanho de um selo.
    { float cx2 = selo.x + d * 0.5f, cy2 = selo.y + d * 0.5f;
      int k;
      for (k = 0; k < 4; k++)
        gfx_cor((GfxRect){ cx2 - 9.0f + k * 2.0f, cy2 - 1.0f + k * 2.0f, 3, 3 },
                0.4f, 0.05f, 0.05f, 0.05f, a);
      for (k = 0; k < 6; k++)
        gfx_cor((GfxRect){ cx2 - 1.0f + k * 2.0f, cy2 + 5.0f - k * 2.0f, 3, 3 },
                0.4f, 0.05f, 0.05f, 0.05f, a); }
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
    snprintf(cab, sizeof cab, i18n("EPISÓDIO %d"), epNum);
    TxtLinha l = txt_linha(TXT_CAPTION2, cab, 255, 255, 255, 255);
    float w = l.w + NV_DETP_EP_SELO_PADX * 2;
    GfxRect s = { tx, r.y + NV_DETP_EP_SELO_Y, w, NV_DETP_EP_SELO_H };
    gfx_cor(s, 12.0f / NV_DETP_EP_SELO_H, 0.05f, 0.05f, 0.06f, 0.78f * a);
    txt_peso(l, s.x + NV_DETP_EP_SELO_PADX,
             s.y + (NV_DETP_EP_SELO_H - l.h) * 0.5f, a, 1.0f); }

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
    if (nota > 0) {
      char valor[32]; snprintf(valor, sizeof valor, "Trakt %d.%d", nota / 10, nota % 10);
      TxtLinha ln = txt_linha(TXT_CAPTION2, valor, 229, 231, 236, 255);
      GfxRect selo = { x, y - 3, ln.w + 16, NV_DETP_EP_ICONE + 6 };
      gfx_cor(selo, 0.18f, 0.15f, 0.15f, 0.17f, 0.94f * a);
      txt_desenhar_alpha(ln, x + 8, y, a);
      x += selo.w + 16;
    }
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
// NAO E FOCAVEL, e isso e decisao, nao pendencia: este app nao tem reprodutor
// de YouTube. A mesma regra ja tirou o botao de trailer do hero (detail.c) e o
// glifo do YouTube do terceiro circular (gfx.c) — um controle que promete o que
// nao cumpre e pior que a ausencia dele. O card entra na composicao para a
// pagina nao mentir sobre o que o filme tem; abrir, nao abre.
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
    gfx_cor(anel, 0.5f, 1, 1, 1, f * a);
  }
  GLuint t2 = foto ? tex_obter_larg(foto, NV_DETP_EL_AVATAR) : 0;
  if (t2) {
    gfx_tex_aspect_atual = tex_aspecto(foto);
    gfx_rect(av, t2, GFX_CARD, 0, 0, 0, 0.5f, 0, 0, 0, a);
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
static float raioCartaz(float w, float h) {
  float menor = w < h ? w : h;
  if (menor <= 0.0f) return NV_RAIO_CARD;
  return ajustes_raio_poster_px() / menor;
}

static void moldura(GfxRect r, float raio, float a) {
  float menor = r.w < r.h ? r.w : r.h;
  raio = menor > 0.0f ? raio / menor : 0.0f;
  // CINZA NEUTRO, o mesmo #2D2D2D das pilulas de temporada e do resto dos
  // componentes. O azul-escuro que estava aqui (#12171F) era o unico tom da
  // familia nesta tela: o cartao de comentario lia como peca de outro app.
  gfx_cor(r, raio, 0.176f, 0.176f, 0.176f, 0.94f * a);
  // A BORDA SEGUE O CANTO. Eram QUATRO RETANGULOS RETOS de 1 px, um por lado —
  // eles cruzavam por fora do arredondamento e desenhavam bico nos quatro
  // cantos, que e o que o dono viu como "borda nao arredondada". GFX_ANEL usa o
  // mesmo SDF do preenchimento, entao o traco acompanha o raio.
  gfx_rect(r, 0, GFX_ANEL, 0, 1.0f / (menor > 0.0f ? menor : 1.0f), 0, raio,
           1, 1, 1, 0.14f * a);
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
  int nt = extras_n_temporadas(), t, i, ne;
  if (ratTemp >= nt) ratTemp = 0;
  for (t = 0; t < nt; t++) {
    char rot[8];
    float bx = x + t * (58.0f + RAT_TEMP_GAP);
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
    if (i == EX_IMDB && !v) v = notaDe(idx);
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
      float w = larguraPilulaCom(COM_ROT[k]);
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
      { float cresce = sel ? (1.0f + 0.07f * f) : 1.0f;
        float dw = r.w * (cresce - 1.0f), dh = r.h * (cresce - 1.0f);
        GfxRect rc = { r.x - dw * 0.5f, r.y - dh * 0.5f, r.w + dw, r.h + dh };
        float lum = sel ? 0.961f : 0.176f;      // #F5F5F5 / #2D2D2D
        gfx_cor(rc, NV_RAIO_PILL, lum, lum, lum, a);
        r = rc; }
      if (f > 0.01f && !sel) {
        GfxRect anel = { r.x - NV_ANEL_FOCO, r.y - NV_ANEL_FOCO,
                         r.w + NV_ANEL_FOCO * 2, r.h + NV_ANEL_FOCO * 2 };
        gfx_rect(anel, 0, GFX_ANEL, 0, NV_ANEL_FOCO / anel.h, 0, NV_RAIO_PILL,
                 1, 1, 1, f * a);
      }
      { int cor = sel ? 17 : 255;
        TxtLinha l = txt_linha(TXT_PLR_CORPO, COM_ROT[k], cor, cor, cor, 255);
        txt_peso(l, r.x + (r.w - l.w) * 0.5f, r.y + (r.h - l.h) * 0.5f, a, 0.5f); }
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
    moldura(card, 20.0f, a);
    if (foc) {
      GfxRect anel = { card.x - NV_ANEL_FOCO, card.y - NV_ANEL_FOCO,
                       card.w + NV_ANEL_FOCO * 2, card.h + NV_ANEL_FOCO * 2 };
      // Raio EXTERNO = raio do cartao + espessura do anel, senao o canto do
      // anel fica mais quadrado que o do cartao e as duas curvas descasam.
      gfx_rect(anel, 0, GFX_ANEL, 0, NV_ANEL_FOCO / anel.h, 0,
               (20.0f + NV_ANEL_FOCO) / anel.h, 1, 1, 1, a);
    }

    { TxtLinha lu = txt_linha_corta(TXT_ROW_TITULO,
                                    daSerie ? extras_comentario_usuario(i)
                                            : extras_comentario_ep_usuario(i),
                                    245, 248, 255, 255, larg);
      txt_desenhar_alpha(lu, px, y + COM_PAD, a); }

    // O texto para ANTES do rodape: sem o teto de linhas ele passava por cima
    // das curtidas. 5 linhas e o que cabe entre o nome e o rodape com o leading
    // de 34.
    txt_bloco(TXT_DET_META2, daSerie ? extras_comentario_texto(i)
                                     : extras_comentario_ep_texto(i),
              200, 205, 214,
              px, y + COM_PAD + 46.0f, larg, 34.0f, a * 0.95f, 5);

    { int nota = daSerie ? extras_comentario_nota(i)
                         : extras_comentario_ep_nota(i);
      int cur  = daSerie ? extras_comentario_curtidas(i)
                         : extras_comentario_ep_curtidas(i);
      if (nota > 0)
        snprintf(rodape, sizeof rodape, i18n("%d/10   %d curtidas"), nota, cur);
      else
        snprintf(rodape, sizeof rodape, "%d curtidas", cur);
      { TxtLinha lr = txt_linha(TXT_CAPTION2, rodape, 150, 154, 163, 255);
        txt_desenhar_alpha(lr, px, y + COM_CARD_H - COM_PAD - lr.h, a * 0.9f); } }
  }
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
    case SEC_COMENTARIOS: y = conteudoSec[r]; break;
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
  heroWeb(a2, -scrollY + (1.0f - a2) * NV_TELA_H * 0.05f);


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
int detail_pediu_abrir(void) { int v = pedAbrir; pedAbrir = -1; return v; }
int detail_pediu_assistido(void) { int v = pedAssistido; pedAssistido = 0; return v; }
int detail_pediu_marcar(void)     { int v = pedMarcar;     pedMarcar = 0;     return v; }
int detail_pediu_fontes(void)     { int v = pedFontes;     pedFontes = 0;     return v; }
// "Reproduzir desde o inicio" ainda cai no mesmo caminho do primario: o
// roteador so sabe abrir o player no ponto salvo. Consumir o pedido aqui evita
// que ele fique pendurado.
int detail_pediu_do_inicio(void)  { int v = pedDoInicio;   pedDoInicio = 0;   return v; }
