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
//   .library-view-mode-row y=136: pilulas 150x56 raio 999, 21/400
//   .library-picker-row    y=212: seletores 110 de altura, raio 36, cada um com
//                          rotulo 19/500 rgb(128) e valor 30/500 branco embaixo
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
#include "biblioteca.h"
#include "contalib.h"
#include "extras.h"
#include "detail.h"
#include "listas.h"
#include "teclado.h"
#include "trakt.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "focus.h"
#include "anim.h"
#include "layout.h"
#include "ajustes.h"
#include "catalogo.h"
#include "dados.h"
#include "perfis.h"
#include "idioma.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>

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
#define BIB_TEXTO_ESCURO 20      // o mesmo de menu.c: texto sobre pilula clara

// TRES SELETORES onde havia dois. As medidas do web (840 de largura, passo 888)
// eram para dois; com "Exibição" no fim dos tres, 1728 uteis dividem em
// (1728 - 2*24)/3 = 560. O resto da folha — altura, raio, recuo — nao muda.
#define BIB_PICK_W     560.0f
#define BIB_PICK_PASSO 584.0f

// LISTA (a exibicao alternativa): uma linha por titulo, miniatura 2:3 a
// esquerda e uma COLUNA A DIREITA com a nota.
//
// A ALTURA CAIU DE 108 PARA 96, e a razao e a que o dono deu olhando a tela:
// "a linha em foco e grande demais para o pouco que diz". Ela carregava duas
// linhas curtas encostadas na esquerda e METADE DA LARGURA VAZIA. A resposta
// nao foi so encolher — foi dar trabalho ao lado direito (nota e progresso) e
// so entao apertar a altura. Miniatura 60x90 dentro de 96 deixa 3 px de folga.
//
// MEMORIA: 60 px de largura de desenho continua caindo no MESMO teto de decode
// que os 64 de antes. capDeLargura e ceil32(larg * escala * 1.25) com piso de
// 128, entao qualquer largura ate 102 custa o mesmo — MEDIDO em 48 KB por arte
// contra 363 KB do cartaz. Diminuir o cartaz aqui nao economizaria nada, e por
// isso a decisao foi tomada por legibilidade, nao por bytes.
#define BIB_LIN_H        96.0f
#define BIB_LIN_PASSO   112.0f
#define BIB_LIN_MINI_W   60.0f
#define BIB_LIN_MINI_H   90.0f
#define BIB_LIN_PAD      14.0f
// A COLUNA DA NOTA, ancorada na direita da linha. Largura fixa: e ela que faz o
// olho descer a coluna de numeros em vez de cacar a nota em cada linha. O grupo
// e o mesmo selo do detalhe (NV_DETW2_IMDB_*), reaproveitado medida por medida.
#define BIB_COL_NOTA_W   48.0f    // largura reservada ao numero, alinhado a direita
#define BIB_COL_DIR      30.0f    // folga entre o numero e a borda da linha
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

// Onde a grade comeca em cada estado. Com uma lista aberta nao ha a faixa de
// seletores, entao a grade sobe.
#define BIB_GRADE_Y_ABERTA 264.0f

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

