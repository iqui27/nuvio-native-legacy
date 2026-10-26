// Tela AGENDA — a LINHA DO TEMPO das series acompanhadas.
//
// ---------------------------------------------------------------------------
// O ACABAMENTO DO MOCKUP (Glass UI "ilha", tela 8; out/2026)
//
// O dono pos a captura ao lado do mockup aprovado: "o mockup ta bem mais
// polido que a build, nao podemos errar". As medidas agora sao as dele (ver
// "AS MEDIDAS DO MOCKUP" abaixo), e o que mudou de estrutura foi:
//   - a faixa de origem "HOJE" e os cabecalhos de mes sairam: hoje e o ponto
//     no fio (acento com halo) e o rotulo/numeral no acento; data de outro mes
//     se escreve "4/11" no proprio numeral;
//   - o fio virou 2 px de branco a 8% sem nos por linha — so o ponto de hoje;
//   - a data fica no TOPO da linha (cabecalho dela), nao centrada no cartao;
//   - o cartao vai ate a margem direita, cresce em foco (cartaz 70x100 ->
//     90x130, titulo 27,5 -> 32, sinopse por baixo) e o foco e so superficie;
//   - o sino circular virou o CHIP "Lembrete ativo" (acento a 22%, texto no
//     acento) / "Lembrar-me" (vidro), em toda linha que pode ter lembrete;
//   - sairam a data por extenso do canto e a frase "OK abre as opcoes...", que
//     o mockup nao tem. Lista/Mes alterna entre a timeline e a grade mensal;
//     as duas vistas usam as mesmas datas e series ja guardadas pela Agenda.
// As notas historicas abaixo continuam valendo no que diz respeito a DADOS
// (de onde vem cada texto, o que nunca se inventa); o desenho e o daqui.
//
// ---------------------------------------------------------------------------
// O PASSO "MENOS TINTA, MAIS AR" (set/2026)
//
// A estrutura nao mudou — estacao, eixo, conteudo — porque ela ja era a
// resposta certa para a pergunta da tela. O que mudou foi o PESO de cada
// peca, na direcao do pedido "mais elegante, mais minimalista":
//   - o eixo emagreceu (2px, alfa 0,22) e os nos encolheram (13/21): a linha
//     do tempo segura a tela sem gritar;
//   - os rotulos pequenos (dia da semana, meses, "sem data") viraram caps
//     espacadas — a voz que so o "HOJE" tinha, estendida a tela inteira;
//   - o canto superior direito, vazio desde sempre, ganhou a data de hoje por
//     extenso: a unica peca de calendario que a estacao nao da;
//   - a laje de foco ganhou canto de 26px e mais respiro em volta do cartaz;
//   - as tres linhas de texto da linha respiram mais (7/6 em vez de 5/4).
// Nenhuma string nova: a data do cabecalho e composta de pecas que ja existem
// traduzidas (agenda_semana_nome, desc_data_extenso).
//
// ---------------------------------------------------------------------------
// POR QUE UM EIXO, e nao a lista de pilulas que estava aqui
//
// A pergunta da tela e temporal ("o que sai, e quando"). A versao anterior ja
// acertava em nao ser uma grade de cartazes, mas respondia com uma PILHA DE
// PILULAS de altura igual: sete retangulos escuros, sete datas soltas a
// esquerda, e nada dizendo que aquelas datas estao numa mesma reta de tempo. O
// dono viu a captura e pediu "a agenda ser mais elegante, como uma timeline,
// calendario, minimalista, mas com muita informacao util".
//
// Entao a tela virou o que ela ja queria ser:
//
//   HOJE  ──────────────────────────────  setembro 2026
//    qui
//     18  ●  [cartaz]  Fundacao            Ultimo episodio em 12 de     (~)
//  hoje   │            T3E9 · The Last...  setembro de 2026          Lembrete
//         │            Final da temp...    Gaal e Salvor chegam a       ativo
//         │                                Trantor no dia em que o
//     19  ●  [cartaz]  The Last of Us      Imperio anuncia o fim da…
//  amanha │            T2E4 · Day One
//
// Tres colunas fixas e um eixo entre elas: a ESTACAO (dia da semana, numeral do
// dia, quanto falta), o EIXO com um no por serie, e o CONTEUDO. O numeral e o
// no ficam no MESMO y — e o que faz a coluna de datas ler como calendario em
// vez de como rotulo.
//
// MINIMALISTA E SOBRE TINTA, NAO SOBRE INFORMACAO. A linha nao focada mostra
// tres textos (titulo, episodio, apoio) e nenhum retangulo de fundo: o que
// desenha a estrutura e o eixo de 3px, nao sete caixas. A linha FOCADA e a
// unica que ganha superficie.
//
// ---------------------------------------------------------------------------
// A LINHA FOCADA NAO CRESCE MAIS, ela PREENCHE o que ja tinha vazio
//
// Ate aqui a linha focada abria 86px por baixo para caber a sinopse, e o
// resultado era uma laje de 254px de altura com o terco de baixo vazio e a
// METADE DIREITA INTEIRA vazia. O dono olhou a tela e disse: "quando ta
// selecionado nao ta bonito, ta destoando, ta muito grande sem informacoes,
// podemos diminuir e colocar mais informacoes no canto direito".
//
// As duas queixas tem UMA correcao so, e e por isso que ela e esta: o conteudo
// que estava embaixo passou para a direita. A linha focada agora tem a MESMA
// altura da nao focada (177px, medidos: 153 de cartaz + 12 de respiro dos dois
// lados), a coluna da direita deixou de ser espaco morto, e a tela parou de
// empurrar tres linhas para baixo toda vez que o foco anda um passo — que era,
// a 3 m, o movimento que mais chamava atencao nesta tela.
//
// A coluna do conteudo virou DUAS sub-colunas de largura fixa nos dois estados
// (58% / 42% do que sobra depois do cartaz e da faixa do despertador):
//
//   ESQUERDA  titulo, T<n>E<n> · nome, marco · rede · duracao · temporadas
//   DIREITA   quando foi o episodio ANTERIOR, e a sinopse do proximo
//
// A largura da esquerda e a MESMA focada ou nao. Encolher a coluna so no foco
// faria o titulo passar a caber/nao caber conforme o foco, e um titulo que
// muda de reticencia ao receber foco le como defeito.
//
// O QUE FOI REJEITADO NA DIREITA, e por que:
//   - repetir rede/duracao/temporadas: ja estao na terceira linha da esquerda,
//     e o pedido foi "mais informacoes", nao "as mesmas duas vezes";
//   - repetir o marco ("Final da temporada"): ele e a razao de alguem marcar o
//     lembrete e por isso tem de aparecer TAMBEM na linha nao focada — tirar da
//     esquerda para enfeitar a direita perderia informacao onde ela conta mais;
//   - repetir a data por extenso: o numeral, o dia da semana e "em 3 dias" ja
//     estao na estacao, a 300px dali;
//   - preencher a direita com qualquer coisa quando a serie nao tem sinopse nem
//     episodio anterior: ai a coluna fica VAZIA mesmo. Ver PRODUCT.md — nao se
//     inventa metadado para tapar espaco. A linha continua com 177px, que e o
//     tamanho certo para o que ela diz.
//
// O que SOBROU e o que a esquerda nunca disse: a sinopse do proximo episodio
// (que so existia no estado focado e vivia embaixo) e a DATA DO EPISODIO
// ANTERIOR, que ja estava no cache (`dataUlt`) e so era usada quando nao havia
// proximo — e e a resposta de "eu estou em dia com esta serie?".
//
// ---------------------------------------------------------------------------
// FOCO: superficie clara PREENCHIDA com texto ESCURO, sem contorno — a mesma
// regra do menu lateral e das linhas de Ajustes. Texto claro sobre superficie
// clara ja sumiu duas vezes neste repositorio. A troca de cor do texto e um
// DEGRAU em f > 0,5 e nao uma mistura continua: txt_linha cacheia por cor, e
// interpolar geraria uma textura nova por quadro.
//
// A superficie preenchida cobre so a coluna do CONTEUDO. Preenchendo a linha
// inteira, o eixo e o numeral do dia sumiam dentro dela e a tela deixava de ser
// calendario justamente na linha em que o dono esta olhando.
//
// ---------------------------------------------------------------------------
// O QUE A LINHA DIZ, e de onde vem cada pedaco
//
// Todo texto desta tela sai do corpo /tv/<id> que agenda.c JA BAIXA. Nenhum
// pedido novo, nenhuma API nova, nada inventado:
//   titulo/cartaz    catalogo local
//   T<n>E<n> · nome  next_episode_to_air
//   marco            next_episode_to_air.episode_type ("Final da temporada")
//   rede/duracao     networks[0].name, runtime / episode_run_time
//   temporadas       number_of_seasons
//   sinopse          next_episode_to_air.overview — so na linha FOCADA
//   anterior         last_episode_to_air.air_date — so na linha FOCADA, na
//                    coluna da direita, e so quando ha data futura (sem ela a
//                    coluna da esquerda ja gasta a segunda linha com a mesma
//                    frase — ver desenhaConteudo)
//
// O dono pediu tambem "noticias sobre a serie" e "citacoes importantes". Nao ha
// fonte para nenhuma das duas neste app: nao existe API de noticia ligada aqui,
// e o TMDB nao tem endpoint de citacao. O que existe e o MARCO do episodio, que
// e literalmente "o que importa nesta data" dito pela propria fonte — estreia,
// final de temporada, volta da meia temporada. E o mais perto que da para
// chegar sem inventar, e inventar e o que PRODUCT.md proibe.
//
// SEM DATA nao vira data. As series encerradas, canceladas ou sem anuncio caem
// depois de um separador, com o eixo TRACEJADO e a situacao no lugar do
// numeral. "A definir" escrito onde deveria haver um dia e metadado inventado.
#include "menu.h"
#include "agendaui.h"
#include "agenda.h"
#include "gfx.h"
#include "text.h"
#include "sinopse.h"
#include "tex_cache.h"
#include "anim.h"
#include "layout.h"
#include "ajustes.h"
#include "idioma.h"
#include "noticias.h"
#include "idiomacod.h"
#include "catalogo.h"
#include "badges.h"
#include "descoberta.h"
#include "noticia.h"
#include "leitura.h"
#include "qr.h"
#include "visto.h"
#include "vistoep.h"
#include "textogate.h"
#include "layout.h"
#include "escala.h"
#include "ponteiro.h"
#include "rolagemtoque.h"
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// TAMANHO DA INTERFACE, COM PISO DE 120% (pedido do dono: a Agenda nasce a
// 120%). A escala efetiva e escala_min(1,2) (escala.h): 120% no padrao,
// acompanha o ajuste acima disso. A tela virtual da Agenda e sempre 1920/s x
// 1080/s, na medida, no evento e no desenho, e e so o desenho publico que liga
// a escala (AG_ESC_INI/FIM), como escala.h manda.
#define AG_ESCALA_MIN 1.2f
// Escala forcada so enquanto o painel Social desenha a lista da Agenda com as
// MESMAS pecas desta tela (agendaui_painel_*): ali a unidade ja e o pixel da
// tela virtual do painel, e P() tem de ser a identidade.
static float escForcada;
static float escModal;
static float agEscala(void) { return escForcada > 0.0f ? escForcada : escModal > 0.0f ? escModal : escala_min(AG_ESCALA_MIN); }
static int agColunaUnica(void) {
#ifdef NV_TOUCH_UI
  float w = NV_LAYOUT_REAL_W, h = NV_LAYOUT_REAL_H;
  return layout_modo_mobile() && escForcada <= 0.0f && w > 0.0f && h > 0.0f &&
         fmaxf(w, h) / fminf(w, h) >= 1.7f;
#else
  return 0;
#endif
}
#define AG_ESC_INI() ESCALA_MIN_INI(AG_ESCALA_MIN)
#define AG_ESC_FIM() ESCALA_MIN_FIM()
#undef NV_TELA_W
#undef NV_TELA_H
#define NV_TELA_W (NV_LAYOUT_REAL_W / agEscala())
#define NV_TELA_H (NV_LAYOUT_REAL_H / agEscala())

// --- AS MEDIDAS DO MOCKUP APROVADO (Glass UI "ilha", tela 8; out/2026) -----
//
// Dono, olhando a captura ao lado do mockup: "o mockup ta bem mais polido que a
// build, nao podemos errar". Os numeros daqui sao os do glass-ilha.html em
// pixel de 1920x1080, e nao uma releitura: pagina com 64 de topo, coluna de
// datas de 96 alinhada a direita, 28 de vao ate um fio de 2 px e mais 28 ate os
// cartoes; cartao de canto 26 com recuo 16/22; cartaz de 70x100 em repouso e
// 90x130 em foco; 14 entre cartoes. O cartao vai ate a margem direita — o
// mockup aprovado (02/10) e posterior ao pedido de cartao estreito (21/09).
#define AG_TOPO         64.0f
// A MARGEM do MES: 80 virtuais = os 96 reais da C1 (ver c1()), para a grade do
// mes comecar na mesma coluna do titulo. A rail fixa (144 reais) entra
// dividida pela escala: a tela virtual e menor.
#define AG_MARGEM 80.0f
static float agX(void) { return ajustes_rail_largura_fixa() / agEscala() +
                              (agColunaUnica() ? 48.0f / agEscala() : AG_MARGEM); }
static float agFim(void) { return NV_TELA_W - (agColunaUnica() ? 48.0f / agEscala() : AG_MARGEM); }
#define AG_LISTA_Y     218.0f   // reserva uma faixa abaixo do subtitulo para os controles
#define AG_EST_W        96.0f   // coluna de datas
#define AG_EIXO_GAP     28.0f
#define AG_EIXO_W        2.0f
#define AG_CARD_RAIO    26.0f
#define AG_CARD_PX      22.0f
#define AG_CARD_PY      16.0f
// A ALTURA DA LINHA sai do cartaz: 16 + 100 + 16 em repouso. Em foco o mockup
// pede min-height 150 + 2 x 16 = 182, que e onde cabem a sinopse de uma linha e
// o cartaz de 130; sinopse de duas linhas ou a dica da noticia passam disso, e
// ai a altura e a medida (alturaFoco).
#define AG_LINHA_H     132.0f
#define AG_FOCO_H      182.0f
#define AG_LINHA_GAP    14.0f
#define AG_CZ_W0        70.0f
#define AG_CZ_H0       100.0f
#define AG_CZ_W1        90.0f
#define AG_CZ_H1       130.0f
#define AG_CZ_RAIO      14.0f
#define AG_CZ_GAP       24.0f   // cartaz -> texto, e texto -> chip
#define AG_CHIP_H       44.0f
#define AG_SIN_W       880.0f   // max-width da sinopse
#define AG_SIN_LD17     25.5f   // 17 px x line-height 1,5
#define AG_FAIXA_H      56.0f   // o separador "SEM DATA PREVISTA"
#define AG_TRACO        10.0f   // traco e vao do fio tracejado
#define AG_PONTO        14.0f   // o ponto de hoje no fio; o halo tem +6 de cada lado
// CORPOS QUE O text.c NAO TEM. O mockup usa 36/800 no numeral, 26-30/700 no
// titulo, 17/16/15 nos apoios — nenhum e estilo da tabela, e um estilo novo la
// seria conflito com quem mexe em text.c em paralelo. Entao o texto e
// rasterizado no estilo mais proximo ACIMA, com o mesmo peso, e desenhado
// reduzido (txtEsc): reduzir 5-25% com GL_LINEAR nao borra; ampliar borraria.
#define AG_CIT_MAX          2
// O passo das linhas de texto corrido dos paineis (manchete, noticia) e o teto
// do cache de quebra (AG_SIN_MAX, tambem o tamanho do buffer de agQuebra).
#define AG_SIN_LD       30.0f
#define AG_SIN_MAX         6
// O cartaz e o que faz reconhecer a serie antes de ler o nome, e a 3 m ele tem
// de ter tamanho para isso. 102x153, 2:3 como todo cartaz do app.
//
// 102 E O TETO DE GRACA, nao um arredondamento a olho. tex_obter_larg decide o
// teto de decodificacao por capDeLargura: ceil32(largura x escala x 1,25), com
// PISO DE 128. Na TV (escala 1) qualquer largura de desenho ate 102 cai no
// mesmo piso de 128 — 102 x 1,25 = 127,5, que arredonda para 128. O primeiro
// degrau real e 103: 128,75 sobe o teto para 160 e a arte junto.
//
// MEDIDO com tex_estatisticas nas capturas desta tela (o `[tex]` que
// tests/agenda_shot.c imprime por foto), e os tres numeros sao de rodadas de
// verdade, nao de conta de cabeca:
//
//   cartaz 96   captura -hoje: 5 texturas quentes, 277 KB
//   cartaz 102  captura -hoje: 5 texturas quentes, 277 KB   <- o mesmo byte
//   cartaz 103  captura -hoje: 5 texturas quentes, 386 KB   <- +109 KB (+39%)
//
// Ou seja: +6px de arte por linha custaram ZERO, e o 7o pixel custa 109 KB.
// (Na -semdata o mesmo degrau vai de 192 para 300 KB.) Um pixel de largura nao
// se ve a 3 m; 109 KB numa TV que ja anda perto do teto de textura se veem
// quando a arte da fileira seguinte nao carrega.
#define AG_CARTAZ_W    102.0f
#define AG_CARTAZ_H    153.0f
// Canto do cartaz em PIXEIS, convertido aqui. O raio do gfx_cor/gfx_rect e
// fracao da ALTURA do retangulo, e nao do menor lado: o fragmento normaliza com
// `p = (uv-0.5) * vec2(asp,1.0)`, entao a meia-extensao vertical e sempre 0,5 e
// `raio * h` e o raio em pixeis (ver FS_SDF em gfx.c e a secao 5 do DESIGN.md).
//
// O QUE ESTAVA AQUI ESTAVA ERRADO, e o comentario dizia o contrario do codigo:
// dividia por AG_CARTAZ_W "porque o raio e fracao do menor lado". Nos 96x144
// isso pedia 10/96 = 0,104 da ALTURA, ou seja 15 px de canto — 50% a mais do
// que os 10 px que o comentario afirmava ter conferido na captura. Dividindo
// pela altura o numero passa a ser o que esta escrito. 12 e nao 10 porque o
// cartaz cresceu: a proporcao canto/altura de antes (10/144) mantida em 153 da
// 10,6, e 12 e o canto do resto dos cartazes do app a este tamanho.
#define AG_CARTAZ_RAIO (12.0f / AG_CARTAZ_H)

void agendaui_cor_lembrete(int ligado, int sobreClaro,
                           float *r, float *g, float *b) {
  // O relogio deixou de ser disco colorido: a diferenca entre os estados agora
  // esta no diametro/espessura do anel e na legenda, entao o desenho pode ser
  // branco e silencioso como a referencia. No foco, a tinta ainda respeita a
  // regra global para o raro tema de realce branco.
  if (sobreClaro) {
    *r = *g = *b = ajustes_acento_tinta(NULL, NULL, NULL);
    return;
  }
  if (ligado) { *r = *g = *b = 1.0f; return; }
  *r = *g = *b = 170.0f / 255.0f;
}

void agendaui_despertador(GfxRect r, int ligado, float cr, float cg, float cb,
                          float a, Uint32 agora, Uint32 desde) {
  float s = r.w < r.h ? r.w : r.h;
  (void)ligado; (void)agora; (void)desde;
  if (s <= 0.0f || a <= 0.001f) return;
  // O relogio feito a mao parecia um simbolo de fonte. O sino Lucide mora em
  // asset rasterizado, como os demais glifos do app; sem moldura externa ele
  // respira e nao parece um botao dentro de outro botao. O glifo cresce para
  // ocupar a presenca que antes vinha do circulo, sem introduzir fill.
  { GfxRect glifo = { r.x + s * 0.04f, r.y, s * 0.92f, s * 0.92f };
    gfx_icone(glifo, "sino", cr, cg, cb, a); }
}

// A QUEBRA POR PALAVRA, num lugar so, para quem MEDE e para quem DESENHA.
// Preenche ate `max - 1` linhas inteiras e devolve quantas sairam; o que sobrou
// fica em *resto e vai por txt_linha_corta, que e quem sabe fechar com "…".
//
// Wrapping is independent of text color. Cache by font and language as well
// as text/width so focus changes reuse it and font changes invalidate it.
// A QUEBRA E LEMBRADA. agQuebra mede palavra por palavra com txt_linha, e a
// linha em foco a chama duas vezes por quadro (medir e desenhar) para a
// sinopse ou a citacao — na C9 isso era ~11 ms de CPU por quadro em
// agendaui_desenhar (des=11,5 com gfx=0,0 no nuvio-fps.txt de 21/09/2026),
// e a tela parada ficava em 30 fps. Oito entradas bastam: o foco esta numa
// linha, e o texto/largura/estilo so mudam quando ele anda.
#define AGQ_CACHE 8
typedef struct {
  unsigned hash; float larg; int estilo, max, familia, idioma, n;
  char linhas[AG_SIN_MAX][512];
  size_t restoOff;   // deslocamento de `resto` dentro de `s`
  int vivo;
} AgQuebra;
static AgQuebra agqCache[AGQ_CACHE];
static int agqProx;
static unsigned agqHash(const char *s) {
  unsigned h = 2166136261u;
  while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
  return h;
}
static int agQuebraCru(TxtEstilo estilo, const char *s, float larg, int max,
                       char linhas[][512], const char **resto, int r, int g, int b);
static int agQuebra(TxtEstilo estilo, const char *s, float larg, int max,
                    char linhas[][512], const char **resto,
                    int r, int g, int b) {
  unsigned h; int i, n;
  *resto = "";
  if (!s || !s[0] || larg <= 0.0f || max <= 0) return 0;
  if (max > AG_SIN_MAX) max = AG_SIN_MAX;
  h = agqHash(s);
  for (i = 0; i < AGQ_CACHE; i++) {
    AgQuebra *c = &agqCache[i];
    if (c->vivo && c->hash == h && c->larg == larg && c->estilo == (int)estilo &&
        c->max == max && c->familia == (int)txt_fonte_interface() &&
        c->idioma == ajustes_idioma()) {
      int k;
      for (k = 0; k < c->n; k++) memcpy(linhas[k], c->linhas[k], strlen(c->linhas[k]) + 1);
      *resto = s + c->restoOff;
      return c->n;
    }
  }
  n = agQuebraCru(estilo, s, larg, max, linhas, resto, r, g, b);
  { AgQuebra *c = &agqCache[agqProx]; int k;
    agqProx = (agqProx + 1) % AGQ_CACHE;
    c->vivo = 1; c->hash = h; c->larg = larg; c->estilo = (int)estilo; c->max = max;
    c->familia = txt_fonte_interface(); c->idioma = ajustes_idioma(); c->n = n;
    for (k = 0; k < n; k++) memcpy(c->linhas[k], linhas[k], strlen(linhas[k]) + 1);
    // `resto` pode ser o literal "" (nada sobrou): guardar o fim de `s`.
    c->restoOff = (**resto) ? (size_t)(*resto - s) : strlen(s); }
  return n;
}
static int agQuebraCru(TxtEstilo estilo, const char *s, float larg, int max,
                       char linhas[][512], const char **resto,
                       int r, int g, int b) {
  const char *p = s;
  int n = 0;
  (void)r; (void)g; (void)b;   // a medida e por txt_largura, que nao tem cor
  *resto = "";
  if (!s || !s[0] || larg <= 0.0f || max <= 0) return 0;
  while (*p && n < max - 1) {
    char *l = linhas[n];
    int espacoAntes = 1;     // o token anterior veio separado por espaco? (txt_token_tam)
    l[0] = 0;
    while (*p) {
      const char *ini = p;
      char tent[512];
      size_t nl = strlen(l), np;
      int espaco;
      p += txt_token_tam(p);
      np = (size_t)(p - ini);
      espaco = (*p == ' ' || *p == '\n');
      if (nl + np + 2 >= sizeof tent) { p = ini; break; }
      memcpy(tent, l, nl);
      if (nl && espacoAntes) tent[nl++] = ' ';
      memcpy(tent + nl, ini, np);
      tent[nl + np] = 0;
      // A MEDIDA E A MESMA QUE VAI DESENHAR. Estimar por largura media de glifo
      // erra em nome proprio e em maiuscula, e o erro aparece como uma linha
      // estourando a coluna — que e o defeito que este corte existe para evitar.
      // txt_largura e nao txt_linha: mede pela mesma fonte SEM rasterizar cada
      // prefixo (so a linha final vira textura, no desenho).
      if ((float)txt_largura(estilo, tent) > larg) {
        if (l[0]) { p = ini; break; }
        // UMA PALAVRA SO MAIS LARGA QUE A COLUNA. Era aceita inteira e a linha
        // estourava o componente: "Напоминание" (ru) mede ~140 em CAPTION2 e a
        // coluna do sino tem 120; composto alemao na citacao faz o mesmo. Agora
        // a palavra fica sozinha na linha e o desenho (agendaui_sinopse e a
        // legenda do sino, ambos por txt_linha_corta) fecha com "…" dentro da
        // largura — a linha seguinte continua com o resto da frase.
        memcpy(l, tent, nl + np + 1);
        while (*p == ' ' || *p == '\n') p++;
        break;
      }
      memcpy(l, tent, nl + np + 1);
      while (*p == ' ' || *p == '\n') p++;
      espacoAntes = espaco;
    }
    if (!l[0]) break;
    n++;
    if (!*p) break;
  }
  *resto = p;
  return n;
}

