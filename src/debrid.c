#include "debrid.h"
#include "rede.h"
#include "js.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>

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
  snprintf(chave[q], sizeof chave[q], "%s", k);
  printf("[debrid] chave do %s vinda da conta\n", nomeServ[q]);
}
int debrid_ativo(void) {
  int q;
  for (q = 0; q < SN; q++) if (chave[q][0]) return 1;
  return 0;
}
void debrid_esquecer(void) { memset(chave, 0, sizeof chave); alvoT = alvoE = 0; }
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

static int resolverRD(const char *infoHash, int fileIdx, char *url, unsigned n) {
  char corpo[700], enc[600], rota[120], tid[64], status[32], link[600];
  char *r; int st = 0, id, tent;
  const char *files, *links, *el;

  snprintf(corpo, sizeof corpo, "magnet:?xt=urn:btih:%s", infoHash);
  urlenc(enc, sizeof enc, corpo);
  snprintf(corpo, sizeof corpo, "magnet=%s", enc);
  r = post_form(RD, "torrents/addMagnet", SRD, corpo, &st);
  if (!ok2xx(r, st) || !js_texto(r, NULL, "id", tid, sizeof tid)) {
    printf("[debrid] addMagnet: HTTP %d\n", st); free(r); return 0;
  }
  free(r);

  snprintf(rota, sizeof rota, "torrents/info/%s", tid);
  r = get_auth(RD, rota, SRD, &st);
  if (!ok2xx(r, st) || !(files = js_array(r, NULL, "files"))) {
    printf("[debrid] info: HTTP %d\n", st); free(r); return 0;
  }
  el = escolherArquivo(files, fileIdx, "path", "bytes");
  id = el ? (int)js_num(el, js_fim(el), "id", -1) : -1;
  free(r);
  if (id < 0) { printf("[debrid] torrent sem video utilizavel\n"); return 0; }

  snprintf(rota, sizeof rota, "torrents/selectFiles/%s", tid);
  snprintf(corpo, sizeof corpo, "files=%d", id);
  r = post_form(RD, rota, SRD, corpo, &st);
  free(r);
  if (!(st == 204 || st == 202 || (st >= 200 && st < 300))) {
    printf("[debrid] selectFiles: HTTP %d\n", st); return 0;
  }

  // Em cache o RD marca "downloaded" quase na hora; fora de cache ele
  // comecaria a BAIXAR — e isso nao e "tocar agora". Tres olhadas e desiste.
  link[0] = 0;
  for (tent = 0; tent < 3 && !link[0]; tent++) {
    snprintf(rota, sizeof rota, "torrents/info/%s", tid);
    r = get_auth(RD, rota, SRD, &st);
    if (ok2xx(r, st) && js_texto(r, NULL, "status", status, sizeof status)
        && !strcmp(status, "downloaded") && (links = js_array(r, NULL, "links"))
        && *links == '"') {
      const char *fim = strchr(links + 1, '"');
      if (fim && (size_t)(fim - links - 1) < sizeof link) {
        memcpy(link, links + 1, (size_t)(fim - links - 1)); link[fim - links - 1] = 0;
      }
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
    return 0;
  }

  urlenc(enc, sizeof enc, link);
  snprintf(corpo, sizeof corpo, "link=%s", enc);
  r = post_form(RD, "unrestrict/link", SRD, corpo, &st);
  if (!ok2xx(r, st) || !js_texto(r, NULL, "download", url, n)) {
    printf("[debrid] unrestrict: HTTP %d\n", st); free(r); return 0;
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

static char *tb_criar(const char *magnet, int *st) {
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
           "--" BND "\r\nContent-Disposition: form-data; name=\"add_only_if_cached\"\r\n\r\ntrue\r\n"
           "--" BND "--\r\n", magnet);
  return rede_postar_st(url, 15, cab, corpo, st);
}

static int resolverTB(const char *infoHash, int fileIdx, char *url, unsigned n) {
  char h[80], magnet[300], rota[600];
  char *r; int st = 0, fid = -1, tent, tid;
  const char *files, *el;

  hashMin(h, sizeof h, infoHash);

  // 1) So conteudo JA EM CACHE toca na hora. `data` volta como lista de
  //    {name,size,hash}; lista ausente ou vazia significa fora de cache, e ai
  //    nao se pede nada ao servico.
  snprintf(rota, sizeof rota, "torrents/checkcached?hash=%s&format=list", h);
  r = get_auth(TB, rota, STB, &st);
  if (!ok2xx(r, st) || !js_array(r, NULL, "data")) {
    printf("[debrid] TorBox: %s fora de cache (HTTP %d)\n", h, st);
    free(r); return 0;
  }
  free(r);

  snprintf(magnet, sizeof magnet, "magnet:?xt=urn:btih:%s", h);
  r = tb_criar(magnet, &st);
  tid = r ? (int)js_num(r, NULL, "torrent_id", -1) : -1;
  free(r);
  if (tid < 0) { printf("[debrid] TorBox createtorrent: HTTP %d\n", st); return 0; }

  // 3) Os campos aqui sao "name"/"size", e nao "path"/"bytes" do RD — por isso
  //    escolherArquivo recebe os nomes. Tres olhadas de 1 s, como no RD: em
  //    cache a lista ja vem pronta, e se nao vier nao vale travar a TV.
  for (tent = 0; tent < 3 && fid < 0; tent++) {
    snprintf(rota, sizeof rota, "torrents/mylist?id=%d&bypass_cache=true", tid);
    r = get_auth(TB, rota, STB, &st);
    if (ok2xx(r, st) && (files = js_array(r, NULL, "files"))
        && (el = escolherArquivo(files, fileIdx, "name", "size")) != NULL)
      fid = (int)js_num(el, js_fim(el), "id", -1);
    free(r);
    if (fid < 0) sleep(1);
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
    printf("[debrid] TorBox requestdl: HTTP %d\n", st); free(r); return 0;
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
  // O array de "response" e de BOOLEANOS, e js_array so sabe abrir array de
  // objeto ou de texto — dai a leitura crua.
  emCache = ok2xx(r, st) && js_bruto(r, NULL, "response", bruto, sizeof bruto);
  if (emCache) {
    const char *q = bruto;
    if (*q == '[') q++;
    while (*q && (unsigned char)*q <= ' ') q++;
    emCache = !strncmp(q, "true", 4);
  }
  free(r);
  if (!emCache) {
    printf("[debrid] Premiumize: %s fora de cache (HTTP %d)\n", infoHash, st);
    return 0;
  }

  snprintf(corpo, sizeof corpo, "src=%s", enc);
  r = post_form(PM, "transfer/directdl", SPM, corpo, &st);
  if (!ok2xx(r, st) || !(cont = js_array(r, NULL, "content"))) {
    printf("[debrid] Premiumize directdl: HTTP %d\n", st); free(r); return 0;
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

int debrid_resolver(const char *infoHash, int fileIdx, char *url, unsigned n) {
  int q;
  if (!infoHash || !*infoHash || !url || n == 0) return 0;

  // ORDEM FIXA: Real-Debrid, TorBox, Premiumize; ganha o PRIMEIRO QUE
  // RESOLVER, nao o primeiro que tem chave. Quem tem duas contas costuma ter
  // uma principal, e daqui nao ha como saber qual — entao a ordem e sempre a
  // mesma (previsivel no log, que e o que se le no relato de defeito) e cada
  // um so custa uma consulta de cache quando nao tem o conteudo.
  for (q = 0; q < SN; q++) {
    int deu;
    if (!chave[q][0]) continue;
    url[0] = 0;
    deu = (q == SRD) ? resolverRD(infoHash, fileIdx, url, n)
        : (q == STB) ? resolverTB(infoHash, fileIdx, url, n)
                     : resolverPM(infoHash, fileIdx, url, n);
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
  return 0;
}
