// Home nativa compatível com a interface moderna do Nuvio 1.0.1 legacy:
// hero no topo, rail fixa à esquerda e fileiras horizontais de posters. A
// infraestrutura nativa cuida de cache assíncrono, foco e transições.
#include "home.h"
#include "idioma.h"
#include "catordem.h"
#include "fileiras.h"
#include "continuar.h"
#include "vertudo.h"
#include "ctxmenu.h"
#include "marco.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "focus.h"
#include "anim.h"
#include "layout.h"
#include "ajustes.h"
#include "catalogo.h"
#include "colecoes.h"
#include "badges.h"
#include "extras.h"
#include "diretor.h"
#include "descoberta.h"
#include "dados.h"
#include <strings.h>
// Declarado a mao em vez de incluir detail.h: aquele header inclui ESTE (por
// causa do HomeItem), e o ciclo so nao explode por causa das guardas. Uma
// funcao de uma linha nao vale amarrar os dois arquivos.
float detail_progresso(void);
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

#define MAX_ARTE   64
// 16, o teto do web para ESTE runtime: HOME_MAX_ROWS_LEGACY_TV em
// js/ui/screens/home/homeConstants.js, que e o ramo escolhido por
// isLegacyTvRuntime(). O HOME_MAX_ROWS_DEFAULT de 40 e do navegador de mesa.
#define MAX_FIL    32
// 13 e nao 12: sao 12 CARTAZES mais a coluna do card "Ver tudo", que ocupa a
// posicao seguinte a ultima arte. Com 12 aqui, animFoco[r][12] escrevia fora do
// vetor — o card nunca acendia ao receber foco e a memoria do vizinho era
// corrompida em silencio.
#define MAX_CARDS 33

typedef struct {
  char titulo[96];
  TipoFileira tipo;
  int n;
  // Primeiro item DESTA fileira no catalogo. Antes o desenho fazia `r * 8 + c`,
  // ou seja, cada fileira era uma janela fixa de 8 no vetor plano — o que so
  // funcionava porque as fileiras eram quatro e cravadas. Com as fileiras
  // vindo dos catalogos dos addons cada uma tem tamanho proprio.
  int ini;
  // O card "Ver tudo" ocupa a coluna `n` (a seguinte a ultima arte). Guardado
  // por fileira porque so as que vieram de catalogo de addon o tem.
  int verTudo;
  char base[600], catId[96];
  // "movie" | "series" do CATALOGO. O `tipo` acima e a forma do card
  // (retrato/deitado), que e outra coisa — nao da para deduzir um do outro.
  char catTipo[8];
  // Chave do catalogo desta fileira. Existe para o foco sobreviver a uma
  // republicacao: a descoberta agora publica a cada fileira que chega da rede,
  // e reencontrar por INDICE nao serve — uma fileira nova pode entrar no meio,
  // porque a ordem sai de art/fileiras.txt.
  char chave[192];
  int folders[MAX_CARDS];
  int stackN;
  // Fator de TAMANHO desta fileira, escolhido em Ajustes (fileiras.c). Fica no
  // Fileira e nao numa consulta por chave dentro do desenho: larguraDe/alturaDe
  // sao chamadas varias vezes por fileira por QUADRO, e cada consulta por chave
  // custa um mutex e uma varredura de 64 strings. 0 = nunca preenchido, lido
  // como 1.0 — e o caso da tabela de reserva abaixo e das fileiras montadas com
  // `Fileira v={0}`.
  float escala;
} Fileira;

static char bd[MAX_ARTE][512];    int nBd = 0;    // backdrops 16:9
static char pst[MAX_ARTE][512];   int nPst = 0;   // posters 2:3

// RESERVA, e so isso: e o que a home mostra enquanto a rede nao respondeu, ou
// quando nao respondeu nenhuma. As fileiras de verdade vem de cat_fileira(),
// montadas em descoberta.c a partir dos catalogos que os addons declaram.
// SEM "Continuar assistindo" AQUI, e a ausencia e o conserto.
//
// Esta tabela e a reserva mostrada enquanto a rede nao respondeu, e ela aponta
// para as primeiras posicoes do catalogo DO PACOTE. Chamar essas posicoes de
// "Continuar assistindo" dizia ao usuario que aqueles eram OS TITULOS DELE pela
// metade — quando sao os quarenta titulos de demonstracao de quem empacotou. No
// primeiro arranque, antes de a conta responder, a home abria com o "continue
// assistindo" de um estranho. E o issue #19.
//
// As outras tres continuam: "Popular" e "Em alta" sao uma vitrine e nao afirmam
// pertencer a ninguem. A fileira de continuar so nasce de progresso de verdade
// — progresso.c, a conta ou o Trakt — em montarContinuar (descoberta.c).
static Fileira fileiras[MAX_FIL] = {
  { "Popular - Filme",      FILEIRA_NORMAL,   8, 0  },
  { "Popular - S\xc3\xa9rie", FILEIRA_NORMAL, 8, 8  },
  { "Em alta",              FILEIRA_NORMAL,   8, 16 },
};
static int nFileiras = 3;
static int retomarIndice = -1;
static char retomarId[64];
static unsigned retomarRev, retomarAplicada;
static int pedidoSocial;
static int pedidoPessoaSocial;
static CatItem pessoaSocial;
int home_pediu_pessoa_social(CatItem *saida) {
  if (!pedidoPessoaSocial) return 0;
  pedidoPessoaSocial=0; if(saida)*saida=pessoaSocial; return 1;
}
int home_pediu_social(void) { int v=pedidoSocial;pedidoSocial=0;return v; }

// Classifica somente os nomes públicos do catálogo. Nunca inspeciona a URL
// (que pode conter tokens) nem inventa premiações ou disponibilidade.
static int contemNome(const char *nome, const char *termo) {
  size_t n = strlen(termo);
  for (; nome && *nome; nome++) {
    size_t i;
    for (i = 0; i < n && nome[i] &&
         tolower((unsigned char)nome[i]) == (unsigned char)termo[i]; i++) {}
    if (i == n) return 1;
  }
  return 0;
}
static TipoFileira perfilCatalogo(const char *nome) {
  static const char *premios[] = {"oscar", "academy", "award", "premia", "cannes", "golden globe"};
  static const char *servicos[] = {"netflix", "disney", "prime video", "amazon", "apple tv", "hbo", "max -", "paramount", "globoplay", "mubi", "crunchyroll"};
  for (size_t i = 0; i < sizeof premios / sizeof premios[0]; i++)
    if (contemNome(nome, premios[i])) return FILEIRA_COLECAO;
  for (size_t i = 0; i < sizeof servicos / sizeof servicos[0]; i++)
    if (contemNome(nome, servicos[i])) return FILEIRA_SERVICO;
  return FILEIRA_NORMAL;
}
static int editorial(TipoFileira t) {
  return t == FILEIRA_DESTAQUE || t == FILEIRA_COLECAO || t == FILEIRA_SERVICO
      || t == FILEIRA_SOCIAL;
}


static Foco foco;
static HomeItem itemFoco;      // preenchido durante o desenho, lido pela transicao
static int  temItemFoco = 0;
static float animFoco[MAX_FIL][MAX_CARDS];
static float scrollX[MAX_FIL];
static float scrollY = 0.0f;
// Velocidades das molas de 2a ordem do deslize. Ficam ao lado da posicao
// porque anim_mola2() precisa das duas. Ver anim.h.
static float velX[MAX_FIL];
// Fileiras que o limite cortou na ultima montagem — colecoes incluidas. Ver o
// comentario no corte, em remontar().
static int   cortadasPeloLimite;
static float velY = 0.0f;
static int sair = 0, pedidoAbrir = 0, pedidoMenu = 0;
// VOLTAR NA HOME PEDE CONFIRMACAO. Ver home_evento; o aviso e desenhado em
// home_desenhar enquanto a janela esta aberta.
#define HOME_SAIR_MS 3000
static Uint32 sairPerguntadoEm;
static Uint32 okDesde = 0;
static int okPressionando = 0;
static int okLongDisparado = 0;
static int okConsumirSoltura = 0;
static float okHold = 0.0f;

// --- hero-carrossel ---
static int heroAtual = 0, heroAnterior = 0;
// Candidato a heroi e desde quando ele e o candidato. Ver NV_HERO_REPOUSO_MS.
static int    heroPendente = 0;
static Uint32 heroPendenteEm = 0;
// ARTE JA ESCOLHIDA MAS AINDA NAO NO AR.
//
// O repouso de NV_HERO_REPOUSO_MS decide QUANDO trocar; isto decide SE ja da
// para trocar. Antes, no instante do repouso a arte velha comecava a apagar e a
// nova so aparecia quando a textura ficasse pronta — no meio ficava VAZIO, e
// andando depressa pela fileira o dono via o fundo piscar entre uma arte e
// outra. Agora a velha fica NO LUGAR ate a nova estar decodificada; so entao a
// troca comeca. Andar rapido deixa de mexer no fundo.
static int    heroDesejado = -1;
// Quando heroDesejado foi anunciado. E o relogio do teto de NV_HERO_ESPERA_MS.
static Uint32 heroDesejadoEm = 0;
// Telemetria da espera do heroi — ver o bloco que a imprime, no desenho.
static int heroEstourou, heroEsperaN, heroEsperaEstouros;
static unsigned heroEsperaSoma, heroEsperaPior;
// O item cujo heroi estourou o prazo e ainda nao teve a arte decodificada, e o
// instante em que ele passou a ser desejado. -1 = ninguem atrasado.
static int      heroTardeItem = -1;
static unsigned heroTardeEm;

// --- EXPANSAO DO CARTAZ FOCADO EM REPOUSO ------------------------------------
//
// `focusedPosterBackdropExpandEnabled` e `...DelaySeconds` ja existiam em
// art/ajustes.txt e em ajustes.c, mas NADA no desenho os lia — o ajuste estava
// na tela de Ajustes sem efeito nenhum. E o comportamento que o dono chama de
// "o card crescer quando ta parado": o foco pousa num cartaz, e depois de
// alguns segundos ele se abre na arte DEITADA, empurrando os vizinhos.
//
// Guardado por fileira/coluna e nao so o alvo, porque mover o foco tem de
// FECHAR o que estava aberto no mesmo quadro em que abre o relogio do novo.
static int    expFileira = -1, expColuna = -1;
static Uint32 expDesde = 0;
static float  expAbre = 0.0f;   // 0 fechado, 1 aberto

// MEDIDO na TCL (1920x1080, com.nuvio.tv), capturando 0,8 s e 7,8 s apos a
// tecla: o card focado vai de 214x320 para 565x320. A ALTURA NAO MUDA e a
// borda ESQUERDA fica parada em x=102 — ele cresce so para a direita e empurra
// os vizinhos. 565/320 = 1,77, ou seja 16:9 na mesma altura.
#define NV_EXP_ASPECTO (16.0f / 9.0f)

// Fileira que pode expandir: so a de cartaz EM PE. A de continuar assistindo e
// a de posteres deitados ja mostram a arte larga — nao ha para o que abrir.
static int podeExpandir(int r) {
  if (r < 0 || r >= nFileiras) return 0;
  if (fileiras[r].tipo != FILEIRA_NORMAL) return 0;
  return !ajustes_posteres_deitados();
}
static Uint32 heroTrocaEm = 0;
// APAGAR -> VAZIO -> CORTE SECO. Ver a medida em NV_HERO_FADE_MS.
// `heroSai`   alfa da arte que esta SAINDO: 1 no instante da troca, 0 no fim.
// `heroEntra` 0 enquanto a arte nova esta escondida; vira 1 de uma vez, no
//             quadro em que a textura fica pronta E o esvanecimento acabou.
static float heroSai   = 0.0f;
static float heroEntra = 1.0f;

static void carregaDir(const char *dir, char destino[][512], int *n, const char *sub) {
  char caminho[512];
  if (sub) snprintf(caminho, sizeof caminho, "%s/%s", dir, sub);
  else snprintf(caminho, sizeof caminho, "%s", dir);
  DIR *d = opendir(caminho);
  if (!d) return;
  struct dirent *e;
  while ((e = readdir(d)) && *n < MAX_ARTE) {
    if (!strstr(e->d_name, ".jpg") && !strstr(e->d_name, ".png")) continue;
    snprintf(destino[*n], 512, "%s/%s", caminho, e->d_name);
    (*n)++;
  }
  closedir(d);
}

// A forma do card sai de DUAS preferencias, e nao do tipo da fileira:
//
//   `modernLandscapePostersEnabled` troca o poster 2:3 (212x322) pelo card
//   deitado 16:9 (318x182.9). MEDIDO no app web com a preferencia ligada.
//
//   `continueWatchingCardStyle` decide a fileira de "Continuar assistindo":
//   "card" e "largo" desenham deitado, "poster" usa o mesmo 2:3 das outras.
static float larguraDe(TipoFileira t) {
  switch (t) {
    case FILEIRA_CONTINUE: return NV_DESTAQUE_W;
    case FILEIRA_DESTAQUE: return 568.0f;
    case FILEIRA_COLECAO: return 480.0f;
    case FILEIRA_SERVICO: return 360.0f;
    case FILEIRA_SOCIAL: return 540.0f;
    case FILEIRA_TOP10: return 212.0f;
    case FILEIRA_RETORNO: return 680.0f;
    case FILEIRA_CATALOGOS: return 360.0f;
    default:               return ajustes_posteres_deitados() ? NV_CARD_LAND_W
                                                              : NV_CARD_W;
  }
}
// Quantos titulos o hero percorre. Vem do catalogo quando existe.
static int nAcervoHero(void) { int n = cat_n(); if (n) return n; return nBd ? nBd : 1; }

// Arte de um titulo nunca pode ser preenchida por uma posição equivalente de
// outro vetor. O catalogo chega em lotes, e a ordem dos backdrops do pacote não
// tem relação estável com a ordem dos itens da rede. Retornar somente arte que
// pertence ao próprio item deixa o estado sem arte explícito, em vez de trocar
// identidade silenciosamente.
static const char *arteDoItem(const CatItem *item, int *ehPoster) {
  if (ehPoster) *ehPoster = 0;
  if (!item) return NULL;
  if (item->backdrop[0]) return item->backdrop;
  if (item->poster[0]) {
    if (ehPoster) *ehPoster = 1;
    return item->poster;
  }
  return NULL;
}

// Um poster é uma boa reserva editorial, mas não deve ser cover-stretched num
// hero 16:9, pois isso corta justamente o rosto e o título. Ele fica contido no
// lado direito, com a mesma vinheta do hero, e o restante da composição segue
// disponível para a cópia do título.
static int desenhaArteHero(GfxRect r, GfxModo modo, const CatItem *item,
                           const char *path, float alpha) {
  int ehPoster = 0;
  const char *arte = item ? arteDoItem(item, &ehPoster) : path;
  GLuint tex;
  if (!arte || !arte[0]) return 0;
  tex = tex_obter_hero(arte);
  if (!tex) return 0;
  gfx_tex_aspect_atual = tex_aspecto(arte);
  if (!ehPoster) {
    gfx_rect(r, tex, modo, 0, 0, 0, 0, 0, 0, 0, alpha);
  } else {
    float ap = gfx_tex_aspect_atual > 0.05f ? gfx_tex_aspect_atual : (2.0f / 3.0f);
    float h = r.h, w = h * ap, limite = r.w * 0.42f;
    if (w > limite) { w = limite; h = w / ap; }
    GfxRect poster = { r.x + r.w - w, r.y + (r.h - h) * 0.5f, w, h };
    gfx_rect(poster, tex, GFX_HERO, 0, 0, 0, 0, 0, 0, 0, alpha);
  }
  gfx_tex_aspect_atual = 0.0f;
  return 1;
}

// `esperando` separa DUAS COISAS QUE NAO SAO A MESMA, e a diferenca nasceu com
// o teto de NV_HERO_ESPERA_MS: passado o prazo o heroi troca sem a arte, e a
// arte esta A CAMINHO — dizer "Arte indisponível" ali seria trocar uma mentira
// (a arte do titulo anterior) por outra (a arte nao existe). Sem o teto so
// havia o caso de arte que de fato nao existe, e por isso a frase era uma so.
static void desenhaPlaceholderHero(GfxRect r, const CatItem *item, float alpha,
                                   int esperando) {
  GfxRect bloco = { r.x + r.w * 0.58f, r.y + 32.0f,
                    r.w * 0.34f, r.h - 64.0f };
  gfx_cor(bloco, 0.035f, 0.075f, 0.082f, 0.098f, alpha * 0.92f);
  { TxtLinha t = txt_linha(TXT_HERO_META,
                            esperando ? "Carregando arte…" : "Arte indisponível",
                            185, 191, 204, 255);
    txt_desenhar_alpha(t, bloco.x + 28.0f,
                       bloco.y + bloco.h * 0.5f - t.h * 0.5f,
                       alpha); }
  if (item && item->titulo[0]) {
    TxtLinha t = txt_linha_corta(TXT_CAPTION, item->titulo,
                                 211, 216, 226, 255, bloco.w - 56.0f);
    txt_desenhar_alpha(t, bloco.x + 28.0f,
                       bloco.y + bloco.h * 0.5f + 20.0f, alpha * 0.76f);
  }
}

static void desenhaArteAusente(GfxRect r, float raio, const CatItem *item,
                               float alpha) {
  gfx_cor(r, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G,
          NV_COR_ESQUELETO_B, alpha);
  TxtLinha estado = txt_linha_corta(TXT_CAPTION, "Arte indisponível",
                                    184, 188, 198, 255, r.w - 32.0f);
  float centro = r.y + r.h * 0.5f;
  txt_desenhar_alpha(estado, r.x + (r.w - estado.w) * 0.5f,
                     centro - estado.h * 0.5f - (item && item->titulo[0] ? 8.0f : 0.0f),
                     alpha * 0.9f);
  if (item && item->titulo[0]) {
    TxtLinha nome = txt_linha_corta(TXT_MINI, item->titulo,
                                    160, 165, 178, 255, r.w - 32.0f);
    txt_desenhar_alpha(nome, r.x + (r.w - nome.w) * 0.5f,
                       centro + 12.0f, alpha * 0.78f);
  }
}

// Escolha de formato para cards. O helper arteDoItem acima informa se precisou
// usar poster como fallback; aqui a ordem visual do card continua explicita.
static const char *arte_por_formato(const CatItem *item, int deitado) {
  if (!item) return NULL;
  if (deitado) return item->backdrop[0] ? item->backdrop
                                      : (item->poster[0] ? item->poster : NULL);
  return item->poster[0] ? item->poster
                         : (item->backdrop[0] ? item->backdrop : NULL);
}

// `cat_item()` faz wrap para telas que percorrem listas circulares. A home nao
// pode usar esse contrato para resolver arte: indice stale vira ausencia, nunca
// outro titulo.
static const CatItem *cat_item_exato(int i) {
  int n = cat_n();
  if (i < 0 || i >= n) return NULL;
  return cat_item(i);
}

// A pasta art/ e acervo de reserva apenas no modo sem catalogo. Quando a rede
// publicou itens, nenhum arquivo generico pode ocupar o lugar de outro titulo.
static const char *arte_por_identidade(int indice, int deitado) {
  const CatItem *item = cat_item_exato(indice);
  const char *arte = arte_por_formato(item, deitado);
  if (arte) return arte;
  if (cat_n() == 0 && indice >= 0) {
    if (!deitado && indice < nPst) return pst[indice];
    if (indice < nBd) return bd[indice];
  }
  return NULL;
}

