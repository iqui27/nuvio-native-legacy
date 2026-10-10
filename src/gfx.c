#include "gfx.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "gfx_smartphone_png.h"
#include "layout.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "anim.h"
#include "corviva.h"
#include "gpunivel.h"
#include "ajustes.h"

// A MISTURA, com o estado lembrado. gfx_rect desliga a mistura num desenho
// opaco e a devolve ao que estava — nao a "ligada" — porque quem gera o
// desfoque e a luz assada ja a desligou em volta de varios desenhos.
static int blendLigado = 1;
static void gfxBlend(int on) {
  if (on) glEnable(GL_BLEND); else glDisable(GL_BLEND);
  blendLigado = on ? 1 : 0;
}

// Um programa por modo, e os uniforms de cada um: as posicoes NAO coincidem
// entre programas, entao guardar um conjunto so devolveria lixo no segundo
// shader que usasse a mesma variavel.
typedef struct {
  GLuint prog;
  GLint rect, tela, tex, foco, reflexo, par, raio, cor, asp, texAsp, forcarCover, veuTela, borda, varre, veu, desl, fundo,
        grad0, grad1, grad2, tempo, reg0, reg1, reg2, reg3, vaza;
  GLint amb, ambOn;         // uAmb/uAmbOn: so os tres modos de arte com rampa
  GLint texB, texAspB, alfaB;   // camadas do destaque (gfx_hero_camadas)
  GLint alt;     // uAlt: altura do rect em pixels do alvo (a rampa de 1 px do SDF)
  GLint margem;  // uMargem do VS: 1 px de folga no quad dos modos de SDF
  GLint jan;     // uJan: janela do GFX_JANELA
  GLint leve;    // uLeve: 1 = efeitos leves (sem dither), ver gfx_definir_efeitos_leves
  GLint sub;     // uSub do VS: o pedaco do rect desenhado (gfx_sombra_vazada)
  float subAtual[4];
  GLint din;     // uDin: cor do topo e queda do fundo da Dinamica (GFX_VITRINE_DIN)
  GLint giro;    // uGiro do VS: rotacao de grupo (gfx_girar)
  float giroAtual[4];
  float altAtual, margemAtual, leveAtual;  // o ultimo valor enviado: so chama o GL se mudar
  float telaAtual[2];       // o uTela enviado (muda so dentro de uma miniatura)
} Programa;
static Programa progs[GFX_NMODOS];
static int progAtual = -1;
// Proporcao da textura corrente, para o "cover". Fica global porque o desenho e
// imediato: quem chama define antes de cada rect com textura.
float gfx_tex_aspect_atual = 0.0f;
int gfx_arte_opaca_atual = 0;
float gfx_janela_atual[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
// TEXTURA (cor de destaque "Textura"): a do titulo em cena, posta uma vez por
// quadro por ajustes_textura_quadro. 0 = desligada.
static GLuint txTex;
static float txJan[4], txAsp, txForca, txVeu, txVeuBranco, txUv[4];
void gfx_textura_definir(GLuint tex, const float janela[4], float aspecto,
                         float forca, float veu, int veuBranco) {
  txTex = tex;
  if (!tex || !janela) return;
  memcpy(txJan, janela, sizeof txJan);
  txAsp = aspecto; txForca = forca; txVeu = veu; txVeuBranco = veuBranco ? 1.0f : 0.0f;
}
int gfx_textura_ativa(void) { return txTex != 0; }
float gfx_card_forcar_cover_atual = 0.0f;
float gfx_card_veu_tela_atual = 0.0f;
// O pedaco do rect que o proximo gfx_rect desenha (uSub). So gfx_sombra_vazada
// mexe, e devolve a (0,0,1,1) antes de sair.
static float subAtual[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
// Camadas do destaque (gfx_hero_camadas): a camada B e o fundo, lidos por
// gfx_rect no desenho imediato que a funcao dispara.
static GLuint camTexB;
static float  camAspB, camAlfaB;
// 1 = desenhar o rebordo claro que marca o cartaz em foco; 0 = nao desenhar.
// Vive aqui, e nao num parametro de gfx_rect, pela mesma razao do aspecto da
// textura: sao dezenas de chamadas e a resposta e a mesma para todas dentro do
// mesmo quadro. Quem o define e a tela, a partir do ajuste da pessoa.
float gfx_borda_foco_atual = 1.0f;
// Deslocamento da faixa especular do cartaz em foco (revela.h): 0 = no lugar
// de repouso. Mesmo regime do rebordo: global, e quem mexe devolve a 0.
float gfx_varre_atual = 0.0f;
// O VEU DA BASE DENTRO DA ARTE (gfx.h, gfx_veu_card_atual).
float gfx_veu_card_atual[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
int gfx_veu_na_arte = 0;
// Deslize horizontal da ARTE dentro do retangulo do destaque (GFX_HERO,
// GFX_HERO_CHEIO, GFX_VITRINE), em fracao da largura dele: a imagem anda e as
// rampas/veu ficam paradas; o que sai do retangulo nao pinta. 0 = no lugar.
// Mesmo regime do rebordo: global, e quem mexe devolve a 0.
float gfx_desliza_atual = 0.0f;
float gfx_opacidade_grupo = 1.0f;
float gfx_osd_mult = 1.0f;
// Tamanho real do alvo da tela (em retina, maior que 1920x1080). Guardado aqui
// porque toda volta de FBO precisa restaurar o viewport com ele.
static int telaW = (int)NV_TELA_W, telaH = (int)NV_TELA_H;
// MINIATURA (gfx_mini_*): o espaco de layout que o VS mapeia no viewport e a
// escala do alvo. Fora dela, a tela cheia de sempre.
static float uTelaW = NV_TELA_W, uTelaH = NV_TELA_H;
static int   miniAtiva, miniPxW, miniPxH;
static float miniX0, miniY0, miniEsc;
// O alvo da TELA (o ultimo gfx_tamanho_alvo): a unidade de gfx_fill_gpu. Os
// alvos pequenos (luz assada, snapshot) mudam telaW por dentro e contam pelo
// tamanho deles.
static int telaRealW = (int)NV_TELA_W, telaRealH = (int)NV_TELA_H;
// A tesoura ligada por gfx_recorte, em pixels do alvo (origem embaixo).
static int recorteAtivo;
static GLint recorteBox[4];
// Pixels que o quad `r` (layout) pinta no alvo atual: cortado pelo alvo e pela
// tesoura, em telas do alvo da tela. Ignora giro e miniatura (aproximado).
static double fillGpuArea(GfxRect r, const float sub[4]) {
  float ex = (float)telaW / NV_TELA_W, ey = (float)telaH / NV_TELA_H;
  float x0 = (r.x + r.w * sub[0]) * ex, x1 = (r.x + r.w * sub[2]) * ex;
  float y0 = (NV_TELA_H - (r.y + r.h * sub[3])) * ey, y1 = (NV_TELA_H - (r.y + r.h * sub[1])) * ey;
  if (x0 < 0.0f) x0 = 0.0f;
  if (y0 < 0.0f) y0 = 0.0f;
  if (x1 > (float)telaW) x1 = (float)telaW;
  if (y1 > (float)telaH) y1 = (float)telaH;
  if (recorteAtivo) {
    float sx0 = (float)recorteBox[0], sy0 = (float)recorteBox[1];
    float sx1 = sx0 + (float)recorteBox[2], sy1 = sy0 + (float)recorteBox[3];
    if (x0 < sx0) x0 = sx0;
    if (y0 < sy0) y0 = sy0;
    if (x1 > sx1) x1 = sx1;
    if (y1 > sy1) y1 = sy1;
  }
  if (x1 <= x0 || y1 <= y0 || telaRealW <= 0 || telaRealH <= 0) return 0.0;
  return (double)(x1 - x0) * (double)(y1 - y0) / ((double)telaRealW * (double)telaRealH);
}
/* Pixels do alvo da tela por unidade de layout (1920 de largura): 1 em 1080p, 2 em 4K. */
float gfx_px_por_unidade(void) { return (float)telaRealW / 1920.0f; }
void gfx_tamanho_alvo(int w, int h) { telaW = w; telaH = h; telaRealW = w; telaRealH = h; }
// Tamanho da interface (gfx.h). escAtiva multiplica o retangulo de layout
// antes de tudo: o SDF, os raios e as espessuras sao fracoes do proprio rect,
// e uAlt sai da altura JA ampliada — a rampa de borda segue com 1 px do alvo.
// As passadas internas de tela cheia (luz assada, snapshot, desfoques) desligam
// o fator: elas falam em tela real, nao em layout de camada.
static float escUi = 1.0f, escAtiva = 1.0f, miniEscAnt = 1.0f;
static int escUiFixa;   // NUVIO_TAMANHO_UI (capturas): Ajustes nao sobrescreve
void gfx_escala_ui_definir(float s) { if (!escUiFixa) escUi = (s >= 1.0f && s <= 2.0f) ? s : 1.0f; }
float gfx_escala_ui(void) { return escUi; }
float gfx_escala(void) { return escAtiva; }
float gfx_escala_entrar(void) { float a = escAtiva; escAtiva = escUi; return a; }
void gfx_escala_sair(float anterior) { escAtiva = anterior; }
// TRANSFORMACAO DE GRUPO (gfx_transformar): escala em torno de (ox, oy) e
// deslocamento, em coordenadas de layout, ANTES do fator da camada. As
// passadas internas (ESC_REAL, miniatura) desligam junto com o fator.
static struct { int on; float ox, oy, s, dx, dy; } gfxTr, miniTrAnt;
void gfx_transformar(float ox, float oy, float s, float dx, float dy) {
  gfxTr.on = !(s == 1.0f && dx == 0.0f && dy == 0.0f);
  gfxTr.ox = ox; gfxTr.oy = oy; gfxTr.s = s; gfxTr.dx = dx; gfxTr.dy = dy;
}
void gfx_sem_transformar(void) { gfxTr.on = 0; }
// GIRO DE GRUPO (gfx_girar): cos, sin e o pivo em pixels do alvo. Identidade
// = (1, 0, 0, 0). Aplicado no vertice, depois do fator da camada.
static float giroLayout[3];          // angulo e pivo como quem chamou pediu
static int   giroOn;
void gfx_girar(float rad, float cx, float cy) {
  giroOn = rad != 0.0f;
  giroLayout[0] = rad; giroLayout[1] = cx; giroLayout[2] = cy;
}
void gfx_sem_girar(void) { giroOn = 0; }
#define GFX_TR_RECT(x, y, w, h) do { if (gfxTr.on) { \
    x = gfxTr.ox + ((x) - gfxTr.ox) * gfxTr.s + gfxTr.dx; \
    y = gfxTr.oy + ((y) - gfxTr.oy) * gfxTr.s + gfxTr.dy; \
    w *= gfxTr.s; h *= gfxTr.s; } } while (0)
#define ESC_REAL_INI() float escGuard_ = escAtiva; int trGuard_ = gfxTr.on, giGuard_ = giroOn; escAtiva = 1.0f; gfxTr.on = 0; giroOn = 0
#define ESC_REAL_FIM() escAtiva = escGuard_; gfxTr.on = trGuard_; giroOn = giGuard_

static GLuint snapFbo = 0, snapTex = 0;
static int snapW = 0, snapH = 0;
// O alvo de tela guardado enquanto o desenho vai para o snapshot: gfx_recorte
// converte layout em pixel pelo tamanho do ALVO, e com o da tela o recorte de
// uma fileira caia no lugar errado do FBO.
static int snapTelaW = 0, snapTelaH = 0, snapAtivo = 0;
// E o alvo que estava ligado antes: volta para ELE, e nao para o 0. Mesma
// disciplina de gfx_desfocado — quem desenha a tela num FBO (os testes de
// captura com janela escondida) nao perde o alvo no meio do quadro.
static GLint snapFboAnt = 0, snapVpAnt[4];
// Dois alvos: o desfoque gaussiano e separavel, entao uma passada escreve no
// segundo e a outra volta para o primeiro.
static GLuint borFbo[4] = {0,0,0,0}, borTex[4] = {0,0,0,0};
static int borW = 0, borH = 0;

static const char *VS =
  NV_GLSL_PREFIXO
  "attribute vec2 aPos;\n"
  "uniform vec4 uRect;\n"
  "uniform vec2 uTela;\n"
  // MARGEM DE 1 PX NOS MODOS DE SDF. O quad era o proprio rect, entao tudo que
  // o SDF pintasse com d > 0 caia fora dele e era cortado: a metade de fora da
  // rampa da borda sumia nos lados retos (a borda saia dura, com degrau de
  // pixel quando o rect anda em fracao) e o GFX_ANEL, que era centrado na
  // borda, perdia a metade de fora do traco nos lados retos e a mantinha nos
  // cantos — traco de 1 px no reto e 2 px na curva, o contorno "quebrado".
  // Com a margem a rampa inteira cabe no quad; vUv continua 0..1 NO RECT (a
  // margem sai com vUv um pouco fora disso, e o SDF da cobertura 0 la).
  // uMargem e 0 nos modos sem SDF (texto, icones, rampas) e no retangulo de
  // canto vivo (raio 0), que seguem iguais — ver gfx_rect.
  // Rect vazio ou invertido nao cresce: a divisao abaixo nao pode ver zero.
  "uniform float uMargem;\n"
  "varying highp vec2 vUv;\n"
  // vAmb: a posicao do fragmento NA TELA, em 0..1 (x da esquerda, y de
  // baixo — a orientacao de gl_FragCoord), para ler a luz ambiente assada.
  // E o mesmo numero que gl_FragCoord.xy dividido pelo tamanho do alvo dava,
  // so que interpolado do vertice. MEDIDO na C9 (Mali-G71): a coordenada de
  // textura derivada de gl_FragCoord no fragmento custava ~15 ms por passada
  // de tela cheia (home nas fileiras a 45 fps; 60 so com esta troca);
  // interpolada, nada. Nos centros de pixel os dois numeros sao iguais.
  "varying highp vec2 vAmb;\n"
  // uSub: o PEDACO do rect que o quad cobre (x0, y0, x1, y1 em fracao dele;
  // (0,0,1,1) = o rect inteiro). vUv continua medido no rect inteiro, entao o
  // fragmento de um pedaco e o MESMO do desenho inteiro — e o que deixa
  // gfx_sombra_vazada pular o miolo de uma sombra que um painel opaco cobre.
  // A margem de 1 px so cresce nos lados do pedaco que sao borda do rect.
  "uniform vec4 uSub;\n"
  // uGiro: rotacao de grupo (gfx_girar) — cos, sin e o pivo em pixels. O
  // default de um uniform recem-linkado e 0, e por isso gfx_iniciar manda
  // (1, 0, 0, 0) a todo programa: com cos 0 tudo colapsaria no pivo.
  "uniform vec4 uGiro;\n"
  "void main(){\n"
  "  vec2 a = mix(uSub.xy, uSub.zw, aPos);\n"
  "  vec2 sg = mix(-step(uSub.xy, vec2(0.0)), step(vec2(1.0), uSub.zw), aPos);\n"
  "  vec2 e = sg * uMargem * step(0.5, min(uRect.z, uRect.w));\n"
  "  vUv = a + e / max(uRect.zw, vec2(0.5));\n"
  "  vec2 p = uRect.xy + a * uRect.zw + e;\n"
  "  vec2 dg = p - uGiro.zw;\n"
  "  p = uGiro.zw + vec2(dg.x*uGiro.x - dg.y*uGiro.y, dg.x*uGiro.y + dg.y*uGiro.x);\n"
  "  vAmb = vec2(p.x/uTela.x, 1.0-p.y/uTela.y);\n"
  "  gl_Position = vec4(p.x/uTela.x*2.0-1.0, 1.0-p.y/uTela.y*2.0, 0.0, 1.0);\n"
  "}\n";

// O SDF corrige pela proporcao do rect (uAspect), senao o canto de um card
// landscape sai oval.
// UM PROGRAMA POR MODO. Antes isto era um shader unico com um `uniform int
// uModo` e oito caminhos. Mesmo com o if sendo coerente para o desenho inteiro,
// a GPU reserva registradores pelo PIOR caminho do shader, e menos
// registradores livres significa menos fragmentos em voo ao mesmo tempo — a
// Mali-G71 desta TV entregava ~40fps com apenas duas camadas de tela cheia.
// Com um programa enxuto por modo cada desenho usa so o que precisa.
static const char *FS_CABECA =
  NV_GLSL_PREFIXO
  "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
  "varying highp vec2 vUv;\n"
  "varying highp vec2 vAmb;\n"
  "#else\n"
  "varying mediump vec2 vUv;\n"
  "varying mediump vec2 vAmb;\n"
  "#endif\n"
  "uniform sampler2D uTex;\n"
  "uniform float uFoco;\n"
  "uniform float uBorda;\n"
  "uniform float uVarre;\n"
  "uniform vec4  uVeu;     // GFX_CARD: dois veus da base (fracao, alfa)\n"
  "uniform float uDesliza;\n"
  "uniform vec2  uPar;\n"
  "uniform float uRaio;\n"
  "uniform vec4  uCor;\n"
  "uniform float uAspect;\n"
  "uniform float uTexAsp;   // w/h da TEXTURA; 0 = nao ajustar\n"
  "uniform float uForceCover;\n"
  "uniform float uVeuTela;   // GFX_CARD: os veus de borda da tela de perfis, no fragmento (gfx.h)\n"
  "uniform vec4  uJan;\n"
  // A COR DO FUNDO DA PAGINA, para as rampas que dissolvem a arte nela
  // (destaque, destaque cheio, detalhe). Era vec3(0.051) cravado em cada uma;
  // com o tema dinamico estilizado o fundo e tingido (layout.h,
  // NV_COR_FUNDO_R), e a rampa cravada deixaria uma emenda onde a arte acaba.
  "uniform vec3  uFundo;\n"
  // Cor viva: paradas do degrade, relogio, luzes de regiao e o "vazar" das
  // rampas (imersiva: a arte se apaga em alfa em vez de se fundir no fundo,
  // e o que aparece por tras e a luz ambiente). Cada programa so declara de
  // fato o que usa — o resto some no link e volta -1.
  "uniform vec3  uGrad0;\n"
  "uniform vec3  uGrad1;\n"
  "uniform vec3  uGrad2;\n"
  "uniform float uTempo;\n"
  "uniform vec3  uReg0;\n"
  "uniform vec3  uReg1;\n"
  "uniform vec3  uReg2;\n"
  "uniform vec3  uReg3;\n"
  "uniform float uVaza;\n"
  "uniform vec4  uDin;\n"
  // A LUZ AMBIENTE ASSADA, LIDA PELO PROPRIO SHADER DA ARTE (30/09/2026).
  //
  // Com o tema imersivo o destaque de tela cheia sai com alfa = 1 - rampa
  // (uVaza) e e MISTURADO sobre a luz ambiente pintada antes dele: duas
  // camadas de tela cheia, uma delas com a leitura da tela que a mistura
  // exige. MEDIDO na C9 (Mali-G71, home parada, imersiva): 44 fps; sem o
  // destaque, 60; sem a luz, 53. Aqui a luz entra como SEGUNDA TEXTURA
  // (uAmb, unidade 1, em coordenadas de tela) e a mistura e feita no
  // fragmento: mix(c, amb, rampa) e exatamente c*a + amb*(1-a) com
  // a = 1 - rampa, o mesmo pixel do blend, so que OPACO e sem a leitura da
  // tela. O quad da luz por baixo deixa de ser pintado (gfx_ambiente
  // "pendente", ver gfx_rect). uAmbOn liga o caminho; a UV da luz e vAmb, a
  // posicao na tela vinda do vertex shader — nao gl_FragCoord (ver o VS).
  "uniform sampler2D uAmb;\n"
  "uniform float uAmbOn;\n"
  // CAMADAS DO DESTAQUE NUMA PASSADA SO (gfx_hero_camadas, GFX_*_CAM). Durante o
  // crossfade a arte que sai e a que entra eram DUAS passadas de tela cheia
  // misturadas sobre a luz pintada antes: tres
  // telas, duas delas lendo a tela. MEDIDO na C9 (Mali-G71): 42 ms por quadro
  // enquanto durava, em toda troca de destaque da navegacao; a mesma passada
  // opaca, ~4 ms. Aqui o fragmento faz a conta que o blend fazia, na ordem:
  // fundo (luz assada ou cor do fundo), depois a camada B
  // (uTexB/uAlfaB, a arte que sai), depois a camada A (uTex/uCor.a). Cada
  // camada passa pelo mesmo nv_dither que passava sozinha, entao o pixel e o
  // do blend a menos do arredondamento do mixer de 8 bits.
  "uniform sampler2D uTexB;\n"
  "uniform float uTexAspB;\n"
  "uniform float uAlfaB;\n"
  // DITHER DOS DEGRADES (25/09/2026, foto do dono: "o gradiente fica duro").
  //
  // MEDIDO na captura do framebuffer da C9 (Mali-G71, R8G8B8A8): a luz do
  // canto da gaveta de Categorias sobe de (18,18,20) a (23,28,33) em 43 faixas
  // de 1/255 de ~10 px cada; a luz do painel de previa, (20,20,22)->(21,21,24)
  // em faixas de ate 171 px. O degrau e SEMPRE 1/255 — nunca 2 dentro de uma
  // rampa —, entao nao e precisao do shader nem superficie de 16 bits: e o
  // 8 bits do framebuffer num degrade escuro e de pouco contraste, que o OLED
  // mostra como contorno. Remedio: ruido de meio degrau antes de quantizar.
  //
  // O ruido entra NA COR, dividido pelo alfa. Com o blend
  // SRC_ALPHA/ONE_MINUS_SRC_ALPHA o resultado e c*a + dst*(1-a): somar n/a em
  // c desloca o pixel final em exatamente n, sem saber o destino. Ruido no
  // alfa nao serviria — o deslocamento seria n*|c-dst|, diferente por canal e
  // quase nulo num veu sobre fundo escuro. O clamp limita o deslocamento ao
  // fisicamente possivel ([-c*a, (1-c)*a]): num veu preto o ruido so clareia,
  // o que ainda desfaz o contorno. Opaco (a = 1) e o ruido puro.
  //
  // Ruido: interleaved gradient noise (Jimenez) sobre gl_FragCoord, parado no
  // tempo — ruido temporal cintilaria. Precisa de highp (o fract de 52,98*x
  // em fp16 vira padrao); sem highp, Bayer 4x4, exato em fp16. Custo: ~6 ALU.
  "float nv_bayer2(vec2 a){ a = floor(a); return fract(dot(a, vec2(0.5, a.y * 0.75))); }\n"
  "float nv_ruido_bayer(){\n"
  "  vec2 p = mod(gl_FragCoord.xy, 4.0);\n"
  "  return nv_bayer2(p * 0.5) * 0.25 + nv_bayer2(p) + 0.03125;\n"
  "}\n"
  "#if defined(GL_FRAGMENT_PRECISION_HIGH) || !defined(GL_ES)\n"
  "float nv_ruido(){\n"
  "  highp float f = fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715)));\n"
  "  return fract(52.9829189 * f);\n"
  "}\n"
  "#else\n"
  "float nv_ruido(){ return nv_ruido_bayer(); }\n"
  "#endif\n"
  // 2x2 BAYER (levels 0, .5, .75, .25 — the classic matrix), ~7 ALU ops. The
  // 4x4 above costs ~23 ops (two mod/floor/dot/fract rounds plus the tail), and
  // on the GPUs that run with light effects every op per pixel is what the
  // frame is made of: MEASURED on the TCL Smart TV Pro (Mali-G52, GPU timer
  // query, 06/10/2026) ~0.25 ms per shader op per full screen, so a dithered
  // full-screen veil paid ~4 ms for the dither alone. With a 1/255 amplitude
  // four levels already break the band into steps the eye does not resolve.
  "float nv_ruido_leve(){\n"
  "  vec2 p = fract(gl_FragCoord.xy * 0.5);\n"
  "  return p.x + 1.5 * p.y - 4.0 * p.x * p.y + 0.125;\n"
  "}\n"
  // uLeve > 0.5 = EFEITOS LEVES (gpunivel.h, nivel 1): o ruido passa a ser o
  // BAYER 4x4 (mediump, exato em fp16, poucas ALU) em vez do highp. Antes a
  // cor saia SEM ruido nenhum, e as faixas de 8 bits voltavam justamente nas
  // TVs mais fracas — MEDIDO na TCL Smart TV Pro (Mali-G52, fica no nivel 1:
  // "[gpu-nivel] decidido: fica no nivel 1", 40 fps), dono 02/10: "o fundo
  // ta com o degrade ruim na TCL". O desvio e uniforme para o desenho
  // inteiro, entao todo fragmento toma o mesmo lado.
  "uniform float uLeve;\n"
  "vec4 nv_dither(vec3 c, float a){\n"
  "  float n = ((uLeve > 0.5 ? nv_ruido_leve() : nv_ruido()) - 0.5) * (1.0 / 255.0);\n"
  "  return vec4(clamp(c + n / max(a, 0.004), 0.0, 1.0), a);\n"
  "}\n"
  // uAlt = altura do rect em PIXELS DO ALVO. O SDF mede em fracao da altura,
  // entao 1 px la e 1/uAlt; e o que deixa a rampa da borda com 1 px em
  // qualquer tamanho (ver borda() em FS_SDF).
  "uniform float uAlt;\n"
  "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
  "#define NV_HP highp\n"
  "#else\n"
  "#define NV_HP mediump\n"
  "#endif\n"
  // O fundo social (GFX_SOCIAL), como funcao da posicao na tela (y para
  // baixo), para o modo SOCIAL e para servir de fundo em gfx_hero_camadas.
  "vec3 nv_social(vec2 p){\n"
  "  float glow = 1.0-smoothstep(0.0,0.95,length((p-vec2(0.88,0.18))*vec2(1.0,1.25)));\n"
  "  float ribbon = 1.0-smoothstep(0.04,0.40,abs(p.y-0.12-p.x*0.44));\n"
  "  vec3 c = mix(vec3(0.105,0.065,0.095),vec3(0.40,0.14,0.18),glow);\n"
  "  c += vec3(0.065,0.028,0.020)*ribbon*glow;\n"
  "  c = mix(c,vec3(0.047,0.045,0.055),smoothstep(0.44,1.0,p.y));\n"
  "  return c;\n"
  "}\n";

// SDF de retangulo arredondado, corrigido pela proporcao — sem a correcao o
// canto de um card landscape sai oval.
//
// p E q EM highp (onde a GPU tem): o parametro era mediump e rebaixava o vUv
// highp para fp16 na entrada. fp16 tem 11 bits de mantissa: num cartao de
// 1500x900 o p.x anda em degraus de 1/2048 da altura, e a posicao da borda
// erra ate 0,4 px (simulado em fp16; 0,3 px numa linha de Ajustes de
// 1200x96, 0,1 px numa pilula) — com rampa de 1 px isso e borda tremida. q =
// |p| - b e pequeno perto da borda, entao dali em diante mediump basta: so
// as duas linhas pagam highp (erro simulado depois: 0,02 px).
//
// borda(d) e a cobertura com rampa de UM PIXEL centrada na borda. Era
// smoothstep(0.006,-0.006,d): 1,2% da ALTURA, que numa pilula de 56 px da
// 0,7 px (serrilha nas pontas) e num painel de 900 px da 11 px (borda
// borrada). Um clamp linear sobre a distancia em pixels e o antialias de SDF
// de livro, e custa menos que o smoothstep.
static const char *FS_SDF =
  "float sdf(NV_HP vec2 uv, float r, float asp){\n"
  "  NV_HP vec2 p = (uv - 0.5) * vec2(asp, 1.0);\n"
  "  NV_HP vec2 qh = abs(p) - (vec2(0.5*asp, 0.5) - r);\n"
  "  vec2 q = qh;\n"
  "  return min(max(q.x,q.y),0.0) + length(max(q,0.0)) - r;\n"
  "}\n"
  "float borda(float d){ return clamp(0.5 - d * uAlt, 0.0, 1.0); }\n"
  // Rect with square corners (uRaio 0, the full-screen passes): the SDF would
  // give 1 in every pixel of the quad (uAlt is 8192 there, the ramp is a step
  // at the rect edge, and the quad IS the rect). ~15 ops per pixel saved on
  // every full-screen veil, destaque and background; a uniform branch costs
  // nothing on Mali. The rounded case is untouched.
  "float bordaR(NV_HP vec2 uv, float r, float asp){ return r > 0.0 ? borda(sdf(uv, r, asp)) : 1.0; }\n";

// "cover": recorta o excedente em vez de deformar a arte.
static const char *FS_COVER =
  "vec2 coverAsp(vec2 uv, float asp){\n"
  "  if (asp <= 0.0) return uv;\n"
  "  float ra = uAspect / asp;\n"
  "  if (ra > 1.0) uv.y = (uv.y - 0.5) / ra + 0.5;\n"
  "  else          uv.x = (uv.x - 0.5) * ra + 0.5;\n"
  "  return uv;\n"
  "}\n"
  "vec2 cover(vec2 uv){ return coverAsp(uv, uTexAsp); }\n"
  "";

static const char *FS_CORPO[GFX_NMODOS] = {
  // GFX_CARD — arte inteira com cantos e especular no foco (sem zoom nem corte)
  "uniform float uReflexo;\n"
  "void main(){\n"
  "  float d = sdf(vUv, uRaio, uAspect);\n"
  "  float m = borda(d);\n"
  "  if (m <= 0.001) discard;\n"
  // COVER VIRA CONTAIN quando a arte foge muito da moldura (issue #89):
  // catalogo como o Xperience declara fileira deitada mas serve o poster
  // retrato de sempre — cover num card 16:9 cortava ~60% da altura. Aqui
  // a amostragem abre no eixo oposto e a faixa recebe a cor do esqueleto,
  // a mesma do card vazio. Dentro de +/-25% de proporcao segue cover.
  "  float ra = uAspect / max(uTexAsp, 0.01);\n"
  "  float contem = (uForceCover < 0.5 && uTexAsp > 0.05 && (ra < 0.80 || ra > 1.25)) ? 1.0 : 0.0;\n"
  "  vec2 uv0 = cover(vUv);\n"
  "  if (contem > 0.5) {\n"
  "    uv0 = vUv;\n"
  "    if (ra > 1.0) uv0.x = (uv0.x - 0.5) * ra + 0.5;\n"
  "    else          uv0.y = (uv0.y - 0.5) / ra + 0.5;\n"
  "  }\n"
  // SEM OVER-SCAN E SEM ZOOM NO FOCO (issue #176). Havia aqui uma amostragem a
  // 0.94 em repouso e 0.89 com foco (3% e ate 5,5% cortados de cada borda,
  // "reserva de parallax" do Top Shelf da Apple) mais o deslocamento uPar. So
  // que a arte que chega e o cartaz inteiro, e provedores como o TopPosters
  // gravam a nota na BASE da imagem: o foco a cortava justo onde ela mora. Quem
  // cresce no foco e o cartao inteiro (moldura + arte, home.c), nunca a arte
  // por dentro. uPar segue so na luz, que nao corta nada.
  "  vec2 uv = uv0;\n"
  "  vec3 cor = (contem > 0.5 && (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0))\n"
  "    ? vec3(0.173)\n"
  "    : texture2D(uTex, clamp(uv, 0.0, 1.0)).rgb;\n"
  "  if (uFoco > 0.004) {\n"
  // uVarre = a LUZ ENTRANDO (revela.h): a faixa nasce fora do canto
  // inferior esquerdo e desliza ate o repouso; em 0 e o especular de sempre.
  // Um pouco mais forte enquanto anda, para o olho seguir a passagem.
  "    float e = (dot(vUv-0.5, vec2(0.5029,-0.8644)) + uPar.x*3.0 + uVarre) * 3.0;\n"
  "    cor += exp(-e*e) * (0.16 + 0.10*min(abs(uVarre),1.0)) * uFoco * uReflexo;\n"
  "    cor *= (0.80 + 0.20*uFoco);\n"
  // O REBORDO E OPCIONAL (Ajustes > Borda no cartaz em foco). O resto do
  // bloco de foco fica: o cartaz em foco continua mais claro e com o
  // especular, entao desligar a borda nao deixa o foco invisivel.
  "    cor += smoothstep(0.010,0.0,abs(d)) * uFoco * 0.35 * uBorda;\n"
  "  } else cor *= 0.80;\n"
  // THE EDGE VEILS OF THE PROFILE PICKER, IN THE CARD (gfx_card_veu_tela_atual).
  // Over a black background a black veil of alpha v is cor *= (1 - v), so the
  // poster wall can carry the two veils itself: the same ramps as GFX_VEU_TOPO
  // (0..330 px, 0.92) and GFX_VEU_BAIXO (780..1080 px, 0.95), measured on the
  // screen (vAmb.y counts from the bottom). Same pixel, two full-width passes
  // less per frame (0.59 screens). MEASURED on the TCL Smart TV Pro
  // (Mali-G52): the picker frame is fill-bound at ~7-8 ms per screen.
  "  if (uVeuTela > 0.5) {\n"
  "    float sy = 1.0 - vAmb.y;\n"
  "    float vt = smoothstep(1.0, 0.15, sy / 0.3055556) * 0.92;\n"
  "    float tb = clamp((sy - 0.7222222) / 0.2777778, 0.0, 1.0);\n"
  "    float gb = tb * tb * (3.0 - 2.0 * tb); gb = gb * gb * 0.95;\n"
  "    cor *= (1.0 - vt) * (1.0 - gb);\n"
  "  }\n"
  // O VEU DA BASE NO MESMO FRAGMENTO DA ARTE (gfx_veu_card_atual). Desenhado
  // como passada separada, ele e a arte dividiam a MESMA cobertura de borda:
  // no pixel da borda a arte entra com m e o veu escurece so m dela, entao
  // ali sobra arte quase sem veu — um fio claro na base e na curva de baixo
  // do card (fotos do dono, 01/10/2026). Aqui o veu escurece a arte inteira
  // e so depois a borda recorta. Mesma rampa do GFX_BRILHO_TOPO com
  // uPar = (1, 0.42) sobre um retangulo de `fracao` da altura.
  "  if (uVeu.y > 0.0 || uVeu.w > 0.0) {\n"
  "    float yb = 1.0 - vUv.y;\n"
  "    float t1 = 1.0 - smoothstep(0.42, 1.0, yb / max(uVeu.x, 0.001));\n"
  "    float t2 = 1.0 - smoothstep(0.42, 1.0, yb / max(uVeu.z, 0.001));\n"
  "    float v = 1.0 - (1.0 - uVeu.y * t1 * t1) * (1.0 - uVeu.w * t2 * t2);\n"
  "    cor = mix(cor, vec3(0.02, 0.02, 0.03), v);\n"
  "  }\n"
  "  gl_FragColor = vec4(cor, m * uCor.a);\n"
  "}\n",

  // GFX_SOMBRA — mancha difusa atras do item em foco
  "void main(){\n"
  "  float d = sdf(vUv, uRaio, uAspect);\n"
  // MANCHA COM QUEDA RADIAL: 1 no centro, 0 na borda do retangulo, com a
  // curva ao quadrado para a luz morrer devagar. Era smoothstep(0.22,-0.03,d)
  // — um disco solido com 20% de penumbra, que na tela de perfis saia como
  // uma laje colorida. A cor vem de uCor (preto = sombra; a cor de um perfil
  // = luz ambiente em perfilsel.c). Nao havia chamador em src/ antes disso.
  // uPar.x > 0: a queda e de uPar.x PIXELS a partir da borda (o box-shadow
  // do CSS), e nao da metade da altura. Numa peca ALTA e estreita (a rail do
  // menu, 88 x 700) a distancia em fracao da altura nunca passava de ~0,06 e
  // a mancha nao aparecia. uAlt e a altura em px do alvo.
  "  float t = uPar.x > 0.0 ? clamp(-d * uAlt / uPar.x, 0.0, 1.0)\n"
  "                         : clamp(-d * 2.0, 0.0, 1.0);\n"
  "  gl_FragColor = nv_dither(uCor.rgb, t * t * uFoco * uCor.a);\n"
  "}\n",

  // GFX_COR — retangulo/pilula de cor solida
  "void main(){\n"
  "  float m = bordaR(vUv, uRaio, uAspect);\n"
  "  if (m <= 0.001) discard;\n"
  "  gl_FragColor = vec4(uCor.rgb, uCor.a*m);\n"
  "}\n",

  // GFX_HERO — arte da faixa superior, dissolvendo no fundo.
  //
  // As duas rampas sao as do app web, MEDIDAS nos pseudo-elementos de
  // .home-modern-hero-media (getComputedStyle, nao leitura de folha):
  //
  //   ::before  horizontal, cobrindo os 639px ESQUERDOS de 1421 (= 45% da UV):
  //             #0d0d0d -> 0.86 em 22% -> 0.56 em 46% -> 0.16 em 76% -> 0
  //   ::after   vertical, altura toda:
  //             0 ate 82% -> 0.25 em 89.2% -> 0.65 em 95.5% -> solido no fim
  //
  // Sao rampas LINEARES POR PARTES, entao a conta usa clamp e nao smoothstep:
  // um smoothstep unico nao passa pelos pontos intermediarios (em 89.2% dava
  // 0.35 no lugar de 0.25) e e justamente o miolo da rampa que se enxerga.
  //
  // O que estava aqui antes vinha do app da Apple: o fade vertical comecava em
  // 45% da altura, quase o dobro de cedo, e o horizontal MULTIPLICAVA a cor
  // (c*0.35) em vez de fundir no fundo — o que deixava a borda dura visivel em
  // vez de dissolver.
  "void main(){\n"
  "  float xd = vUv.x - uDesliza;\n"
  "  float dentro = step(0.0, xd) * step(xd, 1.0);\n"
  "  vec3 c = texture2D(uTex, clamp(cover(vec2(xd, vUv.y)), 0.0, 1.0)).rgb;\n"
  "  vec3 bg = uFundo;\n"   // #0d0d0d fora do estilizado
  "  float y = vUv.y;\n"
  "  float av = clamp((y-0.820)/0.072,0.0,1.0)*0.25\n"
  "           + clamp((y-0.892)/0.063,0.0,1.0)*0.40\n"
  "           + clamp((y-0.955)/0.045,0.0,1.0)*0.35;\n"
  "  float t = vUv.x/0.45;\n"
  "  float ah = 1.0 - clamp(t/0.22,0.0,1.0)*0.14\n"
  "                 - clamp((t-0.22)/0.24,0.0,1.0)*0.30\n"
  "                 - clamp((t-0.46)/0.30,0.0,1.0)*0.40\n"
  "                 - clamp((t-0.76)/0.24,0.0,1.0)*0.16;\n"
  "  ah *= step(vUv.x, 0.45);\n"
  // uPar.x > 0.5 = SO AS RAMPAS, como veu com alpha: por cima do trailer que
  // toca atras do canvas no lugar da arte (trailer.h). Mesma regra do
  // GFX_DETALHE.
  "  if (uPar.x > 0.5) { vec3 vb = (uAmbOn > 0.5) ? texture2D(uAmb, vAmb).rgb : bg;\n"
  "    gl_FragColor = nv_dither(vb, clamp(ah + av - ah*av, 0.0, 1.0) * uCor.a); return; }\n"
  "  if (uAmbOn > 0.5) { vec3 amb = texture2D(uAmb, vAmb).rgb;\n"
  "    gl_FragColor = nv_dither(mix(c, amb, clamp(ah + av - ah*av, 0.0, 1.0)), 1.0); return; }\n"
  "  if (uVaza > 0.5) { gl_FragColor = nv_dither(c, uCor.a * dentro * (1.0 - clamp(ah + av - ah*av, 0.0, 1.0))); return; }\n"
  "  c = mix(c, bg, clamp(ah + av - ah*av, 0.0, 1.0));\n"
  "  gl_FragColor = nv_dither(c, uCor.a * dentro);\n"
  "}\n",

  // GFX_VEU — escurece a base E a esquerda, onde fica o texto sobreposto
  "void main(){\n"
  "  float m = bordaR(vUv, uRaio, uAspect);\n"
  "  if (m <= 0.001) discard;\n"
  "  float gb = smoothstep(0.34, 1.0, vUv.y);\n"
  "  float ge = smoothstep(0.62, 0.0, vUv.x) * 0.78;\n"
  "  gl_FragColor = nv_dither(vec3(0.0), clamp(gb+ge-gb*ge,0.0,1.0)*uCor.a*m);\n"
  "}\n",

  // GFX_TEXTO — a forma da letra vem do ALPHA da textura, nunca do RGB
  "void main(){\n"
  "  vec4 g = texture2D(uTex, vUv);\n"
  "  gl_FragColor = vec4(g.rgb, g.a * uCor.a);\n"
  "}\n",

  // GFX_FUNDO — arte desfocada por mipmap (uFoco carrega o bias), com o
  // gradiente medido no aparelho: claro no topo, quase preto na base, vinheta
  "void main(){\n"
  "  // A textura ja chega desfocada pelo gaussiano de duas passadas.\n"
  "  vec3 cb = texture2D(uTex, vec2(vUv.x, 1.0 - vUv.y)).rgb;\n"
  // Curva conferida contra uma captura do app da Apple na mesma TV: a base
  // dele fica bem mais escura que a minha estava (L~39 contra L~84 a 5/6 da
  // altura) e a vinheta lateral e bem mais funda (L~55 na borda contra L~139 no
  // centro, no topo). Sem escurecer a base o texto branco das secoes de baixo
  // perde contraste; sem a vinheta a pagina nao tem centro.
  // A queda comeca tarde: no original o fundo se mantem claro ate perto da
  // metade e so entao escurece. Perseguir o brilho ABSOLUTO da referencia seria
  // erro — ele depende da arte do titulo, que e outra — entao o que se copia
  // aqui e a forma da curva.
  // Comeca a cair mais cedo e de mais baixo: com o pico em 1.12 a faixa do
  // meio ficava clara demais e o texto cinza dos cards sem foco sumia dentro
  // dela. O fundo existe para dar cor a pagina, nao para competir com o texto.
  "  float ky = mix(0.92, 0.05, smoothstep(0.16, 0.98, vUv.y));\n"
  "  float vg = 1.0 - 0.66 * smoothstep(0.46, 0.0, min(vUv.x, 1.0 - vUv.x));\n"
  "  gl_FragColor = nv_dither(cb * ky * vg, uCor.a);\n"
  "}\n",

  // GFX_VEU_TOPO — degrade de cima para baixo, sob o cabecalho fixo
  "void main(){\n"
  "  gl_FragColor = nv_dither(vec3(0.0), smoothstep(1.0,0.15,vUv.y)*uCor.a);\n"
  "}\n",

  // GFX_SNAP — imagem ja pronta: sem SDF, sem efeito, so o quad.
  // uPar.y > 0.5 diz que a fonte e um FBO: como o alvo de render tem a origem
  // no canto INFERIOR e o resto do app trabalha com y crescendo para baixo, a
  // imagem sai de cabeca para baixo se lida direto.
  // uFoco > 0: um veu de cor uCor.rgb com essa forca na MESMA passada (o veu
  // de 28% da "Arte borrada", gfx_luz_canal_desenhar); mix com 0 devolve a
  // textura exata. uPar.x > 0.5: a cor sai pelo nv_dither (o Frost). Os outros
  // chamadores passam 0 nos dois.
  "void main(){\n"
  "  vec2 uv = (uPar.y > 0.5) ? vec2(vUv.x, 1.0 - vUv.y) : vUv;\n"
  "  vec3 c = mix(texture2D(uTex, uv).rgb, uCor.rgb, uFoco);\n"
  "  gl_FragColor = uPar.x > 0.5 ? nv_dither(c, uCor.a) : vec4(c, uCor.a);\n"
  "}\n",

  // GFX_PLAY — triangulo apontando para a direita. Existe como primitiva
  // porque depender do glifo U+25B6 da fonte e loteria: se a familia embarcada
  // nao tiver o caractere, o simbolo simplesmente nao aparece, e desenhar um
  // retangulo no lugar (o que eu tinha feito) fica pior que nao ter nada.
  "void main(){\n"
  "  float dy = abs(vUv.y - 0.5) * 2.0;\n"
  "  float m = smoothstep(0.02, -0.02, vUv.x - (1.0 - dy));\n"
  "  if (m <= 0.001) discard;\n"
  "  gl_FragColor = vec4(uCor.rgb, uCor.a * m);\n"
  "}\n",

  // GFX_BLUR — uma passada de desfoque gaussiano de 9 amostras. uPar da a
  // direcao e o passo (horizontal numa passada, vertical na outra). Separar em
  // duas passadas custa 18 leituras em vez das 81 de um kernel 9x9.
  "void main(){\n"
  "  vec3 c = texture2D(uTex, vUv).rgb * 0.1633;\n"
  "  c += (texture2D(uTex, vUv + uPar).rgb        + texture2D(uTex, vUv - uPar).rgb)        * 0.1531;\n"
  "  c += (texture2D(uTex, vUv + uPar*2.0).rgb    + texture2D(uTex, vUv - uPar*2.0).rgb)    * 0.1224;\n"
  "  c += (texture2D(uTex, vUv + uPar*3.0).rgb    + texture2D(uTex, vUv - uPar*3.0).rgb)    * 0.0836;\n"
  "  c += (texture2D(uTex, vUv + uPar*4.0).rgb    + texture2D(uTex, vUv - uPar*4.0).rgb)    * 0.0477;\n"
  "  gl_FragColor = vec4(c, 1.0);\n"
  "}\n",

  // GFX_DETALHE — backdrop da tela de titulo, ja com a vinheta.
  //
  // MEDIDO no app web (getComputedStyle em .series-detail-vignette, nao leitura
  // de folha): um linear-gradient(90deg) de #0d0d0d indo a transparente, com
  // NOVE paradas — 0%:1.00  7.8%:0.95  17.16%:0.84  28.08%:0.70  40.56%:0.52
  // 51.48%:0.34  60.84%:0.18  70.2%:0.07  78%:0. Depois de 78% a arte aparece
  // limpa. Como no hero da home, sao rampas LINEARES POR PARTES: um smoothstep
  // unico erra o miolo, que e justamente onde o texto branco se apoia.
  //
  // A arte entra em "cover" com ancoragem CENTRAL, que e o que o web faz na
  // pratica: a regra e `background-position:100% 0`, mas o backdrop e 16:9 num
  // quadro 16:9 e nao sobra nada para deslocar.
  "void main(){\n"
  "  vec3 c = texture2D(uTex, clamp(cover(vUv), 0.0, 1.0)).rgb;\n"
  "  vec3 bg = uFundo;\n"   // #0d0d0d fora do estilizado
  "  float x = vUv.x;\n"
  // CURVA LISA NO LUGAR DAS RAMPAS (dono, 29/09/2026: "o gradiente do
  // detalhe ainda esta duro"). As oito rampas lineares por partes do web tem
  // quinas de inclinacao em cada parada; na TV, com 8 bits por canal, cada
  // quina vira uma faixa visivel, e a ultima (78%) termina com inclinacao
  // nao-nula — a arte "comeca" numa linha. 1 - smoothstep(0, 0.82, x) passa
  // pelas mesmas paradas com erro <= 0,05 (0.975/0.887/0.507/0.166/0.055 contra
  // 0.95/0.84/0.52/0.18/0.07) e chega a zero com inclinacao zero.
  //
  // E a BASE: a arte terminava seca na borda de baixo, onde a pagina continua
  // no fundo liso. Uma rampa vertical nos ultimos 30% leva a arte ao fundo sem
  // borda; as duas se combinam como camadas (1-(1-a)(1-b)).
  "  float a = 1.0 - smoothstep(0.0, 0.82, x);\n"
  "  float ab = smoothstep(0.70, 1.0, vUv.y) * 0.92;\n"
  "  a = 1.0 - (1.0 - a) * (1.0 - ab);\n"
  // uFoco = FORCA da vinheta: 1 no topo, 0 com a pagina rolada. No web a
  // vinheta e uma CAMADA IRMA do backdrop e tem opacidade propria — ao rolar,
  // `.detail-scrolled` leva a arte a 0.15 E a vinheta a 0 (components.css:17348).
  // Aqui os dois estao fundidos num modo so, por fill rate (ver gfx.h:21-25),
  // entao a opacidade da vinheta precisa entrar como uniforme. Sem isto ela
  // ficava em forca TOTAL sobre uma arte ja a 15%, e os 78% da esquerda — que e
  // exatamente onde o texto se apoia — viravam preto solido.
  // uPar.x > 0.5 = SO A VINHETA, como veu com alpha, sem textura. E o que a
  // pagina desenha por cima do trailer (trailer.h): o video e um plano ATRAS
  // do canvas, visto por um furo, e o texto do titulo precisa do mesmo
  // escuro a esquerda que teria sobre a arte. Mesmo perfil, mesma uFoco.
  "  if (uPar.x > 0.5) { gl_FragColor = nv_dither(bg, clamp(a,0.0,1.0) * uFoco * uCor.a); return; }\n"
  "  if (uAmbOn > 0.5) { vec3 amb = texture2D(uAmb, vAmb).rgb;\n"
  "    gl_FragColor = nv_dither(mix(c, amb, clamp(a,0.0,1.0) * uFoco), 1.0); return; }\n"
  "  if (uVaza > 0.5) { gl_FragColor = nv_dither(c, uCor.a * (1.0 - clamp(a,0.0,1.0) * uFoco)); return; }\n"
  "  c = mix(c, bg, clamp(a,0.0,1.0) * uFoco);\n"
  "  gl_FragColor = nv_dither(c, uCor.a);\n"
  "}\n",

  // GFX_HERO_CHEIO — hero ocupando a tela inteira.
  //
  // MEDIDO nos pseudo-elementos de .home-modern-hero-media com
  // `modernHeroFullScreenBackdropEnabled` ligado (1920x1062 em 0,0):
  //
  //   ::before  horizontal, cobrindo os 1248px ESQUERDOS de 1920 (= 65%):
  //             #0d0d0d -> 0.90 em 22% -> 0.80 em 46% -> 0.42 em 76% -> 0
  //   ::after   vertical, altura toda:
  //             0 ate 64% -> 0.35 em 74.8% -> 0.75 em 85.6% -> solido no fim
  //
  // As paradas percentuais sao as MESMAS do hero em faixa; o que muda e a
  // cobertura (65% da largura em vez de 45%) e a profundidade. Faz sentido: com
  // a arte ocupando a tela toda, o texto precisa de mais fundo escuro sob ele.
  "void main(){\n"
  "  float xd = vUv.x - uDesliza;\n"
  "  float dentro = step(0.0, xd) * step(xd, 1.0);\n"
  "  vec3 c = texture2D(uTex, clamp(cover(vec2(xd, vUv.y)), 0.0, 1.0)).rgb;\n"
  "  vec3 bg = uFundo;\n"
  "  float y = vUv.y;\n"
  "  float av = clamp((y-0.640)/0.108,0.0,1.0)*0.35\n"
  "           + clamp((y-0.748)/0.108,0.0,1.0)*0.40\n"
  "           + clamp((y-0.856)/0.144,0.0,1.0)*0.25;\n"
  "  float t = vUv.x/0.65;\n"
  "  float ah = 1.0 - clamp(t/0.22,0.0,1.0)*0.10\n"
  "                 - clamp((t-0.22)/0.24,0.0,1.0)*0.10\n"
  "                 - clamp((t-0.46)/0.30,0.0,1.0)*0.38\n"
  "                 - clamp((t-0.76)/0.24,0.0,1.0)*0.42;\n"
  "  ah *= step(vUv.x, 0.65);\n"
  "  if (uPar.x > 0.5) { vec3 vb = (uAmbOn > 0.5) ? texture2D(uAmb, vAmb).rgb : bg;\n"
  "    gl_FragColor = nv_dither(vb, clamp(ah + av - ah*av, 0.0, 1.0) * uCor.a); return; }\n"
  "  if (uAmbOn > 0.5) { vec3 amb = texture2D(uAmb, vAmb).rgb;\n"
  "    gl_FragColor = nv_dither(mix(c, amb, clamp(ah + av - ah*av, 0.0, 1.0)), 1.0); return; }\n"
  "  if (uVaza > 0.5) { gl_FragColor = nv_dither(c, uCor.a * dentro * (1.0 - clamp(ah + av - ah*av, 0.0, 1.0))); return; }\n"
  "  c = mix(c, bg, clamp(ah + av - ah*av, 0.0, 1.0));\n"
  "  gl_FragColor = nv_dither(c, uCor.a * dentro);\n"
  "}\n",

  // GFX_ANEL — contorno, cheio ou tracejado, sem miolo.
  //
  // Existe porque o selo de "episodio nao assistido" da pagina de titulo e um
  // ANEL, e com GFX_COR saia um disco cinza. Pintar o miolo da cor do fundo nao
  // resolve: ali o veu esta em 0.06 e o fundo aparece atraves dele, entao o
  // "tampao" ficaria visivel como uma mancha mais clara.
  //
  // O SDF ja existente da a distancia com sinal ate a borda; um anel e
  // simplesmente `abs(d) < espessura`. Por isso este modo custa o mesmo que
  // GFX_COR e serve para retangulo arredondado tanto quanto para circulo (raio
  // 0.5 no menor lado = circulo).
  //
  // Parametros, reaproveitando uPar para nao criar uniform novo:
  //   uPar.x = espessura do traco, na mesma escala normalizada de uRaio
  //            (fracao da ALTURA do rect: 2 px = 2.0/r.h — gfx_anel faz a conta)
  //   uPar.y = numero de tracos do pontilhado; 0 (ou <0.5) = anel continuo
  //
  // O TRACO FICA POR DENTRO DO RECT, de d = -esp ate d = 0, com rampa de 1 px
  // nas duas bordas. Era `smoothstep(esp, esp*0.55, abs(d))`: um traco CENTRADO
  // na borda, com a metade de fora caindo fora do quad. Nos lados retos essa
  // metade era cortada (sobrava o lado de dentro, com a borda de fora dura) e
  // nos cantos ela aparecia. Medido: na pilula "Depois" (56 px, traco de
  // 1,5 px) uma linha de pixel no reto e duas na diagonal da curva; no anel
  // de 3 px do Reproduzir, 2,3 px no reto e 4,4 px a 45 graus. E o mesmo
  // "squircle" que agendaui.c e salvospainel.c descrevem num circulo: reto
  // nos quatro lados (o corte do quad) e cheio nas diagonais. E a rampa era
  // 45% da espessura: abaixo de ~2 px ela tinha menos de 1 px e serrilhava.
  // Agora a espessura e a mesma em todo o contorno e igual a pedida, e a borda
  // de fora do anel coincide com a do rect: anel e miolo desenhados no mesmo
  // rect dao UMA borda so. Anel por fora de outra peca: gfx_anel_fora.
  "void main(){\n"
  "  float d = sdf(vUv, uRaio, uAspect);\n"
  "  float esp = max(uPar.x, 0.0015);\n"
  "  float m = borda(d) * clamp(0.5 + (d + esp) * uAlt, 0.0, 1.0);\n"
  "  if (m <= 0.002) discard;\n"
  "  if (uPar.y > 0.5) {\n"
  "    vec2 p = (vUv - 0.5) * vec2(uAspect, 1.0);\n"
  "    float t = fract((atan(p.y, p.x) / 6.2831853 + 0.5) * uPar.y);\n"
  // Ciclo de 50%: metade traco, metade vao, com as pontas suavizadas para o
  // pontilhado nao cintilar quando o circulo e pequeno.
  "    m *= smoothstep(0.56, 0.44, t);\n"
  "    if (m <= 0.002) discard;\n"
  "  }\n"
  "  gl_FragColor = vec4(uCor.rgb, uCor.a * m);\n"
  "}\n",

  // GFX_OLHO — o olho de "marcar assistido".
  //
  // A lente e a INTERSECCAO de dois discos de raio grande deslocados para cima
  // e para baixo; e a construcao classica da forma de amendoa, e sai mais
  // barata (duas distancias) que tentar dois arcos de Bezier. O contorno e
  // `abs(d) < esp`, como no GFX_ANEL, e a iris e um disco cheio no centro.
  //
  // uPar.x > 0.5 acrescenta o risco na diagonal (estado "nao assistido"): uma
  // faixa em torno da reta y = x, com a borda apagada dos dois lados para o
  // traco nao serrilhar.
  "void main(){\n"
  "  vec2 p = (vUv - 0.5) * vec2(uAspect, 1.0);\n"
  // Centros a +-0.62 e raio 0.78: a amendoa resultante tem cerca de 1.0 de
  // largura por 0.32 de altura, que e a proporcao do glifo do web.
  "  float d = max(length(p - vec2(0.0, 0.62)) - 0.78,\n"
  "                length(p + vec2(0.0, 0.62)) - 0.78);\n"
  // Traco de 0.055 e nao 0.038: ao lado de um "+" de 5px o contorno fino fazia
  // o olho parecer de outra familia de icone. A iris tambem cresceu.
  "  float esp = 0.055;\n"
  "  float m = smoothstep(esp, esp*0.45, abs(d));\n"
  "  m = max(m, smoothstep(0.185, 0.160, length(p)));\n"
  // O risco: apaga um sulco no olho e desenha a barra dentro dele, para que o
  // traco se leia por cima da lente como no SVG (que usa dois caminhos).
  // O risco atravessa o olho inteiro, com um sulco de fundo para ele se
  // destacar por cima da lente — e o que o SVG faz com dois caminhos.
  "  if (uPar.x > 0.5) {\n"
  "    float r = (p.x - p.y) * 0.7071;\n"
  "    m *= smoothstep(0.045, 0.075, abs(r));\n"
  "    float lim = step(max(abs(p.x), abs(p.y) * 1.6), 0.60);\n"
  "    m = max(m, smoothstep(0.045, 0.026, abs(r)) * lim);\n"
  "  }\n"
  "  if (m <= 0.002) discard;\n"
  "  gl_FragColor = vec4(uCor.rgb, uCor.a * m);\n"
  "}\n",

  // GFX_FONTES — tres barras empilhadas, a de baixo mais curta: e o simbolo de
  // "lista de fontes". Substituiu o glifo do YouTube no terceiro botao redondo:
  // o app nao toca trailer do YouTube, e um botao que promete o que nao faz e
  // pior que um botao com outra funcao.
  "void main(){\n"
  "  vec2 p = (vUv - 0.5) * vec2(uAspect, 1.0);\n"
  // Tres barras de 0.12 de altura, centradas em -0.28, 0 e +0.28. A de baixo
  // tem metade da largura, que e o que faz o simbolo ler como lista e nao como
  // grade.
  "  float m = 0.0;\n"
  "  for (int i = 0; i < 3; i++) {\n"
  "    float cy = (float(i) - 1.0) * 0.28;\n"
  "    float larg = (i == 2) ? 0.24 : 0.46;\n"
  "    vec2 q = abs(p - vec2(0.0, cy)) - vec2(larg, 0.06) + 0.06;\n"
  "    float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - 0.06;\n"
  "    m = max(m, smoothstep(0.012, -0.012, d));\n"
  "  }\n"
  "  if (m <= 0.002) discard;\n"
  "  gl_FragColor = vec4(uCor.rgb, uCor.a * m);\n"
  "}\n",

  // GFX_MARCA — a forma vem do ALPHA, a cor de uCor. Ver a nota em gfx.h.
  "void main(){\n"
  "  float m = texture2D(uTex, vUv).a;\n"
  "  gl_FragColor = vec4(uCor.rgb, uCor.a * m);\n"
  "}\n",

  // GFX_VEU_BAIXO — vertical puro, transparente em cima. Ver a nota em gfx.h.
  //
  // A COR VEM DE uCor.rgb e nao e mais preto cravado. Preto cravado obriga quem
  // usa o veu para APAGAR uma arte contra o fundo da pagina a deixar uma
  // emenda: o veu converge para (0,0,0), o fundo e #0D0D0D, e onde a arte
  // termina fica um degrau reto de ponta a ponta da tela. Foi o que a captura
  // da escolha de perfil mostrou. Os tres chamadores anteriores ja passavam
  // 0,0,0, entao para eles nada muda.
  "void main(){\n"
  "  float t = clamp(vUv.y, 0.0, 1.0);\n"
  "  float g = t * t * (3.0 - 2.0 * t);\n"
  "  g = g * g;\n"
  "  gl_FragColor = nv_dither(uCor.rgb, g * uCor.a);\n"
  "}\n",
  // GFX_SOCIAL: broad off-centre light, quiet left side for copy.
  "void main(){\n"
  "  gl_FragColor = nv_dither(nv_social(vUv),uCor.a);\n"
  "}\n",

  // GFX_AVATAR: mascara radial exata. O GFX_CARD usa o SDF de retangulo
  // arredondado e over-scan de parallax; num circulo pequeno isso deixava a
  // aresta irregular e deslocava a fotografia dentro do disco.
  "void main(){\n"
  "  vec2 p=(vUv-0.5)*vec2(uAspect,1.0);\n"
  "  float d=length(p);\n"
  // Rampa de 1 px POR DENTRO do disco (o quad deste modo nao tem margem).
  // Era smoothstep(0.500,0.486): 1,4% da altura, 0,4 px num disco de 27.
  "  float m=clamp((0.5-d)*uAlt,0.0,1.0);\n"
  "  if(m<=0.001) discard;\n"
  "  vec3 c=texture2D(uTex,clamp(cover(vUv),0.0,1.0)).rgb;\n"
  "  gl_FragColor=vec4(c,m*uCor.a);\n"
  "}\n",

  // GFX_RETRATO: preserva o enquadramento vertical do profile still e o
  // ancora a direita. Fora da fotografia o shader fica transparente, deixando
  // o hero de base aparecer sem a emenda de um segundo painel.
  "void main(){\n"
  // O pipeline pode entregar JPEG/RGB ou PNG com alpha real. Nao tentamos
  // adivinhar o fundo por luminancia: cabelo e roupa escuros tambem sao pixels
  // validos e um chroma-key heuristico os apagaria. Sem matte, o fallback e a
  // foto inteira com uma dissolucao de borda segura; com alpha, a silhueta
  // fornecida pela origem permanece intacta.
  // Zoom editorial: a referencia nao mostra o retrato inteiro; mostra a
  // cabeca ocupando o hero e saindo pela borda direita. O recorte vertical
  // amplia o rosto sem esticar a textura.
  "  float cropY=0.05;\n"
  "  float cropH=0.78;\n"
  "  float dispW=clamp((uTexAsp/uAspect)/cropH,0.46,0.90);\n"
  "  float x0=1.0-dispW;\n"
  "  float localX=clamp((vUv.x-x0)/dispW,0.0,1.0);\n"
  "  vec2 uv=vec2(localX,cropY+vUv.y*cropH);\n"
  "  float inside=step(x0,vUv.x)*step(vUv.x,1.0);\n"
  "  vec4 pix=texture2D(uTex,clamp(uv,0.0,1.0));\n"
  "  vec3 c=pix.rgb;\n"
  // Dissolve amplo nas quatro bordas: o retrato se mistura com o banner em
  // vez de denunciar um retangulo cinza. O centro continua inteiro para o
  // rosto manter detalhe e contraste.
  "  float left=smoothstep(0.0,0.28,localX);\n"
  "  float right=1.0-smoothstep(0.82,1.0,localX);\n"
  "  float top=smoothstep(0.0,0.12,vUv.y);\n"
  "  float bottom=1.0-smoothstep(0.68,0.99,vUv.y);\n"
  "  float mask=inside*left*right*top*bottom*pix.a;\n"
  "  if(mask<=0.001) discard;\n"
  "  gl_FragColor=vec4(c,uCor.a*mask);\n"
  "}\n",

  // GFX_DISCO: preenchimento circular com antialias. Ao ficar atras do avatar
  // produz um aro perfeito sem esconder pixels da imagem nem criar rebarbas.
  "void main(){\n"
  "  vec2 p=(vUv-0.5)*vec2(uAspect,1.0);\n"
  "  float m=clamp((0.5-length(p))*uAlt,0.0,1.0);\n"
  "  if(m<=0.001) discard;\n"
  "  gl_FragColor=vec4(uCor.rgb,uCor.a*m);\n"
  "}\n",

  // Production illustration: do not crop, recolor, flatten or add fake detail.
  // The caller fits the source aspect within the header, anchored right.
  "void main(){\n"
  "  vec4 c=texture2D(uTex,vUv);\n"
  "  float edge=smoothstep(0.0,0.32,vUv.x);\n"
  "  edge*=smoothstep(0.0,0.045,vUv.y);\n"
  "  edge*=1.0-smoothstep(0.84,1.0,vUv.y);\n"
  "  gl_FragColor=vec4(c.rgb,c.a*uCor.a*edge);\n"
  "}\n",

  // GFX_VEU_CARD — a rampa de cinco paradas do card de episodio, por pixel.
  //
  // Escrita como tres mix() encadeados em vez de um laco: GLSL ES 1.00 nao
  // garante laco com limite variavel, e tres mix compilam para o mesmo punhado
  // de instrucoes que o laco geraria.
  "void main(){\n"
  "  float m = bordaR(vUv, uRaio, uAspect);\n"
  "  if (m <= 0.001) discard;\n"
  "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
  "  highp float t = clamp(vUv.y, 0.0, 1.0);\n"
  "  highp float g = mix(0.06, 0.18, smoothstep(0.00, 0.22, t));\n"
  "  g = mix(g, 0.62, smoothstep(0.22, 0.52, t));\n"
  "  g = mix(g, 0.86, smoothstep(0.52, 0.82, t));\n"
  "  g = mix(g, 0.95, smoothstep(0.82, 1.00, t));\n"
  "#else\n"
  "  mediump float t = clamp(vUv.y, 0.0, 1.0);\n"
  "  float g = mix(0.06, 0.18, smoothstep(0.00, 0.22, t));\n"
  "  g = mix(g,   0.62, smoothstep(0.22, 0.52, t));\n"
  "  g = mix(g,   0.86, smoothstep(0.52, 0.82, t));\n"
  "  g = mix(g,   0.95, smoothstep(0.82, 1.00, t));\n"
  "#endif\n"
  "  gl_FragColor = nv_dither(uCor.rgb, uCor.a * g * m);\n"
  "}\n",

  // GFX_BRILHO_TOPO — realce claro no alto, rampa por pixel, cantos do card.
  //
  // A rampa e o smoothstep AO QUADRADO, pelo mesmo motivo escrito em
  // GFX_VEU_BAIXO: com a rampa linear o olho enxerga a segunda derivada e
  // aparece uma emenda onde ela comeca — que e exatamente o "risco" que um
  // retangulo chapado ja fazia, so que mais fraco.
  "void main(){\n"
  "  float d = sdf(vUv, uRaio, uAspect);\n"
  "  float m = borda(d);\n"
  "  if (m <= 0.001) discard;\n"
  // uPar.y > 0 VIRA A RAMPA DE BAIXO PARA CIMA (veu escuro sob a legenda do
  // cartao deitado): cheia da base ate uPar.y e zero em uPar.x, medidos da
  // base. Com uPar.y = 0 a conta e a de sempre, o realce do topo.
  "  float yy = uPar.y > 0.0 ? 1.0 - vUv.y : vUv.y;\n"
  "  float t = 1.0 - smoothstep(uPar.y, max(uPar.x, uPar.y + 0.001), yy);\n"
  "  gl_FragColor = nv_dither(uCor.rgb, uCor.a * t * t * m);\n"
  "}\n",

  // GFX_ARTE — a textura intacta, recortada pelos cantos. Ver a nota em gfx.h.
  "void main(){\n"
  "  float m = bordaR(vUv, uRaio, uAspect);\n"
  "  if (m <= 0.001) discard;\n"
  "  vec4 t = texture2D(uTex, vUv);\n"
  // uFoco > 0 = ARTE DE TELA CHEIA COM O VEU DOS AJUSTES NA MESMA PASSADA (so
  // com arte opaca e alfa 1, ver gfx_arte_veu): preto de alfa V(x), V = uFoco
  // no meio, subindo ate uFoco + (1-uFoco)*uPar.x na borda esquerda e
  // uFoco + (1-uFoco)*uPar.y na direita — o mesmo degrade das duas metades de
  // GFX_VEU_CSS, com o mesmo ruido (so clareia, como o do veu preto).
  "  if (uFoco > 0.0) {\n"
  "    float v = uFoco + (1.0 - uFoco) * (vUv.x < 0.5 ? uPar.x * (1.0 - 2.0 * vUv.x) : uPar.y * (2.0 * vUv.x - 1.0));\n"
  "    gl_FragColor = vec4(t.rgb * (1.0 - v) + nv_dither(vec3(0.0), 1.0).rgb, 1.0);\n"
  "    return;\n"
  "  }\n"
  "  gl_FragColor = vec4(t.rgb, t.a * uCor.a * m);\n"
  "}\n",

  // GFX_LUZ — a queda radial do GFX_SOMBRA, presa aos cantos do painel. A
  // distancia e medida em fracao da ALTURA nos dois eixos (o x multiplica por
  // uAspect), senao a luz sai oval num painel deitado.
  "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
  "#define GFX_LUZ_PREC highp\n"
  "#else\n"
  "#define GFX_LUZ_PREC mediump\n"
  "#endif\n"
  "void main(){\n"
  "  float m = bordaR(vUv, uRaio, uAspect);\n"
  "  if (m <= 0.001) discard;\n"
  "  GFX_LUZ_PREC vec2 p = (vUv - uPar) * vec2(uAspect, 1.0);\n"
  // O falloff linear ao quadrado ainda mostrava um limite circular em TVs com
  // pouca precisao de fragmento. Usar highp quando a GPU oferece evita que
  // mediump arredonde as faixas de alfa; o easing cubico abre a penumbra e
  // deixa o falloff no mesmo formato nas TVs GLES2 sem highp.
  "  GFX_LUZ_PREC float t = clamp(1.0 - length(p) / max(uFoco, 0.001), 0.0, 1.0);\n"
  "  GFX_LUZ_PREC float suave = t * t * (3.0 - 2.0 * t);\n"
  "  gl_FragColor = nv_dither(uCor.rgb, suave * suave * uCor.a * m);\n"
  "}\n",

  // GFX_SINO — contorno de sino pequeno e resolvido no fragmento. E usado
  // pelo toast porque uma textura nova poderia ainda estar no fio de decode
  // quando o aviso entra; um glifo nativo precisa estar presente no primeiro
  // frame e continua limpo quando a TV reduz a escala.
  "void main(){\n"
  "  vec2 p = (vUv - 0.5) * vec2(uAspect, 1.0);\n"
  "  float dome = length((p - vec2(0.0, 0.035)) / vec2(0.275, 0.285)) - 1.0;\n"
  "  float shell = 1.0 - smoothstep(0.008, 0.045, abs(dome));\n"
  "  shell *= 1.0 - step(0.225, p.y);\n"
  "  vec2 qb = abs(p - vec2(0.0, 0.225)) - vec2(0.31, 0.035);\n"
  "  float bd = length(max(qb, 0.0)) + min(max(qb.x, qb.y), 0.0) - 0.035;\n"
  "  float base = 1.0 - smoothstep(0.008, 0.026, bd);\n"
  "  float top = 1.0 - smoothstep(0.010, 0.028, length((p - vec2(0.0, -0.285)) / vec2(0.045, 0.045)) - 1.0);\n"
  "  float clapper = 1.0 - smoothstep(0.010, 0.028, length((p - vec2(0.0, 0.300)) / vec2(0.055, 0.040)) - 1.0);\n"
  "  float m = max(max(shell, base), max(top, clapper));\n"
  "  if (m <= 0.002) discard;\n"
  "  gl_FragColor = vec4(uCor.rgb, uCor.a * m);\n"
  "}\n",

  // GFX_ESQUELETO — o card CARREGANDO: a superficie #2C2C2C com uma faixa
  // clara que atravessa a TELA (nao cada card por si). uPar.x = onde a onda
  // esta, em fracao da largura da tela; uPar.y = x do card, na mesma fracao;
  // uFoco = largura do card, idem. Assim os cards de uma fileira acendem em
  // sequencia, como uma so luz passando, e a conta e a mesma do GFX_COR mais
  // um exp — nenhum desenho a mais.
  "void main(){\n"
  "  float m = bordaR(vUv, uRaio, uAspect);\n"
  "  if (m <= 0.001) discard;\n"
  "  float x = uPar.y + vUv.x * uFoco + (1.0 - vUv.y) * 0.05 - uPar.x;\n"
  "  float b = exp(-x*x*70.0);\n"
  "  gl_FragColor = vec4(uCor.rgb + b * 0.05, uCor.a * m);\n"
  "}\n",

  // GFX_LINHA — distancia ao segmento; nucleo liso e halo quadratico. Tudo em
  // unidades da altura do retangulo, que e a unica escala que o shader tem.
  "void main(){\n"
  "  vec2 p = vUv * vec2(uAspect, 1.0);\n"
  "  float g = uRaio;\n"
  "  vec2 a = vec2(g, uPar.x > 0.5 ? 1.0 - g : g);\n"
  "  vec2 b = vec2(uAspect - g, uPar.x > 0.5 ? g : 1.0 - g);\n"
  "  vec2 ab = b - a;\n"
  "  float t = clamp(dot(p - a, ab) / max(dot(ab, ab), 0.000001), 0.0, 1.0);\n"
  "  float d = length(p - a - ab * t);\n"
  "  float nucleo = 1.0 - smoothstep(uFoco * 0.5, uFoco * 1.6, d);\n"
  "  float h = clamp(1.0 - d / max(g, 0.0001), 0.0, 1.0);\n"
  "  float m = max(nucleo, h * h * 0.32);\n"
  "  if (m <= 0.003) discard;\n"
  "  gl_FragColor = vec4(uCor.rgb, uCor.a * m);\n"
  "}\n",

  // GFX_CEU — hash de celula sem seno (o seno de numero grande em mediump vira
  // listra na Mali); highp so onde existe.
  "\n#ifdef GL_ES\n#ifdef GL_FRAGMENT_PRECISION_HIGH\nprecision highp float;\n#endif\n#endif\n"
  "float h12(vec2 c){\n"
  "  vec3 p3 = fract(vec3(c.xyx) * 0.1031);\n"
  "  p3 += dot(p3, p3.yzx + 33.33);\n"
  "  return fract((p3.x + p3.y) * p3.z);\n"
  "}\n"
  "void main(){\n"
  "  vec2 q = vUv * vec2(uAspect, 1.0);\n"
  "  vec2 gr = q * 58.0 + uPar;\n"
  "  vec2 c = floor(gr);\n"
  "  vec2 f = fract(gr) - 0.5;\n"
  "  float h = h12(c);\n"
  "  float h2 = fract(h * 17.13);\n"
  "  vec2 o = (vec2(h2, fract(h * 31.7)) - 0.5) * 0.55;\n"
  "  float d = length(f - o);\n"
  "  float tw = 0.62 + 0.38 * sin(uFoco * (0.6 + 1.8 * h2) + h * 40.0);\n"
  "  float s = step(0.972, h) * (1.0 - smoothstep(0.02, 0.07 + 0.10 * h2 * h2, d)) * tw * (0.35 + 0.65 * h2);\n"
  "  float s2 = step(0.996, h) * (1.0 - smoothstep(0.0, 0.40, d)) * 0.20;\n"
  // A coluna de leitura (direita) fica com metade das estrelas.
  "  s *= 1.0 - 0.55 * smoothstep(0.68, 0.72, vUv.x);\n"
  "  float n1 = 1.0 - smoothstep(0.0, 0.95, length((q - vec2(uAspect * 0.36, 0.50)) * vec2(0.62, 1.0)));\n"
  "  float n2 = 1.0 - smoothstep(0.0, 0.70, length((q - vec2(uAspect * 0.80, 0.18)) * vec2(0.80, 1.0)));\n"
  "  vec3 base = vec3(0.018, 0.019, 0.030) + uCor.rgb * (0.20 * n1 * n1) + vec3(0.20, 0.24, 0.42) * (0.07 * n2 * n2);\n"
  "  gl_FragColor = nv_dither(base + vec3(0.82, 0.86, 1.0) * (s + s2), 1.0);\n"
  "}\n",

  // GFX_COR_GRAD — o GFX_COR com as tres paradas do degrade no lugar da cor.
  // A direcao e quase horizontal (78/22): num botao deitado o degrade inteiro
  // cabe no que o olho ve, e a sombra fica no canto de baixo a direita, onde a
  // luz de uma superficie iluminada de cima-esquerda cairia. O deslocamento
  // lento (uTempo) e a luz passeando pelo material, ~9 s por volta; o brilho
  // de 6% no topo e o que faz a superficie ler como material e nao como tinta.
  "vec3 grad3(float t){\n"
  "  t = clamp(t, 0.0, 1.0);\n"
  "  return t < 0.5 ? mix(uGrad0, uGrad1, t * 2.0) : mix(uGrad1, uGrad2, t * 2.0 - 1.0);\n"
  "}\n"
  "void main(){\n"
  "  float m = bordaR(vUv, uRaio, uAspect);\n"
  "  if (m <= 0.001) discard;\n"
  "  float t = vUv.x * 0.78 + vUv.y * 0.22 + 0.10 * sin(uTempo * 0.7);\n"
  "  vec3 c = grad3(t) + (1.0 - vUv.y) * 0.06;\n"
  "  gl_FragColor = nv_dither(c, uCor.a * m);\n"
  "}\n",

  // GFX_ANEL_GRAD — o anel do GFX_ANEL com o degrade GIRANDO em volta dele:
  // a parada clara anda pelo contorno (uma volta a cada ~7 s). E o anel de
  // foco "vivo"; parado com animacoes reduzidas.
  "vec3 grad3(float t){\n"
  "  t = clamp(t, 0.0, 1.0);\n"
  "  return t < 0.5 ? mix(uGrad0, uGrad1, t * 2.0) : mix(uGrad1, uGrad2, t * 2.0 - 1.0);\n"
  "}\n"
  "void main(){\n"
  "  float d = sdf(vUv, uRaio, uAspect);\n"
  "  float esp = max(uPar.x, 0.0015);\n"
  "  float m = borda(d) * clamp(0.5 + (d + esp) * uAlt, 0.0, 1.0);\n"
  "  if (m <= 0.002) discard;\n"
  "  vec2 p = (vUv - 0.5) * vec2(uAspect, 1.0);\n"
  "  if (uPar.y > 0.5) {\n"
  "    float tt = fract((atan(p.y, p.x) / 6.2831853 + 0.5) * uPar.y);\n"
  "    m *= smoothstep(0.56, 0.44, tt);\n"
  "    if (m <= 0.002) discard;\n"
  "  }\n"
  "  float g = 0.5 + 0.5 * cos(atan(p.y, p.x) - uTempo * 0.9);\n"
  "  gl_FragColor = vec4(grad3(g), uCor.a * m);\n"
  "}\n",

  // GFX_AMBIENTE — quatro luzes de regiao, grandes e macias, respirando.
  //
  // Os centros ficam FORA da tela (a luz entra pelas bordas, como a de uma
  // parede atras da TV) e oscilam alguns por cento em periodos diferentes, e a
  // intensidade de cada uma tambem: nunca duas batem juntas, entao a tela nao
  // "pulsa", ela respira. A cor e a media PONDERADA das luzes e o alfa a soma
  // delas, no maximo 0,72 (era 0,5 e a luz quase nao
  // aparecia) — tinge forte, mas o texto branco por cima ainda le.
  //
  // DITHER: o nv_dither do cabecalho. O daqui somava n/255 na cor E no alfa
  // sem dividir pelo alfa, e com o alfa em no maximo 0,5 o pixel final andava
  // um quarto de degrau — pouco para desfazer a faixa que a C9 mostrava.
  // highp onde existe: a soma das quatro luzes e larga e lenta.
  "\n#ifdef GL_ES\n#ifdef GL_FRAGMENT_PRECISION_HIGH\nprecision highp float;\n#endif\n#endif\n"
  "float luz(vec2 q, vec2 c, float r){\n"
  "  float k = 1.0 - smoothstep(0.0, r, length(q - c));\n"
  "  return k * k;\n"
  "}\n"
  "void main(){\n"
  "  float A = uAspect;\n"
  "  vec2 q = vUv * vec2(A, 1.0);\n"
  "  float b = uTempo * 0.35;\n"
  "  float wE = luz(q, vec2(-0.12*A + 0.03*sin(b),         0.55 + 0.06*sin(b*0.7)), 1.30) * (0.85 + 0.15*sin(b*0.9));\n"
  "  float wD = luz(q, vec2( 1.12*A + 0.03*sin(b+2.0),     0.45 + 0.06*cos(b*0.8)), 1.30) * (0.85 + 0.15*sin(b*1.1+1.7));\n"
  "  float wT = luz(q, vec2( 0.55*A + 0.08*sin(b*0.5+1.0), -0.28), 1.15) * (0.85 + 0.15*sin(b*0.7+3.1));\n"
  "  float wB = luz(q, vec2( 0.45*A + 0.08*cos(b*0.6),     1.28), 1.15) * (0.85 + 0.15*sin(b*1.3+4.4));\n"
  "  float w = wE + wD + wT + wB;\n"
  "  vec3 c = (uReg0*wE + uReg1*wD + uReg2*wT + uReg3*wB) / max(w, 0.001);\n"
  "  gl_FragColor = nv_dither(c, min(w, 1.0) * 0.72 * uCor.a);\n"
  "}\n",

  // GFX_VITRINE — ver gfx.h. O veu escurece PARA PRETO (o fundo da pagina e
  // #0D0D0D, indistinguivel dele), sem depender de uFundo. As duas rampas sao
  // as do GFX_VEU (base e esquerda), com a esquerda mais larga: no Dinamica o
  // texto ocupa 40% da largura e no banner do Padrao, 45%.
  "void main(){\n"
  "  float m = bordaR(vUv, uRaio, uAspect);\n"
  "  if (m <= 0.001) discard;\n"
  "  vec2 uv = vec2(vUv.x - uDesliza, vUv.y);\n"
  "  float dentro = step(0.0, uv.x) * step(uv.x, 1.0);\n"
  "  float ra = uAspect / max(uTexAsp, 0.01);\n"
  "  if (uTexAsp > 0.0) {\n"
  "    if (ra > 1.0) uv.y = uv.y / ra + uPar.x * (1.0 - 1.0 / ra);\n"
  "    else          uv.x = (uv.x - 0.5) * ra + 0.5;\n"
  "  }\n"
  "  vec3 c = texture2D(uTex, clamp(uv, 0.0, 1.0)).rgb;\n"
  "  float ge = smoothstep(0.62, 0.0, vUv.x) * 0.80 * uFoco;\n"
  "  float gb = smoothstep(uCor.r > 0.0 ? uCor.r : 0.38, 1.0, vUv.y) * 0.72 * uFoco;\n"
  "  c *= 1.0 - clamp(ge + gb - ge * gb, 0.0, 1.0);\n"
  "  float d = smoothstep(0.66, 1.0, vUv.y);\n"
  "  float a = uCor.a * m * dentro * (1.0 - uPar.y * d * d);\n"
  "  gl_FragColor = (uLeve > 0.5 && a > 0.5) ? vec4(c, a) : nv_dither(c, a);\n"   // see GFX_VITRINE_DIN
  "}\n",

  // GFX_FUNDO_DIN — ver gfx.h. SO COR: um degrade vertical de uma cor so. O
  // topo e uCor.rgb, a base e essa cor vezes uFoco (a "queda"), com curva suave,
  // e o dither de sempre para a rampa escura nao virar degrau na C9. Sem
  // textura, sem laco, sem SDF: uma multiplicacao por pixel.
  "void main(){\n"
  "  vec3 c = uCor.rgb * mix(1.0, uFoco, smoothstep(0.0, 1.0, vUv.y));\n"
  "  gl_FragColor = nv_dither(c, uCor.a);\n"
  "}\n",

  // GFX_COPIA — ampliacao do alvo interno (gpunivel.c). RGB E ALPHA da
  // textura, que e o que diferencia do GFX_SNAP (la o alpha vem de uCor): o
  // furo de alpha 0 por onde o plano de video aparece tem de sobreviver.
  "void main(){\n"
  "  vec2 uv = (uPar.y > 0.5) ? vec2(vUv.x, 1.0 - vUv.y) : vUv;\n"
  "  gl_FragColor = texture2D(uTex, uv);\n"
  "}\n",

  // GFX_HERO_CAM / GFX_HERO_CHEIO_CAM — AS CAMADAS DO DESTAQUE NUMA PASSADA
  // (gfx_hero_camadas). Programas PROPRIOS, e nao um ramo nos shaders do
  // destaque: a GPU reserva registradores pelo pior caminho do programa, e
  // com o ramo dentro deles a passada comum do destaque tambem ficou mais
  // lenta (MEDIDO na C9: fileira parada 60 -> 49 fps; a social, 30).
  //
  // O fragmento faz a conta que o blend fazia, na ordem: fundo (luz assada
  // em uAmb ou a cor do fundo), a camada B (uTexB/uAlfaB, a
  // arte que sai) e a camada A (uTex/uCor.a). Cada camada passa pelo mesmo
  // nv_dither e pela mesma rampa que passava sozinha, entao o pixel e o do
  // blend a menos do arredondamento do mixer de 8 bits. As rampas sao as do
  // GFX_HERO e do GFX_HERO_CHEIO, copiadas.
  "void main(){\n"
  "  float xd = vUv.x - uDesliza;\n"
  "  float dentro = step(0.0, xd) * step(xd, 1.0);\n"
  "  vec3 c = texture2D(uTex, clamp(cover(vec2(xd, vUv.y)), 0.0, 1.0)).rgb;\n"
  "  vec3 bg = uFundo;\n"
  "  float y = vUv.y;\n"
  "  float av = clamp((y-0.820)/0.072,0.0,1.0)*0.25\n"
  "           + clamp((y-0.892)/0.063,0.0,1.0)*0.40\n"
  "           + clamp((y-0.955)/0.045,0.0,1.0)*0.35;\n"
  "  float t = vUv.x/0.45;\n"
  "  float ah = 1.0 - clamp(t/0.22,0.0,1.0)*0.14\n"
  "                 - clamp((t-0.22)/0.24,0.0,1.0)*0.30\n"
  "                 - clamp((t-0.46)/0.30,0.0,1.0)*0.40\n"
  "                 - clamp((t-0.76)/0.24,0.0,1.0)*0.16;\n"
  "  ah *= step(vUv.x, 0.45);\n"
  "  float rampa = clamp(ah + av - ah*av, 0.0, 1.0);\n"
  "  vec3 dst = (uAmbOn > 0.5) ? texture2D(uAmb, vAmb).rgb : bg;\n"
  "  vec3 cB = texture2D(uTexB, clamp(coverAsp(vec2(xd, vUv.y), uTexAspB), 0.0, 1.0)).rgb;\n"
  "  vec4 dB = (uVaza > 0.5) ? nv_dither(cB, uAlfaB * dentro * (1.0 - rampa)) : nv_dither(mix(cB, bg, rampa), uAlfaB * dentro);\n"
  "  vec4 dA = (uVaza > 0.5) ? nv_dither(c, uCor.a * dentro * (1.0 - rampa)) : nv_dither(mix(c, bg, rampa), uCor.a * dentro);\n"
  "  dst = mix(dst, dB.rgb, dB.a);\n"
  "  dst = mix(dst, dA.rgb, dA.a);\n"
  "  gl_FragColor = vec4(dst, 1.0);\n"
  "}\n",

  "void main(){\n"
  "  float xd = vUv.x - uDesliza;\n"
  "  float dentro = step(0.0, xd) * step(xd, 1.0);\n"
  "  vec3 c = texture2D(uTex, clamp(cover(vec2(xd, vUv.y)), 0.0, 1.0)).rgb;\n"
  "  vec3 bg = uFundo;\n"
  "  float y = vUv.y;\n"
  "  float av = clamp((y-0.640)/0.108,0.0,1.0)*0.35\n"
  "           + clamp((y-0.748)/0.108,0.0,1.0)*0.40\n"
  "           + clamp((y-0.856)/0.144,0.0,1.0)*0.25;\n"
  "  float t = vUv.x/0.65;\n"
  "  float ah = 1.0 - clamp(t/0.22,0.0,1.0)*0.10\n"
  "                 - clamp((t-0.22)/0.24,0.0,1.0)*0.10\n"
  "                 - clamp((t-0.46)/0.30,0.0,1.0)*0.38\n"
  "                 - clamp((t-0.76)/0.24,0.0,1.0)*0.42;\n"
  "  ah *= step(vUv.x, 0.65);\n"
  "  float rampa = clamp(ah + av - ah*av, 0.0, 1.0);\n"
  "  vec3 dst = (uAmbOn > 0.5) ? texture2D(uAmb, vAmb).rgb : bg;\n"
  "  vec3 cB = texture2D(uTexB, clamp(coverAsp(vec2(xd, vUv.y), uTexAspB), 0.0, 1.0)).rgb;\n"
  "  vec4 dB = (uVaza > 0.5) ? nv_dither(cB, uAlfaB * dentro * (1.0 - rampa)) : nv_dither(mix(cB, bg, rampa), uAlfaB * dentro);\n"
  "  vec4 dA = (uVaza > 0.5) ? nv_dither(c, uCor.a * dentro * (1.0 - rampa)) : nv_dither(mix(c, bg, rampa), uCor.a * dentro);\n"
  "  dst = mix(dst, dB.rgb, dB.a);\n"
  "  dst = mix(dst, dA.rgb, dA.a);\n"
  "  gl_FragColor = vec4(dst, 1.0);\n"
  "}\n",
  // GFX_JANELA — o cartao do carrossel. Ver a nota em gfx.h.
  // uFoco = forca do veu, uPar.x = apagar da pagina rolada, uPar.y = qual veu.
  "void main(){\n"
  "  float m = bordaR(vUv, uRaio, uAspect);\n"
  "  if (m <= 0.001) discard;\n"
  "  vec2 u = uJan.xy + vUv * uJan.zw;\n"
  // No estado de CARTAO o veu e so o canto de baixo a esquerda, sob o texto
  // (uPar.y = 0); na pagina cheia, a vinheta do GFX_DETALHE (uPar.y = 1).
  // SO O VEU DO ESTADO EM CENA E CALCULADO: assentado (uPar.y 0 ou 1) o outro
  // conjunto de rampas era computado e jogado fora, ~18 ops por pixel num
  // cartao de 0,8 tela. MEDIDO na TCL Smart TV Pro (Mali-G52, GPU timer): o
  // cartao do titulo custava 15,5 ms, ~90 ops por pixel. Ramo uniforme.
  "  float a;\n"
  "  if (uPar.y < 0.001) {\n"
  "    float ac = (1.0 - smoothstep(0.18, 0.72, u.x)) * smoothstep(0.18, 0.68, u.y);\n"
  "    float base = smoothstep(0.56, 1.0, u.y) * 0.60;\n"
  "    a = 1.0 - (1.0 - ac) * (1.0 - base);\n"
  "  } else if (uPar.y > 0.999) {\n"
  "    a = 1.0 - smoothstep(0.0, 0.82, u.x);\n"
  "    float ab = smoothstep(0.70, 1.0, u.y) * 0.92;\n"
  "    a = 1.0 - (1.0 - a) * (1.0 - ab);\n"
  "  } else {\n"
  "    float ap = 1.0 - smoothstep(0.0, 0.82, u.x);\n"
  "    float ab = smoothstep(0.70, 1.0, u.y) * 0.92;\n"
  "    ap = 1.0 - (1.0 - ap) * (1.0 - ab);\n"
  "    float ac = (1.0 - smoothstep(0.18, 0.72, u.x)) * smoothstep(0.18, 0.68, u.y);\n"
  "    float base = smoothstep(0.56, 1.0, u.y) * 0.60;\n"
  "    ac = 1.0 - (1.0 - ac) * (1.0 - base);\n"
  "    a = mix(ac, ap, uPar.y);\n"
  "  }\n"
  // SO O VEU (uCor.r < 0.5): o trailer toca no cartao, atras do canvas, pelo
  // furo; o veu fica por cima com alpha, para o texto seguir no mesmo escuro.
  "  if (uCor.r < 0.5) { float va = clamp(a, 0.0, 1.0) * uFoco;\n"
  "    va = 1.0 - (1.0 - va) * (1.0 - uPar.x * (1.0 - uPar.y));\n"
  "    gl_FragColor = nv_dither(uFundo, va * uCor.a * m * (1.0 - uPar.x * uPar.y)); return; }\n"
  "  vec2 uv = u;\n"
  "  if (uTexAsp > 0.0) { float ra = 1.7777778 / uTexAsp;\n"
  "    if (ra > 1.0) uv.y = (uv.y - 0.5) / ra + 0.5; else uv.x = (uv.x - 0.5) * ra + 0.5; }\n"
  "  vec3 c = texture2D(uTex, clamp(uv, 0.0, 1.0)).rgb;\n"
  // A janela converge para a mesma composicao do GFX_DETALHE: a vinheta
  // revela a base ambiente/Frost, e a rolagem apaga a arte por alfa. No
  // cartao fechado (uPar.y=0) preserva o veu opaco anterior.
  "  float v = clamp(a, 0.0, 1.0) * uFoco;\n"
  "  float e = clamp(uPar.y, 0.0, 1.0);\n"
  "  float vaz = uVaza > 0.5 ? e : 0.0;\n"
  "  c = mix(c, uFundo, v * (1.0 - vaz));\n"
  "  c = mix(c, uFundo, uPar.x * (1.0 - e));\n"
  // Efeitos leves: sem dither onde a arte domina (v < 0.5), como no destaque.
  "  float aj = uCor.a * m * (1.0 - v * vaz) * (1.0 - uPar.x * e);\n"
  "  gl_FragColor = (uLeve > 0.5 && v < 0.5) ? vec4(c, aj) : nv_dither(c, aj);\n"
  "}\n",

  // GFX_VEU_CSS — degrade de uma borda a outra (ver gfx.h). Sem SDF: o veu e
  // sempre um retangulo de borda de tela.
  "void main(){\n"
  "  float d = uPar.x < 0.5 ? 1.0 - vUv.y : uPar.x < 1.5 ? vUv.y : uPar.x < 2.5 ? vUv.x : 1.0 - vUv.x;\n"
  "  d = clamp(d / max(uPar.y, 0.001), 0.0, 1.0);\n"
  "  float g = uFoco > 0.0 ? pow(1.0 - d, uFoco) : 1.0 - smoothstep(0.0, 1.0, d);\n"
  // uRaio (que este modo nao usa para canto) = um chapado da MESMA cor por
  // baixo do degrade, ja composto: 1-(1-base)(1-veu). 0 = so o degrade.
  "  gl_FragColor = nv_dither(uCor.rgb, uRaio + (1.0 - uRaio) * uCor.a * g);\n"
  "}\n",

  // GFX_MINI — o alvo de uma miniatura (gfx_mini_*): a textura e um FBO
  // (origem embaixo), recortada pelos cantos.
  "void main(){\n"
  "  float m = bordaR(vUv, uRaio, uAspect);\n"
  "  if (m <= 0.001) discard;\n"
  "  vec4 t = texture2D(uTex, vec2(vUv.x, 1.0 - vUv.y));\n"
  "  gl_FragColor = vec4(t.rgb / max(t.a, 0.004), t.a * uCor.a * m);\n"
  "}\n",

  // GFX_TEXTURA — a pilula/barra feita do MATERIAL do titulo (cor de destaque
  // "Textura", acentos-mockup.html quadros 8-10). uJan = recorte da textura em
  // UV (origem xy, tamanho zw) ja em "cover" para este rect; uCor.rgb = a cor
  // lisa por baixo (o transparente do logo mostra ela) e uCor.a o alfa; uFoco
  // = forca da textura (1 Textura, 0,35 sutil); uPar.x = alfa do veu atras do
  // rotulo (elipse 62% x 78% em 56%/50%, como o radial-gradient do mockup) e
  // uPar.y = 1 veu branco (tinta escura), 0 preto. Cantos pelo SDF, grao pelo
  // nv_dither.
  "void main(){\n"
  "  float m = bordaR(vUv, uRaio, uAspect);\n"
  "  if (m <= 0.001) discard;\n"
  "  vec4 t = texture2D(uTex, uJan.xy + vUv * uJan.zw);\n"
  "  vec3 c = mix(uCor.rgb, t.rgb, t.a * uFoco);\n"
  "  vec2 q = (vUv - vec2(0.56, 0.5)) / vec2(0.62, 0.78);\n"
  "  c = mix(c, vec3(uPar.y), uPar.x * clamp(1.0 - length(q), 0.0, 1.0));\n"
  "  gl_FragColor = nv_dither(c, uCor.a * m);\n"
  "}\n",

  // GFX_FOSCO — o assado lido em coordenada de tela, so dentro dos cantos.
  "void main(){\n"
  "  float m = bordaR(vUv, uRaio, uAspect);\n"
  "  if (m <= 0.001) discard;\n"
  "  gl_FragColor = nv_dither(texture2D(uTex, vAmb).rgb, uCor.a * m);\n"
  "}\n",

  // GFX_VITRINE_DIN — o corpo do GFX_VITRINE (copiado: um ramo no programa
  // dele reservaria registradores pelo pior caminho, ver GFX_HERO_CAM) e, por
  // baixo, o degrade do GFX_FUNDO_DIN na altura de TELA do fragmento (vAmb.y
  // conta de baixo). A arte e misturada no fundo aqui, e o pixel sai opaco:
  // mix(fundo, arte, alfa) e o que a mistura do GFX_VITRINE fazia sobre o
  // fundo ja pintado.
  "void main(){\n"
  "  float m = bordaR(vUv, uRaio, uAspect);\n"
  "  vec2 uv = vec2(vUv.x - uDesliza, vUv.y);\n"
  "  float dentro = step(0.0, uv.x) * step(uv.x, 1.0);\n"
  "  float ra = uAspect / max(uTexAsp, 0.01);\n"
  "  if (uTexAsp > 0.0) {\n"
  "    if (ra > 1.0) uv.y = uv.y / ra + uPar.x * (1.0 - 1.0 / ra);\n"
  "    else          uv.x = (uv.x - 0.5) * ra + 0.5;\n"
  "  }\n"
  "  vec3 c = texture2D(uTex, clamp(uv, 0.0, 1.0)).rgb;\n"
  "  float ge = smoothstep(0.62, 0.0, vUv.x) * 0.80 * uFoco;\n"
  "  float gb = smoothstep(uCor.r > 0.0 ? uCor.r : 0.38, 1.0, vUv.y) * 0.72 * uFoco;\n"
  "  c *= 1.0 - clamp(ge + gb - ge * gb, 0.0, 1.0);\n"
  "  float d = smoothstep(0.66, 1.0, vUv.y);\n"
  "  float a = clamp(uCor.a * m * dentro * (1.0 - uPar.y * d * d), 0.0, 1.0);\n"
  "  vec3 bg = uDin.rgb * mix(1.0, uDin.a, smoothstep(0.0, 1.0, 1.0 - vAmb.y));\n"
  // Light effects: no dither where the art dominates (a > 0.5). Photo content
  // has its own noise; the band the dither fights lives in the background
  // gradient and the dissolve, which keep it. The regions are large and
  // contiguous, so the branch only diverges along one line.
  "  vec3 o = mix(bg, c, a);\n"
  "  gl_FragColor = (uLeve > 0.5 && a > 0.5) ? vec4(o, 1.0) : nv_dither(o, 1.0);\n"
  "}\n",
};

// Cada corpo declara o que usa; montar so o necessario mantem o shader enxuto.
static const struct { int sdf, cover; } PRECISA[GFX_NMODOS] = {
  {1,1}, {1,0}, {1,0}, {0,1}, {1,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
  {0,1}, {0,1},
  {1,0},   /* GFX_ANEL */
  {0,0},   /* GFX_OLHO    — SDF proprio, nao o do retangulo */
  {0,0},   /* GFX_FONTES  — idem */
  {0,0},   /* GFX_MARCA   — so o alpha da textura: sem SDF, sem cover */
  {0,0},   /* GFX_VEU_BAIXO — degrade vertical puro */
  {0,0},   /* GFX_SOCIAL */
  {0,1},   /* GFX_AVATAR */
  {0,0},   /* GFX_RETRATO */
  {0,0},   /* GFX_DISCO */
  {0,0},   /* GFX_EDITORIAL */
  {1,0},   /* GFX_VEU_CARD — precisa do SDF: o veu segue os cantos do card */
  {1,0},   /* GFX_BRILHO_TOPO — idem, e pelo mesmo motivo */
  {1,0},   /* GFX_ARTE — SDF para os cantos; sem cover, a arte nao e recortada */
  {1,0},   /* GFX_LUZ — SDF para os cantos do painel */
  {0,0},   /* GFX_SINO — glifo vetorial, sem textura nem SDF de retangulo */
  {1,0}    /* GFX_ESQUELETO — SDF para os cantos do card */,
  {0,0},   /* GFX_LINHA — distancia ao segmento, sem SDF de retangulo */
  {0,0},   /* GFX_CEU — procedural, sem textura */
  {1,0},   /* GFX_COR_GRAD — SDF do GFX_COR */
  {1,0},   /* GFX_ANEL_GRAD — SDF do GFX_ANEL */
  {0,0},   /* GFX_AMBIENTE — procedural, tela cheia */
  {1,0},   /* GFX_VITRINE — SDF para os cantos; o cover e proprio (ancoragem) */
  {0,0},   /* GFX_FUNDO_DIN — procedural, so cor, tela cheia */
  {0,0},   /* GFX_COPIA — so a leitura da textura */
  {0,1},   /* GFX_HERO_CAM */
  {0,1},   /* GFX_HERO_CHEIO_CAM */
  {1,0},   /* GFX_JANELA — SDF da abertura; o cover e o do quadro da tela */
  {0,0},   /* GFX_VEU_CSS — degrade puro, sem SDF */
  {1,0},   /* GFX_MINI — SDF dos cantos; a textura e um FBO */
  {1,0},   /* GFX_TEXTURA — SDF dos cantos; o recorte vem pronto em uJan */
  {1,0},   /* GFX_FOSCO — SDF dos cantos; a textura e o assado, lido por vAmb */
  {1,0}    /* GFX_VITRINE_DIN — o do GFX_VITRINE */
};

static GLuint compila(GLenum tipo, const char *src) {
  GLuint s = glCreateShader(tipo);
  glShaderSource(s, 1, &src, NULL);
  glCompileShader(s);
  GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) { char log[700]; glGetShaderInfoLog(s, 700, NULL, log); printf("gfx shader: %s\n", log); }
  return s;
}

int gfx_iniciar(void) {
  GLuint vs = compila(GL_VERTEX_SHADER, VS);
  // Capturas (tests/*_shot): NUVIO_TAMANHO_UI=1.2 liga o "Tamanho da
  // interface" sem passar por Ajustes. No app quem define e ajustes.c.
  { const char *t = getenv("NUVIO_TAMANHO_UI"); if (t && *t) { gfx_escala_ui_definir((float)atof(t)); escUiFixa = 1; } }
  char fonte[12000];   // cabeca + sdf/cover + corpo; o maior (camadas) passa de 6000
  for (int m = 0; m < GFX_NMODOS; m++) {
    snprintf(fonte, sizeof fonte, "%s%s%s%s", FS_CABECA,
             PRECISA[m].sdf ? FS_SDF : "", PRECISA[m].cover ? FS_COVER : "",
             FS_CORPO[m]);
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, compila(GL_FRAGMENT_SHADER, fonte));
    glBindAttribLocation(p, 0, "aPos");
    glLinkProgram(p);
    GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { char log[700]; glGetProgramInfoLog(p, 700, NULL, log);
               printf("gfx link modo %d: %s\n", m, log); return 0; }
    progs[m].prog = p;
    progs[m].rect = glGetUniformLocation(p, "uRect");
    progs[m].tela = glGetUniformLocation(p, "uTela");
    progs[m].tex  = glGetUniformLocation(p, "uTex");
    progs[m].foco = glGetUniformLocation(p, "uFoco");
    progs[m].reflexo = glGetUniformLocation(p, "uReflexo");
    progs[m].par  = glGetUniformLocation(p, "uPar");
    progs[m].raio = glGetUniformLocation(p, "uRaio");
    progs[m].cor  = glGetUniformLocation(p, "uCor");
    progs[m].asp  = glGetUniformLocation(p, "uAspect");
    progs[m].texAsp = glGetUniformLocation(p, "uTexAsp");
    progs[m].forcarCover = glGetUniformLocation(p, "uForceCover");
    progs[m].veuTela = glGetUniformLocation(p, "uVeuTela");
    progs[m].borda  = glGetUniformLocation(p, "uBorda");
    progs[m].varre  = glGetUniformLocation(p, "uVarre");
    progs[m].veu    = glGetUniformLocation(p, "uVeu");
    progs[m].desl   = glGetUniformLocation(p, "uDesliza");
    progs[m].fundo  = glGetUniformLocation(p, "uFundo");
    progs[m].grad0  = glGetUniformLocation(p, "uGrad0");
    progs[m].grad1  = glGetUniformLocation(p, "uGrad1");
    progs[m].grad2  = glGetUniformLocation(p, "uGrad2");
    progs[m].tempo  = glGetUniformLocation(p, "uTempo");
    progs[m].reg0   = glGetUniformLocation(p, "uReg0");
    progs[m].reg1   = glGetUniformLocation(p, "uReg1");
    progs[m].reg2   = glGetUniformLocation(p, "uReg2");
    progs[m].reg3   = glGetUniformLocation(p, "uReg3");
    progs[m].vaza   = glGetUniformLocation(p, "uVaza");
    progs[m].amb    = glGetUniformLocation(p, "uAmb");
    progs[m].ambOn  = glGetUniformLocation(p, "uAmbOn");
    progs[m].texB   = glGetUniformLocation(p, "uTexB");
    progs[m].texAspB = glGetUniformLocation(p, "uTexAspB");
    progs[m].alfaB  = glGetUniformLocation(p, "uAlfaB");
    progs[m].alt    = glGetUniformLocation(p, "uAlt");
    progs[m].margem = glGetUniformLocation(p, "uMargem");
    progs[m].leve   = glGetUniformLocation(p, "uLeve");
    progs[m].jan    = glGetUniformLocation(p, "uJan");
    progs[m].sub    = glGetUniformLocation(p, "uSub");
    progs[m].giro   = glGetUniformLocation(p, "uGiro");
    progs[m].din    = glGetUniformLocation(p, "uDin");
    progs[m].altAtual = -1.0f;
    progs[m].telaAtual[0] = NV_TELA_W; progs[m].telaAtual[1] = NV_TELA_H;
    progs[m].margemAtual = 0.0f;   // o default de um uniform recem-linkado e 0
    progs[m].leveAtual = 0.0f;
    glUseProgram(p);
    glUniform2f(progs[m].tela, NV_TELA_W, NV_TELA_H);
    // O default de um uniform recem-linkado e 0: o pedaco tem de nascer inteiro.
    if (progs[m].sub >= 0) glUniform4f(progs[m].sub, 0.0f, 0.0f, 1.0f, 1.0f);
    progs[m].subAtual[0] = progs[m].subAtual[1] = 0.0f;
    progs[m].subAtual[2] = progs[m].subAtual[3] = 1.0f;
    if (progs[m].giro >= 0) glUniform4f(progs[m].giro, 1.0f, 0.0f, 0.0f, 0.0f);
    progs[m].giroAtual[0] = 1.0f;
    progs[m].giroAtual[1] = progs[m].giroAtual[2] = progs[m].giroAtual[3] = 0.0f;
    glUniform1i(progs[m].tex, 0);
    if (progs[m].amb >= 0) glUniform1i(progs[m].amb, 1);
    if (progs[m].texB >= 0) glUniform1i(progs[m].texB, 2);
  }
  glUseProgram(progs[GFX_CARD].prog);
  progAtual = GFX_CARD;

  // O quad vai num BUFFER, nao num ponteiro para memoria do processo.
  //
  // GLES2 e o GL de desktop aceitam array do cliente em glVertexAttribPointer,
  // e por isso isto funcionou na TV LG e no Mac durante todo o projeto. O
  // WebGL NAO aceita: o alvo Tizen roda dentro do Chromium, que recusa cada
  // desenho com "INVALID_OPERATION: drawArrays: no buffer is bound to enabled
  // attribute" — em WARNING, sem parar nada. O app subia, media quadro, escrevia
  // telemetria, e a tela ficava PRETA. Foi o primeiro defeito do port.
  //
  // O buffer e unico e fica ligado para sempre: todo desenho do app e este
  // mesmo quad de 4 vertices, deformado pelo uniform uRect no vertex shader.
  // Nenhum outro ponto do codigo liga GL_ARRAY_BUFFER, entao nao ha o que
  // restaurar por quadro.
  {
    static const GLfloat quad[] = { 0,0, 1,0, 0,1, 1,1 };
    GLuint vbo = 0;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (const void *)0);
  }
  gfxBlend(1);
  // Blend SEPARADO para cor e alpha, e o GL_ONE do alpha nao e detalhe.
  //
  // Com GL_SRC_ALPHA nos dois canais, cada desenho translucido computa
  // dst.a = a*a + dst.a*(1-a) — ou seja, ele FURA a propria superficie. Um veu
  // a 40% derruba o alpha do destino de 1.0 para 0.76. Na TV o compositor
  // mistura a janela com o que esta atras dela usando esse alpha, entao o
  // buraco aparece como uma mancha escura; numa captura por glReadPixels ele e
  // invisivel, porque a captura le a cor e nao a composicao. Isso vale para
  // TODOS os veus e fades do app, nao so para a tela de video.
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  return 1;
}

