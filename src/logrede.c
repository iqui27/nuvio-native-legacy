// Ver logrede.h para o porque e as regras.
#include "logrede.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

// --- limpeza de uma linha (vale em todo alvo, inclusive no teste) -----------

// URL cortada no host: "https://host/caminho?x" -> "https://host/…". Como o
// registro.c (mascarar): so troca quando sobra 4 bytes ou mais, nunca cresce.
static void cortarUrls(char *s) {
  char *p = s;
  while ((p = strstr(p, "://")) != NULL) {
    char *h = p + 3, *at = NULL, *fim, *q;
    for (q = h; *q && *q != '/' && *q != '?' && *q != ' ' && *q != '"' && *q != '\''; q++)
      if (*q == '@') at = q;
    // userinfo (usuario:senha@) fora, como rede_url_publica faz.
    if (at) { memmove(h, at + 1, strlen(at + 1) + 1); q -= at + 1 - h; }
    if (*q != '/' && *q != '?') { p = q; continue; }
    fim = q;
    while (*fim && *fim != ' ' && *fim != '"' && *fim != '\'' && *fim != '>' && *fim != ')') fim++;
    if (fim - q < 4) { p = fim; continue; }
    memmove(q + 4, fim, strlen(fim) + 1);
    memcpy(q, "/\xe2\x80\xa6", 4);
    p = q + 4;
  }
}

// Esquema sem "//" (magnet:, etc.): se o valor tem '=' (parametros), tudo do
// ':' ao fim do token vira "…". Em magnet: caem xt (hash), dn (titulo) e
// tr (rastreador) — os tres sensiveis.
static void cortarEsquemas(char *s) {
  for (char *p = s; *p; p++) {
    char *a = p, *fim;
    if (!isalpha((unsigned char)*p)) continue;
    if (p > s && (isalnum((unsigned char)p[-1]) || p[-1] == '+' || p[-1] == '-' ||
                  p[-1] == '.' || p[-1] == ':')) continue;
    while (isalnum((unsigned char)*p) || *p == '+' || *p == '-' || *p == '.') p++;
    if (*p != ':' || p - a < 2 || p - a > 16 || p[1] == '/' || p[1] == ':') continue;
    fim = p + 1;
    while (*fim && *fim != ' ' && *fim != '"' && *fim != '\'' && *fim != '>' && *fim != ')') fim++;
    if (!memchr(p + 1, '=', (size_t)(fim - p - 1)) || fim - p < 5) continue;
    memmove(p + 4, fim, strlen(fim) + 1);
    memcpy(p, ":\xe2\x80\xa6", 4);
    p += 4;
  }
}

// "apikey=XYZ", "token: XYZ", "Authorization: Bearer XYZ" -> valor "…".
static void cortarSegredos(char *s) {
  static const char *const CH[] = { "apikey", "api_key", "api-key", "token", "password", "passwd",
                                    "senha", "secret", "authorization", "bearer", "auth", "key",
                                    "pass", "cookie", "sessionid", "signature", "sig" };
  for (size_t k = 0; k < sizeof CH / sizeof *CH; k++) {
    size_t n = strlen(CH[k]);
    char *p = s;
    while (*p) {
      char *q, *v, *fim;
      if (strncasecmp(p, CH[k], n)) { p++; continue; }
      // palavra inteira a esquerda ("monkey=" nao conta como "key=")
      if (p > s && (isalnum((unsigned char)p[-1]) || p[-1] == '_')) { p += n; continue; }
      q = p + n;
      if (*q == '"' || *q == '\'') q++;
      if (!strcasecmp(CH[k], "bearer")) { if (*q != ' ') { p = q; continue; } }
      else if (*q != '=' && *q != ':') { p = q; continue; }
      else q++;
      while (*q == ' ' || *q == '"' || *q == '\'') q++;
      if (!strncasecmp(q, "bearer ", 7)) q += 7;
      v = q; fim = v;
      while (*fim && *fim != ' ' && *fim != '&' && *fim != '"' && *fim != '\'' && *fim != ',' &&
             *fim != ';' && *fim != '}' && *fim != ')') fim++;
      if (fim - v >= 3) {
        memmove(v + 3, fim, strlen(fim) + 1);
        memcpy(v, "\xe2\x80\xa6", 3);
        fim = v + 3;
      }
      p = fim;
    }
  }
}

