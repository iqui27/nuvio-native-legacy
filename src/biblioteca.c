// Biblioteca, alinhada com a tela do app web (MEDIDA rodando, perfil do dono).
//
// ------------------------------------------------------------------------
// O QUE MUDOU, E POR QUE
//
// O port tinha tres pilulas CENTRALIZADAS ("Minha Lista" / "Comprados" /
// "Gêneros") e uma grade de 6 colunas de 212. Medida a tela do web, a estrutura
// e outra e tem QUATRO faixas, todas alinhadas a esquerda em x=96:
//
//   .library-page-title    "Biblioteca" 56/600, letter-spacing 1, em (96,48)
//   .library-page-source   selo "NUVIO" 28/500 rgb(128,128,128) ls 4, a DIREITA
//   .library-view-mode-row y=136: abas, a ativa em destaque e as outras so texto
//   .library-picker-row    y=212: pilulas compactas em linha, 56 de altura, com
//                          rotulo e valor na mesma linha
//   .library-grid          6 colunas de 268 (auto-fill com minimo 252 sobre os
//                          1728 uteis, gutter 24), poster 2:3 = 268x402 raio 24
//                          com borda de 4px POR DENTRO, titulo 32/500 a 16 do
//                          poster; passo de linha 487.8
//
// ------------------------------------------------------------------------
// A RENOVADA DE 16/09/2026 — pedido do dono, palavra por palavra: "manda um
// subagent pra biblioteca dar uma renovada, colocar a opcao de ver listas do
// traktv, do simkl, do nuvio, ter a opcao de ver em card ou em lista. e tb
// procurar nas listas publicas do traktv. com a opcao de fixar ela na
// biblioteca ou adicionar a home".
//
// TRES COISAS ENTRARAM, e nenhuma delas e uma tela nova:
//
//   1. UM TERCEIRO MODO, "Listas". Salvos e Coleção continuam mostrando
//      TITULOS; Listas mostra CONJUNTOS — as listas do Trakt, os cinco estados
//      do Simkl, as pastas de colecao da conta Nuvio, as listas publicas do
//      Trakt e o que ja foi fixado aqui. Abrir uma delas troca a grade pelos
//      itens dela, na MESMA tela: empurrar isso para uma tela nova custaria uma
//      entrada em app.c e nao ganharia nada que um breadcrumb nao resolva.
//
//   2. EXIBIÇÃO: cartaz ou lista. E o terceiro seletor, e vale para as tres
//      grades (titulos, listas e itens de uma lista). A escolha e LOCAL e POR
//      PERFIL (bibliotecaui-p<N>.txt), pela mesma razao escrita no topo de
//      fileiras.h: a TV nao tem como empurrar isto de volta para a conta sem
//      arriscar apagar a configuracao dos outros aparelhos.
//
//      MEMORIA, que aqui nao e detalhe: a lista pede a arte por
//      tex_obter_larg(url, 64) e nao por tex_obter. O teto de decodificacao sai
//      da largura desenhada (ver tex_cache.h), entao a miniatura de 64 px custa
//      uma fracao do cartaz de 268 — e so as linhas VISIVEIS pedem textura,
//      porque o laco de desenho ja recusa o que esta fora da tela. Uma lista
//      que mantivesse todos os cartazes vivos seria regressao no orcamento de
//      96 MB do tex_cache, que o proprio relato do dono ja satura.
//
//   3. FIXAR e ADICIONAR À HOME, dentro de uma lista aberta. Fixar guarda a
//      lista no arquivo do perfil (src/listas.c). Adicionar a home NAO inventa
//      mecanismo: a lista do Trakt vira uma pasta de colecao com fonte
//      `prov="trakt"` + `traktLista`, que e exatamente o que o editor do site
//      grava e o que descoberta.c ja busca em /lists/<id>/items. A pasta Nuvio
//      ja tem fileira propria: o que muda e o liga/desliga dela em fileiras.c.
//
// Duas decisoes que vieram de erros ja cometidos em outras telas deste app:
//
//   1. A pilula ESCOLHIDA continua marcada quando o foco desce para a grade. Foi
//      o mesmo problema das abas de temporada do detalhe: sem o estado de
//      escolha separado do foco, o usuario perde de vista onde esta.
//   2. A rolagem move o MINIMO para a linha focada caber. Alinhar a linha focada
//      ao topo empurra o cabecalho para fora da tela na primeira descida.
#include "menu.h"
#include "biblioteca.h"
#include "botoes.h"
#include "teclavoltar.h"
#include "jfid.h"
#include "posterprov.h"
#include "contalib.h"
#include "salvos.h"
#include "artemetahub.h"
#include "extras.h"
#include "detail.h"
#include "badges.h"
#include "listas.h"
#include "listasescrita.h"
#include "celbotao.h"
#include "ilha.h"
#include "teclado.h"
#include "trakt.h"
#include "simkl.h"
#include "gfx.h"
#include "text.h"
#include "sinopse.h"
#include "tex_cache.h"
#include "focus.h"
#include "anim.h"
#include "movimento.h"
#include "revela.h"
#include "layout.h"
#include "segurar.h"
#include "ajustes.h"
#include "nlanc.h"
#include "ctxmenu.h"
#include "tela21.h"
#define NV_TXT_TELA   // NV_TELA_W/H = tela virtual da escala de texto (escala.h)
#include "escala.h"
#include "telefoneui.h"
#include "ponteiro.h"
#include "sync.h"
#include "descoberta.h"
#include "addons.h"
#include "servidores.h"

// Tinta do texto sobre o foco: em vidro o foco e so contorno sobre superficie
// escura, entao o texto fica CLARO mesmo com realce branco (que pede escuro).
// GLASS UI (dono, 02/10, mockup "ilha" tela 9): o foco de LINHA e superficie
// clara nos dois materiais, entao o texto da linha focada e sempre claro (255 /
// 205). So o foco de BOTAO (chips, abas) e pilula cheia no acento, e la a tinta
// e a de ajustes_tinta_foco (tintaBotao).
static int bibTinta(void) { return 255; }
static int bibTinta2(void) { return 205; }
#include "catalogo.h"
#include "dados.h"
#include "perfis.h"
#include "idioma.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <time.h>

// Fileiras de foco: 0 = modos (ou as acoes, dentro de uma lista aberta),
// 1 = seletores, 2.. = grade. Dentro de uma lista aberta a grade comeca em 1 —
// nao ha seletores ali, as acoes ocupam a faixa de cima.
#define BIB_FIL_MODO   0
#define BIB_FIL_PICK   1
#define BIB_FIL_GRADE  2
#define BIB_MAX_LINHAS (FOCUS_MAX_FILEIRAS - BIB_FIL_GRADE)
#define BIB_GRADE_BASE (NV_TELA_H - NV_MARGEM_Y)
// Em quantos px um poster desaparece ao subir por baixo do cabecalho. O recorte
// de tesoura resolveria, mas gfx_recorte assume alvo 1:1 com a tela e o Mac em
// retina entrega o dobro; o esmaecimento nao depende do drawable.
#define BIB_FADE       90.0f
// How far above the grid origin the clip starts: 284 - 28 = 256, 8 px under
// the tab pills (193 + 55 = 248).
#define BIB_CLIP_SOBE  28.0f

// SELETORES COMPACTOS (21/09/2026, foto do dono da aba Salvos em lista:
// "aumentar o tamanho dos cards, diminuir a largura do botao, usar o espaco
// para enriquecer com informacoes").
//
// Eram TRES CAIXAS de 560x110 ocupando a largura toda, cada uma com rotulo em
// cima, valor embaixo e "OK: alterar" repetido tres vezes. Um seletor e um
// botao de uma linha: "Tipo · Todos" numa pilula de 56 de altura cuja largura
// SAI DO TEXTO (mais 2x24 de recuo), encostadas a esquerda uma apos a outra. A
// dica de OK deixou de ser repetida: ela vai UMA vez para a linha de resumo, a
// direita, so enquanto o foco esta nesta faixa. A faixa cai de 110 para 56 e a
// grade sobe de 354 para BIB_GRADE_Y_TITULOS.
#define BIB_PICK_H      56.0f
#define BIB_PICK_RAIO   28.0f
#define BIB_PICK_PADX   24.0f
#define BIB_PICK_GAP    12.0f
// Folga explicita nos dois lados do ponto: o espaco da fonte sozinho some a
// tres metros, e "Display·Artwork" vira uma palavra colada. O ponto continua
// sendo o separador visual, nao uma virgula nem um segundo rotulo.
#define BIB_PICK_SEP_GAP 7.0f
// Onde a grade de titulos/listas comeca: ver BIB_GRADE_Y (o mockup).
#define BIB_GRADE_Y_TITULOS (BIB_GRADE_Y - bibSubida())

// LISTA (a exibicao alternativa): uma linha por titulo, cartaz 2:3 a
// esquerda, titulo + meta rica no meio e a COLUNA DA NOTA a direita.
//
// A ALTURA SUBIU DE 96 PARA 168 (foto do dono, 21/09/2026). A linha de 96 tinha
// um cartaz de 60x90 que a 3 m e uma mancha, e o subtitulo dizia so "Filme"
// para quem veio da watchlist. Com 168 cabe um cartaz de 96x144 (2:3, ainda
// legivel como cartaz e nao como icone), o titulo em TXT_HEADLINE e uma linha
// de meta montada de tudo o que o CatItem ja carrega — e, so na linha em foco,
// a sinopse em uma linha. Separacao de 12 px entre linhas: as linhas viraram
// superficies com cor propria e precisam de fresta para nao virar uma tabela.
//
// MEMORIA: o cartaz pede tex_obter_larg com os 96 REAIS. capDeLargura e
// ceil32(larg * escala * 1.25) com piso de 128, entao 96 (-> 120, piso 128)
// custa o MESMO que os 60 de antes: MEDIDO em 48 KB por arte contra 363 KB do
// cartaz da grade. A linha cresceu sem custar um byte a mais.
#define BIB_LIN_H       124.0f   // mock 6B: 64x96 art, title, meta, 2-line note
#define BIB_LIN_GAP       8.0f
#define BIB_LIN_PASSO   (BIB_LIN_H + BIB_LIN_GAP)
#define BIB_LIN_MINI_W  (64.0f * gfx_poster_k())
#define BIB_LIN_MINI_H  (96.0f * gfx_poster_k())
#define BIB_LIN_PAD      12.0f
#define BIB_LIN_TX_GAP   24.0f    // do cartaz ao texto
// A linha de uma LISTA (modo Listas em exibicao de lista) nao tem cartaz nem
// meta: continua nos 96 de antes. Crescer junto so por simetria deixaria uma
// linha de texto so boiando em 168 px.
#define BIB_LL_H         96.0f
#define BIB_LL_PASSO    112.0f

// LISTA DE TITULOS (grade de titulos em exibicao de lista). O mockup aprovado
// "biblioteca-lista-servidor" (dono, 24/10/2026: "na biblioteca so a lista
// que poderia ficar mais parecido com o mockup") pede uma TABELA: linha
// compacta de 126 px, cartaz 2:3 de 64x96, UMA linha de meta, e na direita a
// coluna de origens e a de disponibilidade. Medido no PNG: pilula do foco em
// #363838 sobre a pagina, passo de ~130 px, 4-5 linhas visiveis. A pagina de
// uma lista aberta (EST_ITENS) NAO muda: os 168 de BIB_LIN_* continuam la.
#define BIB_TL_H        126.0f
#define BIB_TL_GAP        4.0f
#define BIB_TL_PASSO    (BIB_TL_H + BIB_TL_GAP)
#define BIB_TL_MINI_W    64.0f
#define BIB_TL_MINI_H    96.0f
#define BIB_TL_PAD       24.0f    // do inicio da linha ao cartaz
#define BIB_TL_TX_GAP    20.0f    // do cartaz ao texto (mockup: 652 -> 672)
#define BIB_TL_DIR       32.0f    // folga da coluna de disponibilidade
#define BIB_TL_CAB       42.0f    // altura do cabecalho de colunas acima

// CARTAO DE LISTA (o modo Listas em exibicao de cartaz): CINCO por linha.
// (1728 - 4*24)/5 = 326.4, arredondado para 326.
//
// ERAM QUATRO DE 414x200 e o dono pediu "podia mostrar mais listas": seis
// listas ocupavam dois tercos da tela e o terco de baixo ficava preto. Densidade
// aqui NAO foi encolher tudo 20% — foi decidir o que um cartao de lista precisa
// ter para ser ESCOLHIDO a tres metros, e deixar a contagem sair disso:
//
//   FICOU: o nome (em ate DUAS linhas, porque "Harry Potter e o Prisioneiro de
//          Azkaban" cortado em "Harry Potter…" nao distingue nada de uma
//          prateleira de Harry Potter) e o tamanho da lista com o dono.
//   SAIU:  a descricao. Ela vinha cortada em duas linhas SEMPRE terminando em
//          reticencias no meio de uma frase — uma frase que nunca termina e
//          decoracao, nao informacao, e custava 60 px de altura por cartao.
//   SAIU:  o wordmark do Trakt repetido em TODO cartao. Quando toda peca da
//          grade carrega a mesma marca, a marca deixa de identificar qualquer
//          coisa — e o seletor "Fonte" logo acima ja diz de onde a grade veio.
//          Ele volta SO onde a grade e MISTA (a aba Fixadas), que e o unico
//          lugar onde ele responde uma pergunta.
//
// Resultado MEDIDO na area util de 726 px: 726/156 = 4,6 fileiras de 5 = 20
// cartoes visiveis, contra 726/224 = 3,2 fileiras de 4 = 12. Mais 67%.
#define BIB_LC_COLS       3
#define BIB_LC_W        398.0f
#define BIB_LC_H        292.0f
#define BIB_LC_PASSO    430.0f
#define BIB_LC_LINHA    320.0f
// 16 e nao 20, e o numero saiu de uma CONTA, nao de gosto: a linha de apoio e
// ancorada na BASE do cartao para formar coluna com a dos vizinhos, e com pad 20
// mais duas linhas de titulo (2x32 de entrelinha) o bloco de cima invadia essa
// base — a guarda empurrava o apoio 5 px para baixo e a captura mostrou os
// cartoes de titulo longo com o apoio desalinhado do resto da fileira.
// 16 + 64 + 8 + 25 + 16 = 129 cabe nos 132 sem a guarda disparar.
#define BIB_LC_PAD       16.0f
#define BIB_LC_ENTRE     32.0f    // entrelinha do titulo de duas linhas

// --- O MOCKUP APROVADO (Glass UI "ilha", tela 9; out/2026) ---------------
//
// Dono, ao lado da captura: "o mockup ta bem mais polido que a build, nao
// podemos errar". A pagina passa a ser a do glass-ilha.html, em pixel de
// 1920x1080:
//   - titulo 56/800 em y 64 e, logo abaixo, o resumo ("14 titulos · sua
//     watchlist") no lugar de subtitulo; o selo da origem a direita, na linha
//     de base do resumo;
//   - UMA faixa em y 193: o seletor segmentado dos modos COM A CONTAGEM de
//     cada um, um fio vertical, e os tres filtros como chips de vidro "rotulo
//     cinza + valor branco";
//   - grade de cartazes de 220x330 com 26 de vao em ate SETE colunas, a 36 da
//     faixa; o cartaz em foco sobe (escala 1,08 a partir do centro) com sombra
//     longa, e o titulo dele clareia e abre em ate duas linhas.
// Corpos calibrados pela LARGURA medida na captura do mockup (ver a mesma nota
// em agendaui.c, escTitulo): a Inter variavel do navegador e ~9% mais larga
// abaixo de 24 px que a Inter estatica daqui.
#define BIB_TOPO         (124.0f / bibHS())
#define BIB_FAIXA_Y      faixaY()
// Owner, 25/10/2026: "subir tudo pra nao ficar esse espaco vazio em cima, suba
// os tabs ate ao lado do library". In the title tabs the tabs share the row of
// the Library pill (or title), and the summary, chips, Origens and grid rise by
// BIB_SUBIDA. Listas and an open list keep their own approved headers.
#define BIB_LINHA1_Y      37.0f   // tabs top when there is no menu pill
#define BIB_SUBIDA       152.0f   // chips 312 -> 160, grid 388 -> 236
#define BIB_SEG_H         NV_CTRL_H   // .seg: 5 + (23 + 2 x 11) + 5
#define BIB_SEG_PAD        5.0f
#define BIB_SEG_ITEM_PADX 22.0f
#define BIB_SEG_ITEM_GAP   4.0f
#define BIB_CHIP_H        NV_CTRL_H   // .vid dos filtros: 12 + 21 + 12
#define BIB_CHIP_PADX     20.0f
#define BIB_FAIXA_GAP     16.0f   // entre seletor, fio e chips
#define BIB_GRADE_Y      388.0f   // faixa + 36
#define BIB_COLUNAS_MAX      8
// Cartazes mantem o tamanho real (Escala de texto nao os muda): k compensa a tela
// virtual da camada de texto (gfx_poster_k, 1 em 100%).
#define BIB_CARD_W       (220.0f * gfx_poster_k())
#define BIB_POSTER_H     (330.0f * gfx_poster_k())
#define BIB_CARD_GAP      26.0f
#define BIB_TIT_GAP       10.0f
#define BIB_LINHA_PASSO  (BIB_POSTER_H * (1.0f + BIB_FOCO_ESCALA * 0.5f) + BIB_TIT_GAP + 14.0f + 52.0f + 2.8f)
#define BIB_FOCO_ESCALA    0.08f
// Onde a grade comeca em cada estado: o mesmo y, com ou sem lista aberta — as
// acoes da lista ocupam a faixa dos filtros.
#define BIB_GRADE_Y_ABERTA BIB_GRADE_Y

// "Salvos" = QUERO VER, e ele tem DUAS fontes que caem na mesma marca
// (CatItem.naLista): a watchlist do Trakt, posta ali pela descoberta, e a
// biblioteca da CONTA (sync_pull_library), posta ali por contalib.c.
// "Coleção" = TENHO, e so o Trakt tem. "Listas" = conjuntos, nao titulos.
//
// 2.1: as abas sao ESTADOS do titulo, nao fontes. Cada titulo cai em uma so de
// Para ver / Em andamento / Assistidos (ver estadoDoItem); Coleções e Listas
// seguem como antes.
enum { MODO_PARAVER, MODO_ANDAMENTO, MODO_ASSISTIDOS, MODO_COLECOES, MODO_LISTAS, BIB_N_MODOS };
static const char *ROT_MODO[BIB_N_MODOS] =
  { "Para ver", "Em andamento", "Assistidos", "Coleções", "Listas" };

// Seletor "Tipo": os mesmos valores do web.
enum { TIPO_TODOS, TIPO_FILME, TIPO_SERIE, BIB_N_TIPOS };
static const char *ROT_TIPO[BIB_N_TIPOS] = { "Todos", "Filmes", "Séries" };
// Seletor "Ordenar".
enum { ORD_ADICIONADOS, ORD_TITULO, ORD_ANO, ORD_VISTO, ORD_NAO_VISTO, BIB_N_ORD };
static const char *ROT_ORD[BIB_N_ORD] = { "Ordem da lista", "Título: A a Z", "Ano: mais recentes",
                                          "Assistidos primeiro", "Não assistidos primeiro" };
// Seletor "Exibição".
enum { VIS_CARTAZ, VIS_LISTA, BIB_N_VIS };
static const char *ROT_VIS[BIB_N_VIS] = { "Cartazes", "Lista" };
// Seletor "Origem", so em Para ver: de onde vem a marca de lista do titulo.
// Um titulo salvo em Trakt e Simkl aparece uma vez, em Todas e nas duas. Salvo
// so nesta TV (sem fonte remota) conta como Conta Nuvio.
enum { ORIG_TODAS, ORIG_TRAKT, ORIG_SIMKL, ORIG_NUVIO, BIB_N_ORIG };
static const char *ROT_ORIG[BIB_N_ORIG] = { "Todas", "Trakt", "Simkl", "Conta Nuvio" };
// Seletor "Fonte", no modo Listas. "Públicas" e a busca — ela vive na mesma
// roda porque e mais uma origem de listas, nao um lugar separado.
enum { FONTE_TRAKT, FONTE_SIMKL, FONTE_NUVIO, FONTE_FIXADAS, FONTE_PUB, FONTE_PLEX, FONTE_JELLYFIN, FONTE_EMBY, BIB_N_FONTES };
static const char *ROT_FONTE[BIB_N_FONTES] =
  { "Trakt", "Simkl", "Nuvio", "Fixadas", "Públicas", "Plex", "Jellyfin", "Emby" };

// Em que grade o foco esta. Nao e um modo a mais: e o mesmo modo Listas com uma
// lista aberta por cima.
enum { EST_TITULOS, EST_LISTAS, EST_ITENS };

static int modo = MODO_PARAVER;
static int tipo = TIPO_TODOS;
static int ordem = ORD_ADICIONADOS;
static int origem = ORIG_TODAS;
static int focoOrigem, origemSel;
static int contaOrigem[BIB_N_ORIG];
static struct { char id[96], nome[96]; int count; } addonOrigens[8];
static int nAddonOrigens;

// Source labels share one bounds check, including stale add-on selections.
static const char *rotuloOrigem(int indice, int listas) {
  if (listas) return ROT_FONTE[indice >= 0 && indice < BIB_N_FONTES ? indice : FONTE_TRAKT];
  if (indice >= 0 && indice < BIB_N_ORIG) return ROT_ORIG[indice];
  if (indice >= BIB_N_ORIG && indice - BIB_N_ORIG < nAddonOrigens)
    return addonOrigens[indice - BIB_N_ORIG].nome;
  return ROT_ORIG[ORIG_TODAS];
}
static BibliotecaOrigens serverRows;
static void (*serverSelect)(int);
static int (*serverContains)(const CatItem *, int);
// Qualidade de um titulo nos servidores (altura/hdr), para a coluna de
// disponibilidade da lista. Teste de captura injeta um falso aqui; sem ele
// vale o caminho de producao (servidores_info).
static int (*serverQualidade)(const CatItem *, int *, int *);
static unsigned char serverMembership[CAT_MAX];
static int serverFilter = -1;
void biblioteca_servidores_contem(int (*contains)(const CatItem *, int)) { serverContains = contains; }
void biblioteca_servidores_qualidade(int (*info)(const CatItem *, int *altura, int *hdr)) { serverQualidade = info; }
static void reconstruir(void);
void biblioteca_servidor_filtrar(int provider) {
  serverFilter = provider >= 0 && provider < 3 ? provider : -1;
  origem = ORIG_TODAS;
  reconstruir();
}
void biblioteca_origens_servidores(BibliotecaOrigens rows, void (*select)(int)) {
  serverRows = rows; serverSelect = select;
}
static int nOrigens(void) {
  int n = BIB_N_ORIG + nAddonOrigens;
  if (serverRows) {
    const char *name, *sub; int count;
    for (int i = 0; i < 4 && serverRows(i, &name, &sub, &count); i++) n++;
  }
  return n;
}
static int exibicao = VIS_CARTAZ;
static int esconderVistos, generoSel;
static const char *generos[] = { "Todos", "Drama", "Comédia", "Ação", "Animação", "Documentário", "Terror" };
static const char *generosEn[] = { "", "Drama", "Comedy", "Action", "Animation", "Documentary", "Horror" };
static int fonte = FONTE_TRAKT;
static int listaTab;
static const char *ROT_LISTA_TAB[] = { "Minhas listas", "Seguindo", "Explorar", "Trilhas" };
static int fonteSalvos = -1;
static int pickSel = 0;          // qual dos seletores esta em foco

static LstLista aberta;          // lista aberta (EST_ITENS)
static int  temAberta;
static char abertaMidia[8] = "MOVIE";
static int  acaoSel = 0;

static char recado[160];         // resposta de uma acao ("Fixada", o porque do nao)
static float recadoAte;

// LISTA PROPRIA (Trakt > Minhas listas): ganha Descricao, Ordem, Notas e
// Compartilhar. `edModo` diz o que o OK faz na grade: nada de especial, pegar e
// soltar um titulo (Ordem) ou escrever a nota dele (Notas). A ordem so vai ao
// Trakt ao CONCLUIR; ate la ela existe so na tela (lst_ordem_mover).
enum { ED_NADA, ED_ORDEM, ED_NOTAS };
static int  abertaPropria;
static int  edModo = ED_NADA;
static int  ordemPega = -1;      // posicao do titulo pego, -1 = solto
static int  ordemMudou;
static int  escrevendo;          // 1 = uma escrita DESTA tela esta em voo
static int  compartilha;         // 1 = o QR do link da lista esta na tela
static float compartA, compartForma, compartFormaV;  // mola da folha sem relogio
static char urlLista[200];
static char descNova[192];       // a descricao enviada, para espelhar no OK
static long notaItemId;
static int trilha;
static int nAcoes(void) { return (abertaPropria ? 7 : 3) + (aberta.fonte == LST_TRAKT ? 1 : 0); }

// Indices visiveis. >= 0 e indice do CATALOGO; < 0 e um titulo da lista LOCAL
// de salvos que nao esta no catalogo agora, codificado como -(indice + 1) em
// salvos_item(). Ver itemFiltro.
static int filtro[CAT_MAX + SALVOS_MAX];
static int nFiltro = 0;
static int totalModo = 0;
// Quantos titulos cada modo de titulos tem (Salvos, Colecao), para a contagem
// do seletor segmentado. Refeito em reconstruir(), nunca por quadro.
static int contaModo[BIB_N_MODOS];
static Foco foco;

void biblioteca_teste_foco(int *fileira, int *coluna, int *acao) {
  *fileira = foco.fileira;
  *coluna = foco.coluna;
  *acao = acaoSel;
}
static float animModo[BIB_N_MODOS];
static float animPick[8];
static float animOrigem[64], animTela, animVista = 1, animListaTab, animListaEd, animModoPos;
// Retain only rendered cells, not a second catalog, during tab crossfades.
#define BIB_TRANSITION_CELLS 32
typedef struct {
  CatItem item;
  LstLista lista;
  GfxRect rect;
  float focus, alpha;
  int kind;
} BibTransitionCell;
static BibTransitionCell vistaAtual[BIB_TRANSITION_CELLS], vistaAnterior[BIB_TRANSITION_CELLS];
static int nVistaAtual, nVistaAnterior, modoAnterior, exibicaoAnterior, vistaModo, vistaExibicao;
static int estadoFoco;
static float animEstado;
static float trilhaX, trilhaVel, vistaClipY, anteriorClipY;
static Foco focoListas;
static int temFocoListas;
static float animFoco[BIB_MAX_LINHAS][BIB_COLUNAS_MAX];
// Arte chegando, a mesma da home (revela.h): um registro por celula da grade.
static RevelaArte revArte[BIB_MAX_LINHAS][BIB_COLUNAS_MAX];
// A ONDA da grade (revela.h): armada quando a grade e refeita do zero
// (remapear sem preservar: entrar, trocar modo/filtro/ordem) e disparada no
// primeiro quadro com celulas. Pagina que chega pela rede preserva o foco e
// nao reacende a onda; rolar tambem nao.
static Uint32 ondaEm;
static int ondaArmada;
static float scrollY = 0.0f;
static Uint32 okDesde;
#ifdef NV_TOUCH_UI
static int toqueLivre;
#endif
// Velocidade da mola de 2a ordem da rolagem (anim_mola2): partida macia e
// cauda exponencial, a MESMA curva que a home mede. A de 1a ordem que estava
// aqui partia na velocidade maxima e o primeiro quadro ja saltava 12%.
static float velY = 0.0f;
static int sair = 0, pedido = -1;
static int pediuRelogio;
static int nCelulas = 0;         // celulas da grade do estado atual
// Para que o teclado modal esta na tela; o resultado e lido em
// biblioteca_atualizar quando ele fecha.
enum { TEC_NADA, TEC_BUSCA, TEC_NOVA, TEC_DESC, TEC_NOTA };
static int tecPara;
// Texto livre: a modal so digita o que o alfabeto tem, e ela ignora acento
// (teclado.c). Duas caixas viram camadas.
static const char *ALFA_TEXTO =
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .,:;!?'\"()-&/+#@";

// Estado de conta. A lista de verdade e a do Trakt, que marca
// ci->naLista/naColecao NO ITEM — nao ha mais tabela por indice aqui: o
// catalogo e reconstruido da rede e um indice guardado aponta para outro titulo
// na volta seguinte. `comprado` sobrevive porque nao ha fonte para ele ainda.
static char comprado[CAT_MAX];

// ---------------------------------------------------------------- preferencia

// A ESCOLHA DE EXIBIÇÃO E POR PERFIL, como fileirasui-p<N>.txt e agenda-p<N>.txt.
// Quem troca de perfil na tela "Quem esta assistindo?" nao herda a exibicao do
// outro, e o arquivo sem sufixo continua servindo a quem nunca escolheu perfil.
static int prefPerfil = -1;
// The Origens panel opens and closes from the "Origens" chip (owner, 25/10),
// remembered per profile next to the view. animOrig follows it on the island
// spring; flip* keep the grid as it was so the posters glide to their new slot.
static int origensAbertas = 1, focoBotaoOrig, faixaNoBotao;
static float animOrig = 1, velOrig, animBotaoOrig, flipDe = 1;
static float flipOffset[CAT_MAX + SALVOS_MAX][2];
static const char *arquivoPref(void) {
  static char nome[64];
  int p = perfis_ativo();
  if (p <= 0) { snprintf(nome, sizeof nome, "bibliotecaui.txt"); return nome; }
  snprintf(nome, sizeof nome, "bibliotecaui-p%d.txt", p);
  return nome;
}
static void lerPref(void) {
  char *b;
  int p = perfis_ativo();
  if (p == prefPerfil) return;
  prefPerfil = p;
  exibicao = VIS_CARTAZ;
  origensAbertas = 1;
  b = dados_ler(arquivoPref());
  if (b) {
    const char *s = strstr(b, "exibicao\t");
    if (s) exibicao = atoi(s + 9) ? VIS_LISTA : VIS_CARTAZ;
    s = strstr(b, "origens\t");
    if (s) origensAbertas = atoi(s + 8) != 0;
    free(b);
  }
  animOrig = flipDe = (float)origensAbertas; velOrig = 0;
}
static void gravarPref(void) {
  char txt[200];
  snprintf(txt, sizeof txt,
           "# Exibicao da Biblioteca neste perfil. 0 = cartazes, 1 = lista.\n"
           "exibicao\t%d\n"
           "# Painel Origens: 1 = aberto, 0 = fechado.\n"
           "origens\t%d\n", exibicao, origensAbertas);
  dados_gravar(arquivoPref(), txt);
}

// ---------------------------------------------------------------- geometria

// O RAIO DE CANTO E FRACAO DA ALTURA, nao do menor lado. CONFERIDO em FS_SDF
// (src/gfx.c): o fragmento normaliza com `p = (uv - 0.5) * vec2(asp, 1.0)`, e a
// meia-extensao VERTICAL e sempre 0.5 — entao `raio * h` e o raio em pixels,
// qualquer que seja a largura. Os comentarios deste repositorio que dizem
// "menor lado" estao errados; novidades11.c ja registra o mesmo achado.
//
// O QUE ISSO CUSTAVA AQUI, e nao e teorico: o cartaz pedia `24.0f/268` (a
// LARGURA), que sobre 402 px de altura dava 36 px de canto — meia vez mais
// redondo do que a folha do web manda. A miniatura da lista pedia `8/64` e
// saia com 12. Duas formas diferentes para a mesma intencao, na mesma tela.
static float raioPx(float px, float w, float h) {
  float r;
  if (h <= 0.0f) return 0.0f;
  r = px / h;
  if (r > 0.5f) r = 0.5f;                     // capsula vertical e o maximo
  if (w > 0.0f && r > 0.5f * w / h) r = 0.5f * w / h;   // nem mais que a meia-largura
  return r;
}

