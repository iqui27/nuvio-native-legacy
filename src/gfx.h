// Camada de desenho: um shader unico com SDF de retangulo arredondado, capaz de
// desenhar card com textura, sombra difusa, retangulo de cor e o hero com
// gradiente. Um so programa GL evita troca de estado a cada primitiva, que e o
// que mais custa nesta GPU.
#ifndef NV_GFX_H
#define NV_GFX_H
#include "gl_compat.h"
extern int gfx_hdr_output; // 0 SDR, 1 PQ, 2 linear scRGB, 3 HLG, 4 linear BT.2020, 5 limited PQ.

typedef enum {
  GFX_CARD   = 0,  // textura com cantos, parallax e especular
  GFX_SOMBRA = 1,  // mancha difusa (cor de uCor; `foco` = intensidade)
  GFX_COR    = 2,  // retangulo/pill de cor solida
  GFX_HERO   = 3,  // arte com gradiente para o fundo (aceita alpha p/ crossfade)
  GFX_VEU    = 4,  // veu escuro na base do card, sob texto sobreposto
  GFX_TEXTO  = 5,  // glifo: a forma vem do alpha da textura, nao do RGB
  GFX_FUNDO  = 6,  // arte desfocada por mipmap; `foco` carrega o bias
  GFX_VEU_TOPO = 7,// degrade escuro do topo para baixo (barra de cabecalho)
  GFX_SNAP   = 8,  // imagem ja pronta: sem SDF, sem efeito, so o quad
  GFX_PLAY   = 9,  // triangulo de "reproduzir", apontando para a direita
  GFX_BLUR   = 10, // uma passada de desfoque gaussiano; uPar da a direcao
  // Fundo da tela de DETALHE: a arte em "cover" ja fundida na vinheta
  // horizontal do app web. Um modo so, e nao arte + veu por cima, porque sao
  // duas camadas de tela CHEIA — nesta GPU o custo e de preenchimento, e a
  // segunda passada sozinha derrubava o quadro.
  GFX_DETALHE = 11,
  // Hero em tela cheia. Mesma ideia do GFX_HERO, com as rampas do OUTRO estado
  // da preferencia: cobrem mais da tela e sao mais fundas. Dois modos e nao um
  // parametrizado porque as paradas estao anotadas junto das medidas, e e assim
  // que este shader vem sendo mantido.
  GFX_HERO_CHEIO = 12,
  // Contorno sem miolo, cheio ou tracejado. Usa o mesmo SDF dos outros modos —
  // o traco e `-espessura < d < 0`, POR DENTRO do rect —, entao serve para
  // retangulo arredondado tanto quanto para circulo (raio 0.5 = circulo).
  // Prefira gfx_anel / gfx_anel_fora, que recebem a espessura em pixels.
  //
  // Passe a espessura em `parx`, na mesma escala normalizada de `raio`, e o
  // numero de tracos do pontilhado em `pary` (0 = anel continuo). Exemplo, o
  // circulo tracejado de "episodio nao assistido":
  //
  //   gfx_rect(r, 0, GFX_ANEL, 0, 0.06f, 12.0f, 0.5f, 1,1,1, 0.55f);
  //
  // Nao pinte o miolo da cor do fundo para simular anel: onde o veu esta em
  // 0.06 o fundo aparece atraves dele e o tampao se ve como mancha clara.
  GFX_ANEL = 13,
  // Icones dos botoes redondos da tela de titulo. Existem como SDF pelo mesmo
  // motivo do GFX_PLAY: os glifos sao SVG no web e a familia embarcada nao
  // garante simbolo nenhum. Antes os tres botoes eram "+" e dois "...", que nao
  // dizem o que fazem.
  //
  // GFX_OLHO — "marcar como assistido". Lente (dois arcos), iris cheia e, com
  // `parx > 0.5`, o risco na diagonal do estado "nao assistido".
  GFX_OLHO = 14,
  // GFX_FONTES — tres barras empilhadas, simbolo de "lista de fontes". Ocupou o
  // lugar do glifo do YouTube: este app nao toca trailer do YouTube (nao ha
  // extrator de stream), e um botao que promete o que nao faz e pior que um
  // botao com outra funcao. Fontes e coisa que o app SABE fazer.
  GFX_FONTES = 15,
  // GFX_MARCA — logo de UMA COR: a forma vem do ALPHA da textura e a cor vem de
  // uCor. E o oposto do GFX_TEXTO, que preserva o RGB da textura.
  //
  // Existe para o logo do titulo. O TMDB serve logos claros e escuros sem
  // marcar qual e qual, e logo preto sobre backdrop escuro some. Tingir pelo
  // GFX_TEXTO nao serve: la o RGB da textura passa direto, e TEM de passar —
  // senao o logo do IMDb vira silhueta branca e todo texto colorido da tela
  // perde a cor, que ja vem assada pelo SDL_ttf.
  //
  // Use SO com arte de uma cor. Logo colorido (o dourado, o vermelho) vai por
  // GFX_CARD, senao vira mancha chapada.
  GFX_MARCA = 16,
  // GFX_VEU_BAIXO — degrade PURAMENTE VERTICAL, transparente em cima e opaco
  // na base. E o par do GFX_VEU_TOPO.
  //
  // A COR DA BASE E A QUE VOCE PASSA (uCor.rgb). Para escurecer, passe 0,0,0.
  // Para APAGAR uma arte contra o fundo da pagina, passe a cor do fundo e
  // alfa 1: convergir para preto onde a pagina e #0D0D0D deixa uma emenda reta
  // na altura em que a arte acaba.
  //
  // O player usava GFX_VEU aqui, que escurece a base E A ESQUERDA. Aquele veu
  // foi feito para o hero da home, onde o texto fica no canto inferior
  // esquerdo; no player a componente lateral deixava o canto superior esquerdo
  // do retangulo escuro enquanto o direito era transparente, e a borda entre os
  // dois lia como uma placa — o "retangulo reto" que o dono apontou.
  //
  // A curva e o smoothstep ELEVADO AO QUADRADO: um smoothstep simples ainda
  // deixa uma banda percebivel onde a rampa comeca, porque o olho enxerga a
  // segunda derivada. Ao quadrado o inicio e quase plano e a transicao some.
  GFX_VEU_BAIXO = 17,
  GFX_SOCIAL = 18, // static wine/coral ambient background, no texture or blur
  // Foto circular sem o SDF de card. O disco tem sua propria mascara radial,
  // para a borda ficar uniforme e sem rebarbas em qualquer tamanho.
  GFX_AVATAR = 19,
  // Retrato editorial: foto limpa ancorada a direita, dessaturada e dissolvida
  // no fundo. Feito para pessoas, nao para backdrops 16:9 com texto embutido.
  GFX_RETRATO = 20,
  // Disco geometrico sólido. Usado como base do avatar e do foco para que o
  // contorno seja sempre concentrico, em vez de ser pintado sobre a foto.
  GFX_DISCO = 21,
  GFX_EDITORIAL = 22, // unmodified cinematic art, edge fade only; caller fits aspect
  // GFX_VEU_CARD — o veu do card de episodio, com a rampa AVALIADA POR PIXEL e
  // respeitando os cantos arredondados.
  //
  // Existia como 14 retangulos empilhados, um por degrau da rampa. Numa TV de
  // 55" isso e visivel: o dono mandou a foto do card com as faixas contadas a
  // olho, "da pra ver a graduacao do degrade". Nao adianta subir o numero de
  // degraus — o olho enxerga a SEGUNDA derivada, e a emenda entre faixas
  // continua aparecendo (e a mesma razao por que GFX_VEU_BAIXO eleva o
  // smoothstep ao quadrado). Cada faixa ainda era um quad de largura inteira
  // com SDF, entao eram 14 passadas de preenchimento por card.
  //
  // A rampa e a MESMA de antes, a do `linear-gradient` do app web: alfa 0.06 no
  // topo, 0.18 a 22%, 0.62 a 52%, 0.86 a 82% e 0.95 na base — so que agora
  // interpolada no fragmento.
  GFX_VEU_CARD = 23,
  // GFX_BRILHO_TOPO — realce CLARO no alto do card, com a rampa por pixel e
  // respeitando os cantos arredondados. E o par claro do GFX_VEU_CARD.
  //
  // Existe porque o "efeito de profundidade" (cardDepth* do web) era desenhado
  // com DOIS RETANGULOS CHAPADOS: um branco de 12 a 30 px no topo e outro
  // cobrindo 28% da altura. O dono descreveu o resultado como "uma barra grossa
  // no topo, fica estranho", e era literalmente isso — inclusive passando por
  // cima dos cantos arredondados, porque gfx_cor com raio proprio nao
  // acompanha o canto do card embaixo.
  //
  // uPar.x = ate onde a rampa vai, em fracao da ALTURA DESTE retangulo. A cor
  // vem de uCor.rgb (branco para realce), o alfa de uCor.a.
  // uPar.y > 0 inverte o sentido: cheia da BASE ate uPar.y, zero em uPar.x
  // (medidos da base) — o veu escuro da legenda do cartao deitado.
  GFX_BRILHO_TOPO = 24,
  // GFX_ARTE — a imagem como ela e, so que com os CANTOS ARREDONDADOS.
  //
  // Existe por causa do logo de canal que traz fundo proprio (o quadrado preto
  // do HBO Max, o cinza do Disney+): ele era desenhado por GFX_TEXTO, que nao
  // tem SDF nenhum, entao o quadrado do arquivo aparecia com canto vivo dentro
  // de um cartao arredondado — foi o que o dono viu.
  //
  // Nenhum dos modos que ja tinham SDF servia: o GFX_CARD escurece a arte em
  // 20% quando `foco` e 0 e ainda recorta 3% de cada borda para o parallax
  // (over-scan), o que num logo come a margem que o proprio arquivo reserva; o
  // GFX_MARCA joga fora o RGB e pinta de uma cor so. Aqui o RGB e o alpha vao
  // inteiros, multiplicados so pela mascara do SDF.
  GFX_ARTE = 25,
  // GFX_LUZ — luz difusa DENTRO de um retangulo arredondado: a queda radial
  // do GFX_SOMBRA, so que recortada pelos cantos do painel (uRaio) em vez de
  // pela tesoura. Existe para a "luz entrando pelo canto" dos paineis
  // flutuantes (menu.c, 21/09/2026): com gfx_recorte a mancha pintava o
  // canto por fora do arredondado e o cartao saia com um canto QUADRADO — foi
  // o que a captura do cartao de lembrete mostrou. Aqui um so desenho do
  // tamanho do painel faz os dois, e ainda custa menos preenchimento que uma
  // mancha de 1,3x a largura sob a tesoura.
  //
  //   uPar.xy = centro da luz, em fracao do retangulo (pode ficar fora dele)
  //   uFoco   = alcance da luz, em fracao da ALTURA do retangulo
  //   uCor    = cor e alfa no centro; falloff suave usa highp quando disponivel
  //             e smoothstep cubico elevado ao quadrado para abrir a penumbra.
  //
  // Use gfx_luz_canto, que faz essa conta em pixels.
  GFX_LUZ = 26,
  // GFX_SINO — glifo vetorial do toast de avisos. O toast aparece antes que o
  // cache de texturas tenha necessariamente terminado de carregar, entao o
  // sino nao pode depender de PNG assincrono para existir no primeiro quadro.
  GFX_SINO = 27,
  // GFX_ESQUELETO — superficie de card CARREGANDO com uma faixa de luz que
  // atravessa a tela (skeleton shimmer). Use gfx_esqueleto, que calcula a onda
  // e cai no GFX_COR parado com animacoes reduzidas.
  GFX_ESQUELETO = 28,
  // GFX_LINHA — segmento de reta com nucleo e halo, resolvido no fragmento.
  // Existe para a constelacao da tela Explorar: nao havia primitiva de linha
  // (so retangulos alinhados), e pontilhar com dezenas de discos custava um
  // desenho por ponto. O segmento vai de um canto ao oposto do retangulo,
  // descontada a margem do halo:
  //   uPar.x = 0 -> do alto-esquerda ao baixo-direita; 1 -> baixo-esq ao alto-dir
  //   uFoco  = meia espessura do nucleo, em fracao da ALTURA do retangulo
  //   uRaio  = alcance do halo (e margem), na mesma unidade
  // O retangulo e o envelope do segmento: quem chama parte linhas longas em
  // pedacos para o envelope nao virar area de tela cheia (ver explorar.c).
  GFX_LINHA = 29,
  // GFX_CEU — ceu procedural da Explorar numa passada: base escura, nebulosa
  // tingida por uCor e estrelas por celula de grade com cintilacao.
  //   uPar  = deriva da grade (parallax), em celulas
  //   uFoco = tempo em segundos (0 = parado, para animacoes reduzidas)
  // Substitui o preenchimento de fundo da tela, entao nao soma camada cheia.
  GFX_CEU = 30,
  // COR VIVA (corviva.h). Os dois primeiros NAO se pedem pelo nome: gfx_rect
  // troca GFX_COR e GFX_ANEL por eles quando a cor passada e exatamente o
  // destaque vivo e o tema e "Dinâmica gradiente" ou "imersiva" — e assim que
  // o botao, a linha em foco e o anel de ~40 arquivos viram degrade sem que
  // nenhum mude. As paradas vem de nv_grad_viva, e a luz anda devagar com
  // nv_tempo_viva (o degrade "respira"; parado com animacoes reduzidas).
  GFX_COR_GRAD = 31,
  GFX_ANEL_GRAD = 32,
  // GFX_AMBIENTE — a cor da arte VAZANDO na interface: quatro luzes grandes e
  // macias (esquerda, direita, topo, base) com as cores de regiao da arte,
  // numa passada so de tela cheia, com dither contra faixas. Use gfx_ambiente.
  GFX_AMBIENTE = 33,
  // GFX_COPIA — a textura INTEIRA, RGB e ALPHA, sem SDF nem efeito. E a
  // ampliacao do alvo interno de 1280x720 (gpunivel.h, nivel 2): o alpha tem
  // de passar junto, senao o furo do plano de video some. uPar.y > 0.5 = a
  // fonte e um FBO (origem embaixo), como no GFX_SNAP.
  GFX_COPIA = 36,
  // GFX_VITRINE — a arte do destaque nos layouts PADRAO e DINAMICA da home
  // (ajustes_home_layout). Cover com ANCORAGEM vertical, cantos por SDF, o veu
  // de leitura (esquerda e base) ASSADO na propria passada, e um esvanecer
  // opcional da base em ALFA. Um modo so e nao arte + veu por cima, pelo mesmo
  // motivo do GFX_DETALHE: o custo desta GPU e o de preenchimento, e cada
  // camada extra de um destaque de ponta a ponta e uma tela cheia a mais.
  //
  //   uPar.x  = ancoragem vertical do recorte, 0 (topo) a 1 (base); 0,5 = centro
  //   uPar.y  = 1 dissolve a base em alfa (o fundo atras aparece);
  //             0 deixa a arte inteira
  //   uCor.r  = inicio do veu inferior em 0..1; 0 usa o padrao 0,38
  //   uCor.a  = alfa da arte; uCor.g/b nao sao usados
  //   uFoco   = forca do veu de leitura, 0 a 1 (0 = arte pura, sobre o trailer)
  //   uRaio   = canto, como no GFX_CARD (fracao da altura)
  //   uTexAsp = w/h da textura (gfx_tex_aspect_atual)
  GFX_VITRINE = 34,
  // GFX_FUNDO_DIN — o fundo da home DINAMICA: SO COR, um degrade vertical de
  // uma cor (a do titulo em foco). Sem textura e sem arte desfocada.
  //   uCor.rgb = cor do topo; uCor.a = alfa
  //   uFoco    = quanto da cor sobra na base (0..1)
  //   uPar.x   = 0 = alfa cheio ate a base; > 0 = onde o ALFA comeca a
  //              esvair (fracao da altura, 1-smoothstep ate a base)
  //   uPar.y   = fade endpoint when > uPar.x; otherwise the bottom edge
  GFX_FUNDO_DIN = 35,
  // GFX_HERO_CAM / GFX_HERO_CHEIO_CAM — as camadas do destaque numa passada
  // (o crossfade). So por gfx_hero_camadas.
  GFX_HERO_CAM = 37,
  GFX_HERO_CHEIO_CAM = 38,
  // GFX_JANELA — o CARTAO DO CARROSSEL da Dinamica (detail.c): a arte em
  // "cover" na TELA CHEIA, vista por uma janela arredondada. A arte nunca
  // escala: o rect e a abertura, e gfx_janela_atual diz onde ela fica no
  // quadro da arte (x, y, w, h em fracao da tela). Esticar o cartao so
  // aumenta a abertura. Leva a vinheta do GFX_DETALHE (mesma curva, medida no
  // quadro da arte) com forca em uFoco, e o apagar da pagina rolada em
  // uPar.x (mistura com o fundo, opaco); uPar.y escolhe o veu: 0 = so o canto
  // de baixo a esquerda (cartao), 1 = a vinheta da pagina — no cartao do tamanho da tela os
  // dois modos dao o mesmo pixel, e a troca entre eles nao se ve.
  // Cor com r < 0.5 = SO O VEU (cor do fundo com alpha, sem a arte): por cima
  // do furo do trailer que toca no cartao.
  GFX_JANELA = 39,
  // VEU EM DEGRADE DE CSS (Glass UI do player, 03/10): o preto (ou uCor.rgb)
  // cheio numa borda do rect e zero na outra, pelo dither de nv_dither. Os
  // veus do mockup sao linear-gradient de varias paradas; aqui viram uma curva
  // so: (1-d)^uFoco com uFoco > 0, ou 1-smoothstep(d) com uFoco = 0. `d` e a
  // distancia da borda cheia, em fracao do rect, dividida por uPar.y (o ponto
  // em que o veu zera; 1 = a borda oposta). uPar.x escolhe a borda cheia:
  // 0 = baixo, 1 = cima, 2 = esquerda, 3 = direita. Use por gfx_veu_css.
  GFX_VEU_CSS = 40,
  // GFX_MINI — o alvo de uma miniatura (gfx_mini_desenhar): FBO lido de cabeca
  // para cima, cantos por SDF.
  GFX_MINI = 41,
  // GFX_TEXTURA — o recorte do titulo numa pilula/barra (cor de destaque
  // "Textura"). Nao se pede direto: gfx_rect troca para ele todo GFX_COR
  // cheio na cor exata do destaque enquanto gfx_textura_definir tem textura.
  GFX_TEXTURA = 42,
  // GFX_VIDRO — o vidro do PlxNative (fs_glass): o backdrop desfocado da
  // propria tela (uTex, meia resolucao, coordenada de tela) com lente, luz de
  // borda, aro e scrim; sem uTex sai chapado. Nao se pede direto: gfx_vidro.
  GFX_VIDRO = 43,
  // GFX_VITRINE_DIN — o GFX_VITRINE OPACO sobre o fundo da Dinamica
  // (GFX_FUNDO_DIN) calculado no proprio fragmento: o destaque e o fundo numa
  // passada so. Nao se pede direto: gfx_rect troca o GFX_VITRINE por ele
  // quando o fundo da Dinamica esta pendente e o destaque cobre o topo da tela
  // (ver gfx_fundo_din_desenhar). uDin = cor do topo e queda.
  GFX_VITRINE_DIN = 44,
  // GFX_HOLOFOTE — the Stage row spotlight in ONE full-screen pass: a tinted
  // dark veil everywhere except a soft ellipse, plus a faint beam from the top
  // edge down to the lit poster. No texture, no blur.
  //   uPar     = ellipse centre (fraction of the rect)
  //   uRaio    = ellipse x radius, uFoco = y radius (fractions of the HEIGHT)
  //   uCor.a   = veil alpha outside the light
  //   uCor.r   = poster floor y (fraction of height)
  //   uCor.g   = beam x (fraction of width), uCor.b = floor pool radius
  //              (fraction of height). uAcento = current app accent.
  GFX_HOLOFOTE = 45,
  // Selected internally for GFX_CARD while the Hero reading tint is active.
  GFX_CARD_HERO = 46,
  GFX_PALCO_CHAO = 47, // Stage floor glow without a fullscreen veil.
  // GFX_PALCO_ARTE — Stage backdrop art, one pass, alpha-faded into the page.
  //   uPar.x = 0 Cortina (left/bottom mask), 1 Aura (saturated, vignette;
  //            pass the gfx_desfocado copy), 2 Triptych centre panel,
  //            3 Triptych side panel (grey, dim), 4 Monument (accent duotone)
  //   uPar.y = horizontal cover anchor, 0 left .. 1 right
  //   uFoco  = Triptych skew: top edge shift, in fractions of the rect width
  //   uCor.a = alpha; uTexAsp = texture w/h
  GFX_PALCO_ARTE = 48,
  // Integer prescale with a bilinear transition. uPar=inverse source size,
  // uFoco=integer scale; alpha remains the original bilinear sample.
  GFX_COPIA_NITIDA = 49,
// Opt-in upscalers (Settings > This TV > Upscaling algorithm), compiled on demand
  // on first use. Source = internal FBO: uPar = source size in texels.
  GFX_COPIA_EASU = 50,     // FSR 1 EASU
  GFX_COPIA_LANCZOS = 51,  // Lanczos3
  GFX_RCAS = 52,           // FSR 1 RCAS (sharpening after upscale)
  GFX_HDR_OUTPUT = 53,     // Final SDR UI to HDR conversion; preserves all color accuracy
  GFX_NMODOS = 54
} GfxModo;

