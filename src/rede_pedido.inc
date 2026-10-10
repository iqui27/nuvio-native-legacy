/* N01: included only by the native rede.c, after libcurl initialization.
 * A request owns its easy handle and callback context. No compiler TLS and
 * no implicit retries; legacy workers keep their existing connection cache.
 * CURLOPT_NOSIGNAL prevents process-wide alarms. A libcurl built with a
 * blocking DNS resolver can still block inside DNS past the deadline; the
 * application cannot interrupt that resolver. Never advertise synchronous
 * cancellation or a hard wall-clock DNS bound on those builds.
 */
typedef struct {
  const RedePedido *p;
  RedeResposta *r;
  uint64_t inicio, prazo, primeiro, janela, baseFio, fio;
  size_t corpoCap, cabCap, vistosCab;
  size_t tetoCorpo, tetoCab;
  int head, status;
  uint64_t descartado;
} PedidoCtx;

static unsigned pedidoMs(uint64_t v) { return v > UINT_MAX ? UINT_MAX : (unsigned)v; }
static int pedidoParar(PedidoCtx *c) {
  RedeErro e = rede_job_estado(c->p->job);
  if (e == REDE_OK && c->p->parar && c->p->parar(c->p->parar_usuario)) e = REDE_CANCELADO;
  if (e == REDE_OK && pedidoAgoraMs() >= c->prazo) e = REDE_PRAZO;
  if (e != REDE_OK && c->r->erro == REDE_OK) c->r->erro = e;
  return c->r->erro != REDE_OK;
}
static int pedidoGuardar(char **s, size_t *n, size_t *cap, size_t teto,
                          const void *b, size_t len) {
  size_t precisa, novoCap;
  char *novo;
  if (*n > teto || len > teto - *n) return 0;
  precisa = *n + len + 1;
  if (precisa > *cap) {
    novoCap = *cap ? *cap : 4096;
    while (novoCap < precisa) novoCap *= 2;
    if (novoCap > teto + 1) novoCap = teto + 1;
    novo = realloc(*s, novoCap);
    if (!novo) return -1;
    *s = novo; *cap = novoCap;
  }
  if (len) memcpy(*s + *n, b, len);
  *n += len; (*s)[*n] = 0;
  return 1;
}
static void pedidoValor(char *dst, size_t cap, const char *s, size_t n) {
  while (n && (*s == ' ' || *s == '\t')) { s++; n--; }
  while (n && isspace((unsigned char)s[n - 1])) n--;
  if (n >= cap) n = cap - 1;
  memcpy(dst, s, n); dst[n] = 0;
}
static size_t pedidoCab(void *b, size_t size, size_t count, void *u) {
  PedidoCtx *c = u;
  const char *s = b;
  size_t n;
  int ok;
  if (size && count > SIZE_MAX / size) { c->r->erro = REDE_LIMITE_CABECALHOS; return 0; }
  n = size * count;
  if (pedidoParar(c)) return 0;
  /* Total across informational headers and redirect hops, before realloc. */
  if (n > c->tetoCab - c->vistosCab) { c->r->erro = REDE_LIMITE_CABECALHOS; return 0; }
  c->vistosCab += n;
  if (n >= 5 && !strncasecmp(s, "HTTP/", 5)) {
    char linha[80];
    c->r->n_cabecalhos = 0;
    c->r->mime[0] = 0; c->r->retry_after_s = 0;
    pedidoValor(linha, sizeof linha, s, n);
    { const char *p = strchr(linha, ' '); c->status = p ? atoi(p + 1) : 0; }
  }
  if (n > 13 && !strncasecmp(s, "Content-Type:", 13)) {
    char *fim;
    pedidoValor(c->r->mime, sizeof c->r->mime, s + 13, n - 13);
    fim = strchr(c->r->mime, ';'); if (fim) *fim = 0;
  } else if (n > 12 && !strncasecmp(s, "Retry-After:", 12)) {
    c->r->retry_after_s = retrySegundos(s + 12, n - 12);
  } else if (!c->head && !c->p->janela_corpo_ms && n > 15 && !strncasecmp(s, "Content-Length:", 15)) {
    size_t i = 15, valor = 0;
    while (i < n && (s[i] == ' ' || s[i] == '\t')) i++;
    while (i < n && isdigit((unsigned char)s[i])) {
      unsigned dig = (unsigned)(s[i++] - '0');
      if (valor > c->tetoCorpo / 10 || valor * 10 + dig > c->tetoCorpo) {
        c->r->erro = REDE_LIMITE_CORPO; return 0;
      }
      valor = valor * 10 + dig;
    }
  }
  ok = pedidoGuardar(&c->r->cabecalhos, &c->r->n_cabecalhos, &c->cabCap,
                     c->tetoCab, b, n);
  if (ok != 1) { c->r->erro = ok ? REDE_MEMORIA : REDE_LIMITE_CABECALHOS; return 0; }
  return n;
}
static size_t pedidoCorpo(void *b, size_t size, size_t count, void *u) {
  PedidoCtx *c = u;
  size_t n;
  int ok;
  if (size && count > SIZE_MAX / size) { c->r->erro = REDE_LIMITE_CORPO; return 0; }
  n = size * count;
  if (pedidoParar(c)) return 0;
  if (c->p->janela_corpo_ms && !(c->p->seguir &&
      (c->status == 301 || c->status == 302 || c->status == 303 || c->status == 307 || c->status == 308))) {
    uint64_t agora = pedidoAgoraMs();
    size_t prefixo;
    if (c->primeiro && agora - c->primeiro >= c->p->janela_corpo_ms) {
      c->r->fim_janela = 1; return 0;
    }
    if (n > c->p->max_descartado - c->descartado) { c->r->fim_teto = 1; return 0; }
    if (n && !c->primeiro) {
      c->primeiro = c->janela = agora;
      c->r->primeiro_byte_ms = pedidoMs(agora - c->inicio);
    }
    prefixo = sizeof c->r->prefixo - c->r->n_prefixo;
    if (prefixo > n) prefixo = n;
    memcpy(c->r->prefixo + c->r->n_prefixo, b, prefixo);
    c->r->n_prefixo += (unsigned)prefixo;
    c->descartado += n;
    return n;
  }
  /* Cap applies to decoded bytes, including gzip expansion, before malloc. */
  if (n > c->tetoCorpo - c->r->n_corpo) { c->r->erro = REDE_LIMITE_CORPO; return 0; }
  if (n && !c->primeiro) {
    c->primeiro = c->janela = pedidoAgoraMs();
    c->r->primeiro_byte_ms = pedidoMs(c->primeiro - c->inicio);
  }
  ok = pedidoGuardar(&c->r->corpo, &c->r->n_corpo, &c->corpoCap, c->tetoCorpo, b, n);
  if (ok != 1) { c->r->erro = ok ? REDE_MEMORIA : REDE_LIMITE_CORPO; return 0; }
  return n;
}
static int pedidoEhRedirect(int st) {
  return st == 301 || st == 302 || st == 303 || st == 307 || st == 308;
}
static void pedidoIntervalo(PedidoCtx *c, uint64_t agora, int final) {
  RedeIntervalo v;
  uint64_t tempo;
  if (!c->primeiro || (c->p->seguir && pedidoEhRedirect(c->status))) return;
  tempo = agora - c->janela;
  if (!tempo || (!final && tempo < 1000)) return;
  v.bytes = c->fio >= c->baseFio ? c->fio - c->baseFio : 0;
  v.ms = pedidoMs(tempo); v.inicio_ms = pedidoMs(c->janela - c->primeiro);
  v.completa = tempo >= 1000;
  if (v.completa) c->r->intervalos_completos++;
  if (c->p->intervalo) c->p->intervalo(&v, c->p->intervalo_usuario);
  c->janela = agora; c->baseFio = c->fio;
}
static int pedidoProgresso(void *u, long long total, long long agora,
                            long long upTotal, long long upAgora) {
  PedidoCtx *c = u;
  (void)total; (void)upTotal; (void)upAgora;
  if (pedidoParar(c)) return 1;
  if (agora >= 0 && (uint64_t)agora > c->fio) c->fio = (uint64_t)agora;
  pedidoIntervalo(c, pedidoAgoraMs(), 0);
  if (c->p->janela_corpo_ms && c->primeiro &&
      !(c->p->seguir && pedidoEhRedirect(c->status)) &&
      pedidoAgoraMs() - c->primeiro >= c->p->janela_corpo_ms) {
    c->r->fim_janela = 1; return 1;
  }
  return pedidoParar(c);
}