static void desfEncerrar(void);
static void iconesEncerrar(void);
void gfx_encerrar(void) {
  iconesEncerrar();
  desfEncerrar();
  for (int m = 0; m < GFX_NMODOS; m++)
    if (progs[m].prog) { glDeleteProgram(progs[m].prog); progs[m].prog = 0; }
  progAtual = -1;
}

// Ultima textura vista no bind. O driver ate ignora rebind do mesmo nome, mas
// so depois de pagar a entrada na chamada — e num quadro cheio de texto a
// MESMA textura de glifo e desenhada varias vezes seguida.
static GLuint texAtual = 0;

// Chamar quando uma textura e destruida (o nome pode ser reutilizado por
// glGenTextures) ou quando alguem deu glBindTexture por fora do gfx_rect
// (upload de arte, raster de glifo) — nos dois casos o cache mentiria.
// tex = 0 significa "esqueca tudo": e o que os uploads usam.
static void desfEsquecerFonte(GLuint tex);
void gfx_tex_esquecer(GLuint tex) {
  if (tex == 0 || texAtual == tex) texAtual = 0;
  // Textura de arte que vai ser destruida: a copia desfocada dela deixa de
  // valer, porque o nome pode voltar de glGenTextures com OUTRA imagem.
  if (tex) desfEsquecerFonte(tex);
}