static int foco_pode_pressao_longa(void) {
  if (foco.fileira < 0 || foco.fileira >= nFileiras) return 0;
  const Fileira *s = &fileiras[foco.fileira];
  if (s->tipo == FILEIRA_CATALOGOS || s->tipo == FILEIRA_SOCIAL ||
      s->tipo == FILEIRA_TOP10) return 0;
  if (s->verTudo && foco.coluna == s->n) return 0;
  return foco.coluna >= 0 && foco.coluna < s->n;
}


static float alturaDe(TipoFileira t) {
  switch (t) {
    case FILEIRA_CONTINUE: return NV_DESTAQUE_H;
    case FILEIRA_DESTAQUE: return 320.0f;
    case FILEIRA_COLECAO: return 270.0f;
    case FILEIRA_SERVICO: return 203.0f;
    case FILEIRA_SOCIAL: return 240.0f;
    case FILEIRA_RETORNO: return 178.0f;
    case FILEIRA_TOP10: return 320.0f;
    case FILEIRA_CATALOGOS: return 203.0f;
    default:               return ajustes_posteres_deitados() ? NV_CARD_LAND_H
                                                              : NV_CARD_H;
  }
}
static int temRotulo(TipoFileira t) {
  // O rotulo abaixo do poster so existe no poster EM PE. No card deitado o web
  // poe a legenda DENTRO da moldura (.home-poster-landscape-copy) e esconde o
  // bloco de fora (.home-poster-card.is-landscape .home-poster-copy{display:none}).
  return t == FILEIRA_NORMAL && ajustes_rotulos_poster()
      && !ajustes_posteres_deitados();
}
// O gap do tvOS e fixo em 40px e ja foi dimensionado para caber o crescimento
// do foco: um card de 410 crescendo 9% invade 18px de cada lado. O card
// destaque e maior que qualquer coisa que a Apple usa, entao para ele o gap
// segue proporcional — senao a invasao (31px) come quase todo o respiro.
static float gapDe(TipoFileira t) {
  (void)t;
  return NV_CARD_GAP;
}
// Passo vertical entre fileiras. `.home-modern-landscape-posters` aperta o
// `--home-row-gap` de 32 para 24 (components.css:6473) — a fileira deitada e
// mais baixa e o respiro do poster em pe sobraria nela.
static float fileiraGap(void) {
  return ajustes_posteres_deitados() ? NV_FILEIRA_GAP_LAND : NV_FILEIRA_GAP;
}
// Raio do card, em fracao do menor lado (o SDF do shader e normalizado). Este e
// o UNICO numero do card moderno que sai mesmo de `posterCardCornerRadiusDp`:
// 12dp x 2 = 24px, conferido no app rodando. A largura NAO sai de la (ver a
// nota em ajustes.h).
static float raioDe(float w, float h) {
  float menor = w < h ? w : h;
  if (menor <= 0.0f) return NV_RAIO_CARD;
  return ajustes_raio_poster_px() / menor;
}

// --- Profundidade dos cartoes (`cardDepth*`) ---------------------------------
// O web faz isto com dois pseudo-elementos sobre a arte: um brilho na borda de
// CIMA com opacidade `--card-depth-edge` e uma faixa clara e discreta —
// `--card-depth-sheen` — atravessando a parte alta do cartao. `--card-depth-
// coverage` engorda a banda da borda: `12 + round(18 * coverage)` px
// (layoutPreferences.js:181). Sao os mesmos tres numeros da tela de Ajustes.
static void desenhaProfundidade(GfxRect card, float raio, int ligadaAqui) {
  if (!ajustes_profundidade() || !ligadaAqui) return;
  float borda = ajustes_profundidade_borda();
  float brilho = ajustes_profundidade_brilho();
  float cobertura = ajustes_profundidade_cobertura();
  if (borda > 0.001f) {
    float h = 12.0f + 18.0f * cobertura;
    GfxRect faixa = { card.x, card.y, card.w, h };
    // Raio proporcional: a faixa e muito mais baixa que o card, entao repetir a
    // fracao do card arredondaria demais e a borda descolaria do canto.
    gfx_cor(faixa, raio * (card.h / (h > 0.0f ? h : 1.0f)) * 0.5f,
            1.0f, 1.0f, 1.0f, borda * 0.55f);
  }
  if (brilho > 0.001f) {
    GfxRect refl = { card.x, card.y + card.h * 0.06f, card.w, card.h * 0.28f };
    gfx_cor(refl, raio, 1.0f, 1.0f, 1.0f, brilho * 0.18f);
  }
}
// ZERO. MEDIDO no app web (sessao logada, perfil do dono): o card em foco tem
// `transform: none`, `scale: none` e o mesmo getBoundingClientRect do card ao
// lado — 212x322 nos dois, mesma linha, mesmo topo. A escala de 9% e o
// levantamento de 8px vinham das tabelas de Top Shelf do tvOS, e nao desta
// interface. Eram eles que faziam o card focado subir 22px e encostar no titulo
// da fileira, que fica 15px acima dos cards (titulo 518..549, cards em 564).
//
// O foco no web se marca por um ANEL de 2px `#f5f5f5` desenhado por dentro e
// por fora da arte (box-shadow inset + outset), com o card mantendo a caixa.
static float escalaDe(TipoFileira t) {
  (void)t;
  return 0.0f;
}
// --- MEDIDA POR FILEIRA, e nao por tipo -------------------------------------
//
// O tipo continua dando a FORMA (proporcao e medida base, tudo medido no app
// web); a fileira acrescenta o fator de tamanho que a pessoa escolheu em
// Ajustes -> Fileiras da Home. Sao duas coisas: mudar a forma troca 2:3 por
// 16:9, mudar o tamanho mantem a proporcao.
//
// O gap NAO entra na escala de proposito. Ele foi dimensionado para caber o
// crescimento do foco (a nota em gapDe), e um respiro que encolhe junto com o
// card faz dois cards grandes se encostarem exatamente quando um deles cresce.
static float escalaFil(int r) {
  float e = (r >= 0 && r < MAX_FIL) ? fileiras[r].escala : 0.0f;
  return e > 0.05f ? e : 1.0f;
}
static float larguraFil(int r) {
  // 680 e a largura da PILHA do Top 10, que nao sai de larguraDe: ela e uma
  // fileira de um card so, com o ranking dentro. Estava repetida nos dois
  // lugares que mediam a fileira e agora esta num.
  float w = (r >= 0 && r < MAX_FIL && fileiras[r].stackN)
          ? 680.0f : larguraDe(fileiras[r].tipo);
  return w * escalaFil(r);
}
static float alturaFil(int r)      { return alturaDe(fileiras[r].tipo) * escalaFil(r); }
// Altura TOTAL que a fileira ocupa: a arte mais o bloco de rotulo, quando ele
// existe. Sem somar o rotulo aqui, a fileira seguinte sobe por cima do texto —
// foi o mesmo defeito que o titulo de fileira ja tinha tido sobre os cards.
static float alturaTotalFil(int r) {
  return alturaFil(r) + (temRotulo(fileiras[r].tipo) ? NV_POSTER_COPY_H : 0.0f);
}
static float passoFil(int r)       { return larguraFil(r) + gapDe(fileiras[r].tipo); }

// FilTipo (escolha em Ajustes) -> TipoFileira (forma que o desenho conhece).
// A traducao vive aqui porque este e o unico arquivo que sabe o que cada forma
// mede; fileiras.h nao pode incluir home.h sem fechar um ciclo de headers.
static TipoFileira tipoDaEscolha(int t) {
  switch (t) {
    case FIL_TIPO_CARTAZ:   return FILEIRA_NORMAL;
    case FIL_TIPO_DESTAQUE: return FILEIRA_DESTAQUE;
    case FIL_TIPO_COLECAO:  return FILEIRA_COLECAO;
    case FIL_TIPO_SERVICO:  return FILEIRA_SERVICO;
    default:                return FILEIRA_TOP10;
  }
}

// --- ONDE A PESSOA ESTAVA NA HOME -------------------------------------------
//
// Dois defeitos com a MESMA causa, e por isso um codigo so.
//
// 1. FECHAR O APP PERDIA TUDO. O catalogo ja tinha cache em disco, entao a home
//    reabria com as mesmas fileiras — e o foco no primeiro card da primeira.
//    Quem estava na oitava fileira, na coluna 11, refazia o caminho inteiro com
//    o D-pad a cada abertura. Nada de posicao era gravado: uma varredura por
//    persistencia de foco ou rolagem em src/ nao achava nada.
//
// 2. `colunaLembrada[]` MORRIA A CADA REPUBLICACAO. focus_iniciar faz
//    `memset(f, 0, sizeof *f)`, e a remontagem so devolvia `fileira` e
//    `coluna`. Depois de qualquer republicacao — catalogo da rede chegando
//    (sao ~16 nos primeiros segundos), mudanca em Ajustes, fim de reproducao —
//    descer e subir de fileira caia na coluna 0 em vez da coluna lembrada. E
//    exatamente o defeito que o comentario no topo de focus.h diz que a
//    estrutura existe para evitar.
//
// A CHAVE, NUNCA O INDICE. `colunaLembrada[]` e `scrollX[]` sao indexados por
// INDICE de fileira, e o indice nao sobrevive a nada: a ordem sai de
// art/fileiras.txt e de Ajustes, uma fileira nova entra no meio, o limite corta
// o fim. Guardar por indice devolveria a coluna 11 de "Continuar assistindo"
// para dentro de "Oscars 2026". Toda linha aqui e casada por chave.
#define HOME_POS_ARQ    "home-pos.txt"
#define HOME_POS_VERSAO "v1"

// REPOUSO ANTES DE GRAVAR, e nao gravacao por quadro.
//
// Andando pela fileira com o D-pad segurado a tecla repete a cada ~150 ms;
// gravar a cada movimento seria uma escrita de arquivo por card atravessado, e
// no alvo Tizen cada escrita ainda paga a trava do sistema de arquivos (ver a
// nota longa em dados.c) no fio do desenho. Com 1,2 s de repouso, atravessar
// uma fileira inteira grava UMA vez, no fim.
//
// Por que 1,2 s e nao menos: dados.c ja agrupa as escritas do usuario numa
// descarga a cada 700 ms (NV_DESC_MIN_MS). Um repouso mais curto que isso so
// enfileiraria escritas que a camada de baixo ia juntar de qualquer jeito.
// Acima de 700 ms cada gravacao nossa tende a virar uma descarga propria — e
// esperar mais um pouco e de graca, porque a mola horizontal de scrollX ja
// assentou e o que vai para o disco e o valor final, nao um quadro do meio.
#define NV_POS_REPOUSO_MS 1200

// Uma fileira, do jeito que a posicao a conhece. `scrollX` em INTEIRO de
// proposito: sub-pixel de rolagem nao e informacao, e o arquivo e texto.
typedef struct { char chave[192]; int coluna; int scrollX; } HomePos;

// INSTANTANEO VIVO: tirado no comeco de sincronizarFileiras, devolvido no fim.
// E o que conserta o defeito 2. static e nao pilha pelo mesmo motivo do
// `arranjo` la embaixo: sao ~6,5 KB e a funcao ja carrega dois vetores desse
// porte. So o fio do desenho toca nisto.
static HomePos posViva[MAX_FIL];
static int     nPosViva;
static char    posVivaFoco[192];
static int     posVivaCol;

// INSTANTANEO DO DISCO: lido uma vez em home_iniciar e consumido UMA vez, na
// primeira remontagem cujo conjunto de fileiras bater com o gravado. Enquanto
// pendente ele nao pode ser sobrescrito — no arranque a descoberta publica
// fileira a fileira, e uma home de duas fileiras gravada por cima apagaria a
// posicao de ontem antes de ela ter chance de ser aplicada.
static HomePos posDisco[MAX_FIL];
static int     nPosDisco;
static char    posDiscoFoco[192];
static int     posDiscoCol;
static int     posDiscoPendente;

// Detector de repouso. Guardado em INDICE porque e so para saber se algo
// mexeu desde o quadro anterior; o que vai para o disco e sempre por chave.
static int    posUltFil = 0, posUltCol = 0;
static Uint32 posMexeuEm;
static int    posSujo;

static const HomePos *posAchar(const HomePos *t, int n, const char *chave) {
  int i;
  if (!chave || !chave[0]) return NULL;
  for (i = 0; i < n; i++) if (!strcmp(t[i].chave, chave)) return &t[i];
  return NULL;
}

// "Retomar agora" FICA DE FORA de tudo isto. Ela nao vem do catalogo: nasce de
// uma sessao interrompida nesta execucao (retomarIndice e static, nao e
// gravado) e some sozinha. Inclui-la no conjunto gravado faria o casamento
// falhar sempre que a pessoa fechasse o app logo depois de assistir a algo —
// justo a vez em que reabrir no mesmo lugar mais importa.
static int posIgnora(const char *chave) {
  return !chave[0] || !strcmp(chave, "last_session");
}

static void posCapturar(void) {
  int r;
  nPosViva = 0;
  posVivaFoco[0] = 0;
  posVivaCol = 0;
  for (r = 0; r < nFileiras && nPosViva < MAX_FIL; r++) {
    HomePos *p;
    if (posIgnora(fileiras[r].chave)) continue;
    p = &posViva[nPosViva++];
    snprintf(p->chave, sizeof p->chave, "%s", fileiras[r].chave);
    // A coluna do foco e a MAIS NOVA das duas: colunaLembrada[r] so e escrita
    // quando o foco SAI da fileira r, entao para a fileira em foco ela esta
    // atrasada de uma travessia inteira.
    p->coluna  = (r == foco.fileira) ? foco.coluna : foco.colunaLembrada[r];
    p->scrollX = (int)(scrollX[r] + 0.5f);
  }
  if (foco.fileira >= 0 && foco.fileira < nFileiras
      && !posIgnora(fileiras[foco.fileira].chave)) {
    snprintf(posVivaFoco, sizeof posVivaFoco, "%s", fileiras[foco.fileira].chave);
    posVivaCol = foco.coluna;
  }
}

// Devolve o indice que ficou com o foco, ou -1 se a chave gravada nao esta na
// tela — e nesse caso NADA e movido: o foco fica onde focus_iniciar o deixou,
// em (0,0). Posicionar num vizinho por aproximacao seria pior que nao
// restaurar: a pessoa acharia que voltou ao lugar certo.
static int posAplicarTabela(const HomePos *t, int n,
                            const char *chFoco, int colFoco) {
  int r, achou = -1;
  for (r = 0; r < nFileiras; r++) {
    const HomePos *p = posAchar(t, n, fileiras[r].chave);
    int c;
    if (!p) continue;
    c = p->coluna;
    // A fileira pode ter encolhido entre uma publicacao e outra, ou entre
    // ontem e hoje.
    if (c >= foco.nColunas[r]) c = foco.nColunas[r] - 1;
    if (c < 0) c = 0;
    foco.colunaLembrada[r] = c;
    scrollX[r] = (float)p->scrollX;
  }
  if (chFoco && chFoco[0])
    for (r = 0; r < nFileiras; r++)
      if (!strcmp(fileiras[r].chave, chFoco)) { achou = r; break; }
  if (achou >= 0) {
    int c = colFoco;
    if (c >= foco.nColunas[achou]) c = foco.nColunas[achou] - 1;
    if (c < 0) c = 0;
    foco.fileira = achou;
    foco.coluna  = c;
    foco.colunaLembrada[achou] = c;
  }
  return achou;
}

// O CONJUNTO TEM DE BATER, e nao so a chave do foco.
//
// Casar so a chave do foco bastaria para nunca pousar na fileira errada, mas
// nao para a restauracao fazer sentido: no arranque a home passa por ~16
// estados intermediarios, e o primeiro deles em que a chave gravada aparece
// pode ter duas fileiras. Restaurar ali coloca o foco na fileira certa de uma
// home que ainda vai mudar de forma embaixo dele. Exigir o conjunto inteiro faz
// a restauracao acontecer uma vez so, na home montada — e, quando as fileiras
// de hoje nao sao as de ontem, nao acontecer, em silencio, ficando em (0,0).
static int posConjuntoBate(const HomePos *t, int n) {
  int r, vivas = 0;
  if (n < 1) return 0;
  for (r = 0; r < nFileiras; r++) {
    // Fileira sem chave e a tabela de RESERVA, de antes de o catalogo chegar:
    // ali nao ha home nenhuma com que casar.
    if (!fileiras[r].chave[0]) return 0;
    if (posIgnora(fileiras[r].chave)) continue;
    if (!posAchar(t, n, fileiras[r].chave)) return 0;
    vivas++;
  }
  return vivas == n;
}

static void posGravar(void) {
  // static: ~7,5 KB, e esta funcao roda no fio do desenho.
  static char buf[MAX_FIL * 232 + 64];
  int k, r;
  // Enquanto a leitura do disco nao foi consumida nem descartada, quem manda e
  // o disco. Sem esta guarda a home pela metade do arranque grava por cima da
  // posicao de ontem antes de ela chegar a ser aplicada.
  if (posDiscoPendente) return;
  posCapturar();
  // HOME SEM FILEIRA NENHUMA NAO APAGA A DE ONTEM. Acontece de verdade: se
  // home_iniciar desiste (nenhum backdrop) o app ainda passa por home_encerrar,
  // e sem esta linha o arquivo sairia daqui vazio, trocando a posicao guardada
  // por nada. Nao gravar deixa a de ontem valendo, que e sempre o melhor
  // desfecho quando nao ha o que dizer.
  if (nPosViva < 1) return;
  k = snprintf(buf, sizeof buf, "%s\n", HOME_POS_VERSAO);
  // A CHAVE VAI POR ULTIMO na linha, e o resto da linha E a chave. Chave de
  // catalogo de addon e texto de terceiro e pode ter espaco; com a chave no
  // meio, um espaco quebraria a leitura em silencio.
  if (posVivaFoco[0] && k < (int)sizeof buf - 232)
    k += snprintf(buf + k, sizeof buf - (unsigned)k, "f %d %s\n",
                  posVivaCol, posVivaFoco);
  for (r = 0; r < nPosViva && k < (int)sizeof buf - 232; r++) {
    // Uma quebra de linha na chave viraria duas linhas no arquivo. Nao ha como
    // isso acontecer hoje, e e barato garantir que continue assim.
    if (strchr(posViva[r].chave, '\n')) continue;
    k += snprintf(buf + k, sizeof buf - (unsigned)k, "r %d %d %s\n",
                  posViva[r].coluna, posViva[r].scrollX, posViva[r].chave);
  }
  // NADA DE CREDENCIAL AQUI: so chave de fileira, coluna e rolagem. A `base` da
  // fileira e uma URL de addon com JWT no caminho (ver catalogo.h) e nunca
  // entra neste arquivo.
  // LEVE de proposito: ver dados_gravar_leve. Esta funcao roda toda vez que o
  // foco descansa, e no Tizen cada descarga custa 30-52 ms sincronos — andar
  // pela home virava um quadro travado por movimento (#21).
  dados_gravar_leve(HOME_POS_ARQ, buf);
}

