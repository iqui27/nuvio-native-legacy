// Onde assistir — ver ondever.h para a forma geral.
#include "ondever.h"
#include "rede.h"
#include "js.h"
#include "dados.h"
#include "ajustes.h"
#include "perfilregras.h"
#include "descoberta.h"   /* desc_chave_tmdb_reserva */
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#ifdef __ANDROID__
#include "android.h"
#endif

#define TMDB "https://api.themoviedb.org/3"
// Uma semana: o pais muda quando a TV muda de casa, nao a cada arranque.
#define PAIS_VALIDADE_S (7 * 24 * 3600)

// --- pais ----------------------------------------------------------------------

static char paisRede[3];
static atomic_int paisOk;
static atomic_int iniciado;

static int paisValido(const char *p) {
  // XX = desconhecido e T1 = Tor na convencao da Cloudflare.
  return p && isupper((unsigned char)p[0]) && isupper((unsigned char)p[1]) &&
         !isalpha((unsigned char)p[2]) && strncmp(p, "XX", 2) && strncmp(p, "T1", 2);
}

void ondever_pais(char *out, unsigned capacity) {
  const char *idi, *region;
  if(!out || capacity<3) return;
  if(atomic_load(&paisOk)) { memcpy(out,paisRede,3); return; }
  // Caller-owned snapshot: no TLS (TPK 4.0 cannot load ELF TLS), no shared
  // mutable fallback buffer. This runs on the SDL thread when requesting.
  idi=ajustes_tmdb_idioma();region=idi ? strchr(idi,'-') : NULL;
  if(region && paisValido(region+1)) { out[0]=region[1];out[1]=region[2];out[2]=0; }
  else memcpy(out,"US",3);
}

static int paisDoTexto(const char *corpo, const char *marca, char *dst) {
  const char *p = corpo ? strstr(corpo, marca) : NULL;
  if (!p) return 0;
  p += strlen(marca);
  if (!paisValido(p)) return 0;
  dst[0] = p[0]; dst[1] = p[1]; dst[2] = 0;
  return 1;
}

static void *fioPais(void *u) {
  char *corpo, novo[3] = "";
  (void)u;
  corpo = dados_ler("pais.txt");
  if (corpo) {
    long quando = 0;
    char p[4] = "";
    if (sscanf(corpo, "%3s %ld", p, &quando) == 2 && paisValido(p) &&
        time(NULL) >= quando && time(NULL) - quando < PAIS_VALIDADE_S) {
      memcpy(paisRede, p, 3);
      atomic_store(&paisOk, 1);
      printf("[ondever] country=%s source=cache\n", paisRede);
      free(corpo);
      return NULL;
    }
    free(corpo);
  }
  // Cloudflare: "loc=BR" no meio de um texto chave=valor, sem chave de API.
  corpo = rede_baixar("https://www.cloudflare.com/cdn-cgi/trace", 8);
  if (!paisDoTexto(corpo, "\nloc=", novo)) {
    free(corpo);
    // Reserva, com CORS aberto (o .wgt roda num navegador): o corpo e so "BR".
    corpo = rede_baixar("https://ipapi.co/country/", 8);
    if (corpo && paisValido(corpo)) { novo[0] = corpo[0]; novo[1] = corpo[1]; novo[2] = 0; }
  }
  free(corpo);
  if (novo[0]) {
    char linha[32];
    memcpy(paisRede, novo, 3);
    atomic_store(&paisOk, 1);
    snprintf(linha, sizeof linha, "%s %ld\n", novo, (long)time(NULL));
    dados_gravar_leve("pais.txt", linha);
    printf("[ondever] country=%s source=network\n", novo);
  } else {
    printf("[ondever] country lookup failed; locale fallback will be used\n");
  }
  fflush(stdout);
  return NULL;
}

// --- chave de casamento servico <-> app ------------------------------------------

static void tirarSufixo(char *s, const char *suf) {
  size_t n = strlen(s), m = strlen(suf);
  if (n > m && !strcmp(s + n - m, suf)) s[n - m] = 0;
}

