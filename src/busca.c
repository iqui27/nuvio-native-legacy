// Busca, alinhada com a tela do app web (MEDIDA rodando, perfil do dono).
//
// ------------------------------------------------------------------------
// O QUE MUDOU, E POR QUE
//
// O port tinha um teclado em GRADE a esquerda e uma GRADE de posteres 4x a
// direita. Medida a tela do web, nenhuma das duas coisas confere:
//
//   1. O web nao tem grade de resultados. Tem FILEIRAS horizontais, uma por
//      catalogo de addon, com o nome do catalogo em 48/600 e a origem
//      ("from Xperience") em 20/400 logo abaixo. Card de 248 de largura, poster
//      248x372, nome 28/500 e ano 20/400 embaixo; passo 280 entre cards e 562.4
//      entre fileiras.
//   2. O web nao tem teclado nenhum: tem um <input> largo no topo, e quem
//      levanta o teclado e o SISTEMA da TV.
//
// A (1) foi portada inteira. A (2) NAO da para portar: este app e SDL puro e
// nao existe IME para chamar — sem teclado na tela nao ha como digitar, e uma
// busca em que nao se digita nao e uma busca. O teclado ficou, agora ABAIXO do
// cabecalho e a esquerda, ocupando a faixa onde o web desenha o estado vazio; as
// fileiras de resultado correm a direita dele. E a unica divergencia deliberada
// desta tela, e esta anotada aqui para nao ser confundida com descuido.
//
// DECISAO DE PROJETO — o teclado e em GRADE, nao a linha unica do tvOS.
// A faixa horizontal do tvOS e bonita e cabe em pouca altura, mas custa caro no
// D-pad: sao 38 teclas em UMA dimensao, entao a distancia media entre duas
// letras e ~13 toques e o pior caso passa de 37. A grade 6x7 poe a mesma tecla a
// no maximo 5+6 toques e ~5 em media.
#include "menu.h"
#include "busca.h"
#include "posterprov.h"
#include "idioma.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "focus.h"
#include "teclado.h"
#include "anim.h"
#include "revela.h"
#include "layout.h"
#define NV_TXT_TELA
#include "escala.h"
#include "segurar.h"
#include "ajustes.h"
#include "catalogo.h"
#include "descoberta.h"
#include "buscasrec.h"
#include "buscanorm.h"
#include "botoes.h"
#include "sistexto.h"
#include "ponteiro.h"
#include "celbotao.h"
#include "spotpessoa.h"
#include "spotlight.h"
#include "ctxlista.h"
#include <string.h>
#include <stdio.h>

// --- GLASS UI "ILHA" (mockup aprovado, secao 10 de design/glass-ilha) ---------
// Campo e teclado sao DUAS ILHAS na coluna da esquerda (520 de largura); a
// direita ficam o melhor resultado, os titulos e as buscas recentes. Margem 96
// (a .pg do mockup), rail fixa entra antes. Sem contorno: foco = superficie
// mais clara (linhas, cartoes) ou pilula no acento (tecla, chip).
//   campo   520 x 76, raio 38, padding 28, lupa 28, texto 28/500
//   teclado ilha 520, raio 32, padding 20, teclas 62 (raio 18) com vao 12
//   dica    16, a 16 abaixo do teclado
// Descobrir nao aparece: nao ha acao de descoberta nesta tela nativa. ONDE HA
// TECLADO E VOZ DO SISTEMA (Android, sistexto.h) o campo vira alvo de foco —
// CIMA da primeira fileira do teclado — e OK nele chama o teclado da TV; o
// microfone (Falar) e o celular entram dentro do campo, no fim.
#define BU_MARG        96.0f
#define BU_COL_W      buColW()
#define BU_COL_GAP     56.0f
#define BU_CAMPO_H     buCampoH()
#define BU_ILHA_GAP    22.0f
#define BU_KB_PAD      20.0f
#define BU_CAMPO_PADX  28.0f
static int buRetrato(void) {
#ifdef NV_TOUCH_UI
  return layout_modo_mobile() && NV_TELA_H > NV_TELA_W;
#else
  return 0;
#endif
}
static int buTelefone(void) {
#ifdef NV_TOUCH_UI
  return layout_modo_mobile();
#else
  return 0;
#endif
}
static int buRetratoTelefone(void) { return buRetrato() && buTelefone(); }
static int buTecladoNativo(void) { return buTelefone() && st_ime_disponivel(); }
static int buUmaColuna(void) { return buRetrato() || buTecladoNativo(); }
static float buCampoH(void) { return buTecladoNativo() ? 104.0f : 76.0f; }
static float buMargem(void) { return buTelefone() ? 48.0f : ANC(BU_MARG); }
static float buX(void)   { return ANC(ajustes_rail_largura_fixa()) + buMargem(); }
static float buDir(void) { return NV_TELA_W - (buRetratoTelefone() ? buX() : buMargem()); }
static float buColW(void) { return buUmaColuna() ? buDir() - buX() : fminf(520.0f, ANC(520.0f)); }
static float buResY(void);
// A coluna da esquerda desce quando a pilula da Dinamica ocupa o canto. A
// pilula vem em px REAIS e esta tela mede pela virtual (real / escala de
// texto): sem a conversao o campo colava na pilula com texto menor que 100%.
#define BU_TOPO_VAO 28.0f
static float buTopoEsq(void) {
  float px, py, pw, ph;
  if (menu_pilula_rect(&px, &py, &pw, &ph)) return ANC(fmaxf(64.0f, py + ph + BU_TOPO_VAO));
  return ANC(64.0f);
}

// --- Teclado -----------------------------------------------------------------
#define BU_TECLA_W     66.0f
#define BU_TECLA_GAP   10.0f
#define BU_KB_COLS      6
// FILEIRAS DE TECLAS DE LETRA, no maximo (o layout latino usa 6; o cirilico, 7)
// mais a de baixo (espaco/apagar/limpar/layout). O numero vivo e kbFil.
#define BU_KB_MAX_FIL   8
#define BU_KB_PASSO   (BU_TECLA_W + BU_TECLA_GAP)
#define BU_KB_W       (BU_KB_COLS * BU_TECLA_W + (BU_KB_COLS - 1) * BU_TECLA_GAP)
#define BU_KB_X       buX()
#define BU_KB_Y       (buTopoEsq() + BU_CAMPO_H + BU_ILHA_GAP)   // topo da ilha do teclado
#define BU_TECLA_RAIO   20.0f
#define BU_MAX_CONSULTA 48

// --- Coluna da direita --------------------------------------------------------
#define BU_RES_X       (buUmaColuna() ? buX() : buX() + ANC(520.0f + BU_COL_GAP))
#define BU_RES_Y       buResY()
#define BU_DIR         buDir()
#define BU_RES_AREA_H  (NV_TELA_H - 48.0f - BU_RES_Y)
// 32 fixas, e nao FOCUS_MAX_FILEIRAS: aquele teto subiu para a grade da
// Biblioteca caber inteira, e a busca nao precisa de mais fileiras por isso.
#define BU_MAX_FILEIRAS 32
#define BU_MAX_POR_FIL  DESC_BUSCA_POR_ALVO
// Rotulo em caixa alta (15/700) 18 acima do conteudo; melhor resultado 226 de
// altura (capa 330x186 + padding 20); cartaz 180x270, nome 17 a 10 abaixo.
#define BU_KICK_H       34.0f
#define BU_MELHOR_H    fmaxf(226.0f, 186.0f * gfx_poster_k() + 40.0f)
#define BU_CARTAZ_W    (180.0f * gfx_poster_k())
#define BU_CARTAZ_H    (270.0f * gfx_poster_k())
#define BU_NOME_ALT     (buTecladoNativo() ? 50.0f : 30.0f)
#define BU_SECAO_GAP    34.0f

// --- Buscas recentes (campo vazio; regras em buscasrec.h) ---------------------
// Chips de 56 (o .chip do mockup) a partir de BU_RES_Y + 34, em LINHAS que
// quebram na borda direita, porque dez termos de tamanho livre nao cabem numa
// fileira rolavel sem esconder o "Limpar" no fim.
#define BU_REC_Y       (BU_RES_Y + BU_KICK_H)
#define BU_REC_H       56.0f
#define BU_REC_PADX    22.0f
#define BU_REC_GAP     14.0f
#define BU_REC_LINHA   (BU_REC_H + 16.0f)
#define BU_REC_ITENS   (BUSCASREC_MAX + 1)   // termos + "Limpar"

// --- Estado ------------------------------------------------------------------
static Foco  focoKb;
static Foco  focoRes;
// 0 = teclado, 1 = resultados, 2 = buscas recentes (so com o campo vazio).
static int   painel = 0;
static char  consulta[BU_MAX_CONSULTA];
static int   nConsulta = 0;
static char consultaFiltrada[BU_MAX_CONSULTA];
// Resultados agrupados por CATALOGO, como no web: uma fileira por catalogo que
// teve pelo menos um titulo casando. Guardamos indices do catalogo global.
static struct {
  const char *titulo;      // nome do catalogo ("Top 100 Today - Filme")
  const char *origem;      // "from <addon>"; vazio quando nao se sabe
  int itens[BU_MAX_POR_FIL];
  int n;
  int melhor;              // 1 = a entrada do MELHOR RESULTADO (1 item, tile grande)
  int pessoas;             // 1 = fileira de PESSOAS: itens[] indexa pess[], nao o catalogo
} fil[BU_MAX_FILEIRAS];
// PESSOAS: atores e diretores que casam com a consulta. O elenco dos titulos ja
// carregados entra primeiro; o TMDB (spotpessoa.h, a mesma busca do Spotlight)
// completa quando a resposta chega. Abrir uma pessoa devolve o MESMO pedido que
// o Spotlight devolve (SpotPedido), e o app abre a filmografia pelo mesmo caminho.
#define BU_MAX_PESS 8
static struct {
  long tmdb, tituloTmdb;
  char tituloTipo[8];
  int  ref2;               // indice de catalogo de onde veio do elenco; -1 = TMDB
  char nome[96];
  char foto[200];
} pess[BU_MAX_PESS];
static int nPess = 0;
static unsigned gerPessoa = 0;
// 1 = campo vazio (ou 1 letra): fil[] guarda SUGESTOES (Populares), nao
// resultados. Os dois usam a mesma maquina de foco, rolagem e abertura.
static int sugestao = 0;
// Altura que o bloco de buscas recentes ocupa acima das sugestoes (preenchida
// pelo desenho, lida pela rolagem).
static float sugDesloc = 0.0f;
static int   pedidoPessoa = 0;
static SpotPedido pedidoP;
static int nFil = 0;
static int sair = 0;
static int pediuRelogio;
static int pedido = -1;             // indice de catalogo escolhido, -1 = nenhum
static float animTecla[BU_KB_MAX_FIL + 1][BU_KB_COLS];
static float animRes[BU_MAX_FILEIRAS][BU_MAX_POR_FIL];
// Arte chegando e luz do foco, as mesmas da home (revela.h).
static RevelaArte  revRes[BU_MAX_FILEIRAS][BU_MAX_POR_FIL];
static RevelaVarre revVarre = { -1, 0, 0 };
// ONDA das fileiras de resultado (revela.h). Uma fileira entra UMA vez, na
// primeira vez que aparece para a consulta: a identidade e titulo + origem,
// entao a letra digitada que so refina uma fileira que ja estava na tela nao a
// faz entrar de novo — so a fileira nova (um addon que respondeu agora) entra.
static char   filChave[BU_MAX_FILEIRAS][96];
static int    filNova[BU_MAX_FILEIRAS];
static Uint32 filEntraEm[BU_MAX_FILEIRAS];
static float scrollY = 0.0f, scrollAlvo = 0.0f;
static float scrollX[BU_MAX_FILEIRAS];
#ifdef NV_TOUCH_UI
static int toqueLivreY, toqueLivreX[BU_MAX_FILEIRAS], toqueFileira = -1;
static void toqueBuscaLimpar(void) {
  toqueLivreY = 0; toqueFileira = -1;
  memset(toqueLivreX, 0, sizeof toqueLivreX);
}
static void toqueBuscaRetomarFoco(void);
#endif
// Velocidade da mola de 2a ordem da rolagem (anim_mola2): partida macia e
// cauda exponencial, a MESMA curva que a home mede. A de 1a ordem que estava
// aqui partia na velocidade maxima e o primeiro quadro ja saltava 12%.
static float velY = 0.0f, velX[BU_MAX_FILEIRAS];
static float animCampo = 0.0f;
static HomeItem itemFoco;
static int   temItemFoco = 0;
// SEGURAR OK NUM RESULTADO = o menu do cartaz (ctxlista.h). So titulo: a
// fileira de pessoas abre a filmografia, que nao tem menu.
static CtxHold holdRes;

// Buscas recentes. focoRec vai de 0 a n (n = o "Limpar"). recRect/recLin sao
// preenchidos pelo DESENHO (a largura de cada pilula depende do texto
// rasterizado) e lidos pela navegacao: cima/baixo procuram a pilula de x mais
// proximo na linha vizinha, e isso so se sabe com a geometria real.
static int     focoRec = 0;
static float   animRec[BU_REC_ITENS];
static GfxRect recRect[BU_REC_ITENS];
static int     recLin[BU_REC_ITENS];
static int     nRecLayout = 0;
// Pressao longa na pilula: o tempo corre do KEYDOWN e a remocao DISPARA em
// busca_atualizar ao cruzar NV_HOLD_MS, com o dedo ainda no botao — como no
// guia (guia.c). Esperar o KEYUP deixaria o dono segurando sem saber se ja
// pode soltar. okLongo faz o KEYUP daquela pressao nao virar tambem um OK
// curto.
static int     okPress = 0, okLongo = 0;
static Uint32  okDesde = 0;
// Foco na barra: 0 = teclado, 1 = campo, 2 = Falar (estes dois so com sistexto),
// 3 = Digitar pelo celular (celbotao.h, onde ha servidor).
static int     campoFoco = 0;
static float   animFocoCampo = 0.0f, animMic = 0.0f;

// Minusculas como no aparelho: o campo mostra o que foi digitado, e uma consulta
// em caixa alta le como grito. A comparacao ignora caixa de qualquer forma.
//
// O ALFABETO MORA EM teclado.c desde que a modal de digitacao existe. Ele
// estava escrito aqui, e este arquivo era a referencia que o servidor de
// recomendacoes cita para dizer que o codigo de pareamento e `a-z0-9`
// (servidor/recomendacoes/src/index.js) — com duas copias, a segunda a ganhar
// uma letra deixaria um codigo indigitavel numa das duas telas.

