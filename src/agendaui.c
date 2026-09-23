// Tela AGENDA — a LINHA DO TEMPO das series acompanhadas.
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
#include "agendaui.h"
#include "agenda.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "anim.h"
#include "layout.h"
#include "ajustes.h"
#include "idioma.h"
#include "noticias.h"
#include "catalogo.h"
#include "badges.h"
#include "descoberta.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define AG_TOPO         48.0f
// A ESTACAO e o EIXO. 138 cabe "quarta" abreviada, o numeral em corpo 48 e
// "em 12 semanas" — medido com a linha mais larga que agenda_falta produz.
#define AG_CHIP_W      138.0f
#define AG_EIXO_GAP     34.0f
// 2px e nao 3: o eixo segura a estrutura da tela inteira, e a 3 m um traco de
// 2px com alfa baixo le como fio de pauta — 3px lia como regua desenhada.
#define AG_EIXO_W        2.0f
#define AG_CONT_GAP     48.0f
// Nos menores (eram 16/25): com o eixo fino, o disco de 16 cobria o fio; 13 e
// o diametro em que o disco cheio ainda se distingue do anel vazado do "sem
// data" sem ampliar, e o de hoje desce junto para guardar a proporcao.
#define AG_NO_D         13.0f
#define AG_NO_HOJE      21.0f
#define AG_TRACO        10.0f   // traco e vao do eixo tracejado
// Vermelho de calendario (#e53935), fixo para que HOJE continue sendo HOJE
// mesmo quando o usuario troca o tema de foco para branco, azul ou violeta.
#define AG_HOJE_R        0.898f
#define AG_HOJE_G        0.224f
#define AG_HOJE_B        0.208f