int    gfx_n_rect = 0, gfx_n_prog = 0, gfx_n_bind = 0, gfx_n_outros = 0;
double gfx_ms_rect = 0.0, gfx_ms_outros = 0.0;
// PREENCHIMENTO SUBMETIDO no quadro, em telas cheias (1920x1080 = 1,0).
// Nesta Mali o custo e de fragmento, nao de chamada: a nota no topo deste
// arquivo diz que DUAS camadas de tela cheia derrubavam o quadro para ~40fps.
// Sem contar a area, "quantas camadas cheias tem esta tela" e chute — com o
// contador e uma medida por quadro.
double gfx_fill = 0.0;
double gfx_fill_vis = 0.0;
double gfx_fill_gpu = 0.0, gfx_fill_gpu_mist = 0.0;
double gfx_fill_gpu_ult = 0.0, gfx_fill_gpu_mist_ult = 0.0;
unsigned long long gfx_modos_desligados = 0;
int    gfx_n_cheio = 0;   // desenhos que cobrem >= 50% da tela
// So na medida (tests/fluidez_perf.c, -DNV_FLUIDEZ_PERF): quantos desses foram
// desenhados COM mistura, que e a leitura da tela que a Mali paga a mais.
int    gfx_n_cheio_mistura = 0;
double gfx_fill_modo[GFX_NMODOS];
int gfx_rastro_grandes;
double gfx_fill_modo_ult[GFX_NMODOS];   // o do quadro anterior (o log le este)
static int efeitosLeves = 0;
// Dentro do ASSADO de um fundo (gfx_luz_canal) as GFX_LUZ valem mesmo com
// efeitos leves: o assado e pintado uma vez por cor, nao por quadro. Sem isto
// o Frost de toda TV no nivel 1 (TCL, Samsung Q80A: "[cor] fundo frost lido:
// esperado 61,60,59 ... tela 20,21,25") saia so com a base, quase preto.
static int assandoCanal = 0;
void gfx_definir_efeitos_leves(int leves) { efeitosLeves = leves ? 1 : 0; }
int  gfx_efeitos_leves(void) { return efeitosLeves; }
int gfx_veu_card_por(float fracao, float alfa) {
  int i;
  if (fracao <= 0.0f || alfa <= 0.001f) return 0;
  if (fracao > 1.0f) fracao = 1.0f;
  for (i = 0; i < 4; i += 2)
    if (gfx_veu_card_atual[i + 1] <= 0.0f) {
      gfx_veu_card_atual[i] = fracao; gfx_veu_card_atual[i + 1] = alfa; return 1; }
  return 0;
}
void gfx_veu_card_limpar(void) {
  memset(gfx_veu_card_atual, 0, sizeof gfx_veu_card_atual);
  gfx_veu_na_arte = 0;
}
void gfx_veu_base(GfxRect card, float raio, float fracao, float alfa) {
  GfxRect v;
  if (fracao <= 0.0f || alfa <= 0.001f) return;
  if (fracao > 1.0f) fracao = 1.0f;
  // Ja foi junto da arte (gfx_veu_na_arte): so o veu que ESTA na lista sai;
  // um pedido que nao foi previsto continua desenhado como antes.
  if (gfx_veu_na_arte) {
    int i;
    for (i = 0; i < 4; i += 2)
      if (gfx_veu_card_atual[i + 1] > 0.0f &&
          fabsf(gfx_veu_card_atual[i] - fracao) < 1e-4f &&
          fabsf(gfx_veu_card_atual[i + 1] - alfa) < 1e-4f) return;
  }
  v = (GfxRect){ card.x, card.y + card.h * (1.0f - fracao), card.w, card.h * fracao };
  // GFX_BRILHO_TOPO com uPar.y > 0: cheio da BASE ate 42% e zero no topo do
  // retangulo. O raio vira fracao da altura DESTE retangulo (o shader mede
  // pela altura), senao o canto do veu fecha mais que o do card.
  gfx_rect(v, 0, GFX_BRILHO_TOPO, 0, 1.0f, 0.42f, raio / fracao,
           0.02f, 0.02f, 0.03f, alfa);
}
// REALCE DO TOPO SO NA FAIXA QUE A RAMPA COBRE. O GFX_BRILHO_TOPO era pedido
// com o retangulo do cartao INTEIRO e a rampa cortava dentro (uPar.x): abaixo
// dela o fragmento saia com alfa 0 — e ainda assim era executado e misturado,
// em ~90% da area do cartao. MEDIDO na C9 (30/09/2026, Moderna, imersiva):
// 0,76 tela de GFX_BRILHO_TOPO por quadro, e sem ele a home ia de 44 a 60 fps.
//
// Aqui o retangulo e a faixa da rampa MAIS o raio do canto: os cantos de baixo
// do sub-retangulo caem onde a rampa ja e zero, entao o SDF arredondado ali
// nao apaga nada, e os cantos de cima sao os do cartao (mesmo raio em pixels,
// mesma borda). O pixel e o mesmo; a area submetida cai para `alcance` + raio.
void gfx_brilho_topo(GfxRect r, float raio, float alcance,
                     float cr, float cg, float cb, float ca) {
  float rpx, h2;
  GfxRect f;
  if (r.w <= 0.0f || r.h <= 0.0f || ca <= 0.001f || alcance <= 0.0f) return;
  if (alcance > 1.0f) alcance = 1.0f;
  rpx = raio * r.h;   // o shader mede o raio pela ALTURA (ver sdf em FS_SDF)
  if (rpx < 0.0f) rpx = 0.0f;
  h2 = r.h * alcance + rpx + 1.0f;
  if (h2 >= r.h) { gfx_rect(r, 0, GFX_BRILHO_TOPO, 0, alcance, 0, raio, cr, cg, cb, ca); return; }
  f = (GfxRect){ r.x, r.y, r.w, h2 };
  // O mesmo raio em pixels, agora em fracao da altura DESTE retangulo.
  gfx_rect(f, 0, GFX_BRILHO_TOPO, 0, r.h * alcance / h2, 0, rpx / h2, cr, cg, cb, ca);
}
// A ARTE DE TELA CHEIA DOS AJUSTES COM O VEU NA MESMA PASSADA. Devolve 0 (e nao
// desenha nada) quando nao da para garantir o mesmo pixel das duas passadas —
// arte e veu separados, o caminho de sempre: textura nao opaca, alfa < 1,
// opacidade de grupo, canto, deslize. `base` = alfa do veu no meio; `aEsq` e
// `aDir` = o que o degrade soma nas bordas (como em gfx_veu_css_base).
int gfx_arte_veu(GfxRect r, GLuint tex, float base, float aEsq, float aDir, float a) {
  if (!tex || !gfx_arte_opaca_atual || gfx_opacidade_grupo < 0.999f || a < 0.999f ||
      gfx_desliza_atual != 0.0f || snapAtivo || base <= 0.002f || base >= 1.0f) return 0;
  if (!(r.x <= 0.0f && r.y <= 0.0f && r.x + r.w >= NV_TELA_W && r.y + r.h >= NV_TELA_H)) return 0;
  if (progs[GFX_ARTE].foco < 0 || progs[GFX_ARTE].par < 0) return 0;
  gfx_rect(r, tex, GFX_ARTE, base, aEsq, aDir, 0.0f, 1, 1, 1, 1.0f);
  return 1;
}
void gfx_veu_css(GfxRect r, int borda, float curva, float fim, float a) {
  if (r.w <= 0.0f || r.h <= 0.0f || a <= 0.002f) return;
  gfx_rect(r, 0, GFX_VEU_CSS, curva, (float)borda, fim > 0.0f ? fim : 1.0f, 0.0f, 0, 0, 0, a);
}
// O veu com um chapado preto de alfa `base` por baixo, na mesma passada: o que
// era gfx_cor(r, 0, 0,0,0, base) + gfx_veu_css(...) vira uma camada so. A
// opacidade de grupo entra no chapado aqui (no degrade ela entra pelo uCor).
void gfx_veu_css_base(GfxRect r, int borda, float curva, float fim, float a, float base) {
  float b = base * gfx_opacidade_grupo;
  if (r.w <= 0.0f || r.h <= 0.0f || (a <= 0.002f && b <= 0.002f)) return;
  if (b > 1.0f) b = 1.0f;
  if (b < 0.0f) b = 0.0f;
  gfx_rect(r, 0, GFX_VEU_CSS, curva, (float)borda, fim > 0.0f ? fim : 1.0f, b, 0, 0, 0, a);
}
static int efeitosMinimos = 0;
void gfx_definir_efeitos_minimos(int m) { efeitosMinimos = m ? 1 : 0; }
int  gfx_efeitos_minimos(void) { return efeitosMinimos; }
static double gfxFreqMs = 0.0;
static int desfGeradosQuadro = 0;   // ver gfx_desfocado
// Estado da luz ambiente (ver a nota "A LUZ AMBIENTE PENDENTE", em gfx_rect).
static GLuint ambTex;          // a que se LE neste quadro (ambTexPar[ambLado])
#define AMB_N 4
static GLuint ambTexPar[AMB_N], ambFboPar[AMB_N];
static int ambLado;
int gfx_n_assados;
static float ambChave[20];
static int ambPendente, ambIntacta;
// A FONTE INTACTA: a textura que E a tela agora (ambIntacta = 1), lida pelo
// uAmb dos modos de arte. A luz imersiva (ambTex) ou um fundo de tela cheia
// adiado por gfx_luz_canal_adiar (o Frost assado). 0 = nenhuma.
static GLuint ambFonte;
// O que gfx_ambiente_descarregar pinta: a textura e se passa pelo nv_dither.
static GLuint ambPendTex;
static int ambPendPont;
// O FUNDO DA DINAMICA ADIADO (gfx_fundo_din_desenhar): cor do topo e queda.
static int dinPendente;
// FUNDO DA DINAMICA ADIADO PARA O FIM DO QUADRO, POR BAIXO DO QUE JA FOI
// DESENHADO (Android). A tela e limpa transparente (alfa 0); todo desenho
// mistura com SRC_ALPHA/ONE_MINUS_SRC_ALPHA e o alfa do destino vira a
// COBERTURA do que ja esta pintado (premultiplicado, porque o fundo era preto
// com alfa 0). No fim, o degrade entra com (ONE_MINUS_DST_ALPHA, ONE): o mesmo
// pixel da ordem de pintor, so que agora ele e pintado DEPOIS dos cartoes — e
// cada cartaz opaco deixou uma marca no buffer de profundidade
// (gfx_mascara_opaca), entao o fragmento do fundo embaixo dele e descartado
// antes de ser sombreado. MEDIDO na TCL Smart TV Pro (Mali-G52, GPU timer):
// o degrade de tela cheia custava 6,4 ms por quadro e ~70% dele ficava
// escondido sob as fileiras. Qualquer leitura da tela ou furo de video chama
// dinDescarregar antes, que resolve o fundo naquele ponto (correto em
// qualquer momento). Fora do Android (sem como medir) fica o caminho antigo.
static int dinAdiado, dinAdiadoOk = -1;
// Os retangulos OPACOS pintados neste quadro (gfx_mascara_opaca): o fundo
// adiado so e pintado no COMPLEMENTO deles. Nada de buffer de profundidade:
// MEDIDO na TCL (06/10), marcar os cartazes na profundidade custava mais
// fragmentos do que poupava — nesta GPU o custo e por fragmento invocado,
// nao por byte escrito — e o recorte em CPU nao invoca nenhum.
#define DIN_NOP 160
static GfxRect dinOp[DIN_NOP];
static int dinNOp;
static void dinResolverAdiado(void);
static float dinPend[4];
static void dinDescarregar(void);
static void dinPintar(float y0);
static int foscoOk;   // este quadro assou fundo (gfx_ambiente_preparar): o vidro fosco tem fonte
static int foscoBloq; // este quadro tem video vivo por baixo (gfx_vidro_fosco_bloquear)
static GLuint foscoFonte; // fonte do fosco posta pelo fundo deste quadro (gfx_vidro_fosco_fonte)
void gfx_novo_quadro(void) {
  gfx_n_rect = gfx_n_prog = gfx_n_bind = gfx_n_outros = 0;
  gfx_ms_rect = gfx_ms_outros = 0.0;
  gfx_fill = 0.0; gfx_fill_vis = 0.0; gfx_n_cheio = 0; gfx_n_cheio_mistura = 0;
  gfx_fill_gpu_ult = gfx_fill_gpu; gfx_fill_gpu_mist_ult = gfx_fill_gpu_mist;
  gfx_fill_gpu = 0.0; gfx_fill_gpu_mist = 0.0;
  memcpy(gfx_fill_modo_ult, gfx_fill_modo, sizeof gfx_fill_modo);
  memset(gfx_fill_modo, 0, sizeof gfx_fill_modo);
  desfGeradosQuadro = 0;
  ambPendente = 0; ambIntacta = 0; foscoOk = 0; foscoBloq = 0; foscoFonte = 0;
  ambFonte = 0; dinPendente = 0; dinAdiado = 0; dinNOp = 0;
  gfx_n_assados = 0;
}
// Relogio dos pontos de GL que NAO sao gfx_rect: recorte, FBO do snapshot e as
// tres passadas do desfoque. Numa GPU de ladrilhos trocar de alvo de render no
// meio do quadro forca descarga do ladrilho — e o suspeito natural para o custo
// de CPU que sobra dentro de app_desenhar depois de descontar gfx_rect e texto.
#define GFX_OUTRO_INI() \
  if (gfxFreqMs == 0.0) gfxFreqMs = 1000.0 / (double)SDL_GetPerformanceFrequency(); \
  Uint64 tO_ = SDL_GetPerformanceCounter()
