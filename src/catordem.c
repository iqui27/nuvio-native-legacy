#include "catordem.h"
#include "js.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>

// Chave de DESATIVAR do web (<base>_<tipo>_<catalogoId>_<nome>) nao cabe em 192:
// so a base do Xperience tem 367 caracteres. A lista de ocultos guarda as duas
// formas misturadas, entao ela usa o tamanho da maior.
#define CATORD_DESL 384

// CONCORRENCIA (#203): catordem_ler roda no fio do sync e a `montar` da
// descoberta (e o hash da home) le por catordem_n/catordem_chave em outro. Duas
// bancadas: o leitor monta na que nao esta publicada e so troca o ponteiro
// `vivo` sob `trava`; as consultas olham `vivo` sob a mesma trava. O ponteiro
// devolvido por catordem_chave segue valido ate DUAS leituras depois (a bancada
// so e reescrita na leitura seguinte a seguinte), e leituras sao minutos
// separadas.
typedef struct {
  char ord[CATORD_MAX][CATORD_CHAVE];
  int  no;
  char ocu[CATORD_MAX][CATORD_DESL];
  int  nc;
} Banca;
static Banca bancas[2];
static Banca *vivo = &bancas[0];
static Banca *alvo = &bancas[0];   // onde add* escreve (so durante a leitura)
#define ordem   (alvo->ord)
#define nOrdem  (alvo->no)
#define ocultos (alvo->ocu)
#define nOcultos (alvo->nc)
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static int  temOrdem;
static int  temNLanc, valNLanc, temSublinhado, valSublinhado;
static unsigned revisao;
unsigned catordem_revisao(void) {
  unsigned r;
  pthread_mutex_lock(&trava); r = revisao; pthread_mutex_unlock(&trava);
  return r;
}
// Itens da resposta que nao couberam (ordem ou ocultos), so para o log.
static int nAlem;

// Impressao digital do que foi lido. Substitui as copias inteiras de antes
// (antesOrdem/antesOcultos: 442 KB com 768 itens, 1,2 MB com 2048).
static unsigned long long assinaturaLida(void) {
  unsigned long long h = 1469598103934665603ULL;
  int i;
  const unsigned char *b;
  for (i = 0; i < nOrdem; i++) {
    for (b = (const unsigned char *)ordem[i]; *b; b++) h = (h ^ *b) * 1099511628211ULL;
    h = (h ^ 1u) * 1099511628211ULL;
  }
  h = (h ^ (unsigned)nOrdem) * 1099511628211ULL;
  for (i = 0; i < nOcultos; i++) {
    for (b = (const unsigned char *)ocultos[i]; *b; b++) h = (h ^ *b) * 1099511628211ULL;
    h = (h ^ 2u) * 1099511628211ULL;
  }
  return (h ^ (unsigned)nOcultos) * 1099511628211ULL;
}

// ---------------------------------------------------------------- leitura

// Fim de UM elemento de array. js_fim so sabe andar sobre objeto e array; a
// forma legada e um array de STRINGS, e sem isto js_prox nunca avancava — a
// ordem legada seria lida com um item so, em silencio.
static const char *fimElemento(const char *p) {
  if (*p == '"') {
    const char *q = p + 1;
    while (*q && *q != '"') { if (*q == '\\' && q[1]) q++; q++; }
    return *q ? q + 1 : q;
  }
  return js_fim(p);
}

