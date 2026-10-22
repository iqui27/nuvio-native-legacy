#include "colecoes.h"
#include "rede.h"
#include "js.h"
#include "addons.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <pthread.h>
static pthread_mutex_t colTrava = PTHREAD_MUTEX_INITIALIZER;
// PASTAS EM BLOCOS (#255). Um bloco de 64 so e alocado quando a montagem chega
// nele, e nunca e devolvido: col_folder entrega o ENDERECO da pasta, e quem o
// guarda (vertudo) continua lendo o mesmo lugar depois de um sync, como fazia
// com o vetor estatico. A conta de 296 pastas usa 5 blocos (1,3 MB); o vetor
// fixo de antes custava 13,4 MB para qualquer conta.
#define COL_BLOCO 64
static ColFolder *blocos[COL_MAX / COL_BLOCO];
#define PASTA(i) (blocos[(i) / COL_BLOCO][(i) % COL_BLOCO])
static int count;
// Maior indice ja escrito: as pastas de [count, altaMarca) sobraram de um
// conjunto maior e sao apontadas para `vazias` (ver fecharSobras).
static int altaMarca;
static ColFolder extras[COL_EXTRA_MAX];
static int nExtras;
// FONTES. `fontesConta` e o bloco do conjunto na tela (pacote ou conta); `novas` e
// o que a montagem em curso enche, e vira `fontesConta` em fecharMontagem. Os dois
// terminam com COL_SOURCE_MAX posicoes zeradas de folga: quem guarda o
// endereco de uma pasta e le sources[k] com um k da pasta ANTERIOR que morava
// ali le zeros, e nao memoria alheia — a mesma garantia do vetor embutido.
static ColSource *fontesConta, *novas, *extrasFontes;
static int nFontes, nNovas, capNovas;
// Destino das pastas que sobraram de um conjunto maior. So zeros.
static ColSource vazias[COL_SOURCE_MAX];

static int vagaPasta(int i) {
  int b = i / COL_BLOCO;
  if (i < 0 || i >= COL_MAX) return 0;
  if (!blocos[b] && !(blocos[b] = calloc(COL_BLOCO, sizeof(ColFolder)))) {
    printf("[colecoes] sem memoria para mais %d pastas (bloco %d)\n", COL_BLOCO, b);
    return 0;
  }
  if (i >= altaMarca) altaMarca = i + 1;
  return 1;
}
// As pastas que nao sao mais do conjunto nao podem apontar para um bloco de
// fontes ja liberado.
static void fecharSobras(void) {
  for (int i = count; i < altaMarca; i++) {
    PASTA(i).sources = vazias;
    PASTA(i).nSources = 0;
  }
}
// Espaco para mais uma pasta inteira no fim de `novas`, com a folga zerada do
// fim ja garantida. NULL so sem memoria.
static ColSource *vagaFontes(void) {
  if (nNovas + 2 * COL_SOURCE_MAX > capNovas) {
    int cap = capNovas ? capNovas * 2 : 8 * COL_SOURCE_MAX;
    while (cap < nNovas + 2 * COL_SOURCE_MAX) cap *= 2;
    ColSource *n = realloc(novas, sizeof(ColSource) * (size_t)cap);
    if (!n) { printf("[colecoes] sem memoria para %d fontes\n", cap); return NULL; }
    novas = n; capNovas = cap;
  }
  return novas + nNovas;
}
static void comecarMontagem(void) {
  free(novas); novas = NULL; nNovas = capNovas = 0;
}
// A montagem terminou com as pastas [0,count), na ordem em que as fontes
// entraram em `novas` (realloc pode ter mudado o endereco no caminho: o
// ponteiro de cada pasta e refeito aqui, pela posicao). `novas` vira o bloco
// do conjunto e o anterior e liberado — depois de nenhuma pasta apontar mais
// para ele.
static void fecharMontagem(void) {
  int off = 0;
  if (!vagaFontes()) { count = 0; fecharSobras(); return; }
  memset(novas + nNovas, 0, sizeof(ColSource) * (size_t)(capNovas - nNovas));
  for (int i = 0; i < count; i++) {
    PASTA(i).sources = novas + off;
    off += PASTA(i).nSources;
  }
  fecharSobras();
  free(fontesConta);
  fontesConta = novas;
  nFontes = nNovas;
  novas = NULL; nNovas = capNovas = 0;
}
// Uma impressao digital do conjunto, para decidir se a Home remonta. Era um
// memcmp das pastas com as fontes embutidas; agora as fontes estao fora, e o
// ponteiro muda a cada montagem mesmo com o mesmo conteudo.
static unsigned long long assinatura(void) {
  unsigned long long h = 1469598103934665603ULL;
  for (int i = 0; i < count; i++) {
    ColFolder f = PASTA(i);
    const unsigned char *b;
    size_t k;
    f.sources = NULL;
    b = (const unsigned char *)&f;
    for (k = 0; k < sizeof f; k++) h = (h ^ b[k]) * 1099511628211ULL;
    b = (const unsigned char *)PASTA(i).sources;
    for (k = 0; b && k < sizeof(ColSource) * (size_t)PASTA(i).nSources; k++)
      h = (h ^ b[k]) * 1099511628211ULL;
  }
  return h;
}
// TROCA DE PERFIL (col_esquecer_perfil). A pasta do pacote fica guardada para a
// arte curada voltar quando a conta mandar as colecoes do perfil novo; semConta
// diz que folders[] foi esvaziado e que, se a conta nao mandar nada, deve
// continuar vazio — mostrar o pacote ali seria mostrar as colecoes de outro.
static char dirPacote[600];
static int semConta;
static int contaAplicada;
static void localiza(char *value,size_t cap,const char *dir) {
  if(!value[0]||strstr(value,"://")||value[0]=='/')return;
  char rel[600];snprintf(rel,sizeof rel,"%s",value);snprintf(value,cap,"%s/%s",dir,rel);
}
int col_n(void) { int n; pthread_mutex_lock(&colTrava); n = count; pthread_mutex_unlock(&colTrava); return n; }

// SOBE A CADA TROCA DO CONJUNTO DE PASTAS. A home decide se remonta por uma
// assinatura, e ate agora essa assinatura so olhava as fileiras de CATALOGO —
// nada de colecao. Como as colecoes da conta chegam DEPOIS de o catalogo se
// acomodar, a assinatura nao mudava e a remontagem nao acontecia: as pastas
// existiam em memoria e nao entravam na tela. E o #30.
//
// Contador e nao `count`: trocar quinze pastas por outras quinze deixa o
// numero igual e a home errada.
static unsigned revisao;
unsigned col_revisao(void) { unsigned n; pthread_mutex_lock(&colTrava); n = revisao; pthread_mutex_unlock(&colTrava); return n; }
int col_tem_conta(void) { int n; pthread_mutex_lock(&colTrava); n = contaAplicada; pthread_mutex_unlock(&colTrava); return n; }
// Tira as extras que estao em folders[] e poe de volta as de agora. Chamada
// depois de TODA reconstrucao (pacote ou conta) e a cada troca do conjunto.
static void aplicarExtras(void) {
  int i, k = 0;
  for (i = 0; i < count; i++) if (!PASTA(i).extra) { if (k != i) PASTA(k) = PASTA(i); k++; }
  count = k;
  for (i = 0; i < nExtras && vagaPasta(count); i++) { PASTA(count) = extras[i]; count++; }
  fecharSobras();
}

static int fontesValidas(const ColFolder *v) {
  return v->nSources < 0 ? 0 : (v->nSources > COL_SOURCE_MAX ? COL_SOURCE_MAX : v->nSources);
}

int col_extra_definir(const ColFolder *v, int n) {
  int i, total = 0;
  ColSource *bloco, *velho;
  if (n < 0) n = 0;
  if (n > COL_EXTRA_MAX) n = COL_EXTRA_MAX;
  for (i = 0; i < n && v; i++) total += fontesValidas(&v[i]);
  // As fontes da pasta extra sao COPIADAS: quem chama monta a pasta na pilha.
  bloco = calloc((size_t)(total + COL_SOURCE_MAX), sizeof(ColSource));
  if (!bloco) { printf("[colecoes] pastas extras: sem memoria, ficam as de antes\n"); return nExtras; }
  pthread_mutex_lock(&colTrava);
  velho = extrasFontes;
  extrasFontes = bloco;
  nExtras = 0;
  for (i = 0, total = 0; i < n && v; i++) {
    int ns = fontesValidas(&v[i]);
    extras[nExtras] = v[i];
    extras[nExtras].sources = bloco + total;
    extras[nExtras].nSources = ns;
    if (ns && v[i].sources) memcpy(bloco + total, v[i].sources, sizeof(ColSource) * (size_t)ns);
    total += ns;
    extras[nExtras].extra = 1;
    extras[nExtras].local = 0;   // nao e do pacote: nao disputa a arte curada
    nExtras++;
  }
  aplicarExtras();
  free(velho);
  // A home so remonta quando ESTE numero muda. Sem o bump, fixar uma lista na
  // Home so apareceria no proximo ciclo de rede.
  revisao++;
  pthread_mutex_unlock(&colTrava);
  return nExtras;
}

