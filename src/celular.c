// Ver celular.h para o porque e as regras de seguranca.
#include "celular.h"
#include "posterprov.h"
#include <string.h>

#ifdef __EMSCRIPTEN__
int  celular_disponivel(void) { return 0; }
int  celular_abrir(const char *t) { (void)t; return 0; }
void celular_fechar(void) {}
int  celular_estado(void) { return CEL_PARADO; }
const char *celular_url(void) { return ""; }
int  celular_pegar(char *d, size_t n) { (void)d; (void)n; return 0; }
int  celular_porta(void) { return 0; }
#else

#include "idioma.h"
int ajustes_idioma(void);   // ajustes.h puxa SDL; so o idioma interessa aqui
#include "idiomacod.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0   // Mac: SO_NOSIGPIPE no socket
#endif

#define CEL_TOKEN_N   8
#define CEL_CAB_MAX   8192     // cabecalhos de um pedido
#define CEL_PRAZO_MS  5000     // um pedido inteiro, do accept ao fim do corpo

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static pthread_t fio;
static int fioVivo, ouvir = -1, acorda[2] = { -1, -1 }, porta;
static volatile int estado = CEL_PARADO;
static char token[CEL_TOKEN_N + 1], url[96], hostLan[64], hostLocal[64], origem[96];
static char recebido[CEL_CORPO_MAX + 1];
static int  temRecebido;
static time_t expira;
// Montadas na abertura (thread do app, por causa do i18n); o fio so le.
static char pagina[12288], pagEnviado[8192], pagUsado[8192];

int celular_disponivel(void) { return 1; }
int celular_estado(void) { return estado; }
int celular_porta(void) { return porta; }
const char *celular_url(void) { return estado == CEL_ESPERANDO ? url : ""; }

int celular_pegar(char *dst, size_t n) {
  int r = 0;
  pthread_mutex_lock(&trava);
  if (temRecebido) {
    if (dst && n) snprintf(dst, n, "%s", recebido);
    memset(recebido, 0, sizeof recebido);
    temRecebido = 0;
    r = 1;
  }
  pthread_mutex_unlock(&trava);
  return r;
}

// --- IP da LAN ------------------------------------------------------------------
static int privado(in_addr_t a) {
  unsigned long h = ntohl(a);
  return (h >> 24) == 10 || (h >> 20) == 0xAC1 || (h >> 16) == 0xC0A8;
}

// A ROTA PADRAO DIZ QUAL INTERFACE A TV USA: connect() num socket UDP so
// escolhe a rota, nao manda pacote nenhum (192.0.2.1 e TEST-NET, ninguem
// responde nem recebe). Sem rota padrao (rede so local), a primeira interface
// IPv4 privada que estiver de pe.
static int ipLan(char *dst, size_t n) {
  struct sockaddr_in alvo, eu;
  socklen_t tam = sizeof eu;
  struct ifaddrs *lista = NULL, *i;
  int s = socket(AF_INET, SOCK_DGRAM, 0), ok = 0;
  in_addr_t achou = 0;
  if (s >= 0) {
    memset(&alvo, 0, sizeof alvo);
    alvo.sin_family = AF_INET;
    alvo.sin_port = htons(9);
    inet_pton(AF_INET, "192.0.2.1", &alvo.sin_addr);
    if (!connect(s, (struct sockaddr *)&alvo, sizeof alvo) &&
        !getsockname(s, (struct sockaddr *)&eu, &tam) && privado(eu.sin_addr.s_addr))
      achou = eu.sin_addr.s_addr;
    close(s);
  }
  if (!achou && !getifaddrs(&lista)) {
    for (i = lista; i; i = i->ifa_next) {
      if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET) continue;
      if (!(i->ifa_flags & IFF_UP) || (i->ifa_flags & IFF_LOOPBACK)) continue;
      if (privado(((struct sockaddr_in *)i->ifa_addr)->sin_addr.s_addr)) {
        achou = ((struct sockaddr_in *)i->ifa_addr)->sin_addr.s_addr;
        break;
      }
    }
    freeifaddrs(lista);
  }
  if (achou) {
    struct in_addr a; a.s_addr = achou;
    ok = inet_ntop(AF_INET, &a, dst, (socklen_t)n) != NULL;
  }
  return ok;
}