static int filtro[CAT_MAX];      // indices do catalogo visiveis
static int nFiltro = 0;
static int totalModo = 0;
static Foco foco;
static float animModo[BIB_N_MODOS];
static float animPick[3];
static float animFoco[BIB_MAX_LINHAS][NV_BIB_COLUNAS];
static float scrollY = 0.0f;
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
static int colunas(void) {
  if (exibicao == VIS_LISTA) return 1;
  return estado() == EST_LISTAS ? BIB_LC_COLS : NV_BIB_COLUNAS;
}
static float passoColuna(void) {
  return estado() == EST_LISTAS ? BIB_LC_PASSO : (NV_BIB_CARD_W + NV_BIB_CARD_GAP);
}
static float alturaLinha(void) {
  if (exibicao == VIS_LISTA) return BIB_LIN_H;
  if (estado() == EST_LISTAS) return BIB_LC_H;
  // poster + gap + titulo (32/500, lh 1.18 -> 37.8)
  return NV_BIB_POSTER_H + NV_BIB_TIT_GAP + 37.8f;
}
static float passoLinha(void) {
  if (exibicao == VIS_LISTA) return BIB_LIN_PASSO;
  if (estado() == EST_LISTAS) return BIB_LC_LINHA;
  return NV_BIB_LINHA_PASSO;
}
static float gradeY(void) {
  return estado() == EST_ITENS ? BIB_GRADE_Y_ABERTA : NV_BIB_GRADE_Y;
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
static void remapear(int preservar) {
  int linhas, cols[FOCUS_MAX_FILEIRAS], r, ini = gradeIni(), nc = colunas();
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
  scrollY = 0.0f;
  memset(animFoco, 0, sizeof animFoco);
}

// Refaz a lista visivel de TITULOS (modos Salvos e Coleção).
static void reconstruir(void) {
  int n = cat_n();
  if (n > CAT_MAX) n = CAT_MAX;
  nFiltro = 0;
  totalModo = 0;
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
    totalModo++;
    if (tipo == TIPO_FILME && ehSerie(ci)) continue;
    if (tipo == TIPO_SERIE && !ehSerie(ci)) continue;
    // `hideUnreleasedContent`: sem ano em `meta` o titulo ainda nao estreou do
    // ponto de vista do catalogo, e a preferencia manda escondê-lo.
    if (ajustes_ocultar_nao_lancados() && !ci->meta[0]) continue;
    filtro[nFiltro++] = i;
  }

  // Ordenacao por insercao — sao poucas dezenas de itens, uma vez por troca.
  if (ordem != ORD_ADICIONADOS) {
    for (int i = 1; i < nFiltro; i++) {
      int v = filtro[i], j = i - 1;
      while (j >= 0) {
        const CatItem *a = cat_item(filtro[j]), *b = cat_item(v);
        int maior;
        if (ordem == ORD_TITULO) maior = a && b && strcmp(a->titulo, b->titulo) > 0;
        else /* ORD_ANO, decrescente */
          maior = a && b && strcmp(a->meta, b->meta) < 0;
        if (!maior) break;
        filtro[j + 1] = filtro[j]; j--;
      }
      filtro[j + 1] = v;
    }
  }
  remapear(0);
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

static void eventoAberta(SDL_Keycode k) {
  if (foco.fileira == 0) {
    if (k == SDLK_RIGHT && acaoSel < 2) { acaoSel++; foco.coluna = acaoSel; return; }
    if (k == SDLK_LEFT  && acaoSel > 0) { acaoSel--; foco.coluna = acaoSel; return; }
    if (k == SDLK_DOWN) { if (nCelulas) focus_mover_grade(&foco, 0, 1); return; }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) executarAcao(acaoSel);
    return;
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    int i = (foco.fileira - 1) * colunas() + foco.coluna;
    if (i >= 0 && i < nCelulas) abrirItemDaLista(i);
    return;
  }
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

  // Barra de modos: esquerda/direita TROCA o modo, e trocar refaz o mapa de
  // foco. Por isso o modo muda AQUI e nao por focus_mover — chamar os dois na
  // ordem errada devolvia o foco para a coluna 0 a cada movimento.
  if (foco.fileira == BIB_FIL_MODO) {
    if ((k == SDLK_RIGHT && modo < BIB_N_MODOS - 1) ||
        (k == SDLK_LEFT  && modo > 0)) {
      modo += (k == SDLK_RIGHT) ? 1 : -1;
      recado[0] = 0;
      if (modo == MODO_LISTAS) pedirFonte(); else reconstruir();
      foco.fileira = BIB_FIL_MODO; foco.coluna = modo;
      return;
    }
    if (k == SDLK_DOWN || k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
      foco.fileira = BIB_FIL_PICK; foco.coluna = pickSel;
    }
    return;
  }

  // Linha de seletores: esquerda/direita anda ENTRE os tres; OK cicla o valor do
  // que esta em foco. O web abre um menu suspenso; num D-pad, ciclar no proprio
  // seletor poupa a viagem de ida e volta ate a lista.
  if (foco.fileira == BIB_FIL_PICK) {
    if (k == SDLK_RIGHT && pickSel < 2) { pickSel++; foco.coluna = pickSel; return; }
    if (k == SDLK_LEFT  && pickSel > 0) { pickSel--; foco.coluna = pickSel; return; }
    if (k == SDLK_UP)   { foco.fileira = BIB_FIL_MODO; foco.coluna = modo; return; }
    if (k == SDLK_DOWN) { if (nCelulas) focus_mover_grade(&foco, 0, 1); return; }
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

  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    int i = (foco.fileira - BIB_FIL_GRADE) * colunas() + foco.coluna;
    if (i < 0 || i >= nCelulas) return;
    if (estado() == EST_LISTAS) abrirLista(i);
    else                        pedido = filtro[i];
    return;
  }
  // A grade da biblioteca e uma GRADE: manter a coluna ao subir e descer, e
  // nao voltar para a coluna onde o cursor esteve por ultimo naquela linha.
  if (k == SDLK_RIGHT)     focus_mover_grade(&foco, 1, 0);
  else if (k == SDLK_LEFT) { if (!focus_mover_grade(&foco, -1, 0)) sair = 1; }
  else if (k == SDLK_DOWN) focus_mover_grade(&foco, 0, 1);
  else if (k == SDLK_UP) {
    if (foco.fileira == BIB_FIL_GRADE) { foco.fileira = BIB_FIL_PICK; foco.coluna = pickSel; }
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

  // A GRADE CRESCE SOZINHA quando a rede entrega mais. Sem remapear, as linhas
  // novas existem no modulo e o foco nao alcanca nenhuma delas.
  if (contarCelulas() != nCelulas) remapear(1);

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
    for (c = 0; c < colunas() && c < NV_BIB_COLUNAS; c++) {
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
  scrollY = anim_mola(scrollY, alvo, dt, NV_MOLA_SCROLL);
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
// Sobre a superficie CLARA do foco a mesma intencao precisa de outro valor:
// 96,98,106 mantem a familia fria e da ~5,9:1 contra branco. Cinza claro ali
// seria o light-on-light de sempre.
static void tintaSecundaria(int principal, int *r, int *g, int *b) {
  int claro = principal > 128;
  *r = claro ? 150 : 96;
  *g = claro ? 153 : 98;
  *b = claro ? 162 : 106;
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
// A TINTA DO TEXTO E CALCULADA, e nao cravada em "escuro". A cor de realce e
// escolha da pessoa (Ajustes): com um realce claro os dois estados escolhidos
// pedem texto escuro, mas com um realce escuro o 60% dele fica escuro demais e
// texto preto ali seria o mesmo defeito de contraste ao contrario. A luminancia
// decide — e ela e a de verdade (Rec.709), nao a media dos canais, porque o
// verde pesa sete vezes o azul para o olho.
static int tintaSobre(float r, float g, float b) {
  float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
  return lum > 0.42f ? BIB_TEXTO_ESCURO : 235;
}

// Quanto do realce sobra no estado "escolhido, mas o foco esta noutro lugar".
// 0.60 medido na captura: abaixo disso ele se aproxima demais do #222 dos nao
// escolhidos a 3 m; acima, fica perto demais do focado.
#define BIB_ESCOLHIDA_DIM 0.60f

static int pilula(GfxRect r, float raio, float f, int escolhida) {
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  if (escolhida) {
    // Uma unica pilula, com a cor interpolando entre o realce a 60% e o realce
    // cheio conforme a mola do foco. Sem duas camadas sobrepostas: a de baixo
    // aparecia pelas bordas do anti-aliasing enquanto a de cima subia.
    float m = anim_clamp(f, 0.0f, 1.0f);
    float k = BIB_ESCOLHIDA_DIM + (1.0f - BIB_ESCOLHIDA_DIM) * m;
    float cr = ar * k, cg = ag * k, cb = ab * k;
    gfx_cor(r, raio, cr, cg, cb, 1.0f);
    return tintaSobre(cr, cg, cb);
  }
  if (f > 0.01f) gfx_cor(r, raio, ar, ag, ab, f);
  gfx_cor(r, raio, 0.133f, 0.133f, 0.133f, 1.0f - f);
  // A troca claro -> escuro e no MEIO da mola: texto ja rasterizado nao muda de
  // cor, e virar no fim deixaria texto claro sobre pilula clara por meio
  // caminho — que e o defeito "light-on-light" ja visto neste app.
  return f > 0.5f ? tintaSobre(ar, ag, ab) : 200;
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

static void desenhaModo(int a, float f) {
  GfxRect r = { NV_BIB_X + a * NV_BIB_MODO_PASSO, NV_BIB_MODO_Y,
                NV_BIB_MODO_W, NV_BIB_MODO_H };
  int sel = (a == modo);
  int cor = pilula(r, NV_RAIO_PILL, f, sel);
  TxtLinha l = txt_linha(TXT_CAPTION2, ROT_MODO[a], cor, cor, cor, 255);
  txt_desenhar_alpha(l, r.x + (r.w - l.w) * 0.5f, r.y + (r.h - l.h) * 0.5f,
                     sel || f > 0.5f ? 1.0f : 0.82f);
}

// Seletor: rotulo pequeno em cinza e valor grande, com a dica encostada na
// direita. Em foco os TRES textos escurecem juntos — deixar so o valor escuro
// sobre a pilula clara e o mesmo erro de contraste de sempre.
static void desenhaPicker(int p, float f) {
  GfxRect r = { NV_BIB_X + p * BIB_PICK_PASSO, NV_BIB_PICK_Y,
                BIB_PICK_W, NV_BIB_PICK_H };
  float raio = raioPx(NV_BIB_PICK_RAIO, BIB_PICK_W, NV_BIB_PICK_H);
  const char *rot, *val, *dica = "OK: alterar";
  int cor = pilula(r, raio, f, 0);
  int escuro = (cor == BIB_TEXTO_ESCURO);
  if (modo == MODO_LISTAS) {
    rot = p == 0 ? "Fonte" : p == 1 ? "Exibição" : "Listas públicas";
    val = p == 0 ? ROT_FONTE[fonte] : p == 1 ? ROT_VIS[exibicao] : "Procurar";
    if (p == 2) dica = "OK: digitar";
  } else {
    rot = p == 0 ? "Tipo" : p == 1 ? "Ordenar" : "Exibição";
    val = p == 0 ? ROT_TIPO[tipo] : p == 1 ? ROT_ORD[ordem] : ROT_VIS[exibicao];
  }
  { int sr, sg, sb;
    tintaSecundaria(cor, &sr, &sg, &sb);
    // TXT_CAPTION2 e nao TXT_MINI: o rotulo do seletor ("Fonte", "Exibição") e
    // PALAVRA PARA LER, e o DESIGN.md define TXT_MINI (15 px) como tamanho de
    // SELO, abaixo do piso de leitura desta base. Era deriva silenciosa.
    TxtLinha tr = txt_linha(TXT_CAPTION2, rot, sr, sg, sb, 255);
    TxtLinha tv = txt_linha_corta(TXT_CALLOUT, val, cor, cor, cor, 255,
                                  BIB_PICK_W - NV_BIB_PICK_PADX * 2.0f - 120.0f);
    TxtLinha td = txt_linha(TXT_CAPTION2, dica, sr, sg, sb, 255);
    float tx = r.x + NV_BIB_PICK_PADX;
    float ty = r.y + NV_BIB_PICK_PADY;
    txt_desenhar_alpha(tr, tx, ty, escuro ? 0.85f : 0.95f);
    txt_desenhar_alpha(tv, tx, ty + tr.h + 4.0f, 1.0f);
    txt_desenhar_alpha(td, r.x + r.w - NV_BIB_PICK_PADX - td.w,
                       r.y + (r.h - td.h) * 0.5f, 0.85f); }
}

// Barra de acoes de uma lista aberta. Tres pilulas largas: fixar, levar para a
// Home e trocar o tipo de midia.
static void desenhaAcoes(void) {
  static const float W[3] = { 520.0f, 520.0f, 300.0f };
  float x = NV_BIB_X;
  int a;
  for (a = 0; a < 3; a++) {
    GfxRect r = { x, NV_BIB_MODO_Y, W[a], 72.0f };
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
    cor = pilula(r, NV_RAIO_PILL, animPick[a], ligada);
    { TxtLinha l = txt_linha(TXT_CAPTION2, rot, cor, cor, cor, 255);
      txt_desenhar_alpha(l, r.x + (r.w - l.w) * 0.5f, r.y + (r.h - l.h) * 0.5f, 1.0f); }
    x += W[a] + 24.0f;
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
    l1 = totalModo ? "Nenhum título neste filtro"
        : modo == MODO_NUVEM ? "Sua coleção aparece aqui" : "Sua próxima sessão começa aqui";
    l2 = totalModo
        ? "Em Tipo, escolha Todos. Confira também os filtros em Ajustes."
        : modo == MODO_NUVEM
          ? "Os filmes e séries da sua coleção no Trakt ficam reunidos nesta aba."
          : "Abra um filme ou série e escolha Adicionar à lista para guardar.";
  }
  { TxtLinha t1 = txt_linha(TXT_TITULO2, l1, 255, 255, 255, 255);
    TxtLinha t2 = txt_linha_corta(TXT_CALLOUT, l2, 150, 153, 162, 255, NV_BIB_W);
    float cx = NV_BIB_X + NV_BIB_W * 0.5f;
    float y = NV_BIB_VAZIO_Y + 190.0f;
    gfx_icone((GfxRect){cx - 32.0f, y - 100.0f, 64.0f, 64.0f},
               "menu_library", 0.70f, 0.70f, 0.72f, 1.0f);
    txt_desenhar_alpha(t1, cx - t1.w * 0.5f, y, 0.96f);
    txt_desenhar_alpha(t2, cx - t2.w * 0.5f, y + t1.h + 18.0f, 0.85f);
    { TxtLinha d = txt_linha(TXT_CAPTION2, dica, 150, 153, 162, 255);
      txt_desenhar(d, cx - d.w * 0.5f, y + t1.h + t2.h + 58.0f); } }
}

// Um TITULO, na exibicao de cartaz. Continua sendo o que a tela sempre
// desenhou: o contorno de foco e por DENTRO do poster e obedece Ajustes.
static void desenhaCartaz(const CatItem *ci, GfxRect base, float f, float a) {
  float esc = 1.0f + NV_BIB_FOCO_ESCALA * f;
  float bw = base.w * esc, bh = base.h * esc;
  GfxRect card = { base.x - (bw - base.w) * 0.5f, base.y, bw, bh };
  // 24 px de canto, como a folha do web manda — e NAO `24/NV_BIB_CARD_W`, que
  // era a conta antiga. Dividir pela LARGURA sobre um cartaz 268x402 pedia
  // 0,0896 da ALTURA, ou seja 36 px: meia vez mais redondo do que o medido, e
  // diferente do canto de toda peca vizinha. Ver raioPx.
  float raio = raioPx(24.0f, base.w, base.h);
  const char *arte = (ci && ci->poster[0]) ? ci->poster : NULL;
  GLuint tex = arte ? tex_obter_larg(arte, NV_BIB_CARD_W) : 0;
  if (tex) {
    gfx_tex_aspect_atual = tex_aspecto(arte);
    gfx_rect(card, tex, GFX_CARD, f, 0.0f, 0.0f, raio, 0, 0, 0, a);
    gfx_tex_aspect_atual = 0.0f;
  } else {
    // Esqueleto VISIVEL, o mesmo da home: #2C2C2C. Placeholder do tom do fundo
    // le como card quebrado, nao como carregando.
    gfx_cor(card, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G,
            NV_COR_ESQUELETO_B, a);
  }
  // A borda de foco do web e de 4px POR DENTRO do poster, e nao um halo por
  // fora. Arte e o unico lugar onde o contorno sobrevive a regra de 16/09: nao
  // da para preencher um cartaz. Ele obedece Ajustes > Foco no cartaz e sai na
  // cor de realce, como nos outros treze pontos do app.
  if (f > 0.01f && ajustes_borda_foco()) {
    float ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    gfx_rect(card, 0, GFX_ANEL, 0, NV_BIB_POSTER_BORDA / card.w,
             0, raio, ar, ag, ab, f * a);
  }
  if (ci) {
    TxtLinha tl = txt_linha_corta(TXT_CALLOUT, ci->titulo, 255, 255, 255, 255,
                                  base.w);
    txt_desenhar_alpha(tl, base.x, base.y + base.h + NV_BIB_TIT_GAP, a * 0.98f);
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

static void linhaMeta(const CatItem *ci, char *dst, size_t n) {
  char ano[8] = "";
  int nGen = 0;
  size_t k = 0;
  const char *p;
  if (!n) return;
  dst[0] = 0;
  if (!ci) return;
  for (p = ci->meta; p[0] && p[1] && p[2] && p[3]; p++)
    if ((p[0] == '1' || p[0] == '2') && p[1] >= '0' && p[1] <= '9' &&
        p[2] >= '0' && p[2] <= '9' && p[3] >= '0' && p[3] <= '9') {
      snprintf(ano, sizeof ano, "%.4s", p); break; }
  if (ano[0]) k += (size_t)snprintf(dst + k, n - k, "%s   ·   ", ano);
  k += (size_t)snprintf(dst + k, n - k, "%s", i18n(ehSerie(ci) ? "Série" : "Filme"));
  // O SEPARADOR E PROCURADO COMO SEQUENCIA DE DOIS BYTES, e nao por strtok.
  //
  // "·" e U+00B7, que em UTF-8 sao os bytes C2 B7. strtok trata o conjunto de
  // delimitadores como BYTES SOLTOS: ele cortaria em C2 OU em B7, e B7 e byte de
  // continuacao de outros caracteres (U+00F7 "÷" e C3 B7). Num genero que
  // trouxesse um deles a linha partiria no meio de um caractere e sairia com
  // lixo. Funcionava por sorte com os generos de hoje; e o tipo de defeito que
  // so aparece com o dado de outra pessoa.
  { const char *seg = ci->genero;
    while (*seg && nGen < 2) {
      const char *fim = seg;
      char item[64];
      size_t m = 0;
      while (*fim && !((unsigned char)fim[0] == 0xC2 && (unsigned char)fim[1] == 0xB7))
        fim++;
      while (seg < fim && *seg == ' ') seg++;
      while (fim > seg && fim[-1] == ' ') fim--;
      for (; seg < fim && m + 1 < sizeof item; seg++) item[m++] = *seg;
      item[m] = 0;
      if (m && !ehPalavraDeTipo(item) && k + m + 10 < n)
        { k += (size_t)snprintf(dst + k, n - k, "   ·   %s", i18n(item)); nGen++; }
      seg = fim;
      while (*seg && !((unsigned char)seg[0] == 0xC2 && (unsigned char)seg[1] == 0xB7)) seg++;
      if (*seg) seg += 2;
    } }
}

// A NOTA, no mesmo selo que a tela de detalhe usa — as medidas sao as dela
// (NV_DETW2_IMDB_*, em detail.h), nao numeros novos. Reimplementar o desenho e
// inevitavel porque `desenhaSeloImdb` e static em detail.c, que nao e meu; o
// que da para garantir e que as MEDIDAS venham do mesmo lugar e andem juntas.
//
// PROCEDENCIA, que o DESIGN.md exige escrita junto do numero: `CatItem.nota` e
// o `imdbRating` do Cinemeta multiplicado por 10 (descoberta.c), e o selo diz
// "IMDb" por isso. 84 e 8,4 — nao 84%.
//
// SEM NOTA NAO DEIXA BURACO E NAO DESLOCA NADA: sai um travessao na MESMA
// posicao do numero. Zero seria mentira (DESIGN.md: ausencia nunca se desenha
// como zero) e espaco em branco quebraria a coluna que o olho esta descendo.
static void desenhaNota(const CatItem *ci, float xDir, float yCentro,
                        int tinta, float a) {
  int sr, sg, sb;
  tintaSecundaria(tinta, &sr, &sg, &sb);
  if (!ci || ci->nota <= 0) {
    // Alfa CHEIO, e nao 0,8: o travessao e o que segura a coluna quando falta
    // nota, e a 0,8 sobre a tinta secundaria ele quase sumia na captura. Ele
    // precisa ser visto para dizer "nao se sabe" — apagado ele vira buraco, que
    // e exatamente o que ele existe para evitar.
    TxtLinha t = txt_linha(TXT_BODY, "–", sr, sg, sb, 255);
    txt_desenhar_alpha(t, xDir - t.w, yCentro - t.h * 0.5f, a);
    return;
  }
  { char txt[8];
    TxtLinha l;
    GfxRect marca;
    // SEPARADOR DECIMAL PELO IDIOMA. Estava VIRGULA cravada nos tres pontos que
    // desenham nota, e em ingles "8,4" nao e um numero com uma casa: le como
    // milhar interrompido. Apareceu ao gerar o album do post em ingles.
    snprintf(txt, sizeof txt,
           ajustes_idioma_ingles() ? "%d.%d" : "%d,%d",
           ci->nota / 10, ci->nota % 10);
    l = txt_linha(TXT_BODY, txt, tinta, tinta, tinta, 255);
    marca.w = NV_DETW2_IMDB_W; marca.h = NV_DETW2_IMDB_H;
    marca.x = xDir - l.w - NV_DETW2_IMDB_GAP - marca.w;
    marca.y = yCentro - marca.h * 0.5f;
    gfx_cor(marca, raioPx(NV_DETW2_IMDB_R, marca.w, marca.h),
            0.965f, 0.780f, 0.0f, a);                       // #f6c700
    { TxtLinha lm = txt_linha(TXT_MINI, "IMDb", 10, 10, 10, 255);
      txt_desenhar_alpha(lm, marca.x + (marca.w - lm.w) * 0.5f,
                         marca.y + (marca.h - lm.h) * 0.5f, a); }
    txt_desenhar_alpha(l, xDir - l.w, yCentro - l.h * 0.5f, a); }
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
// MEMORIA: a miniatura pede tex_obter_larg(60), nao tex_obter — o teto de
// decodificacao sai da largura desenhada, e so a linha VISIVEL chega aqui.
static void desenhaLinhaTitulo(const CatItem *ci, float y, float f, float a) {
  GfxRect r = { NV_BIB_X, y, NV_BIB_W, BIB_LIN_H };
  int cor = pilula(r, raioPx(16.0f, r.w, r.h), f, 0);
  int sr, sg, sb;
  float yc = y + BIB_LIN_H * 0.5f;
  float xNotaDir = r.x + NV_BIB_W - BIB_COL_DIR;
  float xNotaIni = xNotaDir - BIB_COL_NOTA_W - NV_DETW2_IMDB_GAP - NV_DETW2_IMDB_W;
  float xProgDir = xNotaIni - BIB_COL_PROG_GAP;
  GfxRect mini = { r.x + BIB_LIN_PAD, y + (BIB_LIN_H - BIB_LIN_MINI_H) * 0.5f,
                   BIB_LIN_MINI_W, BIB_LIN_MINI_H };
  const char *arte = (ci && ci->poster[0]) ? ci->poster : NULL;
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

  desenhaNota(ci, xNotaDir, yc, cor, a);

  { float tx = mini.x + mini.w + 22.0f;
    float larg = xProgDir - BIB_COL_PROG_W - 28.0f - tx;
    char sub[220];
    TxtLinha t = txt_linha_corta(TXT_CALLOUT, ci->titulo, cor, cor, cor, 255, larg);
    linhaMeta(ci, sub, sizeof sub);
    { TxtLinha sl = txt_linha_corta(TXT_CAPTION2, sub, sr, sg, sb, 255, larg);
      // As duas linhas CENTRADAS no eixo da linha, e nao ancoradas no topo: com
      // a altura em 96 um bloco colado em cima deixava a base oca e devolvia a
      // sensacao de linha grande demais.
      float hBloco = (float)t.h + 6.0f + (float)sl.h;
      float ty = yc - hBloco * 0.5f;
      txt_desenhar_alpha(t, tx, ty, a);
      txt_desenhar_alpha(sl, tx, ty + (float)t.h + 6.0f, a * 0.95f); } }
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
  GfxRect r = { NV_BIB_X, y, NV_BIB_W, BIB_LIN_H };
  int cor = pilula(r, raioPx(16.0f, r.w, r.h), f, 0);
  int sr, sg, sb;
  char sub[220];
  float tx = r.x + 24.0f;
  if (!l) return;
  tintaSecundaria(cor, &sr, &sg, &sb);
  subtituloLista(l, sub, sizeof sub);
  { float w = seloFonte(l, tx, y + (BIB_LIN_H - 26.0f) * 0.5f, 26.0f, cor, a);
    if (w > 0.0f) tx += w + 26.0f; }
  { TxtLinha s2 = txt_linha_corta(TXT_CAPTION2, sub, sr, sg, sb, 255, 340.0f);
    float xDir = r.x + NV_BIB_W - 24.0f;
    TxtLinha t = txt_linha_corta(TXT_CALLOUT, l->titulo, cor, cor, cor, 255,
                                 xDir - (float)s2.w - 60.0f - tx);
    txt_desenhar_alpha(t, tx, y + (BIB_LIN_H - (float)t.h) * 0.5f, a);
    txt_desenhar_alpha(s2, xDir - (float)s2.w,
                       y + (BIB_LIN_H - (float)s2.h) * 0.5f, a * 0.95f);
    if (lst_fixada(l)) {
      TxtLinha fx = txt_linha(TXT_CAPTION2, i18n("FIXADA"), cor, cor, cor, 255);
      txt_desenhar_alpha(fx, xDir - (float)s2.w - (float)fx.w - 32.0f,
                         y + (BIB_LIN_H - (float)fx.h) * 0.5f, a * 0.95f);
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
  { TxtLinha t = txt_linha_corta(TXT_TITULO2, tit,
                                 255, 255, 255, 255, NV_BIB_W - 320.0f);
    txt_desenhar(t, NV_BIB_X, NV_BIB_Y); }

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
    // Branco, que e a cor do PROPRIO arquivo, sobre o fundo escuro da pagina.
    if (!strcmp(f, "TRAKT")) {
      float w = marcaTraktLargura(38.0f);
      if (w > 0.0f) {
        marcaTrakt(NV_BIB_DIR - w, NV_BIB_Y + 4.0f, 38.0f, 255, 0.92f);
        return;
      }
    }
    { float w = txt_tracking(TXT_CALLOUT, f, 128, 128, 128, -1.0f, 0.0f, 0.0f, 4.0f);
      txt_tracking(TXT_CALLOUT, f, 128, 128, 128,
                   NV_BIB_DIR - w, NV_BIB_Y + 10.0f, 0.9f, 4.0f); } }
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
  { TxtLinha info = txt_linha_corta(TXT_CAPTION2, txt, 150, 153, 162, 255, 560.0f);
    txt_desenhar(info, NV_BIB_DIR - info.w,
                 NV_BIB_MODO_Y + (NV_BIB_MODO_H - info.h) * 0.5f); }
}

void biblioteca_desenhar(Uint32 agora) {
  int linhas, r, c, nc = colunas();
  float passoC, passoL, gy;
  // Mesmo ajuste, mesma disciplina da home: o rebordo claro do GFX_CARD e
  // ligado aqui e DEVOLVIDO no fim, porque a variavel e global e as outras
  // telas desenham card tambem.
  gfx_borda_foco_atual = ajustes_borda_foco() ? 1.0f : 0.0f;
  (void)agora;
  // A tela ja foi limpa com a cor de fundo por glClearColor/glClear em main.c
  // antes de app_desenhar. Pintar por cima era uma camada de tela cheia jogada
  // fora por quadro — e o custo dominante nesta GPU e fill rate (gfx.c registra
  // que DUAS camadas de tela cheia derrubavam a Mali-G71 para ~40fps).

  desenhaCabecalho();
  if (temAberta) desenhaAcoes();
  else {
    for (int a = 0; a < BIB_N_MODOS; a++) desenhaModo(a, animModo[a]);
    for (int p = 0; p < 3; p++)           desenhaPicker(p, animPick[p]);
  }
  desenhaResumo();

  if (nCelulas == 0) {
    desenhaVazio();
    if (teclado_aberto()) teclado_desenhar(agora);
    gfx_borda_foco_atual = 1.0f;
    return;
  }

  linhas = nLinhas();
  if (linhas > BIB_MAX_LINHAS) linhas = BIB_MAX_LINHAS;
  passoC = passoColuna(); passoL = passoLinha(); gy = gradeY();

  // Dois passes: o item focado escala 2% e precisa ser desenhado por ULTIMO,
  // senao o vizinho da direita corta a borda dele.
  for (int passe = 0; passe < 2; passe++)
    for (r = 0; r < linhas; r++) {
      float topo = gy + r * passoL - scrollY;
      float a;
      if (topo > NV_TELA_H || topo + alturaLinha() < -80.0f) continue;
      // O que sobe para baixo do cabecalho some antes de cruza-lo: sem o
      // esmaecimento, cartaz e seletor se leem um sobre o outro.
      //
      // O LIMIAR E `gy - BIB_FADE`, e isso importa: com qualquer outro valor a
      // PRIMEIRA linha ja nasce esmaecida com a tela parada no topo. Foi
      // exatamente o que a captura mostrou — a linha do CODA cinza enquanto as
      // de baixo estavam brancas, sem nenhuma rolagem acontecendo.
      a = anim_clamp((topo - (gy - BIB_FADE)) / BIB_FADE, 0.0f, 1.0f);
      if (a <= 0.005f) continue;

      for (c = 0; c < nc; c++) {
        int i = r * nc + c;
        float f;
        if (i >= nCelulas) break;
        f = (c < NV_BIB_COLUNAS) ? animFoco[r][c] : 0.0f;
        if ((passe == 0) == (f > 0.01f)) continue;

        if (estado() == EST_LISTAS) {
          const LstLista *l = lst_lista(i);
          if (exibicao == VIS_LISTA) desenhaLinhaLista(l, topo, f, a);
          else desenhaCartaoLista(l, (GfxRect){ NV_BIB_X + c * passoC, topo,
                                                BIB_LC_W, BIB_LC_H }, f, a);
          continue;
        }
        { CatItem tmp;
          const CatItem *ci;
          if (estado() == EST_ITENS) ci = lst_item(i, &tmp) ? &tmp : NULL;
          else                       ci = cat_item(filtro[i]);
          if (exibicao == VIS_LISTA) desenhaLinhaTitulo(ci, topo, f, a);
          else desenhaCartaz(ci, (GfxRect){ NV_BIB_X + c * passoC, topo,
                                            NV_BIB_CARD_W, NV_BIB_POSTER_H }, f, a); }
      }
    }
  if (teclado_aberto()) teclado_desenhar(agora);
  gfx_borda_foco_atual = 1.0f;
}
