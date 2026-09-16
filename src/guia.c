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
// CONCORRENCIA: o fio escreve em s* (staging); quando termina sobe
// `pendPronto` e o fio de desenho copia para os vetores publicados. Leitores
// nunca tocam no staging — mesma disciplina do epg.c.
#include "guia.h"
#include "ajustes.h"   /* ajustes_acento: cor do anel de foco */
#include "epg.h"
#include "rede.h"
#include "addons.h"
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
static void sondaManifestos(void) {
  int a;
  for (a = 0; a < addons_n() && sNFontes < G_MAX_FONTE; a++) {
    const char *base;
    char url[700];
    char *corpo;
    const char *p, *fim;
    if (!addons_ativo(a)) continue;
    base = addons_base(a);
    if (!base || !base[0]) continue;
    snprintf(url, sizeof url, "%s/manifest.json", base);
    corpo = rede_baixar(url, 15);
    if (!corpo) continue;
    fim = corpo + strlen(corpo);
    p = js_array(corpo, fim, "catalogs");
    while (p && sNFontes < G_MAX_FONTE) {
      const char *f = js_fim(p);
      char tipo[16] = "", id[96] = "";
      js_texto(p, f, "type", tipo, sizeof tipo);
      js_texto(p, f, "id", id, sizeof id);
      if (ehCanal(tipo) && id[0] && !fonteJa(base, id)) {
        snprintf(sFontes[sNFontes].base, sizeof sFontes[sNFontes].base, "%s", base);
        snprintf(sFontes[sNFontes].tipo, sizeof sFontes[sNFontes].tipo, "%s", tipo);
        snprintf(sFontes[sNFontes].id,   sizeof sFontes[sNFontes].id,   "%s", id);
        sNFontes++;
      }
      p = js_prox(f);
    }
    free(corpo);
  }
}

