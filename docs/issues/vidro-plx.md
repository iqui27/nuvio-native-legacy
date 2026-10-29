# Vidro e barra do topo do PlxNative (2.0.5)

Dono: "vamos pegar do plx o glass", "a top bar tem que ser menor e mais premium",
"o glass UI no frost ta tendo aberracao de cor, ficando meio verde".

Fonte: PlxNative (github.com/GLinnik21/plx-native, commit 9d2777fe),
GPL-3.0-or-later, (c) 2026 Gleb Linnik e contribuidores. Credito em
`docs/CREDITS.md` e no comentario de cabecalho do codigo adaptado.

## O que veio de onde

| Nosso | Deles |
|---|---|
| `src/gfx.c` captura do backdrop (glCopyTexSubImage2D da regiao atras das superficies, alcance 68 + folga 20) | `rust-modules/gfx/src/gfx.rs` (BLUR_REACH, BLUR_MARGIN, blur_region_union) |
| `src/gfx.c` 2 reducoes pela metade, Kawase de 4 amostras em 1/4 (0,35 e 0,75 texel), subida a 1/2 (1,25 texel): 5 passadas | `rust-modules/gfx/src/shaders/fs_blur.frag`, `gfx.rs` |
| `src/gfx.c` corpo do `GFX_VIDRO` (lente do bisel, luz da borda, aro, especular, solve de densidade sobre scrim neutro) | `rust-modules/gfx/src/shaders/fs_glass.frag` |
| `src/gfx.c` tile de dither TPDF 256x256 (lowbias32), REPEAT | `rust-modules/gfx/src/shaders/dither.glsl`, `noise_tex` em `gfx.rs` |
| `src/gfx.c` scrim da barra .20 em cima / .36 embaixo, teto .72/.82, aro .14 / luz do aro .28 | `rust-modules/ui/src/theme.rs` (TAB_GLASS_TOP/BOT, TAB_TRACK_A_TOP/BOT, GLASS_RIM, GLASS_RIM_LIGHT) |
| `src/gfx.c` uma captura por quadro dividida por barra, perfil, ilha e paineis | `gfx::blur_region_union`; `widgets_glass_budget_tests.rs` |
| `src/menu.c` barra do topo: avatar em circulo sozinho a esquerda, trilha de vidro abracando as pilulas, Busca quadrada no fim, ilha do relogio na mesma linha de centro | `rust-modules/ui/src/widgets.rs` (`draw_tab_row`, `TabStrip`) |
| `src/menu.c` medidas a 0,9 das deles em 1920 (pilula 54, trilha 7,2, vao 14,4, icone 28,8) | `ui/src/widgets.rs` (TAB_PILL_H, TAB_PILL_PAD, TAB_GAP, TAB_TRACK_PAD, TAB_ICON_D) |
| `src/menu.c` capsulas de selecao e foco que viajam em molas criticas (x e largura em sqrt(240), alfa em sqrt(300)); pousam em vez de deslizar quando invisiveis; tinta do rotulo segue a cobertura | `widgets.rs` (`Capsule`), `widgets_tab_capsule_motion_tests.rs` |

Ficou nosso: alvos em coordenada de tela, reuso entre quadros pelo hash dos
desenhos (`vidroDesenho`), caminho chapado (vidro que cruza vidro, video vivo,
efeitos minimos). Idiomas longos apertam o vao, depois o respiro, antes de
cortar rotulo.

Ajustes: "Opacidade do vidro" escala o scrim, "Vidro fosco" liga a captura
desfocada (desligado = chapado), "Contorno do vidro" liga o aro.

GLES2: texturas RGB/RGBA `GL_UNSIGNED_BYTE`; alvos NPOT com `CLAMP_TO_EDGE` e
sem mipmap; so o tile de ruido (256, POT) usa `REPEAT`; shaders com precisao
explicita.

## O verde

O fosco antigo lia um assado 320x180 da PALETA do titulo, nao o que esta atras
do painel; titulo verde = vidro verde sobre cinza. Agora a fonte e a propria
tela e o scrim e cinza neutro (R = G = B).

`bash tests/vidro_neutro.sh` (titulo verde, fundo cinza e branco, |G-(R+B)/2| <= 2):

| | antes (2e4d2434) | depois |
|---|---|---|
| painel sobre cinza | 36.8,43.1,38.1  +5.65 FALHA | 63.5,63.5,63.5  +0.00 |
| folha sobre cinza | 21.4,27.0,24.8  +3.87 FALHA | 52.2,52.2,52.2  +0.00 |
| painel sobre branco | 36.8,43.1,38.1  +5.65 FALHA | 96.5,96.5,96.5  +0.00 |
| desfoca o backdrop real | nao (27.0 nos dois lados) | sim (27.2/42.6, 96.7/112.1) |

## Orcamento (passadas e ms)

`bash tests/vidro_orcamento.sh`: barra, perfil, ilha e um painel, pagina rolando
20 quadros, depois parada 5 quadros. Mac, glFinish; so informa, a GPU da TV e outra.

| | antes (9afd9aea) | depois |
|---|---|---|
| capturas por quadro rolando | 4 | 1 |
| passadas de desfoque por quadro rolando | 20 | 5 |
| pagina parada (5 quadros) | 20 capturas, 100 passadas | 0, 0 |
| ms por quadro rolando | 1.26 | 0.31 |

No 2e4d2434 o vidro nao desfocava nada (0 passadas): pintava o assado da paleta.

## Barra

`bash tests/menu_topo_tira.sh`: geometria e mola da capsula; pt/en/de/ru/fr sem
corte (trilha 1003 px em pt, 1234 px em de). No 2e4d2434 nao compila
(`MenuTopoGeo` nao existe).

Capturas 1080p: `/Volumes/ExternalSSD/tmp/rev/vidroplx/`.