static int estado(void) {
  if (modo != MODO_LISTAS) return EST_TITULOS;
  return temAberta ? EST_ITENS : EST_LISTAS;
}
// Listas › Minhas listas (design/2.1/listas.html 6D): one column of rows and,
// on the right, the panel of the focused list (description, edits, share QR).
#define BIB_LM_H       78.0f
#define BIB_LM_PASSO   82.0f
#define BIB_LM_PAINEL 624.0f
static int listasMinhas(void) { return modo == MODO_LISTAS && !temAberta && listaTab == 0; }
// At large text scales the panel gives way so the rows keep ~380 for their titles.
static float bibDireita(void);
static float bibX(void);
static float lmPainel(void) { return fminf(BIB_LM_PAINEL, fmaxf(480, bibDireita() - bibX() - 380)); }
static int focoPainel, painelSel, painelIdx = -1;
static int painelAcoes(const LstLista *l, int *ids);
static float animPainel[4], animPainelTroca = 1;
// Primeira fileira de foco da grade. Com uma lista aberta nao ha seletores.
static int gradeIni(void) { return estado() == EST_ITENS ? 1 : BIB_FIL_GRADE; }
// A AREA UTIL SAI DA RAIL (26/09, dono: "quando a sidebar ta no modo pinned
// ela corta a interface"). NV_BIB_X/NV_BIB_W sao a medida do web com a barra
// RECOLHIDA — 96 de recuo, 1728 uteis. Presa, a rail cobre os primeiros 144 px
// e o titulo, as abas e a primeira coluna nasciam debaixo dela. Agora tudo
// parte de bibX() e a direita continua em NV_BIB_DIR: a tela encolhe pela
// esquerda, e quem nao cabe e COLUNA, nao pixel.
static float paginaX(void) {
  float x;
  t21_inicio(&x, NULL);
  return x;
}
static int temOrigens(void) { return estado() != EST_ITENS && modo != MODO_ASSISTIDOS; }
static int bibRetrato(void);
// The "Origens" chip exists where the title grid has the panel beside it.
static int botaoOrigens(void) { return temOrigens() && modo != MODO_LISTAS && !bibRetrato(); }
// The panel takes its column only while open (layout snaps; drawing animates).
static int origensNaGrade(void) { return temOrigens() && (origensAbertas || !botaoOrigens()); }
static int modoLinha(void);
static float bibX(void) {
  if (temAberta && aberta.fonte == LST_TRAKT && !trilha) return paginaX() + 560;
  if (botaoOrigens() && modoLinha()) return paginaX() + ANC(364.0f) * animOrig;
  return paginaX() + (origensNaGrade() ? (modo == MODO_LISTAS ? 364.0f : ANC(364.0f)) : 0.0f);
}
// Right edge in text-layer units. NV_TELA_W changes with the active layer (the
// header runs at another scale than the grid), so the edge is derived from the
// text scale alone: the same left/right margins at every scale, and the grid,
// the columns and the header badge all agree on it.
static float bibDireita(void) {
  float x, w;
  t21_inicio(&x, &w);
  return x + w;
}
static float bibW(void) {
  if (temAberta && aberta.fonte == LST_TRAKT && !trilha)
    return fmaxf(320, bibDireita() - bibX() - 448);
  if (listasMinhas()) return bibDireita() - bibX() - lmPainel() - 36;
  return bibDireita() - bibX();
}
// O TOPO (modos, chips, botoes, titulo, resumo e selo) NASCE A 120% E ACOMPANHA
// o Tamanho da interface acima disso (dono, 03/10); a GRADE de cartazes fica no
// tamanho de sempre. O topo e medido e desenhado numa tela virtual de 1920/hs x
// 1080/hs (escala.h): hdrX/hdrW sao a area util em unidades virtuais — o mesmo
// canto esquerdo e a mesma largura reais de bibX/bibW — e a grade comeca
// DEPOIS do topo ampliado (gradeYBase). So biblioteca_desenhar liga a escala.
#define BIB_TOPO_ESCALA_MIN 1.0f
static int bibRetrato(void) {
#ifdef NV_TOUCH_UI
  return NV_TELA_W < NV_TELA_H;
#else
  return 0;
#endif
}
static float bibHS(void) { return fmaxf(1.0f, escala_min(BIB_TOPO_ESCALA_MIN)); }
static float bibRel(void) { return bibHS() / gfx_escala_txt(); }
static float hdrX(void) { return paginaX() / bibRel(); }
static float hdrDir(void) { return bibDireita() / bibRel(); }
static float hdrW(void) { return hdrDir() - hdrX(); }
// Quantos seletores a faixa tem: "Para ver" ganha o quarto, "Origem".
static int bibNPicks(void);
static float bibControleMax(void) { return (hdrW() - BIB_FAIXA_GAP * 2.0f) / (float)bibNPicks(); }
// Os chips descem para uma segunda linha sob as abas: sempre no retrato e, na
// paisagem, quando abas + chips nao cabem na largura (ver bibFaixaQuebra).
static int bibFaixaQuebra(void);
static int bibCompacto(void) { return !temAberta && modo != MODO_LISTAS && !bibRetrato(); }
static float bibSubida(void) { return bibCompacto() ? BIB_SUBIDA : 0.0f; }
// Tabs row top, in header units: centred on the menu pill when there is one.
static float faixaY(void) {
  float px, py, pw, ph;
  if (!bibCompacto()) return 232.0f / bibHS();
  if (menu_pilula_rect(&px, &py, &pw, &ph)) return (py + ph * 0.5f) / bibHS() - BIB_SEG_H * 0.5f;
  return BIB_LINHA1_Y / bibHS();
}
// Where the tabs start: right of the pill, or right of the "Biblioteca" title.
static float modosX(void) {
  float px, py, pw, ph;
  if (!bibCompacto()) return hdrX();
  if (menu_pilula_rect(&px, &py, &pw, &ph)) return (px + pw) / bibHS() + 24.0f;
  return hdrX() + (float)txt_linha(TXT_TITULO2, "Biblioteca", 245, 245, 243, 255).w + 32.0f;
}
// Summary line top: under the tabs in the compact header, under the title before.
static float resumoY(void) {
  if (bibCompacto()) return faixaY() + BIB_SEG_H + 14.0f;
  return BIB_TOPO + (float)txt_linha(TXT_TITULO2, "Hg", 255, 255, 255, 255).h - 2.0f;
}
static float pickerY(void) {
  return (312.0f - bibSubida()) / bibHS();
}
// Cartazes: o cartaz fica nos 268 medidos e sai uma coluna (6 -> 5 com a rail
// fixa: 5 x 268 + 4 x 24 = 1436 nos 1584). Encolher o cartaz para manter seis
// mudaria o raio, a borda e a arte pedida — e o dono ja aprovou esse tamanho.
// Cartoes de lista: continuam CINCO e estreitam (326 -> 297). O cartao e texto
// em duas linhas, nao arte, e a densidade de cinco foi o pedido dele (ver
// BIB_LC_COLS).
// GLASS UI: cartaz de 220 com 26 de vao, ate sete colunas. A coluna e 1fr no
// mockup (o cartaz de 220 encostado a esquerda de uma celula de 224,6), entao
// o passo e a largura util dividida pelas colunas: 250,6 nos 1728.
static int colunasCartaz(void) {
  int n = (int)((bibW() + BIB_CARD_GAP) / (BIB_CARD_W + BIB_CARD_GAP));
  return n < 1 ? 1 : n > 7 ? 7 : n;
}
static int colunasLista(void) {
#ifdef NV_TOUCH_UI
  if (NV_TELA_W < NV_TELA_H) {
    int n = (int)((bibW() + BIB_LC_PASSO - BIB_LC_W) / BIB_LC_PASSO);
    return n < 1 ? 1 : n > BIB_LC_COLS ? BIB_LC_COLS : n;
  }
#endif
  return BIB_LC_COLS;
}
static float larguraCartaoLista(void) {
  int n = colunasLista();
  float w = (bibW() - (n - 1) * (BIB_LC_PASSO - BIB_LC_W)) / n;
  return w < BIB_LC_W ? w : BIB_LC_W;
}
// A PAGINA DA LISTA (design/2.1/listas.html 6B). Uma lista do Trakt aberta nao e
// uma grade de cartazes: tem dono, descricao, ordem (`rank`) e nota do curador
// por item, e nada disso cabe num cartaz. Ela abre em LINHAS, com o cabecalho
// logo acima, e a exibicao salva da pessoa continua valendo nas outras fontes
// (Simkl/Nuvio nao informam rank nem nota).
#define BIB_PAG_CAB_H   150.0f   // descricao (2 linhas) + dono/datas + numeros
#define BIB_PAG_DESC_H   72.0f   // o que a descricao ocupa; sem ela o cabecalho encolhe
#define BIB_PAG_RANK_W   84.0f   // coluna do rank, antes da miniatura
static int paginaLista(void) { return temAberta && aberta.fonte == LST_TRAKT; }
static float paginaCabH(void) { return BIB_PAG_CAB_H - (aberta.descricao[0] ? 0.0f : BIB_PAG_DESC_H); }
static int modoLinha(void) { return exibicao == VIS_LISTA || paginaLista() || listasMinhas(); }
static int colunas(void) {
  if (trilha && temAberta) return 8;
  if (modoLinha()) return 1;
  return estado() == EST_LISTAS ? colunasLista() : colunasCartaz();
}
static float passoColuna(void) {
  return estado() == EST_LISTAS ? larguraCartaoLista() + (BIB_LC_PASSO - BIB_LC_W)
                                : (bibW() + BIB_CARD_GAP) / (float)colunasCartaz();
}
// A ALTURA DA LINHA E A DA ROLAGEM: alturaLinha/passoLinha sao a UNICA fonte
// para o laco de desenho e para o calculo de scrollY em biblioteca_atualizar.
// Uma linha de titulo (168) e uma linha de lista (96) nao tem a mesma altura,
// e e por isso que a exibicao de lista pergunta o estado.
static float alturaLinha(void) {
  if (modo == MODO_ASSISTIDOS && modoLinha()) return 120;
  if (modoLinha())
    return estado() == EST_LISTAS ? (listasMinhas() ? BIB_LM_H : BIB_LL_H)
         : paginaLista() ? BIB_LIN_H : BIB_TL_H;
  if (estado() == EST_LISTAS) return BIB_LC_H;
  // poster + 10 + o titulo em ate duas linhas (o focado abre a segunda)
  return BIB_POSTER_H + BIB_TIT_GAP + 14.0f + 52.0f;
}
static float passoLinha(void) {
  if (modo == MODO_ASSISTIDOS && modoLinha()) return 126;
  if (modoLinha())
    return estado() == EST_LISTAS ? (listasMinhas() ? BIB_LM_PASSO : BIB_LL_PASSO)
         : paginaLista() ? BIB_LIN_PASSO : BIB_TL_PASSO;
  if (estado() == EST_LISTAS) return BIB_LC_LINHA;
  return BIB_LINHA_PASSO;
}
static float gradeY(void) {
  // A tabela de titulos comeca mais baixo para caber o cabecalho de colunas
  // (mockup: etiqueta em ~395, primeira linha em ~433).
  if (modoLinha() && estado() == EST_TITULOS) return BIB_GRADE_Y_TITULOS + BIB_TL_CAB;
  float y = estado() == EST_ITENS ? BIB_GRADE_Y_ABERTA : BIB_GRADE_Y_TITULOS;
  // The Lists tabs share the Library header and chip row, so their rows anchor
  // the same way (y * bibRel() climbed into the chips at 85 %).
  y = !temAberta ? ANC(y) : y * bibRel();
  if (modo == MODO_ASSISTIDOS && modoLinha()) y += 44;
  return paginaLista() && !trilha ? 160.0f : y;   // under the 6B column kicker
}
static int nLinhas(void) { return (nCelulas + colunas() - 1) / colunas(); }

#ifdef NV_TOUCH_UI
static float toqueBibliotecaMax(void) {
  int linhas = nLinhas();
  if (estado() != EST_ITENS && linhas > BIB_MAX_LINHAS) linhas = BIB_MAX_LINHAS;
  return linhas > 0 ? fmaxf(0.0f, gradeY() + (linhas - 1) * passoLinha() + alturaLinha() - BIB_GRADE_BASE) : 0.0f;
}
static int toqueBibliotecaRolar(const PonteiroRolagem *e) {
  if (!layout_modo_mobile()) { toqueLivre = 0; return 0; }
  if (e->fase == PONT_ROL_INICIO) {
    if (!e->eixoY || e->x < bibX() || e->x >= bibX() + bibW() || e->y < gradeY() || e->y >= BIB_GRADE_BASE || nCelulas < 1 || teclado_aberto()) return 0;
    toqueLivre = 1; velY = 0.0f; okDesde = 0;
    return 1;
  }
  if (e->fase == PONT_ROL_MOVER || e->fase == PONT_ROL_INERCIA) {
    float antes = scrollY;
    scrollY = anim_clamp(scrollY - e->delta, 0.0f, toqueBibliotecaMax());
    if (estado() == EST_ITENS && scrollY >= toqueBibliotecaMax() - 2.0f * passoLinha()) lst_itens_mais();
    return fabsf(scrollY - antes) > 0.001f;
  }
  return 1;
}
static void toqueBibliotecaRetomarFoco(void) {
  if (layout_modo_mobile() && toqueLivre && nCelulas > 0) {
    int r = (int)((scrollY + (BIB_GRADE_BASE - gradeY()) * 0.35f) / passoLinha());
    int i = r * colunas() + foco.coluna;
    if (i >= nCelulas) i = nCelulas - 1;
    if (i < 0) i = 0;
    foco.fileira = gradeIni() + i / colunas(); foco.coluna = i % colunas();
  }
  toqueLivre = 0;
}
#endif

static int ehSerie(const CatItem *ci) {
  return ci && (!strcmp(ci->tipo, "series") || ci->nTemporadas > 0
                || ci->temporada > 0);
}

// ---------------------------------------------------------------- montagem

// Quantas celulas a grade do estado atual tem. As tres fontes sao diferentes e
// e aqui que elas se encontram, para o mapa de foco e a rolagem serem um so.
static int contarCelulas(void) {
  int n;
  if (estado() == EST_TITULOS) return nFiltro;
  if (estado() == EST_LISTAS)  return lst_n();
  n = lst_itens_n();
  return n;
}

// Refaz o mapa de foco. Chamada a cada troca de modo, fonte, tipo, ordem,
// exibicao — e quando a rede acrescenta itens —, porque o numero de colunas da
// ultima linha muda com o filtro e um foco apontando para uma coluna que nao
// existe mais desenha um retangulo vazio.
// Colunas com que o mapa de foco foi montado. A rail fixa muda colunas() por
// fora desta tela (a opcao fica em Ajustes); biblioteca_atualizar compara e
// refaz o mapa, senao a grade de 5 andaria com a navegacao de 6.
static int ncMapa = 0;
static int bibNPicks(void);
static int estadoFalhou(void) {
  if (nCelulas) return 0;
  if (estado() == EST_ITENS)
    return aberta.fonte != LST_SIMKL && !lst_itens_carregando() && desc_vertudo_erro();
  return estado() == EST_LISTAS && !lst_carregando() && strstr(lst_aviso(), "Não foi possível");
}
// Quantos dos itens da lista aberta a pessoa ja viu, para o cabecalho da pagina.
// Contado aqui e nao por quadro: remapear roda quando chegam itens, e varrer
// ate 960 CatItem a 60 Hz seria custo sem ninguem pedindo.
static int pagVistos = 0, pagProximo = -1;   // first unwatched item, for "Continuar"
static void contarVistos(void) {
  CatItem tmp;
  pagVistos = 0; pagProximo = -1;
  if (!paginaLista()) return;
  for (int i = 0; i < nCelulas; i++)
    if (!lst_item(i, &tmp)) continue;
    else if (cat_visto(&tmp)) pagVistos++;
    else if (pagProximo < 0) pagProximo = i;
}
// 6B primary button above the action chips; Up from the chips reaches it.
static int focoCont;
static float animCont;
static int temContinuar(void) { return paginaLista() && !trilha && pagProximo >= 0; }
static void remapear(int preservar) {
  if (pickSel >= bibNPicks()) pickSel = bibNPicks() - 1;
  int linhas, cols[FOCUS_MAX_FILEIRAS], r, ini = gradeIni(), nc = colunas();
  ncMapa = nc;
  int fAntes = foco.fileira, cAntes = foco.coluna;
  nCelulas = contarCelulas();
  contarVistos();
  linhas = nLinhas();
  if (linhas > FOCUS_MAX_FILEIRAS - ini) linhas = FOCUS_MAX_FILEIRAS - ini;
  if (estado() == EST_ITENS) cols[0] = nAcoes();    // as acoes
  else { cols[BIB_FIL_MODO] = modo == MODO_LISTAS ? 4 : BIB_N_MODOS; cols[BIB_FIL_PICK] = bibNPicks(); }
  for (r = 0; r < linhas; r++) {
    int resto = nCelulas - r * nc;
    cols[ini + r] = resto > nc ? nc : resto;
  }
  focus_iniciar(&foco, ini + linhas, cols);
  if (preservar && fAntes < ini + linhas) {
    foco.fileira = fAntes;
    foco.coluna = cAntes < foco.nColunas[fAntes] ? cAntes
                                                 : foco.nColunas[fAntes] - 1;
    if (foco.coluna < 0) foco.coluna = 0;
    // NAO ZERA AS ANIMACOES quando o foco e preservado. remapear(1) e chamado a
    // CADA quadro em que a rede acrescenta itens; zerar ali fazia a mola do
    // cartaz focado recomecar a cada pagina que chegava, e o foco piscava
    // enquanto a lista carregava.
    return;
  }
  if (!temAberta && nVistaAtual) {
    memcpy(vistaAnterior, vistaAtual, nVistaAtual * sizeof *vistaAtual);
    nVistaAnterior = nVistaAtual;
    modoAnterior = vistaModo; exibicaoAnterior = vistaExibicao;
    anteriorClipY = vistaClipY;
  }
  nVistaAtual = 0; estadoFoco = 0;
  trilhaX = trilhaVel = 0;
  scrollY = 0.0f; velY = 0.0f;
#ifdef NV_TOUCH_UI
  toqueLivre = 0;
#endif
  memset(animFoco, 0, sizeof animFoco); memset(revArte, 0, sizeof revArte);
  ondaArmada = nVistaAnterior == 0; ondaEm = 0;
  animVista = 0;
}

// O item de uma posicao de `filtro`. Salvo local fora do catalogo vira um
// CatItem montado em `tmp` com o que a lista local guardou (titulo, poster,
// meta), do mesmo jeito que o item de uma lista aberta (lst_item).
static const CatItem *itemFiltro(int v, CatItem *tmp) {
  const SalvoItem *s;
  if (v >= 0) return cat_item(v);
  s = salvos_item(-v - 1);
  if (!s || !tmp) return NULL;
  memset(tmp, 0, sizeof *tmp);
  snprintf(tmp->imdb, sizeof tmp->imdb, "%s", s->id);
  snprintf(tmp->tipo, sizeof tmp->tipo, "%s", s->tipo);
  snprintf(tmp->titulo, sizeof tmp->titulo, "%s", s->titulo);
  snprintf(tmp->poster, sizeof tmp->poster, "%s", s->poster);
  snprintf(tmp->meta, sizeof tmp->meta, "%s", s->meta);
  tmp->nota = s->nota;
  tmp->naLista = 1;
  arte_metahub_preencher(tmp);
  return tmp;
}
// So as chaves de ordenacao, SEM montar CatItem: a ordenacao compara O(n^2)
// vezes e montar 15 KB por comparacao custaria gigabytes de memset.
static const char *tituloFiltro(int v) {
  const CatItem *c; const SalvoItem *s;
  if (v >= 0) { c = cat_item(v); return c ? c->titulo : ""; }
  s = salvos_item(-v - 1); return s ? s->titulo : "";
}
static const char *metaFiltro(int v) {
  const CatItem *c; const SalvoItem *s;
  if (v >= 0) { c = cat_item(v); return c ? c->meta : ""; }
  s = salvos_item(-v - 1); return s ? s->meta : "";
}
// 1 quando o titulo de uma posicao de `filtro` consta como visto (cat_visto,
// o MESMO estado do selo). Salvo local fora do catalogo: so imdb e tipo importam
// para o historico (#352).
static int vistoFiltro(int v) {
  if (v >= 0) return cat_visto(cat_item(v));
  { const SalvoItem *s = salvos_item(-v - 1);
    CatItem tmp;
    if (!s) return 0;
    memset(&tmp, 0, sizeof tmp);
    snprintf(tmp.imdb, sizeof tmp.imdb, "%s", s->id);
    snprintf(tmp.tipo, sizeof tmp.tipo, "%s", s->tipo);
    return cat_visto(&tmp); }
}
static int ehSerieSalvo(const SalvoItem *s) {
  return s && !strcmp(s->tipo, "series");
}

// Em andamento: comecou (progresso parcial ou ponto de retomada) e ainda nao
// terminou. cat_visto manda: >=90% tocado ou historico ja e assistido.
static int emAndamento(const CatItem *c) {
  return c && !cat_visto(c) && (c->progresso > 0 || c->retomadoMs > 0);
}
// Quantos seletores a faixa tem: "Para ver" ganha o quarto, "Origem".
// "Listas" ganha o quarto na fonte Trakt: "Nova lista".
static int bibNPicks(void) {
  if (listasMinhas()) return 1;
  return modo == MODO_LISTAS ? (fonte == FONTE_TRAKT ? 4 : 3) : modo == MODO_ASSISTIDOS ? 5 : 6;
}
// 6D keeps one chip: the Origens panel is the source, the rows are the view.
// Trakt creates ("Nova lista"); other sources search public lists.
static int pickId(int p) { return listasMinhas() ? (fonte == FONTE_TRAKT ? 3 : 2) : p; }
static int bibViewStart(void) { return modo == MODO_ASSISTIDOS ? 3 : 4; }
// 1 quando o titulo passa no seletor Origem. `fontes` 0 com naLista e o salvo
// desta TV: Conta Nuvio.
static int daOrigem(int fontes) {
  if (origem == ORIG_TODAS || origem >= BIB_N_ORIG) return 1;
  if (!fontes) fontes = CAT_FONTE_NUVIO;
  return (fontes & (origem == ORIG_TRAKT ? CAT_FONTE_TRAKT
                  : origem == ORIG_SIMKL ? CAT_FONTE_SIMKL : CAT_FONTE_NUVIO)) != 0;
}
// 1 quando o item `ci` (indice i do catalogo) pertence a aba `m`.
//   Para ver     : esta na lista (watchlist/Conta) e nao esta em andamento. O
//                  assistido que continua na lista fica aqui com o selo de visto,
//                  e o chip "Esconder assistidos" o tira.
//   Em andamento : emAndamento, venha de onde vier.
//   Assistidos   : cat_visto.
//   Coleções     : naColecao ou comprado (independe do estado).
static int salvoNaFonte(const CatItem *ci);
static int noModo(int m, const CatItem *ci, int i) {
  switch (m) {
    case MODO_PARAVER:    return (salvoNaFonte(ci) || (i >= 0 && i < CAT_MAX && serverMembership[i] &&
             (!jfid_e(ci->imdb) || serverFilter >= 0))) && !emAndamento(ci);
    case MODO_ANDAMENTO:  return emAndamento(ci);
    case MODO_ASSISTIDOS: return cat_visto(ci);
    case MODO_COLECOES:   return ci->naColecao || (i >= 0 && i < CAT_MAX && comprado[i]);
    default:              return 0;
  }
}

// 1 quando um item do catalogo ja CONTADO e o mesmo titulo que `id`. Olha os
// contados, e nao so os de `filtro`: o seletor de tipo esconde alguns, e o
// contador do modo tambem nao pode contar o mesmo titulo duas vezes.
static int contados[CAT_MAX];
static int nContados;
static int jaNoFiltro(const char *id) {
  int i;
  if (!id || !id[0]) return 0;
  for (i = 0; i < nContados; i++) {
    const CatItem *c = cat_item(contados[i]);
    if (c && salvos_mesmo_titulo(c->imdb, id)) return 1;
  }
  return 0;
}

// QUANTOS TITULOS o modo `m` tem, sem filtro de tipo: a contagem do seletor
// segmentado ("Salvos 14"). A MESMA regra de entrada e de "um cartaz por
// titulo" de reconstruir — contar diferente do que a grade mostra seria o
// numero mentindo. Usa `contados` como rascunho; reconstruir zera depois.
static int salvoNaFonte(const CatItem *ci) {
  return ci->naLista && (ci->listaFontes & (1u << fonteSalvos));
}

static int contarModo(int m) {
  int n = cat_n(), total = 0;
  if (n > CAT_MAX) n = CAT_MAX;
  nContados = 0;
  for (int i = 0; i < n; i++) {
    const CatItem *ci = cat_item(i);
    if (!ci) continue;
    if (!noModo(m, ci, i)) continue;
    if (jaNoFiltro(ci->imdb)) continue;
    if (nContados < CAT_MAX) contados[nContados++] = i;
    total++;
  }
  if (m == MODO_PARAVER && ajustes_fonte_traz_locais(fonteSalvos))
    for (int k = salvos_n() - 1; k >= 0; k--) {
      const SalvoItem *sv = salvos_item(k);
      if (sv && sv->id[0] && cat_indice_por_imdb(sv->id) < 0) total++;
    }
  return total;
}

// Refaz a lista visivel de TITULOS (modos Salvos e Coleção).
static void reconstruirFonte(int novaFonte) {
  fonteSalvos = novaFonte;
  int n = cat_n();
  int exclTipo = 0, exclMeta = 0, locaisFora = 0;
  int anoAtual = 0;
  if (ajustes_ocultar_nao_lancados()) {
    time_t agora = time(NULL);
    struct tm tmv;
    if (gmtime_r(&agora, &tmv)) anoAtual = tmv.tm_year + 1900;
  }
  if (n > CAT_MAX) n = CAT_MAX;
  memset(serverMembership, 0, sizeof serverMembership);
  if (serverContains) for (int i = 0; i < n; i++) {
    const CatItem *ci = cat_item(i);
    if (ci) for (int p = 0; p < 3; p++) if (serverContains(ci, p)) serverMembership[i] |= 1 << p;
  }
  for (int m = 0; m < MODO_LISTAS; m++) contaModo[m] = contarModo(m);
  memset(contaOrigem, 0, sizeof contaOrigem);
  memset(addonOrigens, 0, sizeof addonOrigens); nAddonOrigens = 0;
  for (int i = 0; i < n; i++) {
    const CatItem *ci = cat_item(i);
    if (!ci || !noModo(modo, ci, i) || !ci->origem[0] ||
        !strcmp(ci->origem,"plex") || !strcmp(ci->origem,"jellyfin") || !strcmp(ci->origem,"emby")) continue;
    int k;
    for (k = 0; k < nAddonOrigens; k++) if (!strcmp(addonOrigens[k].id, ci->origem)) break;
    if (k == nAddonOrigens) {
      if (nAddonOrigens == 8) continue;
      snprintf(addonOrigens[k].id, sizeof addonOrigens[k].id, "%s", ci->origem);
      const char *name = addons_nome_por_id(ci->origem);
      snprintf(addonOrigens[k].nome, sizeof addonOrigens[k].nome, "%s", name && name[0] ? name : ci->origem);
      nAddonOrigens++;
    }
    int duplicate = 0;
    for (int j = 0; j < i; j++) {
      const CatItem *before = cat_item(j);
      if (before && noModo(modo, before, j) && !strcmp(before->origem, ci->origem) && salvos_mesmo_titulo(before->imdb, ci->imdb)) { duplicate = 1; break; }
    }
    if (!duplicate) addonOrigens[k].count++;
  }
  nContados = 0;
  for (int i = 0; i < n; i++) {
    const CatItem *ci = cat_item(i);
    if (!ci || !noModo(modo, ci, i) || jaNoFiltro(ci->imdb)) continue;
    contados[nContados++] = i;
    contaOrigem[ORIG_TODAS]++;
    int bits = ci->fontes ? ci->fontes : CAT_FONTE_NUVIO;
    if (bits & CAT_FONTE_TRAKT) contaOrigem[ORIG_TRAKT]++;
    if (bits & CAT_FONTE_SIMKL) contaOrigem[ORIG_SIMKL]++;
    if (bits & CAT_FONTE_NUVIO) contaOrigem[ORIG_NUVIO]++;
  }
  nFiltro = 0;
  totalModo = 0;
  nContados = 0;
  for (int i = 0; i < n; i++) {
    const CatItem *ci = cat_item(i);
    if (!ci) continue;
    // "Salvos" = QUERO VER (watchlist do Trakt). "Coleção" = TENHO.
    //
    // Os dois modos mostravam quase a MESMA lista: ambos incluiam ci->naLista,
    // entao trocar de pilula praticamente nao mudava nada e as duas nao tinham
    // razao de existir. A divisao agora e a do proprio Trakt, que separa
    // watchlist (o que se pretende ver) de collection (o que se possui) — sao
    // perguntas diferentes e cada pilula responde uma.
    //
    // E le do ITEM, nao mais do vetor naLista[] indexado por posicao. Aquele
    // vetor era um erro conhecido e documentado: o catalogo e RECONSTRUIDO da
    // rede a cada descoberta, entao a posicao 3 de hoje e outro titulo amanha —
    // a marca "salvo" migrava sozinha para um filme que ninguem salvou.
    if (!noModo(modo, ci, i)) continue;
    if (serverFilter >= 0 && !(serverMembership[i] & (1 << serverFilter))) continue;
    // UM CARTAZ POR TITULO. O mesmo titulo vive em varias fileiras, cada uma
    // com a sua copia marcada, e a serie com progresso tem id "tt123:1:2" ao
    // lado do "tt123" da watchlist — a grade mostrava o titulo duas vezes. A
    // regra e a de salvos_mesmo_titulo, a mesma do painel de Salvos.
    if (!daOrigem(ci->fontes)) continue;
    if (origem >= BIB_N_ORIG && (origem - BIB_N_ORIG >= nAddonOrigens || strcmp(ci->origem, addonOrigens[origem - BIB_N_ORIG].id))) continue;
    if (jaNoFiltro(ci->imdb)) continue;
    if (nContados < CAT_MAX) contados[nContados++] = i;
    totalModo++;
    if (esconderVistos && modo != MODO_ASSISTIDOS && cat_visto(ci)) continue;
    if (modo != MODO_ASSISTIDOS && generoSel && !strstr(ci->genero, generos[generoSel]) && !strstr(ci->genero, generosEn[generoSel]) && !strstr(ci->genero, i18n(generos[generoSel]))) continue;
    if (tipo == TIPO_FILME && ehSerie(ci)) { exclTipo++; continue; }
    if (tipo == TIPO_SERIE && !ehSerie(ci)) { exclTipo++; continue; }
    // Mesmo criterio da Home: so ano futuro conhecido; sem meta permanece.
    if (anoAtual && nlanc_meta_futuro(ci->meta, anoAtual)) { exclMeta++; continue; }
    filtro[nFiltro++] = i;
  }
  // OS SALVOS LOCAIS QUE O CATALOGO NAO TEM. salvos_aplicar_catalogo so MARCA
  // quem esta no catalogo, de proposito (nao infla o catalogo com 15 KB por
  // titulo); mas era por isso que o titulo salvo NESTA TV sumia da Biblioteca
  // assim que a descoberta republicava o catalogo sem ele — "o que eu adiciono
  // agora nao aparece" (issue do Owlphibia29). O painel de Salvos ja desenhava
  // da lista local; a Biblioteca agora tambem. Os mais novos primeiro: a lista
  // local e na ordem de insercao, e quem acabou de salvar procura o que salvou.
  if (modo == MODO_PARAVER && ajustes_fonte_traz_locais(fonteSalvos)) {
    for (int k = salvos_n() - 1; k >= 0 && nFiltro < (int)(sizeof filtro / sizeof filtro[0]); k--) {
      const SalvoItem *s = salvos_item(k);
      if (!s || !s->id[0] || cat_indice_por_imdb(s->id) >= 0) continue;
      contaOrigem[ORIG_TODAS]++;
      contaOrigem[ORIG_NUVIO]++;
      if (serverFilter >= 0 || !daOrigem(0) || origem >= BIB_N_ORIG) continue;
      totalModo++;
      locaisFora++;
      if (generoSel) continue;
      if (esconderVistos && vistoFiltro(-k - 1)) continue;
      if (tipo == TIPO_FILME && ehSerieSalvo(s)) { exclTipo++; continue; }
      if (tipo == TIPO_SERIE && !ehSerieSalvo(s)) { exclTipo++; continue; }
      if (anoAtual && nlanc_meta_futuro(s->meta, anoAtual)) { exclMeta++; continue; }
      filtro[nFiltro++] = -k - 1;
    }
  }

  // Ordenacao por insercao — sao poucas dezenas de itens, uma vez por troca.
  if (ordem == ORD_VISTO || ordem == ORD_NAO_VISTO) {
    // Duas faixas, ESTAVEL: a ordem de adicao segue dentro de cada uma.
    static int tmpF[CAT_MAX + SALVOS_MAX];
    int quer = ordem == ORD_VISTO, k = 0;
    for (int i = 0; i < nFiltro; i++) if (!!vistoFiltro(filtro[i]) == quer) tmpF[k++] = filtro[i];
    for (int i = 0; i < nFiltro; i++) if (!!vistoFiltro(filtro[i]) != quer) tmpF[k++] = filtro[i];
    memcpy(filtro, tmpF, (size_t)nFiltro * sizeof filtro[0]);
  } else if (ordem != ORD_ADICIONADOS) {
    for (int i = 1; i < nFiltro; i++) {
      int v = filtro[i], j = i - 1;
      while (j >= 0) {
        int maior;
        if (ordem == ORD_TITULO)
          maior = strcmp(tituloFiltro(filtro[j]), tituloFiltro(v)) > 0;
        else /* ORD_ANO, decrescente */
          maior = strcmp(metaFiltro(filtro[j]), metaFiltro(v)) < 0;
        if (!maior) break;
        filtro[j + 1] = filtro[j]; j--;
      }
      filtro[j + 1] = v;
    }
  }
  remapear(0);
  // Uma linha por montagem, nao por quadro: distingue filtro de janela/rolagem.
  // As fontes remotas ja chegam fundidas em naLista; nao somar seus logs.
  printf("[biblioteca] perfil=%d modo=%d tipo=%d ocultar=%d exibicao=%d "
         "salvos=%d colecao=%d total=%d cat_unicos=%d locais_fora=%d "
         "excl_tipo=%d excl_meta=%d grade=%d colunas=%d linhas=%d teto_linhas=%d\n",
         perfis_ativo(), modo, tipo, ajustes_ocultar_nao_lancados(), exibicao,
         contaModo[MODO_PARAVER], contaModo[MODO_COLECOES], totalModo, nContados, locaisFora,
         exclTipo, exclMeta, nCelulas, colunas(), nLinhas(), BIB_MAX_LINHAS);
  fflush(stdout);
}