#define GFX_OUTRO_FIM() do { \
  gfx_ms_outros += (double)(SDL_GetPerformanceCounter() - tO_) * gfxFreqMs; \
  gfx_n_outros++; } while (0)

// A LUZ AMBIENTE PENDENTE (ver a nota de uAmb no cabecalho dos shaders).
//
// main.c pede a luz logo depois do clear, com alfa 1. Em vez de pintar o quad
// de tela cheia na hora, gfx_ambiente ANOTA o pedido; o primeiro gfx_rect do
// quadro decide: se ele proprio e um desenho OPACO DE TELA CHEIA (o destaque
// cheio ou o fundo do detalhe lendo a luz pelo uAmb, ou o fundo da Dinamica),
// a luz por baixo nao apareceria em pixel nenhum e o quad e dispensado; senao
// a luz e pintada ANTES dele, como sempre foi. Quem termina o quadro chama
// gfx_ambiente_descarregar, para um quadro sem desenho nenhum nao ficar sem
// ela. O resultado na tela e o mesmo; o que muda e uma camada cheia a menos.
static float ambPendAlfa;
// ambIntacta = 1: a tela ainda e so o clear + a luz (nada foi pintado por
// cima), condicao para o shader misturar com a luz em vez de com a tela.
// Qualquer desenho derruba; gfx_ambiente levanta.
static void ambPintar(float alfa);
void gfx_ambiente_descarregar(void) {
  int rec;
  float g;
  dinDescarregar();
  if (!ambPendente) return;
  ambPendente = 0;
  // O pedido foi feito sem tesoura (tela cheia): a que estiver ligada agora e
  // do desenho que o disparou, nao do fundo.
  // O mesmo vale para a opacidade de grupo: o pedido foi feito com ela em 1.
  rec = recorteAtivo;
  g = gfx_opacidade_grupo;
  if (rec) { glDisable(GL_SCISSOR_TEST); recorteAtivo = 0; }
  gfx_opacidade_grupo = 1.0f;
  ambPintar(ambPendAlfa);
  gfx_opacidade_grupo = g;
  if (rec) { glEnable(GL_SCISSOR_TEST); recorteAtivo = 1; }
}
// O pendente pintado JA com um veu de cor (cr, cg, cb) a `va` por cima, numa
// passada opaca: o GFX_SNAP faz mix(textura, cor, uFoco), que e o pixel do
// veu misturado sobre ele. Ver o veu de tela cheia em gfx_rect.
static void ambPintarVeu(float cr, float cg, float cb, float va);


