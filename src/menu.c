// Rail lateral fixa do Nuvio 1.0.1 legacy, com overlay expansível para
// navegação por D-pad.
//
// Duas animacoes independentes, e a separacao e o que da o movimento certo:
//   - `desliza` tira a barra da borda esquerda (posicao);
//   - `expande` troca a largura de "so icone" para "icone + rotulo".
// No tvOS a barra recolhida mostra apenas os icones e so alarga quando ganha o
// foco. Aqui ela nasce fora da tela, entao os dois acontecem quase juntos — mas
// com molas de rigidez diferente, de forma que a largura ATRASA em relacao a
// entrada. E esse atraso que produz a leitura "entrou e entao se abriu"; com uma
// mola so, a barra aparece ja no tamanho final e o efeito some.
//
// Icones derivados dos SVGs originais do sidebar, com alpha e recortes reais.
#include "menu.h"
#include "perfis.h"
#include "tex_cache.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "ajustes.h"
#include "botoes.h"
#include "ponteiro.h"

// Larguras: a recolhida cabe so o icone; a aberta e a da barra do tvOS, larga o
// bastante para o rotulo mais comprido ("Biblioteca") nao encostar na borda.
#define NV_MENU_W_ICONE   NV_LEGACY_RAIL_W
#define NV_MENU_W_ABERTO  392.0f
#define NV_MENU_LINHA_H    88.0f
#define NV_MENU_ICONE      38.0f
// O centro do icone e o mesmo nas duas larguras: no aparelho o icone NAO anda
// quando a barra abre, so o rotulo entra ao lado dele. Se o icone deslizasse
// junto, a abertura viraria um empurrao lateral em vez de uma revelacao.
#define NV_MENU_ICONE_CX  (NV_MENU_W_ICONE * 0.5f)
#define NV_MENU_ROTULO_X  112.0f
#define NV_MENU_PILL_PAD   20.0f
#define NV_MENU_RAIO_PILL  0.20f
// Quanto o conteudo a direita escurece com a barra aberta. Sem isso o menu
// disputa atencao com a arte do hero, que e clara e ocupa a tela toda.
#define NV_MENU_VEU        0.58f
#define NV_MENU_INATIVO      0.72f
// TEMPO DE ABRIR E DE FECHAR, com relogio proprio.
//
// A referencia nao tem barra lateral nenhuma na home — LEFT e UP a partir do
// primeiro card sobem para os botoes do hero e param ali; nao ha rail para
// medir. O unico overlay comparavel que consegui abrir la foi a folha de
// contexto da tecla MENU, e ela da o tempo e a FORMA do veu:
//
//   fechar (medida limpa, 10 quadros seguidos, sem perda):
//     16ms 0,00 | 33 0,09 | 50 0,19 | 66 0,29 | 83 0,38 | 100 0,49
//     117 0,60 | 133 0,73 | 151 0,84 | 166 0,98
//   ou seja RAMPA RETA, ~0,10 a cada 17 ms, terminando em ~150 ms de percurso.
//   abrir: mesma rampa reta, ~0,0044/ms, o que da ~230 ms de percurso.
//
// Dois achados que a mola nao reproduzia: o veu e LINEAR (mola nenhuma e), e
// FECHAR e bem mais rapido que ABRIR. NV_MOLA_TELA (9,0) dava 333 ms simetricos
// e com a partida mais veloz do percurso, que e o oposto de uma rampa.
#define NV_MENU_ABRIR_MS  230.0f
#define NV_MENU_FECHAR_MS 150.0f
// A largura continua ATRASADA em relacao a entrada — e o efeito "entrou e
// entao se abriu" descrito no topo do arquivo. Nao ha medida da referencia para
// ele (la nao existe esta barra); o que mudou foi so o tempo total, agora
// amarrado ao mesmo relogio em vez de uma mola de rigidez solta.
#define NV_MENU_EXP_LENTO  1.6f

