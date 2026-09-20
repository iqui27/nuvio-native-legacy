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
#include "avisos.h"
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
// A ABA AVISOS e a central de avisos (avisos.h) dentro deste painel, para
// abrir quando se quiser e nao so no toast. Existe SEMPRE; a Social so com o
// servico de recomendacoes.
enum { SP_ABA_SALVOS = 0, SP_ABA_SOCIAL = 1, SP_ABA_AVISOS = 2 };
#define SP_FOCO_ABAS (-1)
static int aba;
static RecItem recs[REC_MAX];
static int nRecs;

// A ABA SOCIAL DEIXOU DE SER UMA LISTA SO. Ela tem agora quatro tipos de linha
// com ALTURAS DIFERENTES, e por isso existe este vetor em vez de um indice
// direto na lista de recomendacoes: a rolagem, o foco e o desenho tem de
// concordar sobre onde comeca a linha `i`, e a unica forma de garantir isso e
// as tres perguntarem ao MESMO lugar.
//
//   SPS_CONSENT_NAO / _SIM  as duas respostas da pergunta de primeira entrada
//   SPS_REC                 uma recomendacao recebida
//   SPS_SUG                 alguem que a pessoa talvez conheca
//   SPS_ADICIONAR           "Adicionar um amigo"
//   SPS_APARECER            o interruptor de "apareco para os outros?"
//
// COM A PERGUNTA NA TELA A LISTA TEM SO DUAS LINHAS, as duas respostas. Nao e
// uma tela separada com laco proprio: o D-pad, a rolagem, a animacao de foco e
// o recorte do painel ja funcionam para linhas, e uma segunda maquina de estado
// para duas pilulas divergiria da primeira na primeira correcao.
enum { SPS_CONSENT_NAO = 0, SPS_CONSENT_SIM, SPS_REC, SPS_SUG,
       SPS_ADICIONAR, SPS_APARECER };
typedef struct { unsigned char tipo; short idx; } SPSocial;
#define SP_SOCIAL_MAX (REC_MAX + REC_SUGESTOES_MAX + 4)
static SPSocial social[SP_SOCIAL_MAX];
static int nSocial;
static RecSugestao sugs[REC_SUGESTOES_MAX];
static int nSugs;
// Retrato do estado do consentimento na ultima reconstrucao. O fio de rede pode
// adotar um "sim" respondido em OUTRA TV no meio de um ciclo (ver a
// reconciliacao em recomenda.c), e sem esta marca a pergunta continuaria na
// tela depois de ja ter sido respondida.
static int consentEstado = -1;

// Alturas das linhas novas. A recomendacao mantem SP_POSTER_H + SPS_GAP, que e
// exatamente o SP_PASSO de antes — a aba nao mudou de ritmo, so ganhou vizinhos.
#define SPS_GAP         22.0f
#define SPS_H_CONSENT   84.0f
#define SPS_H_SUG      112.0f
#define SPS_H_ACAO      76.0f
#define SPS_H_APARECER 104.0f
// Alturas dos dois blocos de texto que NAO sao linha e por isso nao recebem
// foco: o enunciado da pergunta e a explicacao do estado vazio. Sao constantes
// e nao medidas porque a rolagem precisa delas ANTES do desenho — e as duas
// foram conferidas na captura, que e o unico juiz util aqui.
#define SPS_CONSENT_TOPO 400.0f
#define SPS_VAZIO_TOPO   320.0f

// O INTERRUPTOR DE "APARECER". As medidas sao para TRES METROS, e nao copiadas
// de um telefone.
//
// A conta: numa TV de 55" o painel de 1920 px cobre ~1218 mm, ou seja 0,63 mm
// por pixel; a 3 m um minuto de arco mede 0,87 mm. Da 0,73' por pixel. Com
// isso a trilha de 96 px mede ~70' (1,2 grau) e o PERCURSO da bola, 48 px,
// mede ~35'. O interruptor do iOS (51x31 pt) daria 37' de trilha e 13' de
// percurso: perfeito na mao, e a 3 m o deslocamento vira um tremor.
//
// A bola e 36 px com 6 px de folga de cada lado — os 6 px vem do percurso, e
// nao de gosto: a folga sai duas vezes da largura (12 px dos 96), entao cada
// pixel a mais de folga custa um de deslocamento, que e o canal que carrega o
// estado. Menos que isso nao foi testado.
#define SPS_SW_W        96.0f
#define SPS_SW_H        48.0f
#define SPS_SW_PAD       6.0f
#define SPS_SW_BOLA    (SPS_SW_H - SPS_SW_PAD * 2.0f)
#define SPS_SW_GAP      28.0f   // do fim do texto ate a trilha
// ESPESSURA DO ANEL da trilha vazia. 4 px sao 2,9' de arco a 3 m, acima do
// minuto de arco que e o limite de resolucao; 1 px (0,73') sumiria.
#define SPS_SW_ANEL      4.0f
// VAO EXTRA ANTES DO INTERRUPTOR. Os 22 px de SPS_GAP separam linhas do MESMO
// tipo; aqui a lista de gente e de acoes acaba e comeca um ajuste que fica.
// Sao 20 px, e nao um cabecalho de secao: um rotulo ali repetiria o titulo da
// propria linha, que e exatamente o ar de formulario que se quer evitar.
#define SPS_SEP_APARECER 20.0f

static int aberto, foco, marcaCatN = -1;
static float entrada, scrollY;
static float animFoco[SP_MAX];
// POSICAO DA BOLA DO INTERRUPTOR, 0 = desligado, 1 = ligado. E estado PROPRIO
// e nao uma leitura direta de recomenda_aparecer() por um motivo que e a razao
// de ser desta linha inteira: o deslize e a unica coisa na tela que responde
// "o OK MUDOU alguma coisa" em vez de "o OK ABRIU alguma coisa". Sem ele o
// desenho pularia entre dois retratos e voltaria a ser um botao.
// -1 = ainda nao lido; a primeira atualizacao assenta sem deslizar do nada.
static float animSw = -1.0f;
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
static int temAbas(void) { return 1; }
// A Social so existe com o servico; sem ele as abas sao Salvos e Avisos.
static int temSocial(void) { return recomenda_ativo(); }
static int proximaAba(int de, int dir) {
  int a = de + dir;
  if (a == SP_ABA_SOCIAL && !temSocial()) a += dir;
  if (a < SP_ABA_SALVOS) return de;
  if (a > SP_ABA_AVISOS) return de;
  return a;
}

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
  if (aba == SP_ABA_SOCIAL) return nSocial;
  if (aba == SP_ABA_AVISOS) return avisos_lista_n();
  return nLinhas;
}

static float listaTopo(void) { return SP_LISTA_Y; }

// 1 enquanto a pergunta de primeira entrada esta na tela.
static int consentindo(void) {
  return aba == SP_ABA_SOCIAL && nSocial > 0 && social[0].tipo == SPS_CONSENT_NAO;
}