void ondever_chave(const char *nome, char *dst, unsigned tam) {
  static const struct { const char *de, *para; } ALIAS[] = {
    { "amazonprimevideo", "primevideo" }, { "amazonvideo", "primevideo" },
    { "hbomax", "max" }, { "appletvplus", "appletv" }, { "disney", "disneyplus" },
    { "clarotvplus", "clarotv" }, { "youtubepremium", "youtube" },
  };
  char t[96];
  unsigned o = 0;
  if (!dst || !tam) return;
  for (; nome && *nome && o + 5 < sizeof t; nome++) {
    unsigned char c = (unsigned char)*nome;
    if (c == '+') { memcpy(t + o, "plus", 4); o += 4; }
    else if (isalnum(c)) t[o++] = (char)tolower(c);
  }
  t[o] = 0;
  // As variantes viram o servico: "Netflix Standard with Ads", "Max Amazon
  // Channel" (o canal mora DENTRO do Prime Video, mas e o mesmo catalogo).
  tirarSufixo(t, "standardwithads"); tirarSufixo(t, "basicwithads");
  tirarSufixo(t, "withads");
  for (size_t k = 0; k < sizeof ALIAS / sizeof ALIAS[0]; k++)
    if (!strcmp(t, ALIAS[k].de)) { snprintf(t, sizeof t, "%s", ALIAS[k].para); break; }
  snprintf(dst, tam, "%s", t);
}

int ondever_casa(const char *servico, const char *app) {
  char a[96], b[96];
  ondever_chave(servico, a, sizeof a);
  ondever_chave(app, b, sizeof b);
  if (!a[0] || !b[0]) return 0;
  return !strcmp(a, b);
}

// --- lista do titulo ------------------------------------------------------------

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static char pedido[32];          // base do titulo pedido ("tt123", "tmdb:9")
static char publicado[32];       // base da lista em `itens`
static OndeVer itens[ONDEVER_MAX];
static int nItens;
static unsigned geracao;
static time_t pedidoEm;
static int consultando, falhou;

static void baseDe(const char *imdb, char *dst, size_t capacity) {
  const char *digits, *p;
  size_t length;
  if(!dst || !capacity) return;
  dst[0]=0;
  if(!imdb) return;
  digits=!strncmp(imdb,"tt",2) ? imdb+2 : !strncmp(imdb,"tmdb:",5) ? imdb+5 : NULL;
  if(!digits || !isdigit((unsigned char)*digits)) return;
  for(p=digits;isdigit((unsigned char)*p);p++) {}
  if(*p && *p!=':') return;
  length=(size_t)(p-imdb);
  if(length>=capacity) return;
  memcpy(dst,imdb,length);dst[length]=0;
}

typedef struct { char base[32], country[3]; int serie; long tmdb; unsigned ger; } Pedido;

// Junta os servicos de um array ("flatrate") da regiao, sem repetir.
static void juntar(const char *ini, const char *fim, const char *arr, int gratis,
                   OndeVer *l, int *n) {
  const char *p = js_array(ini, fim, arr);
  while (p && p < fim && *n < ONDEVER_MAX) {
    const char *f = js_fim(p);
    char nome[64] = "", logo[96] = "", ch[96], ch2[96];
    int k, ja = 0;
    if (!f || f > fim) break;
    js_texto(p, f, "provider_name", nome, sizeof nome);
    js_texto(p, f, "logo_path", logo, sizeof logo);
    ondever_chave(nome, ch, sizeof ch);
    for (k = 0; k < *n && !ja; k++) {
      ondever_chave(l[k].nome, ch2, sizeof ch2);
      ja = !strcmp(ch, ch2);
    }
    if (nome[0] && ch[0] && !ja) {
      OndeVer *o = &l[(*n)++];
      memset(o, 0, sizeof *o);
      snprintf(o->nome, sizeof o->nome, "%s", nome);
      if (logo[0]) snprintf(o->logo, sizeof o->logo, "https://image.tmdb.org/t/p/w92%s", logo);
      o->gratis = gratis;
    }
    p = js_prox(f);
  }
}

