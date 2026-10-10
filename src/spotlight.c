// Spotlight: a caixa de busca por cima da tela. O porque e quem abre estao em
// spotlight.h; aqui so as decisoes de forma e de custo.
//
// FORMA (pedido do dono, 01/10/2026: "deixar a barra do spotlight so a barra e
// so crescer com os resultados aparecendo"). Como o Spotlight do macOS: abre SO
// a barra — vidro, centrada no alto, lupa, campo e microfone a direita. Quando
// o texto chega, a mesma superficie CRESCE para baixo com a mola lenta da ilha
// (ilha.c, ILHA_MOLA_*) e mostra a lista; sem resultado, encolhe de volta. A
// lista e VERTICAL e agrupada (o Spotlight do Mac, o Google TV): o melhor
// resultado no alto fica a UM toque (baixo + OK).
//
// QUEM DIGITA. No Android, o teclado do SISTEMA (sistexto.h): abrir ja foca o
// campo e o chama; a tecla de voz ja comeca o ditado. Onde nao ha IME
// confiavel (LG, Samsung) o teclado do app — o mesmo alfabeto de
// teclado_alfabeto() — so aparece com OK no campo, a esquerda dentro do corpo,
// e some quando o foco desce para os resultados. Voltar desfaz na ordem
// inversa: teclado/lista -> campo; campo com texto -> limpa (a barra encolhe);
// campo vazio -> fecha.
//
// AS FONTES SAO AS QUE O APP JA TEM, nenhuma rede nova:
//   - titulos: o catalogo em memoria (as fileiras da home) + desc_buscar, a
//     mesma busca nos addons da tela de Busca;
//   - pessoas: o ELENCO dos titulos ja carregados (CatItem.elenco), com o id do
//     TMDB que abre a filmografia, e depois o /search/person do TMDB
//     (spotpessoa.h) — com debounce, um pedido quando o texto para, nunca um
//     por letra. A do elenco vem antes: e da biblioteca do dono;
//   - colecoes (pastas), canais da Live TV (lista publicada do guia),
//     catalogos (titulos das fileiras) e addons instalados.
//   Fora isso so a busca nos addons (desc_buscar) e a de pessoas vao a rede.
//
// CUSTO (60 fps na C9): remontar so roda quando o texto muda ou a resposta da
// rede chega, nunca por quadro. O desenho e um veu, um painel e no maximo
// ~12 linhas visiveis; com a entrada assentada app.c congela o fundo numa
// copia (como o painel de Salvos), e o quadro passa a ser copia + painel.
#include "spotlight.h"
#include "buscasrec.h"
#include "buscanorm.h"
#include "catalogo.h"
#include "descoberta.h"
#include "colecoes.h"
#include "guia.h"
#include "spotpessoa.h"
#include "addons.h"
#include "teclado.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "anim.h"
#include "layout.h"
#include "ajustes.h"
#include "ajustes_ux.h"
#include "idioma.h"
#include "posterprov.h"
#include "rede.h"
#include "ponteiro.h"
#include "sistexto.h"
#include "celbotao.h"
#define NV_ESCALA_TELA   // o arquivo inteiro mede pela tela virtual (escala.h)
#include "escala.h"
// ESCALA COM PISO (dono, 03/10): o Spotlight inteiro nasce a 130% e acompanha o
// Tamanho da interface acima disso. A tela virtual do arquivo e a do fator
// efetivo, e e so o desenho publico que liga a escala (ESCALA_MIN_INI/FIM).
#define SP_ESCALA_MIN 1.3f
#undef NV_TELA_W
#undef NV_TELA_H
#define NV_TELA_W (1920.0f / escala_min(SP_ESCALA_MIN))
#define NV_TELA_H (1080.0f / escala_min(SP_ESCALA_MIN))
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

// --- Geometria (1920x1080) ----------------------------------------------------
// AS MEDIDAS SAO AS DO MOCKUP "ilha" tela 6 (dono, 02/10: "o mockup ta bem
// mais polido que a build, nao podemos errar"): duas ilhas de 980 px
// centradas, a barra com 76 px de altura a 120 do topo e a dos resultados 20
// abaixo dela, raio 36 e 22 px de ar por dentro. Com o teclado do app aberto
// as duas alargam para 1240 (a mola do teclado): o teclado ocupa 500 px a
// esquerda e em 980 a lista ficaria com menos de 460.
#define SP_BW_BASE   980.0f
#define SP_BW_KB     1240.0f
static float spBW = SP_BW_BASE;
#define SP_BW        spBW
#define SP_BX        ((NV_TELA_W - SP_BW) * 0.5f)
#define SP_BY        120.0f
#define SP_BH        76.0f
#define SP_RAIO      36.0f          // px do corpo aberto; a barra sozinha e pilula
#define SP_CORPO_Y   (SP_BY + SP_BH)
// A ilha dos resultados desce ate perto do fim da tela (dono, 03/10: "a lista
// debaixo descer mais ate o final, que ta muito curta"): 40 de margem embaixo,
// como as outras ilhas, e nao os 56 de antes. Tela virtual (escala.h).
#define SP_MARGEM_B  40.0f
#define SP_CORPO_MAX (NV_TELA_H - SP_MARGEM_B - SP_CORPO_Y)
// DUAS ILHAS (Glass UI, mockup "ilha" tela 6): o campo e uma pilula sozinha e
// os resultados moram numa segunda ilha SP_ILHA_VAO abaixo dela. O recuo de
// cima do corpo conta o vao e o ar de dentro da ilha de baixo: 22 de padding
// menos os 15 que o cabecalho de grupo (50 px, texto colado embaixo) ja traz,
// para o "MELHOR RESULTADO" cair onde o mockup o poe.
#define SP_ILHA_VAO  20.0f
#define SP_ILHA_PAD  22.0f
#define SP_CPAD_T    (SP_ILHA_VAO + 7.0f)
#define SP_CPAD_B    0.0f
#define SP_RODAPE_H  58.0f          // 14 + a dica + 4, e os 22 de ar da ilha
#define SP_MIC_D     52.0f
#define SP_TECLA     62.0f
#define SP_TECLA_GAP 10.0f
#define SP_KB_COLS   6
#define SP_KB_PASSO  (SP_TECLA + SP_TECLA_GAP)
#define SP_KB_X      (SP_BX + 36.0f)
#define SP_KB_Y      (SP_CORPO_Y + SP_ILHA_VAO + SP_ILHA_PAD + 8.0f)
#define SP_KB_W      (SP_KB_COLS * SP_TECLA + (SP_KB_COLS - 1) * SP_TECLA_GAP)
#define SP_LISTA_X0  (SP_BX + SP_ILHA_PAD)            // sem o teclado do app
#define SP_LISTA_X1  (SP_KB_X + SP_KB_W + 44.0f)     // com ele
#define SP_LISTA_XF  (SP_BX + SP_BW - SP_ILHA_PAD)
#define SP_MAX_TXT   48             // = BUSCASREC_TERMO
#define SP_MAX_LIN   48
#define SP_KB_MAX_FIL 8
#define SP_MAX_RECENTES 4
// A mola da ilha (ilha.c): subamortecida, o corpo passa um pouco e volta.
#define SP_MOLA_W    10.0f
#define SP_MOLA_Z    0.72f

// --- Linhas da lista -----------------------------------------------------------
enum {
  L_CAB = 0,      // cabecalho de grupo (nao focavel)
  L_AVISO,        // "Buscando...", "Nada encontrado" (nao focavel)
  L_TOPO,         // melhor resultado
  L_TITULO,
  L_PESSOA,
  L_COLECAO,
  L_CANAL,
  L_CATALOGO,
  L_ADDON,
  L_RECENTE,
  L_LIMPAR,
  L_AJUSTE,
  L_GUIA,         // recurso do Guia de uso (modo guia)
};
// Alturas SEM vao entre as linhas, como as .row do mockup: o melhor resultado
// e 140 de arte + 16 em cima e embaixo, a linha de titulo 62 de cartaz + 12.
static const float ALTURA[] = { 50, 64, 172, 86, 86, 86, 86, 72, 72, 64, 64, 80, 76 };

typedef struct {
  int  tipo;
  int  ref;          // indice de catalogo / pasta / canal / fileira / addon / termo
  int  ref2;         // pessoa: indice do titulo de onde ela veio
  long tmdb;
  long tituloTmdb;   // pessoa do TMDB: o titulo pelo qual a filmografia abre
  char tituloTipo[8];
  char t1[160];
  char t2[200];
  char arte[1024];
  char genero[160];
  int paisagem;
  char icone[32];
  char id[80];
  char base[600];
  char chave[96];    // identidade, para foco e entrada sobreviverem a remontagem
  float y, h;
} Linha;

static Linha lin[SP_MAX_LIN];
static int   nLin;
static float animLin[SP_MAX_LIN], entraLin[SP_MAX_LIN];

// --- Estado ----------------------------------------------------------------------
static int   aberto;
static int   modoAjustes;
// MODO GUIA (spot_abrir_guia): o "Buscar no guia" do Guia de uso. E um modo
// Ajustes (so local, sem historico) com os recursos do guia na frente.
static int   modoGuia;
static char  consultaRealce[SP_MAX_TXT * 2];   // o termo realcado no modo guia
static int   retornoAjustesValido;
static char  retornoAjustesConsulta[SP_MAX_TXT];
static char  retornoAjustesChave[96];
static float entrada;              // 0..1 (mola)
// Onde esta o foco. O teclado do app so existe com kbAberto.
enum { P_CAMPO = 0, P_MIC, P_TECLADO, P_LISTA, P_CEL };
static int   painel;
static int   kbAberto;
static float kbAnim;               // 0..1, o teclado do app entrando
static float corpoH, corpoV;       // altura do corpo (mola da ilha) e velocidade
static float animMic;
static int   focoL = -1;           // linha focada (indice em lin)
static int   kbF, kbC;             // tecla focada
static char  consulta[SP_MAX_TXT];
static int   nConsulta;
static char  montada[SP_MAX_TXT];  // consulta da ultima remontagem
static int   ultimoRemoto = -1, ultimoBuscando = -1, ultimaGeracao = -1;
static unsigned ultimaGerPessoa;
static unsigned ultimaRevCatalogo;
static float scrollY, scrollAlvo, velY;
static float animTecla[SP_KB_MAX_FIL + 1][SP_KB_COLS];
static float animCampo;
static SpotPedido pedido;
static int   temPedido;
static int   okPress, okLongo;
static Uint32 okDesde;
static float nivelVoz;             // nivel do som suavizado (st_nivel)

// Soma do que cada alvo de busca ja devolveu para o termo corrente: a resposta
// de um addon lento chega depois da tecla e tem de aparecer sozinha.
static int remotoTotal(void) {
  int a, n = 0, nA = desc_busca_n_alvos();
  for (a = 0; a < nA; a++) n += desc_busca_alvo_n(a, consulta);
  return n + desc_busca_n(consulta) * 1000;
}

// Teclado: 36 caracteres em 6 fileiras e a fileira de baixo de comandos.
static char  kbTeclas[64][5];
static int   kbN, kbFil;
enum { K_ESPACO, K_APAGAR, K_LIMPAR, K_FALAR, K_TECLADO };
static int   kbCmd[6], kbNCmd;

static int ditadoDisponivel(void) { return st_voz_disponivel(); }
// DIGITAR PELO CELULAR (celbotao.h): o disco no fim da barra, depois do Falar.
static int celDisponivel(void) { return celb_disponivel(); }
static void spotAbrirBase(int voz, int tecladoAuto);
static int primeiraFocavel(void);
// Largura que os botoes da direita (Falar, Celular) tiram da barra.
// Largura dos botoes da ponta direita da barra (microfone, celular), com o
// vao de 10 entre eles e os 12 da borda; 0 sem nenhum.
static float botoesW(void) {
  int n = ditadoDisponivel() + celDisponivel();
  return n ? n * SP_MIC_D + (n - 1) * 10.0f + 12.0f : 0.0f;
}
static int imeDisponivel(void) { return st_ime_disponivel(); }
static int ouvindo(void) {
  return st_dono() == ST_SPOT && (st_estado() == ST_OUVINDO || st_estado() == ST_PERMISSAO ||
                                  st_estado() == ST_VOZ_SISTEMA);
}
static int digitandoSis(void) { return st_dono() == ST_SPOT && st_estado() == ST_DIGITANDO; }

static void kbMontar(void) {
  const unsigned char *p = (const unsigned char *)teclado_alfabeto();
  kbN = 0;
  while (*p && kbN < 48) {
    int len = *p < 0x80 ? 1 : (*p >= 0xF0 ? 4 : (*p >= 0xE0 ? 3 : 2)), i;
    for (i = 0; i < len; i++) kbTeclas[kbN][i] = (char)p[i];
    kbTeclas[kbN][len] = 0;
    kbN++; p += len;
  }
  kbFil = (kbN + SP_KB_COLS - 1) / SP_KB_COLS;
  kbNCmd = 0;
  kbCmd[kbNCmd++] = K_ESPACO;
  kbCmd[kbNCmd++] = K_APAGAR;
  kbCmd[kbNCmd++] = K_LIMPAR;
  if (ditadoDisponivel()) kbCmd[kbNCmd++] = K_FALAR;
  if (imeDisponivel()) kbCmd[kbNCmd++] = K_TECLADO;
}

static int kbColunas(int f) {
  if (f < kbFil) {
    int resto = kbN - f * SP_KB_COLS;
    return resto > SP_KB_COLS ? SP_KB_COLS : resto;
  }
  return kbNCmd;
}

static GfxRect teclaRect(int f, int c) {
  GfxRect r;
  r.y = SP_KB_Y + f * SP_KB_PASSO;
  r.h = SP_TECLA;
  if (f < kbFil) { r.x = SP_KB_X + c * SP_KB_PASSO; r.w = SP_TECLA; }
  else {
    r.w = (SP_KB_W - (kbNCmd - 1) * SP_TECLA_GAP) / (float)kbNCmd;
    r.x = SP_KB_X + c * (r.w + SP_TECLA_GAP);
  }
  return r;
}

