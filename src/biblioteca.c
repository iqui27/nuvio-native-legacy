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
#include "posterprov.h"
#include "contalib.h"
#include "salvos.h"
#include "artemetahub.h"
#include "extras.h"
#include "detail.h"
#include "badges.h"
#include "listas.h"
#include "teclado.h"
#include "trakt.h"
#include "simkl.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "focus.h"
#include "anim.h"
#include "revela.h"
#include "layout.h"
#include "ajustes.h"
#include "nlanc.h"
#include "ctxmenu.h"
#include "escala.h"
#include "ponteiro.h"

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
#define BIB_GRADE_Y_TITULOS BIB_GRADE_Y

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
#define BIB_LIN_H       168.0f
#define BIB_LIN_GAP      12.0f
#define BIB_LIN_PASSO   (BIB_LIN_H + BIB_LIN_GAP)
#define BIB_LIN_MINI_W   96.0f
#define BIB_LIN_MINI_H  144.0f
#define BIB_LIN_PAD      12.0f
#define BIB_LIN_TX_GAP   24.0f    // do cartaz ao texto
// A linha de uma LISTA (modo Listas em exibicao de lista) nao tem cartaz nem
// meta: continua nos 96 de antes. Crescer junto so por simetria deixaria uma
// linha de texto so boiando em 168 px.
#define BIB_LL_H         96.0f
#define BIB_LL_PASSO    112.0f
// A COLUNA DA NOTA, ancorada na direita da linha. Largura fixa: e ela que faz o
// olho descer a coluna de numeros em vez de cacar a nota em cada linha. O grupo
// e o mesmo selo do detalhe (NV_DETW2_IMDB_*), reaproveitado medida por medida.
#define BIB_COL_DIR      30.0f    // folga entre a coluna e a borda da linha
// O CHEVRON "›" no canto direito da linha EM FOCO, no lugar do travessao que
// ficava em toda linha sem nota. O travessao segurava a coluna, mas o dono o
// leu como "um – perdido": um sinal de ausencia repetido em metade das linhas
// vira ruido. A coluna continua alinhada porque o espaco do chevron e
// RESERVADO em toda linha; ele so e desenhado na focada, onde responde "OK
// abre". Sem nota a coluna fica vazia — com 168 px de linha e a meta cheia,
// vazio ali le como "sem nota", nao mais como buraco.
#define BIB_CHEV_W       22.0f
// Barra de progresso da retomada. Aparece SO quando ha progresso — ver a nota
// no desenho sobre por que ela nao reserva coluna e a nota reserva.
#define BIB_COL_PROG_W  132.0f
#define BIB_COL_PROG_H    8.0f
#define BIB_COL_PROG_GAP 44.0f

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
#define BIB_LC_COLS       5
#define BIB_LC_W        326.0f
#define BIB_LC_H        132.0f
#define BIB_LC_PASSO    350.0f
#define BIB_LC_LINHA    156.0f
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
#define BIB_TOPO          64.0f
#define BIB_FAIXA_Y      193.0f
#define BIB_SEG_H         55.0f   // .seg: 5 + (23 + 2 x 11) + 5
#define BIB_SEG_PAD        5.0f
#define BIB_SEG_ITEM_PADX 20.0f
#define BIB_SEG_ITEM_GAP   4.0f
#define BIB_CHIP_H        45.0f   // .vid dos filtros: 12 + 21 + 12
#define BIB_CHIP_PADX     20.0f
#define BIB_FAIXA_GAP     16.0f   // entre seletor, fio e chips
#define BIB_GRADE_Y      284.0f   // faixa + 36
#define BIB_COLUNAS_MAX      7
#define BIB_CARD_W       220.0f
#define BIB_POSTER_H     330.0f
#define BIB_CARD_GAP      26.0f
#define BIB_TIT_GAP       10.0f
#define BIB_LINHA_PASSO  422.0f   // 284 -> 706 no mockup
#define BIB_FOCO_ESCALA    0.08f
// Onde a grade comeca em cada estado: o mesmo y, com ou sem lista aberta — as
// acoes da lista ocupam a faixa dos filtros.
#define BIB_GRADE_Y_ABERTA BIB_GRADE_Y

// "Salvos" = QUERO VER, e ele tem DUAS fontes que caem na mesma marca
// (CatItem.naLista): a watchlist do Trakt, posta ali pela descoberta, e a
// biblioteca da CONTA (sync_pull_library), posta ali por contalib.c.
// "Coleção" = TENHO, e so o Trakt tem. "Listas" = conjuntos, nao titulos.
enum { MODO_SALVOS, MODO_NUVEM, MODO_LISTAS, BIB_N_MODOS };
static const char *ROT_MODO[BIB_N_MODOS] = { "Salvos", "Coleção", "Listas" };

// Seletor "Tipo": os mesmos valores do web.
enum { TIPO_TODOS, TIPO_FILME, TIPO_SERIE, BIB_N_TIPOS };
static const char *ROT_TIPO[BIB_N_TIPOS] = { "Todos", "Filmes", "Séries" };
// Seletor "Ordenar".
enum { ORD_ADICIONADOS, ORD_TITULO, ORD_ANO, BIB_N_ORD };
static const char *ROT_ORD[BIB_N_ORD] = { "Ordem da lista", "Título: A a Z", "Ano: mais recentes" };
// Seletor "Exibição".
enum { VIS_CARTAZ, VIS_LISTA, BIB_N_VIS };
static const char *ROT_VIS[BIB_N_VIS] = { "Cartazes", "Lista" };
// Seletor "Fonte", no modo Listas. "Públicas" e a busca — ela vive na mesma
// roda porque e mais uma origem de listas, nao um lugar separado.
enum { FONTE_TRAKT, FONTE_SIMKL, FONTE_NUVIO, FONTE_FIXADAS, FONTE_PUB, BIB_N_FONTES };
static const char *ROT_FONTE[BIB_N_FONTES] =
  { "Trakt", "Simkl", "Nuvio", "Fixadas", "Públicas" };

// Em que grade o foco esta. Nao e um modo a mais: e o mesmo modo Listas com uma
// lista aberta por cima.
enum { EST_TITULOS, EST_LISTAS, EST_ITENS };

static int modo = MODO_SALVOS;
static int tipo = TIPO_TODOS;
static int ordem = ORD_ADICIONADOS;
static int exibicao = VIS_CARTAZ;
static int fonte = FONTE_TRAKT;
static int pickSel = 0;          // qual dos tres seletores esta em foco

static LstLista aberta;          // lista aberta (EST_ITENS)
static int  temAberta;
static char abertaMidia[8] = "MOVIE";
static int  acaoSel = 0;
static char recado[160];         // resposta de uma acao ("Fixada", o porque do nao)
static float recadoAte;