static const char *fimString(const char *p) {
  if(!p || *p++!='"') return NULL;
  while(*p) {
    if(*p=='\\' && p[1]) p+=2;
    else if(*p++=='"') return p;
  }
  return NULL;
}

static const char *objeto(const char *json, const char *key) {
  const char *p=json;
  if(!p) return NULL;
  while(*p && isspace((unsigned char)*p)) p++;
  if(*p++!='{') return NULL;
  while(*p) {
    char name[80];
    while(*p && (isspace((unsigned char)*p)||*p==',')) p++;
    if(*p!='"' || !js_cadeia(p,name,sizeof name)) return NULL;
    p=fimString(p);
    if(!p) return NULL;
    while(*p && isspace((unsigned char)*p)) p++;
    if(*p++!=':') return NULL;
    while(*p && isspace((unsigned char)*p)) p++;
    if(!strcmp(name,key)) break;
    if(*p=='{' || *p=='[') { p=js_fim(p); if(!p) return NULL; }
    else if(*p=='"') { p=fimString(p); if(!p) return NULL; }
    else { while(*p && *p!=',' && *p!='}')p++; }
    if(*p=='}') return NULL;
  }
  const char *end=p && *p=='{' ? js_fim(p) : NULL;
  return end && end>p && end[-1]=='}' ? p : NULL;
}
static const char *ondever_regiao(const char *json, const char *country) {
  if(!country || !paisValido(country)) return NULL;
  const char *results=objeto(json,"results");
  return results ? objeto(results,country) : NULL;
}
int ondever_extrair(const char *json, const char *country, OndeVer *out, int capacity) {
  OndeVer list[ONDEVER_MAX];
  const char *region=ondever_regiao(json,country);
  int count=0;
  if(!region || !out || capacity<1) return 0;
  const char *end=js_fim(region);
  juntar(region,end,"flatrate",0,list,&count);
  juntar(region,end,"free",1,list,&count);
  juntar(region,end,"ads",1,list,&count);
  if(count>capacity) count=capacity;
  memcpy(out,list,(size_t)count*sizeof *out);
  return count;
}

static void *fioLista(void *u) {
  Pedido *q = u;
  const char *chave = desc_chave_tmdb_reserva();
  char url[320];
  char *corpo;
  long id = q->tmdb;
  OndeVer l[ONDEVER_MAX];
  int n = 0, ok=0;
  if (!chave || !chave[0]) goto fim;
  if (id <= 0 && !strncmp(q->base, "tmdb:", 5)) id = atol(q->base + 5);
  if (id <= 0) {
    snprintf(url, sizeof url, "%s/find/%s?api_key=%s&external_source=imdb_id",
             TMDB, q->base, chave);
    corpo = rede_baixar(url, 12);
    if (corpo) {
      const char *v = js_array(corpo, NULL, q->serie ? "tv_results" : "movie_results");
      if (v) id = (long)js_num(v, js_fim(v), "id", 0.0);
      free(corpo);
    }
  }
  if (id <= 0) goto fim;
  snprintf(url, sizeof url, "%s/%s/%ld/watch/providers?api_key=%s",
           TMDB, q->serie ? "tv" : "movie", id, chave);
  corpo = rede_baixar(url, 12);
  if (corpo) {
    const char *region = ondever_regiao(corpo, q->country);
    ok=objeto(corpo,"results") != NULL;
    if(region) n=ondever_extrair(corpo,q->country,l,ONDEVER_MAX);
    free(corpo);
  }
fim:
  pthread_mutex_lock(&trava);
  if (q->ger == geracao) {
    memcpy(itens, l, sizeof(OndeVer) * (size_t)n);
    nItens = n; consultando=0; falhou=!ok;
    snprintf(publicado, sizeof publicado, "%s", q->base);
  }
  pthread_mutex_unlock(&trava);
  printf("[ondever] title=%s providers=%d country=%s\n", q->base, n, q->country);
  fflush(stdout);
  free(q);
  return NULL;
}