// --- token ----------------------------------------------------------------------
// 31 simbolos sem os que se confundem (0/o, 1/l/i): quem nao tem camera digita
// o endereco. 8 deles = 39 bits.
static int gerarToken(void) {
  static const char A[] = "abcdefghjkmnpqrstuvwxyz23456789";
  unsigned char b[CEL_TOKEN_N * 2];
  int f = open("/dev/urandom", O_RDONLY), i, k = 0;
  size_t lidos = 0;
  if (f < 0) return 0;
  while (lidos < sizeof b) {
    ssize_t r = read(f, b + lidos, sizeof b - lidos);
    if (r <= 0) { close(f); return 0; }
    lidos += (size_t)r;
  }
  close(f);
  // Rejeicao (248 = 8 x 31) para nao enviesar os primeiros simbolos.
  for (i = 0; i < (int)sizeof b && k < CEL_TOKEN_N; i++)
    if (b[i] < 248) token[k++] = A[b[i] % 31];
  token[k] = 0;
  memset(b, 0, sizeof b);
  return k == CEL_TOKEN_N;
}

// --- paginas --------------------------------------------------------------------
static void escapar(char *dst, size_t n, const char *s) {
  size_t k = 0;
  for (; s && *s && k + 7 < n; s++) {
    const char *e = NULL;
    switch (*s) {
      case '<': e = "&lt;"; break;   case '>': e = "&gt;"; break;
      case '&': e = "&amp;"; break;  case '"': e = "&quot;"; break;
      case '\'': e = "&#39;"; break;
    }
    if (e) { size_t m = strlen(e); memcpy(dst + k, e, m); k += m; }
    else dst[k++] = *s;
  }
  dst[k] = 0;
}

// A PAGINA NO CELULAR, na linguagem 2.0 (ilha): fundo quase preto com UMA luz
// suave na cor de realce da TV num canto, e o conteudo num cartao-ilha (raio
// grande, sombra curta, sem contorno). A cor de realce entra por variaveis CSS
// (--a, --at, --l: ver estiloAcento). Tudo embutido: a pagina vem da TV pela
// rede local e a CSP abaixo (default-src 'none') nao deixa buscar nada, nem
// fonte nem imagem.
#define CEL_ESTILO \
  "<meta name=viewport content='width=device-width,initial-scale=1'>" \
  "<meta name=theme-color content='#0b0c0f'>" \
  "<style>*{box-sizing:border-box}html{background:#0b0c0f}" \
  "body{margin:0;min-height:100vh;font:17px -apple-system,system-ui,sans-serif;color:#fff;" \
  "background:radial-gradient(90%% 55%% at 100%% 0%%,var(--l) 0%%,rgba(11,12,15,0) 70%%),#0b0c0f;" \
  "background-attachment:fixed;overflow-x:hidden}" \
  "main{max-width:540px;margin:24px auto;width:calc(100%% - 32px);padding:30px 24px 26px;border-radius:28px;" \
  "background:rgba(20,21,25,.86);box-shadow:0 10px 28px rgba(0,0,0,.45)}" \
  ".k{font:600 12px system-ui,sans-serif;letter-spacing:.16em;color:rgba(255,255,255,.5);margin:0 0 10px}" \
  "h1{font-size:26px;line-height:1.2;font-weight:600;margin:0 0 12px;color:#fff}" \
  "p{color:rgba(255,255,255,.62);margin:0 0 20px;line-height:1.45}" \
  "textarea{display:block;width:100%%;min-height:9em;font:19px ui-monospace,monospace;padding:16px;border-radius:18px;" \
  "border:0;background:rgba(255,255,255,.06);color:#fff;outline:0}" \
  "textarea:focus{box-shadow:0 0 0 2px var(--a)}" \
  "textarea::placeholder{color:rgba(255,255,255,.4)}" \
  "button{font:600 18px system-ui,sans-serif;padding:16px 22px;border-radius:999px;border:0;margin-top:14px;cursor:pointer}" \
  ".ok{background:var(--a);color:var(--at);width:100%%}" \
  ".ok:active{transform:scale(.98)}" \
  ".sec{background:rgba(255,255,255,.10);color:#fff}.r{display:flex;gap:8px}" \
  ".v{display:flex;flex-direction:column;align-items:center;text-align:center;margin-top:18vh}" \
  ".c{width:84px;height:84px;border-radius:50%%;background:var(--a);color:var(--at);font:700 44px system-ui;" \
  "display:flex;align-items:center;justify-content:center;margin:0 0 22px}" \
  ".v h1{margin:0}</style>"

