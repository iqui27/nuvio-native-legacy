#include "debrid.h"
#include "rede.h"
#include "js.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <stdatomic.h>

// As tres bases. A versao esta EMBUTIDA no caminho de proposito: o TorBox pede
// /v1/ antes de /api/ (api.torbox.app/v1/api/...), e deixar isso implicito na
// montagem de cada rota e o tipo de detalhe que se erra uma vez e se paga em
// 404 silencioso.
#define RD "https://api.real-debrid.com/rest/1.0"
#define TB "https://api.torbox.app/v1/api"
#define PM "https://www.premiumize.me/api"

enum { SRD, STB, SPM, SN };
static const char *nomeServ[SN] = { "Real-Debrid", "TorBox", "Premiumize" };

// UMA CHAVE POR SERVICO, e nao uma so. A conta pode trazer mais de uma
// credencial "debrid:*" (sync.c chama esta funcao uma vez por provedor), e
// guardar so a ultima faria o resultado depender da ORDEM que o servidor
// devolve as linhas — o pior tipo de defeito, porque muda sozinho.
static char chave[SN][200];
static int  alvoT, alvoE;

// RECUSA DE CONTA POR BUSCA. recusado[q] guarda o status HTTP da recusa (0 =
// nenhuma); `geracao` sobe a cada debrid_nova_busca. Atomicos porque
// debrid_resolver rodava em ate 4 fios da verificacao ao mesmo tempo; desde o
// #130 a verificacao e em serie, mas o fio dela continua sendo outro que o de
// desenho, e a trava nao custa nada.
//
// A GERACAO e o que impede um fio da busca ANTERIOR — uma verificacao de
// streams.c que ainda esta no prazo dela — de marcar recusa na busca nova:
// ele guardou a geracao ao entrar e so escreve se ela nao mudou.
//
// O LIMITE QUE ESTAVA ACEITO AQUI CAIU COM O #130: com 4 fios, ate 4
// createtorrent saiam antes de o primeiro 403 voltar. Com a verificacao em
// serie, o primeiro 403 marca a recusa antes da candidata seguinte.
static _Atomic unsigned geracao;
static _Atomic int recusado[SN];

// CONTA SEM PLANO, e isto nao e "recusa desta busca": vale a SESSAO inteira.
// Registros 1731-1774 (webOS 1.4.1): o TorBox respondeu 403
// PLAN_RESTRICTED_FEATURE ("API feature not available on your plan") e o
// Premiumize 200 com "Account not premium." — conta gratuita nos dois. Um
// plano nao muda entre uma busca e a outra, e a recusa por busca fazia a mesma
// pessoa pagar 4 createtorrent a cada titulo aberto; o Premiumize, que nem era
// reconhecido como erro de conta, foi perguntado torrent por torrent (197
// linhas). So uma chave nova (debrid_definir_chave) ou o logout limpam.
// `avisado` e o "ja mostrei o aviso" de cada servico, pelo mesmo tempo.
static _Atomic int semPlano[SN];
static _Atomic int avisado[SN];

// Torrents que a busca atual achou fora de cache (debrid_fora_de_cache).
static _Atomic int foraCache;

// Codigo INTERNO dos resolvedores: "fora de cache", que para quem chama e 0
// (nao tocou), mas que a escolha manual precisa separar de "falhou" para saber
// se vale pedir ao servico que baixe. Nunca sai deste arquivo.
#define FORA 3

static int idServico(const char *s) {
  if (!strcasecmp(s, "realdebrid") || !strcasecmp(s, "real-debrid")) return SRD;
  if (!strcasecmp(s, "torbox")     || !strcasecmp(s, "tor-box"))     return STB;
  if (!strcasecmp(s, "premiumize") || !strcasecmp(s, "premiumize-me")
      || !strcasecmp(s, "premiumizeme")) return SPM;
  return -1;
}

void debrid_definir_chave(const char *servico, const char *k) {
  int q;
  if (!servico || !k || !*k) return;
  q = idServico(servico);
  if (q < 0) {
    printf("[debrid] %s: servico sem resolvedor aqui, ignorado\n", servico);
    return;
  }
  if (strcmp(chave[q], k)) { atomic_store(&semPlano[q], 0); atomic_store(&avisado[q], 0); }
  snprintf(chave[q], sizeof chave[q], "%s", k);
  printf("[debrid] chave do %s vinda da conta\n", nomeServ[q]);
}
// So conta servico que PODE resolver: com todas as chaves em conta sem plano,
// streams.c descarta os torrents sem url logo na lista (como sem debrid) e a
// verificacao vai direto as fontes diretas, em vez de gastar o lote nelas.
int debrid_ativo(void) {
  int q;
  for (q = 0; q < SN; q++) if (chave[q][0] && !atomic_load(&semPlano[q])) return 1;
  return 0;
}
void debrid_esquecer(void) {
  int q;
  memset(chave, 0, sizeof chave); alvoT = alvoE = 0;
  atomic_store(&foraCache, 0);
  for (q = 0; q < SN; q++) {
    atomic_store(&recusado[q], 0);
    atomic_store(&semPlano[q], 0);
    atomic_store(&avisado[q], 0);
  }
}
void debrid_nova_busca(void) {
  int q;
  atomic_fetch_add(&geracao, 1u);
  atomic_store(&foraCache, 0);
  for (q = 0; q < SN; q++) atomic_store(&recusado[q], 0);
}
int debrid_recusa(char *dst, unsigned n) {
  int q;
  for (q = 0; q < SN; q++) {
    int st = atomic_load(&recusado[q]);
    if (!st && atomic_load(&semPlano[q])) {
      if (dst && n) snprintf(dst, n, "%s free-plan", nomeServ[q]);
      return 1;
    }
    if (!st) continue;
    if (dst && n) snprintf(dst, n, "%s %d", nomeServ[q], st);
    return 1;
  }
  return 0;
}