void ondever_pedir(const char *imdb, int serie, long tmdbId) {
  char base[32];
  Pedido *q;
  unsigned version;
  pthread_t t;
  baseDe(imdb, base, sizeof base);
  if (!base[0]) return;
  pthread_mutex_lock(&trava);
  if (!strcmp(base, pedido) && time(NULL)-pedidoEm < 300) { pthread_mutex_unlock(&trava); return; }
  snprintf(pedido, sizeof pedido, "%s", base);
  pedidoEm=time(NULL); consultando=1; falhou=0; version=++geracao;
  q = calloc(1, sizeof *q);
  if (q) {
    snprintf(q->base, sizeof q->base, "%s", base);
    ondever_pais(q->country,sizeof q->country);
    q->serie = serie; q->tmdb = tmdbId; q->ger = version;
  }
  pthread_mutex_unlock(&trava);
  if (!q) { pthread_mutex_lock(&trava); consultando=0; falhou=1; pedido[0]=0; pthread_mutex_unlock(&trava); return; }
  if (pthread_create(&t, NULL, fioLista, q) == 0) pthread_detach(t);
  else { free(q); pthread_mutex_lock(&trava); pedido[0] = 0; consultando=0; falhou=1; pthread_mutex_unlock(&trava); }
}

int ondever_status(const char *id) {
  char base[32]; int state;
  baseDe(id,base,sizeof base);
  pthread_mutex_lock(&trava);
  state = !base[0] || strcmp(base,pedido) ? ONDE_SEM_PEDIDO
        : consultando ? ONDE_BUSCANDO : falhou ? ONDE_FALHOU : ONDE_PRONTO;
  pthread_mutex_unlock(&trava);
  return state;
}

int ondever_n(const char *imdb) {
  char base[32];
  int n;
  baseDe(imdb, base, sizeof base);
  if (!base[0]) return 0;
  pthread_mutex_lock(&trava);
  n = !strcmp(base, publicado) ? nItens : 0;
  pthread_mutex_unlock(&trava);
  return n;
}

int ondever_item(const char *imdb, int i, OndeVer *dst) {
  char base[32];
  int ok;
  baseDe(imdb, base, sizeof base);
  pthread_mutex_lock(&trava);
  ok = base[0] && !strcmp(base, publicado) && i >= 0 && i < nItens;
  if (ok && dst) *dst = itens[i];
  pthread_mutex_unlock(&trava);
  return ok;
}

#ifdef NV_SHOT_HOOKS
// Capturas: a lista de um titulo sem rede (tests/player_glass_shot.c).
void ondever_shot(const char *imdb, const OndeVer *l, int n) {
  char base[32];
  baseDe(imdb, base, sizeof base);
  if (n > ONDEVER_MAX) n = ONDEVER_MAX;
  pthread_mutex_lock(&trava);
  snprintf(publicado, sizeof publicado, "%s", base);
  snprintf(pedido, sizeof pedido, "%s", base);
  for (int i = 0; i < n; i++) itens[i] = l[i];
  nItens = n; consultando = 0; falhou = 0;
  pthread_mutex_unlock(&trava);
}
#endif

// --- apps instalados -------------------------------------------------------------

#define APPS_MAX 160
static pthread_mutex_t appsTrava = PTHREAD_MUTEX_INITIALIZER;
typedef struct { char id[96], nome[64]; } AppInstalado;
static AppInstalado apps[APPS_MAX];
static int nApps;

void ondever_apps_limpar(void) {
  pthread_mutex_lock(&appsTrava); nApps = 0; pthread_mutex_unlock(&appsTrava);
}
void ondever_app_visto(const char *id, const char *nome) {
  if (!id || !id[0] || !nome || !nome[0]) return;
  for(const char *p=id;*p;p++)
    if(!isalnum((unsigned char)*p) && *p!='.' && *p!='_' && *p!='-') return;
  pthread_mutex_lock(&appsTrava);
  if (nApps < APPS_MAX) {
    snprintf(apps[nApps].id, sizeof apps[nApps].id, "%s", id);
    snprintf(apps[nApps].nome, sizeof apps[nApps].nome, "%s", nome);
    nApps++;
  }
  pthread_mutex_unlock(&appsTrava);
}