// --- Montagem da lista --------------------------------------------------------------
static int focavel(int tipo) { return tipo != L_CAB && tipo != L_AVISO; }

static Linha *nova(int tipo) {
  Linha *l;
  if (nLin >= SP_MAX_LIN) return NULL;
  l = &lin[nLin++];
  memset(l, 0, sizeof *l);
  l->tipo = tipo;
  l->ref = l->ref2 = -1;
  return l;
}

static void cabecalho(const char *t) {
  Linha *l = nova(L_CAB);
  if (!l) return;
  snprintf(l->t1, sizeof l->t1, "%s", t);
  snprintf(l->chave, sizeof l->chave, "cab|%s", t);
}

// Quanto o titulo `t` (normalizado) casa com o alvo: igual > comeca com >
// uma palavra comeca com > contem. 0 = nao casa.
static int pontuar(const char *t, const char *alvo) {
  size_t n = strlen(alvo);
  const char *p;
  if (!strcmp(t, alvo)) return 100;
  if (!strncmp(t, alvo, n)) return 80;
  for (p = t; (p = strstr(p, alvo)) != NULL; p++)
    if (p > t && (p[-1] == ' ' || p[-1] == ':' || p[-1] == '-')) return 60;
  return strstr(t, alvo) ? 40 : 0;
}

static const char *rotuloTipo(const char *tipo) {
  if (!strcmp(tipo, "movie")) return i18n("Filme");
  if (!strcmp(tipo, "series")) return i18n("Série");
  if (!strcmp(tipo, "channel") || !strcmp(tipo, "tv")) return i18n("Canal");
  return "";
}

// "2022 · 3 temporadas · Série · ★ 8.1": o que a lista mostra sob o nome.
static void metaTitulo(const CatItem *ci, char *dst, size_t n) {
  const char *tp = rotuloTipo(ci->tipo);
  char nota[24] = "";
  // A nota com a virgula do idioma ("8,5"), como o resto do app; o ponto so
  // em ingles (ajustes.h).
  if (ci->nota > 0) snprintf(nota, sizeof nota, "\xe2\x98\x85 %d%c%d", ci->nota / 10,
                             ajustes_idioma_ingles() ? '.' : ',', ci->nota % 10);
  snprintf(dst, n, "%s%s%s%s%s", ci->meta,
           ci->meta[0] && tp[0] ? " \xc2\xb7 " : "", tp,
           (ci->meta[0] || tp[0]) && nota[0] ? " \xc2\xb7 " : "", nota);
}

static const char *arteDe(const CatItem *ci, int paisagem) {
  const char *a;
  if (paisagem && ci->backdrop[0]) return ci->backdrop;
  // A busca preserva a arte do resultado; provedor por ID so sem nenhuma.
  if (ci->poster[0]) return ci->poster;
  if (ci->backdrop[0]) return ci->backdrop;
  a = posterprov_card_addon(ci->origem, ci->imdb, ci->tmdb, ci->tipo, "");
  return a && a[0] ? a : "";
}

static void linhaTitulo(int tipo, int idx) {
  const CatItem *ci = cat_item(idx);
  Linha *l;
  if (!ci || !(l = nova(tipo))) return;
  l->ref = idx;
  snprintf(l->t1, sizeof l->t1, "%s", ci->titulo);
  metaTitulo(ci, l->t2, sizeof l->t2);
  snprintf(l->arte, sizeof l->arte, "%s", arteDe(ci, tipo == L_TOPO));
  snprintf(l->id, sizeof l->id, "%s", ci->imdb);
  snprintf(l->genero, sizeof l->genero, "%s", ci->genero);
  l->paisagem = ci->backdrop[0] && strcmp(ci->backdrop, ci->poster) &&
                !strcmp(l->arte, ci->backdrop);
  // Uma linha por montagem, nao por quadro. URLs redigidas, inclusive addon.
  { char origem[160], poster[256], fundo[256], escolhida[256];
    printf("[spotlight-arte] id=%s tipo=%s meta=%.96s linha=%s fonte=%s paisagem=%d origem=%s poster=%s background=%s escolhida=%s\n",
           ci->imdb, ci->tipo, ci->meta, tipo == L_TOPO ? "topo" : "titulo",
           !l->arte[0] ? "nenhuma" : !strcmp(l->arte, ci->poster) ? "poster" :
           !strcmp(l->arte, ci->backdrop) ? "background" : "provedor-id", l->paisagem,
           rede_url_publica(ci->origem, origem, sizeof origem),
           rede_url_log(ci->poster, poster, sizeof poster),
           rede_url_log(ci->backdrop, fundo, sizeof fundo),
           rede_url_log(l->arte, escolhida, sizeof escolhida));
  }
  snprintf(l->chave, sizeof l->chave, "t|%s|%s", ci->imdb, ci->titulo);
}

typedef struct { int idx, pont, ordem; } Cand;
static int candCmp(const void *a, const void *b) {
  const Cand *x = a, *y = b;
  if (x->pont != y->pont) return y->pont - x->pont;
  return x->ordem - y->ordem;
}

#define SP_MAX_TIT 9
static int tituloDoUsuario(const CatItem *ci) {
  return ci->naLista || ci->naColecao || ci->progresso > 0 || ci->retomadoMs > 0;
}
static void montarTitulos(const char *alvo) {
  Cand c[64];
  int nc = 0, r, i, ordem = 0;
  char nome[320];
  // REDE primeiro na ORDEM (o addon ja ranqueou), local depois; o placar
  // decide o resto. Remoto que nao contem o texto (o addon casou por outro
  // campo) ainda entra, abaixo de qualquer casamento de nome.
  { int a, nAlvos = desc_busca_n_alvos();
    for (a = 0; a < nAlvos && nc < 40; a++) {
      int nRem = desc_busca_alvo_n(a, consulta);
      CatItem novos[8];
      int posNovo[8], idxNovos[8], nNovos = 0;
      for (i = 0; i < nRem && i < 8 && nc < 40; i++) {
        CatItem it;
        int idx, k, dup = 0;
        // Sem ID, cada montagem reinseriria o resultado e mudaria a revisao.
        if (!desc_busca_alvo_item(a, i, &it) || !it.imdb[0]) continue;
        idx = cat_indice_por_imdb(it.imdb);
        if (idx >= 0) for (k = 0; k < nc; k++) if (c[k].idx == idx) { dup = 1; break; }
        if (dup) continue;
        busca_normalizar(it.titulo, nome, sizeof nome);
        c[nc].pont = pontuar(nome, alvo);
        if (c[nc].pont < 20) c[nc].pont = 20;
        c[nc].pont += 6 - (a < 6 ? a : 6);   // primeiro addon desempata
        c[nc].ordem = ordem++;
        if (idx >= 0) c[nc++].idx = idx;
        else if (nNovos < 8) { novos[nNovos] = it; posNovo[nNovos++] = nc; c[nc++].idx = -1; }
      }
      // Os que nao estao no catalogo entram numa troca de bloco so (o porque
      // esta em refiltrar, busca.c: cat_acrescentar por item copia tudo).
      if (nNovos > 0) {
        int entraram = cat_acrescentar_lote(novos, nNovos, idxNovos);
        for (i = 0; i < nNovos; i++) c[posNovo[i]].idx = i < entraram ? idxNovos[i] : -1;
      }
    } }
  // Salvos podem estar fora das fileiras. O bonus pessoal (15) vence a
  // ordem dos addons (0..6), mas nao a classe de casamento do nome (20).
  // Nao usamos nota como popularidade: CatItem nao tem esse dado.
  for (r = -1; r < cat_n_fileiras(); r++) {
    const CatFileira *cf = r >= 0 ? cat_fileira(r) : NULL;
    int n = cf ? cf->n : cat_n();
    if (r >= 0 && !cf) break;
    if (cf && desc_busca_base_oculta(cf->base)) continue;
    for (i = 0; i < n; i++) {
      int idx = cf ? cf->ini + i : i;
      const CatItem *ci = cat_item(idx);
      int p, k, bonus, dup = 0;
      if (!ci || !strcmp(ci->tipo, "channel")) continue;
      if (!cf && !tituloDoUsuario(ci)) continue;
      bonus = tituloDoUsuario(ci) || (cf && !strcmp(cf->chave, "continue_watching")) ? 15 : 5;
      busca_normalizar(ci->titulo, nome, sizeof nome);
      if (!(p = pontuar(nome, alvo))) continue;
      for (k = 0; k < nc; k++) {
        const CatItem *o = c[k].idx >= 0 ? cat_item(c[k].idx) : NULL;
        if (c[k].idx == idx || (o && ci->imdb[0] && !strcmp(o->imdb, ci->imdb))) {
          if (p + bonus > c[k].pont) c[k].pont = p + bonus;
          dup = 1; break;
        }
      }
      if (dup) continue;
      // O teto limita memoria, nao a busca: um exato tardio substitui parcial.
      k = nc;
      if (nc == 64) {
        k = 0;
        for (int j = 1; j < nc; j++) if (candCmp(&c[j], &c[k]) > 0) k = j;
        if (p + bonus <= c[k].pont) continue;
      } else nc++;
      c[k].idx = idx; c[k].pont = p + bonus; c[k].ordem = ordem++;
    }
  }
  // tira os -1 (catalogo no teto) antes de ordenar
  { int w = 0; for (i = 0; i < nc; i++) if (c[i].idx >= 0) c[w++] = c[i]; nc = w; }
  qsort(c, (size_t)nc, sizeof *c, candCmp);
  // O mesmo titulo vindo de dois addons sem imdb conhecido so ganha o indice
  // no lote: fica o de placar maior (o primeiro depois da ordenacao).
  { int w = 0, k;
    for (i = 0; i < nc; i++) {
      for (k = 0; k < w; k++) if (c[k].idx == c[i].idx) break;
      if (k == w) c[w++] = c[i];
    }
    nc = w; }
  // O MESMO TITULO COM DOIS IDS: um addon de busca devolve "tvdb:383203" (ou
  // "tmdb:...") e o catalogo do Nuvio devolve "tt10986410" (Ted Lasso, TCL
  // 05/10/2026). O de id de fora so tem POSTER: o melhor resultado saia em
  // retrato, a pagina abria com o poster esticado e o elenco sem foto. Fica o
  // do IMDb, com o placar maior dos dois. Mesmo nome, mesmo tipo e mesmo ano
  // (ou ano desconhecido de um lado) — remake com o mesmo nome tem outro ano.
  { int w = 0, k;
    unsigned char sai[64] = { 0 };
    for (i = 0; i < nc; i++) {
      const CatItem *a = cat_item(c[i].idx);
      int junta = -1;
      if (a && a->imdb[0] && strncmp(a->imdb, "tt", 2) && strncmp(a->imdb, "kitsu:", 6)) {
        char na[160], nb[160];
        busca_normalizar(a->titulo, na, sizeof na);
        for (k = 0; k < nc && junta < 0; k++) {
          const CatItem *b = k == i ? NULL : cat_item(c[k].idx);
          if (!b || strncmp(b->imdb, "tt", 2) || strcmp(a->tipo, b->tipo)) continue;
          busca_normalizar(b->titulo, nb, sizeof nb);
          if (strcmp(na, nb)) continue;
          if (isdigit((unsigned char)a->meta[0]) && isdigit((unsigned char)b->meta[0]) &&
              strncmp(a->meta, b->meta, 4)) continue;
          junta = k;
        }
      }
      if (junta >= 0) { sai[i] = 1; if (c[i].pont > c[junta].pont) c[junta].pont = c[i].pont; }
    }
    for (i = 0; i < nc; i++) if (!sai[i]) c[w++] = c[i];
    nc = w;
    qsort(c, (size_t)nc, sizeof *c, candCmp); }
  if (nc > 0) {
    cabecalho(i18n("Melhor resultado"));
    linhaTitulo(L_TOPO, c[0].idx);
  }
  if (nc > 1) {
    cabecalho(i18n("Títulos"));
    for (i = 1; i < nc && i < SP_MAX_TIT; i++) linhaTitulo(L_TITULO, c[i].idx);
  }
}

static void montarPessoas(const char *alvo) {
  long vistos[SPP_MAX + 4];
  int nv = 0, i, j, n = cat_n(), cab = 0;
  char nome[160];
  for (i = 0; i < n && nv < 4; i++) {
    const CatItem *ci = cat_item(i);
    if (!ci || ci->nElenco <= 0) continue;
    for (j = 0; j < ci->nElenco && nv < 4; j++) {
      int k, dup = 0;
      Linha *l;
      if (ci->elenco[j].tmdb <= 0 || !ci->elenco[j].nome[0]) continue;
      for (k = 0; k < nv; k++) if (vistos[k] == ci->elenco[j].tmdb) { dup = 1; break; }
      if (dup) continue;
      busca_normalizar(ci->elenco[j].nome, nome, sizeof nome);
      if (pontuar(nome, alvo) < 60) continue;   // pessoa: so inicio de nome/sobrenome
      if (!cab) { cabecalho(i18n("Pessoas")); cab = 1; }
      if (!(l = nova(L_PESSOA))) return;
      vistos[nv++] = ci->elenco[j].tmdb;
      l->ref2 = i;
      l->tmdb = ci->elenco[j].tmdb;
      snprintf(l->t1, sizeof l->t1, "%s", ci->elenco[j].nome);
      snprintf(l->t2, sizeof l->t2, i18n("Em %s"), ci->titulo);
      snprintf(l->arte, sizeof l->arte, "%s", ci->elenco[j].foto);
      snprintf(l->chave, sizeof l->chave, "p|%ld", l->tmdb);
    }
  }
  // O TMDB completa ate SPP_MAX, na ordem de popularidade dele. Sem resposta
  // ainda (debounce, rede) a lista fica com as do elenco e remonta quando a
  // resposta chega (spot_atualizar).
  { int nt = spotpessoa_n(consulta);
    for (i = 0; i < nt && nv < SPP_MAX; i++) {
      SpotPessoa sp;
      int k, dup = 0;
      Linha *l;
      if (!spotpessoa_item(consulta, i, &sp)) break;
      for (k = 0; k < nv; k++) if (vistos[k] == sp.tmdb) { dup = 1; break; }
      if (dup) continue;
      if (!cab) { cabecalho(i18n("Pessoas")); cab = 1; }
      if (!(l = nova(L_PESSOA))) return;
      vistos[nv++] = sp.tmdb;
      l->tmdb = sp.tmdb;
      l->tituloTmdb = sp.tituloTmdb;
      snprintf(l->tituloTipo, sizeof l->tituloTipo, "%s", sp.tituloTipo);
      snprintf(l->t1, sizeof l->t1, "%s", sp.nome);
      if (sp.conhecido[0]) snprintf(l->t2, sizeof l->t2, i18n("Conhecido por  %s"), sp.conhecido);
      snprintf(l->arte, sizeof l->arte, "%s", sp.foto);
      snprintf(l->chave, sizeof l->chave, "p|%ld", l->tmdb);
    } }
}