static void reconstruir(void) {
  reconstruirFonte(ajustes_fonte_biblioteca());
}

static void atualizarFonte(void) {
  int novaFonte = ajustes_fonte_biblioteca();
  if (novaFonte == fonteSalvos) return;
  if (modo != MODO_LISTAS) reconstruirFonte(novaFonte);
  else fonteSalvos = novaFonte;
}

// Pede a fonte escolhida ao modulo de listas. Cada uma custa no maximo UM
// pedido de rede (ver listas.h).
static void pedirFonte(void) {
  if (fonte >= FONTE_PLEX) { lst_pedir_servidores(fonte - FONTE_PLEX, 0); remapear(0); return; }
  switch (fonte) {
    case FONTE_TRAKT:   lst_pedir(LST_TRAKT); break;
    case FONTE_SIMKL:   lst_pedir(LST_SIMKL); break;
    case FONTE_NUVIO:   lst_pedir(LST_NUVIO); break;
    case FONTE_FIXADAS: lst_pedir_fixadas();  break;
    default:            lst_buscar("");       break;   // tendencias
  }
  remapear(0);
}

static int iniciado;
void biblioteca_abrir_sessao(void) {
  fonte = FONTE_TRAKT;
  nVistaAtual = nVistaAnterior = 0;
  animTela = 0;
  pedido = -1; sair = 0; pediuRelogio = 0; temAberta = 0;
  okDesde = 0;
  prefPerfil = -1;
}
int biblioteca_iniciar(void) {
  nVistaAtual = nVistaAnterior = 0;
  // Zerado UMA vez por processo, e nao a cada entrada.
  if (!iniciado) {
    memset(comprado, 0, sizeof comprado);
    iniciado = 1;
  }
  prefPerfil = -1;
  lerPref();
  lst_iniciar();
  modo = MODO_PARAVER; tipo = TIPO_TODOS; ordem = ORD_ADICIONADOS; origem = ORIG_TODAS; serverFilter = -1; trilha = 0;
  focoOrigem = focoBotaoOrig = faixaNoBotao = 0; origemSel = 0; esconderVistos = 0; generoSel = 0;
  pickSel = 0; temAberta = 0; acaoSel = 0; listaTab = 0; recado[0] = 0; recadoAte = 0.0f;
  tecPara = TEC_NADA; abertaPropria = 0; edModo = ED_NADA; ordemPega = -1;
  ordemMudou = 0; compartilha = 0;
  memset(animModo, 0, sizeof animModo);
  memset(animPick, 0, sizeof animPick);
  sair = 0; pedido = -1; pediuRelogio = 0;
  animTela = 0; temFocoListas = 0;
  reconstruir();
  // O foco nasce na barra de modos: quem entra ainda esta escolhendo o recorte.
  foco.fileira = BIB_FIL_MODO;
  foco.coluna = modo;
  return 1;
}

void biblioteca_encerrar(void) { pediuRelogio = 0; }

// So para o teste de tela conferir foco e modo sem ler pixel.
int biblioteca_teste_trilha(void) { return temAberta && trilha; }
const char *biblioteca_teste_estado(int *ed, int *acao, int *pega, int *compart, int *fileira) {
  *ed = edModo; *acao = acaoSel; *pega = ordemPega; *compart = compartilha; *fileira = foco.fileira;
  return recadoAte > 0.0f ? recado : "";
}

int biblioteca_na_lista(int i) {
  // Discovery may mark only one catalog copy; membership belongs to the title.
  atualizarFonte();
  const CatItem *c = cat_item(i);
  return c ? salvoNaFonte(c) || cat_imdb_na_lista_na_fonte(c->imdb, fonteSalvos) : 0;
}
int biblioteca_comprado(int i) { return (i >= 0 && i < CAT_MAX) ? comprado[i] : 0; }
void biblioteca_alternar_lista(int i) {
  // So remonta a lista. Quem vira a marca e cat_definir_na_lista, no mesmo
  // ponto que fala com o Trakt (app.c) — ter DOIS donos do mesmo estado era o
  // que deixava a biblioteca discordando do botao "+" do detalhe.
  (void)i;
  if (modo != MODO_LISTAS) reconstruir();
}

int biblioteca_quer_sair(void) { return sair && animTela < .005f; }
int biblioteca_pediu_relogio(void) { int v = pediuRelogio; pediuRelogio = 0; return v; }

int biblioteca_pediu_abrir(int *indiceCatalogo) {
  if (pedido < 0) return 0;
  if (indiceCatalogo) *indiceCatalogo = pedido;
  pedido = -1;
  return 1;
}

// ---------------------------------------------------------------- acoes

static void dizer(const char *s) {
  snprintf(recado, sizeof recado, "%s", s ? s : "");
  recadoAte = 4.0f;
}

// OK sobre um item vindo de uma LISTA. Ele nao esta no catalogo: quem abre o
// detalhe precisa de um indice em cat_item(), entao ou ja existe (mesmo imdb) ou
// entra no fim. E o mesmo caminho da filmografia de ator e do "Mais como este".
static void abrirItemDaLista(int i) {
  CatItem it;
  int idx;
  if (!lst_item(i, &it) || !it.imdb[0]) return;
  char serverKey[96];
  if (aberta.fonte == LST_SERVIDOR && lst_servidor_chave(&it, serverKey, sizeof serverKey)) {
    snprintf(aberta.colId, sizeof aberta.colId, "%s", serverKey);
    snprintf(aberta.titulo, sizeof aberta.titulo, "%s", it.titulo);
    lst_abrir(&aberta, abertaMidia); remapear(0); foco.fileira = 0; foco.coluna = 0; return;
  }
  idx = cat_indice_por_imdb(it.imdb);
  if (idx < 0) idx = cat_acrescentar(&it);
  if (idx >= 0) pedido = idx;
}

static void alternarExibicao(void) {
  exibicao = (exibicao + 1) % BIB_N_VIS;
  gravarPref();
  remapear(0);
}

static void fecharAberta(void) {
  static CatItem preview[4];
  int n = lst_itens_n(); if (n > 4) n = 4;
  int got = 0;
  for (int i = 0; i < n; i++) if (lst_item(i, &preview[got])) got++;
  lst_previa_registrar(&aberta, preview, got);
  { DescItemLista ex; lst_previa_resumo(&aberta, lst_item_extra(0, &ex) ? ex.nota : "", got ? pagVistos : -1); }
  temAberta = 0; trilha = 0;
  abertaPropria = 0; edModo = ED_NADA; ordemPega = -1; ordemMudou = 0;
  // O recado era da acao feita DENTRO da lista ("Adicionada à Home"). Deixa-lo
  // vivo na lista de listas o faz parecer resposta ao que a pessoa fez agora —
  // apareceu assim na captura, sobre a aba do Simkl.
  recado[0] = 0; recadoAte = 0.0f;
  remapear(0);
  if (temFocoListas) {
    foco.fileira = focoListas.fileira < foco.nFileiras ? focoListas.fileira : foco.nFileiras - 1;
    foco.coluna = focoListas.coluna < foco.nColunas[foco.fileira] ? focoListas.coluna : 0;
    temFocoListas = 0;
  } else { foco.fileira = BIB_FIL_GRADE; foco.coluna = 0; }
}

static void abrirLista(int i) {
  const LstLista *l = lst_lista(i);
  if (!l) return;
  focoListas = foco; temFocoListas = 1;
  aberta = *l;
  snprintf(abertaMidia, sizeof abertaMidia, "%s",
           l->midia[0] ? l->midia : "MOVIE");
  temAberta = 1; trilha = listaTab == 3;
  // "Minhas listas" do Trakt e /users/me/lists: sao as unicas que a pessoa edita.
  abertaPropria = modo == MODO_LISTAS && fonte == FONTE_TRAKT &&
                  aberta.fonte == LST_TRAKT && aberta.traktId > 0;
  edModo = ED_NADA; ordemPega = -1; ordemMudou = 0;
  acaoSel = 0;
  recado[0] = 0;
  lst_abrir(&aberta, abertaMidia);
  remapear(0);
  foco.fileira = 0;
  foco.coluna = 0;
}

// Conclui Ordem/Notas. A ordem movida so vai ao Trakt aqui; se o pedido nao sai
// (sem token, outra escrita em voo) a lista volta ao que o servidor tem.
static void sairEdicao(void) {
  if (edModo == ED_ORDEM && ordemMudou) {
    long ids[LST_ITENS_MAX];
    int n = lst_ordem_ids(ids, LST_ITENS_MAX);
    if (n > 0 && lstesc_ordenar(aberta.traktId, ids, n)) {
      escrevendo = 1;
      dizer("Salvando…");
    } else {
      lst_abrir(&aberta, abertaMidia);
      remapear(1);
      dizer("Não foi possível salvar a lista");
    }
  }
  edModo = ED_NADA; ordemPega = -1; ordemMudou = 0;
}

// As quatro acoes que so a lista propria tem (chips 3..6).
static void acaoPropria(int a) {
  if (a == 3) {
    tecPara = TEC_DESC;
    teclado_abrir_com("Descrição da lista", "Vazio apaga a descrição.", 190,
                      ALFA_TEXTO, aberta.descricao);
    return;
  }
  if (a == 6) {
    if (!lst_url_publica(&aberta, urlLista, sizeof urlLista)) {
      dizer("Só listas públicas têm link para compartilhar");
      return;
    }
    compartilha = 1;
    return;
  }
  if (edModo != ED_NADA) {
    int era = edModo;
    sairEdicao();
    if (era == (a == 4 ? ED_ORDEM : ED_NOTAS)) return;   // o mesmo chip: so conclui
  }
  if (a == 4) {
    long ids[LST_ITENS_MAX];
    if (lst_itens_carregando() || aberta.itens < 0 || lst_itens_n() < aberta.itens ||
        lst_itens_n() < 2 || !lst_ordem_ids(ids, LST_ITENS_MAX)) {
      dizer("Reordenar pede a lista inteira carregada, de um tipo só");
      return;
    }
    edModo = ED_ORDEM;
    dizer("OK pega o título, as setas movem, OK solta. Concluir salva");
  } else {
    if (nCelulas < 1) return;
    edModo = ED_NOTAS;
    dizer("OK no título escreve a nota dele");
  }
  foco.fileira = gradeIni(); foco.coluna = 0;
}

static void executarAcao(int a) {
  const char *porque = "";
  if (!temAberta) return;
  if (aberta.fonte == LST_TRAKT && a == nAcoes() - 1) {
    trilha = !trilha; remapear(0); foco.fileira = 0; foco.coluna = acaoSel; return;
  }
  if (a >= 3) { if (abertaPropria) acaoPropria(a); return; }
  if (a == 0) {
    dizer(lst_alternar_fixada(&aberta) ? "Fixada na Biblioteca"
                                       : "Tirada da Biblioteca");
    return;
  }
  if (a == 1) {
    if (!lst_aceita_home(&aberta, &porque)) { dizer(porque); return; }
    dizer(lst_alternar_home(&aberta) ? "Adicionada à Home"
                                     : "Tirada da Home");
    return;
  }
  // TIPO. /lists/<id>/items pede UM tipo por vez — e assim que descoberta.c
  // monta a URL, e o Simkl separa /sync/all-items/movies de /shows do mesmo
  // jeito. Entao a troca e explicita em vez de fingir uma lista mista.
  edModo = ED_NADA; ordemPega = -1; ordemMudou = 0;
  snprintf(abertaMidia, sizeof abertaMidia, "%s",
           strcasecmp(abertaMidia, "TV") ? "TV" : "MOVIE");
  lst_abrir(&aberta, abertaMidia);
  remapear(0);
  foco.coluna = acaoSel;
}

// OK on a 6D panel button: share shows the list's QR; the edits open the list
// on that chip (Descrição starts typing at once).
static void acaoPainel(int a) {
  const LstLista *l = lst_lista(painelIdx);
  if (!l) return;
  focoPainel = 0;
  if (a == 6) { if (lst_url_publica(l, urlLista, sizeof urlLista)) compartilha = 1; return; }
  abrirLista(painelIdx);
  acaoSel = a; foco.fileira = 0; foco.coluna = a;
  if (a == 3) acaoPropria(3);
}

// ---------------------------------------------------------------- eventos

// Geometria da faixa de cima, desenhada mais abaixo no arquivo.
static float larguraModo(int a);
static float pickerX(int p);
static float pickerLargura(int p);

// QUAL DOS DOIS GRUPOS DA FAIXA TINHA O FOCO por ultimo: e para ele que o Cima
// volta, em vez de sempre cair no mesmo lugar.
static int faixaNoPicker;
static int ctxListaIdx;   // a lista de onde o menu saiu; "Abrir" volta para ela

// A coluna da grade que fica embaixo do centro do item da faixa que tem o foco.
// Descer pela coluna 0 sempre fazia "Ordenar" ou "Exibicao" aterrissarem no
// primeiro cartaz, o outro lado da tela.
static int colunaSobFaixa(void) {
  float cx, passo = passoColuna();
  if (focoBotaoOrig) return 0;
  if (foco.fileira == BIB_FIL_MODO) {
    float x = modosX() + BIB_SEG_PAD;
    for (int a = 0; a < foco.coluna && a < BIB_N_MODOS; a++) x += larguraModo(a) + BIB_SEG_ITEM_GAP;
    cx = x + larguraModo(foco.coluna < BIB_N_MODOS ? foco.coluna : 0) * 0.5f;
  } else {
    cx = pickerX(foco.coluna) + pickerLargura(foco.coluna) * 0.5f;
  }
  if (passo < 1.0f) return 0;
  return (int)((cx * bibRel() - bibX()) / passo);  // cx e virtual do topo
}

static void descerDaFaixa(void) {
  int c;
  if (!nCelulas) return;
  faixaNoPicker = foco.fileira == BIB_FIL_PICK;
  faixaNoBotao = focoBotaoOrig;
  c = colunaSobFaixa();
  foco.fileira = BIB_FIL_GRADE;
  if (c >= foco.nColunas[BIB_FIL_GRADE]) c = foco.nColunas[BIB_FIL_GRADE] - 1;
  foco.coluna = c < 0 ? 0 : c;
}

// OK on the "Origens" chip. The layout takes its target at once; the posters
// glide from the slots recorded here (see the grid loop) while the island
// shrinks into the chip or grows out of it.
static float origensFlipRestante(void) {
  float distancia = (float)origensAbertas - flipDe;
  return fabsf(distancia) > .001f ? ((float)origensAbertas - animOrig) / distancia : 0.0f;
}
static void alternarOrigens(void) {
  int cols = colunas();
  float passo = passoColuna(), x = bibX(), restante = origensFlipRestante();
  origensAbertas = !origensAbertas;
  // A pointer can close the panel while a source row still owns focus.
  if (!origensAbertas && focoOrigem) {
    focoOrigem = 0; focoBotaoOrig = 1;
    foco.fileira = BIB_FIL_PICK; foco.coluna = pickSel;
  }
  for (int i = 0; i < nCelulas; i++) {
    flipOffset[i][0] = flipOffset[i][0] * restante + x + (i % cols) * passo
                    - (bibX() + (i % colunas()) * passoColuna());
    flipOffset[i][1] = flipOffset[i][1] * restante + (i / cols - i / colunas()) * passoLinha();
  }
  flipDe = animOrig;
  gravarPref();
  remapear(1);
}

static void subirParaFaixa(void) {
  if (faixaNoPicker) {
    foco.fileira = BIB_FIL_PICK; foco.coluna = pickSel;
    focoBotaoOrig = faixaNoBotao && botaoOrigens();
  }
  else               { foco.fileira = BIB_FIL_MODO; foco.coluna = modo; }
}

// O indice no CATALOGO do titulo da celula `i` (grade de titulos ou itens de
// uma lista aberta), acrescentando-o ao fim quando ele nao esta la — o detalhe
// e o menu do cartaz trabalham por indice. -1 quando nao ha titulo.
static int indiceDaCelula(int i) {
  CatItem it;
  const CatItem *ci;
  int idx;
  if (estado() == EST_LISTAS || i < 0 || i >= nCelulas) return -1;
  if (estado() == EST_ITENS) {
    if (!lst_item(i, &it) || !it.imdb[0]) return -1;
    ci = &it;
  } else {
    if (filtro[i] >= 0) return filtro[i];
    ci = itemFiltro(filtro[i], &it);
    if (!ci || !ci->imdb[0]) return -1;
  }
  idx = cat_indice_por_imdb(ci->imdb);
  if (idx < 0) idx = cat_acrescentar(ci);
  return idx;
}

static int celulaEmFoco(void) {
  int i = (foco.fileira - gradeIni()) * colunas() + foco.coluna;
  return (foco.fileira >= gradeIni() && i >= 0 && i < nCelulas) ? i : -1;
}

// O OK CURTO NUMA CELULA: abre. Roda no KEYUP (ver okDesde): so ali se sabe se
// foi toque ou pressao longa.
static void okNaCelula(int i) {
  if (i < 0 || i >= nCelulas) return;
  if (estado() == EST_ITENS && edModo == ED_ORDEM) {
    ordemPega = ordemPega == i ? -1 : i;
    return;
  }
  if (estado() == EST_ITENS && edModo == ED_NOTAS) {
    DescItemLista x;
    if (!lst_item_extra(i, &x) || x.itemId <= 0) { dizer("Este título não aceita nota"); return; }
    notaItemId = x.itemId;
    tecPara = TEC_NOTA;
    teclado_abrir_com("Nota do título", "Vazio apaga a nota.", 190, ALFA_TEXTO, x.nota);
    return;
  }
  if (estado() == EST_LISTAS) { abrirLista(i); return; }
  if (estado() == EST_ITENS) { abrirItemDaLista(i); return; }
  { int idx = indiceDaCelula(i);
    if (idx >= 0) pedido = idx; }
}

// SEGURAR OK NUMA CELULA: o menu de contexto. Cartaz de titulo/item = o MESMO
// menu do cartaz da home (ctxmenu.c); cartao de lista = o menu da lista, com o
// resumo do que tem nela.
static void menuNaCelula(int i) {
  if (i < 0 || i >= nCelulas || edModo != ED_NADA) return;
  if (estado() == EST_LISTAS) {
    const LstLista *l = lst_lista(i);
    if (l) { ctxListaIdx = i; ctx_abrir_lista(l); }
    return;
  }
  { int idx = indiceDaCelula(i);
    CatItem tmp;
    const CatItem *ci;
    if (idx < 0) return;
    ctx_fileira(NULL, NULL);
    ci = estado() == EST_ITENS ? (lst_item(i, &tmp) ? &tmp : NULL) : itemFiltro(filtro[i], &tmp);
    if (!modoLinha() && ci) {
      // O poster volta por cima do veu, onde esta na tela agora.
      float esc = 1.0f + BIB_FOCO_ESCALA, w = BIB_CARD_W * esc, h = BIB_POSTER_H * esc;
      int nc = colunas(), r = i / nc, c = i % nc;
      float x = bibX() + c * passoColuna() + (BIB_CARD_W - w) * 0.5f;
      float y = gradeY() + r * passoLinha() - scrollY + (BIB_POSTER_H - h) * 0.5f;
      const char *arte = posterprov_card_addon(ci->origem, ci->imdb, ci->tmdb, ci->tipo, ci->poster);
      ctx_abrir_cartaz(idx, (GfxRect){ x, y, w, h }, arte);
    } else ctx_abrir(idx);
  }
}

// O OK da grade e medido: KEYDOWN arma, KEYUP decide (toque x pressao longa), e
// biblioteca_atualizar dispara o menu no limiar com o dedo ainda no botao — a
// mesma mecanica da home (home.c), com o mesmo NV_HOLD_MS. Setas cancelam.

static void eventoAberta(SDL_Keycode k) {
  // TITULO PEGO (Ordem): as setas MOVEM o titulo e prendem o foco nele, ate o OK
  // soltar. Lista de uma coluna so anda por cima/baixo.
  if (edModo == ED_ORDEM && ordemPega >= 0 && foco.fileira >= gradeIni()) {
    int nc = colunas(), d = 0, para;
    if (k == SDLK_DOWN)  d = nc;
    if (k == SDLK_UP)    d = -nc;
    if (k == SDLK_RIGHT && nc > 1) d = 1;
    if (k == SDLK_LEFT  && nc > 1) d = -1;
    if (k == SDLK_RIGHT || k == SDLK_LEFT || k == SDLK_UP || k == SDLK_DOWN) {
      para = ordemPega + d;
      if (d && lst_ordem_mover(ordemPega, para)) {
        ordemPega = para; ordemMudou = 1;
        foco.fileira = gradeIni() + para / nc; foco.coluna = para % nc;
      }
      return;
    }
  }
  if (trilha && (k == SDLK_UP || k == SDLK_DOWN) && foco.fileira >= 1) {
    if (k == SDLK_UP) { foco.fileira = 0; foco.coluna = acaoSel; }
    else {
      if (foco.fileira < foco.nFileiras - 1) focus_mover_grade(&foco, 0, 1);
      if (foco.fileira >= foco.nFileiras - 2) lst_itens_mais();
    }
    return;
  }
  if (paginaLista() && !trilha && foco.fileira >= 1 && k == SDLK_LEFT) {
    foco.fileira = 0; foco.coluna = acaoSel; return;
  }
  if (foco.fileira == 0 && focoCont) {
    if (k == SDLK_DOWN) focoCont = 0;
    else if (k == SDLK_UP) { if (ajustes_menu_topo()) sair = 1; else pediuRelogio = 1; }
    else if (k == SDLK_LEFT) sair = 1;
    else if (k == SDLK_RIGHT && nCelulas) { focoCont = 0; foco.fileira = 1; foco.coluna = 0; }
    else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) abrirItemDaLista(pagProximo);
    return;
  }
  if (foco.fileira == 0) {
    if (k == SDLK_UP && temContinuar()) { focoCont = 1; return; }
    if (paginaLista() && !trilha && k == SDLK_RIGHT && acaoSel == nAcoes() - 1 && nCelulas) {
      foco.fileira = 1; foco.coluna = 0; return;
    }
    if (k == SDLK_UP) { if (ajustes_menu_topo()) sair = 1; else pediuRelogio = 1; return; }
    if (k == SDLK_RIGHT && acaoSel < nAcoes() - 1) { acaoSel++; foco.coluna = acaoSel; return; }
    if (k == SDLK_LEFT  && acaoSel > 0) { acaoSel--; foco.coluna = acaoSel; return; }
    if (k == SDLK_LEFT) { sair = 1; return; }   // comeco da fileira: o app abre a barra
    if (k == SDLK_DOWN) { if (nCelulas) { foco.fileira = 1; foco.coluna = 0; } return; }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) executarAcao(acaoSel);
    return;
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) return;  // KEYUP
  // Open lists can exceed the fixed focus map. Navigate their regular grid
  // directly, without allocating another map per title.
  int row = foco.fileira - 1, nc = colunas();
  int cols = nCelulas - row * nc;
  if (cols > nc) cols = nc;
  if (k == SDLK_RIGHT) { if (foco.coluna + 1 < cols) foco.coluna++; }
  else if (k == SDLK_LEFT) { if (foco.coluna > 0) foco.coluna--; else sair = 1; }
  else if (k == SDLK_DOWN) {
    if (row + 1 < nLinhas()) {
      foco.fileira++;
      int rest = nCelulas - (row + 1) * nc;
      if (foco.coluna >= rest) foco.coluna = rest - 1;
    }
    // Chegou ao fim do que baixou: pede a proxima pagina. O modulo recusa
    // sozinho quando a anterior veio curta.
    if (foco.fileira >= foco.nFileiras - 2) lst_itens_mais();
  } else if (k == SDLK_UP) {
    if (foco.fileira == 1) { foco.fileira = 0; foco.coluna = acaoSel; }
    else foco.fileira--;
  }
}

static void biblioteca_evento_(const SDL_Event *e) {
  pediuRelogio = 0;
  // O TECLADO DA BUSCA E MODAL: enquanto ele esta na tela, ele fica com TODAS
  // as teclas. Deixar a grade responder por baixo foi o defeito que a busca de
  // codigo de amigo ja teve.
  if (teclado_aberto()) { teclado_evento(e); return; }
  // O QR do link e um aviso: qualquer tecla (nova) o fecha e e so isso.
  if (compartilha) {
    if (e->type == SDL_KEYDOWN && !e->key.repeat) compartilha = 0;
    return;
  }
#ifdef NV_TOUCH_UI
  if (e->type == SDL_KEYDOWN) toqueBibliotecaRetomarFoco();
#endif
  { SDL_Keycode kk = e->key.keysym.sym;
    int ehOk = kk == SDLK_RETURN || kk == SDLK_KP_ENTER || kk == SDLK_SPACE;
    // SOLTAR O OK: se ele foi armado numa celula e o menu nao abriu, foi toque.
    // Sem armar (o KEYDOWN foi na faixa, ou antes de entrar nesta tela) nao ha
    // clique nenhum — a mesma guarda da home.
    if (e->type == SDL_KEYUP && ehOk) {
      Uint32 desde = okDesde;
      okDesde = 0;
      if (desde) {
        if (ponteiro_ok_longo()) menuNaCelula(celulaEmFoco());
        else okNaCelula(celulaEmFoco());
      }
      return;
    }
    if (e->type == SDL_KEYDOWN && ehOk && !e->key.repeat) {
      int dentro = !focoOrigem && !focoPainel && (estado() == EST_ITENS ? foco.fileira >= 1 : foco.fileira >= BIB_FIL_GRADE);
      okDesde = dentro && celulaEmFoco() >= 0 ? (SDL_GetTicks() | 1u) : 0;
    } else if (e->type == SDL_KEYDOWN && !ehOk) okDesde = 0;
  }
  if (e->type != SDL_KEYDOWN) return;
  SDL_Keycode k = e->key.keysym.sym;
  if (estadoFoco && estadoFalhou()) {
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
      if (temAberta) lst_abrir(&aberta, abertaMidia); else pedirFonte();
      estadoFoco = 0; return;
    }
    if (k == SDLK_UP || nv_tecla_voltar(e)) { estadoFoco = 0; return; }
    return;
  }
  if (k == SDLK_DOWN && estadoFalhou() && !focoOrigem &&
      (temAberta || foco.fileira == BIB_FIL_PICK)) { estadoFoco = 1; return; }
  if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE) {
    // Dentro de uma lista, o Back volta PARA A LISTA DE LISTAS antes de fechar
    // a tela — o movimento inverso do que levou ate la.
    if (!temAberta && modo == MODO_LISTAS) { modo = MODO_PARAVER; focoOrigem = 0; reconstruir(); }
    else if (!temAberta) sair = 1;
    else if (ordemPega >= 0) ordemPega = -1;       // solta; a edicao continua
    else if (edModo != ED_NADA) sairEdicao();
    else fecharAberta();
    return;
  }

  if (estado() == EST_ITENS) { eventoAberta(k); return; }
  if (focoPainel) {
    int ids[4], n = painelAcoes(lst_lista(painelIdx), ids);
    int pilulas = n && ids[n - 1] == 6 ? n - 1 : n;   // the share button sits below
    if (k == SDLK_LEFT && painelSel > 0 && painelSel < pilulas) painelSel--;
    else if (k == SDLK_LEFT || !n) focoPainel = 0;
    else if (k == SDLK_RIGHT && painelSel + 1 < pilulas) painelSel++;
    else if (k == SDLK_DOWN && painelSel < pilulas && n > pilulas) painelSel = n - 1;
    else if (k == SDLK_UP && painelSel >= pilulas && pilulas) painelSel = 0;
    else if ((k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) && painelSel < n) acaoPainel(ids[painelSel]);
    return;
  }
  if (focoOrigem) {
    if (k == SDLK_UP && origemSel > 0) origemSel--;
    else if (k == SDLK_UP) { focoOrigem = 0; foco.fileira = BIB_FIL_PICK; focoBotaoOrig = botaoOrigens(); }
    else if (k == SDLK_DOWN && origemSel < (modo == MODO_LISTAS ? BIB_N_FONTES : nOrigens()) - 1) origemSel++;
    else if (k == SDLK_RIGHT) { focoOrigem = 0; foco.fileira = BIB_FIL_GRADE; foco.coluna = 0; }
    else if (k == SDLK_LEFT) sair = 1;
    else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
      if (modo == MODO_LISTAS) {
        if (origemSel >= 0 && origemSel < BIB_N_FONTES) { fonte = origemSel; pedirFonte(); }
      }
      else if (origemSel >= BIB_N_ORIG + nAddonOrigens) {
        if (serverSelect) serverSelect(origemSel - BIB_N_ORIG - nAddonOrigens);
      } else { serverFilter = -1; origem = origemSel; reconstruir(); }
    }
    return;
  }

  // A FAIXA DE CIMA E UMA LINHA SO (modos + seletores, lado a lado desde a
  // Glass UI), entao esquerda/direita ANDA por ela de ponta a ponta: do ultimo
  // modo para o primeiro seletor e de volta. Cima/baixo e que mudam de linha:
  // baixo desce para a grade, na coluna que fica embaixo do que estava em foco.
  // Internamente continuam duas "fileiras" de foco (MODO e PICK) porque cada
  // uma tem o proprio vetor de animacao; o D-pad que as trata como uma so.
  //
  // Barra de modos: esquerda/direita MOVE O FOCO entre as abas e, na ultima,
  // segue para o primeiro seletor; OK escolhe a aba em foco. Antes a seta
  // TROCAVA o modo, e isso era o que impedia o lado: para chegar nos seletores
  // a partir de "Salvos" era preciso atravessar "Coleção" e "Listas", trocando
  // de aba (e pedindo a rede, nas listas) a cada passo.
  if (modo == MODO_LISTAS && foco.fileira == BIB_FIL_MODO) {
    if (k == SDLK_UP) { if (ajustes_menu_topo()) sair = 1; else pediuRelogio = 1; return; }
    if (k == SDLK_LEFT && foco.coluna > 0) foco.coluna--;
    else if (k == SDLK_LEFT) { modo = MODO_PARAVER; reconstruir(); }
    else if (k == SDLK_RIGHT && foco.coluna < 3) foco.coluna++;
    else if (k == SDLK_DOWN) { foco.fileira = BIB_FIL_PICK; foco.coluna = pickSel; }
    else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
      listaTab = foco.coluna;
      fonte = listaTab == 1 || listaTab == 3 ? FONTE_FIXADAS : listaTab == 2 ? FONTE_PUB : FONTE_TRAKT;
      pedirFonte(); foco.fileira = BIB_FIL_MODO; foco.coluna = listaTab;
    }
    return;
  }
  if (foco.fileira == BIB_FIL_MODO) {
    if (k == SDLK_UP) { if (ajustes_menu_topo()) sair = 1; else pediuRelogio = 1; return; }
    if (k == SDLK_LEFT  && foco.coluna > 0) { foco.coluna--; return; }
    if (k == SDLK_LEFT) { sair = 1; return; }   // 1a aba: o app abre a barra
    if (k == SDLK_RIGHT && foco.coluna < BIB_N_MODOS - 1) { foco.coluna++; return; }
    if (k == SDLK_RIGHT) return;
    if (k == SDLK_DOWN) { foco.fileira = BIB_FIL_PICK; foco.coluna = pickSel; return; }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
      int novo = foco.coluna;
      if (novo != modo) {
        modo = novo;
        if (modo == MODO_ASSISTIDOS) exibicao = VIS_LISTA;
        recado[0] = 0;
        if (modo == MODO_LISTAS) { listaTab = 0; fonte = FONTE_TRAKT; pedirFonte(); }
        else reconstruir();
        foco.fileira = BIB_FIL_MODO; foco.coluna = modo == MODO_LISTAS ? listaTab : modo;
      }
    }
    return;
  }

  // Seletores: esquerda/direita anda ENTRE os tres e, na ponta esquerda, volta
  // para a barra de modos; OK cicla o valor do que esta em foco. O web abre um
  // menu suspenso; num D-pad, ciclar no proprio seletor poupa a viagem de ida
  // e volta ate a lista.
  if (foco.fileira == BIB_FIL_PICK && focoBotaoOrig) {
    int ok = k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE;
    if (k == SDLK_RIGHT) { focoBotaoOrig = 0; pickSel = 0; foco.coluna = 0; }
    else if (k == SDLK_LEFT) sair = 1;
    else if (k == SDLK_UP) { focoBotaoOrig = 0; foco.fileira = BIB_FIL_MODO; foco.coluna = modo; }
    else if (k == SDLK_DOWN && origensAbertas) { focoBotaoOrig = 0; focoOrigem = 1; origemSel = origem; }
    else if (k == SDLK_DOWN && nCelulas) { descerDaFaixa(); focoBotaoOrig = 0; }
    else if (ok) alternarOrigens();
    return;
  }
  if (foco.fileira == BIB_FIL_PICK) {
    if (k == SDLK_RIGHT && pickSel < bibNPicks() - 1) { pickSel++; foco.coluna = pickSel; return; }
    if (k == SDLK_LEFT  && pickSel > 0) { pickSel--; foco.coluna = pickSel; return; }
    if (k == SDLK_LEFT && botaoOrigens()) { focoBotaoOrig = 1; return; }
    if (k == SDLK_LEFT)  { foco.fileira = BIB_FIL_MODO; foco.coluna = modo == MODO_LISTAS ? listaTab : BIB_N_MODOS - 1; return; }
    if (k == SDLK_UP) { foco.fileira = BIB_FIL_MODO; foco.coluna = modo == MODO_LISTAS ? listaTab : modo; return; }
    if (k == SDLK_DOWN) {
      if (origensNaGrade() && pickSel == 0) { focoOrigem = 1; origemSel = modo == MODO_LISTAS ? fonte : origem; }
      else descerDaFaixa();
      return;
    }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
      if (modo == MODO_LISTAS) {
        int ps = pickId(pickSel);
        if (ps == 0) {
          fonte = (fonte + 1) % BIB_N_FONTES;
          pedirFonte();
        } else if (ps == 1) alternarExibicao();
        else if (ps == 3) {
          // NOVA LISTA (fonte Trakt): so o nome; a descricao se edita depois,
          // dentro da lista. Nasce privada, como no Trakt.
          if (!trakt_ativo()) dizer("Conecte sua conta do Trakt em Ajustes para editar listas");
          else {
            tecPara = TEC_NOVA;
            teclado_abrir_com("Nome da nova lista", "Ela nasce privada no Trakt.", 60,
                              ALFA_TEXTO, NULL);
          }
        } else {
          // O terceiro seletor no modo Listas E a busca publica. Ele abre a
          // modal de digitacao que o app ja tem (src/teclado.c) em vez de uma
          // segunda grade de letras — ver a nota no topo daquele arquivo.
          fonte = FONTE_PUB;
          tecPara = TEC_BUSCA;
          teclado_abrir("Procurar listas públicas",
                        "Listas de qualquer pessoa no Trakt", TECLADO_MAX);
        }
      } else {
        if (modo == MODO_ASSISTIDOS) {
          if (pickSel == 0) { origem = (origem + 1) % BIB_N_ORIG; reconstruir(); }
          else if (pickSel == 1) { tipo = (tipo + 1) % BIB_N_TIPOS; reconstruir(); }
          else if (pickSel == 2) { ordem = (ordem + 1) % BIB_N_ORD; reconstruir(); }
          else { exibicao = pickSel == 4 ? VIS_LISTA : VIS_CARTAZ; gravarPref(); remapear(0); }
        } else if (pickSel == 0) { tipo = (tipo + 1) % BIB_N_TIPOS; reconstruir(); }
        else if (pickSel == 1) { generoSel = (generoSel + 1) % (sizeof generos / sizeof generos[0]); reconstruir(); }
        else if (pickSel == 2) { ordem = (ordem + 1) % BIB_N_ORD; reconstruir(); }
        else if (pickSel == 3) { esconderVistos = !esconderVistos; reconstruir(); }
        else { exibicao = pickSel == 5 ? VIS_LISTA : VIS_CARTAZ; gravarPref(); remapear(0); }
      }
      foco.fileira = BIB_FIL_PICK; foco.coluna = pickSel;
    }
    return;
  }

  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) return;  // KEYUP
  // A grade da biblioteca e uma GRADE: manter a coluna ao subir e descer, e
  // nao voltar para a coluna onde o cursor esteve por ultimo naquela linha.
  if (k == SDLK_RIGHT && listasMinhas()) {
    int ids[4];
    if (painelAcoes(lst_lista(painelIdx), ids)) { focoPainel = 1; painelSel = 0; }
  }
  else if (k == SDLK_RIGHT)     focus_mover_grade(&foco, 1, 0);
  else if (k == SDLK_LEFT) {
    if (!focus_mover_grade(&foco, -1, 0)) {
      if (origensNaGrade()) { focoOrigem = 1; origemSel = modo == MODO_LISTAS ? fonte : origem; }
      else sair = 1;
    }
  }
  else if (k == SDLK_DOWN) focus_mover_grade(&foco, 0, 1);
  else if (k == SDLK_UP) {
    if (foco.fileira == BIB_FIL_GRADE) { faixaNoPicker = 1; subirParaFaixa(); }
    else focus_mover_grade(&foco, 0, -1);
  }
}