// Fonte da conta vem com addonId e sem URL; a URL so existe depois que a sonda
// leu o manifesto daquele addon. Resolver no acesso deixa a pasta pronta assim
// que a sonda passar, sem ninguem precisar avisar.
//
// ColSource.base FICA EM 600 (#201), e nao em NV_ADDON_URL_MAX: sao
// COL_MAX x COL_SOURCE_MAX = 8192 fontes, e 2048 em cada uma custaria ~12 MB.
// Ela e so um atalho: a base inteira mora na tabela de addons e toda leitura
// cai em addons_base_por_id(addonId) quando o atalho esta vazio. Entao a regra
// e NUNCA guardar aqui uma base cortada — o que nao cabe fica vazio.
static int baseCabe(ColSource *a, const char *base) {
  if (!base || strlen(base) >= sizeof a->base) { a->base[0] = 0; return 0; }
  snprintf(a->base, sizeof a->base, "%s", base);
  return 1;
}
// Le a base de uma fonte do JSON para `a->base`, sem nunca deixa-la cortada.
// 0 quando a chave nao existe. A base que nao cabe fica VAZIA e a linha do log
// diz a pasta e o tamanho (nunca a URL: ela carrega a chave do addon).
static int lerBase(const char *s, const char *se, const char *chave, ColSource *a, const char *pasta) {
  char lida[NV_ADDON_URL_MAX];
  a->base[0] = 0;
  if (!js_texto(s, se, chave, lida, sizeof lida)) return 0;
  if (!baseCabe(a, lida))
    printf("[col] %s: base de addon com %lu caracteres ou mais nao cabe na fonte (maximo %lu): resolvida pelo id do addon\n",
           pasta && *pasta ? pasta : "pasta", (unsigned long)strlen(lida),
           (unsigned long)sizeof a->base - 1);
  return 1;
}
static void resolverBases(ColFolder *v) {
  for (int s = 0; s < v->nSources; s++)
    if (!v->sources[s].prov[0] && !v->sources[s].base[0] && v->sources[s].addonId[0])
      baseCabe(&v->sources[s], addons_base_por_id(v->sources[s].addonId));
}
const ColFolder *col_folder(int i) {
  pthread_mutex_lock(&colTrava);
  if (i < 0 || i >= count) { pthread_mutex_unlock(&colTrava); return NULL; }
  resolverBases(&PASTA(i));
  pthread_mutex_unlock(&colTrava);
  return &PASTA(i);
}
// FNV-1a de 32 bits, igual ao de homeestado.c: o valor tem de bater com o do
// hashColecoes que morava la, ou todo snapshot da Home ja gravado seria
// descartado uma vez sem motivo.
static unsigned colHashBytes(unsigned h, const void *data, size_t n) {
  const unsigned char *p = (const unsigned char *)data;
  while (n--) h = (h ^ *p++) * 16777619u;
  return h;
}
static unsigned colHashTexto(unsigned h, const char *s) {
  if (s) h = colHashBytes(h, s, strlen(s));
  return (h ^ 0xffu) * 16777619u;
}
unsigned col_hash_estrutura(void) {
  unsigned h = 2166136261u;
  pthread_mutex_lock(&colTrava);
  h = colHashBytes(h, &count, sizeof count);
  for (int i = 0; i < count; i++) {
    ColFolder *f = &PASTA(i);
    // Igual a col_folder(): a base de uma fonte de addon e resolvida antes de
    // entrar no hash.
    resolverBases(f);
    h = colHashTexto(h, f->group);
    h = colHashTexto(h, f->id);
    h = colHashBytes(h, &f->nSources, sizeof f->nSources);
    for (int s = 0; s < f->nSources; s++) {
      const ColSource *src = &f->sources[s];
      h = colHashTexto(h, src->prov); h = colHashTexto(h, src->addonId);
      h = colHashTexto(h, src->base); h = colHashTexto(h, src->type);
      h = colHashTexto(h, src->catId); h = colHashTexto(h, src->tmdbTipo);
      h = colHashBytes(h, &src->tmdbId, sizeof src->tmdbId);
      h = colHashTexto(h, src->midia); h = colHashTexto(h, src->ordenar);
      h = colHashTexto(h, src->ordem); h = colHashBytes(h, &src->traktLista, sizeof src->traktLista);
    }
  }
  pthread_mutex_unlock(&colTrava);
  return h;
}
// O ADDON DE UM GRUPO DE COLECOES, quando ha um so. Um grupo e um conjunto de
// pastas, cada pasta com fontes de varios addons (ou TMDB/Trakt), entao o
// "addon da colecao" nao existe em geral — mas na pratica quase toda colecao
// aponta para um addon so, e e assim que a tela de fileiras consegue agrupa-la
// junto dos catalogos dele em vez de num bloco "Colecao" a parte. Devolve o
// addonId dominante quando ele responde por TODAS as fontes de addon do grupo,
// senao "" (grupo misto fica como "Colecao").
int col_grupo_addon(const char *name, char *dst, unsigned n) {
  int i, k;
  char dono[96] = "";
  if (n) dst[0] = 0;
  for (i = 0; i < count; i++) {
    if (strcasecmp(name, PASTA(i).group)) continue;
    for (k = 0; k < PASTA(i).nSources; k++) {
      const ColSource *sc = &PASTA(i).sources[k];
      if (sc->prov[0] || !sc->addonId[0]) continue;   // TMDB/Trakt nao e addon
      if (!dono[0]) snprintf(dono, sizeof dono, "%s", sc->addonId);
      else if (strcmp(dono, sc->addonId)) return 0;   // misto
    }
  }
  if (!dono[0]) return 0;
  snprintf(dst, n, "%s", dono);
  return 1;
}

int col_forma_texto(const char *s) {
  if (s && !strcasecmp(s, "POSTER")) return COL_FORMA_POSTER;
  if (s && (!strcasecmp(s, "LANDSCAPE") || !strcasecmp(s, "WIDE"))) return COL_FORMA_PAISAGEM;
  return COL_FORMA_QUADRADO;
}

int col_grupo_forma(const char *name) {
  int conta[COL_FORMA_N] = {0}, primeira = -1, melhor, i;
  for (i = 0; i < count; i++) {
    int f = PASTA(i).forma;
    if (strcasecmp(name, PASTA(i).group) || f < 0 || f >= COL_FORMA_N) continue;
    if (primeira < 0) primeira = f;
    conta[f]++;
  }
  if (primeira < 0) return COL_FORMA_PAISAGEM;
  melhor = primeira;
  for (i = 0; i < COL_FORMA_N; i++) if (conta[i] > conta[melhor]) melhor = i;
  return melhor;
}

