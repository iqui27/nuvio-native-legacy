#include "gfx.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include "layout.h"
#include <stdio.h>
#include "anim.h"

// Um programa por modo, e os uniforms de cada um: as posicoes NAO coincidem
// entre programas, entao guardar um conjunto so devolveria lixo no segundo
// shader que usasse a mesma variavel.
typedef struct {
  GLuint prog;
  GLint rect, tela, tex, foco, par, raio, cor, asp, texAsp, forcarCover, borda, varre, fundo;
} Programa;
static Programa progs[GFX_NMODOS];
static int progAtual = -1;
// Proporcao da textura corrente, para o "cover". Fica global porque o desenho e
// imediato: quem chama define antes de cada rect com textura.
float gfx_tex_aspect_atual = 0.0f;
float gfx_card_forcar_cover_atual = 0.0f;
// 1 = desenhar o rebordo claro que marca o cartaz em foco; 0 = nao desenhar.
// Vive aqui, e nao num parametro de gfx_rect, pela mesma razao do aspecto da
// textura: sao dezenas de chamadas e a resposta e a mesma para todas dentro do
// mesmo quadro. Quem o define e a tela, a partir do ajuste da pessoa.
float gfx_borda_foco_atual = 1.0f;
// Deslocamento da faixa especular do cartaz em foco (revela.h): 0 = no lugar
// de repouso. Mesmo regime do rebordo: global, e quem mexe devolve a 0.
float gfx_varre_atual = 0.0f;
float gfx_opacidade_grupo = 1.0f;
// Tamanho real do alvo da tela (em retina, maior que 1920x1080). Guardado aqui
// porque toda volta de FBO precisa restaurar o viewport com ele.
static int telaW = (int)NV_TELA_W, telaH = (int)NV_TELA_H;
void gfx_tamanho_alvo(int w, int h) { telaW = w; telaH = h; }

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
  "varying highp vec2 vUv;\n"
  "void main(){\n"
  "  vUv = aPos;\n"
  "  vec2 p = uRect.xy + aPos * uRect.zw;\n"
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
  "#else\n"
  "varying mediump vec2 vUv;\n"
  "#endif\n"
  "uniform sampler2D uTex;\n"
  "uniform float uFoco;\n"
  "uniform float uBorda;\n"
  "uniform float uVarre;\n"
  "uniform vec2  uPar;\n"
  "uniform float uRaio;\n"
  "uniform vec4  uCor;\n"
  "uniform float uAspect;\n"
  "uniform float uTexAsp;   // w/h da TEXTURA; 0 = nao ajustar\n"
  "uniform float uForceCover;\n"
  // A COR DO FUNDO DA PAGINA, para as rampas que dissolvem a arte nela
  // (destaque, destaque cheio, detalhe). Era vec3(0.051) cravado em cada uma;
  // com o tema dinamico estilizado o fundo e tingido (layout.h,
  // NV_COR_FUNDO_R), e a rampa cravada deixaria uma emenda onde a arte acaba.
  "uniform vec3  uFundo;\n";

// SDF de retangulo arredondado, corrigido pela proporcao — sem a correcao o
// canto de um card landscape sai oval.
static const char *FS_SDF =
  "float sdf(vec2 uv, float r, float asp){\n"
  "  vec2 p = (uv - 0.5) * vec2(asp, 1.0);\n"
  "  vec2 b = vec2(0.5*asp, 0.5) - r;\n"
  "  vec2 q = abs(p) - b;\n"
  "  return min(max(q.x,q.y),0.0) + length(max(q,0.0)) - r;\n"
  "}\n";

// "cover": recorta o excedente em vez de deformar a arte.
static const char *FS_COVER =
  "vec2 cover(vec2 uv){\n"
  "  if (uTexAsp <= 0.0) return uv;\n"
  "  float ra = uAspect / uTexAsp;\n"
  "  if (ra > 1.0) uv.y = (uv.y - 0.5) / ra + 0.5;\n"
  "  else          uv.x = (uv.x - 0.5) * ra + 0.5;\n"
  "  return uv;\n"
  "}\n";

