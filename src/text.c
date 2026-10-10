#ifdef NV_ANDROID
#include <unistd.h>   // access (fontes de /system/fonts)
#endif
#include "text.h"
#include "dobra.h"
#include "idioma.h"
#include "ajustes.h"
#include "gfx.h"
#include "layout.h"
#include "marco.h"
#include "bidi.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Quantas linhas de texto ficam guardadas ao mesmo tempo.
//
// 256 chegou ao limite quando a pagina de titulo passou a ter a secao de
// comentarios: cada cartao sao ~7 linhas (nome, cinco de texto e o rodape) e
// eles convivem com episodios, elenco, abas, sinopse e as duas linhas de meta.
// Passando do teto, o LRU despeja linhas que a PROPRIA TELA ainda vai desenhar
// no mesmo quadro; elas voltam pela fila de TXT_POR_QUADRO, duas por vez, e
// sao despejadas de novo. E esse laco que aparece como o texto "piscando".
//
// 512 nao muda o custo de busca (a sondagem parte do hash e para no primeiro
// buraco) nem o de rasterizacao. Custa memoria de textura para linhas que nao
// estao na tela — o preco de nao rerasterizar as que estao.
#define MAX_LINHAS 512

typedef struct {
  char chave[288];
  unsigned long hash;   // FNV-1a da chave, para pular o strcmp
  TxtLinha linha;
  unsigned long uso;
  unsigned long quadroUso;
  int ocupado;
} Entrada;

// Fator entre o pixel do BUFFER e o pixel de layout. As fontes sao abertas em
// `corpo * escala` e a linha cacheada guarda a medida DIVIDIDA por ele, entao
// todo o resto do app continua medindo em 1920x1080 enquanto o glifo tem a
// resolucao real da tela.
//
// Sem isto o texto era rasterizado a 1080p e ampliado ao dobro na TV 4K — que
// e exatamente o borrao que o dono viu comparando com o app web, onde o
// navegador rasteriza no devicePixelRatio.
static float escalaTxt = 1.0f;
// CAMADA AMPLIADA (gfx.h, "Tamanho da interface"). Dentro de uma camada com
// gfx_escala() != 1 o texto e rasterizado em `corpo * escalaTxt * s` e a
// linha guarda a medida dividida por esse fator: o layout da camada mede em
// unidades virtuais e o glifo sai com a resolucao do tamanho final, nunca uma
// textura de 1x esticada. A camada 0 e o caminho de sempre, intocado; a 1 tem
// as proprias fontes, abertas sob demanda sobre os mesmos bytes, e as proprias
// linhas no cache (a chave leva o fator).
//
// VARIAS CAMADAS AO MESMO TEMPO (04/10). Eram duas (1x e "a ampliada"), e a
// ampliada fechava tudo quando o fator mudava. Mas um mesmo quadro pode
// desenhar em DOIS fatores diferentes de 1: Ajustes a 80% e, por cima, a
// pilula do menu no fator fixo dele (0,9, layout Dinamica). Cada quadro
// trocava 0,8 -> 0,9 -> 0,8, fechava as fontes e despejava as linhas da
// outra: 62 linhas rasterizadas de novo POR QUADRO (tests/ajustes_shot.c,
// NUVIO_SHOT_TXT). No Mac cabe no orcamento; na TV o orcamento por quadro
// (TXT_MS_QUADRO) acaba antes, e o que vem depois na ordem do desenho (os
// chips do resumo, as dicas, o rotulo da pilula) ficava sem texto para
// sempre. Agora cada fator tem a sua casa (TXT_NCAM - 1 fatores vivos); a
// casa menos usada so e reaproveitada quando aparece um fator novo.
#define TXT_NCAM 4
static int camada;
static char arabeBoldCaminho[600];   // face Bold real do Noto Naskh (#335), vazio ate txt_iniciar
static int arabeCorpo;
static void arabeBoldFechar(void);
static float escCamSlot[TXT_NCAM];   // o fator de cada casa (0 = livre; a 0 e sempre 1x)
static unsigned long usoCam[TXT_NCAM];
static unsigned long relogioCam;
#define escCam (escCamSlot[camada])
#define ESC_T (camada ? escalaTxt * escCam : escalaTxt)
static TTF_Font *fontesCam[TXT_NCAM][TXT_FAMILIA_N][TXT_NFONTES];
#define fontes (fontesCam[camada])
static TxtFamilia fonteInterface = TXT_FAMILIA_INTER;
static TxtFamilia fonteInterfaceFallback = TXT_FAMILIA_INTER;

// Os arquivos TTF dos tres pesos, LIDOS UMA VEZ e mantidos vivos enquanto o app
// vive: as faces do FreeType leem deles sob demanda, entao liberar aqui e
// leitura de memoria liberada no primeiro glifo novo. Sao ~900 KB no total.
// `donoPeso` marca quais ponteiros sao proprios: pesos que apontam para o mesmo
// arquivo compartilham o buffer e so um deles libera.
static unsigned char *bytesPeso[TXT_FAMILIA_N][3];
static size_t         tamPeso[TXT_FAMILIA_N][3];
static int            donoPeso[TXT_FAMILIA_N][3];
static int            familiaCarregada[TXT_FAMILIA_N];
static int            familiaTentada[TXT_FAMILIA_N];
static char           caminhoPeso[TXT_FAMILIA_N][3][512];
// O RWops de cada estilo. Guardado porque abrimos com freesrc=0 (o buffer e
// compartilhado, a fonte nao pode fecha-lo) e alguem tem de fechar em
// txt_encerrar.
static SDL_RWops     *rwFonteCam[TXT_NCAM][TXT_FAMILIA_N][TXT_NFONTES];
#define rwFonte (rwFonteCam[camada])
// A JetBrains Mono do registro (PESO_MONO_R/B): dois arquivos lidos uma vez, na
// primeira familia que abrir um estilo TXT_MONO*, e vivos ate txt_encerrar.
// O TERCEIRO e o numeral fino do relogio da tela de descanso (PESO_FINO,
// fonts/Montserrat-ExtraLight-Relogio.ttf, so digitos e ':'), pelo mesmo
// caminho: um arquivo, em toda familia.
static char           caminhoMono[3][512];
static unsigned char *bytesMono[3];
static size_t         tamMono[3];
static int            monoTentada[3];