// Id do app instalado que casa com o servico; 0 quando nao ha.
static int appDo(const char *servico, char *id, size_t tam) {
  int k, ok = 0;
  pthread_mutex_lock(&appsTrava);
  for (k = 0; k < nApps && !ok; k++)
    if (ondever_casa(servico, apps[k].nome)) { snprintf(id, tam, "%s", apps[k].id); ok = 1; }
  pthread_mutex_unlock(&appsTrava);
  return ok;
}

// ID NA LOJA de cada servico, para "baixar" quando nao esta instalado. Nao ha
// API que diga isso; quando falta aqui, a linha manda procurar pelo nome.
//   LG: conferidos no listApps da OLED65C9 do dono (02/10/2026), menos Disney+
//       (nao instalado la — e o id publico do app).
//   Samsung: ids publicos dos apps Tizen (os mesmos que controle remoto por
//       rede usa para abrir cada app). NAO conferidos numa Samsung nossa.
//   Android: pacotes conferidos no pm list packages da TCL de teste (02/10).
//       Sem pacote, a Play Store abre na busca pelo nome.
static const struct { const char *chave, *lg, *samsung, *android; } LOJA[] = {
  { "netflix",    "netflix",                    "3201907018807", "com.netflix.ninja" },
  { "primevideo", "amazon",                     "3201910019365", "com.amazon.amazonvideo.livingroom" },
  { "max",        "com.wbd.stream",             "3201601007230", "com.wbd.stream" },
  { "disneyplus", "com.disney.disneyplus-prod", "3201901017640", "com.disney.disneyplus" },
  { "appletv",    "com.apple.appletv",          "3201807016597", "com.apple.atve.androidtv.appletv" },
  { "globoplay",  "globoplaywebos",             "",              "com.globo.globotv" },
  { "clarotv",    "br.com.claro-now",           "",              "br.com.claro.now.smarttvclient" },
  { "youtube",    "youtube.leanback.v4",        "111299001912",  "com.google.android.youtube.tv" },
};
static const char *idNaLoja(const char *servico) {
  char ch[96];
  ondever_chave(servico, ch, sizeof ch);
  for (size_t k = 0; k < sizeof LOJA / sizeof LOJA[0]; k++)
    if (!strcmp(ch, LOJA[k].chave)) {
#if defined(__ANDROID__)
      return LOJA[k].android;
#elif defined(__EMSCRIPTEN__) || defined(NV_TPK)
      return LOJA[k].samsung;
#else
      return LOJA[k].lg;
#endif
    }
  return "";
}

#if defined(NV_TPK)
// Ponte com o host .NET (tizen-tpk/Apps.cs): C pede, C# responde chamando
// nv_tpk_app() por app instalado (de outro fio — por isso a trava acima).
typedef void (*FnSemArg)(void);
typedef void (*FnTexto)(const char *);
static FnSemArg tpkListar;
static FnTexto tpkAbrir, tpkLoja;
// visibility("default") e OBRIGATORIO: tools/tpk.sh compila tudo com
// -fvisibility=hidden, e sem isto o simbolo nao sai da .so — o host .NET
// (Apps.cs) levava EntryPointNotFoundException em "apps-init" (8 TVs na 1.7.4).
__attribute__((visibility("default")))
void nv_tpk_apps_registrar(FnSemArg listar, FnTexto abrir, FnTexto loja) {
  tpkListar = listar; tpkAbrir = abrir; tpkLoja = loja;
}
__attribute__((visibility("default")))
void nv_tpk_app(const char *id, const char *nome) { ondever_app_visto(id, nome); }
#endif

