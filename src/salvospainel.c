// Painel "Salvos" — a camada da direita que a tecla AZUL abre. Ver salvospainel.h
// para o que ele substituiu e por que.
//
// O QUE ELE MOSTRA, e a decisao nao e obvia: a UNIAO das tres fontes de "quero
// ver", nao so a lista local. As tres caem na mesma marca (`CatItem.naLista`):
// a watchlist do Trakt (descoberta.c), a biblioteca da conta (contalib.c) e a
// lista local (salvos.c). A aba "Salvos" da tela de Biblioteca ja mostra essa
// uniao, e duas telas chamadas "Salvos" mostrando conjuntos diferentes seria
// exatamente o defeito que o app irmao teve com quatro botoes "+".
//
// A lista local entra por fora do catalogo de proposito. Ela guarda titulo,
// poster e meta no proprio arquivo (ver salvos.h), entao o painel se desenha no
// primeiro quadro do arranque — antes de a descoberta responder. Sem isso o
// atalho mais rapido do controle abriria vazio por ~20 s toda vez que a TV
// liga, que e justamente quando alguem aperta.
#include "salvospainel.h"
#include "salvos.h"
#include "recomenda.h"
#include "recenviar.h"
#include "catalogo.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "anim.h"
#include "layout.h"
#include "ajustes.h"
#include "idioma.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// Mesma pegada do painel "Sua atividade" que ele substitui (perfil.c desenhava
// em x=1120, 776x1032): quem ja tinha o gesto na memoria muscular encontra a
// camada no mesmo lugar, so com outro conteudo.
#define SP_X          1120.0f
#define SP_W           776.0f
#define SP_Y            24.0f
#define SP_H          1032.0f
#define SP_PAD          44.0f
#define SP_INTERNO    (SP_W - SP_PAD * 2.0f)
// ABAS. Elas so existem quando o servico de recomendacoes foi compilado
// (recomenda_ativo); sem ele o painel e exatamente o que era, sem uma linha a
// mais de cromo para uma funcao que nao existe naquele pacote.
// AS ABAS OCUPAM O LUGAR DO TITULO GRANDE, e nao uma faixa a mais. A pilula
// acesa ja diz em que secao a pessoa esta — repetir isso num "Salvos" de 40px
// logo acima gastaria 76px de altura para dizer duas vezes a mesma coisa, e a
// lista comecaria mais embaixo em todo pacote com o servico ligado.
#define SP_ABAS_Y      (SP_Y + 56.0f)
#define SP_ABAS_H        52.0f
#define SP_ABA_GAP       14.0f
#define SP_LISTA_Y     200.0f
#define SP_LISTA_BASE (SP_Y + SP_H - 24.0f)
#define SP_POSTER_W     92.0f
#define SP_POSTER_H    138.0f
#define SP_PASSO       160.0f
#define SP_SECAO_H      54.0f
// 28, e nao 20: o ponto de "nao lida" mora neste vao, e com 20 ele encostava
// na primeira letra do titulo — foi o que o dono viu na foto ampliada.
#define SP_TEXTO_X    (SP_PAD + SP_POSTER_W + 28.0f)
#define SP_TEXTO_W    (SP_INTERNO - SP_POSTER_W - 28.0f)
// Barra de progresso do card de retomada: a mesma altura da que a home usa nos
// cards de "Continuar assistindo", para as duas lerem como a mesma coisa.
#define SP_BARRA_W     360.0f
#define SP_BARRA_H       6.0f
// Entrada e saida com o MESMO relogio do menu lateral (menu.c): as duas camadas
// aparecem no mesmo app e tempos diferentes se leem como bug, nao como estilo.
#define SP_ABRIR_MS    230.0f
#define SP_FECHAR_MS   150.0f
#define SP_VEU           0.58f

#define SP_MAX 200

// Linha ja resolvida: o desenho nao volta ao catalogo nem a lista local por
// quadro.
//
// OS TEXTOS SAO COPIADOS, E NAO APONTADOS — e isto derrubou o app na TV.
//
// A primeira versao guardava `const char *titulo` apontando para dentro do
// CatItem, com um comentario afirmando que a memoria era estavel. Nao e: o
// vetor `itens` de catalogo.c e do heap e TROCA DE BLOCO a cada republicacao
// (cat_definir_tudo/cat_acrescentar_lote fazem malloc do bloco novo e liberam o
// antigo). O proprio catalogo.c documenta isso na linha da troca e segura UM
// bloco velho em `lixo` justamente porque alguem ja leu memoria liberada ali —
// mas uma folga de um bloco nao salva quem guarda o ponteiro por varios ciclos.
//
// Na TV o resultado foi core dump de 218 MB alguns segundos depois do arranque,
// quando o segundo ciclo de sync republicou o catalogo (o log parava logo apos
// "[contalib] biblioteca da conta aplicada"). No Mac, sem conta, o catalogo
// nunca era republicado e nada acontecia — o defeito so existia com dados reais.
//
// Copiar custa ~150 KB estaticos para 200 linhas. E o preco de nao depender do
// tempo de vida de um bloco que outro modulo troca sem avisar.
typedef struct {
  char  titulo[160], poster[512], meta[96];
  char  id[24];
  int   serie;
  int   nota;
  int   progresso, temporada, episodio, restanteMin;
  long long quandoS;      // 0 = veio do Trakt/conta, nao sabemos quando entrou
} SPLinha;

static SPLinha linhas[SP_MAX];
static int nLinhas;
static int nCont;            // quantas das primeiras linhas sao "Continuar"

// A ABA SOCIAL. `foco == SP_FOCO_ABAS` e a linha de cima, onde esquerda e
// direita trocam de aba; do zero para baixo o D-pad e o de sempre. Uma linha
// de foco "fora da lista" em vez de um modo separado porque o resto do painel
// (rolagem, animacao de foco, recorte) continua valendo sem mudanca nenhuma.
enum { SP_ABA_SALVOS = 0, SP_ABA_SOCIAL = 1 };
#define SP_FOCO_ABAS (-1)
static int aba;
static RecItem recs[REC_MAX];
static int nRecs;