int col_grupo(const char *name,int *indices,int max) {
  int n=0;for(int i=0;i<count&&n<max;i++) if(!strcasecmp(name,PASTA(i).group)) indices[n++]=i;return n;
}
// RESOLVE A BASE ANTES DE COMPARAR, e isso e o conserto de verdade do #18.
//
// Era o unico leitor de `folders[]` que NAO passava por col_folder(), e por
// isso o unico que via a fonte crua. Uma fonte da CONTA chega com `addonId` e
// SEM URL (ver lerColecaoWeb), e a URL so existe depois que alguem le o
// manifesto daquele addon. A sequencia esta medida na C9 e anotada em
// descoberta.c (geracaoPedida): desc_iniciar() em 0,9 s, o sync aplica o que
// veio da conta em ~2 s, e os manifestos so sao lidos em ~7 s — colecoes e
// addons chegam no MESMO bloco de sync_passo, entao a colecao e sempre guardada
// antes de qualquer manifesto ter sido lido. Ou seja: no instante em que
// col_definir_json guarda a fonte, addons_base_por_id ainda devolve "" — a
// fonte fica com base VAZIA e esta funcao nunca casava com a base real que a
// descoberta lhe passa.
//
// Resultado: o catalogo que esta dentro de uma colecao nao era reconhecido como
// tal e virava fileira solta na home, exatamente o que o relator do #18 continua
// vendo depois do v1.0.11. O conserto daquela versao (pular o catalogo que ja
// aparece numa pasta) estava certo e simplesmente nunca disparava para quem tem
// colecao da CONTA — que no Tizen e o unico caminho possivel, porque o
// collections.json nao vai no .wgt (ver #10). No aparelho de quem consertou as
// pastas vinham do pacote, com `base` escrita no arquivo, e por isso funcionava.
//
// O unico caminho que resolvia a base era col_folder(), chamado do DESENHO da
// home — uma corrida contra o fio da descoberta, o que explica "as vezes".
//
// CUSTO, medido e nao suposto (tests/colcusto.c no Mac M-series, -O1, 32 pastas
// x 8 fontes = 256 fontes, 1.000.000 de chamadas no PIOR caso, em que nada casa
// e as duas varreduras vao ate o fim): 2048 e 2053 ms antes, 2101 e 2116 ms
// depois. Sao ~53 ns a mais por chamada, 2,6%, para 256 fontes — resolverBases
// vira um teste de `base[0]` por fonte e nao chama addons_base_por_id nenhuma
// vez depois que a base entrou. A montagem chama isto uma vez por catalogo
// declarado: com os 605 do Xperience sao ~32 us a mais no ciclo inteiro.
// QUANTAS FONTES DE COLECAO AINDA NAO TEM BASE. Enquanto a base e "", a fonte
// nao casa com nada e o catalogo dela vira fileira solta (#18). A base so
// aparece depois que o manifesto do addon e lido — na LG, 14 s depois do
// arranque. Este numero e o que separa "a colecao nao chegou" de "a colecao
// chegou mas ainda nao da para reconhece-la", que produzem o MESMO sintoma.
int col_fontes_sem_base(void) {
  int i, s, n = 0;
  pthread_mutex_lock(&colTrava);
  for (i = 0; i < count; i++) {
    for (s = 0; s < PASTA(i).nSources; s++)
      if (!PASTA(i).sources[s].prov[0] && !PASTA(i).sources[s].base[0] &&
          !addons_base_por_id(PASTA(i).sources[s].addonId)[0]) n++;
  }
  pthread_mutex_unlock(&colTrava);
  return n;
}

// O OUTRO LADO DA COMPARACAO. Base REDIGIDA: a fonte do Xperience carrega um
// JWT no caminho e este log vai para relato de defeito.
void col_despejar_fontes(int max) {
  char seg[120];
  int i, s2, n = 0;
  pthread_mutex_lock(&colTrava);
  for (i = 0; i < count && n < max; i++) {
    resolverBases(&PASTA(i));
    for (s2 = 0; s2 < PASTA(i).nSources && n < max; s2++, n++) {
      const ColSource *v = &PASTA(i).sources[s2];
      // O addonId ENTRA no despejo, e ele e o campo que decide.
      //
      // Uma fonte sem base nao esta "meio pronta": ela quer um addon que o app
      // nao soube apontar. Sem o id nao da para dizer se o addon nao esta
      // instalado, se o manifesto dele falhou, ou se o id mudou dos dois lados
      // — tres causas com o mesmo "fontes-sem-base>0". Um relator do #18 mandou
      // colecoes=137 e fontes-sem-base=360: sem esta linha nao havia como saber
      // QUAIS 360.
      if (v->prov[0])
        printf("[col]   fonte[%s/%s]: prov=%s tmdbTipo=%s tmdbId=%ld lista=%ld midia=%s\n",
               PASTA(i).group, PASTA(i).title, v->prov, v->tmdbTipo,
               v->tmdbId, v->traktLista, v->midia);
      else
        printf("[col]   fonte[%s/%s]: addon=%s base=%s tipo=%s id=%s\n",
               PASTA(i).group, PASTA(i).title,
               v->addonId[0] ? v->addonId : "(sem id)",
               v->base[0] ? rede_url_publica(v->base, seg, sizeof seg)
                          : "(NAO RESOLVIDA)",
               v->type, v->catId);
    }
  }
  pthread_mutex_unlock(&colTrava);
}

// POR QUE ESTA FILEIRA NAO FOI ENGOLIDA. O despejo de fontes nao respondia
// isso: com 137 pastas ele mostra as 6 primeiras, todas da MESMA pasta, e um
// relator do #18 mandou exatamente isso — seis fontes de "Streaming/Netflix"
// que nao tem nada a ver com as fileiras soltas dele. Amostra cega nao serve
// para comparar dois lados.
//
// Aqui a pergunta e feita ao contrario: dada a fileira, ATE ONDE ela chegou
// antes de nao casar. Os niveis sao cumulativos e a ordem e a da comparacao em
// col_por_catalogo:
//
//   0  nenhuma fonte tem esta base       — o addon nao esta em colecao nenhuma
//   1  base casa, `type` nao             — a colecao pede movie e a fileira e series
//   2  base e `type` casam, `catId` nao  — id de catalogo diferente dos dois lados
//   3  os tres casam                     — e ai o motivo NAO e o casamento: o
//                                          grupo esta oculto (ver
//                                          dentroDeColecaoVisivelBase)
//
// Nada de URL sai daqui de proposito. O que o relator precisa mandar e o
// NIVEL, e a base do Xperience leva um JWT no caminho — imprimi-la redigida
// esconderia justamente o trecho que diferencia duas bases, e imprimi-la
// inteira publicaria a credencial dele num relato de defeito.
int col_diagnostico(const char *base, const char *type, const char *id,
                    char *grupo, unsigned n) {
  int i, s, melhor = 0;
  if (grupo && n) grupo[0] = 0;
  if (!base || !base[0] || !type || !id) return 0;
  pthread_mutex_lock(&colTrava);
  for (i = 0; i < count; i++) {
    for (s = 0; s < PASTA(i).nSources; s++) {
      const ColSource *v = &PASTA(i).sources[s];
      int nivel;
      if (v->prov[0]) continue;   // fonte tmdb/trakt nao casa com catalogo de addon
      const char *b = v->base[0] ? v->base : addons_base_por_id(v->addonId);
      if (strcmp(b, base)) continue;
      nivel = 1;
      if (!strcmp(v->type, type)) {
        nivel = 2;
        if (!strcmp(v->catId, id)) nivel = 3;
      }
      if (nivel > melhor) {
        melhor = nivel;
        if (grupo && n) snprintf(grupo, n, "%s", PASTA(i).group);
        if (melhor == 3) goto pronto;
      }
    }
  }
pronto:
  pthread_mutex_unlock(&colTrava);
  return melhor;
}

// A copia entregue a descoberta leva as fontes JUNTO: a pasta so aponta para o
// bloco do conjunto, e o proximo sync o libera.
typedef struct { ColFolder f; ColSource s[COL_SOURCE_MAX]; } ColCopia;
// Per-thread snapshot on the heap (pthread key, freed on thread exit). A
// `static __thread ColCopia` (~52 KB) inflated PT_TLS for every thread, and
// Tizen 4/5's manual ELF loader cannot initialize compiler TLS at all (#317).
static pthread_key_t colCopiaKey;
static pthread_once_t colCopiaOnce = PTHREAD_ONCE_INIT;
static int colCopiaKeyOk;
static void colCopiaCriar(void) { colCopiaKeyOk = pthread_key_create(&colCopiaKey, free) == 0; }
static ColCopia *colCopiaDoFio(void) {
  pthread_once(&colCopiaOnce, colCopiaCriar);
  if (!colCopiaKeyOk) return NULL;
  ColCopia *p = pthread_getspecific(colCopiaKey);
  if (!p) {
    p = calloc(1, sizeof *p);
    if (p && pthread_setspecific(colCopiaKey, p)) { free(p); p = NULL; }
  }
  return p;
}
const ColFolder *col_por_catalogo(const char *base,const char *type,const char *id) {
  // Base vazia nao pergunta nada: sem esta guarda uma consulta sem URL casava
  // com QUALQUER fonte cuja base ainda estivesse vazia — um falso positivo que
  // esconderia a fileira errada.
  if(!base||!base[0]||!type||!id) return NULL;
  // Discovery runs concurrently with main-thread account sync. Never let it
  // observe the cleared/partially parsed builder or keep a pointer that sync
  // can replace after the lock is released.
  ColCopia *copiaP = colCopiaDoFio();
  if (!copiaP) return NULL;
#define copia (*copiaP)
  const ColFolder *resultado = NULL;
  pthread_mutex_lock(&colTrava);
  for(int i=0;i<count;i++) {
    for(int s=0;s<PASTA(i).nSources;s++) {
      const ColSource *v=&PASTA(i).sources[s];
      const char *b = v->base[0] ? v->base : addons_base_por_id(v->addonId);
      if(!strcmp(b,base)&&!strcmp(v->type,type)&&!strcmp(v->catId,id)) {
        int ns = fontesValidas(&PASTA(i));
        copia.f = PASTA(i);
        memcpy(copia.s, PASTA(i).sources, sizeof(ColSource) * (size_t)ns);
        copia.f.sources = copia.s; copia.f.nSources = ns;
        resolverBases(&copia.f); resultado = &copia.f; goto pronto;
      }
    } }
pronto:
  pthread_mutex_unlock(&colTrava);
  return resultado;
#undef copia
}
/* Arte editorial: JPEG primeiro, PNG depois.
 *
 * O gerador escrevia PNG 4K e os heros somavam 142 MB — 45% do pacote inteiro,
 * para imagens SEM canal alfa (colortype 2, conferido nos arquivos), ou seja
 * pagando o preco do PNG sem usar nada do que ele oferece. Em JPEG a mesma arte
 * cabe numa fracao disso e a TV nao ve diferenca.
 *
 * A ordem importa e o PNG FICA como reserva: quem ja tem o pacote antigo
 * instalado, ou quem regerar a arte com a ferramenta antiga, continua com a
 * pagina ilustrada em vez de cair no fundo chapado. */
