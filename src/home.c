// Home nativa compatível com a interface moderna do Nuvio 1.0.1 legacy:
// hero no topo, rail fixa à esquerda e fileiras horizontais de posters. A
// infraestrutura nativa cuida de cache assíncrono, foco e transições.
#include "home.h"
// NV_LEVE (tools/tizen.sh --leve): build de diagnostico sem a animacao do
// cartaz em foco, um dos suspeitos do travamento de #72.
#ifdef NV_LEVE
#  define NV_SEM_GIF 1
#else
#  define NV_SEM_GIF 0
#endif
#include "trakt.h"
#include "simkl.h"
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
#include "artehero.h"
#include "colecoes.h"
#include "addons.h"   /* addons_nome_por_id: o addon de um grupo de colecoes */
#include "gif.h"
#include "badges.h"
#include "extras.h"
#include "diretor.h"
#include "descoberta.h"
#include "dados.h"
#include "tendencia.h"
#include <strings.h>
// Declarado a mao em vez de incluir detail.h: aquele header inclui ESTE (por
// causa do HomeItem), e o ciclo so nao explode por causa das guardas. Uma
// funcao de uma linha nao vale amarrar os dois arquivos.
float detail_progresso(void);
int detail_aberto(void);
int player_aberto(void);
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include "trailer.h"
#include "trailerimdb.h"
#include "trailerapple.h"

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
// A faixa editorial precisa de uma terceira alternativa para não terminar
// visualmente depois de apenas dois cards. Quando o catálogo que virou
// destaque entrega menos que isso, completamos com títulos já publicados no
// catálogo seguinte — sem inventar item, arte ou metadado.
#define DESTAQUE_OPCOES 3

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
  // Normalmente os itens de uma fileira são contíguos em `cat[]` e `ini` basta.
  // A faixa de destaque pode ganhar uma alternativa de outro catálogo quando a
  // fonte original tem só dois itens; neste caso guardamos os índices reais,
  // preservando identidade, arte e navegação do título.
  int itens[MAX_CARDS];
  int usaItens;
  // Fator de TAMANHO desta fileira, escolhido em Ajustes (fileiras.c). Fica no
  // Fileira e nao numa consulta por chave dentro do desenho: larguraDe/alturaDe
  // sao chamadas varias vezes por fileira por QUADRO, e cada consulta por chave
  // custa um mutex e uma varredura de 64 strings. 0 = nunca preenchido, lido
  // como 1.0 — e o caso da tabela de reserva abaixo e das fileiras montadas com
  // `Fileira v={0}`.
  float escala;
} Fileira;

static int fileiraItemIndice(const Fileira *f, int coluna) {
  if (!f || coluna < 0 || coluna >= f->n) return -1;
  return f->usaItens ? f->itens[coluna] : f->ini + coluna;
}

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

