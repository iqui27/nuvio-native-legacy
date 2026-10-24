// Guia de TV — ver guia.h para a forma geral.
//
// DE ONDE VEM O QUE:
//   - canais e categorias: o catalogo do addon de canal (FrostView), lido por
//     inteiro num fio proprio (a home so carrega 12 por fileira; o guia
//     precisa dos ~768 para agrupar por `genre`). Paginacao Stremio padrao:
//     /catalog/<tipo>/<id>.json depois /skip=100,200,...
//   - o que esta passando: epg.c (XMLTV do epgshare01, cache de 12 h).
//   - favoritos: guia-fav-p<N>.txt, um id por linha, privado ao perfil. O arquivo
//     proprio existe porque SalvoItem.id tem 24 bytes e os ids do FrostView
//     tem ~45 — eles nao cabem no sistema de "salvos" comum.
//
//   - modo de exibicao: guia-modo-p<N>.txt na pasta de dados ("lista" ou
//     "cartoes"). Arquivo proprio pelo mesmo motivo do de favoritos: e uma
//     preferencia DESTE guia, e o sistema de ajustes nao e meu para editar.
//   - addons recomendados: <pasta art>/addons-recomendados.txt, mantido a mao
//     pelo dono. Ver a nota em recLer().
//
// DOIS MODOS DE EXIBICAO, um estado de foco so:
//   - CARTOES: fileiras por categoria com cartoes horizontais (o original).
//   - LISTA: guia tradicional — uma linha por canal, coluna de nome a esquerda
//     e uma faixa de tempo a direita com os blocos de programa na proporcao
//     da duracao, regua de meias horas em cima e a linha "agora" na cor de
//     realce. Nos dois modos o HEROI em cima (ficha + preview 16:9) mostra o
//     canal focado — ver o bloco de layout.
//   focoLin/focoCol sao os mesmos nos dois; alternar preserva o canal focado.
//
// COMO SE ALTERNA, e por que assim:
//   - controle segmentado no CABECALHO (Cartoes | Lista | Addons), alcancado
//     com CIMA a partir da primeira linha. E o caminho primario porque e
//     VISIVEL: tecla que ninguem ve ninguem descobre.
//   - botao AMARELO da LG como atalho. Era o unico colorido livre: o AZUL
//     abre e fecha o overlay, CH+/- pulam secao, e no Tizen a casca ja gasta
//     vermelho e verde em outras telas (tizen-shell.html). O scancode 488 e
//     SUPOSTO pela sequencia do SDL_webOS.h (RED..BLUE contiguos, BLUE=489
//     medido); nao havia TV nesta bancada para confirmar. No Tizen a amarela
//     NAO e repassada pela casca, entao la so o cabecalho alterna.
//
// CONCORRENCIA: o fio escreve em s* (staging); quando termina sobe
// `pendPronto` e o fio de desenho copia para os vetores publicados. Leitores
// nunca tocam no staging — mesma disciplina do epg.c.
#include "horafmt.h"
#include "guia.h"
#include "idcanal.h"
#include "registro.h"
#include "guiaaddons.h"
#include "guiafiltro.h"
#include "plrui.h"
#include "badges.h"   /* marcas de resolucao no heroi */
#include "aovivo.h"
#include "fontecache.h"
#include "ajustes.h"   /* ajustes_acento: cor do anel de foco */
#include "ponteiro.h"
#include "rolagemtoque.h"
#include "telefoneui.h"
#include "epg.h"
#include "rede.h"
#include "addons.h"
#include "sync.h"        /* sync_iniciar: o ciclo que traz a conta para esta TV */
#include "descoberta.h"  /* desc_repetir: addon novo so entra com ciclo novo */
#include "stalker.h"
#include "xtream.h"
#include "m3u.h"
#include "iptvfontes.h"
#include "iptvid.h"
#if __has_include("backupcategories.h")
#include "backupcategories.h"
#define NV_GUIA_BACKUP_CATEGORIAS 1
#endif
#include "guiacorte.h"   /* tetos e o aviso "Mostrando N de M" */
#include "livecat.h"   /* secao sugerida pelo Jev para canal sem categoria */
#include "xtepg.h"     /* grade curta por canal do Xtream (#158) */
#include "dados.h"
#include "perfis.h"   /* perfis_ativo: o cache do guia e por perfil */
#include "perfisessao.h"
#include "marco.h"
#include "js.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "anim.h"
#include "layout.h"
#include "segurar.h"
#include "idioma.h"
#include "video.h"      /* preview do canal focado no canto do guia */
#include "player.h"     /* o preview e uma sessao "mini no guia" do player */
#include "lembrete.h"   /* lembrete de programa futuro */
#include "guialembrete.h"
#include "streams.h"    /* Stream: url do preview vinda do fio */
#include "livetv_regras.h" /* variantes FHD/HD/SD do mesmo canal */
#include "teclado.h"       /* a busca do guia */
#include "buscanorm.h"     /* guia_buscar_canais (Spotlight) */
#include "ltv.h"           /* modelo por fonte: arena, teto, numeracao, M3U */
#include "perfiltv.h"      /* ptv_plataforma: o teto de canais da TV */
#include <stdio.h>
#include <limits.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>


// --- layout ---------------------------------------------------------------
// DESDE 25/09/2026 O GUIA E UM HEROI "AGORA" EM CIMA E A GRADE EMBAIXO, na
// largura inteira — a forma dos guias de referencia (YouTube TV, Google TV,
// Samsung TV Plus, TiviMate). Pedido do dono: "aumentar o video do guide,
// igual guides originais". Antes o preview era um retangulo de 320x180 numa
// coluna de 400 px a direita, e a grade perdia essa coluna inteira.
//
// A 1920x1080:
//   cabecalho  titulo/relogio, quatro grupos de controles, contagem
//   heroi      y 108..558  ficha do canal (x 80..992) + preview 800x450 16:9
//                          encostado na margem direita (x 1040..1840)
//   grade      y 580..1018 regua de meia hora + linhas de canal
//   ajuda      y 1032      barra de teclas
// O preview fica a DIREITA porque a ficha e lida primeiro (esquerda para a
// direita) e o video, que se mexe, nao precisa de leitura: e o olho que vai
// ate ele. Com o preview a esquerda o titulo do programa caia no meio da
// tela, longe da coluna de canais da grade, que e onde a pessoa esta.
#define G_AREA_X   NV_MARGEM_X
#define G_AREA_W   (NV_TELA_W - 2.0f * NV_MARGEM_X)
#define G_AREA_DIR (G_AREA_X + G_AREA_W)
static int gRetrato(void) {
#ifdef NV_TOUCH_UI
  return telefoneui_ativo() && NV_TELA_W < NV_TELA_H;
#else
  return 0;
#endif
}
static float gTopoFim = 80.0f;
static void gTopoMedir(void);
static float gPreviewLargura(void);
#define G_HERO_Y   (gRetrato() ? gTopoFim + 60.0f : 204.0f)
// PREVIEW: 800x450 e 41,7% da largura, 16:9 exato. Os MESMOS numeros furam a
// superficie GL (desenharHero) e posicionam o plano de video (a sessao "mini
// no guia" do player recebe estes numeros por guia_preview_rect) — um lugar
// so, senao o furo e o video desencontram.
#define G_PREVIEW_W gPreviewLargura()
#define G_PREVIEW_H (G_PREVIEW_W * (9.0f / 16.0f))
#define G_PREVIEW_X (gRetrato() ? G_AREA_X + (G_AREA_W - G_PREVIEW_W) * 0.5f : NV_TELA_W - G_PREVIEW_W)
#define G_INFO_H    450.0f
#define G_PREVIEW_Y (gRetrato() ? G_HERO_Y + G_INFO_H + 24.0f : 0.0f)
#define G_PREVIEW_RAIO (gRetrato() ? 16.0f : 0.0f)          // px; vira fracao do menor lado no uso
// Ficha do canal a esquerda do preview, com 48 de respiro ate ele.
#define G_INFO_X   G_AREA_X
#define G_INFO_W   (gRetrato() ? G_AREA_W : G_PREVIEW_X - 48.0f - G_INFO_X)
// Topo da grade (regua do modo lista, primeira fileira do modo cartoes).
#define G_TOPO     (G_PREVIEW_Y + G_PREVIEW_H + 22.0f)
#define G_CARD_W   330.0f
#define G_CARD_H   226.0f
#define G_GAP_X     20.0f
#define G_HEAD_H    48.0f
#define G_PASSO_Y  (G_HEAD_H + G_CARD_H + 36.0f)
// A coluna de categorias do modo "salta secao": largura da faixa que sobe na
// esquerda enquanto o modo esta armado.
#define G_CAT_W    440.0f     // gaveta de categorias
#define G_CAT_ROW   60.0f
#define G_CAT_TOPO 150.0f
#define G_CAT_BASE (NV_TELA_H - 96.0f)
// webOS may send held keys as separate KEYDOWNs with repeat=0.
// Reuse the 450 ms gap to distinguish those repeats from fresh presses.
#define G_CAT_PASSO_MS 550
#define G_REP_MS     450

// Botao AMARELO do controle da LG. SUPOSTO (ver o cabecalho do arquivo):
// SDL_webOS.h enumera RED, GREEN, YELLOW, BLUE em sequencia e BLUE e 489.
#define G_SCANCODE_YELLOW 488
#define G_SCANCODE_GREEN  487   // vermelho 486, verde 487, amarelo 488, azul 489

// --- layout do MODO LISTA -----------------------------------------------------
// Regua de horas em G_TOPO; as linhas comecam 42 px abaixo dela. Coluna de
// canal com 400 px: numero (48) + logo numa caixa de 84x44 + nome em
// TXT_CW_TITULO (28 px). O resto, 1352 px, e a faixa de tempo.
#define G_L_TOPO    (G_TOPO + 42.0f)
// Linha de 80 (celula de 72 + 8 de vao): titulo de 28 e horario de 23 em
// duas linhas cabem com folga, e sobram CINCO canais visiveis abaixo do
// heroi — o numero dos guias de referencia com preview grande.
#define G_L_ROW      (gRetrato() ? 120.0f : 80.0f)
#define G_L_CEL      (G_L_ROW - 8.0f)
#define G_L_HEAD     44.0f     // cabecalho de categoria, discreto (22 px)
#define G_L_COL     400.0f
#define G_L_FAIXA_X (G_AREA_X + G_L_COL + 8.0f)
#define G_L_FAIXA_W (G_AREA_DIR - G_L_FAIXA_X)
// Janela de 120 min: a 1352 px isso da 11,3 px por minuto — meia hora mede
// 338 px, e um bloco de 5 min (o menor de grade de TV aberta) ainda tem 56.
#define G_L_JANELA_MIN (gRetrato() ? 60 : 120)
#define G_L_PASSO_MIN   30     // regua de meia em meia hora, como toda grade
#define G_L_DESL_MAX   180     // ate 3 h a frente com DIREITA
#define G_L_BASE    (NV_TELA_H - (gRetrato() ? 62.0f : 0.0f))
#define G_L_FADE     36.0f     // esmaecimento nas bordas da area rolavel

// --- cabecalho: chips de modo, addons e preview -----------------------------
// CHIPS e nao pilulas: 40 px de altura e 22 px de fonte, contra os 48/25 de
// antes. O dono (21/09): "os botoes estao muito grandes perto do resto". A
// linha do cabecalho e navegacao secundaria; o que pesa na tela e o heroi.
#define G_TOPO_Y     32.0f
#define G_TOPO_H     (gRetrato() ? 72.0f : 48.0f)
#define G_CHIPS_Y    (G_TOPO_Y + G_TOPO_H + 16.0f)
#define G_CHIP_PAD   20.0f
#define G_CHIP_TEXTO (gRetrato() ? TXT_V2_SEG : TXT_PG_ROTULO)
static float gPreviewLargura(void) {
  if (!gRetrato()) return 992.0f;   // 558 px high: same grid position, art reaches top/right.
  // A regua, uma categoria e tres canais continuam visiveis mesmo quando
  // os controles traduzidos ocupam mais linhas. O video conserva 16:9.
  float h = G_L_BASE - (G_HERO_Y + G_INFO_H + 24.0f + 22.0f + 42.0f) - G_L_HEAD - 3.0f * G_L_ROW;
  return fminf(G_AREA_W, fmaxf(112.5f, h) * (16.0f / 9.0f));
}
// CATEGORIAS e o primeiro chip: a porta VISIVEL do painel de categorias,
// que antes so abria segurando a seta (ninguem descobre gesto que nao se ve).
enum { G_TOPO_BUSCAR = 0, G_TOPO_CATEGORIAS, G_TOPO_CARTOES, G_TOPO_LISTA, G_TOPO_OPCOES, G_TOPO_ADDONS, G_TOPO_FILTROS, G_TOPO_DIAG,
       G_TOPO_LIMPAR, G_TOPO_PREVIEW, G_TOPO_N };
#define G_TOPO_PRIMARIOS (G_TOPO_OPCOES + 1)

#ifdef NV_TOUCH_UI
static GfxRect gTopoRects[G_TOPO_N];
/* Chips share their visual and tap rectangles. Keep the display-mode pair
   together while the remaining controls wrap inside the portrait gutters. */
static void gTopoDistribuir(const float *larguras) {
  float x = G_AREA_X, y = G_CHIPS_Y;
  for (int i = 0; i < G_TOPO_PRIMARIOS; i++) {
    float w = fminf(larguras[i], G_AREA_W * 0.5f);
    float grupo = w;
    if (i == G_TOPO_CARTOES)
      grupo += fminf(larguras[G_TOPO_LISTA], G_AREA_W * 0.5f);
    if (i != G_TOPO_LISTA && x > G_AREA_X && x + grupo > G_AREA_DIR) {
      x = G_AREA_X;
      y += G_TOPO_H + 12.0f;
    }
    gTopoRects[i] = (GfxRect){x, y, w, G_TOPO_H};
    x += w + (i == G_TOPO_CARTOES ? 0.0f : 12.0f);
  }
  gTopoFim = y + G_TOPO_H;
}
#endif

// --- painel de addons ---------------------------------------------------------
#define G_PA_W      720.0f
#define G_PA_X      (NV_TELA_W - G_PA_W)
#define G_PA_MARG    48.0f
#define G_PA_ROW     (telefoneui_ativo() ? 152.0f : 92.0f)
// Sugestao tem descricao de DUAS linhas (a de uma linha cortava toda frase
// em "…", foto do dono em 19/09): a linha e mais alta.
#define G_PA_ROW_REC 118.0f
#define G_MAX_REC    12


// --- O MODELO POR FONTE (plano do guia de TV, M1/M2) ----------------------------
//
// ANTES: tres vetores fixos de 900 GCanal de ~2,4 KB (char[600] por campo) e 48
// categorias fundidas por nome entre todas as fontes, em ordem A-Z. Xtream de
// 16 mil canais virava 900 sem aviso, e o canal da 49a categoria sumia.
//
// AGORA:
//   - GCanal tem PONTEIROS para uma arena de strings (ltv.h) e a FONTE dele.
//     30 mil canais custam ~9 MB, nao 72.
//   - FONTE (GFonteLtv) = addon (por base), Stalker, Xtream, M3U por URL. Tem
//     chave estavel, saude, contas, os grupos TODOS (mesmo os que nao couberam)
//     e o cache proprio em disco: uma fonte que falha nao apaga as outras.
//   - GRUPO (GCat) e da fonte: "Esportes" do addon e do Xtream sao dois grupos
//     (decisao 2 do dono). Ordem do provedor, A-Z opcional por fonte (decisao 3).
//   - TETO por plataforma (ltv_teto_canais) somando as fontes; passou, entram
//     grupos inteiros na ordem do provedor ou os escolhidos pela pessoa, e a
//     fonte fica marcada `cortada` para a tela dizer (nunca mais em silencio).
#define G_MAX_FAV    400
// Cada addon pode declarar VARIOS catalogos de canal no manifesto.
#define G_MAX_FONTE   24
#define G_MAX_LTV     (IPTV_MAX_CONTAS + G_MAX_FONTE + LTV_URLS_MAX)     // fontes da barra (addons + Stalker + Xtream + M3U)
#define G_PAGINA     100

typedef struct {
  const char *id, *nome, *logo, *desc;
  // DE QUE ADDON ESTE CANAL VEIO (so addon): o player pergunta a fonte so a
  // ele. Aponta para a base da fonte (uma copia por fonte, nao por canal).
  const char *base;
  const char *tipo;
  const char *epgId;    // tvg-id / epg_channel_id
  const char *tvgNome;  // tvg-name (M3U)
  const char *url;      // M3U: a URL do canal (nunca em log)
  const char *ua, *ref; // M3U: cabecalhos do canal
  unsigned long long logoId, logoNome;  // donor lookup keys; provider metadata stays intact
  int  cat;             // indice em cats[]
  int  epg;             // -1 = ainda nao resolvido; -2 = sem grade real
  int  fav;             // espelho do arquivo, para o desenho nao varrer a lista
  int  numero;          // do provedor (tvg-chno, num) ou a posicao na fonte
  short fonte;          // indice em ltvF[]
  short epgMetodo;      // EPG_M_* do casamento
  unsigned char sug;    // sem genero da fonte nem da heuristica: candidato a secao do Jev
  const char *grupoOriginal; // fallback before suggestions; persist this in source cache
} GCanal;

typedef struct {
  const char *nome;
  short fonte;
  int   ordem;          // ordem do provedor dentro da fonte
} GCat;

// Todos os grupos que a fonte ofereceu, inclusive os que nao couberam no teto:
// e a lista do "Escolher grupos".
typedef struct { const char *nome; int n, nInventario; unsigned char entra; } GGrupoInfo;

typedef struct {
  char chave[LTV_CHAVE];
  char nome[64];
  int  tipo;                    // LTV_*
  const char *base;             // addon: a base (arena); "" nos outros
  int  saude;                   // LTV_SAUDE_*
  int  nCanais, nGrupos;        // retidos
  int  total;                   // o que a fonte ofereceu
  int  cortada;                 // passou do teto: so parte entrou
  int  deCache;                 // a rede falhou e a lista e a do disco
  int  respondeu;               // successful fetch, including filtered-empty results
  long long atualizada;         // epoch da lista
  int  erro;                    // XT_* (Xtream) ou 1 = nao respondeu
  int  ngi; GGrupoInfo *gi;     // malloc, nomes na arena da lista
  // EPG (M5): canais resolvidos e com grade, por metodo
  int  epgResolvidos, epgCom, epgPor[EPG_M_MANUAL + 1];
  char urlTvg[1024];            // M3U: url-tvg do cabecalho (nunca em log)
  int  fusoS;                   // deslocamento da grade (M6), das prefs
} GFonteLtv;

// Publicados (so o fio de desenho le).
static GCanal *canais;   static int nCanais, capCanais;
static GCat   *cats;     static int nCats, capCats;
static int    *catIni, *catN;
static GFonteLtv ltvF[G_MAX_LTV]; static int nLtvF;
static LtvArena arena;
static unsigned canaisRevisao;
// Staging (so o fio de carga escreve).
static GCanal *sCanais;  static int sNCanais, sCapCanais;
static GCat   *sCats;    static int sNCats, sCapCats;
static GFonteLtv sLtvF[G_MAX_LTV]; static int sNLtvF;
static LtvArena sArena;
static int sTeto;            // visible channel limit
// Bound the spool even when tmpfile() uses Emscripten's in-memory filesystem.
#define G_RAW_BYTES_MAX (2 * 1024 * 1024)
static size_t sCacheBytes;
#define G_PF_CANAIS_MAX 4000
// Global and per-category limits; category metadata and disk lookup stay available.
#define G_INVENTARIO_POR_CAT 64
#define G_INVENTARIO_MAX 4000
#define G_INVENTARIO_BYTES_MAX (4 * 1024 * 1024)
typedef struct { char id[GF_ID], nome[GF_NOME], cat[GF_NOME], fonte[LTV_CHAVE]; } GInventario;
static GInventario *inventario, *sInventario;
static int nInventario, sNInventario;
// Compact records use source/group indexes instead of repeating their names.
// Circular blocks retain recent controls; overflow pages refetch through the loader.
#define G_INVENTARIO_BLOCO 4096
static FILE *inventarioDisco, *sInventarioDisco;
static int inventarioCortado, sInventarioCortado;
static char fInventarioCat[GF_NOME], sInventarioCat[GF_NOME];
static int fInventarioInicio, sInventarioInicio, sInventarioVistos, sInventarioMais;

static FILE *sCache[G_MAX_LTV];
static int sCacheN[G_MAX_LTV], sCacheErro[G_MAX_LTV], sCacheCortada[G_MAX_LTV];
#define G_MAX_PENDENTES 1200  // One load's suggestion budget.
#define G_CANDIDATOS_MAX 30000 // Bounded input; visible caps apply only in selecionarSecoes().
static int sPendentes;
static size_t sTextoSubstituido;

enum { G_PARADO, G_BAIXANDO, G_PRONTO, G_FALHOU };
static int estado = G_PARADO;
static int fioVivo;
static int recarregarPend;
static GuiaCorte sCorte;       // staging: o que a carga deixou de fora
static GuiaCorte corte;        // publicado: o que o cabecalho mostra
static int sOk;                 // staging: a carga achou canal (antes de pendPronto)
static atomic_int pendPronto;   // release no fio de carga, acquire no desenho
// Foco pedido com a lista ainda baixando; publicar() aplica ao chegar.
static char focoPend[80];

// Catalogos de canal descobertos na home (tipo "channel"/"tv").
typedef struct { char base[NV_ADDON_URL_MAX], tipo[16], id[96], nome[96]; } GFonte;
static GFonte fontes[G_MAX_FONTE]; static int nFontes, fontesOk;
static GFonte sFontes[G_MAX_FONTE]; static int sNFontes;

static void fontesLtvSoltar(GFonteLtv *f, int n) {
  int i;
  for (i = 0; i < n; i++) { free(f[i].gi); f[i].gi = NULL; f[i].ngi = 0; }
}

// --- filtro da Live TV: qualidade, categorias e canais escondidos ----------------
// Por perfil, padrao: tudo aparece, todas as versoes. `fltLive` e o que a tela
// edita; a carga le a COPIA `sFltLive` tirada em iniciarCarga, entao mexer no
// painel no meio de uma recarga nao muda a lista pela metade.
static GuiaFiltro fltLive, sFltLive;
static GuiaFiltroDup sDup;
static int fltPerfil = -1;
static int sFiltrados;
// Outras resolucoes do mesmo canal que o modo "Preferir 4K" escondeu: viram
// fontes de reserva em guia_variantes. So Xtream (ids de resolucao proprios).
typedef struct { const char *dono, *id, *nome; } GAlt;
#define G_ALT_MAX 8192
static GAlt *alts, *sAlts; static int nAlts, sNAlts;

static void fltGarantir(void) {
  int p = perfis_ativo();
  char nome[48], *t;
  if (p == fltPerfil) return;
  fltPerfil = p;
  t = dados_ler(guiafiltro_arquivo(p, nome, sizeof nome));
  guiafiltro_ler(&fltLive, t);
  free(t);
}
static void fltGravar(void) {
  char nome[48], *buf = malloc(sizeof fltLive + 64);
  if (!buf) return;
  if (guiafiltro_texto(&fltLive, buf, sizeof fltLive + 64))
    dados_gravar(guiafiltro_arquivo(fltPerfil, nome, sizeof nome), buf);
  free(buf);
}

// --- staging ---------------------------------------------------------------------
static void stagingZerar(void) {
  for (int i = 0; i < G_MAX_LTV; i++) {
    if (sCache[i]) fclose(sCache[i]);
    sCache[i] = NULL; sCacheN[i] = sCacheErro[i] = sCacheCortada[i] = 0;
  }
  sCacheBytes = 0;
  if (sInventarioDisco) fclose(sInventarioDisco);
  sInventarioDisco = NULL; sInventarioCortado = 0;
  sInventarioVistos = sInventarioMais = 0;
  free(sInventario); sInventario = NULL; sNInventario = 0;
  sFiltrados = 0;
  guiafiltro_dup_zerar(&sDup);
  free(sAlts); sAlts = NULL; sNAlts = 0;
  fontesLtvSoltar(sLtvF, sNLtvF);
  ltv_arena_soltar(&sArena);
  free(sCanais); sCanais = NULL; sNCanais = sCapCanais = 0;
  free(sCats); sCats = NULL; sNCats = sCapCats = 0;
  memset(sLtvF, 0, sizeof sLtvF); sNLtvF = 0;
  sPendentes = 0; sTextoSubstituido = 0;
}

static int sFonteChave(const char *ch, int tipo, const char *nome, const char *base) {
  int i;
  for (i = 0; i < sNLtvF; i++) if (!strcmp(sLtvF[i].chave, ch)) return i;
  if (sNLtvF >= G_MAX_LTV) return -1;
  i = sNLtvF++;
  memset(&sLtvF[i], 0, sizeof sLtvF[i]);
  snprintf(sLtvF[i].chave, sizeof sLtvF[i].chave, "%s", ch);
  snprintf(sLtvF[i].nome, sizeof sLtvF[i].nome, "%s", nome && nome[0] ? nome : ltv_tipo_nome(tipo));
  sLtvF[i].tipo = tipo;
  sLtvF[i].base = ltv_dup(&sArena, base ? base : "");
  sLtvF[i].saude = LTV_SAUDE_OK;
  sLtvF[i].atualizada = (long long)time(NULL);
  return i;
}
static int sFonte(int tipo, const char *ref, const char *nome, const char *base) {
  char ch[LTV_CHAVE];
  ltv_chave_fonte(tipo, ref, ch, sizeof ch);
  return sFonteChave(ch, tipo, nome, base);
}

// Grupo (com a contagem do que a fonte ofereceu) na lista completa da fonte.
static int sGrupoInfo(int f, const char *nome, int somar) {
  GFonteLtv *F = &sLtvF[f];
  int i;
  for (i = F->ngi - 1; i >= 0; i--) if (!strcmp(F->gi[i].nome, nome)) break;
  if (i < 0) {
    GGrupoInfo *no;
    if (F->ngi % 64 == 0) {
      no = realloc(F->gi, sizeof *no * (size_t)(F->ngi + 64));
      if (!no) return -1;
      F->gi = no;
    }
    i = F->ngi++;
    F->gi[i].nome = ltv_dup(&sArena, nome);
    F->gi[i].n = F->gi[i].nInventario = 0; F->gi[i].entra = 1;
  }
  F->gi[i].n += somar;
  return i;
}

// Grupo retido (linha do guia) da fonte `f`. A ultima consulta fica lembrada:
// canais do mesmo grupo vem em sequencia em toda lista medida.
static int sCatDe(int f, const char *nome) {
  static int ult = -1;
  int i;
  if (ult >= 0 && ult < sNCats && sCats[ult].fonte == f && !strcmp(sCats[ult].nome, nome)) return ult;
  for (i = sNCats - 1; i >= 0; i--)
    if (sCats[i].fonte == f && !strcmp(sCats[i].nome, nome)) return ult = i;
  if (sNCats >= sCapCats) {
    int cap = sCapCats ? sCapCats * 2 : 128;
    GCat *no = realloc(sCats, sizeof *no * (size_t)cap);
    if (!no) return -1;
    sCats = no; sCapCats = cap;
  }
  i = sNCats++;
  sCats[i].nome = ltv_dup(&sArena, nome);
  sCats[i].fonte = (short)f;
  sCats[i].ordem = sLtvF[f].nGrupos++;
  return ult = i;
}

static int sCanalPorId(int f, const char *id) {
  int i;
  for (i = sNCanais - 1; i >= 0; i--)
    if (sCanais[i].fonte == f && !strcmp(sCanais[i].id, id)) return i;
  return -1;
}

// Store bounded candidates from JSON, M3U or cache; visible quotas apply later.
typedef struct {
  const char *id, *nome, *logo, *desc, *epgId, *tvgNome, *url, *ua, *ref, *grupo, *tipo;
  int numero;
  int sug;              // 1 = sem categoria (livecat.h sugere uma depois da carga)
} GCru;
static int sGruposEscolhidos(int f, const char **v, int max);
static int nomeNaLista(const char *nome, const char *const *v, int n);
static void cacheCru(int f, const char *grupo, const GCru *c);
static int sCotaCompleta(void);
static int sCategoriaOculta(int f,const char *nome);
static void categoriasCopiar(void);
static int sCategoriasTemOcultas(void);
static int compactarStaging(void);
// Uma versao escondida pelo "Preferir 4K" vira reserva do canal que ficou.
static void sAltAdd(const char *dono, const char *id, const char *nome) {
  if (!xtream_e_id(id) || sNAlts >= G_ALT_MAX) return;
  if (!(sNAlts % 256)) {
    GAlt *no = realloc(sAlts, sizeof *no * (size_t)(sNAlts + 256));
    if (!no) return;
    sAlts = no;
  }
  sAlts[sNAlts].dono = dono;
  sAlts[sNAlts].id = ltv_dup(&sArena, id);
  sAlts[sNAlts].nome = ltv_dup(&sArena, nome);
  sNAlts++;
}
static int sCanalAdd(int f, const GCru *c) {
  GCanal *g;
  const char *sel[256], *original = c->grupo && c->grupo[0] ? c->grupo : "Outros";
  int cat, ns = sGruposEscolhidos(f, sel, 256);
  int resolvido = 0;
  const char *sec = c->sug && ajustes_livetv_sugerir() ? livecat_secao(c->nome, &resolvido) : NULL;
  const char *grupo = sec ? sec : original;
  int pendente = ns && c->sug && ajustes_livetv_sugerir() && !resolvido;
  if (sec && strcmp(sec, original)) {
    int gi = sGrupoInfo(f, original, 0);
    sGrupoInfo(f, sec, 1);
    if (gi >= 0 && sLtvF[f].gi[gi].n > 0) sLtvF[f].gi[gi].n--;
  }
  int gi = sGrupoInfo(f, grupo, 0);
  if (sInventarioCat[0]) {
    if (strcmp(grupo, sInventarioCat) || !c->id || !c->id[0] || !c->nome || !c->nome[0]) return 0;
    if (sInventarioVistos++ < sInventarioInicio) return 0;
    if (sNInventario == G_PF_CANAIS_MAX) { sInventarioMais = 1; return 0; }
    if (!sInventario) sInventario = calloc(G_PF_CANAIS_MAX, sizeof *sInventario);
    if (!sInventario) return 0;
    GInventario *v = &sInventario[sNInventario++];
    snprintf(v->id, sizeof v->id, "%s", c->id);
    snprintf(v->nome, sizeof v->nome, "%s", c->nome);
    snprintf(v->cat, sizeof v->cat, "%s", grupo);
    snprintf(v->fonte, sizeof v->fonte, "%s", sLtvF[f].chave);
    return 0;
  }
  if (c->id && c->id[0] && c->nome && c->nome[0] && gi >= 0 && gi <= USHRT_MAX) {
    unsigned short h[] = {(unsigned short)f, (unsigned short)gi,
      (unsigned short)(strnlen(c->id, GF_ID - 1) + 1),
      (unsigned short)(strnlen(c->nome, GF_NOME - 1) + 1)};
    char record[sizeof h + GF_ID + GF_NOME] = {0};
    size_t bytes = sizeof h + h[2] + h[3];
    memcpy(record, h, sizeof h);
    memcpy(record + sizeof h, c->id, h[2] - 1);
    memcpy(record + sizeof h + h[2], c->nome, h[3] - 1);
    if (!sInventarioDisco) sInventarioDisco = tmpfile();
    if (sInventarioDisco) {
      long pos = ftell(sInventarioDisco);
      int ok = pos >= 0;
      if (ok && bytes > G_INVENTARIO_BLOCO - pos % G_INVENTARIO_BLOCO)
        pos += G_INVENTARIO_BLOCO - pos % G_INVENTARIO_BLOCO;
      if (pos >= G_INVENTARIO_BYTES_MAX) { pos = 0; sInventarioCortado = 1; }
      if (ok) ok = fseek(sInventarioDisco, pos, SEEK_SET) == 0;
      if (ok && !(pos % G_INVENTARIO_BLOCO)) {
        char empty[G_INVENTARIO_BLOCO] = {0};
        ok = fwrite(empty, sizeof empty, 1, sInventarioDisco) == 1 &&
             fseek(sInventarioDisco, pos, SEEK_SET) == 0;
      }
      if (!ok || fwrite(record, bytes, 1, sInventarioDisco) != 1) {
        fclose(sInventarioDisco); sInventarioDisco = NULL;
      }
    }
  }
  if (sNInventario >= G_INVENTARIO_MAX && sCotaCompleta()) { sLtvF[f].cortada = 1; return 0; }
  if (c->id && c->id[0] && c->nome && c->nome[0] &&
      sNInventario < G_INVENTARIO_MAX &&
      (size_t)(sNInventario + G_INVENTARIO_POR_CAT) * sizeof *sInventario <= G_INVENTARIO_BYTES_MAX &&
      gi >= 0 && sLtvF[f].gi[gi].nInventario < G_INVENTARIO_POR_CAT) {
    GInventario *v = sInventario;
    if (!(sNInventario % G_INVENTARIO_POR_CAT))
      v = realloc(sInventario, sizeof *v * (size_t)(sNInventario + G_INVENTARIO_POR_CAT > G_INVENTARIO_MAX ?
                  G_INVENTARIO_MAX : sNInventario + G_INVENTARIO_POR_CAT));
    if (v) {
      sInventario = v;
      v = &sInventario[sNInventario++];
      snprintf(v->id, sizeof v->id, "%s", c->id);
      snprintf(v->nome, sizeof v->nome, "%s", c->nome);
      snprintf(v->cat, sizeof v->cat, "%s", grupo);
      snprintf(v->fonte, sizeof v->fonte, "%s", sLtvF[f].chave);
      sLtvF[f].gi[gi].nInventario++;
    }
  }
  if (!sLtvF[f].deCache &&
      (sFltLive.modo != GF_TODAS || sFltLive.nCat || sFltLive.nCan || sCategoriasTemOcultas())) cacheCru(f, original, c);
  if (ns && !pendente && !nomeNaLista(grupo, sel, ns)) return 0;
  if (sCategoriaOculta(f,grupo)) { sFiltrados++; return 0; }
  // Skip known invisible entries without spending the bounded candidate budget.
  if (guiafiltro_canal_oculto_fonte(&sFltLive, sLtvF[f].chave, c->id)) { sFiltrados++; return 0; }
  int velho = -1;
  int quem = guiafiltro_avaliar(&sFltLive, &sDup, f, grupo, c->id, c->nome, sLtvF[f].nome, &velho);
  if (quem == GF_FORA) { sFiltrados++; return 0; }
  if (quem == GF_FORA_DUP) {
    if (velho >= 0) sAltAdd(sCanais[velho].id, c->id, c->nome);
    return 0;
  }
  if (quem != GF_TROCA &&
      (sCotaCompleta() || sNCanais >= G_CANDIDATOS_MAX || (pendente && sPendentes >= G_MAX_PENDENTES))) {
    sLtvF[f].cortada = 1; return 0;
  }
  // Stable rows cannot free slots later. Keep scanning for inventory and filters.
  if (quem != GF_TROCA && sFltLive.modo != GF_PREFERIR_4K && !ajustes_livetv_sugerir()) {
    int nc = 0, found = 0;
    for (int i = 0; i < sNCats; i++) {
      nc++;
      if (sCats[i].fonte == f && !strcmp(sCats[i].nome, grupo)) found = 1;
    }
    if (sNCanais >= sTeto ||
        (!found && nc >= GUIACORTE_MAX_CAT)) {
      sLtvF[f].cortada = 1; return 0;
    }
  }
  cat = sCatDe(f, grupo);
  if (cat < 0) return 0;
  if (sNCanais >= sCapCanais) {
    int cap = sCapCanais ? sCapCanais * 2 : 512;
    GCanal *no = realloc(sCanais, sizeof *no * (size_t)cap);
    if (!no) return 0;
    sCanais = no; sCapCanais = cap;
  }
  g = &sCanais[quem == GF_TROCA ? velho : sNCanais];
  const char *antigo = quem == GF_TROCA ? g->id : NULL;
  if (antigo) {
    sAltAdd(antigo, antigo, g->nome);
    const char *fields[] = {g->id, g->nome, g->logo, g->desc, g->epgId, g->tvgNome,
                            g->url, g->ua, g->ref, g->tipo, g->grupoOriginal};
    for (size_t i = 0; i < sizeof fields / sizeof fields[0]; i++)
      sTextoSubstituido += strlen(fields[i]) + 1;
  }
  memset(g, 0, sizeof *g);
  g->id = ltv_dup(&sArena, c->id);
  g->nome = ltv_dup(&sArena, c->nome);
  g->logo = ltv_dup(&sArena, c->logo);
  // Full descriptions belong to display slots, not the 30000-candidate scan.
  // Overflow winners retain playback metadata and may publish without a description.
  int slot = quem == GF_TROCA ? velho : sNCanais;
  g->desc = slot < sTeto ?
    ltv_dupn(&sArena, c->desc, c->desc ? strnlen(c->desc, 1024) : 0) : "";
  g->epgId = ltv_dup(&sArena, c->epgId);
  g->tvgNome = ltv_dup(&sArena, c->tvgNome);
  g->url = ltv_dup(&sArena, c->url);
  g->ua = ltv_dup(&sArena, c->ua);
  g->ref = ltv_dup(&sArena, c->ref);
  g->base = sLtvF[f].base;
  g->tipo = ltv_dup(&sArena, c->tipo);
  g->cat = cat;
  g->sug = c->sug ? 1 : 0;
  g->grupoOriginal = sec ? ltv_dup(&sArena, original) : sCats[cat].nome;
  g->epg = -1;
  g->fonte = (short)f;
  if (antigo) {
    for (int k = 0; k < sNAlts; k++) if (sAlts[k].dono == antigo) sAlts[k].dono = g->id;
  } else {
    guiafiltro_dup_registrar(&sFltLive, &sDup, f, grupo, c->nome, sLtvF[f].nome, sNCanais);
    sLtvF[f].nCanais++;
    sNCanais++; if (pendente) sPendentes++;
  }
  g->numero = c->numero > 0 ? c->numero : sLtvF[f].nCanais;
  // Replacements leave dead arena strings; reclaim them during the scan.
  if (sTextoSubstituido >= 1024 * 1024 && compactarStaging()) sTextoSubstituido = 0;
  return 1;
}

// Os grupos que a pessoa escolheu para esta fonte (ltv_pref). 0 = sem escolha.
static int sGruposEscolhidos(int f, const char **v, int max) {
  LtvPref *p = ltv_pref(sLtvF[f].chave, 0);
  return p ? ltv_pref_grupos(p, v, max) : 0;
}
static int nomeNaLista(const char *nome, const char *const *v, int n) {
  int i;
  for (i = 0; i < n; i++) if (!strcmp(v[i], nome)) return 1;
  return 0;
}

static void arquivoPerfil(const char *tipo, char *nome, size_t tam) {
  int p = perfis_ativo();
  snprintf(nome, tam, "guia-%s-p%d.txt", tipo, p > 0 ? p : 1);
}

// Os arquivos antigos eram do aparelho inteiro. Somente o perfil primario
// herda essa escolha; os demais nao recebem favoritos de outra pessoa.
static char *lerPerfil(const char *tipo) {
  char nome[48], antigo[48];
  char *t;
  arquivoPerfil(tipo, nome, sizeof nome);
  t = dados_ler(nome);
  if (!t && perfis_ativo() <= 1) {
    snprintf(antigo, sizeof antigo, "guia-%s.txt", tipo);
    t = dados_ler(antigo);
    if (t) dados_gravar(nome, t);
  }
  return t;
}

// --- favoritos --------------------------------------------------------------
// ID ESTAVEL "fonte:id" (M1): o mesmo id pode existir em duas fontes, e o
// favorito e de UMA. Arquivo novo ltv-fav.txt, "chave TAB id" por linha; o
// antigo guia-fav.txt (so o id) e lido na primeira vez e cada linha dele vale
// para qualquer fonte ate a primeira lista publicada dizer de qual fonte o canal
// e — ai o arquivo novo e gravado com a chave (favMigrar). O antigo fica no
// disco por duas versoes (volta de versao nao perde favorito).
typedef struct { char chave[LTV_CHAVE]; char id[80]; } GFav;
static GFav fav[G_MAX_FAV]; static int nFav, favLido, favLegado;
static int  favOrd[G_MAX_FAV];  static int nFavOrd;   // indices em canais[]

static const char *chaveDo(const GCanal *c) {
  return c->fonte >= 0 && c->fonte < nLtvF ? ltvF[c->fonte].chave : "";
}
static int favIndiceCanal(const GCanal *c) {
  int i;
  const char *ch = chaveDo(c);
  for (i = 0; i < nFav; i++)
    if (!strcmp(fav[i].id, c->id) && (!fav[i].chave[0] || !strcmp(fav[i].chave, ch))) return i;
  return -1;
}
static int favIndice(const char *id) {
  for (int i = 0; i < nFav; i++) if (!strcmp(fav[i].id, id)) return i;
  return -1;
}

static void favLerTexto(const char *t) {
  while (t && *t && nFav < G_MAX_FAV) {
    const char *fim = t + strcspn(t, "\n");
    char linha[200];
    size_t k = (size_t)(fim - t);
    if (k < sizeof linha) {
      memcpy(linha, t, k); linha[k] = 0;
      if (ltv_fav_linha(linha, fav[nFav].chave, sizeof fav[nFav].chave, fav[nFav].id, sizeof fav[nFav].id))
        nFav++;
    }
    t = *fim ? fim + 1 : fim;
  }
}
static void favLer(void) {
  char nome[48];
  arquivoPerfil("ltv-fav", nome, sizeof nome);
  char *t = dados_ler(nome);
  favLido = 1;
  if (t) { favLerTexto(t); free(t); return; }
  t = lerPerfil("fav");
  if (t) { favLerTexto(t); free(t); favLegado = nFav > 0; }
}

static void favGravar(void) {
  char buf[G_MAX_FAV * 110]; size_t n = 0;
  for (int i = 0; i < nFav && n + 110 < sizeof buf; i++)
    n += (size_t)snprintf(buf + n, sizeof buf - n, "%s\t%s\n", fav[i].chave, fav[i].id);
  buf[n] = 0;
  char nome[48];
  arquivoPerfil("ltv-fav", nome, sizeof nome);
  dados_gravar(nome, buf);
}

// MIGRACAO: favorito sem fonte ganha a do primeiro canal com aquele id. So
// grava quando alguma linha mudou.
static void favMigrar(void) {
  int i, j, mudou = 0;
  for (i = 0; i < nFav; i++) {
    if (fav[i].chave[0]) continue;
    for (j = 0; j < nCanais; j++)
      if (!strcmp(canais[j].id, fav[i].id)) {
        snprintf(fav[i].chave, sizeof fav[i].chave, "%s", chaveDo(&canais[j]));
        mudou = 1;
        break;
      }
  }
  if (mudou || favLegado) {
    favLegado = 0;
    favGravar();
    printf("[guia] favoritos com fonte: %d\n", nFav);
    fflush(stdout);
  }
}

static void favAplicar(void) {
  for (int i = 0; i < nCanais; i++)
    canais[i].fav = favIndiceCanal(&canais[i]) >= 0;
}

static void vistaMontar(void);
static void favAlternar(GCanal *c) {
  int i = favIndiceCanal(c);
  if (i >= 0) { memmove(&fav[i], &fav[i + 1], (size_t)(nFav - i - 1) * sizeof fav[0]); nFav--; c->fav = 0; }
  else if (nFav < G_MAX_FAV) {
    snprintf(fav[nFav].chave, sizeof fav[nFav].chave, "%s", chaveDo(c));
    snprintf(fav[nFav].id, sizeof fav[nFav].id, "%s", c->id);
    nFav++; c->fav = 1;
  }
  favGravar();
  nFavOrd = 0;
  for (i = 0; i < nCanais && nFavOrd < G_MAX_FAV; i++)
    if (canais[i].fav) favOrd[nFavOrd++] = i;
  vistaMontar();
}

// --- modo de exibicao ------------------------------------------------------------
// 1 = lista, 0 = cartoes. Persistido por perfil: quem escolheu lista
// espera reabrir em lista, senao a escolha vira um gesto a repetir por sessao.
static int modoLista, modoLido;

static void modoLer(void) {
  char *t = lerPerfil("modo");
  modoLido = 1;
  modoLista = 0;
  if (!t) return;
  modoLista = !strncmp(t, "lista", 5);
  free(t);
}

static void modoGravar(void) {
  char nome[48]; arquivoPerfil("modo", nome, sizeof nome);
  dados_gravar(nome, modoLista ? "lista\n" : "cartoes\n");
}

// --- preview de video do canal focado ------------------------------------------
//
// O QUE E: quando o foco DESCANSA num canal (tela cheia do guia), o canal toca
// no preview 800x450 do heroi. A pessoa VE o canal antes de apertar OK, e o OK
// so faz o mesmo video crescer para a tela cheia (ver "preview de video: e uma
// SESSAO DO PLAYER" mais abaixo — desde 25/09/2026 nao ha pipeline proprio).
//
// PADRAO DESLIGADO: preview gasta rede e CPU numa TV que ja disputa texturas e
// decode com o resto da interface; o HLS ao vivo demora ~20 s do "fonte
// escolhida" ao primeiro quadro (ver video.h). Quem liga, liga sabendo disto.
// O canal que ja estava no ar (Voltar da tela cheia) vai para o preview com o
// preview ligado ou nao: ali nao ha custo novo, o fluxo e o mesmo.
//
// SO TELA CHEIA, nunca na faixa do mini guia: la o video esta em tela cheia
// atras da faixa, e o pipeline e UNICO (um mediaId so no barramento LS2).
static int previewLigado, previewLido;
static void previewLer(void) {
  char *t = lerPerfil("preview");
  previewLido = 1;
  previewLigado = 0;
  if (!t) return;
  previewLigado = (t[0] == '1');
  free(t);
}
static void previewGravar(void) {
  char nome[48]; arquivoPerfil("preview", nome, sizeof nome);
  dados_gravar(nome, previewLigado ? "1\n" : "0\n");
}

// --- o que cada addon da conta declara -----------------------------------------
// "Este addon fornece canal?" e uma pergunta que so o manifesto responde, e a
// sonda deste guia ja o baixa. Guardar a resposta POR BASE e o que permite ao
// painel de addons dizer "fornece canais" / "sem catalogo de canais" em vez
// de listar os dezesseis addons da conta sem distinguir nada. -1 = manifesto
// nao respondeu (nao e "nao fornece": e "nao se sabe").
typedef struct { char base[NV_ADDON_URL_MAX]; int canal; } GSabe;
#define G_MAX_SABE 16
static GSabe sSabe[G_MAX_SABE]; static int sNSabe;   // do fio
static GSabe sabe[G_MAX_SABE];  static int nSabe;    // publicado

typedef struct {
  char base[NV_ADDON_URL_MAX], nome[64];
  int ativo, nCatalogos;
  AddCatCanal catalogos[ADD_CANAL_MAX];
} GAddonCarga;
static GAddonCarga *addonsCarga;
static int nAddonsCarga;
static int fonteAtual=-1;
static NvPerfilSessao cargaSessao;
static LtvUrl cargaUrls[LTV_URLS_MAX]; static int nCargaUrls;
static IptvConta cargaContas[IPTV_MAX_CONTAS]; static int nCargaContas;
static volatile int cargaCancelada;

static int cargaAtual(void) {
  return !cargaCancelada && nv_perfil_sessao_atual(cargaSessao);
}
static int cargaBaseLigada(const char *base) {
  for (int i = 0; i < nAddonsCarga; i++)
    if (!strcmp(addonsCarga[i].base, base)) return addonsCarga[i].ativo;
  return 0;
}
static char *cargaBaixar(const char *url) {
  RedeControle ctl = {0};
  if (!cargaAtual()) return NULL;
  ctl.cancelado = &cargaCancelada;
  return rede_baixar_medido_controle(url, 15, NULL, &ctl, NULL);
}

static int sabeCanal(const char *base) {   // 1, 0, ou -1 = desconhecido
  for (int i = 0; i < nSabe; i++)
    if (!strcmp(sabe[i].base, base)) return sabe[i].canal;
  return -1;
}

// Only an installed, enabled add-on may supply channels. Native IPTV sources
// have an empty base and bypass this add-on check.
static int baseLigada(const char *base) {
  for (int i = 0; i < addons_n(); i++) {
    const char *b = addons_base(i);
    if (b && b[0] && !strcmp(b, base)) return addons_ativo(i);
  }
  return 0;
}

// --- add-ons escondidos SO NO GUIA (#283) ---------------------------------------
// Por perfil, padrao: todos aparecem. Esconder tira os canais daquele add-on
// desta tela; ele segue ligado na home, na busca e nas fontes. Lido sob
// demanda e relido quando o perfil muda.
static GuiaAddonsOcultos ocultos;
static int ocultosPerfil = -1;

static void ocultosGarantir(void) {
  int p = perfis_ativo();
  char nome[48], *t;
  if (p == ocultosPerfil) return;
  ocultosPerfil = p;
  t = dados_ler(guiaaddons_arquivo(p, nome, sizeof nome));
  guiaaddons_ler(&ocultos, t);
  free(t);
}

static int baseOculta(const char *base) {
  ocultosGarantir();
  return guiaaddons_oculto(&ocultos, base);
}

static void ocultosGravar(void) {
  char nome[48], buf[GUIAADDONS_MAX * 2049 + 1];
  guiaaddons_texto(&ocultos, buf, sizeof buf);
  dados_gravar(guiaaddons_arquivo(ocultosPerfil, nome, sizeof nome), buf);
}

// --- addons recomendados ---------------------------------------------------------
// PREMISSA, escrita aqui porque ela decide o que a tela PODE mostrar: nao
// existe ranking publico de addons Stremio — nenhum contador de instalacoes,
// nenhuma nota. "Recomendado" neste guia e CURADORIA DO DONO, um arquivo que
// ele edita a mao. Por isso a secao nao mostra numero nenhum: qualquer
// "1.2k instalacoes" ou estrela ali seria dado inventado.
//
// Formato de addons-recomendados.txt, uma linha por addon:
//     <nome>\t<url do manifest>\t<descricao em portugues>\t<descricao em ingles>
// A quarta coluna e opcional: sem ela, a tela em ingles mostra a portuguesa —
// e melhor que mostrar nada. Arquivo ausente ou vazio = a secao nao aparece.
//
// DUAS COLUNAS E NAO i18n(): descricao e texto livre, nao chave de tabela. A
// tabela de idioma_tab.h e para o que o app diz de si; o que o addon diz de si
// vive no arquivo curado, nas duas linguas.
typedef struct { char nome[64]; char url[NV_ADDON_URL_MAX]; char desc[200]; char descEn[200]; } GRec;
static GRec rec[G_MAX_REC]; static int nRec;

// ONDE FICA A PASTA art/. Este modulo nao recebe dirArte (so main.c o tem, e
// ele nao o publica). Os tres candidatos sao os tres jeitos REAIS de o app
// ser lancado, na ordem em que aparecem em main.c e tools/mac.sh:
//   1. <SDL_GetBasePath()>/art — o pacote instalado na LG (SAM passa JSON em
//      argv[1], e main.c cai neste mesmo calculo);
//   2. deploy/app/art relativo — tools/mac.sh faz cd para a raiz do repo e
//      passa esse caminho em argv[1];
//   3. /app/art — o --preload-file do alvo Tizen.
// Um getter em main/app (`const char *app_dir_arte(void)`) tiraria esta
// adivinhacao; ver o relatorio da entrega.
static FILE *abrirNaArte(const char *nome) {
  char cam[700];
  FILE *f = NULL;
  char *base = SDL_GetBasePath();
  if (base) {
    snprintf(cam, sizeof cam, "%sart/%s", base, nome);
    f = fopen(cam, "r");
    SDL_free(base);
  }
  if (!f) { snprintf(cam, sizeof cam, "deploy/app/art/%s", nome); f = fopen(cam, "r"); }
  if (!f) { snprintf(cam, sizeof cam, "/app/art/%s", nome); f = fopen(cam, "r"); }
  return f;
}

// Relida a cada abertura do painel: o arquivo e pequeno e o dono edita sem
// reiniciar o app.
static void recLer(void) {
  char linha[NV_ADDON_URL_MAX + 512];   // nome, URL inteira (addonurl.h) e as duas descricoes
  FILE *f = abrirNaArte("addons-recomendados.txt");
  nRec = 0;
  if (!f) return;
  while (nRec < G_MAX_REC && fgets(linha, sizeof linha, f)) {
    char *a = linha, *b, *c, *fim;
    fim = a + strlen(a);
    while (fim > a && (fim[-1] == '\n' || fim[-1] == '\r')) *--fim = 0;
    if (!*a || *a == '#') continue;
    b = strchr(a, '\t'); if (!b) continue; *b++ = 0;
    c = strchr(b, '\t'); if (c) *c++ = 0;
    { char *d = c ? strchr(c, '\t') : NULL; if (d) *d++ = 0;
    if (!*a || !*b) continue;
    snprintf(rec[nRec].nome,   sizeof rec[nRec].nome,   "%s", a);
    snprintf(rec[nRec].url,    sizeof rec[nRec].url,    "%s", b);
    snprintf(rec[nRec].desc,   sizeof rec[nRec].desc,   "%s", c ? c : "");
    snprintf(rec[nRec].descEn, sizeof rec[nRec].descEn, "%s", d ? d : ""); }
    nRec++;
  }
  fclose(f);
}

// A mesma normalizacao de addons.c (nv_addon_base, addonurl.h). E o que faz
// "instalado" bater com addons_base(i) para a URL que o dono escreveu no
// arquivo.
static void baseDaUrl(const char *url, char *dst, size_t tam) {
  nv_addon_base(url, dst, tam);
}

static int recInstalado(const GRec *r) {
  char base[NV_ADDON_URL_MAX];
  baseDaUrl(r->url, base, sizeof base);
  for (int i = 0; i < addons_n(); i++)
    if (!strcmp(addons_base(i), base)) return 1;
  return 0;
}


// --- linhas e a VISTA (barra de fontes, M3) ------------------------------------
// FILTRO de fonte da barra: -1 "Todas" (o padrao a cada abertura do app,
// decisao 1), -2 "Favoritos", k = so a fonte k. Guardado so na sessao.
#define G_FILTRO_TODAS (-1)
#define G_FILTRO_FAV   (-2)
static int filtro = G_FILTRO_TODAS;
static int focoLin, focoCol;           // definidos de novo no estado de tela
static char filtroChave[LTV_CHAVE];   // a fonte do filtro sobrevive a republicacao
// Os grupos visiveis (indices em cats[]) e a ORDEM DE ZAP da vista: CH+/-, o
// numero digitado e a faixa do mini guia andam dentro do que a tela mostra.
static int *vis;  static int nVis, capVis;
static int *zapL; static int nZapL, capZapL;

#include "guia_grupos.inc"
#include "guia_categorias.inc"

static int mostraFav(void) {
  return nFavOrd > 0 && (filtro == G_FILTRO_TODAS || filtro == G_FILTRO_FAV);
}
static int nLinhas(void) { return nVis + (mostraFav() ? 1 : 0); }
static int linhaEhFav(int l) { return mostraFav() && l == 0; }
static int linhaCatDe(int l) {  // indice em cats[], -1 = favoritos
  return linhaEhFav(l) ? -1 : vis[l - (mostraFav() ? 1 : 0)];
}
static int linhaN(int l) {
  if (l < 0 || l >= nLinhas()) return 0;
  return linhaEhFav(l) ? nFavOrd : catN[linhaCatDe(l)];
}
// "Esportes · Xtream" em Todas quando ha mais de uma fonte (decisao 2: grupos
// de mesmo nome de fontes diferentes sao linhas diferentes, e a tela diz qual).
static const char *linhaNome(int l) {
  static char buf[200];
  int c;
  if (linhaEhFav(l)) return i18n("Favoritos");
  c = linhaCatDe(l);
  if (filtro == G_FILTRO_TODAS && nLtvF > 1 && cats[c].fonte >= 0 && cats[c].fonte < nLtvF) {
    snprintf(buf, sizeof buf, "%s  \xc2\xb7  %s", i18n(cats[c].nome), ltvF[cats[c].fonte].nome);
    return buf;
  }
  return i18n(cats[c].nome);
}
static GCanal *linhaItem(int l, int c) {
  if (c < 0 || c >= linhaN(l)) return NULL;
  if (linhaEhFav(l)) return &canais[favOrd[c]];
  return &canais[catIni[linhaCatDe(l)] + c];
}

static int vetorCabe(int **v, int *cap, int n) {
  if (n <= *cap) return 1;
  { int *no = realloc(*v, sizeof(int) * (size_t)(n + 64));
    if (!no) return 0;
    *v = no; *cap = n + 64; }
  return 1;
}

static void vistaMontar(void) {
  int i;
  if (filtro >= 0) {
    // A fonte do filtro pela CHAVE: a republicacao pode mudar o indice.
    for (i = 0; i < nLtvF && strcmp(ltvF[i].chave, filtroChave); i++) {}
    filtro = i < nLtvF ? i : G_FILTRO_TODAS;
  }
  categoriasGarantir();
  nFavOrd=0;
  for(i=0;i<nCanais && nFavOrd<G_MAX_FAV;i++)
    if(canais[i].fav && fonteVisivel(canais[i].fonte) && !categoriaOculta(canais[i].cat))favOrd[nFavOrd++]=i;
  nVis = 0; nZapL = 0;
  if (!vetorCabe(&vis, &capVis, nCats) || !vetorCabe(&zapL, &capZapL, nCanais)) return;
  if (filtro != G_FILTRO_FAV)
    for (i = 0; i < nCats; i++)
      if (catN[i] > 0 && fonteVisivel(cats[i].fonte) && !categoriaOculta(i) && (filtro < 0 || cats[i].fonte == filtro)) vis[nVis++] = i;
  if (filtro == G_FILTRO_FAV) for (i = 0; i < nFavOrd; i++) zapL[nZapL++] = favOrd[i];
  else
    for (i = 0; i < nCanais; i++)
      if (fonteVisivel(canais[i].fonte) && !categoriaOculta(canais[i].cat) && (filtro < 0 || canais[i].fonte == filtro)) zapL[nZapL++] = i;
}

static void filtroDefinir(int f) {
  filtro = f;
  if (f >= 0 && f < nLtvF) snprintf(filtroChave, sizeof filtroChave, "%s", ltvF[f].chave);
  vistaMontar();
}

// --- carga --------------------------------------------------------------------
// Tipos de catalogo que os addons de canal usam no manifesto Stremio:
// "channel" (FrostView), "tv" (o tipo nativo de TV ao vivo do Stremio) e os
// raros "channels"/"live"/"iptv" de addons de lista. Qualquer addon novo que
// declare um catalogo desses cai direto no guia — e a resposta ao "se outro
// addon de canais entrar, ele ja vai parar aqui".
static int ehCanal(const char *t) {
  static const char *tipos[] = { "channel", "tv", "channels", "live", "iptv" };
  int i;
  if (!t || !t[0]) return 0;
  for (i = 0; i < (int)(sizeof tipos / sizeof tipos[0]); i++)
    if (!strcasecmp(t, tipos[i])) return 1;
  return 0;
}

// Descobre os catalogos de canal pelas fileiras que a descoberta ja montou.
// Chamado no fio de desenho (cat_fileira nao e seguro fora dele).
static void descobrirFontes(void) {
  nFontes = 0;
  for (int i = 0; i < cat_n_fileiras(); i++) {
    const CatFileira *f = cat_fileira(i);
    int ja = 0;
    if (!f || !ehCanal(f->tipo) || !f->base[0] || !f->catId[0]) continue;
    for (int j = 0; j < nFontes; j++)
      if (!strcmp(fontes[j].base, f->base) && !strcmp(fontes[j].id, f->catId)) ja = 1;
    if (!ja && nFontes < G_MAX_FONTE) {
      snprintf(fontes[nFontes].base, sizeof fontes[nFontes].base, "%s", f->base);
      snprintf(fontes[nFontes].tipo, sizeof fontes[nFontes].tipo, "%s", f->tipo);
      snprintf(fontes[nFontes].id,   sizeof fontes[nFontes].id,   "%s", f->catId);
      snprintf(fontes[nFontes].nome, sizeof fontes[nFontes].nome, "%s", f->titulo);
      nFontes++;
    }
  }
  fontesOk = 1;
}

// CATEGORIA PELO NOME DO CANAL, quando o addon nao manda genero nenhum.
//
// O Meu Futebol manda 68 canais e o Pluto TV 1.523, todos sem `genres`; so o
// Fenix classifica. Cair tudo em "Outros" (ou no nome do catalogo) deixava o
// guia sem secao para o que a pessoa mais usa. O nome do canal carrega a
// classificacao na maior parte dos casos — "ESPN 4", "GloboNews", "Cartoon
// Network" — e uma tabela de palavras cobre os canais brasileiros e
// americanos que esses addons trazem. E HEURISTICA e esta dito aqui: quem
// nao casa vai para o nome do catalogo, nunca para uma secao errada por
// palpite fraco. A ORDEM DA TABELA IMPORTA: "BandSports" tem de cair em
// Esportes antes de "Band" cair em Abertos, "TNT Sports" antes de "TNT".
//
// As secoes sao chaves de idioma_tab.h — linhaNome passa cats[] por i18n().
static int contemSemCaixa(const char *texto, const char *chave) {
  size_t n = strlen(chave);
  for (; *texto; texto++)
    if (!strncasecmp(texto, chave, n)) return 1;
  return 0;
}
static const char *categoriaPorNome(const char *nome) {
  static const struct { const char *cat; const char *chaves[40]; } tab[] = {
    { "Esportes", { "espn", "sportv", "sport", "premiere", "combate", "dazn", "ufc",
                    "nba", "nfl", "nhl", "mlb", "tyc", "futebol", "golf", "tennis",
                    "motor", "caz", "ge tv", "bein", "fight", "wwe",
                    "racing", "olymp", "paramount+ esport", "goat", "poker", NULL } },
    { "Notícias", { "news", "cnn", "jovem pan", "bloomberg", "msnbc", "cnbc",
                    "euronews", "newsmax", "noticia", "al jazeera",
                    "weather", "tempo", NULL } },
    { "Infantil", { "cartoon", "nick", "disney", "gloob", "boomerang", "kids",
                    "baby", "junior", "tooncast", "cartoonito", "zoomoo",
                    "discovery kids", "pbs kids", NULL } },
    { "Documentários", { "discovery", "history", "nat geo", "national geo",
                    "animal planet", "curta", "investiga", "science", "smithsonian",
                    "docu", "h2", "planet", "nature", "crime", NULL } },
    { "Filmes", { "hbo", "telecine", "cine", "megapix", "tcm", "paramount", "amc",
                  "space", "star channel", "movie", "film", "cinemax", "studio universal",
                  "sony movies", "canal brasil", "syfy", "arte 1", "arte1", NULL } },
    { "Séries", { "warner", "sony", "universal", "fx", "axn", "a&e", "ae ", "series",
                  "tnt", "comedy", "fox", "cw", NULL } },
    { "Música", { "mtv", "music", "vh1", "bis", "radio", "hits", "trace",
                  "sertanejo", "pagode", "rock", "jazz", "classic", NULL } },
    { "Variedades", { "gnt", "multishow", "viva", "off", "tlc", "lifetime", "food",
                      "hgtv", "e!", "reality", "entertainment", "travel", "cooking",
                      "shopping", "bravo", "oxygen", "hallmark", "we tv", NULL } },
    { "Abertos", { "globo", "sbt", "record", "band", "redetv", "rede tv", "cultura",
                   "tv brasil", "rede vida", "futura", "gazeta", "cnt", "aparecida",
                   "cancao nova", "rede brasil", "novo tempo",
                   "abc", "nbc", "cbs", "pbs", "ion", "rede genesis", "rit", "boas novas",
                   "tv escola", "rbi", NULL } },
  };
  int i, k;
  if (!nome || !*nome) return NULL;
  for (i = 0; i < (int)(sizeof tab / sizeof tab[0]); i++)
    for (k = 0; tab[i].chaves[k]; k++)
      if (contemSemCaixa(nome, tab[i].chaves[k])) return tab[i].cat;
  return NULL;
}


// Primeiro elemento de texto de um array JSON ("genre":["X"] -> "X"). Pelo
// js_cadeia, e nao copia crua: addon em Python (json.dumps) escapa todo
// caractere fora do ASCII, e "Not\u00edcias" virava a categoria
// "Notu00edcias" (medido em 01/10/2026 com addon de teste).
static int lerStrEl(const char *p, char *dst, int tam) {
  if (tam > 0) dst[0] = 0;
  return js_cadeia(p, dst, (size_t)tam);
}


static int fonteJa(const char *base, const char *id) {   // indice ou -1
  for (int i = 0; i < sNFontes; i++)
    if (!strcmp(sFontes[i].base, base) && !strcmp(sFontes[i].id, id)) return i;
  return -1;
}

// SONDA DE MANIFESTOS, no fio de trabalho. A home monta no maximo
// CAT_FIL_MAX fileiras e so depois que cada catalogo responde — um catalogo
// de canal fora do corte (ou ainda nao montado) deixava o guia sem fonte e
// era exatamente o "nao tem os dados de todos os canais". O manifesto declara
// tudo; baixar `base/manifest.json` de cada addon ativo e varrer catalogs[]
// cobre o caso. E o mesmo padrao de lerManifesto da descoberta, do mesmo
// tipo de fio.
// Quantos addons de canal foram TENTADOS e nao responderam nesta rodada. Zero
// com a lista vazia significa mesmo "nenhum addon declara canal". `s` e do fio,
// `falhas` e a copia publicada, como o resto do estado desta tela.
static int sFalhas, falhas;
// O XTREAM, SEPARADO DOS ADDONS (issue #112). Lista do Xtream que nao
// respondeu somava zero canais em silencio: com um addon de canais junto, o
// guia enchia com os dele e ninguem ficava sabendo que o portal falhou — no
// registro 1647 (Samsung) o log dizia "servidor nao respondeu a lista de
// canais" e a tela "619 canais". XT_OK / XT_SEM_RESPOSTA / XT_RECUSOU de
// xtream.h; `s` e do fio, o outro e a copia publicada, como `falhas`.
static int sXtFalha, xtFalha;
// O codigo HTTP da lista quando xtFalha == XT_HTTP (#158): 403, 429, 458...
static int sXtHttp, xtHttp;

// Texto do cabecalho com o aviso da conta Xtream; 0 quando nao ha aviso.
// Usa nCanais/nCats, entao so vale depois deles declarados — por isso o
// prototipo aqui e a definicao perto do desenho.
static int xtAviso(char *sub, size_t tam);
// Ver a definicao, junto do EPG por canal (#158).
static void epgPaisesEscolher(void);

static void sondaManifestos(void) {
  int a;
  sNSabe = 0;
  for (a = 0; cargaAtual() && a < nAddonsCarga && sNFontes < G_MAX_FONTE; a++) {
    const char *base;
    char url[NV_ADDON_PEDIDO_MAX];
    char *corpo;
    const char *p, *fim;
    const GAddonCarga *ad = &addonsCarga[a];
    int ativo = ad->ativo, temCanal = 0;
    GSabe *sb = sNSabe < G_MAX_SABE ? &sSabe[sNSabe] : NULL;
    base = ad->base;
    if (!base || !base[0]) continue;
    if (sb) { snprintf(sb->base, sizeof sb->base, "%s", base); sb->canal = -1; sNSabe++; }
    // O MANIFESTO JA FOI LIDO PELA DESCOBERTA, no arranque e em paralelo: o
    // que ele declara de canal esta em addons.c. Reler aqui era um GET por
    // addon, em serie, com 15 s de prazo cada — a maior parte da espera para o
    // guia abrir numa conta com 10-20 addons (a maioria sem canal nenhum).
    // So cai na rede quando aquele manifesto ainda nao chegou.
    { const AddCatCanal *cc = ad->catalogos;
      int nc = ad->nCatalogos, k;
      if (nc >= 0) {
        if (sb) sb->canal = nc > 0;
        if (!ativo) continue;
        for (k = 0; k < nc && sNFontes < G_MAX_FONTE; k++) {
          int ja = fonteJa(base, cc[k].id);
          if (ja < 0) {
            ja = sNFontes++;
            snprintf(sFontes[ja].base, sizeof sFontes[ja].base, "%s", base);
            snprintf(sFontes[ja].tipo, sizeof sFontes[ja].tipo, "%s", cc[k].tipo);
            snprintf(sFontes[ja].id,   sizeof sFontes[ja].id,   "%s", cc[k].id);
            sFontes[ja].nome[0] = 0;
          }
          if (cc[k].nome[0]) snprintf(sFontes[ja].nome, sizeof sFontes[ja].nome, "%s", cc[k].nome);
        }
        continue;
      } }
    // ADDON DESLIGADO TAMBEM E LIDO, so que nao vira fonte. Custa um GET por
    // addon desligado (medido: 0 a 3 numa conta tipica) e e o que permite ao
    // painel dizer se vale a pena religa-lo para o guia.
    corpo = nv_addon_pedido_coube(ad->nome, nv_addon_url(url, sizeof url, base, "/manifest.json"),
                                  sizeof url)
            ? cargaBaixar(url) : NULL;
    if (!cargaAtual()) { free(corpo); return; }
    if (!ativo) {
      if (corpo) {
        fim = corpo + strlen(corpo);
        p = js_array(corpo, fim, "catalogs");
        while (p) {
          const char *f = js_fim(p);
          char tipo[16] = "";
          js_texto(p, f, "type", tipo, sizeof tipo);
          if (ehCanal(tipo)) { temCanal = 1; break; }
          p = js_prox(f);
        }
        if (sb) sb->canal = temCanal;
        free(corpo);
      }
      continue;
    }
    // MANIFESTO QUE NAO RESPONDE NAO E "ADDON SEM CANAL". A sonda pulava em
    // silencio, e com isso a tela vazia acusava a conta da pessoa ("instale um
    // addon de canais") justamente quando o addon ESTAVA instalado e era o
    // servidor dele que estava fora. Foi o que aconteceu com o FrostView
    // devolvendo 408 e o Minha TV nao respondendo o catalogo: o guia mandava
    // instalar o que ja estava la. Contar a falha e o que permite dizer a
    // verdade tres linhas abaixo.
    if (!corpo) { sFalhas++; continue; }
    fim = corpo + strlen(corpo);
    p = js_array(corpo, fim, "catalogs");
    while (p && sNFontes < G_MAX_FONTE) {
      const char *f = js_fim(p);
      char tipo[16] = "", id[96] = "", nome[96] = "";
      int ja;
      js_texto(p, f, "type", tipo, sizeof tipo);
      js_texto(p, f, "id", id, sizeof id);
      js_texto(p, f, "name", nome, sizeof nome);
      if (ehCanal(tipo)) temCanal = 1;
      if (ehCanal(tipo) && id[0]) {
        ja = fonteJa(base, id);
        if (ja < 0) {
          ja = sNFontes++;
          snprintf(sFontes[ja].base, sizeof sFontes[ja].base, "%s", base);
          snprintf(sFontes[ja].tipo, sizeof sFontes[ja].tipo, "%s", tipo);
          snprintf(sFontes[ja].id,   sizeof sFontes[ja].id,   "%s", id);
          sFontes[ja].nome[0] = 0;
        }
        // O nome do manifesto e o limpo; o titulo da fileira carrega sufixo
        // de tipo. Quando os dois existem, fica o do manifesto.
        if (nome[0]) snprintf(sFontes[ja].nome, sizeof sFontes[ja].nome, "%s", nome);
      }
      p = js_prox(f);
    }
    if (sb) sb->canal = temCanal;
    free(corpo);
  }
}


// Uma pagina de catalogo de addon para a fonte `fl`. Devolve quantos METAS a
// pagina trouxe — nao quantos entraram: uma fonte pode repetir canais de outra,
// e contar so os novos encerraria a pagina uma rodada cedo.
// `novos` includes cached resolutions skipped by selection, so reloads can
// reach later candidates. Identical consecutive pages stop providers ignoring skip.
static int lerPagina(const GFonte *f, int fl, int skip, int *novos) {
  static unsigned long ultimaPagina;
  char url[NV_ADDON_PEDIDO_MAX];
  char *corpo;
  const char *p, *sel[256];
  int n = 0, w, nsel = sGruposEscolhidos(fl, sel, 256);
  if (skip > 0)
    w = nv_addon_url(url, sizeof url, f->base, "/catalog/%s/%s/skip=%d.json",
                     f->tipo, f->id, skip);
  else
    w = nv_addon_url(url, sizeof url, f->base, "/catalog/%s/%s.json", f->tipo, f->id);
  *novos = 0;
  // Pedido cortado nao sai (addonurl.h); o log leva o nome do catalogo.
  if (!nv_addon_pedido_coube(f->nome, w, sizeof url)) return 0;
  if (!cargaAtual() || !cargaBaseLigada(f->base)) return 0;
  corpo = cargaBaixar(url);
  if (!corpo || !cargaAtual()) { free(corpo); return 0; }
  unsigned long pagina = ltv_hash(corpo);
  if (skip > 0 && pagina == ultimaPagina) { free(corpo); return 0; }
  ultimaPagina = pagina;
  p = js_array(corpo, NULL, "metas");
  while (p) {
    const char *fim = js_fim(p);
    char id[80], idLongo[IDCANAL_LONGO_MAX + 1], nome[200], logo[600], desc[600], gen[64] = "", mtipo[16] = "";
    int temId = js_texto(p, fim, "id", idLongo, sizeof idLongo);
    n++;
    if (temId && idcanal_curto(idLongo, id, sizeof id) &&
        js_texto(p, fim, "name", nome, sizeof nome) && sCanalPorId(fl, id) < 0) {
      GCru c;
      int semCat;
      // O catalogo se declara de canal, mas um meta que se diz filme/serie
      // nao entra — protege o guia de catalogo mal declarado.
      if (js_texto(p, fim, "type", mtipo, sizeof mtipo) &&
          mtipo[0] && !ehCanal(mtipo)) { p = js_prox(fim); continue; }
      logo[0] = desc[0] = 0;
      if (!js_texto(p, fim, "poster", logo, sizeof logo))
        js_texto(p, fim, "logo", logo, sizeof logo);
      js_texto(p, fim, "description", desc, sizeof desc);
      { const char *g = js_array(p, fim, "genre");
        if (!g) g = js_array(p, fim, "genres");
        lerStrEl(g, gen, sizeof gen); }
      // SEM GENERO: primeiro o nome do canal (categoriaPorNome), depois o
      // nome do catalogo de onde ele veio ("Meu Futebol"). "Outros" fica so
      // para catalogo sem nome.
      if (!gen[0]) {
        const char *h = categoriaPorNome(nome);
        if (h) snprintf(gen, sizeof gen, "%s", h);
      }
      semCat = !gen[0];
      if (!gen[0]) snprintf(gen, sizeof gen, "%s", f->nome[0] ? f->nome : "Outros");
      memset(&c, 0, sizeof c);
      c.id = id; c.nome = nome; c.logo = logo; c.desc = desc; c.grupo = gen;
      c.tipo = mtipo[0] ? mtipo : f->tipo;
      c.epgId = c.tvgNome = c.url = c.ua = c.ref = "";
      c.sug = semCat;
      sLtvF[fl].total++;
      { int gi = sGrupoInfo(fl, gen, 1);
        if (nsel && !(semCat && ajustes_livetv_sugerir()) && !nomeNaLista(gen, sel, nsel)) { if (gi >= 0) sLtvF[fl].gi[gi].entra = 0; }
        else if (sCanalAdd(fl, &c)) (*novos)++;
        else {
          int resolvido = 0;
          if (semCat && ajustes_livetv_sugerir()) livecat_secao(nome, &resolvido);
          // Cached negatives must not hide candidates on later pages after reload.
          if (resolvido) (*novos)++;
        } }
    }
    p = js_prox(fim);
  }
  free(corpo);
  return n;
}

// With no filter rules, later pages cannot change a full final visible list.
// Count eligible rows without applying caps or discarding staging candidates.
static int sCotaCompleta(void) {
  int nc = 0, n = 0, visiveis[GUIACORTE_MAX_CAT];
  if (sInventarioCat[0]) return sInventarioMais;
  if (sFltLive.modo != GF_TODAS || sFltLive.nCat || sFltLive.nCan || sPendentes || sCategoriasTemOcultas()) return 0;
  for (int i = 0; i < sNCanais; i++) {
    const GCanal *g = &sCanais[i];
    int j;
    // Suggestions can still move a channel into or out of an admitted row.
    if (g->sug && ajustes_livetv_sugerir()) return 0;
    for (j = 0; j < nc && visiveis[j] != g->cat; j++) {}
    if (j == nc) {
      if (nc == GUIACORTE_MAX_CAT) continue;
      visiveis[nc++] = g->cat;
    }
    if (++n >= sTeto) return 1;
  }
  return 0;
}
static int carregarCatalogo(const GFonte *f, int fl) {
  int ok = 0;
  for (int skip = 0; cargaAtual() && !sCotaCompleta(); skip += G_PAGINA) {
    int novos = 0, n = lerPagina(f, fl, skip, &novos);
    if (n <= 0) break;
    ok = 1;
    sLtvF[fl].respondeu = 1;
    if (n < G_PAGINA) break;
  }
  if (sNCanais >= G_CANDIDATOS_MAX || sPendentes >= G_MAX_PENDENTES || sCotaCompleta())
    sLtvF[fl].cortada = 1;
  return ok;
}

// --- Stalker, Xtream e M3U para o staging ---------------------------------------
static int stCanal(const StalkerCanal *st, void *u) {
  int fl = *(int *)u;
  GCru c = {0};
  if (!cargaAtual()) return 0;
  const char *g = st->categoria[0] ? st->categoria : categoriaPorNome(st->nome);
  int semCat = !g;
  if (!g) g = "Outros";
  sGrupoInfo(fl, g, 1);
  sLtvF[fl].total++;
  if (sCanalPorId(fl, st->id) >= 0) return 1;
  c.id = st->id; c.nome = st->nome; c.logo = st->logo; c.grupo = g;
  c.desc = c.epgId = c.tvgNome = c.url = c.ua = c.ref = "";
  c.sug = semCat;
  sCanalAdd(fl, &c);
  return 1;
}
static int stalkerCarregar(const IptvConta *conta) {
  char chave[LTV_CHAVE]; contaChave(conta, chave, sizeof chave);
  int fl = sFonteChave(chave, LTV_STALKER, conta->nome, "");
  if (fl < 0) return 0;
  int n = stalker_fonte_canais_cb(conta->id, stCanal, &fl, cargaSessao);
  sLtvF[fl].respondeu = n > 0;
  return n;
}

typedef struct {
  int fl, nsel;
  const char *sel[256];
  int *giDe;            // ordem do painel -> indice em gi
  int nOrd;
} XtCtx;
static void xtCat(const char *nome, int ordem, int n, void *u) {
  XtCtx *x = u;
  if (ordem >= x->nOrd) {
    int *no = realloc(x->giDe, sizeof(int) * (size_t)(ordem + 64));
    if (!no) return;
    x->giDe = no; x->nOrd = ordem + 64;
  }
  x->giDe[ordem] = sGrupoInfo(x->fl, nome, n);
  sLtvF[x->fl].total += n;
}
static int xtCanal(const XtreamItem *it, void *u) {
  XtCtx *x = u;
  GFonteLtv *F = &sLtvF[x->fl];
  int gi;
  GCru c;
  gi = it->catOrdem >= 0 && it->catOrdem < x->nOrd ? x->giDe[it->catOrdem] : -1;

  memset(&c, 0, sizeof c);
  c.id = it->id; c.nome = it->nome; c.logo = it->logo; c.epgId = it->epgId;
  c.grupo = it->semCategoria ? categoriaPorNome(it->nome) : it->categoria;
  c.sug = !c.grupo || !c.grupo[0];
  // Infer and count before filtering: the selector includes omitted channels too.
  if (it->semCategoria && !c.sug &&
      (gi < 0 || strcmp(c.grupo, F->gi[gi].nome))) {
    int inferred = sGrupoInfo(x->fl, c.grupo, 1);
    if (inferred >= 0) {
      if (gi >= 0 && F->gi[gi].n > 0) F->gi[gi].n--;
      gi = inferred;
    }
  }
  c.numero = it->num;
  c.desc = c.tvgNome = c.url = c.ua = c.ref = "";
  sCanalAdd(x->fl, &c);
  return 1;
}
static int xtreamCarregar(const IptvConta *conta) {
  char chave[LTV_CHAVE]; contaChave(conta, chave, sizeof chave);
  XtCtx x;
  int total = 0;
  memset(&x, 0, sizeof x);
  x.fl = sFonteChave(chave, LTV_XTREAM, conta->nome, "");
  if (x.fl < 0) return 0;
  x.nsel = sGruposEscolhidos(x.fl, x.sel, 256);
  xtream_fonte_canais_cb(conta->id, xtCat, xtCanal, &x, &total, cargaSessao);
  sXtFalha = xtream_fonte_estado(conta->id, &sXtHttp);
  sLtvF[x.fl].erro = sXtFalha;
  sLtvF[x.fl].respondeu = sXtFalha == XT_OK;
  free(x.giDe);
  return sLtvF[x.fl].nCanais;
}

// M3U POR URL (M9), em fluxo: a lista de 20 mil canais nunca existe inteira.
// Grupos na ordem em que aparecem; passou do teto, o resto conta mas nao entra.
typedef struct {
  int fl, nsel; const char *sel[256];
  int nIdDup, conta; unsigned contaId; const char *origem;
  int *indice; unsigned capIndice;
} M3uCtx;
static uint64_t m3uHash(uint64_t h,const char *s) {
  for(const unsigned char *p=(const unsigned char*)s;*p;p++)h=(h^*p)*UINT64_C(1099511628211);
  return h;
}
static int m3uCanal(const LtvM3uCanal *m, void *u) {
  M3uCtx *x = u;
  char id[80], chave[300];
  const char *g = m->grupo && m->grupo[0] ? m->grupo : categoriaPorNome(m->nome);
  int semCat = !g, gi;
  GCru c;
  if (!g) g = "Outros";
  gi = sGrupoInfo(x->fl, g, 1);
  sLtvF[x->fl].total++;
  if (x->nsel && !(semCat && ajustes_livetv_sugerir()) && !nomeNaLista(g, x->sel, x->nsel)) { if (gi >= 0) sLtvF[x->fl].gi[gi].entra = 0; return 1; }
  // ID ESTAVEL sem a URL (que pode ter token que muda e credencial): tvg-id,
  // nome e grupo. Repetido exato e o mesmo canal duas vezes na lista.
  snprintf(chave, sizeof chave, "%s|%s|%s", m->tvgId, m->nome, g);
  unsigned slot=0;
  if(x->conta) {
    uint64_t h=m3uHash(UINT64_C(14695981039346656037),x->origem);
    h=m3uHash(h,m->tvgId[0]?m->tvgId:m->url);h=m3uHash(h,m->nome);h=m3uHash(h,m->grupo);
    char original[32];snprintf(original,sizeof original,"nvm3u:%016" PRIx64,h);
    iptvid_montar("nvm3u:",x->contaId,original,id,sizeof id);
    slot=(unsigned)h&(x->capIndice-1);
    while(x->indice[slot] && strcmp(sCanais[x->indice[slot]-1].id,id))slot=(slot+1)&(x->capIndice-1);
    if(x->indice[slot] && strcmp(sCanais[x->indice[slot]-1].url,m->url)) {
      h=m3uHash(h,m->url);snprintf(original,sizeof original,"nvm3u:%016" PRIx64,h);
      iptvid_montar("nvm3u:",x->contaId,original,id,sizeof id);
      slot=(unsigned)h&(x->capIndice-1);
      while(x->indice[slot] && strcmp(sCanais[x->indice[slot]-1].id,id))slot=(slot+1)&(x->capIndice-1);
    }
    if(x->indice[slot])return 1;
  } else snprintf(id, sizeof id, "m3u:%s:%08lx", sLtvF[x->fl].chave, ltv_hash(chave));
  memset(&c, 0, sizeof c);
  c.id = id; c.nome = m->nome; c.logo = m->logo; c.epgId = m->tvgId; c.tvgNome = m->tvgNome;
  c.url = m->url; c.ua = m->ua; c.ref = m->referer; c.grupo = g; c.numero = m->chno;
  c.desc = "";
  c.sug = semCat;
  int antes=sNCanais;
  if(sCanalAdd(x->fl, &c) && x->conta && sNCanais>antes)x->indice[slot]=antes+1;
  return 1;
}
static int alimentarM3u(const char *d, size_t n, void *u) { return cargaAtual() && ltv_m3u_alimentar((LtvM3u *)u, d, n); }
static int m3uCarregar(const LtvUrl *u, const char *chave, const char *epgUrl) {
  static LtvM3u p;     // ~10 KB: fora da pilha do fio
  M3uCtx x;
  int st = 0, ok;
  long long bytes = 0;
  memset(&x, 0, sizeof x);
  x.fl = chave ? sFonteChave(chave, LTV_M3U, u->nome, "") : sFonte(LTV_M3U, u->url, u->nome, "");
  if (x.fl < 0) return 0;
  x.nsel = sGruposEscolhidos(x.fl, x.sel, 256);
  x.conta=chave && fonteConta(&sLtvF[x.fl],&x.contaId);x.origem=u->url;
  if(x.conta) {
    x.capIndice=1;while(x.capIndice<(unsigned)G_CANDIDATOS_MAX*2+1)x.capIndice*=2;
    x.indice=calloc(x.capIndice,sizeof *x.indice);if(!x.indice)return 0;
  }
  ltv_m3u_iniciar(&p, m3uCanal, &x);
  ok = rede_baixar_fluxo(u->url, 120, NULL, alimentarM3u, &p, 200LL << 20, &st, &bytes);
  ltv_m3u_fim(&p);
  free(x.indice);
  snprintf(sLtvF[x.fl].urlTvg, sizeof sLtvF[x.fl].urlTvg, "%s", epgUrl && epgUrl[0] ? epgUrl : p.urlTvg);
  // A chave da fonte e o tamanho; nunca a URL.
  printf("[ltv] m3u %s: %s, %lld KB, %d canais na lista, %d retidos%s\n", sLtvF[x.fl].chave,
         ok ? "ok" : "falhou", bytes / 1024, p.nCanais, sLtvF[x.fl].nCanais,
         p.urlTvg[0] ? ", com url-tvg" : "");
  if (!ok && st) printf("[ltv] m3u %s: HTTP %d\n", sLtvF[x.fl].chave, st);
  fflush(stdout);
  sLtvF[x.fl].respondeu = ok;
  if (!ok && !sLtvF[x.fl].nCanais) sLtvF[x.fl].erro = 1;
  return sLtvF[x.fl].nCanais;
}

// --- CACHE POR FONTE EM DISCO (M1) -----------------------------------------------
//
// POR QUE: o guia so existia depois de UMA volta completa pela rede. A ultima
// lista de cada fonte fica em disco e e o que a tela mostra NA HORA; a volta pela
// rede continua em fundo. POR FONTE (e nao um arquivo so): a fonte que falha
// hoje mostra a lista de ontem dela, sem apagar nem segurar as outras.
//
// ltv-<chave>-p<perfil>.bin com as strings em comprimento+bytes (nada de
// struct crua: uma build que mude GCanal le o mesmo arquivo), e
// ltv-indice-p<perfil>.txt com as chaves na ordem da barra.
#define GC_MAGIA  0x324c564eu   /* "NVL2" */
#define GC_VERSAO 3u
static int cacheLido;          // ja tentou ler nesta sessao
static unsigned long assinaturaPublicada;

static void cacheNomeFonte(const char *chave, char *d, size_t n) {
  int p = perfis_ativo();
  if (p <= 0) snprintf(d, n, "ltv-%s.bin", chave);
  else        snprintf(d, n, "ltv-%s-p%d.bin", chave, p);
}
static void cacheNomeIndice(char *d, size_t n) {
  int p = perfis_ativo();
  if (p <= 0) snprintf(d, n, "ltv-indice.txt");
  else        snprintf(d, n, "ltv-indice-p%d.txt", p);
}

static int cStr(FILE *f, const char *s) {
  size_t k = s ? strlen(s) : 0;
  unsigned short m = (unsigned short)(k > 65535 ? 65535 : k);
  return fwrite(&m, 2, 1, f) == 1 && (!m || fwrite(s, 1, m, f) == m);
}
static int cInt(FILE *f, long long v) { return fwrite(&v, sizeof v, 1, f) == 1; }

static int cacheGrupoComparar(const void *a, const void *b) {
  return strcmp(((const GGrupoInfo *)a)->nome, ((const GGrupoInfo *)b)->nome);
}

static void cacheCru(int f, const char *grupo, const GCru *c) {
  if (sCacheErro[f]) return;
  size_t bytes = 2 * sizeof(long long) + 11 * sizeof(unsigned short);
  const char *fields[] = {grupo, c->id, c->nome, c->logo, c->desc, c->epgId,
                         c->tvgNome, c->url, c->ua, c->ref, c->tipo};
  for (size_t i = 0; i < sizeof fields / sizeof fields[0]; i++) {
    size_t n = fields[i] ? strlen(fields[i]) : 0;
    bytes += n > 65535 ? 65535 : n;
  }
  if (bytes > G_RAW_BYTES_MAX - sCacheBytes) {
    sCacheCortada[f] = sLtvF[f].cortada = 1;
    return;
  }
  if (!sCache[f]) sCache[f] = tmpfile();
  FILE *fp = sCache[f];
  if (!fp || !(cStr(fp, grupo) && cInt(fp, c->numero) && cStr(fp, c->id) &&
      cStr(fp, c->nome) && cStr(fp, c->logo) && cStr(fp, c->desc) &&
      cStr(fp, c->epgId) && cStr(fp, c->tvgNome) && cStr(fp, c->url) &&
      cStr(fp, c->ua) && cStr(fp, c->ref) && cStr(fp, c->tipo) && cInt(fp, c->sug))) {
    sCacheErro[f] = 1;
    return;
  }
  sCacheN[f]++;
  sCacheBytes += bytes;
}

static void cacheFonteGravar(int f) {
  char nome[64], caminho[600], tmp[620];
  const GFonteLtv *F = &ltvF[f];
  FILE *fp;
  GGrupoInfo *originais = NULL;
  int ok, i;
  // A truncated raw prefix may omit admitted channels; save the displayed list instead.
  int raw = sCache[f] != NULL && !sCacheCortada[f];
  if (sCacheErro[f]) return;
  cacheNomeFonte(F->chave, nome, sizeof nome);
  if (!dados_caminho(caminho, sizeof caminho, nome)) return;
  snprintf(tmp, sizeof tmp, "%s.tmp", caminho);
#ifdef __EMSCRIPTEN__
  dados_fs_travar();
#endif
  fp = fopen(tmp, "wb");
  if (fp) {
    ok = cInt(fp, GC_MAGIA) && cInt(fp, GC_VERSAO) && cInt(fp, F->tipo) && cInt(fp, raw ? sCacheN[f] : F->nCanais) &&
         cInt(fp, F->ngi) && cInt(fp, F->total) && cInt(fp, F->cortada) && cInt(fp, F->atualizada) &&
         cStr(fp, F->nome) && cStr(fp, F->base) && cStr(fp, F->urlTvg);
    // Reverse regrouping once per changed channel; leave published counts intact.
    for (i = 0; ok && !raw && i < nCanais && F->ngi; i++) {
      const GCanal *c = &canais[i];
      if (c->fonte != f || !c->sug || !strcmp(c->grupoOriginal, cats[c->cat].nome)) continue;
      if (!originais) {
        originais = malloc(sizeof *originais * (size_t)F->ngi);
        if (!originais) { ok = 0; break; }
        memcpy(originais, F->gi, sizeof *originais * (size_t)F->ngi);
        qsort(originais, (size_t)F->ngi, sizeof *originais, cacheGrupoComparar);
      }
      GGrupoInfo key = {.nome = c->grupoOriginal};
      GGrupoInfo *g = bsearch(&key, originais, (size_t)F->ngi, sizeof *originais, cacheGrupoComparar);
      if (g) g->n++;
      key.nome = cats[c->cat].nome;
      g = bsearch(&key, originais, (size_t)F->ngi, sizeof *originais, cacheGrupoComparar);
      if (g) g->n--;
    }
    for (i = 0; ok && i < F->ngi; i++) {
      const GGrupoInfo *g = originais ? bsearch(&F->gi[i], originais, (size_t)F->ngi,
                                               sizeof *originais, cacheGrupoComparar) : &F->gi[i];
      ok = cStr(fp, F->gi[i].nome) && cInt(fp, g->n) && cInt(fp, F->gi[i].entra);
    }
    if (raw) {
      char buf[8192]; size_t n;
      if (fflush(sCache[f]) != 0) ok = 0;
      rewind(sCache[f]);
      while (ok && (n = fread(buf, 1, sizeof buf, sCache[f])) > 0)
        ok = fwrite(buf, 1, n, fp) == n;
      if (ferror(sCache[f])) ok = 0;
    }
    for (i = 0; !raw && ok && i < nCanais; i++) {
      const GCanal *c = &canais[i];
      if (c->fonte != f) continue;
      ok = cStr(fp, c->sug ? c->grupoOriginal : cats[c->cat].nome) && cInt(fp, c->numero) && cStr(fp, c->id) && cStr(fp, c->nome) &&
           cStr(fp, c->logo) && cStr(fp, c->desc) && cStr(fp, c->epgId) && cStr(fp, c->tvgNome) &&
           cStr(fp, c->url) && cStr(fp, c->ua) && cStr(fp, c->ref) && cStr(fp, c->tipo) && cInt(fp, c->sug);
    }
    ok = (fclose(fp) == 0) && ok;
    if (!ok || rename(tmp, caminho) != 0) remove(tmp);
  }
  free(originais);
#ifdef __EMSCRIPTEN__
  dados_fs_liberar();
  dados_marcar_sujo(0);
#endif
}

typedef struct { const char *p, *fim; int ok; } CLeitor;
static long long lInt(CLeitor *l) {
  long long v = 0;
  if (!l->ok || l->p + sizeof v > l->fim) { l->ok = 0; return 0; }
  memcpy(&v, l->p, sizeof v); l->p += sizeof v;
  return v;
}
static const char *lStr(CLeitor *l, char *buf, size_t n) {
  unsigned short m;
  if (!l->ok || l->p + 2 > l->fim) { l->ok = 0; return ""; }
  memcpy(&m, l->p, 2); l->p += 2;
  if (l->p + m > l->fim) { l->ok = 0; return ""; }
  { size_t k = m < n - 1 ? m : n - 1; memcpy(buf, l->p, k); buf[k] = 0; }
  l->p += m;
  return buf;
}

// Le a fonte `chave` do disco PARA O STAGING (do fio, ou do arranque com o fio
// parado). Devolve quantos canais entraram, -1 sem cache que sirva.
static int cacheFonteLer(const char *chave) {
  char nome[64], caminho[600], *b = NULL;
  long n = 0;
  FILE *fp;
  CLeitor l;
  int tipo, nc, ngi, i, fl, total, cortada, versao, antes = sNCanais;
  long long quando;
  static char s1[1024], s2[1024], s3[1024];
  cacheNomeFonte(chave, nome, sizeof nome);
  if (!dados_caminho(caminho, sizeof caminho, nome)) return -1;
#ifdef __EMSCRIPTEN__
  dados_fs_travar();
#endif
  fp = fopen(caminho, "rb");
  if (fp) {
    fseek(fp, 0, SEEK_END); n = ftell(fp); fseek(fp, 0, SEEK_SET);
    if (n > 0 && (b = malloc((size_t)n)) != NULL && (long)fread(b, 1, (size_t)n, fp) != n) { free(b); b = NULL; }
    fclose(fp);
  }
#ifdef __EMSCRIPTEN__
  dados_fs_liberar();
#endif
  if (!b) return -1;
  l.p = b; l.fim = b + n; l.ok = 1;
  if (lInt(&l) != GC_MAGIA) { free(b); return -1; }
  versao = (int)lInt(&l);
  if (versao != 2 && versao != GC_VERSAO) { free(b); return -1; }
  tipo = (int)lInt(&l); nc = (int)lInt(&l); ngi = (int)lInt(&l);
  total = (int)lInt(&l); cortada = (int)lInt(&l); quando = lInt(&l);
  lStr(&l, s1, sizeof s1); lStr(&l, s2, sizeof s2); lStr(&l, s3, sizeof s3);
  if (!l.ok || nc < 0 || ngi < 0) { free(b); return -1; }
  // A chave vem do nome do arquivo; o tipo e o nome de exibicao, de dentro.
  fl = sFonteChave(chave, tipo, s1, s2);
  if (fl < 0) { free(b); return -1; }
  sLtvF[fl].atualizada = quando;
  sLtvF[fl].deCache = 1;
  snprintf(sLtvF[fl].urlTvg, sizeof sLtvF[fl].urlTvg, "%s", s3);
  for (i = 0; i < ngi && l.ok; i++) {
    int gi = sGrupoInfo(fl, lStr(&l, s1, sizeof s1), 0), cnt = (int)lInt(&l), en = (int)lInt(&l);
    if (gi >= 0) { sLtvF[fl].gi[gi].n = cnt; sLtvF[fl].gi[gi].entra = (unsigned char)en; }
  }
  for (i = 0; i < nc && l.ok; i++) {
    static char g[256], id[256], nm[256], lg[1024], ds[1024], ei[256], tn[256], ur[4096], ua[512], rf[512];
    GCru c;
    memset(&c, 0, sizeof c);
    c.grupo = lStr(&l, g, sizeof g); c.numero = (int)lInt(&l);
    c.id = lStr(&l, id, sizeof id); c.nome = lStr(&l, nm, sizeof nm); c.logo = lStr(&l, lg, sizeof lg);
    c.desc = lStr(&l, ds, sizeof ds); c.epgId = lStr(&l, ei, sizeof ei); c.tvgNome = lStr(&l, tn, sizeof tn);
    c.url = lStr(&l, ur, sizeof ur); c.ua = lStr(&l, ua, sizeof ua); c.ref = lStr(&l, rf, sizeof rf);
    char tipoCanal[16]; c.tipo = lStr(&l, tipoCanal, sizeof tipoCanal);
    if (versao >= 3) c.sug = (int)lInt(&l);
    if (l.ok) sCanalAdd(fl, &c);
  }
  sLtvF[fl].total = total > sLtvF[fl].nCanais ? total : sLtvF[fl].nCanais;
  sLtvF[fl].cortada = cortada || sLtvF[fl].cortada;
  free(b);
  return sNCanais - antes;
}

static void cacheGravar(void) {
  char nome[48], buf[G_MAX_LTV * 32];
  size_t u = 0;
  int i;
  for (i = 0; i < nLtvF; i++) {
    if (!ltvF[i].deCache && (sCacheN[i] > 0 || ltvF[i].nCanais > 0))
      cacheFonteGravar(i);
    if (sCache[i]) { fclose(sCache[i]); sCache[i] = NULL; }
    sCacheN[i] = 0;
    u += (size_t)snprintf(buf + u, sizeof buf - u, "%s\n", ltvF[i].chave);
  }
  buf[u] = 0;
  cacheNomeIndice(nome, sizeof nome);
  dados_gravar(nome, buf);
  printf("[guia] cache gravado: %d canais em %d fonte(s)\n", nCanais, nLtvF);
  fflush(stdout);
}

// Check installed sources directly: disabling an add-on must preserve its saved data.
static int chaveConfigurada(const char *ch) {
  LtvUrl us[LTV_URLS_MAX];
  char atual[LTV_CHAVE];
  int i, nu;
  IptvConta contas[IPTV_MAX_CONTAS];
  int nc = iptv_contas(contas, IPTV_MAX_CONTAS);
  for (i = 0; i < nc; i++) {
    contaChave(&contas[i], atual, sizeof atual);
    if (!strcmp(atual, ch)) return 1;
  }
  for (i = 0; i < addons_n(); i++) {
    const char *b = addons_base(i);
    if (!b || !b[0]) continue;
    ltv_chave_fonte(LTV_ADDON, b, atual, sizeof atual);
    if (!strcmp(atual, ch)) return 1;
  }
  nu = ltv_urls_copiar(us, LTV_URLS_MAX);
  for (i = 0; i < nu; i++) {
    if (us[i].tipo != LTV_URL_M3U) continue;
    ltv_chave_fonte(LTV_M3U, us[i].url, atual, sizeof atual);
    if (!strcmp(atual, ch)) return 1;
  }
  return 0;
}

static void focoValido(void);
static void empacotar(void);
static void epgExtrasMontar(void);
static int gpAberto;
static int focoFontes, fontesCol;
static Uint32 fontesOkDesde; static int fontesOkLongo;
static int fopAberto, fopFoco;            // opcoes da fonte
static GCanal *fopCanal;

// Apaga o arquivo de cache de UMA fonte deste perfil.
static void cacheFonteApagar(const char *ch) {
  char nome[64], caminho[600], *t;
  cacheNomeIndice(nome, sizeof nome);
  t = dados_ler(nome);
  if (t) {
    char *p = t, *w = t;
    while (*p) {
      size_t k = strcspn(p, "\r\n"), linha = k;
      while (p[linha] == '\r' || p[linha] == '\n') linha++;
      if (strlen(ch) != k || strncmp(p, ch, k)) { memmove(w, p, linha); w += linha; }
      p += linha;
    }
    *w = 0; dados_gravar(nome, t); free(t);
  }
  cacheNomeFonte(ch, nome, sizeof nome);
  if (dados_caminho(caminho, sizeof caminho, nome)) remove(caminho);
  snprintf(nome, sizeof nome, "epg-x-%s-p%d.bin", ch, perfis_ativo() > 0 ? perfis_ativo() : 1);
  if (dados_caminho(caminho, sizeof caminho, nome)) remove(caminho);
#ifdef __EMSCRIPTEN__
  dados_marcar_sujo(0);
#endif
}

// Fonte que saiu da configuracao (addon removido, Xtream/M3U apagado): some
// os canais dela e o cache em disco NA HORA. Chamada por Ajustes/sync no fio
// de desenho.
void guia_fonte_esquecer(const char *chave) {
  int i, w;
  char ch[LTV_CHAVE];
  snprintf(ch, sizeof ch, "%s", chave);
  chave = ch;   // O chamador pode apontar para ltvF, que sera compactado.
  if (fioVivo) { cargaCancelada = 1; recarregarPend = 1; }
  cacheFonteApagar(chave);
  if (!favLido) favLer();
  for (i = 0, w = 0; i < nFav; i++)
    if (strcmp(fav[i].chave, chave)) fav[w++] = fav[i];
  if (w != nFav) { nFav = w; favGravar(); }
  for (i = 0; i < nLtvF; i++)
    if (!strcmp(ltvF[i].chave, chave)) {
      int k, w;
      // Source slots and channel pointers become invalid during compaction.
      gpAberto = fopAberto = focoFontes = fontesCol = 0;
      fontesOkDesde = fontesOkLongo = 0;
      fopCanal = NULL;
      // Spool records contain source/group indexes from the old source array.
      if (inventarioDisco) fclose(inventarioDisco);
      inventarioDisco = NULL; inventarioCortado = 0;
      fInventarioCat[0] = 0;
      for (k = 0, w = 0; k < nInventario; k++)
        if (strcmp(inventario[k].fonte, chave)) inventario[w++] = inventario[k];
      nInventario = w;
      for (k = 0, w = 0; k < nCanais; k++)
        if (canais[k].fonte != i) canais[w++] = canais[k];
      nCanais = w;
      for (k = 0; k < nCanais; k++) if (canais[k].fonte > i) canais[k].fonte--;
      for (k = 0; k < nCats; k++) if (cats[k].fonte > i) cats[k].fonte--;
      free(ltvF[i].gi); ltvF[i].gi = NULL;
      memmove(&ltvF[i], &ltvF[i + 1], sizeof ltvF[0] * (size_t)(nLtvF - i - 1));
      nLtvF--;
      break;
    }
  if (filtroChave[0] && !strcmp(filtroChave, chave)) {
    filtro = G_FILTRO_TODAS; filtroChave[0] = 0;
  }
  empacotar();
  focoValido();
  assinaturaPublicada = 0;
  if (!strcmp(chave, "xt")) xtepg_limpar();
  epgExtrasMontar();
  printf("[guia] fonte %s esquecida: %d canais ficam\n", chave, nCanais);
  fflush(stdout);
}

// Assinatura INDEPENDENTE DA ORDEM (soma de hashes por canal).
static unsigned long assinaturaDe(const GCanal *c, int n, const GCat *cs, const GFonteLtv *fs) {
  unsigned long soma = 0;
  int i;
  for (i = 0; i < n; i++) {
    unsigned long h = ltv_hash(c[i].id) ^ (ltv_hash(fs[c[i].fonte].chave) * 31UL);
    if (c[i].cat >= 0) h ^= ltv_hash(cs[c[i].cat].nome) * 7UL;
    soma += h & 0xffffffffUL;
  }
  return soma ^ (unsigned long)n;
}

// --- o fio de carga ----------------------------------------------------------------
static int tetoCarga = 30000;
static char sNomeAddon[64];
static const char *nomeDoAddon(const char *base) {
  int i;
  for (i = 0; i < addons_n(); i++)
    if (addons_base(i) && !strcmp(addons_base(i), base)) {
      snprintf(sNomeAddon, sizeof sNomeAddon, "%s", addons_nome(i));
      return sNomeAddon;
    }
  return "";
}

// Failed source: recover yesterday's list from disk. Successful filtered-empty
// results remain empty. Cached sources are degraded; missing cache means offline.
static void fonteVaziaRecupera(int tipo, const char *ref, const char *nome, const char *base) {
  char ch[LTV_CHAVE];
  int i, r;
  ltv_chave_fonte(tipo, ref, ch, sizeof ch);
  for (i = 0; i < sNLtvF && strcmp(sLtvF[i].chave, ch); i++) {}
  if (i < sNLtvF && (sLtvF[i].nCanais > 0 || sLtvF[i].respondeu)) return;
  r = cacheFonteLer(ch);
  for (i = 0; i < sNLtvF && strcmp(sLtvF[i].chave, ch); i++) {}
  if (i == sNLtvF) i = sFonte(tipo, ref, nome, base);
  if (i < 0) return;
  if (r > 0) {
    sLtvF[i].saude = LTV_SAUDE_DEGRADADA;
    printf("[ltv] fonte %s (%s) nao respondeu: fica a lista do cache, %d canais\n",
           ch, ltv_tipo_nome(tipo), r);
  } else {
    sLtvF[i].saude = LTV_SAUDE_FORA;
    if (!sLtvF[i].erro) sLtvF[i].erro = 1;
    printf("[ltv] fonte %s (%s) nao respondeu e nao ha cache\n", ch, ltv_tipo_nome(tipo));
  }
  fflush(stdout);
}

// SECAO SUGERIDA (Jev, via livecat.c) para o canal que nao veio com categoria
// nem casou na heuristica local. Mexe so nesses (g->sug): a categoria que a
// lista trouxe nunca troca. O canal sai do grupo-sobra ("Outros" ou o nome do
// catalogo) para a secao; o grupo que esvaziar some em empacotar(). Roda no fio
// de carga, antes de publicar, e so se a pessoa deixou ligado.
static void sugerirSecoes(void) {
  const char **nomes = NULL, **sec = NULL;
  unsigned char *sem = NULL;
  int *idx = NULL, i, n = 0;
  if (!ajustes_livetv_sugerir()) return;
  for (i = 0; i < sNCanais; i++) if (sCanais[i].sug) n++;
  if (!n) return;
  nomes = malloc(sizeof *nomes * (size_t)n); sec = malloc(sizeof *sec * (size_t)n);
  sem = malloc((size_t)n); idx = malloc(sizeof *idx * (size_t)n);
  if (nomes && sec && sem && idx) {
    int k = 0;
    for (i = 0; i < sNCanais; i++)
      if (sCanais[i].sug) { idx[k] = i; nomes[k] = sCanais[i].nome; sem[k] = 1; k++; }
    if (livecat_sugerir(nomes, sem, n, sec, cargaAtual) > 0 && cargaAtual())
      for (k = 0; k < n; k++) {
        int cat;
        if (!sec[k]) continue;
        cat = sCatDe(sCanais[idx[k]].fonte, sec[k]);
        if (cat >= 0) {
          GCanal *g = &sCanais[idx[k]];
          if (cat == g->cat) continue;
          int gi = sGrupoInfo(g->fonte, sCats[g->cat].nome, 0);
          int novo = sGrupoInfo(g->fonte, sec[k], 1);
          if (novo < 0) continue;
          if (gi >= 0 && sLtvF[g->fonte].gi[gi].n > 0) sLtvF[g->fonte].gi[gi].n--;
          g->cat = cat;
        }
      }
  }
  free(nomes); free(sec); free(sem); free(idx);
}

static void *fioGuia(void *u) {
  int ok = 0, i, nu;
  LtvUrl urls[LTV_URLS_MAX];
  (void)u;
  stagingZerar();
  sTeto = tetoCarga;
  sFalhas = 0; sXtFalha = XT_OK; sXtHttp = 0;
  // Fontes das fileiras primeiro — zero rede extra. A sonda de manifestos
  // completa com o que a home nao montou. ADDON DESLIGADO NAO ENTRA.
  sondaManifestos();
  // ADDONS: uma fonte da barra por addon (base), com todos os catalogos dele.
  for (i = 0; cargaAtual() && i < sNFontes; i++) {
    int fl = sFonte(LTV_ADDON, sFontes[i].base, nomeDoAddon(sFontes[i].base), sFontes[i].base);
    if (fl < 0) continue;
    if (carregarCatalogo(&sFontes[i], fl)) ok = 1;
  }
  // Fonte de addon sem canal nenhum (todos os catalogos falharam): o cache.
  for (i = 0; i < sNFontes; i++) {
    int fl = sFonte(LTV_ADDON, sFontes[i].base, "", sFontes[i].base);
    if (fl >= 0 && !sLtvF[fl].nCanais) fonteVaziaRecupera(LTV_ADDON, sFontes[i].base, sLtvF[fl].nome, sFontes[i].base);
  }
  // Native accounts retain the registry order, distinct IDs and cache per source.
  for (i = 0; cargaAtual() && i < nCargaContas; i++) {
    const IptvConta *c = &cargaContas[i];
    char chave[LTV_CHAVE]; int n = 0, tipo;
    if (!c->ativa) continue;
    contaChave(c, chave, sizeof chave);
    tipo = c->tipo == IPTV_XTREAM ? LTV_XTREAM : c->tipo == IPTV_STALKER ? LTV_STALKER : LTV_M3U;
    if (c->tipo == IPTV_XTREAM && xtream_fonte_configurado(c->id)) n = xtreamCarregar(c);
    else if (c->tipo == IPTV_STALKER && stalker_fonte_configurado(c->id)) n = stalkerCarregar(c);
    else if (c->tipo == IPTV_M3U) {
      LtvUrl u = {.tipo=LTV_URL_M3U}; char epgUrl[1024];
      snprintf(u.nome, sizeof u.nome, "%s", c->nome);
      if (!m3u_fonte_urls(c->id, u.url, sizeof u.url, epgUrl, sizeof epgUrl)) continue;
      n = m3uCarregar(&u, chave, epgUrl);
    } else continue;
    if (n > 0) ok = 1;
    else {
      int fl = sFonteChave(chave, tipo, c->nome, "");
      if (fl>=0 && !sLtvF[fl].respondeu) {
        int r=cacheFonteLer(chave);
        sLtvF[fl].saude=r>0?LTV_SAUDE_DEGRADADA:LTV_SAUDE_FORA;
      }
    }
  }
  nu = nCargaUrls; memcpy(urls, cargaUrls, sizeof urls);
  for (i = 0; cargaAtual() && i < nu; i++) {
    int duplicada = 0;
    if (urls[i].tipo != LTV_URL_M3U) continue;
    // The latest URL manager remains available. A URL already managed as an
    // account is loaded once; disabling that account must not resurrect it.
    for (int k = 0; k < nCargaContas; k++) if (cargaContas[k].tipo == IPTV_M3U) {
      char lista[1024], epgUrl[1024];
      if (m3u_fonte_urls(cargaContas[k].id, lista, sizeof lista, epgUrl, sizeof epgUrl) && !strcmp(lista, urls[i].url)) duplicada = 1;
    }
    if (duplicada) continue;
    if (m3uCarregar(&urls[i], NULL, NULL) > 0) ok = 1;
    else fonteVaziaRecupera(LTV_M3U, urls[i].url, urls[i].nome, "");
  }
  if (cargaAtual()) sugerirSecoes();
  for (i = 0; i < sNLtvF; i++) if (sLtvF[i].nCanais > 0) ok = 1;
  // FONTE ACHADA E NENHUMA PAGINA RESPONDEU tambem e "nao respondeu".
  if (sNFontes > 0 && !ok) sFalhas++;
  sOk = ok || sFiltrados > 0;
  free(addonsCarga); addonsCarga = NULL; nAddonsCarga = 0;
  atomic_store_explicit(&pendPronto, 1, memory_order_release);
  return NULL;
}

// Ordem das linhas: FONTE na ordem da barra, e dentro dela a do provedor (ou
// A-Z quando a pessoa pediu para aquela fonte, decisao 3).
static int *ordAz;   // por fonte: 1 = A-Z
static int cmpCat(const void *a, const void *b) {
  const GCat *x = &cats[*(const int *)a], *y = &cats[*(const int *)b];
  int rx=grupoOrdFonte[x->fonte], ry=grupoOrdFonte[y->fonte];
  if (rx != ry) {
    if (!ajustes_guia_ordem()) {
      int c = strcasecmp(x->nome, y->nome);
      if (c) return c;
    }
    return rx - ry;
  }
  if (x->fonte != y->fonte) return x->fonte - y->fonte;
  if (ordAz && ordAz[x->fonte]) {
    int c = strcasecmp(x->nome, y->nome);
    if (c) return c;
  }
  return x->ordem - y->ordem;
}

// Missing/failed logos borrow loaded M3U metadata, then matched XMLTV, then
// another channel variant. Index once when publishing, not once per grid row.
// Keep all donors so download failures can advance without republishing.
// Channel indexes and keys are rebuilt after every repack.
typedef struct { unsigned long long h; int i; } GLogoK;
static GLogoK *logoM3u, *logoVar;
static int nLogoM3u, nLogoVar;
static unsigned long long hLogo(const char *tag, const char *s) {
  unsigned long long h = 1469598103934665603ULL;
  for (; *tag; tag++) { h ^= (unsigned char)*tag; h *= 1099511628211ULL; }
  for (; *s; s++) { h ^= (unsigned char)tolower((unsigned char)*s); h *= 1099511628211ULL; }
  return h;
}
static int cmpLogoK(const void *a, const void *b) {
  const GLogoK *x = a, *y = b;
  return x->h < y->h ? -1 : x->h > y->h ? 1 : x->i - y->i;
}
static int logoFalhou(const char *logo) {
  if (!tex_falhou(logo)) return 0;
  // Keep the loader's bounded retries alive while this draw uses a fallback.
  (void)tex_obter_larg(logo, 92.0f);
  return 1;
}
static int logoAchar(const GLogoK *v, int n, unsigned long long h, int excluir) {
  int lo = 0, hi = n;
  while (lo < hi) { int m = (lo + hi) / 2; if (v[m].h < h) lo = m + 1; else hi = m; }
  for (; lo < n && v[lo].h == h; lo++)
    if (v[lo].i != excluir && !logoFalhou(canais[v[lo].i].logo)) return v[lo].i;
  return -1;
}
static void logosEmprestar(void) {
  GLogoK *m3u = malloc(sizeof *m3u * (size_t)(nCanais * 2 + 1)), *todos = malloc(sizeof *todos * (size_t)(nCanais + 1));
  int nm = 0, nt = 0, i;
  char b[160];
  free(logoM3u); free(logoVar);
  logoM3u = logoVar = NULL; nLogoM3u = nLogoVar = 0;
  if (!m3u || !todos) { free(m3u); free(todos); return; }
  for (i = 0; i < nCanais; i++) {
    GCanal *c = &canais[i];
    nv_nome_base(c->nome, b, sizeof b);
    c->logoNome = b[0] ? hLogo("n", b) : 0;
    c->logoId = c->epgId && c->epgId[0] ? hLogo("i", c->epgId) : 0;
    if (!c->logo || !c->logo[0]) continue;
    if (c->logoNome) todos[nt++] = (GLogoK){ c->logoNome, i };
    if (c->fonte < 0 || c->fonte >= nLtvF || ltvF[c->fonte].tipo != LTV_M3U) continue;
    if (c->logoId) m3u[nm++] = (GLogoK){ c->logoId, i };
    if (c->logoNome) m3u[nm++] = (GLogoK){ c->logoNome, i };
  }
  qsort(m3u, (size_t)nm, sizeof *m3u, cmpLogoK);
  qsort(todos, (size_t)nt, sizeof *todos, cmpLogoK);
  logoM3u = m3u; nLogoM3u = nm;
  logoVar = todos; nLogoVar = nt;
}

// Ordena os canais por grupo (estavel na ordem de chegada) para que cada
// linha seja uma janela contigua, e refaz Favoritos, contas e a vista. Roda ao
// publicar e ao desligar um addon no painel (que tira canais sem esperar o fio).
static void empacotar(void) {
  canaisRevisao++;
  grupoNContas = iptv_contas(grupoContas, IPTV_MAX_CONTAS);
  for(int f=0;f<nLtvF;f++)grupoOrdFonte[f]=fonteOrdem(f);
  int i, w, *ordem = NULL, *mapa = NULL, *pos = NULL;
  int az[G_MAX_LTV];
  GCanal *tmp;
  // Canal de addon desligado nao fica.
  for (i=0;i<nLtvF;i++) {
    int ordem=fonteOrdem(i);
    if(ordem<grupoNContas)snprintf(ltvF[i].nome,sizeof ltvF[i].nome,"%s",grupoContas[ordem].nome);
  }
  nLogoM3u = nLogoVar = 0;  // Invalidate indexes even if repacking runs out of memory.
  // Apply current visibility rules even when an offline refresh keeps this list.
  for (i = 0, w = 0; i < nCanais; i++)
    if (fonteAtiva(canais[i].fonte) && (!canais[i].base[0] || (baseLigada(canais[i].base) && !baseOculta(canais[i].base))) &&
        !guiafiltro_canal_oculto_fonte(&fltLive, ltvF[canais[i].fonte].chave, canais[i].id) &&
        !(canais[i].cat >= 0 && guiafiltro_cat_oculta(&fltLive, cats[canais[i].cat].nome)))
      canais[w++] = canais[i];
  nCanais = w;
  for (i = 0; i < nLtvF; i++) {
    LtvPref *p = ltv_pref(ltvF[i].chave, 0);
    az[i] = p ? p->az : 0;
    ltvF[i].fusoS = p ? p->fusoMin * 60 : 0;
    ltvF[i].nCanais = 0; ltvF[i].nGrupos = 0;
  }
  ordAz = az;
  { int *no = realloc(catN, sizeof(int) * (size_t)(nCats + 1)), *no2;
    if (no) catN = no;
    no2 = realloc(catIni, sizeof(int) * (size_t)(nCats + 1));
    if (no2) catIni = no2; }
  ordem = malloc(sizeof(int) * (size_t)(nCats + 1));
  mapa = malloc(sizeof(int) * (size_t)(nCats + 1));
  pos = malloc(sizeof(int) * (size_t)(nCats + 1));
  tmp = malloc(sizeof(GCanal) * (size_t)(nCanais + 1));
  if (!catN || !catIni || !ordem || !mapa || !pos || !tmp) {
    free(ordem); free(mapa); free(pos); free(tmp); ordAz = NULL; return;
  }
  // Grupo sem canal sai; os outros em ordem.
  for (i = 0; i < nCats; i++) catN[i] = 0;
  for (i = 0; i < nCanais; i++) if (canais[i].cat >= 0) catN[canais[i].cat]++;
  for (i = 0, w = 0; i < nCats; i++) if (catN[i]) ordem[w++] = i;
  qsort(ordem, (size_t)w, sizeof(int), cmpCat);
  ordAz = NULL;
  for (i = 0; i < nCats; i++) mapa[i] = -1;
  for (i = 0; i < w; i++) mapa[ordem[i]] = i;
  { GCat *nc = malloc(sizeof(GCat) * (size_t)(w + 1));
    if (!nc) { free(ordem); free(mapa); free(pos); free(tmp); return; }
    for (i = 0; i < w; i++) nc[i] = cats[ordem[i]];
    free(cats); cats = nc; nCats = w; capCats = w + 1; }
  for (i = 0; i < nCanais; i++) canais[i].cat = canais[i].cat >= 0 ? mapa[canais[i].cat] : -1;
  for (i = 0; i < nCats; i++) catN[i] = 0;
  for (i = 0; i < nCanais; i++) if (canais[i].cat >= 0) catN[canais[i].cat]++;
  if (nCats) catIni[0] = 0;
  for (i = 1; i < nCats; i++) catIni[i] = catIni[i - 1] + catN[i - 1];
  memcpy(pos, catIni, sizeof(int) * (size_t)nCats);
  for (i = 0, w = 0; i < nCanais; i++)
    if (canais[i].cat >= 0) { tmp[pos[canais[i].cat]++] = canais[i]; w++; }
  if(w) memcpy(canais, tmp, sizeof(GCanal) * (size_t)w);
  nCanais = w;
  free(ordem); free(mapa); free(pos); free(tmp);
  for (i = 0; i < nCanais; i++) if (canais[i].fonte >= 0 && canais[i].fonte < nLtvF) ltvF[canais[i].fonte].nCanais++;
  for (i = 0; i < nCats; i++) if (cats[i].fonte >= 0 && cats[i].fonte < nLtvF) ltvF[cats[i].fonte].nGrupos++;
  logosEmprestar();
  favAplicar();
  nFavOrd = 0;
  for (i = 0; i < nCanais && nFavOrd < G_MAX_FAV; i++)
    if (canais[i].fav) favOrd[nFavOrd++] = i;
  vistaMontar();
}

// Resolve final sections before spending either visible-list quota.
static void selecionarSecoes(void) {
  int w = 0, nc = 0, visiveis[GUIACORTE_MAX_CAT];
  for (int f = 0; f < sNLtvF; f++) {
    const char *sel[256];
    int ns = sGruposEscolhidos(f, sel, 256);
    for (int i = 0; i < sLtvF[f].ngi; i++)
      if (ns) sLtvF[f].gi[i].entra = nomeNaLista(sLtvF[f].gi[i].nome, sel, ns);
  }
  for (int i = 0; i < sNCanais; i++) {
    GCanal *g = &sCanais[i];
    const char *sel[256];
    int ns = sGruposEscolhidos(g->fonte, sel, 256);
    if (ns && !nomeNaLista(sCats[g->cat].nome, sel, ns)) continue;
    if (sCategoriaOculta(g->fonte,sCats[g->cat].nome) || guiafiltro_cat_oculta(&sFltLive, sCats[g->cat].nome) ||
        guiafiltro_canal_oculto_fonte(&sFltLive, sLtvF[g->fonte].chave, g->id)) { sFiltrados++; continue; }
    sCanais[w++] = *g;
  }
  guiafiltro_dup_zerar(&sDup);
  sNCanais = w; w = 0;
  // One admission pass over final winners: category cap, then channel cap.
  for (int i = 0, j; i < sNCanais; i++) {
    GCanal *g = &sCanais[i];
    for (j = 0; j < nc && visiveis[j] != g->cat; j++) {}
    if ((j == nc && nc >= GUIACORTE_MAX_CAT) || w >= sTeto) {
      sLtvF[g->fonte].cortada = 1; continue;
    }
    if (j == nc) visiveis[nc++] = g->cat;
    sCanais[w++] = *g;
  }
  sNCanais = w;
}

// Copy only admitted channels, their fallbacks and source/group controls.
// Keep staging intact on allocation failure so the previous list can survive.
static int compactarStaging(void) {
  LtvArena texto = {0};
  GFonteLtv fontesNovas[G_MAX_LTV];
  int nc = 0, na = 0, keptAlts = 0;
  for (int k = 0; k < sNAlts; k++)
    for (int i = 0; i < sNCanais; i++)
      if (sAlts[k].dono == sCanais[i].id) { keptAlts++; break; }
  int *mapa = malloc(sizeof *mapa * (size_t)(sNCats + 1));
  GCanal *cs = malloc(sizeof *cs * (size_t)(sNCanais ? sNCanais : 1));
  GCat *ct = malloc(sizeof *ct * (size_t)(sNCanais + 1));
  GAlt *as = malloc(sizeof *as * (size_t)(keptAlts ? (keptAlts + 255) / 256 * 256 : 1));
  memcpy(fontesNovas, sLtvF, sizeof fontesNovas);
  for (int f = 0; f < sNLtvF; f++) fontesNovas[f].gi = NULL;
  if (!mapa || !cs || !ct || !as) goto falhou;
#define COPY_TEXT(dst, src) do { \
    const char *old = (src); \
    (dst) = ltv_dup(&texto, old); \
    if (old && old[0] && !(dst)[0]) goto falhou; \
  } while (0)
  for (int f = 0; f < sNLtvF; f++) {
    GFonteLtv *F = &fontesNovas[f];
    COPY_TEXT(F->base, sLtvF[f].base);
    if (!F->ngi) continue;
    F->gi = malloc(sizeof *F->gi * (size_t)((F->ngi + 63) / 64 * 64));
    if (!F->gi) goto falhou;
    memcpy(F->gi, sLtvF[f].gi, sizeof *F->gi * (size_t)F->ngi);
    for (int i = 0; i < F->ngi; i++) COPY_TEXT(F->gi[i].nome, sLtvF[f].gi[i].nome);
  }
  for (int i = 0; i < sNCats; i++) mapa[i] = -1;
  for (int i = 0; i < sNCanais; i++) {
    const GCanal *g = &sCanais[i];
    cs[i] = *g;
    if (mapa[g->cat] < 0) {
      mapa[g->cat] = nc;
      ct[nc] = sCats[g->cat];
      COPY_TEXT(ct[nc].nome, sCats[g->cat].nome);
      nc++;
    }
    cs[i].cat = mapa[g->cat];
    cs[i].base = fontesNovas[g->fonte].base;
#define CHANNEL_TEXT(field) COPY_TEXT(cs[i].field, g->field)
    CHANNEL_TEXT(id); CHANNEL_TEXT(nome); CHANNEL_TEXT(logo); CHANNEL_TEXT(desc);
    CHANNEL_TEXT(epgId); CHANNEL_TEXT(tvgNome); CHANNEL_TEXT(url);
    CHANNEL_TEXT(ua); CHANNEL_TEXT(ref); CHANNEL_TEXT(tipo); CHANNEL_TEXT(grupoOriginal);
#undef CHANNEL_TEXT
    for (int k = 0; k < sNAlts; k++) {
      if (sAlts[k].dono != g->id) continue;
      as[na].dono = cs[i].id;
      COPY_TEXT(as[na].id, sAlts[k].id);
      COPY_TEXT(as[na].nome, sAlts[k].nome);
      na++;
    }
  }
#undef COPY_TEXT
  free(mapa);
  free(sCanais); sCanais = cs; sCapCanais = sNCanais;
  free(sCats); sCats = ct; sNCats = nc; sCapCats = sNCanais + 1;
  free(sAlts); sAlts = as; sNAlts = na;
  fontesLtvSoltar(sLtvF, sNLtvF);
  memcpy(sLtvF, fontesNovas, sizeof sLtvF);
  ltv_arena_soltar(&sArena); sArena = texto;
  return 1;
falhou:
  free(mapa); free(cs); free(ct); free(as);
  fontesLtvSoltar(fontesNovas, sNLtvF);
  ltv_arena_soltar(&texto);
  return 0;
}

// O staging vira a lista publicada (fio parado). O que era publicado e solto.
static void adotarStaging(void) {
  selecionarSecoes();
  if (!compactarStaging()) { stagingZerar(); return; }
  fontesLtvSoltar(ltvF, nLtvF);
  ltv_arena_soltar(&arena);
  free(canais); free(cats); free(alts);
  if (inventarioDisco) fclose(inventarioDisco);
  inventarioDisco = sInventarioDisco; sInventarioDisco = NULL;
  inventarioCortado = sInventarioCortado;
  free(inventario); inventario = sInventario; nInventario = sNInventario;
  sInventario = NULL; sNInventario = 0;
  alts = sAlts; nAlts = sNAlts; sAlts = NULL; sNAlts = 0;
  canais = sCanais; nCanais = sNCanais; capCanais = sCapCanais;
  cats = sCats; nCats = sNCats; capCats = sCapCats;
  memcpy(ltvF, sLtvF, sizeof sLtvF); nLtvF = sNLtvF;
  arena = sArena;
  sCanais = NULL; sNCanais = sCapCanais = 0;
  sCats = NULL; sNCats = sCapCats = 0;
  memset(sLtvF, 0, sizeof sLtvF); sNLtvF = 0;
  memset(&sArena, 0, sizeof sArena);
  sPendentes = 0;
  empacotar();
}

// Le o cache do disco direto para a lista publicada (fio de desenho, antes de
// qualquer fio de carga). 1 se a tela ja tem lista.
static int cacheLer(void) {
  char nome[48], *t, *p;
  if (fioVivo) return 0;
  // Spotlight can reach the cache before Guide loads the active profile's URLs.
  ltv_urls_carregar(perfis_ativo());
  cacheNomeIndice(nome, sizeof nome);
  t = dados_ler(nome);
  if (!t) return 0;
  fltGarantir(); sFltLive = fltLive;categoriasCopiar();
  stagingZerar();
  sTeto = tetoCarga;
  for (p = t; *p; ) {
    char ch[LTV_CHAVE];
    size_t k = strcspn(p, "\r\n");
    // Fonte que nao existe mais na configuracao nao volta do cache (addon
    // removido, Xtream/M3U apagado, outra conta neste perfil). A que so
    // FALHOU a rede continua configurada e entra normal.
    if (k && k < sizeof ch) {
      memcpy(ch, p, k); ch[k] = 0;
      if (chaveConfigurada(ch)) cacheFonteLer(ch);
      else guia_fonte_esquecer(ch);
    }
    p += k; while (*p == '\r' || *p == '\n') p++;
  }
  free(t);
  if (!sNCanais) { stagingZerar(); empacotar(); focoValido(); return 0; }
  adotarStaging();
  assinaturaPublicada = assinaturaDe(canais, nCanais, cats, ltvF);
  printf("[guia] cache lido: %d canais em %d grupos, %d fonte(s)\n", nCanais, nCats, nLtvF);
  fflush(stdout);
  marco("guia: lista do cache na tela");
  return 1;
}

// As grades de cada fonte (M4/M9): o xmltv.php do Xtream, o url-tvg de cada
// M3U e os XMLTV por URL dos Ajustes, cada uma com o filtro dos canais dela (a
// por URL vale para todas). O epg.c so recarrega se URL ou fonte mudaram.
static void epgExtrasMontar(void) {
  EpgExtra x[EPG_MAX_EXTRAS];
  LtvUrl us[LTV_URLS_MAX];
  char urlsContas[G_MAX_LTV][1100];
  const char **ids, **nomes;
  int nx = 0, f, i, nu, ni = 0, nn = 0;
  ids = malloc(sizeof(char *) * (size_t)(nCanais + 1));
  nomes = malloc(sizeof(char *) * (size_t)(2 * nCanais + 1));
  if (!ids || !nomes) { free(ids); free(nomes); return; }
  for (f = 0; f < nLtvF && nx < EPG_MAX_EXTRAS; f++) {
    const char *url = NULL;
    int i0 = ni, n0 = nn;
    unsigned contaId=0;
    if (ltvF[f].tipo == LTV_XTREAM && fonteConta(&ltvF[f], &contaId) && xtream_fonte_xmltv(contaId, urlsContas[f], sizeof urlsContas[f])) url = urlsContas[f];
    else if (ltvF[f].tipo == LTV_M3U && ltvF[f].urlTvg[0]) url = ltvF[f].urlTvg;
    if (!url) continue;
    for (i = 0; i < nCanais; i++) {
      if (canais[i].fonte != f) continue;
      if (canais[i].epgId[0]) ids[ni++] = canais[i].epgId;
      nomes[nn++] = canais[i].nome;
      if (canais[i].tvgNome[0] && strcmp(canais[i].tvgNome, canais[i].nome)) nomes[nn++] = canais[i].tvgNome;
    }
    memset(&x[nx], 0, sizeof x[nx]);
    x[nx].chave = ltvF[f].chave; x[nx].url = url;
    x[nx].ids = ids + i0; x[nx].nIds = ni - i0;
    x[nx].nomes = nomes + n0; x[nx].nNomes = nn - n0;
    nx++;
  }
  nu = ltv_urls_copiar(us, LTV_URLS_MAX);
  { static char chaves[LTV_URLS_MAX][LTV_CHAVE];
    const char **todosIds = NULL, **todosNomes = NULL;
    int nti = 0, ntn = 0;
    for (i = 0; i < nu && nx < EPG_MAX_EXTRAS; i++) {
      if (us[i].tipo != LTV_URL_XMLTV) continue;
      if (!todosIds) {
        todosIds = malloc(sizeof(char *) * (size_t)(nCanais + 1));
        todosNomes = malloc(sizeof(char *) * (size_t)(nCanais + 1));
        if (!todosIds || !todosNomes) break;
        for (f = 0; f < nCanais; f++) {
          if (canais[f].epgId[0]) todosIds[nti++] = canais[f].epgId;
          todosNomes[ntn++] = canais[f].nome;
        }
      }
      snprintf(chaves[i], sizeof chaves[i], "x%08lx", ltv_hash(us[i].url));
      memset(&x[nx], 0, sizeof x[nx]);
      x[nx].chave = chaves[i]; x[nx].url = us[i].url;
      x[nx].ids = todosIds; x[nx].nIds = nti; x[nx].nomes = todosNomes; x[nx].nNomes = ntn;
      nx++;
    }
    epg_extras_definir(x, nx);
    free(todosIds); free(todosNomes); }
  free(ids); free(nomes);
}

static void epgCoberturaZerar(void);
static void publicar(void) {
  char idFoco[80] = "";
  unsigned long nova;
  int i, igual;
  // REDE VAZIA NAO APAGA A LISTA QUE HA: sem resposta nenhuma (todos fora), a
  // lista de ontem continua valendo mais que uma tela vazia.
  if (sNCanais == 0 && nCanais > 0 && !sFiltrados) {
    falhas = sFalhas; xtFalha = sXtFalha; xtHttp = sXtHttp;
    memcpy(sabe, sSabe, sizeof sSabe); nSabe = sNSabe;
    stagingZerar();
    printf("[guia] rede sem canal nenhum: fica a lista que estava\n");
    fflush(stdout);
    return;
  }
  nova = assinaturaDe(sCanais, sNCanais, sCats, sLtvF);
  igual = nCanais > 0 && nova == assinaturaPublicada;
  { GCanal *f = linhaItem(focoLin, focoCol);
    if (f) snprintf(idFoco, sizeof idFoco, "%s", f->id); }
  memcpy(fontes, sFontes, sizeof sFontes); nFontes = sNFontes;
  falhas = sFalhas; xtFalha = sXtFalha; xtHttp = sXtHttp;
  memcpy(sabe, sSabe, sizeof sSabe); nSabe = sNSabe;
  adotarStaging();
  assinaturaPublicada = nova;
  favMigrar();
  favAplicar();
  // O FOCO FICA NO CANAL: a lista nova (ou a mesma, recarregada) reordena por
  // dentro e o foco por indice cairia em outro canal.
  if (focoPend[0]) {
    char id[80];
    snprintf(id, sizeof id, "%s", focoPend);
    focoPend[0] = 0;
    guia_focar_id(id);
    focoPend[0] = 0;
  } else if (idFoco[0]) { guia_focar_id(idFoco); focoPend[0] = 0; }
  printf("[guia] %d canais em %d grupos, %d fonte(s), teto %d (%d favoritos)%s\n",
         nCanais, nCats, nLtvF, tetoCarga, nFavOrd, igual ? ", igual a da tela" : "");
  // Uma linha por fonte: chave e contas, nunca URL nem credencial (M6).
  for (i = 0; i < nLtvF; i++)
    printf("[ltv] fonte=%s tipo=%s canais=%d/%d grupos=%d%s%s%s\n", ltvF[i].chave,
           ltv_tipo_nome(ltvF[i].tipo), ltvF[i].nCanais, ltvF[i].total, ltvF[i].nGrupos,
           ltvF[i].cortada ? " cortada=sim" : "", ltvF[i].deCache ? " lista=cache" : "",
           ltvF[i].saude == LTV_SAUDE_FORA ? " estado=fora" : "");
  printf("[ltv] memoria da lista: %lu KB de texto + %lu KB de canais\n",
         (unsigned long)(ltv_arena_bytes(&arena) / 1024),
         (unsigned long)(sizeof(GCanal) * (size_t)capCanais / 1024));
  fflush(stdout);
  if (guiacorte_cortou(&corte))
    printf("[canais] n=%d de %d (corte: %s)\n", nCanais, guiacorte_total(&corte, nCanais),
           guiacorte_motivo(&corte));
  fflush(stdout);
  marco("guia: lista da rede publicada");
  if (nCanais > 0 || sFiltrados > 0) cacheGravar();
  epgPaisesEscolher();
  epgExtrasMontar();
  epgCoberturaZerar();
}

static void iniciarCarga(void) {
  pthread_t t;
  // Sem portaria por nFontes: a sonda de manifestos dentro do fio pode achar
  // catalogo de canal que nenhuma fileira montou.
  if (fioVivo) return;
  fltGarantir(); sFltLive = fltLive;categoriasCopiar();
  snprintf(sInventarioCat, sizeof sInventarioCat, "%s", fInventarioCat);
  sInventarioInicio = fInventarioInicio;
  // Copia no desenho: nenhum pedido relera as URLs ou os manifestos do
  // perfil novo enquanto a resposta do anterior ainda estiver em voo.
  cargaSessao = nv_perfil_sessao_ler(); cargaCancelada = 0;
  nCargaUrls = ltv_urls_copiar(cargaUrls, LTV_URLS_MAX);
  nCargaContas = iptv_contas(cargaContas, IPTV_MAX_CONTAS);
  nAddonsCarga = addons_n();
  addonsCarga = nAddonsCarga > 0 ? calloc((size_t)nAddonsCarga, sizeof *addonsCarga) : NULL;
  if (nAddonsCarga > 0 && !addonsCarga) { nAddonsCarga = 0; estado = G_FALHOU; return; }
  for (int i = 0; i < nAddonsCarga; i++) {
    GAddonCarga *a = &addonsCarga[i];
    snprintf(a->base, sizeof a->base, "%s", addons_base(i));
    snprintf(a->nome, sizeof a->nome, "%s", addons_nome(i));
    a->ativo = addons_ativo(i);
    a->nCatalogos = addons_catalogos_canal(i, a->catalogos, ADD_CANAL_MAX);
  }
  sNFontes = 0;
  for (int i = 0; i < nFontes && sNFontes < G_MAX_FONTE; i++)
    if (cargaBaseLigada(fontes[i].base)) sFontes[sNFontes++] = fontes[i];
  fioVivo = 1;
  estado = G_BAIXANDO;
  if (pthread_create(&t, NULL, fioGuia, NULL) != 0) {
    free(addonsCarga); addonsCarga = NULL; nAddonsCarga = 0;
    fioVivo = 0; estado = G_FALHOU;
  }
  else pthread_detach(t);
}

// A parte "carregar" da abertura, sem a parte "mostrar". O zap por CH+/-
// funciona com o guia nunca aberto: o primeiro toque dispara a carga e nao
// troca nada (a lista nao existe ainda), o seguinte ja troca.
static int perfilPrefs = -999;
void guia_carregar(void) {
  if (perfilPrefs != perfis_ativo()) {
    perfilPrefs = perfis_ativo();
    ltv_pref_carregar(perfilPrefs);
    ltv_urls_carregar(perfilPrefs);
  }
  if (!favLido) favLer();
  if (!modoLido) modoLer();
  if (!previewLido) previewLer();
  // O TETO DA TV (M2): pela plataforma e pela RAM que o tex_cache ja mediu.
  { long mem = 0;
    tex_orcamento_info(NULL, &mem, NULL, NULL);
    tetoCarga = ltv_teto_canais((int)ptv_plataforma(), mem); }
  // Sem fonte achada, tenta de novo a cada chamada: a descoberta da home pode
  // nao ter montado as fileiras ainda quando o primeiro CH+/- chega.
  if (!fontesOk) descobrirFontes();
  // A lista de ontem NA HORA; a de hoje vem atras. Ver o bloco do cache.
  if (!cacheLido) { cacheLido = 1; if (estado == G_PARADO && nCanais == 0) cacheLer(); }
  if (estado == G_PARADO || estado == G_FALHOU) {
    if (fioVivo) recarregarPend = 1;
    else iniciarCarga();
  }
  // As grades das fontes (Xtream, url-tvg, XMLTV por URL) ANTES da primeira
  // carga do EPG, com a lista do cache ja na tela: a grade nao carrega duas vezes.
  if (nCanais > 0) epgExtrasMontar();
  epgPaisesEscolher();
  epg_iniciar();
}

// --- estado de tela -------------------------------------------------------------
static int   aberta, overlay, querSair;
static int   focoLin, focoCol;
static float rolY, velY;
// Rolagem horizontal por linha (modo cartoes). Cresce com as linhas: com o teto
// de grupos por fonte nao ha mais um numero fixo de linhas.
static float *rolXv; static int capRolX;
static float *rolXde(int l) {
  static float zero;
  if (l < 0) return &zero;
  if (l >= capRolX) {
    int cap = l + 64, i;
    float *no = realloc(rolXv, sizeof(float) * (size_t)cap);
    if (!no) { zero = 0.0f; return &zero; }
    for (i = capRolX; i < cap; i++) no[i] = 0.0f;
    rolXv = no; capRolX = cap;
  }
  return &rolXv[l];
}
static float entrada;
static int   pediuCanal; static CatItem pedido;
// PAINEL DE CATEGORIAS (gaveta da esquerda). 0 = fechado; 1 = aberto por
// SEGURAR ↑↓ (a soltura entra na categoria escolhida); 2 = aberto pelo chip
// "Categorias" (fica aberto, OK entra, Voltar fecha). catFoco e a linha em
// foco DENTRO do painel — o guia so muda de secao ao confirmar.
static int    catAberto, catFoco;
// A LISTA DE CATEGORIAS FICA (dono, 01/10/2026): aberta por segurar a seta
// (modo 1), ao soltar ela vira a lista navegavel (modo 2) em vez de entrar
// sozinha. Cima/baixo andam, OK ou direita entram, esquerda/Voltar fecham;
// sem tecla por G_CAT_OCIOSO_MS ela fecha sem mudar nada.
#define G_CAT_OCIOSO_MS 8000u
static Uint32 catUlt;
static float  catAnim, catRol, catVelRol;
static int    dirSeg;                 // SDLK_UP/DOWN segurado, 0 = solto
static Uint32 dirTick, ultNavCat;

// OK longo = favorito.
static Uint32 okDesde; static int okLongo;

// --- preview de video: e uma SESSAO DO PLAYER no preview -------------------
// DESDE 25/09/2026 o preview nao tem pipeline proprio. Ele era um fio que
// buscava UMA url e chamava video_tocar direto, e por isso OK no canal que ja
// estava tocando no preview RECARREGAVA tudo pelo player (queixa do dono:
// "ao inves de so deixar em tela cheia, ele comeca a carregar de novo").
// Agora o preview e o player em modo "mini no guia" (player.h): o guia PEDE
// (guia_pediu_preview), o app abre a sessao com player_manter_mini, e o
// mesmo fluxo vai da tela cheia ao preview e volta so trocando o retangulo.
//
// QUEM ESTA NO PREVIEW:
//   - guia aberto sem canal no ar, preview ligado: o canal FOCADO, depois de
//     o foco descansar G_PREVIEW_ESPERA_MS;
//   - guia aberto COM canal no ar (Voltar da tela cheia, guia cheio aberto da
//     faixa, guia aberto da barra com o PiP no ar): o canal que ESTAVA
//     tocando, e ele fica — navegar nao troca o que toca, como no YouTube TV
//     e no TiviMate. So OK em outro canal troca (e ai ele vai a tela cheia).
//     `previewPreso` guarda isso.
// Descanso do foco antes de pedir o preview. 600 e nao os 350 do fio antigo:
// agora cada pedido e uma busca de fonte do player, e passar a seta por tres
// canais nao deve abrir tres buscas.
#define G_PREVIEW_ESPERA_MS 600u
static int      previewPreso;           // canal no ar adotado: nao segue o foco
static char     previewId[80];          // canal pedido/tocando no preview
static Uint32   previewFocoMudou;       // quando o foco chegou no canal atual
static int      previewUltLin = -1, previewUltCol = -1;
static int      pedPreview, pedPararPreview, pedRestaurar, pedGuiaCheio;
static CatItem  pedPreviewItem;

// Foco no CABECALHO (controle segmentado + Addons). 0 = nas linhas.
static int   focoTopo, topoCol;
static int   opcoesAberto, opcoesFoco;
static float animTopo[G_TOPO_N];

// HEROI: o canal que ele mostra e a opacidade do texto. Trocar de canal
// derruba o texto a 40% e a mola o traz de volta (~150 ms) — a troca le como
// transicao, nao como pisca. Nao parte do zero: segurando a seta, o texto
// ficaria apagado o tempo todo.
static char  heroId[80];
static float heroA = 1.0f;

// ANEL DE FOCO DA GRADE, que DESLIZA de celula a celula em vez de saltar. O
// alvo e medido no desenho (e la que se sabe onde cada celula caiu) em
// coordenadas de CONTEUDO (y sem a rolagem), e a mola persegue o alvo no
// proprio desenho, com o dt tirado do relogio que guia_desenhar recebe. Um
// alvo longe demais (troca de modo, salto de secao) e assumido de uma vez.
static GfxRect focoAnel, focoAnelAlvo;
static int     focoAnelOk, focoAnelTem;
static Uint32  focoAnelTick;

// Modo lista: deslocamento da janela de tempo (min, multiplo de 30) e a
// rolagem vertical propria — a de cartoes anda em fileiras, esta em linhas de
// altura diferente, e misturar as duas numa mola so puxava a lista para o
// lugar errado na troca de modo.
static int   janelaDesl;
static float rolL, velL;

// Painel de addons por cima do guia.
static int   painel, paFoco, paMexeu;
// 0 = painel de addons; 1 = filtros da Live TV (qualidade, categorias, canais).
// Mesma gaveta, mesmo foco/rolagem/toque: so a lista de itens muda.
static int   paModo;
static Uint32 paOkTick;
typedef struct { char id[GF_ID], nome[GF_NOME], fonte[LTV_CHAVE]; int oculto, n; } GFItem;
static GFItem *fIt;            // categorias (nivel 0) ou canais da categoria (nivel 1)
static int   fN, fNivel, fSujo, fFocoCat, fProximo;
static char  fCatNome[GF_NOME];
#define G_PF_ROW 100.0f
// 1 quando nesta visita alguem LIGOU um addon (ou instalou um novo). Ligar so
// traz catalogo baixando da rede, entao o fechar pede desc_repetir() — o ciclo
// completo. Desligar nao precisa de rede: as fileiras do addon saem por filtro
// (desligada() em descoberta.c), e desc_remontar_fileiras() refaz a home em
// memoria sem refazer Trakt, sem reler manifestos e sem re-baixar catalogos.
// Sem esta bandeira, desligar um addon no Guia disparava um ciclo de ~20 s
// que re-baixava TODOS os manifestos (inclusive de addons desligados) por nada.
static int   paLigou;
static float paRol, paVelRol;
#ifdef NV_TOUCH_UI
static ToqueRolagem toqueGradeY, toqueLista, toqueTempo, toqueCat, toqueAddon, toqueBusca, toqueVazio;
static float rolVazio;
static ToqueRolagem *toqueGradeX; static int capToqueGrade;
static ToqueRolagem *toqueXde(int l) {
  static ToqueRolagem vazio;
  if (l < 0) return &vazio;
  if (l >= capToqueGrade) {
    int cap = l + 64;
    ToqueRolagem *v = realloc(toqueGradeX, sizeof *v * (size_t)cap);
    if (!v) return &vazio;
    memset(v + capToqueGrade, 0, sizeof *v * (size_t)(cap - capToqueGrade));
    toqueGradeX = v; capToqueGrade = cap;
  }
  return &toqueGradeX[l];
}
static int toqueLinha = -1;
static float toqueTempoMin, toquePpm;
static ToqueRolagem *toqueAtual;
static int toqueGuiaRolar(const PonteiroRolagem *e);
static int toqueCategoriaRolar(const PonteiroRolagem *e);
static int toqueAddonRolar(const PonteiroRolagem *e);
static int toqueBuscaRolar(const PonteiroRolagem *e);
static void toqueGuiaRetomar(void);
static void toqueLimpar(void) {
  toquerol_limpar(&toqueGradeY); toquerol_limpar(&toqueLista); toquerol_limpar(&toqueTempo);
  toquerol_limpar(&toqueCat); toquerol_limpar(&toqueAddon); toquerol_limpar(&toqueBusca);
  toquerol_limpar(&toqueVazio);
  for (int l = 0; l < capToqueGrade; l++) toquerol_limpar(&toqueGradeX[l]);
  toqueLinha = -1;
  toqueAtual = NULL;
}
#endif
static float tempoDesloc(void) {
#ifdef NV_TOUCH_UI
  if (toquerol_livre(&toqueTempo)) return toqueTempoMin;
#endif
  return (float)janelaDesl;
}
// Linha do painel que falhou ao instalar (-1 = nenhuma) e o motivo, para a
// frase ser a certa: "a conta esta cheia" e "nao foi possivel" sao coisas
// diferentes para quem esta no sofa.
static int   paErro = -1, paErroCheio;
// recarregarPend foi declarada junto do worker: uma abertura nova pode
// aguardar a carga cancelada devolver seu staging, sem bloquear a tela.

// Quando foi a ultima tentativa de carga com o guia vazio. Ver guia_atualizar.
#define G_RETENTAR_MS 10000u
static Uint32 ultTentativa;

// Instantaneo da ABERTURA do overlay. O firmware repete o KEYDOWN da tecla
// segurada, e a tecla que ABRE (azul, ou `s` no Tizen) e a mesma que FECHA:
// sem este repouso, segurar o botao um pouco a mais abria e fechava o overlay
// em ~130 ms — o sintoma relatado de "o guia nao aparece quando esta tocando".
static Uint32 overlayDesde;
#define G_OVERLAY_REP_MS 400

// ENGATILHA OS VIZINHOS DO FOCO.
//
// Zapear no guia esperava a consulta a TODOS os addons a cada canal. O cache de
// fontes (fontecache.c) busca os vizinhos enquanto a pessoa decide, e so quando
// a busca principal esta ociosa — pedido real tem prioridade e o prefetch cede.
//
// UM DE CADA LADO, e nao a fileira toda: cada canal engatilhado e uma consulta
// de rede a addon que a pessoa pode nunca abrir. Um vizinho cobre o caso comum
// (descer/subir um) sem transformar navegar em tempestade de requisicao.
//
// ISTO ENCURTA O CASO COMUM, NAO CONSERTA LENTIDAO. Ha relato aberto de fonte
// que demora minutos; o prefetch esconde parte dele e nao substitui achar a
// causa.
static void engatilharVizinhos(void) {
  GCanal *antes  = linhaItem(focoLin, focoCol - 1);
  GCanal *depois = linhaItem(focoLin, focoCol + 1);
  fontecache_engatilhar(antes  ? antes->id  : NULL, antes  ? antes->base  : NULL,
                        depois ? depois->id : NULL, depois ? depois->base : NULL);
}

static void focoValido(void) {
  int l = nLinhas();
  if (focoLin >= l) focoLin = l - 1;
  if (focoLin < 0) focoLin = 0;
  if (focoCol >= linhaN(focoLin)) focoCol = linhaN(focoLin) - 1;
  if (focoCol < 0) focoCol = 0;
  engatilharVizinhos();
}

// --- preview de video: pedidos ao app -----------------------------------------
static void canalParaItem(const GCanal *c, CatItem *dst);
static GCanal *canalPorId(const char *id) {
  int i;
  if (!id || !id[0]) return NULL;
  for (i = 0; i < nCanais; i++) if (!strcmp(canais[i].id, id)) return &canais[i];
  return NULL;
}
// O MESMO id pode existir em duas fontes (M1): o lembrete e o Spotlight guardam
// a base do addon, que diz qual. Base vazia ou que nao casa: o primeiro.
static GCanal *canalPorIdBase(const char *id, const char *base) {
  int i;
  GCanal *primeiro = NULL;
  if (!id || !id[0]) return NULL;
  for (i = 0; i < nCanais; i++)
    if (!strcmp(canais[i].id, id)) {
      if (!base || !base[0] || !strcmp(canais[i].base, base)) return &canais[i];
      if (!primeiro) primeiro = &canais[i];
    }
  return primeiro;
}
// VARIANTES DO MESMO CANAL (Live TV > Resolucao principal). No Xtream cada
// resolucao e um canal da lista ("RO| CINEMAX FHD", "RO| CINEMAX HD"); o
// nome-base (livetv_regras.h) junta os que sao o mesmo canal. O proprio canal
// vem primeiro; os outros, na ordem da lista. So Xtream: no addon as
// resolucoes ja chegam como fontes do mesmo canal.
int guia_variantes(const char *id, GuiaVariante *out, int max) {
  GCanal *c = canalPorId(id);
  char base[160], outra[160];
  int i, j, k, n = 0;
  if (!c || max < 1 || !xtream_e_id(c->id)) return 0;
  nv_nome_base(c->nome, base, sizeof base);
  snprintf(out[n].id, sizeof out[n].id, "%s", c->id);
  snprintf(out[n].nome, sizeof out[n].nome, "%s", c->nome);
  out[n].altura = nv_res_do_texto(c->nome);
  n++;
  if (!base[0]) return n;
  for (i = 0; i < nCanais && n < max; i++) {
    if (&canais[i] == c || canais[i].fonte != c->fonte ||
        guiafiltro_canal_oculto_fonte(&fltLive, ltvF[canais[i].fonte].chave, canais[i].id) || !xtream_e_id(canais[i].id)) continue;
    nv_nome_base(canais[i].nome, outra, sizeof outra);
    if (strcmp(base, outra)) continue;
    snprintf(out[n].id, sizeof out[n].id, "%s", canais[i].id);
    snprintf(out[n].nome, sizeof out[n].nome, "%s", canais[i].nome);
    out[n].altura = nv_res_do_texto(canais[i].nome);
    n++;
  }
  // Reservas que o filtro "Preferir 4K" escondeu, da maior para a menor.
  for (i = 0, k = n; i < nAlts; i++) {
    int altura;
    if (strcmp(alts[i].dono, c->id) || guiafiltro_canal_oculto_fonte(&fltLive, ltvF[c->fonte].chave, alts[i].id)) continue;
    altura = nv_res_do_texto(alts[i].nome);
    // Keep the best reserves while scanning, before applying the caller's cap.
    for (j = k; j < n && out[j].altura >= altura; j++) {}
    if (j >= max) continue;
    if (n < max) n++;
    memmove(&out[j + 1], &out[j], sizeof *out * (size_t)(n - j - 1));
    snprintf(out[j].id, sizeof out[j].id, "%s", alts[i].id);
    snprintf(out[j].nome, sizeof out[j].nome, "%s", alts[i].nome);
    out[j].altura = altura;
  }
  return n;
}

// OS CANAIS DO TESTE do diagnostico da Live TV (M8). Nos logs 2.0.x, 20 de 40
// recomendacoes nao mediram nada: a fileira em foco estava vazia ou era curta.
// Agora: a fileira em foco a partir do canal focado e, se ela tiver menos que
// `max`, o resto vem da MESMA FONTE do canal focado, um de cada grupo (grupos
// diferentes costumam ser servidores diferentes). Sem foco valido, a primeira
// fonte com canal.
int guia_canais_para_teste(GuiaVariante *out, char bases[][600], int max, char *grupo, size_t ng) {
  int l = focoLin, c0 = focoCol, n = 0, k, total, f = -1, g;
  GCanal *foco;
  if (l < 0 || l >= nLinhas()) l = 0;
  total = linhaN(l);
  if (c0 < 0 || c0 >= total) c0 = 0;
  foco = linhaItem(l, c0);
  if (foco) f = foco->fonte;
  else if (nCanais) f = canais[0].fonte;
  if (grupo && ng) snprintf(grupo, ng, "%s", nLinhas() ? linhaNome(l) : "");
  #define POE(g_) do { GCanal *g2 = (g_); int j_, ja_ = 0;                          \
      for (j_ = 0; j_ < n; j_++) if (!strcmp(out[j_].id, g2->id)) ja_ = 1;          \
      if (!ja_) { snprintf(out[n].id, sizeof out[n].id, "%s", g2->id);              \
        snprintf(out[n].nome, sizeof out[n].nome, "%s", g2->nome);                  \
        out[n].altura = nv_res_do_texto(g2->nome);                                  \
        if (bases) snprintf(bases[n], 600, "%s", g2->base); n++; } } while (0)
  for (k = 0; k < total && n < max && n < 3; k++) {
    GCanal *g1 = linhaItem(l, (c0 + k) % total);
    if (g1) POE(g1);
  }
  for (g = 0; g < nCats && n < max && f >= 0; g++)
    if (cats[g].fonte == f && catN[g] > 0) POE(&canais[catIni[g]]);
  for (k = 3; k < total && n < max; k++) {
    GCanal *g1 = linhaItem(l, (c0 + k) % total);
    if (g1) POE(g1);
  }
  #undef POE
  if (grupo && ng && f >= 0 && f < nLtvF && n > total)
    snprintf(grupo, ng, "%s", ltvF[f].nome);
  return n;
}

// O diagnostico grava o que mediu NA FONTE (M8), e nao so nos Ajustes globais:
// cada provedor pede um formato/modo diferente (#158).
int guia_fonte_do_canal(const char *id, char *chave, size_t n) {
  GCanal *c = canalPorId(id);
  if (!c || c->fonte < 0 || c->fonte >= nLtvF) return 0;
  snprintf(chave, n, "%s", ltvF[c->fonte].chave);
  return 1;
}
void guia_fonte_gravar_diag(const char *id, int formato, int modo, int proxy) {
  char ch[LTV_CHAVE];
  LtvPref *p;
  if (!guia_fonte_do_canal(id, ch, sizeof ch) || !(p = ltv_pref(ch, 1))) return;
  p->formato = formato; p->modo = modo; p->proxy = proxy;
  ltv_pref_gravar();
  printf("[ltv] fonte=%s diagnostico gravado: formato=%d modo=%d proxy=%d\n", ch, formato, modo, proxy);
  fflush(stdout);
}
int guia_proxy_do_canal(const char *id, int padrao) {
  char ch[LTV_CHAVE];
  LtvPref *p;
  if (!guia_fonte_do_canal(id, ch, sizeof ch) || !(p = ltv_pref(ch, 0)) || p->proxy < 0) return padrao;
  return p->proxy;
}
int guia_modo_do_canal(const char *id, int padrao) {
  char ch[LTV_CHAVE];
  LtvPref *p;
  if (!guia_fonte_do_canal(id, ch, sizeof ch) || !(p = ltv_pref(ch, 0)) || p->modo < 0) return padrao;
  return p->modo;
}

static int pedLivetvDiag;
int guia_pediu_livetv_diag(void) { int v = pedLivetvDiag; pedLivetvDiag = 0; return v; }

static char pedidoBase[600], pedidoTipo[16];
// The video hole opens only for the focused channel, by ID: duplicate
// entries of the same channel (two sources, favorites) share the id but not
// the GCanal*, and canalPorId() always returns the first.
static int previewFocoEh(const char *id) {
  return previewId[0] && id && id[0] && !strcmp(previewId, id);
}

static void previewPedir(GCanal *c) {
  if (!c || !c->id[0]) return;
  canalParaItem(c, &pedPreviewItem);
  snprintf(pedidoBase, sizeof pedidoBase, "%s", c->base);
  snprintf(pedidoTipo, sizeof pedidoTipo, "%s", c->tipo);
  snprintf(previewId, sizeof previewId, "%s", c->id);
  pedPreview = 1;
}
// Para o preview (sair do guia, desligar o preview). Quem para e o app: a
// sessao e do player.
static void previewParar(void) {
  pedPreview = 0;
  previewId[0] = 0;
  previewPreso = 0;
  pedPararPreview = 1;
}

// DICA DE PRIMEIRA VEZ: ESQUERDA LEVA AS OPCOES DO GUIA. Pedido do dono: a
// barra de cima (Categorias, Cartoes/Lista, Addons, Preview) so e alcancada
// por ESQUERDA no primeiro canal da fileira (no modo lista, com a grade no
// "agora") ou por CIMA na primeira fileira, e ninguem descobre isso sozinho.
// Uma pilula discreta sob a barra, nas primeiras G_DICA_VEZES aberturas do guia
// completo ou ate a pessoa usar a ESQUERDA para subir uma vez; some em
// G_DICA_MS ou na primeira tecla. A contagem fica em guia-dica.txt ("N" ou
// "usou"), nesta TV.
#define G_DICA_ARQ   "guia-dica.txt"
#define G_DICA_VEZES 3
#define G_DICA_MS    4000u
static Uint32 dicaDesde;      // 0 = fora da tela
static void dicaGravar(const char *v) { dados_gravar(G_DICA_ARQ, v); }
static void dicaUsou(void) {
  if (dicaDesde) dicaDesde = 0;
  dicaGravar("usou\n");
}
static void dicaTalvez(void) {
  char *t = dados_ler(G_DICA_ARQ), buf[16];
  int n = 0;
  if (t && !strncmp(t, "usou", 4)) { free(t); return; }
  if (t) { n = atoi(t); free(t); }
  if (n >= G_DICA_VEZES) return;
  snprintf(buf, sizeof buf, "%d\n", n + 1);
  dicaGravar(buf);
  dicaDesde = SDL_GetTicks();
  if (!dicaDesde) dicaDesde = 1;
}

void guia_abrir(void) {
#ifdef NV_TOUCH_UI
  toqueLimpar();
  rolVazio = 0.0f;
#endif
  guia_carregar();
  if (!previewLido) previewLer();
  aberta = 1; querSair = 0; entrada = 0.0f;
  focoTopo = 0; painel = 0; opcoesAberto = 0;
  previewPreso = 0; previewId[0] = 0;
  previewUltLin = previewUltCol = -1; previewFocoMudou = SDL_GetTicks();
  focoValido();
  dicaTalvez();
}

// O CANAL NO AR VEM PARA O GUIA. O app ja mandou o player encolher para o
// preview (player_minimizar_para_guia ou player_mini_no_guia); aqui o guia so
// fica sabendo quem esta tocando, foca a linha dele e para de seguir o foco.
void guia_adotar_canal(const char *id) {
  if (!id || !id[0]) return;
  snprintf(previewId, sizeof previewId, "%s", id);
  previewPreso = 1; pedPreview = 0; pedPararPreview = 0;
  overlay = 0; aberta = 1;
  guia_focar_id(id);
  previewUltLin = focoLin; previewUltCol = focoCol;
}

int guia_pediu_preview(CatItem *it) {
  if (!pedPreview || !it) return 0;
  pedPreview = 0; *it = pedPreviewItem; return 1;
}
int guia_pediu_parar_preview(void) { int v = pedPararPreview; pedPararPreview = 0; return v; }
int guia_pediu_restaurar(void)     { int v = pedRestaurar; pedRestaurar = 0; return v; }
int guia_pediu_guia_cheio(void)    { int v = pedGuiaCheio; pedGuiaCheio = 0; return v; }
void guia_preview_rect(float *x, float *y, float *w, float *h) {
  gTopoMedir();
  *x = G_PREVIEW_X; *y = G_PREVIEW_Y; *w = G_PREVIEW_W; *h = G_PREVIEW_H;
}

// A FAIXA ("mini guia") por cima do video em tela cheia. Some sozinha depois
// de G_B_OCIOSO_MS sem tecla, como os mini guias de referencia.
#define G_B_OCIOSO_MS 6000u
static Uint32 bandaUlt;
void guia_overlay_abrir(void) {
#ifdef NV_TOUCH_UI
  toqueLimpar();
#endif
  guia_carregar();
  overlay = 1; entrada = 0.0f;
  overlayDesde = bandaUlt = SDL_GetTicks();
  okDesde = 0; okLongo = 0;
  focoTopo = 0; painel = 0; catAberto = 0; opcoesAberto = 0;
  janelaDesl = 0; focoAnelOk = 0;
  focoValido();
}

int guia_aberta(void)         { return aberta; }
int guia_overlay_aberta(void) { return overlay; }
int guia_visivel(void)        { return aberta || overlay; }
int guia_quer_sair(void)      { int q = querSair; querSair = 0; return q; }
// Testes do ponteiro (#99): o chip da barra de cima em foco, ou -1.
int guia_foco_topo(void) { return focoTopo ? topoCol : -1; }

// O foco pedido com a lista ainda baixando fica pendente em focoPend;
// publicar() aplica assim que os canais chegam — o overlay aberto pelo
// player foca o canal que esta no ar mesmo se a carga so terminou depois.
void guia_focar_id(const char *id) {
  int l, c, volta;
  if (!id || !*id) return;
  snprintf(focoPend, sizeof focoPend, "%s", id);
  // Canal fora da fonte filtrada (o overlay focando o canal no ar, que veio de
  // outra fonte): o filtro volta para Todas e o foco acha o canal.
  for (volta = 0; volta < 2; volta++) {
    for (l = 0; l < nLinhas(); l++)
      for (c = 0; c < linhaN(l); c++)
        if (!strcmp(linhaItem(l, c)->id, id)) { focoLin = l; focoCol = c; return; }
    if (filtro == G_FILTRO_TODAS || !canalPorId(id)) return;
    filtroDefinir(G_FILTRO_TODAS);
  }
}

// Settings changes presentation immediately without re-fetching providers.
void guia_reordenar(void) {
  if(nCanais<1)return;
  char id[80];GCanal *c=linhaItem(focoLin,focoCol);
  snprintf(id,sizeof id,"%s",c?c->id:"");
  empacotar();if(id[0])guia_focar_id(id);
}

int guia_categorias_definir(const GuiaCategoria *categorias,int n) {
  if(n<0 || n>60000 || (n && !categorias))return 0;
  if(!n)return 1;
  uint64_t sessao=categorias[0].sessao;
  if(!nv_perfil_sessao_atual(sessao))return 0;
  for(int i=0;i<n;i++)if(!categorias[i].chave || categorias[i].sessao!=sessao)return 0;
  categoriasGarantir();
  int nNovas=nCategoriasOcultas,nVisiveis=nCategoriasVisiveis,mudou=0;
  int cap=nCategoriasOcultas+n,capVis=nCategoriasVisiveis+n;
  if(cap>G_MAX_OCULTAS)cap=G_MAX_OCULTAS;
  if(capVis>G_MAX_OCULTAS)capVis=G_MAX_OCULTAS;
  uint64_t *novas=malloc((size_t)(cap?cap:1)*sizeof *novas);
  uint64_t *visiveis=malloc((size_t)(capVis?capVis:1)*sizeof *visiveis);
  if(!novas || !visiveis){free(novas);free(visiveis);return 0;}
  memcpy(novas,categoriasOcultas,(size_t)nNovas*sizeof *novas);
  memcpy(visiveis,categoriasVisiveis,(size_t)nVisiveis*sizeof *visiveis);
  // Index only changed draft keys; merge existing overrides in linear time.
  unsigned tabelaCap=1;
  while(tabelaCap<(unsigned)n*2+1)tabelaCap*=2;
  int *indice=calloc(tabelaCap,sizeof *indice);
  if(!indice){free(novas);free(visiveis);return 0;}
  for(int k=0;k<n;k++) {
    unsigned slot=(unsigned)(categorias[k].chave^(categorias[k].chave>>32))&(tabelaCap-1);
    while(indice[slot] && categorias[indice[slot]-1].chave!=categorias[k].chave)slot=(slot+1)&(tabelaCap-1);
    indice[slot]=k+1;
  }
  nNovas=nVisiveis=0;
  for(int lado=0;lado<2;lado++) {
    const uint64_t *antes=lado?categoriasVisiveis:categoriasOcultas;
    int quantos=lado?nCategoriasVisiveis:nCategoriasOcultas;
    uint64_t *depois=lado?visiveis:novas;
    int *nd=lado?&nVisiveis:&nNovas;
    for(int i=0;i<quantos;i++) {
      unsigned slot=(unsigned)(antes[i]^(antes[i]>>32))&(tabelaCap-1);
      while(indice[slot] && categorias[indice[slot]-1].chave!=antes[i])slot=(slot+1)&(tabelaCap-1);
      if(!indice[slot])depois[(*nd)++]=antes[i];
    }
  }
  for(unsigned slot=0;slot<tabelaCap;slot++)if(indice[slot]) {
    const GuiaCategoria *c=&categorias[indice[slot]-1];
    uint64_t *v=c->oculta?novas:visiveis;
    int *nv=c->oculta?&nNovas:&nVisiveis,lim=c->oculta?cap:capVis;
    if(*nv>=lim){free(indice);free(novas);free(visiveis);return 0;}
    v[(*nv)++]=c->chave;
  }
  free(indice);
  mudou=nNovas!=nCategoriasOcultas || nVisiveis!=nCategoriasVisiveis;
  qsort(novas,(size_t)nNovas,sizeof *novas,categoriaComparar);
  qsort(visiveis,(size_t)nVisiveis,sizeof *visiveis,categoriaComparar);
  mudou=mudou || memcmp(novas,categoriasOcultas,(size_t)nNovas*sizeof *novas) ||
    memcmp(visiveis,categoriasVisiveis,(size_t)nVisiveis*sizeof *visiveis);
  if(!mudou){free(novas);free(visiveis);return 1;}
  char *texto=calloc((size_t)nNovas*17+(size_t)nVisiveis*22+1,1);int ok=0;
  if(texto) {
    for(int k=0;k<nNovas;k++)snprintf(texto+(size_t)k*17,18,"%016llx\n",(unsigned long long)novas[k]);
    for(int k=0;k<nVisiveis;k++)snprintf(texto+(size_t)nNovas*17+(size_t)k*22,23,"show %016llx\n",(unsigned long long)visiveis[k]);
    if(nv_perfil_sessao_travar(sessao)) {
      char nome[64];arquivoPerfil("categorias",nome,sizeof nome);
      ok=dados_gravar(nome,texto);
      if(ok) {
        memcpy(categoriasOcultas,novas,(size_t)nNovas*sizeof *novas);nCategoriasOcultas=nNovas;
        memcpy(categoriasVisiveis,visiveis,(size_t)nVisiveis*sizeof *visiveis);nCategoriasVisiveis=nVisiveis;
        categoriasOrdenar();
      }
      nv_perfil_sessao_destravar();
    }
    free(texto);
  }
  free(novas);free(visiveis);
  if(!ok)return 0;
  guia_reordenar();focoPend[0]=0;
  cargaCancelada=1;recarregarPend=1;
  if(focoLin>=nLinhas())focoLin=nLinhas()>0?nLinhas()-1:0;
  if(focoCol>=linhaN(focoLin))focoCol=0;
  catFoco=focoLin;rolY=velY=rolL=velL=0.0f;
  return 1;
}

int guia_categoria_definir(const GuiaCategoria *categoria,int ocultar) {
  if(!categoria)return 0;
  GuiaCategoria c=*categoria;c.oculta=!!ocultar;
  return guia_categorias_definir(&c,1);
}

const char *guia_id_focado(void) {
  GCanal *c = linhaItem(focoLin, focoCol);
  return c ? c->id : "";
}

int guia_pediu_canal(CatItem *saida) {
  if (!pediuCanal || !saida) return 0;
  pediuCanal = 0;
  *saida = pedido;
  return 1;
}

// --- navegacao ------------------------------------------------------------------
static void canalParaItem(const GCanal *c, CatItem *dst) {
  memset(dst, 0, sizeof *dst);
  snprintf(dst->imdb,    sizeof dst->imdb,    "%s", c->id);
  // O CatItem tipo so conhece dois nomes de canal (ver player.c, home.c e os
  // "channel"/"tv" espalhados pelo app): o declarado fica quando e um dos
  // dois, e os demais ("live", "channels", "iptv") viram "channel". O nome
  // cru para a consulta de stream esta em GCanal.tipo / guia_canal_tipo().
  snprintf(dst->tipo,    sizeof dst->tipo,    "%s",
           !strcasecmp(c->tipo, "tv") ? "tv" : "channel");
  snprintf(dst->titulo,  sizeof dst->titulo,  "%s", c->nome);
  snprintf(dst->poster,  sizeof dst->poster,  "%s", c->logo);
  snprintf(dst->backdrop,sizeof dst->backdrop,"%s", c->logo);
  snprintf(dst->sinopse, sizeof dst->sinopse, "%s", c->desc);
  snprintf(dst->genero,  sizeof dst->genero,  "%s · %s",
           i18n("Canal"), c->cat >= 0 ? cats[c->cat].nome : "");
}

// A origem do ULTIMO canal pedido, lida por app.c junto com o CatItem. Fica
// fora do CatItem de proposito: ele e gravado em disco (catalogo-rede.bin) e
// um campo novo invalidaria o cache de todo mundo por um dado de sessao.
const char *guia_canal_origem(void) { return pedidoBase; }
// O tipo DECLARADO do catalogo do mesmo canal, para o /stream/<tipo>/ ir
// pelo nome que o addon usa. "channel" sem declaracao: e o que este caminho
// sempre pediu antes de conhecer o tipo.
const char *guia_canal_tipo(void) { return pedidoTipo[0] ? pedidoTipo : "channel"; }

static time_t janelaIni(time_t agoraT);
static time_t instanteFoco(time_t agoraT);
static int epgDo(GCanal *c);
static int gAgora(GCanal *c, time_t t, EpgProg *p);   // ver a definicao (#158)
static int gProximo(GCanal *c, time_t t, int k, EpgProg *p);
static int gFaixa(GCanal *c, time_t de, time_t ate, EpgProg *out, int cap);
static void pedirCanal(GCanal *c);
static void buscaAbrir(void);
static int buscaEvento(const SDL_Event *e);
static void buscaAtualizar(float dt, Uint32 agora);
static void buscaFechar(void);

// O instante que o foco aponta, na grade cheia ou na faixa do mini guia.
static time_t tFocoAgora(void) {
  time_t t = time(NULL);
  if (overlay) return tempoDesloc() > 0 ? janelaIni(t) + 10 * 60 : t;
  return instanteFoco(t);
}

// O programa da celula em foco (o que cobre tFocoAgora), 1 se ha.
static int programaFocado(GCanal *c, EpgProg *p) {
  if (!c) return 0;
  return gAgora(c, tFocoAgora(), p);
}

// OK NUMA CELULA DO FUTURO MARCA (OU DESMARCA) O LEMBRETE — programa que nao
// comecou nao tem o que assistir, entao o OK ali so pode querer dizer "me
// avisa". Pedido do dono (25/09/2026): "avisar de um programa que ainda vai
// comecar". Em qualquer outra celula o OK e o de sempre: assistir o canal.
// A celula so e do futuro com a grade ADIANTADA (DIREITA), que e justamente
// o gesto de quem esta procurando o que vem depois.
static void acaoOk(void) {
  GCanal *c = linhaItem(focoLin, focoCol);
  EpgProg p;
  if (c && programaFocado(c, &p) && p.ini > time(NULL)) {
    char msg[200];
    int r = lembrete_alternar(c->id, c->nome, p.titulo, c->base, p.ini, p.fim);
    if (r == 1) snprintf(msg, sizeof msg, i18n("Lembrete marcado: %s"), p.titulo);
    else if (r == 0) snprintf(msg, sizeof msg, "%s", i18n("Lembrete cancelado"));
    else snprintf(msg, sizeof msg, "%s", i18n("Não cabe mais lembrete"));
    glem_aviso_curto(msg);
    printf("[lembrete] %s\n", r == 1 ? "marcado" : r == 0 ? "cancelado" : "lista cheia");
    fflush(stdout);
    return;
  }
  pedirCanal(c);
}

// O CANAL DE UM LEMBRETE, para o "Assistir" do cartao: o CatItem do guia
// quando a lista ja esta carregada; senao so a identidade guardada.
int guia_item_do_canal(const char *id, const char *nome, const char *base, CatItem *it) {
  GCanal *c = canalPorIdBase(id, base);
  if (c) { canalParaItem(c, it); snprintf(pedidoBase, sizeof pedidoBase, "%s", c->base); snprintf(pedidoTipo, sizeof pedidoTipo, "%s", c->tipo); return 1; }
  if (!id || !id[0]) return 0;
  memset(it, 0, sizeof *it);
  snprintf(it->imdb, sizeof it->imdb, "%s", id);
  snprintf(it->tipo, sizeof it->tipo, "%s", "channel");
  snprintf(it->titulo, sizeof it->titulo, "%s", nome ? nome : "");
  snprintf(pedidoBase, sizeof pedidoBase, "%s", base ? base : "");
  pedidoTipo[0] = 0;   // so a identidade: tipo desconhecido, vale o padrao
  return 1;
}

static void pedirCanal(GCanal *c) {
  if (!c || pediuCanal) return;
  // OK NO CANAL QUE JA TOCA (no preview, ou atras da faixa): tela cheia com o
  // MESMO fluxo, sem recarregar nada. Qualquer outro canal abre pelo caminho
  // de sempre — o player_abrir da sessao nova solta o preview.
  if (!strcmp(c->id, player_id_canal()) &&
      (player_mini_no_guia_ativo() || player_aberto())) {
    // Na faixa, o segundo OK (no canal que toca) abre o guia completo, como o
    // AZUL — o caminho de quem nao tem tecla colorida.
    if (player_mini_no_guia_ativo()) pedRestaurar = 1;
    else if (overlay) pedGuiaCheio = 1;
    overlay = 0;
    return;
  }
  pedPreview = 0; previewId[0] = 0; previewPreso = 0;
  canalParaItem(c, &pedido);
  snprintf(pedidoBase, sizeof pedidoBase, "%s", c->base);
  snprintf(pedidoTipo, sizeof pedidoTipo, "%s", c->tipo);
  pediuCanal = 1;
  // No overlay o OK TROCA o canal e volta ao video: ficar aberto por cima da
  // troca deixaria a pessoa assistindo atras de uma grade que ela ja usou.
  overlay = 0;
}

// A ordem publicada e a do guia (canais agrupados por categoria, na ordem em
// que o addon os declara), entao o CH+/− percorre exatamente o que se ve na
// tela. Da volta nas pontas: de "premiere" para "globo" direto e o jeito
// antigo de zapear, nao um erro de foco.
// ZAP DENTRO DA VISTA (M3): com uma fonte filtrada na barra, CH+/- anda nos
// canais dela; em Favoritos, nos favoritos; em Todas, na lista inteira. Canal
// no ar fora da vista (veio de outra fonte): o zap anda na lista inteira.
static int zapAlvo(const char *idAtual, int dir, CatItem *saida, int aplicar) {
  int i, alvo = -1, n = nZapL, inteira = 0, idx;
  if (!saida || estado != G_PRONTO || nCanais < 1) return 0;
  for (i = 0; i < n; i++)
    if (!strcmp(canais[zapL[i]].id, idAtual ? idAtual : "")) { alvo = i; break; }
  if (alvo < 0 || n < 1) {
    inteira = 1; n = nCanais; alvo = 0;
    for (i = 0; i < nCanais; i++)
      if (!strcmp(canais[i].id, idAtual ? idAtual : "")) { alvo = i; break; }
  }
  // `dir` pode ser um deslocamento maior que 1 (o zapping com debounce soma os
  // toques) e 0 devolve o proprio canal (recarregar a fonte).
  alvo = aovivo_ordem(n, alvo, dir);
  idx = inteira ? alvo : zapL[alvo];
  canalParaItem(&canais[idx], saida);
  // A origem so muda quando o zap VAI tocar; o banner do zapping so espia.
  if (aplicar) { snprintf(pedidoBase, sizeof pedidoBase, "%s", canais[idx].base);
    snprintf(pedidoTipo, sizeof pedidoTipo, "%s", canais[idx].tipo); }
  return 1;
}
int guia_zap(const char *idAtual, int dir, CatItem *saida) {
  return zapAlvo(idAtual, dir, saida, 1);
}
int guia_zap_ver(const char *idAtual, int dir, CatItem *saida) {
  return zapAlvo(idAtual, dir, saida, 0);
}


// CANAL DE LISTA M3U (M9): o id e "m3u:<chave da fonte>:<hash>" e a URL, o
// User-Agent e o Referer ficam no canal (arena), nunca no log. 1 = achou.
int guia_e_m3u(const char *id) {
  const GCanal *c = id && (!strncmp(id,"m3u:",4) || !strncmp(id,"nvm3u:",6)) ? canalPorId(id) : NULL;
  return c && c->fonte>=0 && c->fonte<nLtvF && ltvF[c->fonte].tipo==LTV_M3U && c->url && c->url[0];
}
int guia_m3u_canal(const char *id, char *url, size_t nu, char *ua, size_t nua, char *ref, size_t nr) {
  const GCanal *c = guia_e_m3u(id) ? canalPorId(id) : NULL;
  if (url && nu) url[0] = 0;
  if (ua && nua) ua[0] = 0;
  if (ref && nr) ref[0] = 0;
  if (!c || !c->url || !c->url[0]) return 0;
  if (url && nu) snprintf(url, nu, "%s", c->url);
  if (ua && nua) snprintf(ua, nua, "%s", c->ua ? c->ua : "");
  if (ref && nr) snprintf(ref, nr, "%s", c->ref ? c->ref : "");
  return 1;
}

// DIGITAR O NUMERO (M7, decisao 7): o canal que o numero digitado leva, pela
// regra do ltv.h (fonte filtrada: so ela; "Todas": favoritos primeiro, depois
// as fontes na ordem da barra). `aplicar` 0 so espia (o selo do player chama a
// cada quadro: o resultado fica em cache ate o numero, o filtro ou a lista
// mudarem). `fonte` recebe o nome da fonte quando o mesmo numero existe em mais
// de uma ("Fonte · 101" na tela). 0 = nao ha canal com esse numero.
int guia_numero_canal(int numero, CatItem *saida, char *fonte, size_t nf, int aplicar) {
  static int cNum = -1, cFiltro = -9, cN = -1, cIdx = -1, cAmb;
  static const GCanal *cLista;
  static LtvNum *buf; static int capBuf;
  int idx, amb = 0;
  if (fonte && nf) fonte[0] = 0;
  if (estado != G_PRONTO || nCanais < 1 || numero <= 0) return 0;
  if (numero == cNum && filtro == cFiltro && nCanais == cN && canais == cLista) { idx = cIdx; amb = cAmb; }
  else {
    int i, f = filtro >= 0 ? filtro : -1;
    if (nCanais > capBuf) {
      LtvNum *no = realloc(buf, sizeof *no * (size_t)nCanais);
      if (!no) return 0;
      buf = no; capBuf = nCanais;
    }
    for (i = 0; i < nCanais; i++) {
      buf[i].fonte = canais[i].fonte; buf[i].numero = canais[i].numero;
      buf[i].fav = canais[i].fav; buf[i].ordemFonte = canais[i].fonte;
    }
    idx = ltv_numero_achar(buf, nCanais, numero, f, &amb);
    cNum = numero; cFiltro = filtro; cN = nCanais; cLista = canais; cIdx = idx; cAmb = amb;
  }
  if (idx < 0) return 0;
  if (fonte && nf && amb && canais[idx].fonte >= 0 && canais[idx].fonte < nLtvF)
    snprintf(fonte, nf, "%s", ltvF[canais[idx].fonte].nome);
  if (saida) {
    canalParaItem(&canais[idx], saida);
    if (aplicar) {
    snprintf(pedidoBase, sizeof pedidoBase, "%s", canais[idx].base);
    snprintf(pedidoTipo, sizeof pedidoTipo, "%s", canais[idx].tipo);
  }
  }
  return 1;
}

// Onde o canal esta na ordem do guia e de que categoria e: o numero que o OSD
// do player mostra. 0 com a lista ainda nao carregada ou canal fora dela.
int guia_info_canal(const char *id, int *numero, int *total, char *cat, size_t n) {
  int i;
  if (estado != G_PRONTO || nCanais < 1 || !id || !id[0]) return 0;
  for (i = 0; i < nCanais; i++) if (!strcmp(canais[i].id, id)) break;
  if (i >= nCanais) return 0;
  // O NUMERO DO PROVEDOR (decisao 7): tvg-chno / num do Xtream, ou a posicao
  // na fonte. O total e o da fonte.
  if (numero) *numero = canais[i].numero;
  if (total) *total = canais[i].fonte >= 0 && canais[i].fonte < nLtvF ? ltvF[canais[i].fonte].nCanais : nCanais;
  if (cat && n) snprintf(cat, n, "%s", canais[i].cat >= 0 ? cats[canais[i].cat].nome : "");
  return 1;
}

int guia_e_favorito(const char *id) {
  if (!favLido) favLer();
  return id && id[0] && favIndice(id) >= 0;
}

void guia_alternar_favorito(const char *id) {
  GCanal *c = canalPorId(id);
  if (!favLido) favLer();
  if (c) favAlternar(c);
}

// O programa NO AR do canal `id` (a mesma consulta do cartao do guia, com a
// XMLTV e a grade curta do Xtream). O ponteiro de titulo vale ate o proximo
// epg_passo/xtepg_passo: quem guarda copia.
int guia_programa_agora(const char *id, time_t t, EpgProg *p) {
  GCanal *c = canalPorId(id);
  return c && p && gAgora(c, t, p);
}

// A faixa de programas do canal pela MESMA cascata e o MESMO fuso do guia (M6).
int guia_programa_faixa(const char *id, time_t de, time_t ate, EpgProg *out, int cap) {
  GCanal *c = canalPorId(id);
  return c && out ? gFaixa(c, de, ate, out, cap) : 0;
}
int guia_programa_proximo(const char *id, time_t t, int k, EpgProg *p) {
  GCanal *c = canalPorId(id);
  return c && p && gProximo(c, t, k, p);
}

static void moverVertical(int dir) {
  int nl = nLinhas();
  if (nl < 1) return;
  focoLin += dir;
  if (focoLin < 0) focoLin = 0;
  if (focoLin >= nl) focoLin = nl - 1;
  if (focoCol >= linhaN(focoLin)) focoCol = linhaN(focoLin) - 1;
  if (focoCol < 0) focoCol = 0;
}

// Salta uma CATEGORIA inteira. No modo salta-categoria o foco nao desce linha a
// linha: vai direto ao primeiro canal da secao seguinte — e o unico jeito
// razoavel de atravessar ~34 secoes de controle remoto.
static void saltarCat(int dir) {
  int nl = nLinhas();
  if (nl < 1) return;
  focoLin += dir;
  if (focoLin < 0) focoLin = 0;
  if (focoLin >= nl) focoLin = nl - 1;
  focoCol = 0;
}

// --- painel de categorias -------------------------------------------------------
static void catAbrir(int modo, int dir) {
  if (nLinhas() < 1) return;
  if (!catAberto) catFoco = focoLin;
#ifdef NV_TOUCH_UI
  if (!catAberto) toquerol_limpar(&toqueCat);
#endif
  catAberto = modo;
  catUlt = SDL_GetTicks();
  if (dir) {
    catFoco += dir;
    if (catFoco < 0) catFoco = 0;
    if (catFoco >= nLinhas()) catFoco = nLinhas() - 1;
  }
}
static void catMover(int dir) {
  catFoco += dir;
  if (catFoco < 0) catFoco = 0;
  if (catFoco >= nLinhas()) catFoco = nLinhas() - 1;
}
static void catFechar(void) { catAberto = 0; dirSeg = 0; }
// Entra na categoria escolhida: primeiro canal dela, foco de volta na grade.
static void catConfirmar(void) {
  if (catFoco != focoLin) { focoLin = catFoco; focoCol = 0; }
  focoTopo = 0;
  catFechar();
  focoValido();
}

// No modo lista a navegacao e canal a canal ATRAVES das categorias: BAIXO no
// ultimo canal de "Filmes" cai no primeiro de "Esportes". E o que uma lista
// vertical promete; parar na borda da categoria obrigaria a descobrir que
// existe um "pular secao" so para continuar descendo.
static void moverLista(int dir) {
  if (nLinhas() < 1) return;
  if (dir > 0) {
    if (focoCol + 1 < linhaN(focoLin)) focoCol++;
    else if (focoLin + 1 < nLinhas()) { focoLin++; focoCol = 0; }
  } else {
    if (focoCol > 0) focoCol--;
    else if (focoLin > 0) { focoLin--; focoCol = linhaN(focoLin) - 1; }
  }
}

static void alternarModo(void) {
#ifdef NV_TOUCH_UI
  toqueLimpar();
#endif
  modoLista = !modoLista;
  modoGravar();
  // O canal focado e o mesmo; so a rolagem recomeca do lugar certo para o
  // modo novo (as duas molas sao independentes, ver a declaracao).
  velY = 0.0f; velL = 0.0f;
  janelaDesl = 0;
}

// --- painel de addons -------------------------------------------------------------
// Itens do painel, em ordem: os addons de CANAL da conta (paIdx[i] = indice em
// addons.c) e depois os recomendados (indice = paN + k).
//
// SO PROVEDOR DE CANAL ENTRA. Este e o painel do guia, nao a tela de addons
// dos Ajustes: Cinemeta, TMDB e OpenSubtitles nao tem nada a ver com a lista
// de canais, e liga-los ou desliga-los daqui so confundia ("desliguei e nada
// mudou"). O criterio e o manifesto que a sonda deste guia leu (sabe[]):
// fornece canal = entra; nao fornece = fica de fora; NAO RESPONDEU = entra,
// porque um addon de canal cujo servidor esta fora (FrostView em 408) e
// exatamente o que a pessoa quer poder desligar daqui. Antes da primeira
// sonda todos sao "nao respondeu" e o painel lista todos — por poucos
// segundos, e dizendo "ainda nao conferido".
#define G_MAX_PA 16
static int paIdx[G_MAX_PA], paN;
static void painelMontar(void) {
  int i;
  paN = 0;
  for (i = 0; i < addons_n() && paN < G_MAX_PA; i++) {
    const char *b = addons_base(i);
    if (!b || !b[0]) continue;
    if (sabeCanal(b) == 0) continue;
    paIdx[paN++] = i;
  }
  if (paFoco >= paN + nRec) paFoco = paN + nRec - 1;
  if (paFoco < 0) paFoco = 0;
}
static int painelN(void) { return paModo == 1 ? (fNivel ? fN : 3 + fN) : paN + nRec; }

static void painelAbrir(void) {
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toqueAddon);
#endif
  recLer();
  painel = 1; paModo = 0; paFoco = 0; paMexeu = 0; paLigou = 0; paRol = 0.0f; paVelRol = 0.0f;
  paErro = -1;
  painelMontar();
  // O painel mostra o que o manifesto disse; se a sonda de Ajustes nunca
  // rodou, e barato pedi-la agora (uma vez por lista, ver addons.h).
  addons_sondar_manifestos();
}

// LIGAR/DESLIGAR VALE NA HORA, nao ao fechar o painel. Desligar tira os canais
// daquele addon de `canais[]` neste mesmo quadro (sem rede) e pede a recarga
// em segundo plano para a lista de fontes ficar coerente; ligar so tem como
// trazer canal baixando o catalogo, entao pede a recarga e a lista atual fica
// na tela ate ela chegar (G_BAIXANDO desenha a lista que ha). desc_repetir,
// que e o ciclo da home (~20 s na TV), continua uma vez por visita, no fechar.
static void painelAplicar(int i) {
  const char *b = addons_base(i);
  if (!addons_ativo(i) && b && b[0]) {
    int k, w, antes = nCanais;
    empacotar();   // ja descarta os canais de addon desligado
    if (nCanais != antes) focoValido();
    // As fontes desse addon saem daqui tambem: fioGuia copia `fontes[]` antes
    // de sondar, e baseLigada() ja as barra la — isto e so para o rotulo
    // "nenhum catalogo" nao contar fonte de addon desligado.
    for (k = 0, w = 0; k < nFontes; k++)
      if (strcmp(fontes[k].base, b) != 0) fontes[w++] = fontes[k];
    nFontes = w;
  }
  if (fioVivo) recarregarPend = 1;
  else { estado = G_PARADO; ultTentativa = 0; iniciarCarga(); }
}

// FECHAR E QUANDO O RESTO DO APP FICA SABENDO. Mesma regra de addonsui.c no
// LIGAR: desc_repetir() refaz o ciclo inteiro (~20 s na TV) porque ligar um
// addon so traz catalogo baixando o manifesto dele. No DESLIGAR, porem, nao ha
// o que baixar — as fileiras do addon saem por filtro (desligada() em
// descoberta.c), e desc_remontar_fileiras() refaz a home em memoria, sem
// rede, sem reler Trakt e sem re-baixar os manifestos dos outros addons (que
// e o overhead que o dono viu no log como "recarrega tudo de novo, inclusive
// os desativados"). Roda uma vez por visita, nao por tecla: o guia ja se
// atualizou a cada tecla em painelAplicar.
static int recargaDoPainel;   // ver guia_atualizando_lista

// --- FILTROS DA LIVE TV (qualidade, categorias, canais) -----------------------------
// Itens do painel: 0..2 = qualidade (Todas / So 4K / Preferir 4K), depois uma
// linha por categoria. Direita numa categoria abre os canais dela; OK mostra
// ou esconde. Tudo vale por perfil e e gravado na hora; a lista so e refeita
// ao fechar. O cache sem filtro permite recuperar a lista sem rede.
static void fMontarCats(void) {
  int i, j, k, cap = fltLive.nCat + 1;
  for (i = 0; i < nLtvF; i++) cap += ltvF[i].ngi;
  free(fIt); fIt = calloc((size_t)cap, sizeof *fIt); fN = 0;
  if (!fIt) return;
  for (i = 0; i < nLtvF; i++) for (j = 0; j < ltvF[i].ngi; j++) {
    const GGrupoInfo *g = &ltvF[i].gi[j];
    for (k = 0; k < fN && strcmp(fIt[k].nome, g->nome); k++) {}
    if (k == fN) {
      snprintf(fIt[k].nome, sizeof fIt[k].nome, "%s", g->nome);
      fN++;
    }
    fIt[k].n += g->n;
  }
  for (i = 0; i < fN; i++) fIt[i].oculto = 0;
  for (i = 0; i < fltLive.nCat; i++) {
    for (k = 0; k < fN && strcmp(fIt[k].nome, fltLive.cat[i]); k++) {}
    if (k == fN) { snprintf(fIt[k].nome, sizeof fIt[k].nome, "%s", fltLive.cat[i]); fN++; }
    fIt[k].oculto = 1;
  }
}
static void fCanalAdd(const char *fonte, const char *id, const char *nome) {
  int i;
  for (i = 0; i < fN; i++) if (!strcmp(fIt[i].fonte, fonte) && !strcmp(fIt[i].id, id)) return;
  if (fN >= G_PF_CANAIS_MAX) return;
  snprintf(fIt[fN].fonte, sizeof fIt[fN].fonte, "%s", fonte);
  snprintf(fIt[fN].id, sizeof fIt[fN].id, "%s", id);
  snprintf(fIt[fN].nome, sizeof fIt[fN].nome, "%s", nome);
  fN++;
}
// Read only this category into the bounded panel, including later raw entries.
static void fCanaisCache(const char *chave, const char *cat) {
  char nome[64], caminho[600], grupo[GF_NOME], id[GF_ID], canal[GF_NOME];
  long long header[8];
  FILE *fp;
  cacheNomeFonte(chave, nome, sizeof nome);
  if (!dados_caminho(caminho, sizeof caminho, nome) || !(fp = fopen(caminho, "rb"))) return;
  if (fread(header, sizeof header, 1, fp) != 1 || header[0] != GC_MAGIA || header[1] != GC_VERSAO ||
      header[3] < 0 || header[3] > INT_MAX || header[4] < 0 || header[4] > INT_MAX) {
    fclose(fp); return;
  }
  // Source strings, then group name/count/visibility records.
  for (long long i = 0; i < 3 + header[4]; i++) {
    unsigned short n;
    if (fread(&n, sizeof n, 1, fp) != 1 || fseek(fp, n, SEEK_CUR) != 0 ||
        (i >= 3 && fseek(fp, 2 * sizeof(long long), SEEK_CUR) != 0)) goto fim;
  }
  for (long long i = 0; i < header[3] && fN < G_PF_CANAIS_MAX; i++) {
    for (int field = 0; field < 11; field++) {
      unsigned short n;
      char *dst = field == 0 ? grupo : field == 1 ? id : field == 2 ? canal : NULL;
      size_t cap = field == 1 ? sizeof id : sizeof grupo;
      if (fread(&n, sizeof n, 1, fp) != 1) goto fim;
      size_t k = dst ? (n < cap - 1 ? n : cap - 1) : 0;
      if ((k && fread(dst, 1, k, fp) != k) || fseek(fp, n - (long)k, SEEK_CUR) != 0) goto fim;
      if (dst) dst[k] = 0;
      if (field == 0 && fseek(fp, sizeof(long long), SEEK_CUR) != 0) goto fim;
    }
    if (header[1] >= 3 && fseek(fp, sizeof(long long), SEEK_CUR) != 0) goto fim;
    if (!strcmp(grupo, cat)) fCanalAdd(chave, id, canal);
  }
fim:
  fclose(fp);
}
static void fMontarCanaisPagina(const char *cat, int inicio) {
  int i, k;
  // Negative cursors address refetched category pages, independent of spool order.
  if (inicio < 0) {
    if (fInventarioCat[0]) return;
    snprintf(fInventarioCat, sizeof fInventarioCat, "%s", cat);
    fInventarioInicio = -(inicio + 1);
    if (fioVivo) recarregarPend = 1;
    else {
      iniciarCarga();
      if (!fioVivo) fInventarioCat[0] = 0;
    }
    return;
  }
  fProximo = 0;
  free(fIt); fIt = calloc(G_PF_CANAIS_MAX + GF_CANAIS_MAX + 1, sizeof *fIt); fN = 0;
  if (!fIt) return;
  if (inventarioDisco) {
    // Preserve the inventory order and reserve room for admitted late channels.
    for (i = 0; !inicio && i < nInventario && fN < G_PF_CANAIS_MAX - nCanais; i++)
      if (!strcmp(inventario[i].cat, cat))
        fCanalAdd(inventario[i].fonte, inventario[i].id, inventario[i].nome);
    for (i = 0; i < nCanais; i++)
      if (canais[i].cat >= 0 && !strcmp(cats[canais[i].cat].nome, cat))
        fCanalAdd(ltvF[canais[i].fonte].chave, canais[i].id, canais[i].nome);
    char block[G_INVENTARIO_BLOCO];
    int pos = 0;
    rewind(inventarioDisco);
    while (!fProximo && fread(block, sizeof block, 1, inventarioDisco) == 1) {
      for (size_t off = 0; off + 4 * sizeof(unsigned short) <= sizeof block;) {
        unsigned short h[4];
        memcpy(h, block + off, sizeof h); off += sizeof h;
        if (!h[2] || !h[3] || h[2] > GF_ID || h[3] > GF_NOME ||
            off + h[2] + h[3] > sizeof block) break;
        const char *id = block + off, *nome = id + h[2];
        off += h[2] + h[3];
        if (h[0] >= nLtvF || h[1] >= ltvF[h[0]].ngi ||
            id[h[2] - 1] || nome[h[3] - 1] || strcmp(ltvF[h[0]].gi[h[1]].nome, cat)) continue;
        if (pos++ < inicio) continue;
        if (fN == G_PF_CANAIS_MAX) { fProximo = pos - 1; break; }
        fCanalAdd(ltvF[h[0]].chave, id, nome);
      }
    }
    if (!fProximo && inventarioCortado) {
      int total = 0;
      for (i = 0; i < nLtvF; i++)
        for (k = 0; k < ltvF[i].ngi; k++)
          if (!strcmp(ltvF[i].gi[k].nome, cat)) total += ltvF[i].gi[k].n;
      if (pos < total) fProximo = -1;
    }
  } else {
  // Reserve room for displayed channels even if this raw category filled the panel.
  for (i = 0; i < nInventario && fN < G_PF_CANAIS_MAX - nCanais; i++)
    if (!strcmp(inventario[i].cat, cat)) fCanalAdd(inventario[i].fonte, inventario[i].id, inventario[i].nome);
  for (i = 0; i < nCanais; i++)
    if (canais[i].cat >= 0 && !strcmp(cats[canais[i].cat].nome, cat))
      fCanalAdd(ltvF[canais[i].fonte].chave, canais[i].id, canais[i].nome);
  for (i = 0; i < nLtvF && fN < G_PF_CANAIS_MAX; i++) fCanaisCache(ltvF[i].chave, cat);
  }
  for (i = 0; i < fltLive.nCan; i++) {
    if (strcmp(fltLive.can[i].cat, cat)) continue;
    if (!fltLive.can[i].fonte[0]) {
      for (k = 0; k < nLtvF; k++)
        fCanalAdd(ltvF[k].chave, fltLive.can[i].id, fltLive.can[i].nome);
    } else {
      for (k = 0; k < fN; k++)
        if (!strcmp(fIt[k].fonte, fltLive.can[i].fonte) && !strcmp(fIt[k].id, fltLive.can[i].id)) break;
      if (k == fN) {
        snprintf(fIt[k].fonte, sizeof fIt[k].fonte, "%s", fltLive.can[i].fonte);
        snprintf(fIt[k].id, sizeof fIt[k].id, "%s", fltLive.can[i].id);
        snprintf(fIt[k].nome, sizeof fIt[k].nome, "%s", fltLive.can[i].nome);
        fN++;
      }
    }
  }
  for (i = 0; i < fN; i++)
    fIt[i].oculto = guiafiltro_canal_oculto_fonte(&fltLive, fIt[i].fonte, fIt[i].id);
  if (fProximo) {
    snprintf(fIt[fN].nome, sizeof fIt[fN].nome, "%s", i18n("Carregar mais"));
    fIt[fN++].n = -1;
  }
}
static void fMontarCanais(const char *cat) { fMontarCanaisPagina(cat, 0); }
static void filtrosAbrir(void) {
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toqueAddon);
#endif
  fltGarantir();
  paOkTick = 0;
  painel = 1; paModo = 1; paFoco = 0; paMexeu = 0; paRol = 0.0f; paVelRol = 0.0f;
  fNivel = 0; fSujo = 0;
  fMontarCats();
}
static void filtrosVoltar(void) {
  fInventarioCat[0] = 0;
  fNivel = 0;
  fMontarCats();
  paFoco = 3 + fFocoCat < 3 + fN ? 3 + fFocoCat : 2;
  paRol = paVelRol = 0.0f;
}
static void filtrosFechar(void) {
  fInventarioCat[0] = 0;
  free(fIt); fIt = NULL; fN = 0; fNivel = 0; paModo = 0;
  if (!fSujo) return;
  fSujo = 0;
  empacotar();
  focoValido();
  assinaturaPublicada = 0;
  if (fioVivo) { cargaCancelada = 1; recarregarPend = 1; }
  else { estado = G_PARADO; ultTentativa = 0; iniciarCarga(); }
  recargaDoPainel = 1;
}
static void filtrosOk(void) {
  if (paFoco < 0 || paFoco >= painelN()) return;
  if (!fNivel && paFoco < 3) {
    if (fltLive.modo != paFoco) { guiafiltro_modo_definir(&fltLive, paFoco); fSujo = 1; fltGravar(); }
    return;
  }
  { GFItem *it = &fIt[fNivel ? paFoco : paFoco - 3];
    if (fNivel && it->n == -1) {
      fMontarCanaisPagina(fCatNome, fProximo);
      paFoco = 0; paRol = paVelRol = 0.0f;
      return;
    }
    if (fNivel) it->oculto = guiafiltro_canal_alternar_fonte(&fltLive, it->fonte, it->id, it->nome, fCatNome);
    else it->oculto = guiafiltro_cat_alternar(&fltLive, it->nome);
    fSujo = 1; fltGravar(); }
}
static void filtrosEvento(SDL_Keycode k, Uint32 agora) {
  int n = painelN();
  if (k == SDLK_UP)   { if (paFoco > 0) paFoco--; return; }
  if (k == SDLK_DOWN) { if (paFoco + 1 < n) paFoco++; return; }
  if (k == SDLK_LEFT && fNivel) { filtrosVoltar(); return; }
  if (k == SDLK_RIGHT && !fNivel && paFoco >= 3 && paFoco < n) {
    fFocoCat = paFoco - 3;
    snprintf(fCatNome, sizeof fCatNome, "%s", fIt[fFocoCat].nome);
    fNivel = 1; fMontarCanais(fCatNome); paFoco = 0; paRol = 0.0f; paVelRol = 0.0f;
    return;
  }
  if ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && n > 0 && agora - paOkTick >= G_REP_MS) {
    paOkTick = agora; filtrosOk();
  }
}

static void painelFechar(void) {
  painel = 0;
  if (paModo == 1) { filtrosFechar(); return; }
  // A recarga que o painel pediu continua depois que ele fecha: quem diz
  // "Atualizando a lista de canais…" passa a ser a ilha (app.c).
  if (paMexeu && (fioVivo || recarregarPend)) recargaDoPainel = 1;
  if (!paMexeu) return;
  paMexeu = 0;
  if (paLigou) {
    paLigou = 0;
    desc_repetir();
  } else {
    // So desligou nesta visita: remonta as fileiras sem tocar na rede. Se a
    // descoberta estiver com um ciclo no ar, desc_remontar_fileiras opera sobre
    // a ultima montagem concluida e o ciclo que chega depois aplica por cima —
    // sem race, porque cat_republicar_fileiras e cat_definir_tudo sao os dois
    // atomicos no fio de desenho.
    desc_remontar_fileiras();
  }
}

static void instalar(int k) {
  // addons_adicionar em vez de exportar-acrescentar-definir.
  //
  // O caminho antigo funcionava, com dois efeitos colaterais: addons_definir_lista
  // REFAZ a lista e com isso zera o `sondado` e o `id` de manifesto de TODOS os
  // addons — e esse id e a chave que as colecoes da conta usam, entao perde-lo
  // faz colecao abrir vazia ate a proxima sonda. E o log registrava "vindos da
  // conta" para uma lista que veio daqui.
  paErro = -1; paErroCheio = 0;
  if (k < 0 || k >= nRec) return;
  if (!addons_adicionar(rec[k].nome, rec[k].url)) {
    paErro = paN + k;
    paErroCheio = (addons_n() >= 16);
    return;
  }
  registro_aviso_legal();
  // #360 mao unica: a instalacao vale so nesta TV ate o proximo pull da conta.
  paMexeu = 1;
  paLigou = 1;   // instalar e ligar: precisa baixar o manifesto e o catalogo
  // O recem-instalado entra na secao de cima (ainda "nao conferido") e o
  // foco segue o mesmo item, que desceu uma linha.
  painelMontar();
  if (paFoco < paN + nRec - 1) paFoco++;
  painelAplicar(addons_n() - 1);
}

static void painelOk(void) {
  if (paFoco < paN) {
    int i = paIdx[paFoco];
    int ligado = addons_alternar(i);
    // #360 mao unica: desligar aqui vale nesta TV ate o proximo pull da conta.
    paMexeu = 1;
    if (ligado) paLigou = 1;   // ligou (ou acabou de instalar): fechar pede ciclo
    painelAplicar(i);
  } else if (paFoco - paN < nRec && !recInstalado(&rec[paFoco - paN])) {
    instalar(paFoco - paN);
  }
}

// ESCONDER/MOSTRAR SO NO GUIA (#283). Esconder tira os canais do add-on agora,
// sem rede. Mostrar de novo so tem como trazer baixando: a lista que esta em
// memoria nao os guarda. O add-on em si nao muda de estado.
static void painelGuia(int i) {
  const char *b = addons_base(i);
  if (!b || !b[0]) return;
  ocultosGarantir();
  if (guiaaddons_alternar(&ocultos, b)) {
    int antes = nCanais;
    empacotar();
    if (nCanais != antes) focoValido();
  } else {
    assinaturaPublicada = 0;   // a lista da rede e igual: sem isto nao seria republicada
    if (fioVivo) recarregarPend = 1;
    else { estado = G_PARADO; ultTentativa = 0; iniciarCarga(); }
  }
  ocultosGravar();
}
#ifdef NV_TOUCH_UI
static void ponteiroAddonGuia(int addon, int linha) {
  if (!telefoneui_ativo() || !painel || linha < 0 || linha >= paN || paIdx[linha] != addon) return;
  painelGuia(addon);
}
#endif

// OK segurado NAO e varios OK: o firmware repete o KEYDOWN a cada ~130 ms, e
// sem este repouso segurar a tecla ligava e desligava o addon em sequencia
// (addonsui.c herda esse comportamento; aqui nao).
static void painelEvento(SDL_Keycode k, Uint32 agora) {
  int n = painelN();
  if (paModo == 1) { filtrosEvento(k, agora); return; }
  if (k == SDLK_UP)   { if (paFoco > 0) paFoco--; return; }
  if (k == SDLK_DOWN) { if (paFoco + 1 < n) paFoco++; return; }
  if ((k == SDLK_LEFT || k == SDLK_RIGHT) && paFoco < paN) {
    if (agora - paOkTick >= G_REP_MS) { paOkTick = agora; painelGuia(paIdx[paFoco]); }
    return;
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
    if (n > 0 && agora - paOkTick >= G_REP_MS) { paOkTick = agora; painelOk(); }
    return;
  }
}

static void sair(void) {
  if (opcoesAberto) { opcoesAberto = 0; return; }
  if (painel) { if (paModo == 1 && fNivel) filtrosVoltar(); else painelFechar(); return; }
  if (catAberto) { catFechar(); return; }
  if (overlay) overlay = 0;
  else { aberta = 0; querSair = 1; previewParar(); }
  catAberto = 0; dirSeg = 0; okDesde = 0; focoTopo = 0;
}

// --- BARRA DE FONTES (M3) -----------------------------------------------------------
// "★ Favoritos · Todas · <addon> · Xtream · <M3U>" no alto da ficha do heroi.
// Cada chip de fonte tem um PONTO DE SAUDE (verde ok, ambar lista do cache ou
// grade fraca, vermelho fora) e a contagem. Foco num chip mostra a linha da
// fonte (canais, grupos, grade em %, atualizada ha quanto, e o aviso de corte).
// OK filtra (a escolha vale so na sessao, decisao 1); segurar OK abre as opcoes
// da fonte: ordem dos grupos (provedor / A-Z) e o horario da grade (M6).
#define G_FONTES_H 56.0f
// OPCOES DO CANAL (segurar OK numa linha, M5/M7): Favorito e "Escolher grade
// deste canal". fopModo 0 = fonte, 1 = canal, 2 = lista de grades.
enum { FOP_FONTE = 0, FOP_CANAL, FOP_GRADE };
static int fopModo;
#define G_SUG 8
static int sugN, sug[G_SUG];
static void mapaDefinir(GCanal *c, const char *gf, const char *gid);
static float fontesRol;
static int barraVisivel(void) { return nLtvF > 0 && !overlay; }
static int fontesN(void) { return 2 + nLtvF; }
static int filtroDoChip(int i) { return i == 0 ? G_FILTRO_FAV : i == 1 ? G_FILTRO_TODAS : i - 2; }
static int chipDoFiltro(void) { return filtro == G_FILTRO_FAV ? 0 : filtro == G_FILTRO_TODAS ? 1 : filtro + 2; }
static void fontesEntrar(void) { focoFontes = 1; focoTopo = 0; fontesCol = chipDoFiltro(); fopAberto = 0; }
static void fontesEscolher(int i) {
  filtroDefinir(filtroDoChip(i));
  focoLin = 0; focoCol = 0; focoValido();
  printf("[ltv] filtro: %s\n", i == 0 ? "favoritos" : i == 1 ? "todas" : ltvF[i - 2].chave);
  fflush(stdout);
}
enum { FOP_AZ, FOP_GRUPOS, FOP_FUSO, FOP_RECARREGAR, FOP_LIMPAR, FOP_N };
static void gpAbrir(int f);
static void fopCanalAbrir(GCanal *c) { fopAberto = 1; fopModo = FOP_CANAL; fopFoco = 0; fopCanal = c; }
static int fopItens(void) { return fopModo == FOP_CANAL ? 2 : fopModo == FOP_GRADE ? 2 + sugN : FOP_N; }
static void fopCanalOk(void) {
  GCanal *c = fopCanal;
  if (!c) { fopAberto = 0; return; }
  if (fopModo == FOP_CANAL) {
    if (fopFoco == 0) { favAlternar(c); fopAberto = 0; return; }
    // As sugestoes: as grades com programa mais parecidas com o nome (fuzzy so
    // SUGERE, nunca casa sozinho), e a atual primeiro quando ha.
    sugN = epg_sugerir(c->tvgNome[0] ? c->tvgNome : c->nome, sug, G_SUG);
    fopModo = FOP_GRADE; fopFoco = 0;
    return;
  }
  if (fopFoco == 0) mapaDefinir(c, NULL, NULL);
  else if (fopFoco == 1) mapaDefinir(c, "", "-");
  else mapaDefinir(c, epg_canal_fonte(sug[fopFoco - 2]), epg_canal_id(sug[fopFoco - 2]));
  epgCoberturaZerar();
  fopAberto = 0;
}
static int limparConfirm, limparFoco;   // confirmacao do "Limpar lista"
static void fopMudar(int dir) {
  int f = fontesCol - 2;
  LtvPref *p;
  if (fopModo != FOP_FONTE) { if (dir == 0) fopCanalOk(); return; }
  if (f < 0 || f >= nLtvF || !(p = ltv_pref(ltvF[f].chave, 1))) return;
  if (fopFoco == FOP_AZ) { p->az = !p->az; ltv_pref_gravar(); empacotar(); focoValido(); }
  else if (fopFoco == FOP_GRUPOS) { if (dir == 0) gpAbrir(f); }
  else if (fopFoco == FOP_FUSO) {
    p->fusoMin += dir * 30;
    if (p->fusoMin < -720) p->fusoMin = -720;
    if (p->fusoMin > 840) p->fusoMin = 840;
    ltvF[f].fusoS = p->fusoMin * 60;
    ltv_pref_gravar();
    printf("[ltv] fonte=%s fuso=%+dmin\n", ltvF[f].chave, p->fusoMin); fflush(stdout);
  } else if (fopFoco == FOP_RECARREGAR && dir == 0) {
    fopAberto = 0;
    if (fioVivo) recarregarPend = 1;
    else { estado = G_PARADO; ultTentativa = 0; iniciarCarga(); }
  } else if (fopFoco == FOP_LIMPAR && dir == 0) {
    fopAberto = 0;
    limparConfirm = 1; limparFoco = 1;
  }
}
// OK na confirmacao do Limpar: foco 0 = executa, 1 = cancela.
// --- ESCOLHER GRUPOS (M2) ---------------------------------------------------------
// Uma fonte que passa do teto da TV mostra so parte dos canais; aqui a pessoa
// marca os grupos que quer, vendo "N de M canais" ao vivo (M = o teto da
// plataforma menos o que as OUTRAS fontes ja ocupam). Sem nada marcado vale o
// automatico: grupos inteiros na ordem do provedor. Marcar so e permitido
// enquanto couber, para a conta da tela ser a do resultado.
// Aplicar grava em ltv_pref (grupos) e recarrega; Voltar descarta.
static int gpFonte, gpFoco, gpN, gpAviso;
static unsigned char *gpMarca;
static float gpRol;
static Uint32 gpAbriuEm, gpAvisoEm;
static int gpCota(void) {
  int j, usado = 0;
  for (j = 0; j < nLtvF; j++) if (j != gpFonte) usado += ltvF[j].nCanais;
  return tetoCarga - usado > 0 ? tetoCarga - usado : 0;
}
// Canais que entram com a marcacao atual (automatico = a regra do provedor).
static int gpConta(int *auto_) {
  const GFonteLtv *F = &ltvF[gpFonte];
  int i, soma = 0, marcados = 0;
  for (i = 0; i < gpN; i++) if (gpMarca[i]) { soma += F->gi[i].n; marcados++; }
  if (auto_) *auto_ = !marcados;
  if (marcados) return soma;
  { int *c = malloc(sizeof(int) * (size_t)(gpN ? gpN : 1));
    const char **nm = malloc(sizeof(char *) * (size_t)(gpN ? gpN : 1));
    unsigned char *en = malloc((size_t)(gpN ? gpN : 1));
    if (c && nm && en) {
      for (i = 0; i < gpN; i++) { c[i] = F->gi[i].n; nm[i] = F->gi[i].nome; }
      soma = ltv_grupos_admitir(c, nm, gpN, NULL, 0, gpCota(), en);
    }
    free(c); free(nm); free(en); }
  return soma;
}
static void gpAbrir(int f) {
  const GFonteLtv *F = &ltvF[f];
  LtvPref *p = ltv_pref(F->chave, 0);
  const char *sel[256];
  int i, ns = p ? ltv_pref_grupos(p, sel, 256) : 0;
  if (F->ngi <= 0) return;
  free(gpMarca);
  gpMarca = calloc((size_t)F->ngi, 1);
  if (!gpMarca) return;
  gpFonte = f; gpN = F->ngi; gpFoco = 2; gpRol = 0.0f; gpAviso = 0;
  // Ja havia escolha: ela. Senao o que esta na tela agora (quando a fonte foi
  // cortada) ou nada (cabe tudo: automatico).
  for (i = 0; i < gpN; i++)
    gpMarca[i] = ns > 0 ? (unsigned char)nomeNaLista(F->gi[i].nome, sel, ns)
                        : (unsigned char)(F->cortada && F->gi[i].entra);
  fopAberto = 0; gpAberto = 1; gpAbriuEm = SDL_GetTicks();
}
static void gpAplicar(void) {
  const GFonteLtv *F = &ltvF[gpFonte];
  LtvPref *p = ltv_pref(F->chave, 1);
  const char **v = malloc(sizeof(char *) * (size_t)(gpN ? gpN : 1));
  int i, n = 0;
  if (p && v) {
    for (i = 0; i < gpN; i++) if (gpMarca[i]) v[n++] = F->gi[i].nome;
    ltv_pref_definir_grupos(p, v, n);
    ltv_pref_gravar();
    printf("[ltv] fonte=%s grupos escolhidos=%d\n", F->chave, n); fflush(stdout);
    if (fioVivo) recarregarPend = 1;
    else { estado = G_PARADO; ultTentativa = 0; iniciarCarga(); }
  }
  free(v);
  gpAberto = 0; focoFontes = 0;
}
// Teclas do seletor; sempre consome.
static void gpEvento(SDL_Keycode k, int repete, Uint32 agora) {
  int n = 2 + gpN;
  if (k == SDLK_UP)   { if (gpFoco > 0) gpFoco--; return; }
  if (k == SDLK_DOWN) { if (gpFoco + 1 < n) gpFoco++; return; }
  if (k == SDLK_PAGEUP) { gpFoco = gpFoco > 8 ? gpFoco - 8 : 0; return; }
  if (k == SDLK_PAGEDOWN) { gpFoco = gpFoco + 8 < n ? gpFoco + 8 : n - 1; return; }
  if ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && !repete && agora - gpAbriuEm > 350u) {
    if (gpFoco == 0) {
      int au; (void)gpConta(&au);
      gpAplicar();
    } else if (gpFoco == 1) memset(gpMarca, 0, (size_t)gpN);
    else {
      int i = gpFoco - 2;
      if (gpMarca[i]) gpMarca[i] = 0;
      else {
        int au, antes = gpConta(&au);
        int livre = au ? 0 : antes;   // o automatico some ao marcar: conta de zero
        if (livre + ltvF[gpFonte].gi[i].n <= gpCota()) gpMarca[i] = 1;
        else { gpAviso = 1; gpAvisoEm = agora; }
      }
    }
  }
}
// Teclas da barra. 1 = consumiu.
static int fontesEvento(SDL_Keycode k, Uint32 agora) {
  if (fopAberto) {
    if (k == SDLK_UP)   { if (fopFoco > 0) fopFoco--; return 1; }
    if (k == SDLK_DOWN) { if (fopFoco + 1 < fopItens()) fopFoco++; return 1; }
    if (k == SDLK_LEFT || k == SDLK_RIGHT) { fopMudar(k == SDLK_RIGHT ? 1 : -1); return 1; }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER) { fopMudar(0); return 1; }
    return 1;
  }
  if (k == SDLK_LEFT)  { if (fontesCol > 0) fontesCol--; return 1; }
  if (k == SDLK_RIGHT) { if (fontesCol + 1 < fontesN()) fontesCol++; return 1; }
  if (k == SDLK_UP)    { focoFontes = 0; focoTopo = 1; topoCol = modoLista ? G_TOPO_LISTA : G_TOPO_CARTOES; return 1; }
  if (k == SDLK_DOWN)  { focoFontes = 0; focoValido(); return 1; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
    if (!fontesOkDesde) { fontesOkDesde = agora ? agora : 1; fontesOkLongo = 0; }
    return 1;
  }
  return 0;
}

// Both the primary row and Options dispatch the existing guide actions.
static void topoAcionar(int item) {
  if (item == G_TOPO_BUSCAR) buscaAbrir();
  else if (item == G_TOPO_CATEGORIAS) catAbrir(2, 0);
  else if (item == G_TOPO_OPCOES) { opcoesAberto = 1; opcoesFoco = G_TOPO_ADDONS; }
  else if (item == G_TOPO_ADDONS) painelAbrir();
  else if (item == G_TOPO_FILTROS) filtrosAbrir();
  else if (item == G_TOPO_DIAG) pedLivetvDiag = 1;
  else if (item == G_TOPO_LIMPAR) { limparConfirm = 1; limparFoco = 1; }
  else if (item == G_TOPO_PREVIEW) {
    previewLigado = !previewLigado;
    previewGravar();
    if (previewLigado) { previewUltLin = previewUltCol = -1; previewFocoMudou = 0; }
    else previewParar();
  }
  else if ((item == G_TOPO_LISTA) != modoLista) alternarModo();
}

void guia_evento(const SDL_Event *e) {
  if (!guia_visivel()) return;
#ifdef NV_TOUCH_UI
  if (toquerol_navegacao(e)) toqueGuiaRetomar();
#endif
  if (buscaEvento(e)) return;

  if (e->type == SDL_KEYUP) {
    SDL_Keycode k = e->key.keysym.sym;
    if (k == SDLK_UP || k == SDLK_DOWN) {
      dirSeg = 0;
      // Soltou a seta que abriu o painel: a lista fica, com o foco onde parou.
      if (catAberto == 1) { catAberto = 2; catUlt = SDL_GetTicks(); }
    }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
      if (okDesde && !okLongo) {
        if (ponteiro_ok_longo()) { GCanal *c = linhaItem(focoLin, focoCol); if (c) favAlternar(c); }
        else acaoOk();
      }
      okDesde = 0; okLongo = 0;
      if (fontesOkDesde && !fontesOkLongo && focoFontes && !fopAberto) {
        int ch = fontesCol;
        fontesEscolher(ch);
        focoFontes = 0;
      }
      fontesOkDesde = 0; fontesOkLongo = 0;
    }
    return;
  }
  if (e->type != SDL_KEYDOWN) return;
  SDL_Keycode k = e->key.keysym.sym;
  Uint32 agora = SDL_GetTicks();
  // A dica sai na primeira tecla; a tecla segue valendo.
  dicaDesde = 0;

  if (opcoesAberto) {
    if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
        k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
      opcoesAberto = 0; return;
    }
    if (k == SDLK_UP && opcoesFoco > G_TOPO_ADDONS) opcoesFoco--;
    if (k == SDLK_DOWN && opcoesFoco < G_TOPO_PREVIEW) opcoesFoco++;
    if ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && !e->key.repeat) {
      opcoesAberto = 0;
      topoAcionar(opcoesFoco);
    }
    return;
  }
  if (gpAberto) {
    if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
        k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) { gpAberto = 0; return; }
    gpEvento(k, e->key.repeat, agora);
    return;
  }
  // CONFIRMACAO DO LIMPAR: so esta camada ve tecla enquanto aberta.
  if (limparConfirm) {
    if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
        k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) { limparConfirm = 0; return; }
    if (k == SDLK_LEFT || k == SDLK_RIGHT) { limparFoco = !limparFoco; return; }
    if ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && !e->key.repeat) {
      limparConfirm = 0;
      if (limparFoco == 0) guia_limpar_lista();
    }
    return;
  }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    if (fopAberto) { fopAberto = 0; return; }
    if (focoFontes && !painel && !catAberto) { focoFontes = 0; return; }
    sair(); return;
  }
  // NA FAIXA, AZUL ABRE O GUIA COMPLETO (o canal no ar encolhe para o
  // preview; quem faz e o app, por guia_pediu_guia_cheio). O repouso de
  // G_OVERLAY_REP_MS ignora a repeticao do firmware do botao que ABRIU a
  // faixa, senao segurar o azul pulava direto para o guia completo.
  if (overlay && (k == SDLK_F10 || k == SDLK_MENU)) {
    overlay = 0; pedGuiaCheio = 1; return;
  }
  if (overlay && (k == SDLK_s || e->key.keysym.scancode == NV_SCANCODE_BLUE)) {
    if (agora - overlayDesde < G_OVERLAY_REP_MS) return;
    overlay = 0; pedGuiaCheio = 1; return;
  }
  // O resto da faixa: CIMA/BAIXO andam de canal em canal (atravessando as
  // categorias), ESQUERDA/DIREITA andam o tempo. OK tunes another channel
  // or opens the full guide on the playing channel. Every key delays hiding.
  if (overlay) {
    bandaUlt = agora;
    if (k == SDLK_UP || k == SDLK_DOWN) {
      if (agora - overlayDesde < G_OVERLAY_REP_MS && k == SDLK_DOWN) return;
      moverLista(k == SDLK_DOWN ? 1 : -1);
      return;
    }
    if (k == SDLK_LEFT)  { if (janelaDesl > 0) janelaDesl -= G_L_PASSO_MIN; return; }
    if (k == SDLK_RIGHT) { if (janelaDesl < G_L_DESL_MAX) janelaDesl += G_L_PASSO_MIN; return; }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
      // Require a fresh press: the opening OK may repeat past the time guard.
      if (e->key.repeat || agora - overlayDesde < G_OVERLAY_REP_MS) return;
      if (!okDesde) { okDesde = agora; okLongo = 0; }
      return;
    }
    if (e->key.keysym.scancode == NV_SCANCODE_CH_UP ||
        e->key.keysym.scancode == NV_SCANCODE_CH_DOWN) {
      saltarCat(e->key.keysym.scancode == NV_SCANCODE_CH_UP ? -1 : 1);
      return;
    }
    return;
  }

  // O painel de addons captura tudo que nao e Voltar (tratado acima).
  if (painel) { painelEvento(k, agora); return; }

  // O painel de categorias captura as setas e o OK enquanto aberto.
  if (catAberto) {
    catUlt = agora;
    if (k == SDLK_UP || k == SDLK_DOWN) {
      int dir = k == SDLK_DOWN ? 1 : -1;
      if (catAberto == 1) {
        // Segurando: um passo a cada G_CAT_PASSO_MS, nao a cada repeticao
        // do firmware (~130 ms), que atravessava dez secoes num piscar.
        dirTick = agora;
        if (agora - ultNavCat >= G_CAT_PASSO_MS) { catMover(dir); ultNavCat = agora; }
      } else catMover(dir);
      return;
    }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_RIGHT) { catConfirmar(); return; }
    if (k == SDLK_LEFT) { catFechar(); return; }
    return;
  }

  // VERDE abre o diagnostico da Live TV (`d` no teclado do Mac). Fora da
  // faixa: por cima do video ela e o zapping, e a tela nova pararia o canal.
  if (!overlay && (e->key.keysym.scancode == G_SCANCODE_GREEN || k == SDLK_d)) {
    pedLivetvDiag = 1; return;
  }
  // AMARELO alterna lista e cartoes de qualquer lugar. `l` no teclado do Mac
  // e o equivalente de bancada, como `s` e do azul.
  if (e->key.keysym.scancode == G_SCANCODE_YELLOW || k == SDLK_l) {
    dirSeg = 0; catAberto = 0;
    alternarModo();
    return;
  }

  // CH+/- do controle da LG (scancodes 480/481 do SDL_webOS.h): dentro do
  // guia eles pulam CATEGORIA, nao canal — e a mesma leitura do "segurar"
  // sem exigir o gesto. No .wgt o CH+ chega como "s" (tizen-shell.html),
  // azul de mentira, e o CH- como CH_DOWN cru.
  if (e->key.keysym.scancode == NV_SCANCODE_CH_UP ||
      e->key.keysym.scancode == NV_SCANCODE_CH_DOWN ||
      (!overlay && k == SDLK_s)) {
    focoTopo = 0;
    saltarCat(e->key.keysym.scancode == NV_SCANCODE_CH_UP ? -1 : 1);
    return;
  }

  if (fopAberto) { dirSeg = 0; fontesEvento(k, agora); return; }
  if (focoFontes && barraVisivel()) { dirSeg = 0; if (fontesEvento(k, agora)) return; }
  else focoFontes = 0;

  // Foco no cabecalho: ESQUERDA/DIREITA entre os tres controles, BAIXO volta
  // a barra de fontes (ou as linhas), OK age. CIMA nao faz nada.
  if (focoTopo) {
    dirSeg = 0;
    if (k == SDLK_LEFT)  { if (topoCol > 0) topoCol--; return; }
    if (k == SDLK_RIGHT) { if (topoCol + 1 < G_TOPO_PRIMARIOS) topoCol++; return; }
    if (k == SDLK_DOWN)  { focoTopo = 0; if (barraVisivel()) fontesEntrar(); return; }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
      if (!e->key.repeat) topoAcionar(topoCol);
      return;
    }
    return;
  }

  if (k == SDLK_UP || k == SDLK_DOWN) {
    int dir = (k == SDLK_DOWN) ? 1 : -1;
    int fresco = !e->key.repeat && !(dirSeg == k && agora - dirTick < G_REP_MS);
    // Only a fresh Up at the first channel may leave the channel list.
    if (dir < 0 && fresco && focoLin == 0 && (!modoLista || focoCol == 0)) {
      focoTopo = 1; focoFontes = 0;
      topoCol = modoLista ? G_TOPO_LISTA : G_TOPO_CARTOES;
      dirSeg = 0;
      return;
    }
    // Held Up/Down keep scrolling; the movement helpers clamp at the edges.
    dirSeg = k;
    dirTick = agora;
    if (modoLista) moverLista(dir); else moverVertical(dir);
    return;
  }
  // Qualquer outra tecla desarma a direcao segurada.
  dirSeg = 0;

  // No modo lista ESQUERDA/DIREITA andam a JANELA DE TEMPO, meia hora por
  // toque, ate 3 h a frente — a pergunta "o que passa mais tarde" que a
  // grade tradicional responde e o cartao nao.
  //
  // ESQUERDA SEM PARA ONDE IR (primeira coluna, ou janela de tempo ja no
  // "agora") SOBE AO CABECALHO — Cartoes/Lista/Addons — de qualquer linha.
  // Pedido do dono (21/09/2026): "se quiser trocar algo tem que subir a lista
  // toda". BAIXO no cabecalho volta para a linha em que estava.
  if (modoLista) {
    if (k == SDLK_LEFT)  { if (janelaDesl > 0) janelaDesl -= G_L_PASSO_MIN;
                           else { if (barraVisivel()) fontesEntrar(); else { focoTopo = 1; topoCol = G_TOPO_LISTA; } dicaUsou(); } return; }
    if (k == SDLK_RIGHT) { if (janelaDesl < G_L_DESL_MAX) janelaDesl += G_L_PASSO_MIN; return; }
  } else {
    if (k == SDLK_LEFT)  { if (focoCol > 0) focoCol--;
                           else { if (barraVisivel()) fontesEntrar(); else { focoTopo = 1; topoCol = G_TOPO_CARTOES; } dicaUsou(); } return; }
    if (k == SDLK_RIGHT) { if (focoCol + 1 < linhaN(focoLin)) focoCol++; return; }
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
    if (!okDesde) { okDesde = agora; okLongo = 0; }
    return;
  }
}

// Ver o prototipo, junto de xtFalha.
static int xtAviso(char *sub, size_t tam) {
  XtreamConta c;
  long long agora = (long long)time(NULL);
  int a;
  if (!xtream_conta(&c)) return 0;
  a = xtream_conta_aviso(&c, agora);
  switch (a) {
    case XA_EXPIRADA:
      snprintf(sub, tam, i18n("%d canais · %d categorias · assinatura Xtream vencida"), nCanais, nCats);
      return 1;
    case XA_DESATIVADA: case XA_RECUSOU:
      snprintf(sub, tam, i18n("%d canais · %d categorias · conta Xtream desativada"), nCanais, nCats);
      return 1;
    case XA_TELAS_CHEIAS:
      snprintf(sub, tam, i18n("%d canais · %d categorias · telas do Xtream em uso (%d de %d)"),
               nCanais, nCats, c.conexoes, c.maxConexoes);
      return 1;
    case XA_VENCE_LOGO:
      snprintf(sub, tam, i18n("%d canais · %d categorias · o Xtream vence em %d dia(s)"),
               nCanais, nCats, (int)((c.expira - agora) / 86400) + 1);
      return 1;
    default: return 0;
  }
}

// --- de que pais vem a grade (#158) ---------------------------------------
// A ESCOLHA MANUAL (Ajustes > Conteudo > Guia TV) manda. No automatico, nesta
// ordem, ate tres paises:
//   1. o prefixo de pais dos canais e das categorias do Xtream ("RO: Pro TV",
//      "RO | SPORT", "|RO| Antena 1") ou o nome do pais na categoria
//      ("ROMANIA"), quando cobre pelo menos 5% dos canais do Xtream (e 10
//      canais). E a dica mais especifica que existe: diz de onde vem a lista
//      que a pessoa assina, e nao de onde e a pessoa.
//   2. a regiao do idioma dos metadados ("ro-RO" -> RO, "pt-BR" -> BR);
//   3. o idioma do app (portugues = as cinco de sempre, romeno = RO).
// Nada disso = as cinco de sempre (epg_paises_definir("")), que e o que quem
// nunca mexeu ja via.
typedef struct { const char *palavra, *pais; } GPaisPalavra;
static const GPaisPalavra PAIS_PALAVRA[] = {
  { "romania", "RO" }, { "rom\xc3\xa2nia", "RO" }, { "romana", "RO" }, { "romanian", "RO" },
  { "portugal", "PT" }, { "brasil", "BR" }, { "brazil", "BR" }, { "espana", "ES" },
  { "espa\xc3\xb1""a", "ES" }, { "spain", "ES" }, { "italia", "IT" }, { "italy", "IT" },
  { "france", "FR" }, { "deutschland", "DE" }, { "germany", "DE" }, { "turkey", "TR" },
  { "turkiye", "TR" }, { "greece", "GR" }, { "bulgaria", "BG" }, { "hungary", "HU" },
  { "magyar", "HU" }, { "serbia", "RS" }, { "croatia", "HR" }, { "hrvatska", "HR" },
  { "netherlands", "NL" }, { "nederland", "NL" }, { "albania", "AL" }, { "mexico", "MX" },
  { "argentina", "AR" }, { "united kingdom", "UK" },
};

static const char *paisDoTexto(const char *t, char buf[4]) {
  char low[96];
  size_t i;
  int k;
  epg_sem_prefixo(t, buf);
  if (buf[0] && epg_pais_existe(buf)) return buf;
  for (i = 0; t[i] && i < sizeof low - 1; i++)
    low[i] = (char)((t[i] >= 'A' && t[i] <= 'Z') ? t[i] + 32 : t[i]);
  low[i] = 0;
  for (k = 0; k < (int)(sizeof PAIS_PALAVRA / sizeof PAIS_PALAVRA[0]); k++)
    if (strstr(low, PAIS_PALAVRA[k].palavra)) return PAIS_PALAVRA[k].pais;
  return NULL;
}

static void epgPaisesEscolher(void) {
  char lista[24] = "";
  const char *manual = ajustes_epg_pais();
  int n = 0;
  #define ADD(c) do { if ((c) && (c)[0] && n < 3 && !strstr(lista, (c)) && epg_pais_existe(c)) { \
      size_t ln_ = strlen(lista); \
      snprintf(lista + ln_, sizeof lista - ln_, "%s%.2s", ln_ ? "," : "", (c)); n++; } } while (0)
  if (manual[0]) { epg_paises_definir(manual); return; }
  { // 1. dicas da lista do Xtream
    char cod[8][3]; int cont[8], nc = 0, nXt = 0, i, j;
    for (i = 0; i < nCanais; i++) {
      char b[4]; const char *p;
      if (!xtream_e_id(canais[i].id)) continue;
      nXt++;
      p = paisDoTexto(canais[i].nome, b);
      if (!p && canais[i].cat >= 0 && canais[i].cat < nCats) p = paisDoTexto(cats[canais[i].cat].nome, b);
      if (!p) continue;
      for (j = 0; j < nc && strcmp(cod[j], p); j++) {}
      if (j == nc) { if (nc == 8) continue; snprintf(cod[nc], 3, "%s", p); cont[nc++] = 0; }
      cont[j]++;
    }
    for (;;) {                                   // maior contagem primeiro
      int m = -1;
      for (j = 0; j < nc; j++) if (cont[j] > 0 && (m < 0 || cont[j] > cont[m])) m = j;
      if (m < 0) break;
      if (cont[m] >= 10 && cont[m] * 20 >= nXt) ADD(cod[m]);
      cont[m] = 0;
    } }
  { // 2. regiao do idioma dos metadados
    const char *t = ajustes_tmdb_idioma();
    if (t && strlen(t) == 5 && t[2] == '-') { char r[3] = { t[3], t[4], 0 }; if (!strcmp(r, "GB")) snprintf(r, 3, "UK"); ADD(r); } }
  // 3. idioma do app
  if (ajustes_idioma() == IDIOMA_RO) ADD("RO");
  #undef ADD
  // Portugues (e o pt-BR que o "da interface" devolve) cai nas cinco de sempre
  // sem precisar dizer: BR sozinho seria menos do que quem ja usa tinha.
  if (!strcmp(lista, "BR") || !strcmp(lista, "PT") || !strcmp(lista, "BR,PT") || !strcmp(lista, "PT,BR"))
    lista[0] = 0;
  epg_paises_definir(lista);
}

// --- EPG por canal (M5: cascata, manual e cobertura; M6: fuso) --------------------
// A ESCOLHA MANUAL ("Escolher grade deste canal") vale por FONTE:CANAL e fica em
// ltv-epgmapa-p<N>.txt: "chave TAB id TAB fonteDaGrade TAB idDaGrade" ("-" =
// sem grade). E a ultima palavra: vence o id e o nome.
#define G_MAX_MAPA 600
typedef struct { char chave[LTV_CHAVE]; char id[80]; char gf[LTV_CHAVE]; char gid[96]; } GMapa;
static GMapa mapa[G_MAX_MAPA]; static int nMapa, mapaPerfil = -999;
static void mapaNome(char *d, size_t n) {
  int p = perfis_ativo();
  if (p <= 0) snprintf(d, n, "ltv-epgmapa.txt"); else snprintf(d, n, "ltv-epgmapa-p%d.txt", p);
}
static void mapaLer(void) {
  char nome[48], *t, *q;
  if (mapaPerfil == perfis_ativo()) return;
  mapaPerfil = perfis_ativo();
  nMapa = 0;
  mapaNome(nome, sizeof nome);
  t = dados_ler(nome);
  for (q = t; q && *q && nMapa < G_MAX_MAPA; ) {
    char *fim = q + strcspn(q, "\n"), *c[4];
    int k = 0;
    if (*fim) *fim++ = 0;
    c[0] = q;
    while (k < 3 && (c[k + 1] = strchr(c[k], '\t'))) { *c[k + 1]++ = 0; k++; }
    if (k == 3) {
      snprintf(mapa[nMapa].chave, sizeof mapa[0].chave, "%s", c[0]);
      snprintf(mapa[nMapa].id, sizeof mapa[0].id, "%s", c[1]);
      snprintf(mapa[nMapa].gf, sizeof mapa[0].gf, "%s", c[2]);
      snprintf(mapa[nMapa].gid, sizeof mapa[0].gid, "%s", c[3]);
      nMapa++;
    }
    q = fim;
  }
  free(t);
}
static void mapaGravar(void) {
  static char buf[G_MAX_MAPA * 240];
  char nome[48];
  size_t u = 0;
  int i;
  for (i = 0; i < nMapa && u + 240 < sizeof buf; i++)
    u += (size_t)snprintf(buf + u, sizeof buf - u, "%s\t%s\t%s\t%s\n", mapa[i].chave, mapa[i].id,
                          mapa[i].gf, mapa[i].gid);
  buf[u] = 0;
  mapaNome(nome, sizeof nome);
  dados_gravar(nome, buf);
}
static GMapa *mapaDe(const GCanal *c) {
  int i;
  mapaLer();
  for (i = 0; i < nMapa; i++)
    if (!strcmp(mapa[i].id, c->id) && !strcmp(mapa[i].chave, chaveDo(c))) return &mapa[i];
  return NULL;
}
// gid NULL = volta ao automatico; "-" = sem grade; senao o canal da grade.
static void mapaDefinir(GCanal *c, const char *gf, const char *gid) {
  GMapa *m = mapaDe(c);
  if (!gid) {
    if (m) { int i = (int)(m - mapa); memmove(&mapa[i], &mapa[i + 1], sizeof mapa[0] * (size_t)(nMapa - i - 1)); nMapa--; }
  } else {
    if (!m) {
      if (nMapa >= G_MAX_MAPA) { memmove(mapa, mapa + 1, sizeof mapa[0] * (G_MAX_MAPA - 1)); nMapa--; }
      m = &mapa[nMapa++];
      snprintf(m->chave, sizeof m->chave, "%s", chaveDo(c));
      snprintf(m->id, sizeof m->id, "%s", c->id);
    }
    snprintf(m->gf, sizeof m->gf, "%s", gf ? gf : "");
    snprintf(m->gid, sizeof m->gid, "%s", gid);
  }
  mapaGravar();
  c->epg = -1;
  printf("[ltv] grade de um canal da fonte %s: %s\n", chaveDo(c),
         !gid ? "automatica" : !strcmp(gid, "-") ? "nenhuma (escolha)" : "escolhida a mao");
  fflush(stdout);
}

// Resolve e guarda o indice da grade. -2 marca "sem grade real" para nao
// consultar de novo a cada quadro.
static int epgDo(GCanal *c) {
  if (c->epg == -1) {
    GMapa *m;
    int met = EPG_M_NENHUM;
    m = mapaDe(c);
    if (m && !strcmp(m->gid, "-")) { c->epg = -2; c->epgMetodo = EPG_M_MANUAL; return -2; }
    if (m && (c->epg = epg_achar(m->gf, m->gid)) >= 0) { c->epgMetodo = EPG_M_MANUAL; return c->epg; }
    c->epg = epg_casar(chaveDo(c), c->epgId, c->tvgNome, c->nome, &met);
    c->epgMetodo = (short)met;
    if (c->epg < 0) {
      int estadoFonte=epg_fonte_estado(chaveDo(c));
      if (estadoFonte == EPG_FALHOU) return -2;
      if (estadoFonte != EPG_PRONTO) return -1;
      c->epg = -2;
    }
  }
  return c->epg;
}

// O FUSO DA FONTE (M6): grade de provedor com hora deslocada (o classico "tudo
// 1 h adiantado", ou 5-8 h num painel de outro continente). A pessoa acerta por
// fonte nas opcoes da fonte; a grade guarda epoch e o deslocamento entra na
// consulta e na resposta.
static int fusoDe(const GCanal *c) {
  return c->fonte >= 0 && c->fonte < nLtvF ? ltvF[c->fonte].fusoS : 0;
}
static void fusoAplicar(EpgProg *p, int off) { if (off) { p->ini += off; p->fim += off; } }

// A GRADE DE UM CANAL, venha de onde vier (#158): primeiro a XMLTV (epg.c:
// provedor, epgshare01 do pais), e, para canal Xtream que nao casou nela, a
// grade curta do proprio painel (xtepg.c).
static int xtCurta(GCanal *c) {
  if (!xtream_e_id(c->id)) return 0;
  xtepg_querer(c->id);
  return 1;
}
static int gAgora(GCanal *c, time_t t, EpgProg *p) {
  int epg = epgDo(c), off = fusoDe(c), r;
  if (epg >= 0) r = epg_agora(epg, t - off, p);
  else r = xtCurta(c) && xtepg_agora(c->id, t - off, p);
  if (r) fusoAplicar(p, off);
  return r;
}
static int gProximo(GCanal *c, time_t t, int k, EpgProg *p) {
  int epg = epgDo(c), off = fusoDe(c), r;
  if (epg >= 0) r = epg_proximo(epg, t - off, k, p);
  else r = xtCurta(c) && xtepg_proximo(c->id, t - off, k, p);
  if (r) fusoAplicar(p, off);
  return r;
}
// 0 = available, -1 = pending, -2 = no schedule, -3 = failed.
static int gEstadoGrade(GCanal *c) {
  int epg = epgDo(c);
  if (epg >= 0) return 0;
  if (xtCurta(c)) {
    if (xtepg_tem(c->id)) return 0;
    int estadoCurto = xtepg_estado(c->id);
    if (estadoCurto == XTEPG_CARREGANDO) return -1;
    if (estadoCurto == XTEPG_FALHOU) return -3;
    if (estadoCurto == XTEPG_VAZIO) return -2;
  }
  return epg_fonte_estado(chaveDo(c)) == EPG_FALHOU ? -3 : epg;
}
static int gFaixaDesde(GCanal *c, time_t de, time_t ate, int pular, EpgProg *out, int cap) {
  int epg = epgDo(c), off = fusoDe(c), n;
  if (epg >= 0) n = epg_faixa_desde(epg, de - off, ate - off, pular, out, cap);
  else n = xtCurta(c) ? xtepg_faixa_desde(c->id, de - off, ate - off, pular, out, cap) : 0;
  for (int i = 0; out && i < n && i < cap; i++) fusoAplicar(&out[i], off);
  return n;
}
static int gFaixa(GCanal *c, time_t de, time_t ate, EpgProg *out, int cap) {
  int epg = epgDo(c), off = fusoDe(c), n, i;
  if (epg >= 0) n = epg_faixa(epg, de - off, ate - off, out, cap);
  else n = xtCurta(c) ? xtepg_faixa(c->id, de - off, ate - off, out, cap) : 0;
  for (i = 0; out && i < n && i < cap; i++) fusoAplicar(&out[i], off);
  return n;
}

// COBERTURA DA GRADE POR FONTE (M5/M6): em fundo, alguns canais por quadro (as
// regras de prefixo varrem a grade inteira), ate a lista toda estar resolvida.
// Ai sai UMA linha [ltv] por fonte: canais, grade em %, por metodo, idade da
// grade, fuso. E o numero que mede as mudancas do plano.
static int cobCursor, cobFeita;
static void epgCoberturaZerar(void) {
  int i, k;
  cobCursor = 0; cobFeita = 0;
  for (i = 0; i < nLtvF; i++) {
    ltvF[i].epgResolvidos = ltvF[i].epgCom = 0;
    for (k = 0; k <= EPG_M_MANUAL; k++) ltvF[i].epgPor[k] = 0;
  }
}
static void epgCoberturaPasso(Uint32 orcamentoMs) {
  Uint32 t0;
  int i;
  if (cobFeita || epg_estado() != EPG_PRONTO || estado != G_PRONTO || !nCanais) return;
  t0 = SDL_GetTicks();
  while (cobCursor < nCanais && SDL_GetTicks() - t0 < orcamentoMs) {
    GCanal *c = &canais[cobCursor++];
    int e = epgDo(c);
    if (c->fonte < 0 || c->fonte >= nLtvF) continue;
    ltvF[c->fonte].epgResolvidos++;
    if (e >= 0 && epg_total(e) > 0) {
      ltvF[c->fonte].epgCom++;
      if (c->epgMetodo >= 0 && c->epgMetodo <= EPG_M_MANUAL) ltvF[c->fonte].epgPor[c->epgMetodo]++;
    }
  }
  if (cobCursor < nCanais) return;
  cobFeita = 1;
  { EpgFonteInfo inf[EPG_MAX_EXTRAS];
    int ni = epg_fontes_info(inf, EPG_MAX_EXTRAS);
    for (i = 0; i < nLtvF; i++) {
      GFonteLtv *F = &ltvF[i];
      int pct = F->nCanais ? F->epgCom * 100 / F->nCanais : 0, k;
      const char *grade = "epgshare01";
      long idade = -1;
      for (k = 0; k < ni; k++)
        if (!strcmp(inf[k].chave, F->chave)) {
          grade = inf[k].ok ? (inf[k].deCache ? "provedor(cache)" : "provedor") : "provedor(falhou)";
          idade = inf[k].idadeS;
        }
      // Saude: lista do cache ou grade quase vazia num provedor com grade = ambar.
      if (F->saude == LTV_SAUDE_OK && (F->deCache || (strncmp(grade, "epgshare", 8) && pct < 20)))
        F->saude = LTV_SAUDE_DEGRADADA;
      printf("[ltv] fonte=%s tipo=%s canais=%d grupos=%d grade=%d%% (id %d, nome %d, apelido %d, manual %d) "
             "fonte_grade=%s idade=%ldh fuso=%+dmin%s\n",
             F->chave, ltv_tipo_nome(F->tipo), F->nCanais, F->nGrupos, pct, F->epgPor[EPG_M_ID],
             F->epgPor[EPG_M_NOME], F->epgPor[EPG_M_ALIAS], F->epgPor[EPG_M_MANUAL], grade,
             idade >= 0 ? idade / 3600 : 0, F->fusoS / 60, F->cortada ? " cortada" : "");
    }
    fflush(stdout); }
}

// Quando a grade EPG e (re)publicada, os indices guardados morrem — a troca
// inteira do vetor torna todo -1 de novo.
static int epgRev;
static unsigned epgVista;
static void epgPasso(void) {
  // Trocou o pais em Ajustes com o guia aberto: refaz a escolha (o epg_passo
  // abaixo ve a troca e recarrega).
  { static char ultimo[4] = "?";
    if (strcmp(ultimo, ajustes_epg_pais())) {
      snprintf(ultimo, sizeof ultimo, "%s", ajustes_epg_pais());
      epgPaisesEscolher();
    } }
  epg_passo();
  xtepg_passo();
  GCanal *canalFoco = linhaItem(focoLin, focoCol);
  if (canalFoco) xtepg_priorizar(canalFoco->id);
  if (epgVista != epg_revisao()) {
    epgRev++;
    for (int i = 0; i < nCanais; i++) canais[i].epg = -1;
    epgCoberturaZerar();
  }
  epgVista = epg_revisao();
  epgCoberturaPasso(3);
}

// Posicao vertical (antes da rolagem) do canal `c` da linha `l` no modo
// lista, e a altura total. Linhas de altura desigual (cabecalho x canal)
// impedem a conta fechada que o modo cartoes usa.
static float listaYDe(int l, int c) {
  float y = 0.0f;
  for (int i = 0; i < l; i++) y += G_L_HEAD + (float)linhaN(i) * G_L_ROW;
  return y + G_L_HEAD + (float)c * G_L_ROW;
}
static float listaAltura(void) {
  float y = 0.0f;
  for (int i = 0; i < nLinhas(); i++) y += G_L_HEAD + (float)linhaN(i) * G_L_ROW;
  return y;
}

// Posicao de cada item do painel de addons, relativa ao topo da lista. Os
// numeros fixos sao a altura do rotulo de secao (36) e, na segunda secao, o
// respiro (28) + rotulo (36) + a frase de duas linhas sobre curadoria (60).
// desenharPainelAddons usa as mesmas contas — mude aqui e la.
static float paItemY(int i) {
  int n = paN;
  if (paModo == 1) {
    if (fNivel || i < 3) return 36.0f + (float)i * G_PF_ROW;
    return 36.0f + 3.0f * G_PF_ROW + 28.0f + 36.0f + (float)(i - 3) * G_PF_ROW;
  }
  if (i < n) return 36.0f + (float)i * G_PA_ROW;
  return 36.0f + (float)n * G_PA_ROW + 28.0f + 36.0f + 60.0f + (float)(i - n) * G_PA_ROW_REC;
}
#define G_PA_LISTA_Y 200.0f
// Base do conteudo rolavel do painel (ultimo item + a altura dele).
static float paFim(void) {
  int n = painelN();
  if (n <= 0) return 0.0f;
  return paItemY(n - 1) + (paModo == 1 ? G_PF_ROW : nRec > 0 ? G_PA_ROW_REC : G_PA_ROW);
}

void guia_sugestoes_mudaram(void) {
  if (fioVivo) { cargaCancelada = 1; recarregarPend = 1; }
  else { estado = G_PARADO; ultTentativa = 0; }
}

// Ajustes mexeu nas listas M3U / guias XMLTV por URL (M9): o que ja esta na
// tela recarrega para a fonte nova entrar (ou a removida sair). Sem lista
// carregada nao ha o que refazer: a proxima carga ja le as URLs.
void guia_fontes_mudaram(void) {
  ltv_urls_carregar(perfis_ativo());
  if (nCanais <= 0 && estado == G_PARADO) return;
  if (fioVivo) recarregarPend = 1;
  else { estado = G_PARADO; ultTentativa = 0; iniciarCarga(); }
}

// Explicit reset: discard this profile's lists, then reload configured sources.
void guia_limpar_lista(void) {
  char nome[48], *t, *p;
  int i;
  if (fioVivo) cargaCancelada = 1;
  cacheNomeIndice(nome, sizeof nome);
  t = dados_ler(nome);
  if (t) {
    for (p = t; *p; ) {
      char ch[LTV_CHAVE];
      size_t k = strcspn(p, "\r\n");
      if (k && k < sizeof ch) {
        memcpy(ch, p, k); ch[k] = 0;
        cacheFonteApagar(ch);
      }
      p += k; while (*p == '\r' || *p == '\n') p++;
    }
    free(t);
  }
  for (i = 0; i < nLtvF; i++) cacheFonteApagar(ltvF[i].chave);
  cacheNomeIndice(nome, sizeof nome);
  dados_gravar(nome, "");
  fontesLtvSoltar(ltvF, nLtvF); nLtvF = 0;
  nCanais = nCats = 0;
  filtro = G_FILTRO_TODAS; filtroChave[0] = 0;
  fopAberto = gpAberto = 0;
  assinaturaPublicada = 0;
  empacotar(); focoValido();
  epg_extras_definir(NULL, 0);
  xtepg_limpar();
  if (fioVivo) recarregarPend = 1;
  else { estado = G_PARADO; ultTentativa = 0; iniciarCarga(); }
}

int guia_atualizando_lista(void) {
  return recargaDoPainel && !painel && (fioVivo || recarregarPend);
}

static void drenarCarga(void) {
  if (atomic_load_explicit(&pendPronto, memory_order_acquire)) {
    int atual = cargaAtual();
    if (sInventarioCat[0]) {
      if (atual && painel && paModo == 1 && fNivel &&
          !strcmp(fCatNome, sInventarioCat) && !strcmp(fInventarioCat, sInventarioCat) &&
          sInventarioInicio == fInventarioInicio && sNInventario > 0) {
        fN = 0;
        for (int i = 0; i < sNInventario; i++) {
          const GInventario *v = &sInventario[i];
          fCanalAdd(v->fonte, v->id, v->nome);
        }
        for (int i = 0; i < fN; i++)
          fIt[i].oculto = guiafiltro_canal_oculto_fonte(&fltLive, fIt[i].fonte, fIt[i].id);
        fProximo = sInventarioMais ? -(sInventarioInicio + sNInventario + 1) : 0;
        if (fProximo) {
          snprintf(fIt[fN].nome, sizeof fIt[fN].nome, "%s", i18n("Carregar mais"));
          fIt[fN++].n = -1;
        }
        paFoco = 0; paRol = paVelRol = 0.0f;
      }
      if (!strcmp(fInventarioCat, sInventarioCat) && sInventarioInicio == fInventarioInicio)
        fInventarioCat[0] = 0;
      sInventarioCat[0] = 0;
      stagingZerar();
      sOk = nLtvF > 0;
    } else if (atual) {
      publicar();
      if (painel && paModo == 1) {
        int item = fNivel ? paFoco : paFoco - 3;
        GFItem foco = {0};
        if (item >= 0 && item < fN) foco = fIt[item];
        if (fNivel) fMontarCanais(fCatNome);
        else fMontarCats();
        for (int i = 0; i < fN; i++) {
          if (fNivel ? (!strcmp(fIt[i].fonte, foco.fonte) && !strcmp(fIt[i].id, foco.id)) :
                       !strcmp(fIt[i].nome, foco.nome)) {
            paFoco = fNivel ? i : 3 + i;
            break;
          }
        }
        if (paFoco >= painelN()) paFoco = painelN() - 1;
        if (paFoco < 0) paFoco = 0;
      }
    }
    // Quem decide e o desenho: falha da carga continua G_FALHOU, e e ela que
    // faz guia_carregar tentar de novo.
    estado = atual ? (sOk ? G_PRONTO : G_FALHOU) : G_PARADO;
    atomic_store_explicit(&pendPronto, 0, memory_order_relaxed);
    fioVivo = 0;
    if (atual) {
      focoValido();
      if (painel && paModo == 0) painelMontar();   // sabe[] mudou: quem nao fornece canal sai
    }
  }
}

void guia_atualizar(float dt, Uint32 agora) {
#ifdef NV_TOUCH_UI
  if (!telefoneui_ativo()) toqueLimpar();
#endif
  if (guia_visivel()) gTopoMedir();
  entrada = anim_mola(entrada, guia_visivel() ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  int prontoPend = atomic_load_explicit(&pendPronto, memory_order_acquire);
  if (recargaDoPainel && !fioVivo && !recarregarPend && !prontoPend) recargaDoPainel = 0;
  int escolha=ajustes_guia_fonte();
  if(fonteAtual!=escolha) { fonteAtual=escolha; vistaMontar(); focoValido(); }
  drenarCarga();
  // Configuracao mudou fora do guia (inclusive pull da conta): remove agora.
  for (int i = nLtvF - 1; i >= 0; i--)
    if (!chaveConfigurada(ltvF[i].chave)) guia_fonte_esquecer(ltvF[i].chave);
  // O painel de addons pediu recarga com o fio ainda vivo: agora que ele
  // acabou, vai.
  if (recarregarPend && !fioVivo) {
    recarregarPend = 0; estado = G_PARADO; ultTentativa = 0; iniciarCarga();
  }
  // PREVIEW: aplica a URL que o fio buscou (no fio de desenho, nunca no de
  // rede) e arranca o preview quando o foco descansa num canal novo.
  // The app may open the band after capturing this frame's timestamp.
  if (overlay && (Sint32)(agora - bandaUlt) > (Sint32)G_B_OCIOSO_MS) { overlay = 0; okDesde = 0; }
  // O foco mudou de canal: o relogio do descanso recomeca.
  { static int ul = -1, uc = -1;
    if (focoLin != ul || focoCol != uc) { ul = focoLin; uc = focoCol; previewFocoMudou = agora; } }
  // Com o preview LIGADO o descanso do foco troca o que toca, inclusive com
  // canal adotado no ar (conta limitada: o app fecha a sessao antes de abrir
  // a nova — uma conexao so). Preview desligado: o canal no ar fica.
  if (aberta && !overlay && previewLigado && !player_aberto() &&
      nCanais > 0 && (estado == G_PRONTO || estado == G_BAIXANDO) &&
      (focoLin != previewUltLin || focoCol != previewUltCol) &&
      agora - previewFocoMudou >= G_PREVIEW_ESPERA_MS) {
    GCanal *c = linhaItem(focoLin, focoCol);
    previewUltLin = focoLin; previewUltCol = focoCol;
    if (c && c->id[0] && strcmp(c->id, previewId)) previewPedir(c);
  }
  if (!guia_visivel()) { buscaFechar(); return; }
  buscaAtualizar(dt, agora);

  for (int i = 0; i < G_TOPO_N; i++)
    animTopo[i] = anim_mola(animTopo[i], focoTopo && topoCol == i ? 1.0f : 0.0f,
                            dt, NV_MOLA_FOCO);

  { GCanal *hc = linhaItem(focoLin, focoCol);
    const char *id = hc ? hc->id : "";
    if (strcmp(id, heroId)) {
      snprintf(heroId, sizeof heroId, "%s", id);
      if (heroA > 0.4f) heroA = 0.4f;
    }
    heroA = anim_mola(heroA, 1.0f, dt, 16.0f); }

  // Rolagem do modo lista: a linha focada fica a UMA linha do topo (a grade
  // abaixo do heroi mostra cinco; duas de contexto em cima deixavam o foco
  // quase no rodape), com o cabecalho da categoria dela visivel quando ela e
  // a primeira.
  if (modoLista && nLinhas() > 0) {
    float areaH = G_L_BASE - G_L_TOPO;
    float alvo = listaYDe(focoLin, focoCol) - G_L_HEAD - 1.0f * G_L_ROW;
    // +G_L_FADE: o ultimo canal para ACIMA do esmaecimento da base, e nao
    // debaixo dele (ver guia_desenhar).
    float maxY = listaAltura() + G_L_FADE - areaH;
    if (maxY < 0.0f) maxY = 0.0f;
    if (alvo > maxY) alvo = maxY;
    if (alvo < 0.0f) alvo = 0.0f;
#ifdef NV_TOUCH_UI
    if (!toquerol_livre(&toqueLista))
#endif
      rolL = anim_mola2(&velL, rolL, alvo, dt, NV_MOLA_SCROLL);
  }
  if (painel) {
    float areaH = NV_TELA_H - 80.0f - G_PA_LISTA_Y;
    float alvo = paItemY(paFoco) - areaH * 0.4f;
    float maxY = paFim() - areaH;
    if (maxY < 0.0f) maxY = 0.0f;
    if (alvo > maxY) alvo = maxY;
    if (alvo < 0.0f) alvo = 0.0f;
#ifdef NV_TOUCH_UI
    if (!toquerol_livre(&toqueAddon))
#endif
      paRol = anim_mola2(&paVelRol, paRol, alvo, dt, NV_MOLA_SCROLL);
  }

  epgPasso();
  // O catalogo pode ter chegado DEPOIS da abertura (descoberta ainda montando
  // as fileiras): tentar de novo ate aparecer uma fonte de canal.
  //
  // A RETENTATIVA NAO PODE DEPENDER DE `nFontes`, e era essa a trava.
  // descobrirFontes() le apenas as FILEIRAS publicadas da home, e o catalogo de
  // canal do FrostView nao vira fileira — quem o encontra e a sonda de
  // manifestos, que so roda DENTRO do fio de carga (o proprio iniciarCarga ja
  // diz isso: "sem portaria por nFontes"). Entao, com a primeira carga tendo
  // acontecido antes de os addons da conta chegarem, ficava assim para sempre:
  //   [guia] 0 canais em 0 categorias
  // e a tela dizendo "o guia precisa de um addon de canais", com o FrostView
  // instalado e respondendo 200. MEDIDO na LG: reabrir o guia nao consertava,
  // porque nada aqui chamava a carga de novo.
  //
  // Retry missing/failed catalogs every 10 s while visible. Successful filtered
  // emptiness stays ready until configuration changes or an explicit refresh.
  if (!nCanais && estado != G_PRONTO && estado != G_BAIXANDO &&
      (!ultTentativa || agora - ultTentativa > G_RETENTAR_MS)) {
    ultTentativa = agora ? agora : 1;
    descobrirFontes();
    iniciarCarga();
  }

  // Timeout do modo salta-categoria e da direcao segurada. O firmware nao
  // garante KEYUP, entao o silencio e o que vale.
  if (dirSeg && agora - dirTick > G_REP_MS) dirSeg = 0;
  // O firmware nao garante KEYUP: silencio de G_REP_MS na seta que abriu o
  // painel por segurar vale como soltura.
  if (catAberto == 1 && !dirSeg) { catAberto = 2; catUlt = agora; }
  if (catAberto == 2 && catUlt && agora - catUlt > G_CAT_OCIOSO_MS) catFechar();
  catAnim = anim_mola(catAnim, catAberto ? 1.0f : 0.0f, dt, 12.0f);
  if (catAberto || catAnim > 0.01f) {
    float areaH = G_CAT_BASE - G_CAT_TOPO;
    float alvo = (float)catFoco * G_CAT_ROW - (areaH - G_CAT_ROW) * 0.5f;
    float maxY = (float)nLinhas() * G_CAT_ROW - areaH;
    if (maxY < 0.0f) maxY = 0.0f;
    if (alvo > maxY) alvo = maxY;
    if (alvo < 0.0f) alvo = 0.0f;
#ifdef NV_TOUCH_UI
    if (!toquerol_livre(&toqueCat)) {
#endif
      if (catAnim < 0.02f) { catRol = alvo; catVelRol = 0.0f; }
      else catRol = anim_mola2(&catVelRol, catRol, alvo, dt, NV_MOLA2_SCROLL);
#ifdef NV_TOUCH_UI
    }
#endif
  }

  // OK segurado = favorito.
  // OK segurado num chip de fonte: as opcoes da fonte (Todas/Favoritos nao tem).
  if (fontesOkDesde && !fontesOkLongo && agora - fontesOkDesde >= NV_HOLD_MS) {
    fontesOkLongo = 1;
    if (focoFontes && fontesCol >= 2) { fopAberto = 1; fopModo = FOP_FONTE; fopFoco = 0; }
  }
  if (!okLongo && segurar_venceu(agora, okDesde, "guia")) {
    GCanal *c = linhaItem(focoLin, focoCol);
    // Na faixa sobre o video o OK segurado continua sendo o favorito; no guia
    // completo abre as opcoes do canal (Favorito, Escolher grade).
    if (c && overlay) favAlternar(c);
    else if (c) fopCanalAbrir(c);
    okLongo = 1;
  }

  // Rolagem vertical dos cartoes: com o heroi em cima sobra altura para UMA
  // fileira inteira e o cabecalho da seguinte, entao a fileira focada ancora
  // no topo da area. A ficha completa do canal ja esta no heroi; o que a
  // fileira de baixo precisa dizer e so "ha mais".
  { float alvo = (float)focoLin * G_PASSO_Y;
    float maxY = (float)(nLinhas() - 1) * G_PASSO_Y;
    if (maxY < 0.0f) maxY = 0.0f;
    if (alvo > maxY) alvo = maxY;
    if (alvo < 0.0f) alvo = 0.0f;
#ifdef NV_TOUCH_UI
    if (!toquerol_livre(&toqueGradeY))
#endif
      rolY = anim_mola2(&velY, rolY, alvo, dt, NV_MOLA_SCROLL); }
  // Rolagem horizontal da fileira em foco.
  { int l = focoLin;
    float passo = G_CARD_W + G_GAP_X;
    float alvo = (float)focoCol * passo - G_AREA_W * 0.5f + G_CARD_W * 0.5f;
    if (alvo < 0.0f) alvo = 0.0f;
#ifdef NV_TOUCH_UI
    if (!toquerol_livre(toqueXde(l)))
#endif
      *rolXde(l) = anim_mola(*rolXde(l), alvo, dt, 14.0f); }
}

// --- desenho ---------------------------------------------------------------------
static void fmtHora(time_t t, char *dst, size_t n) {
  struct tm lt; localtime_r(&t, &lt);
  hora_tela(dst, n, &lt);
}

// O cartao de canal: logo + nome em cima, "agora" com barra de progresso e o
// proximo embaixo. Sem grade real o canal se mostra como "AO VIVO" — a
// verdade, em vez de um programa inventado.
// O CARTAO DE CANAL TEM DUAS CORES, E SO DUAS — decisao do dono (16/09),
// olhando a captura do HBO Max: "tira a borda quando nao ta selecionado" e
// "muda o fundo da logo do canal, deixa so branco e preto, e o selecionado tb
// vai ser um ou outro".
//
//   sem foco:  cartao QUASE PRETO, logo e texto BRANCOS
//   com foco:  cartao BRANCO,      logo e texto QUASE PRETOS
//
// O QUE SAIU, e por que. Havia um AZULEJO de 92x92 por tras do logo, pintado
// numa cor derivada do proprio logo (media da borda ou luminancia). Ele era a
// "borda" da reclamacao: em volta da marca preta do HBO sobrava um quadrado
// claro que nao pertencia a nada. A cor derivada tambem fazia cada cartao ter
// um fundo diferente — o oposto de "so branco e preto". Nao ha mais azulejo:
// o logo e desenhado direto sobre o cartao, nos dois estados.
//
// A ARMADILHA DO DESENHO DO LOGO. GFX_TEXTO preserva o RGB da textura;
// GFX_MARCA tira a forma do ALFA e pinta com a cor passada (ver gfx.h). So o
// MARCA da um logo de uma cor so — mas se o arquivo nao tiver recorte (fundo
// opaco proprio), o alfa e cheio e o MARCA desenharia um BLOCO chapado no
// lugar da marca. tex_cor_fundo ja sabe distinguir os dois casos: devolve 1
// quando a borda e opaca. Logo recortado vai por MARCA e fica de uma cor so;
// logo com fundo proprio continua por TEXTO, com o fundo que ja vem no
// arquivo — tinta-lo apagaria a marca, que e pior que um quadrado preto num
// cartao preto (que, no HBO, e justamente o resultado certo).
#define G_LOGO_CLARO 0.965f
#define G_LOGO_ESC   0.08f

// Iniciais do canal para o azulejo de espera: a primeira letra das duas
// primeiras palavras ("Telecine Action" -> "TA", "GNT" -> "GN"), inteiras em
// UTF-8. Palavra que comeca com digito entra tambem ("Canal 24h" -> "C2").
static void iniciaisDe(const char *nome, char *dst, size_t tam) {
  size_t n = 0;
  int palavras = 0, noInicio = 1;
  const unsigned char *p = (const unsigned char *)(nome ? nome : "");
  dst[0] = 0;
  for (; *p && palavras < 2; p++) {
    if (*p == ' ' || *p == '-' || *p == '.' || *p == '|') { noInicio = 1; continue; }
    if (noInicio) {
      int k = (*p < 0x80) ? 1 : (*p >= 0xF0) ? 4 : (*p >= 0xE0) ? 3 : 2, j;
      if (n + (size_t)k >= tam) break;
      if (*p < 0x80) dst[n++] = (char)((*p >= 'a' && *p <= 'z') ? *p - 32 : *p);
      else for (j = 0; j < k && p[j]; j++) dst[n++] = (char)p[j];
      p += k - 1;
      palavras++;
      noInicio = 0;
    }
  }
  // Nome de uma palavra so ganha a segunda letra, para o azulejo nao ficar
  // com um caractere solto ("GNT" -> "GN").
  if (palavras == 1 && nome) {
    const unsigned char *q = (const unsigned char *)nome;
    while (*q == ' ') q++;
    if (q[0] && q[0] < 0x80 && q[1] && q[1] < 0x80 && q[1] != ' ' && n + 1 < tam)
      dst[n++] = (char)((q[1] >= 'a' && q[1] <= 'z') ? q[1] - 32 : q[1]);
  }
  dst[n] = 0;
}

// Desenha o logo do canal CENTRADO na caixa `cx`, sem azulejo, cabendo em
// maxW x maxH. `tom` e a cor do logo de UM TOM SO (claro sobre superficie
// escura, escuro sobre superficie clara). Logo de canal e quase sempre largo
// (3:1, 4:1); a caixa quadrada de desenharLogo o deixava minusculo na grade.
//
// TRES CAMINHOS, e o terceiro e o conserto de 25/09/2026:
//   - borda opaca (fundo proprio): GFX_ARTE, o arquivo como veio, cantos
//     arredondados;
//   - borda transparente e UM TOM SO (silhueta): GFX_MARCA, a forma do alfa
//     pintada em `tom` — a regra do dono de 16/09, logo branco ou preto;
//   - borda transparente e VARIOS TONS: GFX_TEXTO, o arquivo como veio. E o
//     logo em AZULEJO (quadrado arredondado claro, a marca escura dentro, uma
//     margem transparente em volta), o formato da maioria dos pacotes de logo
//     de IPTV. Pela forma do alfa ele virava um QUADRADO CREME chapado — os
//     "logos brancos" da foto do dono na C9. O log dizia ok=1 para esses
//     downloads: a imagem chegava, quem a apagava era o desenho.
//
// SEM TEXTURA (baixando, ou o servidor de logo falhou: o log da C9 mostra
// "corpo curto" do 24horas.cc e do imgur, e "sem corpo" do watchplay) o lugar
// e um azulejo ESCURO discreto com as iniciais do canal. Nunca um bloco claro.
// PUBLICO (guia.h) desde 29/09/2026: o OSD do player ao vivo, o banner do
// zapping e o cartao de erro desenham a marca do canal por AQUI, para o canal
// ter a mesma cara no guia e no player (sem azulejo, branco quando recortado).
void guia_logo_desenhar(const char *logo, const char *nome, GfxRect cx,
                        float maxW, float maxH, float tom, float a) {
  GLuint t = 0;
  float ap = 0.0f, w, h, fr, fg, fb;
  if (logo && logo[0]) {
    t = tex_obter_larg(logo, cx.w);
    ap = tex_aspecto(logo);
  }
  if (!t || ap <= 0.0f) {
    char ini[16];
    float lado = maxH < maxW ? maxH : maxW;
    GfxRect az = { cx.x + (cx.w - lado) * 0.5f, cx.y + (cx.h - lado) * 0.5f, lado, lado };
    int claro = tom < 0.5f;   // superficie clara (cartao em foco): azulejo claro
    TxtLinha l;
    iniciaisDe(nome ? nome : "", ini, sizeof ini);
    if (claro) gfx_cor(az, 0.22f, 0.0f, 0.0f, 0.0f, 0.10f * a);
    else       gfx_cor(az, 0.22f, 1.0f, 1.0f, 1.0f, 0.075f * a);
    if (ini[0]) {
      l = claro ? txt_linha_corta(lado >= 80.0f ? TXT_HEADLINE : TXT_PG_ROTULO, ini, 70, 72, 80, 255, lado - 8.0f)
                : txt_linha_corta(lado >= 80.0f ? TXT_HEADLINE : TXT_PG_ROTULO, ini, 150, 153, 162, 255, lado - 8.0f);
      txt_desenhar_alpha(l, az.x + (az.w - (float)l.w) * 0.5f, az.y + (az.h - (float)l.h) * 0.5f, a);
    }
    return;
  }
  w = maxW; h = w / ap;
  if (h > maxH) { h = maxH; w = h * ap; }
  { GfxRect lr = { cx.x + (cx.w - w) * 0.5f, cx.y + (cx.h - h) * 0.5f, w, h };
    gfx_tex_aspect_atual = 0.0f;
    // O LOGO COM FUNDO PROPRIO E UM QUADRADO, e quadrado dentro de cartao
    // arredondado aparece — foi o que o dono viu no HBO Max. GFX_ARTE e o
    // GFX_TEXTO com a mascara dos cantos: mesmo RGB, mesmo alpha, so recortado.
    // O raio e o do cartao (NV_RAIO_CARD e fracao da ALTURA do retangulo, nao
    // pixel), entao o canto do logo acompanha o canto do cartao.
    if (tex_cor_fundo(logo, &fr, &fg, &fb) == 1)
      gfx_rect(lr, t, GFX_ARTE, 0, 0, 0, NV_RAIO_CARD, 1, 1, 1, a);
    else if (tex_logo_tom_unico(logo) == 1)
      gfx_rect(lr, t, GFX_MARCA, 0, 0, 0, 0.0f, tom, tom, tom, a);
    else
      gfx_rect(lr, t, GFX_TEXTO, 0, 0, 0, 0.0f, 1, 1, 1, a);
  }
}
// Pending images keep their priority; only a confirmed failure falls through.
static const char *logoDe(const GCanal *c) {
  const char *s;
  int i, k;
  if (!c) return NULL;
  if (c->logo && c->logo[0] && !logoFalhou(c->logo)) return c->logo;
  i = (int)(c - canais);
  k = c->logoId ? logoAchar(logoM3u, nLogoM3u, c->logoId, i) : -1;
  if (k < 0 && c->logoNome) k = logoAchar(logoM3u, nLogoM3u, c->logoNome, i);
  if (k >= 0) return canais[k].logo;
  s = c->epg >= 0 ? epg_canal_icone(c->epg) : "";
  if (!s[0]) s = epg_icone_id(chaveDo(c), c->epgId);
  if (s[0] && !logoFalhou(s)) return s;
  k = c->logoNome ? logoAchar(logoVar, nLogoVar, c->logoNome, i) : -1;
  return k >= 0 ? canais[k].logo : NULL;
}
static void logoNaCaixa(const GCanal *c, GfxRect cx, float maxW, float maxH,
                        float tom, float a) {
  guia_logo_desenhar(logoDe(c), c ? c->nome : "", cx, maxW, maxH, tom, a);
}

static void desenharLogo(const GCanal *c, GfxRect cx, float lado, float tom,
                         float a) {
  logoNaCaixa(c, cx, lado, lado, tom, a);
}

static void desenharCard(GCanal *c, float x, float y, float foco, float a,
                         time_t agoraT) {
  GfxRect r = { x, y, G_CARD_W, G_CARD_H };
  float lum = anim_mistura(0.075f, 0.16f, foco);
  EpgProg ag, px;
  int temAgora = gAgora(c, agoraT, &ag);
  int temProx  = gProximo(c, agoraT, 0, &px);
  int epg = gEstadoGrade(c);

  // FOCO = O CARTAO PREENCHIDO NA COR DO FUNDO DO LOGO, sem anel. Pedido do
  // dono (16/09), olhando a captura do guia: "quando ta selecionado ficar com
  // a cor do fundo da logo do canal" — e, na rodada seguinte, "nao ta da
  // mesma cor o fundo do card com o fundo da logo". Entao e A MESMA COR, e
  // nao uma parecida: logo com fundo proprio (o quadrado cinza do Disney+)
  // da o cartao naquele cinza; logo recortado nao tem fundo, e o cartao fica
  // na cor do AZULEJO em que o logo sempre e desenhado — o azulejo some no
  // cartao. Enquanto o logo nao carregou, a cor de realce, como todo botao
  // do app desde a mesma decisao (ver NV_COR_FOCO em layout.h).
  //
  // O texto acompanha: escuro sobre fundo claro, claro sobre fundo escuro,
  // decidido pela luminancia e trocado no meio da mola (texto ja
  // rasterizado nao muda de cor).
  // O foco troca o cartao de PRETO para BRANCO, e mais nada — sem anel, sem
  // cor derivada do logo. `escuro` diz que o texto e o logo tem de virar
  // escuros, e a troca e em DEGRAU no meio da mola: texto ja rasterizado nao
  // muda de cor, e uma cor por quadro rasterizaria a linha a cada quadro (a
  // nota longa esta em ctxmenu.c).
  int escuro = foco > 0.5f;
  gfx_cor(r, 0.08f, lum, lum, lum + 0.01f, a);
  if (foco > 0.01f) gfx_cor(r, 0.08f, G_LOGO_CLARO, G_LOGO_CLARO,
                            G_LOGO_CLARO + 0.004f, foco * a);

  // Logo direto sobre o cartao, SEM AZULEJO em nenhum dos dois estados: era o
  // azulejo que desenhava a "borda" em volta da marca.
  { GfxRect cx = { x + 14.0f, y + 14.0f, 92.0f, 92.0f };
    desenharLogo(c, cx, 80.0f, escuro ? G_LOGO_ESC : G_LOGO_CLARO, a); }

  // Nome ao lado do logo, favorito marcado com a estrela que a fonte ja tem.
  { float tx = x + 120.0f, tw = r.w - 120.0f - 14.0f;
    if (escuro) txt_bloco(TXT_PAINEL_ITEM, c->nome, 20, 21, 25, tx, y + 16.0f, tw, 30.0f, a, 2);
    else        txt_bloco(TXT_PAINEL_ITEM, c->nome, 240, 241, 245, tx, y + 16.0f, tw, 30.0f, a, 2);
    if (c->fav) {
      TxtLinha s = txt_linha(TXT_CAPTION, "\xe2\x98\x85", 255, 214, 90, 255);
      txt_desenhar_alpha(s, x + r.w - s.w - 14.0f, y + 14.0f, a);
    } }

  // AGORA.
  { float ly = y + 118.0f;
    if (temAgora) {
      char h1[12], h2[12], faixa[40];
      fmtHora(ag.ini, h1, sizeof h1); fmtHora(ag.fim, h2, sizeof h2);
      snprintf(faixa, sizeof faixa, "%s %s\xe2\x80\x93%s", i18n("AGORA"), h1, h2);
      { TxtLinha l = escuro ? txt_linha_corta(TXT_MINI, faixa, 30, 50, 90, 255, r.w - 28.0f)
                            : txt_linha_corta(TXT_MINI, faixa, 148, 200, 255, 255, r.w - 28.0f);
        txt_desenhar_alpha(l, x + 14.0f, ly, a); ly += l.h + 2.0f; }
      { int ct = escuro ? 20 : 236;
        TxtLinha l = txt_linha_corta(TXT_CAPTION, ag.titulo, ct, ct + 1, ct + 6, 255,
                                   r.w - 28.0f);
        txt_desenhar_alpha(l, x + 14.0f, ly, a); }
      // Barra do programa: quanto ja passou dentro da janela dele.
      { float f = (ag.fim > ag.ini)
                  ? anim_clamp((float)(agoraT - ag.ini) / (float)(ag.fim - ag.ini), 0.0f, 1.0f)
                  : 0.0f;
        GfxRect tr = { x + 14.0f, y + G_CARD_H - 52.0f, r.w - 28.0f, 4.0f };
        GfxRect an = { tr.x, tr.y, tr.w * f, tr.h };
        if (escuro) { gfx_cor(tr, 0.5f, 0, 0, 0, 0.18f * a);
                      gfx_cor(an, 0.5f, 0.06f, 0.08f, 0.14f, a); }
        else        { gfx_cor(tr, 0.5f, 1, 1, 1, 0.18f * a);
                      gfx_cor(an, 0.5f, 0.36f, 0.64f, 1.0f, a); } }
    } else {
      TxtLinha l = escuro ? txt_linha(TXT_MINI, i18n("AO VIVO"), 150, 30, 30, 255)
                          : txt_linha(TXT_MINI, i18n("AO VIVO"), 255, 120, 120, 255);
      txt_desenhar_alpha(l, x + 14.0f, ly, a);
      ly += l.h + 2.0f;
      { int ct = escuro ? 20 : 236;
        TxtLinha t = txt_linha_corta(TXT_CAPTION, c->nome, ct, ct + 1, ct + 6, 255,
                                   r.w - 28.0f);
        txt_desenhar_alpha(t, x + 14.0f, ly, a); }
      { GfxRect tr = { x + 14.0f, y + G_CARD_H - 52.0f, r.w - 28.0f, 4.0f };
        if (escuro) gfx_cor(tr, 0.5f, 0, 0, 0, 0.14f * a);
        else        gfx_cor(tr, 0.5f, 1, 1, 1, 0.10f * a); }
    } }

  // A SEGUIR, numa linha so.
  { char linha[300];
    if (temProx) {
      char h1[12]; fmtHora(px.ini, h1, sizeof h1);
      snprintf(linha, sizeof linha, "%s %s  \xc2\xb7  %s",
               i18n("A seguir"), h1, px.titulo);
    } else {
      // -2 = canal sem grade real (os "24h"): nao e "carregando", e "nao tem".
      snprintf(linha, sizeof linha, "%s",
               epg == -3 ? i18n("Não foi possível carregar a programação")
               : epg == -2 ? i18n("Sem grade de programação")
               : epg >= 0 ? i18n("Sem próximos programas")
                          : i18n("Carregando programação…"));
    }
    { int ct = escuro ? 60 : 160;
      TxtLinha l = txt_linha_corta(TXT_MINI, linha, ct, ct + 2, ct + 10, 255,
                                 r.w - 28.0f);
      txt_desenhar_alpha(l, x + 14.0f, y + G_CARD_H - 36.0f, a); } }
}

// --- heroi "agora" ---------------------------------------------------------------
// Selo AO VIVO: pilula vermelha, texto branco. Devolve a largura, para a linha
// de meta continuar ao lado dele.
// PUBLICO (guia.h) desde 29/09/2026, pelo mesmo motivo de guia_logo_desenhar:
// o selo do OSD ao vivo e ESTE selo, e nao um parecido.
float guia_selo_ao_vivo(float x, float y, float a) {
  TxtLinha t = txt_linha(TXT_PG_ROTULO, i18n("AO VIVO"), 255, 255, 255, 255);
  GfxRect r = { x, y, (float)t.w + 26.0f, 32.0f };
  gfx_cor(r, 0.5f, 0.84f, 0.15f, 0.19f, a);
  txt_desenhar_alpha(t, r.x + 13.0f, r.y + (r.h - (float)t.h) * 0.5f, a);
  return r.w;
}

// Etiqueta discreta (categoria): vidro translucido, texto secundario. A
// categoria era texto AZUL solto embaixo do nome, e competia com o titulo.
float guia_etiqueta(const char *s, float x, float y, float maxW, float a) {
  TxtLinha t;
  GfxRect r;
  if (!s || !s[0]) return 0.0f;
  t = txt_linha_corta(TXT_DET_META2, s, 196, 198, 206, 255, maxW - 24.0f);
  r = (GfxRect){ x, y, (float)t.w + 24.0f, 34.0f };
  gfx_cor(r, 0.5f, 1.0f, 1.0f, 1.0f, 0.085f * a);
  txt_desenhar_alpha(t, r.x + 12.0f, r.y + (r.h - (float)t.h) * 0.5f, a);
  return r.w;
}

// O MOLDE DA DESCRICAO do addon de canal, medido na C9 (25/09/2026), em duas
// formas:
//   "Categoria: HBO Qualidades: 4K, FHD, HD, SD 8 fonte(s)"
//   "Categoria: Canais 24 Horas 1 fonte(s)"
// A categoria ja e etiqueta no heroi e sai; as qualidades viram selos; "8
// fonte(s)" vira "8 fontes". O que vier antes de "Categoria:" ou depois de
// "fonte(s)" fica em `resto`. Sem nenhum dos tres marcadores nao e o molde:
// devolve 0 e o texto vai cru, como qualquer descricao.
typedef struct { char q[6][8]; int nq; int fontes; char resto[600]; } GDesc;
static void aparar(char *s) {
  size_t n = strlen(s), i = 0;
  while (n && (s[n - 1] == ' ' || s[n - 1] == '\n')) s[--n] = 0;
  while (s[i] == ' ' || s[i] == '\n') i++;
  if (i) memmove(s, s + i, n - i + 1);
}
// "N fonte(s)": devolve o inicio do numero e poe em *depois o fim do rotulo.
static const char *acharFontes(const char *d, int *n, const char **depois) {
  const char *f = d;
  while ((f = strstr(f, " fonte")) != NULL) {
    const char *i = f;
    while (i > d && i[-1] >= '0' && i[-1] <= '9') i--;
    if (i < f && (i == d || i[-1] == ' ')) {
      const char *p = f + 6;
      if (!strncmp(p, "(s)", 3)) p += 3; else if (*p == 's') p++;
      *n = atoi(i); *depois = p;
      return i;
    }
    f += 6;
  }
  return NULL;
}
static int descMolde(const char *d, GDesc *o) {
  const char *qu = strstr(d, "Qualidades:"), *ca = strstr(d, "Categoria:");
  const char *fo, *fimFo = NULL, *ini, *p, *fim = NULL;
  int nf = 0;
  size_t pre;
  memset(o, 0, sizeof *o);
  fo = acharFontes(d, &nf, &fimFo);
  if (!qu && !ca && !fo) return 0;
  ini = d + strlen(d);
  if (ca && ca < ini) ini = ca;
  if (qu && qu < ini) ini = qu;
  if (fo && fo < ini) ini = fo;
  pre = (size_t)(ini - d);
  if (pre >= sizeof o->resto) pre = sizeof o->resto - 1;
  memcpy(o->resto, d, pre); o->resto[pre] = 0;
  if (qu) {
    p = qu + 11;
    for (;;) {
      char tok[16]; int n = 0;
      while (*p == ' ' || *p == ',') p++;
      if (fo && p >= fo) break;
      while (*p && *p != ' ' && *p != ',' && n < 15) tok[n++] = *p++;
      tok[n] = 0;
      if (!n) break;
      if (n < 8 && o->nq < 6) snprintf(o->q[o->nq++], sizeof o->q[0], "%s", tok);
    }
  }
  if (fo) { o->fontes = nf; fim = fimFo; }
  aparar(o->resto);
  if (fim && *fim) {
    char suf[600];
    size_t k = strlen(o->resto);
    snprintf(suf, sizeof suf, "%s", fim);
    aparar(suf);
    if (suf[0]) snprintf(o->resto + k, sizeof o->resto - k, "%s%s", k ? " " : "", suf);
  }
  return 1;
}

const char *guia_desc_livre(const char *desc, char *buf, size_t n) {
  GDesc d;
  if (!desc || !desc[0] || !buf || !n) return "";
  if (!descMolde(desc, &d)) return desc;
  snprintf(buf, n, "%s", d.resto);
  return buf;
}

// O HEROI: a ficha do canal focado a esquerda e o preview 16:9 a direita.
//
// `tFoco` e o instante que o foco aponta: agora, ou o comeco da janela quando
// a pessoa adiantou a grade com DIREITA — o heroi mostra o programa que a
// celula em foco mostra, e nao sempre o do ar.
//
// SEM GRADE (a maioria dos canais: ~60% do FrostView nao casa com o XMLTV) o
// titulo grande e o proprio canal e a linha de meta diz "Sem grade de
// programacao". Um estado limpo, em vez da pilha de barras vazias de antes.
static void desenharHero(float a, time_t agoraT, time_t tFoco) {
  GCanal *c = linhaItem(focoLin, focoCol);
  GfxRect pv = { G_PREVIEW_X, G_PREVIEW_Y, G_PREVIEW_W, G_PREVIEW_H };
  float raio = G_PREVIEW_RAIO / G_PREVIEW_H;
  float ha = a * heroA;
  float x = G_INFO_X, w = G_INFO_W, y = G_HERO_Y;
  float proxY = gRetrato() ? G_HERO_Y + G_INFO_H - 34.0f : G_PREVIEW_H - 34.0f;
  float ar, ag, ab;
  int epg, tem = 0, noAr = 0;
  EpgProg p;
  if (!c) return;
  ajustes_acento(&ar, &ag, &ab);
  tem = gAgora(c, tFoco, &p);
  epg = gEstadoGrade(c);
  noAr = tem && p.ini <= agoraT && agoraT < p.fim;

  // The native video plane sits behind GL. TV art reaches the screen edges;
  // portrait retains its existing geometry. So em tela cheia: no overlay o player ja esta atras, e
  // o pipeline de video e um so.
  //
  // O furo so abre quando o pipeline tem um quadro (video_pronto); antes
  // disso o plano esta vazio e furar cedo mostrava um retangulo PRETO. Ate la
  // o lugar do video e uma superficie com o logo: o heroi nunca muda de forma.
  // Enquanto a janela do player ANDA (encolhendo da tela cheia), o furo e o
  // do degrau, desenhado no fim de guia_desenhar — aqui fica so a superficie.
  //
  // Retained playback keeps running while browsing, but only the focused
  // channel may show through GL. Other channels use their own logo.
  { GCanal *pc = previewId[0] ? canalPorId(previewId) : NULL;
    int sessao = aberta && !overlay && player_mini_no_guia_ativo();
    int anda = player_janela_animando(NULL, NULL, NULL, NULL);
    // Compare IDs, not pointers: the same channel id can occupy two GCanal
    // entries (M3U + addon, favorites dup), and canalPorId returns the FIRST
    // while linhaItem may focus the second — a pointer compare never opens
    // the hole for it.
    // O video do heroi e o da SESSAO do preview, nao o do foco: navegar pelo
    // guia com um canal tocando mantem o video (e o audio) visivel; ele so
    // sai quando o preview troca de canal ou o stream para. Compara por ID,
    // nunca por ponteiro de GCanal.
    int focado = sessao || previewFocoEh(c->id);
    if (!focado) pc = c;
    if (!pc) pc = c;
  if (sessao && focado && !anda && video_pronto() && !player_carregando()) {
    gfx_furo_raio(pv, raio);
    // Selo sobre o video: GL opaco por cima do furo aparece por cima do plano.
    { float badgeX = pv.x + (gRetrato() ? 20.0f : 240.0f);
      float badgeY = pv.y + pv.h - (gRetrato() ? 52.0f : 180.0f);
      guia_selo_ao_vivo(badgeX, badgeY, a); }
  } else if (anda && focado) {
    // encolhendo: nada aqui, o furo do degrau vem por cima de tudo
  } else {
    if (gRetrato()) gfx_vidro_ou_solido(pv, raio, GFX_VIDRO_DENS, a, 0.078f, 0.080f, 0.088f, 1.0f);
    else gfx_cor(pv, 0, 0.078f, 0.080f, 0.088f, a);
    // Uma luz so, bem fraca, na cor de realce: tira a cara de caixa vazia.
    if (!ajustes_vidro()) gfx_luz_canto(pv, raio, pv.w * 0.82f, -pv.h * 0.2f, pv.w * 0.85f,
                  ar, ag, ab, 0.10f * a);
    { float lw = gRetrato() ? fminf(320.0f, pv.w - 40.0f) : 320.0f;
      float lh = gRetrato() ? fminf(170.0f, fmaxf(20.0f, pv.h - 120.0f)) : 170.0f;
      GfxRect cx = { pv.x + (pv.w - lw) * 0.5f, pv.y + (pv.h - lh) * 0.5f - (pv.h < 200.0f ? 24.0f : 12.0f), lw, lh };
      logoNaCaixa(pc, cx, fminf(300.0f, lw), fminf(150.0f, lh), G_LOGO_CLARO, pc == c ? ha : a); }
    // "Carregando" so enquanto ha algo a caminho: sem sessao nem pedido,
    // dizer "carregando" seria mentir para sempre.
    if ((sessao && player_carregando()) || (aberta && !overlay && pedPreview)) {
      TxtLinha t = gRetrato()
        ? txt_linha_corta(TXT_DET_META2, i18n("carregando canal…"), 160, 163, 172, 255, pv.w - 40.0f)
        : txt_linha(TXT_DET_META2, i18n("carregando canal…"), 160, 163, 172, 255);
      txt_desenhar_alpha(t, pv.x + (pv.w - (float)t.w) * 0.5f, pv.y + pv.h - 60.0f, a);
    }
  } }
  if (!gRetrato()) {
    gfx_rect((GfxRect){pv.x, pv.y, 240.0f, pv.h}, 0, GFX_VEU_CSS, 0, 2, 1, 0,
             NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, a);
    gfx_rect((GfxRect){pv.x, pv.y + pv.h - 200.0f, pv.w, 200.0f}, 0, GFX_VEU_CSS, 0, 0, 1, 0,
             NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, a);
    gfx_rect((GfxRect){pv.x, pv.y, pv.w, 130.0f}, 0, GFX_VEU_CSS, 0, 1, 1, 0,
             NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 0.85f * a);
  }

  // Channel identity belongs with LIVE, below the header summary.
  { char num[16];
    float tx = x;
    logoNaCaixa(c, (GfxRect){tx, y - 4.0f, 48.0f, 40.0f}, 48, 40, G_LOGO_CLARO, ha);
    tx += 60.0f;
    snprintf(num, sizeof num, "%d", c->numero);
    TxtLinha n = txt_linha(TXT_DET_META2, num, 150, 153, 162, 255);
    txt_desenhar_alpha(n, tx, y + (32.0f - n.h) * 0.5f, ha);
    tx += n.w + 16.0f;
    tx += guia_selo_ao_vivo(tx, y, ha) + 16.0f;
    guia_etiqueta(c->cat >= 0 ? i18n(cats[c->cat].nome) : "", tx, y,
                  fmaxf(0, x + w - tx), ha);
  }
  y += 56.0f;
  if (tem) {
    TxtLinha n = txt_linha_corta(TXT_CW_TITULO, c->nome, 196, 198, 206, 255, w);
    txt_desenhar_alpha(n, x, y, ha);
    y += n.h + 12.0f;
  }

  // TITULO: o programa, ou o canal quando nao ha programa. 56 px em ate duas
  // linhas — o degrau acima do nome do canal (28) e de 2x, e o de baixo, ate
  // a linha de meta (25), de 2,2x: a ficha le em tres niveis de relance.
  y += txt_bloco(TXT_TITULO2, tem ? p.titulo : c->nome, 246, 247, 250,
                 x, y - 4.0f, w, 66.0f, ha, 1) + 10.0f;

  // META: selo + faixa de horario + quanto falta, e a barra de progresso.
  { char meta[160], h1[12], h2[12];
    float mx = x;
    meta[0] = 0;
    if (tem) {
      fmtHora(p.ini, h1, sizeof h1); fmtHora(p.fim, h2, sizeof h2);
      if (noAr) {
        int falta = (int)((p.fim - agoraT + 59) / 60);
        char resto[64];
        snprintf(resto, sizeof resto, i18n("%d min restantes"), falta);
        snprintf(meta, sizeof meta, "%s \xe2\x80\x93 %s  \xc2\xb7  %s", h1, h2, resto);
      } else {
        int em = (int)((p.ini - agoraT + 59) / 60);
        char quando[64];
        snprintf(quando, sizeof quando, i18n("Começa em %d min"), em > 0 ? em : 0);
        snprintf(meta, sizeof meta, "%s \xe2\x80\x93 %s  \xc2\xb7  %s", h1, h2, quando);
      }
    } else if (epg == -1) {
      snprintf(meta, sizeof meta, "%s", i18n("Carregando programação…"));
    } else if (epg == -2) {
      snprintf(meta, sizeof meta, "%s", i18n("Sem grade de programação"));
    }
    if (meta[0]) {
      TxtLinha t = txt_linha_corta(TXT_DET_META, meta, 196, 198, 206, 255, x + w - mx);
      txt_desenhar_alpha(t, mx, y + (32.0f - (float)t.h) * 0.5f, ha);
    }
    y += 32.0f + 18.0f;
    // PROGRAMA DO FUTURO: a linha diz o que o OK faz aqui — lembrar, nao
    // assistir — e se o lembrete ja esta marcado.
    if (tem && !noAr && p.ini > agoraT) {
      int marcado = lembrete_achar(c->id, p.ini, p.titulo) >= 0;
      TxtLinha t = txt_linha(TXT_DET_META, marcado ? i18n("Lembrete marcado  ·  OK cancela")
                                                   : i18n("OK avisa quando começar"),
                             marcado ? 236 : 196, marcado ? 237 : 198, marcado ? 242 : 206, 255);
      gfx_icone((GfxRect){ x, y + 2.0f, 26.0f, 26.0f }, "sino",
                marcado ? ar : 0.75f, marcado ? ag : 0.76f, marcado ? ab : 0.80f, ha);
      txt_desenhar_alpha(t, x + 38.0f, y + (30.0f - (float)t.h) * 0.5f, ha);
      y += 30.0f + 18.0f;
    }
    if (noAr) {
      float f = (p.fim > p.ini)
                ? anim_clamp((float)(agoraT - p.ini) / (float)(p.fim - p.ini), 0.0f, 1.0f)
                : 0.0f;
      GfxRect tr = { x, y, 440.0f, 4.0f };
      GfxRect an = { tr.x, tr.y, tr.w * f, tr.h };
      if (ajustes_vidro()) gfx_vidro_chip(tr, 0.5f, 0.55f, a);
      else gfx_cor(tr, 0.5f, 1, 1, 1, 0.14f * a);
      gfx_cor(an, 0.5f, ar, ag, ab, a);
      y += 4.0f + 22.0f;
    } }

  // DESCRICAO do canal (o XMLTV deste guia nao traz sinopse de programa). O
  // molde do addon ("Categoria: X Qualidades: 4K, FHD 8 fonte(s)") vira
  // selos de qualidade e a contagem de fontes; o que sobrar, ou texto fora
  // do molde, vai como veio — no maximo tres linhas, e so as que cabem antes
  // da linha "A seguir".
  // "Canal: <nome do canal>" (outro addon, visto na C9 em 25/09) nao diz nada
  // que a linha do canal ja nao diga.
  { char eco[200];
    snprintf(eco, sizeof eco, "Canal: %s", c->nome);
  if (c->desc[0] && strcmp(c->desc, eco)) {
    GDesc d;
    int molde = descMolde(c->desc, &d);
    const char *txt = molde ? d.resto : c->desc;
    if (molde && (d.nq > 0 || d.fontes > 0)) {
      float bx = x;
      int k;
      // QUALIDADES COMO MARCA (dono, 29/09/2026): eram caixas de texto com
      // contorno ("4K", "FHD", "HD", "SD"); agora sao as marcas de resolucao
      // do app (badges.h), as mesmas do OSD ao vivo e da folha de fontes.
      // Nome que nao e resolucao conhecida vai no selo neutro da tabela unica.
      for (k = 0; k < d.nq; k++) {
        int fm = marca_resolucao(d.q[k]);
        if (fm >= 0)
          bx += marca_formato((FormatoMarca)fm, bx, y + 2.0f, 26.0f, 0.86f, 0.87f, 0.90f, ha) + 18.0f;
        else
          bx += badge_desenhar(bx, y + 1.0f, d.q[k], BADGE_NEUTRO, ha) + BADGE_GAP;
      }
      if (d.fontes > 0) {
        char f[48];
        TxtLinha t;
        if (d.fontes == 1) snprintf(f, sizeof f, "%s", i18n("1 fonte"));
        else snprintf(f, sizeof f, i18n("%d fontes"), d.fontes);
        t = txt_linha(TXT_DET_META2, f, 150, 153, 162, 255);
        txt_desenhar_alpha(t, bx + (d.nq ? 8.0f : 0.0f), y + (30.0f - (float)t.h) * 0.5f, ha);
      }
      y += 30.0f + 20.0f;
    }
    if (txt[0]) {
      float descFim = gRetrato() ? proxY : G_PREVIEW_H - 104.0f;
      int linhas = (int)((descFim - 22.0f - y) / 34.0f);
      if (linhas > 3) linhas = 3;
      if (linhas > 0)
        txt_bloco(TXT_DET_SIN, txt, 172, 175, 184, x, y, w, 34.0f, ha * 0.95f, linhas);
    }
  } }

  // A SEGUIR, ancorado na base do heroi (alinhado a base do preview).
  { EpgProg q;
    if (gProximo(c, tFoco, 0, &q)) {
      char hh[12];
      TxtLinha l = txt_linha(TXT_PG_ROTULO, i18n("A SEGUIR"), 140, 143, 152, 255);
      TxtLinha hr, tt;
      float lx = x;
      fmtHora(q.ini, hh, sizeof hh);
      gfx_cor((GfxRect){ x, proxY - 18.0f, w, 1.0f }, 0.0f, 1, 1, 1, 0.08f * a);
      txt_desenhar_alpha(l, lx, proxY + (30.0f - (float)l.h) * 0.5f, ha);
      lx += (float)l.w + 20.0f;
      hr = txt_linha(TXT_BODY, hh, 180, 183, 192, 255);
      txt_desenhar_alpha(hr, lx, proxY + (30.0f - (float)hr.h) * 0.5f, ha);
      lx += (float)hr.w + 14.0f;
      tt = txt_linha_corta(TXT_BODY, q.titulo, 230, 231, 236, 255, x + w - lx);
      txt_desenhar_alpha(tt, lx, proxY + (30.0f - (float)tt.h) * 0.5f, ha);
    } else {
      // SEM "A SEGUIR", a base do heroi diz o que o OK faz: a acao principal
      // escrita onde o olho ja esta, em vez de so na barra de ajuda.
      TxtLinha k = txt_linha(TXT_PG_ROTULO, i18n("OK"), 24, 25, 30, 255);
      TxtLinha l = txt_linha(TXT_BODY, i18n("Assistir"), 222, 224, 230, 255);
      GfxRect kc = { x, proxY, (float)k.w + 24.0f, 30.0f };
      gfx_cor(kc, 0.25f, 0.88f, 0.885f, 0.90f, ha);
      txt_desenhar_alpha(k, kc.x + 12.0f, kc.y + (kc.h - (float)k.h) * 0.5f, ha);
      txt_desenhar_alpha(l, kc.x + kc.w + 14.0f, proxY + (30.0f - (float)l.h) * 0.5f, ha);
    } }
}

// O PAINEL DE CATEGORIAS: uma gaveta opaca de 440 px na esquerda, com um veu
// escurecendo o resto. Antes era uma lista TRANSPARENTE desenhada por cima do
// heroi e dos cartoes (foto do dono, 25/09/2026: "PULANDO CATEGORIAS" lido
// atraves do titulo do programa) — ilegivel.
//
// Cada linha: nome da secao a esquerda (cortado com "…", ha secoes como
// "Canais Series 24h") e a contagem alinhada a direita. A secao ATUAL tem a
// barra na cor de realce; a linha em FOCO e preenchida na cor de realce com
// texto escuro, a mesma linguagem das pilulas do menu lateral. A lista rola
// mantendo o foco no meio. Entra deslizando da esquerda com a mola (ease-out).
//
// SEGUNDA FOTO DO DONO (25/09, a tarde), tres defeitos e o que mudou:
//   - linha CORTADA NO MEIO DA LETRA em cima ("Canais Esportes" sob o titulo)
//     e embaixo ("Canais Series 24h"): o recorte era a borda dura da lista.
//     Agora cada linha apaga pelo proprio TOPO (em cima) e pela propria BASE
//     (embaixo) ao chegar na borda, em G_CAT_FADE px, e chega a alfa 0 antes
//     de tocar o recorte — nenhuma linha visivel e cortada. So apaga do lado
//     em que ha mais lista: no inicio e no fim a lista enche a area inteira
//     (catRol = 0 / maxY) e a primeira e a ultima linha ficam cheias.
//   - um ARCO LARANJA no canto de cima a direita: era esta gaveta, nao a
//     imersiva por tras — a luz de canto na cor de realce, a 10%, com o centro
//     no canto e alcance de 0,8 da largura. O limite do circulo caia DENTRO do
//     painel e, sem dither, o primeiro degrau de 1/255 (18->19) desenhava o
//     contorno. Agora a luz vem de cima-esquerda, larga (alcance maior que o
//     painel: o limite fica fora dele) e a 6%; com o dither do gfx.c a rampa
//     sai lisa.
//   - o canto era 28/p.w, na unidade errada: o SDF mede em fracao da ALTURA,
//     entao saia um canto de 63 px, dentro da tela, e o fundo aparecia pela
//     quina. Agora os cantos ficam fora da tela (borda reta) e o fio de 1,2 px
//     a 6% do cartao de novidades marca a borda.
#define G_CAT_FADE 60.0f
// As gavetas do guia usam a mesma folha do Glass UI. O miolo de vidro aplica
// Opacidade e Frost; o ramo solido permanece opaco quando a interface de vidro
// esta desligada.
static void materialPainelGuia(GfxRect p, float raioPx, float a) {
  float raio = raioPx / (p.h > 1.0f ? p.h : 1.0f);
  gfx_rect((GfxRect){ p.x - 18.0f, p.y - 8.0f, p.w + 36.0f, p.h + 40.0f },
           0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, 0, 0, 0, 0.38f * a);
  if (ajustes_vidro()) gfx_vidro_folha(p, raio, a);
  else gfx_cor(p, raio, 0.071f, 0.075f, 0.086f, 0.98f * a);
}

static void ponteiroCategoria(int i, int b);
static void desenharPainelCategorias(float a) {
  float e = catAnim, ar, ag, ab;
  float x0 = -G_CAT_W * 0.18f * (1.0f - e);   // desliza ~80 px enquanto aparece
  float ea = e * a;
  float areaH = G_CAT_BASE - G_CAT_TOPO, maxY, acima, abaixo;
  int nl = nLinhas(), i, tf = 243;
  if (e < 0.01f || nl < 1) return;
  if (catAberto) ponteiro_camada();
#ifdef NV_TOUCH_UI
  if (catAberto) {
    toquerol_vincular(&toqueCat, (GfxRect){ 0, G_CAT_TOPO, G_CAT_W, areaH }, gfx_escala(), 0.0f, fmaxf(0.0f, nl * G_CAT_ROW - areaH), 1, &catRol);
    ponteiro_rolagem(toqueCategoriaRolar);
  }
#endif
  ajustes_acento(&ar, &ag, &ab);
  maxY = (float)nl * G_CAT_ROW - areaH;
  if (maxY < 0.0f) maxY = 0.0f;
  // Quanto de lista ha fora da area, em cima e embaixo (0..1 de uma linha):
  // e o que liga o esmaecimento de cada borda.
  acima  = catRol / G_CAT_ROW;          acima  = acima  < 0 ? 0 : (acima  > 1 ? 1 : acima);
  abaixo = (maxY - catRol) / G_CAT_ROW; abaixo = abaixo < 0 ? 0 : (abaixo > 1 ? 1 : abaixo);
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, (ajustes_vidro() ? 0.30f : 0.42f) * ea);
  // O painel passa 40 px das bordas de cima e de baixo: os cantos ficam fora
  // da tela e o que se ve e uma borda reta de alto a baixo. Com o canto
  // arredondado DENTRO da tela, o fundo aparecia pela quina.
  { GfxRect p = { x0 - 40.0f, -40.0f, G_CAT_W + 40.0f, NV_TELA_H + 80.0f };
    materialPainelGuia(p, 28.0f, ea); }
  // Titulo, linhas e rodape na MESMA coluna de texto (x0 + 56): antes o titulo
  // e o rodape ficavam a 48 e o texto das linhas a 56, e a borda esquerda do
  // bloco nao fechava.
  { TxtLinha t = txt_linha(TXT_HEADLINE, i18n("Categorias"), 242, 243, 247, 255);
    txt_desenhar_alpha(t, x0 + 56.0f, 60.0f, ea); }

  // O recorte fica G_CAT_FADE/2 alem da area: e so rede de seguranca, porque
  // a linha que chegaria nele ja esta em alfa 0.
  gfx_recorte(0.0f, G_CAT_TOPO - G_CAT_FADE * 0.5f, G_CAT_W,
              areaH + G_CAT_FADE);
  for (i = 0; i < nl; i++) {
    float y = G_CAT_TOPO + (float)i * G_CAT_ROW - catRol;
    int foc = i == catFoco, atual = i == focoLin;
    GfxRect r = { x0 + 32.0f, y, G_CAT_W - 56.0f, G_CAT_ROW - 8.0f };
    float ft, fb, la;
    char n[16];
    TxtLinha tn, tc;
    // Topo da linha contra a borda de cima; base da linha contra a de baixo.
    ft = anim_suave((r.y - (G_CAT_TOPO - G_CAT_FADE * 0.5f)) / G_CAT_FADE);
    fb = anim_suave(((G_CAT_BASE + G_CAT_FADE * 0.5f) - (r.y + r.h)) / G_CAT_FADE);
    la = (1.0f - (1.0f - ft) * acima) * (1.0f - (1.0f - fb) * abaixo) * ea;
    if (la < 0.01f) continue;
    if (catAberto && e > 0.99f)
      ponteiro_alvo_faixa(r.x, r.y, r.w, r.h, G_CAT_TOPO, G_CAT_BASE, ponteiroCategoria, NULL, i, 0);
    snprintf(n, sizeof n, "%d", linhaN(i));
    if (foc) plrui_linha_foco(r, 20.0f, la);
    else if (atual) gfx_cor(r, 12.0f / r.h, 1, 1, 1, 0.06f * la);
    // A barra da secao atual: 4x22 com raio de 2 px. Com raio 0,5 (da altura)
    // num retangulo de 4 px o SDF saia um grao oval colado na borda da linha.
    if (atual && !foc)
      gfx_cor((GfxRect){ r.x + 10.0f, r.y + (r.h - 22.0f) * 0.5f, 4.0f, 22.0f }, 2.0f / 22.0f,
              ar, ag, ab, la);
    tc = foc ? txt_linha(TXT_DET_META2, n, tf, tf, tf, 255)
             : txt_linha(TXT_DET_META2, n, 140, 143, 152, 255);
    txt_desenhar_alpha(tc, r.x + r.w - 16.0f - (float)tc.w, r.y + (r.h - (float)tc.h) * 0.5f, la);
    { float nw = r.w - 24.0f - 16.0f - (float)tc.w - 16.0f;
      int cn = atual ? 245 : 205;
      tn = foc ? txt_linha_corta(TXT_BODY, linhaNome(i), tf, tf, tf, 255, nw)
               : txt_linha_corta(TXT_BODY, linhaNome(i), cn, cn + 1, cn + 5 > 255 ? 255 : cn + 5, 255, nw);
      txt_desenhar_alpha(tn, r.x + 24.0f, r.y + (r.h - (float)tn.h) * 0.5f, la); }
  }
  gfx_sem_recorte();
  { TxtLinha t = txt_linha_corta(TXT_CAPTION, i18n("OK entra  ·  Voltar fecha"),
                                 128, 130, 138, 255, G_CAT_W - 96.0f);
    txt_desenhar_alpha(t, x0 + 56.0f, NV_TELA_H - 60.0f, ea); }
}

// DE ONDE VEM CANAL: as duas portas, desenhadas em codigo.
//
// EM CODIGO E NAO NUM PNG, e a razao e uma so: rotulo dentro de imagem nao
// passa por i18n. A primeira versao disto era um diagrama inteiro rasterizado,
// e para nao mentir em ingles ele tinha de ser MUDO — dois pictogramas e uma
// seta, sem dizer o que era nenhum dos dois. Desenhado aqui, cada cartao tem
// titulo e explicacao na lingua da pessoa, ganha a cor de realce do tema e
// continua nitido em qualquer resolucao de interface.
//
// Os icones seguem sendo arquivo (addon.png, portal.png, menu_guide.png): a
// forma mora no alpha e a cor vem daqui, que e o contrato de gfx_icone.
static void desenharDuasPortas(float x, float y, float a) {
  const float CW = 470.0f, CH = 152.0f, GAP = 24.0f;
  const float AR = 104.0f;                /* faixa da seta entre as colunas */
  float ac_r, ac_g, ac_b;
  float x2 = x + CW + AR;
  int i;
  ajustes_acento(&ac_r, &ac_g, &ac_b);

  for (i = 0; i < 2; i++) {
    float cy = y + (float)i * (CH + GAP);
    GfxRect c = { x, cy, CW, CH };
    const char *ic  = i ? "portal" : "addon";
    const char *tit = i ? i18n("Portal IPTV") : i18n("Addon de canais");
    const char *sub = i ? i18n("Cadastrado em Ajustes › Conta")
                        : i18n("Instalado na sua conta");
    /* DUAS COR FIXAS NAS ORIGENS, e a de realce so no destino.
       O acento e BRANCO por padrao (ver NV_COR_FOCO em layout.h), entao pintar
       tudo com ele nao daria cor nenhuma na configuracao que a maioria usa. Um
       azul e um ambar separam as duas portas de relance, sobrevivem em todos os
       temas (nenhum deles é azul ou ambar) e tem contraste de sobra sobre
       #141414. */
    float ir = i ? 1.00f : 0.51f, ig = i ? 0.78f : 0.71f, ib = i ? 0.44f : 1.00f;
    gfx_cor(c, 0.12f, 1.0f, 1.0f, 1.0f, 0.05f * a);
    /* Disco atras do icone na mesma cor, bem apagado: e o que faz o icone
       parecer pousado no cartao em vez de solto sobre ele. */
    { GfxRect disco = { c.x + 30.0f, c.y + (CH - 72.0f) * 0.5f, 72.0f, 72.0f };
      gfx_cor(disco, 0.5f, ir, ig, ib, 0.14f * a); }
    { GfxRect gi = { c.x + 46.0f, c.y + (CH - 40.0f) * 0.5f, 40.0f, 40.0f };
      gfx_icone(gi, ic, ir, ig, ib, a); }
    /* TXT_HEADLINE e nao TXT_TITULO2: "Channel addon" estourava os 340 px
       uteis no titulo maior e saia "Channel…". As duas linhas ficam a 56 px
       uma da outra — TITULO2 a 52 encostava a descida do "g" na linha de
       baixo. */
    { TxtLinha t = txt_linha_corta(TXT_HEADLINE, tit, 240, 242, 248, 255,
                                   CW - 136.0f);
      txt_desenhar_alpha(t, c.x + 122.0f, c.y + 38.0f, a); }
    { TxtLinha t = txt_linha_corta(TXT_CAPTION, sub, 150, 153, 162, 255,
                                   CW - 136.0f);
      txt_desenhar_alpha(t, c.x + 122.0f, c.y + 94.0f, a * 0.95f); }

    /* A SETA E UM ARQUIVO INTEIRO (fluxo.png), haste e ponta juntas. Montar a
       ponta com retangulos em diagonal leu como um "x" — gfx_cor nao gira, e
       diagonal ali vira escada; e emendar haste de gfx_cor com ponta de icone
       deixava degrau na junta. */
    { float sy = cy + CH * 0.5f;
      GfxRect f = { x + CW + 16.0f, sy - 12.0f, AR - 32.0f, 24.0f };
      gfx_icone(f, "fluxo", 1.0f, 1.0f, 1.0f, 0.42f * a); }
  }

  /* O DESTINO, alto o bastante para abracar as duas origens: e a figura que
     diz "as duas enchem a MESMA tela", que e a informacao toda. */
  { float dh = CH * 2.0f + GAP;
    GfxRect d = { x2, y, CW - 90.0f, dh };
    gfx_cor(d, 0.09f, 1.0f, 1.0f, 1.0f, 0.08f * a);
    { GfxRect disco = { d.x + (d.w - 104.0f) * 0.5f, y + dh * 0.5f - 112.0f,
                        104.0f, 104.0f };
      gfx_cor(disco, 0.5f, ac_r, ac_g, ac_b, 0.12f * a); }
    { GfxRect gi = { d.x + (d.w - 56.0f) * 0.5f, y + dh * 0.5f - 88.0f,
                     56.0f, 56.0f };
      gfx_icone(gi, "menu_guide", ac_r, ac_g, ac_b, a); }
    { TxtLinha t = txt_linha(TXT_HEADLINE, i18n("Guia de TV"), 240, 242, 248, 255);
      txt_desenhar_alpha(t, d.x + (d.w - t.w) * 0.5f, y + dh * 0.5f + 6.0f, a); }
    { TxtLinha t = txt_linha_corta(TXT_CAPTION,
          i18n("As duas enchem a mesma grade"), 150, 153, 162, 255, d.w - 36.0f);
      txt_desenhar_alpha(t, d.x + (d.w - t.w) * 0.5f, y + dh * 0.5f + 56.0f,
                         a * 0.95f); } }
}


// O "atualizado ha" curto da fonte.
static void haQuanto(long long quando, char *d, size_t n) {
  long s = (long)((long long)time(NULL) - quando);
  if (quando <= 0 || s < 0) { d[0] = 0; return; }
  if (s < 120) snprintf(d, n, "%s", i18n("agora"));
  else if (s < 7200) snprintf(d, n, i18n("há %d min"), (int)(s / 60));
  else if (s < 2 * 86400) snprintf(d, n, i18n("há %d h"), (int)(s / 3600));
  else snprintf(d, n, i18n("há %d dias"), (int)(s / 86400));
}

static void desenharFontes(float a) {
  float x0 = G_INFO_X, xm = G_INFO_X + G_INFO_W, y = gRetrato() ? G_HERO_Y + G_INFO_H - 112.0f : G_PREVIEW_H - 82.0f, h = 40.0f, x, alvo, xf = 0, wf = 0;
  float xs[2 + G_MAX_LTV], ws[2 + G_MAX_LTV];
  char rot[2 + G_MAX_LTV][96];
  int i, n = fontesN(), sel = chipDoFiltro(), tf = ajustes_tinta_foco();
  if (!barraVisivel()) return;
  for (i = 0; i < n; i++) {
    if (i == 0) snprintf(rot[i], sizeof rot[i], "\xe2\x98\x85 %s", i18n("Favoritos"));
    else if (i == 1) snprintf(rot[i], sizeof rot[i], "%s", i18n("Todas"));
    else snprintf(rot[i], sizeof rot[i], "%s  %d", ltvF[i - 2].nome, ltvF[i - 2].nCanais);
    ws[i] = (float)txt_linha(TXT_PG_ROTULO, rot[i], 255, 255, 255, 255).w + 2.0f * G_CHIP_PAD + (i >= 2 ? 18.0f : 0.0f);
  }
  for (i = 0, x = 0.0f; i < n; i++) { xs[i] = x; x += ws[i] + 10.0f; }
  // A barra rola para o chip em foco (ou o escolhido) ficar a vista.
  { int k = focoFontes ? fontesCol : sel;
    alvo = fontesRol;
    if (xs[k] - fontesRol < 0.0f) alvo = xs[k];
    if (xs[k] + ws[k] - fontesRol > xm - x0) alvo = xs[k] + ws[k] - (xm - x0);
    fontesRol = alvo; }
  gfx_recorte(x0 - 4.0f, y - 6.0f, xm - x0 + 8.0f, h + 12.0f);
  for (i = 0; i < n; i++) {
    GfxRect r = { x0 + xs[i] - fontesRol, y, ws[i], h };
    int foc = focoFontes && fontesCol == i, ct;
    float tx;
    if (r.x + r.w < x0 - 4.0f || r.x > xm + 4.0f) continue;
    if (foc) { plrui_pilula_foco(r, a); xf = r.x; wf = r.w; }
    else if (i == sel) { if (ajustes_vidro()) gfx_cor(r, 0.5f, 1, 1, 1, 0.14f * a); else gfx_cor(r, 0.5f, 0.204f, 0.212f, 0.243f, a); }
    else plrui_botao_repouso(r, a);
    ct = foc ? tf : i == sel ? 245 : 168;
    tx = r.x + G_CHIP_PAD;
    if (i >= 2) {
      // Ponto de saude: verde, ambar, vermelho.
      int sd = ltvF[i - 2].saude;
      GfxRect d = { tx, r.y + (h - 10.0f) * 0.5f, 10.0f, 10.0f };
      if (sd == LTV_SAUDE_FORA) gfx_cor(d, 0.5f, 0.92f, 0.33f, 0.31f, a);
      else if (sd == LTV_SAUDE_DEGRADADA) gfx_cor(d, 0.5f, 0.98f, 0.73f, 0.26f, a);
      else gfx_cor(d, 0.5f, 0.30f, 0.84f, 0.46f, a);
      tx += 18.0f;
    }
    { TxtLinha t = txt_linha(TXT_PG_ROTULO, rot[i], ct, ct < 255 ? ct + 1 : 255, ct + 5 > 255 ? 255 : ct + 5, 255);
      txt_desenhar_alpha(t, tx, r.y + (h - (float)t.h) * 0.5f, a); }
  }
  gfx_sem_recorte();
  // A LINHA DA FONTE em foco: o que ela tem, de quando e o aviso de corte.
  if (focoFontes && fontesCol >= 2 && !fopAberto) {
    const GFonteLtv *F = &ltvF[fontesCol - 2];
    char linha[320], quando[64], grade[64] = "";
    haQuanto(F->atualizada, quando, sizeof quando);
    if (F->epgResolvidos && F->nCanais)
      snprintf(grade, sizeof grade, i18n("grade em %d%%"), F->epgCom * 100 / F->nCanais);
    if (F->saude == LTV_SAUDE_FORA)
      snprintf(linha, sizeof linha, "%s", i18n("Fora do ar agora · sem lista guardada · segure OK para recarregar"));
    else {
      size_t u;
      if (F->cortada) snprintf(linha, sizeof linha, i18n("Mostrando %d de %d canais (o que cabe nesta TV)"),
                               F->nCanais, F->total);
      else snprintf(linha, sizeof linha, i18n("%d canais · %d grupos"), F->nCanais, F->nGrupos);
      u = strlen(linha);
      if (grade[0]) u += (size_t)snprintf(linha + u, sizeof linha - u, " \xc2\xb7 %s", grade);
      snprintf(linha + u, sizeof linha - u, " \xc2\xb7 %s", F->deCache ? i18n("lista guardada") : quando);
    }
    { TxtLinha t = txt_linha_corta(TXT_CAPTION, linha, 196, 198, 206, 255, G_INFO_W);
      GfxRect p = { x0, y - 44.0f, (float)t.w + 28.0f, 36.0f };
      gfx_cor(p, 0.5f, 0.08f, 0.085f, 0.10f, 0.94f * a);
      txt_desenhar_alpha(t, p.x + 14.0f, p.y + (p.h - (float)t.h) * 0.5f, a); }
  }
  if (fopAberto && fopModo != FOP_FONTE && fopCanal) {
    static char it[2 + G_SUG][160];
    int nIt = fopItens(), k, tf2 = ajustes_tinta_foco();
    GfxRect c = { x0, G_HERO_Y + G_FONTES_H + 20.0f, 760.0f, 60.0f + (float)nIt * 58.0f };
    const char *tit = fopModo == FOP_CANAL ? fopCanal->nome : i18n("Escolher grade deste canal");
    if (fopModo == FOP_CANAL) {
      snprintf(it[0], sizeof it[0], "%s%s", fopCanal->fav ? "\xe2\x98\x85 " : "", i18n("Favorito"));
      snprintf(it[1], sizeof it[1], "%s", i18n("Escolher grade deste canal"));
    } else {
      int e = epgDo(fopCanal);
      snprintf(it[0], sizeof it[0], "%s%s%s", i18n("Automática"), e >= 0 ? " \xc2\xb7 " : "",
               e >= 0 ? epg_canal_nome(e) : "");
      snprintf(it[1], sizeof it[1], "%s", i18n("Sem grade de programação"));
      for (k = 0; k < sugN; k++)
        snprintf(it[2 + k], sizeof it[0], "%s%s%s", epg_canal_nome(sug[k]),
                 epg_canal_fonte(sug[k])[0] ? "  \xc2\xb7  " : "", epg_canal_fonte(sug[k]));
    }
    materialPainelGuia(c, 22.0f, a);
    gfx_cor(c, 22.0f / c.h, 0.071f, 0.075f, 0.086f, 0.92f * a);   // o heroi nao vaza pelo vidro
    { TxtLinha t = txt_linha_corta(TXT_PG_ROTULO, tit, 150, 153, 162, 255, c.w - 40.0f);
      txt_desenhar_alpha(t, c.x + 24.0f, c.y + 18.0f, a); }
    for (k = 0; k < nIt; k++) {
      GfxRect r = { c.x + 12.0f, c.y + 52.0f + (float)k * 58.0f, c.w - 24.0f, 50.0f };
      int foc = k == fopFoco, cc = foc ? tf2 : 225;
      TxtLinha t;
      if (foc) plrui_linha_foco(r, 18.0f, a);
      t = txt_linha_corta(TXT_BODY, it[k], cc, cc, cc, 255, r.w - 28.0f);
      txt_desenhar_alpha(t, r.x + 14.0f, r.y + (r.h - (float)t.h) * 0.5f, a);
    }
  }
  // AS OPCOES DA FONTE (segurar OK): um cartao sob o chip.
  if (fopAberto && fopModo == FOP_FONTE && fontesCol >= 2) {
    const GFonteLtv *F = &ltvF[fontesCol - 2];
    LtvPref *p = ltv_pref(F->chave, 0);
    char it[FOP_N][120];
    GfxRect c = { xf > 0 ? xf : x0, y + h + 10.0f, 560.0f, 52.0f + FOP_N * 58.0f };
    int k;
    snprintf(it[FOP_AZ], sizeof it[0], "%s: %s", i18n("Ordem dos grupos"),
             p && p->az ? "A\xe2\x80\x93Z" : i18n("do provedor"));
    { size_t u = (size_t)snprintf(it[FOP_FUSO], sizeof it[0], i18n("Horário da grade: %+.1f h"),
                                  (p ? p->fusoMin : 0) / 60.0);
      snprintf(it[FOP_FUSO] + u, sizeof it[0] - u, "  \xe2\x97\x82\xe2\x96\xb8"); }
    snprintf(it[FOP_GRUPOS], sizeof it[0], "%s", i18n("Escolher grupos"));
    snprintf(it[FOP_RECARREGAR], sizeof it[0], "%s", i18n("Recarregar"));
    snprintf(it[FOP_LIMPAR], sizeof it[0], "%s", i18n("Limpar lista de canais"));
    if (c.x + c.w > xm) c.x = xm - c.w;
    (void)wf;
    materialPainelGuia(c, 22.0f, a);
    gfx_cor(c, 22.0f / c.h, 0.071f, 0.075f, 0.086f, 0.92f * a);   // o heroi nao vaza pelo vidro
    { TxtLinha t = txt_linha_corta(TXT_PG_ROTULO, F->nome, 150, 153, 162, 255, c.w - 40.0f);
      txt_desenhar_alpha(t, c.x + 24.0f, c.y + 16.0f, a); }
    for (k = 0; k < FOP_N; k++) {
      GfxRect r = { c.x + 12.0f, c.y + 48.0f + (float)k * 58.0f, c.w - 24.0f, 50.0f };
      int foc = k == fopFoco, cc = foc ? tf : 225;
      TxtLinha t;
      if (foc) plrui_linha_foco(r, 18.0f, a);
      t = txt_linha_corta(TXT_BODY, it[k], cc, cc, cc, 255, r.w - 28.0f);
      txt_desenhar_alpha(t, r.x + 14.0f, r.y + (r.h - (float)t.h) * 0.5f, a);
    }
  }

}

static void ponteiroLimpar(int i, int b) { (void)b; limparFoco = i; }
static void desenharLimpar(float a) {
  // CONFIRMACAO de "Limpar lista de canais": um cartao central, porque a
  // acao apaga cache E a lista da tela — sem ela, um toque errado no chip
  // esvaziava o guia e a pessoa nao sabia como voltar atras.
  if (limparConfirm) {
    ponteiro_camada();
    GfxRect c = { (NV_TELA_W - 640.0f) * 0.5f, (NV_TELA_H - 300.0f) * 0.5f, 640.0f, 300.0f };
    TxtLinha t;
    int tf2 = 240; // The focus surface stays dark for every accent.
    gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.55f * a);
    materialPainelGuia(c, 26.0f, a);
    gfx_cor(c, 26.0f / c.h, 0.071f, 0.075f, 0.086f, 0.94f * a);
    t = txt_linha(TXT_HEADLINE, i18n("Limpar a lista de canais?"), 240, 242, 248, 255);
    txt_desenhar_alpha(t, c.x + 36.0f, c.y + 32.0f, a);
    txt_bloco(TXT_BODY,
              i18n("Apaga a lista guardada deste perfil e recarrega dos addons e contas configurados."),
              200, 202, 210, c.x + 36.0f, c.y + 100.0f, c.w - 72.0f, 30.0f, a, 3);
    { GfxRect r0 = { c.x + 36.0f, c.y + 216.0f, (c.w - 84.0f) * 0.5f, 56.0f };
      GfxRect r1 = { r0.x + r0.w + 12.0f, r0.y, r0.w, r0.h };
      int f0 = limparFoco == 0;
      TxtLinha t0, t1;
      ponteiro_alvo(r0.x, r0.y, r0.w, r0.h, ponteiroLimpar, NULL, 0, 0);
      ponteiro_alvo(r1.x, r1.y, r1.w, r1.h, ponteiroLimpar, NULL, 1, 0);
      if (f0) plrui_linha_foco(r0, 18.0f, a); else plrui_linha_foco(r1, 18.0f, a);
      t0 = txt_linha(TXT_BODY, i18n("Limpar"), f0 ? tf2 : 225, f0 ? tf2 : 225, f0 ? tf2 : 225, 255);
      t1 = txt_linha(TXT_BODY, i18n("Cancelar"), !f0 ? tf2 : 225, !f0 ? tf2 : 225, !f0 ? tf2 : 225, 255);
      txt_desenhar_alpha(t0, r0.x + (r0.w - t0.w) * 0.5f, r0.y + (r0.h - t0.h) * 0.5f, a);
      txt_desenhar_alpha(t1, r1.x + (r1.w - t1.w) * 0.5f, r1.y + (r1.h - t1.h) * 0.5f, a);
    }
  }
}

// O SELETOR DE GRUPOS (M2): folha central de vidro, uma linha por grupo da
// fonte com caixa de marcar e a contagem; o contador "N de M canais" no alto
// fica vermelho se a escolha nao couber (a marcacao que estouraria e recusada).
static void desenharGrupos(float a) {
  const GFonteLtv *F;
  float W = 980.0f, H = 840.0f, rowH = 52.0f, listaY, listaH, ar, ag, ab;
  GfxRect c;
  int i, n, vis, au, tf = ajustes_tinta_foco(), cota, conta;
  char tmp[200];
  if (!gpAberto || gpFonte >= nLtvF) return;
  F = &ltvF[gpFonte];
  n = 2 + gpN;
  c = (GfxRect){ (NV_TELA_W - W) * 0.5f, (NV_TELA_H - H) * 0.5f, W, H };
  ajustes_acento(&ar, &ag, &ab);
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0, 0, 0, 0, 0.62f * a);
  materialPainelGuia(c, 26.0f, a);
  gfx_cor(c, 26.0f / c.h, 0.071f, 0.075f, 0.086f, 0.94f * a);
  conta = gpConta(&au); cota = gpCota();
  // Cabecalho: titulo, fonte e o contador.
  { TxtLinha t = txt_linha(TXT_CALLOUT, i18n("Escolher grupos"), 240, 241, 246, 255);
    txt_desenhar_alpha(t, c.x + 36.0f, c.y + 26.0f, a); }
  { TxtLinha t = txt_linha_corta(TXT_CAPTION, F->nome, 150, 153, 162, 255, W * 0.4f);
    txt_desenhar_alpha(t, c.x + 36.0f, c.y + 70.0f, a); }
  { int estoura = conta > cota;
    snprintf(tmp, sizeof tmp, i18n("%d de %d"), conta, cota);
    { TxtLinha t = txt_linha(TXT_BODY, tmp, estoura ? 240 : 236, estoura ? 110 : 237, estoura ? 100 : 242, 255);
      GfxRect pl = { c.x + c.w - 36.0f - (float)t.w - 32.0f, c.y + 30.0f, (float)t.w + 32.0f, 44.0f };
      gfx_cor(pl, 0.5f, 1, 1, 1, 0.08f * a);
      txt_desenhar_alpha(t, pl.x + 16.0f, pl.y + (pl.h - (float)t.h) * 0.5f, a); }
    if (au) { TxtLinha t = txt_linha(TXT_CAPTION, i18n("Automático: a ordem do provedor, até onde cabe"), 150, 153, 162, 255);
      txt_desenhar_alpha(t, c.x + c.w - 36.0f - (float)t.w, c.y + 82.0f, a); } }
  // Linhas: 0 Aplicar, 1 Automatico, depois os grupos.
  listaY = c.y + 124.0f; listaH = c.h - 124.0f - 64.0f;
  vis = (int)(listaH / rowH);
  { float alvo = gpRol;
    if ((float)gpFoco < alvo) alvo = (float)gpFoco;
    if ((float)(gpFoco + 1) > alvo + (float)vis) alvo = (float)(gpFoco + 1 - vis);
    if (alvo < 0.0f) alvo = 0.0f;
    gpRol += (alvo - gpRol) * 0.35f;
    if (alvo - gpRol < 0.01f && gpRol - alvo < 0.01f) gpRol = alvo; }
  gfx_recorte(c.x, listaY - 4.0f, c.w, listaH + 8.0f);
  for (i = 0; i < n; i++) {
    GfxRect r = { c.x + 16.0f, listaY + ((float)i - gpRol) * rowH, c.w - 32.0f, rowH - 4.0f };
    int foc = i == gpFoco, cc = foc ? tf : 225;
    TxtLinha t;
    float tx = r.x + 18.0f;
    if (r.y + r.h < listaY - 4.0f || r.y > listaY + listaH + 4.0f) continue;
    if (foc) plrui_linha_foco(r, 18.0f, a);
    if (i == 0) {
      t = txt_linha_corta(TXT_BODY, i18n("Aplicar e recarregar"), cc, cc, cc, 255, r.w - 40.0f);
      txt_desenhar_alpha(t, tx, r.y + (r.h - (float)t.h) * 0.5f, a);
    } else if (i == 1) {
      t = txt_linha_corta(TXT_BODY, i18n("Automático (ordem do provedor)"), cc, cc, cc, 255, r.w - 40.0f);
      txt_desenhar_alpha(t, tx, r.y + (r.h - (float)t.h) * 0.5f, a);
    } else {
      int g = i - 2, m = gpMarca[g];
      GfxRect b = { tx, r.y + (r.h - 26.0f) * 0.5f, 26.0f, 26.0f };
      if (m) gfx_cor(b, 0.28f, foc ? 0.10f : ar, foc ? 0.10f : ag, foc ? 0.10f : ab, a);
      else {
        gfx_cor(b, 0.28f, foc ? 0.0f : 1.0f, foc ? 0.0f : 1.0f, foc ? 0.0f : 1.0f, (foc ? 0.45f : 0.30f) * a);
        gfx_cor((GfxRect){ b.x + 2.0f, b.y + 2.0f, b.w - 4.0f, b.h - 4.0f }, 0.26f, 0.071f, 0.075f, 0.086f, 0.96f * a);
      }
      if (m) gfx_cor((GfxRect){ b.x + 7.0f, b.y + 7.0f, 12.0f, 12.0f }, 0.2f, foc ? 1.0f : 0.07f, foc ? 1.0f : 0.07f, foc ? 1.0f : 0.08f, a);
      snprintf(tmp, sizeof tmp, "%d", F->gi[g].n);
      { TxtLinha q = txt_linha(TXT_CAPTION, tmp, foc ? cc : 150, foc ? cc : 153, foc ? cc : 162, 255);
        txt_desenhar_alpha(q, r.x + r.w - 18.0f - (float)q.w, r.y + (r.h - (float)q.h) * 0.5f, a);
        t = txt_linha_corta(TXT_BODY, F->gi[g].nome, cc, cc, cc, 255, r.w - 130.0f - (float)q.w);
        txt_desenhar_alpha(t, tx + 44.0f, r.y + (r.h - (float)t.h) * 0.5f, a); }
    }
  }
  gfx_sem_recorte();
  // Rodape: dica, ou o aviso de que nao cabe.
  { int aviso = gpAviso && SDL_GetTicks() - gpAvisoEm < 2500u;
    TxtLinha t = txt_linha_corta(TXT_CAPTION,
        aviso ? i18n("Esse grupo não cabe nesta TV. Desmarque outro antes.")
              : i18n("OK marca  ·  Voltar descarta"),
        aviso ? 240 : 128, aviso ? 130 : 130, aviso ? 110 : 138, 255, c.w - 72.0f);
    txt_desenhar_alpha(t, c.x + 36.0f, c.y + c.h - 46.0f, a); }
}

// --- cabecalho: [Cartoes | Lista]  [Addons]  [Preview]  relogio -------------
// CHIPS: vidro translucido em repouso, um degrau mais claro quando e o
// selecionado (modo atual, preview ligado), e o FOCO e um anel na cor de
// realce por fora do chip — a linguagem do anel de cartaz (NV_ANEL_FOCO), e
// nao o preenchimento claro das pilulas grandes. Preencher de branco um chip
// de 40 px no alto da tela era o "botao gordo bege" da foto do dono.
//
// Cartoes | Lista e UM controle segmentado: um trilho so, com o segmento
// escolhido destacado dentro dele. Addons e Preview sao chips soltos.
//
// Devolve o x onde os chips comecam, para o subtitulo nao passar dali.
// PONTEIRO (#99) NO GUIA. Pelas mesmas variaveis das setas: o chip da barra de
// cima, o canal da grade (lista ou cartoes), a linha da gaveta de categorias,
// o resultado da busca e a linha do painel de addons. O OK do clique segue o
// caminho de sempre. Gaveta, busca e painel sao camadas (so elas valem
// enquanto abertas), e so com a animacao assentada.
static int buscaEstado;   // declarada de novo com a busca, mais abaixo
static int guiaCamadaAberta(void) { return catAberto || buscaEstado || painel || limparConfirm || opcoesAberto; }
#ifdef NV_TOUCH_UI
static int guiaVazioTelefone(void) {
  return telefoneui_ativo() && aberta && !overlay && !nCanais &&
    (estado == G_FALHOU || (fontesOk && !nFontes && estado != G_BAIXANDO));
}
static void ponteiroConfigGuia(int i, int b) {
  (void)i; (void)b;
  if (!guiaVazioTelefone() || guiaCamadaAberta() || falhas || xtFalha) return;
  focoTopo = 1; topoCol = G_TOPO_OPCOES;
}
static int toqueVazioRolar(const PonteiroRolagem *e) {
  if (!guiaVazioTelefone() || guiaCamadaAberta()) return 0;
  int r = toquerol_evento(&toqueVazio, e);
  if (r && (e->fase == PONT_ROL_INICIO || e->fase == PONT_ROL_MOVER)) {
    okDesde = 0; okLongo = 0; dirSeg = 0; dicaDesde = 0;
  }
  return r;
}

/* The setup explanation belongs below the wrapped header. Its TV diagram
   uses fixed positions that cross the last phone chip and truncate its text. */
static void desenharVazioTelefone(const char *msg, int configurar, float a) {
  float topo = (gRetrato() ? gTopoFim : G_TOPO_Y + G_TOPO_H) + 40.0f;
  float fim = NV_TELA_H - 80.0f, area = fmaxf(0.0f, fim - topo);
  float textoH = txt_bloco_corta(TXT_V2_26, msg, 200, 202, 210,
                                 G_AREA_X, topo, G_AREA_W, 36.0f, 0, 0);
  float cardH[2] = {0}, tituloH[2] = {0}, subH[2] = {0};
  const char *tit[2] = {i18n("Addon de canais"), i18n("Portal IPTV")};
  const char *sub[2] = {i18n("Instalado na sua conta"), i18n("Cadastrado em Ajustes › Conta")};
  float altura = textoH;
  if (configurar) {
    altura += 28.0f;
    for (int i = 0; i < 2; i++) {
      tituloH[i] = txt_bloco_corta(TXT_HEADLINE, tit[i], 240, 242, 248,
                                   G_AREA_X + 24.0f, 0, G_AREA_W - 48.0f, 48.0f, 0, 0);
      subH[i] = txt_bloco_corta(TXT_V2_26, sub[i], 150, 153, 162,
                                G_AREA_X + 24.0f, 0, G_AREA_W - 48.0f, 36.0f, 0, 0);
      cardH[i] = 48.0f + tituloH[i] + 8.0f + subH[i];
      altura += cardH[i] + (i ? 0.0f : 16.0f);
    }
  }
  toquerol_vincular(&toqueVazio, (GfxRect){G_AREA_X, topo, G_AREA_W, area},
                    gfx_escala(), 0, fmaxf(0, altura - area), 1, &rolVazio);
  rolVazio = toquerol_clamp(rolVazio, 0, toqueVazio.maximo);
  if (!guiaCamadaAberta()) ponteiro_rolagem(toqueVazioRolar);
  gfx_recorte(G_AREA_X, topo, G_AREA_W, area);
  float y = topo - rolVazio;
  y += txt_bloco_corta(TXT_V2_26, msg, 200, 202, 210,
                       G_AREA_X, y, G_AREA_W, 36.0f, a, 0);
  if (configurar) {
    y += 28.0f;
    for (int i = 0; i < 2; i++) {
      GfxRect c = {G_AREA_X, y, G_AREA_W, cardH[i]};
      if (!i) plrui_botao_repouso(c, a);
      else gfx_cor(c, .08f, 1, 1, 1, .04f * a);
      txt_bloco_corta(TXT_HEADLINE, tit[i], 240, 242, 248,
                      c.x + 24.0f, c.y + 24.0f, c.w - 48.0f, 48.0f, a, 0);
      txt_bloco_corta(TXT_V2_26, sub[i], 150, 153, 162,
                      c.x + 24.0f, c.y + 32.0f + tituloH[i], c.w - 48.0f, 36.0f, a, 0);
      if (!i && a > .99f && !guiaCamadaAberta())
        ponteiro_alvo_faixa(c.x, c.y, c.w, c.h, topo, fim, ponteiroConfigGuia, NULL, 0, 0);
      y += cardH[i] + 16.0f;
    }
  }
  gfx_sem_recorte();
}
#endif
static void ponteiroTopo(int i, int b) {
  (void)b;
  if (guiaCamadaAberta() || i < 0 || i >= G_TOPO_N) return;
  focoTopo = 1; topoCol = i;
}
static void ponteiroCanal(int l, int i) {
  if (guiaCamadaAberta() || l < 0 || l >= nLinhas() || i < 0 || i >= linhaN(l)) return;
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toqueGradeY); toquerol_limpar(&toqueLista);
  toquerol_limpar(toqueXde(l));
#endif
  focoTopo = 0; focoLin = l; focoCol = i;
}
#ifdef NV_TOUCH_UI
static void ponteiroCard(float x, float y, int l, int i) {
  float esquerda = fmaxf(x, G_AREA_X), direita = fminf(x + G_CARD_W, G_AREA_DIR);
  if (direita <= esquerda) return;
  ponteiro_alvo_faixa(esquerda, y, direita - esquerda, G_CARD_H, G_TOPO - 8.0f,
                     G_L_BASE, ponteiroCanal, NULL, l, i);
}
static void desenharCardVisivel(GCanal *c, float x, float y, int l, int i,
                                float foco, float a, time_t agoraT) {
  if (x + G_CARD_W <= G_AREA_X || x >= G_AREA_DIR) return;
  if (a > 0.99f) ponteiroCard(x, y, l, i);
  desenharCard(c, x, y, foco, a, agoraT);
}
#endif
static void ponteiroCategoria(int i, int b) {
  (void)b;
  if (!catAberto || i < 0 || i >= nLinhas()) return;
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toqueCat);
#endif
  catFoco = i;
}
static void ponteiroAddon(int i, int b) {
  (void)b;
  if (!painel || i < 0 || i >= painelN()) return;
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toqueAddon);
#endif
  paFoco = i;
}

static void gTopoRotulos(const char **rot) {
  rot[G_TOPO_BUSCAR] = i18n("Buscar");
  rot[G_TOPO_CATEGORIAS] = i18n("Categorias");
  rot[G_TOPO_CARTOES] = i18n("Cartões");
  rot[G_TOPO_LISTA]   = i18n("Lista");
  rot[G_TOPO_OPCOES]  = i18n("Opções");
  rot[G_TOPO_ADDONS]  = i18n("Addons");
  rot[G_TOPO_FILTROS] = i18n("Filtros da Live TV");
  // DIAGNOSTICO na barra (dono, 01/10: o controle do Android nao tem a tecla
  // VERDE). O verde continua como atalho onde existe.
  rot[G_TOPO_DIAG]    = i18n("Diagnóstico");
  rot[G_TOPO_LIMPAR]  = i18n("Limpar lista de canais");
  rot[G_TOPO_PREVIEW] = previewLigado ? i18n("Preview: sim") : i18n("Preview: não");
}

static void gTopoLarguras(const char **rot, float *w) {
  for (int i = 0; i < G_TOPO_N; i++)
    w[i] = (float)txt_largura(G_CHIP_TEXTO, rot[i])
           + 2.0f * G_CHIP_PAD
           + (i == G_TOPO_PREVIEW ? 20.0f : (i == G_TOPO_CATEGORIAS || i == G_TOPO_BUSCAR) ? 26.0f : 0.0f);
}

// Four control groups stay below the title and clear of channel art.
static void gTopoAjustar(float *w, float *xs, float right) {
  float x = G_AREA_X;
  right = fminf(right, G_INFO_X + G_INFO_W);
  float available = fmaxf(0.0f, right - x - 12.0f * (G_TOPO_PRIMARIOS - 2));
  float total = 0.0f;
  for (int i = 0; i < G_TOPO_PRIMARIOS; i++) total += w[i];
  if (total > available) {
    // Water-fill: short labels keep their width; only long labels ellipsize.
    float lo = 0.0f, hi = total;
    for (int step = 0; step < 20; step++) {
      float cap = (lo + hi) * 0.5f, used = 0.0f;
      for (int i = 0; i < G_TOPO_PRIMARIOS; i++) used += fminf(w[i], cap);
      if (used > available) hi = cap; else lo = cap;
    }
    for (int i = 0; i < G_TOPO_PRIMARIOS; i++) w[i] = fminf(w[i], lo);
  }
  for (int i = 0; i < G_TOPO_PRIMARIOS; i++) {
    xs[i] = x;
    x += w[i] + (i == G_TOPO_CARTOES ? 0.0f : 12.0f);
  }
}

static void gTopoMedir(void) {
#ifdef NV_TOUCH_UI
  if (gRetrato()) {
    const char *rot[G_TOPO_N];
    float w[G_TOPO_N];
    gTopoRotulos(rot);
    gTopoLarguras(rot, w);
    gTopoDistribuir(w);
  }
#endif
}

static void ponteiroOpcao(int i, int b) {
  (void)b;
  if (opcoesAberto && i >= G_TOPO_ADDONS && i <= G_TOPO_PREVIEW) opcoesFoco = i;
}

static void desenharOpcoes(float a) {
  if (!opcoesAberto) return;
  const char *rot[G_TOPO_N];
  float w[G_TOPO_N], xs[G_TOPO_N];
  gTopoRotulos(rot); gTopoLarguras(rot, w);
  gTopoAjustar(w, xs, G_INFO_X + G_INFO_W);
  float mw = 0;
  for (int i = G_TOPO_ADDONS; i <= G_TOPO_PREVIEW; i++) mw = fmaxf(mw, w[i]);
  mw = fminf(mw + 32.0f, gRetrato() ? G_AREA_W : G_INFO_W);
  float x = fmaxf(G_AREA_X, xs[G_TOPO_OPCOES] + w[G_TOPO_OPCOES] - mw);
  float y = G_CHIPS_Y + G_TOPO_H + 12.0f;
#ifdef NV_TOUCH_UI
  if (gRetrato()) { x = G_AREA_X; y = gTopoFim + 12.0f; }
#endif
  GfxRect panel = {x, y, mw, 5 * 56.0f + 24.0f};
  gfx_vidro_ou_solido(panel, 28.0f / panel.h, GFX_VIDRO_DENS, a, .071f, .075f, .086f, .98f);
  for (int i = G_TOPO_ADDONS; i <= G_TOPO_PREVIEW; i++) {
    GfxRect row = {x + 12, y + 12 + (i - G_TOPO_ADDONS) * 56.0f, mw - 24, 56};
    int foco = i == opcoesFoco;
    if (foco) plrui_linha_foco(row, 20.0f, a);
    TxtLinha t = txt_linha_corta(G_CHIP_TEXTO, rot[i], foco ? 243 : 196,
                                foco ? 242 : 198, foco ? 239 : 206, 255, row.w - 32);
    txt_desenhar_alpha(t, row.x + 16, row.y + (row.h - t.h) * .5f, a);
    if (a > .99f) ponteiro_alvo(row.x, row.y, row.w, row.h, ponteiroOpcao, NULL, i, 0);
  }
}

static float desenharTopo(float a) {
  const char *rot[G_TOPO_N];
  float w[G_TOPO_N], xs[G_TOPO_N], x, ar, ag, ab, seg0, tituloDir;
  int i;
  gTopoRotulos(rot);
  gTopoLarguras(rot, w);
  ajustes_acento(&ar, &ag, &ab);

  // Relogio na margem direita, na altura dos chips.
  { time_t tt = time(NULL); struct tm lt; char hora[12];
    TxtLinha t;
    localtime_r(&tt, &lt); hora_tela(hora, sizeof hora, &lt);
    t = txt_linha(TXT_CALLOUT, hora, 236, 237, 242, 255);
    x = G_AREA_DIR - (float)t.w;
    txt_desenhar_alpha(t, x, G_TOPO_Y + (G_TOPO_H - (float)t.h) * 0.5f, a);
    x -= 36.0f; }
  tituloDir = x;

  gTopoAjustar(w, xs, tituloDir);
#ifdef NV_TOUCH_UI
  if (gRetrato()) for (i = 0; i < G_TOPO_PRIMARIOS; i++) {
    xs[i] = gTopoRects[i].x;
    w[i] = gTopoRects[i].w;
  }
#endif
  seg0 = xs[G_TOPO_CARTOES];
  // Trilho do controle segmentado.
  { GfxRect tr = { seg0, G_CHIPS_Y, w[G_TOPO_CARTOES] + w[G_TOPO_LISTA], G_TOPO_H };
#ifdef NV_TOUCH_UI
    if (gRetrato()) tr.y = gTopoRects[G_TOPO_CARTOES].y;
#endif
    if (ajustes_vidro()) gfx_vidro_folha(tr, 0.5f, a);
    else gfx_cor(tr, 0.5f, 0.114f, 0.118f, 0.137f, a); }

  for (i = 0; i < G_TOPO_PRIMARIOS; i++) {
    float f = animTopo[i];
    int seg = i == G_TOPO_CARTOES || i == G_TOPO_LISTA;
    int sel = i == G_TOPO_PREVIEW ? previewLigado
            : i == G_TOPO_CATEGORIAS ? catAberto != 0
            : seg ? ((i == G_TOPO_LISTA) == modoLista) : 0;
    GfxRect r = { xs[i], G_CHIPS_Y, w[i], G_TOPO_H };
#ifdef NV_TOUCH_UI
    if (gRetrato()) r = gTopoRects[i];
#endif
    int ct;
    if (!overlay && !guiaCamadaAberta() && a > 0.99f) ponteiro_alvo(r.x, r.y, r.w, r.h, ponteiroTopo, NULL, i, 0);
    if (seg) {
      // Segmento: so o escolhido tem superficie, 3 px para dentro do trilho.
      GfxRect ir = { r.x + 3.0f, r.y + 3.0f, r.w - 6.0f, r.h - 6.0f };
      if (f > 0.5f) plrui_pilula_foco(ir, a);
      else if (sel) {
        if (ajustes_vidro()) gfx_cor(ir, 0.5f, 1, 1, 1, 0.14f * a);
        else gfx_cor(ir, 0.5f, 0.204f, 0.212f, 0.243f, a);
      }
    } else {
      // CHIP do Glass UI: branco 8% em repouso, ligado 14%, foco = pilula
      // cheia no acento (sem anel).
      if (f > 0.5f) plrui_pilula_foco(r, a);
      else if (sel) { if (ajustes_vidro()) gfx_cor(r, 0.5f, 1, 1, 1, 0.14f * a);
                      else gfx_cor(r, 0.5f, 0.204f, 0.212f, 0.243f, a); }
      else plrui_botao_repouso(r, a);
    }
    ct = f > 0.5f ? ajustes_tinta_foco() : sel ? 245 : 168;
    { // ct+1 estourava 255 -> 0 no canal verde (tinta branca = magenta no foco).
      float iconeW = i == G_TOPO_PREVIEW ? 20.0f
                   : (i == G_TOPO_CATEGORIAS || i == G_TOPO_BUSCAR) ? 26.0f : 0.0f;
      TxtLinha t = txt_linha_corta(G_CHIP_TEXTO, rot[i], ct, ct < 255 ? ct + 1 : 255,
                          ct + 5 > 255 ? 255 : ct + 5, 255, r.w - 2.0f * G_CHIP_PAD - iconeW);
      float tx = r.x + (r.w - (float)t.w) * 0.5f;
      if (i == G_TOPO_PREVIEW) {
        // Ponto de estado: verde ligado, cinza desligado. O texto ja diz,
        // o ponto e o que se le de relance.
        GfxRect d = { r.x + G_CHIP_PAD, r.y + (r.h - 10.0f) * 0.5f, 10.0f, 10.0f };
        if (f > 0.5f) { float k = (float)ct / 255.0f; gfx_cor(d, 0.5f, k, k, k, a); }
        else if (previewLigado) gfx_cor(d, 0.5f, 0.30f, 0.84f, 0.46f, a);
        else               gfx_cor(d, 0.5f, 0.42f, 0.43f, 0.47f, a);
        tx = d.x + 20.0f;
      } else if (i == G_TOPO_BUSCAR) {
        // A lupa do menu lateral: o chip se le como busca antes do texto.
        float c = (float)ct / 255.0f;
        gfx_icone((GfxRect){ r.x + G_CHIP_PAD - 2.0f, r.y + (r.h - 20.0f) * 0.5f, 20.0f, 20.0f },
                  "menu_search", c, c, c, a);
        tx = r.x + G_CHIP_PAD + 26.0f;
      } else if (i == G_TOPO_CATEGORIAS) {
        // Tres tracos de "lista": o chip se le como menu antes do texto.
        float c = (float)ct / 255.0f, gx = r.x + G_CHIP_PAD, gy = r.y + (r.h - 14.0f) * 0.5f;
        int k;
        for (k = 0; k < 3; k++)
          gfx_cor((GfxRect){ gx, gy + 6.0f * (float)k, 16.0f, 2.0f }, 0.5f, c, c, c, a);
        tx = gx + 26.0f;
      }
      txt_desenhar_alpha(t, tx, r.y + (r.h - (float)t.h) * 0.5f, a); }
  }
  return gRetrato() ? tituloDir : xs[G_TOPO_BUSCAR];
}

// --- modo lista ---------------------------------------------------------------------
// Inicio da janela de tempo: a meia hora cheia anterior a agora, mais o
// deslocamento pedido com DIREITA. Regua em meias horas porque e assim que a
// grade de TV sempre foi lida; o olho ja sabe onde procurar.
static time_t janelaIni(time_t agoraT) {
  return (agoraT / 1800) * 1800 + (time_t)(tempoDesloc() * 60.0f);
}

// O instante que o foco aponta: agora, ou DEZ MINUTOS depois do comeco da
// janela quando ela foi adiantada. A celula em foco e a do programa que cobre
// este instante. Os dez minutos existem porque o comeco exato da janela
// costuma cair no restinho do programa anterior (01:32-02:02 numa janela que
// abre as 02:00), e o anel abracava uma lasca de 20 px.
static time_t instanteFoco(time_t agoraT) {
  if (!modoLista || tempoDesloc() <= 0) return agoraT;
  return janelaIni(agoraT) + 10 * 60;
}

static void desenharReguaEm(float a, time_t ini, float yR);
static void desenharRegua(float a, time_t ini) { desenharReguaEm(a, ini, G_TOPO); }
static void desenharReguaEm(float a, time_t ini, float yR) {
  float ppm = G_L_FAIXA_W / (float)G_L_JANELA_MIN;
  int i, n = G_L_JANELA_MIN / G_L_PASSO_MIN;
#ifdef NV_TOUCH_UI
  time_t reguaIni = ini;
  if (telefoneui_ativo()) {
    reguaIni = (ini / (G_L_PASSO_MIN * 60)) * (G_L_PASSO_MIN * 60);
    n++;
  }
#endif
  for (i = 0; i < n; i++) {
    char h[12];
    float x = G_L_FAIXA_X + (float)(i * G_L_PASSO_MIN) * ppm;
    TxtLinha t;
#ifdef NV_TOUCH_UI
    time_t tick = reguaIni + (time_t)i * G_L_PASSO_MIN * 60;
    x = G_L_FAIXA_X + (float)(tick - ini) / 60.0f * ppm;
    if (telefoneui_ativo() && (x < G_L_FAIXA_X || x >= G_AREA_DIR)) continue;
    fmtHora(tick, h, sizeof h);
#else
    fmtHora(ini + (time_t)i * G_L_PASSO_MIN * 60, h, sizeof h);
#endif
    t = txt_linha(TXT_DET_META2, h, 160, 163, 172, 255);
    // Rotulo logo a direita do tique, como nas grades de referencia: o tique
    // marca o instante, o rotulo le junto dele.
#ifdef NV_TOUCH_UI
    if (!gRetrato() || (float)t.w <= G_AREA_DIR - x - 10.0f)
#endif
      txt_desenhar_alpha(t, x + 10.0f, yR + 2.0f, a);
    gfx_cor((GfxRect){ x, yR + 4.0f, 1.0f, 30.0f }, 0.0f, 1, 1, 1, 0.20f * a);
    // Meio da meia hora: tique curto.
    if (!gRetrato() || x + 15.0f * ppm < G_AREA_DIR)
      gfx_cor((GfxRect){ x + 15.0f * ppm, yR + 26.0f, 1.0f, 8.0f }, 0.0f,
              1, 1, 1, 0.12f * a);
  }
  gfx_cor((GfxRect){ G_AREA_X, yR + 34.0f, G_AREA_W, 1.0f }, 0.0f,
          1, 1, 1, 0.08f * a);
}

// Superficies da grade: cinzas quentes em degraus, do mais fundo ao em foco.
// So quatro valores, e em DEGRAU — cor de texto por quadro rasterizaria a
// linha a cada quadro (ver ctxmenu.c); a superficie pode variar, o texto nao.
#define G_SUP_COL      0.092f   // coluna do canal
#define G_SUP_COL_FOCO 0.165f
#define G_SUP_FUTURO   0.108f
#define G_SUP_NO_AR    0.150f
#define G_SUP_FOCO     0.235f

// A linha de um canal no modo lista, em DUAS PASSADAS: `passo` 0 pinta as
// superficies, 1 pinta logo e texto. Entre as duas guia_desenhar escurece o
// PASSADO (a faixa antes da linha "agora") de uma vez so, por cima das
// superficies e por baixo do texto — o titulo do programa no ar comeca no
// passado e tem de continuar legivel.
//
// Com `alvo`, devolve o retangulo que o anel de foco deve abracar: a celula
// do programa em `tFoco`, ou a faixa "sem grade" inteira.
// Altura da celula: G_L_CEL na grade, menor na faixa do mini guia.
static float gCel = 72.0f;
// O canal no ar ganha a barra na cor de realce na coluna (so na faixa, onde
// o foco pode estar longe dele).
static const char *gIdNoAr = "";
static void desenharLinhaLista(GCanal *c, float y, int focada, float a,
                               time_t agoraT, time_t ini, time_t tFoco,
                               int passo, float agoraX, GfxRect *alvo) {
  float h = gCel;
#ifdef NV_TOUCH_UI
  if (telefoneui_ativo() && !overlay) h = G_L_CEL;
#endif
  int textoRetrato = gRetrato() && h >= 70.0f;
  float raioB = 10.0f / h;
  time_t fimJ = ini + (time_t)G_L_JANELA_MIN * 60;
  float ppm = G_L_FAIXA_W / (float)G_L_JANELA_MIN;
  GfxRect col = { G_AREA_X, y, G_L_COL, h };
  // Grade da XMLTV ou, no Xtream, a curta (#158): ver gFaixa.
  int epg = gEstadoGrade(c);

  if (passo == 0) {
    // GLASS UI: a coluna do canal nao tem superficie propria; o foco e a
    // superficie da LINHA inteira (desenhada por quem chama). So o logo
    // ganha uma placa.
    GfxRect pl = { G_AREA_X + 8.0f, y + (h - 52.0f) * 0.5f, 96.0f, 52.0f };
    gfx_cor(pl, 0.28f, 1, 1, 1, (focada ? 0.12f : 0.07f) * a);
    if (gIdNoAr[0] && !strcmp(c->id, gIdNoAr)) {
      float ar, ag, ab;
      ajustes_acento(&ar, &ag, &ab);
      gfx_cor((GfxRect){ col.x - 4.0f, col.y + (h - 28.0f) * 0.5f, 4.0f, 28.0f }, 0.5f,
              ar, ag, ab, a);
    }
  } else {
    char num[16];
    TxtLinha n;
    snprintf(num, sizeof num, "%d", c->numero);
    n = txt_linha(textoRetrato ? TXT_DET_META2 : TXT_ILHA_HORA, num, 115, 114, 113, 255);
    { GfxRect cx = { G_AREA_X + 14.0f, y + (h - 44.0f) * 0.5f, 84.0f, 44.0f };
      logoNaCaixa(c, cx, 76.0f, 36.0f, G_LOGO_CLARO, a); }
    { float nx = G_AREA_X + 120.0f;
      float tw = G_L_COL - 120.0f - 16.0f - (c->fav ? 30.0f : 0.0f);
      int ct = focada ? 255 : 222;
      TxtLinha t = txt_linha_corta(textoRetrato ? TXT_V2_ROT : TXT_ILHA_NOME, c->nome, ct, ct, ct - 4, 255, tw);
      float bloco = (float)t.h + (float)n.h - 2.0f;
      txt_desenhar_alpha(t, nx, y + (h - bloco) * 0.5f, a);
      txt_desenhar_alpha(n, nx, y + (h - bloco) * 0.5f + (float)t.h - 2.0f, a);
      if (c->fav) {
        TxtLinha st = txt_linha(TXT_DET_META2, "\xe2\x98\x85", 255, 214, 90, 255);
        txt_desenhar_alpha(st, G_AREA_X + G_L_COL - 16.0f - (float)st.w,
                           y + (h - (float)st.h) * 0.5f, a);
      } }
  }
  if (alvo) *alvo = col;

  if (epg >= 0) {
    EpgProg ps[16];
    int n = gFaixa(c, ini, fimJ, ps, 16), k, desenhou = 0;
    if (n > 16) n = 16;
    for (k = 0; k < n; k++) {
      time_t i0 = ps[k].ini > ini ? ps[k].ini : ini;
      time_t i1 = ps[k].fim < fimJ ? ps[k].fim : fimJ;
      float x1 = G_L_FAIXA_X + (float)(i0 - ini) / 60.0f * ppm;
      float x2 = G_L_FAIXA_X + (float)(i1 - ini) / 60.0f * ppm;
      int atual = ps[k].ini <= agoraT && agoraT < ps[k].fim;
      int foc = focada && ps[k].ini <= tFoco && tFoco < ps[k].fim;
      GfxRect b = { x1 + 3.0f, y, x2 - x1 - 6.0f, h };
      if (b.w < 6.0f) continue;
      desenhou = 1;
      if (foc && alvo) *alvo = b;
      if (passo == 0) {
        // Superficie branca em alfa: foco 18%, no ar 8%, futuro 4%.
        float l = foc ? 0.18f : atual ? 0.08f : 0.04f;
        gfx_cor(b, raioB, 1, 1, 1, l * a);
        if (atual && ps[k].fim > ps[k].ini) {
          float f = anim_clamp((float)(agoraT - ps[k].ini) / (float)(ps[k].fim - ps[k].ini), 0.0f, 1.0f);
          float ar3, ag3, ab3, bw = b.w - 28.0f;
          ajustes_acento(&ar3, &ag3, &ab3);
#ifdef NV_TOUCH_UI
          if (!telefoneui_ativo() || bw > 0.0f) {
#endif
          gfx_cor((GfxRect){ b.x + 14.0f, b.y + h - 5.0f, bw, 3.0f }, 0.5f, 1, 1, 1, 0.14f * a);
          if (bw * f > 1.0f)
            gfx_cor((GfxRect){ b.x + 14.0f, b.y + h - 5.0f, bw * f, 3.0f }, 0.5f, ar3, ag3, ab3, a);
#ifdef NV_TOUCH_UI
          }
#endif
        }
        continue;
      }
      // Rotulo so onde cabe: a regra da casa e desenhar MENOS rotulos, nunca
      // fonte menor. Titulo acima de 64 px, horario acima de 150.
      if (b.w > 64.0f) {
        int ct = foc ? 255 : atual ? 238 : 150;
        // O titulo do programa que comecou antes da janela comeca na borda
        // visivel dela. Texto vem DEPOIS do veu do passado, entao o do
        // programa no ar continua claro mesmo nascendo antes da linha agora.
        float tx = b.x + 14.0f;
        // SINO do lembrete no canto direito da celula. A grade que mudou o
        // horario do programa leva o lembrete junto (ver lembrete.h).
        int li = ps[k].ini > agoraT ? lembrete_achar(c->id, ps[k].ini, ps[k].titulo) : -1;
        float sinoW = li >= 0 && b.w > 64.0f ? 30.0f : 0.0f;
        TxtLinha t = txt_linha_corta(textoRetrato ? TXT_V2_ROT : TXT_CW_TITULO, ps[k].titulo, ct, ct, ct, 255,
                                     b.x + b.w - 14.0f - tx - sinoW);
        if (li >= 0) {
          float ar2, ag2, ab2;
          const Lembrete *lm = lembrete_item(li);
          if (lm->ini != ps[k].ini || lm->fim != ps[k].fim) lembrete_ajustar(li, ps[k].ini, ps[k].fim);
          ajustes_acento(&ar2, &ag2, &ab2);
          gfx_icone((GfxRect){ b.x + b.w - 34.0f, y + (h < 70.0f ? (h - 22.0f) * 0.5f : 10.0f),
                               22.0f, 22.0f }, "sino", ar2, ag2, ab2, a);
        }
        if (h < 70.0f) {
          // Celula baixa (faixa): so o titulo, centrado, e a barra do quanto
          // ja passou no programa do ar — a regua em cima diz os horarios.
          txt_desenhar_alpha(t, tx, y + (h - (float)t.h) * 0.5f - 2.0f, a);
        } else
        txt_desenhar_alpha(t, tx, y + 7.0f, a);
        if (b.w > 120.0f && h >= 70.0f) {
          char h1[12], h2[12], faixa[24];
          int cm = foc ? 200 : 146;
          TxtLinha m;
          fmtHora(ps[k].ini, h1, sizeof h1); fmtHora(ps[k].fim, h2, sizeof h2);
          snprintf(faixa, sizeof faixa, "%s \xe2\x80\x93 %s", h1, h2);
          // Horario pela metade ("00:54 –…") nao informa: inteiro ou nada.
          m = txt_linha(gRetrato() ? TXT_V2_28 : TXT_DET_META2, faixa, cm, cm + 2, cm + 8, 255);
          if ((float)m.w <= b.x + b.w - 14.0f - tx)
            txt_desenhar_alpha(m, tx, y + (gRetrato() ? 58.0f : 41.0f), a);
        }
      }
    }
    if (desenhou) return;
  }

  // SEM GRADE (ou grade que nao cobre esta janela): UMA celula discreta na
  // faixa inteira, com a frase comecando na linha "agora" — e nao a fileira
  // de barras cinza vazias que parecia defeito.
  { GfxRect b = { G_L_FAIXA_X + 3.0f, y, G_L_FAIXA_W - 6.0f, h };
    if (focada && alvo) *alvo = b;
    if (passo == 0) {
      if (epg == -1) gfx_esqueleto(b, raioB, 0.095f, 0.097f, 0.105f, a);
      else {
        float l = focada ? 0.105f : 0.068f;
        gfx_cor(b, raioB, l, l, l, a);
      }
    } else {
      int ct = focada ? 176 : 118;
      const char *m = epg == -1 ? i18n("Carregando programação…")
                    : epg == -3 ? i18n("Não foi possível carregar a programação")
               : epg == -2 ? i18n("Sem grade de programação")
                                : i18n("Sem próximos programas");
      float tx = agoraX > b.x + 20.0f ? agoraX + 18.0f : b.x + 20.0f;
      TxtLinha t = txt_linha_corta(TXT_DET_META, m, ct, ct + 2, ct + 8, 255,
                                   b.x + b.w - 20.0f - tx);
      txt_desenhar_alpha(t, tx, y + (h - (float)t.h) * 0.5f, a);
    } }
}

// --- painel de addons -------------------------------------------------------------
static void desenharPainelAddons(float a) {
  float x = G_PA_X + G_PA_MARG, w = G_PA_W - 2.0f * G_PA_MARG;
  float ar, ag, ab, y0 = G_PA_LISTA_Y;
  int n = paN, i, vidro = ajustes_vidro();
  int tf = 243;
  ajustes_acento(&ar, &ag, &ab);

  ponteiro_camada();
#ifdef NV_TOUCH_UI
  if (painel) {
    float area = NV_TELA_H - 80.0f - y0;
    float fim = paFim();
    toquerol_vincular(&toqueAddon, (GfxRect){ G_PA_X, y0, G_PA_W, area }, gfx_escala(), 0.0f, fmaxf(0.0f, fim - area), 1, &paRol);
    ponteiro_rolagem(toqueAddonRolar);
  }
#endif
  { GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    gfx_cor(tela, 0.0f, 0, 0, 0, (vidro ? 0.30f : 0.42f) * a); }
  // PAINEL FLUTUANTE como o de Salvos e a barra lateral (21/09/2026): solto
  // 24 px do topo e da base, cantos de 28 px pelo menor lado, translucido,
  // UMA luz difusa na cor de realce pelo canto superior direito, presa aos
  // cantos (GFX_LUZ). O veu de tela cheia acima ja e a primeira camada.
  { GfxRect p = { G_PA_X, 24.0f, G_PA_W, NV_TELA_H - 48.0f };
    materialPainelGuia(p, 28.0f, a); }

  { TxtLinha t = txt_linha(TXT_HEADLINE, i18n("Addons de canais"), 240, 242, 248, 255);
    txt_desenhar_alpha(t, x, 64.0f, a); }
  // ENQUANTO O GUIA RECARREGA POR CAUSA DE UMA TECLA DAQUI, o painel diz isso
  // no lugar da dica. Instalar ou ligar um addon dispara a recarga em fundo
  // (painelAplicar) e sem aviso a pessoa via o "Instalado" e nenhum canal
  // novo, por segundos — pedido do dono em 18/09.
  if (paMexeu && (fioVivo || recarregarPend)) {
    TxtLinha t = txt_linha(TXT_CAPTION, i18n("Atualizando a lista de canais…"), ar * 255, ag * 255, ab * 255, 255);
    txt_desenhar_alpha(t, x, 118.0f, a);
  } else
  txt_bloco(TXT_CAPTION,
            i18n("Ligar ou desligar aqui vale para o app inteiro, não só para o guia."),
            150, 153, 162, x, 118.0f, w, 28.0f, a, 2);

  gfx_recorte(G_PA_X, y0 - 8.0f, G_PA_W, NV_TELA_H - 80.0f - y0 + 8.0f);

  { TxtLinha t = txt_linha(TXT_CAPTION, i18n("NA SUA CONTA"), 148, 200, 255, 255);
    txt_desenhar_alpha(t, x, y0 - paRol, a); }
  if (n == 0) {
    TxtLinha t = txt_linha(TXT_CAPTION, i18n("Nenhum addon de canais nesta conta."), 150, 153, 162, 255);
    txt_desenhar_alpha(t, x, y0 + 36.0f - paRol, a);
  }
  for (i = 0; i < painelN(); i++) {
    float yi = y0 + paItemY(i) - paRol;
    GfxRect row = { x, yi, w, (i < n ? G_PA_ROW : G_PA_ROW_REC) - 8.0f };
    int f = i == paFoco;
    if (yi + row.h < y0 - 8.0f || yi > NV_TELA_H - 80.0f) continue;
    if (a > 0.99f) ponteiro_alvo_faixa(row.x, row.y, row.w, row.h, y0 - 8.0f, NV_TELA_H - 80.0f, ponteiroAddon, NULL, i, 0);
    // A linha em repouso faz parte da folha; apenas o foco recebe
    // a superficie clara comum das listas, sem brilho ou bloco de accent.
    if (f) plrui_linha_foco(row, 20.0f, a);
    if (i < n) {
      int ai = paIdx[i];
      int sc = sabeCanal(addons_base(ai));
      int ligado = addons_ativo(ai);
      // MANIFESTO QUE NAO RESPONDEU (sonda ja rodou, add-on ligado): o guia
      // diz isso em vez de "ainda nao conferido" (#283).
      int semResposta = sc == -1 && ligado && nSabe > 0 && !fioVivo && !recarregarPend;
      const char *sub0 = sc == 1 ? i18n("Fornece canais")
                      : sc == 0 ? i18n("Sem catálogo de canais")
                      : (fioVivo || recarregarPend) ? i18n("Carregando canais…")
                      : semResposta ? i18n("Addon demorou ou não respondeu")
                                : i18n("Ainda não conferido pelo guia");
      char subBuf[200];
      const char *sub = sub0;
      if (baseOculta(addons_base(ai))) {
        snprintf(subBuf, sizeof subBuf, "%s  ·  %s", sub0, i18n("Oculto no guia"));
        sub = subBuf;
      }
      GfxRect pill = { x + w - 24.0f - 136.0f, yi + (row.h - 40.0f) * 0.5f, 136.0f, 40.0f };
#ifdef NV_TOUCH_UI
      if (telefoneui_ativo()) pill.y = yi + 22.0f;
#endif
      float txtW = pill.x - 24.0f - (x + 24.0f);
      {
        TxtLinha t = f ? txt_linha_corta(TXT_BODY, addons_nome(ai), tf, tf, tf, 255, txtW)
                       : txt_linha_corta(TXT_BODY, addons_nome(ai), 240, 241, 245, 255, txtW);
        txt_desenhar_alpha(t, x + 24.0f, yi + 12.0f, a); }
      { TxtLinha t = f ? txt_linha_corta(TXT_CAPTION, sub, 190, 192, 200, 255, txtW)
                       : txt_linha_corta(TXT_CAPTION, sub, 150, 153, 162, 255, txtW);
        txt_desenhar_alpha(t, x + 24.0f, yi + 48.0f, a); }
      // LIGADO = pilula preenchida; DESLIGADO = so o anel. Preenchimento e o
      // que o app usa para "e este", e o anel e o que sobra para "poderia
      // ser" — sem inventar um interruptor de celular que a 3 m nao se le.
      if (ligado) {
        if (f) gfx_cor(pill, 0.5f, 0.11f, 0.115f, 0.13f, a);
        else   gfx_cor(pill, 0.5f, 0.86f, 0.865f, 0.88f, a);
        { TxtLinha t = f ? txt_linha(TXT_CAPTION, i18n("Ligado"), 240, 241, 245, 255)
                         : txt_linha(TXT_CAPTION, i18n("Ligado"), 20, 21, 25, 255);
          txt_desenhar_alpha(t, pill.x + (pill.w - t.w) * 0.5f, pill.y + (pill.h - t.h) * 0.5f, a); }
      } else {
        float c = 0.72f;
        gfx_rect(pill, 0, GFX_ANEL, 0, 0.05f, 0, 0.5f, c, c, c + 0.02f, 0.9f * a);
        { TxtLinha t = txt_linha(TXT_CAPTION, i18n("Desligado"), 190, 192, 200, 255);
          txt_desenhar_alpha(t, pill.x + (pill.w - t.w) * 0.5f, pill.y + (pill.h - t.h) * 0.5f, a); }
      }
#ifdef NV_TOUCH_UI
      if (telefoneui_ativo()) {
        int oculto = baseOculta(addons_base(ai));
        GfxRect local = {x + 24, yi + 80, w - 48, 56};
        gfx_cor(local, .5f, .14f, .15f, .17f, a);
        gfx_icone((GfxRect){local.x + 12, local.y + 18, 20, 20}, oculto ? "oculto" : "check", .82f, .84f, .88f, a);
        TxtLinha t = txt_linha_corta(TXT_CAPTION2, i18n(oculto ? "Oculto no guia" : "No guia"), 218, 222, 228, 255, local.w - 56);
        txt_desenhar_alpha(t, local.x + 44, local.y + (local.h - t.h) * .5f, a);
        if (a > .99f) ponteiro_alvo_faixa(local.x, local.y, local.w, local.h, y0 - 8, NV_TELA_H - 80,
                                         NULL, ponteiroAddonGuia, ai, i);
      }
#endif
    } else {
      const GRec *rc = &rec[i - n];
      int inst = recInstalado(rc);
      GfxRect pill = { x + w - 24.0f - 136.0f, yi + (row.h - 40.0f) * 0.5f, 136.0f, 40.0f };
      // A coluna de texto termina ANTES da pilula, com folga: pill.x - 24.
      float txtW = pill.x - 24.0f - (x + 24.0f);
      // O PRIMEIRO DA LISTA E O DESTAQUE — a ordem do arquivo e a curadoria,
      // e o dono pediu o Fenix TV la em cima (foi o mais rapido a responder
      // e o unico que ja vem classificado, medido em 18/09).
      //
      // O selo vai NA LINHA DO NOME, logo depois dele, como uma etiqueta —
      // e nao encostado na pilula, onde ficava em cima da descricao (foto do
      // dono em 19/09: "Spotlight" atravessando "channels,…").
      int destaque = (i - n == 0);
      TxtLinha selo = txt_linha(TXT_CAPTION2, i18n("Destaque"), 20, 21, 25, 255);
      float seloW = destaque ? selo.w + 20.0f : 0.0f;
      { float nomeW = txtW - (destaque ? seloW + 12.0f : 0.0f);
        TxtLinha t = f ? txt_linha_corta(TXT_BODY, rc->nome, tf, tf, tf, 255, nomeW)
                       : txt_linha_corta(TXT_BODY, rc->nome, 240, 241, 245, 255, nomeW);
        txt_desenhar_alpha(t, x + 24.0f, yi + 12.0f, a);
        if (destaque) {
          GfxRect d = { x + 24.0f + t.w + 12.0f, yi + 12.0f + (t.h - 26.0f) * 0.5f, seloW, 26.0f };
          if (f) gfx_cor(d, 0.5f, 0.11f, 0.115f, 0.13f, a);
          else   gfx_cor(d, 0.5f, ar, ag, ab, a);
          { TxtLinha t2 = f ? txt_linha(TXT_CAPTION2, i18n("Destaque"), 240, 241, 245, 255) : selo;
            txt_desenhar_alpha(t2, d.x + 10.0f, d.y + (d.h - t2.h) * 0.5f, a); }
        } }
      if (paErro == i) {
        const char *m = paErroCheio ? i18n("Não coube: a conta já tem o máximo de addons")
                                    : i18n("Não foi possível instalar");
        TxtLinha t = txt_linha_corta(TXT_CAPTION, m, 237, 77, 77, 255, txtW);
        txt_desenhar_alpha(t, x + 24.0f, yi + 48.0f, a);
      } else if (rc->desc[0]) {
        // Em ingles usa a quarta coluna se existir; senao a portuguesa, que e
        // melhor que linha vazia.
        const char *desc = (ajustes_idioma() != IDIOMA_PT && ajustes_idioma() != IDIOMA_PTPT && rc->descEn[0]) ? rc->descEn : rc->desc;
        if (f) txt_bloco(TXT_CAPTION, desc, 190, 192, 200,     x + 24.0f, yi + 46.0f, txtW, 27.0f, a, 2);
        else   txt_bloco(TXT_CAPTION, desc, 150, 153, 162,  x + 24.0f, yi + 46.0f, txtW, 27.0f, a, 2);
      }
      if (inst) {
        TxtLinha t = f ? txt_linha(TXT_CAPTION, i18n("Instalado"), 190, 192, 200, 255)
                       : txt_linha(TXT_CAPTION, i18n("Instalado"), 150, 153, 162, 255);
        txt_desenhar_alpha(t, pill.x + pill.w - t.w, pill.y + (pill.h - t.h) * 0.5f, a);
      } else {
        float c = f ? tf / 255.0f : 0.72f;
        gfx_rect(pill, 0, GFX_ANEL, 0, 0.05f, 0, 0.5f, c, c, c + 0.02f, 0.9f * a);
        { TxtLinha t = f ? txt_linha(TXT_CAPTION, i18n("Instalar"), tf, tf, tf, 255)
                         : txt_linha(TXT_CAPTION, i18n("Instalar"), 240, 241, 245, 255);
          txt_desenhar_alpha(t, pill.x + (pill.w - t.w) * 0.5f, pill.y + (pill.h - t.h) * 0.5f, a); }
      }
    }
  }
  if (nRec > 0) {
    float yr = y0 + 36.0f + (float)n * G_PA_ROW + 28.0f - paRol;
    TxtLinha t = txt_linha(TXT_CAPTION, i18n("SUGESTÕES DE QUEM PUBLICA O APP"), 148, 200, 255, 255);
    txt_desenhar_alpha(t, x, yr, a);
    // A premissa, na tela e nao so no comentario: quem le "sugestoes" tem
    // direito de saber que nao ha ranking por tras.
    txt_bloco(TXT_CAPTION,
              i18n("Lista mantida à mão. Não é ranking: não existe medição pública de popularidade de addons Stremio."),
              150, 153, 162, x, yr + 30.0f, w, 28.0f, a, 2);
  }
  gfx_sem_recorte();

  { TxtLinha t = txt_linha_corta(TXT_CAPTION,
        i18n("OK liga ou desliga no app todo  ·  ← → mostra ou esconde só no guia  ·  Voltar volta ao guia"),
        140, 142, 150, 255, w);
    txt_desenhar_alpha(t, x, NV_TELA_H - 54.0f, a); }
}

// --- painel de filtros da Live TV ---------------------------------------------------
// Mesma folha do painel de addons (veu, material, recorte, linha em foco). Nivel 0:
// tres linhas de qualidade e uma por categoria; nivel 1: os canais de uma categoria.
static void ponteiroFiltroCanais(int i, int b) {
  (void)b;
  if (!painel || paModo != 1 || fNivel || i < 3 || i >= painelN()) return;
  paFoco = i;
  filtrosEvento(SDLK_RIGHT, SDL_GetTicks());
}
static void desenharPainelFiltros(float a) {
  float x = G_PA_X + G_PA_MARG, w = G_PA_W - 2.0f * G_PA_MARG;
  float y0 = G_PA_LISTA_Y;
  int n = painelN(), i, vidro = ajustes_vidro();
  const char *qNome[3] = { i18n("Todas as versões"), i18n("Só 4K"), i18n("Preferir 4K") };
  const char *qSub[3] = { NULL, NULL,
                          i18n("Um por canal: o 4K, ou a maior resolução disponível") };
  ponteiro_camada();
#ifdef NV_TOUCH_UI
  if (painel) {
    float area = NV_TELA_H - 80.0f - y0;
    toquerol_vincular(&toqueAddon, (GfxRect){ G_PA_X, y0, G_PA_W, area }, gfx_escala(), 0.0f, fmaxf(0.0f, paFim() - area), 1, &paRol);
    ponteiro_rolagem(toqueAddonRolar);
  }
#endif
  { GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    gfx_cor(tela, 0.0f, 0, 0, 0, (vidro ? 0.30f : 0.42f) * a); }
  { GfxRect p = { G_PA_X, 24.0f, G_PA_W, NV_TELA_H - 48.0f };
    materialPainelGuia(p, 28.0f, a); }
  { TxtLinha t = txt_linha(TXT_HEADLINE, i18n("Filtros da Live TV"), 240, 242, 248, 255);
    txt_desenhar_alpha(t, x, 64.0f, a); }
  txt_bloco(TXT_CAPTION, i18n("As mudanças valem ao fechar este painel."), 150, 153, 162, x, 118.0f, w, 28.0f, a, 2);

  gfx_recorte(G_PA_X, y0 - 8.0f, G_PA_W, NV_TELA_H - 80.0f - y0 + 8.0f);
  if (fNivel) {
    TxtLinha t = txt_linha_corta(TXT_CAPTION, fCatNome, 148, 200, 255, 255, w);
    txt_desenhar_alpha(t, x, y0 - paRol, a);
    if (!fN) {
      TxtLinha vazio = txt_linha(TXT_CAPTION, i18n("Nenhum canal disponível"), 150, 153, 162, 255);
      txt_desenhar_alpha(vazio, x, y0 + 36.0f, a);
    }
  } else {
    TxtLinha t = txt_linha(TXT_CAPTION, i18n("Qualidade"), 148, 200, 255, 255);
    TxtLinha t2 = txt_linha(TXT_CAPTION, i18n("Categorias"), 148, 200, 255, 255);
    txt_desenhar_alpha(t, x, y0 - paRol, a);
    txt_desenhar_alpha(t2, x, y0 + 36.0f + 3.0f * G_PF_ROW + 28.0f - paRol, a);
    if (fN == 0) {
      TxtLinha t3 = txt_linha(TXT_CAPTION, i18n("Nenhum canal disponível"), 150, 153, 162, 255);
      txt_desenhar_alpha(t3, x, y0 + 36.0f + 3.0f * G_PF_ROW + 28.0f + 36.0f - paRol, a);
    }
  }
  for (i = 0; i < n; i++) {
    float yi = y0 + paItemY(i) - paRol;
    GfxRect row = { x, yi, w, G_PF_ROW - 8.0f };
    GfxRect pill = { x + w - 24.0f - 136.0f, yi + (row.h - 40.0f) * 0.5f, 136.0f, 40.0f };
    float txtW = pill.x - 24.0f - (x + 24.0f);
    int f = i == paFoco, q = !fNivel && i < 3, ligado;
    const char *nome, *sub = NULL, *pillTxt;
    char cnt[48];
    if (yi + row.h < y0 - 8.0f || yi > NV_TELA_H - 80.0f) continue;
    if (a > 0.99f) ponteiro_alvo_faixa(row.x, row.y, row.w, row.h, y0 - 8.0f, NV_TELA_H - 80.0f, ponteiroAddon, NULL, i, 0);
    if (f) plrui_linha_foco(row, 20.0f, a);
    if (!q && !fNivel && a > 0.99f) {
      GfxRect abrir = { pill.x - 68.0f, row.y, 64.0f, row.h };
      ponteiro_alvo_faixa(abrir.x, abrir.y, abrir.w, abrir.h, y0 - 8.0f, NV_TELA_H - 80.0f,
                          NULL, ponteiroFiltroCanais, i, 0);
      TxtLinha t = txt_linha(TXT_BODY, ">", 190, 192, 200, 255);
      txt_desenhar_alpha(t, abrir.x + 20.0f, row.y + 22.0f, a);
      txtW -= 68.0f;
    }
    if (q) {
      nome = qNome[i]; sub = qSub[i]; ligado = fltLive.modo == i;
      pillTxt = ligado ? i18n("Ligado") : "";
    } else {
      const GFItem *it = &fIt[fNivel ? i : i - 3];
      nome = it->nome; ligado = !it->oculto;
      if (!fNivel && it->n > 0) { snprintf(cnt, sizeof cnt, i18n("Canais · %d"), it->n); sub = cnt; }
      pillTxt = ligado ? i18n("Ligado") : i18n("Desligado");
    }
    { TxtLinha t = f ? txt_linha_corta(TXT_BODY, nome, 243, 243, 243, 255, txtW)
                     : txt_linha_corta(TXT_BODY, nome, 240, 241, 245, 255, txtW);
      txt_desenhar_alpha(t, x + 24.0f, sub ? yi + 12.0f : yi + (row.h - t.h) * 0.5f, a); }
    if (sub) txt_bloco(TXT_CAPTION, sub, f ? 190 : 150, f ? 192 : 153, f ? 200 : 162,
                        x + 24.0f, yi + 40.0f, txtW, 24.0f, a, 2);
    // Como no painel de addons: preenchida = "e este", anel = "poderia ser".
    if (ligado) {
      if (f) gfx_cor(pill, 0.5f, 0.11f, 0.115f, 0.13f, a);
      else   gfx_cor(pill, 0.5f, 0.86f, 0.865f, 0.88f, a);
      if (pillTxt[0]) { TxtLinha t = f ? txt_linha(TXT_CAPTION, pillTxt, 240, 241, 245, 255)
                                       : txt_linha(TXT_CAPTION, pillTxt, 20, 21, 25, 255);
        txt_desenhar_alpha(t, pill.x + (pill.w - t.w) * 0.5f, pill.y + (pill.h - t.h) * 0.5f, a); }
    } else {
      float c = 0.72f;
      gfx_rect(pill, 0, GFX_ANEL, 0, 0.05f, 0, 0.5f, c, c, c + 0.02f, 0.9f * a);
      if (pillTxt[0]) { TxtLinha t = txt_linha(TXT_CAPTION, pillTxt, 190, 192, 200, 255);
        txt_desenhar_alpha(t, pill.x + (pill.w - t.w) * 0.5f, pill.y + (pill.h - t.h) * 0.5f, a); }
    }
  }
  gfx_sem_recorte();
  txt_bloco(TXT_CAPTION,
      fNivel ? i18n("OK mostra ou esconde o canal  ·  ← ou Voltar volta às categorias")
             : i18n("OK escolhe ou esconde  ·  → abre os canais da categoria  ·  Voltar volta ao guia"),
      140, 142, 150, x, NV_TELA_H - 78.0f, w, 24.0f, a, 2);
}

// O anel que desliza. `alvo` em coordenadas de CONTEUDO (y + rolagem): a mola
// persegue o alvo com o dt do relogio do desenho, e a rolagem e subtraida so
// na hora de pintar — rolar a grade nao faz o anel "correr atras" da celula.
__attribute__((unused)) static void desenharAnelFoco(float a, Uint32 agora, float rol) {
  float dt, ar, ag, ab;
  GfxRect r;
  if (!focoAnelTem) { focoAnelOk = 0; return; }
  dt = focoAnelTick ? (float)(agora - focoAnelTick) / 1000.0f : 0.0f;
  if (dt < 0.0f) dt = 0.0f;
  if (dt > 0.05f) dt = 0.05f;
  focoAnelTick = agora;
  if (!focoAnelOk || fabsf(focoAnelAlvo.y - focoAnel.y) > 3.0f * G_L_ROW) {
    focoAnel = focoAnelAlvo; focoAnelOk = 1;
  } else {
    // 20/s: ~150 ms para 95% — um pouco mais lento que o anel de cartaz, que
    // SALTA; aqui o deslize e o que diz de que celula o foco veio.
    focoAnel.x = anim_mola(focoAnel.x, focoAnelAlvo.x, dt, 20.0f);
    focoAnel.y = anim_mola(focoAnel.y, focoAnelAlvo.y, dt, 20.0f);
    focoAnel.w = anim_mola(focoAnel.w, focoAnelAlvo.w, dt, 20.0f);
    focoAnel.h = anim_mola(focoAnel.h, focoAnelAlvo.h, dt, 20.0f);
  }
  ajustes_acento(&ar, &ag, &ab);
  // Anel POR FORA da celula, encostado nela: 3 px de traco (3/4 de
  // NV_ANEL_FOCO), concentrico com o canto de 10 px da celula (raioB).
  r = (GfxRect){ focoAnel.x, focoAnel.y - rol, focoAnel.w, focoAnel.h };
  if (r.h > 0.0f)
    gfx_anel_fora(r, 10.0f / r.h, 0.0f, NV_ANEL_FOCO * 0.75f, ar, ag, ab, a);
}

// --- a faixa do mini guia ------------------------------------------------------
#define G_B_LINHAS   5
#define G_B_ROW     66.0f
#define G_B_CEL     58.0f

// O vizinho de (l, c) na ordem da lista, atravessando as categorias. 0 quando
// nao ha.
static int vizinhoLista(int *l, int *c, int dir) {
  if (dir > 0) {
    if (*c + 1 < linhaN(*l)) { (*c)++; return 1; }
    { int k = *l + 1;
      while (k < nLinhas() && linhaN(k) < 1) k++;
      if (k >= nLinhas()) return 0;
      *l = k; *c = 0; return 1; }
  }
  if (*c > 0) { (*c)--; return 1; }
  { int k = *l - 1;
    while (k >= 0 && linhaN(k) < 1) k--;
    if (k < 0) return 0;
    *l = k; *c = linhaN(k) - 1; return 1; }
}

// A FAIXA: o rodape da tela (~41% da altura) por cima do video, que segue em
// tela cheia e intocado (nenhum video_janela aqui). Degrade escuro de baixo
// para cima, a regua de meia hora e cinco canais — o focado no meio sempre
// que da, dois de cada lado. O canal no ar tem a barra na cor de realce.
static void desenharBanda(float a, Uint32 agora) {
  time_t agoraT = time(NULL);
  time_t ini = janelaIni(agoraT);
  time_t tFoco = tempoDesloc() > 0 ? ini + 10 * 60 : agoraT;
  float topo = NV_TELA_H - 36.0f - (float)G_B_LINHAS * G_B_ROW - 42.0f - 44.0f;
  float yR = topo + 44.0f, y0 = yR + 42.0f, fimY = y0 + (float)G_B_LINHAS * G_B_ROW - 8.0f;
  int ll[G_B_LINHAS], cc[G_B_LINHAS], n = 0, k, passo, subiu = 0;
  int agoraVis = agoraT >= ini && agoraT < ini + (time_t)G_L_JANELA_MIN * 60;
  float agoraX = agoraVis ? G_L_FAIXA_X + (float)(agoraT - ini) / 60.0f
                            * (G_L_FAIXA_W / (float)G_L_JANELA_MIN) : 0.0f;
  float ar, ag, ab;
  if (nLinhas() < 1 || !linhaItem(focoLin, focoCol)) return;
#ifdef NV_TOUCH_UI
  if (!toquerol_livre(&toqueTempo)) toqueTempoMin = (float)janelaDesl;
  toquePpm = G_L_FAIXA_W / G_L_JANELA_MIN;
  toquerol_vincular(&toqueTempo, (GfxRect){ G_L_FAIXA_X, yR, G_L_FAIXA_W, fimY - yR }, gfx_escala(), 0.0f, G_L_DESL_MAX, 0, &toqueTempoMin);
  ponteiro_rolagem(toqueGuiaRolar);
#endif
  ajustes_acento(&ar, &ag, &ab);

  // Uma rampa continua em vez de oito faixas de alfa constante: aquelas
  // criavam emendas horizontais por definicao. GFX_VEU_BAIXO aplica nv_dither por
  // fragmento; manter a mesma cor e o teto de alfa (0,94). O plano de video
  // continua visivel no topo, e a base sustenta o texto da grade.
  // GLASS UI (mockup do player de 03/10, "aovivo-guia"): a banda vira uma
  // ILHA baixa a 40 das bordas sobre o veu leve (era um veu de 94% da
  // metade da tela), com o titulo, as dicas em teclas e a linha focada em
  // superficie — o anel de acento em volta da celula sai.
  gfx_veu_css((GfxRect){ 0, NV_TELA_H - 300.0f, NV_TELA_W, 300.0f }, 0, 1.0f, 1.0f, 0.60f * a);
  plrui_material((GfxRect){ 40.0f, topo - 26.0f, NV_TELA_W - 80.0f, NV_TELA_H - 40.0f - (topo - 26.0f) }, 36.0f, 0, a);

  // Quem entra: sobe ate dois a partir do foco e completa descendo.
  { int l = focoLin, c = focoCol;
    while (subiu < 2 && vizinhoLista(&l, &c, -1)) subiu++;
    ll[0] = l; cc[0] = c; n = 1;
    while (n < G_B_LINHAS && vizinhoLista(&l, &c, 1)) { ll[n] = l; cc[n] = c; n++; }
    // No fim da lista faltam linhas embaixo: sobe mais para a faixa encher.
    while (n < G_B_LINHAS) {
      int l2 = ll[0], c2 = cc[0];
      if (!vizinhoLista(&l2, &c2, -1)) break;
      memmove(ll + 1, ll, sizeof(int) * (size_t)n); memmove(cc + 1, cc, sizeof(int) * (size_t)n);
      ll[0] = l2; cc[0] = c2; n++;
    } }

  { TxtLinha t = txt_linha(TXT_G30B, "Guia de canais", 243, 242, 239, 255);
    const char *k[3] = { "OK", "OK", "Voltar" },
      *r[3] = { "Troca de canal", "Guia completo", "Fechar" };
    txt_desenhar_alpha(t, G_AREA_X, topo - 2.0f, a);
    plrui_dicas(k, r, 3, G_AREA_DIR, topo - 2.0f + (float)t.h * 0.5f, 1, a); }
  desenharReguaEm(a, ini, yR);

  gCel = G_B_CEL;
  gIdNoAr = player_id_canal();
  focoAnelTem = 0;
  for (k = 0; k < n; k++)
    if (ll[k] == focoLin && cc[k] == focoCol)
      plrui_linha_foco((GfxRect){ G_AREA_X - 14.0f, y0 + (float)k * G_B_ROW - 5.0f,
                                  G_AREA_W + 28.0f, G_B_ROW - 2.0f }, 22.0f, a);
  for (passo = 0; passo < 2; passo++) {
    for (k = 0; k < n; k++) {
      int foc = ll[k] == focoLin && cc[k] == focoCol;
      GfxRect alvo;
      float y = y0 + (float)k * G_B_ROW;
#ifdef NV_TOUCH_UI
      if (passo == 0 && a > 0.99f) ponteiro_alvo(G_AREA_X, y, G_AREA_W, G_B_ROW, ponteiroCanal, NULL, ll[k], cc[k]);
#endif
      desenharLinhaLista(linhaItem(ll[k], cc[k]), y, foc, a, agoraT, ini, tFoco,
                         passo, agoraX, foc ? &alvo : NULL);
      if (foc && passo == 0) { focoAnelAlvo = alvo; focoAnelTem = 1; }
    }
    if (passo == 0 && agoraVis) {
      if (agoraX > G_L_FAIXA_X)
        gfx_cor((GfxRect){ G_L_FAIXA_X, y0 - 4.0f, agoraX - G_L_FAIXA_X, fimY - y0 + 8.0f },
                0.0f, 0.02f, 0.02f, 0.025f, 0.45f * a);
      gfx_cor((GfxRect){ agoraX - 1.0f, yR + 30.0f, 2.0f, fimY - yR - 26.0f }, 0.0f,
              ar, ag, ab, 0.85f * a);
    }
  }
  gCel = G_L_CEL;
  gIdNoAr = "";
  (void)agora;
  focoAnelOk = 0;   // na banda o foco e a superficie da linha, sem anel
  if (agoraVis) {
    char hora[12];
    int tf = ajustes_tinta_foco();
    TxtLinha t;
    GfxRect pl;
    fmtHora(agoraT, hora, sizeof hora);
    t = txt_linha(TXT_PG_ROTULO, hora, tf, tf, tf, 255);
    pl = (GfxRect){ agoraX - ((float)t.w + 20.0f) * 0.5f, yR + 1.0f, (float)t.w + 20.0f, 30.0f };
    gfx_cor(pl, 0.5f, ar, ag, ab, a);
    txt_desenhar_alpha(t, pl.x + 10.0f, pl.y + (pl.h - (float)t.h) * 0.5f, a);
  }
}

// --- BUSCA DO GUIA ------------------------------------------------------------------
// Pedido do dono (01/10/2026): "Buscar" na barra de cima. O teclado do app
// (teclado.h, com espaco) colhe o texto; a busca procura CANAL pelo nome (sem
// acento nem caixa, livetv_regras.h) ou pelo numero, e PROGRAMA na grade de
// agora ate G_BUSCA_HORAS a frente. O resultado e uma lista com duas secoes:
//   Canais    — OK abre o canal;
//   Programas — OK no que esta no ar abre o canal; no futuro, leva o guia ao
//               canal com a grade naquela hora (modo lista), onde o OK marca o
//               lembrete como em qualquer celula do futuro.
#define G_BUSCA_CANAIS   24
#define G_BUSCA_PROGS    40
#define G_BUSCA_HORAS     6
#define G_BUSCA_RODADAS  16     // lotes de 24 por canal (#344)
#define G_BUSCA_ROW      (gRetrato() ? 120.0f : 76.0f)
#define G_BUSCA_W       (gRetrato() ? fminf(1180.0f, G_AREA_W) : 1180.0f)
static int buscaEstado;        // 0 fechada, 1 teclado, 2 resultado
static char buscaTexto[TECLADO_MAX + 1];
static int buscaNC, buscaNP, buscaFoco;
static int buscaCanal[G_BUSCA_CANAIS];
typedef struct { int canal; time_t ini, fim; char titulo[112]; } GBuscaProg;
static GBuscaProg buscaProg[G_BUSCA_PROGS];
// A varredura continua no fio do guia: os titulos do EPG sao emprestados ate
// epg_passo. Lotes pequenos deixam o teclado fechar e o painel aparecer sem
// bloquear a entrada por segundos em listas grandes. Publica os programas
// juntos, ja ordenados, para nao trocar a linha sob o dedo ou o foco.
static struct {
  int ativa, canal, vistos, rodada, n, teto;
  unsigned canaisRev, epgRev;
  Uint32 inicio;
  time_t de, ate;
  char agulha[TECLADO_MAX + 1];
  GBuscaProg progs[G_BUSCA_PROGS];
} buscaPendente;
static float buscaRol, buscaAnim;

static void buscaAbrir(void) {
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toqueBusca);
#endif
  buscaEstado = 1;
  teclado_abrir_com("Buscar no guia", "Nome ou número do canal, ou o nome de um programa",
                    TECLADO_MAX, "abcdefghijklmnopqrstuvwxyz0123456789 ", buscaTexto);
}
static int buscaN(void) { return buscaNC + buscaNP; }
static void ponteiroBuscaItem(int i, int b) {
  (void)b;
  if (buscaEstado != 2 || i < -1 || i >= buscaN()) return;
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toqueBusca);
#endif
  buscaFoco = i;
}

static void buscaConcluir(void) {
  time_t agoraT = buscaPendente.de;
  buscaNP = buscaPendente.n;
  memcpy(buscaProg, buscaPendente.progs, (size_t)buscaNP * sizeof *buscaProg);
  { int a, b;   // ordem: no ar antes, depois o que comeca primeiro
    for (a = 1; a < buscaNP; a++)
      for (b = a; b > 0; b--) {
        int vivoA = buscaProg[b - 1].ini <= agoraT, vivoB = buscaProg[b].ini <= agoraT;
        if (vivoA > vivoB || (vivoA == vivoB && buscaProg[b - 1].ini <= buscaProg[b].ini)) break;
        { GBuscaProg t = buscaProg[b]; buscaProg[b] = buscaProg[b - 1]; buscaProg[b - 1] = t; }
      } }
  buscaPendente.ativa = 0;
  printf("[guia] busca: %d canal(is), %d programa(s), %u ms\n",
         buscaNC, buscaNP, (unsigned)(SDL_GetTicks() - buscaPendente.inicio));
  fflush(stdout);
}

static void buscaFazer(const char *q) {
  char agulha[TECLADO_MAX + 1];
  int i, numero = 0, soDigito = 1;
  const char *p;
  if (q != buscaTexto) snprintf(buscaTexto, sizeof buscaTexto, "%s", q ? q : "");
  nv_dobrar(buscaTexto, agulha, sizeof agulha);
  { size_t n = strlen(agulha), k = 0;
    while (n && agulha[n - 1] == ' ') agulha[--n] = 0;
    while (agulha[k] == ' ') k++;
    if (k) memmove(agulha, agulha + k, strlen(agulha + k) + 1); }
  buscaNC = buscaNP = 0; buscaFoco = 0; buscaRol = 0.0f;
  memset(&buscaPendente, 0, sizeof buscaPendente);
  buscaPendente.canaisRev = canaisRevisao;
  buscaPendente.epgRev = epg_revisao();
  buscaPendente.inicio = SDL_GetTicks();
  buscaPendente.de = time(NULL);
  buscaPendente.ate = buscaPendente.de + G_BUSCA_HORAS * 3600;
  snprintf(buscaPendente.agulha, sizeof buscaPendente.agulha, "%s", agulha);
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toqueBusca);
#endif
  if (!agulha[0]) return;
  for (p = agulha; *p; p++) if (*p < '0' || *p > '9') soDigito = 0;
  if (soDigito) numero = atoi(agulha);
  for (i = 0; i < nCanais && buscaNC < G_BUSCA_CANAIS; i++)
    if (fonteVisivel(canais[i].fonte) && !categoriaOculta(canais[i].cat) &&
        ((numero && canais[i].numero == numero) || nv_contem_dobrado(canais[i].nome, agulha)))
      buscaCanal[buscaNC++] = i;
  buscaPendente.ativa = nCanais > 0 && (!soDigito || strlen(agulha) > 3);
  if (!buscaPendente.ativa) buscaConcluir();
}

static void buscaPasso(void) {
  Uint32 inicio = SDL_GetTicks();
  // Limite de tempo E de paginas: relogios de baixa resolucao tambem cedem.
  for (int paginas = 0; buscaPendente.ativa && paginas < 256; paginas++) {
    EpgProg ps[24];
    int i = buscaPendente.canal;
    if (i >= nCanais || buscaPendente.n >= G_BUSCA_PROGS) { buscaConcluir(); break; }
    if(!fonteVisivel(canais[i].fonte) || categoriaOculta(canais[i].cat)) {
      buscaPendente.canal++;buscaPendente.vistos=buscaPendente.rodada=0;continue;
    }
    int n = gFaixaDesde(&canais[i], buscaPendente.de, buscaPendente.ate,
                        buscaPendente.vistos, ps, (int)(sizeof ps / sizeof ps[0]));
    if (n > (int)(sizeof ps / sizeof ps[0])) n = (int)(sizeof ps / sizeof ps[0]);
    for (int k = 0; k < n && buscaPendente.n < G_BUSCA_PROGS; k++) {
      if (!ps[k].titulo || !nv_contem_dobrado(ps[k].titulo, buscaPendente.agulha)) continue;
      GBuscaProg *p = &buscaPendente.progs[buscaPendente.n++];
      p->canal = i; p->ini = ps[k].ini; p->fim = ps[k].fim;
      snprintf(p->titulo, sizeof p->titulo, "%s", ps[k].titulo);
    }
    buscaPendente.rodada++;
    if (n < (int)(sizeof ps / sizeof ps[0]) || buscaPendente.rodada >= G_BUSCA_RODADAS) {
      if (buscaPendente.rodada == G_BUSCA_RODADAS && !buscaPendente.teto) {
        buscaPendente.teto = 1;
        printf("[guia] busca: canal com %d programas ou mais na janela; o resto fica de fora\n",
               G_BUSCA_RODADAS * (int)(sizeof ps / sizeof ps[0]));
      }
      buscaPendente.canal++; buscaPendente.vistos = buscaPendente.rodada = 0;
    } else buscaPendente.vistos += n;
    if (buscaPendente.canal >= nCanais || buscaPendente.n >= G_BUSCA_PROGS) buscaConcluir();
    if (SDL_GetTicks() - inicio >= 4) break;
  }
}

static void buscaFechar(void) { buscaEstado = 0; buscaPendente.ativa = 0; }

// OK num resultado.
static void buscaAgir(void) {
  if (buscaFoco < buscaNC) {
    GCanal *c = &canais[buscaCanal[buscaFoco]];
    buscaFechar();
    guia_focar_id(c->id);
    pedirCanal(c);
    return;
  }
  if (buscaFoco - buscaNC < buscaNP) {
    int k = buscaFoco - buscaNC;
    GCanal *c = &canais[buscaProg[k].canal];
    time_t agoraT = time(NULL);
    buscaFechar();
    guia_focar_id(c->id);
    if (buscaProg[k].ini <= agoraT) { pedirCanal(c); return; }
    // FUTURO: a grade em lista, adiantada ate a meia hora do programa.
    { long min = (long)(buscaProg[k].ini - (agoraT / 1800) * 1800) / 60;
      int passo = (int)(min / G_L_PASSO_MIN) * G_L_PASSO_MIN;
      if (!modoLista) alternarModo();
      janelaDesl = passo < 0 ? 0 : passo > G_L_DESL_MAX ? G_L_DESL_MAX : passo; }
  }
}

// 1 = a tecla era da busca.
static int buscaEvento(const SDL_Event *e) {
  SDL_Keycode k;
  if (buscaEstado == 1) { teclado_evento(e); return 1; }
  if (buscaEstado != 2) return 0;
  if (e->type != SDL_KEYDOWN) return 1;
  k = e->key.keysym.sym;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      e->key.keysym.scancode == NV_SCANCODE_BACK) { buscaFechar(); return 1; }
  if (k == SDLK_UP) { if (buscaFoco > -1) buscaFoco--; return 1; }
  if (k == SDLK_DOWN) { if (buscaFoco + 1 < buscaN()) buscaFoco++; return 1; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
    if (buscaFoco < 0) buscaAbrir();      // a linha "Nova busca"
    else buscaAgir();
    return 1;
  }
  return 1;
}

static void buscaAtualizar(float dt, Uint32 agora) {
  buscaAnim = anim_mola(buscaAnim, buscaEstado == 2 ? 1.0f : 0.0f, dt, 12.0f);
  if (buscaEstado == 1) {
    int r;
    teclado_atualizar(dt, agora);
    r = teclado_resultado();
    if (r == TECLADO_PRONTO) { buscaFazer(teclado_texto()); buscaEstado = 2; }
    else if (r == TECLADO_CANCELOU) buscaEstado = (buscaN() || buscaPendente.ativa) ? 2 : 0;
  }
  if (buscaEstado == 2) {
    // Uma publicacao pode reordenar os canais sem mudar sua quantidade.
    if (buscaPendente.canaisRev != canaisRevisao || buscaPendente.epgRev != epg_revisao())
      buscaFazer(buscaTexto);
    buscaPasso();
    // A linha em foco fica a vista: a lista rola por ela.
    float y = (float)(buscaFoco + 1) * G_BUSCA_ROW + (buscaFoco >= buscaNC ? 56.0f : 0.0f);
    float area = NV_TELA_H - 300.0f;
    float alvo = y - area * 0.5f;
    if (alvo < 0.0f) alvo = 0.0f;
#ifdef NV_TOUCH_UI
    if (!toquerol_livre(&toqueBusca))
#endif
      buscaRol += (alvo - buscaRol) * (dt * 12.0f > 1.0f ? 1.0f : dt * 12.0f);
  }
}

static void buscaDesenhar(Uint32 agora) {
  float ea = buscaAnim, ar, ag, ab, x0 = (NV_TELA_W - G_BUSCA_W) * 0.5f, y;
  int i, tf = ajustes_tinta_foco();
  time_t agoraT = time(NULL);
  char b[200];
  (void)tf;
  if (buscaEstado == 1) { teclado_desenhar(agora); return; }
  if (buscaEstado == 2) {
    // Mesmo no primeiro quadro (alpha zero), nao deixar um toque chegar ao
    // guia nem virar OK na lista vazia enquanto os alvos ainda aparecem.
    ponteiro_camada();
    ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, NULL, 0, 0);
  }
  if (ea < 0.01f) return;
#ifdef NV_TOUCH_UI
  if (buscaEstado == 2) {
    float area = NV_TELA_H - 230.0f;
    float fim = (buscaN() + 1) * G_BUSCA_ROW + (buscaNC > 0 ? 56.0f : 0.0f) + (buscaNP > 0 ? 56.0f : 0.0f);
    toquerol_vincular(&toqueBusca, (GfxRect){ x0, 130.0f, G_BUSCA_W, area }, gfx_escala(), 0.0f, fmaxf(0.0f, fim + 10.0f - area), 1, &buscaRol);
    ponteiro_rolagem(toqueBuscaRolar);
  }
#endif
  ajustes_acento(&ar, &ag, &ab);
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.80f * ea);
  { GfxRect p = { x0 - 40.0f, 40.0f, G_BUSCA_W + 80.0f, NV_TELA_H - 80.0f };
    gfx_vidro_ou_solido(p, 28.0f / p.h, GFX_VIDRO_DENS_MODAL, ea, 0.070f, 0.072f, 0.080f, 1.0f);
    if (!ajustes_vidro()) gfx_luz_canto(p, 28.0f / p.h, p.w * 0.30f, -p.w * 0.25f, p.w * 1.45f, ar, ag, ab, 0.06f * ea); }
  snprintf(b, sizeof b, i18n("Busca: “%s”"), buscaTexto);
  txt_desenhar_alpha(txt_linha_corta(TXT_HEADLINE, b, 242, 243, 247, 255, G_BUSCA_W), x0, 72.0f, ea);
  gfx_recorte(x0 - 20.0f, 130.0f, G_BUSCA_W + 40.0f, NV_TELA_H - 230.0f);
  y = 140.0f - buscaRol;
  // "Nova busca": foco -1.
  { GfxRect r = { x0, y, G_BUSCA_W, G_BUSCA_ROW - 10.0f };
    int foc = buscaFoco == -1;
    if (buscaEstado == 2 && ea > 0.99f)
      ponteiro_alvo_faixa(r.x, r.y, r.w, r.h, 130.0f, NV_TELA_H - 100.0f, ponteiroBuscaItem, NULL, -1, 0);
    if (foc) gfx_cor(r, 14.0f / r.h, ar, ag, ab, ea);
    gfx_icone((GfxRect){ r.x + 20.0f, r.y + (r.h - 26.0f) * 0.5f, 26.0f, 26.0f }, "menu_search",
              foc ? 0.06f : 0.9f, foc ? 0.07f : 0.9f, foc ? 0.08f : 0.92f, ea);
    txt_desenhar_alpha(txt_linha(TXT_BODY, i18n("Nova busca"), foc ? 16 : 232, foc ? 18 : 236, foc ? 22 : 244, 255),
                       r.x + 62.0f, r.y + 18.0f, ea);
    y += G_BUSCA_ROW; }
  for (i = 0; i < buscaN(); i++) {
    int foc = buscaFoco == i;
    GfxRect r;
    if (i == 0 && buscaNC) {
      snprintf(b, sizeof b, i18n("Canais · %d"), buscaNC);
      txt_desenhar_alpha(txt_linha(TXT_CAPTION, b, 150, 154, 164, 255), x0, y + 20.0f, ea);
      y += 56.0f;
    }
    if (i == buscaNC && buscaNP) {
      snprintf(b, sizeof b, i18n("Programas · %d"), buscaNP);
      txt_desenhar_alpha(txt_linha(TXT_CAPTION, b, 150, 154, 164, 255), x0, y + 20.0f, ea);
      y += 56.0f;
    }
    r = (GfxRect){ x0, y, G_BUSCA_W, G_BUSCA_ROW - 10.0f };
    if (r.y > NV_TELA_H || r.y + r.h < 120.0f) { y += G_BUSCA_ROW; continue; }
    if (buscaEstado == 2 && ea > 0.99f)
      ponteiro_alvo_faixa(r.x, r.y, r.w, r.h, 130.0f, NV_TELA_H - 100.0f, ponteiroBuscaItem, NULL, i, 0);
    if (foc) gfx_cor(r, 14.0f / r.h, ar, ag, ab, ea);
    else gfx_cor(r, 14.0f / r.h, 1, 1, 1, 0.04f * ea);
    if (i < buscaNC) {
      const GCanal *c = &canais[buscaCanal[i]];
      char n[16];
      guia_logo_desenhar(logoDe(c), c->nome, (GfxRect){ r.x + 14.0f, r.y + 6.0f, 92.0f, r.h - 12.0f },
                         92.0f, r.h - 16.0f, 0.965f, ea);
      snprintf(n, sizeof n, "%d", buscaCanal[i] + 1);
      txt_desenhar_alpha(txt_linha(TXT_CAPTION, n, foc ? 30 : 150, foc ? 32 : 153, foc ? 38 : 162, 255),
                         r.x + 124.0f, r.y + 22.0f, ea);
      txt_desenhar_alpha(txt_linha_corta(gRetrato() ? TXT_V2_ROT : TXT_BODY, c->nome, foc ? 16 : 236, foc ? 18 : 238, foc ? 22 : 244, 255,
                                         G_BUSCA_W - 260.0f), r.x + 190.0f, r.y + 16.0f, ea);
    } else {
      int k = i - buscaNC, vivo = buscaProg[k].ini <= agoraT;
      const GCanal *c = &canais[buscaProg[k].canal];
      char h1[12], h2[12], quando[64];
      struct tm lt;
      localtime_r(&buscaProg[k].ini, &lt); hora_tela(h1, sizeof h1, &lt);
      localtime_r(&buscaProg[k].fim, &lt); hora_tela(h2, sizeof h2, &lt);
      if (vivo) snprintf(quando, sizeof quando, "%s", i18n("Ao vivo"));
      else {
        long m = (long)(buscaProg[k].ini - agoraT) / 60;
        if (m < 60) snprintf(quando, sizeof quando, i18n("daqui a %ld min"), m);
        else snprintf(quando, sizeof quando, i18n("daqui a %ld h %02ld"), m / 60, m % 60);
      }
      txt_desenhar_alpha(txt_linha_corta(gRetrato() ? TXT_V2_ROT : TXT_BODY, buscaProg[k].titulo, foc ? 16 : 236, foc ? 18 : 238,
                                         foc ? 22 : 244, 255, G_BUSCA_W - 360.0f), r.x + 24.0f, r.y + 8.0f, ea);
      snprintf(b, sizeof b, "%s · %s–%s", c->nome, h1, h2);
      txt_desenhar_alpha(txt_linha_corta(gRetrato() ? TXT_V2_28 : TXT_CAPTION, b, foc ? 40 : 150, foc ? 42 : 154, foc ? 48 : 164, 255,
                                         G_BUSCA_W - 360.0f), r.x + 24.0f, r.y + (gRetrato() ? 58.0f : 40.0f), ea);
      { TxtLinha q = vivo ? txt_linha(TXT_CAPTION, quando, 200, 30, 40, 255)
                          : txt_linha(TXT_CAPTION, quando, foc ? 30 : 190, foc ? 32 : 196, foc ? 38 : 206, 255);
        if (vivo && !foc) guia_selo_ao_vivo(r.x + r.w - 24.0f - 120.0f, r.y + (r.h - 32.0f) * 0.5f, ea);
        else txt_desenhar_alpha(q, r.x + r.w - 24.0f - (float)q.w, r.y + (r.h - (float)q.h) * 0.5f, ea); }
    }
    y += G_BUSCA_ROW;
  }
  if (!buscaPendente.ativa && !buscaN())
    txt_desenhar_alpha(txt_linha_corta(TXT_BODY,
        i18n("Nada encontrado nos canais nem na grade das próximas horas."),
        180, 184, 194, 255, G_BUSCA_W), x0, y + 20.0f, ea);
  gfx_sem_recorte();
  txt_desenhar_alpha(txt_linha(TXT_CAPTION, buscaPendente.ativa ? i18n("Carregando programação…") : i18n("OK abre  ·  ↑↓ escolhe  ·  Voltar fecha"), 128, 130, 138, 255),
                     x0, NV_TELA_H - 84.0f, ea);
}

#ifdef NV_TOUCH_UI
static int toqueGuiaRolar(const PonteiroRolagem *e) {
  if (!telefoneui_ativo()) { toqueLimpar(); return 0; }
  PonteiroRolagem local = *e;
  if (e->fase == PONT_ROL_INICIO) {
    toqueAtual = NULL; toqueLinha = -1;
    if (modoLista || overlay) {
      if (!e->eixoY) toqueAtual = &toqueTempo;
      else if (!overlay) toqueAtual = &toqueLista;
    } else if (e->eixoY) toqueAtual = &toqueGradeY;
    else {
      for (int l = 0; l < nLinhas(); l++)
        if (toquerol_evento(toqueXde(l), e)) { toqueLinha = l; toqueAtual = toqueXde(l); break; }
      if (!toqueAtual) return 0;
    }
  }
  if (!toqueAtual) return 0;
  if (toqueAtual == &toqueTempo) {
    if (toquePpm <= 0.0f) return 0;
    local.delta /= toquePpm;
  }
  int r = toquerol_evento(toqueAtual, &local);
  if (r) {
    okDesde = 0; dirSeg = 0;
    if (e->fase == PONT_ROL_INICIO && !e->eixoY) {
      if (modoLista && !overlay) toqueLista.livre = 1;
      else if (!overlay) toqueGradeY.livre = 1;
    }
    if (e->fase == PONT_ROL_INICIO) velY = velL = 0.0f;
    if (overlay) bandaUlt = SDL_GetTicks();
  }
  return r;
}
static int toqueCategoriaRolar(const PonteiroRolagem *e) {
  int r = toquerol_evento(&toqueCat, e);
  if (r) { catUlt = SDL_GetTicks(); catVelRol = 0.0f; dirSeg = 0; }
  return r;
}
static int toqueAddonRolar(const PonteiroRolagem *e) {
  int r = toquerol_evento(&toqueAddon, e);
  if (r && e->fase == PONT_ROL_INICIO) paVelRol = 0.0f;
  return r;
}
static int toqueBuscaRolar(const PonteiroRolagem *e) { return toquerol_evento(&toqueBusca, e); }
static void toqueGuiaRetomar(void) {
  if (!telefoneui_ativo()) { toqueLimpar(); return; }
  if (buscaEstado == 2) {
    if (toquerol_livre(&toqueBusca)) {
      float ponto = buscaRol + toqueBusca.regiao.h * 0.35f, y = G_BUSCA_ROW;
      buscaFoco = -1;
      for (int i = 0; i < buscaN(); i++) {
        if (i == 0 && buscaNC) y += 56.0f;
        if (i == buscaNC && buscaNP) y += 56.0f;
        buscaFoco = i;
        if (y + G_BUSCA_ROW >= ponto) break;
        y += G_BUSCA_ROW;
      }
    }
    toquerol_limpar(&toqueBusca); return;
  }
  if (catAberto) {
    if (toquerol_livre(&toqueCat)) catFoco = (int)((catRol + toqueCat.regiao.h * 0.35f) / G_CAT_ROW);
    if (catFoco >= nLinhas()) catFoco = nLinhas() - 1;
    if (catFoco < 0) catFoco = 0;
    toquerol_limpar(&toqueCat); return;
  }
  if (painel) {
    if (toquerol_livre(&toqueAddon)) {
      float ponto = paRol + toqueAddon.regiao.h * 0.35f;
      paFoco = 0;
      while (paFoco + 1 < painelN() && paItemY(paFoco + 1) <= ponto) paFoco++;
    }
    toquerol_limpar(&toqueAddon); return;
  }
  if (toquerol_livre(&toqueLista) && nLinhas() > 0) {
    float ponto = rolL + toqueLista.regiao.h * 0.35f, y = 0.0f;
    for (int l = 0; l < nLinhas(); l++) {
      float h = G_L_HEAD + linhaN(l) * G_L_ROW;
      if (ponto < y + h || l + 1 == nLinhas()) {
        focoLin = l; focoCol = (int)((ponto - y - G_L_HEAD) / G_L_ROW);
        if (focoCol >= linhaN(l)) focoCol = linhaN(l) - 1;
        if (focoCol < 0) focoCol = 0;
        break;
      }
      y += h;
    }
    focoTopo = 0;
  }
  if (toquerol_livre(&toqueGradeY) && nLinhas() > 0) {
    focoLin = (int)((rolY + toqueGradeY.regiao.h * 0.35f) / G_PASSO_Y);
    if (focoLin >= nLinhas()) focoLin = nLinhas() - 1;
    if (focoCol >= linhaN(focoLin)) focoCol = linhaN(focoLin) - 1;
    if (focoCol < 0) focoCol = 0;
    focoTopo = 0;
  }
  if (toqueLinha >= 0 && toqueLinha < nLinhas() && toquerol_livre(&toqueGradeX[toqueLinha])) {
    focoLin = toqueLinha;
    focoCol = (int)((*rolXde(focoLin) + toqueGradeX[focoLin].regiao.w * 0.35f) / (G_CARD_W + G_GAP_X));
    if (focoCol >= linhaN(focoLin)) focoCol = linhaN(focoLin) - 1;
    if (focoCol < 0) focoCol = 0;
    focoTopo = 0;
  }
  if (toquerol_livre(&toqueTempo)) janelaDesl = (int)((toqueTempoMin + G_L_PASSO_MIN * 0.5f) / G_L_PASSO_MIN) * G_L_PASSO_MIN;
  toqueLimpar();
}
#endif

void guia_desenhar(Uint32 agora) {
#ifdef NV_TOUCH_UI
  if (!telefoneui_ativo()) toqueLimpar();
#endif
  time_t agoraT = time(NULL);
  time_t tFoco = instanteFoco(agoraT);
  float a = entrada;
  int l, i, temLinhas;
  if (a < 0.01f || !guia_visivel()) return;
  if (overlay) { desenharBanda(a, agora); return; }
  gTopoMedir();
  temLinhas = (estado == G_PRONTO || estado == G_BAIXANDO) && nLinhas() > 0;
#ifdef NV_TOUCH_UI
  if (!guiaVazioTelefone()) toqueVazio.offset = NULL;
  if (temLinhas) {
    float area = G_L_BASE - (modoLista ? G_L_TOPO : G_TOPO);
    if (modoLista) {
      toquerol_vincular(&toqueLista, (GfxRect){ G_AREA_X, G_L_TOPO, G_AREA_W, area }, gfx_escala(), 0.0f, fmaxf(0.0f, listaAltura() + G_L_FADE - area), 1, &rolL);
      if (!toquerol_livre(&toqueTempo)) toqueTempoMin = (float)janelaDesl;
      toquePpm = G_L_FAIXA_W / G_L_JANELA_MIN;
      toquerol_vincular(&toqueTempo, (GfxRect){ G_L_FAIXA_X, G_TOPO, G_L_FAIXA_W, G_L_BASE - G_TOPO }, gfx_escala(), 0.0f, G_L_DESL_MAX, 0, &toqueTempoMin);
    } else {
      toquerol_vincular(&toqueGradeY, (GfxRect){ G_AREA_X, G_TOPO, G_AREA_W, area }, gfx_escala(), 0.0f, fmaxf(0.0f, nLinhas() * G_PASSO_Y - area), 1, &rolY);
      for (int k = 0; k < capToqueGrade; k++) toqueGradeX[k].offset = NULL;
    }
    ponteiro_rolagem(toqueGuiaRolar);
  }
#endif

  // Full guide owns every background pixel, including during entry/exit.
  // Only the validated channel video hole can reveal the native plane.
  { GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    gfx_cor(tela, 0.0f, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B,
            1.0f); }

  // Open the native video hole before drawing header text over the hero.
  if (temLinhas) desenharHero(a, agoraT, tFoco);

  // Title and clock stand alone; controls and summary each have their row.
  { float chipsX = desenharTopo(a);
    char sub[160];
    TxtLinha t = txt_linha(TXT_ILHA_TITULO,
                           overlay ? i18n("Guia de canais") : i18n("Guia TV"),
                           243, 242, 239, 255);
    float sx = G_AREA_X + (float)t.w + 20.0f;
    txt_desenhar_alpha(t, G_AREA_X, G_TOPO_Y + (G_TOPO_H - (float)t.h) * 0.5f, a);
    if (estado == G_BAIXANDO && nCanais > 0)   // lista do cache na tela, rede atras
      snprintf(sub, sizeof sub, i18n("%d canais · %d categorias · atualizando…"), nCanais, nCats);
    else if (estado == G_BAIXANDO)
      snprintf(sub, sizeof sub, "%s", i18n("Carregando canais…"));
    else if (falhas && !nCanais)
      snprintf(sub, sizeof sub, "%s",
               i18n("Os addons de canais não responderam."));
    else if (xtFalha == XT_HTTP && !nCanais)
      snprintf(sub, sizeof sub, i18n("O Xtream respondeu com erro HTTP %d."), xtHttp);
    else if (xtFalha && !nCanais)
      snprintf(sub, sizeof sub, "%s", xtFalha == XT_RECUSOU
               ? i18n("O Xtream recusou o usuário e a senha.")
               : xtFalha == XT_PAGINA
               ? i18n("O Xtream devolveu uma página da web em vez da lista.")
               : i18n("A lista do Xtream não respondeu."));
    else if (estado == G_FALHOU || (fontesOk && !nFontes && !nCanais))
      snprintf(sub, sizeof sub, "%s",
               i18n("Nenhum canal: sem addon de canais e sem portal IPTV."));
    // A falha do Xtream toma o lugar da contagem simples, e so ela: os canais
    // dos addons continuam na tela e funcionam, o que falta e o portal.
    else if (xtFalha == XT_SEM_RESPOSTA || xtFalha == XT_PAGINA || xtFalha == XT_HTTP)
      snprintf(sub, sizeof sub, i18n("%d canais · %d categorias · a lista do Xtream não respondeu"),
               nCanais, nCats);
    else if (xtFalha == XT_RECUSOU)
      snprintf(sub, sizeof sub, i18n("%d canais · %d categorias · o Xtream recusou o usuário e a senha"),
               nCanais, nCats);
    // A CONTA XTREAM, quando ha o que dizer (#158): vencida, desativada,
    // telas cheias ou vencendo. E a causa de "nenhum canal toca" que se sabe
    // ANTES de tentar um canal. Lido do que o fio do guia guardou, sem rede.
    else if (xtream_configurado() && xtAviso(sub, sizeof sub)) { }
    // O CORTE SE DIZ: o guia guarda um teto de canais/categorias (guiacorte.h)
    // e antes cortava em silencio. Mesma frase da linha da fonte (feat/guia-tv).
    else if (guiacorte_cortou(&corte))
      guiacorte_linha(&corte, nCanais,
                      guiacorte_por_canais(&corte) ? i18n("Mostrando %d de %d canais (o que cabe nesta TV)")
                                                   : i18n("Mostrando %d de %d canais (limite de categorias)"),
                      sub, sizeof sub);
    else
      snprintf(sub, sizeof sub, i18n("%d canais · %d categorias · segure %s para pular seção"),
               nCanais, nCats, "\xe2\x86\x91\xe2\x86\x93");
    { float subX = G_AREA_X;
      TxtLinha st = txt_linha_corta(gRetrato() ? TXT_DET_META2 : TXT_ILHA_HORA, sub, 150, 153, 162, 255,
                                    gRetrato() ? G_AREA_W : G_INFO_W);
      float subY = (gRetrato() ? gTopoFim : G_CHIPS_Y + G_TOPO_H) + 12.0f;
      (void)chipsX; (void)sx;
      txt_desenhar_alpha(st, subX, subY, a); } }

  // Fileiras de canais. G_BAIXANDO com linhas publicadas e a RECARGA pedida
  // pelo painel de addons: a lista antiga fica na tela ate a nova chegar, em
  // vez de sumir por segundos (o subtitulo ja diz "Carregando canais…").
  focoAnelTem = 0;
  if (modoLista && temLinhas) {
    time_t ini = janelaIni(agoraT);
    int agoraVis = agoraT >= ini && agoraT < ini + (time_t)G_L_JANELA_MIN * 60;
    float agoraX = agoraVis ? G_L_FAIXA_X + (float)(agoraT - ini) / 60.0f
                              * (G_L_FAIXA_W / (float)G_L_JANELA_MIN)
                            : 0.0f;
    // Onde a grade ACABA na tela: a linha agora e o veu nao descem alem do
    // ultimo canal quando a lista e curta.
    float fimY = G_L_TOPO - rolL + listaAltura();
    int maisAcima = rolL > 1.0f;
    int passo;
    float ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    if (fimY > G_L_BASE) fimY = G_L_BASE;
    // Rows share the page surface and extend through the screen edges.
    desenharRegua(a, ini);
    gfx_recorte(0.0f, G_L_TOPO - 8.0f, NV_TELA_W, G_L_BASE - G_L_TOPO + 8.0f);
    for (passo = 0; passo < 2; passo++) {
      float y = G_L_TOPO - rolL;
      for (l = 0; l < nLinhas(); l++) {
        int n = linhaN(l);
        float dim = 1.0f;
        float bloco = G_L_HEAD + (float)n * G_L_ROW;
        if (y > G_L_BASE) break;
        if (y + bloco < G_L_TOPO - 8.0f) { y += bloco; continue; }
        // Cabecalho de categoria DISCRETO: 22 px em cinza. A categoria e
        // contexto da lista, nao titulo — quem manda na tela e o heroi.
        if (passo == 1) {
          char cab[140];
          int at = 0;
          TxtLinha t;
          snprintf(cab, sizeof cab, "%s  \xc2\xb7  %d", linhaNome(l), n);
          t = txt_linha_corta(TXT_PG_ROTULO, cab, at ? 255 : 150, at ? 255 : 153,
                              at ? 255 : 162, 255, G_L_COL);
          txt_desenhar_alpha(t, G_AREA_X + 4.0f, y + G_L_HEAD - 12.0f - (float)t.h, a * dim);
        }
        y += G_L_HEAD;
        for (i = 0; i < n; i++, y += G_L_ROW) {
          int foc = l == focoLin && i == focoCol && !focoTopo;
          GfxRect alvo;
          if (y + G_L_ROW < G_L_TOPO - 8.0f || y > G_L_BASE) continue;
          if (passo == 0 && a > 0.99f)
            ponteiro_alvo_faixa(G_AREA_X - 14.0f, y - 4.0f, G_AREA_W + 28.0f, G_L_CEL + 8.0f,
                                G_L_TOPO - 8.0f, G_L_BASE, ponteiroCanal, NULL, l, i);
          if (passo == 0) {
            GfxRect row = {0, y - 4.0f, NV_TELA_W, G_L_CEL + 8.0f};
            if (foc) plrui_linha_foco(row, 0.0f, a * dim);
            else gfx_cor(row, 0, 1, 1, 1, 0.025f * a * dim);
          }
          desenharLinhaLista(linhaItem(l, i), y, foc, a * dim, agoraT, ini, tFoco,
                             passo, agoraX, foc ? &alvo : NULL);
          if (foc && passo == 0) {
            focoAnelAlvo = alvo; focoAnelAlvo.y += rolL; focoAnelTem = 1;
          }
        }
      }
      // O PASSADO, entre as duas passadas: um veu da cor do fundo da borda da
      // faixa ate a linha agora, por cima das superficies e por baixo do
      // texto. Um desenho so para a grade inteira. A linha agora vai junto,
      // tambem por baixo do texto: por cima, ela riscava o titulo do
      // programa no ar bem no meio.
      if (passo == 0 && agoraVis) {
        gfx_cor((GfxRect){ agoraX - 1.0f, G_L_TOPO - 8.0f, 2.0f, fimY - G_L_TOPO + 8.0f },
                0.0f, ar, ag, ab, 0.85f * a);
      }
    }
    focoAnelOk = 0;   // Glass UI: foco e superficie, sem anel
    (void)agora;
    // Dither the scroll boundary into the page, without stacked alpha strips.
    if (maisAcima)
      gfx_rect((GfxRect){0, G_L_TOPO - 8.0f, NV_TELA_W, G_L_FADE}, 0, GFX_VEU_CSS, 0, 1, 1, 0,
               NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, a);
    gfx_sem_recorte();
    // A cabeca da linha "agora": a hora numa pilula na regua.
    if (agoraVis) {
      char hora[12];
      TxtLinha t;
      GfxRect pl;
      int tf = ajustes_tinta_foco();
      gfx_cor((GfxRect){ agoraX - 1.0f, G_TOPO + 30.0f, 2.0f, G_L_TOPO - 8.0f - G_TOPO - 30.0f },
              0.0f, ar, ag, ab, 0.85f * a);
      fmtHora(agoraT, hora, sizeof hora);
      t = txt_linha(TXT_PG_ROTULO, hora, tf, tf, tf, 255);
      pl = (GfxRect){ agoraX - ((float)t.w + 20.0f) * 0.5f, G_TOPO + 1.0f,
                      (float)t.w + 20.0f, 30.0f };
      gfx_cor(pl, 0.5f, ar, ag, ab, a);
      txt_desenhar_alpha(t, pl.x + 10.0f, pl.y + (pl.h - (float)t.h) * 0.5f, a);
    }

  } else if (temLinhas) {
#ifdef NV_TOUCH_UI
    if (telefoneui_ativo())
      gfx_recorte(G_AREA_X, G_TOPO - 8.0f, G_AREA_W, G_L_BASE - G_TOPO + 8.0f);
    else
#endif
      gfx_recorte(0.0f, G_TOPO - 8.0f, NV_TELA_W, G_L_BASE - G_TOPO + 8.0f);
    for (l = 0; l < nLinhas(); l++) {
      float y = G_TOPO + (float)l * G_PASSO_Y - rolY;
      int n = linhaN(l);
      float dim = 1.0f;
      if (y > G_L_BASE || y + G_PASSO_Y < G_TOPO - 8.0f) continue;
#ifdef NV_TOUCH_UI
      { float topo = fmaxf(y + G_HEAD_H, G_TOPO), base = fminf(y + G_HEAD_H + G_CARD_H, G_L_BASE);
        toquerol_vincular(toqueXde(l), (GfxRect){ G_AREA_X, topo, G_AREA_W, fmaxf(0.0f, base - topo) }, gfx_escala(), 0.0f,
                          fmaxf(0.0f, (n - 1) * (G_CARD_W + G_GAP_X) + G_CARD_W - G_AREA_W), 0, rolXde(l)); }
#endif
      { char cab[140];
        snprintf(cab, sizeof cab, "%s  \xc2\xb7  %d", linhaNome(l), n);
        TxtLinha t = txt_linha_corta(TXT_ROW_TITULO, cab,
                                   220, 221, 224, 255,
                                   G_AREA_W);
        txt_desenhar_alpha(t, G_AREA_X, y, a * dim); }
      { float x = G_AREA_X - *rolXde(l);
        for (i = 0; i < n; i++, x += G_CARD_W + G_GAP_X) {
#ifdef NV_TOUCH_UI
          if (telefoneui_ativo()) {
            desenharCardVisivel(linhaItem(l, i), x, y + G_HEAD_H, l, i,
                               l == focoLin && i == focoCol ? 1.0f : 0.0f, a * dim, agoraT);
            continue;
          }
#endif
          if (x + G_CARD_W < 0.0f || x > NV_TELA_W) continue;
          if (a > 0.99f)
            ponteiro_alvo_faixa(x, y + G_HEAD_H, G_CARD_W, G_CARD_H, G_TOPO - 8.0f, G_L_BASE,
                              ponteiroCanal, NULL, l, i);
          desenharCard(linhaItem(l, i), x, y + G_HEAD_H,
                       l == focoLin && i == focoCol ? 1.0f : 0.0f,
                       a * dim, agoraT);
        } }
    }
    gfx_sem_recorte();

  } else if (estado == G_FALHOU || (fontesOk && !nFontes && !nCanais && estado != G_BAIXANDO)) {
    // DUAS FRASES, e a diferenca entre elas e a diferenca entre acusar a
    // pessoa e contar o que houve. `falhas` diz que alguem foi tentado e nao
    // respondeu; sem ele, a lista vazia e mesmo falta de addon.
    // DUAS PORTAS, e a mensagem tem de citar as duas. Ela dizia so "instale um
    // addon de canais" desde que o guia existe; com o portal IPTV entrando pela
    // mesma tela, quem configurou um portal e nao viu canal nenhum leria uma
    // frase que fala de outra coisa — e quem nao tem addon nem sabe que a
    // segunda porta existe.
    char msgHttp[240];
    const char *msg;
    snprintf(msgHttp, sizeof msgHttp,
             i18n("O servidor Xtream respondeu com erro HTTP %d. O guia tenta de novo a cada 10 segundos enquanto esta tela estiver aberta."),
             xtHttp);
    msg = xtFalha == XT_HTTP ? msgHttp
      : xtFalha == XT_PAGINA
      ? i18n("O servidor Xtream devolveu uma página da web em vez da lista de canais. Tente mais tarde; se continuar, fale com o provedor.")
      : xtFalha == XT_SEM_RESPOSTA
      ? i18n("A lista de canais do Xtream não respondeu agora. O guia tenta de novo a cada 10 segundos enquanto esta tela estiver aberta.")
      : xtFalha == XT_RECUSOU
      ? i18n("O Xtream recusou o usuário e a senha. Confira o cadastro em Ajustes › Conta.")
      : falhas
      ? i18n("Os addons de canais desta conta não responderam agora. O guia tenta de novo a cada 10 segundos enquanto esta tela estiver aberta.")
      : i18n("O guia se enche por dois caminhos: um addon de canais (como o FrostView TV) instalado na conta, ou um portal IPTV cadastrado em Ajustes › Conta.");
#ifdef NV_TOUCH_UI
    if (telefoneui_ativo()) {
      desenharVazioTelefone(msg, !falhas && !xtFalha, a);
    } else
#endif
    {
      float msgY = 300.0f;
    // O DESENHO SO NO CASO DE "FALTA FONTE", e nao no de "nao responderam".
    //
    // Ele explica de ONDE vem canal — resposta util para quem nao tem nenhuma
    // das duas portas, e resposta nenhuma para quem tem um addon que esta fora
    // do ar neste minuto. Ali a frase ja diz tudo, e um diagrama por cima dela
    // seria decoracao a atrapalhar a leitura.
      if (!falhas && !xtFalha) {
      // ALINHADO A MARGEM, e nao centralizado: a tela le como uma coluna,
      // alinhada ao titulo do cabecalho.
        desenharDuasPortas(G_AREA_X, 196.0f, a);
        msgY = 560.0f;
      }
      TxtLinha t = txt_linha_corta(TXT_BODY, msg, 200, 202, 210, 255,
                                   NV_TELA_W - 2 * NV_MARGEM_X);
      txt_desenhar_alpha(t, G_AREA_X, msgY, a);
    }
  }

  if (temLinhas && !gRetrato()) {
    gfx_rect((GfxRect){0, G_L_TOPO, 40, NV_TELA_H - G_L_TOPO}, 0, GFX_VEU_CSS, 0, 2, 1, 0,
             NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, a);
    gfx_rect((GfxRect){NV_TELA_W - 40, G_L_TOPO, 40, NV_TELA_H - G_L_TOPO}, 0, GFX_VEU_CSS, 0, 3, 1, 0,
             NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, a);
    gfx_rect((GfxRect){0, NV_TELA_H - 110, NV_TELA_W, 110}, 0, GFX_VEU_CSS, 0, 0, 1, 0,
             NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, a);
  }

  // (A gaveta de categorias vem depois da barra de ajuda, por cima de tudo.)

  // Barra de ajuda — a resposta ao "explicar no componente como abrir o
  // overlay": as teclas que o dono precisa lembrar ficam escritas na tela.
  // No modo lista ESQUERDA/DIREITA ganharam funcao (a janela de tempo) e a
  // barra tem de dizer. O amarelo nao aparece aqui de proposito: o controle
  // segmentado do cabecalho e o caminho que se ve, e a barra ja esta longa.
#ifdef NV_ANDROID
#define GUIA_FECHA_L i18n("OK troca de canal  ·  segure OK = favorito  ·  \xe2\x86\x90\xe2\x86\x92 adianta a grade  ·  segure \xe2\x86\x91\xe2\x86\x93 pula seção  ·  Voltar ou CH+ fecha")
#define GUIA_FECHA_G i18n("OK troca de canal  ·  segure OK = favorito  ·  segure \xe2\x86\x91\xe2\x86\x93 pula seção  ·  Voltar ou CH+ fecha")
#else
#define GUIA_FECHA_L i18n("OK troca de canal  ·  segure OK = favorito  ·  \xe2\x86\x90\xe2\x86\x92 adianta a grade  ·  segure \xe2\x86\x91\xe2\x86\x93 pula seção  ·  Voltar ou Azul fecha")
#define GUIA_FECHA_G i18n("OK troca de canal  ·  segure OK = favorito  ·  segure \xe2\x86\x91\xe2\x86\x93 pula seção  ·  Voltar ou Azul fecha")
#endif
  { const char *dica = modoLista
      ? (overlay
         ? GUIA_FECHA_L
         : i18n("OK assiste  ·  segure OK = opções  ·  \xe2\x86\x90\xe2\x86\x92 adianta a grade  ·  \xe2\x86\x91 fontes  ·  Voltar sai"))
      : (overlay
         ? GUIA_FECHA_G
         : i18n("OK assiste  ·  segure OK = opções  ·  \xe2\x86\x91 fontes  ·  segure \xe2\x86\x91\xe2\x86\x93 pula seção  ·  Voltar sai"));
    if (dicaDesde && SDL_GetTicks() - dicaDesde < G_DICA_MS)
      dica = i18n("← no primeiro canal: opções do guia");
    TxtLinha t = txt_linha_corta(TXT_CAPTION, dica, 128, 130, 138, 255,
                                 G_AREA_W);
    txt_desenhar_alpha(t, G_AREA_X, NV_TELA_H - 48.0f, a);
    // O VERDE do diagnostico da Live TV, na ponta direita quando cabe — so na
    // LG, que tem a tecla. No Android e no navegador o caminho e o botao
    // Diagnostico da barra de cima.
#if !defined(NV_ANDROID) && !defined(__EMSCRIPTEN__) && !defined(NV_TPK)
    if (!overlay) {
      TxtLinha v = txt_linha(TXT_CAPTION, i18n("Verde: diagnóstico da Live TV"), 128, 130, 138, 255);
      if ((float)t.w + 48.0f + (float)v.w <= G_AREA_W)
        txt_desenhar_alpha(v, G_AREA_X + G_AREA_W - (float)v.w, NV_TELA_H - 48.0f, a);
    }
#endif
    }

  if ((estado == G_PRONTO || estado == G_BAIXANDO) && nLtvF > 0) {
    desenharFontes(a);
    if (filtro == G_FILTRO_FAV && !nFavOrd) {
      TxtLinha t = txt_linha_corta(TXT_BODY, i18n("Nenhum favorito ainda. Segure OK num canal para marcar."),
                                   200, 202, 210, 255, G_INFO_W);
      txt_desenhar_alpha(t, G_INFO_X, G_HERO_Y + G_FONTES_H + 80.0f, a);
    }
  }
  desenharOpcoes(a);
  desenharPainelCategorias(a);
  buscaDesenhar(agora);
  if (painel) { if (paModo == 1) desenharPainelFiltros(a); else desenharPainelAddons(a); }
  desenharGrupos(a);
  desenharLimpar(a);
  // O CANAL NO AR ENCOLHENDO para o preview: o furo segue o degrau da janela
  // do player por cima de tudo, e o video "pousa" no preview.
  { float x, y, w, h;
    GCanal *c = linhaItem(focoLin, focoCol);
    if (aberta && c && !strcmp(c->id, previewId) && player_janela_animando(&x, &y, &w, &h))
      gfx_furo_raio((GfxRect){ x, y, w, h }, G_PREVIEW_RAIO / (h > 1.0f ? h : 1.0f)); }
}

// --- Spotlight (spotlight.c) ---------------------------------------------------
// Varre a lista PUBLICADA, que so o fio de desenho le — o mesmo fio que chama
// isto. Sem cache de nome normalizado: com 900 canais sao 900 normalizacoes de
// ~20 bytes por letra digitada, a mesma ordem de custo do refiltrar da busca
// (o catalogo inteiro por letra).
// A LISTA DE ONTEM SEM IR A REDE. O Spotlight so via canal com o guia
// carregado NESTA sessao (estado G_PRONTO): quem abria o app e buscava um
// canal sem ter passado pelo guia nao achava nenhum, e com a lista do cache na
// tela e a rede atras (G_BAIXANDO) tambem nao. O cache e o mesmo que o guia
// mostra NA HORA ao abrir (cacheLer); ler aqui nao dispara carga nem EPG.
void guia_preparar_busca(void) {
  if (!favLido) favLer();   // como guia_carregar: empacotar monta os favoritos
  if (!cacheLido) { cacheLido = 1; if (estado == G_PARADO && nCanais == 0) cacheLer(); }
}

int guia_buscar_canais(const char *alvoNorm, int *indices, int max) {
  int i, n = 0;
  char nome[300];
  if (!alvoNorm || !alvoNorm[0] || !indices || max <= 0) return 0;
  // `canais` e a lista PUBLICADA (so o fio de desenho escreve, em publicar e
  // cacheLer); o fio de carga mexe em sCanais. Ter canal basta.
  if (nCanais < 1) return 0;
  for (i = 0; i < nCanais && n < max; i++) {
    busca_normalizar(canais[i].nome, nome, sizeof nome);
    if (strstr(nome, alvoNorm)) indices[n++] = i;
  }
  return n;
}

int guia_canal_campos(int i, const char **id, const char **nome, const char **logo,
                      const char **cat, const char **base) {
  if (i < 0 || i >= nCanais) return 0;
  if (id) *id = canais[i].id;
  if (nome) *nome = canais[i].nome;
  if (logo) { const char *l = logoDe(&canais[i]); *logo = l ? l : ""; }
  if (cat) *cat = canais[i].cat >= 0 ? cats[canais[i].cat].nome : "";
  if (base) *base = canais[i].base;
  return 1;
}

void guia_abrir_sessao(void) {
  // Nunca tocar no staging de um fio vivo. Ele sai no proximo limite de
  // pedido e o desenho descarta o resultado pela geracao da abertura.
  cargaCancelada = 1;
  estado = G_PARADO; recarregarPend = 0; recargaDoPainel = 0;
  nCanais = nCats = nFontes = nSabe = nFav = nFavOrd = 0;
  fontesOk = cacheLido = favLido = modoLido = previewLido = 0;
  modoLista = previewLigado = 0; assinaturaPublicada = 0;
  falhas = xtFalha = xtHttp = 0;
  memset(&corte, 0, sizeof corte);
  memset(&ocultos, 0, sizeof ocultos); ocultosPerfil = -1;
  fltPerfil = -1;
  free(fIt); fIt = NULL; fN = fNivel = fSujo = paModo = 0;
  free(alts); alts = NULL; nAlts = 0;
  free(inventario); inventario = NULL; nInventario = 0;
  if (inventarioDisco) fclose(inventarioDisco);
  inventarioDisco = NULL; inventarioCortado = 0;
  fInventarioCat[0] = 0;
  focoPend[0] = pedidoBase[0] = previewId[0] = heroId[0] = 0;
  aberta = overlay = querSair = pediuCanal = 0;
  focoLin = focoCol = focoTopo = topoCol = 0;
  rolY = velY = rolL = velL = entrada = 0.0f;
  free(rolXv); rolXv = NULL; capRolX = 0;
  memset(animTopo, 0, sizeof animTopo);
  nLtvF = nVis = nZapL = 0; filtro = G_FILTRO_TODAS; filtroChave[0] = 0;
  focoFontes = fontesCol = fopAberto = gpAberto = limparConfirm = 0; perfilPrefs = -999;
  opcoesAberto = opcoesFoco = 0;
  catAberto = catFoco = dirSeg = okLongo = 0;
  catUlt = dirTick = ultNavCat = okDesde = 0;
  catAnim = catRol = catVelRol = 0.0f;
  previewPreso = pedPreview = pedPararPreview = pedRestaurar = pedGuiaCheio = 0;
  previewUltLin = previewUltCol = -1; previewFocoMudou = 0;
  memset(&pedido, 0, sizeof pedido); memset(&pedPreviewItem, 0, sizeof pedPreviewItem);
  focoAnelOk = focoAnelTem = 0; focoAnelTick = 0;
  janelaDesl = painel = paFoco = paMexeu = paLigou = paN = paErroCheio = 0;
  paErro = -1; paRol = paVelRol = 0.0f; paOkTick = 0;
  ultTentativa = overlayDesde = bandaUlt = dicaDesde = 0;
  heroA = 1.0f;
  buscaFechar();
  pedLivetvDiag = buscaEstado = buscaNC = buscaNP = buscaFoco = 0;
  buscaTexto[0] = 0; buscaRol = buscaAnim = 0.0f;
  epgVista = epg_revisao(); epgRev++;
#ifdef NV_TOUCH_UI
  toqueLimpar(); rolVazio = toqueTempoMin = toquePpm = 0.0f;
#endif
  epg_abrir_sessao(perfis_ativo()); xtepg_limpar();
  categoriasPerfil = -1; categoriasSessao = 0; fonteAtual=-1;
  mapaPerfil = -999;
  glem_abrir_sessao();
}

// Refresh retains the published source caches until replacements arrive.
void guia_recarregar(int limparLista) {
  cargaCancelada = 1; xtepg_limpar();
  if (limparLista) { guia_limpar_lista(); return; }
  IptvConta contas[IPTV_MAX_CONTAS]; int n = iptv_contas(contas, IPTV_MAX_CONTAS);
  for (int i=0;i<n;i++) if(contas[i].tipo==IPTV_M3U) m3u_fonte_recarregar(contas[i].id);
  epgExtrasMontar(); epg_recarregar();
  recarregarPend = 1; recargaDoPainel = 1; estado = G_PARADO;
}