static const char *FS_CORPO[GFX_NMODOS] = {
  // GFX_CARD — arte com cantos, over-scan de parallax e especular no foco
  "void main(){\n"
  "  float d = sdf(vUv, uRaio, uAspect);\n"
  "  float m = smoothstep(0.006,-0.006,d);\n"
  "  if (m <= 0.001) discard;\n"
  // COVER VIRA CONTAIN quando a arte foge muito da moldura (issue #89):
  // catalogo como o Xperience declara fileira deitada mas serve o poster
  // retrato de sempre — cover num card 16:9 cortava ~60% da altura. Aqui
  // a amostragem abre no eixo oposto e a faixa recebe a cor do esqueleto,
  // a mesma do card vazio. Dentro de +/-25% de proporcao segue cover.
  "  float ra = uAspect / max(uTexAsp, 0.01);\n"
  "  float contem = (uForceCover < 0.5 && uTexAsp > 0.05 && (ra < 0.80 || ra > 1.25)) ? 1.0 : 0.0;\n"
  "  vec2 uv = cover(vUv);\n"
  "  if (contem > 0.5) {\n"
  "    uv = vUv;\n"
  "    if (ra > 1.0) uv.x = (uv.x - 0.5) * ra + 0.5;\n"
  "    else          uv.y = (uv.y - 0.5) / ra + 0.5;\n"
  "  }\n"
  // Over-scan de 3%: a Apple reserva essa margem em todas as bordas para que o
  // parallax nunca revele borda vazia (diferenca entre "actual size" e "safe
  // zone" nas tabelas do Top Shelf). Sem ela o clamp estica o pixel da borda.
  "  uv = (uv - 0.5) * (0.94 - 0.05*uFoco) + 0.5 + uPar;\n"
  "  vec3 cor = (contem > 0.5 && (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0))\n"
  "    ? vec3(0.173)\n"
  "    : texture2D(uTex, clamp(uv, 0.0, 1.0)).rgb;\n"
  "  if (uFoco > 0.004) {\n"
  // uVarre = a LUZ ENTRANDO (revela.h): a faixa nasce fora do canto
  // inferior esquerdo e desliza ate o repouso; em 0 e o especular de sempre.
  // Um pouco mais forte enquanto anda, para o olho seguir a passagem.
  "    float e = (dot(vUv-0.5, vec2(0.5029,-0.8644)) + uPar.x*3.0 + uVarre) * 3.0;\n"
  "    cor += exp(-e*e) * (0.16 + 0.10*min(abs(uVarre),1.0)) * uFoco;\n"
  "    cor *= (0.80 + 0.20*uFoco);\n"
  // O REBORDO E OPCIONAL (Ajustes > Borda no cartaz em foco). O resto do
  // bloco de foco fica: o cartaz em foco continua mais claro e com o
  // especular, entao desligar a borda nao deixa o foco invisivel.
  "    cor += smoothstep(0.010,0.0,abs(d)) * uFoco * 0.35 * uBorda;\n"
  "  } else cor *= 0.80;\n"
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
  "  float t = clamp(-d * 2.0, 0.0, 1.0);\n"
  "  gl_FragColor = vec4(uCor.rgb, t * t * uFoco * uCor.a);\n"
  "}\n",

  // GFX_COR — retangulo/pilula de cor solida
  "void main(){\n"
  "  float m = smoothstep(0.006,-0.006, sdf(vUv, uRaio, uAspect));\n"
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
  "  vec3 c = texture2D(uTex, clamp(cover(vUv), 0.0, 1.0)).rgb;\n"
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
  "  if (uPar.x > 0.5) { gl_FragColor = vec4(bg, clamp(ah + av - ah*av, 0.0, 1.0) * uCor.a); return; }\n"
  "  c = mix(c, bg, clamp(ah + av - ah*av, 0.0, 1.0));\n"
  "  gl_FragColor = vec4(c, uCor.a);\n"
  "}\n",

  // GFX_VEU — escurece a base E a esquerda, onde fica o texto sobreposto
  "void main(){\n"
  "  float m = smoothstep(0.006,-0.006, sdf(vUv, uRaio, uAspect));\n"
  "  if (m <= 0.001) discard;\n"
  "  float gb = smoothstep(0.34, 1.0, vUv.y);\n"
  "  float ge = smoothstep(0.62, 0.0, vUv.x) * 0.78;\n"
  "  gl_FragColor = vec4(0.0,0.0,0.0, clamp(gb+ge-gb*ge,0.0,1.0)*uCor.a*m);\n"
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
  "  gl_FragColor = vec4(cb * ky * vg, uCor.a);\n"
  "}\n",

  // GFX_VEU_TOPO — degrade de cima para baixo, sob o cabecalho fixo
  "void main(){\n"
  "  gl_FragColor = vec4(0.0,0.0,0.0, smoothstep(1.0,0.15,vUv.y)*uCor.a);\n"
  "}\n",

  // GFX_SNAP — imagem ja pronta: sem SDF, sem efeito, so o quad.
  // uPar.y > 0.5 diz que a fonte e um FBO: como o alvo de render tem a origem
  // no canto INFERIOR e o resto do app trabalha com y crescendo para baixo, a
  // imagem sai de cabeca para baixo se lida direto.
  "void main(){\n"
  "  vec2 uv = (uPar.y > 0.5) ? vec2(vUv.x, 1.0 - vUv.y) : vUv;\n"
  "  gl_FragColor = vec4(texture2D(uTex, uv).rgb, uCor.a);\n"
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
  "  float a = 1.0 - clamp(x/0.0780,0.0,1.0)*0.05\n"
  "                - clamp((x-0.0780)/0.0936,0.0,1.0)*0.11\n"
  "                - clamp((x-0.1716)/0.1092,0.0,1.0)*0.14\n"
  "                - clamp((x-0.2808)/0.1248,0.0,1.0)*0.18\n"
  "                - clamp((x-0.4056)/0.1092,0.0,1.0)*0.18\n"
  "                - clamp((x-0.5148)/0.0936,0.0,1.0)*0.16\n"
  "                - clamp((x-0.6084)/0.0936,0.0,1.0)*0.11\n"
  "                - clamp((x-0.7020)/0.0780,0.0,1.0)*0.07;\n"
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
  "  if (uPar.x > 0.5) { gl_FragColor = vec4(bg, clamp(a,0.0,1.0) * uFoco * uCor.a); return; }\n"
  "  c = mix(c, bg, clamp(a,0.0,1.0) * uFoco);\n"
  "  gl_FragColor = vec4(c, uCor.a);\n"
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
  "  vec3 c = texture2D(uTex, clamp(cover(vUv), 0.0, 1.0)).rgb;\n"
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
  "  if (uPar.x > 0.5) { gl_FragColor = vec4(bg, clamp(ah + av - ah*av, 0.0, 1.0) * uCor.a); return; }\n"
  "  c = mix(c, bg, clamp(ah + av - ah*av, 0.0, 1.0));\n"
  "  gl_FragColor = vec4(c, uCor.a);\n"
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
  //   uPar.y = numero de tracos do pontilhado; 0 (ou <0.5) = anel continuo
  "void main(){\n"
  "  float d = sdf(vUv, uRaio, uAspect);\n"
  "  float esp = max(uPar.x, 0.0015);\n"
  // A borda externa e a interna recebem o mesmo esmaecimento, senao o anel fica
  // com o lado de dentro serrilhado e o de fora liso.
  "  float m = smoothstep(esp, esp*0.55, abs(d));\n"
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
  "  gl_FragColor = vec4(uCor.rgb, g * uCor.a);\n"
  "}\n",
  // GFX_SOCIAL: broad off-centre light, quiet left side for copy.
  "void main(){\n"
  "  vec2 p = vUv;\n"
  "  float glow = 1.0-smoothstep(0.0,0.95,length((p-vec2(0.88,0.18))*vec2(1.0,1.25)));\n"
  "  float ribbon = 1.0-smoothstep(0.04,0.40,abs(p.y-0.12-p.x*0.44));\n"
  "  vec3 c = mix(vec3(0.105,0.065,0.095),vec3(0.40,0.14,0.18),glow);\n"
  "  c += vec3(0.065,0.028,0.020)*ribbon*glow;\n"
  "  c = mix(c,vec3(0.047,0.045,0.055),smoothstep(0.44,1.0,p.y));\n"
  "  gl_FragColor = vec4(c,uCor.a);\n"
  "}\n",

  // GFX_AVATAR: mascara radial exata. O GFX_CARD usa o SDF de retangulo
  // arredondado e over-scan de parallax; num circulo pequeno isso deixava a
  // aresta irregular e deslocava a fotografia dentro do disco.
  "void main(){\n"
  "  vec2 p=(vUv-0.5)*vec2(uAspect,1.0);\n"
  "  float d=length(p);\n"
  "  float m=smoothstep(0.500,0.486,d);\n"
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
  "  float m=smoothstep(0.500,0.486,length(p));\n"
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
  "  float m = smoothstep(0.006,-0.006, sdf(vUv, uRaio, uAspect));\n"
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
  "  gl_FragColor = vec4(uCor.rgb, uCor.a * g * m);\n"
  "}\n",

  // GFX_BRILHO_TOPO — realce claro no alto, rampa por pixel, cantos do card.
  //
  // A rampa e o smoothstep AO QUADRADO, pelo mesmo motivo escrito em
  // GFX_VEU_BAIXO: com a rampa linear o olho enxerga a segunda derivada e
  // aparece uma emenda onde ela comeca — que e exatamente o "risco" que um
  // retangulo chapado ja fazia, so que mais fraco.
  "void main(){\n"
  "  float d = sdf(vUv, uRaio, uAspect);\n"
  "  float m = smoothstep(0.006,-0.006,d);\n"
  "  if (m <= 0.001) discard;\n"
  "  float t = 1.0 - smoothstep(0.0, max(uPar.x, 0.001), vUv.y);\n"
  "  gl_FragColor = vec4(uCor.rgb, uCor.a * t * t * m);\n"
  "}\n",

  // GFX_ARTE — a textura intacta, recortada pelos cantos. Ver a nota em gfx.h.
  "void main(){\n"
  "  float m = smoothstep(0.006,-0.006, sdf(vUv, uRaio, uAspect));\n"
  "  if (m <= 0.001) discard;\n"
  "  vec4 t = texture2D(uTex, vUv);\n"
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
  "  float m = smoothstep(0.006,-0.006, sdf(vUv, uRaio, uAspect));\n"
  "  if (m <= 0.001) discard;\n"
  "  GFX_LUZ_PREC vec2 p = (vUv - uPar) * vec2(uAspect, 1.0);\n"
  // O falloff linear ao quadrado ainda mostrava um limite circular em TVs com
  // pouca precisao de fragmento. Usar highp quando a GPU oferece evita que
  // mediump arredonde as faixas de alfa; o easing cubico abre a penumbra e
  // deixa o falloff no mesmo formato nas TVs GLES2 sem highp.
  "  GFX_LUZ_PREC float t = clamp(1.0 - length(p) / max(uFoco, 0.001), 0.0, 1.0);\n"
  "  GFX_LUZ_PREC float suave = t * t * (3.0 - 2.0 * t);\n"
  "  gl_FragColor = vec4(uCor.rgb, suave * suave * uCor.a * m);\n"
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
  "  float m = smoothstep(0.006,-0.006, sdf(vUv, uRaio, uAspect));\n"
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
  "  gl_FragColor = vec4(base + vec3(0.82, 0.86, 1.0) * (s + s2), 1.0);\n"
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
  {0,0}    /* GFX_CEU — procedural, sem textura */
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
  char fonte[6000];
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
    progs[m].par  = glGetUniformLocation(p, "uPar");
    progs[m].raio = glGetUniformLocation(p, "uRaio");
    progs[m].cor  = glGetUniformLocation(p, "uCor");
    progs[m].asp  = glGetUniformLocation(p, "uAspect");
    progs[m].texAsp = glGetUniformLocation(p, "uTexAsp");
    progs[m].forcarCover = glGetUniformLocation(p, "uForceCover");
    progs[m].borda  = glGetUniformLocation(p, "uBorda");
    progs[m].varre  = glGetUniformLocation(p, "uVarre");
    progs[m].fundo  = glGetUniformLocation(p, "uFundo");
    glUseProgram(p);
    glUniform2f(progs[m].tela, NV_TELA_W, NV_TELA_H);
    glUniform1i(progs[m].tex, 0);
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
  glEnable(GL_BLEND);
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
void gfx_encerrar(void) {
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
int    gfx_n_cheio = 0;   // desenhos que cobrem >= 50% da tela
static double gfxFreqMs = 0.0;
static int desfGeradosQuadro = 0;   // ver gfx_desfocado
void gfx_novo_quadro(void) {
  gfx_n_rect = gfx_n_prog = gfx_n_bind = gfx_n_outros = 0;
  gfx_ms_rect = gfx_ms_outros = 0.0;
  gfx_fill = 0.0; gfx_n_cheio = 0;
  desfGeradosQuadro = 0;
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

void gfx_rect(GfxRect r, GLuint tex, GfxModo modo, float foco,
              float parx, float pary, float raio,
              float cr, float cg, float cb, float ca) {
  if ((int)modo < 0 || (int)modo >= GFX_NMODOS) return;
  if (gfxFreqMs == 0.0) gfxFreqMs = 1000.0 / (double)SDL_GetPerformanceFrequency();
  (void)gfxFreqMs;
#ifdef NV_PERF_FINO
  Uint64 t0 = SDL_GetPerformanceCounter();
#endif
  gfx_n_rect++;
  { float area = (r.w * r.h) / (NV_TELA_W * NV_TELA_H);
    gfx_fill += area;
    if (area >= 0.5f) gfx_n_cheio++; }
  const Programa *P = &progs[modo];
  if (progAtual != (int)modo) { glUseProgram(P->prog); progAtual = (int)modo; gfx_n_prog++; }
  // Uniform que o shader do modo nao declara volta como -1 do link; passar -1
  // ao glUniform e no-op valido mas ainda paga a travessia da chamada GL. Num
  // quadro tipico da home sao centenas de gfx_rect, a maioria em modos que nao
  // usam foco/parallax/texAsp, entao o teste barato aqui poupa a chamada cara.
  glUniform4f(P->rect, r.x, r.y, r.w, r.h);
  if (P->foco >= 0)   glUniform1f(P->foco, foco);
  if (P->par >= 0)    glUniform2f(P->par, parx, pary);
  if (P->raio >= 0)   glUniform1f(P->raio, raio);
  if (P->asp >= 0)    glUniform1f(P->asp, r.h > 0 ? r.w / r.h : 1.0f);
  if (P->texAsp >= 0) glUniform1f(P->texAsp, gfx_tex_aspect_atual);
  if (P->forcarCover >= 0) glUniform1f(P->forcarCover, gfx_card_forcar_cover_atual);
  if (P->borda >= 0)  glUniform1f(P->borda, gfx_borda_foco_atual);
  if (P->varre >= 0)  glUniform1f(P->varre, gfx_varre_atual);
  // So os tres modos de rampa declaram uFundo: e uma chamada por destaque ou
  // fundo de detalhe desenhado, nao por retangulo.
  if (P->fundo >= 0)  glUniform3f(P->fundo, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B);
  if (P->cor >= 0)    glUniform4f(P->cor, cr, cg, cb, ca * gfx_opacidade_grupo);
  if (tex && tex != texAtual) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    texAtual = tex;
    gfx_n_bind++;
  }
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
#ifdef NV_PERF_FINO
  gfx_ms_rect += (double)(SDL_GetPerformanceCounter() - t0) * gfxFreqMs;
#endif
}

void gfx_cor(GfxRect r, float raio, float cr, float cg, float cb, float ca) {
  gfx_rect(r, 0, GFX_COR, 0, 0, 0, raio, cr, cg, cb, ca);
}
void gfx_cartao_foco_vidro(GfxRect r, float raio, float foco, float alfa,
                           float cr, float cg, float cb) {
  float f, luminancia, lavagem, baseR, baseG, baseB;
  GfxRect halo;
  if (r.w <= 0.0f || r.h <= 0.0f || alfa <= 0.001f) return;
  f = foco < 0.0f ? 0.0f : (foco > 1.0f ? 1.0f : foco);
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
    gfx_rect(r, 0, GFX_BRILHO_TOPO, 0, 0.38f, 0, raio,
             0.88f, 0.92f, 1.0f, 0.13f * f * alfa);
  }
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
void gfx_furo(GfxRect r) {
  gfx_furo_raio(r, 0.0f);
}

void gfx_furo_raio(GfxRect r, float raio) {
  glDisable(GL_BLEND);
  gfx_rect(r, 0, GFX_COR, 0, 0, 0, raio, 0, 0, 0, 0);
  glEnable(GL_BLEND);
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
  glBindFramebuffer(GL_FRAMEBUFFER, snapFbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, snapTex, 0);
  GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  if (st != GL_FRAMEBUFFER_COMPLETE) {
    printf("snapshot indisponivel (fbo 0x%x) — seguindo sem ele\n", st);
    glDeleteFramebuffers(1, &snapFbo); glDeleteTextures(1, &snapTex);
    snapFbo = snapTex = 0;
    return 0;
  }
  return 1;
}

int gfx_snap_ok(void) { return snapFbo != 0; }

void gfx_snap_comecar(void) {
  if (!snapFbo || snapAtivo) return;
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
  gfx_rect(r, snapTex, GFX_SNAP, 0, 0.0f, 1.0f, 0.0f, 0, 0, 0, 1.0f);
}

void gfx_snap_encerrar(void) {
  if (snapFbo) glDeleteFramebuffers(1, &snapFbo);
  if (snapTex) glDeleteTextures(1, &snapTex);
  snapFbo = snapTex = 0;
}

// --- icones ------------------------------------------------------------------
static char dirIcones[512];

void gfx_icones_dir(const char *dirArte) {
  snprintf(dirIcones, sizeof dirIcones, "%s/icones", dirArte ? dirArte : ".");
}

void gfx_icone(GfxRect r, const char *nome, float cr, float cg, float cb, float ca) {
  char cam[600];
  GLuint t;
  if (!nome || !nome[0] || !dirIcones[0]) return;
  // Caminho ABSOLUTO: o diretorio de trabalho do app nao e a pasta da arte, e
  // com caminho relativo o IMG_Load falha em silencio e o icone some sem erro.
  // Mesma armadilha ja documentada em extras_caminho_marca.
  snprintf(cam, sizeof cam, "%s/%s.png", dirIcones, nome);
  // Pede pela largura de desenho: um icone de 38px nao precisa dos 128 do
  // arquivo, e o teto por uso e o que mantem o cache fora do vermelho.
  t = tex_obter_larg(cam, r.w);
  if (!t) return;
  gfx_tex_aspect_atual = 0.0f;   // o arquivo ja e quadrado
  gfx_rect(r, t, GFX_MARCA, 0, 0, 0, 0.0f, cr, cg, cb, ca);
}

void gfx_recorte(float x, float y, float w, float h) {
  GFX_OUTRO_INI();
  if (w <= 0.0f || h <= 0.0f) { glEnable(GL_SCISSOR_TEST); glScissor(0, 0, 0, 0);
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
  int yy = (int)((NV_TELA_H - (y + h)) * ey);
  glEnable(GL_SCISSOR_TEST);
  glScissor((int)(x * ex), yy, (int)(w * ex), (int)(h * ey));
  GFX_OUTRO_FIM();
}
void gfx_sem_recorte(void) { glDisable(GL_SCISSOR_TEST); }

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
  glBindFramebuffer(GL_FRAMEBUFFER, borFbo[i]);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, borTex[i], 0);
  GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
  GFX_OUTRO_INI();
  glDisable(GL_BLEND);
  glViewport(0, 0, borW, borH);

  glBindFramebuffer(GL_FRAMEBUFFER, borFbo[a0]);
  gfx_tex_aspect_atual = texAspecto;
  gfx_rect(cheio, tex, GFX_SNAP, 0, 0, 0, 0.0f, 0, 0, 0, 1.0f);
  gfx_tex_aspect_atual = 0.0f;

  // O passo e maior que um texel: com passo de um texel o desfoque mal cobre
  // 4px do alvo, que esticado 4x ainda deixa a estrutura da imagem visivel.
  float px = NV_BLUR_PASSO / (float)borW, py = NV_BLUR_PASSO / (float)borH;
  glBindFramebuffer(GL_FRAMEBUFFER, borFbo[a1]);
  gfx_rect(cheio, borTex[a0], GFX_BLUR, 0, px, 0.0f, 0.0f, 0, 0, 0, 1.0f);
  glBindFramebuffer(GL_FRAMEBUFFER, borFbo[a0]);
  gfx_rect(cheio, borTex[a1], GFX_BLUR, 0, 0.0f, py, 0.0f, 0, 0, 0, 1.0f);

  glEnable(GL_BLEND);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, telaW, telaH);
  GFX_OUTRO_FIM();
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
    GFX_OUTRO_INI();
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glViewport(0, 0, NV_DESF_W, NV_DESF_H);
    gfx_tex_aspect_atual = 0.0f;   // a arte INTEIRA, esticada; o card recorta depois
    // Passada 1: horizontal, lendo a arte original e ja reduzindo. Escrever
    // num FBO inverte o eixo y (ver GFX_SNAP); a passada 2 inverte de novo, e
    // o resultado sai de pe para o GFX_CARD, como uma arte comum.
    glBindFramebuffer(GL_FRAMEBUFFER, desfFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, desfTmp, 0);
    gfx_rect(cheio, src, GFX_BLUR, 0, NV_DESF_PASSO / (float)NV_DESF_W, 0.0f,
             0.0f, 0, 0, 0, 1.0f);
    // Passada 2: vertical, do intermediario para a textura guardada.
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst, 0);
    gfx_rect(cheio, desfTmp, GFX_BLUR, 0, 0.0f, NV_DESF_PASSO / (float)NV_DESF_H,
             0.0f, 0, 0, 0, 1.0f);
    gfx_tex_aspect_atual = aspAnt;
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)fboAnt);
    glViewport(vp[0], vp[1], vp[2], vp[3]);
    if (mistura) glEnable(GL_BLEND);
    if (tesoura) glEnable(GL_SCISSOR_TEST);
    GFX_OUTRO_FIM();
  }
  desf[vago].src = src;
  desf[vago].chave = h;
  desf[vago].uso = desfRelogio;
  desfGeradosQuadro++;
  return desf[vago].tex;
}

static void desfEncerrar(void) {
  for (int i = 0; i < NV_DESF_N; i++) {
    if (desf[i].tex) { gfx_tex_esquecer(desf[i].tex); glDeleteTextures(1, &desf[i].tex); }
    desf[i].tex = desf[i].src = 0; desf[i].chave = desf[i].uso = 0;
  }
  if (desfFbo) { glDeleteFramebuffers(1, &desfFbo); desfFbo = 0; }
  if (desfTmp) { gfx_tex_esquecer(desfTmp); glDeleteTextures(1, &desfTmp); desfTmp = 0; }
}
