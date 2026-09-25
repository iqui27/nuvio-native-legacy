// Diagnostico sob demanda dos addons e do caminho de artes, e o OTIMIZADOR que
// aplica um perfil so depois de provar que ele nao piora.
//
// Este modulo mede o que esta sob controle do Nuvio e deixa claro quando um
// defeito esta no servidor externo. Nenhuma URL completa, token ou titulo
// pessoal entra no relatorio enviado.
//
// O FLUXO, na ordem em que acontece:
//   1. fio do diagnostico: manifestos, catalogos, fontes de video e os assets
//      dos manifestos (como sempre foi);
//   2. fio do diagnostico: CADA FONTE DE ARTE que o app sabe usar para fundo e
//      logo (catalogo, Metahub, TMDB via /find, Trakt via /search/imdb, logo),
//      na mesma amostra de ate 3 titulos: resolucao, download, bytes, tamanho;
//   3. fio de desenho: a amostra de artes pelo CACHE DE TEXTURAS, tres vezes —
//      frio (enche o disco), ANTES (perfil atual, disco quente) e DEPOIS (o
//      candidato aplicado, a mesma amostra esquecida e pedida de novo). E o
//      fio de desenho porque tex_obter e tex_esquecer mexem em textura GL;
//   4. comparacao (ptv_decidir): pior -> o anterior volta SOZINHO e a tela
//      diz o motivo; dentro do ruido (sem ganho alem da margem) -> o anterior
//      tambem volta, `mantido_ruido`; ganho medido -> o perfil fica e vai
//      para o disco;
//   5. fio do diagnostico: relatorio e envio.
//
// O TESTE DE VELOCIDADE ("Teste de velocidade", botao proprio) roda SOZINHO,
// em outro fio e com estado fora de `d`: tempo do endpoint de fontes de cada
// addon e 8 s de corpo de ate 3 fontes reais de hosts diferentes, contados e
// descartados; a conta que vira "ate X GB por filme" mora em vazao.c. Quando
// ele rodou, os NUMEROS entram no relatorio (chaves vazao*), nunca url ou host.
//
// A FONTE DO DESTAQUE (ajuste do usuario) NUNCA muda sozinha: a medicao por
// fonte vira uma PROPOSTA escrita na tela, e so o botao "Aplicar sugestao"
// troca o ajuste — com o mesmo reteste e a mesma volta automatica.
#include "diagnostico.h"
#include "addons.h"
#include "artehero.h"
#include "artereserva.h"
#include "catalogo.h"
#include "avisos.h"
#include "dados.h"
#include "gfx.h"
#include "idioma.h"
#include "ajustes.h"
#include "layout.h"
#include "perfiltv.h"
#include "rede.h"
#include "fonteauto.h"
#include "streams.h"
#include "vazao.h"
#include "tex_cache.h"
#include "text.h"
#include "js.h"
#include <stdatomic.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NV_VERSAO
#define NV_VERSAO "dev"
#endif
#ifndef NV_DIAG_AUTO_OPT
/* LIGADA desde 22/09/2026. Ficou desligada enquanto nao havia reteste: aplicar
 * um perfil sem comparar era inventar ganho. Agora o candidato so fica se a
 * MESMA amostra, medida antes e depois pelo cache de texturas, nao piorou; se
 * piorou, o anterior volta sozinho (concluirComparacao). -DNV_DIAG_AUTO_OPT=0
 * volta ao modo so-relatorio para um build de medicao. */
#define NV_DIAG_AUTO_OPT 1
#endif

#define DIAG_MAX_ADDONS 16
#define DIAG_MAX_ASSETS 12
#define DIAG_TIMEOUT_S  6
#define DIAG_SESSAO_MS (8u * 60u * 1000u)
#define DIAG_MANIFEST_MAX (512L * 1024L)
#define DIAG_STREAM_MAX   (1024L * 1024L)
#define DIAG_ASSET_MAX    (12L * 1024L * 1024L)
// Amostra: 3 titulos (os mesmos das fontes de video) e ate 10 artes pelo cache.
#define DIAG_MAX_TITULOS 3
#define DIAG_MAX_ARTES   10
// Largura do cartaz pedido na amostra: a do card em pe da home (212 dp) com a
// folga de decode. O numero exato nao importa, importa ser o MESMO nos passes.
#define DIAG_LARG_CARTAZ 240.0f
// Prazo de UM passe pelo cache. 12 s cobrem 10 artes com disco quente mesmo
// na Samsung (fundo de 1280 decodificado em ~1,5 s, medido em 17/09); o que
// passar disso conta como falha do passe, nao trava o diagnostico.
#define DIAG_PASSE_PRAZO_MS 12000u
// Se a tela sair de cena no meio dos passes (a barra lateral abre por cima),
// o fio do diagnostico desiste em 90 s e desfaz o experimento sozinho.
#define DIAG_PASSES_MAX_MS 90000u

typedef enum {
  DR_UNUSED = 0, DR_OK, DR_DESATIVADO, DR_OFFLINE, DR_AUTH,
  DR_NOT_FOUND, DR_RATE_LIMIT, DR_INVALIDO, DR_VAZIO, DR_SERVIDOR,
  DR_INCOMPATIVEL
} DiagResultado;

// O que a aplicacao automatica fez, para a tela e para o relatorio.
typedef enum {
  DA_NENHUM = 0,
  DA_MANTIDO,          // candidato aplicado, reteste passou na margem (ptv_decidir)
  DA_RESTAURADO_AUTO,  // reteste piorou: o anterior voltou sozinho
  DA_IGUAL,            // o candidato e o que ja vale
  DA_SEM_AMOSTRA,      // nenhuma arte para comparar
  DA_SEM_CHECKPOINT,   // nao deu para gravar o checkpoint: nada aplicado
  DA_DESLIGADO,        // build com NV_DIAG_AUTO_OPT=0
  DA_CANCELADO,        // Voltar no meio: o anterior voltou
  DA_RESTAURADO_MANUAL, // a pessoa pediu o anterior
  DA_RUIDO             // nao piorou, mas nao ganhou alem da margem: o anterior voltou
} DiagAplicacao;

// O botao "Aplicar sugestao" da arte do destaque.
typedef enum { DS_NENHUMA = 0, DS_PROPOSTA, DS_TESTANDO, DS_MANTIDA, DS_DESFEITA } DiagSugEstado;

typedef struct {
  char nome[64];
  char host[160];
  char resultado[32];
  int ativo;
  int http;
  int bytes;
  int manifest_ms;
  int catalog_http;
  int catalog_ms;
  int catalog_ok;
  int stream_ms;
  int asset_ms;
  int asset_bytes;
  int catalogo;
  int stream;
  int legenda;
} DiagAddon;

typedef struct {
  char url[512];
  int heroi;
  int feito, ok, ms;
} DiagArte;

typedef struct {
  _Atomic int estado; // 0 parado, 1 rodando, 2 pronto, 3 cancelado, 4 falhou
  _Atomic int fase;   // 1 addons, 2 fontes de arte, 3 frio, 4 antes, 5 depois, 6 envio
  _Atomic int total;
  _Atomic int feitos;
  _Atomic int cancelado;
  _Atomic int sondasProntas;   // fio -> desenho: fases 1 e 2 acabaram
  _Atomic int passesProntos;   // desenho -> fio: comparacao decidida
  _Atomic int experimento;     // candidato no ar, ainda sem veredito
  _Atomic int enviando;
  _Atomic int sugPronta;       // fio da sugestao terminou a medida
  SDL_Thread *fio, *fioEnvio, *fioSug;
  DiagnosticoModo modo;
  char id[40];
  DiagAddon addon[DIAG_MAX_ADDONS];
  int nAddon;
  int nAssets;
  int imagensOk;
  int imagensFalhas;
  int assetsMs;
  int assetsBytes;
  char assetUrl[DIAG_MAX_ASSETS][512];
  int manifestMs;
  int catalogMs;
  int streamMs;
  int manifestOk;
  int manifestFalhas;
  int catalogOk;
  int catalogFalhas;
  int streamOk;
  int streamFalhas;
  // Amostra de titulos, COPIADA no fio de desenho: artehero devolve buffer
  // estatico e cat_item muda quando o catalogo recarrega.
  CatItem titulo[DIAG_MAX_TITULOS];
  int nTitulo;
  char fonteUrl[DIAG_MAX_TITULOS][PTV_N_FONTES][512];
  PtvFonte fonte[PTV_N_FONTES];
  // O CARD de cada titulo com os ajustes em vigor, e a assinatura do que cada
  // download devolveu (url real + FNV dos bytes): e o que decide
  // `igual_ao_card` por fonte (medirFontesDeArte).
  char cardUrl[DIAG_MAX_TITULOS][512];
  char realUrl[DIAG_MAX_TITULOS][PTV_N_FONTES][600];
  unsigned long long hashArte[DIAG_MAX_TITULOS][PTV_N_FONTES];
  long bytesArte[DIAG_MAX_TITULOS][PTV_N_FONTES];
  int fontesMs;
  // Passes pelo cache de texturas.
  DiagArte arte[DIAG_MAX_ARTES];
  int nArte;
  int passe;              // 0 nenhum, 1 frio, 2 antes, 3 depois
  Uint32 passeIni;
  int passePior;
  long passeDesp0;
  // Arte VISIVEL despejada desde o arranque ate o passe frio: o sinal de falta
  // de memoria que justifica subir o orcamento (ptv_decidir).
  long despSessao;
  PtvMedida medFrio, medAntes, medDepois;
  PtvPerfil perfAntes, perfCand;
  int travadoMb;
  char *cfgAntes;         // o perfil aprovado que valia antes (ou NULL)
  DiagAplicacao aplicacao;
  const char *motivo;
  // Sugestao de arte do destaque.
  PtvSugestao sug;
  DiagSugEstado sugEstado;
  int sugCalculada;
  int sugFonteAntes, sugDifAntes;
  char sugUrlA[DIAG_MAX_TITULOS][512], sugUrlB[DIAG_MAX_TITULOS][512];
  PtvMedida sugA, sugB;
  const char *sugMotivo;
  int enviado;
  int envioFalhou;
  char registroId[96];
  int intro;
  int botao;
  Uint32 inicioMs;
  int coberturaParcial;
  // O teste de velocidade terminou DEPOIS do envio: o relatorio foi remontado
  // com ele e o "Enviar de novo" aparece (botaoVisivel).
  int vazaoPendente;
  char erro[128];
  char relatorio[20000];
} Diagnostico;

static Diagnostico d;
static int focoModo;
// Na escolha do objetivo: 0 = os cartoes de objetivo, 1 = o botao do teste
// de velocidade embaixo deles (cima e baixo trocam).
static int focoLinha;

// ---------------------------------------------------------------------------
// TESTE DE VELOCIDADE (vazao.h): estado PROPRIO, fora de `d`.
//
// Fora de `d` porque roda sozinho, antes ou depois do diagnostico, e o
// "Testar de novo" do diagnostico zera `d` inteiro — o resultado da vazao tem
// de sobreviver a isso para entrar no relatorio. Um fio so, o mesmo padrao dos
// outros (estado atomico, juntado em juntarFios, cancelado pelo Voltar); os dois
// testes nunca rodam ao mesmo tempo, para um nao medir a rede ocupada pelo outro.
#define VAZ_POR_ADDON   3    // candidatas guardadas por addon
#define VAZ_MAX_CAND    24
#define VAZ_TENTATIVAS  6    // urls tocadas no maximo (resolucao + medida)
// Segundos de corpo por fonte. #ifndef so para tests/vazao.sh encurtar.
#ifndef VAZ_JANELA_S
#define VAZ_JANELA_S    8
#endif
#define VAZ_TITULOS     2    // 2o titulo so se o 1o nao trouxe fonte medivel
#define VAZ_ORCAMENTO_MS 45000u  // passou disso, nao comeca outra fonte
// O Range comeca 5 MB DENTRO do arquivo: o comeco de um MKV/MP4 e cabecalho,
// e o CDN costuma servi-lo de cache quente — mediria o cache, nao a fonte.
#define VAZ_INICIO_BYTE (5L * 1024L * 1024L)
// Teto de bytes por fonte, so contra conta errada: os bytes sao descartados
// e nada disto fica em memoria. 8 s a 1 Gbps sao 1 GB.
#define VAZ_TETO_BYTES  (1536LL * 1024LL * 1024LL)

typedef enum {
  VR_OK = 0, VR_SEM_ADDONS, VR_SEM_FONTES, VR_AVISO, VR_RECUSADO,
  VR_SEM_RESPOSTA, VR_NAVEGADOR, VR_CURTO, VR_CANCELADO, VR_N
} VazResultado;

typedef struct {
  int medido;       // 0 = desligado ou sem fontes no manifesto
  int ms, http, ok;
  int fontes;       // streams na resposta
  int candidatas;   // com link direto medivel
} VazAddon;

typedef struct {
  int kbps[VAZAO_SEG_MAX];
  int n;
  VazaoResumo r;
  int http;
  long long bytes;
  unsigned long ms;
} VazFonte;

typedef struct {
  char url[4096];
  char cab[512];
  long pontos;
  unsigned char acima;
} VazCand;

typedef struct {
  _Atomic int estado;     // 0 parado, 1 rodando, 2 pronto, 3 cancelado
  _Atomic int fase;       // 1 add-ons, 2 fontes
  _Atomic int total, feitos, cancelado;
  _Atomic int nFonte;     // fio -> desenho: fonte[k] ja escrita para k < nFonte
  SDL_Thread *fio;
  int aberto;             // a tela mostra o teste no lugar do diagnostico
  int modo;               // FONTEAUTO_*, lido no fio de desenho
  char titulo[VAZ_TITULOS][64];
  int nTitulo;
  VazAddon addon[DIAG_MAX_ADDONS];
  int nAddon;
  VazCand cand[VAZ_MAX_CAND];
  int nCand;
  VazFonte fonte[VAZAO_FONTES_MAX];
  int tentadas;
  int falhas[VR_N];
  int amostra[VAZAO_AMOSTRAS_MAX];
  int nAmostra;
  VazaoResumo resumo;
  VazResultado resultado;
  Uint32 inicioMs, fonteIniMs;
} Vazao;

static Vazao vz;
static int sairTela;
// O ATALHO DE AJUSTES (Ajustes › Diagnóstico › Teste de velocidade). `pedido`
// e o recado de diagnostico_abrir_velocidade para o diagnostico_iniciar que
// vem logo depois; `soVelocidade` e a tela aberta SO para o teste: sem
// apresentacao, sem escolha de objetivo, e o Voltar do resultado sai da tela
// em vez de cair na escolha que a pessoa nunca viu.
static int velocidadePedida, soVelocidade;
static int introGlobal;
static int introDecidido;

static const char *gargaloPrincipal(void);

static int apresentacaoVista(void) {
  char *marca = dados_ler("diagnostico-otimizacao-intro.cfg");
  int vista = marca != NULL;
  free(marca);
  return vista;
}

static int sessaoExpirada(void) {
  return d.inicioMs && SDL_GetTicks() - d.inicioMs >= DIAG_SESSAO_MS;
}

static RedeControle controleDiagnostico(long maxBytes) {
  RedeControle c;
  c.max_bytes = maxBytes;
  c.cancelado = (volatile int *)&d.cancelado;
  return c;
}

// SEM dados_fs_travar EM VOLTA de dados_gravar/ler/apagar: cada um ja trava
// por dentro, e a trava do Tizen nao e recursiva. As duas juntas no fio de
// desenho congelavam a Samsung no OK da apresentacao (issue #113, 1.4.2); na
// LG a trava e vazia e nada aparecia. tests/diagnostico.sh liga a trava real.
static void marcarApresentacaoVista(void) {
  dados_gravar("diagnostico-otimizacao-intro.cfg", "versao=1\n");
}

static void campoSeguro(char *dst, size_t cap, const char *src) {
  size_t n = 0;
  if (!dst || !cap) return;
  if (!src) src = "";
  while (*src && n + 1 < cap) {
    unsigned char c = (unsigned char)*src++;
    dst[n++] = (c < 0x20 || c == '|' || c == '=' || c == '\n' || c == '\r') ? '_' : (char)c;
  }
  dst[n] = 0;
}

static const char *resultadoNome(DiagResultado r) {
  switch (r) {
    case DR_OK: return "ok";
    case DR_DESATIVADO: return "desativado";
    case DR_OFFLINE: return "sem_resposta";
    case DR_AUTH: return "autenticacao";
    case DR_NOT_FOUND: return "nao_encontrado";
    case DR_RATE_LIMIT: return "limite";
    case DR_INVALIDO: return "invalido";
    case DR_VAZIO: return "vazio";
    case DR_SERVIDOR: return "erro_servidor";
    case DR_INCOMPATIVEL: return "incompativel";
    default: return "desconhecido";
  }
}

static const char *aplicacaoNome(DiagAplicacao a) {
  switch (a) {
    case DA_MANTIDO: return "aplicada";
    case DA_RESTAURADO_AUTO: return "restaurada_auto";
    case DA_IGUAL: return "ja_no_perfil";
    case DA_SEM_AMOSTRA: return "sem_amostra";
    case DA_SEM_CHECKPOINT: return "sem_checkpoint";
    case DA_DESLIGADO: return "padrao_mantido";
    case DA_CANCELADO: return "cancelada";
    case DA_RESTAURADO_MANUAL: return "restaurada_manual";
    case DA_RUIDO: return "mantido_ruido";
    default: return "sem_acao";
  }
}

static int jsonValido(const char *corpo) {
  const char *fim;
  if (!corpo || (corpo[0] != '{' && corpo[0] != '[')) return 0;
  fim = js_fim(corpo);
  if (!fim || fim == corpo) return 0;
  while (*fim && (unsigned char)*fim <= ' ') fim++;
  return *fim == 0;
}