#define CEL_MARCA "<div class=k>NUVIO LEGACY</div>"

// A cor de realce da TV como variaveis CSS: --a (cor), --at (tinta do rotulo
// sobre ela: preto ou branco pela luminancia) e --l (a luz do canto).
void ajustes_acento(float *r, float *g, float *b);
int  ajustes_tinta_foco(void);
static void estiloAcento(char *dst, size_t n) {
  float r = 0.9f, g = 0.9f, b = 0.9f;
  int R, G, B;
  ajustes_acento(&r, &g, &b);
  R = (int)(r * 255.0f + 0.5f); G = (int)(g * 255.0f + 0.5f); B = (int)(b * 255.0f + 0.5f);
  R = R < 0 ? 0 : R > 255 ? 255 : R; G = G < 0 ? 0 : G > 255 ? 255 : G; B = B < 0 ? 0 : B > 255 ? 255 : B;
  snprintf(dst, n, "<style>:root{--a:#%02x%02x%02x;--at:%s;--l:rgba(%d,%d,%d,.34)}</style>",
           R, G, B, ajustes_tinta_foco() > 128 ? "#fff" : "#111", R, G, B);
}

static void montarPaginas(const char *titulo) {
  char t[256], h1[256], ph[256], colar[96], enviar[128], fim[384], usado[384], cel[128], cv[128];
  int lang = ajustes_idioma();
  estiloAcento(cv, sizeof cv);
  escapar(t, sizeof t, i18n(titulo && *titulo ? titulo : "Digitar pelo celular"));
  escapar(cel, sizeof cel, i18n("Digitar pelo celular"));
  escapar(h1, sizeof h1, i18n("Cole ou digite o texto e envie. Ele aparece no campo da TV."));
  escapar(ph, sizeof ph, i18n("Toque e segure para colar"));
  escapar(colar, sizeof colar, i18n("Colar"));
  escapar(enviar, sizeof enviar, i18n("Enviar para a TV"));
  escapar(fim, sizeof fim, i18n("Enviado. Confira na TV."));
  escapar(usado, sizeof usado, i18n("Este endereço expirou ou já foi usado. Gere outro na TV."));
  // FORMULARIO COMUM, sem fetch: funciona em qualquer navegador de celular, com
  // ou sem JS. O unico JS e o botao Colar, que so aparece onde a area de
  // transferencia existe (no http da LAN o Safari e o Chrome a escondem: la a
  // pessoa toca e segura no campo).
  snprintf(pagina, sizeof pagina,
    "<!doctype html><html lang='%s'><head><meta charset=utf-8><title>%s</title>" CEL_ESTILO "%s"
    "</head><body><main>" CEL_MARCA "<h1>%s</h1><p>%s</p>"
    "<form method=post action='/%s' accept-charset=utf-8>"
    "<textarea name=t id=t maxlength=%d autocapitalize=off autocomplete=off autocorrect=off "
    "spellcheck=false placeholder='%s' autofocus></textarea>"
    "<div class=r><button type=button class=sec id=c hidden>%s</button></div>"
    "<button class=ok>%s</button></form></main>"
    "<script>var c=document.getElementById('c'),t=document.getElementById('t');"
    "if(navigator.clipboard&&navigator.clipboard.readText){c.hidden=false;"
    "c.onclick=function(){navigator.clipboard.readText().then(function(x){t.value=x;}).catch(function(){t.focus();});};}"
    "</script></body></html>",
    idioma_iso(lang), cel, cv, t, h1, token, PP_MODELO_MAX - 1, ph, colar, enviar);
  snprintf(pagEnviado, sizeof pagEnviado,
    "<!doctype html><html lang='%s'><head><meta charset=utf-8><title>%s</title>" CEL_ESTILO "%s"
    "</head><body><main class=v><div class=c>&#10003;</div>" CEL_MARCA
    "<h1>%s</h1></main></body></html>",
    idioma_iso(lang), cel, cv, fim);
  snprintf(pagUsado, sizeof pagUsado,
    "<!doctype html><html lang='%s'><head><meta charset=utf-8><title>%s</title>" CEL_ESTILO "%s"
    "</head><body><main>" CEL_MARCA "<h1>%s</h1></main></body></html>",
    idioma_iso(lang), cel, cv, usado);
}

