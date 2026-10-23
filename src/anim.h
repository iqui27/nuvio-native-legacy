// Animacao por mola criticamente amortecida, por propriedade.
//
// Por que mola e nao easing de duracao fixa: no D-pad o alvo muda no meio do
// movimento (usuario segura a tecla), e uma tween com duracao precisa ser
// reiniciada a cada troca — o que produz o "engasgo" tipico. A mola so persegue
// o alvo novo a partir da posicao atual, sem descontinuidade.
#ifndef NV_ANIM_H
#define NV_ANIM_H
#include <math.h>

// POLITICA DE ANIMACOES REDUZIDAS, num lugar so.
//
// Metade das telas (busca, biblioteca, detalhe, menu, player, guia, avisos...)
// chamava anim_mola sem nunca perguntar pelo ajuste — ligar "Reduzidas" nao
// mudava nada nelas, e o ajuste de acessibilidade valia so para a home. Aqui a
// primitiva mesma obedece: com a bandeira ligada, mola e rampa vao DIRETO ao
// alvo. Quem ja testava o ajuste na chamada continua funcionando igual.
//
// Quem liga e o roteador (app_atualizar), uma vez por quadro, a partir de
// ajustes_animacoes_reduzidas(). E `weak` para o cabecalho continuar sem .c:
// os testes linkam subconjuntos de src/ e nao pode faltar o simbolo em nenhum.
__attribute__((weak)) int anim_politica_reduzida = 0;

// Independente de framerate: usa exp(-k*dt), nao um passo fixo por quadro.
static inline float anim_mola(float atual, float alvo, float dt, float rigidez) {
  if (anim_politica_reduzida) return alvo;
  return atual + (alvo - atual) * (1.0f - expf(-rigidez * dt));
}
static inline float anim_clamp(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}
static inline float anim_mistura(float a, float b, float t) { return a + (b - a) * t; }

// MOLA DE SEGUNDA ORDEM, CRITICAMENTE AMORTECIDA (posicao + velocidade).
//
// POR QUE ELA EXISTE, ao lado da de cima. MEDIDO na referencia (video do
// aparelho, quadros com carimbo de tempo, tecla DIREITA na fileira "Continuar
// assistindo"): o anel de foco salta para o card novo em UM quadro e e a
// FILEIRA que desliza um passo de card por baixo dele. O deslize chega a
// metade do caminho em ~180 ms, a 83% em ~215 ms, e daí decai como uma
// exponencial de k ~= 12,5 /s ate assentar por volta de 450 ms.
//
// Ou seja: comeca DEVAGAR, acelera, e termina com cauda exponencial. A
// `anim_mola` de primeira ordem faz o contrario — parte com velocidade MAXIMA
// e so desacelera. Ajustada para acertar o meio (k=4,8) ela ainda estaria em
// 89% aos 470 ms, onde a referencia ja esta em 99%; ajustada para acertar a
// cauda (k=12,5) ela cruza a metade aos 55 ms em vez de 180. Nenhum k unico
// serve, porque a forma e outra.
//
// A criticamente amortecida tem exatamente essa forma: p(t) = 1-(1+wt)e^-wt.
// Velocidade inicial zero (partida macia), cauda e^-wt (o k medido) e ZERO
// overshoot — nao "passa do ponto e volta", que e o defeito que a mola crua
// tem em bloco grande. E, como a de primeira ordem, ela PERSEGUE o alvo: se a
// tecla fica presa e o alvo muda no meio do voo, nao ha o que reiniciar.
//
// `v` e a velocidade, guardada pelo chamador junto da posicao. `w` e a
// frequencia em rad/s e vale o k da cauda medida.
static inline float anim_mola2(float *v, float atual, float alvo, float dt, float w) {
  // Um quadro perdido (aba escondida, decode longo) nao pode virar um passo
  // gigante. A forma fechada evita o overshoot do Euler semi-implicito e
  // continua retargetavel quando o D-pad muda o alvo durante o movimento.
  if (anim_politica_reduzida) { *v = 0.0f; return alvo; }
  if (dt <= 0.0f || w <= 0.0f) return atual;
  if (dt > 0.05f) dt = 0.05f;
  if ((alvo - atual) * (*v) < 0.0f) *v = 0.0f;
  float x = atual - alvo;
  float e = expf(-w * dt);
  float c = (*v + w * x) * dt;
  float novo = alvo + (x + c) * e;
  float nv = (*v - w * c) * e;
  if ((alvo > atual && novo > alvo) || (alvo < atual && novo < alvo)) {
    novo = alvo;
    nv = 0.0f;
  }
  *v = nv;
  return novo;
}