static DiagResultado classificar(int status, const char *corpo) {
  if (status == 401 || status == 403) return DR_AUTH;
  if (status == 404) return DR_NOT_FOUND;
  if (status == 429) return DR_RATE_LIMIT;
  if (status >= 500) return DR_SERVIDOR;
  if (!corpo) return DR_OFFLINE;
  if (!corpo[0]) return DR_VAZIO;
  if (!jsonValido(corpo)) return DR_INVALIDO;
  return DR_OK;
}

static void urlJoin(char *dst, size_t cap, const char *base, const char *path) {
  size_t n;
  if (!dst || !cap) return;
  snprintf(dst, cap, "%s", base ? base : "");
  n = strlen(dst);
  while (n && dst[n - 1] == '/') dst[--n] = 0;
  snprintf(dst + n, cap - n, "/%s", path ? path : "");
}

// Id do titulo da amostra `ordem`. Le a COPIA feita no fio de desenho: o
// catalogo pode recarregar enquanto o fio do diagnostico roda.
static const char *amostraTitulo(int ordem) {
  if (d.nTitulo > 0) return d.titulo[ordem % d.nTitulo].imdb;
  return "tt0111161";
}

// O ENDPOINT DE FONTES do addon `i` para o filme `id`, medido. E o MESMO
// pedido nos dois testes: o diagnostico so conta tempo e status (e joga o
// corpo fora); o teste de velocidade ainda tira dele as candidatas a medir.
static char *baixarFontesAddon(int i, const char *id, const RedeControle *controle,
                               RedeMedida *medida) {
  char url[800];
  snprintf(url, sizeof url, "%s/stream/movie/%s.json", addons_base(i), id);
  return rede_baixar_medido_controle(url, DIAG_TIMEOUT_S, NULL, controle, medida);
}

static int fontesRespondeu(const char *corpo, const RedeMedida *m) {
  return corpo && m->status >= 200 && m->status < 300 && !m->limitado && jsonValido(corpo);
}

static int extrairUrl(const char *json, const char *campo, char *dst, size_t cap) {
  const char *p, *q;
  size_t n;
  if (!json || !campo || !dst || cap < 2) return 0;
  dst[0] = 0;
  p = strstr(json, campo);
  if (!p) return 0;
  p = strchr(p, ':');
  if (!p) return 0;
  p++;
  while (*p == ' ' || *p == '\t') p++;
  if (*p != '"') return 0;
  p++;
  q = p;
  while (*q && *q != '"') {
    if (*q == '\\' && q[1]) q += 2; else q++;
  }
  n = (size_t)(q - p);
  if (!n || n >= cap) return 0;
  memcpy(dst, p, n);
  dst[n] = 0;
  return !strncmp(dst, "http://", 7) || !strncmp(dst, "https://", 8);
}

static void medirAsset(const char *url) {
  Uint32 inicio;
  long n = 0;
  char *corpo;
  RedeMedida medida;
  RedeControle controle = controleDiagnostico(DIAG_ASSET_MAX);
  if (!url || !*url || d.nAssets >= DIAG_MAX_ASSETS || atomic_load(&d.cancelado)) return;
  if (sessaoExpirada()) { d.coberturaParcial = 1; return; }
  snprintf(d.assetUrl[d.nAssets], sizeof d.assetUrl[d.nAssets], "%s", url);
  inicio = SDL_GetTicks();
  corpo = rede_baixar_bin_medido_controle(url, DIAG_TIMEOUT_S, NULL, &controle, &n, &medida);
  d.assetsMs += (int)(SDL_GetTicks() - inicio);
  if (corpo && n > 0) {
    d.imagensOk++;
    d.assetsBytes += (int)(n > 2147483647L ? 2147483647L : n);
  } else if (!medida.cancelado) d.imagensFalhas++;
  d.nAssets++;
  if (medida.cancelado) atomic_store(&d.cancelado, 1);
  free(corpo);
}

// UMA ARTE DE UMA FONTE: resolucao (so url virtual: TMDB /find ou Trakt
// /search/imdb, sem cache — e o custo que o fio de rede do tex_cache paga uma
// vez por titulo) e download, com bytes e o tamanho lido do cabecalho.
// Fora do fio de desenho: so rede e aritmetica.
static int medirUrlArte(const char *url, int *resolveMs, int *downloadMs,
                        long *bytes, int *w, int *h,
                        char *final, size_t nFinal, unsigned long long *hash) {
  char real[600];
  const char *alvo = url;
  Uint32 t0;
  long n = 0;
  char *corpo;
  RedeMedida medida;
  RedeControle controle = controleDiagnostico(DIAG_ASSET_MAX);
  *resolveMs = *downloadMs = 0; *bytes = 0; *w = *h = 0;
  if (!url || !*url) return 0;
  t0 = SDL_GetTicks();
  { int r = arte_fonte_resolver(url, real, sizeof real);
    *resolveMs = (int)(SDL_GetTicks() - t0);
    if (r < 0) return 0;
    if (r > 0) alvo = real; }
  corpo = rede_baixar_bin_medido_controle(alvo, DIAG_TIMEOUT_S, NULL, &controle, &n, &medida);
  *downloadMs = (int)medida.ms;
  if (medida.cancelado) atomic_store(&d.cancelado, 1);
  if (!corpo || n <= 0 || medida.status >= 400) { free(corpo); return 0; }
  *bytes = n;
  ptv_dimensoes((const unsigned char *)corpo, n, w, h);
  if (final && nFinal) snprintf(final, nFinal, "%s", alvo);
  if (hash) *hash = arte_bytes_hash(corpo, n);
  free(corpo);
  return 1;
}

// A MESMA ARTE DO CARD? Mesma url real (o tamanho do TMDB e o medium/full do
// Trakt nao contam: e a mesma foto) ou mesmos bytes. O catalogo do Cinemeta e
// o metahub pelo id caem aqui pelos bytes (1763947 nos dois no relatorio
// 1669). A mesma foto REENCODADA (metahub x backdrop do TMDB) nao: isso so
// comparando pixels, e o relatorio diz que a comparacao e por arquivo.
static void chaveArte(const char *u, char *k, size_t n) {
  const char *p = strstr(u, "/t/p/");
  if (p && (p = strchr(p + 5, '/')) != NULL) { snprintf(k, n, "tmdb%s", p); return; }
  if (strstr(u, "media.trakt.tv/") && ((p = strstr(u, "/medium/")) || (p = strstr(u, "/full/")))) {
    snprintf(k, n, "trakt%s", strchr(p + 1, '/')); return;
  }
  snprintf(k, n, "%s", u);
}
static int mesmaArte(int t, int a, int b) {
  char ka[640], kb[640];
  if (!d.realUrl[t][a][0] || !d.realUrl[t][b][0]) return 0;
  chaveArte(d.realUrl[t][a], ka, sizeof ka);
  chaveArte(d.realUrl[t][b], kb, sizeof kb);
  if (!strcmp(ka, kb)) return 1;
  return d.bytesArte[t][a] == d.bytesArte[t][b] && d.hashArte[t][a] == d.hashArte[t][b];
}

// TODAS as fontes de fundo (catalogo, Metahub, TMDB, Trakt, Apple TV,
// fanart.tv, anime e o outro do TMDB) e o logo, por titulo; depois o CARD do
// titulo — que quase sempre e uma das urls ja medidas (a do catalogo) e entao
// nao custa download — e a comparacao de cada fonte com ele.
static void medirFontesDeArte(void) {
  int t, f;
  for (t = 0; t < d.nTitulo; t++) {
    int card = -1;
    for (f = 1; f < PTV_N_FONTES; f++) {
      PtvFonte *s = &d.fonte[f];
      int rMs, dMs, w, h;
      long b;
      if (!d.fonteUrl[t][f][0]) continue;
      if (atomic_load(&d.cancelado)) return;
      if (sessaoExpirada()) { d.coberturaParcial = 1; return; }
      if (medirUrlArte(d.fonteUrl[t][f], &rMs, &dMs, &b, &w, &h,
                       d.realUrl[t][f], sizeof d.realUrl[t][f], &d.hashArte[t][f])) {
        s->ok++;
        s->bytes += b;
        s->downloadOkMs += dMs;
        d.bytesArte[t][f] = b;
        if (w > 0) { s->largura = w; s->altura = h; }
      } else s->falhas++;
      s->resolveMs += rMs;
      s->downloadMs += dMs;
      if (dMs > s->downloadPiorMs) s->downloadPiorMs = dMs;
      d.fontesMs += rMs + dMs;
      atomic_fetch_add(&d.feitos, 1);
    }
    for (f = 1; f <= PTV_FONTE_FUNDO_MAX && card < 0; f++)
      if (d.cardUrl[t][0] && !strcmp(d.cardUrl[t], d.fonteUrl[t][f]) && d.realUrl[t][f][0]) card = f;
    if (card < 0 && d.cardUrl[t][0]) {
      // Card fora das fontes (still, cartaz, url de addon): baixa uma vez,
      // fora dos tempos por fonte, na celula 0 (nenhuma fonte usa o 0).
      int rMs, dMs, w, h;
      long b;
      if (medirUrlArte(d.cardUrl[t], &rMs, &dMs, &b, &w, &h,
                       d.realUrl[t][0], sizeof d.realUrl[t][0], &d.hashArte[t][0])) {
        d.bytesArte[t][0] = b;
        card = 0;
      }
    }
    if (card < 0) continue;
    for (f = 1; f <= PTV_FONTE_FUNDO_MAX; f++)
      if (f != card && d.realUrl[t][f][0] && mesmaArte(t, f, card)) d.fonte[f].iguais++;
    // A fonte que E o card conta como igual a ele, por definicao.
    if (card > 0) d.fonte[card].iguais++;
  }
}

// ---------------------------------------------------------------------------
// PERFIL: aplicar, restaurar, persistir.

static void aplicarPerfilTex(const PtvPerfil *pf, int travado) {
  // O orcamento so muda quando nao ha escolha manual (Ajustes, NV_TEX_MB_FIXO
  // do alto-cache, NUVIO_TEX_MB): o perfil troca o que "Automatico" significa.
  if (!travado) tex_definir_orcamento_auto_mb(pf->texMb);
  tex_definir_fios_rede(pf->fiosRede);
  tex_definir_teto_heroi(pf->heroiLarg);
}

static void perfilAtual(PtvPerfil *pf, int *travado, long *mem) {
  int mb = 0, fixo = 0;
  long m = 0;
  tex_orcamento_info(&mb, &m, &fixo, NULL);
  pf->texMb = mb;
  pf->fiosRede = tex_fios_rede();
  pf->heroiLarg = tex_teto_heroi_perfil();
  if (pf->heroiLarg <= 0) pf->heroiLarg = ptv_heroi_max(ptv_plataforma(), m);
  if (travado) *travado = fixo ? mb : 0;
  if (mem) *mem = m;
}

static const char *modoNome(void) {
  return d.modo == DIAG_DESEMPENHO ? "desempenho" : "qualidade";
}

static void restaurarAntes(void) {
  aplicarPerfilTex(&d.perfAntes, d.travadoMb);
  printf("[diagnostico] perfil anterior restaurado: %d MB, %d fios, heroi %d\n",
         d.perfAntes.texMb, d.perfAntes.fiosRede, d.perfAntes.heroiLarg);
  fflush(stdout);
}

// Quem pega o experimento o desfaz. Fio de desenho (Voltar, reteste pior) ou
// fio do diagnostico (tela fora de cena): a troca atomica garante uma vez so.
static int desfazerExperimento(void) {
  if (atomic_exchange(&d.experimento, 0) != 1) return 0;
  restaurarAntes();
  dados_apagar("diagnostico-otimizacao.checkpoint");
  return 1;
}

// Candidato no ar. 0 = nada aplicado (d.aplicacao diz por que).
static int aplicarCandidato(void) {
  long mem = 0;
  char ck[256];
  int ok;
  perfilAtual(&d.perfAntes, &d.travadoMb, &mem);
  ptv_candidato(ptv_plataforma(), mem,
                d.modo == DIAG_DESEMPENHO ? PTV_DESEMPENHO : PTV_QUALIDADE,
                d.travadoMb, &d.perfCand);
#if !NV_DIAG_AUTO_OPT
  d.aplicacao = DA_DESLIGADO;
  return 0;
#endif
  if (!memcmp(&d.perfCand, &d.perfAntes, sizeof d.perfCand)) {
    d.aplicacao = DA_IGUAL;
    return 0;
  }
  // CHECKPOINT ANTES DE MEXER: o candidato vive so em memoria ate o veredito,
  // entao um desligamento no meio nao o deixa gravado. O arquivo existe para o
  // arranque seguinte saber que o experimento foi interrompido e apagar o
  // rastro, e para o relatorio contar isso. Sem conseguir grava-lo, nada muda.
  snprintf(ck, sizeof ck,
           "versao=2\nestado=experiment_pending\ntex_mb=%d\nfios_rede=%d\nheroi=%d\ntravado=%d\n",
           d.perfAntes.texMb, d.perfAntes.fiosRede, d.perfAntes.heroiLarg, d.travadoMb);
  ok = dados_gravar("diagnostico-otimizacao.checkpoint", ck);
  free(d.cfgAntes);
  d.cfgAntes = dados_ler("diagnostico-otimizacao.cfg");
  if (!ok) {
    snprintf(d.erro, sizeof d.erro, "%s", "Não foi possível salvar o checkpoint");
    d.aplicacao = DA_SEM_CHECKPOINT;
    return 0;
  }
  aplicarPerfilTex(&d.perfCand, d.travadoMb);
  atomic_store(&d.experimento, 1);
  printf("[diagnostico] candidato %s no ar: %d MB, %d fios, heroi %d (antes %d/%d/%d)\n",
         modoNome(), d.perfCand.texMb, d.perfCand.fiosRede, d.perfCand.heroiLarg,
         d.perfAntes.texMb, d.perfAntes.fiosRede, d.perfAntes.heroiLarg);
  fflush(stdout);
  return 1;
}

// O veredito tem MARGEM (ptv_decidir): pior restaura; dentro do ruido tambem
// volta ao anterior, sem gravar nada, para a TV nao trocar de perfil a cada
// rodada; so um ganho medido (ou o heroi que Qualidade pede, ou memoria com
// arte visivel despejada) fica.
static void concluirComparacao(void) {
  const char *m = NULL;
  PtvDecisao dec;
  if (atomic_load(&d.experimento) != 1) return;
  dec = ptv_decidir(&d.perfAntes, &d.perfCand, &d.medAntes, &d.medDepois, d.despSessao, &m);
  if (dec != PTV_DEC_APLICAR) {
    desfazerExperimento();
    d.aplicacao = dec == PTV_DEC_RESTAURAR ? DA_RESTAURADO_AUTO : DA_RUIDO;
    d.motivo = m;
  } else {
    char cfg[256];
    atomic_store(&d.experimento, 0);
    ptv_serializar(&d.perfCand, modoNome(), cfg, sizeof cfg);
    dados_gravar("diagnostico-otimizacao.cfg", cfg);
    dados_apagar("diagnostico-otimizacao.checkpoint");
    d.aplicacao = DA_MANTIDO;
  }
  printf("[diagnostico] reteste: antes %d ms/%d falhas/pior %d ms, depois %d ms/%d falhas/pior %d ms -> %s%s%s\n",
         d.medAntes.artesMs, d.medAntes.falhas, d.medAntes.piorQuadroMs,
         d.medDepois.artesMs, d.medDepois.falhas, d.medDepois.piorQuadroMs,
         aplicacaoNome(d.aplicacao), m ? ": " : "", m ? m : "");
  fflush(stdout);
}

// "Restaurar anterior", depois de um perfil mantido.
static void restaurarManual(void) {
  if (d.aplicacao != DA_MANTIDO) return;
  restaurarAntes();
  if (d.cfgAntes) dados_gravar("diagnostico-otimizacao.cfg", d.cfgAntes);
  else dados_apagar("diagnostico-otimizacao.cfg");
  d.aplicacao = DA_RESTAURADO_MANUAL;
}

// ---------------------------------------------------------------------------
// PASSES PELO CACHE DE TEXTURAS (fio de desenho).

static void montarAmostraArtes(void) {
  int i, t, fonte = ajustes_hero_fonte(), dif = ajustes_hero_arte_diferente();
  d.nArte = 0;
  // O FUNDO DO DESTAQUE com os ajustes em vigor: e a arte mais cara que o app
  // pede (1920 na LG) e a que depende do teto do heroi.
  for (t = 0; t < d.nTitulo && d.nArte < DIAG_MAX_ARTES; t++) {
    const char *u = artehero_url_destaque(&d.titulo[t], fonte, dif);
    if (!u || !*u) continue;
    snprintf(d.arte[d.nArte].url, sizeof d.arte[d.nArte].url, "%s", u);
    d.arte[d.nArte++].heroi = 1;
  }
  for (i = 0; i < cat_n() && d.nArte < DIAG_MAX_ARTES; i++) {
    const CatItem *c = cat_item(i);
    int j, repetido = 0;
    if (!c || !c->poster[0]) continue;
    for (j = 0; j < d.nArte; j++) if (!strcmp(d.arte[j].url, c->poster)) repetido = 1;
    if (repetido) continue;
    snprintf(d.arte[d.nArte].url, sizeof d.arte[d.nArte].url, "%s", c->poster);
    d.arte[d.nArte++].heroi = 0;
  }
  // Sem catalogo (primeira abertura, conta vazia): os assets dos manifestos.
  for (i = 0; i < d.nAssets && d.nArte < DIAG_MAX_ARTES; i++) {
    if (!d.assetUrl[i][0]) continue;
    snprintf(d.arte[d.nArte].url, sizeof d.arte[d.nArte].url, "%s", d.assetUrl[i]);
    d.arte[d.nArte++].heroi = 0;
  }
}

