// Tokens visuais do port nativo. A referencia e o Nuvio 1.0.1 legacy (webOS):
// hero moderno, rail fixa e fileiras de posters. O prototipo Apple TV continua
// em outro build e nao define estes valores.
//
// Escala de tipografia: FATO da HIG do tvOS. Em 1080p, 1pt = 1px, entao os
// valores sao literais. Title1 76 / Title2 57 / Title3 48 / Headline 38 /
// Body 29 / Caption 25. Corpo nunca abaixo de 29.
#ifndef NV_LAYOUT_H
#define NV_LAYOUT_H

#define NV_TELA_W        1920.0f
#define NV_TELA_H        1080.0f

// O shell legacy usa uma rail de 72dp (144px no canvas 1080p) e inicia o
// conteúdo 104px depois dela, como no CSS .home-main + --home-content-start.
#define NV_LEGACY_RAIL_W        144.0f
#define NV_LEGACY_CONTENT_X     248.0f
#define NV_LEGACY_CONTENT_RIGHT 104.0f
// A regra de verdade, medida nos dois estados: o conteudo tem SEMPRE 104 de
// recuo, e a rail acrescenta os 144 dela quando esta aberta. Nao sao dois
// layouts — e `collapseSidebar` em layoutPreferences.js, que o perfil do dono
// tem em true. Com ela recolhida a home comeca em 104.
#define NV_CONTENT_PAD          104.0f

// Hero em TELA CHEIA (`modernHeroFullScreenBackdropEnabled`, tambem true no
// perfil do dono). MEDIDO: .home-modern-hero-media 1920x1062 em (0,0), imagem
// em `object-fit: cover` com `object-position: 100% 0`. O bloco de texto
// continua com 640 de largura, mas em x = NV_CONTENT_PAD e comecando em y=40.
// 1080 e nao 1062. Os 1062 vieram da medida do `.home-modern-hero-media` no
// app WEB, e ali sobravam 18px porque a janela do navegador tinha barra. Nesta
// TV a tela e 1080 cravados, e os 18px que a arte nao cobria apareciam como uma
// FAIXA no rodape — foi o "buraco/margem no final do background" que o dono viu.
//
// MEDIDO na captura do aparelho: y=1060 dava (13,13,13), y=1064 saltava para
// (37,38,41) e ficava assim ate o fim da tela, em qualquer coluna.
//
// INVESTIGADO ate o fim, e a conclusao importa para quem mexer nisto depois: os
// ultimos 18px NAO SAO NOSSOS. Provas, nesta ordem:
//   - com o heroi em 1062 havia faixa; subindo para 1080 a faixa CONTINUOU
//     igual, no mesmo y — ou seja, nao era o tamanho do heroi;
//   - trocar a cor do glClear por magenta nao pintou aquela regiao;
//   - desenhar um retangulo opaco ali tambem nao pintou;
//   - SDL_GL_GetDrawableSize responde 1920x1080 (esta no /tmp/nuvio-fps.txt).
// Ou seja: o SDL relata 1080 e a superficie GL de verdade tem 1062. A faixa e o
// fundo do compositor do webOS aparecendo, e nenhum desenho do app a alcanca.
//
// Fica em 1080 assim mesmo: e o valor CORRETO para a tela, o excedente e
// descartado sem custo, e se um firmware devolver a superficie inteira a arte
// passa a cobrir sozinha.
#define NV_HERO_CHEIO_H        1080.0f
#define NV_HERO_CHEIO_COPY_Y     40.0f
// Com o hero em TELA CHEIA o bloco de texto sobe: o web poe o logo em y=65 e a
// linha de meta em 257, contra 135 e 327 do hero de faixa. MEDIDO na sessao
// logada, que e a que tem `modernHeroFullScreenBackdropEnabled`. O port usava os
// numeros da faixa nos dois modos, e por isso o texto todo ficava 70px baixo
// demais — foi o que o dono descreveu como margem errada.
// A sinopse fica em 411 nos DOIS modos; so o que esta acima dela se desloca.
#define NV_HERO_CHEIO_LOGO_Y     65.0f
#define NV_HERO_CHEIO_META_Y    257.0f
// Em tela cheia o logo pode ser BEM maior: 640 de largura contra os 440 do hero
// de faixa. MEDIDO na sessao logada (.home-hero-logo = 640x160 em 104,65). O
// port limitava a 440 nos dois modos, e era isso que deixava a arte do titulo
// pequena — um dos pontos que o dono levantou olhando a referencia.
#define NV_LOGO_HERO_CHEIO_MAX_W 640.0f
// LINHA SECUNDARIA, que so existe em tela cheia: "2H RESTANTES • 6.3 • EN".
// y=341, altura 38, fonte 18 peso 600, rgba(255,255,255,0.88). Fica ENTRE a
// linha de meta (257) e a sinopse (411); sem ela sobrava um vao no meio do
// bloco, que e parte do que se lia como espacamento errado.
#define NV_HERO_CHEIO_SEC_Y     341.0f
// Respiro entre a base do bloco de texto do hero e o titulo da primeira
// fileira, e entre as linhas do proprio bloco. Vem da diferenca medida nas
// capturas: a sinopse termina ~48px acima do titulo da fileira, e as linhas do
// bloco se separam por ~52.
// Bloco de texto do hero, LIDO do CSS do app web e nao estimado das capturas.
// `.home-modern-hero-copy` (components.css:6723) e um flex column com
// justify-content:flex-end e gap:16 — ou seja ANCORADO NA BASE, e as linhas se
// separam por 16, nao pelos 52/84 que eu tinha deduzido de imagem. A base:
//   bottom: var(--modern-rows-viewport-height) + var(--modern-hero-copy-bottom-gap)
//         = 52% de 1080 + 40 = 601,6  ->  base do bloco em y = 478,4
// e o topo das fileiras cai nos mesmos 518,4 que NV_SHELF_TOP ja usa.
#define NV_HERO_COPY_GAP        40.0f   // --modern-hero-copy-bottom-gap
#define NV_HERO_COPY_LINHA      16.0f   // gap do flex column

