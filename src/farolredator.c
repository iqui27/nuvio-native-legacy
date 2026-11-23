#include "farolredator.h"
#include "farolnativo.h"
#include "sha256.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define I(k) {k, FT_INT, NULL}
#define B(k) {k, FT_BOOL, NULL}
#define N(k) {k, FT_NUM1, NULL}
#define T(k) {k, FT_TOKEN, NULL}
#define E(k,v) {k, FT_ENUM, v}
#define V(k) {k, FT_VERSAO, NULL}
#define H(k) {k, FT_HASH, NULL}
#define O(k) {k, FT_OBJETO, NULL}
#define A(k) {k, FT_ARRAY, NULL}
static const FarolCampo schema[] = {
  I("v"), E("gatilho", "falha_sem_conserto|manual|falha_repetida"),
  E("sintoma", "nao_comeca|trava|sem_som|legenda|qualidade|outro"),
  O("erro"), E("erro.codigo", "NENHUM|REDE_FORA|REDE_NAO_CHEGA|ADDON_NENHUM|ADDON_LENTO|DEBRID_DESCONECTOU|DEBRID_SEM_PLANO|FONTE_RECUSOU|FONTE_SEM_RANGE|REDE_CURTA|TV_SEM_MEMORIA|TV_NEGOU_PLAYER|AUDIO_DTS_SEM_SAIDA|AUDIO_OUTRA_FAIXA|AUDIO_SEM_SAIDA|IMAGEM_DV|TV_FORMATO|TV_FORMATO_REPETE|PLAYER_PAROU|NADA_ACHADO"),
  E("erro.estagio", "internet|addons|debrid|fonte|tv|audio"),
  T("erro.nativo"), I("erro.nativo_num"), I("erro.http"), T("erro.fingerprint"), H("erro.fp"),
  A("erro.tentou"), O("erro.tentou[]"),
  E("erro.tentou[].acao", "proxima_fonte|reconexao|sem_dv|sem_dts|fonte_menor|converter_ac3|isolar_addon|aliviar_memoria|reconectar_rede|limpar_cache|buffer_maior|desligar_hdr|login_debrid|login_addon|player_externo"),
  B("erro.tentou[].ok"), I("erro.tentou[].ms"),
  O("aparelho"), E("aparelho.plataforma", "webos|tizen_wgt|tizen_tpk|android|outra"),
  V("aparelho.so"), I("aparelho.so_maior"), T("aparelho.modelo"),
  I("aparelho.ram_total_mb"), I("aparelho.ram_livre_mb"), O("aparelho.tela"),
  I("aparelho.tela.w"), I("aparelho.tela.h"), I("aparelho.tela.hdr"), I("aparelho.tela.dv"),
  I("aparelho.tela.hevc"), I("aparelho.tela.av1"), O("aparelho.audio"),
  I("aparelho.audio.dts"), I("aparelho.audio.truehd"), I("aparelho.audio.eac3"), I("aparelho.audio.ac3"),
  O("aparelho.rede"), B("aparelho.rede.offline"), I("aparelho.rede.hosts_com_falha"), N("aparelho.rede.vazao_mbps"),
  O("cliente"), V("cliente.versao"), H("cliente.commit"),
  E("cliente.build", "webos-ipk|tizen-wgt|tizen-tpk|tizen-tpk40|tizen-tpk60|tizen-tpk65|android-apk|debug"),
  E("cliente.backend", "starfish|avplay|tpk|media3|outro"),
  E("cliente.idioma", "pt-BR|en|es|fr|de|it|ar|bg|ca|cs|da|el|fi|he|hi|hr|hu|id|ja|ko|nl|no|pl|ro|ru|sk|sv|tr|uk|vi|zh-CN|zh-TW"),
  I("cliente.aberto_s"), E("cliente.gpu", "auto:alto|auto:medio|auto:baixo|alto|medio|baixo|desconhecido"),
  O("cliente.ajustes"), I("cliente.ajustes.glass_ui"), E("cliente.ajustes.qualidade_max", "auto|4k|1080p|720p|480p"),
  N("cliente.ajustes.tamanho_max_gb"), E("cliente.ajustes.dts", "auto|ac3|aac|direto|desligado"),
  I("cliente.ajustes.dv_mkv"), I("cliente.ajustes.legenda_auto_sync"),
  O("cliente.addons"), I("cliente.addons.total"), I("cliente.addons.ativos"), I("cliente.addons.isolados"),
  A("cliente.addons.tipos"), E("cliente.addons.tipos[]", "catalogo|fontes|legendas|meta"),
  A("cliente.debrid"), E("cliente.debrid[]", "torbox|realdebrid|alldebrid|premiumize|debridlink|easydebrid"),
  A("cliente.contas"), E("cliente.contas[]", "trakt|simkl"),
  O("midia"), E("midia.container", "mkv|mp4|ts|hls|dash|avi|webm|outro|x"),
  E("midia.video", "h264|hevc|hevc10|av1|vp9|mpeg2|outro|x"),
  E("midia.hdr", "sdr|hdr10|hdr10plus|hlg|dv|x"), E("midia.res", "2160p|1080p|720p|480p|outra|x"),
  N("midia.fps"), N("midia.mbps"), N("midia.gb"),
  E("midia.audio", "aac|aac_20|ac3|ac3_51|eac3|eac3_51|eac3_atmos|dts|dts_51|truehd|truehd_atmos|mp3|flac|outro|x"),
  E("midia.legenda", "srt_embutida|srt_externa|ass_embutida|ass_externa|pgs|nenhuma|outra|x"),
  E("midia.entrega", "debrid_cache|debrid|http|torrent|local|outro|x"), I("midia.http"), I("midia.ttfb_ms"),
  A("cadeia"), O("cadeia[]"), E("cadeia[].passo", "internet|addons|debrid|fonte|tv|audio"),
  E("cadeia[].estado", "ok|falhou|nao_testado|testando"), I("cadeia[].ms"), I("cadeia[].http"),
  A("linha"), O("linha[]"), I("linha[].t"),
  E("linha[].m", "player|video|rede|addon|debrid|fonte|audio|legenda|memoria|farol"),
  E("linha[].e", "abrir|pronto|erro_fonte|recusa_sinc|parado|erro_nativo|audio_recusado|pipeline_morreu|reconexao|rede_estado|addon_busca|addon_lento|debrid|fonte_escolhida|escolhida|fonte_proxima|mem_baixa|farol_veredito|farol_acao|farol_resultado|farol_envio|farol_consertou"),
  I("linha[].a"), I("linha[].b"), T("linha[].k"),
  O("privacidade"), I("privacidade.redator"), I("privacidade.campos_recusados")
};
_Static_assert(sizeof schema / sizeof *schema <= 256, "schema maior que bitmap");