static int arteEditorial(char *saida,size_t n,const char *dir,const char *sub,
                         const char *id,const char *sufixo) {
  snprintf(saida,n,"%s/%s/%s-%s.jpg",dir,sub,id,sufixo);
  if(!access(saida,R_OK)) return 1;
  snprintf(saida,n,"%s/%s/%s-%s.png",dir,sub,id,sufixo);
  return !access(saida,R_OK);
}

static int carregarPacote(const char *dir) {
  revisao++;
  semConta = 0;
  if (dir && dir != dirPacote) snprintf(dirPacote, sizeof dirPacote, "%s", dir);
  char path[700];snprintf(path,sizeof path,"%s/collections.json",dir);
  FILE *f=fopen(path,"rb");if(!f)return 0;
  fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);
  if(size<2||size>4000000){fclose(f);return 0;}
  char *body=malloc((size_t)size+1);if(!body){fclose(f);return 0;}
  size_t got=fread(body,1,(size_t)size,f);body[got]=0;fclose(f);count=0;comecarMontagem();
  for(const char *g=js_array(body,NULL,"groups");g;g=js_prox(js_fim(g))) {
    const char *end=js_fim(g);char group[64],groupId[64]="";js_texto(g,end,"title",group,sizeof group);js_texto(g,end,"id",groupId,sizeof groupId);
    for(const char *p=js_array(g,end,"folders");p&&vagaPasta(count);p=js_prox(js_fim(p))) {
      const char *pe=js_fim(p);ColFolder *v=&PASTA(count);memset(v,0,sizeof *v);
      if(!(v->sources=vagaFontes()))break;
      memset(v->sources,0,sizeof(ColSource)*COL_SOURCE_MAX);
      snprintf(v->group,sizeof v->group,"%s",group);snprintf(v->groupId,sizeof v->groupId,"%s",groupId);
      js_texto(p,pe,"id",v->id,sizeof v->id);js_texto(p,pe,"title",v->title,sizeof v->title);
      js_texto(p,pe,"cover",v->cover,sizeof v->cover);js_texto(p,pe,"hero",v->hero,sizeof v->hero);js_texto(p,pe,"logo",v->logo,sizeof v->logo);
      localiza(v->cover,sizeof v->cover,dir);localiza(v->hero,sizeof v->hero,dir);localiza(v->logo,sizeof v->logo,dir);
      v->hideTitle=js_num(p,pe,"hideTitle",0);v->frames=js_num(p,pe,"frames",0);
      if(v->frames<0||v->frames>90)v->frames=0;
      /* O pacote nao traz tileShape: sem ele fica PAISAGEM (o zero), que e o
         desenho para o qual a arte curada dele foi feita. */
      { char forma[16]=""; if(js_texto(p,pe,"tileShape",forma,sizeof forma)) v->forma=col_forma_texto(forma); }
      snprintf(v->frameDir,sizeof v->frameDir,"%s/collections/%s",dir,v->id);
      /* Local paired artwork survives catalog imports. Activate only a complete pair. */
      char editorial[512];
      if(arteEditorial(editorial,sizeof editorial,dir,"editorial",v->id,"home")&&
         arteEditorial(v->detailHero,sizeof v->detailHero,dir,"editorial",v->id,"detail")) {
        snprintf(v->hero,sizeof v->hero,"%s",editorial);v->editorial=1;
      } else v->detailHero[0]=0;
      char cinematic[512],cinematicDetail[512];
      if(arteEditorial(cinematic,sizeof cinematic,dir,"cinematic",v->id,"home")&&
         arteEditorial(cinematicDetail,sizeof cinematicDetail,dir,"cinematic",v->id,"detail")) {
        snprintf(v->hero,sizeof v->hero,"%s",cinematic);
        snprintf(v->detailHero,sizeof v->detailHero,"%s",cinematicDetail);
        v->editorial=2;
      }
      for(const char *s=js_array(p,pe,"sources");s&&v->nSources<COL_SOURCE_MAX;s=js_prox(js_fim(s))) {
        const char *se=js_fim(s);ColSource *a=&v->sources[v->nSources];
        js_texto(s,se,"title",a->title,sizeof a->title);lerBase(s,se,"base",a,v->title);
        js_texto(s,se,"type",a->type,sizeof a->type);js_texto(s,se,"catId",a->catId,sizeof a->catId);js_texto(s,se,"genre",a->genre,sizeof a->genre);
        if(a->base[0]&&a->type[0]&&a->catId[0])v->nSources++;
      }
      v->local=1;
      if(v->nSources&&v->title[0]){nNovas+=v->nSources;count++;}
    }
  }free(body);fecharMontagem();aplicarExtras();return count;
}
int col_carregar(const char *dir) {
  pthread_mutex_lock(&colTrava);
  int n = carregarPacote(dir);
  pthread_mutex_unlock(&colTrava);
  return n;
}
const char *col_banner(const ColFolder *f) {
  if (!f) return "";
  if (f->hero[0]) return f->hero;
  if (f->cover[0]) return f->cover;
  return f->groupBackdrop;
}
const char *col_capa(const ColFolder *f) {
  if (!f) return "";
  return f->cover[0] ? f->cover : f->groupBackdrop;
}
void col_cor(const ColFolder *f,float *r,float *g,float *b) {
  *r=.16f;*g=.23f;*b=.30f;if(!f)return;
  if(strstr(f->title,"Netflix")){*r=.52f;*g=.035f;*b=.065f;}
  else if(strstr(f->title,"Prime")){*r=.025f;*g=.32f;*b=.58f;}
  else if(strstr(f->title,"Disney")){*r=.10f;*g=.13f;*b=.46f;}
  else if(strstr(f->title,"Max")||strstr(f->title,"HBO")){*r=.27f;*g=.12f;*b=.44f;}
  else if(strstr(f->title,"Letterboxd")){*r=.07f;*g=.32f;*b=.21f;}
  else if(!strcmp(f->group,"Awards")){*r=.40f;*g=.31f;*b=.095f;}
  else if(!strcmp(f->group,"Directors")){*r=.29f;*g=.24f;*b=.19f;}
}

// ---------------------------------------------------------------- conta

// A base da fonte na MESMA forma de addons_base (nv_addon_base, addonurl.h):
// sem /manifest.json nem barra final, com a query — e o que faz a comparacao
// com a lista de addons e o pedido de catalogo (nv_addon_url) baterem.
static void tirarManifest(char *base) {
  char t[NV_ADDON_URL_MAX];
  nv_addon_base(base, t, sizeof t);
  memcpy(base, t, strlen(t) + 1);   // nunca cresce: so tira
}

// Uma colecao do web -> N pastas em `folders`. Mesma traducao de
// tools/import-collections.mjs, sem baixar arte: cover/hero/logo ficam como URL
// e tex_cache baixa quando desenhar.
// POR QUE UMA PASTA SOME INTEIRA, contado em vez de silencioso.
//
// Uma pasta so entra com pelo menos UMA fonte utilizavel e um titulo. As
// descartadas nao deixavam rastro nenhum: quem instalava uma colecao na conta e
// nao a via na TV nao tinha como saber se ela nao chegou, se chegou vazia, ou se
// foi recusada aqui — e os tres tem conserto diferente. E o issue #13.
static int fPulProvedor, fPulSemFonte, fPulSemTitulo, fPulCheio, fGifCortado;
static char fPrimeiraPulada[128];