// Scancode do BACK no SDL da LG (SDL_SCANCODE_WEBOS_BACK). Nao esta no
// SDL_scancode.h padrao, por isso vem como numero.
#define NV_SCANCODE_BACK 482
#define NV_SCANCODE_BLUE 489 // SDL_webOS.h: SDL_WEBOS_SCANCODE_BLUE
// Quanto tempo o OK precisa ficar pressionado para valer como pressao longa.
//
// 700 ms, e nao os 500 de antes. O relato foi "seguro e ele acaba clicando
// duas vezes": num controle de TV o OK afunda com forca e a soltura demora,
// entao um toque comum passava facil dos 500 e virava pressao longa. 700 e o
// limiar do Android TV, e a diferenca se sente.
//
// NAO HAVIA UM LIMIAR SO. A home usava NV_HOLD_FEEDBACK_MS, um valor separado
// de 110 ms cujo nome dizia "feedback" mas que era o GATILHO
// (`okHold >= 1.0f`) — ou seja, a home abria o menu do cartaz em 110 ms,
// enquanto o mesmo gesto na pagina de titulo esperava 500. Um decimo de
// segundo e menos que um toque normal, e era essa a "rapidez" do relato. O
// valor foi apagado: uma medida, um lugar.
#define NV_HOLD_MS       700
// fatia da fileira vizinha que fica visivel acima/abaixo da fileira em foco
#define NV_ESPIA_VIZINHA 0.10f

// Safe area: HIG pede >=60pt das bordas; overscan classico usa 80-90 nas
// laterais. Medido nas fotos de referencia: bate com 90.
// Safe area OFICIAL do tvOS: 80px nas laterais, 60px em cima e embaixo (HIG
// Layout). O valor 90 que estava aqui era chute meu.
#define NV_MARGEM_X      80.0f
#define NV_MARGEM_Y      60.0f

// No layout moderno legacy a arte ocupa a faixa superior (72% da largura e
// ~650px de altura); o viewport de fileiras permanece fixo nos 52% inferiores.
#define NV_HERO_H       650.0f
// Medido na foto do aparelho: a linha de botoes do hero termina a ~65% da
// altura, e a primeira fileira comeca logo abaixo, aparecendo cortada. Com a
// base em 150 os botoes desciam ate onde o cabecalho da fileira comeca, e os
// dois se sobrepunham.
// Medido na foto do aparelho: a linha de botoes do hero termina a ~69% da
// altura e o cabecalho da fileira vem ~100px depois. Com a base em 380 sobrava
// margem demais entre o bloco e os cards.
#define NV_HERO_BASE     570.0f
// A partir de quanta rolagem o bloco do hero comeca a sumir, e em quantos px
// ele desaparece por completo.
#define NV_HERO_FADE_INI 420.0f
#define NV_HERO_FADE_EXT 260.0f
#define NV_FUNDO_ESCURO   0.40f   // quanto o fundo da home escurece a arte
// A partir de quanta rolagem a ARTE do hero comeca a sumir, e em quantos px
// ela desaparece. Depois disso o que se ve e o fundo desfocado.
#define NV_HERO_ARTE_INI 300.0f
#define NV_HERO_ARTE_EXT 520.0f
#define NV_HERO_BOTAO_H   68.0f
#define NV_HERO_NBOTOES      3
// MEDIDO no app web: .home-modern-hero-media em x=555, y=0, 1421x670, com a
// arte em object-fit:cover. Os degrades que dissolvem a borda esquerda e a base
// estao no shader GFX_HERO, com as paradas anotadas la.
#define NV_HERO_ARTE_X   555.0f
#define NV_HERO_ARTE_W  1421.0f
#define NV_HERO_ARTE_H   670.0f
// CSS (components.css:6755): .home-hero-logo em modern tem height E max-height
// --modern-hero-logo-max-height (200px), width min(100%, 440px), object-fit
// contain com object-position LEFT TOP. Ou seja a caixa mede sempre 440x200 e
// a arte encosta no TOPO dela — nao cresce a partir da base, que era o que o
// port fazia. 160 era medida de uma arte concreta, nao da caixa.
#define NV_LOGO_HERO_H   200.0f

// Quando um logo e a VARIANTE ESCURA do TMDB e precisa ser tingido de branco.
//
// DUAS condicoes, e a segunda importa tanto quanto a primeira. Os numeros saem
// de medir os 40 logos de art/logo (media dos pixels opacos, croma =
// max(RGB)-min(RGB)):
//
//   arquivo  lum  croma   decisao
//   08, 19     0      0   TINGE  — preto puro, a variante errada
//   07        17     19   TINGE  — quase preto
//   27        74     21   TINGE  — cinza escuro, ilegivel sobre o backdrop
//   01        63    124   passa  — VERMELHO ESCURO: cor de marca, deliberada
//   24        76    255   passa  — vermelho puro
//   38        84     43   passa
//   20       129      0   passa  — acromatico, mas claro
//
// So a luminancia nao serve: reprovaria o 01 junto com os pretos, e tingir de
// branco um logo vermelho-escuro troca um defeito por outro. So o croma
// tambem nao: reprovaria o 20, que e cinza CLARO e le bem. E a conjuncao que
// isola exatamente a variante errada — escura E sem cor propria.
//
// O limiar de luminancia e 80 e nao 70 por causa do 27, que mede 74: com 70 ele
// escapava por quatro pontos e continuava sumindo na tela.
#define NV_LOGO_LUM_MIN     80
#define NV_LOGO_CROMA_MAX   45
#define NV_LOGO_HERO_MAX_W 440.0f
// Posicoes ABSOLUTAS do bloco de texto do hero, medidas no app web com a home
// no topo. Antes isto era ancorado na BASE (base = 1080 - NV_HERO_BASE) e
// empilhado para cima, que e como o app da Apple faz — o efeito colateral era o
// texto descer conforme a sinopse crescia e encostar no titulo da primeira
// fileira. No web cada linha tem lugar fixo:
//   logo      y=135  (h 160)
//   meta      y=327  (fonte 21, peso 500, rgb(179,179,179))
//   sinopse   y=411  (largura 640, fonte 22, h 89 em 2 linhas)
// e a fileira 0 comeca em 518, logo abaixo dos 500 onde a sinopse termina.
#define NV_HERO_LOGO_Y   135.0f
#define NV_HERO_META_Y   327.0f
#define NV_HERO_SIN_Y    411.0f
// 560 e o valor do tema padrao; a TV cai na regra `.legacy-webos`
// (components.css:19171), que devolve 640 para meta, secundaria E sinopse — com
// 560 a linha de meta de um episodio quebrava em duas ou tres linhas.
#define NV_HERO_SIN_W    640.0f