static void montarColecoes(const char *alvo) {
  int i, n = col_n(), achou = 0;
  char t[300];
  for (i = 0; i < n && achou < 3; i++) {
    const ColFolder *f = col_folder(i);
    Linha *l;
    if (!f || !f->title[0]) continue;
    busca_normalizar(f->title, t, sizeof t);
    if (!pontuar(t, alvo)) {
      busca_normalizar(f->group, t, sizeof t);
      if (!f->group[0] || pontuar(t, alvo) < 60) continue;
    }
    if (!achou) cabecalho(i18n("Coleções"));
    if (!(l = nova(L_COLECAO))) return;
    achou++;
    l->ref = i;
    snprintf(l->t1, sizeof l->t1, "%s", f->title);
    snprintf(l->t2, sizeof l->t2, "%s%s%s", i18n("Coleção"), f->group[0] ? "  \xc2\xb7  " : "", f->group);
    snprintf(l->arte, sizeof l->arte, "%s", col_capa(f) ? col_capa(f) : "");
    snprintf(l->chave, sizeof l->chave, "c|%s", f->id);
  }
}

static void montarCanais(const char *alvo) {
  int idx[4], n = guia_buscar_canais(alvo, idx, 4), i;
  if (n > 0) cabecalho(i18n("Canais ao vivo"));
  for (i = 0; i < n; i++) {
    const char *id, *nome, *logo, *cat, *base;
    Linha *l;
    if (!guia_canal_campos(idx[i], &id, &nome, &logo, &cat, &base) || !(l = nova(L_CANAL))) continue;
    l->ref = idx[i];
    snprintf(l->id, sizeof l->id, "%s", id);
    snprintf(l->t1, sizeof l->t1, "%s", nome);
    // A categoria passa por i18n como no guia (linhaNome): as secoes de
    // categoriaPorNome sao chaves da tabela; genero do addon volta como veio.
    snprintf(l->t2, sizeof l->t2, "%s%s%s", i18n("Canal"), cat[0] ? "  \xc2\xb7  " : "",
             cat[0] ? i18n(cat) : "");
    snprintf(l->arte, sizeof l->arte, "%s", logo);
    snprintf(l->base, sizeof l->base, "%s", base);
    snprintf(l->chave, sizeof l->chave, "k|%.90s", id);
  }
}

static void montarCatalogos(const char *alvo) {
  int r, achou = 0, i;
  char t[300];
  for (r = 0; r < cat_n_fileiras() && achou < 3; r++) {
    const CatFileira *cf = cat_fileira(r);
    Linha *l;
    if (!cf || !cf->base[0] || !cf->catId[0]) continue;
    busca_normalizar(cf->titulo, t, sizeof t);
    if (pontuar(t, alvo) < 60) continue;
    if (!achou) cabecalho(i18n("Catálogos"));
    if (!(l = nova(L_CATALOGO))) return;
    achou++;
    l->ref = r;
    snprintf(l->t1, sizeof l->t1, "%s", cf->titulo);
    snprintf(l->t2, sizeof l->t2, "%s  \xc2\xb7  %d %s", i18n("Catálogo"), cf->n,
             i18n(cf->n == 1 ? "título" : "títulos"));
    snprintf(l->icone, sizeof l->icone, "aj_rows-3");
    snprintf(l->chave, sizeof l->chave, "f|%.90s", cf->chave);
  }
  for (i = 0, achou = 0; i < addons_n() && achou < 2; i++) {
    const char *nm = addons_nome(i);
    Linha *l;
    if (!nm || !nm[0]) continue;
    busca_normalizar(nm, t, sizeof t);
    if (pontuar(t, alvo) < 60) continue;
    if (!achou) cabecalho(i18n("Addons"));
    if (!(l = nova(L_ADDON))) return;
    achou++;
    l->ref = i;
    snprintf(l->t1, sizeof l->t1, "%s", nm);
    snprintf(l->t2, sizeof l->t2, "%s", i18n(addons_ativo(i) ? "Addon ativo" : "Addon desligado"));
    snprintf(l->icone, sizeof l->icone, "addon");
    snprintf(l->chave, sizeof l->chave, "a|%.90s", nm);
  }
}

// Campo vazio: pesquisas recentes e "Em alta" (os primeiros da primeira
// fileira de catalogo de addon — o que o dono ja ve no topo da home, e o unico
// "em alta" que existe sem uma viagem de rede).
// CAMPO VAZIO: so as pesquisas recentes, curtas (o dono: "mantenha
// discreto"). Sem recentes, a barra fica sozinha — nada de "Em alta" nem de
// frase de ajuda abrindo o corpo antes de a pessoa digitar.
static void montarVazio(void) {
  int i, n = buscasrec_n();
  if (n <= 0) return;
  cabecalho(i18n("Pesquisas recentes"));
  for (i = 0; i < n && i < SP_MAX_RECENTES; i++) {
    Linha *l = nova(L_RECENTE);
    if (!l) return;
    l->ref = i;
    snprintf(l->t1, sizeof l->t1, "%s", buscasrec_termo(i));
    snprintf(l->icone, sizeof l->icone, "aj_rotate-ccw-clock");
    snprintf(l->chave, sizeof l->chave, "r|%s", l->t1);
  }
  { Linha *l = nova(L_LIMPAR);
    if (l) { snprintf(l->t1, sizeof l->t1, "%s", i18n("Limpar pesquisas recentes"));
             snprintf(l->chave, sizeof l->chave, "limpar"); } }
}

static void montarAjustes(const char *consultaLocal) {
  AjusteBuscaResultado resultados[10];
  int i, n;
  n = ajustes_buscar(consultaLocal, resultados, (int)(sizeof resultados / sizeof resultados[0]));
  if (modoAjustes && (!consultaLocal || !consultaLocal[0]) && n == 0) {
    static const char *const termos[] = {"idioma", "legenda", "tema", "qualidade", "animacoes"};
    int t;
    for (t = 0; t < (int)(sizeof termos / sizeof termos[0]) && n < 5; t++) {
      AjusteBuscaResultado sugestao[1];
      int dup = 0, j;
      if (ajustes_buscar(termos[t], sugestao, 1) != 1) continue;
      for (j = 0; j < n; j++) if (resultados[j].op == sugestao[0].op) { dup = 1; break; }
      if (!dup) resultados[n++] = sugestao[0];
    }
  }
  // NADA COM TODAS AS PALAVRAS (2.0.2): antes a busca so dizia "Nenhum ajuste
  // encontrado." Agora tenta, nesta ordem, cada palavra sozinha ("legenda
  // grande" acha as legendas) e a consulta encurtada pelo fim (o erro de
  // digitacao no fim da palavra, "legendz"). Se ainda nao ha nada, o aviso
  // vem com os termos mais buscados embaixo.
  if (modoAjustes && consultaLocal && consultaLocal[0] && n == 0) {
    // Rotulos exatos (em portugues: a busca indexa o rotulo pt em qualquer
    // idioma), para cair na linha certa e nao num vizinho que contem a palavra.
    static const char *const termos[] = {"Idioma da legenda", "Idioma do áudio", "Trailer no destaque",
                                         "Qualidade máxima", "Addons"};
    char q[128], *w, *ctx = NULL;
    size_t len;
    int t, k;
    snprintf(q, sizeof q, "%s", consultaLocal);
    for (w = strtok_r(q, " ", &ctx); w && n < 6; w = strtok_r(NULL, " ", &ctx)) {
      AjusteBuscaResultado r[3];
      int m, j, a2;
      if (strlen(w) < 3 || !strcmp(w, consultaLocal)) continue;
      m = ajustes_buscar(w, r, 3);
      for (j = 0; j < m && n < 6; j++) {
        int dup = 0;
        for (a2 = 0; a2 < n; a2++) if (resultados[a2].op == r[j].op) dup = 1;
        if (!dup) resultados[n++] = r[j];
      }
    }
    len = strlen(consultaLocal);
    for (k = (int)len - 1; !n && k >= 3; k--) {
      if (((unsigned char)consultaLocal[k] & 0xC0) == 0x80) continue;   // nao corta no meio de um caractere
      snprintf(q, sizeof q, "%.*s", k, consultaLocal);
      n = ajustes_buscar(q, resultados, 6);
    }
    if (n) cabecalho(i18n("Talvez seja um destes"));
    else {
      Linha *l = nova(L_AVISO);
      if (l) {
        snprintf(l->t1, sizeof l->t1, "%s", i18n("Nenhum ajuste encontrado."));
        snprintf(l->chave, sizeof l->chave, "aj-vazio");
      }
      cabecalho(i18n("Experimente"));
      for (t = 0; t < (int)(sizeof termos / sizeof termos[0]) && n < 5; t++)
        if (ajustes_buscar(termos[t], resultados + n, 1) == 1) n++;
    }
    for (i = 0; i < n; i++) {
      Linha *l = nova(L_AJUSTE);
      if (!l) return;
      l->ref = resultados[i].op;
      snprintf(l->t1, sizeof l->t1, "%s", resultados[i].titulo);
      snprintf(l->t2, sizeof l->t2, "%s%s%s", resultados[i].caminho,
               resultados[i].valor[0] ? "  ·  " : "", resultados[i].valor);
      snprintf(l->icone, sizeof l->icone, "%s", resultados[i].icone[0] ? resultados[i].icone : "menu_settings");
      snprintf(l->chave, sizeof l->chave, "aj|%d", resultados[i].op);
    }
    return;
  }
  // No modo dedicado, o rotulo explicita o escopo e consultas vazias podem
  // trazer sugestoes locais do proprio backend de preferencias. Na busca
  // comum o grupo so aparece quando ha um ajuste relevante.
  // MODO AJUSTES (mockup de Ajustes, quadro "busca"): com consulta, o primeiro
  // vira o MELHOR RESULTADO, com a previa da opcao; os demais em "Ajustes".
  { int topo = modoAjustes && consultaLocal && consultaLocal[0] && n > 0;
  if (topo) cabecalho(i18n("Melhor resultado"));
  else if (modoAjustes || n > 0) cabecalho(i18n("Ajustes"));
  for (i = 0; i < n && i < (int)(sizeof resultados / sizeof resultados[0]); i++) {
    Linha *l;
    if (topo && i == 1) cabecalho(i18n("Ajustes"));
    l = nova(L_AJUSTE);
    if (!l) return;
    l->ref = resultados[i].op;
    if (topo && i == 0) {
      l->ref2 = 1;
      // So a primeira frase: o melhor resultado e um resumo, nao a ajuda.
      { char *pt;
        snprintf(l->base, sizeof l->base, "%s", resultados[i].ajuda);
        pt = strstr(l->base, ". ");
        if (pt) pt[1] = 0; }
    }
    snprintf(l->t1, sizeof l->t1, "%s", resultados[i].titulo);
    snprintf(l->t2, sizeof l->t2, "%s%s%s%s", resultados[i].caminho,
             resultados[i].valor[0] ? "  ·  " : "", resultados[i].valor,
             resultados[i].bloqueado ? "  ·  " : "");
    if (resultados[i].bloqueado) {
      size_t usado = strlen(l->t2);
      snprintf(l->t2 + usado, sizeof l->t2 - usado, "%s", i18n("Indisponível"));
    }
    snprintf(l->icone, sizeof l->icone, "%s", resultados[i].icone[0] ? resultados[i].icone : "menu_settings");
    snprintf(l->chave, sizeof l->chave, "aj|%d", resultados[i].op);
  } }
}

