# ASS com estilo (libass) e PGS: peso e o ajuste "Legenda ASS com estilo"

Frente da pesquisa de mercado (seção 3). Estado em `integracao/2.1`.

## O que já existia (não foi refeito)

- **ASS com estilo**: `src/assrender.c` usa o libass (`NV_ASS_LIBASS`) no webOS (`tools/arm.sh`),
  no `.tpk` da Samsung (`tools/tpk.sh`) e no WASM (`tools/tizen.sh`): fio próprio, cópia dos
  bitmaps para a CPU, fontes embutidas do MKV, HarfBuzz/FriBidi, render no pixel do alvo em 4K,
  teto de 16 MiB por quadro e 32 MiB no total.
- **PGS**: `src/pgs.c` (decodificador próprio), `src/mkvass.c` (extração do MKV),
  `src/imgsub.c` (fila de quadros com tempo) e o desenho em `src/player.c`.
- Estilo da legenda nos Ajustes, atraso, sincronia automática, "preservar o estilo do autor".

O que faltava era a medida do peso e uma saída para a TV que não aguenta uma cena pesada.

## Medida

`bash tests/ass_peso_bench.sh` (precisa do libass). Mede `ass_render_frame` + cópia dos bitmaps,
como o fio do assrender, e o `pgs_decodificar`. **Máquina: Xeon x86-64 de servidor, libass 0.17.3
compilado sem assembly** (como no ARM de 32 bits das TVs antigas, para o qual o libass não tem
rotinas otimizadas). **Não é medida em TV.** O número absoluto só vale para esta máquina; o que
orienta a decisão é a razão entre cenas e entre resoluções.

| cena | render | médio ms | p95 ms | KiB de alfa (pico) |
|---|---|---:|---:|---:|
| diálogo | 1080p 1:1 | 0,06 | 0,04 | 872 |
| karaokê (`\kf`, blur) | 1080p 1:1 | 0,23 | 0,23 | 1 478 |
| gradiente de 120 `\clip` | 1080p 1:1 | 3,35 | 3,64 | 745 |
| placa de 40 camadas com blur | 4K 1:1 | 212,80 | 239,76 | 164 644 |
| placa de 40 camadas com blur | 1080p 1:1 | 97,87 | 110,45 | 42 616 |
| placa de 40 camadas com blur | 1080p /2 | 41,58 | 46,71 | 11 930 |
| placa de 40 camadas com blur | 1080p /3 | 27,52 | 32,37 | 5 804 |

| PGS | médio ms | p95 ms | RGBA KiB |
|---|---:|---:|---:|
| duas linhas 1400x160 | 0,19 | 0,21 | 875 |
| placa 1920x540 | 0,87 | 0,94 | 4 050 |
| tela cheia 1920x1080 | 1,76 | 1,87 | 8 100 |

Leitura:

- Diálogo, karaokê e gradiente custam de 400 a 1 600 vezes menos que a placa. Mesmo com uma CPU
  de TV dezenas de vezes mais lenta, o problema não está neles.
- A placa tipografada com blur em muitas camadas é o caso que pesa: o tempo e a memória crescem
  com a área. Em 1/3 da resolução o tempo cai 3,6 vezes e a memória 7,3 vezes.
- No pico, a mesma placa passa do teto de 16 MiB por quadro; o `assrender` não publica um quadro acima do teto, então a legenda para de atualizar ali.
- PGS é barato de decodificar (menos de 2 ms a tela cheia). O custo dele é a textura de até
  8 MiB, que já tem fila e teto no `imgsub`. Nada a mudar sem medida em TV.

## O que entrou

Ajuste **Legenda ASS com estilo** (Idiomas e legendas, avançado; local da TV):

| valor | o que faz |
|---|---|
| Automático (padrão) | Mantém o estilo do autor. Se o render médio passa de 45 ms ou de 3 MiB por quadro em duas janelas de 12 renders seguidas, ou se um quadro passa do teto de cópia, o libass passa a renderizar em 1/2 e depois 1/3 da resolução e o quadro é esticado na tela. Volta um degrau por vez depois de 8 janelas leves. |
| Sempre completo | O comportamento anterior: nunca reduz. |
| Texto simples | O ASS do autor não passa pelo libass: sai como texto do app, com o estilo dos Ajustes. |

- Regra pura em `src/asspeso.c`, teste em `tests/asspeso.sh`.
- No `assrender.c` de verdade com libass: `tests/ass_peso.sh` (pula sem libass >= 0.17).
- SRT/VTT desenhados pelo libass (texto RTL) ficam sempre 1:1.
- Em diálogo comum o Automático não muda nada: o limite fica longe.

## O que não foi verificado

- Nenhuma TV física. Os limites (45 ms, 3 MiB) saíram da medida acima e do orçamento de um
  quadro; precisam ser conferidos numa TV fraca com um anime de placas pesadas. O log
  `[ass] peso: render em 1/N` diz quando a redução entrou.
- Os relatos #772, #786 e #394 continuam sem reprodução; este ajuste não afirma resolvê-los.
- Os testes de libass que só compilam no macOS (`ass_pisca`, `ass_area`, `ass_fontes`,
  `subtitle_fallback_bold`) não rodaram no Linux.

## O que falta

- Medir em webOS e Tizen reais e ajustar os limites.
- Aparência da placa em 1/3 numa TV 4K (nitidez), com captura.
- Custo de envio de textura do PGS a tela cheia nas TVs fracas.