static int aberto, foco, marcaCatN = -1;
static float entrada, scrollY;
static float animFoco[SP_MAX];
static char  pedido[24];
static int   temPedido;

int spainel_aberto(void)  { return aberto; }
int spainel_visivel(void) { return aberto || entrada > 0.002f; }

const char *spainel_pediu_abrir(void) {
  if (!temPedido) return NULL;
  temPedido = 0;
  return pedido;
}

static int ehSerie(const char *tipo, int nTemporadas) {
  return (tipo && !strcmp(tipo, "series")) || nTemporadas > 0;
}

// Retrato barato do catalogo, para detectar troca de bloco com a mesma
// contagem. Um strcmp de 16 bytes por quadro com o painel aberto.
static char marcaPrimeiro[24];
static int catTrocou(void) {
  const CatItem *c = cat_n() > 0 ? cat_item(0) : NULL;
  return c ? strcmp(c->imdb, marcaPrimeiro) != 0 : 0;
}

static int jaTem(const char *id) {
  int i;
  for (i = 0; i < nLinhas; i++) if (!strcmp(linhas[i].id, id)) return 1;
  return 0;
}

// Monta a lista visivel. Duas passadas e uma reordenacao:
//   1. a lista LOCAL, na ordem de insercao (ela existe mesmo sem catalogo);
//   2. o que o catalogo tem marcado como naLista e ainda nao entrou;
//   3. os itens COM progresso sobem para o topo, virando a secao "Continuar".
// A reordenacao e uma insercao estavel: dentro de cada secao a ordem das duas
// passadas e preservada, senao a lista dancaria a cada reconstrucao.
static void reconstruir(void) {
  int i, n, escrita = 0;
  nLinhas = 0;
  n = salvos_n();
  for (i = 0; i < n && nLinhas < SP_MAX; i++) {
    const SalvoItem *s = salvos_item(i);
    SPLinha *l;
    int k;
    if (!s) continue;
    l = &linhas[nLinhas++];
    memset(l, 0, sizeof *l);
    snprintf(l->id, sizeof l->id, "%s", s->id);
    snprintf(l->titulo, sizeof l->titulo, "%s", s->titulo);
    snprintf(l->poster, sizeof l->poster, "%s", s->poster);
    snprintf(l->meta, sizeof l->meta, "%s", s->meta);
    l->nota   = s->nota;
    l->quandoS = s->quandoS;
    l->serie  = ehSerie(s->tipo, 0);
    // O PROGRESSO SO EXISTE NO CATALOGO. A lista local guarda o que e dela
    // (titulo, poster, quando entrou); posicao de retomada e de progresso.c e
    // muda sem passar por aqui. Guardar uma copia envelheceria em minutos.
    k = cat_indice_por_imdb(s->id);
    if (k >= 0) {
      const CatItem *c = cat_item(k);
      if (c) {
        l->progresso = c->progresso;
        l->temporada = c->temporada;
        l->episodio  = c->episodio;
        l->restanteMin = c->restanteMin;
        if (c->nota > 0) l->nota = c->nota;
        if (c->poster[0]) snprintf(l->poster, sizeof l->poster, "%s", c->poster);
        if (c->meta[0])   snprintf(l->meta, sizeof l->meta, "%s", c->meta);
        if (ehSerie(c->tipo, c->nTemporadas)) l->serie = 1;
      }
    }
  }
  n = cat_n();
  for (i = 0; i < n && nLinhas < SP_MAX; i++) {
    const CatItem *c = cat_item(i);
    SPLinha *l;
    if (!c || !c->naLista || !c->imdb[0] || jaTem(c->imdb)) continue;
    l = &linhas[nLinhas++];
    memset(l, 0, sizeof *l);
    snprintf(l->id, sizeof l->id, "%s", c->imdb);
    snprintf(l->titulo, sizeof l->titulo, "%s", c->titulo);
    snprintf(l->poster, sizeof l->poster, "%s", c->poster);
    snprintf(l->meta, sizeof l->meta, "%s", c->meta);
    l->nota   = c->nota;
    l->serie  = ehSerie(c->tipo, c->nTemporadas);
    l->progresso = c->progresso;
    l->temporada = c->temporada;
    l->episodio  = c->episodio;
    l->restanteMin = c->restanteMin;
  }
  // Estavel: percorre uma vez e move para a frente quem tem progresso.
  for (i = 0; i < nLinhas; i++) {
    if (linhas[i].progresso <= 0) continue;
    if (i != escrita) {
      SPLinha t = linhas[i];
      memmove(&linhas[escrita + 1], &linhas[escrita],
              sizeof(SPLinha) * (size_t)(i - escrita));
      linhas[escrita] = t;
    }
    escrita++;
  }
  nCont = escrita;
  marcaCatN = cat_n();
  { const CatItem *c = cat_n() > 0 ? cat_item(0) : NULL;
    snprintf(marcaPrimeiro, sizeof marcaPrimeiro, "%s", c ? c->imdb : ""); }
  // O FOCO DAS ABAS (-1) NAO E UM FOCO FORA DA FAIXA. Sem esta guarda, uma
  // reconstrucao com a lista vazia jogaria o foco de volta para a linha 0, que
  // nao existe, e a linha de abas perderia o anel debaixo do dedo.
  if (foco >= 0 && foco >= nLinhas) foco = nLinhas > 0 ? nLinhas - 1 : 0;
}

// 1 quando o pacote tem o servico de recomendacoes. Com 0 nao ha aba, nao ha
// selo e nao ha uma linha de rede: o dono publica builds sem NUVIO_REC_URL.
static int temAbas(void) { return recomenda_ativo(); }

