#include "homekitapp.h"
#include "addons.h"
#include "colecoes.h"
#include "dados.h"
#include "perfis.h"
#include "rede.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 0 livre, 1 baixando, 3 fio terminou (falta recolher).
static pthread_t fio;
static int fioVivo;
static _Atomic int estado;
static char urlPedida[NV_HK_URL_MAX];
static char *corpo;
static int perfilDoPedido;

// -1 nada a dizer; HkResultado; 100 sem resposta; 101 endereco invalido.
static int resultado = -1;
static int ignorados;
static int temKit;
static char nomeKit[96];
static int perfilLido = -1;
static HkKit kit;   // ~340 KB: estatico, nunca na pilha

static void arquivo(char *dst, unsigned n) { snprintf(dst, n, "kit-home-p%d.json", perfis_ativo()); }

static int addonInstalado(const char *id) {
  for (int i = 0, n = addons_n(); i < n; i++)
    if (!strcmp(addons_id_manifesto(i), id)) return 1;
  return 0;
}

static void limparHome(void) {
  char g[32];
  for (int i = 0; i < HK_GRUPOS_MAX; i++) { hk_grupo_id(i, g, sizeof g); col_extra_definir_grupo(g, NULL, 0); }
}

// `kit` ja validado -> pastas extras, um grupo (uma fileira) por colecao.
static void porNaHome(void) {
  char g[32];
  for (int gi = 0; gi < HK_GRUPOS_MAX; gi++) {
    int ini = -1, n = 0;
    for (int i = 0; i < kit.nPastas; i++)
      if (kit.grupoDe[i] == gi) { if (ini < 0) ini = i; n++; }
    hk_grupo_id(gi, g, sizeof g);
    // grupoDe e crescente (homekit.h): as pastas do grupo sao contiguas.
    col_extra_definir_grupo(g, n ? &kit.pastas[ini] : NULL, n);
  }
  temKit = 1;
  snprintf(nomeKit, sizeof nomeKit, "%s", kit.nome[0] ? kit.nome : "Kit de Home");
}

static void esquecer(void) {
  if (temKit) limparHome();
  temKit = 0;
  nomeKit[0] = 0;
}

static void lerDoPerfil(void) {
  char nome[48], *j;
  esquecer();
  resultado = -1;
  ignorados = 0;
  perfilLido = perfis_ativo();
  arquivo(nome, sizeof nome);
  j = dados_ler(nome);
  if (!j) return;
  // O arquivo e o que ESTE app ja aceitou: sem filtro de addon instalado (a
  // lista de addons pode nem ter chegado ainda; fonte de addon que sumiu fica
  // vazia na pasta, como em qualquer colecao da conta).
  if (hk_ler(j, &kit, NULL) == HK_OK) {
    porNaHome();
    printf("[kit] kit de Home do perfil %d: %d pastas em %d fileiras\n", perfilLido, kit.nPastas, kit.nGrupos);
  } else printf("[kit] arquivo do kit de Home ilegivel: ignorado\n");
  free(j);
}

void homekit_iniciar(void) {
  lerDoPerfil();
#ifdef NV_LINUX_DESKTOP
  // So no preview de mesa: importar sem digitar o endereco no teclado da TV.
  //   NUVIO_KIT_URL=http://127.0.0.1:8000/kit.json build/linux/nuvio-native-legacy ...
  { const char *u = getenv("NUVIO_KIT_URL"); if (u && *u) homekit_importar(u); }
#endif
}

HkResultado homekit_aplicar_json(const char *json) {
  // Valida num kit a parte: um JSON ruim nao derruba o kit que esta na Home.
  static HkKit novo;
  char nome[48], *s;
  HkResultado r = hk_ler(json, &novo, addonInstalado);
  ignorados = novo.pulAddon + novo.pulFonte + novo.pulPasta + novo.pulArte;
  if (r != HK_OK) return r;
  s = hk_serializar(&novo);
  arquivo(nome, sizeof nome);
  if (!s || !dados_gravar(nome, s)) printf("[kit] nao deu para guardar o kit: vale so ate fechar o app\n");
  // Relido do proprio texto guardado: `kit` fica com os ponteiros dele mesmo.
  if (!s || hk_ler(s, &kit, NULL) != HK_OK) { free(s); return HK_ERR_JSON; }
  free(s);
  esquecer();
  porNaHome();
  printf("[kit] kit de Home importado: %d pastas em %d fileiras; fora: %d addon, %d fonte, %d pasta, %d arte\n",
         kit.nPastas, kit.nGrupos, novo.pulAddon, novo.pulFonte, novo.pulPasta, novo.pulArte);
  return HK_OK;
}

