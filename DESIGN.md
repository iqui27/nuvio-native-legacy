---
name: Nuvio Native Legacy
description: Interface de TV de 10 pés em C99 + SDL2 + GLES2, lida a três metros com um controle de D-pad.
colors:
  fundo: "#0D0D0D"
  superficie-painel: "#1B1C1F"
  superficie-trilho: "#202124"
  superficie-repouso: "#303030"
  superficie-esqueleto: "#2C2C2C"
  borda-sutil: "#2D2E32"
  texto-primario: "#FFFFFF"
  texto-claro: "#ECEEF4"
  texto-secundario: "#BEC0C8"
  texto-terciario: "#9699A2"
  realce: "#FFFFFF"
  dado-positivo: "#3DDB85"
  dado-negativo: "#ED4D4D"
  dado-ambar: "#F5C74D"
  dado-azul: "#8CA6FF"
typography:
  display:
    fontFamily: "Inter, sans-serif"
    fontSize: "76px"
    fontWeight: 700
  headline:
    fontFamily: "Inter, sans-serif"
    fontSize: "38px"
    fontWeight: 500
  title:
    fontFamily: "Inter, sans-serif"
    fontSize: "33px"
    fontWeight: 700
  body:
    fontFamily: "Inter, sans-serif"
    fontSize: "25px"
    fontWeight: 500
  label:
    fontFamily: "Inter, sans-serif"
    fontSize: "22px"
    fontWeight: 400
rounded:
  card: "0.055"
  badge: "0.18"
  pill: "0.5"
spacing:
  conteudo: "104px"
  card-gap: "24px"
  fileira-gap: "48px"
components:
  botao-repouso:
    backgroundColor: "{colors.superficie-repouso}"
    textColor: "{colors.texto-primario}"
    rounded: "{rounded.pill}"
  botao-foco:
    backgroundColor: "{colors.realce}"
    textColor: "#141414"
    rounded: "{rounded.pill}"
  painel-grafico:
    backgroundColor: "{colors.superficie-painel}"
    textColor: "{colors.texto-claro}"
    rounded: "{rounded.badge}"
---

# Design System: Nuvio Native Legacy

## 1. Overview

**Creative North Star: "A sala escura às onze da noite"**