static void esquecerAmostra(void) {
  int i;
  for (i = 0; i < d.nArte; i++) tex_esquecer(d.arte[i].url);
}

static void passeIniciar(int qual) {
  int i;
  d.passe = qual;
  d.passeIni = SDL_GetTicks();
  d.passePior = 0;
  d.passeDesp0 = tex_despejos_quentes_total;
  for (i = 0; i < d.nArte; i++) { d.arte[i].feito = 0; d.arte[i].ok = 0; d.arte[i].ms = 0; }
  atomic_store(&d.fase, 2 + qual);
}

// 1 quando o passe terminou, com `m` preenchida.
static int passePasso(PtvMedida *m) {
  Uint32 agora = SDL_GetTicks(), dt = agora - d.passeIni;
  int i, pend = 0, maior = 0;
  for (i = 0; i < d.nArte; i++) {
    DiagArte *a = &d.arte[i];
    GLuint t;
    if (a->feito) continue;
    t = a->heroi ? tex_obter_hero(a->url) : tex_obter_larg(a->url, DIAG_LARG_CARTAZ);
    if (t) { a->feito = 1; a->ok = 1; a->ms = (int)dt; }
    else if (tex_falhou(a->url)) { a->feito = 1; a->ok = 0; }
    else pend++;
  }
  if (pend && dt < DIAG_PASSE_PRAZO_MS) return 0;
  memset(m, 0, sizeof *m);
  for (i = 0; i < d.nArte; i++) {
    if (d.arte[i].ok) { m->prontas++; if (d.arte[i].ms > maior) maior = d.arte[i].ms; }
    else m->falhas++;
  }
  m->artesMs = pend ? (int)dt : maior;
  m->piorQuadroMs = d.passePior;
  m->despejosQuentes = (int)(tex_despejos_quentes_total - d.passeDesp0);
  return 1;
}

static void passesAvancar(void) {
  PtvMedida m;
  if (!d.passe) {
    montarAmostraArtes();
    if (!d.nArte) {
      d.aplicacao = DA_SEM_AMOSTRA;
      atomic_store(&d.passesProntos, 1);
      return;
    }
    d.despSessao = tex_despejos_quentes_total;
    passeIniciar(1);
    return;
  }
  if (!passePasso(&m)) return;
  if (d.passe == 1) {
    d.medFrio = m;
    esquecerAmostra();
    passeIniciar(2);
  } else if (d.passe == 2) {
    d.medAntes = m;
    if (!aplicarCandidato()) { d.passe = 0; atomic_store(&d.passesProntos, 1); return; }
    esquecerAmostra();
    passeIniciar(3);
  } else {
    d.medDepois = m;
    concluirComparacao();
    d.passe = 0;
    atomic_store(&d.passesProntos, 1);
  }
}

// ---------------------------------------------------------------------------
// SUGESTAO PARA A ARTE DO DESTAQUE.

// Qual das fontes medidas e esta url, para o titulo `t`. Casa pela url exata
// que foi medida; sem casamento, pelo host.
static int fonteDaUrl(int t, const char *u) {
  int f;
  if (!u) return 0;
  for (f = 1; f <= PTV_FONTE_FUNDO_MAX; f++)
    if (d.fonteUrl[t][f][0] && !strcmp(d.fonteUrl[t][f], u)) return f;
  return ptv_fonte_da_url(u);
}

static void calcularSugestao(void) {
  int fonte = ajustes_hero_fonte(), dif = ajustes_hero_arte_diferente();
  int hero, card, t;
  d.sugEstado = DS_NENHUMA;
  if (!d.nTitulo) return;
  hero = fonteDaUrl(0, artehero_url_destaque(&d.titulo[0], fonte, dif));
  card = dif ? PTV_FONTE_CATALOGO
             : fonteDaUrl(0, artehero_url_card_fonte(&d.titulo[0], fonte, dif));
  // Com outra arte, o card e o catalogo; o que conta como "igual" ja foi
  // medido por titulo (PtvFonte.iguais), e a fonte do card o e por definicao.
  if (!ptv_sugerir_destaque(d.fonte, hero, card, fonte, dif, &d.sug)) return;
  // A MESMA FOTO DO CARD NAO CONTA COMO OUTRA ARTE (artehero_url_destaque): a
  // fonte proposta pode cair de volta na lenta para algum titulo. Confere na
  // amostra inteira; se cair, a proposta vira desligar a outra arte.
  if (d.sug.diferente)
    for (t = 0; t < d.nTitulo; t++)
      if (fonteDaUrl(t, artehero_url_destaque(&d.titulo[t], d.sug.fonte, 1)) == d.sug.lenta) {
        d.sug.diferente = 0;
        d.sug.fonte = fonte;
        break;
      }
  d.sugEstado = DS_PROPOSTA;
}

static int sugestaoWorker(void *arg) {
  int rep, t;
  (void)arg;
  memset(&d.sugA, 0, sizeof d.sugA);
  memset(&d.sugB, 0, sizeof d.sugB);
  // A e B INTERCALADOS, duas voltas: a rede da TV oscila no tempo, e medir
  // todos os A e depois todos os B atribuiria a oscilacao a fonte.
  for (rep = 0; rep < 2; rep++)
    for (t = 0; t < d.nTitulo; t++) {
      int k;
      for (k = 0; k < 2; k++) {
        const char *u = k ? d.sugUrlB[t] : d.sugUrlA[t];
        PtvMedida *m = k ? &d.sugB : &d.sugA;
        int rMs, dMs, w, h;
        long b;
        if (!u[0] || atomic_load(&d.cancelado)) continue;
        if (medirUrlArte(u, &rMs, &dMs, &b, &w, &h, NULL, 0, NULL)) m->prontas++;
        else m->falhas++;
        m->artesMs += rMs + dMs;
      }
    }
  atomic_store(&d.sugPronta, 1);
  return 0;
}

static void aplicarSugestao(void) {
  int t;
  if (d.sugEstado != DS_PROPOSTA || d.fioSug) return;
  d.sugFonteAntes = ajustes_hero_fonte();
  d.sugDifAntes = ajustes_hero_arte_diferente();
  for (t = 0; t < d.nTitulo; t++) {
    const char *a = artehero_url_destaque(&d.titulo[t], d.sugFonteAntes, d.sugDifAntes);
    snprintf(d.sugUrlA[t], sizeof d.sugUrlA[t], "%s", a ? a : "");
  }
  for (t = 0; t < d.nTitulo; t++) {
    const char *b = artehero_url_destaque(&d.titulo[t], d.sug.fonte, d.sug.diferente);
    snprintf(d.sugUrlB[t], sizeof d.sugUrlB[t], "%s", b ? b : "");
  }
  ajustes_definir_destaque(d.sug.fonte, d.sug.diferente);
  atomic_store(&d.cancelado, 0);
  atomic_store(&d.sugPronta, 0);
  d.sugEstado = DS_TESTANDO;
  d.fioSug = SDL_CreateThread(sugestaoWorker, "nuvio-diag-arte", NULL);
  if (!d.fioSug) {
    ajustes_definir_destaque(d.sugFonteAntes, d.sugDifAntes);
    d.sugEstado = DS_DESFEITA;
    d.sugMotivo = "Não foi possível iniciar o teste";
  }
}

static void concluirSugestao(void) {
  const char *m = NULL;
  if (d.fioSug) { SDL_WaitThread(d.fioSug, NULL); d.fioSug = NULL; }
  if (ptv_depois_pior(&d.sugA, &d.sugB, &m) || atomic_load(&d.cancelado)) {
    ajustes_definir_destaque(d.sugFonteAntes, d.sugDifAntes);
    d.sugEstado = DS_DESFEITA;
    d.sugMotivo = m ? m : "O teste foi cancelado.";
  } else d.sugEstado = DS_MANTIDA;
  printf("[diagnostico] sugestao de destaque: antes %d ms/%d falhas, depois %d ms/%d falhas -> %s\n",
         d.sugA.artesMs, d.sugA.falhas, d.sugB.artesMs, d.sugB.falhas,
         d.sugEstado == DS_MANTIDA ? "mantida" : "desfeita");
  fflush(stdout);
}

// ---------------------------------------------------------------------------
// RELATORIO E FIO DO DIAGNOSTICO.

static const char *vazaoNome(VazResultado r) {
  static const char *const NOMES[VR_N] = {
    "ok", "sem_addons", "sem_fontes", "aviso", "recusado", "sem_resposta",
    "navegador", "curto", "cancelado"
  };
  return r >= 0 && r < VR_N ? NOMES[r] : "desconhecido";
}

static void montarRelatorio(void) {
  int texItens = 0, texPend = 0, texQuentes = 0, texSlots = 0;
  int texMb = 0, fios = 0, fiosMax = 0, i;
  long texBytes = 0, texLimite = 0, memTotal = 0;
  char *p = d.relatorio;
  size_t left = sizeof d.relatorio;
  int wrote;
  tex_estatisticas(&texItens, &texPend, &texBytes, &texQuentes, NULL);
  tex_orcamento_info(&texMb, &memTotal, NULL, &texSlots);
  texLimite = tex_orcamento_bytes();
  tex_threads_info(&fios, &fiosMax);
  d.relatorio[0] = 0;
#define ACRESCENTA(...) do { wrote = snprintf(p, left, __VA_ARGS__); \
    if (wrote > 0 && (size_t)wrote < left) { p += wrote; left -= (size_t)wrote; } } while (0)
  ACRESCENTA("diagnostico=v2\nid=%s\nversao=%s\nmodo=%s\naddons=%d\nmanifest_ok=%d\nmanifest_falhas=%d\nmanifest_ms=%d\ncatalog_ok=%d\ncatalog_falhas=%d\ncatalog_ms=%d\nassets=%d\nassets_ok=%d\nassets_falhas=%d\nassets_ms=%d\nassets_bytes=%d\nstreams_ok=%d\nstreams_falhas=%d\nstreams_ms=%d\ntex_itens=%d\ntex_pendentes=%d\ntex_quentes=%d\ntex_bytes=%ld\ntex_limite=%ld\ntex_orcamento_mb=%d\nmem_total_mb=%ld\nthreads_usadas=%d\nthreads_disponiveis=%d\ngargalo=%s\naplicacao=%s\ncobertura=%s\n",
    d.id, NV_VERSAO, modoNome(), d.nAddon, d.manifestOk, d.manifestFalhas, d.manifestMs,
    d.catalogOk, d.catalogFalhas, d.catalogMs, d.nAssets, d.imagensOk,
    d.imagensFalhas, d.assetsMs, d.assetsBytes, d.streamOk, d.streamFalhas,
    d.streamMs, texItens, texPend, texQuentes, texBytes, texLimite, texMb,
    memTotal, fios, fiosMax, gargaloPrincipal(), aplicacaoNome(d.aplicacao),
    d.coberturaParcial ? "parcial" : "completa");
  ACRESCENTA("perfil_antes=%d|%d|%d\nperfil_candidato=%d|%d|%d\ntravado_mb=%d\n",
             d.perfAntes.texMb, d.perfAntes.fiosRede, d.perfAntes.heroiLarg,
             d.perfCand.texMb, d.perfCand.fiosRede, d.perfCand.heroiLarg, d.travadoMb);
  ACRESCENTA("amostra_artes=%d\nartes_frio_ms=%d\nartes_antes_ms=%d\nartes_depois_ms=%d\nartes_falhas_antes=%d\nartes_falhas_depois=%d\npior_quadro_antes_ms=%d\npior_quadro_depois_ms=%d\ndespejos_quentes_depois=%d\nmotivo=%s\n",
             d.nArte, d.medFrio.artesMs, d.medAntes.artesMs, d.medDepois.artesMs,
             d.medAntes.falhas, d.medDepois.falhas, d.medAntes.piorQuadroMs,
             d.medDepois.piorQuadroMs, d.medDepois.despejosQuentes, d.motivo ? d.motivo : "");
  // Chaves novas (23/09): o que ptv_decidir usou alem das de cima. Relatorio
  // antigo nao as tem; o agregador le as duas formas.
  ACRESCENTA("despejos_quentes_antes=%d\ndespejos_quentes_sessao=%ld\nmargem_ganho=%d%%|%dms\n",
             d.medAntes.despejosQuentes, d.despSessao, PTV_GANHO_PCT, PTV_GANHO_MIN_MS);
  ACRESCENTA("destaque_fonte=%d\ndestaque_diferente=%d\n", ajustes_hero_fonte(),
             ajustes_hero_arte_diferente());
  for (i = 1; i < PTV_N_FONTES; i++) {
    const PtvFonte *f = &d.fonte[i];
    // `iguais` = em quantos titulos a arte foi a mesma do card; o logo nao e
    // fundo e fica sempre 0. Fonte nao medida (sem chave, nao e anime, sem
    // ano para a Apple) sai com ok=0 e falhas=0.
    // download_ms continua sendo a SOMA (o agregador e os relatorios antigos
    // leem assim); download_medio_ms (so os que deram certo) e
    // download_pior_ms (um pedido) vao no fim, para um prazo estourado nao se
    // esconder na soma. -1 = nenhum download deu certo.
    ACRESCENTA("arte_fonte=%s|ok=%d|falhas=%d|resolve_ms=%d|download_ms=%d|bytes=%ld|largura=%d|altura=%d|iguais=%d|igual_ao_card=%d|download_medio_ms=%d|download_pior_ms=%d\n",
               ptv_fonte_nome(i), f->ok, f->falhas, f->resolveMs, f->downloadMs,
               f->bytes, f->largura, f->altura, f->iguais, f->iguais > 0,
               f->ok > 0 ? f->downloadOkMs / f->ok : -1, f->downloadPiorMs);
  }
  if (d.sugEstado == DS_PROPOSTA)
    ACRESCENTA("sugestao_destaque=alvo:%s|diferente:%d|lenta:%s|ms_lenta=%d|base:%s|ms_base=%d|motivo:%s\n",
               ptv_fonte_nome(d.sug.alvo > 0 ? d.sug.alvo : d.sug.fonte), d.sug.diferente,
               ptv_fonte_nome(d.sug.lenta), d.sug.msLenta, ptv_fonte_nome(d.sug.base), d.sug.msBase,
               d.sug.motivo == PTV_MOTIVO_IGUAL ? "igual_ao_card" : "lenta");
  else ACRESCENTA("sugestao_destaque=-\n");
  for (i = 0; i < d.nAddon && left > 40; i++) {
    DiagAddon *a = &d.addon[i];
    ACRESCENTA("addon=%s|host=%s|active=%d|result=%s|http=%d|bytes=%d|manifest_ms=%d|catalog_http=%d|catalog_ms=%d|catalog_ok=%d|stream_ms=%d|asset_ms=%d|catalog=%d|stream=%d|subtitle=%d\n",
               a->nome, a->host, a->ativo, a->resultado, a->http,
               a->bytes, a->manifest_ms, a->catalog_http, a->catalog_ms,
               a->catalog_ok, a->stream_ms, a->asset_ms,
               a->catalogo, a->stream, a->legenda);
  }
  // TESTE DE VELOCIDADE, quando rodou nesta tela. SO NUMEROS: nenhuma url,
  // host, token ou titulo — o addon e a fonte vao pelo NUMERO (o addon N e a
  // N-esima linha addon= acima, quando o diagnostico tambem rodou).
  if (atomic_load(&vz.estado) == 2) {
    int nf = atomic_load(&vz.nFonte);
    ACRESCENTA("vazao=v1\nvazao_resultado=%s\nvazao_modo=%s\nvazao_mediana_kbps=%d\nvazao_p20_kbps=%d\nvazao_otimo_kbps=%d\nvazao_maximo_kbps=%d\nvazao_amostras=%d\nvazao_fontes_medidas=%d\nvazao_fontes_tentadas=%d\nvazao_candidatas=%d\n",
               vazaoNome(vz.resultado), vz.modo == FONTEAUTO_PRIMEIRA ? "primeira" : "melhor",
               vz.resumo.medianaKbps, vz.resumo.p20Kbps, vz.resumo.otimoKbps,
               vz.resumo.maximoKbps, vz.nAmostra, nf, vz.tentadas, vz.nCand);
    ACRESCENTA("vazao_falhas=aviso:%d|recusado:%d|sem_resposta:%d|navegador:%d|curto:%d\n",
               vz.falhas[VR_AVISO], vz.falhas[VR_RECUSADO], vz.falhas[VR_SEM_RESPOSTA],
               vz.falhas[VR_NAVEGADOR], vz.falhas[VR_CURTO]);
    for (i = 0; i < nf && left > 40; i++) {
      const VazFonte *f = &vz.fonte[i];
      ACRESCENTA("vazao_fonte=%d|mediana_kbps=%d|p20_kbps=%d|segundos=%d|mb=%lld|http=%d\n",
                 i + 1, f->r.medianaKbps, f->r.p20Kbps, f->n, f->bytes / 1000000LL, f->http);
    }
    for (i = 0; i < vz.nAddon && left > 40; i++) {
      const VazAddon *a = &vz.addon[i];
      if (!a->medido) continue;
      ACRESCENTA("vazao_addon=%d|ms=%d|http=%d|ok=%d|streams=%d|diretas=%d\n",
                 i + 1, a->ms, a->http, a->ok, a->fontes, a->candidatas);
    }
  }
#undef ACRESCENTA
}