typedef struct {
  float x, y, w, h;
} GfxRect;

// A textura do titulo para o modo Textura (ajustes_textura_quadro): `janela`
// e o recorte (x, y, w, h em fracao da imagem), `aspecto` o w/h da textura,
// `forca` 1 ou 0,35 (sutil), `veu` o alfa do veu atras do rotulo e
// `veuBranco` 1 quando a tinta e escura. tex 0 desliga.
void gfx_textura_definir(GLuint tex, const float janela[4], float aspecto,
                         float forca, float veu, int veuBranco);
int  gfx_textura_ativa(void);
// O matiz da Imersiva por cima de um miolo de vidro (16% da luz do destaque,
// com a forca da luz ambiente). gfx_vidro_painel/folha ja o chamam.
void gfx_vidro_matiz(GfxRect r, float raio, float a);
// O miolo de cor lisa (cr,cg,cb,ca) E a matiz acima numa passada so — o mesmo
// pixel que gfx_cor(miolo) + gfx_vidro_matiz(r, raio, a), uma camada a menos.
void gfx_vidro_miolo(GfxRect r, float raio, float cr, float cg, float cb, float ca, float a);

// Proporcao (w/h) da textura a desenhar. 0 = mapeia direto (texto, veu).
// Definir ANTES de gfx_rect para que a arte seja recortada, nunca esticada.
extern float gfx_tex_aspect_atual;
// 1 = a textura do proximo GFX_ARTE tem alfa 255 em todo pixel (tex_opaca).
// Com ela, um GFX_ARTE de tela cheia, canto vivo e alfa 1 esconde tudo o que
// estiver por baixo, e a luz ambiente pendente (gfx_ambiente) nao e pintada.
// Quem liga desliga logo depois do desenho, como gfx_tex_aspect_atual.
extern int gfx_arte_opaca_atual;
int gfx_arte_veu(GfxRect r, GLuint tex, float base, float aEsq, float aDir, float a);
// Deslize da arte do destaque dentro do proprio retangulo, em fracao da largura
// (so GFX_HERO, GFX_HERO_CHEIO e GFX_VITRINE). Quem define devolve a 0.
extern float gfx_desliza_atual;
// 1 = o GFX_CARD deve sempre preencher a moldura com cover. Usado pela forma
// editorial 4:3, que nao pode cair no contain quando recebe arte 16:9.
extern float gfx_card_forcar_cover_atual;
// 1 = o GFX_CARD escurece a arte pelos dois veus de borda da tela de perfis
// (os mesmos GFX_VEU_TOPO 330 px / 0,92 e GFX_VEU_BAIXO 300 px / 0,95, em
// coordenada de tela). Sobre fundo preto e o mesmo pixel que os dois veus
// desenhados por cima, sem as duas passadas de largura inteira. So a parede
// de capas (psestilos.c) liga; quem liga desliga.
extern float gfx_card_veu_tela_atual;
// Hero A horizontal tint inside GFX_CARD; callers reset alpha to zero after drawing.
extern float gfx_card_heroi_veu_atual[4];
// 1 = o cartaz em foco ganha o rebordo claro no GFX_CARD; 0 = nao ganha. E um
// ajuste da pessoa (Ajustes > Foco no cartaz), lido uma vez por quadro pela
// tela que desenha; o brilho e o especular do foco nao dependem dele.
extern float gfx_borda_foco_atual;
// Deslocamento da luz do foco no GFX_CARD (revela.h, revela_varre): 0 = faixa
// especular no repouso. Quem define para um card devolve a 0 logo depois.
extern float gfx_varre_atual;
// O VEU DA BASE DENTRO DA ARTE DO CARD. {fracao1, alfa1, fracao2, alfa2}: os
// veus de gfx_veu_base que o PROXIMO GFX_CARD ja aplica no proprio fragmento.
// Como passada separada o veu dividia com a arte a cobertura da borda e, no
// pixel da borda, deixava arte quase sem veu: um fio claro na base do card.
// Quem chama: limpa, poe os veus (gfx_veu_card_por), desenha a arte, liga
// gfx_veu_na_arte (gfx_veu_base passa a pular os mesmos veus) e limpa no fim.
// Draw a rectangle except for opaque interiors that will be painted later.
void gfx_rect_furos(GfxRect r, GLuint tex, GfxModo modo, float foco,
                    float parx, float pary, float raio, float cr, float cg,
                    float cb, float ca, const GfxRect *furos, int n);
