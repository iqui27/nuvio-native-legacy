// Camada de desenho: um shader unico com SDF de retangulo arredondado, capaz de
// desenhar card com textura, sombra difusa, retangulo de cor e o hero com
// gradiente. Um so programa GL evita troca de estado a cada primitiva, que e o
// que mais custa nesta GPU.
#ifndef NV_GFX_H
#define NV_GFX_H
#include "gl_compat.h"

typedef enum {
  GFX_CARD   = 0,  // textura com cantos, parallax e especular
  GFX_SOMBRA = 1,  // sombra difusa atras do card focado
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
  // um anel e `abs(d) < espessura` —, entao serve para retangulo arredondado
  // tanto quanto para circulo (raio 0.5 = circulo).
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
  GFX_NMODOS = 24
} GfxModo;

typedef struct {
  float x, y, w, h;
} GfxRect;

// Proporcao (w/h) da textura a desenhar. 0 = mapeia direto (texto, veu).
// Definir ANTES de gfx_rect para que a arte seja recortada, nunca esticada.
extern float gfx_tex_aspect_atual;
// Opacidade de grupo: deve voltar a 1 ao terminar o grupo.
extern float gfx_opacidade_grupo;

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
void gfx_snap_comecar(void);   // redireciona o desenho para o snapshot
void gfx_snap_terminar(void);  // volta para a tela
void gfx_snap_desenhar(void);  // pinta o snapshot ocupando a tela toda
void gfx_snap_encerrar(void);

void gfx_tamanho_alvo(int w, int h);   // drawable real, para restaurar viewport
int  gfx_iniciar(void);
void gfx_encerrar(void);

// O gfx_rect lembra a ultima textura que ele mesmo bindou e pula rebinds
// repetidos. Quem binda ou destroi textura POR FORA dele precisa avisar:
// passe o nome destruido, ou 0 para "esqueca tudo" (apos um upload).
void gfx_tex_esquecer(GLuint tex);

// Desenha um retangulo. `foco` 0..1 controla especular/sombra; `parx/pary`
// deslocam a arte dentro do card (parallax); `raio` em fracao do menor lado.
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
extern int    gfx_n_cheio;   // desenhos cobrindo >= 50% da tela
void gfx_novo_quadro(void);

void gfx_rect(GfxRect r, GLuint tex, GfxModo modo, float foco,
              float parx, float pary, float raio,
              float cr, float cg, float cb, float ca);

// Atalhos legiveis para os casos comuns.
void gfx_cor(GfxRect r, float raio, float cr, float cg, float cb, float ca);
// Zera cor E alpha do retangulo, com blend desligado, abrindo a superficie para
// o plano de video que fica atras dela. Ver video.h.
void gfx_furo(GfxRect r);
void gfx_textura(GfxRect r, GLuint tex);

#endif