// Sequencia longa de letras E numeros (chave de debrid, hash, token): fica o
// comeco, o resto vira "…".
static void cortarLongas(char *s) {
  char *p = s;
  while (*p) {
    char *a = p;
    // Sequencia longa do alfabeto de token/chave (letras, numeros e os
    // extras de base64): mesmo sem digito pode ser uma chave (hex de
    // rastreador, token so de letras), entao basta o comprimento.
    while (isalnum((unsigned char)*p) || *p == '_' || *p == '+' || *p == '/' ||
           *p == '=' || *p == '-') p++;
    if (p - a >= 32) {
      memmove(a + 7, p, strlen(p) + 1);
      memcpy(a + 4, "\xe2\x80\xa6", 3);
      p = a + 7;
    }
    if (*p) p++;
  }
}

void logrede_limpar(char *linha) {
  if (!linha) return;
  cortarUrls(linha);
  cortarEsquemas(linha);
  cortarSegredos(linha);
  cortarLongas(linha);
}

#ifdef __EMSCRIPTEN__
void logrede_configurar(int ligado, const char *arquivo) { (void)ligado; (void)arquivo; }
const char *logrede_url(void) { return ""; }
#else

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#define LR_CAB_MAX   4096
#define LR_INICIO    (512 * 1024)        // primeira carga: o fim do arquivo
#define LR_PASSO     (1024 * 1024)       // no maximo por pedido incremental
#define LR_BAIXAR    (4 * 1024 * 1024)   // "Baixar": o fim do arquivo, ate 4 MB
#define LR_FILTRO_LER (8 * 1024 * 1024)  // filtro "escolha da fonte": varre ate 8 MB do fim
#define LR_FILTRO_MAX (1024 * 1024)      // e devolve no maximo o ultimo 1 MB do que casou

// AS ABAS DA PAGINA (dono, 16/10): cada uma e um filtro que roda AQUI, no
// servidor, sobre ate 8 MB do fim do log (o log tem uma linha de FPS por
// segundo; filtrar so no navegador sobre 512 KB perdia os blocos de minutos
// atras). Termos em minuscula; a linha e comparada em minuscula.
//   1 Source picking      — como a fonte foi escolhida
//   2 Won't open          — o player recusou a fonte antes de tocar
//   3 Error while playing — a fonte tocou e depois deu erro / parou
static const char *const F_FONTE[] = {
  "[fonte", "[voltafonte", "[addons", "[addon]", "[preparo", "[proximo", "[debrid", "[verif",
  "[next", "proximo episodio", "buscando fontes", "automatico", "auto-play", "auto pick",
  "bingegroup", "fonte escolhida", "nenhuma fonte", "[partes]", "[logrede]", NULL
};
static const char *const F_ABRIR[] = {
  "[abrir-diag]", "abrir:", "pipeline open", "picked in the sheet", "sheet opened", "hand-picked",
  "escolha manual", "erro do player", "sonda apos recusa", "nao abriu", "sem sinal de abertura",
  "fonte escolhida", "automatico (verificado)", "tocando enquanto confere", "[voltafonte]", "failover",
  "[host] erro", "[host] abrir", "[host] interrompido", "[host] cabecalho", "[mkv]", "[dts-tpk]",
  "[video] tpk", "[fonte-cdn]", "duration", "[partes]", "[mkvfix]", "[logrede]", NULL
};
static const char *const F_TOCAR[] = {
  "[tocar-diag]", "[host] erro", "[host] interrompido", "[video]", "[player]", "[dts-tpk]", "[dts-range]",
  "[dts]", "[audio]", "[mkv]", "[tv] ", "reconex", "reconect", "failover", "escolha manual",
  "tocando agora", "fonte escolhida", "pipeline open", "opened ok", "[abrir-diag] started", "[fonte-cdn]", "[partes]", "[mkvfix]", "[logrede]",
  "[seek]", NULL
};
static const char *const *const FILTROS[4] = { NULL, F_FONTE, F_ABRIR, F_TOCAR };
static const char *const NOMES_ARQ[4] = { "nuvio-log.txt", "nuvio-source-picking.txt", "nuvio-wont-open.txt",
                                          "nuvio-error-while-playing.txt" };