static int diagnosticoWorker(void *arg) {
  int i;
  int catalogTestados = 0, streamTestados = 0;
  RedeControle controle;
  (void)arg;
  d.nAddon = addons_n();
  if (d.nAddon > DIAG_MAX_ADDONS) d.nAddon = DIAG_MAX_ADDONS;
  { int urls = 0, t, f;
    for (t = 0; t < d.nTitulo; t++)
      for (f = 1; f < PTV_N_FONTES; f++) if (d.fonteUrl[t][f][0]) urls++;
    atomic_store(&d.total, d.nAddon + urls); }
  atomic_store(&d.fase, 1);
  controle = controleDiagnostico(DIAG_MANIFEST_MAX);
  for (i = 0; i < d.nAddon; i++) {
    DiagAddon *a = &d.addon[i];
    const char *base = addons_base(i);
    char url[760], safe[200], *corpo;
    int status = 0;
    RedeMedida medida;
    memset(a, 0, sizeof *a);
    campoSeguro(a->nome, sizeof a->nome, addons_nome(i));
    rede_url_publica(base, safe, sizeof safe);
    campoSeguro(a->host, sizeof a->host, safe);
    a->ativo = addons_ativo(i);
    if (sessaoExpirada()) { d.coberturaParcial = 1; break; }
    if (!a->ativo) {
      snprintf(a->resultado, sizeof a->resultado, "%s", resultadoNome(DR_DESATIVADO));
      atomic_fetch_add(&d.feitos, 1);
      continue;
    }
    urlJoin(url, sizeof url, base, "manifest.json");
    corpo = rede_baixar_medido_controle(url, DIAG_TIMEOUT_S, NULL, &controle, &medida);
    a->manifest_ms = (int)medida.ms;
    d.manifestMs += a->manifest_ms;
    a->http = medida.status;
    status = medida.status;
    if (corpo) a->bytes = (int)medida.bytes;
    { DiagResultado r = classificar(status, corpo);
      if (r == DR_OK) d.manifestOk++;
      else if (r != DR_DESATIVADO) d.manifestFalhas++;
      snprintf(a->resultado, sizeof a->resultado, "%s", resultadoNome(r));
      if (corpo && jsonValido(corpo) && corpo[0] == '{') {
        addons_manifesto_lido(i, corpo);
        a->catalogo = addons_fornece(i, ADD_CATALOGO);
        a->stream = addons_fornece(i, ADD_STREAM);
        a->legenda = addons_fornece(i, ADD_LEGENDA);
        if (r == DR_OK && !a->catalogo && !a->stream && !a->legenda)
          snprintf(a->resultado, sizeof a->resultado, "%s", resultadoNome(DR_INCOMPATIVEL));
      }
    }
    if (medida.cancelado) {
      atomic_store(&d.cancelado, 1);
      free(corpo);
      break;
    }
    if (corpo && jsonValido(corpo) && corpo[0] == '{') {
      if (a->catalogo && catalogTestados < 6 && !sessaoExpirada()) {
        char catalogUrl[800];
        char *catalogo;
        snprintf(catalogUrl, sizeof catalogUrl, "%s/catalog/movie/%s.json", base,
                 amostraTitulo(catalogTestados));
        controle = controleDiagnostico(2L * 1024L * 1024L);
        catalogo = rede_baixar_medido_controle(catalogUrl, DIAG_TIMEOUT_S, NULL, &controle, &medida);
        a->catalog_http = medida.status;
        a->catalog_ms = (int)medida.ms;
        a->catalog_ok = catalogo && !medida.limitado && jsonValido(catalogo);
        d.catalogMs += a->catalog_ms;
        if (a->catalog_ok) d.catalogOk++; else d.catalogFalhas++;
        catalogTestados++;
        if (medida.cancelado) atomic_store(&d.cancelado, 1);
        free(catalogo);
      }
      if (a->stream && streamTestados < 6 && !sessaoExpirada()) {
        char *streams;
        controle = controleDiagnostico(DIAG_STREAM_MAX);
        streams = baixarFontesAddon(i, amostraTitulo(streamTestados), &controle, &medida);
        a->stream_ms = (int)medida.ms;
        d.streamMs += a->stream_ms;
        if (fontesRespondeu(streams, &medida)) d.streamOk++;
        else d.streamFalhas++;
        streamTestados++;
        if (medida.cancelado) atomic_store(&d.cancelado, 1);
        free(streams);
      }
      if (d.nAssets < DIAG_MAX_ASSETS) {
        char asset[800];
        if (extrairUrl(corpo, "logo", asset, sizeof asset)) medirAsset(asset);
        if (extrairUrl(corpo, "poster", asset, sizeof asset)) medirAsset(asset);
        if (extrairUrl(corpo, "background", asset, sizeof asset)) medirAsset(asset);
      }
    }
    free(corpo);
    atomic_fetch_add(&d.feitos, 1);
    if (atomic_load(&d.cancelado)) break;
    if (sessaoExpirada()) { d.coberturaParcial = 1; break; }
  }
  if (!atomic_load(&d.cancelado)) {
    atomic_store(&d.fase, 2);
    medirFontesDeArte();
  }
  // Passa a vez ao fio de desenho (passes pelo cache e comparacao) e espera.
  atomic_store(&d.sondasProntas, 1);
  { Uint32 ini = SDL_GetTicks();
    while (!atomic_load(&d.passesProntos) && !atomic_load(&d.cancelado)) {
      if (SDL_GetTicks() - ini > DIAG_PASSES_MAX_MS) {
        // A tela saiu de cena no meio: sem veredito, o candidato nao fica.
        atomic_store(&d.cancelado, 1);
        if (desfazerExperimento()) d.aplicacao = DA_CANCELADO;
        break;
      }
      SDL_Delay(30);
    } }
  if (!atomic_load(&d.cancelado)) {
    atomic_store(&d.fase, 6);
    montarRelatorio();
    dados_gravar("diagnostico-otimizacao.txt", d.relatorio);
    printf("[diagnostico] relatorio inicio id=%s addons=%d assets=%d streams_ok=%d streams_falhas=%d aplicacao=%s\n",
           d.id, d.nAddon, d.nAssets, d.streamOk, d.streamFalhas, aplicacaoNome(d.aplicacao));
    printf("[diagnostico] relatorio fim\n");
    fflush(stdout);
    // O relatorio segue por um envio proprio, com a execucao correlacionada.
    // O log geral da sessao nao entra neste caminho. So conta como enviado
    // com o `registro_id` da resposta (avisos.c: extrairRegistroId).
    d.enviado = avisos_enviar_diagnostico(d.id, d.relatorio,
                                          d.registroId, sizeof d.registroId);
    d.envioFalhou = !d.enviado;
  }
  if (atomic_load(&d.cancelado)) atomic_store(&d.estado, 3);
  else atomic_store(&d.estado, 2);
  return 0;
}

static int envioWorker(void *arg) {
  (void)arg;
  d.enviado = avisos_enviar_diagnostico(d.id, d.relatorio,
                                        d.registroId, sizeof d.registroId);
  d.envioFalhou = !d.enviado;
  if (d.enviado) d.vazaoPendente = 0;
  atomic_store(&d.enviando, 0);
  return 0;
}

// ---------------------------------------------------------------------------
// TESTE DE VELOCIDADE: fio proprio (vazaoWorker), estado em `v`.
//
// 1. add-ons: o endpoint de fontes de cada addon ativo (baixarFontesAddon, o
//    mesmo do diagnostico), com o tempo de resposta de cada um; das fontes
//    que voltam ficam ate 3 por addon, na ordem da fonte automatica;
// 2. fontes: as candidatas de todos os addons, de novo na ordem da fonte
//    automatica (stream_pontos + fonteauto_fila, o modo de Ajustes), e ate 3
//    delas de HOSTS DIFERENTES — o link resolvido como a verificacao de fonte
//    resolve, e 8 s de corpo contados e descartados (rede_medir_vazao);
// 3. a conta (vazao_resumir): mediana e p20 de todas as amostras por segundo.
//
// SO LINK DIRETO: torrent sem url (so infoHash) exigiria mandar o debrid
// resolver, e fonte marcada fora de cache mandaria o servico BAIXAR (#130).
// Link de aviso (slate, downloading.mp4, clipe de erro) mediria o servidor de
// aviso. Os tres ficam de fora antes de qualquer pedido.

// Corta os proxyHeaders ("Nome: valor" por linha) num vetor para rede.h,
// como playlistVazia em streams.c. `copia` guarda as linhas.
static const char *const *vetorCabecalhos(const char *cab, char *copia, size_t n,
                                          const char **vet, int max) {
  char *l, *ctx = NULL;
  int k = 0;
  if (!cab || !*cab) return NULL;
  snprintf(copia, n, "%s", cab);
  for (l = strtok_r(copia, "\n", &ctx); l && k < max - 1; l = strtok_r(NULL, "\n", &ctx))
    vet[k++] = l;
  vet[k] = NULL;
  return k ? vet : NULL;
}

static int candidataMedivel(const Stream *s) {
  return s->url[0] && !strncmp(s->url, "http", 4) && !s->foraCache &&
         !vazao_url_aviso(s->url);
}

// Ate VAZ_POR_ADDON fontes deste addon, na ordem em que o automatico as
// tentaria. Devolve quantas entraram em vz.cand.
static int coletarCandidatas(const Stream *lista, int n) {
  long *pts;
  unsigned char *acima, *fora;
  int fila[VAZ_POR_ADDON], nf, q, entrou = 0;
  if (n < 1 || vz.nCand >= VAZ_MAX_CAND) return 0;
  pts = malloc(sizeof *pts * (size_t)n);
  acima = calloc((size_t)n, 1);
  fora = calloc((size_t)n, 1);
  if (!pts || !acima || !fora) { free(pts); free(acima); free(fora); return 0; }
  for (q = 0; q < n; q++) {
    pts[q] = stream_pontos(&lista[q]);
    acima[q] = (unsigned char)!stream_cabe_no_teto(&lista[q]);
    fora[q] = (unsigned char)!candidataMedivel(&lista[q]);
  }
  nf = fonteauto_fila(vz.modo, n, -1, pts, acima, fora, VAZ_POR_ADDON, fila);
  for (q = 0; q < nf && vz.nCand < VAZ_MAX_CAND; q++) {
    VazCand *c = &vz.cand[vz.nCand++];
    snprintf(c->url, sizeof c->url, "%s", lista[fila[q]].url);
    snprintf(c->cab, sizeof c->cab, "%s", lista[fila[q]].cabecalhos);
    c->pontos = pts[fila[q]];
    c->acima = acima[fila[q]];
    entrou++;
  }
  free(pts); free(acima); free(fora);
  return entrou;
}

static void vazaoAddons(void) {
  RedeControle ctl;
  int t, i;
  ctl.max_bytes = DIAG_STREAM_MAX;
  ctl.cancelado = (volatile int *)&vz.cancelado;
  for (t = 0; t < vz.nTitulo && !vz.nCand; t++) {
    for (i = 0; i < vz.nAddon; i++) {
      VazAddon *a = &vz.addon[i];
      RedeMedida m;
      Stream *lista = NULL;
      char *corpo;
      int n = 0;
      if (atomic_load(&vz.cancelado)) return;
      if (!addons_ativo(i) || (addons_sondado(i) && !addons_fornece(i, ADD_STREAM))) {
        if (t == 0) atomic_fetch_add(&vz.feitos, 1);
        continue;
      }
      corpo = baixarFontesAddon(i, vz.titulo[t], &ctl, &m);
      if (m.cancelado) { free(corpo); atomic_store(&vz.cancelado, 1); return; }
      if (fontesRespondeu(corpo, &m)) n = stream_extrair(corpo, addons_nome(i), &lista);
      if (n < 0) n = 0;
      // O tempo por addon e o do 1o titulo: o 2o so existe quando nenhum
      // addon trouxe fonte medivel, e somar os dois mediria outra coisa.
      if (t == 0) {
        a->medido = 1;
        a->ms = (int)m.ms;
        a->http = m.status;
        a->ok = fontesRespondeu(corpo, &m);
      }
      if (n > a->fontes) a->fontes = n;
      a->candidatas += coletarCandidatas(lista, n);
      if (t == 0) {
        printf("[vazao] addon %d: %d ms, HTTP %d, %d fontes, %d com link direto\n",
               i + 1, a->ms, a->http, n, a->candidatas);
        fflush(stdout);
        atomic_fetch_add(&vz.feitos, 1);
      }
      free(lista);
      free(corpo);
    }
  }
}

// Categoria da falha de uma medida, para a tela e o relatorio.
static VazResultado falhaDaMedida(const RedeVazao *r, int amostras) {
  if (r->cancelado) return VR_CANCELADO;
  if (r->status >= 400) return VR_RECUSADO;
  if (r->erro == -1) return VR_NAVEGADOR;
  if (!r->status) return VR_SEM_RESPOSTA;
  return amostras > 0 ? VR_OK : VR_CURTO;
}

static int medirUmaFonte(const VazCand *c, char hosts[][96], int nHosts) {
  char fim[4096], fim2[4096], copia[512], host[96];
  const char *vet[8];
  const char *const *cab = vetorCabecalhos(c->cab, copia, sizeof copia, vet, 8);
  const char *alvo = c->url;
  VazFonte *f = &vz.fonte[atomic_load(&vz.nFonte)];
  RedeVazao r;
  int n, k;
  VazResultado cat;
  vz.tentadas++;
  // RESOLVIDO COMO A VERIFICACAO DE FONTE RESOLVE (verificarUma em streams.c):
  // o link do addon redireciona para o CDN do debrid, e e esse final que diz
  // se e aviso e de qual host e. Fonte com proxyHeaders vai direto (a
  // resolucao nao manda cabecalho, e o CDN que confere Referer recusaria).
  if (!cab) {
    if (rede_url_final(c->url, DIAG_TIMEOUT_S, fim, sizeof fim)) alvo = fim;
    else {
#ifdef __EMSCRIPTEN__
      vz.falhas[VR_NAVEGADOR]++;
#else
      vz.falhas[VR_SEM_RESPOSTA]++;
#endif
      printf("[vazao] fonte %d/%d: o link nao resolveu\n", vz.tentadas, VAZ_TENTATIVAS);
      fflush(stdout);
      return 0;
    }
  }
  if (vazao_url_aviso(alvo)) {
    vz.falhas[VR_AVISO]++;
    printf("[vazao] fonte %d/%d: link de aviso, nao medida\n", vz.tentadas, VAZ_TENTATIVAS);
    fflush(stdout);
    return 0;
  }
  vazao_host(alvo, host, sizeof host);
  for (k = 0; k < nHosts; k++)
    if (!strcmp(hosts[k], host)) return -1;   // host ja medido: a proxima
  memset(f, 0, sizeof *f);
  vz.fonteIniMs = SDL_GetTicks();
  n = rede_medir_vazao(alvo, cab, VAZ_JANELA_S, VAZ_INICIO_BYTE, VAZ_TETO_BYTES,
                       (volatile int *)&vz.cancelado, f->kbps, VAZAO_SEG_MAX, &r,
                       fim2, sizeof fim2);
  // 416: o arquivo e menor que o deslocamento. Do comeco, entao.
  if (r.status == 416 && !r.cancelado)
    n = rede_medir_vazao(alvo, cab, VAZ_JANELA_S, 0, VAZ_TETO_BYTES,
                         (volatile int *)&vz.cancelado, f->kbps, VAZAO_SEG_MAX, &r,
                         fim2, sizeof fim2);
  if (r.cancelado) { atomic_store(&vz.cancelado, 1); return 0; }
  if (n > 0 && fim2[0] && vazao_url_aviso(fim2)) {
    vz.falhas[VR_AVISO]++;
    printf("[vazao] fonte %d/%d: redirecionou para aviso, descartada\n", vz.tentadas, VAZ_TENTATIVAS);
    fflush(stdout);
    return 0;
  }
  cat = falhaDaMedida(&r, n);
  if (cat != VR_OK) {
    vz.falhas[cat]++;
    printf("[vazao] fonte %d/%d: sem medida (HTTP %d, erro %d)\n", vz.tentadas,
           VAZ_TENTATIVAS, r.status, r.erro);
    fflush(stdout);
    return 0;
  }
  f->n = n;
  f->http = r.status;
  f->bytes = r.bytes;
  f->ms = r.ms;
  vazao_resumir(f->kbps, n, &f->r);
  for (k = 0; k < n && vz.nAmostra < VAZAO_AMOSTRAS_MAX; k++) vz.amostra[vz.nAmostra++] = f->kbps[k];
  snprintf(hosts[nHosts], 96, "%s", host);
  printf("[vazao] fonte %d/%d: %.1f Mbps mediana, p20 %.1f, %lu s, %lld MB\n",
         atomic_load(&vz.nFonte) + 1, VAZAO_FONTES_MAX, f->r.medianaKbps / 1000.0,
         f->r.p20Kbps / 1000.0, (f->ms + 500UL) / 1000UL, f->bytes / 1000000LL);
  fflush(stdout);
  atomic_fetch_add(&vz.nFonte, 1);
  atomic_fetch_add(&vz.feitos, 1);
  return 1;
}

static void vazaoFontes(void) {
  long pts[VAZ_MAX_CAND];
  unsigned char acima[VAZ_MAX_CAND];
  int fila[VAZ_MAX_CAND], nf, q;
  char hosts[VAZAO_FONTES_MAX][96];
  for (q = 0; q < vz.nCand; q++) { pts[q] = vz.cand[q].pontos; acima[q] = vz.cand[q].acima; }
  nf = fonteauto_fila(vz.modo, vz.nCand, -1, pts, acima, NULL, VAZ_MAX_CAND, fila);
  for (q = 0; q < nf; q++) {
    int nFonte = atomic_load(&vz.nFonte);
    if (nFonte >= VAZAO_FONTES_MAX || vz.tentadas >= VAZ_TENTATIVAS) break;
    if (atomic_load(&vz.cancelado)) break;
    // Orcamento: com uma fonte medida, nao comeca outra que passaria dos 45 s.
    if (nFonte > 0 && SDL_GetTicks() - vz.inicioMs > VAZ_ORCAMENTO_MS) break;
    medirUmaFonte(&vz.cand[fila[q]], hosts, nFonte);
  }
}