int debrid_fora_de_cache(void) { return atomic_load(&foraCache); }

int debrid_sem_plano(void) {
  int q, m = 0;
  for (q = 0; q < SN; q++)
    if (chave[q][0] && atomic_load(&semPlano[q])) m |= 1 << q;
  return m;
}

// Frases FIXAS por servico, uma chave de traducao cada (idioma_tab.h): texto
// montado com %s nunca casaria com a tabela.
const char *debrid_sem_plano_frase(int mascara) {
  int tb = (mascara & (1 << STB)) != 0, pm = (mascara & (1 << SPM)) != 0;
  int rd = (mascara & (1 << SRD)) != 0;
  if (tb + pm + rd > 1)
    return "Suas contas de debrid são gratuitas e não permitem uso pela API — as fontes torrent ficam de fora";
  if (tb) return "Sua conta TorBox é gratuita e não permite uso pela API — fontes torrent do TorBox ficam de fora";
  if (pm) return "Sua conta Premiumize é gratuita e não permite uso pela API — fontes torrent do Premiumize ficam de fora";
  if (rd) return "Sua conta Real-Debrid não é premium e não permite uso pela API — fontes torrent do Real-Debrid ficam de fora";
  return NULL;
}

int debrid_sem_plano_novo(void) {
  int q, m = 0;
  for (q = 0; q < SN; q++) {
    int zero = 0;
    if (!chave[q][0] || !atomic_load(&semPlano[q])) continue;
    if (atomic_compare_exchange_strong(&avisado[q], &zero, 1)) m |= 1 << q;
  }
  return m;
}
void debrid_definir_episodio(int t, int e) { alvoT = t; alvoE = e; }

// ---------------------------------------------------------------- http

static void urlenc(char *dst, unsigned n, const char *s) {
  unsigned k = 0;
  for (; *s && k + 4 < n; s++) {
    unsigned char c = (unsigned char)*s;
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') dst[k++] = (char)c;
    else k += (unsigned)snprintf(dst + k, n - k, "%%%02X", c);
  }
  dst[k] = 0;
}

// Os tres servicos aceitam a chave como "Authorization: Bearer" — e o que a
// documentacao de cada um recomenda, inclusive a do Premiumize, que ainda
// aceita ?apikey= por compatibilidade e diz em texto que o cabecalho existe
// justamente para a chave NAO entrar em registro de servidor nem em Referer.
// A unica excecao e o requestdl do TorBox, mais abaixo.
static char *get_auth(const char *base, const char *rota, int qual, int *st) {
  char url[900], auth[260];
  const char *cab[2];
  snprintf(url, sizeof url, "%s/%s", base, rota);
  snprintf(auth, sizeof auth, "Authorization: Bearer %s", chave[qual]);
  cab[0] = auth; cab[1] = NULL;
  return rede_baixar_st(url, 15, cab, st);
}
static char *post_form(const char *base, const char *rota, int qual,
                       const char *corpo, int *st) {
  char url[900], auth[260];
  const char *cab[3];
  snprintf(url, sizeof url, "%s/%s", base, rota);
  snprintf(auth, sizeof auth, "Authorization: Bearer %s", chave[qual]);
  cab[0] = auth; cab[1] = "Content-Type: application/x-www-form-urlencoded"; cab[2] = NULL;
  return rede_postar_st(url, 15, cab, corpo, st);
}
static int ok2xx(const char *r, int st) { return r && st >= 200 && st < 300; }

// ---------------------------------------------------------------- diagnostico
//
// O CORPO DA RESPOSTA DE ERRO VAI PARA O LOG. No registro 1541 (webOS 1.4.0)
// o TorBox respondeu `createtorrent: HTTP 403` oito vezes e o log so tinha o
// numero: o JSON do TorBox traz `error` (codigo) e `detail` (frase), e era ali
// que estava a diferenca entre "chave sem plano", "limite de torrents ativos"
// e "add_only_if_cached recusado". Sem o corpo nao da para escolher.
//
// 160 BYTES: cabe o {"success":false,"error":"...","detail":"..."} do TorBox
// e o {"error":"...","error_code":N} do Real-Debrid inteiros, e uma pagina
// HTML de WAF mostra o <title>, que e o que a identifica.
//
// A CHAVE E CORTADA DO TEXTO mesmo nao devendo estar la: ela vai no cabecalho
// Authorization (ou na query do requestdl), nao no corpo — mas uma API que
// ecoa o pedido no erro a poria no log, e o log sai da TV. O corte roda numa
// janela de 160 + 200 bytes (200 = o maior tamanho de chave) ANTES de truncar,
// para que uma chave atravessando o byte 160 nao deixe o comeco dela no log.
//
// SO RESPOSTA NAO-2xx VAI CRUA. Corpo 2xx pode trazer link direto (o `links`
// do torrents/info do RD e credencial, ver rede_url_publica); de 2xx que nao
// serviu saem so os campos de erro (error/detail/message), passados pelo
// mesmo corte.
#define CORPO_LOG 160

