#include "imdbnota.h"
// Home nativa compatível com a interface moderna do Nuvio 1.0.1 legacy:
// hero no topo, rail fixa à esquerda e fileiras horizontais de posters. A
// infraestrutura nativa cuida de cache assíncrono, foco e transições.
#include "home.h"
#include "botoes.h"
#include "cwretido.h"
#include "focoprof.h"
#include "posterprov.h"
#include "corviva.h"
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
#include "homeestado.h"
#include "cachearte.h"
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
#include "revela.h"
#include "layout.h"
#include "ajustes.h"
#include "cwordem.h"
#include "catalogo.h"
#include "artehero.h"
#include "colecoes.h"
#include "colfileiras.h"
#include "addons.h"   /* addons_nome_por_id: o addon de um grupo de colecoes */
#include "gif.h"
#include "gifcolecao.h"
#include "badges.h"
#include "selospacote.h"
#include "svdesenho.h"
#include "amigostitulo.h"
#include "extras.h"
#include "diretor.h"
#include "descoberta.h"
#include "dados.h"
#include "tendencia.h"
#include <strings.h>
// Os veus da base de cada forma de card (gfx_veu_base): a fracao da altura que
// o degrade cobre e o alfa na base. Nomeados porque o laco da fileira os poe
// DENTRO da arte (veusDoCard) e quem desenha a legenda tem de pedir o mesmo.
#define NV_EDITORIAL_VEU_F 0.62f
#define NV_EDITORIAL_VEU_A 0.90f
#define NV_ABERTA_VEU_F    0.55f
#define NV_ABERTA_VEU_A    0.72f
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
#include <time.h>
#include "trailer.h"
#include "trailercinema.h"
#include "trailerimdb.h"
#include "trailerapple.h"
#include "trailerfonte.h"
#include "ponteiro.h"
#include "amigosfil.h"
#include "socialvis.h"

#define MAX_ARTE   64
// Era 32 para as 16 fileiras (CAT_FIL_MAX) do web neste runtime, mais o que a
// home acrescenta por conta propria (grupos de colecao, social, retomada). Com
// CAT_FIL_MAX em 40 (o HOME_MAX_ROWS_DEFAULT do web) o dobro seria 80, mas os
// acrescimos nao crescem junto: 64 sobra para 40 + colecoes + social + retomada.
// Custo: as tabelas por fileira aqui somam ~200 KB estaticos (Fileira pesa
// 1,3 KB); nada disto mora na pilha.
#define MAX_FIL    64
_Static_assert(MAX_FIL >= CAT_FIL_MAX + 16, "a home precisa de folga sobre o teto do catalogo");
// 13 e nao 12: sao 12 CARTAZES mais a coluna do card "Ver tudo", que ocupa a
// posicao seguinte a ultima arte. Com 12 aqui, animFoco[r][12] escrevia fora do
// vetor — o card nunca acendia ao receber foco e a memoria do vizinho era
// corrompida em silencio.
#define MAX_CARDS 33
_Static_assert(DESC_ITENS_POR_FILEIRA <= MAX_CARDS - 1, "itens por fileira + Ver tudo cabem em MAX_CARDS");
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
  char base[NV_ADDON_URL_MAX], catId[96];   // a de CatFileira (addonurl.h)
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
  // O fator que a FORMA trouxe para dentro de `escala` (fil_tipo_fator): 1 em
  // tudo, 1,25/1,5 nos Destaques 4:3 maiores. Guardado a parte porque so ele
  // tem teto por layout (limita43) — o Tamanho de sempre nao muda de medida.
  float fator;
  // FORMA DO CARTAO numa fileira de colecao (COL_FORMA_*, colecoes.h): a do
  // grupo na conta (tileShape), ou a que a pessoa escolheu por cima. So vale
  // com tipo == FILEIRA_CATALOGOS; o tipo continua sendo o que diz "isto e um
  // grupo de atalhos" para o resto da home.
  int forma;
  // A FORMA DE AUTOMATICO, guardada antes de a escolha da pessoa valer
  // (sincronizarFileiras). E o que a previa do modal de estilo desenha na linha
  // "Automatico" quando a fileira ja tem outra forma escolhida — sem ela, a
  // previa so saberia mostrar a forma atual.
  TipoFileira tipoAuto;
  int formaAuto;
} Fileira;