// HERO EDITORIAL DE COLECOES. A arte de uma colecao ja e uma composicao 16:9
// pronta (gradiente, luz e area de respiro); o texto nao deve competir com uma
// segunda capa grande no lado direito. As posicoes seguem a referencia da tela
// de Awards: grupo no alto, logo real da lista no centro-esquerdo e a acao
// encostando antes do cabecalho da fileira.
#define NV_COLLECTION_HERO_GROUP_Y      182.0f
#define NV_COLLECTION_HERO_LOGO_Y       258.0f
#define NV_COLLECTION_HERO_LOGO_MAX_W   520.0f
#define NV_COLLECTION_HERO_LOGO_MAX_H   150.0f
#define NV_COLLECTION_HERO_CAPTION_Y    448.0f
// line-height do CSS: sinopse 22*1.35, meta 21*1.25, secundaria 18*1.35.
#define NV_LD_HERO_SIN   30
#define NV_LD_HERO_META  26
#define NV_LD_HERO_SEC   24
// MEDIDO no app web: o titulo da primeira fileira fica em y=518 e os cards em
// y=564. A regra dos "2/3 da tela" que estava aqui e do productTemplate do
// tvOS, e nao e a nossa: no web as fileiras sobem por cima da parte de baixo da
// arte do hero (que vai ate 670), em vez de comecarem depois dela.
#define NV_SHELF_TOP     518.0f   // topo do cabecalho da primeira fileira
#define NV_LEGACY_ROW_HEAD_H 46.0f // titulo + margem ate os cards (564 - 518)

// As quatro secoes visuais que a home do Apple TV usa, cada uma com proporcao
// propria — OBSERVADO nas fotos de referencia:
//  1. HERO      arte 16:9 full-bleed que TROCA sozinha (carrossel + dots)
//  2. CARD      landscape 16:9 comum, a fileira padrao
//  3. POSTER    retrato 2:3, usado no Top 10 ao lado do numeral
//  4. DESTAQUE  card grande com bloco de metadados embaixo ("Assista em seguida")
// Larguras OFICIAIS da tabela de grid do tvOS (somam 1760 = area util):
//   4 colunas -> 410   |   5 -> 320   |   6 -> 260   |   8 -> 184
// MEDIDO no app web rodando em 1920x1080 (getBoundingClientRect, nao leitura de
// CSS): .home-content-card = 212 x 322, primeiro card em x=248, segundo em
// x=520. Os valores anteriores vinham do grid do tvOS e erravam nos dois:
// altura 318 (o 212 x 1.5 "certinho" que o web nao usa) e gap 24.
#define NV_CARD_W        212.0f
#define NV_CARD_H        322.0f
// 24, e o 60 que estava aqui era ERRO DE MEDIDA. A conta "520 - 248 = 272 de
// passo, menos 212 do card = 60" mediu um estado com o card EXPANDIDO pelo
// foco; o passo em repouso e outro.
//
// MEDIDO no app de referencia (com.nuvio.tv na TCL, 1920x1080, deteccao de
// goteira coluna a coluna): card 210, goteira 24,0 px em todas as ocorrencias
// consecutivas, passo 234. O CSS do web concorda: `--home-track-gap: 24px` em
// `.home-layout-modern`. Duas fontes independentes contra uma conta feita em
// cima do estado errado.
//
// Efeito: de ~6,6 para ~7,8 posteres por tela, e a fileira deixa de parecer
// rala — que era o defeito oposto ao que o comentario antigo dizia consertar.
#define NV_CARD_GAP      24.0f
// Passo entre fileiras MEDIDO: titulo da fileira 0 em y=518, da fileira 1 em
// y=934 -> 416. Desses, 46 sao do cabecalho ate os cards (518 -> 564) e 322 do
// card, sobrando 48 de respiro entre uma fileira e a proxima.
#define NV_FILEIRA_GAP  48.0f

// POSTER DEITADO (`modernLandscapePostersEnabled`). MEDIDO no app web com a
// preferencia ligada: `.home-poster-card.is-landscape` = 318 de largura, moldura
// 314x178.875 (16:9) com 2px de borda em volta -> caixa 318x182.9.
//
// De onde sai o 318: a folha do layout moderno define
// `--home-landscape-poster-width: calc(var(--home-poster-width) * 1.5)` e
// `--home-landscape-poster-height: calc(... * 0.5625)` sobre o
// `--home-poster-width: 212px` do proprio layout moderno (components.css:6462).
// NAO sai de `posterCardWidthDp`: a variavel inline que
// `buildModernHomeSizingStyle` escreve (399x225 para 120dp) e sobrescrita, e
// isso foi CONFERIDO no app rodando — trocar a variavel para 300px nao moveu um
// pixel do card.
//
// A fileira deitada tambem aperta o passo vertical: `--home-row-gap` cai de 32
// para 24 em `.home-modern-landscape-posters` (components.css:6473).
#define NV_CARD_LAND_W   318.0f
#define NV_CARD_LAND_H   182.9f   // 178.875 de moldura + 2px de borda em cima e embaixo
#define NV_CARD_LAND_ART 178.875f
#define NV_FILEIRA_GAP_LAND 24.0f
// A legenda do card deitado fica DENTRO da moldura: left/right 14, bottom 12,
// largura maxima 76% do card, sobre um degrade que cobre 54% da altura.
#define NV_LAND_COPY_PAD  14.0f
#define NV_LAND_COPY_BASE 12.0f
#define NV_LAND_COPY_MAXW 0.76f
#define NV_LAND_VEU       0.54f

// Rotulo abaixo do poster (`posterLabelsEnabled`). `.home-poster-copy`: padding
// 8px 2px 0, altura fixa `--home-poster-copy-height: 74px`, titulo 16/500 e
// subtitulo 13/400 rgba(255,255,255,0.7).
//
// ATENCAO: no layout MODERNO a folha esconde este bloco —
// `.home-screen-shell.home-layout-modern .home-poster-copy { display: none }`
// (components.css:7334) — e por isso a tela de Ajustes do web nem mostra a
// opcao quando o layout e moderno (`!isModernLayout` em settingsScreen.js:4050).
// O port desenha o bloco quando a preferencia esta ligada; ver a nota em home.c.
#define NV_POSTER_COPY_H   74.0f
#define NV_POSTER_COPY_PADT 8.0f
#define NV_POSTER_COPY_PADX 2.0f

#define NV_POSTER_W      212.0f
#define NV_POSTER_H      322.0f
// O Top 10 reserva espaco abaixo do poster para o rotulo de genero.
#define NV_TOP10_ROTULO   38.0f
#define NV_POSTER_NUM_W  118.0f   // faixa do numeral gigante do Top 10