static void corpoSeguro(const char *r, char *dst, unsigned n) {
  char jan[CORPO_LOG + sizeof chave[0] + 1];
  size_t L = strlen(r), i;
  int q;
  if (L > sizeof jan - 1) L = sizeof jan - 1;
  memcpy(jan, r, L); jan[L] = 0;
  // quebra de linha e tab viram espaco: uma linha de log por falha
  for (i = 0; i < L; i++) if ((unsigned char)jan[i] < ' ') jan[i] = ' ';
  for (q = 0; q < SN; q++) {
    size_t K = strlen(chave[q]);
    char *p;
    if (!K) continue;
    while ((p = strstr(jan, chave[q])) != NULL) {
      memcpy(p, "***", 3);
      memmove(p + 3, p + K, strlen(p + K) + 1);
    }
  }
  if (strlen(jan) > CORPO_LOG) jan[CORPO_LOG] = 0;
  snprintf(dst, n, "%s", jan);
}

// strcasestr nao e C padrao e o conjunto de ferramentas do webOS nao o declara
// sem _GNU_SOURCE; sao tres usos, nao vale o define.
static int contem(const char *r, const char *s) {
  size_t L = strlen(s);
  for (; r && *r; r++) if (!strncasecmp(r, s, L)) return 1;
  return 0;
}

// ERRO DA CONTA OU DO TORRENT? A regra:
//   - o corpo diz que o item nao esta em cache -> do TORRENT: o proximo
//     torrent da mesma busca pode estar, segue tentando o servico;
//   - 401, 403 ou 429 -> da CONTA: chave, plano ou limite valem igual para
//     todo torrent, e repetir so gasta ate 15 s por tentativa;
//   - os codigos de erro de conta do TorBox em qualquer status (ACTIVE_LIMIT,
//     MONTHLY_LIMIT, COOLDOWN_LIMIT, PLAN_RESTRICTED_FEATURE, BAD_TOKEN,
//     AUTH_ERROR, NO_AUTH — lista da documentacao/SDK do TorBox, NAO
//     conferida contra uma resposta real ainda: o log novo e que vai mostrar
//     qual deles o 403 do registro 1541 traz);
//   - o resto (400, 404, 5xx, sem resposta) -> do torrent.
// Vale para os tres servicos: o 403 "permission_denied" do Real-Debrid tambem
// e da conta.
// CONTA SEM PLANO QUE PERMITA A API. Os textos sao os que os servicos
// mandaram de verdade (registros 1731-1774): TorBox `"error":
// "PLAN_RESTRICTED_FEATURE"` com HTTP 403, Premiumize `"Account not
// premium."` com HTTP 200. Se o Real-Debrid mandar um texto desses, entra pelo
// mesmo caminho — mas nenhum registro mostrou o RD assim ainda; o 403 dele
// continua sendo recusa por busca, como antes.
int debrid_eh_sem_plano(int st, const char *r) {
  (void)st;
  if (!r) return 0;
  return strstr(r, "PLAN_RESTRICTED") != NULL || contem(r, "not premium")
      || contem(r, "not_premium") || contem(r, "premium account required");
}

static int erroDeConta(int st, const char *r) {
  if (r && (contem(r, "not cached") || contem(r, "not_cached")
            || contem(r, "uncached")))
    return 0;
  if (st == 401 || st == 403 || st == 429) return 1;
  if (debrid_eh_sem_plano(st, r)) return 1;
  if (r && (strstr(r, "ACTIVE_LIMIT") || strstr(r, "MONTHLY_LIMIT")
            || strstr(r, "COOLDOWN_LIMIT") || strstr(r, "PLAN_RESTRICTED")
            || strstr(r, "BAD_TOKEN") || strstr(r, "AUTH_ERROR")
            || strstr(r, "NO_AUTH")))
    return 1;
  return 0;
}

// Registra a falha de uma chamada e devolve o que o resolvedor devolve: 0
// (falhou este torrent) ou -st (falhou pela CONTA; debrid_resolver para de
// usar o servico nesta busca). `rota` e o nome da chamada, NUNCA a URL — a do
// requestdl leva o token.
static int falha(int q, const char *rota, int st, const char *r) {
  char txt[CORPO_LOG + 1];
  if (!r) {
    printf("[debrid] %s %s: HTTP %d sem corpo\n", nomeServ[q], rota, st);
  } else if (st < 200 || st >= 300) {
    corpoSeguro(r, txt, sizeof txt);
    printf("[debrid] %s %s: HTTP %d (%u bytes) %s\n", nomeServ[q], rota, st,
           (unsigned)strlen(r), txt);
  } else {
    char campo[CORPO_LOG + 1], e[64] = "", d[CORPO_LOG + 1] = "";
    if (js_texto(r, NULL, "error", campo, sizeof campo)) corpoSeguro(campo, e, sizeof e);
    if (js_texto(r, NULL, "detail", campo, sizeof campo)
        || js_texto(r, NULL, "message", campo, sizeof campo))
      corpoSeguro(campo, d, sizeof d);
    printf("[debrid] %s %s: HTTP %d sem o esperado; error=%s detail=%s\n",
           nomeServ[q], rota, st, e[0] ? e : "-", d[0] ? d : "-");
  }
  if (debrid_eh_sem_plano(st, r)) {
    int zero = 0;
    if (atomic_compare_exchange_strong(&semPlano[q], &zero, 1))
      printf("[debrid] %s: conta sem plano para a API; fora pelo resto da sessao\n",
             nomeServ[q]);
  }
  return erroDeConta(st, r) ? -(st > 0 ? st : 1) : 0;
}