static void minusculas(char *s) {
  for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

static void addOrdem(const char *chave) {
  int i;
  if (!chave || !chave[0]) return;
  for (i = 0; i < nOrdem; i++) if (!strcmp(ordem[i], chave)) return;
  if (nOrdem >= CATORD_MAX) { nAlem++; return; }
  snprintf(ordem[nOrdem++], CATORD_CHAVE, "%s", chave);
}

static void addOculto(const char *chave) {
  int i;
  if (!chave || !chave[0]) return;
  for (i = 0; i < nOcultos; i++) if (!strcmp(ocultos[i], chave)) return;
  if (nOcultos >= CATORD_MAX) { nAlem++; return; }
  snprintf(ocultos[nOcultos++], CATORD_DESL, "%s", chave);
}

// syncItemKey() do web: colecao vira `collection_<id>`, catalogo vira
// `<addonId>_<tipo>_<catalogoId>`. Sem as tres partes o item NAO tem
// identidade e e descartado — casar pela metade poria a fileira errada no
// lugar da que a pessoa escolheu.
static int chaveDoItem(const char *ini, const char *fim, char *dst, size_t tam) {
  char addon[96], tipo[32], cat[96], col[96], b[32];
  if (js_bruto(ini, fim, "is_collection", b, sizeof b) ||
      js_bruto(ini, fim, "isCollection", b, sizeof b)) {
    if (strstr(b, "true")) {
      col[0] = 0;
      if (!js_texto(ini, fim, "collection_id", col, sizeof col))
        js_texto(ini, fim, "collectionId", col, sizeof col);
      if (!col[0]) return 0;
      snprintf(dst, tam, "collection_%s", col);
      return 1;
    }
  }
  addon[0] = tipo[0] = cat[0] = 0;
  if (!js_texto(ini, fim, "addon_id", addon, sizeof addon))
    js_texto(ini, fim, "addonId", addon, sizeof addon);
  js_texto(ini, fim, "type", tipo, sizeof tipo);
  if (!js_texto(ini, fim, "catalog_id", cat, sizeof cat))
    js_texto(ini, fim, "catalogId", cat, sizeof cat);
  if (!addon[0] || !tipo[0] || !cat[0]) return 0;
  minusculas(tipo);
  snprintf(dst, tam, "%s_%s_%s", addon, tipo, cat);
  return 1;
}

// Formato moderno: `items: [{addon_id, type, catalog_id, enabled, order}]`.
// `order` manda, e nao a posicao no array — o web ordena por ele antes de
// gravar, mas nada garante que o servidor devolva ja ordenado.
static int lerItens(const char *ini, const char *fim) {
  typedef struct { char chave[CATORD_CHAVE]; int pos, ligado; } Ent;
  // Applied on the main thread, like the module state. Keep TV stacks small.
  static Ent v[CATORD_MAX];
  int nv = 0, i, j;
  const char *p = js_array(ini, fim, "items");
  if (!p) {
    char raw[32];
    if (!js_bruto(ini, fim, "items", raw, sizeof raw)) return 0;
    const char *q = raw;
    while (*q && (unsigned char)*q <= ' ') q++;
    if (*q++ != '[') return 0;
    while (*q && (unsigned char)*q <= ' ') q++;
    return *q == ']';
  }
  for (; p; p = js_prox(fimElemento(p))) {
    const char *f = fimElemento(p);
    char b[32];
    if (*p != '{') continue;
    if (nv >= CATORD_MAX) { nAlem++; continue; }
    if (!chaveDoItem(p, f, v[nv].chave, CATORD_CHAVE)) continue;
    v[nv].pos  = (int)js_num(p, f, "order", nv);
    // `enabled !== false`: ausente significa LIGADO. Tratar ausencia como
    // desligado esconderia toda fileira que o web nunca precisou marcar.
    v[nv].ligado = !(js_bruto(p, f, "enabled", b, sizeof b) && strstr(b, "false"));
    nv++;
  }
  if (!nv) return 0;
  for (i = 1; i < nv; i++) {           // Stable insertion sort over bounded account configuration
    int k = i;
    while (k > 0 && v[k - 1].pos > v[k].pos) {
      Ent t = v[k - 1]; v[k - 1] = v[k]; v[k] = t; k--;
    }
  }
  for (j = 0; j < nv; j++) {
    addOrdem(v[j].chave);
    if (!v[j].ligado) addOculto(v[j].chave);
  }
  return 1;
}

// Formato legado: arrays de chaves soltas, sob o PRIMEIRO nome que existir.
// Os cinco nomes nao sao invencao: e o que `firstStringArrayFromRaw` procura,
// nessa ordem, porque o blob mudou de nome tres vezes na vida do app web.
static int lerLista(const char *ini, const char *fim, const char *const *nomes,
                    int nNomes, void (*add)(const char *)) {
  int i, achou = 0;
  for (i = 0; i < nNomes && !achou; i++) {
    const char *p = js_array(ini, fim, nomes[i]);
    char bruto[64];
    // Presente porem VAZIO tambem conta como resposta: js_array devolve NULL
    // para `[]`, e sem esta conferencia o leitor cairia no nome seguinte.
    if (!p && js_bruto(ini, fim, nomes[i], bruto, sizeof bruto) && bruto[0] == '[')
      return 1;
    if (!p) continue;
    achou = 1;
    for (; p; p = js_prox(fimElemento(p))) {
      char chave[CATORD_DESL];
      const char *f = fimElemento(p);
      size_t n;
      if (*p != '"') continue;
      n = (size_t)(f - p) - 2;
      if (n + 1 > sizeof chave) continue;
      memcpy(chave, p + 1, n);
      chave[n] = 0;
      add(chave);
    }
  }
  return achou;
}

// O objeto de ajustes dentro da resposta. A RPC devolve `[{...}]`, o campo
// pode chamar `settings_json` ou `settingsJson`, e — como no web — a propria
// linha serve de blob quando nenhum dos dois existe.
static const char *blobDe(const char *resposta, const char **fimOut) {
  const char *p = js_raiz_array(resposta);
  const char *obj = p ? p : resposta;
  const char *fim;
  if (!obj) return NULL;
  while (*obj && (unsigned char)*obj <= ' ') obj++;
  if (*obj != '{') return NULL;
  fim = js_fim(obj);
  { const char *nomes[2] = { "\"settings_json\"", "\"settingsJson\"" };
    int i;
    for (i = 0; i < 2; i++) {
      const char *k = strstr(obj, nomes[i] );
      if (!k || k >= fim) continue;
      k = strchr(k, ':');
      if (!k) continue;
      k++;
      while (*k && (unsigned char)*k <= ' ') k++;
      if (*k != '{') continue;   // string serializada nao e aplicada pela metade
      *fimOut = js_fim(k);
      return k;
    }
  }
  *fimOut = fim;
  return obj;
}

int catordem_ler(const char *resposta) {
  unsigned long long antes;
  int nAntesOrdem, nAntesOcultos, mudou = 0;
  int nlAntes = temNLanc, vnlAntes = valNLanc;
  int suAntes = temSublinhado, vsuAntes = valSublinhado;
  int nlNovo = temNLanc, vnlNovo = valNLanc, suNovo = temSublinhado, vsuNovo = valSublinhado;
  const char *fim = NULL, *blob;
  static const char *NOMES_ORDEM[4] = {
    "catalog_order_keys", "home_catalog_order", "catalog_order", "order"
  };
  static const char *NOMES_OFF[5] = {
    "disabled_catalog_keys", "hidden_catalog_keys", "catalog_disabled_keys",
    "home_catalog_disabled", "disabled"
  };
  alvo = vivo;                    // so este fio troca `vivo`: ler sem trava
  antes = assinaturaLida();
  nAntesOrdem = nOrdem; nAntesOcultos = nOcultos;
  if (!resposta || !*resposta) { printf("[catordem] resposta vazia\n"); return 0; }
  blob = blobDe(resposta, &fim);
  // DIZ QUAL DOS CASOS E. Este ponto ja devolveu 0 em silencio uma vez, e do
  // lado de fora "a home nao obedeceu" ficava indistinguivel de "o servidor
  // nao tem a funcao".
  //
  // E separa o `[]` do resto: MEDIDO na conta real, a RPC responde 200 com
  // array VAZIO quando ninguem gravou ordem naquele perfil. Isso e o servidor
  // funcionando e a conta sem configuracao — chamar de "blob irreconhecivel"
  // mandaria procurar defeito onde nao ha. Os 80 primeiros bytes bastam para
  // reconhecer a forma das outras respostas sem despejar o blob inteiro no log
  // a cada ciclo de sync.
  if (!blob) {
    const char *q = resposta;
    while (*q && (unsigned char)*q <= ' ') q++;
    if (q[0] == '[' && q[1] == ']')
      printf("[catordem] a conta nao gravou ordem de catalogos neste perfil\n");
    else
      printf("[catordem] sem settings_json reconhecivel em: %.80s\n", resposta);
    return 0;
  }

  // Monta na bancada que NAO esta publicada; se nenhum formato responder, `vivo`
  // continua intacto e nada precisa ser desfeito.
  nAlem = 0;
  alvo = (vivo == &bancas[0]) ? &bancas[1] : &bancas[0];
  nOrdem = nOcultos = 0;

  if (!lerItens(blob, fim)) {
    int a = lerLista(blob, fim, NOMES_ORDEM, 4, addOrdem);
    int d = lerLista(blob, fim, NOMES_OFF, 5, addOculto);
    // Nenhum dos dois formatos respondeu: a conta nao tem ordem gravada. Nao e
    // ordem VAZIA — devolver a lista local para o comeco aqui apagaria a home.
    if (!a && !d) {
      printf("[catordem] a conta nao gravou ordem nem catalogos ocultos\n");
      alvo = vivo;
      return 0;
    }
  }

  { char b[16];
    if (js_bruto(blob, fim, "hide_unreleased_content", b, sizeof b)) {
      nlNovo = 1; vnlNovo = strstr(b, "true") != NULL;
    }
    if (js_bruto(blob, fim, "hide_catalog_underline", b, sizeof b)) {
      suNovo = 1; vsuNovo = strstr(b, "true") != NULL;
    } }

  if (nOrdem != nAntesOrdem || nOcultos != nAntesOcultos || assinaturaLida() != antes) mudou = 1;
  if (nlAntes != nlNovo || vnlAntes != vnlNovo ||
      suAntes != suNovo || vsuAntes != vsuNovo) mudou = 1;
  pthread_mutex_lock(&trava);
  vivo = alvo;
  temOrdem = nOrdem > 0 || nOcultos > 0;
  temNLanc = nlNovo; valNLanc = vnlNovo; temSublinhado = suNovo; valSublinhado = vsuNovo;
  if (mudou) revisao++;
  pthread_mutex_unlock(&trava);
  // Imprime TAMBEM quando nao mudou nada, e dizendo qual dos dois casos e. Uma
  // conta sem ordem configurada e uma leitura que falhou davam a mesma linha
  // (nenhuma), e "a home nao obedeceu" nao tem como ser respondido assim.
  if (mudou)
    printf("[catordem] %d na ordem da conta, %d desligadas\n", nOrdem, nOcultos);
  // O TETO NAO E MAIS MUDO (#255): o que passou dele perde posicao e o
  // "desligado" — um catalogo desligado no web pode voltar como fileira.
  if (nAlem)
    printf("[catordem] %d item(ns) da conta alem do teto de %d: ficam sem posicao e sem desligar\n",
           nAlem, CATORD_MAX);
  else if (!temOrdem)
    printf("[catordem] a conta nao tem ordem de catalogos gravada\n");
  return mudou;
}

// ---------------------------------------------------------------- consulta

static int lerInt(const int *p) {
  int v;
  pthread_mutex_lock(&trava); v = *p; pthread_mutex_unlock(&trava);
  return v;
}
int         catordem_tem_ordem(void) { return lerInt(&temOrdem); }
int         catordem_n(void) {
  int v;
  pthread_mutex_lock(&trava); v = vivo->no; pthread_mutex_unlock(&trava);
  return v;
}
const char *catordem_chave(int i) {
  const char *r = "";
  pthread_mutex_lock(&trava);
  if (i >= 0 && i < vivo->no) r = vivo->ord[i];
  pthread_mutex_unlock(&trava);
  return r;
}

int catordem_unir(const char *const *locais, int nLocais, int *saida, int max) {
  int n = 0, i, j;
  Banca *b;
  // Locais vem do registro de fileiras (FIL_MAX) ou dos catalogos declarados;
  // nao do tamanho da ordem da conta. Pilha fixa, como era (6 KB).
  char usado[6144];
  if (!locais || !saida || nLocais < 1) return 0;
  if (nLocais > (int)sizeof usado) nLocais = (int)sizeof usado;
  memset(usado, 0, (size_t)nLocais);
  pthread_mutex_lock(&trava);
  b = vivo;
  for (i = 0; i < b->no; i++)
    for (j = 0; j < nLocais && n < max; j++)
      if (!usado[j] && locais[j] && !strcmp(locais[j], b->ord[i])) {
        saida[n++] = j; usado[j] = 1; break;
      }
  pthread_mutex_unlock(&trava);
  // O QUE O REMOTO NAO CONHECE NAO SE PERDE: vai para o fim, na ordem em que
  // ja estava. Sem este laco, todo catalogo instalado depois de a ordem ter
  // sido gravada sumiria da home a cada sync.
  for (j = 0; j < nLocais && n < max; j++) if (!usado[j]) saida[n++] = j;
  return n;
}

int catordem_oculta(const char *chave, const char *chaveDesativar) {
  int i, r = 0;
  pthread_mutex_lock(&trava);
  for (i = 0; !r && i < vivo->nc; i++) {
    if (chave && chave[0] && !strcmp(vivo->ocu[i], chave)) r = 1;
    else if (chaveDesativar && chaveDesativar[0] && !strcmp(vivo->ocu[i], chaveDesativar)) r = 1;
  }
  pthread_mutex_unlock(&trava);
  return r;
}

int catordem_tem_ocultar_nao_lancados(void) { return lerInt(&temNLanc); }
int catordem_ocultar_nao_lancados(void)     { return lerInt(&valNLanc); }
int catordem_tem_ocultar_sublinhado(void)   { return lerInt(&temSublinhado); }
int catordem_ocultar_sublinhado(void)       { return lerInt(&valSublinhado); }

void catordem_esquecer(void) {
  pthread_mutex_lock(&trava);
  revisao++;
  temOrdem = 0;
  temNLanc = valNLanc = temSublinhado = valSublinhado = 0;
  memset(bancas, 0, sizeof bancas);   // zera as duas: n=0 e sem chave de quem saiu
  pthread_mutex_unlock(&trava);
}