// CORRIGIDO apos foto de referencia: no Apple TV os metadados do card grande
// ficam DENTRO da arte, sobrepostos na base sobre um veu escuro — nao abaixo
// dela. O logo do titulo aparece embutido na propria arte-chave.
// Medido por proporcao nas fotos: o card grande ocupa ~40% da largura da tela
// (~760px em 1920). E NAO e 16:9 — comparando a altura na foto do Apple TV, a
// proporcao fica perto de 3:2. Como nossa arte de origem e backdrop 16:9, o
// shader recorta (cover) em vez de esticar; sem isso a imagem deforma, que foi
// exatamente o defeito que apareceu na primeira tentativa.
#define NV_DESTAQUE_W    419.0f
#define NV_CW_PAD          18.0f
#define NV_CW_BAR_H         4.0f
#define NV_CW_BAR_BOTTOM   10.0f
#define NV_CW_BADGE_PAD_X  14.0f
#define NV_CW_BADGE_PAD_Y   8.0f
#define NV_CW_BADGE_RADIUS  7.0f
#define NV_DESTAQUE_H    236.0f   // continue watching: 419 x 236
                                  // (16:9 -> 3:2 -> 4:3 -> +20%: cada passo foi
                                  //  comparado lado a lado na TV)

// Hero-carrossel: tempo em cada arte e duracao do crossfade.
#define NV_HERO_INTERVALO_MS  7000
// APAGAR a arte velha, NAO dissolver uma na outra.
//
// MEDIDO na referencia (screenrecord do aparelho, quadros com carimbo de tempo,
// tres trocas de heroi na home, tecla DIREITA em "Continuar assistindo"):
//   - a arte fica intacta ate ~90 ms depois da tecla (latencia de entrada);
//   - a partir dai ela APAGA, e chega a alfa 0 aos ~450 ms depois da tecla,
//     ou seja ~330 ms de esvanecimento;
//   - a tela fica REALMENTE VAZIA (so o fundo) por um tempo que depende do
//     carregamento: medi 120 ms com a arte em cache e 780 ms sem;
//   - a arte nova entra de CORTE SECO, em UM UNICO QUADRO. Numa das trocas a
//     luminancia da regiao da arte saltou de 20,5 para 93,1 entre dois quadros
//     consecutivos (+72,5), e nos 1,8 s seguintes o gravador nao emitiu nem um
//     quadro — nada se moveu. Nao ha esvanecimento de entrada.
// Nao ha crossfade em momento nenhum: a arte velha e a nova nunca aparecem
// juntas. Eram 900 ms de mistura, quase o triplo do tempo e a forma errada.
#define NV_HERO_FADE_MS       220.0f
// REPOUSO ANTES DE TROCAR O HEROI. O fundo so acompanha o foco depois que ele
// PARA por este tempo.
//
// Sem isso, atravessar uma fileira de 12 cards trocava o heroi 12 vezes: a arte
// piscava a cada passo (o dono: "no outro aplicativo, se eu passar rapido pelos
// posteres, ele mantem a arte que estava ate eu parar no filme") e, pior, cada
// troca PEDIA UMA TEXTURA DE 1920 — ~8 MB cada. Doze delas em dois segundos
// estouram o orcamento do cache e despejam justamente os posteres que estao na
// tela, que e a outra queixa ("continua nao aparecendo todos os posteres"). As
// duas coisas eram o mesmo defeito.
//
// 220 ms: acima do intervalo de repeticao do D-pad segurado (~130 ms nesta TV),
// entao atravessar a fileira nao dispara nenhuma troca; e curto o bastante para
// que parar no card e ver o fundo responder pareca imediato.
#define NV_HERO_REPOUSO_MS    220
#define NV_HERO_DOT           9.0f
#define NV_HERO_DOT_GAP      14.0f

// Tipografia (px em 1080p)
// Escala tipografica do tvOS em 1080p (1pt = 1px). Os pesos vem do arquivo:
// a TV so tem Light e Regular da fonte LG, entao o negrito e sintetico.
#define NV_FT_TITULO1    76
// 56 e nao os 57 da escala do tvOS: hoje este estilo so serve a
// `.library-page-title` e aos titulos de estado vazio da busca e da biblioteca,
// e os tres medem 56 no web. O cabecalho da pagina de titulo do app da Apple,
// que era o dono do 57, nao existe mais no port.
#define NV_FT_TITULO2    56
#define NV_FT_TITULO3    48
#define NV_FT_HEADLINE   38
// Os corpos pequenos ficam ABAIXO da tabela do tvOS de proposito. A escala
// oficial pressupoe a SF Pro, e a Inter — que e a substituta possivel aqui — e
// visivelmente mais larga: no mesmo corpo, a mesma frase ocupou 336px contra
// 225px na captura do aparelho. Manter os numeros oficiais deixaria a mancha de
// texto de cada card metade maior que a do original. Os titulos ficam nos
// valores oficiais, onde a medida bateu (cap 40 contra 41).
#define NV_FT_BODY       25
#define NV_FT_CALLOUT    28
#define NV_FT_CAPTION    22
#define NV_FT_CAPTION2   21
#define NV_FT_MINI       15   // selo de classificacao (icone, nao texto)
// Corpos do PLAYER. Nao saem da escala do tvOS: saem do app web, que e a
// referencia desta variante. Os valores estao resolvidos para 1920x1080, que e
// onde o app roda — no CSS eles sao min(2.92vw,56px) e min(1.67vw,32px), e a
// TV cai sempre no teto. 56 nao vira TITULO2 (57) nem 32 vira HEADLINE (38)
// porque a diferenca aparece: o subtitulo em 38 empurra a barra de progresso
// para fora do lugar que o web reserva para ela.
// .home-row-title do web: 26px, peso 600. O nativo usava HEADLINE (38), e era
// isso que fazia o titulo da fileira invadir o card logo abaixo dele.
// 33 e nao 26. MEDIDO comparando a MESMA string ("Top 100 Today - Filme")
// presente nas duas capturas: largura da tinta 258 px no nosso contra 329 na
// referencia, altura 25,2 contra 32,0 — razao 1,27 nos dois eixos. 26 x 1,27 =
// 33. O 26 vinha do `.home-row-title` do web; o web e a TCL divergem aqui e a
// TCL manda, por ser o aparelho.
#define NV_FT_ROW_TITULO 33
// .home-modern-hero-secondary: 18/600 na sessao logada.
#define NV_FT_HERO_SEC   18   // --modern-hero-secondary-size (212*0.085)
#define NV_FT_HERO_META  21   // --modern-hero-meta-size (212*0.1), peso 500
#define NV_FT_HERO_SIN   22   // --modern-hero-description-size, peso 400
// Tela de DETALHE, medidos no app web rodando (getBoundingClientRect e
// getComputedStyle sobre .series-detail-shell), nao lidos da folha.
#define NV_FT_DET_BOTAO  25   // .series-primary-btn (peso 600)
#define NV_FT_DET_META   25   // .series-detail-support e .detail-meta-row
#define NV_FT_DET_SIN    26   // .series-detail-description
#define NV_FT_DET_META2  23   // .detail-meta-row.secondary
#define NV_FT_PLR_TITULO 56   // .player-title
#define NV_FT_PLR_CORPO  32   // .player-subtitle e .player-time-label
// Canto superior do player. O relogio e o "Termina as" vem do bloco ATV
// (components.css:15282), ja convertidos para o canvas de 1920; o guia
// parental nao e refeito la e fica com os 22 da regra base.
#define NV_FT_PG_RELOGIO 26   // .player-clock
#define NV_FT_PG_FIM     20   // .player-ends-at
#define NV_FT_PG_ROTULO  22   // .player-parental-label
#define NV_FT_PG_GRAV    22   // .player-parental-severity
// Entrelinha (leading) OFICIAL de cada estilo. Usar a altura que o SDL_ttf
// devolve nao e a mesma coisa: ela varia com os acentos da linha, entao um
// paragrafo fica com espacamento irregular linha a linha.
#define NV_LD_TITULO1    96
#define NV_LD_TITULO3    56
#define NV_LD_HEADLINE   46
#define NV_LD_BODY       32
#define NV_LD_CAPTION    29
#define NV_LD_CAPTION2   30