// Pedido de abrir o GUIA DE CANAIS: o "Ver tudo" de uma fileira de canal nao
// leva a grade de cartazes — leva ao guia, e o OK num cartao de canal abre o
// guia ja focado nele. `guiaId` guarda o id para guia_focar_id.
static int pedidoGuia;
static char guiaId[80];
static int fileiraEhCanal(const Fileira *f) {
  return f && (!strcmp(f->catTipo, "channel") || !strcmp(f->catTipo, "tv"));
}
int home_pediu_guia(char *id, int tam) {
  if (!pedidoGuia) return 0;
  pedidoGuia = 0;
  if (id && tam > 0) { snprintf(id, (size_t)tam, "%s", guiaId); guiaId[0] = 0; }
  return 1;
}

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
  // Top 100 / Top 10 viram a pilha com ranking. Detecta pelo titulo que o
  // catalogo trouxe (ex: "Top 100 · Movies"), nao pelo id — o id e do addon e
  // pode ser qualquer coisa.
  if (contemNome(nome, "top 100") || contemNome(nome, "top100") ||
      contemNome(nome, "top 10") || contemNome(nome, "top10"))
    return FILEIRA_TOP10;
  for (size_t i = 0; i < sizeof premios / sizeof premios[0]; i++)
    if (contemNome(nome, premios[i])) return FILEIRA_COLECAO;
  for (size_t i = 0; i < sizeof servicos / sizeof servicos[0]; i++)
    if (contemNome(nome, servicos[i])) return FILEIRA_SERVICO;
  return FILEIRA_NORMAL;
}
static int editorial(TipoFileira t) {
  return t == FILEIRA_DESTAQUE || t == FILEIRA_DESTAQUE_QUADRADO
      || t == FILEIRA_COLECAO || t == FILEIRA_SERVICO
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
static int sair = 0, pedidoAbrir = 0, pedidoTocar = 0, pedidoMenu = 0;
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
// O DESTAQUE E A PRIMEIRA FILEIRA DA HOME, e nao um cabecalho decorativo.
//
// Pedido do dono: "o hero ficar uma fileira que a gente pode ir trocando para o
// lado e com o botao de reproduzir; quando descer fica normal como ja ta".
//
// A alternativa era transformar o destaque na fileira 0 de verdade, dentro de
// `fileiras[]`. Nao serve: aquele vetor e a lista de CATALOGOS (cada fileira
// tem base, catId, cards e rolagem horizontal propria), e o destaque nao tem
// nada disso — ele tem um item por vez e ocupa a tela. Um bit de estado ao lado
// do foco descreve melhor o que a tela faz: existe um degrau ACIMA da fileira 0.
static int focoHero = 1;
// Quando a ultima tecla foi vista. O carrossel do destaque so anda depois de o
// controle ficar QUIETO: com o destaque em foco ha um contador na tela ("3 / 10")
// e uma arte que a pessoa escolheu — trocar por baixo dela a cada sete segundos
// le como a TV desobedecendo, e cada troca custa uma textura de 1920 (~8 MB),
// que na C9 (teto de 128 MB) despeja os cartazes visiveis.
//
// NUNCA fica em 0 depois da home viva: static zero fazia `agora - 0 >= 12 s`
// virar verdade assim que o arranque (login + catalogo) passava de 12 s, e o
// carrossel avancava no MESMO instante em que a home assentava — matando a
// janela do trailer do destaque (#86: detalhe toca, hero nao inicia).
static Uint32 heroUltTecla;
#define HOME_HERO_OCIO_MS 12000
// Quantos titulos o destaque percorre com a seta. O carrossel automatico
// atravessava o catalogo inteiro (281 titulos na conta do dono) — invisivel
// enquanto ninguem contava, mentira assim que aparece um "3 / 281" na tela e a
// pessoa tenta chegar ao fim.
#define HOME_HERO_LISTA 10
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
// TRAILER NO DESTAQUE (trailer.h; dono, 20/09/2026: "coloca para tocar no
// hero tb"). Com o foco parado no hero e a arte assentada, espera
// NV_TRAILER_HERO_ESPERA_MS e troca a arte pelo trailer mudo do titulo, no
// mesmo retangulo. Mover o foco, sair da home ou abrir qualquer coisa por
// cima (app.c diz, por `topo`) volta para a arte. Uma tentativa por titulo
// por parada de foco: o trailer que acabou nao recomeca.
static Uint32 heroTrailerDesde = 0;
static int    heroTrailerItem = -1, heroTrailerTentado = 0;
static char   heroTrailerImdb[24];
static Uint32 heroTrailerPreparandoAte = 0;
// 0 = ainda sem fonte, 1 = Apple, 2 = YouTube, 3 = falha final. A Apple e o
// YouTube contam como tentativas separadas; um erro de Apple libera exatamente
// uma tentativa de fallback sem voltar a abrir a mesma URL.
static int    heroTrailerFonte = 0, heroTrailerAppleFalhou = 0;
static float  heroTrailerFade = 0.0f;
static char   heroTrailerYoutubeId[16];
static int heroTrailerSegurando(Uint32 agora);

static Uint32 heroTrailerPrazoPreparacao(Uint32 agora) {
  // O limite de 3200 ms resolve a primeira fonte; depois de escolher Apple ou
  // YouTube, cada elemento precisa de sua propria janela para produzir
  // `playing`. Isso evita abrir o fallback ja vencido quando a Apple chega no
  // ultimo instante da janela de resolucao. Com no maximo duas fontes, o teto
  // total continua finito e esta declarado em layout.h.
  return agora + NV_TRAILER_HERO_PREPARA_MS;
}

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
// "Largura do item" (posterCardWidthDp) VIRANDO TAMANHO DE VERDADE.
//
// A opcao existia em Ajustes, era gravada e sincronizava com a conta — e nao
// mexia em UM pixel: ajustes_largura_poster_dp() nao tinha NENHUM consumidor no
// app inteiro. So o arredondamento (o ajuste vizinho) era aplicado. Relato do
// @praveencudz no #42: "poster size increases in the settings was not working".
//
// ESCALA RELATIVA, e nao a formula do web. O web calcula a largura a partir do
// dp (`buildModernHomeSizingStyle`: dp * 0,84 * 1,08 * 2), e aplicar isso aqui
// daria 229 px no padrao de fabrica contra os 212 que este app usa — e 212 nao
// e um chute, e a MEDICAO do app web rodando em 1920x1080 (ver NV_CARD_W em
// layout.h). Escalar pelo padrao preserva exatamente o que ja estava medido: em
// 126 dp o fator e 1,0 e nada muda; 200 dp da 1,59x e 72 dp da 0,57x, a mesma
// amplitude que a opcao oferece la.
//
// A ALTURA ACOMPANHA, senao o cartaz deixa de ser 2:3 e a arte distorce.
static float escalaDoAjuste(void) {
  int dp = ajustes_largura_poster_dp();
  if (dp < 72 || dp > 200) return 1.0f;    // valor de outra versao: nao mexe
  return (float)dp / 126.0f;               // 126 = o padrao de fabrica
}

static float larguraDe(TipoFileira t) {
  switch (t) {
    case FILEIRA_CONTINUE: return NV_DESTAQUE_W;
    // Opção editorial já existente: card panorâmico 16:9.
    case FILEIRA_DESTAQUE: return NV_DESTAQUE_EDITORIAL_W;
    // Opção adicional da referência: maior e quase quadrada, em 4:3.
    case FILEIRA_DESTAQUE_QUADRADO: return NV_DESTAQUE_QUADRADO_W;
    case FILEIRA_COLECAO: return 480.0f;
    case FILEIRA_SERVICO: return 360.0f;
    case FILEIRA_SOCIAL: return 540.0f;
    case FILEIRA_TOP10: return 212.0f;
    case FILEIRA_RETORNO: return 680.0f;
    case FILEIRA_CATALOGOS: return 360.0f;
    default:               return escalaDoAjuste() *
                             (ajustes_posteres_deitados() ? NV_CARD_LAND_W
                                                          : NV_CARD_W);
  }
}
// Quantos titulos o hero percorre. Vem do catalogo quando existe.
static int nAcervoHero(void) { int n = cat_n(); if (n) return n; return nBd ? nBd : 1; }
// O CONJUNTO DE TITULOS QUE O DESTAQUE PERCORRE.
//
// Tres fontes, escolhidas na folha de fileiras (fil_hero_fonte):
//   ""   os primeiros do catalogo, que e o que a home sempre fez
//   "*"  um sorteio do catalogo
//   ...  a chave de uma fileira: o destaque mostra os titulos dela
//
// GUARDA OS INDICES, e nao um intervalo: a fileira escolhida pode encolher, o
// catalogo pode ser republicado e o sorteio nao e contiguo por definicao. Um
// intervalo sobreviveria a nenhum dos tres.
static int heroSet[HOME_HERO_LISTA];
static int heroSetN;
// Assinatura do que a lista depende. Refazer a lista a cada quadro custaria o
// strcmp da preferencia e uma varredura das fileiras; refazer so quando algo
// mudou custa tres comparacoes de inteiro.
static unsigned heroSetRev, heroSetFilRev;
static int heroSetCat, heroSetNFil;

// Sorteio ESTAVEL dentro da sessao: o embaralhamento usa uma semente propria
// que so muda quando a lista e refeita. Sortear a cada quadro daria um destaque
// diferente por quadro; sortear uma vez por arranque e o que "lista aleatoria"
// quer dizer para quem esta no sofa.
static unsigned heroSemente;

static void heroMontarSet(void) {
  const char *fonte = fil_hero_fonte();
  int total = nAcervoHero();
  int i;
  heroSetN = 0;
  if (fonte[0] == '*' && !fonte[1]) {
    // Sorteio sem repeticao: embaralha os `total` indices por Fisher-Yates
    // parcial, usando so os HOME_HERO_LISTA primeiros passos.
    int n = total < HOME_HERO_LISTA ? total : HOME_HERO_LISTA;
    static int baralho[512];
    int m = total < (int)(sizeof baralho / sizeof *baralho)
          ? total : (int)(sizeof baralho / sizeof *baralho);
    unsigned x = heroSemente ? heroSemente : (heroSemente = (unsigned)SDL_GetTicks() | 1u);
    for (i = 0; i < m; i++) baralho[i] = i;
    for (i = 0; i < n && i < m; i++) {
      int j;
      x = x * 1103515245u + 12345u;
      j = i + (int)((x >> 16) % (unsigned)(m - i));
      { int t = baralho[i]; baralho[i] = baralho[j]; baralho[j] = t; }
      heroSet[heroSetN++] = baralho[i];
    }
    return;
  }
  if (fonte[0]) {
    // FILEIRA ESCOLHIDA. Nao existir nao apaga a escolha: o addon pode voltar,
    // e ate la o destaque cai no automatico — que e o comportamento de quem
    // nunca escolheu nada, nao um estado de erro.
    for (i = 0; i < nFileiras; i++) {
      if (strcmp(fileiras[i].chave, fonte)) continue;
      { int k;
        for (k = 0; k < fileiras[i].n && heroSetN < HOME_HERO_LISTA; k++) {
          int idx = fileiraItemIndice(&fileiras[i], k);
          if (idx >= 0 && idx < cat_n()) heroSet[heroSetN++] = idx;
        } }
      break;
    }
    if (heroSetN) return;
  }
  for (i = 0; i < total && heroSetN < HOME_HERO_LISTA; i++) heroSet[heroSetN++] = i;
}

static void heroSetGarantir(void) {
  unsigned fr = fil_revisao();
  int cn = cat_n(), nf = nFileiras;
  if (heroSetRev && fr == heroSetFilRev && cn == heroSetCat && nf == heroSetNFil)
    return;
  heroSetFilRev = fr; heroSetCat = cn; heroSetNFil = nf; heroSetRev = 1;
  heroMontarSet();
}

// Quantos titulos o destaque oferece.
static int heroNLista(void) {
  heroSetGarantir();
  return heroSetN;
}

// Posicao do titulo `idx` (indice de catalogo) dentro do conjunto, ou -1.
static int heroPosDe(int idx) {
  int i;
  heroSetGarantir();
  for (i = 0; i < heroSetN; i++) if (heroSet[i] == idx) return i;
  return -1;
}
// O indice de catalogo na posicao `pos`, ou -1.
static int heroIdxEm(int pos) {
  heroSetGarantir();
  return (pos >= 0 && pos < heroSetN) ? heroSet[pos] : -1;
}
// O indice que a seta move. `heroAtual` so muda quando a arte nova esta pronta
// (ver a troca em desenhaHero), entao ele NAO serve de ponto de partida para o
// passo seguinte: dois toques rapidos na direita voltariam ao mesmo titulo. A
// intencao mora em `heroDesejado`, que muda no toque.
static int heroIntencao(void) {
  return heroDesejado >= 0 ? heroDesejado : heroAtual;
}

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
// ARTE DO CARD. A url que o catalogo guarda ja foi dimensionada para ele —
// subir para a versao grande aqui seria baixar 3840 px para desenhar 419.
static const char *arte_por_formato(const CatItem *item, int deitado) {
  if (!item) return NULL;
  // Deitado e a url guardada (w1280 no TMDB, 1920 no metahub) — o MESMO
  // arquivo que o heroi e o detalhe vao promover (artehero_url): um download
  // por titulo. O decode escalado (jpegrapido.c) faz o card custar pouco.
  if (deitado) return item->backdrop[0] ? item->backdrop
                                      : (item->poster[0] ? item->poster : NULL);
  return item->poster[0] ? item->poster
                         : (item->backdrop[0] ? item->backdrop : NULL);
}

// ARTE DE TELA CHEIA, que e outra pergunta: o destaque desenha 1920 e por isso
// pede a versao grande da mesma arte (artehero.h).
//
// EPISODIO PRIMEIRO. Um item de "Continuar assistindo" de serie e um episodio,
// e o still dele e a unica imagem que diz onde a pessoa parou — a arte da serie
// e a mesma para as dez temporadas. Nem todo episodio tem still (o log da TV ja
// mostrou 404), entao a escolha e feita com tex_falhou: enquanto nao se sabe,
// pede-se o still; quando o cache diz que ele nao vem, cai na arte do titulo,
// uma vez e sem piscar.
static const char *arte_hero_do_item(const CatItem *item) {
  int fonte = ajustes_hero_fonte();
  const char *escolhida = artehero_url_fonte(item, fonte);
  // Ao escolher uma origem, o usuario esta pedindo a arte do titulo — nao o
  // still automatico do episodio. Automatico mantem o comportamento anterior,
  // inclusive o still de Continuar assistindo.
  if (fonte > 0) {
    if (escolhida && !tex_falhou(escolhida)) return escolhida;
    return artehero_url(item);
  }
  const char *ep = artehero_url_episodio(item);
  // STILL PEQUENO NAO VAI AO DESTAQUE (#85, pokazideia: "backdrop pixelado
  // em alguns titulos de Continuar assistindo"). O metahub serve o still no
  // tamanho que a fonte tiver, e para varios episodios isso e 400 px — a
  // 1920 vira mosaico. Abaixo de 900 px de origem, o fundo do titulo.
  if (ep && !tex_falhou(ep)) {
    int w = tex_largura_fonte(ep);
    if (w == 0 || w >= 900) return ep;
  }
  return artehero_url(item);
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
// `deitado` 2 quer dizer TELA CHEIA (o destaque). 1 e o card deitado.
static const char *arte_por_identidade(int indice, int deitado) {
  const CatItem *item = cat_item_exato(indice);
  const char *arte = (deitado == 2) ? arte_hero_do_item(item)
                                    : arte_por_formato(item, deitado);
  if (arte) return arte;
  if (cat_n() == 0 && indice >= 0) {
    if (!deitado && indice < nPst) return pst[indice];
    if (indice < nBd) return bd[indice];
  }
  return NULL;
}

// UM PASSO DO DESTAQUE, pela seta. Anuncia o desejo do mesmo jeito que o
// carrossel automatico: quem efetiva a troca continua sendo o desenho, quando a
// arte estiver pronta (ou quando a espera estourar). Assim o gesto tem a mesma
// resposta visual que a troca sozinha ja tinha, sem um segundo caminho.
static void heroPasso(int d) {
  int n = heroNLista();
  // O PASSO E NA POSICAO DENTRO DO CONJUNTO, e a arte vem do indice de
  // catalogo que mora nela: com a fonte "fileira escolhida" ou "sorteio" os
  // indices nao sao contiguos, e somar 1 ao indice andaria para um titulo que
  // nao esta na lista — e o contador da tela deixaria de bater com a arte.
  int pos = heroPosDe(heroIntencao());
  int alvo;
  if (pos < 0) pos = 0;
  alvo = heroIdxEm(pos + d);
  if (n <= 0 || alvo < 0) return;
  heroPendente = alvo;
  // Sem repouso: aqui a pessoa DISSE qual titulo quer. O repouso de
  // NV_HERO_REPOUSO_MS existe para o foco que atravessa uma fileira, onde cada
  // passo e caminho e nao destino.
  heroPendenteEm = SDL_GetTicks() - NV_HERO_REPOUSO_MS;
  if (heroDesejado != alvo) heroDesejadoEm = SDL_GetTicks();
  heroDesejado = alvo;
  // O carrossel automatico so volta a contar depois do intervalo inteiro: uma
  // troca sozinha logo depois do toque leria como "a TV ignorou o que eu fiz".
  heroTrocaEm = SDL_GetTicks() + NV_HERO_INTERVALO_MS;
  { const char *quente = arte_por_identidade(alvo, 2);
    if (quente) tex_arquivo(quente); }
}

static int foco_pode_pressao_longa(void) {
  // No destaque ha sempre um titulo do catalogo por tras, entao o menu do
  // cartaz vale ali como vale num card.
  if (focoHero) return cat_n() > 0;
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
    case FILEIRA_DESTAQUE: return NV_DESTAQUE_EDITORIAL_H;
    case FILEIRA_DESTAQUE_QUADRADO: return NV_DESTAQUE_QUADRADO_H;
    case FILEIRA_COLECAO: return 270.0f;
    case FILEIRA_SERVICO: return 203.0f;
    case FILEIRA_SOCIAL: return 240.0f;
    case FILEIRA_RETORNO: return 178.0f;
    case FILEIRA_TOP10: return 320.0f;
    case FILEIRA_CATALOGOS: return 203.0f;
    default:               return escalaDoAjuste() *
                             (ajustes_posteres_deitados() ? NV_CARD_LAND_H
                                                          : NV_CARD_H);
  }
}
static int temRotulo(TipoFileira t) {
  // O rotulo abaixo do poster so existe no poster EM PE. No card deitado o web
  // poe a legenda DENTRO da moldura (.home-poster-landscape-copy) e esconde o
  // bloco de fora (.home-poster-card.is-landscape .home-poster-copy{display:none}).
  return t == FILEIRA_NORMAL && ajustes_rotulos_poster()
      && !ajustes_posteres_deitados();
}
// O gap comum é 24px. Cards editoriais grandes precisam de 40px porque crescem
// 6% quando focados e, com o gap menor, quase encostam no vizinho.
static float gapDe(TipoFileira t) {
  return (t == FILEIRA_DESTAQUE || t == FILEIRA_DESTAQUE_QUADRADO)
       ? NV_CARD_GAP_GRANDE : NV_CARD_GAP;
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
// O DIVISOR E A ALTURA, e isto foi conferido no shader, nao deduzido: em
// FS_SDF (gfx.c) o fragmento faz `p = (uv - 0.5) * vec2(asp, 1.0)` e
// `b = vec2(0.5*asp, 0.5) - r`, ou seja a meia-extensao vertical e sempre 0,5 e
// o `r` e medido contra ela. Dividir pelo MENOR LADO, como estava aqui, so
// acerta em retangulo mais largo que alto; num cartaz (retrato) o menor lado e
// a LARGURA, e pedir 24/largura sobre uma altura maior pedia um canto bem mais
// fechado que os 24 px que o comentario dizia garantir. O teto e metade da
// LARGURA medida na mesma escala, senao um retangulo mais largo que alto
// termina com canto reto na horizontal e redondo na vertical.
static float raioDe(float w, float h) {
  if (h <= 0.0f) return NV_RAIO_CARD;
  { float r = ajustes_raio_poster_px() / h;
    float teto = 0.5f * w / h;
    if (r > 0.5f)  r = 0.5f;
    if (r > teto)  r = teto;
    return r; }
}

// --- Profundidade dos cartoes (`cardDepth*`) ---------------------------------
// O web faz isto com dois pseudo-elementos sobre a arte: um brilho na borda de
// CIMA com opacidade `--card-depth-edge` e uma faixa clara e discreta —
// `--card-depth-sheen` — atravessando a parte alta do cartao. `--card-depth-
// coverage` engorda a banda da borda: `12 + round(18 * coverage)` px
// (layoutPreferences.js:181). Sao os mesmos tres numeros da tela de Ajustes.
// Faixa de informacao do CARD ABERTO — ver a chamada. `esc` e a escala de
// foco do card (as medidas seguem o card, nao a tela).
static void desenhaFaixaAberta(const CatItem *ci, int r, float px, float py,
                               float w, float h, float esc, float abre) {
  float pad = 34.0f * esc, ch = 34.0f * esc, gap = 10.0f * esc;
  float cx = px + w - pad, cy = py + h - pad - ch;
  float a = abre;
  int delta = 0, novo = 0, temTend;
  { GfxRect veu = { px, py, w, h };
    gfx_rect(veu, 0, GFX_VEU, 0, 0, 0, NV_RAIO_CARD, 0, 0, 0, 0.72f * abre); }
  temTend = (r >= 0 && r < nFileiras)
          ? tend_delta(fileiras[r].chave, ci->imdb, &delta, &novo) : 0;

  // Da direita para a esquerda, cada chip devolve a largura que ocupou.
  // 1. Tendencia.
  if (temTend && (novo || delta != 0)) {
    char rot[24];
    float cr, cg, cb;
    TxtLinha l;
    if (novo) { ajustes_acento(&cr, &cg, &cb); snprintf(rot, sizeof rot, "%s", i18n("Novo")); }
    else if (delta > 0) { cr = 0.30f; cg = 0.78f; cb = 0.45f;
                          snprintf(rot, sizeof rot, "\xe2\x86\x91 %d", delta); }
    else { cr = 0.90f; cg = 0.36f; cb = 0.36f;
           snprintf(rot, sizeof rot, "\xe2\x86\x93 %d", -delta); }
    l = txt_linha(TXT_CAPTION, rot, 255, 255, 255, 255);
    { float bw = l.w + 22.0f;
      cx -= bw;
      gfx_cor((GfxRect){ cx, cy, bw, ch }, 0.5f, cr, cg, cb, 0.92f * a);
      { float lum = 0.2126f * cr + 0.7152f * cg + 0.0722f * cb;
        int c = lum > 0.55f ? 17 : 255;
        TxtLinha lt = txt_linha(TXT_CAPTION, rot, c, c, c, 255);
        txt_desenhar_alpha(lt, cx + 11.0f, cy + (ch - lt.h) * 0.5f, a); }
      cx -= gap; }
  }
  // 2. Classificacao etaria.
  if (ci->classificacao[0]) {
    TxtLinha l = txt_linha(TXT_CAPTION, ci->classificacao, 235, 235, 240, 255);
    float bw = l.w + 22.0f;
    cx -= bw;
    gfx_cor((GfxRect){ cx, cy, bw, ch }, 0.22f, 0.13f, 0.14f, 0.16f, 0.94f * a);
    gfx_rect((GfxRect){ cx, cy, bw, ch }, 0, GFX_ANEL, 0, 0.006f, 0, 0.22f, 1, 1, 1, 0.35f * a);
    txt_desenhar_alpha(l, cx + 11.0f, cy + (ch - l.h) * 0.5f, a);
    cx -= gap;
  }
  // 3. Ano · temporadas (o `meta` do catalogo, ja formatado).
  if (ci->meta[0]) {
    TxtLinha l = txt_linha(TXT_CAPTION, ci->meta, 226, 228, 233, 255);
    cx -= l.w;
    txt_desenhar_alpha(l, cx, cy + (ch - l.h) * 0.5f, 0.95f * a);
    cx -= gap + 4.0f;
  }
  // 4. IMDb + nota (o selo do heroi, na mesma escala).
  // Sem ID nao ha como atribuir a nota ao IMDb; em itens `tmdb:` a nota pode
  // ser do TMDB e o rotulo amarelo seria enganoso. O restante da faixa segue
  // independente e continua aparecendo quando existe.
  if (ci->nota > 0 && ci->imdb[0] && strncmp(ci->imdb, "tmdb:", 5) != 0) {
    float bw = badge_imdb_largura(ci->nota);
    float xMin = px + pad + (ci->logo[0] ? w * 0.34f : 0.0f);
    float badgeX = cx - bw;
    // O logo ocupa a esquerda da mesma faixa. Se os outros chips consumirem
    // todo o espaço, omitir o par é melhor que atravessar o logo ou o canto
    // arredondado do card.
    if (badgeX >= xMin) {
      cx = badgeX;
      badge_imdb(cx, cy + (ch - BADGE_H) * 0.5f, ci->nota, 0, a);
    }
  }
  // 5. Progresso em andamento: fio na base, dentro do raio do card.
  if (ci->progresso > 0 && ci->progresso < 90) {
    float fx = px + pad, fw = w - pad * 2, fy = py + h - 12.0f * esc, fh = 4.0f;
    float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
    gfx_cor((GfxRect){ fx, fy, fw, fh }, 0.5f, 1, 1, 1, 0.22f * a);
    gfx_cor((GfxRect){ fx, fy, fw * (float)ci->progresso / 100.0f, fh }, 0.5f, ar, ag, ab, a);
  }
}

static void desenhaProfundidade(GfxRect card, float raio, int ligadaAqui) {
  if (!ajustes_profundidade() || !ligadaAqui) return;
  float borda = ajustes_profundidade_borda();
  float brilho = ajustes_profundidade_brilho();
  float cobertura = ajustes_profundidade_cobertura();
  // OS DOIS SAO DEGRADE, e nao retangulo chapado — foi a queixa do dono:
  // "se ativar o brilho do card ele so coloca uma barra grossa no topo, fica
  // estranho". Era literalmente isso: um branco solido de 12 a 30 px em cima e
  // outro cobrindo 28% da altura, os dois com aresta dura embaixo. A propria
  // nota acima ja dizia que a referencia usa gradiente.
  //
  // O retangulo e desenhado com a ALTURA DO CARD e a rampa corta dentro dele
  // (uPar.x), em vez de um retangulo baixo com raio proprio: assim o realce
  // segue os cantos arredondados do card, que era o outro defeito visivel —
  // a faixa passava reta por cima do canto.
  if (borda > 0.001f) {
    float alcance = (12.0f + 18.0f * cobertura) / (card.h > 1.0f ? card.h : 1.0f);
    gfx_rect(card, 0, GFX_BRILHO_TOPO, 0, alcance, 0, raio,
             1.0f, 1.0f, 1.0f, borda * 0.55f);
  }
  if (brilho > 0.001f) {
    // O reflexo vai mais fundo e mais fraco: e o `--card-depth-sheen`, uma
    // claridade que desce pela parte alta, nao uma segunda borda.
    gfx_rect(card, 0, GFX_BRILHO_TOPO, 0, 0.34f, 0, raio,
             1.0f, 1.0f, 1.0f, brilho * 0.18f);
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
//
// E POR ISSO QUE O CRESCIMENTO VOLTOU AMARRADO AO ANEL, e nao solto: com a
// borda ligada o card mantem a caixa, como na referencia; com a borda
// DESLIGADA o foco perderia a unica marca que tinha, entao quem marca passa a
// ser o tamanho. Pedido do dono: "vamos deixar ele crescer ao focar quando
// tirar o contorno". As duas coisas nunca acontecem juntas.
//
// 6%, e o teto vem da medida que ja estava escrita aqui: o titulo da fileira
// fica 15 px acima dos cards, o crescimento e simetrico em torno do centro, e
// um poster de 322 px sobe metade de 322*0,06 = 9,7 px. Os 9% do tvOS subiam
// 14,5 px e era isso que encostava no titulo.
static float escalaDe(TipoFileira t) {
  (void)t;
  return ajustes_borda_foco() ? 0.0f : 0.06f;
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

// QUANTO O CARD EM FOCO PASSA DA CAIXA EM REPOUSO, pela DIREITA (#103).
//
// O alvo da rolagem horizontal media o card parado e somava so metade da
// escala de foco. Faltavam as outras duas parcelas, e o sintoma foi o da foto
// do issue: com o foco no fim da fileira o cartao cresce, encosta na borda e e
// cortado — a fileira nao anda o bastante para ele caber INTEIRO.
//
// As tres parcelas, nas mesmas contas que o desenho faz la embaixo:
//
//   ESCALA   lw*(esc-1)/2. Sao os 6% de escalaDe: num cartaz de 322 px, 9,7 px
//            de cada lado. VALE ZERO com a borda de foco ligada — la o foco se
//            marca pelo anel e o card nao cresce (ver escalaDe).
//
//   ABERTURA (w - lw*esc). Com `expandir_poster` o cartao em repouso vira 16:9
//            a partir do centro e, como o vizinho e EMPURRADO, o crescimento e
//            todo para a direita. Num cartaz de 322x483 o aberto mede
//            483*1,06*16/9 = 910 px contra 341 px fechados: 569 px de sobra,
//            CINCO VEZES os 104 px de NV_HOME_SAFE_RIGHT. Era esta a parcela
//            que faltava na foto — o card cortado la esta aberto, nao so
//            crescido.
//
//   ANEL     NV_ANEL_FOCO, 4 px POR FORA da arte, e so existe justamente
//            quando a escala vale zero. Com a conta antiga a fileira com borda
//            ligada reservava NADA.
//
// `abre` e o expAbre do quadro (0 fechado, 1 aberto) — entra como parametro em
// vez de ser lido do estatico para esta funcao ficar pura e o teste de layout
// conseguir cobrar o card aberto sem mexer no relogio da expansao.
static float sobraDireitaFoco(int r, float abre) {
  float lw  = larguraFil(r);
  float esc = 1.0f + escalaDe(fileiras[r].tipo);
  float w   = lw * esc;
  if (abre > 0.0f)
    w += (alturaFil(r) * esc * NV_EXP_ASPECTO - lw * esc) * abre;
  return (w - lw * esc) * 0.5f + w * 0.5f - lw * 0.5f
       + (ajustes_borda_foco() ? NV_ANEL_FOCO : 0.0f);
}

// Alvo da rolagem horizontal da fileira `r` com a coluna `col` em foco, a
// partir da rolagem `atual`. Separada de home_atualizar porque e ELA que o
// #103 errava, e um teste sem janela nao consegue chamar o laco de atualizacao
// inteiro (ele arrasta trailer, sync e SDL atras de si).
static float alvoScrollFil(int r, int col, float atual, float abre) {
  float passo = passoFil(r);
  float esq   = (float)col * passo;
  float dir   = esq + larguraFil(r);
  float util  = NV_TELA_W - ajustes_conteudo_x() - NV_HOME_SAFE_RIGHT;
  float folga = sobraDireitaFoco(r, abre);
  float alvo  = atual;
  if (dir + folga - alvo > util) alvo = dir + folga - util;
  // A esquerda ganha a MESMA folga: sem ela o card em foco na primeira coluna
  // visivel apoia a caixa em repouso na margem e o que cresce (ou o anel) fica
  // por baixo da barra lateral. Na coluna 0 o alvo fica negativo e o corte
  // abaixo o devolve a zero, que e o inicio da fileira de sempre.
  if (esq - folga - alvo < 0.0f) alvo = esq - folga;
  if (alvo < 0) alvo = 0;
  return alvo;
}

// FilTipo (escolha em Ajustes) -> TipoFileira (forma que o desenho conhece).
// A traducao vive aqui porque este e o unico arquivo que sabe o que cada forma
// mede; fileiras.h nao pode incluir home.h sem fechar um ciclo de headers.
static TipoFileira tipoDaEscolha(int t) {
  switch (t) {
    case FIL_TIPO_CARTAZ:   return FILEIRA_NORMAL;
    case FIL_TIPO_DESTAQUE: return FILEIRA_DESTAQUE;
    case FIL_TIPO_DESTAQUE_QUADRADO: return FILEIRA_DESTAQUE_QUADRADO;
    case FIL_TIPO_COLECAO:  return FILEIRA_COLECAO;
    case FIL_TIPO_SERVICO:  return FILEIRA_SERVICO;
    default:                return FILEIRA_TOP10;
  }
}

// --- ONDE A PESSOA ESTAVA NA HOME -------------------------------------------
//
// DENTRO DA SESSAO, e so dentro dela. Ir ao detalhe e voltar, trocar de perfil,
// mexer em Ajustes ou receber mais um catalogo da rede devolve a pessoa a
// coluna e a rolagem em que ela estava; FECHAR O APP nao devolve nada, e as
// fileiras reabrem no primeiro cartaz.
//
// O defeito que isto conserta e o 2, e e o unico que sobrou:
// `colunaLembrada[]` MORRIA A CADA REPUBLICACAO. focus_iniciar faz
// `memset(f, 0, sizeof *f)`, e a remontagem so devolvia `fileira` e `coluna`.
// Depois de qualquer republicacao — catalogo da rede chegando (sao ~16 nos
// primeiros segundos), mudanca em Ajustes, fim de reproducao — descer e subir
// de fileira caia na coluna 0 em vez da coluna lembrada. E exatamente o defeito
// que o comentario no topo de focus.h diz que a estrutura existe para evitar.
//
// O QUE FOI EMBORA DAQUI, e por que nao volta (#95): havia um `home-pos.txt`
// que gravava a coluna e a rolagem de CADA fileira e as devolvia no arranque
// seguinte. O relato foi direto — "I would expect the row to start from the
// first tile again after restarting the app" — e nao ha meio-termo util: a
// unica coisa que aquele arquivo carregava era exatamente a coluna que o
// arranque tem de esquecer. Com ele foram tambem a guarda de "leitura pendente"
// espalhada pelo teclado e o relogio de repouso de 1,2 s que existia so para
// nao gravar um arquivo por movimento de D-pad — nao ha mais o que gravar.
//
// NAO ACRESCENTE UM AJUSTE para escolher entre as duas. O issue pede o
// comportamento, nao a opcao, e um interruptor aqui custaria de volta o arquivo
// inteiro (leitura, casamento por conjunto, gravacao em repouso) para servir o
// lado que ninguem pediu.
//
// A CHAVE, NUNCA O INDICE. `colunaLembrada[]` e `scrollX[]` sao indexados por
// INDICE de fileira, e o indice nao sobrevive a nada: a ordem sai de
// art/fileiras.txt e de Ajustes, uma fileira nova entra no meio, o limite corta
// o fim. Guardar por indice devolveria a coluna 11 de "Continuar assistindo"
// para dentro de "Oscars 2026". Toda linha aqui e casada por chave.

// Uma fileira, do jeito que a posicao a conhece. `scrollX` em INTEIRO de
// proposito: sub-pixel de rolagem nao e informacao — e o que sobreviveu do
// formato de texto que o arquivo tinha.
typedef struct { char chave[192]; int coluna; int scrollX; } HomePos;

// INSTANTANEO VIVO: tirado no comeco de sincronizarFileiras, devolvido no fim.
// E o que conserta o defeito acima. static e nao pilha pelo mesmo motivo do
// `arranjo` la embaixo: sao ~6,5 KB e a funcao ja carrega dois vetores desse
// porte. So o fio do desenho toca nisto.
static HomePos posViva[MAX_FIL];
static int     nPosViva;
static char    posVivaFoco[192];
static int     posVivaCol;

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
    // `focoHero` NAO E DESLIGADO AQUI. Esta tabela e a da SESSAO, e ela e
    // reaplicada a cada uma das ~16 republicacoes do arranque: apagar o
    // destaque aqui faria a home sair dele sozinha assim que o segundo
    // catalogo chegasse da rede. Nada se perde — `foco` fica onde a tabela o
    // pos, e o primeiro toque para baixo cai la, com a fileira ja rolada.
  }
  return achou;
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

  // A HOME ABRE NO DESTAQUE. Era aqui que morava a nota dizendo que o hero e
  // informativo e que a navegacao comeca na primeira fileira — deixou de ser
  // verdade: ele recebe foco, anda para o lado e tem botao. A fileira 0
  // continua sendo o primeiro degrau ABAIXO dele, e posAplicarTabela desliga
  // este estado quando ha posicao de ontem para restaurar.
  focoHero = 1;
  int cols[MAX_FIL];
  for (int i = 0; i < nFileiras; i++)
    cols[i] = fileiras[i].n + (fileiras[i].verTudo ? 1 : 0);
  focus_iniciar(&foco, nFileiras, cols);
  // NADA E LIDO DO DISCO AQUI (#95). A home abre no destaque e cada fileira
  // abre no primeiro cartaz; o que a sessao lembra nasce em posCapturar, na
  // primeira remontagem.
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
      // O DESTAQUE RESPONDE AO OK ANTES DAS FILEIRAS. Toque curto abre a pagina
      // do titulo; segurar abre o menu do cartaz, como em qualquer card.
      if (focoHero) {
        okDesde = 0; okPressionando = 0; okLongDisparado = 0; okHold = 0.0f;
        if (dur >= NV_HOLD_MS) ctx_abrir(heroAtual);
        else pedidoAbrir = 1;
        return;
      }
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
        const CatItem *ci=cat_item_exato(fileiraItemIndice(&fileiras[foco.fileira], foco.coluna));
        if(ci){pessoaSocial=*ci;pedidoPessoaSocial=1;}return;
      }
      if (fileiras[foco.fileira].tipo == FILEIRA_CATALOGOS) {
        if (foco.coluna >= 0 && foco.coluna < fileiras[foco.fileira].n) {
          vertudo_colecao(col_folder(fileiras[foco.fileira].folders[foco.coluna]));
        }
      } else if (noVerTudo) {
        if (fileiraEhCanal(&fileiras[foco.fileira])) {
          pedidoGuia = 1; guiaId[0] = 0;
        } else {
          vertudo_abrir(fileiras[foco.fileira].base, fileiras[foco.fileira].catTipo,
                        fileiras[foco.fileira].catId, fileiras[foco.fileira].titulo);
        }
      } else if (dur >= NV_HOLD_MS) {
        ctx_abrir(fileiraItemIndice(&fileiras[foco.fileira], foco.coluna));
      } else if (fileiraEhCanal(&fileiras[foco.fileira])) {
        // OK num cartao de canal abre o guia focado nele — o canal nao tem
        // pagina de detalhe que ajude: nao ha episodios, elenco ou "sobre".
        const CatItem *ci = cat_item_exato(fileiraItemIndice(&fileiras[foco.fileira], foco.coluna));
        pedidoGuia = 1;
        snprintf(guiaId, sizeof guiaId, "%s", ci ? ci->imdb : "");
      } else {
        // OK num card da retomada TOCA de onde parou quando o ajuste pede
        // (issue #93): abrirTitulo seguido de detail_pedir_reproduzir cai no
        // mesmo caminho do botao Reproduzir. Com o estilo "poster" a fileira
        // de CW vira FILEIRA_NORMAL, entao a CHAVE e o que a identifica; a
        // "Retomar agora" (FILEIRA_RETORNO) sempre responde assim, porque o
        // card dela ja e um convite a tocar. Segurar OK continua abrindo o
        // menu — o ramo NV_HOLD_MS acima nem chega aqui.
        const Fileira *fl = &fileiras[foco.fileira];
        if (ajustes_cw_ok_toca() &&
            (fl->tipo == FILEIRA_CONTINUE || fl->tipo == FILEIRA_RETORNO ||
             !strcmp(fl->chave, "continue_watching")))
          pedidoTocar = 1;
        else
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
    if (!focoHero) {
      // O canto superior esquerdo passou a ser o DESTAQUE: subir ate a fileira
      // 0 e parar ali deixaria um degrau invisivel entre "o topo" e "o topo de
      // verdade", e o segundo Voltar fecharia o app com a pessoa achando que
      // ainda tinha para onde subir.
      focoHero = 1;
      foco.fileira = 0;
      foco.coluna = 0;
      foco.colunaLembrada[0] = 0;
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
  heroUltTecla = SDL_GetTicks();
  // O DESTAQUE E UM DEGRAU ACIMA DA FILEIRA 0, e nao uma fileira do vetor: as
  // setas dele sao tratadas aqui e nao chegam ao focus_mover.
  if (focoHero) {
    if (k == SDLK_RIGHT) { heroPasso(1); return; }
    if (k == SDLK_LEFT) {
      // Mesma regra do resto da home: esquerda na PRIMEIRA posicao chama o menu
      // lateral. O destaque nao da a volta justamente para que essa saida
      // exista sempre no mesmo lugar.
      if (heroPosDe(heroIntencao()) <= 0) { pedidoMenu = 1; return; }
      heroPasso(-1); return;
    }
    if (k == SDLK_DOWN) { focoHero = 0; return; }
    if (k == SDLK_UP) return;
  } else if (k == SDLK_UP && foco.fileira == 0) {
    focoHero = 1;
    return;
  }
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
  // GUARDA CURTO POR CONTADORES, antes do hash das fileiras. O hash FNV sobre
  // todas as chaves/titulos/ini/n de cada CatFileira e o guarda de seguranca
  // (logo abaixo) e continua existindo — este so evita computa-lo quando nenhum
  // dos contadores relevantes mudou desde a ultima montagem. Cada contador e
  // um inteiro lido sob mutex barato (fil_revisao, fil_limite, col_revisao) ou
  // direto (cat_revisao); cat_revisao e bumpado em cat_definir_tudo,
  // cat_trocar_continuar E cat_republicar_fileiras, cobrindo todo caminho
  // que troca fils[].
  static unsigned ultCatRev, ultFilRev, ultColRev, ultFilLim;
  unsigned catRev = cat_revisao(), filRev = fil_revisao();
  int filLim = fil_limite();
  unsigned colRev = col_revisao();
  if (nCat == filsAplicadas && assin == prefsAplicadas &&
      catRev == ultCatRev && filRev == ultFilRev &&
      colRev == ultColRev && filLim == ultFilLim &&
      retomarAplicada == retomarRev &&
      ultCatRev) {   // ultCatRev=0: primeira chamada, cai no hash
    ultFilRev = filRev; ultColRev = colRev; ultFilLim = (unsigned)filLim;
    return;
  }
  ultCatRev = catRev; ultFilRev = filRev; ultColRev = colRev; ultFilLim = (unsigned)filLim;
  unsigned revisao = 2166136261u;
  // A escolha LOCAL de fileiras entra na mesma assinatura do catalogo: ordem,
  // liga/desliga, forma, tamanho e limite mudam a lista tanto quanto uma
  // fileira nova da rede. Sem isto, sair de Ajustes deixaria a home igual ate a
  // proxima publicacao da descoberta — que e o defeito que a assinatura de
  // preferencias logo acima existe para nao repetir. Duas chamadas por quadro,
  // nao uma varredura: fil_revisao e fil_limite leem um inteiro sob mutex.
  revisao = (revisao ^ filRev) * 16777619u;
  revisao = (revisao ^ (unsigned)filLim) * 16777619u;
  // AS COLECOES ENTRAM NA ASSINATURA. Sem este termo a home so remontava quando
  // uma fileira de CATALOGO mudava — e as colecoes da conta chegam depois de o
  // catalogo ja ter se acomodado (sync.c chama col_definir_json e so entao pede
  // a remontagem). Resultado: as pastas existiam em memoria e nao entravam na
  // tela ate alguma outra coisa mexer no catalogo.
  //
  // Era isso que o relator do #30 contornava desligando e religando uma fileira
  // em Ajustes: aquilo bumpa fil_revisao(), a assinatura muda, a home remonta e
  // a colecao aparece. Fechar e reabrir voltava ao mesmo lugar.
  revisao = (revisao ^ colRev) * 16777619u;
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
  int destaqueIndice = -1;
  for (r = 0; r < nCat && destino < MAX_FIL - 1; r++) {
    const CatFileira *cf = cat_fileira(r);
    if (!cf) break;
    if (cf->n < 1) continue;
    // `continueWatchingEnabled: false` tira a fileira da home inteira — nao a
    // esvazia, tira. E o que renderModernHomeLayout faz quando
    // computeContinueWatchingRenderState devolve a fileira desligada.
    if (!strcmp(cf->chave, "continue_watching") && !ajustes_cw_ligado()) continue;
    // A fileira pode ser republicada depois de uma montagem em que o destaque
    // usou índices compostos. Limpar a marca evita que uma linha comum herde
    // silenciosamente os índices daquela montagem anterior.
    fileiras[destino].usaItens = 0;
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
      destaqueIndice = destino;
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
  // UMA TERCEIRA OPÇÃO NO DESTAQUE. A fonte principal continua sendo a
  // primeira fileira escolhida pela conta; quando ela tem só dois títulos, a
  // terceira posição vem do próximo catálogo que já foi carregado. Assim o
  // card é real e comparável aos outros (arte, logo e metadados próprios), sem
  // fabricar um placeholder nem alterar a ordem das fileiras originais.
  if (destaqueIndice >= 0 && destaqueIndice < destino &&
      fileiras[destaqueIndice].n < DESTAQUE_OPCOES) {
    Fileira *d = &fileiras[destaqueIndice];
    int baseN = d->n;
    int c, r;
    for (c = 0; c < baseN && c < MAX_CARDS; c++) d->itens[c] = d->ini + c;
    d->usaItens = 1;
    for (r = destaqueIndice + 1;
         r < destino && d->n < DESTAQUE_OPCOES; r++) {
      Fileira *fonte = &fileiras[r];
      if (!fonte->base[0] || !fonte->catId[0]) continue;
      for (c = 0; c < fonte->n && d->n < DESTAQUE_OPCOES; c++) {
        int idx = fonte->ini + c;
        int repetido = 0, k;
        if (idx < 0 || idx >= cat_n()) continue;
        for (k = 0; k < d->n; k++)
          if (d->itens[k] == idx) { repetido = 1; break; }
        if (repetido) continue;
        d->itens[d->n++] = idx;
      }
    }
  }
  if (col_n()) {
    // GRUPOS DE COLECAO entram no fim, na ordem em que a conta os declarou.
    // A ordem da home e a ordem que a pessoa escolheu na conta (catordem) ou
    // na TV (fil_unir); esta lista so ACRESCENTA o que a conta tem e a home
    // ainda nao mostra. Nenhuma tabela fixa reordena por cima.
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
    const char *ch[MAX_FIL], *ti[MAX_FIL];
    int ord[MAX_FIL], q, k, w = 0, lim = fil_limite();
    for (q = 0; q < destino; q++) {
      // REGISTRA TAMBEM O QUE VAI SAIR abaixo. E o registro que deixa a tela de
      // Ajustes RELIGAR uma fileira desligada: desligada, ela nao existe mais
      // nem aqui nem em cat_fileira(), e sem a lista de conhecidas desligar
      // seria irreversivel pela TV.
      // Addon vazio: quem sabe o nome dele e a descoberta, que registra a mesma
      // chave com ele (o primeiro a saber preenche). Daqui saem o TIPO do
      // catalogo e a CONTAGEM, que so existem depois de a fileira ser montada.
      if (strcmp(fileiras[q].chave, "last_session")) {
        // GRUPO DE COLECOES leva o nome do addon dominante, para a tela de
        // fileiras agrupa-lo junto dos catalogos daquele addon ("deixar
        // agrupado as fileiras e colecoes por addons"). Misto ou sem addon
        // fica "" e a tela o rotula "Colecao".
        const char *addonNome = "";
        if (!strncmp(fileiras[q].chave, "collection_", 11)) {
          char id[96];
          if (col_grupo_addon(fileiras[q].titulo, id, sizeof id))
            addonNome = addons_nome_por_id(id);
        }
        fil_registrar(fileiras[q].chave, fileiras[q].titulo,
                      addonNome, fileiras[q].catTipo, fileiras[q].n);
      }
      ch[q] = fileiras[q].chave;
      ti[q] = fileiras[q].titulo;
    }
    fil_gravar_registro();
    // A lista de Ajustes passa a espelhar ESTA ordem enquanto ninguem tiver
    // reordenado. Sem isto o primeiro movimento em Ajustes reembaralhava a home
    // inteira em vez de mover uma fileira — ver fil_espelhar_ordem. O titulo
    // vai junto para a chave resgatada de tabela cheia nao mostrar a si mesma.
    fil_espelhar_ordem(ch, ti, destino);
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
    // O LIMITE CONTA O QUE CUSTA REQUISICAO, E SO ISSO.
    //
    // A definicao esta escrita em descoberta.c, onde o mesmo numero e aplicado:
    // "corta o que vai ser PEDIDO pela rede, e nao o desenho: sete fileiras tem
    // de custar sete GET, senao o ajuste economiza pixel e nao trabalho".
    //
    // Aqui ele estava sendo aplicado uma SEGUNDA vez, sobre a lista ja montada,
    // e nessa lista entram coisas que nao pedem nada a rede: as fileiras de
    // COLECAO (pastas que ja estao em memoria) e as fixas — "Continuar
    // assistindo", "Amigos assistindo", "Retomar agora". Elas consumiam vagas
    // de um orcamento que existe para poupar REDE, e o que sobrava do teto era
    // cortado pelo fim — que e onde as colecoes entram (ver o bloco de col_n()
    // acima). Com o Trakt ligado, as fileiras dele empurravam o corte para
    // cima e as colecoes eram as primeiras a cair. E a metade do #30 que o
    // conserto da assinatura nao resolvia.
    //
    // Agora so conta quem tem `base` e `catId`, que e exatamente o par que
    // define um catalogo de addon — a mesma condicao que decide o card
    // "Ver tudo" mais acima. MAX_FIL continua sendo o teto duro da estrutura.
    // COMPACTA, NAO TRUNCA. Truncar no primeiro catalogo excedente jogaria fora
    // tudo que vem DEPOIS dele — e as fileiras de colecao entram justamente no
    // fim da lista. O corte tem de pular o catalogo que passou do orcamento e
    // seguir, deixando passar quem nao pede rede.
    { int q2, rede = 0, mantidas = 0;
      for (q2 = 0; q2 < w; q2++) {
        int pedeRede = arranjo[q2].base[0] && arranjo[q2].catId[0];
        if (pedeRede && ++rede > lim) continue;
        if (mantidas != q2) arranjo[mantidas] = arranjo[q2];
        mantidas++;
      }
      cortadasPeloLimite = w - mantidas;
      w = mantidas; }
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
  // Primeira batida da home viva: ancora o ocio do carrossel no "agora", nao
  // no zero do BSS (ver nota em heroUltTecla).
  if (!heroUltTecla) heroUltTecla = agora;

  // A Home continua atualizando catalogo e progresso enquanto uma tela de
  // detalhe/player esta na frente, mas o hero nao pode mudar escondido. Se a
  // troca vencer nesse intervalo, a volta exibiria uma arte que nunca foi
  // observada e a seleção de logo poderia divergir do detalhe que acabou de
  // sair. O relógio é rearmado na volta, preservando a escolha visível.
  const int heroOculto = detail_aberto() || player_aberto();
  static int heroOcultoAntes;
  if (heroOculto) {
    if (!heroOcultoAntes) {
      // Um desejo armado antes da abertura nao pode ser efetivado pelo
      // desenho que ainda aparece por baixo durante a transição.
      heroDesejado = -1;
      heroPendenteEm = agora;
    }
    heroUltTecla = agora;
    heroOcultoAntes = 1;
  } else if (heroOcultoAntes) {
    heroUltTecla = agora;
    heroPendenteEm = agora;
    heroTrocaEm = agora + NV_HERO_INTERVALO_MS;
    heroDesejado = -1;
    heroOcultoAntes = 0;
  }

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
    ctx_abrir(focoHero ? heroAtual : fileiraItemIndice(&fileiras[foco.fileira], foco.coluna));
  }

  // O catalogo pode encolher entre duas respostas. Normalizar os indices do
  // carrossel evita que um estado stale caia no wrap de cat_item().
  { int total = nAcervoHero();
    if (heroAtual < 0 || heroAtual >= total) heroAtual = 0;
    // COMECAR DENTRO DO CONJUNTO — E SO QUANDO O DESTAQUE ESTA NO COMANDO.
    //
    // `focoHero` e a condicao que faltava, e a falta dela foi um defeito de
    // tela inteira: nas fileiras o heroi segue o CARD em foco, e o card quase
    // nunca esta entre os dez do conjunto. Sem esta guarda, o quadro seguinte a
    // cada movimento via `heroAtual` "fora da lista" e o devolvia para o
    // primeiro item — a arte voltava para o mesmo titulo a cada passo, que e o
    // "travado no Tenet e piscando mesmo movendo" que o dono relatou.
    //
    // Com o destaque em foco a regra continua valendo e continua necessaria:
    // ali o contador "3 / 10" tem de corresponder a arte, e um `heroAtual`
    // fora do conjunto nao tem posicao para mostrar.
    if (!heroOculto && focoHero && heroNLista() > 0 && heroPosDe(heroAtual) < 0) {
      int primeiro = heroIdxEm(0);
      if (primeiro >= 0) heroAtual = heroAnterior = heroPendente = primeiro;
    }
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
  if (!heroOculto) {
  {
    int alvo = -1;
    // COM O DESTAQUE EM FOCO, a arte e escolhida por ele — pela seta ou pelo
    // carrossel — e nao pelo card que ficou para tras nas fileiras.
    if (!focoHero && foco.fileira >= 0 && foco.fileira < nFileiras) {
      int i = fileiraItemIndice(&fileiras[foco.fileira], foco.coluna);
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
      // AQUECE O ARQUIVO JA, no passo do foco e nao no da troca. O pedido de
      // 1920 so acontecia quando o candidato virava heroi desejado — depois do
      // repouso — e o download+decode de uma arte de tela cheia nao cabe no
      // prazo de NV_HERO_ESPERA_MS: o log do #39 media arte chegando entre
      // ~700 ms e ~8,6 s DEPOIS do estouro. tex_arquivo baixa para o cache de
      // disco por um pedido de 128 px — centavos de textura, nao os ~8 MB do
      // hero — e quem atravessa a fileira rapido tem o decode descartado por
      // pedidoObsoleto antes de virar textura. Quando a troca enfim acontece,
      // garantirLocal acha o arquivo no disco e so resta o decode.
      { const char *quente = arte_por_identidade(alvo, 2);
        if (quente) tex_arquivo(quente); }
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
    } else if (alvo < 0 && agora >= heroTrocaEm &&
               (!focoHero || agora - heroUltTecla >= HOME_HERO_OCIO_MS) &&
               // Com o trailer tocando, preparando ou ainda dentro da janela
               // finita de fonte, o carrossel espera. Se o ajuste for desligado
               // ou o prazo vencer, heroTrailerSegurando libera a rotacao no
               // mesmo quadro e home_trailer_passo fecha o elemento velho.
               !heroTrailerSegurando(agora)) {
      // Sem card em foco, agenda o proximo item e deixa o desenho efetivar a
      // troca somente quando a textura ou o placeholder ja estiver pronto.
      // A MESMA LISTA QUE A SETA PERCORRE. O carrossel andava pelo catalogo
      // inteiro; com o contador na tela isso viraria um "3 / 281" que ninguem
      // atravessa e que contradiz o que a seta faz.
      int total = heroNLista();
      int pos = heroPosDe(heroAtual);
      int proximo = total > 0 ? heroIdxEm(((pos < 0 ? 0 : pos) + 1) % total) : 0;
      if (proximo < 0) proximo = 0;
      heroPendente = proximo;
      heroPendenteEm = agora - NV_HERO_REPOUSO_MS;
      // O carrossel tambem ganha o arquivo quente: o proximo item ja e
      // conhecido aqui, muito antes de o prazo de espera comecar a contar.
      { const char *quente = arte_por_identidade(proximo, 2);
        if (quente) tex_arquivo(quente); }
      if (heroDesejado != proximo) heroDesejadoEm = agora;
      heroDesejado = proximo;
      heroTrocaEm = agora + NV_HERO_INTERVALO_MS;
    }
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
      // UM FOCO POR VEZ. Com o destaque em foco, o card que ficou para tras
      // continuava com o anel aceso: a tela mostrava dois lugares selecionados
      // e o D-pad so obedecia a um deles. O `foco` nao e zerado — ele guarda
      // onde a pessoa estava, e e para la que o baixo devolve.
      float alvo = (!focoHero && focus_indice(&foco, r, c)) ? 1.0f : 0.0f;
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
      // A folga cobre TUDO o que o card em foco tem por fora da caixa em
      // repouso — escala, abertura 16:9 e anel. Ver sobraDireitaFoco (#103).
      // `expAbre` so vale para a coluna que esta aberta: o alvo acompanha a
      // abertura enquanto ela acontece, e a fileira desliza junto em vez de
      // deixar o card crescer para fora da tela.
      float abre = (r == expFileira && foco.coluna == expColuna) ? expAbre : 0.0f;
      float alvo = alvoScrollFil(r, foco.coluna, scrollX[r], abre);
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
  if (focoHero) {
    // ROLAGEM NEGATIVA: as fileiras sao desenhadas em NV_SHELF_TOP - scrollY,
    // entao empurra-las para baixo e o mesmo movimento de sempre com o sinal
    // trocado — a mola que ja existe faz a ida e a volta, e nao ha um segundo
    // relogio para descasar do primeiro.
    alvoY = -NV_HOME_HERO_EMPURRA;
  } else {
    int r = foco.fileira;
    for (int i = 0; i < r && i < nFileiras; i++)
      alvoY += NV_LEGACY_ROW_HEAD_H + alturaTotalFil(i) + fileiraGap();
  }
  scrollY = anim_mola2_reduzida(&velY, scrollY, alvoY, dt,
                                NV_MOLA2_SCROLL, motionReduzido);

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
    const CatItem *p=(fileiraItemIndice(s, foco.coluna) >= 0)
                    ?cat_item_exato(fileiraItemIndice(s, foco.coluna)):NULL;
    if(p) {
      const char *arte=arte_hero_do_item(p);   // tela cheia: arte grande
      GLuint ta=arte?tex_obter_hero(arte):0;
      // A atividade continua com um ambiente discreto, mas quando o Trakt
      // trouxe arte real ela vira o assunto do hero. A pessoa fica apenas na
      // ficha social, onde o avatar tem contexto e não compete com o titulo.
      if(ta){gfx_tex_aspect_atual=tex_aspecto(arte);
        gfx_rect(r,ta,modoHero,0,0,0,0,0,0,0,aArte);gfx_tex_aspect_atual=0;}
      else if (!arte) desenhaArteAusente(r, 0.0f, p, aArte);

      const char *nome=p->socialNome[0]&&strcmp(p->socialNome,"Amigo")?p->socialNome:NULL;
      char autoria[240];
      // A ACAO VEM EM PORTUGUES DO trakt.c ("assistiu", "assistindo agora") e
      // e montada aqui numa frase so: sem i18n() na peca, o txt_linha traduzia
      // a frase inteira — que nao e chave — e o "Pedro · assistiu" saia em
      // portugues numa home em ingles (C9, 22/09). Mesmo defeito que social.c
      // ja tinha corrigido no cartao.
      const char *acaoTr=p->socialAcao[0]?i18n(p->socialAcao):"";
      if(nome&&acaoTr[0])snprintf(autoria,sizeof autoria,"%s  ·  %s",nome,acaoTr);
      else if(nome)snprintf(autoria,sizeof autoria,"%s",nome);
      else snprintf(autoria,sizeof autoria,"%s",acaoTr);
      txt_desenhar_alpha(txt_linha_corta(TXT_HERO_META,autoria,210,210,221,255,680),x,146,a);

      const char *urlPl=p->logo[0]?artehero_url_logo_larg(p->logo,520):NULL;
      GLuint tl=urlPl?tex_obter_larg(urlPl,520):0;
      if(tl&&tex_aspecto(urlPl)>0){
        float ap=tex_aspecto(urlPl),w=520,h=w/ap;
        if(h>104){h=104;w=h*ap;}
        gfx_rect((GfxRect){x,208,w,h},tl,tex_marca_escura(urlPl)?GFX_MARCA:GFX_TEXTO,
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
          snprintf(linha,sizeof linha,i18n("Conhecido por  %s"),con);
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
      const char *urlFl=(!ehDiretor && folder->logo[0])
        ?artehero_url_logo_larg(folder->logo,NV_COLLECTION_HERO_LOGO_MAX_W+40.0f):NULL;
      GLuint logo=urlFl?tex_obter_larg(urlFl,NV_COLLECTION_HERO_LOGO_MAX_W+40.0f):0;
      float ap=logo?tex_aspecto(urlFl):0;
      float fimTitulo=NV_COLLECTION_HERO_LOGO_Y+NV_COLLECTION_HERO_LOGO_MAX_H;
      if(logo&&ap>0){
        float w=NV_COLLECTION_HERO_LOGO_MAX_W,h=w/ap;
        if(h>NV_COLLECTION_HERO_LOGO_MAX_H){h=NV_COLLECTION_HERO_LOGO_MAX_H;w=h*ap;}
        gfx_rect((GfxRect){x,NV_COLLECTION_HERO_LOGO_Y,w,h},logo,
                 tex_marca_escura(urlFl)?GFX_MARCA:GFX_TEXTO,
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
          if(con[0]){char l[300];snprintf(l,sizeof l,i18n("Conhecido por  %s"),con);
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
    const char *arteD = arte_por_identidade(heroDesejado, 2);
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
        printf("[hero] espera %u ms%s hash=%08lx | n=%d media=%u pior=%u estouros=%d\n",
               esperou, heroEstourou ? " (ESTOUROU, sem arte)" : "",
               tex_hash_public(arteD),
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
  const char *arteA = arte_por_identidade(heroAtual, 2);
  const CatItem *cAnt = cat_item_exato(heroAnterior);
  const char *arteB = arte_por_identidade(heroAnterior, 2);
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
      printf("[hero] arte atrasada chegou em %u ms (prazo e %d) hash=%08lx\n",
             (unsigned)(SDL_GetTicks() - heroTardeEm), NV_HERO_ESPERA_MS,
             tex_hash_public(arteA));
      fflush(stdout);
      heroTardeItem = -1;
    }
  }
  // TRAILER TOCANDO ATRAS DO CANVAS no lugar da arte: furo no retangulo do
  // hero, a arte se apaga por cima dele (heroTrailerFade) e as rampas do
  // hero ficam como veu com alpha, para o texto seguir apoiado no mesmo
  // escuro. So no hero de titulo (colecao e social nao chegam aqui com
  // trailer: ver home_trailer_passo).
  float aTrailer = (heroTrailerFade > 0.005f && heroTrailerItem == heroAtual) ? heroTrailerFade : 0.0f;
  if (aTrailer > 0.0f) {
    GfxRect furo = r;
    if (furo.y + furo.h > NV_TELA_H) furo.h = NV_TELA_H - furo.y;
    gfx_furo(furo);
    aArte *= (1.0f - aTrailer);
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
  } else if (!tAtu && aTrailer <= 0.0f) {
    // ESPERANDO quando ha caminho de arte e ela ainda nao decodificou; ausente
    // quando o titulo nao tem arte nenhuma para pedir.
    desenhaPlaceholderHero(r, ci,
                           aArte * (heroEntra > 0.0f ? 1.0f : heroEntra),
                           arteA != NULL && arteA[0] != 0);
  }
  // SEM VEU SOBRE O TRAILER (dono, 20/09/2026: "quando tocar o trailer do
  // hero tirar o overlay, so voltar quando tiver so a arte"): as rampas vao
  // embora junto com a arte e voltam com ela.
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
  int seguirHero = (ci && ci->progresso == 0 && (trakt_e_a_seguir(ci->imdb) || simkl_e_a_seguir(ci->imdb)));

  // Linha de meta. No web sao tokens juntados por "•"; ci->genero ja chega
  // como "Filme · Terror", que e o par (tipo, primeiro genero) do web.
  char metaLinha[288];
  metaLinha[0] = 0;
  // DOIS CASOS, DUAS FRASES (dono, 20/09/2026): "A SEGUIR" e o PROXIMO
  // episodio, que so faz sentido quando o anterior terminou; quem parou no
  // meio "continua de onde parou". Os dois dizem QUAL episodio — o item de
  // "a seguir" ja carrega temporada/episodio do proximo (trakt.c) — e o nome
  // dele quando o catalogo tem.
  if ((contHero || seguirHero) && ci->temporada > 0) {
    char cab[192];
    snprintf(cab, sizeof cab, "S%d E%d%s%s", ci->temporada, ci->episodio,
             ci->nomeEpisodio[0] ? "  \xc2\xb7  " : "", ci->nomeEpisodio);
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
  if (contHero) snprintf(destaque, sizeof destaque, i18n("CONTINUAR DE ONDE PAROU  \xc2\xb7  %d MIN"),
                         ci->restanteMin);
  else if (seguirHero) snprintf(destaque, sizeof destaque, "%s", i18n("A SEGUIR"));
  const char *selo = (ci && ci->classificacao[0] && !contHero && !seguirHero) ? ci->classificacao : NULL;
  char nota[8];
  nota[0] = 0;
  if (ci && ci->nota > 0) snprintf(nota, sizeof nota, "%.1f", ci->nota / 10.0f);
  int temSec = (destaque[0] || selo || nota[0]);

  const char *sinopse = (ci && ci->sinopse[0]) ? ci->sinopse : "";

  // --- empilhamento de baixo para cima, como o flex-end do CSS ---
  //
  // O BLOCO DESCE JUNTO COM AS FILEIRAS. Pedido do dono: "deixar as informacoes
  // mais para baixo e subir so quando descer para a fileira". Ele nao ganha uma
  // animacao propria: anda exatamente o que `scrollY` empurrou, entao a
  // distancia entre a ultima linha do texto e o titulo da primeira fileira e a
  // mesma nos dois estados — e nao ha duas molas para descasar.
  //
  // A RESERVA e o que impede o botao de cair em cima do titulo da fileira. O
  // bloco e ancorado pela BASE (flex-end), entao pendurar o botao abaixo dele
  // sem descontar a altura seria desenhar 94px para dentro do espaco da fileira
  // — e a colisao so apareceria no estado empurrado, que e justamente o que a
  // foto do sofa mostra primeiro.
  float empurra = scrollY < 0.0f ? -scrollY : 0.0f;
  float aBotao = anim_clamp(empurra / NV_HOME_HERO_EMPURRA, 0.0f, 1.0f);
  float reservaBotao = aBotao * (NV_HOME_HERO_BOTAO_GAP + NV_HERO_BOTAO_H + 24.0f);
  float base = NV_SHELF_TOP - NV_HERO_COPY_GAP + descidaCopy
             + empurra - reservaBotao;
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
  // O catalogo guarda o logo do TMDB em `original` (4127 px de largura medidos
  // na C9, 1,3 a 1,6 s de decodificacao) e aqui ele nunca passa de
  // NV_LOGO_HERO_CHEIO_MAX_W. A politica de tamanho e a mesma do fundo e mora
  // em artehero.c; url que nao e do TMDB passa intacta.
  // Só o logo do hero fixa a seleção da sessão. Os cards vizinhos consultam a
  // seleção, mas não podem substituir a identidade que está na tela grande.
  // Durante a abertura/saída do detalhe a Home ainda pode ser desenhada por
  // baixo do backdrop. Nesse intervalo o item do hero pode ser A enquanto a
  // sessão ativa já é B; observar A ali sobrescreveria o snapshot de B antes
  // do primeiro frame do detalhe. A leitura simples mantém A no plano de
  // fundo, e a observação volta a ser permitida quando a Home recupera o
  // primeiro plano.
  const char *urlLogo = ci ? ((detail_aberto() || player_aberto())
                              ? artehero_logo_sessao(ci)
                              : artehero_logo_sessao_observar(ci)) : NULL;
  float maxWLogo = cheio ? NV_LOGO_HERO_CHEIO_MAX_W : NV_LOGO_HERO_MAX_W;
  // Durante a promoção para o hero, entregar a textura menor já pronta evita
  // um quadro vazio; o cache continua reprocessando para o teto final.
  GLuint tlogo = urlLogo ? tex_obter_larg_qualquer(urlLogo, maxWLogo) : 0;
  // Igual ao detalhe: nome escrito so quando nao ha logo ou o cache ja falhou.
  // Antes, qualquer decode pendente caia no ramo de texto — ao voltar do
  // detalhe (catalogo com url nova do TMDB) parecia "sumiu a arte do titulo".
  int mostraNomeLogo = !tlogo && (!urlLogo || tex_falhou(urlLogo));
  if (tlogo) {
    float ap = tex_aspecto(urlLogo);
    if (ap <= 0.0f) ap = 4.0f;
    float hTit = NV_LOGO_HERO_H, wTit = hTit * ap;
    if (wTit > maxWLogo) { wTit = maxWLogo; hTit = wTit / ap; }
    // object-position: left top — a arte encosta no TOPO da caixa.
    GfxRect rl = { x, logoY, wTit, hTit };
    gfx_tex_aspect_atual = 0.0f;
    // Logo escuro vira branco. Mesma regra da tela de detalhe: o TMDB nao marca
    // claro/escuro, entao a decisao sai da luminancia MEDIDA (tex_luminancia).
    // Logo claro ou colorido passa intacto; -1 (ainda carregando) nao tinge.
    { GfxModo m = tex_marca_escura(urlLogo) ? GFX_MARCA : GFX_TEXTO;
      // O LOGO DO TITULO acompanha a ARTE, nao o texto. MEDIDO: 205 ms depois
      // da tecla a arte antiga ainda estava a 85% e o logo JA tinha sumido por
      // inteiro; ele so reaparece no mesmo quadro em que a arte nova entra.
      gfx_rect(rl, tlogo, m, 0, 0, 0, 0.0f, 1, 1, 1, aTexto * heroEntra); }
  } else if (mostraNomeLogo) {
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
      // O mesmo selo de 28 px da aba Salvos e do detalhe; classificacao nao
      // ganha uma caixa vermelha propria em cada superficie.
      float bw = badge_largura(selo);
      if (cx + bw <= x + NV_HERO_SIN_W)
        cx += badge_desenhar(cx, ySec + (NV_LD_HERO_SEC - BADGE_H) * 0.5f,
                             selo, BADGE_NEUTRO, a) + 14.0f;
    }
    if (nota[0] && ci && ci->nota > 0) {
      float bw = badge_imdb_largura(ci->nota);
      if (cx + bw <= x + NV_HERO_SIN_W)
        badge_imdb(cx, ySec + (NV_LD_HERO_SEC - BADGE_H) * 0.5f,
                   ci->nota, 0, a);
    }
  }

  if (sinopse[0])
    txt_bloco(TXT_HERO_SIN, sinopse, 255, 255, 255, x, ySin, NV_HERO_SIN_W,
              NV_LD_HERO_SIN, aTexto, 3);

  // O BOTAO E A POSICAO, que so existem enquanto o destaque tem o foco.
  //
  // A opacidade vem da ROLAGEM, e nao de uma mola propria: `scrollY` ja e
  // negativo na medida exata do empurrao das fileiras, entao o botao aparece e
  // some EXATAMENTE junto com o movimento que o trouxe. Duas molas para o mesmo
  // gesto descasariam, e o olho le descasamento como defeito.
  { int n = heroNLista();
    if (aBotao > 0.004f && n > 0) {
      float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
      // OK ABRE A PAGINA DO TITULO — e o rotulo diz isso. "Reproduzir" seria a
      // promessa de comecar o filme, e quem aperta acaba numa pagina: o rotulo
      // tem de descrever o que a tecla FAZ, nao o que seria bonito escrever.
      const char *rot = i18n("Ver título");
      int tb = ajustes_tinta_foco();
      TxtLinha lb = txt_linha(TXT_CALLOUT, rot, tb, tb, tb, 255);
      float bh = NV_HERO_BOTAO_H;
      float bw = lb.w + 96.0f;
      float by = base + NV_HOME_HERO_BOTAO_GAP;
      GfxRect bt = { x, by, bw, bh };
      // Brilho difuso por tras do botao (0,9x a altura de folga, alpha 0,35):
      // a luz da pilula em foco do menu lateral (21/09/2026). Uma mancha de
      // ~450x160 px sobre a arte do hero — 0,035 tela, o unico acrescimo de
      // preenchimento da home nesta cara nova.
      { GfxRect luz = { bt.x - bh * 0.9f, bt.y - bh * 0.9f, bw + bh * 1.8f, bh * 2.8f };
        gfx_rect(luz, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, ar, ag, ab, 0.35f * aBotao); }
      // Raio = metade da ALTURA: o raio do gfx_cor e fracao da altura do
      // retangulo, entao 0,5 e a pilula exata em qualquer largura.
      gfx_cor(bt, 0.5f, ar, ag, ab, aBotao);
      // O TRIANGULO DE REPRODUZIR NAO ENTRA AQUI. Ele e a marca universal de
      // "comeca agora" e este botao nao comeca nada; desenha-lo seria a mesma
      // mentira do rotulo, so que em forma.
      txt_desenhar_alpha(lb, x + (bw - lb.w) * 0.5f, by + (bh - lb.h) * 0.5f,
                         aBotao);

      { char pos[24];
        int p = heroPosDe(heroIntencao());
        snprintf(pos, sizeof pos, "%d / %d", (p < 0 ? 0 : p) + 1, n);
        TxtLinha lp = txt_linha(TXT_HERO_META, pos, 196, 199, 208, 255);
        txt_desenhar_alpha(lp, x + bw + 28.0f, by + (bh - lp.h) * 0.5f,
                           aBotao * 0.92f); }

      // CONTINUIDADE DA ABERTURA: a pagina de titulo cresce a partir do
      // retangulo que o item ocupava. Com o foco no destaque esse retangulo e a
      // arte do proprio destaque, e sem isto o OK abriria o ULTIMO card que
      // recebeu foco — o titulo errado, com a animacao vindo de fora da tela.
      if (focoHero && ci) {
        itemFoco.indice = heroAtual;
        itemFoco.rect   = heroArteRect;
        itemFoco.arte   = arteDoItem(ci, NULL);
        itemFoco.titulo = ci->titulo;
        itemFoco.genero = ci->genero;
        itemFoco.meta   = ci->meta;
        temItemFoco = 1;
      }
    } }
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
  // Estado do ramo de GIF (#29), ao lado do da sequencia de JPEG porque os dois
  // descrevem o MESMO cartaz em foco e sao zerados juntos quando ele muda.
  //   gifAnima  -1 = ainda nao perguntei, 0 = nao e animado, 1 = e
  //   gifUltimo instante da ultima amostra, para o passo de 67 ms
  //   gifTex    a textura devolvida, reaproveitada entre as amostras
  static int gifAnima=-1;static Uint32 gifUltimo;static GLuint gifTex;
  // Quadro da sequencia de JPEG que ja esta resolvido, para nao reconsultar
  // o cache nos ~4 quadros de tela que cabem entre dois passos de 67 ms.
  static int seqIndice=-1;static GLuint seqTex;
  for (int c = 0; c < fileiras[r].n; c++) {
    float x = ajustes_conteudo_x() + c * passoFil(r) - scrollX[r];
    if (x + w < 0 || x > NV_TELA_W) continue;
    float f = animFoco[r][c], raio = raioDe(w, h);
    GfxRect card = {x, y, w, h};
    // O ANEL E OPCIONAL (Ajustes > Foco no cartaz). Sem ele o foco continua
    // dito pelo tamanho e pela animacao do cartaz — o que sai e so a borda.
    if (f > .01f && ajustes_borda_foco()) {
      float menor = w < h ? w : h;
      float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
      gfx_cor((GfxRect){x - NV_ANEL_FOCO, y - NV_ANEL_FOCO,
        w + 2*NV_ANEL_FOCO, h + 2*NV_ANEL_FOCO},
        (raio * menor + NV_ANEL_FOCO) / (menor + 2*NV_ANEL_FOCO), ar, ag, ab, f);
    }
    gfx_cor(card, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, 1);
    const ColFolder *folder=col_folder(fileiras[r].folders[c]);if(!folder)continue;
    const char *arte = folder->cover;
    // POR CARTAZ, e nao estatico: diz se ESTE quadro esta desenhando o GIF, e a
    // resposta muda de cartaz para cartaz dentro do mesmo laco.
    int gifDesenhando = 0;
    GLuint tex = arte && arte[0] ? tex_obter_larg(arte, w) : 0;
    // GIF DA CONTA, quando nao ha sequencia de JPEG (#29).
    //
    // O importador converte `focusGifUrl` em 001.jpg…090.jpg com ffmpeg, e o
    // ramo de baixo toca isso. Mas 117 das 169 pastas do pacote saem com
    // frames==0 — o importador so gera a sequencia quando o perfil trazia a URL
    // NA HORA da importacao —, e pasta que vem da conta nunca tem sequencia
    // nenhuma. Para todas essas, a unica animacao possivel e o proprio GIF.
    //
    // So o Tizen anima: la o navegador conta o tempo e compoe os quadros. No
    // webOS gif_textura devolve 0 (nao ha libgif nem IMG_LoadAnimation no
    // aparelho) e o cartaz fica na capa parada, como hoje. Ver gif.h.
    if(gif_pode_animar()&&
       foco.fileira==r&&foco.coluna==c&&folder->frames<1&&folder->focusGif[0] && !NV_SEM_GIF &&
       !ajustes_animacoes_reduzidas()) {
      int id=fileiras[r].folders[c];Uint32 now=SDL_GetTicks();
      // O ARQUIVO E PEDIDO FORA DO ATRASO de 350 ms. Dentro dele, o download so
      // comecaria depois do atraso e o primeiro quadro chegaria tarde; pedir
      // cedo custa uma consulta ao cache, que devolve NULL enquanto nao chegou.
      const char *arq = tex_arquivo(folder->focusGif);
      if(ultimo!=id){
        ultimo=id;desde=now;gifUltimo=0;gifAnima=-1;
        // gif_parar SOLTA O BLOB do cartaz anterior. Sem isto ele fica preso e
        // o proximo cartaz teria de revoga-lo tarde.
        gif_parar();
      }
      // gif_animado LE O ARQUIVO INTEIRO. Uma vez por cartaz, e nao por quadro.
      if(gifAnima<0&&arq) gifAnima=gif_animado(arq);
      if(arq&&gifAnima>0&&now-desde>350&&now-gifUltimo>=67) {
        // 67 ms e o mesmo passo da sequencia de JPEG. Cada chamada copia
        // 480x270 RGBA = 518 KB do canvas ate a textura; a 60 fps seriam
        // ~31 MB/s numa TV que ja e o gargalo do cache de imagem.
        GLuint motion=gif_textura(arq,480);
        gifUltimo=now;
        if(motion){
          tex=motion;
          // A PROPORCAO DA CAPA NAO VALE AQUI. Abaixo o desenho usa
          // tex_aspecto(arte), que e a da capa; o GIF do CDN pode vir em
          // qualquer proporcao. Zero deixa o desenho usar a moldura.
          gifDesenhando=1;
        }
      } else if(arq&&gifAnima>0&&now-desde>350&&gifTex){
        tex=gifTex;gifDesenhando=1;
      }
      if(tex&&gifDesenhando)gifTex=tex;
    }
    if(foco.fileira==r&&foco.coluna==c&&folder->frames>0 && !NV_SEM_GIF &&
       !ajustes_animacoes_reduzidas()) {
      int id=fileiras[r].folders[c];Uint32 now=SDL_GetTicks();
      if(ultimo!=id){ultimo=id;desde=now;seqIndice=-1;seqTex=0;}
      if(now-desde>350) {
        char frame[700];int index=(int)((now-desde-350)/67)%folder->frames+1;
        // SO QUANDO O QUADRO DA SEQUENCIA MUDA, e nao a 60 por segundo.
        //
        // Este bloco pedia TRES texturas ao cache em TODO quadro — a atual e
        // duas de pre-busca — enquanto o indice so avanca a cada 67 ms. Numa
        // TV a 60 fps sao 180 consultas por segundo aos mesmos tres arquivos,
        // cada uma pegando o mutex que os dois fios de decode tambem disputam,
        // e cada uma remarcando `ultimoQuadro` das tres entradas — o que
        // distorce o LRU a favor da sequencia e contra os cartazes visiveis.
        // O ramo do GIF logo acima ja andava no passo certo (`>=67`); este
        // ficou de fora.
        //
        // A textura resolvida fica guardada entre os passos, senao o cartaz
        // voltaria a capa parada nos quadros em que nao se consulta nada.
        if(index!=seqIndice){
          seqIndice=index;
          snprintf(frame,sizeof frame,"%s/%03d.jpg",folder->frameDir,index);
          // PASSAGEIRA, e nao tex_obter_larg: o quadro vale 67 ms, e pedido
          // como cartaz ele expulsava do cache, um por quadro, os posteres
          // das fileiras de cima (medido: 15 despejos/s com a tela parada).
          seqTex=tex_obter_passageira(frame,480);
          // UMA de pre-busca, nao duas: a segunda so existia para cobrir o
          // caso de a primeira nao ter chegado, e a 67 ms de passo ela chega.
          // Cada quadro da sequencia e 480x270 RGBA = 518 KB no cache; uma
          // pasta de 90 quadros sao 46 MB de um orcamento de 96.
          snprintf(frame,sizeof frame,"%s/%03d.jpg",folder->frameDir,index%folder->frames+1);
          tex_obter_passageira(frame,480);
        }
        if(seqTex)tex=seqTex;
      }
    }
    if (tex) {
      gfx_tex_aspect_atual = gifDesenhando ? 0.0f : tex_aspecto(arte);
      // PASSA O FOCO, como a fileira de cartazes faz. Antes ia 0 fixo: o card
      // de COLECAO era o unico formato que nao clareava, nao ganhava o
      // especular e nao respondia ao foco de jeito nenhum — a queixa do dono
      // de que "tem cards que nao tem as animacoes de foco". A forma continua
      // diferente; a resposta ao foco, nao.
      gfx_rect(card, tex, GFX_CARD, f, 0, 0, raio, 0, 0, 0, 1);
      gfx_tex_aspect_atual = 0;
      // E a profundidade tambem vale aqui, pelo mesmo interruptor dos cartazes.
      desenhaProfundidade(card, raio, ajustes_profundidade_posters());
    }
    // A propria capa e a identidade do catalogo. O nome/logo vinha sendo
    // desenhado novamente por cima dela e criava exatamente a duplicacao que
    // o usuario apontou em Netflix, Prime Video, Disney+ e nas listas IMDb.
    // Titulo de fileira continua no cabecalho; dentro do card fica somente a
    // arte, sem veu, badge ou logo auxiliar.
  }
}

static const char *heroTrailerYoutube(const char *imdb) {
#ifdef __EMSCRIPTEN__
  size_t j, tam;
  if (!extras_hero_trailer_obter(imdb, heroTrailerYoutubeId,
                                 sizeof heroTrailerYoutubeId)) return NULL;
  tam = strlen(heroTrailerYoutubeId);
  if (tam < 6 || tam > 15) return NULL;
  for (j = 0; j < tam; j++)
    if (!((heroTrailerYoutubeId[j] >= 'A' && heroTrailerYoutubeId[j] <= 'Z') ||
          (heroTrailerYoutubeId[j] >= 'a' && heroTrailerYoutubeId[j] <= 'z') ||
          (heroTrailerYoutubeId[j] >= '0' && heroTrailerYoutubeId[j] <= '9') ||
          heroTrailerYoutubeId[j] == '_' || heroTrailerYoutubeId[j] == '-')) return NULL;
  return heroTrailerYoutubeId;
#else
  (void)imdb;
#endif
  return NULL;
}

// O carrossel compartilha o mesmo contrato do pedido de trailer: espera
// enquanto a fonte ainda pode chegar, segura enquanto o elemento prepara e
// nunca segura quando a preferencia foi desligada. `home_atualizar` chama esta
// funcao antes de home_trailer_passo, entao o prazo tambem e o que libera a
// rotacao no quadro em que o trailer falhou.
static int heroTrailerSegurando(Uint32 agora) {
  const CatItem *ci;
  if (!ajustes_trailer_hero() || heroTrailerItem != heroAtual) return 0;
  ci = cat_item_exato(heroAtual);
  if (!ci || strcmp(heroTrailerImdb, ci->imdb) != 0) return 0;
  if (trailer_aberto()) {
    if (trailer_tocando()) return 1;
    if (heroTrailerPreparandoAte &&
        (Sint32)(heroTrailerPreparandoAte - agora) >= 0) return 1;
  }
  return !heroTrailerTentado && heroTrailerDesde &&
         agora - heroTrailerDesde <= NV_TRAILER_HERO_MAX_ESPERA_MS;
}

void home_trailer_passo(int topo, float dt, Uint32 agora) {
  const CatItem *ci = NULL;
  int pronto;
  Uint32 decorrido;
  if (!trailer_suportado()) return;
  pronto = topo && focoHero && ajustes_hero_ligado() && ajustes_trailer_hero() &&
           heroDesejado < 0 && heroAtual >= 0 && heroEntra >= 0.999f && heroSai <= 0.001f &&
           !(foco.fileira >= 0 && foco.fileira < nFileiras &&
             (fileiras[foco.fileira].tipo == FILEIRA_CATALOGOS ||
              fileiras[foco.fileira].tipo == FILEIRA_SOCIAL));
  if (pronto) ci = cat_item_exato(heroAtual);
  if (!pronto || !ci || !ci->imdb[0]) {
    // Hero deixou de estar pronto (foco saiu, transicao da arte, detalhe por
    // cima): fecha. E o outro caminho de fechamento que o prazo nao ve —
    // o emulador mostrou "estado -1 -> -2 +8613ms" sem mais nada.
    if (heroTrailerItem >= 0 && trailer_aberto() && !trailer_cheia()) {
      printf("[trailer] hero: saiu de cena, fecha o trailer %s (estado %d, +%u ms)\n",
             trailer_tocando() ? "tocando" : "sem playing", trailer_estado(),
             heroTrailerDesde ? (unsigned)(agora - heroTrailerDesde) : 0u);
      fflush(stdout);
      trailer_fechar();
    }
    heroTrailerItem = -1; heroTrailerDesde = 0; heroTrailerTentado = 0;
    heroTrailerImdb[0] = 0;
    heroTrailerPreparandoAte = 0; heroTrailerFonte = 0; heroTrailerAppleFalhou = 0;
    heroTrailerFade = 0.0f;
  } else if (heroTrailerItem != heroAtual || strcmp(heroTrailerImdb, ci->imdb) != 0) {
    // A rotacao do carrossel roda ANTES deste passo (home_atualizar): quando
    // o prazo de preparo vence, heroTrailerSegurando solta e o hero troca de
    // titulo no mesmo quadro — o fechamento acontece AQUI, nao no ramo do
    // prazo abaixo. Sem esta linha o registro so mostrava o elemento sumir
    // (emulador, 22/09/2026: "estado -1 -> -2 +5157ms" e nada mais).
    if (trailer_aberto() && !trailer_cheia()) {
      printf("[trailer] hero: troca de titulo fecha o trailer %s (estado %d, +%u ms)\n",
             trailer_tocando() ? "tocando" : "sem playing", trailer_estado(),
             heroTrailerDesde ? (unsigned)(agora - heroTrailerDesde) : 0u);
      fflush(stdout);
      trailer_fechar();
    }
    heroTrailerItem = heroAtual; heroTrailerDesde = agora; heroTrailerTentado = 0;
    snprintf(heroTrailerImdb, sizeof heroTrailerImdb, "%s", ci->imdb);
    heroTrailerPreparandoAte = 0; heroTrailerFonte = 0; heroTrailerAppleFalhou = 0;
    heroTrailerFade = 0.0f;
    trailerapple_pedir(ci->imdb, ci->titulo, ci->meta, ci->tipo[0] ? !strcmp(ci->tipo, "series") : 0);
#ifdef __EMSCRIPTEN__
    // A consulta do hero e so /videos; extras_pedir (ficha, creditos,
    // relacionados e imagens) continua reservado para a pagina de detalhe.
    extras_hero_trailer_pedir(ci->imdb,
                              ci->tipo[0] ? !strcmp(ci->tipo, "series") : 0,
                              ci->tmdb);
#endif
#ifndef __EMSCRIPTEN__
    // O IMDb exige Referer, que navegador nenhum deixa por (e o CORS dele so
    // aceita imdb.com): na Samsung nem pedir.
    trailerimdb_pedir(ci->imdb);
#endif
  } else {
    decorrido = agora - heroTrailerDesde;
#ifdef __EMSCRIPTEN__
    // O worker tem uma janela de retry própria. Chamar aqui é barato e
    // idempotente enquanto a mesma fonte ainda pode chegar; depois de uma
    // falha transitória, o cooldown deixa a mesma obra tentar de novo sem
    // depender de uma troca de destaque.
    if (!heroTrailerTentado)
      extras_hero_trailer_pedir(ci->imdb,
                                ci->tipo[0] ? !strcmp(ci->tipo, "series") : 0,
                                ci->tmdb);
#endif
    // Um elemento que ficou em buffering nao pode congelar a home. Fechar
    // aqui tambem invalida a fonte antiga antes de a proxima arte entrar.
    if (trailer_aberto() && !trailer_tocando() && heroTrailerPreparandoAte &&
        (Sint32)(agora - heroTrailerPreparandoAte) >= 0) {
      printf("[trailer] hero: sem playing em %d ms (%s, estado %d), %s\n",
             NV_TRAILER_HERO_PREPARA_MS, heroTrailerFonte == 1 ? "apple" : "youtube",
             trailer_estado(), heroTrailerFonte == 1 ? "tenta YouTube" : "desiste");
      fflush(stdout);
      trailer_fechar();
      heroTrailerPreparandoAte = 0;
      heroTrailerFade = 0.0f;
      if (heroTrailerFonte == 1) {
        heroTrailerAppleFalhou = 1;
        heroTrailerTentado = 0; // a proxima tentativa pode ser YouTube
      } else {
        heroTrailerTentado = 1;
        heroTrailerFonte = 3;
      }
    }
  }
  // trailer_atualizar fecha o elemento que recebeu erro depois deste passo;
  // consumir a marca no quadro seguinte transforma somente erro de Apple em
  // fallback. Um fechamento normal (ended/Voltar) nunca cai no YouTube.
  if (heroTrailerFonte == 1 && !trailer_aberto() && trailer_falhou() && !heroTrailerAppleFalhou) {
    printf("[trailer] hero: apple deu erro, tenta YouTube\n");
    fflush(stdout);
    heroTrailerAppleFalhou = 1;
    heroTrailerTentado = 0;
    heroTrailerPreparandoAte = 0;
    heroTrailerFade = 0.0f;
  }
  decorrido = agora - heroTrailerDesde;
  if (pronto && ci && ci->imdb[0] && heroTrailerItem == heroAtual &&
      !trailer_aberto() && !heroTrailerTentado &&
      agora - heroTrailerDesde >= NV_TRAILER_HERO_ESPERA_MS) {
    // Apple (HLS matted) antes do IMDb (MP4 com tarja), e so depois de a Apple
    // ter a primeira janela para responder. Na Samsung, se a Apple nao veio,
    // cai no primeiro trailer YouTube valido que a busca enxuta do TMDB trouxe.
    const char *u = heroTrailerAppleFalhou ? NULL : trailerapple_url(ci->imdb);
    const char *yt = NULL;
    int ehYoutube = 0;
    // A APPLE TEM A JANELA INTEIRA (ate NV_TRAILER_HERO_MAX_ESPERA_MS) antes
    // do YouTube, tambem na Samsung. Antes, com um id do YouTube em maos, o
    // hero o abria ja aos 1,2 s se a Apple ainda nao tinha respondido — e a
    // resposta da Apple agora inclui baixar o master para escolher a variante
    // (trailerapple.c, varianteMidia), ~0,3 s a mais. No emulador isso deu
    // YouTube aos 90,1 s e a Apple pronta aos 90,4 s; o embed do YouTube nao
    // produziu `playing` em 3,5 s (na TV ele cai em "Video player
    // configuration error", #82/#86), e o trailer bom ficou de fora.
    if (!u && !trailerapple_respondeu(ci->imdb) &&
        decorrido < NV_TRAILER_HERO_MAX_ESPERA_MS)
      goto trailer_hero_fim;
#ifdef __EMSCRIPTEN__
    if (!u) {
      yt = heroTrailerYoutube(ci->imdb);
      if (yt) { u = yt; ehYoutube = 1; }
    }
#endif
#ifndef __EMSCRIPTEN__
    if (!u) u = trailerimdb_url(ci->imdb, NULL);
#endif
    if (!u && decorrido < NV_TRAILER_HERO_MAX_ESPERA_MS) goto trailer_hero_fim;
    if (u) {
      heroTrailerTentado = 1;
      heroTrailerFonte = ehYoutube ? 2 : 1;
      heroTrailerPreparandoAte = heroTrailerPrazoPreparacao(agora);
      trailer_abrir(u, heroArteRect, 0, 0);
      // trailer_abrir e void por compatibilidade com o player nativo; no
      // browser ainda pode recusar a criacao (canvas ausente). Tratar isso
      // como erro da fonte evita deixar a tentativa marcada para sempre.
      if (!trailer_aberto()) {
        heroTrailerPreparandoAte = 0;
        if (ehYoutube) {
          heroTrailerTentado = 1;
          heroTrailerFonte = 3;
        } else {
          heroTrailerAppleFalhou = 1;
          heroTrailerTentado = 0;
          heroTrailerFade = 0.0f;
        }
      }
    } else {
      // Sem fonte depois do orçamento, deixa a arte e o carrossel seguirem.
      printf("[trailer] hero: sem fonte em %u ms (apple %s), fica a arte\n", decorrido,
             heroTrailerAppleFalhou ? "falhou" : trailerapple_respondeu(ci->imdb) ? "sem trailer" : "sem resposta");
      fflush(stdout);
      heroTrailerTentado = 1;
      heroTrailerFonte = 3;
      heroTrailerFade = 0.0f;
    }
  }
trailer_hero_fim:
  { float alvo = (heroTrailerItem >= 0 && heroTrailerItem == heroAtual &&
                  trailer_aberto() && !trailer_cheia() && trailer_tocando()) ? 1.0f : 0.0f;
    heroTrailerFade = anim_mola(heroTrailerFade, alvo, dt, NV_MOLA_SCROLL); }
}

void home_desenhar(Uint32 agora) {
  // O REBORDO DO CARTAZ EM FOCO e ajuste da pessoa, e ele mora no shader do
  // GFX_CARD (nao e um retangulo desenhado por cima): por isso vai por uma
  // variavel de modulo, uma vez por quadro, e nao em cada chamada.
  gfx_borda_foco_atual = ajustes_borda_foco() ? 1.0f : 0.0f;
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
      if (foco.fileira == r && !focoHero) {
        char pos[32];
        if (foco.coluna < fileiras[r].n)
          snprintf(pos, sizeof pos, "%d / %d", foco.coluna + 1, fileiras[r].n);
        else snprintf(pos, sizeof pos, "Ver tudo");
        // PILULA ESCURA ATRAS DO CONTADOR. A fileira de baixo corre sobre a arte
        // do destaque, e cinza 186 direto sobre um fundo claro sumia — o "See
        // all" do fim da fileira virou um "all" solto na C9 (22/09, arte clara
        // de One Night Only). O veu so escurece a esquerda, onde mora o titulo;
        // a direita a arte chega quase crua.
        char posTr[32];
        snprintf(posTr, sizeof posTr, "%s", foco.coluna < fileiras[r].n ? pos : i18n(pos));
        TxtLinha lp = txt_linha(TXT_HERO_META, posTr, 238, 240, 245, 255);
        float px = NV_TELA_W - NV_HOME_SAFE_RIGHT - lp.w, py = y + (tl.h - lp.h)*.5f;
        gfx_cor((GfxRect){ px - 16.0f, py - 6.0f, lp.w + 32.0f, lp.h + 12.0f }, 0.5f,
                0.04f, 0.045f, 0.055f, 0.62f);
        txt_desenhar(lp, px, py);
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
          // O foco pode aumentar o card. No 4:3 ele nao pode subir sobre o
          // titulo da fileira; a expansão acontece para baixo, preservando a
          // separação visual da referência.
          if (tipo == FILEIRA_DESTAQUE_QUADRADO && py < cardY) py = cardY;
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
          // O empurrao e medido no card aberto COM a escala de foco dele, nao
          // no de escala 1: o aberto mede artH*escF*16/9 e o empurrao contava
          // artH*16/9 — faltavam (escF-1)*artH*16/9 ≈ 40 px, e o vizinho da
          // direita ficava por baixo do aberto ("alguns cards quando abrem
          // ficam por cima do outro", 20/09/2026). Com o vizinho empurrado
          // pela medida certa sobra so a folga normal de um card focado.
          if (r == expFileira && expAbre > 0.0f && c > expColuna) {
            float escF = 1.0f + escalaDe(tipo) * animFoco[r][expColuna];
            empurra = (artH * escF * NV_EXP_ASPECTO - lw * escF) * expAbre;
          }
          if (abre > 0.0f) w = lw * esc + (larguraAberta - lw * esc) * abre;
          float cx = ajustes_conteudo_x() + c * passo - scrollX[r] + lw * 0.5f
                   + empurra + (w - lw * esc) * 0.5f;
          // Sem levantamento: no web o card focado nao sai do lugar.
          float cy = cardY + artH * 0.5f;
          if (cx < -lw * 1.5f || cx > NV_TELA_W + lw) continue;
          float px = cx - w * 0.5f, py = cy - h * 0.5f;
          // O foco pode aumentar o card. No 4:3 ele nao pode subir sobre o
          // titulo da fileira; a expansão acontece para baixo, preservando a
          // separação visual da referência.
          if (tipo == FILEIRA_DESTAQUE_QUADRADO && py < cardY) py = cardY;

          if (passe == 0) {
            // Sem sombra. Ela existia para separar o card do fundo, mas sobre
            // arte colorida vira um halo escuro em volta do item focado — e o
            // aparelho nao tem isso: la o foco se marca por escala e brilho.
            (void)f;
            continue;
          }

          const int idxCat = fileiraItemIndice(&fileiras[r], c);
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
            TxtLinha acao=txt_linha_corta(TXT_MINI,cItem->socialAcao[0]?i18n(cItem->socialAcao):cItem->provNome,181,185,196,255,tw);
            txt_desenhar(acao,tx,conteudoTopo+38.0f);
            TxtLinha titulo=txt_linha_corta(TXT_CW_META,cItem->titulo,228,231,239,255,tw);
            txt_desenhar(titulo,tx,conteudoTopo+92.0f);
            TxtLinha ep=txt_linha_corta(TXT_MINI,cItem->temporada?cItem->direcao:i18n("Filme"),181,185,196,255,tw);
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

          // `!focoHero` E A CONDICAO QUE FALTAVA AQUI, e e o mesmo defeito que
          // o anel de foco ja tinha resolvido dez linhas acima (linha 1747).
          //
          // Com o destaque focado, `foco` continua apontando para a fileira 0,
          // coluna 0 — ele nao e zerado, para que descer devolva a pessoa ao
          // lugar de onde ela saiu. Este bloco roda DEPOIS do destaque no mesmo
          // quadro (cards sao desenhados abaixo dele), entao ele sobrescrevia
          // o itemFoco que o destaque tinha acabado de preencher.
          //
          // Efeito relatado pelo dono: "o hero sempre seleciona o primeiro
          // filme, nao importa qual apareca". Era literal — o OK abria sempre
          // o primeiro card da primeira fileira, porque foi ele o ultimo a
          // escrever em itemFoco antes de app.c ler.
          if (!focoHero && focus_indice(&foco, r, c)) {
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
          if (f > 0.01f && ajustes_borda_foco()) {
            GfxRect borda = { px - NV_ANEL_FOCO, py - NV_ANEL_FOCO,
                              w + NV_ANEL_FOCO * 2, h + NV_ANEL_FOCO * 2 };
            float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
            // RAIO DE FORA = raio do cartaz + espessura do anel, em pixels,
            // normalizado pelo lado menor DA BORDA. Passar o `raio` do cartaz
            // direto dava um canto de fora mais fechado que o de dentro — as
            // "pontas feias" da foto do dono (21/09/2026); e a conta que a
            // fileira de colecoes ja fazia.
            { float menor = w < h ? w : h;
              gfx_cor(borda, (raio * menor + NV_ANEL_FOCO) / (menor + 2 * NV_ANEL_FOCO),
                      ar, ag, ab, f); }
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
            // A variante 4:3 deve ocupar a moldura inteira. GFX_CARD tem um
            // fallback contain para posters servidos por catálogos deitados;
            // aqui a forma já foi escolhida como editorial, então o recorte
            // cover é intencional e fica limitado a esta opção.
            gfx_card_forcar_cover_atual = tipo == FILEIRA_DESTAQUE_QUADRADO ? 1.0f : 0.0f;
            gfx_tex_aspect_atual = tex_aspecto(caminho);
            gfx_rect(card, t, GFX_CARD, f, 0.0f, 0.0f,
                     raio, 0, 0, 0, 1);
            gfx_tex_aspect_atual = 0.0f;
            gfx_card_forcar_cover_atual = 0.0f;
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
          //
          // MEDIDO no web (.title-watched-badge, components.css:4744): disco de
          // 34 px a 14 px do canto, fundo na COR DE REALCE (--secondary-color),
          // icone de 28 px em on-secondary. Tamanho FIXO, nao proporcional ao
          // card: era `w * 0.16`, e no card aberto (w ~730) virava um disco de
          // 117 px — a "badge" que o dono fotografou e perguntou o que era.
          //
          // O "v" e o icone check.png (art/icones, com o .svg ao lado), nao
          // dois retangulos: gfx_rect nao gira, e os dois tracos horizontais
          // liam como um traco "—", nao como um check. Nao e o visto.png: esse
          // e um OLHO, o do botao "marcar como visto" do detalhe, e no disco
          // de 34 px virava um olho sobre o poster.
          if (cItem && cItem->progresso >= 90 && tipo != FILEIRA_CONTINUE) {
            float d = 34.0f;
            float mx = px + w - d - 14.0f, my = py + 14.0f;
            GfxRect disco = { mx, my, d, d };
            float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
            // Sombra rasa (box-shadow 0 14px 24px .3 na referencia): separa o
            // disco claro de um poster claro sem virar halo.
            { GfxRect sombra = { mx, my + 3.0f, d, d };
              gfx_cor(sombra, 0.5f, 0, 0, 0, 0.28f); }
            gfx_cor(disco, 0.5f, ar, ag, ab, 1.0f);
            { float ic = 22.0f;
              GfxRect g = { mx + (d - ic) * 0.5f, my + (d - ic) * 0.5f, ic, ic };
              gfx_icone(g, "check", 0.08f, 0.08f, 0.09f, 1.0f); }
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
          // FAIXA DE DECISAO no card aberto (pedido do dono, 20/09/2026:
          // "quando abrir o card, mais informacoes — nota, se subiu ou caiu
          // no trending, coisas uteis para tomada de decisao"). Canto INFERIOR
          // DIREITO, oposto ao logo, sobre o mesmo veu: selo IMDb + nota,
          // ano/temporadas, classificacao e a VARIACAO na fileira desde a
          // ultima visita (tendencia.h): ↑n verde, ↓n vermelho, "Novo" na cor
          // de realce. Sem historico ainda, sem chip — nada de inventar.
          // Progresso em andamento vira um fio na base do card.
          if (abre > 0.01f && cItem) desenhaFaixaAberta(cItem, r, px, py, w, h, esc, abre);
          if (abre > 0.01f && cItem && cItem->logo[0]) {
            // Largura pedida pela tela, nao o teto generico de 640: o logo
            // nunca passa de ~65% do card, e decodificar o arquivo inteiro
            // so para encolher depois era cache e tempo jogados fora.
            const char *urlL = artehero_logo_sessao_larg(cItem, w * 0.65f);
            GLuint tl = tex_obter_larg(urlL, w * 0.65f);
            if (tl) {
              float pad = 34.0f * esc;
              float ap = tex_aspecto(urlL);
              float hL, wL, maxW;
              // O veu ja saiu em desenhaFaixaAberta, o mesmo para o logo e
              // para a faixa.
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
                GfxModo m = tex_marca_escura(urlL) ? GFX_MARCA : GFX_TEXTO;
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
            const char *urlCl = ci ? artehero_logo_sessao_larg(ci, w * .65f) : NULL;
            GLuint tlogo = urlCl ? tex_obter_larg(urlCl, w * .65f) : 0;
            // Sem dado, sem texto — nao a lista de demonstracao que ficava
            // aqui e carimbava nome e genero de outro titulo no card.
            const char *nome   = (ci && ci->titulo[0]) ? ci->titulo : NULL;
            const char *genero = (ci && ci->genero[0]) ? ci->genero
                                : ci ? i18n(!strcmp(ci->tipo, "series") ? "Série" : "Filme") : NULL;
            TxtLinha tg = genero
                        ? txt_linha_corta(TXT_HERO_META, genero, 226, 228, 233, 255, w - 64)
                        : (TxtLinha){ 0, 0, 0 };

            float pad = (tipo == FILEIRA_DESTAQUE || tipo == FILEIRA_DESTAQUE_QUADRADO)
                      ? 28.0f : 22.0f;
            float base = py + h - pad;
            float yMeta = base - tg.h;
            float hTit;
            if (tlogo) {
              float ap = tex_aspecto(urlCl);
              if (ap <= 0.0f) ap = 4.0f;
              hTit = h * .22f;
              float wTit = hTit * ap, maxW = w * .65f;
              if (wTit > maxW) { wTit = maxW; hTit = wTit / ap; }
              GfxRect rl = { px + pad, yMeta - hTit - 10.0f, wTit, hTit };
              gfx_tex_aspect_atual = 0.0f;
              { GfxModo m = tex_marca_escura(urlCl) ? GFX_MARCA : GFX_TEXTO;
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
            if (ci && ci->classificacao[0] && tg.w + BADGE_H + 24.0f < w - pad*2) {
              char clas[8];
              snprintf(clas, sizeof clas, "%s%s", ci->classificacao[0] == 'A' ? "" : "A", ci->classificacao);
              { float bx = px + pad + tg.w + (genero ? 14.0f : 0.0f);
                badge_desenhar(bx, yMeta + (tg.h - BADGE_H) * 0.5f, clas,
                               BADGE_NEUTRO, 0.95f); }
            }
          }

          // Feedback progressivo do gesto, sem duplicar o menu contextual. A
          // barra aparece somente enquanto o mesmo item esta sob pressao;
          // atingido o limiar, ctxmenu ja foi aberto e a soltura e consumida.
          // Mesma guarda: segurar o OK no destaque desenhava a barra de
          // progresso no primeiro card, que nao e o item sob pressao.
          if (!focoHero && okPressionando && okHold > 0.0f &&
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
  // DEVOLVE O REBORDO ao sair: a variavel e global e o detalhe, a busca e a
  // biblioteca desenham GFX_CARD tambem. O ajuste e "na Home", entao ele nao
  // pode vazar para as outras telas — mesma disciplina de gfx_opacidade_grupo
  // logo acima.
  gfx_borda_foco_atual = 1.0f;
  gfx_sem_recorte();
}

// Rede de seguranca, nao o gatilho principal. Numa TV o app raramente sai por
// aqui: a tecla Home do controle mata o processo e este caminho nao roda. Por
// isso a gravacao de verdade e a de repouso, em home_atualizar; esta so pega o
// caso em que a pessoa moveu o foco e saiu antes de completar o repouso.
// Nada a gravar (#95): a posicao na home vive so enquanto o app esta aberto.
void home_encerrar(void) { }
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
int home_pediu_tocar(void) { int v = pedidoTocar; pedidoTocar = 0; return v; }

// Consome o pedido de abrir o menu lateral: quem le, zera.
int home_pediu_menu(void) { int v = pedidoMenu; pedidoMenu = 0; return v; }