// Quantas linhas a aba corrente desenha. Uma funcao so para as duas, senao a
// rolagem e o desenho divergem na primeira mudanca.
static int nVisiveis(void) {
  // A ABA SOCIAL TEM SEMPRE UMA LINHA A MAIS: "Adicionar um amigo".
  //
  // Vazia, ela era uma frase dizendo que nao havia nada e mais nada — o D-pad
  // nao tinha para onde descer, e esse e exatamente o estado em que o dono
  // ficou preso (1 pessoa registrada, 0 contatos no servidor). Cheia, a tela
  // de amigos so seria alcancavel pelo menu de um cartaz — ou seja, para
  // adicionar alguem era preciso escolher um filme primeiro.
  if (aba == SP_ABA_SOCIAL) return nRecs + 1;
  return nLinhas;
}

static float listaTopo(void) { return SP_LISTA_Y; }

// Copia a lista de recomendacoes para dentro do painel. COPIA, e nao ponteiro:
// a lista de recomenda.c vive atras de um mutex que o fio de rede reescreve, e
// e exatamente o erro que derrubou este arquivo antes (ver a nota longa em
// SPLinha).
static void reconstruirSocial(void) {
  int i;
  nRecs = 0;
  if (!temAbas()) return;
  for (i = 0; i < REC_MAX && nRecs < REC_MAX; i++)
    if (recomenda_item(i, &recs[nRecs])) nRecs++;
    else break;
}

static void trocarAba(int nova) {
  if (!temAbas() || nova == aba) return;
  aba = nova;
  foco = SP_FOCO_ABAS;
  scrollY = 0.0f;
  memset(animFoco, 0, sizeof animFoco);
  if (aba == SP_ABA_SOCIAL) {
    // CONSULTA IMEDIATA ao entrar, para nao mostrar lista velha; e o selo some
    // porque a pessoa esta olhando justamente para ela.
    //
    // A ORDEM IMPORTA: a copia acontece ANTES de marcar como vistas, entao o
    // SELO da aba zera e os PONTOS das linhas ficam. Sao coisas diferentes —
    // o selo responde "ha algo novo?" e o ponto responde "qual delas e nova?",
    // e apagar os dois no mesmo instante deixaria a pessoa olhando uma lista
    // sem saber por que foi avisada. Na proxima abertura do painel a copia ja
    // le visto=1 e os pontos somem sozinhos.
    recomenda_pedir_agora();
    reconstruirSocial();
    recomenda_marcar_vistas();
  }
}

void spainel_abrir(void) {
  if (aberto) return;
  aberto = 1;
  foco = 0;
  aba = SP_ABA_SALVOS;
  scrollY = 0.0f;
  memset(animFoco, 0, sizeof animFoco);
  reconstruir();
  reconstruirSocial();
}

void spainel_fechar(void) { aberto = 0; }

// Altura ate o TOPO da linha `i`, contando o cabecalho de cada secao. Nao e
// `i * SP_PASSO`: o rotulo "Não começados" empurra tudo que vem depois dele, e
// sem contar esse empurrao a rolagem para a linha focada erra por 54px — o
// suficiente para o card focado ficar meio escondido atras do cabecalho.
static float topoDe(int i) {
  float y;
  // A aba Social nao tem secoes: uma recomendacao nao esta "comecada" nem
  // "nao comecada", e inventar um cabecalho so para simetria custaria 54px de
  // uma lista que ja e curta.
  if (aba == SP_ABA_SOCIAL) return (float)i * SP_PASSO;
  // Rotulo da primeira secao, sempre; mais o de "Não começados" para quem vem
  // depois dele. Com nCont == 0 nao existe segunda secao — a unica que aparece
  // e "Sua lista", e o segundo termo tem de ser zero para todo mundo.
  y = SP_SECAO_H + (float)i * SP_PASSO;
  if (nCont > 0 && i >= nCont) y += SP_SECAO_H;
  return y;
}

void spainel_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  // Mesmo conjunto de "voltar" que o menu lateral aceita, mais a ESQUERDA: o
  // painel encosta na borda direita da tela, entao sair por ele e ir para a
  // esquerda. E o gesto que perfil.c ja tinha nesta mesma posicao.
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    spainel_fechar(); return;
  }
  // ESQUERDA NA LINHA DE ABAS NAO FECHA SE HA PARA ONDE IR. Fora dela, e fora
  // da primeira aba, ela continua sendo "sair pela borda" — o gesto que
  // perfil.c ja tinha nesta posicao.
  if (k == SDLK_LEFT) {
    if (temAbas() && foco == SP_FOCO_ABAS && aba != SP_ABA_SALVOS) {
      trocarAba(SP_ABA_SALVOS); return;
    }
    spainel_fechar(); return;
  }
  if (k == SDLK_RIGHT) {
    if (temAbas() && foco == SP_FOCO_ABAS) trocarAba(SP_ABA_SOCIAL);
    return;
  }
  if (k == SDLK_DOWN) {
    if (foco == SP_FOCO_ABAS) { if (nVisiveis() > 0) foco = 0; return; }
    if (foco + 1 < nVisiveis()) foco++;
    return;
  }
  if (k == SDLK_UP) {
    // DE CIMA DA LISTA SOBE PARA AS ABAS, e nao para lugar nenhum. Sem isto a
    // unica forma de trocar de aba seria fechar e reabrir o painel.
    if (foco == 0 && temAbas()) { foco = SP_FOCO_ABAS; return; }
    if (foco > 0) foco--;
    return;
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    if (foco == SP_FOCO_ABAS) {
      // OK na linha de abas alterna, para quem nao descobriu a seta.
      trocarAba(aba == SP_ABA_SALVOS ? SP_ABA_SOCIAL : SP_ABA_SALVOS);
      return;
    }
    if (aba == SP_ABA_SOCIAL) {
      if (foco == nRecs) {
        // A ULTIMA LINHA abre a tela de amigos. O painel FICA ABERTO atras: a
        // modal e uma camada por cima dele e Voltar devolve o foco aqui, em vez
        // de jogar a pessoa de volta na home.
        recenviar_abrir_amigos();
        return;
      }
      if (foco >= 0 && foco < nRecs) {
        // A ACAO QUE IMPORTA E ABRIR O TITULO, e o contrato para isso ja
        // existe: o painel entrega o IMDb e app.c resolve. Ele nao conhece
        // detail.c nem a descoberta, exatamente como antes.
        snprintf(pedido, sizeof pedido, "%s", recs[foco].imdb);
        temPedido = 1;
        aberto = 0;
      }
      return;
    }
    if (foco >= 0 && foco < nLinhas) {
      snprintf(pedido, sizeof pedido, "%s", linhas[foco].id);
      temPedido = 1;
      aberto = 0;
    }
    return;
  }
}