const FarolCampo *farolredator_schema(size_t *n) {
  if (n) *n = sizeof schema / sizeof *schema;
  return schema;
}
static int lista(const char *s, const char *opcoes) {
  size_t n = strlen(s);
  if (!n || !opcoes) return 0;
  for (const char *p = opcoes; *p;) {
    const char *fim = strchr(p, '|');
    size_t len = fim ? (size_t)(fim - p) : strlen(p);
    if (len == n && !memcmp(s, p, n)) return 1;
    p += len;
    if (*p) p++;
  }
  return 0;
}
extern void farolanel_token(const char *s, char out[FAROL_REDATOR_TOKEN_N]) __attribute__((weak));
void farolredator_token(const char *s, char out[FAROL_REDATOR_TOKEN_N]) {
  if (farolanel_token) { farolanel_token(s, out); return; }
  size_t n = 0;
  if (!s) { strcpy(out, "x"); return; }
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    unsigned char c = *p;
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || strchr("_.:+-", c))) {
      strcpy(out, "x"); return;
    }
    if (n < FAROL_REDATOR_TOKEN_N - 1) out[n++] = (char)c;
  }
  out[n] = 0;
  if (!n) strcpy(out, "x");
}
static int hexval(const char *s, size_t min, size_t max) {
  size_t n = strlen(s);
  return n >= min && n <= max && strspn(s, "0123456789abcdef") == n;
}
// Um pedaco IPv4 do parser WHATWG: "0x"/"0X" abre hexadecimal ("0x" sozinho
// vale 0), "0" seguido de mais digitos abre octal, o resto e decimal. Zeros a
// esquerda sao livres, sem teto de comprimento; valor acima de 32 bits satura
// — todo valor grande falha igual, mas um digito invalido mais a frente ainda
// invalida o pedaco inteiro.
static int ipv4_numero(const char *p, size_t n, unsigned long long *out) {
  unsigned long long v = 0;
  int base = 10;
  size_t i = 0;
  if (!n) return 0;
  if (p[0] == '0' && n > 1) {
    if (p[1] == 'x' || p[1] == 'X') { base = 16; i = 2; }
    else { base = 8; i = 1; }
  }
  for (; i < n; i++) {
    int d;
    char c = p[i];
    if (c >= '0' && c <= '9') d = c - '0';
    else if (base == 16 && c >= 'a' && c <= 'f') d = c - 'a' + 10;
    else if (base == 16 && c >= 'A' && c <= 'F') d = c - 'A' + 10;
    else return 0;
    if (d >= base) return 0;
    if (v <= 0xffffffffULL) {
      v = v * (unsigned)base + (unsigned)d;
      if (v > 0xffffffffULL) v = 0x100000000ULL;
    }
  }
  *out = v;
  return 1;
}
// 1 quando s nao e um dominio valido para um parser de URL WHATWG: ou o
// parser le como IPv4 — 1-4 partes decimais/octais/hex, a ultima expandindo
// para os octetos que faltam ("127.0.1" = 127.0.0.1, "0x" = 0,
// "0x000c633642a" = 198.51.100.42) — ou a ultima parte e um "numero" e o
// parse falha, o que o WHATWG marca como host invalido, nao dominio. Nas
// duas saidas o trecho nao sai, nem com hash.
static int ipv4_whatwg(const char *s) {
  size_t n = strlen(s), len;
  const char *p, *fim, *ultimo;
  unsigned long long v;
  int partes = 0;
  if (!n) return 0;
  if (s[n-1] == '.') n--;              // ponto final nao conta
  if (!n) return 0;
  fim = s + n;
  // "Termina em numero": so a ultima parte decide se o WHATWG tenta IPv4 —
  // senao e dominio de verdade e sai normalmente (com hash).
  ultimo = fim;
  while (ultimo > s && ultimo[-1] != '.') ultimo--;
  len = (size_t)(fim - ultimo);
  if (!len || (!ipv4_numero(ultimo, len, &v) &&
               strspn(ultimo, "0123456789") != len))
    return 0;
  for (p = s; ; ) {
    const char *q = memchr(p, '.', (size_t)(fim - p));
    len = q ? (size_t)(q - p) : (size_t)(fim - p);
    if (++partes > 4 || !len || !ipv4_numero(p, len, &v)) return 1;
    if (!q) break;
    if (v > 255) return 1;
    p = q + 1;
  }
  return 1;
}
// "2.1", "2.1.0-ipk", "4.9-webos": parte numerica (1-3 partes de ate 4
// digitos) mais rotulo opcional "-tag". O rotulo e da lista fechada de
// origens — qualquer outro texto atras do "-" e recusado: um sufixo livre
// deixaria passar PIN ("4.9-7362194") e titulo ("2.1.0-segreditulo").
// A numerica pura que um parser de URL le como IPv4 ("4.9", "127.0.1",
// "10.1") tambem e recusada — o coletor envia a versao com o rotulo.
static int versao(const char *s) {
  const char *tag = strchr(s, '-');
  char num[24];
  size_t n, i;
  int partes = 1, digitos = 0;
  if (!*s || strlen(s) > 23) return 0;
  n = tag ? (size_t)(tag - s) : strlen(s);
  if (!n || n >= sizeof num) return 0;
  if (tag && !lista(tag + 1,
        "ipk|webos|tizen|wgt|tpk|tpk40|tpk60|tpk65|apk|android|dev|debug"))
    return 0;
  memcpy(num, s, n); num[n] = 0;
  if (!strchr(num, '.') && n > 2) return 0;
  for (i = 0; i < n; i++) {
    char c = num[i];
    if (c >= '0' && c <= '9') { if (++digitos > 4) return 0; }
    else if (c == '.' && digitos && partes < 3) { partes++; digitos = 0; }
    else return 0;
  }
  if (!digitos || partes > 3) return 0;
  return !ipv4_whatwg(s);
}
// Commit do build: 7-12 hex com ao menos uma letra — um PIN de 4-8 digitos
// (admin.h) e sempre so digitos e nunca passa; um SHA curto todo numerico e
// recusado na direcao segura.
static int commit(const char *s) {
  return hexval(s, 7, 12) && strpbrk(s, "abcdef") != NULL;
}
static int modelo(const char *s) {
  const char *prefixos[] = {"OLED", "QN", "QE", "UE", "UN", "UA", "KD-", "XR-", "TCL"};
  if (!strcmp(s, "x")) return 1;
  for (size_t i = 0; i < sizeof prefixos / sizeof *prefixos; i++) {
    size_t n = strlen(prefixos[i]);
    if (strncmp(s, prefixos[i], n)) continue;
    const char *p = s+n;
    size_t tela = strspn(p, "0123456789");
    if (tela != 2) return 0;
    p += tela;
    size_t serie = strspn(p, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    if (!serie || serie > (i ? 2u : 1u)) return 0;
    p += serie;
    size_t numero = strspn(p, "0123456789");
    if (!numero || numero > (i ? 4u : 2u)) return 0;
    p += numero;
    size_t sufixo = strspn(p, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    return !p[sufixo] && sufixo <= (i ? 6u : 3u);
  }
  // Espelho do modeloOk do servidor publicado (recomendacoes/src/farol.js):
  // fora dos prefixos nao ha excecao — LG com digito a frente ("50NANO80ASA")
  // nao passa no servidor, entao aqui tambem nao passa (o pacote voltaria 422).
  return 0;
}
static int fingerprint(const char *s) {
  char copia[160];
  if (strlen(s) >= sizeof copia) return 0;
  strcpy(copia, s);
  const char *valores[] = {"webos|tizen_wgt|tizen_tpk|android|outra", NULL,
    "starfish|avplay|tpk|media3|outro", NULL,
    "mkv|mp4|ts|hls|dash|avi|webm|outro|x", "h264|hevc|hevc10|av1|vp9|mpeg2|outro|x",
    "sdr|hdr10|hdr10plus|hlg|dv|x", "aac|ac3|eac3|dts|truehd|mp3|flac|outro|x"};
  char *p = copia;
  for (size_t i = 0; i < 8; i++) {
    char *fim = strchr(p, '-');
    if ((i < 7 && !fim) || (i == 7 && fim)) return 0;
    if (fim) *fim = 0;
    if (i == 1) { if (!*p || strlen(p) > 2 || strspn(p, "0123456789") != strlen(p)) return 0; }
    else if (i == 3) {
      char tok[FAROL_REDATOR_TOKEN_N];
      farolredator_token(p, tok);
      if (strcmp(tok, p) || !(farolnativo_cod_ok(p) || !strcmp(p, "x"))) return 0;
    } else if (!lista(p, valores[i])) return 0;
    if (fim) p = fim+1;
  }
  return 1;
}
// "<res>_<sdr|hdr>_<dv|nodv>_<ac3|eac3|dts|outro>": o token composto de
// partes fechadas que o produtor de fonte (farol-10) monta de enums.
static int token_fonte(const char *s) {
  static const char *const partes[] = {
    "sd|480p|720p|1080p|2160p", "sdr|hdr", "dv|nodv", "ac3|eac3|dts|outro"};
  char copia[32];
  size_t n = strlen(s);
  if (n >= sizeof copia) return 0;
  strcpy(copia, s);
  char *p = copia;
  for (size_t i = 0; i < 4; i++) {
    char *fim = strchr(p, '_');
    if ((i < 3 && !fim) || (i == 3 && fim)) return 0;
    if (fim) *fim = 0;
    if (!lista(p, partes[i])) return 0;
    if (fim) p = fim + 1;
  }
  return 1;
}
static int token_seguro(const char *s) {
  // Alem dos codigos nativos, do pseudonimo h: e do token composto da fonte,
  // a lista fechada tem os valores FIXOS de linha[].k que os produtores
  // emitem (grep de farol_ev nas branches farol-10 e farol-7): busca
  // (FE_ADDON_BUSCA, addons.c), proxima (FE_FONTE_PROXIMA, streams.c),
  // mem_baixa (FE_MEM_BAIXA, memlog.c), mpv (FE_ABRIR sob NV_DESKTOP_MPV,
  // player.c), dns/tls/prazo/recusa (FE_REDE_ESTADO, redesaude.c),
  // recusa/sem_plano/fora_cache (FE_DEBRID, debrid.c) e os backends
  // starfish/avplay/tpk/media3 (FE_ABRIR, player.c).
  if (lista(s, "x|starfish|avplay|tpk|media3|mpv|dns|tls|prazo|recusa|sem_plano|fora_cache|busca|proxima|mem_baixa|2160p_hdr_dv_eac3")) return 1;
  if (!strncmp(s, "h:", 2)) return hexval(s + 2, 8, 8);
  if (farolnativo_cod_ok(s)) return 1;
  if (token_fonte(s)) return 1;
  // O pseudonimo do add-on e o "h:" de cima: hash do identificador com o sal
  // da instalacao (a mesma funcao do host, farolredator_hash_host — PLANO
  // 2.5 "nome de add-on fora da lista publica: idem"). O formato addon_<8hex>
  // era o FNV-1a sem sal do produtor antigo (src/addons.c): estavel entre
  // instalacoes, virava identificador — recusado.
  return 0;
}
static int recusar(FarolJson *j) {
  if (j && j->campos_recusados < UINT_MAX) j->campos_recusados++;
  return 0;
}
int farolredator_abrir(FarolJson *j, char *buf, size_t tam) {
  if (!j) return 0;
  memset(j, 0, sizeof *j);
  if (!buf || tam < 3) return recusar(j);
  j->buf = buf; j->tam = tam < FAROL_JSON_MAX + 1 ? tam : FAROL_JSON_MAX + 1;
  strcpy(buf, "{"); j->n = 1; j->prof = 1;
  return 1;
}
static int procurar(FarolJson *j, const char *chave, char caminho[96]) {
  if (!j || !j->buf || j->fechado || j->prof < 1) return -1;
  FarolJsonNivel *p = &j->nivel[j->prof-1];
  int n;
  if (p->array) {
    if (chave) return -1;
    n = snprintf(caminho, 96, "%s[]", p->caminho);
  } else {
    if (!chave || !*chave || strspn(chave, "abcdefghijklmnopqrstuvwxyz_0123456789") != strlen(chave)) return -1;
    n = snprintf(caminho, 96, "%s%s%s", p->caminho, *p->caminho ? "." : "", chave);
  }
  if (n < 0 || n >= 96) return -1;
  for (size_t i = 0; i < sizeof schema / sizeof *schema; i++)
    if (!strcmp(caminho, schema[i].caminho)) return (int)i;
  return -1;
}
static int escrever(FarolJson *j, const char *chave, int idx, const char *valor) {
  FarolJsonNivel *p = &j->nivel[j->prof-1];
  uint64_t bit = (uint64_t)1 << (idx % 64);
  if ((!p->array && (p->vistos[idx/64] & bit)) || (p->array && p->itens >= 256)) return recusar(j);
  char campo[320];
  int n = p->array ? snprintf(campo, sizeof campo, "%s%s", p->itens ? "," : "", valor) :
    snprintf(campo, sizeof campo, "%s\"%s\":%s", p->itens ? "," : "", chave, valor);
  if (n < 0 || (size_t)n >= sizeof campo || j->n + (size_t)n + (size_t)j->prof + 2 > j->tam) return recusar(j);
  memcpy(j->buf+j->n, campo, (size_t)n+1); j->n += (size_t)n;
  p->itens++; p->vistos[idx/64] |= bit;
  return 1;
}
int farolredator_campo(FarolJson *j, const char *chave, FarolTipo t, const void *valor) {
  char caminho[96], texto[256], tok[FAROL_REDATOR_TOKEN_N];
  int idx = procurar(j, chave, caminho);
  if (idx < 0 || !valor || schema[idx].tipo != t || t >= FT_OBJETO || t < FT_INT) return recusar(j);
  if (t == FT_INT) snprintf(texto, sizeof texto, "%d", *(const int *)valor);
  else if (t == FT_BOOL) {
    int v = *(const int *)valor;
    if (v != 0 && v != 1) return recusar(j);
    strcpy(texto, v ? "true" : "false");
  } else if (t == FT_NUM1) {
    double v = *(const double *)valor;
    if (!isfinite(v) || v < 0 || v > 1000000000) return recusar(j);
    unsigned long long decimos = (unsigned long long)(v * 10 + 0.5);
    snprintf(texto, sizeof texto, "%llu.%llu", decimos/10, decimos%10);
  } else {
    const char *s = valor;
    if (strlen(s) > 160) return recusar(j);
    if (t == FT_ENUM && !lista(s, schema[idx].valores)) return recusar(j);
    if (t == FT_VERSAO && !versao(s)) return recusar(j);
    if (t == FT_HASH && !(!strcmp(caminho, "cliente.commit") ? commit(s) : hexval(s, 16, 16))) return recusar(j);
    if (t == FT_TOKEN) {
      farolredator_token(s, tok);
      if (!strcmp(caminho, "aparelho.modelo")) {
        if (!modelo(s)) return recusar(j);
      } else if (!strcmp(caminho, "erro.fingerprint")) {
        if (!fingerprint(s)) return recusar(j);
      } else {
        if (!token_seguro(tok)) return recusar(j);
        s = tok;
      }
    }
    snprintf(texto, sizeof texto, "\"%s\"", s);
  }
  return escrever(j, chave, idx, texto);
}
static int container(FarolJson *j, const char *chave, FarolTipo t) {
  char caminho[96];
  int idx = procurar(j, chave, caminho);
  if (idx < 0 || schema[idx].tipo != t || j->prof >= FAROL_JSON_PROF) return recusar(j);
  if (!escrever(j, chave, idx, t == FT_ARRAY ? "[" : "{")) return 0;
  FarolJsonNivel *p = &j->nivel[j->prof++];
  memset(p, 0, sizeof *p); strcpy(p->caminho, caminho); p->array = t == FT_ARRAY;
  return 1;
}
int farolredator_objeto(FarolJson *j, const char *chave) { return container(j, chave, FT_OBJETO); }
int farolredator_array(FarolJson *j, const char *chave) { return container(j, chave, FT_ARRAY); }
int farolredator_fechar(FarolJson *j) {
  if (!j || !j->buf || j->fechado || j->prof < 1 || j->n + 1 >= j->tam) return recusar(j);
  j->buf[j->n++] = j->nivel[--j->prof].array ? ']' : '}'; j->buf[j->n] = 0;
  if (!j->prof) j->fechado = 1;
  return 1;
}
static int host_ler(const char *entrada, char host[254]) {
  if (!entrada || !*entrada) return 0;
  const char *p = entrada, *fim;
  if (!strncmp(p, "https://", 8)) p += 8;
  else if (!strncmp(p, "http://", 7)) p += 7;
  fim = p + strcspn(p, "/?#");
  const char *arroba = memchr(p, '@', (size_t)(fim-p));
  if (arroba) p = arroba+1;
  size_t n = 0;
  while (p < fim && *p != ':') {
    unsigned char c = (unsigned char)*p++;
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-') || n >= 253) return 0;
    host[n++] = (char)c;
  }
  if (n && host[n-1] == '.') n--;
  host[n] = 0;
  if (!n || host[0] == '.' || host[0] == '-' || host[n-1] == '-' || strstr(host, "..")) return 0;
  if (p < fim) {
    if (*p++ != ':' || p == fim) return 0;
    for (; p < fim; p++) if (*p < '0' || *p > '9') return 0;
  }
  if (strspn(host, "0123456789.") == n || ipv4_whatwg(host)) return 0;
  return 1;
}
int farolredator_host_publico(const char *entrada) {
  char host[254];
  if (!entrada || strpbrk(entrada, "/?#@:")) return 0;
  if (!host_ler(entrada, host)) return 0;
  return lista(host, "api.real-debrid.com|real-debrid.com|api.alldebrid.com|alldebrid.com|api.torbox.app|torbox.app|www.premiumize.me|premiumize.me|debrid-link.com|api.themoviedb.org|image.tmdb.org|api.trakt.tv|api.simkl.com|v3-cinemeta.strem.io");
}
void farolredator_hash_host(const char *entrada, const unsigned char sal[16], char out[12]) {
  char host[254], digest[65];
  strcpy(out, "x");
  if (!sal || !host_ler(entrada, host)) return;
  Sha s; unsigned char d[32];
  sha_ini(&s); sha_add(&s, sal, 16); sha_add(&s, (const unsigned char *)host, strlen(host)); sha_fim(&s, d);
  hex(d, 32, digest); snprintf(out, 12, "h:%.8s", digest);
}