void homekit_remover(void) {
  char nome[48];
  arquivo(nome, sizeof nome);
  dados_apagar(nome);
  esquecer();
  resultado = -1;
  ignorados = 0;
}

static void *baixarFio(void *u) {
  // O servidor do kit e um desconhecido: o download para um byte alem do teto,
  // e esse byte a mais faz o hk_ler recusar pelo tamanho. Mesmo caminho do
  // rede_baixar (inclusive a reserva sem libcurl), so que com teto.
  RedeControle c = { (long)HK_CORPO_MAX + 1, NULL };
  RedeMedida m;
  (void)u;
  memset(&m, 0, sizeof m);
  corpo = rede_baixar_medido_controle(urlPedida, 20, NULL, &c, &m);
  // Pagina de erro que por acaso e um kit valido nao troca o kit em uso.
  if (corpo && m.status >= 400) { free(corpo); corpo = NULL; }
  atomic_store_explicit(&estado, 3, memory_order_release);
  return NULL;
}

void homekit_importar(const char *url) {
  char t[NV_HK_URL_MAX];
  size_t n;
  if (fioVivo) return;
  while (url && (*url == ' ' || *url == '\t')) url++;
  n = url ? strlen(url) : 0;
  while (n && (url[n - 1] == ' ' || url[n - 1] == '\t' || url[n - 1] == '\n' || url[n - 1] == '\r')) n--;
  if (!n) { homekit_remover(); return; }
  // Nunca um endereco cortado: o que nao cabe e recusado.
  if (n >= sizeof t) { resultado = 101; return; }
  memcpy(t, url, n); t[n] = 0;
  if (!hk_url_valida(t)) { resultado = 101; return; }
  memcpy(urlPedida, t, n + 1);
  perfilDoPedido = perfis_ativo();
  resultado = -1;
  atomic_store_explicit(&estado, 1, memory_order_release);
  if (pthread_create(&fio, NULL, baixarFio, NULL) != 0) {
    resultado = 100;
    atomic_store_explicit(&estado, 0, memory_order_release);
    return;
  }
  fioVivo = 1;
}

void homekit_passo(void) {
  if (fioVivo && atomic_load_explicit(&estado, memory_order_acquire) == 3) {
    pthread_join(fio, NULL);
    fioVivo = 0;
    atomic_store_explicit(&estado, 0, memory_order_release);
    // O perfil trocou no meio do download: o kit era para o outro, some.
    if (perfilDoPedido == perfis_ativo()) {
      resultado = corpo ? (int)homekit_aplicar_json(corpo) : 100;
      if (resultado != HK_OK)
        printf("[kit] importacao recusada (%d): %s\n", resultado, rede_url_publica(urlPedida, (char[160]){0}, 160));
    }
    free(corpo); corpo = NULL;
  }
  if (perfilLido != perfis_ativo()) lerDoPerfil();
}

int homekit_tem(void) { return temKit; }
const char *homekit_nome(void) { return nomeKit; }
int homekit_ignorados(void) { return ignorados; }
int homekit_baixando(void) { return atomic_load_explicit(&estado, memory_order_acquire) != 0; }

const char *homekit_estado_texto(void) {
  if (homekit_baixando()) return "baixando…";
  switch (resultado) {
    case HK_ERR_JSON:   return "isso não é um kit de Home";
    case HK_ERR_VERSAO: return "kit feito para uma versão mais nova do app";
    case HK_ERR_VAZIO:  return "nenhuma fileira do kit serve aqui";
    case HK_ERR_GRANDE: return "isso não é um kit de Home";
    case 100:           return "sem resposta do servidor";
    case 101:           return "o endereço precisa começar com http";
    default:            return NULL;
  }
}