static int vazaoWorker(void *arg) {
  int k, maior = 0;
  (void)arg;
  atomic_store(&vz.fase, 1);
  vazaoAddons();
  if (!atomic_load(&vz.cancelado)) {
    atomic_store(&vz.fase, 2);
    vazaoFontes();
  }
  if (atomic_load(&vz.cancelado)) vz.resultado = VR_CANCELADO;
  else if (vazao_resumir(vz.amostra, vz.nAmostra, &vz.resumo)) vz.resultado = VR_OK;
  else {
    int algum = 0;
    for (k = 0; k < vz.nAddon; k++) if (vz.addon[k].medido) algum = 1;
    vz.resultado = !algum ? VR_SEM_ADDONS : !vz.nCand ? VR_SEM_FONTES : VR_SEM_RESPOSTA;
    // A falha que MAIS aconteceu diz o motivo; empate fica a primeira.
    for (k = VR_AVISO; k < VR_N; k++)
      if (vz.falhas[k] > maior) { maior = vz.falhas[k]; vz.resultado = (VazResultado)k; }
  }
  if (vz.resultado == VR_OK)
    printf("[vazao] resultado: otimo %.1f Mbps, maximo %.1f Mbps (mediana %.1f, p20 %.1f, %d amostras, %d fonte(s))\n",
           vz.resumo.otimoKbps / 1000.0, vz.resumo.maximoKbps / 1000.0,
           vz.resumo.medianaKbps / 1000.0, vz.resumo.p20Kbps / 1000.0, vz.nAmostra,
           atomic_load(&vz.nFonte));
  else
    printf("[vazao] resultado: sem medida (%d)\n", (int)vz.resultado);
  fflush(stdout);
  atomic_store(&vz.estado, atomic_load(&vz.cancelado) ? 3 : 2);
  return 0;
}

// Fio de desenho: copia o que o fio vai precisar (o catalogo recarrega, o
// ajuste pode mudar) e o dispara.
static void iniciarVazao(void) {
  int i;
  if (atomic_load(&vz.estado) == 1 || atomic_load(&d.estado) == 1 || d.fioSug) return;
  if (vz.fio) { SDL_WaitThread(vz.fio, NULL); vz.fio = NULL; }
  memset(&vz, 0, sizeof vz);
  vz.aberto = 1;
  vz.modo = ajustes_fonte_primeira() ? FONTEAUTO_PRIMEIRA : FONTEAUTO_MELHOR;
  // FILME do catalogo, que e o que /stream/movie/ responde; sem nenhum, um
  // classico que todo addon de fontes tem.
  for (i = 0; i < cat_n() && vz.nTitulo < VAZ_TITULOS - 1; i++) {
    const CatItem *c = cat_item(i);
    if (!c || strncmp(c->imdb, "tt", 2) || strcmp(c->tipo, "movie")) continue;
    snprintf(vz.titulo[vz.nTitulo++], sizeof vz.titulo[0], "%s", c->imdb);
  }
  snprintf(vz.titulo[vz.nTitulo++], sizeof vz.titulo[0], "%s", "tt0111161");
  vz.nAddon = addons_n();
  if (vz.nAddon > DIAG_MAX_ADDONS) vz.nAddon = DIAG_MAX_ADDONS;
  atomic_store(&vz.total, vz.nAddon + VAZAO_FONTES_MAX);
  vz.inicioMs = SDL_GetTicks();
  atomic_store(&vz.estado, 1);
  vz.fio = SDL_CreateThread(vazaoWorker, "nuvio-diag-vazao", NULL);
  if (!vz.fio) {
    vz.resultado = VR_SEM_RESPOSTA;
    atomic_store(&vz.estado, 3);
  }
}

// O teste acabou (fio de desenho). Com um relatorio de diagnostico ja
// montado, ele e remontado com a vazao e o "Enviar de novo" aparece: o
// relatorio que ja saiu nao a tinha.
static void concluirVazao(void) {
  if (vz.fio) { SDL_WaitThread(vz.fio, NULL); vz.fio = NULL; }
  if (atomic_load(&vz.estado) != 2 || atomic_load(&d.estado) != 2 || !d.relatorio[0] ||
      atomic_load(&d.enviando))
    return;
  montarRelatorio();
  dados_gravar("diagnostico-otimizacao.txt", d.relatorio);
  if (d.enviado) d.vazaoPendente = 1;
}

static void juntarFios(int esperarTodos) {
  if (d.fio && (esperarTodos || atomic_load(&d.estado) != 1)) {
    SDL_WaitThread(d.fio, NULL); d.fio = NULL;
  }
  if (d.fioEnvio && (esperarTodos || !atomic_load(&d.enviando))) {
    SDL_WaitThread(d.fioEnvio, NULL); d.fioEnvio = NULL;
  }
}

void diagnostico_iniciar(void) {
  // VOLTAR A TELA NO MEIO DE UM TESTE nao pode zerar o estado: o fio ainda
  // escreve em `d`. A barra lateral deixa sair durante o teste.
  if (atomic_load(&d.estado) == 1 || d.fioSug || atomic_load(&d.enviando) ||
      atomic_load(&vz.estado) == 1) {
    sairTela = 0;
    // O atalho com um teste de velocidade JA rodando (a barra lateral deixa
    // sair no meio): mostra o que roda, em vez de comecar outro.
    if (velocidadePedida && atomic_load(&vz.estado) == 1) {
      vz.aberto = 1;
      soVelocidade = 1;
    }
    velocidadePedida = 0;
    return;
  }
  juntarFios(1);
  free(d.cfgAntes);
  memset(&d, 0, sizeof d);
  if (vz.fio) { SDL_WaitThread(vz.fio, NULL); vz.fio = NULL; }
  memset(&vz, 0, sizeof vz);
  focoModo = 0;
  focoLinha = 0;
  sairTela = 0;
  d.intro = !apresentacaoVista();
  dados_uuid(d.id, sizeof d.id);
  soVelocidade = 0;
  if (velocidadePedida) {
    velocidadePedida = 0;
    // A apresentacao NAO e marcada como vista: ela explica o diagnostico, e
    // quem so mediu a rede ainda nao a leu.
    d.intro = 0;
    soVelocidade = 1;
    iniciarVazao();
  }
}

void diagnostico_abrir_velocidade(void) { velocidadePedida = 1; }

// ARRANQUE: o perfil aprovado volta a valer, e um experimento interrompido
// (checkpoint pendente) so e apagado — o candidato nunca foi gravado como
// aprovado, entao o que vale e o .cfg de antes dele.
void diagnostico_recuperar_checkpoint(void) {
  char *ck = dados_ler("diagnostico-otimizacao.checkpoint");
  char *cfg;
  if (ck) {
    printf("[diagnostico] experimento interrompido na sessao anterior: candidato descartado\n");
    dados_apagar("diagnostico-otimizacao.checkpoint");
    free(ck);
  }
  cfg = dados_ler("diagnostico-otimizacao.cfg");
  if (cfg) {
    PtvPerfil pf;
    long mem = 0;
    int fixo = 0;
    tex_orcamento_info(NULL, &mem, &fixo, NULL);
    if (ptv_ler(cfg, &pf)) {
      ptv_limitar(ptv_plataforma(), mem, &pf);
      aplicarPerfilTex(&pf, fixo);
      printf("[diagnostico] perfil aprovado aplicado: %d MB, %d fios, heroi %d\n",
             pf.texMb, pf.fiosRede, pf.heroiLarg);
    }
    free(cfg);
  }
}

static void iniciarTeste(void) {
  int intro = d.intro, i, t, f;
  if (atomic_load(&d.estado) == 1 || d.fioSug || atomic_load(&d.enviando)) return;
  // Um teste por vez: o diagnostico mediria a rede ocupada pela vazao.
  if (atomic_load(&vz.estado) == 1) return;
  juntarFios(1);
  free(d.cfgAntes);
  memset(&d, 0, sizeof d);
  d.intro = intro;
  d.inicioMs = SDL_GetTicks();
  dados_uuid(d.id, sizeof d.id);
  d.modo = focoModo ? DIAG_DESEMPENHO : DIAG_QUALIDADE;
  // AMOSTRA COPIADA AQUI, no fio de desenho: os tres primeiros titulos com id
  // do IMDb, e para cada um a url de cada fonte de arte (artehero devolve
  // buffer estatico, e o desenho da home o reutiliza a cada quadro).
  for (i = 0; i < cat_n() && d.nTitulo < DIAG_MAX_TITULOS; i++) {
    const CatItem *c = cat_item(i);
    if (!c || strncmp(c->imdb, "tt", 2)) continue;
    d.titulo[d.nTitulo++] = *c;
  }
  for (t = 0; t < d.nTitulo; t++) {
    const char *c = artehero_url_card_fonte(&d.titulo[t], ajustes_hero_fonte(),
                                            ajustes_hero_arte_diferente());
    snprintf(d.cardUrl[t], sizeof d.cardUrl[t], "%s", c ? c : "");
    for (f = 1; f <= PTV_FONTE_FUNDO_MAX; f++) {
      const char *u = artehero_url_fonte(&d.titulo[t], f);
      snprintf(d.fonteUrl[t][f], sizeof d.fonteUrl[t][f], "%s", u ? u : "");
    }
    if (d.titulo[t].logo[0]) {
      const char *u = artehero_url_logo(d.titulo[t].logo);
      snprintf(d.fonteUrl[t][PTV_FONTE_LOGO], sizeof d.fonteUrl[t][PTV_FONTE_LOGO], "%s", u ? u : "");
    }
  }
  atomic_store(&d.estado, 1);
  d.fio = SDL_CreateThread(diagnosticoWorker, "nuvio-diagnostico", NULL);
  if (!d.fio) {
    snprintf(d.erro, sizeof d.erro, "%s", "Não foi possível iniciar o teste");
    atomic_store(&d.estado, 4);
  }
}

// ---------------------------------------------------------------------------
// BOTOES DA TELA DE RESULTADO. O controle da TV nao tem letra nenhuma, e as
// coloridas ja tem dono (vermelha = painel de registro, verde = painel DOM no
// Tizen, azul = Salvos): a acao vira botao navegavel por esquerda/direita.

enum { B_RETESTAR, B_RESTAURAR, B_SUGESTAO, B_REENVIAR, B_VELOCIDADE, B_OBJETIVO, B_N };
static const char *const BOTAO_ROTULO[B_N] = {
  "Testar de novo", "Restaurar anterior", "Aplicar sugestão", "Enviar de novo",
  "Teste de velocidade", "Trocar objetivo"
};

static int botaoVisivel(int b) {
  switch (b) {
    case B_RESTAURAR: return d.aplicacao == DA_MANTIDO;
    case B_SUGESTAO: return d.sugEstado == DS_PROPOSTA;
    case B_REENVIAR: return (d.envioFalhou || d.vazaoPendente) && d.relatorio[0] &&
                            !atomic_load(&d.enviando);
    default: return 1;
  }
}

static int botoesVisiveis(int *lista) {
  int b, n = 0;
  for (b = 0; b < B_N; b++) if (botaoVisivel(b)) lista[n++] = b;
  return n;
}

static void acionarBotao(int b) {
  switch (b) {
    case B_RETESTAR: iniciarTeste(); break;
    case B_RESTAURAR: restaurarManual(); break;
    case B_SUGESTAO: aplicarSugestao(); break;
    case B_REENVIAR:
      atomic_store(&d.enviando, 1);
      d.fioEnvio = SDL_CreateThread(envioWorker, "nuvio-diag-envio", NULL);
      if (!d.fioEnvio) atomic_store(&d.enviando, 0);
      break;
    case B_VELOCIDADE: iniciarVazao(); break;
    case B_OBJETIVO: atomic_store(&d.estado, 0); focoLinha = 0; break;
  }
  d.botao = 0;
}

static int teclaVoltar(const SDL_Event *e) {
  SDL_Keycode k = e->key.keysym.sym;
  return k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE ||
         e->key.keysym.scancode == NV_SCANCODE_BACK;
}

void diagnostico_evento(const SDL_Event *e) {
  SDL_Keycode k;
  int estado;
  if (!e || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  estado = atomic_load(&d.estado);
  if (d.intro) {
    if (teclaVoltar(e)) {
      sairTela = 1;
    } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
      marcarApresentacaoVista();
      d.intro = 0;
    }
    return;
  }
  // TESTE DE VELOCIDADE NA TELA: Voltar cancela o que roda, ou fecha o
  // resultado e volta ao que estava (objetivo ou resultado do diagnostico).
  if (vz.aberto) {
    int ve = atomic_load(&vz.estado);
    if (teclaVoltar(e)) {
      if (ve == 1) atomic_store(&vz.cancelado, 1);
      else {
        vz.aberto = 0;
        // Aberta pelo atalho: nao ha objetivo nem resultado atras, volta a
        // Ajustes (app.c).
        if (soVelocidade) { soVelocidade = 0; sairTela = 1; }
      }
    } else if ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && ve != 1) {
      iniciarVazao();
    }
    return;
  }
  if (teclaVoltar(e)) {
    if (estado == 1 || d.sugEstado == DS_TESTANDO) atomic_store(&d.cancelado, 1);
    else sairTela = 1;
    return;
  }
  if (estado == 1 || d.sugEstado == DS_TESTANDO) return;
  if (estado == 2) {
    int lista[B_N], n = botoesVisiveis(lista);
    if (d.botao >= n) d.botao = n - 1;
    if (k == SDLK_LEFT && d.botao > 0) { d.botao--; return; }
    if (k == SDLK_RIGHT && d.botao < n - 1) { d.botao++; return; }
    if ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && n > 0) acionarBotao(lista[d.botao]);
    return;
  }
  if (estado == 0) {
    if (k == SDLK_DOWN) { focoLinha = 1; return; }
    if (k == SDLK_UP) { focoLinha = 0; return; }
    if (focoLinha == 1) {
      if (k == SDLK_RETURN || k == SDLK_KP_ENTER) iniciarVazao();
      return;
    }
  }
  if (k == SDLK_LEFT || k == SDLK_RIGHT) { focoModo = !focoModo; return; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER) iniciarTeste();
}

void diagnostico_atualizar(float dt, Uint32 agora) {
  (void)agora;
  juntarFios(0);
  if (vz.fio && atomic_load(&vz.estado) != 1) concluirVazao();
  if (d.passe) {
    int ms = (int)(dt * 1000.0f + 0.5f);
    if (ms > d.passePior) d.passePior = ms;
  }
  if (atomic_load(&d.estado) == 1) {
    if (atomic_load(&d.cancelado)) {
      // Voltar no meio: o candidato sai antes de qualquer outra coisa.
      if (desfazerExperimento()) d.aplicacao = DA_CANCELADO;
      d.passe = 0;
      atomic_store(&d.passesProntos, 1);
    } else if (atomic_load(&d.sondasProntas) && !atomic_load(&d.passesProntos)) {
      if (!d.sugCalculada) { d.sugCalculada = 1; calcularSugestao(); }
      passesAvancar();
    }
  }
  if (d.sugEstado == DS_TESTANDO && atomic_load(&d.sugPronta)) concluirSugestao();
}

// ---------------------------------------------------------------------------
// DESENHO.

static void miniCartao(GfxRect r, int tipo, const char *rotulo) {
  TxtLinha t;
  gfx_cor(r, 18.0f / r.h, 0.11f, 0.12f, 0.15f, 1.0f);
  if (tipo == 0) {
    gfx_cor((GfxRect){ r.x + 18, r.y + 18, r.w * 0.34f, r.h - 36 }, 8.0f / r.h, 0.34f, 0.46f, 0.70f, 1.0f);
    gfx_cor((GfxRect){ r.x + r.w * 0.42f, r.y + 28, r.w * 0.48f, 10 }, 0.5f, 0.86f, 0.88f, 0.92f, 0.8f);
    gfx_cor((GfxRect){ r.x + r.w * 0.42f, r.y + 54, r.w * 0.34f, 8 }, 0.5f, 0.45f, 0.55f, 0.68f, 0.8f);
  } else if (tipo == 1) {
    int i;
    for (i = 0; i < 5; i++) {
      float bh = 24.0f + (float)((i * 17) % 54);
      gfx_cor((GfxRect){ r.x + 24.0f + i * 34.0f, r.y + r.h - 34.0f - bh, 20.0f, bh },
              0.25f, 0.30f + i * 0.06f, 0.68f, 0.70f + i * 0.04f, 1.0f);
    }
    gfx_cor((GfxRect){ r.x + 20, r.y + r.h - 30, r.w - 40, 2 }, 0.5f, 0.54f, 0.58f, 0.66f, 0.9f);
  } else {
    int i;
    for (i = 0; i < 7; i++)
      gfx_cor((GfxRect){ r.x + 18.0f + i * (r.w - 36.0f) / 7.0f, r.y + 28,
                         (r.w - 54.0f) / 7.0f, r.h - 56 },
              5.0f / r.h, i == 3 ? 0.75f : 0.28f, i == 3 ? 0.58f : 0.34f,
              i == 3 ? 0.38f : 0.42f, 1.0f);
  }
  t = txt_linha_corta(TXT_CAPTION, i18n(rotulo), 206, 210, 220, 255, r.w);
  txt_desenhar(t, r.x, r.y + r.h + 14.0f);
}

