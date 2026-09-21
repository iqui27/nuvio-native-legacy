// Tela AGENDA — a LINHA DO TEMPO das series acompanhadas.
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
#include <stdio.h>
#include <string.h>
#include <math.h>

#define AG_TOPO         48.0f
// A ESTACAO e o EIXO. 138 cabe "quarta" abreviada, o numeral em corpo 48 e
// "em 12 semanas" — medido com a linha mais larga que agenda_falta produz.
#define AG_CHIP_W      138.0f
#define AG_EIXO_GAP     34.0f
#define AG_EIXO_W        3.0f
#define AG_CONT_GAP     48.0f
#define AG_NO_D         16.0f
#define AG_NO_HOJE      25.0f
#define AG_TRACO        10.0f   // traco e vao do eixo tracejado

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
// A SINOPSE, agora na coluna da direita: passo de 30 e ate TRES linhas. Era
// duas linhas de 32 numa coluna de 1380px; na coluna de 42% (~525px) a mesma
// frase precisa de tres para nao virar "…" na primeira virgula, e tres linhas
// de 30 medem 90px — cabem folgadas nos 177 da linha, que e o ponto.
#define AG_SIN_LD       30.0f
#define AG_SIN_MAX         3
// O VAO ENTRE AS DUAS COLUNAS DE TEXTO e a fracao que fica com a direita.
// 44 e o menor vao em que as duas colunas ainda leem como colunas a 3 m: com 24
// a sinopse parecia continuacao da linha de apoio. A fracao 0,42 foi escolhida
// pelo TITULO: com 0,50 "Uma Série Cancelada" ja pedia reticencia, com 0,42 a
// esquerda fica com ~725px e cabe titulo de ~40 caracteres em TXT_HEADLINE.
#define AG_COL_GAP      44.0f
#define AG_COL_DIR      0.42f
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
#define AG_SINO         54.0f
// A FAIXA DO DESPERTADOR, reservada em TODA linha e nao so nas marcadas. Custa
// 178px de largura de texto que quase nenhum titulo usava, e em troca as duas
// colunas de texto ficam no mesmo x em todas as linhas — que e a disciplina de
// calendario que esta tela inteira defende. Antes a linha sem lembrete esticava
// o texto por cima da faixa e as colunas dancavam de linha para linha.
//
// 178 E MEDIDO NA LEGENDA, nao no icone. Com AG_SINO + 46 = 100 (o que bastava
// para o icone) a legenda da linha focada saia "Lembret…" e "Lembrar-…" na
// captura — uma legenda cortada e pior que legenda nenhuma, porque o que sobra
// e a mesma palavra nos dois estados. "Lembrete ativo" em TXT_CAPTION2 mede
// ~153px; 178 deixa a folga do corte e ainda centra o icone na faixa.
#define AG_SINO_FAIXA  (AG_SINO + 124.0f)
#define AG_TEXTO_ESCURO 20

// EMERALD #66bb6a — o MESMO da paleta de acentos (ajustes.c:204) e do selo do
// Social (salvospainel.c:708). Nao ha verde novo neste arquivo.
#define AG_VERDE_R 0.400f
#define AG_VERDE_G 0.733f
#define AG_VERDE_B 0.416f
// Sobre superficie clara o esmeralda cheio da 2,2:1 e some. Escurecido a 42% da
// 8:1 contra #f5f5f5 e continua sendo o mesmo matiz — escurecer nao inventa cor.
#define AG_VERDE_ESC 0.42f

void agendaui_cor_lembrete(int ligado, int sobreClaro,
                           float *r, float *g, float *b) {
  float k = ligado ? (sobreClaro ? AG_VERDE_ESC : 1.0f) : 0.0f;
  if (!ligado) {
    float t = sobreClaro ? (AG_TEXTO_ESCURO / 255.0f) : 0.94f;
    *r = *g = *b = t;
    return;
  }
  *r = AG_VERDE_R * k; *g = AG_VERDE_G * k; *b = AG_VERDE_B * k;
}