extern float gfx_veu_card_atual[4];
extern int gfx_veu_na_arte;
int  gfx_veu_card_por(float fracao, float alfa);
void gfx_veu_card_limpar(void);
// Opacidade de grupo: deve voltar a 1 ao terminar o grupo.
extern float gfx_opacidade_grupo;
// Opt-in for axis-aligned card rows; restores legacy submission elsewhere.
extern int gfx_cortar_invisiveis;
// Transformacao de grupo (Ajustes v2: a lista que cresce por cima do menu):
// todo retangulo e recorte desenhado ate gfx_sem_transformar sai escalado por
// `s` em torno de (ox, oy) e deslocado por (dx, dy), em coordenadas de layout.
// Nao aninha. Miniaturas e passadas internas ficam de fora.
void gfx_transformar(float ox, float oy, float s, float dx, float dy);
void gfx_sem_transformar(void);
// Ha um gfx_transformar ativo? Durante uma transicao o desenho sai escalado
// e os retangulos das funcoes nao sao os da tela: quem registra alvos de
// ponteiro/toque espera ele voltar 0.
int  gfx_transformado(void);
// GIRO DE GRUPO: todo retangulo desenhado ate gfx_sem_girar sai girado `rad`
// radianos em torno de (cx, cy), em coordenadas de layout. Existe para a parede
// inclinada da escolha de perfil e o feixe do projetor (perfilsel.c). O recorte
// (tesoura) NAO gira. Nao aninha; passadas internas ficam de fora.
void gfx_girar(float rad, float cx, float cy);
void gfx_sem_girar(void);
// Janela do GFX_JANELA: x, y, w, h do rect no quadro da arte em tela cheia,
// em fracao (0..1). Quem desenha devolve a {0,0,1,1}.
extern float gfx_janela_atual[4];