void spainel_atualizar(float dt, Uint32 agora) {
  int i;
  float alvo, topo, base;
  (void)agora;
  if (!aberto && entrada < 0.002f) {
    if (entrada != 0.0f) entrada = 0.0f;
    return;
  }
  // O catalogo pode ter sido republicado com o painel aberto (a descoberta faz
  // isso varias vezes por ciclo). Sem esta reconstrucao a lista continuaria a
  // do instante da abertura, com ponteiros de titulo apontando para CatItem que
  // ja mudou de conteudo — texto de outro filme no card certo.
  // CONTAGEM IGUAL NAO PROVA CATALOGO IGUAL — a descoberta republica o mesmo
  // numero de titulos com outro conteudo. Reconstruir por contagem deixava o
  // painel com o texto do catalogo anterior; agora que os textos sao COPIADOS
  // isso nao e mais leitura de memoria liberada, mas continua sendo o nome
  // errado no card certo. A marca extra e a mesma de contalib_reconciliar: o
  // primeiro item do catalogo raramente sobrevive identico a uma troca de bloco.
  if (aberto && (cat_n() != marcaCatN || catTrocou())) reconstruir();
  // A LISTA SOCIAL TAMBEM MUDA COM O PAINEL ABERTO: o fio de recomenda.c sonda
  // a cada 60 s, e uma recomendacao que chega enquanto a aba esta na tela tem
  // de aparecer. A copia e barata (memcpy de ate 60 registros) e so acontece
  // com a aba Social visivel.
  if (aberto && aba == SP_ABA_SOCIAL && nRecs != recomenda_n()) reconstruirSocial();

  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt,
                       aberto ? SP_ABRIR_MS : SP_FECHAR_MS);
  for (i = 0; i < nVisiveis() && i < SP_MAX; i++) {
    float a = (aberto && i == foco) ? 1.0f : 0.0f;
    animFoco[i] = ajustes_animacoes_reduzidas()
      ? a
      : anim_mola(animFoco[i], a, dt,
                  a > animFoco[i] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  }
  // Rola o MINIMO para a linha focada caber inteira, como a grade da
  // Biblioteca. Alinhar a focada ao topo joga o cabecalho para fora na primeira
  // descida e a pessoa perde de vista em que painel esta.
  alvo = scrollY;
  if (foco == SP_FOCO_ABAS) alvo = 0.0f;
  else if (nVisiveis() > 0 && foco >= 0 && foco < nVisiveis()) {
    float janela = SP_LISTA_BASE - listaTopo();
    topo = topoDe(foco);
    base = topo + SP_POSTER_H;
    if (base - alvo > janela) alvo = base - janela;
    if (topo - alvo < 0.0f) alvo = topo;
  }
  if (alvo < 0.0f) alvo = 0.0f;
  scrollY = ajustes_animacoes_reduzidas()
    ? alvo : anim_mola(scrollY, alvo, dt, NV_MOLA_SCROLL);
}

// "Salvo há 2 horas". A FRASE INTEIRA passa por i18n como FORMATO, nao montada
// de pedacos: "há" e "atrás" trocam de lugar na traducao e uma frase remendada
// aqui sairia "2 horas ago" em ingles.
static void quandoTexto(char *dst, size_t tam, long long quandoS) {
  long long agora = (long long)time(NULL);
  long long d = agora - quandoS;
  if (quandoS <= 0) { dst[0] = 0; return; }
  if (d < 0) d = 0;
  if (d < 90)            snprintf(dst, tam, "%s", i18n("Salvo agora"));
  else if (d < 5400)     snprintf(dst, tam, i18n("Salvo há %d min"), (int)(d / 60));
  else if (d < 172800)   snprintf(dst, tam, i18n("Salvo há %d h"),   (int)(d / 3600));
  else                   snprintf(dst, tam, i18n("Salvo há %d dias"),(int)(d / 86400));
}

// "Série · 2004 · ★ 8,1". As PARTES passam por i18n e a juncao nao: a chave da
// tabela e o portugues inteiro de uma string, e a frase montada nunca existiria
// como chave. E a mesma correcao que a biblioteca ja levou (issue #3).
static void metaTexto(char *dst, size_t tam, const SPLinha *l) {
  const char *tipo = i18n(l->serie ? "Série" : "Filme");
  if (l->meta[0] && l->nota > 0)
    snprintf(dst, tam, "%s · %s · \xe2\x98\x85 %d,%d", tipo, l->meta,
             l->nota / 10, l->nota % 10);
  else if (l->meta[0])
    snprintf(dst, tam, "%s · %s", tipo, l->meta);
  else if (l->nota > 0)
    snprintf(dst, tam, "%s · \xe2\x98\x85 %d,%d", tipo, l->nota / 10, l->nota % 10);
  else
    snprintf(dst, tam, "%s", tipo);
}