// ---------------------------------------------------------------- atualizar

// O QUE O TECLADO DIGITOU, conforme para que ele abriu. As escritas so
// disparam aqui; o resultado volta em pollEscrita.
static void tecladoPronto(int para, const char *txt) {
  int ok = 0;
  if (para == TEC_BUSCA) { lst_buscar(txt); remapear(0); return; }
  if (para == TEC_NOVA) {
    if (!txt[0]) { dizer("Dê um nome à lista"); return; }
    ok = lstesc_criar(txt, "", 1);
  } else if (para == TEC_DESC && temAberta) {
    // O texto so fica quando o pedido saiu: lstesc_descricao recusa (0) com
    // outra escrita em voo, e sobrescrever descNova ali destruiria a descricao
    // do pedido ANTERIOR, que ainda vai chegar com 200 e seria espelhada errada
    // na tela e nas tabelas. descNova e o texto do pedido ACEITO, nao o ultimo
    // digitado.
    ok = lstesc_descricao(aberta.traktId, txt);
    if (ok) snprintf(descNova, sizeof descNova, "%s", txt);
  } else if (para == TEC_NOTA && temAberta) {
    ok = lstesc_nota(aberta.traktId, notaItemId, txt);
  }
  if (ok) { escrevendo = 1; dizer("Salvando…"); }
  else dizer("Não foi possível salvar a lista");
}

// A escrita desta tela terminou: diz o que houve e refaz o que mudou. So olha o
// que ELA disparou (`escrevendo`); a escrita do menu do titulo tem dono proprio.
static void pollEscrita(void) {
  int est, q, mesma;
  if (!escrevendo) return;
  est = lstesc_estado();
  if (est == LSTESC_PENDENTE) return;
  if (est != LSTESC_OK && est != LSTESC_FALHA) { escrevendo = 0; return; }
  q = lstesc_qual();
  mesma = temAberta && aberta.fonte == LST_TRAKT &&
          aberta.traktId == lstesc_lista_id();
  lstesc_consumir();
  escrevendo = 0;
  if (est == LSTESC_FALHA) {
    // A ordem na tela nao e a do servidor: recarrega a dele.
    if (q == LSTESC_ORDENAR && mesma) { lst_abrir(&aberta, abertaMidia); remapear(1); }
    dizer("Não foi possível salvar a lista");
    return;
  }
  if (q == LSTESC_CRIAR) {
    lst_minhas_pedir();
    if (modo == MODO_LISTAS && !temAberta && fonte == FONTE_TRAKT) pedirFonte();
    dizer("Lista criada");
  } else if (q == LSTESC_DESCRICAO) {
    lst_descricao_atualizar(lstesc_lista_id(), descNova);
    if (mesma) snprintf(aberta.descricao, sizeof aberta.descricao, "%s", descNova);
    dizer("Descrição salva");
  } else if (q == LSTESC_ORDENAR) {
    // Recarrega: o rank que cada linha mostra vem do servidor.
    if (mesma) { lst_abrir(&aberta, abertaMidia); remapear(1); }
    dizer("Ordem salva");
  } else if (q == LSTESC_NOTA) {
    if (mesma) { lst_abrir(&aberta, abertaMidia); remapear(1); }
    dizer("Nota salva");
  }
}