static void posLer(void) {
  char *txt, *l;
  nPosDisco = 0;
  posDiscoFoco[0] = 0;
  posDiscoCol = 0;
  posDiscoPendente = 0;
  txt = dados_ler(HOME_POS_ARQ);
  if (!txt) return;
  // Versao na primeira linha: um arquivo de outro formato e simplesmente
  // ignorado, e a home abre em (0,0) como antes de tudo isto existir.
  if (strncmp(txt, HOME_POS_VERSAO "\n", sizeof HOME_POS_VERSAO)) {
    free(txt);
    return;
  }
  for (l = strchr(txt, '\n'); l; ) {
    char *fim;
    int c, sx, off;
    l++;
    fim = strchr(l, '\n');
    if (fim) *fim = 0;
    off = 0;
    if (l[0] == 'f' && l[1] == ' '
        && sscanf(l + 2, "%d %n", &c, &off) == 1 && off > 0 && l[2 + off]) {
      posDiscoCol = c < 0 ? 0 : c;
      snprintf(posDiscoFoco, sizeof posDiscoFoco, "%s", l + 2 + off);
    } else if (l[0] == 'r' && l[1] == ' ' && nPosDisco < MAX_FIL
        && sscanf(l + 2, "%d %d %n", &c, &sx, &off) == 2 && off > 0 && l[2 + off]) {
      HomePos *p = &posDisco[nPosDisco];
      p->coluna  = c  < 0 ? 0 : c;
      p->scrollX = sx < 0 ? 0 : sx;
      snprintf(p->chave, sizeof p->chave, "%s", l + 2 + off);
      if (!posIgnora(p->chave)) nPosDisco++;
    }
    if (!fim) break;
    l = fim;
  }
  free(txt);
  posDiscoPendente = nPosDisco > 0;
}

int home_iniciar(const char *dirArte) {
  extras_carregar(dirArte);
  col_carregar(dirArte);
  badges_carregar(dirArte);
  cat_carregar(dirArte);
  // O cache da ULTIMA sessao entra por cima do catalogo do pacote, antes de
  // qualquer rede. Se nao existir (primeira execucao) ou for de outra build,
  // segue-se com o do pacote, como sempre foi.
  if (cat_ler_cache(dirArte)) marco("catalogo do cache na tela");
  carregaDir(dirArte, bd, &nBd, NULL);
  carregaDir(dirArte, pst, &nPst, "poster");
  if (!nBd) { printf("home: nenhum backdrop em %s\n", dirArte); return 0; }
  if (!nPst) { printf("home: sem posters retrato, Top 10 usara backdrop\n"); }

  // No layout moderno legacy o hero é informativo; a navegação começa na
  // primeira fileira de conteúdo (como buildModernNavigationRows()).
  int cols[MAX_FIL];
  for (int i = 0; i < nFileiras; i++)
    cols[i] = fileiras[i].n + (fileiras[i].verTudo ? 1 : 0);
  focus_iniciar(&foco, nFileiras, cols);
  // A posicao de ontem so pode ser APLICADA quando as fileiras de verdade
  // existirem. Aqui `fileiras[]` ainda e a tabela de reserva, que nem chave
  // tem; a leitura fica pendente e sincronizarFileiras a consome na primeira
  // montagem cujo conjunto bata com o gravado.
  posLer();
  posUltFil = foco.fileira;
  posUltCol = foco.coluna;
  posSujo = 0;
  heroTrocaEm = SDL_GetTicks() + NV_HERO_INTERVALO_MS;
  printf("home: %d backdrops, %d posters, %d fileiras\n", nBd, nPst, nFileiras);
  return 1;
}

void home_evento(const SDL_Event *e) {
  if (e->type == SDL_QUIT) { sair = 1; return; }

  // SEGURAR O OK ABRE O MENU DO CARTAZ.
  //
  // O tempo so e conhecido quando a tecla SOBE, entao o KEYUP tem de ser visto
  // — e ele era descartado logo abaixo, junto com todo evento que nao fosse
  // KEYDOWN. A tela de titulo ja usa esta mesma medida (NV_HOLD_MS) para
  // separar "Reproduzir" de "escolher fonte".
  { SDL_Keycode kk = e->key.keysym.sym;
    int ehOk = (kk == SDLK_RETURN || kk == SDLK_KP_ENTER || kk == SDLK_SPACE);
    if (e->type == SDL_KEYDOWN && ehOk) {
      if (!okPressionando) {
        okPressionando = 1;
        okLongDisparado = 0;
        okConsumirSoltura = 0;
        okDesde = SDL_GetTicks();
      }
      return;
    } else if (e->type == SDL_KEYUP && ehOk) {
      // SOLTAR SEM TER PRESSIONADO AQUI NAO E CLIQUE — a mesma guarda que
      // detail.c ja tem, e que faltava nesta tela.
      //
      // A barra lateral decide no KEYDOWN (menu.c: escolher()) e se fecha ali
      // mesmo. O KEYUP do MESMO toque chega quando menu_aberto() ja e 0, e o
      // roteador de app.c entao o entrega a home — que abria o card em foco.
      // Escolhendo "Trocar de usuário" no rodape o efeito era o do issue #8: o
      // app ia para a tela de perfis e, ao voltar para a home, o pedido de
      // abrir que ficou pendente disparava e "abria um filme aleatorio de
      // Continuar assistindo". Vale para todo item da barra, e o mesmo para o
      // OK que fecha qualquer folha desenhada acima da home.
      if (!okPressionando) { okDesde = 0; okHold = 0.0f; return; }
      if (okConsumirSoltura) {
        okConsumirSoltura = 0;
        okDesde = 0;
        okPressionando = 0;
        okHold = 0.0f;
        return;
      }
      Uint32 dur = okDesde ? SDL_GetTicks() - okDesde : 0;
      int noVerTudo = (foco.fileira >= 0 && foco.fileira < nFileiras &&
                       fileiras[foco.fileira].verTudo &&
                       foco.coluna == fileiras[foco.fileira].n);
      okDesde = 0;
      okPressionando = 0;
      okLongDisparado = 0;
      okHold = 0.0f;
      if (foco.fileira < 0 || foco.fileira >= nFileiras) return;
      if(fileiras[foco.fileira].tipo==FILEIRA_TOP10 && fileiras[foco.fileira].stackN) {
        Fileira *s=&fileiras[foco.fileira];
        s->n=s->stackN<10?s->stackN:10;
        s->stackN=0;s->verTudo=1;
        foco.coluna=0;foco.colunaLembrada[foco.fileira]=0;
        foco.nColunas[foco.fileira]=s->n+1;
        return;
      }
      if(fileiras[foco.fileira].tipo==FILEIRA_SOCIAL && fileiras[foco.fileira].ini<0) {
        pedidoSocial=1;return;
      }
      if(fileiras[foco.fileira].tipo==FILEIRA_SOCIAL) {
        const CatItem *ci=cat_item_exato(fileiras[foco.fileira].ini+foco.coluna);
        if(ci){pessoaSocial=*ci;pedidoPessoaSocial=1;}return;
      }
      if (fileiras[foco.fileira].tipo == FILEIRA_CATALOGOS) {
        if (foco.coluna >= 0 && foco.coluna < fileiras[foco.fileira].n) {
          vertudo_colecao(col_folder(fileiras[foco.fileira].folders[foco.coluna]));
        }
      } else if (noVerTudo) {
        vertudo_abrir(fileiras[foco.fileira].base, fileiras[foco.fileira].catTipo,
                      fileiras[foco.fileira].catId, fileiras[foco.fileira].titulo);
      } else if (dur >= NV_HOLD_MS) {
        ctx_abrir(fileiras[foco.fileira].ini + foco.coluna);
      } else {
        pedidoAbrir = 1;
      }
      return;
    } }

  if (e->type != SDL_KEYDOWN) return;
  SDL_Keycode k = e->key.keysym.sym;
#ifdef __APPLE__
  // Rodando no Mac, o Back na home NAO fecha: fechar a janela no meio de um
  // teste custa recompilar e reabrir. No aparelho ele sai do app, como deve.
  if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE) return;
  if (k == SDLK_q) { sair = 1; return; }
#else
  if (k == SDLK_ESCAPE || k == SDLK_AC_BACK) {
    // UM TOQUE NAO FECHA O APLICATIVO.
    //
    // O relato: "na home, se apertar voltar, na LG ele fecha o aplicativo e no
    // Tizen ele trava". Fechava mesmo — era `sair = 1` direto, no primeiro
    // toque, sem aviso nenhum. Numa TV o Voltar e a tecla mais usada para
    // desfazer, e perder a sessao inteira por um toque a mais e o pior
    // desfecho possivel de um gesto de correcao.
    //
    // DOIS DEGRAUS, que e o que as outras telas do aparelho fazem:
    //   1. Fora do canto superior esquerdo, Voltar SOBE ate ele. E o gesto de
    //      "voltar um nivel" que o dono ja espera, e cobre o caso mais comum
    //      (estava no meio da home e queria voltar ao topo).
    //   2. Ja no canto, o primeiro Voltar so PERGUNTA, por 3 s. So o segundo
    //      dentro da janela sai.
    if (foco.fileira > 0 || foco.coluna > 0) {
      foco.fileira = 0;
      foco.coluna = 0;
      foco.colunaLembrada[0] = 0;
      posDiscoPendente = 0;
      sairPerguntadoEm = 0;
      return;
    }
    if (!sairPerguntadoEm || SDL_GetTicks() - sairPerguntadoEm > HOME_SAIR_MS) {
      sairPerguntadoEm = SDL_GetTicks();
      return;
    }
    sair = 1;
    return;
  }
#endif
  // O OK NAO AGE MAIS NO KEYDOWN. Abrir o titulo ali tornava o "segurar"
  // impossivel: quando a tecla subia, o detalhe ja estava aberto ha meio
  // segundo. Toda a decisao — abrir, "Ver tudo" ou menu do cartaz — mora no
  // KEYUP acima, que e o unico ponto que conhece a DURACAO.
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) return;
  // A PESSOA MEXEU NO D-PAD: a posicao de ontem deixa de estar pendente.
  //
  // Ela nao e aplicada com a home pela metade (posConjuntoBate exige o conjunto
  // inteiro), e sem esta linha, numa home cujas fileiras mudaram desde ontem, o
  // conjunto nunca bateria — a leitura ficaria pendente para sempre e posGravar
  // sairia cedo em toda chamada, congelando o arquivo no estado de ontem. Quem
  // navega assume a posicao; o disco passa a seguir esta sessao.
  if (k == SDLK_RIGHT || k == SDLK_LEFT || k == SDLK_DOWN || k == SDLK_UP)
    posDiscoPendente = 0;
  if (k == SDLK_RIGHT) {
    // O `&&` aqui era um curto-circuito com efeito colateral: escrito como
    // `if (fileira == 0 && !focus_mover(...))`, o focus_mover so era chamado
    // NO HERO — em qualquer outra fileira a seta direita nao movia nada. Mover
    // primeiro, decidir depois.
    (void)focus_mover(&foco, 1, 0);
  } else if (k == SDLK_LEFT) {
  // Esquerda na primeira coluna chama o menu lateral, em QUALQUER fileira —
    // inclusive no hero. Antes o hero era excecao e usava a esquerda para
    // voltar um titulo no carrossel: quem chegava ali (voltando de outra tela,
    // por exemplo) nao tinha como abrir o menu sem antes descer. O carrossel
    // continua acessivel pela direita e pela troca automatica.
    if (foco.coluna == 0) { pedidoMenu = 1; return; }
    focus_mover(&foco, -1, 0);
  }
  else if (k == SDLK_DOWN)  focus_mover(&foco, 0, 1);
  else if (k == SDLK_UP)    focus_mover(&foco, 0, -1);
}