// `dx` e o deslocamento da animacao de entrada. Ele PRECISA chegar ate aqui: as
// linhas sao desenhadas em coordenada absoluta, e sem somar o mesmo `dx` do
// painel elas ficariam paradas no lugar final enquanto a moldura ainda desliza
// — o conteudo apareceria antes da caixa que o contem.
static void desenhaLinha(int i, float dx, float y, float a) {
  const SPLinha *l = &linhas[i];
  float f = animFoco[i];
  float px = SP_X + dx + SP_PAD, tx = SP_X + dx + SP_TEXTO_X;
  char buf[192];
  GfxRect poster = { px, y, SP_POSTER_W, SP_POSTER_H };

  if (f > 0.01f) {
    // PILULA CLARA COM TEXTO ESCURO, e nao anel. Esta nota dizia o contrario
    // — "anel por fora, e nao pilula clara" — e o dono decidiu o oposto em
    // 16/09, olhando a TV: "os botoes quando selecionados ficar brancos com o
    // texto preto ... na sidebar quando selecionado ficar assim tambem, e pode
    // tirar o contorno". A regra passou a valer para o app inteiro (menu.c,
    // folha de fontes, e esta camada), entao o comentario antigo fica aqui so
    // como registro de que a troca foi deliberada.
    GfxRect r = { px - 12.0f, y - 10.0f, SP_INTERNO + 24.0f, SP_POSTER_H + 20.0f };
    gfx_cor(r, 0.06f, 0.961f, 0.961f, 0.968f, f * a);
  }

  { GLuint tex = l->poster[0] ? tex_obter(l->poster) : 0;
    if (tex) {
      gfx_tex_aspect_atual = tex_aspecto(l->poster);
      gfx_rect(poster, tex, GFX_CARD, 0.0f, 0.0f, 0.0f, 0.08f, 0, 0, 0, a);
      gfx_tex_aspect_atual = 0.0f;
    } else {
      // Esqueleto VISIVEL (#2C2C2C), o mesmo da home e da biblioteca: um
      // retangulo da cor do fundo le como card quebrado, nao como carregando.
      gfx_cor(poster, 0.08f, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G,
              NV_COR_ESQUELETO_B, a);
    } }

  // COM A PILULA CLARA, O TEXTO INVERTE. Claro sobre claro nao se le, e a
  // troca acontece em DEGRAU (f > 0.5) e nao interpolada: a cor faz parte da
  // chave do cache de linhas de text.c, e uma cor por quadro rasteriza a
  // linha a cada quadro — a nota longa disso esta em ctxmenu.c.
  { int esc = f > 0.5f;
    int c1 = esc ? 20 : 245, c2 = esc ? 74 : 168;
    { TxtLinha t = txt_linha_corta(TXT_CALLOUT, l->titulo,
                                   c1, c1 + 1, c1 + 5, 255, SP_TEXTO_W);
      txt_desenhar_alpha(t, tx, y + 4.0f, a); }
    metaTexto(buf, sizeof buf, l);
    { TxtLinha t = txt_linha_corta(TXT_CAPTION2, buf, c2, c2 + 4, c2 + 14, 255,
                                   SP_TEXTO_W);
      txt_desenhar_alpha(t, tx, y + 42.0f, a * 0.95f); } }

  if (l->progresso > 0) {
    float p = anim_clamp(l->progresso / 100.0f, 0.0f, 1.0f);
    GfxRect trilho = { tx, y + 84.0f, SP_BARRA_W, SP_BARRA_H };
    GfxRect cheio  = { tx, y + 84.0f, SP_BARRA_W * p, SP_BARRA_H };
    gfx_cor(trilho, 0.5f, 0.24f, 0.25f, 0.28f, a);
    if (cheio.w > 1.0f) gfx_cor(cheio, 0.5f, 0.93f, 0.94f, 0.97f, a);
    // "T1E3 · 29 min restantes" para serie; so o tempo para filme. Formatos
    // inteiros em i18n: a ordem de "T"/"E" e de "min restantes" nao sobrevive a
    // uma montagem por pedacos.
    if (l->temporada > 0 && l->episodio > 0 && l->restanteMin > 0)
      snprintf(buf, sizeof buf, i18n("T%dE%d · %d min restantes"),
               l->temporada, l->episodio, l->restanteMin);
    else if (l->temporada > 0 && l->episodio > 0)
      snprintf(buf, sizeof buf, i18n("T%dE%d · retomar"), l->temporada, l->episodio);
    else if (l->restanteMin > 0)
      snprintf(buf, sizeof buf, i18n("%d min restantes"), l->restanteMin);
    else
      snprintf(buf, sizeof buf, "%s", i18n("Retomar"));
    { int c = f > 0.5f ? 56 : 198;
      TxtLinha t = txt_linha_corta(TXT_CAPTION, buf, c, c + 4, c + 14, 255, SP_TEXTO_W);
      txt_desenhar_alpha(t, tx, y + 102.0f, a * 0.95f); }
  } else {
    quandoTexto(buf, sizeof buf, l->quandoS);
    if (buf[0]) {
      int c = f > 0.5f ? 84 : 150;
      TxtLinha t = txt_linha_corta(TXT_CAPTION, buf, c, c + 4, c + 15, 255, SP_TEXTO_W);
      txt_desenhar_alpha(t, tx, y + 92.0f, a * 0.9f);
    }
  }
}