// LAYOUTS DO TECLADO DA TELA. O latino (a-z0-9) e o de sempre; o cirilico
// (russo + ucraniano) existe porque a pessoa que tem um addon de metadados em
// russo ou ucraniano e um teclado so de a-z nao tinha como escrever o nome que
// o addon conhece (#176: "para achar um filme romeno preciso do nome em
// ingles"). A tecla de layout so aparece quando o idioma dos metadados e
// cirilico. As teclas moram em kbTeclas como UTF-8, uma por casa.
#define BU_KB_MAX_TECLAS (BU_KB_MAX_FIL * BU_KB_COLS)
static const char LAYOUT_CIRILICO[] =
  "\xd0\xb0" "\xd0\xb1" "\xd0\xb2" "\xd0\xb3" "\xd2\x91" "\xd0\xb4"   /* а б в г ґ д */
  "\xd0\xb5" "\xd1\x94" "\xd0\xb6" "\xd0\xb7" "\xd0\xb8" "\xd1\x96"   /* е є ж з и і */
  "\xd1\x97" "\xd0\xb9" "\xd0\xba" "\xd0\xbb" "\xd0\xbc" "\xd0\xbd"   /* ї й к л м н */
  "\xd0\xbe" "\xd0\xbf" "\xd1\x80" "\xd1\x81" "\xd1\x82" "\xd1\x83"   /* о п р с т у */
  "\xd1\x84" "\xd1\x85" "\xd1\x86" "\xd1\x87" "\xd1\x88" "\xd1\x89"   /* ф х ц ч ш щ */
  "\xd1\x8c" "\xd1\x8e" "\xd1\x8f" "\xd1\x8b" "\xd1\x8d" "\xd1\x8a"   /* ь ю я ы э ъ */
  "\xd1\x91";                                                              /* ё */
static int  layoutCir = 0;                 // 0 latino, 1 cirilico
// Pagina de acentos e sinais (tecla ?!#): os acentos que o catalogo usa em
// titulos ("Amélie") e a pontuacao de "WALL·E" e "Spider-Man: ...". Digitar
// o acento entra em UTF-8; buscanorm dobra para a base na hora de casar.
static const char SIMBOLOS[] =
  "\xc3\xa1\xc3\xa0\xc3\xa2\xc3\xa3\xc3\xa9\xc3\xaa\xc3\xad\xc3\xb3\xc3\xb4\xc3\xb5\xc3\xba\xc3\xa7\xc3\xb1\xc3\xbc"
  ".,:;'\"!?-_&()/@#+=*\xc2\xb7";
static char kbTeclas[BU_KB_MAX_TECLAS][5];
static int  kbN;                           // teclas de letra do layout ativo
static int  kbFil = 6;                     // fileiras de letra do layout ativo
static int  kbTemLayout;                   // a tecla de layout aparece?
static int  pagSim;                        // 1 = pagina de acentos e sinais (?!#)
static int  KB_COLUNAS[BU_KB_MAX_FIL + 1] = { 6, 6, 6, 6, 6, 6, 3 };
static float buResY(void) {
  if (buTecladoNativo()) return buTopoEsq() + BU_CAMPO_H + 44.0f;
  if (!buRetrato()) return ANC(64.0f);
  return BU_KB_Y + 2.0f * BU_KB_PAD + (kbFil + 1) * BU_TECLA_W
       + kbFil * BU_TECLA_GAP + 92.0f;
}

// O idioma dos metadados usa cirilico? (ru, uk, be, bg, sr, mk)
static int idiomaCirilico(void) {
  const char *l = desc_tmdb_idioma();
  return l && (!strncmp(l, "ru", 2) || !strncmp(l, "uk", 2) || !strncmp(l, "be", 2) ||
               !strncmp(l, "bg", 2) || !strncmp(l, "sr", 2) || !strncmp(l, "mk", 2));
}

// Separa `texto` (UTF-8) em teclas de uma casa cada.
static void kbCarregar(const char *texto) {
  const unsigned char *p = (const unsigned char *)texto;
  kbN = 0;
  while (*p && kbN < BU_KB_MAX_TECLAS) {
    int len = *p < 0x80 ? 1 : (*p >= 0xF0 ? 4 : (*p >= 0xE0 ? 3 : 2)), i;
    for (i = 0; i < len; i++) kbTeclas[kbN][i] = (char)p[i];
    kbTeclas[kbN][len] = 0;
    kbN++;
    p += len;
  }
}

// Monta teclas e colunas do layout ativo. Sai da tecla de layout quando ela
// deixa de existir (o idioma dos metadados mudou fora da tela).
static void kbMontar(void) {
  int r, resto;
  kbTemLayout = idiomaCirilico();
  if (!kbTemLayout) layoutCir = 0;
  kbCarregar(pagSim ? SIMBOLOS : layoutCir ? LAYOUT_CIRILICO : teclado_alfabeto());
  kbFil = (kbN + BU_KB_COLS - 1) / BU_KB_COLS;
  resto = kbN - (kbFil - 1) * BU_KB_COLS;
  for (r = 0; r < kbFil; r++) KB_COLUNAS[r] = (r == kbFil - 1) ? resto : BU_KB_COLS;
  KB_COLUNAS[kbFil] = kbTemLayout && !pagSim ? 5 : 4;
}

// --- Normalizacao -----------------------------------------------------------
// Vive em buscanorm.c (testada sozinha). Aqui so o nome curto.
#define normalizar busca_normalizar

// --- Pessoas e sugestoes -------------------------------------------------------
// O nome casa quando a consulta e o inicio do nome ou de alguma palavra dele
// (nome ou sobrenome), como o Spotlight. Os dois ja vem normalizados.
static int nomeCasa(const char *nome, const char *alvo) {
  const char *p;
  size_t n = strlen(alvo);
  if (!n) return 0;
  if (!strncmp(nome, alvo, n)) return 1;
  for (p = nome; (p = strstr(p, alvo)) != NULL; p++)
    if (p > nome && (p[-1] == ' ' || p[-1] == '-' || p[-1] == ':')) return 1;
  return 0;
}

static int pessoaJa(long tmdb) {
  for (int i = 0; i < nPess; i++) if (pess[i].tmdb == tmdb) return 1;
  return 0;
}

static void pessoaAcrescentar(long tmdb, const char *nome, const char *foto, int ref2,
                              long tituloTmdb, const char *tituloTipo) {
  if (nPess >= BU_MAX_PESS) return;
  pess[nPess].tmdb = tmdb;
  pess[nPess].tituloTmdb = tituloTmdb;
  snprintf(pess[nPess].tituloTipo, sizeof pess[nPess].tituloTipo, "%s", tituloTipo ? tituloTipo : "");
  pess[nPess].ref2 = ref2;
  snprintf(pess[nPess].nome, sizeof pess[nPess].nome, "%s", nome);
  snprintf(pess[nPess].foto, sizeof pess[nPess].foto, "%s", foto ? foto : "");
  nPess++;
}

// Quem casa: primeiro o elenco dos titulos que ja estao no catalogo (abre na
// hora), depois o TMDB na ordem de popularidade dele (spotpessoa.h). Sem
// resposta ainda (debounce, rede) ficam so as do elenco, e a fileira se refaz
// quando a resposta chega (busca_atualizar vigia spotpessoa_geracao).
static void montarPessoas(const char *alvo) {
  int n = cat_n(), i, j, deElenco = 0;
  char nome[160];
  nPess = 0;
  for (i = 0; i < n && deElenco < 4; i++) {
    const CatItem *ci = cat_item(i);
    if (!ci || ci->nElenco <= 0) continue;
    for (j = 0; j < ci->nElenco && deElenco < 4; j++) {
      if (ci->elenco[j].tmdb <= 0 || !ci->elenco[j].nome[0] || pessoaJa(ci->elenco[j].tmdb)) continue;
      busca_normalizar(ci->elenco[j].nome, nome, sizeof nome);
      if (!nomeCasa(nome, alvo)) continue;
      pessoaAcrescentar(ci->elenco[j].tmdb, ci->elenco[j].nome, ci->elenco[j].foto, i, 0, NULL);
      deElenco++;
    }
  }
  { int nt = spotpessoa_n(consulta);
    for (i = 0; i < nt && nPess < SPP_MAX && nPess < BU_MAX_PESS; i++) {
      SpotPessoa sp;
      if (!spotpessoa_item(consulta, i, &sp)) break;
      if (pessoaJa(sp.tmdb)) continue;
      pessoaAcrescentar(sp.tmdb, sp.nome, sp.foto, -1, sp.tituloTmdb, sp.tituloTipo);
    } }
}

// CAMPO VAZIO: nada de vazio no meio da tela. Os POPULARES sao as primeiras
// fileiras de filme/serie que o catalogo ja tem (as mesmas da home), com a
// mesma maquina dos resultados — foco, rolagem, abrir. A primeira leva o rotulo
// "Populares"; a segunda, o nome do proprio catalogo.
static void montarSugestoes(void) {
  int nCat = cat_n_fileiras(), r;
  for (r = 0; r < nCat && nFil < 2; r++) {
    const CatFileira *cf = cat_fileira(r);
    int achou = 0, i;
    if (!cf || !cf->titulo[0] || !strcmp(cf->tipo, "channel") || !strcmp(cf->tipo, "tv") ||
        !strcmp(cf->tipo, "channels") || !strcmp(cf->tipo, "live") || !strcmp(cf->tipo, "iptv")) continue;
    for (i = 0; i < cf->n && achou < BU_MAX_POR_FIL; i++) {
      const CatItem *ci = cat_item(cf->ini + i);
      if (!ci || !ci->titulo[0] || (!ci->poster[0] && !ci->backdrop[0])) continue;
      if (!strcmp(ci->tipo, "channel")) continue;
      fil[nFil].itens[achou++] = cf->ini + i;
    }
    if (achou < 3) continue;
    fil[nFil].titulo = nFil == 0 ? i18n("Populares") : cf->titulo;
    fil[nFil].origem = NULL;
    fil[nFil].n = achou;
    fil[nFil].melhor = 0; fil[nFil].pessoas = 0;
    nFil++;
  }
}

// --- Filtro ------------------------------------------------------------------
// Uma fileira por CATALOGO, exatamente como o web monta `.search-results-row`.
// Antes isto era uma lista plana do acervo inteiro, o que perdia a informacao de
// ONDE cada resultado foi achado — e e essa informacao que o subtitulo "from
// <addon>" mostra.
// DEBOUNCE DA REDE (#368). Cada tecla chamava desc_buscar: geracao nova,
// contagens zeradas, TODOS os alvos (ate 16 + Nuvio) refeitos em 3 fios. Em TV
// antiga isso engasgava a digitacao. Agora refiltrar so ANOTA o termo; o pedido
// sai quando o texto assenta (BU_ESPERA_REDE_MS). O filtro LOCAL segue imediato.
#define BU_ESPERA_REDE_MS 300
#define BU_COALESCE_MS    150
static int    redeImediata;        // 1 = esta chamada dispara a rede sem esperar
static int    redePend;            // ha termo anotado e ainda nao disparado
static Uint32 redePendDesde;
static char   redePendTermo[BU_MAX_CONSULTA];
static char   redeDisparado[BU_MAX_CONSULTA];   // ultimo termo entregue a desc_buscar
// Fileiras de add-on do termo ANTERIOR, mantidas ate a 1a resposta do novo.
static struct { const char *titulo, *origem; int itens[BU_MAX_POR_FIL]; int n; } velho[BU_MAX_FILEIRAS];
static int    nVelho;
static Uint32 ultimoRefiltro;

static void refiltrar(void);
static void refiltrarJa(void) { redeImediata = 1; refiltrar(); redeImediata = 0; }

static void redeDisparar(void) {
  redePend = 0;
  snprintf(redeDisparado, sizeof redeDisparado, "%s", consulta);
  desc_buscar(consulta);
}