// MODO GUIA (mockup do guia, quadro "guia-busca"): o melhor resultado com a
// imagem do recurso, os outros recursos em "No guia" (com a contagem) e, embaixo,
// os ajustes que casam, para quem quer ir direto a opcao. Campo vazio: os
// recursos novos da 1.8.0.
static void montarGuia(const char *q) {
  int res[16], n, i;
  if (!q || !q[0]) {
    cabecalho(i18n("Novo na 2.0"));
    for (i = 0, n = 0; i < 200 && n < 5; i++) {
      Linha *l;
      if (!ajustes_guia_titulo(i)[0]) break;
      if (!ajustes_guia_novo(i)) continue;
      l = nova(L_GUIA);
      if (!l) return;
      l->ref = i;
      snprintf(l->t1, sizeof l->t1, "%s", i18n(ajustes_guia_titulo(i)));
      snprintf(l->t2, sizeof l->t2, "%s  ·  %s", i18n(ajustes_guia_capitulo(i)), i18n(ajustes_guia_texto(i)));
      snprintf(l->icone, sizeof l->icone, "%s", ajustes_guia_icone(i));
      snprintf(l->chave, sizeof l->chave, "g|%d", i);
      n++;
    }
    return;
  }
  n = ajustes_guia_buscar(q, res, 16);
  for (i = 0; i < n && i < 7; i++) {
    Linha *l;
    if (i == 0) cabecalho(i18n("Melhor resultado"));
    if (i == 1) {
      cabecalho(i18n("No guia"));
      snprintf(lin[nLin - 1].t2, sizeof lin[nLin - 1].t2, "%d", n);
    }
    l = nova(L_GUIA);
    if (!l) return;
    l->ref = res[i];
    l->ref2 = i == 0;
    snprintf(l->t1, sizeof l->t1, "%s", i18n(ajustes_guia_titulo(res[i])));
    if (i == 0) {
      char onde[400], *p;
      snprintf(onde, sizeof onde, "%s", ajustes_guia_onde(res[i]));
      p = strstr(onde, " | ");
      if (p) *p = 0;
      // A trilha traduzida pedaco a pedaco, como no inspetor do guia.
      { char tr[400] = "", *s = onde, *sep;
        while (s && *s) {
          size_t k = strlen(tr);
          sep = strstr(s, " › ");
          if (sep) *sep = 0;
          snprintf(tr + k, sizeof tr - k, "%s%s", k ? " › " : "", i18n(s));
          s = sep ? sep + strlen(" › ") : NULL;
        }
        snprintf(l->t2, sizeof l->t2, "%s · %s", i18n(ajustes_guia_capitulo(res[i])), tr); }
      snprintf(l->base, sizeof l->base, "%s", i18n(ajustes_guia_texto(res[i])));
    } else snprintf(l->t2, sizeof l->t2, "%s · %s", i18n(ajustes_guia_capitulo(res[i])), i18n(ajustes_guia_texto(res[i])));
    snprintf(l->icone, sizeof l->icone, "%s", ajustes_guia_icone(res[i]));
    snprintf(l->chave, sizeof l->chave, "g|%d", res[i]);
  }
  { AjusteBuscaResultado r[3];
    int m = ajustes_buscar(q, r, 3), k;
    if (m > 0) cabecalho(i18n("Nos ajustes"));
    for (k = 0; k < m; k++) {
      Linha *l = nova(L_AJUSTE);
      if (!l) return;
      l->ref = r[k].op;
      snprintf(l->t1, sizeof l->t1, "%s", r[k].titulo);
      snprintf(l->t2, sizeof l->t2, "%s%s%s", r[k].caminho, r[k].valor[0] ? " · " : "", r[k].valor);
      snprintf(l->icone, sizeof l->icone, "%s", r[k].icone[0] ? r[k].icone : "menu_settings");
      snprintf(l->chave, sizeof l->chave, "aj|%d", r[k].op);
    } }
  if (nLin == 0) {
    Linha *l = nova(L_AVISO);
    if (l) { snprintf(l->t1, sizeof l->t1, i18n("Nada no guia para “%s”."), q);
             snprintf(l->chave, sizeof l->chave, "g-vazio"); }
  }
  snprintf(consultaRealce, sizeof consultaRealce, "%s", q);
}

static void remontar(void) {
  char alvo[SP_MAX_TXT * 2];
  char chaveFoco[96] = "";
  static char chavesAntes[SP_MAX_LIN][96];
  float entraAntes[SP_MAX_LIN];
  int nAntes = nLin, i, j;
  // Publicacao durante a montagem deve continuar pendente no proximo quadro.
  ultimaRevCatalogo = cat_revisao_itens();
  if (focoL >= 0 && focoL < nLin) snprintf(chaveFoco, sizeof chaveFoco, "%s", lin[focoL].chave);
  for (i = 0; i < nLin; i++) { memcpy(chavesAntes[i], lin[i].chave, 96); entraAntes[i] = entraLin[i]; }
  snprintf(montada, sizeof montada, "%s", consulta);
  nLin = 0;
  busca_normalizar(consulta, alvo, sizeof alvo);
  if (modoGuia) {
    montarGuia(consulta);
  } else if (modoAjustes) {
    montarAjustes(consulta);
    if (nLin <= (modoAjustes ? 1 : 0)) {
      Linha *l = nova(L_AVISO);
      if (l) {
        snprintf(l->t1, sizeof l->t1, "%s", i18n("Nenhum ajuste encontrado."));
        snprintf(l->chave, sizeof l->chave, "aj-vazio");
      }
    }
  } else {
  // Sempre, inclusive com o campo vazio: o debounce precisa saber que o
  // texto mudou para nao disparar um termo que ja nao esta no campo.
  spotpessoa_pedir(consulta, SDL_GetTicks());
  if (busca_codepoints(alvo) < 2) montarVazio();
  else {
    desc_buscar(consulta);
    montarTitulos(alvo);
    montarPessoas(alvo);
    montarColecoes(alvo);
    montarCanais(alvo);
    montarCatalogos(alvo);
    if (desc_buscando()) {
      Linha *l = nova(L_AVISO);
      if (l) { snprintf(l->t1, sizeof l->t1, "%s", i18n("Buscando nos seus addons…"));
               snprintf(l->chave, sizeof l->chave, "aviso"); }
    } else if (nLin == 0) {
      montarAjustes(consulta);
    }
    if (nLin == 0) {
      Linha *l = nova(L_AVISO);
      if (l) { snprintf(l->t1, sizeof l->t1, i18n("Nada encontrado para “%s”."), consulta);
               snprintf(l->t2, sizeof l->t2, "%s", i18n("Confira a grafia ou tente o nome original."));
               snprintf(l->chave, sizeof l->chave, "aviso"); }
    }
  }
  }
  // Posicoes, e quem ja estava herda foco e entrada.
  { float y = 0.0f;
    for (i = 0; i < nLin; i++) {
      lin[i].h = ALTURA[lin[i].tipo];
      if (lin[i].tipo == L_AJUSTE && lin[i].ref2 == 1) lin[i].h = 194.0f;
      if (lin[i].tipo == L_GUIA && lin[i].ref2 == 1) lin[i].h = 204.0f;
      if (lin[i].tipo == L_AVISO && lin[i].t2[0]) lin[i].h += 30.0f;
      // Sem vao extra antes de um grupo: os 50 px do cabecalho ja sao o
      // "22 em cima, 10 embaixo" do kicker do mockup.
      lin[i].y = y; y += lin[i].h;
      entraLin[i] = 0.0f;
      for (j = 0; j < nAntes; j++)
        if (lin[i].chave[0] && !strcmp(chavesAntes[j], lin[i].chave)) { entraLin[i] = entraAntes[j]; break; }
    } }
  memset(animLin, 0, sizeof animLin);
  focoL = -1;
  if (chaveFoco[0])
    for (i = 0; i < nLin; i++) if (!strcmp(lin[i].chave, chaveFoco)) { focoL = i; break; }
  if (focoL < 0) for (i = 0; i < nLin; i++) if (focavel(lin[i].tipo)) { focoL = i; break; }
  if (focoL < 0 && painel == P_LISTA) painel = P_CAMPO;
  ultimoRemoto = !modoAjustes && busca_codepoints(alvo) >= 2 ? remotoTotal() : -1;
  ultimoBuscando = modoAjustes ? 0 : desc_buscando();
  ultimaGeracao = modoAjustes ? 0 : desc_busca_geracao();
  ultimaGerPessoa = modoAjustes ? 0 : spotpessoa_geracao();
}

static int temResultados(void) {
  int i;
  for (i = 0; i < nLin; i++)
    if (focavel(lin[i].tipo) && lin[i].tipo != L_RECENTE && lin[i].tipo != L_LIMPAR) return 1;
  return 0;
}
// QUANDO A BUSCA CONTA COMO FEITA: a mesma regra da tela de Busca (busca.c,
// registrarConsulta) — abriu um resultado, fechou com resultado na tela, ou
// o ditado trouxe o texto. Nunca por letra.
static void registrar(void) {
  if (modoAjustes) return;
  if (nConsulta >= 2 && temResultados()) buscasrec_registrar(consulta);
}

// --- Campo -------------------------------------------------------------------------
static void acrescentar(const char *t) {
  size_t n = strlen(t);
  if ((size_t)nConsulta + n + 1 > SP_MAX_TXT) return;
  memcpy(consulta + nConsulta, t, n);
  nConsulta += (int)n;
  consulta[nConsulta] = 0;
}
static void apagar(void) { nConsulta = (int)busca_apagar_ultimo(consulta, (size_t)nConsulta); }

// O campo inteiro de uma vez (ditado, teclado do sistema, testes). `aparar` tira
// os espacos das pontas — o reconhecedor as vezes devolve; o IME NAO apara: o
// espaco que a pessoa acabou de digitar tem de aparecer antes da proxima letra.
static void campoDefinir(const char *t, int aparar) {
  size_t i, w = 0;
  if (!t) return;
  if (aparar) while (*t == ' ' || *t == '\n') t++;
  for (i = 0; t[i] && w + 1 < sizeof consulta; i++)
    consulta[w++] = (t[i] == '\n' || t[i] == '\t') ? ' ' : t[i];
  if (aparar) while (w > 0 && consulta[w - 1] == ' ') w--;
  // nao deixa meia sequencia UTF-8 no fim (o corte em 47 bytes pode cair nela)
  if (w > 0) {
    size_t k = w;
    while (k > 0 && ((unsigned char)consulta[k - 1] & 0xC0) == 0x80) k--;
    if (k > 0) {
      unsigned char c0 = (unsigned char)consulta[k - 1];
      size_t len = c0 < 0x80 ? 1 : (c0 >= 0xF0 ? 4 : (c0 >= 0xE0 ? 3 : 2));
      if (k - 1 + len > w) w = k - 1;
    }
  }
  consulta[w] = 0;
  nConsulta = (int)w;
  if (strcmp(consulta, montada)) remontar();
}
void spot_texto_externo(const char *t) { campoDefinir(t, 1); }


static void entrarLista(void);

// --- Teclado e voz do sistema (sistexto.h) ----------------------------------------
static void abrirTecladoSis(void) { st_ime_abrir(ST_SPOT, consulta, SP_MAX_TXT - 1); }
static void ditar(void) { if (ditadoDisponivel()) st_voz_iniciar(ST_SPOT); }

// OK no campo: o teclado do sistema onde ha; senao o do app, com o foco nele.
static void okCampo(void) {
  if (imeDisponivel()) { abrirTecladoSis(); return; }
  kbAberto = 1;
  painel = P_TECLADO;
  if (kbF > kbFil) kbF = 0;
}

static void lerSistema(void) {
  char t[SP_MAX_TXT * 2];
  // DO CELULAR: o campo inteiro, como se a pessoa tivesse digitado, e o foco
  // vai para os resultados (o mesmo que o "Concluir" do teclado da TV).
  if (celb_pegar(CELB_SPOT, t, sizeof t)) {
    st_fechar(ST_SPOT);
    campoDefinir(t, 1);
    memset(t, 0, sizeof t);
    registrar();
    if (temResultados()) entrarLista(); else painel = P_CAMPO;
    return;
  }
  int r = st_ler(ST_SPOT, t, sizeof t);
  int voz = ouvindo();
  if (r == ST_NADA) return;
  if (r == ST_PEDE_TECLADO) { painel = P_CAMPO; abrirTecladoSis(); return; }
  if (r == ST_TEXTO || r == ST_FIM) campoDefinir(t, voz || r == ST_FIM);
  if (r == ST_FIM) {
    registrar();
    // Fim da fala ou "Concluir": o proximo passo provavel e escolher.
    if (temResultados()) entrarLista(); else painel = P_CAMPO;
  }
  if (r == ST_CANCELOU && painel != P_LISTA) painel = P_CAMPO;
}

// --- Ciclo de vida -------------------------------------------------------------------
void spot_abrir(int voz) {
  modoAjustes = 0;
  modoGuia = 0;
  retornoAjustesValido = 0;
  spotAbrirBase(voz, 1);
}

static void spotAbrirBase(int voz, int tecladoAuto) {
  kbMontar();
  if (!modoAjustes) guia_preparar_busca();
  aberto = 1;
  painel = P_CAMPO; kbF = 0; kbC = 0;
  kbAberto = 0; kbAnim = 0.0f; spBW = SP_BW_BASE;
  corpoH = corpoV = 0.0f; animMic = 0.0f; nivelVoz = 0.0f;
  nConsulta = 0; consulta[0] = 0; montada[0] = 0;
  scrollY = scrollAlvo = velY = 0.0f;
  temPedido = 0; okPress = okLongo = 0;
  memset(animTecla, 0, sizeof animTecla);
  nLin = 0; focoL = -1;
  memset(entraLin, 0, sizeof entraLin);
  remontar();
  // O corpo das recentes ja nasce aberto: crescer de zero a cada abertura
  // seria a barra "pulando" antes de a pessoa fazer qualquer coisa.
  printf("[spotlight] aberto (%s)\n", voz ? "voz" : "tecla");
#if defined(__linux__) && !defined(NV_TPK) && !defined(NV_ANDROID) && !defined(__EMSCRIPTEN__)
  // LG: o teclado do SISTEMA por SDL_StartTextInput nao esta ligado (ver
  // spotlight.h, "LG E O TECLADO DO SISTEMA"). Uma linha por sessao para o
  // D1 dizer o que o SDL do aparelho responde, sem chamar nada que mude a tela.
  { static int dito;
    if (!dito) {
      SDL_version v;
      dito = 1;
      SDL_GetVersion(&v);
      printf("[spotlight] lg sdl %d.%d.%d osk=%d textinput=%d\n", v.major, v.minor, v.patch,
             (int)SDL_HasScreenKeyboardSupport(), (int)SDL_IsTextInputActive());
    } }
#endif
  fflush(stdout);
  if (voz && ditadoDisponivel()) { painel = P_MIC; ditar(); }
  else if (tecladoAuto && imeDisponivel() && st_abre_sozinho()) abrirTecladoSis();
}

void spot_abrir_guia(void) {
  modoAjustes = 1;
  modoGuia = 1;
  retornoAjustesValido = 0;
  spotAbrirBase(0, 1);
}

void spot_abrir_ajustes(int voz) {
  modoAjustes = 1;
  modoGuia = 0;
  retornoAjustesValido = 0;
  spotAbrirBase(voz, 1);
}

void spot_reabrir_ajustes(void) {
  int i;
  char chave[sizeof retornoAjustesChave];
  if (!retornoAjustesValido) return;
  snprintf(chave, sizeof chave, "%s", retornoAjustesChave);
  modoAjustes = 1;
  spotAbrirBase(0, 0);
  snprintf(consulta, sizeof consulta, "%s", retornoAjustesConsulta);
  nConsulta = (int)strlen(consulta);
  painel = P_LISTA;
  kbAberto = 0;
  remontar();
  for (i = 0; i < nLin; i++) {
    if (!strcmp(lin[i].chave, chave)) { focoL = i; break; }
  }
  if (focoL >= 0 && focoL < nLin && lin[focoL].tipo == L_AJUSTE) {
    scrollAlvo = lin[focoL].y;
  } else {
    /* O catálogo local pode ter mudado: mantém a consulta e deixa foco útil. */
    focoL = primeiraFocavel();
    scrollAlvo = focoL >= 0 ? lin[focoL].y : 0.0f;
  }
  retornoAjustesValido = 0;
}

