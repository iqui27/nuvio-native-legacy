// Guia de TV — ver guia.h para a forma geral.
//
// DE ONDE VEM O QUE:
//   - canais e categorias: o catalogo do addon de canal (FrostView), lido por
//     inteiro num fio proprio (a home so carrega 12 por fileira; o guia
//     precisa dos ~768 para agrupar por `genre`). Paginacao Stremio padrao:
//     /catalog/<tipo>/<id>.json depois /skip=100,200,...
//   - o que esta passando: epg.c (XMLTV do epgshare01, cache de 12 h).
//   - favoritos: guia-fav.txt na pasta de dados, um id por linha. O arquivo
//     proprio existe porque SalvoItem.id tem 24 bytes e os ids do FrostView
//     tem ~45 — eles nao cabem no sistema de "salvos" comum.
//
//   - modo de exibicao: guia-modo.txt na pasta de dados ("lista" ou
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
#include "guia.h"
#include "fontecache.h"
#include "ajustes.h"   /* ajustes_acento: cor do anel de foco */
#include "epg.h"
#include "rede.h"
#include "addons.h"
#include "sync.h"        /* sync_sujar_addons: ligar/desligar sobe para a conta */
#include "descoberta.h"  /* desc_repetir: addon novo so entra com ciclo novo */
#include "stalker.h"
#include "xtream.h"
#include "dados.h"
#include "perfis.h"   /* perfis_ativo: o cache do guia e por perfil */
#include "marco.h"
#include "js.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "anim.h"
#include "layout.h"
#include "idioma.h"
#include "video.h"      /* preview do canal focado no canto do guia */
#include "player.h"     /* o preview e uma sessao "mini no guia" do player */
#include "streams.h"    /* Stream: url do preview vinda do fio */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <time.h>

#define G_MAX_CANAL  900
#define G_MAX_CAT     48
#define G_MAX_FAV    400
// Cada addon pode declarar VARIOS catalogos de canal no manifesto; 8 so
// bastava para um FrostView.
#define G_MAX_FONTE   24
#define G_PAGINA     100
#define G_COTA_MIN   150   // piso da cota por catalogo (ver fioGuia)

// --- layout ---------------------------------------------------------------
// DESDE 25/09/2026 O GUIA E UM HEROI "AGORA" EM CIMA E A GRADE EMBAIXO, na
// largura inteira — a forma dos guias de referencia (YouTube TV, Google TV,
// Samsung TV Plus, TiviMate). Pedido do dono: "aumentar o video do guide,
// igual guides originais". Antes o preview era um retangulo de 320x180 numa
// coluna de 400 px a direita, e a grade perdia essa coluna inteira.
//
// A 1920x1080:
//   cabecalho  y  40..80   titulo, contagem, chips, relogio
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
#define G_HERO_Y   108.0f
// PREVIEW: 800x450 e 41,7% da largura, 16:9 exato. Os MESMOS numeros furam a
// superficie GL (desenharHero) e posicionam o plano de video (a sessao "mini
// no guia" do player recebe estes numeros por guia_preview_rect) — um lugar
// so, senao o furo e o video desencontram.
#define G_PREVIEW_W 800.0f
#define G_PREVIEW_H 450.0f
#define G_PREVIEW_X (G_AREA_DIR - G_PREVIEW_W)
#define G_PREVIEW_Y G_HERO_Y
#define G_PREVIEW_RAIO 16.0f          // px; vira fracao do menor lado no uso
// Ficha do canal a esquerda do preview, com 48 de respiro ate ele.
#define G_INFO_X   G_AREA_X
#define G_INFO_W   (G_PREVIEW_X - 48.0f - G_INFO_X)
// Topo da grade (regua do modo lista, primeira fileira do modo cartoes).
#define G_TOPO     (G_HERO_Y + G_PREVIEW_H + 22.0f)
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
// Tempos do modo salta-categoria: segurar ~600 ms entra, 2 s parado sai.
// 450 ms e a folga entre repeticoes de KEYDOWN do firmware (ele manda uma
// tecla segurada como varios KEYDOWN com repeat=0).
// 1100 e nao 600, e G_CAT_PASSO_MS: com 600 ms segurar "um pouco" ja saltava
// de secao, e uma vez no modo cada repeticao do firmware (~130 ms) pulava
// outra — dez secoes num piscar. Pedido do dono em 18/09: mais lerdo para
// entrar e mais lerdo para andar.
#define G_HOLD_MS    1100
#define G_CAT_PASSO_MS 550
#define G_REP_MS     450

// Botao AMARELO do controle da LG. SUPOSTO (ver o cabecalho do arquivo):
// SDL_webOS.h enumera RED, GREEN, YELLOW, BLUE em sequencia e BLUE e 489.
#define G_SCANCODE_YELLOW 488

// --- layout do MODO LISTA -----------------------------------------------------
// Regua de horas em G_TOPO; as linhas comecam 42 px abaixo dela. Coluna de
// canal com 400 px: numero (48) + logo numa caixa de 84x44 + nome em
// TXT_CW_TITULO (28 px). O resto, 1352 px, e a faixa de tempo.
#define G_L_TOPO    (G_TOPO + 42.0f)
// Linha de 80 (celula de 72 + 8 de vao): titulo de 28 e horario de 23 em
// duas linhas cabem com folga, e sobram CINCO canais visiveis abaixo do
// heroi — o numero dos guias de referencia com preview grande.
#define G_L_ROW      80.0f
#define G_L_CEL      72.0f
#define G_L_HEAD     44.0f     // cabecalho de categoria, discreto (22 px)
#define G_L_COL     400.0f
#define G_L_FAIXA_X (G_AREA_X + G_L_COL + 8.0f)
#define G_L_FAIXA_W (G_AREA_DIR - G_L_FAIXA_X)
// Janela de 120 min: a 1352 px isso da 11,3 px por minuto — meia hora mede
// 338 px, e um bloco de 5 min (o menor de grade de TV aberta) ainda tem 56.
#define G_L_JANELA_MIN 120
#define G_L_PASSO_MIN   30     // regua de meia em meia hora, como toda grade
#define G_L_DESL_MAX   180     // ate 3 h a frente com DIREITA
#define G_L_BASE    (NV_TELA_H - 62.0f)   // acima da barra de ajuda
#define G_L_FADE     36.0f     // esmaecimento nas bordas da area rolavel

// --- cabecalho: chips de modo, addons e preview -----------------------------
// CHIPS e nao pilulas: 40 px de altura e 22 px de fonte, contra os 48/25 de
// antes. O dono (21/09): "os botoes estao muito grandes perto do resto". A
// linha do cabecalho e navegacao secundaria; o que pesa na tela e o heroi.
#define G_TOPO_Y     40.0f
#define G_TOPO_H     40.0f
#define G_CHIP_PAD   20.0f
// CATEGORIAS e o primeiro chip: a porta VISIVEL do painel de categorias,
// que antes so abria segurando a seta (ninguem descobre gesto que nao se ve).
enum { G_TOPO_CATEGORIAS = 0, G_TOPO_CARTOES, G_TOPO_LISTA, G_TOPO_ADDONS,
       G_TOPO_PREVIEW, G_TOPO_N };

// --- painel de addons ---------------------------------------------------------
#define G_PA_W      720.0f
#define G_PA_X      (NV_TELA_W - G_PA_W)
#define G_PA_MARG    48.0f
#define G_PA_ROW     92.0f
// Sugestao tem descricao de DUAS linhas (a de uma linha cortava toda frase
// em "…", foto do dono em 19/09): a linha e mais alta.
#define G_PA_ROW_REC 118.0f
#define G_MAX_REC    12

typedef struct {
  char id[80];
  char nome[140];
  char logo[480];
  char desc[600];
  // DE QUE ADDON ESTE CANAL VEIO. Sem isto o player perguntava a fonte a TODOS
  // os addons — e um addon quebrado (FrostView com 408 o dia inteiro) segurava
  // o canal de OUTRO addon ate o timeout dele. Medido na C9 em 18/09: com o
  // FrostView desligado, 1,6 s da tecla ate a fonte; ligado, dezenas de
  // segundos e "nao foi possivel abrir a fonte" num canal que ele nem fornece.
  char base[600];
  int  cat;      // indice em cats[]
  int  epg;      // -1 = ainda nao resolvido; -2 = sem grade real
  int  fav;      // espelho do arquivo, para o desenho nao varrer a lista
} GCanal;

// Publicados (so o fio de desenho le).
static GCanal canais[G_MAX_CANAL];   static int nCanais;
static char   cats[G_MAX_CAT][64];   static int nCats;
static int    catIni[G_MAX_CAT], catN[G_MAX_CAT];
// Staging (so o fio de carga escreve).
static GCanal sCanais[G_MAX_CANAL];  static int sNCanais;
static char   sCats[G_MAX_CAT][64];  static int sNCats;

enum { G_PARADO, G_BAIXANDO, G_PRONTO, G_FALHOU };
static int estado = G_PARADO;
static int fioVivo, pendPronto;
// Foco pedido com a lista ainda baixando; publicar() aplica ao chegar.
static char focoPend[80];

// Catalogos de canal descobertos na home (tipo "channel"/"tv").
// `nome` e o nome do catalogo no manifesto (ou o titulo da fileira): vira a
// categoria do canal que nao traz genero nenhum — ver lerPagina.
typedef struct { char base[600], tipo[16], id[96], nome[96]; } GFonte;
static GFonte fontes[G_MAX_FONTE]; static int nFontes, fontesOk;
// Copia de trabalho do fio: as fileiras sao so o caminho rapido. O manifesto
// de cada addon ativo declara TODOS os catalogos de canal, montados na home
// ou nao — sem a sonda o guia ficava limitado ao que coube nas fileiras.
static GFonte sFontes[G_MAX_FONTE]; static int sNFontes;

// --- favoritos --------------------------------------------------------------
static char fav[G_MAX_FAV][80]; static int nFav, favLido;
static int  favOrd[G_MAX_FAV];  static int nFavOrd;   // indices em canais[]

static int favIndice(const char *id) {
  for (int i = 0; i < nFav; i++) if (!strcmp(fav[i], id)) return i;
  return -1;
}

static void favLer(void) {
  char *t = dados_ler("guia-fav.txt"), *p, *q;
  if (!t) { favLido = 1; return; }
  p = t;
  while (*p && nFav < G_MAX_FAV) {
    while (*p == '\n' || *p == '\r' || *p == ' ') p++;
    if (!*p) break;
    q = p; while (*q && *q != '\n' && *q != '\r') q++;
    if (q - p < 80) { memcpy(fav[nFav], p, (size_t)(q - p)); fav[nFav][q - p] = 0; nFav++; }
    p = q;
  }
  free(t);
  favLido = 1;
}

static void favGravar(void) {
  // O arquivo e pequeno (um id por linha); montar em memoria e gravar de uma
  // vez com dados_gravar, que ja e atomico.
  char buf[G_MAX_FAV * 82]; size_t n = 0;
  for (int i = 0; i < nFav && n + 82 < sizeof buf; i++)
    n += (size_t)snprintf(buf + n, sizeof buf - n, "%s\n", fav[i]);
  buf[n] = 0;
  dados_gravar("guia-fav.txt", buf);
}

// Marca o espelho `fav` de cada canal. Chamado depois de publicar a lista e a
// cada alternancia.
static void favAplicar(void) {
  for (int i = 0; i < nCanais; i++)
    canais[i].fav = favIndice(canais[i].id) >= 0;
}

static void favAlternar(GCanal *c) {
  int i = favIndice(c->id);
  if (i >= 0) { memmove(fav[i], fav[i + 1], (size_t)(nFav - i - 1) * 80); nFav--; c->fav = 0; }
  else if (nFav < G_MAX_FAV) { snprintf(fav[nFav++], 80, "%s", c->id); c->fav = 1; }
  favGravar();
  // A fileira "Favoritos" muda de tamanho aqui — remontar as janelas.
  nFavOrd = 0;
  for (i = 0; i < nCanais && nFavOrd < G_MAX_FAV; i++)
    if (canais[i].fav) favOrd[nFavOrd++] = i;
}

// --- modo de exibicao ------------------------------------------------------------
// 1 = lista, 0 = cartoes. Persistido em guia-modo.txt: quem escolheu lista
// espera reabrir em lista, senao a escolha vira um gesto a repetir por sessao.
static int modoLista, modoLido;

static void modoLer(void) {
  char *t = dados_ler("guia-modo.txt");
  modoLido = 1;
  if (!t) return;
  modoLista = !strncmp(t, "lista", 5);
  free(t);
}

static void modoGravar(void) {
  dados_gravar("guia-modo.txt", modoLista ? "lista\n" : "cartoes\n");
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
  char *t = dados_ler("guia-preview.txt");
  previewLido = 1;
  if (!t) return;
  previewLigado = (t[0] == '1');
  free(t);
}
static void previewGravar(void) {
  dados_gravar("guia-preview.txt", previewLigado ? "1\n" : "0\n");
}

// --- o que cada addon da conta declara -----------------------------------------
// "Este addon fornece canal?" e uma pergunta que so o manifesto responde, e a
// sonda deste guia ja o baixa. Guardar a resposta POR BASE e o que permite ao
// painel de addons dizer "fornece canais" / "sem catalogo de canais" em vez
// de listar os dezesseis addons da conta sem distinguir nada. -1 = manifesto
// nao respondeu (nao e "nao fornece": e "nao se sabe").
typedef struct { char base[600]; int canal; } GSabe;
#define G_MAX_SABE 16
static GSabe sSabe[G_MAX_SABE]; static int sNSabe;   // do fio
static GSabe sabe[G_MAX_SABE];  static int nSabe;    // publicado