// Snapshot: renderiza uma tela inteira para textura, para poder redesenha-la
// como um unico quad. Existe porque a home continua visivel pela moldura da
// tela de detalhe, e redesenhar hero + ~20 cards a cada quadro so para preencher
// uma borda de 120px derrubava o app para 30fps. A home nao muda enquanto o
// detalhe esta aberto, entao basta guardar a imagem dela.
//
// A meia resolucao e deliberada: o snapshot aparece escurecido e so nas bordas,
// e ninguem distingue — mas o custo de preenchimento cai a um quarto.
// Recorte de tesoura: limita o desenho a um retangulo da tela. Serve para
// pintar so a parte que aparece — no detalhe, o cartao cobre o centro e a home
// atras so e vista pela moldura, entao pintar a tela inteira por baixo dele e
// trabalho jogado fora.
void gfx_recorte(float x, float y, float w, float h);

// --- ICONES ------------------------------------------------------------------
//
// Os glifos da interface vinham DESENHADOS NO SHADER (GFX_OLHO, GFX_FONTES, o
// "+" feito de dois retangulos). Cada um era uma aproximacao a mao do SVG
// original, e o resultado o dono resumiu assim: "ta muito feio esses icones,
// nao invente essas merdas, usa SVG reais".
//
// Agora sao os ARQUIVOS DE VERDADE. Os .svg do app web foram rasterizados a
// 128px para deploy/app/art/icones/ (script /tmp/svg2png.py: injeta
// width/height, troca currentColor por branco e chama sips). O PNG guarda a
// forma no ALPHA, entao gfx_icone desenha com GFX_MARCA e a COR vem do
// chamador — o mesmo arquivo serve preto sobre pilula branca e branco sobre
// circulo escuro.
//
// `nome` e o basename sem extensao: "mais", "visto", "naovisto", "fontes",
// "play", "pause", "legenda", "audio", "aspecto", "avancar", "episodios",
// "trailer".
// Excecao: aj_smartphone acompanha o nucleo como o mesmo PNG embutido. O TPK
// pode atualizar somente a .so e conservar arte antiga sem esse arquivo.
// Carga preguiçosa, uma textura de 128x128; gfx_encerrar devolve a textura.
void gfx_icones_dir(const char *dirArte);
void gfx_icone(GfxRect r, const char *nome, float cr, float cg, float cb, float ca);
void gfx_sem_recorte(void);

int  gfx_snap_iniciar(int w, int h);