static void *fioGuia(void *u) {
  int ok = 0;
  (void)u;
  sNCanais = 0; sNCats = 0;
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

// Quando foi a ultima tentativa de carga com o guia vazio. Ver guia_atualizar.
#define G_RETENTAR_MS 10000u
static Uint32 ultTentativa;

// Instantaneo da ABERTURA do overlay. O firmware repete o KEYDOWN da tecla
// segurada, e a tecla que ABRE (azul, ou `s` no Tizen) e a mesma que FECHA:
// sem este repouso, segurar o botao um pouco a mais abria e fechava o overlay
// em ~130 ms — o sintoma relatado de "o guia nao aparece quando esta tocando".
static Uint32 overlayDesde;
#define G_OVERLAY_REP_MS 400

static void focoValido(void) {
  int l = nLinhas();
  if (focoLin >= l) focoLin = l - 1;
  if (focoLin < 0) focoLin = 0;
  if (focoCol >= linhaN(focoLin)) focoCol = linhaN(focoLin) - 1;
  if (focoCol < 0) focoCol = 0;
}

void guia_abrir(void) {
  guia_carregar();
  aberta = 1; querSair = 0; entrada = 0.0f;
  focoValido();
}

void guia_overlay_abrir(void) {
  guia_carregar();
  overlay = 1; entrada = 0.0f;
  overlayDesde = SDL_GetTicks();
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

static void sair(void) {
  if (overlay) overlay = 0;
  else { aberta = 0; querSair = 1; }
  modoCat = 0; dirSeg = 0; okDesde = 0;
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

  // CH+/- do controle da LG (scancodes 480/481 do SDL_webOS.h): dentro do
  // guia eles pulam CATEGORIA, nao canal — e a mesma leitura do "segurar"
  // sem exigir o gesto. No Tizen o CH+ chega como "s" (tizen-shell.html), e o
  // CH- nao chega — a casca nao o registra.
  if (e->key.keysym.scancode == NV_SCANCODE_CH_UP ||
      e->key.keysym.scancode == NV_SCANCODE_CH_DOWN ||
      (!overlay && k == SDLK_s)) {
    saltarCat(e->key.keysym.scancode == NV_SCANCODE_CH_UP ? -1 : 1);
    return;
  }

  if (k == SDLK_UP || k == SDLK_DOWN) {
    int dir = (k == SDLK_DOWN) ? 1 : -1;
    if (modoCat) {
      saltarCat(dir);
      ultNavCat = agora; dirTick = agora;
      return;
    }
    // Ainda segurando a mesma direcao? O firmware repete o KEYDOWN a cada
    // ~130 ms; o relogio e quem distingue "toque" de "segurado".
    if (dirSeg == k && agora - dirTick < G_REP_MS) {
      if (agora - dirDesde >= G_HOLD_MS) {
        modoCat = 1;
        saltarCat(dir);
        ultNavCat = agora; dirTick = agora;
        return;
      }
    } else { dirSeg = k; dirDesde = agora; }
    dirTick = agora;
    moverVertical(dir);
    return;
  }
  // Qualquer outra tecla desarma os dois estados de direcao segurada.
  dirSeg = 0; modoCat = 0;

  if (k == SDLK_LEFT)  { if (focoCol > 0) focoCol--; return; }
  if (k == SDLK_RIGHT) { if (focoCol + 1 < linhaN(focoLin)) focoCol++; return; }
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

void guia_atualizar(float dt, Uint32 agora) {
  entrada = anim_mola(entrada, guia_visivel() ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  if (pendPronto) { publicar(); estado = G_PRONTO; pendPronto = 0; fioVivo = 0; focoValido(); }
  if (!guia_visivel()) return;

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
static void desenharCard(GCanal *c, float x, float y, float foco, float a,
                         time_t agoraT) {
  GfxRect r = { x, y, G_CARD_W, G_CARD_H };
  float lum = anim_mistura(0.075f, 0.16f, foco);
  EpgProg ag, px;
  int epg = epgDo(c);
  int temAgora = epg >= 0 && epg_agora(epg, agoraT, &ag);
  int temProx  = epg >= 0 && epg_proximo(epg, agoraT, 0, &px);

  // FOCO = O CARTAO PREENCHIDO NA COR DO CANAL, sem anel. Pedido do dono
  // (16/09), olhando a captura do guia: "quando ta selecionado ficar com a
  // cor do fundo da logo do canal". A cor sai dos pixels do logo
  // (tex_cor_marca: media pesada pelo croma — o azul do Disney+, o verde do
  // SBT); enquanto o logo nao carregou, a cor de realce, como todo botao do
  // app desde a mesma decisao (ver NV_COR_FOCO em layout.h).
  //
  // O texto acompanha: escuro sobre marca clara, claro sobre marca escura,
  // decidido pela luminancia da cor e trocado no meio da mola (texto ja
  // rasterizado nao muda de cor).
  float cr, cg, cb;
  int escuro = 0;
  if (!tex_cor_marca(c->logo, &cr, &cg, &cb)) ajustes_acento(&cr, &cg, &cb);
  if (foco > 0.5f) escuro = (cr * 0.299f + cg * 0.587f + cb * 0.114f) > 0.55f;
  gfx_cor(r, 0.08f, lum, lum, lum + 0.01f, a);
  if (foco > 0.01f) gfx_cor(r, 0.08f, cr, cg, cb, foco * a);

  // Logo sobre um azulejo claro: os logos do FrostView sao PNG escuros/coloridos
  // pensados para fundo branco — num fundo escuro varios somem.
  { GfxRect az = { x + 14.0f, y + 14.0f, 92.0f, 92.0f };
    gfx_cor(az, 0.14f, 0.93f, 0.94f, 0.95f, a);
    if (c->logo[0]) {
      GLuint t = tex_obter_larg(c->logo, 92.0f);
      float ap = tex_aspecto(c->logo);
      if (t && ap > 0.0f) {
        float w = 80.0f, h = w / ap;
        if (h > 80.0f) { h = 80.0f; w = h * ap; }
        GfxRect lr = { az.x + (az.w - w) * 0.5f, az.y + (az.h - h) * 0.5f, w, h };
        gfx_tex_aspect_atual = 0.0f;
        gfx_rect(lr, t, GFX_TEXTO, 0, 0, 0, 0.0f, 1, 1, 1, a);
      }
    } }

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

  // Logo grande, na mesma regra do card (fundo claro).
  { GfxRect az = { G_PAN_X + (G_PAN_W - 168.0f) * 0.5f, y, 168.0f, 168.0f };
    gfx_cor(az, 0.10f, 0.93f, 0.94f, 0.95f, a);
    if (c->logo[0]) {
      GLuint t = tex_obter_larg(c->logo, 168.0f);
      float ap = tex_aspecto(c->logo);
      if (t && ap > 0.0f) {
        float w = 148.0f, h = w / ap;
        if (h > 148.0f) { h = 148.0f; w = h * ap; }
        GfxRect lr = { az.x + (az.w - w) * 0.5f, az.y + (az.h - h) * 0.5f, w, h };
        gfx_tex_aspect_atual = 0.0f;
        gfx_rect(lr, t, GFX_TEXTO, 0, 0, 0, 0.0f, 1, 1, 1, a);
      } }
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
    else if (estado == G_FALHOU || (fontesOk && !nFontes))
      snprintf(sub, sizeof sub, "%s",
               i18n("Nenhum catálogo de canais nos addons instalados."));
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

  // Fileiras de canais.
  if (estado == G_PRONTO && nLinhas() > 0) {
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
    TxtLinha t = txt_linha_corta(TXT_BODY,
        i18n("O guia precisa de um addon de canais (como o FrostView TV) instalado na conta."),
        200, 202, 210, 255, NV_TELA_W - 2 * NV_MARGEM_X);
    txt_desenhar_alpha(t, G_AREA_X, 300.0f, a);
  }

  desenharPainel(a, agoraT);
  if (modoCat) desenharRailCategorias(a);

  // Barra de ajuda — a resposta ao "explicar no componente como abrir o
  // overlay": as teclas que o dono precisa lembrar ficam escritas na tela.
  { const char *dica = overlay
      ? i18n("OK troca de canal  ·  segure OK = favorito  ·  segure \xe2\x86\x91\xe2\x86\x93 pula seção  ·  Voltar ou Azul fecha")
      : i18n("OK assiste  ·  segure OK = favorito  ·  segure \xe2\x86\x91\xe2\x86\x93 pula seção  ·  Voltar sai");
    TxtLinha t = txt_linha_corta(TXT_CAPTION, dica, 140, 142, 150, 255,
                                 G_AREA_W);
    txt_desenhar_alpha(t, G_AREA_X, NV_TELA_H - 54.0f, a); }
}