static void biblioteca_atualizar_(float dt, Uint32 agora) {
  int linhas, r, c, ini = gradeIni();
#ifdef NV_TOUCH_UI
  if (!layout_modo_mobile()) toqueLivre = 0;
#endif
  animTela = anim_mola(animTela, sair ? 0 : 1, dt, NV_MOLA_TELA);
  animVista = anim_rampa(animVista, 1, dt, 160);
  animEstado = anim_mola(animEstado, estadoFoco && estadoFalhou(), dt, NV_MOLA_FOCO);
  animListaTab = anim_mola(animListaTab, listaTab, dt, NV_MOLA_FOCO);
  compartA = anim_mola(compartA, compartilha, dt, NV_MOLA_TELA);
  compartForma = mov_mola_assenta(&compartFormaV, compartForma, compartilha, dt, MOV_MODAL_W, MOV_MODAL_Z);
  // The tab focus pill slides from tab to tab; off the row it rests on the chosen one.
  animModoPos = anim_mola(animModoPos, !focoOrigem && foco.fileira == BIB_FIL_MODO && estado() != EST_ITENS
                                       ? (float)foco.coluna : (float)modo, dt, NV_MOLA_FOCO);
  animListaEd = anim_mola(animListaEd, !focoOrigem && foco.fileira == BIB_FIL_MODO, dt, NV_MOLA_FOCO);
  for (int i = 0; i < 64; i++)
    animOrigem[i] = anim_mola(animOrigem[i], focoOrigem && origemSel == i, dt, NV_MOLA_FOCO);
  if (!listasMinhas()) focoPainel = 0;
  for (int i = 0; i < 4; i++)
    animPainel[i] = anim_mola(animPainel[i], focoPainel && painelSel == i, dt, NV_MOLA_FOCO);
  // The panel follows the focused row; a new list fades its content in.
  { int i = celulaEmFoco();
    if (listasMinhas() && i >= 0 && i != painelIdx) { painelIdx = i; animPainelTroca = 0; }
    if (painelIdx >= nCelulas) painelIdx = -1;
    animPainelTroca = anim_rampa(animPainelTroca, 1, dt, 160); }
  lerPref();
  atualizarFonte();
  if (teclado_aberto()) teclado_atualizar(dt, agora);
  if (tecPara != TEC_NADA && !teclado_aberto()) {
    // O resultado e CONSUMIDO NA LEITURA (contrato de teclado.h): perguntar
    // duas vezes disparava duas buscas para o mesmo OK.
    int r2 = teclado_resultado(), para = tecPara;
    tecPara = TEC_NADA;
    if (r2 == TECLADO_PRONTO) tecladoPronto(para, teclado_texto());
  }
  pollEscrita();
  if (recadoAte > 0.0f) recadoAte -= dt;
  // SEGUROU O OK NA CELULA ATE O LIMIAR: abre o menu agora, com o dedo ainda no
  // botao, e desarma — o KEYUP seguinte nao e toque. O modal ignora o OK que
  // ainda esta afundado (ctxmenu.c, esperandoSoltura).
  if (!ctx_aberto() && segurar_venceu(SDL_GetTicks(), okDesde, "biblioteca")) {
    int i = celulaEmFoco();
    okDesde = 0;
    menuNaCelula(i);
  }
  // A lista foi fixada/desfixada pelo menu: a grade de Fixadas se refaz.
  if (ctx_lista_alterou() && modo == MODO_LISTAS && !temAberta && fonte == FONTE_FIXADAS)
    pedirFonte();
  if (ctx_pediu_lista()) abrirLista(ctxListaIdx);

  // A GRADE CRESCE SOZINHA quando a rede entrega mais. Sem remapear, as linhas
  // novas existem no modulo e o foco nao alcanca nenhuma delas.
  if (contarCelulas() != nCelulas || colunas() != ncMapa) remapear(1);

  for (int a = 0; a < BIB_N_MODOS; a++) {
    float alvo = (estado() != EST_ITENS && foco.fileira == BIB_FIL_MODO
                  && foco.coluna == a) ? 1.0f : 0.0f;
    animModo[a] = anim_mola(animModo[a], alvo, dt,
                            alvo > animModo[a] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  }
  if (!temContinuar() || foco.fileira != 0) focoCont = 0;
  animCont = anim_mola(animCont, focoCont, dt, focoCont ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  if (!botaoOrigens() || foco.fileira != BIB_FIL_PICK) focoBotaoOrig = 0;
  animBotaoOrig = anim_mola(animBotaoOrig, focoBotaoOrig, dt, focoBotaoOrig ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  animOrig = mov_mola_assenta(&velOrig, animOrig, (float)origensAbertas, dt, MOV_MODAL_W, MOV_MODAL_Z);
  for (int p = 0; p < 8; p++) {
    float alvo = (foco.fileira == (estado() == EST_ITENS ? 0 : BIB_FIL_PICK)
                  && foco.coluna == p && !focoCont && !focoBotaoOrig) ? 1.0f : 0.0f;
    animPick[p] = anim_mola(animPick[p], alvo, dt,
                            alvo > animPick[p] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  }
  linhas = nLinhas();
  if (linhas > BIB_MAX_LINHAS) linhas = BIB_MAX_LINHAS;
  for (r = 0; r < linhas; r++)
    for (c = 0; c < colunas() && c < BIB_COLUNAS_MAX; c++) {
      float alvo = focus_indice(&foco, ini + r, c) && !focoPainel ? 1.0f : 0.0f;
      animFoco[r][c] = anim_mola(animFoco[r][c], alvo, dt,
                                 alvo > animFoco[r][c] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
    }

  // Rola o MINIMO para a linha focada caber inteira na area util. Com o foco no
  // cabecalho o alvo e 0 — voltar ao topo faz parte de voltar para a barra.
  float alvo = scrollY;
  if (trilha) {
    alvo = 0;
    int index = celulaEmFoco();
    float step = BIB_CARD_W + BIB_CARD_GAP;
    float target = trilhaX;
    if (index >= 0) {
      float left = (index % 8) * step;
      if (left < target) target = left;
      if (left + BIB_CARD_W > target + bibW()) target = left + BIB_CARD_W - bibW();
    }
    trilhaX = anim_mola2(&trilhaVel, trilhaX, fmaxf(0, target), dt, NV_MOLA2_SCROLL);
  }
  else if (foco.fileira >= ini) {
    float topo = gradeY() + (foco.fileira - ini) * passoLinha();
    float base = topo + alturaLinha();
    if (base - alvo > BIB_GRADE_BASE)  alvo = base - BIB_GRADE_BASE;
    if (topo - alvo < gradeY())        alvo = topo - gradeY();
  } else {
    alvo = 0.0f;
  }
  if (alvo < 0.0f) alvo = 0.0f;
#ifdef NV_TOUCH_UI
  if (toqueLivre) scrollY = anim_clamp(scrollY, 0.0f, toqueBibliotecaMax());
  else
#endif
    scrollY = anim_mola2(&velY, scrollY, alvo, dt, NV_MOLA2_SCROLL);
}

// ---------------------------------------------------------------- desenho

// TRADUCAO: NAO HA i18n() NESTE BLOCO DE DESENHO, e nao e esquecimento. Quem
// traduz e text.c — todo txt_linha/txt_linha_corta/txt_bloco passa a string por
// i18n antes de rasterizar (ver a nota no topo de idioma.h). Chamar i18n aqui
// seria traduzir duas vezes. As UNICAS chamadas a i18n neste arquivo estao
// dentro de snprintf: uma frase MONTADA nunca casa com uma chave, entao ali as
// PARTES tem de ser traduzidas antes de serem coladas.
//
// A SUPERFICIE DE FOCO DESTE APP, numa funcao so: pilula PREENCHIDA na cor de
// realce com texto ESCURO, sem anel. Decisao do dono em 16/09/2026, registrada
// em menu.c ("os botoes quando selecionados ficar brancos com o texto preto ...
// e pode tirar o contorno"). Aqui ela vale para modos, seletores e acoes.
//
// Escolhida-mas-sem-foco continua com fundo #303030 e um anel discreto: sem
// esse terceiro estado, descer o foco para a grade apaga a indicacao de qual
// recorte esta valendo — o defeito das abas de temporada do detalhe.
// A TINTA SECUNDARIA dos dois lados da superficie, na escada de texto do
// DESIGN.md. #9699A2 (150,153,162) e o PISO de contraste sobre o fundo escuro
// (~5,6:1) e e a grafia que o documento manda usar em codigo novo — este
// arquivo tinha 179,179,179 e 168 cinza puro, que e deriva.
//
// Sobre a superficie DE REALCE a tinta secundaria vem de bibTinta2():
// 238 sobre realce colorido (a 3 m, 225 ja lia como cinza sobre rosa — dono,
// 21/09) e 60 sobre realce branco. A principal e bibTinta(), 255 ou
// 20 — e so ela chega aqui como 255 ou 20: as superficies de repouso escrevem
// em 235/200, entao o valor da principal diz em que superficie o texto esta.
static void tintaSecundaria(int principal, int *r, int *g, int *b) {
  if (principal == bibTinta()) {
    *r = *g = *b = bibTinta2();
    return;
  }
  *r = 150; *g = 153; *b = 162;
}

// NAO HA ANEL AQUI, e essa e a correcao de 16/09 (segunda passada). O dono
// olhou a captura e foi direto ao ponto: "o botao selecionado tem ser fill sem
// contorno". A regra da casa nao abre excecao para "escolhido mas sem foco" —
// era exatamente o que este arquivo desenhava, pilula #303030 com um anel claro
// em volta, e era o unico contorno que sobrou numa superficie preenchivel.
//
// OS TRES ESTADOS PASSAM A SER TRES PREENCHIMENTOS da MESMA familia, que e o
// que permite tirar o anel sem perder a distincao:
//
//   escolhido + foco   -> a cor de realce CHEIA
//   escolhido sem foco -> a mesma cor a 60%, misturada com o fundo da pagina
//   nao escolhido      -> #222, como sempre foi
//
// A TINTA DO TEXTO SOBRE O REALCE E bibTinta(), e nao uma conta
// local. Este arquivo tinha a sua propria luminancia com degrau em 0,42, que
// punha texto PRETO sobre rosa e sobre amarelo; a regra do dono (21/09/2026)
// e uma so para o app inteiro — branco sobre qualquer realce, escuro so sobre
// realce branco — e mora em ajustes.c para nao haver duas versoes dela.
//
// Sobre o realce a 60% (escolhida sem foco) a mesma tinta continua certa: 60%
// de uma cor e mais escuro que a cor, entao branco ganha contraste; e 60% de
// branco e cinza 153, onde o escuro que ajustes_tinta_foco devolve para o
// realce branco ainda le.

// Quanto do realce sobra no estado "escolhido, mas o foco esta noutro lugar".
// 0.60 medido na captura: abaixo disso ele se aproxima demais do #222 dos nao
// escolhidos a 3 m; acima, fica perto demais do focado.
#define BIB_ESCOLHIDA_DIM 0.60f

// O BRILHO ATRAS DO FOCO: a mancha difusa na cor de realce que faz a pilula
// "acender" em vez de so trocar de cor — a mesma medida do menu lateral e do
// topo do guia (0,9x a altura de folga por lado, 2,8x a altura no total, alfa
// 0,35 x mola). GFX_SOMBRA e uma mancha radial, entao numa pilula larga e
// baixa ela sai mais forte nas pontas do que no meio, e e assim no menu
// tambem. FILL-RATE: a mancha de uma linha de 1728x168 mede ~2030x470, quase
// meia tela — e so UMA linha esta em foco por vez, o que cabe na regra de
// gfx.h de no maximo uma tela cheia a mais por quadro.
//
// `folga` e quanto a mancha ultrapassa a pilula, em fracao da altura dela: 0,9
// para as pilulas de 56 (a medida do menu) e 0,3 para as linhas de 168 — com
// 0,9 a mancha de uma linha subia 150 px pela vizinha de cima e descia outros
// 150 pela de baixo, e as duas saiam com uma faixa clara atravessada no meio,
// visto na captura. 0,3 (50 px) para em cima da fresta de 12 e do rebordo.
static void brilhoFoco(GfxRect r, float folga, float f, float a) {
  float ar, ag, ab;
  GfxRect luz;
  if (f <= 0.01f) return;
  if (ajustes_vidro()) return;   // vidro: a pilula cheia sem mancha
  ajustes_acento(&ar, &ag, &ab);
  luz.x = r.x - r.h * folga; luz.y = r.y - r.h * folga;
  luz.w = r.w + r.h * folga * 2.0f; luz.h = r.h * (1.0f + folga * 2.0f);
  gfx_rect(luz, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, ar, ag, ab, 0.35f * f * a);
}

// CHIP (filtro, acao de lista): o chipFolha da folha de Fontes. Repouso em
// branco a 8 % (vidro) ou cinza opaco (solido); `escolhida` (estado ligado:
// "Fixada", "Na Home") no acento a 22 % com o texto no acento; FOCO = pilula
// cheia no acento com a tinta de foco. Devolve a tinta do texto.
static int pilula(GfxRect r, float raio, float f, int escolhida) {
  float ar, ag, ab, v = anim_clamp(f, 0.0f, 1.0f);
  ajustes_acento(&ar, &ag, &ab);
  if (escolhida) gfx_cor(r, raio, ar, ag, ab, .22f * (1.0f - v));
  else if (ajustes_vidro()) gfx_vidro_folha(r, raio, 1.0f - v);
  else gfx_cor(r, raio, .082f, .086f, .102f, 1.0f - v);
  if (v > 0.01f) {
    if (ajustes_vidro()) gfx_vidro_pilula_cheia(r, raio, v, 1.0f);
    else { brilhoFoco(r, 0.9f, v, 1.0f); gfx_cor(r, raio, ar, ag, ab, v); }
  }
  if (v > 0.5f) return ajustes_tinta_foco();
  return escolhida ? 250 : 245;
}

// A SUPERFICIE DE UMA LINHA DA LISTA: em repouso um degrau acima da pagina
// (branco a 5 % no vidro, 0.10/0.11/0.13 no solido); em foco a superficie um
// degrau mais clara (branco 12 % / cinza opaco), sem realce nem brilho. Devolve
// a tinta principal do texto (clara nos dois estados).
static int superficieLinha(GfxRect r, float raio, float f, float a) {
  plrui_linha_foco(r, raio * r.h, anim_clamp(f, 0, 1) * a);
  return 235;
}

// O WORDMARK DO TRAKT, no lugar da palavra "TRAKT" composta com a fonte da
// interface. Pedido do dono: "vamos usar a logo do trakt a que e escrito
// trakt". A arte ja esta versionada em art/marcas/trakt_wordmark.png.
//
// GFX_MARCA, e nao desenho de imagem comum, e o arquivo justifica: MEDIDO, ele
// tem UMA unica cor opaca (255,255,255) sobre alfa — e um recorte monocromatico,
// que e como a Trakt distribui o wordmark. Num recorte assim a forma vem do
// ALFA e a cor vem de quem desenha, entao ele pode receber a TINTA DA SUPERFICIE
// em que esta. Isso nao e recolorir marca por capricho: sem tingir, o wordmark
// branco sobre a pilula clara do foco SOME — e o mesmo light-on-light que este
// app ja levou duas vezes. Com o RGB do arquivo (GFX_CARD) sairia pior ainda: o
// modo de cartao ignora o alfa e pinta uma CAIXA atras das letras, que e o
// defeito que detail.c ja registrou.
//
// A ALTURA MANDA e a largura sai do aspecto REAL do arquivo — cravar as duas
// deformaria o desenho se a arte for trocada. Devolve a largura desenhada, ou 0
// quando a textura ainda nao chegou: ai quem chama escreve a palavra, porque um
// selo que pisca enquanto decodifica e pior que um selo de texto.
// Largura que o wordmark ocupa numa dada altura, ou 0 enquanto a textura nao
// chegou. Separada do desenho porque o selo do cabecalho e alinhado a DIREITA:
// ele precisa da largura antes de saber onde comecar a desenhar.
static float marcaTraktLargura(float h) {
  const char *cam = extras_caminho_marca_nome("trakt_wordmark");
  float ap;
  if (!cam || !cam[0] || !tex_obter_larg(cam, 220.0f)) return 0.0f;
  ap = tex_aspecto(cam);
  if (ap <= 0.0f) ap = 320.0f / 122.0f;
  return h * ap;
}

static float marcaTrakt(float x, float y, float h, int tinta, float alpha) {
  const char *cam = extras_caminho_marca_nome("trakt_wordmark");
  GLuint t;
  float w, c;
  if (!cam || !cam[0]) return 0.0f;
  t = tex_obter_larg(cam, 220.0f);
  if (!t) return 0.0f;
  w = marcaTraktLargura(h);
  c = (float)tinta / 255.0f;
  gfx_tex_aspect_atual = 0.0f;
  gfx_rect((GfxRect){ x, y, w, h }, t, GFX_MARCA, 0, 0, 0, 0.0f, c, c, c, alpha);
  return w;
}

// TEXTO REDUZIDO: a textura de um estilo desenhada em `esc` do tamanho. O
// mockup usa corpos que o text.c nao tem (17, 18, 19,5); um estilo novo la seria
// conflito com quem mexe em text.c em paralelo, entao se rasteriza no estilo
// mais proximo ACIMA e reduz — reduzir 5-15% com GL_LINEAR nao borra.
static void txtEsc(TxtLinha l, float x, float y, float esc, float a) {
  GfxRect r;
  if (!l.tex || a <= 0.001f) return;
  r.x = floorf(x + 0.5f); r.y = floorf(y + 0.5f);
  r.w = (float)l.w * esc; r.h = (float)l.h * esc;
  gfx_rect(r, l.tex, GFX_TEXTO, 0, 0, 0, 0.0f, 1, 1, 1, a);
}
// Corpos da faixa (medidos no mockup, ver BIB_TOPO): rotulo e valor dos chips
// 18,5 (17 no CSS), nome do modo 20,5 (19), contagem 17,5 (16), titulo do
// cartaz 19,5 (18). Os 600 do CSS (valor do chip, nome do modo) saem em BOLD,
// pela regra de text.c para 600 sobre escuro: o Bold de 18 para o valor e o de
// 33 reduzido para o nome do modo — o Medium deixava o seletor magro ao lado do
// mockup.
#define BIB_ESC_ROT  (18.5f / NV_FT_CAPTION2)
#define BIB_ESC_VAL  1.0f   /* TXT_HERO_SEC, 18 bold */
#define BIB_ESC_MODO (20.5f / NV_FT_ROW_TITULO)
#define BIB_ESC_N    (17.5f / NV_FT_CAPTION2)
#define BIB_ESC_TIT  (19.5f / NV_FT_HERO_META)

// O SELETOR SEGMENTADO DOS MODOS (.seg do mockup, o mesmo das Fontes e do
// Social): conteiner pilula com 5 de recuo, cada modo com 20 de recuo e a
// CONTAGEM em cinza ao lado ("Salvos 14"), o modo aberto num segmento um
// degrau mais claro e, com o foco nele, a pilula cheia no acento. As larguras
// saem do texto. A contagem de Listas so aparece quando o modulo ja tem as
// listas da fonte; antes disso um "0" seria mentira.
static void textoModo(int a, char *num, size_t n) {
  int v = a == MODO_LISTAS ? lst_n() : contaModo[a];
  num[0] = 0;
  if (a != MODO_LISTAS || v > 0) snprintf(num, n, "%d", v);
}
// Contagem de cada aba para o plrui_seg (-1 = sem numero).
static int contagemModo(int a) {
  char num[16];
  textoModo(a, num, sizeof num);
  return num[0] ? atoi(num) : -1;
}
static float larguraModo(int a) {
  char num[16];
  if (!bibRetrato()) {
    // Match the shared rimless track's label, padding and count gap.
    float w = (float)txt_linha(TXT_AJ_SEG, ROT_MODO[a], 243, 243, 243, 255).w + 44.0f;
    int n = contagemModo(a);
    if (n >= 0) {
      snprintf(num, sizeof num, "%d", n);
      w += 10.0f + (float)txt_linha(TXT_ILHA_NUM, num, 243, 243, 243, 255).w;
    }
    return w;
  }
  float w = (float)txt_linha(TXT_ROW_TITULO, ROT_MODO[a], 140, 140, 140, 255).w * BIB_ESC_MODO;
  textoModo(a, num, sizeof num);
  if (num[0]) w += 9.0f + (float)txt_linha(TXT_CAPTION2, num, 93, 93, 93, 255).w * BIB_ESC_N;
  w += BIB_SEG_ITEM_PADX * 2.0f;
  return bibRetrato() ? fminf(w, (hdrW() - BIB_SEG_PAD * 2.0f - BIB_SEG_ITEM_GAP * (BIB_N_MODOS - 1)) / BIB_N_MODOS) : w;
}
static float larguraSeletor(void) {
  float w = BIB_SEG_PAD * 2.0f + BIB_SEG_ITEM_GAP * (BIB_N_MODOS - 1);
  for (int a = 0; a < BIB_N_MODOS; a++) w += larguraModo(a);
  return w;
}
// PONTEIRO (#99). Poe o foco pelas MESMAS variaveis que as setas mexem em
// biblioteca_evento; o OK do clique chega depois, pelo caminho de sempre (aba
// escolhe, seletor cicla, celula abre ou, segurado, abre o menu do cartaz).
static void ponteiroModo(int a, int b) {
#ifdef NV_TOUCH_UI
  toqueLivre = 0;
#endif
  (void)b;
  if (estado() == EST_ITENS || a < 0 || a >= BIB_N_MODOS) return;
  foco.fileira = BIB_FIL_MODO; foco.coluna = a; focoBotaoOrig = 0;
}
static void ponteiroPicker(int p, int b) {
#ifdef NV_TOUCH_UI
  toqueLivre = 0;
#endif
  (void)b;
  if (estado() == EST_ITENS || p < 0 || p >= bibNPicks()) return;
  pickSel = p; foco.fileira = BIB_FIL_PICK; foco.coluna = p; focoBotaoOrig = 0;
}
static void ponteiroBotaoOrig(int a, int b) {
  (void)a; (void)b;
  if (botaoOrigens()) { focoOrigem = 0; foco.fileira = BIB_FIL_PICK; focoBotaoOrig = 1; }
}
static void ponteiroRetry(int a, int b) { (void)a; (void)b; estadoFoco = 1; }
static void ponteiroAcao(int a, int b) {
#ifdef NV_TOUCH_UI
  toqueLivre = 0;
#endif
  (void)b;
  if (estado() != EST_ITENS || a < 0 || a >= nAcoes()) return;
  acaoSel = a; foco.fileira = 0; foco.coluna = a; focoCont = 0;
}
static void ponteiroCont(int a, int b) { (void)a; (void)b; foco.fileira = 0; foco.coluna = acaoSel; focoCont = 1; }
static void ponteiroCelula(int i, int b) {
  int nc = colunas();
  (void)b;
  if (i < 0 || i >= nCelulas || nc < 1) return;
#ifdef NV_TOUCH_UI
  toqueLivre = 0;
#endif
  foco.fileira = gradeIni() + i / nc; foco.coluna = i % nc; focoBotaoOrig = 0;
}

static void desenhaModos(void) {
  if (modo == MODO_LISTAS) {
    // Same rimless seg as the Library tabs; the open tab counts its lists.
    int cont[4] = { -1, -1, -1, -1 };
    if (!lst_carregando() && nCelulas) cont[listaTab] = nCelulas;
    plrui_seg_ex(ROT_LISTA_TAB, cont, 4, listaTab, animListaTab, animListaEd, hdrX(), BIB_FAIXA_Y, 1, 1);
    float x = hdrX() + 5;
    for (int i = 0; i < 4; i++) {
      float w = txt_linha(TXT_AJ_SEG, ROT_LISTA_TAB[i], 243, 243, 243, 255).w + 44;
      ponteiro_alvo(x, BIB_FAIXA_Y + 5, w, 48, ponteiroModo, NULL, i, 0);
      x += w + 4;
    }
    return;
  }
  if (!bibRetrato()) {
    // The shared segmented control (LINGUAGEM 2): chosen tab = .sel, the
    // accent pill follows the focus.
    int cont[BIB_N_MODOS];
    float x = modosX() + BIB_SEG_PAD;
    for (int a = 0; a < BIB_N_MODOS; a++) cont[a] = contagemModo(a);
    plrui_seg_ex(ROT_MODO, cont, BIB_N_MODOS, modo, animModoPos, animListaEd,
                 modosX(), BIB_FAIXA_Y, 1, 1);
    for (int a = 0; a < BIB_N_MODOS; a++) {
      float w = larguraModo(a);
      ponteiro_alvo(x, BIB_FAIXA_Y + BIB_SEG_PAD, w, 48, ponteiroModo, NULL, a, 0);
      x += w + BIB_SEG_ITEM_GAP;
    }
    return;
  }
  GfxRect c = { hdrX(), BIB_FAIXA_Y, larguraSeletor(), BIB_SEG_H };
  float x = c.x + BIB_SEG_PAD, ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  if (ajustes_vidro()) gfx_vidro_folha(c, 0.5f, 1.0f);
  else gfx_cor(c, 0.5f, .113f, .118f, .137f, 1.0f);          // #1d1e23
  for (int a = 0; a < BIB_N_MODOS; a++) {
    float w = larguraModo(a), v = anim_clamp(animModo[a], 0.0f, 1.0f);
    GfxRect r = { x, c.y + BIB_SEG_PAD, w, BIB_SEG_H - BIB_SEG_PAD * 2.0f };
    int sel = a == modo, t, tn;
    char num[16];
    TxtLinha l, ln;
    ponteiro_alvo(r.x, r.y, r.w, r.h, ponteiroModo, NULL, a, 0);
    if (sel) {
      if (ajustes_vidro()) gfx_cor(r, 0.5f, 1, 1, 1, .14f * (1.0f - v));
      else gfx_cor(r, 0.5f, .204f, .212f, .243f, 1.0f - v);  // #34363e
    }
    // O FOCO E INDEPENDENTE DA ESCOLHA: a aba em foco acende mesmo sem ser a
    // escolhida (a seta so move o foco; OK escolhe).
    if (v > 0.01f) {
      if (ajustes_vidro()) gfx_vidro_pilula_cheia(r, 0.5f, v, 1.0f);
      else { brilhoFoco(r, 0.9f, v, 1.0f); gfx_cor(r, 0.5f, ar, ag, ab, v); }
    }
    t  = v > 0.5f ? ajustes_tinta_foco() : sel ? 255 : 140;
    tn = v > 0.5f ? ajustes_tinta_foco2() : sel ? 118 : 93;
    textoModo(a, num, sizeof num);
    ln = (TxtLinha){0};
    if (bibRetrato()) {
      float util = r.w - BIB_SEG_ITEM_PADX * 2.0f;
      if (num[0]) ln = txt_linha_corta(TXT_CAPTION2, num, tn, tn, tn, 255, util * .4f / BIB_ESC_N);
      l = txt_linha_corta(TXT_ROW_TITULO, ROT_MODO[a], t, t, t, 255,
                          (util - (num[0] ? 9.0f + (float)ln.w * BIB_ESC_N : 0.0f)) / BIB_ESC_MODO);
    } else l = txt_linha(TXT_ROW_TITULO, ROT_MODO[a], t, t, t, 255);
    txtEsc(l, r.x + BIB_SEG_ITEM_PADX, r.y + (r.h - (float)l.h * BIB_ESC_MODO) * 0.5f,
           BIB_ESC_MODO, 1.0f);
    if (num[0]) {
      // Na linha de BASE do nome (align-items: baseline), nao no meio dele.
      if (!bibRetrato()) ln = txt_linha(TXT_CAPTION2, num, tn, tn, tn, 255);
      txtEsc(ln, r.x + BIB_SEG_ITEM_PADX + (float)l.w * BIB_ESC_MODO + 9.0f,
             r.y + (r.h + (float)l.h * BIB_ESC_MODO) * 0.5f - (float)ln.h * BIB_ESC_N - 1.0f,
             BIB_ESC_N, 1.0f);
    }
    x += w + BIB_SEG_ITEM_GAP;
  }
}

static void pickerTexto(int p, const char **rot, const char **val) {
  if (modo == MODO_LISTAS) {
    p = pickId(p);
    *rot = p == 0 ? "Fonte" : p == 1 ? "Exibição" : p == 2 ? "Listas públicas" : "+ Nova lista";
    *val = p == 0 ? rotuloOrigem(fonte, 1) : p == 1 ? ROT_VIS[exibicao] : p == 2 ? "Procurar" : "nome no celular";
  } else if (modo == MODO_ASSISTIDOS) {
    *rot = p == 0 ? "Origem" : p == 1 ? "Tipo" : p == 2 ? "Ordem" : "";
    const char *nomeOrigem = rotuloOrigem(origem, 0);
    *val = p == 0 ? nomeOrigem : p == 1 ? ROT_TIPO[tipo] : p == 2 ? ROT_ORD[ordem] : "";
  } else {
    *rot = p == 0 ? "Tipo" : p == 1 ? "Gênero" : p == 2 ? "Ordem" : p == 3 ? "Esconder assistidos" : "";
    *val = p == 0 ? (tipo == TIPO_TODOS ? "Filmes e séries" : ROT_TIPO[tipo])
         : p == 1 ? generos[generoSel] : p == 2 ? (ordem == ORD_ADICIONADOS ? "Adicionados por último" : ROT_ORD[ordem]) : "";
  }
}

// O CHIP DE FILTRO (.vid do mockup, raio 24, 12/20 de recuo): o rotulo em
// cinza e o valor em branco 600, separados por um espaco — o "·" que existia
// aqui saiu com o mockup. A largura sai do texto.
// Grade/lista: um .seg so de dois itens (pad 5, vao 4, item 64 x 44).
#define BIB_VIS_W  64.0f
#define BIB_VIS_TW (BIB_VIS_W * 2.0f + 4.0f + 10.0f)
// "Esconder assistidos" ligado: o ponto (7) e o vao (10) antes do rotulo.
static int pickerPonto(int p) { return modo != MODO_LISTAS && modo != MODO_ASSISTIDOS && p == 3 && esconderVistos; }
static float pickerLarguraNat(int p) {
  if (modo != MODO_LISTAS && p >= bibViewStart()) return BIB_VIS_W;
  const char *rot, *val;
  pickerTexto(p, &rot, &val);
  return (float)txt_linha(TXT_HERO_SEC, rot, 245, 245, 245, 255).w + 8.0f
       + (float)txt_linha(TXT_ILHA_SUB, val, 117, 117, 117, 255).w
       + BIB_CHIP_PADX * 2.0f + (pickerPonto(p) ? 17.0f : 0.0f);
}

// Folga a direita para o anel de foco (brilhoFoco passa da borda do chip).
#define BIB_FAIXA_FOLGA 16.0f

// A faixa cabe numa linha so se seletor + fio + todos os chips (com a folga do
// anel) ficam dentro da largura util. "Origem: Conta Nuvio" com contagens
// grandes passava de 1920 a 120% (x=1803-2054). Medido com os chips naturais,
// sem o teto do retrato, para nao depender do proprio resultado.
static int bibFaixaQuebra(void) { return 1; }

// "Origens" chip, first of the filter row: Lucide panel-left + the label, the
// same rimless .chip as the filters. OK opens/closes the panel below it.
#define BIB_BOTAO_ICONE 22.0f
static float larguraBotaoOrig(void) {
  return BIB_BOTAO_ICONE + 10.0f + BIB_CHIP_PADX * 2.0f
       + (float)txt_linha(TXT_HERO_SEC, "Origens", 245, 245, 245, 255).w;
}
static void desenhaBotaoOrig(void) {
  GfxRect r = { hdrX(), pickerY(), larguraBotaoOrig(), BIB_CHIP_H };
  float v = anim_clamp(animBotaoOrig, 0.0f, 1.0f);
  int t = v > 0.5f ? ajustes_tinta_foco() : 240;
  ponteiro_alvo(r.x, r.y, r.w, r.h, ponteiroBotaoOrig, NULL, 0, 0);
  if (ajustes_vidro()) gfx_cor(r, 0.5f, 1, 1, 1, .07f * (1.0f - v));
  else gfx_cor(r, 0.5f, .110f, .114f, .133f, 1.0f - v);    // #1c1d22
  plrui_pilula_foco(r, v);
  gfx_icone((GfxRect){ r.x + BIB_CHIP_PADX, r.y + (r.h - BIB_BOTAO_ICONE) * 0.5f, BIB_BOTAO_ICONE, BIB_BOTAO_ICONE },
            "aj_panel-left", t / 255.0f, t / 255.0f, t / 255.0f, 1);
  TxtLinha l = txt_linha(TXT_HERO_SEC, "Origens", t, t, t, 255);
  txt_desenhar(l, r.x + BIB_CHIP_PADX + BIB_BOTAO_ICONE + 10.0f, r.y + (r.h - (float)l.h) * 0.5f);
}

static float pickerLargura(int p) {
  float w = pickerLarguraNat(p);
  if (bibRetrato()) return fminf(w, bibControleMax());
  return bibFaixaQuebra() ? fminf(w, hdrW() - BIB_FAIXA_FOLGA) : w;
}

static float pickerX(int p) {
  if (modo != MODO_LISTAS && p >= bibViewStart())
    return hdrDir() - BIB_VIS_TW + 5.0f + (float)(p - bibViewStart()) * (BIB_VIS_W + 4.0f);
  float x = hdrX() + (bibFaixaQuebra() ? 0.0f : larguraSeletor() + BIB_FAIXA_GAP * 2.0f + 1.0f);
  int i;
  if (botaoOrigens()) x += larguraBotaoOrig() + BIB_FAIXA_GAP;
  for (i = 0; i < p; i++) x += pickerLargura(i) + BIB_FAIXA_GAP;
  return x;
}

static void desenhaPicker(int p, float f) {
  const char *rot, *val;
  float w = pickerLargura(p);
  GfxRect r = { pickerX(p), pickerY(), w, BIB_CHIP_H };
  float v = anim_clamp(f, 0.0f, 1.0f);
  int vid = ajustes_vidro();
  if (modo != MODO_LISTAS && p >= bibViewStart()) {
    // The mock's grid/list .seg: rimless track, chosen = .sel, focus = accent pill.
    int grid = p == bibViewStart();
    int sel = grid != modoLinha();
    r = (GfxRect){ r.x, pickerY() + (BIB_CHIP_H - 44.0f) * 0.5f, BIB_VIS_W, 44.0f };
    ponteiro_alvo(r.x, r.y, r.w, r.h, ponteiroPicker, NULL, p, 0);
    if (grid) {
      GfxRect tr = { r.x - 5.0f, r.y - 5.0f, BIB_VIS_TW, 54.0f };
      if (vid) gfx_cor(tr, .5f, 1, 1, 1, .06f);
      else gfx_cor(tr, .5f, .110f, .114f, .133f, 1);        // #1c1d22
    }
    if (sel) {
      if (vid) gfx_cor(r, .5f, 1, 1, 1, .14f * (1.0f - v));
      else gfx_cor(r, .5f, .204f, .212f, .243f, 1.0f - v);
    }
    plrui_pilula_foco(r, v);
    int cor = v > .5f ? ajustes_tinta_foco() : sel ? 255 : 140;
    gfx_icone((GfxRect){ r.x + (r.w - 22) * 0.5f, r.y + 11, 22, 22 },
              grid ? "aj_layout-dashboard" : "aj_layout-list",
              cor / 255.0f, cor / 255.0f, cor / 255.0f, 1);
    return;
  }
  ponteiro_alvo(r.x, r.y, r.w, r.h, ponteiroPicker, NULL, p, 0);
  // .chip of the mock: rimless light veil at rest, full accent pill on focus.
  if (vid) gfx_cor(r, 0.5f, 1, 1, 1, .07f * (1.0f - v));
  else gfx_cor(r, 0.5f, .110f, .114f, .133f, 1.0f - v);    // #1c1d22
  plrui_pilula_foco(r, v);
  float px = 0.0f;
  if (pickerPonto(p)) {
    float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
    if (v > .5f) ar = ag = ab = ajustes_tinta_foco() / 255.0f;
    gfx_cor((GfxRect){r.x + BIB_CHIP_PADX, r.y + (r.h - 7) * 0.5f, 7, 7}, .5f, ar, ag, ab, 1);
    px = 17.0f;
  }
  // Mockup: rotulo em negrito claro, valor apagado e normal.
  { int valor = v > 0.5f ? ajustes_tinta_foco2() : 125;
    int rotulo = v > 0.5f ? ajustes_tinta_foco() : 240;
    TxtLinha tr, tv;
    float x = r.x + BIB_CHIP_PADX + px;
    pickerTexto(p, &rot, &val);
    if (bibRetrato()) {
      float util = r.w - BIB_CHIP_PADX * 2.0f - px;
      tr = txt_linha_corta(TXT_HERO_SEC, rot, rotulo, rotulo, rotulo, 255, util * .55f);
      tv = txt_linha_corta(TXT_ILHA_SUB, val, valor, valor, valor, 255,
                           util - (float)tr.w - 8.0f);
    } else {
      tr = txt_linha(TXT_HERO_SEC, rot, rotulo, rotulo, rotulo, 255);
      tv = txt_linha(TXT_ILHA_SUB, val, valor, valor, valor, 255);
    }
    txt_desenhar(tr, x, r.y + (r.h - (float)tr.h) * 0.5f);
    x += (float)tr.w + 8.0f;
    txt_desenhar(tv, x, r.y + (r.h - (float)tv.h) * 0.5f); }
}

// Barra de acoes de uma lista aberta: fixar, levar para a Home e trocar o tipo
// de midia. Na faixa dos filtros, como CHIPS do mesmo corpo deles (a largura sai
// do texto) — eram tres pilulas de 520/520/300 x 72, o dobro de qualquer botao
// do mockup.
static float larguraAcao(const char *rot) {
  float w = (float)txt_linha(TXT_HERO_SEC, rot, 245, 245, 245, 255).w * BIB_ESC_VAL
          + BIB_CHIP_PADX * 2.0f + 8.0f;
  return bibRetrato() ? fminf(w, (hdrW() - BIB_FAIXA_GAP * (float)(nAcoes() - 1)) / (float)nAcoes()) : w;
}
static void desenhaAcoes(void) {
  float x = hdrX(), ay = BIB_FAIXA_Y, y0 = ay + 316;
  int a;
  if (temContinuar()) {
    DescItemLista ex; char s[64];
    snprintf(s, sizeof s, "%s · %d", i18n("Continuar"),
             lst_item_extra(pagProximo, &ex) && ex.rank > 0 ? ex.rank : pagProximo + 1);
    ponteiro_alvo(x, y0, plrui_botao_largura(s, "aj_play"), 60, ponteiroCont, NULL, 0, 0);
    plrui_botao(x, y0, s, "aj_play", animCont, 1);
    y0 += 76;
  }
  for (a = 0; a < nAcoes(); a++) {
    GfxRect r = { x, ay, 0.0f, BIB_CHIP_H };
    const char *rot;
    int ligada;
    int cor;
    if (a == 0)      { ligada = lst_fixada(&aberta);
                       rot = ligada ? "Fixada na Biblioteca" : "Fixar na Biblioteca"; }
    else if (a == 1) { const char *porque;
                       ligada = lst_na_home(&aberta);
                       rot = !lst_aceita_home(&aberta, &porque) ? "Adicionar à Home"
                           : ligada ? "Na Home" : "Adicionar à Home"; }
    else if (a == 2) { ligada = 0;
                       rot = strcasecmp(abertaMidia, "TV") ? "Filmes" : "Séries"; }
    else if (a == 3) { ligada = 0; rot = "Descrição"; }
    else if (a == 4) { ligada = edModo == ED_ORDEM; rot = ligada ? "Concluir ordem" : "Ordem"; }
    else if (a == 5) { ligada = edModo == ED_NOTAS; rot = ligada ? "Concluir notas" : "Notas"; }
    else             { ligada = 0; rot = "Compartilhar"; }
    if (aberta.fonte == LST_TRAKT && a == nAcoes() - 1) {
      ligada = trilha; rot = trilha ? "Lista" : "Trilha de filmes";
    }
    r.w = larguraAcao(rot);
    if (paginaLista() && !trilha) {
      if (x + r.w > hdrX() + 510) { x = hdrX(); ay += BIB_CHIP_H + 12; }
      r.x = x; r.y = ay - BIB_FAIXA_Y + y0;
    }
    ponteiro_alvo(r.x, r.y, r.w, r.h, ponteiroAcao, NULL, a, 0);
    cor = pilula(r, 0.5f, animPick[a], ligada);
    { TxtLinha l = bibRetrato()
        ? txt_linha_corta(TXT_HERO_SEC, rot, cor, cor, cor, 255, (r.w - BIB_CHIP_PADX * 2.0f) / BIB_ESC_VAL)
        : txt_linha(TXT_HERO_SEC, rot, cor, cor, cor, 255);
      txtEsc(l, r.x + (r.w - (float)l.w * BIB_ESC_VAL) * 0.5f,
             r.y + (r.h - (float)l.h * BIB_ESC_VAL) * 0.5f, BIB_ESC_VAL, 1.0f); }
    x += r.w + BIB_FAIXA_GAP;
  }
  // 6B footer note under the actions: where the pin puts the list.
  if (paginaLista() && !trilha)
    txt_desenhar(txt_linha_corta(TXT_MINI, "Fixar guarda a lista em Biblioteca › Listas › Fixadas.", 115, 117, 122, 255, 510),
                 hdrX(), ay - BIB_FAIXA_Y + y0 + BIB_CHIP_H + 20);
}

// O selo de origem e desenhado em CAIXA ALTA (letter-spacing 4, como no web).
// "TRAKT" e "LOCAL" ja sao caixa alta em qualquer idioma; "Conta" nao.
//
// POR QUE NAO ESCREVER "CONTA" DIRETO. A chave da tabela de traducao e o
// portugues como ele se escreve, e "Conta" ja esta la (idioma_tab.h -> a
// "Account"). Um literal "CONTA" seria uma SEGUNDA chave para a mesma palavra,
// que alguem teria de lembrar de traduzir de novo — e tools/varredura-i18n.py
// acusa exatamente isso. Traduz primeiro, levanta a caixa depois.
//
// A subida e so de a..z: as duas traducoes de hoje sao ASCII, e uma letra
// acentuada que nao subir fica minuscula em vez de virar outra letra — que e o
// que aconteceria mexendo em bytes de UTF-8 um a um.
static const char *seloCaixaAlta(const char *chave) {
  static char buf[32];
  const char *s = i18n(chave);
  size_t k = 0;
  for (; s[k] && k + 1 < sizeof buf; k++)
    buf[k] = (s[k] >= 'a' && s[k] <= 'z') ? (char)(s[k] - 'a' + 'A') : s[k];
  buf[k] = 0;
  return buf;
}

// Estado vazio: 46/500 branco e 28/400 rgb(179,179,179), centrado na largura
// util. Uma grade em branco parece tela quebrada.
static void desenhaVazio(void) {
  const char *l1, *l2, *dica = "↑ Voltar aos filtros   ·   Voltar: menu";
  int kind = T21_EMPTY;
  if (estado() == EST_ITENS) {
    // DENTRO DE UMA LISTA o aviso da FONTE nao se aplica: ele fala do pedido
    // que traz as listas, nao dos itens desta. Reusa-lo aqui dizia "conecte sua
    // conta do Trakt" dentro de uma lista publica que nao precisa de conta
    // nenhuma — visto na captura, nao deduzido.
    dica = "↑ Voltar às ações   ·   Voltar: lista de listas";
    if (lst_itens_carregando()) { kind = T21_LOADING; l1 = "Carregando os títulos"; l2 = "Um instante."; }
    else if (aberta.fonte != LST_SIMKL && desc_vertudo_erro()) {
      kind = T21_ERROR; l1 = "Não foi possível carregar. OK para tentar novamente."; l2 = NULL;
    } else { l1 = "Nada nesta lista";
           l2 = "Uma lista do Trakt vem por tipo: experimente trocar entre Filmes e Séries."; }
  } else if (estado() == EST_LISTAS) {
    // O AVISO DA FONTE VEM DO MODULO e diz o que falta (sem conta do Trakt, sem
    // vinculo do Simkl, nada fixado). Uma frase generica aqui esconderia
    // exatamente a informacao que resolve.
    const char *av = lst_aviso();
    if (lst_carregando()) { kind = T21_LOADING; l1 = "Carregando listas"; l2 = "Um instante."; }
    else if (av[0])       { kind = strstr(av, "Não foi possível") ? T21_ERROR : T21_EMPTY;
                            l1 = "Nada para mostrar aqui"; l2 = av; }
    else                  { l1 = "Nada para mostrar aqui";
                            l2 = "Escolha outra fonte no seletor acima."; }
  } else {
    // "Salvos" com o "+" apontado para o Simkl e sem vinculo (issue #110): a
    // aba vazia diz o que falta, e nao "sua proxima sessao comeca aqui".
    const char *semSimkl = modo == MODO_PARAVER && !totalModo
        ? simkl_aviso_sem_vinculo(ajustes_salvos_no_simkl()) : NULL;
    l1 = semSimkl ? semSimkl
        : totalModo ? "Nenhum título neste filtro"
        : modo == MODO_COLECOES ? "Sua coleção aparece aqui"
        : modo == MODO_ANDAMENTO ? "Nada em andamento"
        : modo == MODO_ASSISTIDOS ? "Nada assistido ainda"
        : "Sua próxima sessão começa aqui";
    l2 = semSimkl
        ? "O + salva no Plan to Watch do Simkl, e ele ainda não está vinculado nesta TV."
        : totalModo
        ? "Em Tipo, escolha Todos. Confira também os filtros em Ajustes."
        : modo == MODO_COLECOES
          ? "Os filmes e séries da sua coleção no Trakt ficam reunidos nesta aba."
        : modo == MODO_ANDAMENTO
          ? "O que você começa a ver aparece aqui para retomar de onde parou."
        : modo == MODO_ASSISTIDOS
          ? "O que você termina de ver fica registrado aqui."
          : "Abra um filme ou série e escolha Adicionar à lista para guardar.";
  }
  float y = fmaxf(gradeY(), ANC(360 - bibSubida()));
  GfxRect panel = {bibX(), y, fminf(bibW(), 1240), fminf(440, ANC(T21_SAFE_B) - y)};
  t21_estado(panel, kind, "menu_library", l1, l2, NULL, SDL_GetTicks() * .001f, 1);
  if (kind == T21_ERROR) {
    const char *label = i18n("Recarregar");
    GfxRect button = {panel.x + T21_SHEET_PAD, panel.y + panel.h - 96,
                      botao_largura(label, "aj_rotate-cw", 0), BOTAO_H_SECUNDARIO};
    botao_pilula(button, label, "aj_rotate-cw", animEstado, 0, 1, 1);
    ponteiro_alvo(button.x, button.y, button.w, button.h, ponteiroRetry, NULL, 0, 0);
  } else txt_bloco_corta(TXT_CAPTION2, dica, 150, 153, 162, panel.x + T21_SHEET_PAD,
                         panel.y + panel.h - 72, panel.w - 2 * T21_SHEET_PAD, 28, 1, 2);
}

// Reject landscape artwork after decode; try the item's portrait poster, then a title card.
static GLuint bibPoster(const CatItem *ci, float largura, const char **url) {
  *url = ci ? posterprov_card_addon(ci->origem, ci->imdb, ci->tmdb, ci->tipo, ci->poster) : NULL;
  if (!*url || !(*url)[0]) return 0;
  GLuint t = tex_obter_larg(*url, largura);
  float asp = tex_aspecto(*url);
  if (t && asp > .85f) {
    if (strcmp(*url, ci->poster) && ci->poster[0]) {
      *url = ci->poster; t = tex_obter_larg(*url, largura);
      asp = tex_aspecto(*url);
    }
    if (asp > .85f) return 0;
  }
  return t;
}

static void bibTitleCard(const CatItem *ci, GfxRect r, float a) {
  const char *s = ci ? ci->titulo : "";
  plrui_material_s(r, 14, 0, a, 1);
  gfx_icone((GfxRect){r.x + (r.w - 32) / 2, r.y + 22, 32, 32}, "aj_film", 1, 1, 1, .5f * a);
  if (r.w > 100)
    txt_bloco_corta(TXT_HERO_SEC, s, 243, 242, 239, r.x + 14, r.y + r.h - 85, r.w - 28, 23, a, 3);
}

static void bibSourceBadges(const CatItem *ci, float x, float y, float a) {
  if (!ci) return;
  int bits = ci->fontes ? ci->fontes : ci->naLista ? CAT_FONTE_NUVIO : 0;
  const char *letters[] = { "T", "S", "N" };
  for (int b = 0; b < 3; b++) if (bits & (1 << b)) {
    gfx_cor((GfxRect){x, y, 26, 26}, 8.0f / 26, .035f, .035f, .045f, .85f * a);
    TxtLinha t = txt_linha(TXT_MINI, letters[b], 230, 230, 230, 255);
    txt_desenhar_alpha(t, x + (26 - t.w) / 2, y + (26 - t.h) / 2, a);
    x += 31;
  }
  if (ci->origem[0] && strcmp(ci->origem,"plex") && strcmp(ci->origem,"jellyfin") && strcmp(ci->origem,"emby")) {
    gfx_cor((GfxRect){x, y, 26, 26}, 8.0f / 26, .035f, .035f, .045f, .85f * a);
    txt_desenhar_alpha(txt_linha(TXT_MINI, "A", 230, 230, 230, 255), x + 8, y + 5, a);
    x += 31;
  }
  const char *logos[] = { "deploy/app/art/marcas/plex.png", "deploy/app/art/marcas/jellyfin.png", "deploy/app/art/marcas/emby.png" };
  int membership = 0;
  if (serverContains) for (int i = 0; i < cat_n() && i < CAT_MAX; i++) {
    const CatItem *stored = cat_item(i);
    if (stored && !strcmp(stored->imdb, ci->imdb)) { membership = serverMembership[i]; break; }
  }
  for (int p = 0; p < 3; p++) if (membership & (1 << p)) {
    gfx_cor((GfxRect){x, y, 26, 26}, 8.0f / 26, .035f, .035f, .045f, .85f * a);
    GLuint t = tex_obter_larg(logos[p], 22);
    if (t) gfx_rect((GfxRect){x + 3,y + 3,20,20},t,GFX_MARCA,0,0,0,0,1,1,1,a);
    x += 31;
  }
}
static void linhaMeta(const CatItem *ci, char *dst, size_t n);

// Um TITULO, na exibicao de cartaz (mockup: 220x330, canto 14, titulo 18/500 a
// 62% a 10 do cartaz). O FOCO LEVANTA O CARTAZ: escala 1,08 a partir do
// CENTRO (transform: scale do CSS) com a sombra longa embaixo (0 22 50 a 55%),
// e o titulo clareia, ganha peso e abre em ate duas linhas, descendo os 14 que
// o cartaz cresceu.
//
// O CONTORNO: o mockup nao tem. Ele so aparece se Ajustes > Foco no cartaz
// ("Borda no cartaz em foco") estiver ligado — o ajuste e da pessoa e e
// respeitado aqui como na Home. O padrao de fabrica dele e LIGADO (ajustes.c,
// valor[]), entao quem quer o Glass UI do mockup desliga la.
static void desenhaCartaz(const CatItem *ci, GfxRect base, float f, float a,
                          RevelaArte *rv, Uint32 agora) {
  float esc = 1.0f + BIB_FOCO_ESCALA * f;
  float bw = base.w * esc, bh = base.h * esc;
  GfxRect card = { base.x - (bw - base.w) * 0.5f, base.y - (bh - base.h) * 0.5f, bw, bh };
  if (card.y >= BIB_GRADE_BASE || base.y + alturaLinha() <= 0 ||
      card.x >= NV_TELA_W || card.x + card.w <= 0) return;
  float raio = raioPx(14.0f * esc, card.w, card.h);
  const char *arte = NULL;
  // Labels can remain visible after the artwork leaves the grid's clip.
  GLuint tex = card.y + card.h > gradeY() - 18.0f * bibRel()
    ? bibPoster(ci, BIB_CARD_W * (1.0f + BIB_FOCO_ESCALA), &arte) : 0;
  // Arte chegando esvanece sobre o esqueleto (revela.h), como na home.
  float aArte = rv ? revela_arte(rv, tex != 0, agora) : 1.0f;
  if (f > 0.01f)
    gfx_rect((GfxRect){ card.x - 50.0f, card.y + 22.0f - 50.0f, card.w + 100.0f, card.h + 100.0f }, 0,
             GFX_SOMBRA, 1.0f, 0, 0, 0.5f, 0, 0, 0, .55f * f * a);
  if (tex) {
    if (aArte < 0.999f)
      gfx_cor(card, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G,
              NV_COR_ESQUELETO_B, a);
    gfx_tex_aspect_atual = tex_aspecto(arte);
    gfx_rect(card, tex, GFX_CARD, f, 0.0f, 0.0f, raio, 0, 0, 0, a * aArte);
    gfx_tex_aspect_atual = 0.0f;
  } else {
    // Esqueleto VISIVEL, o mesmo da home: #2C2C2C. Placeholder do tom do fundo
    // le como card quebrado, nao como carregando. Com a luz passando enquanto
    // a arte ainda pode chegar.
    if (arte && !tex_falhou(arte) && tex_aspecto(arte) <= .85f)
      gfx_esqueleto(card, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G,
                    NV_COR_ESQUELETO_B, a);
    else bibTitleCard(ci, card, a);
  }
  // GFX_CARD owns the configured poster border, as in Home.
  bibSourceBadges(ci, card.x + 10, card.y + 10, a);
  // Selo de assistido: o da Home (badges_selo_visto), no cartaz JA ESCALADO,
  // entao acompanha o foco de 1.08. Salvo local fora do catalogo chega como
  // CatItem temporario com imdb/tipo: o historico responde igual.
  if (ci && ajustes_selo_visto() && cat_visto(ci))
    badges_selo_visto(card.x, card.y, card.w, a * aArte);
  if (ci) {
    float ty = base.y + base.h + BIB_TIT_GAP + 14.0f * f;
    if (f > 0.5f) {
      // Em foco: 18/600 branco (o Bold de 18, sem reducao), em ate duas
      // linhas — "Killers of the Flower Moon" inteiro, como no mockup.
      txt_bloco_corta(TXT_HERO_SEC, ci->titulo, 255, 255, 255, base.x, ty, base.w,
                      22.0f, a, 1);
      char meta[220]; linhaMeta(ci, meta, sizeof meta);
      TxtLinha m = txt_linha_corta(TXT_MINI, meta, 145, 147, 153, 255, base.w);
      txt_desenhar_alpha(m, base.x, ty + 28, a);
    } else {
      TxtLinha tl = txt_linha_corta(TXT_HERO_META, ci->titulo, 156, 156, 156, 255,
                                    base.w / BIB_ESC_TIT);
      txtEsc(tl, base.x, ty, BIB_ESC_TIT, a);
    }
  }
}

// A LINHA DE META, montada de CAMPOS e nao colando `meta` com `genero`.
//
// O DEFEITO, visto na captura do dono: duas linhas diziam "2025 · Movie ·
// Action · Crime · Thriller · Drama" e as tres seguintes diziam so "Movie". Nao
// e bug de leitura — e AUSENCIA de dado, e a causa esta em trakt.c: um item que
// chega pela watchlist recebe `genero` = "Filme"/"Programa de TV" e NENHUM
// `meta`, porque trakt_lista nao consulta o Cinemeta item a item (decisao
// documentada la: uma consulta por item custava ~0,3 s e limitava a lista a
// dez). Quem foi enriquecido pela descoberta tem os seis campos; quem nao foi
// tem um.
//
// Colar as duas strings propagava essa diferenca inteira para a tela. Montando
// de campos, a linha tem FORMA ESTAVEL:
//   - o TIPO e derivado (ehSerie), entao existe SEMPRE — o minimo e "Filme";
//   - o ano sai de `meta` quando houver;
//   - os generos entram no MAXIMO DOIS, e o segmento que so repete o tipo
//     ("Movie", "Programa de TV") e descartado.
// O maximo vira "2025 · Filme · Ação · Crime" e o minimo "Filme": a linha varia
// em comprimento, nao em natureza, e nenhuma delas parece quebrada.
static int ehPalavraDeTipo(const char *seg) {
  static const char *T[] = { "Filme", "Movie", "Série", "Serie", "Series",
                             "Programa de TV", "TV Show", "Show" };
  size_t i;
  for (i = 0; i < sizeof T / sizeof T[0]; i++)
    if (!strcasecmp(seg, T[i])) return 1;
  return 0;
}

// Os separadores dos metadados sao o ponto medio UTF-8. Nao usar strtok: ele
// trata os dois bytes de "·" como delimitadores separados e pode cortar um
// caractere acentuado no meio.
static int proximoMeta(const char **cursor, char *dst, size_t n) {
  const char *p = *cursor, *sep, *fim;
  size_t m = 0;
  if (!p || !*p || !n) return 0;
  sep = p;
  while (*sep && !((unsigned char)sep[0] == 0xC2 &&
                   (unsigned char)sep[1] == 0xB7)) sep++;
  // O cursor avanca a partir do SEPARADOR, nao do fim aparado: aparar o espaco
  // antes do "·" recuava `fim` um byte, e `fim + 2` caia no B7 — todo segmento
  // depois do primeiro saia com um byte de continuacao solto na frente, que a
  // fonte desenha como tofu. Visto na captura ("▯ Drama").
  fim = sep;
  while (p < fim && *p == ' ') p++;
  while (fim > p && fim[-1] == ' ') fim--;
  while (p < fim && m + 1 < n) dst[m++] = *p++;
  dst[m] = 0;
  *cursor = *sep ? sep + 2 : sep;
  return dst[0] != 0;
}

static int ehAnoMeta(const char *s) {
  int i;
  if (!s || strlen(s) != 4) return 0;
  for (i = 0; i < 4; i++) if (s[i] < '0' || s[i] > '9') return 0;
  return s[0] == '1' || s[0] == '2';
}

static int ehTemporadasMeta(const char *s) {
  return s && (strstr(s, "season") || strstr(s, "temporada"));
}

static int numeroMeta(const char *s) {
  const char *p;
  if (!s) return 0;
  for (p = s; *p; p++)
    if (*p >= '0' && *p <= '9') return atoi(p);
  return 0;
}

static void acrescentarMeta(char *dst, size_t n, size_t *k, const char *s) {
  int escreveu;
  if (!s || !s[0] || !n || *k >= n - 1) return;
  if (*k) {
    escreveu = snprintf(dst + *k, n - *k, "   ·   ");
    *k += (size_t)(escreveu > 0 ? escreveu : 0);
    if (*k >= n - 1) { *k = n - 1; dst[*k] = 0; return; }
  }
  escreveu = snprintf(dst + *k, n - *k, "%s", s);
  *k += (size_t)(escreveu > 0 ? escreveu : 0);
  if (*k >= n) *k = n - 1;
  dst[*k] = 0;
}

static void linhaMeta(const CatItem *ci, char *dst, size_t n) {
  char ano[8] = "", complemento[96] = "", item[96], temporadas[64];
  int nGen = 0, nTemporadas = 0;
  size_t k = 0;
  const char *p, *g;
  if (!n) return;
  dst[0] = 0;
  if (!ci) return;
  // Primeiro le o que o catalogo ja trouxe: ano, temporadas e uma duracao real
  // quando existir. O campo e opcional; nao inventar "1 h" quando so temos
  // progresso ou restanteMin.
  for (p = ci->meta; proximoMeta(&p, item, sizeof item); ) {
    if (ehAnoMeta(item)) snprintf(ano, sizeof ano, "%s", item);
    else if (ehTemporadasMeta(item) && !nTemporadas) nTemporadas = numeroMeta(item);
    else if (!ehPalavraDeTipo(item) && !complemento[0])
      snprintf(complemento, sizeof complemento, "%s", item);
  }
  if (!nTemporadas && ci->nTemporadas > 0) nTemporadas = ci->nTemporadas;

  acrescentarMeta(dst, n, &k, i18n(ehSerie(ci) ? "Série" : "Filme"));
  if (ano[0]) acrescentarMeta(dst, n, &k, ano);
  if (nTemporadas > 0) {
    snprintf(temporadas, sizeof temporadas, i18n(nTemporadas == 1
                                                  ? "%d temporada"
                                                  : "%d temporadas"),
             nTemporadas);
    acrescentarMeta(dst, n, &k, temporadas);
  } else if (complemento[0]) acrescentarMeta(dst, n, &k, complemento);

  // Generos entram depois da parte tecnica, no maximo dois para a linha
  // continuar escaneavel. O primeiro segmento do Trakt costuma repetir o tipo.
  g = ci->genero;
  while (proximoMeta(&g, item, sizeof item) && nGen < 2) {
    if (!ehPalavraDeTipo(item)) {
      acrescentarMeta(dst, n, &k, i18n(item));
      nGen++;
    }
  }
  // RestanteMin so entra com o rotulo certo. Progresso sem restante vira uma
  // porcentagem, nunca uma duracao inventada.
  if (ci->restanteMin > 0 && ci->progresso > 0 && ci->progresso < 100) {
    snprintf(item, sizeof item, i18n("%d min restantes"), ci->restanteMin);
    acrescentarMeta(dst, n, &k, item);
  } else if (ci->progresso > 0 && ci->progresso < 100) {
    snprintf(item, sizeof item, i18n("%d%% assistido"), ci->progresso);
    acrescentarMeta(dst, n, &k, item);
  }
}

// META CURTA DA TABELA (mockup "biblioteca-lista-servidor"): tipo, o que
// identifica (ano ou T1 E4) e o ESTADO — "Filme · 2025 · Pode continuar",
// "Série · T1 E4 · Pode continuar", "Filme · 2025 · Não assistido". E uma
// linha so: generos e duracao ficam para o detalhe; aqui a coluna responde
// "o que e e em que ponto estou".
static void linhaMetaLista(const CatItem *ci, char *dst, size_t n) {
  char ano[8] = "", item[96], seg[48];
  const char *p;
  size_t k = 0;
  dst[0] = 0;
  if (!ci) return;
  for (p = ci->meta; proximoMeta(&p, item, sizeof item); )
    if (ehAnoMeta(item)) { snprintf(ano, sizeof ano, "%s", item); break; }
  acrescentarMeta(dst, n, &k, i18n(ehSerie(ci) ? "Série" : "Filme"));
  if (ehSerie(ci) && ci->temporada > 0 && ci->episodio > 0) {
    snprintf(seg, sizeof seg, "T%d E%d", ci->temporada, ci->episodio);
    acrescentarMeta(dst, n, &k, seg);
  } else if (ano[0])
    acrescentarMeta(dst, n, &k, ano);
  acrescentarMeta(dst, n, &k,
                  i18n(ci->progresso > 0 && ci->progresso < 100
                         ? "Pode continuar" : "Não assistido"));
}

// COLUNA DE DISPONIBILIDADE: a retomada quando ha, senao a melhor qualidade
// que um servidor pessoal informa ("4K · HDR"). Sem dado, coluna vazia —
// inventar "4K" onde nenhum servidor falou e pior do que o silencio.
static void disponibilidadeTexto(const CatItem *ci, char *dst, size_t n) {
  dst[0] = 0;
  if (!ci) return;
  if (ci->restanteMin > 0 && ci->progresso > 0 && ci->progresso < 100) {
    snprintf(dst, n, i18n("Restam %d min"), ci->restanteMin);
    return;
  }
  { int altura = 0, hdr = 0, tem = 0;
    const char *p1, *p2;
    if (ci->imdb[0]) {
      if (serverQualidade) tem = serverQualidade(ci, &altura, &hdr);
      else {
        SrvAchado ach;
        if (servidores_info(ci->imdb, ci->tipo, &ach)) {
          altura = ach.altura;
          hdr = ach.hdr;
          tem = 1;
        }
      }
    }
    if (!tem) return;
    p1 = altura >= 2000 ? "4K" : altura >= 1000 ? "1080p" : NULL;
    p2 = hdr ? "HDR" : NULL;
    if (p1 && p2) snprintf(dst, n, "%s · %s", p1, p2);
    else if (p1 || p2) snprintf(dst, n, "%s", p1 ? p1 : p2);
  }
}

// QUANTOS BADGES bibSourceBadges VAI DESENHAR (para alinhar a coluna pela
// direita sem desenhar duas vezes).
static int bibSourceBadgesCont(const CatItem *ci) {
  int bits = ci->fontes ? ci->fontes : ci->naLista ? CAT_FONTE_NUVIO : 0;
  int membership = 0, n = 0;
  for (int b = 0; b < 3; b++) if (bits & (1 << b)) n++;
  if (ci->origem[0] && strcmp(ci->origem, "plex") && strcmp(ci->origem, "jellyfin")
      && strcmp(ci->origem, "emby")) n++;
  if (serverContains)
    for (int i = 0; i < cat_n() && i < CAT_MAX; i++) {
      const CatItem *stored = cat_item(i);
      if (stored && !strcmp(stored->imdb, ci->imdb)) {
        for (int p = 0; p < 3; p++) if (serverContains(ci, p)) membership |= 1 << p;
        break;
      }
    }
  for (int p = 0; p < 3; p++) if (membership & (1 << p)) n++;
  return n;
}

// Um TITULO, na exibicao de lista. Linha inteira preenchida, miniatura a
// esquerda, texto no meio e a COLUNA DE NOTA ancorada na direita.
//
// O QUE ENTROU NA DIREITA, e o que foi recusado. O dono pediu "mais informacoes
// no canto direito, falta as notas". Tudo o que entrou ja estava em maos, a
// custo ZERO de rede — nenhum campo novo, nenhuma consulta:
//
//   NOTA (ci->nota)      entrou. E o pedido explicito e e o unico numero que
//                        responde "vale a pena?" sem abrir o titulo.
//   PROGRESSO            entrou, como barra, e SO quando ha progresso. Numa
//   (ci->progresso)      biblioteca a segunda pergunta e "onde eu parei?".
//
//   DURACAO              recusada: o CatItem nao tem duracao total, so
//                        `restanteMin`, que e "quanto falta" e so existe em
//                        quem esta em andamento. Mostrar "restante" como se
//                        fosse duracao seria dado com o rotulo trocado.
//   VISTO (check)        recusado: nao ha marca de "assistido" por item no
//                        CatItem — so progresso. Um check derivado de
//                        progresso >= 100 e inferencia, nao dado.
//   TEMPORADAS           recusado: ja vive em `meta`, e entra na linha de meta
//                        quando o campo existir. Duas vezes a mesma coisa.
//   CLASSIFICACAO ETARIA RECUSADA E E O CASO MAIS IMPORTANTE: trakt.c grava
//                        `classificacao = "14"` FIXO em todo item de lista. E
//                        constante disfarcada de dado, o defeito que o
//                        PRODUCT.md proibe e que descoberta.c ja consertou do
//                        lado dele. Nao vai para a tela.
//   ADDON DE ORIGEM      recusado: nao acompanha o item, e e procedencia do
//                        CATALOGO, nao do titulo. Repetido em toda linha vira
//                        ruido, pela mesma razao que tirou o wordmark dos
//                        cartoes.
//
// POR QUE A NOTA RESERVA COLUNA E O PROGRESSO NAO: a nota e para ser LIDA EM
// COLUNA, descendo a lista — ela tem posicao fixa e um travessao onde falta. O
// progresso e um estado de poucos itens; reservar 132 px em toda linha para
// mostrar nada na maioria delas seria o vazio que esta correcao veio tirar.
//
// MEMORIA: a miniatura pede tex_obter_larg(96), nao tex_obter — o teto de
// decodificacao sai da largura desenhada, e so a linha VISIVEL chega aqui.
//
// NA PAGINA DA LISTA (idx >= 0) a linha ganha o RANK antes da miniatura e a NOTA
// DO CURADOR no lugar da sinopse. O rank so aparece quando o Trakt o informa
// (0 = nao informado), e a nota quando a pessoa que fez a lista escreveu uma.
static void desenhaWatched(const CatItem *ci, float y, float f, float a) {
  if (!ci) return;
  GfxRect row = {paginaX(), y, bibDireita() - paginaX(), 120};
  plrui_linha_foco(row, T21_R_ROW, f * a);
  GfxRect poster = {row.x + 22, y + 12, 64, 96};
  const char *url; GLuint tex = bibPoster(ci, 64, &url);
  if (tex) {
    gfx_tex_aspect_atual = tex_aspecto(url);
    gfx_rect(poster, tex, GFX_CARD, 0, 0, 0, 9.0f / 96, 0, 0, 0, a);
    gfx_tex_aspect_atual = 0;
  } else bibTitleCard(ci, poster, a);
  txt_desenhar_alpha(txt_linha_corta(TXT_CALLOUT, ci->titulo, 243, 242, 239, 255, 520), row.x + 108, y + 24, a);
  char meta[220]; linhaMeta(ci, meta, sizeof meta);
  txt_desenhar_alpha(txt_linha_corta(TXT_MINI, meta, 120, 122, 128, 255, 520), row.x + 108, y + 59, a);
  long long ms = 0;
  cat_historico_estado_tempo(ci->imdb, ci->tipo, &ms);
  if (ms > 0) {
    time_t seconds = (time_t)(ms / 1000); struct tm tmv;
    if (localtime_r(&seconds, &tmv)) {
      char date[32]; strftime(date, sizeof date, "%d/%m/%Y  %H:%M", &tmv);
      txt_desenhar_alpha(txt_linha(TXT_HERO_META, date, 185, 187, 193, 255), row.x + 700, y + 43, a);
    }
  }
  // Aggregate play counts are not present in the local history model.
  txt_desenhar_alpha(txt_linha(TXT_HERO_META, "—", 110, 112, 118, 255), row.x + 1030, y + 43, a);
  bibSourceBadges(ci, row.x + 1160, y + 42, a);
}

static void desenhaLinhaTitulo(const CatItem *ci, int idx, float y, float f, float a) {
  if (modo == MODO_ASSISTIDOS) { desenhaWatched(ci, y, f, a); return; }
  // PAGINA DE UMA LISTA ABERTA: a linha de 168 com rank, meta rica e nota do
  // curador continua como era — o mockup aprovado e o da GRADE de titulos.
  if (paginaLista() && !trilha) {
    GfxRect r = { bibX(), y, bibW(), BIB_LIN_H };
    DescItemLista ex = { 0 };
    GfxRect mini;
    float dx = 0.0f;
    plrui_linha_foco(r, T21_R_ROW, f * a);
    if (idx >= 0) {
      lst_item_extra(idx, &ex);
      dx = BIB_PAG_RANK_W;
      // Ordem em edicao: o numero e a posicao NA TELA, nao o rank do servidor.
      if (edModo == ED_ORDEM && ex.rank > 0) ex.rank = idx + 1;
    }
    mini = (GfxRect){ r.x + BIB_LIN_PAD + dx, y + (BIB_LIN_H - BIB_LIN_MINI_H) * 0.5f,
                      BIB_LIN_MINI_W, BIB_LIN_MINI_H };
    { const char *arte;
      GLuint tex = bibPoster(ci, BIB_LIN_MINI_W, &arte);
      float raioMini = raioPx(8.0f, mini.w, mini.h);
      if (tex) {
        gfx_tex_aspect_atual = tex_aspecto(arte);
        gfx_rect(mini, tex, GFX_CARD, 0.0f, 0.0f, 0.0f, raioMini, 0, 0, 0, a);
        gfx_tex_aspect_atual = 0.0f;
      } else bibTitleCard(ci, mini, a); }
    if (!ci) return;
    { float tx = mini.x + mini.w + 16;
      if (ex.rank > 0) {
        char rank[16]; snprintf(rank, sizeof rank, "%d", ex.rank);
        TxtLinha rk = txt_linha(TXT_CALLOUT, rank, 120, 122, 128, 255);
        txt_desenhar_alpha(rk, mini.x - 18 - rk.w, y + 14, a);
      }
      txt_desenhar_alpha(txt_linha_corta(TXT_ILHA_NOME, ci->titulo, 243, 242, 239, 255, r.x + r.w - tx - 56), tx, y + 10, a);
      char meta[220]; linhaMeta(ci, meta, sizeof meta);
      txt_desenhar_alpha(txt_linha_corta(TXT_ILHA_HORA, meta, 151, 153, 163, 255, r.x + r.w - tx - 56), tx, y + 40, a);
      if (ex.nota[0]) { char q[400]; snprintf(q, sizeof q, "“%s”", ex.nota);
        txt_bloco_corta(TXT_ILHA_APOIO, q, 205, 207, 213, tx, y + 66, r.x + r.w - tx - 56, 25, a, 2); }
      else txt_desenhar_alpha(txt_linha(TXT_ILHA_HORA, "sem nota do curador", 115, 117, 122, 255), tx, y + 66, a);
      if (cat_visto(ci)) { float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
        gfx_icone((GfxRect){r.x + r.w - 44, y + 14, 24, 24}, "aj_check", ar, ag, ab, a); } }
    return;
  }
  // GRADE DE TITULOS EM LISTA: a TABELA do mockup "biblioteca-lista-servidor".
  // Linha de 126, cartaz 64x96, titulo 24 + UMA meta curta. Na direita, a
  // coluna de ORIGENS (badges) e a de DISPONIBILIDADE (retomada ou
  // qualidade). SEM sinopse e SEM selo IMDb: foi o que transformava a linha
  // num cartao grande e o dono pediu o compacto do mockup. O foco e a pilula
  // clara do mockup; em repouso a linha e a propria pagina, sem superficie.
  { GfxRect r = { bibX(), y, bibW(), BIB_TL_H };
    float yc = y + BIB_TL_H * 0.5f;
    float v = anim_clamp(f, 0.0f, 1.0f);
    plrui_linha_foco(r, T21_R_ROW, v * a);
    // O titulo PEGO na Ordem: barra de acento na borda esquerda da linha.
    if (edModo == ED_ORDEM && idx >= 0 && idx == ordemPega) {
      float ar, ag, ab;
      ajustes_acento(&ar, &ag, &ab);
      gfx_cor((GfxRect){ r.x - 14.0f, y + 10.0f, 6.0f, BIB_TL_H - 20.0f }, 0.5f, ar, ag, ab, a);
    }
    { GfxRect mini = { r.x + BIB_TL_PAD, y + (BIB_TL_H - BIB_TL_MINI_H) * 0.5f,
                       BIB_TL_MINI_W, BIB_TL_MINI_H };
      const char *arte;
      GLuint tex = bibPoster(ci, BIB_TL_MINI_W, &arte);
      float raioMini = raioPx(9.0f, mini.w, mini.h);
      if (tex) {
        gfx_tex_aspect_atual = tex_aspecto(arte);
        gfx_rect(mini, tex, GFX_CARD, 0.0f, 0.0f, 0.0f, raioMini, 0, 0, 0, a);
        gfx_tex_aspect_atual = 0.0f;
      } else bibTitleCard(ci, mini, a); }
    if (!ci) return;
    { float tx = r.x + BIB_TL_PAD + BIB_TL_MINI_W + BIB_TL_TX_GAP;
      char disp[64];
      TxtLinha t, av;
      float larg = r.w - BIB_TL_PAD - BIB_TL_MINI_W - BIB_TL_TX_GAP - 440.0f;
      if (larg < 80.0f) larg = 80.0f;
      t = txt_linha_corta(TXT_ILHA_NOME, ci->titulo, 243, 242, 239, 255, larg);
      txt_desenhar_alpha(t, tx, yc - (float)t.h - 5.0f, a);
      { char sub[96];
        linhaMetaLista(ci, sub, sizeof sub);
        txt_desenhar_alpha(txt_linha_corta(TXT_ILHA_GENERO, sub, 151, 153, 163, 255, larg),
                           tx, yc + 8.0f, a * 0.95f); }
      // COLUNA DE DISPONIBILIDADE, ancorada na direita como no mockup: o que
      // resta da retomada, ou a melhor qualidade que um servidor informa.
      disponibilidadeTexto(ci, disp, sizeof disp);
      av = txt_linha(TXT_ILHA_GENERO, disp, 151, 153, 163, 255);
      txt_desenhar_alpha(av, r.x + r.w - BIB_TL_DIR - (float)av.w, yc - (float)av.h * 0.5f, a);
      // COLUNA DE ORIGEM: os badges terminam antes da disponibilidade.
      { int n = bibSourceBadgesCont(ci);
        float largB = n > 0 ? n * 31.0f - 5.0f : 0.0f;
        float xB = r.x + r.w - BIB_TL_DIR - (float)av.w - 40.0f - largB;
        bibSourceBadges(ci, xB, yc - 13.0f, a); } }
  }
}

static const char *rotuloFonte(int f) {
  return f == LST_TRAKT ? "TRAKT" : f == LST_SIMKL ? "SIMKL" : f == LST_SERVIDOR ? "Servidores pessoais" : "NUVIO";
}

// A MARCA DA FONTE SO APARECE QUANDO ELA INFORMA ALGO.
//
// Ela estava em TODO cartao da grade, sempre a mesma. Uma marca repetida em
// cada peca de uma grade homogenea nao identifica nada — e papel de parede —, e
// o seletor "Fonte" logo acima ja diz de onde aquela grade veio. Sobra o unico
// caso em que ela responde uma pergunta: a aba FIXADAS, que mistura listas do
// Trakt, do Simkl e do Nuvio na mesma grade. So ali o cartao precisa dizer de
// onde ele e.
//
// A regra e "a grade e mista?", e nao "a fonte e Trakt?": se amanha outra aba
// misturar origens, ela ganha a marca sozinha.
static int gradeMista(void) {
  return !temAberta && modo == MODO_LISTAS && fonte == FONTE_FIXADAS;
}

// Linha de apoio de um cartao de lista: quem fez e quantos itens tem. A
// contagem SO APARECE quando a fonte informa (-1 = nao informa) — escrever
// "0 títulos" onde o dado nao existe e inventar dado.
static void subtituloLista(const LstLista *l, char *dst, size_t n) {
  char qt[48] = "";
  if (l->itens >= 0)
    snprintf(qt, sizeof qt, "%d %s", l->itens,
             i18n(l->itens == 1 ? "título" : "títulos"));
  if (l->autor[0] && qt[0]) snprintf(dst, n, "%s   ·   %s", l->autor, qt);
  else if (l->autor[0])     snprintf(dst, n, "%s", l->autor);
  else                      snprintf(dst, n, "%s", qt);
}

// A marca da fonte, desenhada no canto do cartao/linha quando a grade e mista.
// Devolve a largura ocupada (0 quando nao ha marca a desenhar).
static float seloFonte(const LstLista *l, float x, float y, float h,
                       int tinta, float a) {
  int sr, sg, sb;
  if (!gradeMista()) return 0.0f;
  if (l->fonte == LST_TRAKT) {
    float w = marcaTrakt(x, y, h, tinta, a * 0.95f);
    if (w > 0.0f) return w;
  }
  tintaSecundaria(tinta, &sr, &sg, &sb);
  { TxtLinha t = txt_linha(TXT_CAPTION2, rotuloFonte(l->fonte), sr, sg, sb, 255);
    txt_desenhar_alpha(t, x, y + (h - (float)t.h) * 0.5f, a * 0.95f);
    return (float)t.w; }
}

// CARTAO DE LISTA. Ver a nota das medidas no topo para o que saiu e por que.
// O que sobrou responde a unica pergunta que a grade faz — "e esta?": o NOME,
// em ate duas linhas, e o tamanho da lista com o dono.
static void desenhaCartaoLista(const LstLista *l, GfxRect r, float f, float a) {
  if (!telefoneui_ativo()) {
    if (!l) return;
    plrui_material_s(r, T21_R_CARD, 0, a, 1);
    plrui_linha_foco(r, T21_R_CARD, f * a);
    LstPrevia preview; lst_previa(l, &preview);
    for (int i = 0; i < 4; i++) {
      GfxRect tile = { r.x + i * r.w / 4, r.y, r.w / 4 + 1, 150 };
      GLuint t = i < preview.n ? tex_obter_larg(preview.poster[i], tile.w) : 0;
      if (t && tex_aspecto(preview.poster[i]) <= .85f) {
        gfx_tex_aspect_atual = tex_aspecto(preview.poster[i]);
        gfx_rect(tile, t, GFX_CARD, 0, 0, 0, 0, 0, 0, 0, a);
        gfx_tex_aspect_atual = 0;
      } else {
        CatItem blank = {0};
        snprintf(blank.titulo, sizeof blank.titulo, "%s %d", l->titulo, i + 1);
        bibTitleCard(&blank, tile, a);
      }
    }
    { TxtLinha origem = txt_linha_corta(TXT_ILHA_HORA, rotuloFonte(l->fonte), 235, 235, 235, 255, r.w - 64);
      GfxRect selo = { r.x + 12, r.y + 12, origem.w + 24.0f, 30.0f };
      plrui_botao_repouso(selo, a);
      txt_desenhar_alpha(origem, selo.x + 12, selo.y + (selo.h - origem.h) * .5f, a); }
    TxtLinha title = txt_linha_corta(TXT_CALLOUT, l->titulo, 243, 242, 239, 255, r.w - 40);
    txt_desenhar_alpha(title, r.x + 20, r.y + 166, a);
    char sub[220]; subtituloLista(l, sub, sizeof sub);
    TxtLinha detail = txt_linha_corta(TXT_HERO_META, sub, 145, 147, 153, 255, r.w - 40);
    txt_desenhar_alpha(detail, r.x + 20, r.y + 206, a);
    if (lst_fixada(l)) txt_desenhar_alpha(txt_linha(TXT_MINI, "FIXADA", 160, 162, 168, 255), r.x + 20, r.y + 251, a);
    return;
  }
  int cor = pilula(r, raioPx(20.0f, r.w, r.h), f, 0);
  int sr, sg, sb;
  char sub[220];
  float larg = r.w - BIB_LC_PAD * 2.0f;
  float y = r.y + BIB_LC_PAD;
  if (!l) return;
  tintaSecundaria(cor, &sr, &sg, &sb);
  subtituloLista(l, sub, sizeof sub);
  if (telefoneui_ativo()) {
    int fixada = lst_fixada(l);
    TxtLinha fx = fixada ? txt_linha_corta(TXT_CAPTION2, i18n("FIXADA"), cor, cor, cor, 255, larg * .45f)
                        : (TxtLinha){0};
    TxtLinha medida = txt_linha(TXT_CAPTION2, sub, sr, sg, sb, 255);
    float rodapeH = fmaxf(medida.h, fx.h), rodapeY = r.y + r.h - BIB_LC_PAD - rodapeH;
    float marca = seloFonte(l, r.x + BIB_LC_PAD, rodapeY, rodapeH, cor, a);
    float apoioX = r.x + BIB_LC_PAD + (marca > 0 ? marca + 12 : 0);
    float apoioW = r.x + r.w - BIB_LC_PAD - apoioX - (fixada ? fx.w + 12 : 0);
    TxtLinha apoio = txt_linha_corta(TXT_CAPTION2, sub, sr, sg, sb, 255, apoioW);
    int linhas = (int)floorf((rodapeY - y - 6) / BIB_LC_ENTRE);
    if (linhas < 1) linhas = 1;
    if (linhas > 2) linhas = 2;
    txt_bloco(TXT_CALLOUT, l->titulo, cor, cor, cor, r.x + BIB_LC_PAD, y, larg, BIB_LC_ENTRE, a, linhas);
    txt_desenhar_alpha(apoio, apoioX, rodapeY, a * .95f);
    if (fixada) txt_desenhar_alpha(fx, r.x + r.w - BIB_LC_PAD - fx.w, rodapeY, a * .95f);
    return;
  }
  { float wMarca = seloFonte(l, r.x + BIB_LC_PAD, y, 22.0f, cor, a);
    if (wMarca > 0.0f) y += 28.0f; }
  // TITULO EM ATE DUAS LINHAS. Com quatro cartoes de 414 o nome ja saia cortado
  // ("Harry Potter…"), e estreitar para cinco pioraria isso — por isso a
  // densidade so foi possivel DEPOIS de o titulo poder quebrar. `txt_bloco`
  // devolve a altura usada, entao um nome de uma linha nao deixa buraco: o
  // apoio sobe junto.
  y += txt_bloco(TXT_CALLOUT, l->titulo, cor, cor, cor,
                 r.x + BIB_LC_PAD, y, larg, BIB_LC_ENTRE, a, 2);
  // FIXADA e uma PALAVRA para ler, entao ela sai em TXT_CAPTION2 e nao no
  // TXT_MINI de 15 px — o DESIGN.md e explicito: aquele tamanho e de SELO, nao
  // de texto, e o piso de leitura desta base e 21/22 px.
  { TxtLinha s2 = txt_linha_corta(TXT_CAPTION2, sub, sr, sg, sb, 255, larg);
    float yBase = r.y + r.h - BIB_LC_PAD - (float)s2.h;
    if (yBase < y + 4.0f) yBase = y + 4.0f;
    txt_desenhar_alpha(s2, r.x + BIB_LC_PAD, yBase, a * 0.95f);
    if (lst_fixada(l)) {
      TxtLinha fx = txt_linha(TXT_CAPTION2, i18n("FIXADA"), cor, cor, cor, 255);
      txt_desenhar_alpha(fx, r.x + r.w - BIB_LC_PAD - (float)fx.w, yBase, a * 0.95f);
    } }
}

// " · s" at the end of `dst` (just `s` when empty).
static void juntarApoio(char *dst, size_t n, const char *s) {
  size_t k = strlen(dst);
  if (s[0] && k + 1 < n) snprintf(dst + k, n - k, "%s%s", k ? " · " : "", s);
}
// 6D support line: "18 títulos · pública · editada há 4 dias", each part only
// when the source tells it.
static void apoioMinha(const LstLista *l, char *dst, size_t n) {
  char s[64];
  dst[0] = 0;
  if (l->itens >= 0) { snprintf(s, sizeof s, i18n(l->itens == 1 ? "%d título" : "%d títulos"), l->itens); juntarApoio(dst, n, s); }
  juntarApoio(dst, n, i18n(l->fonte == LST_TRAKT ? (l->privada ? "privada" : "pública")
                         : l->fonte == LST_SIMKL ? "estado do Simkl"
                         : l->fonte == LST_NUVIO ? "pasta de coleção" : ""));
  struct tm t = {0};
  if (sscanf(l->atualizada, "%d-%d-%d", &t.tm_year, &t.tm_mon, &t.tm_mday) == 3) {
    t.tm_year -= 1900; t.tm_mon -= 1;
    long dias = (long)((time(NULL) - timegm(&t)) / 86400);
    if (dias < 1) snprintf(s, sizeof s, "%s", i18n("editada hoje"));
    else snprintf(s, sizeof s, i18n("editada há %d dias"), (int)dias);
    juntarApoio(dst, n, s);
  }
}
static const char *letraFonte(int f) {
  return f == LST_SIMKL ? "S" : f == LST_NUVIO ? "N" : f == LST_SERVIDOR ? "P" : "T";
}
// The source letter disc of the 6D rows and Origens panel.
static void discoFonte(const char *letra, float x, float y) {
  gfx_cor((GfxRect){x, y, 34, 34}, 10.0f / 34, 1, 1, 1, .08f);
  TxtLinha t = txt_linha(TXT_HERO_SEC, letra, 235, 235, 235, 255);
  txt_desenhar(t, x + (34 - t.w) / 2, y + (34 - t.h) / 2);
}
static void desenhaLinhaMinha(const LstLista *l, float y, float f, float a) {
  GfxRect r = { bibX(), y, bibW(), BIB_LM_H };
  float ga = gfx_opacidade_grupo;
  LstPrevia pv;
  char viu[32] = "—", sub[220];
  if (!l) return;
  gfx_opacidade_grupo *= a;
  plrui_linha_foco(r, T21_R_ROW, f);
  discoFonte(letraFonte(l->fonte), r.x + 20, y + (BIB_LM_H - 34) / 2);
  if (lst_previa(l, &pv) && pv.vistos >= 0) snprintf(viu, sizeof viu, i18n("viu %d"), pv.vistos);
  TxtLinha v = txt_linha(TXT_G18R, viu, 150, 152, 158, 255);
  txt_desenhar(v, r.x + r.w - 20 - v.w, y + (BIB_LM_H - v.h) / 2);
  float tx = r.x + 70, tw = r.x + r.w - 44 - v.w - tx;
  apoioMinha(l, sub, sizeof sub);
  TxtLinha t = txt_linha_corta(TXT_G21B, l->titulo, 243, 242, 239, 255, tw);
  TxtLinha s = txt_linha_corta(TXT_ILHA_HORA, sub, 135, 137, 143, 255, tw);
  float ty = y + (BIB_LM_H - t.h - 2 - s.h) / 2;
  txt_desenhar(t, tx, ty);
  txt_desenhar(s, tx, ty + t.h + 2);
  gfx_opacidade_grupo = ga;
}

static void ponteiroPainel(int a, int b) { (void)b; focoOrigem = 0; focoPainel = 1; painelSel = a; }
// What the 6D panel offers for list `l`: own Trakt lists edit (Descrição,
// Nota no filme, Ordem = the open list's chips 3, 5, 4) and public lists share (6).
static int painelAcoes(const LstLista *l, int *ids) {
  char url[200];
  int n = 0;
  if (!l) return 0;
  if (fonte == FONTE_TRAKT && l->fonte == LST_TRAKT && l->traktId > 0) { ids[n++] = 3; ids[n++] = 5; ids[n++] = 4; }
  if (lst_url_publica(l, url, sizeof url)) ids[n++] = 6;
  return n;
}
static const char *rotuloPainel(int a) {
  return a == 3 ? "Descrição" : a == 5 ? "Nota no filme" : a == 4 ? "Ordem" : "Compartilhar";
}
static void desenhaPainelLista(void) {
  const LstLista *l = lst_lista(painelIdx < 0 ? 0 : painelIdx);
  int ids[4], n = painelAcoes(l, ids);
  char k[200], url[200];
  LstPrevia pv;
  if (!l) return;
  GfxRect p = { bibDireita() - lmPainel(), BIB_FAIXA_Y, lmPainel(), ANC(T21_SAFE_B) - BIB_FAIXA_Y };
  t21_ilha(p, T21_R_SHEET, 1);
  float ga = gfx_opacidade_grupo;
  gfx_opacidade_grupo *= anim_suave(animPainelTroca);
  float x = p.x + 32, w = p.w - 64, y = p.y + 34;
  snprintf(k, sizeof k, "%s · %s", i18n("Lista"), rotuloOrigem(l->fonte == LST_SIMKL ? FONTE_SIMKL : l->fonte == LST_NUVIO ? FONTE_NUVIO : FONTE_TRAKT, 1));
  if (l->fonte == LST_TRAKT) juntarApoio(k, sizeof k, i18n(l->privada ? "privada" : "pública"));
  plrui_kicker(k, x, y, 115, 117, 122, 1);
  y += 24;
  y += txt_bloco_corta(TXT_LOG_T34, l->titulo, 243, 242, 239, x, y, w, 42, 1, 2) + 10;
  if (l->descricao[0]) y += txt_bloco_corta(TXT_ILHA_SUB, l->descricao, 175, 177, 183, x, y, w, 27, 1, 2) + 18;
  else {
    char sub[220]; apoioMinha(l, sub, sizeof sub);
    txt_desenhar(txt_linha_corta(TXT_ILHA_SUB, sub, 175, 177, 183, 255, w), x, y);
    y += 45;
  }
  { float px = x;
    for (int i = 0; i < n; i++) if (ids[i] != 6) {
      const char *ic = ids[i] == 4 ? NULL : "aj_smartphone";
      float bw = plrui_botao_largura(rotuloPainel(ids[i]), ic);
      if (px > x && px + bw > x + w) { px = x; y += 72; }
      ponteiro_alvo(px, y, bw, 60, ponteiroPainel, NULL, i, 0);
      px += plrui_botao(px, y, rotuloPainel(ids[i]), ic, animPainel[i], 1) + 12;
    }
    if (px > x) y += 76; }
  // Large text scales: the share card keeps its room, the preview gives way.
  float fim = p.y + p.h - 24, qrH = n && ids[n - 1] == 6 ? 240 : 0;
  if (lst_previa(l, &pv) && pv.n && y + 98 + qrH <= fim) {
    GfxRect th = { x, y, 52, 78 };
    GLuint t = tex_obter_larg(pv.poster[0], th.w);
    if (t) { gfx_tex_aspect_atual = tex_aspecto(pv.poster[0]); gfx_rect(th, t, GFX_CARD, 0, 0, 0, 0, 0, 0, 0, 1); gfx_tex_aspect_atual = 0; }
    char um[160]; snprintf(um, sizeof um, "1 · %s", pv.titulo);
    txt_desenhar(txt_linha_corta(TXT_HERO_SEC, um, 243, 242, 239, 255, w - 68), x + 68, y + 2);
    if (pv.nota[0]) {
      char q[180]; snprintf(q, sizeof q, "“%s”", pv.nota);
      txt_bloco_corta(TXT_ILHA_APOIO, q, 190, 192, 198, x + 68, y + 30, w - 68, 22, 1, 2);
    }
    y += 98;
  }
  if (n && ids[n - 1] == 6 && lst_url_publica(l, url, sizeof url)) {
    GfxRect c = { x, y, w, fminf(240, fim - y) };
    float lado = fminf(fminf(200, w * .36f), c.h - 40);
    gfx_cor(c, 20.0f / c.h, 1, 1, 1, .04f);
    celb_qr_desenhar_lado(url, c.x + 20, c.y + 20, lado, 1);
    float bx = c.x + lado + 44;
    float bw = plrui_botao_largura("Compartilhar", "aj_send");
    ponteiro_alvo(bx, c.y + 30, bw, 60, ponteiroPainel, NULL, n - 1, 0);
    // The panel's one primary action: always in the accent; focus adds the halo.
    { float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
      brilhoFoco((GfxRect){ bx, c.y + 30, bw, 60 }, 0.5f, animPainel[n - 1], 1.0f);
      gfx_cor((GfxRect){ bx, c.y + 30, bw, 60 }, 0.5f, ar, ag, ab, 1); }
    plrui_botao(bx, c.y + 30, "Compartilhar", "aj_send", 0, 1);
    const char *curto = strstr(url, "://") ? strstr(url, "://") + 3 : url;
    txt_bloco_corta(TXT_HERO_SEC, curto, 235, 235, 235, bx, c.y + 104, c.x + c.w - 20 - bx, 24, 1, 2);
    int dica = (int)fminf(3, (c.h - 172) / 21);
    if (dica > 0)
      txt_bloco_corta(TXT_ILHA_HORA, "Aponte a câmera: abre a lista no celular de quem estiver na sala. O endereço serve para ditar.",
                      150, 152, 158, bx, c.y + 160, c.x + c.w - 20 - bx, 21, 1, dica);
  }
  gfx_opacidade_grupo = ga;
}

static void desenhaLinhaLista(const LstLista *l, float y, float f, float a) {
  if (listasMinhas()) { desenhaLinhaMinha(l, y, f, a); return; }
  GfxRect r = { bibX(), y, bibW(), BIB_LL_H };
  int cor = superficieLinha(r, raioPx(14.0f, r.w, r.h), f, a);
  int sr, sg, sb;
  char sub[220];
  float tx = r.x + 24.0f;
  if (!l) return;
  tintaSecundaria(cor, &sr, &sg, &sb);
  subtituloLista(l, sub, sizeof sub);
  { float w = seloFonte(l, tx, y + (BIB_LL_H - 26.0f) * 0.5f, 26.0f, cor, a);
    if (w > 0.0f) tx += w + 26.0f; }
  { TxtLinha s2 = txt_linha_corta(TXT_CAPTION2, sub, sr, sg, sb, 255, 340.0f);
    float xDir = r.x + bibW() - 24.0f;
    if (telefoneui_ativo() && lst_fixada(l)) {
      TxtLinha fx = txt_linha_corta(TXT_CAPTION2, i18n("FIXADA"), cor, cor, cor, 255, (xDir - tx) * .25f);
      float fxX = xDir - s2.w - 24 - fx.w;
      TxtLinha t = txt_linha_corta(TXT_CALLOUT, l->titulo, cor, cor, cor, 255, fxX - 24 - tx);
      txt_desenhar_alpha(t, tx, y + (BIB_LL_H - t.h) * .5f, a);
      txt_desenhar_alpha(fx, fxX, y + (BIB_LL_H - fx.h) * .5f, a * .95f);
      txt_desenhar_alpha(s2, xDir - s2.w, y + (BIB_LL_H - s2.h) * .5f, a * .95f);
      return;
    }
    TxtLinha t = txt_linha_corta(TXT_CALLOUT, l->titulo, cor, cor, cor, 255,
                                 xDir - (float)s2.w - 60.0f - tx);
    txt_desenhar_alpha(t, tx, y + (BIB_LL_H - (float)t.h) * 0.5f, a);
    txt_desenhar_alpha(s2, xDir - (float)s2.w,
                       y + (BIB_LL_H - (float)s2.h) * 0.5f, a * 0.95f);
    if (lst_fixada(l)) {
      TxtLinha fx = txt_linha(TXT_CAPTION2, i18n("FIXADA"), cor, cor, cor, 255);
      txt_desenhar_alpha(fx, xDir - (float)s2.w - (float)fx.w - 32.0f,
                         y + (BIB_LL_H - (float)fx.h) * 0.5f, a * 0.95f);
    } }
}

static void ponteiroOrigem(int a, int b) {
  (void)b;
  focoOrigem = 1; origemSel = a; focoBotaoOrig = 0;
}

static void desenhaOrigens(void) {
  if (modo == MODO_LISTAS) {
    // Same panel as the Library Origens (mock 6D): tracked kicker, letter disc
    // (server brands as logos), name, what the source holds, and the count of
    // the open source.
    GfxRect panel = {paginaX(), gradeY(), 320, fminf(590, ANC(T21_SAFE_B) - gradeY())};
    const char *sub[BIB_N_FONTES] = { "suas listas", "5 estados, sem nome", "pastas de coleção",
      "na Biblioteca", "do Trakt", "coleções do servidor", "coleções do servidor", "coleções do servidor" };
    const char *letra = "TSNFP";
    const char *logos[] = {"deploy/app/art/marcas/plex.png","deploy/app/art/marcas/jellyfin.png","deploy/app/art/marcas/emby.png"};
    int vis = (int)((panel.h - 62) / 72), start = focoOrigem && origemSel > vis - 1 ? origemSel - (vis - 1) : 0;
    gfx_vidro_sem_aro = 1; t21_ilha(panel, T21_R_SHEET, 1); gfx_vidro_sem_aro = 0;
    txt_tracking(TXT_MINI, seloCaixaAlta("Origens"), 115, 117, 122, panel.x + 30, panel.y + 20, 1.0f, 2.2f);
    for (int i = start; i < BIB_N_FONTES && i < start + vis; i++) {
      GfxRect row = {panel.x + 12, panel.y + 50 + (i - start) * 72, 296, 72};
      float tx = row.x + 18;
      ponteiro_alvo(row.x, row.y, row.w, row.h, ponteiroOrigem, NULL, i, 0);
      plrui_linha_foco(row, T21_R_ROW, i < 64 ? animOrigem[i] : 0);
      gfx_cor((GfxRect){tx, row.y + 19, 34, 34}, 10.0f / 34, 1, 1, 1, .08f);
      GLuint brand = i >= FONTE_PLEX ? tex_obter_larg(logos[i - FONTE_PLEX], 28) : 0;
      if (brand) gfx_rect((GfxRect){tx + 4, row.y + 23, 26, 26}, brand, GFX_MARCA, 0, 0, 0, 0, 1, 1, 1, 1);
      else {
        char l[2] = { i < FONTE_PLEX ? letra[i] : rotuloOrigem(i, 1)[0], 0 };
        TxtLinha t = txt_linha(TXT_HERO_SEC, l, 235, 235, 235, 255);
        txt_desenhar(t, tx + (34 - t.w) / 2, row.y + 19 + (34 - t.h) / 2);
      }
      tx += 48;
      if (fonte == i) {
        float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
        gfx_cor((GfxRect){tx, row.y + 27, 7, 7}, .5f, ar, ag, ab, 1);
        tx += 16;
      }
      txt_desenhar(txt_linha_corta(TXT_ILHA_NOME, rotuloOrigem(i, 1), 243, 242, 239, 255, row.x + row.w - tx - 48), tx, row.y + 12);
      txt_desenhar(txt_linha_corta(TXT_MINI, sub[i], 98, 100, 106, 255, row.x + row.w - tx - 48), tx, row.y + 42);
      if (fonte == i && !lst_carregando()) {
        char num[16]; snprintf(num, sizeof num, "%d", nCelulas);
        TxtLinha ct = txt_linha(TXT_HERO_META, num, 140, 142, 148, 255);
        txt_desenhar(ct, row.x + row.w - 18 - ct.w, row.y + 24);
      }
    }
    return;
  }
  // The mock's island and row rhythm stay in physical pixels at 85–100%.
  float panelX = paginaX() * gfx_escala_txt();
  ESCALA_REAL_INI();
  GfxRect painel = { panelX, 388.0f - bibSubida(), 320.0f, fminf(700, 46 + nOrigens() * 78 + (serverRows ? 54 : 0) + 86) };
  // Opening/closing: the island grows out of the "Origens" chip and shrinks
  // back into it (p 0 = the chip's rect, 1 = the panel); the rows fade in
  // late, fade out early, and are clipped to the island while it moves.
  float p = botaoOrigens() ? animOrig : 1.0f, grupo = gfx_opacidade_grupo;
  int movendo = fabsf(p - 1.0f) > 0.001f;
  GfxRect forma = painel;
  if (movendo) {
    float hs = bibHS();
    GfxRect b = { hdrX() * hs, pickerY() * hs, larguraBotaoOrig() * hs, BIB_CHIP_H * hs };
    forma = (GfxRect){ b.x + (painel.x - b.x) * p, b.y + (painel.y - b.y) * p,
                       b.w + (painel.w - b.w) * p, b.h + (painel.h - b.h) * p };
    gfx_opacidade_grupo = grupo * anim_clamp(p * 3.0f, 0.0f, 1.0f);
  }
  // .vid of the mock: island glass without the light rim.
  gfx_vidro_sem_aro = 1; t21_ilha(forma, T21_R_SHEET, 1); gfx_vidro_sem_aro = 0;
  if (movendo) {
    gfx_opacidade_grupo = grupo * anim_clamp((p - 0.45f) / 0.55f, 0.0f, 1.0f);
    gfx_recorte(forma.x, forma.y, forma.w, forma.h);
  }
  // Heading as in the mock: tracked caps, dim.
  txt_tracking(TXT_MINI, seloCaixaAlta(i18n("Origens")), 115, 117, 122, painel.x + 30, painel.y + 20, 1.0f, 2.2f);
  const char *sub[] = { i18n("sem repetir títulos"), i18n("watchlist"), i18n("plano para ver"), i18n("salvos neste perfil") };
  float footerY = 0;
  const char *letra[] = { "", "T", "S", "N" };
  int vis = (int)((painel.h - 46 - 86) / 78), start = focoOrigem && origemSel > vis - 1 ? origemSel - (vis - 1) : 0;
  for (int i = start; i < nOrigens() && i < start + vis; i++) {
    const char *name = rotuloOrigem(i, 0);
    const char *description = i < BIB_N_ORIG ? sub[i] : i18n("lista do add-on");
    int count = i < BIB_N_ORIG ? contaOrigem[i] : 0;
    if (i >= BIB_N_ORIG && i < BIB_N_ORIG + nAddonOrigens) {
      count = addonOrigens[i - BIB_N_ORIG].count;
    } else if (i >= BIB_N_ORIG + nAddonOrigens) {
      const char *serverSub = NULL;
      if (!serverRows || !serverRows(i - BIB_N_ORIG - nAddonOrigens, &name, &serverSub, &count)) continue;
      description = serverSub ? serverSub : i18n("Biblioteca");
    }
    GfxRect row = { painel.x + 12, painel.y + 46 + (i - start) * 78, 296, 78 };
    ponteiro_alvo(row.x, row.y, row.w, row.h, ponteiroOrigem, NULL, i, 0);
    plrui_linha_foco(row, T21_R_ROW, i < 64 ? animOrigem[i] : 0);
    float tx = row.x + 18;
    if (i) {
      gfx_cor((GfxRect){tx, row.y + 21, 34, 34}, 10.0f / 34, 1, 1, 1, .08f);
      TxtLinha t = txt_linha(TXT_HERO_SEC, i < BIB_N_ORIG ? letra[i] : "A", 235, 235, 235, 255);
      int p = i - BIB_N_ORIG - nAddonOrigens;
      const char *logos[] = {"deploy/app/art/marcas/plex.png","deploy/app/art/marcas/jellyfin.png","deploy/app/art/marcas/emby.png"};
      GLuint brand = p >= 0 && p < 3 ? tex_obter_larg(logos[p], 28) : 0;
      if (brand) gfx_rect((GfxRect){tx + 4,row.y + 25,26,26},brand,GFX_MARCA,0,0,0,0,1,1,1,1);
      else txt_desenhar(t, tx + (34 - t.w) / 2, row.y + 21 + (34 - t.h) / 2);
      tx += 48;
    }
    if ((serverFilter < 0 && origem == i) || (serverFilter >= 0 && i == BIB_N_ORIG + nAddonOrigens + serverFilter)) {
      float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
      gfx_cor((GfxRect){tx, row.y + 29, 7, 7}, .5f, ar, ag, ab, 1);
      tx += 16;
    }
    txt_desenhar(txt_linha_corta(TXT_AJ_SEG, name, 243, 242, 239, 255, row.x + row.w - tx - 48), tx, row.y + 14);
    txt_desenhar(txt_linha(TXT_ILHA_HORA, i18n(description), 98, 100, 106, 255), tx, row.y + 44);
    char num[16]; snprintf(num, sizeof num, "%d", count);
    TxtLinha ct = txt_linha(TXT_HERO_META, num, 140, 142, 148, 255);
    txt_desenhar(ct, row.x + row.w - 18 - ct.w, row.y + 26);
    footerY = row.y + row.h;
  }
  if (serverRows && footerY && footerY + 52 < painel.y + painel.h - 90) {
    // The approved Library mock keeps server actions under the origins list:
    // one connect entry and one manage entry, both opening the same setup.
    txt_desenhar(txt_linha_corta(TXT_MINI, i18n("+ Conectar servidor"), 150, 153, 162, 255, 280), painel.x + 18, footerY + 8);
    txt_desenhar(txt_linha_corta(TXT_MINI, i18n("Gerenciar conexões"), 150, 153, 162, 255, 280), painel.x + 18, footerY + 30);
  }
  // Footer note of the mock: divider, then two muted lines.
  { float ny = painel.y + painel.h - 79;
    gfx_cor((GfxRect){painel.x + 12, ny, painel.w - 24, 1}, 0, 1, 1, 1, .07f);
    txt_desenhar(txt_linha_corta(TXT_ILHA_HORA, i18n("O + salva em Trakt e Simkl."), 98, 100, 106, 255, 260), painel.x + 30, ny + 14);
    txt_desenhar(txt_linha_corta(TXT_ILHA_HORA, i18n("Muda em Ajustes › Contas."), 98, 100, 106, 255, 260), painel.x + 30, ny + 36); }
  if (movendo) gfx_sem_recorte();
  gfx_opacidade_grupo = grupo;
  ESCALA_REAL_FIM();
  // Provider rows share this page's focus and title filters.
}

// Cabecalho da tela. Com uma lista aberta ele vira caminho de volta ("Listas ›
// nome"), e nao mais o titulo da tela: quem esta tres niveis dentro precisa ver
// onde esta, nao o nome do app.
static void desenhaCabecalho(void) {
  char caminho[260];
  const char *tit = modo == MODO_LISTAS ? (listaTab == 2 ? "Explorar listas" : listaTab == 3 ? "Trilhas" : listaTab == 1 ? "Seguindo" : "Minhas listas") : "Biblioteca";
  if (temAberta) {
    snprintf(caminho, sizeof caminho, "%s  ›  %s", i18n("Listas"), aberta.titulo);
    tit = caminho;
  }
  if (paginaLista() && !trilha) {
    // 6B: "LISTA · TRAKT · PÚBLICA" over the name; the rest is desenhaPaginaCab.
    char k[96];
    snprintf(k, sizeof k, "%s · Trakt · %s", i18n("Lista"), i18n(aberta.privada ? "privada" : "pública"));
    plrui_kicker(k, paginaX(), BIB_TOPO, 115, 117, 122, 1);
    txt_desenhar(txt_linha_corta(TXT_TITULO2, aberta.titulo, 243, 242, 239, 255, 510), paginaX(), BIB_TOPO + 22);
    return;
  }
  // Layout Dinamica: "Biblioteca" esta na pilula da barra. Com uma lista
  // aberta sobra o nome dela, ao lado da pilula e centrado nela.
  { float px, py, pw, ph;
    if (menu_pilula_rect(&px, &py, &pw, &ph)) {
      // A pilula do menu e medida em px reais; o topo desenha na tela virtual.
      float hs = bibHS();
      px /= hs; py /= hs; pw /= hs; ph /= hs;
      if (temAberta) {
        float x0 = px + pw + NV_MENU_PILULA_VAO / hs;
        TxtLinha t = txt_linha_corta(TXT_HEADLINE, aberta.titulo, 255, 255, 255, 255,
                                     hdrX() + hdrW() - 320.0f - x0);
        txt_desenhar(t, x0, py + (ph - t.h) * 0.5f);
      }
    } else {
      TxtLinha t = txt_linha_corta(TXT_TITULO2, tit,
                                   245, 245, 243, 255, hdrW() - 320.0f);
      // Lists (design/2.1/listas.html): the "BIBLIOTECA › LISTAS" kicker sits
      // where the subtitle was, so the title steps down under it.
      float ty = BIB_TOPO;
      if (modo == MODO_LISTAS && !temAberta) {
        char k[96];   // seloCaixaAlta returns one static buffer: one call each
        snprintf(k, sizeof k, "%s › ", seloCaixaAlta("Biblioteca"));
        snprintf(k + strlen(k), sizeof k - strlen(k), "%s", seloCaixaAlta("Listas"));
        txt_tracking(TXT_MINI, k, 115, 117, 122, hdrX(), BIB_TOPO, 1.0f, 2.2f);
        ty += 22.0f;
      }
      // Compact header: the title sits on the tabs row, centred on it.
      if (bibCompacto()) ty = BIB_FAIXA_Y + (BIB_SEG_H - (float)t.h) * 0.5f + 6.0f;
      txt_desenhar(t, hdrX(), ty - (modo == MODO_LISTAS ? 0.0f : 6.0f));
    } }

  if (listasMinhas()) return;   // 6D: the list panel owns the right side
  if (!temAberta) {
    float x = hdrDir();
    char status[96];
    // "sincronizado ha 3 min" do mockup. Trakt/Simkl chegam pelo ciclo de
    // descoberta, a conta pelo sync.c: vale o instante mais recente dos dois.
    // Sem instante (nada terminou nesta sessao) nao ha linha: "ha 0 min"
    // inventaria relogio.
    unsigned ok = sync_ultimo_ok(), okDesc = desc_ultimo_ok(), t = SDL_GetTicks();
    if (okDesc && (!ok || (int)(okDesc - ok) > 0)) ok = okDesc;
    status[0] = 0;
    if (sync_estado() == SYNC_RODANDO) snprintf(status, sizeof status, "%s", i18n("sincronizando…"));
    else if (sync_estado() == SYNC_FALHOU) snprintf(status, sizeof status, "%s", i18n("sem sincronizar"));
    else if (ok && t > ok) {
      long long min = (t - ok) / 60000ull;
      if (min < 1) min = 1;
      snprintf(status, sizeof status, i18n("sincronizado há %d min"), (int)min);
    }
    // As origens remotas do mockup; a Conta Nuvio e local e fica so no painel.
    int active[] = { trakt_ativo(), simkl_ativo(), nAddonOrigens > 0 };
    const char *services[] = { "Trakt", "Simkl", nAddonOrigens > 0 ? addonOrigens[0].nome : "" };
#ifdef NV_J1_SHOT
    // Capture hook: pretend every source is connected and synced N minutes ago.
    { const char *fake = getenv("NUVIO_SHOT_SYNC_MIN");
      if (fake && fake[0]) {
        for (int i = 0; i < 3; i++) active[i] = 1;
        if (!nAddonOrigens) services[2] = "AIOMetadata";
        snprintf(status, sizeof status, i18n("sincronizado há %d min"), atoi(fake)); } }
#endif
    // Same baseline as the subtitle on the left, so the two read as one row.
    float lineY = resumoY();
    TxtLinha st = txt_linha(TXT_ILHA_APOIO, status, 105, 105, 105, 255);
    float y = lineY + (float)(txt_linha(TXT_CAPTION2, "Hg", 255, 255, 255, 255).h - st.h) * 0.5f;
    float ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    // .sinc: 18 entre itens, 6 entre o check (18) e o nome.
    if (st.w) { x -= st.w; txt_desenhar(st, x, y); x -= 18; }
    for (int i = 2; i >= 0; i--) if (active[i] && services[i][0]) {
      TxtLinha name = txt_linha(TXT_ILHA_APOIO, services[i], 146, 146, 146, 255);
      x -= name.w; txt_desenhar(name, x, y);
      x -= 6 + 18;
      gfx_icone((GfxRect){x, y + (name.h - 18) * 0.5f, 18, 18}, "aj_check", ar, ag, ab, 1);
      x -= 18;
    }
    return;
  }
  // Selo de origem, alinhado a direita da area util. Espacado de proposito: no
  // web ele tem letter-spacing 4 e le como etiqueta, nao como palavra.
  //
  // O SELO DIZ A ORIGEM DE VERDADE. Estava cravado em "NUVIO", que e o nome do
  // app e nao a fonte dos dados. Sem credencial do Trakt a biblioteca e local,
  // e o selo diz isso. Com conta E Trakt ao mesmo tempo, "Salvos" e uma MISTURA
  // das duas listas e nenhum rotulo unico e completo: fica a conta na frente,
  // que e quem responde a pergunta que o selo existe para responder — "isto
  // acompanha meus outros aparelhos?".
  { const char *f =
        temAberta ? rotuloFonte(aberta.fonte)
      : modo == MODO_LISTAS ? (fonte == FONTE_NUVIO ? "NUVIO"
                             : fonte == FONTE_SIMKL ? "SIMKL"
                             : fonte == FONTE_FIXADAS ? "LOCAL"
                             : "TRAKT")
      : modo == MODO_COLECOES ? (trakt_ativo() ? "TRAKT" : "LOCAL")
      : (contalib_tem_conta() ? seloCaixaAlta("Conta")
                              : ajustes_salvos_no_simkl() && simkl_ativo() ? "SIMKL"
                              : trakt_ativo() ? "TRAKT" : "LOCAL");
    // TRAKT SAI COMO LOGO, nao como palavra. As outras origens continuam texto
    // espacado porque nao ha wordmark delas no pacote — "SIMKL" e "NUVIO"
    // escritos sao o que existe, e inventar um desenho seria pior.
    //
    // 38 px DE CAIXA, e nao os ~19 do selo de texto. O numero saiu de olhar a
    // captura ampliada: o arquivo tem folga de ascendente e descendente, entao
    // a caixa de 30 que tentei primeiro deixava as letras com ~17 px de altura
    // visivel — menos do que o "LOCAL" que ele substitui. Com 38 as letras
    // ficam em ~22 px, na mesma presenca do texto antigo, e os ~100 px de
    // largura ainda cabem folgados na margem direita.
    //
    // GLASS UI (mockup tela 9): o selo e uma etiqueta discreta (16/700,
    // espacamento .14em, branco a 40%) na LINHA DE BASE do resumo, e nao mais
    // um logo de 38 ao lado do titulo. O wordmark do Trakt fica, no tamanho em
    // que as letras dele medem o mesmo que as do selo de texto (caixa de 24 ->
    // ~13 px de letra) e na mesma tinta apagada.
    { float yBase = BIB_TOPO + 81.0f;   // o fim da caixa do resumo
      if (!strcmp(f, "TRAKT")) {
        float w = marcaTraktLargura(24.0f);
        if (w > 0.0f) {
          marcaTrakt(hdrDir() - w, yBase - 24.0f + 3.0f, 24.0f, 255, 0.42f);
          return;
        }
      }
      { TxtLinha m = txt_linha(TXT_HERO_SEC, "Hg", 104, 104, 104, 255);
        float w = txt_tracking(TXT_HERO_SEC, f, 104, 104, 104, -1.0f, 0.0f, 0.0f, 2.2f);
        txt_tracking(TXT_HERO_SEC, f, 104, 104, 104,
                     hdrDir() - w, yBase - (float)m.h, 1.0f, 2.2f); } } }
}

// Resumo a direita da barra de modos, e o recado da ultima acao quando houver.
static void desenhaResumo(void) {
  if (paginaLista() && !trilha) return;
  char resumo[220];
  const char *txt = resumo;
  if (recadoAte > 0.0f && recado[0]) txt = recado;
  else if (temAberta) {
    snprintf(resumo, sizeof resumo, "%d %s   ·   %s", nCelulas,
             i18n(nCelulas == 1 ? "título" : "títulos"),
             i18n(lst_itens_carregando() ? "Carregando" : "Voltar: lista de listas"));
  } else if (modo == MODO_LISTAS) {
    resumo[0] = 0;   // the kicker replaces the "N listas" subtitle (mock)
  } else {
    // "48 títulos para ver" do mockup: um formato por aba, inteiro na tabela
    // (a chave e o portugues da string toda; ver issue #3).
    int um = nFiltro == 1;
    snprintf(resumo, sizeof resumo, i18n(
      modo == MODO_PARAVER    ? (um ? "%d título para ver" : "%d títulos para ver")
    : modo == MODO_ANDAMENTO  ? (um ? "%d título em andamento" : "%d títulos em andamento")
    : modo == MODO_ASSISTIDOS ? (um ? "%d título assistido" : "%d títulos assistidos")
    :                           (um ? "%d título nas coleções" : "%d títulos nas coleções")), nFiltro);
    // "· perfil Marina" do mockup: de quem e esta lista.
    { const ContaPerfil *pf = perfis_item_ativo();
      if (pf && pf->nome[0]) {
        size_t k = strlen(resumo);
        snprintf(resumo + k, sizeof resumo - k, " · %s %s", i18n("perfil"), pf->nome); } }
  }
  // O TETO DA GRADE NUNCA E CALADO. Com a exibicao de lista (uma coluna) a
  // grade para em BIB_MAX_LINHAS titulos; o contador acima continua dizendo
  // quantos existem, e esta parte diz onde esta o resto.
  if (estado() != EST_ITENS && txt == resumo && nCelulas > BIB_MAX_LINHAS * colunas()) {
    size_t k = strlen(resumo);
    snprintf(resumo + k, sizeof resumo - k, "   ·   %s",
             i18n("o resto na exibição em grade"));
  }
  // A DICA DE OK, UMA VEZ SO. Ela era repetida dentro das tres caixas dos
  // seletores; agora entra na frente do resumo, e SO enquanto o foco esta na
  // faixa de seletores — que e quando ela responde alguma coisa. Uma linha
  // propria abaixo da faixa (tentada primeiro) caia em cima do topo da grade.
  if (!temAberta && foco.fileira == BIB_FIL_PICK && txt == resumo) {
    char comDica[260];
    snprintf(comDica, sizeof comDica, "%s%s%s",
             i18n(modo == MODO_LISTAS && pickId(pickSel) >= 2 ? "OK: digitar" : "OK: alterar"),
             resumo[0] ? "   ·   " : "", resumo);
    snprintf(resumo, sizeof resumo, "%s", comDica);
  }
  // GLASS UI: o resumo e o SUBTITULO da pagina (mockup: "14 titulos · sua
  // watchlist do Trakt", 19 a 55% logo abaixo do titulo) e nao mais um texto
  // espremido a direita da faixa dos filtros. 21 aqui, pela medida de BIB_TOPO.
  // O teto deixa a margem direita para o selo de origem.
  { TxtLinha info = txt_linha_corta(TXT_CAPTION2, txt, 140, 140, 140, 255,
                                    hdrW() - 280.0f);
    txt_desenhar(info, hdrX(), resumoY()); }
}

// "2024-03-01T10:00:00.000Z" -> "01/03/2024". "" quando o Trakt nao mandou.
static void dataCurta(const char *iso, char *dst, size_t n) {
  dst[0] = 0;
  if (strlen(iso) >= 10 && iso[4] == '-' && iso[7] == '-')
    snprintf(dst, n, "%.2s/%.2s/%.4s", iso + 8, iso + 5, iso);
}

// CABECALHO DA PAGINA DA LISTA: descricao, quem fez e quando, os numeros e
// quantos a pessoa ja viu. Cada parte SO ENTRA quando o Trakt a informou
// (-1 / "" = nao informado): zero inventado vira "0 curtidas" numa lista que so
// nao trouxe o campo.
static void desenhaPaginaCab(void) {
  if (!trilha) {
    // Mock 6B, left column: count, curator, description, numbers, progress.
    float x = paginaX(), ar, ag, ab;
    char s[160];
    CatItem it; DescItemLista ex;
    ajustes_acento(&ar, &ag, &ab);
    snprintf(s, sizeof s, i18n(nCelulas == 1 ? "%d título" : "%d títulos"), nCelulas);
    txt_desenhar(txt_linha(TXT_ILHA_SUB, s, 160, 162, 168, 255), x, 214);
    if (aberta.autor[0]) {
      static const float AV[][3] = { {.23f,.51f,.96f}, {.93f,.29f,.53f}, {.06f,.65f,.54f}, {.96f,.62f,.04f}, {.55f,.36f,.96f} };
      const float *c = AV[(unsigned char)aberta.autor[0] % 5];
      char ini[2] = { (char)toupper((unsigned char)aberta.autor[0]), 0 };
      gfx_cor((GfxRect){x, 258, 36, 36}, .5f, c[0], c[1], c[2], 1);
      TxtLinha t = txt_linha(TXT_MINI, ini, 255, 255, 255, 255);
      txt_desenhar(t, x + (36 - t.w) / 2, 258 + (36 - t.h) / 2);
      snprintf(s, sizeof s, "@%s", aberta.autor);
      t = txt_linha_corta(TXT_ILHA_NOME, s, 243, 242, 239, 255, 460);
      txt_desenhar(t, x + 48, 276 - t.h / 2);
    }
    txt_bloco_corta(TXT_ILHA_CORPO, aberta.descricao, 195, 197, 203, x, 314, 510, 28, 1, 4);
    s[0] = 0;
    if (aberta.curtidas >= 0) snprintf(s, sizeof s, "%d %s", aberta.curtidas, i18n(aberta.curtidas == 1 ? "curtida" : "curtidas"));
    if (aberta.comentarios >= 0) { size_t k = strlen(s); snprintf(s + k, sizeof s - k, "%s%d %s", k ? "  ·  " : "", aberta.comentarios, i18n(aberta.comentarios == 1 ? "comentário" : "comentários")); }
    txt_desenhar(txt_linha_corta(TXT_ILHA_HORA, s, 150, 152, 158, 255, 510), x, 442);
    snprintf(s, sizeof s, i18n("Você viu %d de %d"), pagVistos, nCelulas);
    txt_desenhar(txt_linha(TXT_ILHA_HORA, s, 220, 222, 228, 255), x, 484);
    for (int i = 0; i < nCelulas && i < 200; i++)
      if (lst_item(i, &it) && !cat_visto(&it)) {
        snprintf(s, sizeof s, "%s: %d", i18n("próximo"), lst_item_extra(i, &ex) && ex.rank > 0 ? ex.rank : i + 1);
        TxtLinha t = txt_linha(TXT_ILHA_HORA, s, 150, 152, 158, 255);
        txt_desenhar(t, x + 510 - t.w, 484);
        break;
      }
    GfxRect track = {x, 514, 510, 6};
    gfx_cor(track, .5f, 1, 1, 1, .12f);
    if (nCelulas && pagVistos) { track.w *= (float)pagVistos / nCelulas; gfx_cor(track, .5f, ar, ag, ab, 1); }
    // Middle column heading.
    plrui_kicker("Títulos, com a nota do curador", bibX() + 18, BIB_TOPO, 115, 117, 122, 1);
    // History: a timeline, oldest on top, the newest entry on an accent dot.
    struct { int i; long long ms; } h[64];
    int nh = 0;
    for (int i = 0; i < nCelulas && nh < 64; i++)
      if (lst_item_extra(i, &ex) && ex.entrouMs) { h[nh].i = i; h[nh++].ms = ex.entrouMs; }
    for (int a = 1; a < nh; a++)
      for (int b = a; b > 0 && h[b - 1].ms > h[b].ms; b--) { __typeof__(h[0]) t = h[b]; h[b] = h[b - 1]; h[b - 1] = t; }
    int de = nh > 6 ? nh - 6 : 0;
    GfxRect history = {bibDireita() - 424, 120, 424, 88 + (nh - de) * 88 + 64};
    t21_ilha(history, T21_R_SHEET, 1);
    plrui_kicker("História da lista", history.x + 26, 150, 115, 117, 122, 1);
    float y = 186, lx = history.x + 34;
    if (nh - de > 1) gfx_cor((GfxRect){lx - 1, y + 12, 2, (nh - de - 1) * 88}, 0, 1, 1, 1, .14f);
    for (int k = de; k < nh; k++) {
      time_t seconds = h[k].ms / 1000; struct tm tmv; char date[32] = "";
      if (!lst_item(h[k].i, &it)) continue;
      if (localtime_r(&seconds, &tmv)) strftime(date, sizeof date, "%d/%m/%Y", &tmv);
      if (k == nh - 1) gfx_cor((GfxRect){lx - 8, y + 4, 16, 16}, .5f, ar, ag, ab, 1);
      else { gfx_cor((GfxRect){lx - 8, y + 4, 16, 16}, .5f, .55f, .56f, .6f, 1);
             gfx_cor((GfxRect){lx - 6, y + 6, 12, 12}, .5f, .09f, .09f, .1f, 1); }
      txt_tracking(TXT_MINI, date, 125, 127, 133, lx + 22, y, 1.0f, 1.5f);
      snprintf(s, sizeof s, "+1 · %s", it.titulo);
      txt_desenhar(txt_linha_corta(TXT_HERO_META, s, 235, 236, 240, 255, 340), lx + 22, y + 26);
      y += 88;
    }
    gfx_cor((GfxRect){history.x + 26, y + 4, history.w - 52, 1}, 0, 1, 1, 1, .07f);
    txt_desenhar(txt_linha_corta(TXT_MINI, "O Trakt não diz o que saiu da lista.", 115, 117, 122, 255, history.w - 52), history.x + 26, y + 20);
    return;
  }
  float x = bibX(), y = BIB_GRADE_Y_ABERTA * bibHS() + 4.0f;
  char l2[240], l3[240], c1[16], c2[16];
  size_t k;
  if (aberta.descricao[0]) {
    txt_bloco_corta(TXT_CALLOUT, aberta.descricao, 214, 216, 222, x, y, bibW(), 32.0f, 1.0f, 2);
    y += BIB_PAG_DESC_H;
  }
  l2[0] = 0;
  if (aberta.autorNome[0] || aberta.autor[0])
    snprintf(l2, sizeof l2, "%s %s", i18n("por"),
             aberta.autorNome[0] ? aberta.autorNome : aberta.autor);
  dataCurta(aberta.criada, c1, sizeof c1);
  dataCurta(aberta.atualizada, c2, sizeof c2);
  k = strlen(l2);
  if (c1[0]) { snprintf(l2 + k, sizeof l2 - k, "%s%s %s", k ? "   ·   " : "", i18n("criada"), c1); k = strlen(l2); }
  if (c2[0]) snprintf(l2 + k, sizeof l2 - k, "%s%s %s", k ? "   ·   " : "", i18n("atualizada"), c2);
  if (aberta.privada) { k = strlen(l2); snprintf(l2 + k, sizeof l2 - k, "%s%s", k ? "   ·   " : "", i18n("privada")); }
  if (l2[0]) {
    TxtLinha t = txt_linha_corta(TXT_CAPTION2, l2, 150, 153, 162, 255, bibW());
    txt_desenhar(t, x, y);
    y += (float)t.h + 6.0f;
  }
  l3[0] = 0;
  snprintf(l3, sizeof l3, "%d %s", nCelulas, i18n(nCelulas == 1 ? "título" : "títulos"));
  if (aberta.curtidas >= 0) { k = strlen(l3); snprintf(l3 + k, sizeof l3 - k, "   ·   %d %s", aberta.curtidas, i18n(aberta.curtidas == 1 ? "curtida" : "curtidas")); }
  if (aberta.comentarios >= 0) { k = strlen(l3); snprintf(l3 + k, sizeof l3 - k, "   ·   %d %s", aberta.comentarios, i18n(aberta.comentarios == 1 ? "comentário" : "comentários")); }
  if (nCelulas > 0) { k = strlen(l3); snprintf(l3 + k, sizeof l3 - k, "   ·   %s %d/%d", i18n("você viu"), pagVistos, nCelulas); }
  { TxtLinha t = txt_linha_corta(TXT_CAPTION2, l3, 200, 203, 210, 255, bibW());
    txt_desenhar(t, x, y); }
}

// BARRA DE PRESSAO LONGA NO CARTAO, a mesma da Home (home.c: trilho de 4 px
// perto da base, preenchimento claro e a dica "Segure para opcoes"): enche em
// NV_HOLD_MS, o mesmo relogio que abre o menu em biblioteca_atualizar. So no
// cartao em foco que o OK esta segurando; desarma junto com okDesde.
static void barraHold(GfxRect r, float a) {
  float h;
  if (!okDesde || ctx_aberto()) return;
  h = anim_clamp((float)(SDL_GetTicks() - okDesde) / (float)NV_HOLD_MS, 0.0f, 1.0f);
  if (h <= 0.0f) return;
  { float g = r.w > 400.0f ? 28.0f : 16.0f;
    float bx = r.x + g, bw = r.w - g * 2.0f;
    GfxRect trilho = { bx, r.y + r.h - 16.0f, bw, 4.0f };
    if (ajustes_vidro()) gfx_vidro_chip(trilho, 0.5f, 0.92f, a);
    else gfx_cor(trilho, 0.5f, 0.18f, 0.19f, 0.22f, 0.92f * a);
    gfx_cor((GfxRect){ bx, trilho.y, bw * h, trilho.h }, 0.5f, 0.92f, 0.93f, 0.96f, a);
    { TxtLinha dica = txt_linha(TXT_MINI, h >= 1.0f ? "Solte para abrir opções"
                                                     : "Segure para opções",
                                225, 228, 235, 255);
      txt_desenhar_alpha(dica, bx, trilho.y - 8.0f - (float)dica.h, 0.92f * a); } }
}

// COMPARTILHAR: o link publico da lista como QR (celbotao.c). Sem servidor e
// sem token: so o endereco que o Trakt ja mostra para qualquer um. Com o
// relogio na tela e a ilha que estica ate o corpo (folha de Fontes); sem ele,
// a mesma ilha centrada, na mola de entrada e saida.
#define COMP_W 900.0f
#define COMP_H(topo) ((topo) + 214.0f + celb_qr_lado() + 96.0f)
static void fechaCompartilhar(int a, int b) { (void)a; (void)b; compartilha = 0; }
static void corpoCompartilhar(GfxRect r, float a, void *u) {
  float dy = u ? NV_ILHA_H : 0, lado = celb_qr_lado(), y;
  if (a < .01f) return;
  float opacity = gfx_opacidade_grupo;
  gfx_opacidade_grupo *= a;
  if (compartilha && a > .5f) {
    ponteiro_camada();
    ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, fechaCompartilhar, 0, 0);
    ponteiro_alvo(r.x, r.y, r.w, r.h, NULL, NULL, 0, 0);
  }
  plrui_kicker(i18n("Compartilhar"), r.x + 48, r.y + dy + 36, 243, 242, 239, .55f);
  { TxtLinha t = txt_linha_corta(TXT_ILHA_TITULO, i18n("Aponte a câmera para abrir a lista"),
                                 243, 242, 239, 255, r.w - 96);
    txt_desenhar(t, r.x + 48, r.y + dy + 76); }
  y = r.y + dy + 152;
  celb_qr_desenhar(urlLista, r.x + (r.w - lado) * .5f, y, 1.0f);
  { TxtLinha url = txt_linha_corta(TXT_ILHA_SUB, urlLista, 243, 242, 239, 255, r.w - 96);
    txt_desenhar_alpha(url, r.x + (r.w - url.w) * .5f, y + lado + 20, .62f); }
  { const char *k[] = { "OK" }, *l[] = { i18n("Fechar") };
    plrui_dicas(k, l, 1, r.x + 48, r.y + r.h - 44, 0, 1); }
  gfx_opacidade_grupo = opacity;
}
static void desenhaCompartilhar(void) {
  float ix, iy, iw, ih;
  if (ajustes_relogio_ligado() && ilha_rect(&ix, &iy, &iw, &ih)) {
    // The shared island owns shape, dim and exit; it replays the last body.
    if (compartilha) ilha_corpo(COMP_W, COMP_H(NV_ILHA_H), corpoCompartilhar, urlLista);
    return;
  }
  if (compartA < .005f) return;
  GfxRect r = { (NV_TELA_W - COMP_W) * .5f, (NV_TELA_H - COMP_H(0)) * .5f, COMP_W, COMP_H(0) };
  gfx_sem_recorte();
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0, 0, 0, 0, T21_VEIL * compartA);
  float opacity = gfx_opacidade_grupo;
  gfx_opacidade_grupo *= compartA;
  gfx_transformar(r.x + r.w * .5f, r.y + r.h * .5f, anim_mistura(T21_SHEET_SCALE, 1, compartForma), 0, 0);
  t21_ilha(r, T21_R_MODAL, 1);
  gfx_opacidade_grupo = opacity;
  corpoCompartilhar(r, compartA, NULL);
  gfx_sem_transformar();
}

static void desenhaVistaAnterior(Uint32 agora) {
  float alpha = 1 - anim_suave(animVista);
  if (temAberta || alpha <= .005f || ajustes_animacoes_reduzidas()) {
    nVistaAnterior = 0; return;
  }
  int currentMode = modo, currentView = exibicao;
  modo = modoAnterior; exibicao = exibicaoAnterior;
  gfx_recorte(0, anteriorClipY, NV_TELA_W, BIB_GRADE_BASE - anteriorClipY);
  for (int i = 0; i < nVistaAnterior; i++) {
    const BibTransitionCell *cell = &vistaAnterior[i];
    float a = alpha * cell->alpha;
    if (cell->kind == 0) desenhaCartaz(&cell->item, cell->rect, cell->focus, a, NULL, agora);
    else if (cell->kind == 1) desenhaLinhaTitulo(&cell->item, -1, cell->rect.y, cell->focus, a);
    else if (cell->kind == 2) desenhaCartaoLista(&cell->lista, cell->rect, cell->focus, a);
    else desenhaLinhaLista(&cell->lista, cell->rect.y, cell->focus, a);
  }
  gfx_sem_recorte();
  modo = currentMode; exibicao = currentView;
}

static void desenhaTrilha(Uint32 agora) {
  int selected = celulaEmFoco();
  if (selected < 0) selected = 0;
  int first = selected / 8 * 8;
  float x = bibX(), step = BIB_CARD_W + BIB_CARD_GAP;
  float posterY = ANC(404);
  float captionH = BIB_TIT_GAP + 14 + 28 + txt_linha(TXT_MINI, "Hg", 255, 255, 255, 255).h;
  float noteY = posterY + BIB_POSTER_H + captionH + 12;
  gfx_recorte(x - 12, ANC(330), bibW() + 24, noteY - ANC(330));
  float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
  gfx_cor((GfxRect){x + step / 2 - trilhaX, ANC(360), step * 7, 3}, 0, 1, 1, 1, .16f);
  for (int k = 0; k < 8 && first + k < nCelulas; k++) {
    int index = first + k; CatItem ci;
    if (!lst_item(index, &ci)) continue;
    float entry = ondaEm ? revela_entra(ondaEm, revela_onda_atraso(k, 0), agora) : 1;
    float opacity = gfx_opacidade_grupo;
    gfx_opacidade_grupo *= entry;
    float cx = x + k * step + BIB_CARD_W / 2 - trilhaX;
    int done = cat_visto(&ci);
    float focus = animFoco[index / 8][index % 8];
    GfxRect circle = {cx - 22, ANC(340), 44, 44};
    gfx_cor(circle, .5f, done ? ar : .085f, done ? ag : .09f, done ? ab : .105f, 1);
    plrui_linha_foco(circle, 22, focus);
    char number[16]; snprintf(number, sizeof number, "%d", index + 1);
    TxtLinha n = txt_linha(TXT_HERO_SEC, number, done ? ajustes_tinta_foco() : 220, done ? ajustes_tinta_foco() : 220, done ? ajustes_tinta_foco() : 220, 255);
    txt_desenhar(n, cx - n.w / 2, circle.y + (circle.h - n.h) / 2);
    GfxRect poster = {cx - BIB_CARD_W / 2, posterY + (1 - entry) * NV_ENTRA_DY,
                      BIB_CARD_W, BIB_POSTER_H};
    desenhaCartaz(&ci, poster, focus, 1, &revArte[index / 8][index % 8], agora);
    ponteiro_alvo(poster.x, poster.y, poster.w, poster.h, ponteiroCelula, NULL, index, 0);
    gfx_opacidade_grupo = opacity;
  }
  gfx_sem_recorte();
  CatItem ci; DescItemLista ex;
  if (!lst_item(selected, &ci)) return;
  lst_item_extra(selected, &ex);
  float noteW = (bibDireita() - x - 40) * .66f;
  float noteH = fmaxf(120, ANC(T21_SAFE_B) - noteY);
  GfxRect note = {x, noteY, noteW, noteH};
  t21_ilha(note, T21_R_SHEET, 1);
  char heading[180]; snprintf(heading, sizeof heading, "%s %d · %s", i18n("Passo"), selected + 1, ci.titulo);
  txt_desenhar(txt_linha_corta(TXT_CAPTION2, heading, 180, 182, 188, 255, noteW - 60), x + 30, noteY + 20);
  txt_bloco_corta(TXT_CALLOUT, ex.nota[0] ? ex.nota : ci.sinopse, 220, 222, 228, x + 30, noteY + 60, noteW - 60, 30, 1, (int)((noteH - 74) / 30));
  GfxRect about = {x + noteW + 40, noteY, bibDireita() - x - noteW - 40, noteH};
  t21_ilha(about, T21_R_SHEET, 1);
  txt_desenhar(txt_linha(TXT_CAPTION2, "Sobre", 180, 182, 188, 255), about.x + 28, noteY + 20);
  txt_bloco_corta(TXT_HERO_META, aberta.descricao, 160, 162, 168, about.x + 28, noteY + 60, about.w - 56, 26, 1, (int)fmaxf(1, (noteH - 110) / 26));
  char progress[80]; snprintf(progress, sizeof progress, "%s %d/%d", i18n("você viu"), pagVistos, nCelulas);
  txt_desenhar(txt_linha(TXT_HERO_META, progress, 220, 222, 228, 255), about.x + 28, noteY + noteH - 34);
}

static void biblioteca_desenhar_(Uint32 agora) {
  int linhas, r, c, nc = colunas();
  float passoC, passoL, gy, gcy;
#ifdef NV_TOUCH_UI
  if (!teclado_aberto()) ponteiro_rolagem(toqueBibliotecaRolar);
#endif
  // Mesmo ajuste, mesma disciplina da home: o rebordo claro do GFX_CARD e
  // ligado aqui e DEVOLVIDO no fim, porque a variavel e global e as outras
  // telas desenham card tambem.
  gfx_borda_foco_atual = ajustes_borda_foco() ? 1.0f : 0.0f;
  // A tela ja foi limpa com a cor de fundo por glClearColor/glClear em main.c
  // antes de app_desenhar. Pintar por cima era uma camada de tela cheia jogada
  // fora por quadro — e o custo dominante nesta GPU e fill rate (gfx.c registra
  // que DUAS camadas de tela cheia derrubavam a Mali-G71 para ~40fps).

  { float headerScale = gfx_escala();
    gfx_escala_sair(bibHS());
    desenhaCabecalho();
    if (temAberta) desenhaAcoes();
    else {
      desenhaModos();
      for (int p = 0; p < bibNPicks(); p++)  desenhaPicker(p, animPick[p]);
      if (botaoOrigens()) desenhaBotaoOrig();
    }
    desenhaResumo();
    gfx_escala_sair(headerScale); }
  if (origensNaGrade() || (temOrigens() && animOrig > 0.002f)) desenhaOrigens();
  desenhaVistaAnterior(agora);
  nVistaAtual = 0; vistaModo = modo; vistaExibicao = exibicao;
  if (modo == MODO_ASSISTIDOS && modoLinha()) {
    float x = paginaX();
    const char *labels[] = { "Título", "Data", "Vezes", "Origens" };
    const float dx[] = {108, 700, 1030, 1160};
    float y = 384 - bibSubida();
    for (int i = 0; i < 4; i++) txt_desenhar(txt_linha(TXT_MINI, labels[i], 115, 117, 123, 255), x + dx[i], y);
    gfx_cor((GfxRect){x, y + 30, bibDireita() - x, 1}, 0, 1, 1, 1, .08f);
  }
  // CABECALHO DE COLUNAS da tabela de titulos (mockup "biblioteca-lista-
  // servidor"): uma unica etiqueta em caixa alta com tracking, na largura da
  // tabela, SEM linha divisoria — o espaco acima dos chips ja separa. As linhas
  // comecam BIB_TL_CAB abaixo (gradeY).
  if (modoLinha() && estado() == EST_TITULOS && modo != MODO_ASSISTIDOS)
    txt_tracking(TXT_ILHA_HORA, i18n("TÍTULO / ORIGEM / PROGRESSO / DISPONIBILIDADE"),
                 110, 112, 114, bibX() + 1.0f, BIB_GRADE_Y_TITULOS + 7.0f, 1.0f, 1.8f);
  if (ondaArmada && nCelulas) { ondaEm = agora ? agora : 1u; ondaArmada = 0; }
  if (paginaLista() && trilha && nCelulas) {
    desenhaTrilha(agora);
    if (teclado_aberto()) teclado_desenhar(agora);
    gfx_borda_foco_atual = 1;
    return;
  }
  if (paginaLista()) desenhaPaginaCab();

  if (nCelulas == 0) {
    desenhaVazio();
    if (teclado_aberto()) teclado_desenhar(agora);
    gfx_borda_foco_atual = 1.0f;
    return;
  }

  linhas = nLinhas();
  if (estado() != EST_ITENS && linhas > BIB_MAX_LINHAS) linhas = BIB_MAX_LINHAS;
  passoC = passoColuna(); passoL = passoLinha(); gy = gradeY();
  if (ondaArmada) { ondaEm = agora ? agora : 1u; ondaArmada = 0; }
  if (ondaEm && revela_onda_fim(ondaEm, agora)) ondaEm = 0;
  // Keep the visible part of the previous row until it leaves the grid.
  // The renderer maps this layout clip to the drawable (including Retina).
  // The boundary sits just under the tab pills (faixa ends at y 248), not at
  // the grid origin (284): art scrolls up close to the buttons, no black band.
  gcy = gy - 18.0f * bibRel();
  vistaClipY = gcy;
  gfx_recorte(0.0f, gcy, NV_TELA_W, BIB_GRADE_BASE - gcy);
  { int lin0 = passoL > 0.0f ? (int)(scrollY / passoL) : 0;

  // ITEM 4 (plx): recusa no ponto do pedido. A grade diz ao cache se esta em
  // movimento; um quadro parado nao recusa nada (o lookahead do item 1 cobre a
  // vizinhanca) e por isso a rolagem curta continua revelando cartazes prontos.
  // O limiar e o deles: MOVING_SPEED = 120 px/s medido na TV (card_motion.rs).
  tex_grade_movendo(fabsf(velY) > 120.0f);
  tex_grade_velocidade(velY / passoL);

  // Origens opening/closing: every poster glides from its slot in the grid as
  // it was (flip*) to its slot now, on the panel's own spring.
  float flipRestante = origensFlipRestante();
  int flip = botaoOrigens() && !modoLinha() && fabsf(flipRestante) > 0.001f;

  // Dois passes: o item focado escala 2% e precisa ser desenhado por ULTIMO,
  // senao o vizinho da direita corta a borda dele.
  for (int passe = 0; passe < 2; passe++)
    for (r = 0; r < linhas; r++) {
      float topo = gy + r * passoL - scrollY;
      float a;
      if (!flip && (topo - BIB_POSTER_H * BIB_FOCO_ESCALA * .5f >= BIB_GRADE_BASE ||
                    topo + alturaLinha() <= gcy)) continue;
      // Fade only the final visible strip, not the entire row as its top
      // crosses the header. The clip above protects the header itself.
      a = anim_clamp((topo + alturaLinha() - gcy) / BIB_FADE, 0.0f, 1.0f);
      if (!flip && a <= 0.005f) continue;
      // CARTAO FORA DA JANELA: fileira cortada pelo topo ou pelo rodape. Ela
      // ainda e desenhada (a faixa de fade), entao e NIVEL 1 (borda): o cache
      // so recusa em movimento. O cartao em FOCO nunca e tratado como fora.
      int fora = (topo < gcy || topo + alturaLinha() > NV_TELA_H) ? 1 : 0;
      // Fileira CORTADA no lado em que a rolagem entra (rolando): NIVEL 3 —
      // seus cards viram visiveis nos proximos quadros; recusa-los e o poster
      // que carrega na frente. Em repouso valem como borda comum (nivel 1).
      if (fora && tex_grade_direcao() > 0.0f && topo > NV_TELA_H - 4.0f) fora = 3;
      else if (fora && tex_grade_direcao() < 0.0f && topo + alturaLinha() < gcy + 4.0f) fora = 3;

      for (c = 0; c < nc; c++) {
        int i = r * nc + c;
        float f, entra, ac, ty, fx = 0.0f, fy = 0.0f, ar = a;
        if (i >= nCelulas) break;
        if (flip) {
          fx = flipOffset[i][0] * flipRestante;
          fy = flipOffset[i][1] * flipRestante;
          ar = anim_clamp((topo + fy + alturaLinha() - gcy) / BIB_FADE, 0.0f, 1.0f);
        }
        // Reflow can move a poster across rows: cull its animated position,
        // including the focus lift, before registering targets or requesting art.
        f = focoOrigem ? 0.0f : (r < BIB_MAX_LINHAS && c < BIB_COLUNAS_MAX) ? animFoco[r][c]
            : (float)focus_indice(&foco, gradeIni() + r, c);
        float lift = modoLinha() ? 0 : BIB_POSTER_H * BIB_FOCO_ESCALA * f * .5f;
        if (topo + fy - lift >= BIB_GRADE_BASE || topo + fy + alturaLinha() <= gcy || ar <= .005f) continue;
        int foraAnimado = flip ? (topo + fy < gcy || topo + fy + alturaLinha() > BIB_GRADE_BASE) : fora;
        tex_pedido_fora(foraAnimado && f <= 0.01f ? foraAnimado : 0);
        // O alvo e a celula em REPOUSO (sem a escala do foco nem a onda), uma
        // vez por celula, recortado a janela da grade.
        if (passe == 0) {
          GfxRect ra = modoLinha()
            ? (GfxRect){ bibX(), topo, bibW(), alturaLinha() }
            : estado() == EST_LISTAS
              ? (GfxRect){ bibX() + c * passoC, topo, larguraCartaoLista(), BIB_LC_H }
              : (GfxRect){ bibX() + c * passoC + fx, topo + fy, BIB_CARD_W, BIB_POSTER_H };
          ponteiro_alvo_faixa(ra.x, ra.y, ra.w, ra.h, gy, BIB_GRADE_BASE, ponteiroCelula, NULL, i, 0);
        }
        if ((passe == 0) == (f > 0.01f)) continue;
        // ONDA: atraso pela coluna e pela fileira visiveis (revela.h).
        entra = ondaEm ? revela_entra(ondaEm, revela_onda_atraso(c, r - lin0), agora)
                       : 1.0f;
        ac = ar * entra * anim_suave(animVista);
        ty = topo + fy + (1.0f - entra) * NV_ENTRA_DY;
        if (!temAberta && nVistaAtual < BIB_TRANSITION_CELLS) {
          BibTransitionCell *cell = &vistaAtual[nVistaAtual++];
          cell->rect = (GfxRect){bibX() + c * passoC, ty, BIB_CARD_W, BIB_POSTER_H};
          cell->focus = f; cell->alpha = ac;
          cell->kind = modoLinha() ? 1 : 0;
          if (estado() == EST_LISTAS) {
            const LstLista *list = lst_lista(i);
            if (list) cell->lista = *list;
            cell->kind = modoLinha() ? 3 : 2;
            cell->rect.w = larguraCartaoLista(); cell->rect.h = BIB_LC_H;
          } else {
            CatItem tmp;
            const CatItem *item = itemFiltro(filtro[i], &tmp);
            if (item) cell->item = *item;
          }
        }
        if (ac <= 0.005f) continue;

        if (estado() == EST_LISTAS) {
          const LstLista *l = lst_lista(i);
          if (modoLinha()) {
            desenhaLinhaLista(l, ty, f, ac);
            if (i == celulaEmFoco()) barraHold((GfxRect){ bibX(), ty, bibW(), alturaLinha() }, ac);
          } else {
            GfxRect rc = { bibX() + c * passoC, ty, larguraCartaoLista(), BIB_LC_H };
            desenhaCartaoLista(l, rc, f, ac);
            if (i == celulaEmFoco()) barraHold(rc, ac);
          }
          continue;
        }
        { CatItem tmp;
          const CatItem *ci;
          if (estado() == EST_ITENS) ci = lst_item(i, &tmp) ? &tmp : NULL;
          else                       ci = itemFiltro(filtro[i], &tmp);
          if (modoLinha()) {
            desenhaLinhaTitulo(ci, paginaLista() ? i : -1, ty, f, ac);
            if (i == celulaEmFoco()) barraHold((GfxRect){ bibX(), ty, bibW(), alturaLinha() }, ac);
          } else {
            GfxRect rp = { bibX() + c * passoC + fx, ty, BIB_CARD_W, BIB_POSTER_H };
            desenhaCartaz(ci, rp, f, ac,
                          (r < BIB_MAX_LINHAS && c < BIB_COLUNAS_MAX)
                            ? &revArte[r][c] : NULL, agora);
            if (i == celulaEmFoco()) {
              float e = 1.0f + BIB_FOCO_ESCALA * f, bw = rp.w * e, bh = rp.h * e;
              barraHold((GfxRect){ rp.x - (bw - rp.w) * 0.5f, rp.y - (bh - rp.h) * 0.5f, bw, bh }, ac);
            }
          } }
      }
      tex_pedido_fora(0);
    }
  }
  tex_pedido_fora(0);
  gfx_sem_recorte();
  if (listasMinhas()) desenhaPainelLista();
  if (teclado_aberto()) teclado_desenhar(agora);
  gfx_borda_foco_atual = 1.0f;

  static TexLookahead adiante;
  if (tex_lookahead_parado(&adiante, scrollY) &&
      (!botaoOrigens() || fabsf(origensFlipRestante()) <= .001f) && !ondaEm &&
      !teclado_aberto() && estado() != EST_LISTAS) {
    int primeira = -1, ultima = -1, pediu = 0;
    for (r = 0; r < linhas; r++) {
      float topo = gy + r * passoL - scrollY;
      if (topo <= NV_TELA_H && topo + alturaLinha() > gcy) {
        if (primeira < 0) primeira = r;
        ultima = r;
      }
    }
    int dir = adiante.direcao < 0 ? -1 : 1;
    for (int passo = 1; primeira >= 0 && passo <= 3 && !pediu; passo++) {
      r = (dir > 0 ? ultima : primeira) + passo * dir;
      if (r < 0 || r >= linhas) break;
      for (c = 0; c < nc && !pediu; c++) {
        int i = r * nc + c;
        CatItem tmp;
        if (i >= nCelulas) break;
        const CatItem *ci = estado() == EST_ITENS
          ? (lst_item(i, &tmp) ? &tmp : NULL) : itemFiltro(filtro[i], &tmp);
        const char *cam = ci ? posterprov_card_addon(ci->origem, ci->imdb,
                          ci->tmdb, ci->tipo, ci->poster) : NULL;
        pediu = tex_lookahead_pedir(cam, exibicao == VIS_LISTA ? 96 : (int)(BIB_CARD_W * gfx_escala_txt()));
      }
    }
  } else if (!ondaEm && !teclado_aberto() && estado() != EST_LISTAS) {
    // EM MOVIMENTO: a fileira que entra na borda ja foi pedida como nivel 3;
    // aquece a PRIMEIRA arte da fileira seguinte, na direcao da rolagem.
    static TexLookahead fileira;
    int d = tex_lookahead_mover(&fileira);
    if (d && passoL > 0.0f) {
      int r = (int)(scrollY / passoL) +
              (d > 0 ? (int)((NV_TELA_H - gy) / passoL) + 2 : -2);
      if (r >= 0 && r < linhas) {
        for (c = 0; c < nc; c++) {
          int i = r * nc + c;
          if (i >= nCelulas) break;
          CatItem tmp;
          const CatItem *ci = estado() == EST_ITENS
            ? (lst_item(i, &tmp) ? &tmp : NULL) : itemFiltro(filtro[i], &tmp);
          const char *cam = ci ? posterprov_card_addon(ci->origem, ci->imdb,
                            ci->tmdb, ci->tipo, ci->poster) : NULL;
          if (tex_lookahead_pedir(cam, exibicao == VIS_LISTA ? 96 : (int)(BIB_CARD_W * gfx_escala_txt())))
            break;
        }
      }
    }
  }
}

// ESCALA DE TEXTO E COMPONENTES (escala.h): as entradas publicas ligam a camada
// de texto; o corpo mede NV_TELA_W/H na tela virtual dela.
void biblioteca_evento(const SDL_Event *e) { TXT_INI(); biblioteca_evento_(e); TXT_FIM(); }
void biblioteca_atualizar(float dt, Uint32 agora) { TXT_INI(); biblioteca_atualizar_(dt, agora); TXT_FIM(); }
void biblioteca_desenhar(Uint32 agora) {
  TXT_INI();
  float opacity = gfx_opacidade_grupo;
  gfx_opacidade_grupo *= animTela;
  gfx_transformar(0, 0, 1, 0, (1 - animTela) * NV_ENTRA_DY);
  biblioteca_desenhar_(agora);
  gfx_sem_transformar();
  gfx_opacidade_grupo = opacity;
  desenhaCompartilhar();
  TXT_FIM();
}