static void refiltrar(void) {
  char alvo[BU_MAX_CONSULTA * 2];
  int anterior = -1, mesmaConsulta = !strcmp(consultaFiltrada, consulta);
  if (mesmaConsulta && painel == 1 && focoRes.fileira < nFil && !fil[focoRes.fileira].pessoas &&
      focoRes.coluna < fil[focoRes.fileira].n)
    anterior = fil[focoRes.fileira].itens[focoRes.coluna];
  snprintf(consultaFiltrada, sizeof consultaFiltrada, "%s", consulta);
  normalizar(consulta, alvo, sizeof alvo);
  nFil = 0; nPess = 0;
  // Menos de 2 caracteres = estado vazio, como o web ("Digite ao menos 2
  // caracteres"). Buscar com uma letra devolve o acervo inteiro e nao ajuda.
  // Em vez do vazio, as SUGESTOES (Populares). So derruba o painel de
  // RESULTADOS: o de buscas recentes vive justamente com o campo vazio.
  if (busca_codepoints(alvo) < 2) {
    if (painel == 1 && !sugestao) painel = 0;
    sugestao = 1;
    redePend = 0; nVelho = 0; redeDisparado[0] = 0;
    montarSugestoes();
    goto montado;
  }
  sugestao = 0;
  spotpessoa_pedir(consulta, SDL_GetTicks());
  gerPessoa = spotpessoa_geracao();

  // BUSCA NA REDE. A tela so filtrava o que ja estava em memoria — as ~12
  // primeiras linhas de cada catalogo da home — entao qualquer titulo fora
  // disso simplesmente nao existia para a busca. Dispara e volta na hora; o
  // resultado aparece sozinho quando chegar, porque refiltrar roda a cada
  // tecla e desc_busca_n so responde para o termo corrente.
  // O TERMO QUE VAI AOS ADDONS E O QUE A PESSOA ESCREVEU (#176), nao o dobrado.
  // `alvo` e a forma sem acento e sem caixa, boa para o filtro LOCAL; mandada a
  // um addon localizado ela perde justamente o que ele indexa ("Ștefan",
  // "Друзья"). O addon faz o seu proprio casamento.
  if (!strcmp(consulta, redeDisparado)) { redePend = 0; desc_buscar(consulta); }   // mesmo termo: so confere o interruptor
  else if (redeImediata) redeDisparar();
  else {
    // O relogio so recomeca quando o TEXTO muda; uma resposta que refaz a tela
    // com o mesmo texto nao pode adiar o pedido.
    if (!redePend || strcmp(consulta, redePendTermo)) {
      redePendDesde = SDL_GetTicks();
      snprintf(redePendTermo, sizeof redePendTermo, "%s", consulta);
    }
    redePend = 1;
  }

  // UMA FILEIRA POR CATALOGO CONSULTADO, com a origem embaixo — igual ao web,
  // que monta uma `.search-results-row` por catalogo em vez de uma lista unica.
  //
  // Antes so o Cinemeta era consultado e tudo caia numa fileira "Resultados da
  // busca". Com dez alvos numa lista so o dono nao tinha como saber de onde
  // veio nada, e os resultados do addon lento pareciam nunca chegar (chegavam;
  // ficavam no fim de uma fileira de 12 que ja estava cheia de Cinemeta).
  //
  // Os itens entram no catalogo global via cat_acrescentar_lote porque a tela
  // abre titulo por INDICE de catalogo — um resultado que vivesse so aqui nao
  // seria abrivel.
  // #311: o catalogo do Nuvio pode ir para o FIM dos alvos (passo 1) ou nem ser
  // consultado (desc_busca_*); o Continuar assistindo, que vem das fileiras
  // locais abaixo, nao muda de lugar. A "fonte" sob o grupo some com o ajuste
  // ou quando so ha uma fonte de busca ativa.
  // UM cat_acrescentar_lote POR refiltrar (#368): cada chamada copia o catalogo
  // inteiro (CatItem = 16 KB) sob trava, no fio de desenho. Antes era um por
  // alvo com titulo novo. Fase A monta as fileiras e junta os novos de TODOS os
  // alvos (marcador -(k+2) em itens[]); fase B acrescenta de uma vez e resolve.
  { int alvoIdx, passo, nAlvos = desc_busca_n_alvos();
    // So os alvos remotos acrescentam itens neste lote. As fileiras locais
    // usam indices existentes: nao reservar 32 fileiras de CatItem para elas.
    int capNovos = (nAlvos < BU_MAX_FILEIRAS ? nAlvos : BU_MAX_FILEIRAS) * BU_MAX_POR_FIL;
    int nuvioDepois = ajustes_busca_nuvio() == 1;
    int mostraFonte = ajustes_busca_origem() && desc_busca_n_fontes() > 1;
    int chegou = desc_busca_chegou(consulta);
    int fil0 = nFil, nNovos = 0, k;
    CatItem *novos = NULL;
    int *idxNovos = NULL;
    for (passo = 0; passo < (nuvioDepois ? 2 : 1); passo++)
     for (alvoIdx = 0; alvoIdx < nAlvos && nFil < BU_MAX_FILEIRAS; alvoIdx++) {
      int nRem = desc_busca_alvo_n(alvoIdx, consulta), i, achou = 0;
      if (nRem <= 0) continue;
      if (nuvioDepois && (desc_busca_alvo_nuvio(alvoIdx) != 0) != (passo == 1)) continue;
      for (i = 0; i < nRem && achou < BU_MAX_POR_FIL; i++) {
        CatItem it;
        int idx;
        if (!desc_busca_alvo_item(alvoIdx, i, &it)) continue;
        idx = it.imdb[0] ? cat_indice_por_imdb(it.imdb) : -1;
        if (idx < 0 && it.imdb[0]) {   // ja juntado por outro alvo neste refiltrar?
          for (k = 0; k < nNovos; k++)
            if (!strcmp(novos[k].imdb, it.imdb)) { idx = -(k + 2); break; }
        }
        if (idx >= 0 || idx <= -2) {
          fil[nFil].itens[achou++] = idx;
        } else {
          if (!novos) {
            novos = malloc(sizeof(CatItem) * (size_t)capNovos);
            idxNovos = malloc(sizeof(int) * (size_t)capNovos);
            if (!novos || !idxNovos) { free(novos); free(idxNovos); novos = NULL; idxNovos = NULL; break; }
          }
          if (nNovos >= capNovos) break;
          novos[nNovos] = it;
          fil[nFil].itens[achou++] = -(nNovos + 2);
          nNovos++;
        }
      }
      if (achou > 0) {
        fil[nFil].titulo = desc_busca_alvo_titulo(alvoIdx);
        fil[nFil].origem = mostraFonte ? desc_busca_alvo_addon(alvoIdx) : NULL;
        fil[nFil].n = achou;
        nFil++;
      }
    }
    if (nNovos > 0) {
      int entraram = cat_acrescentar_lote(novos, nNovos, idxNovos);
      for (k = fil0; k < nFil; k++) {
        int r, w = 0;
        for (r = 0; r < fil[k].n; r++) {
          int v = fil[k].itens[r];
          if (v <= -2) { int q = -v - 2; v = q < entraram ? idxNovos[q] : -1; }
          // Os que nao couberam (catalogo no teto) saem COMPACTADOS: buraco no
          // meio apontaria o card para o item errado.
          if (v >= 0) fil[k].itens[w++] = v;
        }
        fil[k].n = w;
      }
      { int w = fil0;
        for (k = fil0; k < nFil; k++) if (fil[k].n > 0) { if (w != k) fil[w] = fil[k]; w++; }
        nFil = w; }
    }
    free(novos); free(idxNovos);
    // SEM PISCAR: ate a 1a resposta do termo novo, as fileiras do termo anterior
    // ficam. Com resposta (mesmo vazia), so o que e do termo corrente.
    if (chegou) {
      nVelho = 0;
      for (k = fil0; k < nFil && nVelho < BU_MAX_FILEIRAS; k++, nVelho++) {
        velho[nVelho].titulo = fil[k].titulo; velho[nVelho].origem = fil[k].origem;
        velho[nVelho].n = fil[k].n;
        memcpy(velho[nVelho].itens, fil[k].itens, sizeof velho[nVelho].itens);
      }
    } else if (nFil == fil0) {
      for (k = 0; k < nVelho && nFil < BU_MAX_FILEIRAS; k++, nFil++) {
        fil[nFil].titulo = velho[k].titulo; fil[nFil].origem = velho[k].origem;
        fil[nFil].n = velho[k].n;
        memcpy(fil[nFil].itens, velho[k].itens, sizeof velho[k].itens);
      }
    }
  }

  int nCat = cat_n_fileiras();
  for (int r = 0; r < nCat && nFil < BU_MAX_FILEIRAS; r++) {
    const CatFileira *cf = cat_fileira(r);
    if (!cf) break;
    if (desc_busca_base_oculta(cf->base)) continue;   // "Buscar no Cinemeta" desligado
    int achou = 0;
    for (int i = 0; i < cf->n && achou < BU_MAX_POR_FIL; i++) {
      const CatItem *ci = cat_item(cf->ini + i);
      if (!ci) continue;
      char titulo[320];
      normalizar(ci->titulo, titulo, sizeof titulo);
      if (strstr(titulo, alvo)) fil[nFil].itens[achou++] = cf->ini + i;
    }
    if (!achou) continue;
    fil[nFil].titulo = cf->titulo;
    // `catalogAddonNameEnabled` decide a linha "de <addon>" sob o titulo da
    // fileira. O dado NAO existe deste lado: `CatFileira` guarda chave, titulo,
    // tipo e a janela no vetor — o nome do addon fica em addons.c e a fileira
    // nao o carrega. Ate descoberta.c passar esse campo adiante, a linha nao e
    // desenhada; escrever o TIPO ("movie") no lugar seria pior que a ausencia,
    // porque leria como se fosse a origem.
    fil[nFil].origem = NULL;
    fil[nFil].n = achou;
    nFil++;
  }

  // O MELHOR RESULTADO (mockup): o primeiro titulo da primeira fileira sai dela
  // e vira uma entrada propria, de um item, desenhada como o tile grande. Foco,
  // abertura e onda tratam essa entrada como qualquer fileira.
  for (int i = 0; i < nFil; i++) { fil[i].melhor = 0; fil[i].pessoas = 0; }
  if (nFil > 0 && nFil < BU_MAX_FILEIRAS && fil[0].n > 0) {
    int melhorIdx = fil[0].itens[0];
    memmove(&fil[1], &fil[0], (size_t)nFil * sizeof fil[0]);
    nFil++;
    fil[0].titulo = NULL; fil[0].origem = NULL;
    fil[0].itens[0] = melhorIdx; fil[0].n = 1; fil[0].melhor = 1;
    memmove(&fil[1].itens[0], &fil[1].itens[1], (size_t)(fil[1].n - 1) * sizeof(int));
    if (--fil[1].n == 0) { memmove(&fil[1], &fil[2], (size_t)(nFil - 2) * sizeof fil[0]); nFil--; }
  }

  // PESSOAS (mockup): a fileira entra logo depois da primeira de titulos.
  montarPessoas(alvo);
  if (nPess > 0 && nFil < BU_MAX_FILEIRAS) {
    int pos = nFil < (fil[0].melhor ? 2 : 1) ? nFil : (fil[0].melhor ? 2 : 1);
    if (nFil == 0) pos = 0;
    memmove(&fil[pos + 1], &fil[pos], (size_t)(nFil - pos) * sizeof fil[0]);
    nFil++;
    memset(&fil[pos], 0, sizeof fil[pos]);
    fil[pos].titulo = i18n("Pessoas");
    fil[pos].pessoas = 1;
    fil[pos].n = nPess;
    for (int i = 0; i < nPess; i++) fil[pos].itens[i] = i;
  }
montado:
  // Quem ja estava na tela herda o estado da onda; quem nao estava e nova.
  { char chaveAntes[BU_MAX_FILEIRAS][96];
    Uint32 entraAntes[BU_MAX_FILEIRAS];
    int novaAntes[BU_MAX_FILEIRAS];
#ifdef NV_TOUCH_UI
    int livresAntes[BU_MAX_FILEIRAS], toqueAntes = toqueFileira;
    float scrollAntes[BU_MAX_FILEIRAS];
    memcpy(livresAntes, toqueLivreX, sizeof livresAntes);
    memcpy(scrollAntes, scrollX, sizeof scrollAntes);
    memset(toqueLivreX, 0, sizeof toqueLivreX); toqueFileira = -1;
#endif
    memcpy(chaveAntes, filChave, sizeof chaveAntes);
    memcpy(entraAntes, filEntraEm, sizeof entraAntes);
    memcpy(novaAntes, filNova, sizeof novaAntes);
    for (int r = 0; r < BU_MAX_FILEIRAS; r++) {
      filChave[r][0] = 0; filNova[r] = 0; filEntraEm[r] = 0;
      if (r >= nFil) continue;
      snprintf(filChave[r], sizeof filChave[r], "%s|%s",
               fil[r].titulo ? fil[r].titulo : "", fil[r].origem ? fil[r].origem : "");
      filNova[r] = 1;
      for (int k = 0; k < BU_MAX_FILEIRAS; k++)
        if (chaveAntes[k][0] && !strcmp(chaveAntes[k], filChave[r])) {
          filNova[r] = novaAntes[k]; filEntraEm[r] = entraAntes[k];
#ifdef NV_TOUCH_UI
          if (buTelefone() && mesmaConsulta && livresAntes[k]) {
            toqueLivreX[r] = 1; scrollX[r] = scrollAntes[k]; velX[r] = 0.0f;
          }
          if (buTelefone() && mesmaConsulta && k == toqueAntes) toqueFileira = r;
#endif
          break;
        }
    } }

  int cols[BU_MAX_FILEIRAS];
  for (int i = 0; i < nFil; i++) cols[i] = fil[i].n;
  focus_iniciar(&focoRes, nFil > 0 ? nFil : 1, nFil > 0 ? cols : (int[]){ 1 });
  if (anterior >= 0) {
    int encontrado = 0;
    for (int r = 0; r < nFil && !encontrado; r++)
      for (int c = 0; c < fil[r].n; c++)
        if (fil[r].itens[c] == anterior) {
          focoRes.fileira = r; focoRes.coluna = c;
          focoRes.colunaLembrada[r] = c;
          encontrado = 1; break;
        }
  }
  if (nFil == 0 && painel == 1) painel = 0;
  memset(animRes, 0, sizeof animRes); memset(revRes, 0, sizeof revRes);
  if (!mesmaConsulta) {
#ifdef NV_TOUCH_UI
    toqueBuscaLimpar();
#endif
    memset(scrollX, 0, sizeof scrollX); memset(velX, 0, sizeof velX);
    scrollY = scrollAlvo = 0.0f; velY = 0.0f;
  }
}

static float buPasso(void);
static float buCartazW(void);

// --- Buscas recentes --------------------------------------------------------
// QUANDO UMA BUSCA CONTA COMO FEITA. Nao a cada letra: refiltrar roda por
// tecla, e gravar ali encheria a lista de "ma", "mat", "matr". Conta quando o
// dono DEMONSTRA que a busca serviu — entrou nos resultados, abriu um titulo,
// saiu da tela ou apagou o campo com resultado na tela (viu e desistiu; o que
// viu ainda e o que ele buscou), ou refez pela pilula. O limite de 2
// caracteres e o dedupe ficam em buscasrec.c.
static void registrarConsulta(void) {
  if (nConsulta >= 2 && nFil > 0) buscasrec_registrar(consulta);
}

static int recentesVisiveis(void) { return nConsulta == 0 && buscasrec_n() > 0; }
// Resultados DE VERDADE (e nao as sugestoes do campo vazio).
static int temResultados(void) { return !sugestao && nFil > 0; }

static void recentesAjustarFoco(void) {
  int n = buscasrec_n();
  if (focoRec > n) focoRec = n;
  if (focoRec < 0) focoRec = 0;
  if (n == 0 && painel == 2) painel = 0;
}

// Entrar na lista vindo do teclado: a pilula da linha mais proxima, na altura,
// da tecla em foco — a mesma continuidade que a ponte teclado->resultados tem.
// Sem geometria ainda (primeiro quadro), a primeira pilula.
static void recentesEntrar(void) {
  float ky = BU_KB_Y + BU_KB_PAD + focoKb.fileira * BU_KB_PASSO + BU_TECLA_W * 0.5f;
  float melhor = 1e9f;
  int i;
  painel = 2;
  focoRec = 0;
  for (i = 0; i < nRecLayout; i++) {
    float d = recRect[i].y + recRect[i].h * 0.5f - ky;
    if (d < 0) d = -d;
    if (d < melhor - 0.5f) { melhor = d; focoRec = i; }
  }
  recentesAjustarFoco();
}