int agendaui_sinopse_linhas(TxtEstilo estilo, const char *s, float larg,
                            int maxLinhas, int r, int g, int b) {
  char linhas[AG_SIN_MAX][512];
  const char *resto;
  int n;
  if (maxLinhas > AG_SIN_MAX) maxLinhas = AG_SIN_MAX;
  n = agQuebra(estilo, s, larg, maxLinhas, linhas, &resto, r, g, b);
  return n + (resto[0] ? 1 : 0);
}

int agendaui_sinopse(TxtEstilo estilo, const char *s, float x, float y,
                     float larg, float leading, int maxLinhas,
                     int r, int g, int b, float alpha) {
  char linhas[AG_SIN_MAX][512];
  const char *resto;
  int n, i;
  if (maxLinhas > AG_SIN_MAX) maxLinhas = AG_SIN_MAX;
  n = agQuebra(estilo, s, larg, maxLinhas, linhas, &resto, r, g, b);
  for (i = 0; i < n; i++) {
    // corta e nao txt_linha: a linha cheia ja cabe (e corta devolve a mesma
    // textura), mas a linha de UMA palavra larga demais so cabe cortada.
    TxtLinha l = txt_linha_corta(estilo, linhas[i], r, g, b, 255, larg);
    txt_desenhar_alpha(l, x, y + leading * (float)i, alpha);
  }
  if (resto[0]) {
    TxtLinha l = txt_linha_corta(estilo, resto, r, g, b, 255, larg);
    txt_desenhar_alpha(l, x, y + leading * (float)n, alpha);
    n++;
  }
  return n;
}

// O relogio do quadro e o instante da ultima troca de lembrete, guardados para
// o despertador. Ficam aqui em vez de viajar por parametro porque desenhaLinha
// ja recebe cinco, e o relogio e do QUADRO, nao da linha.
static Uint32 relogio, trocaEm;

static int   foco;
static int   vistaMes, focoCabecalho;
static int   calAno, calMes, calDia, calCelula, calPainel, calEvento;
static int   versaoVista;   // agenda_versao() da ultima montagem; ver agendaui_atualizar
static float animFoco[AG_MAX];
static float scrollY;
static int ctxAberto;
#ifdef NV_TOUCH_UI
static ToqueRolagem toqueAgenda, toqueNoticia, toqueCalendario, toqueManchetes, toqueModal;
static ToqueRolagem *toqueAgAtiva;
static float toqueCalOffset, toqueManchetesOffset, toqueModalOffset;
static unsigned toqueManchetesChave;
static float toqueMancheteY(int i);
static int toqueMancheteIndice(float offset, int n);
static void toqueManchetesReiniciar(void) {
  toqueManchetesOffset = 0.0f; toqueManchetesChave = 0;
  toquerol_limpar(&toqueManchetes);
  if (toqueAgAtiva == &toqueManchetes) toqueAgAtiva = NULL;
}
static int toqueAgendaRolar(const PonteiroRolagem *e) {
  if (!agColunaUnica()) return 0;
  if (e->fase == PONT_ROL_INICIO) {
    toqueAgAtiva = NULL;
    ToqueRolagem *v[2]; int n = 0;
    if (ctxAberto == 2) v[n++] = &toqueManchetes;
    else if (ctxAberto == 3) v[n++] = &toqueNoticia;
    else if (ctxAberto == 1) v[n++] = &toqueModal;
    else if (!ctxAberto) { v[n++] = &toqueCalendario; v[n++] = &toqueAgenda; }
    for (int i = 0; i < n; i++) if (toquerol_evento(v[i], e)) { toqueAgAtiva = v[i]; return 1; }
    return 0;
  }
  if ((ctxAberto == 2 && toqueAgAtiva != &toqueManchetes) ||
      (ctxAberto == 3 && toqueAgAtiva != &toqueNoticia) ||
      (ctxAberto == 1 && toqueAgAtiva != &toqueModal) ||
      (!ctxAberto && (toqueAgAtiva == &toqueManchetes || toqueAgAtiva == &toqueNoticia || toqueAgAtiva == &toqueModal))) {
    toqueAgAtiva = NULL; return 0;
  }
  int r = toqueAgAtiva ? toquerol_evento(toqueAgAtiva, e) : 0;
  if (e->fase == PONT_ROL_FIM || e->fase == PONT_ROL_CANCELAR) toqueAgAtiva = NULL;
  return r;
}
#endif
// Velocidade da mola de 2a ordem da rolagem (anim_mola2): partida macia e
// cauda exponencial, a MESMA curva que a home mede. A de 1a ordem que estava
// aqui partia na velocidade maxima e o primeiro quadro ja saltava 12%.
static float velY;
static int   sair;
static int   pediuRelogio;
// O MODAL DA LINHA (qualquer OK numa linha, ver agendaui_evento). `ctxAberto`
// 1 = modal da serie (acoes + historico), 2 = manchetes, 3 = a noticia aberta.
static int    ctxFoco, ctxItem, notFoco;
// O painel que estava aberto por ultimo: com ctxAberto ja 0, o esvanecimento
// de saida ainda desenha ELE, e nao o modal da serie por cima.
static int    ctxUltimo;
static float  ctxA;
static char   pediuAbrir[40];
// Manchetes e noticia: quando o foco parou na linha (o trecho so e pedido
// depois de AGN_ESPERA_MS), a rolagem do texto e os portoes de texto.
static Uint32 notDesde;
static float  notRol, notRolAlvo, notRolMax, notVel;
static TextoGate notGate, notTrechoGate;
static float  notGateA, notTrechoA;
// A altura do painel da noticia anda por mola ate o alvo do estado (900 com
// texto ou carregando, AGL_H_FALHA no fallback). 0 = encaixa no primeiro quadro.
static float  notH, notHAlvo = 900.0f;
// A esquerda da C1: o item em foco e o anterior (copias), e o esvanecer entre
// as duas artes (0 = so a anterior, 1 = so a atual).
static AgItem arteCur, arteAnt;
static float  arteT = 1.0f;

enum { AG_CAB_LISTA = 1, AG_CAB_MES, AG_CAB_ANTERIOR, AG_CAB_HOJE, AG_CAB_PROXIMO };

// Onde a grade do MES termina (a lista da C1 mede pela ilha, ver c1()).
static float listaBase(void) { return NV_TELA_H - NV_MARGEM_Y; }

static int temData(const AgItem *it) {
  return it && it->dataProx[0] && agenda_dias(it->dataProx) >= 0;
}

static int anoBissexto(int a) {
  return (a % 4 == 0 && (a % 100 != 0 || a % 400 == 0));
}

static int diasNoMes(int a, int m) {
  static const unsigned char dias[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
  if (m < 1 || m > 12) return 30;
  return dias[m - 1] + (m == 2 && anoBissexto(a));
}

static void dataIso(int a, int m, int d, char *dst, size_t tam) {
  if (!dst || !tam) return;
  snprintf(dst, tam, "%04d-%02d-%02d", a, m, d);
}

static void selecionaData(int a, int m, int d) {
  if (m < 1) { --a; m = 12; }
  if (m > 12) { ++a; m = 1; }
  if (d < 1) d = 1;
  if (d > diasNoMes(a, m)) d = diasNoMes(a, m);
  calAno = a; calMes = m; calDia = d;
  { char primeiro[12];
    dataIso(calAno, calMes, 1, primeiro, sizeof primeiro);
    calCelula = agenda_semana(primeiro) + calDia - 1;
    if (calCelula < 0) calCelula = calDia - 1;
  }
  calEvento = 0;
#ifdef NV_TOUCH_UI
  toqueCalOffset = 0.0f; toquerol_limpar(&toqueCalendario);
#endif
}

static void selecionaCelula(int celula) {
  char primeiro[12];
  int dia, ano = calAno, mes = calMes;
  int semana;
  if (celula < 0) celula = 0;
  if (celula > 41) celula = 41;
  dataIso(ano, mes, 1, primeiro, sizeof primeiro);
  semana = agenda_semana(primeiro);
  if (semana < 0) semana = 0;
  dia = celula - semana + 1;
  if (dia < 1) {
    if (--mes < 1) { mes = 12; --ano; }
    dia += diasNoMes(ano, mes);
  } else if (dia > diasNoMes(ano, mes)) {
    dia -= diasNoMes(ano, mes);
    if (++mes > 12) { mes = 1; ++ano; }
  }
  selecionaData(ano, mes, dia);
}

static void mudaMes(int passo) {
  int a = calAno, m = calMes + passo, d = calDia;
  while (m < 1) { m += 12; --a; }
  while (m > 12) { m -= 12; ++a; }
  if (d > diasNoMes(a, m)) d = diasNoMes(a, m);
  selecionaData(a, m, d);
}

static void dataSelecionada(char *dst, size_t tam) {
  dataIso(calAno, calMes, calDia, dst, tam);
}

static int eventosDoDia(int *indices, int max) {
  char iso[12];
  int i, n = 0;
  dataSelecionada(iso, sizeof iso);
  for (i = 0; i < agenda_n(); i++) {
    const AgItem *it = agenda_lista(i);
    if (temData(it) && !strcmp(it->dataProx, iso)) {
      if (indices && n < max) indices[n] = i;
      ++n;
    }
  }
  return n;
}

// PONTEIRO (#99). Poe o foco pelas MESMAS variaveis que as setas mexem em
// agendaui_evento; o OK do clique chega depois pelo caminho de sempre (linha
// abre o modal, Lista/Mes troca a vista, dia abre o painel do dia, acao do
// modal age). Todas validam o estado e nao mexem em nada se ja e o foco.
static int calIniFixo = -1;   // janela do painel do dia parada sob o ponteiro
static void ponteiroCab(int id, int b) {
  (void)b;
  if (ctxAberto || focoCabecalho == id) return;
  if (!vistaMes && id != AG_CAB_LISTA && id != AG_CAB_MES) return;
  if (id < AG_CAB_LISTA || id > AG_CAB_PROXIMO) return;
  focoCabecalho = id;
}
static void ponteiroLinha(int i, int b) {
  (void)b;
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toqueAgenda);
#endif
  if (ctxAberto || vistaMes || i < 0 || i >= agenda_n() || i >= AG_MAX) return;
  if (foco == i && !focoCabecalho) return;
  focoCabecalho = 0; foco = i;
}
static void ponteiroDia(int c, int b) {
  (void)b;
  if (ctxAberto || !vistaMes || c < 0 || c > 41) return;
  if (c == calCelula && !calPainel && !focoCabecalho) return;
  focoCabecalho = 0; calPainel = 0;
  if (c != calCelula) selecionaCelula(c);
}
// `ini` = a primeira linha que o painel desenhou: a janela fica parada ali
// enquanto o ponteiro anda dentro dela (centrar no foco a cada passo poria
// outra linha debaixo do cursor).
static void ponteiroEvento(int linha, int ini) {
  if (ctxAberto || !vistaMes || linha < 0 || linha >= eventosDoDia(NULL, 0)) return;
  if (calPainel && calEvento == linha && !focoCabecalho) return;
  focoCabecalho = 0; calPainel = 1; calEvento = linha; calIniFixo = ini;
}

// --- C1: A GEOMETRIA DA TELA (dono aprovou a variacao C1 de agenda-v2.html) --
//
// A mesma divisao dos Ajustes A3: a ESQUERDA mostra o episodio em foco grande
// (arte, quando, titulo, episodio, sinopse, lembrete) e a DIREITA e uma ilha
// com a lista compacta agrupada (Hoje / Esta semana / Mais tarde / sem data)
// sobre o fio do tempo. As medidas sao as do mockup em pixel REAL de
// 1920x1080 (Montserrat, a fonte da TV do dono): P() converte para a tela
// virtual da Agenda, que continua com o piso de 120% para o mes e os paineis.
// Assim a lista sai do tamanho que o dono aprovou em qualquer escala.
static float P(float px) { return px / agEscala(); }
static float agTelefonePx(float px) { return px * gfx_escala_ui(); }

typedef struct {
  float x0;                     // margem esquerda (rail + 96)
  float artY, artW, artH;       // a arte grande
  float infoY;                  // o bloco de texto da esquerda
  float pnX, pnY, pnW, pnH;     // a ilha da direita
  float lsX, lsY, lsW, lsH;     // a lista (dentro da ilha, abaixo do cabecalho)
} AgC1;

static AgC1 c1(void) {
  AgC1 L;
  float dir = NV_TELA_W - P(56), avail;
  L.x0 = ajustes_rail_largura_fixa() / agEscala() + P(96);
  avail = dir - L.x0;
  // 760 de arte, 74 de vao e 934 de ilha no mockup (1768 sem rail): com a rail
  // fixa as tres partes encolhem juntas, na mesma proporcao.
  L.artW = avail * 760.0f / 1768.0f;
  L.artH = L.artW * 428.0f / 760.0f;
  L.artY = P(150);
  L.infoY = L.artY + L.artH + P(28);
  L.pnX = L.x0 + L.artW + avail * 74.0f / 1768.0f;
  L.pnY = P(112);
  L.pnW = dir - L.pnX;
  L.pnH = NV_TELA_H - P(44) - L.pnY;
  if (agColunaUnica()) {
    L.x0 = agX();
    L.artW = L.artH = 0.0f;
    L.pnX = L.x0;
    L.pnW = agFim() - L.pnX;
    L.pnY = P(agTelefonePx(142));
    L.pnH = NV_TELA_H - P(44) - L.pnY;
  }
  L.lsX = L.pnX + P(22);
  L.lsY = L.pnY + P(22) + P(44) + P(8);
  L.lsW = L.pnW - P(44);
  L.lsH = L.pnY + L.pnH - P(22) - L.lsY;
  if (agColunaUnica()) {
    L.lsY = L.pnY + P(agTelefonePx(264));
    L.lsH = L.pnY + L.pnH - P(22) - L.lsY;
  }
  return L;
}

// O GRUPO de uma linha: 0 hoje, 1 os proximos seis dias, 2 mais tarde, 3 sem
// data (encerrada, cancelada, sem anuncio — continuam linhas de verdade, com o
// modal e as noticias, so apagadas e no fim).
static int grupoDe(const AgItem *it) {
  int d;
  if (!temData(it)) return 3;
  d = agenda_dias(it->dataProx);
  return d == 0 ? 0 : d < 7 ? 1 : 2;
}
static int grupo(int i) { return grupoDe(agenda_lista(i)); }

#define AG_ROW_H   116.0f   // 96 de miniatura + 10 em cima e embaixo
#define AG_GRP_H    51.0f   // .grp: 18 + texto + 6 + fio + 6
#define AG_GAP       4.0f
// A lista da TV mede em pixels reais. No telefone, texto, linhas e botoes
// acompanham o tamanho da interface, sem alterar a lista do painel Social.
static float agLinhaH(void) { return P(agColunaUnica() ? agTelefonePx(184) : AG_ROW_H); }
static float agGrupoH(void) { return P(agColunaUnica() ? agTelefonePx(72) : AG_GRP_H); }
static float agLinhaGap(void) { return P(agColunaUnica() ? agTelefonePx(12) : AG_GAP); }

// y do TOPO DA LINHA `i` em coordenada de DOCUMENTO (antes da rolagem). Um
// cabecalho de grupo entra antes da primeira linha de cada grupo.
static float yDe(int i) {
  float y = 0.0f;
  int j, g = -1;
  for (j = 0; j <= i && j < agenda_n(); j++) {
    int gj = grupo(j);
    if (gj != g) { y += agGrupoH() + agLinhaGap(); g = gj; }
    if (j < i) y += agLinhaH() + agLinhaGap();
  }
  return y;
}
static int abreGrupo(int i) { return i == 0 || grupo(i) != grupo(i - 1); }

static float alturaDoc(void) {
  int n = agenda_n();
  if (n <= 0) return 0.0f;
  return yDe(n - 1) + agLinhaH();
}

// A troca de interface descarta a rolagem livre e as medidas da tela anterior.
static void agInterfaceAtualizar(void) {
#ifdef NV_TOUCH_UI
  static int modoAnterior = -1;
  int mobile = layout_modo_mobile();
  if (modoAnterior == mobile) return;
  modoAnterior = mobile;
  memset(&toqueAgenda, 0, sizeof toqueAgenda);
  memset(&toqueNoticia, 0, sizeof toqueNoticia);
  memset(&toqueCalendario, 0, sizeof toqueCalendario);
  memset(&toqueManchetes, 0, sizeof toqueManchetes);
  memset(&toqueModal, 0, sizeof toqueModal);
  toqueAgAtiva = NULL;
  toqueCalOffset = toqueManchetesOffset = toqueModalOffset = 0.0f;
  toqueManchetesChave = 0;
  scrollY = velY = 0.0f;
  notRol = notRolAlvo = notRolMax = notVel = 0.0f;
  notH = escModal = 0.0f;
  calIniFixo = -1;
#endif
}

int agendaui_iniciar(void) {
  agInterfaceAtualizar();
  int i;
  sair = 0; pediuRelogio = 0;
  foco = 0;
  vistaMes = focoCabecalho = calPainel = calEvento = 0;
  { const char *h = agenda_hoje();
    int a = agenda_ano(h), m = agenda_mes(h), d = agenda_dia(h);
    selecionaData(a ? a : 2026, m ? m : 1, d ? d : 1); }
  scrollY = 0.0f; velY = 0.0f;
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toqueAgenda); toquerol_limpar(&toqueNoticia); toquerol_limpar(&toqueCalendario);
  toqueManchetesReiniciar();
  toqueModalOffset = 0; toquerol_limpar(&toqueModal);
#endif
  ctxAberto = 0; ctxUltimo = 0; ctxFoco = 0; ctxA = 0.0f; notFoco = 0;
  notRol = notRolAlvo = notRolMax = notVel = 0.0f;
  for (i = 0; i < AG_MAX; i++) animFoco[i] = 0.0f;
  memset(&arteCur, 0, sizeof arteCur); memset(&arteAnt, 0, sizeof arteAnt);
  arteT = 1.0f;
  agenda_iniciar();
  agenda_montar();
  versaoVista = agenda_versao();
  // Uma passada de rede por abertura da tela, e so para as series seguidas com
  // registro faltando ou velho. Ver a nota longa em agenda.c: este e o unico
  // pedido do recurso que nao vem de graca.
  agenda_atualizar_seguidas();
  // As manchetes de cada titulo, em fio proprio e com cache de 6 h: e o que a
  // citacao da linha e o painel do menu de contexto mostram.
  { int i, n = agenda_n();
    for (i = 0; i < n && i < 16; i++) {
      const AgItem *it = agenda_lista(i);
      if (it && it->imdb[0] && it->titulo[0]) noticias_pedir(it->imdb, it->titulo, it->rede, 1);
    } }
  return 1;
}

int agendaui_quer_sair(void) { int s = sair; sair = 0; return s; }
int agendaui_pediu_relogio(void) { int v = pediuRelogio; pediuRelogio = 0; return v; }

static void alternarLembrete(void) {
  const AgItem *it = agenda_lista(foco);
  if (it && it->imdb[0]) {
    agenda_alternar_lembrete(it->imdb);
    trocaEm = SDL_GetTicks();
    // Remonta para o estado do lembrete voltar na linha no mesmo quadro. A
    // ordem nao muda (a chave e a data), entao o foco continua onde estava.
    agenda_montar();
  }
}

// --- O MODAL DA LINHA -----------------------------------------------------------
//
// PEDIDO DO DONO (29/09/2026): "deixar sempre o modal contextual quando clicar".
// Ate aqui o OK curto ligava/desligava o lembrete e so SEGURAR OK (NV_HOLD_MS)
// abria o menu — duas acoes no mesmo botao, e a que abre o que a linha tem para
// dizer era a escondida. Agora QUALQUER OK abre o modal, e o lembrete e uma das
// acoes dele. Segurar continua abrindo o mesmo modal: quem aprendeu o gesto
// antigo nao cai num lugar diferente.
//
// O modal responde a pergunta que a linha nao cabe: "o que saiu desde que eu
// liguei o lembrete, e o que disso eu vi?". As acoes sao montadas a partir do
// que e VERDADE agora (montaAcoes), e nao de uma lista fixa:
//   Assistir T<n>E<n>        o primeiro lancado e NAO visto da janela
//   Abrir o titulo
//   Ultimas noticias
//   Marcar ... como assistido os lancados que o mapa ainda nao tem como vistos
//   Lembrar-me / Desligar      so quando da para ligar, ou quando esta ligado
//                              (um lembrete de data vencida tem de poder sair)
enum { AC_ASSISTIR, AC_ABRIR, AC_NOTICIAS, AC_VISTOS, AC_LEMBRETE, AC_N };

#define AG_HIST_VIS 6

// A janela do historico e as acoes do modal, num lugar so: o desenho e o
// evento chamam a MESMA conta, entao o foco nunca aponta para uma acao que o
// desenho nao mostrou.
typedef struct {
  AgEp eps[AG_HIST_VIS];
  int  n, estado, proximo;   // proximo = indice em eps, -1 = nada a assistir
  int  naoVistos;            // lancados com visto != 1
  int  ac[AC_N], nAc;
} AgModal;

static void montaModal(const AgItem *it, AgModal *m) {
  memset(m, 0, sizeof *m);
  m->proximo = -1;
  if (!it) return;
  m->n = agenda_historico(it->imdb, m->eps, AG_HIST_VIS, &m->estado);
  if (m->n > 0) {
    int i;
    m->proximo = agenda_historico_proximo(m->eps, m->n);
    for (i = 0; i < m->n; i++) if (m->eps[i].visto != 1) m->naoVistos++;
  }
  if (m->proximo >= 0) m->ac[m->nAc++] = AC_ASSISTIR;
  m->ac[m->nAc++] = AC_ABRIR;
  m->ac[m->nAc++] = AC_NOTICIAS;
  if (m->naoVistos > 0) m->ac[m->nAc++] = AC_VISTOS;
  if (it->lembrete || agenda_pode_lembrar(it->imdb)) m->ac[m->nAc++] = AC_LEMBRETE;
}