static float socialAlt(int i) {
  if (i < 0 || i >= nSocial) return 0.0f;
  switch (social[i].tipo) {
    case SPS_REC:       return SP_POSTER_H;
    case SPS_SUG:       return SPS_H_SUG;
    case SPS_ADICIONAR: return SPS_H_ACAO;
    case SPS_APARECER:  return SPS_H_APARECER;
    default:            return SPS_H_CONSENT;
  }
}

// Espaco ANTES da linha `i`, quando houver. Sao dois casos, e so um deles
// carrega texto:
//   SPS_SUG  o cabecalho que separa as recomendacoes das sugestoes. Sem ele,
//            um nome desconhecido apareceria logo abaixo de uma recomendacao
//            de um amigo e leria como remetente.
//   SPS_APARECER  vao mudo. Ver SPS_SEP_APARECER.
// Quem desenha tem de olhar o tipo para saber se escreve o rotulo — um vao
// mudo com o cabecalho das sugestoes por cima seria pior que vao nenhum.
static float socialAntes(int i) {
  if (i < 0 || i >= nSocial) return 0.0f;
  if (social[i].tipo == SPS_APARECER) return SPS_SEP_APARECER;
  if (social[i].tipo != SPS_SUG) return 0.0f;
  return (i == 0 || social[i - 1].tipo != SPS_SUG) ? SP_SECAO_H : 0.0f;
}

// Altura do bloco de texto que abre a lista e nao recebe foco.
static float socialTopo(void) {
  if (aba != SP_ABA_SOCIAL) return 0.0f;
  if (consentindo()) return SPS_CONSENT_TOPO;
  return nRecs == 0 ? SPS_VAZIO_TOPO : 0.0f;
}

// Copia a lista de recomendacoes para dentro do painel. COPIA, e nao ponteiro:
// a lista de recomenda.c vive atras de um mutex que o fio de rede reescreve, e
// e exatamente o erro que derrubou este arquivo antes (ver a nota longa em
// SPLinha).
static void reconstruirSocial(void) {
  int i;
  nRecs = 0;
  nSugs = 0;
  nSocial = 0;
  consentEstado = -1;
  if (!temAbas()) return;
  consentEstado = recomenda_aparecer();

  // A PERGUNTA VEM ANTES DE TUDO, e ela e a lista inteira enquanto durar. Nao e
  // um cartaz por cima de uma lista que da para ler por baixo: o pedido foi
  // "quando entrar a primeira vez, perguntar", e uma pergunta que se pode
  // ignorar rolando a tela nao foi feita.
  //
  // O "NAO" VEM PRIMEIRO. E a resposta padrao, e a primeira linha e a que
  // recebe o foco quando o D-pad desce — quem apertar OK duas vezes sem ler
  // acaba em "nao", que e o unico lado em que errar nao custa nada a ninguem.
  if (consentEstado == REC_APARECER_NAO_PERGUNTADO) {
    social[nSocial].tipo = SPS_CONSENT_NAO; social[nSocial].idx = 0; nSocial++;
    social[nSocial].tipo = SPS_CONSENT_SIM; social[nSocial].idx = 0; nSocial++;
    return;
  }

  for (i = 0; i < REC_MAX && nRecs < REC_MAX; i++)
    if (recomenda_item(i, &recs[nRecs])) nRecs++;
    else break;
  for (i = 0; i < REC_SUGESTOES_MAX && nSugs < REC_SUGESTOES_MAX; i++)
    if (recomenda_sugestao(i, &sugs[nSugs])) nSugs++;
    else break;

  for (i = 0; i < nRecs && nSocial < SP_SOCIAL_MAX; i++) {
    social[nSocial].tipo = SPS_REC; social[nSocial].idx = (short)i; nSocial++;
  }
  for (i = 0; i < nSugs && nSocial < SP_SOCIAL_MAX; i++) {
    social[nSocial].tipo = SPS_SUG; social[nSocial].idx = (short)i; nSocial++;
  }
  if (nSocial < SP_SOCIAL_MAX) {
    social[nSocial].tipo = SPS_ADICIONAR; social[nSocial].idx = 0; nSocial++;
  }
  // O INTERRUPTOR FECHA A ABA, e nao mora em Ajustes. Ele responde uma pergunta
  // que so faz sentido olhando para esta lista ("quem me ve?"), e quem quiser
  // mudar de ideia vai procura-lo onde a pergunta foi feita. A alternativa em
  // Ajustes esta descrita no relatorio; as duas podem coexistir.
  if (nSocial < SP_SOCIAL_MAX) {
    social[nSocial].tipo = SPS_APARECER; social[nSocial].idx = 0; nSocial++;
  }
}

static void trocarAba(int nova) {
  if (!temAbas() || nova == aba) return;
  if (aba == SP_ABA_AVISOS) avisos_marcar_lidos();
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
    // COM A PERGUNTA NA TELA NADA E MARCADO COMO LIDO. A lista esta atras dela:
    // apagar o selo agora diria "voce ja viu" sobre uma lista que ninguem viu,
    // e o aviso nao voltaria.
    if (!consentindo()) recomenda_marcar_vistas();
  }
}

void spainel_abrir(void) {
  if (aberto) return;
  aberto = 1;
  foco = 0;
  aba = SP_ABA_SALVOS;
  scrollY = 0.0f;
  memset(animFoco, 0, sizeof animFoco);
  // O INTERRUPTOR ABRE NO ESTADO, e nao deslizando ate ele. A resposta pode ter
  // sido reconciliada com o servidor (respondida em OUTRA TV) com o painel
  // fechado; sem isto a pessoa abriria o painel e veria a bola andar sozinha,
  // que le como "alguem acabou de mexer aqui".
  animSw = -1.0f;
  reconstruir();
  reconstruirSocial();
}

void spainel_fechar(void) {
  if (aberto && aba == SP_ABA_AVISOS) avisos_marcar_lidos();
  aberto = 0;
}