// Cima/baixo: a pilula da linha vizinha cujo CENTRO em x esta mais perto.
static int recentesVertical(int dy) {
  int alvoLin, i, melhorI = -1;
  float cx, melhor = 1e9f;
  if (focoRec >= nRecLayout) return 0;
  alvoLin = recLin[focoRec] + dy;
  cx = recRect[focoRec].x + recRect[focoRec].w * 0.5f;
  for (i = 0; i < nRecLayout; i++) {
    float d;
    if (recLin[i] != alvoLin) continue;
    d = recRect[i].x + recRect[i].w * 0.5f - cx;
    if (d < 0) d = -d;
    if (d < melhor) { melhor = d; melhorI = i; }
  }
  if (melhorI >= 0) { focoRec = melhorI; return 1; }
  // Abaixo da ultima linha de pilulas: a fileira de Populares, na coluna mais
  // proxima. Acima da primeira, nada (o teclado fica a esquerda).
  else if (dy > 0 && sugestao && nFil > 0) {
    float passo = buPasso();
    int c = (int)((cx - BU_RES_X + scrollX[0]) / passo);
    if (c < 0) c = 0;
    if (c >= fil[0].n) c = fil[0].n - 1;
    painel = 1; focoRes.fileira = 0; focoRes.coluna = c;
    return 1;
  } else if (dy < 0 && buTecladoNativo()) {
    painel = 0; campoFoco = 1; return 1;
  } else if (dy < 0 && !ajustes_menu_topo()) { pediuRelogio = 1; return 1; }
  return 0;
}

// OK curto: refaz a busca com o termo (e ele sobe para o topo), ou, no
// "Limpar", apaga tudo. O foco volta ao TECLADO depois de refazer: os
// resultados chegam da rede em seguida e a ponte -> e a mesma de sempre;
// deixar o foco numa lista que acabou de sumir o poria em lugar nenhum.
static void recentesAcionar(void) {
  int n = buscasrec_n();
  if (focoRec >= n) { buscasrec_limpar(); recentesAjustarFoco(); return; }
  snprintf(consulta, sizeof consulta, "%s", buscasrec_termo(focoRec));
  nConsulta = (int)strlen(consulta);
  buscasrec_registrar(consulta);
  painel = 0;
  refiltrarJa();
}

// Pressao longa: remove o termo em foco; no "Limpar", o mesmo que o OK curto.
static void recentesRemover(void) {
  if (focoRec >= buscasrec_n()) buscasrec_limpar();
  else buscasrec_remover(focoRec);
  recentesAjustarFoco();
}

// --- Teclas ------------------------------------------------------------------
// Acrescenta texto UTF-8 ao campo se couber inteiro (nunca meia sequencia).
static void campoAcrescentar(const char *t) {
  size_t n = strlen(t);
  if ((size_t)nConsulta + n + 1 > BU_MAX_CONSULTA) return;
  memcpy(consulta + nConsulta, t, n);
  nConsulta += (int)n;
  consulta[nConsulta] = 0;
}

static void campoApagar(void) {
  nConsulta = (int)busca_apagar_ultimo(consulta, (size_t)nConsulta);
}

// O campo INTEIRO vindo do teclado/voz do sistema. Cortado sem meia sequencia
// UTF-8; `aparar` tira os espacos das pontas (a voz as vezes manda).
static void campoDefinir(const char *t, int aparar) {
  size_t i, w = 0;
  if (aparar) while (*t == ' ') t++;
  for (i = 0; t[i] && w + 1 < sizeof consulta; i++)
    consulta[w++] = (t[i] == '\n' || t[i] == '\t') ? ' ' : t[i];
  // cortou no meio de um caractere: volta ao comeco dele (i == w, copia 1:1)
  if (t[w]) while (w > 0 && ((unsigned char)t[w] & 0xC0) == 0x80) w--;
  if (aparar) while (w > 0 && consulta[w - 1] == ' ') w--;
  consulta[w] = 0;
  nConsulta = (int)w;
}

// Consulta vinda de fora (deeplink): como o campo digitado, resultados na hora.
void busca_definir_consulta(const char *t) {
  campoDefinir(t, 1);
  refiltrarJa();
  campoFoco = 0;
}

static int vozBusca(void) {
  return st_dono() == ST_BUSCA && (st_estado() == ST_OUVINDO || st_estado() == ST_PERMISSAO ||
                                   st_estado() == ST_VOZ_SISTEMA);
}
static void campoOk(void) {
  if (campoFoco == 3) { st_fechar(ST_BUSCA); celb_abrir(CELB_BUSCA, "Buscar filmes e séries"); }
  else if (campoFoco == 2) st_voz_iniciar(ST_BUSCA);
  else st_ime_abrir(ST_BUSCA, consulta, BU_MAX_CONSULTA - 1);
}
static void focarCampoPonteiro(int a, int b) {
  (void)b;
#ifdef NV_TOUCH_UI
  toqueBuscaLimpar();
#endif
  painel = 0; campoFoco = a;
}

// PONTEIRO (#99) NO TECLADO E NOS RESULTADOS: a tecla ou o titulo sob o cursor
// ganha o foco pelas mesmas variaveis das setas; o OK do clique digita a tecla
// ou abre o titulo, pelo caminho de sempre.
static void ponteiroTecla(int f, int c) {
  if (f < 0 || f > kbFil || c < 0 || c >= KB_COLUNAS[f]) return;
#ifdef NV_TOUCH_UI
  toqueBuscaLimpar();
#endif
  painel = 0; campoFoco = 0; focoKb.fileira = f; focoKb.coluna = c; focoKb.colunaLembrada[f] = c;
}
static void ponteiroResultado(int r, int c) {
  if (r < 0 || r >= nFil || c < 0 || c >= fil[r].n) return;
#ifdef NV_TOUCH_UI
  toqueBuscaLimpar();
#endif
  painel = 1; focoRes.fileira = r; focoRes.coluna = c; focoRes.colunaLembrada[r] = c;
}
#ifdef NV_TOUCH_UI
static void ponteiroRecente(int i, int b) {
  (void)b;
  if (!buTelefone()) return;
  if (i < 0 || i >= nRecLayout) return;
  toqueBuscaLimpar();
  painel = 2; focoRec = i;
}
#endif

static void aplicarTecla(void) {
  if (focoKb.fileira < kbFil) {
    int k = focoKb.fileira * BU_KB_COLS + focoKb.coluna;
    if (k < kbN) campoAcrescentar(kbTeclas[k]);
  } else if (focoKb.coluna == 0) {
    // espaco no comeco nao entra: nao muda o filtro e so acumula lixo no campo
    if (nConsulta > 0) campoAcrescentar(" ");
  } else if (focoKb.coluna == 1) {
    campoApagar();
  } else if (focoKb.coluna == 2) {
    registrarConsulta();
    nConsulta = 0;
  } else {
    // Troca de pagina (?!#/abc, coluna 3) ou de layout (cirilico, coluna 4):
    // a tecla de troca continua no mesmo lugar da fileira de baixo.
    int col = focoKb.coluna;
    if (col == 3) pagSim = !pagSim; else { layoutCir = !layoutCir; pagSim = 0; }
    kbMontar();
    focus_iniciar(&focoKb, kbFil + 1, KB_COLUNAS);
    focoKb.fileira = kbFil; focoKb.coluna = col == 3 ? 3 : 4;
    memset(animTecla, 0, sizeof animTecla);
    return;
  }
  consulta[nConsulta] = 0;
  refiltrar();
}

static GfxRect retanguloTecla(int fileira, int coluna) {
  GfxRect r;
  // A grade de teclas fica centrada na ilha (o justify-content:center do mockup).
  float fit = fminf(1.0f, (BU_COL_W - 2.0f * BU_KB_PAD) / BU_KB_W);
  float gx = BU_KB_X + (BU_COL_W - BU_KB_W * fit) * 0.5f;
  r.y = BU_KB_Y + BU_KB_PAD + fileira * BU_KB_PASSO;
  r.h = BU_TECLA_W;
  if (fileira < kbFil) {
    r.x = gx + coluna * BU_KB_PASSO;
    r.w = BU_TECLA_W;
  } else {
    int n = KB_COLUNAS[kbFil];
    r.w = (BU_KB_W - (n - 1) * BU_TECLA_GAP) / (float)n;
    r.x = gx + coluna * (r.w + BU_TECLA_GAP);
  }
  r.x = gx + (r.x - gx) * fit;
  r.w *= fit;
  return r;
}

// --- Ciclo de vida -----------------------------------------------------------
// A mudanca de interface abandona offsets e editor medidos na outra tela.
static void buInterfaceAtualizar(void) {
#ifdef NV_TOUCH_UI
  static int modoAnterior = -1;
  int mobile = buTelefone();
  if (modoAnterior == mobile) return;
  if (modoAnterior == 1 && !mobile) st_fechar(ST_BUSCA);
  modoAnterior = mobile;
  toqueBuscaLimpar();
  scrollY = scrollAlvo = velY = 0.0f;
  memset(scrollX, 0, sizeof scrollX); memset(velX, 0, sizeof velX);
  nRecLayout = 0; sugDesloc = 0.0f; temItemFoco = 0;
  campoFoco = 0;
#endif
}

int busca_iniciar(void) {
  buInterfaceAtualizar();
#ifdef NV_TOUCH_UI
  toqueBuscaLimpar();
#endif
  layoutCir = pagSim = 0;
  kbMontar();
  focus_iniciar(&focoKb, kbFil + 1, KB_COLUNAS);
  painel = 0; sair = 0; pediuRelogio = 0; pedido = -1; pedidoPessoa = 0;
  sugestao = 0; nPess = 0; sugDesloc = 0.0f;
  nConsulta = 0; consulta[0] = 0;
  consultaFiltrada[0] = 0;
  scrollY = scrollAlvo = 0.0f; velY = 0.0f;
  animCampo = 0.0f;
  temItemFoco = 0;
  memset(animTecla, 0, sizeof animTecla);
  memset(animRes, 0, sizeof animRes); memset(revRes, 0, sizeof revRes);
  memset(scrollX, 0, sizeof scrollX); memset(velX, 0, sizeof velX);
  memset(animRec, 0, sizeof animRec);
  focoRec = 0; nRecLayout = 0;
  okPress = okLongo = 0; okDesde = 0;
  campoFoco = buTecladoNativo() ? 1 : 0; animFocoCampo = animMic = 0.0f;
  // "Do sistema" no ajuste Teclado: abre o IME logo ao entrar na busca.
  if (st_ime_disponivel() && st_teclado_modo() != 1 &&
      (st_abre_sozinho() || buTelefone() || st_teclado_modo() == 2)) {
    campoFoco = 1;
    st_ime_abrir(ST_BUSCA, consulta, BU_MAX_CONSULTA - 1);
  }
  refiltrar();
  return 1;
}

void busca_encerrar(void) { temItemFoco = 0; pediuRelogio = 0; st_fechar(ST_BUSCA); celb_fechar_dono(CELB_BUSCA); }
const char *busca_consulta(void) { return consulta; }
int busca_foco_campo(void) { return painel == 0 ? campoFoco : 0; }
#ifdef NV_TESTE
void busca_teste_layout_recentes(void) {
  int n = buscasrec_n();
  nRecLayout = n + 1 < BU_REC_ITENS ? n + 1 : BU_REC_ITENS;
  for (int i = 0; i < nRecLayout; i++) {
    recRect[i] = (GfxRect){ BU_RES_X + i * 180.0f, BU_REC_Y, 160.0f, BU_REC_H };
    recLin[i] = 0;
  }
}
#endif
int  busca_quer_sair(void) { return sair; }
int busca_pediu_relogio(void) { int v = pediuRelogio; pediuRelogio = 0; return v; }
int busca_relogio_navegavel(void) { return !teclado_aberto() && st_estado() == ST_PARADO; }

int busca_pediu_abrir(int *indiceCatalogo) {
  if (pedido < 0) return 0;
  if (indiceCatalogo) *indiceCatalogo = pedido;
  pedido = -1;
  return 1;
}

int busca_pediu_pessoa(SpotPedido *p) {
  if (!pedidoPessoa) return 0;
  pedidoPessoa = 0;
  if (p) *p = pedidoP;
  return 1;
}

int busca_item_focado(HomeItem *out) {
  if (!out || painel != 1 || focoRes.fileira < 0 || focoRes.fileira >= nFil ||
      fil[focoRes.fileira].pessoas || focoRes.coluna < 0 ||
      focoRes.coluna >= fil[focoRes.fileira].n) return 0;
  int i = fil[focoRes.fileira].itens[focoRes.coluna];
  const CatItem *ci = cat_item(i);
  if (!ci) return 0;
  // Events can select another card before the next draw. Only reuse its rect
  // when the cached card matches; title identity always comes from selection.
  memset(out, 0, sizeof *out);
  out->indice = i;
  out->rect = temItemFoco && itemFoco.indice == i ? itemFoco.rect
            : (GfxRect){0, 0, NV_TELA_W, NV_TELA_H};
  out->arte = ci->backdrop[0] ? ci->backdrop : ci->poster;
  out->titulo = ci->titulo; out->genero = ci->genero; out->meta = ci->meta;
  return 1;
}

// O OK num resultado (toque curto). Titulo: o pedido do indice; pessoa: a
// filmografia dela, que app.c abre.
static void abrirResultado(void) {
  if (focoRes.fileira < nFil && focoRes.coluna < fil[focoRes.fileira].n) {
    registrarConsulta();
    if (fil[focoRes.fileira].pessoas) {
      int q = fil[focoRes.fileira].itens[focoRes.coluna];
      memset(&pedidoP, 0, sizeof pedidoP);
      pedidoP.tipo = SPOT_PESSOA; pedidoP.indice = pess[q].ref2; pedidoP.tmdb = pess[q].tmdb;
      pedidoP.tituloTmdb = pess[q].tituloTmdb;
      snprintf(pedidoP.tituloTipo, sizeof pedidoP.tituloTipo, "%s", pess[q].tituloTipo);
      snprintf(pedidoP.nome, sizeof pedidoP.nome, "%s", pess[q].nome);
      snprintf(pedidoP.arte, sizeof pedidoP.arte, "%s", pess[q].foto);
      pedidoPessoa = 1;
    } else pedido = fil[focoRes.fileira].itens[focoRes.coluna];
  }
}

// O resultado em foco aceita o menu do cartaz?
static int resultadoAceitaMenu(void) {
  return painel == 1 && temItemFoco && focoRes.fileira >= 0 && focoRes.fileira < nFil &&
         !fil[focoRes.fileira].pessoas && focoRes.coluna < fil[focoRes.fileira].n &&
         itemFoco.indice >= 0 && itemFoco.indice < cat_n();
}
static int menuNoResultado(void) {
  const CatItem *ci;
  const char *arte;
  if (!resultadoAceitaMenu()) return 0;
  ci = cat_item(itemFoco.indice);
  if (!ci) return 0;
  // "Melhor resultado" mostra o backdrop deitado; o cartaz em pe usa o poster.
  arte = itemFoco.rect.w > itemFoco.rect.h ? itemFoco.arte
       : posterprov_card_addon(ci->origem, ci->imdb, ci->tmdb, ci->tipo, ci->poster);
  if (!arte || !arte[0]) arte = itemFoco.arte;
  registrarConsulta();
  return ctxlista_abrir(itemFoco.indice, itemFoco.rect, arte);
}