static int linhaDoFiltro(const char *l, int modo) {
  char b[1024];
  size_t i;
  if (modo <= 0 || modo > 3) return 1;
  for (i = 0; i + 1 < sizeof b && l[i]; i++) b[i] = (char)tolower((unsigned char)l[i]);
  b[i] = 0;
  for (int k = 0; FILTROS[modo][k]; k++) if (strstr(b, FILTROS[modo][k])) return 1;
  return 0;
}

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static pthread_t fio;
static int vivo, ouvir = -1, acorda[2] = { -1, -1 }, porta;
static char arqLog[600], url[96];
static time_t proximaTentativa, ipQuando;

static int privado(in_addr_t a) {
  unsigned h = ntohl(a);
  // 10/8, 172.16/12, 192.168/16, 127/8, e tambem 100.64/10 (CGNAT de mesh e
  // de operadora) e 169.254/16 (link-local): redes de casa que nao sao RFC 1918.
  return (h >> 24) == 10 || (h >> 20) == 0xAC1 || (h >> 16) == 0xC0A8 || (h >> 24) == 127 ||
         (h >> 22) == (0x64400000u >> 22) || (h >> 16) == 0xA9FE;
}

// O IP da TV na rede de casa (a mesma conta de celular.c).
static int ipLan(char *dst, size_t n) {
  struct sockaddr_in alvo, eu;
  socklen_t tam = sizeof eu;
  struct ifaddrs *lista = NULL, *i;
  int s = socket(AF_INET, SOCK_DGRAM, 0), ok = 0;
  in_addr_t achou = 0;
  if (s >= 0) {
    memset(&alvo, 0, sizeof alvo);
    alvo.sin_family = AF_INET; alvo.sin_port = htons(9);
    inet_pton(AF_INET, "192.0.2.1", &alvo.sin_addr);
    if (!connect(s, (struct sockaddr *)&alvo, sizeof alvo) &&
        !getsockname(s, (struct sockaddr *)&eu, &tam) && privado(eu.sin_addr.s_addr) &&
        (ntohl(eu.sin_addr.s_addr) >> 24) != 127)
      achou = eu.sin_addr.s_addr;
    close(s);
  }
  if (!achou && !getifaddrs(&lista)) {
    for (i = lista; i; i = i->ifa_next) {
      in_addr_t a;
      if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET) continue;
      if (!(i->ifa_flags & IFF_UP) || (i->ifa_flags & IFF_LOOPBACK)) continue;
      a = ((struct sockaddr_in *)i->ifa_addr)->sin_addr.s_addr;
      if (privado(a) && (ntohl(a) >> 24) != 127) { achou = a; break; }
    }
    freeifaddrs(lista);
  }
  if (achou) { struct in_addr a; a.s_addr = achou; ok = inet_ntop(AF_INET, &a, dst, (socklen_t)n) != NULL; }
  return ok;
}

static int enviarTudo(int c, const char *b, size_t n) {
  while (n) {
    ssize_t w = send(c, b, n, MSG_NOSIGNAL);
    if (w <= 0) { if (w < 0 && errno == EINTR) continue; return -1; }
    b += w; n -= (size_t)w;
  }
  return 0;
}

static void responderTexto(int c, int codigo, const char *tipo, const char *extra, const char *corpo, size_t n) {
  char cab[512];
  int k = snprintf(cab, sizeof cab,
                   "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %lu\r\nCache-Control: no-store\r\n"
                   "X-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\nConnection: close\r\n%s\r\n",
                   codigo, codigo == 200 ? "OK" : codigo == 404 ? "Not Found" : codigo == 403 ? "Forbidden" : "Bad Request",
                   tipo, (unsigned long)n, extra ? extra : "");
  if (enviarTudo(c, cab, (size_t)k) == 0 && n) enviarTudo(c, corpo, n);
}