static void minusc(char *s) { for (; *s; s++) *s = (char)tolower((unsigned char)*s); }

// Copia do hash em minusculas. O TorBox guarda o infohash em minusculo e a
// consulta de cache compara TEXTO: um hash que o addon mandou em maiusculas
// volta "nao esta em cache" mesmo estando.
static void hashMin(char *dst, unsigned n, const char *h) {
  snprintf(dst, n, "%s", h);
  minusc(dst);
}

// ---------------------------------------------------------------- arquivo

static int ehVideo(const char *nome) {
  static const char *ext[] = { ".mp4", ".mkv", ".webm", ".avi", ".mov", ".m4v", ".ts", ".m2ts", ".wmv", NULL };
  size_t L = strlen(nome); int i;
  for (i = 0; ext[i]; i++) {
    size_t e = strlen(ext[i]);
    if (L > e && !strcasecmp(nome + L - e, ext[i])) return 1;
  }
  return 0;
}

// Mesma ordem do selectDebridFile do web: padrao SxxEyy > fileIdx > maior
// video. Devolve o ELEMENTO escolhido (ponteiro para o '{' dele, dentro de
// `files`) ou NULL.
//
// POR QUE DEVOLVE O ELEMENTO E NAO UM id. Os tres servicos nomeiam os campos a
// sua maneira — o RD manda "path"/"bytes" e o arquivo tem "id"; o TorBox manda
// "name"/"size" com "id"; o Premiumize manda "path"/"size" e NAO tem id
// nenhum, o link direto vem dentro do proprio elemento. Devolvendo o elemento,
// cada resolvedor tira dali o que o seu servico usa e a ORDEM DE ESCOLHA — que
// e a unica coisa que nao pode divergir entre os tres — fica escrita uma vez
// so. Os nomes dos campos entram por parametro pelo mesmo motivo.
static const char *escolherArquivo(const char *files, int fileIdx,
                                   const char *kNome, const char *kTam) {
  char pad1[16] = "", pad2[16] = "";
  const char *p, *melhor = NULL; int idx = 0; double melhorTam = -1;
  if (alvoT > 0 && alvoE > 0) {
    snprintf(pad1, sizeof pad1, "s%02de%02d", alvoT, alvoE);
    snprintf(pad2, sizeof pad2, "%dx%02d", alvoT, alvoE);
  }
  for (p = files; p && *p == '{'; p = js_prox(js_fim(p)), idx++) {
    const char *f = js_fim(p);
    char nome[600]; double tam;
    if (!js_texto(p, f, kNome, nome, sizeof nome)) continue;
    tam = js_num(p, f, kTam, 0);
    minusc(nome);
    if (!ehVideo(nome)) continue;
    if (pad1[0] && (strstr(nome, pad1) || strstr(nome, pad2))) return p;
    if (idx == fileIdx && fileIdx >= 0) { melhor = p; melhorTam = 1e18; continue; }
    if (tam > melhorTam) { melhor = p; melhorTam = tam; }
  }
  return melhor;
}

// ---------------------------------------------------------------- Real-Debrid
//
// Rotas (api.real-debrid.com/rest/1.0, documentacao oficial "REST API"):
//   POST torrents/addMagnet, GET torrents/info/<id>,
//   POST torrents/selectFiles/<id>, POST unrestrict/link.