// Reconstroi a lista a partir do catalogo. Chamada a cada quadro porque a
// descoberta roda noutro fio e pode trocar o catalogo a qualquer momento; sai
// cedo quando nada mudou, entao custa uma comparacao de inteiro.
static int filsAplicadas = -1;
// Assinatura das preferencias que MUDAM a lista de fileiras. Sem isto, desligar
// "Continuar assistindo" em Ajustes so valia depois que a rede trocasse o
// catalogo — a comparacao de `nCat` saia cedo e a fileira continuava na tela.
static int prefsAplicadas = -1;
static int assinaturaPrefs(void) {
  return (ajustes_cw_ligado() ? 1 : 0)
       | (ajustes_cw_estilo() << 1)
       | (ajustes_posteres_deitados() ? 8 : 0)
       | (ajustes_rotulos_poster() ? 16 : 0);
}
static void sincronizarFileiras(void) {
  int nCat = cat_n_fileiras(), r, destino = 0;
  int assin = assinaturaPrefs();
  static unsigned ultimaRevisao;
  unsigned revisao = 2166136261u;
  // A escolha LOCAL de fileiras entra na mesma assinatura do catalogo: ordem,
  // liga/desliga, forma, tamanho e limite mudam a lista tanto quanto uma
  // fileira nova da rede. Sem isto, sair de Ajustes deixaria a home igual ate a
  // proxima publicacao da descoberta — que e o defeito que a assinatura de
  // preferencias logo acima existe para nao repetir. Duas chamadas por quadro,
  // nao uma varredura: fil_revisao e fil_limite leem um inteiro sob mutex.
  revisao = (revisao ^ fil_revisao()) * 16777619u;
  revisao = (revisao ^ (unsigned)fil_limite()) * 16777619u;
  for (r = 0; r < nCat; r++) {
    const CatFileira *cf = cat_fileira(r);
    if (!cf) break;
    for (const unsigned char *s = (const unsigned char *)cf->chave; *s; s++)
      revisao = (revisao ^ *s) * 16777619u;
    for (const unsigned char *s = (const unsigned char *)cf->titulo; *s; s++)
      revisao = (revisao ^ *s) * 16777619u;
    revisao = (revisao ^ (unsigned)cf->ini) * 16777619u;
    revisao = (revisao ^ (unsigned)cf->n) * 16777619u;
  }
  if (nCat < 1 || (nCat == filsAplicadas && assin == prefsAplicadas
      && revisao == ultimaRevisao && retomarAplicada == retomarRev)) return;
  // Guardar o estado por chave: inserir o hub não deve transferir a rolagem
  // horizontal de uma fileira para outra.
  //
  // TIRADO ANTES do laco abaixo, que sobrescreve fileiras[]: depois dele nao ha
  // mais como saber em que fileira o foco estava. posCapturar leva a chave do
  // foco, a coluna, o `colunaLembrada[]` INTEIRO e os scrollX — antes daqui
  // saiam so a chave do foco, a coluna e os scrollX, e era a ausencia do
  // colunaLembrada que fazia a memoria de coluna morrer a cada republicacao.
  posCapturar();
  Fileira antigas[MAX_FIL];
  int nAntigas = nFileiras;
  memcpy(antigas, fileiras, sizeof antigas);
  int temDestaque = 0;
  for (r = 0; r < nCat && destino < MAX_FIL - 1; r++) {
    const CatFileira *cf = cat_fileira(r);
    if (!cf) break;
    if (cf->n < 1) continue;
    // `continueWatchingEnabled: false` tira a fileira da home inteira — nao a
    // esvazia, tira. E o que renderModernHomeLayout faz quando
    // computeContinueWatchingRenderState devolve a fileira desligada.
    if (!strcmp(cf->chave, "continue_watching") && !ajustes_cw_ligado()) continue;
    snprintf(fileiras[destino].titulo, sizeof fileiras[destino].titulo, "%s", cf->titulo);
    // "Continuar assistindo" e a unica landscape: e o
    // `continueWatchingCardStyle: "card"` do perfil. Todo o resto e poster 2:3.
    // `continueWatchingCardStyle`: "card" e "largo" desenham landscape, "poster"
    // usa o mesmo 2:3 das outras fileiras. E a preferencia, nao o tipo da
    // fileira, que decide a forma.
    fileiras[destino].tipo = (!strcmp(cf->chave, "continue_watching")
                              && ajustes_cw_estilo() != 2)
                           ? FILEIRA_CONTINUE : perfilCatalogo(cf->titulo);
    if (!strcmp(cf->chave, "continue_watching")) {
      if (ajustes_cw_estilo() == 2) fileiras[destino].tipo = FILEIRA_NORMAL;
    } else if (!temDestaque && cf->base[0] && cf->catId[0]) {
      fileiras[destino].tipo = FILEIRA_DESTAQUE;
      temDestaque = 1;
    }
    // MAX_CARDS - 1: a ultima coluna e do card "Ver tudo". Sem reservar, uma
    // fileira cheia empurraria o card para fora do vetor de animacao.
    fileiras[destino].n   = cf->n > 12 ? 12 : cf->n;
    // UMA COLUNA A MAIS: o card "Ver tudo" no fim. So em fileira que veio de um
    // CATALOGO de addon — "Continuar assistindo" e as listas do Trakt nao tem
    // continuacao para pedir (o base fica vazio nelas).
    fileiras[destino].verTudo = (cf->base[0] && cf->catId[0]) ? 1 : 0;
    snprintf(fileiras[destino].base,  sizeof fileiras[destino].base,  "%s", cf->base);
    snprintf(fileiras[destino].catId, sizeof fileiras[destino].catId, "%s", cf->catId);
    snprintf(fileiras[destino].catTipo, sizeof fileiras[destino].catTipo, "%s", cf->tipo);
    fileiras[destino].ini = cf->ini;
    if(!strcmp(cf->chave,"social_activity"))fileiras[destino].tipo=FILEIRA_SOCIAL;
    snprintf(fileiras[destino].chave, sizeof fileiras[destino].chave,
             "%s", cf->chave);
    destino++;
  }
  if (col_n()) {
    Fileira orig[MAX_FIL];int total=destino;memcpy(orig,fileiras,sizeof orig);destino=0;
    // A ordem que a pessoa gravou na conta GANHA da curadoria fixa abaixo. Sem
    // isto o recurso nao existiria em aparelho com colecoes: a descoberta ja
    // teria posto as fileiras na ordem da conta e esta tabela as reordenaria
    // de novo, e a pessoa veria a TV ignorar o que ela arrumou no app web.
    // Os grupos de colecao continuam entrando — no FIM, que e onde a regra de
    // uniao poe o que e local e o remoto nao conhece.
    int ordemDaConta = catordem_tem_ordem();
    if (ordemDaConta) { memcpy(fileiras,orig,sizeof(Fileira)*(size_t)total); destino=total; }
    const char *ids[]={"continue_watching","social_activity","now_playing_movies","@Streaming",
      "trending_movies","trending_series","@Themes","ai_movies_for_you",
      "ai_series_for_you","snoak_top100_movies","snoak_top100_series",
      "@Awards","@Directors","@Genres"};
    const char *names[]={"Continuar assistindo","Entre amigos","Recent Release","Streaming",
      "Trending Movies","Trending Series","Themes","Picked for You · Movies",
      "Picked for You · Series","Top 100 · Movies","Top 100 · Series",
      "Awards","Directors","Genres"};
    // A curadoria conhecida ganha prioridade, mas nao e uma lista de corte.
    // O web mantem chaves novas no fim e a home nativa precisa fazer o mesmo:
    // catalogos e grupos que nao estavam nesta tabela continuam acessiveis.
    for(size_t s=0;s<sizeof ids/sizeof ids[0] && destino<MAX_FIL;s++) {
      // Com ordem da conta, so os grupos de colecao ('@') entram por aqui: as
      // entradas de catalogo desta tabela promoveriam fileiras para cima da
      // ordem escolhida.
      if(ordemDaConta && ids[s][0]!='@') continue;
      if(ids[s][0]=='@') {
        Fileira v={0};v.n=col_grupo(ids[s]+1,v.folders,MAX_CARDS);
        if(!v.n)continue;
        v.tipo=FILEIRA_CATALOGOS;
        col_chave_grupo(ids[s]+1,v.chave,sizeof v.chave);
        snprintf(v.titulo,sizeof v.titulo,"%s",names[s]);fileiras[destino++]=v;
      } else for(int k=0;k<total;k++) {
        if(strcmp(orig[k].catId,ids[s])&&strcmp(orig[k].chave,ids[s]))continue;
        fileiras[destino]=orig[k];
        snprintf(fileiras[destino].titulo,sizeof fileiras[destino].titulo,"%s",names[s]);
        if(s==1)fileiras[destino].tipo=FILEIRA_SOCIAL;
        else if(s==2)fileiras[destino].tipo=FILEIRA_DESTAQUE;
        else if(s==9||s==10)fileiras[destino].tipo=FILEIRA_TOP10;
        else if(s!=0)fileiras[destino].tipo=FILEIRA_NORMAL;
        destino++;break;
      }
    }
    // Tudo que nao casou com a curadoria acima permanece na ordem declarada
    // pelo addon/preferencia. A comparacao por chave evita duplicar uma linha
    // especial que ja foi promovida.
    for(int k=0;k<total && destino<MAX_FIL;k++) {
      int visto=0;
      for(int j=0;j<destino;j++) if(!strcmp(fileiras[j].chave,orig[k].chave)){visto=1;break;}
      if(!visto) fileiras[destino++]=orig[k];
    }
    // Grupos adicionais tambem sao configuracao do usuario. Nao dependem de
    // nomes que conheciamos quando a tabela foi escrita, e cada grupo aparece
    // uma vez com todos os seus folders.
    for(int i=0;i<col_n() && destino<MAX_FIL;i++) {
      const ColFolder *folder=col_folder(i);
      int grupoVisto=0;
      if(!folder||!folder->group[0])continue;
      for(int j=0;j<destino;j++) {
        char chave[192];col_chave_grupo(folder->group,chave,sizeof chave);
        if(!strcmp(fileiras[j].chave,chave)){grupoVisto=1;break;}
      }
      if(grupoVisto)continue;
      { Fileira v={0};
        v.n=col_grupo(folder->group,v.folders,MAX_CARDS);
        if(!v.n)continue;
        v.tipo=FILEIRA_CATALOGOS;
        col_chave_grupo(folder->group,v.chave,sizeof v.chave);
        snprintf(v.titulo,sizeof v.titulo,"%s",folder->group);
        fileiras[destino++]=v;
      }
    }
  }
  // FORMA E TAMANHO ESCOLHIDOS POR FILEIRA (Ajustes -> Fileiras da Home).
  // Aplicado AQUI, antes da normalizacao do Top 10 logo abaixo: quem escolhe
  // "Top 10" espera a pilha com o ranking, e ela sai daquele laco. Depois dele,
  // a mesma escolha viraria uma fileira de cartazes de 212 px — o ajuste
  // pareceria sem efeito.
  for(int i=0;i<destino;i++) {
    int t = fil_tipo(fileiras[i].chave);
    fileiras[i].escala = fil_escala(fileiras[i].chave);
    if (t != FIL_TIPO_AUTO) fileiras[i].tipo = tipoDaEscolha(t);
  }
  for(int i=0;i<destino;i++) {
    Fileira *s=&fileiras[i];s->stackN=0;
    if(s->tipo==FILEIRA_TOP10 && s->base[0] && s->catId[0]) {
      s->stackN=s->n;s->n=1;s->verTudo=0;
    }
  }
  int socialExiste=0;
  for(int i=0;i<destino;i++)if(fileiras[i].tipo==FILEIRA_SOCIAL)socialExiste=1;
  if(!socialExiste && destino<MAX_FIL) {
    int pos=destino>0?1:0;
    memmove(fileiras+pos+1,fileiras+pos,(destino-pos)*sizeof *fileiras);
    Fileira *s=&fileiras[pos];memset(s,0,sizeof *s);
    s->tipo=FILEIRA_SOCIAL;s->ini=-1;s->n=1;
    snprintf(s->titulo,sizeof s->titulo,"Entre amigos");
    snprintf(s->chave,sizeof s->chave,"social_activity");destino++;
  }
  // Um retorno do player e contexto, nao catalogo: entra acima das fileiras e
  // desaparece quando nao existe sessao incompleta. Nao duplica dados nem faz
  // rede; aponta para o item que o player acabou de atualizar em memoria.
  if (retomarId[0]) retomarIndice = cat_indice_por_imdb(retomarId);
  if (retomarIndice >= 0 && destino < MAX_FIL) {
    memmove(fileiras + 1, fileiras, sizeof(Fileira) * (size_t)destino);
    memset(&fileiras[0], 0, sizeof fileiras[0]);
    snprintf(fileiras[0].titulo, sizeof fileiras[0].titulo, "Retomar agora");
    snprintf(fileiras[0].chave, sizeof fileiras[0].chave, "last_session");
    fileiras[0].tipo = FILEIRA_RETORNO;
    fileiras[0].ini = retomarIndice; fileiras[0].n = 1;
    destino++;
  }
  // --- ESCOLHA LOCAL: registro, ordem, ocultacao e LIMITE --------------------
  //
  // POR QUE AQUI E NAO NA DESCOBERTA. A descoberta ja corta o que vai PEDIR
  // pela rede (menos fileiras = menos GET, e e la que o limite economiza
  // trabalho de verdade), mas ela nao conhece a lista final: o feed dos amigos,
  // os grupos de colecao e "Retomar agora" nascem aqui. O limite que a pessoa
  // ve tem de valer sobre o que a tela desenha, entao o corte final e neste
  // ponto — e e um corte na LISTA, nao no desenho: fileira invisivel desenhada
  // continuaria custando quadro.
  //
  // Cortar a lista e seguro para o foco e para a rolagem porque as duas coisas
  // sao reencontradas por CHAVE mais abaixo, nunca por indice.
  {
    // static, e nao pilha: sao ~35 KB e esta funcao ja carrega dois vetores
    // desse tamanho (antigas, orig). Roda so no fio de desenho.
    static Fileira arranjo[MAX_FIL];
    const char *ch[MAX_FIL];
    int ord[MAX_FIL], q, k, w = 0, lim = fil_limite();
    for (q = 0; q < destino; q++) {
      // REGISTRA TAMBEM O QUE VAI SAIR abaixo. E o registro que deixa a tela de
      // Ajustes RELIGAR uma fileira desligada: desligada, ela nao existe mais
      // nem aqui nem em cat_fileira(), e sem a lista de conhecidas desligar
      // seria irreversivel pela TV.
      // Addon vazio: quem sabe o nome dele e a descoberta, que registra a mesma
      // chave com ele (o primeiro a saber preenche). Daqui saem o TIPO do
      // catalogo e a CONTAGEM, que so existem depois de a fileira ser montada.
      if (strcmp(fileiras[q].chave, "last_session"))
        fil_registrar(fileiras[q].chave, fileiras[q].titulo,
                      "", fileiras[q].catTipo, fileiras[q].n);
      ch[q] = fileiras[q].chave;
    }
    fil_gravar_registro();
    // A lista de Ajustes passa a espelhar ESTA ordem enquanto ninguem tiver
    // reordenado. Sem isto o primeiro movimento em Ajustes reembaralhava a home
    // inteira em vez de mover uma fileira — ver fil_espelhar_ordem.
    fil_espelhar_ordem(ch, destino);
    // "Retomar agora" e contexto do player, nao fileira de catalogo: fica presa
    // no topo, fora da ordem e fora do liga/desliga. Ela aparece por causa de
    // uma sessao interrompida e desaparece sozinha; deixar a pessoa mover ou
    // desligar uma fileira que ela nao controla seria um ajuste fantasma.
    if (destino > 0 && !strcmp(fileiras[0].chave, "last_session"))
      arranjo[w++] = fileiras[0];
    q = fil_unir(ch, destino, ord, MAX_FIL);
    for (k = 0; k < q && w < MAX_FIL; k++) {
      Fileira *f = &fileiras[ord[k]];
      if (!strcmp(f->chave, "last_session")) continue;
      if (fil_oculta(f->chave)) continue;
      arranjo[w++] = *f;
    }
    // QUANTAS FILEIRAS ESTE CORTE ENGOLIU. O aviso do fim da home contava so
    // `desc_catalogos_fora()` — catalogos que a DESCOBERTA nao chegou a pedir.
    // O corte daqui e outro: ele acontece depois, sobre a lista ja montada, e
    // pega tambem as fileiras de COLECAO, que nao passam pela descoberta.
    //
    // E o relato do rawldon (#30): ligar o Trakt fez colecoes sumirem da home.
    // Nao sumiram — foram empurradas para fora do limite pelas fileiras que o
    // Trakt trouxe, e como elas nao sao catalogo, o aviso que existe desde o
    // #11 nao dizia nada. Silencio, que e exatamente o defeito que aquele aviso
    // foi criado para nao deixar acontecer.
    //
    // Fileira que a pessoa DESLIGOU nao conta: ela ja saiu no laco acima, e
    // anunciar como "cabe mais uma" o que ela mandou embora seria ruido.
    cortadasPeloLimite = (w > lim) ? w - lim : 0;
    if (w > lim) w = lim;
    memcpy(fileiras, arranjo, sizeof(Fileira) * (size_t)w);
    destino = w;
  }
  nFileiras = destino;
  retomarAplicada = retomarRev;
  ultimaRevisao = revisao;
  filsAplicadas = nCat;
  prefsAplicadas = assin;
  memset(animFoco, 0, sizeof animFoco);
  memset(velX, 0, sizeof velX);
  memset(scrollX, 0, sizeof scrollX);
  for (r = 0; r < nFileiras; r++)
    for (int a = 0; a < nAntigas; a++)
      if (!strcmp(fileiras[r].chave, antigas[a].chave)) {
        if(fileiras[r].tipo==FILEIRA_TOP10 && antigas[a].tipo==FILEIRA_TOP10 &&
           !antigas[a].stackN && antigas[a].verTudo && fileiras[r].stackN) {
          fileiras[r].n=fileiras[r].stackN<10?fileiras[r].stackN:10;
          fileiras[r].stackN=0;fileiras[r].verTudo=1;
        }
        break;
      }
  expFileira = expColuna = -1; expAbre = 0.0f;
  if (nFileiras < 1) return;
  {
    int cols[MAX_FIL], k;
    // PRESERVAR O FOCO. focus_iniciar faz memset e zera fileira e coluna, e a
    // descoberta agora publica o catalogo A CADA FILEIRA que chega da rede —
    // sao ~16 publicacoes nos primeiros segundos. Com o reset, o foco do dono
    // saltaria para o primeiro card umas dezesseis vezes enquanto ele tenta
    // navegar. Antes isso nao aparecia porque a publicacao era unica.
    //
    // A fileira e reencontrada pela CHAVE do catalogo, nao pelo indice: uma
    // fileira nova pode entrar no meio (a ordem vem de art/fileiras.txt), e o
    // indice antigo passaria a apontar para outra coisa.
    for (k = 0; k < nFileiras; k++)
      cols[k] = fileiras[k].n + (fileiras[k].verTudo ? 1 : 0);
    focus_iniciar(&foco, nFileiras, cols);
    posAplicarTabela(posViva, nPosViva, posVivaFoco, posVivaCol);
    // E SO AGORA a posicao de ontem pode entrar: com as fileiras montadas e o
    // conjunto delas igual ao que foi gravado. Vem DEPOIS da tabela viva de
    // proposito — se as duas valem, a desta sessao e a mais nova.
    if (posDiscoPendente && posConjuntoBate(posDisco, nPosDisco)) {
      posDiscoPendente = 0;
      posAplicarTabela(posDisco, nPosDisco, posDiscoFoco, posDiscoCol);
      // O que esta na tela passa a ser exatamente o que esta no disco: nao ha
      // o que gravar, e o repouso nao deve disparar por causa da restauracao.
      posSujo = 0;
    }
    // O INDICE do foco muda entre remontagens mesmo com a posicao intacta (uma
    // fileira nova entrou acima). Reacertar o detector aqui evita que cada uma
    // das ~16 publicacoes do arranque reinicie o relogio de repouso e adie a
    // gravacao para sempre. `posSujo` NAO e limpo: se havia uma gravacao
    // pendente antes da remontagem, ela continua devida.
    posUltFil = foco.fileira;
    posUltCol = foco.coluna;
  }
  // Diz TAMBEM o limite e quantas vinham do catalogo. Com um numero so, uma
  // home de 5 fileiras nao distinguia "o limite e 5" de "so 5 catalogos
  // responderam" — as duas perguntas que o dono faz quando a home vem curta.
  printf("[home] %d fileiras na tela (limite %d, %d fileira(s) no catalogo)\n",
         nFileiras, fil_limite(), nCat);
}

int home_tem_fileiras(void) { return nFileiras > 0; }

