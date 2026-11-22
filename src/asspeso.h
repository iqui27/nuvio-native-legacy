// Peso da legenda ASS com estilo (libass) em TV fraca: regra pura, sem libass,
// sem GL e sem relogio. O fio do assrender entrega o custo de cada quadro e
// recebe de volta por quanto dividir a resolucao do render.
//
// Por que resolucao: medido em tests/ass_peso_bench.sh (docs/plans/ass-pgs-peso.md),
// a cena que pesa e a placa tipografada com blur em dezenas de camadas, e nela
// o tempo cai ~2x e os bytes por quadro caem ~7x quando o render vai a 1/3. O
// estilo do autor (posicao, cor, fonte, movimento) fica; so a nitidez cai, e
// volta sozinha quando a cena pesada passa.
#ifndef NV_ASSPESO_H
#define NV_ASSPESO_H
#include <stddef.h>

// Valores do ajuste "Legenda ASS com estilo" (append-only, na ordem da tela).
enum { ASSPESO_AUTO = 0, ASSPESO_COMPLETO = 1, ASSPESO_SIMPLES = 2 };

#define ASSPESO_NIVEL_MAX 2       // 0 = 1:1, 1 = metade, 2 = um terco
#define ASSPESO_JANELA 12         // renders por janela
#define ASSPESO_RUIM_MS 45.0      // media por render acima disto: janela ruim
#define ASSPESO_RUIM_BYTES (3u << 20)   // alfa por quadro (x4 em RGBA na textura)
#define ASSPESO_RUINS 2           // janelas ruins seguidas para descer
#define ASSPESO_BOAS 8            // janelas boas seguidas para subir

typedef struct {
  int nivel, n, ruins, boas;
  double somaMs, somaBytes;
} AssPeso;

void asspeso_zerar(AssPeso *p);
// Um render do libass: `ms` de ass_render_frame + copia, `bytes` de alfa do
// quadro (0 quando o quadro saiu igual ao anterior). `estouro` = o quadro
// passou do teto de copia e NAO foi publicado: desce na hora. Fora do modo
// Automatico o nivel fica em 0. Devolve 1 quando o nivel mudou.
int asspeso_amostra(AssPeso *p, int modo, double ms, size_t bytes, int estouro);
// Divisor da resolucao do render para o nivel atual: 1, 2 ou 3.
int asspeso_divisor(const AssPeso *p);
// 1 = o libass nao desenha o ASS do autor; a legenda sai como texto do app.
int asspeso_sem_estilo(int modo);
#endif