// `pct` recebe o progresso (0-100) do torrent que o RD ficou baixando, quando
// ele nao ficou pronto nas tres olhadas; a escolha manual mostra isso.
static int resolverRD(const char *infoHash, int fileIdx, char *url, unsigned n,
                      int *pct) {
  char corpo[700], enc[600], rota[120], tid[64], status[32], link[600];
  char *r; int st = 0, id, tent;
  const char *files, *links, *el;

  snprintf(corpo, sizeof corpo, "magnet:?xt=urn:btih:%s", infoHash);
  urlenc(enc, sizeof enc, corpo);
  snprintf(corpo, sizeof corpo, "magnet=%s", enc);
  r = post_form(RD, "torrents/addMagnet", SRD, corpo, &st);
  if (!ok2xx(r, st) || !js_texto(r, NULL, "id", tid, sizeof tid)) {
    int v = falha(SRD, "addMagnet", st, r); free(r); return v;
  }
  free(r);

  snprintf(rota, sizeof rota, "torrents/info/%s", tid);
  r = get_auth(RD, rota, SRD, &st);
  if (!ok2xx(r, st) || !(files = js_array(r, NULL, "files"))) {
    int v = falha(SRD, "info", st, r); free(r); return v;
  }
  el = escolherArquivo(files, fileIdx, "path", "bytes");
  id = el ? (int)js_num(el, js_fim(el), "id", -1) : -1;
  free(r);
  if (id < 0) { printf("[debrid] torrent sem video utilizavel\n"); return 0; }

  snprintf(rota, sizeof rota, "torrents/selectFiles/%s", tid);
  snprintf(corpo, sizeof corpo, "files=%d", id);
  r = post_form(RD, rota, SRD, corpo, &st);
  if (!(st == 204 || st == 202 || (st >= 200 && st < 300))) {
    int v = falha(SRD, "selectFiles", st, r); free(r); return v;
  }
  free(r);

  // Em cache o RD marca "downloaded" quase na hora; fora de cache ele
  // comecaria a BAIXAR — e isso nao e "tocar agora". Tres olhadas e desiste.
  link[0] = 0;
  for (tent = 0; tent < 3 && !link[0]; tent++) {
    snprintf(rota, sizeof rota, "torrents/info/%s", tid);
    r = get_auth(RD, rota, SRD, &st);
    if (ok2xx(r, st) && pct) *pct = (int)js_num(r, NULL, "progress", -1);
    if (ok2xx(r, st) && js_texto(r, NULL, "status", status, sizeof status)
        && !strcmp(status, "downloaded") && (links = js_array(r, NULL, "links"))
        && *links == '"') {
      const char *fim = strchr(links + 1, '"');
      if (fim && (size_t)(fim - links - 1) < sizeof link) {
        memcpy(link, links + 1, (size_t)(fim - links - 1)); link[fim - links - 1] = 0;
      }
    } else if (r && !ok2xx(r, st)) {
      // so o nao-2xx para: 2xx "ainda nao baixou" e o caso das tres olhadas
      int v = falha(SRD, "info", st, r); free(r); return v;
    }
    free(r);
    if (!link[0]) sleep(1);
  }
  if (!link[0]) {
    printf("[debrid] %s nao esta em cache no Real-Debrid\n", infoHash);
    // ponytail: o torrent fica na lista do RD. Apagar exige DELETE
    // /torrents/delete/<id>, e rede_apagar ja existe — mas remover aqui muda o
    // comportamento visivel da conta de quem usa, e isso e decisao de quem
    // manda, nao efeito colateral de uma correcao de resolvedor.
    return FORA;
  }

  urlenc(enc, sizeof enc, link);
  snprintf(corpo, sizeof corpo, "link=%s", enc);
  r = post_form(RD, "unrestrict/link", SRD, corpo, &st);
  if (!ok2xx(r, st) || !js_texto(r, NULL, "download", url, n)) {
    int v = falha(SRD, "unrestrict", st, r); free(r); return v;
  }
  free(r);
  return 1;
}

// ---------------------------------------------------------------- TorBox
//
// Rotas (api.torbox.app/v1/api, documentacao oficial em api-docs.torbox.app e
// o SDK oficial torbox-sdk-py/src/torbox_api/services/torrents.py):
//   GET  torrents/checkcached?hash=&format=list   (get_torrent_cached_availability)
//   POST torrents/createtorrent                   (create_torrent)
//   GET  torrents/mylist?id=&bypass_cache=true    (get_torrent_list)
//   GET  torrents/requestdl?token=&torrent_id=&file_id=  (request_download_link)
//
// O createtorrent vai como multipart/form-data e NAO como urlencoded: e o que
// o SDK oficial manda (set_body(request_body, "multipart/form-data")) e a
// documentacao nao promete que o outro formato sirva. Montar o multipart a mao
// aqui e feio, mas e o unico formato confirmado — e chutar urlencoded daria um
// 4xx que so aparece na casa de quem tem TorBox.
#define BND "----nuvio-debrid"

// `soCache` 1 = add_only_if_cached (o automatico); 0 = a pessoa escolheu o
// torrent fora de cache e o TorBox deve BAIXAR — e o "P2P" do TorBox.
static char *tb_criar(const char *magnet, int soCache, int *st) {
  char url[300], auth[260], corpo[1200];
  const char *cab[3];
  snprintf(url, sizeof url, TB "/torrents/createtorrent");
  snprintf(auth, sizeof auth, "Authorization: Bearer %s", chave[STB]);
  cab[0] = auth;
  cab[1] = "Content-Type: multipart/form-data; boundary=" BND;
  cab[2] = NULL;
  // add_only_if_cached=true e o que garante a regra desta casa: o servico
  // RECUSA o que nao esta em cache em vez de comecar a baixar. A consulta de
  // cache acima ja filtrou, mas entre uma chamada e outra o item pode sair do
  // cache, e sem isto a TV ficaria esperando um download comecar.
  snprintf(corpo, sizeof corpo,
           "--" BND "\r\nContent-Disposition: form-data; name=\"magnet\"\r\n\r\n%s\r\n"
           "--" BND "\r\nContent-Disposition: form-data; name=\"add_only_if_cached\"\r\n\r\n%s\r\n"
           "--" BND "--\r\n", magnet, soCache ? "true" : "false");
  return rede_postar_st(url, 15, cab, corpo, st);
}

