// FAROL — erro nativo normalizado. Ver a tabela de assinaturas em farolerro.h.
// Modulo puro: so string.h/stdlib.h/ctype.h e fontesonda_causa (tambem puro).
// A lista real de textos do AVPlay/Media3 do D1 esta pendente (dono); o que
// nao casa vira "<plataforma>_outro" para o registro capturar a assinatura.
#include "farolerro.h"
#include "fontesonda.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// Substring sem caso, igual a nv_recon_tem: nao depende de strcasestr
// (_GNU_SOURCE), que nem toda toolchain da TV expoe.
static int tem(const char *t, const char *agulha) {
  size_t n = strlen(agulha);
  for (; t && *t; t++) {
    size_t i;
    for (i = 0; i < n; i++) {
      char a = t[i], b = agulha[i];
      if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
      if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
      if (a != b) break;
    }
    if (i == n) return 1;
  }
  return 0;
}

// Ponteiro para a primeira ocorrencia sem caso, ou NULL.
static const char *acha(const char *t, const char *agulha) {
  size_t n = strlen(agulha);
  for (; t && *t; t++) {
    size_t i;
    for (i = 0; i < n; i++) {
      char a = t[i], b = agulha[i];
      if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
      if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
      if (a != b) break;
    }
    if (i == n) return t;
  }
  return NULL;
}

// Inteiro no inicio do texto (o formato "<codigo> <texto>" do uMS e do
// Media3), ou -1 quando o texto nao comeca com digito.
static long digito(const char *t) {
  if (!t || !isdigit((unsigned char)*t)) return -1;
  return strtol(t, NULL, 10);
}

static const char *prefixo(FarolPlat p) {
  static const char *const nomes[] = {"ums", "av", "tpk", "m3", "gen"};
  return nomes[p >= FP_WEBOS && p <= FP_OUTRA ? (int)p : (int)FP_OUTRA];
}
const char *farolerro_prefixo(FarolPlat p) { return prefixo(p); }

const char *farolerro_classe_nome(FarolClasse c) {
  static const char *const nomes[] = {"nenhuma", "rede", "http", "formato",
                                      "audio", "sistema", "parado"};
  return c >= 0 && c <= FC_PARADO ? nomes[c] : "?";
}

static void numero(FarolPlat p, FarolErro *e, long n) {
  snprintf(e->cod, sizeof e->cod, "%s_%ld", prefixo(p), n);
}

// O que nao casa com assinatura nenhuma cai em fontesonda_causa: o mesmo
// classificador que o cartao de erro ja usa, portanto nunca discordam.
static int generico(FarolPlat p, const char *t, FarolErro *e) {
  int http = 0, c = fontesonda_causa(t, &http);
  const char *suf;
  if (c == FS_CAUSA_NENHUMA) return 0;
  if (c == FS_CAUSA_HTTP) { e->classe = FC_HTTP; e->http = http; suf = "http"; }
  else if (c == FS_CAUSA_REDE) {
    e->classe = FC_REDE;
    suf = p == FP_TIZEN_WGT || p == FP_TIZEN_TPK ? "conexao" : "rede";
  } else { e->classe = FC_FORMATO; suf = "formato"; }
  snprintf(e->cod, sizeof e->cod, "%s_%s", prefixo(p), suf);
  return 1;
}

// --- LG webOS (uMS). erroTexto = "<errorCode> <errorText>". -----------------
static int lerWebos(const char *t, int dec, FarolErro *e) {
  long n;
  // "com.webos.pipeline.<id> is not running": o pipeline morreu e o app ja o
  // recarrega sozinho; e defeito do sistema, nao da fonte.
  if (tem(t, "is not running")) {
    e->classe = FC_SISTEMA; snprintf(e->cod, sizeof e->cod, "ums_pipeline");
    return 1;
  }
  // Hub LS2 negou por permissao/privilegio nesta sessao (video_registro_negado).
  if (dec == -1027 || dec == -1031 ||
      (tem(t, "permission") &&
       (tem(t, "ls2") || tem(t, "luna") || tem(t, "palm") || tem(t, "hub")))) {
    e->classe = FC_SISTEMA; snprintf(e->cod, sizeof e->cod, "ls2_permissao");
    return 1;
  }
  // "Cannot safely identify the selected native audio track" (video.c:1337).
  if (tem(t, "native audio track")) {
    e->classe = FC_AUDIO; snprintf(e->cod, sizeof e->cod, "ums_faixa_audio");
    return 1;
  }
  n = digito(t);
  if (n < 0 && dec > 0) n = dec;
  if (n == 200) {  // "Audio Codec Not Supported": o video segue, mudo.
    e->classe = FC_AUDIO; numero(FP_WEBOS, e, n); return 1;
  }
  if (n == 300) {  // "Network Error", medido nos registros da LG.
    e->classe = FC_REDE; numero(FP_WEBOS, e, n); return 1;
  }
  if (n >= 40000 && n < 50000) {  // 40000+status: o servidor da fonte recusou.
    e->classe = FC_HTTP; numero(FP_WEBOS, e, n);
    if (n >= 40400 && n <= 40599) e->http = (int)(n - 40000);
    return 1;
  }
  // Familia 1xx/2xx e o que o proprio ARQUIVO decide (202 Media Not Found,
  // 203 AV Type Not Founded, 204 Fail To Demultiplex, 210 Unknown Subtitle);
  // o 100 "Playing error" generico fica junto.
  if (n >= 100 && n < 300) {
    e->classe = FC_FORMATO; numero(FP_WEBOS, e, n); return 1;
  }
  return generico(FP_WEBOS, t, e);
}