static char pediuTocarId[40];
static int  pediuTocarT, pediuTocarE;

static void abrirModal(int i) {
  const AgItem *it = agenda_lista(i);
  if (!it) return;
  ctxAberto = 1; ctxFoco = 0; ctxItem = i;
#ifdef NV_TOUCH_UI
  toqueModalOffset = 0; toquerol_limpar(&toqueModal);
#endif
  // Os dois pedidos de rede do modal saem AQUI e so aqui: a grade de episodios
  // (um GET ao Cinemeta, 30 min de memoria) e as manchetes (cache de 6 h, na
  // pratica ja pedidas ao abrir a tela).
  agenda_historico_pedir(it->imdb);
  noticias_pedir(it->imdb, it->titulo, it->rede, 1);
}

// MARCAR COMO ASSISTIDO: os lancados da janela que o mapa nao tem como vistos.
// Local primeiro (a lista redesenha no mesmo quadro) e depois os destinos
// vinculados em fio, pelo mesmo visto_episodios da lista de episodios do
// detalhe — Trakt, Simkl e conta, cada um se estiver ligado.
static void marcarVistos(const AgItem *it, const AgModal *m) {
  VistoPar pares[AG_HIST_VIS];
  int i, n = 0;
  for (i = 0; i < m->n; i++)
    if (m->eps[i].visto != 1) {
      pares[n].temporada = (short)m->eps[i].temporada;
      pares[n].episodio  = (short)m->eps[i].episodio;
      n++;
    }
  if (!n) return;
  printf("[visto] marcado por agenda-manual pos=0 dur=0 assistido=0s id=%s n=%d\n", it->imdb, n);
  fflush(stdout);
  vistoep_marcar_lote(it->imdb, pares, n, 1);
  visto_episodios(it->imdb, "series", pares, n, 1, visto_destinos());
}

static void acaoModal(int ac) {
  const AgItem *it = agenda_lista(ctxItem);
  AgModal m;
  if (!it) return;
  montaModal(it, &m);
  switch (ac) {
    case AC_ASSISTIR:
      if (m.proximo >= 0) {
        snprintf(pediuTocarId, sizeof pediuTocarId, "%s", it->imdb);
        pediuTocarT = m.eps[m.proximo].temporada;
        pediuTocarE = m.eps[m.proximo].episodio;
        ctxAberto = 0;
      }
      break;
    case AC_ABRIR:
      snprintf(pediuAbrir, sizeof pediuAbrir, "%s", it->imdb);
      ctxAberto = 0;
      break;
    case AC_NOTICIAS:
      ctxAberto = 2; notFoco = 0; notDesde = SDL_GetTicks();
#ifdef NV_TOUCH_UI
      toqueManchetesReiniciar();
#endif
      break;
    case AC_VISTOS:
      marcarVistos(it, &m);
      break;
    case AC_LEMBRETE:
      // O modal FICA aberto: o titulo do historico ("Lancados desde o
      // lembrete" / "Ultimos episodios") e a linha do lembrete mudam na frente
      // de quem apertou, que e a confirmacao que o gesto precisa.
      foco = ctxItem;
      alternarLembrete();
      break;
  }
}

static void ponteiroAcao(int i, int b) {
  AgModal m;
  (void)b;
  if (ctxAberto != 1 || i == ctxFoco || i < 0) return;
  montaModal(agenda_lista(ctxItem), &m);
  if (i < m.nAc) ctxFoco = i;
}
static void ponteiroManchete(int i, int b) {
  const AgItem *it = agenda_lista(ctxItem);
  (void)b;
  if (ctxAberto != 2 || !it || i == notFoco || i < 0 || i >= noticias_n(it->imdb)) return;
  notFoco = i;
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toqueManchetes);
#endif
  notDesde = SDL_GetTicks();
  textogate_reiniciar(&notTrechoGate); notTrechoA = 0.0f;
}

void agendaui_teste_foco(int *linha, int *cabecalho, int *modal, int *modalFoco,
                         int *celula, int *evento) {
  if (linha) *linha = foco;
  if (cabecalho) *cabecalho = focoCabecalho;
  if (modal) *modal = ctxAberto;
  if (modalFoco) *modalFoco = ctxAberto == 2 ? notFoco : ctxFoco;
  if (celula) *celula = calPainel ? -1 : calCelula;
  if (evento) *evento = calPainel ? calEvento : -1;
}

void agendaui_evento(const SDL_Event *e) {
  pediuRelogio = 0;
  agInterfaceAtualizar();
#ifdef NV_TOUCH_UI
  if (agColunaUnica() && toquerol_navegacao(e)) {
    if (toquerol_livre(&toqueAgenda)) {
      for (int i = 0; i < agenda_n() && i < AG_MAX; i++)
        if (yDe(i) + agLinhaH() > scrollY) { foco = i; break; }
    }
    if (toquerol_livre(&toqueCalendario) && !agColunaUnica()) calEvento = (int)(toqueCalOffset / 78.0f);
    if (toquerol_livre(&toqueNoticia)) notRolAlvo = notRol;
    if (ctxAberto == 2 && toquerol_livre(&toqueManchetes)) {
      const AgItem *it = agenda_lista(ctxItem);
      int nn = noticias_n(it ? it->imdb : "");
      if (nn > 0) notFoco = toqueMancheteIndice(toqueManchetesOffset, nn);
    }
    toquerol_limpar(&toqueAgenda); toquerol_limpar(&toqueNoticia); toquerol_limpar(&toqueCalendario);
    toquerol_limpar(&toqueManchetes);
    toquerol_limpar(&toqueModal);
  }
#endif
  SDL_Keycode k;
  int n = agenda_n();
  int volta = 0, ok;
  k = e->key.keysym.sym;
  volta = (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE || k == SDLK_DELETE);
  ok = (k == SDLK_RETURN || k == SDLK_KP_ENTER);
  if (e->type != SDL_KEYDOWN) return;
  if (!ok) calIniFixo = -1;   // seta: a janela do dia volta a seguir o foco
  if (ctxAberto == 3) {           // a noticia aberta
    if (volta || k == SDLK_LEFT) { ctxAberto = 2; return; }
    if (k == SDLK_DOWN) notRolAlvo += 132.0f;
    else if (k == SDLK_UP) { notRolAlvo -= 132.0f; if (notRolAlvo < 0.0f) notRolAlvo = 0.0f; }
    return;
  }
  if (ctxAberto == 2) {           // a lista de manchetes
    const AgItem *it = agenda_lista(ctxItem);
    int nn = noticias_n(it ? it->imdb : "");
    if (volta) { ctxAberto = 1; return; }
    if ((k == SDLK_DOWN && notFoco < nn - 1) || (k == SDLK_UP && notFoco > 0)) {
      notFoco += k == SDLK_DOWN ? 1 : -1;
      notDesde = SDL_GetTicks();
      textogate_reiniciar(&notTrechoGate); notTrechoA = 0.0f;
    }
    else if (ok && !e->key.repeat && it) {
      const Noticia *nt = noticias_item(it->imdb, notFoco);
      if (nt) {
        ctxAberto = 3;
        notRol = notRolAlvo = 0.0f;
        notH = 0.0f;
        textogate_reiniciar(&notGate);
        if (nt->link[0]) noticia_pedir(nt->link);
      }
    }
    return;
  }
  if (ctxAberto == 1) {
    const AgItem *it = agenda_lista(ctxItem);
    AgModal m;
    montaModal(it, &m);
    if (volta) { ctxAberto = 0; return; }
    if (k == SDLK_DOWN && ctxFoco < m.nAc - 1) ctxFoco++;
    else if (k == SDLK_UP && ctxFoco > 0) ctxFoco--;
    else if (ok && !e->key.repeat && ctxFoco < m.nAc) {
      int ac = m.ac[ctxFoco], j;
      acaoModal(ac);
      // O FOCO SEGUE A ACAO, nao o indice. "Marcar como assistido" some da
      // lista quando nao sobra nada a marcar, e "Assistir" pode sumir junto: o
      // indice antigo caia em "Desligar o lembrete" — um OK a mais desligava o
      // lembrete sem a pessoa ter ido ate ele (captura -fx-modal-vistos).
      // A mesma acao se ainda existe; senao, a primeira.
      if (ctxAberto == 1) {
        montaModal(agenda_lista(ctxItem), &m);
        ctxFoco = 0;
        for (j = 0; j < m.nAc; j++) if (m.ac[j] == ac) { ctxFoco = j; break; }
      }
    }
    return;
  }
  if (vistaMes) {
    int itens[AG_MAX], qtd = eventosDoDia(itens, AG_MAX);
    if (focoCabecalho) {
      int ordemLista[] = { AG_CAB_ANTERIOR, AG_CAB_HOJE, AG_CAB_PROXIMO,
                           AG_CAB_LISTA, AG_CAB_MES };
      int ordemListaN = vistaMes ? 5 : 2;
      int pos = 0, j;
      if (volta || k == SDLK_DOWN) { focoCabecalho = 0; return; }
      if (k == SDLK_UP) { if (ajustes_menu_topo()) sair = 1; else pediuRelogio = 1; return; }
      if (k == SDLK_LEFT || k == SDLK_RIGHT) {
        for (j = 0; j < ordemListaN; j++) {
          int id = vistaMes ? ordemLista[j] : (j == 0 ? AG_CAB_LISTA : AG_CAB_MES);
          if (id == focoCabecalho) { pos = j; break; }
        }
        // ESQUERDA no primeiro chip abre a barra lateral por cima da Agenda
        // (dono, 03/10), em vez de dar a volta ate o ultimo chip.
        if (k == SDLK_LEFT && pos == 0) { sair = 1; return; }
        pos += k == SDLK_RIGHT ? 1 : -1;
        if (pos < 0) pos = ordemListaN - 1;
        if (pos >= ordemListaN) pos = 0;
        focoCabecalho = vistaMes ? ordemLista[pos]
                                  : (pos == 0 ? AG_CAB_LISTA : AG_CAB_MES);
        return;
      }
      if (ok && !e->key.repeat) {
        switch (focoCabecalho) {
          case AG_CAB_LISTA: vistaMes = 0; calPainel = 0; focoCabecalho = 0; break;
          case AG_CAB_MES: vistaMes = 1; calPainel = 0; focoCabecalho = 0; break;
          case AG_CAB_HOJE: {
            const char *h = agenda_hoje();
            selecionaData(agenda_ano(h), agenda_mes(h), agenda_dia(h));
            focoCabecalho = 0;
            break;
          }
          case AG_CAB_ANTERIOR: mudaMes(-1); focoCabecalho = 0; break;
          case AG_CAB_PROXIMO: mudaMes(1); focoCabecalho = 0; break;
        }
      }
      return;
    }
    if (volta) {
      if (calPainel) { calPainel = 0; return; }
      sair = 1; return;
    }
    if (calPainel) {
      if (k == SDLK_LEFT) { calPainel = 0; return; }
      if (k == SDLK_UP) {
        if (calEvento > 0) calEvento--;
        else calPainel = 0;
      } else if (k == SDLK_DOWN && calEvento < qtd - 1) calEvento++;
      else if (ok && !e->key.repeat && calEvento < qtd)
        abrirModal(itens[calEvento]);
      return;
    }
    if (k == SDLK_UP && calCelula < 7) {
      focoCabecalho = AG_CAB_HOJE;
      return;
    }
    // COLUNA 0 DA GRADE (domingo): a barra lateral por cima (dono, 03/10).
    // Antes a ESQUERDA ali pulava para o ultimo dia da semana de cima.
    if (k == SDLK_LEFT) {
      if (calCelula % 7 == 0) sair = 1;
      else selecionaCelula(calCelula - 1);
      return;
    }
    if (k == SDLK_RIGHT) {
      if (calCelula < 41) selecionaCelula(calCelula + 1);
      return;
    }
    if (k == SDLK_UP) {
      if (calCelula >= 7) selecionaCelula(calCelula - 7);
      return;
    }
    if (k == SDLK_DOWN) {
      if (calCelula < 35) selecionaCelula(calCelula + 7);
      return;
    }
    if (ok && !e->key.repeat && qtd > 0) {
#ifdef NV_TOUCH_UI
      if (agColunaUnica()) toquerol_limpar(&toqueCalendario);
#endif
      calPainel = 1; calEvento = 0;
    }
    return;
  }
  if (focoCabecalho) {
    if (volta || k == SDLK_DOWN) { focoCabecalho = 0; return; }
    if (k == SDLK_UP) { if (ajustes_menu_topo()) sair = 1; else pediuRelogio = 1; return; }
    if (k == SDLK_LEFT && focoCabecalho == AG_CAB_LISTA) { sair = 1; return; }
    if (k == SDLK_LEFT || k == SDLK_RIGHT) {
      focoCabecalho = focoCabecalho == AG_CAB_LISTA ? AG_CAB_MES : AG_CAB_LISTA;
      return;
    }
    if (ok && !e->key.repeat) {
      vistaMes = focoCabecalho == AG_CAB_MES;
      focoCabecalho = 0;
      calPainel = 0;
    }
    return;
  }
  if (volta || k == SDLK_LEFT) { sair = 1; return; }
  if (k == SDLK_UP && foco == 0) { focoCabecalho = AG_CAB_LISTA; return; }
  if (k == SDLK_DOWN && foco < n - 1) foco++;
  else if (k == SDLK_UP && foco > 0)  foco--;
  // NO KEYDOWN, e nao no KEYUP como o gesto de segurar pedia: sem duas acoes
  // no mesmo botao nao ha duracao a medir, e o modal aparece no instante do
  // aperto. O KEYUP que vem depois cai no `return` do topo (so KEYDOWN passa),
  // e a repeticao do OK segurado e ignorada tanto aqui quanto nas acoes.
  else if (ok && !e->key.repeat) abrirModal(foco);
  // ESQUERDA cai fora daqui e chega ao menu lateral, como nas outras telas.
}

const char *agendaui_pediu_tocar(int *temporada, int *episodio) {
  static char s[40];
  if (!pediuTocarId[0]) return NULL;
  snprintf(s, sizeof s, "%s", pediuTocarId);
  if (temporada) *temporada = pediuTocarT;
  if (episodio) *episodio = pediuTocarE;
  pediuTocarId[0] = 0;
  return s;
}

const char *agendaui_pediu_abrir(void) {
  static char s[40];
  if (!pediuAbrir[0]) return NULL;
  snprintf(s, sizeof s, "%s", pediuAbrir);
  pediuAbrir[0] = 0;
  return s;
}
int agendaui_menu_aberto(void) { return ctxAberto != 0; }