// PRONTO PARA requestdl? Em cache o mylist ja vem com download_finished (ou
// download_present) true. Torrent que o TorBox ainda baixa traz a lista de
// arquivos assim que le os metadados — e o requestdl dele falha. Por isso,
// quando a pessoa mandou baixar, arquivo listado nao basta: vale a bandeira.
static int tbPronto(const char *r) {
  char b[16];
  return (js_bruto(r, NULL, "download_finished", b, sizeof b) && !strncmp(b, "true", 4))
      || (js_bruto(r, NULL, "download_present", b, sizeof b) && !strncmp(b, "true", 4));
}

// `baixar` 0: so em cache (o automatico). 1: a pessoa escolheu o torrent e o
// TorBox deve baixar o que nao tem — sem checkcached (quem chama ja
// perguntou) e sem add_only_if_cached. `pct` recebe o progresso quando o
// torrent ainda esta baixando.
static int resolverTB(const char *infoHash, int fileIdx, char *url, unsigned n,
                      int baixar, int *pct) {
  char h[80], magnet[300], rota[600];
  char *r; int st = 0, fid = -1, tent, tid, pronto = !baixar;
  const char *files, *el;

  hashMin(h, sizeof h, infoHash);
  if (baixar) goto criar;

  // 1) So conteudo JA EM CACHE toca na hora. `data` volta como lista de
  //    {name,size,hash}; lista ausente ou vazia significa fora de cache, e ai
  //    nao se pede nada ao servico.
  snprintf(rota, sizeof rota, "torrents/checkcached?hash=%s&format=list", h);
  r = get_auth(TB, rota, STB, &st);
  // Nao-2xx aqui NAO e "fora de cache": era como o 1.4.0 escrevia, e uma chave
  // recusada (401/403) aparecia no log como se o conteudo so nao estivesse la.
  if (!ok2xx(r, st)) { int v = falha(STB, "checkcached", st, r); free(r); return v; }
  if (!js_array(r, NULL, "data")) {
    printf("[debrid] TorBox: %s fora de cache (HTTP %d)\n", h, st);
    free(r); return FORA;
  }
  free(r);

criar:
  snprintf(magnet, sizeof magnet, "magnet:?xt=urn:btih:%s", h);
  r = tb_criar(magnet, !baixar, &st);
  tid = ok2xx(r, st) ? (int)js_num(r, NULL, "torrent_id", -1) : -1;
  // O 403 DO REGISTRO 1541 SAI AQUI. A linha continua comecando por
  // "TorBox createtorrent: HTTP" (o que se procura no D1), agora com o corpo.
  //
  // HIPOTESE DESCARTADA ANTES DE ESCREVER ISTO: "a libcurl do app nao manda
  // User-Agent e o WAF do TorBox recusa cliente sem UA" (medido com o Trakt:
  // 401 com UA, 403 sem). Nao se aplica — rede.c:617 (GET) e rede.c:768
  // (POST com status) poem "Nuvio/1.0 (webOS)" em TODA requisicao da libcurl,
  // e no Tizen o XHR nao deixa definir UA e o navegador manda o dele. Se o
  // corpo novo vier como pagina HTML de WAF em vez do JSON do TorBox, a
  // pergunta volta a ser o UA (o valor, nao a ausencia).
  // Mandado baixar com a conta no limite de downloads ativos, o TorBox ENFILA
  // o torrent e devolve `queued_id` no lugar de `torrent_id` (campo da
  // documentacao, NAO conferido contra uma resposta real). Nao e falha: e
  // "baixando", so que ainda na fila.
  if (tid < 0 && baixar && ok2xx(r, st) && js_num(r, NULL, "queued_id", -1) >= 0) {
    printf("[debrid] TorBox: %s na fila de downloads do TorBox; fica na conta\n", h);
    if (pct) *pct = -1;
    free(r); return DEBRID_BAIXANDO;
  }
  if (tid < 0) {
    // "Nao esta em cache" no createtorrent do automatico (o item saiu do
    // cache entre as duas chamadas) e fora de cache tambem, nao falha.
    int foraC = !baixar && r && (contem(r, "not cached") || contem(r, "not_cached"));
    int v = falha(STB, "createtorrent", st, r); free(r);
    return v == 0 && foraC ? FORA : v;
  }
  free(r);

  // 3) Os campos aqui sao "name"/"size", e nao "path"/"bytes" do RD — por isso
  //    escolherArquivo recebe os nomes. Tres olhadas de 1 s, como no RD: em
  //    cache a lista ja vem pronta, e se nao vier nao vale travar a TV.
  //    Mandado baixar: as mesmas tres olhadas, mas so vale o torrent PRONTO
  //    (tbPronto). O que o TorBox ainda baixa vira DEBRID_BAIXANDO com o
  //    progresso — a TV nao fica presa minutos num fio de rede, e o torrent
  //    fica na conta para a proxima escolha tocar.
  { char estado[48] = "";
    double prog = -1;
    for (tent = 0; tent < 3 && (fid < 0 || !pronto); tent++) {
      snprintf(rota, sizeof rota, "torrents/mylist?id=%d&bypass_cache=true", tid);
      r = get_auth(TB, rota, STB, &st);
      if (ok2xx(r, st)) {
        if (baixar) {
          pronto = tbPronto(r);
          prog = js_num(r, NULL, "progress", -1);
          js_texto(r, NULL, "download_state", estado, sizeof estado);
        }
        if ((files = js_array(r, NULL, "files"))
            && (el = escolherArquivo(files, fileIdx, "name", "size")) != NULL)
          fid = (int)js_num(el, js_fim(el), "id", -1);
      } else if (r) {
        int v = falha(STB, "mylist", st, r); free(r); return v;
      }
      free(r);
      if (fid < 0 || !pronto) sleep(1);
    }
    if (baixar && !pronto) {
      // progress do TorBox e fracao 0-1; algum cliente antigo mandava 0-100
      int p = prog < 0 ? -1 : prog <= 1.0 ? (int)(prog * 100 + 0.5) : (int)prog;
      if (pct) *pct = p;
      printf("[debrid] TorBox: %s baixando no TorBox (%d%%, estado=%s); fica na conta, "
             "a proxima escolha toca quando terminar\n", h, p, estado[0] ? estado : "-");
      return DEBRID_BAIXANDO;
    }
  }
  if (fid < 0) { printf("[debrid] TorBox: sem video utilizavel em %s\n", h); return 0; }

  // 4) ESTA E A UNICA ROTA QUE LEVA A CHAVE NA QUERY, e nao por escolha nossa:
  //    request_download_link poe `token` em add_query, nao em cabecalho. Logo,
  //    esta URL nunca pode ir para log nem inteira nem em pedaco — o que sai
  //    no fim de debrid_resolver e so a resposta passada por rede_url_publica.
  { char u[1200]; const char *cab[1];
    cab[0] = NULL;
    snprintf(u, sizeof u, TB "/torrents/requestdl?token=%s&torrent_id=%d&file_id=%d&redirect=false",
             chave[STB], tid, fid);
    r = rede_baixar_st(u, 15, cab, &st); }
  if (!ok2xx(r, st) || !js_texto_raiz(r, "data", url, n)) {
    // "requestdl" e nao a URL: a URL leva o token (ver acima)
    int v = falha(STB, "requestdl", st, r); free(r); return v;
  }
  free(r);
  return 1;
}