/* A tela usa as mesmas primitivas vetoriais do restante do app. Elas sao
 * equivalentes aos SVGs da versao web, mas nao dependem de imagem ou fonte
 * externa para aparecer no primeiro quadro da TV. */
static void painel(GfxRect r, float ar, float ag, float ab) {
  gfx_cor(r, 22.0f / r.h, 0.055f, 0.065f, 0.085f, 0.97f);
  gfx_luz_canto(r, 26.0f / r.h, 150.0f, -70.0f, 500.0f, ar, ag, ab, 0.11f);
  gfx_cor((GfxRect){ r.x, r.y, r.w, 2.0f }, 1.0f, ar, ag, ab, 0.48f);
}

static void painelTitulo(GfxRect r, const char *titulo, const char *subtitulo) {
  txt_desenhar(txt_linha_corta(TXT_BODY, i18n(titulo), 238, 242, 248, 255, r.w - 56.0f),
               r.x + 28.0f, r.y + 22.0f);
  if (subtitulo)
    txt_desenhar(txt_linha_corta(TXT_CAPTION, i18n(subtitulo), 154, 164, 178, 255, r.w - 56.0f),
                 r.x + 28.0f, r.y + 58.0f);
}

static float limitePct(float n) {
  if (n < 0.0f) return 0.0f;
  if (n > 100.0f) return 100.0f;
  return n;
}

static void barraProgresso(GfxRect r, float pct, float ar, float ag, float ab,
                           int animado, Uint32 agora) {
  float w = r.w * limitePct(pct) / 100.0f;
  gfx_cor(r, 9.0f / r.h, 0.10f, 0.12f, 0.15f, 1.0f);
  if (w > 1.0f)
    gfx_cor((GfxRect){ r.x, r.y, w, r.h }, 9.0f / r.h, ar, ag, ab, 0.96f);
  // O BRILHO QUE CORRE fica DENTRO do preenchimento (dono, 26/09/2026: "o
  // trem que mexe comeca fora da barra e termina depois, ta descasado"). Ele
  // andava de 80 px antes da barra ate 80 px depois do fim do preenchido, sem
  // recorte, e era mais baixo e com outro raio que a barra. Agora o trecho e
  // cortado nas duas pontas do preenchido e tem a altura e o raio dela: le
  // como luz passando pelo material, nao como uma peca solta por cima.
  if (animado && w > 24.0f) {
    float larg = w < 240.0f ? w * 0.35f : 84.0f;
    float x0 = r.x - larg + fmodf((float)agora * 0.24f, w + larg);
    float x1 = x0 + larg;
    if (x0 < r.x) x0 = r.x;
    if (x1 > r.x + w) x1 = r.x + w;
    if (x1 - x0 > 1.0f)
      gfx_cor((GfxRect){ x0, r.y, x1 - x0, r.h }, 9.0f / r.h, 1.0f, 1.0f, 1.0f, 0.16f);
  }
}

// Barras com o ROTULO DE CADA UMA embaixo dela, centrado. O rotulo unico com
// espacos ("Manifestos   Catalogos...") nao casava com as barras em ingles.
static void graficoBarras(GfxRect r, const int *valores, const char *const *rotulos,
                          int n, float ar, float ag, float ab) {
  int i, maior = 1;
  float largura;
  if (n < 1) return;
  for (i = 0; i < n; i++) if (valores[i] > maior) maior = valores[i];
  largura = (r.w - (float)(n - 1) * 24.0f) / (float)n;
  gfx_cor((GfxRect){ r.x, r.y + r.h - 2.0f, r.w, 2.0f }, 1.0f, 0.25f, 0.29f, 0.36f, 0.85f);
  for (i = 0; i < n; i++) {
    float h = (r.h - 12.0f) * (float)valores[i] / (float)maior;
    float x = r.x + i * (largura + 24.0f);
    TxtLinha l = txt_linha_corta(TXT_CAPTION, i18n(rotulos[i]), 150, 160, 174, 255, largura);
    if (h < 3.0f && valores[i] > 0) h = 3.0f;
    gfx_cor((GfxRect){ x, r.y + r.h - h, largura, h }, 7.0f / (h > 7.0f ? h : 7.0f), ar, ag, ab,
            valores[i] == maior ? 0.98f : 0.58f);
    txt_desenhar(l, x + (largura - l.w) * 0.5f, r.y + r.h + 10.0f);
  }
}

// ROTULO A ESQUERDA, VALOR ALINHADO A DIREITA DO PAINEL, e cada um com a sua
// largura maxima. O valor antes comecava num x fixo (r.w - 290) e crescia para
// a direita: "300 MB · 0 pendentes · 0 quentes · 450 slots" atravessava a
// borda e encavalava no painel vizinho (captura da C9, 22/09).
static void metrica(GfxRect r, float y, const char *rotulo, const char *valor,
                    int cr, int cg, int cb) {
  float colValor = r.w * 0.52f - 28.0f;
  TxtLinha v = txt_linha_corta(TXT_BODY, valor, cr, cg, cb, 255, colValor);
  txt_desenhar(txt_linha_corta(TXT_CAPTION, i18n(rotulo), 154, 164, 178, 255,
                               r.w - 56.0f - v.w - 24.0f),
               r.x + 28.0f, y + 3.0f);
  txt_desenhar(v, r.x + r.w - 28.0f - v.w, y);
}

static const char *gargaloPrincipal(void) {
  int artes = d.assetsMs + d.fontesMs;
  int maior = artes;
  const char *nome = "Artes, logos e fundos";
  if (d.streamMs > maior) { maior = d.streamMs; nome = "Fontes de vídeo"; }
  if (d.catalogMs > maior) { maior = d.catalogMs; nome = "Catálogos"; }
  if (d.manifestMs > maior) { maior = d.manifestMs; nome = "Manifestos"; }
  return maior > 0 ? nome : "Sem gargalo identificado";
}

static int coberturaPercentual(void) {
  int total = atomic_load(&d.total);
  int feito = atomic_load(&d.feitos);
  if (d.coberturaParcial && total > 0) return (feito * 100) / total;
  return total > 0 ? 100 : 0;
}

// A folha centra na area A DIREITA DA RAIL quando e aberta de dentro da tela
// (a rail fixa e desenhada por cima dela, depois). A da primeira abertura vem
// de diagnostico_intro_desenhar, que cobre tudo inclusive a rail: essa centra
// na tela inteira.
static void desenharApresentacao(int primeiraAbertura) {
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  float rail = primeiraAbertura ? 0.0f : ajustes_rail_largura_fixa();
  GfxRect cartao = { rail + (NV_TELA_W - rail - 1620.0f) * 0.5f, 136.0f, 1620.0f, 808.0f };
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  gfx_cor(tela, 0.0f, 0, 0, 0, 0.78f);
  gfx_cor(cartao, 30.0f / cartao.h, 0.055f, 0.058f, 0.068f, 0.98f);
  gfx_luz_canto(cartao, 30.0f / cartao.h, 160.0f, -80.0f, 620.0f, ar, ag, ab, 0.20f);
  txt_desenhar(txt_linha(TXT_TITULO2, i18n("Antes de começar"), 255, 255, 255, 255),
               cartao.x + 64.0f, cartao.y + 42.0f);
  txt_desenhar(txt_linha(TXT_BODY, i18n("O que será medido"), 178, 182, 190, 255),
               cartao.x + 64.0f, cartao.y + 124.0f);
  miniCartao((GfxRect){ cartao.x + 64.0f, cartao.y + 180.0f, 320.0f, 142.0f }, 0, "Artes, logos e fundos");
  miniCartao((GfxRect){ cartao.x + 412.0f, cartao.y + 180.0f, 320.0f, 142.0f }, 1, "Tempos, bytes e falhas");
  miniCartao((GfxRect){ cartao.x + 760.0f, cartao.y + 180.0f, 320.0f, 142.0f }, 2, "Fontes de vídeo");
  txt_bloco(TXT_BODY, i18n("O app lê manifestos e catálogos, testa artes e fontes de vídeo e compara o caminho de carregamento desta TV."),
            192, 196, 206, cartao.x + 64.0f, cartao.y + 388.0f, cartao.w - 128.0f, 34.0f, 1, 3);
  txt_bloco(TXT_BODY, i18n("Haverá testes de rede e consumo de dados. O relatório técnico será enviado automaticamente ao suporte."),
            214, 190, 170, cartao.x + 64.0f, cartao.y + 500.0f, cartao.w - 128.0f, 34.0f, 1, 3);
  txt_desenhar(txt_linha(TXT_CAPTION, i18n("Não altera assistidos, histórico, progresso ou scrobbling."), 164, 168, 178, 255),
               cartao.x + 64.0f, cartao.y + 650.0f);
  txt_desenhar(txt_linha(TXT_CAPTION,
                         i18n(primeiraAbertura ? "OK continua · Voltar fecha" : "OK começa · Voltar cancela"),
                         172, 176, 184, 255),
               cartao.x + 64.0f, cartao.y + cartao.h - 48.0f);
}

void diagnostico_intro_primeira_vez(void) {
  if (introDecidido) return;
  introDecidido = 1;
  introGlobal = !apresentacaoVista();
}

int diagnostico_intro_aberto(void) { return introGlobal; }

void diagnostico_intro_dispensar(int marcarVista) {
  introDecidido = 1;
  introGlobal = 0;
  if (marcarVista) marcarApresentacaoVista();
}

void diagnostico_intro_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!introGlobal || !e || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    marcarApresentacaoVista();
    introGlobal = 0;
  } else if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE ||
             k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    introGlobal = 0;
  }
}

void diagnostico_intro_atualizar(float dt, Uint32 agora) {
  (void)dt;
  (void)agora;
}

void diagnostico_intro_desenhar(Uint32 agora) {
  (void)agora;
  if (introGlobal) desenharApresentacao(1);
}

// "128 → 300 MB": o antes e o depois de um parametro, com a unidade traduzida.
static void antesDepois(char *v, size_t cap, int a, int b, const char *unidade) {
  if (a == b) snprintf(v, cap, "%d %s", b, i18n(unidade));
  else snprintf(v, cap, "%d → %d %s", a, b, i18n(unidade));
}

static void linhasDoPerfil(GfxRect r, float y, float passo, const PtvPerfil *a,
                           const PtvPerfil *b) {
  char v[96];
  antesDepois(v, sizeof v, a->texMb, b->texMb, "MB");
  metrica(r, y, "Memória para imagens", v, 220, 226, 236);
  antesDepois(v, sizeof v, a->fiosRede, b->fiosRede, "fios");
  metrica(r, y + passo, "Fios de rede das artes", v, 220, 226, 236);
  antesDepois(v, sizeof v, a->heroiLarg, b->heroiLarg, "px");
  metrica(r, y + 2.0f * passo, "Largura do fundo em tela cheia", v, 220, 226, 236);
}

// AREA UTIL DA TELA (26/09, "no diagnostico tem varias"). Todos os paineis
// foram medidos na tela inteira, entre as margens de 80 do tvOS; com a rail
// FIXA os 144 da esquerda sao dela, e o painel da esquerda, o titulo e os
// botoes nasciam embaixo. x0/W vem de ajustes_area_conteudo: recolhida e o
// 80..1840 de sempre, fixa e 224..1840 — e as colunas se repartem a partir de
// W em vez de numeros cravados.
static float areaX(void) { float x; ajustes_area_conteudo(NV_MARGEM_X, NV_MARGEM_X, &x, NULL); return x; }
static float areaW(void) { float w; ajustes_area_conteudo(NV_MARGEM_X, NV_MARGEM_X, NULL, &w); return w; }
// Duas colunas com o vao de 80 de sempre: 840 + 80 + 840 na tela inteira.
static float colunaW(void) { return (areaW() - 80.0f) * 0.5f; }

static void desenharBotoes(float y, float ar, float ag, float ab) {
  int lista[B_N], n = botoesVisiveis(lista), i;
  float x = areaX(), soma = 0.0f, folga = 64.0f, vao = 20.0f;
  if (d.botao >= n) d.botao = n > 0 ? n - 1 : 0;
  // COM SEIS BOTOES (sugestao, restaurar e reenvio juntos, raro) a fileira
  // passava da margem direita: mede antes e encolhe o respiro para caber.
  for (i = 0; i < n; i++)
    soma += (float)txt_linha(TXT_BODY, i18n(BOTAO_ROTULO[lista[i]]), 232, 236, 244, 255).w;
  if (soma + (float)n * folga + (float)(n - 1) * vao > areaW()) {
    folga = 36.0f;
    vao = 12.0f;
  }
  for (i = 0; i < n; i++) {
    int foco = i == d.botao;
    TxtLinha t = txt_linha(TXT_BODY, i18n(BOTAO_ROTULO[lista[i]]),
                           foco ? 16 : 232, foco ? 18 : 236, foco ? 22 : 244, 255);
    float w = (float)t.w + folga;
    if (foco) gfx_cor((GfxRect){ x, y, w, 60.0f }, 0.5f, ar, ag, ab, 1.0f);
    else gfx_cor((GfxRect){ x, y, w, 60.0f }, 0.5f, 0.13f, 0.15f, 0.19f, 1.0f);
    txt_desenhar(t, x + folga * 0.5f, y + (60.0f - (float)t.h) * 0.5f);
    x += w + vao;
  }
}

static const char *textoAplicacao(int *cor) {
  *cor = 0;
  switch (d.aplicacao) {
    case DA_MANTIDO: *cor = 1; return "Perfil aplicado: o reteste não piorou";
    case DA_RESTAURADO_AUTO: *cor = 2; return "Restaurado sozinho: o reteste piorou";
    case DA_IGUAL: *cor = 1; return "Esta TV já está no perfil escolhido";
    case DA_SEM_AMOSTRA: *cor = 2; return "Sem artes para comparar: nada foi aplicado";
    case DA_SEM_CHECKPOINT: *cor = 2; return "Não foi possível salvar o checkpoint";
    case DA_DESLIGADO: return "Aplicação automática desligada neste build";
    case DA_CANCELADO: *cor = 2; return "Cancelado: configuração anterior restaurada";
    case DA_RESTAURADO_MANUAL: *cor = 1; return "Configuração anterior restaurada";
    case DA_RUIDO: *cor = 1; return "Diferença dentro do ruído: o perfil atual foi mantido";
    default: return "Nada foi aplicado";
  }
}

static const char *nomeFonteAjuste(int f) {
  return f <= 0 ? "Automático" : ptv_fonte_rotulo(f);
}

static void desenharFontes(GfxRect r, float ar, float ag, float ab) {
  int f, maior = 1, lenta = 0, msLenta = -1;
  char v[96];
  int linhas = 0, linha = 0;
  float passo;
  painelTitulo(r, "Artes por fonte", "Tempo médio por arte: consulta e download");
  for (f = 1; f < PTV_N_FONTES; f++) {
    int ms = ptv_fonte_ms(&d.fonte[f]);
    if (ms > maior) maior = ms;
    if (ms > msLenta) { msLenta = ms; lenta = f; }
    if (d.fonte[f].ok || d.fonte[f].falhas) linhas++;
  }
  // NOVE FONTES NAO CABEM a 36 px nos 180 px acima da proposta: so as que
  // foram medidas (sem chave do fanart.tv, sem anime, sem ano para a Apple,
  // a linha nem existe), com o passo encolhendo ate 20 px.
  passo = linhas > 5 ? 180.0f / (float)linhas : 36.0f;
  if (passo < 20.0f) passo = 20.0f;
  for (f = 1; f < PTV_N_FONTES; f++) {
    const PtvFonte *s = &d.fonte[f];
    int ms = ptv_fonte_ms(s);
    float y;
    if (!s->ok && !s->falhas && linhas > 0) continue;
    y = r.y + 90.0f + (float)(linha++) * passo;
    float bx = r.x + 190.0f, bw = r.w * 0.34f;
    int destaque = f == lenta && ms > 0;
    txt_desenhar(txt_linha_corta(TXT_CAPTION, i18n(ptv_fonte_rotulo(f)), destaque ? 244 : 190,
                                 destaque ? 206 : 198, destaque ? 150 : 210, 255, 150.0f),
                 r.x + 28.0f, y + 2.0f);
    gfx_cor((GfxRect){ bx, y + 8.0f, bw, 12.0f }, 0.5f, 0.10f, 0.12f, 0.15f, 1.0f);
    if (ms > 0)
      gfx_cor((GfxRect){ bx, y + 8.0f, bw * (float)ms / (float)maior, 12.0f }, 0.5f,
              destaque ? 0.95f : ar, destaque ? 0.62f : ag, destaque ? 0.36f : ab, 0.95f);
    if (!s->ok && !s->falhas) snprintf(v, sizeof v, "%s", i18n("não medida"));
    else if (ms < 0) snprintf(v, sizeof v, "%s", i18n("falhou"));
    else if (s->iguais > 0 && f <= PTV_FONTE_FUNDO_MAX)
      snprintf(v, sizeof v, i18n("%d ms · igual ao card"), ms);
    else if (s->largura > 0)
      snprintf(v, sizeof v, i18n("%d ms · %d×%d · %d falhas"), ms, s->largura, s->altura, s->falhas);
    else snprintf(v, sizeof v, i18n("%d ms · %d falhas"), ms, s->falhas);
    { TxtLinha t = txt_linha_corta(TXT_CAPTION, v, 210, 216, 226, 255, r.w - (bx - r.x) - bw - 48.0f);
      txt_desenhar(t, r.x + r.w - 28.0f - t.w, y + 2.0f); }
  }
  // A PROPOSTA ESCRITA ANTES DE QUALQUER MUDANCA: o que esta lento, quanto, e
  // o que o botao vai trocar. O ajuste so muda no OK do botao.
  { float y = r.y + 276.0f, lw = r.w - 56.0f;
    char a[200], b[200];
    if (d.sugEstado == DS_PROPOSTA || d.sugEstado == DS_TESTANDO) {
      if (d.sug.motivo == PTV_MOTIVO_IGUAL && d.sug.alvo > 0)
        snprintf(a, sizeof a, i18n("Destaque em %s repete a imagem do card; %s é outra arte, %d ms."),
                 i18n(ptv_fonte_rotulo(d.sug.lenta)), i18n(ptv_fonte_rotulo(d.sug.alvo)),
                 ptv_fonte_ms(&d.fonte[d.sug.alvo]));
      else
        snprintf(a, sizeof a, i18n("Destaque em %s: %d ms por arte; %s: %d ms."),
                 i18n(ptv_fonte_rotulo(d.sug.lenta)), d.sug.msLenta,
                 i18n(ptv_fonte_rotulo(d.sug.base)), d.sug.msBase);
      if (!d.sug.diferente)
        snprintf(b, sizeof b, "%s", i18n("Vai mudar: Destaque com outra arte, de Ligado para Desligado."));
      else
        snprintf(b, sizeof b, i18n("Vai mudar: Background do hero, de %s para %s."),
                 i18n(nomeFonteAjuste(ajustes_hero_fonte())), i18n(nomeFonteAjuste(d.sug.fonte)));
      if (d.sugEstado == DS_TESTANDO) snprintf(b, sizeof b, "%s", i18n("Retestando a arte do destaque…"));
      txt_desenhar(txt_linha_corta(TXT_CAPTION, a, 244, 206, 150, 255, lw), r.x + 28.0f, y);
      txt_desenhar(txt_linha_corta(TXT_CAPTION, b, 214, 220, 230, 255, lw), r.x + 28.0f, y + 28.0f);
    } else if (d.sugEstado == DS_MANTIDA) {
      txt_desenhar(txt_linha_corta(TXT_CAPTION, i18n("Sugestão aplicada: o destaque ficou mais rápido no reteste."),
                                   170, 220, 190, 255, lw), r.x + 28.0f, y + 14.0f);
    } else if (d.sugEstado == DS_DESFEITA) {
      snprintf(a, sizeof a, i18n("Sugestão desfeita sozinha: %s"), i18n(d.sugMotivo ? d.sugMotivo : ""));
      txt_desenhar(txt_linha_corta(TXT_CAPTION, a, 240, 170, 150, 255, lw), r.x + 28.0f, y + 14.0f);
    } else {
      txt_desenhar(txt_linha_corta(TXT_CAPTION, i18n("A fonte do destaque não está mais lenta que a do card."),
                                   170, 178, 190, 255, lw), r.x + 28.0f, y + 14.0f);
    }
  }
}