void gfx_rect(GfxRect r, GLuint tex, GfxModo modo, float foco,
              float parx, float pary, float raio,
              float cr, float cg, float cb, float ca) {
  int comAmb = 0, veuAmb = 0, opaco = 0, cheia, clearCor, duplo = 0;
  float din[4] = { 0.0f, 0.0f, 0.0f, 0.0f }, dinResto = -1.0f;
  if ((int)modo < 0 || (int)modo >= GFX_NMODOS) return;
  // Grupo a 0 = "desenhar sem aparecer" (quem mede uma previa ou pede as
  // texturas de uma cena que ainda nao entrou): todo modo multiplica o alfa
  // pelo grupo, entao nada chegaria a tela — nao gasta a GPU com isso.
  if (gfx_opacidade_grupo <= 0.0f) return;
  // BRILHO DA INTERFACE DO PLAYER: multiplica a cor, nao o alfa (esmaecer.h).
  if (gfx_osd_mult < 0.999f &&
      (modo == GFX_COR || modo == GFX_TEXTO || modo == GFX_ANEL || modo == GFX_MARCA)) {
    cr *= gfx_osd_mult; cg *= gfx_osd_mult; cb *= gfx_osd_mult;
  }
  GFX_TR_RECT(r.x, r.y, r.w, r.h);
  if (escAtiva != 1.0f) { r.x *= escAtiva; r.y *= escAtiva; r.w *= escAtiva; r.h *= escAtiva; }
  // A COR DO DESTAQUE E A ASSINATURA. Com o degrade ligado, todo retangulo ou
  // anel pintado EXATAMENTE com o destaque vivo (os tres floats que
  // ajustes_acento devolveu, sem conta no meio) e superficie de destaque, e
  // vira degrade aqui. Quem escurece ou mistura o destaque (ar*0.8) nao casa e
  // segue chapado, que e o certo: aquilo ja e outra cor. Tres comparacoes por
  // retangulo, e so com o degrade ligado.
  if (nv_grad_ativo && (modo == GFX_COR || modo == GFX_ANEL) &&
      cr == nv_acento_viva[0] && cg == nv_acento_viva[1] && cb == nv_acento_viva[2])
    modo = modo == GFX_COR ? GFX_COR_GRAD : GFX_ANEL_GRAD;
  // TEXTURA, pela mesma assinatura: todo GFX_COR CHEIO (alfa >= 0,9) na cor
  // exata do destaque e superficie de foco/estado — pilula em foco, barra de
  // progresso, disco do aviso — e vira o recorte do titulo. Chip ligado (22%)
  // e lavagem de foco nao casam: ficam na cor lisa, que e o Da arte.
  // O recorte entra em "cover" no rect: a janela do titulo (3,2:1) inteira
  // numa pilula, uma faixa dela numa barra fina.
  else if (txTex && modo == GFX_COR && ca * gfx_opacidade_grupo >= 0.9f && r.w > 0.0f && r.h > 0.0f &&
           cr == nv_acento_viva[0] && cg == nv_acento_viva[1] && cb == nv_acento_viva[2]) {
    float A = r.w / r.h, vw = txJan[2], vh;
    if (vw * txAsp / txJan[3] > A) vw = txJan[3] * A / txAsp;   // recorte mais largo que o rect
    vh = vw * txAsp / A;
    txUv[0] = txJan[0] + (txJan[2] - vw) * 0.5f; txUv[1] = txJan[1] + (txJan[3] - vh) * 0.5f;
    txUv[2] = vw; txUv[3] = vh;
    modo = GFX_TEXTURA; tex = txTex; foco = txForca;
    parx = r.h >= 30.0f ? txVeu : 0.0f;   // o veu e atras do ROTULO: barra e disco nao tem
    pary = txVeuBranco;
  }
  // Efeitos leves: os dois realces que so enfeitam (brilho no alto do card,
  // luz de canto de painel) nao sao desenhados. Nenhum carrega informacao — o
  // foco continua marcado pelo anel e pelo especular do GFX_CARD.
  //
  // SO O REALCE CLARO SAI. O GFX_BRILHO_TOPO tambem desenha VEU ESCURO
  // (gfx_veu_base, os veus da previa em novidades160.c): a mesma rampa em
  // preto, atras de texto. Esse carrega informacao — sem ele o "Temporada 1 ·
  // Episodio 2" do Continuar assistindo some na arte clara. MEDIDO na TCL
  // Smart TV Pro (Android 14, Mali-G52, D1 14886: "[gpu-nivel] nivel 1 -> 1
  // (salvo)"): no nivel 1 o card do CW saia sem veu. Os realces sao brancos
  // (soma >= 2,6); os veus, quase pretos.
  if (efeitosLeves && ((modo == GFX_LUZ && !assandoCanal) ||
      (modo == GFX_BRILHO_TOPO && cr + cg + cb > 1.5f))) return;
  // Efeitos minimos: sombra e halo tambem saem (o anel continua marcando o foco).
  if (efeitosMinimos && modo == GFX_SOMBRA) return;
  if (gfx_modos_desligados && ((gfx_modos_desligados >> (unsigned)modo) & 1ull)) return;
  // O FUNDO DA DINAMICA ADIADO (gfx_fundo_din_desenhar) E O DESTAQUE POR CIMA
  // DELE NUMA PASSADA SO. O destaque (GFX_VITRINE) de largura inteira, a partir
  // do topo, alfa 1 e sem canto cobre o fundo com alfa 1 em ~2/3 da altura e o
  // dissolve na base: o fundo inteiro era uma tela opaca pintada e quase toda
  // escondida, e o destaque, uma tela inteira misturada por cima. Aqui o
  // GFX_VITRINE_DIN calcula o degrade do fundo no proprio fragmento e sai
  // OPACO; o que sobra abaixo do retangulo (destaque rolado) leva o fundo no
  // fim deste desenho. Qualquer outro primeiro desenho pinta o fundo antes,
  // como sempre.
  if (dinPendente) {
    if (modo == GFX_VITRINE && tex && !giroOn && !snapAtivo && !miniAtiva && !recorteAtivo &&
        raio <= 0.0f && ca * gfx_opacidade_grupo >= 0.999f && gfx_desliza_atual == 0.0f &&
        progs[GFX_VITRINE_DIN].din >= 0 && !((gfx_modos_desligados >> (unsigned)GFX_FUNDO_DIN) & 1ull)) {
      float rx = r.x, ry = r.y, rw = r.w, rh = r.h;   // r ja em coordenadas de tela
      if (rx <= 0.0f && rx + rw >= NV_TELA_W && ry <= 0.0f && rh > 0.0f) {
        memcpy(din, dinPend, sizeof din);
        dinPendente = 0; dinAdiado = 0;
        modo = GFX_VITRINE_DIN;
        if (ry + rh < NV_TELA_H) dinResto = (ry + rh) / NV_TELA_H;
      }
    }
    if (modo != GFX_VITRINE_DIN && !dinAdiado) dinDescarregar();   // adiado: fica para o fim
  }
  // VEU DE TELA CHEIA SOBRE O FUNDO PENDENTE (a luz imersiva, ou o Frost
  // adiado): o "dt-cobre" de 55% da pagina do titulo rolada era uma tela
  // inteira misturada sobre a passada opaca do fundo. Aqui o fundo sai com o
  // veu na MESMA passada opaca (GFX_SNAP: mix(fundo, cor, alfa)), que e o
  // pixel da mistura, e o veu nao e desenhado.
  if (ambPendente && modo == GFX_COR && raio <= 0.0f && !giroOn && !recorteAtivo) {
    float va = ca * gfx_opacidade_grupo, rx = r.x, ry = r.y, rw = r.w, rh = r.h;
    if (va > 0.001f && va < 0.999f && rx <= 0.0f && ry <= 0.0f && rx + rw >= NV_TELA_W && ry + rh >= NV_TELA_H) {
      ambPendente = 0;
      ambPintarVeu(cr, cg, cb, va);
      return;
    }
  }
  // ARTE COM RAMPA SOBRE A LUZ AMBIENTE INTACTA, opaca: o shader le a luz
  // (uAmb) e faz a mistura. So com alfa 1, sem deslize (o `dentro` do shader
  // deixaria o lado de fora transparente), sem o modo "so a rampa" (uPar.x) e
  // fora de snapshot. As condicoes sao as mesmas em que o blend daria alfa
  // final 1 em todo pixel do retangulo.
  if ((modo == GFX_HERO || modo == GFX_HERO_CHEIO || modo == GFX_DETALHE) &&
      ambIntacta && ambFonte &&
      nv_ambiente_forca > 0.001f && !efeitosMinimos && !snapAtivo &&
      parx <= 0.5f && ca * gfx_opacidade_grupo >= 0.999f && gfx_desliza_atual == 0.0f &&
      progs[modo].ambOn >= 0)
    comAmb = 1;
  // SO A RAMPA (uPar.x) SOBRE O TRAILER DO HERO (#290): o veu tem de fundir
  // no MESMO fundo em que a arte parada funde. Com a luz ambiente esse fundo e
  // a luz (o caminho comAmb acima mistura nela), entao o veu le a luz como
  // cor; a mistura continua ligada (o veu e alfa sobre o furo do video).
  else if ((modo == GFX_HERO || modo == GFX_HERO_CHEIO) && parx > 0.5f && ambFonte &&
           nv_ambiente_forca > 0.001f && !efeitosMinimos && !snapAtivo &&
           progs[modo].ambOn >= 0)
    veuAmb = 1;
  // SEM luz ambiente, a mesma arte com alfa 1 ja saia com alfa 1 em todo
  // pixel (c misturada no fundo pelo proprio shader): a mistura era um no-op
  // que ainda lia a tela. Desliga-la nao muda um pixel.
  else if ((modo == GFX_HERO || modo == GFX_HERO_CHEIO || modo == GFX_DETALHE) &&
           nv_ambiente_forca <= 0.001f && parx <= 0.5f &&
           ca * gfx_opacidade_grupo >= 0.999f && gfx_desliza_atual == 0.0f)
    opaco = 1;
  // ARTE OPACA DE TELA CHEIA, canto vivo, alfa 1: o fragmento sai com alfa 1 em
  // todo pixel do retangulo (que cobre a tela), entao a mistura era um no-op que
  // ainda lia a tela. Mesma condicao de arteCobre abaixo.
  else if (modo == GFX_ARTE && gfx_arte_opaca_atual && raio <= 0.0f &&
           r.x <= 0.0f && r.y <= 0.0f && r.x + r.w >= NV_TELA_W && r.y + r.h >= NV_TELA_H &&
           ca * gfx_opacidade_grupo >= 0.999f && gfx_desliza_atual == 0.0f && !snapAtivo)
    opaco = 1;
  // Modos cujo alfa de saida e o proprio uCor.a (ou 1): com alfa 1 a mistura
  // tambem era um no-op com leitura da tela. Fundo social, ceu da Explorar,
  // snapshot/luz assada e a arte desfocada do fundo.
  else if ((modo == GFX_SOCIAL || modo == GFX_CEU || modo == GFX_SNAP || modo == GFX_FUNDO ||
            (modo == GFX_FOSCO && raio <= 0.0f) ||
            (modo == GFX_JANELA && raio <= 0.0f && cr > 0.5f &&
             nv_ambiente_forca <= 0.001f && parx <= 0.0f)) &&
           ca * gfx_opacidade_grupo >= 0.999f)
    opaco = 1;
  // CAMADAS DO DESTAQUE (gfx_hero_camadas): a passada e opaca e o fragmento
  // compoe fundo + B + A. A luz entra como uAmb quando e ela o fundo (as
  // condicoes ja foram conferidas em gfx_hero_camadas).
  if (modo == GFX_HERO_CAM || modo == GFX_HERO_CHEIO_CAM) {
    duplo = 1; opaco = 1;
    comAmb = nv_ambiente_forca > 0.001f;
  }
  if (modo == GFX_VITRINE_DIN) opaco = 1;
  cheia = !giroOn && r.x <= 0.0f && r.y <= 0.0f && r.x + r.w >= NV_TELA_W && r.y + r.h >= NV_TELA_H;
  // COR CHAPADA DE TELA CHEIA, canto vivo e alfa 1 (o fundo opaco que varias
  // telas pintam por cima do clear): e um glClear com essa cor — o mesmo
  // pixel (a mistura de alfa 1 devolve a cor e alfa 1), sem passar dois
  // milhoes de fragmentos pelo pipeline. Respeita a tesoura, como o quad.
  clearCor = modo == GFX_COR && cheia && raio <= 0.0f && ca * gfx_opacidade_grupo >= 0.999f;
  if (ambPendente) {
    // Tela cheia e opaco: a luz por baixo nao apareceria. Senao, ela primeiro.
    // A ARTE OPACA DE TELA CHEIA (o fundo dos Ajustes e da pagina do titulo,
    // fundo.c) tambem esconde a luz: todo texel tem alfa 255 (tex_opaca) e o
    // GFX_ARTE sem canto le a textura inteira no retangulo. A mistura continua
    // ligada — so a franja de 1 px da borda da TELA, se tiver cobertura parcial,
    // passa a misturar com o clear em vez da luz. Uma tela cheia a menos por
    // quadro nos Ajustes com a Imersiva (tests/fluidez_perf.sh, cenario dono).
    int arteCobre = modo == GFX_ARTE && gfx_arte_opaca_atual && raio <= 0.0f &&
                    ca * gfx_opacidade_grupo >= 0.999f && gfx_desliza_atual == 0.0f;
    if (cheia && (comAmb || clearCor || opaco || arteCobre || modo == GFX_FUNDO_DIN)) ambPendente = 0;
    else gfx_ambiente_descarregar();
  }
  if (gfxFreqMs == 0.0) gfxFreqMs = 1000.0 / (double)SDL_GetPerformanceFrequency();
  (void)gfxFreqMs;
#ifdef NV_PERF_FINO
  Uint64 t0 = SDL_GetPerformanceCounter();
#endif
  gfx_n_rect++;
  { float area = (r.w * r.h) / (NV_TELA_W * NV_TELA_H) *
                 (subAtual[2] - subAtual[0]) * (subAtual[3] - subAtual[1]);
    if (gfx_rastro_grandes && (area >= 0.12f || (modo == GFX_COR && area >= 0.01f)))
      printf("[qd-rect] modo=%d %.0fx%.0f@%.0f,%.0f a=%.2f raio=%.0f tex=%u\n", (int)modo, r.w, r.h, r.x, r.y,
             ca * gfx_opacidade_grupo, raio, (unsigned)tex);
    gfx_fill += area;
    gfx_fill_modo[modo] += area;
    { float sx0 = r.x + r.w * subAtual[0], sy0 = r.y + r.h * subAtual[1];
      float sx1 = r.x + r.w * subAtual[2], sy1 = r.y + r.h * subAtual[3];
      float x0 = sx0 < 0.0f ? 0.0f : sx0, y0 = sy0 < 0.0f ? 0.0f : sy0;
      float x1 = sx1 > NV_TELA_W ? NV_TELA_W : sx1;
      float y1 = sy1 > NV_TELA_H ? NV_TELA_H : sy1;
      if (x1 > x0 && y1 > y0) gfx_fill_vis += (double)((x1 - x0) * (y1 - y0)) / (NV_TELA_W * NV_TELA_H); }
    if (area >= 0.5f) { gfx_n_cheio++;
#ifdef NV_FLUIDEZ_PERF
      if (!comAmb && !opaco && !clearCor && glIsEnabled(GL_BLEND)) gfx_n_cheio_mistura++;
#endif
    } }
  if (clearCor) {
    GFX_OUTRO_INI();
    glClearColor(cr, cg, cb, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    GFX_OUTRO_FIM();
    ambIntacta = 0;
    return;
  }
  if (!miniAtiva) {
    double ag = fillGpuArea(r, subAtual);
    gfx_fill_gpu += ag;
    if (blendLigado && !comAmb && !opaco) gfx_fill_gpu_mist += ag;
  }
  Programa *P = &progs[modo];
  if (progAtual != (int)modo) { glUseProgram(P->prog); progAtual = (int)modo; gfx_n_prog++; }
  // Uniform que o shader do modo nao declara volta como -1 do link; passar -1
  // ao glUniform e no-op valido mas ainda paga a travessia da chamada GL. Num
  // quadro tipico da home sao centenas de gfx_rect, a maioria em modos que nao
  // usam foco/parallax/texAsp, entao o teste barato aqui poupa a chamada cara.
  glUniform4f(P->rect, r.x, r.y, r.w, r.h);
  if (P->foco >= 0)   glUniform1f(P->foco, foco);
  // #410: o especular da arte tambem obedece a Reflexo/Profundidade.
  // uFoco continua inteiro: contraste e rebordo nao sao intensidade de reflexo.
  if (P->reflexo >= 0)
    glUniform1f(P->reflexo, ajustes_profundidade() ? ajustes_profundidade_brilho() : 0.0f);
  if (P->par >= 0)    glUniform2f(P->par, parx, pary);
  if (P->raio >= 0)   glUniform1f(P->raio, raio);
  if (P->asp >= 0)    glUniform1f(P->asp, r.h > 0 ? r.w / r.h : 1.0f);
  if (P->texAsp >= 0) glUniform1f(P->texAsp, gfx_tex_aspect_atual);
  if (P->jan >= 0) {
    const float *j = modo == GFX_TEXTURA ? txUv : gfx_janela_atual;
    glUniform4f(P->jan, j[0], j[1], j[2], j[3]);
  }
  if (P->forcarCover >= 0) glUniform1f(P->forcarCover, gfx_card_forcar_cover_atual);
  if (P->veuTela >= 0)     glUniform1f(P->veuTela, gfx_card_veu_tela_atual);
  if (P->borda >= 0)  glUniform1f(P->borda, gfx_borda_foco_atual);
  if (P->varre >= 0)  glUniform1f(P->varre, gfx_varre_atual);
  // Depois da arte (gfx_veu_na_arte) a lista so serve para gfx_veu_base pular
  // os mesmos veus: nenhum outro GFX_CARD do card (miniatura) ganha veu.
  if (P->veu >= 0) {
    static const float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    glUniform4fv(P->veu, 1, gfx_veu_na_arte ? zero : gfx_veu_card_atual);
  }
  if (P->desl >= 0)   glUniform1f(P->desl, gfx_desliza_atual);
  // So os tres modos de rampa declaram uFundo: e uma chamada por destaque ou
  // fundo de detalhe desenhado, nao por retangulo.
  if (P->fundo >= 0)  glUniform3f(P->fundo, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B);
  // Cor viva: so os modos que declaram (degrade, luz, rampas) pagam a chamada.
  if (P->grad0 >= 0) {
    glUniform3fv(P->grad0, 1, nv_grad_viva[0]);
    glUniform3fv(P->grad1, 1, nv_grad_viva[1]);
    glUniform3fv(P->grad2, 1, nv_grad_viva[2]);
  }
  if (P->tempo >= 0)  glUniform1f(P->tempo, nv_tempo_viva);
  if (P->reg0 >= 0) {
    glUniform3fv(P->reg0, 1, nv_ambiente_viva[0]);
    glUniform3fv(P->reg1, 1, nv_ambiente_viva[1]);
    glUniform3fv(P->reg2, 1, nv_ambiente_viva[2]);
    glUniform3fv(P->reg3, 1, nv_ambiente_viva[3]);
  }
  if (P->vaza >= 0)   glUniform1f(P->vaza, nv_ambiente_forca > 0.001f ? 1.0f : 0.0f);
  if (P->din >= 0)    glUniform4f(P->din, din[0], din[1], din[2], din[3]);
  if (duplo) {
    glUniform1f(P->texAspB, camAspB);
    glUniform1f(P->alfaB, camAlfaB * gfx_opacidade_grupo);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, camTexB);
    glActiveTexture(GL_TEXTURE0);
  }
  if (P->ambOn >= 0) {
    glUniform1f(P->ambOn, comAmb || veuAmb ? 1.0f : 0.0f);
    if (comAmb || veuAmb) {
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, ambFonte);
      glActiveTexture(GL_TEXTURE0);
    }
  }
  // Altura em pixels do ALVO (o layout e 1920x1080; em retina ou num snapshot
  // o alvo tem outro tamanho): a rampa de borda do SDF mede 1 px dele.
  //
  // RETANGULO DE CANTO VIVO (raio 0) FICA COMO ERA: borda no pixel, sem
  // margem nem rampa (uAlt enorme = degrau). Fio de 1 px, ponteiro de
  // relogio e divisoria caem em posicao fracionaria; com a rampa eles viram
  // duas linhas a meia forca — borrados. So a curva precisa de antialias.
  // Os dois valores ficam em cache por programa: fileira de cartoes iguais
  // nao repete a chamada.
  if (P->tela >= 0 && (P->telaAtual[0] != uTelaW || P->telaAtual[1] != uTelaH)) {
    glUniform2f(P->tela, uTelaW, uTelaH);
    P->telaAtual[0] = uTelaW; P->telaAtual[1] = uTelaH;
  }
  { float alt = r.h * (miniAtiva ? miniEsc : (float)telaH / NV_TELA_H), mg = 0.0f;
    if (PRECISA[modo].sdf) {
      if (raio > 0.0f) mg = 1.0f; else alt = 8192.0f;
    }
    if (P->alt >= 0 && alt != P->altAtual) { glUniform1f(P->alt, alt); P->altAtual = alt; }
    if (P->margem >= 0 && mg != P->margemAtual) { glUniform1f(P->margem, mg); P->margemAtual = mg; } }
  if (P->giro >= 0) {
    float g[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
    if (giroOn) {
      float e = escAtiva;
      g[0] = cosf(giroLayout[0]); g[1] = sinf(giroLayout[0]);
      g[2] = giroLayout[1] * e;   g[3] = giroLayout[2] * e;
    }
    if (memcmp(P->giroAtual, g, sizeof g)) {
      glUniform4f(P->giro, g[0], g[1], g[2], g[3]);
      memcpy(P->giroAtual, g, sizeof g);
    }
  }
  if (P->sub >= 0 && memcmp(P->subAtual, subAtual, sizeof subAtual)) {
    glUniform4f(P->sub, subAtual[0], subAtual[1], subAtual[2], subAtual[3]);
    memcpy(P->subAtual, subAtual, sizeof subAtual);
  }
  if (P->leve >= 0 && (float)efeitosLeves != P->leveAtual) {
    P->leveAtual = (float)efeitosLeves;
    glUniform1f(P->leve, P->leveAtual);
  }
  if (P->cor >= 0)    glUniform4f(P->cor, cr, cg, cb, ca * gfx_opacidade_grupo);
  if (tex && tex != texAtual) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    texAtual = tex;
    gfx_n_bind++;
  }
  { int semMistura = (comAmb || opaco) && blendLigado;
    if (semMistura) glDisable(GL_BLEND);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    if (semMistura) glEnable(GL_BLEND); }
  if (comAmb || veuAmb) {
    // Solta a luz da unidade 1: ela volta a ser ALVO em gfx_ambiente_preparar,
    // e alvo ligado a uma unidade e o laco que o GLES deixa indefinido.
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
  }
  ambIntacta = 0;
#ifdef NV_PERF_FINO
  gfx_ms_rect += (double)(SDL_GetPerformanceCounter() - t0) * gfxFreqMs;
