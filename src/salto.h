// Regra do AVANCO SEGURADO (setas esquerda/direita do player), sem SDL nem
// estado global para poder ser testada com um fluxo de teclas simulado.
//
// O QUE ESTAVA ERRADO: o passo crescia por REPETICAO da tecla. O controle da
// C9 repete a cada ~100 ms, entao em 0,6 s cada repeticao ja valia 30 s e em
// 2,6 s valia 120 s: ~20 minutos por segundo. "As vezes voce quer pular so um
// pedacinho e ele ja voa pro final." E o ritmo de repeticao muda de TV e de
// controle (Samsung, Android), entao o mesmo gesto dava distancias diferentes.
//
// AGORA O RELOGIO MANDA, nao a contagem de teclas:
//  - o degrau vem do TEMPO desde a primeira tecla da rajada;
//  - no maximo um passo a cada SALTO_INTERVALO_MS; repeticoes extras entre
//    dois passos sao IGNORADAS (nao acumulam). 100 ms ou 40 ms de repeticao
//    dao a mesma distancia.
// Toque unico continua sendo exatamente 10 s.
#ifndef NUVIO_SALTO_H
#define NUVIO_SALTO_H

#define SALTO_SEG           10.0f
#define SALTO_INTERVALO_MS  300u     // ~3 passos por segundo, qualquer repeticao
#define SALTO_T1_MS        1500u     // ate aqui: 10 s por passo (o toque e a primeira volta)
// RAMPA PROPORCIONAL A DURACAO (#340). Depois de T1 a velocidade (segundos de
// midia por segundo segurado) sobe linear de V0 (o ritmo de 10 s a cada 300 ms)
// ate DURACAO/SALTO_VMAX_DIV, em SALTO_RAMPA_MS. Atravessar o arquivo inteiro
// leva ~9-11 s para 20 min, 2 h ou 3 h; antes eram 120 s por passo no teto, ou
// seja, ~7 min de aperto num filme de 2 h.
#define SALTO_RAMPA_MS     3000u
#define SALTO_VMAX_DIV     7.0f
#define SALTO_V0           (SALTO_SEG * 1000.0f / SALTO_INTERVALO_MS)

// Passo (segundos) para quem esta com a tecla ha `heldMs` numa midia de
// `duracaoSeg`. Funcao pura: o player so soma e o clamp [0,dur] e dele.
static inline float salto_passo(unsigned heldMs, float duracaoSeg) {
  float vmax = duracaoSeg > 0.0f ? duracaoSeg / SALTO_VMAX_DIV : 0.0f;
  if (heldMs < SALTO_T1_MS || vmax <= SALTO_V0) return SALTO_SEG;
  float f = (float)(heldMs - SALTO_T1_MS) / (float)SALTO_RAMPA_MS;
  if (f > 1.0f) f = 1.0f;
  float v = SALTO_V0 + (vmax - SALTO_V0) * f;
  float p = v * SALTO_INTERVALO_MS / 1000.0f;
  return p < SALTO_SEG ? SALTO_SEG : p;
}

// ESTADO DA RAJADA e a regra de quem e toque e quem e tecla segurada.
//
// TOQUE SOLTO nunca e perdido: cada KEYDOWN novo aplica 10 s na hora e NAO
// inicia a rampa — cinco toques rapidos sao +50 s. So a REPETICAO de tecla
// segurada passa pelo limite de SALTO_INTERVALO_MS e pela rampa de tempo.
//
// Como saber que e repeticao, se cada plataforma entrega de um jeito: o
// teclado do SDL marca key.repeat; o firmware da TV manda a tecla segurada como
// KEYDOWNs SEPARADOS com repeat=0 (mesma observacao de app.c, issue #11), e a
// casca Tizen ainda despacha um par keydown+keyup por tecla, entao "descida sem
// subida" nao serve de criterio. Sobra o RELOGIO: repeticao de controle vem a
// ~100 ms (SDL de teclado, ~30 ms); o toque mais rapido de uma mao, ~150 ms.
// Logo, descida a menos de SALTO_REP_MS da anterior e repeticao. Abaixo de
// SALTO_REP_MIN_MS (dois eventos do mesmo quadro) nao existe mao nem controle:
// sao dois toques entregues juntos, e valem os dois.
#define SALTO_REP_MS      130u
#define SALTO_REP_MIN_MS   20u