// --- Android (Media3). erroTxt = "%d player error:%d". -----------------------
static int lerAndroid(const char *t, int dec, FarolErro *e) {
  long n;
  // VOD parada em silencio ou prazo da reconexao: o app sintetiza o texto.
  if (tem(t, "stalled download") || tem(t, "recovery timeout")) {
    e->classe = FC_PARADO; snprintf(e->cod, sizeof e->cod, "m3_parado");
    return 1;
  }
  // EV_AUDIO_SEM_DECODER chega como flag, nao como texto do player; o produtor
  // passa "audio sem decoder". Tambem cobre "audio codec not supported".
  if (tem(t, "no decoder") || tem(t, "nodecoder") ||
      (tem(t, "audio") && (tem(t, "decoder") || tem(t, "codec") ||
                           tem(t, "not supported") || tem(t, "unsupported")))) {
    e->classe = FC_AUDIO; snprintf(e->cod, sizeof e->cod, "m3_audio");
    return 1;
  }
  n = digito(t);
  if (n < 0 && dec > 0) n = dec;
  if (tem(t, "player error") || n >= 2000) {
    if (n == 2000 || n == 2001 || n == 2002 || n == 2008) {
      e->classe = FC_REDE; numero(FP_ANDROID, e, n); return 1;
    }
    if (n >= 2004 && n <= 2006) {
      e->classe = FC_HTTP; numero(FP_ANDROID, e, n);
      e->http = n == 2005 ? 404 : n == 2006 ? 403 : 0;
      return 1;
    }
    if (n >= 3000 && n < 5000) {
      e->classe = FC_FORMATO; numero(FP_ANDROID, e, n); return 1;
    }
  }
  return generico(FP_ANDROID, t, e);
}

// --- .wgt (AVPlay). S.erro do onerror: o NOME do erro, ex. -------------------
// "PLAYER_ERROR_CONNECTION_FAILED".
static int lerWgt(const char *t, int dec, FarolErro *e) {
  const char *q;
  char nome[21];   // "av_" + 20 cabe em FarolErro.cod
  int i;
  (void)dec;
  if (tem(t, "connection_failed") || tem(t, "timed_out") ||
      tem(t, "timeout") || tem(t, "network")) {
    e->classe = FC_REDE; snprintf(e->cod, sizeof e->cod, "av_conexao");
    return 1;
  }
  q = acha(t, "player_error");
  if (q) {
    q += strlen("player_error");
    if (*q == '_') q++;
    for (i = 0; i < (int)sizeof nome - 1 &&
                (isalnum((unsigned char)q[i]) || q[i] == '_'); i++) {
      char c = q[i];
      nome[i] = (char)(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    }
    nome[i] = 0;
    if (i) {
      e->classe = tem(nome, "audio") ? FC_AUDIO : FC_FORMATO;
      snprintf(e->cod, sizeof e->cod, "av_%s", nome);
      return 1;
    }
  }
  return generico(FP_TIZEN_WGT, t, e);
}

// --- .tpk (Tizen.Multimedia.Player do host .NET). ----------------------------
static int lerTpk(const char *t, int dec, FarolErro *e) {
  const char *x = strstr(t, "0x");
  long hex = x ? strtol(x, NULL, 16) : -1;  // positivo; `dec` ja chega wrapado
  if (dec == (int)0xFE6C0026 || hex == 0xFE6C0026 ||
      tem(t, "connectionfailed")) {
    e->classe = FC_REDE; snprintf(e->cod, sizeof e->cod, "tpk_conexao");
    return 1;
  }
  if (dec == (int)0xFE6C002E || hex == 0xFE6C002E ||
      tem(t, "audiocodecnotsupported") || tem(t, "notsupportedaudiocodec")) {
    e->classe = FC_AUDIO; snprintf(e->cod, sizeof e->cod, "tpk_audio");
    return 1;
  }
  if (tem(t, "resourceconflict")) {
    e->classe = FC_SISTEMA; snprintf(e->cod, sizeof e->cod, "tpk_conflito");
    return 1;
  }
  // Relogio congelado 15 s em Playing (TPK_PRESO_MS): recuperarPlayer usa -2.
  if (dec == -2 || tem(t, "preso")) {
    e->classe = FC_PARADO; snprintf(e->cod, sizeof e->cod, "tpk_preso");
    return 1;
  }
  return generico(FP_TIZEN_TPK, t, e);
}

int farolerro_ler(FarolPlat p, const char *texto, int codigoDecoder, FarolErro *e) {
  const char *t = texto ? texto : "";
  long n;
  int ok;
  if (!e) return 0;
  e->classe = FC_NENHUMA;
  e->http = 0;
  e->nativo = codigoDecoder;
  e->cod[0] = 0;
  if (p < FP_WEBOS || p > FP_OUTRA) p = FP_OUTRA;
  // O numero nativo sai do proprio texto quando ele o carrega: inteiro no
  // inicio (uMS, Media3) ou "0x…" no .tpk.
  n = digito(t);
  if (p == FP_TIZEN_TPK) {
    const char *x = strstr(t, "0x");
    if (x) { long h = strtol(x, NULL, 16); if (h) e->nativo = (int)h; }
  } else if (n >= 0) {
    e->nativo = (int)n;
  }
  switch (p) {
    case FP_WEBOS:     ok = lerWebos(t, codigoDecoder, e); break;
    case FP_ANDROID:   ok = lerAndroid(t, codigoDecoder, e); break;
    case FP_TIZEN_WGT: ok = lerWgt(t, codigoDecoder, e); break;
    case FP_TIZEN_TPK: ok = lerTpk(t, codigoDecoder, e); break;
    default:           ok = generico(p, t, e); break;
  }
  if (ok) return 1;
  snprintf(e->cod, sizeof e->cod, "%s_outro", prefixo(p));
  return 0;
}