void agendaui_atualizar(float dt, Uint32 agora) {
  agInterfaceAtualizar();
  int i, n;
  float alvoY, topo, base, y, h;
  (void)agora;
  // O FIO TERMINOU: remonta, para o que ele trouxe aparecer JA. Ate aqui o
  // resultado so entrava na proxima abertura da tela — o dono abria a Agenda,
  // o fio preenchia nome, cartaz e data no cache, e a tela continuava com
  // "TV Show" e o retangulo cinza ate ele sair e voltar. A ordem pode mudar (a
  // serie ganhou data), entao o foco segue o TITULO, nao o indice.
  { int v = agenda_versao();
    if (v != versaoVista) {
      char id[24] = "";
      const AgItem *f = agenda_lista(foco);
      versaoVista = v;
      if (f) snprintf(id, sizeof id, "%s", f->imdb);
      agenda_montar();
      for (i = 0; id[0] && i < agenda_n(); i++)
        if (!strcmp(agenda_lista(i)->imdb, id)) { foco = i; break; }
    } }
  n = agenda_n();
  // REDUZIR ANIMACOES: o alvo entra direto, sem mola, tanto no foco quanto na
  // rolagem. Mesma forma de ajustes.c — o estado final e o mesmo e nada da tela
  // depende de estar a meio caminho. Continua valendo agora que a linha nao
  // cresce mais: o que a mola ainda move e a ROLAGEM, e e ela que quem liga o
  // ajuste pediu para nao deslizar.
  int reduz = ajustes_animacoes_reduzidas();
  ctxA = reduz ? (ctxAberto ? 1.0f : 0.0f) : anim_mola(ctxA, ctxAberto ? 1.0f : 0.0f, dt, 18.0f);
  if (ctxAberto) ctxUltimo = ctxAberto;
  // A rolagem da noticia: o alvo anda 132 px por tecla e para no fim do texto
  // (notRolMax, medido no desenho); a mesma mola 2 da lista.
  if (notRolAlvo > notRolMax) notRolAlvo = notRolMax;
  if (notRolAlvo < 0.0f) notRolAlvo = 0.0f;
#ifdef NV_TOUCH_UI
  if (agColunaUnica() && toquerol_livre(&toqueNoticia)) { notRolAlvo = notRol; notVel = 0.0f; }
  else
#endif
    notRol = anim_mola2_reduzida(&notVel, notRol, notRolAlvo, dt, NV_MOLA2_SCROLL, reduz);
  notH = (reduz || notH <= 0.0f) ? notHAlvo : anim_mola(notH, notHAlvo, dt, 18.0f);
  if (foco >= n) foco = n > 0 ? n - 1 : 0;
  for (i = 0; i < n && i < AG_MAX; i++) {
    float a = (i == foco) ? 1.0f : 0.0f;
    animFoco[i] = reduz ? a
                        : anim_mola(animFoco[i], a, dt,
                                    a > animFoco[i] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  }
  // ROLAGEM MINIMA para a linha focada caber, e nao "alinhar ao topo" — mesma
  // regra da Biblioteca. Quando a linha abre um grupo, o cabecalho dele entra
  // junto: "Qui 17" sem o "Esta semana" em cima perde o contexto.
  { AgC1 L = c1();
    topo = 0.0f; base = L.lsH;
    y = yDe(foco);
    h = agLinhaH();
    if (abreGrupo(foco)) { y -= agGrupoH() + agLinhaGap(); h += agGrupoH() + agLinhaGap(); }
    if (foco == 0) { h += y; y = 0.0f; } }
  alvoY = scrollY;
  if (y - alvoY < 0.0f) alvoY = y;
  if (y + h - alvoY > base - topo) alvoY = y + h - (base - topo);
  { float maxY = alturaDoc() - (base - topo);
    if (maxY < 0.0f) maxY = 0.0f;
    if (alvoY > maxY) alvoY = maxY;
    if (alvoY < 0.0f) alvoY = 0.0f; }
  // A ESQUERDA ACOMPANHA O FOCO com um esvanecer curto da arte: a anterior fica
  // por baixo ate a nova cobrir. Guardada por COPIA (o ponteiro de agenda_lista
  // nao sobrevive a uma remontagem).
  { const AgItem *f = agenda_lista(foco);
    if (f && strcmp(f->imdb, arteCur.imdb)) {
      if (arteCur.imdb[0]) { arteAnt = arteCur; arteT = 0.0f; }
      else arteT = 1.0f;
    }
    if (f) arteCur = *f;
    else memset(&arteCur, 0, sizeof arteCur);
    arteT = reduz ? 1.0f : arteT + dt * 5.0f;
    if (arteT > 1.0f) arteT = 1.0f; }
#ifdef NV_TOUCH_UI
  if (!agColunaUnica() || !toquerol_livre(&toqueAgenda))
#endif
    scrollY = anim_mola2_reduzida(&velY, scrollY, alvoY, dt, NV_MOLA2_SCROLL, reduz);
}

// MAIUSCULA que nao quebra acento. O nome do mes vem em minusculas de
// agenda.c (os MESMOS de desc_data_extenso) e o cabecalho do calendario quer
// caixa alta; toupper() byte a byte transformaria o 0xC3 de "marco" em lixo.
// A regra do bloco Latin-1 e uma so: 0xC3 0xAn/0xBn -> 0xC3 (0xAn - 0x20);
// romeno e cirilico entram pela mesma funcao (idiomacod.h).
static void maiusc(char *dst, size_t tam, const char *s) {
  // Latin-1, Latin Estendido-A, vietnamita, grego e cirilico (ver idioma_maiusc_em
  // em idiomacod.h): o mes chega em qualquer dos idiomas. O turco muda o "i".
  idioma_maiusc_em(ajustes_idioma(), dst, tam, s);
}

// Uma regua de 1px (o historico do modal).
static void regua(float x0, float x1, float y, float lum, float a) {
  GfxRect r = { x0, y, x1 - x0, 1.0f };
  if (r.w <= 0.0f) return;
  gfx_cor(r, 0.0f, lum, lum, lum, a);
}

// TEXTO REDUZIDO: a textura de um estilo desenhada em `esc` do tamanho (ver
// "CORPOS QUE O text.c NAO TEM"). O canto encaixa no pixel como em txt_desenhar_alpha — fora da
// grade o GL_LINEAR espalharia cada letra por duas colunas.
static void txtEsc(TxtLinha l, float x, float y, float esc, float a) {
  GfxRect r;
  if (!l.tex || a <= 0.001f) return;
  r.x = floorf(x + 0.5f); r.y = floorf(y + 0.5f);
  r.w = (float)l.w * esc; r.h = (float)l.h * esc;
  gfx_rect(r, l.tex, GFX_TEXTO, 0, 0, 0, 0.0f, 1, 1, 1, a);
}
static float altLinha(TxtEstilo e, int c);
// O ROTULO EM CAIXA ALTA do mockup (.cap2: 15/600, letter-spacing .06em): o
// TXT_MINI (15 bold) espacado. `x` < 0 so mede.
static float kicker(const char *s, int r, int g, int b, float x, float y, float a) {
  return txt_tracking(TXT_MINI, s, r, g, b, x, y, a, 0.9f);
}
static void acentoInt(int *r, int *g, int *b) {
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  *r = (int)(ar * 255.0f + 0.5f); *g = (int)(ag * 255.0f + 0.5f);
  *b = (int)(ab * 255.0f + 0.5f);
}

// --- C1: AS PECAS DA TELA ----------------------------------------------------
//
// Corpos do mockup em pixel real, rasterizados no estilo mais proximo ACIMA e
// desenhados reduzidos (txtEsc), como o resto deste arquivo: reduzir com
// GL_LINEAR nao borra. `S` e o corpo do estilo na tabela de text.c.
#define AG_INK    244, 245, 247
#define AG_INK2   179, 183, 191
#define AG_INK3   117, 122, 132

// ALTURA DA CAIXA DE LINHA de um estilo, medida na fonte ativa. "Hg" tem
// ascendente e descendente: medir a string real faria a base oscilar entre
// linhas. A cor e a de quem desenha, para cair na mesma entrada do cache.
static float altLinha(TxtEstilo e, int c) {
  float h = (float)txt_linha(e, "Hg", c, c, c, 255).h;
  return h > 0.0f ? h : 28.0f;
}
static float escDe(float realPx, float S) { return P(realPx) / S; }
// Uma linha cortada em `larg` e desenhada em `realPx`. Devolve a largura na
// tela; `x` < 0 so mede.
static float txC1(TxtEstilo e, float S, const char *s, int r, int g, int b,
                  float x, float y, float realPx, float larg, float a) {
  float esc = escDe(realPx, S);
  TxtLinha l;
  if (!s || !s[0]) return 0.0f;
  l = txt_linha_corta(e, s, r, g, b, 255, larg / esc);
  if (x >= 0.0f) txtEsc(l, x, y, esc, a);
  return (float)l.w * esc;
}
static float hC1(TxtEstilo e, float S, float realPx, int c) {
  return altLinha(e, c) * escDe(realPx, S);
}
// Texto corrido em ate `max` linhas, com "…" na ultima (agQuebra, a mesma
// quebra da sinopse do cartao do lembrete). Devolve as linhas desenhadas.
static int blocoC1(TxtEstilo e, float S, const char *s, float x, float y,
                   float realPx, float larg, float leadReal, int max,
                   int r, int g, int b, float a) {
  char lin[AG_SIN_MAX][512];
  const char *resto;
  float esc = escDe(realPx, S);
  int n, k;
  if (!s || !s[0] || max <= 0) return 0;
  n = agQuebra(e, s, larg / esc, max, lin, &resto, r, g, b);
  for (k = 0; k < n; k++)
    txtEsc(txt_linha_corta(e, lin[k], r, g, b, 255, larg / esc),
           x, y + P(leadReal) * (float)k, esc, a);
  if (resto[0]) {
    txtEsc(txt_linha_corta(e, resto, r, g, b, 255, larg / esc),
           x, y + P(leadReal) * (float)n, esc, a);
    n++;
  }
  return n;
}

// O acento clareado do mockup (color-mix com branco): kicker a 45%, selo a 50%.
static void acentoClaro(float fr, int *r, int *g, int *b) {
  int ar, ag, ab;
  acentoInt(&ar, &ag, &ab);
  *r = (int)(ar * fr + 255.0f * (1.0f - fr) + 0.5f);
  *g = (int)(ag * fr + 255.0f * (1.0f - fr) + 0.5f);
  *b = (int)(ab * fr + 255.0f * (1.0f - fr) + 0.5f);
}

// PRIMEIRA LETRA MAIUSCULA ("qui" -> "Qui"), sem quebrar acento ou cirilico:
// a primeira sequencia UTF-8 passa pela mesma maiusc do resto do arquivo.
static void capital(char *dst, size_t tam, const char *s) {
  size_t n = 1;
  char ini[8], up[16];
  if (!tam) return;
  dst[0] = 0;
  if (!s || !s[0]) return;
  while (s[n] && ((unsigned char)s[n] & 0xC0) == 0x80 && n < 4) n++;
  memcpy(ini, s, n); ini[n] = 0;
  maiusc(up, sizeof up, ini);
  snprintf(dst, tam, "%s%s", up, s + n);
}

// A DATA CURTA da linha: "Hoje", "Qui 17", "Qua 4/11" (outro mes leva o mes).
static void dataLinha(const AgItem *it, char *dst, size_t tam) {
  char sem[24];
  const char *hj = agenda_hoje();
  int d = agenda_dia(it->dataProx), m = agenda_mes(it->dataProx);
  dst[0] = 0;
  if (!temData(it)) return;
  if (agenda_dias(it->dataProx) == 0) { snprintf(dst, tam, "%s", i18n("Hoje")); return; }
  capital(sem, sizeof sem, i18n(agenda_semana_nome(agenda_semana(it->dataProx))));
  if (hj && (agenda_mes(hj) != m || agenda_ano(hj) != agenda_ano(it->dataProx)))
    snprintf(dst, tam, "%s %d/%d", sem, d, m);
  else
    snprintf(dst, tam, "%s %d", sem, d);
}

static const char *situacaoTxt(const AgItem *it) {
  switch (it->situacao) {
    case AG_ENCERRADA: return i18n("Encerrada");
    case AG_CANCELADA: return i18n("Cancelada");
    case AG_PRODUCAO:  return i18n("Em produção");
  }
  return NULL;
}

// Segunda linha do conteudo: o episodio. Vazio quando nao ha nada verdadeiro a
// dizer — e ai a linha some.
static void linhaEpisodio(const AgItem *it, char *dst, size_t tam) {
  dst[0] = 0;
  if (it->dataProx[0] && it->temporada > 0 && it->episodio > 0) {
    if (it->nomeEp[0])
      snprintf(dst, tam, "T%d E%d \xc2\xb7 %s", it->temporada, it->episodio, it->nomeEp);
    else
      snprintf(dst, tam, "T%d E%d", it->temporada, it->episodio);
    return;
  }
  if (it->dataUlt[0]) {
    char q[64];
    agenda_quando(it->dataUlt, q, sizeof q);
    if (q[0]) snprintf(dst, tam, i18n("Último episódio em %s"), q);
    return;
  }
  // NENHUMA DATA, NEM A DO ULTIMO, depois de todas as fontes (pedido do dono,
  // 22/09/2026: "avisar na agenda tambem para que o usuario saiba"). A linha
  // vazia deixava o cartao so com o nome e parecia defeito de carga; esta frase
  // diz o que e verdade — nenhuma fonte confirmou dia. Enquanto o fio ainda
  // busca a frase fica de fora: ali "sem data" seria so "ainda nao chegou".
  // Situacao conhecida (encerrada, cancelada) ja esta escrita na estacao.
  if (it->situacao == AG_DESCONHECIDA && !agenda_atualizando())
    snprintf(dst, tam, "%s", i18n("Sem data confirmada"));
}

// A ARTE DE UMA LINHA, em "cover" no retangulo: a de paisagem (still/backdrop)
// quando ha; senao o CARTAZ inteiro, nitido e centrado, sobre a copia desfocada
// dele mesmo — cortar um cartaz em pe para 16:9 mostraria so o meio do rosto.
// Sem nada carregado, o retangulo neutro (e o que fica por baixo do esvanecer).
static void arteItem(GfxRect r, const AgItem *it, float raioPx, float a) {
  float raio = raioPx / r.h;
  gfx_cor(r, raio, 0.16f, 0.17f, 0.19f, a);
  if (!it || a <= 0.001f) return;
  if (it->fundo[0]) {
    GLuint t = tex_obter_larg(it->fundo, r.w);
    float asp = tex_aspecto(it->fundo);
    if (t && asp > 0.0f) {
      gfx_tex_aspect_atual = asp;
      gfx_rect(r, t, GFX_CARD, 0, 0, 0, raio, 0, 0, 0, a);
      gfx_tex_aspect_atual = 0.0f;
      return;
    }
  }
  if (it->poster[0]) {
    float ch = r.h - 2.0f * P(r.h > P(200) ? 24.0f : 8.0f);
    GLuint t = tex_obter_larg(it->poster, ch * 2.0f / 3.0f);
    float asp = tex_aspecto(it->poster);
    if (t && asp > 0.0f) {
      GLuint d = gfx_desfocado(t, it->poster);
      GfxRect cz = { 0, 0, ch * asp, ch };
      if (d) {
        gfx_tex_aspect_atual = asp;
        gfx_rect(r, d, GFX_CARD, 0, 0, 0, raio, 0, 0, 0, a);
        gfx_tex_aspect_atual = 0.0f;
        gfx_cor(r, raio, 0, 0, 0, 0.35f * a);
      }
      if (cz.w > r.w - P(16)) { cz.w = r.w - P(16); cz.h = cz.w / asp; }
      cz.x = r.x + (r.w - cz.w) * 0.5f; cz.y = r.y + (r.h - cz.h) * 0.5f;
      gfx_tex_aspect_atual = asp;
      gfx_rect(cz, t, GFX_CARD, 0, 0, 0, P(raioPx > 16.0f ? 14.0f : 8.0f) / cz.h, 0, 0, 0, a);
      gfx_tex_aspect_atual = 0.0f;
    }
  }
}

// O SELO de marco: "Final da temporada" na arte (pilula escura), "Final" /
// "Estreia" na linha (pilula no acento a 16%). Devolve a largura.
static float seloC1(const char *s, float x, float y, int escuro, float a, int desenhar) {
  float w, h, padX = P(10), padY = P(4);
  int r, g, b;
  if (!s || !s[0]) return 0.0f;
  if (escuro) { r = g = b = 255; } else acentoClaro(0.5f, &r, &g, &b);
  w = txC1(TXT_AJ_CAPS13, 14, s, r, g, b, -1.0f, 0, 15.0f, P(600), 1.0f) + 2.0f * padX;
  h = hC1(TXT_AJ_CAPS13, 14, 15.0f, r) + 2.0f * padY;
  if (desenhar && a > 0.001f) {
    GfxRect pr = { x, y, w, h };
    if (escuro) gfx_cor(pr, 0.5f, 0, 0, 0, 0.55f * a);
    else {
      float ar, ag, ab;
      ajustes_acento(&ar, &ag, &ab);
      gfx_cor(pr, 0.5f, ar, ag, ab, 0.16f * a);
    }
    txC1(TXT_AJ_CAPS13, 14, s, r, g, b, x + padX, y + padY, 15.0f, P(600), a);
  }
  return w;
}

// O CHIP do mockup (.chip / .chip.on): pilula de 22/600; `on` = branco cheio
// com texto escuro. O sino Lucide (aj_bell) entra quando `sino`.
static float chipC1(const char *s, float x, float y, int on, int sino, float a) {
  float padX = P(22), padY = P(12), ic = sino ? P(23) + P(10) : 0.0f;
  int c = on ? 17 : 244;
  float tw = txC1(TXT_AJ_SEG, 20, s, c, c, c, -1.0f, 0, 22.0f, P(400), 1.0f);
  float th = hC1(TXT_AJ_SEG, 20, 22.0f, c);
  GfxRect r = { x, y, padX * 2.0f + ic + tw, padY * 2.0f + th };
  if (on) gfx_cor(r, 0.5f, 1, 1, 1, a);
  else if (ajustes_vidro()) gfx_cor(r, 0.5f, 1, 1, 1, 0.07f * a);
  else gfx_cor(r, 0.5f, .141f, .149f, .173f, a);
  if (sino)
    gfx_icone((GfxRect){ x + padX, y + (r.h - P(23)) * 0.5f, P(23), P(23) }, "aj_bell",
              c / 255.0f, c / 255.0f, c / 255.0f, a);
  txC1(TXT_AJ_SEG, 20, s, c, c, c, x + padX + ic, y + padY, 22.0f, P(400), a);
  return r.w;
}

static int temChip(const AgItem *it) {
  return it->lembrete || agenda_pode_lembrar(it->imdb);
}

// O AVISO DE FONTE, so quando o TMDB esta fora E falta dado na lista. Sem o
// TMDB o fio cai para o Trakt e o Cinemeta (agenda.c, "AS TRES FONTES"), que
// nao tem rede nem marco de temporada e as vezes nem data — e o dono pediu
// que a tela dissesse isso. Duas frases porque sao duas causas com saidas
// diferentes: ajuste desligado tem conserto em Ajustes; pacote sem chave
// nenhuma nao tem, e mandar a pessoa a um ajuste que nao resolve seria mentir.
static const char *avisoFonte(void) {
  const char *chave = desc_chave_tmdb_reserva();   /* ver fioAgenda em agenda.c */
  int i, n = agenda_n(), falta = 0;
  if (chave && chave[0]) return NULL;
  for (i = 0; i < n && !falta; i++) {
    const AgItem *it = agenda_lista(i);
    if (it && (!it->poster[0] || !it->titulo[0] ||
               (!it->dataProx[0] && !it->dataUlt[0] && it->situacao == AG_DESCONHECIDA)))
      falta = 1;
  }
  if (!falta) return NULL;
  { const char *res = desc_chave_tmdb_reserva();
    return (res && res[0])
      ? i18n("TMDB desligado: datas e cartazes vêm do Cinemeta e podem faltar em algumas séries · Ajustes › Integrações › TMDB")
      : i18n("Sem chave do TMDB: datas e cartazes vêm do Cinemeta e podem faltar em algumas séries"); }
}

// A ESQUERDA: a arte grande do item em foco (com o esvanecer da anterior), o
// selo, e embaixo o quando, o titulo, o episodio, a sinopse e os chips.
static void desenhaEsquerda(const AgC1 *L, float a) {
  const AgItem *it = arteCur.imdb[0] ? &arteCur : NULL;
  GfxRect art = { L->x0, L->artY, L->artW, L->artH };
  float t = arteT * arteT * (3.0f - 2.0f * arteT);
  float y = L->infoY, w = L->artW;
  const char *aviso = avisoFonte();
  char buf[400], marco[64];
  int maxSin;

  if (t < 1.0f && arteAnt.imdb[0]) arteItem(art, &arteAnt, 24.0f, a);
  arteItem(art, it, 24.0f, a * (arteAnt.imdb[0] ? t : 1.0f));
  if (!it) return;
  a *= 0.25f + 0.75f * t;

  // O selo: o marco do episodio; sem data, a situacao da serie.
  marco[0] = 0;
  if (!agenda_marco(it, marco, sizeof marco) && !temData(it) && situacaoTxt(it))
    snprintf(marco, sizeof marco, "%s", situacaoTxt(it));
  if (marco[0]) seloC1(marco, art.x + P(26), art.y + P(26), 1, a, 1);

  // O QUANDO, no acento clareado e em caixa alta: "QUA 16 · HOJE",
  // "QUA 4/11 · EM 7 SEMANAS". Sem data: "SEM DATA PREVISTA".
  { char cru[160], dl[48], falta[64];
    int kr, kg, kb;
    acentoClaro(0.45f, &kr, &kg, &kb);
    cru[0] = 0;
    if (temData(it)) {
      char sem[24];
      const char *hj = agenda_hoje();
      int d = agenda_dia(it->dataProx), m = agenda_mes(it->dataProx);
      snprintf(sem, sizeof sem, "%s", i18n(agenda_semana_nome(agenda_semana(it->dataProx))));
      if (hj && (agenda_mes(hj) != m || agenda_ano(hj) != agenda_ano(it->dataProx)))
        snprintf(dl, sizeof dl, "%s %d/%d", sem, d, m);
      else snprintf(dl, sizeof dl, "%s %d", sem, d);
      agenda_falta(it->dataProx, falta, sizeof falta);
      snprintf(cru, sizeof cru, falta[0] ? "%s \xc2\xb7 %s" : "%s", dl, falta);
    } else {
      // A situacao ja esta no selo da arte: aqui vai o que ela significa.
      snprintf(cru, sizeof cru, "%s", i18n("Sem data prevista"));
    }
    maiusc(buf, sizeof buf, cru);
    txt_tracking(TXT_AJ_CAPS13, buf, kr, kg, kb, L->x0, y, 0.8f * a, 1.8f);
    y += altLinha(TXT_AJ_CAPS13, kr) + P(10); }

  // O titulo, 46/700.
  txC1(TXT_ILHA_TITULO, 40, it->titulo[0] ? it->titulo : i18n("Série"), AG_INK,
       L->x0, y, 46.0f, w, a);
  y += hC1(TXT_ILHA_TITULO, 40, 46.0f, 244) + P(8);

  // "T3 E9 · The Last Empress · Apple TV+ · 58 min" (o marco ja esta no selo).
  { char ep[220], apoio[240];
    linhaEpisodio(it, ep, sizeof ep);
    agenda_apoio(it, apoio, sizeof apoio);
    if (ep[0] && apoio[0]) snprintf(buf, sizeof buf, "%s \xc2\xb7 %s", ep, apoio);
    else snprintf(buf, sizeof buf, "%s", ep[0] ? ep : apoio);
    if (buf[0]) {
      txC1(TXT_V2_24, 24, buf, AG_INK2, L->x0, y, 24.0f, w, a);
      y += hC1(TXT_V2_24, 24, 24.0f, 179) + P(12);
    } }

  // A sinopse (21, ate 3 linhas) e, quando ha, a ultima manchete como citacao
  // com a dica de abrir — o que o cartao focado dizia antes. OK abre o modal
  // da linha, e "Ultimas noticias" mora nele.
  { const Noticia *nt = noticias_item(it->imdb, 0);
    int temNot = nt && nt->titulo[0];
    maxSin = temNot ? 2 : 3;
    if (aviso) maxSin--;
    if (it->sinopse[0]) {
      float lead = sinopse_entrelinha(TXT_AJ_ESTADO, 30);
      float px = sinopse_entrelinha(TXT_AJ_ESTADO, 21);
      int max = sinopse_linhas(TXT_AJ_ESTADO, 30, maxSin);
      int k = blocoC1(TXT_AJ_ESTADO, 18, it->sinopse, L->x0, y, px, w, lead, max, AG_INK2, a);
      y += P(lead) * (float)k + P(6);
    }
    if (temNot) {
      const char *dica = i18n("OK para ler mais");
      float dw = txC1(TXT_ILHA_APOIO, 16, dica, AG_INK3, -1.0f, 0, 18.0f, w, 1.0f);
      float qw;
      snprintf(buf, sizeof buf, "\xe2\x80\x9c%s\xe2\x80\x9d", nt->titulo);
      qw = txC1(TXT_AJ_ESTADO, 18, buf, 205, 208, 214, L->x0, y, 21.0f, w - dw - P(14), a);
      txC1(TXT_ILHA_APOIO, 16, dica, AG_INK3, L->x0 + qw + P(14), y + P(3), 18.0f, dw + 1.0f, a);
      y += P(30) + P(6);
    } }

  // Os chips: o estado do lembrete e "Ver serie". Sao ESTADO, nao foco — o
  // D-pad mora na lista; OK abre o modal, onde ligar/desligar e abrir o titulo
  // continuam sendo acoes (mesmo caminho de antes).
  y += P(10);
  { float x = L->x0;
    if (temChip(it))
      x += chipC1(it->lembrete ? i18n("Lembrete ativo") : i18n("Lembrar-me"), x, y,
                  it->lembrete, 1, a) + P(12);
    chipC1(i18n("Ver série"), x, y, 0, 0, a); }

  if (aviso) {
    // Ambar apagado e nao o acento: e informacao, nao erro. No pe da coluna.
    float lh = P(23);
    int nl = agendaui_sinopse_linhas(TXT_ILHA_HORA, aviso, w / escDe(16.0f, 15), 2, 214, 178, 110);
    if (nl < 1) nl = 1;
    blocoC1(TXT_ILHA_HORA, 15, aviso, L->x0, NV_TELA_H - P(44) - lh * (float)nl,
            16.0f, w, 23.0f, 2, 214, 178, 110, a);
  }
}

// O SEGMENTADO Lista/Mes do mockup (.seg): trilho de vidro, o item ATIVO em
// branco cheio com texto escuro. Com o foco no cabecalho, o item FOCADO e o
// branco; o ativo nao focado fica no degrau claro com o texto no acento — o
// estado continua legivel sem anel de foco.
static float segC1(float xDir, float yc, int desenhar) {
  const char *rot[2] = { i18n("Lista"), i18n("Mês") };
  int id[2] = { AG_CAB_LISTA, AG_CAB_MES };
  int telefone = agColunaUnica();
  float corpo = telefone ? agTelefonePx(32) : 21;
  TxtEstilo estilo = telefone ? TXT_G30M : TXT_AJ_SEG;
  float fonte = telefone ? 30 : 20;
  float padX = P(telefone ? agTelefonePx(24) : 16), padY = P(telefone ? agTelefonePx(16) : 6);
  float pad = P(4), gap = P(telefone ? agTelefonePx(8) : 4), w[2], h, total, x;
  int k, ar, ag, ab;
  acentoInt(&ar, &ag, &ab);
  h = hC1(estilo, fonte, corpo, 17) + 2.0f * padY;
  if (telefone && h < P(agTelefonePx(120))) h = P(agTelefonePx(120));
  for (k = 0; k < 2; k++) {
    w[k] = txC1(estilo, fonte, rot[k], 17, 17, 17, -1.0f, 0, corpo, P(300), 1.0f) + 2.0f * padX;
    // Um pixel da fonte evita que o arredondamento da escala corte o rotulo.
    if (telefone) w[k] += escDe(corpo, fonte);
  }
  total = pad * 2.0f + w[0] + gap + w[1];
  if (!desenhar) return total;
  { GfxRect tr = { xDir - total, yc - h * 0.5f - pad, total, h + pad * 2.0f };
    if (ajustes_vidro()) gfx_vidro_folha(tr, 0.5f, 1.0f);
    else gfx_cor(tr, 0.5f, .141f, .149f, .173f, 1.0f);
    x = tr.x + pad; }
  for (k = 0; k < 2; k++) {
    int ativo = (id[k] == AG_CAB_MES) == (vistaMes != 0);
    int foc = focoCabecalho == id[k];
    int branco = focoCabecalho ? foc : ativo;
    GfxRect ir = { x, yc - h * 0.5f, w[k], h };
    if (!ctxAberto) ponteiro_alvo(ir.x, ir.y, ir.w, ir.h, ponteiroCab, NULL, id[k], 0);
    if (branco) gfx_cor(ir, 0.5f, 1, 1, 1, 1.0f);
    else if (ativo) gfx_cor(ir, 0.5f, 1, 1, 1, 0.12f);
    if (branco)
      txC1(estilo, fonte, rot[k], 17, 17, 17, x + padX,
           telefone ? ir.y + (ir.h - hC1(estilo, fonte, corpo, 17)) * .5f : ir.y + padY, corpo, telefone ? w[k] - padX * 2 : P(300), 1.0f);
    else if (ativo)
      txC1(estilo, fonte, rot[k], ar, ag, ab, x + padX,
           telefone ? ir.y + (ir.h - hC1(estilo, fonte, corpo, ar)) * .5f : ir.y + padY, corpo, telefone ? w[k] - padX * 2 : P(300), 1.0f);
    else
      txC1(telefone ? TXT_G30M : TXT_AJ_SUB, fonte, rot[k], AG_INK2, x + padX,
           telefone ? ir.y + (ir.h - hC1(estilo, fonte, corpo, 179)) * .5f : ir.y + padY, corpo, telefone ? w[k] - padX * 2 : P(300), 1.0f);
    x += w[k] + gap;
  }
  return total;
}

// A ILHA DA DIREITA: o material das ilhas (salvospainel.c, painelFlutuante):
// sombra curta, miolo de vidro (gfx_vidro_folha) ou solido e a luz larga e
// fraca no canto de cima. Sem aro.
static void ilhaC1(GfxRect p) {
  const int vid = ajustes_vidro();
  float raio = P(36) / p.h;
  gfx_sombra_sob((GfxRect){ p.x - 18.0f, p.y - 8.0f, p.w + 36.0f, p.h + 40.0f }, 1.0f, 0, 0.5f,
                 0, 0, 0, .38f, p, raio * p.h, vid ? 0.0f : .98f);
  if (vid) gfx_vidro_folha(p, raio, 1.0f);
  else gfx_cor(p, raio, .071f, .075f, .086f, .98f);
  if (!vid) gfx_luz_canto(p, raio, p.w * .25f, -p.h * .25f, p.w * .9f, 1, 1, 1, .04f);
}

// O cabecalho da ilha: a contagem a esquerda e o seletor a direita.
static void seletorAgendaTelefone(float x, float xDir, float y);
static void cabecalhoIlha(const AgC1 *L) {
  char sub[200];
  int i, n = agenda_n(), comData = 0;
  float yc = L->pnY + P(22) + P(22);
  float segW = 0;
  if (!agColunaUnica()) segW = segC1(L->pnX + L->pnW - P(22), yc, 1);
  for (i = 0; i < n; i++) if (temData(agenda_lista(i))) comData++;
  if (n == 0)
    snprintf(sub, sizeof sub, "%s", i18n("Salve uma série ou comece a assistir para ela aparecer aqui"));
  else if (comData == 1)
    snprintf(sub, sizeof sub, i18n("%d série com data confirmada · %d acompanhadas"), comData, n);
  else
    snprintf(sub, sizeof sub, i18n("%d séries com data confirmada · %d acompanhadas"), comData, n);
  if (agColunaUnica()) {
    blocoC1(TXT_G28R, 28, sub, L->pnX + P(22), L->pnY + P(agTelefonePx(24)),
            agTelefonePx(28), L->pnW - P(44), agTelefonePx(40), 2, AG_INK2, 1);
    seletorAgendaTelefone(L->pnX + P(22), L->pnX + L->pnW - P(22), L->pnY + P(agTelefonePx(120)));
    return;
  }
  txC1(TXT_AJ_ESTADO, 18, sub, AG_INK2, L->pnX + P(22),
       yc - hC1(TXT_AJ_ESTADO, 18, 20.0f, 179) * 0.5f, 20.0f,
       L->pnW - P(44) - segW - P(20), 1.0f);
}

// O FIO DO TEMPO: 3 px a 18 do inicio da lista, no acento no topo e
// esvanecendo para o neutro (branco a 12%) — o degrade do mockup em faixas.
// Segue o DOCUMENTO: o acento e o comeco da agenda (hoje), nao o topo da ilha.
static void fioC1(const AgC1 *L, float y0, float y1) {
  float ar, ag, ab, x = L->lsX + P(18) - P(1.5f), passo = P(8), y, gr;
  ajustes_acento(&ar, &ag, &ab);
  gr = (y1 - y0) * 0.4f;
  if (gr > P(420)) gr = P(420);
  if (gr < 1.0f) gr = 1.0f;
  for (y = y0; y < y1; y += passo) {
    float h = y + passo > y1 ? y1 - y : passo;
    float t = (y - y0) / gr;
    if (y + h < L->lsY || y > L->lsY + L->lsH) continue;
    if (t > 1.0f) t = 1.0f;
    gfx_cor((GfxRect){ x, y, P(3), h }, 0.0f,
            ar + (1.0f - ar) * t, ag + (1.0f - ag) * t, ab + (1.0f - ab) * t,
            1.0f - 0.88f * t);
  }
}

// O PONTO de cada linha no fio: 14 px, miolo #2a2d33 e aro de 3 a 35%; o da
// linha em foco acende (miolo branco, aro no acento, halo de 6 a 30%).
static void pontoC1(float cx, float cy, float f) {
  float ar, ag, ab, d = P(14), aro = P(20);
  ajustes_acento(&ar, &ag, &ab);
  if (f > 0.01f) {
    float hd = aro + P(12);
    gfx_cor((GfxRect){ cx - hd * 0.5f, cy - hd * 0.5f, hd, hd }, 0.5f, ar, ag, ab, 0.30f * f);
  }
  gfx_cor((GfxRect){ cx - aro * 0.5f, cy - aro * 0.5f, aro, aro }, 0.5f, 1, 1, 1, 0.35f * (1.0f - f));
  gfx_cor((GfxRect){ cx - aro * 0.5f, cy - aro * 0.5f, aro, aro }, 0.5f, ar, ag, ab, f);
  gfx_cor((GfxRect){ cx - d * 0.5f, cy - d * 0.5f, d, d }, 0.5f,
          .165f + .835f * f, .176f + .824f * f, .2f + .8f * f, 1.0f);
}

// O cabecalho de grupo (.grp): caps espacadas a 17 no cinza claro, fio de 1 px
// embaixo — a mesma voz dos cabecalhos de secao dos Ajustes.
static void grupoC1(const AgC1 *L, int g, int qtd, float y) {
  char cru[80], rot[120];
  const char *nome = g == 0 ? i18n("Hoje") : g == 1 ? i18n("Esta semana")
                   : g == 2 ? i18n("Mais tarde") : i18n("Sem data prevista");
  float ty = y + P(18);
  if (g == 3) snprintf(cru, sizeof cru, "%s \xc2\xb7 %d", nome, qtd);
  else snprintf(cru, sizeof cru, "%s", nome);
  maiusc(rot, sizeof rot, cru);
  if (agColunaUnica())
    txC1(TXT_G28B, 28, rot, AG_INK2, L->lsX + P(46), y + P(agTelefonePx(18)),
         agTelefonePx(28), L->lsW - P(46), 1);
  else txt_tracking(TXT_AJ_CAPS13, rot, AG_INK2, L->lsX + P(46), ty, 1.0f, 1.6f);
  gfx_cor((GfxRect){ L->lsX, y + agGrupoH() - P(7), L->lsW, P(1) }, 0, 1, 1, 1, 0.08f);
}

static void metaTelefone(const AgItem *it, char *meta, size_t tam) {
  char dl[48], ep[220];
  const char *selo = !strcmp(it->tipoEp, "premiere") ? i18n("Estreia") :
                     !strcmp(it->tipoEp, "finale") ? i18n("Final") : "";
  if (temData(it)) {
    size_t u;
    dataLinha(it, dl, sizeof dl);
    u = (size_t)snprintf(meta, tam, "%s", dl);
    if (it->temporada > 0 && it->episodio > 0 && u < tam)
      u += (size_t)snprintf(meta + u, tam - u, " \xc2\xb7 T%d E%d", it->temporada, it->episodio);
    if (selo[0] && u < tam) u += (size_t)snprintf(meta + u, tam - u, " \xc2\xb7 %s", selo);
    if (it->rede[0] && u < tam) snprintf(meta + u, tam - u, " \xc2\xb7 %s", it->rede);
  } else {
    const char *sit = situacaoTxt(it);
    linhaEpisodio(it, ep, sizeof ep);
    snprintf(meta, tam, "%s%s%s", sit ? sit : "", sit && ep[0] ? " \xc2\xb7 " : "",
             ep[0] ? ep : sit ? "" : i18n("Sem data"));
  }
}

// UMA LINHA (.ar): miniatura 170x96, nome (+ selo Estreia/Final), meta e o sino
// a direita — cheio e claro com lembrete, de traco e apagado sem. A linha em
// foco ganha a superficie do mockup (acento a 14% sobre branco a 12%). As sem
// data ficam a 55% fora do foco.
static void linhaC1(const AgC1 *L, const AgItem *it, float y, float f) {
  float x = L->lsX + P(46), w = L->lsW - P(46);
  float tx = x + P(14) + P(170) + P(18), sinoW = P(26), tw;
  float apaga = grupoDe(it) == 3 ? 0.55f + 0.45f * f : 1.0f;
  char meta[300], dl[48], selo[32];
  GfxRect r = { x, y, w, P(AG_ROW_H) };
  if (agColunaUnica()) {
    float pad = P(agTelefonePx(14)), artW = P(agTelefonePx(200)), artH = P(agTelefonePx(112));
    float corpo = agTelefonePx(42), apoio = agTelefonePx(34), lead = agTelefonePx(44);
    float nh = hC1(TXT_TITULO3, 48, corpo, 244), mh = P(lead * 2);
    float textY;
    r.h = agLinhaH();
    sinoW = P(agTelefonePx(32));
    tx = x + pad + artW + P(agTelefonePx(20));
    tw = x + w - pad - sinoW - P(agTelefonePx(18)) - tx;
    if (f > .01f) {
      float ar, ag, ab;
      ajustes_acento(&ar, &ag, &ab);
      gfx_cor(r, P(18) / r.h, .14f * ar + .86f, .14f * ag + .86f, .14f * ab + .86f, .243f * f);
    }
    arteItem((GfxRect){x + pad, y + (r.h - artH) * .5f, artW, artH}, it, 16, apaga);
    if (temChip(it)) gfx_icone((GfxRect){x + w - pad - sinoW, y + (r.h - sinoW) * .5f, sinoW, sinoW},
                              it->lembrete ? "sino" : "aj_bell", .88f, .9f, .94f, apaga);
    textY = y + (r.h - nh - P(agTelefonePx(8)) - mh) * .5f;
    txC1(TXT_TITULO3, 48, it->titulo[0] ? it->titulo : i18n("Série"), AG_INK,
         tx, textY, corpo, tw, apaga);
    metaTelefone(it, meta, sizeof meta);
    blocoC1(TXT_G28R, 28, meta, tx, textY + nh + P(agTelefonePx(8)), apoio, tw,
            lead, 2, AG_INK2, apaga);
    return;
  }
  if (f > 0.01f) {
    float ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    gfx_cor(r, P(18) / r.h, .14f * ar + .86f, .14f * ag + .86f, .14f * ab + .86f, .243f * f);
  }
  arteItem((GfxRect){ x + P(14), y + P(10), P(170), P(96) }, it, 16.0f, apaga);

  // O sino: so em quem pode ter lembrete.
  if (temChip(it)) {
    GfxRect ic = { x + w - P(14) - sinoW, y + (r.h - sinoW) * 0.5f, sinoW, sinoW };
    if (it->lembrete) gfx_icone(ic, "sino", 244 / 255.0f, 245 / 255.0f, 247 / 255.0f, apaga);
    else gfx_icone(ic, "aj_bell", 179 / 255.0f, 183 / 255.0f, 191 / 255.0f, 0.55f * apaga);
  }
  tw = x + w - P(14) - sinoW - P(18) - tx;

  // Nome + selo curto.
  selo[0] = 0;
  if (!strcmp(it->tipoEp, "premiere")) snprintf(selo, sizeof selo, "%s", i18n("Estreia"));
  else if (!strcmp(it->tipoEp, "finale")) snprintf(selo, sizeof selo, "%s", i18n("Final"));
  { float sw = selo[0] ? seloC1(selo, 0, 0, 0, 1.0f, 0) : 0.0f;
    float nh = hC1(TXT_ILHA_NOME, 24, 24.0f, 244), mh = hC1(TXT_ILHA_APOIO, 16, 18.0f, 179);
    float ny = y + (r.h - nh - P(2) - mh) * 0.5f, nw;
    nw = txC1(TXT_ILHA_NOME, 24, it->titulo[0] ? it->titulo : i18n("Série"), AG_INK,
              tx, ny, 24.0f, sw > 0.0f ? tw - sw - P(10) : tw, apaga);
    if (sw > 0.0f)
      seloC1(selo, tx + nw + P(10), ny + (nh - hC1(TXT_AJ_CAPS13, 14, 15.0f, 255) - P(8)) * 0.5f,
             0, apaga, 1);

    // A meta: "Qui 17 · T2 E4 · HBO"; mais tarde leva "em 7 semanas"; sem
    // data, a situacao e o ultimo episodio.
    meta[0] = 0;
    if (temData(it)) {
      size_t u;
      dataLinha(it, dl, sizeof dl);
      u = (size_t)snprintf(meta, sizeof meta, "%s", dl);
      if (grupoDe(it) == 2) {
        char fal[64];
        agenda_falta(it->dataProx, fal, sizeof fal);
        if (fal[0] && u < sizeof meta) u += (size_t)snprintf(meta + u, sizeof meta - u, " \xc2\xb7 %s", fal);
      }
      if (it->temporada > 0 && it->episodio > 0 && u < sizeof meta)
        u += (size_t)snprintf(meta + u, sizeof meta - u, " \xc2\xb7 T%d E%d", it->temporada, it->episodio);
      if (it->rede[0] && u < sizeof meta)
        snprintf(meta + u, sizeof meta - u, " \xc2\xb7 %s", it->rede);
    } else {
      char ep[220];
      const char *sit = situacaoTxt(it);
      linhaEpisodio(it, ep, sizeof ep);
      if (sit && ep[0]) snprintf(meta, sizeof meta, "%s \xc2\xb7 %s", sit, ep);
      else snprintf(meta, sizeof meta, "%s", sit ? sit : ep[0] ? ep : i18n("Sem data"));
    }
    txC1(TXT_ILHA_APOIO, 16, meta, AG_INK2, tx, ny + nh + P(2), 18.0f, tw, apaga); }
}

// A LISTA inteira dentro da ilha, rolada e recortada nela.
static void desenhaLista(const AgC1 *L) {
  int n = agenda_n(), i, semData = 0;
  float top;
#ifdef NV_TOUCH_UI
  if (agColunaUnica()) {
    toquerol_vincular(&toqueAgenda, (GfxRect){L->pnX, L->lsY, L->pnW, L->lsH}, gfx_escala(),
                     0.0f, alturaDoc() - L->lsH, 1, &scrollY);
    ponteiro_rolagem(toqueAgendaRolar);
  }
#endif
  top = L->lsY - scrollY;
  for (i = 0; i < n; i++) if (grupo(i) == 3) semData++;
  gfx_recorte(L->pnX, L->lsY, L->pnW, L->lsH);
  if (n > 0) fioC1(L, top + P(10), top + alturaDoc() - P(10));
  for (i = 0; i < n && i < AG_MAX; i++) {
    const AgItem *it = agenda_lista(i);
    float y = top + yDe(i);
    if (!it) continue;
    if (abreGrupo(i)) {
      float gy = y - agGrupoH() - agLinhaGap();
      if (gy < L->lsY + L->lsH && gy + agGrupoH() > L->lsY)
        grupoC1(L, grupo(i), semData, gy);
    }
    if (y > L->lsY + L->lsH || y + agLinhaH() < L->lsY) continue;
    if (!ctxAberto)
      ponteiro_alvo_faixa(L->lsX + P(46), y, L->lsW - P(46), agLinhaH(), L->lsY, L->lsY + L->lsH,
                          ponteiroLinha, NULL, i, 0);
    linhaC1(L, it, y, animFoco[i]);
    pontoC1(L->lsX + P(18), y + agLinhaH() * 0.5f, animFoco[i]);
  }
  gfx_sem_recorte();
}


// --- os paineis flutuantes: o modal da linha, as manchetes, a noticia ------------
//
// NA LINGUAGEM DOS PAINEIS FLUTUANTES (menu.c, 21/09/2026): superficie
// 0.055/0.058/0.068 quase opaca com canto de 28, a luz na cor de realce
// entrando pelo topo (gfx_luz_canto, recortada pelo proprio canto) e a linha
// em foco como PILULA de realce com a mancha difusa atras (GFX_SOMBRA 0,35).
//
// COM A INTERFACE DE VIDRO (ajustes_vidro), o MESMO desenho do menu com vidro:
// o painel e gfx_vidro_painel (translucido, fio de 1,5 px) e a linha em foco e
// a pilula cheia de gfx_vidro_pilula_cheia — o realce continua sendo o do tema,
// e a tinta vem de gfx_vidro_tinta. Nenhuma superficie ganha degrade.
#define AGC_W      1320.0f
#define AGC_PAD      44.0f
#define AGC_ACAO_W  500.0f
#define AGC_LINHA    76.0f
#define AGC_HIST_L   60.0f
#define AGN_W      1040.0f
#define AGN_LINHA    96.0f
#define AGN_FOCO    236.0f
#define AGL_W      1200.0f
#define AGL_H       700.0f
// Sem texto (o fallback do QR), o painel encolhe para o que tem: manchete de
// ate tres linhas, a frase e o QR. Com 900 fixos sobrava meia tela vazia
// abaixo do QR (captura -fx-noticia-qr, 29/09/2026).
#define AGL_H_FALHA 560.0f
#define AGL_IMG_W   360.0f
#define AGL_QR      124.0f

// GLASS UI: os paineis flutuantes (o modal da linha, as manchetes, a noticia)
// sao ILHAS — sombra curta, miolo de vidro ou solido, luz larga e fraca
// BRANCA no canto de cima (era a luz na cor do realce), sem fio. No vidro
// continua o apoio escuro por baixo: atras ha TEXTO (as linhas da Agenda), e
// texto atras de texto le como sujeira (captura -fx-modal-vidro, 29/09/2026).
static void painelFlutuante(GfxRect r, float ar, float ag, float ab, float a) {
  const int vid = ajustes_vidro();
  float raio = 36.0f / r.h;
  (void)ar; (void)ag; (void)ab;
  gfx_rect((GfxRect){ r.x - 18.0f, r.y - 8.0f, r.w + 36.0f, r.h + 40.0f }, 0, GFX_SOMBRA,
           1.0f, 0, 0, 0.5f, 0, 0, 0, .42f * a);
  if (vid) gfx_vidro_folha(r, raio, a);
  else gfx_cor(r, raio, .071f, .075f, .086f, .99f * a);
  if (!vid) gfx_luz_canto(r, raio, r.w * .25f, -r.h * .25f, r.w * .9f, 1, 1, 1, .04f * a);
}
// A LINHA EM FOCO dos paineis (acoes do modal, manchete aberta): superficie
// um degrau mais clara, como o menu e o Social — sem pilula no realce.
static void pilulaFoco(GfxRect lr, float ar, float ag, float ab, float a) {
  (void)ar; (void)ag; (void)ab;
  float rf = 0.5f > 22.0f / lr.h ? 22.0f / lr.h : 0.5f;
  if (gfx_foco_placa(lr, rf, 1.0f, a)) return;
  if (ajustes_vidro()) gfx_cor(lr, rf, 1, 1, 1, .12f * a);
  else gfx_cor(lr, rf, .17f, .176f, .204f, a);
}
// Tinta do texto sobre a linha em foco: clara, a superficie nao e realce.
static GfxRect agModalR(float w, float h, float a) {
  float margem = agColunaUnica() ? P(48) : NV_MARGEM_Y;
  if (agColunaUnica()) w = fminf(w, NV_TELA_W - 2 * margem);
  h = fminf(h, NV_TELA_H - 2 * margem);
  return (GfxRect){(NV_TELA_W - w) * .5f, (NV_TELA_H - h) * .5f + (1 - a) * 24, w, h};
}
static GfxRect agModalCorpo(GfxRect r) {
  float y = r.y + AGC_PAD + AG_CARTAZ_H + 44;
  return (GfxRect){r.x + AGC_PAD, y, r.w - 2 * AGC_PAD, r.y + r.h - AGC_PAD - y};
}
static int tintaF(void)  { return 255; }
static int tintaF2(void) { return 200; }

// "12 set" / "12 Sep" / "12 вер" de uma data ISO. "" quando nao e data.
static void dataCurtaIso(const char *iso, char *dst, size_t tam) {
  int d = agenda_dia(iso), m = agenda_mes(iso), a = agenda_ano(iso);
  dst[0] = 0;
  if (d < 1 || m < 1) return;
  // O ANO so quando nao e o de hoje, a mesma regra das datas de noticias.c.
  if (a && a != agenda_ano(agenda_hoje()))
    snprintf(dst, tam, "%d %s %d", d, idioma_mes_curto(ajustes_idioma(), m - 1), a);
  else
    snprintf(dst, tam, "%d %s", d, idioma_mes_curto(ajustes_idioma(), m - 1));
}

// Altura de uma pilha de texto medida, nao cravada: "Hg" da a caixa inteira.
static float hEstilo(TxtEstilo e) { return altLinha(e, 238); }

// Titulo e subtitulo de painel EMPILHADOS PELA ALTURA MEDIDA. O painel de
// manchetes punha o subtitulo em +58 fixos debaixo de um TXT_TITULO2 de ~68 de
// caixa: "Ultimas noticias" encostava no pe do titulo (captura -ctx-noticias,
// antes desta revisao). Devolve o y logo abaixo do subtitulo.
static float cabecalho(float x, float y, float w, const char *titulo,
                       const char *sub, float a) {
  TxtLinha t = txt_linha_corta(TXT_TITULO3, titulo, 246, 247, 250, 255, w);
  txt_desenhar_alpha(t, x, y, a);
  y += (float)t.h + 4.0f;
  if (sub && sub[0]) {
    TxtLinha s = txt_linha_corta(TXT_CAPTION, sub, 150, 153, 162, 255, w);
    txt_desenhar_alpha(s, x, y, a);
    y += (float)s.h;
  }
  return y;
}

static const char *rotuloAcao(const AgItem *it, const AgModal *m, int ac,
                              char *buf, size_t tam) {
  switch (ac) {
    case AC_ASSISTIR:
      snprintf(buf, tam, i18n("Assistir T%dE%d"), m->eps[m->proximo].temporada,
               m->eps[m->proximo].episodio);
      return buf;
    case AC_ABRIR:    return i18n("Abrir o título");
    case AC_NOTICIAS: return i18n("Últimas notícias");
    case AC_VISTOS:
      if (m->naoVistos == 1) {
        int i;
        for (i = 0; i < m->n; i++)
          if (m->eps[i].visto != 1) {
            snprintf(buf, tam, i18n("Marcar T%dE%d como assistido"),
                     m->eps[i].temporada, m->eps[i].episodio);
            return buf;
          }
      }
      snprintf(buf, tam, i18n("Marcar %d episódios como assistidos"), m->naoVistos);
      return buf;
    case AC_LEMBRETE:
      return it->lembrete ? i18n("Desligar o lembrete") : i18n("Lembrar-me");
  }
  return "";
}

// A COLUNA DO HISTORICO: cabecalho em caps espacadas (a voz dos rotulos de mes
// da linha do tempo), uma linha por episodio — codigo, nome, dia, estado — e
// os estados vazios ditos em uma frase, nunca uma lista em branco.
//
// O ESTADO NAO E SO COR: assistido leva o check e a palavra, nao assistido o
// ponto no realce e a palavra, desconhecido so a palavra, mais apagada. Quem
// nao distingue a cor le a forma, como no sino da linha.
static void desenhaHistorico(const AgItem *it, const AgModal *m, float x, float y,
                             float w, float a) {
  char rot[160], resumo[64];
  const char *desde = agenda_lembrete_desde(it->imdb);
  float ar, ag, ab;
  int i;
  ajustes_acento(&ar, &ag, &ab);
  if (it->lembrete && desde[0]) {
    char d[32];
    dataCurtaIso(desde, d, sizeof d);
    snprintf(rot, sizeof rot, i18n("Lançados desde o lembrete · %s"), d);
  } else snprintf(rot, sizeof rot, "%s", i18n("Últimos episódios"));
  { char caps[200];
    idioma_maiusc(caps, sizeof caps, rot);
    txt_tracking(TXT_CAPTION2, caps, 132, 134, 142, x, y, a, 2.0f); }
  // "2 de 3 assistidos" a direita do cabecalho, so quando o mapa sabe de todos.
  resumo[0] = 0;
  if (m->estado == AG_HIST_PRONTO && m->n > 0) {
    int vistos = 0, sabidos = 0;
    for (i = 0; i < m->n; i++) { if (m->eps[i].visto >= 0) sabidos++; if (m->eps[i].visto == 1) vistos++; }
    if (sabidos == m->n) snprintf(resumo, sizeof resumo, i18n("%d de %d assistidos"), vistos, m->n);
  }
  if (resumo[0]) {
    TxtLinha l = txt_linha(TXT_CAPTION2, resumo, 150, 153, 162, 255);
    txt_desenhar_alpha(l, x + w - (float)l.w, y, a);
  }
  y += hEstilo(TXT_CAPTION2) + 14.0f;
  regua(x, x + w, y, 0.62f, 0.12f * a);
  y += 10.0f;

  if (m->estado != AG_HIST_PRONTO) {
    const char *msg = m->estado == AG_HIST_FALHOU ? i18n("Não foi possível carregar os episódios.")
                                                  : i18n("Carregando episódios…");
    TxtLinha l = txt_linha_corta(TXT_CALLOUT, msg, 170, 174, 184, 255, w);
    txt_desenhar_alpha(l, x, y + (AGC_HIST_L - (float)l.h) * 0.5f, a);
    if (m->estado != AG_HIST_FALHOU) {
      // O ESQUELETO de tres linhas no lugar onde a lista vai entrar: diz o
      // formato do que esta vindo, e a lista entra inteira de uma vez.
      int k;
      for (k = 1; k <= 3; k++) {
        GfxRect sk = { x, y + AGC_HIST_L * (float)k + 20.0f, w * (k == 2 ? 0.62f : 0.78f), 18.0f };
        gfx_esqueleto(sk, 0.5f, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, a);
      }
    }
    return;
  }
  if (m->n == 0) {
    const char *msg = it->lembrete ? i18n("Nada foi lançado desde o lembrete.")
                                   : i18n("Nenhum episódio lançado ainda.");
    TxtLinha l = txt_linha_corta(TXT_CALLOUT, msg, 170, 174, 184, 255, w);
    txt_desenhar_alpha(l, x, y + (AGC_HIST_L - (float)l.h) * 0.5f, a);
    return;
  }
  // Colunas FIXAS em toda linha (codigo 92, estado 196, dia 104): e o que
  // alinha os dias numa reta e deixa o nome com a sobra, cortado com "…".
  // A coluna de ESTADO na largura do maior rotulo DO IDIOMA ATIVO: com 196
  // cravados, "Не просмотрено" (ru) saia "Не…" (captura -fx-ru-modal).
  { const float wCod = 80.0f, wDia = 90.0f;
    float wEst = 0.0f, wNome;
    { const char *R[3] = { i18n("Assistido"), i18n("Não assistido"), i18n("Desconhecido") };
      int k;
      for (k = 0; k < 3; k++) { float lw = (float)txt_largura(TXT_CAPTION2, R[k]); if (lw > wEst) wEst = lw; }
      wEst += 44.0f + 12.0f;
      if (wEst < 170.0f) wEst = 170.0f;
      if (wEst > w * 0.34f) wEst = w * 0.34f; }
    wNome = w - wCod - wEst - wDia - 24.0f;
    for (i = 0; i < m->n; i++) {
      const AgEp *e = &m->eps[i];
      float ly = y + AGC_HIST_L * (float)i, cy = ly + AGC_HIST_L * 0.5f;
      char cod[24], dia[32];
      const char *est;
      int c = e->visto == 1 ? 150 : 236;
      TxtLinha l;
      snprintf(cod, sizeof cod, i18n("T%dE%d"), e->temporada, e->episodio);
      l = txt_linha_corta(TXT_CAPTION2, cod, 132, 134, 142, 255, wCod - 8.0f);
      txt_desenhar_alpha(l, x, cy - (float)l.h * 0.5f, a);
      l = txt_linha_corta(TXT_CALLOUT, e->nome[0] ? e->nome : cod, c, c, c + 4 > 255 ? 255 : c + 4, 255, wNome);
      txt_desenhar_alpha(l, x + wCod, cy - (float)l.h * 0.5f, a);
      dataCurtaIso(e->data, dia, sizeof dia);
      l = txt_linha_corta(TXT_CAPTION2, dia, 150, 153, 162, 255, wDia);
      txt_desenhar_alpha(l, x + w - wEst - (float)l.w - 12.0f, cy - (float)l.h * 0.5f, a);
      { float xe = x + w - wEst + 12.0f;
        if (e->visto == 1) {
          GfxRect ic = { xe, cy - 11.0f, 22.0f, 22.0f };
          gfx_icone(ic, "check", 0.72f, 0.73f, 0.76f, a);
          est = i18n("Assistido");
          l = txt_linha_corta(TXT_CAPTION2, est, 170, 172, 180, 255, wEst - 44.0f);
          txt_desenhar_alpha(l, xe + 32.0f, cy - (float)l.h * 0.5f, a);
        } else if (e->visto == 0) {
          GfxRect pt = { xe + 5.0f, cy - 6.0f, 12.0f, 12.0f };
          gfx_cor(pt, 0.5f, ar, ag, ab, a);
          est = i18n("Não assistido");
          l = txt_linha_corta(TXT_CAPTION2, est, 238, 240, 244, 255, wEst - 44.0f);
          txt_desenhar_alpha(l, xe + 32.0f, cy - (float)l.h * 0.5f, a);
        } else {
          est = i18n("Desconhecido");
          // No piso de 150 (DESIGN.md): apagado, mas legivel a 3 m.
          l = txt_linha_corta(TXT_CAPTION2, est, 150, 152, 160, 255, wEst - 44.0f);
          txt_desenhar_alpha(l, xe + 32.0f, cy - (float)l.h * 0.5f, a);
        } }
    } }
}

static void desenhaModalSerie(const AgItem *it, float a) {
  float ar, ag, ab;
  AgModal m;
  float h, x, y, xTxt, wTxt, yCorpo, xHist, wHist;
  GfxRect r;
  int i, tf = tintaF();
  ajustes_acento(&ar, &ag, &ab);
  montaModal(it, &m);
  if (ctxFoco >= m.nAc) ctxFoco = m.nAc > 0 ? m.nAc - 1 : 0;
  // Altura: cabecalho do cartaz (153) + respiro + o MAIOR dos dois blocos de
  // baixo. Fixa pela lista cheia (AG_HIST_VIS linhas) e nao pelo que chegou,
  // para o painel nao crescer diante da pessoa quando o Cinemeta responde.
  int telefone = agColunaUnica();
  float hHist;
  { float hAc = (float)m.nAc * AGC_LINHA;
    hHist = hEstilo(TXT_CAPTION2) + 24.0f + AGC_HIST_L * (float)AG_HIST_VIS;
    h = AGC_PAD + AG_CARTAZ_H + 44.0f +
        (telefone ? hAc + 24 + hHist : hAc > hHist ? hAc : hHist) + AGC_PAD; }
  r = agModalR(AGC_W, h, a);
  painelFlutuante(r, ar, ag, ab, a);
  x = r.x + AGC_PAD; y = r.y + AGC_PAD;

  // --- cabecalho: cartaz, titulo, episodio, lembrete -------------------------
  // O cartaz na MESMA largura da linha (AG_CARTAZ_W): e a mesma textura, sem
  // segunda decodificacao (ver a medida de tex_obter_larg no topo).
  { GfxRect cz = { x, y, AG_CARTAZ_W, AG_CARTAZ_H };
    GLuint tex = it->poster[0] ? tex_obter_larg(it->poster, AG_CARTAZ_W) : 0;
    if (tex) {
      gfx_tex_aspect_atual = AG_CARTAZ_W / AG_CARTAZ_H;
      gfx_rect(cz, tex, GFX_CARD, 0, 0, 0, AG_CARTAZ_RAIO, 0, 0, 0, a);
      gfx_tex_aspect_atual = 0.0f;
    } else gfx_cor(cz, AG_CARTAZ_RAIO, 0.18f, 0.19f, 0.21f, a); }
  xTxt = x + AG_CARTAZ_W + 32.0f;
  wTxt = r.x + r.w - AGC_PAD - xTxt;
  { char ep[220], linha[300], falta[64];
    float yt = y + 6.0f;
    TxtLinha t = txt_linha_corta(TXT_TITULO3, it->titulo[0] ? it->titulo : i18n("Série"),
                                 246, 247, 250, 255, wTxt);
    txt_desenhar_alpha(t, xTxt, yt, a);
    yt += (float)t.h + 6.0f;
    linhaEpisodio(it, ep, sizeof ep);
    falta[0] = 0;
    if (temData(it)) agenda_falta(it->dataProx, falta, sizeof falta);
    if (ep[0] && falta[0]) snprintf(linha, sizeof linha, "%s \xc2\xb7 %s", ep, falta);
    else snprintf(linha, sizeof linha, "%s", ep[0] ? ep : falta);
    if (linha[0]) {
      TxtLinha l = txt_linha_corta(TXT_CALLOUT, linha, 206, 208, 214, 255, wTxt);
      txt_desenhar_alpha(l, xTxt, yt, a);
      yt += (float)l.h + 10.0f;
    }
    // O LEMBRETE em uma linha: o sino e a frase. "Sem lembrete" so quando da
    // para ligar — serie encerrada nao tem lembrete a oferecer, e a frase seria
    // ruido.
    if (it->lembrete || agenda_pode_lembrar(it->imdb)) {
      char lb[120];
      const char *desde = agenda_lembrete_desde(it->imdb);
      int c = it->lembrete ? 236 : 150;
      GfxRect ic = { xTxt, yt + 1.0f, 26.0f, 26.0f };
      TxtLinha l;
      if (it->lembrete && desde[0]) {
        char d[32]; dataCurtaIso(desde, d, sizeof d);
        snprintf(lb, sizeof lb, i18n("Lembrete ativo desde %s"), d);
      } else snprintf(lb, sizeof lb, "%s", it->lembrete ? i18n("Lembrete ativo") : i18n("Sem lembrete"));
      agendaui_despertador(ic, it->lembrete, (float)c / 255.0f, (float)c / 255.0f,
                           (float)c / 255.0f, a, relogio, trocaEm);
      l = txt_linha_corta(TXT_CAPTION, lb, c, c, c + 4 > 255 ? 255 : c + 4, 255, wTxt - 40.0f);
      txt_desenhar_alpha(l, xTxt + 38.0f, yt + 14.0f - (float)l.h * 0.5f, a);
    } }

  // --- corpo: acoes a esquerda, historico a direita -------------------------
  yCorpo = y + AG_CARTAZ_H + 44.0f;
#ifdef NV_TOUCH_UI
  GfxRect corpo = agModalCorpo(r);
  if (telefone) {
    if (!toquerol_livre(&toqueModal)) {
      float fy = ctxFoco * AGC_LINHA;
      if (fy < toqueModalOffset) toqueModalOffset = fy;
      if (fy + AGC_LINHA > toqueModalOffset + corpo.h) toqueModalOffset = fy + AGC_LINHA - corpo.h;
    }
    toquerol_vincular(&toqueModal, corpo, gfx_escala(), 0,
                     m.nAc * AGC_LINHA + 24 + hHist - corpo.h, 1, &toqueModalOffset);
    toqueModalOffset = toquerol_clamp(toqueModalOffset, 0, toqueModal.maximo);
    ponteiro_rolagem(toqueAgendaRolar);
    yCorpo -= toqueModalOffset;
    gfx_recorte(corpo.x, corpo.y, corpo.w, corpo.h);
  }
#endif
  for (i = 0; i < m.nAc; i++) {
    char buf[160];
    int f = (i == ctxFoco);
    GfxRect lr = { telefone ? x : x - 20.0f, yCorpo + (float)i * AGC_LINHA,
                   telefone ? r.w - 2 * AGC_PAD : AGC_ACAO_W, AGC_LINHA - 12.0f };
    const char *rot = rotuloAcao(it, &m, m.ac[i], buf, sizeof buf);
    TxtLinha t;
    if (a > 0.99f && ctxAberto == 1) {
#ifdef NV_TOUCH_UI
      if (telefone) ponteiro_alvo_faixa(lr.x, lr.y, lr.w, lr.h, corpo.y, corpo.y + corpo.h, ponteiroAcao, NULL, i, 0);
      else
#endif
        ponteiro_alvo(lr.x, lr.y, lr.w, lr.h, ponteiroAcao, NULL, i, 0);
    }
    if (f) pilulaFoco(lr, ar, ag, ab, a);
    t = txt_linha_corta(TXT_CALLOUT, rot, f ? tf : 238, f ? tf : 240, f ? tf : 244, 255,
                        lr.w - 48.0f);
    txt_desenhar_alpha(t, lr.x + 24.0f, lr.y + (lr.h - (float)t.h) * 0.5f, a);
  }
  xHist = telefone ? x : x + AGC_ACAO_W + 32.0f;
  wHist = r.x + r.w - AGC_PAD - xHist;
  desenhaHistorico(it, &m, xHist, telefone ? yCorpo + m.nAc * AGC_LINHA + 24 : yCorpo + 4, wHist, a);
#ifdef NV_TOUCH_UI
  if (telefone) gfx_sem_recorte();
#endif
}

// --- AS MANCHETES ------------------------------------------------------------------
//
// PEDIDO DO DONO (29/09/2026): "quando estamos focados, vamos melhorar o
// layout, deixar mais bonito, com o header e o texto". A linha em foco virou
// um CARTAO DE LEITURA: a manchete como cabecalho (TXT_HEADLINE, ate duas
// linhas), veiculo e quando ("IMDb · há 3 h") e um trecho do texto em duas
// linhas. As outras linhas continuam uma manchete e o veiculo, em 96 px.
//
// O TRECHO E DA PAGINA DO VEICULO, e por isso chega depois: o RSS do Google so
// traz a manchete de novo na <description>. Pedir a pagina de cada manchete ao
// abrir o painel seria uma dezena de downloads de 500 KB; entao o trecho e
// pedido (noticia_pedir) quando o foco PARA 350 ms numa linha — a mesma
// pagina que o OK vai abrir, entao nada e baixado a toa. Enquanto nao chega,
// o cartao ja tem a altura final e o lugar do trecho e um esqueleto de duas
// linhas; o texto entra INTEIRO de uma vez (portao de texto, textogate.h).
// Pagina que nao deu texto: o cartao fica com manchete e veiculo, sem trecho.
#define AGN_ESPERA_MS 350u

static int alturaManchete(int f) { return f ? (int)AGN_FOCO : (int)AGN_LINHA; }

#ifdef NV_TOUCH_UI
static float toqueMancheteY(int i) {
  return i * AGN_LINHA + (i > notFoco ? AGN_FOCO - AGN_LINHA : 0.0f);
}
static int toqueMancheteIndice(float offset, int n) {
  for (int i = 0; i < n; i++)
    if (toqueMancheteY(i + 1) > offset) return i;
  return n > 0 ? n - 1 : 0;
}
static unsigned toqueMancheteHash(unsigned h, const char *s) {
  for (; *s; s++) h = (h ^ (unsigned char)*s) * 16777619u;
  return (h ^ 0xffu) * 16777619u;
}
static float toqueManchetePreparar(const AgItem *it, int n, GfxRect regiao, int ini, int ativa) {
  unsigned chave = toqueMancheteHash(2166136261u, it->imdb);
  chave = (chave ^ (unsigned)n) * 16777619u;
  for (int i = 0; i < n; i++) {
    const Noticia *nt = noticias_item(it->imdb, i);
    if (!nt) continue;
    chave = toqueMancheteHash(chave, nt->titulo);
    chave = toqueMancheteHash(chave, nt->link);
    chave = toqueMancheteHash(chave, nt->fonte);
  }
  if (chave != toqueManchetesChave) {
    toqueManchetesReiniciar(); toqueManchetesChave = chave;
  }
  if (!toquerol_livre(&toqueManchetes)) toqueManchetesOffset = toqueMancheteY(ini);
  if (ativa) {
    toquerol_vincular(&toqueManchetes, regiao, gfx_escala(), 0.0f,
                     toqueMancheteY(n) - regiao.h, 1, &toqueManchetesOffset);
    toqueManchetesOffset = toquerol_clamp(toqueManchetesOffset, 0.0f, toqueManchetes.maximo);
    ponteiro_rolagem(toqueAgendaRolar);
  }
  return toqueManchetesOffset;
}
#endif

static void desenhaManchetes(const AgItem *it, float a) {
  float ar, ag, ab;
  int n = noticias_n(it->imdb), i, resp = noticias_respondeu(it->imdb);
  int tf = tintaF(), tf2 = tintaF2();
  float h = 160.0f + (float)(n > 0 ? (n - 1) * AGN_LINHA + AGN_FOCO : AGN_LINHA) + 40.0f;
  float maxH = NV_TELA_H - 2.0f * NV_MARGEM_Y, y, yFim;
  long long agora = (long long)time(NULL);
  GfxRect r;
  ajustes_acento(&ar, &ag, &ab);
  if (h > maxH) h = maxH;
  r = agModalR(AGN_W, h, a);
  painelFlutuante(r, ar, ag, ab, a);
  y = cabecalho(r.x + 48.0f, r.y + 40.0f, r.w - 96.0f, it->titulo,
                i18n("Últimas notícias · Google News"), a) + 40.0f;
  if (!resp || n == 0) {
    TxtLinha t = txt_linha_corta(TXT_BODY, !resp ? i18n("Procurando…")
                                 : i18n("Nada publicado recentemente sobre este título."),
                                 170, 174, 184, 255, r.w - 96.0f);
    txt_desenhar_alpha(t, r.x + 48.0f, y + 20.0f, a);
    return;
  }
  if (notFoco >= n) notFoco = n - 1;
  yFim = r.y + r.h - 24.0f;
  // A PRIMEIRA LINHA VISIVEL: a menor que ainda deixa a focada inteira dentro
  // do painel, somando as alturas de verdade (a focada e mais alta).
  { int ini = notFoco;
    float soma = (float)alturaManchete(1);
    while (ini > 0 && soma + (float)alturaManchete(0) <= yFim - y) { ini--; soma += (float)alturaManchete(0); }
#ifdef NV_TOUCH_UI
    float yTopo = y;
    if (agColunaUnica()) {
      y -= toqueManchetePreparar(it, n, (GfxRect){r.x, yTopo, r.w, yFim - yTopo}, ini, a > 0.99f && ctxAberto == 2);
      ini = 0;
    }
    gfx_recorte(r.x, yTopo, r.w, yFim - yTopo);
#else
    gfx_recorte(r.x, y, r.w, yFim - y);
#endif
    for (i = ini; i < n && y < yFim; i++) {
      const Noticia *nt = noticias_item(it->imdb, i);
      int f = (i == notFoco);
      float lh = (float)alturaManchete(f);
      // SO LINHA INTEIRA: a que nao cabe ate o pe do painel fica para a
      // rolagem, em vez de sair cortada no meio das letras.
#ifdef NV_TOUCH_UI
      if (agColunaUnica()) {
        if (y + lh < yTopo) { y += lh; continue; }
      } else
#endif
      if (y + lh - 12.0f > yFim) break;
      GfxRect lr = { r.x + 28.0f, y, r.w - 56.0f, lh - 12.0f };
      char sub[160], quando[48];
      float tx = lr.x + 24.0f, tw = lr.w - 48.0f, ty;
      if (!nt) { y += lh; continue; }
      if (a > 0.99f && ctxAberto == 2) {
#ifdef NV_TOUCH_UI
        if (agColunaUnica()) ponteiro_alvo_faixa(lr.x, lr.y, lr.w, lr.h, yTopo, yFim, ponteiroManchete, NULL, i, 0);
        else
#endif
          ponteiro_alvo(lr.x, lr.y, lr.w, lr.h, ponteiroManchete, NULL, i, 0);
      }
      noticias_quando(nt, agora, quando, sizeof quando);
      if (quando[0] && nt->fonte[0]) snprintf(sub, sizeof sub, "%s \xc2\xb7 %s", nt->fonte, quando);
      else snprintf(sub, sizeof sub, "%s%s", nt->fonte, quando);
      if (!f) {
        TxtLinha t = txt_linha_corta(TXT_CALLOUT, nt->titulo, 238, 240, 244, 255, tw);
        txt_desenhar_alpha(t, tx, y + 14.0f, a);
        t = txt_linha_corta(TXT_CAPTION, sub, 150, 153, 162, 255, tw);
        txt_desenhar_alpha(t, tx, y + 14.0f + 40.0f, a);
        y += lh;
        continue;
      }
      pilulaFoco(lr, ar, ag, ab, a);
      // O trecho e pedido quando o foco para aqui (ver a nota acima).
      if (nt->link[0] && SDL_GetTicks() - notDesde >= AGN_ESPERA_MS) noticia_pedir(nt->link);
      ty = y + 22.0f;
      { int nl = agendaui_sinopse(TXT_HEADLINE, nt->titulo, tx, ty, tw, 40.0f, 2, tf, tf, tf, a);
        ty += 40.0f * (float)(nl > 0 ? nl - 1 : 0) + hEstilo(TXT_HEADLINE) + 6.0f; }
      { TxtLinha s = txt_linha_corta(TXT_CAPTION, sub, tf2, tf2, tf2, 255, tw);
        txt_desenhar_alpha(s, tx, ty, a);
        ty += (float)s.h + 12.0f; }
      { int est = NTC_NADA;
        const NoticiaTexto *nx = nt->link[0] ? noticia_texto(nt->link, &est) : NULL;
        const char *trecho = NULL;
        if (nx && est == NTC_PRONTA) trecho = nx->l.resumo[0] ? nx->l.resumo : (nx->l.n ? nx->l.par[0] : NULL);
        if (trecho && trecho[0]) {
          int antes = txt_pendentes;
          float ga = f ? a * notTrechoA : 0.0f;
          // O ultimo quadro do portao ja desenhou com opacidade quase nula;
          // o bloco aparece junto (NV_TXTGATE_AQUECER enquanto fecha).
          agendaui_sinopse(TXT_CAPTION, trecho, tx, ty, tw, AG_SIN_LD, 2, tf2, tf2, tf2,
                           ga > NV_TXTGATE_AQUECER ? ga : NV_TXTGATE_AQUECER);
          notTrechoA = textogate_passo(&notTrechoGate, txt_pendentes - antes, SDL_GetTicks());
        } else if (nt->link[0] && (est == NTC_BUSCANDO ||
                                   (est == NTC_NADA && SDL_GetTicks() - notDesde < AGN_ESPERA_MS + 50u))) {
          int k;
          textogate_reiniciar(&notTrechoGate); notTrechoA = 0.0f;
          for (k = 0; k < 2; k++) {
            GfxRect sk = { tx, ty + AG_SIN_LD * (float)k + 6.0f, tw * (k ? 0.58f : 0.86f), 16.0f };
            // Esqueleto em cima do realce: a propria tinta secundaria a 22%.
            gfx_esqueleto(sk, 0.5f, (float)tf2 / 255.0f, (float)tf2 / 255.0f,
                          (float)tf2 / 255.0f, 0.22f * a);
          }
        } else { textogate_reiniciar(&notTrechoGate); notTrechoA = 0.0f; } }
      y += lh;
    }
    gfx_sem_recorte(); }
}

// --- A NOTICIA -------------------------------------------------------------------
//
// O MODAL DE LEITURA: a capa (og:image) e o veiculo a esquerda, o texto a
// direita. A capa passa por tex_obter_larg NA LARGURA DESENHADA (560), a mesma
// regra de todo cartaz do app: a decodificacao para no tamanho da tela.
//
// TRES ESTADOS, e nenhum finge:
//   BUSCANDO  manchete do RSS + esqueleto de paragrafos; o texto entra inteiro
//             pelo portao de texto quando chega.
//   PRONTA    resumo (og:description) em destaque e ate seis paragrafos,
//             rolados por CIMA/BAIXO dentro da coluna (recorte, sem degrade).
//   FALHOU    a manchete, uma frase dizendo que a pagina nao deixou ler, e o QR
//             "Abrir no celular" grande — o caminho que sobra de verdade.
// O QR aparece nos dois ultimos estados sempre que a URL cabe nele (sem a
// query, ate 134 bytes: leitura_url_qr); sem URL, "Leia em <veiculo>".
static GLuint texQrNot;
static char   qrNotDe[160];

static GLuint qrTextura(const char *texto) {
  Qr q; int lado, xq, yq; unsigned char *px;
  if (!texto || !texto[0]) return 0;
  if (!strcmp(qrNotDe, texto) && texQrNot) return texQrNot;
  if (!qr_gerar(&q, texto)) return 0;
  lado = q.lado + 8;                    // 4 modulos de silencio por lado
  px = (unsigned char *)malloc((size_t)lado * lado * 3);
  if (!px) return 0;
  memset(px, 255, (size_t)lado * lado * 3);
  for (yq = 0; yq < q.lado; yq++)
    for (xq = 0; xq < q.lado; xq++)
      if (qr_modulo(&q, xq, yq)) {
        size_t k = ((size_t)(yq + 4) * lado + (xq + 4)) * 3;
        px[k] = px[k + 1] = px[k + 2] = 0;
      }
  if (!texQrNot) glGenTextures(1, &texQrNot);
  glBindTexture(GL_TEXTURE_2D, texQrNot);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, lado, lado, 0, GL_RGB, GL_UNSIGNED_BYTE, px);
  // NEAREST: um modulo borrado com o vizinho e ilegivel para a camera.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gfx_tex_esquecer(0);   // o bind acima foi por fora do gfx_rect
  free(px);
  snprintf(qrNotDe, sizeof qrNotDe, "%s", texto);
  return texQrNot;
}