// Uma linha da aba Social: cartaz, titulo, quem mandou e quando, e a frase.
// O cartaz e a MESMA tex_cache do resto do app; a recomendacao guarda a URL do
// poster no proprio arquivo (recomenda.h), entao a linha se desenha sem
// depender do catalogo.
static void desenhaRecLinha(int i, float dx, float y, float a) {
  const RecItem *r = &recs[i];
  float f = animFoco[i];
  float px = SP_X + dx + SP_PAD, tx = SP_X + dx + SP_TEXTO_X;
  char buf[320], quando[64];
  GfxRect poster = { px, y, SP_POSTER_W, SP_POSTER_H };

  if (f > 0.01f) {
    // Mesma pilula clara da aba Salvos — as duas listas sao a mesma camada e
    // marcar o foco de dois jeitos dentro dela seria pior que qualquer um dos
    // dois.
    GfxRect anel = { px - 12.0f, y - 10.0f, SP_INTERNO + 24.0f, SP_POSTER_H + 20.0f };
    gfx_cor(anel, 0.06f, 0.961f, 0.961f, 0.968f, f * a);
  }

  { GLuint tex = r->poster[0] ? tex_obter(r->poster) : 0;
    if (tex) {
      gfx_tex_aspect_atual = tex_aspecto(r->poster);
      gfx_rect(poster, tex, GFX_CARD, 0.0f, 0.0f, 0.0f, 0.08f, 0, 0, 0, a);
      gfx_tex_aspect_atual = 0.0f;
    } else {
      gfx_cor(poster, 0.08f, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G,
              NV_COR_ESQUELETO_B, a);
    } }

  { int esc = f > 0.5f;
    int c1 = esc ? 20 : 245;
    TxtLinha t = txt_linha_corta(TXT_CALLOUT, r->titulo, c1, c1 + 1, c1 + 5, 255,
                                 SP_TEXTO_W);
    txt_desenhar_alpha(t, tx, y + 2.0f, a); }

  // "Gustavo · há 2 h" — as PARTES passam por i18n e a juncao nao, pela mesma
  // razao de metaTexto: a chave da tabela e uma string inteira, e a frase
  // montada nunca existiria como chave.
  rec_quando_texto(quando, sizeof quando, r->criado);
  if (quando[0]) snprintf(buf, sizeof buf, "%s · %s", r->deNome, quando);
  else           snprintf(buf, sizeof buf, "%s", r->deNome);
  // As DUAS linhas de baixo invertem junto com o titulo: com a pilula clara,
  // cinza-claro sobre claro fica ilegivel — foi o que a captura mostrou antes
  // de isto existir.
  { int esc = f > 0.5f;
    int c2 = esc ? 74 : 168, c3 = esc ? 48 : 214;
    { TxtLinha t = txt_linha_corta(TXT_CAPTION2, buf, c2, c2 + 4, c2 + 14, 255,
                                   SP_TEXTO_W);
      txt_desenhar_alpha(t, tx, y + 44.0f, a * 0.95f); }
    { const char *frase = rec_frase(r);
      if (frase[0]) {
        snprintf(buf, sizeof buf, "\xe2\x80\x9c%s\xe2\x80\x9d", frase);
        { TxtLinha t = txt_linha_corta(TXT_CAPTION, buf, c3, c3 + 4, c3 + 14, 255,
                                       SP_TEXTO_W);
          txt_desenhar_alpha(t, tx, y + 88.0f, a * 0.95f); }
      } } }

  if (!r->visto) {
    // Ponto de "ainda nao lida". Some quando a aba e aberta, junto do selo.
    // Fica CENTRADO na linha do titulo e com folga dos dois lados do vao: a
    // versao anterior nascia no topo do titulo e a 8px dele, e a foto ampliada
    // mostrou os dois grudados.
    GfxRect ponto = { tx - 22.0f, y + 15.0f, 10.0f, 10.0f };
    gfx_cor(ponto, 0.5f, 0.42f, 0.72f, 0.98f, a);
  }
}

// A LINHA DE ABAS. Sem animacao de cor de proposito: a cor faz parte da chave
// do cache de linhas de text.c, e uma cor por quadro cria uma rasterizacao TTF
// e uma textura GL por quadro — estourado o orcamento, a linha simplesmente
// NAO E DESENHADA (ver a nota longa em ctxmenu.c). O foco aparece no anel, que
// e geometria e nao custa texto.
static void desenhaAbas(float dx, float a) {
  const char *rot[2];
  float x = SP_X + dx + SP_PAD;
  int i, novas = recomenda_n_novas();
  rot[SP_ABA_SALVOS] = "SALVOS";
  rot[SP_ABA_SOCIAL] = "SOCIAL";
  for (i = 0; i < 2; i++) {
    int ativa = (i == aba);
    int emFoco = (foco == SP_FOCO_ABAS && ativa);
    int cor = emFoco ? 20 : (ativa ? 246 : 176);
    TxtLinha t = txt_linha(TXT_CALLOUT, i18n(rot[i]), cor, cor, cor, 255);
    // O selo so aparece na aba que NAO esta aberta. Ele responde "ha algo
    // novo la?"; com a aba Social na tela, a propria lista responde isso, e o
    // numero ficaria repetido a dois centimetros da contagem do cabecalho.
    float selo = (i == SP_ABA_SOCIAL && !ativa && novas > 0) ? 44.0f : 0.0f;
    GfxRect p = { x, SP_ABAS_Y, t.w + 44.0f + selo, SP_ABAS_H };
    // FOCO EM SUPERFICIE ESCURA, nunca pilula branca com texto preto: a nota
    // de NV_COR_FOCO em layout.h chama isso de o padrao errado, e perfilsel.c
    // ja tinha sido corrigido pelo mesmo motivo. A primeira versao desta linha
    // repetiu o erro — pilula 0.94 com texto 17 — e era o que mais pesava na
    // foto ampliada.
    // A ativa tem de ser a MAIS clara das duas. O fundo do painel ja e 0.075,
    // entao um branco a 0.07 por cima dele chega perto de 0.14 — colado nos
    // 0.188 de NV_COR_FOCO, e na captura a aba fechada parecia a aberta.
    // (A nota acima sobre "nunca pilula branca com texto preto" ficou velha:
    // a regra mudou em 16/09/2026, ver NV_COR_FOCO em layout.h.)
    //
    // Com o D-pad NA LINHA DE ABAS a aba aberta e preenchida na cor de
    // realce com texto escuro, sem anel; fora dela, a aberta e a superficie
    // clara e a outra fica apagada.
    if (emFoco) { float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
                  gfx_cor(p, NV_RAIO_PILL, ar, ag, ab, a); }
    else if (ativa) gfx_cor(p, NV_RAIO_PILL, 0.26f, 0.26f, 0.27f, a);
    else            gfx_cor(p, NV_RAIO_PILL, 1.0f, 1.0f, 1.0f, 0.04f * a);
    txt_desenhar_alpha(t, x + 22.0f, SP_ABAS_Y + (SP_ABAS_H - t.h) * 0.5f, a);
    if (selo > 0.0f) {
      char n[16];
      GfxRect b;
      TxtLinha tn;
      snprintf(n, sizeof n, "%d", novas > 99 ? 99 : novas);
      // NUMERO ESCURO SOBRE O VERDE, e nao branco. Branco 250 sobre #66bb6a
      // da 2,27:1 de contraste — abaixo dos 3:1 que a propria AA pede ate
      // para texto GRANDE, e este numeral tem 21px a tres metros. Escuro da
      // 7,6:1 e e o que faz o selo ler como um selo, e nao como uma mancha.
      tn = txt_linha(TXT_CAPTION2, n, 12, 26, 16, 255);
      b.w = 32.0f; b.h = 32.0f;
      b.x = x + 22.0f + t.w + 12.0f;
      b.y = SP_ABAS_Y + (SP_ABAS_H - b.h) * 0.5f;
      // VERDE #66bb6a — o mesmo EMERALD que ja esta na paleta de acentos
      // (ajustes.c:204), e nao um verde novo inventado para este selo.
      gfx_cor(b, 0.5f, 0.400f, 0.733f, 0.416f, a);
      txt_desenhar_alpha(tn, b.x + (b.w - tn.w) * 0.5f,
                         b.y + (b.h - tn.h) * 0.5f, a);
    }
    x += p.w + SP_ABA_GAP;
  }
}