// --- O DESPERTADOR ANIMADO ---------------------------------------------------
//
// TRES PARTES, e so a primeira e imagem:
//   1. o icone (art/icones/lembrete.png, forma na alpha, tingido daqui);
//   2. o TREMOR — deslocamento horizontal em seno de ~9 Hz, com envelope: toca
//      ~600 ms a cada 2,4 s. Um tremor continuo vira ruido na periferia da
//      visao a 3 m; um que toca e para le como "esta armado";
//   3. as ONDAS DE SOM — dois pares de pastilhas saindo das campainhas, com
//      alfa e tamanho puxados pelo mesmo envelope.
//
// AS ONDAS SAO FRACAO DA CAIXA DO GLIFO, e a caixa encolheu de 0,42 para 0,333
// do circulo junto com o icone — entao a mesma fracao passou a desenhar menos
// pixel. A primeira tentativa foi 0,075 de largura; na captura as pastilhas
// sairam com 2,4 px e sumiram. Ficaram em 0,10 por 0,26 e 0,17 de altura, que
// em pixel da quase o mesmo que as antigas (3,2 contra 3,6 de largura) — o
// alivio de peso veio do DESENHO, nao de apagar o sinal de estado.
//
// Conta fechada dentro do circulo de 96: o botao armado punha 472 px2 de icone
// mais 117 de onda, 589 no total, numa fileira cujos vizinhos andam entre 118
// ("+") e 316 (olho). Agora sao 253 de icone mais 88 de onda, 341.
//
// NAO APAGAR AS ONDAS: com "reduzir animacoes" ligado elas sao o UNICO sinal de
// estado que nao e cor. Emagrecer ate sumir troca um defeito de peso por um
// defeito de acesso.
//
// Nada disso existe com o lembrete DESLIGADO: ali e so o icone, mais apagado.
// E o par de estados que o dono pediu para nao depender de texto — e tambem a
// razao de o verde nunca ser o unico sinal, ver agendaui_cor_lembrete.
#define AG_TOQUE_MS   2400
#define AG_TOQUE_DUR   600.0f
#define AG_TROCA_MS     700

void agendaui_despertador(GfxRect r, int ligado, float cr, float cg, float cb,
                          float a, Uint32 agora, Uint32 desde) {
  float env = 0.0f, dx = 0.0f;
  int reduz = ajustes_animacoes_reduzidas();
  if (ligado && !reduz) {
    float ciclo = (float)(agora % AG_TOQUE_MS);
    env = ciclo < AG_TOQUE_DUR ? (1.0f - ciclo / AG_TOQUE_DUR) : 0.0f;
    // Acabou de ligar: toca INTEIRO, porque e o unico instante em que o dono
    // esta olhando para o botao que acabou de apertar.
    if (desde && agora - desde < AG_TROCA_MS) env = 1.0f;
    dx = env * (r.w * 0.055f) * sinf((float)agora * 0.055f);
  }
  if (ligado) {
    // As ondas: duas de cada lado, na altura das campainhas. Com movimento
    // reduzido ficam PARADAS e opacas — o estado nao pode depender de animar.
    float base = reduz ? 0.85f : (0.30f + 0.70f * env);
    // NA ALTURA DO SUPORTE DO SINO. O desenho novo poe os dois tracos
    // inclinados entre 0,128 e 0,226 da caixa (lembrete.svg); 0,20 encosta na
    // ponta deles, que e de onde o som sairia. Com 0,16, herdado das campainhas
    // redondas do desenho antigo, as pastilhas flutuavam acima do icone.
    float cy = r.y + r.h * 0.20f;
    int k;
    for (k = 0; k < 2; k++) {
      float d = r.w * (0.60f + 0.20f * (float)k);
      float h = r.h * (0.26f - 0.09f * (float)k);
      float w = r.w * 0.10f;
      float al = a * base * (k == 0 ? 1.0f : 0.55f);
      GfxRect esq = { r.x + r.w * 0.5f - d - w * 0.5f + dx, cy - h * 0.5f, w, h };
      GfxRect dir = { r.x + r.w * 0.5f + d - w * 0.5f + dx, cy - h * 0.5f, w, h };
      if (h <= 1.5f || w <= 1.0f) continue;
      gfx_cor(esq, 0.5f, cr, cg, cb, al);
      gfx_cor(dir, 0.5f, cr, cg, cb, al);
    }
  }
  { GfxRect ic = { r.x + dx, r.y, r.w, r.h };
    gfx_icone(ic, "lembrete", cr, cg, cb, a * (ligado ? 1.0f : 0.62f)); }
}