// --- HTTP -----------------------------------------------------------------------
static long agoraMs(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000L + tv.tv_usec / 1000;
}

// Espera `fd` ficar legivel ate `prazo` (ms absolutos), acordando no pipe de
// parada. 1 = legivel; 0 = prazo, parada ou erro.
static int esperar(int fd, long prazo) {
  for (;;) {
    struct pollfd p[2];
    long falta = prazo - agoraMs();
    int r;
    if (falta <= 0) return 0;
    p[0].fd = fd; p[0].events = POLLIN; p[0].revents = 0;
    p[1].fd = acorda[0]; p[1].events = POLLIN; p[1].revents = 0;
    r = poll(p, 2, (int)(falta > 500 ? 500 : falta));
    if (r < 0 && errno == EINTR) continue;
    if (r < 0 || (p[1].revents & POLLIN)) return 0;
    if (p[0].revents & (POLLIN | POLLHUP | POLLERR)) return 1;
  }
}

static void enviarTudo(int c, const char *b, size_t n) {
  while (n) {
    ssize_t w = send(c, b, n, MSG_NOSIGNAL);
    if (w <= 0) return;
    b += w; n -= (size_t)w;
  }
}

static void responder(int c, int cod, const char *corpo) {
  char cab[640];
  const char *txt = cod == 200 ? "OK" : cod == 403 ? "Forbidden" : cod == 404 ? "Not Found"
                  : cod == 405 ? "Method Not Allowed" : cod == 410 ? "Gone"
                  : cod == 413 ? "Payload Too Large" : "Bad Request";
  size_t n = corpo ? strlen(corpo) : 0;
  int k = snprintf(cab, sizeof cab,
    "HTTP/1.1 %d %s\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: %zu\r\n"
    "Cache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nReferrer-Policy: same-origin\r\n"
    "X-Frame-Options: DENY\r\n"
    "Content-Security-Policy: default-src 'none'; style-src 'unsafe-inline'; "
    "script-src 'unsafe-inline'; form-action 'self'; frame-ancestors 'none'\r\n"
    "%sConnection: close\r\n\r\n", cod, txt, n, cod == 405 ? "Allow: GET, POST\r\n" : "");
  enviarTudo(c, cab, (size_t)k);
  if (n) enviarTudo(c, corpo, n);
}

// Valor do cabecalho `nome` (sem diferenciar caixa) em `cab`, copiado para dst.
static int cabecalho(const char *cab, const char *nome, char *dst, size_t n) {
  size_t m = strlen(nome);
  const char *p = cab;
  while ((p = strstr(p, "\r\n")) != NULL) {
    p += 2;
    if (!strncasecmp(p, nome, m) && p[m] == ':') {
      const char *v = p + m + 1, *f;
      while (*v == ' ' || *v == '\t') v++;
      f = strstr(v, "\r\n");
      if (!f) f = v + strlen(v);
      snprintf(dst, n, "%.*s", (int)(f - v), v);
      return 1;
    }
  }
  return 0;
}