void spot_fechar(void) {
  if (!aberto) return;
  aberto = 0;
  okPress = okLongo = 0;
  kbAberto = 0;
  st_fechar(ST_SPOT);
  celb_fechar_dono(CELB_SPOT);
}

int spot_aberto(void)  { return aberto; }
int spot_visivel(void) { return aberto || entrada > 0.004f; }
int spot_cheio(void)   { return aberto && entrada >= 0.999f; }
const char *spot_consulta(void) { return consulta; }
int spot_n_linhas(void) { return nLin; }
int spot_linha_tipo(int i) { return i >= 0 && i < nLin ? lin[i].tipo : -1; }
const char *spot_linha_texto(int i) { return i >= 0 && i < nLin ? lin[i].t1 : ""; }
int spot_linha_focada(void) { return painel == P_LISTA ? focoL : -1; }
int spot_teclado_app_aberto(void) { return kbAberto; }
int spot_foco_campo(void) { return painel == P_CAMPO ? 1 : painel == P_MIC ? 2 : painel == P_CEL ? 3 : 0; }
float spot_altura_corpo(void) { return corpoH; }

int spot_pediu(SpotPedido *p) {
  if (!temPedido) return 0;
  temPedido = 0;
  if (p) *p = pedido;
  return 1;
}

static void acionar(int i) {
  Linha *l;
  if (i < 0 || i >= nLin) return;
  l = &lin[i];
  memset(&pedido, 0, sizeof pedido);
  switch (l->tipo) {
    case L_RECENTE:
      snprintf(consulta, sizeof consulta, "%s", buscasrec_termo(l->ref));
      nConsulta = (int)strlen(consulta);
      buscasrec_registrar(consulta);
      remontar();
      return;
    case L_LIMPAR:
      buscasrec_limpar();
      painel = P_CAMPO;
      remontar();
      return;
    case L_TOPO: case L_TITULO:
      // Eventos podem chegar antes do atualizar que observa a publicacao.
      pedido.indice = cat_indice_vivo(l->ref, l->id);
      if (pedido.indice < 0) { remontar(); return; }
      pedido.tipo = SPOT_TITULO; break;
    case L_PESSOA:
      pedido.tipo = SPOT_PESSOA; pedido.indice = l->ref2; pedido.tmdb = l->tmdb;
      pedido.tituloTmdb = l->tituloTmdb;
      snprintf(pedido.tituloTipo, sizeof pedido.tituloTipo, "%s", l->tituloTipo);
      snprintf(pedido.nome, sizeof pedido.nome, "%s", l->t1);
      snprintf(pedido.arte, sizeof pedido.arte, "%s", l->arte);
      break;
    case L_COLECAO:  pedido.tipo = SPOT_COLECAO;  pedido.indice = l->ref; break;
    case L_CATALOGO: pedido.tipo = SPOT_CATALOGO; pedido.indice = l->ref; break;
    case L_ADDON:    pedido.tipo = SPOT_ADDONS;   pedido.indice = l->ref; break;
    case L_AJUSTE:   pedido.tipo = SPOT_AJUSTE;   pedido.indice = l->ref; break;
    case L_GUIA:     pedido.tipo = SPOT_GUIA;     pedido.indice = l->ref; break;
    case L_CANAL:
      pedido.tipo = SPOT_CANAL;
      snprintf(pedido.id, sizeof pedido.id, "%s", l->id);
      snprintf(pedido.nome, sizeof pedido.nome, "%s", l->t1);
      snprintf(pedido.base, sizeof pedido.base, "%s", l->base);
      break;
    default: return;
  }
  if (l->tipo == L_AJUSTE) {
    snprintf(retornoAjustesConsulta, sizeof retornoAjustesConsulta, "%s", consulta);
    snprintf(retornoAjustesChave, sizeof retornoAjustesChave, "%s", l->chave);
    retornoAjustesValido = 1;
  }
  if (!modoAjustes) registrar();
  temPedido = 1;
  spot_fechar();
}

static void removerRecente(int i) {
  if (i < 0 || i >= nLin) return;
  if (lin[i].tipo == L_RECENTE) buscasrec_remover(lin[i].ref);
  else if (lin[i].tipo == L_LIMPAR) buscasrec_limpar();
  else return;
  remontar();
}

static int primeiraFocavel(void) {
  int i;
  for (i = 0; i < nLin; i++) if (focavel(lin[i].tipo)) return i;
  return -1;
}

// Cima da primeira linha volta ao campo (o unico lugar acima dela).
static void moverLista(int d) {
  int i = focoL;
  if (nLin == 0) return;
  for (;;) {
    i += d;
    if (i < 0) { if (d < 0) painel = P_CAMPO; return; }
    if (i >= nLin) return;
    if (focavel(lin[i].tipo)) { focoL = i; return; }
  }
}

// Descer aos resultados FECHA o teclado do app: ele so serve ao campo.
static void entrarLista(void) {
  if (focoL < 0 || focoL >= nLin || !focavel(lin[focoL].tipo)) focoL = primeiraFocavel();
  if (focoL >= 0) { painel = P_LISTA; kbAberto = 0; }
}

static void aplicarTecla(void) {
  if (kbF < kbFil) {
    int k = kbF * SP_KB_COLS + kbC;
    if (k < kbN) acrescentar(kbTeclas[k]);
  } else {
    switch (kbCmd[kbC]) {
      case K_ESPACO: if (nConsulta > 0 && consulta[nConsulta - 1] != ' ') acrescentar(" "); break;
      case K_APAGAR: apagar(); break;
      case K_LIMPAR: registrar(); nConsulta = 0; consulta[0] = 0; break;
      case K_FALAR:  ditar(); return;
      case K_TECLADO: abrirTecladoSis(); return;
      default: break;
    }
  }
  remontar();
}

static void kbMover(int dx, int dy) {
  if (dy) {
    int nf = kbF + dy;
    if (nf < 0) { painel = P_CAMPO; return; }   // cima da primeira fileira: o campo
    if (nf > kbFil) return;
    // Entre a grade e a fileira de comandos o x e o que conta: a coluna da
    // tecla mais perto do centro da tecla de onde se saiu.
    { GfxRect a = teclaRect(kbF, kbC);
      float cx = a.x + a.w * 0.5f, melhor = 1e9f;
      int c, nc = kbColunas(nf), alvo = 0;
      for (c = 0; c < nc; c++) {
        GfxRect b = teclaRect(nf, c);
        float d = b.x + b.w * 0.5f - cx;
        if (d < 0) d = -d;
        if (d < melhor) { melhor = d; alvo = c; }
      }
      kbF = nf; kbC = alvo; }
    return;
  }
  if (dx < 0) { if (kbC > 0) kbC--; return; }
  if (kbC + 1 < kbColunas(kbF)) kbC++;
  else entrarLista();
}

static void focarTecla(int f, int c) { painel = P_TECLADO; kbF = f; kbC = c; }
static void focarLinha(int i, int b) {
  (void)b;
  if (i >= 0 && i < nLin && focavel(lin[i].tipo)) { painel = P_LISTA; focoL = i; }
}
static void focarCampo(int a, int b) { (void)b; painel = a == 2 ? P_CEL : a ? P_MIC : P_CAMPO; }

// Baixo a partir da barra: o teclado do app (se aberto) ou a lista.
static void descerDaBarra(void) {
  if (kbAberto) painel = P_TECLADO;
  else entrarLista();
}

void spot_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto) return;
  // Teclado da TV aberto: o texto (e o Backspace) ja entraram no valor inteiro.
  if (st_evento(e)) return;
  if (e->type == SDL_TEXTINPUT) {
    // Teclado FISICO: ASCII alfanumerico chega TAMBEM como KEYDOWN (tratado
    // abaixo); o resto (acentos, cirilico, pontuacao) so por aqui.
    unsigned char c = (unsigned char)e->text.text[0];
    if (c >= 0x80 || (c > ' ' && c < 0x7f && !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                              (c >= '0' && c <= '9')))) {
      acrescentar(e->text.text);
      remontar();
    }
    return;
  }
  if (e->type != SDL_KEYDOWN && e->type != SDL_KEYUP) return;
  k = e->key.keysym.sym;

  // OK numa pesquisa recente decide na SOLTURA (toque = buscar, segurar =
  // remover), como as pilulas da tela de Busca.
  if ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && painel == P_LISTA && focoL >= 0 && focoL < nLin &&
      (lin[focoL].tipo == L_RECENTE || lin[focoL].tipo == L_LIMPAR)) {
    if (e->type == SDL_KEYDOWN) {
      if (!okPress) { okPress = 1; okLongo = 0; okDesde = SDL_GetTicks(); }
    } else if (okPress) {
      okPress = 0;
      if (okLongo) okLongo = 0;
      else acionar(focoL);
    }
    return;
  }
  if (e->type != SDL_KEYDOWN) { okPress = 0; return; }

  if (modoAjustes && (k == SDLK_AC_BACK || k == SDLK_ESCAPE ||
      e->key.keysym.scancode == NV_SCANCODE_BACK)) {
    spot_fechar();
    return;
  }
  if (k == SPOT_TECLA_ABRIR || e->key.keysym.scancode == NV_SCANCODE_YELLOW) {
    registrar(); spot_fechar(); return;
  }
  // VOLTAR desfaz na ordem inversa (ver o topo).
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    if (ouvindo()) { st_fechar(ST_SPOT); painel = P_CAMPO; return; }
    if (kbAberto || painel == P_LISTA || painel == P_TECLADO || painel == P_CEL) {
      kbAberto = 0; painel = P_CAMPO; return;
    }
    if (nConsulta > 0) { registrar(); nConsulta = 0; consulta[0] = 0; remontar(); return; }
    spot_fechar(); return;
  }
  if (k == SPOT_TECLA_VOZ) {
    if (ditadoDisponivel()) { painel = P_MIC; ditar(); }
    else { registrar(); spot_fechar(); }
    return;
  }
  if (k == SDLK_BACKSPACE || k == SDLK_DELETE) {
    if (nConsulta > 0) { apagar(); remontar(); if (painel == P_LISTA) painel = P_CAMPO; }
    else spot_fechar();
    return;
  }
  // A AZUL / CH+ do controle chega com sym "s": tecla de controle nao escreve.
  if (!(e->key.keysym.mod & (KMOD_CTRL | KMOD_ALT | KMOD_GUI)) &&
      e->key.keysym.scancode != NV_SCANCODE_BLUE &&
      ((k >= SDLK_a && k <= SDLK_z) || (k >= SDLK_0 && k <= SDLK_9) || k == SDLK_SPACE)) {
    if (k != SDLK_SPACE || (nConsulta && consulta[nConsulta - 1] != ' ')) {
      char um[2] = { (char)k, 0 };
      acrescentar(um);
      remontar();
    }
    if (painel == P_LISTA || painel == P_MIC || painel == P_CEL) painel = P_CAMPO;
    return;
  }
  switch (painel) {
    case P_CAMPO: case P_MIC: case P_CEL:
      switch (k) {
        // DIREITA anda campo -> Falar -> Celular, o que existir aqui.
        case SDLK_RIGHT:
          if (painel == P_CAMPO && ditadoDisponivel()) painel = P_MIC;
          else if (painel != P_CEL && celDisponivel()) painel = P_CEL;
          break;
        case SDLK_LEFT:
          if (painel == P_CEL && ditadoDisponivel()) painel = P_MIC;
          else if (painel != P_CAMPO) painel = P_CAMPO;
          break;
        case SDLK_DOWN: case SDLK_TAB: descerDaBarra(); break;
        case SDLK_RETURN: case SDLK_KP_ENTER:
          if (e->key.repeat) break;
          if (painel == P_MIC) ditar();
          else if (painel == P_CEL) { st_fechar(ST_SPOT); celb_abrir(CELB_SPOT, "Buscar"); }
          else okCampo();
          break;
        default: break;
      }
      return;
    case P_TECLADO:
      switch (k) {
        case SDLK_LEFT:  kbMover(-1, 0); break;
        case SDLK_RIGHT: kbMover(1, 0);  break;
        case SDLK_UP:    kbMover(0, -1); break;
        case SDLK_DOWN:  kbMover(0, 1);  break;
        case SDLK_TAB:   entrarLista();  break;
        case SDLK_RETURN: case SDLK_KP_ENTER: aplicarTecla(); break;
        default: break;
      }
      return;
    default: break;
  }
  switch (k) {
    case SDLK_LEFT: case SDLK_TAB: painel = P_CAMPO; break;
    case SDLK_UP:   moverLista(-1); break;
    case SDLK_DOWN: moverLista(1);  break;
    case SDLK_RETURN: case SDLK_KP_ENTER: acionar(focoL); break;
    default: break;
  }
}

// Altura que o corpo quer: a lista inteira (ate o teto) ou o teclado do app.
static float alturaLista(void) { return nLin ? lin[nLin - 1].y + lin[nLin - 1].h : 0.0f; }
static float alturaTeclado(void) { return (kbFil + 1) * SP_KB_PASSO - SP_TECLA_GAP + 52.0f; }
static float corpoAlvo(void) {
  float h = alturaLista(), teto = SP_CORPO_MAX - SP_CPAD_T - SP_CPAD_B - SP_RODAPE_H;
  // LISTA MAIOR QUE A ILHA: a ilha desce ate a margem e a janela mostra o
  // maximo de linhas. O desenho so admite uma linha inteira na borda inferior;
  // a navegacao rola a proxima para dentro sem encurtar a ilha.
  if (h > teto) h = teto;
  if (kbAberto && alturaTeclado() > h) h = alturaTeclado();
  if (h <= 0.0f) return 0.0f;
  h += SP_CPAD_T + SP_CPAD_B + SP_RODAPE_H;
  return h > SP_CORPO_MAX ? SP_CORPO_MAX : h;
}
// A janela da lista dentro do corpo (para a rolagem): pelo alvo, nao pela mola.
static float listaVisivel(void) {
  float h = corpoAlvo() - SP_CPAD_T - SP_CPAD_B - SP_RODAPE_H;
  return h > 0.0f ? h : 0.0f;
}