// A QUEBRA POR PALAVRA, num lugar so, para quem MEDE e para quem DESENHA.
// Preenche ate `max - 1` linhas inteiras e devolve quantas sairam; o que sobrou
// fica em *resto e vai por txt_linha_corta, que e quem sabe fechar com "…".
//
// A COR ENTRA NA MEDIDA de proposito. A largura nao depende dela, mas o cache
// de txt_linha e indexado por (estilo, texto, cor): medir numa cor que nao vai
// ser desenhada rasteriza uma segunda copia de cada prefixo de linha. Quem mede
// passa a MESMA cor de quem desenha.
static int agQuebra(TxtEstilo estilo, const char *s, float larg, int max,
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
static float alturaLinha(int i) { (void)i; return AG_LINHA_H; }

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
  // Uma passada de rede por abertura da tela, e so para as series seguidas com
  // registro faltando ou velho. Ver a nota longa em agenda.c: este e o unico
  // pedido do recurso que nao vem de graca.
  agenda_atualizar_seguidas();
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
      noticias_pedir(it->imdb, it->titulo, 1);
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
  if (volta) { sair = 1; return; }
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
  int i, n = agenda_n();
  float alvoY, topo, base, y, h;
  (void)agora;
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
      TxtLinha t;
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
      txt_tracking(TXT_CAPTION2, hj, (int)(ar * 255.0f + 0.5f),
                   (int)(ag * 255.0f + 0.5f), (int)(ab * 255.0f + 0.5f),
                   xChipDir - lw, yl - 26.0f, 1.0f, 2.4f);
      if (rot[0]) {
        t = txt_linha(TXT_CAPTION2, rot, 150, 152, 160, 255);
        txt_desenhar(t, xCont, yl - 26.0f);
      }
      regua(xEixo, xDir, yl, ar * 0.9f, 0.30f);
      { GfxRect no = { xEixo - AG_NO_HOJE * 0.5f, yl - AG_NO_HOJE * 0.5f,
                       AG_NO_HOJE, AG_NO_HOJE };
        gfx_cor(no, 0.5f, ar, ag, ab, 1.0f); }
    }
    usado += AG_HOJE_H;
  }

  if (i == indiceSeparador()) {
    float yl = y + usado + AG_FAIXA_H - 22.0f;
    if (desenhar) {
      TxtLinha t = txt_linha(TXT_CAPTION2, i18n("Sem data prevista"),
                             138, 140, 148, 255);
      txt_desenhar(t, xCont, yl - 26.0f);
      // TRACEJADA, como o eixo daqui para baixo: o tempo acaba de ser
      // interrompido, e uma regua continua diria que a contagem segue.
      { float x = xEixo;
        while (x < xDir) {
          float w = AG_TRACO;
          if (x + w > xDir) w = xDir - x;
          regua(x, x + w, yl, 0.62f, 0.22f);
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
        TxtLinha t;
        rotuloMes(it->dataProx, rot, sizeof rot);
        t = txt_linha(TXT_CAPTION2, rot, 150, 152, 160, 255);
        txt_desenhar(t, xCont, yl - 26.0f);
        regua(xEixo, xDir, yl, 0.62f, 0.14f);
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
    TxtLinha sem, dia, fal;
    int cN = hoje ? (int)(ar * 255.0f + 0.5f) : (f > 0.5f ? 246 : 232);
    int cNg = hoje ? (int)(ag * 255.0f + 0.5f) : (f > 0.5f ? 246 : 232);
    int cNb = hoje ? (int)(ab * 255.0f + 0.5f) : (f > 0.5f ? 246 : 232);
    float alt, y;
    snprintf(num, sizeof num, "%d", agenda_dia(it->dataProx));
    agenda_falta(it->dataProx, falta, sizeof falta);
    sem = txt_linha(TXT_CAPTION2, agenda_semana_nome(agenda_semana(it->dataProx)),
                    132, 134, 142, 255);
    dia = txt_linha(TXT_TITULO3, num, cN, cNg, cNb, 255);
    // "hoje" e "amanha" saem na cor de realce: sao as duas unicas respostas que
    // fazem alguem mudar o que ia assistir hoje a noite.
    fal = txt_linha_corta(TXT_CAPTION2, falta,
                          d <= 1 ? (int)(ar * 255.0f + 0.5f) : 146,
                          d <= 1 ? (int)(ag * 255.0f + 0.5f) : 148,
                          d <= 1 ? (int)(ab * 255.0f + 0.5f) : 156, 255,
                          AG_CHIP_W);
    alt = (float)sem.h + 2.0f + (float)dia.h + 2.0f + (float)fal.h;
    y = yCentro - alt * 0.5f;
    txt_desenhar(sem, xDir - (float)sem.w, y); y += (float)sem.h + 2.0f;
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
  }
}

// Terceira linha: o MARCO primeiro, depois rede/duracao/temporadas. O marco vem
// na frente porque e a unica coisa desta linha que muda de um episodio para o
// seguinte — "Final da temporada" e a razao de alguem marcar o lembrete.
static void linhaApoio(const AgItem *it, char *dst, size_t tam) {
  char marco[64], apoio[160];
  dst[0] = 0;
  agenda_marco(it, marco, sizeof marco);
  agenda_apoio(it, apoio, sizeof apoio);
  if (marco[0] && apoio[0]) snprintf(dst, tam, "%s \xc2\xb7 %s", marco, apoio);
  else if (marco[0])        snprintf(dst, tam, "%s", marco);
  else                      snprintf(dst, tam, "%s", apoio);
}

// O CONTEUDO DA LINHA: cartaz, coluna esquerda, coluna direita e a faixa do
// despertador. As quatro larguras saem de UMA conta, feita aqui e usada por
// todos os blocos — separadas, elas ja discordaram em outro ponto deste
// arquivo e o sintoma foi texto passando por baixo do icone.
static void desenhaConteudo(const AgItem *it, float x, float y, float larg,
                            float f) {
  char ep[220], apoio[220], ant[120];
  int c  = f > 0.5f ? ajustes_tinta_foco() : 246;
  int c2 = f > 0.5f ? ajustes_tinta_foco2() : 158;
  int c3 = f > 0.5f ? ajustes_tinta_foco2() : 126;
  float tx, util, wEsq, wDir, xDirCol, yb, blocoH;
  TxtLinha t, l2, l3;

  if (f > 0.01f) {
    float ar, ag, ab;
    GfxRect fundo = { x - 16.0f, y, larg + 16.0f, AG_LINHA_H };
    ajustes_acento(&ar, &ag, &ab);
    // O raio do gfx_cor e FRACAO DA ALTURA (ver FS_SDF em gfx.c): 20 px sobre os
    // 177 da linha. Era 16 dividido pela altura VIVA, que mudava enquanto a
    // linha abria; a linha nao abre mais, entao a conta e uma constante e o
    // canto parou de mudar de curvatura no meio da transicao. 20 e nao 16
    // porque a laje encolheu: o mesmo canto numa peca menor le mais duro.
    gfx_cor(fundo, 20.0f / AG_LINHA_H, ar, ag, ab, f);
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

  // 26 e nao 22: o vao entre arte e texto cresceu junto com a arte. Mantido em
  // 22 o titulo encostava num cartaz um terco maior.
  tx = x + AG_CARTAZ_W + 26.0f;
  // AS DUAS COLUNAS, medidas do que sobra depois do cartaz e da faixa do
  // despertador — e a faixa e descontada SEMPRE, marcada ou nao (ver
  // AG_SINO_FAIXA). Com a rail recolhida a conta da 1288 uteis: 725 a esquerda,
  // 44 de vao, 519 a direita.
  util = larg - (tx - x) - AG_SINO_FAIXA;
  if (util < 320.0f) util = 320.0f;
  wDir = (util - AG_COL_GAP) * AG_COL_DIR;
  wEsq = util - AG_COL_GAP - wDir;
  xDirCol = tx + wEsq + AG_COL_GAP;

  linhaEpisodio(it, ep, sizeof ep);
  linhaApoio(it, apoio, sizeof apoio);
  t  = txt_linha_corta(TXT_HEADLINE, it->titulo[0] ? it->titulo : i18n("Série"),
                       c, c, c, 255, wEsq);
  l2 = ep[0] ? txt_linha_corta(TXT_CAPTION, ep, c2, c2, c2, 255, wEsq)
             : (TxtLinha){ 0, 0, 0 };
  l3 = apoio[0] ? txt_linha_corta(TXT_CAPTION2, apoio, c3, c3, c3, 255, wEsq)
                : (TxtLinha){ 0, 0, 0 };
  blocoH = (float)t.h + (l2.h ? 5.0f + (float)l2.h : 0.0f)
                      + (l3.h ? 4.0f + (float)l3.h : 0.0f);
  yb = y + (AG_LINHA_H - blocoH) * 0.5f;
  txt_desenhar(t, tx, yb); yb += (float)t.h;
  if (l2.h) { yb += 5.0f; txt_desenhar(l2, tx, yb); yb += (float)l2.h; }
  if (l3.h) { yb += 4.0f; txt_desenhar(l3, tx, yb); }

  // --- A COLUNA DA DIREITA, so na linha focada -----------------------------
  //
  // Duas coisas, nesta ordem, e nenhuma delas esta na esquerda:
  //   1. QUANDO FOI O ANTERIOR (`dataUlt`). Ja estava no cache e so era usada
  //      quando NAO havia proximo episodio; numa linha que tem data futura ela
  //      responde outra pergunta, que e "eu estou em dia com esta serie?".
  //   2. A SINOPSE do proximo episodio, ate tres linhas, cortando em "…".
  //
  // O bloco e CENTRADO na altura da linha junto com o da esquerda: duas colunas
  // com topos diferentes leem como dois elementos soltos, nao como uma linha.
  // Por isso a sinopse e medida antes de desenhar (agendaui_sinopse_linhas).
  //
  // VAZIO E VAZIO. Serie sem sinopse e sem episodio anterior deixa esta coluna
  // em branco; nao entra data por extenso nem rede repetida para tapar o
  // espaco. A linha continua com 177px de altura, que e o tamanho do que ela
  // tem a dizer — a laje de 254px com metade vazia era exatamente a queixa.
  if (f > 0.02f) {
    int cs = f > 0.5f ? ajustes_tinta_foco2() : 168;
    int ca = f > 0.5f ? ajustes_tinta_foco2() : 132;
    int nSin = it->sinopse[0]
             ? agendaui_sinopse_linhas(TXT_CAPTION, it->sinopse, wDir,
                                       AG_SIN_MAX, cs, cs, cs)
             : 0;
    TxtLinha la = { 0, 0, 0 };
    float altD;
    ant[0] = 0;
    // A CABECA DA COLUNA SO EXISTE NA LINHA COM DATA FUTURA, e a razao esta na
    // coluna da ESQUERDA: quando nao ha proximo episodio, linhaEpisodio ja gasta
    // a segunda linha com "Último episódio em ...". Sem esta guarda a captura
    // -semdata saia com a MESMA frase duas vezes na mesma linha, palavra por
    // palavra, a 700px de distancia. Numa linha sem data a direita fica com a
    // sinopse, ou com nada.
    if (!temData(it)) {
      /* a esquerda ja disse o que havia a dizer sobre a data */
    } else if (it->dataUlt[0]) {
      char q[64];
      agenda_quando(it->dataUlt, q, sizeof q);
      if (q[0]) snprintf(ant, sizeof ant, i18n("Último episódio em %s"), q);
    } else if (agenda_dias(it->dataProx) > 7) {
      // SEM EPISODIO ANTERIOR NO CACHE, e a estreia esta a mais de uma semana:
      // entra a DATA POR EXTENSO, sozinha. Nao e a mesma coisa que a estacao diz
      // a 300px daqui: `agenda_falta` ARREDONDA PARA BAIXO de proposito ("em 7
      // semanas" cobre de 49 a 55 dias — ver a nota em agenda.c), e o numeral
      // sozinho depende do cabecalho de mes, que some ao rolar. Aqui vai o dia
      // exato, que e o que nenhum dos dois diz.
      //
      // So acima de 7 dias: dentro da semana agenda_quando devolve "amanhã" /
      // "em 3 dias", que e literalmente o texto que a estacao ja tem. Repetir
      // isso seria enchimento, e enchimento e o que esta coluna nao pode ter.
      agenda_quando(it->dataProx, ant, sizeof ant);
    }
    if (ant[0]) la = txt_linha_corta(TXT_CAPTION2, ant, ca, ca, ca, 255, wDir);
    altD = (la.h ? (float)la.h + 10.0f : 0.0f)
         + (nSin ? AG_SIN_LD * (float)(nSin - 1) + 26.0f : 0.0f);
    if (altD > 0.0f) {
      float yd = y + (AG_LINHA_H - altD) * 0.5f;
      if (la.h) { txt_desenhar_alpha(la, xDirCol, yd, f); yd += (float)la.h + 10.0f; }
      if (nSin)
        agendaui_sinopse(TXT_CAPTION, it->sinopse, xDirCol, yd, wDir,
                         AG_SIN_LD, AG_SIN_MAX, cs, cs, cs, f);
    }
  }

  // ESTADO DO LEMBRETE, encostado na direita: o DESPERTADOR, e nao uma
  // pastilha de texto. Duas razoes, e as duas sao do sofa: a 3 m um icone com
  // movimento se acha varrendo a coluna, uma palavra de 22 px nao; e a linha
  // ja tem quatro blocos de texto — um quinto vira cinza.
  //
  // FORA DO FOCO so aparece LIGADO. Um despertador apagado em toda linha seria
  // um campo de ruido, e o que interessa de longe e quais poucas linhas estao
  // marcadas.
  //
  // NA LINHA FOCADA ele aparece nos DOIS estados, com uma legenda embaixo
  // ("Lembrete ativo" / "Lembrar-me"). Isto nao desfaz a regra acima: e UMA
  // linha, a que o OK vai atingir, e ali o desligado deixa de ser ruido e passa
  // a ser o alvo do botao. Era a unica coisa desta tela que so tinha um estado
  // desenhado — quem chegava numa serie sem lembrete nao via onde o OK agia.
  // As duas legendas ja existem na tabela de traducao, de detail.c.
  { int pode = agenda_pode_lembrar(it->imdb);
    int mostra = it->lembrete || (f > 0.5f && pode);
    if (mostra) {
      float cr, cg, cb, altP = AG_SINO, yl;
      TxtLinha leg = { 0, 0, 0 };
      if (f > 0.5f && pode) {
        const char *s = it->lembrete ? i18n("Lembrete ativo") : i18n("Lembrar-me");
        int cl = ajustes_tinta_foco2();
        leg = txt_linha_corta(TXT_CAPTION2, s, cl, cl, cl, 255, AG_SINO_FAIXA);
        altP += 6.0f + (float)leg.h;
      }
      yl = y + (AG_LINHA_H - altP) * 0.5f;
      // ICONE E LEGENDA CENTRADOS NO MESMO EIXO, pedido do dono olhando a
      // captura: "deixa centralizado com o nome abaixo".
      //
      // A versao anterior alinhava os dois A DIREITA, cada um com o seu recuo,
      // e eles fechavam em verticais diferentes — o par lia como desalinhado,
      // que e justamente o que alinhar deveria evitar. A objecao que levou
      // aquilo (centrar joga a palavra para fora da laje) continua valendo, e a
      // resposta certa nao e desistir de centrar: e centrar na FAIXA, nao no
      // icone. A faixa e o espaco que a linha ja reserva para este par (ver
      // AG_SINO_FAIXA, descontado da largura util em toda linha), e a legenda
      // ja e cortada nessa mesma medida — logo nada pode ultrapassar a borda.
      //
      // O recuo de 18px continua, agora como margem DA FAIXA: as ondas de som
      // saem para fora do quadrado do icone, e encostada na margem a onda da
      // direita caia metade na superficie clara e metade no fundo da tela.
      { float eixo = x + larg - 18.0f - AG_SINO_FAIXA * 0.5f;
        { GfxRect ic = { eixo - AG_SINO * 0.5f, yl, AG_SINO, AG_SINO };
          agendaui_cor_lembrete(it->lembrete, f > 0.5f, &cr, &cg, &cb);
          agendaui_despertador(ic, it->lembrete, cr, cg, cb, 1.0f,
                               relogio, trocaEm); }
        if (leg.h)
          txt_desenhar_alpha(leg, eixo - (float)leg.w * 0.5f,
                             yl + AG_SINO + 6.0f, f); }
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
    // 3px de espessura sobre um no de 20 a 25: fino o bastante para ler como
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
    aneis(xEixo, y, d * 1.9f, 3.0f, ar, ag, ab, 0.38f);
  }
  if (hoje || f > 0.5f) gfx_cor(no, 0.5f, ar, ag, ab, 1.0f);
  else                  gfx_cor(no, 0.5f, lum, lum, lum, 1.0f);
}

// O EIXO. Continuo ate o separador, tracejado depois dele — e o tracejado nao e
// enfeite: dali para baixo nao ha tempo, so situacao.
static void desenhaEixo(float xEixo, float y0, float y1, int tracejado) {
  if (y1 <= y0) return;
  if (!tracejado) {
    GfxRect r = { xEixo - AG_EIXO_W * 0.5f, y0, AG_EIXO_W, y1 - y0 };
    gfx_cor(r, 0.0f, 0.70f, 0.71f, 0.76f, 0.30f);
    return;
  }
  { float y = y0;
    while (y < y1) {
      float h = AG_TRACO;
      GfxRect r;
      if (y + h > y1) h = y1 - y;
      r.x = xEixo - AG_EIXO_W * 0.5f; r.y = y;
      r.w = AG_EIXO_W; r.h = h;
      gfx_cor(r, 0.0f, 0.70f, 0.71f, 0.76f, 0.30f);
      y += AG_TRACO * 2.0f;
    } }
}

// --- menu de contexto e noticias ----------------------------------------------
//
// Cartao central com tres linhas (abrir o titulo / ultimas noticias / lembrete)
// e, atras dele, um painel de manchetes do Google News (noticias.h): so
// manchete, veiculo e dia — a TV nao abre link, entao e leitura, nao indice.
#define AGC_W     760.0f
#define AGC_LINHA  84.0f
#define AGN_W    1180.0f
#define AGN_LINHA 108.0f
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
    gfx_cor(r, 28.0f / r.h, 0.075f, 0.078f, 0.09f, 0.98f * a);
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
        if (f) gfx_cor(lr, 16.0f / lr.h, ar, ag, ab, a);
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
    gfx_cor(r, 28.0f / r.h, 0.075f, 0.078f, 0.09f, 0.98f * a);
    t = txt_linha_corta(TXT_TITULO3, it->titulo, 246, 247, 250, 255, AGC_W - 96.0f);
    txt_desenhar_alpha(t, r.x + 48.0f, r.y + 36.0f, a);
    for (i = 0; i < no; i++) {
      int f = (i == ctxFoco);
      GfxRect lr = { r.x + 24.0f, r.y + 118.0f + (float)i * AGC_LINHA, r.w - 48.0f, AGC_LINHA - 10.0f };
      if (f) gfx_cor(lr, 16.0f / lr.h, ar, ag, ab, a);
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
    txt_desenhar(l, x, yc); }

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
    desenhaEstacao(it, xChipDir, y + AG_LINHA_H * 0.5f, hoje, animFoco[i]);
    desenhaNo(xEixo, y + AG_LINHA_H * 0.5f, it, hoje, animFoco[i]);
    desenhaConteudo(it, xCont, y, xDir - xCont, animFoco[i]);
  }
  gfx_sem_recorte();
  if (ctxA > 0.01f) desenhaContexto(ctxA);
}