static const char PAGINA[] =
"<!doctype html><html lang=en><head><meta charset=utf-8>"
"<meta name=viewport content='width=device-width,initial-scale=1'>"
"<title>Nuvio log</title><style>"
":root{color-scheme:dark;--bg:#0e1116;--fg:#d6dbe3;--dim:#7d8590;--fonte:#7ee0a1;--diag:#f2c45a;--next:#7cc4ff;--erro:#ff8a80;--abrir:#e0a8ff}"
"body{margin:0;background:var(--bg);color:var(--fg);font:13px/1.45 ui-monospace,Menlo,Consolas,monospace}"
"header{position:sticky;top:0;background:#151a21;padding:8px 12px;display:flex;flex-wrap:wrap;gap:8px;align-items:center;border-bottom:1px solid #222a35}"
"header b{font:600 14px system-ui,sans-serif;margin-right:6px}"
"button,input,a.b{background:#1f2630;color:var(--fg);border:1px solid #2d3745;border-radius:6px;padding:5px 9px;font:12px system-ui,sans-serif;text-decoration:none}"
"button.on{background:#2b5b3f;border-color:#3f8a5e}input#q{min-width:160px;flex:1;max-width:320px}"
"#st,#cfg{color:var(--dim);font:12px system-ui,sans-serif}#cfg label{margin-right:12px;white-space:nowrap}#cfg{width:100%}"
"#log{padding:8px 12px 40px;white-space:pre-wrap;word-break:break-word}"
".l{padding:1px 0}.f{color:var(--fonte)}.d{color:var(--diag)}.n{color:var(--next)}.e{color:var(--erro)}.a{color:var(--abrir)}.h{color:#fff;font-weight:700;margin-top:10px;border-top:1px solid #333}.x{display:none}"
"</style></head><body><header><b>Nuvio log</b>"
"<button id=b1 class=on>Source picking</button><button id=b2>Won't open</button><button id=b3>Error while playing</button><button id=b0>Everything</button>"
"<input id=q placeholder='Search (e.g. REFUSED, track #, T7)'>"
"<button id=ba class=on>Auto-scroll</button><button id=bc>Clear</button>"
"<a class=b id=dl href='/fonte.txt?baixar=1'>Download</a><span id=st></span>"
"</header>"
"<div id=log></div><script>"
"var de=-1,f=1,auto=1,q='',gen=0,L=document.getElementById('log'),S=document.getElementById('st');"
"var ARQ=['/log.txt','/fonte.txt','/abrir.txt','/tocar.txt'],BT=[b0,b1,b2,b3];"
"function cls(t){return /=====/.test(t)?'h':/REFUSED|FAILED|failed AFTER|NOT CHANGED/.test(t)?'e':/\\[abrir-diag\\]|\\[tocar-diag\\]/.test(t)?'a':/\\[fonte-diag\\]/.test(t)?'d':/next/i.test(t)?'n':/(falh|erro|error|fail|refused)/i.test(t)?'e':/\\[fonte/.test(t)?'f':''}"
"function vis(d){return !q||d.textContent.toLowerCase().indexOf(q)>=0}"
"function add(txt){var fr=document.createDocumentFragment();txt.split('\\n').forEach(function(t){if(!t)return;"
"var d=document.createElement('div');d.className='l '+cls(t);d.textContent=t;if(!vis(d))d.classList.add('x');fr.appendChild(d)});"
"L.appendChild(fr);while(L.childNodes.length>20000)L.removeChild(L.firstChild);if(auto)window.scrollTo(0,document.body.scrollHeight)}"
"function filtrar(){for(var i=0;i<L.childNodes.length;i++){var d=L.childNodes[i];d.classList.toggle('x',!vis(d))}if(auto)window.scrollTo(0,document.body.scrollHeight)}"
"function puxar(){var g=gen;fetch('/log.txt?f='+f+'&de='+de,{cache:'no-store'}).then(function(r){if(g!==gen)return '';var x=r.headers.get('X-Fim');"
"if(r.headers.get('X-Novo')==='1')L.textContent='';if(x!==null)de=+x;return r.text()}).then(function(t){if(g!==gen)return;if(t)add(t);"
"S.textContent=(L.childNodes.length?L.childNodes.length+' lines':'no lines yet')+' • live • '+new Date().toLocaleTimeString()})"
".catch(function(){S.textContent='TV not answering, retrying…'}).then(function(){if(g===gen)setTimeout(puxar,1500)})}"
"function modo(v){f=v;for(var i=0;i<4;i++)BT[i].classList.toggle('on',i===v);"
"dl.href=ARQ[v]+'?baixar=1';gen++;de=-1;L.textContent='';S.textContent='loading…';puxar()}"
"b0.onclick=function(){modo(0)};b1.onclick=function(){modo(1)};b2.onclick=function(){modo(2)};b3.onclick=function(){modo(3)};"
"ba.onclick=function(){auto=!auto;ba.classList.toggle('on',auto)};bc.onclick=function(){L.textContent=''};"
"document.getElementById('q').oninput=function(e){q=e.target.value.toLowerCase();filtrar()};"
"puxar();"
"</script></body></html>";