// ANEL DE FOCO, em pixels, para o app INTEIRO. MEDIDO na referencia: 4 px
// solidos de branco puro, sem rampa, por fora da caixa do elemento. Vale para
// card de home, card de episodio, botao de detalhe e tecla de teclado — um
// numero so. NV_DETW_ANEL ja era 4 e so era aplicado no detalhe.
#define NV_ANEL_FOCO      4.0f

// Area util explicita da home: a rail pode variar, mas o texto e o foco nunca
// encostam na safe area direita.
#define NV_HOME_SAFE_RIGHT    NV_LEGACY_CONTENT_RIGHT
#define NV_HOME_TEXT_GUTTER   24.0f

// FOCO EM SUPERFICIE (pilula, item de menu, chip): fundo ESCURO com texto
// branco — nao o contrario.
//
// Usavamos #E4E4E9 (claro) com texto escuro, que alem de invertido em relacao a
// referencia nao e cor de sistema nenhuma: nem #FFFFFF, nem o #F5F5F5 de
// --secondary-color. Era um off-white azulado inventado. A referencia tem UM
// token: --focus-bg #303030, confirmado no CSS do web e MEDIDO exato na TCL.
//
// Excecao legitima: o botao primario do detalhe ("Reproduzir") e branco com
// texto preto nos DOIS apps. Esse continua como esta.
#define NV_COR_FOCO_R     0.188f
#define NV_COR_FOCO_G     0.188f
#define NV_COR_FOCO_B     0.188f

// Raios, em fracao do menor lado (o shader usa SDF normalizado)
#define NV_RAIO_CARD     0.055f
#define NV_RAIO_PILL     0.5f
#define NV_RAIO_BADGE    0.18f

// Fundo: #0D0D0D. Aqui estava #252629, com a justificativa de que "o
// quase-preto fazia os cards flutuarem no vazio" — mas a referencia E
// quase-preta: MEDIDO #0D0D0D na home da TCL e #020202 na home rolada, e
// `--bg-color: #0D0D0D` no CSS do web. As duas fontes concordam.
//
// Nao e cosmetico. Com #252629 o placeholder de card sem arte (#242429) ficava
// a uma distancia de (1,2,0) do fundo — contraste 1,0:1, ou seja, INVISIVEL. Os
// posteres "que nao apareciam" apareciam: como retangulos da cor exata do
// fundo. Ver NV_COR_ESQUELETO logo abaixo.
#define NV_COR_FUNDO_R   0.051f
#define NV_COR_FUNDO_G   0.051f
#define NV_COR_FUNDO_B   0.051f

// Superficie de CARD SEM ARTE (#2C2C2C). MEDIDO na referencia, que a desenha
// solida na caixa exata do card enquanto a imagem nao chega — luminancia ~22x
// a do fundo, impossivel nao ver. E o que faz "carregando" ler como carregando
// em vez de como quebrado.
#define NV_COR_ESQUELETO_R 0.173f
#define NV_COR_ESQUELETO_G 0.173f
#define NV_COR_ESQUELETO_B 0.173f

// Foco: escala 1.05-1.10x na HIG. Usamos 1.09 no card.
// Escala do foco DERIVADA das tabelas oficiais de Top Shelf, que publicam
// tamanho focado e nao focado: poster 2:3 e quadrado crescem ~14%, card 16:9
// cresce ~9%. Eu usava 9% para tudo, o que deixava o poster subdimensionado.
#define NV_FOCO_ESCALA   0.09f    // cards 16:9
#define NV_FOCO_ESCALA_P 0.14f    // posters 2:3 e circulos
// O item em foco tambem SOBE, nao so cresce: no tvOS ele se levanta em direcao
// ao espectador e a sombra cai por baixo. Sem o deslocamento, escala e sombra
// juntas leem como "a imagem inchou", nao como "esta item veio para frente".
#define NV_FOCO_LIFT      8.0f
// Sombra do item em foco. Numeros de reimplementacoes de terceiros do efeito
// do tvOS (a Apple nao publica os dela): raio 25px, deslocada 16px para baixo,
// preto a 30%. O deslocamento vertical importa mais do que parece — sombra
// centrada le como halo, sombra caida le como objeto levantado.
#define NV_FOCO_SOMBRA   25.0f
#define NV_SOMBRA_DY     16.0f
#define NV_SOMBRA_ALFA   0.30f