// Indices visiveis. >= 0 e indice do CATALOGO; < 0 e um titulo da lista LOCAL
// de salvos que nao esta no catalogo agora, codificado como -(indice + 1) em
// salvos_item(). Ver itemFiltro.
static int filtro[CAT_MAX + SALVOS_MAX];
static int nFiltro = 0;
static int totalModo = 0;
// Quantos titulos cada modo de titulos tem (Salvos, Colecao), para a contagem
// do seletor segmentado. Refeito em reconstruir(), nunca por quadro.
static int contaModo[2];
static Foco foco;
static float animModo[BIB_N_MODOS];
static float animPick[3];
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
// Velocidade da mola de 2a ordem da rolagem (anim_mola2): partida macia e
// cauda exponencial, a MESMA curva que a home mede. A de 1a ordem que estava
// aqui partia na velocidade maxima e o primeiro quadro ja saltava 12%.
static float velY = 0.0f;
static int sair = 0, pedido = -1;
static int nCelulas = 0;         // celulas da grade do estado atual
static unsigned buscaAberta;     // 1 enquanto o teclado de busca esta na tela

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
  b = dados_ler(arquivoPref());
  if (!b) return;
  { const char *s = strstr(b, "exibicao\t");
    if (s) exibicao = atoi(s + 9) ? VIS_LISTA : VIS_CARTAZ; }
  free(b);
}
static void gravarPref(void) {
  char txt[160];
  snprintf(txt, sizeof txt,
           "# Exibicao da Biblioteca neste perfil. 0 = cartazes, 1 = lista.\n"
           "exibicao\t%d\n", exibicao);
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
// Primeira fileira de foco da grade. Com uma lista aberta nao ha seletores.
static int gradeIni(void) { return estado() == EST_ITENS ? 1 : BIB_FIL_GRADE; }
// A AREA UTIL SAI DA RAIL (26/09, dono: "quando a sidebar ta no modo pinned
// ela corta a interface"). NV_BIB_X/NV_BIB_W sao a medida do web com a barra
// RECOLHIDA — 96 de recuo, 1728 uteis. Presa, a rail cobre os primeiros 144 px
// e o titulo, as abas e a primeira coluna nasciam debaixo dela. Agora tudo
// parte de bibX() e a direita continua em NV_BIB_DIR: a tela encolhe pela
// esquerda, e quem nao cabe e COLUNA, nao pixel.
static float bibX(void) {
  float x;
  ajustes_area_conteudo(NV_BIB_X, NV_TELA_W - NV_BIB_DIR, &x, NULL);
  return x;
}
static float bibW(void) {
  float w;
  ajustes_area_conteudo(NV_BIB_X, NV_TELA_W - NV_BIB_DIR, NULL, &w);
  return w;
}
// O TOPO (modos, chips, botoes, titulo, resumo e selo) NASCE A 120% E ACOMPANHA
// o Tamanho da interface acima disso (dono, 03/10); a GRADE de cartazes fica no
// tamanho de sempre. O topo e medido e desenhado numa tela virtual de 1920/hs x
// 1080/hs (escala.h): hdrX/hdrW sao a area util em unidades virtuais — o mesmo
// canto esquerdo e a mesma largura reais de bibX/bibW — e a grade comeca
// DEPOIS do topo ampliado (gradeYBase). So biblioteca_desenhar liga a escala.
#define BIB_TOPO_ESCALA_MIN 1.2f
static float bibHS(void) { return escala_min(BIB_TOPO_ESCALA_MIN); }
static float hdrX(void) { return bibX() / bibHS(); }
static float hdrW(void) { return bibW() / bibHS(); }
static float hdrDir(void) { return NV_BIB_DIR / bibHS(); }
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
  return n < 1 ? 1 : n > BIB_COLUNAS_MAX ? BIB_COLUNAS_MAX : n;
}
static float larguraCartaoLista(void) {
  float w = (bibW() - (BIB_LC_COLS - 1) * (BIB_LC_PASSO - BIB_LC_W)) / BIB_LC_COLS;
  return w < BIB_LC_W ? w : BIB_LC_W;
}
static int colunas(void) {
  if (exibicao == VIS_LISTA) return 1;
  return estado() == EST_LISTAS ? BIB_LC_COLS : colunasCartaz();
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
  if (exibicao == VIS_LISTA) return estado() == EST_LISTAS ? BIB_LL_H : BIB_LIN_H;
  if (estado() == EST_LISTAS) return BIB_LC_H;
  // poster + 10 + o titulo em ate duas linhas (o focado abre a segunda)
  return BIB_POSTER_H + BIB_TIT_GAP + 14.0f + 52.0f;
}
static float passoLinha(void) {
  if (exibicao == VIS_LISTA) return estado() == EST_LISTAS ? BIB_LL_PASSO : BIB_LIN_PASSO;
  if (estado() == EST_LISTAS) return BIB_LC_LINHA;
  return BIB_LINHA_PASSO;
}
static float gradeY(void) {
  return (estado() == EST_ITENS ? BIB_GRADE_Y_ABERTA : BIB_GRADE_Y_TITULOS) * bibHS();
}
static int nLinhas(void) { return (nCelulas + colunas() - 1) / colunas(); }

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
  return n > LST_ITENS_MAX * 8 ? LST_ITENS_MAX * 8 : n;
}