// Altura ate o TOPO da linha `i`, contando o cabecalho de cada secao. Nao e
// `i * SP_PASSO`: o rotulo "Não começados" empurra tudo que vem depois dele, e
// sem contar esse empurrao a rolagem para a linha focada erra por 54px — o
// suficiente para o card focado ficar meio escondido atras do cabecalho.
static float topoDe(int i) {
  float y;
  // A ABA SOCIAL SOMA LINHA A LINHA, e nao multiplica por um passo fixo: as
  // linhas dela tem quatro alturas diferentes. Era `i * SP_PASSO` enquanto
  // todas eram recomendacoes; com uma sugestao de 112px no meio, a multiplicacao
  // erraria a partir dali e a rolagem pararia o foco meio fora da janela.
  if (aba == SP_ABA_SOCIAL) {
    int k;
    y = socialTopo();
    for (k = 0; k < i && k < nSocial; k++)
      y += socialAntes(k) + socialAlt(k) + SPS_GAP;
    return y + socialAntes(i);
  }
  if (aba == SP_ABA_AVISOS) return avisos_lista_y(i, foco);
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
      trocarAba(proximaAba(aba, -1)); return;
    }
    spainel_fechar(); return;
  }
  if (k == SDLK_RIGHT) {
    if (temAbas() && foco == SP_FOCO_ABAS) trocarAba(proximaAba(aba, 1));
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
      { int p = proximaAba(aba, 1); trocarAba(p == aba ? SP_ABA_SALVOS : p); }
      return;
    }
    if (aba == SP_ABA_AVISOS) {
      if (avisos_lista_ok(foco)) spainel_fechar();
      return;
    }
    if (aba == SP_ABA_SOCIAL) {
      if (foco < 0 || foco >= nSocial) return;
      switch (social[foco].tipo) {
        case SPS_CONSENT_NAO:
        case SPS_CONSENT_SIM:
          recomenda_responder_aparecer(social[foco].tipo == SPS_CONSENT_SIM);
          reconstruirSocial();
          // A LISTA COMECA DO TOPO depois da resposta. Manter o foco na linha 1
          // deixaria o dedo em cima de uma sugestao que a pessoa nem viu
          // aparecer, e o proximo OK a adicionaria como contato.
          foco = 0;
          scrollY = 0.0f;
          memset(animFoco, 0, sizeof animFoco);
          recomenda_marcar_vistas();
          return;
        case SPS_SUG:
          // UMA ACAO, como o pedido pediu: o OK vincula. O servidor recalcula
          // as sugestoes antes de aceitar, entao um id que ja nao esta na lista
          // (a pessoa revogou entre a tela e o OK) volta recusado.
          if (social[foco].idx >= 0 && social[foco].idx < nSugs)
            recomenda_adicionar_sugerido(sugs[social[foco].idx].id);
          reconstruirSocial();
          if (foco >= nSocial) foco = nSocial > 0 ? nSocial - 1 : 0;
          return;
        case SPS_ADICIONAR:
          // A TELA DE AMIGOS. O painel FICA ABERTO atras: a modal e uma camada
          // por cima dele e Voltar devolve o foco aqui, em vez de jogar a
          // pessoa de volta na home.
          recenviar_abrir_amigos();
          return;
        case SPS_APARECER:
          // MUDAR DE IDEIA CUSTA UM OK, nos dois sentidos. Sem confirmacao de
          // proposito: desligar e a direcao segura, e pedir "tem certeza?" para
          // sair de uma lista e o padrao que faz as pessoas desistirem de sair.
          recomenda_responder_aparecer(recomenda_aparecer() != REC_APARECER_SIM);
          reconstruirSocial();
          return;
        default:
          // A ACAO QUE IMPORTA E ABRIR O TITULO, e o contrato para isso ja
          // existe: o painel entrega o IMDb e app.c resolve. Ele nao conhece
          // detail.c nem a descoberta, exatamente como antes.
          if (social[foco].idx >= 0 && social[foco].idx < nRecs) {
            snprintf(pedido, sizeof pedido, "%s", recs[social[foco].idx].imdb);
            temPedido = 1;
            aberto = 0;
          }
          return;
      }
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
  // A LISTA SOCIAL TAMBEM MUDA COM O PAINEL ABERTO, e agora por tres motivos e
  // nao um: chegou recomendacao, chegou (ou saiu) sugestao, ou a resposta sobre
  // aparecer foi reconciliada com o servidor — esta ultima acontece quando a
  // pessoa respondeu SIM em outra TV e o registro deste aparelho adotou a
  // resposta. Sem ela a pergunta continuaria na tela ja respondida.
  if (aberto && aba == SP_ABA_SOCIAL &&
      (nRecs != recomenda_n() || nSugs != recomenda_n_sugestoes() ||
       consentEstado != recomenda_aparecer())) {
    reconstruirSocial();
    if (foco >= nVisiveis()) foco = nVisiveis() > 0 ? nVisiveis() - 1 : 0;
  }

  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt,
                       aberto ? SP_ABRIR_MS : SP_FECHAR_MS);
  for (i = 0; i < nVisiveis() && i < SP_MAX; i++) {
    float a = (aberto && i == foco) ? 1.0f : 0.0f;
    animFoco[i] = ajustes_animacoes_reduzidas()
      ? a
      : anim_mola(animFoco[i], a, dt,
                  a > animFoco[i] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  }
  // A BOLA DO INTERRUPTOR, na mesma mola do foco (NV_MOLA_FOCO, 95% em 120 ms):
  // as duas coisas acontecem no mesmo OK e tempos diferentes leriam como bug.
  //
  // COM ANIMACOES REDUZIDAS ELE SALTA, e continua legivel — o estado esta na
  // POSICAO e no preenchimento, nunca no movimento. O deslize so acrescenta a
  // leitura de "isto MUDOU", que e um ganho para quem pode ve-lo e nao uma
  // condicao para entender o controle.
  { float alvoSw = (temAbas() && recomenda_aparecer() == REC_APARECER_SIM)
                   ? 1.0f : 0.0f;
    if (animSw < 0.0f) animSw = alvoSw;   // primeira leitura: assenta sem deslizar
    animSw = ajustes_animacoes_reduzidas()
           ? alvoSw : anim_mola(animSw, alvoSw, dt, NV_MOLA_FOCO); }
  // Rola o MINIMO para a linha focada caber inteira, como a grade da
  // Biblioteca. Alinhar a focada ao topo joga o cabecalho para fora na primeira
  // descida e a pessoa perde de vista em que painel esta.
  alvo = scrollY;
  if (foco == SP_FOCO_ABAS) alvo = 0.0f;
  else if (nVisiveis() > 0 && foco >= 0 && foco < nVisiveis()) {
    float janela = SP_LISTA_BASE - listaTopo();
    topo = topoDe(foco);
    // A ALTURA DA LINHA FOCADA, e nao SP_POSTER_H sempre: na aba Social a linha
    // pode ter 84, 112 ou 138px, e usar a maior empurraria a rolagem 54px alem
    // do necessario num interruptor de 104.
    base = topo + (aba == SP_ABA_SOCIAL ? socialAlt(foco) : aba == SP_ABA_AVISOS ? avisos_lista_altura_linha(foco, foco) - 10.0f : SP_POSTER_H);
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

// Uma linha da aba Social: cartaz, titulo, a CARA e o nome de quem mandou,
// filme-ou-serie, a nota do IMDb, a frase — e, quando ainda nao foi lida, a
// barra de acento na borda esquerda.
//
// O cartaz e a MESMA tex_cache do resto do app; a recomendacao guarda a URL do
// poster, a do avatar e a nota no proprio arquivo (recomenda.h), entao a linha
// se desenha sem depender do catalogo. ISSO E O PONTO: o titulo recomendado
// costuma NAO estar no catalogo de quem recebe — se a nota fosse procurada
// ali, ela apareceria justamente nas linhas em que menos importa.
//
// AS QUATRO FAIXAS, dentro dos 138px do cartaz:
//   +0    titulo                       (TXT_CALLOUT, 28)
//   +38   disco de 30 + "Nome · há 2 h"
//   +76   selos: [Filme] [IMDb 8,3]    (REC_SELO_H = 30)
//   +108  a frase entre aspas          (TXT_CAPTION, 22)
#define SPR_AVATAR   30.0f
#define SPR_AV_GAP   12.0f
#define SPR_Y_NOME   38.0f
#define SPR_Y_SELOS  76.0f
#define SPR_Y_FRASE 108.0f
// Barra de "ainda nao lida" na borda esquerda da linha.
#define SPR_BARRA_W   6.0f
// A LINHA DE SUGESTAO. 56 e o mesmo disco do cartao de abertura (RC_AVATAR) e o
// menor em que a INICIAL ainda se le a 3 m — sugestao de conta Nuvio quase
// nunca tem foto, entao a inicial e o caso comum e nao a excecao.
#define SPS_SUG_AV   56.0f
#define SPS_SUG_GAP  18.0f
#define SPS_SUG_PADX 14.0f

// `linha` e a posicao na lista da aba (de onde sai a animacao de foco) e `idx`
// a posicao na lista de recomendacoes. Os dois coincidem hoje — as
// recomendacoes vem primeiro — e sao parametros separados para que continuem
// coincidindo por construcao e nao por sorte no dia em que algo entrar antes.
static void desenhaRecLinha(int linha, int idx, float dx, float y, float a) {
  const RecItem *r = &recs[idx];
  float f = (linha >= 0 && linha < SP_MAX) ? animFoco[linha] : 0.0f;
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

  if (!r->visto) {
    // A MARCA DE "NAO LIDA" E UMA BARRA, e nao mais um ponto de 10px.
    //
    // O ponto morava no vao entre o cartaz e o texto, tinha a area de um grao
    // de arroz a tres metros e sumia por completo quando a linha ganhava a
    // pilula clara do foco (azul 0.42/0.72/0.98 sobre branco 0.96). A barra
    // ocupa a altura INTEIRA do cartaz na borda da camada, onde nada mais
    // desenha, e por isso continua visivel com e sem foco.
    //
    // NA COR DE ACENTO DO APARELHO, e nao no azul cravado de antes: o dono
    // escolhe o acento em Ajustes e toda marca de estado do app ja o obedece.
    //
    // E POR ISSO ELA FICA FORA DA PILULA DO FOCO, em px-24 contra os px-12 em
    // que a pilula comeca. O acento PADRAO e BRANCO: pintada por dentro da
    // pilula clara, a barra sumiria justamente na linha em que o dedo esta.
    // CONFERIDO nas duas capturas — tema OCEANO (azul) e tema padrao (branco).
    //
    // O RAIO E 3px EXPRESSOS EM ALTURAS, e nao 0.5: o SDF de gfx.c normaliza
    // pela ALTURA do retangulo (uAspect = w/h), entao 0.5 num retangulo de
    // 6x138 pede um raio de 69px e o que sai e uma lente pontuda de 50px — foi
    // exatamente o que a primeira captura mostrou.
    float ar, ag, ab;
    GfxRect barra = { px - 24.0f, y, SPR_BARRA_W, SP_POSTER_H };
    ajustes_acento(&ar, &ag, &ab);
    gfx_cor(barra, SPR_BARRA_W * 0.5f / SP_POSTER_H, ar, ag, ab, a);
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
    // O DISCO DA FOTO ANTES DO NOME. Com quatro recomendacoes na tela, a cara
    // e o que distingue uma linha da outra antes de qualquer leitura — era o
    // pedido do dono, e e o unico item da linha que nao depende de ler.
    { GfxRect av = { tx, y + SPR_Y_NOME, SPR_AVATAR, SPR_AVATAR };
      rec_avatar(av, r->deAvatar, r->deNome, r->de, a); }
    { float nx = tx + SPR_AVATAR + SPR_AV_GAP;
      TxtLinha t = txt_linha_corta(TXT_CAPTION2, buf, c2, c2 + 4, c2 + 14, 255,
                                   SP_TEXTO_W - (SPR_AVATAR + SPR_AV_GAP));
      txt_desenhar_alpha(t, nx, y + SPR_Y_NOME + (SPR_AVATAR - t.h) * 0.5f,
                         a * 0.95f); }
    // FILME OU SÉRIE, E A NOTA. `tipo` sempre existiu no RecItem e nunca era
    // desenhado: a linha nao dizia se o amigo estava mandando um filme de duas
    // horas ou oito temporadas.
    { float sx = tx;
      sx += rec_selo_tipo(sx, y + SPR_Y_SELOS, r->tipo, esc, a) + REC_SELO_GAP;
      rec_selo_imdb(sx, y + SPR_Y_SELOS, r->nota, esc, a); }
    { const char *frase = rec_frase(r);
      if (frase[0]) {
        snprintf(buf, sizeof buf, "\xe2\x80\x9c%s\xe2\x80\x9d", frase);
        { TxtLinha t = txt_linha_corta(TXT_CAPTION, buf, c3, c3 + 4, c3 + 14, 255,
                                       SP_TEXTO_W);
          txt_desenhar_alpha(t, tx, y + SPR_Y_FRASE, a * 0.95f); }
      } } }
}

// A LINHA DE ABAS. Sem animacao de cor de proposito: a cor faz parte da chave
// do cache de linhas de text.c, e uma cor por quadro cria uma rasterizacao TTF
// e uma textura GL por quadro — estourado o orcamento, a linha simplesmente
// NAO E DESENHADA (ver a nota longa em ctxmenu.c). O foco aparece no anel, que
// e geometria e nao custa texto.
// Largura da faixa de abas, para a contagem do cabecalho parar antes dela.
static float abasLargura(void) {
  const char *rot[3]; int i; float w = 0.0f;
  int novasRec = recomenda_n_novas(), novasAv = avisos_n_novos();
  rot[SP_ABA_SALVOS] = "SALVOS"; rot[SP_ABA_SOCIAL] = "SOCIAL"; rot[SP_ABA_AVISOS] = "AVISOS";
  for (i = 0; i < 3; i++) {
    int ativa = (i == aba);
    int novas = i == SP_ABA_SOCIAL ? novasRec : i == SP_ABA_AVISOS ? novasAv : 0;
    TxtLinha t;
    if (i == SP_ABA_SOCIAL && !temSocial()) continue;
    t = txt_linha(TXT_CALLOUT, i18n(rot[i]), 176, 176, 176, 255);
    w += t.w + 44.0f + ((!ativa && novas > 0) ? 34.0f : 0.0f) + SP_ABA_GAP;
  }
  return w;
}

static void desenhaAbas(float dx, float a) {
  const char *rot[3];
  float x = SP_X + dx + SP_PAD;
  int i, novasRec = recomenda_n_novas(), novasAv = avisos_n_novos();
  rot[SP_ABA_SALVOS] = "SALVOS";
  rot[SP_ABA_SOCIAL] = "SOCIAL";
  rot[SP_ABA_AVISOS] = "AVISOS";
  for (i = 0; i < 3; i++) {
    int ativa = (i == aba);
    int emFoco = (foco == SP_FOCO_ABAS && ativa);
    int cor = emFoco ? 20 : (ativa ? 246 : 176);
    int novas = i == SP_ABA_SOCIAL ? novasRec : i == SP_ABA_AVISOS ? novasAv : 0;
    TxtLinha t;
    if (i == SP_ABA_SOCIAL && !temSocial()) continue;
    t = txt_linha(TXT_CALLOUT, i18n(rot[i]), cor, cor, cor, 255);
    // O selo so aparece na aba que NAO esta aberta. Ele responde "ha algo
    // novo la?"; com a aba Social na tela, a propria lista responde isso, e o
    // numero ficaria repetido a dois centimetros da contagem do cabecalho.
    // O SELO E PEQUENO E MORA DENTRO DA PILULA (dono, 20/09/2026: "o numero
    // ta feio, menos amador"): 24 px, numeral TXT_MINI, 10 px depois do
    // rotulo, e a pilula cresce para ele — antes eram 32 px encostados na
    // borda, e a contagem do cabecalho vinha logo atras sem folga.
    float selo = (!ativa && novas > 0) ? 34.0f : 0.0f;
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
      tn = txt_linha(TXT_MINI, n, 12, 26, 16, 255);
      b.w = 24.0f; b.h = 24.0f;
      b.x = x + 22.0f + t.w + 10.0f;
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
static void desenhaSocialVazio(float dx, float y0, float a) {
  float x = SP_X + dx + SP_PAD;
  float y = y0 + 8.0f;
  const char *cod = recomenda_meu_codigo();
  { TxtLinha t = txt_linha(TXT_CALLOUT, "Nenhuma recomendação ainda",
                           240, 242, 248, 255);
    txt_desenhar_alpha(t, x, y, a * 0.96f); y += t.h + 14.0f; }
  // EM BLOCO: a coluna do painel tem 688px e a frase tem duas oracoes; numa
  // linha so, a captura saiu cortada no meio.
  y += txt_bloco(TXT_CAPTION,
      "Quando alguém da sua lista te recomendar um filme ou série, ele aparece aqui.",
      190, 194, 204, x, y, SP_INTERNO, 30.0f, a * 0.9f, 3) + 22.0f;
  if (cod[0]) {
    // O CODIGO TAMBEM AQUI, e nao so na tela de amigos: este e o painel que o
    // dono abre com uma tecla, e ditar seis caracteres ao telefone e a acao que
    // resolve uma lista vazia sem depender de ninguem ter aceitado aparecer.
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
    txt_desenhar_alpha(t, x, y, a * 0.85f); }
}

// A PERGUNTA DA PRIMEIRA ENTRADA, por extenso.
//
// AS QUATRO FRASES SAO A FUNCAO INTEIRA, e a ordem delas foi escolhida: o que
// os OUTROS passam a ver vem primeiro, o que eles NAO veem vem logo depois, o
// preco de recusar (nenhum) vem em terceiro e a reversibilidade fecha. Quem ler
// so a primeira ja sabe o essencial; quem ler ate o fim nao encontra nenhuma
// ressalva que contradiga o comeco. Nao ha frase aqui que o codigo nao cumpra:
// o servidor so guarda nome, foto e contatos, e a consulta de sugestao filtra
// por `descobrivel = 1` nos DOIS ramos justamente para esta lista ser verdade.
static void desenhaConsentimento(float dx, float y0, float a) {
  float x = SP_X + dx + SP_PAD;
  float y = y0 + 8.0f;
  // TXT_CALLOUT E NAO TXT_TITULO2. Com o titulo grande a pergunta saiu da
  // captura como "Aparecer para outras pessoa" — 688px de coluna nao cabem uma
  // frase de 29 caracteres naquele corpo, e um enunciado cortado ao meio e
  // pior que um enunciado menor.
  { TxtLinha t = txt_linha(TXT_CALLOUT, "Aparecer para outras pessoas?",
                           246, 247, 252, 255);
    txt_desenhar_alpha(t, x, y, a); y += t.h + 18.0f; }
  y += txt_bloco(TXT_CAPTION,
      "Se você aceitar, quem já te segue no Trakt e os amigos dos seus amigos passam a ver seu nome e sua foto numa lista de sugestões, e podem te adicionar como contato.",
      214, 218, 228, x, y, SP_INTERNO, 30.0f, a * 0.95f, 4) + 18.0f;
  y += txt_bloco(TXT_CAPTION,
      "Eles não veem o que você assiste, o que você salvou nem o que você recomendou. Nada disso sai desta TV.",
      214, 218, 228, x, y, SP_INTERNO, 30.0f, a * 0.95f, 3) + 18.0f;
  y += txt_bloco(TXT_CAPTION,
      "Se você recusar, continua recebendo e enviando recomendações do mesmo jeito. Você só não aparece na lista de ninguém.",
      190, 194, 204, x, y, SP_INTERNO, 30.0f, a * 0.9f, 3) + 18.0f;
  txt_bloco(TXT_CAPTION,
      "Dá para mudar essa resposta quando quiser, no fim desta aba.",
      160, 164, 175, x, y, SP_INTERNO, 30.0f, a * 0.85f, 2);
}

// Uma linha-botao: pilula que inverte no foco, com um subtitulo opcional.
// Mesma pilula e mesmo foco invertido das outras listas do app.
static void desenhaBotaoLinha(int i, float dx, float y, float alt, float a,
                              const char *titulo, const char *sub) {
  GfxRect r = { SP_X + dx + SP_PAD, y, SP_INTERNO, alt };
  float f = (i >= 0 && i < SP_MAX) ? animFoco[i] : 0.0f;
  float lum = anim_mistura(0.176f, 0.961f, f);
  int c1 = f >= 0.5f ? 17 : 240, c2 = f >= 0.5f ? 74 : 168;
  gfx_cor(r, 14.0f / alt, lum, lum, lum, a);
  if (sub && sub[0]) {
    TxtLinha t = txt_linha_corta(TXT_PLR_CORPO, titulo, c1, c1, c1, 255,
                                 SP_INTERNO - 64.0f);
    TxtLinha s = txt_linha_corta(TXT_CAPTION, sub, c2, c2 + 4, c2 + 14, 255,
                                 SP_INTERNO - 64.0f);
    float h = t.h + 8.0f + s.h;
    txt_desenhar_alpha(t, r.x + 32.0f, y + (alt - h) * 0.5f, a);
    txt_desenhar_alpha(s, r.x + 32.0f, y + (alt - h) * 0.5f + t.h + 8.0f,
                       a * 0.95f);
    return;
  }
  { TxtLinha t = txt_linha_corta(TXT_PLR_CORPO, titulo, c1, c1, c1, 255,
                                 SP_INTERNO - 64.0f);
    txt_desenhar_alpha(t, r.x + 32.0f, y + (alt - t.h) * 0.5f, a); }
}

// O INTERRUPTOR DE "APARECER PARA OUTRAS PESSOAS", e por que ele deixou de ser
// mais uma desenhaBotaoLinha.
//
// O DEFEITO: ele era a MESMA pilula, do MESMO tamanho, com o MESMO foco
// invertido de "Adicionar um amigo" e das duas respostas do consentimento. Mas
// "Adicionar um amigo" e uma ACAO — o OK abre um teclado e alguma coisa
// acontece — e isto aqui e um ESTADO que fica, o interruptor de privacidade da
// aba inteira. Duas especies de coisa com a mesma silhueta significa que, do
// sofa, nao da para saber se o OK vai FAZER ou vai MUDAR. Num controle de
// privacidade esse e o pior lugar possivel para ficar ambiguo.
//
// O QUE FOI REJEITADO, e por que:
//
// 1. A CONVENCAO DE AJUSTES — rotulo a esquerda e o valor em texto a direita
//    ("Ligado"/"Desligado", V_LIGA em ajustes.c:135). Rejeitada por tres
//    razoes, e a primeira e a que decide: la o valor a direita e uma COLUNA —
//    TODA linha da tela tem uma, e e a coluna que faz uma palavra ser lida como
//    valor. Aqui seria uma palavra solta na margem direita de uma lista de
//    posteres e de rostos, sem nenhuma outra na mesma prumada; ela leria como
//    um pedaco do subtitulo. Segunda: em Ajustes o OK ENTRA EM EDICAO e sao
//    ESQUERDA/DIREITA que trocam o valor (o bloco `emEdicao` em ajustes.c:2051).
//    Aqui o OK inverte na hora. Vestir a roupa de Ajustes com outro gesto e
//    defeito pior que o que se esta corrigindo. Terceira: "Ligado" nao diz o
//    que esta ligado.
// 2. TRILHA NA COR DE REALCE quando ligado, como num telefone. Rejeitada: o
//    foco desta lista PREENCHE a linha com superficie clara, e um realce claro
//    dentro de uma linha clara some — e exatamente a armadilha que a pilula
//    "Adicionar" de desenhaSugLinha ja documenta logo abaixo. E cor nao pode
//    ser o sinal, porque o estado tem de ser legivel sem cor.
// 3. ESQUERDA = desligar, DIREITA = ligar. Rejeitada: ESQUERDA ja significa
//    "sair do painel" nesta camada (ver spainel_evento), gesto herdado de
//    perfil.c. Um interruptor em que um lado inverte e o outro fecha a tela
//    inteira e pior que interruptor sem lados.
// 4. UMA PALAVRA AO LADO DA BOLA ("Sim"/"Nao"). Rejeitada: se o controle diz o
//    estado, a palavra e a segunda coisa dizendo a mesma coisa — e duas coisas
//    dizendo o mesmo e o que da cara de formulario a uma linha.
//
// COMO O ESTADO E LIDO SEM FOCO E SEM COR. Tres canais redundantes, nenhum
// deles matiz: a POSICAO da bola (48 px de percurso, ~35' de arco a 3 m), o
// PREENCHIMENTO da trilha (anel vazio / capsula cheia) e a POLARIDADE da bola
// (clara dentro do anel vazio, cor da propria linha sobre a capsula cheia).
// MEDIDO em bytes sRGB na captura, nas quatro combinacoes:
//
//   linha em repouso (45)   desligado: anel 152, miolo 45,  bola 240
//                           ligado:    capsula 240,         bola 45
//   linha em foco   (245)   desligado: anel 120, miolo 245, bola 18
//                           ligado:    capsula 18,          bola 245
//
// Contraste da bola contra o que esta atras dela: 12:1, 12:1, 17:1 e 17:1. O
// canal que carrega o estado nunca desce de 12:1 em nenhuma das quatro.
//
// QUAL LADO E O SEGURO, sem sermao. O padrao — e a resposta que o produto
// defende — e NAO aparecer, e esse e o lado da trilha VAZIA. Ligar acende
// alguma coisa; desligado nao ha nada aceso. A assimetria esta na fisica do
// interruptor, e nao num aviso, num alerta ou numa cor de perigo: o dono ja
// recusou copy moralista neste app e um interruptor que repreende e a mesma
// coisa desenhada.
static void desenhaAparecer(int i, float dx, float y, float alt, float a) {
  GfxRect r = { SP_X + dx + SP_PAD, y, SP_INTERNO, alt };
  GfxRect trilho, bola;
  float f = (i >= 0 && i < SP_MAX) ? animFoco[i] : 0.0f;
  float lum = anim_mistura(0.176f, 0.961f, f);
  int esc = f >= 0.5f;
  int c1 = esc ? 17 : 240, c2 = esc ? 74 : 168;
  // Clamp de seguranca: animSw nasce em -1 e so assenta na primeira
  // atualizacao. Um quadro desenhado antes dela (o painel abre e desenha no
  // mesmo quadro) deslocaria a bola para fora da capsula.
  float lig = animSw < 0.0f ? 0.0f : anim_clamp(animSw, 0.0f, 1.0f);
  // A TINTA CONTRARIA A DA LINHA. O foco inverte a linha inteira, entao o
  // interruptor tem de inverter junto — desenhar sempre claro deixaria a bola
  // branca sumida dentro da pilula branca do foco.
  float tinta = esc ? 0.07f : 0.94f;
  float largTexto = SP_INTERNO - 64.0f - SPS_SW_W - SPS_SW_GAP;
  const char *sub = recomenda_aparecer() == REC_APARECER_SIM
      // O SUBTITULO PAROU DE REPETIR O ESTADO. Ele dizia "Sim, voce aparece
      // nas sugestoes de quem te conhece" / "Nao, voce nao aparece na lista de
      // ninguem", que era a unica coisa na linha dizendo ligado ou desligado —
      // agora quem diz isso e o interruptor. Sobrou para o subtitulo o que o
      // interruptor NAO consegue dizer: ligado, QUEM passa a te achar; e
      // desligado, o que voce NAO perde por recusar (nada) — que e a mesma
      // promessa que a pergunta de primeira entrada ja faz, nas mesmas
      // palavras, e a duvida real de quem esta com o dedo em cima.
      //
      // AS DUAS TEM 36 CARACTERES, e isso foi medido e nao estimado: a coluna
      // de texto tem 500 px (688 - 32 - 32 - 96 de trilha - 28 de vao) e o
      // TXT_CAPTION de 22 px gasta ~10,8 px por caractere aqui. "Voce continua
      // recebendo e enviando recomendacoes" pedia ~511 px e saiu da PRIMEIRA
      // captura como "Voce continua recebendo e enviando…", com a palavra que
      // importa cortada fora. "Trocando" diz os dois sentidos numa palavra e
      // guarda o substantivo.
      ? "Quem te conhece te acha nas sugestões"
      : "Você continua trocando recomendações";

  // Raio de 14 px em fracao da ALTURA, como as outras linhas desta camada: em
  // 104 px de altura sao 0,135, longe dos dois tetos do gfx_cor (0,5 e
  // 0,5*w/h = 3,3). Dividir por min(w,h) daria 0,020 aqui e canto vivo.
  gfx_cor(r, 14.0f / alt, lum, lum, lum, a);

  { TxtLinha t = txt_linha_corta(TXT_PLR_CORPO, "Aparecer para outras pessoas",
                                 c1, c1, c1, 255, largTexto);
    TxtLinha s = txt_linha_corta(TXT_CAPTION, sub, c2, c2 + 4, c2 + 14, 255,
                                 largTexto);
    float h = t.h + 8.0f + s.h;
    txt_desenhar_alpha(t, r.x + 32.0f, y + (alt - h) * 0.5f, a);
    txt_desenhar_alpha(s, r.x + 32.0f, y + (alt - h) * 0.5f + t.h + 8.0f,
                       a * 0.95f); }

  // A TRILHA. Raio 0,5 numa caixa 96x48: o teto 0,5*w/h vale 1,0, entao os
  // 0,5 passam inteiros e as pontas saem em semicirculo de 24 px. Capsula de
  // verdade, e nao "quase".
  //
  // OS 32 px DE RECUO SAO OS MESMOS DO TEXTO. Sem eles a capsula encostava na
  // borda da pilula — apareceu na primeira captura e parecia a linha cortada.
  trilho.x = r.x + SP_INTERNO - 32.0f - SPS_SW_W;
  trilho.y = y + (alt - SPS_SW_H) * 0.5f;
  trilho.w = SPS_SW_W;
  trilho.h = SPS_SW_H;
  //
  // DESLIGADA A TRILHA E UM ANEL, e nao uma capsula chapada de alfa baixo. A
  // primeira versao pintava tinta a 16% e MEDI o resultado na captura: a
  // capsula saia em 76 sobre uma linha de 45, ou seja 1,56:1 — e em foco, 209
  // sobre 245, 1,41:1. Nos dois casos a capsula praticamente nao existia, e
  // sem ela a bola clara a esquerda flutua sem dizer que ha um percurso.
  //
  // O ANEL E EXATO, e nao GFX_ANEL: aquele modo sai losangudo (squircle) em
  // diametro pequeno, em todo tamanho que ja se tentou nesta base. Duas
  // formas CONCENTRICAS preenchidas dao um anel exato — com raio
  // 0,5 o centro da tampa fica a h/2 da borda, entao a de fora tem centro em
  // x+24 (h=48) e a de dentro, recuada 4, tem centro em x+4+20 = x+24. Mesmo
  // centro, raios 24 e 20: anel de 4 px uniforme, sem emenda.
  //
  // Medido depois: 152 sobre 45 (4,77:1) em repouso e 120 sobre 245 (3,97:1)
  // em foco, com a bola mantendo 12:1 e 17:1 contra o miolo — que e o ponto,
  // porque quem carrega o ESTADO e a bola e nao a trilha.
  gfx_cor(trilho, 0.5f, tinta, tinta, tinta, anim_mistura(0.55f, 1.0f, lig) * a);
  if (lig < 0.999f) {
    GfxRect furo = { trilho.x + SPS_SW_ANEL, trilho.y + SPS_SW_ANEL,
                     SPS_SW_W - SPS_SW_ANEL * 2.0f,
                     SPS_SW_H - SPS_SW_ANEL * 2.0f };
    // O miolo e a COR DA PROPRIA LINHA e vai sumindo: ligar nao troca de
    // desenho, TAMPA o buraco. Vazio vira cheio, que e a leitura que se quer.
    gfx_cor(furo, 0.5f, lum, lum, lum, (1.0f - lig) * a);
  }

  // A BOLA. Quadrada com raio 0,5 = circulo EXATO pelo SDF (com asp = 1 a
  // funcao vira length(p) - 0,5). Nao e GFX_ANEL: aquele sai losangudo em
  // diametro pequeno, e aqui nem ha anel — sao dois discos preenchidos, que e
  // a saida que gfx.h ja recomenda.
  bola.w = SPS_SW_BOLA;
  bola.h = SPS_SW_BOLA;
  bola.x = trilho.x + SPS_SW_PAD +
           (SPS_SW_W - SPS_SW_PAD * 2.0f - SPS_SW_BOLA) * lig;
  bola.y = trilho.y + SPS_SW_PAD;
  // A COR DA BOLA TROCA EM DEGRAU no meio do percurso, e nao interpolada.
  // Interpolando, no meio do caminho ela valeria 142 em bytes sRGB enquanto o
  // miolo da trilha, meio tampado, vale 120: 1,1:1, ou seja a bola SOME no
  // meio do movimento. Em degrau ela vira ao cruzar o centro — que e o que um
  // interruptor de verdade faz — e nunca chega perto do valor do miolo.
  // (E o mesmo degrau em f > 0.5 que o texto desta linha ja usa, so que aqui
  // a razao e optica e nao o cache de text.c.)
  { float cb = lig > 0.5f ? lum : tinta;
    gfx_cor(bola, 0.5f, cb, cb, cb, a); }
}

// Uma sugestao: a cara, o nome, POR ONDE ela chegou, e a pilula que diz o que
// o OK faz.
//
// A LINHA DA ORIGEM NAO E ENFEITE — e ela que separa "alguem que voce talvez
// conheca" de "um estranho que o app resolveu mostrar". Sem "Segue no Trakt" ou
// "Amigo de Gustavo", a resposta honesta a "quem e essa pessoa?" seria "nao
// sei", e a de quem esta olhando seria recusar.
static void desenhaSugLinha(int i, int idx, float dx, float y, float a) {
  const RecSugestao *s = &sugs[idx];
  float f = (i >= 0 && i < SP_MAX) ? animFoco[i] : 0.0f;
  float px = SP_X + dx + SP_PAD;
  char origem[128];
  int esc = f > 0.5f;

  if (f > 0.01f) {
    // Mesma pilula clara das outras linhas desta camada.
    GfxRect p = { px - 12.0f, y - 10.0f, SP_INTERNO + 24.0f, SPS_H_SUG + 20.0f };
    gfx_cor(p, 0.06f, 0.961f, 0.961f, 0.968f, f * a);
  }
  { GfxRect av = { px, y + (SPS_H_SUG - SPS_SUG_AV) * 0.5f,
                   SPS_SUG_AV, SPS_SUG_AV };
    rec_avatar(av, s->avatar, s->nome, s->id, a); }
  rec_sugestao_origem(origem, sizeof origem, s);
  { float tx = px + SPS_SUG_AV + SPS_SUG_GAP;
    // A PILULA DA ACAO E MEDIDA ANTES DO NOME, e o nome e cortado para caber ao
    // lado dela: sem isso, "Carolina Menezes" passava por baixo de "Adicionar"
    // e as duas ficavam ilegiveis na captura.
    TxtLinha acao = txt_linha(TXT_CAPTION2, "Adicionar",
                              esc ? 32 : 222, esc ? 34 : 226, esc ? 40 : 236, 255);
    float pw = acao.w + SPS_SUG_PADX * 2.0f;
    float larg = SP_INTERNO - (SPS_SUG_AV + SPS_SUG_GAP) - pw - 20.0f;
    int c1 = esc ? 20 : 245, c2 = esc ? 74 : 168;
    { TxtLinha t = txt_linha_corta(TXT_CALLOUT, s->nome, c1, c1 + 1, c1 + 5, 255,
                                   larg);
      txt_desenhar_alpha(t, tx, y + 26.0f, a); }
    { TxtLinha t = txt_linha_corta(TXT_CAPTION2, origem, c2, c2 + 4, c2 + 14,
                                   255, larg);
      txt_desenhar_alpha(t, tx, y + 62.0f, a * 0.95f); }
    // PREENCHIMENTO E NAO CONTORNO, e com as DUAS combinacoes: sobre a pilula
    // clara do foco, um preenchimento branco a 0.12 desaparece — a mesma
    // armadilha de rec_selo_tipo, e a mesma saida.
    { GfxRect p = { px + SP_INTERNO - pw, y + (SPS_H_SUG - REC_SELO_H) * 0.5f,
                    pw, REC_SELO_H };
      gfx_cor(p, 0.5f, esc ? 0.06f : 1.0f, esc ? 0.06f : 1.0f,
              esc ? 0.08f : 1.0f, (esc ? 0.10f : 0.12f) * a);
      txt_desenhar_alpha(acao, p.x + SPS_SUG_PADX,
                         p.y + (REC_SELO_H - acao.h) * 0.5f, a); } }
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
  // recomenda_n() E NAO nRecs: com a pergunta de consentimento na tela a lista
  // local esta vazia de proposito, e escrever "0 recomendações" ao lado de uma
  // aba com o selo em 2 seria o painel se contradizendo em dois centimetros.
  if (aba == SP_ABA_SOCIAL) {
    int n = recomenda_n();
    snprintf(buf, sizeof buf, "%d %s", n,
             i18n(n == 1 ? "recomendação" : "recomendações"));
  }
  else if (aba == SP_ABA_AVISOS) {
    int n = avisos_lista_n(), nv = avisos_n_novos();
    if (nv > 0) snprintf(buf, sizeof buf, i18n("%d avisos · %d novos"), n, nv);
    else snprintf(buf, sizeof buf, "%d %s", n, i18n(n == 1 ? "aviso" : "avisos"));
  }
  else {
    snprintf(buf, sizeof buf, "%d %s   ·   %d %s", nLinhas,
             i18n(nLinhas == 1 ? "título" : "títulos"),
             nCont, i18n("para retomar"));
  }
  // COM ABAS a contagem vai para a DIREITA da propria linha de abas, e nao
  // numa linha solta acima delas: sozinha la em cima ela lia como um titulo
  // orfao, que foi a primeira coisa que saltou na foto ampliada.
  { // Nunca por cima das abas: o que nao cabe entre a faixa e a borda sai
    // com reticencias, em vez de a contagem colar no selo (foto de 20/09).
    float sobra = SP_W - 2.0f * SP_PAD - (temAbas() ? abasLargura() + 24.0f : 0.0f);
    TxtLinha t = txt_linha(TXT_CAPTION2, buf, 160, 164, 175, 255);
    // Com tres abas nao sobra lugar para a contagem na mesma linha: ela
    // SOME em vez de sair cortada ("124 titles · 2 to…" nao diz nada). A
    // informacao continua na propria lista.
    if (temAbas()) {
      if (t.w <= sobra)
        txt_desenhar_alpha(t, SP_X + x + SP_W - SP_PAD - t.w,
                           SP_ABAS_Y + (SP_ABAS_H - t.h) * 0.5f, a * 0.95f);
    }
    else
      txt_desenhar_alpha(t, SP_X + x + SP_PAD, SP_Y + 38.0f, a * 0.95f); }
  if (temAbas()) desenhaAbas(x, a);
  else {
    TxtLinha t = txt_linha(TXT_TITULO2, "Salvos", 246, 247, 252, 255);
    txt_desenhar_alpha(t, SP_X + x + SP_PAD, SP_Y + 74.0f, a);
  }

  if (aba == SP_ABA_AVISOS) {
    gfx_recorte(SP_X + x, listaTopo(), SP_W, SP_LISTA_BASE - listaTopo());
    avisos_lista_desenhar(SP_X + x + SP_PAD, listaTopo() - scrollY, SP_INTERNO, a, foco);
    gfx_sem_recorte();
    return;
  }

  if (aba == SP_ABA_SOCIAL) {
    gfx_recorte(SP_X + x, listaTopo(), SP_W, SP_LISTA_BASE - listaTopo());
    y = listaTopo() - scrollY;
    // O BLOCO DE TEXTO ROLA COM A LISTA, e nao fica preso no topo: ele explica
    // a lista que vem logo abaixo, e um texto fixo com linhas passando por
    // baixo dele leria como duas telas empilhadas.
    if (consentindo())    desenhaConsentimento(x, y, a);
    else if (nRecs == 0)  desenhaSocialVazio(x, y, a);
    y += socialTopo();
    for (i = 0; i < nSocial; i++) {
      float alt = socialAlt(i);
      float cab = socialAntes(i);
      if (cab > 0.0f) {
        // So a secao das sugestoes tem rotulo; o vao do interruptor e mudo.
        if (social[i].tipo == SPS_SUG &&
            y + cab >= listaTopo() && y <= SP_LISTA_BASE) {
          TxtLinha t = txt_linha(TXT_CAPTION2, "Pessoas que você talvez conheça",
                                 150, 154, 165, 255);
          txt_desenhar_alpha(t, SP_X + x + SP_PAD, y + cab - t.h - 12.0f, a * 0.9f);
        }
        y += cab;
      }
      // Fora da janela nao custa texto nem textura — mesma razao da lista de
      // Salvos logo abaixo.
      if (y + alt >= listaTopo() && y <= SP_LISTA_BASE) {
        switch (social[i].tipo) {
          case SPS_REC: desenhaRecLinha(i, social[i].idx, x, y, a); break;
          case SPS_SUG: desenhaSugLinha(i, social[i].idx, x, y, a); break;
          case SPS_ADICIONAR:
            // A linha de "Adicionar um amigo" fecha a lista, e nao um botao
            // solto no rodape: ela rola com o resto e recebe foco como qualquer
            // outra.
            desenhaBotaoLinha(i, x, y, alt, a, "Adicionar um amigo", NULL);
            break;
          case SPS_APARECER:
            // NAO e desenhaBotaoLinha, e a diferenca e o ponto todo desta
            // linha. Ver a nota longa em desenhaAparecer.
            desenhaAparecer(i, x, y, alt, a);
            break;
          case SPS_CONSENT_SIM:
            desenhaBotaoLinha(i, x, y, alt, a, "Sim, pode me mostrar", NULL);
            break;
          default:
            desenhaBotaoLinha(i, x, y, alt, a, "Não, não quero aparecer", NULL);
            break;
        }
      }
      y += alt + SPS_GAP;
    }
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