#endif
  // O destaque nao chegou ao pe da tela: o fundo da Dinamica no resto.
  if (dinResto >= 0.0f) {
    memcpy(dinPend, din, sizeof dinPend);
    dinPintar(dinResto);
  }
}

void gfx_cor(GfxRect r, float raio, float cr, float cg, float cb, float ca) {
  gfx_rect(r, 0, GFX_COR, 0, 0, 0, raio, cr, cg, cb, ca);
}
// SOMBRA SOB UM PAINEL OPACO, SEM O MIOLO. A mancha (GFX_SOMBRA) de uma ilha
// cobre o retangulo da ilha inteira, e a ilha opaca pintada logo depois
// esconde tudo o que caiu dentro dela: era preenchimento com mistura jogado
// fora. MEDIDO (tests/fluidez_perf.sh, Ajustes): 0,90 tela de GFX_SOMBRA por
// quadro, ~0,7 dela embaixo das duas ilhas. Aqui a mesma mancha sai em ate
// quatro faixas em volta de `furo` (o miolo da ilha que ela cobre de certeza,
// ja descontado o canto arredondado). Cada faixa e o MESMO rect com outro uSub,
// entao todo fragmento desenhado e identico ao do desenho inteiro.
// So chame se o que vier por cima pinta `furo` inteiro com alfa 1.
void gfx_sombra_vazada(GfxRect s, float foco, float parx, float raio, float cr, float cg, float cb, float ca,
                       GfxRect furo) {
  float x0, y0, x1, y1;
  if (s.w <= 0.0f || s.h <= 0.0f) return;
  x0 = (furo.x - s.x) / s.w; x1 = (furo.x + furo.w - s.x) / s.w;
  y0 = (furo.y - s.y) / s.h; y1 = (furo.y + furo.h - s.y) / s.h;
  if (furo.w <= 0.0f || furo.h <= 0.0f || x0 <= 0.0f || y0 <= 0.0f || x1 >= 1.0f || y1 >= 1.0f ||
      progs[GFX_SOMBRA].sub < 0) {
    gfx_rect(s, 0, GFX_SOMBRA, foco, parx, 0, raio, cr, cg, cb, ca);
    return;
  }
  // A luz ambiente pendente sai ANTES, com o quad inteiro: senao ela seria
  // pintada de dentro do primeiro gfx_rect abaixo, ja com o uSub da faixa (a
  // Agenda da C9 saia com a luz so na faixa de cima).
  gfx_ambiente_descarregar();
  { const float f[4][4] = { { 0, 0, 1, y0 }, { 0, y1, 1, 1 }, { 0, y0, x0, y1 }, { x1, y0, 1, y1 } };
    int k;
    for (k = 0; k < 4; k++) {
      memcpy(subAtual, f[k], sizeof subAtual);
      gfx_rect(s, 0, GFX_SOMBRA, foco, parx, 0, raio, cr, cg, cb, ca);
    }
    subAtual[0] = subAtual[1] = 0.0f; subAtual[2] = subAtual[3] = 1.0f; }
}
// A sombra de uma ILHA/folha (`painel`, canto `raioPx` em pixels) que vai ser
// pintada por cima com alfa `alfaPainel`. Com o miolo opaco (>= 0,98 depois da
// opacidade de grupo) a mancha sai sem o que ele cobre. A 0,98 sobram 2% da
// mancha atras do miolo: no maximo 0,02 x 0,45 do fundo, menos de 1 nivel de
// 8 bits sobre o fundo escuro destas telas.
void gfx_sombra_sob(GfxRect s, float foco, float parx, float raio, float cr, float cg, float cb,
                    float ca, GfxRect painel, float raioPx, float alfaPainel) {
  float m = raioPx + 2.0f;
  if (alfaPainel * gfx_opacidade_grupo >= 0.98f && painel.w > 2.0f * m && painel.h > 2.0f * m)
    gfx_sombra_vazada(s, foco, parx, raio, cr, cg, cb, ca,
                      (GfxRect){ painel.x + m, painel.y + m, painel.w - 2.0f * m, painel.h - 2.0f * m });
  else gfx_rect(s, 0, GFX_SOMBRA, foco, parx, 0, raio, cr, cg, cb, ca);
}
// AS CAMADAS DO DESTAQUE NUMA PASSADA (GFX_HERO_CAM / GFX_HERO_CHEIO_CAM). Devolve 0
// quando nao pode garantir o mesmo pixel do caminho em duas passadas — e
// nesse caso NAO desenha nada; quem chama segue pelo caminho de sempre.
//   texA/aspA/alfaA: a arte que entra (ou a unica); texB/aspB/alfaB: a que
//   sai (texB 0 = so uma camada). O fundo e a luz ambiente assada (imersiva)
//   ou a cor do fundo. (Um fundo social por baixo foi tentado e MEDIDO na C9:
//   a passada ficou mais lenta que o fundo pintado + arte misturada, 30 fps
//   contra 38-45; a fileira social segue pelo caminho de sempre.)
int gfx_hero_camadas(GfxRect r, GfxModo modo, GLuint texA, float aspA, float alfaA,
                     GLuint texB, float aspB, float alfaB) {
  GfxModo cam;
  if (modo == GFX_HERO) cam = GFX_HERO_CAM;
  else if (modo == GFX_HERO_CHEIO) cam = GFX_HERO_CHEIO_CAM;
  else return 0;
  if (!texA && !texB) return 0;
  if (efeitosMinimos || snapAtivo || gfx_desliza_atual != 0.0f || progs[cam].texB < 0) return 0;
  if (gfx_modos_desligados && ((gfx_modos_desligados >> (unsigned)modo) & 1ull)) return 0;
  // Com a luz imersiva o fundo e a luz assada: ela precisa existir e estar
  // intacta (nada desenhado por cima ainda neste quadro).
  if (nv_ambiente_forca > 0.001f && !(ambIntacta && ambFonte && progs[cam].ambOn >= 0))
    return 0;
  if (!texA) { texA = texB; aspA = aspB; alfaA = alfaB; texB = 0; }
  camTexB = texB ? texB : texA;
  camAspB = texB ? aspB : aspA;
  camAlfaB = texB ? alfaB : 0.0f;
  gfx_tex_aspect_atual = aspA;
  gfx_rect(r, texA, cam, 0, 0, 0, 0, 0, 0, 0, alfaA);
  gfx_tex_aspect_atual = 0.0f;
  // Solta a camada B da unidade 2: a textura pode ser a de um cartao que vem
  // logo abaixo, e um objeto ligado em duas unidades ao mesmo tempo nao e
  // garantido igual em todo driver.
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE0);
  return 1;
}
// A LUZ DA "DINAMICA IMERSIVA" E ASSADA NUM QUADRO PEQUENO. MEDIDO na C9 do
// dono em 26/09/2026: com o tema ligado a home parada caia para 34 fps, pior
// quadro 50 ms — o GFX_AMBIENTE rodava quatro smoothstep, um hash e highp em
// CADA um dos 2 milhoes de pixels, todo quadro, duas vezes quando o detalhe
// estava por cima. A luz e enorme e macia: 320x180 guarda tudo o que ela tem,
// e o bilinear da ampliacao e o proprio desfoque. O quadro pequeno e
// redesenhado so quando algo muda — cores, forca, o fundo tingido — ou a cada
// 1/12 s pela respiracao, que e lenta (periodos de dezenas de segundos). O
// custo por quadro vira um quad com UMA leitura de textura.
//
// O assado ja inclui o FUNDO por baixo (clear na cor de NV_COR_FUNDO) e sai
// OPACO: nos dois chamadores a luz entra logo acima de um fundo liso dessa
// mesma cor (o clear de main.c; o gfx_cor do detalhe com o mesmo alfa), entao
// pintar fundo+luz com o alfa do chamador da o mesmo resultado.
#define AMB_W 320
#define AMB_H 180
static GLuint ambFbo;
static int ambFalhou;

// O ALVO LIGADO AGORA, para devolver a ele — e nao ao 0 — depois de criar um
// FBO. Com o alvo interno do nivel 2 (gpunivel.c) a "tela" do quadro e um FBO,
// e voltar cegamente ao 0 no meio do desenho mandaria o resto do quadro para a
// janela, fora da ampliacao.
static GLint fboLigado(void) {
  GLint f = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &f);
  return f;
}

// OS QUADROS PEQUENOS DA LUZ: criacao e assado, os MESMOS para a luz imersiva
// e para os canais de fundo (gfx_luz_canal: "Arte borrada" e "Frost"). Na C9
// este e o caminho que a luz imersiva usa desde 26/09 e que sai certo: alvo
// GL_RGB 320x180, LINEAR + CLAMP, clear na cor do fundo e o desenho por cima.
// Cria `n` pares textura+FBO. Devolve 1 se todos ficaram completos; senao
// apaga o que criou e devolve 0. Nenhuma textura fica ligada a unidade 0: elas
// vao ser alvo (alvo ligado para leitura e o laco que o GLES deixa indefinido).
static int ambCriarAlvos(GLuint *tex, GLuint *fbo, int n) {
  GLint ant = fboLigado();
  int k, ok = 1;
  glActiveTexture(GL_TEXTURE0);
  for (k = 0; k < n; k++) {
    glGenTextures(1, &tex[k]);
    glBindTexture(GL_TEXTURE_2D, tex[k]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, AMB_W, AMB_H, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &fbo[k]);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo[k]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex[k], 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) ok = 0;
  }
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)ant);
  gfx_tex_esquecer(0);  // os binds acima foram por fora do gfx_rect
  if (!ok) {
    glDeleteFramebuffers(n, fbo); glDeleteTextures(n, tex);
    memset(fbo, 0, sizeof(GLuint) * (size_t)n); memset(tex, 0, sizeof(GLuint) * (size_t)n);
  }
  return ok;
}
// Pinta `pintar(ctx)` (coordenadas de layout de tela cheia) no quadro pequeno
// `fbo`, por cima do clear na cor do fundo. O estado do quadro em volta nao
// entra no assado (recorte, opacidade de grupo, deslize, luz pendente, escala
// da camada, mistura) e volta como estava.
static void ambAssarEm(GLuint fbo, void (*pintar)(void *), void *ctx) {
  GLint fboAnt, vpAnt[4];
  int twAnt = telaW, thAnt = telaH, pend = ambPendente, intacta = ambIntacta, blendAnt = blendLigado, recAnt, dinAnt;
  float g = gfx_opacidade_grupo, desl = gfx_desliza_atual;
  GLboolean tesoura;
  GFX_OUTRO_INI();
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fboAnt);
  glGetIntegerv(GL_VIEWPORT, vpAnt);
  tesoura = glIsEnabled(GL_SCISSOR_TEST);
  if (tesoura) glDisable(GL_SCISSOR_TEST);
  recAnt = recorteAtivo; recorteAtivo = 0;
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, 0);
  texAtual = 0;
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  telaW = AMB_W; telaH = AMB_H;
  glViewport(0, 0, AMB_W, AMB_H);
  // A mistura e posta, nao lembrada: quem desliga por fora (gpunivel, player)
  // nao passa pelo gfxBlend, e o cache pode mentir. Sem ela a luz SUBSTITUI o
  // clear em vez de somar (tests/fundo_assado.sh, caso 3).
  gfxBlend(1);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  ambPendente = 0; ambIntacta = 0;   // a luz pendente e da tela, nao do assado
  dinAnt = dinPendente; dinPendente = 0;
  gfx_opacidade_grupo = 1.0f; gfx_desliza_atual = 0.0f;
  { ESC_REAL_INI();
    pintar(ctx);
    ESC_REAL_FIM(); }
  gfx_opacidade_grupo = g; gfx_desliza_atual = desl;
  ambPendente = pend; ambIntacta = intacta; dinPendente = dinAnt;
  gfxBlend(blendAnt);
  telaW = twAnt; telaH = thAnt;
  glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)fboAnt);
  glViewport(vpAnt[0], vpAnt[1], vpAnt[2], vpAnt[3]);
  if (tesoura) glEnable(GL_SCISSOR_TEST);
  recorteAtivo = recAnt;
  GFX_OUTRO_FIM();
}
// O desenho de um assado de luz: as quatro luzes de regiao (nv_ambiente_viva)
// com a forca de agora.
static void ambPintarLuz(void *ctx) {
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  (void)ctx;
  gfx_rect(tela, 0, GFX_AMBIENTE, 0, 0, 0, 0, 1, 1, 1, nv_ambiente_forca);
}

static int ambPreparar(void) {
  if (ambFbo) return 1;
  if (ambFalhou) return 0;
  // QUATRO QUADROS PEQUENOS, em rodizio: o assado de um quadro escreve no que
  // NAO foi lido nos quadros anteriores (ver ambAssar).
  if (!ambCriarAlvos(ambTexPar, ambFboPar, AMB_N)) {
    printf("[cor] luz imersiva sem quadro pequeno (fbo incompleto): desenho direto\n");
    ambFbo = ambTex = 0; ambFalhou = 1;
    return 0;
  }
  ambLado = 0; ambTex = ambTexPar[0]; ambFbo = ambFboPar[0];
  memset(ambChave, 0, sizeof ambChave);
  ambChave[0] = -1.0f;
  return 1;
}

static void ambAssar(void) {
  float k[20];
  int i, j, n = 0;
  k[n++] = floorf(nv_tempo_viva * 12.0f);
  k[n++] = nv_ambiente_forca;
  k[n++] = NV_COR_FUNDO_R; k[n++] = NV_COR_FUNDO_G; k[n++] = NV_COR_FUNDO_B;
  for (i = 0; i < 4; i++) for (j = 0; j < 3; j++) k[n++] = nv_ambiente_viva[i][j];
  if (!memcmp(k, ambChave, sizeof(float) * (size_t)n)) return;
  memcpy(ambChave, k, sizeof(float) * (size_t)n);
  // ALTERNA O ALVO: a GPU pode ainda estar lendo o assado do quadro anterior
  // (a arte por cima dele) quando este novo comeca; escrever no MESMO alvo
  // obriga o driver a esperar aquele desenho terminar antes de assar.
  ambLado = (ambLado + 1) % AMB_N; ambTex = ambTexPar[ambLado]; ambFbo = ambFboPar[ambLado];
  gfx_n_assados++;
  ambAssarEm(ambFbo, ambPintarLuz, NULL);
}

void gfx_ambiente_preparar(void) {
  if (nv_ambiente_forca <= 0.003f || efeitosMinimos || snapAtivo || !ambPreparar()) return;
  ambAssar();
  foscoOk = 1;
}

