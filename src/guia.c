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
//     da duracao, regua de meias horas em cima e a linha "agora" em azul.
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
#include "dados.h"
#include "js.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "anim.h"
#include "layout.h"
#include "idioma.h"
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

// --- layout ---------------------------------------------------------------
// Painel de detalhe a direita, como a tela "Ver tudo" faz. As fileiras de
// canal ocupam o resto: cabecalho de categoria + cartao com logo, nome e a
// programacao no ar.
#define G_PAN_W    400.0f
#define G_PAN_X    (NV_TELA_W - NV_MARGEM_X - G_PAN_W)
#define G_AREA_X   NV_MARGEM_X
#define G_AREA_W   (G_PAN_X - 28.0f - G_AREA_X)
#define G_TOPO     178.0f
#define G_CARD_W   330.0f
#define G_CARD_H   226.0f
#define G_GAP_X     20.0f
#define G_HEAD_H    48.0f
#define G_PASSO_Y  (G_HEAD_H + G_CARD_H + 36.0f)
// A coluna de categorias do modo "salta secao": largura da faixa que sobe na
// esquerda enquanto o modo esta armado.
#define G_RAIL_W   300.0f
// Tempos do modo salta-categoria: segurar ~600 ms entra, 2 s parado sai.
// 450 ms e a folga entre repeticoes de KEYDOWN do firmware (ele manda uma
// tecla segurada como varios KEYDOWN com repeat=0).
#define G_HOLD_MS    600
#define G_REP_MS     450
#define G_CAT_SAIR_MS 2000

// Botao AMARELO do controle da LG. SUPOSTO (ver o cabecalho do arquivo):
// SDL_webOS.h enumera RED, GREEN, YELLOW, BLUE em sequencia e BLUE e 489.
#define G_SCANCODE_YELLOW 488

// --- layout do MODO LISTA -----------------------------------------------------
// Regua de horas em G_TOPO; as linhas comecam 40 px abaixo dela. Coluna de
// nome com 400 px: logo de 52 + nome em TXT_BODY (25 px) numa linha so, que
// cabe "Discovery Home & Health" sem cortar. O resto e a faixa de tempo.
#define G_L_TOPO    (G_TOPO + 40.0f)
#define G_L_ROW      76.0f     // 52 de logo + 12 de folga em cima e embaixo
#define G_L_HEAD     56.0f     // cabecalho de categoria (TXT_ROW_TITULO, 33 px)
#define G_L_COL     400.0f
#define G_L_FAIXA_X (G_AREA_X + G_L_COL + 16.0f)
#define G_L_FAIXA_W (G_AREA_W - G_L_COL - 16.0f)
// Janela de 120 min: a 916 px isso da 7,6 px por minuto, e um bloco de 5 min
// (o menor que aparece em grade de TV aberta) ainda tem 38 px — visivel,
// embora sem rotulo. 180 min deixaria o de 5 min com 25 px, um risco.
#define G_L_JANELA_MIN 120
#define G_L_PASSO_MIN   30     // regua de meia em meia hora, como toda grade
#define G_L_DESL_MAX   180     // ate 3 h a frente com DIREITA
#define G_L_BASE    (NV_TELA_H - 70.0f)   // acima da barra de ajuda

// --- cabecalho: controle segmentado + botao de addons -----------------------
#define G_TOPO_Y     52.0f
#define G_TOPO_H     48.0f
enum { G_TOPO_CARTOES = 0, G_TOPO_LISTA, G_TOPO_ADDONS, G_TOPO_N };

// --- painel de addons ---------------------------------------------------------
#define G_PA_W      720.0f
#define G_PA_X      (NV_TELA_W - G_PA_W)
#define G_PA_MARG    48.0f
#define G_PA_ROW     92.0f
#define G_MAX_REC    12

typedef struct {
  char id[80];
  char nome[140];
  char logo[480];
  char desc[600];
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
typedef struct { char base[600], tipo[16], id[96]; } GFonte;
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
  return cats[l - (nFavOrd > 0 ? 1 : 0)];
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
      nFontes++;
    }
  }
  fontesOk = 1;
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
static int lerPagina(const GFonte *f, int skip) {
  char url[1200];
  char *corpo;
  const char *p;
  int n = 0;
  if (skip > 0)
    snprintf(url, sizeof url, "%s/catalog/%s/%s/skip=%d.json",
             f->base, f->tipo, f->id, skip);
  else
    snprintf(url, sizeof url, "%s/catalog/%s/%s.json", f->base, f->tipo, f->id);
  corpo = rede_baixar(url, 15);
  if (!corpo) return 0;
  p = js_array(corpo, NULL, "metas");
  while (p && sNCanais < G_MAX_CANAL) {
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
      c.cat = sCatDe(gen[0] ? gen : "Outros");
      if (c.cat >= 0) sCanais[sNCanais++] = c;
    }
    p = js_prox(fim);
  }
  free(corpo);
  return n;
}

