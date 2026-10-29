// P2P experimental via servidor de streaming do Stremio. Ver p2p.h.
#include "p2p.h"
#include "p2pmotor.h"
#include "ajustes.h"
#include "debrid.h"
#include "js.h"
#include "rede.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <stdatomic.h>

// Escrito pelo fio do torrent, lido pelo fio da tela: atomico (TSan).
static _Atomic int ultimoErro;
int p2p_ultimo_erro(void) { return atomic_load(&ultimoErro); }

int p2p_ativo(void) {
  const char *u = ajustes_p2p_url();
  return ajustes_p2p_ligado() && ((u && u[0]) || p2pmotor_disponivel());
}

int p2p_usa_motor(void) {
  const char *u = ajustes_p2p_url();
  return ajustes_p2p_ligado() && !(u && u[0]) && p2pmotor_disponivel();
}

int p2p_hash_valido(const char *h) {
  int i;
  if (!h) return 0;
  for (i = 0; i < 40; i++)
    if (!isxdigit((unsigned char)h[i])) return 0;
  return h[40] == 0;
}

// ---------------------------------------------------------------- endereco

int p2p_normalizar_url(const char *entrada, char *out, unsigned n) {
  char t[200], host[160], porta[8] = "";
  const char *esq = "http", *p, *fim;
  unsigned k = 0, l;
  if (!entrada || !out || n < 24) return 0;
  while (*entrada == ' ' || *entrada == '\t') entrada++;
  l = (unsigned)strlen(entrada);
  while (l && (entrada[l - 1] == ' ' || entrada[l - 1] == '\t' ||
               entrada[l - 1] == '\n' || entrada[l - 1] == '\r')) l--;
  if (!l || l >= sizeof t) return 0;
  memcpy(t, entrada, l); t[l] = 0;
  p = t;
  if (!strncasecmp(p, "http://", 7)) p += 7;
  else if (!strncasecmp(p, "https://", 8)) { esq = "https"; p += 8; }
  else if (strstr(p, "://")) return 0;                 // ftp://, magnet://...
  // Caminho, consulta e ancora nao servem: a base e so esquema+host+porta.
  fim = p + strcspn(p, "/?#");
  if (fim == p) return 0;
  if (*p == '[') {                                      // IPv6 literal
    const char *c = memchr(p, ']', (size_t)(fim - p));
    if (!c) return 0;
    for (const char *q = p + 1; q < c; q++)
      if (!isxdigit((unsigned char)*q) && *q != ':' && *q != '.') return 0;
    if (c - p + 1 >= (long)sizeof host) return 0;
    memcpy(host, p, (size_t)(c - p + 1)); host[c - p + 1] = 0;
    if (c + 1 < fim) {
      if (c[1] != ':') return 0;
      if (fim - (c + 2) >= (long)sizeof porta || fim == c + 2) return 0;
      memcpy(porta, c + 2, (size_t)(fim - (c + 2))); porta[fim - (c + 2)] = 0;
    }
  } else {
    const char *dp = memchr(p, ':', (size_t)(fim - p));
    const char *fh = dp ? dp : fim;
    if (fh == p || fh - p >= (long)sizeof host) return 0;
    for (const char *q = p; q < fh; q++)
      if (!isalnum((unsigned char)*q) && *q != '.' && *q != '-') return 0;
    memcpy(host, p, (size_t)(fh - p)); host[fh - p] = 0;
    if (dp) {
      if (fim - (dp + 1) >= (long)sizeof porta || fim == dp + 1) return 0;
      memcpy(porta, dp + 1, (size_t)(fim - (dp + 1))); porta[fim - (dp + 1)] = 0;
    }
  }
  if (porta[0]) {
    long v;
    for (k = 0; porta[k]; k++) if (!isdigit((unsigned char)porta[k])) return 0;
    v = atol(porta);
    if (v < 1 || v > 65535) return 0;
  }
  // 12470 e a porta HTTPS do stremio/server (certificado proprio); 11470 e a HTTP.
  if (snprintf(out, n, "%s://%s:%s", esq, host,
               porta[0] ? porta : (esq[4] == 's' ? "12470" : "11470")) >= (int)n)
    return 0;
  return 1;
}

// ---------------------------------------------------------------- corpo