// Molas: rigidez usada em anim_mola(). ~250-350ms de assentamento, sem overshoot.
// Ganhar foco e mais rapido que perder: assimetria que a Apple declara no HIG
// ("focusing animations should be prominent, unfocusing subtler"). Com a mesma
// rigidez nos dois sentidos a navegacao fica com um peso uniforme que nao
// existe no aparelho.
// ANEL DE FOCO — MEDIDO na referencia: ele nao esvanece, ele SALTA.
//
// Rastreei a aresta branca do card focado quadro a quadro. No primeiro quadro
// desenhado depois da tecla (16-49 ms) o anel ja esta no card novo com o branco
// cheio, e a folha do app web declara 120 ms `ease` para foco/borda. Os 13,0 /
// 8,5 que estavam aqui davam 230 ms e 350 ms para 95% — o dobro e o triplo.
// 25,0 fecha 95% em 120 ms, que e a medida. A assimetria foco/desfoco que havia
// aqui vinha do HIG do tvOS, nao desta interface: na referencia os dois lados
// levam o mesmo tempo, e com tempos diferentes existe um instante com DOIS
// aneis na tela, que a referencia nunca mostra.
#define NV_MOLA_FOCO     25.0f    // entrando no foco  (95% em 120ms)
#define NV_MOLA_DESFOCO  25.0f    // saindo dele       (mesmo tempo: ver acima)
#define NV_MOLA_SCROLL    8.0f
// Frequencia (rad/s) da mola de 2a ordem que rola as fileiras da home. Vale o
// k da CAUDA medida no deslize da referencia (~12,5 /s); 11,5 e o valor que
// faz a curva inteira bater, porque nessa mola a cauda e so metade do ajuste:
// p(t)=1-(1+wt)e^-wt cruza a metade em 1,678/w = 146 ms com w=11,5, e o medido
// foi ~145 ms. Ver anim_mola2() em anim.h para o porque da troca de mola.
#define NV_MOLA2_SCROLL  11.5f
#define NV_MOLA_TELA      9.0f
// Abertura da PAGINA de secoes do detalhe. MEDIDO na folha do app web:
// `.series-detail-shell.detail-scrolled .series-detail-backdrop` vai a
// `opacity: 0.15` em 0.8s cubic-bezier(0.4, 0, 0.2, 1). exp(-3.8*0.8) = 0.05,
// ou seja 95% do caminho em 800ms. Com NV_MOLA_TELA (9.0) a mola assenta em
// ~330ms e a arte apaga num piscar, que e menos da metade do tempo do web.
#define NV_MOLA_PAGINA    3.8f

// Tela de detalhe: um CARTAO da arte cobrindo quase tudo, com a home aparecendo
// pela moldura. O voo do card usa NV_MOLA_TELA, mais lenta que a do foco de
// proposito: troca de tela e movimento maior, e na rigidez do foco viraria corte.
// Medido no video do app da Apple: o cartao central ocupa ~88% da largura e
// ~94% da altura. A margem LATERAL e grande de proposito — e por ela que os
// cartoes vizinhos aparecem, ~95px de cada lado. Com margem pequena o cartao
// vira tela cheia e o deslize deixa de parecer troca de cartao: parece troca de
// quadro de um filme, que foi exatamente o que o dono viu na primeira versao.
// MEDIDO por retificacao por homografia de um frame do aparelho (a tela da TV
// mapeada para 1920x1080 exatos; validacao: o centro do cartao caiu em 957 de
// 960 esperado). Cartao 1674x?? com margem lateral 120 e superior 38 — e ele
// NAO tem margem inferior: e cortado pela base da tela.
// ---------------------------------------------------------------------------
// Tela de DETALHE — layout FULL-BLEED do app web.
//
// Tudo MEDIDO no app web rodando em 1920x1080, com o titulo "The Whisper Man"
// aberto (getBoundingClientRect). O que existia aqui antes — cartao com moldura
// de 120px, carrossel de vizinhos, tres niveis de zoom — e o padrao do app da
// Apple TV, e nao o desta variante. O web nao tem cartao: tem o backdrop
// cobrindo 1920x1080 em (0,0), a vinheta horizontal por cima, e UMA coluna de
// conteudo ancorada na base.
//
//   .detail-hero-section   padding 0 96 32 72, justify-content: flex-end
//   .series-detail-logo    261x104 em (72, 445)   [altura fixa 104, max-w 710]
//   .series-detail-actions 1752x108 em (72, 589), padding 6, gap 24
//     .series-primary-btn  298x96  em (78, 595)  raio 64, fonte 25/600
//                          padding lateral 48, gap icone-texto 16, icone 36
//     .series-circle-btn   84x84   em (439|586|734, 601)  raio 999, bg #222
//   .series-detail-support 1040x36  em (72, 727)  fonte 25/400 rgb(179,179,179)
//   .series-detail-descr.  1040x117 em (72, 787)  fonte 26/400 branco, lh 39
//   .detail-meta-stack     1752x120 em (72, 928)  gap 16
//     .detail-meta-row     y=928 h=49, fonte 25/400 rgb(179,179,179);
//                          generos a esquerda, ANO empurrado a direita (1824)
//     .detail-meta-row.sec y=1003 h=45, fonte 23/400 BRANCO; duracao e pais
//
// Os espacos entre blocos (30, 24, 24) sao margens do CSS e nao sobra de
// layout: com a sinopse mais curta o web encolhe pela base, porque a coluna e
// flex-end. Por isso aqui tambem se empilha DE BAIXO PARA CIMA.
#define NV_DETW_X          72.0f   // coluna de conteudo
#define NV_DETW_DIR      1824.0f   // borda direita util (1920 - 96)
#define NV_DETW_BASE     1048.0f   // base do bloco (1080 - 32 de padding)
// 104x710 era o MEDIDO no app web (.series-detail-logo 261x104 em 72,445), mas
// aquela medida saiu de uma janela estreita. Na tela de 1920 a arte do titulo
// ficava ocupando um quarto da largura e o dono apontou lado a lado com a
// referencia dele, onde ela toma mais de dois tercos. 200x1000 dobra o tamanho
// sem deixar o logo dominar a coluna de texto que vem abaixo.
//
// DIVERGENCIA DELIBERADA da medida do web, e nao descuido.
#define NV_DETW_LOGO_H    200.0f
#define NV_DETW_LOGO_MAXW 1000.0f
#define NV_DETW_LOGO_GAP   40.0f   // base do logo ao topo da linha de acoes
#define NV_DETW_ACOES_H   108.0f   // inclui os 6px de padding do anel de foco
#define NV_DETW_BTN_H      96.0f
#define NV_DETW_BTN_PADX   48.0f
// 34 e nao os 16 que o `gap` do flex declara. MEDIDO nos dois estados: o icone
// comeca em 126 e o rotulo em 196, e o icone tem 36 de largura — sobra 34. A
// folha mente aqui, como mentia no corpo do titulo do player.
#define NV_DETW_BTN_GAPI   34.0f   // icone -> rotulo
#define NV_DETW_BTN_ICONE  36.0f
#define NV_DETW_CIRC       84.0f
// Os botoes ficam em FLUXO, com 63px entre um e o outro. As posicoes
// x=439/586/734 que estavam aqui nao sao constantes: sao o que da a conta
// quando o rotulo e "Reproduzir" e nao ha botao secundario. Medido em duas
// telas diferentes (Whisper Man deslogado, Silo logado): em ambas o vao entre
// botoes vizinhos e 63, e o primario muda de largura com o rotulo — "Retomar
// T2E3" da 334 no lugar de 298, e tudo a direita anda junto.
#define NV_DETW_BTN_GAP    63.0f
#define NV_DETW_ANEL        4.0f   // box-shadow 0 0 0 4px #fff do item focado
#define NV_DETW_GAP_ACOES  30.0f   // acoes -> "Diretor:"
#define NV_DETW_GAP_SUP    24.0f   // "Diretor:" -> sinopse
#define NV_DETW_GAP_SIN    24.0f   // sinopse -> pilha de meta
#define NV_DETW_TEXTO_W  1040.0f   // largura de sinopse e linha de apoio
#define NV_DETW_LD_SUP     36.0f   // line-height da linha de apoio
#define NV_DETW_LD_SIN     39.0f   // line-height da sinopse
#define NV_DETW_SIN_LINHAS    3    // 117 / 39
#define NV_DETW_META_GAP   26.0f   // gap 16 + margin-top 10 da segunda linha
// Linha de retomada, so quando o titulo tem progresso. MEDIDA na sessao logada
// (Silo, 45%): 1720x37 em (72,633), fonte 22.66/400 rgba(255,255,255,0.82),
// entre a linha de acoes e a linha de apoio.
#define NV_DETW_RETOM_H    37.0f
#define NV_DETW_GAP_RETOM  22.0f   // acoes -> retomada (633 - 611)
#define NV_DETW_LD_META    35.0f
#define NV_DETW_LD_META2   31.0f
#define NV_DETW_META_SEP   24.0f   // gap do flex, dos dois lados do ponto