// Desfoque por REDUCAO, no lugar do mipmap. O mipmap parecia a saida barata,
// mas as texturas de arte nao tem lado potencia de dois, e em GLES2 a piramide
// de uma textura NPOT e mal definida — o resultado era um padrao de listras
// verticais no fundo da pagina, bem visivel contra a referencia. Aqui a arte e
// desenhada num alvo minusculo e depois esticada com filtro linear: o borrao
// sai liso e custa uma leitura por pixel.
// `via` escolhe o alvo: 0 = pagina de detalhe, 1 = fundo da home. Dois alvos
// porque as duas telas coexistem — o detalhe cobre a home, mas a home continua
// desenhada por tras dele, e um alvo so faria as duas brigarem pela mesma
// textura a cada quadro.
int  gfx_borrao_iniciar(int w, int h);
void gfx_borrao_gerar(int via, unsigned int tex, float texAspecto);
void gfx_borrao_desenhar(int via, GfxRect r, float alpha);
void gfx_borrao_encerrar(void);
// Copia DESFOCADA e pequena (96x54) de uma textura de arte, para o ajuste
// "Desfocar nao assistidos" (#133). Gerada na primeira chamada e guardada; as
// seguintes devolvem a mesma textura. `chave` e o caminho da arte, e com o
// nome GL identifica a copia (o nome sozinho volta de glGenTextures com outra
// imagem depois de um despejo). Desenhe como qualquer arte, por GFX_CARD e
// com gfx_tex_aspect_atual = tex_aspecto(caminho): a copia guarda a imagem
// INTEIRA e ja sai de pe.
//
// Devolve 0 enquanto nao ha copia (limite de geracoes por quadro, ou alvo de
// render indisponivel). Nesse caso NAO desenhe a arte nitida no lugar — o
// ajuste existe para esconder o spoiler; pinte o fundo do card.
GLuint gfx_desfocado(GLuint src, const char *chave);
// Um pixel da copia desfocada (u da esquerda, v de cima). So diagnostico.
int gfx_desfocado_px(GLuint tex, float u, float v, unsigned char rgb[3]);
int  gfx_snap_ok(void);        // 1 quando o FBO do snapshot existe
void gfx_snap_comecar(void);   // redireciona o desenho para o snapshot
void gfx_snap_terminar(void);   // volta para a tela
int  gfx_snap_vazio(int *maxCanal);   // 1 = o snapshot (alvo ligado) saiu preto
void gfx_snap_desenhar(void);  // pinta o snapshot ocupando a tela toda
void gfx_snap_encerrar(void);
int  gfx_snap_ativo(void);     // 1 enquanto o desenho vai para o snapshot
// Cresce a cada gfx_snap_comecar: quem guardou algo no FBO sabe se outro
// desenho passou por cima desde entao (explorar.c, o ceu assado).
unsigned gfx_snap_geracao(void);

void gfx_tamanho_alvo(int w, int h);
void gfx_tamanho_tela(int *w, int *h);   // current drawable, excluding internal render passes
float gfx_px_por_unidade(void);   // pixels do alvo da tela por unidade de layout (assrender.c, #335)

// MINIATURA: uma tela DE VERDADE desenhada num alvo proprio e mostrada
// reduzida (previa das Novidades da 1.8.0, inspetor do Guia de uso). Entre
// comecar e terminar o desenho segue em coordenadas de layout: o ponto
// (x0, y0) cai no canto de cima do alvo e cada unidade vale `esc` pixels.
// Nada de gfx_desfocado/gfx_ambiente/snapshot la dentro.
typedef struct { GLuint fbo, tex; int w, h; } GfxMini;
int  gfx_mini_alvo(GfxMini *m, int w, int h);   // cria ou recria; 0 = sem FBO
void gfx_mini_comecar(GfxMini *m, float x0, float y0, float esc);
void gfx_mini_terminar(void);
int  gfx_mini_ativo(void);
void gfx_mini_desenhar(const GfxMini *m, GfxRect r, float raioPx, float a);
void gfx_mini_liberar(GfxMini *m);
// Dentro da miniatura: a base de `r` some ate `alfa` (o .gv.fade do mockup).
void gfx_mini_esvanecer(GfxRect r, float alfa);   // drawable real, para restaurar viewport
// TAMANHO DA INTERFACE (Ajustes > Aparencia). As camadas que nao sao a home
// nem o detalhe (Ajustes, player, folhas, ilhas, menus, modais) desenham numa
// TELA VIRTUAL de 1920/s x 1080/s, e o gfx amplia tudo por s na hora de
// rasterizar: retangulos, recortes, furos e alvos do ponteiro. O texto dessas
// camadas e rasterizado JA no tamanho ampliado (text.c), nunca esticado.
//   gfx_escala_ui      o fator configurado (1, 1.2, 1.3, 1.5)
//   gfx_escala         o fator ATIVO agora (1 fora de uma camada ampliada)
//   gfx_escala_entrar  liga o fator configurado; devolve o anterior
//   gfx_escala_sair    devolve o anterior (aninhar e seguro: nao multiplica)
// Com s = 1 nenhum caminho muda: e o mesmo pixel de antes.
void  gfx_escala_ui_definir(float s);
float gfx_escala_ui(void);
float gfx_escala(void);
float gfx_escala_entrar(void);
void  gfx_escala_sair(float anterior);
// ESCALA DE TEXTO E COMPONENTES (Ajustes > Aparencia). Cobre o que o Tamanho
// da interface nao cobre: home, detalhe, biblioteca. Os posters mantem os
// controles proprios (fileiras, FIL_ESTILO_TAMS): gfx_poster_k() e o fator
// que as constantes de poster do layout.h multiplicam para voltar ao px de
// antes dentro de uma camada de texto.
//   gfx_escala_ui       agora devolve o PRODUTO (interface x texto): as
//                       camadas ampliadas ja existentes seguem as duas
//   gfx_escala_ui_base  so o Tamanho da interface
//   gfx_escala_txt      so a escala de texto (1 = de sempre)
//   gfx_escala_txt_entrar liga so a escala de texto; sair e o mesmo
void  gfx_escala_txt_definir(float s);
float gfx_escala_ui_base(void);
float gfx_escala_txt(void);
float gfx_escala_txt_entrar(void);
// O fator da tela virtual de uma camada de texto: a escala de texto quando a
// camada ativa e exatamente ela, senao 1 (a tela de sempre).
float gfx_escala_tela_txt(void);
float gfx_poster_k(void);
int  gfx_iniciar(void);
void gfx_tex_aquecer(GLuint tex); // amostra offscreen antes de publicar arte nova
void gfx_encerrar(void);

// O gfx_rect lembra a ultima textura que ele mesmo bindou e pula rebinds
// repetidos. Quem binda ou destroi textura POR FORA dele precisa avisar:
// passe o nome destruido, ou 0 para "esqueca tudo" (apos um upload).
void gfx_tex_esquecer(GLuint tex);

// Desenha um retangulo. `foco` 0..1 controla especular/sombra; `parx/pary`
// so deslocam a luz; a arte nunca e cortada nem deslocada (#176); `raio` em fracao do menor lado.
// TELEMETRIA DE QUADRO. Zerados por gfx_novo_quadro, uma vez por quadro.
//
// O QUE ESTES NUMEROS JA RESPONDERAM (medido na TV, home rolando, 1920x1080):
// o pior quadro gastava ~20ms dentro de app_desenhar e a suspeita era travessia
// de GL. Nao era: com 123 desenhos por quadro, gfx_rect somava 1,9ms — menos de
// 10% do quadro. O custo estava em CPU de layout/texto, fora deste arquivo.
//
// `gfx_fill` e o que sobrou de util no dia a dia: a nota no topo do gfx.c diz
// que DUAS camadas de tela cheia derrubavam esta Mali para ~40fps, e sem medir
// a area "quantas camadas cheias tem esta tela" e chute. Medido: home 1,4-1,9
// telas de preenchimento; DETALHE 2,2-3,5 telas, com 2 desenhos cobrindo mais
// de meia tela cada. A tela de detalhe e a que anda perto do limite.
//
// Os relogios finos (gfx_ms_rect, tex_ms_busca) ficam atras de NV_PERF_FINO
// porque custam DUAS leituras de relogio por desenho — cerca de 250 chamadas
// por quadro so para medir. Ligue com -DNV_PERF_FINO quando a pergunta voltar.
extern int    gfx_n_rect;   // chamadas de gfx_rect
extern int    gfx_n_prog;   // trocas de programa GL (glUseProgram)
extern int    gfx_n_bind;   // trocas de textura (glBindTexture)
extern double gfx_ms_rect;  // ms de CPU dentro de gfx_rect
extern int    gfx_n_outros;  // chamadas de recorte/FBO/desfoque
extern double gfx_ms_outros; // ms de CPU nesses pontos de GL
extern double gfx_fill;      // area submetida no quadro, em telas cheias
// O mesmo, RECORTADO na tela: retangulo que sai pelas bordas (a prateleira que
// sangra, o destaque rolado) so conta o que aparece. E o mais proximo do
// preenchimento real que da para tirar sem GPU; nao enxerga a tesoura.
extern double gfx_fill_vis;
// O PREENCHIMENTO QUE A GPU PAGA: o quad em pixels do alvo, cortado pelo alvo e
// pela tesoura, em telas do alvo da tela. Os alvos pequenos (luz assada 320x180,
// snapshot) contam pelo tamanho deles, e o clear de cor chapada (gfx_rect de
// tela cheia opaca, gfx_furo) nao conta: e um glClear. `_mist` e a parte
// desenhada COM mistura (le a tela; a Mali nao descarta o que fica por baixo).
// `_ult` = o do quadro anterior (o log le estes).
extern double gfx_fill_gpu, gfx_fill_gpu_mist;
extern double gfx_fill_gpu_ult, gfx_fill_gpu_mist_ult;
extern int    gfx_n_cheio;   // desenhos cobrindo >= 50% da tela
extern int    gfx_n_cheio_mistura;   // desses, com mistura (so -DNV_FLUIDEZ_PERF)
// O mesmo gfx_fill repartido por modo (programa): diz QUAL shader cobre a
// tela, que e a pergunta de uma GPU presa em preenchimento (gpunivel.h).
extern double gfx_fill_modo[GFX_NMODOS];
extern int gfx_n_assados;
extern int gfx_rastro_grandes;   // rastro: imprime todo desenho >= 12% da tela
// INSTRUMENTO DE CAMPO: bit N ligado = o modo N nao e desenhado. main.c le a
// lista de /tmp/nuvio-gfx-off a cada 3 s (numeros separados por espaco). Serve
// para medir na TV quanto fps cada tipo de desenho custa, sem recompilar.
extern unsigned long long gfx_modos_desligados;
extern double gfx_fill_modo_ult[GFX_NMODOS];
// Quadros iniciados: quem anima por reaparicao compara com o ultimo quadro em
// que desenhou (um relogio de parede confunde engasgo com reaparicao).
extern unsigned gfx_quadro_n;
void gfx_novo_quadro(void);