/* Normalize scheme/host/default port without copying path/query secrets.
 * Accept explicit IPv6 and DNS ASCII/punycode. Userinfo/control characters,
 * ambiguous authorities and non-HTTP protocols are rejected before I/O. */
static int pedidoOrigem(const char *u, char *dst, size_t cap, int *tls) {
  const char *p, *fim, *host, *hostFim, *porta = NULL;
  size_t n, i;
  unsigned port;
  if (!u || (n = strnlen(u, 4096)) >= 4096) return 0;
  for (i = 0; i < n; i++) if ((unsigned char)u[i] <= 32 || u[i] == '\\' || (unsigned char)u[i] == 127) return 0;
  if (!strncasecmp(u, "https://", 8)) { *tls = 1; p = u + 8; port = 443; }
  else if (!strncasecmp(u, "http://", 7)) { *tls = 0; p = u + 7; port = 80; }
  else return 0;
  fim = p + strcspn(p, "/?#"); host = p;
  if (p == fim || memchr(p, '@', (size_t)(fim - p))) return 0;
  if (*p == '[') {
    hostFim = memchr(p, ']', (size_t)(fim - p));
    if (!hostFim) return 0;
    hostFim++;
    if (hostFim != fim) { if (*hostFim != ':') return 0; porta = hostFim + 1; }
  } else {
    hostFim = memchr(p, ':', (size_t)(fim - p));
    if (hostFim) porta = hostFim + 1; else hostFim = fim;
    for (p = host; p < hostFim; p++)
      if (!isalnum((unsigned char)*p) && *p != '.' && *p != '-' && *p != '_') return 0;
  }
  if (host == hostFim) return 0;
  if (porta) {
    if (porta == fim) return 0;
    port = 0;
    for (p = porta; p < fim; p++) {
      if (!isdigit((unsigned char)*p) || port > 6553) return 0;
      port = port * 10 + (unsigned)(*p - '0');
    }
    if (!port || port > 65535) return 0;
  }
  n = (size_t)(hostFim - host);
  if (n + 16 >= cap) return 0;
  memcpy(dst, *tls ? "https://" : "http://", *tls ? 8 : 7);
  i = *tls ? 8 : 7;
  for (p = host; p < hostFim; p++) dst[i++] = (char)tolower((unsigned char)*p);
  snprintf(dst + i, cap - i, ":%u", port);
  return 1;
}
static int pedidoMetodo(const char *m) {
  return !strcmp(m, "GET") || !strcmp(m, "HEAD") || !strcmp(m, "POST") ||
         !strcmp(m, "PUT") || !strcmp(m, "PATCH") || !strcmp(m, "DELETE");
}
static int pedidoCabsValidos(const char *const *cab, size_t teto) {
  size_t vistos = 0;
  unsigned k;
  for (k = 0; cab && cab[k] && k < 64; k++) {
    const char *s = cab[k], *fim;
    size_t n = strnlen(s, teto + 1), i;
    if (n > teto) return 0;
    fim = memchr(s, ':', n);
    if (!fim || fim == s || n + 2 > teto - vistos) return 0;
    for (i = 0; i < (size_t)(fim - s); i++)
      if (!isalnum((unsigned char)s[i]) && !strchr("!#$%&'*+-.^_`|~", s[i])) return 0;
    for (i = (size_t)(fim - s) + 1; i < n; i++)
      if (((unsigned char)s[i] < 32 && s[i] != '\t') || (unsigned char)s[i] == 127) return 0;
    /* Avoid implicit proxy/origin routing and inconsistent body framing. */
    if ((fim - s == 4 && !strncasecmp(s, "Host", 4)) ||
        (fim - s == 14 && !strncasecmp(s, "Content-Length", 14)) ||
        (fim - s == 17 && !strncasecmp(s, "Transfer-Encoding", 17))) return 0;
    vistos += n + 2;
  }
  return !cab || !cab[k];
}
unsigned rede_pedido_capacidades(void) {
  return REDE_CAP_CORPO | REDE_CAP_JOB | REDE_CAP_REDIRECT | REDE_CAP_INTERVALO;
}
int rede_pedir(const RedePedido *p, RedeResposta *r) {
  PedidoCtx c;
  void *curl = NULL, *lista = NULL;
  const char *metodo;
  const void *corpo;
  size_t nCorpo;
  char url[4096], origem[300], origemPessoal[300];
  int tls, headersPermitidos = 1, hop, ok = 0;
  if (!r) return 0;
  memset(r, 0, sizeof *r); memset(&c, 0, sizeof c);
  c.p = p; c.r = r;
  if (!p) { r->erro = REDE_ENTRADA; return 0; }
  metodo = p->metodo ? p->metodo : "GET";
  c.head = !strcmp(metodo, "HEAD");
  c.tetoCorpo = p->max_bytes ? p->max_bytes : REDE_CORPO_PADRAO;
  c.tetoCab = p->max_cabecalhos ? p->max_cabecalhos : REDE_CAB_PADRAO;
  if (!pedidoMetodo(metodo) || !pedidoOrigem(p->url, origem, sizeof origem, &tls) ||
      c.tetoCorpo > REDE_CORPO_MAXIMO || c.tetoCab > REDE_CAB_MAXIMO ||
      p->prazo_ms > 300000 || (p->n_corpo && !p->corpo) ||
      p->n_corpo > REDE_CORPO_PADRAO || ((c.head || !strcmp(metodo, "GET")) && p->n_corpo) ||
      p->janela_corpo_ms > 60000 || (p->janela_corpo_ms &&
        (strcmp(metodo, "GET") || !p->max_descartado || p->max_descartado > UINT64_C(2147483648))) ||
      !pedidoCabsValidos(p->cabecalhos, c.tetoCab)) {
    r->erro = REDE_ENTRADA; return 0;
  }
  /* `origem` follows redirects; the personal-server origin is the first one. */
  memcpy(origemPessoal, origem, sizeof origemPessoal);
  rede_job_reter(p->job);
  c.inicio = pedidoAgoraMs();
  c.prazo = c.inicio + (p->prazo_ms ? p->prazo_ms : 15000);
  if (pedidoParar(&c)) goto fim;
  if (!abrir() || !curl_cleanup || !curl_getinfo || !slist_append || !slist_free) {
    r->erro = REDE_INDISPONIVEL; goto fim;
  }
  curl = curl_init();
  if (!curl) { r->erro = REDE_MEMORIA; goto fim; }
  strcpy(url, p->url); corpo = p->corpo; nCorpo = p->n_corpo;
  for (hop = 0; ; hop++) {
    long status = 0;
    uint64_t agora;
    double fio = 0;
    char *destino = NULL;
    unsigned k;
    if (pedidoParar(&c)) break;
    if (lista) { slist_free(lista); lista = NULL; }
    for (k = 0; p->cabecalhos && p->cabecalhos[k]; k++) {
      void *nova;
      /* #203: Range is not a credential and must reach the CDN a debrid link
       * redirects to; without it every speed-test connection read byte 0 (200)
       * and the extra Android connections were never summed. */
      if (!headersPermitidos && strncasecmp(p->cabecalhos[k], "Range:", 6)) continue;
      nova = slist_append(lista, p->cabecalhos[k]);
      if (!nova) { r->erro = REDE_MEMORIA; break; }
      lista = nova;
    }
    if (r->erro) break;
    agora = pedidoAgoraMs();
    if (pedidoParar(&c)) break;
    /* Required options fail closed, including verification on old SDKs. */
#define PEDIDO_OPT(op, val) do { if (curl_setopt(curl, (op), (val))) { r->erro = REDE_INDISPONIVEL; goto fim; } } while (0)
    PEDIDO_OPT(OPT_URL, url);
    PEDIDO_OPT(OPT_FOLLOWLOCATION, 0L);
    PEDIDO_OPT(OPT_PROTOCOLS, 3L); /* HTTP | HTTPS; never file/ftp/etc */
    PEDIDO_OPT(OPT_TIMEOUT_MS, (long)(c.prazo - agora));
    PEDIDO_OPT(OPT_CONNECTTIMEOUT_MS, (long)(c.prazo - agora));
    PEDIDO_OPT(OPT_NOSIGNAL, 1L);
    { /* #290: servidor pessoal so na propria origem (ver tls_pessoal em rede.h). */
      char aqui[300];
      int tlsAqui = 0, pessoal = p->tls_pessoal &&
        pedidoOrigem(url, aqui, sizeof aqui, &tlsAqui) && tlsAqui && !strcmp(aqui, origemPessoal);
      PEDIDO_OPT(OPT_SSL_VERIFYPEER, pessoal ? 0L : 1L);
      PEDIDO_OPT(OPT_SSL_VERIFYHOST, pessoal ? 0L : 2L);
    }
    if (p->ca_arquivo || discordCa[0]) PEDIDO_OPT(OPT_CAINFO, p->ca_arquivo ? p->ca_arquivo : discordCa);
    PEDIDO_OPT(OPT_USERAGENT, "Nuvio/1.8");
    /* Media diagnostics measure the player's wire body, without transparent
     * gzip inflation. Buffered requests retain negotiated decompression. */
    PEDIDO_OPT(OPT_ACCEPT_ENCODING, p->janela_corpo_ms ? (void *)NULL : "");
    PEDIDO_OPT(OPT_HTTPHEADER, lista);
    PEDIDO_OPT(OPT_NOBODY, c.head ? 1L : 0L);
    PEDIDO_OPT(OPT_CUSTOMREQUEST, metodo);
    if (corpo || nCorpo || (strcmp(metodo, "GET") && !c.head)) {
      PEDIDO_OPT(OPT_POSTFIELDSIZE, (long)nCorpo);
      PEDIDO_OPT(OPT_POSTFIELDS, corpo ? corpo : "");
    } else {
      /* POSTFIELDS=NULL enables POST; explicitly restore GET after a 303. */
      PEDIDO_OPT(OPT_POSTFIELDS, (void *)NULL);
      PEDIDO_OPT(80 /* CURLOPT_HTTPGET */, c.head ? 0L : 1L);
      PEDIDO_OPT(OPT_NOBODY, c.head ? 1L : 0L);
    }
    PEDIDO_OPT(OPT_WRITEFUNCTION, pedidoCorpo);
    PEDIDO_OPT(OPT_WRITEDATA, &c);
    PEDIDO_OPT(OPT_HEADERFUNCTION, pedidoCab);
    PEDIDO_OPT(OPT_HEADERDATA, &c);
    PEDIDO_OPT(OPT_XFERINFOFUNCTION, pedidoProgresso);
    PEDIDO_OPT(OPT_XFERINFODATA, &c);
    PEDIDO_OPT(OPT_NOPROGRESS, 0L);
#undef PEDIDO_OPT
    strcpy(r->final, url);
    rede_url_publica(url, r->host, sizeof r->host);
    r->curl_erro = performTmdb(curl);
    { char *final = NULL;
      if (!curl_getinfo(curl, INFO_URL_FINAL, &final) && final)
        snprintf(r->final, sizeof r->final, "%s", final);
    }
    curl_getinfo(curl, INFO_RESPONSE_CODE, &status); r->status = (int)status;
    if (!curl_getinfo(curl, INFO_SIZE_DOWNLOAD, &fio) && fio >= 0 && fio < 9007199254740992.0)
      c.fio = (uint64_t)fio;
    r->bytes_fio = c.fio;
    agora = pedidoAgoraMs();
    if (c.primeiro) r->corpo_ms = pedidoMs(agora - c.primeiro);
    if (pedidoParar(&c)) break;
    if (r->curl_erro && !(p->janela_corpo_ms && (r->fim_janela || r->fim_teto) &&
        (r->curl_erro == 23 || r->curl_erro == 42 || r->curl_erro == 56))) {
      /* 56: an h2 stream cut by our own window/cap abort reports recv failure. */
      r->erro = r->curl_erro == 28 ? REDE_PRAZO : REDE_TRANSPORTE; break;
    }
    if (!p->seguir || !pedidoEhRedirect(r->status)) {
      pedidoIntervalo(&c, agora, 1);
      if (pedidoParar(&c)) break;
      /* Empty 204/HEAD still owns a terminated buffer like an ordinary reply. */
      if (!r->corpo && !p->janela_corpo_ms) {
        r->corpo = calloc(1, 1);
        if (!r->corpo) { r->erro = REDE_MEMORIA; break; }
      }
      ok = 1; break;
    }
    if (hop >= 5 || curl_getinfo(curl, INFO_REDIRECT_URL, &destino) || !destino) {
      r->erro = REDE_REDIRECT; break;
    }
    {
      char novaOrigem[300]; int novoTls;
      if (!pedidoOrigem(destino, novaOrigem, sizeof novaOrigem, &novoTls) || (tls && !novoTls)) {
        r->erro = REDE_REDIRECT; break;
      }
      /* 303 converts to GET (except HEAD); 301/302 only convert POST. */
      if ((r->status == 303 && !c.head) || ((r->status == 301 || r->status == 302) && !strcmp(metodo, "POST"))) {
        metodo = "GET"; corpo = NULL; nCorpo = 0;
      }
      if (strcmp(origem, novaOrigem)) {
        if (strcmp(metodo, "GET") && strcmp(metodo, "HEAD")) { r->erro = REDE_REDIRECT; break; }
        /* All caller headers, including unknown API-key names, are dropped.
         * Do not restore them if a later hop returns to the original host. */
        headersPermitidos = 0;
      }
      strcpy(url, destino); strcpy(origem, novaOrigem); tls = novoTls;
    }
    free(r->corpo); r->corpo = NULL; r->n_corpo = c.corpoCap = 0;
    r->n_cabecalhos = 0; r->mime[0] = 0; r->retry_after_s = 0;
    r->bytes_fio = r->corpo_ms = r->primeiro_byte_ms = r->intervalos_completos = 0;
    c.fio = c.baseFio = c.primeiro = c.janela = 0;
    c.descartado = 0; r->n_prefixo = 0; r->fim_janela = r->fim_teto = 0;
  }
fim:
  r->ms = pedidoMs(pedidoAgoraMs() - c.inicio);
  if (!ok) { free(r->corpo); r->corpo = NULL; r->n_corpo = 0; }
  if (lista) slist_free(lista);
  if (curl) curl_cleanup(curl);
  /* Cleanup may block briefly; fence generation again at the handoff. */
  if (ok) {
    RedeErro e = rede_job_estado(p->job);
    if (e != REDE_OK) { r->erro = e; ok = 0; free(r->corpo); r->corpo = NULL; r->n_corpo = 0; }
  }
  rede_job_soltar(p->job);
  /* Consumers choose retry/status policy. No credentials, headers or URL
   * path are logged, and cancellation/staleness is not a network outage. */
  return ok;
}