static void desenhaNoticia(const AgItem *it, float a) {
  const Noticia *nt = noticias_item(it->imdb, notFoco);
  float ar, ag, ab, xL, xR, wR, y, yTopo, yFim;
  int est = NTC_NADA;
  const NoticiaTexto *nx;
  GfxRect r;
  char qrUrl[160] = "", host[120] = "", quando[48] = "";
  int semCapa = 0;
  if (!nt) return;
  ajustes_acento(&ar, &ag, &ab);
  nx = nt->link[0] ? noticia_texto(nt->link, &est) : NULL;
  if (!nt->link[0]) est = NTC_FALHOU;
  notHAlvo = est == NTC_FALHOU ? AGL_H_FALHA : AGL_H;
  { float h = notH > 0.0f ? notH : notHAlvo;
    r = agModalR(AGL_W, h, a); }
  painelFlutuante(r, ar, ag, ab, a);
  xL = r.x + 48.0f;
  xR = xL + AGL_IMG_W + 44.0f;
  int empilhado = agColunaUnica() && NV_TELA_H > NV_TELA_W;
  if (empilhado) xR = xL;
  wR = r.x + r.w - 56.0f - xR;
  yTopo = r.y + 48.0f;
  yFim = r.y + r.h - 48.0f;
#ifdef NV_TOUCH_UI
  if (agColunaUnica()) {
    toquerol_vincular(&toqueNoticia, (GfxRect){xR, yTopo, wR, yFim - yTopo}, gfx_escala(),
                     0.0f, notRolMax, 1, &notRol);
    ponteiro_rolagem(toqueAgendaRolar);
  }
#endif
  noticias_quando(nt, (long long)time(NULL), quando, sizeof quando);
  if (nx && nx->url[0]) { leitura_url_qr(nx->url, qrUrl, sizeof qrUrl); leitura_host(nx->url, host, sizeof host); }

  // --- coluna da esquerda: capa, veiculo, QR -----------------------------------
  if (!empilhado) {
  y = yTopo;
  { const char *img = nx ? nx->l.imagem : "";
    GfxRect cap = { xL, y, AGL_IMG_W, AGL_IMG_W * 9.0f / 16.0f };
    GLuint tex = img[0] ? tex_obter_larg(img, AGL_IMG_W) : 0;
    if (tex) {
      gfx_tex_aspect_atual = 16.0f / 9.0f;
      gfx_rect(cap, tex, GFX_CARD, 0, 0, 0, 20.0f / cap.h, 0, 0, 0, a);
      gfx_tex_aspect_atual = 0.0f;
      y += cap.h + 28.0f;
    } else if (est == NTC_BUSCANDO || (img[0] && !tex_falhou(img))) {
      // A capa ainda vindo: o lugar dela ja esta la.
      gfx_esqueleto(cap, 20.0f / cap.h, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, a);
      y += cap.h + 28.0f;
    } else {
      semCapa = 1;
      // SEM CAPA (pagina sem og:image, ou que nem deixou ler): o nome do
      // veiculo como MANCHETE DE JORNAL no lugar da foto, na superficie
      // neutra dos cartoes. A coluna nao fica um vao com duas palavras no
      // topo, e nada ali finge ser a imagem da materia.
      const char *fonte = (nx && nx->l.site[0]) ? nx->l.site : nt->fonte;
      TxtLinha t = txt_linha_corta(TXT_TITULO2, fonte, 214, 216, 222, 255, cap.w - 64.0f);
      gfx_cor(cap, 20.0f / cap.h, 0.10f, 0.11f, 0.13f, a);
      // O "quando" vai junto, logo abaixo do nome: sozinho embaixo do cartao
      // ele ficava orfao, uma palavra solta a 100 px de tudo.
      { TxtLinha q = txt_linha_corta(TXT_CAPTION, quando, 150, 153, 162, 255, cap.w - 64.0f);
        float hb = (float)t.h + (quando[0] ? 6.0f + (float)q.h : 0.0f);
        float yb = cap.y + (cap.h - hb) * 0.5f;
        txt_desenhar_alpha(t, cap.x + (cap.w - (float)t.w) * 0.5f, yb, a);
        if (quando[0]) txt_desenhar_alpha(q, cap.x + (cap.w - (float)q.w) * 0.5f, yb + (float)t.h + 6.0f, a); }
      y += cap.h + 28.0f;
    } }
  { const char *fonte = (nx && nx->l.site[0]) ? nx->l.site : nt->fonte;
    TxtLinha t = txt_linha_corta(TXT_CALLOUT, fonte, 238, 240, 244, 255, AGL_IMG_W);
    // Com a capa de MANCHETE (sem og:image) o veiculo ja esta escrito nela:
    // repetir embaixo seria o mesmo nome duas vezes a 300 px.
    if (semCapa) t.w = t.h = 0;
    else txt_desenhar_alpha(t, xL, y, a);
    y += (float)t.h + 4.0f;
    if (quando[0] && !semCapa) {
      t = txt_linha_corta(TXT_CAPTION, quando, 150, 153, 162, 255, AGL_IMG_W);
      txt_desenhar_alpha(t, xL, y, a);
      y += (float)t.h;
    } }
  if (est == NTC_PRONTA) {
    GLuint q = qrUrl[0] ? qrTextura(qrUrl) : 0;
    float lado = AGL_QR;
    float yq = yFim - lado;
    if (q && yq > y + 24.0f) {
      GfxRect rq = { xL, yq, lado, lado };
      TxtLinha t;
      gfx_rect(rq, q, GFX_SNAP, 0, 0.0f, 0.0f, 0.0f, 0, 0, 0, a);
      t = txt_linha_corta(TXT_CALLOUT, i18n("Abrir no celular"), 238, 240, 244, 255, AGL_IMG_W - lado - 28.0f);
      txt_desenhar_alpha(t, xL + lado + 28.0f, yq + lado * 0.5f - (float)t.h, a);
      if (host[0]) {
        t = txt_linha_corta(TXT_CAPTION, host, 150, 153, 162, 255, AGL_IMG_W - lado - 28.0f);
        txt_desenhar_alpha(t, xL + lado + 28.0f, yq + lado * 0.5f + 4.0f, a);
      }
    } else if (host[0] || nt->fonte[0]) {
      char leia[200];
      TxtLinha t;
      snprintf(leia, sizeof leia, i18n("Leia em %s"), host[0] ? host : nt->fonte);
      t = txt_linha_corta(TXT_CAPTION, leia, 150, 153, 162, 255, AGL_IMG_W);
      txt_desenhar_alpha(t, xL, yFim - (float)t.h, a);
    }
  }

  }
  // --- coluna da direita: manchete, resumo, paragrafos -------------------------
  gfx_recorte(xR, yTopo - 4.0f, wR + 24.0f, yFim - yTopo + 8.0f);
  { float yy = yTopo - notRol;
    int antes = txt_pendentes;
    float ga = est == NTC_PRONTA ? notGateA : 1.0f;
    const char *titulo = (nx && nx->l.titulo[0]) ? nx->l.titulo : nt->titulo;
    float gAlpha = a * (ga > NV_TXTGATE_AQUECER ? ga : NV_TXTGATE_AQUECER);
    if (empilhado) {
      char origem[256];
      snprintf(origem, sizeof origem, "%s%s%s", nx && nx->l.site[0] ? nx->l.site : nt->fonte,
               quando[0] ? " \xc2\xb7 " : "", quando);
      TxtLinha l = txt_linha_corta(TXT_CAPTION, origem, 150, 153, 162, 255, wR);
      txt_desenhar_alpha(l, xR, yy, a); yy += l.h + 16;
    }
    // A manchete do RSS primeiro (ja esta na mao); com a pagina pronta, o
    // og:title — que vem sem o " - Veiculo" e as vezes mais completo.
    yy += txt_bloco_corta(TXT_TITULO3, est == NTC_PRONTA ? titulo : nt->titulo,
                          246, 247, 250, xR, yy, wR, 58.0f, est == NTC_PRONTA ? gAlpha : a, 3);
    yy += 22.0f;
    if (est == NTC_PRONTA && nx) {
      int k;
      if (nx->l.resumo[0]) {
        yy += txt_bloco_corta(TXT_CALLOUT, nx->l.resumo, 214, 216, 222, xR, yy, wR, 38.0f, gAlpha, 4);
        yy += 26.0f;
      }
      for (k = 0; k < nx->l.n; k++) {
        yy += txt_bloco_corta(TXT_BODY, nx->l.par[k], 196, 198, 206, xR, yy, wR, 36.0f, gAlpha, 0);
        yy += 22.0f;
      }
      if (empilhado) {
        GLuint q = qrUrl[0] ? qrTextura(qrUrl) : 0;
        if (q) {
          gfx_rect((GfxRect){xR, yy, AGL_QR, AGL_QR}, q, GFX_SNAP, 0, 0, 0, 0, 0, 0, 0, a);
          TxtLinha l = txt_linha_corta(TXT_CALLOUT, i18n("Abrir no celular"), 238, 240, 244, 255, wR - AGL_QR - 28);
          txt_desenhar_alpha(l, xR + AGL_QR + 28, yy + 24, a);
          if (host[0]) {
            l = txt_linha_corta(TXT_CAPTION, host, 150, 153, 162, 255, wR - AGL_QR - 28);
            txt_desenhar_alpha(l, xR + AGL_QR + 28, yy + 66, a);
          }
          yy += AGL_QR + 22;
        } else if (host[0] || nt->fonte[0]) {
          char leia[200];
          snprintf(leia, sizeof leia, i18n("Leia em %s"), host[0] ? host : nt->fonte);
          TxtLinha l = txt_linha_corta(TXT_CAPTION, leia, 150, 153, 162, 255, wR);
          txt_desenhar_alpha(l, xR, yy, a); yy += l.h + 22;
        }
      }
      notGateA = textogate_passo(&notGate, txt_pendentes - antes, SDL_GetTicks());
      // O teto da rolagem: o fim do texto encosta no pe da coluna.
      { float total = yy + notRol - yTopo, cabe = yFim - yTopo;
        notRolMax = total > cabe ? total - cabe : 0.0f; }
    } else if (est == NTC_BUSCANDO || est == NTC_NADA) {
      int k;
      TxtLinha t = txt_linha_corta(TXT_CAPTION, i18n("Carregando a notícia…"), 150, 153, 162, 255, wR);
      txt_desenhar_alpha(t, xR, yy, a);
      yy += (float)t.h + 22.0f;
      for (k = 0; k < 7; k++) {
        GfxRect sk = { xR, yy + 40.0f * (float)k, wR * (k % 3 == 2 ? 0.64f : 0.94f), 20.0f };
        gfx_esqueleto(sk, 0.5f, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, a);
      }
      notRolMax = 0.0f;
    } else {
      // O FALLBACK: a frase, e o QR GRANDE logo abaixo dela, na coluna do
      // texto — e o caminho que sobra para ler a materia, entao e ele que
      // ocupa o lugar do texto. Sem URL que caiba no QR, "Leia em <veiculo>".
      GLuint q = qrUrl[0] ? qrTextura(qrUrl) : 0;
      yy += txt_bloco_corta(TXT_CALLOUT, i18n("Não deu para trazer o texto desta página."),
                            214, 216, 222, xR, yy, wR, 38.0f, a, 2);
      yy += 36.0f;
      if (q) {
        float lado = AGL_QR + 40.0f, xq = xR + lado + 32.0f, wq = wR - lado - 32.0f;
        float blocoH = lado;
        GfxRect rq = { xR, yy, lado, lado };
        TxtLinha t;
        gfx_rect(rq, q, GFX_SNAP, 0, 0.0f, 0.0f, 0.0f, 0, 0, 0, a);
        t = txt_linha_corta(TXT_HEADLINE, i18n("Abrir no celular"), 238, 240, 244, 255, wq);
        txt_desenhar_alpha(t, xq, yy + 20.0f, a);
        { float hb = txt_bloco_corta(TXT_CAPTION, i18n("Aponte a câmera do celular para ler a matéria completa."),
                                     150, 153, 162, xq, yy + 20.0f + (float)t.h + 10.0f, wq, 32.0f, a, 3);
          blocoH = fmaxf(blocoH, 20 + t.h + 10 + hb);
          if (host[0]) {
            TxtLinha hl = txt_linha_corta(TXT_CAPTION, host, 150, 153, 162, 255, wq);
            txt_desenhar_alpha(hl, xq, yy + 20.0f + (float)t.h + 10.0f + hb + 8.0f, a);
            blocoH = fmaxf(blocoH, 20 + t.h + 10 + hb + 8 + hl.h);
          } }
        yy += blocoH + 16;
      } else if (host[0] || nt->fonte[0]) {
        char leia[200];
        TxtLinha t;
        snprintf(leia, sizeof leia, i18n("Leia em %s"), host[0] ? host : nt->fonte);
        t = txt_linha_corta(TXT_CALLOUT, leia, 170, 174, 184, 255, wR);
        txt_desenhar_alpha(t, xR, yy, a);
        yy += t.h;
      }
      notRolMax = empilhado ? fmaxf(0, yy + notRol - yTopo - (yFim - yTopo)) : 0;
    } }
  gfx_sem_recorte();
  // O FIO DE ROLAGEM: so quando ha o que rolar, fino e neutro, na borda da
  // coluna — diz "tem mais" sem degrade por cima do texto.
  if (notRolMax > 1.0f) {
    float trilho = yFim - yTopo, fr = trilho / (trilho + notRolMax);
    GfxRect tr = { r.x + r.w - 30.0f, yTopo, 4.0f, trilho };
    GfxRect ba = { tr.x, yTopo + (trilho - trilho * fr) * (notRol / notRolMax), 4.0f, trilho * fr };
    // O raio e FRACAO DA ALTURA: 0,5 num trilho de 4 x 700 virava uma lente
    // afilada nas pontas (trilho sumido, barra em dois gomos). 2 px / h e o
    // meio da largura — a capsula de 4 px que se queria.
    if (ajustes_vidro()) gfx_vidro_chip(tr, 2.0f / tr.h, 0.55f, a);
    else gfx_cor(tr, 2.0f / tr.h, 1.0f, 1.0f, 1.0f, 0.14f * a);
    gfx_cor(ba, 2.0f / ba.h, 0.59f, 0.59f, 0.59f, a);
  }
}