// EFEITOS LEVES (nivel 1 de gpunivel.h). 1 = sem o dither dos degrades (o
// ruido highp por pixel em todo veu, rampa do destaque e luz) e sem os dois
// realces puramente decorativos (GFX_BRILHO_TOPO CLARO, GFX_LUZ; o veu escuro
// de gfx_veu_base fica, ele carrega o texto). E um uniform, e
// nao outra compilacao: trocar de nivel no meio da sessao nao pode custar a
// compilacao inteira dos shaders (0,7-2 s numa TV fraca).
// VEU SO NA BASE de um card: degrade vertical que chega a zero em `fracao` da
// altura (medida da base), com os cantos de baixo do card. `raio` e o do card
// (fracao da altura DELE). Substitui o GFX_VEU de cartao inteiro, que cobria
// arte onde nao ha texto e custava fps na C9 (29/09: 48 -> 60 sem ele).
void gfx_veu_base(GfxRect card, float raio, float fracao, float alfa);
// REALCE CLARO NO TOPO de um cartao (GFX_BRILHO_TOPO): rampa de `alcance`
// (fracao da altura) a partir do topo, com os cantos do cartao. Desenha so a
// faixa da rampa: o mesmo pixel de pedir o cartao inteiro, com ~90% menos
// preenchimento (ver gfx.c).
void gfx_brilho_topo(GfxRect r, float raio, float alcance,
                     float cr, float cg, float cb, float ca);
// Veu em degrade (GFX_VEU_CSS): `borda` 0 = cheio embaixo, 1 = em cima,
// 2 = a esquerda, 3 = a direita; `curva` > 0 e o expoente de (1-d), 0 = a
// queda suave; `fim` (0..1] e onde zera. Preto a `a`.
void gfx_veu_css(GfxRect r, int borda, float curva, float fim, float a);
// gfx_veu_css com um chapado preto de alfa `base` por baixo, composto na mesma
// passada (uma tela cheia misturada a menos que gfx_cor + gfx_veu_css).
void gfx_veu_css_base(GfxRect r, int borda, float curva, float fim, float a, float base);
void gfx_definir_efeitos_leves(int leves);
int  gfx_efeitos_leves(void);
// EFEITOS MINIMOS (nivel 2 de gpunivel.h), por cima dos leves: sem a luz de
// tela cheia do tema imersivo (gfx_ambiente) e sem sombra/halo (GFX_SOMBRA).
// Resolucao nativa: tira camadas que so enfeitam em vez de borrar o texto.
void gfx_definir_efeitos_minimos(int minimos);
int  gfx_efeitos_minimos(void);

void gfx_rect(GfxRect r, GLuint tex, GfxModo modo, float foco,
              float parx, float pary, float raio,
              float cr, float cg, float cb, float ca);

// Atalhos legiveis para os casos comuns.
void gfx_cor(GfxRect r, float raio, float cr, float cg, float cb, float ca);
// Multiplicador de COR (nao de alfa) aplicado a GFX_COR/TEXTO/ANEL/MARCA: o brilho
// da interface do player (esmaecer.h). 1 = sem efeito; quem muda restaura.
extern float gfx_osd_mult;
// UI em HDR (esmaecer.h): HDR_UI_BRILHO enquanto o video do player esta em
// HDR/DV, 1 fora. Vale para texto, cor e logos; quem muda restaura.
extern float gfx_hdr_ui;
// A mancha GFX_SOMBRA em `s` sem o miolo `furo`, que um painel OPACO pintado em
// seguida cobre inteiro (mesmo pixel, menos preenchimento). Ver gfx.c.
void gfx_sombra_vazada(GfxRect s, float foco, float parx, float raio, float cr, float cg, float cb, float ca,
                       GfxRect furo);
// A sombra `s` de um painel que vem por cima com alfa `alfaPainel` e canto de
// `raioPx` pixels: vazada quando o painel e opaco (>= 0,98), inteira senao.
void gfx_sombra_sob(GfxRect s, float foco, float parx, float raio, float cr, float cg, float cb,
                    float ca, GfxRect painel, float raioPx, float alfaPainel);
// Destaque em camadas numa passada so (o crossfade): devolve 0 sem desenhar
// se nao puder garantir o mesmo pixel do caminho em duas passadas.
int gfx_hero_camadas(GfxRect r, GfxModo modo, GLuint texA, float aspA, float alfaA,
                     GLuint texB, float aspB, float alfaB);
// A luz ambiente do tema "Dinâmica imersiva", tela cheia, com `alfa` a mais
// por cima da forca que corviva ja anima. Nao desenha nada fora dele: o custo
// e zero nos outros temas. main.c chama logo depois do glClear.
// Assa a luz imersiva no quadro pequeno. Chamar ANTES do clear da tela (ver gfx.c).
void gfx_ambiente_preparar(void);
// O VIDRO (port do PlxNative, ver gfx.c): o que JA esta pintado atras de `r` e
// capturado, desfocado em meia resolucao e lido pela superficie, com o scrim
// por cima. Um backdrop por quadro, dividido entre as superficies e reusado
// entre quadros enquanto nada pintado antes delas muda. `cinza` = o tom do
// scrim (R = G = B); `dens` = alfa dele no painel (a barra usa o perfil
// proprio, escalado pela "Opacidade do vidro"). Chapado (so o scrim) com "Vidro
// fosco" desligado, efeitos minimos, video por baixo, ou sobre outro vidro.
enum { GFX_VIDRO_PAINEL = 0, GFX_VIDRO_BARRA = 1 };
void gfx_vidro(GfxRect r, float raio, int tipo, float cinza, float dens, float a);
// A27 TOP CHROME (avatar chip, top bar, clock island): one material, one tint.
// gfx_chrome_tinta = scrim alpha from "Opacidade do vidro", >= the floor;
// gfx_chrome = the glass (BARRA), or opaque #1A1C21 + 1 px rim .22 without vidro.
// gfx_chrome_dens = the density solve for that glass (luma ceiling, top cap,
// bottom cap). Higher opacity lowers the ceiling and raises the caps, so the
// scrim over bright art thickens. 78% stays 0.125 / 0.72 / 0.82.
#define GFX_CHROME_TINTA_MIN 0.30f
float gfx_chrome_tinta(void);
void gfx_chrome(GfxRect r, float raio, float a);
void gfx_chrome_dens(float out[3]);
// O MATERIAL PADRAO de todo popup/dialogo/painel/ilha no vidro: o gfx_vidro do
// cinza 0,06 na densidade `dens` (GFX_VIDRO_DENS, GFX_VIDRO_DENS_MODAL) vezes a
// "Opacidade do vidro". Compartilha o backdrop do quadro; nunca um gfx_cor.
#define GFX_VIDRO_DENS       0.80f
#define GFX_VIDRO_DENS_MODAL 0.86f
void gfx_vidro_material(GfxRect r, float raio, float dens, float a);
// 1 while drawing a glass LAYER above the screen (sidebar): earlier glasses it
// covers no longer force it flat (set/cleared by the caller around the draw).
extern int gfx_vidro_camada;
// O mesmo com "Interface de vidro" desligada: a cor lisa (sr,sg,sb) a `sa`.
void gfx_vidro_ou_solido(GfxRect r, float raio, float dens, float a,
                         float sr, float sg, float sb, float sa);