int p2p_corpo_criar(const char *hash, const char *fontes, char *out, unsigned n) {
  char h[41];
  unsigned u = 0;
  int i, e;
  if (!p2p_hash_valido(hash) || !out || n < 128) return 0;
  for (i = 0; i < 40; i++) h[i] = (char)tolower((unsigned char)hash[i]);
  h[40] = 0;
  e = snprintf(out, n, "{\"torrent\":{\"infoHash\":\"%s\",\"peerSearch\":{\"sources\":[", h);
  if (e < 0 || (unsigned)e >= n) return 0;
  u = (unsigned)e;
  // Uma entrada por linha. Cada uma so entra se for texto seguro de pôr entre
  // aspas sem escape (o addon e terceiro: aspa, barra invertida ou controle
  // quebrariam o JSON, ou pior, o fechariam para injetar campos).
  for (const char *p = fontes; p && *p;) {
    const char *nl = strchr(p, '\n');
    size_t L = nl ? (size_t)(nl - p) : strlen(p);
    int ok = L > 4 && L < 300 && (!strncmp(p, "tracker:", 8) || !strncmp(p, "dht:", 4));
    for (size_t q = 0; ok && q < L; q++)
      if ((unsigned char)p[q] < 32 || p[q] == '"' || p[q] == '\\' || p[q] == 127) ok = 0;
    if (ok) {
      e = snprintf(out + u, n - u, "\"%.*s\",", (int)L, p);
      if (e < 0 || (unsigned)e >= n - u) break;   // sem espaco: o resto fica de fora
      u += (unsigned)e;
    }
    p = nl ? nl + 1 : p + L;
  }
  e = snprintf(out + u, n - u, "\"dht:%s\"],\"min\":40,\"max\":150}}}", h);
  if (e < 0 || (unsigned)e >= n - u) return 0;
  return (int)(u + (unsigned)e);
}

// ---------------------------------------------------------------- arquivo

static int ehVideo(const char *nome) {
  static const char *ext[] = { ".mp4", ".mkv", ".webm", ".avi", ".mov", ".m4v",
                               ".ts", ".m2ts", ".wmv", NULL };
  size_t L = strlen(nome); int i;
  for (i = 0; ext[i]; i++) {
    size_t e = strlen(ext[i]);
    if (L > e && !strcasecmp(nome + L - e, ext[i])) return 1;
  }
  return 0;
}

int p2p_escolher_lista(const char *const *nomes, const double *tams, int n,
                       int fileIdx, int temporada, int episodio) {
  char pad1[16] = "", pad2[16] = "";
  int idx, melhor = -1, porPadrao = -1, doAddon = -1;
  double melhorTam = -1;
  if (!nomes || !tams || n <= 0) return -1;
  if (temporada >= 0 && episodio > 0) {
    snprintf(pad1, sizeof pad1, "s%02de%02d", temporada, episodio);
    snprintf(pad2, sizeof pad2, "%dx%02d", temporada, episodio);
  }
  for (idx = 0; idx < n; idx++) {
    char nome[600];
    if (!nomes[idx] || !nomes[idx][0]) continue;
    snprintf(nome, sizeof nome, "%s", nomes[idx]);
    for (char *c = nome; *c; c++) *c = (char)tolower((unsigned char)*c);
    if (!ehVideo(nome)) continue;
    if (idx == fileIdx) doAddon = idx;
    if (porPadrao < 0 && pad1[0] && (strstr(nome, pad1) || strstr(nome, pad2))) porPadrao = idx;
    if (tams[idx] > melhorTam) { melhor = idx; melhorTam = tams[idx]; }
  }
  // O indice do addon vem primeiro: e o arquivo que ELE mediu (Torrentio manda
  // o fileIdx exato do episodio dentro do pacote de temporada). O padrao
  // SxxEyy so entra quando o addon nao disse, ou disse um que nao e video.
  if (doAddon >= 0) return doAddon;
  if (porPadrao >= 0) return porPadrao;
  if (temporada == 0 && episodio > 0) return -1; // especial nao e o maior episodio regular
  return melhor;
}

#define P2P_MAX_ARQ 4096
int p2p_escolher_arquivo(const char *json, int fileIdx, int temporada, int episodio) {
  const char *p;
  int n = 0, r;
  if (!json) return -1;
  // O stats.json do Stremio manda "path" e "name"; "path" cobre o nome com
  // pasta ("Temporada 1/Serie.S01E02.mkv"). Ate P2P_MAX_ARQ arquivos.
  {
    char (*buf)[600] = malloc(sizeof(char[600]) * P2P_MAX_ARQ);
    const char **pp = malloc(sizeof *pp * P2P_MAX_ARQ);
    double *tt = malloc(sizeof *tt * P2P_MAX_ARQ);
    if (!buf || !pp || !tt) { free(buf); free(pp); free(tt); return -1; }
    for (p = js_array(json, NULL, "files"); p && *p == '{' && n < P2P_MAX_ARQ;
         p = js_prox(js_fim(p)), n++) {
      const char *f = js_fim(p);
      buf[n][0] = 0;
      if (!js_texto(p, f, "path", buf[n], sizeof buf[n]) &&
          !js_texto(p, f, "name", buf[n], sizeof buf[n])) buf[n][0] = 0;
      pp[n] = buf[n];
      tt[n] = js_num(p, f, "length", 0);
    }
    r = p2p_escolher_lista(pp, tt, n, fileIdx, temporada, episodio);
    free(buf); free(pp); free(tt);
  }
  return r;
}