static void busca_evento_(const SDL_Event *e) {
  pediuRelogio = 0;
  buInterfaceAtualizar();
  if (e->type == SDL_QUIT) { sair = 1; return; }
#ifdef NV_TOUCH_UI
  if (buTelefone() && e->type == SDL_KEYDOWN) toqueBuscaRetomarFoco();
#endif
  if (st_evento(e)) return;   // teclado da TV: o valor inteiro vem por sistexto
  // TEXTO DE TECLADO FISICO OU IME (#176): o que nao e ASCII (cirilico, ș, ț,
  // ă...) chega como SDL_TEXTINPUT. O ASCII fica com o SDL_KEYDOWN abaixo — o
  // mesmo caractere chega pelos dois, e entrar nos dois duplicaria a letra.
  if (e->type == SDL_TEXTINPUT) {
    if (painel == 0 && (unsigned char)e->text.text[0] >= 0x80) {
      campoAcrescentar(e->text.text);
      refiltrar();
    }
    return;
  }
  SDL_Keycode k = e->key.keysym.sym;

  // OK NUM RESULTADO DECIDE NA SOLTURA, como a pilula: toque abre o titulo,
  // segurar abre o menu do cartaz.
  switch (ctxhold_evento(&holdRes, e, resultadoAceitaMenu())) {
    case CTXH_LONGO: if (!menuNoResultado()) abrirResultado(); return;
    case CTXH_CONSUMIDO: return;
    case CTXH_TOQUE: abrirResultado(); return;
    default: break;
  }

  // OK NA PILULA DECIDE NA SOLTURA: so ali se sabe se foi toque ou pressao
  // longa. KEYUP sem KEYDOWN visto aqui nao e clique (a barra lateral decide no
  // KEYDOWN e o KEYUP do mesmo toque cai nesta tela — o issue #8 da home).
  if ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && painel == 2) {
    if (e->type == SDL_KEYDOWN) {
      if (!okPress) { okPress = 1; okLongo = 0; okDesde = SDL_GetTicks(); }
    } else if (e->type == SDL_KEYUP && okPress) {
      Uint32 dur = ponteiro_ok_longo() ? NV_HOLD_MS : SDL_GetTicks() - okDesde;
      okPress = 0; okDesde = 0;
      if (okLongo) okLongo = 0;
      else if (dur >= NV_HOLD_MS) recentesRemover();
      else recentesAcionar();
    }
    return;
  }
  if (e->type == SDL_KEYUP && (k == SDLK_RETURN || k == SDLK_KP_ENTER)) {
    okPress = 0; okLongo = 0; okDesde = 0;
  }
  if (e->type != SDL_KEYDOWN) return;
  if (buTecladoNativo() && painel == 0 && !campoFoco) campoFoco = 1;

  if (k == SDLK_BACKSPACE && painel == 0) {
    if (nConsulta > 0) { campoApagar(); refiltrar(); }
    return;
  }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE) {
    // Nos resultados, o Back volta ao teclado: e o movimento inverso do que
    // levou ate la. So do teclado ele fecha a tela.
    if (painel != 0) painel = 0;
    else { registrarConsulta(); sair = 1; }
    return;
  }

  if (painel == 2) {
    // Letra do teclado FISICO com o foco nas pilulas: vai para o teclado da
    // tela e digita — o dono comecou uma busca nova, nao quer escolher pilula.
    if (!(e->key.keysym.mod & (KMOD_CTRL | KMOD_ALT | KMOD_GUI)) &&
        e->key.keysym.scancode != NV_SCANCODE_BLUE &&   // a AZUL/CH+ do controle chega com sym "s"
        ((k >= SDLK_a && k <= SDLK_z) || (k >= SDLK_0 && k <= SDLK_9))) {
      painel = 0;
    } else {
      switch (k) {
        case SDLK_TAB: painel = 0; break;
        case SDLK_LEFT:
          // Primeira pilula da LINHA devolve ao teclado, como a primeira coluna
          // dos resultados. Nas outras, anda para a anterior.
          if (focoRec == 0 || focoRec >= nRecLayout || focoRec - 1 >= nRecLayout ||
              recLin[focoRec - 1] != recLin[focoRec]) painel = 0;
          else focoRec--;
          break;
        case SDLK_RIGHT:
          // Fim da linha para: pular para a linha de baixo pela direita
          // desencontra do ESQUERDA, que no comeco da linha volta ao teclado.
          if (focoRec + 1 < nRecLayout && recLin[focoRec + 1] == recLin[focoRec])
            focoRec++;
          break;
        case SDLK_UP:
          if (!recentesVertical(-1) && ajustes_menu_topo()) sair = 1;
          break;
        case SDLK_DOWN: recentesVertical(1);  break;
        default: break;
      }
      return;
    }
  }

  if (painel == 0 && campoFoco) {
    switch (k) {
      case SDLK_DOWN:
        if (buTecladoNativo()) {
          if (nFil > 0 && !sugestao) { registrarConsulta(); painel = 1; }
          else if (recentesVisiveis()) recentesEntrar();
          else if (nFil > 0) painel = 1;
        } else campoFoco = 0;
        return;
      // DIREITA anda campo -> Falar -> Celular, o que existir aqui.
      case SDLK_RIGHT:
        if (campoFoco == 1 && st_voz_disponivel()) campoFoco = 2;
        else if (campoFoco < 3 && celb_disponivel()) campoFoco = 3;
        return;
      case SDLK_LEFT:
        if (campoFoco == 3 && st_voz_disponivel()) campoFoco = 2;
        else if (campoFoco >= 2 && st_ime_disponivel()) campoFoco = 1;
        else if (campoFoco >= 2) campoFoco = 0;
        else { registrarConsulta(); sair = 1; }
        return;
      case SDLK_UP:
        if (ajustes_menu_topo()) sair = 1; else pediuRelogio = 1;
        return;
      case SDLK_RETURN: case SDLK_KP_ENTER: if (!e->key.repeat) campoOk(); return;
      default: break;
    }
  }
  if (painel == 0) {
    if (!(e->key.keysym.mod & (KMOD_CTRL | KMOD_ALT | KMOD_GUI)) &&
        e->key.keysym.scancode != NV_SCANCODE_BLUE &&
        ((k >= SDLK_a && k <= SDLK_z) || (k >= SDLK_0 && k <= SDLK_9) || k == SDLK_SPACE)) {
      if (nConsulta + 1 < BU_MAX_CONSULTA && (k != SDLK_SPACE || nConsulta)) {
        char um[2] = { (char)k, 0 };
        campoAcrescentar(um); refiltrar();
      }
      return;
    }
    if (k == SDLK_TAB && temResultados()) { registrarConsulta(); painel = 1; return; }
    if (k == SDLK_TAB && recentesVisiveis()) { recentesEntrar(); return; }
    if (k == SDLK_TAB && nFil > 0) { painel = 1; return; }
    switch (k) {
      case SDLK_LEFT:
        if (!focus_mover_grade(&focoKb, -1, 0)) { registrarConsulta(); sair = 1; }
        break;
      case SDLK_RIGHT:
        // Passar da ULTIMA coluna do teclado entra nos resultados. E a unica
        // ponte entre os dois paineis, e por isso ela nao pode falhar em
        // silencio: sem resultado nenhum, o foco fica onde esta.
        // Com o campo vazio a ponte leva as buscas recentes, que ocupam o
        // mesmo lugar das fileiras.
        if (focoKb.coluna >= KB_COLUNAS[focoKb.fileira] - 1) {
          if (temResultados()) { registrarConsulta(); painel = 1; }
          else if (recentesVisiveis()) recentesEntrar();
          else if (nFil > 0) painel = 1;   // Populares
        } else focus_mover_grade(&focoKb, 1, 0);
        break;
      // GRADE, e nao fileiras: ver focus_mover_grade. Era daqui que saia o
      // salto para uma letra aleatoria ao subir ou descer no teclado.
      // CIMA da primeira fileira: o campo, onde ha teclado do sistema.
      case SDLK_UP:
        // Sem teclado do sistema o campo nao e alvo: cima vai ao microfone
        // da Samsung, ou ao celular onde nao ha voz. Sem esses alvos,
        // a primeira fileira abre a barra do topo.
        if (!focus_mover_grade(&focoKb, 0, -1)) {
          if (st_ime_disponivel()) campoFoco = 1;
          else if (st_voz_disponivel()) campoFoco = 2;
          else if (celb_disponivel()) campoFoco = 3;
          else if (ajustes_menu_topo()) sair = 1;
          else pediuRelogio = 1;
        }
        break;
      case SDLK_DOWN:   focus_mover_grade(&focoKb, 0,  1); break;
      case SDLK_RETURN: case SDLK_KP_ENTER: aplicarTecla(); break;
      default: break;
    }
    return;
  }

  switch (k) {
    case SDLK_TAB: painel = 0; break;
    case SDLK_LEFT:
      // Voltar da primeira coluna dos resultados devolve o foco ao teclado.
      if (focoRes.coluna == 0) painel = 0;
      else focus_mover(&focoRes, -1, 0);
      break;
    case SDLK_RIGHT:
      // `fastHorizontalNavigationEnabled`: o web pula de 3 em 3 dentro da
      // fileira quando a preferencia esta ligada. Numa fileira de 12 cards, 4
      // toques em vez de 12 para chegar ao fim.
      focus_mover(&focoRes, 1, 0);
      if (ajustes_navegacao_horizontal_rapida()) {
        focus_mover(&focoRes, 1, 0);
        focus_mover(&focoRes, 1, 0);
      }
      break;
    case SDLK_UP: {
      int moveu = focus_mover(&focoRes, 0, -1);
      if (!moveu && sugestao && focoRes.fileira == 0 && recentesVisiveis()) {
        // Do primeiro Populares de volta as pilulas: a mais proxima em x.
        float cx = BU_RES_X + focoRes.coluna * buPasso() - scrollX[0] + buCartazW() * 0.5f;
        float melhor = 1e9f;
        int i;
        painel = 2; focoRec = 0;
        for (i = 0; i < nRecLayout; i++) {
          float d = recRect[i].x + recRect[i].w * 0.5f - cx;
          if (recLin[i] != recLin[nRecLayout - 1]) continue;
          if (d < 0) d = -d;
          if (d < melhor) { melhor = d; focoRec = i; }
        }
        recentesAjustarFoco();
      } else if (!moveu && buTecladoNativo()) { painel = 0; campoFoco = 1; }
      else if (!moveu) { if (ajustes_menu_topo()) sair = 1; else pediuRelogio = 1; }
      break;
    }
    case SDLK_DOWN: focus_mover(&focoRes, 0,  1); break;
    case SDLK_RETURN: case SDLK_KP_ENTER: abrirResultado(); break;
    default: break;
  }
}

// --- Geometria das entradas da direita ---------------------------------------
// Five columns at 100%; scaled text keeps enough stride for preserved artwork.
static float buPasso(void) {
  float larg = BU_DIR - BU_RES_X;
  if (buTelefone()) {
    if (buTecladoNativo()) {
      int cols = buRetrato() ? 3 : (int)((larg + 22.0f) / 270.0f);
      if (cols < 1) cols = 1;
      if (cols > 8) cols = 8;
      return (fmaxf(1.0f, larg) - (cols - 1) * 22.0f) / cols + 22.0f;
    }
    int cols = (int)((larg + 22.0f) / 182.0f);
    if (cols < 1) cols = 1;
    if (cols > 5) cols = 5;
    return (fmaxf(1.0f, larg) - (cols - 1) * 22.0f) / cols + 22.0f;
  } else {
    float passo = (larg - 4.0f * 22.0f) / 5.0f + 22.0f;
    return gfx_escala_txt() != 1.0f ? fmaxf(passo, BU_CARTAZ_W + 22.0f) : passo;
  }
}
static float buCartazW(void) {
  if (gfx_escala_txt() != 1.0f) {
    if (!buTelefone()) return BU_CARTAZ_W;
    float old = gfx_escala();
    gfx_escala_sair(1.0f);
    float w = buPasso() - 22.0f;
    if (!buTecladoNativo() && w > BU_CARTAZ_W) w = BU_CARTAZ_W;
    gfx_escala_sair(old);
    return w * gfx_poster_k();
  }
  float w = buPasso() - 22.0f;
  return buTecladoNativo() || w < BU_CARTAZ_W ? w : BU_CARTAZ_W;
}
// PESSOAS: avatar 64 + 14 + nome (18), vao 26 entre pessoas (o .av do mockup).
#define BU_PESS_AV     (buTecladoNativo() ? 96.0f : 64.0f)
#define BU_PESS_GAP    26.0f
#define BU_PESS_NOMEMAX 230.0f
// x (relativo a coluna) e largura de cada pessoa, preenchidos pelo desenho (a
// largura do nome vem do texto rasterizado) e lidos pela rolagem.
static float pessX[BU_MAX_PESS], pessW[BU_MAX_PESS];
static float filKickH(int r) {
  if (buTelefone()) {
    int origem = !fil[r].melhor && fil[r].origem && fil[r].origem[0];
    if (origem && (buRetrato() || BU_DIR - BU_RES_X < 760.0f))
      return buTecladoNativo() ? 78.0f : 58.0f;
    if (buTecladoNativo()) return 46.0f;
  }
  return BU_KICK_H;
}
static float filAlt(int r) {
  // O alvo e a pilula de foco chegam 8px abaixo do avatar no telefone.
  if (fil[r].pessoas) return filKickH(r) + BU_PESS_AV + (buTelefone() ? 8.0f : 0.0f);
  if (fil[r].melhor) return filKickH(r) + BU_MELHOR_H;
  return filKickH(r) + buCartazW() * 1.5f + BU_NOME_ALT;
}
static float filTopo(int r) {
  float y = sugestao ? sugDesloc : 0.0f;
  for (int i = 0; i < r; i++) y += filAlt(i) + BU_SECAO_GAP;
  return y;
}