// Conteudo mudou sem passar por gfx_rect (textura reenviada no lugar): o
// proximo quadro recaptura.
void gfx_vidro_sujar(void);
// Detail title <-> episodes (#205): this frame's glass backdrop may reuse the
// capture of the same crossfade step (>= 0); -1 (reset every frame) = exact.
void gfx_vidro_fundo_passo(int passo);
// Capturas e passadas de desfoque do quadro corrente (zeradas em gfx_novo_quadro).
extern int gfx_vidro_capturas, gfx_vidro_passadas;
// Blur writes and framebuffer copy area, in drawable screens per frame.
extern double gfx_vidro_fill, gfx_vidro_copia;
// O quadro tem video vivo (furo) por baixo: o vidro sai chapado ate o proximo
// gfx_novo_quadro (a tela ali e o furo, nao o que passa no video).
void gfx_vidro_fosco_bloquear(void);
// Call right after a full-screen veil: glass drawn earlier no longer makes a
// glass over it go flat (modal islands over the Home sidebar).
void gfx_vidro_cobrir(void);
// Glass drawn with a blur source this frame (zeroed in gfx_novo_quadro).
extern int gfx_vidro_com_fonte;
// Smoked (flat tinted) glasses drawn this frame: no capture, no blur (zeroed in gfx_novo_quadro).
extern int gfx_vidro_chapados;
extern int gfx_vidro_area;                                // glass area (px^2) drawn this frame (governor workload signature)
int  gfx_vidro_degradado(void);                           // governor dropped the glass to smoked glass (sticky)
void gfx_vidro_governar_zerar(void);                      // tests: forget the governor state
void gfx_vidro_governar(double ms, double alvo_ms);       // once per frame, after the swap
// CANAIS DE FUNDO DA LUZ ASSADA (fundo.c, "Arte borrada" e "Frost" de tela
// cheia): o quadro pequeno (320x180, GL_RGB) do `canal` (0..1), pintado por
// `pintar(ctx)` em coordenadas de layout de tela cheia pelo MESMO assado da luz
// imersiva, so quando `chave` (n <= 24 floats) muda. Devolve a textura (0 =
// sem FBO: desenhe direto).
GLuint gfx_luz_canal(int canal, const float *chave, int n, void (*pintar)(void *), void *ctx);
// O MESMO fundo de tela cheia (alfa 1, sem veu), ADIADO como a luz imersiva
// pendente: o primeiro desenho decide. A arte com rampa de tela cheia
// (GFX_DETALHE, GFX_HERO*) le o fundo pelo uAmb e sai opaca numa passada so;
// um veu de cor de tela cheia vai junto na passada do fundo; qualquer outro
// desenho pinta o fundo antes, como gfx_luz_canal_desenhar faria. Devolve 0
// (e nao adia) dentro de snapshot/miniatura, com tesoura ou opacidade de grupo.
// Quem precisa LER a tela logo depois do fundo (conferencia) nao adia.
int gfx_luz_canal_adiar(GLuint tex, int pontilhar);
// A passada de tela da luz imersiva (GFX_SNAP de tela cheia, opaca e sem
// mistura com alfa 1). `veu` = { r, g, b, forca } aplicado na mesma passada, ou
// NULL; `pontilhar` = a cor pelo nv_dither.
void gfx_luz_canal_desenhar(GLuint tex, float a, const float *veu, int pontilhar);
extern int gfx_n_fundo_assados;
// 1 = o desenho direto de sempre, sem quadro pequeno (testes comparam os dois).
extern int gfx_fundo_assado_desligado;
// DIAGNOSTICO DE UMA VEZ (fundo.c): le um pixel do quadro pequeno `tex` ou do
// alvo atual (u da esquerda, v de cima, 0..1), e grava em BMP o quadro pequeno
// (320x180) ou o alvo atual (metade do tamanho). Devolvem 0 se nao deu.
int gfx_luz_canal_px(GLuint tex, float u, float v, unsigned char rgb[3]);
int gfx_tela_px(float u, float v, unsigned char rgb[3]);
int gfx_luz_canal_bmp(GLuint tex, const char *caminho);
int gfx_tela_bmp(const char *caminho);
// Fator da opacidade do vidro: 1,0 = os 78% de sempre (ajustes_vidro_opacidade / 0,78).
float gfx_vidro_opacidade(void);
// A luz de borda unica de todo vidro (gfx_vidro painel e gfx_vidro_chip ja a
// desenham; ver gfx.c).
void gfx_vidro_luz(GfxRect r, float raio, float a);
// Vidro chapado de chip/selo/trilho/fundo de foco: tom e alfa (`fundo` x
// opacidade) do gfx_vidro_painel, aro e luz de borda, sem passada de fundo.
void gfx_vidro_chip(GfxRect r, float raio, float fundo, float a);
void gfx_ambiente(float alfa);
// Pinta a luz que gfx_ambiente deixou pendente, se ainda houver (ver a nota
// em gfx.c). main.c chama no fim do desenho do quadro; e inofensivo repetir.
void gfx_ambiente_descarregar(void);
// O mesmo, sem pintar dentro dos `n` (<= 4) furos que paineis opacos cobrem em
// seguida com alfa 1 (o MESMO quad em faixas uSub: mesmo pixel fora deles).
void gfx_ambiente_descarregar_fora(const GfxRect *furos, int n);
// --- FUNDO DA HOME DINAMICA (GFX_FUNDO_DIN) ---------------------------------
// Um quad de tela cheia, OPACO e sem mistura, com um degrade vertical de uma
// cor: `topo` no alto e topo*`queda` na base. E o unico custo do fundo — nada
// e assado nem decodificado por troca de titulo. Substitui o clear.
// ADIADO ate o primeiro desenho do quadro: se ele e o destaque (GFX_VITRINE)
// de largura inteira, a partir do topo e com alfa 1, os dois saem numa passada
// opaca (GFX_VITRINE_DIN) e o fundo so e pintado onde o destaque nao chega;
// senao o fundo e pintado antes dele, como sempre. Mesmo pixel.
void          gfx_fundo_din_desenhar(const float topo[3], float queda);
// Um cartaz OPACO desenhado sobre o fundo da Dinamica adiado (Android): marca
// o retangulo (sem os cantos de `raioPx`) na profundidade para o fundo nao ser
// sombreado embaixo dele. No-op fora desse regime. Ver dinAdiado em gfx.c.
void          gfx_mascara_opaca(GfxRect r, float raioPx);
// Contorno de `esp` PIXELS por dentro de r: a borda de fora do anel e a borda
// de r, entao anel e miolo no mesmo rect dao uma borda so. `raio` e o de r,
// normalizado pela altura, como em gfx_cor.
void gfx_anel(GfxRect r, float raio, float esp,
              float cr, float cg, float cb, float ca);