// Trackers de reserva para o magnet quando o addon nao manda "sources": os
// mesmos que o Torrentio poe na maioria dos streams.
static const char *TRACKERS_RESERVA[] = {
  "udp://tracker.opentrackr.org:1337/announce",
  "udp://open.stealth.si:80/announce",
  "udp://tracker.torrent.eu.org:451/announce",
  "udp://exodus.desync.com:6969/announce",
  NULL
};

static int pctCodificar(const char *s, size_t L, char *out, unsigned n) {
  static const char hx[] = "0123456789ABCDEF";
  unsigned u = 0;
  for (size_t i = 0; i < L; i++) {
    unsigned char c = (unsigned char)s[i];
    if (isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~') {
      if (u + 1 >= n) return -1;
      out[u++] = (char)c;
    } else {
      if (u + 3 >= n) return -1;
      out[u++] = '%'; out[u++] = hx[c >> 4]; out[u++] = hx[c & 15];
    }
  }
  out[u] = 0;
  return (int)u;
}

int p2p_magnet(const char *hash, const char *fontes, char *out, unsigned n) {
  unsigned u;
  int i, e, k = 0;
  if (!p2p_hash_valido(hash) || !out || n < 64) return 0;
  e = snprintf(out, n, "magnet:?xt=urn:btih:");
  u = (unsigned)e;
  for (i = 0; i < 40; i++) out[u++] = (char)tolower((unsigned char)hash[i]);
  out[u] = 0;
  for (const char *p = fontes; p && *p;) {
    const char *nl = strchr(p, '\n');
    size_t L = nl ? (size_t)(nl - p) : strlen(p);
    int ok = L > 8 && L < 300 && !strncmp(p, "tracker:", 8);
    for (size_t q = 0; ok && q < L; q++)
      if ((unsigned char)p[q] < 33 || p[q] == 127) ok = 0;
    if (ok && u + 5 < n) {
      memcpy(out + u, "&tr=", 4);
      e = pctCodificar(p + 8, L - 8, out + u + 4, n - u - 4);
      if (e > 0) { u += 4 + (unsigned)e; k++; }
      else out[u] = 0;                         // nao coube: o resto fica de fora
    }
    p = nl ? nl + 1 : p + L;
  }
  for (i = 0; !k && TRACKERS_RESERVA[i]; i++) {
    if (u + 5 >= n) break;
    memcpy(out + u, "&tr=", 4);
    e = pctCodificar(TRACKERS_RESERVA[i], strlen(TRACKERS_RESERVA[i]), out + u + 4, n - u - 4);
    if (e > 0) u += 4 + (unsigned)e; else { out[u] = 0; break; }
  }
  return 1;
}

int p2p_url_reproducao(const char *base, const char *hash, int idx, char *out, unsigned n) {
  char h[41];
  int i;
  if (!base || !p2p_hash_valido(hash) || idx < 0 || !out) return 0;
  for (i = 0; i < 40; i++) h[i] = (char)tolower((unsigned char)hash[i]);
  h[40] = 0;
  i = snprintf(out, n, "%s/%s/%d", base, h, idx);
  return i > 0 && (unsigned)i < n;
}

// ---------------------------------------------------------------- rede

// Passo 1: /settings responde {"values":{"serverVersion":"4.21.2",...}} no
// stremio/server. Qualquer outra coisa que responda 200 (um roteador, um Plex)
// nao tem esse campo, e "conectou" seria mentira.
int p2p_testar(char *detalhe, unsigned n) {
  char url[300], v[32] = "";
  int st = 0;
  char *r;
  if (detalhe && n) detalhe[0] = 0;
  // Sem endereco e com o motor embutido: nao ha rede a testar; o teste diz que
  // o motor esta neste build, a versao dele e o teto de disco que ele teria
  // agora (statvfs no fio do teste, nunca no da tela). statvfs falhou ou pouco
  // livre: o mesmo erro que o motor daria ao subir.
  if (!ajustes_p2p_url()[0] && p2pmotor_disponivel())
    return ultimoErro = p2pmotor_resumo(detalhe, n);
  // Testa com o ajuste DESLIGADO tambem: a pessoa confere o endereco antes de
  // ligar. Sem endereco nao ha o que testar.
  if (!ajustes_p2p_url()[0]) {
    printf("[p2p] sem endereco e sem motor: %s\n", p2pmotor_motivo_indisponivel());
    return ultimoErro = P2P_ERR_DESLIGADO;
  }
  snprintf(url, sizeof url, "%s/settings", ajustes_p2p_url());
  r = rede_baixar_st(url, P2P_PRAZO_TESTE, NULL, &st);
  if (!r) return ultimoErro = P2P_ERR_SERVIDOR;
  if (st != 200 || !js_texto(r, NULL, "serverVersion", v, sizeof v)) {
    free(r);
    return ultimoErro = P2P_ERR_NAO_STREMIO;
  }
  free(r);
  if (detalhe && n) snprintf(detalhe, n, "%s", v);
  return ultimoErro = P2P_OK;
}