// Refaz o mapa de foco. Chamada a cada troca de modo, fonte, tipo, ordem,
// exibicao — e quando a rede acrescenta itens —, porque o numero de colunas da
// ultima linha muda com o filtro e um foco apontando para uma coluna que nao
// existe mais desenha um retangulo vazio.
// Colunas com que o mapa de foco foi montado. A rail fixa muda colunas() por
// fora desta tela (a opcao fica em Ajustes); biblioteca_atualizar compara e
// refaz o mapa, senao a grade de 5 andaria com a navegacao de 6.
static int ncMapa = 0;
static void remapear(int preservar) {
  int linhas, cols[FOCUS_MAX_FILEIRAS], r, ini = gradeIni(), nc = colunas();
  ncMapa = nc;
  int fAntes = foco.fileira, cAntes = foco.coluna;
  nCelulas = contarCelulas();
  linhas = nLinhas();
  if (linhas > FOCUS_MAX_FILEIRAS - ini) linhas = FOCUS_MAX_FILEIRAS - ini;
  if (estado() == EST_ITENS) cols[0] = 3;          // as tres acoes
  else { cols[BIB_FIL_MODO] = BIB_N_MODOS; cols[BIB_FIL_PICK] = 3; }
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
  scrollY = 0.0f; velY = 0.0f;
  memset(animFoco, 0, sizeof animFoco); memset(revArte, 0, sizeof revArte);
  ondaArmada = 1; ondaEm = 0;
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
static int ehSerieSalvo(const SalvoItem *s) {
  return s && !strcmp(s->tipo, "series");
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
static int contarModo(int m) {
  int n = cat_n(), total = 0;
  if (n > CAT_MAX) n = CAT_MAX;
  nContados = 0;
  for (int i = 0; i < n; i++) {
    const CatItem *ci = cat_item(i);
    if (!ci) continue;
    if (!(m == MODO_SALVOS ? ci->naLista : (ci->naColecao || comprado[i]))) continue;
    if (jaNoFiltro(ci->imdb)) continue;
    if (nContados < CAT_MAX) contados[nContados++] = i;
    total++;
  }
  if (m == MODO_SALVOS)
    for (int k = salvos_n() - 1; k >= 0; k--) {
      const SalvoItem *sv = salvos_item(k);
      if (sv && sv->id[0] && cat_indice_por_imdb(sv->id) < 0) total++;
    }
  return total;
}

// Refaz a lista visivel de TITULOS (modos Salvos e Coleção).
static void reconstruir(void) {
  int n = cat_n();
  int exclTipo = 0, exclMeta = 0, locaisFora = 0;
  int anoAtual = 0;
  if (ajustes_ocultar_nao_lancados()) {
    time_t agora = time(NULL);
    struct tm tmv;
    if (gmtime_r(&agora, &tmv)) anoAtual = tmv.tm_year + 1900;
  }
  if (n > CAT_MAX) n = CAT_MAX;
  contaModo[0] = contarModo(MODO_SALVOS);
  contaModo[1] = contarModo(MODO_NUVEM);
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
    int entra = (modo == MODO_SALVOS) ? ci->naLista
                                      : (ci->naColecao || comprado[i]);
    if (!entra) continue;
    // UM CARTAZ POR TITULO. O mesmo titulo vive em varias fileiras, cada uma
    // com a sua copia marcada, e a serie com progresso tem id "tt123:1:2" ao
    // lado do "tt123" da watchlist — a grade mostrava o titulo duas vezes. A
    // regra e a de salvos_mesmo_titulo, a mesma do painel de Salvos.
    if (jaNoFiltro(ci->imdb)) continue;
    if (nContados < CAT_MAX) contados[nContados++] = i;
    totalModo++;
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
  if (modo == MODO_SALVOS) {
    for (int k = salvos_n() - 1; k >= 0 && nFiltro < (int)(sizeof filtro / sizeof filtro[0]); k--) {
      const SalvoItem *s = salvos_item(k);
      if (!s || !s->id[0] || cat_indice_por_imdb(s->id) >= 0) continue;
      totalModo++;
      locaisFora++;
      if (tipo == TIPO_FILME && ehSerieSalvo(s)) { exclTipo++; continue; }
      if (tipo == TIPO_SERIE && !ehSerieSalvo(s)) { exclTipo++; continue; }
      if (anoAtual && nlanc_meta_futuro(s->meta, anoAtual)) { exclMeta++; continue; }
      filtro[nFiltro++] = -k - 1;
    }
  }

  // Ordenacao por insercao — sao poucas dezenas de itens, uma vez por troca.
  if (ordem != ORD_ADICIONADOS) {
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
         contaModo[0], contaModo[1], totalModo, nContados, locaisFora,
         exclTipo, exclMeta, nCelulas, colunas(), nLinhas(), BIB_MAX_LINHAS);
  fflush(stdout);
}

// Pede a fonte escolhida ao modulo de listas. Cada uma custa no maximo UM
// pedido de rede (ver listas.h).
static void pedirFonte(void) {
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
int biblioteca_iniciar(void) {
  // Zerado UMA vez por processo, e nao a cada entrada.
  if (!iniciado) {
    memset(comprado, 0, sizeof comprado);
    iniciado = 1;
  }
  prefPerfil = -1;
  lerPref();
  lst_iniciar();
  modo = MODO_SALVOS; tipo = TIPO_TODOS; ordem = ORD_ADICIONADOS;
  pickSel = 0; temAberta = 0; acaoSel = 0; recado[0] = 0; recadoAte = 0.0f;
  buscaAberta = 0;
  memset(animModo, 0, sizeof animModo);
  memset(animPick, 0, sizeof animPick);
  sair = 0; pedido = -1;
  reconstruir();
  // O foco nasce na barra de modos: quem entra ainda esta escolhendo o recorte.
  foco.fileira = BIB_FIL_MODO;
  foco.coluna = modo;
  return 1;
}

void biblioteca_encerrar(void) { }

int biblioteca_na_lista(int i) {
  // A verdade e a marca DO ITEM, que a descoberta preenche com a watchlist do
  // Trakt. O vetor por indice que respondia aqui apontava para outro titulo
  // assim que o catalogo era reconstruido.
  const CatItem *c = cat_item(i);
  return c ? c->naLista : 0;
}
int biblioteca_comprado(int i) { return (i >= 0 && i < CAT_MAX) ? comprado[i] : 0; }
void biblioteca_alternar_lista(int i) {
  // So remonta a lista. Quem vira a marca e cat_definir_na_lista, no mesmo
  // ponto que fala com o Trakt (app.c) — ter DOIS donos do mesmo estado era o
  // que deixava a biblioteca discordando do botao "+" do detalhe.
  (void)i;
  if (modo != MODO_LISTAS) reconstruir();
}

int biblioteca_quer_sair(void) { return sair; }

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
  temAberta = 0;
  // O recado era da acao feita DENTRO da lista ("Adicionada à Home"). Deixa-lo
  // vivo na lista de listas o faz parecer resposta ao que a pessoa fez agora —
  // apareceu assim na captura, sobre a aba do Simkl.
  recado[0] = 0; recadoAte = 0.0f;
  remapear(0);
  foco.fileira = BIB_FIL_GRADE;
  foco.coluna = 0;
}

static void abrirLista(int i) {
  const LstLista *l = lst_lista(i);
  if (!l) return;
  aberta = *l;
  snprintf(abertaMidia, sizeof abertaMidia, "%s",
           l->midia[0] ? l->midia : "MOVIE");
  temAberta = 1;
  acaoSel = 0;
  recado[0] = 0;
  lst_abrir(&aberta, abertaMidia);
  remapear(0);
  foco.fileira = 0;
  foco.coluna = 0;
}

static void executarAcao(int a) {
  const char *porque = "";
  if (!temAberta) return;
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
  snprintf(abertaMidia, sizeof abertaMidia, "%s",
           strcasecmp(abertaMidia, "TV") ? "TV" : "MOVIE");
  lst_abrir(&aberta, abertaMidia);
  remapear(0);
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
  if (foco.fileira == BIB_FIL_MODO) {
    float x = hdrX() + BIB_SEG_PAD;
    for (int a = 0; a < foco.coluna && a < BIB_N_MODOS; a++) x += larguraModo(a) + BIB_SEG_ITEM_GAP;
    cx = x + larguraModo(foco.coluna < BIB_N_MODOS ? foco.coluna : 0) * 0.5f;
  } else {
    cx = pickerX(foco.coluna) + pickerLargura(foco.coluna) * 0.5f;
  }
  if (passo < 1.0f) return 0;
  return (int)((cx * bibHS() - bibX()) / passo);  // cx e virtual do topo
}

static void descerDaFaixa(void) {
  int c;
  if (!nCelulas) return;
  faixaNoPicker = foco.fileira == BIB_FIL_PICK;
  c = colunaSobFaixa();
  foco.fileira = BIB_FIL_GRADE;
  if (c >= foco.nColunas[BIB_FIL_GRADE]) c = foco.nColunas[BIB_FIL_GRADE] - 1;
  foco.coluna = c < 0 ? 0 : c;
}

static void subirParaFaixa(void) {
  if (faixaNoPicker) { foco.fileira = BIB_FIL_PICK; foco.coluna = pickSel; }
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
  if (estado() == EST_ITENS) { abrirItemDaLista(i); return; }
  if (estado() == EST_LISTAS) { abrirLista(i); return; }
  { int idx = indiceDaCelula(i);
    if (idx >= 0) pedido = idx; }
}

// SEGURAR OK NUMA CELULA: o menu de contexto. Cartaz de titulo/item = o MESMO
// menu do cartaz da home (ctxmenu.c); cartao de lista = o menu da lista, com o
// resumo do que tem nela.
static void menuNaCelula(int i) {
  if (i < 0 || i >= nCelulas) return;
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
    if (exibicao == VIS_CARTAZ && ci) {
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
static Uint32 okDesde;

static void eventoAberta(SDL_Keycode k) {
  if (foco.fileira == 0) {
    if (k == SDLK_RIGHT && acaoSel < 2) { acaoSel++; foco.coluna = acaoSel; return; }
    if (k == SDLK_LEFT  && acaoSel > 0) { acaoSel--; foco.coluna = acaoSel; return; }
    if (k == SDLK_LEFT) { sair = 1; return; }   // comeco da fileira: o app abre a barra
    if (k == SDLK_DOWN) { if (nCelulas) focus_mover_grade(&foco, 0, 1); return; }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) executarAcao(acaoSel);
    return;
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) return;  // KEYUP
  if (k == SDLK_RIGHT)     focus_mover_grade(&foco, 1, 0);
  else if (k == SDLK_LEFT) { if (!focus_mover_grade(&foco, -1, 0)) sair = 1; }
  else if (k == SDLK_DOWN) {
    focus_mover_grade(&foco, 0, 1);
    // Chegou ao fim do que baixou: pede a proxima pagina. O modulo recusa
    // sozinho quando a anterior veio curta.
    if (foco.fileira >= foco.nFileiras - 2) lst_itens_mais();
  } else if (k == SDLK_UP) {
    if (foco.fileira == 1) { foco.fileira = 0; foco.coluna = acaoSel; }
    else focus_mover_grade(&foco, 0, -1);
  }
}

void biblioteca_evento(const SDL_Event *e) {
  // O TECLADO DA BUSCA E MODAL: enquanto ele esta na tela, ele fica com TODAS
  // as teclas. Deixar a grade responder por baixo foi o defeito que a busca de
  // codigo de amigo ja teve.
  if (teclado_aberto()) { teclado_evento(e); return; }
  { SDL_Keycode kk = e->key.keysym.sym;
    int ehOk = kk == SDLK_RETURN || kk == SDLK_KP_ENTER || kk == SDLK_SPACE;
    // SOLTAR O OK: se ele foi armado numa celula e o menu nao abriu, foi toque.
    // Sem armar (o KEYDOWN foi na faixa, ou antes de entrar nesta tela) nao ha
    // clique nenhum — a mesma guarda da home.
    if (e->type == SDL_KEYUP && ehOk) {
      Uint32 desde = okDesde;
      okDesde = 0;
      if (desde) okNaCelula(celulaEmFoco());
      return;
    }
    if (e->type == SDL_KEYDOWN && ehOk && !e->key.repeat) {
      int dentro = estado() == EST_ITENS ? foco.fileira >= 1 : foco.fileira >= BIB_FIL_GRADE;
      okDesde = dentro && celulaEmFoco() >= 0 ? (SDL_GetTicks() | 1u) : 0;
    } else if (e->type == SDL_KEYDOWN && !ehOk) okDesde = 0;
  }
  if (e->type != SDL_KEYDOWN) return;
  SDL_Keycode k = e->key.keysym.sym;
  if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE) {
    // Dentro de uma lista, o Back volta PARA A LISTA DE LISTAS antes de fechar
    // a tela — o movimento inverso do que levou ate la.
    if (temAberta) fecharAberta(); else sair = 1;
    return;
  }

  if (estado() == EST_ITENS) { eventoAberta(k); return; }

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
  if (foco.fileira == BIB_FIL_MODO) {
    if (k == SDLK_LEFT  && foco.coluna > 0) { foco.coluna--; return; }
    if (k == SDLK_LEFT) { sair = 1; return; }   // 1a aba: o app abre a barra
    if (k == SDLK_RIGHT && foco.coluna < BIB_N_MODOS - 1) { foco.coluna++; return; }
    if (k == SDLK_RIGHT) { pickSel = 0; foco.fileira = BIB_FIL_PICK; foco.coluna = 0; return; }
    if (k == SDLK_DOWN) { descerDaFaixa(); return; }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
      int novo = foco.coluna;
      if (novo != modo) {
        modo = novo;
        recado[0] = 0;
        if (modo == MODO_LISTAS) pedirFonte(); else reconstruir();
        foco.fileira = BIB_FIL_MODO; foco.coluna = modo;
      }
    }
    return;
  }

  // Seletores: esquerda/direita anda ENTRE os tres e, na ponta esquerda, volta
  // para a barra de modos; OK cicla o valor do que esta em foco. O web abre um
  // menu suspenso; num D-pad, ciclar no proprio seletor poupa a viagem de ida
  // e volta ate a lista.
  if (foco.fileira == BIB_FIL_PICK) {
    if (k == SDLK_RIGHT && pickSel < 2) { pickSel++; foco.coluna = pickSel; return; }
    if (k == SDLK_LEFT  && pickSel > 0) { pickSel--; foco.coluna = pickSel; return; }
    if (k == SDLK_LEFT)  { foco.fileira = BIB_FIL_MODO; foco.coluna = BIB_N_MODOS - 1; return; }
    if (k == SDLK_DOWN) { descerDaFaixa(); return; }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
      if (modo == MODO_LISTAS) {
        if (pickSel == 0) {
          fonte = (fonte + 1) % BIB_N_FONTES;
          pedirFonte();
        } else if (pickSel == 1) alternarExibicao();
        else {
          // O terceiro seletor no modo Listas E a busca publica. Ele abre a
          // modal de digitacao que o app ja tem (src/teclado.c) em vez de uma
          // segunda grade de letras — ver a nota no topo daquele arquivo.
          fonte = FONTE_PUB;
          buscaAberta = 1;
          teclado_abrir("Procurar listas públicas",
                        "Listas de qualquer pessoa no Trakt", TECLADO_MAX);
        }
      } else {
        if (pickSel == 0)      { tipo = (tipo + 1) % BIB_N_TIPOS;   reconstruir(); }
        else if (pickSel == 1) { ordem = (ordem + 1) % BIB_N_ORD;   reconstruir(); }
        else                     alternarExibicao();
      }
      foco.fileira = BIB_FIL_PICK; foco.coluna = pickSel;
    }
    return;
  }

  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) return;  // KEYUP
  // A grade da biblioteca e uma GRADE: manter a coluna ao subir e descer, e
  // nao voltar para a coluna onde o cursor esteve por ultimo naquela linha.
  if (k == SDLK_RIGHT)     focus_mover_grade(&foco, 1, 0);
  else if (k == SDLK_LEFT) { if (!focus_mover_grade(&foco, -1, 0)) sair = 1; }
  else if (k == SDLK_DOWN) focus_mover_grade(&foco, 0, 1);
  else if (k == SDLK_UP) {
    if (foco.fileira == BIB_FIL_GRADE) subirParaFaixa();
    else focus_mover_grade(&foco, 0, -1);
  }
}