static float molaIlha(float *v, float x, float alvo, float dt) {
  int k;
  if (anim_politica_reduzida || ajustes_animacoes_reduzidas()) { *v = 0.0f; return alvo; }
  if (dt > 0.05f) dt = 0.05f;
  for (k = 0; k < 4; k++) {
    float h = dt * 0.25f, ac = SP_MOLA_W * SP_MOLA_W * (alvo - x) - 2.0f * SP_MOLA_Z * SP_MOLA_W * (*v);
    *v += ac * h;
    x += *v * h;
  }
  return x;
}

void spot_atualizar(float dt, Uint32 agora) {
  int i, f, c;
  entrada = anim_mola(entrada, aberto ? 1.0f : 0.0f, dt, aberto ? 16.0f : 22.0f);
  if (!aberto) { if (entrada < 0.004f) entrada = 0.0f; return; }

  lerSistema();
  // A RESPOSTA DA REDE CHEGA DEPOIS DA TECLA: remonta quando a contagem do termo
  // corrente muda ou quando a busca termina (o aviso "Buscando..." sai).
  if (!modoAjustes) spotpessoa_atualizar(agora);
  if (strcmp(montada, consulta) ||
      (!modoAjustes && cat_revisao_itens() != ultimaRevCatalogo)) remontar();
  else if (!modoAjustes && nConsulta >= 2) {
    int n = remotoTotal(), b = desc_buscando(), g = desc_busca_geracao();
    if (n != ultimoRemoto || b != ultimoBuscando || g != ultimaGeracao ||
        spotpessoa_geracao() != ultimaGerPessoa) remontar();
  }
  corpoH = molaIlha(&corpoV, corpoH, corpoAlvo(), dt);
  if (corpoH < 0.0f) { corpoH = 0.0f; if (corpoV < 0.0f) corpoV = 0.0f; }
  kbAnim = anim_mola(kbAnim, kbAberto ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  spBW = anim_mistura(SP_BW_BASE, SP_BW_KB, anim_suave(kbAnim));
  nivelVoz = anim_mola(nivelVoz, st_nivel(), dt, 18.0f);
  animMic = anim_mola(animMic, painel == P_MIC ? 1.0f : 0.0f, dt, NV_MOLA_FOCO);
  for (f = 0; f <= kbFil && f <= SP_KB_MAX_FIL; f++)
    for (c = 0; c < SP_KB_COLS; c++) {
      float alvo = (painel == P_TECLADO && f == kbF && c == kbC) ? 1.0f : 0.0f;
      animTecla[f][c] = anim_mola(animTecla[f][c], alvo, dt, NV_MOLA_FOCO);
    }
  animCampo = anim_mola(animCampo, painel == P_CAMPO ? 1.0f : 0.0f, dt, NV_MOLA_FOCO);
  for (i = 0; i < nLin; i++) {
    float alvo = (painel == P_LISTA && i == focoL) ? 1.0f : 0.0f;
    animLin[i] = anim_mola(animLin[i], alvo, dt, NV_MOLA_FOCO);
    entraLin[i] = anim_mola(entraLin[i], 1.0f, dt, 14.0f);
  }
  if (painel == P_LISTA && okPress && !okLongo && agora - okDesde >= NV_HOLD_MS) {
    okLongo = 1;
    removerRecente(focoL);
  }
  if (painel != P_LISTA) okPress = okLongo = 0;
  // Rolagem: so o necessario para a linha focada caber (com o cabecalho do
  // grupo dela visivel, quando ele e a linha de cima).
  if (painel == P_LISTA && focoL >= 0) {
    float topo = lin[focoL].y, base = topo + lin[focoL].h, vis = listaVisivel();
    if (focoL > 0 && lin[focoL - 1].tipo == L_CAB) topo = lin[focoL - 1].y;
    if (topo - scrollAlvo < 0.0f) scrollAlvo = topo;
    if (base - scrollAlvo > vis) scrollAlvo = base - vis;
  } else if (painel != P_LISTA) scrollAlvo = 0.0f;
  if (scrollAlvo < 0.0f) scrollAlvo = 0.0f;
  scrollY = anim_mola2(&velY, scrollY, scrollAlvo, dt, NV_MOLA2_SCROLL);
  // OK ja aciona o foco: nao espere a mola para faze-lo caber no recorte real.
  if (painel == P_LISTA && focoL >= 0 && focoL < nLin) {
    float vis = corpoH - SP_CPAD_T - SP_CPAD_B - SP_RODAPE_H;
    float topo = lin[focoL].y + (1.0f - entraLin[focoL]) * 10.0f;
    if (vis >= lin[focoL].h) {
      float y = anim_clamp(scrollY, topo + lin[focoL].h - vis, topo);
      if (y != scrollY) { scrollY = y; velY = 0.0f; }
    }
  }
}

// --- Desenho -----------------------------------------------------------------------
static float listaX = (1920.0f - SP_BW_BASE) * 0.5f + SP_ILHA_PAD,   // refeito a cada quadro
             listaW = SP_BW_BASE - 2.0f * SP_ILHA_PAD;

static void spot_veuCorpo_(void);
// Camada ampliada (escala.h): o corpo desenha na tela virtual.
void spot_veu(void) {
  ESCALA_MIN_INI(SP_ESCALA_MIN);
  spot_veuCorpo_();
  ESCALA_MIN_FIM();
}
static void spot_veuCorpo_(void) {
  // O veu do mockup: preto a 55 % sobre a tela de tras.
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0.0f, 0.0f, 0.01f, 0.55f);
}

// O MATERIAL DA ILHA, o mesmo da folha de Fontes, do menu e da ilha do relogio:
// sombra curta, miolo de vidro (gfx_vidro_folha) ou solido, luz larga e fraca
// BRANCA no canto de cima, sem aro. A luz e a mancha na cor do tema que havia
// atras da barra sairam: o acento agora so marca estado (o cursor, a voz).
static void ilhaSpot(GfxRect p, float raioPx, float a) {
  const int vid = ajustes_vidro();
  float raio;
  if (p.h < 2.0f || a <= 0.01f) return;
  if (raioPx > p.h * 0.5f) raioPx = p.h * 0.5f;
  raio = raioPx / p.h;
  gfx_rect((GfxRect){ p.x - 18.0f, p.y - 8.0f, p.w + 36.0f, p.h + 40.0f }, 0, GFX_SOMBRA,
           1.0f, 0, 0, 0.5f, 0, 0, 0, .42f * a);
  // No vidro, a folha sozinha (78 %), como a do mockup (80 %): o miolo escuro
  // extra que havia por baixo dela (01/10, sobre cartazes) fechava a ilha a
  // ponto de a arte de tras sumir — o oposto do que o vidro e.
  if (vid) gfx_vidro_folha(p, raio, a);
  else gfx_cor(p, raio, .071f, .075f, .086f, .98f * a);
  gfx_luz_canto(p, raio, p.w * .22f, -120.0f, p.w * .6f, 1, 1, 1, (vid ? .06f : .04f) * a);
}

// As duas ilhas: a barra (pilula) e, com o corpo aberto, os resultados logo
// abaixo. A de baixo nasce do vao e cresce com a mesma mola do corpo.
static void desenhaSuperficie(GfxRect p, float a) {
  GfxRect barra = { p.x, p.y, p.w, SP_BH };
  ilhaSpot(barra, SP_BH * 0.5f, a);
  if (corpoH > SP_ILHA_VAO + 4.0f) {
    GfxRect corpo = { p.x, p.y + SP_BH + SP_ILHA_VAO, p.w, corpoH - SP_ILHA_VAO };
    ilhaSpot(corpo, SP_RAIO, a * anim_clamp((corpoH - SP_ILHA_VAO) / 60.0f, 0.0f, 1.0f));
  }
}

// Texto claro do mockup (#f3f2ef) com a opacidade do papel dele: a cor
// fica a mesma e quem gradua e o alfa, como o CSS (rgba(243,242,239,.62)).
#define SP_TINTA 243, 242, 239

// Uma dica "tecla acao" (o mesmo par do rodape) e a largura dela.
static float dicaPar(float x, float y, const char *tecla, const char *acao, float a) {
  char t[96];
  snprintf(t, sizeof t, "%s %s", tecla, acao);
  { TxtLinha l = txt_linha(TXT_ILHA_APOIO, t, SP_TINTA, 255);
    if (x >= 0.0f) txt_desenhar_alpha(l, x, y, a);
    return (float)l.w; }
}