static void desenhaContexto(float a) {
  const AgItem *it = agenda_lista(ctxItem);
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  if (!it) return;
  // PONTEIRO (#99): o modal e uma camada — a lista de tras deixa de valer.
  // A noticia aberta (3) nao registra nada: o clique e o OK, que ali nao faz
  // nada, e a rodinha rola o texto.
  if (ctxAberto) ponteiro_camada();
  // O veu subiu de 0,62 para 0,74: com o modal aberto, o "Agenda" e as linhas
  // atras ainda liam como texto concorrente nas bordas do painel (critica de
  // 29/09/2026). Com vidro, mais ainda: o painel deixa passar o fundo.
  gfx_cor(tela, 0.0f, 0.0f, 0.0f, 0.0f, (ajustes_vidro() ? 0.82f : 0.74f) * a);
  int telefone = agColunaUnica();
  float escalaAnt = gfx_escala();
  if (telefone) { escModal = 1; gfx_escala_sair(1); }
  if (ctxAberto == 3 || (ctxAberto == 0 && ctxUltimo == 3)) desenhaNoticia(it, a);
  else if (ctxAberto == 2 || (ctxAberto == 0 && ctxUltimo == 2)) desenhaManchetes(it, a);
  else desenhaModalSerie(it, a);
  if (telefone) { escModal = 0; gfx_escala_sair(escalaAnt); }
}