#ifdef NV_TOUCH_UI
static float toqueBuscaMaxY(void) {
  float fim = nFil > 0 ? filTopo(nFil - 1) + filAlt(nFil - 1)
                      : recentesVisiveis() ? sugDesloc - BU_SECAO_GAP : 0.0f;
  return fmaxf(0.0f, fim - BU_RES_AREA_H);
}
static float toqueBuscaMaxX(int r) {
  int n = fil[r].n;
  float fim;
  if (n < 1 || fil[r].melhor) return 0.0f;
  fim = fil[r].pessoas ? pessX[n - 1] + pessW[n - 1]
                      : (n - 1) * buPasso() + buCartazW();
  return fmaxf(0.0f, fim - (BU_DIR - BU_RES_X));
}
static int toqueBuscaRolar(const PonteiroRolagem *e) {
  if (!buTelefone()) { toqueBuscaLimpar(); return 0; }
  if (e->fase == PONT_ROL_INICIO) {
    if (e->x < BU_RES_X || e->x >= BU_DIR || e->y < BU_RES_Y - 20.0f ||
        e->y >= BU_RES_Y + BU_RES_AREA_H || (nFil < 1 && !recentesVisiveis())) return 0;
    toqueFileira = -1;
    if (!e->eixoY) {
      for (int r = 0; r < nFil; r++) {
        float y = BU_RES_Y + filTopo(r) - scrollY;
        if (e->y >= y && e->y < y + filAlt(r)) { toqueFileira = r; break; }
      }
      if (toqueFileira < 0) return 0;
      toqueLivreX[toqueFileira] = 1; velX[toqueFileira] = 0.0f;
    }
    toqueLivreY = 1; velY = 0.0f; scrollAlvo = scrollY;
    ctxhold_cancelar(&holdRes);
    return 1;
  }
  if (e->fase == PONT_ROL_MOVER || e->fase == PONT_ROL_INERCIA) {
    float *s, antes, maximo;
    if (e->eixoY) { s = &scrollY; maximo = toqueBuscaMaxY(); }
    else {
      if (toqueFileira < 0 || toqueFileira >= nFil) return 0;
      s = &scrollX[toqueFileira]; maximo = toqueBuscaMaxX(toqueFileira);
    }
    antes = *s; *s = anim_clamp(*s - e->delta, 0.0f, maximo);
    if (e->eixoY) scrollAlvo = scrollY;
    return fabsf(*s - antes) > 0.001f;
  }
  if (e->fase == PONT_ROL_FIM || e->fase == PONT_ROL_CANCELAR) toqueFileira = -1;
  return 1;
}
static void toqueBuscaRetomarFoco(void) {
  if (!buTelefone()) { toqueBuscaLimpar(); return; }
  if (toqueLivreY && nFil > 0) {
    float ponto = scrollY + BU_RES_AREA_H * 0.35f;
    int r = 0;
    while (r + 1 < nFil && filTopo(r) + filAlt(r) < ponto) r++;
    painel = 1; focoRes.fileira = r; focoRes.coluna = focoRes.colunaLembrada[r];
  }
  for (int r = 0; r < nFil; r++) if (toqueLivreX[r] && fil[r].n > 0) {
    float ponto = scrollX[r] + (BU_DIR - BU_RES_X) * 0.35f;
    int c = (int)(ponto / buPasso());
    if (fil[r].pessoas) { c = 0; while (c + 1 < fil[r].n && pessX[c] + pessW[c] < ponto) c++; }
    if (c >= fil[r].n) c = fil[r].n - 1;
    if (c < 0) c = 0;
    focoRes.colunaLembrada[r] = c;
    if (focoRes.fileira == r) focoRes.coluna = c;
  }
  toqueBuscaLimpar();
}
#endif