static int fonteJa(const char *base, const char *id) {
  for (int i = 0; i < sNFontes; i++)
    if (!strcmp(sFontes[i].base, base) && !strcmp(sFontes[i].id, id)) return 1;
  return 0;
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
    // ADDON DESLIGADO TAMBEM E LIDO, so que nao vira fonte. Custa um GET por
    // addon desligado (medido: 0 a 3 numa conta tipica) e e o que permite ao
    // painel dizer se vale a pena religa-lo para o guia.
    snprintf(url, sizeof url, "%s/manifest.json", base);
    corpo = rede_baixar(url, 15);
    if (sb) { snprintf(sb->base, sizeof sb->base, "%s", base); sb->canal = -1; sNSabe++; }
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
      char tipo[16] = "", id[96] = "";
      js_texto(p, f, "type", tipo, sizeof tipo);
      js_texto(p, f, "id", id, sizeof id);
      if (ehCanal(tipo)) temCanal = 1;
      if (ehCanal(tipo) && id[0] && !fonteJa(base, id)) {
        snprintf(sFontes[sNFontes].base, sizeof sFontes[sNFontes].base, "%s", base);
        snprintf(sFontes[sNFontes].tipo, sizeof sFontes[sNFontes].tipo, "%s", tipo);
        snprintf(sFontes[sNFontes].id,   sizeof sFontes[sNFontes].id,   "%s", id);
        sNFontes++;
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
  sNCanais = 0; sNCats = 0; sFalhas = 0;
  // Fontes das fileiras (descobertas no fio de desenho) primeiro — zero rede
  // extra. A sonda de manifestos completa com o que a home nao montou.
  sNFontes = 0;
  for (int i = 0; i < nFontes && sNFontes < G_MAX_FONTE; i++)
    sFontes[sNFontes++] = fontes[i];
  sondaManifestos();
  for (int i = 0; i < sNFontes; i++) {
    for (int skip = 0; sNCanais < G_MAX_CANAL; skip += G_PAGINA) {
      int n = lerPagina(&sFontes[i], skip);
      if (n <= 0) break;
      ok = 1;
      if (n < G_PAGINA) break;    // ultima pagina veio curta
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
  // FONTE ACHADA E NENHUMA PAGINA RESPONDEU tambem e "nao respondeu", e nao
  // "nao existe": e o caso do catalogo de canais que estoura o prazo com o
  // manifesto tendo vindo 200.
  if (sNFontes > 0 && !ok) sFalhas++;
  pendPronto = 1;
  if (!ok) estado = G_FALHOU;
  return NULL;
}

static void publicar(void) {
  int i, w;
  memcpy(canais, sCanais, sizeof(GCanal) * (size_t)sNCanais);
  memcpy(cats, sCats, sizeof sCats);
  nCanais = sNCanais; nCats = sNCats;
  // As fontes efetivas sao as do fio — fileiras + manifestos. Sem a copia, a
  // mensagem "Nenhum catalogo de canais" e o loop de re-tentativa liam a
  // contagem das fileiras apenas.
  memcpy(fontes, sFontes, sizeof sFontes);
  nFontes = sNFontes;
  falhas = sFalhas;
  memcpy(sabe, sSabe, sizeof sSabe);
  nSabe = sNSabe;
  // Ordena os canais por categoria (estavel na ordem de chegada) para que cada
  // fileira seja uma janela contigua — o mesmo desenho de CatFileira.
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
  (void)w;
  favAplicar();
  nFavOrd = 0;
  for (i = 0; i < nCanais && nFavOrd < G_MAX_FAV; i++)
    if (canais[i].fav) favOrd[nFavOrd++] = i;
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
  // Sem fonte achada, tenta de novo a cada chamada: a descoberta da home pode
  // nao ter montado as fileiras ainda quando o primeiro CH+/- chega.
  if (!fontesOk) descobrirFontes();
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

// Modo salta-categoria.
static int    modoCat;
static int    dirSeg;                 // SDLK_UP/DOWN segurado, 0 = solto
static Uint32 dirDesde, dirTick, ultNavCat;

// OK longo = favorito.
static Uint32 okDesde; static int okLongo;

// Foco no CABECALHO (controle segmentado + Addons). 0 = nas linhas.
static int   focoTopo, topoCol;
static float animTopo[G_TOPO_N];

// Modo lista: deslocamento da janela de tempo (min, multiplo de 30) e a
// rolagem vertical propria — a de cartoes anda em fileiras, esta em linhas de
// altura diferente, e misturar as duas numa mola so puxava a lista para o
// lugar errado na troca de modo.
static int   janelaDesl;
static float rolL, velL;

// Painel de addons por cima do guia.
static int   painel, paFoco, paMexeu;
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
  fontecache_engatilhar(antes ? antes->id : NULL, depois ? depois->id : NULL);
}

static void focoValido(void) {
  int l = nLinhas();
  if (focoLin >= l) focoLin = l - 1;
  if (focoLin < 0) focoLin = 0;
  if (focoCol >= linhaN(focoLin)) focoCol = linhaN(focoLin) - 1;
  if (focoCol < 0) focoCol = 0;
  engatilharVizinhos();
}

void guia_abrir(void) {
  guia_carregar();
  aberta = 1; querSair = 0; entrada = 0.0f;
  focoTopo = 0; painel = 0;
  focoValido();
}

void guia_overlay_abrir(void) {
  guia_carregar();
  overlay = 1; entrada = 0.0f;
  overlayDesde = SDL_GetTicks();
  focoTopo = 0; painel = 0;
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

static void pedirCanal(GCanal *c) {
  if (!c || pediuCanal) return;
  canalParaItem(c, &pedido);
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
// Itens do painel, em ordem: os addons da conta (indice = i em addons.c) e
// depois os recomendados (indice = n + k). Um vetor de posicoes Y por quadro
// e mais simples do que dois lacos com a mesma aritmetica de rolagem.
static int painelN(void) { return addons_n() + nRec; }

static void painelAbrir(void) {
  recLer();
  painel = 1; paFoco = 0; paMexeu = 0; paRol = 0.0f; paVelRol = 0.0f;
  paErro = -1;
  // O painel mostra o que o manifesto disse; se a sonda de Ajustes nunca
  // rodou, e barato pedi-la agora (uma vez por lista, ver addons.h).
  addons_sondar_manifestos();
}

// FECHAR E QUANDO O RESTO DO APP FICA SABENDO. Mesma regra de addonsui.c:
// desc_repetir() refaz o ciclo inteiro (~20 s na TV), entao ele roda uma vez
// por visita e nao uma vez por tecla. O guia tambem se recarrega: a lista de
// canais depende de quais addons estao ligados, e sem isso o addon recem
// desligado continuava enchendo a tela ate a proxima abertura.
static void painelFechar(void) {
  painel = 0;
  if (!paMexeu) return;
  paMexeu = 0;
  desc_repetir();
  if (fioVivo) recarregarPend = 1;
  else { estado = G_PARADO; ultTentativa = 0; iniciarCarga(); }
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
    paErro = addons_n() + k;
    paErroCheio = (addons_n() >= 16);
    return;
  }
  sync_sujar_addons();
  paMexeu = 1;
}

static void painelOk(void) {
  int n = addons_n();
  if (paFoco < n) {
    addons_alternar(paFoco);
    sync_sujar_addons();   // desligar aqui e desligar no celular tambem
    paMexeu = 1;
  } else if (paFoco - n < nRec && !recInstalado(&rec[paFoco - n])) {
    instalar(paFoco - n);
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
  if (overlay) overlay = 0;
  else { aberta = 0; querSair = 1; }
  modoCat = 0; dirSeg = 0; okDesde = 0; focoTopo = 0;
}

void guia_evento(const SDL_Event *e) {
  if (!guia_visivel()) return;

  if (e->type == SDL_KEYUP) {
    SDL_Keycode k = e->key.keysym.sym;
    if (k == SDLK_UP || k == SDLK_DOWN) dirSeg = 0;
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
  // No overlay, AZUL nunca navega: e a tecla que o abre e fecha. O BAIXO que
  // abriu o overlay NAO o fecha aqui — dentro dele, baixo navega como no guia
  // (o Voltar e a saida). O repouso de G_OVERLAY_REP_MS ignora a repeticao do
  // firmware do botao que o abriu, senao segurar o azul fechava na hora.
  if (overlay && (k == SDLK_s || e->key.keysym.scancode == NV_SCANCODE_BLUE)) {
    if (agora - overlayDesde < G_OVERLAY_REP_MS) return;
    sair(); return;
  }

  // O painel de addons captura tudo que nao e Voltar (tratado acima).
  if (painel) { painelEvento(k, agora); return; }

  // AMARELO alterna lista e cartoes de qualquer lugar. `l` no teclado do Mac
  // e o equivalente de bancada, como `s` e do azul.
  if (e->key.keysym.scancode == G_SCANCODE_YELLOW || k == SDLK_l) {
    dirSeg = 0; modoCat = 0;
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
    dirSeg = 0; modoCat = 0;
    if (k == SDLK_LEFT)  { if (topoCol > 0) topoCol--; return; }
    if (k == SDLK_RIGHT) { if (topoCol + 1 < G_TOPO_N) topoCol++; return; }
    if (k == SDLK_DOWN)  { focoTopo = 0; return; }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
      if (topoCol == G_TOPO_ADDONS) painelAbrir();
      else if ((topoCol == G_TOPO_LISTA) != modoLista) alternarModo();
      return;
    }
    return;
  }

  if (k == SDLK_UP || k == SDLK_DOWN) {
    int dir = (k == SDLK_DOWN) ? 1 : -1;
    int fresco = !(dirSeg == k && agora - dirTick < G_REP_MS);
    if (modoCat) {
      saltarCat(dir);
      ultNavCat = agora; dirTick = agora;
      return;
    }
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
        modoCat = 1;
        saltarCat(dir);
        ultNavCat = agora; dirTick = agora;
        return;
      }
    } else { dirSeg = k; dirDesde = agora; }
    dirTick = agora;
    if (modoLista) moverLista(dir); else moverVertical(dir);
    return;
  }
  // Qualquer outra tecla desarma os dois estados de direcao segurada.
  dirSeg = 0; modoCat = 0;

  // No modo lista ESQUERDA/DIREITA andam a JANELA DE TEMPO, meia hora por
  // toque, ate 3 h a frente — a pergunta "o que passa mais tarde" que a
  // grade tradicional responde e o cartao nao.
  if (modoLista) {
    if (k == SDLK_LEFT)  { if (janelaDesl > 0) janelaDesl -= G_L_PASSO_MIN; return; }
    if (k == SDLK_RIGHT) { if (janelaDesl < G_L_DESL_MAX) janelaDesl += G_L_PASSO_MIN; return; }
  } else {
    if (k == SDLK_LEFT)  { if (focoCol > 0) focoCol--; return; }
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
  int n = addons_n();
  if (i < n) return 36.0f + (float)i * G_PA_ROW;
  return 36.0f + (float)n * G_PA_ROW + 28.0f + 36.0f + 60.0f + (float)(i - n) * G_PA_ROW;
}
#define G_PA_LISTA_Y 200.0f

void guia_atualizar(float dt, Uint32 agora) {
  entrada = anim_mola(entrada, guia_visivel() ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  if (pendPronto) { publicar(); estado = G_PRONTO; pendPronto = 0; fioVivo = 0; focoValido(); }
  // O painel de addons pediu recarga com o fio ainda vivo: agora que ele
  // acabou, vai.
  if (recarregarPend && !fioVivo) {
    recarregarPend = 0; estado = G_PARADO; ultTentativa = 0; iniciarCarga();
  }
  if (!guia_visivel()) return;

  for (int i = 0; i < G_TOPO_N; i++)
    animTopo[i] = anim_mola(animTopo[i], focoTopo && topoCol == i ? 1.0f : 0.0f,
                            dt, NV_MOLA_FOCO);

  // Rolagem do modo lista: a linha focada fica a duas linhas do topo, com o
  // cabecalho da categoria dela visivel quando ela e a primeira — e o mesmo
  // criterio de "a anterior inteira como contexto" da rolagem de cartoes.
  if (modoLista && nLinhas() > 0) {
    float areaH = G_L_BASE - G_L_TOPO;
    float alvo = listaYDe(focoLin, focoCol) - G_L_HEAD - 2.0f * G_L_ROW;
    float maxY = listaAltura() - areaH;
    if (maxY < 0.0f) maxY = 0.0f;
    if (alvo > maxY) alvo = maxY;
    if (alvo < 0.0f) alvo = 0.0f;
    rolL = anim_mola2(&velL, rolL, alvo, dt, NV_MOLA_SCROLL);
  }
  if (painel) {
    float areaH = NV_TELA_H - 80.0f - G_PA_LISTA_Y;
    float alvo = paItemY(paFoco) - areaH * 0.4f;
    float maxY = (painelN() > 0 ? paItemY(painelN() - 1) + G_PA_ROW : 0.0f) - areaH;
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
  if (modoCat && agora - ultNavCat > G_CAT_SAIR_MS) modoCat = 0;

  // OK segurado = favorito.
  if (okDesde && !okLongo && agora - okDesde >= NV_HOLD_MS) {
    GCanal *c = linhaItem(focoLin, focoCol);
    if (c) favAlternar(c);
    okLongo = 1;
  }

  // Rolagem vertical: a fileira focada ancora uma fileira abaixo do topo da
  // area, alinhada no limite de fileira. Assim a fileira anterior fica
  // INTEIRA visivel (cabecalho + cartoes) como contexto — antes a mira de
  // ~1/3 deixava o cabecalho dela cortado na borda do recorte, como se a
  // fileira estivesse "fora da lista".
  { float alvo = (float)(focoLin > 0 ? focoLin - 1 : 0) * G_PASSO_Y;
    float maxY = (float)nLinhas() * G_PASSO_Y - (NV_TELA_H - G_TOPO) + 80.0f;
    if (alvo < 0.0f) alvo = 0.0f;
    if (maxY < 0.0f) maxY = 0.0f;
    if (alvo > maxY) {
      alvo = maxY;
      // O clamp do fim desalinha as fileiras: se a borda de cima do recorte
      // cai no meio de um cabecalho de categoria, rola o restinho para
      // esconde-lo de vez (o espaco que sobra embaixo absorve a folga).
      { float corte = alvo - 20.0f;
        if (corte > 0.0f) {
          float resto = corte - (float)(int)(corte / G_PASSO_Y) * G_PASSO_Y;
          if (resto > 0.0f && resto < G_HEAD_H) alvo += G_HEAD_H - resto;
        } }
    }
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

// Desenha o logo do canal na caixa `cx`, sem azulejo. `tom` e a cor do logo
// recortado (claro sobre cartao escuro, escuro sobre cartao claro).
static void desenharLogo(const char *logo, GfxRect cx, float lado, float tom,
                         float a) {
  GLuint t;
  float ap, w, h, fr, fg, fb;
  int comFundo;
  if (!logo || !logo[0]) return;
  t = tex_obter_larg(logo, cx.w);
  ap = tex_aspecto(logo);
  if (!t || ap <= 0.0f) return;
  w = lado; h = w / ap;
  if (h > lado) { h = lado; w = h * ap; }
  comFundo = tex_cor_fundo(logo, &fr, &fg, &fb) == 1;
  { GfxRect lr = { cx.x + (cx.w - w) * 0.5f, cx.y + (cx.h - h) * 0.5f, w, h };
    gfx_tex_aspect_atual = 0.0f;
    // O LOGO COM FUNDO PROPRIO E UM QUADRADO, e quadrado dentro de cartao
    // arredondado aparece — foi o que o dono viu no HBO Max. GFX_ARTE e o
    // GFX_TEXTO com a mascara dos cantos: mesmo RGB, mesmo alpha, so recortado.
    // O raio e o do cartao (NV_RAIO_CARD e fracao da ALTURA do retangulo, nao
    // pixel), entao o canto do logo acompanha o canto do cartao em vez de ter
    // um raio proprio que brigaria com ele.
    //
    // O recortado nao passa por aqui: ele nao tem fundo para arredondar, e a
    // forma dele ja vem do alpha do arquivo.
    if (comFundo) gfx_rect(lr, t, GFX_ARTE, 0, 0, 0, NV_RAIO_CARD, 1, 1, 1, a);
    else          gfx_rect(lr, t, GFX_MARCA, 0, 0, 0, 0.0f, tom, tom, tom, a);
  }
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
    desenharLogo(c->logo, cx, 80.0f, escuro ? G_LOGO_ESC : G_LOGO_CLARO, a); }

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

// Painel da direita: a ficha do canal em foco — logo grande, descricao do
// addon e os proximos tres programas.
static void desenharPainel(float a, time_t agoraT) {
  GCanal *c = linhaItem(focoLin, focoCol);
  float y = G_TOPO + 6.0f;
  if (!c) return;

  GfxRect painel = { G_PAN_X - 24.0f, 0.0f,
                     NV_TELA_W - G_PAN_X + 24.0f, NV_TELA_H };
  gfx_cor(painel, 0.0f, 0.045f, 0.047f, 0.055f, 0.85f * a);

  // Logo grande, na MESMA regra do cartao: sem azulejo, claro sobre o painel
  // escuro. Duas regras diferentes para o mesmo logo em duas telas vizinhas
  // era o que fazia o Paramount+ sumir de uma e aparecer na outra.
  { GfxRect cx = { G_PAN_X + (G_PAN_W - 168.0f) * 0.5f, y, 168.0f, 168.0f };
    desenharLogo(c->logo, cx, 148.0f, G_LOGO_CLARO, a);
    y += 168.0f + 20.0f; }

  { TxtLinha t = txt_linha_corta(TXT_PAINEL_TITULO, c->nome, 245, 246, 250, 255,
                               G_PAN_W);
    txt_desenhar_alpha(t, G_PAN_X, y, a); y += t.h + 6.0f; }
  { char gen[160];
    snprintf(gen, sizeof gen, "%s%s", c->fav ? "\xe2\x98\x85 " : "",
             c->cat >= 0 ? cats[c->cat] : "");
    if (gen[0]) {
      TxtLinha t = txt_linha_corta(TXT_CAPTION, gen, 148, 200, 255, 255, G_PAN_W);
      txt_desenhar_alpha(t, G_PAN_X, y, a); y += t.h + 8.0f;
    } }
  if (c->desc[0])
    y += txt_bloco(TXT_CAPTION, c->desc, 190, 192, 200, G_PAN_X, y, G_PAN_W,
                   30.0f, a * 0.9f, 4) + 10.0f;

  // Programa no ar.
  { int epg = epgDo(c);
    EpgProg p;
    if (epg >= 0 && epg_agora(epg, agoraT, &p)) {
      char h1[8], h2[8], faixa[48];
      TxtLinha l;
      fmtHora(p.ini, h1, sizeof h1); fmtHora(p.fim, h2, sizeof h2);
      l = txt_linha(TXT_MINI, i18n("NO AR AGORA"), 148, 200, 255, 255);
      txt_desenhar_alpha(l, G_PAN_X, y, a); y += l.h + 4.0f;
      l = txt_linha_corta(TXT_PAINEL_ITEM, p.titulo, 240, 241, 245, 255, G_PAN_W);
      txt_desenhar_alpha(l, G_PAN_X, y, a); y += l.h + 2.0f;
      snprintf(faixa, sizeof faixa, "%s \xe2\x80\x93 %s", h1, h2);
      l = txt_linha(TXT_CAPTION, faixa, 160, 162, 170, 255);
      txt_desenhar_alpha(l, G_PAN_X, y, a); y += l.h + 8.0f;
      { float f = (p.fim > p.ini)
                  ? anim_clamp((float)(agoraT - p.ini) / (float)(p.fim - p.ini), 0.0f, 1.0f)
                  : 0.0f;
        GfxRect tr = { G_PAN_X, y, G_PAN_W, 5.0f };
        GfxRect an = { tr.x, tr.y, tr.w * f, tr.h };
        gfx_cor(tr, 0.5f, 1, 1, 1, 0.16f * a);
        gfx_cor(an, 0.5f, 0.36f, 0.64f, 1.0f, a); }
      y += 24.0f;

      { int k, mostrou = 0;
        TxtLinha h = txt_linha(TXT_MINI, i18n("A SEGUIR"), 148, 200, 255, 255);
        txt_desenhar_alpha(h, G_PAN_X, y, a); y += h.h + 6.0f;
        for (k = 0; k < 3; k++) {
          char linha[300], hh[8];
          if (!epg_proximo(epg, agoraT, k, &p)) break;
          fmtHora(p.ini, hh, sizeof hh);
          snprintf(linha, sizeof linha, "%s  %s", hh, p.titulo);
          { TxtLinha t = txt_linha_corta(TXT_CAPTION, linha, 210, 211, 218, 255,
                                       G_PAN_W);
            txt_desenhar_alpha(t, G_PAN_X, y, a); y += t.h + 6.0f; }
          mostrou = 1;
        }
        if (!mostrou) {
          TxtLinha t = txt_linha(TXT_CAPTION, i18n("Sem próximos programas"),
                               150, 152, 160, 255);
          txt_desenhar_alpha(t, G_PAN_X, y, a);
        } }
    } else if (epg == -1) {
      TxtLinha t = txt_linha(TXT_CAPTION, i18n("Carregando programação…"),
                           150, 152, 160, 255);
      txt_desenhar_alpha(t, G_PAN_X, y, a);
    } else {
      // epg == -2 (sem grade real) ou casado mas sem nada no ar agora.
      TxtLinha t = txt_linha(TXT_CAPTION, i18n("Canal ao vivo, sem grade de programação"),
                           150, 152, 160, 255);
      txt_desenhar_alpha(t, G_PAN_X, y, a);
    } }
}

// A coluna de categorias do modo salta-secao. Ela existe so enquanto o modo
// esta armado — e a resposta visual ao "estou pulando de secao em secao".
static void desenharRailCategorias(float a) {
  int nl = nLinhas(), primeiro, i;
  float y, alturaLinha = 44.0f;
  int vis = (int)((NV_TELA_H - 260.0f) / alturaLinha);
  if (nl < 1) return;
  primeiro = focoLin - vis / 2;
  if (primeiro < 0) primeiro = 0;
  if (primeiro + vis > nl) primeiro = nl - vis;
  if (primeiro < 0) primeiro = 0;

  GfxRect fundo = { 0.0f, 0.0f, G_AREA_X + G_RAIL_W + 20.0f, NV_TELA_H };
  gfx_cor(fundo, 0.0f, 0.03f, 0.032f, 0.04f, 0.92f * a);

  { TxtLinha t = txt_linha(TXT_MINI, i18n("PULANDO CATEGORIAS"), 148, 200, 255, 255);
    txt_desenhar_alpha(t, G_AREA_X, 200.0f, a); }

  y = 240.0f;
  for (i = primeiro; i < nl && i < primeiro + vis; i++, y += alturaLinha) {
    int at = (i == focoLin);
    char rot[120];
    snprintf(rot, sizeof rot, "%s  \xc2\xb7  %d", linhaNome(i), linhaN(i));
    if (at) {
      GfxRect pill = { G_AREA_X - 8.0f, y - 6.0f, G_RAIL_W, alturaLinha - 8.0f };
      gfx_cor(pill, 0.30f, 0.20f, 0.22f, 0.25f, a);
    }
    { TxtLinha t = txt_linha_corta(TXT_CAPTION, rot,
                                 at ? 255 : 150, at ? 255 : 152,
                                 at ? 255 : 160, 255, G_RAIL_W - 16.0f);
      txt_desenhar_alpha(t, G_AREA_X + 8.0f, y + 4.0f, a); }
  }
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

// --- cabecalho: Cartoes | Lista | Addons ---------------------------------------
// Mesmo vocabulario de botao do resto do app (ver addonsui.c e NV_COR_FOCO):
// repouso = preenchido no cinza de superficie, foco = preenchido na cor de
// realce com texto escuro, sem anel. O segmento SELECIONADO sem foco fica no
// cinza de repouso; o nao selecionado fica so texto, em terciario — e a
// diferenca entre "onde estou" e "para onde posso ir".
static void desenharTopo(float a) {
  const char *rot[G_TOPO_N];
  float w[G_TOPO_N], x, ar, ag, ab;
  int i;
  rot[G_TOPO_CARTOES] = i18n("Cartões");
  rot[G_TOPO_LISTA]   = i18n("Lista");
  rot[G_TOPO_ADDONS]  = i18n("Addons");
  ajustes_acento(&ar, &ag, &ab);
  for (i = 0; i < G_TOPO_N; i++)
    w[i] = txt_linha(TXT_BODY, rot[i], 255, 255, 255, 255).w + 44.0f;
  // Da direita para a esquerda, encostado na borda do painel de detalhe. O
  // botao Addons fica 20 px separado do par, para ler como outra coisa.
  x = G_PAN_X - 28.0f;
  for (i = G_TOPO_N - 1; i >= 0; i--) {
    float f = animTopo[i];
    int sel = i < G_TOPO_ADDONS ? ((i == G_TOPO_LISTA) == modoLista) : 1;
    int escuro = f > 0.5f;
    GfxRect r;
    x -= w[i];
    r.x = x; r.y = G_TOPO_Y; r.w = w[i]; r.h = G_TOPO_H;
    if (sel) gfx_cor(r, 0.5f, NV_COR_FOCO_R, NV_COR_FOCO_G, NV_COR_FOCO_B, a);
    if (f > 0.01f) gfx_cor(r, 0.5f, ar, ag, ab, f * a);
    { TxtLinha t = escuro ? txt_linha(TXT_BODY, rot[i], 20, 21, 25, 255)
                  : sel   ? txt_linha(TXT_BODY, rot[i], 240, 241, 245, 255)
                          : txt_linha(TXT_BODY, rot[i], 150, 153, 162, 255);
      txt_desenhar_alpha(t, r.x + (r.w - t.w) * 0.5f, r.y + (r.h - t.h) * 0.5f, a); }
    x -= (i == G_TOPO_ADDONS) ? 20.0f : 6.0f;
  }
}

// --- modo lista ---------------------------------------------------------------------
// Inicio da janela de tempo: a meia hora cheia anterior a agora, mais o
// deslocamento pedido com DIREITA. Regua em meias horas porque e assim que a
// grade de TV sempre foi lida; o olho ja sabe onde procurar.
static time_t janelaIni(time_t agoraT) {
  return (agoraT / 1800) * 1800 + (time_t)janelaDesl * 60;
}

static void desenharRegua(float a, time_t ini) {
  float ppm = G_L_FAIXA_W / (float)G_L_JANELA_MIN;
  int i;
  for (i = 0; i <= G_L_JANELA_MIN / G_L_PASSO_MIN; i++) {
    char h[8];
    float x = G_L_FAIXA_X + (float)(i * G_L_PASSO_MIN) * ppm;
    TxtLinha t;
    fmtHora(ini + (time_t)i * G_L_PASSO_MIN * 60, h, sizeof h);
    t = txt_linha(TXT_CAPTION, h, 150, 153, 162, 255);
    // O ultimo rotulo alinha pela direita para nao vazar da faixa.
    txt_desenhar_alpha(t, i == G_L_JANELA_MIN / G_L_PASSO_MIN ? x - t.w : x,
                       G_TOPO + 2.0f, a);
    { GfxRect tick = { x - (i == G_L_JANELA_MIN / G_L_PASSO_MIN ? 1.0f : 0.0f),
                       G_TOPO + 30.0f, 1.0f, 8.0f };
      gfx_cor(tick, 0.0f, 1, 1, 1, 0.22f * a); }
  }
  { GfxRect linha = { G_L_FAIXA_X, G_TOPO + 37.0f, G_L_FAIXA_W, 1.0f };
    gfx_cor(linha, 0.0f, 1, 1, 1, 0.10f * a); }
}

// A linha de um canal no modo lista. As duas cores do cartao valem aqui
// (decisao do dono de 16/09: quase preto sem foco, branco com foco). Os
// blocos de programa seguem a escada de superficies do DESIGN.md: trilho
// mais claro que a linha, bloco do programa no ar mais claro que o trilho, os
// seguintes entre os dois. Sobre a linha branca em foco a escada inverte.
static void desenharLinhaLista(GCanal *c, float y, float foco, float a,
                               time_t agoraT, time_t ini) {
  GfxRect r = { G_AREA_X, y, G_AREA_W, G_L_ROW - 6.0f };
  float lum = 0.075f;
  float raioL = 10.0f / r.h;           // 10 px, em fracao da ALTURA (gfx.h)
  int escuro = foco > 0.5f;
  int epg = epgDo(c);
  gfx_cor(r, raioL, lum, lum, lum + 0.01f, a);
  if (foco > 0.01f) gfx_cor(r, raioL, G_LOGO_CLARO, G_LOGO_CLARO,
                            G_LOGO_CLARO + 0.004f, foco * a);

  { GfxRect cx = { G_AREA_X + 12.0f, y + 9.0f, 52.0f, 52.0f };
    desenharLogo(c->logo, cx, 44.0f, escuro ? G_LOGO_ESC : G_LOGO_CLARO, a); }
  { float tw = G_L_COL - 84.0f - (c->fav ? 34.0f : 0.0f);
    TxtLinha t = escuro ? txt_linha_corta(TXT_BODY, c->nome, 20, 21, 25, 255, tw)
                        : txt_linha_corta(TXT_BODY, c->nome, 240, 241, 245, 255, tw);
    txt_desenhar_alpha(t, G_AREA_X + 78.0f, y + (r.h - t.h) * 0.5f, a);
    if (c->fav) {
      TxtLinha s = txt_linha(TXT_CAPTION, "\xe2\x98\x85", 255, 214, 90, 255);
      txt_desenhar_alpha(s, G_AREA_X + G_L_COL - 34.0f, y + (r.h - s.h) * 0.5f, a);
    } }

  // Faixa de tempo.
  { GfxRect trilho = { G_L_FAIXA_X, y + 9.0f, G_L_FAIXA_W, 52.0f };
    float raioB = 8.0f / trilho.h;
    if (escuro) gfx_cor(trilho, raioB, 0.88f, 0.885f, 0.90f, a);
    else        gfx_cor(trilho, raioB, 0.125f, 0.13f, 0.14f, a);
    if (epg >= 0) {
      time_t fimJ = ini + (time_t)G_L_JANELA_MIN * 60;
      float ppm = G_L_FAIXA_W / (float)G_L_JANELA_MIN;
      EpgProg p;
      int k;
      // k = -1 e o programa NO AR; 0.. sao os seguintes. Para de pedir quando
      // um comeca depois do fim da janela — a grade e ordenada por hora.
      for (k = -1; k < 12; k++) {
        int ok = k < 0 ? epg_agora(epg, agoraT, &p) : epg_proximo(epg, agoraT, k, &p);
        time_t i0, i1;
        float x1, x2;
        int atual;
        if (!ok) { if (k < 0) continue; break; }
        if (p.ini >= fimJ) break;
        if (p.fim <= ini) continue;
        i0 = p.ini > ini ? p.ini : ini;
        i1 = p.fim < fimJ ? p.fim : fimJ;
        x1 = G_L_FAIXA_X + (float)(i0 - ini) / 60.0f * ppm;
        x2 = G_L_FAIXA_X + (float)(i1 - ini) / 60.0f * ppm;
        if (x2 - x1 < 6.0f) continue;
        atual = p.ini <= agoraT && agoraT < p.fim;
        { GfxRect b = { x1 + 2.0f, trilho.y, x2 - x1 - 4.0f, trilho.h };
          if (escuro) { if (atual) gfx_cor(b, raioB, 0.11f, 0.115f, 0.13f, a);
                        else       gfx_cor(b, raioB, 0.78f, 0.79f, 0.81f, a); }
          else        { if (atual) gfx_cor(b, raioB, 0.22f, 0.225f, 0.25f, a);
                        else       gfx_cor(b, raioB, 0.155f, 0.16f, 0.175f, a); }
          // Bloco estreito NAO recebe rotulo: a regra da casa e desenhar menos
          // rotulos, nunca fonte menor que 22 px. 72 px cabem "Jornal…".
          if (b.w > 72.0f) {
            int ct = escuro ? (atual ? 240 : 30) : (atual ? 240 : 190);
            TxtLinha t = txt_linha_corta(TXT_CAPTION, p.titulo, ct, ct, ct, 255,
                                         b.w - 24.0f);
            txt_desenhar_alpha(t, b.x + 12.0f, b.y + (b.h - t.h) * 0.5f, a);
          } }
      }
    } else {
      // -2 = sem grade real; -1 = a grade ainda nao chegou. Dizer qual dos
      // dois e o que separa "nao tem" de "espere".
      int ct = escuro ? 90 : 120;
      TxtLinha t = txt_linha(TXT_CAPTION,
                             epg == -2 ? i18n("Sem grade de programação")
                                       : i18n("Carregando programação…"),
                             ct, ct + 2, ct + 8, 255);
      txt_desenhar_alpha(t, trilho.x + 16.0f, trilho.y + (trilho.h - t.h) * 0.5f, a);
    } }
}

// --- painel de addons -------------------------------------------------------------
static void desenharPainelAddons(float a) {
  float x = G_PA_X + G_PA_MARG, w = G_PA_W - 2.0f * G_PA_MARG;
  float ar, ag, ab, y0 = G_PA_LISTA_Y;
  int n = addons_n(), i;
  ajustes_acento(&ar, &ag, &ab);

  { GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    gfx_cor(tela, 0.0f, 0, 0, 0, 0.45f * a); }
  { GfxRect p = { G_PA_X, 0, G_PA_W, NV_TELA_H };
    gfx_cor(p, 0.0f, 0.106f, 0.110f, 0.122f, 0.98f * a); }   /* #1B1C1F */

  { TxtLinha t = txt_linha(TXT_HEADLINE, i18n("Addons de canais"), 240, 242, 248, 255);
    txt_desenhar_alpha(t, x, 64.0f, a); }
  txt_bloco(TXT_CAPTION,
            i18n("Ligar ou desligar aqui vale para o app inteiro, não só para o guia."),
            150, 153, 162, x, 118.0f, w, 28.0f, a, 2);

  gfx_recorte(G_PA_X, y0 - 8.0f, G_PA_W, NV_TELA_H - 80.0f - y0 + 8.0f);

  { TxtLinha t = txt_linha(TXT_CAPTION, i18n("NA SUA CONTA"), 148, 200, 255, 255);
    txt_desenhar_alpha(t, x, y0 - paRol, a); }
  if (n == 0) {
    TxtLinha t = txt_linha(TXT_CAPTION, i18n("Nenhum addon nesta conta."), 150, 153, 162, 255);
    txt_desenhar_alpha(t, x, y0 + 36.0f - paRol, a);
  }
  for (i = 0; i < painelN(); i++) {
    float yi = y0 + paItemY(i) - paRol;
    GfxRect row = { x, yi, w, G_PA_ROW - 8.0f };
    float raio = 12.0f / row.h;
    int f = i == paFoco;
    if (yi + row.h < y0 - 8.0f || yi > NV_TELA_H - 80.0f) continue;
    // Linha em repouso e linha em foco: as mesmas de addonsui.c.
    gfx_cor(row, raio, NV_COR_FOCO_R, NV_COR_FOCO_G, NV_COR_FOCO_B, 0.34f * a);
    if (f) gfx_cor(row, raio, ar, ag, ab, a);
    if (i < n) {
      int sc = sabeCanal(addons_base(i));
      int ligado = addons_ativo(i);
      const char *sub = sc == 1 ? i18n("Fornece canais")
                      : sc == 0 ? i18n("Sem catálogo de canais")
                                : i18n("Ainda não conferido pelo guia");
      GfxRect pill = { x + w - 24.0f - 136.0f, yi + (row.h - 40.0f) * 0.5f, 136.0f, 40.0f };
      { TxtLinha t = f ? txt_linha_corta(TXT_BODY, addons_nome(i), 20, 21, 25, 255, w - 200.0f)
                       : txt_linha_corta(TXT_BODY, addons_nome(i), 240, 241, 245, 255, w - 200.0f);
        txt_desenhar_alpha(t, x + 24.0f, yi + 12.0f, a); }
      { TxtLinha t = f ? txt_linha_corta(TXT_CAPTION, sub, 60, 62, 70, 255, w - 200.0f)
                       : txt_linha_corta(TXT_CAPTION, sub, 150, 153, 162, 255, w - 200.0f);
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
      { TxtLinha t = f ? txt_linha_corta(TXT_BODY, rc->nome, 20, 21, 25, 255, w - 200.0f)
                       : txt_linha_corta(TXT_BODY, rc->nome, 240, 241, 245, 255, w - 200.0f);
        txt_desenhar_alpha(t, x + 24.0f, yi + 12.0f, a); }
      if (paErro == i) {
        const char *m = paErroCheio ? i18n("Não coube: a conta já tem o máximo de addons")
                                    : i18n("Não foi possível instalar");
        TxtLinha t = txt_linha_corta(TXT_CAPTION, m, 237, 77, 77, 255, w - 200.0f);
        txt_desenhar_alpha(t, x + 24.0f, yi + 48.0f, a);
      } else if (rc->desc[0]) {
        // Em ingles usa a quarta coluna se existir; senao a portuguesa, que e
        // melhor que linha vazia.
        const char *desc = (ajustes_idioma_ingles() && rc->descEn[0]) ? rc->descEn : rc->desc;
        TxtLinha t = f ? txt_linha_corta(TXT_CAPTION, desc, 60, 62, 70, 255, w - 200.0f)
                       : txt_linha_corta(TXT_CAPTION, desc, 150, 153, 162, 255, w - 200.0f);
        txt_desenhar_alpha(t, x + 24.0f, yi + 48.0f, a);
      }
      if (inst) {
        TxtLinha t = f ? txt_linha(TXT_CAPTION, i18n("Instalado"), 60, 62, 70, 255)
                       : txt_linha(TXT_CAPTION, i18n("Instalado"), 150, 153, 162, 255);
        txt_desenhar_alpha(t, pill.x + pill.w - t.w, pill.y + (pill.h - t.h) * 0.5f, a);
      } else {
        float c = f ? 0.12f : 0.72f;
        gfx_rect(pill, 0, GFX_ANEL, 0, 0.05f, 0, 0.5f, c, c, c + 0.02f, 0.9f * a);
        { TxtLinha t = f ? txt_linha(TXT_CAPTION, i18n("Instalar"), 20, 21, 25, 255)
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

void guia_desenhar(Uint32 agora) {
  time_t agoraT = time(NULL);
  float a = entrada;
  int l, i;
  (void)agora;
  if (a < 0.01f || !guia_visivel()) return;

  // Fundo: opaco na tela cheia, quase opaco no overlay (o video continua
  // tocando atras — ve-se o movimento nas bordas, que e o que diz "a TV nao
  // parou").
  { GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    gfx_cor(tela, 0.0f, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B,
            overlay ? 0.94f * a : a); }

  // Cabecalho.
  { TxtLinha t = txt_linha(TXT_TITULO2,
                           overlay ? i18n("Guia de canais") : i18n("Guia TV"),
                           242, 243, 247, 255);
    txt_desenhar_alpha(t, G_AREA_X, 44.0f, a); }
  { char sub[160];
    if (estado == G_BAIXANDO)
      snprintf(sub, sizeof sub, "%s", i18n("Carregando canais…"));
    else if (falhas && !nCanais)
      snprintf(sub, sizeof sub, "%s",
               i18n("Os addons de canais não responderam."));
    else if (estado == G_FALHOU || (fontesOk && !nFontes))
      snprintf(sub, sizeof sub, "%s",
               i18n("Nenhum canal: sem addon de canais e sem portal IPTV."));
    else
      snprintf(sub, sizeof sub, i18n("%d canais · %d categorias · segure %s para pular seção"),
               nCanais, nCats, "\xe2\x86\x91\xe2\x86\x93");
    { TxtLinha t = txt_linha_corta(TXT_CAPTION, sub, 160, 162, 170, 255,
                                   G_AREA_W);
      txt_desenhar_alpha(t, G_AREA_X, 116.0f, a); } }
  { time_t tt = time(NULL); struct tm lt; char hora[8];
    localtime_r(&tt, &lt); strftime(hora, sizeof hora, "%H:%M", &lt);
    TxtLinha t = txt_linha(TXT_PG_RELOGIO, hora, 255, 255, 255, 255);
    txt_desenhar_alpha(t, G_PAN_X + G_PAN_W - t.w, 48.0f, a); }

  desenharTopo(a);

  // Fileiras de canais. G_BAIXANDO com linhas publicadas e a RECARGA pedida
  // pelo painel de addons: a lista antiga fica na tela ate a nova chegar, em
  // vez de sumir por segundos (o subtitulo ja diz "Carregando canais…").
  if (modoLista && (estado == G_PRONTO || estado == G_BAIXANDO) && nLinhas() > 0) {
    time_t ini = janelaIni(agoraT);
    float y = G_L_TOPO - rolL;
    desenharRegua(a, ini);
    gfx_recorte(0.0f, G_L_TOPO - 8.0f, G_PAN_X - 24.0f, G_L_BASE - G_L_TOPO + 8.0f);
    for (l = 0; l < nLinhas(); l++) {
      int n = linhaN(l);
      float dim = modoCat && l != focoLin ? 0.35f : 1.0f;
      float bloco = G_L_HEAD + (float)n * G_L_ROW;
      if (y > G_L_BASE) break;
      if (y + bloco < G_L_TOPO - 8.0f) { y += bloco; continue; }
      { char cab[140];
        snprintf(cab, sizeof cab, "%s  \xc2\xb7  %d", linhaNome(l), n);
        TxtLinha t = txt_linha_corta(TXT_ROW_TITULO, cab,
                                   modoCat && l == focoLin ? 148 : 220,
                                   modoCat && l == focoLin ? 200 : 221,
                                   modoCat && l == focoLin ? 255 : 224, 255,
                                   G_AREA_W);
        txt_desenhar_alpha(t, G_AREA_X, y + 10.0f, a * dim); }
      y += G_L_HEAD;
      for (i = 0; i < n; i++, y += G_L_ROW) {
        if (y + G_L_ROW < G_L_TOPO - 8.0f || y > G_L_BASE) continue;
        desenharLinhaLista(linhaItem(l, i), y,
                           l == focoLin && i == focoCol ? 1.0f : 0.0f,
                           a * dim, agoraT, ini);
      }
    }
    gfx_sem_recorte();
    // A linha "agora", por cima de tudo, so quando agora esta na janela.
    if (agoraT >= ini && agoraT < ini + (time_t)G_L_JANELA_MIN * 60) {
      float x = G_L_FAIXA_X + (float)(agoraT - ini) / 60.0f
                * (G_L_FAIXA_W / (float)G_L_JANELA_MIN);
      GfxRect ln = { x - 1.0f, G_TOPO + 30.0f, 2.0f, G_L_BASE - G_TOPO - 30.0f };
      gfx_cor(ln, 0.0f, 0.58f, 0.78f, 1.0f, 0.75f * a);
      { TxtLinha t = txt_linha(TXT_MINI, i18n("AGORA"), 148, 200, 255, 255);
        txt_desenhar_alpha(t, x + 6.0f, G_TOPO - 14.0f, a); }
    }

  } else if ((estado == G_PRONTO || estado == G_BAIXANDO) && nLinhas() > 0) {
    gfx_recorte(0.0f, G_TOPO - 20.0f, G_PAN_X - 24.0f,
                NV_TELA_H - G_TOPO + 20.0f);
    for (l = 0; l < nLinhas(); l++) {
      float y = G_TOPO + (float)l * G_PASSO_Y - rolY;
      int n = linhaN(l);
      float dim = modoCat && l != focoLin ? 0.35f : 1.0f;
      if (y > NV_TELA_H || y + G_PASSO_Y < G_TOPO - 20.0f) continue;
      { char cab[140];
        snprintf(cab, sizeof cab, "%s  \xc2\xb7  %d", linhaNome(l), n);
        TxtLinha t = txt_linha_corta(TXT_ROW_TITULO, cab,
                                   modoCat && l == focoLin ? 148 : 220,
                                   modoCat && l == focoLin ? 200 : 221,
                                   modoCat && l == focoLin ? 255 : 224, 255,
                                   G_AREA_W);
        txt_desenhar_alpha(t, G_AREA_X, y, a * dim); }
      { float x = G_AREA_X - rolX[l];
        for (i = 0; i < n; i++, x += G_CARD_W + G_GAP_X) {
          if (x + G_CARD_W < 0.0f || x > G_PAN_X) continue;
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
    const char *msg = falhas
      ? i18n("Os addons de canais desta conta não responderam agora. O guia tenta de novo a cada 10 segundos enquanto esta tela estiver aberta.")
      : i18n("O guia se enche por dois caminhos: um addon de canais (como o FrostView TV) instalado na conta, ou um portal IPTV cadastrado em Ajustes › Conta.");
    float msgY = 300.0f;
    // O DESENHO SO NO CASO DE "FALTA FONTE", e nao no de "nao responderam".
    //
    // Ele explica de ONDE vem canal — resposta util para quem nao tem nenhuma
    // das duas portas, e resposta nenhuma para quem tem um addon que esta fora
    // do ar neste minuto. Ali a frase ja diz tudo, e um diagrama por cima dela
    // seria decoracao a atrapalhar a leitura.
    if (!falhas) {
      // ALINHADO A MARGEM, e nao centralizado: neste estado o painel da direita
      // NAO e desenhado, entao o "centro da area de conteudo" nao e o centro de
      // nada que a pessoa veja, e o desenho nascia deslocado em relacao ao
      // titulo e a frase. Na margem, a tela le como uma coluna.
      desenharDuasPortas(G_AREA_X, 196.0f, a);
      msgY = 560.0f;
    }
    TxtLinha t = txt_linha_corta(TXT_BODY, msg, 200, 202, 210, 255,
                                 NV_TELA_W - 2 * NV_MARGEM_X);
    txt_desenhar_alpha(t, G_AREA_X, msgY, a);
  }

  desenharPainel(a, agoraT);
  if (modoCat) desenharRailCategorias(a);

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
    TxtLinha t = txt_linha_corta(TXT_CAPTION, dica, 140, 142, 150, 255,
                                 G_AREA_W);
    txt_desenhar_alpha(t, G_AREA_X, NV_TELA_H - 54.0f, a); }

  if (painel) desenharPainelAddons(a);
}