// Reduced motion e uma politica, nao um detalhe de cada tela.
static inline float anim_reduzida(float atual, float alvo, int reduzida) {
  return reduzida ? alvo : atual;
}
static inline float anim_mola2_reduzida(float *v, float atual, float alvo,
                                        float dt, float w, int reduzida) {
  if (reduzida) { *v = 0.0f; return alvo; }
  return anim_mola2(v, atual, alvo, dt, w);
}

// Aceleracao e desaceleracao simetricas, para animacao com relogio proprio.
static inline float anim_suave(float t) {
  t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
  return t * t * (3.0f - 2.0f * t);
}

// Parte rapido e assenta devagar, sem passar do alvo (cubica de saida). Para
// deslocamentos com relogio proprio que tem de chegar num tempo certo, como a
// troca deslizada do destaque: sem repique, que num carrossel le como erro.
static inline float anim_saida(float t) {
  t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
  t = 1.0f - t;
  return 1.0f - t * t * t;
}

// Progresso 0..1 com RELOGIO PROPRIO: anda `dt` segundos em direcao a `alvo`
// gastando `ms` no percurso inteiro. Usada onde o tempo precisa bater com uma
// medida (o veu do menu), e nao apenas "assentar rapido".
static inline float anim_rampa(float p, float alvo, float dt, float ms) {
  if (anim_politica_reduzida) return alvo;
  float passo = dt * (1000.0f / ms);
  if (alvo > p) { p += passo; if (p > alvo) p = alvo; }
  else          { p -= passo; if (p < alvo) p = alvo; }
  return p;
}

// PILULA QUE ABRE: dois canais ORDENADOS, largura e rotulo. ABRINDO, a
// largura vai primeiro e o rotulo so entra com ela pronta; FECHANDO, o
// rotulo sai primeiro e a largura encolhe depois. O rotulo e desenhado
// INTEIRO — nunca cortado pela largura em crescimento, que fazia o texto
// chegar palavra por palavra, como digitado (detail.c, "Assistir do comeco").
// *rotulo e 0 enquanto *largura nao chegou a 1: e isso que impede o corte.
static inline void anim_expande(float *largura, float *rotulo, float alvo,
                                float dt, float msLargura, float msRotulo) {
  if (anim_politica_reduzida) { *largura = alvo; *rotulo = alvo; return; }
  if (alvo > 0.5f) {
    if (*largura < 1.0f)
      *largura = anim_rampa(*largura, 1.0f, dt, msLargura);
    else
      *rotulo = anim_rampa(*rotulo, 1.0f, dt, msRotulo);
  } else {
    if (*rotulo > 0.0f)
      *rotulo = anim_rampa(*rotulo, 0.0f, dt, msRotulo);
    else
      *largura = anim_rampa(*largura, 0.0f, dt, msLargura);
  }
}

// RETORNO DE BORDA: a seta que bate no fim (ultimo cartao, topo da pagina) nao
// move o foco, e sem resposta nenhuma o controle parece ter falhado. Aqui um
// deslocamento curto na direcao da tecla, que volta a zero com mola.
//
// DUAS FASES, porque a anim_mola2 zera a velocidade que se afasta do alvo e
// um "impulso" puro nao sairia do lugar: a ida persegue `alvo` (= amplitude)
// com a mola de 1a ordem, que parte rapido como uma batida; ao chegar perto
// o alvo vira zero e a volta e a criticamente amortecida, sem overshoot.
//
// Parado (x == 0 e alvo == 0) o passo sai na primeira comparacao. Com a
// politica reduzida nao ha movimento nenhum: bater nao arma e o passo zera.
typedef struct { float x, v, alvo; } AnimBorda;
#define ANIM_BORDA_IDA   32.0f   // rigidez da ida: ~90% em 70 ms
#define ANIM_BORDA_VOLTA 11.5f   // w da volta, o mesmo NV_MOLA2_SCROLL do deslize
static inline void anim_borda_bater(AnimBorda *b, float amplitude) {
  if (anim_politica_reduzida) return;
  b->alvo = amplitude;
  b->v = 0.0f;
}
// 1 enquanto ha deslocamento a desenhar.
static inline int anim_borda_passo(AnimBorda *b, float dt, int reduzida) {
  if (b->x == 0.0f && b->alvo == 0.0f) return 0;
  if (reduzida || anim_politica_reduzida) { b->x = b->v = b->alvo = 0.0f; return 0; }
  if (b->alvo != 0.0f) {
    b->x = b->x + (b->alvo - b->x) * (1.0f - expf(-ANIM_BORDA_IDA * dt));
    if (fabsf(b->x) >= fabsf(b->alvo) * 0.9f) { b->alvo = 0.0f; b->v = 0.0f; }
    return 1;
  }
  b->x = anim_mola2(&b->v, b->x, 0.0f, dt, ANIM_BORDA_VOLTA);
  if (fabsf(b->x) < 0.25f) { b->x = b->v = 0.0f; return 0; }
  return 1;
}

#endif