// ---------------------------------------------------------------------------
// Tela de BUSCA — MEDIDA no app web rodando (perfil do dono, 1920x1080).
//
// O port tinha um TECLADO EM GRADE 6x7 a esquerda e uma grade de resultados a
// direita. O web nao tem teclado nenhum: tem um campo de texto largo no topo
// (o sistema da TV abre o teclado dele) e os resultados vem em FILEIRAS
// horizontais, uma por catalogo de addon, com titulo e a origem embaixo dele.
//
//   .search-header        y=22  h=110, padding 0 104
//     .search-discover-btn 110x110 em (104,22)  bg #222, borda 1px #333, raio 22
//     .search-voice-btn    110x110 em (262,22)  -> passo 158 (gap 48)
//     .search-input-field  1396x110 em (420,22) bg #222, raio 22, 34/500,
//                          padding 0 32; focado: borda #f5f5f5 e
//                          box-shadow 0 0 0 2px rgba(245,245,245,.22)
//   .search-empty-state   y=148 h=400, centrado: icone 136 em y=220.5,
//                          titulo 56/600 em y=378.5, apoio 24/400 em y=446.7
//   .search-results-row   titulo 48/600 lh 51.84; subtitulo 20/400 rgb(179)
//                          com margin-top 4; trilho 88.3 abaixo do titulo
//     .search-result-card  248 de largura, poster 248x372 raio 22 borda 2px
//                          nome 28/500 lh 33.6 (margin-top 8)
//                          data 20/400 rgb(179) (margin-top 4)
//                          passo horizontal 280 (248 + 32)
//   passo entre fileiras 562.4
#define NV_BUSCA_HEAD_Y     22.0f
#define NV_BUSCA_HEAD_H    110.0f
#define NV_BUSCA_BTN       110.0f
#define NV_BUSCA_BTN_GAP    48.0f
#define NV_BUSCA_BTN_ICONE  54.0f
#define NV_BUSCA_RAIO       22.0f
#define NV_BUSCA_CAMPO_PADX 32.0f
#define NV_BUSCA_VAZIO_Y   148.0f
#define NV_BUSCA_VAZIO_ICO 136.0f
#define NV_BUSCA_VAZIO_TIT 378.5f
#define NV_BUSCA_VAZIO_SUB 446.7f
#define NV_BUSCA_ROW_SUB    55.8f   // topo do titulo -> topo do subtitulo
#define NV_BUSCA_ROW_TRILHO 92.3f   // topo do titulo -> topo dos cards
#define NV_BUSCA_ROW_PASSO 562.4f
#define NV_BUSCA_CARD_W    248.0f
#define NV_BUSCA_CARD_PASSO 280.0f
#define NV_BUSCA_POSTER_H  372.0f
#define NV_BUSCA_NOME_GAP    8.0f
#define NV_BUSCA_DATA_GAP    4.0f