// Le [ini, ini+max) do arquivo, so linhas inteiras, limpas. *fim = onde parou.
static char *lerTrecho(const char *arq, long de, long max, int filtrar, long *fim, int *novo, size_t *n) {
  // filtrar: 0 tudo, 1..3 a aba (FILTROS)
  FILE *f = fopen(arq, "rb");
  struct stat st;
  long tam, ini;
  char *bruto, *saida, *p, *o;
  size_t lidos;
  *n = 0; *novo = 0; *fim = 0;
  if (!f) return NULL;
  if (fstat(fileno(f), &st) != 0) { fclose(f); return NULL; }
  tam = (long)st.st_size;
  if (de < 0 || de > tam) {   // primeira carga, ou o arquivo recomecou
    *novo = de >= 0;
    ini = tam > max ? tam - max : 0;
  } else ini = de;
  if (tam - ini > max) ini = tam - max;
  bruto = malloc((size_t)(tam - ini) + 1);
  saida = malloc((size_t)(tam - ini) + 1);
  if (!bruto || !saida) { free(bruto); free(saida); fclose(f); return NULL; }
  fseek(f, ini, SEEK_SET);
  lidos = fread(bruto, 1, (size_t)(tam - ini), f);
  fclose(f);
  bruto[lidos] = 0;
  p = bruto;
  // Comecou no meio de uma linha (corte por tamanho): pula ate a proxima.
  if (ini > 0 && (de < 0 || ini != de)) { char *nl = memchr(p, '\n', lidos); p = nl ? nl + 1 : bruto + lidos; }
  { char *ult = NULL;
    for (char *q = bruto + lidos; q > p; q--) if (q[-1] == '\n') { ult = q; break; }
    if (!ult) { *fim = ini + (long)(p - bruto); free(bruto); free(saida); saida = NULL; return calloc(1, 1); }
    *ult = 0;
    *fim = ini + (long)(ult - bruto); }
  o = saida;
  while (*p) {
    char *nl = strchr(p, '\n'), *ln = p;
    size_t k;
    if (nl) *nl = 0;
    if (linhaDoFiltro(ln, filtrar)) {
      logrede_limpar(ln);
      k = strlen(ln);
      memcpy(o, ln, k); o += k; *o++ = '\n';
    }
    if (!nl) break;
    p = nl + 1;
  }
  *o = 0;
  *n = (size_t)(o - saida);
  // Filtrado: so o fim do que casou, comecando numa linha inteira.
  if (filtrar && *n > LR_FILTRO_MAX) {
    char *c = saida + *n - LR_FILTRO_MAX, *nl = strchr(c, '\n');
    c = nl ? nl + 1 : c;
    *n = (size_t)(o - c);
    memmove(saida, c, *n + 1);
  }
  free(bruto);
  return saida;
}

static void atender(int c) {
  char cab[LR_CAB_MAX + 1], metodo[8], caminho[256], arq[600];
  size_t k = 0;
  struct sockaddr_in par;
  socklen_t tp = sizeof par;
  struct timeval tv = { 2, 0 }, tvs = { 8, 0 };
  setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
  setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, &tvs, sizeof tvs);
#ifdef SO_NOSIGPIPE
  { int um = 1; setsockopt(c, SOL_SOCKET, SO_NOSIGPIPE, &um, sizeof um); }