// ---------------------------------------------------------------- Premiumize
//
// Rotas (www.premiumize.me/api, documentacao oficial):
//   POST cache/check          items[]=<link>  -> {"status","response":[bool],...}
//   POST transfer/directdl    src=<link>      -> {"status","content":[{path,size,link}]}
//
// Nao ha passo de "selecionar arquivo" como no RD: o directdl ja devolve TODOS
// os arquivos do torrent com o link direto de cada um, e a escolha e local.

static int resolverPM(const char *infoHash, int fileIdx, char *url, unsigned n) {
  char magnet[300], enc[500], corpo[600], bruto[64];
  char *r; int st = 0, emCache;
  const char *cont, *el;

  snprintf(magnet, sizeof magnet, "magnet:?xt=urn:btih:%s", infoHash);
  urlenc(enc, sizeof enc, magnet);

  // O magnet inteiro como item, e nao o hash pelado: a documentacao descreve o
  // parametro como "links to check", e o directdl logo abaixo recebe esse
  // mesmo texto em `src` — usar a mesma forma nos dois evita que "esta em
  // cache" e "me da o link" falem de coisas diferentes.
  snprintf(corpo, sizeof corpo, "items%%5B%%5D=%s", enc);
  r = post_form(PM, "cache/check", SPM, corpo, &st);
  // O Premiumize responde erro de conta como 200 {"status":"error",
  // "message":...}, sem "response": falha() cobre os dois (nao-2xx cru, 2xx so
  // com a mensagem).
  if (!ok2xx(r, st) || !js_bruto(r, NULL, "response", bruto, sizeof bruto)) {
    int v = falha(SPM, "cache/check", st, r); free(r); return v;
  }
  // O array de "response" e de BOOLEANOS, e js_array so sabe abrir array de
  // objeto ou de texto — dai a leitura crua.
  emCache = 1;
  {
    const char *q = bruto;
    if (*q == '[') q++;
    while (*q && (unsigned char)*q <= ' ') q++;
    emCache = !strncmp(q, "true", 4);
  }
  free(r);
  if (!emCache) {
    printf("[debrid] Premiumize: %s fora de cache (HTTP %d)\n", infoHash, st);
    return FORA;
  }

  snprintf(corpo, sizeof corpo, "src=%s", enc);
  r = post_form(PM, "transfer/directdl", SPM, corpo, &st);
  if (!ok2xx(r, st) || !(cont = js_array(r, NULL, "content"))) {
    int v = falha(SPM, "directdl", st, r); free(r); return v;
  }
  el = escolherArquivo(cont, fileIdx, "path", "size");
  // "link" e nao "stream_link": o segundo e a versao transcodificada, que nem
  // sempre existe e nem sempre e o arquivo que se pediu.
  if (!el || !js_texto(el, js_fim(el), "link", url, n)) {
    printf("[debrid] Premiumize: sem video utilizavel em %s\n", infoHash);
    free(r); return 0;
  }
  free(r);
  return 1;
}

// ---------------------------------------------------------------- resolver