// ---------------------------------------------------------------------------
// TELA DO TESTE DE VELOCIDADE. Dois paineis: a esquerda o que interessa ao
// dono (velocidade, otimo, maximo, dica), a direita de onde veio (tempo de
// cada addon e vazao de cada fonte).

static char sepDecimal(void) { return ajustes_idioma_ingles() ? '.' : ','; }

static const char *textoFalhaVazao(VazResultado r, const char **detalhe) {
  *detalhe = NULL;
  switch (r) {
    case VR_SEM_ADDONS: return "Nenhum add-on de fontes ativo para testar.";
    case VR_SEM_FONTES:
      *detalhe = "Torrents sem link e fontes fora de cache no debrid não são baixados.";
      return "Nenhuma fonte com link direto para medir.";
    case VR_AVISO:
      *detalhe = "O link levou a um vídeo de aviso: expirado ou fora de cache.";
      return "As fontes responderam com vídeo de aviso.";
    case VR_RECUSADO: return "O servidor da fonte recusou o download.";
    case VR_NAVEGADOR:
      *detalhe = "O servidor da fonte não libera acesso pelo app da Samsung (CORS). Nenhum número foi estimado.";
      return "Não foi possível medir o stream pelo navegador da TV.";
    case VR_CURTO: return "A fonte respondeu, mas não entregou dados suficientes.";
    case VR_CANCELADO: return "O teste foi cancelado.";
    default: return "A fonte não respondeu a tempo.";
  }
}

// "até 12 GB por filme de 2 h · 4,5 GB por episódio", para `kbps`.
static void linhaTamanho(char *dst, size_t n, const char *formatoTraduzido, int kbps) {
  char filme[16], ep[16];
  char sep = sepDecimal();
  vazao_fmt_gb(filme, sizeof filme, vazao_gb(kbps, VAZAO_FILME_S), sep);
  vazao_fmt_gb(ep, sizeof ep, vazao_gb(kbps, VAZAO_EPISODIO_S), sep);
  snprintf(dst, n, formatoTraduzido, filme, ep);
}

static void desenharVazaoResumo(GfxRect r, float ar, float ag, float ab, Uint32 agora) {
  int ve = atomic_load(&vz.estado);
  char a[200], b[32], c[32];
  float lw = r.w - 56.0f;
  if (ve == 1) {
    int feito = atomic_load(&vz.feitos), total = atomic_load(&vz.total);
    int fase = atomic_load(&vz.fase), nf = atomic_load(&vz.nFonte);
    float pct = total > 0 ? 100.0f * (float)feito / (float)total : 0.0f;
    // Dentro da fonte que esta sendo medida, o tempo da janela anda a barra.
    if (fase == 2 && nf < VAZAO_FONTES_MAX && vz.fonteIniMs && total > 0) {
      float dentro = (float)(SDL_GetTicks() - vz.fonteIniMs) / (VAZ_JANELA_S * 1000.0f);
      if (dentro > 1.0f) dentro = 1.0f;
      pct += 100.0f * dentro / (float)total;
    }
    if (pct > 99.0f) pct = 99.0f;
    painelTitulo(r, "Teste de velocidade", fase <= 1 ? "Perguntando as fontes a cada add-on"
                                                      : "Baixando um trecho de cada fonte");
    snprintf(a, sizeof a, "%d%%", (int)pct);
    txt_desenhar(txt_linha(TXT_TITULO2, a, 246, 249, 255, 255), r.x + 28.0f, r.y + 100.0f);
    barraProgresso((GfxRect){ r.x + 28.0f, r.y + 190.0f, r.w - 56.0f, 20.0f }, pct, ar, ag, ab, 1, agora);
    snprintf(a, sizeof a, i18n("%d de %d"), feito < vz.nAddon ? feito : vz.nAddon, vz.nAddon);
    metrica(r, r.y + 246.0f, "Add-ons", a, 214, 220, 230);
    snprintf(a, sizeof a, i18n("%d de %d"), nf, VAZAO_FONTES_MAX);
    metrica(r, r.y + 290.0f, "Fontes medidas", a, 214, 220, 230);
    txt_bloco(TXT_CAPTION, i18n("Baixa por 8 s até 3 fontes de servidores diferentes, começando no meio do arquivo; os dados são descartados."),
              170, 178, 190, r.x + 28.0f, r.y + r.h - 110.0f, lw, 30.0f, 1, 2);
    return;
  }
  if (vz.resultado != VR_OK) {
    const char *det, *txt = textoFalhaVazao(vz.resultado, &det);
    painelTitulo(r, "Teste de velocidade", "Sem medida");
    txt_bloco(TXT_BODY, i18n(txt), 244, 196, 150, r.x + 28.0f, r.y + 110.0f, lw, 36.0f, 1, 2);
    if (det) txt_bloco(TXT_CAPTION, i18n(det), 190, 196, 206, r.x + 28.0f, r.y + 176.0f, lw, 30.0f, 1, 3);
    snprintf(a, sizeof a, i18n("%d de %d"), vz.tentadas, VAZ_TENTATIVAS);
    metrica(r, r.y + 290.0f, "Fontes tentadas", a, 214, 220, 230);
    return;
  }
  painelTitulo(r, "Teste de velocidade", "Tamanho de arquivo que toca sem parar nesta TV");
  vazao_fmt_mbps(b, sizeof b, vz.resumo.medianaKbps, sepDecimal());
  vazao_fmt_mbps(c, sizeof c, vz.resumo.p20Kbps, sepDecimal());
  snprintf(a, sizeof a, i18n("Velocidade: %s Mbps (pior trecho %s Mbps)"), b, c);
  txt_desenhar(txt_linha_corta(TXT_BODY, a, 238, 242, 248, 255, lw), r.x + 28.0f, r.y + 104.0f);

  linhaTamanho(a, sizeof a, i18n("Ótimo: até %s GB por filme de 2 h · %s GB por episódio"), vz.resumo.otimoKbps);
  txt_desenhar(txt_linha_corta(TXT_BODY, a, 170, 222, 190, 255, lw), r.x + 28.0f, r.y + 170.0f);
  vazao_fmt_mbps(b, sizeof b, vz.resumo.otimoKbps, sepDecimal());
  snprintf(a, sizeof a, i18n("Arquivos de até %s Mbps tocam sem parar, mesmo nos piores trechos."), b);
  txt_desenhar(txt_linha_corta(TXT_CAPTION, a, 170, 178, 190, 255, lw), r.x + 28.0f, r.y + 210.0f);

  linhaTamanho(a, sizeof a, i18n("Máximo: até %s GB por filme de 2 h · %s GB por episódio"), vz.resumo.maximoKbps);
  txt_desenhar(txt_linha_corta(TXT_BODY, a, 244, 214, 150, 255, lw), r.x + 28.0f, r.y + 270.0f);
  vazao_fmt_mbps(b, sizeof b, vz.resumo.maximoKbps, sepDecimal());
  snprintf(a, sizeof a, i18n("Até %s Mbps toca, mas pode pausar quando a rede cai."), b);
  txt_desenhar(txt_linha_corta(TXT_CAPTION, a, 170, 178, 190, 255, lw), r.x + 28.0f, r.y + 310.0f);

  txt_desenhar(txt_linha_corta(TXT_BODY, i18n(vazao_dica(vz.resumo.otimoKbps)),
                               (int)(ar * 255.0f), (int)(ag * 255.0f), (int)(ab * 255.0f), 255, lw),
               r.x + 28.0f, r.y + 380.0f);
  snprintf(a, sizeof a, i18n("%d de %d tentadas · %d segundos"), atomic_load(&vz.nFonte),
           vz.tentadas, vz.nAmostra);
  metrica(r, r.y + 450.0f, "Fontes medidas", a, 214, 220, 230);
  txt_bloco(TXT_CAPTION, i18n("Filme de 2 h e episódio de 45 min; o tamanho do arquivo aparece no nome da fonte."),
            160, 170, 182, r.x + 28.0f, r.y + r.h - 80.0f, lw, 30.0f, 1, 2);
}

static void desenharVazaoOrigem(GfxRect r, float ar, float ag, float ab) {
  int i, linha = 0, medidos = 0, nf = atomic_load(&vz.nFonte), maiorMs = 1;
  int ve = atomic_load(&vz.estado);
  char val[96];
  painelTitulo(r, "Add-ons e fontes", "Resposta de cada add-on e vazão de cada fonte");
  for (i = 0; i < vz.nAddon; i++) {
    if (vz.addon[i].medido) medidos++;
    if (vz.addon[i].ms > maiorMs) maiorMs = vz.addon[i].ms;
  }
  // Durante a fase dos add-ons os numeros ainda estao sendo escritos pelo
  // fio; so aparecem depois dela.
  if (ve == 1 && atomic_load(&vz.fase) < 2) medidos = 0;
  for (i = 0; i < vz.nAddon && linha < 7 && medidos; i++) {
    const VazAddon *a = &vz.addon[i];
    float y = r.y + 96.0f + (float)linha * 36.0f, bx = r.x + 210.0f, bw = r.w * 0.18f;
    TxtLinha t;
    if (!a->medido) continue;
    linha++;
    txt_desenhar(txt_linha_corta(TXT_CAPTION, addons_nome(i), 190, 198, 210, 255, 170.0f),
                 r.x + 28.0f, y + 2.0f);
    gfx_cor((GfxRect){ bx, y + 8.0f, bw, 12.0f }, 0.5f, 0.10f, 0.12f, 0.15f, 1.0f);
    if (a->ok)
      gfx_cor((GfxRect){ bx, y + 8.0f, bw * (float)a->ms / (float)maiorMs, 12.0f }, 0.5f,
              ar, ag, ab, 0.95f);
    if (a->ok) snprintf(val, sizeof val, i18n("%d ms · %d fontes"), a->ms, a->fontes);
    else if (a->http) snprintf(val, sizeof val, i18n("HTTP %d"), a->http);
    else snprintf(val, sizeof val, "%s", i18n("falhou"));
    t = txt_linha_corta(TXT_CAPTION, val, 210, 216, 226, 255, r.w - (bx - r.x) - bw - 48.0f);
    txt_desenhar(t, r.x + r.w - 28.0f - t.w, y + 2.0f);
  }
  if (!linha)
    txt_desenhar(txt_linha_corta(TXT_CAPTION, i18n(ve == 1 ? "Medindo…" : "Nenhum add-on de fontes respondeu."),
                                 170, 178, 190, 255, r.w - 56.0f), r.x + 28.0f, r.y + 98.0f);
  // Vazao de cada fonte medida: mediana em barras, com o numero embaixo.
  { float y = r.y + 372.0f;
    txt_desenhar(txt_linha(TXT_BODY, i18n("Vazão por fonte"), 238, 242, 248, 255), r.x + 28.0f, y);
    if (nf > 0) {
      int valores[VAZAO_FONTES_MAX];
      char rot[VAZAO_FONTES_MAX][32], b[16];
      const char *rotulos[VAZAO_FONTES_MAX];
      for (i = 0; i < nf; i++) {
        vazao_fmt_mbps(b, sizeof b, vz.fonte[i].r.medianaKbps, sepDecimal());
        snprintf(rot[i], sizeof rot[i], "%s Mbps", b);
        rotulos[i] = rot[i];
        valores[i] = vz.fonte[i].r.medianaKbps;
      }
      // Largura pelo numero de fontes: uma barra so ocupando o painel inteiro
      // parecia um bloco, nao uma medida.
      { float gw = (float)nf * 170.0f + (float)(nf - 1) * 24.0f;
        if (gw > r.w - 64.0f) gw = r.w - 64.0f;
        graficoBarras((GfxRect){ r.x + 32.0f, y + 48.0f, gw, 110.0f }, valores, rotulos, nf, ar, ag, ab); }
    } else {
      txt_desenhar(txt_linha_corta(TXT_CAPTION, i18n(ve == 1 ? "Medindo…" : "Nenhuma fonte medida."),
                                   170, 178, 190, 255, r.w - 56.0f), r.x + 28.0f, y + 48.0f);
    }
  }
}

static void desenharVazao(float y0, float ar, float ag, float ab, Uint32 agora) {
  // 1080 + 20 + 660 na tela inteira; com a rail fixa os dois encolhem na
  // mesma proporcao (991 e 605), e o painel da origem continua a direita.
  float x0 = areaX(), we = (areaW() - 20.0f) * (1080.0f / 1740.0f);
  GfxRect esq = { x0, y0, we, 640.0f };
  GfxRect dir = { x0 + we + 20.0f, y0, areaW() - 20.0f - we, 640.0f };
  painel(esq, ar, ag, ab);
  painel(dir, ar, ag, ab);
  desenharVazaoResumo(esq, ar, ag, ab, agora);
  desenharVazaoOrigem(dir, ar, ag, ab);
  txt_desenhar(txt_linha(TXT_CAPTION, i18n(atomic_load(&vz.estado) == 1 ? "Voltar cancela o teste"
                                                                          : "OK testa de novo · Voltar fecha"),
                         172, 176, 184, 255),
               x0, y0 + 668.0f);
}

