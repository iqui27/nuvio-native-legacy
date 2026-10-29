// CH+ SEGURADO x CH+ TOCADO (central.h). Aritmetica pura, sem SDL: o teste
// (tests/chsegura.sh) alimenta tempos a mao.
//
// O PROBLEMA. Um toque curto no CH+ ja tem dono (Salvos/ilha no LG, Samsung e
// Android; zap com canal na tela) e segurar passa a abrir a Central de
// controle. So da para saber qual dos dois DEPOIS: o toque curto espera a
// tecla subir (ou o silencio, sem KEYUP) e e reentregue com atraso.
//
// O QUE CADA CONTROLE MANDA (o que se sabe, nao o que se supoe):
//  - tecla segurada chega como KEYDOWNs SEPARADOS, repeat=0 (app.c, #11);
//  - Android: KEYUP no soltar (dispatchKeyEvent repassa ACTION_UP);
//  - .tpk: o host .NET repassa Down e Up (Program.cs, Tecla);
//  - .wgt: tizen-shell.html repassa keydown e keyup do 427 como F7;
//  - LG: o BACK chega com KEYDOWN e KEYUP "quase juntos" mesmo segurado (main.c).
//    Se o CH+ fizer igual, o KEYUP imediato nao pode valer como soltar.
//
// A REGRA:
//  - segurar = CHS_SEGURAR_MS desde o primeiro KEYDOWN sem soltar, contado
//    pelos KEYDOWNs repetidos OU, onde ja se viu um KEYUP de verdade nesta
//    sessao, pelo relogio;
//  - KEYUP a menos de CHS_FANTASMA_MS do KEYDOWN, antes de qualquer KEYUP de
//    verdade, e o par "quase junto" do LG: ignorado. Sem KEYUP valido o
//    toque curto sai pelo silencio (CHS_SILENCIO_MS sem KEYDOWN novo);
//  - KEYDOWN que chega ate CHS_EMENDA_MS depois de um KEYUP e a mesma tecla
//    (repeticao em pares Up/Down do X11/EFL), nao toque novo.
#ifndef NV_CHSEGURA_H
#define NV_CHSEGURA_H

#define CHS_SEGURAR_MS  600u
#define CHS_ATRASO_MS   300u   // dead zone: no progress drawn, a click shows nothing
#define CHS_EMENDA_MS    90u
#define CHS_FANTASMA_MS  35u
#define CHS_SILENCIO_MS 450u
#define CHS_ESQUECER_MS 5000u   // segurado sem KEYUP nem KEYDOWN: solta

enum { CHS_NADA = 0, CHS_CURTO = 1, CHS_LONGO = 2 };
enum { CHS_LIVRE = 0, CHS_APERTADO = 1, CHS_SEGURADO = 2 };

typedef struct {
  int estado;        // CHS_LIVRE / _APERTADO / _SEGURADO
  unsigned t0;       // primeiro KEYDOWN do gesto
  unsigned tUlt;     // ultimo KEYDOWN
  unsigned tSolta;   // KEYUP valido
  int solto;         // houve KEYUP valido depois do ultimo KEYDOWN
  int viuSolta;      // sessao ja viu um KEYUP de verdade desta tecla
} ChSegura;

// Cada um devolve CHS_NADA, CHS_CURTO (reentregar o toque) ou CHS_LONGO (abrir).
int chs_desce(ChSegura *s, unsigned t);
int chs_sobe(ChSegura *s, unsigned t);
int chs_quadro(ChSegura *s, unsigned t);
// 1 enquanto o gesto esta em curso: o CH+ e engolido.
int chs_ocupado(const ChSegura *s);

#endif