static int hex(int c) {
  return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10
       : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

// Campo `t` de um corpo x-www-form-urlencoded, decodificado no lugar em dst.
static void campoT(const char *corpo, char *dst, size_t n) {
  const char *p = corpo;
  size_t k = 0;
  dst[0] = 0;
  while (p && *p) {
    if (p[0] == 't' && p[1] == '=') {
      p += 2;
      while (*p && *p != '&' && k + 1 < n) {
        if (*p == '+') { dst[k++] = ' '; p++; }
        else if (*p == '%' && hex(p[1]) >= 0 && hex(p[2]) >= 0) {
          dst[k++] = (char)(hex(p[1]) * 16 + hex(p[2])); p += 3;
        } else dst[k++] = *p++;
      }
      break;
    }
    p = strchr(p, '&');
    if (p) p++;
  }
  dst[k] = 0;
}

// Tira espaco/quebra das pontas: o celular cola "chave\n" e o teclado do
// Android completa com espaco.
static void aparar(char *s) {
  size_t i = 0, n = strlen(s);
  while (n && isspace((unsigned char)s[n - 1])) s[--n] = 0;
  while (s[i] && isspace((unsigned char)s[i])) i++;
  if (i) memmove(s, s + i, n - i + 1);
}

// Atende UMA conexao. 1 = recebeu o texto (o servidor pode fechar).
static int atender(int c, int *erros) {
  static char cab[CEL_CAB_MAX + 1], corpo[CEL_CORPO_MAX * 3 + 1];
  char metodo[8], caminho[128], host[96], orig[128], tam[24], tipo[96];
  long prazo = agoraMs() + CEL_PRAZO_MS, cl;
  size_t n = 0, fimCab, temCorpo;
  char *sep;
  int r;
  memset(cab, 0, sizeof cab);
  for (;;) {
    ssize_t k;
    if (n >= CEL_CAB_MAX) { responder(c, 400, NULL); return 0; }
    if (!esperar(c, prazo)) return 0;
    k = recv(c, cab + n, CEL_CAB_MAX - n, 0);
    if (k <= 0) return 0;
    n += (size_t)k; cab[n] = 0;
    if ((sep = strstr(cab, "\r\n\r\n")) != NULL) break;
  }
  fimCab = (size_t)(sep - cab) + 4;
  temCorpo = n - fimCab;
  *sep = 0;   // cab agora e so a linha do pedido + cabecalhos
  if (sscanf(cab, "%7s %127s", metodo, caminho) != 2) { responder(c, 400, NULL); return 0; }

  // HOST E ORIGIN: so o endereco do QR (ou o 127.0.0.1 dos testes). Um site
  // aberto no celular que tentasse falar com a TV por outro nome (rebinding)
  // ou de outra origem para aqui.
  if (!cabecalho(cab, "Host", host, sizeof host) ||
      (strcmp(host, hostLan) && strcmp(host, hostLocal))) {
    puts("[celular] recusado: Host de fora");
    responder(c, 403, pagUsado);
    return 0;
  }
  if (cabecalho(cab, "Origin", orig, sizeof orig)) {
    char o2[96];
    snprintf(o2, sizeof o2, "http://%s", hostLocal);
    // "Referrer-Policy: same-origin" (e nao no-referrer) e o que faz o
    // navegador mandar o Origin de verdade no POST do formulario: com
    // no-referrer o Chrome manda "Origin: null" e o envio era recusado
    // (medido no Chrome, 02/10).
    if (strcmp(orig, origem) && strcmp(orig, o2)) {
      puts("[celular] recusado: Origin de fora");
      responder(c, 403, pagUsado);
      return 0;
    }
  }
  { char *q = strchr(caminho, '?'); if (q) *q = 0; }
  if (caminho[0] != '/' || strcmp(caminho + 1, token)) {
    // O favicon que todo navegador pede nao e tentativa de adivinhar o token.
    if (strcmp(caminho, "/favicon.ico")) (*erros)++;
    responder(c, 404, NULL);
    return 0;
  }
  if (!strcmp(metodo, "GET") || !strcmp(metodo, "HEAD")) {
    // Sem IP de quem pediu: so que a pagina chegou ao celular. E esta linha
    // que diz, no log de uma LG/Samsung, se a rede da TV deixa entrar.
    if (metodo[0] == 'G') puts("[celular] pagina aberta");
    responder(c, 200, metodo[0] == 'G' ? pagina : NULL);
    return 0;
  }
  if (strcmp(metodo, "POST")) { responder(c, 405, NULL); return 0; }

  // POST: tamanho declarado, dentro do teto, e corpo de formulario.
  if (!cabecalho(cab, "Content-Length", tam, sizeof tam)) { responder(c, 400, pagUsado); return 0; }
  cl = strtol(tam, NULL, 10);
  // Urlencoded triplica no pior caso (%XX); o teto e do texto DECODIFICADO.
  if (cl < 0 || cl > CEL_CORPO_MAX * 3) { responder(c, 413, NULL); return 0; }
  if (cabecalho(cab, "Content-Type", tipo, sizeof tipo) &&
      strncasecmp(tipo, "application/x-www-form-urlencoded", 33)) { responder(c, 400, NULL); return 0; }
  if (temCorpo > (size_t)cl) temCorpo = (size_t)cl;
  memcpy(corpo, cab + fimCab, temCorpo);
  while ((long)temCorpo < cl) {
    ssize_t k;
    if (!esperar(c, prazo)) return 0;
    k = recv(c, corpo + temCorpo, (size_t)cl - temCorpo, 0);
    if (k <= 0) return 0;
    temCorpo += (size_t)k;
  }
  corpo[temCorpo] = 0;
  pthread_mutex_lock(&trava);
  campoT(corpo, recebido, sizeof recebido);
  aparar(recebido);
  r = recebido[0] != 0;
  temRecebido = r;
  if (r) estado = CEL_RECEBIDO;
  // NUNCA o texto: so o tamanho (campos de chave).
  printf("[celular] recebido %zu bytes\n", strlen(recebido));
  pthread_mutex_unlock(&trava);
  memset(corpo, 0, temCorpo);
  responder(c, 200, r ? pagEnviado : pagina);
  return r;
}

static void *servir(void *arg) {
  int erros = 0;
  (void)arg;
  while (estado == CEL_ESPERANDO) {
    struct pollfd p[2];
    int r;
    if (time(NULL) >= expira) { estado = CEL_EXPIROU; puts("[celular] expirou"); break; }
    p[0].fd = ouvir; p[0].events = POLLIN; p[0].revents = 0;
    p[1].fd = acorda[0]; p[1].events = POLLIN; p[1].revents = 0;
    r = poll(p, 2, 500);
    if (r < 0 && errno == EINTR) continue;
    if (r < 0 || (p[1].revents & POLLIN)) break;
    if (p[0].revents & POLLIN) {
      int c = accept(ouvir, NULL, NULL);
      if (c < 0) continue;
#ifdef SO_NOSIGPIPE
      { int um = 1; setsockopt(c, SOL_SOCKET, SO_NOSIGPIPE, &um, sizeof um); }
#endif
      if (atender(c, &erros)) { close(c); break; }
      close(c);
      if (erros >= CEL_ERROS_MAX) {
        estado = CEL_FALHOU;
        puts("[celular] caminhos errados demais: servidor fechado");
        break;
      }
    }
  }
  return NULL;
}

void celular_fechar(void) {
  if (fioVivo) {
    if (acorda[1] >= 0) { char b = 1; ssize_t w = write(acorda[1], &b, 1); (void)w; }
    pthread_join(fio, NULL);
    fioVivo = 0;
  }
  if (ouvir >= 0) { close(ouvir); ouvir = -1; }
  if (acorda[0] >= 0) { close(acorda[0]); close(acorda[1]); acorda[0] = acorda[1] = -1; }
  pthread_mutex_lock(&trava);
  memset(recebido, 0, sizeof recebido);
  temRecebido = 0;
  pthread_mutex_unlock(&trava);
  memset(token, 0, sizeof token);
  url[0] = 0;
  porta = 0;
  estado = CEL_PARADO;
}

int celular_abrir(const char *titulo) {
  struct sockaddr_in a;
  socklen_t tam = sizeof a;
  char ip[48];
  int um = 1;
  celular_fechar();
  if (!ipLan(ip, sizeof ip)) { puts("[celular] sem IP de rede local"); return 0; }
  if (!gerarToken() || pipe(acorda)) { acorda[0] = acorda[1] = -1; return 0; }
  ouvir = socket(AF_INET, SOCK_STREAM, 0);
  if (ouvir < 0) { celular_fechar(); return 0; }
  setsockopt(ouvir, SOL_SOCKET, SO_REUSEADDR, &um, sizeof um);
  memset(&a, 0, sizeof a);
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_ANY);   // a LAN e o 127.0.0.1 dos testes
  a.sin_port = 0;                          // porta livre, do sistema
  if (bind(ouvir, (struct sockaddr *)&a, sizeof a) || listen(ouvir, 4) ||
      getsockname(ouvir, (struct sockaddr *)&a, &tam)) {
    printf("[celular] servidor nao subiu (errno %d)\n", errno);
    celular_fechar();
    return 0;
  }
  porta = ntohs(a.sin_port);
  snprintf(hostLan, sizeof hostLan, "%s:%d", ip, porta);
  snprintf(hostLocal, sizeof hostLocal, "127.0.0.1:%d", porta);
  snprintf(origem, sizeof origem, "http://%s", hostLan);
  snprintf(url, sizeof url, "http://%s/%s", hostLan, token);
  montarPaginas(titulo);
  expira = time(NULL) + CEL_VALIDADE_S;
  estado = CEL_ESPERANDO;
  if (pthread_create(&fio, NULL, servir, NULL)) { celular_fechar(); return 0; }
  fioVivo = 1;
  // Sem IP nem token no log: so que subiu.
  printf("[celular] servidor no ar (porta %d)\n", porta);
  return 1;
}
#endif