#endif
  if (getpeername(c, (struct sockaddr *)&par, &tp) != 0 || par.sin_family != AF_INET ||
      !privado(par.sin_addr.s_addr)) {
    // Com o motivo na tela: uma pagina em branco parecia "quebrado".
    char msg[160], ip[48] = "?";
    int k;
    if (par.sin_family == AF_INET) inet_ntop(AF_INET, &par.sin_addr, ip, sizeof ip);
    k = snprintf(msg, sizeof msg, "Nuvio log: this page only answers devices on the home network (your address: %s).\n", ip);
    printf("[logrede] refused %s (not a home-network address)\n", ip);
    fflush(stdout);
    responderTexto(c, 403, "text/plain; charset=utf-8", NULL, msg, (size_t)k);
    return;
  }
  while (k < LR_CAB_MAX) {
    ssize_t r = recv(c, cab + k, LR_CAB_MAX - k, 0);
    if (r <= 0) break;
    k += (size_t)r; cab[k] = 0;
    if (strstr(cab, "\r\n\r\n")) break;
  }
  cab[k] = 0;
  if (sscanf(cab, "%7s %255s", metodo, caminho) != 2 || strcmp(metodo, "GET")) {
    responderTexto(c, 400, "text/plain", NULL, "", 0);
    return;
  }
  pthread_mutex_lock(&trava);
  snprintf(arq, sizeof arq, "%s", arqLog);
  pthread_mutex_unlock(&trava);
  if (!strcmp(caminho, "/") || !strcmp(caminho, "/log") || !strcmp(caminho, "/log/")) {
    responderTexto(c, 200, "text/html; charset=utf-8", NULL, PAGINA, sizeof PAGINA - 1);
    return;
  }
  if (!strncmp(caminho, "/log.txt", 8) || !strncmp(caminho, "/fonte.txt", 10) ||
      !strncmp(caminho, "/abrir.txt", 10) || !strncmp(caminho, "/tocar.txt", 10)) {
    const char *qs = strchr(caminho, '?'), *v;
    long de = -1, fim = 0, max = LR_PASSO;
    int novo = 0, baixar = 0, filtrar = !strncmp(caminho, "/fonte.txt", 10) ? 1 : !strncmp(caminho, "/abrir.txt", 10) ? 2
                                      : !strncmp(caminho, "/tocar.txt", 10) ? 3 : 0;
    size_t n = 0;
    char *txt, extra[200];
    if (qs && (v = strstr(qs, "de="))) de = strtol(v + 3, NULL, 10);
    if (qs && ((v = strstr(qs, "?f=")) || (v = strstr(qs, "&f=")))) { filtrar = atoi(v + 3); if (filtrar < 0 || filtrar > 3) filtrar = 0; }
    if (qs && strstr(qs, "baixar=1")) { baixar = 1; de = -1; max = filtrar ? LR_FILTRO_LER : LR_BAIXAR; }
    else if (de < 0) max = filtrar ? LR_FILTRO_LER : LR_INICIO;
    if (!arq[0] || !(txt = lerTrecho(arq, de, max, filtrar, &fim, &novo, &n))) {
      static const char sem[] = "[logrede] no log file on this build yet\n";
      responderTexto(c, 200, "text/plain; charset=utf-8", "X-Fim: 0\r\n", de < 0 ? sem : "", de < 0 ? sizeof sem - 1 : 0);
      return;
    }
    if (baixar) snprintf(extra, sizeof extra, "X-Fim: %ld\r\nX-Novo: %d\r\nContent-Disposition: attachment; filename=\"%s\"\r\n",
                         fim, novo, NOMES_ARQ[filtrar]);
    else snprintf(extra, sizeof extra, "X-Fim: %ld\r\nX-Novo: %d\r\n", fim, novo);
    responderTexto(c, 200, "text/plain; charset=utf-8", extra, txt, n);
    free(txt);
    return;
  }
  responderTexto(c, 404, "text/plain", NULL, "", 0);
}

// UM FIO POR CONEXAO, no maximo LR_CONEXOES ao mesmo tempo. Com um fio so, a
// conexao que o Chrome abre adiantada e nao usa (preconnect) segurava o
// atendimento ate o tempo de leitura acabar, e as seguintes ficavam na fila:
// a pagina "carregava sem nunca abrir". O painel de 8099 da etapa de DTS, que
// funcionava nesta TV, tambem atendia em fios proprios.
#define LR_CONEXOES 6
static pthread_mutex_t contTrava = PTHREAD_MUTEX_INITIALIZER;
static int emAtendimento;
static void *atenderFio(void *u) {
  int c = (int)(intptr_t)u;
  atender(c);
  shutdown(c, SHUT_WR);
  close(c);
  pthread_mutex_lock(&contTrava); emAtendimento--; pthread_mutex_unlock(&contTrava);
  return NULL;
}