// O ESTADO VAZIO DIZ POR QUE ESTA VAZIO, e nao so que esta.
//
// A versao anterior dizia "quando um amigo mandar um filme, ele aparece aqui",
// que e verdade e nao ajuda em nada: o dono tinha ZERO contatos e a tela nao
// dava nenhuma pista de que faltava um passo — o vinculo do Trakt so alcanca
// quem JA usa o servico, e em 15/09/2026 isso eram zero pessoas. Aqui a tela
// diz a razao, mostra o codigo que ele precisa ditar e oferece a porta.
// Devolve o y logo abaixo do texto, para a linha-botao nascer colada nele em
// vez de boiar no fim do painel.
static float desenhaSocialVazio(float dx, float a) {
  float x = SP_X + dx + SP_PAD;
  float y = listaTopo() + 24.0f;
  const char *cod = recomenda_meu_codigo();
  { TxtLinha t = txt_linha(TXT_CALLOUT, "Nenhuma recomendação ainda",
                           240, 242, 248, 255);
    txt_desenhar_alpha(t, x, y, a * 0.96f); y += t.h + 14.0f; }
  // EM BLOCO: a coluna do painel tem 688px e a frase tem duas oracoes; numa
  // linha so, a captura saiu terminando em "Amigos do Trakt entram..." — a
  // metade que explica o motivo ficava de fora.
  y += txt_bloco(TXT_CAPTION,
      "Ninguém da sua lista está aqui ainda. Amigos do Trakt entram sozinhos só depois de instalarem o app.",
      190, 194, 204, x, y, SP_INTERNO, 30.0f, a * 0.9f, 3) + 22.0f;
  if (cod[0]) {
    // O CODIGO TAMBEM AQUI, e nao so na tela de amigos: este e o painel que o
    // dono abre com uma tecla, e ditar seis caracteres ao telefone e a unica
    // acao que resolve uma lista vazia hoje.
    TxtLinha r = txt_linha(TXT_CAPTION2, "Seu código", 160, 164, 175, 255);
    TxtLinha c = txt_linha(TXT_TITULO2, cod, 246, 248, 255, 255);
    txt_desenhar_alpha(r, x, y, a * 0.88f);
    y += r.h + 6.0f;
    txt_desenhar_alpha(c, x, y, a);
    y += c.h + 20.0f;
  }
  { TxtLinha t = txt_linha_corta(TXT_CAPTION,
        "Peça o código do seu amigo e adicione-o abaixo.",
        168, 172, 182, 255, SP_INTERNO);
    txt_desenhar_alpha(t, x, y, a * 0.85f);
    y += t.h + 28.0f; }
  return y;
}

// A linha-botao do estado vazio. Mesma pilula e mesmo foco invertido das
// outras listas do app.
static void desenhaSocialAcao(int i, float dx, float y, float a) {
  GfxRect r = { SP_X + dx + SP_PAD, y, SP_INTERNO, 76.0f };
  float f = i >= 0 && i < SP_MAX ? animFoco[i] : 0.0f;
  float lum = anim_mistura(0.176f, 0.961f, f);
  int cor = f >= 0.5f ? 17 : 240;
  gfx_cor(r, 14.0f / r.h, lum, lum, lum, a);
  { TxtLinha t = txt_linha(TXT_PLR_CORPO, "Adicionar um amigo",
                           cor, cor, cor, 255);
    txt_desenhar_alpha(t, r.x + 32.0f, y + (r.h - t.h) * 0.5f, a); }
}

static void desenhaVazio(float dx, float a) {
  float cx = SP_X + dx + SP_W * 0.5f;
  TxtLinha t1 = txt_linha(TXT_CALLOUT, "Nada salvo por enquanto", 240, 242, 248, 255);
  TxtLinha t2 = txt_linha_corta(TXT_CAPTION,
      "Aperte + em um filme ou série e ele aparece aqui.",
      168, 172, 182, 255, SP_INTERNO);
  gfx_icone((GfxRect){ cx - 30.0f, listaTopo() + 140.0f, 60.0f, 60.0f },
            "mais", 0.55f, 0.57f, 0.62f, a);
  txt_desenhar_alpha(t1, cx - t1.w * 0.5f, listaTopo() + 232.0f, a * 0.96f);
  txt_desenhar_alpha(t2, cx - t2.w * 0.5f, listaTopo() + 278.0f, a * 0.85f);
}