// ---------------------------------------------------------------------------
// Tela de BIBLIOTECA — MEDIDA no app web rodando.
//
// O port tinha tres abas centralizadas ("Minha Lista", "Comprados", "Generos") e
// uma grade de 6 colunas de 212. O web tem: titulo a esquerda com um selo de
// origem a direita, DUAS pilulas de modo ("Salvos" / "Nuvem") e DOIS seletores
// largos ("Tipo" e "Ordenar"), e so entao a grade.
//
//   .library-main       padding 48 96 64 -> conteudo em x=96, y=48, largura 1728
//   .library-page-title 56/600, letter-spacing 1px, em (96,48)
//   .library-page-source 28/500 rgb(128,128,128) ls 4px, alinhado a direita (1824)
//   .library-view-mode-row y=136 h=56, gap 16: pilulas 150x56 raio 999,
//                        14/24 de padding, 21/400; escolhida bg #303030 borda
//                        2px #fff; as outras bg #222 borda 2px #333
//   .library-picker-row  y=212 h=110: dois seletores 840x110 em x=96 e x=984,
//                        raio 36, padding 18/28; focado bg #303030 borda 1px
//                        #fff, os outros bg #222 borda 1px rgba(255,255,255,.1)
//     .library-picker-title 19/500 rgb(128,128,128) ls 0.45 lh 24
//     .library-picker-value 30/500 branco ls 0.3 lh 40, margin-top 4
//   .library-empty-state y=354, padding-top 38, gap 18: titulo 46/500 lh 49.68,
//                        apoio 28/400 rgb(179,179,179) lh 35
//   .library-grid       6 colunas de 268 (auto-fill sobre minimo 252 em 1728,
//                        com 24 de gutter), poster 2:3 = 268x402 raio 24 com
//                        borda de 4px POR DENTRO, titulo 32/500 lh 1.18 a 16 do
//                        poster; passo de linha 487.8 (455.8 + 32)
#define NV_BIB_X            96.0f
#define NV_BIB_Y            48.0f
#define NV_BIB_W          1728.0f
#define NV_BIB_DIR        1824.0f
#define NV_BIB_MODO_Y      136.0f
#define NV_BIB_MODO_W      150.0f
#define NV_BIB_MODO_H       56.0f
#define NV_BIB_MODO_PASSO  182.0f
#define NV_BIB_PICK_Y      212.0f
#define NV_BIB_PICK_W      840.0f
#define NV_BIB_PICK_H      110.0f
#define NV_BIB_PICK_PASSO  888.0f
#define NV_BIB_PICK_RAIO    36.0f
#define NV_BIB_PICK_PADX    28.0f
#define NV_BIB_PICK_PADY    18.0f
#define NV_BIB_VAZIO_Y     354.0f
#define NV_BIB_GRADE_Y     354.0f
#define NV_BIB_COLUNAS         6
#define NV_BIB_CARD_W      268.0f
#define NV_BIB_CARD_GAP     24.0f
#define NV_BIB_POSTER_H    402.0f
#define NV_BIB_POSTER_BORDA  4.0f
#define NV_BIB_TIT_GAP      16.0f
#define NV_BIB_LINHA_PASSO 487.8f
// `.library-grid-card.focused { transform: scale(1.02) }` com origem no topo —
// e a UNICA escala de foco que sobrou em qualquer tela deste app, e ela e do
// web: as outras eram das tabelas de Top Shelf do tvOS e foram removidas.
#define NV_BIB_FOCO_ESCALA  0.02f

#define NV_DET_MARGEM_X  120.0f
#define NV_DET_MARGEM_Y   38.0f
#define NV_DET_PAD        44.0f   // medido: texto a 44px da borda do cartao
#define NV_DET_BOTAO_H    70.0f   // medido: pilula 254x70, raio = h/2
#define NV_DET_BASE      163.0f   // medido: fim do botao ate a base da tela
// Altura do logo do titulo. Bate com a altura de tinta medida na referencia
// (cap 83px), com folga para as letras que descem.
#define NV_LOGO_H        104.0f
#define NV_LOGO_MAX_W    620.0f
// No cabecalho da pagina o logo aparece menor que no cartao — ali ele e a
// etiqueta da tela, nao o protagonista.
#define NV_LOGO_CAB_H     62.0f
#define NV_LOGO_CAB_MAX_W 420.0f
// Logo dentro do card destaque da home, no tamanho do card sem foco. Ele cresce
// junto com o card, senao o titulo "descola" da arte ao ganhar foco.
#define NV_LOGO_CARD_H    54.0f
// Quanto a arte se ATRASA dentro da moldura durante o deslize, em fracao de
// textura. 0 = arte colada na moldura (parece um panorama unico passando);
// 0.12 = a janela corre por cima e a arte quase fica — o efeito do painel de
// feira em que a pessoa poe o rosto e o quadro troca.
#define NV_DET_PARALLAX  0.12f
// Quanto a arte cresce ao virar fundo da pagina esticada, e quanto escurece.
// Os dois juntos e que a transformam de foto em campo de cor.
#define NV_DET_ZOOM_FUNDO  1.35f
#define NV_DET_ESCURO_FUNDO 0.62f
// Nivel de mipmap amostrado no fundo da pagina: quanto maior, mais borrado.
// Medido: no fundo da pagina NENHUMA estrutura menor que ~250px sobrevive — e
// praticamente um gradiente de manchas. Bias 5.5 preservava detalhe demais.
#define NV_BLUR_PASSO       2.4f   // passo do gaussiano, em texels do alvo
// Teto de memoria das texturas. A TV tem cota, e a arte de verdade e grande:
// um backdrop 1920x1080 ocupa 8 MB depois de decodificado.
// 72 MB foi o teto posto depois de um "double free" — mas aquele estouro veio
// de o cache NAO TER teto nenhum (passava de 104 MB e crescia), nao de 96 ser
// demais. Com o catalogo dinamico sao ~48 titulos x 2 imagens, e a 72 o cache
// vivia encostado no limite (medido: 70,7 MB com 46 texturas), despejando e
// rebaixando sem parar — 12 a 15 janks por segundo durante a navegacao.
#define NV_TEX_ORCAMENTO_MB 96
// Secoes da pagina do titulo.
#define NV_ABA_W          236.0f   // medido
#define NV_ABA_H           63.0f   // medido; capsule (raio = h/2)
#define NV_ABA_PITCH      277.0f   // medido: texto a texto
// Medido: miniatura 410x228, texto ABAIXO dela, base da miniatura ao rotulo
// "EPISODIO n" 18px, e do fim do texto ao proximo cabecalho 143px.
#define NV_EP_H           512.0f   // miniatura + rotulo + titulo + 5 linhas + data
#define NV_EP_THUMB_GAP    18.0f
#define NV_AVATAR         168.0f
// Tracking: no tvOS ele e LEVEMENTE POSITIVO nos corpos pequenos (+0.4px) e
// praticamente zero nos titulos grandes — o oposto do reflexo de apertar
// titulos que vem do design web. O cabecalho da pagina e a excecao: ele e
// maiusculo e espacado de proposito, e da para ver isso na foto do aparelho.
#define NV_TRACKING_CAB     9.0f   // medido contra a captura do aparelho
// Espacamentos verticais MEDIDOS na pagina expandida.
// 64 e nao 84: o valor medido (84) e onde comeca a TINTA das maiusculas, e o
// desenho do texto parte do topo da caixa da linha, uns 20px acima disso.
#define NV_PG_TOPO         64.0f
#define NV_PG_TIT_ABAS     82.0f   // base do titulo ao topo das abas
#define NV_PG_SEC_CARDS    22.0f   // cabecalho de secao ao topo dos cards
#define NV_PG_ENTRE_SEC   143.0f   // fim de uma secao ao cabecalho da proxima
#define NV_ONDE_W         420.0f
#define NV_ONDE_H         106.0f
#define NV_SOBRE_H        150.0f
#define NV_DET_GAP        35.0f   // medido: gutter entre cartao e vizinho
// Quanto o cartao passa do tamanho final antes de assentar. Sem esse estouro a
// abertura parece "aparecer maior"; com ele, parece vir para a frente.
#define NV_DET_ESTOURO   0.035f

#endif