// "Inicio" sem acento era erro de portugues NA TELA. E "Busca", nao "Buscar":
// os outros tres sao substantivos (Biblioteca, Ajustes) e o verbo destoava.
// Rotulos e ordem conferidos na referencia.
static const char *ROTULOS[MENU_N] = { "Início", "Explorar", "Guia TV", "Busca", "Biblioteca", "Agenda", "Perfil e Stats", "Ajustes" };

// RODAPE: quem esta usando o app, e a porta para trocar. Ele e um item de
// FOCO a mais, no indice MENU_N — nao entrou no enum de proposito, porque
// trocar de perfil nao e uma aba do app e ninguem deve poder "navegar" para
// ela como destino.
#define NV_MENU_RODAPE_H   112.0f
#define NV_MENU_AVATAR      56.0f
#define NV_MENU_FOCOS      (MENU_N + 1)
#define MENU_RODAPE         MENU_N

static int   pediuTrocar = 0;
static int   aberto  = 0;
static int   destino = MENU_INICIO;
static int   linha   = MENU_INICIO;   // destaque; so vira destino ao escolher
static int   mudou   = 0;
static float desliza = 0.0f;
static float expande = 0.0f;
static float animFoco[NV_MENU_FOCOS];
static void icone(int d, float cx, float cy, float s, float r, float g, float b, float a);
// Tinta de texto e icone sobre o accent vem da mesma regra dos botoes.
static void desenhaRodape(float px, float w, float alpha, float foco);

// A rail mantem o estado atual em tom baixo; o foco navegavel ganha a mesma
// pilula solida de accent e a mesma luz macia dos botoes primarios.
static void corFocoMenu(float *r, float *g, float *b) {
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  if (ajustes_acento_tinta(NULL, NULL, NULL) < 0.5f) {
    *r = 0.105f; *g = 0.112f; *b = 0.132f;
  } else {
    *r = 0.088f + ar * 0.055f;
    *g = 0.075f + ag * 0.035f;
    *b = 0.090f + ab * 0.045f;
  }
}

// Foco solido na cor do tema; so a luz macia do botao primario aparece atras.
static void focoMenu(GfxRect pill, float f, float alpha) {
  float cr, cg, cb;
  if (f <= 0.01f || alpha <= 0.01f) return;
  ajustes_acento_tinta(&cr, &cg, &cb);
  botao_luz(pill, f, alpha);
  gfx_cor(pill, NV_MENU_RAIO_PILL,
          anim_mistura(0.14f, cr, f),
          anim_mistura(0.15f, cg, f),
          anim_mistura(0.17f, cb, f), alpha);
}

// O legacy deixa a rail de 144px sempre visível. O menu expandido é uma
// camada adicional; não deslocamos o conteúdo quando ele fecha.
// PONTEIRO (#99). Passar por cima da rail ABRE a barra ja com o destaque na
// linha sob o cursor — o mesmo que ESQUERDA e depois cima/baixo. O clique e o
// OK de sempre (escolher). Com a barra aberta, clicar fora dela fecha, como o
// Voltar.
static void ponteiroLinha(int i, int b) {
  (void)b;
  if (i < 0 || i >= NV_MENU_FOCOS) return;
  if (!aberto) menu_abrir();
  linha = i;
}
static void ponteiroFora(int a, int b) { (void)a; (void)b; menu_fechar(); }
static void alvosDasLinhas(float x, float w) {
  float y = (NV_TELA_H - MENU_N * NV_MENU_LINHA_H) * 0.5f;
  if (!ponteiro_ativo()) return;
  for (int i = 0; i < MENU_N; i++, y += NV_MENU_LINHA_H)
    ponteiro_alvo(x, y, w, NV_MENU_LINHA_H, ponteiroLinha, NULL, i, 0);
  ponteiro_alvo(x, NV_TELA_H - NV_MARGEM_Y - NV_MENU_RODAPE_H, w, NV_MENU_RODAPE_H,
                ponteiroLinha, NULL, MENU_RODAPE, 0);
}