// Arco do circulo inscrito em r: `esp` px por dentro, de `ini` (fracao de
// volta, 0 = topo) por `frac` voltas no sentido horario. Um quad (GFX_ANEL).
void gfx_arco(GfxRect r, float esp, float ini, float frac,
              float cr, float cg, float cb, float ca);
// Painel opaco grande de cor com canto arredondado: clear tesourado no miolo,
// faixa de borda pelo shader (mesmo pixel de gfx_cor; bem mais barato na GPU).
void gfx_cor_painel(GfxRect r, float raio, float cr, float cg, float cb, float ca);
// Anel de foco POR FORA de uma peca: `folga` px de vao e `esp` px de traco,
// concentrico com ela (o raio em pixels cresce folga + esp). `raio` e o da
// PECA. E o jeito de desenhar anel em volta de botao, cartao ou celula.
void gfx_anel_fora(GfxRect peca, float raio, float folga, float esp,
                   float cr, float cg, float cb, float ca);
// Cartao focado com material tipo vidro: mancha de accent atras, base escura
// translucida, lavagem em degrade por pixel e reflexo de topo muito leve.
// A area extra fica limitada a um unico item focado, nunca a tela inteira.
void gfx_cartao_foco_vidro(GfxRect r, float raio, float foco, float alfa,
                           float cr, float cg, float cb);
// --- INTERFACE DE VIDRO (Ajustes > Aparencia; ajustes_vidro()) ---------------
// O miolo comum do visual "vidro" (referencia: outro fork do app, em video):
// superficie translucida que deixa a arte de tras passar, borda de fio de
// cabelo, cantos generosos, e o foco marcado por CONTORNO BRANCO em vez de
// preenchimento na cor de realce ou brilho colorido. Quem decide se usa e a
// tela (ajustes_vidro()); estas funcoes so desenham, e desligado nenhuma delas
// e chamada — o desenho antigo fica byte a byte o mesmo.
//
// SEM DESFOQUE DE FUNDO, de proposito: o vidro real pede uma copia do quadro
// ATRAS de cada painel, e nesta GPU (C9) ate um quad de tela cheia com blend
// derrubou o quadro (ver a luz imersiva, assada em 320x180). Aqui e uma cor
// unica com alfa — a arte de tras aparece por ela, so que nitida — e o custo
// e o de um gfx_cor por painel.
//
// Superficie de um painel/pilula/linha: cinza-frio translucido + aro de 1,5 px
// a 14 % de branco. `fundo` e o alfa do miolo (0,55 em pilula sobre pagina
// escura; ~0,78 num painel flutuante sobre arte, onde o texto precisa de
// contraste). Em repouso 0,16 x 0,55 sobre o fundo #0D0D0D da 0,11 — o cinza
// das pilulas do video de referencia.
// Scoped pre-blurred full-screen artwork, independent of the glass toggle.
// Returns the previous source; restore it after drawing the owning screen.
GLuint gfx_vidro_fonte_fixa(GLuint tex);
void gfx_vidro_folha(GfxRect r, float raio, float a);
// Borda de vidro (Liquid Glass): fio claro de 1 px e a luz que escorre do alto,
// por cima do miolo. Nada em efeitos minimos (TVs fracas: so translucido liso).
void gfx_vidro_borda(GfxRect r, float raio, float a);
void gfx_vidro_superficie(GfxRect r, float raio, float a);
// Fio de REPOUSO de um cartao/painel de vidro. Obedece a "Contorno do vidro";
// o anel de FOCO (gfx_vidro_foco) nunca passa por aqui.
void gfx_vidro_aro(GfxRect r, float raio, float esp, float cr, float cg, float cb, float ca);
void gfx_vidro_painel(GfxRect r, float raio, float fundo, float a);
// FOCO de uma pilula/linha JA desenhada com gfx_vidro_painel: o miolo clareia
// (branco a 9 %) e o contorno de 2 px em branco acende com a mola `foco`.
void gfx_vidro_foco(GfxRect r, float raio, float foco, float a);
// FOCO de um cartaz/cartao de arte: contorno na cor do realce (branco no padrao) de 3 px POR FORA, com 2 px
// de vao — limpo, sem brilho colorido nem sombra.
void gfx_vidro_cartao(GfxRect r, float raio, float foco, float a);
// "Foco na cor de destaque": vidro tingido com o realce no lugar da barra cinza
// de uma linha/pilula/aba. Devolve 1 se desenhou; 0 (nada desenhado) com o
// ajuste desligado ou sem vidro — o chamador entao desenha o cinza de sempre.
int gfx_foco_placa(GfxRect r, float raio, float foco, float a);
// Alpha of the accent tint in that plate (lower for light accents, for text contrast).
float gfx_foco_placa_alfa(float cr, float cg, float cb);
// FOCO de botao de acao e item de menu: a pilula vira BRANCA cheia (a mola
// interpola a opacidade), e o texto passa a escuro em foco >= 0,5 — quem chama
// escolhe a tinta com gfx_vidro_tinta.
// Com vidro e realce colorido ela e vidro tingido: realce a GFX_VIDRO_ACENTO_ALFA
// (cor viva), aro claro e brilho de topo como as pilulas de vidro em repouso.
#define GFX_VIDRO_ACENTO_ALFA 0.92f
void gfx_vidro_pilula_cheia(GfxRect r, float raio, float foco, float a);
// Tinta do texto sobre a superficie de vidro: escura sobre a pilula branca do
// foco, clara (235) no resto.
int  gfx_vidro_tinta(float foco);
// Como gfx_vidro_painel, mas com lavagem e aro na COR DO REALCE: a acao
// principal em repouso (Play) e as marcas de estado mantem o tema visivel.
void gfx_vidro_painel_acento(GfxRect r, float raio, float fundo, float a);
// A luz de realce dos paineis flutuantes (ver GFX_LUZ): `raio` e o dos cantos
// do painel, na mesma fracao do menor lado que gfx_cor usa; (cx, cy) e o
// centro da luz em pixels RELATIVOS ao canto superior esquerdo de `r` (pode
// ser negativo, para a luz entrar de fora); `alcance` e onde ela morre, em
// pixels. Um desenho do tamanho do painel — conte-o como tal em gfx_fill.
void gfx_luz_canto(GfxRect r, float raio, float cx, float cy, float alcance,
                   float cr, float cg, float cb, float ca);
// Zera cor E alpha do retangulo, com blend desligado, abrindo a superficie para
// o plano de video que fica atras dela. Ver video.h.
void gfx_furo(GfxRect r);
// O mesmo furo com cantos arredondados (raio em fracao do menor lado, como o
// gfx_cor): o que fica fora do SDF mantem a alpha da superficie e o plano de
// video so aparece pela area arredondada.
void gfx_furo_raio(GfxRect r, float raio);
// Furo parcial: cor e alfa do retangulo vezes 1 - a (a >= 0,995 e o gfx_furo).
void gfx_furo_alfa(GfxRect r, float a);
// Furo com cantos + veu so-veu do GFX_JANELA (cor 0) numa passada: ver gfx.c.
void gfx_furo_raio_veu(GfxRect r, float raio, float veu, float rolada, float exp, float ca);
void gfx_textura(GfxRect r, GLuint tex);
// Esmaece o quadro inteiro ja desenhado para `a` (0 = transparente, mostra o
// plano de video de baixo). Usado no dissolve do voo da ilha.
void gfx_dissolver_tela(float a);
// Card carregando: a superficie do esqueleto com a luz passando (GFX_ESQUELETO).
void gfx_esqueleto(GfxRect r, float raio, float cr, float cg, float cb, float ca);

#endif