void gfx_ambiente(float alfa) {
  float a = nv_ambiente_forca * alfa;
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  // Efeitos minimos: uma tela cheia a menos por quadro; o clear ja pinta o fundo.
  if (a <= 0.003f || efeitosMinimos) return;
  // Dentro de um snapshot (outro FBO ativo) ou sem FBO: o caminho antigo.
  if (snapAtivo || !ambPreparar() || ambChave[0] < 0.0f) {
    gfx_ambiente_descarregar();
    { ESC_REAL_INI();
      gfx_rect(tela, 0, GFX_AMBIENTE, 0, 0, 0, 0, 1, 1, 1, a);
      ESC_REAL_FIM(); }
    return;
  }
  // O pedido de main.c (alfa 1, logo depois do clear): fica pendente ate o
  // primeiro desenho decidir (ver gfx_rect). A tela e so o clear ate aqui.
  if (alfa >= 0.999f && gfx_opacidade_grupo >= 0.999f && !ambPendente) {
    ambPendente = 1; ambPendAlfa = alfa; ambIntacta = 1;
    ambPendTex = ambTex; ambPendPont = 0; ambFonte = ambTex;
    return;
  }
  gfx_ambiente_descarregar();
  ambPendTex = ambTex; ambPendPont = 0;
  ambPintar(alfa);
}
// O FUNDO DE TELA CHEIA ADIADO (gfx.h). Mesmo regime da luz pendente: o
// primeiro desenho decide se o fundo e pintado antes dele, lido pelo uAmb da
// arte, ou levado junto de um veu de tela cheia.
int gfx_luz_canal_adiar(GLuint tex, int pontilhar) {
  if (!tex || snapAtivo || miniAtiva || recorteAtivo || giroOn || dinPendente ||
      gfx_opacidade_grupo < 0.999f) return 0;
  // Um fundo opaco de tela cheia: a luz que estivesse pendente nao apareceria.
  ambPendente = 1; ambPendAlfa = 1.0f; ambIntacta = 1;
  ambPendTex = tex; ambPendPont = pontilhar ? 1 : 0; ambFonte = tex;
  return 1;
}
static void ambPintarVeu(float cr, float cg, float cb, float va) {
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  float aspAnt = gfx_tex_aspect_atual, deslAnt = gfx_desliza_atual,
        coverAnt = gfx_card_forcar_cover_atual, g = gfx_opacidade_grupo;
  int bl = blendLigado;
  ESC_REAL_INI();
  gfx_desliza_atual = 0.0f; gfx_card_forcar_cover_atual = 0.0f; gfx_tex_aspect_atual = 0.0f;
  gfx_opacidade_grupo = 1.0f;
  gfxBlend(0);
  gfx_rect(tela, ambPendTex, GFX_SNAP, va, ambPendPont ? 1.0f : 0.0f, 1.0f, 0.0f, cr, cg, cb, 1.0f);
  gfxBlend(bl);
  ambIntacta = 0;   // a tela agora e o fundo COM o veu, nao a fonte
  gfx_opacidade_grupo = g;
  gfx_tex_aspect_atual = aspAnt; gfx_desliza_atual = deslAnt;
  gfx_card_forcar_cover_atual = coverAnt;
  ESC_REAL_FIM();
}
static void ambPintar(float alfa) {
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  // O ESTADO DO DESENHO DE QUEM CHAMOU VOLTA COMO ESTAVA. Com a luz pendente
  // (gfx_ambiente_descarregar), este quad e pintado DE DENTRO do gfx_rect do
  // primeiro desenho do quadro — o destaque do Padrao (GFX_VITRINE), que ja
  // tinha posto a proporcao da textura em gfx_tex_aspect_atual. Zerar aqui
  // sem devolver mandava uTexAsp = 0 para a arte: sem cover, a imagem
  // inteira esticada na faixa 1920x528 (dono, 01/10/2026, C9 com a
  // "Dinamica imersiva"; circulos viravam elipses no tests/homelayouts_shot).
  float aspAnt = gfx_tex_aspect_atual, deslAnt = gfx_desliza_atual,
        coverAnt = gfx_card_forcar_cover_atual;
  ESC_REAL_INI();
  gfx_desliza_atual = 0.0f; gfx_card_forcar_cover_atual = 0.0f;
  // O assado mora em gfx_ambiente_preparar, ANTES do clear da tela: trocar de
  // alvo com a tela ja limpa obriga a GPU de ladrilhos a gravar e reler a tela.
  // (Medido na C9: nao era isso que custava — era a mistura, abaixo — mas o
  // lugar certo de trocar de alvo continua sendo antes do clear.)
  gfx_tex_aspect_atual = 0.0f;
  // OPACO SEM MISTURA quando o chamador e o clear de main.c (alfa 1): o assado
  // ja traz o fundo por baixo, entao ele SUBSTITUI o clear e a GPU nao precisa
  // ler a tela para misturar. MEDIDO na C9: o mesmo quad com mistura custava o
  // bastante para a home parada cair de 60 para 48 fps.
  if (alfa >= 0.999f && gfx_opacidade_grupo >= 0.999f) {
    int intacta = ambIntacta;
    gfxBlend(0);
    gfx_rect(tela, ambPendTex, GFX_SNAP, 0, ambPendPont ? 1.0f : 0.0f, 1.0f, 0.0f, 0, 0, 0, 1.0f);
    gfxBlend(1);
    // A luz pintada sobre o clear continua sendo "so a luz": o destaque que
    // vier depois ainda pode misturar com ela pelo uAmb.
    ambIntacta = intacta;
    if (intacta) ambFonte = ambPendTex;
  } else {
    gfx_rect(tela, ambPendTex, GFX_SNAP, 0, ambPendPont ? 1.0f : 0.0f, 1.0f, 0.0f, 0, 0, 0, alfa);
  }
  gfx_tex_aspect_atual = aspAnt; gfx_desliza_atual = deslAnt;
  gfx_card_forcar_cover_atual = coverAnt;
  ESC_REAL_FIM();
}
// --- FUNDO DA HOME DINAMICA ---------------------------------------------------
//
// SO COR. O fundo antigo assava a arte do titulo desfocada em 160x90 (quatro
// passadas por troca, mais um quad de tela cheia com leitura de textura e, na
// dissolucao, um SEGUNDO quad de tela cheia com mistura) e custava demais na
// C9. Agora e um unico quad OPACO, sem mistura e sem textura: um degrade
// vertical de UMA cor (a do titulo em foco, cruzada no CPU por home.c). Nada e
// assado, nada e decodificado, nenhum FBO.
// ADIADO: o pedido fica pendente ate o primeiro desenho do quadro (ver o
// GFX_VITRINE_DIN em gfx_rect). O fundo e opaco e de tela cheia, entao a luz
// pendente embaixo dele nao apareceria em pixel nenhum.
void gfx_fundo_din_desenhar(const float topo[3], float queda) {
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  if (!snapAtivo && !miniAtiva && !recorteAtivo && !giroOn && gfx_opacidade_grupo >= 0.999f) {
    dinDescarregar();
    dinPend[0] = topo[0]; dinPend[1] = topo[1]; dinPend[2] = topo[2]; dinPend[3] = queda;
    dinPendente = 1;
    ambPendente = 0; ambIntacta = 0;
    // Adiado ate o fim do quadro (ver dinAdiado): so com a tela ainda vazia e
    // um canal alfa de verdade na janela. Sem alfa (EGL de reserva RGB888 ou
    // RGB565, main.c) o ONE_MINUS_DST_ALPHA le sempre 0 e o degrade nunca
    // apareceria: fica o caminho antigo.
    // #318: ficou ligado. O Xiaomi do relato estava no layout 0 (sem fundo da
    // Dinamica) e consertou com a teste-318.1 do mesmo jeito; o que foi
    // desligado de vez foram o descarte (gpunivel.c) e o gputempo.
    if (dinAdiadoOk < 0) {
#ifdef NV_ANDROID
      GLint fboAnt = fboLigado(), bitsA = 0;
      if (fboAnt) glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glGetIntegerv(GL_ALPHA_BITS, &bitsA);
      if (fboAnt) glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)fboAnt);
      dinAdiadoOk = bitsA >= 8;
      printf("[fundo-din] adiado para o fim do quadro: %s (alfa da janela %d bits)\n",
             dinAdiadoOk ? "sim" : "nao", (int)bitsA);
#else
      dinAdiadoOk = 0;
      printf("[fundo-din] adiado para o fim do quadro: nao\n");
#endif
    }
    dinAdiado = dinAdiadoOk && gfx_n_rect == 0 && !gfx_modos_desligados;
    if (dinAdiado) {
      GFX_OUTRO_INI();
      glClearColor(0.0f, 0.0f, 0.0f, 0.0f);   // alfa 0: a cobertura comeca vazia
      glClear(GL_COLOR_BUFFER_BIT);
      GFX_OUTRO_FIM();
      dinNOp = 0;
    }
    return;
  }
  gfx_tex_aspect_atual = 0.0f;
  ESC_REAL_INI();
  gfxBlend(0);   // substitui o clear: a GPU nao le a tela para misturar
  gfx_rect(tela, 0, GFX_FUNDO_DIN, queda, 0, 0, 0.0f, topo[0], topo[1], topo[2], 1.0f);
  gfxBlend(1);
  ESC_REAL_FIM();
}
// O fundo pendente (dinPend) de `y0` (fracao da altura) ate a base: o mesmo
// quad de tela cheia com uSub, entao o degrade e o mesmo pixel do inteiro.
// Sem tesoura e com o estado de quem disparou devolvido.
static void dinPintar(float y0) {
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  float aspAnt = gfx_tex_aspect_atual, deslAnt = gfx_desliza_atual, g = gfx_opacidade_grupo, sub[4];
  int bl = blendLigado, rec = recorteAtivo;
  if (y0 >= 1.0f) return;
  ESC_REAL_INI();
  if (rec) { glDisable(GL_SCISSOR_TEST); recorteAtivo = 0; }
  memcpy(sub, subAtual, sizeof sub);
  subAtual[0] = 0.0f; subAtual[1] = y0 > 0.0f ? y0 : 0.0f; subAtual[2] = 1.0f; subAtual[3] = 1.0f;
  gfx_tex_aspect_atual = 0.0f; gfx_desliza_atual = 0.0f; gfx_opacidade_grupo = 1.0f;
  gfxBlend(0);
  gfx_rect(tela, 0, GFX_FUNDO_DIN, dinPend[3], 0, 0, 0.0f, dinPend[0], dinPend[1], dinPend[2], 1.0f);
  gfxBlend(bl);
  memcpy(subAtual, sub, sizeof sub);
  gfx_tex_aspect_atual = aspAnt; gfx_desliza_atual = deslAnt; gfx_opacidade_grupo = g;
  if (rec) { glEnable(GL_SCISSOR_TEST); recorteAtivo = 1; }
  ESC_REAL_FIM();
}
static void dinDescarregar(void) {
  if (!dinPendente) return;
  dinPendente = 0;
  if (dinAdiado) { dinAdiado = 0; dinResolverAdiado(); return; }
  dinPintar(0.0f);
}
// O fundo adiado, por baixo do que ja foi pintado (ver dinAdiado): mistura
// (ONE_MINUS_DST_ALPHA, ONE), so no complemento dos retangulos opacos. O
// complemento sai em faixas horizontais entre as bordas dos retangulos; em
// cada faixa, os vaos entre os intervalos (ordenados e fundidos) viram um
// pedaco do MESMO quad de tela cheia (uSub), entao o degrade e o mesmo pixel.
static int dinCmpF(const void *a, const void *b) {
  float x = *(const float *)a, y = *(const float *)b;
  return x < y ? -1 : x > y;
}
static void dinQuad(float x0, float y0, float x1, float y1) {
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  if (x1 - x0 < 0.5f || y1 - y0 < 0.5f) return;
  subAtual[0] = x0 / NV_TELA_W; subAtual[1] = y0 / NV_TELA_H;
  subAtual[2] = x1 / NV_TELA_W; subAtual[3] = y1 / NV_TELA_H;
  gfx_rect(tela, 0, GFX_FUNDO_DIN, dinPend[3], 0, 0, 0.0f, dinPend[0], dinPend[1], dinPend[2], 1.0f);
}
static void dinResolverAdiado(void) {
  float aspAnt = gfx_tex_aspect_atual, deslAnt = gfx_desliza_atual, g = gfx_opacidade_grupo, sub[4];
  float ys[2 * DIN_NOP + 2];
  int bl = blendLigado, rec = recorteAtivo, ny = 0, i, k;
  ESC_REAL_INI();
  if (rec) { glDisable(GL_SCISSOR_TEST); recorteAtivo = 0; }
  memcpy(sub, subAtual, sizeof sub);
  gfx_tex_aspect_atual = 0.0f; gfx_desliza_atual = 0.0f; gfx_opacidade_grupo = 1.0f;
  gfxBlend(1);
  glBlendFuncSeparate(GL_ONE_MINUS_DST_ALPHA, GL_ONE, GL_ONE_MINUS_DST_ALPHA, GL_ONE);
  ys[ny++] = 0.0f; ys[ny++] = NV_TELA_H;
  for (i = 0; i < dinNOp; i++) {
    float a = dinOp[i].y, b = dinOp[i].y + dinOp[i].h;
    if (a > 0.0f && a < NV_TELA_H) ys[ny++] = a;
    if (b > 0.0f && b < NV_TELA_H) ys[ny++] = b;
  }
  qsort(ys, (size_t)ny, sizeof ys[0], dinCmpF);
  for (k = 0; k + 1 < ny; k++) {
    float y0 = ys[k], y1 = ys[k + 1], x = 0.0f;
    float xs[DIN_NOP][2]; int nx = 0;
    if (y1 - y0 < 0.5f) continue;
    for (i = 0; i < dinNOp; i++)
      if (dinOp[i].y <= y0 + 0.01f && dinOp[i].y + dinOp[i].h >= y1 - 0.01f) {
        float a = dinOp[i].x, b = dinOp[i].x + dinOp[i].w;
        if (a < 0.0f) a = 0.0f;
        if (b > NV_TELA_W) b = NV_TELA_W;
        if (b > a) { xs[nx][0] = a; xs[nx][1] = b; nx++; }
      }
    // ordena por x0 (insercao: poucos por faixa) e varre os vaos
    for (i = 1; i < nx; i++) {
      float t0 = xs[i][0], t1 = xs[i][1]; int j = i - 1;
      while (j >= 0 && xs[j][0] > t0) { xs[j + 1][0] = xs[j][0]; xs[j + 1][1] = xs[j][1]; j--; }
      xs[j + 1][0] = t0; xs[j + 1][1] = t1;
    }
    for (i = 0; i < nx; i++) {
      if (xs[i][0] > x) dinQuad(x, y0, xs[i][0], y1);
      if (xs[i][1] > x) x = xs[i][1];
    }
    if (x < NV_TELA_W) dinQuad(x, y0, NV_TELA_W, y1);
  }
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  // ALFA 1 SOB OS CARTAZES (#318). Fora deles o degrade (alfa 1) ja fechou o
  // alfa em 1; dentro, o alfa e o que o cartaz deixou, e um cartaz "opaco" com
  // aArte 0,999 ou arte com transparencia deixaria alfa < 1 numa superficie
  // TRANSLUCIDA (NuvioActivity: PixelFormat.TRANSLUCENT, media overlay), e o
  // compositor mostraria o que estiver atras. Um clear so do alfa, com
  // tesoura, em cada retangulo: nenhum fragmento sombreado, o RGB (ja
  // premultiplicado sobre preto) fica igual. Nenhum furo vive ali: os dois
  // furos resolvem o fundo antes de abrir o buraco (gfx_furo, gfx_furo_raio).
  if (dinNOp > 0) {
    float ex = (float)telaW / NV_TELA_W, ey = (float)telaH / NV_TELA_H;
    glEnable(GL_SCISSOR_TEST);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    for (i = 0; i < dinNOp; i++) {
      int x0 = (int)floorf(dinOp[i].x * ex + 0.5f), x1 = (int)floorf((dinOp[i].x + dinOp[i].w) * ex + 0.5f);
      int y0 = (int)floorf((NV_TELA_H - (dinOp[i].y + dinOp[i].h)) * ey + 0.5f);
      int y1 = (int)floorf((NV_TELA_H - dinOp[i].y) * ey + 0.5f);
      if (x1 <= x0 || y1 <= y0) continue;
      glScissor(x0, y0, x1 - x0, y1 - y0);
      glClear(GL_COLOR_BUFFER_BIT);
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (rec) glScissor(recorteBox[0], recorteBox[1], recorteBox[2], recorteBox[3]);
    else glDisable(GL_SCISSOR_TEST);
  }
  gfxBlend(bl);
  memcpy(subAtual, sub, sizeof sub);
  gfx_tex_aspect_atual = aspAnt; gfx_desliza_atual = deslAnt; gfx_opacidade_grupo = g;
  if (rec) { glEnable(GL_SCISSOR_TEST); recorteAtivo = 1; }
  dinNOp = 0;
  ESC_REAL_FIM();
}
// UM CARTAZ OPACO sobre o fundo adiado (ver dinAdiado): guarda dois
// retangulos em cruz, 1 px para dentro e sem os cantos (raio + 1 px), para a
// rampa de borda do SDF e as quinas arredondadas continuarem vendo o fundo.
// Com a tesoura ativa guarda so a parte visivel. Nenhuma chamada GL.
void gfx_mascara_opaca(GfxRect r, float raioPx) {
  float c = (raioPx > 0.0f ? raioPx : 0.0f) + 1.0f;
  GfxRect a, b;
  if (!dinPendente || !dinAdiado || miniAtiva || snapAtivo || giroOn || escAtiva != 1.0f ||
      gfx_opacidade_grupo < 0.999f) return;
  if (r.w < 2.0f * c + 4.0f || r.h < 2.0f * c + 4.0f || dinNOp + 2 > DIN_NOP) return;
  a = (GfxRect){ r.x + c, r.y + 1.0f, r.w - 2.0f * c, r.h - 2.0f };
  b = (GfxRect){ r.x + 1.0f, r.y + c, r.w - 2.0f, r.h - 2.0f * c };
  if (recorteAtivo) {
    // recorteBox e em pixels do buffer, origem embaixo: volta ao layout.
    float ex = (float)telaW / NV_TELA_W, ey = (float)telaH / NV_TELA_H;
    GfxRect rc = { (float)recorteBox[0] / ex, NV_TELA_H - (float)(recorteBox[1] + recorteBox[3]) / ey,
                   (float)recorteBox[2] / ex, (float)recorteBox[3] / ey };
    GfxRect t[2] = { a, b };
    int i;
    for (i = 0; i < 2; i++) {
      float x0 = t[i].x > rc.x ? t[i].x : rc.x;
      float y0 = t[i].y > rc.y ? t[i].y : rc.y;
      float x1 = t[i].x + t[i].w < rc.x + rc.w ? t[i].x + t[i].w : rc.x + rc.w;
      float y1 = t[i].y + t[i].h < rc.y + rc.h ? t[i].y + t[i].h : rc.y + rc.h;
      if (x1 > x0 && y1 > y0) dinOp[dinNOp++] = (GfxRect){ x0, y0, x1 - x0, y1 - y0 };
    }
    return;
  }
  dinOp[dinNOp++] = a;
  dinOp[dinNOp++] = b;
}

void gfx_anel(GfxRect r, float raio, float esp,
              float cr, float cg, float cb, float ca) {
  if (r.h <= 0.0f || esp <= 0.0f) return;
  gfx_rect(r, 0, GFX_ANEL, 0, esp / r.h, 0, raio, cr, cg, cb, ca);
}
// CONCENTRICO: o rect cresce g = folga + esp de cada lado e o raio em pixels
// cresce o MESMO g. Com o raio normalizado copiado da peca (o erro comum), o
// canto do anel fica mais fechado que o da peca e o vao entre os dois engorda
// na diagonal — ou afina, quando a peca e mais redonda que o anel.
void gfx_anel_fora(GfxRect peca, float raio, float folga, float esp,
                   float cr, float cg, float cb, float ca) {
  float g = folga + esp;
  GfxRect r = { peca.x - g, peca.y - g, peca.w + 2.0f * g, peca.h + 2.0f * g };
  if (peca.h <= 0.0f) return;
  gfx_anel(r, (raio * peca.h + g) / r.h, esp, cr, cg, cb, ca);
}
void gfx_cartao_foco_vidro(GfxRect r, float raio, float foco, float alfa,
                           float cr, float cg, float cb) {
  float f, luminancia, lavagem, baseR, baseG, baseB;
  GfxRect halo;
  if (r.w <= 0.0f || r.h <= 0.0f || alfa <= 0.001f) return;
  f = foco < 0.0f ? 0.0f : (foco > 1.0f ? 1.0f : foco);
  if (ajustes_vidro()) {   // vidro: superficie fina + contorno branco, sem mancha
    gfx_vidro_painel(r, raio, 0.45f, alfa);
    if (f > 0.01f) gfx_anel(r, raio, 3.0f, cr, cg, cb, 0.96f * f * alfa);
    return;
  }
  luminancia = cr * 0.2126f + cg * 0.7152f + cb * 0.0722f;
  lavagem = 0.29f * (1.0f - 0.48f * (luminancia > 0.65f ? (luminancia - 0.65f) / 0.35f : 0.0f));
  baseR = 0.057f + cr * 0.028f;
  baseG = 0.062f + cg * 0.028f;
  baseB = 0.078f + cb * 0.032f;
  if (f > 0.001f) {
    float folga = 0.38f;
    halo.x = r.x - r.h * folga;
    halo.y = r.y - r.h * folga;
    halo.w = r.w + r.h * folga * 2.0f;
    halo.h = r.h * (1.0f + folga * 2.0f);
    gfx_rect(halo, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f,
             cr, cg, cb, 0.44f * f * alfa);
  }
  // O fundo deixa transparecer o painel de tras; duas passadas leves criam a
  // lavagem de cor e a reflexao larga de vidro sem contorno nem degraus.
  gfx_cor(r, raio, baseR, baseG, baseB, alfa * (0.93f - 0.08f * f));
  if (f > 0.001f) {
    gfx_rect(r, 0, GFX_VEU_CARD, 0, 0, 0, raio,
             cr, cg, cb, lavagem * f * alfa);
    gfx_brilho_topo(r, raio, 0.38f, 0.88f, 0.92f, 1.0f, 0.13f * f * alfa);
  }
}
// Cor do miolo do vidro: 0,16 e um degrau ACIMA do fundo escuro da pagina, e
// nao um preto — sobre #0D0D0D um miolo preto some e so o aro sobra.
#define VIDRO_MIOLO 0.16f
// VIDRO SEM CONTORNO (dono, 29-30/09: "tirar o contorno", "bem glass mesmo").
// O fio de 1,5 px fazia cada componente ler como caixa desenhada; a separacao
// vem do miolo translucido e, no foco, do aro na cor do realce.
// VIDRO FOSCO (teste de 03/10): a arte borrada do fundo, a mesma luz assada de
// 320x180 que a Imersiva e a "Arte borrada" usam, desenhada DENTRO do painel em
// coordenada de tela — alinha com o que esta atras, como vidro jateado de
// verdade. Um quad texturizado por painel, sem desfoque por quadro. Sem assado
// neste quadro (player, fundo liso) nao desenha nada e o vidro fica normal.
// COM VIDEO VIVO POR BAIXO o assado nao e o fundo: e a luz de 320x180 da arte
// do titulo, e o fosco a pintaria OPACA por cima do plano de video, ampliada
// ~6x pelo bilinear (elipse serrilhada das luzes e a luz do lado direito vira
// uma faixa clara na folha, que fica a direita). O player avisa por quadro.
void gfx_vidro_fosco_bloquear(void) { foscoBloq = 1; }
void gfx_vidro_fosco(GfxRect r, float raio, float a) {
  if (foscoBloq || !ajustes_vidro_fosco() || efeitosMinimos || snapAtivo ||
      r.w <= 0.0f || r.h <= 0.0f || a <= 0.001f) return;
  if (foscoFonte) { gfx_rect(r, foscoFonte, GFX_FOSCO, 0, 0, 0, raio, 1, 1, 1, a); return; }
  if (!foscoOk || !ambTex || ambChave[0] < 0.0f) return;
  gfx_rect(r, ambTex, GFX_FOSCO, 0, 0, 0, raio, 1, 1, 1, a);
}
void gfx_vidro_fosco_fonte(GLuint tex) { foscoFonte = tex; }

// OS CANAIS DE FUNDO DA LUZ ASSADA (fundo.c: "Arte borrada" e "Frost" de tela
// cheia). MEDIDO no Mac (tests/fluidez_perf.sh, Ajustes): o Frost direto pinta
// por quadro um clear, um GFX_VEU_CSS e tres GFX_LUZ do tamanho da tela, todos
// com mistura; a "Arte borrada" direta reaproveitava o quadro da luz imersiva,
// e com a Imersiva ligada as duas chaves se revezavam (dois assados por quadro)
// mais um quad da luz com mistura e o veu de 28% por cima.
//
// C9 (Mali-G71), 05/10/2026: o assado proprio do 1bcd6ebe (gfx_fundo_assado:
// RGBA, desenhado pelo GFX_FOSCO) saiu escuro na TV — a Borrada so com o veu,
// o Frost so com a base — com a conferencia de um padrao de cor passando e o
// Mac e o ANGLE iguais ao direto. A causa na TV nao foi provada. Por isso aqui
// os fundos usam o caminho da LUZ IMERSIVA, o que a C9 ja mostra certo: os
// MESMOS ambCriarAlvos/ambAssarEm (GL_RGB 320x180, clear na cor do fundo) e a
// mesma passada de tela do ambPintar (GFX_SNAP, opaca e sem mistura com alfa
// 1). Cada canal tem os seus quadros e a sua chave, entao a Borrada nao
// disputa o quadro da Imersiva. O veu da Borrada nao e assado: entra na mesma
// passada (uFoco do GFX_SNAP), e o quadro sem veu e a fonte do vidro fosco.
// fundo.c ainda confere UMA vez o pixel da tela contra a conta feita no CPU
// e, se errar, volta ao desenho direto pela sessao (ver fundo.c).
#define CAN_N 2      // canais: 0 Arte borrada, 1 Frost (a imersiva e o ambTex)
#define CAN_ALVOS 2  // conteudo parado: dois alvos alternados bastam
static GLuint canTex[CAN_N][CAN_ALVOS], canFbo[CAN_N][CAN_ALVOS];
static int canLado[CAN_N], canPronto[CAN_N], canLeve[CAN_N], canFalhou;
static unsigned long long canModos[CAN_N];
static float canChave[CAN_N][24];
int gfx_n_fundo_assados;
int gfx_fundo_assado_desligado;
GLuint gfx_luz_canal(int canal, const float *chave, int n, void (*pintar)(void *), void *ctx) {
  if (canal < 0 || canal >= CAN_N || n > 24 || canFalhou || gfx_fundo_assado_desligado ||
      snapAtivo || miniAtiva) return 0;
  // Contexto perdido/refeito: o nome deixou de ser textura. Refaz os alvos.
  if (canTex[canal][0] && !glIsTexture(canTex[canal][0])) {
    memset(canTex[canal], 0, sizeof canTex[canal]); memset(canFbo[canal], 0, sizeof canFbo[canal]);
    canPronto[canal] = 0;
  }
  if (!canTex[canal][0]) {
    if (!ambCriarAlvos(canTex[canal], canFbo[canal], CAN_ALVOS)) {
      printf("[cor] fundo de luz sem quadro pequeno (fbo incompleto): desenho direto\n");
      fflush(stdout);
      canFalhou = 1;
      return 0;
    }
    canPronto[canal] = 0;
  }
  // Os efeitos leves tiram as GFX_LUZ do desenho; o instrumento de campo
  // desliga modos: os dois entram na chave.
  if (canPronto[canal] && canLeve[canal] == efeitosLeves && canModos[canal] == gfx_modos_desligados &&
      !memcmp(chave, canChave[canal], sizeof(float) * (size_t)n))
    return canTex[canal][canLado[canal]];
  memcpy(canChave[canal], chave, sizeof(float) * (size_t)n);
  canPronto[canal] = 1; canLeve[canal] = efeitosLeves; canModos[canal] = gfx_modos_desligados;
  canLado[canal] = (canLado[canal] + 1) % CAN_ALVOS;
  gfx_n_fundo_assados++;
  assandoCanal = 1;
  ambAssarEm(canFbo[canal][canLado[canal]], pintar, ctx);
  assandoCanal = 0;
  return canTex[canal][canLado[canal]];
}
// A passada de tela do ambPintar: GFX_SNAP de tela cheia (fonte FBO), opaca e
// sem mistura com alfa 1. `veu` (rgb + forca) entra no fragmento; `pontilhar`
// passa a cor pelo nv_dither (o Frost direto era pontilhado).
void gfx_luz_canal_desenhar(GLuint tex, float a, const float *veu, int pontilhar) {
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  float aspAnt = gfx_tex_aspect_atual, deslAnt = gfx_desliza_atual, coverAnt = gfx_card_forcar_cover_atual;
  if (!tex || a <= 0.003f) return;
  gfx_tex_aspect_atual = 0.0f; gfx_desliza_atual = 0.0f; gfx_card_forcar_cover_atual = 0.0f;
  { ESC_REAL_INI();
    if (veu) gfx_rect(tela, tex, GFX_SNAP, veu[3], pontilhar ? 1.0f : 0.0f, 1.0f, 0.0f, veu[0], veu[1], veu[2], a);
    else gfx_rect(tela, tex, GFX_SNAP, 0.0f, pontilhar ? 1.0f : 0.0f, 1.0f, 0.0f, 0, 0, 0, a);
    ESC_REAL_FIM(); }
  gfx_tex_aspect_atual = aspAnt; gfx_desliza_atual = deslAnt; gfx_card_forcar_cover_atual = coverAnt;
}
// DIAGNOSTICO (fundo.c, o despejo de uma vez por fundo por execucao): leituras
// de uma vez, nunca por quadro.
static int bmpGravar(const char *caminho, unsigned char *px, int w, int h) {
  // 32 bits, linhas de baixo para cima (a ordem do glReadPixels), R<->B trocados.
  unsigned char cab[54] = { 0 };
  unsigned int n = (unsigned int)w * (unsigned int)h * 4u, tam = 54u + n, i;
  char tmp[600];
  FILE *f;
  for (i = 0; i < n; i += 4) { unsigned char t = px[i]; px[i] = px[i + 2]; px[i + 2] = t; }
  cab[0] = 'B'; cab[1] = 'M';
  cab[2] = tam & 255; cab[3] = (tam >> 8) & 255; cab[4] = (tam >> 16) & 255; cab[5] = (tam >> 24) & 255;
  cab[10] = 54; cab[14] = 40;
  cab[18] = w & 255; cab[19] = (w >> 8) & 255;
  cab[22] = h & 255; cab[23] = (h >> 8) & 255;
  cab[26] = 1; cab[28] = 32;
  cab[34] = n & 255; cab[35] = (n >> 8) & 255; cab[36] = (n >> 16) & 255; cab[37] = (n >> 24) & 255;
  snprintf(tmp, sizeof tmp, "%s.tmp", caminho);
  f = fopen(tmp, "wb");
  if (!f) return 0;
  fwrite(cab, 1, 54, f); fwrite(px, 1, n, f); fclose(f);
  return rename(tmp, caminho) == 0;
}
int gfx_luz_canal_bmp(GLuint tex, const char *caminho) {
  int canal, k, ok = 0;
  unsigned char *px;
  GLint ant;
  for (canal = 0; canal < CAN_N; canal++) for (k = 0; k < CAN_ALVOS; k++)
    if (tex && canTex[canal][k] == tex) goto achou;
  return 0;
achou:
  px = malloc((size_t)AMB_W * AMB_H * 4);
  if (!px) return 0;
  ant = fboLigado();
  glBindFramebuffer(GL_FRAMEBUFFER, canFbo[canal][k]);
  glReadPixels(0, 0, AMB_W, AMB_H, GL_RGBA, GL_UNSIGNED_BYTE, px);
  glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)ant);
  ok = bmpGravar(caminho, px, AMB_W, AMB_H);
  free(px);
  return ok;
}
int gfx_luz_canal_px(GLuint tex, float u, float v, unsigned char rgb[3]) {
  int canal, k;
  unsigned char px[4];
  GLint ant;
  for (canal = 0; canal < CAN_N; canal++) for (k = 0; k < CAN_ALVOS; k++)
    if (tex && canTex[canal][k] == tex) goto achou;
  return 0;
achou:
  ant = fboLigado();
  glBindFramebuffer(GL_FRAMEBUFFER, canFbo[canal][k]);
  // u da esquerda, v de cima (layout); o FBO tem a origem embaixo
  glReadPixels((int)(u * AMB_W), (int)((1.0f - v) * AMB_H), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
  glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)ant);
  rgb[0] = px[0]; rgb[1] = px[1]; rgb[2] = px[2];
  return 1;
}
int gfx_tela_px(float u, float v, unsigned char rgb[3]) {
  unsigned char px[4];
  int x = (int)(u * (float)telaW), y = (int)((1.0f - v) * (float)telaH);
  if (snapAtivo || miniAtiva || telaW <= 0 || telaH <= 0) return 0;
  if (x >= telaW) x = telaW - 1;
  if (y >= telaH) y = telaH - 1;
  glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
  rgb[0] = px[0]; rgb[1] = px[1]; rgb[2] = px[2];
  return 1;
}
int gfx_tela_bmp(const char *caminho) {
  // A tela inteira lida uma vez e gravada pela metade (um pixel de cada 2x2):
  // ~2 MB num 1080p, no /tmp da TV.
  int w = telaW, h = telaH, w2, h2, x, y, ok;
  unsigned char *px, *meio;
  if (snapAtivo || miniAtiva || w <= 1 || h <= 1) return 0;
  w2 = w / 2; h2 = h / 2;
  px = malloc((size_t)w * h * 4);
  meio = malloc((size_t)w2 * h2 * 4);
  if (!px || !meio) { free(px); free(meio); return 0; }
  glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);
  for (y = 0; y < h2; y++) for (x = 0; x < w2; x++)
    memcpy(meio + ((size_t)y * w2 + x) * 4, px + ((size_t)(y * 2) * w + x * 2) * 4, 4);
  ok = bmpGravar(caminho, meio, w2, h2);
  free(px); free(meio);
  return ok;
}
// 78% e o vidro de sempre: o fator escala o alfa do miolo (folha e painel).
float gfx_vidro_opacidade(void) { return ajustes_vidro_opacidade() / 0.78f; }
void gfx_vidro_painel(GfxRect r, float raio, float fundo, float a) {
  float k = gfx_vidro_opacidade(), al;
  if (r.w <= 0.0f || r.h <= 0.0f || a <= 0.001f) return;
  al = fundo * k; if (al > 1.0f) al = 1.0f;
  gfx_vidro_fosco(r, raio, a);
  gfx_vidro_miolo(r, raio, VIDRO_MIOLO, VIDRO_MIOLO, VIDRO_MIOLO * 1.04f, al * a, a);
}
// IMERSIVA NO VIDRO (acentos-mockup.html, quadro 5): a ilha leva 16% da luz
// do destaque (L 0,42, croma <= 0,11), entrando e saindo com a luz ambiente.
// Fora da Imersiva nv_ambiente_forca e 0 e isto nao desenha nada.
void gfx_vidro_matiz(GfxRect r, float raio, float a) {
  float f = 0.16f * nv_ambiente_forca * a;
  if (f <= 0.002f || r.w <= 0.0f || r.h <= 0.0f) return;
  gfx_cor(r, raio, nv_luz_viva[0], nv_luz_viva[1], nv_luz_viva[2], f);
}
// MIOLO + MATIZ NUMA PASSADA SO. As duas camadas sao cor lisa no MESMO
// retangulo com os MESMOS cantos, e "B sobre A sobre o fundo" e, pela conta da
// mistura (SRC_ALPHA/ONE_MINUS_SRC_ALPHA na cor, ONE/ONE_MINUS_SRC_ALPHA no
// alfa), uma camada so com
//   alfa = aA + aB - aA*aB   e   cor = (cB*aB + cA*aA*(1-aB)) / alfa.
// Nesta GPU o custo e o de preenchimento: cada ilha de vidro com a Imersiva
// eram duas camadas misturadas do tamanho dela (tests/fluidez_perf.sh, Ajustes
// no cenario do dono: 0,84 tela por quadro so de matiz). O pixel muda so pelo
// arredondamento de 8 bits da camada intermediaria (1 nivel) e na franja de
// 1 px do canto, onde a cobertura parcial entra uma vez em vez de duas.
// A opacidade de grupo entra NA CONTA (cada camada a recebia sozinha).
void gfx_vidro_miolo(GfxRect r, float raio, float cr, float cg, float cb, float ca, float a) {
  float f = 0.16f * nv_ambiente_forca * a, g = gfx_opacidade_grupo, aA, aB, al;
  if (r.w <= 0.0f || r.h <= 0.0f) return;
  // Sem matiz, ou com a matiz na cor exata do destaque com o degrade ligado (o
  // gfx_rect a trocaria pelo degrade): as duas passadas de sempre.
  if (f <= 0.002f || g <= 0.001f ||
      (nv_grad_ativo && nv_luz_viva[0] == nv_acento_viva[0] &&
       nv_luz_viva[1] == nv_acento_viva[1] && nv_luz_viva[2] == nv_acento_viva[2])) {
    gfx_cor(r, raio, cr, cg, cb, ca);
    gfx_vidro_matiz(r, raio, a);
    return;
  }
  aA = ca * g; if (aA > 1.0f) aA = 1.0f; if (aA < 0.0f) aA = 0.0f;
  aB = f * g;  if (aB > 1.0f) aB = 1.0f;
  al = aA + aB - aA * aB;
  if (al <= 0.0f) return;
  gfx_cor(r, raio,
          (nv_luz_viva[0] * aB + cr * aA * (1.0f - aB)) / al,
          (nv_luz_viva[1] * aB + cg * aA * (1.0f - aB)) / al,
          (nv_luz_viva[2] * aB + cb * aA * (1.0f - aB)) / al,
          al / g);
}
// Painel lateral/flutuante (menu, Fontes, Salvos): miolo frio translucido e um
// brilho largo no alto, sem aro.
void gfx_vidro_folha(GfxRect r, float raio, float a) {
  if (r.w <= 0.0f || r.h <= 0.0f || a <= 0.001f) return;
  gfx_vidro_fosco(r, raio, a);
  gfx_vidro_miolo(r, raio, 0.085f, 0.088f, 0.10f, ajustes_vidro_opacidade() * a, a);
  gfx_brilho_topo(r, raio, 0.38f, 0.88f, 0.92f, 1.0f, 0.06f * a);
}
void gfx_vidro_aro(GfxRect r, float raio, float esp, float cr, float cg, float cb, float ca) {
  if (!ajustes_vidro_contorno()) return;
  gfx_anel(r, raio, esp, cr, cg, cb, ca);
}
// Cartao/linha em repouso dentro de uma folha de vidro: um veu claro, sem aro.
void gfx_vidro_superficie(GfxRect r, float raio, float a) {
  if (r.w <= 0.0f || r.h <= 0.0f || a <= 0.001f) return;
  gfx_cor(r, raio, 1, 1, 1, 0.05f * a);
}
// A COR DO FOCO NO VIDRO E O REALCE ESCOLHIDO. O vidro muda a SUPERFICIE (fina,
// translucida, sem brilho nem sombra), nao a cor de quem esta selecionado: com
// o tema padrao o realce e branco e o resultado e o da referencia (contorno /
// pilula brancos); com "Cor de destaque" ou os temas dinamicos o contorno e a
// pilula seguem a cor. Forcar branco aqui apagava o realce de todas as telas.
void gfx_vidro_foco(GfxRect r, float raio, float foco, float a) {
  float f = foco < 0.0f ? 0.0f : (foco > 1.0f ? 1.0f : foco), cr, cg, cb;
  if (f <= 0.01f || a <= 0.001f) return;
  ajustes_acento(&cr, &cg, &cb);
  // "Contorno do vidro" DESLIGADO (pedido do dono, 30/09: "o contorno que eu
  // queria desligar e esse quando ta em foco, mais minimalista"): sem anel,
  // o foco e so o fundo — um degrau claro e a lavagem do realce, fortes o
  // bastante para ler a 3 m. Cartaz (gfx_vidro_cartao) nao passa por aqui:
  // sem superficie propria, sem anel ele ficaria sem foco nenhum.
  if (!ajustes_vidro_contorno()) {
    gfx_cor(r, raio, 1, 1, 1, 0.07f * f * a);
    gfx_cor(r, raio, cr, cg, cb, 0.20f * f * a);
    return;
  }
  gfx_cor(r, raio, cr, cg, cb, 0.09f * f * a);
  gfx_anel(r, raio, 2.0f, cr, cg, cb, 0.96f * f * a);
}
void gfx_vidro_cartao(GfxRect r, float raio, float foco, float a) {
  float f = foco < 0.0f ? 0.0f : (foco > 1.0f ? 1.0f : foco), cr, cg, cb;
  if (f <= 0.01f || a <= 0.001f) return;
  ajustes_acento(&cr, &cg, &cb);
  gfx_anel_fora(r, raio, 2.0f, 3.0f, cr, cg, cb, 0.96f * f * a);
}
void gfx_vidro_pilula_cheia(GfxRect r, float raio, float foco, float a) {
  float f = foco < 0.0f ? 0.0f : (foco > 1.0f ? 1.0f : foco), cr, cg, cb;
  if (f <= 0.01f || a <= 0.001f) return;
  ajustes_acento(&cr, &cg, &cb);
  gfx_cor(r, raio, cr, cg, cb, f * a);
}
// Texto sobre a pilula cheia: a tinta que contrasta com o realce (escura sobre
// realce claro, branca sobre escuro); fora do foco, o cinza claro de sempre.
int gfx_vidro_tinta(float foco) {
  if (foco < 0.5f) return 235;
  return ajustes_acento_tinta(NULL, NULL, NULL) < 0.5f ? 20 : 255;
}
// Superficie de um painel/pilula NA COR DO REALCE (acao principal em repouso):
// o vidro comum com uma lavagem do realce e aro na cor dele. Sem realce
// (tema branco) fica um degrau mais claro que o vidro, sem cor nenhuma.
void gfx_vidro_painel_acento(GfxRect r, float raio, float fundo, float a) {
  float cr, cg, cb;
  if (r.w <= 0.0f || r.h <= 0.0f || a <= 0.001f) return;
  ajustes_acento(&cr, &cg, &cb);
  gfx_cor(r, raio, VIDRO_MIOLO, VIDRO_MIOLO, VIDRO_MIOLO * 1.04f, fundo * a);
  gfx_cor(r, raio, cr, cg, cb, 0.20f * a);
  gfx_vidro_aro(r, raio, 1.5f, cr, cg, cb, 0.55f * a);
}
void gfx_luz_canto(GfxRect r, float raio, float cx, float cy, float alcance,
                   float cr, float cg, float cb, float ca) {
  if (r.w <= 0.0f || r.h <= 0.0f || ca <= 0.001f) return;
  gfx_rect(r, 0, GFX_LUZ, alcance / r.h, cx / r.w, cy / r.h, raio, cr, cg, cb, ca);
}
// Buraco transparente por onde o plano de video do aparelho aparece.
//
// Precisa ser com o blend DESLIGADO. Com blend ligado, escrever alpha 0 apenas
// mistura com o que ja esta no destino e o alpha final continua 1 — a
// superficie segue opaca e o video permanece invisivel, sem nenhum erro. E o
// alpha aqui e o canal de composicao da janela, entao isto so tem efeito com
// SDL_GL_ALPHA_SIZE 8 pedido antes de criar a janela.
// O FURO RETO E UM CLEAR COM TESOURA, nao um quad. O quad opaco de tela cheia
// (o plano de video no player, o trailer no destaque cheio) passava pelo
// pipeline inteiro so para escrever (0,0,0,0); numa GPU de ladrilhos o clear
// de um retangulo e o caminho barato para o mesmo resultado. A tesoura cobre
// exatamente os pixels que o quad cobriria (centro do pixel dentro do rect:
// de round(x0) a round(x1)), e o recorte que estava ativo volta depois.
void gfx_furo(GfxRect r) {
  float ex = (float)telaW / NV_TELA_W, ey = (float)telaH / NV_TELA_H;
  int x0, x1, y0, y1, cheia;
  GFX_TR_RECT(r.x, r.y, r.w, r.h);
  if (escAtiva != 1.0f) { r.x *= escAtiva; r.y *= escAtiva; r.w *= escAtiva; r.h *= escAtiva; }
  if (r.w <= 0.0f || r.h <= 0.0f) return;
  if (gfx_modos_desligados && ((gfx_modos_desligados >> (unsigned)GFX_COR) & 1ull)) return;
  cheia = r.x <= 0.0f && r.y <= 0.0f && r.x + r.w >= NV_TELA_W && r.y + r.h >= NV_TELA_H;
  // A luz pendente ficaria por cima do furo se saisse depois dele; sob um
  // furo de tela cheia ela nao apareceria em pixel nenhum.
  if (dinPendente) { if (cheia) dinPendente = 0; else dinDescarregar(); }
  if (ambPendente) { if (cheia) ambPendente = 0; else gfx_ambiente_descarregar(); }
  ambIntacta = 0;
  gfx_n_rect++;
  { float area = (r.w * r.h) / (NV_TELA_W * NV_TELA_H) *
                 (subAtual[2] - subAtual[0]) * (subAtual[3] - subAtual[1]);
    if (gfx_rastro_grandes) printf("[qd-rect] furo %.0fx%.0f@%.0f,%.0f\n", r.w, r.h, r.x, r.y);
    gfx_fill += area; gfx_fill_modo[GFX_COR] += area;
    if (area >= 0.5f) gfx_n_cheio++; }
  x0 = (int)floorf(r.x * ex + 0.5f); x1 = (int)floorf((r.x + r.w) * ex + 0.5f);
  y0 = (int)floorf((NV_TELA_H - (r.y + r.h)) * ey + 0.5f);
  y1 = (int)floorf((NV_TELA_H - r.y) * ey + 0.5f);
  if (x1 <= x0 || y1 <= y0) return;
  GFX_OUTRO_INI();
  glEnable(GL_SCISSOR_TEST);
  glScissor(x0, y0, x1 - x0, y1 - y0);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  if (recorteAtivo) glScissor(recorteBox[0], recorteBox[1], recorteBox[2], recorteBox[3]);
  else glDisable(GL_SCISSOR_TEST);
  GFX_OUTRO_FIM();
}

