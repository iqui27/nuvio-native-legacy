#include "perfis.h"
#include "sessao.h"
#include "nuvem.h"
#include "dados.h"
#include "js.h"
#include "jsw.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARQ_ATIVO "perfil.txt"
#define ARQ_LISTA "perfis.txt"

static ContaPerfil lista[CONTA_PERFIL_MAX];
static int n;
static char dono[64];
static int ativo = 1;
// ESCOLHIDO E DE SESSAO, GRAVADO E DE DISCO — e a diferenca entre as duas e a
// mudanca de comportamento inteira desta tela.
//
// Antes so existia `escolhido`, e perfis_carregar_ativo() o ligava ao ler
// perfil.txt: a tela aparecia UMA VEZ POR INSTALACAO e nunca mais. Numa TV de
// sala isso significa que quem liga o aparelho herda em silencio o perfil de
// quem o desligou — e como `p_profile_id` vai em quase toda RPC, o app passa a
// ESCREVER progresso no perfil da outra pessoa sem nunca ter perguntado.
//
// Agora `escolhido` zera a cada arranque (a pergunta vale uma vez por sessao) e
// `gravado` guarda que existe uma resposta de ontem — que e o que permite ao
// Voltar dispensar a tela sem escolher nada, e o que faz o cursor nascer no
// perfil certo.
static int escolhido;      // 1 depois que o usuario decidiu NESTA sessao
static int gravado;        // 1 quando perfil.txt existia no arranque

static void lerDono(void) {
  char *r;
  int st = 0;
  if (dono[0]) return;
  r = sessao_rpc("get_sync_owner", "{}", &st);
  if (r && st >= 200 && st < 300) {
    // MEDIDO: a resposta e uma string JSON CRUA — "441bf572-…" — e nao um
    // objeto. js_texto nao serve aqui; o valor esta entre as aspas do corpo
    // inteiro.
    const char *a = strchr(r, '"');
    const char *b = a ? strchr(a + 1, '"') : NULL;
    if (a && b && b > a + 1 && (size_t)(b - a - 1) < sizeof dono) {
      memcpy(dono, a + 1, (size_t)(b - a - 1));
      dono[b - a - 1] = 0;
    }
  }
  free(r);
  if (!dono[0]) {
    // Sem o dono, a leitura da tabela de addons nao tem por quem filtrar. Cair
    // no `sub` do token e a aproximacao correta: numa conta que nao e
    // compartilhada os dois sao a mesma coisa.
    snprintf(dono, sizeof dono, "%s", sessao_usuario());
    printf("[perfis] get_sync_owner nao respondeu; usando o sub do token\n");
  }
}

// --- CACHE EM DISCO DA LISTA -------------------------------------------------
//
// Uma linha por perfil, campos separados por TAB:
//
//   indice \t temPin \t primario \t usaAddons \t corHex \t nome \t avatar \t fundo
//
// Formato de linha e nao JSON de proposito: e o mesmo estilo de perfil.txt e
// progresso.txt, nao precisa do leitor de JSON no caminho do arranque, e um
// arquivo truncado pela metade custa UMA linha, nao o arquivo inteiro.
//
// O TAB e o separador porque nenhum dos campos pode conte-lo: a gravacao troca
// tab e quebra de linha por espaco antes de escrever. Sem isso um nome de
// perfil com um tab dentro deslocaria todos os campos seguintes.
static void limpo(char *dst, size_t tam, const char *src) {
  size_t i = 0;
  if (!tam) return;
  for (; src && src[i] && i + 1 < tam; i++)
    dst[i] = (src[i] == '\t' || src[i] == '\n' || src[i] == '\r') ? ' ' : src[i];
  dst[i] = 0;
}

static void gravarCache(void) {
  // 8 perfis x (2 URLs de 300 + nome + cor + quatro numeros) cabe com folga.
  char buf[8192];
  size_t p = 0;
  int i;
  if (n <= 0) return;
  for (i = 0; i < n && p < sizeof buf; i++) {
    char nome[64], av[300], fu[300], cor[10];
    limpo(nome, sizeof nome, lista[i].nome);
    limpo(av, sizeof av, lista[i].avatarUrl);
    limpo(fu, sizeof fu, lista[i].fundoUrl);
    limpo(cor, sizeof cor, lista[i].corHex);
    p += (size_t)snprintf(buf + p, sizeof buf - p, "%d\t%d\t%d\t%d\t%s\t%s\t%s\t%s\n",
                          lista[i].indice, lista[i].temPin, lista[i].primario,
                          lista[i].usaAddonsDoPrimario, cor, nome, av, fu);
  }
  if (p >= sizeof buf) return;   // nao gravar um arquivo truncado
  dados_gravar(ARQ_LISTA, buf);
}

// Le um campo ate o proximo TAB ou fim de linha e avanca `*p`.
static void campo(const char **p, char *dst, size_t tam) {
  const char *s = *p;
  size_t i = 0;
  while (*s && *s != '\t' && *s != '\n') {
    if (i + 1 < tam) dst[i++] = *s;
    s++;
  }
  if (tam) dst[i] = 0;
  if (*s == '\t') s++;
  *p = s;
}