static int sabeCanal(const char *base) {   // 1, 0, ou -1 = desconhecido
  for (int i = 0; i < nSabe; i++)
    if (!strcmp(sabe[i].base, base)) return sabe[i].canal;
  return -1;
}

// A base pertence a um addon da conta que esta DESLIGADO? Base que nenhum
// addon reivindica (portal Stalker, addon recem-removido) conta como ligada:
// o painel so manda no que ele lista.
static int baseLigada(const char *base) {
  for (int i = 0; i < addons_n(); i++) {
    const char *b = addons_base(i);
    if (b && b[0] && !strcmp(b, base)) return addons_ativo(i);
  }
  return 1;
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
typedef struct { char nome[64]; char url[600]; char desc[200]; char descEn[200]; } GRec;
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
  char linha[1000];
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

// A mesma normalizacao de baseNormalizada em addons.c (que e estatica la):
// sem query, sem /manifest.json, sem barra final. E o que faz "instalado"
// bater com addons_base(i) para a URL que o dono escreveu no arquivo.
static void baseDaUrl(const char *url, char *dst, size_t tam) {
  size_t k; char *q;
  snprintf(dst, tam, "%s", url);
  q = strchr(dst, '?'); if (q) *q = 0;
  k = strlen(dst);
  if (k > 14 && !strcmp(dst + k - 14, "/manifest.json")) { k -= 14; dst[k] = 0; }
  while (k && dst[k - 1] == '/') dst[--k] = 0;
}

static int recInstalado(const GRec *r) {
  char base[600];
  baseDaUrl(r->url, base, sizeof base);
  for (int i = 0; i < addons_n(); i++)
    if (!strcmp(addons_base(i), base)) return 1;
  return 0;
}

// --- linhas -------------------------------------------------------------------
// Linha 0 = Favoritos (quando ha algum); as demais sao as categorias com canal.
static int nLinhas(void) { return nCats + (nFavOrd > 0 ? 1 : 0); }
static int linhaEhFav(int l) { return nFavOrd > 0 && l == 0; }
static int linhaN(int l) {
  if (l < 0 || l >= nLinhas()) return 0;
  return linhaEhFav(l) ? nFavOrd : catN[l - (nFavOrd > 0 ? 1 : 0)];
}
static const char *linhaNome(int l) {
  if (linhaEhFav(l)) return i18n("Favoritos");
  // As secoes de categoriaPorNome sao chaves da tabela; genero vindo do
  // addon nao e, e i18n devolve o texto como veio.
  return i18n(cats[l - (nFavOrd > 0 ? 1 : 0)]);
}
static GCanal *linhaItem(int l, int c) {
  if (c < 0 || c >= linhaN(l)) return NULL;
  if (linhaEhFav(l)) return &canais[favOrd[c]];
  return &canais[catIni[l - (nFavOrd > 0 ? 1 : 0)] + c];
}
static int linhaCatDe(int l) {  // indice em cats[], -1 = favoritos
  return linhaEhFav(l) ? -1 : l - (nFavOrd > 0 ? 1 : 0);
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

static int sCatDe(const char *nome) {
  for (int i = 0; i < sNCats; i++) if (!strcmp(sCats[i], nome)) return i;
  if (sNCats >= G_MAX_CAT) return -1;
  snprintf(sCats[sNCats], 64, "%s", nome);
  return sNCats++;
}

// Primeiro elemento de texto de um array JSON ("genre":["X"] -> "X").
static int lerStrEl(const char *p, char *dst, int tam) {
  int n = 0;
  if (!p || *p != '"') return 0;
  p++;
  while (*p && *p != '"' && n < tam - 1) {
    if (*p == '\\' && p[1]) p++;
    dst[n++] = *p++;
  }
  dst[n] = 0;
  return n > 0;
}

static int sCanalPorId(const char *id) {
  for (int i = 0; i < sNCanais; i++) if (!strcmp(sCanais[i].id, id)) return i;
  return -1;
}

// Uma pagina de catalogo. Devolve quantos METAS a pagina trouxe — nao quantos
// entraram: uma fonte pode repetir canais de outra, e contar so os novos
// encerraria a pagina uma rodada cedo, deixando os exclusivos de tras.
// `teto` e o maximo de canais que ESTE catalogo pode somar a sCanais (ver o
// laco em fioGuia). Devolve quantos metas a pagina trouxe; `novos` recebe
// quantos entraram de fato — a diferenca e o que separa "acabou" de
// "catalogo que ignora skip e repete a mesma pagina".
static int lerPagina(const GFonte *f, int skip, int teto, int *novos) {
  char url[1200];
  char *corpo;
  const char *p;
  int n = 0;
  if (skip > 0)
    snprintf(url, sizeof url, "%s/catalog/%s/%s/skip=%d.json",
             f->base, f->tipo, f->id, skip);
  else
    snprintf(url, sizeof url, "%s/catalog/%s/%s.json", f->base, f->tipo, f->id);
  *novos = 0;
  corpo = rede_baixar(url, 15);
  if (!corpo) return 0;
  p = js_array(corpo, NULL, "metas");
  while (p && sNCanais < G_MAX_CANAL && *novos < teto) {
    const char *fim = js_fim(p);
    GCanal c;
    char gen[64] = "";
    memset(&c, 0, sizeof c);
    c.epg = -1; c.cat = -1;
    n++;
    if (js_texto(p, fim, "id", c.id, sizeof c.id) &&
        js_texto(p, fim, "name", c.nome, sizeof c.nome) &&
        sCanalPorId(c.id) < 0) {
      char mtipo[16] = "";
      // O catalogo se declara de canal, mas um meta que se diz filme/serie
      // nao entra — protege o guia de catalogo mal declarado.
      if (js_texto(p, fim, "type", mtipo, sizeof mtipo) &&
          mtipo[0] && !ehCanal(mtipo)) { p = js_prox(fim); continue; }
      if (!js_texto(p, fim, "poster", c.logo, sizeof c.logo))
        js_texto(p, fim, "logo", c.logo, sizeof c.logo);
      js_texto(p, fim, "description", c.desc, sizeof c.desc);
      { const char *g = js_array(p, fim, "genre");
        if (!g) g = js_array(p, fim, "genres");
        lerStrEl(g, gen, sizeof gen); }
      // SEM GENERO: primeiro o nome do canal (categoriaPorNome), depois o
      // nome do catalogo de onde ele veio ("Meu Futebol"), que e a
      // classificacao que o proprio addon deu. Meu Futebol manda 68 canais
      // sem `genres` nenhum (medido em 18/09/2026) e todos caiam em "Outros"
      // — que do sofa se le como "o app nao classificou". "Outros" fica so
      // para catalogo sem nome, que nao devia existir.
      if (!gen[0]) {
        const char *h = categoriaPorNome(c.nome);
        if (h) snprintf(gen, sizeof gen, "%s", h);
      }
      c.cat = sCatDe(gen[0] ? gen : f->nome[0] ? f->nome : "Outros");
      snprintf(c.base, sizeof c.base, "%s", f->base);
      if (c.cat >= 0) { sCanais[sNCanais++] = c; (*novos)++; }
    }
    p = js_prox(fim);
  }
  free(corpo);
  return n;
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

static void sondaManifestos(void) {
  int a;
  sNSabe = 0;
  for (a = 0; a < addons_n() && sNFontes < G_MAX_FONTE; a++) {
    const char *base;
    char url[700];
    char *corpo;
    const char *p, *fim;
    int ativo = addons_ativo(a), temCanal = 0;
    GSabe *sb = sNSabe < G_MAX_SABE ? &sSabe[sNSabe] : NULL;
    base = addons_base(a);
    if (!base || !base[0]) continue;
    if (sb) { snprintf(sb->base, sizeof sb->base, "%s", base); sb->canal = -1; sNSabe++; }
    // O MANIFESTO JA FOI LIDO PELA DESCOBERTA, no arranque e em paralelo: o
    // que ele declara de canal esta em addons.c. Reler aqui era um GET por
    // addon, em serie, com 15 s de prazo cada — a maior parte da espera para o
    // guia abrir numa conta com 10-20 addons (a maioria sem canal nenhum).
    // So cai na rede quando aquele manifesto ainda nao chegou.
    { AddCatCanal cc[ADD_CANAL_MAX];
      int nc = addons_catalogos_canal(a, cc, ADD_CANAL_MAX), k;
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
    snprintf(url, sizeof url, "%s/manifest.json", base);
    corpo = rede_baixar(url, 15);
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

static void *fioGuia(void *u) {
  int ok = 0;
  (void)u;
  sNCanais = 0; sNCats = 0; sFalhas = 0; sXtFalha = XT_OK;
  // Fontes das fileiras (descobertas no fio de desenho) primeiro — zero rede
  // extra. A sonda de manifestos completa com o que a home nao montou.
  // ADDON DESLIGADO NAO ENTRA POR AQUI TAMBEM. As fileiras da home so sao
  // refeitas pelo desc_repetir (~20 s), entao logo depois de desligar um addon
  // no painel elas ainda listam o catalogo dele — e sem esta guarda a recarga
  // "na hora" do painel trazia de volta exatamente o que a pessoa acabou de
  // tirar.
  sNFontes = 0;
  for (int i = 0; i < nFontes && sNFontes < G_MAX_FONTE; i++)
    if (baseLigada(fontes[i].base)) sFontes[sNFontes++] = fontes[i];
  sondaManifestos();
  // COTA POR CATALOGO. O Pluto TV devolve 1.523 canais numa pagina so;
  // instalado, ele enchia os 900 lugares sozinho e o Meu Futebol (68 canais,
  // o que a pessoa realmente assiste) nem era lido — "instalei um addon e o
  // meu sumiu". MEDIDO na C9 em 18/09. A cota divide o teto entre os
  // catalogos que ha, com um piso para o catalogo grande nao virar amostra
  // quando sao muitos; catalogo pequeno nao usa a cota e sobra para os outros
  // porque a conta e sobre o que ainda cabe, catalogo a catalogo.
  for (int i = 0; i < sNFontes; i++) {
    int restantes = sNFontes - i;
    int cota = (G_MAX_CANAL - sNCanais) / (restantes > 0 ? restantes : 1);
    int lidos = 0;
    if (cota < G_COTA_MIN) cota = G_COTA_MIN;
    for (int skip = 0; sNCanais < G_MAX_CANAL && lidos < cota; skip += G_PAGINA) {
      int novos = 0;
      int n = lerPagina(&sFontes[i], skip, cota - lidos, &novos);
      if (n <= 0) break;
      ok = 1;
      lidos += novos;
      // Pagina cheia que nao trouxe nada novo e catalogo que ignora `skip`:
      // pedir a proxima e receber a mesma de novo, para sempre.
      if (n < G_PAGINA || novos == 0) break;
    }
  }
  // PORTAL STALKER, quando houver um configurado neste perfil. Entra DEPOIS
  // dos addons de proposito: quem tem as duas coisas espera ver primeiro o que
  // ja via, e a ordem das categorias no guia e a ordem de chegada.
  //
  // Nao mexe em sFalhas: portal ausente nao e falha, e portal que nao responde
  // ja se anuncia pela ausencia das categorias dele. Somar aqui faria a
  // mensagem "nenhum catalogo de canais" aparecer para quem tem addons bons e
  // um portal errado.
  if (stalker_configurado()) {
    // MALLOC e nao `static`: sao ~670 KB que so servem durante a carga. Como
    // vetor estatico eles ficariam residentes para sempre, inclusive em quem
    // nunca configurou portal nenhum — e este app ja disputa memoria com o
    // cache de texturas numa TV de 2016.
    StalkerCanal *st = malloc(sizeof *st * G_MAX_CANAL);
    int n = st ? stalker_canais(st, G_MAX_CANAL) : 0, i;
    for (i = 0; i < n && sNCanais < G_MAX_CANAL; i++) {
      GCanal c;
      if (sCanalPorId(st[i].id) >= 0) continue;
      memset(&c, 0, sizeof c);
      c.epg = -1;
      snprintf(c.id,   sizeof c.id,   "%s", st[i].id);
      snprintf(c.nome, sizeof c.nome, "%s", st[i].nome);
      snprintf(c.logo, sizeof c.logo, "%s", st[i].logo);
      c.cat = sCatDe(st[i].categoria[0] ? st[i].categoria : "Outros");
      if (c.cat >= 0) { sCanais[sNCanais++] = c; ok = 1; }
    }
    free(st);
  }
  // XTREAM, pela mesma porta e pelas mesmas razoes (ver xtream.h). O epg_id
  // que o servidor manda nao entra: o EPG do guia casa por nome, e o id do
  // Xtream e do XMLTV do proprio provedor, que o app nao baixa.
  if (xtream_configurado()) {
    XtreamCanal *xt = malloc(sizeof *xt * G_MAX_CANAL);
    int n = xt ? xtream_canais(xt, G_MAX_CANAL) : 0, i;
    if (xt) sXtFalha = xtream_ultima_falha();
    for (i = 0; i < n && sNCanais < G_MAX_CANAL; i++) {
      GCanal c;
      if (sCanalPorId(xt[i].id) >= 0) continue;
      memset(&c, 0, sizeof c);
      c.epg = -1;
      snprintf(c.id,   sizeof c.id,   "%s", xt[i].id);
      snprintf(c.nome, sizeof c.nome, "%s", xt[i].nome);
      snprintf(c.logo, sizeof c.logo, "%s", xt[i].logo);
      c.cat = sCatDe(xt[i].categoria[0] ? xt[i].categoria : "Outros");
      if (c.cat >= 0) { sCanais[sNCanais++] = c; ok = 1; }
    }
    free(xt);
  }
  // FONTE ACHADA E NENHUMA PAGINA RESPONDEU tambem e "nao respondeu", e nao
  // "nao existe": e o caso do catalogo de canais que estoura o prazo com o
  // manifesto tendo vindo 200.
  if (sNFontes > 0 && !ok) sFalhas++;
  pendPronto = 1;
  if (!ok) estado = G_FALHOU;
  return NULL;
}

// Ordena os canais por categoria (estavel na ordem de chegada) para que cada
// fileira seja uma janela contigua — o mesmo desenho de CatFileira — e refaz
// a fileira de favoritos. Roda ao publicar uma carga e ao desligar um addon no
// painel (que tira canais de `canais[]` sem esperar o fio).
static void empacotar(void) {
  int i, w;
  // Canal de addon desligado nao fica. Cobre a janela em que o painel desliga
  // um addon com o fio ja no ar: a carga que estava a caminho foi montada com
  // ele ligado e chegaria inteira; a recarga seguinte (recarregarPend) e que
  // corrige a lista de fontes, mas a tela nao precisa esperar por ela.
  for (i = 0, w = 0; i < nCanais; i++)
    if (!canais[i].base[0] || baseLigada(canais[i].base)) canais[w++] = canais[i];
  nCanais = w;
  // Categoria que ficou sem canal sai da lista — senao vira uma fileira so
  // de cabecalho, com o foco caindo nela.
  { int mapa[G_MAX_CAT], k;
    for (i = 0; i < nCats; i++) catN[i] = 0;
    for (i = 0; i < nCanais; i++) if (canais[i].cat >= 0) catN[canais[i].cat]++;
    for (i = 0, k = 0; i < nCats; i++) {
      mapa[i] = catN[i] ? k : -1;
      if (catN[i]) { if (k != i) memcpy(cats[k], cats[i], sizeof cats[k]); k++; }
    }
    for (i = 0; i < nCanais; i++) if (canais[i].cat >= 0) canais[i].cat = mapa[canais[i].cat];
    nCats = k; }
  // ORDEM ALFABETICA DAS SECOES, pedido do dono (18/09): a ordem de chegada
  // muda conforme qual addon respondeu primeiro, e "segurar para pular
  // secao" sem uma ordem previsivel e pular no escuro. Insercao: sao ate 48
  // nomes, uma vez por carga.
  { int a, b, mapa[G_MAX_CAT];
    for (a = 0; a < nCats; a++) mapa[a] = a;
    for (a = 1; a < nCats; a++) {
      char tmpN[64]; int tmpM = mapa[a];
      memcpy(tmpN, cats[a], sizeof tmpN);
      for (b = a - 1; b >= 0 && strcasecmp(cats[b], tmpN) > 0; b--) {
        memcpy(cats[b + 1], cats[b], sizeof cats[b]); mapa[b + 1] = mapa[b];
      }
      memcpy(cats[b + 1], tmpN, sizeof tmpN); mapa[b + 1] = tmpM;
    }
    // mapa[nova] = antiga; o canal precisa do inverso.
    { int inv[G_MAX_CAT];
      for (a = 0; a < nCats; a++) inv[mapa[a]] = a;
      for (i = 0; i < nCanais; i++) if (canais[i].cat >= 0) canais[i].cat = inv[canais[i].cat]; } }
  { GCanal tmp[G_MAX_CANAL];
    int pos[G_MAX_CAT];
    for (i = 0; i < nCats; i++) catN[i] = 0;
    for (i = 0; i < nCanais; i++) if (canais[i].cat >= 0) catN[canais[i].cat]++;
    catIni[0] = 0;
    for (i = 1; i < nCats; i++) catIni[i] = catIni[i - 1] + catN[i - 1];
    memcpy(pos, catIni, sizeof pos);
    for (i = 0; i < nCanais; i++)
      if (canais[i].cat >= 0) tmp[pos[canais[i].cat]++] = canais[i];
    memcpy(canais, tmp, sizeof(GCanal) * (size_t)nCanais); }
  favAplicar();
  nFavOrd = 0;
  for (i = 0; i < nCanais && nFavOrd < G_MAX_FAV; i++)
    if (canais[i].fav) favOrd[nFavOrd++] = i;
}


// --- CACHE DO GUIA EM DISCO ---------------------------------------------------
//
// POR QUE: o guia so existia depois de UMA volta completa pela rede — um GET
// por catalogo de canal (o Pluto responde 1.500 canais numa pagina), mais
// Stalker/Xtream — a cada arranque do app. "Sempre demora muito" (dono,
// 19/09). A lista muda pouco: os canais de ontem sao os de hoje. Entao a
// ultima lista publicada fica em disco e e o que a tela mostra NA HORA; a
// volta pela rede continua, em fundo, e so republica se a lista mudou — a
// mesma regra da home (cat_assinatura).
//
// O arquivo e por perfil (os addons sao por perfil) e carrega o tamanho das
// structs no cabecalho: uma build que mude GCanal ignora o cache antigo em
// vez de ler lixo.
#define GC_MAGIA  0x31474e56u   /* "NVG1" */
#define GC_VERSAO 1u
typedef struct {
  unsigned magia, versao, tamCanal, tamFonte, tamSabe;
  int nCanais, nCats, nFontes, nSabe, perfil;
} GCacheCab;
static int cacheLido;          // ja tentou ler nesta sessao
static unsigned long assinaturaPublicada;

static const char *cacheNome(void) {
  static char nome[48];
  int p = perfis_ativo();
  if (p <= 0) snprintf(nome, sizeof nome, "guia-cache.bin");
  else        snprintf(nome, sizeof nome, "guia-cache-p%d.bin", p);
  return nome;
}

// Assinatura INDEPENDENTE DA ORDEM (soma de hashes por canal): a copia de
// trabalho chega na ordem de chegada e a publicada sai ordenada por
// categoria, e as duas precisam bater quando o conteudo e o mesmo.
static unsigned long assinaturaDe(const GCanal *c, int n, char cs[][64]) {
  unsigned long soma = 0;
  int i;
  for (i = 0; i < n; i++) {
    unsigned long h = 2166136261UL;
    const char *p;
    for (p = c[i].id; *p; p++) { h ^= (unsigned char)*p; h *= 16777619UL; }
    if (c[i].cat >= 0) for (p = cs[c[i].cat]; *p; p++) { h ^= (unsigned char)*p; h *= 16777619UL; }
    soma += h;
  }
  return soma ^ (unsigned long)n;
}

static void cacheGravar(void) {
  char caminho[600], tmp[620];
  GCacheCab cab;
  FILE *f;
  if (!dados_caminho(caminho, sizeof caminho, cacheNome())) return;
  snprintf(tmp, sizeof tmp, "%s.tmp", caminho);
  memset(&cab, 0, sizeof cab);
  cab.magia = GC_MAGIA; cab.versao = GC_VERSAO;
  cab.tamCanal = (unsigned)sizeof(GCanal); cab.tamFonte = (unsigned)sizeof(GFonte);
  cab.tamSabe = (unsigned)sizeof(GSabe);
  cab.nCanais = nCanais; cab.nCats = nCats; cab.nFontes = nFontes; cab.nSabe = nSabe;
  cab.perfil = perfis_ativo();
#ifdef __EMSCRIPTEN__
  dados_fs_travar();
#endif
  f = fopen(tmp, "wb");
  if (f) {
    int ok = fwrite(&cab, sizeof cab, 1, f) == 1 &&
             fwrite(canais, sizeof(GCanal), (size_t)nCanais, f) == (size_t)nCanais &&
             fwrite(cats, sizeof cats[0], (size_t)nCats, f) == (size_t)nCats &&
             fwrite(fontes, sizeof(GFonte), (size_t)nFontes, f) == (size_t)nFontes &&
             fwrite(sabe, sizeof(GSabe), (size_t)nSabe, f) == (size_t)nSabe;
    ok = (fclose(f) == 0) && ok;
    if (!ok || rename(tmp, caminho) != 0) remove(tmp);
    else { printf("[guia] cache gravado: %d canais\n", nCanais); fflush(stdout); }
  }
#ifdef __EMSCRIPTEN__
  dados_fs_liberar();
  dados_marcar_sujo(0);
#endif
}

// Le o cache direto para os vetores PUBLICADOS (roda no fio de desenho, antes
// de qualquer fio de carga existir). 1 se a tela ja tem lista.
static int cacheLer(void) {
  char caminho[600];
  GCacheCab cab;
  FILE *f;
  int ok;
  if (!dados_caminho(caminho, sizeof caminho, cacheNome())) return 0;
  f = fopen(caminho, "rb");
  if (!f) return 0;
  ok = fread(&cab, sizeof cab, 1, f) == 1 &&
       cab.magia == GC_MAGIA && cab.versao == GC_VERSAO &&
       cab.tamCanal == sizeof(GCanal) && cab.tamFonte == sizeof(GFonte) &&
       cab.tamSabe == sizeof(GSabe) && cab.perfil == perfis_ativo() &&
       cab.nCanais > 0 && cab.nCanais <= G_MAX_CANAL && cab.nCats > 0 && cab.nCats <= G_MAX_CAT &&
       cab.nFontes >= 0 && cab.nFontes <= G_MAX_FONTE && cab.nSabe >= 0 && cab.nSabe <= G_MAX_SABE;
  if (ok) ok = fread(canais, sizeof(GCanal), (size_t)cab.nCanais, f) == (size_t)cab.nCanais &&
               fread(cats, sizeof cats[0], (size_t)cab.nCats, f) == (size_t)cab.nCats &&
               fread(fontes, sizeof(GFonte), (size_t)cab.nFontes, f) == (size_t)cab.nFontes &&
               fread(sabe, sizeof(GSabe), (size_t)cab.nSabe, f) == (size_t)cab.nSabe;
  fclose(f);
  if (!ok) { nCanais = nCats = 0; return 0; }
  nCanais = cab.nCanais; nCats = cab.nCats; nFontes = cab.nFontes; nSabe = cab.nSabe;
  // Strings vindas do disco terminam em NUL por conta propria.
  { int i;
    for (i = 0; i < nCanais; i++) { canais[i].id[sizeof canais[i].id - 1] = 0; canais[i].nome[sizeof canais[i].nome - 1] = 0;
      canais[i].logo[sizeof canais[i].logo - 1] = 0; canais[i].base[sizeof canais[i].base - 1] = 0;
      canais[i].epg = -1;
      if (canais[i].cat < -1 || canais[i].cat >= nCats) canais[i].cat = -1; }
    for (i = 0; i < nCats; i++) cats[i][sizeof cats[i] - 1] = 0; }
  empacotar();
  assinaturaPublicada = assinaturaDe(canais, nCanais, cats);
  printf("[guia] cache lido: %d canais em %d categorias\n", nCanais, nCats);
  fflush(stdout);
  marco("guia: lista do cache na tela");
  return 1;
}

static void publicar(void) {
  int i, w;
  // A MESMA LISTA QUE JA ESTA NA TELA (vinda do cache) nao e republicada:
  // trocar o vetor reordena e zera foco e rolagem para mostrar o que ja se
  // mostrava. So o veredito da sonda (`sabe`, para o painel) e as falhas
  // atualizam.
  // REDE VAZIA NAO APAGA A LISTA QUE HA: sem resposta nenhuma (todos os addons
  // fora), a lista de ontem continua valendo mais que uma tela vazia.
  if (sNCanais == 0 && nCanais > 0) {
    falhas = sFalhas; xtFalha = sXtFalha;
    memcpy(sabe, sSabe, sizeof sSabe); nSabe = sNSabe;
    printf("[guia] rede sem canal nenhum: fica a lista que estava\n");
    fflush(stdout);
    return;
  }
  { unsigned long nova = assinaturaDe(sCanais, sNCanais, sCats);
    if (nCanais > 0 && nova == assinaturaPublicada) {
      memcpy(sabe, sSabe, sizeof sSabe); nSabe = sNSabe;
      memcpy(fontes, sFontes, sizeof sFontes); nFontes = sNFontes;
      falhas = sFalhas; xtFalha = sXtFalha;
      printf("[guia] lista da rede igual a da tela: nao republicada\n");
      fflush(stdout);
      marco("guia: rede igual ao cache");
      return;
    }
    assinaturaPublicada = nova; }
  memcpy(canais, sCanais, sizeof(GCanal) * (size_t)sNCanais);
  memcpy(cats, sCats, sizeof sCats);
  nCanais = sNCanais; nCats = sNCats;
  // As fontes efetivas sao as do fio — fileiras + manifestos. Sem a copia, a
  // mensagem "Nenhum catalogo de canais" e o loop de re-tentativa liam a
  // contagem das fileiras apenas.
  memcpy(fontes, sFontes, sizeof sFontes);
  nFontes = sNFontes;
  falhas = sFalhas; xtFalha = sXtFalha;
  memcpy(sabe, sSabe, sizeof sSabe);
  nSabe = sNSabe;
  (void)w;
  empacotar();
  // Foco pedido com a lista vazia (o overlay aberto pelo player, por exemplo)
  // so encontra o canal agora.
  if (focoPend[0]) {
    char id[80];
    snprintf(id, sizeof id, "%s", focoPend);
    focoPend[0] = 0;
    guia_focar_id(id);
  }
  printf("[guia] %d canais em %d categorias (%d favoritos)\n",
         nCanais, nCats, nFavOrd);
  fflush(stdout);
  marco("guia: lista da rede publicada");
  if (nCanais > 0) cacheGravar();
}

static void iniciarCarga(void) {
  pthread_t t;
  // Sem portaria por nFontes: a sonda de manifestos dentro do fio pode achar
  // catalogo de canal que nenhuma fileira montou.
  if (fioVivo) return;
  fioVivo = 1;
  estado = G_BAIXANDO;
  if (pthread_create(&t, NULL, fioGuia, NULL) != 0) { fioVivo = 0; estado = G_FALHOU; }
  else pthread_detach(t);
}

// A parte "carregar" da abertura, sem a parte "mostrar". O zap por CH+/-
// funciona com o guia nunca aberto: o primeiro toque dispara a carga e nao
// troca nada (a lista nao existe ainda), o seguinte ja troca.
void guia_carregar(void) {
  if (!favLido) favLer();
  if (!modoLido) modoLer();
  if (!previewLido) previewLer();
  // Sem fonte achada, tenta de novo a cada chamada: a descoberta da home pode
  // nao ter montado as fileiras ainda quando o primeiro CH+/- chega.
  if (!fontesOk) descobrirFontes();
  // A lista de ontem NA HORA; a de hoje vem atras. Ver o bloco do cache.
  if (!cacheLido) { cacheLido = 1; if (estado == G_PARADO && nCanais == 0) cacheLer(); }
  if (estado == G_PARADO || estado == G_FALHOU) iniciarCarga();
  epg_iniciar();
}

// --- estado de tela -------------------------------------------------------------
static int   aberta, overlay, querSair;
static int   focoLin, focoCol;
static float rolY, velY;
static float rolX[G_MAX_CAT + 1];     // por linha (0 = favoritos)
static float entrada;
static int   pediuCanal; static CatItem pedido;
// PAINEL DE CATEGORIAS (gaveta da esquerda). 0 = fechado; 1 = aberto por
// SEGURAR ↑↓ (a soltura entra na categoria escolhida); 2 = aberto pelo chip
// "Categorias" (fica aberto, OK entra, Voltar fecha). catFoco e a linha em
// foco DENTRO do painel — o guia so muda de secao ao confirmar.
static int    catAberto, catFoco;
static float  catAnim, catRol, catVelRol;
static int    dirSeg;                 // SDLK_UP/DOWN segurado, 0 = solto
static Uint32 dirDesde, dirTick, ultNavCat;

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
// 1 quando nesta visita alguem LIGOU um addon (ou instalou um novo). Ligar so
// traz catalogo baixando da rede, entao o fechar pede desc_repetir() — o ciclo
// completo. Desligar nao precisa de rede: as fileiras do addon saem por filtro
// (desligada() em descoberta.c), e desc_remontar_fileiras() refaz a home em
// memoria sem refazer Trakt, sem reler manifestos e sem re-baixar catalogos.
// Sem esta bandeira, desligar um addon no Guia disparava um ciclo de ~20 s
// que re-baixava TODOS os manifestos (inclusive de addons desligados) por nada.
static int   paLigou;
static float paRol, paVelRol;
// Linha do painel que falhou ao instalar (-1 = nenhuma) e o motivo, para a
// frase ser a certa: "a conta esta cheia" e "nao foi possivel" sao coisas
// diferentes para quem esta no sofa.
static int   paErro = -1, paErroCheio;
// Ha um recarregamento do guia esperando o fio de carga atual terminar.
static int   recarregarPend;

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
static char pedidoBase[600];
static void previewPedir(GCanal *c) {
  if (!c || !c->id[0]) return;
  canalParaItem(c, &pedPreviewItem);
  snprintf(pedidoBase, sizeof pedidoBase, "%s", c->base);
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

void guia_abrir(void) {
  guia_carregar();
  if (!previewLido) previewLer();
  aberta = 1; querSair = 0; entrada = 0.0f;
  focoTopo = 0; painel = 0;
  previewPreso = 0; previewId[0] = 0;
  previewUltLin = previewUltCol = -1; previewFocoMudou = SDL_GetTicks();
  focoValido();
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
  *x = G_PREVIEW_X; *y = G_PREVIEW_Y; *w = G_PREVIEW_W; *h = G_PREVIEW_H;
}

// A FAIXA ("mini guia") por cima do video em tela cheia. Some sozinha depois
// de G_B_OCIOSO_MS sem tecla, como os mini guias de referencia.
#define G_B_OCIOSO_MS 6000u
static Uint32 bandaUlt;
void guia_overlay_abrir(void) {
  guia_carregar();
  overlay = 1; entrada = 0.0f;
  overlayDesde = bandaUlt = SDL_GetTicks();
  focoTopo = 0; painel = 0; catAberto = 0;
  janelaDesl = 0; focoAnelOk = 0;
  focoValido();
}

int guia_aberta(void)         { return aberta; }
int guia_overlay_aberta(void) { return overlay; }
int guia_visivel(void)        { return aberta || overlay; }
int guia_quer_sair(void)      { int q = querSair; querSair = 0; return q; }

// O foco pedido com a lista ainda baixando fica pendente em focoPend;
// publicar() aplica assim que os canais chegam — o overlay aberto pelo
// player foca o canal que esta no ar mesmo se a carga so terminou depois.
void guia_focar_id(const char *id) {
  int l, c;
  if (!id || !*id) return;
  snprintf(focoPend, sizeof focoPend, "%s", id);
  for (l = 0; l < nLinhas(); l++)
    for (c = 0; c < linhaN(l); c++)
      if (!strcmp(linhaItem(l, c)->id, id)) { focoLin = l; focoCol = c; return; }
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
  snprintf(dst->tipo,    sizeof dst->tipo,    "%s", "channel");
  snprintf(dst->titulo,  sizeof dst->titulo,  "%s", c->nome);
  snprintf(dst->poster,  sizeof dst->poster,  "%s", c->logo);
  snprintf(dst->backdrop,sizeof dst->backdrop,"%s", c->logo);
  snprintf(dst->sinopse, sizeof dst->sinopse, "%s", c->desc);
  snprintf(dst->genero,  sizeof dst->genero,  "%s · %s",
           i18n("Canal"), c->cat >= 0 ? cats[c->cat] : "");
}

// A origem do ULTIMO canal pedido, lida por app.c junto com o CatItem. Fica
// fora do CatItem de proposito: ele e gravado em disco (catalogo-rede.bin) e
// um campo novo invalidaria o cache de todo mundo por um dado de sessao.
const char *guia_canal_origem(void) { return pedidoBase; }

static void pedirCanal(GCanal *c) {
  if (!c || pediuCanal) return;
  // OK NO CANAL QUE JA TOCA (no preview, ou atras da faixa): tela cheia com o
  // MESMO fluxo, sem recarregar nada. Qualquer outro canal abre pelo caminho
  // de sempre — o player_abrir da sessao nova solta o preview.
  if (!strcmp(c->id, player_id_canal()) &&
      (player_mini_no_guia_ativo() || player_aberto())) {
    if (player_mini_no_guia_ativo()) pedRestaurar = 1;
    overlay = 0;
    return;
  }
  pedPreview = 0; previewId[0] = 0; previewPreso = 0;
  canalParaItem(c, &pedido);
  snprintf(pedidoBase, sizeof pedidoBase, "%s", c->base);
  pediuCanal = 1;
  // No overlay o OK TROCA o canal e volta ao video: ficar aberto por cima da
  // troca deixaria a pessoa assistindo atras de uma grade que ela ja usou.
  overlay = 0;
}

// A ordem publicada e a do guia (canais agrupados por categoria, na ordem em
// que o addon os declara), entao o CH+/− percorre exatamente o que se ve na
// tela. Da volta nas pontas: de "premiere" para "globo" direto e o jeito
// antigo de zapear, nao um erro de foco.
int guia_zap(const char *idAtual, int dir, CatItem *saida) {
  int i, alvo = 0;
  if (!saida || estado != G_PRONTO || nCanais < 1) return 0;
  for (i = 0; i < nCanais; i++)
    if (!strcmp(canais[i].id, idAtual ? idAtual : "")) { alvo = i; break; }
  alvo = (alvo + dir + nCanais) % nCanais;
  canalParaItem(&canais[alvo], saida);
  return 1;
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
  catAberto = modo;
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
static int painelN(void) { return paN + nRec; }

static void painelAbrir(void) {
  recLer();
  painel = 1; paFoco = 0; paMexeu = 0; paLigou = 0; paRol = 0.0f; paVelRol = 0.0f;
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
static void painelFechar(void) {
  painel = 0;
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
  sync_sujar_addons();
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
    sync_sujar_addons();   // desligar aqui e desligar no celular tambem
    paMexeu = 1;
    if (ligado) paLigou = 1;   // ligou (ou acabou de instalar): fechar pede ciclo
    painelAplicar(i);
  } else if (paFoco - paN < nRec && !recInstalado(&rec[paFoco - paN])) {
    instalar(paFoco - paN);
  }
}

// OK segurado NAO e varios OK: o firmware repete o KEYDOWN a cada ~130 ms, e
// sem este repouso segurar a tecla ligava e desligava o addon em sequencia
// (addonsui.c herda esse comportamento; aqui nao).
static Uint32 paOkTick;
static void painelEvento(SDL_Keycode k, Uint32 agora) {
  int n = painelN();
  if (k == SDLK_UP)   { if (paFoco > 0) paFoco--; return; }
  if (k == SDLK_DOWN) { if (paFoco + 1 < n) paFoco++; return; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
    if (n > 0 && agora - paOkTick >= G_REP_MS) { paOkTick = agora; painelOk(); }
    return;
  }
}

static void sair(void) {
  if (painel) { painelFechar(); return; }
  if (catAberto) { catFechar(); return; }
  if (overlay) overlay = 0;
  else { aberta = 0; querSair = 1; previewParar(); }
  catAberto = 0; dirSeg = 0; okDesde = 0; focoTopo = 0;
}

void guia_evento(const SDL_Event *e) {
  if (!guia_visivel()) return;

  if (e->type == SDL_KEYUP) {
    SDL_Keycode k = e->key.keysym.sym;
    if (k == SDLK_UP || k == SDLK_DOWN) {
      dirSeg = 0;
      // Soltou a seta que abriu o painel: entra onde o foco dele parou.
      if (catAberto == 1) catConfirmar();
    }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
      if (okDesde && !okLongo) pedirCanal(linhaItem(focoLin, focoCol));
      okDesde = 0; okLongo = 0;
    }
    return;
  }
  if (e->type != SDL_KEYDOWN) return;
  SDL_Keycode k = e->key.keysym.sym;
  Uint32 agora = SDL_GetTicks();

  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    sair(); return;
  }
  // NA FAIXA, AZUL ABRE O GUIA COMPLETO (o canal no ar encolhe para o
  // preview; quem faz e o app, por guia_pediu_guia_cheio). O repouso de
  // G_OVERLAY_REP_MS ignora a repeticao do firmware do botao que ABRIU a
  // faixa, senao segurar o azul pulava direto para o guia completo.
  if (overlay && (k == SDLK_s || e->key.keysym.scancode == NV_SCANCODE_BLUE)) {
    if (agora - overlayDesde < G_OVERLAY_REP_MS) return;
    overlay = 0; pedGuiaCheio = 1; return;
  }
  // O resto da faixa: CIMA/BAIXO andam de canal em canal (atravessando as
  // categorias), ESQUERDA/DIREITA andam o tempo, OK troca de canal (no mesmo
  // canal so fecha — ver pedirCanal). Toda tecla adia o sumico.
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

  // AMARELO alterna lista e cartoes de qualquer lugar. `l` no teclado do Mac
  // e o equivalente de bancada, como `s` e do azul.
  if (e->key.keysym.scancode == G_SCANCODE_YELLOW || k == SDLK_l) {
    dirSeg = 0; catAberto = 0;
    alternarModo();
    return;
  }

  // CH+/- do controle da LG (scancodes 480/481 do SDL_webOS.h): dentro do
  // guia eles pulam CATEGORIA, nao canal — e a mesma leitura do "segurar"
  // sem exigir o gesto. No Tizen o CH+ chega como "s" (tizen-shell.html), e o
  // CH- nao chega — a casca nao o registra.
  if (e->key.keysym.scancode == NV_SCANCODE_CH_UP ||
      e->key.keysym.scancode == NV_SCANCODE_CH_DOWN ||
      (!overlay && k == SDLK_s)) {
    focoTopo = 0;
    saltarCat(e->key.keysym.scancode == NV_SCANCODE_CH_UP ? -1 : 1);
    return;
  }

  // Foco no cabecalho: ESQUERDA/DIREITA entre os tres controles, BAIXO volta
  // as linhas, OK age. CIMA nao faz nada — nao ha nada acima.
  if (focoTopo) {
    dirSeg = 0;
    if (k == SDLK_LEFT)  { if (topoCol > 0) topoCol--; return; }
    if (k == SDLK_RIGHT) { if (topoCol + 1 < G_TOPO_N) topoCol++; return; }
    if (k == SDLK_DOWN)  { focoTopo = 0; return; }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
      if (topoCol == G_TOPO_CATEGORIAS) catAbrir(2, 0);
      else if (topoCol == G_TOPO_ADDONS) painelAbrir();
      else if (topoCol == G_TOPO_PREVIEW) {
        previewLigado = !previewLigado;
        previewGravar();
        if (previewLigado) { previewUltLin = previewUltCol = -1; previewFocoMudou = 0; }
        else previewParar();
      }
      else if ((topoCol == G_TOPO_LISTA) != modoLista) alternarModo();
      return;
    }
    return;
  }

  if (k == SDLK_UP || k == SDLK_DOWN) {
    int dir = (k == SDLK_DOWN) ? 1 : -1;
    int fresco = !(dirSeg == k && agora - dirTick < G_REP_MS);
    // CIMA na primeira linha sobe ao cabecalho — so num toque FRESCO: quem
    // esta segurando CIMA quer o modo salta-categoria, nao o controle.
    if (dir < 0 && fresco && focoLin == 0 && (!modoLista || focoCol == 0)) {
      focoTopo = 1; topoCol = modoLista ? G_TOPO_LISTA : G_TOPO_CARTOES;
      dirSeg = 0;
      return;
    }
    // Ainda segurando a mesma direcao? O firmware repete o KEYDOWN a cada
    // ~130 ms; o relogio e quem distingue "toque" de "segurado".
    if (!fresco) {
      if (agora - dirDesde >= G_HOLD_MS) {
        catAbrir(1, dir);
        ultNavCat = agora; dirTick = agora;
        return;
      }
    } else { dirSeg = k; dirDesde = agora; }
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
                           else { focoTopo = 1; topoCol = G_TOPO_LISTA; } return; }
    if (k == SDLK_RIGHT) { if (janelaDesl < G_L_DESL_MAX) janelaDesl += G_L_PASSO_MIN; return; }
  } else {
    if (k == SDLK_LEFT)  { if (focoCol > 0) focoCol--;
                           else { focoTopo = 1; topoCol = G_TOPO_CARTOES; } return; }
    if (k == SDLK_RIGHT) { if (focoCol + 1 < linhaN(focoLin)) focoCol++; return; }
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
    if (!okDesde) { okDesde = agora; okLongo = 0; }
    return;
  }
}

// --- EPG por canal ---------------------------------------------------------------
// Resolve e cacheia o indice da grade. -2 marca "sem grade real" para nao
// consultar de novo a cada quadro.
static int epgDo(GCanal *c) {
  if (c->epg == -1) {
    if (epg_estado() != EPG_PRONTO) return -1;
    c->epg = epg_match(c->nome);
    if (c->epg < 0) c->epg = -2;
  }
  return c->epg;
}

// Quando a grade EPG e (re)publicada, os indices guardados morrem — a troca
// inteira do vetor torna todo -1 de novo.
static int epgRev, epgEraPronto;
static void epgPasso(void) {
  int pronto = epg_estado() == EPG_PRONTO;
  epg_passo();
  if (pronto && !epgEraPronto) {
    epgRev++;
    for (int i = 0; i < nCanais; i++) canais[i].epg = -1;
  }
  epgEraPronto = pronto;
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
  if (i < n) return 36.0f + (float)i * G_PA_ROW;
  return 36.0f + (float)n * G_PA_ROW + 28.0f + 36.0f + 60.0f + (float)(i - n) * G_PA_ROW_REC;
}
#define G_PA_LISTA_Y 200.0f

void guia_atualizar(float dt, Uint32 agora) {
  entrada = anim_mola(entrada, guia_visivel() ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  if (pendPronto) {
    publicar(); estado = G_PRONTO; pendPronto = 0; fioVivo = 0; focoValido();
    if (painel) painelMontar();   // sabe[] mudou: quem nao fornece canal sai
  }
  // O painel de addons pediu recarga com o fio ainda vivo: agora que ele
  // acabou, vai.
  if (recarregarPend && !fioVivo) {
    recarregarPend = 0; estado = G_PARADO; ultTentativa = 0; iniciarCarga();
  }
  // PREVIEW: aplica a URL que o fio buscou (no fio de desenho, nunca no de
  // rede) e arranca o preview quando o foco descansa num canal novo.
  if (overlay && agora - bandaUlt > G_B_OCIOSO_MS) { overlay = 0; okDesde = 0; }
  // O foco mudou de canal: o relogio do descanso recomeca.
  { static int ul = -1, uc = -1;
    if (focoLin != ul || focoCol != uc) { ul = focoLin; uc = focoCol; previewFocoMudou = agora; } }
  if (aberta && !overlay && previewLigado && !previewPreso && !player_aberto() &&
      nCanais > 0 && (estado == G_PRONTO || estado == G_BAIXANDO) &&
      (focoLin != previewUltLin || focoCol != previewUltCol) &&
      agora - previewFocoMudou >= G_PREVIEW_ESPERA_MS) {
    GCanal *c = linhaItem(focoLin, focoCol);
    previewUltLin = focoLin; previewUltCol = focoCol;
    if (c && c->id[0] && strcmp(c->id, previewId)) previewPedir(c);
  }
  if (!guia_visivel()) return;

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
    rolL = anim_mola2(&velL, rolL, alvo, dt, NV_MOLA_SCROLL);
  }
  if (painel) {
    float areaH = NV_TELA_H - 80.0f - G_PA_LISTA_Y;
    float alvo = paItemY(paFoco) - areaH * 0.4f;
    float maxY = (painelN() > 0 ? paItemY(painelN() - 1) + (nRec > 0 ? G_PA_ROW_REC : G_PA_ROW) : 0.0f) - areaH;
    if (maxY < 0.0f) maxY = 0.0f;
    if (alvo > maxY) alvo = maxY;
    if (alvo < 0.0f) alvo = 0.0f;
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
  // Agora a tentativa e por TEMPO enquanto nao ha canal: a cada 10 s com o guia
  // na tela. Sao dois GET de manifesto por tentativa, e so enquanto a tela esta
  // aberta e vazia — que e exatamente a situacao em que a pessoa esta olhando
  // para um guia sem nada e esperando.
  if (!nCanais && estado != G_BAIXANDO &&
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
  if (catAberto == 1 && !dirSeg) catConfirmar();
  catAnim = anim_mola(catAnim, catAberto ? 1.0f : 0.0f, dt, 12.0f);
  if (catAberto || catAnim > 0.01f) {
    float areaH = G_CAT_BASE - G_CAT_TOPO;
    float alvo = (float)catFoco * G_CAT_ROW - (areaH - G_CAT_ROW) * 0.5f;
    float maxY = (float)nLinhas() * G_CAT_ROW - areaH;
    if (maxY < 0.0f) maxY = 0.0f;
    if (alvo > maxY) alvo = maxY;
    if (alvo < 0.0f) alvo = 0.0f;
    if (catAnim < 0.02f) { catRol = alvo; catVelRol = 0.0f; }
    else catRol = anim_mola2(&catVelRol, catRol, alvo, dt, NV_MOLA2_SCROLL);
  }

  // OK segurado = favorito.
  if (okDesde && !okLongo && agora - okDesde >= NV_HOLD_MS) {
    GCanal *c = linhaItem(focoLin, focoCol);
    if (c) favAlternar(c);
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
    rolY = anim_mola2(&velY, rolY, alvo, dt, NV_MOLA_SCROLL); }
  // Rolagem horizontal da fileira em foco.
  { int l = focoLin;
    float passo = G_CARD_W + G_GAP_X;
    float alvo = (float)focoCol * passo - G_AREA_W * 0.5f + G_CARD_W * 0.5f;
    if (alvo < 0.0f) alvo = 0.0f;
    rolX[l] = anim_mola(rolX[l], alvo, dt, 14.0f); }
}

// --- desenho ---------------------------------------------------------------------
static void fmtHora(time_t t, char *dst, size_t n) {
  struct tm lt; localtime_r(&t, &lt);
  strftime(dst, n, "%H:%M", &lt);
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
static void logoNaCaixa(const GCanal *c, GfxRect cx, float maxW, float maxH,
                        float tom, float a) {
  GLuint t = 0;
  float ap = 0.0f, w, h, fr, fg, fb;
  const char *logo = c ? c->logo : NULL;
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
    iniciaisDe(c ? c->nome : "", ini, sizeof ini);
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

static void desenharLogo(const GCanal *c, GfxRect cx, float lado, float tom,
                         float a) {
  logoNaCaixa(c, cx, lado, lado, tom, a);
}

static void desenharCard(GCanal *c, float x, float y, float foco, float a,
                         time_t agoraT) {
  GfxRect r = { x, y, G_CARD_W, G_CARD_H };
  float lum = anim_mistura(0.075f, 0.16f, foco);
  EpgProg ag, px;
  int epg = epgDo(c);
  int temAgora = epg >= 0 && epg_agora(epg, agoraT, &ag);
  int temProx  = epg >= 0 && epg_proximo(epg, agoraT, 0, &px);

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
      char h1[8], h2[8], faixa[40];
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
      char h1[8]; fmtHora(px.ini, h1, sizeof h1);
      snprintf(linha, sizeof linha, "%s %s  \xc2\xb7  %s",
               i18n("A seguir"), h1, px.titulo);
    } else {
      // -2 = canal sem grade real (os "24h"): nao e "carregando", e "nao tem".
      snprintf(linha, sizeof linha, "%s",
               epg == -2 ? i18n("Sem grade de programação")
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
static float seloAoVivo(float x, float y, float a) {
  TxtLinha t = txt_linha(TXT_PG_ROTULO, i18n("AO VIVO"), 255, 255, 255, 255);
  GfxRect r = { x, y, (float)t.w + 26.0f, 32.0f };
  gfx_cor(r, 0.5f, 0.84f, 0.15f, 0.19f, a);
  txt_desenhar_alpha(t, r.x + 13.0f, r.y + (r.h - (float)t.h) * 0.5f, a);
  return r.w;
}

// Etiqueta discreta (categoria): vidro translucido, texto secundario. A
// categoria era texto AZUL solto embaixo do nome, e competia com o titulo.
static float etiqueta(const char *s, float x, float y, float maxW, float a) {
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
  float proxY = G_HERO_Y + G_PREVIEW_H - 34.0f;
  float ar, ag, ab;
  int epg, tem = 0, noAr = 0;
  EpgProg p;
  if (!c) return;
  ajustes_acento(&ar, &ag, &ab);
  epg = epgDo(c);
  tem = epg >= 0 && epg_agora(epg, tFoco, &p);
  noAr = tem && p.ini <= agoraT && agoraT < p.fim;

  // PREVIEW. O plano de video vive ATRAS da superficie GL; o furo e o que o
  // deixa aparecer (mesmo mecanismo do player, ver gfx_furo), com os cantos
  // do raio do heroi. So em tela cheia: no overlay o player ja esta atras, e
  // o pipeline de video e um so.
  //
  // O furo so abre quando o pipeline tem um quadro (video_pronto); antes
  // disso o plano esta vazio e furar cedo mostrava um retangulo PRETO. Ate la
  // o lugar do video e uma superficie com o logo: o heroi nunca muda de forma.
  // Enquanto a janela do player ANDA (encolhendo da tela cheia), o furo e o
  // do degrau, desenhado no fim de guia_desenhar — aqui fica so a superficie.
  //
  // O PREVIEW PODE SER DE OUTRO CANAL que o focado: com o canal no ar adotado
  // (previewPreso) ele segue tocando enquanto a pessoa navega. O selo diz
  // qual, e o logo de espera e o dele.
  { GCanal *pc = previewId[0] ? canalPorId(previewId) : NULL;
    int sessao = aberta && !overlay && player_mini_no_guia_ativo();
    int anda = player_janela_animando(NULL, NULL, NULL, NULL);
    if (!pc) pc = c;
  if (sessao && !anda && video_pronto() && !player_carregando()) {
    gfx_furo_raio(pv, raio);
    // Fio de 1,5 px: o video le como parte da interface, e nao como buraco.
    gfx_rect(pv, 0, GFX_ANEL, 0, 1.5f / pv.h, 0, raio, 1, 1, 1, 0.16f * a);
    // Selo sobre o video: GL opaco por cima do furo aparece por cima do plano.
    { float sw = seloAoVivo(pv.x + 20.0f, pv.y + pv.h - 52.0f, a);
      if (pc != c) {
        TxtLinha t = txt_linha_corta(TXT_PG_ROTULO, pc->nome, 245, 246, 250, 255, pv.w - sw - 80.0f);
        GfxRect r = { pv.x + 20.0f + sw + 8.0f, pv.y + pv.h - 52.0f, (float)t.w + 24.0f, 32.0f };
        gfx_cor(r, 0.5f, 0.02f, 0.02f, 0.03f, 0.72f * a);
        txt_desenhar_alpha(t, r.x + 12.0f, r.y + (r.h - (float)t.h) * 0.5f, a);
      } }
  } else if (anda) {
    // encolhendo: nada aqui, o furo do degrau vem por cima de tudo
  } else {
    gfx_cor(pv, raio, 0.078f, 0.080f, 0.088f, a);
    // Uma luz so, bem fraca, na cor de realce: tira a cara de caixa vazia.
    gfx_luz_canto(pv, raio, pv.w * 0.82f, -pv.h * 0.2f, pv.w * 0.85f,
                  ar, ag, ab, 0.10f * a);
    { GfxRect cx = { pv.x + (pv.w - 320.0f) * 0.5f, pv.y + (pv.h - 170.0f) * 0.5f - 12.0f,
                     320.0f, 170.0f };
      logoNaCaixa(pc, cx, 300.0f, 150.0f, G_LOGO_CLARO, pc == c ? ha : a); }
    // "Carregando" so enquanto ha algo a caminho: sem sessao nem pedido,
    // dizer "carregando" seria mentir para sempre.
    if ((sessao && player_carregando()) || (aberta && !overlay && pedPreview)) {
      TxtLinha t = txt_linha(TXT_DET_META2, i18n("carregando canal…"), 160, 163, 172, 255);
      txt_desenhar_alpha(t, pv.x + (pv.w - (float)t.w) * 0.5f, pv.y + pv.h - 60.0f, a);
    }
  } }

  // LINHA DO CANAL: logo, numero, nome e a categoria como etiqueta. O numero
  // e a posicao na ordem do guia — a mesma que o CH+/- percorre (guia_zap).
  { GfxRect lx = { x, y, 112.0f, 64.0f };
    char num[16], cat[96];
    float tx = x + 132.0f, tw = w - 132.0f, ey;
    TxtLinha n;
    snprintf(num, sizeof num, "%d", (int)(c - canais) + 1);
    snprintf(cat, sizeof cat, "%s", c->cat >= 0 ? i18n(cats[c->cat]) : "");
    logoNaCaixa(c, lx, 112.0f, 60.0f, G_LOGO_CLARO, ha);
    n = txt_linha(TXT_DET_META2, num, 150, 153, 162, 255);
    if (tem) {
      TxtLinha t = txt_linha_corta(TXT_CW_TITULO, c->nome, 236, 237, 242, 255,
                                   tw - (float)n.w - 16.0f);
      txt_desenhar_alpha(n, tx, y + 2.0f + (float)(t.h - n.h) * 0.5f, ha);
      txt_desenhar_alpha(t, tx + (float)n.w + 14.0f, y, ha);
      ey = y + 34.0f;
    } else {
      // Sem programa, o nome do canal vira o titulo grande logo abaixo; aqui
      // ficam so o numero e a categoria, centrados na altura do logo.
      ey = y + 15.0f;
      txt_desenhar_alpha(n, tx, ey + (34.0f - (float)n.h) * 0.5f, ha);
      tx += (float)n.w + 16.0f;
    }
    tx += etiqueta(cat, tx, ey, 320.0f, ha);
    if (c->fav) {
      TxtLinha s = txt_linha(TXT_DET_META2, "\xe2\x98\x85", 255, 214, 90, 255);
      txt_desenhar_alpha(s, tx + 12.0f, ey + (34.0f - (float)s.h) * 0.5f, ha);
    } }
  y += 64.0f + 30.0f;

  // TITULO: o programa, ou o canal quando nao ha programa. 56 px em ate duas
  // linhas — o degrau acima do nome do canal (28) e de 2x, e o de baixo, ate
  // a linha de meta (25), de 2,2x: a ficha le em tres niveis de relance.
  y += txt_bloco(TXT_TITULO2, tem ? p.titulo : c->nome, 246, 247, 250,
                 x, y - 4.0f, w, 66.0f, ha, 2) + 10.0f;

  // META: selo + faixa de horario + quanto falta, e a barra de progresso.
  { char meta[160], h1[8], h2[8];
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
    if (noAr || !tem) mx += seloAoVivo(x, y, ha) + 16.0f;
    if (meta[0]) {
      TxtLinha t = txt_linha_corta(TXT_DET_META, meta, 196, 198, 206, 255, x + w - mx);
      txt_desenhar_alpha(t, mx, y + (32.0f - (float)t.h) * 0.5f, ha);
    }
    y += 32.0f + 18.0f;
    if (noAr) {
      float f = (p.fim > p.ini)
                ? anim_clamp((float)(agoraT - p.ini) / (float)(p.fim - p.ini), 0.0f, 1.0f)
                : 0.0f;
      GfxRect tr = { x, y, 440.0f, 4.0f };
      GfxRect an = { tr.x, tr.y, tr.w * f, tr.h };
      gfx_cor(tr, 0.5f, 1, 1, 1, 0.14f * a);
      gfx_cor(an, 0.5f, ar, ag, ab, a);
      y += 4.0f + 22.0f;
    } }

  // DESCRICAO do canal (o XMLTV deste guia nao traz sinopse de programa). O
  // molde do addon ("Categoria: X Qualidades: 4K, FHD 8 fonte(s)") vira
  // selos de qualidade e a contagem de fontes; o que sobrar, ou texto fora
  // do molde, vai como veio — no maximo tres linhas, e so as que cabem antes
  // da linha "A seguir".
  if (c->desc[0]) {
    GDesc d;
    int molde = descMolde(c->desc, &d);
    const char *txt = molde ? d.resto : c->desc;
    if (molde && (d.nq > 0 || d.fontes > 0)) {
      float bx = x;
      int k;
      for (k = 0; k < d.nq; k++) {
        TxtLinha t = txt_linha(TXT_PG_ROTULO, d.q[k], 222, 224, 230, 255);
        GfxRect r = { bx, y, (float)t.w + 20.0f, 30.0f };
        gfx_cor(r, 0.2f, 1, 1, 1, 0.06f * ha);
        gfx_rect(r, 0, GFX_ANEL, 0, 1.5f / r.h, 0, 0.2f, 1, 1, 1, 0.32f * ha);
        txt_desenhar_alpha(t, r.x + 10.0f, r.y + (r.h - (float)t.h) * 0.5f, ha);
        bx += r.w + 8.0f;
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
      int linhas = (int)((proxY - 22.0f - y) / 34.0f);
      if (linhas > 3) linhas = 3;
      if (linhas > 0)
        txt_bloco(TXT_DET_SIN, txt, 172, 175, 184, x, y, w, 34.0f, ha * 0.95f, linhas);
    }
  }

  // A SEGUIR, ancorado na base do heroi (alinhado a base do preview).
  { EpgProg q;
    if (epg >= 0 && epg_proximo(epg, tFoco, 0, &q)) {
      char hh[8];
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
static void desenharPainelCategorias(float a) {
  float e = catAnim, ar, ag, ab;
  float x0 = -G_CAT_W * 0.18f * (1.0f - e);   // desliza ~80 px enquanto aparece
  float ea = e * a;
  int nl = nLinhas(), i, tf = ajustes_tinta_foco();
  if (e < 0.01f || nl < 1) return;
  ajustes_acento(&ar, &ag, &ab);
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.62f * ea);
  { GfxRect p = { x0 - 40.0f, 0.0f, G_CAT_W + 40.0f, NV_TELA_H };
    gfx_cor(p, 28.0f / p.w, 0.070f, 0.072f, 0.080f, ea);
    gfx_luz_canto(p, 28.0f / p.w, p.w * 0.95f, -60.0f, p.w * 0.8f, ar, ag, ab, 0.10f * ea); }
  { TxtLinha t = txt_linha(TXT_HEADLINE, i18n("Categorias"), 242, 243, 247, 255);
    txt_desenhar_alpha(t, x0 + 48.0f, 60.0f, ea); }

  gfx_recorte(0.0f, G_CAT_TOPO - 4.0f, G_CAT_W, G_CAT_BASE - G_CAT_TOPO + 8.0f);
  for (i = 0; i < nl; i++) {
    float y = G_CAT_TOPO + (float)i * G_CAT_ROW - catRol;
    int foc = i == catFoco, atual = i == focoLin;
    GfxRect r = { x0 + 32.0f, y, G_CAT_W - 56.0f, G_CAT_ROW - 8.0f };
    char n[16];
    TxtLinha tn, tc;
    if (y + G_CAT_ROW < G_CAT_TOPO - 4.0f || y > G_CAT_BASE) continue;
    snprintf(n, sizeof n, "%d", linhaN(i));
    if (foc) gfx_cor(r, 12.0f / r.h, ar, ag, ab, ea);
    else if (atual) gfx_cor(r, 12.0f / r.h, 1, 1, 1, 0.06f * ea);
    if (atual && !foc)
      gfx_cor((GfxRect){ r.x + 8.0f, r.y + (r.h - 24.0f) * 0.5f, 4.0f, 24.0f }, 0.5f,
              ar, ag, ab, ea);
    tc = foc ? txt_linha(TXT_DET_META2, n, tf, tf, tf, 255)
             : txt_linha(TXT_DET_META2, n, 140, 143, 152, 255);
    txt_desenhar_alpha(tc, r.x + r.w - 16.0f - (float)tc.w, r.y + (r.h - (float)tc.h) * 0.5f, ea);
    { float nw = r.w - 24.0f - 16.0f - (float)tc.w - 16.0f;
      int cn = atual ? 245 : 205;
      tn = foc ? txt_linha_corta(TXT_BODY, linhaNome(i), tf, tf, tf, 255, nw)
               : txt_linha_corta(TXT_BODY, linhaNome(i), cn, cn + 1, cn + 5 > 255 ? 255 : cn + 5, 255, nw);
      txt_desenhar_alpha(tn, r.x + 24.0f, r.y + (r.h - (float)tn.h) * 0.5f, ea); }
  }
  gfx_sem_recorte();
  { TxtLinha t = txt_linha_corta(TXT_CAPTION, i18n("OK entra  ·  Voltar fecha"),
                                 128, 130, 138, 255, G_CAT_W - 96.0f);
    txt_desenhar_alpha(t, x0 + 48.0f, NV_TELA_H - 60.0f, ea); }
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
static float desenharTopo(float a) {
  const char *rot[G_TOPO_N];
  float w[G_TOPO_N], xs[G_TOPO_N], x, ar, ag, ab, seg0;
  int i;
  rot[G_TOPO_CATEGORIAS] = i18n("Categorias");
  rot[G_TOPO_CARTOES] = i18n("Cartões");
  rot[G_TOPO_LISTA]   = i18n("Lista");
  rot[G_TOPO_ADDONS]  = i18n("Addons");
  rot[G_TOPO_PREVIEW] = previewLigado ? i18n("Preview: sim") : i18n("Preview: não");
  ajustes_acento(&ar, &ag, &ab);
  for (i = 0; i < G_TOPO_N; i++)
    w[i] = (float)txt_linha(TXT_PG_ROTULO, rot[i], 255, 255, 255, 255).w
           + 2.0f * G_CHIP_PAD
           + (i == G_TOPO_PREVIEW ? 20.0f : i == G_TOPO_CATEGORIAS ? 26.0f : 0.0f);

  // Relogio na margem direita, na altura dos chips.
  { time_t tt = time(NULL); struct tm lt; char hora[8];
    TxtLinha t;
    localtime_r(&tt, &lt); strftime(hora, sizeof hora, "%H:%M", &lt);
    t = txt_linha(TXT_CALLOUT, hora, 236, 237, 242, 255);
    x = G_AREA_DIR - (float)t.w;
    txt_desenhar_alpha(t, x, G_TOPO_Y + (G_TOPO_H - (float)t.h) * 0.5f, a);
    x -= 36.0f; }

  // Da direita para a esquerda: Preview, Addons, e o par segmentado.
  for (i = G_TOPO_N - 1; i >= 0; i--) {
    x -= w[i];
    xs[i] = x;
    x -= (i == G_TOPO_LISTA) ? 0.0f : 12.0f;
  }
  seg0 = xs[G_TOPO_CARTOES];
  // Trilho do controle segmentado.
  { GfxRect tr = { seg0, G_TOPO_Y, w[G_TOPO_CARTOES] + w[G_TOPO_LISTA], G_TOPO_H };
    gfx_cor(tr, 0.5f, 1, 1, 1, 0.07f * a); }

  for (i = 0; i < G_TOPO_N; i++) {
    float f = animTopo[i];
    int seg = i == G_TOPO_CARTOES || i == G_TOPO_LISTA;
    int sel = i == G_TOPO_PREVIEW ? previewLigado
            : i == G_TOPO_CATEGORIAS ? catAberto != 0
            : seg ? ((i == G_TOPO_LISTA) == modoLista) : 0;
    GfxRect r = { xs[i], G_TOPO_Y, w[i], G_TOPO_H };
    int ct;
    if (seg) {
      // Segmento: so o escolhido tem superficie, 3 px para dentro do trilho.
      if (sel) gfx_cor((GfxRect){ r.x + 3.0f, r.y + 3.0f, r.w - 6.0f, r.h - 6.0f },
                       0.5f, 1, 1, 1, 0.17f * a);
    } else {
      gfx_cor(r, 0.5f, 1, 1, 1, (sel ? 0.14f : 0.07f) * a);
    }
    if (f > 0.01f) {
      gfx_cor(r, 0.5f, 1, 1, 1, 0.10f * f * a);
      gfx_anel_fora(r, 0.5f, 0.0f, 3.0f, ar, ag, ab, f * a);
    }
    ct = (f > 0.5f || sel) ? 245 : 168;
    { TxtLinha t = txt_linha(TXT_PG_ROTULO, rot[i], ct, ct + 1, ct + 5 > 255 ? 255 : ct + 5, 255);
      float tx = r.x + (r.w - (float)t.w) * 0.5f;
      if (i == G_TOPO_PREVIEW) {
        // Ponto de estado: verde ligado, cinza desligado. O texto ja diz,
        // o ponto e o que se le de relance.
        GfxRect d = { r.x + G_CHIP_PAD, r.y + (r.h - 10.0f) * 0.5f, 10.0f, 10.0f };
        if (previewLigado) gfx_cor(d, 0.5f, 0.30f, 0.84f, 0.46f, a);
        else               gfx_cor(d, 0.5f, 0.42f, 0.43f, 0.47f, a);
        tx = d.x + 20.0f;
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
  return xs[G_TOPO_CATEGORIAS];
}

// --- modo lista ---------------------------------------------------------------------
// Inicio da janela de tempo: a meia hora cheia anterior a agora, mais o
// deslocamento pedido com DIREITA. Regua em meias horas porque e assim que a
// grade de TV sempre foi lida; o olho ja sabe onde procurar.
static time_t janelaIni(time_t agoraT) {
  return (agoraT / 1800) * 1800 + (time_t)janelaDesl * 60;
}

// O instante que o foco aponta: agora, ou DEZ MINUTOS depois do comeco da
// janela quando ela foi adiantada. A celula em foco e a do programa que cobre
// este instante. Os dez minutos existem porque o comeco exato da janela
// costuma cair no restinho do programa anterior (01:32-02:02 numa janela que
// abre as 02:00), e o anel abracava uma lasca de 20 px.
static time_t instanteFoco(time_t agoraT) {
  if (!modoLista || janelaDesl <= 0) return agoraT;
  return janelaIni(agoraT) + 10 * 60;
}

static void desenharReguaEm(float a, time_t ini, float yR);
static void desenharRegua(float a, time_t ini) { desenharReguaEm(a, ini, G_TOPO); }
static void desenharReguaEm(float a, time_t ini, float yR) {
  float ppm = G_L_FAIXA_W / (float)G_L_JANELA_MIN;
  int i, n = G_L_JANELA_MIN / G_L_PASSO_MIN;
  for (i = 0; i < n; i++) {
    char h[8];
    float x = G_L_FAIXA_X + (float)(i * G_L_PASSO_MIN) * ppm;
    TxtLinha t;
    fmtHora(ini + (time_t)i * G_L_PASSO_MIN * 60, h, sizeof h);
    t = txt_linha(TXT_DET_META2, h, 160, 163, 172, 255);
    // Rotulo logo a direita do tique, como nas grades de referencia: o tique
    // marca o instante, o rotulo le junto dele.
    txt_desenhar_alpha(t, x + 10.0f, yR + 2.0f, a);
    gfx_cor((GfxRect){ x, yR + 4.0f, 1.0f, 30.0f }, 0.0f, 1, 1, 1, 0.20f * a);
    // Meio da meia hora: tique curto.
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
static float gCel = G_L_CEL;
// O canal no ar ganha a barra na cor de realce na coluna (so na faixa, onde
// o foco pode estar longe dele).
static const char *gIdNoAr = "";
static void desenharLinhaLista(GCanal *c, float y, int focada, float a,
                               time_t agoraT, time_t ini, time_t tFoco,
                               int passo, float agoraX, GfxRect *alvo) {
  float h = gCel;
  float raioC = 12.0f / h, raioB = 10.0f / h;
  time_t fimJ = ini + (time_t)G_L_JANELA_MIN * 60;
  float ppm = G_L_FAIXA_W / (float)G_L_JANELA_MIN;
  GfxRect col = { G_AREA_X, y, G_L_COL, h };
  int epg = epgDo(c);

  if (passo == 0) {
    float l = focada ? G_SUP_COL_FOCO : G_SUP_COL;
    gfx_cor(col, raioC, l, l, l, a);   // cinza NEUTRO: sem desvio de cor
    if (gIdNoAr[0] && !strcmp(c->id, gIdNoAr)) {
      float ar, ag, ab;
      ajustes_acento(&ar, &ag, &ab);
      gfx_cor((GfxRect){ col.x - 14.0f, col.y + (h - 28.0f) * 0.5f, 4.0f, 28.0f }, 0.5f,
              ar, ag, ab, a);
    }
  } else {
    char num[16];
    TxtLinha n;
    int cn = focada ? 196 : 128;
    snprintf(num, sizeof num, "%d", (int)(c - canais) + 1);
    n = txt_linha(TXT_DET_META2, num, cn, cn + 2, cn + 8, 255);
    // Numero alinhado pela DIREITA a 48 px: 1, 12 e 312 terminam na mesma
    // coluna, como na grade impressa.
    txt_desenhar_alpha(n, G_AREA_X + 48.0f - (float)n.w, y + (h - (float)n.h) * 0.5f, a);
    { GfxRect cx = { G_AREA_X + 62.0f, y + 10.0f, 92.0f, h - 20.0f };
      logoNaCaixa(c, cx, 84.0f, 42.0f, G_LOGO_CLARO, a); }
    { float nx = G_AREA_X + 168.0f;
      float tw = G_L_COL - 168.0f - 16.0f - (c->fav ? 30.0f : 0.0f);
      int ct = focada ? 250 : 212, cb = ct + 4 > 255 ? 255 : ct + 4;
      // Nome em ATE DUAS linhas: "Canal Recortado HD" cortado em "Canal…"
      // nao diz qual canal e. Uma linha quando cabe, centrada na celula.
      TxtLinha t = txt_linha(TXT_CW_TITULO, c->nome, ct, ct, cb, 255);
      if ((float)t.w <= tw)
        txt_desenhar_alpha(t, nx, y + (h - (float)t.h) * 0.5f, a);
      else
        txt_bloco(TXT_CW_TITULO, c->nome, ct, ct, cb, nx, y + (h - 60.0f) * 0.5f - 2.0f,
                  tw, 30.0f, a, 2);
      if (c->fav) {
        TxtLinha s = txt_linha(TXT_DET_META2, "\xe2\x98\x85", 255, 214, 90, 255);
        txt_desenhar_alpha(s, G_AREA_X + G_L_COL - 16.0f - (float)s.w,
                           y + (h - (float)s.h) * 0.5f, a);
      } }
  }
  if (alvo) *alvo = col;

  if (epg >= 0) {
    EpgProg ps[16];
    int n = epg_faixa(epg, ini, fimJ, ps, 16), k, desenhou = 0;
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
        float l = foc ? G_SUP_FOCO : atual ? G_SUP_NO_AR : G_SUP_FUTURO;
        if (focada && !foc) l += 0.018f;
        gfx_cor(b, raioB, l, l, l, a);
        continue;
      }
      // Rotulo so onde cabe: a regra da casa e desenhar MENOS rotulos, nunca
      // fonte menor. Titulo acima de 64 px, horario acima de 150.
      if (b.w > 64.0f) {
        int ct = foc ? 255 : atual ? 238 : 204;
        // O titulo do programa que comecou antes da janela comeca na borda
        // visivel dela. Texto vem DEPOIS do veu do passado, entao o do
        // programa no ar continua claro mesmo nascendo antes da linha agora.
        float tx = b.x + 14.0f;
        TxtLinha t = txt_linha_corta(TXT_CW_TITULO, ps[k].titulo, ct, ct, ct, 255,
                                     b.x + b.w - 14.0f - tx);
        if (h < 70.0f) {
          // Celula baixa (faixa): so o titulo, centrado, e a barra do quanto
          // ja passou no programa do ar — a regua em cima diz os horarios.
          txt_desenhar_alpha(t, tx, y + (h - (float)t.h) * 0.5f - 2.0f, a);
          if (atual && ps[k].fim > ps[k].ini) {
            float f = anim_clamp((float)(agoraT - ps[k].ini) / (float)(ps[k].fim - ps[k].ini), 0.0f, 1.0f);
            float bw = b.w - 28.0f;
            gfx_cor((GfxRect){ tx, y + h - 9.0f, bw, 3.0f }, 0.5f, 1, 1, 1, 0.16f * a);
            gfx_cor((GfxRect){ tx, y + h - 9.0f, bw * f, 3.0f }, 0.5f, 1, 1, 1, 0.75f * a);
          }
        } else
        txt_desenhar_alpha(t, tx, y + 7.0f, a);
        if (b.w > 120.0f && h >= 70.0f) {
          char h1[8], h2[8], faixa[24];
          int cm = foc ? 200 : 146;
          TxtLinha m;
          fmtHora(ps[k].ini, h1, sizeof h1); fmtHora(ps[k].fim, h2, sizeof h2);
          snprintf(faixa, sizeof faixa, "%s \xe2\x80\x93 %s", h1, h2);
          // Horario pela metade ("00:54 –…") nao informa: inteiro ou nada.
          m = txt_linha(TXT_DET_META2, faixa, cm, cm + 2, cm + 8, 255);
          if ((float)m.w <= b.x + b.w - 14.0f - tx)
            txt_desenhar_alpha(m, tx, y + 41.0f, a);
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
  int n = paN, i;
  ajustes_acento(&ar, &ag, &ab);

  { GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    gfx_cor(tela, 0.0f, 0, 0, 0, 0.45f * a); }
  // PAINEL FLUTUANTE como o de Salvos e a barra lateral (21/09/2026): solto
  // 24 px do topo e da base, cantos de 28 px pelo menor lado, translucido,
  // UMA luz difusa na cor de realce pelo canto superior direito, presa aos
  // cantos (GFX_LUZ). O veu de tela cheia acima ja e a primeira camada.
  { GfxRect p = { G_PA_X, 24.0f, G_PA_W, NV_TELA_H - 48.0f };
    gfx_cor(p, 28.0f / G_PA_W, 0.055f, 0.058f, 0.068f, 0.94f * a);
    gfx_luz_canto(p, 28.0f / G_PA_W, G_PA_W * 0.9f, -G_PA_W * 0.1f, G_PA_W * 0.65f, ar, ag, ab, 0.22f * a); }

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
    float raio = 12.0f / row.h;
    int f = i == paFoco;
    if (yi + row.h < y0 - 8.0f || yi > NV_TELA_H - 80.0f) continue;
    // Linha em repouso e linha em foco: as mesmas de addonsui.c. A em foco
    // ganha o brilho difuso por tras (0,9x a altura de folga), a luz das
    // pilulas do menu lateral.
    gfx_cor(row, raio, NV_COR_FOCO_R, NV_COR_FOCO_G, NV_COR_FOCO_B, 0.34f * a);
    if (f) { GfxRect luz = { row.x - row.h * 0.9f, row.y - row.h * 0.9f, row.w + row.h * 1.8f, row.h * 2.8f };
             gfx_rect(luz, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, ar, ag, ab, 0.35f * a); }
    if (f) gfx_cor(row, raio, ar, ag, ab, a);
    if (i < n) {
      int ai = paIdx[i];
      int sc = sabeCanal(addons_base(ai));
      int ligado = addons_ativo(ai);
      const char *sub = sc == 1 ? i18n("Fornece canais")
                      : sc == 0 ? i18n("Sem catálogo de canais")
                      : (fioVivo || recarregarPend) ? i18n("Carregando canais…")
                                : i18n("Ainda não conferido pelo guia");
      GfxRect pill = { x + w - 24.0f - 136.0f, yi + (row.h - 40.0f) * 0.5f, 136.0f, 40.0f };
      float txtW = pill.x - 24.0f - (x + 24.0f);
      { int tf = ajustes_tinta_foco();
        TxtLinha t = f ? txt_linha_corta(TXT_BODY, addons_nome(ai), tf, tf, tf, 255, txtW)
                       : txt_linha_corta(TXT_BODY, addons_nome(ai), 240, 241, 245, 255, txtW);
        txt_desenhar_alpha(t, x + 24.0f, yi + 12.0f, a); }
      { TxtLinha t = f ? txt_linha_corta(TXT_CAPTION, sub, 60, 62, 70, 255, txtW)
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
        float c = f ? 0.12f : 0.72f;
        gfx_rect(pill, 0, GFX_ANEL, 0, 0.05f, 0, 0.5f, c, c, c + 0.02f, 0.9f * a);
        { TxtLinha t = f ? txt_linha(TXT_CAPTION, i18n("Desligado"), 40, 42, 50, 255)
                         : txt_linha(TXT_CAPTION, i18n("Desligado"), 190, 192, 200, 255);
          txt_desenhar_alpha(t, pill.x + (pill.w - t.w) * 0.5f, pill.y + (pill.h - t.h) * 0.5f, a); }
      }
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
        TxtLinha t = f ? txt_linha_corta(TXT_BODY, rc->nome, ajustes_tinta_foco(), ajustes_tinta_foco(), ajustes_tinta_foco(), 255, nomeW)
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
        const char *desc = (ajustes_idioma_ingles() && rc->descEn[0]) ? rc->descEn : rc->desc;
        if (f) txt_bloco(TXT_CAPTION, desc, 60, 62, 70,     x + 24.0f, yi + 46.0f, txtW, 27.0f, a, 2);
        else   txt_bloco(TXT_CAPTION, desc, 150, 153, 162,  x + 24.0f, yi + 46.0f, txtW, 27.0f, a, 2);
      }
      if (inst) {
        TxtLinha t = f ? txt_linha(TXT_CAPTION, i18n("Instalado"), ajustes_tinta_foco2(), ajustes_tinta_foco2(), ajustes_tinta_foco2(), 255)
                       : txt_linha(TXT_CAPTION, i18n("Instalado"), 150, 153, 162, 255);
        txt_desenhar_alpha(t, pill.x + pill.w - t.w, pill.y + (pill.h - t.h) * 0.5f, a);
      } else {
        float c = f ? ajustes_tinta_foco() / 255.0f : 0.72f;
        gfx_rect(pill, 0, GFX_ANEL, 0, 0.05f, 0, 0.5f, c, c, c + 0.02f, 0.9f * a);
        { TxtLinha t = f ? txt_linha(TXT_CAPTION, i18n("Instalar"), ajustes_tinta_foco(), ajustes_tinta_foco(), ajustes_tinta_foco(), 255)
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
        i18n("OK liga, desliga ou instala  ·  Voltar volta ao guia"),
        140, 142, 150, 255, w);
    txt_desenhar_alpha(t, x, NV_TELA_H - 54.0f, a); }
}

// O anel que desliza. `alvo` em coordenadas de CONTEUDO (y + rolagem): a mola
// persegue o alvo com o dt do relogio do desenho, e a rolagem e subtraida so
// na hora de pintar — rolar a grade nao faz o anel "correr atras" da celula.
static void desenharAnelFoco(float a, Uint32 agora, float rol) {
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
  time_t tFoco = janelaDesl > 0 ? ini + 10 * 60 : agoraT;
  float topo = NV_TELA_H - 36.0f - (float)G_B_LINHAS * G_B_ROW - 42.0f - 44.0f;
  float yR = topo + 44.0f, y0 = yR + 42.0f, fimY = y0 + (float)G_B_LINHAS * G_B_ROW - 8.0f;
  int ll[G_B_LINHAS], cc[G_B_LINHAS], n = 0, k, passo, subiu = 0;
  int agoraVis = agoraT >= ini && agoraT < ini + (time_t)G_L_JANELA_MIN * 60;
  float agoraX = agoraVis ? G_L_FAIXA_X + (float)(agoraT - ini) / 60.0f
                            * (G_L_FAIXA_W / (float)G_L_JANELA_MIN) : 0.0f;
  float ar, ag, ab;
  if (nLinhas() < 1 || !linhaItem(focoLin, focoCol)) return;
  ajustes_acento(&ar, &ag, &ab);

  // Degrade: oito faixas sem sobreposicao, alfa subindo em curva. Meia tela
  // de preenchimento uma vez so.
  { float y = topo - 160.0f, hh = (NV_TELA_H - y) / 8.0f;
    for (k = 0; k < 8; k++) {
      float t = (float)(k + 1) / 8.0f;
      gfx_cor((GfxRect){ 0.0f, y + hh * (float)k, NV_TELA_W, hh + 1.0f }, 0.0f,
              0.02f, 0.02f, 0.025f, (0.10f + 0.84f * t * t) * a);
    } }

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

  { TxtLinha t = txt_linha(TXT_PG_ROTULO, i18n("Guia de canais"), 200, 202, 210, 255);
    TxtLinha d = txt_linha(TXT_CAPTION,
        i18n("OK troca de canal  ·  Azul: guia completo  ·  Voltar fecha"), 150, 153, 162, 255);
    txt_desenhar_alpha(t, G_AREA_X, topo + 6.0f, a);
    txt_desenhar_alpha(d, G_AREA_DIR - (float)d.w, topo + 6.0f, a); }
  desenharReguaEm(a, ini, yR);

  gCel = G_B_CEL;
  gIdNoAr = player_id_canal();
  focoAnelTem = 0;
  for (passo = 0; passo < 2; passo++) {
    for (k = 0; k < n; k++) {
      int foc = ll[k] == focoLin && cc[k] == focoCol;
      GfxRect alvo;
      float y = y0 + (float)k * G_B_ROW;
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
  desenharAnelFoco(a, agora, 0.0f);
  if (agoraVis) {
    char hora[8];
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

void guia_desenhar(Uint32 agora) {
  time_t agoraT = time(NULL);
  time_t tFoco = instanteFoco(agoraT);
  float a = entrada;
  int l, i, temLinhas;
  if (a < 0.01f || !guia_visivel()) return;
  if (overlay) { desenharBanda(a, agora); return; }
  temLinhas = (estado == G_PRONTO || estado == G_BAIXANDO) && nLinhas() > 0;

  // Fundo: opaco na tela cheia, quase opaco no overlay (o video continua
  // tocando atras — ve-se o movimento nas bordas, que e o que diz "a TV nao
  // parou").
  { GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    gfx_cor(tela, 0.0f, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B,
            overlay ? 0.94f * a : a); }

  // Cabecalho numa linha so: titulo e contagem a esquerda, chips e relogio a
  // direita. Era um titulo de 56 px com o subtitulo embaixo — 90 px de altura
  // que agora sao do heroi.
  { float chipsX = desenharTopo(a);
    char sub[160];
    TxtLinha t = txt_linha(TXT_HEADLINE,
                           overlay ? i18n("Guia de canais") : i18n("Guia TV"),
                           242, 243, 247, 255);
    float sx = G_AREA_X + (float)t.w + 24.0f;
    txt_desenhar_alpha(t, G_AREA_X, G_TOPO_Y + (G_TOPO_H - (float)t.h) * 0.5f, a);
    if (estado == G_BAIXANDO && nCanais > 0)   // lista do cache na tela, rede atras
      snprintf(sub, sizeof sub, i18n("%d canais · %d categorias · atualizando…"), nCanais, nCats);
    else if (estado == G_BAIXANDO)
      snprintf(sub, sizeof sub, "%s", i18n("Carregando canais…"));
    else if (falhas && !nCanais)
      snprintf(sub, sizeof sub, "%s",
               i18n("Os addons de canais não responderam."));
    else if (xtFalha && !nCanais)
      snprintf(sub, sizeof sub, "%s", xtFalha == XT_RECUSOU
               ? i18n("O Xtream recusou o usuário e a senha.")
               : i18n("A lista do Xtream não respondeu."));
    else if (estado == G_FALHOU || (fontesOk && !nFontes))
      snprintf(sub, sizeof sub, "%s",
               i18n("Nenhum canal: sem addon de canais e sem portal IPTV."));
    // A falha do Xtream toma o lugar da contagem simples, e so ela: os canais
    // dos addons continuam na tela e funcionam, o que falta e o portal.
    else if (xtFalha == XT_SEM_RESPOSTA)
      snprintf(sub, sizeof sub, i18n("%d canais · %d categorias · a lista do Xtream não respondeu"),
               nCanais, nCats);
    else if (xtFalha == XT_RECUSOU)
      snprintf(sub, sizeof sub, i18n("%d canais · %d categorias · o Xtream recusou o usuário e a senha"),
               nCanais, nCats);
    else
      snprintf(sub, sizeof sub, i18n("%d canais · %d categorias · segure %s para pular seção"),
               nCanais, nCats, "\xe2\x86\x91\xe2\x86\x93");
    { TxtLinha st = txt_linha_corta(TXT_DET_META2, sub, 150, 153, 162, 255,
                                    chipsX - 40.0f - sx);
      txt_desenhar_alpha(st, sx, G_TOPO_Y + (G_TOPO_H - (float)st.h) * 0.5f + 2.0f, a); } }

  if (temLinhas) desenharHero(a, agoraT, tFoco);

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
    int maisAbaixo = fimY > G_L_BASE + 1.0f, maisAcima = rolL > 1.0f;
    int passo;
    float ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    if (fimY > G_L_BASE) fimY = G_L_BASE;
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
        if (agoraX > G_L_FAIXA_X)
          gfx_cor((GfxRect){ G_L_FAIXA_X, G_L_TOPO - 8.0f, agoraX - G_L_FAIXA_X,
                             fimY - G_L_TOPO + 8.0f }, 0.0f,
                  NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 0.50f * a);
        gfx_cor((GfxRect){ agoraX - 1.0f, G_L_TOPO - 8.0f, 2.0f, fimY - G_L_TOPO + 8.0f },
                0.0f, ar, ag, ab, 0.85f * a);
      }
    }
    desenharAnelFoco(a, agora, rolL);
    // A grade SOME nas bordas em vez de ser cortada a faca, e so do lado em
    // que ha mais canal: tres faixas da cor do fundo, 12 px cada. Sao tres
    // retangulos pequenos por borda, nao um degrade de tela.
    { int k;
      float fa = (overlay ? 0.94f : 1.0f) * a;
      for (k = 0; k < 3; k++) {
        float al = (0.25f + 0.25f * (float)k) * fa;
        if (maisAbaixo)
          gfx_cor((GfxRect){ 0.0f, G_L_BASE - G_L_FADE + 12.0f * (float)k, NV_TELA_W, 12.0f },
                  0.0f, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, al);
        if (maisAcima)
          gfx_cor((GfxRect){ 0.0f, G_L_TOPO - 8.0f + G_L_FADE - 12.0f * (float)(k + 1),
                             NV_TELA_W, 12.0f },
                  0.0f, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, al);
      } }
    gfx_sem_recorte();
    // A cabeca da linha "agora": a hora numa pilula na regua.
    if (agoraVis) {
      char hora[8];
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
    gfx_recorte(0.0f, G_TOPO - 8.0f, NV_TELA_W, G_L_BASE - G_TOPO + 8.0f);
    for (l = 0; l < nLinhas(); l++) {
      float y = G_TOPO + (float)l * G_PASSO_Y - rolY;
      int n = linhaN(l);
      float dim = 1.0f;
      if (y > G_L_BASE || y + G_PASSO_Y < G_TOPO - 8.0f) continue;
      { char cab[140];
        snprintf(cab, sizeof cab, "%s  \xc2\xb7  %d", linhaNome(l), n);
        TxtLinha t = txt_linha_corta(TXT_ROW_TITULO, cab,
                                   220, 221, 224, 255,
                                   G_AREA_W);
        txt_desenhar_alpha(t, G_AREA_X, y, a * dim); }
      { float x = G_AREA_X - rolX[l];
        for (i = 0; i < n; i++, x += G_CARD_W + G_GAP_X) {
          if (x + G_CARD_W < 0.0f || x > NV_TELA_W) continue;
          desenharCard(linhaItem(l, i), x, y + G_HEAD_H,
                       l == focoLin && i == focoCol ? 1.0f : 0.0f,
                       a * dim, agoraT);
        } }
    }
    gfx_sem_recorte();

  } else if (estado == G_FALHOU || (fontesOk && !nFontes && estado != G_BAIXANDO)) {
    // DUAS FRASES, e a diferenca entre elas e a diferenca entre acusar a
    // pessoa e contar o que houve. `falhas` diz que alguem foi tentado e nao
    // respondeu; sem ele, a lista vazia e mesmo falta de addon.
    // DUAS PORTAS, e a mensagem tem de citar as duas. Ela dizia so "instale um
    // addon de canais" desde que o guia existe; com o portal IPTV entrando pela
    // mesma tela, quem configurou um portal e nao viu canal nenhum leria uma
    // frase que fala de outra coisa — e quem nao tem addon nem sabe que a
    // segunda porta existe.
    const char *msg = xtFalha == XT_SEM_RESPOSTA
      ? i18n("A lista de canais do Xtream não respondeu agora. O guia tenta de novo a cada 10 segundos enquanto esta tela estiver aberta.")
      : xtFalha == XT_RECUSOU
      ? i18n("O Xtream recusou o usuário e a senha. Confira o cadastro em Ajustes › Conta.")
      : falhas
      ? i18n("Os addons de canais desta conta não responderam agora. O guia tenta de novo a cada 10 segundos enquanto esta tela estiver aberta.")
      : i18n("O guia se enche por dois caminhos: um addon de canais (como o FrostView TV) instalado na conta, ou um portal IPTV cadastrado em Ajustes › Conta.");
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

  // (A gaveta de categorias vem depois da barra de ajuda, por cima de tudo.)

  // Barra de ajuda — a resposta ao "explicar no componente como abrir o
  // overlay": as teclas que o dono precisa lembrar ficam escritas na tela.
  // No modo lista ESQUERDA/DIREITA ganharam funcao (a janela de tempo) e a
  // barra tem de dizer. O amarelo nao aparece aqui de proposito: o controle
  // segmentado do cabecalho e o caminho que se ve, e a barra ja esta longa.
  { const char *dica = modoLista
      ? (overlay
         ? i18n("OK troca de canal  ·  segure OK = favorito  ·  \xe2\x86\x90\xe2\x86\x92 adianta a grade  ·  segure \xe2\x86\x91\xe2\x86\x93 pula seção  ·  Voltar ou Azul fecha")
         : i18n("OK assiste  ·  segure OK = favorito  ·  \xe2\x86\x90\xe2\x86\x92 adianta a grade  ·  segure \xe2\x86\x91\xe2\x86\x93 pula seção  ·  Voltar sai"))
      : (overlay
         ? i18n("OK troca de canal  ·  segure OK = favorito  ·  segure \xe2\x86\x91\xe2\x86\x93 pula seção  ·  Voltar ou Azul fecha")
         : i18n("OK assiste  ·  segure OK = favorito  ·  segure \xe2\x86\x91\xe2\x86\x93 pula seção  ·  Voltar sai"));
    TxtLinha t = txt_linha_corta(TXT_CAPTION, dica, 128, 130, 138, 255,
                                 G_AREA_W);
    txt_desenhar_alpha(t, G_AREA_X, NV_TELA_H - 48.0f, a); }

  desenharPainelCategorias(a);
  if (painel) desenharPainelAddons(a);
  // O CANAL NO AR ENCOLHENDO para o preview: o furo segue o degrau da janela
  // do player por cima de tudo, e o video "pousa" no preview.
  { float x, y, w, h;
    if (aberta && player_janela_animando(&x, &y, &w, &h))
      gfx_furo_raio((GfxRect){ x, y, w, h }, G_PREVIEW_RAIO / (h > 1.0f ? h : 1.0f)); }
}