// Uma volta pelos servicos com chave, na ORDEM FIXA. `baixarTB` 1 = o TorBox
// entra no modo "baixar" (escolha manual, segunda volta); `so` >= 0 limita a
// volta a um servico. Devolve 1 (url pronta), DEBRID_BAIXANDO (quem e quanto
// em *qb/*pct), ou 0. `*fora` recebe a mascara dos servicos que responderam
// "fora de cache".
static int volta(const char *infoHash, int fileIdx, char *url, unsigned n,
                 int baixarTB, int so, int *qb, int *pct, int *fora) {
  int q;
  unsigned g = atomic_load(&geracao);

  // ORDEM FIXA: Real-Debrid, TorBox, Premiumize; ganha o PRIMEIRO QUE
  // RESOLVER, nao o primeiro que tem chave. Quem tem duas contas costuma ter
  // uma principal, e daqui nao ha como saber qual — entao a ordem e sempre a
  // mesma (previsivel no log, que e o que se le no relato de defeito) e cada
  // um so custa uma consulta de cache quando nao tem o conteudo.
  for (q = 0; q < SN; q++) {
    int deu, p = -1;
    if (!chave[q][0] || (so >= 0 && q != so)) continue;
    // Recusado pela conta nesta busca: nem tenta, passa ao proximo servico.
    // E o que faltava no registro 1541 — o Premiumize so era perguntado
    // depois de cada 403 do TorBox, torrent por torrent.
    if (atomic_load(&recusado[q]) || atomic_load(&semPlano[q])) continue;
    url[0] = 0;
    deu = (q == SRD) ? resolverRD(infoHash, fileIdx, url, n, &p)
        : (q == STB) ? resolverTB(infoHash, fileIdx, url, n, baixarTB, &p)
                     : resolverPM(infoHash, fileIdx, url, n);
    if (deu < 0) {
      int zero = 0;
      // so o primeiro fio a ver a recusa escreve a linha, e so se a busca
      // ainda e a mesma (ver `geracao`)
      if (atomic_load(&geracao) == g
          && atomic_compare_exchange_strong(&recusado[q], &zero, -deu))
        printf("[debrid] %s: recusa da conta (HTTP %d), fora do resto desta busca\n",
               nomeServ[q], -deu);
      url[0] = 0;
      continue;
    }
    if (deu == FORA) {
      if (fora) *fora |= 1 << q;
      // O Real-Debrid ja COMECOU a baixar (addMagnet + selectFiles): para a
      // escolha manual isso e "baixando", com o progresso que ele deu.
      if (q == SRD && qb && *qb < 0) { *qb = SRD; if (pct) *pct = p; }
      url[0] = 0;
      continue;
    }
    if (deu == DEBRID_BAIXANDO) {
      if (qb && *qb < 0) { *qb = q; if (pct) *pct = p; }
      url[0] = 0;
      continue;
    }
    if (deu && url[0]) {
      // O CAMINHO DESTA URL E A CREDENCIAL: e o link direto que o servico
      // devolve, e quem o tem baixa usando a conta de quem pediu. Ver
      // rede_url_publica em rede.h. Vale para os tres — o /d/<chave>/ do RD,
      // o link assinado do TorBox e o do Premiumize.
      char seg[120];
      printf("[debrid] %s: %s -> %s\n", nomeServ[q], infoHash,
             rede_url_publica(url, seg, sizeof seg));
      return 1;
    }
    url[0] = 0;
  }
  return qb && *qb >= 0 ? DEBRID_BAIXANDO : 0;
}

int debrid_resolver(const char *infoHash, int fileIdx, char *url, unsigned n) {
  int fora = 0, r;
  if (!infoHash || !*infoHash || !url || n == 0) return 0;
  r = volta(infoHash, fileIdx, url, n, 0, -1, NULL, NULL, &fora);
  if (r != 1 && fora) atomic_fetch_add(&foraCache, 1);
  return r == 1;
}

int debrid_resolver_escolhido(const char *infoHash, int fileIdx, char *url,
                              unsigned n, char *servico, unsigned ns, int *pct) {
  int fora = 0, qb = -1, p = -1, r;
  if (servico && ns) servico[0] = 0;
  if (pct) *pct = -1;
  if (!infoHash || !*infoHash || !url || n == 0) return 0;
  printf("[debrid] escolha manual do torrent %s: em cache primeiro, depois baixar\n",
         infoHash);
  // 1a volta: em cache em qualquer servico — exatamente como o automatico.
  r = volta(infoHash, fileIdx, url, n, 0, -1, &qb, &p, &fora);
  // 2a volta: o TorBox respondeu "fora de cache" e a pessoa quer ESTE
  // torrent: manda baixar. So o TorBox precisa disto — o Real-Debrid ja
  // comecou a baixar na 1a volta (addMagnet), e o Premiumize fica como estava
  // (o transfer/create dele ninguem pediu e nao tem teste).
  if (r != 1 && (fora & (1 << STB))) {
    int qb2 = -1, p2 = -1;
    r = volta(infoHash, fileIdx, url, n, 1, STB, &qb2, &p2, NULL);
    if (r == DEBRID_BAIXANDO) { qb = qb2; p = p2; }
  }
  if (r == 1) return 1;
  if (qb >= 0) {
    if (servico && ns) snprintf(servico, ns, "%s", nomeServ[qb]);
    if (pct) *pct = p;
    return DEBRID_BAIXANDO;
  }
  return 0;
}