void spainel_desenhar(Uint32 agora) {
  float a = anim_suave(entrada), x, y;
  int i;
  char buf[160];
  (void)agora;
  if (entrada < 0.002f) return;

  // O veu usa a rampa CRUA e o painel a suavizada, pelo mesmo motivo do menu
  // lateral: a medida da referencia para o escurecimento e uma reta, e um bloco
  // deste tamanho parando de vez no fim do percurso le como corte.
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, SP_VEU * entrada);

  // Entra deslizando da BORDA DIREITA. `x` e o deslocamento: em a=0 o painel
  // esta inteiro fora da tela.
  x = (1.0f - a) * (NV_TELA_W - SP_X);
  { GfxRect p = { SP_X + x, SP_Y, SP_W, SP_H };
    gfx_cor(p, 0.035f, 0.075f, 0.078f, 0.088f, 0.98f * a); }

  // Tudo daqui para baixo fica preso ao painel: sem o recorte, a lista rolada
  // desenha por cima do cabecalho e por baixo da borda inferior.
  gfx_recorte(SP_X + x, SP_Y, SP_W, SP_H);

  // Cabecalho: a linha de resumo em cima e o nome grande embaixo, como na
  // referencia. As PARTES passam por i18n; a juncao, nao (ver metaTexto).
  if (aba == SP_ABA_SOCIAL)
    snprintf(buf, sizeof buf, "%d %s", nRecs,
             i18n(nRecs == 1 ? "recomendação" : "recomendações"));
  else
    snprintf(buf, sizeof buf, "%d %s   ·   %d %s", nLinhas,
             i18n(nLinhas == 1 ? "título" : "títulos"),
             nCont, i18n("para retomar"));
  // COM ABAS a contagem vai para a DIREITA da propria linha de abas, e nao
  // numa linha solta acima delas: sozinha la em cima ela lia como um titulo
  // orfao, que foi a primeira coisa que saltou na foto ampliada.
  { TxtLinha t = txt_linha(TXT_CAPTION2, buf, 160, 164, 175, 255);
    if (temAbas())
      txt_desenhar_alpha(t, SP_X + x + SP_W - SP_PAD - t.w,
                         SP_ABAS_Y + (SP_ABAS_H - t.h) * 0.5f, a * 0.95f);
    else
      txt_desenhar_alpha(t, SP_X + x + SP_PAD, SP_Y + 38.0f, a * 0.95f); }
  if (temAbas()) desenhaAbas(x, a);
  else {
    TxtLinha t = txt_linha(TXT_TITULO2, "Salvos", 246, 247, 252, 255);
    txt_desenhar_alpha(t, SP_X + x + SP_PAD, SP_Y + 74.0f, a);
  }

  if (aba == SP_ABA_SOCIAL) {
    if (nRecs == 0) {
      desenhaSocialAcao(0, x, desenhaSocialVazio(x, a), a);
      gfx_sem_recorte();
      return;
    }
    gfx_recorte(SP_X + x, listaTopo(), SP_W, SP_LISTA_BASE - listaTopo());
    y = listaTopo() - scrollY;
    for (i = 0; i < nRecs; i++) {
      // Fora da janela nao custa texto nem textura — mesma razao da lista de
      // Salvos logo abaixo.
      if (y + SP_POSTER_H >= listaTopo() && y <= SP_LISTA_BASE)
        desenhaRecLinha(i, x, y, a);
      y += SP_PASSO;
    }
    // A linha de "Adicionar um amigo" fecha a lista, e nao um botao solto no
    // rodape: ela rola com o resto e recebe foco como qualquer outra.
    if (y >= listaTopo() - 76.0f && y <= SP_LISTA_BASE)
      desenhaSocialAcao(nRecs, x, y, a);
    gfx_sem_recorte();
    return;
  }

  if (nLinhas == 0) { desenhaVazio(x, a); gfx_sem_recorte(); return; }

  // A lista rola dentro da propria janela, com um segundo recorte: o cabecalho
  // fica de fora dele e por isso nunca e coberto por um card subindo.
  gfx_recorte(SP_X + x, listaTopo(), SP_W, SP_LISTA_BASE - listaTopo());
  y = listaTopo() - scrollY;

  { TxtLinha t = txt_linha(TXT_CAPTION2,
        i18n(nCont > 0 ? "Continuar" : "Sua lista"), 150, 154, 165, 255);
    txt_desenhar_alpha(t, SP_X + x + SP_PAD, y + SP_SECAO_H - t.h - 12.0f, a * 0.9f); }
  y += SP_SECAO_H;

  for (i = 0; i < nLinhas; i++) {
    if (i == nCont && nCont > 0) {
      TxtLinha t = txt_linha(TXT_CAPTION2, "Não começados", 150, 154, 165, 255);
      txt_desenhar_alpha(t, SP_X + x + SP_PAD, y + SP_SECAO_H - t.h - 12.0f, a * 0.9f);
      y += SP_SECAO_H;
    }
    // Fora da janela nao custa texto nem textura: numa lista de 200 titulos
    // rasterizar as 195 invisiveis estouraria o orcamento de linhas por quadro
    // de text.c e as visiveis sairiam EM BRANCO (ver a nota em ctxmenu.c).
    if (y + SP_POSTER_H >= listaTopo() && y <= SP_LISTA_BASE)
      desenhaLinha(i, x, y, a);
    y += SP_PASSO;
  }

  gfx_sem_recorte();
}