#if defined(__EMSCRIPTEN__)
// "id\tnome" por linha, como o .wgt e o Android devolvem. Consome `l`.
static void lerListaTab(char *l) {
  char *s = l, *nl;
  int n = 0;
  if (!l) return;
  ondever_apps_limpar();
  for (; s && *s; s = nl ? nl + 1 : NULL) {
    char *tab;
    nl = strchr(s, '\n');
    if (nl) *nl = 0;
    tab = strchr(s, '\t');
    if (tab) { *tab = 0; ondever_app_visto(s, tab + 1); n++; }
  }
  free(l);
  printf("[ondever] installed apps=%d\n", n);
}
#endif

#if !defined(__EMSCRIPTEN__) && !defined(NV_TPK) && !defined(__APPLE__) && !defined(NV_LINUX_DESKTOP) && !defined(__ANDROID__)
#define ONDE_WEBOS 1
#include "video.h"
// webOS: pelo barramento LS2 do player (video_luna), e nao por luna-send — o
// app roda como usuario comum no jail e luna-send da "Permission denied"
// (medido na C9, 02/10). As respostas chegam no fio do laco do glib.
static atomic_int appsLgOk;   // 1 = o listApps respondeu com a lista

static int retornoOk(const char *payload) {
  const char *p = payload ? strstr(payload, "\"returnValue\"") : NULL;
  if (!p) return 0;
  p = strchr(p, ':');
  while (p && (*p == ':' || *p == ' ')) p++;
  return p && !strncmp(p, "true", 4);
}

static void respostaApps(const char *payload, void *u) {
  const char *p = payload ? js_array(payload, NULL, "apps") : NULL;
  int vistos = 0;
  (void)u;
  if (!p) {
    printf("[ondever] listApps response unavailable\n");
    fflush(stdout);
    return;
  }
  ondever_apps_limpar();
  while (p) {
    const char *fim = js_fim(p);
    char id[96] = "", nome[64] = "";
    if (!fim) break;
    js_texto_raiz_em(p, fim, "id", id, sizeof id);
    js_texto_raiz_em(p, fim, "title", nome, sizeof nome);
    ondever_app_visto(id, nome);
    vistos++;
    p = js_prox(fim);
  }
  atomic_store(&appsLgOk, 1);
  printf("[ondever] installed apps=%d\n", vistos);
  fflush(stdout);
}

// A LG Content Store repassa params.query a pagina dela;
// "category/GAME_APPS/<id>" e a pagina de detalhe do app. Sem id, a vitrine.
static void lojaLg(const char *id) {
  char carga[300];
  if (id && id[0])
    snprintf(carga, sizeof carga,
             "{\"id\":\"com.webos.app.discovery\",\"params\":{\"query\":\"category/GAME_APPS/%s\"}}", id);
  else snprintf(carga, sizeof carga, "{\"id\":\"com.webos.app.discovery\"}");
  video_luna("luna://com.webos.applicationManager/launch", carga, NULL, NULL);
}

// `u` = id na loja quando o app foi aberto PELA TABELA, sem lista de
// instalados: se a TV disser que nao tem, vai para a pagina dele na loja.
static void respostaLaunch(const char *payload, void *u) {
  printf("[ondever] launch success=%d\n", retornoOk(payload));
  fflush(stdout);
  if (u && !retornoOk(payload)) lojaLg((const char *)u);
}
#endif

#ifdef __ANDROID__
// One detached query at a time. Repeated sheet openings coalesce while the
// PackageManager is busy; readers retain the complete previous snapshot.
static int appsAndroidBuscando, appsAndroidPronto;
static void *fioAppsAndroid(void *unused) {
  struct timespec inicio, fim;
  clock_gettime(CLOCK_MONOTONIC, &inicio);
  AppInstalado novo[APPS_MAX];
  char *lista = android_listar_apps(), *linha = lista;
  int count = 0;
  (void)unused;
  while (linha && *linha && count < APPS_MAX) {
    char *nl = strchr(linha, '\n'), *tab;
    if (nl) *nl = 0;
    tab = strchr(linha, '\t');
    if (tab) {
      int valido = 1;
      *tab++ = 0;
      for (const char *p = linha; *p; p++)
        if (!isalnum((unsigned char)*p) && *p != '.' && *p != '_' && *p != '-') valido = 0;
      if (valido && linha[0] && tab[0]) {
        snprintf(novo[count].id, sizeof novo[count].id, "%s", linha);
        snprintf(novo[count].nome, sizeof novo[count].nome, "%s", tab);
        count++;
      }
    }
    linha = nl ? nl + 1 : NULL;
  }
  pthread_mutex_lock(&appsTrava);
  if (lista) {
    memcpy(apps, novo, (size_t)count * sizeof *apps);
    nApps = count; appsAndroidPronto = 1;
  }
  appsAndroidBuscando = 0;
  pthread_mutex_unlock(&appsTrava);
  clock_gettime(CLOCK_MONOTONIC, &fim);
  long ms = (fim.tv_sec - inicio.tv_sec) * 1000 + (fim.tv_nsec - inicio.tv_nsec) / 1000000;
  printf("[ondever] installed apps query %s count=%d elapsed_ms=%ld\n", lista ? "completed" : "failed", count, ms);
  fflush(stdout);
  free(lista);
  return NULL;
}
#endif