static void desenhaRailFixa(void) {
  GfxRect painel = { 0, 0, NV_LEGACY_RAIL_W, NV_TELA_H };
  gfx_cor(painel, 0.0f, 0.055f, 0.058f, 0.064f, 1.0f);
  float sr, sg, sb;
  corFocoMenu(&sr, &sg, &sb);
  float y = (NV_TELA_H - MENU_N * NV_MENU_LINHA_H) * 0.5f;
  for (int i = 0; i < MENU_N; i++, y += NV_MENU_LINHA_H) {
    int atual = (i == destino);
    float lum = atual ? 0.94f : NV_MENU_INATIVO;
    if (atual) {
      GfxRect marca = { 18.0f, y + 12.0f, NV_LEGACY_RAIL_W - 36.0f,
                        NV_MENU_LINHA_H - 24.0f };
      // A tela ativa precisa continuar legivel quando a rail esta recolhida:
      // o realce e o mesmo acento usado pelo foco expandido e pelos demais
      // controles, em vez de uma pilula cinza que parece inerte.
      gfx_cor(marca, NV_MENU_RAIO_PILL, sr, sg, sb, 0.92f);
    }
    icone(i, NV_MENU_ICONE_CX, y + NV_MENU_LINHA_H * 0.5f,
          NV_MENU_ICONE, lum, lum, lum, 0.95f);
  }
  desenhaRodape(0.0f, NV_LEGACY_RAIL_W, 0.95f, 0.0f);
}

int menu_iniciar(void) {
  aberto = 0; destino = MENU_INICIO; linha = MENU_INICIO; mudou = 0;
  desliza = 0.0f; expande = 0.0f;
  for (int i = 0; i < MENU_N; i++) animFoco[i] = 0.0f;
  return 1;
}

void menu_abrir(void) {
  if (aberto) return;
  // O destaque comeca sempre no destino em vigor, nunca onde ficou da ultima
  // vez: a barra e um mapa de onde voce esta, e abrir com o destaque em outro
  // item faria o usuario ler que ja mudou de tela.
  linha = destino;
  aberto = 1;
}
void menu_fechar(void) { aberto = 0; linha = destino; }

int menu_aberto(void)  { return aberto; }
int menu_visivel(void) { return 1; }
int menu_destino(void) { return destino; }
void menu_definir_destino(int d) {
  if (d < 0 || d >= MENU_N) return;
  destino = d;
  if (!aberto) linha = d;
}
int menu_mudou_destino(void) { int m = mudou; mudou = 0; return m; }
const char *menu_rotulo(int d) {
  return (d >= 0 && d < MENU_N) ? ROTULOS[d] : "";
}

// Confirma o destaque e recolhe. DIREITA tambem passa por aqui: no aparelho a
// barra nao "cancela" ao sair pela direita — o item destacado e o que o usuario
// esta olhando, e desfazer a escolha no caminho de volta seria surpresa.
static void escolher(void) {
  if (linha == MENU_RODAPE) {
    // O rodape nao troca de destino: ele pede a tela de escolha de perfil.
    pediuTrocar = 1;
    aberto = 0;
    linha = destino;
    return;
  }
  if (linha != destino) { destino = linha; mudou = 1; }
  aberto = 0;
}

int menu_pediu_trocar(void) { int p = pediuTrocar; pediuTrocar = 0; return p; }

void menu_evento(const SDL_Event *e) {
  if (!aberto || e->type != SDL_KEYDOWN) return;
  SDL_Keycode k = e->key.keysym.sym;

  // Mesmo conjunto de teclas de "voltar" que o detalhe aceita: no controle e o
  // Back, no teclado cada pessoa alcanca uma diferente.
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE) { menu_fechar(); return; }

  if (k == SDLK_RIGHT || k == SDLK_RETURN || k == SDLK_KP_ENTER) { escolher(); return; }
  // Sem rotacao nas pontas: a barra e curta e o usuario ve as quatro linhas de
  // uma vez, entao dar a volta no fim da lista le como falha, nao como atalho.
  if (k == SDLK_DOWN && linha < NV_MENU_FOCOS - 1) linha++;
  else if (k == SDLK_UP && linha > 0)       linha--;
  // ESQUERDA morre aqui de proposito: a barra ja e a borda da tela.
}