typedef struct { unsigned inicio, ultimaTecla, ultimoPasso; int rep; } SaltoEst;

// `novo`: rajada nova (primeira tecla, ou a direcao mudou). `flagRepeat`:
// key.repeat do SDL. Devolve o passo em segundos (0 = ignorar a repeticao).
static inline float salto_tecla(SaltoEst *st, int novo, int flagRepeat,
                                unsigned agoraMs, float duracaoSeg) {
  unsigned gap = agoraMs - st->ultimaTecla;
  int rep = !novo && (flagRepeat || (gap >= SALTO_REP_MIN_MS && gap <= SALTO_REP_MS));
  st->ultimaTecla = agoraMs;
  st->rep = rep;
  if (!rep) {                       // toque: 10 s agora, rampa recomeca
    st->inicio = agoraMs; st->ultimoPasso = agoraMs;
    return SALTO_SEG;
  }
  if (agoraMs - st->ultimoPasso < SALTO_INTERVALO_MS) return 0.0f;
  st->ultimoPasso = agoraMs;
  return salto_passo(agoraMs - st->inicio, duracaoSeg);
}


// QUANDO O AVANCO TERMINA (#235). O avanco ja e "escolher e depois confirmar":
// video pausado, so a barra anda, UMA busca no fim, OK confirma na hora. O que
// faltava e o tempo de espera: 420 ms nao davam folga para olhar a miniatura
// do Seekr e acertar o ponto ("1 s de atraso antes de aplicar"). Com o Seekr
// ligado a espera e 1 s; sem miniatura nao ha o que olhar e ficam os 420 ms,
// para o filme voltar a tocar logo depois de um toque so.
#define SALTO_FIM_MS        420u
#define SALTO_FIM_SEEKR_MS 1000u
static inline unsigned salto_fim_ms(int seekrLigado) {
  return seekrLigado ? SALTO_FIM_SEEKR_MS : SALTO_FIM_MS;
}


// AVANCO CONTINUO (2.0.3). O passo discreto (10 s ... 5 min a cada 300 ms) fazia
// a posicao andar aos pulos: a barra (mola) parava entre dois passos e o tempo
// escrito e a miniatura do Seekr, que liam a posicao crua, saltavam o passo
// inteiro de uma vez. A velocidade media nao muda (V0 = 10 s / 300 ms, e a
// rampa de salto_passo), mas a REPETICAO da tecla agora so mantem o avanco vivo
// e a posicao anda um pouco por quadro: salto_quadro().
// O toque (rep == 0) continua somando 10 s na hora: salto_tecla devolve o passo
// e o player so o aplica quando `st->rep == 0`.
#define SALTO_VIVO_MS 260u   // sem tecla ha mais que isso, o avanco para

// Velocidade (s de midia por s real) de quem esta com a tecla ha `heldMs`.
static inline float salto_vel(unsigned heldMs, float duracaoSeg) {
  return salto_passo(heldMs, duracaoSeg) * 1000.0f / SALTO_INTERVALO_MS;
}

// Quanto a posicao anda neste quadro (segundos, ja com o sinal de `dir`).
static inline float salto_quadro(const SaltoEst *st, int dir, unsigned agoraMs,
                                 float dt, float duracaoSeg) {
  if (!st->rep || agoraMs - st->ultimaTecla > SALTO_VIVO_MS || dt <= 0.0f) return 0.0f;
  if (dt > 0.05f) dt = 0.05f;
  return dir * salto_vel(agoraMs - st->inicio, duracaoSeg) * dt;
}

#endif