// Devolve 0 quando a colecao NAO COUBE (teto de pastas, de fontes ou falta de
// memoria). Quem chama desfaz a colecao inteira: cortar no meio deixava a
// colecao com parte das pastas e o resto das fontes solto na Home (#255).
static int lerColecaoWeb(const char *c, const char *ce) {
  char group[64] = "", groupId[64] = "", fundo[512] = "";   // js_texto nao zera o que nao acha
  js_texto_raiz_em(c, ce, "title", group, sizeof group);
  js_texto_raiz_em(c, ce, "id", groupId, sizeof groupId);
  js_texto_raiz_em(c, ce, "backdropImageUrl", fundo, sizeof fundo);
  if (!group[0]) return 1;
  for (const char *p = js_array(c, ce, "folders"), *pe = NULL; p; p = js_prox(pe)) {
    if (!vagaPasta(count)) return 0;
    pe = js_fim(p); ColFolder *v = &PASTA(count); memset(v, 0, sizeof *v);
    if (!(v->sources = vagaFontes())) return 0;
    snprintf(v->group, sizeof v->group, "%s", group);
    snprintf(v->groupId, sizeof v->groupId, "%s", groupId);
    js_texto_raiz_em(p, pe, "id", v->id, sizeof v->id); js_texto_raiz_em(p, pe, "title", v->title, sizeof v->title);
    js_texto(p, pe, "coverImageUrl", v->cover, sizeof v->cover);
    js_texto(p, pe, "heroBackdropUrl", v->hero, sizeof v->hero);
    snprintf(v->groupBackdrop, sizeof v->groupBackdrop, "%s", fundo);
    js_texto(p, pe, "titleLogoUrl", v->logo, sizeof v->logo);
    // GIF DE FOCO (#29). Fica como URL, igual a cover/hero/logo: quem for
    // anima-lo pede o arquivo ao cache de disco (tex_arquivo) na hora em que o
    // cartaz recebe foco. O campo pode simplesmente nao vir — pasta sem
    // animacao e o caso comum, e ai `focusGif` continua vazio.
    //
    // focusGifEnabled:false DESLIGA a animacao mesmo com a URL presente, que e
    // a mesma regra de tools/import-collections.mjs. Ausente vale como ligado.
    js_texto(p, pe, "focusGifUrl", v->focusGif, sizeof v->focusGif);
    // URL QUE NAO COUBE (#141): js_texto corta calado no tamanho do campo, e
    // uma URL cortada nao e URL — baixaria um 404 ou outra coisa, e o cartaz
    // ficaria parado sem dizer nada. Cheia ate o ultimo byte = cortada:
    // descarta e conta (a linha do fim diz quantas).
    if (strlen(v->focusGif) >= sizeof v->focusGif - 1) { v->focusGif[0] = 0; fGifCortado++; }
    if (strlen(v->cover) >= sizeof v->cover - 1) { v->cover[0] = 0; fGifCortado++; }
    { char b[8]; v->hideTitle = js_bruto(p, pe, "hideTitle", b, sizeof b) && strstr(b, "true") ? 1 : 0; }
    // FORMA DO CARTAO: tileShape, e posterShape como o web aceita
    // (homeScreen.js le `item.tileShape || item.posterShape`). Ausente vira
    // quadrado, que e o que o web desenha para a mesma pasta.
    { char forma[16] = "";
      if (!js_texto(p, pe, "tileShape", forma, sizeof forma))
        js_texto(p, pe, "posterShape", forma, sizeof forma);
      v->forma = col_forma_texto(forma); }
    { char b[8];
      if (js_bruto(p, pe, "focusGifEnabled", b, sizeof b) && strstr(b, "false")) v->focusGif[0] = 0; }
    const char *src = js_array(p, pe, "sources");
    if (!src) src = js_array(p, pe, "catalogSources");
    for (const char *s = src, *se = NULL; s && v->nSources < COL_SOURCE_MAX; s = js_prox(se)) {
      se = js_fim(s); ColSource *a = &v->sources[v->nSources]; char prov[16] = "";
      memset(a, 0, sizeof *a);
      js_texto(s, se, "provider", prov, sizeof prov);
      // Fontes nao-addon (issue #44): o editor do site grava provider "tmdb"
      // (listas, colecoes, pessoas/diretores, empresas, redes ou um discover
      // com filtros) e "trakt" (listas publicas). Nao sao catalogos de addon:
      // carregam campos proprios e quem busca os itens e o vertudo, via
      // desc_vertudo_fonte. Antes disto a pasta ficava com nSources==0 e era
      // descartada inteira — era a colecao "instalada no site" que nunca
      // aparecia na TV.
      if (!strcasecmp(prov, "tmdb")) {
        js_texto(s, se, "title", a->title, sizeof a->title);
        js_texto(s, se, "tmdbSourceType", a->tmdbTipo, sizeof a->tmdbTipo);
        a->tmdbId = (long)js_num(s, se, "tmdbId", 0.0);
        js_texto(s, se, "mediaType", a->midia, sizeof a->midia);
        js_texto(s, se, "sortBy", a->ordenar, sizeof a->ordenar);
        js_bruto(s, se, "filters", a->filtros, sizeof a->filtros);
        if (!a->title[0])
          snprintf(a->title, sizeof a->title, "%s", a->tmdbTipo[0] ? a->tmdbTipo : "TMDB");
        snprintf(a->prov, sizeof a->prov, "tmdb"); v->nSources++; continue;
      }
      if (!strcasecmp(prov, "trakt")) {
        js_texto(s, se, "title", a->title, sizeof a->title);
        a->traktLista = (long)js_num(s, se, "traktListId", 0.0);
        js_texto(s, se, "mediaType", a->midia, sizeof a->midia);
        js_texto(s, se, "sortBy", a->ordenar, sizeof a->ordenar);
        js_texto(s, se, "sortHow", a->ordem, sizeof a->ordem);
        if (!a->title[0]) snprintf(a->title, sizeof a->title, "Lista Trakt");
        if (a->traktLista <= 0) { fPulProvedor++; continue; }
        snprintf(a->prov, sizeof a->prov, "trakt"); v->nSources++; continue;
      }
      if (prov[0] && strcasecmp(prov, "addon")) { fPulProvedor++; continue; }
      if (!lerBase(s, se, "addonBaseUrl", a, v->title)) lerBase(s, se, "addon_base_url", a, v->title);
      tirarManifest(a->base);
      js_texto(s, se, "addonId", a->addonId, sizeof a->addonId);
      if (!a->base[0]) baseCabe(a, addons_base_por_id(a->addonId));
      js_texto(s, se, "type", a->type, sizeof a->type);
      if (!js_texto(s, se, "catalogId", a->catId, sizeof a->catId)) js_texto(s, se, "catalog_id", a->catId, sizeof a->catId);
      if (!js_texto(s, se, "title", a->title, sizeof a->title) && !js_texto(s, se, "catalogName", a->title, sizeof a->title))
        snprintf(a->title, sizeof a->title, "%s", a->catId);
      js_texto(s, se, "genre", a->genre, sizeof a->genre);
      if (!strcmp(a->genre, "None")) a->genre[0] = 0;
      // Sem base MAS com addonId entra: a base chega quando a sonda ler o manifesto.
      if ((a->base[0] || a->addonId[0]) && a->type[0] && a->catId[0]) v->nSources++;
    }
    if (v->nSources && v->title[0]) {
      if (nNovas + v->nSources > COL_FONTES_MAX) return 0;
      nNovas += v->nSources; count++;
    } else {
      if (!v->nSources) fPulSemFonte++; else fPulSemTitulo++;
      if (!fPrimeiraPulada[0] && v->title[0])
        snprintf(fPrimeiraPulada, sizeof fPrimeiraPulada, "%s", v->title);
    }
  }
  return 1;
}

// URL de fundo do Xperience com estilo escolhido: covers/<estilo>/ que nao e
// o default. So o host e o sufixo importam; o nome do estilo e do CDN.
static int xperienceEstilo(const char *url) {
  const char *c;
  if (!url || !strstr(url, "cdn.xperience-app.com/")) return 0;
  c = strstr(url, "/covers/");
  if (!c || !strstr(c, ".backdrop.")) return 0;
  return strncmp(c, "/covers/default/", 16) != 0;
}