void diagnostico_desenhar(Uint32 agora) {
  int estado = atomic_load(&d.estado);
  int feito = atomic_load(&d.feitos), total = atomic_load(&d.total);
  int itens = 0, pend = 0, quentes = 0, fios = 0, fiosMax = 0;
  long bytes = 0, bytesQuentes = 0, memTotal = 0, limite = 0;
  float ar, ag, ab, yConteudo, x0 = areaX(), W = areaW(), cw = colunaW();
  char v[160];
  ajustes_acento(&ar, &ag, &ab);
  tex_estatisticas(&itens, &pend, &bytes, &quentes, &bytesQuentes);
  tex_orcamento_info(NULL, &memTotal, NULL, NULL);
  limite = tex_orcamento_bytes();
  tex_threads_info(&fios, &fiosMax);
  // O SUBTITULO ABAIXO DO TITULO PELA ALTURA MEDIDA, e nao por +52 cravado: o
  // TITULO1 e mais alto que 52 e a frase era desenhada por cima dele (C9).
  { TxtLinha tit = txt_linha(TXT_TITULO1, i18n("Diagnóstico e otimização"), 255, 255, 255, 255);
    TxtLinha sub = txt_linha_corta(TXT_BODY, i18n("Teste os addons, artes e fontes desta TV; o relatório é enviado ao suporte."),
                                   178, 182, 190, 255, W);
    txt_desenhar(tit, x0, NV_MARGEM_Y);
    txt_desenhar(sub, x0, NV_MARGEM_Y + (float)tit.h + 4.0f);
    yConteudo = NV_MARGEM_Y + (float)tit.h + 4.0f + (float)sub.h + 28.0f;
    if (yConteudo < 196.0f) yConteudo = 196.0f; }

  if (vz.aberto) {
    desenharVazao(yConteudo, ar, ag, ab, agora);
  } else if (estado == 0) {
    GfxRect esq = { x0, yConteudo, cw, 880.0f - yConteudo };
    GfxRect dir = { x0 + cw + 80.0f, yConteudo, cw, 880.0f - yConteudo };
    // Os dois cartoes de objetivo repartem o painel: 380 + 24 + 380 nos 840.
    float oc = (esq.w - 56.0f - 24.0f) * 0.5f;
    static const char *const PASSOS[5] = {
      "Manifestos, catálogos e fontes de vídeo dos add-ons",
      "Cada fonte de arte: catálogo, Metahub, TMDB, Trakt, Apple TV, fanart.tv, anime e logo",
      "As mesmas artes pelo cache, com o perfil atual",
      "Aplica o candidato e mede as mesmas artes de novo",
      "Mantém se não piorou; senão volta sozinho ao anterior",
    };
    PtvPerfil atual, cand;
    int travado, i;
    long mem;
    perfilAtual(&atual, &travado, &mem);
    ptv_candidato(ptv_plataforma(), mem, focoModo ? PTV_DESEMPENHO : PTV_QUALIDADE,
                  travado, &cand);
    painel(esq, ar, ag, ab);
    painel(dir, ar, ag, ab);
    painelTitulo(esq, "Objetivo", "Esquerda e direita escolhem o objetivo");
    for (i = 0; i < 2; i++) {
      GfxRect c = { esq.x + 28.0f + i * (oc + 24.0f), esq.y + 100.0f, oc, 196.0f };
      int foco = focoModo == i && !focoLinha;
      gfx_cor(c, 18.0f / c.h, foco ? ar : 0.10f, foco ? ag : 0.12f, foco ? ab : 0.15f, 0.98f);
      txt_desenhar(txt_linha(TXT_BODY, i18n(i ? "Desempenho" : "Qualidade"),
                             foco ? 16 : 244, foco ? 18 : 246, foco ? 22 : 250, 255),
                   c.x + 24.0f, c.y + 22.0f);
      txt_bloco(TXT_CAPTION, i18n(i ? "Fontes mais leves, menos antecipação e menor pressão de memória"
                                    : "Nitidez na resolução exibida e preferências atuais preservadas"),
                foco ? 24 : 178, foco ? 26 : 184, foco ? 30 : 194,
                c.x + 24.0f, c.y + 72.0f, c.w - 48.0f, 30.0f, 1, 3);
    }
    txt_desenhar(txt_linha(TXT_BODY, i18n("O que muda nesta TV"), 238, 242, 248, 255),
                 esq.x + 28.0f, esq.y + 326.0f);
    linhasDoPerfil(esq, esq.y + 372.0f, 40.0f, &atual, &cand);
    txt_bloco(TXT_CAPTION, i18n("Se o reteste da mesma amostra piorar, o perfil anterior volta sozinho e a tela diz o motivo."),
              170, 178, 190, esq.x + 28.0f, esq.y + 508.0f, esq.w - 56.0f, 30.0f, 1, 2);
    if (travado)
      txt_desenhar(txt_linha_corta(TXT_CAPTION, i18n("A memória para imagens foi escolhida em Ajustes e não muda."),
                                   244, 218, 152, 255, esq.w - 56.0f), esq.x + 28.0f, esq.y + 580.0f);
    txt_desenhar(txt_linha_corta(TXT_CAPTION, i18n("Até 3 títulos, 6 fontes e 12 imagens; limite de 8 minutos"),
                                 160, 170, 182, 255, esq.w - 56.0f), esq.x + 28.0f, esq.h + esq.y - 50.0f);
    painelTitulo(dir, "Como funciona", "Cinco etapas, nesta ordem");
    for (i = 0; i < 5; i++) {
      float y = dir.y + 100.0f + i * 58.0f;
      char n[4];
      snprintf(n, sizeof n, "%d", i + 1);
      gfx_cor((GfxRect){ dir.x + 28.0f, y, 38.0f, 38.0f }, 0.5f, ar, ag, ab, 0.9f);
      { TxtLinha t = txt_linha(TXT_CAPTION, n, 16, 18, 22, 255);
        txt_desenhar(t, dir.x + 28.0f + (38.0f - t.w) * 0.5f, y + (38.0f - t.h) * 0.5f); }
      txt_desenhar(txt_linha_corta(TXT_BODY, i18n(PASSOS[i]), 220, 226, 236, 255, dir.w - 120.0f),
                   dir.x + 84.0f, y + 2.0f);
    }
    txt_bloco(TXT_CAPTION, i18n("O relatório compacto é guardado localmente e só fica como enviado após confirmação do servidor."),
              170, 178, 190, dir.x + 28.0f, dir.y + 410.0f, dir.w - 56.0f, 30.0f, 1, 2);
    txt_desenhar(txt_linha_corta(TXT_CAPTION, i18n("Não altera assistidos, histórico, progresso ou scrobbling."),
                                 160, 170, 182, 255, dir.w - 56.0f), dir.x + 28.0f, dir.h + dir.y - 50.0f);
    { TxtLinha ok = txt_linha(TXT_BODY, i18n(focoLinha ? "OK inicia o teste de velocidade"
                                                       : "OK inicia o diagnóstico"),
                              ar * 255.0f, ag * 255.0f, ab * 255.0f, 255);
      int foco = focoLinha == 1;
      TxtLinha t = txt_linha(TXT_BODY, i18n("Teste de velocidade"), foco ? 16 : 232,
                             foco ? 18 : 236, foco ? 22 : 244, 255);
      float w = (float)t.w + 64.0f, x = x0 + W - w;
      txt_desenhar(ok, x0, 930.0f);
      // Cortada ANTES do botao: com a rail fixa a linha perdeu 144 e a dica
      // encostava nele.
      txt_desenhar(txt_linha_corta(TXT_CAPTION, i18n(focoLinha ? "Para cima volta ao objetivo · Voltar sai"
                                                               : "Para baixo: teste de velocidade · Voltar sai"),
                                   172, 176, 184, 255, x - 32.0f - (x0 + ok.w + 32.0f)),
                   x0 + ok.w + 32.0f, 934.0f);
      // O mesmo botao da tela de resultado (desenharBotoes), a direita.
      if (foco) gfx_cor((GfxRect){ x, 910.0f, w, 60.0f }, 0.5f, ar, ag, ab, 1.0f);
      else gfx_cor((GfxRect){ x, 910.0f, w, 60.0f }, 0.5f, 0.13f, 0.15f, 0.19f, 1.0f);
      txt_desenhar(t, x + 32.0f, 910.0f + (60.0f - (float)t.h) * 0.5f); }
  } else if (estado == 1) {
    // 100 de folga de cada lado da area util: 180..1740 na tela inteira.
    GfxRect pn = { x0 + 100.0f, yConteudo + 20.0f, W - 200.0f, 640.0f };
    // Coluna de metricas ancorada na DIREITA do painel; com a rail fixa ela
    // estreita um pouco (640 -> 604) e as etapas, a esquerda, cortam antes
    // dela em vez de passar por baixo.
    GfxRect col = { 0.0f, pn.y + 230.0f, 640.0f - (1560.0f - pn.w) * 0.25f, 300.0f };
    float etapasW;
    col.x = pn.x + pn.w - 60.0f - col.w;
    etapasW = col.x - (pn.x + 108.0f) - 24.0f;
    if (etapasW > 700.0f) etapasW = 700.0f;
    static const char *const ETAPAS[6] = {
      "Sondando manifestos, catálogos e fontes",
      "Medindo cada fonte de arte",
      "Artes com o cache frio",
      "Artes com o perfil atual",
      "Artes com o perfil candidato",
      "Gravando e enviando o relatório",
    };
    int fase = atomic_load(&d.fase), i;
    float pct;
    if (fase < 1) fase = 1;
    if (fase > 6) fase = 6;
    // 60% para as sondagens de rede (o grosso do tempo), o resto por etapa.
    pct = fase <= 2 ? (total > 0 ? 60.0f * (float)feito / (float)total : 4.0f)
                    : 60.0f + (float)(fase - 2) * 9.0f;
    painel(pn, ar, ag, ab);
    painelTitulo(pn, "Diagnóstico em andamento", ETAPAS[fase - 1]);
    snprintf(v, sizeof v, "%d%%", (int)pct);
    txt_desenhar(txt_linha(TXT_TITULO2, v, 246, 249, 255, 255), pn.x + 64.0f, pn.y + 104.0f);
    barraProgresso((GfxRect){ pn.x + 64.0f, pn.y + 196.0f, pn.w - 128.0f, 20.0f }, pct, ar, ag, ab, 1, agora);
    for (i = 0; i < 6; i++) {
      float y = pn.y + 250.0f + i * 50.0f;
      int feita = i + 1 < fase, atual = i + 1 == fase;
      float a = atual ? 0.55f + 0.45f * sinf((float)agora * 0.006f) : 1.0f;
      gfx_cor((GfxRect){ pn.x + 64.0f, y + 6.0f, 22.0f, 22.0f }, 0.5f,
              feita || atual ? ar : 0.22f, feita || atual ? ag : 0.24f, feita || atual ? ab : 0.28f,
              feita ? 1.0f : atual ? a : 0.9f);
      txt_desenhar(txt_linha_corta(TXT_BODY, i18n(ETAPAS[i]), feita || atual ? 232 : 140,
                                   feita || atual ? 236 : 146, feita || atual ? 244 : 156, 255, etapasW),
                   pn.x + 108.0f, y);
    }
    { snprintf(v, sizeof v, i18n("%d de %d"), feito, total);
      metrica(col, col.y + 20.0f, "Etapas de rede", v, 214, 220, 230);
      if (limite > 0) snprintf(v, sizeof v, "%ld / %ld MB", bytes / (1024L * 1024L), limite / (1024L * 1024L));
      else snprintf(v, sizeof v, "%s", i18n("indisponível"));
      metrica(col, col.y + 64.0f, "Memória usada por imagens", v, 214, 220, 230);
      snprintf(v, sizeof v, i18n("%d de %d"), fios, fiosMax);
      metrica(col, col.y + 108.0f, "Fios em uso", v, 214, 220, 230);
      snprintf(v, sizeof v, "%d px", tex_teto_heroi());
      metrica(col, col.y + 152.0f, "Largura do fundo em tela cheia", v, 214, 220, 230); }
    txt_desenhar(txt_linha(TXT_CAPTION, i18n("Voltar cancela e interrompe as requisições da sessão."), 176, 184, 194, 255),
                 pn.x + 64.0f, pn.y + pn.h - 50.0f);
  } else if (estado == 2) {
    GfxRect tl = { x0, yConteudo, cw, 356.0f };
    GfxRect tr = { x0 + cw + 80.0f, yConteudo, cw, 356.0f };
    GfxRect bl = { x0, yConteudo + 376.0f, cw, 312.0f };
    GfxRect br = { x0 + cw + 80.0f, yConteudo + 376.0f, cw, 312.0f };
    static const char *const ETAPA_ROT[4] = { "Manifestos", "Catálogos", "Artes", "Fontes" };
    int tempos[4];
    int cobertura = coberturaPercentual(), cor;
    const char *txtAp;
    tempos[0] = d.manifestMs; tempos[1] = d.catalogMs;
    tempos[2] = d.assetsMs + d.fontesMs; tempos[3] = d.streamMs;
    painel(tl, ar, ag, ab);
    painel(tr, ar, ag, ab);
    painel(bl, ar, ag, ab);
    painel(br, ar, ag, ab);
    painelTitulo(tl, "Resultado geral", d.coberturaParcial ? "Cobertura parcial da amostra" : "Amostra concluída");
    snprintf(v, sizeof v, "%d%%", cobertura);
    txt_desenhar(txt_linha(TXT_TITULO2, v, ar * 255.0f, ag * 255.0f, ab * 255.0f, 255), tl.x + 28.0f, tl.y + 92.0f);
    barraProgresso((GfxRect){ tl.x + 28.0f, tl.y + 170.0f, tl.w - 56.0f, 14.0f }, (float)cobertura, ar, ag, ab, 0, agora);
    metrica(tl, tl.y + 206.0f, "Gargalo principal", i18n(gargaloPrincipal()), 244, 224, 172);
    { int ok = d.imagensOk, fal = d.imagensFalhas, k;
      for (k = 1; k < PTV_N_FONTES; k++) { ok += d.fonte[k].ok; fal += d.fonte[k].falhas; }
      snprintf(v, sizeof v, i18n("%d ok · %d falhas"), ok, fal); }
    metrica(tl, tl.y + 250.0f, "Artes", v, 210, 218, 230);
    snprintf(v, sizeof v, "%s", i18n(atomic_load(&d.enviando) ? "enviando…"
                                    : d.envioFalhou ? "guardado; envio falhou"
                                    : d.enviado ? "enviado ao suporte" : "guardado localmente"));
    metrica(tl, tl.y + 294.0f, "Relatório", v, d.envioFalhou ? 240 : 170, d.envioFalhou ? 170 : 220, d.envioFalhou ? 150 : 190);

    desenharFontes(tr, ar, ag, ab);

    painelTitulo(bl, "Tempo por etapa", "Maior tempo agregado da amostra; falha isolada não derruba o addon.");
    graficoBarras((GfxRect){ bl.x + 32.0f, bl.y + 94.0f, bl.w - 64.0f, 80.0f }, tempos, ETAPA_ROT, 4, ar, ag, ab);
    snprintf(v, sizeof v, "%ld / %ld MB", bytes / (1024L * 1024L), limite > 0 ? limite / (1024L * 1024L) : 0L);
    metrica(bl, bl.y + 218.0f, "Memória usada por imagens", v, 220, 226, 236);
    snprintf(v, sizeof v, i18n("%d pendentes · %d quentes"), pend, quentes);
    metrica(bl, bl.y + 254.0f, "Fila de imagens", v, 176, 188, 202);

    txtAp = textoAplicacao(&cor);
    painelTitulo(br, "O que foi aplicado", d.modo == DIAG_DESEMPENHO ? "Desempenho" : "Qualidade");
    txt_desenhar(txt_linha_corta(TXT_BODY, i18n(txtAp), cor == 1 ? 170 : cor == 2 ? 244 : 214,
                                 cor == 1 ? 222 : cor == 2 ? 196 : 220, cor == 1 ? 190 : cor == 2 ? 150 : 230,
                                 255, br.w - 56.0f),
                 br.x + 28.0f, br.y + 92.0f);
    if (d.motivo)
      txt_desenhar(txt_linha_corta(TXT_CAPTION, i18n(d.motivo), 240, 176, 150, 255, br.w - 56.0f),
                   br.x + 28.0f, br.y + 128.0f);
    // O CANDIDATO TESTADO aparece como antes -> depois sempre que ele entrou
    // no ar, tenha ficado ou nao: o veredito acima diz qual dos dois vale.
    // Sem candidato (igual, sem amostra), so o que vale agora.
    { int testado = d.aplicacao == DA_MANTIDO || d.aplicacao == DA_RESTAURADO_AUTO ||
                    d.aplicacao == DA_RESTAURADO_MANUAL || d.aplicacao == DA_CANCELADO ||
                    d.aplicacao == DA_RUIDO;
      PtvPerfil atual_;
      int tr_;
      long m_;
      perfilAtual(&atual_, &tr_, &m_);
      if (testado) linhasDoPerfil(br, br.y + 158.0f, 32.0f, &d.perfAntes, &d.perfCand);
      else linhasDoPerfil(br, br.y + 158.0f, 32.0f, &atual_, &atual_); }
    if (d.medAntes.artesMs || d.medDepois.artesMs) {
      snprintf(v, sizeof v, "%d → %d ms", d.medAntes.artesMs, d.medDepois.artesMs);
      metrica(br, br.y + 254.0f, "Artes, mesma amostra", v, 214, 220, 230);
    }
    desenharBotoes(yConteudo + 708.0f, ar, ag, ab);
    txt_desenhar(txt_linha(TXT_CAPTION, i18n("Esquerda e direita escolhem · OK confirma · Voltar sai"), 172, 176, 184, 255),
                 x0, yConteudo + 790.0f);
  } else {
    int cor;
    float px = x0 + (W - 1400.0f) * 0.5f;   // 260 na tela inteira
    painel((GfxRect){ px, 260.0f, 1400.0f, 360.0f }, ar, ag, ab);
    txt_desenhar(txt_linha(TXT_TITULO2, i18n(d.erro[0] ? d.erro : "O teste foi cancelado."), 240, 190, 180, 255), px + 60.0f, 340.0f);
    txt_desenhar(txt_linha(TXT_BODY, i18n(d.aplicacao == DA_CANCELADO ? textoAplicacao(&cor)
                                          : "Nenhuma configuração foi alterada."), 182, 190, 202, 255), px + 60.0f, 420.0f);
    txt_desenhar(txt_linha(TXT_CAPTION, i18n("OK tenta de novo · Voltar sai"), 172, 176, 184, 255), px + 60.0f, 540.0f);
  }
  if (d.intro) desenharApresentacao(0);
}

int diagnostico_quer_sair(void) { return sairTela; }

void diagnostico_encerrar(void) {
  atomic_store(&d.cancelado, 1);
  if (d.fio) { SDL_WaitThread(d.fio, NULL); d.fio = NULL; }
  if (d.fioSug) {
    SDL_WaitThread(d.fioSug, NULL); d.fioSug = NULL;
    if (d.sugEstado == DS_TESTANDO) ajustes_definir_destaque(d.sugFonteAntes, d.sugDifAntes);
  }
  if (d.fioEnvio) { SDL_WaitThread(d.fioEnvio, NULL); d.fioEnvio = NULL; }
  atomic_store(&vz.cancelado, 1);
  if (vz.fio) { SDL_WaitThread(vz.fio, NULL); vz.fio = NULL; }
  desfazerExperimento();
}