static void desenhaCampo(float dy, float a, Uint32 agora) {
  GfxRect barra = { SP_BX, SP_BY + dy, SP_BW, SP_BH };
  float ar, ag, ab, tx, xMax, ty = barra.y + barra.h * 0.5f;
  int ouve = ouvindo(), comMic = ditadoDisponivel();
  const char *av = st_dono() == ST_SPOT || !st_dono() ? st_aviso() : "";
  ajustes_acento(&ar, &ag, &ab);
  // FOCO NO CAMPO E O CURSOR, so ele: a barra inteira ja e o campo, e um
  // segundo degrau claro por dentro dela (como era) fazia a pilula parecer
  // um botao dentro de outro. O mockup nao tem: lupa, texto e o cursor no
  // acento. Com o foco no microfone/celular o cursor some e o botao acende.
  if (ponteiro_ativo()) {
    ponteiro_alvo(barra.x, barra.y, barra.w - botoesW(), barra.h,
                  focarCampo, NULL, 0, 0);
  }
  gfx_icone((GfxRect){ barra.x + 30.0f, ty - 14.0f, 28.0f, 28.0f },
            "menu_search", .95f, .95f, .94f, a);
  tx = barra.x + 30.0f + 28.0f + 18.0f;
  xMax = barra.x + barra.w - (botoesW() > 0.0f ? botoesW() + 12.0f : 30.0f) - (ouve ? 170.0f : 0.0f);
  // Sem botao a direita, a dica do campo mora la, apagada (o "Voz: segure
  // OK" do mockup — aqui o que o OK faz de verdade no campo).
  if (botoesW() <= 0.0f && painel == P_CAMPO && !ouve) {
    float w = dicaPar(-1.0f, 0, "OK", i18n("Digitar"), a);
    TxtLinha l = txt_linha(TXT_ILHA_APOIO, "OK", SP_TINTA, 255);
    dicaPar(barra.x + barra.w - 30.0f - w, ty - l.h * 0.5f, "OK", i18n("Digitar"), .45f * a);
    xMax -= w + 24.0f;
  }
  // No modo Ajustes a ponta direita diz o escopo, como kicker.
  if (modoAjustes && nConsulta && !ouve) {
    char up[96];
    float w;
    idioma_maiusc_em(ajustes_idioma(), up, sizeof up, i18n(modoGuia ? "Buscar no guia" : "Buscar nos ajustes"));
    w = txt_tracking(TXT_MINI, up, SP_TINTA, -1, 0, 0, 2.1f);
    if (xMax - w - 30.0f > tx + 200.0f) {
      TxtLinha m = txt_linha(TXT_MINI, "M", SP_TINTA, 255);
      txt_tracking(TXT_MINI, up, SP_TINTA, xMax - w, ty - m.h * 0.5f, .45f * a, 2.1f);
      xMax -= w + 30.0f;
    }
  }
  if (nConsulta) {
    TxtLinha l = txt_linha_corta(TXT_CALLOUT, consulta, SP_TINTA, 255, xMax - tx - 10.0f);
    txt_desenhar_alpha(l, tx, ty - l.h * 0.5f, a);
    tx += l.w + 2.0f;
  } else {
    const char *ph = ouve ? i18n(st_estado() == ST_PERMISSAO ? "Permita o microfone para falar…" : "Ouvindo…")
                   : av[0] ? i18n(av)
                   : modoAjustes ? i18n(modoGuia ? "Buscar no guia" : "Buscar nos ajustes")
                   : i18n(comMic ? "Buscar ou falar: filmes, séries, pessoas, canais"
                                 : "Buscar filmes, séries, pessoas e canais");
    int amb = !ouve && av[0];
    TxtLinha l = amb ? txt_linha_corta(TXT_ILHA_META, ph, 240, 190, 130, 255, xMax - tx - 10.0f)
                     : txt_linha_corta(TXT_CALLOUT, ph, SP_TINTA, 255, xMax - tx - 10.0f);
    txt_desenhar_alpha(l, tx, ty - l.h * 0.5f, (amb ? 0.95f : 0.45f) * a);
  }
  // Cursor: 2 x 30 no acento (o do mockup), com o foco no campo ou com o
  // teclado do sistema aberto para ele.
  if ((painel == P_CAMPO || painel == P_TECLADO || digitandoSis()) && (agora / 500) % 2 == 0 &&
      (nConsulta > 0 || digitandoSis()))
    gfx_cor((GfxRect){ tx, ty - 15.0f, 2.0f, 30.0f }, 0.5f, ar, ag, ab, 0.95f * a);
  // MICROFONE: botao focavel a direita, so onde ha voz (nao se promete o que a
  // TV nao faz). Ouvindo, acende na cor do tema e um anel cresce com o som.
  if (comMic) {
    float d = SP_MIC_D, cx = barra.x + barra.w - 12.0f - d - (celDisponivel() ? d + 10.0f : 0.0f);
    float cy = barra.y + (barra.h - d) * 0.5f;
    float k = animMic;
    if (ponteiro_ativo()) ponteiro_alvo(cx - 8, barra.y, d + 16, barra.h, focarCampo, NULL, 1, 0);
    if (ouve) {
      float anel = 10.0f + 26.0f * nivelVoz + 4.0f * SDL_sinf(agora * 0.006f);
      float aro = 6.0f + 20.0f * nivelVoz;
      // Com texto no campo o placeholder "Ouvindo…" some: a palavra vai ao lado.
      if (nConsulta) {
        TxtLinha o = txt_linha(TXT_CAPTION, i18n("Ouvindo…"), 200, 204, 212, 255);
        txt_desenhar_alpha(o, cx - 22.0f - o.w, barra.y + (barra.h - o.h) * 0.5f, 0.9f * a);
      }
      // O anel que respira com a voz: e o que diz "estou ouvindo VOCE".
      gfx_vidro_aro((GfxRect){ cx - aro, cy - aro, d + 2 * aro, d + 2 * aro }, 0.5f, 2.0f,
                    ar, ag, ab, (0.25f + 0.45f * nivelVoz) * a);
      gfx_rect((GfxRect){ cx - anel, cy - anel, d + 2 * anel, d + 2 * anel }, 0, GFX_SOMBRA, 1.0f, 0, 0,
               0.5f, ar, ag, ab, (0.30f + 0.35f * nivelVoz) * a);
      gfx_cor((GfxRect){ cx - 6.0f * nivelVoz, cy - 6.0f * nivelVoz, d + 12.0f * nivelVoz, d + 12.0f * nivelVoz },
              0.5f, ar, ag, ab, a);
    } else {
      if (k > 0.01f)
        gfx_rect((GfxRect){ cx - 14, cy - 14, d + 28, d + 28 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f,
                 ar, ag, ab, 0.30f * k * a);
      // Em repouso o disco do mockup (.dsc): branco a 8 % no vidro, cinza
      // opaco no solido; o foco e a pilula cheia no acento (foco de botao).
      if (ajustes_vidro()) gfx_cor((GfxRect){ cx, cy, d, d }, 0.5f, 1, 1, 1, .08f * (1.0f - k) * a);
      else gfx_cor((GfxRect){ cx, cy, d, d }, 0.5f, .125f, .13f, .153f, (1.0f - k) * a);
      if (k > 0.01f) gfx_cor((GfxRect){ cx, cy, d, d }, 0.5f, ar, ag, ab, k * a);
    }
    { int t = (ouve || k > 0.5f) ? ajustes_tinta_foco() : 224;
      gfx_icone((GfxRect){ cx + 14, cy + 14, d - 28, d - 28 }, "aj_mic",
                t / 255.0f, t / 255.0f, t / 255.0f, a); }
  }
  // CELULAR: o ultimo da barra, sempre na mesma ponta.
  if (celDisponivel()) {
    float d = SP_MIC_D;
    celb_botao(CELB_SPOT, (GfxRect){ barra.x + barra.w - 12.0f - d, barra.y + (barra.h - d) * 0.5f, d, d },
               painel == P_CEL, focarCampo, 2, 0, a);
  }
}

static void desenhaTeclado(float dy, float a) {
  int f, c;
  float ar, ag, ab, ka = anim_suave(kbAnim), sx = (1.0f - ka) * -18.0f;
  if (kbAnim < 0.01f) return;
  a *= ka;
  ajustes_acento(&ar, &ag, &ab);
  for (f = 0; f <= kbFil; f++)
    for (c = 0; c < kbColunas(f); c++) {
      float k = animTecla[f][c], esc = 1.0f + 0.08f * k;
      GfxRect b = teclaRect(f, c), t;
      const char *s = "", *ic = NULL;
      int tom;
      b.y += dy; b.x += sx;
      t = (GfxRect){ b.x - b.w * (esc - 1) * 0.5f, b.y - b.h * (esc - 1) * 0.5f, b.w * esc, b.h * esc };
      // Tecla em repouso: branco a 7 % no vidro, cinza opaco no solido; o foco
      // e a pilula cheia no acento (foco de botao), como ja era.
      if (ajustes_vidro()) gfx_cor(t, 0.16f, 1, 1, 1, .07f * a);
      else gfx_cor(t, 0.16f, .125f, .13f, .153f, a);
      if (k > 0.01f) {
        gfx_rect((GfxRect){ t.x - 12, t.y - 12, t.w + 24, t.h + 24 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f,
                 ar, ag, ab, 0.28f * k * a);
        gfx_cor(t, 0.16f, ar, ag, ab, k * a);
      }
      if (ponteiro_ativo() && kbAberto) ponteiro_alvo(b.x, b.y, b.w, b.h, focarTecla, NULL, f, c);
      if (f < kbFil) s = kbTeclas[f * SP_KB_COLS + c];
      else switch (kbCmd[c]) {
        case K_ESPACO:  s = i18n("espaço"); break;
        case K_APAGAR:  s = i18n("apagar"); break;
        case K_LIMPAR:  s = i18n("limpar"); break;
        case K_FALAR:   ic = "aj_mic"; break;
        case K_TECLADO: ic = "aj_keyboard"; break;
      }
      tom = (int)anim_mistura(226.0f, (float)ajustes_tinta_foco(), k);
      if (ic) gfx_icone((GfxRect){ t.x + (t.w - 30) * 0.5f, t.y + (t.h - 30) * 0.5f, 30, 30 }, ic,
                        tom / 255.0f, tom / 255.0f, tom / 255.0f, a);
      else {
        TxtLinha l = txt_linha(f < kbFil ? TXT_PAINEL_ITEM : TXT_CAPTION, s, tom, tom, tom, 255);
        txt_desenhar_alpha(l, t.x + (t.w - l.w) * 0.5f, t.y + (t.h - l.h) * 0.5f, a);
      }
    }
}

// Arte de uma linha no retangulo `r`, com esqueleto enquanto nao chega.
static void arte(GfxRect r, const char *url, float raio, int circulo, float a) {
  GLuint tex = (url && url[0]) ? tex_obter_larg(url, r.w) : 0;
  if (tex) {
    if (circulo) gfx_rect(r, tex, GFX_AVATAR, 0, 0, 0, 0.0f, 0, 0, 0, a);
    else {
      gfx_tex_aspect_atual = tex_aspecto(url);
      gfx_rect(r, tex, GFX_CARD, 0.0f, 0.0f, 0.0f, raio, 0, 0, 0, a);
      gfx_tex_aspect_atual = 0.0f;
    }
  } else if (url && url[0] && !tex_falhou(url))
    gfx_esqueleto(r, circulo ? 0.5f : raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, a);
  else gfx_cor(r, circulo ? 0.5f : raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, a);
}

// CABECALHO DE GRUPO: o kicker das ilhas ("MELHOR RESULTADO", "TITULOS"):
// 15/700 em caixa alta, espacado (.14em), branco a 45 %, colado embaixo.
static void kicker(const char *t, float x, float yBase, float a) {
  char up[200];
  idioma_maiusc_em(ajustes_idioma(), up, sizeof up, t);
  { TxtLinha l = txt_linha(TXT_MINI, "M", SP_TINTA, 255);
    txt_tracking(TXT_MINI, up, SP_TINTA, x, yBase - l.h, .45f * a, 2.1f); }
}

// O termo buscado realcado no texto (Bold e branco cheio), quebrando em linhas
// de `w`: palavra a palavra, com o trecho que casa partido para fora dela.
static size_t realceDesde;   // o realce so vale a partir deste byte (pula "Capitulo · ")
static const char *achaRealce(const char *s, size_t *n) {
  size_t q = strlen(consultaRealce), i, j;
  if (!q || strlen(s) < realceDesde) return NULL;
  for (i = realceDesde; s[i]; i++) {
    for (j = 0; j < q && s[i + j]; j++) {
      unsigned char a = (unsigned char)s[i + j], b = (unsigned char)consultaRealce[j];
      if (a >= 'A' && a <= 'Z') a = (unsigned char)(a + 32);
      if (b >= 'A' && b <= 'Z') b = (unsigned char)(b + 32);
      if (a != b) break;
    }
    if (j == q) { *n = q; return s + i; }
  }
  return NULL;
}
static float textoRealce(const char *s, TxtEstilo eN, TxtEstilo eB, float x, float y, float w,
                         float lh, int maxL, float aN, float aB) {
  size_t mn = 0;
  const char *m = achaRealce(s, &mn), *p = s;
  float xx = x, yy = y, esp = (float)(txt_largura(eN, "a a") - txt_largura(eN, "aa"));
  int linha = 1;
  while (*p) {
    char pal[200];
    size_t k = 0;
    while (*p == ' ') p++;
    if (!*p) break;
    while (p[k] && p[k] != ' ' && k < sizeof pal - 1) k++;
    { float pw = 0;
      const char *a0 = p;
      size_t t;
      for (t = 0; t < k;) {   // mede a palavra, com o trecho realcado em Bold
        int dentro = m && a0 + t >= m && a0 + t < m + mn;
        size_t u = t;
        while (u < k && (m && a0 + u >= m && a0 + u < m + mn) == dentro) u++;
        snprintf(pal, sizeof pal, "%.*s", (int)(u - t), a0 + t);
        pw += (float)txt_largura(dentro ? eB : eN, pal);
        t = u;
      }
      if (xx > x && xx + pw > x + w) {
        if (linha >= maxL) {
          TxtLinha r = txt_linha(eN, "…", SP_TINTA, 255);
          txt_desenhar_alpha(r, xx, yy, aN);
          return yy + lh - y;
        }
        linha++; xx = x; yy += lh;
      }
      for (t = 0; t < k;) {
        int dentro = m && a0 + t >= m && a0 + t < m + mn;
        size_t u = t;
        TxtLinha l;
        while (u < k && (m && a0 + u >= m && a0 + u < m + mn) == dentro) u++;
        snprintf(pal, sizeof pal, "%.*s", (int)(u - t), a0 + t);
        l = dentro ? txt_linha(eB, pal, 255, 255, 255, 255) : txt_linha(eN, pal, SP_TINTA, 255);
        txt_desenhar_alpha(l, xx, yy, dentro ? aB : aN);
        xx += l.w;
        t = u;
      }
      xx += esp; }
    p += k;
  }
  return yy + lh - y;
}
// Uma linha do modo guia: o melhor resultado com a imagem do recurso, ou a
// linha com o icone no ladrilho e "Capitulo · frase" com o termo realcado.
static void desenhaGuia(Linha *l, GfxRect r, float f, float a1, float a2, float a) {
  int vidro = ajustes_vidro();
  if (l->ref2 == 1) {
    GfxRect art = { r.x + 18.0f, r.y + 18.0f, 300.0f, 168.0f };
    float tx = art.x + art.w + 26.0f, dw = 0.0f, tw, ty;
    TxtLinha t, m, o;
    ajustes_guia_imagem(l->ref, art.x, art.y, art.w, art.h);
    o = txt_linha(TXT_ILHA_APOIO, i18n("OK abre"), SP_TINTA, 255);
    if (f > 0.02f) { txt_desenhar_alpha(o, r.x + r.w - 28.0f - o.w, r.y + (r.h - o.h) * 0.5f, .5f * f * a); dw = o.w + 40.0f; }
    tw = r.x + r.w - tx - 26.0f - dw;
    t = txt_linha_corta(TXT_AJ_INSP, l->t1, SP_TINTA, 255, tw);
    m = txt_linha_corta(TXT_ILHA_META, l->t2, SP_TINTA, 255, tw);
    ty = r.y + 38.0f;
    txt_desenhar_alpha(t, tx, ty, a);
    txt_desenhar_alpha(m, tx, ty + t.h + 6.0f, .62f * a);
    textoRealce(l->base, TXT_AJ_ESTADO, TXT_AJ_16B, tx, ty + t.h + 6.0f + m.h + 12.0f,
                tw < 600.0f ? tw : 600.0f, 25.0f, 2, .5f * a, a);
    return;
  }
  { GfxRect dc = { r.x + 18.0f, r.y + (r.h - 52.0f) * 0.5f, 52.0f, 52.0f };
    float tx = dc.x + 52.0f + 18.0f, tw = r.x + r.w - tx - 16.0f;
    TxtLinha t = txt_linha_corta(TXT_ILHA_ITEM, l->t1, SP_TINTA, 255, tw);
    if (vidro) gfx_cor(dc, 16.0f / 52.0f, 1, 1, 1, .07f * a);
    else gfx_cor(dc, 16.0f / 52.0f, .125f, .129f, .153f, a);
    gfx_icone((GfxRect){ dc.x + 14, dc.y + 14, 24, 24 }, l->icone, .953f, .949f, .937f, .75f * a);
    txt_desenhar_alpha(t, tx, r.y + 12.0f, a1 * a);
    { const char *p = strstr(l->t2, " · ");
      realceDesde = p ? (size_t)(p - l->t2) : 0;
      textoRealce(l->t2, TXT_ILHA_APOIO, TXT_AJ_16B, tx, r.y + 12.0f + t.h + 3.0f, tw, 22.0f, 1, a2 * a, a);
      realceDesde = 0; } }
}

static void desenhaLinha(int i, float x, float y, float a) {
  Linha *l = &lin[i];
  float f = animLin[i], w = listaW;
  int vidro = ajustes_vidro();
  float a1, a2;                 // alfa do nome e da linha de apoio
  GfxRect r = { x, y, w, l->h };
  if (l->tipo == L_CAB) {
    kicker(l->t1, x + 12.0f, y + l->h - 11.0f, a);
    if (l->t2[0]) {   // a contagem do grupo ("No guia 4")
      char up[200];
      float kw;
      TxtLinha c = txt_linha(TXT_MINI, l->t2, SP_TINTA, 255);
      idioma_maiusc_em(ajustes_idioma(), up, sizeof up, l->t1);
      kw = txt_tracking(TXT_MINI, up, SP_TINTA, -1, 0, 0, 2.1f);
      txt_desenhar_alpha(c, x + 12.0f + kw + 8.0f, y + l->h - 11.0f - c.h, .35f * a);
    }
    return;
  }
  if (l->tipo == L_AVISO) {
    TxtLinha t = txt_linha_corta(TXT_PG_ROTULO, l->t1, SP_TINTA, 255, w - 32.0f);
    txt_desenhar_alpha(t, x + 16.0f, y + 16.0f, .88f * a);
    if (l->t2[0]) {
      TxtLinha s = txt_linha_corta(TXT_ILHA_APOIO, l->t2, SP_TINTA, 255, w - 32.0f);
      txt_desenhar_alpha(s, x + 16.0f, y + 16.0f + t.h + 6.0f, .48f * a);
    }
    return;
  }
  if (ponteiro_ativo()) ponteiro_alvo(r.x, r.y, r.w, r.h, focarLinha, NULL, i, 0);
  // Foco: SUPERFICIE UM DEGRAU MAIS CLARA (Glass UI, a .row.foco do mockup):
  // branco a 12 % no vidro, cinza opaco no solido, raio 22. Sem contorno e sem
  // bloco cheio no acento — o dono pediu foco so por superficie. O texto nao
  // inverte: o nome vai de 88 % a cheio.
  if (f > 0.01f) {
    float rr = 22.0f / r.h;
    if (vidro) gfx_cor(r, rr, 1, 1, 1, .12f * f * a);
    else gfx_cor(r, rr, .17f, .176f, .204f, f * a);
  }
  a1 = anim_mistura(.88f, 1.0f, f);
  a2 = anim_mistura(.48f, .58f, f);
  if (l->tipo == L_TOPO) {
    GfxRect art = { r.x + 16.0f, r.y + 16.0f, 250.0f, r.h - 32.0f };
    float tx, ty, bloco, dw = 0.0f;
    // Sem paisagem, o cartaz em pe ocupa o mesmo lugar (e a caixa encolhe).
    if (!l->paisagem) art.w = art.h * 2.0f / 3.0f;
    arte(art, l->arte, 14.0f / art.h, 0, a);
    tx = art.x + art.w + 24.0f;
    // "OK Abrir" a direita, no meio da altura: so com o foco nela.
    if (f > 0.02f) {
      TxtLinha o = txt_linha(TXT_ILHA_APOIO, "OK", SP_TINTA, 255);
      dw = dicaPar(-1.0f, 0, "OK", i18n("Abrir"), a);
      dicaPar(r.x + r.w - 26.0f - dw, r.y + (r.h - o.h) * 0.5f, "OK", i18n("Abrir"), .5f * f * a);
      dw += 40.0f;
    }
    { float tw = r.x + r.w - tx - 26.0f - dw;
      TxtLinha t = txt_linha_corta(TXT_ROW_TITULO, l->t1, SP_TINTA, 255, tw);
      TxtLinha m = txt_linha_corta(TXT_ILHA_META, l->t2, SP_TINTA, 255, tw);
      TxtLinha g = { 0, 0, 0 };
      if (l->genero[0]) g = txt_linha_corta(TXT_ILHA_GENERO, l->genero, SP_TINTA, 255, tw);
      bloco = t.h + 6.0f + m.h + (g.h ? 6.0f + g.h : 0.0f);
      ty = r.y + (r.h - bloco) * 0.5f;
      txt_desenhar_alpha(t, tx, ty, a);
      txt_desenhar_alpha(m, tx, ty + t.h + 6.0f, .62f * a);
      if (g.h) txt_desenhar_alpha(g, tx, ty + t.h + 6.0f + m.h + 6.0f, .42f * a); }
    return;
  }
  if (l->tipo == L_GUIA) {
    desenhaGuia(l, r, f, a1, a2, a);
    return;
  }
  if (l->tipo == L_AJUSTE && l->ref2 == 1) {
    // MELHOR RESULTADO DE AJUSTE: a previa da opcao (280 x 158), o nome,
    // o caminho com o valor e a frase da opcao.
    GfxRect art = { r.x + 18.0f, r.y + 18.0f, 280.0f, 158.0f };
    float tx = art.x + art.w + 26.0f, dw = 0.0f, tw, ty, bloco;
    TxtLinha t, m, o;
    ajustes_previa_busca(l->ref, art.x, art.y, art.w, art.h, a);
    o = txt_linha(TXT_ILHA_APOIO, "OK abre", SP_TINTA, 255);
    if (f > 0.02f) { txt_desenhar_alpha(o, r.x + r.w - 28.0f - o.w, r.y + (r.h - o.h) * 0.5f, .5f * f * a); dw = o.w + 40.0f; }
    tw = r.x + r.w - tx - 26.0f - dw;
    t = txt_linha_corta(TXT_AJ_INSP, l->t1, SP_TINTA, 255, tw);
    m = txt_linha_corta(TXT_ILHA_META, l->t2, SP_TINTA, 255, tw);
    bloco = t.h + 6.0f + m.h + 10.0f + 50.0f;
    ty = r.y + (r.h - bloco) * 0.5f;
    txt_desenhar_alpha(t, tx, ty, a);
    txt_desenhar_alpha(m, tx, ty + t.h + 6.0f, .62f * a);
    if (l->base[0]) txt_bloco(TXT_AJ_ESTADO, l->base, SP_TINTA, tx, ty + t.h + 6.0f + m.h + 10.0f,
                              tw < 560.0f ? tw : 560.0f, 25.0f, .45f * a, 2);
    return;
  }
  { GfxRect ic = { r.x + 16.0f, r.y + 12.0f, 0, r.h - 24.0f };
    float tx;
    switch (l->tipo) {
      case L_TITULO:  ic.w = 42.0f; arte(ic, l->arte, 8.0f / ic.h, 0, a); break;
      case L_PESSOA:
        ic.w = ic.h;
        if (l->arte[0]) arte(ic, l->arte, 0.5f, 1, a);
        else {
          // Sem foto no TMDB: o disco com o icone de pessoa, nao um buraco.
          gfx_cor(ic, 0.5f, 0.20f, 0.205f, 0.225f, a);
          gfx_icone((GfxRect){ ic.x + ic.w * 0.25f, ic.y + ic.h * 0.25f, ic.w * 0.5f, ic.h * 0.5f },
                    "aj_user-round", 0.78f, 0.79f, 0.82f, a);
        }
        break;
      case L_COLECAO: ic.w = ic.h * 16.0f / 9.0f; arte(ic, l->arte, 8.0f / ic.h, 0, a); break;
      case L_CANAL:
        ic.w = ic.h * 16.0f / 9.0f;
        gfx_cor(ic, 8.0f / ic.h, 0.16f, 0.165f, 0.185f, a);
        guia_logo_desenhar(l->arte, l->t1, ic, ic.w - 16.0f, ic.h - 16.0f, 0.965f, a);
        break;
      case L_AJUSTE: {
        // O icone da SECAO num ladrilho de 52 (raio 16), como no indice.
        GfxRect dc = { r.x + 18.0f, r.y + (r.h - 52.0f) * 0.5f, 52.0f, 52.0f };
        if (vidro) gfx_cor(dc, 16.0f / 52.0f, 1, 1, 1, .07f * a);
        else gfx_cor(dc, 16.0f / 52.0f, .125f, .129f, .153f, a);
        gfx_icone((GfxRect){ dc.x + 14, dc.y + 14, 24, 24 }, l->icone, .953f, .949f, .937f, .75f * a);
        ic.x = dc.x; ic.w = 52.0f;
        break; }
      default: {
        // Icone num disco: recente, limpar, catalogo, addon.
        float d = 42.0f;
        GfxRect dc = { r.x + 16.0f, r.y + (r.h - d) * 0.5f, d, d };
        int tt = 210;
        gfx_cor(dc, 0.5f, 1.0f, 1.0f, 1.0f, (0.08f + 0.04f * (1.0f - f)) * a);
        gfx_icone((GfxRect){ dc.x + 10, dc.y + 10, d - 20, d - 20 },
                  l->icone[0] ? l->icone : "aj_rotate-ccw-clock",
                  tt / 255.0f, tt / 255.0f, tt / 255.0f, l->icone[0] ? a : 0.6f * a);
        ic.w = d;
        break; }
    }
    tx = ic.x + ic.w + 18.0f;
    if (l->tipo == L_RECENTE || l->tipo == L_LIMPAR) {
      TxtLinha t = txt_linha_corta(l->tipo == L_LIMPAR ? TXT_ILHA_META : TXT_PG_ROTULO, l->t1,
                                   SP_TINTA, 255, r.x + r.w - tx - 16.0f);
      txt_desenhar_alpha(t, tx, r.y + (r.h - t.h) * 0.5f, (l->tipo == L_LIMPAR ? .62f : a1) * a);
      // A barra da pressao longa: solte antes de encher e nao apaga.
      if (painel == P_LISTA && i == focoL && okPress && !okLongo) {
        float p = anim_clamp((SDL_GetTicks() - okDesde) / (float)NV_HOLD_MS, 0.0f, 1.0f);
        if (p > 0.02f) gfx_cor((GfxRect){ r.x + 18.0f, r.y + r.h - 8.0f, (r.w - 36.0f) * p, 4.0f },
                               0.5f, 1, 1, 1, 0.9f * a);
      }
      return;
    }
    { TxtLinha t = txt_linha_corta(TXT_ILHA_ITEM, l->t1, SP_TINTA, 255, r.x + r.w - tx - 16.0f);
      TxtLinha m = txt_linha_corta(TXT_ILHA_APOIO, l->t2, SP_TINTA, 255, r.x + r.w - tx - 16.0f);
      float bloco = t.h + (l->t2[0] ? 3.0f + m.h : 0);
      float ty = r.y + (r.h - bloco) * 0.5f;
      txt_desenhar_alpha(t, tx, ty, a1 * a);
      if (l->t2[0]) txt_desenhar_alpha(m, tx, ty + t.h + 3.0f, a2 * a); }
  }
}

static void desenhaLista(float dy, float a) {
  int i;
  float topo = SP_CORPO_Y + SP_CPAD_T + dy;
  float vis = corpoH - SP_CPAD_T - SP_CPAD_B - SP_RODAPE_H;
  if (vis <= 2.0f) return;
  gfx_recorte(listaX - 30.0f, topo - 8.0f, listaW + 60.0f, vis + 8.0f);
  for (i = 0; i < nLin; i++) {
    float y = topo + lin[i].y - scrollY;
    float e = entraLin[i];
    if (y > topo + vis + 20.0f || y + lin[i].h < topo - 20.0f) continue;
    if (lin[i].h <= vis && y + lin[i].h + (1.0f - e) * 10.0f > topo + vis + 0.1f) continue;
    // Linha nova sobe 10 px e acende; a que ja estava fica parada.
    desenhaLinha(i, listaX, y + (1.0f - e) * 10.0f, a * e);
  }
  gfx_sem_recorte();
}

// Dicas do controle em pares tecla + acao, numa cor so (branco a 42 %, o
// rodape do mockup), 24 px entre um par e o outro.
static float dica(float x, float y, const char *tecla, const char *acao, float a) {
  return x + dicaPar(x, y, tecla, acao, .42f * a) + 24.0f;
}

static void desenhaRodape(float dy, float a) {
  float x = SP_LISTA_X0 + 16.0f, y = SP_CORPO_Y + corpoH - SP_RODAPE_H + 14.0f + dy;
  int recente = painel == P_LISTA && focoL >= 0 && focoL < nLin && lin[focoL].tipo == L_RECENTE;
  a *= anim_clamp((corpoH - 80.0f) / 80.0f, 0.0f, 1.0f);
  if (a < 0.01f) return;
  if (painel == P_TECLADO) {
    x = dica(x, y, "OK", i18n("Digitar"), a);
    x = dica(x, y, "\xe2\x86\x92", i18n("Resultados"), a);
    dica(x, y, i18n("Voltar"), i18n("Campo"), a);
  } else if (painel == P_LISTA) {
    x = dica(x, y, "OK", i18n(recente ? "Buscar de novo" : "Abrir"), a);
    if (recente) x = dica(x, y, i18n("Segure OK"), i18n("Remover"), a);
    dica(x, y, i18n("Voltar"), i18n("Campo"), a);
  } else {
    // O "OK Digitar" do campo sem botoes ja esta na ponta da barra.
    if (botoesW() > 0.0f || painel != P_CAMPO)
      x = dica(x, y, "OK", i18n(painel == P_MIC ? "Falar" : painel == P_CEL ? "Digitar pelo celular" : "Digitar"), a);
    x = dica(x, y, "\xe2\x86\x93", i18n("Resultados"), a);
    dica(x, y, i18n("Voltar"), i18n(nConsulta > 0 ? "Limpar" : "Fechar"), a);
  }
}

static void spot_desenharCorpo_(Uint32 agora, int veuPronto);
// Camada ampliada (escala.h): o corpo desenha na tela virtual.
void spot_desenhar(Uint32 agora, int veuPronto) {
  ESCALA_MIN_INI(SP_ESCALA_MIN);
  spot_desenharCorpo_(agora, veuPronto);
  ESCALA_MIN_FIM();
}
static void spot_desenharCorpo_(Uint32 agora, int veuPronto) {
  float a, dy;
  GfxRect sup;
  if (entrada < 0.004f) return;
  a = anim_suave(entrada);
  dy = (1.0f - a) * -24.0f;
  ponteiro_camada();
  if (!veuPronto) {
    float ga = gfx_opacidade_grupo;
    gfx_opacidade_grupo = ga * a;
    spot_veu();
    gfx_opacidade_grupo = ga;
  }
  { float ka = anim_suave(kbAnim);
    listaX = anim_mistura(SP_LISTA_X0, SP_LISTA_X1, ka);
    listaW = SP_LISTA_XF - listaX; }
  sup = (GfxRect){ SP_BX, SP_BY + dy, SP_BW, SP_BH + (corpoH > 0.0f ? corpoH : 0.0f) };
  // A SUPERFICIE TODA e um anteparo para o ponteiro: clique fora das teclas e
  // das linhas nao vaza para a tela de tras.
  if (ponteiro_ativo()) ponteiro_alvo(sup.x, sup.y, sup.w, sup.h, NULL, NULL, 0, 0);
  desenhaSuperficie(sup, a);
  desenhaCampo(dy, a, agora);
  if (corpoH > 2.0f) {
    gfx_recorte(SP_BX, SP_CORPO_Y + dy, SP_BW, corpoH);
    desenhaTeclado(dy, a);
    gfx_sem_recorte();
    desenhaLista(dy, a);
    desenhaRodape(dy, a);
  }
}