Não é uma metáfora bonita, é a condição de uso literal e ela decide tudo. A
pessoa está a três metros de uma tela de 55", num cômodo com pouca luz, com um
controle que só tem quatro setas e OK. Não há mouse, não há hover, não há
segunda chance de ler um rótulo pequeno. O fundo é quase preto (#0D0D0D) porque
foi MEDIDO na referência e porque numa sala escura um fundo claro é uma lâmpada
apontada para o sofá.

O sistema é de superfícies planas e tons frios sobre esse quase-preto, sem
sombra decorativa e sem vidro. Profundidade aqui vem de LUMINÂNCIA, não de
blur: cada degrau da escada de superfícies existe porque foi preciso separar
duas coisas que estavam se confundindo, e cada um tem um contraste medido
contra o fundo. O defeito fundador desta base está escrito em `src/layout.h`:
um placeholder de card a contraste 1,0:1 com o fundo, ou seja, invisível, que
parecia "os pôsteres que não aparecem". Nada nesta interface pode depender de
uma diferença que só existe no monitor de quem escreveu o código.

O que este sistema REJEITA, vindo do `PRODUCT.md`: metadado inventado, listas
de demonstração no lugar das opções reais do addon, e o contrato de Apple TV do
protótipo separado. Num gráfico isso vira uma regra dura: um número desenhado
tem de ter fonte dita em texto, e ausência de dado nunca pode ser desenhada
como zero.

**Key Characteristics:**
- Quase-preto de base, superfícies frias empilhadas por luminância.
- Foco por PREENCHIMENTO claro com texto escuro, nunca por contorno.
- Piso de tamanho de texto: 22 px. Nada abaixo disso é texto para ler.
- Geometria pura: o shader só desenha quads alinhados aos eixos.
- Toda medida tem procedência escrita ao lado dela.

## 2. Colors

Neutros frios levemente azulados sobre quase-preto, com uma paleta de dado de
quatro posições usada só onde há dado.

### Primary

- **Realce** (#FFFFFF por padrão, configurável em Ajustes > Tema): a cor do
  item em FOCO. Entra como preenchimento de pilha/pílula com texto escuro por
  cima, não como anel. Os doze temas (`TEMA_ACENTO` em `src/ajustes.c`) são os
  `--focus-color` do app web, copiados e não escolhidos, e todos são claros o
  bastante para carregar texto #141414.

### Secondary — paleta de dado

Quatro posições, usadas por gráfico e por selo, nunca como decoração.

- **Positivo** (#3DDB85): melhor da série, retenção, "está subindo".
- **Negativo** (#ED4D4D): pior da série, queda.
- **Âmbar** (#F5C74D): revisita, atenção sem alarme.
- **Azul** (#8CA6FF): conversa, volume social.

A variante de contorno mais saturada de `src/novidades11.c`
(#DB3D3D / #4DBD61 / #EBC233 / #4D8FF0) é a mesma paleta com mais croma, para
peças pequenas onde a cor precisa sobreviver a 20 px de largura.

### Neutral — a escada de superfícies

Cada degrau existe para separar duas coisas, e o salto de luminância entre eles
é o que faz a separação existir a três metros.

- **Fundo** (#0D0D0D): a página. Medido na referência.
- **Superfície de painel** (#1B1C1F): a caixa de um gráfico ou de um cartão de
  conteúdo dentro da página. Frio de propósito — contra o fundo neutro ele lê
  como "uma camada acima", e não como "o fundo ficou sujo".
- **Trilho / calha** (#202124): o leito vazio atrás de uma barra. Tem de ser
  visivelmente mais claro que o painel e visivelmente mais escuro que
  qualquer dado.
- **Borda sutil** (#2D2E32): divisória e contorno de peça pequena.
- **Superfície de repouso** (#303030): botão, item de menu, chip — o estado NÃO
  focado. Medido na TCL como `--focus-bg`.
- **Esqueleto** (#2C2C2C): card sem arte enquanto a imagem não chega.

### Neutral — a escada de texto

- **#FFFFFF**: título, e o número que a tela existe para mostrar.
- **#ECEEF4**: a frase de resposta de um painel, o texto que se lê primeiro
  depois do título.
- **#D6D8E0**: valor secundário ainda legível como dado.
- **#BEC0C8**: rótulo de eixo, legenda, valor de referência.
- **#9699A2**: procedência, unidade, nota de rodapé. É o piso de contraste
  (≈5,6:1 contra #0D0D0D); nada de texto vai abaixo disto.

> **Deriva conhecida, a ser consolidada.** O cinza de rodapé aparece escrito de
> quatro jeitos ao longo de `src/*.c`: `150,154,165`, `150,153,162`,
> `150,152,160` e `150,154,163`. São a mesma intenção com quatro grafias. Código
> novo usa `150,153,162`; código antigo converge quando for tocado por outro
> motivo.

## 3. Typography

Uma família só: **Inter**, embarcada, em Regular / Medium / Bold. Não há peso
600 no pacote, e `src/text.c` registra a regra óptica que decide para onde um
600 do web arredonda: 600 claro sobre fundo escuro vira **Bold**; 600 escuro
sobre pílula clara vira **Medium**, porque texto escuro sobre superfície clara
já parece mais grosso do que é.

A escala NÃO é uma razão geométrica. Cada degrau foi medido no app web de
referência e o nome do seletor está no comentário ao lado, em `src/layout.h`.
Quando uma medida nova for precisa, ela vem da referência, não de multiplicar
a anterior.

| Papel | px | Peso | Onde |
|---|---|---|---|
| `TXT_TITULO1` | 76 | Bold | título da obra no detalhe |
| `TXT_TITULO2` | 56 | Bold | título de página, estado vazio |
| `TXT_TITULO3` | 48 | Bold | nome dentro do card destaque |
| `TXT_HEADLINE` | 38 | Medium | cabeçalho de fileira, título de painel |
| `TXT_ROW_TITULO` | 33 | Bold | `.home-row-title` |
| `TXT_CALLOUT` | 28 | Medium | linha de gênero |
| `TXT_BODY` | 25 | Medium | rótulo de botão, título de episódio |
| `TXT_DET_META2` | 23 | Regular | linha de meta secundária, procedência |
| `TXT_CAPTION` | 22 | Regular | rótulo de eixo, legenda, rodapé |
| `TXT_MINI` | 15 | Bold | selo de classificação — ÍCONE, não texto |

**O piso de 22 px é uma regra, não uma sugestão** (`src/text.c`, perto da linha
159). Quando um rótulo não cabe, a saída é desenhar menos rótulos, nunca
diminuir a fonte. Um eixo de 24 episódios rotula o primeiro e o último; os
outros vinte e dois não recebem rótulo, e isso é a resposta certa.

## 4. Elevation

**Não há elevação por sombra nesta interface**, com uma exceção nomeada.

A hierarquia é TONAL: a escada de superfícies da seção 2 é o único mecanismo de
profundidade em conteúdo. A razão é medida e está em `src/gfx.h`: o recurso
escasso nesta GPU Mali é PREENCHIMENTO, não triângulo. Duas camadas de tela
cheia derrubavam o app para ~40 fps; a home gasta 1,4–1,9 telas de
preenchimento e a tela de detalhe 2,2–3,5, já perto do limite. Uma sombra
difusa atrás de cada peça é área de preenchimento gasta em decoração.

A exceção é o **item em foco**: `GFX_SOMBRA`, raio 25 px, deslocada 16 px para
baixo, preto a 30%, junto com escala (1,09× em card 16:9, 1,14× em pôster) e um
levantamento de 8 px. Sombra centrada leria como halo; sombra caída lê como
objeto levantado. Isso vale para UM item por tela, o focado, e para mais nada.

Gradiente é usado só onde ele carrega texto sobre imagem (`GFX_VEU_CARD`,
`GFX_VEU_BAIXO`, `GFX_BRILHO_TOPO`), sempre avaliado POR PIXEL no shader. A
lição está escrita em `src/gfx.h`: a mesma rampa feita de 14 retângulos
empilhados mostrava as emendas numa tela de 55", porque o olho enxerga a
segunda derivada. Rampa é coisa de fragmento, não de pilha de quads.

## 5. Components

### O contrato do shader

Tudo é desenhado por um programa GL só, com um SDF de retângulo arredondado.
**Ele só desenha QUADS ALINHADOS AOS EIXOS — não há rotação.** Curva, barra,
traço e marca são todos construídos com retângulos. Quem projetar uma peça que
precise de polígono arbitrário está projetando para outro motor.

### O raio de canto, e a armadilha dele

`gfx_cor(r, raio, ...)` recebe o raio como **fração da ALTURA do retângulo**,
não do menor lado. O fragmento normaliza para `p = (uv - 0.5) * vec2(asp, 1.0)`
com `asp = w/h`: a meia-extensão vertical é sempre 0,5, então `raio * h` é o
raio em pixels, qualquer que seja a largura.

Consequência prática, e é um defeito que já foi fotografado numa TV: pedir um
raio constante dividindo por `min(w,h)` NÃO dá pixels constantes. Numa barra de
29 px de largura por 76 px de altura, `8/29` pede 0,27 **da altura**, ou seja
20 px; numa barra baixa a mesma conta dá ~8 px. Mesma linha de barras, duas
formas: cápsula numa ponta, quase quadrado na outra.

A forma correta de pedir um raio constante em pixels:

```c
float raio = px / h;                       // px em fração da ALTURA
if (raio > 0.5f) raio = 0.5f;              // cápsula vertical é o máximo
if (raio > 0.5f * w / h) raio = 0.5f*w/h;  // e nunca maior que a meia-largura
```

### Vocabulário de foco

**Selecionado = SUPERFÍCIE CLARA PREENCHIDA com texto ESCURO, sem contorno.**
A decisão é do dono, olhando a TV: "os botões quando selecionados ficar brancos
com o texto preto, na sidebar também, e pode tirar o contorno — fica mais
bonito fill do que contorno". Anel de foco sobrevive só onde não dá para
preencher: pôster, card com arte, campo de texto.

A troca de estado é em **DEGRAU**, em `f > 0.5`, e não interpolada. `src/text.c`
cacheia linha rasterizada por chave de COR: interpolar a cor do texto entre
claro e escuro geraria uma linha nova por quadro e estouraria o cache.

### Movimento

Molas de segunda ordem, com rigidez em `src/layout.h`. Foco: `NV_MOLA_FOCO`
25,0 — 95% em 120 ms, que é o tempo declarado na folha do app web, e igual nos
dois sentidos (com tempos diferentes existe um instante com dois anéis na tela,
que a referência nunca mostra). Rolagem de fileira: `NV_MOLA2_SCROLL` 11,5 rad/s,
metade do ajuste em 146 ms, medido no deslize da referência. Troca de tela:
`NV_MOLA_TELA` 9,0. Abertura da página de seções: `NV_MOLA_PAGINA` 3,8 — 95% em
800 ms, porque a arte de fundo do web leva 0,8 s para apagar.

### Gráficos

São componentes de pleno direito, não ilustração.

- Escala com base cortada é PERMITIDA quando a base zero apaga o fenômeno
  (nota de episódio vive entre 7 e 9,5; retenção de Trakt entre 90% e 100%),
  e só quando os dois extremos do eixo vão escritos ao lado da caixa.
- Um outlier não pode definir a escala do resto. Um episódio recém-lançado com
  3% de marcações achataria toda a temporada contra o teto.
- Buraco é buraco: episódio sem dado não é interpolado nem desenhado como zero,
  e o desenho tem de dizer "não publicado" em vez de parecer falha de render.
- Nunca empilhar grandezas de unidades diferentes numa barra: a soma não
  significa nada e o olho soma assim mesmo.
- Toda interpolação suave tem de ser MONOTÔNICA entre pontos. Catmull-Rom
  uniforme ultrapassa e desenha, entre dois episódios, uma nota mais baixa do
  que qualquer nota publicada — é metadado inventado por spline.

## 6. Do's and Don'ts

**Do**

- Escreva a PROCEDÊNCIA do número junto do número. "Nota do Trakt por episódio
  — não é o IMDb" é conteúdo, não rodapé jurídico.
- Meça na TV ou numa captura antes de afirmar que algo está resolvido. Esta
  base tem scripts `tests/*_shot.sh` exatamente para isso.
- Conte os desenhos quando adicionar geometria. O quadro inteiro da home são
  123 desenhos em 1,9 ms; um painel que peça 500 está fora do orçamento.
- Separe "carregando" de "não existe". São dois estados e a lista vazia é a
  mesma nos dois.
- Escreva no comentário o que você MEDIU e o que você REJEITOU, com o número.
  É o que impede a próxima pessoa de desfazer a correção.

**Don't**

- Não diminua a fonte para caber mais rótulo. Desenhe menos rótulos.
- Não use contorno para marcar seleção onde dá para preencher.
- Não empilhe retângulos para fazer gradiente. A rampa vai no fragmento.
- Não peça raio de canto dividindo por `min(w,h)`. Divida pela ALTURA.
- Não anime a cor de um texto. O cache de `src/text.c` é por chave de cor.
- Não invente metadado para preencher um espaço vazio no desenho. Um painel que
  diz "sem dados desta temporada" é honesto; um que interpola não é.
- Não escolha a cor pelo reflexo da categoria. "Retenção → azul sobre azul
  marinho" é o primeiro resultado do reflexo e some contra o fundo frio desta
  base.