void menu_atualizar(float dt, Uint32 agora) {
  (void)agora;
  // Recolhido e assentado nao custa nada: nem mola, nem laco pelos destinos.
  if (!aberto && desliza < 0.002f) {
    if (desliza != 0.0f) { desliza = 0.0f; expande = 0.0f; }
    return;
  }
  float alvo = aberto ? 1.0f : 0.0f;
  float ms   = aberto ? NV_MENU_ABRIR_MS : NV_MENU_FECHAR_MS;
  desliza = anim_rampa(desliza, alvo, dt, ms);
  expande = anim_rampa(expande, alvo, dt, ms * NV_MENU_EXP_LENTO);
  for (int i = 0; i < NV_MENU_FOCOS; i++) {
    float a = (aberto && i == linha) ? 1.0f : 0.0f;
    animFoco[i] = anim_mola(animFoco[i], a, dt,
                            a > animFoco[i] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  }
}

// Mesmos vetores do sidebar oficial, rasterizados no build e tintados pelo shader.
static void icone(int d, float cx, float cy, float s, float r, float g, float b, float a) {
  // `portal` ja e um SVG embarcado e le como entrada para uma descoberta;
  // manter o icone real evita inventar um glifo SDF e duplicar o de Busca.
  static const char *nomes[MENU_N] = {"menu_home", "portal", "menu_guide", "menu_search", "menu_library", "menu_agenda", "menu_profile", "menu_settings"};
  if (d < 0 || d >= MENU_N) return;
  gfx_icone((GfxRect){cx-s*.5f, cy-s*.5f, s, s}, nomes[d], r, g, b, a);
}


// Cor do avatar a partir do "#RRGGBB" que a conta guarda. Sem cor legivel, o
// azul do padrao do web.
static void corAvatar(const char *hex, float *r, float *g, float *b) {
  unsigned v = 0;
  *r = 0.12f; *g = 0.53f; *b = 0.90f;
  if (!hex || hex[0] != '#' || strlen(hex) < 7) return;
  if (sscanf(hex + 1, "%6x", &v) != 1) return;
  *r = ((v >> 16) & 255) / 255.0f;
  *g = ((v >> 8) & 255) / 255.0f;
  *b = (v & 255) / 255.0f;
}

// A INICIAL do nome, respeitando UTF-8: um nome comecado por acento tem dois
// bytes, e cortar no primeiro desenha lixo.
static void inicialDe(const char *nome, char *dst, size_t tam) {
  if (tam < 3) { if (tam) dst[0] = 0; return; }
  dst[0] = (nome && nome[0]) ? nome[0] : '?';
  dst[1] = 0;
  if (nome && (unsigned char)nome[0] >= 0xC0 && nome[1]) { dst[1] = nome[1]; dst[2] = 0; }
}

// Rodape: quem esta usando, e a porta para trocar. Desenha nas DUAS larguras —
// recolhida mostra so o avatar (e a unica coisa que cabe em 144px), aberta
// mostra nome e a acao.
static void desenhaRodape(float px, float w, float alpha, float foco) {
  const ContaPerfil *p = perfis_item_ativo();
  // Acima da area segura, nao colado na base: numa TV os ultimos 60px podem
  // estar fora do painel (overscan), e o nome do usuario e justamente o que
  // some primeiro.
  float y = NV_TELA_H - NV_MARGEM_Y - NV_MENU_RODAPE_H;
  float cx = px + NV_MENU_ICONE_CX;
  float cy = y + NV_MENU_RODAPE_H * 0.5f;
  float cr, cg, cb;
  char ini[4];
  GfxRect av;

  if (alpha <= 0.01f) return;

  // Foco = pilula na COR DE REALCE com texto escuro, sem anel — a mesma regra
  // dos itens do menu (ver a nota la) e das linhas de Ajustes.
  if (foco > 0.01f) {
    GfxRect pill = { px + NV_MENU_PILL_PAD, y + 8.0f,
                     w - NV_MENU_PILL_PAD * 2.0f, NV_MENU_RODAPE_H - 16.0f };
    focoMenu(pill, foco, alpha);
  }

  av.x = cx - NV_MENU_AVATAR * 0.5f;
  av.y = cy - NV_MENU_AVATAR * 0.5f;
  av.w = av.h = NV_MENU_AVATAR;

  // FOTO quando a conta tem uma; senao o circulo com a inicial, que e o mesmo
  // que o app web mostra quando `avatar_url` e nulo — e nesta conta ele e.
  { GLuint tex = (p && p->avatarUrl[0]) ? tex_obter(p->avatarUrl) : 0;
    if (tex) {
      gfx_tex_aspect_atual = 1.0f;
      gfx_rect(av, tex, GFX_CARD, 0, 0, 0, 0.5f, 0, 0, 0, alpha);
    } else {
      corAvatar(p ? p->corHex : NULL, &cr, &cg, &cb);
      gfx_cor(av, 0.5f, cr, cg, cb, alpha);
      inicialDe(p ? p->nome : NULL, ini, sizeof ini);
      { TxtLinha l = txt_linha(TXT_HEADLINE, ini, 255, 255, 255, 255);
        txt_desenhar_alpha(l, av.x + (av.w - l.w) * 0.5f,
                           av.y + (av.h - l.h) * 0.5f, alpha); } } }

  // Nome e acao so aparecem com a barra aberta: em 144px nao cabe texto, e
  // espremer o nome ali seria pior que nao mostrar.
  { float aTexto = expande * expande * alpha;
    if (aTexto > 0.01f) {
      // Texto ja rasterizado nao muda de cor: troca no meio da mola.
      int emFoco = foco > 0.5f;
      float tinta = ajustes_acento_tinta(NULL, NULL, NULL);
      int c = emFoco ? (int)(tinta * 255.0f + 0.5f) : 184;
      int c2 = emFoco ? ajustes_tinta_foco2() : 150;
      TxtLinha nome = txt_linha_corta(TXT_BODY, p ? p->nome : "Sua conta",
                                      c, c, c, 255,
                                      NV_MENU_W_ABERTO - NV_MENU_ROTULO_X - 28.0f);
      TxtLinha acao = txt_linha(TXT_CAPTION, "Trocar de usuário", c2, c2, c2 + (emFoco ? 0 : 10), 255);
      txt_desenhar_alpha(nome, px + NV_MENU_ROTULO_X, cy - nome.h - 2.0f, aTexto);
      txt_desenhar_alpha(acao, px + NV_MENU_ROTULO_X, cy + 4.0f, aTexto);
    } }
}

void menu_desenhar(Uint32 agora) {
  (void)agora;
  // Rail fixa sempre presente, como no shell legacy. O overlay expandido só
  // entra em cena quando o menu foi solicitado.
  // `collapseSidebar`: com a barra RECOLHIDA o web nao desenha rail nenhuma —
  // `.home-nav-list` fica com largura 0 e nao ocupa fluxo; ela so aparece como
  // camada quando ganha foco. O port ja movia o conteudo para 104 nesse caso
  // (ajustes_conteudo_x), mas continuava pintando os 144px da rail por baixo
  // dele: uma faixa escura sob o primeiro card, sem nada em cima.
  if (!aberto && desliza < .002f && !ajustes_rail_recolhida()) desenhaRailFixa();
  if (!aberto && desliza < 0.002f) {
    // Recolhida, a rail nao existe na tela; uma faixa na borda faz o papel
    // dela para o ponteiro, como o ESQUERDA na primeira coluna.
    alvosDasLinhas(0.0f, ajustes_rail_recolhida() ? 28.0f : NV_MENU_W_ICONE);
    return;
  }

  float w = anim_mistura(NV_MENU_W_ICONE, NV_MENU_W_ABERTO, anim_suave(expande));
  // O VEU usa a rampa CRUA: a medida da referencia e uma reta (ver
  // NV_MENU_ABRIR_MS). A POSICAO do painel usa a mesma rampa suavizada — um
  // bloco desse tamanho parando de vez no fim do percurso le como corte, e a
  // referencia comeca devagar em tudo que desliza (ver anim_mola2 em anim.h).
  float entrada = anim_suave(desliza);
  float px = -w * (1.0f - entrada);

  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  gfx_cor(tela, 0.0f, 0, 0, 0, NV_MENU_VEU * desliza);

  // Painel quase opaco e um pouco mais escuro que NV_COR_FUNDO: encostado no
  // fundo da home ele precisa de uma aresta propria, senao a barra parece um
  // pedaco da tela que escureceu sozinho.
  // Painel flutuante neutro, com o acento reservado a selecao. Assim a cor
  // do tema nao tinge a tela toda enquanto a pessoa percorre as secoes.
  GfxRect painel = { px, 24.0f, w, NV_TELA_H - 48.0f };
  float ar_, ag_, ab_; ajustes_acento(&ar_, &ag_, &ab_);
  if (aberto) {
    ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, ponteiroFora, 0, 0);
    ponteiro_alvo(painel.x, painel.y, painel.w, painel.h, NULL, NULL, 0, 0);
    alvosDasLinhas(px, w);
  }
  gfx_cor(painel, 28.0f / painel.h, 0.055f, 0.058f, 0.068f, 0.965f * entrada);

  // Tudo daqui para baixo fica preso ao painel. Sem o recorte, o rotulo — que e
  // desenhado no x fixo do texto — vaza para o conteudo enquanto a barra ainda
  // esta estreita, e ve-se a palavra aparecendo fora dela.
  gfx_recorte(px, 0, w, NV_TELA_H);

  float y = (NV_TELA_H - MENU_N * NV_MENU_LINHA_H) * 0.5f;
  for (int i = 0; i < MENU_N; i++, y += NV_MENU_LINHA_H) {
    float f = animFoco[i];
    float cy = y + NV_MENU_LINHA_H * 0.5f;

    if (i == destino && f < .99f) {
      // ONDE VOCE ESTA: um traco na cor de realce a esquerda do icone, em vez
      // da pilula cinza — le como "aba ativa" e nao como um segundo foco.
      GfxRect traco = { px + 20.0f, cy - 16.0f, 4.0f, 32.0f };
      gfx_cor(traco, 0.5f, ar_, ag_, ab_, .68f * (1-f) * desliza);
    }
    if (f > 0.01f) {
      GfxRect pill = { px + NV_MENU_PILL_PAD, y + 7.0f,
                       w - NV_MENU_PILL_PAD * 2.0f, NV_MENU_LINHA_H - 14.0f };
      // Preenchimento accent como no primario, com glow de botao por tras.
      focoMenu(pill, f, desliza);
    }

    // Tres estados, e os tres precisam existir: em foco, destino em vigor e
    // o resto (cinza). Com so dois
    // estados, abrir o menu apaga a indicacao de onde voce estava.
    //
    // Sobre accent colorido a tinta e branca; so o branco pede tinta escura.
    int atual = (i == destino);
    int emFoco = f > 0.5f;
    float tinta = ajustes_acento_tinta(NULL, NULL, NULL);
    float lum = emFoco ? tinta : (atual ? 0.92f : NV_MENU_INATIVO);
    float alpha = desliza * anim_mistura(atual ? 1.0f : 0.85f, 1.0f, f);

    icone(i, px + NV_MENU_ICONE_CX, cy, NV_MENU_ICONE, lum, lum, lum, alpha);

    // O rotulo entra com a largura, nao antes dela: `expande` ao quadrado
    // segura a palavra ate a barra ter espaco de verdade, senao ela nasce
    // espremida contra o icone.
    float aRot = expande * expande * entrada;
    if (aRot > 0.01f) {
      int c = (int)(lum * 255.0f + 0.5f);
      TxtLinha l = txt_linha_corta(TXT_BODY, ROTULOS[i], c, c, c, 255,
                                   NV_MENU_W_ABERTO - NV_MENU_ROTULO_X - 28);
      txt_desenhar_alpha(l, px + NV_MENU_ROTULO_X, cy - l.h * 0.5f, aRot);
    }
  }

  desenhaRodape(px, w, entrada, animFoco[MENU_RODAPE]);

  gfx_sem_recorte();
}