// "ARTE DAS PASTAS DA CONTA" (Ajustes, desligado de fabrica). Desligado, a
// pasta da conta que casa com uma do pacote fica com a arte curada do pacote
// (a regra de sempre, logo abaixo). Ligado, a capa, o fundo e o logo que a
// conta manda vencem — o que a conta nao tem continua o do pacote.
//
// Trocar o ajuste refaz o casamento na hora (col_arte_conta): folders[] ja tem
// a arte da regra anterior, entao o pacote e relido e a ultima resposta da
// conta, guardada aqui, e aplicada de novo.
static int arteConta;
static char *ultimoJson;
static int ultimoComConta;

// Only a complete array in a recognized response can replace the account
// snapshot. A missing RPC row, null, or a truncated response is not a deletion.
static const char *jsonEspacos(const char *p) {
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
  return p;
}

static const char *jsonCadeia(const char *p) {
  if (*p++ != '"') return NULL;
  while (*p && *p != '"') {
    if ((unsigned char)*p < 0x20) return NULL;
    if (*p++ != '\\') continue;
    if (!*p) return NULL;
    if (*p == 'u') {
      p++;
      for (int i = 0; i < 4; i++, p++)
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') ||
              (*p >= 'A' && *p <= 'F'))) return NULL;
    } else {
      if (!strchr("\"\\/bfnrt", *p)) return NULL;
      p++;
    }
  }
  return *p == '"' ? p + 1 : NULL;
}

// js_fim locates a closing delimiter, but does not validate JSON grammar.
// Validate every value, including fields after collections, before accepting
// an authoritative snapshot or admitting it to the last-good sync cache.
static const char *jsonValor(const char *p, unsigned profundidade) {
  p = jsonEspacos(p);
  if (profundidade > 64) return NULL;
  if (*p == '"') return jsonCadeia(p);
  if (*p == '{' || *p == '[') {
    int objeto = *p++ == '{';
    char fecha = objeto ? '}' : ']';
    p = jsonEspacos(p);
    if (*p == fecha) return p + 1;
    for (;;) {
      if (objeto) {
        if (!(p = jsonCadeia(p))) return NULL;
        p = jsonEspacos(p);
        if (*p++ != ':') return NULL;
      }
      if (!(p = jsonValor(p, profundidade + 1))) return NULL;
      p = jsonEspacos(p);
      if (*p == fecha) return p + 1;
      if (*p++ != ',') return NULL;
      p = jsonEspacos(p);
    }
  }
  if (!strncmp(p, "true", 4)) return p + 4;
  if (!strncmp(p, "false", 5)) return p + 5;
  if (!strncmp(p, "null", 4)) return p + 4;
  if (*p == '-') p++;
  if (*p == '0') p++;
  else {
    if (*p < '1' || *p > '9') return NULL;
    do { p++; } while (*p >= '0' && *p <= '9');
  }
  if (*p == '.') {
    p++;
    if (*p < '0' || *p > '9') return NULL;
    do { p++; } while (*p >= '0' && *p <= '9');
  }
  if (*p == 'e' || *p == 'E') {
    p++;
    if (*p == '+' || *p == '-') p++;
    if (*p < '0' || *p > '9') return NULL;
    do { p++; } while (*p >= '0' && *p <= '9');
  }
  return p;
}

static int jsonCompleto(const char *p) {
  const char *fim = p ? jsonValor(p, 0) : NULL;
  return fim && !*jsonEspacos(fim);
}

// The display-text decoder normalizes escaped whitespace. A JSON snapshot
// needs the exact decoded controls, so invalid controls inside strings cannot
// become valid spaces and an embedded NUL cannot hide a malformed suffix.
static int jsonDecodificar(const char *p, char *saida) {
  p++;
  while (*p != '"') {
    if (*p != '\\') { *saida++ = *p++; continue; }
    p++;
    if (*p == 'u') {
      char hex[5], trecho[15], utf8[8];
      memcpy(hex, p + 1, 4); hex[4] = 0;
      unsigned cp = (unsigned)strtoul(hex, NULL, 16);
      int n = 6;
      if (!cp) return 0;
      if (cp >= 0xD800 && cp <= 0xDBFF && !strncmp(p + 5, "\\u", 2)) {
        memcpy(hex, p + 7, 4);
        unsigned lo = (unsigned)strtoul(hex, NULL, 16);
        if (lo >= 0xDC00 && lo <= 0xDFFF) n = 12;
      }
      trecho[0] = '"'; memcpy(trecho + 1, p - 1, (size_t)n);
      trecho[n + 1] = '"'; trecho[n + 2] = 0;
      if (!js_cadeia(trecho, utf8, sizeof utf8)) return 0;
      size_t k = strlen(utf8); memcpy(saida, utf8, k); saida += k;
      p += n - 1;
    } else {
      char c = *p++;
      switch (c) {
        case 'b': c = '\b'; break;
        case 'f': c = '\f'; break;
        case 'n': c = '\n'; break;
        case 'r': c = '\r'; break;
        case 't': c = '\t'; break;
      }
      *saida++ = c;
    }
  }
  *saida = 0;
  return 1;
}

static const char *valorColecoes(const char *p, const char *nome) {
  if (!p || *p++ != '{') return NULL;
  for (;;) {
    const char *k;
    while (*p && (unsigned char)*p <= ' ') p++;
    if (*p != '"') return NULL;
    k = p++;
    while (*p && *p != '"') { if (*p == '\\' && p[1]) p++; p++; }
    if (!*p) return NULL;
    p++;
    int mesma = (size_t)(p - k) == strlen(nome) && !strncmp(k, nome, (size_t)(p - k));
    while (*p && (unsigned char)*p <= ' ') p++;
    if (*p++ != ':') return NULL;
    while (*p && (unsigned char)*p <= ' ') p++;
    if (mesma) return p;
    if (*p == '{' || *p == '[') p = js_fim(p);
    else if (*p == '"') {
      p++;
      while (*p && *p != '"') { if (*p == '\\' && p[1]) p++; p++; }
      if (*p) p++;
    } else while (*p && *p != ',' && *p != '}') p++;
    while (*p && (unsigned char)*p <= ' ') p++;
    if (*p++ != ',') return NULL;
  }
}

static int arrayCompleto(const char *p) {
  const char *f, *v;
  if (!p || *p != '[') return 0;
  f = js_fim(p);
  if (!f || f <= p || f[-1] != ']') return 0;
  v = p + 1;
  while (*v && (unsigned char)*v <= ' ') v++;
  if (*v == ']') return 1;
  for (;;) {
    if (*v != '{') return 0;
    v = js_fim(v);
    if (!v || v >= f || v[-1] != '}') return 0;
    while (*v && (unsigned char)*v <= ' ') v++;
    if (*v == ']') return v == f - 1;
    if (*v++ != ',') return 0;
    while (*v && (unsigned char)*v <= ' ') v++;
  }
}