void ondever_apps_atualizar(void) {
#if defined(__EMSCRIPTEN__)
  // O .wgt pede ao tizen.application (assincrono) e le o que a resposta
  // ANTERIOR deixou em Module.nvApps ("id\tnome\n..."): a lista e de ha
  // segundos, e a seguinte chega para o proximo pedido.
  char *l = (char *)EM_ASM_PTR({
    try {
      if (typeof tizen !== 'undefined' && tizen.application) {
        tizen.application.getAppsInfo(function(v) {
          Module.nvApps = v.map(function(a) { return a.id + '\t' + a.name; }).join('\n');
        }, function() {});
      }
      var t = Module.nvApps || "";
      if (!t) return 0;
      var b = new TextEncoder().encode(t);
      var p = _malloc(b.length + 1);
      if (!p) return 0;
      HEAPU8.set(b, p); HEAPU8[p + b.length] = 0;
      return p;
    } catch (e) { return 0; }
  });
  lerListaTab(l);
#elif defined(__ANDROID__)
  pthread_t fio;
  pthread_mutex_lock(&appsTrava);
  if (!appsAndroidBuscando) {
    appsAndroidBuscando = 1;
    if (!pthread_create(&fio, NULL, fioAppsAndroid, NULL)) {
      pthread_detach(fio);
      printf("[ondever] installed apps query dispatched\n");
    } else {
      appsAndroidBuscando = 0;
      printf("[ondever] installed apps query dispatch failed\n");
    }
    fflush(stdout);
  }
  pthread_mutex_unlock(&appsTrava);
#elif defined(NV_TPK)
  if (tpkListar) { ondever_apps_limpar(); tpkListar(); }
#elif defined(ONDE_WEBOS)
  video_luna("luna://com.webos.applicationManager/listApps", "{}", respostaApps, NULL);
#endif
}

int ondever_estado(const char *nome) {
  char id[96], key[96];
  ondever_chave(nome,key,sizeof key);
  // Channel subscriptions belong to the host service, not the standalone
  // app. Keep availability information without an inaccurate launch/store action.
  if(strstr(key,"amazonchannel") || strstr(key,"appletvchannel")) return ONDE_INFO;
#if defined(__ANDROID__)
  pthread_mutex_lock(&appsTrava);
  int pronto = appsAndroidPronto;
  pthread_mutex_unlock(&appsTrava);
  if (!pronto) return ONDE_INFO;
#endif
#if (defined(__APPLE__) || defined(NV_LINUX_DESKTOP)) && !defined(NV_TPK) && !defined(__ANDROID__)
  (void)id; (void)nome;
  return ONDE_INFO;         // desktop preview has no TV application launcher
#else
#if defined(NV_TPK)
  if (!tpkAbrir) return ONDE_INFO;   // host antigo, sem a ponte
#endif
  if (appDo(nome, id, sizeof id)) return ONDE_ABRIR;
#if defined(ONDE_WEBOS)
  // Sem a lista (listApps recusado ou ainda a caminho): tenta abrir pelo id
  // da tabela, e respostaLaunch cai na loja se a TV nao tiver o app.
  if (!atomic_load(&appsLgOk) && idNaLoja(nome)[0]) return ONDE_ABRIR;
#endif
  return idNaLoja(nome)[0] ? ONDE_LOJA : ONDE_PROCURAR;
#endif
}