// ---------------------------------------------------------------- atualizar

void biblioteca_atualizar(float dt, Uint32 agora) {
  int linhas, r, c, ini = gradeIni();
  (void)agora;
  lerPref();
  if (teclado_aberto()) teclado_atualizar(dt, agora);
  if (buscaAberta && !teclado_aberto()) {
    // O resultado e CONSUMIDO NA LEITURA (contrato de teclado.h): perguntar
    // duas vezes disparava duas buscas para o mesmo OK.
    int r2 = teclado_resultado();
    buscaAberta = 0;
    if (r2 == TECLADO_PRONTO) { lst_buscar(teclado_texto()); remapear(0); }
  }
  if (recadoAte > 0.0f) recadoAte -= dt;
  // SEGUROU O OK NA CELULA ATE O LIMIAR: abre o menu agora, com o dedo ainda no
  // botao, e desarma — o KEYUP seguinte nao e toque. O modal ignora o OK que
  // ainda esta afundado (ctxmenu.c, esperandoSoltura).
  if (okDesde && !ctx_aberto() && SDL_GetTicks() - okDesde >= NV_HOLD_MS) {
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
  for (int p = 0; p < 3; p++) {
    float alvo = (foco.fileira == (estado() == EST_ITENS ? 0 : BIB_FIL_PICK)
                  && foco.coluna == p) ? 1.0f : 0.0f;
    animPick[p] = anim_mola(animPick[p], alvo, dt,
                            alvo > animPick[p] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  }
  linhas = nLinhas();
  if (linhas > BIB_MAX_LINHAS) linhas = BIB_MAX_LINHAS;
  for (r = 0; r < linhas; r++)
    for (c = 0; c < colunas() && c < BIB_COLUNAS_MAX; c++) {
      float alvo = focus_indice(&foco, ini + r, c) ? 1.0f : 0.0f;
      animFoco[r][c] = anim_mola(animFoco[r][c], alvo, dt,
                                 alvo > animFoco[r][c] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
    }

  // Rola o MINIMO para a linha focada caber inteira na area util. Com o foco no
  // cabecalho o alvo e 0 — voltar ao topo faz parte de voltar para a barra.
  float alvo = scrollY;
  if (foco.fileira >= ini) {
    float topo = gradeY() + (foco.fileira - ini) * passoLinha();
    float base = topo + alturaLinha();
    if (base - alvo > BIB_GRADE_BASE)  alvo = base - BIB_GRADE_BASE;
    if (topo - alvo < gradeY())        alvo = topo - gradeY();
  } else {
    alvo = 0.0f;
  }
  if (alvo < 0.0f) alvo = 0.0f;
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
  else if (ajustes_vidro()) gfx_cor(r, raio, 1, 1, 1, .06f * (1.0f - v));
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
  float v = anim_clamp(f, 0.0f, 1.0f);
  if (ajustes_vidro()) gfx_cor(r, raio, 1, 1, 1, (.05f + .07f * v) * a);
  else {
    gfx_cor(r, raio, 0.10f, 0.11f, 0.13f, a);
    if (v > 0.01f) gfx_cor(r, raio, .17f, .176f, .204f, v * a);
  }
  return v > 0.5f ? bibTinta() : 235;
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
static float larguraModo(int a) {
  char num[16];
  float w = (float)txt_linha(TXT_ROW_TITULO, ROT_MODO[a], 140, 140, 140, 255).w * BIB_ESC_MODO;
  textoModo(a, num, sizeof num);
  if (num[0]) w += 9.0f + (float)txt_linha(TXT_CAPTION2, num, 93, 93, 93, 255).w * BIB_ESC_N;
  return w + BIB_SEG_ITEM_PADX * 2.0f;
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
  (void)b;
  if (estado() == EST_ITENS || a < 0 || a >= BIB_N_MODOS) return;
  foco.fileira = BIB_FIL_MODO; foco.coluna = a;
}
static void ponteiroPicker(int p, int b) {
  (void)b;
  if (estado() == EST_ITENS || p < 0 || p > 2) return;
  pickSel = p; foco.fileira = BIB_FIL_PICK; foco.coluna = p;
}
static void ponteiroAcao(int a, int b) {
  (void)b;
  if (estado() != EST_ITENS || a < 0 || a > 2) return;
  acaoSel = a; foco.fileira = 0; foco.coluna = a;
}
static void ponteiroCelula(int i, int b) {
  int nc = colunas();
  (void)b;
  if (i < 0 || i >= nCelulas || nc < 1) return;
  foco.fileira = gradeIni() + i / nc; foco.coluna = i % nc;
}

static void desenhaModos(void) {
  GfxRect c = { hdrX(), BIB_FAIXA_Y, larguraSeletor(), BIB_SEG_H };
  float x = c.x + BIB_SEG_PAD, ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  if (ajustes_vidro()) gfx_cor(c, 0.5f, 1, 1, 1, .06f);
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
    l = txt_linha(TXT_ROW_TITULO, ROT_MODO[a], t, t, t, 255);
    txtEsc(l, r.x + BIB_SEG_ITEM_PADX, r.y + (r.h - (float)l.h * BIB_ESC_MODO) * 0.5f,
           BIB_ESC_MODO, 1.0f);
    textoModo(a, num, sizeof num);
    if (num[0]) {
      // Na linha de BASE do nome (align-items: baseline), nao no meio dele.
      ln = txt_linha(TXT_CAPTION2, num, tn, tn, tn, 255);
      txtEsc(ln, r.x + BIB_SEG_ITEM_PADX + (float)l.w * BIB_ESC_MODO + 9.0f,
             r.y + (r.h + (float)l.h * BIB_ESC_MODO) * 0.5f - (float)ln.h * BIB_ESC_N - 1.0f,
             BIB_ESC_N, 1.0f);
    }
    x += w + BIB_SEG_ITEM_GAP;
  }
  // O fio vertical entre o seletor e os filtros (1 x 34, branco a 12%).
  gfx_cor((GfxRect){ c.x + c.w + BIB_FAIXA_GAP, c.y + (c.h - 34.0f) * 0.5f, 1.0f, 34.0f },
          0.0f, 1, 1, 1, .12f);
}

static void pickerTexto(int p, const char **rot, const char **val) {
  if (modo == MODO_LISTAS) {
    *rot = p == 0 ? "Fonte" : p == 1 ? "Exibição" : "Listas públicas";
    *val = p == 0 ? ROT_FONTE[fonte] : p == 1 ? ROT_VIS[exibicao] : "Procurar";
  } else {
    *rot = p == 0 ? "Tipo" : p == 1 ? "Ordenar" : "Exibição";
    *val = p == 0 ? ROT_TIPO[tipo] : p == 1 ? ROT_ORD[ordem] : ROT_VIS[exibicao];
  }
}

// O CHIP DE FILTRO (.vid do mockup, raio 24, 12/20 de recuo): o rotulo em
// cinza e o valor em branco 600, separados por um espaco — o "·" que existia
// aqui saiu com o mockup. A largura sai do texto.
static float pickerLargura(int p) {
  const char *rot, *val;
  pickerTexto(p, &rot, &val);
  return (float)txt_linha(TXT_CAPTION2, rot, 117, 117, 117, 255).w * BIB_ESC_ROT + 6.0f
       + (float)txt_linha(TXT_HERO_SEC, val, 245, 245, 245, 255).w * BIB_ESC_VAL
       + BIB_CHIP_PADX * 2.0f;
}

static float pickerX(int p) {
  float x = hdrX() + larguraSeletor() + BIB_FAIXA_GAP * 2.0f + 1.0f;
  int i;
  for (i = 0; i < p; i++) x += pickerLargura(i) + BIB_FAIXA_GAP;
  return x;
}

static void desenhaPicker(int p, float f) {
  const char *rot, *val;
  float w = pickerLargura(p);
  GfxRect r = { pickerX(p), BIB_FAIXA_Y + (BIB_SEG_H - BIB_CHIP_H) * 0.5f, w, BIB_CHIP_H };
  float v = anim_clamp(f, 0.0f, 1.0f);
  ponteiro_alvo(r.x, r.y, r.w, r.h, ponteiroPicker, NULL, p, 0);
  // Repouso: o vidro dos filtros (branco a 6%) ou, no solido, o #15161a do
  // .vid; foco: a pilula cheia no acento, como todo botao do app.
  if (ajustes_vidro()) gfx_cor(r, 0.5f, 1, 1, 1, .06f * (1.0f - v));
  else gfx_cor(r, 0.5f, .082f, .086f, .102f, 1.0f - v);
  if (v > 0.01f) {
    float ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    if (ajustes_vidro()) gfx_vidro_pilula_cheia(r, 0.5f, v, 1.0f);
    else { brilhoFoco(r, 0.9f, v, 1.0f); gfx_cor(r, 0.5f, ar, ag, ab, v); }
  }
  { int valor = v > 0.5f ? ajustes_tinta_foco() : 245;
    int rotulo = v > 0.5f ? ajustes_tinta_foco2() : 117;
    TxtLinha tr, tv;
    float x = r.x + BIB_CHIP_PADX;
    pickerTexto(p, &rot, &val);
    tr = txt_linha(TXT_CAPTION2, rot, rotulo, rotulo, rotulo, 255);
    tv = txt_linha(TXT_HERO_SEC, val, valor, valor, valor, 255);
    txtEsc(tr, x, r.y + (r.h - (float)tr.h * BIB_ESC_ROT) * 0.5f, BIB_ESC_ROT, 1.0f);
    x += (float)tr.w * BIB_ESC_ROT + 6.0f;
    txtEsc(tv, x, r.y + (r.h - (float)tv.h * BIB_ESC_VAL) * 0.5f, BIB_ESC_VAL, 1.0f); }
}

// Barra de acoes de uma lista aberta: fixar, levar para a Home e trocar o tipo
// de midia. Na faixa dos filtros, como CHIPS do mesmo corpo deles (a largura sai
// do texto) — eram tres pilulas de 520/520/300 x 72, o dobro de qualquer botao
// do mockup.
static float larguraAcao(const char *rot) {
  return (float)txt_linha(TXT_HERO_SEC, rot, 245, 245, 245, 255).w * BIB_ESC_VAL
       + BIB_CHIP_PADX * 2.0f + 8.0f;
}
static void desenhaAcoes(void) {
  float x = hdrX();
  int a;
  for (a = 0; a < 3; a++) {
    GfxRect r = { x, BIB_FAIXA_Y + (BIB_SEG_H - BIB_CHIP_H) * 0.5f, 0.0f, BIB_CHIP_H };
    const char *rot;
    int ligada;
    int cor;
    if (a == 0)      { ligada = lst_fixada(&aberta);
                       rot = ligada ? "Fixada na Biblioteca" : "Fixar na Biblioteca"; }
    else if (a == 1) { const char *porque;
                       ligada = lst_na_home(&aberta);
                       rot = !lst_aceita_home(&aberta, &porque) ? "Adicionar à Home"
                           : ligada ? "Na Home" : "Adicionar à Home"; }
    else             { ligada = 0;
                       rot = strcasecmp(abertaMidia, "TV") ? "Filmes" : "Séries"; }
    r.w = larguraAcao(rot);
    ponteiro_alvo(r.x, r.y, r.w, r.h, ponteiroAcao, NULL, a, 0);
    cor = pilula(r, 0.5f, animPick[a], ligada);
    { TxtLinha l = txt_linha(TXT_HERO_SEC, rot, cor, cor, cor, 255);
      txtEsc(l, r.x + (r.w - (float)l.w * BIB_ESC_VAL) * 0.5f,
             r.y + (r.h - (float)l.h * BIB_ESC_VAL) * 0.5f, BIB_ESC_VAL, 1.0f); }
    x += r.w + BIB_FAIXA_GAP;
  }
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
  if (estado() == EST_ITENS) {
    // DENTRO DE UMA LISTA o aviso da FONTE nao se aplica: ele fala do pedido
    // que traz as listas, nao dos itens desta. Reusa-lo aqui dizia "conecte sua
    // conta do Trakt" dentro de uma lista publica que nao precisa de conta
    // nenhuma — visto na captura, nao deduzido.
    dica = "↑ Voltar às ações   ·   Voltar: lista de listas";
    if (lst_itens_carregando()) { l1 = "Carregando os títulos"; l2 = "Um instante."; }
    else { l1 = "Nada nesta lista";
           l2 = "Uma lista do Trakt vem por tipo: experimente trocar entre Filmes e Séries."; }
  } else if (estado() == EST_LISTAS) {
    // O AVISO DA FONTE VEM DO MODULO e diz o que falta (sem conta do Trakt, sem
    // vinculo do Simkl, nada fixado). Uma frase generica aqui esconderia
    // exatamente a informacao que resolve.
    const char *av = lst_aviso();
    if (lst_carregando()) { l1 = "Carregando listas"; l2 = "Um instante."; }
    else if (av[0])       { l1 = "Nada para mostrar aqui"; l2 = av; }
    else                  { l1 = "Nada para mostrar aqui";
                            l2 = "Escolha outra fonte no seletor acima."; }
  } else {
    // "Salvos" com o "+" apontado para o Simkl e sem vinculo (issue #110): a
    // aba vazia diz o que falta, e nao "sua proxima sessao comeca aqui".
    const char *semSimkl = modo == MODO_SALVOS && !totalModo
        ? simkl_aviso_sem_vinculo(ajustes_salvos_no_simkl()) : NULL;
    l1 = semSimkl ? semSimkl
        : totalModo ? "Nenhum título neste filtro"
        : modo == MODO_NUVEM ? "Sua coleção aparece aqui" : "Sua próxima sessão começa aqui";
    l2 = semSimkl
        ? "O + salva no Plan to Watch do Simkl, e ele ainda não está vinculado nesta TV."
        : totalModo
        ? "Em Tipo, escolha Todos. Confira também os filtros em Ajustes."
        : modo == MODO_NUVEM
          ? "Os filmes e séries da sua coleção no Trakt ficam reunidos nesta aba."
          : "Abra um filme ou série e escolha Adicionar à lista para guardar.";
  }
  { TxtLinha t1 = txt_linha(TXT_TITULO2, l1, 255, 255, 255, 255);
    TxtLinha t2 = txt_linha_corta(TXT_CALLOUT, l2, 150, 153, 162, 255, bibW());
    float cx = bibX() + bibW() * 0.5f;
    float y = gradeY() + 190.0f;
    gfx_icone((GfxRect){cx - 32.0f, y - 100.0f, 64.0f, 64.0f},
               "menu_library", 0.70f, 0.70f, 0.72f, 1.0f);
    txt_desenhar_alpha(t1, cx - t1.w * 0.5f, y, 0.96f);
    txt_desenhar_alpha(t2, cx - t2.w * 0.5f, y + t1.h + 18.0f, 0.85f);
    { TxtLinha d = txt_linha(TXT_CAPTION2, dica, 150, 153, 162, 255);
      txt_desenhar(d, cx - d.w * 0.5f, y + t1.h + t2.h + 58.0f); } }
}

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
  float raio = raioPx(14.0f * esc, card.w, card.h);
  const char *arte = ci ? posterprov_card_addon(ci->origem, ci->imdb, ci->tmdb, ci->tipo, ci->poster) : NULL;
  if (arte && !arte[0]) arte = NULL;
  // O teto de decodificacao sai da largura pedida: a do cartaz EM FOCO (238),
  // para a arte nao amolecer quando ele sobe.
  GLuint tex = arte ? tex_obter_larg(arte, BIB_CARD_W * (1.0f + BIB_FOCO_ESCALA)) : 0;
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
    if (arte && !tex_falhou(arte))
      gfx_esqueleto(card, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G,
                    NV_COR_ESQUELETO_B, a);
    else
      gfx_cor(card, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G,
              NV_COR_ESQUELETO_B, a);
  }
  if (f > 0.01f && ajustes_borda_foco()) {
    float ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    if (ajustes_vidro()) gfx_vidro_cartao(card, raio, f, a);
    else gfx_anel(card, raio, NV_BIB_POSTER_BORDA, ar, ag, ab, f * a);
  }
  if (ci) {
    float ty = base.y + base.h + BIB_TIT_GAP + 14.0f * f;
    if (f > 0.5f) {
      // Em foco: 18/600 branco (o Bold de 18, sem reducao), em ate duas
      // linhas — "Killers of the Flower Moon" inteiro, como no mockup.
      txt_bloco_corta(TXT_HERO_SEC, ci->titulo, 255, 255, 255, base.x, ty, base.w,
                      22.0f, a, 2);
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

// O selo IMDb e o mesmo helper usado nas outras telas. Sem nota, a coluna nao
// ganha um travessao decorativo: a linha focada recebe apenas um chevron, que
// responde visualmente ao OK de abrir; em repouso o espaco fica limpo.
static float larguraNota(const CatItem *ci) {
  return ci && ci->nota > 0 ? badge_imdb_largura(ci->nota) : BIB_CHEV_W;
}

static void desenhaNota(const CatItem *ci, float xDir, float yCentro,
                        int tinta, float f, float a) {
  if (ci && ci->nota > 0) {
    float w = badge_imdb_largura(ci->nota);
    badge_imdb(xDir - w, yCentro - BADGE_H * 0.5f, ci->nota,
               0, a);
    return;
  }
  if (f > 0.5f) {
    int c = bibTinta2();
    TxtLinha t = txt_linha(TXT_HEADLINE, "›", c, c, c, 255);
    txt_desenhar_alpha(t, xDir - t.w, yCentro - t.h * 0.5f, a);
  } else {
    (void)tinta;
  }
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
static void desenhaLinhaTitulo(const CatItem *ci, float y, float f, float a) {
  GfxRect r = { bibX(), y, bibW(), BIB_LIN_H };
  int cor = superficieLinha(r, raioPx(18.0f, r.w, r.h), f, a);
  int sr, sg, sb;
  float yc = y + BIB_LIN_H * 0.5f;
  float xNotaDir = r.x + bibW() - BIB_COL_DIR;
  float xNotaIni = xNotaDir - larguraNota(ci);
  float xProgDir = xNotaIni - BIB_COL_PROG_GAP;
  GfxRect mini = { r.x + BIB_LIN_PAD, y + (BIB_LIN_H - BIB_LIN_MINI_H) * 0.5f,
                   BIB_LIN_MINI_W, BIB_LIN_MINI_H };
  const char *arte = ci ? posterprov_card_addon(ci->origem, ci->imdb, ci->tmdb, ci->tipo, ci->poster) : NULL;
  if (arte && !arte[0]) arte = NULL;
  GLuint tex = arte ? tex_obter_larg(arte, BIB_LIN_MINI_W) : 0;
  float raioMini = raioPx(8.0f, mini.w, mini.h);
  tintaSecundaria(cor, &sr, &sg, &sb);
  if (tex) {
    gfx_tex_aspect_atual = tex_aspecto(arte);
    gfx_rect(mini, tex, GFX_CARD, 0.0f, 0.0f, 0.0f, raioMini, 0, 0, 0, a);
    gfx_tex_aspect_atual = 0.0f;
  } else gfx_cor(mini, raioMini, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G,
                 NV_COR_ESQUELETO_B, a);
  if (!ci) return;

  // BARRA DE RETOMADA.
  //
  // O TRILHO CONTRASTA COM A LINHA, NAO COM A PAGINA — e a correcao que a
  // captura obrigou. Eu tinha posto o #202124 que o DESIGN.md chama de "calha",
  // mas aquele valor foi medido contra o fundo da PAGINA (#0D0D0D); sobre a
  // linha de repouso (#222) ele e mais ESCURO que a superficie, some, e a barra
  // vira um toco branco flutuando. O documento diz a regra certa ("o leito tem
  // de ser visivelmente mais claro que o painel"), e a regra so se cumpre
  // derivando o trilho da superficie em que ele esta.
  //
  // 100% NAO DESENHA NADA: titulo terminado nao esta "em andamento", e a barra
  // cheia seria lida como progresso parado no fim.
  if (ci->progresso > 0 && ci->progresso < 100) {
    GfxRect calha = { xProgDir - BIB_COL_PROG_W, yc - BIB_COL_PROG_H * 0.5f,
                      BIB_COL_PROG_W, BIB_COL_PROG_H };
    GfxRect ativo = calha;
    float raioB = raioPx(BIB_COL_PROG_H * 0.5f, calha.w, calha.h);
    float c = (float)cor / 255.0f;
    float t = (cor > 128) ? 0.26f : 0.62f;   // linha escura: trilho mais claro
    ativo.w = calha.w * (float)ci->progresso / 100.0f;
    if (ativo.w < BIB_COL_PROG_H) ativo.w = BIB_COL_PROG_H;
    gfx_cor(calha, raioB, t, t, t * 1.04f, a * 0.9f);
    gfx_cor(ativo, raioPx(BIB_COL_PROG_H * 0.5f, ativo.w, ativo.h), c, c, c, a);
  }

  desenhaNota(ci, xNotaDir, yc, cor, f, a);

  { float tx = mini.x + mini.w + BIB_LIN_TX_GAP;
    float larg = xProgDir - BIB_COL_PROG_W - 28.0f - tx;
    char sub[220];
    TxtLinha t, sl, ss = { 0, 0, 0 };
    linhaMeta(ci, sub, sizeof sub);
    if (larg < 80.0f) larg = 80.0f;
    t = txt_linha_corta(TXT_HEADLINE, ci->titulo, cor, cor, cor, 255, larg);
    sl = txt_linha_corta(TXT_CAPTION2, sub, sr, sg, sb, 255, larg);
    if (f > 0.5f && ci->sinopse[0])
      ss = txt_linha_corta(TXT_CAPTION, ci->sinopse, sr, sg, sb, 255, larg);
    { float hBloco = (float)t.h + 7.0f + (float)sl.h;
      if (ss.h) hBloco += 6.0f + (float)ss.h;
      // O bloco fica centrado na linha: a sinopse so aparece quando a linha
      // ja esta realmente em foco, sem roubar altura das linhas em repouso.
      float ty = yc - hBloco * 0.5f;
      txt_desenhar_alpha(t, tx, ty, a);
      txt_desenhar_alpha(sl, tx, ty + (float)t.h + 7.0f, a * 0.95f);
      if (ss.h)
        txt_desenhar_alpha(ss, tx, ty + (float)t.h + 7.0f + (float)sl.h + 6.0f,
                           a * 0.90f); } }
}

static const char *rotuloFonte(int f) {
  return f == LST_TRAKT ? "TRAKT" : f == LST_SIMKL ? "SIMKL" : "NUVIO";
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
  int cor = pilula(r, raioPx(20.0f, r.w, r.h), f, 0);
  int sr, sg, sb;
  char sub[220];
  float larg = r.w - BIB_LC_PAD * 2.0f;
  float y = r.y + BIB_LC_PAD;
  if (!l) return;
  tintaSecundaria(cor, &sr, &sg, &sb);
  subtituloLista(l, sub, sizeof sub);
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

static void desenhaLinhaLista(const LstLista *l, float y, float f, float a) {
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

// Cabecalho da tela. Com uma lista aberta ele vira caminho de volta ("Listas ›
// nome"), e nao mais o titulo da tela: quem esta tres niveis dentro precisa ver
// onde esta, nao o nome do app.
static void desenhaCabecalho(void) {
  char caminho[260];
  const char *tit = "Biblioteca";
  if (temAberta) {
    snprintf(caminho, sizeof caminho, "%s  ›  %s", i18n("Listas"), aberta.titulo);
    tit = caminho;
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
      txt_desenhar(t, hdrX(), BIB_TOPO);
    } }

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
      : modo == MODO_NUVEM ? (trakt_ativo() ? "TRAKT" : "LOCAL")
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
  char resumo[220];
  const char *txt = resumo;
  if (recadoAte > 0.0f && recado[0]) txt = recado;
  else if (temAberta) {
    snprintf(resumo, sizeof resumo, "%d %s   ·   %s", nCelulas,
             i18n(nCelulas == 1 ? "título" : "títulos"),
             i18n(lst_itens_carregando() ? "Carregando" : "Voltar: lista de listas"));
  } else if (modo == MODO_LISTAS) {
    snprintf(resumo, sizeof resumo, "%d %s", nCelulas,
             i18n(nCelulas == 1 ? "lista" : "listas"));
  } else {
    // AS PARTES passam por i18n, e nao a frase pronta: a chave da tabela e o
    // portugues INTEIRO de uma string, e "0 títulos   ·   Sua lista para
    // assistir" nunca vai existir como chave. Era este o defeito visto na
    // biblioteca com a interface em inglês (issue #3).
    snprintf(resumo, sizeof resumo, "%d %s   ·   %s", nFiltro,
             i18n(nFiltro == 1 ? "título" : "títulos"),
             i18n(modo == MODO_SALVOS ? "Sua lista para assistir"
                                      : "Sua coleção no Trakt"));
  }
  // O TETO DA GRADE NUNCA E CALADO. Com a exibicao de lista (uma coluna) a
  // grade para em BIB_MAX_LINHAS titulos; o contador acima continua dizendo
  // quantos existem, e esta parte diz onde esta o resto.
  if (txt == resumo && nCelulas > BIB_MAX_LINHAS * colunas()) {
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
    snprintf(comDica, sizeof comDica, "%s   ·   %s",
             i18n(modo == MODO_LISTAS && pickSel == 2 ? "OK: digitar" : "OK: alterar"),
             resumo);
    snprintf(resumo, sizeof resumo, "%s", comDica);
  }
  // GLASS UI: o resumo e o SUBTITULO da pagina (mockup: "14 titulos · sua
  // watchlist do Trakt", 19 a 55% logo abaixo do titulo) e nao mais um texto
  // espremido a direita da faixa dos filtros. 21 aqui, pela medida de BIB_TOPO.
  // O teto deixa a margem direita para o selo de origem.
  { TxtLinha t = txt_linha(TXT_TITULO2, "Hg", 255, 255, 255, 255);
    TxtLinha info = txt_linha_corta(TXT_CAPTION2, txt, 140, 140, 140, 255,
                                    hdrW() - 280.0f);
    txt_desenhar(info, hdrX(), BIB_TOPO + (float)t.h + 2.0f); }
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
    gfx_cor(trilho, 0.5f, 0.18f, 0.19f, 0.22f, 0.92f * a);
    gfx_cor((GfxRect){ bx, trilho.y, bw * h, trilho.h }, 0.5f, 0.92f, 0.93f, 0.96f, a);
    { TxtLinha dica = txt_linha(TXT_MINI, h >= 1.0f ? "Solte para abrir opções"
                                                     : "Segure para opções",
                                225, 228, 235, 255);
      txt_desenhar_alpha(dica, bx, trilho.y - 8.0f - (float)dica.h, 0.92f * a); } }
}

void biblioteca_desenhar(Uint32 agora) {
  int linhas, r, c, nc = colunas();
  float passoC, passoL, gy, gcy;
  // Mesmo ajuste, mesma disciplina da home: o rebordo claro do GFX_CARD e
  // ligado aqui e DEVOLVIDO no fim, porque a variavel e global e as outras
  // telas desenham card tambem.
  gfx_borda_foco_atual = ajustes_borda_foco() ? 1.0f : 0.0f;
  // A tela ja foi limpa com a cor de fundo por glClearColor/glClear em main.c
  // antes de app_desenhar. Pintar por cima era uma camada de tela cheia jogada
  // fora por quadro — e o custo dominante nesta GPU e fill rate (gfx.c registra
  // que DUAS camadas de tela cheia derrubavam a Mali-G71 para ~40fps).

  { ESCALA_MIN_INI(BIB_TOPO_ESCALA_MIN);   // so o topo; a grade fica a 100%
    desenhaCabecalho();
    if (temAberta) desenhaAcoes();
    else {
      desenhaModos();
      for (int p = 0; p < 3; p++)           desenhaPicker(p, animPick[p]);
    }
    desenhaResumo();
    ESCALA_MIN_FIM(); }

  if (nCelulas == 0) {
    desenhaVazio();
    if (teclado_aberto()) teclado_desenhar(agora);
    gfx_borda_foco_atual = 1.0f;
    return;
  }

  linhas = nLinhas();
  if (linhas > BIB_MAX_LINHAS) linhas = BIB_MAX_LINHAS;
  passoC = passoColuna(); passoL = passoLinha(); gy = gradeY();
  if (ondaArmada) { ondaEm = agora ? agora : 1u; ondaArmada = 0; }
  if (ondaEm && revela_onda_fim(ondaEm, agora)) ondaEm = 0;
  // Keep the visible part of the previous row until it leaves the grid.
  // The renderer maps this layout clip to the drawable (including Retina).
  // The boundary sits just under the tab pills (faixa ends at y 248), not at
  // the grid origin (284): art scrolls up close to the buttons, no black band.
  gcy = gy - BIB_CLIP_SOBE * bibHS();
  gfx_recorte(0.0f, gcy, NV_TELA_W, BIB_GRADE_BASE - gcy);
  { int lin0 = passoL > 0.0f ? (int)(scrollY / passoL) : 0;

  // Dois passes: o item focado escala 2% e precisa ser desenhado por ULTIMO,
  // senao o vizinho da direita corta a borda dele.
  for (int passe = 0; passe < 2; passe++)
    for (r = 0; r < linhas; r++) {
      float topo = gy + r * passoL - scrollY;
      float a;
      if (topo > NV_TELA_H || topo + alturaLinha() < -80.0f) continue;
      // Fade only the final visible strip, not the entire row as its top
      // crosses the header. The clip above protects the header itself.
      a = anim_clamp((topo + alturaLinha() - gcy) / BIB_FADE, 0.0f, 1.0f);
      if (a <= 0.005f) continue;

      for (c = 0; c < nc; c++) {
        int i = r * nc + c;
        float f, entra, ac, ty;
        if (i >= nCelulas) break;
        f = (c < BIB_COLUNAS_MAX) ? animFoco[r][c] : 0.0f;
        // O alvo e a celula em REPOUSO (sem a escala do foco nem a onda), uma
        // vez por celula, recortado a janela da grade.
        if (passe == 0) {
          GfxRect ra = exibicao == VIS_LISTA
            ? (GfxRect){ bibX(), topo, bibW(), estado() == EST_LISTAS ? BIB_LL_H : BIB_LIN_H }
            : estado() == EST_LISTAS
              ? (GfxRect){ bibX() + c * passoC, topo, larguraCartaoLista(), BIB_LC_H }
              : (GfxRect){ bibX() + c * passoC, topo, BIB_CARD_W, BIB_POSTER_H };
          ponteiro_alvo_faixa(ra.x, ra.y, ra.w, ra.h, gy, BIB_GRADE_BASE, ponteiroCelula, NULL, i, 0);
        }
        if ((passe == 0) == (f > 0.01f)) continue;
        // ONDA: atraso pela coluna e pela fileira visiveis (revela.h).
        entra = ondaEm ? revela_entra(ondaEm, revela_onda_atraso(c, r - lin0), agora)
                       : 1.0f;
        ac = a * entra;
        if (ac <= 0.005f) continue;
        ty = topo + (1.0f - entra) * NV_ENTRA_DY;

        if (estado() == EST_LISTAS) {
          const LstLista *l = lst_lista(i);
          if (exibicao == VIS_LISTA) {
            desenhaLinhaLista(l, ty, f, ac);
            if (i == celulaEmFoco()) barraHold((GfxRect){ bibX(), ty, bibW(), BIB_LL_H }, ac);
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
          if (exibicao == VIS_LISTA) {
            desenhaLinhaTitulo(ci, ty, f, ac);
            if (i == celulaEmFoco()) barraHold((GfxRect){ bibX(), ty, bibW(), BIB_LIN_H }, ac);
          } else {
            GfxRect rp = { bibX() + c * passoC, ty, BIB_CARD_W, BIB_POSTER_H };
            desenhaCartaz(ci, rp, f, ac,
                          (r < BIB_MAX_LINHAS && c < BIB_COLUNAS_MAX)
                            ? &revArte[r][c] : NULL, agora);
            if (i == celulaEmFoco()) {
              float e = 1.0f + BIB_FOCO_ESCALA * f, bw = rp.w * e, bh = rp.h * e;
              barraHold((GfxRect){ rp.x - (bw - rp.w) * 0.5f, rp.y - (bh - rp.h) * 0.5f, bw, bh }, ac);
            }
          } }
      }
    }
  }
  gfx_sem_recorte();
  if (teclado_aberto()) teclado_desenhar(agora);
  gfx_borda_foco_atual = 1.0f;
}