static int fileiraItemIndice(const Fileira *f, int coluna) {
  if (!f || coluna < 0 || coluna >= f->n) return -1;
  // A FILEIRA DE AMIGOS (amigosfil.h): as colunas sao ROSTOS, e nao itens do
  // catalogo. O item dela e o cartao em foco (ou o titulo mais novo do rosto),
  // que e o que o destaque e a memoria de posicao precisam.
  if (f->tipo == FILEIRA_SOCIAL) return amigosfil_indice_cat(coluna);
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
// MICRO-ANIMACOES DE CARD (revela.h). Um registro por lugar da grade para a
// arte chegando; um so para a luz do foco, porque so ha um card em foco; e o
// instante em que cada fileira comecou a entrar (0 = ja entrou).
static RevelaArte  revArte[MAX_FIL][MAX_CARDS];
static RevelaVarre revVarre = { -1, 0, 0 };
static Uint32      filEntraEm[MAX_FIL];
static int         filNAntes[MAX_FIL];
// Ultimo quadro em que as fileiras foram desenhadas. Uma ausencia longa (a
// home escondida atras do player, de Ajustes, da selecao de perfil) faz as
// fileiras visiveis entrarem de novo, em cascata, na volta.
static Uint32      fileirasVistasEm;
// Primeira vez que um card de fileira foi PINTADO (ver o marcador em
// home_desenhar): -1 enquanto nao aconteceu. Nao e o mesmo que
// fileirasVistasEm — aquele e o ultimo quadro, este e o PRIMEIRO e so uma
// vez por sessao, para o par publicar -> pintar do arranque.
static int          tPrimeiraFileiraPintada = -1;
static float scrollX[MAX_FIL];
static float scrollY = 0.0f;
// Pastas da fileira "Streaming" que o layout Dinamica levou para a barra.
static int streamBarra[MAX_CARDS], nStreamBarra;
// Velocidades das molas de 2a ordem do deslize. Ficam ao lado da posicao
// porque anim_mola2() precisa das duas. Ver anim.h.
static float velX[MAX_FIL];
// Fileiras que o limite cortou na ultima montagem — colecoes incluidas. Ver o
// comentario no corte, em remontar().
static int   cortadasPeloLimite;
static float velY = 0.0f;
// RETORNO DE FIM DE FILEIRA (anim.h: AnimBorda). Um deslocamento em x por
// fileira e um em y para a pagina, somados so no DESENHO: nao tocam scrollX,
// scrollY nem o foco, e em repouso valem zero.
#define NV_BORDA_AMP 20.0f
static AnimBorda bordaFil[MAX_FIL];
static AnimBorda bordaPag;
static float bordaX(int r) { return (r >= 0 && r < MAX_FIL) ? bordaFil[r].x : 0.0f; }
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
// PRE-BUSCA DO PROXIMO DO CARROSSEL (07/10, TCL Android 2.0.2). O carrossel
// sabe quem vem a seguir ~7 s antes, mas so pedia a arte NO INSTANTE da troca —
// download (500-1300 ms) mais decode contra um prazo de 600 ms: o log mostrava
// `ESTOUROU, sem arte` em toda primeira volta e `arte atrasada chegou em
// 1400-1700 ms`. Nos ultimos NV_HERO_PRE_MS antes da troca o proximo e pedido
// como destaque (arte de 1920 e logo), e a troca encontra tudo pronto. O custo
// e UMA arte de tela a mais por ate NV_HERO_PRE_MS — o mesmo par atual+proximo
// que a mistura da troca ja segura — e nenhuma qualidade a menos: o pedido e
// o mesmo tex_obter_hero da troca, no mesmo teto.
// -1 = nada a pre-buscar. Escrito no passo (home_atualizar), lido no desenho.
#define NV_HERO_PRE_MS 3000
static int heroPreItem = -1;
// O item cuja pre-busca ja foi anunciada no log (uma linha por item).
static int heroPreLogado = -1;
// Diagnostico de `pre-busca nao saiu`: assinatura do motivo e instante da
// ultima linha (ver o bloco no passo).
static Uint32 heroPreDiagEm;
static unsigned heroPreDiagSig;
// AQUECIMENTO DOS VIZINHOS +-1 (09/10, C9 2.0.3). A pre-busca acima so serve a
// rotacao AUTOMATICA: tem de estar ligada (!heroAutoDesligado — a seta esquerda
// a desliga) e, com o foco no destaque, sem tecla ha 12 s (ver a condicao no
// passo). Quem vira o hero na mao nunca atende a nenhuma das duas, entao nada era
// aquecido (`pre-busca nao saiu: alvo=-1 desejado=-1 ... focoHero=1`) e 8 de 13
// viradas pagavam resolucao + rede + decode e estouravam os ~610 ms. Com o hero
// no comando (sem card em foco, sem virada em curso) a arte de heroAtual+-1 e
// pedida UMA vez por posicao com tex_obter_hero_quente: mesma arte e mesmo teto
// da virada, mas sem furar a fila de quem esta na tela; a virada a promove.
// heroVizBase = o heroAtual cujos vizinhos ja foram pedidos; bits de heroVizFeito:
// 1 = anterior, 2 = proximo (so marca quando a URL resolveu).
static int heroVizBase = -1;
static int heroVizFeito;
static Uint32 heroVizTentaEm;

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
// TROCA DESLIZADA (dono, 30/09: "a hero passando animado, tipo trocando a
// imagem pro lado e a outra bem grudada nela"; ajuste "Transicao do destaque").
// No lugar do esvanecimento, a arte e o texto do titulo que sai andam para um
// lado e os do que entra vem colados atras, sem vao, como um carrossel.
// `heroDesliza`    progresso 0..1 do deslize; 1 = parado (nada a mais em repouso).
// `heroDeslizaDir` +1 = o proximo entra pela direita; -1 = o anterior, pela
//                  esquerda.
// `heroDirDesejado` a direcao que acompanha heroDesejado. So a seta no destaque
//                  e o carrossel automatico dizem uma; o hero que segue o
//                  cartaz das fileiras deixa 0 e troca como sempre trocou.
// Cubica de saida (anim_saida), sem mola: nao ha repique a assentar.
#define NV_HERO_DESLIZA_MS 520.0f
static float heroDesliza = 1.0f;
static int   heroDeslizaDir = 0, heroDirDesejado = 0;
// TRAILER NO DESTAQUE (trailer.h; dono, 20/09/2026: "coloca para tocar no
// hero tb"). Com o foco parado no hero e a arte assentada, espera o ajuste
// "Espera do trailer no destaque" (heroTrailerEspera) e troca a arte pelo
// trailer do titulo, mudo salvo com "Som do trailer no destaque", no mesmo
// retangulo. Mover o foco, sair da home ou abrir qualquer coisa por
// cima (app.c diz, por `topo`) volta para a arte. Uma tentativa por titulo
// por parada de foco: o trailer que acabou nao recomeca.
static Uint32 heroTrailerDesde = 0;
static int    heroTrailerItem = -1, heroTrailerTentado = 0;
static char   heroTrailerImdb[24];
static Uint32 heroTrailerPreparandoAte = 0;
// 0 = ainda sem fonte, 1 = Apple, 2 = outra fonte, 3 = falha final.
// Falhas seguem a ordem real do destaque; nenhuma URL falhada reabre nesta
// parada de foco, inclusive IMDb -> Apple no TPK.
static int    heroTrailerFonte = 0, heroTrailerAppleFalhou = 0;
// TRF_* da fonte aberta, usado pela transicao e pelo diagnostico.
static int    heroTrailerQual = 0;
/* Per-focus attempt: a failed source can never reopen while waiting for its
 * next source. On native TPK the real Home order is IMDb -> Apple (#228). */
static unsigned heroTrailerFalhas;
static float  heroTrailerFade = 0.0f;
// MODO CINEMA no destaque (dono, 29/09/2026: "quando o trailer comecar no hero,
// deixar ele igual a quando ta no details do titulo: so a arte do titulo
// embaixo e passando o trailer, e voltar ao normal quando acabar"). A conta e
// a do detalhe (trailercinema.h). So vale para o trailer do DESTAQUE: no do
// cartaz em foco as fileiras sao o assunto, esconde-las apagaria o cartaz.
static TrailerCinema heroCinema;
// UM TRAILER POR TITULO NA SESSAO (dono, 30/09: "toca uma vez e pronto"). O
// hero abria o trailer de novo toda vez que o foco voltava ao mesmo titulo, para
// sempre. Agora, depois que o trailer de um titulo TOCOU (o `playing` chegou),
// o titulo entra neste conjunto e nao toca sozinho outra vez: a volta mostra a
// arte e o logo parados. Fica so na memoria — reabrir o app zera. Cresce conforme
// necessario; em falha de alocacao, bloqueia novos autoplays em vez de esquecer
// titulo ja tocado e viola a regra por sessao.
static char (*heroTrailerTocou)[24];
static size_t heroTrailerTocouN, heroTrailerTocouCap;
static int heroTrailerMemoriaFalhou;
static int heroTrailerJaTocou(const char *imdb) {
  size_t i;
  if (!imdb || !imdb[0]) return 0;
  for (i = 0; i < heroTrailerTocouN; i++)
    if (!strcmp(heroTrailerTocou[i], imdb)) return 1;
  return 0;
}
static void heroTrailerMarcarTocou(const char *imdb) {
  char (*novo)[24];
  size_t cap;
  if (!imdb || !imdb[0] || heroTrailerJaTocou(imdb) || heroTrailerMemoriaFalhou) return;
  if (heroTrailerTocouN == heroTrailerTocouCap) {
    cap = heroTrailerTocouCap ? heroTrailerTocouCap * 2 : 32;
    novo = realloc(heroTrailerTocou, cap * sizeof *novo);
    if (!novo) { heroTrailerMemoriaFalhou = 1; return; }
    heroTrailerTocou = novo;
    heroTrailerTocouCap = cap;
  }
  snprintf(heroTrailerTocou[heroTrailerTocouN], sizeof heroTrailerTocou[0], "%s", imdb);
  heroTrailerTocouN++;
}
// ROTACAO AUTOMATICA DO DESTAQUE. Ela roda so quando TODAS valem (home_atualizar):
//   1. nenhum detalhe/player por cima (heroOculto: o relogio e rearmado na volta);
//   2. o foco NAO esta num cartaz das fileiras (ali o hero segue o cartaz);
//   3. o intervalo de NV_HERO_INTERVALO_MS (7 s) passou — toda troca manual, pela
//      seta ou por ponteiro, o reinicia;
//   4. com o foco no destaque, a ultima tecla foi ha HOME_HERO_OCIO_MS (12 s);
//   5. nenhum trailer tocando, preparando ou ainda dentro da janela da fonte
//      (heroTrailerSegurando);
//   6. NOVO: a pessoa nao VOLTOU um titulo no destaque (esquerda). Voltar
//      significa navegar a mao; a rotacao fica desligada ate a proxima visita a
//      home (heroAutoDesligado, zerado quando as fileiras voltam a aparecer
//      depois de uma ausencia). Avancar continua so reiniciando os relogios 3 e 4.
static int heroAutoDesligado;
static char   heroTrailerYoutubeId[16];
static int heroTrailerSegurando(Uint32 agora);
static int heroTrailerTocando(void);
// A espera e ajuste; a janela da Apple (NV_TRAILER_HERO_JANELA_MS) conta a
// partir dela, senao uma espera longa venceria a janela antes de abrir.
static Uint32 heroTrailerEspera(void) { return ajustes_trailer_hero_espera_ms(); }
static Uint32 heroTrailerMaxEspera(void) { return heroTrailerEspera() + NV_TRAILER_HERO_JANELA_MS; }

static Uint32 heroTrailerPrazoPreparacao(Uint32 agora) {
  // A janela de heroTrailerMaxEspera resolve a primeira fonte; depois de escolher Apple ou
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

// --- LAYOUT DA HOME (Ajustes > Layout > Layout da home) -----------------------
//
// Tres desenhos da MESMA home: o mesmo foco, as mesmas fileiras, as mesmas
// molas. O que muda e onde as coisas ficam (estas funcoes), qual e a arte do
// destaque e como ele se comporta (desenhaHero), e o fundo por baixo das
// fileiras (home_desenhar). MODERNA devolve exatamente as constantes que este
// arquivo sempre usou: tests/homelayouts_shot.sh compara o quadro byte a byte.
static int layoutHome(void) { return ajustes_home_layout(); }

// Onde a fileira EM FOCO ancora o titulo dela. E tambem a referencia do
// esvanecer das de cima (o `fade` de home_desenhar).
static float topoFileiras(void) {
  int heroOn = ajustes_hero_ligado();
  switch (layoutHome()) {
    case HOME_LAYOUT_PADRAO:
      return heroOn ? NV_PAD_TOPO_FIL : NV_DIN_TOPO_FIL - 10.0f;
    case HOME_LAYOUT_DINAMICA: return NV_DIN_TOPO_FIL;
    default:                   return NV_SHELF_TOP;
  }
}
// Acima disto nada de fileira se desenha (o gfx_recorte): a Moderna deixa as
// fileiras subirem 96 px por cima da arte; o Padrao corta rente ao banner, para
// a fileira que sai NAO passar por cima dele; a Dinamica nao corta — a fileira
// some pelo esvanecer e o destaque ja subiu.
static float corteFileiras(void) {
  switch (layoutHome()) {
    case HOME_LAYOUT_PADRAO:
      return ajustes_hero_ligado() ? NV_PAD_BANNER_Y + NV_PAD_BANNER_H + 8.0f : 0.0f;
    case HOME_LAYOUT_DINAMICA: return 0.0f;
    default:                   return NV_SHELF_TOP - 96.0f;
  }
}
// Quanto as fileiras DESCEM com o foco no destaque (a rolagem vale menos isto).
// Padrao: nada — o banner ja mostra o destaque inteiro e a fileira 0 fica onde
// esta. Dinamica: a distancia entre "primeira fileira espiando" e "fileira em
// foco", que e o quanto o destaque sobe ao descer.
static float empurraHero(void) {
  int heroOn = ajustes_hero_ligado();
  switch (layoutHome()) {
    case HOME_LAYOUT_PADRAO:   return 0.0f;
    case HOME_LAYOUT_DINAMICA: return heroOn ? NV_DIN_REPOUSO_FIL - NV_DIN_TOPO_FIL : 0.0f;
    default:                   return NV_HOME_HERO_EMPURRA;
  }
}
// Rolagem em que o destaque da Dinamica esta TODO na tela: y do topo dele.
// Segue a mola das fileiras (scrollY) — um so relogio para o gesto inteiro.
static float dinHeroY(void) {
  float y = -(scrollY + empurraHero());
  return y > 0.0f ? 0.0f : y;
}
static GfxRect padBannerRect(void) {
  return (GfxRect){ 0.0f, NV_PAD_BANNER_Y, NV_TELA_W, NV_PAD_BANNER_H };
}
// O numeral do Top 10 mora ANTES do cartaz: a fileira comeca deslocada por esta
// faixa (e o passo dela ja a inclui, em gapDe).
static float xOffTipo(TipoFileira t) {
  return t == FILEIRA_TOP10_NUM ? NV_TOP10_NUM_FAIXA : 0.0f;
}

// O CARTAZ EM PE DO PADRAO (decisao do dono, 29/09): 260x390 no padrao de
// fabrica, o tamanho da home original do Nuvio. "Largura do item" segue sendo
// um FATOR relativo aos 126 dp de fabrica, como nos outros layouts — so a base
// muda. Assim a escolha da pessoa nunca inverte (subir o numero sempre aumenta
// o cartaz) e ninguem que ja mexeu perde o ajuste: 150 dp continuam ~19% maior
// que o de fabrica, so que o de fabrica aqui e 260.
// TETO: a fileira em foco se ancora em topoFileiras(); o cartaz nunca passa da
// altura que cabe entre o titulo dela e a base da tela, com 40 px de folga
// (destaque ligado: 450 px de altura, alcancado em ~145 dp). Acima disso o
// ajuste para de crescer no Padrao, em vez de cortar o cartaz em foco na borda.
static int cartazPadrao(void) {
  return layoutHome() == HOME_LAYOUT_PADRAO && !ajustes_posteres_deitados();
}
static float escalaCartazPadrao(void) {
  float teto = (NV_TELA_H - topoFileiras() - NV_LEGACY_ROW_HEAD_H - NV_PAD_CARTAZ_FOLGA)
             / NV_PAD_CARTAZ_H;
  float e = escalaDoAjuste();
  return e < teto ? e : teto;
}

// AS TRES FORMAS DA PASTA, nas medidas do web (components.css,
// .home-collection-card): PAISAGEM e a deitada compacta que o nativo sempre
// desenhou; QUADRADO tem o lado da altura do cartaz (`flex-basis:
// var(--home-poster-height)`); POSTER e o cartaz em pe das fileiras de
// catalogo. Cartaz sempre 2:3, mesmo com "posteres deitados": a pasta pediu
// pôster.
static void medidaColecao(int forma, float *w, float *h) {
  float pw = cartazPadrao() ? escalaCartazPadrao() * NV_PAD_CARTAZ_W
                            : escalaDoAjuste() * NV_CARD_W;
  float ph = cartazPadrao() ? escalaCartazPadrao() * NV_PAD_CARTAZ_H
                            : escalaDoAjuste() * NV_CARD_H;
  switch (forma) {
    case COL_FORMA_QUADRADO: *w = ph;  *h = ph;  break;
    case COL_FORMA_POSTER:   *w = pw;  *h = ph;  break;
    default:                 *w = 360.0f; *h = 203.0f; break;
  }
}

static float larguraDe(TipoFileira t) {
  switch (t) {
    case FILEIRA_CONTINUE: return NV_DESTAQUE_W;
    // Opção editorial já existente: card panorâmico 16:9.
    case FILEIRA_DESTAQUE: return layoutHome() == HOME_LAYOUT_DINAMICA
                                ? NV_DIN_DEST_W : NV_DESTAQUE_EDITORIAL_W;
    case FILEIRA_TOP10_NUM: return NV_CARD_W;
    case FILEIRA_LARGA:    return NV_DIN_LARGA_W;
    // Opção adicional da referência: maior e quase quadrada, em 4:3.
    case FILEIRA_DESTAQUE_QUADRADO: return NV_DESTAQUE_QUADRADO_W;
    case FILEIRA_COLECAO: return 480.0f;
    case FILEIRA_SERVICO: return 360.0f;
    case FILEIRA_SOCIAL: return 540.0f;
    case FILEIRA_TOP10: return 212.0f;
    case FILEIRA_RETORNO: return 680.0f;
    case FILEIRA_CATALOGOS: return 360.0f;
    default:               if (cartazPadrao())
                             return escalaCartazPadrao() * NV_PAD_CARTAZ_W;
                           return escalaDoAjuste() *
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
    // #327: a fileira escolhida pode nao estar DESENHADA (catalogo dentro de
    // uma pasta de colecao, ou fora da Home): a descoberta a baixa so para
    // alimentar o destaque. Os titulos dela estao no catalogo publicado.
    if (!heroSetN) {
      for (i = 0; i < cat_n_fileiras(); i++) {
        const CatFileira *cf = cat_fileira(i);
        int k;
        if (!cf || strcmp(cf->chave, fonte)) continue;
        for (k = 0; k < cf->n && heroSetN < HOME_HERO_LISTA; k++)
          if (cf->ini + k >= 0 && cf->ini + k < cat_n()) heroSet[heroSetN++] = cf->ini + k;
        break;
      }
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
  // Os candidatos ja sao conhecidos: completa em segundo plano a sinopse dos
  // que vieram rasos (lista do Trakt), antes de o carrossel chegar a eles.
  desc_sinopse_hero(heroSet, heroSetN);
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
// Parametros do GFX_VITRINE (Padrao e Dinamica), definidos por desenhaHero antes
// de desenhar a arte. Globais de modulo e nao argumentos porque a arte sai por
// tres caminhos (a que entra, a que sai, o poster) e todos leem os mesmos.
static float vitVeu = 0.9f, vitAncora = 0.5f, vitDissolve = 0.0f, vitRaio = 0.0f;
// Onde o veu de baixo comeca, 0..1 da altura da arte (0 = o padrao do shader).
static float vitVeuIni = 0.0f;
// #232 (re-relato do #177): "Desfocar proximo episodio" so valia para o cartao
// do fim do player. O destaque da home (arte_hero_do_item) pede o STILL do
// episodio de quem e "a seguir" e o desenhava nitido: ajustes_cw_desfocar_proximo
// nao tinha nenhum chamador. Aqui: item de serie sem progresso que e o proximo
// episodio de uma serie, desenhado com o still do episodio (nunca a arte do
// titulo, que nao entrega nada), com o ajuste ligado.
int home_proximo_desfocar(const CatItem *ci, const char *arte) {
  const char *still;
  if (!ci || !arte || !arte[0] || !ajustes_cw_desfocar_proximo() || !ajustes_cw_ligado()) return 0;
  if (strcmp(ci->tipo, "series") || ci->progresso != 0) return 0;
  if (!(trakt_e_a_seguir(ci->imdb) || simkl_e_a_seguir(ci->imdb) || cwo_conta_a_seguir(ci->imdb))) return 0;
  still = artehero_url_episodio(ci);
  return still && !strcmp(still, arte);
}

// `desl` = deslize de lado em fracao de r.w (troca deslizada); 0 = no lugar.
// A arte de tela anda DENTRO do retangulo pelo shader, com as rampas paradas;
// o poster (que nao preenche o retangulo) anda o rect inteiro, e quem chama
// recorta em r.
static int desenhaArteHero(GfxRect r, GfxModo modo, const CatItem *item,
                           const char *path, float alpha, float desl) {
  int ehPoster = 0;
  // O `path` GANHA (22/09). Aqui era `item ? arteDoItem(item) : path`, e o
  // arteDoItem devolve item->backdrop cru: o que arte_hero_do_item escolhia
  // (fonte dos Ajustes, still do episodio, `original` da Alta) era pedido ao
  // cache e decodificado, mas o que ia para a tela era sempre o backdrop do
  // catalogo. MEDIDO no tests/heroarte_shot.sh com um printf temporario:
  // arteA = nuvio.invalid/arte/tmdb/w1280/tt0468569, tAtu = 41 (textura
  // pronta) e na captura o fundo do metahub. Esta linha e o "a settings de
  // selecionar o source das artes nao ta funcionando" do destaque. A
  // identidade continua garantida: `path` sai de arte_por_identidade com o
  // MESMO indice de `item`; arteDoItem fica para quando nao ha path.
  const char *arte = (path && path[0]) ? path
                   : item ? arteDoItem(item, &ehPoster) : NULL;
  GLuint tex;
  if (item && path && path[0] && item->poster[0] && !strcmp(path, item->poster))
    ehPoster = 1;
  if (!arte || !arte[0]) return 0;
  if (alpha <= 0.004f) return 1;   // invisivel: nem o quad de tela cheia
  tex = tex_obter_hero(arte);
  if (!tex) return 0;
  if (home_proximo_desfocar(item, arte)) {
    tex = gfx_desfocado(tex, arte);   // copia 96x54; sem ela, nada (nunca o still nitido)
    if (!tex) return 0;
  }
  gfx_tex_aspect_atual = tex_aspecto(arte);
  gfx_desliza_atual = ehPoster ? 0.0f : desl;
  if (!ehPoster && modo == GFX_VITRINE) {
    gfx_rect(r, tex, modo, vitVeu, vitAncora, vitDissolve, vitRaio, vitVeuIni, 0, 0, alpha);
  } else if (!ehPoster) {
    gfx_rect(r, tex, modo, 0, 0, 0, 0, 0, 0, 0, alpha);
  } else {
    r.x += desl * r.w;
    float ap = gfx_tex_aspect_atual > 0.05f ? gfx_tex_aspect_atual : (2.0f / 3.0f);
    float h = r.h, w = h * ap, limite = r.w * 0.42f;
    if (w > limite) { w = limite; h = w / ap; }
    GfxRect poster = { r.x + r.w - w, r.y + (r.h - h) * 0.5f, w, h };
    gfx_rect(poster, tex, GFX_HERO, 0, 0, 0, 0, 0, 0, 0, alpha);
  }
  gfx_tex_aspect_atual = 0.0f;
  gfx_desliza_atual = 0.0f;
  return 1;
}

// `esperando` separa DUAS COISAS QUE NAO SAO A MESMA, e a diferenca nasceu com
// o teto de NV_HERO_ESPERA_MS: passado o prazo o heroi troca sem a arte, e a
// arte esta A CAMINHO — dizer "Arte indisponível" ali seria trocar uma mentira
// (a arte do titulo anterior) por outra (a arte nao existe). Sem o teto so
// havia o caso de arte que de fato nao existe, e por isso a frase era uma so.
// A arte de um item e PLANA (nao poster), para gfx_hero_camadas — a mesma
// decisao de desenhaArteHero. Devolve a proporcao da textura em *asp. NAO
// pede a textura ao cache (quem chama ja a tem): um pedido a mais por quadro
// mudava a ordem do cache e, por ela, em que quadro cada arte chega.
static int heroArtePlana(const CatItem *item, const char *path, float *asp) {
  int ehPoster = 0;
  const char *arte = (path && path[0]) ? path
                   : item ? arteDoItem(item, &ehPoster) : NULL;
  if (item && path && path[0] && item->poster[0] && !strcmp(path, item->poster))
    ehPoster = 1;
  if (!arte || !arte[0] || ehPoster) return 0;
  if (home_proximo_desfocar(item, arte)) return 0;   // camadas usa a textura nitida
  *asp = tex_aspecto(arte);
  return 1;
}

static void desenhaPlaceholderHero(GfxRect r, const CatItem *item, float alpha,
                                   int esperando) {
  GfxRect bloco = { r.x + r.w * 0.58f, r.y + 32.0f,
                    r.w * 0.34f, r.h - 64.0f };
  // A CAMINHO, NADA (#164). O bloco "Carregando arte…" entrava a cada troca
  // de foco em que a arte passava do prazo — na Samsung o decode de uma arte
  // de destaque leva 300-900 ms, entao era quase toda troca — e o bloco
  // aparecendo e sumindo lia como um piscar. O que o #21 exige continua: a
  // arte do titulo ANTERIOR ja saiu (heroSai), nada falso fica na tela. So
  // nao ha mais um cartao por cima do vazio; a arte nova entra quando chegar.
  // "Arte indisponível" (titulo sem arte nenhuma) continua sendo desenhado.
  if (esperando) return;
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

// CARD CARREGANDO: a mesma superficie do esqueleto, com a luz passando
// (gfx_esqueleto) e so o nome do titulo — sem o aviso de indisponivel, que
// leria como quebrado algo que ainda esta a caminho.
static void desenhaCarregando(GfxRect r, float raio, const CatItem *item) {
  gfx_esqueleto(r, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G,
                NV_COR_ESQUELETO_B, 1.0f);
  if (item && item->titulo[0]) {
    TxtLinha nome = txt_linha_corta(TXT_MINI, item->titulo,
                                    160, 165, 178, 255, r.w - 32.0f);
    txt_desenhar_alpha(nome, r.x + (r.w - nome.w) * 0.5f,
                       r.y + (r.h - nome.h) * 0.5f, 0.78f);
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
  // A FONTE ESCOLHIDA NOS AJUSTES VALE AQUI TAMBEM (22/09): antes o card lia
  // item->backdrop cru e "Background do hero" nunca chegava nele. Lido a cada
  // quadro, entao trocar o ajuste muda o card na volta a home, sem reiniciar.
  if (deitado) {
    const char *b = artehero_url_card_fonte(item, ajustes_hero_fonte(),
                                            ajustes_hero_arte_diferente());
    // ARTE EM PE NUM CARD DEITADO: alguns addons (a "AI for you" na C9,
    // 29/09) mandam o cartaz no lugar do fundo, e ele saia como uma faixa
    // estreita no meio do card escuro. Medido pelo arquivo ja decodificado:
    // mais alto que largo = cartaz; o metahub monta o fundo pelo IMDb.
    if (b) { float ap = tex_aspecto(b);
      if (ap > 0.0f && ap < 1.0f) { const char *m = artehero_url_metahub_fundo(item);
        if (m) return m; } }
    return b ? b : (item->poster[0] ? item->poster : NULL);
  }
  // POSTER PERSONALIZADO (posterprov.h): so o cartaz retrato de card. Desligado
  // (padrao) ou sem id que o provedor entenda, devolve item->poster como veio.
  { const char *p = posterprov_card_addon(item->origem, item->imdb, item->tmdb, item->tipo, item->poster);
    if (p && p[0]) return p; }
  return item->backdrop[0] ? item->backdrop : NULL;
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
  int diferente = ajustes_hero_arte_diferente();
  // A ARTE ESCOLHIDA A MAO (#142) VENCE ATE O STILL DO EPISODIO: quem apontou
  // a foto na tela "Trocar arte" quer ve-la no destaque, e o still e regra
  // automatica. Tabela vazia = um retorno NULL, nada mais no quadro.
  { const char *esc = artehero_url_escolhida(item); if (esc) return esc; }
  // Ao escolher uma origem, o usuario esta pedindo a arte do titulo — nao o
  // still automatico do episodio. Automatico mantem o comportamento anterior,
  // inclusive o still de Continuar assistindo. A escolha entre fonte, card e
  // "outra arte" mora em artehero_url_destaque, que o detalhe tambem usa: os
  // dois tem de concordar, senao abrir o titulo troca a foto.
  if (fonte > 0) return artehero_url_destaque(item, fonte, diferente);
  const char *ep = artehero_url_episodio(item);
  // STILL PEQUENO NAO VAI AO DESTAQUE (#85, pokazideia: "backdrop pixelado
  // em alguns titulos de Continuar assistindo"). O metahub serve o still no
  // tamanho que a fonte tiver, e para varios episodios isso e 400 px — a
  // 1920 vira mosaico. Abaixo de 900 px de origem, o fundo do titulo.
  if (ep && !tex_falhou(ep)) {
    int w = tex_largura_fonte(ep);
    if (w == 0 || w >= 900) return ep;
  }
  return artehero_url_destaque(item, 0, diferente);
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
// `deitado`: 0 = cartaz em pe, 1 = deitado, 2 = destaque (tela cheia), 3 = card
// de "Continuar assistindo" (deitado, com o still do episodio quando a
// "Miniatura do episodio" — useEpisodeThumbnailsInCw — esta ligada). O 3 e o
// relato da Shield (teste 318.3): o ajuste existia sem nenhum chamador e o
// card mostrava sempre o fundo da serie. Still que o cache sabe que nao vem
// (404 do metahub) cai na arte do titulo, como no destaque.
static const char *arte_card_retomada(const CatItem *item) {
  if (item && ajustes_cw_thumb_episodio() && !strcmp(item->tipo, "series")) {
    const char *ep = artehero_url_episodio(item);
    if (ep && !tex_falhou(ep)) return ep;
  }
  return arte_por_formato(item, 1);
}
static const char *arte_por_identidade(int indice, int deitado) {
  const CatItem *item = cat_item_exato(indice);
  const char *arte = (deitado == 2) ? arte_hero_do_item(item)
                   : (deitado == 3) ? arte_card_retomada(item)
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
  heroDirDesejado = d > 0 ? 1 : -1;
  // O carrossel automatico so volta a contar depois do intervalo inteiro: uma
  // troca sozinha logo depois do toque leria como "a TV ignorou o que eu fiz".
  heroTrocaEm = SDL_GetTicks() + NV_HERO_INTERVALO_MS;
  { const char *quente = arte_por_identidade(alvo, 2);
    if (quente) tex_arquivo(quente); }
}

// SEGURAR OK NUM CARTAO: o menu do titulo, e a fileira dele para o "Estilo da
// fileira". Continuar assistindo, o Top 10 (pilha ou numeral) e o destaque
// ficam com o visual proprio — nao passam chave, e o menu nao oferece forma.
// Pasta de colecao nao tem titulo: abre o menu so da fileira.
static void abrirMenuCartaz(void) {
  const Fileira *s;
  if (focoHero || foco.fileira < 0 || foco.fileira >= nFileiras) {
    ctx_fileira(NULL, NULL);
    ctx_dispensar_retomar(0);
    ctx_abrir(heroAtual);
    return;
  }
  s = &fileiras[foco.fileira];
  if (s->tipo == FILEIRA_CATALOGOS) { ctx_abrir_fileira(s->chave, s->titulo); return; }
  // A PILHA FECHADA nao e um titulo (sao ate seis num card so): como a pasta
  // de colecao, abre direto no estilo da fileira.
  if (s->tipo == FILEIRA_TOP10 && s->stackN) { ctx_abrir_fileira(s->chave, s->titulo); return; }
  // OS RANKINGS LEVAM "Estilo da fileira" desde que viraram escolha (#201):
  // ficavam de fora, e quem escolhia Ranking numerado ou empilhado no menu nao
  // tinha mais como voltar por ele — so pelos Ajustes. Continuar assistindo
  // segue fora: a forma dela e o estilo da retomada (Ajustes > Continuar).
  if (s->tipo == FILEIRA_CONTINUE || !strcmp(s->chave, "continue_watching"))
    ctx_fileira(NULL, NULL);
  else ctx_fileira(s->chave, s->titulo);
  // "Dispensar" so no cartao de "Retomar agora" (ctxmenu.h).
  ctx_dispensar_retomar(s->tipo == FILEIRA_RETORNO);
  ctx_abrir(fileiraItemIndice(s, foco.coluna));
}

static int foco_pode_pressao_longa(void) {
  // No destaque ha sempre um titulo do catalogo por tras, entao o menu do
  // cartaz vale ali como vale num card.
  if (focoHero) return cat_n() > 0;
  if (foco.fileira < 0 || foco.fileira >= nFileiras) return 0;
  const Fileira *s = &fileiras[foco.fileira];
  // Pasta de colecao: o menu e so o do estilo da fileira (ctx_abrir_fileira).
  if (s->tipo == FILEIRA_SOCIAL) return 0;
  // Pilha fechada: so o estilo da fileira, entao so quando ela aceita forma.
  if (s->tipo == FILEIRA_TOP10 && s->stackN)
    return fil_estilos(s->chave, NULL, NULL, FIL_TIPO_N) > 0;
  if (s->verTudo && foco.coluna == s->n) return 0;
  return foco.coluna >= 0 && foco.coluna < s->n;
}


static float alturaDe(TipoFileira t) {
  switch (t) {
    case FILEIRA_CONTINUE: return NV_DESTAQUE_H;
    case FILEIRA_DESTAQUE: return layoutHome() == HOME_LAYOUT_DINAMICA
                                ? NV_DIN_DEST_H : NV_DESTAQUE_EDITORIAL_H;
    case FILEIRA_TOP10_NUM: return NV_CARD_H;
    case FILEIRA_LARGA:    return NV_DIN_LARGA_H;
    case FILEIRA_DESTAQUE_QUADRADO: return NV_DESTAQUE_QUADRADO_H;
    case FILEIRA_COLECAO: return 270.0f;
    case FILEIRA_SERVICO: return 203.0f;
    case FILEIRA_SOCIAL: return 240.0f;
    case FILEIRA_RETORNO: return 178.0f;
    case FILEIRA_TOP10: return 320.0f;
    case FILEIRA_CATALOGOS: return 203.0f;
    default:               if (cartazPadrao())
                             return escalaCartazPadrao() * NV_PAD_CARTAZ_H;
                           return escalaDoAjuste() *
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
  // Espaco entre titulos (Ajustes): so o vao medido escala, nunca o xOffTipo. Os
  // cards grandes crescem 6% no foco, entao o vao deles nao desce de 24.
  float f = ajustes_espaco_titulos();
  if (t == FILEIRA_DESTAQUE || t == FILEIRA_DESTAQUE_QUADRADO) {
    float g = NV_CARD_GAP_GRANDE * f;
    return g < NV_CARD_GAP ? NV_CARD_GAP : g;
  }
  return NV_CARD_GAP * f + xOffTipo(t);
}
// Passo vertical entre fileiras. `.home-modern-landscape-posters` aperta o
// `--home-row-gap` de 32 para 24 (components.css:6473) — a fileira deitada e
// mais baixa e o respiro do poster em pe sobraria nela.
static float fileiraGapBase(void);
static float fileiraGap(void) { return fileiraGapBase() * ajustes_espaco_fileiras(); }
static float fileiraGapBase(void) {
  // Padrao: o respiro largo entre secoes da home original do Nuvio (~100 px da
  // base dos cartoes ao titulo seguinte na captura do dono); o fundo e liso, e
  // e o vazio que separa uma fileira da outra.
  if (layoutHome() == HOME_LAYOUT_PADRAO) return NV_PAD_FILEIRA_GAP;
  // Dinamica: um vao so, mais justo, para caber mais fileiras em volta da do
  // meio (layout.h, NV_DIN_FILEIRA_GAP).
  if (layoutHome() == HOME_LAYOUT_DINAMICA) return NV_DIN_FILEIRA_GAP;
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
  // VEU SO NA METADE DE BAIXO, onde a faixa mora. Era o GFX_VEU do cartao
  // inteiro (base + esquerda de cima a baixo): o card aberto e o maior desenho
  // da home parada, e na C9 a fileira de cards grandes caia de 60 para 44 fps
  // so por ficar parada nele (29/09, clr=33 ms = GPU presa). Mesmo veu da
  // legenda do cartaz deitado: zero no topo do retangulo, raio convertido para
  // a altura dele.
  //
  // O RAIO E O DO CARD (raioDe), nao NV_RAIO_CARD: 0,055 da altura sao 19 px
  // no card aberto e o cartaz tem 24, entao os cantos de baixo do veu
  // passavam por fora da curva (MEDIDO em homelayouts_shot, 01/10/2026: ~50
  // px escurecidos fora do card). E o mesmo defeito do #144 no Continuar.
  gfx_veu_base((GfxRect){ px, py, w, h }, raioDe(w, h), NV_ABERTA_VEU_F,
               NV_ABERTA_VEU_A * abre);
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
  int imdbRating = imdbnota_obter(ci->imdb, ci->nota, !strcmp(ci->tipo,"series"));
  if (imdbRating > 0) {
    float bw = badge_imdb_largura(imdbRating);
    float xMin = px + pad + (ci->logo[0] ? w * 0.34f : 0.0f);
    float badgeX = cx - bw;
    // O logo ocupa a esquerda da mesma faixa. Se os outros chips consumirem
    // todo o espaço, omitir o par é melhor que atravessar o logo ou o canto
    // arredondado do card.
    if (badgeX >= xMin) {
      cx = badgeX;
      badge_imdb(cx, cy + (ch - BADGE_H) * 0.5f, imdbRating, 0, a);
    }
  }
  // 5. Progresso em andamento: fio na base, dentro do raio do card.
  if (ci->progresso > 0 && ci->progresso < 90) {
    float fx = px + pad, fw = w - pad * 2, fy = py + h - 12.0f * esc, fh = 4.0f;
    float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
    gfx_cor((GfxRect){ fx, fy, fw, fh }, 0.5f, 1, 1, 1, 0.22f * a);
    // Anda do valor anterior ate o novo, como no Continuar (revela.h).
    float prog = revela_progresso(ci->imdb, ci->temporada, ci->episodio,
                                  (float)ci->progresso, SDL_GetTicks());
    gfx_cor((GfxRect){ fx, fy, fw * prog / 100.0f, fh }, 0.5f, ar, ag, ab, a);
  }
}

static void desenhaProfundidade(GfxRect card, float raio, int ligadaAqui) {
  foco_profundidade(card, raio, ligadaAqui, 1.0f);   // focoprof.h: o mesmo do Detalhe
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
// O TETO DOS 4:3 MAIORES POR LAYOUT. Na Moderna e no Padrao com destaque a
// fileira em foco fica SEMPRE no mesmo Y (a nota em alvoY), embaixo do
// destaque: o 4:3 grande (608 px) MEDIDO na captura saia ~110 px pela base da
// tela, com o foco nele. Ali o maior vira o que cabe ate a base, e o medio fica
// no meio do caminho — os tres continuam diferentes. A Dinamica centra a
// fileira em foco e o grande cabe inteiro (captura), sem teto. O 4:3 de sempre
// (fator 1) nunca passa por aqui.
#define NV_43_FOLGA_BASE 24.0f
static float limita43(float fator, float e) {
  float hMax, eMax, teto;
  if (fator <= 1.0f || layoutHome() == HOME_LAYOUT_DINAMICA) return e;
  hMax = NV_TELA_H - (topoFileiras() + NV_LEGACY_ROW_HEAD_H) - NV_43_FOLGA_BASE;
  // O foco cresce para BAIXO no 4:3 (cardY), e o anel fica por fora.
  hMax = hMax / (1.0f + escalaDe(FILEIRA_DESTAQUE_QUADRADO))
       - (ajustes_borda_foco() ? NV_ANEL_FOCO : 0.0f);
  eMax = hMax / NV_DESTAQUE_QUADRADO_H;
  if (eMax < 1.0f) eMax = 1.0f;
  teto = fator >= fil_tipo_fator(FIL_TIPO_DESTAQUE_QUADRADO_G)
       ? eMax : 1.0f + (eMax - 1.0f) * 0.5f;
  return e > teto ? teto : e;
}
static float escalaFil(int r) {
  float e = (r >= 0 && r < MAX_FIL) ? fileiras[r].escala : 0.0f;
  e = e > 0.05f ? e : 1.0f;
  if (r >= 0 && r < MAX_FIL && fileiras[r].fator > 1.0f &&
      fileiras[r].tipo == FILEIRA_DESTAQUE_QUADRADO)
    e = limita43(fileiras[r].fator, e);
  return e;
}
static float larguraFil(int r) {
  // 680 e a largura da PILHA do Top 10, que nao sai de larguraDe: ela e uma
  // fileira de um card so, com o ranking dentro. Estava repetida nos dois
  // lugares que mediam a fileira e agora esta num.
  float w = (r >= 0 && r < MAX_FIL && fileiras[r].stackN)
          ? 680.0f : larguraDe(fileiras[r].tipo);
  if (fileiras[r].tipo == FILEIRA_CATALOGOS) { float h; medidaColecao(fileiras[r].forma, &w, &h); }
  return w * escalaFil(r);
}
static float alturaFil(int r) {
  float h = alturaDe(fileiras[r].tipo);
  if (fileiras[r].tipo == FILEIRA_CATALOGOS) { float w; medidaColecao(fileiras[r].forma, &w, &h); }
  return h * escalaFil(r);
}
// Altura TOTAL que a fileira ocupa: a arte mais o bloco de rotulo, quando ele
// existe. Sem somar o rotulo aqui, a fileira seguinte sobe por cima do texto —
// foi o mesmo defeito que o titulo de fileira ja tinha tido sobre os cards.
static float alturaTotalFil(int r) {
  return alturaFil(r) + (temRotulo(fileiras[r].tipo) ? NV_POSTER_COPY_H : 0.0f);
}
static float passoFil(int r)       { return larguraFil(r) + gapDe(fileiras[r].tipo); }

// ROLAGEM CENTRADA DA DINAMICA (dono, 01/10: "o foco fica na fileira do meio e
// a lista rola por baixo, como o Apple TV"). `off` e a soma das fileiras acima
// da `r` — a rolagem que a ancoraria em NV_DIN_TOPO_FIL, como era. Daqui sai a
// rolagem que poe o CENTRO dela (titulo + cartoes) em NV_DIN_CENTRO_FIL, presa
// nos extremos: nunca abaixo de zero (a primeira fileira nao desce do topo, nao
// sobra vao em cima) e nunca alem do ponto em que a ultima fica a
// NV_DIN_FOLGA_BASE da base (nao sobra vao embaixo; a folga e a do aviso de
// "cabem mais fileiras"). Lista curta que cabe inteira: fica em zero.
static float dinRolagemCentrada(int r, float off) {
  float total = 0.0f, s, sMax;
  int i;
  if (r < 0 || r >= nFileiras) return off;
  for (i = 0; i < nFileiras; i++)
    total += NV_LEGACY_ROW_HEAD_H + alturaTotalFil(i) + (i ? fileiraGap() : 0.0f);
  s = NV_DIN_TOPO_FIL + off + 0.5f * (NV_LEGACY_ROW_HEAD_H + alturaTotalFil(r))
    - NV_DIN_CENTRO_FIL;
  sMax = NV_DIN_TOPO_FIL + total - (NV_TELA_H - NV_DIN_FOLGA_BASE);
  if (s > sMax) s = sMax;
  if (s < 0.0f) s = 0.0f;
  return s;
}

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
  float dir   = esq + larguraFil(r) + xOffTipo(fileiras[r].tipo);
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
    // Os tres tamanhos sao a MESMA forma; o fator vem por fil_escala.
    case FIL_TIPO_DESTAQUE_QUADRADO:
    case FIL_TIPO_DESTAQUE_QUADRADO_M:
    case FIL_TIPO_DESTAQUE_QUADRADO_G: return FILEIRA_DESTAQUE_QUADRADO;
    case FIL_TIPO_COLECAO:  return FILEIRA_COLECAO;
    case FIL_TIPO_SERVICO:  return FILEIRA_SERVICO;
    case FIL_TIPO_RANKING:  return FILEIRA_TOP10_NUM;
    case FIL_TIPO_LARGA:    return FILEIRA_LARGA;
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
typedef struct { char chave[192]; char itemId[64]; char itemTipo[8]; int coluna; int scrollX; } HomePos;

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
  int r, antigoN = nPosViva;
  HomePos antigo[MAX_FIL];
  char antigoFoco[192];
  memcpy(antigo, posViva, sizeof antigo);
  snprintf(antigoFoco, sizeof antigoFoco, "%s", posVivaFoco);
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
    // Para uma fileira que nao esta em foco, o ultimo ID conhecido e mais
    // confiavel que o indice: a publicacao incremental pode ja ter trocado o
    // bloco do catalogo quando este snapshot for capturado.
    { int a, idx = fileiraItemIndice(&fileiras[r], p->coluna);
      const CatItem *it = cat_item(idx);
      p->itemId[0] = p->itemTipo[0] = 0;
      for (a = 0; a < antigoN; a++)
        if (!strcmp(antigo[a].chave, p->chave) && antigo[a].itemId[0]) {
          if (r != foco.fileira || !strcmp(antigoFoco, p->chave)) {
            snprintf(p->itemId, sizeof p->itemId, "%s", antigo[a].itemId);
            snprintf(p->itemTipo, sizeof p->itemTipo, "%s", antigo[a].itemTipo);
            break;
          }
        }
      if (!p->itemId[0] && it) {
        snprintf(p->itemId, sizeof p->itemId, "%s", it->imdb);
        snprintf(p->itemTipo, sizeof p->itemTipo, "%s", it->tipo);
      }
    }
    p->scrollX = (int)(scrollX[r] + 0.5f);
  }
  if (foco.fileira >= 0 && foco.fileira < nFileiras
      && !posIgnora(fileiras[foco.fileira].chave)) {
    snprintf(posVivaFoco, sizeof posVivaFoco, "%s", fileiras[foco.fileira].chave);
    posVivaCol = foco.coluna;
  }
}

static int primeiraFileiraNavegavel(void) {
  for (int r = 0; r < foco.nFileiras; r++)
    if (foco.nColunas[r] > 0) return r;
  return -1;
}

// Restaura a chave somente quando tem cards. Se ficou vazia, usa a primeira
// fileira navegavel; sem nenhuma, conserva o destaque.
static int posAplicarTabela(const HomePos *t, int n,
                            const char *chFoco, int colFoco) {
  int r, achou = -1;
  for (r = 0; r < nFileiras; r++) {
    const HomePos *p = posAchar(t, n, fileiras[r].chave);
    int c;
    if (!p) continue;
    c = p->coluna;
    // Durante a sessao o item e a identidade primaria: uma resposta
    // incremental pode inserir/remover cards dentro da mesma chave sem
    // deslocar o foco para outro titulo. Coluna continua sendo fallback para
    // o caso de o item ter saído legitimamente do catalogo.
    //
    // COLUNA 0 NAO SEGUE ITEM NENHUM (#95, "It's back" na 1.4.3). Fileira na
    // coluna 0 e fileira que ninguem andou: o lugar dela e o COMECO, qualquer
    // que seja o item que esteja la. Seguir o ID era o defeito: o arranque
    // monta primeiro com o cache (a lista de ontem) e depois com a rede, e o
    // primeiro item de ontem numa lista que reordena todo dia (Top 100,
    // tendencias) esta hoje na coluna 3 ou 4 — a coluna lembrada ia junto, e
    // descer ate a fileira caia no 3o/4o cartaz depois de reabrir o app.
    if (p->itemId[0] && p->coluna > 0) {
      int q;
      for (q = 0; q < fileiras[r].n; q++) {
        int idx = fileiraItemIndice(&fileiras[r], q);
        const CatItem *it = cat_item(idx);
        if (it && !strcmp(it->imdb, p->itemId) &&
            !strcmp(it->tipo, p->itemTipo)) { c = q; break; }
      }
    }
    // A fileira pode ter encolhido entre uma publicacao e outra, ou entre
    // ontem e hoje.
    if (c >= foco.nColunas[r]) c = foco.nColunas[r] - 1;
    if (c < 0) c = 0;
    foco.colunaLembrada[r] = c;
    scrollX[r] = (float)p->scrollX;
  }
  if (chFoco && chFoco[0])
    for (r = 0; r < nFileiras; r++)
      if (!strcmp(fileiras[r].chave, chFoco) && foco.nColunas[r] > 0) { achou = r; break; }
  if (achou >= 0) {
    int c = colFoco;
    const HomePos *pf = posAchar(t, n, chFoco);
    // Mesma regra de cima: o foco no primeiro cartaz fica no primeiro cartaz.
    if (pf && pf->itemId[0] && colFoco > 0) {
      int q;
      for (q = 0; q < fileiras[achou].n; q++) {
        int idx = fileiraItemIndice(&fileiras[achou], q);
        const CatItem *it = cat_item(idx);
        if (it && !strcmp(it->imdb, pf->itemId) &&
            !strcmp(it->tipo, pf->itemTipo)) { c = q; break; }
      }
    }
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
  if (foco.fileira < 0 || foco.fileira >= foco.nFileiras ||
      foco.nColunas[foco.fileira] < 1) {
    int primeira = primeiraFileiraNavegavel();
    foco.fileira = primeira >= 0 ? primeira : 0;
    foco.coluna = 0;
    if (primeira < 0) focoHero = 1;
  }
  return achou;
}

static void homeMarcarURL(const char *url, float largura) {
  if (!url || (strncmp(url, "http://", 7) && strncmp(url, "https://", 8))) return;
  tex_cache_marcar_larg(NV_CACHE_ARTE_GRUPO_HOME, url, largura, 1, 0);
}

static void homeAtualizarReferenciasArte(void) {
  int r, i;
  cachearte_limpar_referencias_grupo(NV_CACHE_ARTE_GRUPO_HOME);
  for (r = 0; r < nFileiras; r++) {
    int limite = fileiras[r].n < 8 ? fileiras[r].n : 8;
    float largura = larguraFil(r);
    for (i = 0; i < limite; i++) {
      int idx = fileiraItemIndice(&fileiras[r], i);
      const CatItem *it = cat_item(idx);
      if (!it) continue;
      homeMarcarURL(posterprov_card_addon(it->origem, it->imdb, it->tmdb, it->tipo, it->poster), largura);
      homeMarcarURL(it->logo, largura * 0.65f);
      homeMarcarURL(it->backdrop, largura);
    }
  }
  // Pin the actual source selected for the large hero request, which can use a
  // different backdrop URL and resolution from the row card.
  heroSetGarantir();
  for (i = 0; i < heroSetN; i++)
    homeMarcarURL(arte_por_identidade(heroSet[i], 2), NV_TELA_W);
  cachearte_estatisticas_pedir();
}

int home_iniciar(const char *dirArte) {
  extras_carregar(dirArte);
  col_carregar(dirArte);
  badges_carregar(dirArte);
  selospacote_dir_embutidos(dirArte);   // pacotes de selos embutidos (padrao + colorido)
  cat_carregar(dirArte);
  // O cache da ULTIMA sessao entra por cima do catalogo do pacote, antes de
  // qualquer rede. Se nao existir (primeira execucao) ou for de outra build,
  // segue-se com o do pacote, como sempre foi.
  // E com o texto/arte localizados que a sessao anterior ja buscou (#213): o
  // Continuar gravado no cache podia ter saido antes da traducao.
  if (cat_ler_cache(dirArte)) {
    desc_localizar_catalogo_cache();
    marco("catalogo do cache na tela");
  }
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

  // MODO CINEMA: a primeira tecla devolve a UI (o trailer segue, como no
  // detalhe). Esquerda/direita/baixo NAO se perdem: sao navegacao, e o destaque
  // muda ou o foco desce mesmo — o trailer acaba junto e a UI ja esta voltando.
  // OK, Voltar e Cima so devolvem: nao abrem o titulo nem perguntam se sai sem
  // a pessoa ter visto a tela. (O KEYUP do OK que sobra e ignorado la embaixo:
  // okPressionando nao foi armado.)
  if (heroCinema.oculta && e->type == SDL_KEYDOWN && !e->key.repeat) {
    SDL_Keycode kc = e->key.keysym.sym;
    trailercinema_tecla(&heroCinema);
    if (kc != SDLK_LEFT && kc != SDLK_RIGHT && kc != SDLK_DOWN) return;
  }

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
        if (dur >= NV_HOLD_MS) abrirMenuCartaz();
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
        s->n=s->stackN;   // a fileira inteira, nao 10 (issue #201)
        s->stackN=0;s->verTudo=1;
        foco.coluna=0;foco.colunaLembrada[foco.fileira]=0;
        foco.nColunas[foco.fileira]=s->n+1;
        return;
      }
      // A FILEIRA DE AMIGOS decide o proprio OK: rosto = perfil, cartao =
      // titulo, "+ Adicionar"/convite = tela de amigos ou o QR do celular.
      if(fileiras[foco.fileira].tipo==FILEIRA_SOCIAL) {
        amigosfil_ok(foco.coluna);
        if(amigosfil_pediu_ajustes())pedidoSocial=1;
        return;
      }
      if (fileiras[foco.fileira].tipo == FILEIRA_CATALOGOS) {
        if (dur >= NV_HOLD_MS) abrirMenuCartaz();
        else if (foco.coluna >= 0 && foco.coluna < fileiras[foco.fileira].n) {
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
        abrirMenuCartaz();
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
        // "Proximos episodios" (issue #127) NAO toca: o episodio ainda nao foi
        // ao ar, e nenhum addon tem fonte para ele. OK abre a pagina.
        if (ajustes_cw_ok_toca() && strcmp(fl->chave, "upcoming_section") &&
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
#if defined(__APPLE__) || defined(NV_LINUX_DESKTOP)
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
      heroAutoDesligado = 1;   // voltou um titulo: navega a mao, o carrossel para
      heroPasso(-1); return;
    }
    if (k == SDLK_DOWN) {
      if (foco.fileira < 0 || foco.fileira >= foco.nFileiras ||
          foco.nColunas[foco.fileira] < 1) {
        int primeira = primeiraFileiraNavegavel();
        if (primeira < 0) return;
        foco.fileira = primeira; foco.coluna = 0;
      }
      focoHero = 0; return;
    }
    // Cima no destaque: nao ha para onde ir. A pagina sobe um pouco e volta.
    if (k == SDLK_UP) { anim_borda_bater(&bordaPag, -NV_BORDA_AMP); return; }
  }
  // A FILEIRA DE AMIGOS ANDA POR DENTRO antes da home: direita entra nos
  // cartoes do rosto em foco, esquerda volta ao rosto (amigosfil.h).
  if (foco.fileira >= 0 && foco.fileira < nFileiras &&
      fileiras[foco.fileira].tipo == FILEIRA_SOCIAL) {
    int c = foco.coluna;
    if (amigosfil_tecla(k, &c)) {
      if (c != foco.coluna) {
        foco.coluna = c;
        if (foco.fileira < FOCUS_MAX_FILEIRAS) foco.colunaLembrada[foco.fileira] = c;
      }
      return;
    }
  }
  if (k == SDLK_RIGHT) {
    // O `&&` aqui era um curto-circuito com efeito colateral: escrito como
    // `if (fileira == 0 && !focus_mover(...))`, o focus_mover so era chamado
    // NO HERO — em qualquer outra fileira a seta direita nao movia nada. Mover
    // primeiro, decidir depois.
    // Sem mover (ultimo cartao, ou o "Ver tudo"), a fileira bate na borda.
    if (!focus_mover(&foco, 1, 0) && foco.fileira < MAX_FIL)
      anim_borda_bater(&bordaFil[foco.fileira], NV_BORDA_AMP);
  } else if (k == SDLK_LEFT) {
  // Esquerda na primeira coluna chama o menu lateral, em QUALQUER fileira —
    // inclusive no hero. Antes o hero era excecao e usava a esquerda para
    // voltar um titulo no carrossel: quem chegava ali (voltando de outra tela,
    // por exemplo) nao tinha como abrir o menu sem antes descer. O carrossel
    // continua acessivel pela direita e pela troca automatica.
    // Por isso a esquerda nunca bate na borda: a coluna 0 e a porta do menu.
    if (foco.coluna == 0) { pedidoMenu = 1; return; }
    if (!focus_mover(&foco, -1, 0) && foco.fileira < MAX_FIL)
      anim_borda_bater(&bordaFil[foco.fileira], -NV_BORDA_AMP);
  }
  else if (k == SDLK_DOWN)  focus_mover(&foco, 0, 1);
  else if (k == SDLK_UP && !focus_mover(&foco, 0, -1)) focoHero = 1;
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
       | (ajustes_rotulos_poster() ? 16 : 0)
       // A Ordenacao (issue #127): "Separar futuros" parte a fileira em duas
       // sem a descoberta publicar nada novo quando a lista ja e a mesma.
       | (ajustes_cw_ordem() << 5)
       // O layout muda a FORMA das fileiras (dinAtribuirTipos) e o tamanho do
       // destaque: escolher outro em Ajustes remonta a lista ao voltar.
       | (ajustes_home_layout() << 8)
       // Ligar/desligar o destaque decide se a primeira fileira vira vitrine
       // (vitrineNaPrimeira).
       | (ajustes_hero_ligado() ? 1 << 12 : 0);
}

// A PRIMEIRA FILEIRA DE CATALOGO VIRA VITRINE (cartoes 16:9 grandes) so quando
// ela e a vitrine da home: na Dinamica, onde a forma de cada fileira sai do que
// ela e (dinAtribuirTipos), e com o destaque DESLIGADO, quando nada mais faz
// esse papel.
//
// Na Moderna e no Padrao com o destaque ligado, nao (#201, mackojanko: "First
// row after the Spotlight on top row automatic setting make it different
// (landscape) from the next other rows"). Ali o destaque ja e a vitrine — e,
// sem fonte escolhida, percorre OS MESMOS titulos dessa fileira (heroMontarSet
// cai nos primeiros do catalogo) — entao a fileira repetia o destaque em
// cartoes deitados e era a unica em Automatico que nao saia como as outras.
// Quem quer a vitrine ali escolhe "Paisagem grande" no Estilo da fileira.
static int vitrineNaPrimeira(void) {
  return ajustes_home_layout() == HOME_LAYOUT_DINAMICA || !ajustes_hero_ligado();
}

// --- DINAMICA: A FORMA DE CADA FILEIRA SAI DO QUE ELA E ------------------------
//
// A Apple TV nao empilha "fileira de cartaz, fileira de cartaz...": cada
// prateleira tem a forma do que carrega. A regra, na ordem em que vale (so para
// fileira em modo Automatico em Ajustes -> Fileiras da Home; a forma que a
// pessoa escolheu la nunca e trocada):
//
//   1. a PRIMEIRA fileira de catalogo virou destaque em sincronizarFileiras
//      (cartoes grandes 16:9, com logo) — a vitrine;
//   2. "Continuar assistindo" continua deitado (419x236), e colecoes, servicos,
//      "Entre amigos" e atalhos ficam como estao: ja sao cartoes largos;
//   3. o PRIMEIRO catalogo cujo NOME diz ranking — Top 10/100 (que ja vinha
//      como pilha), "Em alta", "Popular", "Tendencias", "Mais vistos" — vira
//      TOP 10: numeral grande ao lado de cada cartaz, com TODOS os itens da
//      fileira (o "Itens por fileira" de Ajustes, 12/18/24 — issue #201; ate
//      a 1.6.5 cortava em 10 e o resto do catalogo sumia da home). So
//      UM por home, decisao do dono (29/09): os rankings seguintes ("Em alta"
//      depois de "Top 10", "Trending"...) voltam ao cartaz em pe de sempre —
//      dois numerados na mesma tela disputam, e o segundo le como repeticao;
//   4. o resto alterna cartaz em pe / faixa deitada, comecando por cartaz. E o
//      padrao FIXO para quando nao ha sinal nenhum: a home nunca fica com duas
//      fileiras iguais coladas.
//
// Nao ha inferencia de conteudo alem do nome publico do catalogo — o mesmo
// criterio de perfilCatalogo (nunca a URL, que pode carregar token).
// O termo tem de COMECAR uma palavra: "top" sem isto acerta "Christopher Nolan".
static int comecaPalavra(const char *nome, const char *termo) {
  size_t n = strlen(termo);
  for (const char *p = nome; p && *p; p++) {
    size_t i;
    if (p != nome && (isalnum((unsigned char)p[-1]) || (unsigned char)p[-1] >= 0x80)) continue;
    for (i = 0; i < n && p[i] &&
         tolower((unsigned char)p[i]) == (unsigned char)termo[i]; i++) {}
    if (i == n) return 1;
  }
  return 0;
}
static int sinalRanking(const Fileira *f) {
  static const char *palavras[] = { "top", "em alta", "trending", "popular",
                                    "mais vist", "most watched", "ranking",
                                    "tend\xc3\xaa" "ncia" };
  size_t i;
  for (i = 0; i < sizeof palavras / sizeof palavras[0]; i++)
    if (comecaPalavra(f->titulo, palavras[i]) || comecaPalavra(f->catId, palavras[i]))
      return 1;
  return 0;
}
static void dinAtribuirTipos(int total) {
  int ranking = 0, planas = 0, i;
  for (i = 0; i < total; i++) {
    Fileira *f = &fileiras[i];
    if (f->n < 1 || fil_tipo(f->chave) != FIL_TIPO_AUTO) continue;
    if (f->tipo == FILEIRA_TOP10 && f->base[0] && f->catId[0]) {
      // A pilha do Top 10 so tem sentido como ranking; sem a vaga, e uma
      // fileira de cartazes como outra qualquer (a pilha de um card so da
      // Moderna nao e forma da Dinamica).
      if (ranking < 1) { f->tipo = FILEIRA_TOP10_NUM; ranking++; }
      else { f->tipo = FILEIRA_NORMAL; planas = 1; }
    } else if (f->tipo == FILEIRA_NORMAL) {
      if (!strcmp(f->chave, "continue_watching") || !strcmp(f->chave, "upcoming_section"))
        continue;
      if (f->base[0] && f->catId[0] && ranking < 1 && f->n >= 5 && sinalRanking(f)) {
        f->tipo = FILEIRA_TOP10_NUM; ranking++;
      } else if (ranking && f->base[0] && f->catId[0] && sinalRanking(f)) {
        // Ranking sem vaga: cartaz em pe, como pedido — e a alternancia segue
        // dele, para a proxima fileira sem sinal virar a faixa deitada.
        planas = 1;
      } else {
        f->tipo = (planas++ & 1) ? FILEIRA_LARGA : FILEIRA_NORMAL;
      }
    }
  }
}

// OS CARDS FUTUROS DE "CONTINUAR ASSISTINDO" com a Ordenacao em "Separar
// futuros" (issue #127, ver cwordem.h): indices no catalogo, na ordem da
// fileira (a montagem ja os pos pela estreia). Separados no laco das fileiras
// e reinseridos como "upcoming_section" logo abaixo da retomada DEPOIS do
// arranjo por fil_unir: a fileira nao e do catalogo nem da conta, e sim uma
// metade da retomada — registra-la em fileiras.c a poria no fim da home (chave
// nova vai para o fim) e deixaria a pessoa separa-la da outra metade.
static int proxHome[MAX_CARDS], nProxHome;
static void sincronizarFileiras(void) {
  int nCat = cat_n_fileiras(), r, destino = 0;
  int assin = assinaturaPrefs();
  // #392: reservas expiram em 30 s, mesmo sem nova resposta da rede.
  // ponytail: resposta depois do prazo pode mover CW; usar o fim da rodada
  // de descoberta se for preciso esperar catalogos por mais tempo.
  static unsigned reservaGeracao;
  static Uint32 reservaDesde;
  static int reservaEstado; // 0 ainda nao usada, 1 ativa, 2 expirada
  unsigned geracao = fil_passada_ler().geracao;
  if (geracao != reservaGeracao) {
    reservaGeracao = geracao;
    reservaEstado = 0;
    filsAplicadas = -1;
  }
  if (reservaEstado == 1 && SDL_GetTicks() - reservaDesde >= 30000u) {
    reservaEstado = 2;
    filsAplicadas = -1;
  }
  static unsigned ultimaRevisao;
  // GUARDA CURTO POR CONTADORES, antes do hash das fileiras. O hash FNV sobre
  // todas as chaves/titulos/ini/n de cada CatFileira e o guarda de seguranca
  // (logo abaixo) e continua existindo — este so evita computa-lo quando nenhum
  // dos contadores relevantes mudou desde a ultima montagem. Cada contador e
  // um inteiro lido sob mutex barato (fil_revisao, fil_limite, col_revisao) ou
  // direto (cat_revisao); cat_revisao e bumpado em cat_definir_tudo,
  // cat_trocar_continuar E cat_republicar_fileiras, cobrindo todo caminho
  // que troca fils[].
  static unsigned ultCatRev, ultFilRev, ultColRev, ultFilLim, ultCwoRev, ultOrdemRev, ultRetRev;
  unsigned catRev = cat_revisao(), filRev = fil_revisao();
  int filLim = fil_limite();
  unsigned colRev = col_revisao();
  unsigned ordemRev = catordem_revisao();
  // cwo_revisao tambem: o conjunto de futuros pode mudar sem o catalogo mudar
  // (a mesma lista publicada, so a divisao outra), e o hash abaixo ja o pesa.
  unsigned cwoRev = cwo_revisao();
  if (nCat == filsAplicadas && assin == prefsAplicadas &&
      catRev == ultCatRev && filRev == ultFilRev &&
      colRev == ultColRev && ordemRev == ultOrdemRev &&
      filLim == ultFilLim && cwoRev == ultCwoRev &&
      retomarAplicada == retomarRev && cw_retido_rev() == ultRetRev &&
      ultCatRev) {   // ultCatRev=0: primeira chamada, cai no hash
    ultFilRev = filRev; ultColRev = colRev; ultFilLim = (unsigned)filLim;
    return;
  }
  // Local group visibility changes also change which source rows are wrapped.
  // Reconcile only after the fast guard fails, never on an unchanged frame.
  if (filRev != ultFilRev || colRev != ultColRev || ordemRev != ultOrdemRev)
    colfileiras_sincronizar();
  filRev = fil_revisao(); colRev = col_revisao();
  ultCatRev = catRev; ultFilRev = filRev; ultColRev = colRev; ultFilLim = (unsigned)filLim;
  ultCwoRev = cwoRev;
  ultOrdemRev = ordemRev;
  ultRetRev = cw_retido_rev();
  unsigned revisao = 2166136261u;
  revisao = (revisao ^ (unsigned)reservaEstado) * 16777619u;
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
  revisao = (revisao ^ ordemRev) * 16777619u;
  // Quem a montagem publicou como futuro (issue #127): so pesa em "Separar
  // futuros", mas e um inteiro — mais barato perguntar sempre que ramificar.
  revisao = (revisao ^ cwo_revisao()) * 16777619u;
  // O titulo retido pelo cartao/faixa (cwretido.h) muda a fileira desenhada.
  revisao = (revisao ^ cw_retido_rev()) * 16777619u;
  for (r = 0; r < nCat; r++) {
    const CatFileira *cf = cat_fileira(r);
    if (!cf) break;
    for (const unsigned char *s = (const unsigned char *)cf->chave; *s; s++)
      revisao = (revisao ^ *s) * 16777619u;
    for (const unsigned char *s = (const unsigned char *)cf->titulo; *s; s++)
      revisao = (revisao ^ *s) * 16777619u;
    revisao = (revisao ^ (unsigned)cf->ini) * 16777619u;
    revisao = (revisao ^ (unsigned)cf->n) * 16777619u;
    // A fileira pode manter a mesma chave/janela enquanto o feed incremental
    // insere ou substitui itens. A identidade exibida tambem participa da
    // revisao para que o foco seja remapeado pelo ID, e nao pela coluna velha.
    for (int ci = 0; ci < cf->n; ci++) {
      const CatItem *it = cat_item(cf->ini + ci);
      if (it) for (const unsigned char *s = (const unsigned char *)it->imdb; *s; s++)
        revisao = (revisao ^ *s) * 16777619u;
    }
  }
  if ((nCat < 1 && !col_n() && !nFileiras) || (nCat == filsAplicadas && assin == prefsAplicadas
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
  // static: com MAX_FIL em 64 isto sao ~82 KB, e a funcao ja carrega outro vetor
  // do mesmo tamanho (arranjo). Roda so no fio de desenho.
  static Fileira antigas[MAX_FIL];
  int nAntigas = nFileiras;
  memcpy(antigas, fileiras, sizeof antigas);
  int temDestaque = 0;
  int destaqueIndice = -1;
  nProxHome = 0;
  for (r = 0; r < nCat && destino < MAX_FIL - 1; r++) {
    const CatFileira *cf = cat_fileira(r);
    if (!cf) break;
    // `continueWatchingEnabled: false` tira a fileira da home inteira — nao a
    // esvazia, tira. E o que renderModernHomeLayout faz quando
    // computeContinueWatchingRenderState devolve a fileira desligada.
    if (!strcmp(cf->chave, "continue_watching") && !ajustes_cw_ligado()) continue;
    // "Recursos sociais" desligado: a fileira de amigos nao existe.
    if (!strcmp(cf->chave, "social_activity") && !ajustes_social()) continue;
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
    } else if (!temDestaque && vitrineNaPrimeira() && cf->n > 0 && cf->base[0]
               && cf->catId[0]) {
      fileiras[destino].tipo = FILEIRA_DESTAQUE;
      temDestaque = 1;
      destaqueIndice = destino;
    }
    // MAX_CARDS - 1: a ultima coluna e do card "Ver tudo". Sem reservar, uma
    // fileira cheia empurraria o card para fora do vetor de animacao.
    // "Itens por fileira" (#163), no maximo DESC_ITENS_POR_FILEIRA, que tem de
    // caber em MAX_CARDS - 1. Diminuir vale na hora (corta aqui); aumentar,
    // quando os catalogos forem pedidos de novo.
    { int teto = ajustes_itens_fileira();
      if (teto > DESC_ITENS_POR_FILEIRA) teto = DESC_ITENS_POR_FILEIRA;
      fileiras[destino].n = cf->n > teto ? teto : cf->n; }
    // UMA COLUNA A MAIS: o card "Ver tudo" no fim. So em fileira que veio de um
    // CATALOGO de addon — "Continuar assistindo" e as listas do Trakt nao tem
    // continuacao para pedir (o base fica vazio nelas).
    // Um catalogo pode responder vazio validamente. Mantemos o titulo da
    // fileira como estado visivel, mas nao inventamos um card "Ver tudo" sem
    // nenhum item para representar a consulta.
    fileiras[destino].verTudo = (cf->n > 0 && cf->base[0] && cf->catId[0]) ? 1 : 0;
    snprintf(fileiras[destino].base,  sizeof fileiras[destino].base,  "%s", cf->base);
    snprintf(fileiras[destino].catId, sizeof fileiras[destino].catId, "%s", cf->catId);
    snprintf(fileiras[destino].catTipo, sizeof fileiras[destino].catTipo, "%s", cf->tipo);
    fileiras[destino].ini = cf->ini;
    if(!strcmp(cf->chave,"social_activity"))fileiras[destino].tipo=FILEIRA_SOCIAL;
    // UM TITULO, UM LUGAR (cwretido.h): o que o cartao da ilha ou a faixa
    // "Retomar agora" segura nao entra aqui. So a lista desenhada muda; o
    // catalogo continua com o item. Passa pela janela INTEIRA (cf->n), nao so
    // pelo teto ja cortado, para a fileira nao encolher um card a toa.
    if (!strcmp(cf->chave, "continue_watching") &&
        (ajustes_cw_ordem() == CWO_SEPARAR || cw_retido()[0])) {
      Fileira *cw = &fileiras[destino];
      int c, nm = 0, tirou = 0, teto = cw->n;
      for (c = 0; c < cf->n && nm < teto && nm < MAX_CARDS; c++) {
        int idx = cf->ini + c;
        const CatItem *it = cat_item(idx);
        if (it && cw_retido_exclui(it->imdb)) { tirou = 1; continue; }
        if (it && ajustes_cw_ordem() == CWO_SEPARAR && cwo_e_futuro(it->imdb)) {
          if (nProxHome < MAX_CARDS) proxHome[nProxHome++] = idx;
          continue;
        }
        cw->itens[nm++] = idx;
      }
      if (nProxHome || tirou) { cw->usaItens = 1; cw->n = nm; }
    }
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
      char chaveGrupo[192];
      if(!folder||!folder->group[0])continue;
      col_chave_pasta(folder,chaveGrupo,sizeof chaveGrupo);
      for(int j=0;j<destino;j++) {
        if(!strcmp(fileiras[j].chave,chaveGrupo)){grupoVisto=1;break;}
      }
      if(grupoVisto)continue;
      { Fileira v={0};
        v.n=col_grupo_chave(chaveGrupo,v.folders,MAX_CARDS);
        if(!v.n)continue;
        v.tipo=FILEIRA_CATALOGOS;
        v.forma=col_grupo_forma_chave(chaveGrupo);
        snprintf(v.chave,sizeof v.chave,"%s",chaveGrupo);
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
    fileiras[i].fator = fil_tipo_fator(t);
    fileiras[i].tipoAuto = fileiras[i].tipo;
    fileiras[i].formaAuto = fileiras[i].forma;
    // Grupo de colecao: a escolha troca a FORMA da pasta e o tipo fica — e ele
    // que faz a fileira abrir colecoes em vez de titulos. Os numeros sao os de
    // fil_estilos (fileiras.c).
    if (fileiras[i].tipo == FILEIRA_CATALOGOS) {
      if (t == FIL_TIPO_COLECAO) fileiras[i].forma = COL_FORMA_PAISAGEM;
      else if (t == FIL_TIPO_DESTAQUE_QUADRADO) fileiras[i].forma = COL_FORMA_QUADRADO;
      else if (t == FIL_TIPO_CARTAZ) fileiras[i].forma = COL_FORMA_POSTER;
    } else if (t != FIL_TIPO_AUTO) fileiras[i].tipo = tipoDaEscolha(t);
  }
  if (ajustes_home_layout() == HOME_LAYOUT_DINAMICA) dinAtribuirTipos(destino);
  for(int i=0;i<destino;i++) {
    Fileira *s=&fileiras[i];s->stackN=0;
    if(s->tipo==FILEIRA_TOP10 && s->base[0] && s->catId[0]) {
      s->stackN=s->n;s->n=1;s->verTudo=0;
    }
  }
  int socialExiste=0;
  for(int i=0;i<destino;i++)if(fileiras[i].tipo==FILEIRA_SOCIAL)socialExiste=1;
  if(!socialExiste && destino<MAX_FIL && ajustes_social()) {
    int pos=destino>0?1:0;
    memmove(fileiras+pos+1,fileiras+pos,(destino-pos)*sizeof *fileiras);
    Fileira *s=&fileiras[pos];memset(s,0,sizeof *s);
    s->tipo=FILEIRA_SOCIAL;s->ini=-1;s->n=1;
    snprintf(s->titulo,sizeof s->titulo,"Amigos assistindo");
    snprintf(s->chave,sizeof s->chave,"social_activity");destino++;
  }
  // Um retorno do player e contexto, nao catalogo: entra acima das fileiras e
  // desaparece quando nao existe sessao incompleta. Nao duplica dados nem faz
  // rede; aponta para o item que o player acabou de atualizar em memoria.
  if (retomarId[0]) retomarIndice = cat_indice_por_imdb(retomarId);
  // Com o relogio ligado a ilha mostra a mesma sessao (ilhacart.c, mesmo
  // criterio home_retorno_vale): a faixa repetiria o cartao, entao sai.
  if (retomarIndice >= 0 && destino < MAX_FIL && !ajustes_relogio_ligado()) {
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
    // Account order includes collection groups, which are assembled here and
    // never pass through discovery's catalogue ordering. Preserve app rows,
    // then let the local TV order override this complete account projection.
    {
      const char *rem[MAX_FIL];
      int slots[MAX_FIL], ordenados[MAX_FIL], nr = 0;
      for (q = 0; q < destino; q++)
        if (fileiras[q].tipo == FILEIRA_CATALOGOS || fileiras[q].base[0]) {
          slots[nr] = q; rem[nr++] = fileiras[q].chave;
        }
      int n = catordem_unir(rem, nr, ordenados, MAX_FIL);
      memcpy(arranjo, fileiras, sizeof(Fileira) * (size_t)destino);
      for (q = 0; q < n; q++) arranjo[slots[q]] = fileiras[slots[ordenados[q]]];
      memcpy(fileiras, arranjo, sizeof(Fileira) * (size_t)destino);
    }
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
    // #392: fil_unir projeta apenas as chaves presentes. Sem reservar as
    // anteriores, CW sobe na Home parcial e desce a cada resposta da rede.
    // Reusa a fileira vazia (titulo, zero colunas), sem dados nem GET novos.
    int temCw = 0;
    for (int i = 0; i < destino; i++)
      if (!strcmp(fileiras[i].chave, "continue_watching")) temCw = 1;
    if (reservaEstado != 2 && temCw && fil_tem_ordem() && !fil_oculta("continue_watching")) {
      int cw = -1;
      for (int i = 0; i < fil_n(); i++)
        if (!strcmp(fil_chave(i), "continue_watching")) { cw = i; break; }
      for (int i = 0; i < cw && destino < MAX_FIL; i++) {
        const char *key = fil_chave(i);
        if (fil_linha_origem(i) == FIL_ORIGEM_APP || fil_estado(i) != FIL_NA_HOME) continue;
        int existe = 0;
        for (int j = 0; j < destino; j++) if (!strcmp(fileiras[j].chave, key)) existe = 1;
        if (existe) continue;
        if (!reservaEstado) { reservaDesde = SDL_GetTicks(); reservaEstado = 1; }
        Fileira *f = &fileiras[destino];
        memset(f, 0, sizeof *f);
        snprintf(f->chave, sizeof f->chave, "%s", key);
        snprintf(f->titulo, sizeof f->titulo, "%s", fil_titulo(i));
        f->ini = -1;
        f->tipoAuto = FILEIRA_NORMAL;
        int t = fil_tipo(key);
        f->tipo = t == FIL_TIPO_AUTO ? FILEIRA_NORMAL : tipoDaEscolha(t);
        f->escala = fil_escala(key); f->fator = fil_tipo_fator(t);
        ch[destino++] = f->chave;
      }
    }
    // "Retomar agora" e contexto do player, nao fileira de catalogo: fica presa
    // no topo, fora da ordem e fora do liga/desliga. Ela aparece por causa de
    // uma sessao interrompida e desaparece sozinha; deixar a pessoa mover ou
    // desligar uma fileira que ela nao controla seria um ajuste fantasma.
    if (destino > 0 && !strcmp(fileiras[0].chave, "last_session"))
      arranjo[w++] = fileiras[0];
    q = fil_unir(ch, destino, ord, MAX_FIL);
    for (k = 0; k < q && w < MAX_FIL; k++) {
      Fileira *f = &fileiras[ord[k]];
      int j, repetida = 0;
      if (!strcmp(f->chave, "last_session")) continue;
      if (fil_oculta(f->chave)) continue;
      if (f->tipo == FILEIRA_CATALOGOS && catordem_oculta(f->chave, f->chave)) continue;
      // UMA FILEIRA POR CHAVE (issue #127, "Proximos episodios" duplicada na
      // C9 do dono). A chave e a identidade da fileira em todo o resto — foco,
      // rolagem, registro, ordem —, e duas com a mesma chave desenhariam o
      // mesmo conteudo duas vezes. Nenhum caminho medido produz isso, mas a
      // home tem de ser idempotente sobre o que chega; e o log diz de onde
      // veio se voltar a acontecer.
      for (j = 0; j < w && !repetida; j++) repetida = !strcmp(arranjo[j].chave, f->chave);
      if (repetida) {
        printf("[home] fileira repetida descartada: %s\n", f->chave);
        continue;
      }
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
        // FILA NAO DESENHA. Editor e Home usam a mesma cota de catalogos;
        // colecoes e fixas ficam fora dela. Catalogo na fila ou fora nao
        // entra; abre vaga sozinho quando o estado muda na remontagem.
        if (pedeRede) {
          int est = fil_estado_chave(arranjo[q2].chave);
          if (est == FIL_NA_FILA || est == FIL_FORA) continue;
        }
        if (pedeRede && ++rede > lim) continue;
        if (mantidas != q2) arranjo[mantidas] = arranjo[q2];
        mantidas++;
      }
      cortadasPeloLimite = w - mantidas;
      w = mantidas; }
    memcpy(fileiras, arranjo, sizeof(Fileira) * (size_t)w);
    destino = w;
  }
  // STREAMING NA BARRA (so no layout Dinamica; dono, 01/10/2026, foto do app
  // da Apple TV): a fileira de colecao do grupo "Streaming" (Netflix, Prime
  // Video...) sai da home e as pastas dela viram uma secao da barra aberta
  // (menu.c). Depois do corte acima de proposito: fileira que a pessoa
  // desligou em Ajustes nao reaparece na barra. Ordem = a da fileira.
  nStreamBarra = 0;
  if (layoutHome() == HOME_LAYOUT_DINAMICA) {
    int q;
    for (q = 0; q < destino; q++) {
      if (fileiras[q].tipo != FILEIRA_CATALOGOS ||
          strncmp(fileiras[q].chave, "collection_", 11) ||
          strcasecmp(fileiras[q].titulo, "Streaming")) continue;
      nStreamBarra = fileiras[q].n < MAX_CARDS ? fileiras[q].n : MAX_CARDS;
      memcpy(streamBarra, fileiras[q].folders, sizeof(int) * (size_t)nStreamBarra);
      memmove(fileiras + q, fileiras + q + 1, sizeof(Fileira) * (size_t)(destino - q - 1));
      destino--;
      printf("[home] Streaming na barra (Dinamica): %d pastas\n", nStreamBarra);
      break;
    }
  }
  // "PROXIMOS EPISODIOS" LOGO ABAIXO DA RETOMADA (issue #127) — e onde o web
  // poe a `upcoming_section`. Mesma forma, tamanho e tipo da retomada (a copia
  // leva o que fil_tipo/fil_escala decidiram para ela). Retomada escondida pela
  // pessoa leva esta junto; retomada que ficou SO com futuros da o lugar a esta,
  // em vez de sobrar um cabecalho sem card.
  if (nProxHome) {
    int c = -1, q, ja = 0;
    for (q = 0; q < destino; q++) {
      if (c < 0 && !strcmp(fileiras[q].chave, "continue_watching")) c = q;
      if (!strcmp(fileiras[q].chave, "upcoming_section")) ja = 1;
    }
    if (c >= 0 && !ja) {
      Fileira u = fileiras[c];
      u.n = nProxHome;
      memcpy(u.itens, proxHome, sizeof(int) * (size_t)nProxHome);
      u.usaItens = 1;
      u.verTudo = 0;
      snprintf(u.titulo, sizeof u.titulo, "%s", "Pr\xc3\xb3ximos epis\xc3\xb3""dios");
      snprintf(u.chave, sizeof u.chave, "upcoming_section");
      if (fileiras[c].n < 1) fileiras[c] = u;
      else if (destino < MAX_FIL) {
        memmove(fileiras + c + 2, fileiras + c + 1, sizeof(Fileira) * (size_t)(destino - c - 1));
        fileiras[c + 1] = u;
        destino++;
      }
    }
  }
  nFileiras = destino;
  retomarAplicada = retomarRev;
  ultimaRevisao = revisao;
  filsAplicadas = nCat;
  prefsAplicadas = assin;
  memset(animFoco, 0, sizeof animFoco);
  memset(revArte, 0, sizeof revArte);
  memset(velX, 0, sizeof velX);
  memset(scrollX, 0, sizeof scrollX);
  memset(bordaFil, 0, sizeof bordaFil);
  for (r = 0; r < nFileiras; r++)
    for (int a = 0; a < nAntigas; a++)
      if (!strcmp(fileiras[r].chave, antigas[a].chave)) {
        if(fileiras[r].tipo==FILEIRA_TOP10 && antigas[a].tipo==FILEIRA_TOP10 &&
           !antigas[a].stackN && antigas[a].verTudo && fileiras[r].stackN) {
          fileiras[r].n=fileiras[r].stackN;
          fileiras[r].stackN=0;fileiras[r].verTudo=1;
        }
        break;
      }
  expFileira = expColuna = -1; expAbre = 0.0f;
  homeAtualizarReferenciasArte();
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

// A HOME DE OUTRO PERFIL ABRE NO TOPO, como no arranque: destaque em foco,
// fileira 0 no primeiro cartaz, sem rolagem, e a memoria de posicao da sessao
// esquecida — ela e das fileiras do perfil anterior, e a proxima remontagem a
// reaplicaria por chave (Continuar assistindo, Em alta...) no perfil novo.
void home_ir_topo(void) {
  int r;
  focoHero = 1;
  foco.fileira = 0;
  foco.coluna = 0;
  memset(foco.colunaLembrada, 0, sizeof foco.colunaLembrada);
  for (r = 0; r < MAX_FIL; r++) { scrollX[r] = 0.0f; velX[r] = 0.0f; }
  scrollY = 0.0f;
  velY = 0.0f;
  nPosViva = 0;
  posVivaFoco[0] = 0;
  posVivaCol = 0;
  sairPerguntadoEm = 0;
  heroUltTecla = SDL_GetTicks();
}

// A HOME SE MONTA POR TRAS DA ESCOLHA DE PERFIL (app.c), mas NAO E PINTADA. Sem
// isto o carrossel girava no vazio: marcava o proximo desejo a cada 7 s, o
// desenho (quem efetiva a troca) nunca rodava e o desejo ficava preso — medido
// na C9 (08/10, 2.0.3): 38 min na escolha de perfil, 0 linhas `[hero] espera` e
// 327 de `pre-busca nao saiu: desejado=322`. Escondida, vale o mesmo que o
// detalhe/player na frente: o hero nao muda e o relogio e rearmado na volta.
static int homeOculta;
void home_oculta(int oculta) { homeOculta = oculta ? 1 : 0; }

// Pede (uma vez por posicao) a arte do hero de heroAtual+-1, em prioridade
// baixa. Barato no quadro: depois de pedidos os dois, so compara dois inteiros;
// enquanto a URL nao resolve (catalogo chegando) tenta no maximo a cada 250 ms.
static void heroAquecerVizinhos(Uint32 agora) {
  int n = heroNLista(), pos, d;
  if (heroAtual != heroVizBase) { heroVizBase = heroAtual; heroVizFeito = 0; heroVizTentaEm = 0; }
  if (n < 2 || heroVizFeito == 3) return;
  if (heroVizTentaEm && agora - heroVizTentaEm < 250) return;
  heroVizTentaEm = agora ? agora : 1;
  pos = heroPosDe(heroAtual);
  if (pos < 0) return;
  for (d = -1; d <= 1; d += 2) {
    const int bit = d < 0 ? 1 : 2;
    int viz = heroIdxEm((pos + d + n) % n);
    const char *arte;
    if (heroVizFeito & bit) continue;
    if (viz < 0 || viz == heroAtual || (d > 0 && n == 2)) { heroVizFeito |= bit; continue; }
    arte = arte_por_identidade(viz, 2);
    if (!arte) continue;
    heroVizFeito |= bit;
    (void)tex_obter_hero_quente(arte);
    printf("[hero] pre-busca vizinho %s hash=%08lx\n", d < 0 ? "anterior" : "proximo",
           tex_hash_public(arte));
    fflush(stdout);
  }
}

void home_atualizar(float dt, Uint32 agora) {
  sincronizarFileiras();
  // A FILEIRA DE AMIGOS tem tantas colunas quantos rostos (+ "Adicionar"), e
  // isso muda sem o catalogo mudar (um amigo novo chega do recomenda.c).
  for (int r = 0; r < nFileiras; r++) {
    if (fileiras[r].tipo != FILEIRA_SOCIAL) continue;
    int focada = !focoHero && foco.fileira == r, c = foco.coluna;
    amigosfil_atualizar(dt, focada, focada ? &c : NULL);
    int nc = amigosfil_n_colunas();
    // A FILEIRA DO CATALOGO QUE RESPONDEU VAZIA (estado 1) continua sem
    // coluna quando nao ha amigo nenhum: o convite so mora no lugar reservado
    // (ini < 0), e um cabecalho vazio nao recebe foco (tests/fimfileira).
    if (amigosfil_convite() && fileiras[r].ini >= 0) {
      for (int q = 0; q < cat_n_fileiras(); q++) {
        const CatFileira *cf = cat_fileira(q);
        if (cf && !strcmp(cf->chave, "social_activity")) { if (cf->n == 0) nc = 0; break; }
      }
    }
    if (fileiras[r].n != nc || fileiras[r].verTudo) {
      fileiras[r].n = nc; fileiras[r].verTudo = 0;
      if (r < FOCUS_MAX_FILEIRAS) foco.nColunas[r] = nc;
    }
    if (focada) {
      if (c >= nc) c = nc - 1;
      if (c < 0) c = 0;
      foco.coluna = c;
      if (r < FOCUS_MAX_FILEIRAS) foco.colunaLembrada[r] = c;
    }
  }
  // Primeira batida da home viva: ancora o ocio do carrossel no "agora", nao
  // no zero do BSS (ver nota em heroUltTecla).
  if (!heroUltTecla) heroUltTecla = agora;

  // A Home continua atualizando catalogo e progresso enquanto uma tela de
  // detalhe/player esta na frente, mas o hero nao pode mudar escondido. Se a
  // troca vencer nesse intervalo, a volta exibiria uma arte que nunca foi
  // observada e a seleção de logo poderia divergir do detalhe que acabou de
  // sair. O relógio é rearmado na volta, preservando a escolha visível.
  const int heroOculto = detail_aberto() || player_aberto() || homeOculta;
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
    abrirMenuCartaz();
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
      heroDesliza = 1.0f;
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
      heroDirDesejado = 0;   // seguir o cartaz nao e carrossel: esvanece
    } else if (alvo < 0 && agora >= heroTrocaEm &&
               (!focoHero || agora - heroUltTecla >= HOME_HERO_OCIO_MS) &&
               !heroAutoDesligado &&
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
      heroDirDesejado = 1;   // o carrossel sempre anda para o proximo
      heroTrocaEm = agora + NV_HERO_INTERVALO_MS;
    }
    // PRE-BUSCA (ver heroPreItem): as MESMAS condicoes da troca automatica
    // acima, so que NV_HERO_PRE_MS antes — com UMA diferenca, de proposito: o
    // trailer SENDO PROCURADO ou preparado nao impede a pre-busca. Medido na C9
    // (08/10, 2.0.3): nenhuma linha `pre-busca` em duas sessoes, porque cada
    // titulo do carrossel tem a busca do trailer (`[trailer] apple ...`,
    // `[trailer] imdb ...`) e heroTrailerSegurando segura o carrossel durante
    // ela; quando a busca termina, heroTrocaEm ja venceu e a troca sai no
    // mesmo quadro, sem a janela de 3 s. Esse tempo de busca e curto e limitado
    // (heroTrailerMaxEspera), entao pre-buscar nele custa uma arte a mais por
    // poucos segundos. Com o trailer TOCANDO nada: ele pode durar minutos e a
    // arte ficaria ocupando memoria parada.
    // Hero no comando e nenhuma virada em curso: aquece os vizinhos (ver
    // heroVizBase). Com card em foco (alvo>=0) o hero segue o card, e durante a
    // virada (heroDesejado>=0) o pedido de quem esta chegando e que manda.
    if (alvo < 0 && heroDesejado < 0 && !homeOculta) heroAquecerVizinhos(agora);
    heroPreItem = -1;
    { const int janela = agora + NV_HERO_PRE_MS >= heroTrocaEm;
      const int seg = heroTrailerSegurando(agora);
      const int tocando = heroTrailerTocando();
      if (alvo < 0 && heroDesejado < 0 && !heroAutoDesligado && janela &&
          (!focoHero || agora + NV_HERO_PRE_MS - heroUltTecla >= HOME_HERO_OCIO_MS) &&
          (!seg || !tocando)) {
        int total = heroNLista();
        int pos = heroPosDe(heroAtual);
        int proximo = total > 0 ? heroIdxEm(((pos < 0 ? 0 : pos) + 1) % total) : 0;
        if (proximo < 0) proximo = 0;
        if (proximo != heroAtual) heroPreItem = proximo;
      }
      // DIAGNOSTICO: a janela abriu e a pre-busca nao saiu — diz quem a
      // impediu. Sai quando o MOTIVO muda (e no maximo a cada 5 s) ou, com o
      // mesmo motivo, uma vez por minuto: na C9 eram 327 linhas em 38 min.
      if (janela && heroPreItem < 0) {
        const unsigned sig = (unsigned)(alvo >= 0) | (unsigned)(heroDesejado >= 0) << 1 |
                             (unsigned)heroAutoDesligado << 2 | (unsigned)seg << 3 |
                             (unsigned)tocando << 4 | (unsigned)focoHero << 5;
        const Uint32 desde = agora - heroPreDiagEm;
        if (heroPreDiagEm == 0 || (sig != heroPreDiagSig && desde >= 5000) || desde >= 60000) {
          heroPreDiagSig = sig; heroPreDiagEm = agora ? agora : 1;
          printf("[hero] pre-busca nao saiu: alvo=%d desejado=%d autoDesligado=%d "
                 "segurando=%d tocando=%d focoHero=%d ocio=%d lista=%d\n",
                 alvo, heroDesejado, heroAutoDesligado, seg, tocando, focoHero,
                 (int)(agora - heroUltTecla), heroNLista());
          fflush(stdout);
        }
      }
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
    // MENU DO CARTAZ ABERTO = O CARTAO NAO EXPANDE (dono, 03/10: "se esta com
    // o menu contextual aberto ele nao expande"). O relogio nao anda e a mola
    // fica onde esta: o que ja abriu continua aberto, o que nao abriu espera.
    // Ao fechar, a contagem do atraso recomeca do zero — sem salto.
    if (ctx_aberto() || (okPressionando && okHold > 0.0f)) {
      if (expAbre < 0.999f) expDesde = agora;
    } else
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
  if (heroDesliza < 1.0f) {
    heroDesliza = motionReduzido ? 1.0f
                : heroDesliza + dt * (1000.0f / NV_HERO_DESLIZA_MS);
    if (heroDesliza > 1.0f) heroDesliza = 1.0f;
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
    alvoY = -empurraHero();
  } else {
    int r = foco.fileira;
    for (int i = 0; i < r && i < nFileiras; i++)
      alvoY += NV_LEGACY_ROW_HEAD_H + alturaTotalFil(i) + fileiraGap();
    if (layoutHome() == HOME_LAYOUT_DINAMICA) alvoY = dinRolagemCentrada(r, alvoY);
  }
  scrollY = anim_mola2_reduzida(&velY, scrollY, alvoY, dt,
                                NV_MOLA2_SCROLL, motionReduzido);
  for (int r = 0; r < nFileiras && r < MAX_FIL; r++)
    (void)anim_borda_passo(&bordaFil[r], dt, motionReduzido);
  (void)anim_borda_passo(&bordaPag, dt, motionReduzido);

  // Mantem um snapshot vivo para a proxima publicacao incremental. O
  // sincronizador ainda usa posCapturar como fallback nos testes/caminhos que
  // nao passam por este loop, mas no arranque normal este snapshot e tomado
  // antes de um worker trocar o bloco de catalogo.
  posCapturar();

}

// ---------- Fundo da Dinamica: SO COR ------------------------------------------
//
// O fundo antigo (a arte do titulo desfocada por tras das fileiras, mais a
// prateleira de vidro na fileira em foco) custava demais na C9 e saiu. O que
// ficou e uma COR: um degrade vertical de uma cor so, tirada da paleta que a
// cor viva (corviva.c) JA extraiu da arte do titulo em foco — nenhum decode e
// nenhum assado a mais, e um unico quad opaco por quadro (gfx_fundo_din_desenhar).
// A cor CRUZA da anterior para a nova (~0,45 s), ou corta seco com animacoes
// reduzidas. Sem paleta (arte cinza, ou ainda nao decodificada) mantem a cor
// que estava; sem NENHUMA, o cinza de sempre.
//
// A paleta e a do destaque: so a arte pedida no teto do hero e anotada. Com o
// destaque desligado a fonte e a arte do primeiro cartao da fileira em foco, e
// ela so tem cor se ja foi vista como destaque (corviva.txt lembra) — nao se
// decodifica arte grande so para tingir o fundo.
#define DIN_TINTA     0.24f   // quanto da cor do titulo entra no topo do fundo
#define DIN_QUEDA     0.34f   // o que sobra dessa cor na base da tela
#define DIN_TROCA_TAU 0.11f   // s; ~0,45 s para assentar
static char  dinColArte[512];   // banner de colecao mostrado ("" = nenhum), ver dinArteColecao
static float dinColAlfa = 0.0f;
static float dinCor[3] = { 0.051f, 0.051f, 0.051f };
static float dinAlvo[3] = { 0.051f, 0.051f, 0.051f };
static Uint32 dinUlt = 0;
static void dinCorDaArte(const char *arte) {
  CorvivaPaleta p;
  if (!arte || !arte[0] || !corviva_paleta(arte, &p)) return;   // mantem a cor
  for (int k = 0; k < 3; k++) {
    float base = 0.051f;   // #0D0D0D
    dinAlvo[k] = p.ok ? base + (p.acento[k] - base) * DIN_TINTA : base;
  }
}
// A arte de onde a cor sai: a do destaque no ar, ou (sem destaque) a do
// primeiro cartao da fileira em foco — a mesma escolha de antes, sem textura.
static const char *dinArteDaCor(void) {
  const Fileira *f;
  int idx, deitado;
  if (dinColArte[0] && dinColAlfa > 0.3f) return dinColArte;   // colecao em foco
  if (ajustes_hero_ligado()) return arte_por_identidade(heroAtual, 2);
  if (foco.fileira < 0 || foco.fileira >= nFileiras) return NULL;
  f = &fileiras[foco.fileira];
  if (f->tipo == FILEIRA_SOCIAL || f->tipo == FILEIRA_CATALOGOS) return NULL;
  idx = fileiraItemIndice(f, 0);
  if (idx < 0) return NULL;
  deitado = editorial(f->tipo) || f->tipo == FILEIRA_LARGA ||
            f->tipo == FILEIRA_CONTINUE || f->tipo == FILEIRA_RETORNO ||
            ajustes_posteres_deitados();
  return arte_por_identidade(idx, deitado);
}
// O BANNER DA COLECAO NA DINAMICA. Com o foco num grupo de colecao o destaque
// ja rolou para fora, entao o banner da pasta focada entra por tras do cabecalho
// da fileira: a arte de cima da tela, dissolvida para o fundo (o mesmo
// GFX_VITRINE do destaque, sem canto e sem moldura). Um quad so, e so enquanto a
// fileira de colecao esta em foco; a arte sai do cache de destaque, que ja
// anota a cor dela para o fundo (dinArteDaCor).
#define DIN_COL_H 560.0f
static const char *dinColQuer(void) {
  const ColFolder *f;
  if (focoHero || foco.fileira < 0 || foco.fileira >= nFileiras) return "";
  if (fileiras[foco.fileira].tipo != FILEIRA_CATALOGOS) return "";
  if (foco.coluna < 0 || foco.coluna >= fileiras[foco.fileira].n) return "";
  f = col_folder(fileiras[foco.fileira].folders[foco.coluna]);
  return f ? col_banner(f) : "";
}
static void dinArteColecao(float dt) {
  const char *quer = dinColQuer();
  int reduz = ajustes_animacoes_reduzidas();
  if (strcmp(quer, dinColArte)) {   // outra pasta: apaga a que esta e troca no zero
    dinColAlfa = reduz ? 0.0f : dinColAlfa - dt * 4.0f;
    if (dinColAlfa <= 0.0f) {
      dinColAlfa = 0.0f;
      snprintf(dinColArte, sizeof dinColArte, "%s", quer);
    }
  } else if (dinColArte[0]) {
    dinColAlfa = reduz ? 1.0f : dinColAlfa + dt * 3.0f;
    if (dinColAlfa > 1.0f) dinColAlfa = 1.0f;
  } else dinColAlfa = 0.0f;
  if (dinColArte[0] && dinColAlfa > 0.004f) {
    GLuint t = tex_obter_hero(dinColArte);
    if (t) {
      GfxRect r = { 0.0f, 0.0f, NV_TELA_W, DIN_COL_H };
      gfx_tex_aspect_atual = tex_aspecto(dinColArte);
      gfx_rect(r, t, GFX_VITRINE, 0.92f, 0.28f, 1.0f, 0.0f, 0.0f, 0, 0,
               dinColAlfa * (1.0f - trailercinema_t(&heroCinema)));
      gfx_tex_aspect_atual = 0.0f;
    }
  }
}
static void desenhaFundoDin(Uint32 agora) {
  float dt = dinUlt ? (float)(agora - dinUlt) / 1000.0f : 0.0f;
  if (dt > 0.1f) dt = 0.1f;
  dinUlt = agora ? agora : 1u;
  dinCorDaArte(dinArteDaCor());
  { float e = ajustes_animacoes_reduzidas() ? 1.0f : 1.0f - expf(-dt / DIN_TROCA_TAU);
    for (int k = 0; k < 3; k++) dinCor[k] += (dinAlvo[k] - dinCor[k]) * e; }
  gfx_fundo_din_desenhar(dinCor, DIN_QUEDA);
  dinArteColecao(dt);
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
int home_streaming_barra(const int **pastas) {
  if (pastas) *pastas = streamBarra;
  return layoutHome() == HOME_LAYOUT_DINAMICA ? nStreamBarra : 0;
}

// A pilula da barra (layout Dinamica, menu.c) so aparece com a pagina no
// topo: 1 com o destaque inteiro na tela, 0 depois de ~140 px de rolagem.
float home_topo_fracao(void) {
  float f;
  if (layoutHome() != HOME_LAYOUT_DINAMICA) return 1.0f;
  f = 1.0f + dinHeroY() / 140.0f;
  return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
}

void home_hero_rect(float *x, float *y, float *w, float *h) {
  GfxRect r = heroArteRect;
  // Dinamica com o destaque ja rolado para fora: a pagina do titulo cresce a
  // partir do CARTAO que abriu, e nao de uma faixa que ninguem ve.
  if (layoutHome() == HOME_LAYOUT_DINAMICA && -dinHeroY() >= NV_DIN_ARTE_FADE_B - 60.0f && temItemFoco)
    r = itemFoco.rect;
  *x = r.x; *y = r.y; *w = r.w; *h = r.h;
}

// Only a language attached to this exact image confirms a foreign logo.
// An unknown language or a pending decode keeps the artwork slot stable.
static int heroNomeLogo(const CatItem *ci, const char *url, int loaded,
                        const char *language) {
  if (!url || !url[0]) return !loaded;
  if (!ci || !language || !ci->logoIdiomaUrl[0] || !ci->logoIdioma[0] ||
      !strcmp(ci->logoIdioma, "und")) return 0;
  const char *actual = strrchr(url, '/');
  const char *known = strrchr(ci->logoIdiomaUrl, '/');
  int same = !strcmp(url, ci->logoIdiomaUrl) ||
    (!strncmp(url, "https://image.tmdb.org/t/p/", 27) &&
     !strncmp(ci->logoIdiomaUrl, "https://image.tmdb.org/t/p/", 27) &&
     actual && known && !strcmp(actual, known));
  return same && strncmp(ci->logoIdioma, language, 2) != 0;
}

typedef struct {
  float logo, logoHeight, action, caption, friends, meta, secondary, synopsis;
} HeroCopyLayout;

// One bottom boundary, measured text and the same order for every layout:
// LOGO, then the information (caption, friends, meta, secondary, synopsis),
// then the ACTION at the bottom (owner 06/10, Apple TV reference: logo at the
// top-left, "type • genre • year", three lines of synopsis, a compact button).
// `slot` 1 = the button is (or may be) on screen and the text sits above it;
// `slot` 0 = no button (Moderna with focus on the rows): the button slot
// collapses and the whole block settles on `base`. The button rides right under
// the synopsis, so a fading button never overlaps the text. The logo size is
// computed with the full slot, so it never rescales while the slot moves.
static HeroCopyLayout heroCopyLayout(float base, float hSin, int hasMeta,
                                     int hasSec, float captionH, float logoH,
                                     float btnH, float btnGap, float minTop,
                                     float slot, float friendsH) {
  HeroCopyLayout p;
  float s = slot < 0.0f ? 0.0f : slot > 1.0f ? 1.0f : slot;
  float slotH = btnH + btnGap;
  p.synopsis = base - slotH * s - hSin;
  p.action = p.synopsis + hSin + btnGap;
  p.secondary = p.synopsis - (hasSec ? (hSin > 0 ? NV_HERO_COPY_LINHA : 0) + NV_LD_HERO_SEC : 0);
  p.meta = p.secondary - (hasMeta ? ((hasSec || hSin > 0) ? NV_HERO_COPY_LINHA : 0) + NV_LD_HERO_META : 0);
  // A linha de AMIGOS (quem gostou / assistiu) fica logo acima da meta.
  p.friends = p.meta - (friendsH > 0 ? NV_HERO_COPY_LINHA + friendsH : 0);
  p.caption = p.friends - (captionH > 0 ? NV_HERO_COPY_LINHA + captionH : 0);
  // Logo bottom one gap above the first information line.
  float bottom = p.caption - btnGap;
  float bottomCheio = bottom - slotH * (1.0f - s);   // where it is with the slot open
  p.logoHeight = fminf(logoH, fmaxf(48.0f, bottomCheio - minTop));
  p.logo = bottom - p.logoHeight;
  return p;
}

// PONTOS DE PAGINA DO DESTAQUE (dono, 06/10): um ponto por titulo, o atual
// vira uma pilula mais larga, e as setas < > dos lados. `xDir` e a borda
// direita do conjunto, `yc` o centro vertical. A posicao do realce anda com
// mola (animacoes reduzidas: salta); uma volta do ultimo para o primeiro
// salta tambem, para nao varrer a fileira inteira. Mais de 12 titulos: uma
// janela de 12 em volta do atual. Cada ponto e um gfx_cor (pilula de canto
// 0,5): ~16 retangulos pequenos, nada de tela cheia.
#define HERO_PONTOS_MAX 12
static float heroPontosPos = -1.0f;
static Uint32 heroPontosT;
static void desenhaPontosHero(float xDir, float yc, int n, int atual, float a) {
  const float d = 8.0f, gap = 10.0f, larga = 26.0f, seta = 22.0f, folga = 14.0f;
  Uint32 agora = SDL_GetTicks();
  float dt = heroPontosT ? (float)(agora - heroPontosT) / 1000.0f : 0.0f;
  int m, ini, i;
  float w, x;
  heroPontosT = agora;
  if (n <= 1 || a <= 0.004f) return;
  if (dt > 0.1f) dt = 0.1f;
  if (heroPontosPos < 0.0f || ajustes_animacoes_reduzidas() ||
      fabsf(heroPontosPos - (float)atual) > 1.5f)
    heroPontosPos = (float)atual;
  else
    heroPontosPos += ((float)atual - heroPontosPos) * (1.0f - expf(-dt * 14.0f));
  m = n < HERO_PONTOS_MAX ? n : HERO_PONTOS_MAX;
  ini = atual - m / 2;
  if (ini > n - m) ini = n - m;
  if (ini < 0) ini = 0;
  w = (float)(m - 1) * (d + gap) + larga;
  x = xDir - seta - folga - w;
  gfx_icone((GfxRect){ x - folga - seta, yc - seta * 0.5f, seta, seta }, "pl_chevron-left",
            1.0f, 1.0f, 1.0f, 0.55f * a);
  gfx_icone((GfxRect){ xDir - seta, yc - seta * 0.5f, seta, seta }, "pl_chevron-right",
            1.0f, 1.0f, 1.0f, 0.55f * a);
  float dr, dg, db; botao_cor_foco(&dr, &dg, &db);
  for (i = 0; i < m; i++) {
    float k = 1.0f - fabsf((float)(ini + i) - heroPontosPos);
    float pw;
    if (k < 0.0f) k = 0.0f;
    pw = d + (larga - d) * k;
    gfx_cor((GfxRect){ x, yc - d * 0.5f, pw, d }, 0.5f,
            1.0f + (dr - 1.0f) * k, 1.0f + (dg - 1.0f) * k, 1.0f + (db - 1.0f) * k,
            (0.34f + 0.61f * k) * a);
    x += pw + gap;
  }
}

// O BLOCO DE TEXTO DE UM TITULO do destaque (logo, meta, selos, sinopse),
// ancorado pela base em `base` e a partir de `x`. Separado de desenhaHero para
// a troca deslizada desenhar DOIS: o do titulo que sai e o do que entra, cada
// um andando com a sua arte. `principal` 0 = o que sai: nao observa a selecao
// de logo da sessao nem manda na cor viva.
// A largura do logo do titulo no destaque, por layout. Partilhada pelo
// desenho e pelos pedidos antecipados (heroPedirLogo): o MESMO teto cai no
// MESMO item do cache, sem promocao nem segundo decode.
static float heroLogoMaxW(int lay, int cheio) {
  if (lay == HOME_LAYOUT_PADRAO) return NV_PAD_LOGO_MAX_W;
  if (lay == HOME_LAYOUT_DINAMICA) return NV_DIN_LOGO_MAX_W;
  return cheio ? NV_LOGO_HERO_CHEIO_MAX_W : NV_LOGO_HERO_MAX_W;
}

// O LOGO DO TITULO JUNTO COM A ARTE (07/10, TCL Android 2.0.2). O logo so era
// pedido por desenhaCopiaHero, e ela so desenha heroAtual — que so muda quando
// a arte nova fica pronta ou a espera estoura. O logo entrava na fila 600 ms
// depois da arte, no melhor caso (`pedido role=logo` sempre logo apos o
// `ESTOUROU` no log). Pedido aqui, ele baixa EM PARALELO com a arte. So pede:
// nao observa a sessao de logo (artehero_logo_sessao_observar), que e do
// titulo que esta na tela.
static void heroPedirLogo(const CatItem *ci, int lay, int cheio) {
  const char *url = ci ? artehero_logo_sessao(ci) : NULL;
  if (url && url[0]) (void)tex_obter_logo_larg_qualquer(url, heroLogoMaxW(lay, cheio));
}

static float desenhaCopiaHero(const CatItem *ci, int principal, float x,
                             float base, int lay, int cheio, float logoH,
                             float sinW, int sinLinhas, float aTexto,
                             float aCopy, float cin, float btnH, float btnGap,
                             float slot) {
  int contHero = (ci && ci->progresso > 0 && ci->restanteMin > 0);
  int seguirHero = (ci && ci->progresso == 0 && (trakt_e_a_seguir(ci->imdb) || simkl_e_a_seguir(ci->imdb) || cwo_conta_a_seguir(ci->imdb)));

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
             n ? "  \xe2\x80\xa2  " : "", ci->meta);
  }
  // TIPO • GENERO • ANO (dono, 06/10, referencia da Apple TV): o genero chega
  // como "Filme · Terror"; no destaque o separador e o mesmo ponto cheio da
  // linha toda, com o mesmo respiro.
  { char t[sizeof metaLinha]; size_t i = 0, o = 0;
    const char *de = "  \xc2\xb7  ", *para = "  \xe2\x80\xa2  ";
    size_t nd = strlen(de), np = strlen(para);
    while (metaLinha[i] && o + np < sizeof t) {
      if (!strncmp(metaLinha + i, de, nd)) { memcpy(t + o, para, np); o += np; i += nd; }
      else if (!strncmp(metaLinha + i, " \xc2\xb7 ", 4)) { memcpy(t + o, para, np); o += np; i += 4; }
      else t[o++] = metaLinha[i++];
    }
    t[o] = 0;
    memcpy(metaLinha, t, o + 1); }

  // Linha secundaria: destaque de progresso, selos e a nota do IMDb. O web so
  // mostra o IMDb aqui quando ja existe destaque ou selo (showImdbSecondary);
  // no outro caso ele vai para o fim da linha de meta.
  char destaque[64];
  destaque[0] = 0;
  if (contHero) snprintf(destaque, sizeof destaque, i18n("CONTINUAR DE ONDE PAROU  \xc2\xb7  %d MIN"),
                         ci->restanteMin);
  else if (seguirHero) {
    // O FUTURO DIZ QUANDO (issue #127): "ESTREIA 21 OUT", nao o "A SEGUIR" do
    // episodio que ja pode tocar. Mesma decisao e mesma data do card
    // (continuar.c), para os dois nao discordarem.
    char quando[32];
    if (cwo_e_futuro(ci->imdb) &&
        cwo_data_curta(cwo_estreia(ci->imdb), (long long)time(NULL) * 1000LL,
                       ajustes_idioma(), 1, quando, sizeof quando))
      snprintf(destaque, sizeof destaque, i18n("ESTREIA %s"), quando);
    else snprintf(destaque, sizeof destaque, "%s", i18n("A SEGUIR"));
  }
  const char *selo = (ci && ci->classificacao[0] && !contHero && !seguirHero) ? ci->classificacao : NULL;
  char nota[8];
  nota[0] = 0;
  int imdbRating = ci ? imdbnota_obter(ci->imdb, ci->nota, !strcmp(ci->tipo,"series")) : 0;
  if (imdbRating > 0) { snprintf(nota, sizeof nota, "%.1f", imdbRating / 10.0f); idioma_decimal_texto(nota, ajustes_idioma()); }
  int temSec = (destaque[0] || selo || nota[0]);

  const char *sinopse = (ci && ci->sinopse[0]) ? ci->sinopse : "";

  float hSin = sinopse[0] ? txt_bloco_corta(TXT_HERO_SIN, sinopse, 255, 255, 255, -1, 0,
                                            sinW, NV_LD_HERO_SIN, 0.0f, sinLinhas)
                          : 0.0f;

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
  // O titulo que SAI deslizando so e lido: quem observa e o que fica.
  const char *urlLogo = ci ? ((!principal || detail_aberto() || player_aberto())
                              ? artehero_logo_sessao(ci)
                              : artehero_logo_sessao_observar(ci)) : NULL;
  float maxWLogo = heroLogoMaxW(lay, cheio);
  // Durante a promoção para o hero, entregar a textura menor já pronta evita
  // um quadro vazio; o cache continua reprocessando para o teto final.
  GLuint tlogo = urlLogo ? tex_obter_logo_larg_qualquer(urlLogo, maxWLogo) : 0;
  // COR VIVA: o logo do mesmo titulo do destaque ("Cor da logo").
  if (urlLogo && principal) corviva_definir_logo(urlLogo, CORVIVA_HOME);
  // Igual ao detalhe: nome escrito so quando nao ha logo ou o cache ja falhou.
  // Antes, qualquer decode pendente caia no ramo de texto — ao voltar do
  // detalhe (catalogo com url nova do TMDB) parecia "sumiu a arte do titulo".
  int mostraNomeLogo = heroNomeLogo(ci, urlLogo && tex_falhou(urlLogo) ? NULL : urlLogo,
                                    tlogo != 0, desc_tmdb_idioma());
  int caption = mostraNomeLogo && urlLogo && !tex_falhou(urlLogo) && ci && ci->titulo[0];
  float captionH = caption ? txt_bloco_corta(TXT_HERO_META, ci->titulo, 255, 255, 255,
                                            -1, 0, sinW, NV_LD_HERO_META, 0, 1) : 0;
  // Padrão has a shorter banner: fit the logo into the measured remaining
  // space when every real information line exists, including a caption.
  float minTop = 24.0f;
  if (lay == HOME_LAYOUT_PADRAO)
    minTop = base - (NV_PAD_BANNER_H - NV_PAD_TEXTO_BASE) + 24.0f;
  else if (lay == HOME_LAYOUT_DINAMICA)
    minTop = base - (NV_DIN_HERO_H - NV_DIN_TEXTO_BASE) + 54.0f;
  AmigosTitulo amigos;
  int temAmigos = ci && ci->imdb[0] && amigostitulo_obter(ci->imdb, &amigos);
  float friendsH = temAmigos ? NV_AMIGOS_HERO_H : 0.0f;
  HeroCopyLayout copy = heroCopyLayout(base, hSin, metaLinha[0] != 0, temSec,
                                      captionH, logoH, btnH, btnGap, minTop, slot, friendsH);
  logoH = copy.logoHeight;
  float ySin = copy.synopsis, ySec = copy.secondary, yMeta = copy.meta;
  float logoY = copy.logo - cin * NV_CINEMA_DESCE;
  if (tlogo) {
    float ap = tex_aspecto(urlLogo);
    if (ap <= 0.0f) ap = 4.0f;
    float hTit = logoH, wTit = hTit * ap;
    if (wTit > maxWLogo) { wTit = maxWLogo; hTit = wTit / ap; }
    // Align the image to the slot base so actions sit directly below even
    // when a wide logo uses less than the maximum reserved height.
    GfxRect rl = { x, logoY + logoH - hTit, wTit, hTit };
    // MODO CINEMA: o logo ENCOLHE e ANDA ate o canto inferior esquerdo (o do
    // detalhe cruza-apaga, mas la o logo pequeno e outro desenho; aqui e o mesmo
    // logo, entao o caminho e continuo). Mesmas medidas de trailercinema.h.
    if (cin > 0.0f) {
      float wc, hc, fim;
      trailercinema_logo(ap, &wc, &hc);
      fim = trailercinema_base();
      rl.w = anim_mistura(wTit, wc, cin);
      rl.h = anim_mistura(hTit, hc, cin);
      rl.y = anim_mistura(logoY + logoH, fim, cin) - rl.h;
    }
    gfx_tex_aspect_atual = 0.0f;
    // Logo escuro vira branco. Mesma regra da tela de detalhe: o TMDB nao marca
    // claro/escuro, entao a decisao sai da luminancia MEDIDA (tex_luminancia).
    // Logo claro ou colorido passa intacto; -1 (ainda carregando) nao tinge.
    { GfxModo m = tex_marca_escura(urlLogo) ? GFX_MARCA : GFX_TEXTO;
      // O LOGO DO TITULO acompanha a ARTE, nao o texto. MEDIDO: 205 ms depois
      // da tecla a arte antiga ainda estava a 85% e o logo JA tinha sumido por
      // inteiro; ele so reaparece no mesmo quadro em que a arte nova entra.
      // OLED: com a opcao ligada o logo some enquanto o trailer toca (cin = 1) em vez
      // de ficar parado no canto; volta quando o trailer para.
      gfx_rect(rl, tlogo, m, 0, 0, 0, 0.0f, 1, 1, 1,
               aTexto * heroEntra * (ajustes_esconder_logo_trailer() ? 1.0f - cin : 1.0f)); }
  } else if (mostraNomeLogo && !caption) {
    // .legacy-webos .home-hero-title-text: 76px (components.css:19164), nao os
    // 56 do tema padrao.
    // Sem titulo NAO se inventa titulo. Aqui havia uma lista de demonstracao
    // ("Ruptura", "Silo", "Shrinking"...) que preenchia o hero com o nome de
    // outra serie quando o item ainda nao tinha nome — indistinguivel de dado
    // real para quem olha a tela. Mesma familia do elenco e da classificacao
    // que ja sairam do detalhe. Sem nome, o hero fica so com a arte, que ja
    // basta, e o texto aparece quando o dado chegar.
    if (ci && ci->titulo[0]) {
      TxtLinha tit = txt_linha_corta(TXT_TITULO1, ci->titulo, 255, 255, 255, 255, maxWLogo);
      txt_desenhar_alpha(tit, x, logoY + logoH - (float)tit.h,
                         aTexto * (1.0f - cin));
      // Sem logo, o nome pequeno entra embaixo (o mesmo do detalhe).
      if (cin > 0.005f) {
        TxtLinha t2 = txt_linha_corta(TXT_TITULO2, ci->titulo, 255, 255, 255, 255,
                                      NV_DETW_LOGO_MAXW * 0.5f);
        txt_desenhar_alpha(t2, x, trailercinema_base() - t2.h, ajustes_esconder_logo_trailer() ? 0.0f : aTexto * cin);
      }
    }
  }

  if (caption && aCopy > 0.004f)
    txt_bloco_corta(TXT_HERO_META, ci->titulo, 255, 255, 255, x, copy.caption,
                    sinW, NV_LD_HERO_META, aCopy, 1);

  if (temAmigos && aCopy > 0.004f) {
    static const float ANEL[3] = { 0.04f, 0.045f, 0.055f };
    char linha[200];
    float d = NV_AMIGOS_HERO_H, lx;
    lx = x + svd_amigos_pilha(x, copy.friends, d, &amigos, amigos.n < 3 ? amigos.n : 3, 1, ANEL, aCopy) + 14.0f;
    amigostitulo_linha_destaque(&amigos, linha, sizeof linha);
    if (linha[0]) {
      TxtLinha lf = txt_linha_corta(TXT_HERO_META, linha, 214, 217, 224, 255, sinW - (lx - x));
      txt_desenhar_alpha(lf, lx, copy.friends + (d - lf.h) * 0.5f, aCopy);
    }
  }

  if (metaLinha[0] && aCopy > 0.004f) {
    float badgeW=ci?badges_desenhar(badges_provedor(ci->provNome),x,yMeta,150,24,aCopy):0;
    TxtLinha lm = txt_linha_corta(TXT_HERO_META, metaLinha, 179, 179, 179, 255,
                                  sinW-badgeW);
    // META E SINOPSE TROCAM NA HORA, sem esvanecer com a arte. MEDIDO: no
    // quadro a 205 ms, com a arte antiga ainda a 85%, a linha de meta e a
    // sinopse ja eram as do titulo NOVO, com o texto opaco. Multiplicar por um
    // alfa de troca aqui era invencao nossa — e, com o rasterizador fazendo 2
    // linhas por quadro (text.c:40), esvanecer texto que ainda esta assentando
    // e o pior caso possivel.
    txt_desenhar_alpha(lm, x+badgeW, yMeta, aCopy);
  }

  if (temSec && aCopy > 0.004f) {
    float cx = x;
    float a = aCopy;
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
      if (cx + bw <= x + sinW)
        cx += badge_desenhar(cx, ySec + (NV_LD_HERO_SEC - BADGE_H) * 0.5f,
                             selo, BADGE_NEUTRO, a) + 14.0f;
    }
    if (nota[0] && imdbRating > 0) {
      float bw = badge_imdb_largura(imdbRating);
      if (cx + bw <= x + sinW)
        badge_imdb(cx, ySec + (NV_LD_HERO_SEC - BADGE_H) * 0.5f,
                   imdbRating, 0, a);
    }
  }

  if (sinopse[0] && aCopy > 0.004f)
    txt_bloco_corta(TXT_HERO_SIN, sinopse, 255, 255, 255, x, ySin, sinW,
                    NV_LD_HERO_SIN, aCopy, sinLinhas);
  return copy.action;
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
  const int lay = layoutHome();
  // O DESTAQUE PRINCIPAL DA MODERNA E SEMPRE TELA CHEIA (dono, 04/10): com o
  // foco no proprio destaque ele e outra coisa que o resto da home. O ajuste
  // "Fundo em tela cheia" so decide a arte quando o foco esta nas fileiras.
  // A fileira de AMIGOS tambem: o destaque dela e outra tela (fundo social,
  // autoria, ficha) desenhada para a arte inteira; na faixa a arte parava no
  // meio e sobrava o fundo social dos lados (dono, 05/10).
  int cheio = ajustes_hero_cheio() ||
              (lay == HOME_LAYOUT_MODERNA &&
               (focoHero || (foco.fileira >= 0 && foco.fileira < nFileiras &&
                             fileiras[foco.fileira].tipo == FILEIRA_SOCIAL)));
  // Em tela cheia o bloco sobe 70px (ver layout.h).
  GfxModo modoHero = cheio ? GFX_HERO_CHEIO : GFX_HERO;
  GfxRect r = cheio ? (GfxRect){ 0, 0, NV_TELA_W, NV_HERO_CHEIO_H }
                    : (GfxRect){ NV_HERO_ARTE_X, 0, NV_HERO_ARTE_W, NV_HERO_ARTE_H };
  // LAYOUTS NOVOS. O destaque deles e sempre "de titulo" (a arte do titulo em
  // foco) e nunca o de colecao/social da Moderna, que sao telas cheias de
  // outro assunto. Medidas do bloco de texto: a Moderna as le das constantes
  // de sempre, e os dois novos, do layout.h.
  float dinY = 0.0f, aVis = 1.0f;
  float logoH = NV_LOGO_HERO_H, sinW = NV_HERO_SIN_W;
  float btnH = NV_HERO_BOTAO_COMPACTO_H, btnGap = NV_HOME_HERO_BOTAO_GAP;
  int sinLinhas = 3;
  if (lay == HOME_LAYOUT_PADRAO) {
    // TELA CHEIA NA LARGURA (dono, 30/09): sem cartao, sem canto. A arte vai de
    // borda a borda e do topo da tela e se DISSOLVE na base para o fundo, onde
    // comecam as fileiras (o texto fica no trecho ainda opaco, com o veu).
    cheio = 0; modoHero = GFX_VITRINE; r = padBannerRect();
    vitVeu = 0.92f; vitAncora = 0.30f; vitDissolve = 1.0f; vitRaio = 0.0f;
    vitVeuIni = 0.0f;
    logoH = NV_PAD_LOGO_H; sinW = NV_PAD_SIN_W; sinLinhas = 3;
    btnH = NV_HERO_BOTAO_COMPACTO_H; btnGap = 22.0f;
  } else if (lay == HOME_LAYOUT_DINAMICA) {
    cheio = 0; modoHero = GFX_VITRINE;
    dinY = dinHeroY();
    // A ARTE E A TELA INTEIRA (dono, 30/09: "como a Apple TV"); o texto fica
    // na zona de NV_DIN_HERO_H, acima da fileira que espia por baixo.
    r = (GfxRect){ 0.0f, dinY, NV_TELA_W, NV_DIN_ARTE_H };
    // A arte apaga enquanto sobe (de NV_DIN_ARTE_FADE_A ate _B de rolagem): ela
    // e uma tela inteira e, sem isso, a ponta dela ficaria atras da primeira
    // fileira ancorada em cima.
    { float t = anim_clamp((-dinY - NV_DIN_ARTE_FADE_A) /
                           (NV_DIN_ARTE_FADE_B - NV_DIN_ARTE_FADE_A), 0.0f, 1.0f);
      aArte *= 1.0f - t * t * (3.0f - 2.0f * t); }
    // O texto sai ANTES de o destaque terminar de subir: rolar com o texto
    // inteiro na tela le como arrasto, e ele ja nao e o assunto da tela.
    aVis = anim_clamp(1.0f + dinY / 320.0f, 0.0f, 1.0f);
    vitVeu = 0.92f; vitAncora = 0.28f; vitDissolve = 1.0f; vitRaio = 0.0f;
    // O veu de baixo comeca no MESMO y absoluto que tinha com a arte de 780
    // (0,38 x 780): o texto le igual, e a arte segue escurecendo ate a base.
    vitVeuIni = 0.38f * NV_DIN_HERO_H / NV_DIN_ARTE_H;
    logoH = NV_DIN_LOGO_H; sinW = 760.0f;
    btnH = NV_HERO_BOTAO_COMPACTO_H; btnGap = 22.0f;
  }

  if(lay==HOME_LAYOUT_MODERNA && foco.fileira>=0 && foco.fileira<nFileiras && fileiras[foco.fileira].tipo==FILEIRA_SOCIAL) {
    float x=ajustes_conteudo_x(),a=1-saida;
    const Fileira *s=&fileiras[foco.fileira];
    const CatItem *p=(fileiraItemIndice(s, foco.coluna) >= 0)
                    ?cat_item_exato(fileiraItemIndice(s, foco.coluna)):NULL;
    const char *arte=p?arte_hero_do_item(p):NULL;   // tela cheia: arte grande
    GLuint ta=arte?tex_obter_hero(arte):0;
    // O fundo social so onde a arte NAO cobre. Com a arte do titulo opaca por
    // cima (tela cheia), ele era uma tela inteira pintada e escondida: a C9
    // parada na fileira "Entre amigos" ficava a 34 fps (29/09).
    // (Compor fundo e arte numa passada so, gfx_hero_camadas, foi MEDIDO na
    // C9 em 30/09 e saiu PIOR: 30 fps contra 38-45 deste caminho.)
    if (!ta || aArte < 0.999f || r.w < NV_TELA_W || r.h < NV_TELA_H ||
        nv_ambiente_forca > 0.001f)   // imersivo: o hero deixa o fundo vazar (uVaza)
      gfx_rect((GfxRect){0,0,NV_TELA_W,NV_TELA_H},0,GFX_SOCIAL,0,0,0,0,1,1,1,1);
    if(p) {
      if(arte)corviva_definir(arte,CORVIVA_HOME);
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
      // Sem a marca do Trakt: a fonte nao aparece na home (amigosfil.h).
      txt_desenhar_alpha(txt_linha(TXT_HERO_META,"SUA COMUNIDADE",210,191,199,255),x,154,a);
      txt_desenhar_alpha(txt_linha(TXT_TITULO1,"Boas histórias conectam.",244,243,247,255),x,226,a);
      txt_bloco(TXT_HERO_SIN,"Descubra o que seus amigos estão vendo.\nUma nova recomendação pode começar aqui.",187,190,202,x,330,740,36,a,2);
    }
    heroArteRect=r;
    return;
  }

  // COLECAO EM FOCO: o destaque vira o banner da pasta. Moderna e Padrao; a
  // Dinamica nao tem destaque parado no topo e mostra o mesmo banner por tras
  // do cabecalho da fileira (dinArteColecao).
  if(lay!=HOME_LAYOUT_DINAMICA&&foco.fileira>=0&&foco.fileira<nFileiras&&fileiras[foco.fileira].tipo==FILEIRA_CATALOGOS) {
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
      // A ORDEM DO WEB: heroBackdropUrl > coverImageUrl > backdropImageUrl da
      // colecao (col_banner). Antes a pasta sem hero proprio pegava o fundo do
      // grupo antes da capa, e a sem nada ficava vazia.
      const char *art=col_banner(folder);
      GLuint t=0;
      if (ehDiretor) diretor_pedir(folder->title);
      if (!t && art[0]) t=tex_obter_hero(art);
      if (art[0]) corviva_definir(art, CORVIVA_HOME);
      if(t){gfx_tex_aspect_atual=tex_aspecto(art);
        if(modoHero==GFX_VITRINE)   // Padrao: tela cheia, dissolvida na base
          gfx_rect(r,t,modoHero,vitVeu,vitAncora,vitDissolve,vitRaio,vitVeuIni,0,0,aArte);
        else gfx_rect(r,t,modoHero,0,0,0,0,0,0,0,aArte);
        gfx_tex_aspect_atual=0;}
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
      GLuint logo=urlFl?tex_obter_logo_larg(urlFl,NV_COLLECTION_HERO_LOGO_MAX_W+40.0f):0;
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
  // Com um deslize em curso a troca seguinte espera ele assentar: comecar
  // outro no meio faria o titulo que esta entrando saltar de volta ao lugar.
  // A seta segurada nao acumula: vale o ultimo desejo.
  // O logo do desejado entra na fila JUNTO com a arte (ver heroPedirLogo).
  if (heroDesejado >= 0 && heroDesejado != heroAtual)
    heroPedirLogo(cat_item_exato(heroDesejado), lay, cheio);
  // O proximo do carrossel, nos ultimos NV_HERO_PRE_MS (ver heroPreItem).
  if (heroPreItem >= 0 && heroPreItem != heroAtual && heroDesejado < 0) {
    const char *arteP = arte_por_identidade(heroPreItem, 2);
    if (arteP) (void)tex_obter_hero(arteP);
    heroPedirLogo(cat_item_exato(heroPreItem), lay, cheio);
    if (heroPreLogado != heroPreItem) {
      heroPreLogado = heroPreItem;
      printf("[hero] pre-busca do proximo hash=%08lx (%d ms antes da troca)\n",
             tex_hash_public(arteP), NV_HERO_PRE_MS);
      fflush(stdout);
    }
  }
  if (heroDesejado >= 0 && heroDesejado != heroAtual && heroDesliza >= 1.0f) {
    const char *arteD = arte_por_identidade(heroDesejado, 2);
    // Ausencia de arte tambem e um estado pronto: o placeholder pertence ao
    // item e pode entrar sem apagar o hero anterior primeiro.
    int artePronta = !arteD || tex_obter_hero(arteD);
    // A pre-busca valeu? Uma linha por troca que foi pre-buscada.
    if (heroPreLogado == heroDesejado && (artePronta ||
        SDL_GetTicks() - heroDesejadoEm >= NV_HERO_ESPERA_MS)) {
      const CatItem *cD = cat_item_exato(heroDesejado);
      const char *uL = cD ? artehero_logo_sessao(cD) : NULL;
      printf("[hero] pre-busca %s hash=%08lx logo=%s\n",
             artePronta ? "acertou" : "nao chegou a tempo", tex_hash_public(arteD),
             !uL ? "sem" : tex_obter_logo_larg_qualquer(uL, heroLogoMaxW(lay, cheio)) ? "pronto" : "a caminho");
      fflush(stdout);
      heroPreLogado = -1;
    }
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
      // DESLIZA so com a arte nova JA pronta (ou sem arte nenhuma, o marcador):
      // estourado o prazo, o lado que entra seria vazio passando pela tela, e
      // ai vale o esvanecimento de sempre. Efeitos minimos (GPU fraca) tambem
      // esvanecem: o deslize pinta duas artes de tela por quadro durante a
      // troca. Animacoes reduzidas: troca seca, como antes.
      int desliza = heroDirDesejado != 0 && !heroEstourou && !motionReduzido &&
                    ajustes_hero_deslizar() && !gfx_efeitos_minimos();
      heroEstourou = 0;
      heroAnterior = heroAtual;
      heroAtual = heroDesejado;
      heroDesejado = -1;
      if (desliza) {
        heroSai = 0.0f;
        heroEntra = 1.0f;
        heroDesliza = 0.0f;
        heroDeslizaDir = heroDirDesejado;
      } else {
        heroDesliza = 1.0f;
        heroSai = (motionReduzido || !arteD) ? 0.0f : 1.0f;
        heroEntra = (motionReduzido || !arteD) ? 1.0f : 0.0f;
      }
      heroDirDesejado = 0;
      heroTrocaEm = SDL_GetTicks() + NV_HERO_INTERVALO_MS;
    }
  }

  const CatItem *ci = cat_item_exato(heroAtual);
  const char *arteA = arte_por_identidade(heroAtual, 2);
  // COR VIVA: o titulo do destaque e quem manda na cor da home. heroAtual so
  // troca quando a arte nova ja decodificou (acima), entao a cor chega junto
  // com a arte, e corviva ainda espera 150 ms parado antes de mudar.
  if (arteA) corviva_definir(arteA, CORVIVA_HOME);
  const CatItem *cAnt = cat_item_exato(heroAnterior);
  const char *arteB = arte_por_identidade(heroAnterior, 2);
  // Teto de 1920: o hero ocupa a tela e a 960 saia esticado ao dobro.
  // O ANTERIOR so e pedido ENQUANTO a mistura acontece. Estava sendo pedido em
  // TODO quadro, mesmo com a troca ja terminada, quando ele nao e desenhado: se o
  // cache ja o tinha despejado, o pedido o trazia de volta — uma textura de
  // 1920 (~8 MB) re-decodificada para NAO ser desenhada, empurrando os posteres
  // visiveis para fora do orcamento.
  // No deslize o anterior tambem e desenhado (sai pelo lado), e so enquanto
  // ele dura: parado, nada a mais.
  const int deslizando = heroDesliza < 1.0f;
  const float pDesl = deslizando ? anim_saida(heroDesliza) : 1.0f;
  // Em fracao da largura da arte. As duas bordas andam JUNTAS: o que sai
  // termina onde o que entra comeca, sem vao nem sobreposicao.
  const float dAnt = deslizando ? -(float)heroDeslizaDir * pDesl : 0.0f;
  const float dAtu = deslizando ? (float)heroDeslizaDir * (1.0f - pDesl) : 0.0f;
  GLuint tAnt = ((heroSai > 0.0f || deslizando) && arteB) ? tex_obter_hero(arteB) : 0;
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
  // hero e a arte se apaga por cima dele (heroTrailerFade); as rampas do hero
  // voltam por cima do video (#290, ver o fim do bloco). So no hero de titulo (colecao e social nao chegam aqui com
  // trailer: ver home_trailer_passo).
  float aTrailer = (heroTrailerFade > 0.005f && heroTrailerItem == heroAtual) ? heroTrailerFade : 0.0f;
  if (aTrailer > 0.0f) {
    GfxRect furo = r;
    if (furo.y + furo.h > NV_TELA_H) furo.h = NV_TELA_H - furo.y;
    if (furo.y < 0.0f) { furo.h += furo.y; furo.y = 0.0f; }
    gfx_furo(furo);
    aArte *= (1.0f - aTrailer);
  }
  if (deslizando) {
    // O que sai do retangulo da arte nao pinta: o shader corta a arte de tela,
    // e o recorte aqui corta o poster e o marcador, que andam o rect inteiro.
    float y0 = r.y < 0.0f ? 0.0f : r.y;
    float y1 = r.y + r.h > NV_TELA_H ? NV_TELA_H : r.y + r.h;
    GfxRect rAnt = r, rAtu = r;
    rAnt.x += dAnt * r.w;
    rAtu.x += dAtu * r.w;
    // RECORTE POR LADO. A arte que sai so existe em [r.x + dAnt*w, r.x + w +
    // dAnt*w] e a que entra em [r.x + dAtu*w, ...]: fora disso o shader ja
    // devolvia alfa 0 (`dentro`), mas cada fragmento era executado e
    // misturado — duas telas cheias por quadro durante o deslize, numa GPU
    // presa em preenchimento. Com a tesoura em cada lado, a soma dos dois e
    // UMA tela, e o pixel e o mesmo (o que a tesoura tira era transparente).
    { float ax0 = r.x + (dAnt < 0.0f ? 0.0f : dAnt) * r.w;
      float ax1 = r.x + (1.0f + (dAnt > 0.0f ? 0.0f : dAnt)) * r.w;
      float bx0 = r.x + (dAtu < 0.0f ? 0.0f : dAtu) * r.w;
      float bx1 = r.x + (1.0f + (dAtu > 0.0f ? 0.0f : dAtu)) * r.w;
      if (ax1 > ax0) {
        gfx_recorte(ax0, y0, ax1 - ax0, y1 - y0);
        if (tAnt) (void)desenhaArteHero(r, modoHero, cAnt, arteB, aArte, dAnt);
        else if (!arteB) desenhaPlaceholderHero(rAnt, cAnt, aArte, 0);
      }
      if (bx1 > bx0) {
        gfx_recorte(bx0, y0, bx1 - bx0, y1 - y0);
        if (tAtu) (void)desenhaArteHero(r, modoHero, ci, arteA, aArte, dAtu);
        else desenhaPlaceholderHero(rAtu, ci, aArte, arteA != NULL && arteA[0] != 0);
      } }
    gfx_sem_recorte();
  } else {
  // CROSSFADE NUMA PASSADA: a arte que sai e a que entra compostas no
  // fragmento sobre a luz (gfx_hero_camadas) — eram tres telas cheias por
  // quadro, duas misturadas, e a C9 caia a 42 ms em toda troca de destaque.
  // Cai no caminho de sempre quando uma das artes e poster ou o modo nao e
  // o do destaque.
  int camadas = 0;
  if (tAnt && tAtu && heroSai > 0.004f && heroEntra > 0.0f && aArte > 0.004f) {
    float aspA = 0.0f, aspB = 0.0f;
    if (heroArtePlana(ci, arteA, &aspA) && heroArtePlana(cAnt, arteB, &aspB))
      camadas = gfx_hero_camadas(r, modoHero, tAtu, aspA, anim_suave(heroEntra) * aArte,
                                 tAnt, aspB, anim_suave(heroSai) * aArte);
  }
  if (camadas) {
  } else if (tAnt) {
    // Esvanecimento com aceleracao e desaceleracao: o medido fica ~25% do
    // percurso quase parado no comeco, entao rampa reta le como corte na saida.
    (void)desenhaArteHero(r, modoHero, cAnt, arteB,
                          anim_suave(heroSai) * aArte, 0.0f);
  } else if (heroSai > 0.0f) {
    desenhaPlaceholderHero(r, cAnt, anim_suave(heroSai) * aArte, 0);
  }
  if (camadas) {
  } else if (tAtu && heroEntra > 0.0f) {
    (void)desenhaArteHero(r, modoHero, ci, arteA,
                          anim_suave(heroEntra) * aArte, 0.0f);
  } else if (!tAtu && aTrailer <= 0.0f) {
    // ESPERANDO quando ha caminho de arte e ela ainda nao decodificou; ausente
    // quando o titulo nao tem arte nenhuma para pedir.
    desenhaPlaceholderHero(r, ci,
                           aArte * (heroEntra > 0.0f ? 1.0f : heroEntra),
                           arteA != NULL && arteA[0] != 0);
  }
  }
  // RAMPAS SOBRE O TRAILER (#290, dono 06/10: "vamos colocar o gradiente
  // mesmo, vamos fazer ficar bonito"). Sem elas o video era um retangulo de
  // borda dura ao lado do painel do texto. Aqui so as RAMPAS do proprio hero
  // (uPar.x > 0.5: a esquerda, 45% da largura, e o pe, 18% da altura), as
  // mesmas que dissolvem a arte parada — trailer e arte ficam com o mesmo
  // enquadramento, e o miolo do video fica limpo (o veu de 20/09 que o dono
  // tirou escurecia o trailer; este nao toca no miolo). Um quad, sem textura.
  // Alfa = heroTrailerFade: entra enquanto a arte sai, entao a soma das duas
  // rampas fica constante no crossfade. Desenhado DEPOIS do furo, com alfa:
  // no .tpk/LG/Android o plano de video aparece com 1 - alfa sob a rampa.
  if (aTrailer > 0.0f && (modoHero == GFX_HERO || modoHero == GFX_HERO_CHEIO))
    gfx_rect(r, 0, modoHero, 0, 1.0f, 0, 0.0f, 0, 0, 0, aTrailer);
  gfx_tex_aspect_atual = 0.0f;
  heroArteRect = r;

  float aTexto = (1.0f - saida) * aVis;
  // MODO CINEMA: o bloco desce NV_CINEMA_DESCE enquanto apaga (o do detalhe) e
  // so o logo fica. `aCopy` e a opacidade do que SOME; o logo segue `aTexto`.
  float cin = trailercinema_t(&heroCinema);
  float aCopy = aTexto * (1.0f - cin);
  float descidaCopy = saida * NV_TELA_H * 0.06f + cin * NV_CINEMA_DESCE;
  if (aTexto <= 0.004f) return;

  // Logo, actions, then the real information and compact synopsis. Keep
  // each layout's bottom boundary; use the measured copy for the action Y.
  float empurra = scrollY < 0.0f ? -scrollY : 0.0f;
  float aBotao = anim_clamp(empurra / NV_HOME_HERO_EMPURRA, 0.0f, 1.0f);
  float base = NV_SHELF_TOP - NV_HERO_COPY_GAP + descidaCopy
             + empurra - 24.0f;
  // Padrao e Dinamica ancoram o bloco na BASE DO PROPRIO DESTAQUE (e o botao
  // sempre existe, com o foco ou sem ele): o texto anda com a arte, e nao com
  // as fileiras como na Moderna.
  if (lay == HOME_LAYOUT_PADRAO) {
    aBotao = 1.0f;
    base = r.y + r.h - NV_PAD_TEXTO_BASE + descidaCopy;
  } else if (lay == HOME_LAYOUT_DINAMICA) {
    aBotao = aVis;
    base = r.y + NV_DIN_HERO_H - NV_DIN_TEXTO_BASE + descidaCopy;
  }
  base += bordaPag.x;   // retorno de borda do Cima no destaque
  // Moderna: without the button the logo hugs the text (owner 03/10).
  float slotBtn = (lay == HOME_LAYOUT_MODERNA) ? aBotao : 1.0f;
  float x = ajustes_conteudo_x();
  // TROCA DESLIZADA: o bloco do titulo que sai anda junto com a arte dele, e o
  // do que entra vem colado atras, na mesma distancia (a largura da arte).
  if (deslizando && cAnt && cAnt != ci)
    desenhaCopiaHero(cAnt, 0, x + dAnt * r.w, base, lay, cheio, logoH, sinW,
                     sinLinhas, aTexto, aCopy, cin, btnH, btnGap, slotBtn);
  float actionY = desenhaCopiaHero(ci, 1, x + dAtu * r.w, base, lay, cheio, logoH, sinW,
                   sinLinhas, aTexto, aCopy, cin, btnH, btnGap, slotBtn);

  // O BOTAO E A POSICAO, que so existem enquanto o destaque tem o foco.
  //
  // A opacidade vem da ROLAGEM, e nao de uma mola propria: `scrollY` ja e
  // negativo na medida exata do empurrao das fileiras, entao o botao aparece e
  // some EXATAMENTE junto com o movimento que o trouxe. Duas molas para o mesmo
  // gesto descasariam, e o olho le descasamento como defeito.
  { int n = heroNLista();
    if (aBotao > 0.004f && n > 0) {
      // OK ABRE A PAGINA DO TITULO — e o rotulo diz isso. "Reproduzir" seria a
      // promessa de comecar o filme, e quem aperta acaba numa pagina: o rotulo
      // tem de descrever o que a tecla FAZ, nao o que seria bonito escrever.
      const char *rot = i18n("Ver título");
      // Em cinema o botao e o contador nao se desenham (o itemFoco la embaixo
      // segue valendo: a tecla que devolve a UI nao pode abrir o titulo errado).
      float aBtn = aBotao * (1.0f - cin);
      if (aBtn > 0.004f) {
        // Nos layouts novos o botao existe SEMPRE; so aceso (na cor de realce)
        // com o foco no destaque. Na Moderna ele so aparece com o foco la.
        int btnFoco = (lay == HOME_LAYOUT_MODERNA) || focoHero;
        // PILULA COMPACTA (dono, 06/10, referencia da Apple TV): com o foco,
        // branca com o texto escuro; sem ele, a pilula translucida de sempre
        // com o texto claro. ~46 px e o corpo de 24/600 em vez do callout.
        // Em foco, o preenchimento e a tinta vem do ACENTO (botao_superficie,
        // a mesma pilula primaria do resto do app, inclusive "Cor da logo"
        // e vidro) — nunca um branco fixo (dono, 06/10: nao mudava com o acento).
        // O rotulo usa o MESMO corpo (TXT_DET_BOTAO, botoes.h) e a MESMA tinta
        // que botao_superficie devolve para este foco: antes era TXT_ILHA_NOME
        // 24/600 com ajustes_tinta_foco, mais pesado e fora da pilula do app.
        TxtLinha lb = txt_linha(TXT_DET_BOTAO, rot, 245, 245, 245, 255);
        float bh = btnH;
        float bw = lb.w + 38.0f * 2;   // NV_DETW2_BTN_PADX, igual ao detalhe
        float by = actionY;
        GfxRect bt = { x, by, bw, bh };

        // Raio = metade da ALTURA: o raio do gfx_cor e fracao da altura do
        // retangulo, entao 0,5 e a pilula exata em qualquer largura.
        if (btnFoco) {
          int tb = botao_superficie(bt, 1.0f, aBtn);
          lb = txt_linha(TXT_DET_BOTAO, rot, tb, tb, tb, 255);
        }
        else if (ajustes_vidro()) gfx_vidro_painel(bt, 0.5f, 0.55f, aBtn);
        else gfx_cor(bt, 0.5f, 1.0f, 1.0f, 1.0f, 0.18f * aBtn);
        // O TRIANGULO DE REPRODUZIR NAO ENTRA AQUI. Ele e a marca universal de
        // "comeca agora" e este botao nao comeca nada; desenha-lo seria a mesma
        // mentira do rotulo, so que em forma.
        txt_desenhar_alpha(lb, x + (bw - lb.w) * 0.5f, by + (bh - lb.h) * 0.5f,
                           aBtn);

        // PONTOS DE PAGINA no lugar do "1 / 10" (dono, 06/10): no canto
        // direito, na altura do botao. Esquerda/direita do controle continuam
        // trocando o destaque; as setas aqui sao so a dica.
        if (btnFoco) {
          int p = heroPosDe(heroIntencao());
          desenhaPontosHero(NV_TELA_W - ajustes_conteudo_x(), by + bh * 0.5f,
                            n, p < 0 ? 0 : p, aBtn);
        }
      }

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
static void desenhaFundo(Uint32 agora) {
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  // So a Dinamica pinta fundo (o vidro fosco); Moderna e Padrao ficam com o
  // clear, como sempre.
  if (layoutHome() == HOME_LAYOUT_DINAMICA) { desenhaFundoDin(agora); return; }
  // A tela ja foi limpa com ESTA MESMA COR por glClearColor/glClear em
  // main.c antes de app_desenhar. Pintar por cima era uma camada de tela
  // cheia jogada fora por quadro — e o custo dominante nesta GPU e fill
  // rate (gfx.c registra que DUAS camadas de tela cheia derrubavam a
  // Mali-G71 para ~40fps). Nao repor sem antes mudar a cor do clear.
  (void)tela;
}

// PONTEIRO (#99). Poe o foco pelas MESMAS variaveis que as setas mexem em
// home_evento — fileira, coluna e a memoria de coluna da fileira —, com os
// mesmos efeitos colaterais de uma tecla (relogio do destaque, pergunta de
// sair desarmada). O OK do clique chega depois, pelo caminho de sempre.
static void ponteiroCard(int r, int c) {
  if (r < 0 || r >= nFileiras || c < 0 || c >= foco.nColunas[r]) return;
  focoHero = 0;
  foco.fileira = r; foco.coluna = c; foco.colunaLembrada[r] = c;
  heroUltTecla = SDL_GetTicks();
  sairPerguntadoEm = 0;
}
static void ponteiroHero(int a, int b) {
  (void)a; (void)b;
  focoHero = 1;
  heroUltTecla = SDL_GetTicks();
  sairPerguntadoEm = 0;
}
// So a parte VISIVEL do card: acima do viewport das fileiras (o gfx_recorte
// de home_desenhar) o card esta cortado e por baixo mora o destaque.
static void alvoCard(float x, float y, float w, float h, int r, int c) {
  float topo = corteFileiras();
  if (!ponteiro_ativo()) return;
  if (y < topo) { h -= topo - y; y = topo; }
  ponteiro_alvo(x, y, w, h, ponteiroCard, NULL, r, c);
}

static void desenhaAtalhos(int r, float y) {
  float w = larguraFil(r), h = alturaFil(r);
  static int ultimo=-1;static Uint32 desde;
  // Estado do ramo de GIF (#29), SEPARADO do da sequencia de JPEG desde o #141:
  // o GIF passou a poder sair da CAPA, e a troca de fonte no mesmo cartaz
  // tambem zera a pergunta ao arquivo. Ver gifcolecao.h.
  static GcFoco gcFoco; static int gcIniciado;
  if (!gcIniciado) { gifcol_foco_iniciar(&gcFoco); gcIniciado = 1; }
  // Quadro da sequencia de JPEG que ja esta resolvido, para nao reconsultar
  // o cache nos ~4 quadros de tela que cabem entre dois passos de 67 ms.
  static int seqIndice=-1;static GLuint seqTex;
  for (int c = 0; c < fileiras[r].n; c++) {
    float x = ajustes_conteudo_x() + c * passoFil(r) - scrollX[r] + bordaX(r);
    if (x + w < 0 || x > NV_TELA_W) continue;
    float f = animFoco[r][c], raio = raioDe(w, h);
    GfxRect card = {x, y, w, h};
    alvoCard(x, y, w, h, r, c);
    // O ANEL E OPCIONAL (Ajustes > Foco no cartaz). Sem ele o foco continua
    // dito pelo tamanho e pela animacao do cartaz — o que sai e so a borda.
    if (f > .01f && ajustes_borda_foco()) {
      float menor = w < h ? w : h;
      float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
      if (ajustes_vidro()) gfx_vidro_cartao((GfxRect){x, y, w, h}, raio, f, 1.0f);
      else
      gfx_cor((GfxRect){x - NV_ANEL_FOCO, y - NV_ANEL_FOCO,
        w + 2*NV_ANEL_FOCO, h + 2*NV_ANEL_FOCO},
        (raio * menor + NV_ANEL_FOCO) / (menor + 2*NV_ANEL_FOCO), ar, ag, ab, f);
    }
    gfx_cor(card, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, 1);
    const ColFolder *folder=col_folder(fileiras[r].folders[c]);if(!folder)continue;
    const char *arte = col_capa(folder);
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
    // A FONTE (#141): focusGifUrl, ou a propria CAPA quando os bytes que
    // chegaram dela sao GIF e a conta nao mandou focusGifUrl. E, quando nao
    // anima, UMA linha por cartaz dizendo por que (gifcol_registrar).
    //
    // So o Tizen anima: la gif.c decodifica o GIF num fio proprio e conta o
    // tempo. No webOS gif_pode_animar e 0 e o cartaz fica na capa parada,
    // como hoje — e o arquivo nem e pedido; so o motivo vai ao log. Ver gif.h.
    if(foco.fileira==r&&foco.coluna==c&&folder->frames<1) {
      int id=fileiras[r].folders[c];Uint32 now=SDL_GetTicks();
      unsigned char magCapa[4], mag[4];
      int temMagCapa=0, daCapa=0, temMag=0;
      GcMotivo motivo=GC_ANIMA;
      const char *fonte;
      char chave[200];
      // A magica da capa so interessa sem focusGif: e uma busca no cache.
      if(!folder->focusGif[0]&&arte&&arte[0]) temMagCapa=tex_magica(arte,magCapa);
      fonte=gifcol_fonte(folder->focusGif,arte,temMagCapa?magCapa:NULL,&daCapa);
      if(gifcol_focar(&gcFoco,id,fonte,now)) {
        // gif_parar SOLTA O FIO DE DECODE do cartaz anterior; e gif_textura
        // devolve 0 ate o primeiro quadro DESTE chegar (a textura e uma so, e
        // ainda tem o ultimo quadro do outro).
        gif_parar();
      }
      if(!fonte) motivo=gifcol_sem_fonte(arte,temMagCapa?magCapa:NULL);
      else if(NV_SEM_GIF||ajustes_animacoes_reduzidas()) motivo=GC_REDUZIDAS;
      else if(!gif_pode_animar()) motivo=GC_APARELHO;
      else {
        // O ARQUIVO E PEDIDO FORA DO ATRASO de 350 ms. Dentro dele, o download so
        // comecaria depois do atraso e o primeiro quadro chegaria tarde; pedir
        // cedo custa uma consulta ao cache, que devolve NULL enquanto nao chegou.
        const char *arq = tex_arquivo(fonte);
        if(!arq) {
          // Sem arquivo: ou ainda nao chegou, ou chegou e nao e GIF (no Tizen
          // so GIF vira arquivo), ou era GIF e a poda levou (tex_arquivo ja
          // pediu de novo, uma vez).
          temMag=tex_magica(fonte,mag);
          motivo=!temMag?GC_ARQ_AINDA:(gifcol_eh_gif(mag)?GC_SUMIU:GC_FORMATO);
        } else {
          // gif_animado LE O ARQUIVO INTEIRO. Uma vez por cartaz, e nao por quadro.
          if(gcFoco.anima<0) {
            gcFoco.anima=gif_animado(arq);
            gcFoco.motivoArq=gifcol_motivo_arquivo(arq,gcFoco.anima,gcFoco.magicaArq);
          }
          if(gcFoco.anima<=0) { motivo=gcFoco.motivoArq; memcpy(mag,gcFoco.magicaArq,4); temMag=1; }
          else if(now-gcFoco.desde>350) {
            // A CADA DESENHO, sem passo de 67 ms (1.4.7): o relogio e de gif.c,
            // que so sobe as linhas que mudaram quando o quadro do GIF vence. O
            // passo de 67 ms prendia o GIF a 15 fps, abaixo do ritmo do arquivo.
            GLuint motion=gif_textura(arq,480);
            if(motion){
              tex=motion;
              // A PROPORCAO DA CAPA NAO VALE AQUI. Abaixo o desenho usa
              // tex_aspecto(arte), que e a da capa; o GIF do CDN pode vir em
              // qualquer proporcao. Zero deixa o desenho usar a moldura.
              gifDesenhando=1;
            } else if(gif_recusou(arq)) motivo=GC_ORCAMENTO;
          }
        }
      }
      // OS PROVISORIOS ESPERAM O PRAZO: "ainda nao chegou" aos 200 ms e so um
      // download em andamento. Os definitivos saem na hora.
      if(motivo!=GC_ANIMA&&
         ((motivo!=GC_ARQ_AINDA&&motivo!=GC_CAPA_AINDA)||now-gcFoco.desde>GIFCOL_PRAZO_MS)) {
        snprintf(chave,sizeof chave,"%s|%s",folder->groupId,folder->id);
        gifcol_registrar(chave,folder->title,motivo,fonte?fonte:arte,fonte?daCapa:1,
                         temMag?mag:NULL);
      }
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
// O trailer do destaque esta no ar, tocando? (Diferente de segurando, que
// tambem vale durante a busca e o preparo.)
static int heroTrailerTocando(void) {
  const CatItem *ci;
  if (!ajustes_trailer_hero() || heroTrailerItem != heroAtual) return 0;
  ci = cat_item_exato(heroAtual);
  if (!ci || strcmp(heroTrailerImdb, ci->imdb) != 0) return 0;
  return trailer_aberto() && trailer_tocando();
}

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
  // Titulo que ja tocou nao vai abrir de novo: nao ha o que esperar.
  if (heroTrailerJaTocou(heroTrailerImdb)) return 0;
  return !heroTrailerTentado && heroTrailerDesde &&
         agora - heroTrailerDesde <= heroTrailerMaxEspera();
}

enum { HERO_GATE_READY, HERO_GATE_UNSUPPORTED, HERO_GATE_OVERLAY,
       HERO_GATE_DISABLED, HERO_GATE_SETTING, HERO_GATE_DYNAMIC,
       HERO_GATE_POSTER_WAIT, HERO_GATE_TRANSITION, HERO_GATE_NONCONTENT,
       HERO_GATE_NO_ID };
static int heroTrailerGateAnterior = -1;
static const char *heroTrailerTopoMotivo, *heroTrailerTopoAnterior;
void home_trailer_topo_motivo(const char *motivo) { heroTrailerTopoMotivo = motivo; }
static void heroTrailerGateLog(int motivo, int enabled) {
  static const char *const nomes[] = { "ready", "unsupported", "top-overlay",
    "hero-disabled", "focused-setting-disabled", "dynamic-poster-hidden",
    "poster-wait", "art-transition", "non-content-row", "missing-content-id" };
  const char *porque = motivo == HERO_GATE_OVERLAY ? heroTrailerTopoMotivo : NULL;
  if (!enabled) { heroTrailerGateAnterior = -1; heroTrailerTopoAnterior = NULL; return; }
  // The overlay names a string literal from app.c: pointer compare is enough.
  if (motivo != heroTrailerGateAnterior || porque != heroTrailerTopoAnterior) {
    heroTrailerGateAnterior = motivo;
    heroTrailerTopoAnterior = porque;
    if (porque) printf("[home-trailer] autoplay gate=%s (%s)\n", nomes[motivo], porque);
    else printf("[home-trailer] autoplay gate=%s\n", nomes[motivo]);
    fflush(stdout);
  }
}

static void heroTrailerFonteFalhou(void) {
  if(heroTrailerQual>0&&heroTrailerQual<=TRF_YOUTUBE)
    heroTrailerFalhas|=1u<<heroTrailerQual;
  if(heroTrailerQual==TRF_APPLE)heroTrailerAppleFalhou=1;
  heroTrailerTentado=trailerfonte_depois_destaque(trailerfonte_ajuste(),
                    trailerfonte_tizen(),heroTrailerQual)==0;
  heroTrailerFonte=heroTrailerTentado?3:0;
  heroTrailerPreparandoAte=0;heroTrailerFade=0.0f;
}

void home_trailer_passo(int topo, float dt, Uint32 agora) {
  const CatItem *ci = NULL;
  int pronto;
  Uint32 decorrido;
  int heroSetting = ajustes_trailer_hero(), posterSetting = ajustes_trailer_cartaz();
  int enabled = heroSetting || posterSetting, motivo = HERO_GATE_READY;
  if (!trailer_suportado()) { heroTrailerGateLog(HERO_GATE_UNSUPPORTED, enabled); return; }
  // DUAS PORTAS PARA O MESMO TRAILER. Com o foco no destaque, "Trailer no
  // destaque". Com o foco num CARTAZ das fileiras (#124: "parado num titulo do
  // catalogo, nada toca"), "Trailer do cartaz em foco" — o
  // focusedPosterBackdropTrailerEnabled do web, destino hero_media: o destaque
  // ja segue o card em repouso (heroAtual, ver "O HERO SEGUE O FOCO"), entao o
  // trailer toca onde a arte dele ja esta. Espera o mesmo tempo da expansao do
  // cartaz, contado de quando o foco parou nele.
  { int noHero = focoHero && ajustes_hero_ligado() && heroSetting;
    // Na Dinamica o destaque ROLOU para fora quando o foco esta num cartaz: o
    // trailer do cartaz tocaria onde ninguem ve.
    int noCartaz = !focoHero && ajustes_hero_ligado() && posterSetting &&
                   layoutHome() != HOME_LAYOUT_DINAMICA &&
                   heroPendente == heroAtual &&
                   agora - heroPendenteEm >= (Uint32)(ajustes_expandir_poster_atraso() * 1000.0f);
    pronto = topo && (noHero || noCartaz);
    if (!topo) motivo = HERO_GATE_OVERLAY;
    else if (!ajustes_hero_ligado()) motivo = HERO_GATE_DISABLED;
    else if (focoHero ? !heroSetting : !posterSetting) motivo = HERO_GATE_SETTING;
    else if (!focoHero && layoutHome() == HOME_LAYOUT_DINAMICA) motivo = HERO_GATE_DYNAMIC;
    else if (!noHero && !noCartaz) motivo = HERO_GATE_POSTER_WAIT;
  }
  pronto = pronto &&
           heroDesejado < 0 && heroAtual >= 0 && heroEntra >= 0.999f && heroSai <= 0.001f &&
           heroDesliza >= 1.0f &&   // o trailer abre com a arte ja parada
           !(!focoHero && foco.fileira >= 0 && foco.fileira < nFileiras &&
             (fileiras[foco.fileira].tipo == FILEIRA_CATALOGOS ||
              fileiras[foco.fileira].tipo == FILEIRA_SOCIAL));
  if (motivo == HERO_GATE_READY && !pronto) {
    if (!focoHero && foco.fileira >= 0 && foco.fileira < nFileiras &&
        (fileiras[foco.fileira].tipo == FILEIRA_CATALOGOS || fileiras[foco.fileira].tipo == FILEIRA_SOCIAL))
      motivo = HERO_GATE_NONCONTENT;
    else motivo = HERO_GATE_TRANSITION;
  }
  if (pronto) ci = cat_item_exato(heroAtual);
  if (pronto && (!ci || !ci->imdb[0])) motivo = HERO_GATE_NO_ID;
  heroTrailerGateLog(motivo, enabled);
  if (!pronto || !ci || !ci->imdb[0]) {
    // Hero deixou de estar pronto (foco saiu, transicao da arte, detalhe por
    // cima): fecha. E o outro caminho de fechamento que o prazo nao ve —
    // o emulador mostrou "estado -1 -> -2 +8613ms" sem mais nada.
    // Adotado pela pagina do titulo (trailer.h, TRAILER_DONO_DETALHE): o
    // trailer continua la, e nao e mais desta tela fechar.
    if (heroTrailerItem >= 0 && trailer_aberto() && !trailer_cheia() &&
        trailer_dono() != TRAILER_DONO_DETALHE) {
      printf("[trailer] hero: saiu de cena, fecha o trailer %s (estado %d, +%u ms)\n",
             trailer_tocando() ? "tocando" : "sem playing", trailer_estado(),
             heroTrailerDesde ? (unsigned)(agora - heroTrailerDesde) : 0u);
      fflush(stdout);
      trailer_fechar();
    }
    heroTrailerItem = -1; heroTrailerDesde = 0; heroTrailerTentado = 0;
    heroTrailerImdb[0] = 0;
    heroTrailerPreparandoAte = 0; heroTrailerFonte = 0; heroTrailerAppleFalhou = 0;
    heroTrailerFalhas = 0;heroTrailerQual = 0;
    heroTrailerFade = 0.0f;
  } else if (heroTrailerItem != heroAtual || strcmp(heroTrailerImdb, ci->imdb) != 0) {
    // A rotacao do carrossel roda ANTES deste passo (home_atualizar): quando
    // o prazo de preparo vence, heroTrailerSegurando solta e o hero troca de
    // titulo no mesmo quadro — o fechamento acontece AQUI, nao no ramo do
    // prazo abaixo. Sem esta linha o registro so mostrava o elemento sumir
    // (emulador, 22/09/2026: "estado -1 -> -2 +5157ms" e nada mais).
    if (trailer_aberto() && !trailer_cheia() && trailer_dono() != TRAILER_DONO_DETALHE) {
      printf("[trailer] hero: troca de titulo fecha o trailer %s (estado %d, +%u ms)\n",
             trailer_tocando() ? "tocando" : "sem playing", trailer_estado(),
             heroTrailerDesde ? (unsigned)(agora - heroTrailerDesde) : 0u);
      fflush(stdout);
      trailer_fechar();
    }
    heroTrailerItem = heroAtual; heroTrailerDesde = agora; heroTrailerTentado = 0;
    snprintf(heroTrailerImdb, sizeof heroTrailerImdb, "%s", ci->imdb);
    heroTrailerPreparandoAte = 0; heroTrailerFonte = 0; heroTrailerAppleFalhou = 0;
    heroTrailerFalhas = 0;heroTrailerQual = 0;
    heroTrailerFade = 0.0f;
    // Ja tocou nesta sessao: nem consulta/reconsulta fonte para um autoplay
    // que nao vai acontecer. Tambem libera a rotacao se o conjunto ficou sem memoria.
    heroTrailerTentado = heroTrailerMemoriaFalhou || heroTrailerJaTocou(ci->imdb);
    if (heroTrailerTentado) goto trailer_hero_fim;
    trailerapple_pedir(ci->imdb, ci->titulo, ci->meta, ci->tipo[0] ? !strcmp(ci->tipo, "series") : 0);
#ifdef __EMSCRIPTEN__
    // A consulta do hero e so /videos; extras_pedir (ficha, creditos,
    // relacionados e imagens) continua reservado para a pagina de detalhe.
    extras_hero_trailer_pedir(ci->imdb,
                              ci->tipo[0] ? !strcmp(ci->tipo, "series") : 0,
                              ci->tmdb);
#endif
    // O IMDb exige Referer, que navegador nenhum deixa por (e o CORS dele so
    // aceita imdb.com): na Samsung so pelo servico de recomendacoes (#136).
    if (!trailerfonte_tizen() || trailerfonte_imdb_tizen()) trailerimdb_pedir(ci->imdb);
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
             NV_TRAILER_HERO_PREPARA_MS, trailerfonte_nome(heroTrailerQual),
             trailer_estado(),
             trailerfonte_depois_destaque(trailerfonte_ajuste(), trailerfonte_tizen(), heroTrailerQual)
               ? "tenta a proxima fonte" : "desiste");
      fflush(stdout);
      trailer_fechar();
      heroTrailerFonteFalhou();
    }
  }
  // trailer_atualizar fecha a fonte que recebeu erro depois deste passo.
  // Falha anda na ordem REAL do destaque, diferente no TPK; ended/Voltar
  // continua sem fallback. A mascara impede reabrir uma fonte recusada.
  if (heroTrailerQual>0 && heroTrailerFonte>0 && heroTrailerFonte<3 &&
      !trailer_aberto() && trailer_falhou() && !(heroTrailerFalhas&(1u<<heroTrailerQual))) {
    printf("[home-trailer] source failed source=%s action=%s\n",trailerfonte_nome(heroTrailerQual),
           trailerfonte_depois_destaque(trailerfonte_ajuste(), trailerfonte_tizen(), heroTrailerQual)
             ? "try_next_source" : "keep_art");
    fflush(stdout);
    heroTrailerFonteFalhou();
  }
  decorrido = agora - heroTrailerDesde;
  if (pronto && ci && ci->imdb[0] && heroTrailerItem == heroAtual &&
      !trailer_aberto() && !heroTrailerTentado && !heroTrailerMemoriaFalhou &&
      !heroTrailerJaTocou(ci->imdb) &&
      agora - heroTrailerDesde >= heroTrailerEspera()) {
    // A ORDEM e a do ajuste "Fonte do trailer" (trailerfonte.h). Automatico:
    // Apple (HLS matted) antes do IMDb (MP4 com tarja, LG) ou do YouTube (id
    // da busca enxuta do TMDB, Samsung); uma fonte fixa e a unica tentada.
    //
    // A APPLE TEM A JANELA INTEIRA (ate heroTrailerMaxEspera) antes
    // da seguinte, tambem na Samsung. Antes, com um id do YouTube em maos, o
    // hero o abria ja aos 1,2 s se a Apple ainda nao tinha respondido — e a
    // resposta da Apple agora inclui baixar o master para escolher a variante
    // (trailerapple.c, varianteMidia), ~0,3 s a mais. No emulador isso deu
    // YouTube aos 90,1 s e a Apple pronta aos 90,4 s; o embed do YouTube nao
    // produziu `playing` em 3,5 s (na TV ele cai em "Video player
    // configuration error", #82/#86), e o trailer bom ficou de fora.
    // Vencida a janela, quem nao respondeu conta como "sem trailer" e a fila
    // anda (trailerfonte_escolher cede a vez a quem respondeu vazio).
    TrailerCandidatos c;
    TrailerDecisao d;
    const char *u = NULL;
    int qual = 0, venceu = decorrido >= heroTrailerMaxEspera();
    memset(&c, 0, sizeof c);
    c.apple = trailerapple_url(ci->imdb);
    c.appleRespondeu = trailerapple_respondeu(ci->imdb) || venceu;
    c.appleFalhou = heroTrailerAppleFalhou;
    if (!trailerfonte_tizen() || trailerfonte_imdb_tizen()) {
      c.imdb = trailerimdb_url(ci->imdb, NULL);
      c.imdbRespondeu = trailerimdb_respondeu(ci->imdb) || venceu;
    } else c.imdbRespondeu = 1;
    if(heroTrailerFalhas&(1u<<TRF_APPLE)){c.apple=NULL;c.appleRespondeu=1;c.appleFalhou=1;}
    if(heroTrailerFalhas&(1u<<TRF_IMDB)){c.imdb=NULL;c.imdbRespondeu=1;}
#ifdef __EMSCRIPTEN__
    // SEM YOUTUBE NO DESTAQUE DA SAMSUNG (#136). O iframe do embed custa
    // segundos de fio principal nesta TV — registro da AU7000: raf-max=1034
    // ms, longtask-max=934 ms atribuido ao iframe, FPS=6.7 — e depois cai no
    // erro 153 ("Video player configuration error") e o hero fecha sem
    // `playing` aos 6,5 s. O destaque troca a cada seta: e o pior lugar para
    // ele. Aqui so <video> (Apple, IMDb); o YouTube fica na pagina do titulo.
    // A lista do TMDB ainda e pedida (heroTrailerYoutube), mas nao abre.
    (void)heroTrailerYoutube;
    c.youtube = NULL;
    c.youtubeRespondeu = 1;
#else
    c.youtubeRespondeu = venceu;
#endif
    // _destaque: igual a trailerfonte_escolher, salvo no .tpk com
    // NV_TRAILER_CONTINUA_DETALHE (IMDb antes da Apple, para o trailer poder
    // continuar COM SOM na pagina do titulo; ver trailerfonte.h).
    d = trailerfonte_escolher_destaque(trailerfonte_ajuste(), trailerfonte_tizen(), &c, &u, &qual);
    if (d == TRF_ESPERA && !venceu) goto trailer_hero_fim;
    if (d == TRF_ABRE && u) {
      // The source kind is diagnostic state, not proof it is the last source:
      // native TPK prefers IMDb, so its Apple fallback may still remain.
      heroTrailerTentado = 1;
      heroTrailerFonte = qual==TRF_APPLE?1:2;
      heroTrailerQual = qual;
      heroTrailerPreparandoAte = heroTrailerPrazoPreparacao(agora);
      // SOM: so no destaque (o do cartaz em foco segue mudo) e so com o
      // ajuste. Na Samsung (.wgt) trailer_abrir forca mudo de qualquer jeito.
      trailer_abrir(u, heroArteRect, focoHero && ajustes_trailer_hero_som(), 0);
      trailer_marcar_dono(TRAILER_DONO_HOME, ci->imdb);
      // trailer_abrir e void por compatibilidade com o player nativo; no
      // browser ainda pode recusar a criacao (canvas ausente). Tratar isso
      // como erro da fonte evita deixar a tentativa marcada para sempre.
      if (!trailer_aberto()) {
        heroTrailerFonteFalhou();
      }
    } else {
      // Sem fonte depois do orçamento, deixa a arte e o carrossel seguirem.
      printf("[trailer] hero: sem fonte em %u ms (ajuste %d, apple %s), fica a arte\n", decorrido,
             trailerfonte_ajuste(),
             heroTrailerAppleFalhou ? "falhou" : trailerapple_respondeu(ci->imdb) ? "sem trailer" : "sem resposta");
      fflush(stdout);
      heroTrailerTentado = 1;
      heroTrailerFonte = 3;
      heroTrailerFade = 0.0f;
    }
  }
trailer_hero_fim:
  { float alvo = (heroTrailerItem >= 0 && heroTrailerItem == heroAtual &&
                  trailer_aberto() && !trailer_cheia() && trailer_tocando() &&
                  trailer_mostra_video()) ? 1.0f : 0.0f;   // .tpk: ate o recorte assentar (#178)
    heroTrailerFade = anim_mola(heroTrailerFade, alvo, dt, NV_MOLA_SCROLL);
    // O `playing` chegou: este titulo ja teve o seu trailer (ver heroTrailerTocou).
    if (alvo > 0.5f) heroTrailerMarcarTocou(heroTrailerImdb);
    // MODO CINEMA: so com o trailer do DESTAQUE tocando (foco no hero). No do
    // cartaz em foco as fileiras sao o assunto. Fora do topo zera de vez, senao
    // a volta de outra tela mostraria um quadro do estado velho.
    // O modo cinema tira as fileiras da tela para o trailer ficar inteiro. No
    // Padrao o trailer toca num BANNER e as fileiras sao o assunto da tela:
    // sem cinema, o texto e a arte seguem como estao.
    { int toca = topo && focoHero && ajustes_trailer_hero() && alvo > 0.5f &&
                 layoutHome() != HOME_LAYOUT_PADRAO;
      trailercinema_passo(&heroCinema, toca, dt, ajustes_animacoes_reduzidas());
      if (!topo) trailercinema_zerar(&heroCinema); } }
}

// NUMERAIS DO TOP 10 DA DINAMICA: o bloco inteiro ou nada (o mesmo principio do
// textogate.h). Um algarismo de 260 px e rasterizacao cara o bastante para o
// orcamento de text.c soltar um por quadro, e a fileira "contava" 1, 2, 3 na
// frente da pessoa. Pede os dez a cada quadro (dez consultas de cache depois
// de prontos) e so os revela, num esvanecer unico, quando todos existem; ai
// nao esconde mais. Teto de espera igual ao do portao de texto.
// Quase branco e um degrau translucido: grande assim, o branco cheio disputava
// com os cartazes; a 0,86 o fundo passa por ele e o numero fica
// atras do cartaz tambem no tom, sem perder leitura.
#define NUM_COR 232, 234, 240, 255
#define NUM_ALFA 0.86f
static Uint32 numPedidoEm[MAX_FIL], numProntoEm[MAX_FIL];
static float numeraisAlfa(int r, Uint32 agora) {
  if (r < 0 || r >= MAX_FIL) return 0.0f;
  if (!numProntoEm[r]) {
    int c, falta = 0;
    char rank[8];
    if (!numPedidoEm[r]) numPedidoEm[r] = agora ? agora : 1u;
    for (c = 0; c < fileiras[r].n; c++) {
      snprintf(rank, sizeof rank, "%d", c + 1);
      if (!txt_linha(TXT_RANK_GRANDE, rank, NUM_COR).tex) falta = 1;
    }
    if (falta && (Uint32)(agora - numPedidoEm[r]) < 400u) return 0.0f;
    numProntoEm[r] = agora ? agora : 1u;
  }
  if (ajustes_animacoes_reduzidas()) return 1.0f;
  return revela_saida((float)(Uint32)(agora - numProntoEm[r]) / 180.0f);
}

// --- O DESENHO DE UM CARTAO, EM PEDACOS ------------------------------------
//
// Saidos do laco de home_desenhar SEM MUDAR UMA LINHA do que desenham, para a
// previa do modal de estilo (home_previa_fileira) desenhar os cartoes com o
// MESMO codigo da fileira — a mesma arte, o mesmo shader, a mesma legenda —, e
// nao uma imitacao que diverge no primeiro ajuste. Quem muda o cartao aqui muda
// os dois lugares.

// O numeral grande do ranking, no vao `vao` a esquerda do cartaz.
static void desenhaNumeral(int pos, float px, float py, float h, float vao, float alfa) {
  char rank[8];
  snprintf(rank, sizeof rank, "%d", pos);
  TxtLinha nu = txt_linha(TXT_RANK_GRANDE, rank, NUM_COR);
  if (nu.tex) {
    float e = vao / ((1.0f - NV_TOP10_NUM_SOB) * (float)nu.w);
    if (e > 1.0f) e = 1.0f;
    { float nw = (float)nu.w * e, nh = (float)nu.h * e;
      float nx = px + nw * NV_TOP10_NUM_SOB - nw;
      float ny = py + h - nh * NV_TOP10_NUM_BASE;
      gfx_rect((GfxRect){ nx, ny, nw, nh }, nu.tex, GFX_TEXTO, 0, 0, 0, 0.0f,
               1, 1, 1, alfa * NUM_ALFA); }
  }
}

// A pilha do ranking empilhado: ate seis cartazes num card so. `s` e a escala
// da previa (1 na fileira).
static void desenhaPilha(int idxCat, int stackN, float px, float py, float h, float s) {
  // Sem placa de fundo: os cartazes empilhados ja formam o card.
  int count=stackN<6?stackN:6;
  for(int k=0;k<count;k++) {
    const CatItem *it=cat_item_exato(idxCat+k);if(!it)continue;
    GfxRect pr={px+(20+k*78)*s,py+18*s,178*s,h-72*s};
    const char *pa=arte_por_formato(it,0);
    GLuint tx=pa?tex_obter_larg(pa,178*s):0;
    if(tx){gfx_tex_aspect_atual=tex_aspecto(pa);gfx_rect(pr,tx,GFX_CARD,0,0,0,.055f,1,1,1,1);gfx_tex_aspect_atual=0;}
    else desenhaArteAusente(pr,.055f,it,1);
  }
  // O numero e o da fileira (issue #201): "primeiros 10" mentia
  // quando o Itens por fileira era 12, 18 ou 24.
  { char rot[96];
    snprintf(rot,sizeof rot,i18n("Ranking   ·   Explorar os %d primeiros"),stackN);
    txt_desenhar(txt_linha(TXT_CAPTION,rot,242,235,248,255),px+24*s,py+h-42*s); }
}

// A arte do cartao (ou o esqueleto / "arte indisponivel" sem ela). `varre` e a
// luz do foco; 0 fora do cartao em foco.
static void desenhaArteCard(GfxRect card, TipoFileira tipo, const char *caminho, GLuint t,
                            const CatItem *cItem, float f, float raio, float aArte,
                            float varre) {
  if (t && home_proximo_desfocar(cItem, caminho)) t = gfx_desfocado(t, caminho);
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
    // "RETOMAR AGORA" (680x178, ~3,8:1) TAMBEM: o fallback contain do
    // GFX_CARD dispara com a moldura 25% fora da arte, e qualquer arte (fundo
    // 16:9 ou cartaz) ficava no meio do card com faixas cinza dos lados (dono,
    // 03/10: "tem que colocar uma imagem cropada para preencher tudo").
    gfx_card_forcar_cover_atual = (tipo == FILEIRA_DESTAQUE_QUADRADO ||
                                   tipo == FILEIRA_RETORNO) ? 1.0f : 0.0f;
    gfx_tex_aspect_atual = tex_aspecto(caminho);
    // O esqueleto fica por baixo so enquanto a arte esvanece: um
    // desenho do tamanho do card por ~220 ms, e depois nenhum.
    if (aArte < 0.999f)
      gfx_cor(card, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G,
              NV_COR_ESQUELETO_B, 1.0f);
    gfx_varre_atual = varre;
    gfx_rect(card, t, GFX_CARD, f, 0.0f, 0.0f,
             raio, 0, 0, 0, aArte);
    // Cartaz assentado e opaco: o fundo adiado nao precisa ser pintado
    // embaixo dele (gfx_mascara_opaca; no-op fora do regime).
    if (aArte >= 0.999f) gfx_mascara_opaca(card, raio * card.h);
    gfx_varre_atual = 0.0f;
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
    //
    // CARREGANDO NAO E INDISPONIVEL. Enquanto o cache nao disse que a
    // arte falhou, o card mostra o esqueleto com a luz passando e o
    // nome; "Arte indisponivel" fica para quando ela nao vem mesmo.
    if (caminho && !tex_falhou(caminho)) desenhaCarregando(card, raio, cItem);
    else desenhaArteAusente(card, raio, cItem, 1.0f);
  }
}

// O veu da legenda do cartaz DEITADO: 1 e a fracao em `fVeu` se este card leva
// (as mesmas condicoes de desenhaRotuloCard, que o desenha). Separado para o
// laco da fileira saber o veu ANTES da arte (veusDoCard).
static int veuRotulo(const CatItem *cItem, TipoFileira tipo, int deitado, float *fVeu) {
  if (tipo == FILEIRA_CONTINUE || tipo == FILEIRA_RETORNO || editorial(tipo) ||
      !(ajustes_rotulos_poster() || tipo == FILEIRA_LARGA) || !cItem ||
      !deitado || !cItem->titulo[0]) return 0;
  *fVeu = tipo == FILEIRA_LARGA ? NV_DIN_LARGA_VEU : NV_LAND_VEU;
  return 1;
}
#define NV_ROTULO_VEU_A 0.86f

// OS VEUS DA BASE VAO DENTRO DA ARTE (gfx.h, gfx_veu_card_atual). Fotos do dono
// (01/10/2026): "o card da fileira ta com o overlay sobrando um pouco na borda
// inferior". MEDIDO em homelayouts_shot: a arte e o veu, cada um com a sua
// cobertura de borda, no pixel da borda deixavam a arte com menos da metade
// do veu — a ultima linha do card saia MAIS CLARA que a de cima (45 contra 37
// num card aberto sobre fundo 6), um fio da arte por baixo do veu, que na TV
// (720p esticada para o painel) vira traco. Com o veu no mesmo fragmento a
// arte e escurecida inteira e so depois a borda recorta. As condicoes sao as
// de quem desenha cada legenda; gfx_veu_base pula so o veu que estiver aqui.
// `comCw`: o card leva o conteudo de continuar_desenhar (so o laco da home).
static void veusDoCard(const CatItem *cItem, TipoFileira tipo, int deitado, float abre,
                       int comCw) {
  float fVeu;
  if (veuRotulo(cItem, tipo, deitado, &fVeu)) gfx_veu_card_por(fVeu, NV_ROTULO_VEU_A);
  if (comCw && (tipo == FILEIRA_CONTINUE || tipo == FILEIRA_RETORNO) && cItem)
    gfx_veu_card_por(NV_CW_VEU_F, NV_CW_VEU_A);
  if (abre > 0.01f && cItem) gfx_veu_card_por(NV_ABERTA_VEU_F, NV_ABERTA_VEU_A * abre);
  if (editorial(tipo)) gfx_veu_card_por(NV_EDITORIAL_VEU_F, NV_EDITORIAL_VEU_A);
}

// A legenda do cartaz (dentro do deitado, abaixo do cartaz em pe).
static void desenhaRotuloCard(const CatItem *cItem, TipoFileira tipo, int deitado,
                              int rotuloFora, float px, float py, float w, float h,
                              float raio) {
  if (tipo != FILEIRA_CONTINUE && tipo != FILEIRA_RETORNO && !editorial(tipo) &&
      (ajustes_rotulos_poster() || tipo == FILEIRA_LARGA) && cItem) {
    const char *nome = cItem->titulo[0] ? cItem->titulo : NULL;
    const char *sub  = cItem->genero[0] ? cItem->genero : NULL;
    if (deitado && nome) {
      // VEU SO VERTICAL, que chega a ZERO no topo do retangulo. Era o
      // GFX_VEU (base + ESQUERDA): a rampa da esquerda ja vale 0,78 no
      // alto do retangulo, e o que se via era uma placa escura de canto
      // arredondado no meio da arte — degrau duro, com canto. O raio vai
      // convertido para a altura DESTE retangulo (o shader mede o raio
      // pela altura), senao o canto de baixo do veu fica mais fechado
      // que o do cartaz e o escuro vaza pela curva. Mesmo retangulo,
      // mesmo fill de antes.
      // gfx_veu_base e o mesmo retangulo, o mesmo raio e o mesmo fill; com a
      // arte na tela ele ja saiu dentro dela (veusDoCard) e aqui nao repete.
      const int larga = tipo == FILEIRA_LARGA;
      const float fVeu = larga ? NV_DIN_LARGA_VEU : NV_LAND_VEU;
      gfx_veu_base((GfxRect){ px, py, w, h }, raio, fVeu, NV_ROTULO_VEU_A);
      float maxW = w * NV_LAND_COPY_MAXW;
      float bx = px + (larga ? NV_DIN_LARGA_PAD : NV_LAND_COPY_PAD);
      float base = larga ? NV_DIN_LARGA_PAD - 4.0f : NV_LAND_COPY_BASE;
      // A faixa da Dinamica e cartao de LER do sofa: nome no corpo de
      // botao (25) e a linha de baixo em legenda (21), e nao os 22/15
      // do cartaz deitado da Moderna, que vem do web.
      TxtLinha tn = txt_linha_corta(larga ? TXT_BODY : TXT_CAPTION, nome,
                                    245, 246, 250, 255, maxW);
      if (sub) {
        TxtLinha ts = txt_linha_corta(larga ? TXT_CAPTION2 : TXT_MINI, sub,
                                      200, 202, 210, 255, maxW);
        txt_desenhar_alpha(ts, bx, py + h - base - ts.h, 0.85f);
        txt_desenhar_alpha(tn, bx,
                           py + h - base - ts.h - (larga ? 2.0f : 4.0f) - tn.h, 0.98f);
      } else {
        txt_desenhar_alpha(tn, bx, py + h - base - tn.h, 0.98f);
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
}

// O numero pequeno do ranking empilhado aberto, sobre o canto do cartaz.
static void desenhaRankPequeno(int pos, float px, float py, float h) {
  char rank[8];snprintf(rank,sizeof rank,"%d",pos);
  TxtLinha number=txt_linha(TXT_RANK,rank,240,241,245,255);
  TxtLinha ink=txt_linha(TXT_RANK,rank,16,17,20,255);
  float nx=px-12,ny=py+h-number.h-8;
  for(int dx=-2;dx<=2;dx+=2)for(int dy=-2;dy<=2;dy+=2)
    txt_desenhar(number,nx+dx,ny+dy);
  txt_desenhar(ink,nx,ny);
}

// O que o cartao editorial (destaque, colecao, servico, 4:3) leva DENTRO da
// arte: veu na base, logo ou nome, genero, nota.
static void desenhaEditorialCard(const CatItem *cItem, TipoFileira tipo, float px, float py,
                                 float w, float h, float raio) {
  // Base e nao cartao inteiro: logo, nome e genero moram no terco de
  // baixo. O GFX_VEU inteiro era 1,18 tela na "AI for you" e a C9
  // parada nela ficava a 48 fps; sem ele, 60 (29/09).
  gfx_veu_base((GfxRect){ px, py, w, h }, raio, NV_EDITORIAL_VEU_F, NV_EDITORIAL_VEU_A);


  // Logo do titulo, como no aparelho: cada producao tem tipografia
  // propria, e escrever o nome com a fonte da interface apaga isso.
  const CatItem *ci = cItem;
  const char *urlCl = ci ? artehero_logo_sessao_larg(ci, w * .65f) : NULL;
  GLuint tlogo = urlCl ? tex_obter_logo_larg(urlCl, w * .65f) : 0;
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
  // PADRAO E DINAMICA SEGUEM A HOME ORIGINAL DO NUVIO (captura do
  // dono, 29/09): o logo do titulo GRANDE no canto de baixo (~28% da
  // altura do cartao, contra os 22% da Moderna) e, sem logo, o nome
  // em Bold de titulo, nao no corpo do card de Continuar. A Moderna
  // fica como estava (baseline byte a byte de homelayouts_shot).
  const int orig = layoutHome() != HOME_LAYOUT_MODERNA && tipo == FILEIRA_DESTAQUE;
  if (tlogo) {
    float ap = tex_aspecto(urlCl);
    if (ap <= 0.0f) ap = 4.0f;
    hTit = h * (orig ? .28f : .22f);
    float wTit = hTit * ap, maxW = w * (orig ? .55f : .65f);
    if (wTit > maxW) { wTit = maxW; hTit = wTit / ap; }
    GfxRect rl = { px + pad, yMeta - hTit - 10.0f, wTit, hTit };
    gfx_tex_aspect_atual = 0.0f;
    { GfxModo m = tex_marca_escura(urlCl) ? GFX_MARCA : GFX_TEXTO;
    gfx_rect(rl, tlogo, m, 0, 0, 0, 0.0f, 1, 1, 1, 1.0f); }
  } else if (nome) {
    TxtLinha tn = txt_linha_corta(orig ? TXT_TITULO3 : TXT_CW_TITULO, nome,
                                  245, 246, 249, 255, w - pad*2);
    hTit = (float)tn.h;
    txt_desenhar(tn, px + pad, yMeta - hTit - (orig ? 6.0f : 10.0f));
  } else {
    hTit = 0.0f;
  }
  if (genero) txt_desenhar(tg, px + pad, yMeta);

  // A LINHA DE BAIXO DA HOME ORIGINAL e "tipo · genero · nota": a nota
  // entra com o selo IMDb que o app usa em toda tela (hero, card de
  // Continuar, faixa do card aberto), e nao com uma estrela solta —
  // um so vocabulario de nota. Com o selo, a classificacao etaria sai
  // da linha (a referencia nao a tem); sem nota atribuivel ao IMDb
  // (sem ID, ou `tmdb:` sem vínculo IMDb verificado), a classificacao volta como antes.
  int notaFeita = 0;
  int imdbRating = orig && ci ? imdbnota_obter(ci->imdb, ci->nota, !strcmp(ci->tipo,"series")) : 0;
  if (imdbRating > 0) {
    float bx = px + pad + tg.w + (genero ? 14.0f : 0.0f);
    if (bx + badge_imdb_largura(imdbRating) < px + w - pad) {
      badge_imdb(bx, yMeta + (tg.h - BADGE_H) * 0.5f, imdbRating, 0, 1.0f);
      notaFeita = 1;
    }
  }

  // Selo etario vermelho, a direita da linha de genero. SO COM VALOR:
  // o "16" de reserva que estava aqui carimbava uma faixa etaria em
  // todo card sem classificacao, e o selo vermelho tem cara de aviso
  // oficial — e o mesmo defeito do "14" cravado em descoberta.c, so
  // que na home.
  if (!notaFeita && ci && ci->classificacao[0] && tg.w + BADGE_H + 24.0f < w - pad*2) {
    char clas[8];
    snprintf(clas, sizeof clas, "%s%s", (ci->classificacao[0] >= '0' && ci->classificacao[0] <= '9') ? "A" : "", ci->classificacao);
    { float bx = px + pad + tg.w + (genero ? 14.0f : 0.0f);
      badge_desenhar(bx, yMeta + (tg.h - BADGE_H) * 0.5f, clas,
                     BADGE_NEUTRO, 0.95f); }
  }
}

// --- PREVIA DO ESTILO DA FILEIRA (modal do cartaz, ctxmenu.c) ---------------
//
// A fileira `chave` desenhada na forma `filTipo` (FilTipo de fileiras.h) dentro
// de `area`, com as ARTES DOS ITENS DELA e os mesmos pedacos de cartao do laco
// de home_desenhar (desenhaArteCard, desenhaRotuloCard, desenhaEditorialCard,
// desenhaNumeral, desenhaPilha). As medidas sao as da fileira de verdade
// (larguraDe/alturaDe/gapDe, com o fator de tamanho dela), reduzidas por UM
// fator para caberem: cabe a terceira arte espiando, e a altura nunca passa da
// area. E desenhada so enquanto o modal pede — nada aqui roda com ele fechado.
//
// Sem foco, sem expansao, sem cascata: e a fileira em repouso, que e o que a
// pessoa vai ver ao voltar para a Home. 0 quando a fileira nao esta na Home
// montada (nada a mostrar).
// A forma desenhada para `filTipo` nesta fileira e a medida dela, ja com o
// fator de tamanho. Separada porque o modal pede DUAS: a da forma em foco e a
// de referencia, que decide a reducao (ver home_previa_fileira).
static TipoFileira previaMedida(const Fileira *fl, const char *chave, int filTipo,
                                int *forma, int *pilha, float *lw, float *lh) {
  TipoFileira tipo;
  float e;
  *forma = fl->forma;
  if (fl->tipo == FILEIRA_CATALOGOS) {
    // Os numeros de fil_estilos para colecao (fileiras.c), como em
    // sincronizarFileiras.
    tipo = FILEIRA_CATALOGOS;
    if (filTipo == FIL_TIPO_COLECAO) *forma = COL_FORMA_PAISAGEM;
    else if (filTipo == FIL_TIPO_DESTAQUE_QUADRADO) *forma = COL_FORMA_QUADRADO;
    else if (filTipo == FIL_TIPO_CARTAZ) *forma = COL_FORMA_POSTER;
    else *forma = fl->formaAuto;
  } else if (filTipo == FIL_TIPO_AUTO) {
    // Em Automatico agora: a forma desenhada E a automatica (com a da
    // Dinamica). Com outra escolha, a de antes dela.
    tipo = fil_tipo(chave) == FIL_TIPO_AUTO ? fl->tipo : fl->tipoAuto;
  } else tipo = tipoDaEscolha(filTipo);
  // A mesma condicao de sincronizarFileiras para a pilha virar um card so.
  *pilha = tipo == FILEIRA_TOP10 && fl->base[0] && fl->catId[0];
  // O fator da forma EM FOCO, e nao o da gravada (fl->escala): o 4:3 grande
  // ainda nao foi escolhido quando a previa o mostra, e o Automatico de uma
  // fileira gravada no 4:3 grande nao herda o 1,5 dele.
  e = fil_escala_tipo(chave, filTipo);
  if (tipo == FILEIRA_DESTAQUE_QUADRADO) e = limita43(fil_tipo_fator(filTipo), e);
  if (*pilha) { *lw = 680.0f; *lh = alturaDe(FILEIRA_TOP10); }
  else if (tipo == FILEIRA_CATALOGOS) medidaColecao(*forma, lw, lh);
  else { *lw = larguraDe(tipo); *lh = alturaDe(tipo); }
  *lw *= e; *lh *= e;
  return tipo;
}

int home_previa_fileira(const char *chave, int filTipo, int refTipo, GfxRect area,
                        float alfa) {
  const Fileira *fl = NULL;
  TipoFileira tipo;
  int r, c, nItens, forma, pilha, deitado, rotuloFora;
  float lw, lh, gap, xoff, rotH, s, w, h, passo, raio, x0, y0, og;
  if (!chave || !chave[0] || alfa < 0.01f) return 0;
  for (r = 0; r < nFileiras; r++)
    if (!strcmp(fileiras[r].chave, chave)) { fl = &fileiras[r]; break; }
  if (!fl) return 0;
  nItens = fl->stackN ? fl->stackN : fl->n;
  if (nItens < 1) return 0;
  tipo = previaMedida(fl, chave, filTipo, &forma, &pilha, &lw, &lh);
  gap = gapDe(tipo); xoff = xOffTipo(tipo);
  deitado = editorial(tipo) || tipo == FILEIRA_LARGA ||
            ((tipo != FILEIRA_CONTINUE) && ajustes_posteres_deitados());
  rotuloFora = temRotulo(tipo);
  rotH = rotuloFora ? NV_POSTER_COPY_H : 0.0f;
  // A REDUCAO SAI DA FORMA DE REFERENCIA. Sozinha, cada forma era reduzida
  // para caber 2,6 cards — e as tres paisagens, ou os tres 4:3, saiam do MESMO
  // tamanho na previa, que e justamente a diferenca que a pessoa esta
  // escolhendo. Com `refTipo` = o maior tamanho da linha, ele enche o palco
  // como antes e os menores aparecem menores, na proporcao de verdade.
  { int fR, pR; float lwR = lw, lhR = lh, gapR = gap, xoffR = xoff, rotR = rotH;
    if (refTipo != filTipo && refTipo >= 0 && refTipo < FIL_TIPO_N) {
      TipoFileira tR = previaMedida(fl, chave, refTipo, &fR, &pR, &lwR, &lhR);
      gapR = gapDe(tR); xoffR = xOffTipo(tR);
      rotR = temRotulo(tR) ? NV_POSTER_COPY_H : 0.0f;
    } else pR = pilha;
    s = area.w / (xoffR + (pR ? 1.0f : 2.6f) * (lwR + gapR));
    if (s > 1.0f) s = 1.0f;
    if ((lhR + rotR) * s > area.h) s = area.h / (lhR + rotR);
    if ((lh + rotH) * s > area.h) s = area.h / (lh + rotH); }
  w = lw * s; h = lh * s; passo = (lw + gap) * s; raio = raioDe(w, h);
  x0 = area.x + xoff * s;
  // Rente ao topo, como a fileira fica sob o titulo dela na Home.
  y0 = area.y;
  og = gfx_opacidade_grupo;
  gfx_opacidade_grupo = alfa;
  gfx_recorte(area.x, area.y, area.w, area.h);
  if (pilha) {
    desenhaPilha(fl->usaItens ? fl->itens[0] : fl->ini, nItens, x0, y0, h, s);
  } else {
    for (c = 0; c < nItens && c < 8; c++) {
      float px = x0 + (float)c * passo;
      GfxRect card = { px, y0, w, h };
      int idx = fl->usaItens ? fl->itens[c] : fl->ini + c;
      if (px > area.x + area.w) break;
      if (tipo == FILEIRA_CATALOGOS) {
        // O cartao parado de desenhaAtalhos: esqueleto, capa, profundidade.
        const ColFolder *folder = col_folder(fl->folders[c]);
        const char *arte = folder ? col_capa(folder) : NULL;
        GLuint tex = arte && arte[0] ? tex_obter_larg(arte, w) : 0;
        gfx_cor(card, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, 1);
        if (tex) {
          gfx_tex_aspect_atual = tex_aspecto(arte);
          gfx_rect(card, tex, GFX_CARD, 0, 0, 0, raio, 0, 0, 0, 1);
          gfx_tex_aspect_atual = 0;
          desenhaProfundidade(card, raio, ajustes_profundidade_posters());
        }
        continue;
      }
      { const CatItem *ci = cat_item_exato(idx);
        const char *cam = arte_por_identidade(idx, tipo == FILEIRA_CONTINUE ? 3 : deitado);
        GLuint t = cam ? tex_obter_larg(cam, w) : 0;
        if (tipo == FILEIRA_TOP10_NUM)
          desenhaNumeral(c + 1, px, y0, h, passo - w - NV_TOP10_NUM_FOLGA * s, 1.0f);
        gfx_veu_card_limpar();
        if (t) veusDoCard(ci, tipo, deitado, 0.0f, 0);
        desenhaArteCard(card, tipo, cam, t, ci, 0.0f, raio, 1.0f, 0.0f);
        if (t) gfx_veu_na_arte = 1; else gfx_veu_card_limpar();
        desenhaProfundidade(card, raio, ajustes_profundidade_posters());
        desenhaRotuloCard(ci, tipo, deitado, rotuloFora, px, y0, w, h, raio);
        if (editorial(tipo)) desenhaEditorialCard(ci, tipo, px, y0, w, h, raio);
        gfx_veu_card_limpar(); }
    }
  }
  gfx_sem_recorte();
  gfx_opacidade_grupo = og;
  return 1;
}

// O CARTAO FOCADO, PINTADO DE UM LUGAR SO. A home o pinta na fileira e o menu do
// cartaz (ctxmenu.c) o pinta DE NOVO por cima do veu: a mesma funcao, com os
// mesmos parametros, para o cartao nao mudar de arte, recorte, rotulo ou logo
// quando o menu abre (home_cartao_foco_por_cima).
typedef struct {
  int r, c; TipoFileira tipo; const CatItem *cItem; const char *caminho; GLuint t;
  float aArte; int deitado, rotuloFora; float abre, esc, f, px, py, w, h, varre;
} CartaoFoco;
static CartaoFoco cartaoFoco; static float cartaoFocoRaio; static int temCartaoFoco;
// O caminho da arte mora num buffer que o proximo cartao sobrescreve: copia propria.
static char cartaoFocoArte[1024];
// SELO DE ASSISTIDO: pilula escura translucida (material da ilha) com um check
// no acento e a palavra "Assistido". Altura e recuo acompanham a largura do
// cartao (26..32); abaixo de 150 de largura so o check, num disco da mesma
// altura. Sombra rasa separa o selo de um poster claro.
static float desenhaSeloVisto(float px, float py, float w) {
  float h = w * 0.13f, m = w * 0.06f, ic, pw;
  float ar, ag, ab;
  int compacto = w < 150.0f;
  TxtLinha t = { 0 };
  if (h < 26.0f) h = 26.0f;
  if (h > 32.0f) h = 32.0f;
  if (m < 8.0f) m = 8.0f;
  if (m > 14.0f) m = 14.0f;
  ic = h * 0.58f;
  if (!compacto) t = txt_linha(TXT_CAPTION, i18n("Assistido"), 240, 240, 245, 255);
  pw = compacto ? h : h * 0.36f + ic + 6.0f + (float)t.w + h * 0.46f;
  { float mx = px + w - pw - m, my = py + m;
    ajustes_acento(&ar, &ag, &ab);
    gfx_cor((GfxRect){ mx, my + 3.0f, pw, h }, 0.5f, 0, 0, 0, 0.28f);
    gfx_cor((GfxRect){ mx, my, pw, h }, 0.5f, 0.05f, 0.055f, 0.067f, 0.80f);
    { float ix = compacto ? mx + (h - ic) * 0.5f : mx + h * 0.36f;
      gfx_icone((GfxRect){ ix, my + (h - ic) * 0.5f, ic, ic }, "check", ar, ag, ab, 1.0f);
      if (!compacto) txt_desenhar(t, ix + ic + 6.0f, my + (h - t.h) * 0.5f); }
    return mx; }
}

// CHIP DE AMIGOS no canto de cima A ESQUERDA do cartaz (amigostitulo.h): ate 2
// rostos + "+N". Fica longe do titulo (embaixo) e do selo "Assistido" (a
// direita): `limiteX` e a borda esquerda desse selo, e o chip encolhe para nao
// passar dela.
static void desenhaChipAmigos(const CatItem *ci, float px, float py, float w, float limiteX, float a) {
  AmigosTitulo at;
  float h = w * 0.17f, m = w * 0.045f;
  if (!ci || !ci->imdb[0] || w < 110.0f) return;
  if (!amigostitulo_obter(ci->imdb, &at)) return;
  if (h < 28.0f) h = 28.0f;
  if (h > 38.0f) h = 38.0f;
  if (m < 8.0f) m = 8.0f;
  if (m > 14.0f) m = 14.0f;
  svd_amigos_chip(px + m, py + m, h, limiteX - 6.0f - (px + m), &at, a);
}

static void pintarCartao(const CartaoFoco *k, float raio) {
  const int r = k->r, c = k->c; const TipoFileira tipo = k->tipo; const CatItem *cItem = k->cItem;
  const char *caminho = k->caminho; const GLuint t = k->t; const float aArte = k->aArte;
  const int deitado = k->deitado, rotuloFora = k->rotuloFora;
  const float abre = k->abre, esc = k->esc, f = k->f, px = k->px, py = k->py, w = k->w, h = k->h;
  const float varreFoco = k->varre;
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
              if (ajustes_vidro()) gfx_vidro_cartao((GfxRect){px, py, w, h}, raio, f, 1.0f);
              else
              gfx_cor(borda, (raio * menor + NV_ANEL_FOCO) / (menor + 2 * NV_ANEL_FOCO),
                      ar, ag, ab, f); }
          }
          GfxRect card = { px, py, w, h };
          // CARD SEM ARTE: superficie solida, nao o vazio. Sem isto o card
          // ficava da cor do fundo — MEDIDO: #242429 sobre #252629, diferenca
          // de (1,2,0), contraste 1,0:1. Era literalmente invisivel, e foi a
          // origem da queixa "nao aparecem todos os posteres": eles apareciam,
          // do tom exato do fundo. A referencia desenha #2C2C2C na caixa exata.
          gfx_veu_card_limpar();
          if (t) veusDoCard(cItem, tipo, deitado, abre, 1);
          desenhaArteCard(card, tipo, caminho, t, cItem, f, raio, aArte,
                          varreFoco);
          if (t) gfx_veu_na_arte = 1; else gfx_veu_card_limpar();
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
          //
          // O ESTADO E cat_visto (#212), nao mais `progresso >= 90`: o
          // progresso so existe para o que esta pausado, entao filme visto
          // no Trakt (ou marcado pelo menu, que zera o progresso) nunca
          // ganhava o selo. Leitura O(1) por cartaz (hash em catalogo.c),
          // sem pedido de rede: o mapa ja veio no ciclo da descoberta.
          float limiteSelo = px + w;
          if (cItem && tipo != FILEIRA_CONTINUE && tipo != FILEIRA_RETORNO &&
              ajustes_selo_visto() && cat_visto(cItem)) {
            limiteSelo = desenhaSeloVisto(px, py, w);
          }
          if (cItem && tipo != FILEIRA_CONTINUE && tipo != FILEIRA_RETORNO)
            desenhaChipAmigos(cItem, px, py, w, limiteSelo, aArte);

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
          // A faixa deitada da Dinamica SEMPRE leva o titulo dentro do cartao (a
          // Apple TV nao deixa cartao sem nome), com o rotulo ligado ou nao.
          desenhaRotuloCard(cItem, tipo, deitado, rotuloFora, px, py, w, h, raio);

          if(tipo==FILEIRA_TOP10) desenhaRankPequeno(c+1, px, py, h);

          if (tipo == FILEIRA_CONTINUE)
            continuar_desenhar(cItem, (GfxRect){px, py, w, h}, raio);
          if (tipo == FILEIRA_RETORNO)
            continuar_desenhar(cItem, (GfxRect){px, py, w, h}, raio);

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
            GLuint tl = tex_obter_logo_larg(urlL, w * 0.65f);
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
  if (editorial(tipo)) desenhaEditorialCard(cItem, tipo, px, py, w, h, raio);
  gfx_veu_card_limpar();
}

// Chamado pelo menu do cartaz, DEPOIS do veu: repinta o cartao focado inteiro
// (anel, arte, veus, rotulo, logo) onde a home o pintou neste quadro. 0 se o
// cartao focado nao foi pintado (destaque, pasta, pilha).
int home_cartao_foco_por_cima(int indice) {
  float og;
  if (!temCartaoFoco || !temItemFoco || itemFoco.indice != indice) return 0;
  og = gfx_opacidade_grupo;
  gfx_opacidade_grupo = 1.0f;
  pintarCartao(&cartaoFoco, cartaoFocoRaio);
  gfx_opacidade_grupo = og;
  return 1;
}

void home_desenhar(Uint32 agora) {
  amigostitulo_atualizar();   // barato: so remonta quando o feed social mudou
  // O REBORDO DO CARTAZ EM FOCO e ajuste da pessoa, e ele mora no shader do
  // GFX_CARD (nao e um retangulo desenhado por cima): por isso vai por uma
  // variavel de modulo, uma vez por quadro, e nao em cada chamada.
  gfx_borda_foco_atual = ajustes_borda_foco() ? 1.0f : 0.0f;
  desenhaFundo(agora);
  float pd = detail_progresso();
  if (ajustes_hero_ligado()) desenhaHero(agora, pd);
  if (ajustes_hero_ligado()) {
    float heroBaixo = layoutHome() == HOME_LAYOUT_DINAMICA
                    ? dinHeroY() + NV_DIN_HERO_H : corteFileiras();
    if (heroBaixo > 0.0f)
      ponteiro_alvo(0, 0, NV_TELA_W, heroBaixo, ponteiroHero, NULL, 0, 0);
  }

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
  if (pd >= 0.996f) return;
  // MODO CINEMA: as fileiras descem NV_CINEMA_DESCE e apagam (o mesmo que o
  // bloco do hero faz), para o trailer ficar inteiro. Assentado, NAO SE
  // DESENHAM: nem fileiras, nem cartazes, nem o aviso de "cabem mais fileiras".
  const float cinema = trailercinema_t(&heroCinema);
  const int fileirasOcultas = cinema >= 0.996f;
  descida += cinema * NV_CINEMA_DESCE;   // detalhe assentado: nada da home aparece

  // VIEWPORT DAS FILEIRAS. `.home-modern-rows-viewport` (components.css:6929) e
  // um bloco absoluto com bottom:0, height 52% e overflow-y:auto — ou seja as
  // fileiras rolam DENTRO dos 52% de baixo e o que sobe alem disso e CLIPADO.
  // O port desenhava as fileiras soltas sobre a tela inteira, e por isso a
  // fileira que saia por cima aparecia atravessada no bloco do hero em vez de
  // sumir. O hero nao rola: so o conteudo dele muda com o foco.
  const float corte = corteFileiras();
  // Sem prateleira nem contorno na Dinamica (dono, 30/09): as fileiras ficam
  // direto sobre o fundo e o cartao rola ate a beira da tela, como nos outros.
  gfx_recorte(0.0f, corte, NV_TELA_W, NV_TELA_H - corte);
  const float topoFil = topoFileiras();
  float y = topoFil - scrollY + descida + bordaPag.x;
  // NENHUMA FILEIRA. Nao e o arranque (ali a home mostra o catalogo do pacote
  // ou o do cache): e o caso de a pessoa ter desligado todas em Ajustes. Sem
  // texto, o hero sozinho com o resto da tela vazia le como travamento — e ela
  // nao teria como adivinhar que foi ela quem apagou a home.
  if (nFileiras < 1 && !fileirasOcultas) {
    float tx = ajustes_conteudo_x();
    TxtLinha t = txt_linha(TXT_ROW_TITULO, "Nenhuma fileira ativa", 240, 241, 245, 255);
    txt_desenhar(t, tx, topoFil);
    txt_bloco(TXT_CAPTION,
              "Ative fileiras em Ajustes, na categoria Fileiras da Home.",
              183, 186, 194, tx, topoFil + t.h + 14.0f,
              NV_TELA_W - tx - NV_HOME_SAFE_RIGHT, 34, 1, 2);
  }
  // ENTRADA EM CASCATA. Reentra quando as fileiras ficaram fora da tela por
  // um tempo — MAS NAO na volta do detalhe: ali elas ja sobem de volta pela
  // mola do proprio detalhe (`descida`), e duas animacoes no mesmo gesto
  // descasariam.
  int reentra = (!fileirasVistasEm || agora - fileirasVistasEm > 1500u) && pd <= 0.001f;
  if (reentra) heroAutoDesligado = 0;   // outra visita a home: o carrossel volta
  int ordemFil = 0;
  fileirasVistasEm = agora ? agora : 1u;
  temCartaoFoco = 0;
  // A LUZ DO FOCO: uma varredura por foco novo, nenhuma com a tecla presa.
  float varreFoco = revela_varre(&revVarre,
                                 focoHero ? -1 : foco.fileira * 64 + foco.coluna, agora);
  for (int r = 0; r < (fileirasOcultas ? 0 : nFileiras); r++) {
    TipoFileira tipo = fileiras[r].tipo;
    float fade=anim_clamp((y-(topoFil-80))/80,0,1);
    // Dinamica: as fileiras de cima do foco FICAM (rolagem centrada); so a que
    // sai pela borda de cima apaga, e pela metade dela, nao pelo titulo — senao
    // cartoes ainda na tela sumiriam junto com o titulo cortado.
    if (layoutHome() == HOME_LAYOUT_DINAMICA) {
      float meia = 0.5f * (NV_LEGACY_ROW_HEAD_H + alturaTotalFil(r));
      fade = anim_clamp((y + meia) / meia, 0, 1);
    }
    // A FILEIRA DE CIMA ESPIA (dono: "quando desce, a fileira de cima some —
    // nao deveria mostrar um pedaco dela?"). O esvanecer acima se mede pelo
    // CENTRO da fileira (Dinamica). Se a de
    // cima e ALTA (destaque 4:3, ranking), o centro dela ja esta fora da tela
    // quando a de baixo ganha o foco e a metade de baixo — que continua na
    // tela — saia com opacidade zero: um vazio preto no alto. Medido pela BASE,
    // a fatia que sobra aparece, mais fraca quanto menos sobra (0 com a base
    // fora da tela). Nunca escurece o que o calculo de cima ja deixava claro.
    if (layoutHome() == HOME_LAYOUT_DINAMICA) {
      float base = y + NV_LEGACY_ROW_HEAD_H + alturaTotalFil(r);
      float fatia = anim_clamp(base / 480.0f, 0, 1);
      if (fatia > fade) fade = fatia;
    }
    gfx_opacidade_grupo=fade*fade*(3-2*fade)*(1.0f-cinema);
    const float grupoFil = gfx_opacidade_grupo;
    { int n = fileiras[r].n;
      int visivel = y < NV_TELA_H && y + NV_LEGACY_ROW_HEAD_H + alturaFil(r) > corte;
      // Fileira que acabou de ganhar itens (o catalogo chegou) ou a volta
      // depois de uma ausencia. So as VISIVEIS: as de baixo ja estarao
      // assentadas quando o foco descer ate elas.
      if (n > 0 && visivel && (reentra || filNAntes[r] == 0))
        filEntraEm[r] = agora + (Uint32)(ordemFil * NV_ENTRA_FIL_MS);
      else if (!visivel)
        filEntraEm[r] = 0;
      if (visivel && n > 0) ordemFil++;
      filNAntes[r] = n;
      // Assentou de vez: zera para o card perguntar de graca.
      if (filEntraEm[r] && (Sint32)(agora - filEntraEm[r]) >
          (Sint32)(NV_ENTRA_MS + NV_ENTRA_PASSO_MS * NV_ENTRA_MAX_COL + 40))
        filEntraEm[r] = 0;
      gfx_opacidade_grupo = grupoFil * revela_entra(filEntraEm[r], 0.0f, agora); }
    float lw = larguraFil(r);
    float lh = alturaFil(r), passo = passoFil(r);
    float artH = lh;
    // `y` é o topo do cabeçalho da fileira; os cards começam depois do título.
    // Separar os dois evita que o título da fileira seguinte seja desenhado
    // sobre a arte da anterior quando a fileira tem cards altos.
    float cardY = y + NV_LEGACY_ROW_HEAD_H;

    int deitado = editorial(tipo) || tipo == FILEIRA_LARGA ||
                  ((tipo != FILEIRA_CONTINUE) && ajustes_posteres_deitados());
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
      // A FONTE (Trakt, Simkl...) NAO APARECE NA FILEIRA DE AMIGOS: a pessoa
      // quer saber quem viu e o que achou, nao de onde veio o dado. Ela fica
      // so no perfil do amigo (amigoperfil.c). Era a marca do Trakt aqui.
      if(!strncmp(fileiras[r].catId,"ai_",3)) {
        TxtLinha ai=txt_linha(TXT_HERO_META,"AI-powered",183,192,219,255);
        txt_desenhar(ai,ajustes_conteudo_x()+tl.w+22,y+(tl.h-ai.h)*.5f);
      }
      if (foco.fileira == r && !focoHero && tipo != FILEIRA_SOCIAL) {
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
      if (tipo == FILEIRA_SOCIAL) {
        amigosfil_desenhar(ajustes_conteudo_x() + bordaX(r), cardY, alturaFil(r), corte,
                           foco.fileira == r && !focoHero, foco.coluna, agora);
        if (foco.fileira == r && !focoHero) temItemFoco = 0;
        y += NV_LEGACY_ROW_HEAD_H + alturaTotalFil(r) + fileiraGap();
        continue;
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
        float cx = ajustes_conteudo_x() + c * passo - scrollX[r] + lw * 0.5f
                 + xOffTipo(tipo) + bordaX(r);
        float cy = cardY + artH * 0.5f;
        if (cx > -lw * 1.5f && cx < NV_TELA_W + lw) {
          float px = cx - w * 0.5f, py = cy - h * 0.5f;
          // O foco pode aumentar o card. No 4:3 ele nao pode subir sobre o
          // titulo da fileira; a expansão acontece para baixo, preservando a
          // separação visual da referência.
          if (tipo == FILEIRA_DESTAQUE_QUADRADO && py < cardY) py = cardY;
          float raio = raioDe(w, h);
          GfxRect r0 = { px, py, w, h };
          alvoCard(px, py, w, h, r, c);
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

      const float numA = tipo == FILEIRA_TOP10_NUM ? numeraisAlfa(r, agora) : 0.0f;
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
          // Cascata: atraso pela coluna VISIVEL, nao pela absoluta — a
          // fileira rolada ate a coluna 20 entra pelo primeiro card da tela.
          float entra = 1.0f;
          if (filEntraEm[r]) {
            int c0 = passo > 0.0f ? (int)(scrollX[r] / passo) : 0;
            int ordem = c - c0;
            if (ordem < 0) ordem = 0;
            if (ordem > NV_ENTRA_MAX_COL) ordem = NV_ENTRA_MAX_COL;
            entra = revela_entra(filEntraEm[r], ordem * NV_ENTRA_PASSO_MS, agora);
          }
          gfx_opacidade_grupo = grupoFil * entra;
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
                   + empurra + (w - lw * esc) * 0.5f + xOffTipo(tipo) + bordaX(r);
          // Sem levantamento: no web o card focado nao sai do lugar. O que
          // desloca aqui e so a cascata de entrada, e so enquanto ela dura.
          float cy = cardY + artH * 0.5f + (1.0f - entra) * NV_ENTRA_DY;
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
          alvoCard(px, py, w, h, r, c);

          const int idxCat = fileiraItemIndice(&fileiras[r], c);
          // TOP 10 da Dinamica: o numeral mora no vao a esquerda do cartaz e o
          // cartaz, desenhado depois, cobre a ponta direita dele — como na
          // Apple TV. Grande (~60% da altura do cartaz, TXT_RANK_GRANDE), sem
          // sombra: sobre o fundo escuro ele nao precisa de
          // separacao, e a sombra deslocada lia como adesivo. Base do algarismo
          // na base do cartaz. O "10" encolhe para caber no vao em vez de
          // invadir o cartaz anterior. So aparece quando os dez ja existem
          // como textura (numeraisAlfa): entram juntos, nunca um a um.
          if (tipo == FILEIRA_TOP10_NUM && numA > 0.003f)
            desenhaNumeral(c + 1, px, py, h, passo - lw - NV_TOP10_NUM_FOLGA, numA);
          if(tipo==FILEIRA_TOP10 && fileiras[r].stackN) {
            desenhaPilha(idxCat, fileiras[r].stackN, px, py, h, 1.0f);
            if(foco.fileira==r)temItemFoco=0;
            continue;
          }
          if(tipo==FILEIRA_SOCIAL && fileiras[r].ini<0) {
            // ESTADO VAZIO NA COR DE REALCE (nao num roxo fixo) e com a tinta que
            // contrasta com ela; sem repetir "Entre amigos", que ja e o titulo da
            // fileira logo acima (revisao da home, 29/09/2026).
            GfxRect b={px,py,w,h};
            float ar,ag,ab; ajustes_acento(&ar,&ag,&ab);
            int t1=ajustes_tinta_foco(), t2=ajustes_tinta_foco2();
            gfx_cor(b,.055f,ar,ag,ab,1);
            if(f>.01f)gfx_rect(b,0,GFX_ANEL,0,.008f,0,.055f,t1/255.0f,t1/255.0f,t1/255.0f,f);
            txt_desenhar(txt_linha_corta(TXT_CALLOUT,"Nenhuma atividade disponível agora.",t1,t1,t1,255,w-48),px+24,py+24);
            txt_bloco(TXT_CAPTION,i18n("Siga pessoas no Trakt ou adicione amigos do Nuvio."),t2,t2,t2,px+24,py+91,w-48,30.0f,1.0f,2);
            txt_desenhar(txt_linha_corta(TXT_CAPTION,"OK · Conferir conexão",t1,t1,t1,255,w-48),px+24,py+h-50);
            if(foco.fileira==r)temItemFoco=0;
            continue;
          }
          const CatItem *cItem = cat_item_exato(idxCat);
          // LOGO DO CARD ABERTO JA NO FOCO. So era pedido quando o card abria
          // (abre > 0), depois do atraso de expansao: o download so comecava
          // ali, e o logo aparecia atras do card. Pedir agora, na largura do card
          // ABERTO (a mesma do desenho abaixo), usa esse atraso para baixar e
          // decodificar, e nao promove depois por largura diferente.
          if (cItem && cItem->logo[0] && abre <= 0.01f && r == expFileira &&
              c == expColuna && ajustes_expandir_poster() && podeExpandir(r)) {
            float wFim = artH * (1.0f + escalaDe(tipo)) * NV_EXP_ASPECTO;
            const char *uP = artehero_logo_sessao_larg(cItem, wFim * 0.65f);
            if (uP) (void)tex_obter_logo_larg(uP, wFim * 0.65f);
          }
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
            TxtLinha ep=txt_linha_corta(TXT_MINI,(cItem->temporada||!strcmp(cItem->tipo,"series"))?cItem->direcao:i18n("Filme"),181,185,196,255,tw);
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
          caminho = arte_por_identidade(idxCat,
                                        (tipo == FILEIRA_CONTINUE || tipo == FILEIRA_RETORNO) ? 3
                                        : (abre > 0.5f || deitado));

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
          // ARTE CHEGANDO: so esvanece quem foi visto esperando (revela.h).
          float aArte = revela_arte(&revArte[r][c], t != 0, agora);
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
          const float raio = raioDe(w, h);
          CartaoFoco ck = { r, c, tipo, cItem, caminho, t, aArte, deitado, rotuloFora, abre, esc, f,
                            px, py, w, h, (!focoHero && focus_indice(&foco, r, c)) ? varreFoco : 0.0f };
          pintarCartao(&ck, raio);
          // MARCADOR "PRIMEIRA FILEIRA PINTADA" (#7 do handoff de desempenho,
          // 05/10). "primeira fileira da rede pronta" e "catalogo da rede
          // publicado" (descoberta.c) medem quando o DADO esta pronto; nada
          // media quando ele chegou aos PIXELS — sem este marco nao se mede
          // publicar -> pintar, a janela que a pessoa sente no arranque ("hero
          // sem arte no prazo" e a maior dor de campo medida no D1). Aqui, no
          // primeiro card NORMAL de fileira de verdade que foi PINTADO (o
          // pintarCartao acima): uma unica vez por sessao, no primeiro card —
          // pintar uma fileira inteira so para marcar atrasaria o numero que
          // o marcador existe para medir. Hero/social/top10 nao contam: nao
          // representam a fileira comum da home.
          if (tPrimeiraFileiraPintada < 0 && tipo != FILEIRA_SOCIAL &&
              tipo != FILEIRA_TOP10 && !focoHero) {
            tPrimeiraFileiraPintada = (int)agora;
            marco("primeira fileira pintada");
          }
          if (!focoHero && focus_indice(&foco, r, c)) { cartaoFoco = ck; cartaoFocoRaio = raio; temCartaoFoco = 1;
            snprintf(cartaoFocoArte, sizeof cartaoFocoArte, "%s", caminho ? caminho : "");
            cartaoFoco.caminho = caminho ? cartaoFocoArte : NULL; }


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
    if (fora > 0 && nFileiras > 0 && !fileirasOcultas) {
      char aviso[160];
      snprintf(aviso, sizeof aviso,
               fora == 1 ? i18n("Cabe %d fileira a mais aqui")
                         : i18n("Cabem %d fileiras a mais aqui"), fora);
      TxtLinha l = txt_linha(TXT_CAPTION, aviso, 196, 199, 208, 255);
      txt_desenhar_alpha(l, ajustes_conteudo_x(), y, 0.92f * (1.0f - cinema));
      { TxtLinha c = txt_linha(TXT_CAPTION2,
                               "Ajustes  ·  Fileiras da Home  ·  Limite de fileiras",
                               150, 152, 160, 255);
        txt_desenhar_alpha(c, ajustes_conteudo_x(), y + l.h + 6.0f, 0.92f * (1.0f - cinema)); }
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
// Rastro de desempenho (main.c, [qd]): onde o foco esta neste quadro.
const char *home_rastro_foco(void) {
  static char b[160];
  if (focoHero) snprintf(b, sizeof b, "hero");
  else if (foco.fileira >= 0 && foco.fileira < nFileiras)
    snprintf(b, sizeof b, "f%d/%d tipo=%d col=%d \"%.40s\"", foco.fileira, nFileiras,
             (int)fileiras[foco.fileira].tipo, foco.coluna, fileiras[foco.fileira].titulo);
  else snprintf(b, sizeof b, "f%d", foco.fileira);
  return b;
}
int home_retorno_vale(int indice, double posSeg, double durSeg) {
  double p;
  if (indice < 0 || durSeg <= 1.0) return 0;
  p = posSeg / durSeg;
  return p >= 0.01 && p * 100.0 < ajustes_cw_concluido();
}
void home_registrar_retorno(int indice, double posSeg, double durSeg) {
  int novo = home_retorno_vale(indice, posSeg, durSeg) ? indice : -1;
  if (novo != retomarIndice) { retomarIndice = novo; retomarRev++; }
  else if (novo >= 0) retomarRev++; // atualiza barra/tempo da mesma sessao
  const CatItem *c = novo >= 0 ? cat_item_exato(novo) : NULL;
  snprintf(retomarId, sizeof retomarId, "%s", c ? c->imdb : "");
}
const char *home_retomar_imdb(void) { return retomarId; }
// DISPENSAR o cartao "Retomar agora" (menu do cartao). Esquece o titulo: a
// faixa some e NAO volta numa republicacao (retomarId vazio), so quando uma
// NOVA saida do player a recolocar (home_registrar_retorno). Progresso e sync
// nao sao tocados. Quem segura o titulo fora de "Continuar assistindo"
// (cwretido) solta no passo seguinte do app (cwRetidoSincronizar) e o poe na
// FRENTE da fileira, sem refazer o resto.
void home_retomar_dispensar(void) {
  if (!retomarId[0] && retomarIndice < 0) return;
  retomarId[0] = 0;
  retomarIndice = -1;
  retomarRev++;
  printf("[home] cartao Retomar agora dispensado\n");
  fflush(stdout);
}
// Troca de conta/perfil: o cartao era da pessoa anterior. Solta o hold em
// silencio (sem por o titulo na frente do Continuar da pessoa nova).
void home_retomar_esquecer(void) {
  if (!retomarId[0] && retomarIndice < 0) return;
  retomarId[0] = 0;
  retomarIndice = -1;
  retomarRev++;
  cw_retido_definir("");
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
// "Ver detalhes" do menu do cartao de retomada (#350): OK ali toca, e a pagina
// do titulo ficava sem porta. O foco nao mudou, entao e o mesmo pedido que o OK
// faria com "OK no card = abre a pagina": app.c o atende por abrirTitulo.
void home_pedir_abrir(void) { pedidoAbrir = 1; }
// O card focado e de retomada e o OK nele TOCA (Ajustes > Continuar)?
int home_foco_retomada(void) {
  const Fileira *s;
  if (focoHero || foco.fileira < 0 || foco.fileira >= nFileiras) return 0;
  s = &fileiras[foco.fileira];
  return strcmp(s->chave, "upcoming_section") &&
         (s->tipo == FILEIRA_CONTINUE || s->tipo == FILEIRA_RETORNO ||
          !strcmp(s->chave, "continue_watching"));
}
int home_pediu_tocar(void) { int v = pedidoTocar; pedidoTocar = 0; return v; }

// Consome o pedido de abrir o menu lateral: quem le, zera.
int home_pediu_menu(void) { int v = pedidoMenu; pedidoMenu = 0; return v; }

// CARROSSEL DA DINAMICA (detail.c): os titulos da fileira em foco, na ordem
// da fileira, e a posicao do card focado entre eles. So nas fileiras de
// TITULO: destaque, pilha do Top 10, colecoes (atalhos de pasta) e canais
// devolvem 0 e a pagina abre como nos outros layouts. O "Ver tudo" nao e
// titulo e nao entra.
int home_fileira_titulos(int *out, int max, int *pos) {
  const Fileira *fl;
  int c, n = 0;
  if (pos) *pos = -1;
  if (layoutHome() != HOME_LAYOUT_DINAMICA || focoHero) return 0;
  if (foco.fileira < 0 || foco.fileira >= nFileiras) return 0;
  fl = &fileiras[foco.fileira];
  if (fl->tipo == FILEIRA_CATALOGOS || fileiraEhCanal(fl)) return 0;
  if (fl->tipo == FILEIRA_TOP10 && fl->stackN) return 0;
  // CONTINUAR ASSISTINDO NAO ABRE O CARROSSEL (pedido do dono, 01/10): OK no
  // CW faz o que fazia antes dele — toca direto ou abre o detalhe direto,
  // conforme Ajustes > Continuar. Vale para a "Retomar agora" e para o CW no
  // estilo poster (FILEIRA_NORMAL, identificado pela chave), e para os
  // "Proximos episodios", que tambem nao sao fileira de catalogo.
  if (fl->tipo == FILEIRA_CONTINUE || fl->tipo == FILEIRA_RETORNO ||
      !strcmp(fl->chave, "continue_watching") || !strcmp(fl->chave, "upcoming_section"))
    return 0;
  if (foco.coluna < 0 || foco.coluna >= fl->n) return 0;
  for (c = 0; c < fl->n && n < max; c++) {
    int i = fileiraItemIndice(fl, c);
    if (i < 0 || !cat_item(i)) continue;
    if (c == foco.coluna && pos) *pos = n;
    out[n++] = i;
  }
  if (pos && *pos < 0) return 0;
  return n;
}

// Leva o foco da fileira para o titulo `indice` do catalogo (o que o
// carrossel deixou em cena), para o Voltar cair nele. A rolagem horizontal
// segue o foco sozinha no proximo home_atualizar.
void home_focar_titulo(int indice) {
  const Fileira *fl;
  int c;
  if (focoHero || foco.fileira < 0 || foco.fileira >= nFileiras) return;
  fl = &fileiras[foco.fileira];
  for (c = 0; c < fl->n; c++)
    if (fileiraItemIndice(fl, c) == indice) {
      foco.coluna = c;
      foco.colunaLembrada[foco.fileira] = c;
      // A FILEIRA JA FICA NO LUGAR FINAL: o cartao do carrossel encolhe ate o
      // cartaz, e o detalhe le onde ele esta num quadro so (ver detail.c,
      // carEsperaRect) — com a rolagem ainda andando o alvo fugiria.
      scrollX[foco.fileira] = alvoScrollFil(foco.fileira, c, scrollX[foco.fileira], 0.0f);
      velX[foco.fileira] = 0.0f;
      return;
    }
}