int p2p_resolver(const char *hash, int fileIdx, const char *fontes, char *url, unsigned n) {
  char corpo[1800], criar[300], base[200], *r, *pedaco;
  const char *cab[2] = { "Content-Type: text/plain", NULL };
  int st = 0, idx, temp = 0, ep = 0, erro;
  long tam = 0;
  if (url && n) url[0] = 0;
  if (!url || !n) return ultimoErro = P2P_ERR_DESLIGADO;
  if (!p2p_ativo()) return ultimoErro = P2P_ERR_DESLIGADO;
  if (!p2p_hash_valido(hash)) return ultimoErro = P2P_ERR_HASH;
  if (p2p_usa_motor()) {
    debrid_episodio(&temp, &ep);
    return ultimoErro = p2pmotor_resolver(hash, fileIdx, fontes, temp, ep, url, n);
  }
  snprintf(base, sizeof base, "%s", ajustes_p2p_url());
  // 1) o servidor esta la e e o certo? Falhar aqui em 6 s evita esperar os 30 s
  // dos metadados para descobrir que o IP estava errado.
  erro = p2p_testar(NULL, 0);
  if (erro != P2P_OK) { printf("[p2p] servidor: erro %d\n", erro); return erro; }
  // 2) registrar o torrent e esperar os metadados.
  if (!p2p_corpo_criar(hash, fontes, corpo, sizeof corpo)) return ultimoErro = P2P_ERR_HASH;
  { char h[41]; int i;
    for (i = 0; i < 40; i++) h[i] = (char)tolower((unsigned char)hash[i]);
    h[40] = 0;
    snprintf(criar, sizeof criar, "%s/%s/create", base, h); }
  r = rede_postar_st(criar, P2P_PRAZO_METADADOS, cab, corpo, &st);
  // Sem resposta: o servidor estava vivo um instante atras (passo 1), entao o
  // que faltou foram os metadados — torrent sem peers.
  if (!r && st == 0) {
    printf("[p2p] %.8s: metadados nao chegaram em %d s\n", hash, P2P_PRAZO_METADADOS);
    return ultimoErro = P2P_ERR_SEM_PEERS;
  }
  if (!r || st == 504) { free(r); printf("[p2p] %.8s: HTTP %d nos metadados\n", hash, st);
    return ultimoErro = (st == 504 ? P2P_ERR_SEM_PEERS : P2P_ERR_RECUSOU); }
  if (st != 200) {
    free(r);
    printf("[p2p] %.8s: create HTTP %d\n", hash, st);
    return ultimoErro = P2P_ERR_RECUSOU;
  }
  debrid_episodio(&temp, &ep);
  idx = p2p_escolher_arquivo(r, fileIdx, temp, ep);
  free(r);
  if (idx < 0) { printf("[p2p] %.8s: sem arquivo de video\n", hash);
    return ultimoErro = P2P_ERR_SEM_VIDEO; }
  if (!p2p_url_reproducao(base, hash, idx, url, n)) { url[0] = 0; return ultimoErro = P2P_ERR_RECUSOU; }
  // 3) os primeiros KB de VIDEO tem de chegar. create so prova que ha metadados;
  // um torrent com o .torrent no DHT mas sem ninguem semeando passa no passo 2
  // e trava o player em "carregando" para sempre. 64 KB abrem a conexao com
  // peers e aquecem o cache do servidor para o pedido do player.
  st = 0;
  pedaco = rede_baixar_trecho_st(url, P2P_PRAZO_BYTES, 0, 65535, &tam, &st, NULL, NULL, 0);
  if (!pedaco) {
    printf("[p2p] %.8s: arquivo %d sem bytes (HTTP %d)\n", hash, idx, st);
    url[0] = 0;
    return ultimoErro = (st == 0 ? P2P_ERR_SEM_PEERS : P2P_ERR_RECUSOU);
  }
  free(pedaco);
  printf("[p2p] %.8s: arquivo %d pronto\n", hash, idx);
  return ultimoErro = P2P_OK;
}