// Le o arquivo inteiro para um buffer novo. NULL se nao abrir.
static unsigned char *lerTudo(const char *caminho, size_t *tam) {
  FILE *f = fopen(caminho, "rb");
  unsigned char *b;
  long n;
  *tam = 0;
  if (!f) return NULL;
  if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
  n = ftell(f);
  if (n <= 0) { fclose(f); return NULL; }
  rewind(f);
  b = malloc((size_t)n);
  if (!b) { fclose(f); return NULL; }
  if (fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
  fclose(f);
  *tam = (size_t)n;
  return b;
}
// Pesos de fontes legadas que não têm três faces reais usam síntese. Fontes
// novas e Inter carregam as faces reais, uma família por vez e sob demanda.
#define TXT_LEG_N (TXT_LEG_250 - TXT_LEG_50 + 1)
static TTF_Font *fontesLegendaLGCam[TXT_NCAM][TXT_LEG_N];
#define fontesLegendaLG (fontesLegendaLGCam[camada])
static int avisoFallback[TXT_FAMILIA_N];
static unsigned char tentouLegendaLGCam[TXT_NCAM][TXT_LEG_N];
#define tentouLegendaLG (tentouLegendaLGCam[camada])
const char *const TXT_FAMILIAS_PT[TXT_FAMILIA_N] = {
  "Inter", "LG Display", "Droid Sans", "Montserrat", "Roboto",
  "Atkinson Hyperlegible Next"
};

static Entrada cache[MAX_LINHAS];

static void limparCacheTexto(void) {
  for (int i = 0; i < MAX_LINHAS; i++) {
    if (cache[i].ocupado && cache[i].linha.tex) {
      gfx_tex_esquecer(cache[i].linha.tex);
      glDeleteTextures(1, &cache[i].linha.tex);
    }
    memset(&cache[i], 0, sizeof cache[i]);
  }
}

void txt_definir_fonte_interface(TxtFamilia familia) {
  if (familia < TXT_FAMILIA_INTER || familia >= TXT_FAMILIA_N ||
      familia == fonteInterface) return;
  fonteInterface = familia;
  limparCacheTexto();
}

TxtFamilia txt_fonte_interface(void) { return fonteInterface; }

// Quantas linhas NOVAS podem ser rasterizadas por quadro.
//
// Medido no aparelho: entrar na pagina de detalhe rasteriza 12 linhas de uma
// vez e custa 12 ms — um quadro inteiro, e o tranco aparece exatamente na
// transicao que se quer suave. Rasterizar em conta-gotas faz o texto assentar
// um ou dois quadros depois, o que ninguem ve; o tranco, todo mundo ve.
// 2 e nao 4: com 4 o pior quadro media 6 ms so de texto, e o objetivo aqui e
// que NENHUMA parte sozinha coma mais que um terco do quadro.
// QUATRO, e o numero e MEDIDO e nao escolhido.
//
// Eram dois, e o teto existe por um motivo real: rasterizar custa, e uma tela
// nova pede dezenas de linhas ineditas de uma vez. Mas com dois, ABRIR UM MODAL
// deixava a maior parte do texto em branco por varios quadros — as linhas vao
// entrando de duas em duas e, num aparelho mais lento, isso se le como "o texto
// some". Foi o relato do dono no Tizen, e a mesma coisa que a foto do modal de
// opcoes mostrou com uma linha vazia.
//
// MEDIDO na LG OLED65C9 antes de mexer: "texto 2.5ms em 2 linhas", pior quadro
// 23,6 ms, 0 janks. Quatro linhas custam ~5 ms no quadro em que a tela abre, e
// SO nesse — em regime a telemetria diz "texto 0.0ms em 0 linhas". O contador
// de janks e quem decide se este numero pode subir mais; se ele sair de zero
// abrindo tela, o certo e voltar a tres, nao ignorar.
#define TXT_POR_QUADRO 4
static int rastNesteQuadro;
static unsigned long quadroTxt = 1;
extern double txt_ms;
static double msIniQuadro;

// TEXTO DE UMA VEZ (#172: "o texto aparece aos poucos, nao e fluido"; dono,
// 29/09/2026: "tem como carregar de uma vez?"). Com um teto fixo de 4 linhas por
// quadro, uma tela nova mostrava o texto entrando em degraus por varios quadros.
// Agora o teto de 4 e o PISO: depois dele o rasterizador continua enquanto o
// quadro tiver gasto menos de TXT_MS_QUADRO em texto (e ate TXT_MAX_QUADRO
// linhas). O preco e UM quadro mais longo no instante em que a tela abre —
// coberto pela propria animacao de entrada — em troca de o texto inteiro
// aparecer junto. Em regime nada muda: sem linha nova, nada e rasterizado.
#define TXT_MS_QUADRO  24.0
#define TXT_MAX_QUADRO 64

void txt_novo_quadro(void) {
  rastNesteQuadro = 0; quadroTxt++;
  msIniQuadro = txt_ms;
}
static unsigned long relogio = 1;
int    txt_rasterizadas = 0;
// Quantas linhas foram DESPEJADAS para dar lugar a outras. Zero e o estado
// saudavel. Se voltar a subir com a tela parada, a tabela encheu de novo e o
// texto vai piscar — e melhor ler isso num contador do que descobrir pela
// reclamacao de quem esta olhando a tela.
int    txt_despejos = 0;
double txt_ms = 0.0;
// Linhas RECUSADAS por falta de orcamento no quadro (voltam vazias e entram
// num quadro seguinte). Quem quer mostrar uma tela inteira de uma vez le a
// diferenca antes/depois de desenhar: zero = tudo o que foi pedido existe.
int    txt_pendentes = 0;

// Peso por estilo. Cada peso e um ARQUIVO de verdade da Inter Display
// (Regular 400, Medium 500, Bold 700) — nao ha passada repetida nem
// deslocamento sub-pixel para simular peso. O negrito sintetico
// (TTF_SetFontStyle) so entra nas familias de RESERVA (LG, Droid), que nao tem
// arquivo Bold proprio; ver o `if (c > 0 && ...)` em txt_iniciar. Isso importa
// porque negrito sintetico engorda os tracos sem redesenhar nada, e ao lado de
// um Bold de verdade a diferenca aparece logo nos titulos grandes.
//
// COMO O PESO 600 DO WEB E RESOLVIDO, e por que nao ha um valor unico.
// A Inter embarcada nao tem SemiBold, e acrescentar o arquivo esta fora de
// questao (o ipk ja tem 166 MB). Sobra escolher entre Medium (erra 100 para
// baixo) e Bold (erra 100 para cima), e a escolha e OPTICA, nao aritmetica:
//
//   texto CLARO sobre fundo escuro parece mais fino do que e  -> Bold
//   texto ESCURO sobre pilula clara parece mais grosso do que e -> Medium
//
// Por isso `.home-row-title` (600, branco no escuro) fica em Bold e
// `.series-primary-btn` (600, preto na pilula branca de 96px) fica em Medium.
// Sao dois destinos diferentes para o mesmo 600 de propósito, e nao um
// descuido — sem a regra escrita aqui, a proxima pessoa "conserta" um dos dois
// e desalinha a tela.
enum { PESO_REGULAR, PESO_MEDIUM, PESO_BOLD,
       // A MONO DO REGISTRO (JetBrains Mono, OFL, fonts/JetBrainsMonoNL-*.ttf, a variante SEM ligaduras: o "->" do log tem de sair como dois caracteres).
       // Nao e uma familia escolhivel: e a mesma em toda familia, porque so as
       // linhas do log a usam e uma coluna de log so alinha em monoespacada.
       // Sem o arquivo, o estilo cai no Regular/Bold da familia (ver
       // carregarFamilia) — o texto continua legivel, so desalinha.
       PESO_MONO_R, PESO_MONO_B,
       // Numeral fino do relogio da tela de descanso (descanso.c). Sem o
       // arquivo cai no Regular da familia.
       PESO_FINO };
static const struct { int corpo, peso; } ESTILOS[TXT_NFONTES] = {
  { NV_FT_TITULO1,  PESO_BOLD   },   // titulo do filme na tela de detalhe
  // ERA PESO_REGULAR, pelo cabecalho espacado da pagina de titulo do app da
  // Apple. Esse cabecalho NAO EXISTE MAIS: a tela de detalhe do web e um
  // documento rolavel sem cabecalho fixo, e o do app da Apple saiu do port.
  // Hoje TXT_TITULO2 e usado so por "Biblioteca" (.library-page-title 56/600) e
  // pelos titulos de estado vazio da busca e da biblioteca — os TRES em peso
  // 600 no web. Regular errava 200 para baixo em todos.
  { NV_FT_TITULO2,  PESO_BOLD    },  // .library-page-title e estados vazios
  { NV_FT_TITULO3,  PESO_BOLD   },   // nome dentro do card destaque
  { NV_FT_HEADLINE, PESO_MEDIUM },   // cabecalho de fileira
  { NV_FT_BODY,     PESO_MEDIUM },   // rotulo de botao, titulo de episodio
  { NV_FT_CALLOUT,  PESO_MEDIUM },   // linha de genero
  { NV_FT_CAPTION,  PESO_REGULAR },  // sinopse, texto corrido
  { NV_FT_CAPTION2, PESO_REGULAR },  // creditos, datas, rotulos
  // Abaixo do minimo de 23px que o tvOS estabelece para TEXTO — mas isto nao e
  // texto para ler, e um selo de classificacao indicativa, que no aparelho tem
  // mesmo o tamanho de um icone.
  { NV_FT_MINI,     PESO_BOLD    },  // badge de classificacao
  // Player, do app web: titulo em 700 e o corpo em 400 (.player-title tem
  // font-weight 700; .player-subtitle e .player-time-label nao declaram peso e
  // herdam o normal).
  { NV_FT_PLR_TITULO, PESO_BOLD    },
  { NV_FT_PLR_CORPO,  PESO_REGULAR },
  { NV_FT_ROW_TITULO, PESO_BOLD    },  // .home-row-title (600)
  // Linha secundaria do hero em tela cheia. 600 sobre fundo escuro: Bold,
  // pela mesma regra optica ja escrita acima.
  { NV_FT_HERO_SEC,   PESO_BOLD    },
  // Tela de detalhe, medidos no app web. O peso 600 do rotulo do botao nao
  // existe no pacote da Inter embarcada (so Regular, Medium e Bold): fica em
  // MEDIUM, que erra 100 para baixo, e nao em Bold, que erraria 100 para cima e
  // engorda visivelmente numa pilula clara de 96px de altura.
  { NV_FT_DET_BOTAO, PESO_MEDIUM  },
  { NV_FT_DET_META,  PESO_REGULAR },
  { NV_FT_DET_SIN,   PESO_REGULAR },
  { NV_FT_DET_META2, PESO_REGULAR },
  { NV_FT_HERO_META, PESO_MEDIUM  },   // .home-modern-hero-meta-line (21/500)
  { NV_FT_HERO_SIN,  PESO_REGULAR },   // .home-hero-description (22/400)
  { NV_FT_PG_RELOGIO, PESO_MEDIUM  },  // .player-clock (26/600)
  { NV_FT_PG_FIM,     PESO_REGULAR },  // .player-ends-at (20/400)
  { NV_FT_PG_ROTULO,  PESO_MEDIUM  },  // .player-parental-label (22/600)
  { NV_FT_PG_GRAV,    PESO_REGULAR },  // .player-parental-severity (22/400)
  // ESCALA DOS PAINEIS (30/09, revisao de proporcao): titulo 34, item 26,
  // apoio 22 (NV_FT_PG_FIM). Eram 36 Regular / 24 Bold / 20: o item em Bold
  // pesava mais que o titulo, e o apoio ficava abaixo do piso de leitura.
  { 34, PESO_MEDIUM },              // cabecalhos dos paineis (Fontes, Legendas, Episodios)
  { 26, PESO_MEDIUM },              // episodio/fonte/faixa dentro da lista
  { 28, PESO_MEDIUM },              // titulo no card Continuar assistindo
  { 23, PESO_REGULAR },             // temporada e nome do episodio
  { 20, PESO_MEDIUM },              // tempo restante no badge do card
  { 110, PESO_BOLD },               // posição real no ranking
  { 20, PESO_REGULAR }, { 24, PESO_REGULAR }, { 28, PESO_REGULAR },
  { 32, PESO_REGULAR }, { 36, PESO_REGULAR }, { 40, PESO_REGULAR },
  { 44, PESO_REGULAR }, { 48, PESO_REGULAR }, { 52, PESO_REGULAR },
  { 56, PESO_REGULAR }, { 60, PESO_REGULAR }, { 64, PESO_REGULAR },
  { 68, PESO_REGULAR }, { 72, PESO_REGULAR }, { 76, PESO_REGULAR },
  { 80, PESO_REGULAR },
  { 84, PESO_REGULAR }, { 88, PESO_REGULAR }, { 92, PESO_REGULAR },
  { 96, PESO_REGULAR }, { 100, PESO_REGULAR },   // 210..250% (#335)
  { NV_TOP10_NUM_CORPO, PESO_BOLD },   // numeral do Top 10 da Dinamica
  // Escala das ilhas (text.h). 600 e 800 claros sobre o vidro escuro vao para
  // Bold pela regra optica escrita acima; 400 fica Regular.
  { 40, PESO_BOLD    },   // TXT_ILHA_TITULO
  { 22, PESO_BOLD    },   // TXT_ILHA_SECAO
  { 24, PESO_BOLD    },   // TXT_ILHA_NOME
  { 24, PESO_REGULAR },   // TXT_ILHA_CORPO
  { 19, PESO_BOLD    },   // TXT_ILHA_SEG
  { 19, PESO_REGULAR },   // TXT_ILHA_SUB
  { 16, PESO_REGULAR },   // TXT_ILHA_NUM
  { 15, PESO_REGULAR },   // TXT_ILHA_HORA
  { 20, PESO_BOLD    },   // TXT_ILHA_INICIAL
  { 22, PESO_BOLD    },   // TXT_ILHA_ITEM: nome numa linha de resultado (600)
  { 19, PESO_REGULAR },   // TXT_ILHA_META: ano · duracao · tipo do melhor resultado
  { 17, PESO_REGULAR },   // TXT_ILHA_GENERO
  { 16, PESO_REGULAR },   // TXT_ILHA_APOIO: meta das linhas, dicas do rodape
  { 36, PESO_BOLD    },   // TXT_ILHA_PERGUNTA: a pergunta da confirmacao
  { 20, PESO_REGULAR },   // TXT_ILHA_TEXTO: o texto corrido da confirmacao
  { NV_FT_BODY, PESO_BOLD },           // destaque na frase da ilha (TXT_ILHA_FORTE)
  // Ajustes no Glass UI (text.h). 600/800 claros sobre o miolo escuro: Bold.
  { 23, PESO_MEDIUM  },   // TXT_AJ_ITEM: categoria no indice (mockup 21/500)
  { 25, PESO_MEDIUM  },   // TXT_AJ_ROTULO: nome na linha (23/500)
  { 22, PESO_MEDIUM  },   // TXT_AJ_VALOR: valor na linha (20/500)
  { 18, PESO_BOLD    },   // TXT_AJ_CHIP: "Ligado" / "Desligado" (17/600)
  { 33, PESO_BOLD    },   // TXT_AJ_INSP: titulo do inspetor (32/700)
  { 15, PESO_BOLD    },   // TXT_AJ_KBD: tecla das dicas (14/700)
  { 18, PESO_REGULAR },   // TXT_AJ_ESTADO: meta e texto de apoio (17/400)
  { 14, PESO_BOLD    },   // TXT_AJ_CAPS13: marca pequena em caixa alta (13/700)
  {  9, PESO_REGULAR },   // TXT_AJ_MINI9: texto da home em miniatura
  {  9, PESO_BOLD    },   // TXT_AJ_MINI9B
  { 19, PESO_MEDIUM  },   // TXT_AJ_VALOR18: celulas da folha de fileiras (18/500)
  { 58, PESO_BOLD    },   // TXT_AJ_NUM58: numero do painel de memoria
  { 64, PESO_BOLD    },   // TXT_AJ_NUM64: codigo do vinculo
  { 110, PESO_BOLD   },   // TXT_AJ_NUM110: numeros dos editores e do teste
  { 29, PESO_BOLD    },   // TXT_AJ_TIT28: titulo do painel lateral (28/700)
  { 13, PESO_BOLD    },   // TXT_AJ_MINI12 (12/700)
  { 14, PESO_REGULAR },   // TXT_AJ_MINI13: eixos dos graficos (13/400)
  { 15, PESO_REGULAR },   // TXT_AJ_MINI14 (14/400)
  { 17, PESO_BOLD    },   // TXT_AJ_16B: rotulo das miniaturas (16/600)
  { 20, PESO_REGULAR },   // TXT_AJ_SUB: subtitulo e ajuda (19/400)
  { 20, PESO_BOLD    },   // TXT_AJ_SEG: segmentado e chip (19/600)
  { 19, PESO_REGULAR },   // TXT_AJ_18: estado curto da acao (18/400)
  { 23, PESO_BOLD    },   // TXT_AJ_SECAO: cabecalho de grupo (22/800)
  { 22, PESO_REGULAR },   // TXT_AJ_TEXTO: texto corrido dos modais (20/400)
  { 25, PESO_BOLD    },   // TXT_AJ_NOME: botao e nome forte (24/600)
  // Escala do player no Glass UI (text.h, TXT_G*), na mesma ordem do enum.
  { 14, PESO_BOLD    },   // TXT_G14B
  { 16, PESO_BOLD    },   // TXT_G16B
  { 18, PESO_REGULAR },   // TXT_G18R
  { 18, PESO_MEDIUM  },   // TXT_G18M
  { 19, PESO_MEDIUM  },   // TXT_G19M
  { 20, PESO_BOLD    },   // TXT_G20B
  { 20, PESO_MEDIUM  },   // TXT_G20M
  { 21, PESO_BOLD    },   // TXT_G21B
  { 22, PESO_MEDIUM  },   // TXT_G22M
  { 23, PESO_BOLD    },   // TXT_G23B
  { 26, PESO_BOLD    },   // TXT_G26B
  { 28, PESO_BOLD    },   // TXT_G28B
  { 30, PESO_BOLD    },   // TXT_G30B
  { 30, PESO_MEDIUM  },   // TXT_G30M
  { 52, PESO_BOLD    },   // TXT_G52B
  // Registro do app (text.h, TXT_MONO* e TXT_LOG*), na mesma ordem do enum.
  { 18, PESO_MONO_R  },   // TXT_MONO18
  { 18, PESO_MONO_B  },   // TXT_MONO18B
  { 16, PESO_MONO_R  },   // TXT_MONO16
  { 15, PESO_MONO_R  },   // TXT_MONO15
  { 14, PESO_MONO_R  },   // TXT_MONO14
  { 13, PESO_MONO_R  },   // TXT_MONO13
  { 19, PESO_MONO_R  },   // TXT_MONO19
  { 44, PESO_BOLD    },   // TXT_LOG_N44
  { 35, PESO_BOLD    },   // TXT_LOG_T34 (34 no CSS; a InterDisplay e mais estreita)
  { 88, PESO_BOLD    },   // TXT_LOG_COD
  { 19, PESO_BOLD    },   // TXT_LOG_19B
  { 18, PESO_BOLD    },   // TXT_LOG_18B
  { 14, PESO_MONO_B  },   // TXT_MONO14B
  { 31, PESO_BOLD    },   // TXT_LOG_T31
  { 51, PESO_BOLD    },   // TXT_NOV_TITULO (mockup 50/700)
  { 28, PESO_REGULAR },   // TXT_G28R
  { 33, PESO_MEDIUM  },   // TXT_V2_MENU
  { 33, PESO_BOLD    },   // TXT_V2_MENU_B
  { 26, PESO_REGULAR },   // TXT_V2_26
  { 54, PESO_BOLD    },   // TXT_V2_TIT
  { 19, PESO_BOLD    },   // TXT_V2_KICK
  { 23, PESO_BOLD    },   // TXT_V2_CHIP
  { 29, PESO_BOLD    },   // TXT_V2_GRUPO
  { 24, PESO_REGULAR },   // TXT_V2_24
  { 34, PESO_MEDIUM  },   // TXT_V2_ROT
  { 28, PESO_REGULAR },   // TXT_V2_28
  { 25, PESO_BOLD    },   // TXT_V2_SEG
  { 42, PESO_BOLD    },   // TXT_V2_INSP
  { 21, PESO_BOLD    },   // TXT_V2_KBD20
  { 19, PESO_BOLD    },   // TXT_V2_KBD18
  { 53, PESO_BOLD    },   // TXT_V2_NUM
  { 172, PESO_BOLD    },   // TXT_V2_NUM150
  { 36, PESO_BOLD    },   // TXT_V2_36B
  { 18, PESO_REGULAR },   // TXT_V2_18
  { 26, PESO_MEDIUM  },   // TXT_V2_LN
  { 26, PESO_BOLD    },   // TXT_V2_LN_B
  { 48, PESO_BOLD    },   // TXT_V2_A3TIT
  { 28, PESO_BOLD    },   // TXT_ILHA_NOME_L (24 + 17 %)
  { 22, PESO_REGULAR },   // TXT_ILHA_SUB_L (19 + 16 %)
  { 17, PESO_REGULAR },   // TXT_ILHA_HORA_L (15 + 13 %)
  { 124, PESO_BOLD   },   // TXT_W20_HERO (mockup 120/800)
  { 24, PESO_BOLD    },   // TXT_W20_24B (24/600)
  { 230, PESO_FINO   },   // TXT_DESC_HORA: numeral do relogio da tela de descanso
  { 41, PESO_BOLD    },   // TXT_V3_NOME
  { 33, PESO_REGULAR },   // TXT_V3_VALOR
  { 29, PESO_BOLD    },   // TXT_V3_CHIP
  { 29, PESO_REGULAR },   // TXT_V3_CHIP_R
  { 36, PESO_BOLD    },   // TXT_V3_DESC
  { 29, PESO_REGULAR },   // TXT_V3_SUB
  { 32, PESO_MEDIUM  },   // TXT_V3_LN
  { 32, PESO_BOLD    },   // TXT_V3_LN_B
  { 32, PESO_BOLD    },   // TXT_V3_GRP
  { 32, PESO_REGULAR },   // TXT_V3_VAL
  { 25, PESO_REGULAR },   // TXT_V3_BEN
  { 26, PESO_BOLD    },   // TXT_V3_PILL
  { 18, PESO_BOLD    },   // TXT_V3_SELO
  { 28, PESO_REGULAR },   // TXT_V3_INSP
  { 28, PESO_BOLD    },   // TXT_V3_INSP_B
};

// RESERVA PARA O QUE A INTER NAO TEM.
//
// A Inter cobre latim, e so. Um titulo japones da filmografia de um ator (a
// tela nova de pessoa mostra varios) saia como fileira de quadradinhos — a
// fonte nao tem o glifo e o SDL_ttf desenha .notdef sem reclamar. A TV traz
// /usr/share/fonts/DroidSansFallback.ttf, que cobre CJK; abrimos ela SOB
// DEMANDA, no mesmo corpo do estilo, e so para as linhas que precisam.
//
// Nao e fallback por glifo (isso exigiria compor a linha caractere a caractere
// e perder o kerning): a linha INTEIRA vai para a reserva quando o primeiro
// caractere fora do ASCII nao existir na fonte principal. Titulo misto
// "Deadpool & ウルヴァリン" sairia todo na reserva, o que e feio mas legivel —
// e o caso raro; o comum e a linha ser toda de uma escrita so.
// Uma reserva POR ESCRITA. A primeira versao tinha um arquivo so, escolhido
// como "o CJK", e o nome de uma atriz iraniana continuava em quadradinhos: a
// DroidSansFallback nao tem arabe. A TV traz arquivo separado para cada
// familia de escrita, e e por isso que a escolha e por faixa de codepoint.
//
// CJK TEM TRES VARIANTES, e a que vale e a do IDIOMA DA INTERFACE: o mesmo hanzi
// (骨, 直, 値) tem forma de japones, de chines simplificado e de tradicional, e a
// fonte de sistema errada nao da tofu, da o desenho de outra lingua. ESC_CJK e o
// padrao (japones primeiro, que era o unico), ESC_CJK_SC e ESC_CJK_TC entram
// quando a interface esta em chines simplificado/tradicional.
//
// CADA ESCRITA TEM UMA LISTA de fontes (RES_CAND), em ordem de preferencia, e a
// linha vai para a primeira que a desenha INTEIRA — ou, se nenhuma desenha, para
// a que deixa menos caracteres de fora. A lista unica de antes escolhia "o
// primeiro arquivo que existe": a LG_Display_JP existe em toda LG e nao tem 303
// dos hanzi simplificados, entao um titulo chines saia em quadradinhos com a
// DroidSansFallback (que os tem) logo ali ao lado.
typedef enum { ESC_CJK, ESC_CJK_SC, ESC_CJK_TC, ESC_ARABE, ESC_TAILANDES, ESC_CIRILICO_ETC, ESC_N } Escrita;
#define RES_CAND 9
static TTF_Font *reservasCam[TXT_NCAM][ESC_N][RES_CAND][TXT_NFONTES];
static unsigned char reservaFalhouCam[TXT_NCAM][ESC_N][RES_CAND][TXT_NFONTES];
#define reservas (reservasCam[camada])
#define reservaFalhou (reservaFalhouCam[camada])
static char caminhoReserva[ESC_N][RES_CAND][512];

// Um codepoint DECORATIVO: simbolo, seta, pictograma, emoji, seletor de
// variacao. Nao pertence a escrita nenhuma e por isso nao pode decidir com que
// fonte a LINHA INTEIRA e desenhada.
//
// POR QUE ISTO EXISTE, com a cobertura da Inter MEDIDA e nao suposta.
//
// fonteDe manda a linha toda para a fonte de reserva quando o primeiro
// caractere fora do ASCII nao existe na Inter. A regra e certa para escrita
// ("Deadpool & ウルヴァリン" sai legivel na DroidSansFallback) e ERRADA para
// simbolo. Os nomes de fonte que os addons devolvem sao cheios de decoracao, e
// um simbolo que a Inter nao tem bastava para a lista de fontes INTEIRA trocar
// para a DroidSansFallback, que e uma fonte CJK: o latim dela e mais pesado e
// com outro hinting, o que na tela le exatamente como o issue #7 — "todo o
// texto aparece em negrito, a informacao fica pixelada". Era tambem trabalho a
// mais (abrir um segundo arquivo de fonte por estilo e rasterizar com uma fonte
// muito maior) na tela que o mesmo relato descreve como lenta.
//
// CONFERIDO nos tres pesos da InterDisplay embarcada, por TTF_GlyphIsProvided:
//   TEM      ← ↑ → ↓  •  …  –  —  " '  ★  ▶  ✓  ·
//   NAO TEM  ⚡  ⚙  ⭐
// Ou seja: quem derrubava a linha eram os que a Inter nao tem — e "⚡" e o mais
// comum nos nomes do Torrentio e do AIOStreams. As setas e o "▶" que a PROPRIA
// interface usa nos seus rotulos sempre passaram, e continuam passando: esta
// funcao decide por glifo disponivel, nao por faixa de codepoint.
//
// Emoji tinham o OUTRO sintoma, nao este: fonteDe devolve a fonte principal
// para codepoint fora do BMP, entao "💾" e "🇧🇷" nunca trocaram a fonte da linha
// — saiam como o retangulo do .notdef. Os dois casos morrem aqui.
static int decorativo(Uint32 cp) {
  if (cp >= 0x2000  && cp <= 0x2BFF)  return 1;  // pontuacao, setas, simbolos, dingbats
  if (cp >= 0x2E00  && cp <= 0x2E7F)  return 1;  // pontuacao suplementar
  if (cp >= 0xFE00  && cp <= 0xFE0F)  return 1;  // seletores de variacao
  if (cp >= 0x1F000 && cp <= 0x1FAFF) return 1;  // emoji e pictogramas
  return 0;
}

// Decodifica um codepoint UTF-8 e diz quantos bytes ele ocupou. Sequencia
// invalida devolve o byte cru com tamanho 1, para nunca travar o laco.
static Uint32 decodifica(const unsigned char *p, int *n) {
  if (*p < 0x80) { *n = 1; return *p; }
  if ((*p & 0xE0) == 0xC0 && p[1]) { *n = 2; return (Uint32)((*p & 0x1F) << 6 | (p[1] & 0x3F)); }
  if ((*p & 0xF0) == 0xE0 && p[1] && p[2]) { *n = 3;
    return (Uint32)((*p & 0x0F) << 12 | (p[1] & 0x3F) << 6 | (p[2] & 0x3F)); }
  if ((*p & 0xF8) == 0xF0 && p[1] && p[2] && p[3]) { *n = 4;
    return (Uint32)((*p & 0x07) << 18 | (p[1] & 0x3F) << 12 |
                    (p[2] & 0x3F) << 6 | (p[3] & 0x3F)); }
  *n = 1; return *p;
}

// Tira da linha os decorativos que a fonte principal NAO TEM, e so eles.
//
// Sem isto sobrariam duas saidas ruins: manter o caractere e desenhar o
// retangulo do .notdef (que e o que acontecia com os emoji, ja hoje), ou trocar
// a fonte da linha inteira (que e o que acontecia com os simbolos do BMP).
// Tirar e a terceira: "⚡ 1080p · 4.2 GB" continua sendo "1080p · 4.2 GB", na
// Inter, sem quadradinho.
//
// O que a fonte TEM fica: en-dash, reticencias, aspas curvas e o proprio "·"
// estao na Inter e passam intactos — a decisao e por glifo disponivel, nao por
// faixa. Emoji fora do BMP saem sempre: TTF_GlyphIsProvided recebe Uint16 e nao
// alcanca esses codepoints, e nenhuma das fontes deste app os tem.
//
// CUSTO: um passe por linha, e ele so acontece quando a linha tem byte fora do
// ASCII — o caminho comum sai na primeira comparacao. A linha rasterizada e
// cacheada por text.c, mas a CHAVE do cache e montada com o texto ja limpo,
// entao este passe roda por quadro; e por isso que a saida rapida importa.
static const char *semDecorativoSemGlifo(TTF_Font *fonte, TTF_Font *alt, TTF_Font *alt2,
                                         const char *s, char *dst, size_t tam) {
  const unsigned char *p = (const unsigned char *)s;
  size_t k = 0;
  int algum = 0;
  for (; *p; p++) if (*p >= 0x80) { algum = 1; break; }
  if (!algum) return s;
  // Linha maior que o buffer: fica como esta. Cortar texto visivel para caber
  // num buffer meu seria trocar um defeito visual por perda de informacao.
  if (strlen(s) + 1 > tam) return s;
  p = (const unsigned char *)s;
  while (*p) {
    int n = 1;
    Uint32 cp = decodifica(p, &n);
    int tirar = 0, falta = 0;
    char comum = 0;
    if (cp >= 0x80)
      falta = cp >= 0x10000 || (!TTF_GlyphIsProvided(fonte, (Uint16)cp) &&
                                !(alt && TTF_GlyphIsProvided(alt, (Uint16)cp)) &&
                                !(alt2 && TTF_GlyphIsProvided(alt2, (Uint16)cp)));
    // LETRA ESTILIZADA SEM GLIFO VIRA A LETRA COMUM (#144): "RᴇLᴇAꜱᴇ" sai
    // "RELEASE", e nao "R▯L▯AS▯" nem a linha inteira na fonte de reserva.
    if (falta) comum = nv_dobra_estilizada(cp);
    if (comum) { dst[k++] = comum; p += n; continue; }
    if (decorativo(cp)) tirar = falta;
    if (!tirar) { int i; for (i = 0; i < n; i++) dst[k++] = (char)p[i]; }
    p += n;
  }
  dst[k] = 0;
  // Espaco duplo e espaco na borda sao restos do que saiu, nao do texto.
  { size_t r = 0, w = 0;
    while (dst[r] == ' ') r++;
    for (; dst[r]; r++) {
      if (dst[r] == ' ' && w && dst[w - 1] == ' ') continue;
      dst[w++] = dst[r];
    }
    while (w && dst[w - 1] == ' ') w--;
    dst[w] = 0; }
  return dst;
}

// Qual reserva cobre este codepoint. As faixas sao as usuais do Unicode; o que
// nao for arabe/hebraico nem CJK cai na terceira, que e a DroidSansFallback (ela
// cobre cirilico, grego, tailandes e mais).
static Escrita escritaDe(Uint32 cp) {
  if (cp >= 0x0590 && cp <= 0x07FF) return ESC_ARABE;       // hebraico + arabe
  if (cp >= 0xFB50 && cp <= 0xFEFF) return ESC_ARABE;       // formas de apresentacao
  if (cp >= 0x0E00 && cp <= 0x0E7F) return ESC_TAILANDES;   // #369: tailandes (tofu antes)
  if (cp >= 0x2E80 && cp <= 0x9FFF) return ESC_CJK;
  if (cp >= 0xAC00 && cp <= 0xD7AF) return ESC_CJK;         // hangul
  if (cp >= 0xF900 && cp <= 0xFAFF) return ESC_CJK;
  if (cp >= 0xFF00 && cp <= 0xFFEF) return ESC_CJK;         // largura total: "（）：～"
  return ESC_CIRILICO_ETC;
}

static int carregarFamilia(TxtFamilia familia);
static void abrirEstiloCamada(TxtFamilia familia, int i);
static void camadaAtualizar(void);

// Quantos caracteres da linha `s` a fonte NAO desenha, e o primeiro deles em
// *primeiro. Conta so o que faz falta: ASCII nunca, os DECORATIVOS que a fonte
// nao tem (setas, ⚡, emoji: semDecorativoSemGlifo os tira da linha) tambem nao,
// e as letras estilizadas que nv_dobra_estilizada troca pela comum tambem nao.
// Fora do BMP nao tratamos (TTF_GlyphIsProvided recebe Uint16), como antes.
static int faltantes(TTF_Font *fonte, const char *s, Uint32 *primeiro) {
  const unsigned char *p = (const unsigned char *)s;
  int n = 0;
  if (primeiro) *primeiro = 0;
  while (*p) {
    int tam = 1;
    Uint32 cp;
    if (*p < 0x80) { p++; continue; }
    cp = decodifica(p, &tam);
    p += tam;
    if (cp >= 0x10000 || decorativo(cp)) continue;
    if (TTF_GlyphIsProvided(fonte, (Uint16)cp)) continue;
    if (nv_dobra_estilizada(cp)) continue;
    if (!n && primeiro) *primeiro = cp;
    n++;
  }
  return n;
}

// A variante CJK que a INTERFACE pede (ver Escrita).
static Escrita variacaoCjk(void) {
  int lg = ajustes_idioma();
  return lg == IDIOMA_ZHCN ? ESC_CJK_SC : lg == IDIOMA_ZHTW ? ESC_CJK_TC : ESC_CJK;
}

// A fonte de reserva de `e` que melhor desenha `s`, na lista da escrita: a
// primeira que desenha tudo, ou a que deixa menos de fora. NULL se nenhuma abriu.
static TTF_Font *reservaDe(Escrita e, TxtEstilo estilo, const char *s) {
  TTF_Font *melhor = NULL;
  int menos = 0x7fffffff;
  for (int c = 0; c < RES_CAND && caminhoReserva[e][c][0]; c++) {
    TTF_Font *f = reservas[e][c][estilo];
    int falta;
    if (!f && !reservaFalhou[e][c][estilo]) {
      f = reservas[e][c][estilo] = TTF_OpenFont(caminhoReserva[e][c],
                                                (int)(ESTILOS[estilo].corpo * ESC_T + 0.5f));
      if (!f) reservaFalhou[e][c][estilo] = 1;
    }
    if (!f) continue;
    falta = faltantes(f, s, NULL);
    if (!falta) return f;
    if (falta < menos) { menos = falta; melhor = f; }
  }
  return melhor;
}

// MEMORIA DA ESCOLHA. fonteDe roda para CADA linha de CADA quadro (linhaFamilia
// a chama duas vezes, a medida de largura outras duas), e desde que a escolha
// olha TODOS os caracteres — e nao so o primeiro fora do ASCII — o custo de uma
// linha acentuada e um TTF_GlyphIsProvided por caractere, por chamada. Numa TV
// ARM com 300 linhas na tela isso e dezenas de milhares de consultas por
// quadro. A resposta so muda com a fonte (familia, estilo), com a variante CJK
// (idioma da interface) e com o texto, entao cabe numa tabela direta chaveada
// pelo hash do texto, como a do i18n. ASCII puro nem entra: sai na varredura.
#define FD_MEM 512
static struct { unsigned long long h; unsigned n; unsigned char fam, estilo, var, cam; TTF_Font *f; } fdMem[FD_MEM];
// LARGURA MEDIDA, GUARDADA (#191). larguraLinha (abaixo) e TTF_SizeUTF8, que
// passa o texto inteiro pelo HarfBuzz; txt_bloco mede CADA prefixo de CADA
// paragrafo e txt_linha_corta mede cada corte, e isso a cada quadro. Nativo
// custa pouco; no WASM da Samsung era o quadro inteiro: perfil do Chrome (4x
// mais lento) na tela de login com o painel de novidades, 88% do tempo em
// larguraLinha -> hb_shape, c-max 20 ms contra 3 ms da 1.5.3 (que media pela
// linha rasterizada e guardada). Na TV (QE65Q80A, registro 11855) c-max 300 ms
// e 3-5 FPS. A resposta so muda com o texto, a fonte e a variante CJK, e a
// tabela e esquecida junto com a de fonteDe.
#define LG_MEM 4096
#define LG_VIAS 4
static struct { unsigned long long h; unsigned n; unsigned char fam, estilo, enf, var, ok, cam; int w; unsigned uso; } lgMem[LG_MEM];
static unsigned lgRelogio;
// COBERTURA DAS FORMAS ARABES (bidi.c). Um bit por codepoint de FB50-FEFF, por
// fonte: "ja perguntei" e "tem". Chaveado pelo ponteiro da fonte, esquecido junto
// com as outras tabelas (a fonte pode ser fechada e reaberta).
#define AR_INI 0xFB50u
#define AR_N   (0xFF00u - AR_INI)
#define AR_FONTES 8
static struct { TTF_Font *f; unsigned char viu[(AR_N + 7) / 8], tem[(AR_N + 7) / 8]; } arMem[AR_FONTES];
static TTF_Font *arLogada;
static void fdEsquecer(void) { memset(fdMem, 0, sizeof fdMem); memset(lgMem, 0, sizeof lgMem); memset(arMem, 0, sizeof arMem); arLogada = NULL; }

// Fonte com que a linha `s` deve ser desenhada. Devolve a principal quando ela
// da conta — que e o caso da esmagadora maioria das linhas.
//
// A LINHA INTEIRA muda de fonte quando falta QUALQUER caractere (e nao so o
// primeiro fora do ASCII, como era): "OK · 再生" comeca por um "·", que a Inter
// tem, e o teste do primeiro caractere a deixava desenhar o resto em
// quadradinhos. Ordem da troca:
//   1. a INTER embarcada, se a familia escolhida nao e a Inter e ela desenha
//      tudo. Cobre latim estendido, vietnamita (pilhas de diacriticos), grego e
//      cirilico nos tres pesos (conferido por tools/idiomas.py) e e a unica
//      fonte que existe em TODA plataforma — inclusive o WASM da Samsung, que nao
//      tem fonte de sistema nenhuma. Antes disso Montserrat/Roboto sem grego ou
//      Atkinson sem vietnamita/cirilico iam para a reserva do sistema, que no
//      WASM nao existe.
//   2. a lista de reserva da ESCRITA do primeiro caractere que a Inter tambem nao
//      tem (japones/chines/hangul, arabe, o resto), fontes de sistema da TV e por
//      ultimo a CJK embarcada (DroidSansFallback-Subset.ttf).
static TTF_Font *fonteDeLento(TxtFamilia familia, TxtEstilo estilo, const char *s,
                              TTF_Font *principal);
static TTF_Font *fonteDe(TxtFamilia familia, TxtEstilo estilo, const char *s) {
  TTF_Font *principal = NULL, *r;
  if (familia < TXT_FAMILIA_INTER || familia >= TXT_FAMILIA_N)
    familia = TXT_FAMILIA_INTER;
  if (!fontes[familia][estilo] && !familiaTentada[familia])
    carregarFamilia(familia);
  if (!fontes[familia][estilo] && familiaCarregada[familia])
    abrirEstiloCamada(familia, estilo);
  if (!fontes[familia][estilo]) {
    if (familia == fonteInterface && !avisoFallback[familia]) {
      printf("fonte de interface %s indisponivel; usando %s\n",
             TXT_FAMILIAS_PT[familia], TXT_FAMILIAS_PT[fonteInterfaceFallback]);
      avisoFallback[familia] = 1;
    }
    familia = familiaCarregada[familia] ? familia : fonteInterfaceFallback;
    if (!fontes[familia][estilo] && familiaCarregada[familia])
      abrirEstiloCamada(familia, estilo);
    if (!fontes[familia][estilo]) familia = TXT_FAMILIA_INTER;
    if (!fontes[familia][estilo] && familiaCarregada[familia])
      abrirEstiloCamada(familia, estilo);
  }
  principal = fontes[familia][estilo];
  if (!principal) return NULL;
  unsigned long long h = 1469598103934665603ull;
  unsigned n = 0, slot;
  { const unsigned char *p = (const unsigned char *)s;
    while (*p && *p < 0x80) p++;
    if (!*p) return principal;              // ASCII puro: o caminho de quase toda linha
    for (p = (const unsigned char *)s; *p; p++, n++) { h ^= *p; h *= 1099511628211ull; } }
  slot = (unsigned)(h % FD_MEM);
  { const unsigned char var = (unsigned char)variacaoCjk();
    if (fdMem[slot].f && fdMem[slot].h == h && fdMem[slot].n == n && fdMem[slot].fam == familia &&
        fdMem[slot].estilo == estilo && fdMem[slot].var == var && fdMem[slot].cam == camada)
      return fdMem[slot].f;
    r = fonteDeLento(familia, estilo, s, principal);
    fdMem[slot].h = h; fdMem[slot].n = n; fdMem[slot].fam = (unsigned char)familia;
    fdMem[slot].estilo = (unsigned char)estilo; fdMem[slot].var = var; fdMem[slot].f = r;
    fdMem[slot].cam = (unsigned char)camada;
    return r; }
}

// A escolha em si (ver fonteDe), para uma linha com caractere fora do ASCII.
static TTF_Font *fonteDeLento(TxtFamilia familia, TxtEstilo estilo, const char *s,
                              TTF_Font *principal) {
  Uint32 cp = 0;
  Escrita e;
  TTF_Font *r;
  if (!faltantes(principal, s, &cp)) return principal;
  if (familia != TXT_FAMILIA_INTER) {
    if (!fontes[TXT_FAMILIA_INTER][estilo] && !familiaTentada[TXT_FAMILIA_INTER])
      carregarFamilia(TXT_FAMILIA_INTER);
    if (!fontes[TXT_FAMILIA_INTER][estilo] && familiaCarregada[TXT_FAMILIA_INTER])
      abrirEstiloCamada(TXT_FAMILIA_INTER, estilo);
    r = fontes[TXT_FAMILIA_INTER][estilo];
    if (r) {
      Uint32 cpInter = 0;
      if (!faltantes(r, s, &cpInter)) return r;
      cp = cpInter;
    }
  }
  if (!cp) return principal;
  e = escritaDe(cp);
  if (e == ESC_CJK) e = variacaoCjk();
  r = reservaDe(e, estilo, s);
  return r ? r : principal;
}

typedef struct { TTF_Font *f; int slot; } ArCtx;
static int arTemGlifo(unsigned cp, void *u) {
  ArCtx *c = (ArCtx *)u;
  unsigned i;
  if (!c->f || cp < AR_INI || cp >= 0xFF00u) return 1;
  i = cp - AR_INI;
  if (!(arMem[c->slot].viu[i >> 3] & (1u << (i & 7)))) {
    arMem[c->slot].viu[i >> 3] |= (unsigned char)(1u << (i & 7));
    if (TTF_GlyphIsProvided(c->f, (Uint16)cp)) arMem[c->slot].tem[i >> 3] |= (unsigned char)(1u << (i & 7));
  }
  return (arMem[c->slot].tem[i >> 3] >> (i & 7)) & 1;
}

// O contexto de arTemGlifo para a fonte `f` (a que desenha o arabe): uma casa
// de arMem por fonte. Loga uma vez por FAMILIA se a juncao esta ligada.
static void arCtxDe(TTF_Font *f, ArCtx *c) {
  int i, livre = -1;
  c->f = f; c->slot = 0;
  if (!f) return;
  for (i = 0; i < AR_FONTES; i++) {
    if (arMem[i].f == f) { c->slot = i; livre = -2; break; }
    if (!arMem[i].f && livre == -1) livre = i;
  }
  if (livre != -2) {
    if (livre < 0) { livre = 0; memset(&arMem[0], 0, sizeof arMem[0]); }
    arMem[livre].f = f; c->slot = livre;
  }
  // Uma linha por FAMILIA, nao por troca de fonte: cada tamanho e um
  // TTF_Font proprio e a alternancia entre eles repetia a linha 113 vezes
  // num log de 200 KB (Samsung, 05/10/2026).
  if (arLogada != f) {
    const char *nome = TTF_FontFaceFamilyName(f);
    static char familiaLogada[64];
    arLogada = f;
    if (strcmp(familiaLogada, nome ? nome : "?")) {
      snprintf(familiaLogada, sizeof familiaLogada, "%s", nome ? nome : "?");
      if (arTemGlifo(0xFEFB, c) && arTemGlifo(0xFEE1, c) && arTemGlifo(0xFEB3, c))
        printf("[bidi] arabic: shaping on (font %s)\n", nome ? nome : "?");
      else
        printf("[bidi] arabic: reorder only, font %s lacks presentation forms\n", nome ? nome : "?");
    }
  }
}

// Tem letra de escrita da direita para a esquerda (hebraico, arabe, formas de
// apresentacao)? So entao o texto passa pelo bidi.
static int temRtl(const char *s) {
  const unsigned char *p = (const unsigned char *)s;
  while (*p) {
    int n = 1;
    Uint32 cp;
    if (*p < 0x80) { p++; continue; }
    cp = decodifica(p, &n);
    p += n;
    if ((cp >= 0x0590 && cp <= 0x08FF) || (cp >= 0xFB1D && cp <= 0xFDFF) ||
        (cp >= 0xFE70 && cp <= 0xFEFC)) return 1;
  }
  return 0;
}

// Controles de direcao e de formato que o TMDB e os addons mandam no meio do
// texto arabe (LRM/RLM, LRE..RLO/PDF, isolates, ALM, BOM). Nenhuma fonte deste
// app tem glifo para quase todos: viravam o quadradinho "do lado da frase". O
// bidi.c nao implementa embutimento, entao eles tambem nao ajudariam a ordem.
// Os de EMBUTIMENTO (LRE..RLO/PDF, isolates) e o BOM saem antes do bidi; as
// MARCAS (LRM, RLM, ALM) entram nele como letra forte — "John Wick\u200E (2014)"
// depende disso — e saem depois, junto com ZWJ/ZWNJ.
static int formatoInvisivel(Uint32 cp) {
  return (cp >= 0x202A && cp <= 0x202E) || (cp >= 0x2066 && cp <= 0x2069) || cp == 0xFEFF;
}
static int marcaInvisivel(Uint32 cp) {
  return cp == 0x200C || cp == 0x200D || cp == 0x200E || cp == 0x200F || cp == 0x061C;
}

// TEXTO RTL EM ORDEM VISUAL, PARA TODA LINHA (TMDB, addons, legenda). O
// SDL_ttf do .tpk 4/5 (2.22, sem HarfBuzz) desenha um codepoint por vez da
// esquerda para a direita: sinopse, elenco e meta em arabe saiam com as letras
// soltas e de tras para frente. Antes so a legenda passava pelo bidi.c; agora
// linhaFamilia e a medida passam aqui, entao todo caminho (txt_linha, corte com
// reticencias, txt_bloco) recebe o mesmo tratamento. A quebra e o corte
// continuam no texto LOGICO, linha por linha, como bidi.h pede.
// Latim (nenhuma letra RTL) volta o proprio `s`, byte a byte igual.
static const char *visualDe(TTF_Font *f, const char *s, char *dst, size_t tam) {
  char tmp[1024];
  size_t k = 0;
  const unsigned char *p;
  ArCtx c;
  int r;
  if (!temRtl(s) || strlen(s) >= sizeof tmp) return s;
  for (p = (const unsigned char *)s; *p;) {
    int n = 1;
    Uint32 cp = decodifica(p, &n);
    if (!formatoInvisivel(cp)) { memcpy(tmp + k, p, (size_t)n); k += (size_t)n; }
    p += n;
  }
  tmp[k] = 0;
  arCtxDe(f, &c);
  r = bidi_visual_utf8_ex(tmp, dst, tam, c.f ? arTemGlifo : NULL, &c);
  if (r < 0) return s;
  // As marcas ja fizeram o seu trabalho (juncao, direcao); desenhadas, viram caixa.
  { char *w = dst; const unsigned char *q = (const unsigned char *)dst;
    while (*q) {
      int n = 1;
      Uint32 cp = decodifica(q, &n);
      if (!marcaInvisivel(cp)) { memmove(w, q, (size_t)n); w += n; }
      q += n;
    }
    *w = 0; }
  return dst;
}

// Linha de legenda, ja quebrada. Era aqui que a legenda ganhava a ordem
// visual; desde que linhaFamilia faz isso para TODA linha (visualDe), aplicar
// de novo inverteria o texto uma segunda vez. Fica a copia, para quem chama
// (player.c, legendasui.c) nao mudar.
int txt_bidi_legenda(TxtFamilia familia, TxtEstilo estilo, const char *in, char *out, size_t tam) {
  size_t n;
  (void)familia; (void)estilo;
  if (!tam) return 0;
  n = in ? strlen(in) : 0;
  if (n >= tam) {
    n = tam - 1;
    while (n && ((unsigned char)in[n] & 0xC0) == 0x80) n--;
  }
  if (n) memcpy(out, in, n);
  out[n] = 0;
  return 0;
}

// Qual fonte desenharia a linha, em texto: "principal", "inter" (a Inter
// embarcada no lugar da familia escolhida) ou "reserva:<escrita>:<arquivo>". NULL
// se nenhuma fonte carregou. Existe para o teste e para o diagnostico: o
// resultado de fonteDe so aparece na tela, e um quadradinho de .notdef nao se
// mede.
const char *txt_fonte_da_linha(TxtFamilia familia, TxtEstilo estilo, const char *s) {
  static char buf[640];
  static const char *NOME[ESC_N] = { "CJK", "CJK-sc", "CJK-tc", "arabe", "tailandes", "resto" };
  camadaAtualizar();
  TTF_Font *f = (s && estilo >= 0 && estilo < TXT_NFONTES) ? fonteDe(familia, estilo, s) : NULL;
  if (!f) return NULL;
  for (int fam = 0; fam < TXT_FAMILIA_N; fam++)
    if (fontes[fam][estilo] == f)
      return fam == (int)familia || familia < 0 || familia >= TXT_FAMILIA_N ? "principal" : "inter";
  for (int e = 0; e < ESC_N; e++)
    for (int c = 0; c < RES_CAND; c++)
      if (reservas[e][c][estilo] == f) {
        snprintf(buf, sizeof buf, "reserva:%s:%s", NOME[e], caminhoReserva[e][c]);
        return buf;
      }
  return "principal";
}

static TTF_Font *fonteLegendaDe(TxtEstilo estilo, const char *s,
                                TxtFamilia familia) {
  if (familia == TXT_FAMILIA_LG && estilo >= TXT_LEG_50 && estilo <= TXT_LEG_250) {
    int i = estilo - TXT_LEG_50;
    if (!fontesLegendaLG[i] && !tentouLegendaLG[i]) {
      tentouLegendaLG[i] = 1;
#ifdef NV_ANDROID
      fontesLegendaLG[i] = TTF_OpenFont("/system/fonts/Roboto-Regular.ttf",
#else
      fontesLegendaLG[i] = TTF_OpenFont("/usr/share/fonts/LG_Display-Regular.ttf",
#endif
          (int)(ESTILOS[estilo].corpo * ESC_T + 0.5f));
      if (!fontesLegendaLG[i] && !avisoFallback[TXT_FAMILIA_LG]) {
        printf("fonte de legenda LG Display indisponivel; usando fallback\n");
        avisoFallback[TXT_FAMILIA_LG] = 1;
      }
    }
    // #369: a LG Display/Roboto da legenda nao tem tailandes (nem arabe nem CJK);
    // linha com glifo que ela nao desenha segue para a reserva por escrita.
    if (fontesLegendaLG[i] && !faltantes(fontesLegendaLG[i], s, NULL)) return fontesLegendaLG[i];
  }
  return fonteDe(familia, estilo, s);
}

// NEGRITO DA LEGENDA COM A FACE BOLD DE VERDADE. A folha de legenda liga o
// negrito por TTF_STYLE_BOLD sobre a face Regular. Nem o SDL_ttf da C9
// (2.0.14, /usr/lib: nenhum FT_*Embolden entre os simbolos que ele carrega)
// nem o 2.24 do Mac emboldam o contorno: o negrito sintetico sai do BITMAP ja
// rasterizado, o glifo engorda SO na horizontal e a rampa de antialias some
// (tests/legenda_negrito_shot.c). Medido em
// Montserrat 36 px ("Nao depois do que aconteceu no vale."): 789 px de largura
// contra 718 da Montserrat-Bold (+9,9%, a "legenda esticada"), rampa de borda
// de 1,20 px contra 1,55 e 26% de pixels de borda intermediarios contra 35%
// (a "legenda pixelada"). Onde a familia traz o arquivo Bold, a linha em
// negrito usa ESSA face, aberta sob demanda sobre os bytes ja lidos e no mesmo
// corpo. LG Display, Droid e as reservas (CJK, arabe) nao tem o arquivo e
// seguem no sintetico.
static TTF_Font  *fontesNegCam[TXT_NCAM][TXT_FAMILIA_N][TXT_LEG_N];
static SDL_RWops *rwNegCam[TXT_NCAM][TXT_FAMILIA_N][TXT_LEG_N];
static unsigned char tentouNegCam[TXT_NCAM][TXT_FAMILIA_N][TXT_LEG_N];

static void fecharNegrito(int c, int f) {
  for (int i = 0; i < TXT_LEG_N; i++) {
    if (fontesNegCam[c][f][i]) TTF_CloseFont(fontesNegCam[c][f][i]);
    if (rwNegCam[c][f][i]) SDL_FreeRW(rwNegCam[c][f][i]);
    fontesNegCam[c][f][i] = NULL; rwNegCam[c][f][i] = NULL; tentouNegCam[c][f][i] = 0;
  }
}

// A face Bold que substitui o TTF_STYLE_BOLD de `fonte`, ou NULL (fica o
// sintetico). So quando `fonte` e a face principal da familia naquele estilo.
static TTF_Font *fonteNegritoReal(TxtFamilia familia, TxtEstilo estilo, TTF_Font *fonte) {
  int i = estilo - TXT_LEG_50;
  if (familia < TXT_FAMILIA_INTER || familia >= TXT_FAMILIA_N ||
      familia == TXT_FAMILIA_LG || familia == TXT_FAMILIA_DROID) return NULL;
  if (estilo < TXT_LEG_50 || estilo > TXT_LEG_250 || ESTILOS[estilo].peso == PESO_BOLD) return NULL;
  if (!fonte || fonte != fontes[familia][estilo]) return NULL;
  if (!bytesPeso[familia][PESO_BOLD] ||
      bytesPeso[familia][PESO_BOLD] == bytesPeso[familia][PESO_REGULAR]) return NULL;
  if (!fontesNegCam[camada][familia][i] && !tentouNegCam[camada][familia][i]) {
    SDL_RWops *rw = SDL_RWFromConstMem(bytesPeso[familia][PESO_BOLD], (int)tamPeso[familia][PESO_BOLD]);
    tentouNegCam[camada][familia][i] = 1;
    if (rw) {
      fontesNegCam[camada][familia][i] = TTF_OpenFontRW(rw, 0, (int)(ESTILOS[estilo].corpo * ESC_T + 0.5f));
      if (fontesNegCam[camada][familia][i]) {
        rwNegCam[camada][familia][i] = rw;
        // Uma vez por familia/tamanho: a prova na TV de que o negrito saiu da
        // face Bold e nao do borrao do SDL_ttf.
        printf("[leg] negrito: face Bold real de %s, %d px (nao TTF_STYLE_BOLD)\n",
               TXT_FAMILIAS_PT[familia], (int)(ESTILOS[estilo].corpo * ESC_T + 0.5f));
        fflush(stdout);
      } else SDL_FreeRW(rw);
    }
  }
  return fontesNegCam[camada][familia][i];
}

// A fonte e o estilo TTF de uma linha com enfase: troca a Regular pela Bold
// real quando ha, e tira dela o bit de negrito (a face ja e negrita).
static TTF_Font *fonteComEnfase(TxtFamilia familia, TxtEstilo estilo, TTF_Font *fonte,
                                int enfase, int *estiloTtf) {
  int novo = TTF_GetFontStyle(fonte);
  if (enfase & TXT_ENF_NEGRITO) {
    TTF_Font *real = fonteNegritoReal(familia, estilo, fonte);
    if (real) { fonte = real; novo = TTF_GetFontStyle(fonte); }
    else novo |= TTF_STYLE_BOLD;
  }
  if (enfase & TXT_ENF_ITALICO) novo |= TTF_STYLE_ITALIC;
  *estiloTtf = novo;
  return fonte;
}

// Os bits TTF de uma enfase, para as fontes de corrida (ver renderCorridas).
static int enfaseTtf(int enfase) {
  return ((enfase & TXT_ENF_NEGRITO) ? TTF_STYLE_BOLD : 0) |
         ((enfase & TXT_ENF_ITALICO) ? TTF_STYLE_ITALIC : 0);
}

static void liberarFamilia(TxtFamilia familia) {
  fdEsquecer();
  for (int c = 0; c < TXT_NCAM; c++) fecharNegrito(c, familia);
  for (int c = 0; c < TXT_NCAM; c++)
    for (int i = 0; i < TXT_NFONTES; i++) {
      if (fontesCam[c][familia][i]) TTF_CloseFont(fontesCam[c][familia][i]);
      fontesCam[c][familia][i] = NULL;
      if (rwFonteCam[c][familia][i]) SDL_FreeRW(rwFonteCam[c][familia][i]);
      rwFonteCam[c][familia][i] = NULL;
    }
  for (int p = 0; p < 3; p++) {
    if (donoPeso[familia][p]) free(bytesPeso[familia][p]);
    bytesPeso[familia][p] = NULL;
    tamPeso[familia][p] = 0;
    donoPeso[familia][p] = 0;
  }
  familiaCarregada[familia] = 0;
}

// Cada família lê só seus três arquivos, e só quando uma linha passa a usar
// aquela família. As dezenas de TTF_Font por tamanho são abertas sobre os
// mesmos buffers em memória; a TV não relê cada arquivo para cada estilo.
static int carregarFamilia(TxtFamilia familia) {
  if (familia < TXT_FAMILIA_INTER || familia >= TXT_FAMILIA_N) return 0;
  if (familiaCarregada[familia]) return 1;
  if (familiaTentada[familia]) return 0;
  familiaTentada[familia] = 1;
  for (int p = 0; p < 3; p++) {
    int j;
    for (j = 0; j < p; j++)
      if (!strcmp(caminhoPeso[familia][p], caminhoPeso[familia][j])) break;
    if (j < p) {
      bytesPeso[familia][p] = bytesPeso[familia][j];
      tamPeso[familia][p] = tamPeso[familia][j];
      continue;
    }
    bytesPeso[familia][p] = lerTudo(caminhoPeso[familia][p], &tamPeso[familia][p]);
    donoPeso[familia][p] = bytesPeso[familia][p] != NULL;
    if (!bytesPeso[familia][p]) {
      printf("fonte %s indisponivel: %s\n", TXT_FAMILIAS_PT[familia],
             caminhoPeso[familia][p]);
      liberarFamilia(familia);
      familiaTentada[familia] = 1;
      return 0;
    }
  }
  for (int i = 0; i < TXT_NFONTES; i++) {
    int peso = ESTILOS[i].peso;
    const unsigned char *buf;
    size_t tam;
    SDL_RWops *rw;
    if (peso >= PESO_MONO_R) {
      int m = peso - PESO_MONO_R;
      if (!monoTentada[m]) {
        monoTentada[m] = 1;
        bytesMono[m] = lerTudo(caminhoMono[m], &tamMono[m]);
        if (!bytesMono[m]) printf("fonte mono indisponivel: %s\n", caminhoMono[m]);
      }
      if (bytesMono[m]) { buf = bytesMono[m]; tam = tamMono[m]; }
      else { peso = m == 1 ? PESO_BOLD : PESO_REGULAR; buf = bytesPeso[familia][peso]; tam = tamPeso[familia][peso]; }
    } else { buf = bytesPeso[familia][peso]; tam = tamPeso[familia][peso]; }
    rw = SDL_RWFromConstMem(buf, (int)tam);
    fontes[familia][i] = rw
      ? TTF_OpenFontRW(rw, 0, (int)(ESTILOS[i].corpo * ESC_T + 0.5f))
      : NULL;
    rwFonte[familia][i] = rw;
    if (!fontes[familia][i]) {
      if (rw) SDL_FreeRW(rw);
      rwFonte[familia][i] = NULL;
      printf("TTF_OpenFont %s: %s\n", TXT_FAMILIAS_PT[familia], TTF_GetError());
      liberarFamilia(familia);
      familiaTentada[familia] = 1;
      return 0;
    }
    // As fontes de sistema legadas não trazem faces de todos os pesos.
    if ((familia == TXT_FAMILIA_LG || familia == TXT_FAMILIA_DROID) &&
        peso == PESO_BOLD)
      TTF_SetFontStyle(fontes[familia][i], TTF_STYLE_BOLD);
  }
  familiaCarregada[familia] = 1;
  printf("fonte: %s (%d estilos, 3 arquivos)\n", TXT_FAMILIAS_PT[familia], TXT_NFONTES);
  return 1;
}

// Um estilo da camada ATUAL, aberto sobre os bytes ja lidos da familia. E o
// caminho da camada ampliada (que nasce vazia) e o de uma familia carregada
// primeiro dentro dela; falhar aqui so deixa o estilo sem fonte (fonteDe cai
// na reserva), nunca derruba a familia.
static void abrirEstiloCamada(TxtFamilia familia, int i) {
  int peso = ESTILOS[i].peso;
  const unsigned char *buf;
  size_t tam;
  SDL_RWops *rw;
  // A mono do registro: os mesmos bytes que carregarFamilia leu (ou, sem o
  // arquivo, o Regular/Bold da familia, como la).
  if (peso >= PESO_MONO_R) {
    int m = peso - PESO_MONO_R;
    if (bytesMono[m]) { buf = bytesMono[m]; tam = tamMono[m]; }
    else { peso = m == 1 ? PESO_BOLD : PESO_REGULAR; buf = bytesPeso[familia][peso]; tam = tamPeso[familia][peso]; }
  } else { buf = bytesPeso[familia][peso]; tam = tamPeso[familia][peso]; }
  if (!buf) return;
  rw = SDL_RWFromConstMem(buf, (int)tam);
  if (!rw) return;
  fontes[familia][i] = TTF_OpenFontRW(rw, 0, (int)(ESTILOS[i].corpo * ESC_T + 0.5f));
  if (!fontes[familia][i]) { SDL_FreeRW(rw); return; }
  rwFonte[familia][i] = rw;
  if ((familia == TXT_FAMILIA_LG || familia == TXT_FAMILIA_DROID) && peso == PESO_BOLD)
    TTF_SetFontStyle(fontes[familia][i], TTF_STYLE_BOLD);
}

// Fecha tudo o que a camada ampliada abriu e esquece as linhas dela: o fator
// mudou (o ajuste foi trocado) e as fontes daquele tamanho nao servem mais.
static void fecharCamadaAmpliada(int k) {
  char pre[16];
  size_t np;
  if (k <= 0 || k >= TXT_NCAM) return;
  for (int f = 0; f < TXT_FAMILIA_N; f++)
    for (int i = 0; i < TXT_NFONTES; i++) {
      if (fontesCam[k][f][i]) TTF_CloseFont(fontesCam[k][f][i]);
      fontesCam[k][f][i] = NULL;
      if (rwFonteCam[k][f][i]) SDL_FreeRW(rwFonteCam[k][f][i]);
      rwFonteCam[k][f][i] = NULL;
    }
  for (int f = 0; f < TXT_FAMILIA_N; f++) fecharNegrito(k, f);
  for (int i = 0; i < TXT_LEG_N; i++) {
    if (fontesLegendaLGCam[k][i]) TTF_CloseFont(fontesLegendaLGCam[k][i]);
    fontesLegendaLGCam[k][i] = NULL;
  }
  memset(tentouLegendaLGCam[k], 0, sizeof tentouLegendaLGCam[k]);
  for (int e = 0; e < ESC_N; e++)
    for (int c = 0; c < RES_CAND; c++)
      for (int i = 0; i < TXT_NFONTES; i++)
        if (reservasCam[k][e][c][i]) { TTF_CloseFont(reservasCam[k][e][c][i]); reservasCam[k][e][c][i] = NULL; }
  memset(reservaFalhouCam[k], 0, sizeof reservaFalhouCam[k]);
  // So as linhas DESTE fator: a chave da camada comeca por "E<fator*100>:".
  snprintf(pre, sizeof pre, "E%d:", (int)(escCamSlot[k] * 100.0f + 0.5f));
  np = strlen(pre);
  for (int i = 0; i < MAX_LINHAS; i++)
    if (cache[i].ocupado && escCamSlot[k] > 0.0f && !strncmp(cache[i].chave, pre, np)) {
      if (cache[i].linha.tex) {
        gfx_tex_esquecer(cache[i].linha.tex);
        glDeleteTextures(1, &cache[i].linha.tex);
      }
      memset(&cache[i], 0, sizeof cache[i]);
    }
  escCamSlot[k] = 0.0f;
  fdEsquecer();
}

// Qual camada vale agora: a casa do fator do gfx (gfx_escala). Barata — uma
// comparacao por casa — e chamada na entrada de quem rasteriza, mede ou desenha.
static void camadaAtualizar(void) {
  float e = gfx_escala();
  int k, livre = -1, velha = 1;
  if (e == 1.0f) { camada = 0; return; }
  if (camada > 0 && escCamSlot[camada] == e) { usoCam[camada] = ++relogioCam; return; }
  for (k = 1; k < TXT_NCAM; k++) {
    if (escCamSlot[k] == e) { camada = k; usoCam[k] = ++relogioCam; return; }
    if (escCamSlot[k] <= 0.0f && livre < 0) livre = k;
    if (usoCam[k] < usoCam[velha]) velha = k;
  }
  // Fator novo: uma casa livre, ou a menos usada (fechada antes de reusar).
  k = livre > 0 ? livre : velha;
  if (livre < 0) fecharCamadaAmpliada(k);
  escCamSlot[k] = e;
  usoCam[k] = ++relogioCam;
  camada = k;
}

int txt_iniciar(const char *dirRecursos, float escala) {
  if (escala < 0.5f) escala = 1.0f;
  escalaTxt = escala;
  if (TTF_Init() != 0) { printf("TTF_Init: %s\n", TTF_GetError()); return 0; }
  char base[512] = "";
  if (dirRecursos && *dirRecursos) snprintf(base, sizeof base, "%s/", dirRecursos);
  else {
    char *bp = SDL_GetBasePath();
    if (bp) { snprintf(base, sizeof base, "%s", bp); SDL_free(bp); }
  }

  snprintf(caminhoPeso[TXT_FAMILIA_INTER][0], 512, "%sfonts/InterDisplay-Regular.ttf", base);
  snprintf(caminhoPeso[TXT_FAMILIA_INTER][1], 512, "%sfonts/InterDisplay-Medium.ttf", base);
  snprintf(caminhoPeso[TXT_FAMILIA_INTER][2], 512, "%sfonts/InterDisplay-Bold.ttf", base);
  snprintf(caminhoPeso[TXT_FAMILIA_MONTSERRAT][0], 512, "%sfonts/Montserrat-Regular.ttf", base);
  snprintf(caminhoPeso[TXT_FAMILIA_MONTSERRAT][1], 512, "%sfonts/Montserrat-Medium.ttf", base);
  snprintf(caminhoPeso[TXT_FAMILIA_MONTSERRAT][2], 512, "%sfonts/Montserrat-Bold.ttf", base);
  snprintf(caminhoPeso[TXT_FAMILIA_ROBOTO][0], 512, "%sfonts/Roboto-Regular.ttf", base);
  snprintf(caminhoPeso[TXT_FAMILIA_ROBOTO][1], 512, "%sfonts/Roboto-Medium.ttf", base);
  snprintf(caminhoPeso[TXT_FAMILIA_ROBOTO][2], 512, "%sfonts/Roboto-Bold.ttf", base);
  snprintf(caminhoPeso[TXT_FAMILIA_ATKINSON][0], 512, "%sfonts/AtkinsonHyperlegibleNext-Regular.ttf", base);
  snprintf(caminhoPeso[TXT_FAMILIA_ATKINSON][1], 512, "%sfonts/AtkinsonHyperlegibleNext-Medium.ttf", base);
  snprintf(caminhoPeso[TXT_FAMILIA_ATKINSON][2], 512, "%sfonts/AtkinsonHyperlegibleNext-Bold.ttf", base);
  snprintf(caminhoMono[0], 512, "%sfonts/JetBrainsMonoNL-Regular.ttf", base);
  snprintf(caminhoMono[1], 512, "%sfonts/JetBrainsMonoNL-SemiBold.ttf", base);
  snprintf(caminhoMono[2], 512, "%sfonts/Montserrat-ExtraLight-Relogio.ttf", base);
#ifdef NV_ANDROID
  // Android: nao ha LG_Display nem /usr/share/fonts. A "LG" vira Roboto do
  // sistema e a "Droid" a DroidSans (so nas versoes antigas) ou Roboto.
  snprintf(caminhoPeso[TXT_FAMILIA_LG][0], 512, "%s", "/system/fonts/Roboto-Light.ttf");
  snprintf(caminhoPeso[TXT_FAMILIA_LG][1], 512, "%s", "/system/fonts/Roboto-Regular.ttf");
  snprintf(caminhoPeso[TXT_FAMILIA_LG][2], 512, "%s", "/system/fonts/Roboto-Regular.ttf");
  for (int p = 0; p < 3; p++)
    snprintf(caminhoPeso[TXT_FAMILIA_DROID][p], 512, "%s",
             access("/system/fonts/DroidSans.ttf", R_OK) == 0 ? "/system/fonts/DroidSans.ttf"
                                                              : "/system/fonts/Roboto-Regular.ttf");
#else
  snprintf(caminhoPeso[TXT_FAMILIA_LG][0], 512, "%s", "/usr/share/fonts/LG_Display-Light.ttf");
  snprintf(caminhoPeso[TXT_FAMILIA_LG][1], 512, "%s", "/usr/share/fonts/LG_Display-Regular.ttf");
  snprintf(caminhoPeso[TXT_FAMILIA_LG][2], 512, "%s", "/usr/share/fonts/LG_Display-Regular.ttf");
  for (int p = 0; p < 3; p++)
    snprintf(caminhoPeso[TXT_FAMILIA_DROID][p], 512, "%s", "/usr/share/fonts/DroidSans.ttf");
#endif

  // Fontes de RESERVA (ver Escrita), em ordem de preferencia por escrita. Na TV
  // sao as de sistema (LG e Droid, medidas no cmap de uma C9); no Mac, as do
  // sistema que cobrem cada escrita — ali isto e so para a previa nao mentir; e
  // por ultimo, para o CJK e o arabe, o subconjunto embarcado (o WASM da Samsung
  // nao tem fonte de sistema nenhuma).
  // Largura RES_CAND: as listas abaixo tem no maximo 7 caminhos, e o laco para
  // no NULL. Com um vetor menor que a lista o terminador era descartado em
  // silencio e a busca seguia lendo a linha de baixo.
  char cjkEmbarcada[600];
  snprintf(cjkEmbarcada, sizeof cjkEmbarcada, "%sfonts/DroidSansFallback-Subset.ttf", base);
  // Arabe: a Samsung nao traz fonte arabe (#253, #258). O recorte tem as formas
  // de apresentacao que o bidi.c produz (tools/fonte-arabe.py).
  char arabeEmbarcada[600];
  snprintf(arabeEmbarcada, sizeof arabeEmbarcada, "%sfonts/NotoNaskhArabic-Subset.ttf", base);
  // Tailandes (#369): nenhuma outra fonte embarcada tem os glifos U+0E01-0E5B, e
  // o DroidSansFallback-Subset e a Roboto/Inter tambem nao. Noto Sans Thai v2.002
  // Regular (OFL), inteira, 21 KB.
  char tailandesEmbarcada[600];
  snprintf(tailandesEmbarcada, sizeof tailandesEmbarcada, "%sfonts/NotoSansThai-Regular.ttf", base);
  snprintf(arabeBoldCaminho, sizeof arabeBoldCaminho, "%sfonts/NotoNaskhArabic-Bold.ttf", base);
  { const char *cand[ESC_N][RES_CAND + 1] = {
      /* ESC_CJK (japones)  */ { "/usr/share/fonts/LG_Display_JP.ttf",
                                 "/usr/share/fonts/DroidSansFallback.ttf",
                                 "/usr/share/fonts/LG_Display-Regular.ttf",
                                 "/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc",
                                 "/System/Library/Fonts/Hiragino Sans GB.ttc",
                                 "/system/fonts/NotoSansCJK-Regular.ttc",
                                 "/system/fonts/NotoSansCJKjp-Regular.otf",
                                 cjkEmbarcada, NULL },
      /* ESC_CJK_SC (zh-CN) */ { "/usr/share/fonts/DroidSansFallback.ttf",
                                 "/usr/share/fonts/LG_Display-Regular.ttf",
                                 "/System/Library/Fonts/Hiragino Sans GB.ttc",
                                 "/System/Library/Fonts/STHeiti Light.ttc",
                                 "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
                                 "/system/fonts/NotoSansCJK-Regular.ttc",
                                 "/system/fonts/NotoSansSC-Regular.otf",
                                 cjkEmbarcada, NULL },
      /* ESC_CJK_TC (zh-TW) */ { "/usr/share/fonts/LG_Display_HK-Regular.ttf",
                                 "/usr/share/fonts/DroidSansFallback.ttf",
                                 "/usr/share/fonts/LG_Display-Regular.ttf",
                                 "/System/Library/Fonts/STHeiti Light.ttc",
                                 "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
                                 "/system/fonts/NotoSansCJK-Regular.ttc",
                                 "/system/fonts/NotoSansTC-Regular.otf",
                                 cjkEmbarcada, NULL },
      /* ESC_ARABE          */ { "/usr/share/fonts/DroidNaskh-Regular.ttf",
                                 "/usr/share/fonts/LG_Display_Urdu.ttf",
                                 "/System/Library/Fonts/Supplemental/GeezaPro.ttc",
                                 "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
                                 "/system/fonts/NotoNaskhArabic-Regular.ttf",
                                 "/system/fonts/NotoSansArabic-Regular.ttf",
                                 arabeEmbarcada, NULL },
      /* ESC_TAILANDES      */ { tailandesEmbarcada,
                                 "/system/fonts/NotoSansThai-Regular.ttf",
                                 "/system/fonts/NotoSansThaiUI-Regular.ttf",
                                 "/System/Library/Fonts/Supplemental/Thonburi.ttc", NULL },
      /* ESC_CIRILICO_ETC   */ { "/usr/share/fonts/DroidSansFallback.ttf",
                                 "/usr/share/fonts/DroidSans.ttf",
                                 "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
                                 "/system/fonts/NotoSansCJK-Regular.ttc",
                                 "/system/fonts/Roboto-Regular.ttf", NULL },
    };
    const char *nomeEsc[ESC_N] = { "CJK", "CJK-sc", "CJK-tc", "arabe", "tailandes", "resto" };
    // NUVIO_SEM_RESERVA_DE_SISTEMA=1 finge o WASM da Samsung, onde so existe o
    // que vai em deploy/app/fonts: as fontes de sistema saem da lista. Serve
    // para ver, no Mac, o que aquela plataforma desenha (e para o teste).
    const int soEmbarcada = getenv("NUVIO_SEM_RESERVA_DE_SISTEMA") != NULL;
    for (int e = 0; e < ESC_N; e++) {
      int n = 0;
      for (int i = 0; cand[e][i] && n < RES_CAND; i++) {
        FILE *fr;
        if (soEmbarcada && cand[e][i] != cjkEmbarcada && cand[e][i] != arabeEmbarcada &&
            cand[e][i] != tailandesEmbarcada) continue;
        fr = fopen(cand[e][i], "rb");
        if (fr) { fclose(fr);
                  snprintf(caminhoReserva[e][n], sizeof caminhoReserva[e][n], "%s", cand[e][i]);
                  n++; }
      }
      printf("reserva %s: %d fonte(s)%s%s\n", nomeEsc[e], n, n ? ", 1a " : "",
             n ? caminhoReserva[e][0] : "");
    } }

  marco("fontes: inicio");
  const TxtFamilia fallback[] = { TXT_FAMILIA_INTER, TXT_FAMILIA_LG, TXT_FAMILIA_DROID };
  for (int i = 0; i < (int)(sizeof fallback / sizeof fallback[0]); i++)
    if (carregarFamilia(fallback[i])) {
      fonteInterfaceFallback = fallback[i];
      if (fonteInterface < TXT_FAMILIA_INTER || fonteInterface >= TXT_FAMILIA_N)
        fonteInterface = fallback[i];
      marco("fontes: prontas");
      return 1;
    }
  printf("txt: nenhuma fonte carregou\n");
  marco("fontes: nenhuma carregou");
  TTF_Quit();
  return 0;
}

void txt_encerrar(void) {
  arabeBoldFechar();
  limparCacheTexto();
  // ORDEM: a fonte primeiro, o RWops depois, o buffer por ultimo. A face do
  // FreeType ainda referencia o stream, e o stream, os bytes.
  for (int f = 0; f < TXT_FAMILIA_N; f++) liberarFamilia((TxtFamilia)f);
  for (int i = 0; i < TXT_LEG_N; i++) {
    if (fontesLegendaLG[i]) TTF_CloseFont(fontesLegendaLG[i]);
    fontesLegendaLG[i] = NULL;
  }
  memset(tentouLegendaLG, 0, sizeof tentouLegendaLG);
  for (int i = 0; i < TXT_NFONTES; i++)
    for (int e = 0; e < ESC_N; e++)
      for (int c = 0; c < RES_CAND; c++)
        if (reservas[e][c][i]) { TTF_CloseFont(reservas[e][c][i]); reservas[e][c][i] = NULL; }
  memset(reservaFalhou, 0, sizeof reservaFalhou);
  for (int k = 1; k < TXT_NCAM; k++) fecharCamadaAmpliada(k);
  fdEsquecer();
  memset(familiaTentada, 0, sizeof familiaTentada);
  memset(avisoFallback, 0, sizeof avisoFallback);
  memset(caminhoReserva, 0, sizeof caminhoReserva);
  for (int m = 0; m < 3; m++) { free(bytesMono[m]); bytesMono[m] = NULL; tamMono[m] = 0; monoTentada[m] = 0; }
  TTF_Quit();
}

// UMA LINHA, MAIS DE UMA FONTE. A linha com arabe vai inteira para a fonte de
// reserva (fonteDe), e na Samsung a unica e a NotoNaskhArabic-Subset.ttf
// embarcada, que NAO TEM LATIM: so espaco, digitos e ", . : !". Um "·" da linha
// de meta, os parenteses, aspas «», o hifen, o "…" do corte e qualquer nome em
// latim ("Breaking Bad") no meio da sinopse saiam como .notdef — os quadrados
// "do lado das frases" do relato do .tpk 4/5. Agora a linha e desenhada em
// CORRIDAS: cada caractere que a reserva nao tem e a fonte da interface (ou a
// Inter) tem vai para ela, e as corridas sao coladas lado a lado na mesma linha
// de base. So acontece quando a fonte da linha e uma reserva; a linha comum (a
// fonte principal ou a Inter) nao passa aqui e sai identica a antes.
#define TXT_CORR_MAX 32
typedef struct { int ini, fim; TTF_Font *f; } Corrida;

// As fontes que completam a reserva `f`: a principal da familia e a Inter.
// NULL nas duas quando `f` ja e uma delas.
static void altDe(TxtFamilia familia, TxtEstilo estilo, TTF_Font *f,
                  TTF_Font **a, TTF_Font **b) {
  *a = *b = NULL;
  if (familia < TXT_FAMILIA_INTER || familia >= TXT_FAMILIA_N) familia = TXT_FAMILIA_INTER;
  if (!f || estilo < 0 || estilo >= TXT_NFONTES) return;
  if (f == fontes[familia][estilo] || f == fontes[TXT_FAMILIA_INTER][estilo]) return;
  if (estilo >= TXT_LEG_50 && estilo <= TXT_LEG_250 && f == fontesLegendaLG[estilo - TXT_LEG_50]) return;
  *a = fontes[familia][estilo];
  *b = fontes[TXT_FAMILIA_INTER][estilo];
  if (*b == *a) *b = NULL;
}

// Corta `s` em corridas de uma fonte so. Espaco fica na corrida de antes (nao
// vira corrida propria). Mais corridas que `max`: uma so, na fonte da linha.
static int corridasDe(TTF_Font *f, TTF_Font *a, TTF_Font *b, const char *s,
                      Corrida *c, int max) {
  const unsigned char *p = (const unsigned char *)s;
  int n = 0;
  if (!a && !b) { c[0].ini = 0; c[0].fim = (int)strlen(s); c[0].f = f; return 1; }
  while (*p) {
    int tam = 1, pos = (int)((const char *)p - s);
    Uint32 cp = decodifica(p, &tam);
    TTF_Font *g = f;
    if (cp == ' ' && n) g = c[n - 1].f;
    else if (cp < 0x10000 && !TTF_GlyphIsProvided(f, (Uint16)cp)) {
      if (a && TTF_GlyphIsProvided(a, (Uint16)cp)) g = a;
      else if (b && TTF_GlyphIsProvided(b, (Uint16)cp)) g = b;
    }
    if (n && c[n - 1].f == g) c[n - 1].fim = pos + tam;
    else if (n == max) { c[0].ini = 0; c[0].fim = (int)strlen(s); c[0].f = f; return 1; }
    else { c[n].ini = pos; c[n].fim = pos + tam; c[n].f = g; n++; }
    p += tam;
  }
  return n;
}

// #335: negrito SINTETICO sobre arabe estraga a escrita. O Noto Naskh embarcado
// so tem a face Regular, e o SDL_ttf engorda o bitmap ja rasterizado: as letras
// ligadas (formas de apresentacao de bidi.c) se fundem e os pontos viram borrao.
// Texto arabe fica no peso Regular; so o latim da mesma linha recebe o negrito.
static int txtTemLatim(const char *s) {
  for (; s && *s; s++) if ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z')) return 1;
  return 0;
}

// Face Bold REAL do Noto Naskh (#335), aberta sob demanda no corpo da linha em
// curso (arabeCorpo, posto por quem desenha). Sem o arquivo, vale a regra
// acima: arabe sem negrito sintetico. Uma por camada e corpo.
#define ARABE_BOLD_N 24
static struct { int corpo; TTF_Font *f; } arabeBoldCache[TXT_NCAM][ARABE_BOLD_N];
static TTF_Font *arabeBoldFace(void) {
  int i;
  if (!arabeBoldCaminho[0] || arabeCorpo < 4) return NULL;
  for (i = 0; i < ARABE_BOLD_N; i++) {
    if (arabeBoldCache[camada][i].corpo == arabeCorpo) return arabeBoldCache[camada][i].f;
    if (!arabeBoldCache[camada][i].corpo) {
      arabeBoldCache[camada][i].corpo = arabeCorpo;   // tentou: f NULL = arquivo ausente
      arabeBoldCache[camada][i].f = TTF_OpenFont(arabeBoldCaminho, arabeCorpo);
      if (arabeBoldCache[camada][i].f)
        printf("[leg] negrito arabe: face Bold real, %d px\n", arabeCorpo);
      return arabeBoldCache[camada][i].f;
    }
  }
  return NULL;
}
static void arabeBoldFechar(void) {
  int c, i;
  for (c = 0; c < TXT_NCAM; c++)
    for (i = 0; i < ARABE_BOLD_N; i++) {
      if (arabeBoldCache[c][i].f) TTF_CloseFont(arabeBoldCache[c][i].f);
      arabeBoldCache[c][i].f = NULL; arabeBoldCache[c][i].corpo = 0;
    }
}

// Estilo TTF somado a uma fonte de corrida (o negrito/italico da legenda), e
// devolvido depois.
static int estiloSoma(TTF_Font *g, TTF_Font *f, int extra) {
  int ant = TTF_GetFontStyle(g);
  if (g != f && extra && (ant | extra) != ant) TTF_SetFontStyle(g, ant | extra);
  return ant;
}
static void estiloVolta(TTF_Font *g, TTF_Font *f, int ant) {
  if (g != f && TTF_GetFontStyle(g) != ant) TTF_SetFontStyle(g, ant);
}

static int medirCorridas(TTF_Font *f, TTF_Font *a, TTF_Font *b, const char *s,
                         int extra, int *w) {
  Corrida c[TXT_CORR_MAX];
  int n = corridasDe(f, a, b, s, c, TXT_CORR_MAX), i, soma = 0;
  if (n <= 1) { int h; return TTF_SizeUTF8(f, s, w, &h); }
  for (i = 0; i < n; i++) {
    char pedaco[1600];
    int cw = 0, ch = 0, ant, len = c[i].fim - c[i].ini;
    if (len <= 0 || len >= (int)sizeof pedaco) continue;
    memcpy(pedaco, s + c[i].ini, (size_t)len); pedaco[len] = 0;
    { TTF_Font *g = c[i].f; int ex = extra;
      if (bidi_tem_arabe(pedaco) && (extra & TTF_STYLE_BOLD)) {
        TTF_Font *bf = arabeBoldFace();
        if (bf) g = bf;
        ex = extra & ~TTF_STYLE_BOLD;
      }
      ant = estiloSoma(g, f, ex);
      if (TTF_SizeUTF8(g, pedaco, &cw, &ch) == 0) soma += cw;
      estiloVolta(g, f, ant); }
  }
  *w = soma;
  return 0;
}

static SDL_Surface *renderCorridas(TTF_Font *f, TTF_Font *a, TTF_Font *b,
                                   const char *s, SDL_Color cor, int extra) {
  Corrida c[TXT_CORR_MAX];
  SDL_Surface *sf[TXT_CORR_MAX], *out;
  TTF_Font *gf[TXT_CORR_MAX];
  int n = corridasDe(f, a, b, s, c, TXT_CORR_MAX), i, asc = 0, abaixo = 0, w = 0, x = 0;
  if (n <= 1) return TTF_RenderUTF8_Blended(f, s, cor);
  for (i = 0; i < n; i++) {
    char pedaco[1600];
    int ant, len = c[i].fim - c[i].ini, as, ex = extra;
    TTF_Font *g = c[i].f;
    sf[i] = NULL; gf[i] = g;
    if (len <= 0 || len >= (int)sizeof pedaco) continue;
    memcpy(pedaco, s + c[i].ini, (size_t)len); pedaco[len] = 0;
    if (bidi_tem_arabe(pedaco) && (extra & TTF_STYLE_BOLD)) {
      TTF_Font *bf = arabeBoldFace();
      if (bf) g = bf;
      ex = extra & ~TTF_STYLE_BOLD;
    }
    gf[i] = g; as = TTF_FontAscent(g);
    ant = estiloSoma(g, f, ex);
    sf[i] = TTF_RenderUTF8_Blended(g, pedaco, cor);
    estiloVolta(g, f, ant);
    if (!sf[i]) continue;
    if (as > asc) asc = as;
    if (sf[i]->h - as > abaixo) abaixo = sf[i]->h - as;
    w += sf[i]->w;
  }
  out = w > 0 && asc + abaixo > 0
      ? SDL_CreateRGBSurfaceWithFormat(0, w, asc + abaixo, 32, SDL_PIXELFORMAT_ARGB8888) : NULL;
  if (out) SDL_FillRect(out, NULL, 0);
  for (i = 0; i < n; i++) {
    if (!sf[i]) continue;
    if (out) {
      SDL_Rect d = { x, asc - TTF_FontAscent(gf[i]), sf[i]->w, sf[i]->h };
      // Copia crua (alfa incluso): as corridas nao se sobrepoem.
      SDL_SetSurfaceBlendMode(sf[i], SDL_BLENDMODE_NONE);
      SDL_BlitSurface(sf[i], NULL, out, &d);
    }
    x += sf[i]->w;
    SDL_FreeSurface(sf[i]);
  }
  return out;
}

// O que linhaFamilia desenharia para `s` (ver text.h): a string visual, em
// quantas corridas e quantos caracteres ficam sem glifo na fonte da corrida
// (o quadradinho). Para o teste e o diagnostico, como txt_fonte_da_linha.
int txt_visual_da_linha(TxtFamilia familia, TxtEstilo estilo, const char *s,
                        char *out, size_t tam, int *nCorridas) {
  char limpo[1024], vis[1600];
  Corrida c[TXT_CORR_MAX];
  TTF_Font *fl, *a1, *a2;
  const char *v;
  int n, i, falta = 0;
  if (out && tam) out[0] = 0;
  if (nCorridas) *nCorridas = 0;
  camadaAtualizar();
  if (familia < TXT_FAMILIA_INTER || familia >= TXT_FAMILIA_N) familia = TXT_FAMILIA_INTER;
  if (!s || estilo < 0 || estilo >= TXT_NFONTES || !(fl = fonteDe(familia, estilo, s))) return -1;
  altDe(familia, estilo, fl, &a1, &a2);
  s = semDecorativoSemGlifo(fl, a1, a2, s, limpo, sizeof limpo);
  v = visualDe(fl, s, vis, sizeof vis);
  n = corridasDe(fl, a1, a2, v, c, TXT_CORR_MAX);
  for (i = 0; i < n; i++) {
    const unsigned char *p = (const unsigned char *)v + c[i].ini;
    while ((const char *)p < v + c[i].fim) {
      int k = 1;
      Uint32 cp = decodifica(p, &k);
      if (cp != ' ' && (cp >= 0x10000 || !TTF_GlyphIsProvided(c[i].f, (Uint16)cp))) falta++;
      p += k;
    }
  }
  if (out && tam) snprintf(out, tam, "%s", v);
  if (nCorridas) *nCorridas = n;
  return falta;
}

// `enfase` e a combinacao TXT_ENF_* pedida pela LEGENDA ASS, e so por ela.
//
// NEGRITO E ITALICO AQUI SAO SINTETICOS, e isso e uma escolha e nao um
// descuido: o pacote embarca Regular, Medium e Bold da Inter Display e NENHUM
// italico (ver a nota de ESTILOS la em cima — acrescentar arquivo de fonte
// esta fora de questao com o ipk em 166 MB). O SDL_ttf inclina e engorda o
// glifo por conta propria, que e pior do que uma face desenhada e melhor do
// que perder a distincao: num ASS de anime o italico e o que separa o
// pensamento da fala, e o negrito e o que separa o letreiro do dialogo.
//
// A ENFASE ENTRA NA CHAVE DO CACHE. Sem isso a mesma frase em italico e em
// redondo dividiriam a mesma textura, e qual das duas a tela mostra dependeria
// de quem rasterizou primeiro.
static TxtLinha linhaFamilia(TxtEstilo estilo, const char *s, int r, int g,
                             int b, int a, TxtFamilia familia, int enfase) {
  TxtLinha vazia = {0, 0, 0, 0, 0};
  char limpo[1024];
  camadaAtualizar();
  if (familia < TXT_FAMILIA_INTER || familia >= TXT_FAMILIA_N)
    familia = TXT_FAMILIA_INTER;
  if (!s || !*s || estilo < 0 || estilo >= TXT_NFONTES ||
      !fonteDe(familia, estilo, s)) return vazia;

  // ANTES DA CHAVE do cache, para que a linha limpa seja a linha guardada: duas
  // entradas que diferem so por um emoji que ninguem desenha passam a ser a
  // mesma, o que tambem alivia a tabela na tela de fontes.
  // Vale para TODA familia da interface (#186): a limpeza decide por glifo, entao
  // o simbolo que a face sabe desenhar fica; so o que viraria quadrado sai. Ela
  // era so da Inter, e com "LG Display" ou "Droid Sans" o ⚡ dos nomes de fonte
  // de addon aparecia como caixa.
  {
    TTF_Font *fl = fonteDe(familia, estilo, s), *a1, *a2;
    altDe(familia, estilo, fl, &a1, &a2);
    s = semDecorativoSemGlifo(fl, a1, a2, s, limpo, sizeof limpo);
    if (!*s) return vazia;
  }

  char chave[288];
  // A VARIANTE CJK entra na chave: o mesmo hanzi tem forma de japones, de chines
  // simplificado e de tradicional (ver Escrita), e trocar o idioma da interface
  // nao pode devolver a textura da lingua anterior.
  if (camada)
    snprintf(chave, sizeof chave, "E%d:%d:%d:%d:%d|%02x%02x%02x|%.226s", (int)(escCam * 100.0f + 0.5f),
             (int)familia, (int)estilo, enfase & 3, (int)variacaoCjk(), r & 255, g & 255, b & 255, s);
  else
    snprintf(chave, sizeof chave, "%d:%d:%d:%d|%02x%02x%02x|%.232s", (int)familia,
             (int)estilo, enfase & 3, (int)variacaoCjk(), r & 255, g & 255, b & 255, s);

  // Hash da chave para evitar o strcmp em quase todas as entradas: a busca
  // roda para CADA linha de CADA quadro, e comparar 288 bytes centenas de
  // vezes por quadro custa mais que o desenho.
  unsigned long h = 2166136261UL;
  { const char *p = chave;
    for (; *p; p++) { h ^= (unsigned char)*p; h *= 16777619UL; } }

  // Sondagem a partir de h % MAX_LINHAS, e nao varredura das 256 entradas.
  // Esta busca roda para CADA linha de CADA quadro; a varredura completa
  // custava em media 128 comparacoes por acerto. Sondando do ponto do hash o
  // acerto sai nas primeiras casas, e a busca PARA no primeiro slot vazio:
  // quem foi inserido por esta mesma regra nunca esta depois de um buraco.
  //
  // O despejo LRU pode abrir um buraco no meio de uma corrente antiga; o
  // efeito e no maximo uma rerasterizacao daquela linha (que entra de novo
  // mais perto do hash), nunca resultado errado — a chave e conferida por
  // strcmp de qualquer forma.
  int livre = -1;
  for (int k = 0; k < MAX_LINHAS; k++) {
    int i = (int)((h + (unsigned long)k) % MAX_LINHAS);
    if (!cache[i].ocupado) { livre = i; break; }
    if (cache[i].hash == h && strcmp(cache[i].chave, chave) == 0) {
      cache[i].uso = ++relogio;
      cache[i].quadroUso = quadroTxt;
      return cache[i].linha;
    }
  }

  // Orcamento estourado: devolve vazio e tenta de novo no proximo quadro. A
  // linha aparece com um quadro de atraso em vez de travar o atual.
  if (rastNesteQuadro >= TXT_POR_QUADRO &&
      (rastNesteQuadro >= TXT_MAX_QUADRO || txt_ms - msIniQuadro >= TXT_MS_QUADRO)) {
    txt_pendentes++;
    return vazia;
  }
  rastNesteQuadro++;
  int slot = livre;
  if (slot < 0) {
    // Tabela cheia: so agora vale a varredura completa atras do LRU. Isso
    // acontece no maximo TXT_POR_QUADRO vezes por quadro, nao por linha.
    unsigned long menor = ~0UL;
    for (int i = 0; i < MAX_LINHAS; i++)
      if (cache[i].ocupado && cache[i].quadroUso != quadroTxt &&
          cache[i].uso < menor) {
        menor = cache[i].uso;
        slot = i;
      }
  }
  if (slot < 0) { txt_pendentes++; return vazia; }
  if (cache[slot].ocupado) txt_despejos++;
  if (cache[slot].ocupado && cache[slot].linha.tex) {
    // avisa o gfx: o nome pode ser reutilizado pelo glGenTextures logo abaixo
    gfx_tex_esquecer(cache[slot].linha.tex);
    glDeleteTextures(1, &cache[slot].linha.tex);
  }

  Uint64 t0 = SDL_GetPerformanceCounter();
  SDL_Color cor = { (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a };
  TTF_Font *fonte = fonteLegendaDe(estilo, s, familia), *alt1, *alt2;
  if (!fonte) return vazia;
  altDe(familia, estilo, fonte, &alt1, &alt2);
  char vis[1600];
  const char *v = visualDe(fonte, s, vis, sizeof vis);
  // SOMA ao estilo que a fonte ja tem, e RESTAURA depois. As familias de
  // reserva nascem com TTF_STYLE_BOLD ligado (ver txt_iniciar); zerar aqui
  // tiraria delas o peso que o app inteiro conta com.
  int estiloAnt, novo = 0;
  arabeCorpo = (int)(ESTILOS[estilo].corpo * ESC_T + 0.5f);
  if (enfase && bidi_tem_arabe(v) && !txtTemLatim(v) && (enfase & TXT_ENF_NEGRITO)) {   // #335
    TTF_Font *bf = arabeBoldFace();
    if (bf) fonte = bf;               // a face Bold de verdade
    enfase &= ~TXT_ENF_NEGRITO;       // nunca o sintetico sobre arabe
  }
  if (enfase) fonte = fonteComEnfase(familia, estilo, fonte, enfase, &novo);
  estiloAnt = TTF_GetFontStyle(fonte);
  if (enfase && novo != estiloAnt) TTF_SetFontStyle(fonte, novo);
  SDL_Surface *sf = renderCorridas(fonte, alt1, alt2, v, cor, enfaseTtf(enfase));
  if (enfase && novo != estiloAnt) TTF_SetFontStyle(fonte, estiloAnt);
  if (!sf) return vazia;
  SDL_Surface *cv = SDL_ConvertSurfaceFormat(sf, SDL_PIXELFORMAT_ABGR8888, 0);
  SDL_FreeSurface(sf);
  if (!cv) return vazia;

  GLuint t; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cv->w, cv->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, cv->pixels);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gfx_tex_esquecer(0);  // o bind do upload passou por fora do gfx_rect

  cache[slot].ocupado = 1;
  cache[slot].hash = h;
  strncpy(cache[slot].chave, chave, sizeof cache[slot].chave - 1);
  // Medida em unidades de LAYOUT, nao em pixeis do buffer.
  cache[slot].linha.tex = t;
  cache[slot].linha.w = (int)(cv->w / ESC_T + 0.5f);
  cache[slot].linha.h = (int)(cv->h / ESC_T + 0.5f);
  // Tamanho da textura, so na camada ampliada: la o quad e desenhado no pixel
  // exato do glifo (txt_desenhar_alpha), e nao em w*s arredondado.
  cache[slot].linha.pw = camada ? cv->w : 0;
  cache[slot].linha.ph = camada ? cv->h : 0;
  txt_rasterizadas++;
  txt_ms += (double)(SDL_GetPerformanceCounter() - t0) * 1000.0 / (double)SDL_GetPerformanceFrequency();
  cache[slot].uso = ++relogio;
  cache[slot].quadroUso = quadroTxt;
  SDL_FreeSurface(cv);
  return cache[slot].linha;
}

// LARGURA SEM RASTERIZAR. A quebra de linha (txt_bloco) e o corte com
// reticencias (txt_linha_corta) mediam cada tentativa com txt_linha, e txt_linha
// RASTERIZA E GUARDA a linha: uma sinopse de 60 palavras rasterizava ~60
// prefixos ("A", "A vida", "A vida de"...) so para descobrir onde quebrar, e
// cada um custa ~2,4 ms na C9 (medido para uma linha). Alem de caro, enchia o
// cache com linhas que ninguem desenha. Pior: estourado o orcamento do quadro
// a medida voltava 0, o texto "cabia" em uma linha so e a quebra saia errada
// ate o quadro em que tudo cabia — as palavras entravam aos poucos, que e o
// defeito do #172. Aqui e so TTF_SizeUTF8: sem textura, sem orcamento.
//
// Devolve 0 onde linhaFamilia devolveria vazia (fonte ausente, string vazia).
// HIPOTESE nao medida na TV: TTF_SizeUTF8 e a rotina que o TTF_RenderUTF8_Blended
// usa para dimensionar a superficie, entao as larguras coincidem;
// tests/text_largura.sh confere isso no Mac.
static int larguraLinhaMedir(TxtEstilo estilo, const char *s, TxtFamilia familia,
                             int enfase);
static int larguraLinha(TxtEstilo estilo, const char *s, TxtFamilia familia,
                        int enfase) {
  unsigned long long h = 1469598103934665603ull;
  unsigned n = 0, slot;
  unsigned char var;
  int w;
  camadaAtualizar();
  if (familia < TXT_FAMILIA_INTER || familia >= TXT_FAMILIA_N)
    familia = TXT_FAMILIA_INTER;
  if (!s || !*s || estilo < 0 || estilo >= TXT_NFONTES) return 0;
  { const unsigned char *p = (const unsigned char *)s;
    for (; *p; p++, n++) { h ^= *p; h *= 1099511628211ull; } }
  // QUATRO CASAS POR CHAVE, a menos usada sai. Era mapeamento direto: duas
  // linhas vivas na MESMA casa se expulsavam a cada quadro e as duas iam ao
  // HarfBuzz de novo, para sempre. MEDIDO no Mac (06/10/2026): na home, a
  // sinopse do destaque ("A science teacher wakes" e a frase inteira, casa
  // 3379) — milhares de medidas a cada 2 s; nos Ajustes, 26% do desenho em
  // larguraLinha -> TTF_SizeUTF8, sem nenhuma limpeza da tabela no meio.
  { unsigned base = (unsigned)(h % (LG_MEM / LG_VIAS)) * LG_VIAS, k, velha = base;
    var = (unsigned char)variacaoCjk();
    for (k = base; k < base + LG_VIAS; k++) {
      if (lgMem[k].ok && lgMem[k].h == h && lgMem[k].n == n &&
          lgMem[k].fam == familia && lgMem[k].estilo == estilo &&
          lgMem[k].enf == (unsigned char)enfase && lgMem[k].var == var &&
          lgMem[k].cam == (unsigned char)camada) {
        lgMem[k].uso = ++lgRelogio;
        return lgMem[k].w;
      }
      if (!lgMem[k].ok) { if (lgMem[velha].ok) velha = k; }
      else if (lgMem[velha].ok && lgMem[k].uso < lgMem[velha].uso) velha = k;
    }
    slot = velha; }
  w = larguraLinhaMedir(estilo, s, familia, enfase);
  lgMem[slot].h = h; lgMem[slot].n = n; lgMem[slot].fam = (unsigned char)familia;
  lgMem[slot].estilo = (unsigned char)estilo; lgMem[slot].enf = (unsigned char)enfase;
  lgMem[slot].var = var; lgMem[slot].w = w; lgMem[slot].ok = 1;
  lgMem[slot].uso = ++lgRelogio;
  lgMem[slot].cam = (unsigned char)camada;
  return w;
}

static int larguraLinhaMedir(TxtEstilo estilo, const char *s, TxtFamilia familia,
                             int enfase) {
  char limpo[1024], vis[1600];
  TTF_Font *fl = fonteDe(familia, estilo, s), *a1, *a2;
  if (!fl) return 0;
  {
    altDe(familia, estilo, fl, &a1, &a2);
    s = semDecorativoSemGlifo(fl, a1, a2, s, limpo, sizeof limpo);
    if (!*s) return 0;
  }
  TTF_Font *fonte = fonteLegendaDe(estilo, s, familia);
  if (!fonte) return 0;
  altDe(familia, estilo, fonte, &a1, &a2);
  const char *v = visualDe(fonte, s, vis, sizeof vis);
  int estiloAnt, novo = 0;
  arabeCorpo = (int)(ESTILOS[estilo].corpo * ESC_T + 0.5f);
  if (enfase && bidi_tem_arabe(v) && !txtTemLatim(v) && (enfase & TXT_ENF_NEGRITO)) {   // #335
    TTF_Font *bf = arabeBoldFace();
    if (bf) fonte = bf;               // a face Bold de verdade
    enfase &= ~TXT_ENF_NEGRITO;       // nunca o sintetico sobre arabe
  }
  if (enfase) fonte = fonteComEnfase(familia, estilo, fonte, enfase, &novo);
  estiloAnt = TTF_GetFontStyle(fonte);
  if (enfase && novo != estiloAnt) TTF_SetFontStyle(fonte, novo);
  int w = 0;
  int ok = medirCorridas(fonte, a1, a2, v, enfaseTtf(enfase), &w);
  if (enfase && novo != estiloAnt) TTF_SetFontStyle(fonte, estiloAnt);
  if (ok != 0) return 0;
  return (int)(w / ESC_T + 0.5f);
}

int txt_largura(TxtEstilo estilo, const char *s) {
  // Consultas de layout antes de txt_iniciar nao devem marcar fontes como
  // ausentes para sempre nem guardar uma largura zero no cache.
  if (!TTF_WasInit()) return 0;
  return larguraLinha(estilo, i18n(s), fonteInterface, 0);
}

TxtLinha txt_linha(TxtEstilo estilo, const char *s, int r, int g, int b, int a) {
  return linhaFamilia(estilo, i18n(s), r, g, b, a, fonteInterface, 0);
}

TxtLinha txt_linha_familia(TxtEstilo estilo, const char *s, int r, int g,
                           int b, int a, TxtFamilia familia) {
  return linhaFamilia(estilo, i18n(s), r, g, b, a, familia, 0);
}

void txt_desenhar(TxtLinha l, float x, float y) { txt_desenhar_alpha(l, x, y, 1.0f); }

// ENCAIXE NO PIXEL DA TELA.
//
// A textura do glifo tem exatamente a resolucao em que vai ser desenhada, mas
// o CANTO caia em coordenada fracionaria o tempo todo: centralizacao
// (`(r.h - l.h) * 0.5f`), pilhas ancoradas na base, molas de rolagem. Com o
// canto em 478.4 o GL_LINEAR amostra ENTRE dois texels e cada letra sai
// espalhada por duas colunas de pixel — o texto inteiro fica meio pixel fora
// de foco, em toda a tela, o tempo todo.
//
// Era isso que restava do "borrao" depois de o 4K se provar impossivel: nao
// falta resolucao, falta o texto cair em cima do pixel. O web nao tem esse
// problema porque o navegador ja posiciona glifo na grade do dispositivo.
//
// O arredondamento e feito na grade do DRAWABLE e nao na de layout: no Mac
// retina meio pixel de layout e um pixel de tela inteiro, e arredondar na
// grade errada jogaria o texto fora do lugar em vez de assenta-lo.
//
// So o TEXTO encaixa. Encaixar cartao e arte transformaria as molas em degraus
// visiveis; o glifo nao sofre disso porque a letra em si nao se deforma, ela
// so anda de um pixel para o outro.
static float encaixa(float v) {
  float e = ESC_T;
  return (float)((int)(v * e + (v < 0.0f ? -0.5f : 0.5f))) / e;
}

void txt_desenhar_alpha(TxtLinha l, float x, float y, float alpha) {
  if (!l.tex) return;
  camadaAtualizar();
  GfxRect r = { encaixa(x), encaixa(y), (float)l.w, (float)l.h };
  if (camada && l.pw > 0) { r.w = (float)l.pw / ESC_T; r.h = (float)l.ph / ESC_T; }
  gfx_rect(r, l.tex, GFX_TEXTO, 0, 0, 0, 0.0f, 1, 1, 1, alpha);
}

float txt_tracking(TxtEstilo estilo, const char *s, int r, int g, int b,
                   float x, float y, float alpha, float tracking) {
  if (!s || !*s) return 0.0f;
  float larg = 0.0f;
  // Percorre por CARACTERE UTF-8, nao por byte: cortar no meio de um acento
  // produz um glifo invalido, e a fonte da LG devolve um retangulo vazio.
  for (const unsigned char *p = (const unsigned char *)s; *p; ) {
    int n = 1;
    if      ((*p & 0xF8) == 0xF0) n = 4;
    else if ((*p & 0xF0) == 0xE0) n = 3;
    else if ((*p & 0xE0) == 0xC0) n = 2;
    char c[5]; int k = 0;
    while (k < n && p[k]) { c[k] = (char)p[k]; k++; }
    c[k] = 0; p += k ? k : 1;

    TxtLinha l = txt_linha(estilo, c, r, g, b, 255);
    if (x >= 0.0f && l.w) txt_desenhar_alpha(l, x + larg, y, alpha);
    larg += l.w + tracking;
  }
  return larg > 0.0f ? larg - tracking : 0.0f;
}

// Declarada em text.h desde o inicio e NUNCA implementada. Ninguem chamava,
// entao o link passava; a primeira chamada derrubou o build ARM com
// "undefined reference". No Mac isso NAO aparece: `cc -fsyntax-only` num
// arquivo solto nao linka nada.
TxtLinha txt_linha_corta(TxtEstilo estilo, const char *s, int r, int g, int b,
                         int a, float maxW) {
  return txt_linha_corta_familia(estilo, s, r, g, b, a, maxW,
                                 fonteInterface);
}

static TxtLinha cortaFamilia(TxtEstilo estilo, const char *s, int r, int g,
                             int b, int a, float maxW, TxtFamilia familia,
                             int enfase) {
  // Traduzir ANTES de cortar: o corte mede a largura e insere as reticencias,
  // e medir o portugues para desenhar o ingles poe as reticencias no lugar
  // errado — ou corta um texto que caberia inteiro.
  s = i18n(s);
  // Mede sem rasterizar: so a linha FINAL vira textura (ver larguraLinha).
  if (!s || !*s || (float)larguraLinha(estilo, s, familia, enfase) <= maxW)
    return linhaFamilia(estilo, s, r, g, b, a, familia, enfase);
  char buf[512];
  size_t n = strlen(s);
  if (n >= sizeof buf - 4) n = sizeof buf - 4;
  memcpy(buf, s, n); buf[n] = 0;
  // Corta por PALAVRA enquanto houver espaco; so quando sobra uma palavra so e
  // que se corta no meio dela. Cortar sempre por caractere deixa meia palavra
  // antes das reticencias, e isso se le como texto corrompido, nao como corte.
  while (n > 0) {
    size_t corte = n;
    while (corte > 0 && buf[corte - 1] != ' ') corte--;
    if (corte > 1) n = corte - 1; else n--;
    // nunca parar no meio de um caractere UTF-8: meio caractere vira tofu
    while (n > 0 && ((unsigned char)buf[n] & 0xC0) == 0x80) n--;
    buf[n] = 0;
    if (!n) break;
    char t[520];
    snprintf(t, sizeof t, "%s\xe2\x80\xa6", buf);
    if ((float)larguraLinha(estilo, t, familia, enfase) <= maxW)
      return linhaFamilia(estilo, t, r, g, b, a, familia, enfase);
  }
  return linhaFamilia(estilo, "\xe2\x80\xa6", r, g, b, a, familia, enfase);
}

TxtLinha txt_linha_corta_familia(TxtEstilo estilo, const char *s, int r, int g,
                                 int b, int a, float maxW,
                                 TxtFamilia familia) {
  return cortaFamilia(estilo, s, r, g, b, a, maxW, familia, 0);
}

TxtLinha txt_linha_corta_enfase(TxtEstilo estilo, const char *s, int r, int g,
                                int b, int a, float maxW, TxtFamilia familia,
                                int enfase) {
  return cortaFamilia(estilo, s, r, g, b, a, maxW, familia, enfase);
}

static void desenhaBlocoLinha(TxtEstilo estilo, const char *s, int r, int g, int b,
                              float x, float y, float larg, float alpha,
                              int reticencias) {
  if (alpha <= 0.0f) return; // medida de bloco sem rasterizar
  if (!reticencias) {
    TxtLinha l = txt_linha(estilo, s, r, g, b, 255);
    txt_desenhar_alpha(l, x, y, alpha);
    return;
  }
  {
    char fim[512];
    size_t n = strlen(s);
    if (n >= sizeof fim - 4) {
      n = sizeof fim - 4;
      while (n > 0 && ((unsigned char)s[n] & 0xC0) == 0x80) n--;
    }
    memcpy(fim, s, n); fim[n] = 0;
    for (;;) {
      char teste[512];
      snprintf(teste, sizeof teste, "%s\xe2\x80\xa6", fim);
      if (txt_largura(estilo, teste) <= larg) {
        txt_desenhar_alpha(txt_linha(estilo, teste, r, g, b, 255), x, y, alpha);
        return;
      }
      // Remove palavras completas primeiro; uma unica palavra longa cai para
      // codepoints UTF-8, para nunca deixar um acento pela metade.
      size_t corte = n;
      while (corte > 0 && fim[corte - 1] != ' ') corte--;
      if (corte > 0) n = corte - 1;
      else {
        if (!n) break;
        n--;
        while (n > 0 && ((unsigned char)fim[n] & 0xC0) == 0x80) n--;
      }
      fim[n] = 0;
      if (!n) break;
    }
    { TxtLinha l = txt_linha(estilo, "\xe2\x80\xa6", r, g, b, 255);
      txt_desenhar_alpha(l, x, y, alpha); }
  }
}

// QUEBRA DE LINHA EM ESCRITA SEM ESPACO. Japones e chines nao separam palavras
// por espaco, entao um paragrafo inteiro era UMA "palavra" para as quebras deste
// arquivo (e de agendaui.c) e saia por cima da borda da coluna. Aqui o
// paragrafo e cortado em TOKENS: uma palavra de escrita com espaco (latim,
// cirilico, grego, hangul) continua sendo a corrida ate o espaco, e cada
// caractere CJK e um token so — com duas regras de tipografia que valem nas duas
// linguas (kinsoku): pontuacao de FECHAR ("、。）」") nunca abre linha, entao
// gruda no token anterior; pontuacao de ABRIR ("（「") nunca fecha linha, entao
// leva o caractere seguinte junto.
// Quem junta os tokens NAO poe espaco entre dois que vieram colados no texto.
static int cjkLivre(Uint32 cp) {
  return (cp >= 0x2E80 && cp <= 0x9FFF) || (cp >= 0xFF00 && cp <= 0xFFEF);
}
static int naoComecaLinha(Uint32 cp) {
  switch (cp) {
    case 0x3001: case 0x3002: case 0xFF0C: case 0xFF0E: case 0x30FB: case 0xFF1A:
    case 0xFF1B: case 0xFF1F: case 0xFF01: case 0x30FC: case 0x3005: case 0x3009:
    case 0x300B: case 0x300D: case 0x300F: case 0x3011: case 0x3015: case 0xFF09:
    case 0xFF3D: case 0xFF5D: case 0xFF5E: case 0xFF65: return 1;
    default: return 0;
  }
}
static int abreCjk(Uint32 cp) {
  switch (cp) {
    case 0x3008: case 0x300A: case 0x300C: case 0x300E: case 0x3010: case 0x3014:
    case 0xFF08: case 0xFF3B: case 0xFF5B: return 1;
    default: return 0;
  }
}
size_t txt_token_tam(const char *s) {
  const unsigned char *q = (const unsigned char *)s;
  int n = 1;
  Uint32 cp;
  if (!s || !*q || *q == ' ' || *q == '\n') return 0;
  cp = decodifica(q, &n);
  if (cjkLivre(cp) && !naoComecaLinha(cp)) {
    q += n;
    if (abreCjk(cp) && *q && *q != ' ' && *q != '\n') { decodifica(q, &n); q += n; }
    while (*q && *q != ' ' && *q != '\n') {
      cp = decodifica(q, &n);
      if (!naoComecaLinha(cp)) break;
      q += n;
    }
    return (size_t)((const char *)q - s);
  }
  for (;;) {
    q += n;
    if (!*q || *q == ' ' || *q == '\n') break;
    cp = decodifica(q, &n);
    if (cjkLivre(cp) && !naoComecaLinha(cp)) break;
  }
  return (size_t)((const char *)q - s);
}

static float txt_bloco_impl(TxtEstilo estilo, const char *s, int r, int g, int b,
                            float x, float y, float larg, float leading,
                            float alpha, int maxLinhas, int reticencias) {
  // Mesma razao do corte: a quebra de linha e feita no texto final.
  s = i18n(s);
  if (!s || !*s) return 0.0f;
  char linha[512]; linha[0] = 0;
  float usado = 0.0f;
  int nLinhas = 0;
  const char *p = s;
  int espacoAntes = 1;     // o token anterior foi separado deste por espaco? (ver txt_token_tam)
  while (*p && (maxLinhas <= 0 || nLinhas < maxLinhas)) {
    // pega o proximo token (palavra, ou um caractere CJK)
    const char *ini = p;
    int quebra, espaco;
    p += txt_token_tam(p);
    size_t np = (size_t)(p - ini);
    // QUEBRA DURA NO \n, e isto e conserto de defeito visto em foto.
    //
    // Este laco separava palavras SO por espaco. Um \n no meio do texto virava
    // parte da "palavra", chegava inteiro ao TTF_RenderUTF8_Blended e saia como
    // .notdef — o quadradinho. Quem escreveu o rodape de ajuda dos Ajustes
    // ("Navegar\nOK Abrir\nVoltar Ir para as categorias") via tres linhas no
    // codigo e uma linha corrida com dois quadrados na TV. O relator do issue
    // #12 fotografou exatamente isso e eu li a foto como texto sem traducao,
    // que era outra coisa: sao dois defeitos na mesma tela.
    quebra = (*p == '\n');
    espaco = (*p == ' ' || *p == '\n');
    while (*p == ' ' || *p == '\n') { if (*p == '\n') quebra = 1; p++; }

    char tentativa[512];
    size_t nl = strlen(linha);
    if (nl + np + 2 >= sizeof tentativa) {
      if (reticencias) {
        desenhaBlocoLinha(estilo, linha[0] ? linha : ini, r, g, b,
                          x, y + usado, larg, alpha, 1);
        return usado + leading;
      }
      break;
    }
    memcpy(tentativa, linha, nl);
    if (nl && espacoAntes) tentativa[nl++] = ' ';
    memcpy(tentativa + nl, ini, np);
    tentativa[nl + np] = 0;

    int mw = txt_largura(estilo, tentativa);
    if (mw > larg && linha[0]) {
      if (reticencias && maxLinhas > 0 && nLinhas + 1 >= maxLinhas) {
        desenhaBlocoLinha(estilo, linha, r, g, b, x, y + usado, larg,
                          alpha, 1);
        return usado + leading;
      }
      // nao coube: fecha a linha atual e recomeca com a palavra
      if (alpha > 0.0f) {
        TxtLinha l = txt_linha(estilo, linha, r, g, b, 255);
        txt_desenhar_alpha(l, x, y + usado, alpha);
      }
      usado += leading; nLinhas++;
      if (maxLinhas > 0 && nLinhas >= maxLinhas) return usado;
      memcpy(linha, ini, np); linha[np] = 0;
      // Separador "·" sozinho no comeco da linha nova e sobra da linha de
      // cima ("... RELEASE ·" / "· NETFLIX"): quem separa ja ficou la. Some.
      if (np == 2 && (unsigned char)ini[0] == 0xC2 && (unsigned char)ini[1] == 0xB7)
        linha[0] = 0;
      if (reticencias && txt_largura(estilo, linha) > larg) {
        desenhaBlocoLinha(estilo, linha, r, g, b, x, y + usado, larg,
                          alpha, 1);
        return usado + leading;
      }
    } else if (mw > larg && reticencias) {
      // Uma palavra sem espacos pode ser maior que a coluna. O caminho normal
      // de txt_bloco preserva o comportamento antigo; esta variante sinaliza
      // o corte e limita o glifo por largura, inclusive em texto UTF-8 longo.
      desenhaBlocoLinha(estilo, ini, r, g, b, x, y + usado, larg, alpha, 1);
      return usado + leading;
    } else {
      memcpy(linha, tentativa, nl + np + 1);
    }
    // Fecha a linha AQUI quando o TEXTO pediu, em vez de esperar a largura
    // acabar. `linha` pode estar vazia (dois \n seguidos): ai a linha em branco
    // e desenhada de proposito — e o vao que quem escreveu o texto pediu.
    if (quebra && (maxLinhas <= 0 || nLinhas < maxLinhas)) {
      if (reticencias && maxLinhas > 0 && nLinhas + 1 >= maxLinhas && *p) {
        desenhaBlocoLinha(estilo, linha, r, g, b, x, y + usado, larg,
                          alpha, 1);
        return usado + leading;
      }
      if (linha[0] && alpha > 0.0f) {
        TxtLinha l = txt_linha(estilo, linha, r, g, b, 255);
        txt_desenhar_alpha(l, x, y + usado, alpha);
      }
      usado += leading; nLinhas++;
      linha[0] = 0;
    }
    espacoAntes = espaco;
  }
  if (linha[0] && (maxLinhas <= 0 || nLinhas < maxLinhas)) {
    if (reticencias && maxLinhas > 0 && *p)
      desenhaBlocoLinha(estilo, linha, r, g, b, x, y + usado, larg,
                        alpha, 1);
    else {
      if (alpha > 0.0f) {
        TxtLinha l = txt_linha(estilo, linha, r, g, b, 255);
        txt_desenhar_alpha(l, x, y + usado, alpha);
      }
    }
    usado += leading;
  }
  return usado;
}

float txt_bloco(TxtEstilo estilo, const char *s, int r, int g, int b,
                float x, float y, float larg, float leading, float alpha,
                int maxLinhas) {
  return txt_bloco_impl(estilo, s, r, g, b, x, y, larg, leading, alpha,
                        maxLinhas, 0);
}

float txt_bloco_corta(TxtEstilo estilo, const char *s, int r, int g, int b,
                      float x, float y, float larg, float leading,
                      float alpha, int maxLinhas) {
  return txt_bloco_impl(estilo, s, r, g, b, x, y, larg, leading, alpha,
                        maxLinhas, 1);
}

// Quebra igual a txt_bloco, mas posiciona cada linha pela BORDA DIREITA. A
// duplicacao com txt_bloco e pequena e proposital: unificar as duas exigiria um
// parametro de alinhamento em todas as chamadas, e so este caso precisa.
float txt_bloco_dir(TxtEstilo estilo, const char *s, int r, int g, int b,
                    float xDir, float y, float larg, float leading,
                    float alpha, int maxLinhas) {
  // Mesma razao do corte: a quebra de linha e feita no texto final.
  s = i18n(s);
  if (!s || !*s) return 0.0f;
  char linha[512]; linha[0] = 0;
  float usado = 0.0f;
  int nLinhas = 0;
  const char *p = s;
  int espacoAntes = 1;
  while (*p && (maxLinhas <= 0 || nLinhas < maxLinhas)) {
    const char *ini = p;
    int quebra, espaco;
    p += txt_token_tam(p);
    size_t np = (size_t)(p - ini);
    // QUEBRA DURA NO \n, e isto e conserto de defeito visto em foto.
    //
    // Este laco separava palavras SO por espaco. Um \n no meio do texto virava
    // parte da "palavra", chegava inteiro ao TTF_RenderUTF8_Blended e saia como
    // .notdef — o quadradinho. Quem escreveu o rodape de ajuda dos Ajustes
    // ("Navegar\nOK Abrir\nVoltar Ir para as categorias") via tres linhas no
    // codigo e uma linha corrida com dois quadrados na TV. O relator do issue
    // #12 fotografou exatamente isso e eu li a foto como texto sem traducao,
    // que era outra coisa: sao dois defeitos na mesma tela.
    quebra = (*p == '\n');
    espaco = (*p == ' ' || *p == '\n');
    while (*p == ' ' || *p == '\n') { if (*p == '\n') quebra = 1; p++; }

    char tentativa[512];
    size_t nl = strlen(linha);
    if (nl + np + 2 >= sizeof tentativa) break;
    memcpy(tentativa, linha, nl);
    if (nl && espacoAntes) tentativa[nl++] = ' ';
    memcpy(tentativa + nl, ini, np);
    tentativa[nl + np] = 0;

    int mw = txt_largura(estilo, tentativa);
    if (mw > larg && linha[0]) {
      TxtLinha l = txt_linha(estilo, linha, r, g, b, 255);
      if (xDir >= 0.0f) txt_desenhar_alpha(l, xDir - l.w, y + usado, alpha);
      usado += leading; nLinhas++;
      if (maxLinhas > 0 && nLinhas >= maxLinhas) return usado;
      memcpy(linha, ini, np); linha[np] = 0;
    } else {
      memcpy(linha, tentativa, nl + np + 1);
    }
    // Fecha a linha AQUI quando o TEXTO pediu, em vez de esperar a largura
    // acabar. `linha` pode estar vazia (dois \n seguidos): ai a linha em branco
    // e desenhada de proposito — e o vao que quem escreveu o texto pediu.
    if (quebra && (maxLinhas <= 0 || nLinhas < maxLinhas)) {
      if (linha[0]) {
        TxtLinha l = txt_linha(estilo, linha, r, g, b, 255);
        if (xDir >= 0.0f) txt_desenhar_alpha(l, xDir - l.w, y + usado, alpha);
      }
      usado += leading; nLinhas++;
      linha[0] = 0;
    }
    espacoAntes = espaco;
  }
  if (linha[0] && (maxLinhas <= 0 || nLinhas < maxLinhas)) {
    TxtLinha l = txt_linha(estilo, linha, r, g, b, 255);
    if (xDir >= 0.0f) txt_desenhar_alpha(l, xDir - l.w, y + usado, alpha);
    usado += leading;
  }
  return usado;
}