static void *servir(void *u) {
  (void)u;
  for (;;) {
    struct pollfd p[2];
    int c, cheio;
    pthread_t t;
    pthread_attr_t at;
    p[0].fd = ouvir; p[0].events = POLLIN; p[0].revents = 0;
    p[1].fd = acorda[0]; p[1].events = POLLIN; p[1].revents = 0;
    if (poll(p, 2, 1000) < 0) { if (errno == EINTR) continue; break; }
    if (p[1].revents) break;
    if (!(p[0].revents & POLLIN)) continue;
    c = accept(ouvir, NULL, NULL);
    if (c < 0) continue;
    fcntl(c, F_SETFD, FD_CLOEXEC);
    pthread_mutex_lock(&contTrava);
    cheio = emAtendimento >= LR_CONEXOES;
    if (!cheio) emAtendimento++;
    pthread_mutex_unlock(&contTrava);
    if (cheio) { close(c); continue; }
    pthread_attr_init(&at);
    pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&t, &at, atenderFio, (void *)(intptr_t)c) != 0) {
      close(c);
      pthread_mutex_lock(&contTrava); emAtendimento--; pthread_mutex_unlock(&contTrava);
    }
    pthread_attr_destroy(&at);
  }
  return NULL;
}

static void atualizarUrl(void) {
  char ip[64];
  if (ipLan(ip, sizeof ip)) snprintf(url, sizeof url, "http://%s:%d/log", ip, porta);
  else snprintf(url, sizeof url, "http://<TV IP>:%d/log", porta);
  ipQuando = time(NULL);
}

// PORTA 8099 PRIMEIRO: e a do painel de registro da etapa de DTS (b1574f7a),
// a que o dono abriu nesta TV. A 8090 de antes nao abria no S95C (a pagina
// ficava carregando). Ocupada: o sistema escolhe uma livre, como o
// "Digitar pelo celular" (celular.c) — o endereco certo aparece no Ajuste.
static int subir(void) {
  struct sockaddr_in a;
  socklen_t al = sizeof a;
  int s, um = 1;
  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) return 0;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &um, sizeof um);
  fcntl(s, F_SETFD, FD_CLOEXEC);
  memset(&a, 0, sizeof a);
  a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_ANY); a.sin_port = htons(LOGREDE_PORTA);
  if (bind(s, (struct sockaddr *)&a, sizeof a) != 0) {
    a.sin_port = 0;
    if (bind(s, (struct sockaddr *)&a, sizeof a) != 0) { close(s); return 0; }
  }
  if (listen(s, 16) != 0 || getsockname(s, (struct sockaddr *)&a, &al) != 0) { close(s); return 0; }
  if (pipe(acorda) != 0) { close(s); acorda[0] = acorda[1] = -1; return 0; }
  ouvir = s; porta = ntohs(a.sin_port);
  if (pthread_create(&fio, NULL, servir, NULL) != 0) {
    close(ouvir); ouvir = -1; close(acorda[0]); close(acorda[1]); acorda[0] = acorda[1] = -1;
    return 0;
  }
  vivo = 1;
  atualizarUrl();
  printf("[logrede] log page on the local network: %s\n", url);
  fflush(stdout);
  return 1;
}

static void descer(void) {
  if (!vivo) return;
  if (write(acorda[1], "x", 1) < 0) { /* o poll acorda pelo timeout */ }
  pthread_join(fio, NULL);
  close(ouvir); ouvir = -1;
  close(acorda[0]); close(acorda[1]); acorda[0] = acorda[1] = -1;
  vivo = 0; url[0] = 0;
  puts("[logrede] log page stopped");
  fflush(stdout);
}

void logrede_configurar(int ligado, const char *arquivo) {
  time_t agora;
  pthread_mutex_lock(&trava);
  snprintf(arqLog, sizeof arqLog, "%s", arquivo ? arquivo : "");
  pthread_mutex_unlock(&trava);
  if (!ligado) { descer(); return; }
  agora = time(NULL);
  if (!vivo) {
    if (agora < proximaTentativa) return;
    if (!subir()) proximaTentativa = agora + 10;
    return;
  }
  if (agora - ipQuando >= 30) atualizarUrl();   // a TV pode trocar de IP (DHCP)
}

const char *logrede_url(void) { return vivo ? url : ""; }

#endif