static void botaoAgenda(GfxRect r, const char *rotulo, int ativo, int foco,
                        int acento) {
  float ar, ag, ab;
  TxtLinha t;
  int c = ajustes_vidro() ? 230 : 220;
  ajustes_acento(&ar, &ag, &ab);
  if (ajustes_vidro()) {
    gfx_vidro_painel(r, 0.5f, ativo ? 0.58f : 0.36f, 1.0f);
    if (ativo) gfx_cor(r, 0.5f, ar, ag, ab, 0.17f);
    if (foco) gfx_vidro_foco(r, 0.5f, 1.0f, 1.0f);
  } else {
    gfx_cor(r, 0.5f, ativo ? ar : 0.09f, ativo ? ag : 0.095f,
            ativo ? ab : 0.11f, ativo ? 0.24f : 0.92f);
    if (foco) gfx_anel(r, 0.5f, 2.0f, ar, ag, ab, 0.92f);
  }
  if (acento || (ativo && !foco))
    t = txt_linha(TXT_HERO_SEC, rotulo,
                  (int)(ar * 255.0f + 0.5f), (int)(ag * 255.0f + 0.5f),
                  (int)(ab * 255.0f + 0.5f), 255);
  else
    t = txt_linha(TXT_HERO_SEC, rotulo, c, c, c, 255);
  if (agColunaUnica()) {
    int cr = acento || (ativo && !foco) ? (int)(ar * 255.0f + .5f) : c;
    int cg = acento || (ativo && !foco) ? (int)(ag * 255.0f + .5f) : c;
    int cb = acento || (ativo && !foco) ? (int)(ab * 255.0f + .5f) : c;
    float corpo = agTelefonePx(32);
    float w = txC1(TXT_G30M, 30, rotulo, cr, cg, cb, -1, 0, corpo, r.w - P(24), 1);
    txC1(TXT_G30M, 30, rotulo, cr, cg, cb, r.x + (r.w - w) * .5f,
         r.y + (r.h - hC1(TXT_G30M, 30, corpo, cr)) * .5f, corpo, r.w - P(24), 1);
    return;
  }
  txt_desenhar_alpha(t, r.x + (r.w - t.w) * 0.5f,
                     r.y + (r.h - t.h) * 0.5f, 1.0f);
}

static void seletorAgendaTelefone(float x, float xDir, float y) {
  float gap = P(agTelefonePx(12)), h = P(agTelefonePx(112));
  float w = fminf(P(agTelefonePx(180)), (xDir - x - gap) * .5f);
  GfxRect lista = {xDir - w * 2 - gap, y, w, h};
  GfxRect mes = {xDir - w, y, w, h};
  if (!ctxAberto) {
    ponteiro_alvo(lista.x, lista.y, lista.w, lista.h, ponteiroCab, NULL, AG_CAB_LISTA, 0);
    ponteiro_alvo(mes.x, mes.y, mes.w, mes.h, ponteiroCab, NULL, AG_CAB_MES, 0);
  }
  botaoAgenda(lista, i18n("Lista"), !vistaMes, focoCabecalho == AG_CAB_LISTA,
              !vistaMes && focoCabecalho != AG_CAB_LISTA);
  botaoAgenda(mes, i18n("Mês"), vistaMes, focoCabecalho == AG_CAB_MES,
              vistaMes && focoCabecalho != AG_CAB_MES);
}

static void desenhaBarraCalendario(float x, float xDir) {
  if (agColunaUnica()) {
    float h = P(agTelefonePx(112)), gap = P(agTelefonePx(12)), y;
    seletorAgendaTelefone(x, xDir, P(agTelefonePx(192)));
    if (vistaMes) {
      char rot[96], cru[96];
      float seta = P(agTelefonePx(112));
      y = P(agTelefonePx(320));
      GfxRect ant = {x, y, seta, h}, prox = {xDir - seta, y, seta, h};
      GfxRect centro = {x + seta + gap, y, xDir - x - 2 * (seta + gap), h};
      snprintf(cru, sizeof cru, "%s %d", i18n(agenda_mes_nome(calMes)), calAno);
      maiusc(rot, sizeof rot, cru);
      if (!ctxAberto) {
        ponteiro_alvo(ant.x, ant.y, ant.w, ant.h, ponteiroCab, NULL, AG_CAB_ANTERIOR, 0);
        ponteiro_alvo(centro.x, centro.y, centro.w, centro.h, ponteiroCab, NULL, AG_CAB_HOJE, 0);
        ponteiro_alvo(prox.x, prox.y, prox.w, prox.h, ponteiroCab, NULL, AG_CAB_PROXIMO, 0);
      }
      botaoAgenda(ant, "‹", focoCabecalho == AG_CAB_ANTERIOR, focoCabecalho == AG_CAB_ANTERIOR, 0);
      botaoAgenda(centro, rot, 1, focoCabecalho == AG_CAB_HOJE, 0);
      botaoAgenda(prox, "›", focoCabecalho == AG_CAB_PROXIMO, focoCabecalho == AG_CAB_PROXIMO, 0);
    }
    return;
  }
  const float u = agColunaUnica() ? P(1) : 1.0f;
  const float y = (AG_LISTA_Y - 46.0f) * u, h = 42.0f * u;
  int focandoMes = focoCabecalho == AG_CAB_HOJE;
  if (vistaMes) {
    char mes[96], mesBruto[96];
    GfxRect ant = { x, y, 46.0f * u, h };
    GfxRect centro = { x + 54.0f * u, y, 238.0f * u, h };
    GfxRect prox = { x + 300.0f * u, y, 46.0f * u, h };
    snprintf(mesBruto, sizeof mesBruto, "%s %d", i18n(agenda_mes_nome(calMes)), calAno);
    maiusc(mes, sizeof mes, mesBruto);
    if (!ctxAberto) {
      ponteiro_alvo(ant.x, ant.y, ant.w, ant.h, ponteiroCab, NULL, AG_CAB_ANTERIOR, 0);
      ponteiro_alvo(centro.x, centro.y, centro.w, centro.h, ponteiroCab, NULL, AG_CAB_HOJE, 0);
      ponteiro_alvo(prox.x, prox.y, prox.w, prox.h, ponteiroCab, NULL, AG_CAB_PROXIMO, 0);
    }
    botaoAgenda(ant, "‹", focoCabecalho == AG_CAB_ANTERIOR, focoCabecalho == AG_CAB_ANTERIOR, 0);
    botaoAgenda(centro, mes, 1, focandoMes, 0);
    botaoAgenda(prox, "›", focoCabecalho == AG_CAB_PROXIMO, focoCabecalho == AG_CAB_PROXIMO, 0);
  }
  { float w = 94.0f * u, gap = 8.0f * u;
    GfxRect lista = { xDir - w * 2.0f - gap, y, w, h };
    GfxRect mes = { xDir - w, y, w, h };
    if (!ctxAberto) {
      ponteiro_alvo(lista.x, lista.y, lista.w, lista.h, ponteiroCab, NULL, AG_CAB_LISTA, 0);
      ponteiro_alvo(mes.x, mes.y, mes.w, mes.h, ponteiroCab, NULL, AG_CAB_MES, 0);
    }
    botaoAgenda(lista, i18n("Lista"), !vistaMes, focoCabecalho == AG_CAB_LISTA,
                !vistaMes && focoCabecalho != AG_CAB_LISTA);
    botaoAgenda(mes, i18n("Mês"), vistaMes, focoCabecalho == AG_CAB_MES,
                vistaMes && focoCabecalho != AG_CAB_MES);
  }
}