// ALTURA DA LINHA — ela sai do CARTAZ, e nao o contrario.
//
// Era 126, com o cartaz de 70x105 sobrando 10,5 px em cima e embaixo. Nessa
// medida o cartaz ficava mais baixo que o bloco de texto ao lado (titulo em
// TXT_HEADLINE + episodio + apoio medem ~99) e lia como miniatura de apoio, nao
// como a arte da serie — foi o que o dono viu ("aumentar o poster da agenda
// para ficar mais bonito"). Depois foi 168 com cartaz de 144, e agora e 177 com
// cartaz de 153, sempre com 12 de respiro dos dois lados. O cartaz e o elemento
// mais alto da linha, que e o que faz ele virar a arte.
//
// 177 E A ALTURA DOS DOIS ESTADOS. Ate aqui a linha focada somava mais 86px
// para abrir a sinopse por baixo e ia a 254; a sinopse mudou de lugar (ver a
// nota longa no topo) e a linha parou de crescer. Na tela isso vale mais do que
// os 9px que o cartaz cobrou: nos 758px entre listaTopo e listaBase cabem 4,0
// linhas em vez de 4,2, mas nenhuma delas pula de lugar quando o foco anda.
//
// O NUMERAL E O NO NAO PRECISAM DE AJUSTE. Os dois sao desenhados em
// `y + AG_LINHA_H * 0,5` (ver agendaui_desenhar), entao seguem a altura nova
// sozinhos — o alinhamento numeral/no, que e o que faz a coluna de datas ler
// como calendario, e estrutural e nao um literal repetido em tres lugares.
#define AG_LINHA_H     177.0f
#define AG_LINHA_GAP    12.0f
// O CARTAO (set/2026, dono olhando a captura: "diminuir a largura de cada
// item da lista"). Ate aqui a linha ia de xCont ate a margem direita, 1620 px
// de laje para tres linhas de texto que usavam 700. Agora e no maximo 1240 de
// largura e termina em 1500, o que sobra a direita fica LIVRE: o eixo ja da a
// data, e a coluna vazia le como respiro, nao como falta. 16 e o recuo do
// cartao atras do cartaz, o mesmo dos dois lados.
#define AG_CARTAO_MAX  1240.0f
#define AG_CARTAO_FIM  1500.0f
#define AG_CARTAO_PAD    16.0f
// A QUARTA LINHA (citacao/sinopse) comeca AG_EXTRA_SOBE acima da base dos 177
// — o bloco de tres linhas e mais baixo que o cartaz e sobra espaco embaixo
// dele — e no maximo AG_CIT_MAX linhas: a citacao e uma manchete, e uma
// manchete que precisa de tres linhas e um paragrafo.
#define AG_EXTRA_SOBE    20.0f
#define AG_CIT_MAX          2
// O passo das linhas de texto corrido (citacao, sinopse) e o teto do cache de
// quebra (AG_SIN_MAX, tambem o tamanho do buffer de agQuebra — quem pede mais
// linhas que isso recebe AG_SIN_MAX).
#define AG_SIN_LD       30.0f
#define AG_SIN_MAX         3
#define AG_FAIXA_H      72.0f   // cabecalho de mes / separador
#define AG_HOJE_H       94.0f   // a faixa de origem do eixo, um pouco maior
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
// O DISCO DO SINO e a coluna dele, fixa em toda linha para o texto acabar no
// mesmo x em todas — a disciplina de calendario desta tela. 120 cabe o disco
// de 60 com folga e a legenda em duas linhas (ver desenhaConteudo).
#define AG_SINO         60.0f
#define AG_SINO_COL    120.0f

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
// A COR ENTRA NA MEDIDA de proposito. A largura nao depende dela, mas o cache
// de txt_linha e indexado por (estilo, texto, cor): medir numa cor que nao vai
// ser desenhada rasteriza uma segunda copia de cada prefixo de linha. Quem mede
// passa a MESMA cor de quem desenha.
// A QUEBRA E LEMBRADA. agQuebra mede palavra por palavra com txt_linha, e a
// linha em foco a chama duas vezes por quadro (medir e desenhar) para a
// sinopse ou a citacao — na C9 isso era ~11 ms de CPU por quadro em
// agendaui_desenhar (des=11,5 com gfx=0,0 no nuvio-fps.txt de 21/09/2026),
// e a tela parada ficava em 30 fps. Oito entradas bastam: o foco esta numa
// linha, e o texto/largura/estilo so mudam quando ele anda.
#define AGQ_CACHE 8
typedef struct {
  unsigned hash; float larg; int estilo, max, r, g, b, n;
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
        c->max == max && c->r == r && c->g == g && c->b == b) {
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
    c->r = r; c->g = g; c->b = b; c->n = n;
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
  *resto = "";
  if (!s || !s[0] || larg <= 0.0f || max <= 0) return 0;
  while (*p && n < max - 1) {
    char *l = linhas[n];
    l[0] = 0;
    while (*p) {
      const char *ini = p;
      char tent[512];
      size_t nl = strlen(l), np;
      while (*p && *p != ' ' && *p != '\n') p++;
      np = (size_t)(p - ini);
      if (nl + np + 2 >= sizeof tent) { p = ini; break; }
      memcpy(tent, l, nl);
      if (nl) tent[nl++] = ' ';
      memcpy(tent + nl, ini, np);
      tent[nl + np] = 0;
      // A MEDIDA E A MESMA QUE VAI DESENHAR. Estimar por largura media de glifo
      // erra em nome proprio e em maiuscula, e o erro aparece como uma linha
      // estourando a coluna — que e o defeito que este corte existe para evitar.
      if ((float)txt_linha(estilo, tent, r, g, b, 255).w > larg && l[0]) {
        p = ini;
        break;
      }
      memcpy(l, tent, nl + np + 1);
      while (*p == ' ' || *p == '\n') p++;
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
    TxtLinha l = txt_linha(estilo, linhas[i], r, g, b, 255);
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
static int   versaoVista;   // agenda_versao() da ultima montagem; ver agendaui_atualizar
static float animFoco[AG_MAX];
static float scrollY;
static int   sair;
// MENU DE CONTEXTO (segurar OK numa linha): abrir o titulo, ver as ultimas
// noticias, ligar/desligar o lembrete. Toque curto continua sendo o lembrete,
// que e a acao da tela. `ctxAberto` 1 = menu, 2 = painel de noticias.
static int    ctxAberto, ctxFoco, ctxItem, notFoco;
static float  ctxA;
static Uint32 okDesde;
static char   pediuAbrir[40];

// Onde a lista comeca a rolar, e onde ela termina. O cabecalho ocupa o topo e
// nao rola junto: numa TV perder o titulo da tela ao descer uma linha faz a
// pessoa esquecer onde esta.
static float listaTopo(void) { return AG_TOPO + 214.0f; }
static float listaBase(void) { return NV_TELA_H - NV_MARGEM_Y; }

// A linha do separador "sem data prevista": a PRIMEIRA linha da lista que nao
// tem data futura. -1 quando todas tem (ou quando a lista esta vazia).
static int indiceSeparador(void) {
  int i;
  for (i = 0; i < agenda_n(); i++) {
    const AgItem *it = agenda_lista(i);
    if (!it) continue;
    if (!it->dataProx[0] || agenda_dias(it->dataProx) < 0) return i;
  }
  return -1;
}

static int temData(const AgItem *it) {
  return it && it->dataProx[0] && agenda_dias(it->dataProx) >= 0;
}

static float alturaExtra(const AgItem *it);

// A LINHA TEM UMA ALTURA SO, focada ou nao.
//
// Era `AG_LINHA_H + animFoco[i] * 86` — a linha focada abria por baixo para a
// sinopse. A funcao continua existindo, e com o mesmo nome, porque ela e quem
// yDe/alturaDoc/a rolagem chamam: se a altura voltar a depender do estado, e
// aqui que ela volta, e nao espalhada em tres contas que podem discordar.
//
// O QUE MUDOU COM ISSO, alem do tamanho: a rolagem parou de ter alvo movel. O
// `alvoY` de agendaui_atualizar era recalculado a cada quadro porque a altura
// da linha focada estava correndo junto com a mola, e o resultado a 3 m era a
// lista inteira deslizando a cada passo do D-pad. Agora o documento tem altura
// fixa e so o foco anda.
//
// VOLTOU A CRESCER, e de proposito (set/2026): a citacao da ultima noticia
// passou para uma QUARTA LINHA por baixo das tres (a coluna da direita que a
// abrigava colidia com o sino — foto do dono), e a linha em foco abre o que
// alturaExtra mede para ela caber. O alvo movel da rolagem que a nota acima
// descreve continua valendo, mas agora e um degrau de 34 a 68 px numa linha
// so, nao 86 em todas; e a guarda por animFoco poupa a conta nas linhas
// paradas — sem ela yDe faria n*n chamadas por quadro.
static float alturaLinha(int i) {
  if (i < 0 || i >= AG_MAX || animFoco[i] < 0.002f) return AG_LINHA_H;
  return AG_LINHA_H + animFoco[i] * alturaExtra(agenda_lista(i));
}

// As FAIXAS que abrem a linha `i`: a de origem ("HOJE"), a de mes quando o mes
// muda, e a de "sem data prevista". UMA funcao so mede e desenha, para que a
// conta da rolagem nao possa discordar do desenho — foi assim que o separador
// da versao anterior saiu 24px fora do lugar. alturaFaixas e a mesma chamada
// com `desenhar` = 0.
static float faixas(int i, float xChipDir, float xEixo, float xCont, float xDir,
                    float y, int desenhar);

static float alturaFaixas(int i) {
  return faixas(i, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0);
}

// y do TOPO DA LINHA `i` (ja depois das faixas dela) em coordenada de
// DOCUMENTO, antes da rolagem. O(n) por chamada e O(n2) no quadro; com o teto
// de 120 itens sao 14 mil somas de float, que nao aparecem em medicao nenhuma.
static float yDe(int i) {
  float y = 0.0f;
  int j;
  for (j = 0; j < i; j++) y += alturaFaixas(j) + alturaLinha(j) + AG_LINHA_GAP;
  return y + alturaFaixas(i);
}

static float alturaDoc(void) {
  int n = agenda_n();
  if (n <= 0) return 0.0f;
  return yDe(n - 1) + alturaLinha(n - 1);
}

int agendaui_iniciar(void) {
  int i;
  sair = 0;
  foco = 0;
  scrollY = 0.0f;
  ctxAberto = 0; ctxFoco = 0; ctxA = 0.0f; okDesde = 0; notFoco = 0;
  for (i = 0; i < AG_MAX; i++) animFoco[i] = 0.0f;
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

#define CTX_N 3
static int ctxOpcoes(const AgItem *it) { return (it && agenda_pode_lembrar(it->imdb)) ? CTX_N : CTX_N - 1; }

void agendaui_evento(const SDL_Event *e) {
  SDL_Keycode k;
  int n = agenda_n();
  int volta = 0;
  k = e->key.keysym.sym;
  volta = (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE || k == SDLK_DELETE);
  // SEGURAR OK abre o menu de contexto; o toque curto e decidido no KEYUP,
  // como na home (NV_HOLD_MS): so ali se sabe quanto durou.
  if (e->type == SDL_KEYUP && (k == SDLK_RETURN || k == SDLK_KP_ENTER)) {
    Uint32 dur = okDesde ? SDL_GetTicks() - okDesde : 0;
    int era = okDesde != 0;
    okDesde = 0;
    if (!era || ctxAberto) return;
    if (dur >= NV_HOLD_MS) {
      const AgItem *it = agenda_lista(foco);
      if (!it) return;
      ctxAberto = 1; ctxFoco = 0; ctxItem = foco;
      noticias_pedir(it->imdb, it->titulo, it->rede, 1);
    } else alternarLembrete();
    return;
  }
  if (e->type != SDL_KEYDOWN) return;
  if (ctxAberto == 2) {
    int nn = noticias_n(agenda_lista(ctxItem) ? agenda_lista(ctxItem)->imdb : "");
    if (volta) { ctxAberto = 1; return; }
    if (k == SDLK_DOWN && notFoco < nn - 1) notFoco++;
    else if (k == SDLK_UP && notFoco > 0) notFoco--;
    return;
  }
  if (ctxAberto == 1) {
    const AgItem *it = agenda_lista(ctxItem);
    int no = ctxOpcoes(it);
    if (volta) { ctxAberto = 0; return; }
    if (k == SDLK_DOWN && ctxFoco < no - 1) ctxFoco++;
    else if (k == SDLK_UP && ctxFoco > 0) ctxFoco--;
    else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
      if (e->key.repeat) return;
      if (ctxFoco == 0 && it) { snprintf(pediuAbrir, sizeof pediuAbrir, "%s", it->imdb); ctxAberto = 0; }
      else if (ctxFoco == 1) { ctxAberto = 2; notFoco = 0; }
      else { foco = ctxItem; alternarLembrete(); ctxAberto = 0; }
    }
    return;
  }
  if (volta || k == SDLK_LEFT) { sair = 1; return; }
  if (k == SDLK_DOWN && foco < n - 1) foco++;
  else if (k == SDLK_UP && foco > 0)  foco--;
  else if ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && !e->key.repeat) okDesde = SDL_GetTicks();
  // ESQUERDA cai fora daqui e chega ao menu lateral, como nas outras telas.
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
  if (foco >= n) foco = n > 0 ? n - 1 : 0;
  for (i = 0; i < n && i < AG_MAX; i++) {
    float a = (i == foco) ? 1.0f : 0.0f;
    animFoco[i] = reduz ? a
                        : anim_mola(animFoco[i], a, dt,
                                    a > animFoco[i] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  }
  // ROLAGEM MINIMA para a linha focada caber, e nao "alinhar ao topo": alinhar
  // empurra o cabecalho para fora na primeira descida. Mesma regra da
  // Biblioteca — ver a nota 2 no topo de biblioteca.c.
  //
  // Levar em conta as FAIXAS da linha tambem: rolar so o suficiente para a
  // linha aparecer deixaria o cabecalho do mes fora da tela, e o mes e o que da
  // sentido ao numeral logo abaixo.
  topo = listaTopo(); base = listaBase();
  y = yDe(foco) - alturaFaixas(foco);
  h = alturaFaixas(foco) + alturaLinha(foco);
  alvoY = scrollY;
  if (y - alvoY < 0.0f) alvoY = y;
  if (y + h - alvoY > base - topo) alvoY = y + h - (base - topo);
  { float maxY = alturaDoc() - (base - topo);
    if (maxY < 0.0f) maxY = 0.0f;
    if (alvoY > maxY) alvoY = maxY;
    if (alvoY < 0.0f) alvoY = 0.0f; }
  scrollY = reduz ? alvoY : anim_mola(scrollY, alvoY, dt, NV_MOLA_SCROLL);
}

// MAIUSCULA que nao quebra acento. O nome do mes vem em minusculas de
// agenda.c (os MESMOS de desc_data_extenso) e o cabecalho do calendario quer
// caixa alta; toupper() byte a byte transformaria o 0xC3 de "marco" em lixo.
// A regra do bloco Latin-1 e uma so: 0xC3 0xAn/0xBn -> 0xC3 (0xAn - 0x20).
static void maiusc(char *dst, size_t tam, const char *s) {
  size_t i = 0, o = 0;
  if (!tam) return;
  dst[0] = 0;
  if (!s) return;
  while (s[i] && o + 3 < tam) {
    unsigned char c = (unsigned char)s[i];
    if (c == 0xC3 && (unsigned char)s[i + 1] >= 0xA0 &&
                     (unsigned char)s[i + 1] <= 0xBE) {
      dst[o++] = (char)0xC3;
      dst[o++] = (char)((unsigned char)s[i + 1] - 0x20);
      i += 2;
      continue;
    }
    dst[o++] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : (char)c;
    i++;
  }
  dst[o] = 0;
}

// "setembro de 2026" em caixa alta, pronto para o cabecalho.
static void rotuloMes(const char *iso, char *dst, size_t tam) {
  char nome[48];
  int m = agenda_mes(iso), a = agenda_ano(iso);
  dst[0] = 0;
  if (m < 1) return;
  // i18n aqui e nao no desenho: a string sai composta com o ano e nunca casaria
  // com uma chave da tabela (ver idioma.h). O nome do mes e traduzido sozinho.
  maiusc(nome, sizeof nome, i18n(agenda_mes_nome(m)));
  snprintf(dst, tam, "%s %d", nome, a);
}

// Uma regua de 1px atravessando a coluna do conteudo. E a unica linha
// horizontal desta tela: o que separa as series e o espaco, nao um traco.
static void regua(float x0, float x1, float y, float lum, float a) {
  GfxRect r = { x0, y, x1 - x0, 1.0f };
  if (r.w <= 0.0f) return;
  gfx_cor(r, 0.0f, lum, lum, lum, a);
}

// `y` e o topo da FAIXA (antes da linha). Devolve a altura ocupada.
static float faixas(int i, float xChipDir, float xEixo, float xCont, float xDir,
                    float y, int desenhar) {
  const AgItem *it = agenda_lista(i), *ant = i > 0 ? agenda_lista(i - 1) : NULL;
  float usado = 0.0f;
  char rot[80];

  if (i == 0) {
    // A ORIGEM DO EIXO. "Hoje" precisa estar desenhado mesmo quando nenhuma
    // serie estreia hoje: sem ele o primeiro numeral da coluna nao tem contra o
    // que ser lido, e "18" sozinho nao diz se falta um dia ou um ano.
    float ar, ag, ab, yl = y + AG_HOJE_H - 26.0f;
    ajustes_acento(&ar, &ag, &ab);
    if (desenhar) {
      const char *hj = i18n("HOJE");
      // ESPACADO. "HOJE" com 4 letras em corpo 21 e um borrao a tres metros; o
      // espacamento e o que faz quatro maiusculas lerem como rotulo de secao e
      // nao como palavra cortada. Mede-se primeiro (x = -1) para alinhar a
      // direita contra o eixo, como os numerais logo abaixo.
      float lw = txt_tracking(TXT_CAPTION2, hj, 0, 0, 0, -1.0f, 0.0f, 1.0f, 2.4f);
      // O MES DESTA FAIXA E O DA PRIMEIRA LINHA, e nao o de hoje.
      //
      // Aqui nascia um defeito que o dono fotografou: "SETEMBRO 2026" sem uma
      // linha embaixo, e logo em seguida "OUTUBRO 2026" com a primeira serie.
      // Esta faixa escrevia o mes de HOJE, e o cabecalho de virada de mes logo
      // abaixo comparava a primeira linha contra hoje — entao, sempre que a
      // proxima estreia caia num mes seguinte, saiam DOIS cabecalhos e o de
      // cima nao tinha conteudo nenhum. Um cabecalho de secao vazia e uma
      // promessa que a tela nao cumpre.
      //
      // Escrevendo aqui o mes da PRIMEIRA linha, esta faixa vira o cabecalho
      // dela, e a comparacao de virada passa a ser so entre linhas vizinhas
      // (ver o bloco `muda` adiante). Sem data nenhuma na lista, nao ha mes a
      // anunciar e o rotulo some — o "HOJE" e a regua bastam como origem.
      rot[0] = 0;
      if (temData(it)) rotuloMes(it->dataProx, rot, sizeof rot);
      txt_tracking(TXT_CAPTION2, hj, (int)(AG_HOJE_R * 255.0f + 0.5f),
                   (int)(AG_HOJE_G * 255.0f + 0.5f), (int)(AG_HOJE_B * 255.0f + 0.5f),
                   xChipDir - lw, yl - 26.0f, 1.0f, 2.4f);
      if (rot[0])
        txt_tracking(TXT_CAPTION2, rot, 132, 134, 142, xCont, yl - 26.0f,
                     1.0f, 2.0f);
      regua(xEixo, xDir, yl, ar * 0.9f, 0.30f);
      { GfxRect no = { xEixo - AG_NO_HOJE * 0.5f, yl - AG_NO_HOJE * 0.5f,
                       AG_NO_HOJE, AG_NO_HOJE };
        gfx_cor(no, 0.5f, AG_HOJE_R, AG_HOJE_G, AG_HOJE_B, 1.0f); }
    }
    usado += AG_HOJE_H;
  }

  if (i == indiceSeparador()) {
    float yl = y + usado + AG_FAIXA_H - 22.0f;
    if (desenhar) {
      // Caps espacadas, como os cabecalhos de mes: e um rotulo de secao, nao
      // uma frase — e uma voz so para todos os rotulos da tela.
      maiusc(rot, sizeof rot, i18n("Sem data prevista"));
      txt_tracking(TXT_CAPTION2, rot, 132, 134, 142, xCont, yl - 26.0f,
                   1.0f, 2.0f);
      // TRACEJADA, como o eixo daqui para baixo: o tempo acaba de ser
      // interrompido, e uma regua continua diria que a contagem segue.
      { float x = xEixo;
        while (x < xDir) {
          float w = AG_TRACO;
          if (x + w > xDir) w = xDir - x;
          regua(x, x + w, yl, 0.62f, 0.18f);
          x += AG_TRACO * 2.0f;
        } }
    }
    usado += AG_FAIXA_H;
  }

  // A VIRADA DE MES E ENTRE LINHAS VIZINHAS, e so isso. O `i == 0` que existia
  // aqui comparava a primeira linha contra HOJE e duplicava o cabecalho que a
  // faixa de origem ja desenha — era metade do defeito do mes vazio; a outra
  // metade estava la em cima. A primeira linha nao tem virada por definicao: o
  // mes dela e o mes de abertura da tela.
  if (temData(it) && i > 0 && temData(ant)) {
    int muda = agenda_mes(it->dataProx) != agenda_mes(ant->dataProx) ||
               agenda_ano(it->dataProx) != agenda_ano(ant->dataProx);
    if (muda) {
      float yl = y + usado + AG_FAIXA_H - 22.0f;
      if (desenhar) {
        rotuloMes(it->dataProx, rot, sizeof rot);
        txt_tracking(TXT_CAPTION2, rot, 132, 134, 142, xCont, yl - 26.0f,
                     1.0f, 2.0f);
        regua(xEixo, xDir, yl, 0.62f, 0.10f);
      }
      usado += AG_FAIXA_H;
    }
  }
  return usado;
}

// A ESTACAO: dia da semana, numeral do dia, quanto falta. Alinhada A DIREITA
// contra o eixo, que e o que poe os numerais numa reta — alinhada a esquerda,
// "9" e "18" comecam no mesmo x e terminam em x diferentes, e a coluna deixa de
// ser coluna.
//
// Sem data, o numeral da lugar a SITUACAO. Nunca um traco, nunca "a definir".
static void desenhaEstacao(const AgItem *it, float xDir, float yCentro,
                           int hoje, float f) {
  char falta[64];
  int d = temData(it) ? agenda_dias(it->dataProx) : AG_SEM_DATA;
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);

  if (d == AG_SEM_DATA) {
    const char *sit;
    TxtLinha t;
    switch (it->situacao) {
      case AG_ENCERRADA: sit = i18n("Encerrada"); break;
      case AG_CANCELADA: sit = i18n("Cancelada"); break;
      case AG_PRODUCAO:  sit = i18n("Em produção"); break;
      default:           sit = i18n("Sem data"); break;
    }
    t = txt_linha_corta(TXT_CALLOUT, sit, 150, 152, 160, 255, AG_CHIP_W);
    txt_desenhar(t, xDir - (float)t.w, yCentro - (float)t.h * 0.5f);
    return;
  }

  { // A pilha inteira e medida antes de desenhar e centrada no no do eixo: os
    // tres corpos mudam de altura com a fonte, e empilhar por offset fixo ja
    // pos o subtitulo por cima do "g" do titulo em outro ponto deste arquivo.
    char num[8];
    char semM[20];
    float semW;
    TxtLinha sem, dia, fal;
    // O NUMERAL E SEMPRE BRANCO, inclusive no dia de hoje: pedido do dono. O
    // vermelho ja aparece tres vezes na mesma coluna (o rotulo HOJE, o no do
    // eixo e o "hoje" embaixo) e tingir tambem o corpo 48 fazia a estacao de
    // hoje virar um bloco vermelho que puxava mais atencao do que a linha em
    // foco. O dia continua marcado — pelo no e pelo rotulo, nao pelo numero.
    int cN = f > 0.5f ? 246 : 232, cNg = cN, cNb = cN;
    (void)hoje;
    float alt, y;
    snprintf(num, sizeof num, "%d", agenda_dia(it->dataProx));
    agenda_falta(it->dataProx, falta, sizeof falta);
    // O dia da semana em CAPS ESPACADAS, a voz dos rotulos de mes. A altura
    // vem de uma sonda ("Hg", com ascendente e descendente): medir a string
    // real faria a linha de base oscilar entre fileiras ("TER" nao tem
    // descendente, a caixa de "SAB" difere), e a pilha e centrada pela soma
    // das tres alturas — altura instavel aqui e numeral fora da reta.
    maiusc(semM, sizeof semM,
           i18n(agenda_semana_nome(agenda_semana(it->dataProx))));
    sem = txt_linha(TXT_CAPTION2, "Hg", 118, 120, 128, 255);
    semW = txt_tracking(TXT_CAPTION2, semM, 118, 120, 128,
                        -1.0f, 0.0f, 1.0f, 1.8f);
    dia = txt_linha(TXT_TITULO3, num, cN, cNg, cNb, 255);
    // "hoje" e "amanha" saem na cor de realce: sao as duas unicas respostas que
    // fazem alguem mudar o que ia assistir hoje a noite.
    fal = txt_linha_corta(TXT_CAPTION2, falta,
                          d == 0 ? (int)(AG_HOJE_R * 255.0f + 0.5f) :
                          d == 1 ? (int)(ar * 255.0f + 0.5f) : 146,
                          d == 0 ? (int)(AG_HOJE_G * 255.0f + 0.5f) :
                          d == 1 ? (int)(ag * 255.0f + 0.5f) : 148,
                          d == 0 ? (int)(AG_HOJE_B * 255.0f + 0.5f) :
                          d == 1 ? (int)(ab * 255.0f + 0.5f) : 156, 255,
                          AG_CHIP_W);
    alt = (float)sem.h + 2.0f + (float)dia.h + 2.0f + (float)fal.h;
    y = yCentro - alt * 0.5f;
    txt_tracking(TXT_CAPTION2, semM, 118, 120, 128, xDir - semW, y, 1.0f, 1.8f);
    y += (float)sem.h + 2.0f;
    txt_desenhar(dia, xDir - (float)dia.w, y); y += (float)dia.h + 2.0f;
    txt_desenhar(fal, xDir - (float)fal.w, y);
  }
}

// Segunda linha do conteudo: o episodio. Vazio quando nao ha nada verdadeiro a
// dizer — e ai a linha some.
static void linhaEpisodio(const AgItem *it, char *dst, size_t tam) {
  dst[0] = 0;
  if (it->dataProx[0] && it->temporada > 0 && it->episodio > 0) {
    if (it->nomeEp[0])
      snprintf(dst, tam, i18n("T%dE%d · %s"), it->temporada, it->episodio, it->nomeEp);
    else
      snprintf(dst, tam, i18n("T%dE%d"), it->temporada, it->episodio);
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

// --- O QUE O CATALOGO ACRESCENTA A LINHA ------------------------------------
//
// O corpo /tv/<id> (agenda.h) da rede, duracao, temporadas, nome e sinopse do
// episodio. Tres coisas que o dono pediu ("mais informacoes") ele NAO da, mas
// o catalogo local ja tem para toda serie seguida: o GENERO (CatItem.genero,
// "Programa de TV · Drama · Misterio"), o ANO (os quatro digitos de
// CatItem.meta, "2022 · 3 temporadas") e a NOTA (CatItem.nota, 0..100, a mesma
// que a home e o detalhe mostram com a marca do IMDb). Zero pedido de rede:
// cat_indice_por_imdb e uma busca em memoria.
static const CatItem *agendaCatalogoItem(const AgItem *it) {
  int i;
  if (!it || !it->imdb[0]) return NULL;
  i = cat_indice_por_imdb(it->imdb);
  return i >= 0 ? cat_item(i) : NULL;
}

// O ANO sao os primeiros quatro digitos seguidos de `meta`. "" quando nao ha —
// e ai o selo some, nunca "----".
static void agendaCatalogoAno(const CatItem *ci, char *dst, size_t tam) {
  const char *p;
  dst[0] = 0;
  if (!ci) return;
  for (p = ci->meta; p[0] && p[1] && p[2] && p[3]; p++)
    if (p[0] >= '1' && p[0] <= '2' && p[1] >= '0' && p[1] <= '9' &&
        p[2] >= '0' && p[2] <= '9' && p[3] >= '0' && p[3] <= '9') {
      snprintf(dst, tam, "%.4s", p); return;
    }
}

// O PRIMEIRO GENERO DE VERDADE. O primeiro segmento de `genero` e o tipo
// ("Programa de TV", "Filme") e nao conta — ver a nota em catalogo.c:1259; o
// segundo e o genero principal. Um so, porque a linha ja diz rede, duracao e
// temporadas e a 3 m tres generos viram uma frase que ninguem le.
static void agendaCatalogoGenero(const CatItem *ci, char *dst, size_t tam) {
  const char *p, *fim;
  size_t n = 0;
  dst[0] = 0;
  if (!ci || !ci->genero[0]) return;
  p = strstr(ci->genero, "\xc2\xb7");
  if (!p) return;
  p += 2;
  while (*p == ' ') p++;
  fim = strstr(p, "\xc2\xb7");
  if (!fim) fim = p + strlen(p);
  while (fim > p && fim[-1] == ' ') fim--;
  while (p < fim && n + 1 < tam) dst[n++] = *p++;
  dst[n] = 0;
}

// Terceira linha: o MARCO primeiro, depois rede · duracao · temporadas · genero.
// O marco vem na frente porque e a unica coisa desta linha que muda de um
// episodio para o seguinte — "Final da temporada" e a razao de alguem marcar
// o lembrete. O genero fecha a linha: e o que menos muda.
static void linhaApoio(const AgItem *it, char *dst, size_t tam) {
  char marco[64], apoio[240], genero[80];
  dst[0] = 0;
  agenda_marco(it, marco, sizeof marco);
  agenda_apoio(it, apoio, sizeof apoio);
  agendaCatalogoGenero(agendaCatalogoItem(it), genero, sizeof genero);
  // Sem catalogo (serie que veio do progresso ou de um lembrete), o genero que
  // o fio da agenda trouxe do TMDB ou do Cinemeta. O do catalogo manda quando
  // existe: e o que o resto do app mostra para o mesmo titulo.
  if (!genero[0] && it->genero[0]) snprintf(genero, sizeof genero, "%s", it->genero);
  if (genero[0]) {
    size_t n = strlen(apoio);
    if (n) snprintf(apoio + n, sizeof apoio - n, " \xc2\xb7 %s", genero);
    else   snprintf(apoio, sizeof apoio, "%s", genero);
  }
  if (marco[0] && apoio[0]) snprintf(dst, tam, "%s \xc2\xb7 %s", marco, apoio);
  else if (marco[0])        snprintf(dst, tam, "%s", marco);
  else                      snprintf(dst, tam, "%s", apoio);
}

// A QUARTA LINHA, so na linha em foco: a ultima manchete como citacao (dono,
// 21/09/2026: "embaixo, em forma de citacao, a ultima noticia e escrito hold
// to read more") ou, sem manchete, a sinopse do proximo episodio. `noticia`
// diz qual das duas saiu — e o que decide se a dica de segurar OK aparece.
static int linhaExtra(const AgItem *it, char *dst, size_t tam, int *noticia) {
  const Noticia *nt;
  *noticia = 0;
  dst[0] = 0;
  if (!it) return 0;
  nt = noticias_item(it->imdb, 0);
  if (nt && nt->titulo[0]) {
    snprintf(dst, tam, "\xe2\x80\x9c%s\xe2\x80\x9d", nt->titulo);
    *noticia = 1;
    return 1;
  }
  if (it->sinopse[0]) { snprintf(dst, tam, "%s", it->sinopse); return 1; }
  return 0;
}

// --- AS MEDIDAS DO CARTAO, num lugar so ------------------------------------
//
// O cartao vai de xCont - AG_CARTAO_PAD ate no maximo AG_CARTAO_FIM (1500) e
// nunca passa de AG_CARTAO_MAX (1240) de largura. Dentro dele, da esquerda
// para a direita: cartaz, 26 de vao, o TEXTO, 20 de folga e a COLUNA DO SINO
// (AG_SINO_COL, fixa). Quem mede a altura (alturaLinha, para a rolagem) e
// quem desenha (desenhaConteudo) chamam a MESMA conta — separadas, elas ja
// discordaram neste arquivo e o sintoma foi texto passando por baixo do icone.
typedef struct {
  float xCartao, wCartao;   // o retangulo pintado
  float xTexto, wTexto;     // a coluna de texto (titulo, episodio, apoio, citacao)
  float xSino;              // centro da coluna do sino
} AgMedidas;

static AgMedidas medidas(float xCont, float xDir) {
  AgMedidas m;
  float fim = xDir;
  if (fim > AG_CARTAO_FIM) fim = AG_CARTAO_FIM;
  if (fim > xCont - AG_CARTAO_PAD + AG_CARTAO_MAX) fim = xCont - AG_CARTAO_PAD + AG_CARTAO_MAX;
  m.xCartao = xCont - AG_CARTAO_PAD;
  m.wCartao = fim - m.xCartao;
  m.xTexto  = xCont + AG_CARTAZ_W + 26.0f;
  m.wTexto  = fim - AG_CARTAO_PAD - AG_SINO_COL - 20.0f - m.xTexto;
  if (m.wTexto < 220.0f) m.wTexto = 220.0f;
  m.xSino   = fim - AG_CARTAO_PAD - AG_SINO_COL * 0.5f;
  return m;
}

// A conta de agendaui_desenhar, repetida aqui para alturaLinha nao precisar
// de parametro: xCont e xDir saem so de ajustes e de layout.h.
static AgMedidas medidasDaTela(void) {
  float x = ajustes_conteudo_x();
  float xCont = x + AG_CHIP_W + AG_EIXO_GAP + AG_CONT_GAP;
  return medidas(xCont, NV_TELA_W - NV_MARGEM_X);
}

// QUANTO A LINHA EM FOCO CRESCE para caber a quarta linha: 0 sem citacao nem
// sinopse. Medido com a MESMA largura e cor que o desenho usa, para que a
// entrada do agQuebra seja a mesma (uma medida em cor diferente rasterizaria
// uma segunda copia de cada prefixo — ver a nota de agQuebra).
static float alturaExtra(const AgItem *it) {
  char extra[300];
  int noticia, n;
  AgMedidas m;
  int qc = ajustes_tinta_foco2();
  if (!linhaExtra(it, extra, sizeof extra, &noticia)) return 0.0f;
  m = medidasDaTela();
  n = agendaui_sinopse_linhas(TXT_CAPTION, extra, m.wTexto, AG_CIT_MAX, qc, qc, qc);
  if (n < 1) n = 1;
  // 12 de vao acima, as linhas em passo AG_SIN_LD, 8 ate a dica (so com
  // noticia), 16 de respiro ate a borda do cartao — menos os 20 que a quarta
  // linha ja ganha ao comecar acima da base dos 177 (ver desenhaConteudo).
  return 12.0f + AG_SIN_LD * (float)(n - 1) + 26.0f
       + (noticia ? 8.0f + 26.0f : 0.0f) + 16.0f - AG_EXTRA_SOBE;
}

// --- O SINO -------------------------------------------------------------------
//
// O MESMO DESENHO do botao de lembrete do detalhe (desenhaLembrete em
// detail.c, 21/09/2026), no tamanho da linha: um DISCO e o glifo do sino por
// cima, e o estado vai pela cor do disco — nao por ondas, tremor ou orelhas.
//
//   DESLIGADO, fora do foco   sem disco; so o contorno do sino em cinza 170,
//                             o piso de cinza sobre escuro (regra do dono)
//   LIGADO, fora do foco      disco CHEIO na cor de realce, sino na tinta que
//                             contrasta com ela (ajustes_acento_tinta) — o
//                             mesmo "armado" do detalhe
//   DESLIGADO, em foco        sobre a pilula de realce: o sino na tinta
//                             secundaria, sem disco
//   LIGADO, em foco           o disco INVERTIDO: tinta do realce como fundo e
//                             o sino na cor de realce. Um disco de realce
//                             sobre a pilula de realce sumiria; invertido ele
//                             continua sendo "o disco", que e o sinal.
// Quatro estados, e em nenhum deles o sinal e so cor: ligado tem DISCO,
// desligado nao tem. E o que sobrevive a "reduzir animacoes" e a quem nao
// separa verde de cinza.
//
// O glifo ocupa 0,54 do disco (32 px em 60): no detalhe e 0,333 de 96, mas la
// o disco e um botao entre botoes; aqui ele e a unica coisa na coluna e a 3 m
// um sino de 20 px dentro de 60 lia como ponto.
static void desenhaSino(float cx, float cy, int ligado, int emFoco, float a) {
  float ar, ag, ab, tinta = ajustes_acento_tinta(&ar, &ag, &ab);
  float g = AG_SINO * 0.54f;
  GfxRect disco = { cx - AG_SINO * 0.5f, cy - AG_SINO * 0.5f, AG_SINO, AG_SINO };
  GfxRect ic = { cx - g * 0.5f, cy - g * 0.5f, g, g };
  float cr, cg, cb;
  if (ligado && emFoco) {
    gfx_cor(disco, 0.5f, tinta, tinta, tinta, a);
    cr = ar; cg = ag; cb = ab;
  } else if (ligado) {
    gfx_cor(disco, 0.5f, ar, ag, ab, a);
    cr = cg = cb = tinta;
  } else if (emFoco) {
    cr = cg = cb = (float)ajustes_tinta_foco2() / 255.0f;
  } else {
    cr = cg = cb = 170.0f / 255.0f;
  }
  agendaui_despertador(ic, ligado, cr, cg, cb, a, relogio, trocaEm);
}

// O CONTEUDO DA LINHA: o cartao, o cartaz, tres linhas de texto (quatro em
// foco) e a coluna do sino.
//
// O CARTAO EXISTE NOS DOIS ESTADOS (dono, 21/09/2026, olhando a captura com
// as linhas ate a borda: "diminuir a largura de cada item da lista"). Fora do
// foco e a superficie 0.10/0.11/0.13 dos selos (badges.h), com o mesmo canto
// da pilula de foco; em foco e a PILULA NA COR DE REALCE com a luz difusa
// atras (GFX_SOMBRA a 0,35 — a mesma do menu lateral e dos botoes do
// detalhe). O texto sobre o realce vai na tinta de ajustes_tinta_foco.
//
// FILL-RATE: sao quatro cartoes visiveis de 1240x177 (0,42 tela) e uma luz de
// (1240+212)x(177+212) atras do focado (0,27 tela) — 0,7 tela a mais que a
// versao sem superficie, dentro do teto de +1 tela desta rodada. A luz usa
// 0,6 de altura de folga e nao os 0,9 do menu porque a pilula aqui e larga: a
// mancha do menu numa peca de 1240 px viraria meia tela.
//
// AS TRES LINHAS ficam CENTRADAS nos 177 de base, focada ou nao — a quarta
// linha (citacao) entra POR BAIXO delas, a partir de AG_EXTRA_SOBE acima da
// base dos 177, e e so ela que faz o cartao crescer. Centrar o bloco inteiro
// na altura nova faria titulo e cartaz pularem 40 px para cima a cada passo do
// foco, que e o movimento que mais se ve a 3 m.
static void desenhaConteudo(const AgItem *it, float xCont, float xDir,
                            float y, float h, float f) {
  AgMedidas m = medidas(xCont, xDir);
  char ep[220], apoio[300], extra[300], ano[8];
  const CatItem *ci = agendaCatalogoItem(it);
  int noticia = 0, temExtra;
  int emFoco = f > 0.5f;
  int c  = emFoco ? ajustes_tinta_foco()  : 246;
  int c2 = emFoco ? ajustes_tinta_foco2() : 190;
  int c3 = emFoco ? ajustes_tinta_foco2() : 170;
  float x = xCont, tx = m.xTexto, textW = m.wTexto;
  float yb, blocoH, apoioW, selosW = 0.0f, imdbW = 0.0f, l3h;
  TxtLinha t, l2, l3;
  GfxRect card = { m.xCartao, y, m.wCartao, h };

  // A SUPERFICIE. O canto e o dos selos e do menu (20 px sobre a altura
  // base) e fica em pixels: com a linha crescendo, `20 / h` mantem o raio.
  gfx_cor(card, 20.0f / h, 0.10f, 0.11f, 0.13f, 1.0f);
  if (f > 0.01f) {
    float ar, ag, ab;
    GfxRect luz = { card.x - card.h * 0.6f, card.y - card.h * 0.6f,
                    card.w + card.h * 1.2f, card.h * 2.2f };
    ajustes_acento(&ar, &ag, &ab);
    gfx_rect(luz, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, ar, ag, ab, 0.35f * f);
    gfx_cor(card, 20.0f / h, ar, ag, ab, f);
  }

  { GfxRect cz = { x, y + (AG_LINHA_H - AG_CARTAZ_H) * 0.5f,
                   AG_CARTAZ_W, AG_CARTAZ_H };
    GLuint tex = it->poster[0] ? tex_obter_larg(it->poster, AG_CARTAZ_W) : 0;
    if (tex) {
      gfx_tex_aspect_atual = AG_CARTAZ_W / AG_CARTAZ_H;
      gfx_rect(cz, tex, GFX_CARD, 0, 0, 0, AG_CARTAZ_RAIO, 0, 0, 0, 1.0f);
      gfx_tex_aspect_atual = 0.0f;
    } else {
      // Sem cartaz, um retangulo neutro — e nao a inicial do titulo: a linha ja
      // tem o nome escrito ao lado, e a letra viraria ruido.
      gfx_cor(cz, AG_CARTAZ_RAIO, 0.18f, 0.19f, 0.21f, 1.0f);
    } }

  linhaEpisodio(it, ep, sizeof ep);
  linhaApoio(it, apoio, sizeof apoio);
  agendaCatalogoAno(ci, ano, sizeof ano);
  // O ANO fecha o texto de apoio como mais um segmento, e nao como selo: o
  // selo neutro tem o MESMO fundo 0.10/0.11/0.13 do cartao e sumia na linha
  // sem foco (captura de 22/09) — um selo que so existe em foco le como
  // defeito. A MARCA DO IMDb com a nota vem DEPOIS do texto, e o texto e
  // cortado para ela caber sempre — uma marca cortada nao existe.
  if (ano[0]) {
    size_t n = strlen(apoio);
    if (n) snprintf(apoio + n, sizeof apoio - n, " \xc2\xb7 %s", ano);
    else   snprintf(apoio, sizeof apoio, "%s", ano);
  }
  if (ci && ci->nota > 0) imdbW = badge_imdb_largura(ci->nota);
  selosW = imdbW > 0.0f ? imdbW + BADGE_GAP : 0.0f;
  apoioW = textW - selosW;
  if (apoioW < 180.0f) apoioW = 180.0f;
  t  = txt_linha_corta(TXT_HEADLINE, it->titulo[0] ? it->titulo : i18n("Série"),
                       c, c, c, 255, textW);
  l2 = ep[0] ? txt_linha_corta(TXT_CAPTION, ep, c2, c2, c2, 255, textW)
             : (TxtLinha){ 0, 0, 0 };
  l3 = apoio[0] ? txt_linha_corta(TXT_CAPTION2, apoio, c3, c3, c3, 255, apoioW)
                : (TxtLinha){ 0, 0, 0 };
  // A terceira linha tem a altura do SELO quando ha selo: o texto centra nele.
  l3h = selosW > 0.0f ? BADGE_H : (float)l3.h;

  blocoH = (float)t.h + (l2.h ? 7.0f + (float)l2.h : 0.0f)
                      + (l3h > 0.0f ? 6.0f + l3h : 0.0f);
  yb = y + (AG_LINHA_H - blocoH) * 0.5f;
  txt_desenhar(t, tx, yb); yb += (float)t.h;
  if (l2.h) { yb += 7.0f; txt_desenhar(l2, tx, yb); yb += (float)l2.h; }
  if (l3h > 0.0f) {
    float sx = tx;
    yb += 6.0f;
    if (l3.h) { txt_desenhar(l3, tx, yb + (l3h - (float)l3.h) * 0.5f); sx += (float)l3.w + BADGE_GAP; }
    if (imdbW > 0.0f) badge_imdb(sx, yb, ci->nota, emFoco, 1.0f);
    yb += l3h;
  }

  // --- A QUARTA LINHA, so em foco ------------------------------------------
  temExtra = emFoco && linhaExtra(it, extra, sizeof extra, &noticia);
  if (temExtra) {
    int qc = ajustes_tinta_foco2();
    float yq = y + AG_LINHA_H - AG_EXTRA_SOBE + 12.0f;
    int n = agendaui_sinopse(TXT_CAPTION, extra, tx, yq, textW, AG_SIN_LD,
                             AG_CIT_MAX, qc, qc, qc, f);
    if (noticia) {
      TxtLinha dica = txt_linha(TXT_CAPTION2, i18n("Segure OK para ler mais"), qc, qc, qc, 255);
      txt_desenhar_alpha(dica, tx, yq + AG_SIN_LD * (float)(n - 1) + 26.0f + 8.0f, f);
    }
  }

  // --- A COLUNA DO SINO -------------------------------------------------------
  //
  // FORA DO FOCO so aparece LIGADO: um sino apagado em toda linha seria um
  // campo de ruido, e o que interessa de longe e quais poucas linhas estao
  // marcadas. NA LINHA FOCADA aparece nos dois estados com a legenda
  // ("Lembrete ativo" / "Lembrar-me"): e a linha que o OK vai atingir, e ali
  // o desligado deixa de ser ruido e passa a ser o alvo do botao.
  //
  // A LEGENDA QUEBRA EM DUAS LINHAS dentro da coluna de 120: "Lembrete ativo"
  // mede ~153 em TXT_CAPTION2, e cortar em "Lembrete a…" deixaria a mesma
  // palavra nos dois estados. Icone e legenda centrados no MESMO eixo, o da
  // coluna, e o par inteiro centrado nos 177 de base.
  { int pode = agenda_pode_lembrar(it->imdb);
    int mostra = it->lembrete || (emFoco && pode);
    if (mostra) {
      char lin[AG_SIN_MAX][512];
      const char *resto = "";
      int nl = 0, k, cl = ajustes_tinta_foco2();
      float alt = AG_SINO, yl;
      if (emFoco && pode)
        nl = agQuebra(TXT_CAPTION2, it->lembrete ? i18n("Lembrete ativo") : i18n("Lembrar-me"),
                      AG_SINO_COL, 2, lin, &resto, cl, cl, cl) + (resto[0] ? 1 : 0);
      if (nl) alt += 8.0f + 24.0f * (float)nl;
      yl = y + (AG_LINHA_H - alt) * 0.5f;
      desenhaSino(m.xSino, yl + AG_SINO * 0.5f, it->lembrete, emFoco, 1.0f);
      yl += AG_SINO + 8.0f;
      for (k = 0; k < nl; k++) {
        TxtLinha l = (k < nl - 1 || !resto[0])
                   ? txt_linha(TXT_CAPTION2, lin[k], cl, cl, cl, 255)
                   : txt_linha_corta(TXT_CAPTION2, resto, cl, cl, cl, 255, AG_SINO_COL);
        txt_desenhar_alpha(l, m.xSino - (float)l.w * 0.5f, yl + 24.0f * (float)k, f);
      }
    } }
}

// UM ANEL EXATO SAO DOIS DISCOS CONCENTRICOS, e nao GFX_ANEL.
//
// GFX_ANEL contorna o MESMO SDF de retangulo arredondado do resto do shader, e
// num diametro pequeno o raio 0,5 nao chega a fechar a curva: o no "sem data"
// de 20px saia com topo, base e laterais RETOS e quatro cantos arredondados —
// um squircle, nao um circulo. Esta fotografado na captura -semdata, ampliado.
// O comentario que estava aqui culpava a ESPESSURA e afinava o anel; afinar nao
// arruma a forma, so deixa o defeito menor. E o limite e mais alto do que
// parece — o mesmo squircle ja apareceu num anel de 124px noutra tela deste
// repositorio. Entao a regra e: GFX_ANEL nao serve para circulo, em tamanho
// nenhum.
//
// Dois discos resolvem exato em qualquer diametro, porque gfx_cor com raio 0,5
// num QUADRADO e um circulo de verdade (o SDF vira uma capsula que fecha nos
// dois eixos). O de fora leva a cor do anel, o de dentro leva a cor do FUNDO DA
// PAGINA — e isso conserta de brinde um segundo defeito da mesma captura: o
// eixo tracejado atravessava o interior do anel e fazia o "sem data" parecer um
// "O" cortado ao meio.
//
// O disco de dentro pode ser opaco porque o no NUNCA cai sobre a superficie
// clara: a laje da linha focada comeca em xCont - 16 e o eixo esta 48px antes
// dela (AG_CONT_GAP). Atras do no ha sempre o fundo da tela.
#define AG_FUNDO 0.051f   // #0D0D0D, o mesmo do glClearColor e do DESIGN.md

static void aneis(float xEixo, float y, float d, float esp,
                  float r, float g, float b, float a) {
  float di = d - esp * 2.0f;
  GfxRect fora   = { xEixo - d * 0.5f,  y - d * 0.5f,  d,  d  };
  GfxRect dentro = { xEixo - di * 0.5f, y - di * 0.5f, di, di };
  if (d <= 0.0f) return;
  gfx_cor(fora, 0.5f, r, g, b, a);
  if (di > 0.0f) gfx_cor(dentro, 0.5f, AG_FUNDO, AG_FUNDO, AG_FUNDO, 1.0f);
}

// O NO do eixo. Disco cheio para data confirmada, ANEL VAZADO para o que nao
// tem data: a diferenca continua legivel sem cor, que e a regra desta tela
// inteira.
static void desenhaNo(float xEixo, float y, const AgItem *it, int hoje, float f) {
  float ar, ag, ab, d = AG_NO_D + f * 5.0f;
  float lum = 0.52f + f * 0.40f;
  GfxRect no;
  ajustes_acento(&ar, &ag, &ab);
  if (hoje) d = AG_NO_HOJE;
  no.w = no.h = d;
  no.x = xEixo - d * 0.5f;
  no.y = y - d * 0.5f;
  if (!temData(it)) {
    // 3px de espessura sobre um no de 17 a 25: fino o bastante para ler como
    // contorno e nao como rosquinha, grosso o bastante para sobreviver ao
    // downscale de uma TV que nao esta em 1080 nativo.
    aneis(xEixo, y, d + 4.0f, 3.0f, lum, lum, lum, 0.9f);
    return;
  }
  if (hoje) {
    // A AURA de hoje: um anel largo e apagado em volta do no. E o unico ponto
    // da tela com dois elementos concentricos, e por isso o olho cai nele antes
    // de ler qualquer palavra — que e exatamente o trabalho de uma ancora.
    //
    // DESENHADA ANTES DO NO, ao contrario de como estava: o disco de dentro da
    // aura e opaco, entao ele apagaria o no se viesse depois.
    aneis(xEixo, y, d * 1.9f, 3.0f,
          AG_HOJE_R, AG_HOJE_G, AG_HOJE_B, 0.38f);
  }
  if (hoje) gfx_cor(no, 0.5f, AG_HOJE_R, AG_HOJE_G, AG_HOJE_B, 1.0f);
  else if (f > 0.5f) gfx_cor(no, 0.5f, ar, ag, ab, 1.0f);
  else                  gfx_cor(no, 0.5f, lum, lum, lum, 1.0f);
}

// O EIXO. Continuo ate o separador, tracejado depois dele — e o tracejado nao e
// enfeite: dali para baixo nao ha tempo, so situacao.
static void desenhaEixo(float xEixo, float y0, float y1, int tracejado) {
  // SO O QUE ESTA NA TELA. O eixo vai do "hoje" ao fim do documento, e o
  // documento de 32 series tem milhares de pixels; tracejado, isso eram
  // centenas de retangulos por quadro, quase todos fora do recorte — a tela
  // parada ficava em 46 fps na C9 (medido 21/09/2026) enquanto a home faz 60.
  // O recorte de gfx_recorte ja esconde o excesso; aqui e so nao pagar por ele.
  if (y0 < -AG_TRACO * 2.0f) y0 = -AG_TRACO * 2.0f + fmodf(y0, AG_TRACO * 2.0f);
  if (y1 > NV_TELA_H + AG_TRACO * 2.0f) y1 = NV_TELA_H + AG_TRACO * 2.0f;
  if (y1 <= y0) return;
  if (!tracejado) {
    GfxRect r = { xEixo - AG_EIXO_W * 0.5f, y0, AG_EIXO_W, y1 - y0 };
    gfx_cor(r, 0.0f, 0.70f, 0.71f, 0.76f, 0.22f);
    return;
  }
  { float y = y0;
    while (y < y1) {
      float h = AG_TRACO;
      GfxRect r;
      if (y + h > y1) h = y1 - y;
      r.x = xEixo - AG_EIXO_W * 0.5f; r.y = y;
      r.w = AG_EIXO_W; r.h = h;
      gfx_cor(r, 0.0f, 0.70f, 0.71f, 0.76f, 0.22f);
      y += AG_TRACO * 2.0f;
    } }
}

// --- menu de contexto e noticias ----------------------------------------------
//
// Cartao central com tres linhas (abrir o titulo / ultimas noticias / lembrete)
// e, atras dele, um painel de manchetes do Google News (noticias.h): so
// manchete, veiculo e dia — a TV nao abre link, entao e leitura, nao indice.
//
// NA LINGUAGEM DOS PAINEIS FLUTUANTES (menu.c, 21/09/2026): superficie
// 0.055/0.058/0.068 quase opaca com canto de 28, a luz na cor de realce
// entrando pelo topo (gfx_luz_canto, recortada pelo proprio canto) e a linha
// em foco como PILULA de realce com a mancha difusa atras (GFX_SOMBRA 0,35).
// Antes era uma laje 0.075 chapada com uma pilula sem luz — o unico painel do
// app que ainda nao falava a lingua do menu.
#define AGC_W     760.0f
#define AGC_LINHA  84.0f
#define AGN_W    1180.0f
#define AGN_LINHA 108.0f
static void painelFlutuante(GfxRect r, float ar, float ag, float ab, float a) {
  gfx_cor(r, 28.0f / r.h, 0.055f, 0.058f, 0.068f, 0.96f * a);
  gfx_luz_canto(r, 28.0f / r.h, r.w * 0.5f, 0.0f, r.w * 0.9f, ar, ag, ab, 0.22f * a);
}
// A pilula de foco das linhas dos dois paineis: luz atras, pilula na cor de
// realce por cima. O raio e 16 px sobre a altura da linha, como era.
static void pilulaFoco(GfxRect lr, float ar, float ag, float ab, float a) {
  GfxRect luz = { lr.x - lr.h * 0.9f, lr.y - lr.h * 0.9f, lr.w + lr.h * 1.8f, lr.h * 2.8f };
  gfx_rect(luz, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, ar, ag, ab, 0.35f * a);
  gfx_cor(lr, 16.0f / lr.h, ar, ag, ab, a);
}
static void desenhaContexto(float a) {
  const AgItem *it = agenda_lista(ctxItem);
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  float ar, ag, ab, tinta = ajustes_acento_tinta(&ar, &ag, &ab);
  int tf = ajustes_tinta_foco(), tf2 = ajustes_tinta_foco2();
  if (!it) return;
  gfx_cor(tela, 0.0f, 0.0f, 0.0f, 0.0f, 0.62f * a);
  if (ctxAberto == 2) {
    int n = noticias_n(it->imdb), i;
    int resp = noticias_respondeu(it->imdb);
    float h = 150.0f + (float)(n > 0 ? n : 1) * AGN_LINHA + 40.0f;
    float maxH = NV_TELA_H - 2.0f * NV_MARGEM_Y;
    GfxRect r;
    TxtLinha t;
    float y;
    if (h > maxH) h = maxH;
    r = (GfxRect){ (NV_TELA_W - AGN_W) * 0.5f, (NV_TELA_H - h) * 0.5f + (1.0f - a) * 24.0f, AGN_W, h };
    painelFlutuante(r, ar, ag, ab, a);
    t = txt_linha_corta(TXT_TITULO2, it->titulo, 246, 247, 250, 255, AGN_W - 96.0f);
    txt_desenhar_alpha(t, r.x + 48.0f, r.y + 40.0f, a);
    t = txt_linha(TXT_CAPTION, i18n("Últimas notícias · Google News"), 150, 153, 162, 255);
    txt_desenhar_alpha(t, r.x + 48.0f, r.y + 40.0f + 58.0f, a);
    y = r.y + 150.0f;
    if (!resp) {
      t = txt_linha(TXT_BODY, i18n("Procurando…"), 170, 174, 184, 255);
      txt_desenhar_alpha(t, r.x + 48.0f, y + 30.0f, a);
    } else if (n == 0) {
      t = txt_linha(TXT_BODY, i18n("Nada publicado recentemente sobre este título."), 170, 174, 184, 255);
      txt_desenhar_alpha(t, r.x + 48.0f, y + 30.0f, a);
    }
    gfx_recorte(r.x, y, r.w, r.y + r.h - 24.0f - y);
    { int vis = (int)((r.y + r.h - 24.0f - y) / AGN_LINHA);
      int ini = notFoco - vis + 1; if (ini < 0) ini = 0;
      for (i = ini; i < n; i++) {
        const Noticia *nt = noticias_item(it->imdb, i);
        float ly = y + (float)(i - ini) * AGN_LINHA;
        int f = (i == notFoco);
        GfxRect lr = { r.x + 32.0f, ly, r.w - 64.0f, AGN_LINHA - 10.0f };
        if (!nt) continue;
        if (ly > r.y + r.h) break;
        if (f) pilulaFoco(lr, ar, ag, ab, a);
        t = txt_linha_corta(TXT_CALLOUT, nt->titulo, f ? tf : 238, f ? tf : 240, f ? tf : 244, 255, lr.w - 40.0f);
        txt_desenhar_alpha(t, lr.x + 20.0f, ly + 14.0f, a);
        { char sub[140];
          if (nt->data[0] && nt->fonte[0]) snprintf(sub, sizeof sub, "%s · %s", nt->data, nt->fonte);
          else snprintf(sub, sizeof sub, "%s%s", nt->data, nt->fonte);
          t = txt_linha_corta(TXT_CAPTION, sub, f ? tf2 : 150, f ? tf2 : 153, f ? tf2 : 162, 255, lr.w - 40.0f);
          txt_desenhar_alpha(t, lr.x + 20.0f, ly + 14.0f + 40.0f, a); }
      } }
    gfx_sem_recorte();
    (void)tinta;
    return;
  }
  { int no = ctxOpcoes(it), i;
    const char *rot[CTX_N];
    float h = 118.0f + (float)no * AGC_LINHA + 24.0f;
    GfxRect r = { (NV_TELA_W - AGC_W) * 0.5f, (NV_TELA_H - h) * 0.5f + (1.0f - a) * 24.0f, AGC_W, h };
    TxtLinha t;
    rot[0] = i18n("Abrir o título");
    rot[1] = i18n("Últimas notícias");
    rot[2] = it->lembrete ? i18n("Desligar o lembrete") : i18n("Lembrar-me");
    painelFlutuante(r, ar, ag, ab, a);
    t = txt_linha_corta(TXT_TITULO3, it->titulo, 246, 247, 250, 255, AGC_W - 96.0f);
    txt_desenhar_alpha(t, r.x + 48.0f, r.y + 36.0f, a);
    for (i = 0; i < no; i++) {
      int f = (i == ctxFoco);
      GfxRect lr = { r.x + 24.0f, r.y + 118.0f + (float)i * AGC_LINHA, r.w - 48.0f, AGC_LINHA - 10.0f };
      if (f) pilulaFoco(lr, ar, ag, ab, a);
      t = txt_linha(TXT_CALLOUT, rot[i], f ? tf : 238, f ? tf : 240, f ? tf : 244, 255);
      txt_desenhar_alpha(t, lr.x + 28.0f, lr.y + (lr.h - t.h) * 0.5f, a);
    } }
}

void agendaui_desenhar(Uint32 agora) {
  float x = ajustes_conteudo_x();
  float xDir = NV_TELA_W - NV_MARGEM_X;
  float xChipDir = x + AG_CHIP_W;
  float xEixo = xChipDir + AG_EIXO_GAP;
  float xCont = xEixo + AG_CONT_GAP;
  int n = agenda_n(), i, sep = indiceSeparador();
  float topo = listaTopo(), base = listaBase();
  float yc = AG_TOPO;
  relogio = agora;

  // CABECALHO EMPILHADO PELA ALTURA MEDIDA, e nao por offsets cravados: o
  // TXT_TITULO1 tem caixa alta e o "48 + 74" que estava aqui punha o subtitulo
  // POR CIMA do "g" de "Agenda". Um deslocamento fixo so acerta numa fonte.
  { TxtLinha t = txt_linha(TXT_TITULO1, i18n("Agenda"), 255, 255, 255, 255);
    txt_desenhar(t, x, yc);
    // A DATA DE HOJE no canto superior direito, com a base alinhada a do
    // titulo. O quadrante era vazio desde sempre, e a data por extenso e a
    // unica peca de calendario que a estacao nao da: "em 6 dias" nao diz em
    // que dia da semana HOJE cai. Caps espacadas, a voz dos rotulos de mes;
    // composta de pecas ja traduzidas (txt_tracking nao traduz a string
    // inteira — ver text.c), entao nenhuma chave nova de i18n.
    { const char *hj = agenda_hoje();
      int ds = agenda_semana(hj);
      if (ds >= 0) {
        char sem[24], ext[64], extM[64], rot[120];
        TxtLinha m;
        float w;
        maiusc(sem, sizeof sem, i18n(agenda_semana_nome(ds)));
        desc_data_extenso(hj, ext, sizeof ext);
        // `maiusc` limpa o destino antes de escrever; nao passe o mesmo buffer
        // como origem e destino, ou a data some e sobra apenas "DIA ·".
        maiusc(extM, sizeof extM, ext);
        snprintf(rot, sizeof rot, "%s \xc2\xb7 %s", sem, extM);
        // "Hg" so pela altura da caixa do CAPTION2 (ver desenhaEstacao).
        m = txt_linha(TXT_CAPTION2, "Hg", 118, 120, 128, 255);
        w = txt_tracking(TXT_CAPTION2, rot, 118, 120, 128,
                         -1.0f, 0.0f, 1.0f, 2.0f);
        txt_tracking(TXT_CAPTION2, rot, 118, 120, 128, xDir - w,
                     yc + (float)t.h - (float)m.h, 1.0f, 2.0f);
      } }
    yc += (float)t.h + 6.0f; }

  { char sub[200];
    int comData = 0;
    for (i = 0; i < n; i++)
      if (temData(agenda_lista(i))) comData++;
    // A LINHA QUE DIZ O QUE ESTA TELA NAO FAZ. Numa TV nao ha notificacao:
    // nem webOS nem Tizen entregam nada a um app fechado, e este app nao tem
    // servico de fundo. Prometer aviso seria mentira, entao a tela conta a
    // verdade uma vez, no lugar onde a pessoa vai marcar o primeiro lembrete.
    if (n == 0)
      snprintf(sub, sizeof sub, "%s",
               i18n("Salve uma série ou comece a assistir para ela aparecer aqui"));
    else if (comData == 1)
      snprintf(sub, sizeof sub, i18n("%d série com data confirmada · %d acompanhadas"), comData, n);
    else
      snprintf(sub, sizeof sub, i18n("%d séries com data confirmada · %d acompanhadas"), comData, n);
    { TxtLinha l = txt_linha_corta(TXT_BODY, sub, 156, 156, 160, 255, xDir - x);
      txt_desenhar(l, x, yc);
      yc += (float)l.h + 12.0f; } }

  { TxtLinha l = txt_linha(TXT_CAPTION,
                           i18n("OK marca o lembrete. A TV não avisa sozinha: o aviso aparece quando você abrir o app no dia."),
                           120, 122, 130, 255);
    txt_desenhar(l, x, yc);
    yc += (float)l.h + 8.0f; }

  // O AVISO DE FONTE, so quando o TMDB esta fora E falta dado na lista. Sem o
  // TMDB o fio cai para o Trakt e o Cinemeta (agenda.c, "AS TRES FONTES"), que
  // nao tem rede nem marco de temporada e as vezes nem data — e o dono pediu
  // que a tela dissesse isso em vez de a pessoa achar que a Agenda quebrou.
  // Duas frases porque sao duas causas com saidas diferentes: ajuste
  // desligado tem conserto em Ajustes (o caminho e o de ajustes.c: secao
  // "Integracoes", bloco e chave "TMDB"); pacote sem chave nenhuma nao tem, e
  // mandar a pessoa a um ajuste que nao resolve seria mentir.
  // Cabe entre a legenda e a lista: legenda termina em ~200, listaTopo e 262.
  { const char *chave = desc_chave_tmdb();
    int falta = 0;
    if (!chave || !chave[0]) {
      for (i = 0; i < n && !falta; i++) {
        const AgItem *it = agenda_lista(i);
        if (it && (!it->poster[0] || !it->titulo[0] ||
                   (!it->dataProx[0] && !it->dataUlt[0] && it->situacao == AG_DESCONHECIDA)))
          falta = 1;
      }
    }
    if (falta) {
      const char *res = desc_chave_tmdb_reserva();
      const char *msg = (res && res[0])
        ? i18n("TMDB desligado: datas e cartazes vêm do Cinemeta e podem faltar em algumas séries · Ajustes › Integrações › TMDB")
        : i18n("Sem chave do TMDB: datas e cartazes vêm do Cinemeta e podem faltar em algumas séries");
      // Ambar apagado e nao o vermelho do realce: e informacao, nao erro, e o
      // vermelho ja e o "HOJE" da linha do tempo logo abaixo.
      TxtLinha l = txt_linha_corta(TXT_CAPTION, msg, 214, 178, 110, 255, xDir - x);
      txt_desenhar(l, x, yc);
    } }

  // ESTADO VAZIO: o despertador DESLIGADO, grande e apagado, no lugar onde a
  // lista vai ficar. Uma tela so com duas frases cinzas nao diz se ela esta
  // vazia ou se quebrou; o icone diz do que a tela trata antes de a pessoa ler.
  if (n == 0) {
    float lado = 220.0f;
    GfxRect ic = { x + (xDir - x - lado) * 0.5f,
                   topo + (base - topo - lado) * 0.5f - 40.0f, lado, lado };
    agendaui_despertador(ic, 0, 0.30f, 0.30f, 0.30f, 0.55f, agora, 0);
    return;
  }

  // As reguas das faixas (HOJE, mes, separador) acabam onde o cartao acaba:
  // uma regua ate a margem direita, com o cartao parando em 1500, deixava
  // 300 px de traco sem nada embaixo.
  { AgMedidas m = medidas(xCont, xDir); xDir = m.xCartao + m.wCartao; }

  gfx_recorte(0, topo, NV_TELA_W, base - topo);

  // O EIXO PRIMEIRO, atras de tudo. Em duas partes: continua da origem ate o
  // ultimo no com data, tracejada dali ate o fim. Desenhar por linha faria o
  // eixo aparecer com emendas nas faixas de mes.
  { float yIni = topo + yDe(0) - alturaFaixas(0) + AG_HOJE_H - 26.0f - scrollY;
    float yFim = topo + yDe(n - 1) + AG_LINHA_H * 0.5f - scrollY;
    float yCorte = yFim;
    if (sep >= 0) yCorte = topo + yDe(sep) - alturaFaixas(sep) - scrollY;
    if (yCorte < yIni) yCorte = yIni;
    if (yCorte > yFim) yCorte = yFim;
    desenhaEixo(xEixo, yIni, yCorte, 0);
    desenhaEixo(xEixo, yCorte, yFim, 1); }

  for (i = 0; i < n && i < AG_MAX; i++) {
    const AgItem *it = agenda_lista(i);
    float hFaixa = alturaFaixas(i);
    float y = topo + yDe(i) - scrollY;
    float h = alturaLinha(i);
    int hoje;
    if (!it) continue;
    if (y - hFaixa > base || y + h < topo) continue;
    faixas(i, xChipDir, xEixo, xCont, xDir, y - hFaixa, 1);
    hoje = temData(it) && agenda_dias(it->dataProx) == 0;
    // Numeral e no ficam no centro dos 177 DE BASE, nao da altura viva: a
    // quarta linha abre por baixo e o numeral tem de continuar na altura do
    // cartaz e do titulo, senao a estacao desce quando o foco chega.
    desenhaEstacao(it, xChipDir, y + AG_LINHA_H * 0.5f, hoje, animFoco[i]);
    desenhaNo(xEixo, y + AG_LINHA_H * 0.5f, it, hoje, animFoco[i]);
    desenhaConteudo(it, xCont, xDir, y, h, animFoco[i]);
  }
  gfx_sem_recorte();
  if (ctxA > 0.01f) desenhaContexto(ctxA);
}