static int prepararResposta(const char *json, char **solto, const char **arrOut) {
  if (!jsonCompleto(json)) return 0;
  while (*json && (unsigned char)*json <= ' ') json++;
  const char *f = js_fim(json);
  if ((*json != '{' && *json != '[') || !f || f <= json ||
      f[-1] != (*json == '{' ? '}' : ']')) return 0;
  const char *resto = f;
  while (*resto && (unsigned char)*resto <= ' ') resto++;
  if (*resto) return 0;
  const char *linha = *json == '[' ? js_raiz_array(json) : json;
  const char *v = linha ? valorColecoes(linha, "\"collections_json\"") : NULL;
  int embrulhado = v != NULL;
  if (v) {
    if (*v == '"') {
      size_t n = strlen(v);
      *solto = malloc(n + 1);
      if (!*solto || !jsonDecodificar(v, *solto)) return 0;
      json = *solto;
      if (!jsonCompleto(json)) return 0;
    } else json = v;
  }
  while (*json && (unsigned char)*json <= ' ') json++;
  f = js_fim(json);
  if ((*json != '{' && *json != '[') || !f || f <= json ||
      f[-1] != (*json == '{' ? '}' : ']')) return 0;
  if (*solto) {
    resto = f;
    while (*resto && (unsigned char)*resto <= ' ') resto++;
    if (*resto) return 0;
  }
  const char *array = *json == '[' ? json : valorColecoes(json, "\"collections\"");
  // An empty outer RPC result contains no saved snapshot. An explicit
  // collections/collections_json empty array is authoritative.
  int explicito = *json != '[' || embrulhado;
  if (!arrayCompleto(array)) return 0;
  const char *primeiro = array + 1;
  while (*primeiro && (unsigned char)*primeiro <= ' ') primeiro++;
  if (*primeiro == ']' && !explicito) return 0;
  // Validate identities and the folder arrays before mutating anything. A
  // response cut between collections must not publish a partial snapshot.
  //
  // O FIM DE CADA OBJETO E ACHADO UMA VEZ. js_fim varre o objeto inteiro, e esta
  // validacao chamava js_fim(p) tres vezes por colecao e por pasta (mais o
  // valorColecoes repetido): perfil de tests/sync_aplicar_perf.sh, 60% do custo
  // do ciclo de sync era js_fim. Na TV do dono (Mali-G52, A55) o ciclo inteiro
  // pesava 59-95 ms num quadro so, a cada cinco minutos.
  for (const char *p = *primeiro == ']' ? NULL : primeiro, *pFim; p; p = js_prox(pFim)) {
    char id[96] = "", titulo[128] = "";
    pFim = js_fim(p);
    js_texto_raiz_em(p, pFim, "id", id, sizeof id);
    js_texto_raiz_em(p, pFim, "title", titulo, sizeof titulo);
    const char *pastas = valorColecoes(p, "\"folders\"");
    if (!id[0] || !titulo[0] || !arrayCompleto(pastas)) {
      return 0;
    }
    for (const char *pf = js_raiz_array(pastas), *pfFim; pf; pf = js_prox(pfFim)) {
      id[0] = titulo[0] = 0;
      pfFim = js_fim(pf);
      js_texto_raiz_em(pf, pfFim, "id", id, sizeof id);
      js_texto_raiz_em(pf, pfFim, "title", titulo, sizeof titulo);
      const char *fontes = valorColecoes(pf, "\"sources\"");
      if (!fontes) fontes = valorColecoes(pf, "\"catalogSources\"");
      if (!id[0] || !titulo[0] || !arrayCompleto(fontes)) return 0;
    }
  }
  // MEDIDO na conta real: collections_json e o ARRAY direto, nao {collections}.
  // js_raiz_array pula o '[' e para no primeiro elemento, como js_array faz.
  *arrOut = *primeiro == ']' ? NULL : primeiro;
  return 1;
}

int col_resposta_valida(const char *json) {
  char *solto = NULL;
  const char *arr = NULL;
  int ok = prepararResposta(json, &solto, &arr);
  free(solto);
  return ok;
}

static int definirJson(const char *json) {
  char *solto = NULL;
  int fundosXp = 0, arteDaConta = 0;
  const char *arr = NULL;
  int antes, nConta;
  const char *original = json;
  if (!prepararResposta(json, &solto, &arr)) {
    printf("[collections] missing or incomplete account snapshot retained\n");
    free(solto); return 0;
  }
  int antesTela = count;
  int recarregou = 0;
  // O QUE ESTA NA TELA, antes de mexer: decide no fim se a Home remonta.
  for (int i = 0; i < count; i++) resolverBases(&PASTA(i));
  unsigned long long assinaturaAntes = assinatura();
  // Depois de uma troca de perfil folders[] esta vazio: recarrega o pacote para
  // o casamento de arte abaixo ter com quem casar. A revisao volta ao que era —
  // quem decide se a home remonta e o resultado, nao esta recarga.
  if (semConta && arr && dirPacote[0]) {
    unsigned rev = revisao;
    carregarPacote(dirPacote);
    revisao = rev;
    recarregou = 1;
  }
  antes = count;
  // A conta manda o CONJUNTO e a ordem. Mas o pacote traz as mesmas pastas
  // (mesmo id: o collections.json e gerado do perfil do dono) com arte editorial,
  // quadros de animacao e ajustes curados que a conta nao tem — a versao local
  // da pasta e a que fica, com grupo e titulo da conta. Sem isto cada pull
  // trocava a arte curada pela capa crua do CDN.
  //
  // So os CABECALHOS (~4 KB cada): as fontes delas nao sao lidas no casamento,
  // a pasta casada leva as fontes da conta. Antes eram 52 KB por pasta, 13 MB
  // alocados e copiados a cada pull com 256 pastas.
  ColFolder *antigas = malloc(sizeof(ColFolder) * (size_t)(antes > 0 ? antes : 1));
  if (!antigas) {
    // semConta: a tela so tinha as extras. Volta a isso.
    if (recarregou) { count = 0; aplicarExtras(); semConta = 1; }
    free(solto); return 0;
  }
  for (int i = 0; i < antes; i++) antigas[i] = PASTA(i);
  count = 0;
  comecarMontagem();
  fPulProvedor = fPulSemFonte = fPulSemTitulo = fPulCheio = fGifCortado = 0;
  fPrimeiraPulada[0] = 0;
  { const char *c = arr, *cFim;
    for (; c && *c == '{'; c = js_prox(cFim)) {
      int c0 = count, f0 = nNovas;
      cFim = js_fim(c);
      if (lerColecaoWeb(c, cFim)) continue;
      // NAO COUBE: sai a colecao inteira, e o log diz qual. As seguintes ainda
      // tentam — uma colecao menor pode caber no que sobrou.
      { char titulo[128] = "";
        int nPastas = 0;
        const char *p;
        count = c0; nNovas = f0;
        js_texto_raiz_em(c, cFim, "title", titulo, sizeof titulo);
        for (p = js_array(c, cFim, "folders"); p; p = js_prox(js_fim(p))) nPastas++;
        fPulCheio++;
        printf("[colecoes] colecao \"%s\" fora: %d pasta(s) nao cabem (teto de %d pastas e "
               "%d fontes; %d pastas e %d fontes ja dentro)\n",
               titulo, nPastas, COL_MAX, COL_FONTES_MAX, count, nNovas); }
    } }
  fecharMontagem();
  nConta = count;
  // UMA LINHA QUE RESPONDE "cade a colecao que eu instalei". Cada contagem e um
  // conserto diferente: provedor sem equivalente e falta de recurso, pasta sem
  // fonte e dado incompleto do lado da conta, e teto cheio e limite nosso.
  if (fPulProvedor || fPulSemFonte || fPulSemTitulo || fPulCheio)
    printf("[colecoes] descartadas: %d fonte(s) de provedor nao-addon, "
           "%d pasta(s) sem fonte utilizavel, %d sem titulo, %d colecao(oes) inteira(s) alem do teto "
           "(%d pastas / %d fontes)%s%s\n",
           fPulProvedor, fPulSemFonte, fPulSemTitulo, fPulCheio, COL_MAX, COL_FONTES_MAX,
           fPrimeiraPulada[0] ? " | primeira: " : "", fPrimeiraPulada);
  if (nConta && antigas) {
    int casadas = 0;
    for (int i = 0; i < count; i++) for (int j = 0; j < antes; j++) {
      if (!antigas[j].local || strcmp(antigas[j].id, PASTA(i).id)) continue;
      ColFolder v = antigas[j];
      snprintf(v.group, sizeof v.group, "%s", PASTA(i).group);
      snprintf(v.groupId, sizeof v.groupId, "%s", PASTA(i).groupId);
      snprintf(v.title, sizeof v.title, "%s", PASTA(i).title);
      // A FORMA E DA CONTA: e escolha feita no editor do web, e o pacote nem
      // tem o campo.
      v.forma = PASTA(i).forma;
      // Preserve artwork, never stale account membership or source ordering.
      v.nSources = PASTA(i).nSources;
      v.sources = PASTA(i).sources;
      // O GIF DA CONTA SO ENTRA ONDE NAO HA SEQUENCIA LOCAL, e isso nao abre
      // excecao na regra acima: a versao local nao tem GIF nenhum para perder.
      // col_carregar nunca preenche focusGif — no pacote o GIF ja virou
      // 001.jpg..090.jpg na importacao e o que resta dele e frames+frameDir.
      //
      // Entao ha dois casos, e so um deles muda de mao:
      //   frames > 0  — a pasta ja anima pela sequencia curada. Fica como esta.
      //   frames == 0 — nao ha animacao nenhuma para preservar (117 das 169
      //                 pastas do collections.json do pacote estao assim, por
      //                 falta de focusGifUrl no perfil na hora da importacao).
      //                 Descartar a URL da conta aqui seria jogar fora a unica
      //                 animacao que existe, sem nada no lugar.
      if (!v.frames) snprintf(v.focusGif, sizeof v.focusGif, "%s", PASTA(i).focusGif);
      // FUNDO ESCOLHIDO NO XPERIENCE (19/09/2026). O CDN passou a ter 17
      // estilos de fundo por marca, em covers/<estilo>/<pasta>.backdrop.webp
      // (3840x2160), e quem escolhe e o dono, no Xperience, por pasta ou por
      // colecao inteira. A escolha chega aqui como heroBackdropUrl. Ate entao
      // a versao do pacote ganhava sempre — e o hero.jpg importado E o fundo
      // "default", o degrade chapado. Estilo escolhido e intencao expressa:
      // vence o pacote, inclusive o par cinematic, e desliga o modo editorial
      // para o heroi desenhar full-bleed com a logo da marca por cima, que e
      // como esses fundos foram feitos para aparecer. "default" nao e escolha:
      // fica o pacote, que nao precisa baixar nada.
      if (xperienceEstilo(PASTA(i).hero)) {
        snprintf(v.hero, sizeof v.hero, "%s", PASTA(i).hero);
        v.editorial = 0;
        v.detailHero[0] = 0;
        fundosXp++;
      }
      // ARTE DA CONTA LIGADA: campo a campo, so o que a conta mandou. A capa
      // da conta leva junto o GIF dela (a sequencia do pacote e da capa do
      // pacote); o fundo da conta desliga o modo editorial, como o Xperience.
      if (arteConta) {
        int trocou = 0;
        if (PASTA(i).cover[0]) {
          snprintf(v.cover, sizeof v.cover, "%s", PASTA(i).cover);
          snprintf(v.focusGif, sizeof v.focusGif, "%s", PASTA(i).focusGif);
          v.frames = 0;
          trocou = 1;
        }
        if (PASTA(i).hero[0]) {
          snprintf(v.hero, sizeof v.hero, "%s", PASTA(i).hero);
          v.editorial = 0;
          v.detailHero[0] = 0;
          trocou = 1;
        }
        if (PASTA(i).logo[0]) {
          snprintf(v.logo, sizeof v.logo, "%s", PASTA(i).logo);
          trocou = 1;
        }
        arteDaConta += trocou;
      }
      PASTA(i) = v; casadas++; break;
    }
    printf("[colecoes] %d pastas da conta casaram com a arte do pacote, %d com fundo escolhido no Xperience, %d com a arte da conta\n",
           casadas, fundosXp, arteDaConta);
  }
  if (nConta) {
    // SO AQUI, e nao na entrada da funcao. Bumpar de saida faria a assinatura
    // da home mudar a CADA ciclo de sync, inclusive quando a conta veio vazia e
    // as pastas locais foram mantidas — uma remontagem por ciclo, de graca, que
    // reinicia animacoes e refaz o foco.
    // QUANTAS TEM GIF (#141): "Netflix anima e Apple TV nao" comeca aqui — se
    // a conta so mandou focusGifUrl para uma pasta, o resto nao e defeito de
    // animacao. Capa .gif na URL e so indicio (a decisao e pelos bytes, ver
    // gifcolecao.h); conta aqui para o log ja responder o caso comum.
    { int comGif = 0, capaGif = 0;
      for (int i = 0; i < count; i++) {
        if (PASTA(i).focusGif[0]) comGif++;
        else if (strstr(PASTA(i).cover, ".gif") || strstr(PASTA(i).cover, "giphy.com/") ||
                 strstr(PASTA(i).cover, "tenor.com/")) capaGif++;
      }
      printf("[colecoes] %d pastas vindas da conta: %d com focusGifUrl, %d sem ele com capa que parece GIF\n",
             nConta, comGif, capaGif);
      if (fGifCortado)
        printf("[colecoes] %d URL(s) de capa/GIF passavam de 511 bytes e foram descartadas\n", fGifCortado); }
  }
  aplicarExtras();
  for (int i = 0; i < count; i++) resolverBases(&PASTA(i));
  int mudou = count != antesTela || assinatura() != assinaturaAntes;
  if (mudou) revisao++;
  free(antigas);
  semConta = nConta == 0;
  contaAplicada = 1;
  { char *copia = strdup(original);
    if (copia) { free(ultimoJson); ultimoJson = copia; } }
  ultimoComConta = nConta > 0;
  printf("[collections] account snapshot: %d folders, revision=%u, changed=%d\n",
         nConta, revisao, mudou);
  // MEMORIA DO CONJUNTO (#255): blocos de pastas alocados + bloco de fontes.
  { int nb = 0;
    for (int b = 0; b < COL_MAX / COL_BLOCO; b++) nb += blocos[b] != NULL;
    printf("[colecoes] memoria: %d pasta(s) em %d bloco(s) = %lu KB, %d fonte(s) = %lu KB\n",
           count, nb, (unsigned long)(sizeof(ColFolder) * COL_BLOCO * (size_t)nb / 1024),
           nFontes, (unsigned long)(sizeof(ColSource) * (size_t)(nFontes + COL_SOURCE_MAX) / 1024)); }
  free(solto);
  return nConta;
}
int col_definir_json(const char *json) {
  pthread_mutex_lock(&colTrava);
  int n = definirJson(json);
  pthread_mutex_unlock(&colTrava);
  return n;
}