static void desenhaDiaCalendario(GfxRect r, int dia, int mesmoMes,
                                 int hoje, int eventos, int selecionado) {
  float ar, ag, ab;
  int cor = mesmoMes ? 232 : 116;
  char num[8], qtd[16];
  TxtLinha t;
  ajustes_acento(&ar, &ag, &ab);
  if (ajustes_vidro()) {
    gfx_cor(r, 0.14f, 1, 1, 1, selecionado ? 0.12f : 0.035f);
    if (selecionado) gfx_vidro_foco(r, 0.14f, 1.0f, 1.0f);
  } else {
    gfx_cor(r, 0.14f, selecionado ? ar : 0.13f,
            selecionado ? ag : 0.14f, selecionado ? ab : 0.16f,
            selecionado ? 0.32f : 0.92f);
    if (selecionado) gfx_anel(r, 0.14f, 2.0f, ar, ag, ab, 0.9f);
  }
  snprintf(num, sizeof num, "%d", dia);
  if (agColunaUnica()) {
    float corpo = agTelefonePx(36);
    float nw = txC1(TXT_G30B, 30, num, cor, cor, cor, -1, 0, corpo, r.w - P(16), 1);
    txC1(TXT_G30B, 30, num,
         hoje ? (int)(ar * 255) : cor, hoje ? (int)(ag * 255) : cor,
         hoje ? (int)(ab * 255) : cor, r.x + (r.w - nw) * .5f, r.y + P(agTelefonePx(18)),
         corpo, r.w - P(16), 1);
    if (hoje) gfx_cor((GfxRect){r.x + r.w * .5f - P(4), r.y + r.h - P(agTelefonePx(22)), P(8), P(8)}, .5f, ar, ag, ab, 1);
    if (eventos > 0) {
      int vis = eventos > 3 ? 3 : eventos;
      for (int i = 0; i < vis; i++)
        gfx_cor((GfxRect){r.x + P(14 + i * 14), r.y + r.h - P(agTelefonePx(12)), P(8), P(8)}, .5f, ar, ag, ab, .9f);
      if (eventos > 3) {
        snprintf(qtd, sizeof qtd, "+%d", eventos - 3);
        float w = txC1(TXT_G20M, 20, qtd, 190, 190, 193, -1, 0, agTelefonePx(20), r.w - P(58), 1);
        txC1(TXT_G20M, 20, qtd, 190, 190, 193, r.x + r.w - w - P(8),
             r.y + r.h - P(agTelefonePx(32)), agTelefonePx(20), r.w - P(58), 1);
      }
    }
    return;
  }
  t = txt_linha(TXT_HERO_SEC, num,
                hoje ? (int)(ar * 255.0f) : cor,
                hoje ? (int)(ag * 255.0f) : cor,
                hoje ? (int)(ab * 255.0f) : cor, 255);
  txt_desenhar_alpha(t, r.x + 14.0f + (hoje ? 12.0f : 0.0f), r.y + 10.0f, 1.0f);
  if (hoje)
    gfx_cor((GfxRect){ r.x + 14.0f, r.y + 20.0f, 7.0f, 7.0f }, 0.5f, ar, ag, ab, 1.0f);
  if (eventos > 0) {
    int i, vis = eventos > 3 ? 3 : eventos;
    for (i = 0; i < vis; i++)
      gfx_cor((GfxRect){ r.x + 14.0f + i * 11.0f, r.y + r.h - 14.0f,
                         5.0f, 5.0f }, 0.5f, ar, ag, ab, 0.9f);
    if (eventos > 3) {
      snprintf(qtd, sizeof qtd, "+%d", eventos - 3);
      t = txt_linha(TXT_MINI, qtd, 190, 190, 193, 255);
      txt_desenhar_alpha(t, r.x + r.w - t.w - 10.0f, r.y + r.h - t.h - 7.0f, 1.0f);
    }
  }
}

static void desenhaEventoCalendario(GfxRect r, const AgItem *it, int foco) {
  char ep[220];
  TxtLinha t, subt;
  GfxRect cartaz;
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  gfx_cor(r, 0.16f, 1, 1, 1, foco ? 0.095f : 0.035f);
  if (foco) gfx_vidro_foco(r, 0.16f, 1.0f, 1.0f);
  const float u = agColunaUnica() ? P(1) : 1.0f;
  cartaz = (GfxRect){ r.x + 9.0f * u, r.y + 7.0f * u, 38.0f * u, r.h - 14.0f * u };
  if (it->poster[0] && !agColunaUnica()) {
    GLuint tex = tex_obter_larg(it->poster, 42);
    if (tex) {
      gfx_tex_aspect_atual = cartaz.w / cartaz.h;
      gfx_rect(cartaz, tex, GFX_CARD, 0, 0, 0, 12.0f / cartaz.h, 0, 0, 0, 1.0f);
      gfx_tex_aspect_atual = 0.0f;
    }
  }
  if (agColunaUnica()) {
    cartaz = (GfxRect){r.x + P(agTelefonePx(14)), r.y + P(agTelefonePx(14)),
                       P(agTelefonePx(76)), r.h - P(agTelefonePx(28))};
    if (it->poster[0]) {
      GLuint tex = tex_obter_larg(it->poster, cartaz.w);
      if (tex) {
        gfx_tex_aspect_atual = cartaz.w / cartaz.h;
        gfx_rect(cartaz, tex, GFX_CARD, 0, 0, 0, P(12) / cartaz.h, 0, 0, 0, 1);
        gfx_tex_aspect_atual = 0;
      }
    }
    txC1(TXT_TITULO3, 48, it->titulo[0] ? it->titulo : i18n("Série"),
         foco ? 250 : 224, foco ? 250 : 224, foco ? 250 : 224,
         r.x + P(agTelefonePx(112)), r.y + P(agTelefonePx(14)),
         agTelefonePx(40), r.w - P(agTelefonePx(132)), 1);
    linhaEpisodio(it, ep, sizeof ep);
    if (!ep[0]) snprintf(ep, sizeof ep, "%s", i18n("Próximo episódio"));
    txC1(TXT_G28R, 28, ep, (int)(ar * 220), (int)(ag * 220), (int)(ab * 220),
         r.x + P(agTelefonePx(112)), r.y + P(agTelefonePx(78)), agTelefonePx(32),
         r.w - P(agTelefonePx(132)), 1);
    return;
  }
  t = txt_linha_corta(TXT_HERO_SEC, it->titulo[0] ? it->titulo : i18n("Série"),
                      foco ? 250 : 224, foco ? 250 : 224, foco ? 250 : 224, 255,
                      r.w - 68.0f);
  txt_desenhar_alpha(t, r.x + 58.0f, r.y + 9.0f, 1.0f);
  linhaEpisodio(it, ep, sizeof ep);
  if (!ep[0]) snprintf(ep, sizeof ep, "%s", i18n("Próximo episódio"));
  subt = txt_linha_corta(TXT_CAPTION2, ep, (int)(ar * 220), (int)(ag * 220),
                         (int)(ab * 220), 255, r.w - 68.0f);
  txt_desenhar_alpha(subt, r.x + 58.0f, r.y + 12.0f + t.h, 1.0f);
}

typedef struct {
  float x, semanaY, gradeY, gradeW, celW, celH, espacX, espacY;
  GfxRect painel;
} AgMes;

static AgMes agMesMedir(void) {
  AgMes M;
  const float x = agX(), xDir = agFim();
  const float painelW = 366.0f, vao = 22.0f;
  const float semanaY = AG_LISTA_Y + 4.0f;
  const float gradeY = semanaY + 34.0f;
  const float base = listaBase() - 8.0f;
  float disponivel = xDir - x;
  float gradeW = disponivel - painelW - vao;
  float espacX = 8.0f, espacY = 8.0f;
  float celW = (gradeW - espacX * 6.0f) / 7.0f;
  float celH = (base - gradeY - espacY * 5.0f) / 6.0f;
  float painelX = x + gradeW + vao, painelH = base - semanaY;
  if (gradeW < 450.0f) { gradeW = disponivel * 0.68f; celW = (gradeW - espacX * 6.0f) / 7.0f; }
  if (celH < 44.0f) celH = 44.0f;
  M = (AgMes){x, semanaY, gradeY, gradeW, celW, celH, espacX, espacY,
              {painelX, semanaY, painelW, painelH}};
  if (agColunaUnica()) {
    M.semanaY = P(agTelefonePx(456)); M.gradeY = M.semanaY + P(agTelefonePx(46));
    M.gradeW = disponivel; M.espacX = M.espacY = P(8);
    M.celW = (M.gradeW - M.espacX * 6) / 7; M.celH = P(agTelefonePx(128));
    float y = M.gradeY + M.celH * 6 + M.espacY * 5 + P(24);
    int qtd = eventosDoDia(NULL, 0);
    float docH = P(agTelefonePx(144)) + fmaxf(P(agTelefonePx(156)), qtd * P(agTelefonePx(156))) + P(20);
    M.painel = (GfxRect){x, y, disponivel, docH};
  }
  return M;
}

static GfxRect agMesEventos(const AgMes *M) {
  if (agColunaUnica()) {
    float topo = P(agTelefonePx(144));
    return (GfxRect){M->painel.x, M->painel.y + topo, M->painel.w, M->painel.h - topo - P(20)};
  }
  float u = agColunaUnica() ? P(1) : 1.0f;
  return (GfxRect){M->painel.x, M->painel.y + 104 * u, M->painel.w, M->painel.h - 114 * u};
}

static void desenhaCalendarioMensal(void) {
  AgMes M = agMesMedir();
#ifdef NV_TOUCH_UI
  GfxRect corpo = {M.x, M.semanaY, M.gradeW, NV_TELA_H - P(44) - M.semanaY};
  if (agColunaUnica()) {
    float max = M.painel.y + M.painel.h - corpo.y - corpo.h;
    if (!toquerol_livre(&toqueCalendario)) {
      if (!calPainel) {
        float topoDia = M.gradeY + (calCelula / 7) * (M.celH + M.espacY);
        if (topoDia - toqueCalOffset < corpo.y) toqueCalOffset = topoDia - corpo.y;
        if (topoDia + M.celH - toqueCalOffset > corpo.y + corpo.h)
          toqueCalOffset = topoDia + M.celH - corpo.y - corpo.h;
      } else {
        float topoEp = agMesEventos(&M).y + calEvento * P(agTelefonePx(156));
        if (topoEp - toqueCalOffset < corpo.y) toqueCalOffset = topoEp - corpo.y;
        if (topoEp + P(agTelefonePx(142)) - toqueCalOffset > corpo.y + corpo.h)
          toqueCalOffset = topoEp + P(agTelefonePx(142)) - corpo.y - corpo.h;
      }
    }
    if (toqueCalOffset > max) toqueCalOffset = max;
    if (toqueCalOffset < 0) toqueCalOffset = 0;
    toquerol_vincular(&toqueCalendario, corpo, gfx_escala(), 0, max, 1, &toqueCalOffset);
    ponteiro_rolagem(toqueAgendaRolar);
    M.semanaY -= toqueCalOffset; M.gradeY -= toqueCalOffset; M.painel.y -= toqueCalOffset;
    gfx_recorte(corpo.x, corpo.y, corpo.w, corpo.h);
  }
#endif
  const float x = M.x, semanaY = M.semanaY, gradeY = M.gradeY;
  const float celW = M.celW, celH = M.celH, espacX = M.espacX, espacY = M.espacY;
  const float u = agColunaUnica() ? P(1) : 1.0f;
  char datas[42][12], selecionada[12], rot[112], tmp[96];
  int cont[42] = { 0 }, total[AG_MAX], nTotal, i, c, linha;
  static const int DIAS[] = { 0,1,2,3,4,5,6 };
  float ar, ag, ab;
  GfxRect painel = M.painel;
  const char *h = agenda_hoje();
  ajustes_acento(&ar, &ag, &ab);
  dataSelecionada(selecionada, sizeof selecionada);
  for (c = 0; c < 42; c++) {
    char primeiro[12];
    int semana, dia, ano = calAno, mes = calMes;
    dataIso(calAno, calMes, 1, primeiro, sizeof primeiro);
    semana = agenda_semana(primeiro);
    if (semana < 0) semana = 0;
    dia = c - semana + 1;
    if (dia < 1) {
      if (--mes < 1) { mes = 12; --ano; }
      dia += diasNoMes(ano, mes);
    } else if (dia > diasNoMes(ano, mes)) {
      dia -= diasNoMes(ano, mes);
      if (++mes > 12) { mes = 1; ++ano; }
    }
    dataIso(ano, mes, dia, datas[c], sizeof datas[c]);
  }
  for (i = 0; i < agenda_n(); i++) {
    const AgItem *it = agenda_lista(i);
    if (!temData(it)) continue;
    for (c = 0; c < 42; c++)
      if (!strcmp(it->dataProx, datas[c])) { ++cont[c]; break; }
  }

  // O mes e navegavel por setas no topo; as colunas seguem o calendario local
  // (domingo a sabado), e as datas adjacentes ficam visiveis como contexto.
  for (c = 0; c < 7; c++) {
    char sem[24];
    maiusc(sem, sizeof sem, i18n(agenda_semana_nome(DIAS[c])));
    if (agColunaUnica())
      txC1(TXT_G28B, 28, sem, 135, 138, 146, x + c * (celW + espacX) + P(8),
           semanaY, agTelefonePx(28), celW - P(16), 1);
    else kicker(sem, 135, 138, 146, x + c * (celW + espacX) + 12.0f, semanaY, 1.0f);
  }
  for (c = 0; c < 42; c++) {
    int linha = c / 7, coluna = c % 7;
    int dia = agenda_dia(datas[c]);
    int mesmoMes = agenda_mes(datas[c]) == calMes && agenda_ano(datas[c]) == calAno;
    int hoje = !strcmp(datas[c], h);
    GfxRect cel = { x + coluna * (celW + espacX),
                    gradeY + linha * (celH + espacY), celW, celH };
    // So os dias DO MES: um dia vizinho trocaria o mes debaixo do cursor.
    if (mesmoMes && !ctxAberto) {
#ifdef NV_TOUCH_UI
      if (agColunaUnica()) ponteiro_alvo_faixa(cel.x, cel.y, cel.w, cel.h, corpo.y, corpo.y + corpo.h, ponteiroDia, NULL, c, 0);
      else
#endif
        ponteiro_alvo(cel.x, cel.y, cel.w, cel.h, ponteiroDia, NULL, c, 0);
    }
    desenhaDiaCalendario(cel, dia, mesmoMes, hoje, cont[c], c == calCelula && !calPainel);
  }

  // A coluna de detalhe transforma os pontos do calendario em algo acionavel:
  // entrar nela e escolher a serie abre o mesmo painel de acoes da timeline.
  gfx_rect((GfxRect){ painel.x - 9.0f, painel.y - 7.0f, painel.w + 18.0f,
                      painel.h + 24.0f }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f,
           0, 0, 0, 0.42f);
  if (ajustes_vidro()) gfx_vidro_folha(painel, 28.0f / painel.h, 1.0f);
  else gfx_cor(painel, 28.0f / painel.h, .065f, .07f, .082f, .96f);
  snprintf(rot, sizeof rot, "%d %s %d", calDia,
           idioma_mes_data(calMes, agenda_mes_nome(calMes)), calAno);
  if (agColunaUnica())
    txC1(TXT_TITULO3, 48, rot, 246, 247, 250, painel.x + P(22), painel.y + P(agTelefonePx(18)),
         agTelefonePx(40), painel.w - P(44), 1);
  else { TxtLinha titulo = txt_linha_corta(TXT_HERO_SEC, rot, 246, 247, 250, 255,
                                       painel.w - 44.0f);
    txt_desenhar_alpha(titulo, painel.x + 22.0f, painel.y + 18.0f, 1.0f); }
  nTotal = eventosDoDia(total, AG_MAX);
  snprintf(tmp, sizeof tmp, i18n(nTotal == 1 ? "%d episódio" : "%d episódios"), nTotal);
  if (agColunaUnica())
    txC1(TXT_G28R, 28, tmp, 150, 153, 162, painel.x + P(22), painel.y + P(agTelefonePx(84)),
         agTelefonePx(28), painel.w - P(44), 1);
  else { TxtLinha sub = txt_linha_corta(TXT_CAPTION2, tmp, 150, 153, 162, 255,
                                    painel.w - 44.0f);
    txt_desenhar_alpha(sub, painel.x + 22.0f, painel.y + 66.0f, 1.0f); }
  if (nTotal == 0) {
#ifdef NV_TOUCH_UI
    if (agColunaUnica()) gfx_sem_recorte();
#endif
    return;
  }
  { GfxRect eventos = agMesEventos(&M);
    float y = eventos.y;
    float passo = 78.0f * u;
    int maxLinhas = (int)(eventos.h / passo);
    int inicio = calEvento - maxLinhas / 2;
    if (maxLinhas < 1) maxLinhas = 1;
    if (calIniFixo >= 0 && calPainel && calEvento >= calIniFixo && calEvento < calIniFixo + maxLinhas)
      inicio = calIniFixo;
    else calIniFixo = -1;
    if (inicio < 0) inicio = 0;
    if (inicio > nTotal - maxLinhas) inicio = nTotal - maxLinhas;
    if (inicio < 0) inicio = 0;
#ifdef NV_TOUCH_UI
    if (agColunaUnica()) {
      inicio = 0; maxLinhas = nTotal; passo = P(agTelefonePx(156));
    }
#endif
    for (linha = inicio; linha < nTotal && linha < inicio + maxLinhas; linha++, y += passo) {
      GfxRect r = { painel.x + 12.0f * u, y, painel.w - 24.0f * u, 70.0f * u };
      if (agColunaUnica()) r.h = P(agTelefonePx(142));
      int emFoco = calPainel && linha == calEvento;
#ifdef NV_TOUCH_UI
      if (agColunaUnica()) {
        if (r.y + r.h <= corpo.y || r.y >= corpo.y + corpo.h) continue;
        if (!ctxAberto) ponteiro_alvo_faixa(r.x, r.y, r.w, r.h, corpo.y, corpo.y + corpo.h,
                                          ponteiroEvento, NULL, linha, inicio);
      } else
#endif
      {
        if (r.y + r.h > painel.y + painel.h - 4.0f) break;
        if (!ctxAberto) ponteiro_alvo(r.x, r.y, r.w, r.h, ponteiroEvento, NULL, linha, inicio);
      }
      desenhaEventoCalendario(r, agenda_lista(total[linha]), emFoco);
    }
#ifdef NV_TOUCH_UI
    if (agColunaUnica()) gfx_sem_recorte();
#endif
  }
}

static void desenharNaEscala(Uint32 agora) {
  AgC1 L = c1();
  int n = agenda_n();
  relogio = agora;

  // O TITULO pequeno no canto (C1, como nos Ajustes A3): 40/700. Na Dinamica
  // ele mora na pilula da barra.
  if (!menu_pilula_titulo())
    txC1(agColunaUnica() ? TXT_TITULO2 : TXT_ILHA_TITULO,
         agColunaUnica() ? 57 : 40, i18n("Agenda"), AG_INK, L.x0, P(agColunaUnica() ? 44 : 56),
         agColunaUnica() ? agTelefonePx(52) : 40.0f,
         agColunaUnica() ? L.pnW : L.artW, 1.0f);

  // MES: a grade de sempre (o mesmo layout e a mesma navegacao de antes), com
  // a contagem e o aviso de fonte sob o titulo. Ver o relatorio da C1: a grade
  // nao coube dentro da ilha sem refazer a navegacao do mes.
  if (vistaMes) {
    float x = agX(), xDir = agFim(), yc = P(agColunaUnica() ? agTelefonePx(124) : 110);
    char sub[200];
    const char *aviso = avisoFonte();
    int i, comData = 0;
    for (i = 0; i < n; i++) if (temData(agenda_lista(i))) comData++;
    if (n == 0)
      snprintf(sub, sizeof sub, "%s", i18n("Salve uma série ou comece a assistir para ela aparecer aqui"));
    else
      snprintf(sub, sizeof sub, i18n(comData == 1 ? "%d série com data confirmada · %d acompanhadas"
                                                  : "%d séries com data confirmada · %d acompanhadas"),
               comData, n);
    txC1(agColunaUnica() ? TXT_G28R : TXT_AJ_ESTADO, agColunaUnica() ? 28 : 18,
         sub, AG_INK2, x, yc, agColunaUnica() ? agTelefonePx(28) : 20.0f, xDir - x, 1.0f);
    if (aviso)
      txC1(TXT_ILHA_HORA, 15, aviso, 214, 178, 110, x,
           yc + P(agColunaUnica() ? agTelefonePx(40) : 30),
           agColunaUnica() ? agTelefonePx(20) : 16.0f, xDir - x, 1.0f);
    desenhaBarraCalendario(x, xDir);
    if (n > 0) desenhaCalendarioMensal();
    if (ctxA > 0.01f) desenhaContexto(ctxA);
    return;
  }

  ilhaC1((GfxRect){ L.pnX, L.pnY, L.pnW, L.pnH });
  cabecalhoIlha(&L);

  // ESTADO VAZIO: o sino grande e apagado no lugar da arte, e a frase de
  // sempre (no cabecalho da ilha). A tela diz do que trata antes de ler.
  if (n == 0) {
    if (agColunaUnica()) return;
    GfxRect art = { L.x0, L.artY, L.artW, L.artH };
    float lado = L.artH * 0.42f;
    gfx_cor(art, P(24) / art.h, 1, 1, 1, ajustes_vidro() ? 0.05f : 0.04f);
    agendaui_despertador((GfxRect){ art.x + (art.w - lado) * 0.5f, art.y + (art.h - lado) * 0.5f,
                                    lado, lado }, 0, 0.30f, 0.30f, 0.30f, 0.55f, agora, 0);
    blocoC1(TXT_AJ_ESTADO, 18,
            i18n("Salve uma série ou comece a assistir para ela aparecer aqui"),
            L.x0, L.infoY, 21.0f, L.artW, 30.0f, 3, AG_INK2, 1.0f);
    return;
  }

  if (!agColunaUnica()) desenhaEsquerda(&L, 1.0f);
  desenhaLista(&L);
  if (ctxA > 0.01f) desenhaContexto(ctxA);
}

// O desenho publico liga a escala (piso de 120%) e o layout mede pela tela
// virtual — o mesmo modulo que mede e o que liga, como pede escala.h.
void agendaui_desenhar(Uint32 agora) {
  agInterfaceAtualizar();
#ifdef NV_TOUCH_UI
  toqueAgenda.offset = toqueNoticia.offset = toqueCalendario.offset = toqueManchetes.offset = toqueModal.offset = NULL;
#endif
  AG_ESC_INI();
  desenharNaEscala(agora);
  AG_ESC_FIM();
}


// --- A LISTA DA AGENDA NO PAINEL SOCIAL ---------------------------------------
//
// A aba Agenda do painel desenha EXATAMENTE as pecas da lista C1 desta tela (a
// linha, o cabecalho de grupo, o fio e o ponto), por estas portas, para que as
// duas fiquem iguais por construcao. O painel mede em pixel de 1920x1080, entao
// a escala vale 1. `lsX`/`lsW` sao a esquerda e a largura da lista.
static AgC1 lPainel(float lsX, float lsW, float clipY, float clipH) {
  AgC1 L;
  memset(&L, 0, sizeof L);
  L.lsX = lsX; L.lsW = lsW; L.lsY = clipY; L.lsH = clipH;
  return L;
}
float agendaui_painel_linha_h(void)  { return AG_ROW_H + AG_GAP; }
float agendaui_painel_grupo_h(void)  { return AG_GRP_H + AG_GAP; }
int   agendaui_painel_grupo_de(const AgItem *it) { return grupoDe(it); }
void agendaui_painel_grupo(float lsX, float lsW, int g, float y) {
  AgC1 L = lPainel(lsX, lsW, 0, 0);
  escForcada = 1.0f;
  grupoC1(&L, g, 0, y);
  escForcada = 0.0f;
}
void agendaui_painel_linha(float lsX, float lsW, const AgItem *it, float y, float f) {
  AgC1 L = lPainel(lsX, lsW, 0, 0);
  escForcada = 1.0f;
  linhaC1(&L, it, y, f);
  pontoC1(lsX + 18.0f, y + AG_ROW_H * 0.5f, f);
  escForcada = 0.0f;
}
void agendaui_painel_fio(float lsX, float clipY, float clipH, float y0, float y1) {
  AgC1 L = lPainel(lsX, 0, clipY, clipH);
  escForcada = 1.0f;
  fioC1(&L, y0, y1);
  escForcada = 0.0f;
}