void gfx_furo_raio(GfxRect r, float raio) {
  // O fundo adiado sai ANTES do buraco, como no gfx_furo: resolvido depois,
  // o ONE_MINUS_DST_ALPHA pintaria o degrade dentro do furo e o video sumiria.
  dinDescarregar();
  gfxBlend(0);
  gfx_rect(r, 0, GFX_COR, 0, 0, 0, raio, 0, 0, 0, 0);
  gfxBlend(1);
}

// Multiplica TUDO o que ja foi desenhado neste quadro (cor e alfa, que e
// pre-multiplicado na superficie) por `a`: onde havia home opaca passa a
// haver home * a sobre o plano de video * (1 - a). Um quad de tela cheia.
void gfx_dissolver_tela(float a) {
  GfxRect t = { 0, 0, NV_TELA_W, NV_TELA_H };
  if (a >= 0.999f) return;
  if (a < 0.0f) a = 0.0f;
  glBlendFuncSeparate(GL_ZERO, GL_SRC_ALPHA, GL_ZERO, GL_SRC_ALPHA);
  { ESC_REAL_INI();
    gfx_rect(t, 0, GFX_COR, 0, 0, 0, 0.0f, 0, 0, 0, a);
    ESC_REAL_FIM(); }
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

void gfx_esqueleto(GfxRect r, float raio, float cr, float cg, float cb, float ca) {
  // Com animacoes reduzidas, a superficie parada: e o mesmo "carregando" sem
  // nada passando por cima.
  if (anim_politica_reduzida) { gfx_cor(r, raio, cr, cg, cb, ca); return; }
  // Ciclo de 1,8 s: 1,35 s atravessando (de -0,25 a 1,35 da tela) e o resto
  // com a onda fora, para a tela respirar entre uma passada e outra.
  Uint32 ms = SDL_GetTicks() % 1800u;
  float onda = -0.25f + 1.60f * ((float)ms / 1350.0f);
  gfx_rect(r, 0, GFX_ESQUELETO, r.w / NV_TELA_W, onda, r.x / NV_TELA_W, raio,
           cr, cg, cb, ca);
}

void gfx_textura(GfxRect r, GLuint tex) {
  gfx_rect(r, tex, GFX_CARD, 0, 0, 0, 0.0f, 0, 0, 0, 1);
}

int gfx_snap_iniciar(int w, int h) {
  snapW = w; snapH = h;
  glGenTextures(1, &snapTex);
  glBindTexture(GL_TEXTURE_2D, snapTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gfx_tex_esquecer(0);  // o bind acima foi por fora do gfx_rect
  glGenFramebuffers(1, &snapFbo);
  GLint ant = fboLigado();
  glBindFramebuffer(GL_FRAMEBUFFER, snapFbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, snapTex, 0);
  GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)ant);
  if (st != GL_FRAMEBUFFER_COMPLETE) {
    printf("snapshot indisponivel (fbo 0x%x) — seguindo sem ele\n", st);
    glDeleteFramebuffers(1, &snapFbo); glDeleteTextures(1, &snapTex);
    snapFbo = snapTex = 0;
    return 0;
  }
  return 1;
}

int gfx_snap_ok(void) { return snapFbo != 0; }

static unsigned snapGeracao;
int gfx_snap_ativo(void) { return snapAtivo; }
unsigned gfx_snap_geracao(void) { return snapGeracao; }
void gfx_snap_comecar(void) {
  if (!snapFbo || snapAtivo) return;
  snapGeracao++;
  gfx_ambiente_descarregar();   // a luz e da tela, nao do snapshot
  GFX_OUTRO_INI();
  snapTelaW = telaW; snapTelaH = telaH;
  telaW = snapW; telaH = snapH;
  snapAtivo = 1;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &snapFboAnt);
  glGetIntegerv(GL_VIEWPORT, snapVpAnt);
  glBindFramebuffer(GL_FRAMEBUFFER, snapFbo);
  // uTela continua em coordenadas de tela cheia: o viewport menor faz a
  // reducao sozinho, e nenhum codigo de layout precisa saber que existe FBO.
  glViewport(0, 0, snapW, snapH);
  GFX_OUTRO_FIM();
}

// 1 se o snapshot (ainda ligado como alvo) saiu PRETO nas faixas de baixo da
// metade esquerda — onde ha home por baixo do painel. Tres linhas de 1100 px
// lidas do FBO: uns 13 KB, so quando a copia e (re)pintada. Existe porque a
// copia parada do painel de Salvos saiu preta na TCL (Android) sem erro de GL:
// o sintoma do dono e a home sumida atras do Social.
int gfx_snap_vazio(int *maxCanal) {
  static unsigned char lin[1100 * 4];
  int k, i, mx = 0;
  if (!snapFbo || !snapAtivo) return 0;
  for (k = 0; k < 3; k++) {
    int y = snapH * (k == 0 ? 3 : k == 1 ? 5 : 7) / 10;
    glReadPixels(0, y, 1100 < snapW ? 1100 : snapW, 1, GL_RGBA, GL_UNSIGNED_BYTE, lin);
    for (i = 0; i < 1100 * 4; i += 4) {
      if (lin[i] > mx) mx = lin[i];
      if (lin[i + 1] > mx) mx = lin[i + 1];
      if (lin[i + 2] > mx) mx = lin[i + 2];
    }
  }
  if (maxCanal) *maxCanal = mx;
  return mx <= 3;
}

void gfx_snap_terminar(void) {
  if (!snapFbo || !snapAtivo) return;
  GFX_OUTRO_INI();
  telaW = snapTelaW; telaH = snapTelaH;
  snapAtivo = 0;
  glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)snapFboAnt);
  glViewport(snapVpAnt[0], snapVpAnt[1], snapVpAnt[2], snapVpAnt[3]);
  GFX_OUTRO_FIM();
}

void gfx_snap_desenhar(void) {
  if (!snapTex) return;
  GfxRect r = { 0, 0, NV_TELA_W, NV_TELA_H };
  gfx_tex_aspect_atual = 0.0f;
  { ESC_REAL_INI();
    gfx_rect(r, snapTex, GFX_SNAP, 0, 0.0f, 1.0f, 0.0f, 0, 0, 0, 1.0f);
    ESC_REAL_FIM(); }
}

void gfx_snap_encerrar(void) {
  if (snapFbo) glDeleteFramebuffers(1, &snapFbo);
  if (snapTex) glDeleteTextures(1, &snapTex);
  snapFbo = snapTex = 0;
}

// --- icones ------------------------------------------------------------------
static char dirIcones[512];
static GLuint smartphoneTex;
static int smartphoneTentou;

// O TPK atualiza somente libnuvio.so: res/art ainda pode vir de um pacote
// anterior ao botao do celular. Um icone essencial novo precisa acompanhar o
// nucleo. E o MESMO PNG Lucide do pacote, sem desenho aproximado; 1244 bytes
// no binario e 64 KiB de textura, somente quando usado pela primeira vez.
// Nao passa pelo cache de arquivo: ausencia de arte antiga nao entra no ciclo
// de decode/recuo nem deixa o botao vazio. Uma falha de decode/alocacao tambem
// nao vira trabalho por quadro; encerrar o contexto permite outra tentativa.
static GLuint smartphoneObter(void) {
  SDL_RWops *rw;
  SDL_Surface *s, *rgba;
  if (smartphoneTentou) return smartphoneTex;
  smartphoneTentou = 1;
  rw = SDL_RWFromConstMem(gfx_smartphone_png, sizeof gfx_smartphone_png);
  s = rw ? IMG_Load_RW(rw, 1) : NULL;
  if (!s) return 0;
  rgba = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ABGR8888, 0);
  SDL_FreeSurface(s);
  if (!rgba) return 0;
  glGenTextures(1, &smartphoneTex);
  if (smartphoneTex) {
    glBindTexture(GL_TEXTURE_2D, smartphoneTex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gfx_tex_esquecer(0);
  }
  SDL_FreeSurface(rgba);
  return smartphoneTex;
}

static void iconesEncerrar(void) {
  if (smartphoneTex) {
    gfx_tex_esquecer(smartphoneTex);
    glDeleteTextures(1, &smartphoneTex);
  }
  smartphoneTex = 0;
  smartphoneTentou = 0;
}

void gfx_icones_dir(const char *dirArte) {
  snprintf(dirIcones, sizeof dirIcones, "%s/icones", dirArte ? dirArte : ".");
}

void gfx_icone(GfxRect r, const char *nome, float cr, float cg, float cb, float ca) {
  char cam[600];
  GLuint t;
  if (!nome || !nome[0] || ca <= 0.0f || r.w <= 0.0f || r.h <= 0.0f) return;
  if (!strcmp(nome, "aj_smartphone")) t = smartphoneObter();
  else {
    if (!dirIcones[0]) return;
    // Caminho ABSOLUTO: o diretorio de trabalho do app nao e a pasta da arte,
    // e com caminho relativo o IMG_Load falha e o icone some sem erro.
    // Mesma armadilha ja documentada em extras_caminho_marca.
    snprintf(cam, sizeof cam, "%s/%s.png", dirIcones, nome);
    // Pede pela largura de desenho: um icone de 38px nao precisa dos 128 do
    // arquivo, e o teto por uso e o que mantem o cache fora do vermelho.
    t = tex_obter_larg(cam, r.w * escAtiva);
  }
  if (!t) return;
  gfx_tex_aspect_atual = 0.0f;   // o arquivo ja e quadrado
  gfx_rect(r, t, GFX_MARCA, 0, 0, 0, 0.0f, cr, cg, cb, ca);
}

void gfx_recorte(float x, float y, float w, float h) {
  GFX_OUTRO_INI();
  if (w <= 0.0f || h <= 0.0f) { glEnable(GL_SCISSOR_TEST); glScissor(0, 0, 0, 0);
                                recorteAtivo = 1; memset(recorteBox, 0, sizeof recorteBox);
                                GFX_OUTRO_FIM(); return; }
  // Duas conversoes acontecem aqui, e em nenhum outro lugar do app:
  //
  // 1. glScissor conta do canto INFERIOR esquerdo; o resto trabalha com y
  //    crescendo para baixo.
  // 2. glScissor fala em PIXEIS DO BUFFER, nao nas coordenadas de layout. Em
  //    tela retina o buffer tem o dobro do tamanho, e sem a escala o recorte
  //    cobria um quarto da area pedida — o menu lateral perdia os dois
  //    primeiros itens e os rotulos saiam cortados no meio da palavra.
  float ex = (float)telaW / NV_TELA_W, ey = (float)telaH / NV_TELA_H;
  GFX_TR_RECT(x, y, w, h);
  if (escAtiva != 1.0f) { x *= escAtiva; y *= escAtiva; w *= escAtiva; h *= escAtiva; }
  int yy = (int)((NV_TELA_H - (y + h)) * ey);
  if (miniAtiva) {   // layout -> pixel do alvo da miniatura
    ex = ey = miniEsc;
    x -= miniX0;
    yy = (int)((float)miniPxH - (y + h - miniY0) * miniEsc);
  }
  glEnable(GL_SCISSOR_TEST);
  glScissor((int)(x * ex), yy, (int)(w * ex), (int)(h * ey));
  recorteAtivo = 1;
  recorteBox[0] = (int)(x * ex); recorteBox[1] = yy;
  recorteBox[2] = (int)(w * ex); recorteBox[3] = (int)(h * ey);
  GFX_OUTRO_FIM();
}
void gfx_sem_recorte(void) { glDisable(GL_SCISSOR_TEST); recorteAtivo = 0; }

static int criaAlvo(int i, int w, int h) {
  glGenTextures(1, &borTex[i]);
  glBindTexture(GL_TEXTURE_2D, borTex[i]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gfx_tex_esquecer(0);  // o bind acima foi por fora do gfx_rect
  glGenFramebuffers(1, &borFbo[i]);
  GLint ant = fboLigado();
  glBindFramebuffer(GL_FRAMEBUFFER, borFbo[i]);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, borTex[i], 0);
  GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)ant);
  return st == GL_FRAMEBUFFER_COMPLETE;
}

int gfx_borrao_iniciar(int w, int h) {
  borW = w; borH = h;
  // 0/1 = par do detalhe (ping-pong do gaussiano), 2/3 = par da home
  if (!criaAlvo(0, w, h) || !criaAlvo(1, w, h) ||
      !criaAlvo(2, w, h) || !criaAlvo(3, w, h)) {
    gfx_borrao_encerrar();
    printf("desfoque indisponivel: seguindo sem ele\n");
    return 0;
  }
  return 1;
}

// Desenha a arte no alvo e passa duas vezes o gaussiano. So roda quando a arte
// muda — o resultado fica guardado na textura.
void gfx_borrao_gerar(int via, unsigned int tex, float texAspecto) {
  int a0 = via ? 2 : 0, a1 = via ? 3 : 1;
  if (!borFbo[a0] || !tex) return;
  GfxRect cheio = { 0, 0, NV_TELA_W, NV_TELA_H };
  gfx_ambiente_descarregar();   // pendente e da tela, nao deste alvo
  { static int nLog;   // #318: o desfoque em FBO roda quando a tela nova traz arte nova
    if (nLog < 40) { nLog++; printf("[transicao] desfoque de fundo (FBO %s) t=%u\n", via ? "home" : "detalhe", (unsigned)SDL_GetTicks()); } }
  ESC_REAL_INI();
  GFX_OUTRO_INI();
  GLint fboAnt = fboLigado(), vpAnt[4];
  glGetIntegerv(GL_VIEWPORT, vpAnt);
  gfxBlend(0);
  glViewport(0, 0, borW, borH);

  glBindFramebuffer(GL_FRAMEBUFFER, borFbo[a0]);
  // As tres passadas cobrem o alvo inteiro, opacas: o que havia nele nao
  // interessa, e dizer isso a GPU de ladrilhos poupa a leitura dele.
  gpun_descartar_cor(0);
  gfx_tex_aspect_atual = texAspecto;
  gfx_rect(cheio, tex, GFX_SNAP, 0, 0, 0, 0.0f, 0, 0, 0, 1.0f);
  gfx_tex_aspect_atual = 0.0f;

  // O passo e maior que um texel: com passo de um texel o desfoque mal cobre
  // 4px do alvo, que esticado 4x ainda deixa a estrutura da imagem visivel.
  float px = NV_BLUR_PASSO / (float)borW, py = NV_BLUR_PASSO / (float)borH;
  glBindFramebuffer(GL_FRAMEBUFFER, borFbo[a1]);
  gpun_descartar_cor(0);
  gfx_rect(cheio, borTex[a0], GFX_BLUR, 0, px, 0.0f, 0.0f, 0, 0, 0, 1.0f);
  glBindFramebuffer(GL_FRAMEBUFFER, borFbo[a0]);
  gpun_descartar_cor(0);
  gfx_rect(cheio, borTex[a1], GFX_BLUR, 0, 0.0f, py, 0.0f, 0, 0, 0, 1.0f);

  gfxBlend(1);
  // Volta ao alvo de ANTES (a janela, ou o alvo interno do nivel 2), e nao ao 0.
  glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)fboAnt);
  glViewport(vpAnt[0], vpAnt[1], vpAnt[2], vpAnt[3]);
  GFX_OUTRO_FIM();
  ESC_REAL_FIM();
}

void gfx_borrao_desenhar(int via, GfxRect r, float alpha) {
  int a0 = via ? 2 : 0;
  if (!borTex[a0]) return;
  gfx_tex_aspect_atual = 0.0f;
  gfx_rect(r, borTex[a0], GFX_FUNDO, 0, 0, 0, 0.0f, 0, 0, 0, alpha);
}

void gfx_borrao_encerrar(void) {
  for (int i = 0; i < 4; i++) {
    if (borFbo[i]) { glDeleteFramebuffers(1, &borFbo[i]); borFbo[i] = 0; }
    if (borTex[i]) { glDeleteTextures(1, &borTex[i]); borTex[i] = 0; }
  }
}

// --- MINIATURA DESFOCADA (blurUnwatchedEpisodes, #133) ------------------------
//
// O ajuste "Desfocar nao assistidos" era lido da conta e da tela de Ajustes e
// NAO ERA USADO EM LUGAR NENHUM: ajustes_desfocar_nao_assistidos() so tinha um
// chamador, o teste da conta. No LG, no Mac e no Tizen o card saia nitido.
//
// POR QUE ASSIM, e nao um shader de desfoque direto no card: um kernel 2D por
// pixel custaria dezenas de leituras de textura em cada um dos 640x414 pixels
// de cada card, a cada quadro — e a pagina de detalhe ja e a tela que anda
// perto do limite de preenchimento (ver gfx_fill). Nem mipmap com bias serve:
// no Tizen o WebGL 1 recusa piramide em textura NPOT (tex_cache.c), e o still
// do episodio quase nunca e potencia de dois.
//
// Aqui a arte e reduzida UMA VEZ para um alvo de 96x54 pelas duas passadas do
// GFX_BLUR que o fundo do detalhe ja usa, e o resultado fica guardado. No
// quadro, o card desenha essa textura minuscula esticada com filtro linear:
// o mesmo custo de um card nitido. A geracao sao dois quads de 96x54, limitada
// a NV_DESF_POR_QUADRO por quadro.
#define NV_DESF_W 96
#define NV_DESF_H 54
#define NV_DESF_N 16            // cards visiveis + folga para a rolagem
#define NV_DESF_POR_QUADRO 2
// Passo do gaussiano em texels do alvo. Com 9 amostras o borrao alcanca
// +-4 passos: 1.6 * 4 / 96 = ~6.7% da largura para cada lado, ~43 px num card
// de 640. Rosto e texto do still somem; a cor e a composicao continuam.
#define NV_DESF_PASSO 1.6f

typedef struct { GLuint src, tex; unsigned long chave, uso; } Desf;
static Desf desf[NV_DESF_N];
static GLuint desfFbo = 0, desfTmp = 0;
static int desfFalhou = 0;
static unsigned long desfRelogio = 0;

static unsigned long desfHash(const char *s) {
  unsigned long h = 5381;
  if (s) while (*s) h = h * 33u + (unsigned char)*s++;
  return h;
}

static void desfEsquecerFonte(GLuint tex) {
  for (int i = 0; i < NV_DESF_N; i++)
    if (desf[i].src == tex) { desf[i].src = 0; desf[i].chave = 0; }
}

static GLuint desfNovaTex(void) {
  GLuint t = 0;
  glGenTextures(1, &t);
  glBindTexture(GL_TEXTURE_2D, t);
  // RGBA e nao RGB: e o unico formato que o WebGL 1 GARANTE renderizavel.
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, NV_DESF_W, NV_DESF_H, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gfx_tex_esquecer(0);  // o bind acima foi por fora do gfx_rect
  return t;
}

static int desfPreparar(void) {
  GLenum st;
  if (desfFbo) return 1;
  if (desfFalhou) return 0;
  desfTmp = desfNovaTex();
  glGenFramebuffers(1, &desfFbo);
  glBindFramebuffer(GL_FRAMEBUFFER, desfFbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, desfTmp, 0);
  st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (st != GL_FRAMEBUFFER_COMPLETE) {
    printf("[desfoque] alvo da miniatura incompleto (0x%x): cards sem desfoque\n",
           (unsigned)st);
    fflush(stdout);
    desfFalhou = 1;
    desfEncerrar();
    return 0;
  }
  return 1;
}

GLuint gfx_desfocado(GLuint src, const char *chave) {
  unsigned long h = desfHash(chave);
  int i, vago = -1;
  if (!src) return 0;
  desfRelogio++;
  for (i = 0; i < NV_DESF_N; i++)
    if (desf[i].tex && desf[i].src == src && desf[i].chave == h) {
      desf[i].uso = desfRelogio;
      return desf[i].tex;
    }
  if (desfGeradosQuadro >= NV_DESF_POR_QUADRO) return 0;
  gfx_ambiente_descarregar();   // pendente e da tela, nao deste alvo
  { static int nLog;   // #318: idem, a copia desfocada de um cartaz
    if (nLog < 40) { nLog++; printf("[transicao] copia desfocada (FBO) t=%u\n", (unsigned)SDL_GetTicks()); } }
  // Vaga: primeiro uma sem fonte, senao a usada ha mais tempo.
  for (i = 0; i < NV_DESF_N; i++)
    if (!desf[i].src) { vago = i; break; }
  if (vago < 0) {
    vago = 0;
    for (i = 1; i < NV_DESF_N; i++)
      if (desf[i].uso < desf[vago].uso) vago = i;
  }
  {
    GLint fboAnt = 0, vp[4];
    GLboolean tesoura = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean mistura = glIsEnabled(GL_BLEND);
    GLuint dst;
    float aspAnt = gfx_tex_aspect_atual;
    GfxRect cheio = { 0, 0, NV_TELA_W, NV_TELA_H };
    // Estado lido do GL de proposito, e so aqui: o card pode estar sendo
    // desenhado dentro do snapshot da home ou sob gfx_recorte, e voltar
    // cegamente para o framebuffer 0 com o viewport da tela estragaria os dois.
    // Custa um glGet por miniatura NOVA, nao por quadro.
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fboAnt);
    glGetIntegerv(GL_VIEWPORT, vp);
    if (!desfPreparar()) { glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)fboAnt); return 0; }
    if (!desf[vago].tex) desf[vago].tex = desfNovaTex();
    dst = desf[vago].tex;
    ESC_REAL_INI();
    GFX_OUTRO_INI();
    glDisable(GL_SCISSOR_TEST);
    gfxBlend(0);
    glViewport(0, 0, NV_DESF_W, NV_DESF_H);
    gfx_tex_aspect_atual = 0.0f;   // a arte INTEIRA, esticada; o card recorta depois
    // Passada 1: horizontal, lendo a arte original e ja reduzindo. Escrever
    // num FBO inverte o eixo y (ver GFX_SNAP); a passada 2 inverte de novo, e
    // o resultado sai de pe para o GFX_CARD, como uma arte comum.
    glBindFramebuffer(GL_FRAMEBUFFER, desfFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, desfTmp, 0);
    gpun_descartar_cor(0);   // a passada cobre o alvo inteiro, opaca
    gfx_rect(cheio, src, GFX_BLUR, 0, NV_DESF_PASSO / (float)NV_DESF_W, 0.0f,
             0.0f, 0, 0, 0, 1.0f);
    // Passada 2: vertical, do intermediario para a textura guardada.
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst, 0);
    gpun_descartar_cor(0);
    gfx_rect(cheio, desfTmp, GFX_BLUR, 0, 0.0f, NV_DESF_PASSO / (float)NV_DESF_H,
             0.0f, 0, 0, 0, 1.0f);
    gfx_tex_aspect_atual = aspAnt;
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)fboAnt);
    glViewport(vp[0], vp[1], vp[2], vp[3]);
    gfxBlend(mistura);
    if (tesoura) glEnable(GL_SCISSOR_TEST);
    GFX_OUTRO_FIM();
    ESC_REAL_FIM();
  }
  desf[vago].src = src;
  desf[vago].chave = h;
  desf[vago].uso = desfRelogio;
  desfGeradosQuadro++;
  return desf[vago].tex;
}

// DIAGNOSTICO DE UMA VEZ (fundo.c, "Arte borrada"): um pixel da copia
// desfocada `tex` (u da esquerda, v de cima, 0..1). A copia sai de pe, entao
// a linha 0 do alvo e o topo da imagem. Devolve 0 se `tex` nao e uma copia.
int gfx_desfocado_px(GLuint tex, float u, float v, unsigned char rgb[3]) {
  unsigned char px[4];
  GLint ant;
  int i, x, y, achou = 0;
  for (i = 0; i < NV_DESF_N; i++) if (tex && desf[i].tex == tex) achou = 1;
  if (!achou || !desfFbo) return 0;
  x = (int)(u * NV_DESF_W); y = (int)(v * NV_DESF_H);
  if (x < 0) x = 0; if (x >= NV_DESF_W) x = NV_DESF_W - 1;
  if (y < 0) y = 0; if (y >= NV_DESF_H) y = NV_DESF_H - 1;
  ant = fboLigado();
  glBindFramebuffer(GL_FRAMEBUFFER, desfFbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
  glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, desfTmp, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)ant);
  rgb[0] = px[0]; rgb[1] = px[1]; rgb[2] = px[2];
  return 1;
}

static void desfEncerrar(void) {
  for (int i = 0; i < NV_DESF_N; i++) {
    if (desf[i].tex) { gfx_tex_esquecer(desf[i].tex); glDeleteTextures(1, &desf[i].tex); }
    desf[i].tex = desf[i].src = 0; desf[i].chave = desf[i].uso = 0;
  }
  if (desfFbo) { glDeleteFramebuffers(1, &desfFbo); desfFbo = 0; }
  if (desfTmp) { gfx_tex_esquecer(desfTmp); glDeleteTextures(1, &desfTmp); desfTmp = 0; }
}

// --- MINIATURA ---------------------------------------------------------------------
// Uma tela de verdade desenhada num alvo proprio e mostrada reduzida (a previa
// das Novidades, o inspetor do Guia de uso). O desenho continua em coordenadas
// de layout: o uTela e o viewport fazem a reducao, como no snapshot, e a
// regiao [x0, x0 + w/esc] x [y0, y0 + h/esc] do layout cobre o alvo inteiro.
static GLint miniFboAnt, miniVpAnt[4];
static int miniRecAnt;
int gfx_mini_alvo(GfxMini *m, int w, int h) {
  GLint ant;
  GLenum st;
  if (m->fbo && m->w == w && m->h == h) return 1;
  gfx_mini_liberar(m);
  glGenTextures(1, &m->tex);
  glBindTexture(GL_TEXTURE_2D, m->tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gfx_tex_esquecer(0);
  glGenFramebuffers(1, &m->fbo);
  ant = fboLigado();
  glBindFramebuffer(GL_FRAMEBUFFER, m->fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m->tex, 0);
  st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)ant);
  m->w = w; m->h = h;
  if (st != GL_FRAMEBUFFER_COMPLETE) { gfx_mini_liberar(m); return 0; }
  return 1;
}
void gfx_mini_comecar(GfxMini *m, float x0, float y0, float esc) {
  if (!m->fbo || miniAtiva || esc <= 0.0f) return;
  // O fundo pendente (luz, Frost adiado, fundo da Dinamica) e da TELA: sai
  // nela antes de o desenho ir para o alvo da miniatura.
  gfx_ambiente_descarregar();
  GFX_OUTRO_INI();
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &miniFboAnt);
  glGetIntegerv(GL_VIEWPORT, miniVpAnt);
  miniRecAnt = recorteAtivo;
  glBindFramebuffer(GL_FRAMEBUFFER, m->fbo);
  glDisable(GL_SCISSOR_TEST);
  recorteAtivo = 0;
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);
  // A miniatura e uma tela REAL de 1920x1080 reduzida: dentro dela nao ha
  // camada ampliada (escala.h), venha de onde vier quem a desenha.
  miniEscAnt = escAtiva; escAtiva = 1.0f;
  miniTrAnt = gfxTr; gfxTr.on = 0;
  miniAtiva = 1; miniPxW = m->w; miniPxH = m->h;
  miniX0 = x0; miniY0 = y0; miniEsc = esc;
  uTelaW = x0 + (float)m->w / esc;
  uTelaH = y0 + (float)m->h / esc;
  glViewport((GLint)(-x0 * esc), 0, (GLsizei)(uTelaW * esc + 0.5f), (GLsizei)(uTelaH * esc + 0.5f));
  GFX_OUTRO_FIM();
}
void gfx_mini_terminar(void) {
  if (!miniAtiva) return;
  GFX_OUTRO_INI();
  miniAtiva = 0;
  escAtiva = miniEscAnt;
  gfxTr = miniTrAnt;
  uTelaW = NV_TELA_W; uTelaH = NV_TELA_H;
  glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)miniFboAnt);
  glViewport(miniVpAnt[0], miniVpAnt[1], miniVpAnt[2], miniVpAnt[3]);
  glDisable(GL_SCISSOR_TEST);
  if (miniRecAnt) { glEnable(GL_SCISSOR_TEST); glScissor(recorteBox[0], recorteBox[1], recorteBox[2], recorteBox[3]); }
  recorteAtivo = miniRecAnt;
  GFX_OUTRO_FIM();
}
void gfx_mini_desenhar(const GfxMini *m, GfxRect r, float raioPx, float a) {
  if (!m->tex || r.w < 1.0f || r.h < 1.0f) return;
  gfx_tex_aspect_atual = 0.0f;
  gfx_rect(r, m->tex, GFX_MINI, 0, 0, 0, raioPx / r.h, 1, 1, 1, a);
}
// Dentro da miniatura: a imagem some no retangulo `r` (layout), de nada no
// alto a `alfa` na base — o mask-image do mockup. Multiplica cor e alfa do
// alvo (o GFX_MINI desfaz a cor na leitura).
void gfx_mini_esvanecer(GfxRect r, float alfa) {
  if (!miniAtiva) return;
  glBlendFuncSeparate(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
  gfx_veu_css(r, 0, 1.0f, 1.0f, alfa);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}
void gfx_mini_liberar(GfxMini *m) {
  if (m->fbo) glDeleteFramebuffers(1, &m->fbo);
  if (m->tex) { glDeleteTextures(1, &m->tex); gfx_tex_esquecer(m->tex); }
  m->fbo = m->tex = 0; m->w = m->h = 0;
}