static void busca_atualizar_(float dt, Uint32 agora) {
  buInterfaceAtualizar();
  // TECLADO/VOZ DO SISTEMA (sistexto.h): o texto vem inteiro e substitui o campo.
  { char t[BU_MAX_CONSULTA * 2];
    int voz = vozBusca();
    int r;
    // DO CELULAR: o campo inteiro, como se a pessoa tivesse digitado; com
    // resultado o foco vai para eles, como no "Concluir" do teclado da TV.
    if (celb_pegar(CELB_BUSCA, t, sizeof t)) {
      st_fechar(ST_BUSCA);
      campoDefinir(t, 1);
      memset(t, 0, sizeof t);
      refiltrarJa();
      campoFoco = 0;
      if (temResultados()) { registrarConsulta(); painel = 1; }
    }
    r = st_ler(ST_BUSCA, t, sizeof t);
    if (r == ST_PEDE_TECLADO) { campoFoco = 1; st_ime_abrir(ST_BUSCA, consulta, BU_MAX_CONSULTA - 1); }
    else if (r == ST_TEXTO || r == ST_FIM) {
      campoDefinir(t, voz || r == ST_FIM);
      // Concluir / voz entregam o texto FINAL: sem esperar o debounce.
      if (voz || r == ST_FIM) refiltrarJa(); else refiltrar();
      if (r == ST_FIM && temResultados()) { registrarConsulta(); painel = 1; campoFoco = 0; }
    } }
  // O RESULTADO DA REDE CHEGA DEPOIS DA TECLA. refiltrar() so roda quando o
  // dono digita, entao sem isto a resposta do Cinemeta chegava, ficava guardada
  // e NUNCA aparecia — a tela seguia mostrando o filtro local do momento em que
  // a ultima letra foi apertada. Aqui a contagem do termo corrente e vigiada
  // por quadro, e uma mudanca remonta a lista uma vez so.
  // PESSOAS: o pedido ao TMDB sai quando o texto assenta (debounce); a resposta
  // refaz a fileira. Com o campo vazio, o catalogo que chega depois da abertura
  // refaz os Populares.
  // DEBOUNCE DA REDE: o texto assentou? Entao o pedido sai (uma vez por palavra).
  if (redePend && agora - redePendDesde >= BU_ESPERA_REDE_MS) {
    redeDisparar();
  }
  { static int ultimoCat = -1;
    spotpessoa_atualizar(agora);
    if (!sugestao && spotpessoa_geracao() != gerPessoa) refiltrar();
    if (sugestao && cat_n() != ultimoCat) { ultimoCat = cat_n(); refiltrar(); } }
  { char alvo[BU_MAX_CONSULTA * 2];
    static int ultimoRemoto = -1, ultimoChegou = -1;
    normalizar(consulta, alvo, sizeof alvo);
    if (busca_codepoints(alvo) >= 2) {
      int n = desc_busca_n(consulta);
      // A CONTAGEM SOZINHA NAO BASTA: "matrix" -> "matrixs" fica em 0 itens
      // enquanto a resposta nao chega (fileiras antigas mantidas de proposito)
      // e continua 0 quando ela chega VAZIA — n nao muda, o refiltro nao roda e
      // os cartoes de "matrix" ficariam clicaveis para sempre. Chegada e parte
      // da chave: a transicao "sem resposta" -> "resposta vazia" tambem refaz.
      int chegou = desc_busca_chegou(consulta);
      // Respostas de varios alvos no mesmo instante viram UM refiltrar por
      // BU_COALESCE_MS; `ultimoRemoto` so avanca quando ele roda, entao o
      // resto nao se perde (roda no proximo quadro depois da janela).
      if ((n != ultimoRemoto || chegou != ultimoChegou) && agora - ultimoRefiltro >= BU_COALESCE_MS) {
        ultimoRemoto = n; ultimoChegou = chegou; ultimoRefiltro = agora; refiltrar();
      }
    } else {
      ultimoRemoto = -1; ultimoChegou = -1;
    } }
  for (int f = 0; f <= kbFil; f++)
    for (int c = 0; c < KB_COLUNAS[f]; c++) {
      float alvo = (painel == 0 && !campoFoco && focus_indice(&focoKb, f, c)) ? 1.0f : 0.0f;
      animTecla[f][c] = anim_mola(animTecla[f][c], alvo, dt,
                                  alvo > animTecla[f][c] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
    }
  for (int r = 0; r < BU_MAX_FILEIRAS; r++)
    for (int c = 0; c < BU_MAX_POR_FIL; c++) {
      float alvo = (painel == 1 && focus_indice(&focoRes, r, c)) ? 1.0f : 0.0f;
      animRes[r][c] = anim_mola(animRes[r][c], alvo, dt,
                                alvo > animRes[r][c] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
    }
  animCampo = anim_mola(animCampo, painel == 0 ? 1.0f : 0.0f, dt, NV_MOLA_FOCO);
  if (painel != 0) campoFoco = 0;
  animFocoCampo = anim_mola(animFocoCampo, painel == 0 && campoFoco == 1 ? 1.0f : 0.0f, dt, NV_MOLA_FOCO);
  animMic = anim_mola(animMic, painel == 0 && campoFoco == 2 ? 1.0f : 0.0f, dt, NV_MOLA_FOCO);
  if (painel != 1) ctxhold_cancelar(&holdRes);
  else if (ctxhold_passo(&holdRes, agora, 1) && menuNoResultado()) ctxhold_cancelar(&holdRes);
  if (painel == 2 && okPress && !okLongo && segurar_venceu(agora, okDesde ? okDesde : 1, "busca recentes")) {
    okLongo = 1;
    recentesRemover();
  }
  if (painel != 2) { okPress = 0; okLongo = 0; }
  for (int i = 0; i < BU_REC_ITENS; i++) {
    float alvo = (painel == 2 && i == focoRec) ? 1.0f : 0.0f;
    animRec[i] = anim_mola(animRec[i], alvo, dt,
                           alvo > animRec[i] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  }

  // Rola so o necessario para a fileira em foco caber inteira na area util —
  // rolagem proporcional ao indice esconderia a primeira fileira antes de o
  // usuario ter chegado nela.
  if (painel == 1 && nFil > 0) {
    int r = focoRes.fileira;
    float topo = filTopo(r);
    float base = topo + filAlt(r);
#ifdef NV_TOUCH_UI
    if (!buTelefone() || !toqueLivreY) {
#endif
      if (topo - scrollAlvo < 0.0f)             scrollAlvo = topo;
      if (base - scrollAlvo > BU_RES_AREA_H)    scrollAlvo = base - BU_RES_AREA_H;
#ifdef NV_TOUCH_UI
    }
#endif

    // Rolagem horizontal da fileira em foco, mesma regra da home.
    float util = BU_DIR - BU_RES_X;
    float passo = buPasso();
    float esq = focoRes.coluna * passo;
    float dir = esq + buCartazW();
    if (fil[r].pessoas && focoRes.coluna < BU_MAX_PESS) {
      esq = pessX[focoRes.coluna]; dir = esq + pessW[focoRes.coluna];
    }
    float alvoX = scrollX[r];
    if (dir - alvoX > util) alvoX = dir - util;
    if (esq - alvoX < 0.0f) alvoX = esq;
    if (alvoX < 0.0f) alvoX = 0.0f;
#ifdef NV_TOUCH_UI
    if (!buTelefone() || !toqueLivreX[r])
#endif
      scrollX[r] = anim_mola2(&velX[r], scrollX[r], alvoX, dt, NV_MOLA2_SCROLL);
  } else {
#ifdef NV_TOUCH_UI
    if (!buTelefone() || !toqueLivreY)
#endif
    scrollAlvo = 0.0f;
  }
  if (scrollAlvo < 0.0f) scrollAlvo = 0.0f;
#ifdef NV_TOUCH_UI
  if (buTelefone()) {
    for (int r = 0; r < nFil; r++)
      scrollX[r] = anim_clamp(scrollX[r], 0.0f, toqueBuscaMaxX(r));
    scrollAlvo = anim_clamp(scrollAlvo, 0.0f, toqueBuscaMaxY());
  }
  if (buTelefone() && toqueLivreY) scrollY = scrollAlvo = anim_clamp(scrollY, 0.0f, toqueBuscaMaxY());
  else
#endif
    scrollY = anim_mola2(&velY, scrollY, scrollAlvo, dt, NV_MOLA2_SCROLL);
#ifdef NV_TOUCH_UI
  if (buTelefone()) scrollY = anim_clamp(scrollY, 0.0f, toqueBuscaMaxY());
#endif
}

// --- Desenho -----------------------------------------------------------------
// Tudo no material da ilha (ajustes_ui_*): vidro ou solido pela escolha do dono.
// Sem contorno em lugar nenhum: foco = superficie mais clara (campo, tile) ou
// pilula no acento (tecla, chip). O corpo do texto e o #F3F2EF do mockup.
#define BU_TX 243, 242, 239, 255

static void buAlvoResultado(GfxRect r, PonteiroFn focar, int a, int b) {
  if (buTelefone()) {
    float x = fmaxf(r.x, BU_RES_X), y = fmaxf(r.y, BU_RES_Y - 20.0f);
    float dir = fminf(r.x + r.w, BU_DIR);
    float base = fminf(r.y + r.h, BU_RES_Y + BU_RES_AREA_H);
    if (dir > x && base > y) ponteiro_alvo(x, y, dir - x, base - y, focar, NULL, a, b);
  } else {
    ponteiro_alvo_faixa(r.x, r.y, r.w, r.h, BU_RES_Y - 20.0f,
                        BU_RES_Y + BU_RES_AREA_H + 20.0f, focar, NULL, a, b);
  }
}

static void buCabecalho(int r, float y, float larg) {
  const char *titulo = fil[r].melhor ? i18n("Melhor resultado") : fil[r].titulo;
#ifdef NV_TOUCH_UI
  if (buTelefone()) {
  int origem = !fil[r].melhor && fil[r].origem && fil[r].origem[0];
  int empilhado = origem && (buRetrato() || larg < 760.0f);
  float reserva = origem && !empilhado ? fminf(360.0f, larg * 0.32f) + 16.0f : 0.0f;
  TxtLinha t = txt_linha_corta(buTecladoNativo() ? TXT_ILHA_SECAO : TXT_MINI,
                              titulo, BU_TX, fmaxf(1.0f, larg - reserva));
  txt_desenhar_alpha(t, BU_RES_X, y, 0.45f);
  if (origem) {
    char org[96];
    float x = empilhado ? BU_RES_X : BU_RES_X + fminf((float)t.w, larg - reserva) + 16.0f;
    float w = BU_DIR - x;
    snprintf(org, sizeof org, i18n("de %s"), fil[r].origem);
    if (w > 24.0f) {
      TxtLinha ts = txt_linha_corta(buTecladoNativo() ? TXT_ILHA_CORPO : TXT_ILHA_APOIO,
                                   org, BU_TX, w);
      txt_desenhar_alpha(ts, x, y + (empilhado ? (buTecladoNativo() ? 32.0f : 24.0f) : 0.0f), 0.36f);
    }
  }
  return;
  }
#endif
  {
  float kw = ajustes_ui_kicker(titulo, BU_RES_X, y, 1.0f);
  if (!fil[r].melhor && fil[r].origem) {
    char org[96];
    snprintf(org, sizeof org, i18n("de %s"), fil[r].origem);
    TxtLinha ts = txt_linha_corta(TXT_ILHA_APOIO, org, BU_TX, larg - kw - 16.0f);
    txt_desenhar_alpha(ts, BU_RES_X + kw + 16.0f, y, 0.36f);
  }
  }
}

static void buPilulaAcento(GfxRect r, float raioPx, float a) {
  float ar, ag, ab;
  if (a <= 0.003f) return;
  ajustes_acento(&ar, &ag, &ab);
  gfx_rect((GfxRect){ r.x - 14, r.y - 2, r.w + 28, r.h + 30 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f,
           ar, ag, ab, 0.32f * a);
  gfx_cor(r, raioPx / r.h, ar, ag, ab, a);
}

// Neutro da tecla/chip em repouso: branco 7-8% no vidro, #202127/#24262C no solido.
static void buNeutro(GfxRect r, float raioPx, float vidA, float a) {
  const float g = gfx_opacidade_grupo;
  gfx_opacidade_grupo = g * a;
  ajustes_ui_neutro(r, raioPx, vidA);
  gfx_opacidade_grupo = g;
}

static void buArte(GfxRect r, const char *url, float raioPx, float a) {
  float raio = raioPx / r.h;
  GLuint tex = (url && url[0]) ? tex_obter_larg(url, r.w) : 0;
  if (tex) {
    gfx_tex_aspect_atual = tex_aspecto(url);
    gfx_rect(r, tex, GFX_CARD, 0.0f, 0.0f, 0.0f, raio, 0, 0, 0, a);
    gfx_tex_aspect_atual = 0.0f;
  } else if (url && url[0] && !tex_falhou(url))
    gfx_esqueleto(r, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, a);
  else
    gfx_cor(r, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, a);
}

// Ilha do campo: lupa, texto 28/500, cursor no acento; com teclado/voz do
// sistema (Android) o campo e alvo de foco e o microfone/celular entram no fim.
static void desenhaCampo(Uint32 agora) {
  GfxRect c = { buX(), buTopoEsq(), BU_COL_W, BU_CAMPO_H };
  TxtEstilo estilo = buTecladoNativo() ? TXT_HEADLINE : TXT_CALLOUT;
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  ajustes_ui_ilha(c, 38.0f, 0);
  { int temIme = st_ime_disponivel(), temVoz = st_voz_disponivel(), temCel = celb_disponivel();
    float d = c.h - 20.0f, my = c.y + 10.0f;
    float cx = c.x + c.w - 10.0f - d;
    float mx = temCel ? cx - 10.0f - d : cx;
    float fim = temVoz ? mx - 10.0f : temCel ? cx - 10.0f : c.x + c.w;
    if (temIme && animFocoCampo > 0.01f) {
      const float g = gfx_opacidade_grupo;
      gfx_opacidade_grupo = g * animFocoCampo;
      ajustes_ui_foco_linha(c, 38.0f);
      gfx_opacidade_grupo = g;
    }
    if (temIme && ponteiro_ativo())
      ponteiro_alvo(c.x, c.y, (buUmaColuna() ? fminf(fim + 4.0f, c.x + c.w) : fim + 4.0f) - c.x,
                    c.h, focarCampoPonteiro, NULL, 1, 0);
    if (temCel)
      celb_botao(CELB_BUSCA, (GfxRect){ cx, my, d, d }, painel == 0 && campoFoco == 3,
                 focarCampoPonteiro, 3, 0, 1.0f);
    if (temVoz) {
      int ouve = vozBusca();
      float k = ouve ? 1.0f : animMic;
      GfxRect m = { mx, my, d, d };
      if (ponteiro_ativo()) ponteiro_alvo(mx - 4, c.y, d + 14, c.h, focarCampoPonteiro, NULL, 2, 0);
      buNeutro(m, d * 0.5f, 0.08f, 1.0f - k);
      if (k > 0.01f) {
        if (ouve) gfx_rect((GfxRect){ mx - 12, my - 12, d + 24, d + 24 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f,
                           ar, ag, ab, 0.4f * st_nivel() * k);
        buPilulaAcento(m, d * 0.5f, k);
      }
      { int t = k > 0.5f ? ajustes_tinta_foco() : 220;
        gfx_icone((GfxRect){ mx + d * 0.27f, my + d * 0.27f, d * 0.46f, d * 0.46f }, "aj_mic",
                  t / 255.0f, t / 255.0f, t / 255.0f, 1.0f); }
    }
    { float ci = 0.62f + 0.30f * animCampo;
      gfx_icone((GfxRect){ c.x + BU_CAMPO_PADX, c.y + (c.h - 28.0f) * 0.5f, 28.0f, 28.0f },
                "menu_search", ci, ci, ci + 0.01f, 1.0f); }
    { float tx = c.x + BU_CAMPO_PADX + 28.0f + 16.0f;
      float maxW = fim - tx - 14.0f;
      if (buUmaColuna()) gfx_recorte(tx - 8.0f, c.y + 4.0f, fim - tx, c.h - 8.0f);
      const char *av = st_dono() == ST_BUSCA ? st_aviso() : "";
      if (nConsulta && !av[0]) {
        TxtLinha l = txt_linha_corta(estilo, consulta, BU_TX, maxW);
        txt_desenhar(l, tx, c.y + (c.h - l.h) * 0.5f);
        tx += (buUmaColuna() ? fminf((float)l.w, maxW) : (float)l.w) + 2.0f;
      } else {
        const char *ph = vozBusca() ? i18n("Ouvindo…") : av[0] ? i18n(av) : "Buscar filmes e séries";
        TxtLinha l = txt_linha_corta(estilo, ph, 255, av[0] && !vozBusca() ? 200 : 243,
                                     av[0] && !vozBusca() ? 140 : 239, 255, maxW);
        txt_desenhar_alpha(l, tx, c.y + (c.h - l.h) * 0.5f, av[0] ? 0.9f : 0.42f);
      }
      if (painel == 0 && (nConsulta > 0 || campoFoco == 1) && (agora / 500) % 2 == 0)
        gfx_cor((GfxRect){ nConsulta ? tx + 2.0f : tx - 6.0f, c.y + (c.h - 30.0f) * 0.5f, 2.0f, 30.0f }, 0.5f, ar, ag, ab, 1.0f);
      if (buUmaColuna()) gfx_sem_recorte();
    }
  }
}

// Ilha do teclado: teclas de 62 (raio 18), a focada na pilula do acento.
static void desenhaTeclado(void) {
  if (buTecladoNativo()) return;
  char rotulo[8];
  int linhas = kbFil + 1;
  GfxRect ilha = { BU_KB_X, BU_KB_Y, BU_COL_W,
                   2.0f * BU_KB_PAD + linhas * BU_TECLA_W + (linhas - 1) * BU_TECLA_GAP };
  ajustes_ui_ilha(ilha, 32.0f, 0);
  for (int f = 0; f <= kbFil; f++) {
    for (int c = 0; c < KB_COLUNAS[f]; c++) {
      float k = animTecla[f][c];
      GfxRect t = retanguloTecla(f, c);
      ponteiro_alvo(t.x, t.y, t.w, t.h, ponteiroTecla, NULL, f, c);
      buNeutro(t, BU_TECLA_RAIO, 0.07f, 1.0f - k);
      buPilulaAcento(t, BU_TECLA_RAIO, k);
      const char *s;
      if (f < kbFil) {
        snprintf(rotulo, sizeof rotulo, "%s", kbTeclas[f * BU_KB_COLS + c]);
        s = rotulo;
      } else s = (c == 0) ? i18n("espaço") : (c == 1 ? i18n("apagar")
               : (c == 2 ? i18n("limpar") : c == 3 ? (pagSim ? "abc" : "?!#")
               : (layoutCir ? "ABC" : "\xd0\x90\xd0\x91\xd0\x92")));
      // The tone eases every frame while focus moves: tint the cached white
      // line at draw time instead of rasterizing one line per tone.
      float tom = anim_mistura(243.0f, (float)ajustes_tinta_foco(), k) / 255.0f;
      TxtLinha l = f < kbFil ? txt_linha(TXT_G26B, s, 255, 255, 255, 255)
                             : txt_linha_corta(TXT_AJ_SEG, s, 255, 255, 255, 255, t.w - 8.0f);
      txt_desenhar_tom(l, t.x + (t.w - l.w) * 0.5f, t.y + (t.h - l.h) * 0.5f, tom, tom, tom, 1.0f);
    }
  }
  // Dicas do controle, 16 px a 16 abaixo da ilha (o texto mudo do mockup).
  { float y = ilha.y + ilha.h + 16.0f;
    const char *d1 = campoFoco == 1 ? i18n("OK   Teclado da TV")
                   : campoFoco == 2 ? i18n("OK   Falar")
                   : campoFoco == 3 ? i18n("OK   Digitar pelo celular")
                   : temResultados() ? i18n("→   Resultados")
                   : recentesVisiveis() ? i18n("→   Buscas recentes")
                   : nFil ? i18n("→   Populares")
                   : st_ime_disponivel() ? i18n("↑   Campo de busca")
                   : celb_disponivel() ? i18n("↑   Digitar pelo celular")
                   : i18n("OK   Digitar");
    const char *d2 = i18n("Voltar   Menu");
    TxtLinha a1 = txt_linha(TXT_ILHA_APOIO, d1, BU_TX);
    TxtLinha a2 = txt_linha(TXT_ILHA_APOIO, d2, BU_TX);
    txt_desenhar_alpha(a1, BU_KB_X + 12.0f, y, 0.42f);
    txt_desenhar_alpha(a2, BU_KB_X + 12.0f, y + a1.h + 6.0f, 0.42f); }
}

static void desenhaVazio(void) {
  const char *t1 = nConsulta >= 2 ? "Nenhum título recebido" : "O que vamos assistir?";
  const char *t2 = nConsulta >= 2 ? "Os resultados dos addons aparecem aqui."
                             : "Digite ao menos 2 letras de um filme ou série.";
  float larg = fmaxf(1.0f, BU_DIR - BU_RES_X);
  TxtLinha l1 = buTelefone() ? txt_linha_corta(TXT_ILHA_TITULO, t1, BU_TX, larg)
                            : txt_linha(TXT_ILHA_TITULO, t1, BU_TX);
  TxtLinha l2 = buTelefone() ? txt_linha_corta(TXT_ILHA_CORPO, t2, BU_TX, larg)
                            : txt_linha(TXT_ILHA_CORPO, t2, BU_TX);
  float cx = BU_RES_X + (BU_DIR - BU_RES_X) * 0.5f;
  float y = BU_RES_Y + 220.0f;
  txt_desenhar(l1, cx - l1.w * 0.5f, y);
  txt_desenhar_alpha(l2, cx - l2.w * 0.5f, y + l1.h + 14.0f, 0.55f);
  if (nConsulta >= 2) {
    TxtLinha ajuda = buTelefone() ? txt_linha_corta(TXT_ILHA_GENERO,
        "Se não aparecerem, confira a conexão ou tente outro nome.", BU_TX, larg)
        : txt_linha(TXT_ILHA_GENERO,
        "Se não aparecerem, confira a conexão ou tente outro nome.", BU_TX);
    txt_desenhar_alpha(ajuda, cx - ajuda.w * 0.5f, y + l1.h + l2.h + 36.0f, 0.42f);
  }
}

// Buscas recentes: chips de 56 (o .chip do mockup), em linhas que quebram na
// borda; foco = pilula no acento; o filete enche enquanto o OK segura.
static void desenhaRecentes(Uint32 agora, float dy) {
  int n = buscasrec_n(), i, lin = 0;
  float x = BU_RES_X, y = BU_REC_Y + dy, maxW = BU_DIR - BU_RES_X;
  const char *limpar = i18n("Limpar");
#ifdef NV_TOUCH_UI
  if (buTelefone()) {
    TxtLinha t = txt_linha_corta(TXT_MINI, i18n("Buscas recentes"), BU_TX, maxW);
    txt_desenhar_alpha(t, BU_RES_X, BU_RES_Y + dy, 0.45f);
  } else
#endif
  ajustes_ui_kicker(i18n("Buscas recentes"), BU_RES_X, BU_RES_Y + dy, 1.0f);
  nRecLayout = 0;
  for (i = 0; i <= n && i < BU_REC_ITENS; i++) {
    float f = animRec[i];
    int tinta = f > 0.5f ? ajustes_tinta_foco() : 235;
    float w;
    TxtLinha l = i < n
      ? txt_linha_corta(TXT_AJ_SEG, buscasrec_termo(i), tinta, tinta, tinta, 255, maxW - 2.0f * BU_REC_PADX)
      : buTelefone() ? txt_linha_corta(TXT_AJ_SEG, limpar, tinta, tinta, tinta, 255, maxW - 2.0f * BU_REC_PADX)
                     : txt_linha(TXT_AJ_SEG, limpar, tinta, tinta, tinta, 255);
    w = (float)l.w + 2.0f * BU_REC_PADX;
    if (buTelefone()) w = fminf(w, maxW);
    if (x > BU_RES_X && x + w > BU_DIR) { x = BU_RES_X; y += BU_REC_LINHA; lin++; }
    recRect[i] = (GfxRect){ x, y, w, BU_REC_H };
    recLin[i] = lin;
    nRecLayout = i + 1;
#ifdef NV_TOUCH_UI
    if (buTelefone()) buAlvoResultado(recRect[i], ponteiroRecente, i, 0);
#endif
    buNeutro(recRect[i], 28.0f, 0.08f, 1.0f - f);
    buPilulaAcento(recRect[i], 28.0f, f);
    txt_desenhar_alpha(l, x + BU_REC_PADX, y + (BU_REC_H - l.h) * 0.5f, i < n || f > 0.5f ? 1.0f : 0.85f);
    if (painel == 2 && i == focoRec && okPress && !okLongo) {
      float p = anim_clamp((agora - okDesde) / (float)NV_HOLD_MS, 0.0f, 1.0f);
      int t = ajustes_tinta_foco();
      GfxRect barra = { x + BU_REC_PADX, y + BU_REC_H - 10.0f, (w - 2.0f * BU_REC_PADX) * p, 4.0f };
      if (barra.w > 1.0f) gfx_cor(barra, 0.5f, t / 255.0f, t / 255.0f, t / 255.0f, 0.9f);
    }
    x += w + BU_REC_GAP;
  }
  {
    TxtLinha d = buTelefone() ? txt_linha_corta(TXT_ILHA_APOIO, i18n("OK   Buscar de novo      Segure OK   Remover"), BU_TX, maxW)
                             : txt_linha(TXT_ILHA_APOIO, i18n("OK   Buscar de novo      Segure OK   Remover"), BU_TX);
    txt_desenhar_alpha(d, BU_RES_X, y + BU_REC_H + 22.0f, 0.42f);
    // Onde as sugestoes comecam (relativo ao topo da coluna, sem a rolagem).
    sugDesloc = (y - dy) + BU_REC_H + 22.0f + d.h + BU_SECAO_GAP - BU_RES_Y; }
}

static void desenhaResultados(Uint32 agora) {
  float varreFoco = revela_varre(&revVarre, painel == 1
                                 ? focoRes.fileira * 64 + focoRes.coluna : -1, agora);
  int movel = 0;
  temItemFoco = 0;
#ifdef NV_TOUCH_UI
  // Mobile: drawing, gesture and targets share the same results window.
  movel = buTelefone();
  if (movel) {
    gfx_recorte(BU_RES_X, BU_RES_Y - 20.0f, BU_DIR - BU_RES_X, BU_RES_AREA_H + 20.0f);
    scrollY = anim_clamp(scrollY, 0.0f, toqueBuscaMaxY());
    scrollAlvo = anim_clamp(scrollAlvo, 0.0f, toqueBuscaMaxY());
    for (int r = 0; r < nFil; r++) scrollX[r] = anim_clamp(scrollX[r], 0.0f, toqueBuscaMaxX(r));
  }
#endif
  if (nFil == 0 && recentesVisiveis()) {
    sugDesloc = 0.0f;
    desenhaRecentes(agora, movel ? -scrollY : 0.0f);
    if (movel) gfx_sem_recorte();
    return;
  }
  nRecLayout = 0;
  if (nFil == 0) {
    desenhaVazio();
    if (movel) gfx_sem_recorte();
    return;
  }

  if (!movel)
    gfx_recorte(BU_RES_X - 30.0f, BU_RES_Y - 20.0f,
                (BU_DIR - BU_RES_X) + 60.0f, BU_RES_AREA_H + 40.0f);
  // CAMPO VAZIO: as pilulas das buscas recentes em cima e os Populares logo
  // abaixo, na mesma coluna e na mesma rolagem.
  if (sugestao && recentesVisiveis()) desenhaRecentes(agora, -scrollY);
  else sugDesloc = 0.0f;
  const float grupo = gfx_opacidade_grupo;
  const float larg = BU_DIR - BU_RES_X;
  int ordemFil = 0;
  for (int r = 0; r < nFil; r++) {
    float ry = BU_RES_Y + filTopo(r) - scrollY;
    if (ry > NV_TELA_H + 100.0f || ry + filAlt(r) < -100.0f) {
      filNova[r] = 0; filEntraEm[r] = 0;   // assenta fora da tela
      continue;
    }
    if (filNova[r]) {
      filEntraEm[r] = anim_politica_reduzida ? 0u
                    : (agora ? agora : 1u) + (Uint32)(ordemFil * NV_ENTRA_FIL_MS);
      filNova[r] = 0;
    }
    if (filEntraEm[r]) ordemFil++;
    if (filEntraEm[r] && revela_onda_fim(filEntraEm[r], agora)) filEntraEm[r] = 0;
    gfx_opacidade_grupo = grupo * revela_entra(filEntraEm[r], 0.0f, agora);

    // Rotulo da entrada: "Melhor resultado" ou o nome do catalogo (+ origem).
    buCabecalho(r, ry + ANC(4.0f), larg);
    gfx_opacidade_grupo = grupo;
    float cy = ry + filKickH(r);

    if (fil[r].pessoas) {
      // PESSOAS (mockup): avatar redondo de 64, nome 18 a 14 ao lado, vao 26.
      // O foco e a pilula de superficie mais clara em volta do conjunto.
      float x = 0.0f;
      for (int c = 0; c < fil[r].n && c < BU_MAX_PESS; c++) {
        int q = fil[r].itens[c];
        float f = animRes[r][c];
        float entra = filEntraEm[r] ? revela_entra(filEntraEm[r], revela_onda_atraso(c, 0), agora) : 1.0f;
        TxtLinha nm = txt_linha_corta(buTecladoNativo() ? TXT_ILHA_NOME : TXT_ILHA_META,
                                     pess[q].nome, BU_TX, BU_PESS_NOMEMAX);
        float w = BU_PESS_AV + 14.0f + (float)nm.w;
        float px = BU_RES_X + x - scrollX[r];
        GfxRect av = { px, cy + (1.0f - entra) * NV_ENTRA_DY, BU_PESS_AV, BU_PESS_AV };
        pessX[c] = x; pessW[c] = w;
        x += w + BU_PESS_GAP;
        if (px > BU_DIR + 30.0f || px + w < BU_RES_X - 30.0f) continue;
        buAlvoResultado((GfxRect){av.x - 12.0f, cy - 8.0f, w + 30.0f, BU_PESS_AV + 16.0f},
                        ponteiroResultado, r, c);
        gfx_opacidade_grupo = grupo * entra;
        if (f > 0.01f) {
          GfxRect pil = { av.x - 12.0f, av.y - 8.0f, w + 30.0f, BU_PESS_AV + 16.0f };
          gfx_opacidade_grupo = grupo * entra * f;
          ajustes_ui_foco_linha(pil, pil.h * 0.5f);
          gfx_opacidade_grupo = grupo * entra;
        }
        if (pess[q].foto[0] && !tex_falhou(pess[q].foto)) buArte(av, pess[q].foto, BU_PESS_AV * 0.5f, 1.0f);
        else {
          // Sem foto no TMDB: o disco com o icone de pessoa, nao um buraco.
          gfx_cor(av, 0.5f, 0.20f, 0.205f, 0.225f, 1.0f);
          gfx_icone((GfxRect){ av.x + av.w * 0.25f, av.y + av.h * 0.25f, av.w * 0.5f, av.h * 0.5f },
                    "aj_user-round", 0.78f, 0.79f, 0.82f, 1.0f);
        }
        txt_desenhar_alpha(nm, av.x + BU_PESS_AV + 14.0f, av.y + (BU_PESS_AV - nm.h) * 0.5f,
                           anim_mistura(0.75f, 1.0f, f));
      }
      gfx_opacidade_grupo = grupo;
      continue;
    }

    if (fil[r].melhor) {
      // O MELHOR RESULTADO: tile de 226 (ilha), capa 330x186, titulo 40/700.
      const CatItem *ci = cat_item(fil[r].itens[0]);
      float f = animRes[r][0];
      float entra = filEntraEm[r] ? revela_entra(filEntraEm[r], 0.0f, agora) : 1.0f;
      if (!ci) continue;
      GfxRect t = { BU_RES_X, cy + (1.0f - entra) * NV_ENTRA_DY, larg, BU_MELHOR_H };
      buAlvoResultado((GfxRect){BU_RES_X, cy, larg, BU_MELHOR_H}, ponteiroResultado, r, 0);
      gfx_opacidade_grupo = grupo * entra;
      ajustes_ui_ilha(t, 26.0f, 0);
      if (f > 0.01f) {
        gfx_opacidade_grupo = grupo * entra * f;
        ajustes_ui_foco_linha(t, 26.0f);
        gfx_opacidade_grupo = grupo * entra;
      }
      { const char *url = ci->backdrop[0] ? ci->backdrop : ci->poster;
        GfxRect art = { t.x + 20.0f, t.y + 20.0f, 330.0f * gfx_poster_k(), 186.0f * gfx_poster_k() };
        float tx, ty, bloco;
        if (!ci->backdrop[0]) art.w = art.h * 2.0f / 3.0f;
        if (buTelefone()) art.w = fminf(art.w, fmaxf(1.0f, (t.w - 68.0f) * 0.45f));
        buArte(art, posterprov_card_formato(ci->origem, ci->imdb, ci->tmdb, ci->tipo, url, ci->backdrop[0] != 0),
               14.0f, 1.0f);
        tx = art.x + art.w + 28.0f;
        { float tw = t.x + t.w - tx - 26.0f;
          TxtLinha nome = txt_linha_corta(TXT_ILHA_TITULO, ci->titulo, BU_TX, tw);
          TxtLinha meta = txt_linha_corta(TXT_ILHA_META, ci->meta, BU_TX, tw);
          TxtLinha gen = { 0, 0, 0 };
          if (ci->genero[0]) gen = txt_linha_corta(TXT_ILHA_GENERO, ci->genero, BU_TX, tw);
          bloco = nome.h + 6.0f + (ci->meta[0] ? meta.h + 6.0f : 0.0f) + gen.h;
          ty = t.y + (t.h - bloco) * 0.5f;
          txt_desenhar(nome, tx, ty);
          ty += nome.h + 6.0f;
          if (ci->meta[0]) { txt_desenhar_alpha(meta, tx, ty, 0.62f); ty += meta.h + 6.0f; }
          if (gen.h) txt_desenhar_alpha(gen, tx, ty, 0.42f); }
        if (painel == 1 && focus_indice(&focoRes, r, 0)) {
          itemFoco.indice = fil[r].itens[0];
          itemFoco.rect   = art;
          itemFoco.arte   = ci->backdrop[0] ? ci->backdrop : ci->poster;
          itemFoco.titulo = ci->titulo;
          itemFoco.genero = ci->genero;
          itemFoco.meta   = ci->meta;
          temItemFoco = 1;
        } }
      gfx_opacidade_grupo = grupo;
      continue;
    }

    // TITULOS: cartazes 180x270 (raio 14), nome 17/400 a 10 abaixo. O foco
    // levanta o cartaz com sombra, sem contorno.
    { float passo = buPasso(), cw = buCartazW(), ch = cw * 1.5f;
      int c0 = (int)(scrollX[r] / passo);
      for (int passe = 0; passe < 2; passe++)
        for (int c = 0; c < fil[r].n && c < BU_MAX_POR_FIL; c++) {
          float f = animRes[r][c];
          if ((passe == 1) != (f > 0.01f)) continue;
          const CatItem *ci = cat_item(fil[r].itens[c]);
          if (!ci) continue;
          float px = BU_RES_X + c * passo - scrollX[r];
          if (px > BU_DIR + 30.0f || px + cw < BU_RES_X - passo) continue;
          if (passe == 0 || f > 0.01f) {
            if (buTelefone()) buAlvoResultado((GfxRect){px, cy - 6.0f, cw, ch + 40.0f}, ponteiroResultado, r, c);
            else {
              float ax = px < BU_RES_X - 30.0f ? BU_RES_X - 30.0f : px;
              float aw = (px + cw > BU_DIR + 30.0f ? BU_DIR + 30.0f : px + cw) - ax;
              if (aw > 0.0f) ponteiro_alvo_faixa(ax, cy - 6.0f, aw, ch + 40.0f, BU_RES_Y - 20.0f,
                                                 BU_RES_Y + BU_RES_AREA_H + 20.0f, ponteiroResultado, NULL, r, c);
            }
          }
          float entra = filEntraEm[r]
                      ? revela_entra(filEntraEm[r], revela_onda_atraso(c - c0, 0), agora) : 1.0f;
          float esc = 1.0f + 0.05f * f;
          GfxRect p = { px - cw * (esc - 1.0f) * 0.5f, cy - ch * (esc - 1.0f) * 0.5f - 6.0f * f
                          + (1.0f - entra) * NV_ENTRA_DY, cw * esc, ch * esc };
          float raio = 14.0f / p.h;
          gfx_opacidade_grupo = grupo * entra;
          if (f > 0.01f)
            gfx_rect((GfxRect){ p.x - 16, p.y - 4, p.w + 32, p.h + 40 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f,
                     0, 0, 0, 0.50f * f);
          { const char *arte = posterprov_card_addon(ci->origem, ci->imdb, ci->tmdb, ci->tipo, ci->poster);
            if (!arte[0]) arte = ci->backdrop[0] ? ci->backdrop : NULL;
            GLuint tex = arte ? tex_obter_larg(arte, p.w) : 0;
            float aArte = revela_arte(&revRes[r][c], tex != 0, agora);
            if (tex) {
              if (aArte < 0.999f)
                gfx_cor(p, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, 1.0f);
              gfx_tex_aspect_atual = tex_aspecto(arte);
              if (painel == 1 && focoRes.fileira == r && focoRes.coluna == c)
                gfx_varre_atual = varreFoco;
              gfx_rect(p, tex, GFX_CARD, 0.0f, 0.0f, 0.0f, raio, 0, 0, 0, aArte);
              gfx_varre_atual = 0.0f;
              gfx_tex_aspect_atual = 0.0f;
            } else if (arte && !tex_falhou(arte))
              gfx_esqueleto(p, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, 1.0f);
            else
              gfx_cor(p, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, 1.0f); }
          { TxtLinha tn = txt_linha_corta(buTecladoNativo() ? TXT_ILHA_NOME : TXT_ILHA_GENERO,
                                         ci->titulo, BU_TX, cw);
            txt_desenhar_alpha(tn, p.x, p.y + p.h + 10.0f, anim_mistura(0.62f, 1.0f, f)); }
          if (painel == 1 && focus_indice(&focoRes, r, c)) {
            itemFoco.indice = fil[r].itens[c];
            itemFoco.rect   = p;
            itemFoco.arte   = ci->backdrop[0] ? ci->backdrop : ci->poster;
            itemFoco.titulo = ci->titulo;
            itemFoco.genero = ci->genero;
            itemFoco.meta   = ci->meta;
            temItemFoco = 1;
          }
        }
    }
    gfx_opacidade_grupo = grupo;
  }
  gfx_opacidade_grupo = grupo;
  gfx_sem_recorte();
}

static void busca_desenhar_(Uint32 agora) {
  buInterfaceAtualizar();
#ifdef NV_TOUCH_UI
  if (buTelefone()) ponteiro_rolagem(toqueBuscaRolar);
#endif
  ajustes_ui_fundo();
  // O .fundo da pagina no mockup (preto 70% em cima, 92% a 60%): os resultados
  // e o teclado leem sobre a arte sem competir com ela.
  { GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    gfx_cor(tela, 0, 0, 0, 0, 0.45f);
    gfx_veu_css(tela, 0, 1.0f, 1.0f, 0.60f); }
  desenhaCampo(agora);
  desenhaTeclado();
  desenhaResultados(agora);
}

void busca_evento(const SDL_Event *e) { TXT_INI(); busca_evento_(e); TXT_FIM(); }
void busca_atualizar(float dt, Uint32 agora) { TXT_INI(); busca_atualizar_(dt, agora); TXT_FIM(); }
void busca_desenhar(Uint32 agora) { TXT_INI(); busca_desenhar_(agora); TXT_FIM(); }

#ifdef NV_BUSCA_TESTE
// Gancho de teste (tests/busca_debounce.sh): soma dos itens das fileiras de titulos.
int busca_teste_itens(void) {
  int i, t = 0;
  for (i = 0; i < nFil; i++) if (!fil[i].pessoas) t += fil[i].n;
  return t;
}
#endif