void home_atualizar(float dt, Uint32 agora) {
  sincronizarFileiras();

  const int motionReduzido = ajustes_animacoes_reduzidas();
  if (okPressionando && foco_pode_pressao_longa())
    // O MESMO NV_HOLD_MS do resto do app: a barra que enche na tela E o
    // gatilho, entao ela nao pode correr num relogio proprio. Aqui havia um
    // NV_HOLD_FEEDBACK_MS de 110 ms, e era ele quem abria o menu.
    okHold = anim_clamp((agora - okDesde) / (float)NV_HOLD_MS, 0.0f, 1.0f);
  else if (!okPressionando)
    okHold = 0.0f;
  if (okPressionando && okHold >= 1.0f && !okLongDisparado) {
    okLongDisparado = 1;
    okConsumirSoltura = 1;
    okPressionando = 0;
    okDesde = 0;
    // O menu contextual continua sendo o dono das acoes e da UI. A home so
    // dispara uma vez no limiar e consome o KEYUP seguinte.
    ctx_abrir(fileiras[foco.fileira].ini + foco.coluna);
  }

  // O catalogo pode encolher entre duas respostas. Normalizar os indices do
  // carrossel evita que um estado stale caia no wrap de cat_item().
  { int total = nAcervoHero();
    if (heroAtual < 0 || heroAtual >= total) heroAtual = 0;
    if (heroAnterior < 0 || heroAnterior >= total) heroAnterior = heroAtual;
    if (heroPendente < 0 || heroPendente >= total) heroPendente = heroAtual;
  }

  // O HERO SEGUE O FOCO. Pedido do dono, e e uma DIVERGENCIA DELIBERADA do app
  // web: medi duas vezes com o foco andando de verdade (o card mudou de "54
  // minutos restantes" para "1h 12m restantes") e a arte do
  // `.home-hero-backdrop` continuou a mesma — no web ela nao acompanha a
  // selecao. Fica registrado para ninguem "corrigir" isto de volta achando que
  // e desvio: e melhoria escolhida, nao erro.
  //
  // Enquanto ha foco num card, o carrossel automatico nao roda: duas fontes
  // mexendo na mesma arte dariam trocas em cima da escolha do usuario.
  {
    int alvo = -1;
    if (foco.fileira >= 0 && foco.fileira < nFileiras) {
      int i = fileiras[foco.fileira].ini + foco.coluna;
      if (fileiras[foco.fileira].tipo == FILEIRA_CATALOGOS)
        i = -1;
      else if (foco.coluna >= fileiras[foco.fileira].n) i = -1;
      if (i >= 0 && i < cat_n()) alvo = i;
    }
    // TROCA SO COM O FOCO EM REPOUSO. `heroPendente` e o candidato; enquanto o
    // dono anda pela fileira ele muda a cada passo e o relogio reinicia, entao
    // nenhuma troca chega a acontecer. Quando o foco para por
    // NV_HERO_REPOUSO_MS, o candidato vira o heroi.
    //
    // Isto nao e so estetica: cada troca pede uma textura de 1920 (~8 MB), e
    // atravessar uma fileira pedia uma dezena delas em dois segundos — o cache
    // estourava e despejava os posteres visiveis. Ver a nota em layout.h.
    if (alvo >= 0 && alvo != heroPendente) {
      heroPendente = alvo;
      heroPendenteEm = agora;
      // Voltou para a arte que ja esta no ar: cancela a troca que ainda nao
      // aconteceu, senao ela dispararia depois sem ninguem ter pedido.
      if (heroPendente == heroAtual) heroDesejado = -1;
    }
    if (alvo >= 0 && heroPendente != heroAtual &&
        agora - heroPendenteEm >= NV_HERO_REPOUSO_MS) {
      // So ANUNCIA o desejo. Quem efetiva a troca e o desenho, quando a textura
      // da arte nova estiver pronta — ver heroDesejado.
      if (heroDesejado != heroPendente) heroDesejadoEm = agora;
      heroDesejado = heroPendente;
    } else if (alvo < 0 && agora >= heroTrocaEm) {
      // Sem card em foco, agenda o proximo item e deixa o desenho efetivar a
      // troca somente quando a textura ou o placeholder ja estiver pronto.
      int total = nAcervoHero();
      int proximo = total > 0 ? (heroAtual + 1) % total : 0;
      heroPendente = proximo;
      heroPendenteEm = agora - NV_HERO_REPOUSO_MS;
      if (heroDesejado != proximo) heroDesejadoEm = agora;
      heroDesejado = proximo;
      heroTrocaEm = agora + NV_HERO_INTERVALO_MS;
    }
  }
  // Relogio da expansao. Zera a cada movimento; conta so com o foco parado.
  if (ajustes_expandir_poster()) {
    if (foco.fileira != expFileira || foco.coluna != expColuna) {
      expFileira = foco.fileira; expColuna = foco.coluna;
      expDesde = agora;
      expAbre = 0.0f;            // fecha na hora; abrir e que e gradual
    }
    { float atraso = ajustes_expandir_poster_atraso();
      int pronto = expDesde && (agora - expDesde) >= (Uint32)(atraso * 1000.0f);
      // Fileira DEITADA (e a de continuar assistindo) ja mostra a arte larga:
      // nao ha para o que expandir.
      if (pronto && podeExpandir(foco.fileira))
        expAbre = motionReduzido ? 1.0f
                   : anim_mola(expAbre, 1.0f, dt, NV_MOLA_TELA); }
  } else {
    expAbre = 0.0f; expFileira = expColuna = -1;
  }

  if (heroSai > 0.0f) {
    heroSai -= dt * (1000.0f / NV_HERO_FADE_MS);
    if (heroSai < 0.0f) heroSai = 0.0f;
    heroEntra = motionReduzido ? 1.0f : 1.0f - heroSai;
  } else {
    heroEntra = 1.0f;
  }

  // O passeio automatico do foco era so para ver o protótipo se mexendo sem
  // ninguem no controle. Com o app navegavel ele atrapalha: rouba o foco no
  // meio de qualquer teste.

  for (int r = 0; r < nFileiras; r++) {
    int nAnim = fileiras[r].n + (fileiras[r].verTudo ? 1 : 0);
    if (nAnim > MAX_CARDS) nAnim = MAX_CARDS;
    for (int c = 0; c < nAnim; c++) {
      float alvo = focus_indice(&foco, r, c) ? 1.0f : 0.0f;
      animFoco[r][c] = motionReduzido
                     ? alvo
                     : anim_mola(animFoco[r][c], alvo, dt,
                                 alvo > animFoco[r][c] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
    }
    if (r == foco.fileira) {
      // Roda so o necessario para o item focado caber na area util. Deslocar
      // proporcional a coluna, como estava, jogava o primeiro card para fora da
      // tela assim que o foco ia para o segundo — some conteudo a esquerda sem
      // que o usuario tenha andado ate la.
      float lw = larguraFil(r);
      float passo = passoFil(r);
      float esq = (float)foco.coluna * passo;
      float dir = esq + lw;
      float util = NV_TELA_W - ajustes_conteudo_x() - NV_HOME_SAFE_RIGHT;
      float alvo = scrollX[r];
      // a folga cobre o crescimento do foco: o card cresce para os dois lados,
      // e sem reservar essa metade ele encosta na borda ao ficar em foco
      float folga = lw * escalaDe(fileiras[r].tipo) * 0.5f;
      if (dir + folga - alvo > util)  alvo = dir + folga - util;
      if (esq - alvo < 0.0f)  alvo = esq;
      if (alvo < 0) alvo = 0;
      scrollX[r] = anim_mola2_reduzida(&velX[r], scrollX[r], alvo, dt,
                                       NV_MOLA2_SCROLL, motionReduzido);
    }
  }

  // A FILEIRA EM FOCO FICA SEMPRE NO MESMO Y, e as de cima SOMEM.
  //
  // Observado nas duas capturas de referencia do dono: com o foco em "Continuar
  // assistindo" esse titulo aparece na mesma altura em que, ao descer uma
  // fileira, aparece "For You - Filme". A fileira anterior nao sobe — ela deixa
  // de ser desenhada. Palavras dele: "quando desce uma linha as coisas somem e
  // nao sobem".
  //
  // O que estava aqui era uma CAMERA: mantinha a fileira em foco dentro de um
  // viewport e rolava o minimo necessario. Isso desliza tudo para cima, e foi o
  // que fez o texto do hero passar por cima do titulo da fileira.
  //
  // O deslocamento e a soma das fileiras ANTES da que tem foco, entao o topo da
  // focada cai exatamente em NV_SHELF_TOP. Continua com mola: o salto seco
  // entre fileiras de alturas diferentes le como corte, nao como navegacao.
  float alvoY = 0.0f;
  { int r = foco.fileira;
    for (int i = 0; i < r && i < nFileiras; i++)
      alvoY += NV_LEGACY_ROW_HEAD_H + alturaTotalFil(i) + fileiraGap();
  }
  scrollY = anim_mola2_reduzida(&velY, scrollY, alvoY, dt,
                                NV_MOLA2_SCROLL, motionReduzido);

  // GRAVAR A POSICAO — em repouso, nunca por quadro. Ver NV_POS_REPOUSO_MS.
  //
  // O gatilho e o foco, e so ele: `scrollX` da fileira focada e derivado de
  // foco.coluna logo acima (a mola persegue um alvo calculado a partir dela) e
  // o das outras nem se mexe, entao vigiar o foco cobre as duas coisas — e
  // esperar o repouso garante que o valor gravado e o da mola JA ASSENTADA, e
  // nao um quadro do meio da animacao.
  //
  // `scrollY` fica de fora do arquivo por ser derivado tambem: o alvo logo
  // acima e a soma das alturas das fileiras ANTES de foco.fileira, recalculado
  // a cada quadro. Gravado, ele seria um valor a mais para discordar do foco.
  if (foco.fileira != posUltFil || foco.coluna != posUltCol) {
    posUltFil = foco.fileira;
    posUltCol = foco.coluna;
    posMexeuEm = agora;
    posSujo = 1;
  } else if (posSujo && agora - posMexeuEm >= NV_POS_REPOUSO_MS) {
    posSujo = 0;
    posGravar();
  }
}

// ---------- Hero do layout moderno legacy ----------------------------------
//
// A mídia ocupa a direita dos 650px superiores; o texto fica no bloco esquerdo
// e as fileiras rolam em um viewport independente abaixo. O hero não captura
// foco: a navegação espacial começa no primeiro card, como no DOM legacy.
// Rect da ARTE do hero no ultimo quadro. A tela de detalhe le isto para
// comecar o backdrop dela EXATAMENTE onde a arte ja estava, em vez de aparecer
// do nada: o fundo e o mesmo do titulo, entao ele nao deve piscar nem crescer.
static GfxRect heroArteRect = { 0, 0, NV_TELA_W, NV_TELA_H };
void home_hero_rect(float *x, float *y, float *w, float *h) {
  *x = heroArteRect.x; *y = heroArteRect.y;
  *w = heroArteRect.w; *h = heroArteRect.h;
}

// `saida` = 0..1 de quanto o detalhe ja tomou a tela. So o TEXTO do hero sai
// (desce e apaga); a arte fica parada, porque e a mesma arte que o detalhe vai
// usar. Era isso que faltava para a abertura ler como rearranjo de layout e
// nao como troca de tela.
static void desenhaHero(Uint32 agora, float saida) {
  (void)agora;
  const int motionReduzido = ajustes_animacoes_reduzidas();
  float aArte = 1.0f;
  // MEDIDO no app web: .home-modern-hero-media fica em x=555, y=0, 1421x670.
  // A conta que estava aqui (0.28*W - 56 = 481,6 de largura 1438) vinha de
  // proporcao estimada e punha a arte 73px a esquerda do lugar.
  // Faixa ou tela cheia, conforme `modernHeroFullScreenBackdropEnabled`. Sao os
  // dois estados da MESMA tela, nao dois layouts — e cada um tem a sua rampa de
  // degrade, medida separadamente (ver GFX_HERO e GFX_HERO_CHEIO em gfx.c).
  int cheio = ajustes_hero_cheio();
  // Em tela cheia o bloco sobe 70px (ver layout.h).
  GfxModo modoHero = cheio ? GFX_HERO_CHEIO : GFX_HERO;
  GfxRect r = cheio ? (GfxRect){ 0, 0, NV_TELA_W, NV_HERO_CHEIO_H }
                    : (GfxRect){ NV_HERO_ARTE_X, 0, NV_HERO_ARTE_W, NV_HERO_ARTE_H };

  if(foco.fileira>=0 && foco.fileira<nFileiras && fileiras[foco.fileira].tipo==FILEIRA_SOCIAL) {
    float x=ajustes_conteudo_x(),a=1-saida;
    gfx_rect((GfxRect){0,0,NV_TELA_W,NV_TELA_H},0,GFX_SOCIAL,0,0,0,0,1,1,1,1);
    const Fileira *s=&fileiras[foco.fileira];
    const CatItem *p=(s->ini>=0&&foco.coluna<s->n)
                    ?cat_item_exato(s->ini+foco.coluna):NULL;
    if(p) {
      const char *arte=arte_por_formato(p, 1);
      GLuint ta=arte?tex_obter_hero(arte):0;
      // A atividade continua com um ambiente discreto, mas quando o Trakt
      // trouxe arte real ela vira o assunto do hero. A pessoa fica apenas na
      // ficha social, onde o avatar tem contexto e não compete com o titulo.
      if(ta){gfx_tex_aspect_atual=tex_aspecto(arte);
        gfx_rect(r,ta,modoHero,0,0,0,0,0,0,0,aArte);gfx_tex_aspect_atual=0;}
      else if (!arte) desenhaArteAusente(r, 0.0f, p, aArte);

      const char *nome=p->socialNome[0]&&strcmp(p->socialNome,"Amigo")?p->socialNome:NULL;
      char autoria[240];
      if(nome&&p->socialAcao[0])snprintf(autoria,sizeof autoria,"%s  ·  %s",nome,p->socialAcao);
      else if(nome)snprintf(autoria,sizeof autoria,"%s",nome);
      else snprintf(autoria,sizeof autoria,"%s",p->socialAcao);
      txt_desenhar_alpha(txt_linha_corta(TXT_HERO_META,autoria,210,210,221,255,680),x,146,a);

      GLuint tl=p->logo[0]?tex_obter_larg(p->logo,520):0;
      if(tl&&tex_aspecto(p->logo)>0){
        float ap=tex_aspecto(p->logo),w=520,h=w/ap;
        if(h>104){h=104;w=h*ap;}
        gfx_rect((GfxRect){x,208,w,h},tl,tex_marca_escura(p->logo)?GFX_MARCA:GFX_TEXTO,
                 0,0,0,0,1,1,1,a);
      } else if(p->titulo[0]) {
        txt_desenhar_alpha(txt_linha_corta(TXT_TITULO1,p->titulo,244,243,247,255,680),x,208,a);
      }
      if(p->direcao[0])
        txt_desenhar_alpha(txt_linha_corta(TXT_CALLOUT,p->direcao,230,231,238,255,680),x,326,a);
      if(p->meta[0])
        txt_desenhar_alpha(txt_linha_corta(TXT_HERO_META,p->meta,190,194,205,255,680),x,364,a);
      if(p->sinopse[0])
        txt_bloco(TXT_HERO_SIN,p->sinopse,229,231,237,x,402,700,31,a,2);
    } else {
      const char *marca=extras_caminho_marca_nome("trakt_wordmark");
      GLuint logo=tex_obter(marca);
      float marcaAsp=logo?tex_aspecto(marca):2.66f;
      if(marcaAsp<=0)marcaAsp=2.66f;
      if(logo)gfx_rect((GfxRect){x,144,44*marcaAsp,44},logo,GFX_MARCA,0,0,0,0,.96f,.94f,.95f,a);
      txt_desenhar_alpha(txt_linha(TXT_HERO_META,"SUA COMUNIDADE",210,191,199,255),x+44*marcaAsp+24,154,a);
      txt_desenhar_alpha(txt_linha(TXT_TITULO1,"Boas histórias conectam.",244,243,247,255),x,226,a);
      txt_bloco(TXT_HERO_SIN,"Descubra o que seus amigos estão vendo.\nUma nova recomendação pode começar aqui.",187,190,202,x,330,740,36,a,2);
    }
    heroArteRect=r;
    return;
  }

  if(foco.fileira>=0&&foco.fileira<nFileiras&&fileiras[foco.fileira].tipo==FILEIRA_CATALOGOS) {
    const ColFolder *folder=col_folder(fileiras[foco.fileira].folders[foco.coluna]);
    if(folder) {
      if(folder->editorial) {
        /* Art is authored for this rectangle, not cropped as a movie backdrop.
           The neutral canvas continues below it; no art behind the shelves. */
        float x=ajustes_conteudo_x(),a=1-saida;
        GLuint art=tex_obter_hero(folder->hero);
        GfxRect header={0,0,1920,500};
        if(folder->editorial==2&&tex_aspecto(folder->hero)>0) {
          float aspect=tex_aspecto(folder->hero);
          header.w=fminf(1920,header.h*aspect);header.h=header.w/aspect;
          header.x=1920-header.w;
        }
        if(art)gfx_rect(header,art,folder->editorial==2?GFX_EDITORIAL:GFX_TEXTO,0,0,0,0,1,1,1,a);
        heroArteRect=header;
        int director=!strcasecmp(folder->group,"Directors");
        const char *section=director?"DIRETORES":!strcmp(folder->group,"Streaming")?"STREAMING":!strcmp(folder->group,"Themes")?"TEMAS":!strcmp(folder->group,"Genres")?"GÊNEROS":"COLEÇÕES";
        txt_desenhar_alpha(txt_linha(TXT_HERO_META,section,190,193,200,255),x,122,a);
        txt_bloco(TXT_TITULO1,folder->title,244,243,247,x,183,860,72,a,2);
        char caption[160];
        snprintf(caption,sizeof caption,"%s  ·  %d %s",i18n(director?"Filmografia":"Seleção de cinema e séries"),folder->nSources,i18n(folder->nSources==1?"lista":"listas"));
        txt_desenhar_alpha(txt_linha_corta(TXT_HERO_META,caption,190,193,200,255,860),x,358,a);
        txt_desenhar_alpha(txt_linha(TXT_HERO_META,"OK para explorar",224,225,230,255),x,406,a);
        return;
      }
      int ehDiretor=!strcasecmp(folder->group,"Directors");
      // A colecao ja traz o banner certo: e um fundo neutro, sem lettering,
      // feito para receber o conteudo por cima. O retrato do diretor entra como
      // uma segunda camada dissolvida no lado direito — nunca como um card e
      // nunca como o backdrop de um filme conhecido.
      const char *art=folder->hero[0]?folder->hero:folder->cover;
      GLuint t=0;
      if (ehDiretor) diretor_pedir(folder->title);
      if (!t && art[0]) t=tex_obter_hero(art);
      if(t){gfx_tex_aspect_atual=tex_aspecto(art);gfx_rect(r,t,modoHero,0,0,0,0,0,0,0,aArte);gfx_tex_aspect_atual=0;}
      heroArteRect=r;
      float x=ajustes_conteudo_x(),a=1-saida;
      TxtLinha group=txt_linha(TXT_HERO_META,folder->group,201,206,218,255);
      txt_desenhar_alpha(group,x,NV_COLLECTION_HERO_GROUP_Y,a);
      if (ehDiretor) {
        const char *foto=diretor_foto(folder->title);
        GLuint retrato=foto[0]
          ?tex_obter_larg(foto,cheio?1280.0f:1100.0f):0;
        if (retrato) {
          // O shader conserva a proporcao vertical e dissolve as quatro bordas.
          // A largura e intencionalmente generosa para a cabeca ter a mesma
          // presenca visual do exemplo aprovado, sem parecer uma foto espremida.
          GfxRect pr=cheio ? (GfxRect){840.0f,-20.0f,1080.0f,1120.0f}
                           : (GfxRect){980.0f,-15.0f,940.0f,700.0f};
          gfx_tex_aspect_atual=tex_aspecto(foto);
          gfx_rect(pr,retrato,GFX_RETRATO,0,0,0,0,0,0,0,aArte);
          gfx_tex_aspect_atual=0.0f;
        }
        TxtLinha name=txt_linha_corta(TXT_TITULO1,folder->title,244,243,247,255,780);
        txt_desenhar_alpha(name,x,NV_COLLECTION_HERO_LOGO_Y,a);
        const char *meta=diretor_meta(folder->title);
        if (meta[0])
          txt_desenhar_alpha(txt_linha_corta(TXT_HERO_META,meta,201,206,218,255,780),
                             x,NV_COLLECTION_HERO_LOGO_Y+92.0f,a);
        const char *con=diretor_conhecido(folder->title);
        if (con[0]) {
          char linha[300];
          snprintf(linha,sizeof linha,"Conhecido por  %s",con);
          txt_desenhar_alpha(txt_linha_corta(TXT_HERO_META,linha,220,224,233,255,780),
                             x,NV_COLLECTION_HERO_LOGO_Y+136.0f,a);
        }
        char caption[96];
        snprintf(caption, sizeof caption, i18n("%d %s · OK para explorar"),
                 folder->nSources, i18n(folder->nSources == 1 ? "lista" : "listas"));
        txt_desenhar_alpha(txt_linha_corta(TXT_HERO_SIN, caption,
                                           205, 210, 221, 255, 780),
                           x, NV_COLLECTION_HERO_CAPTION_Y, a);
        return;
      }
      // As logos de colecao sao arte, nao texto rasterizado. O limite de
      // decode fica acima do tamanho desenhado para preservar nitidez quando
      // a proporcao da logo pede a altura maxima.
      GLuint logo=(!ehDiretor && folder->logo[0])
        ?tex_obter_larg(folder->logo,NV_COLLECTION_HERO_LOGO_MAX_W+40.0f):0;
      float ap=logo?tex_aspecto(folder->logo):0;
      float fimTitulo=NV_COLLECTION_HERO_LOGO_Y+NV_COLLECTION_HERO_LOGO_MAX_H;
      if(logo&&ap>0){
        float w=NV_COLLECTION_HERO_LOGO_MAX_W,h=w/ap;
        if(h>NV_COLLECTION_HERO_LOGO_MAX_H){h=NV_COLLECTION_HERO_LOGO_MAX_H;w=h*ap;}
        gfx_rect((GfxRect){x,NV_COLLECTION_HERO_LOGO_Y,w,h},logo,
                 tex_marca_escura(folder->logo)?GFX_MARCA:GFX_TEXTO,
                 0,0,0,0,.96f,.97f,.98f,a);
        fimTitulo=NV_COLLECTION_HERO_LOGO_Y+h;
      } else {
        TxtLinha name=txt_linha_corta(TXT_TITULO1,folder->title,241,243,247,255,700);
        txt_desenhar_alpha(name,x,NV_COLLECTION_HERO_LOGO_Y,a);
        fimTitulo=NV_COLLECTION_HERO_LOGO_Y+name.h;
      }
      char caption[96];snprintf(caption,sizeof caption,i18n("%d %s · OK para explorar"),folder->nSources,i18n(folder->nSources==1?"lista":"listas"));
      float yCap=NV_COLLECTION_HERO_CAPTION_Y;
      if(ehDiretor) {
        // Ficha do TMDB abaixo do nome: quem e, quando e onde nasceu, tres
        // linhas de bio e os titulos por que e conhecido. Chega em segundo
        // plano; ate chegar a legenda fica onde sempre ficou.
        diretor_pedir(folder->title);
        if(diretor_pronto(folder->title)) {
          // O bloco comeca logo abaixo do nome e TERMINA antes do cabecalho da
          // fileira (NV_SHELF_TOP): o numero de linhas da bio e o que cede.
          // Largura 780: fica aquem do cartao da capa (que comeca em 1096).
          float yy=fimTitulo+30,teto=NV_SHELF_TOP-30,larg=780;
          const char *meta=diretor_meta(folder->title),*bio=diretor_bio(folder->title),*con=diretor_conhecido(folder->title);
          float fixo=(meta[0]?38:0)+(con[0]?38:0)+34;   // meta + conhecido + legenda
          int linhas=(int)((teto-yy-fixo-12)/31);if(linhas>3)linhas=3;
          if(meta[0]){txt_desenhar_alpha(txt_linha_corta(TXT_HERO_META,meta,201,206,218,255,larg),x,yy,a);yy+=38;}
          if(bio[0]&&linhas>0){yy+=txt_bloco(TXT_HERO_SIN,bio,222,225,232,x,yy,larg,31,a,linhas)+12;}
          if(con[0]){char l[300];snprintf(l,sizeof l,"Conhecido por  %s",con);
            txt_desenhar_alpha(txt_linha_corta(TXT_HERO_META,l,236,232,244,255,larg),x,yy,a);yy+=38;}
          yCap=yy;
        }
      }
      TxtLinha sub=txt_linha(TXT_HERO_SIN,caption,205,210,221,255);txt_desenhar_alpha(sub,x,yCap,a);
      return;
    }
  }

  // TROCA SO COM A ARTE NOVA JA DECODIFICADA.
  //
  // O pedido e feito aqui, no desenho, porque e aqui que se sabe qual arquivo a
  // arte e (o caminho sai do catalogo, com a pasta como reserva). Enquanto o
  // cache nao devolve textura, heroAtual nao muda e a tela segue com a arte que
  // ja estava — que e exatamente o que o dono pediu ao andar depressa.
  if (heroDesejado >= 0 && heroDesejado != heroAtual) {
    const char *arteD = arte_por_identidade(heroDesejado, 1);
    // Ausencia de arte tambem e um estado pronto: o placeholder pertence ao
    // item e pode entrar sem apagar o hero anterior primeiro.
    int artePronta = !arteD || tex_obter_hero(arteD);
    // ...OU A ESPERA ESTOUROU. Ver NV_HERO_ESPERA_MS em layout.h: passar do
    // prazo troca mesmo sem textura, e o heroi mostra o marcador do titulo
    // novo em vez da arte do anterior. O pedido acima ja enfileirou o decode,
    // entao a arte entra sozinha assim que chegar.
    if (!artePronta && SDL_GetTicks() - heroDesejadoEm >= NV_HERO_ESPERA_MS) {
      artePronta = 1;
      heroEstourou = 1;
    }
    if (artePronta) {
      // A ESPERA REAL, medida e nao suposta. E o intervalo entre o foco parar
      // (heroDesejadoEm) e o heroi corresponder — que e exatamente o que o
      // relator do #21 descreve como "don't match for a moment". Sem este
      // numero, pre-buscar os vizinhos seria adivinhacao, e pre-busca custa
      // textura de 1920 (~8 MB) que pode despejar os posteres da tela.
      unsigned esperou = (unsigned)(SDL_GetTicks() - heroDesejadoEm);
      // AMOSTRA ABSURDA NAO ENTRA NA CONTA, e a primeira quase sempre e uma.
      //
      // `heroDesejadoEm` e marcado quando o heroi PASSA A SER desejado, e no
      // arranque isso acontece antes de existir gente apertando tecla. Medindo
      // na LG saiu "espera 122713 ms" na amostra 1 — dois minutos de app
      // PARADO — e a media foi de 125 ms reais para 40989. Um numero desses nao
      // e espera de arte, e ele afoga justamente o dado que a medicao existe
      // para produzir. O teto de descarte e generoso de proposito: uma espera
      // real de 10 s ja seria um defeito gritante e continua sendo contada.
      if (esperou > 10000u) {
        printf("[hero] amostra de %u ms descartada (app parado, nao espera)\n",
               esperou);
        fflush(stdout);
      } else {
        heroEsperaN++;
        heroEsperaSoma += esperou;
        if (esperou > heroEsperaPior) heroEsperaPior = esperou;
        if (heroEstourou) heroEsperaEstouros++;
        printf("[hero] espera %u ms%s | n=%d media=%u pior=%u estouros=%d\n",
               esperou, heroEstourou ? " (ESTOUROU, sem arte)" : "",
               heroEsperaN, (unsigned)(heroEsperaSoma / (unsigned)heroEsperaN),
               heroEsperaPior, heroEsperaEstouros);
      }
      fflush(stdout);
      // O NUMERO ACIMA NAO MEDE A DEMORA, MEDE O NOSSO PRAZO.
      //
      // A espera e interrompida em NV_HERO_ESPERA_MS (400), entao ela NUNCA
      // passa muito de 400 — o relator do #21 mandou "media=412 pior=426
      // estouros=5" em n=6, que lido de fora parece "410 ms de demora" e na
      // verdade quer dizer "estourou todas as vezes, e nao sei por quanto".
      // Com esse numero nao da para saber se a arte chega aos 450 ms ou aos 4 s,
      // e as duas pedem conserto diferente.
      //
      // Por isso o item que estourou continua sendo cronometrado DEPOIS da
      // troca, ate a textura existir de verdade (ver heroTardeItem abaixo).
      if (heroEstourou) { heroTardeItem = heroDesejado; heroTardeEm = heroDesejadoEm; }
      heroEstourou = 0;
      heroAnterior = heroAtual;
      heroAtual = heroDesejado;
      heroDesejado = -1;
      heroSai = (motionReduzido || !arteD) ? 0.0f : 1.0f;
      heroEntra = (motionReduzido || !arteD) ? 1.0f : 0.0f;
      heroTrocaEm = SDL_GetTicks() + NV_HERO_INTERVALO_MS;
    }
  }

  const CatItem *ci = cat_item_exato(heroAtual);
  const char *arteA = arte_por_identidade(heroAtual, 1);
  const CatItem *cAnt = cat_item_exato(heroAnterior);
  const char *arteB = arte_por_identidade(heroAnterior, 1);

  // Teto de 1920: o hero ocupa a tela e a 960 saia esticado ao dobro.
  // O ANTERIOR so e pedido ENQUANTO a mistura acontece. Estava sendo pedido em
  // TODO quadro, mesmo com a troca ja terminada, quando ele nao e desenhado: se o
  // cache ja o tinha despejado, o pedido o trazia de volta — uma textura de
  // 1920 (~8 MB) re-decodificada para NAO ser desenhada, empurrando os posteres
  // visiveis para fora do orcamento.
  GLuint tAnt = (heroSai > 0.0f && arteB) ? tex_obter_hero(arteB) : 0;
  // Pedir a nova JA, durante o esvanecimento: e este pedido que enfileira o
  // decode, e e por isso que o vazio dura o tempo do carregamento e nao mais.
  GLuint tAtu = arteA ? tex_obter_hero(arteA) : 0;
  // A ARTE ATRASADA CHEGOU — e so agora da para dizer quanto ela demorou.
  if (heroTardeItem >= 0) {
    if (heroTardeItem != heroAtual) {
      // O foco andou de novo antes de a arte chegar. Cronometrar ate aqui
      // mediria a paciencia de quem esta com o controle, nao o decode.
      heroTardeItem = -1;
    } else if (tAtu) {
      printf("[hero] arte atrasada chegou em %u ms (prazo e %d)\n",
             (unsigned)(SDL_GetTicks() - heroTardeEm), NV_HERO_ESPERA_MS);
      fflush(stdout);
      heroTardeItem = -1;
    }
  }
  if (tAnt) {
    // Esvanecimento com aceleracao e desaceleracao: o medido fica ~25% do
    // percurso quase parado no comeco, entao rampa reta le como corte na saida.
    (void)desenhaArteHero(r, modoHero, cAnt, arteB,
                          anim_suave(heroSai) * aArte);
  } else if (heroSai > 0.0f) {
    desenhaPlaceholderHero(r, cAnt, anim_suave(heroSai) * aArte, 0);
  }
  if (tAtu && heroEntra > 0.0f) {
    (void)desenhaArteHero(r, modoHero, ci, arteA,
                          anim_suave(heroEntra) * aArte);
  } else if (!tAtu) {
    // ESPERANDO quando ha caminho de arte e ela ainda nao decodificou; ausente
    // quando o titulo nao tem arte nenhuma para pedir.
    desenhaPlaceholderHero(r, ci,
                           aArte * (heroEntra > 0.0f ? 1.0f : heroEntra),
                           arteA != NULL && arteA[0] != 0);
  }
  gfx_tex_aspect_atual = 0.0f;
  heroArteRect = r;

  float aTexto = 1.0f - saida;
  float descidaCopy = saida * NV_TELA_H * 0.06f;
  if (aTexto <= 0.004f) return;

  // BLOCO DE TEXTO DO HERO — transcrito do CSS do app web, nao deduzido de
  // captura. `.home-modern-hero-copy` e um flex column com justify-content
  // flex-end e gap 16, ancorado numa base fixa; os filhos, na ordem:
  //   .home-hero-brand         caixa do logo, 440x200, arte no topo-esquerda
  //   .home-modern-hero-meta-line   21/500 #b3b3b3, tokens separados por •
  //   .home-modern-hero-secondary   18/600 branco 88%, com selos e o IMDb
  //   .home-hero-description        22/400 branco, largura 560, leading 30
  // Cada bloco vazio some (`.is-empty { display: none }`), e e por isso que a
  // altura do conjunto muda de titulo para titulo — nao por posicao absoluta.
  //
  // O conteudo de cada linha vem de buildModernHeroPresentation
  // (homeScreen.js:2497), que separa o caso "continuar assistindo" do resto.
  int contHero = (ci && ci->progresso > 0 && ci->restanteMin > 0);

  // Linha de meta. No web sao tokens juntados por "•"; ci->genero ja chega
  // como "Filme · Terror", que e o par (tipo, primeiro genero) do web.
  char metaLinha[288];
  metaLinha[0] = 0;
  if (contHero && ci->temporada > 0) {
    char cab[64];
    snprintf(cab, sizeof cab, "S%d E%d", ci->temporada, ci->episodio);
    snprintf(metaLinha, sizeof metaLinha, "%s%s%s", cab,
             (ci->genero[0] ? "  \xc2\xb7  " : ""), ci->genero);
  } else if (ci && ci->genero[0]) {
    snprintf(metaLinha, sizeof metaLinha, "%s", ci->genero);
  }
  if (ci && ci->meta[0]) {
    size_t n = strlen(metaLinha);
    snprintf(metaLinha + n, sizeof metaLinha - n, "%s%s",
             n ? "   \xe2\x80\xa2   " : "", ci->meta);
  }

  // Linha secundaria: destaque de progresso, selos e a nota do IMDb. O web so
  // mostra o IMDb aqui quando ja existe destaque ou selo (showImdbSecondary);
  // no outro caso ele vai para o fim da linha de meta.
  char destaque[64];
  destaque[0] = 0;
  if (contHero) snprintf(destaque, sizeof destaque, i18n("%d MINUTOS RESTANTES"),
                         ci->restanteMin);
  const char *selo = (ci && ci->classificacao[0] && !contHero) ? ci->classificacao : NULL;
  char nota[8];
  nota[0] = 0;
  if (ci && ci->nota > 0) snprintf(nota, sizeof nota, "%.1f", ci->nota / 10.0f);
  int temSec = (destaque[0] || selo || nota[0]);

  const char *sinopse = (ci && ci->sinopse[0]) ? ci->sinopse : "";

  // --- empilhamento de baixo para cima, como o flex-end do CSS ---
  float base = NV_SHELF_TOP - NV_HERO_COPY_GAP + descidaCopy;
  float hSin = sinopse[0] ? txt_bloco(TXT_HERO_SIN, sinopse, 255, 255, 255, -1, 0,
                                      NV_HERO_SIN_W, NV_LD_HERO_SIN, 0.0f, 3)
                          : 0.0f;
  float ySin  = base - hSin;
  float ySec  = temSec ? (ySin - (sinopse[0] ? NV_HERO_COPY_LINHA : 0.0f)
                          - NV_LD_HERO_SEC) : ySin;
  float yMeta = ySec - ((temSec || sinopse[0]) ? NV_HERO_COPY_LINHA : 0.0f)
                - (metaLinha[0] ? NV_LD_HERO_META : 0.0f);
  float logoY = yMeta - NV_HERO_COPY_LINHA - NV_LOGO_HERO_H;
  float x = ajustes_conteudo_x();

  // Logo do titulo, ou o nome em texto quando nao ha logo
  // (.home-hero-title-text, 56/600 no modern — nao os 76 do TXT_TITULO1).
  GLuint tlogo = (ci && ci->logo[0]) ? tex_obter(ci->logo) : 0;
  if (tlogo) {
    float ap = tex_aspecto(ci->logo);
    if (ap <= 0.0f) ap = 4.0f;
    float hTit = NV_LOGO_HERO_H, wTit = hTit * ap;
    float maxW = cheio ? NV_LOGO_HERO_CHEIO_MAX_W : NV_LOGO_HERO_MAX_W;
    if (wTit > maxW) { wTit = maxW; hTit = wTit / ap; }
    // object-position: left top — a arte encosta no TOPO da caixa.
    GfxRect rl = { x, logoY, wTit, hTit };
    gfx_tex_aspect_atual = 0.0f;
    // Logo escuro vira branco. Mesma regra da tela de detalhe: o TMDB nao marca
    // claro/escuro, entao a decisao sai da luminancia MEDIDA (tex_luminancia).
    // Logo claro ou colorido passa intacto; -1 (ainda carregando) nao tinge.
    { GfxModo m = tex_marca_escura(ci->logo) ? GFX_MARCA : GFX_TEXTO;
      // O LOGO DO TITULO acompanha a ARTE, nao o texto. MEDIDO: 205 ms depois
      // da tecla a arte antiga ainda estava a 85% e o logo JA tinha sumido por
      // inteiro; ele so reaparece no mesmo quadro em que a arte nova entra.
      gfx_rect(rl, tlogo, m, 0, 0, 0, 0.0f, 1, 1, 1, aTexto * heroEntra); }
  } else {
    // .legacy-webos .home-hero-title-text: 76px (components.css:19164), nao os
    // 56 do tema padrao.
    // Sem titulo NAO se inventa titulo. Aqui havia uma lista de demonstracao
    // ("Ruptura", "Silo", "Shrinking"...) que preenchia o hero com o nome de
    // outra serie quando o item ainda nao tinha nome — indistinguivel de dado
    // real para quem olha a tela. Mesma familia do elenco e da classificacao
    // que ja sairam do detalhe. Sem nome, o hero fica so com a arte, que ja
    // basta, e o texto aparece quando o dado chegar.
    if (ci && ci->titulo[0]) {
      TxtLinha tit = txt_linha(TXT_TITULO1, ci->titulo, 255, 255, 255, 255);
      txt_desenhar_alpha(tit, x, logoY + NV_LOGO_HERO_H - (float)tit.h,
                         aTexto);
    }
  }

  if (metaLinha[0]) {
    float badgeW=ci?badges_desenhar(badges_provedor(ci->provNome),x,yMeta,150,24,aTexto):0;
    TxtLinha lm = txt_linha_corta(TXT_HERO_META, metaLinha, 179, 179, 179, 255,
                                  NV_HERO_SIN_W-badgeW);
    // META E SINOPSE TROCAM NA HORA, sem esvanecer com a arte. MEDIDO: no
    // quadro a 205 ms, com a arte antiga ainda a 85%, a linha de meta e a
    // sinopse ja eram as do titulo NOVO, com o texto opaco. Multiplicar por um
    // alfa de troca aqui era invencao nossa — e, com o rasterizador fazendo 2
    // linhas por quadro (text.c:40), esvanecer texto que ainda esta assentando
    // e o pior caso possivel.
    txt_desenhar_alpha(lm, x+badgeW, yMeta, aTexto);
  }

  if (temSec) {
    float cx = x;
    float a = aTexto;
    if (destaque[0]) {
      // .home-modern-hero-highlight: branco cheio, peso 600, tracking 0.04em.
      cx += txt_tracking(TXT_HERO_SEC, destaque, 255, 255, 255, cx, ySec, a,
                         NV_FT_HERO_SEC * 0.04f);
      cx += 14.0f;
    }
    if (selo) {
      // Metadata, not a focus target: compact neutral surface and soft corners.
      TxtLinha lb = txt_linha(TXT_CAPTION, selo, 235, 235, 240, 255);
      float bw = lb.w + 22.0f, bh = 32.0f;
      float by = ySec + (NV_LD_HERO_SEC - bh) * 0.5f;
      gfx_cor((GfxRect){ cx, by, bw, bh }, 0.22f,
              0.13f, 0.14f, 0.16f, 0.94f * a);
      txt_desenhar_alpha(lb, cx + 11.0f, by + (bh - lb.h) * 0.5f, a);
      cx += bw + 14.0f;
    }
    if (nota[0]) {
      // .home-hero-imdb: o selo amarelo de 40px e a nota logo depois, com 10
      // de respiro. O SVG do IMDb nao esta empacotado aqui; o retangulo
      // amarelo com as letras pretas e a mesma leitura a esta escala.
      TxtLinha ls = txt_linha(TXT_MINI, "IMDb", 8, 8, 8, 255);
      float sw = 40.0f, sh = ls.h + 6.0f;
      gfx_cor((GfxRect){ cx, ySec + (NV_LD_HERO_SEC - sh) * 0.5f, sw, sh },
              0.12f, 0.96f, 0.78f, 0.06f, a);
      txt_desenhar_alpha(ls, cx + (sw - ls.w) * 0.5f,
                         ySec + (NV_LD_HERO_SEC - sh) * 0.5f + 3.0f, a);
      cx += sw + 10.0f;
      TxtLinha ln = txt_linha(TXT_HERO_SEC, nota, 179, 179, 179, 255);
      txt_desenhar_alpha(ln, cx, ySec, a);
    }
  }

  if (sinopse[0])
    txt_bloco(TXT_HERO_SIN, sinopse, 255, 255, 255, x, ySin, NV_HERO_SIN_W,
              NV_LD_HERO_SIN, aTexto, 3);
}

// Fundo CINZA, e so. Eu tinha posto aqui a arte do titulo em destaque
// desfocada, achando que era isso o "cinza do Apple TV" — mas o efeito era o
// oposto do pedido: a arte do hero subia e saia normalmente, e a copia
// desfocada dela continuava no fundo, dando a impressao de que a imagem nunca
// tinha subido. Fundo neutro nao compete com nada.
static void desenhaFundo(void) {
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  // A tela ja foi limpa com ESTA MESMA COR por glClearColor/glClear em
  // main.c antes de app_desenhar. Pintar por cima era uma camada de tela
  // cheia jogada fora por quadro — e o custo dominante nesta GPU e fill
  // rate (gfx.c registra que DUAS camadas de tela cheia derrubavam a
  // Mali-G71 para ~40fps). Nao repor sem antes mudar a cor do clear.
  (void)tela;
}

static void desenhaAtalhos(int r, float y) {
  float w = larguraFil(r), h = alturaFil(r);
  static int ultimo=-1;static Uint32 desde;
  for (int c = 0; c < fileiras[r].n; c++) {
    float x = ajustes_conteudo_x() + c * passoFil(r) - scrollX[r];
    if (x + w < 0 || x > NV_TELA_W) continue;
    float f = animFoco[r][c], raio = raioDe(w, h);
    GfxRect card = {x, y, w, h};
    if (f > .01f) {
      float menor = w < h ? w : h;
      gfx_cor((GfxRect){x - NV_ANEL_FOCO, y - NV_ANEL_FOCO,
        w + 2*NV_ANEL_FOCO, h + 2*NV_ANEL_FOCO},
        (raio * menor + NV_ANEL_FOCO) / (menor + 2*NV_ANEL_FOCO), .96f, .97f, .98f, f);
    }
    gfx_cor(card, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, 1);
    const ColFolder *folder=col_folder(fileiras[r].folders[c]);if(!folder)continue;
    const char *arte = folder->cover;
    GLuint tex = arte && arte[0] ? tex_obter_larg(arte, w) : 0;
    if(foco.fileira==r&&foco.coluna==c&&folder->frames>0 &&
       !ajustes_animacoes_reduzidas()) {
      int id=fileiras[r].folders[c];Uint32 now=SDL_GetTicks();
      if(ultimo!=id){ultimo=id;desde=now;}
      if(now-desde>350) {
        char frame[700];int index=(int)((now-desde-350)/67)%folder->frames+1;
        snprintf(frame,sizeof frame,"%s/%03d.jpg",folder->frameDir,index);
        GLuint motion=tex_obter_larg(frame,480);
        if(motion)tex=motion;
        snprintf(frame,sizeof frame,"%s/%03d.jpg",folder->frameDir,index%folder->frames+1);
        tex_obter_larg(frame,480);
        snprintf(frame,sizeof frame,"%s/%03d.jpg",folder->frameDir,(index+1)%folder->frames+1);
        tex_obter_larg(frame,480);
      }
    }
    if (tex) {
      gfx_tex_aspect_atual = tex_aspecto(arte);
      gfx_rect(card, tex, GFX_CARD, 0, 0, 0, raio, 0, 0, 0, 1);
      gfx_tex_aspect_atual = 0;
    }
    // A propria capa e a identidade do catalogo. O nome/logo vinha sendo
    // desenhado novamente por cima dela e criava exatamente a duplicacao que
    // o usuario apontou em Netflix, Prime Video, Disney+ e nas listas IMDb.
    // Titulo de fileira continua no cabecalho; dentro do card fica somente a
    // arte, sem veu, badge ou logo auxiliar.
  }
}

void home_desenhar(Uint32 agora) {
  desenhaFundo();
  float pd = detail_progresso();
  if (ajustes_hero_ligado()) desenhaHero(agora, pd);

  // ABERTURA DO DETALHE: as fileiras DESCEM e apagam; a arte de fundo fica.
  //
  // E o movimento que o dono descreveu — "so os posters descem e mantem o
  // background". O detalhe ja nao voa mais a partir do card: a arte dele entra
  // em tela cheia ganhando opacidade, entao o que o olho segue e a saida das
  // fileiras. Descer 8% da altura da tela e o bastante para ler como saida sem
  // que a ultima fileira suma antes da hora.
  //
  // O `pd` vem da MESMA mola que o detalhe usa para desenhar (detail_progresso),
  // e nao de um relogio proprio: dois relogios descasariam e a home sairia
  // adiantada ou atrasada em relacao a arte que entra.
  float descida = pd * NV_TELA_H * 0.08f;
  if (pd >= 0.996f) return;   // detalhe assentado: nada da home aparece

  // VIEWPORT DAS FILEIRAS. `.home-modern-rows-viewport` (components.css:6929) e
  // um bloco absoluto com bottom:0, height 52% e overflow-y:auto — ou seja as
  // fileiras rolam DENTRO dos 52% de baixo e o que sobe alem disso e CLIPADO.
  // O port desenhava as fileiras soltas sobre a tela inteira, e por isso a
  // fileira que saia por cima aparecia atravessada no bloco do hero em vez de
  // sumir. O hero nao rola: so o conteudo dele muda com o foco.
  gfx_recorte(0, NV_SHELF_TOP-96, NV_TELA_W, NV_TELA_H - NV_SHELF_TOP+96);
  float y = NV_SHELF_TOP - scrollY + descida;
  // NENHUMA FILEIRA. Nao e o arranque (ali a home mostra o catalogo do pacote
  // ou o do cache): e o caso de a pessoa ter desligado todas em Ajustes. Sem
  // texto, o hero sozinho com o resto da tela vazia le como travamento — e ela
  // nao teria como adivinhar que foi ela quem apagou a home.
  if (nFileiras < 1) {
    float tx = ajustes_conteudo_x();
    TxtLinha t = txt_linha(TXT_ROW_TITULO, "Nenhuma fileira ativa", 240, 241, 245, 255);
    txt_desenhar(t, tx, NV_SHELF_TOP);
    txt_bloco(TXT_CAPTION,
              "Ative fileiras em Ajustes, na categoria Fileiras da Home.",
              183, 186, 194, tx, NV_SHELF_TOP + t.h + 14.0f,
              NV_TELA_W - tx - NV_HOME_SAFE_RIGHT, 34, 1, 2);
  }
  for (int r = 0; r < nFileiras; r++) {
    TipoFileira tipo = fileiras[r].tipo;
    float fade=anim_clamp((y-(NV_SHELF_TOP-80))/80,0,1);
    gfx_opacidade_grupo=fade*fade*(3-2*fade);
    float lw = larguraFil(r);
    float lh = alturaFil(r), passo = passoFil(r);
    float artH = lh;
    // `y` é o topo do cabeçalho da fileira; os cards começam depois do título.
    // Separar os dois evita que o título da fileira seguinte seja desenhado
    // sobre a arte da anterior quando a fileira tem cards altos.
    float cardY = y + NV_LEGACY_ROW_HEAD_H;

    int deitado = editorial(tipo) || ((tipo != FILEIRA_CONTINUE) && ajustes_posteres_deitados());
    int rotuloFora = temRotulo(tipo);
    if (y < NV_TELA_H + 200 && y + NV_LEGACY_ROW_HEAD_H + lh > -200) {
      // `catalogTypeSuffixEnabled`. formatCatalogRowTitle (homeUtils.js:62) faz
      // `if (!showTypeSuffix) return base;` — devolve o nome capitalizado e
      // pronto. Aqui o sufixo e tirado no DESENHO e nao na descoberta, senao a
      // preferencia so valeria depois que a rede trouxesse os catalogos de
      // novo — ou seja, so no proximo arranque.
      const char *rotFil = fileiras[r].titulo;
      char semSufixo[96];
      if (!ajustes_sufixo_tipo() && rotFil) {
        const char *corte = strstr(rotFil, " - ");
        const char *ultimo = NULL;
        while (corte) { ultimo = corte; corte = strstr(corte + 3, " - "); }
        if (ultimo && (!strcmp(ultimo + 3, "Filme") || !strcmp(ultimo + 3, "S\xc3\xa9rie")
                       || !strcmp(ultimo + 3, i18n("Filme")) || !strcmp(ultimo + 3, i18n("S\xc3\xa9rie")))) {
          size_t n = (size_t)(ultimo - rotFil);
          if (n >= sizeof semSufixo) n = sizeof semSufixo - 1;
          memcpy(semSufixo, rotFil, n);
          semSufixo[n] = 0;
          rotFil = semSufixo;
        }
      }
      TxtLinha tl = txt_linha_corta(TXT_ROW_TITULO, rotFil, 245, 246, 249, 255,
                                    NV_TELA_W - ajustes_conteudo_x() - 180);
      txt_desenhar(tl, ajustes_conteudo_x(), y);
      if(tipo==FILEIRA_SOCIAL) {
        const char *marca=extras_caminho_marca_nome("trakt_wordmark");
        GLuint logo=tex_obter(marca);float ap=logo?tex_aspecto(marca):2.66f;
        if(ap<=0)ap=2.66f;
        if(logo)gfx_rect((GfxRect){ajustes_conteudo_x()+tl.w+18,y+(tl.h-30)*.5f,30*ap,30},
                         logo,GFX_MARCA,0,0,0,0,.95f,.93f,.94f,1);
      }
      if(!strncmp(fileiras[r].catId,"ai_",3)) {
        TxtLinha ai=txt_linha(TXT_HERO_META,"AI-powered",183,192,219,255);
        txt_desenhar(ai,ajustes_conteudo_x()+tl.w+22,y+(tl.h-ai.h)*.5f);
      }
      if (foco.fileira == r) {
        char pos[32];
        if (foco.coluna < fileiras[r].n)
          snprintf(pos, sizeof pos, "%d / %d", foco.coluna + 1, fileiras[r].n);
        else snprintf(pos, sizeof pos, "Ver tudo");
        TxtLinha lp = txt_linha(TXT_HERO_META, pos, 186, 191, 202, 255);
        txt_desenhar(lp, NV_TELA_W - NV_HOME_SAFE_RIGHT - lp.w, y + (tl.h - lp.h)*.5f);
      }
      if (tipo == FILEIRA_CATALOGOS) {
        desenhaAtalhos(r, cardY);
        y += NV_LEGACY_ROW_HEAD_H + alturaTotalFil(r) + fileiraGap();
        continue;
      }

      // CARD "VER TUDO" no fim da fileira. Desenhado antes do laco dos cartazes
      // para nao herdar as variaveis dele; ele nao e um titulo e nao usa arte.
      //
      // MEDIDO no web (.home-seeall-card-inner): moldura de 2 px em
      // rgba(255,255,255,0.12) sobre rgba(255,255,255,0.06), seta e rotulo
      // empilhados e centrados. Focado, a moldura acende.
      if (fileiras[r].verTudo) {
        int c = fileiras[r].n;
        float f = animFoco[r][c];
        float esc = 1.0f + escalaDe(tipo) * f;
        float w = lw * esc, h = artH * esc;
        float cx = ajustes_conteudo_x() + c * passo - scrollX[r] + lw * 0.5f;
        float cy = cardY + artH * 0.5f;
        if (cx > -lw * 1.5f && cx < NV_TELA_W + lw) {
          float px = cx - w * 0.5f, py = cy - h * 0.5f;
          float raio = raioDe(w, h);
          GfxRect r0 = { px, py, w, h };
          float lum = 0.06f + 0.10f * f;
          gfx_cor(r0, raio, 1, 1, 1, lum);
          gfx_rect(r0, 0, GFX_ANEL, 0, 2.0f / h, 0, raio,
                   1, 1, 1, (0.12f + 0.70f * f));
          { TxtLinha ls = txt_linha(TXT_TITULO2, "\xe2\x86\x92",
                                    236, 237, 242, 255);
            TxtLinha lr = txt_linha(TXT_ROW_TITULO, "Ver tudo",
                                    f > 0.5f ? 255 : 190, f > 0.5f ? 255 : 194,
                                    f > 0.5f ? 255 : 203, 255);
            float bloco = ls.h + 14.0f + lr.h;
            float by = py + (h - bloco) * 0.5f;
            txt_desenhar_alpha(ls, px + (w - ls.w) * 0.5f, by, 0.95f);
            txt_desenhar_alpha(lr, px + (w - lr.w) * 0.5f,
                               by + ls.h + 14.0f, 1.0f); }
        }
      }

      for (int passe = 1; passe < 2; passe++) {
        for (int c = 0; c < fileiras[r].n; c++) {
          float f = animFoco[r][c];
          if (passe == 0 && f < 0.01f) continue;
          float esc = 1.0f + escalaDe(tipo) * f;
          float w = lw * esc, h = artH * esc;
          // EXPANSAO EM REPOUSO. `abre` so e diferente de zero no card focado
          // desta fileira; os DEPOIS dele sao empurrados pela mesma medida.
          //
          // A altura nao entra na conta: na referencia ela nao muda, e o card
          // cresce so para a direita a partir de uma borda esquerda parada.
          float abre = (r == expFileira && c == expColuna) ? expAbre : 0.0f;
          float larguraAberta = artH * esc * NV_EXP_ASPECTO;
          float empurra = 0.0f;
          if (r == expFileira && expAbre > 0.0f && c > expColuna)
            empurra = (artH * NV_EXP_ASPECTO - lw) * expAbre;
          if (abre > 0.0f) w = lw * esc + (larguraAberta - lw * esc) * abre;
          float cx = ajustes_conteudo_x() + c * passo - scrollX[r] + lw * 0.5f
                   + empurra + (w - lw * esc) * 0.5f;
          // Sem levantamento: no web o card focado nao sai do lugar.
          float cy = cardY + artH * 0.5f;
          if (cx < -lw * 1.5f || cx > NV_TELA_W + lw) continue;
          float px = cx - w * 0.5f, py = cy - h * 0.5f;

          if (passe == 0) {
            // Sem sombra. Ela existia para separar o card do fundo, mas sobre
            // arte colorida vira um halo escuro em volta do item focado — e o
            // aparelho nao tem isso: la o foco se marca por escala e brilho.
            (void)f;
            continue;
          }

          const int idxCat = fileiras[r].ini + c;
          if(tipo==FILEIRA_TOP10 && fileiras[r].stackN) {
            // Sem placa de fundo: os cartazes empilhados ja formam o card.
            int count=fileiras[r].stackN<6?fileiras[r].stackN:6;
            for(int k=0;k<count;k++) {
              const CatItem *it=cat_item_exato(idxCat+k);if(!it)continue;
              GfxRect pr={px+20+k*78,py+18,178,h-72};
              const char *pa=arte_por_formato(it,0);
              GLuint tx=pa?tex_obter_larg(pa,178):0;
              if(tx){gfx_tex_aspect_atual=tex_aspecto(pa);gfx_rect(pr,tx,GFX_CARD,0,0,0,.055f,1,1,1,1);gfx_tex_aspect_atual=0;}
              else desenhaArteAusente(pr,.055f,it,1);
            }
            txt_desenhar(txt_linha(TXT_CAPTION,"TOP 100   ·   Explorar primeiros 10",242,235,248,255),px+24,py+h-42);
            if(foco.fileira==r)temItemFoco=0;
            continue;
          }
          if(tipo==FILEIRA_SOCIAL && fileiras[r].ini<0) {
            GfxRect b={px,py,w,h};
            gfx_cor(b,.055f,.115f,.09f,.15f,1);
            if(f>.01f)gfx_rect(b,0,GFX_ANEL,0,.008f,0,.055f,.95f,.93f,.99f,f);
            txt_desenhar(txt_linha_corta(TXT_CALLOUT,"Entre amigos",240,234,248,255,w-48),px+24,py+24);
            txt_desenhar(txt_linha_corta(TXT_CAPTION,"Nenhuma atividade disponível agora.",195,183,211,255,w-48),px+24,py+91);
            txt_desenhar(txt_linha_corta(TXT_CAPTION,"Siga pessoas no Trakt para descobrir mais.",195,183,211,255,w-48),px+24,py+126);
            txt_desenhar(txt_linha_corta(TXT_CAPTION,"OK · Conferir conexão",240,231,250,255,w-48),px+24,py+h-50);
            if(foco.fileira==r)temItemFoco=0;
            continue;
          }
          const CatItem *cItem = cat_item_exato(idxCat);
          if(tipo==FILEIRA_SOCIAL && cItem) {
            if(foco.fileira==r)temItemFoco=0;
            // A atividade social precisa de contexto, nao de um segundo hero.
            // Sem painel e sem contorno: o palco neutro da home faz o trabalho
            // de fundo. A imagem tem um papel editorial menor, thumbnail da
            // obra, enquanto autoria e acao respiram diretamente na tela.
            const float conteudoTopo=py+24.0f;
            const float conteudoBase=py+h-24.0f;
            const float arteW=134.0f;
            const float arteX=px+w-24.0f-arteW;
            const char *thumbPath=cItem->poster[0]?cItem->poster:
                                  (cItem->backdrop[0]?cItem->backdrop:NULL);
            if(thumbPath){GLuint thumb=tex_obter_larg(thumbPath,arteW);
              if(thumb){GfxRect tr={arteX,conteudoTopo,arteW,conteudoBase-conteudoTopo};
                gfx_tex_aspect_atual=tex_aspecto(thumbPath);
                gfx_rect(tr,thumb,GFX_CARD,0,0,0,.055f,0,0,0,1);gfx_tex_aspect_atual=0;
              }
            }
            // Avatar maior e centralizado na mesma faixa vertical da arte.
            // O eixo comum deixa a composicao com cara de ficha editorial,
            // em vez de avatar solto no topo e thumbnail separado embaixo.
            float d=120.0f, ax=px+24.0f, ay=py+(h-d)*.5f;
            GfxRect avatar={ax,ay,d,d};
            GLuint foto=cItem->socialAvatar[0]?tex_obter_larg(cItem->socialAvatar,220):0;
            // O foco e um disco atras da imagem, nunca um stroke por cima.
            // Assim as duas circunferencias compartilham o mesmo centro e o
            // aro permanece uniforme inclusive no limite superior da fileira.
            float pad=5.0f*f;
            if(f>.01f)gfx_rect(avatar,0,GFX_DISCO,0,0,0,0,.96f,.96f,.98f,f);
            GfxRect miolo={ax+pad,ay+pad,d-pad*2,d-pad*2};
            gfx_rect(miolo,0,GFX_DISCO,0,0,0,0,.15f,.16f,.18f,1);
            if(foto){gfx_tex_aspect_atual=tex_aspecto(cItem->socialAvatar);
              gfx_rect(miolo,foto,GFX_AVATAR,0,0,0,0,1,1,1,1);gfx_tex_aspect_atual=0;}
            else {char inicial[8]="?";const char *nome=cItem->socialNome[0]?cItem->socialNome:cItem->pais;
              if(nome[0]){size_t z=1;while(z<4 && (nome[z]&0xc0)==0x80)z++;memcpy(inicial,nome,z);inicial[z]=0;}
              TxtLinha l=txt_linha(TXT_TITULO2,inicial,235,236,240,255);txt_desenhar(l,ax+(d-l.w)*.5f,ay+(d-l.h)*.5f);}
            float tx=ax+d+24.0f,tw=thumbPath?arteX-tx-24.0f:w-192.0f;
            TxtLinha nome=txt_linha_corta(TXT_CW_TITULO,cItem->socialNome[0]?cItem->socialNome:cItem->pais,245,245,247,255,tw);
            txt_desenhar(nome,tx,conteudoTopo);
            TxtLinha acao=txt_linha_corta(TXT_MINI,cItem->socialAcao[0]?cItem->socialAcao:cItem->provNome,181,185,196,255,tw);
            txt_desenhar(acao,tx,conteudoTopo+38.0f);
            TxtLinha titulo=txt_linha_corta(TXT_CW_META,cItem->titulo,228,231,239,255,tw);
            txt_desenhar(titulo,tx,conteudoTopo+92.0f);
            TxtLinha ep=txt_linha_corta(TXT_MINI,cItem->temporada?cItem->direcao:"Filme",181,185,196,255,tw);
            txt_desenhar(ep,tx,conteudoTopo+130.0f);
            TxtLinha fonte=txt_linha_corta(TXT_MINI,cItem->provNome[0]?cItem->provNome:"Trakt",155,161,174,255,tw);
            txt_desenhar(fonte,tx,conteudoBase-14.0f);
            if(f>.1f){TxtLinha ver=txt_linha(TXT_MINI,"Ver perfil",235,237,244,255);txt_desenhar_alpha(ver,tx,conteudoBase-40.0f,f);}
            continue;
          }
          const char *caminho = NULL;
          // Card DEITADO pede arte deitada. No web o poster do card landscape sai
          // de `landscapePoster` -> `background` -> `backdrop` -> `poster`
          // (homeScreen.js:3155), nao do poster 2:3 — usar o retrato aqui faria o
          // shader recortar a cabeca de todo mundo para caber em 16:9.
          // Aberto, o card mostra a arte DEITADA: e para isso que ele abre.
          // A troca acontece na metade do caminho, quando a moldura ja tem
          // largura de 16:9 e o retrato comecaria a ser recortado feio.
          caminho = arte_por_identidade(idxCat, abre > 0.5f ||
                                        tipo == FILEIRA_CONTINUE ||
                                        tipo == FILEIRA_RETORNO || deitado);

          if (focus_indice(&foco, r, c)) {
            GfxRect aqui = { px, py, w, h };
            itemFoco.indice = idxCat;
            itemFoco.rect   = aqui;
            itemFoco.arte   = caminho;
            itemFoco.titulo = cItem ? cItem->titulo : NULL;
            itemFoco.genero = cItem ? cItem->genero : NULL;
            itemFoco.meta   = cItem ? cItem->meta : NULL;
            temItemFoco = 1;
          }
          // Pede pela largura REAL do card: e esta fileira que multiplica.
          // Com o teto unico de 640 cada poster custava 2,4 MB e o cache
          // estourava com ~40 texturas, despejando o que ainda estava na tela.
          GLuint t = caminho ? tex_obter_larg(caminho, w) : 0;
          // ANEL DE FOCO: 4 px de #FFFFFF, POR FORA da arte.
          //
          // Era 2 px de #f5f5f5, tirado do `box-shadow` do app WEB. MEDIDO no
          // aparelho de referencia (TCL, mesmo card, mesma fileira): 4 px
          // solidos de #FFFFFF, x 102->105 sem rampa. O nosso media 2 px com
          // antialias (#A1A1A2 -> #C6C6C7 -> #E3E3E4 -> #F3F3F3) e a rampa ja
          // entrava na arte.
          //
          // A 3 m de distancia, 2 px cinza-suave contra 4 px branco solido e a
          // diferenca entre ver onde se esta e procurar o foco na tela. O mesmo
          // valor aparece em card de episodio e botao de detalhe na referencia:
          // e UM numero para o app inteiro (NV_DETW_ANEL ja valia 4 e so era
          // usado no detalhe).
          float raio = raioDe(w, h);
          if (f > 0.01f) {
            GfxRect borda = { px - NV_ANEL_FOCO, py - NV_ANEL_FOCO,
                              w + NV_ANEL_FOCO * 2, h + NV_ANEL_FOCO * 2 };
            gfx_cor(borda, raio, 1.0f, 1.0f, 1.0f, f);
          }
          GfxRect card = { px, py, w, h };
          // CARD SEM ARTE: superficie solida, nao o vazio. Sem isto o card
          // ficava da cor do fundo — MEDIDO: #242429 sobre #252629, diferenca
          // de (1,2,0), contraste 1,0:1. Era literalmente invisivel, e foi a
          // origem da queixa "nao aparecem todos os posteres": eles apareciam,
          // do tom exato do fundo. A referencia desenha #2C2C2C na caixa exata.
          if (t) {
            // SEM PARALAXE OSCILANTE no card focado.
            //
            // Havia aqui um sen/cos do relogio deslocando a arte do card em
            // foco para sempre — um "respirar" estilo tvOS. A referencia NAO
            // tem isso, e a prova e direta: o screenrecord do aparelho so
            // escreve quadro quando algo muda na tela, e depois de a navegacao
            // assentar ele ficou 1,8 s e 2,4 s SEM EMITIR UM UNICO QUADRO, em
            // duas gravacoes diferentes. Tela parada de verdade, nao "quase".
            //
            // Alem de nao existir la, era o pior tipo de animacao para esta
            // GPU: obrigava a redesenhar a fileira inteira em todo quadro para
            // sempre, e o custo dominante aqui e fill rate.
            gfx_tex_aspect_atual = tex_aspecto(caminho);
            gfx_rect(card, t, GFX_CARD, f, 0.0f, 0.0f,
                     raio, 0, 0, 0, 1);
            gfx_tex_aspect_atual = 0.0f;
          } else {
            // CARD SEM ARTE: superficie SOLIDA e visivel, nao o vazio.
            //
            // Aqui era #242429 (0.14,0.14,0.16) — MEDIDO contra o fundo que
            // havia entao, #252629: diferenca de (1,2,0), contraste 1,0:1. O
            // card existia e era literalmente invisivel, e essa foi a origem da
            // queixa "nao aparecem todos os posteres". Eles apareciam, do tom
            // exato do fundo, e o unico sinal era o anel de foco em volta de um
            // retangulo vazio — que le como QUEBRADO, nao como carregando.
            //
            // A referencia usa #2C2C2C sobre #0D0D0D: luminancia ~22x a do
            // fundo, impossivel nao ver.
            desenhaArteAusente(card, raio, cItem, 1.0f);
          }
          // SELO DE ASSISTIDO: disco branco com um "v" escuro, no canto
          // superior direito do poster. A referencia o tem e nos nao tinhamos
          // indicador nenhum na home — sem ele nao da para varrer uma fileira e
          // ver o que ja foi visto, que e o principal uso da tela.
          //
          // >= 90% e "visto", nao 100%: quase ninguem assiste os creditos, e o
          // proprio player ja arredonda para o fim quando falta menos de um
          // minuto (player_encerrar). Marcar so em 100% deixaria de fora
          // justamente o que acabou de ser assistido.
          if (cItem && cItem->progresso >= 90 && tipo != FILEIRA_CONTINUE) {
            float d = w * 0.16f;                 // proporcional ao card
            float mx = px + w - d - 10.0f, my = py + 10.0f;
            GfxRect disco = { mx, my, d, d };
            gfx_cor(disco, 0.5f, 1, 1, 1, 0.94f);
            // O "v" desenhado com dois tracos: o glifo da fonte nao serve aqui
            // porque precisaria de uma linha de texto so para isto, e o cache
            // de linhas tem 256 entradas disputadas pelos titulos.
            { float cx = mx + d * 0.5f, cy = my + d * 0.5f;
              float e = d * 0.085f;              // espessura
              GfxRect a1 = { cx - d * 0.20f, cy - e * 0.5f, d * 0.20f, e };
              GfxRect a2 = { cx - d * 0.02f, cy - e * 0.5f, d * 0.34f, e };
              gfx_rect(a1, 0, GFX_COR, 0, 0, 0, 0.5f, 0.07f, 0.07f, 0.07f, 0.94f);
              gfx_rect(a2, 0, GFX_COR, 0, 0, 0, 0.5f, 0.07f, 0.07f, 0.07f, 0.94f); }
          }

          // `cardDepthEnabled` mais o interruptor por secao: `cardDepthPosters`
          // nas fileiras de catalogo, `cardDepthContinueWatching` na primeira.
          desenhaProfundidade(card, raio,
                              tipo == FILEIRA_CONTINUE ? ajustes_profundidade_cw()
                                                       : ajustes_profundidade_posters());

          // --- posterLabelsEnabled ---------------------------------------
          // Card DEITADO: a legenda vai DENTRO da moldura, sobre um degrade que
          // cobre 54% da altura, com 14 de recuo lateral e 12 da base
          // (.home-poster-landscape-copy). Card EM PE: vai ABAIXO do poster, num
          // bloco de 74 de altura com 8 de padding no topo (.home-poster-copy).
          if (tipo != FILEIRA_CONTINUE && tipo != FILEIRA_RETORNO && !editorial(tipo) && ajustes_rotulos_poster() && cItem) {
            const char *nome = cItem->titulo[0] ? cItem->titulo : NULL;
            const char *sub  = cItem->genero[0] ? cItem->genero : NULL;
            if (deitado && nome) {
              GfxRect veu = { px, py + h * (1.0f - NV_LAND_VEU), w, h * NV_LAND_VEU };
              gfx_rect(veu, 0, GFX_VEU, 0, 0, 0, raio, 0, 0, 0, 0.80f);
              float maxW = w * NV_LAND_COPY_MAXW;
              float bx = px + NV_LAND_COPY_PAD;
              TxtLinha tn = txt_linha_corta(TXT_CAPTION, nome, 245, 246, 250, 255, maxW);
              if (sub) {
                TxtLinha ts = txt_linha_corta(TXT_MINI, sub, 200, 202, 210, 255, maxW);
                txt_desenhar_alpha(ts, bx, py + h - NV_LAND_COPY_BASE - ts.h, 0.85f);
                txt_desenhar_alpha(tn, bx,
                                   py + h - NV_LAND_COPY_BASE - ts.h - 4.0f - tn.h, 0.98f);
              } else {
                txt_desenhar_alpha(tn, bx, py + h - NV_LAND_COPY_BASE - tn.h, 0.98f);
              }
            } else if (rotuloFora && nome) {
              float bx = px + NV_POSTER_COPY_PADX;
              float by = py + h + NV_POSTER_COPY_PADT;
              float maxW = w - NV_POSTER_COPY_PADX * 2.0f;
              // 16/500 e 13/400 rgba(255,255,255,.7) — os corpos de
              // .home-poster-title e .home-poster-subtitle.
              TxtLinha tn = txt_linha_corta(TXT_CAPTION2, nome, 245, 246, 250, 255, maxW);
              txt_desenhar_alpha(tn, bx, by, 0.98f);
              if (sub) {
                TxtLinha ts = txt_linha_corta(TXT_MINI, sub, 255, 255, 255, 255, maxW);
                txt_desenhar_alpha(ts, bx, by + tn.h + 2.0f, 0.70f);
              }
            }
          }

          if(tipo==FILEIRA_TOP10) {
            char rank[8];snprintf(rank,sizeof rank,"%d",c+1);
            TxtLinha number=txt_linha(TXT_RANK,rank,240,241,245,255);
            TxtLinha ink=txt_linha(TXT_RANK,rank,16,17,20,255);
            float nx=px-12,ny=py+h-number.h-8;
            for(int dx=-2;dx<=2;dx+=2)for(int dy=-2;dy<=2;dy+=2)
              txt_desenhar(number,nx+dx,ny+dy);
            txt_desenhar(ink,nx,ny);
          }

          if (tipo == FILEIRA_CONTINUE)
            continuar_desenhar(cItem, (GfxRect){px, py, w, h});
          if (tipo == FILEIRA_RETORNO)
            continuar_desenhar(cItem, (GfxRect){px, py, w, h});

          // 4. DESTAQUE: titulo e metadados DENTRO da arte, sobre um veu
          // escuro na base — como o Apple TV faz. O titulo faz o papel do logo
          // embutido na arte-chave, que nos nao temos (o TMDB nem sempre tem
          // logo; quando tiver, entra aqui no lugar do texto).
          // CARD ABERTO: veu na base e o LOGO do titulo, como na TCL.
          //
          // O logo e nao o nome escrito com a fonte da interface: cada producao
          // tem tipografia propria, e escrever "The Pitt" em Inter apaga
          // justamente o que faz o titulo ser reconhecido de longe. Sem logo no
          // catalogo o card fica so com a arte — melhor que um nome generico
          // por cima dela.
          if (abre > 0.01f && cItem && cItem->logo[0]) {
            GLuint tl = tex_obter(cItem->logo);
            if (tl) {
              float pad = 34.0f * esc;
              float ap = tex_aspecto(cItem->logo);
              float hL, wL, maxW;
              GfxRect veu = { px, py, w, h };
              gfx_rect(veu, 0, GFX_VEU, 0, 0, 0, NV_RAIO_CARD, 0, 0, 0, 0.72f * abre);
              // SEM CHUTE DE ASPECTO. O fallback de 4.0 que estava aqui
              // desenhava um retangulo mais largo que a imagem, e o modo de
              // cartao RECORTA o que sobra — o "REACHER" saia com as duas
              // pontas cortadas. Sem medida do arquivo, nao desenha.
              if (ap <= 0.0f) { tl = 0; }
              // Largura MANDA, altura sai dela: assim o retangulo tem sempre o
              // aspecto da imagem e o recorte nunca acontece.
              //
              // MEDIDO na TCL no card aberto: logo de 163 px num card de 565
              // (29% da largura) e 66 de altura num card de 320 (21%). O teto de
              // altura existe para logo quadrado nao virar um bloco.
              maxW = w * 0.30f;
              wL = maxW; hL = wL / ap;
              if (hL > h * 0.22f) { hL = h * 0.22f; wL = hL * ap; }
              // GFX_MARCA/GFX_TEXTO, NAO GFX_CARD. O modo de cartao e para
              // ARTE: ele faz cover com 3% de over-scan de proposito (a margem
              // de parallax) e descarta o alfa da textura. Num logo isso corta
              // as duas pontas — o "REACHER" saia como "EACHE" — e ainda pinta
              // de preto onde deveria ser transparente.
              //
              // O par certo ja existia no projeto, na fileira de destaque:
              // tex_marca_escura decide se a forma vem do alfa (logo claro) ou
              // do desenho (logo escuro). Reusado aqui em vez de reinventado.
              if (tl) { GfxRect rl = { px + pad, py + h - pad - hL, wL, hL };
                GfxModo m = tex_marca_escura(cItem->logo) ? GFX_MARCA : GFX_TEXTO;
                gfx_tex_aspect_atual = 0.0f;
                gfx_rect(rl, tl, m, 0, 0, 0, 0.0f, 1, 1, 1, abre); }
            }
          }

          if (editorial(tipo)) {
            GfxRect veu = { px, py, w, h };
            gfx_rect(veu, 0, GFX_VEU, 0, 0, 0, raio, 0, 0, 0, 0.88f);


            // Logo do titulo, como no aparelho: cada producao tem tipografia
            // propria, e escrever o nome com a fonte da interface apaga isso.
            const CatItem *ci = cItem;
            GLuint tlogo = (ci && ci->logo[0]) ? tex_obter_larg(ci->logo, w * .65f) : 0;
            // Sem dado, sem texto — nao a lista de demonstracao que ficava
            // aqui e carimbava nome e genero de outro titulo no card.
            const char *nome   = (ci && ci->titulo[0]) ? ci->titulo : NULL;
            const char *genero = (ci && ci->genero[0]) ? ci->genero
                                : ci ? (!strcmp(ci->tipo, "series") ? "Série" : "Filme") : NULL;
            TxtLinha tg = genero
                        ? txt_linha_corta(TXT_HERO_META, genero, 226, 228, 233, 255, w - 64)
                        : (TxtLinha){ 0, 0, 0 };

            float pad = tipo == FILEIRA_DESTAQUE ? 28.0f : 22.0f;
            float base = py + h - pad;
            float yMeta = base - tg.h;
            float hTit;
            if (tlogo) {
              float ap = tex_aspecto(ci->logo);
              if (ap <= 0.0f) ap = 4.0f;
              hTit = h * .22f;
              float wTit = hTit * ap, maxW = w * .65f;
              if (wTit > maxW) { wTit = maxW; hTit = wTit / ap; }
              GfxRect rl = { px + pad, yMeta - hTit - 10.0f, wTit, hTit };
              gfx_tex_aspect_atual = 0.0f;
              { GfxModo m = tex_marca_escura(ci->logo) ? GFX_MARCA : GFX_TEXTO;
              gfx_rect(rl, tlogo, m, 0, 0, 0, 0.0f, 1, 1, 1, 1.0f); }
            } else if (nome) {
              TxtLinha tn = txt_linha_corta(TXT_CW_TITULO, nome, 245, 246, 249, 255, w - pad*2);
              hTit = (float)tn.h;
              txt_desenhar(tn, px + pad, yMeta - hTit - 10.0f);
            } else {
              hTit = 0.0f;
            }
            if (genero) txt_desenhar(tg, px + pad, yMeta);

            // Selo etario vermelho, a direita da linha de genero. SO COM VALOR:
            // o "16" de reserva que estava aqui carimbava uma faixa etaria em
            // todo card sem classificacao, e o selo vermelho tem cara de aviso
            // oficial — e o mesmo defeito do "14" cravado em descoberta.c, so
            // que na home.
            if (ci && ci->classificacao[0] && tg.w + 100 < w - pad*2) {
              char clas[8];
              snprintf(clas, sizeof clas, "%s%s", ci->classificacao[0] == 'A' ? "" : "A", ci->classificacao);
              { TxtLinha tb = txt_linha(TXT_CAPTION, clas, 255, 255, 255, 255);
                float bx = px + pad + tg.w + (genero ? 14.0f : 0.0f);
                GfxRect badge = { bx, yMeta + 2, tb.w + 16, tg.h - 4 };
                gfx_cor(badge, NV_RAIO_BADGE, 0.78f, 0.14f, 0.14f, 0.95f);
                txt_desenhar(tb, bx + 8, yMeta + 2); }
            }
          }

          // Feedback progressivo do gesto, sem duplicar o menu contextual. A
          // barra aparece somente enquanto o mesmo item esta sob pressao;
          // atingido o limiar, ctxmenu ja foi aberto e a soltura e consumida.
          if (okPressionando && okHold > 0.0f &&
              foco_pode_pressao_longa() && focus_indice(&foco, r, c)) {
            float bx = px + NV_HOME_TEXT_GUTTER;
            float bw = w - NV_HOME_TEXT_GUTTER * 2.0f;
            GfxRect trilho = { bx, py + h - 12.0f, bw, 4.0f };
            gfx_cor(trilho, 0.5f, 0.18f, 0.19f, 0.22f, 0.92f);
            gfx_cor((GfxRect){ bx, trilho.y, bw * okHold, trilho.h },
                    0.5f, 0.92f, 0.93f, 0.96f, 1.0f);
            TxtLinha dica = txt_linha(TXT_MINI,
                                      okHold >= 1.0f ? "Solte para abrir opções"
                                                     : "Segure para opções",
                                      225, 228, 235, 255);
            txt_desenhar_alpha(dica, bx, py + h - 38.0f, 0.92f);
          }
        }
      }
    }
    y += NV_LEGACY_ROW_HEAD_H + alturaTotalFil(r) + fileiraGap();
  }

  // DEPOIS DA ULTIMA FILEIRA: o aviso de que ha mais catalogo do que caberia.
  //
  // O limite de fileiras corta o que e PEDIDO pela rede, e isso e deliberado
  // (sete fileiras tem de custar sete GET). O que NAO pode acontecer e a pessoa
  // perder uma fileira que ela tinha e nao ter como saber por que — foi o relato
  // do issue #11: "recommended no longer showing up like before".
  //
  // AQUI e nao no topo: e onde quem procura a fileira que faltou vai parar. A
  // rolagem e mirada na fileira em foco, entao esta linha aparece justamente
  // quando o foco chega na ultima, com ~160 px de sobra abaixo dela.
  //
  // O texto diz O CAMINHO e nao so o fato. "Ha mais catalogos" sem dizer onde
  // mudar seria informar e nao resolver.
  { int fora = desc_catalogos_fora() + cortadasPeloLimite;
    if (fora > 0 && nFileiras > 0) {
      char aviso[160];
      snprintf(aviso, sizeof aviso,
               fora == 1 ? i18n("Cabe %d fileira a mais aqui")
                         : i18n("Cabem %d fileiras a mais aqui"), fora);
      TxtLinha l = txt_linha(TXT_CAPTION, aviso, 196, 199, 208, 255);
      txt_desenhar_alpha(l, ajustes_conteudo_x(), y, 0.92f);
      { TxtLinha c = txt_linha(TXT_CAPTION2,
                               "Ajustes  ·  Fileiras da Home  ·  Limite de fileiras",
                               150, 152, 160, 255);
        txt_desenhar_alpha(c, ajustes_conteudo_x(), y + l.h + 6.0f, 0.92f); }
    } }

  // PERGUNTA DE SAIDA. Fica por cima de tudo e some sozinha em 3 s; o segundo
  // Voltar dentro da janela e que fecha (ver home_evento).
  if (sairPerguntadoEm && SDL_GetTicks() - sairPerguntadoEm <= HOME_SAIR_MS) {
    TxtLinha t = txt_linha(TXT_CAPTION,
                           "Aperte Voltar de novo para sair do aplicativo",
                           232, 234, 240, 255);
    GfxRect caixa = { (NV_TELA_W - t.w - 56.0f) * 0.5f, NV_TELA_H - 118.0f,
                      t.w + 56.0f, t.h + 28.0f };
    gfx_cor(caixa, 14.0f / caixa.h, 0.10f, 0.10f, 0.12f, 0.96f);
    txt_desenhar_alpha(t, caixa.x + 28.0f, caixa.y + 14.0f, 0.98f);
  }

  gfx_opacidade_grupo=1;
  gfx_sem_recorte();
}

// Rede de seguranca, nao o gatilho principal. Numa TV o app raramente sai por
// aqui: a tecla Home do controle mata o processo e este caminho nao roda. Por
// isso a gravacao de verdade e a de repouso, em home_atualizar; esta so pega o
// caso em que a pessoa moveu o foco e saiu antes de completar o repouso.
void home_encerrar(void) { posGravar(); }
void home_registrar_retorno(int indice, double posSeg, double durSeg) {
  int novo = -1;
  if (indice >= 0 && durSeg > 1.0) {
    double p = posSeg / durSeg;
    if (p >= 0.01 && p < 0.90) novo = indice;
  }
  if (novo != retomarIndice) { retomarIndice = novo; retomarRev++; }
  else if (novo >= 0) retomarRev++; // atualiza barra/tempo da mesma sessao
  const CatItem *c = novo >= 0 ? cat_item_exato(novo) : NULL;
  snprintf(retomarId, sizeof retomarId, "%s", c ? c->imdb : "");
}
int home_quer_sair(void) { return sair; }

int home_item_focado(HomeItem *out) {
  if (!temItemFoco) return 0;
  *out = itemFoco;
  return 1;
}

int home_n_artes(void) { return nBd; }
// Quando ha catalogo, a arte vem dele (na ordem certa, casada com o titulo);
// sem catalogo, cai na varredura da pasta.
const char *home_backdrop(int i) {
  const CatItem *c = cat_item_exato(i);
  return c && c->backdrop[0] ? c->backdrop : NULL;
}
const char *home_arte(int i) { return (nBd && i >= 0 && i < nBd) ? bd[i] : NULL; }

// Consome o pedido de abrir: quem le, zera. Assim o OK vale uma vez so, mesmo
// que o quadro demore.
int home_pediu_abrir(void) { int v = pedidoAbrir; pedidoAbrir = 0; return v; }

// Consome o pedido de abrir o menu lateral: quem le, zera.
int home_pediu_menu(void) { int v = pedidoMenu; pedidoMenu = 0; return v; }