static void lerCache(void) {
  char *b = dados_ler(ARQ_LISTA);
  const char *p;
  int novos = 0;
  if (!b) return;
  memset(lista, 0, sizeof lista);
  for (p = b; *p && novos < CONTA_PERFIL_MAX; ) {
    char num[16];
    ContaPerfil *d = &lista[novos];
    campo(&p, num, sizeof num); d->indice = atoi(num);
    campo(&p, num, sizeof num); d->temPin = atoi(num);
    campo(&p, num, sizeof num); d->primario = atoi(num);
    campo(&p, num, sizeof num); d->usaAddonsDoPrimario = atoi(num);
    campo(&p, d->corHex, sizeof d->corHex);
    campo(&p, d->nome, sizeof d->nome);
    campo(&p, d->avatarUrl, sizeof d->avatarUrl);
    campo(&p, d->fundoUrl, sizeof d->fundoUrl);
    while (*p == '\n' || *p == '\r') p++;
    // Linha sem indice e lixo (arquivo de outra versao, escrita interrompida):
    // pular uma linha e melhor que desistir do arquivo inteiro.
    if (d->indice > 0) novos++;
    else memset(d, 0, sizeof *d);
  }
  free(b);
  if (novos > 0) {
    n = novos;
    printf("[perfis] %d perfil(is) do cache: a tela de escolha ja pode abrir\n", n);
  }
}

int perfis_puxar(void) {
  char *r;
  int st = 0;
  const char *p;

  lerDono();

  r = sessao_rpc("sync_pull_profiles", "{}", &st);
  if (!r || st < 200 || st >= 300) { free(r); return n; }

  // Lista vazia NAO apaga o que ja esta em memoria: e a mesma regra que o app
  // web aplica em toda superficie. Resposta vazia pode ser perfil errado, 401
  // mal tratado ou servidor fora do ar, e nenhum desses e "o usuario apagou os
  // perfis".
  { int novos = 0;
    ContaPerfil tmp[CONTA_PERFIL_MAX];
    memset(tmp, 0, sizeof tmp);
    for (p = js_raiz_array(r); p && novos < CONTA_PERFIL_MAX; p = js_prox(js_fim(p))) {
      const char *f = js_fim(p);
      double idx = js_num(p, f, "profile_index", 0);
      if (idx <= 0) idx = js_num(p, f, "id", 0);
      if (idx <= 0) continue;
      tmp[novos].indice = (int)idx;
      // "ContaPerfil %d" ficou aqui quando a struct Perfil virou ContaPerfil
      // para nao colidir com o trakt.h legado. O tipo mudou de nome; o que o
      // usuario le, nao.
      if (!js_texto(p, f, "name", tmp[novos].nome, sizeof tmp[novos].nome))
        snprintf(tmp[novos].nome, sizeof tmp[novos].nome, "Perfil %d", (int)idx);
      js_texto(p, f, "avatar_url", tmp[novos].avatarUrl, sizeof tmp[novos].avatarUrl);
      js_texto(p, f, "profile_background_url", tmp[novos].fundoUrl,
               sizeof tmp[novos].fundoUrl);
      if (!js_texto(p, f, "avatar_color_hex", tmp[novos].corHex, sizeof tmp[novos].corHex))
        snprintf(tmp[novos].corHex, sizeof tmp[novos].corHex, "#1E88E5");
      { char b[16];
        // Sem o campo, o perfil 1 e o primario — e a mesma regra do web.
        tmp[novos].primario = js_bruto(p, f, "is_primary", b, sizeof b)
                              ? (strcmp(b, "true") == 0) : ((int)idx == 1); }
      { char b[16];
        tmp[novos].usaAddonsDoPrimario =
          js_bruto(p, f, "uses_primary_plugins", b, sizeof b)
          ? (strcmp(b, "true") == 0) : 0; }
      novos++;
    }
    if (novos > 0) { memcpy(lista, tmp, sizeof lista); n = novos; }
  }
  free(r);

  // Travas: um perfil com PIN nao pode ser aberto so por estar na lista.
  r = sessao_rpc("sync_pull_profile_locks", "{}", &st);
  if (r && st >= 200 && st < 300) {
    for (p = js_raiz_array(r); p; p = js_prox(js_fim(p))) {
      const char *f = js_fim(p);
      int idx = (int)js_num(p, f, "profile_id", 0);
      char b[16];
      int i, travado;
      if (!idx) idx = (int)js_num(p, f, "profile_index", 0);
      // MEDIDO: esta RPC devolve UMA LINHA POR PERFIL, com `pin_enabled` false
      // quando nao ha PIN — nao e uma lista so dos travados. Marcar todo perfil
      // que aparece aqui trancava TODOS eles, e como nenhum tem PIN nenhuma
      // digitacao seria aceita: ninguem entraria na propria conta.
      travado = js_bruto(p, f, "pin_enabled", b, sizeof b)
                ? (strcmp(b, "true") == 0) : 0;
      for (i = 0; i < n; i++)
        if (lista[i].indice == idx) lista[i].temPin = travado;
    }
  }
  free(r);

  gravarCache();
  printf("[perfis] %d perfil(is), dono=%s, ativo=%d\n", n, dono, ativo);
  return n;
}