void col_arte_conta(int sim) {
  char *j;
  sim = sim ? 1 : 0;
  if (sim == arteConta) return;
  arteConta = sim;
  // Sem resposta da conta (ou sem pacote) nao ha casamento a refazer: o proximo
  // col_definir_json ja le o valor novo.
  if (!ultimoJson || !ultimoComConta || !dirPacote[0]) return;
  j = strdup(ultimoJson);
  if (!j) return;
  pthread_mutex_lock(&colTrava);
  carregarPacote(dirPacote);
  definirJson(j);
  pthread_mutex_unlock(&colTrava);
  free(j);
}

void col_esquecer_perfil(void) {
  pthread_mutex_lock(&colTrava);
  free(ultimoJson);
  ultimoJson = NULL;
  ultimoComConta = 0;
  contaAplicada = 0;
  count = 0;
  semConta = 1;
  aplicarExtras();
  // Nenhuma pasta aponta mais para o bloco da conta (so as extras ficaram).
  free(fontesConta); fontesConta = NULL; nFontes = 0;
  revisao++;
  pthread_mutex_unlock(&colTrava);
  printf("[colecoes] troca de perfil: colecoes do perfil anterior fora da tela\n");
}

void col_chave_grupo(const char *group, char *dst, unsigned n) {
  for (int i = 0; i < count; i++)
    if (!strcasecmp(PASTA(i).group, group) && PASTA(i).groupId[0]) {
      snprintf(dst, n, "collection_%s", PASTA(i).groupId); return; }
  snprintf(dst, n, "collection_%s", group);
}

void col_chave_pasta(const ColFolder *f, char *dst, unsigned n) {
  if (!dst || !n) return;
  snprintf(dst, n, "collection_%s", f ? (f->groupId[0] ? f->groupId : f->group) : "");
}

int col_grupo_chave(const char *chave, int *indices, int max) {
  int n = 0;
  for (int i = 0; i < count && n < max; i++) {
    char k[192]; col_chave_pasta(&PASTA(i), k, sizeof k);
    if (!strcmp(k, chave)) indices[n++] = i;
  }
  return n;
}

int col_grupo_forma_chave(const char *chave) {
  int conta[COL_FORMA_N] = {0}, primeiro = -1, melhor;
  for (int i = 0; i < count; i++) {
    char k[192]; col_chave_pasta(&PASTA(i), k, sizeof k);
    int f = PASTA(i).forma;
    if (strcmp(k, chave) || f < 0 || f >= COL_FORMA_N) continue;
    if (primeiro < 0) primeiro = f;
    conta[f]++;
  }
  if (primeiro < 0) return COL_FORMA_PAISAGEM;
  melhor = primeiro;
  for (int i = 0; i < COL_FORMA_N; i++) if (conta[i] > conta[melhor]) melhor = i;
  return melhor;
}

// Ver colecoes.h. "Cara de id": tem '_' e nenhum espaco — o id cru do
// catalogo que a aba mostrava ("streaming_netflix_movies · Movies", pasta
// Netflix, 01/10). Nome de verdade com underscore e sem espaco nao foi visto.
static int caraDeId(const char *s, const char *catId) {
  if (!s || !s[0]) return 1;
  if (catId && !strcmp(s, catId)) return 1;
  return strchr(s, '_') && !strchr(s, ' ');
}
void col_nome_fonte(const ColSource *s, const char *manifesto, char *dst, unsigned n) {
  const char *nome = "";
  if (!dst || !n) return;
  if (s && !caraDeId(s->title, s->catId)) nome = s->title;
  else if (s && !caraDeId(manifesto, s->catId)) nome = manifesto;
  snprintf(dst, n, "%s", nome);
}