int ondever_abrir(const char *nome) {
  if(perfil_regra_compras())return ONDE_INFO;
  int e = ondever_estado(nome);
  char id[96] = "";
  const char *loja = idNaLoja(nome);
  if (e == ONDE_ABRIR) appDo(nome, id, sizeof id);
  printf("[ondever] %s: %s %s\n", nome,
         e == ONDE_ABRIR ? "launch" : e == ONDE_LOJA ? "store" : e == ONDE_PROCURAR ? "search-store" : "information",
         e == ONDE_ABRIR ? id : loja);
  fflush(stdout);
  if (e == ONDE_INFO) return ONDE_INFO;
#if defined(__ANDROID__)
  pthread_mutex_lock(&appsTrava);
  int pronto = appsAndroidPronto;
  pthread_mutex_unlock(&appsTrava);
  if (!pronto) return ONDE_INFO;
#endif
#if (defined(__APPLE__) || defined(NV_LINUX_DESKTOP)) && !defined(NV_TPK) && !defined(__ANDROID__)
  return ONDE_INFO;
#elif defined(__EMSCRIPTEN__)
  if (e == ONDE_ABRIR) {
    EM_ASM({ try { tizen.application.launch(UTF8ToString($0)); } catch (e) { console.log("[ondever] " + e); } }, id);
  } else {
    // A loja da Samsung (org.volt.apps) abre na pagina do app com Sub_Menu
    // "detail"; sem id, abre na vitrine e a folha diz o que procurar.
    // Sem virgula solta no JS: EM_ASM e macro, e a virgula fora de parenteses
    // vira outro argumento dela.
    EM_ASM({
      try {
        var id = UTF8ToString($0);
        var dados = [];
        if (id) {
          dados.push(new tizen.ApplicationControlData("Sub_Menu", ["detail"]));
          dados.push(new tizen.ApplicationControlData("widget_id", [id]));
        }
        var ctl = new tizen.ApplicationControl("http://tizen.org/appcontrol/operation/default");
        ctl.data = dados;
        tizen.application.launchAppControl(ctl, "org.volt.apps", null, null);
      } catch (e) { console.log("[ondever] store " + e); }
    }, e == ONDE_LOJA ? loja : "");
  }
#elif defined(__ANDROID__)
  if (e == ONDE_INFO) return ONDE_INFO;
  if (e == ONDE_ABRIR) {
    if(!android_abrir_app(id)) return ONDE_INFO;
  } else if(!android_abrir_loja(e == ONDE_LOJA ? loja : "", nome)) return ONDE_INFO;
#elif defined(NV_TPK)
  if (e == ONDE_ABRIR && tpkAbrir) tpkAbrir(id);
  else if (e != ONDE_INFO && tpkLoja) tpkLoja(e == ONDE_LOJA ? loja : "");
#elif defined(ONDE_WEBOS)
  if (e == ONDE_ABRIR) {
    char carga[200];
    int pelaTabela = !id[0];
    snprintf(carga, sizeof carga, "{\"id\":\"%s\"}", pelaTabela ? loja : id);
    video_luna("luna://com.webos.applicationManager/launch", carga, respostaLaunch,
               pelaTabela ? (void *)loja : NULL);
  } else lojaLg(e == ONDE_LOJA ? loja : "");
#endif
  return e;
}

void ondever_iniciar(void) {
  pthread_t t;
  if(atomic_exchange(&iniciado,1)) return;
  if (pthread_create(&t, NULL, fioPais, NULL) == 0) pthread_detach(t);
#if !defined(ONDE_WEBOS)
  ondever_apps_atualizar();
#endif
  // webOS: a lista sai na primeira folha (stream_folha_abrir), e nao aqui —
  // pedir no arranque registraria o barramento do player antes da hora.
}