int           perfis_n(void)         { return n; }
const ContaPerfil *perfis_item(int i)     { return (i >= 0 && i < n) ? &lista[i] : NULL; }

const ContaPerfil *perfis_item_ativo(void) {
  int i;
  for (i = 0; i < n; i++) if (lista[i].indice == ativo) return &lista[i];
  return NULL;
}
const char   *perfis_dono(void)      { return dono; }
int           perfis_ativo(void)     { return ativo > 0 ? ativo : 1; }

// Addons NAO sao por perfil quando o perfil diz herdar os do primario.
//
// MEDIDO no app web (js/data/local/pluginStore.js:26):
//   return profile?.usesPrimaryPlugins && normalized !== "1" ? "1" : normalized;
// O nativo ignorava isso e pedia sempre profile_id=<indice>, entao um perfil
// com a marca voltava com ZERO addons — o relato "os perfis nao sincronizam os
// addons". O perfil 1 nunca e redirecionado: ele E a origem.
int perfis_ativo_addons(void) {
  const ContaPerfil *p = perfis_item_ativo();
  int a = perfis_ativo();
  return (p && p->usaAddonsDoPrimario && a != 1) ? 1 : a;
}

void perfis_carregar_ativo(void) {
  char *b = dados_ler(ARQ_ATIVO);
  if (b) {
    int v = atoi(b);
    // `gravado`, e NAO `escolhido`: ler o arquivo prova que alguem escolheu
    // ontem, nao que quem esta na frente da TV agora e a mesma pessoa.
    if (v > 0) { ativo = v; gravado = 1; }
    free(b);
  }
  lerCache();
}

void perfis_definir_ativo(int indice) {
  char linha[32];
  if (indice <= 0) return;
  ativo = indice;
  escolhido = 1;
  gravado = 1;
  snprintf(linha, sizeof linha, "%d\n", indice);
  dados_gravar(ARQ_ATIVO, linha);
  printf("[perfis] perfil ativo: %d\n", indice);
}

void perfis_manter_ativo(void) { escolhido = 1; }

int perfis_sem_escolha(void) {
  if (n <= 0) return 1;
  if (n == 1) return !lista[0].temPin;
  return 0;
}

int perfis_precisa_escolher(void) {
  return !escolhido && !perfis_sem_escolha();
}

int perfis_pode_dispensar(void) {
  const ContaPerfil *p;
  if (escolhido) return 1;
  if (!gravado) return 0;
  p = perfis_item_ativo();
  // Sem o perfil na lista nao da para saber se ele esta travado; recusar e a
  // resposta segura. Com ele, o Voltar so vale quando nao ha PIN — senao a
  // tecla de voltar seria a chave da fechadura.
  return p && !p->temPin;
}

int perfis_indice_sugerido(void) {
  int i;
  for (i = 0; i < n; i++) if (lista[i].indice == ativo) return i;
  return 0;
}

PerfilAcao perfis_acao(int i) {
  if (i < 0 || i >= n) return PERFIL_ACAO_NADA;
  return lista[i].temPin ? PERFIL_ACAO_PIN : PERFIL_ACAO_ENTRAR;
}

int perfis_verificar_pin(int indice, const char *pin) {
  Jsw w;
  char *r;
  int st = 0, ok = 0;
  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_ci(&w, "p_profile_id", indice);
  jsw_cs(&w, "p_pin", pin ? pin : "");
  jsw_obj_fim(&w);
  r = sessao_rpc("verify_profile_pin", jsw_texto_final(&w), &st);
  jsw_livre(&w);
  // TRES respostas e nao duas. "Nao consegui perguntar" nao e "voce errou": com
  // dois valores, uma TV sem rede acusava a pessoa de errar o PIN que ela
  // digitou certo — e a tela ate tinha o aviso de conexao, so que inalcancavel.
  if (!r || st < 200 || st >= 300) { free(r); return -1; }
  // A RPC devolve um booleano; aceitar so o HTTP 200 deixaria passar um PIN
  // errado, que responde 200 com `false`.
  ok = (strstr(r, "true") != NULL);
  free(r);
  return ok;
}

void perfis_esquecer(void) {
  memset(lista, 0, sizeof lista);
  n = 0;
  dono[0] = 0;
  ativo = 1;
  escolhido = 0;
  gravado = 0;
  dados_apagar(ARQ_ATIVO);
  // O cache da LISTA sai junto. Ele guarda os nomes das pessoas da conta
  // anterior e as URLs dos avatares delas — deixa-lo no aparelho faria a tela
  // de escolha da proxima conta abrir mostrando a familia da conta passada.
  dados_apagar(ARQ_LISTA);
  printf("[perfis] perfis esquecidos (saiu da conta)\n");
}
