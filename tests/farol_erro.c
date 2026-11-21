// Farol: normalizador de erro nativo (farolerro_ler) — tabela de casos reais
// por plataforma (PLANO.md secao 2.3) + cruzamento com fontesonda_causa:
// onde a sonda tem opiniao, o Farol nunca discorda da classe.
#include <stdio.h>
#include <string.h>
#include "farolerro.h"
#include "fontesonda.h"

static int falhas;
#define OK(c) do { if (!(c)) { printf("FALHOU %d: %s\n", __LINE__, #c); falhas++; } } while (0)

typedef struct {
  FarolPlat p; const char *texto; int dec;
  int ret;              // 1 reconheceu, 0 desconhecido
  FarolClasse classe; int http; const char *cod;
} Caso;

// Todo cod emitido esta na lista fechada farolerro_codigos[] (farolerro.h)
// ou nas familias geradas: ums_<digitos>, m3_<digitos>, av_<nome [a-z0-9_]>.
static int cod_valido(const char *s) {
  size_t n = strlen(s), np;
  int i;
  for (i = 0; i < farolerro_codigos_n; i++)
    if (!strcmp(s, farolerro_codigos[i])) return 1;
  if (!strncmp(s, "ums_", 4) || !strncmp(s, "m3_", 3)) {
    np = s[0] == 'u' ? 4 : 3;
    if (n > np && strspn(s + np, "0123456789") == n - np) return 1;
  }
  if (!strncmp(s, "av_", 3) && n > 3 &&
      strspn(s + 3, "abcdefghijklmnopqrstuvwxyz0123456789_") == n - 3)
    return 1;
  return 0;
}

static void caso(const Caso *c) {
  FarolErro e;
  int r = farolerro_ler(c->p, c->texto, c->dec, &e);
  if (r != c->ret || e.classe != c->classe || e.http != c->http ||
      strcmp(e.cod, c->cod))
    printf("FALHOU: \"%s\" plat=%d -> ret=%d classe=%d http=%d cod=%s "
           "(esperava ret=%d classe=%d http=%d cod=%s)\n",
           c->texto ? c->texto : "(null)", c->p, r, e.classe, e.http, e.cod,
           c->ret, c->classe, c->http, c->cod), falhas++;
  if (r && (!e.cod[0] || strlen(e.cod) > 23)) {
    printf("FALHOU: cod fora do teto: %s\n", e.cod); falhas++;
  }
  if (!cod_valido(e.cod)) {
    printf("FALHOU: cod fora da lista fechada: %s\n", e.cod); falhas++;
  }
}

int main(void) {
  static const Caso casos[] = {
    // LG (uMS): "<errorCode> <errorText>"
    {FP_WEBOS, "40403 server error:40403", 0, 1, FC_HTTP, 403, "ums_40403"},
    {FP_WEBOS, "40404 server error:40404", 0, 1, FC_HTTP, 404, "ums_40404"},
    {FP_WEBOS, "40500 server error:40500", 0, 1, FC_HTTP, 500, "ums_40500"},
    {FP_WEBOS, "300 Network Error", 0, 1, FC_REDE, 0, "ums_300"},
    {FP_WEBOS, "200 Audio Codec Not Supported", 0, 1, FC_AUDIO, 0, "ums_200"},
    {FP_WEBOS, NULL, 200, 1, FC_AUDIO, 0, "ums_200"},
    {FP_WEBOS, "202 Media Not Found", 0, 1, FC_FORMATO, 0, "ums_202"},
    {FP_WEBOS, "203 AV Type Not Founded", 0, 1, FC_FORMATO, 0, "ums_203"},
    {FP_WEBOS, "204 Fail To Demultiplex", 0, 1, FC_FORMATO, 0, "ums_204"},
    {FP_WEBOS, "210 Unknown Subtitle", 0, 1, FC_FORMATO, 0, "ums_210"},
    {FP_WEBOS, "100 Playing error", 0, 1, FC_FORMATO, 0, "ums_100"},
    {FP_WEBOS, "0 com.webos.pipeline.3 is not running", 0, 1, FC_SISTEMA, 0, "ums_pipeline"},
    {FP_WEBOS, NULL, -1027, 1, FC_SISTEMA, 0, "ls2_permissao"},
    {FP_WEBOS, "ls2 PERMISSION", 0, 1, FC_SISTEMA, 0, "ls2_permissao"},
    {FP_WEBOS, "network recovery timeout", 0, 1, FC_REDE, 0, "ums_rede"},
    {FP_WEBOS, "Cannot safely identify the selected native audio track", 0,
     1, FC_AUDIO, 0, "ums_faixa_audio"},
    {FP_WEBOS, "700 seek Failure", 0, 0, FC_NENHUMA, 0, "ums_outro"},
    {FP_WEBOS, "999 codigo nao catalogado", 0, 0, FC_NENHUMA, 0, "ums_outro"},
    {FP_WEBOS, NULL, 0, 0, FC_NENHUMA, 0, "ums_outro"},
    // Android (Media3): "%d player error:%d"
    {FP_ANDROID, "2000 player error:2", 0, 1, FC_REDE, 0, "m3_2000"},
    {FP_ANDROID, "2001 player error:2", 0, 1, FC_REDE, 0, "m3_2001"},
    {FP_ANDROID, "2002 player error:1", 0, 1, FC_REDE, 0, "m3_2002"},
    {FP_ANDROID, "2008 player error:2", 0, 1, FC_REDE, 0, "m3_2008"},
    {FP_ANDROID, "2004 player error:1", 0, 1, FC_HTTP, 0, "m3_2004"},
    {FP_ANDROID, "2005 player error:1", 0, 1, FC_HTTP, 404, "m3_2005"},
    {FP_ANDROID, "2006 player error:1", 0, 1, FC_HTTP, 403, "m3_2006"},
    {FP_ANDROID, "2002 player error: stalled download", 0, 1, FC_PARADO, 0, "m3_parado"},
    {FP_ANDROID, "2002 player error: recovery timeout", 0, 1, FC_PARADO, 0, "m3_parado"},
    {FP_ANDROID, "3003 player error:2", 0, 1, FC_FORMATO, 0, "m3_3003"},
    {FP_ANDROID, "4004 player error:2", 0, 1, FC_FORMATO, 0, "m3_4004"},
    {FP_ANDROID, "audio sem decoder", 0, 1, FC_AUDIO, 0, "m3_audio"},
    {FP_ANDROID, "2003 player error:1", 0, 0, FC_NENHUMA, 0, "m3_outro"},
    {FP_ANDROID, "9999 player error:9", 0, 0, FC_NENHUMA, 0, "m3_outro"},
    // .wgt (AVPlay): nome do erro no onerror
    {FP_TIZEN_WGT, "PLAYER_ERROR_CONNECTION_FAILED", 0, 1, FC_REDE, 0, "av_conexao"},
    {FP_TIZEN_WGT, "PLAYER_ERROR_NETWORK_TIMEOUT", 0, 1, FC_REDE, 0, "av_conexao"},
    {FP_TIZEN_WGT, "PLAYER_ERROR_TIMED_OUT", 0, 1, FC_REDE, 0, "av_conexao"},
    {FP_TIZEN_WGT, "network recovery timeout", 0, 1, FC_REDE, 0, "av_conexao"},
    {FP_TIZEN_WGT, "PLAYER_ERROR_INVALID_URI", 0, 1, FC_FORMATO, 0, "av_invalid_uri"},
    {FP_TIZEN_WGT, "PLAYER_ERROR_INVALID_STATE", 0, 1, FC_FORMATO, 0, "av_invalid_state"},
    {FP_TIZEN_WGT, "PLAYER_ERROR_SEEK_FAILED", 0, 1, FC_FORMATO, 0, "av_seek_failed"},
    {FP_TIZEN_WGT, "PLAYER_ERROR_NOT_SUPPORTED_FORMAT", 0, 1, FC_FORMATO, 0,
     "av_not_supported_format"},
    {FP_TIZEN_WGT, "open: unreachable", 0, 1, FC_REDE, 0, "av_conexao"},
    {FP_TIZEN_WGT, "prepare: algo sem nome", 0, 0, FC_NENHUMA, 0, "av_outro"},
    // .tpk (host .NET): codigo CAPI ou linha "erro <Enum>"
    {FP_TIZEN_TPK, "[host] erro ConnectionFailed", 0, 1, FC_REDE, 0, "tpk_conexao"},
    {FP_TIZEN_TPK, "Samsung player error 0xfe6c0026", 0, 1, FC_REDE, 0, "tpk_conexao"},
    {FP_TIZEN_TPK, "Samsung player error 0xfe6c002e", 0, 1, FC_AUDIO, 0, "tpk_audio"},
    {FP_TIZEN_TPK, "Samsung player error 0XFE6C002E", 0, 1, FC_AUDIO, 0, "tpk_audio"},
    {FP_TIZEN_TPK, "Samsung player error 0xfe6c002e", (int)0xFE6C002E,
     1, FC_AUDIO, 0, "tpk_audio"},
    {FP_TIZEN_TPK, "erro NotSupportedAudioCodec", 0, 1, FC_AUDIO, 0, "tpk_audio"},
    {FP_TIZEN_TPK, "erro AudioCodecNotSupported", 0, 1, FC_AUDIO, 0, "tpk_audio"},
    {FP_TIZEN_TPK, "interrompido: ResourceConflict", 0, 1, FC_SISTEMA, 0, "tpk_conflito"},
    {FP_TIZEN_TPK, NULL, -2, 1, FC_PARADO, 0, "tpk_preso"},
    {FP_TIZEN_TPK, "tpk preso: relogio sem andar", 0, 1, FC_PARADO, 0, "tpk_preso"},
    {FP_TIZEN_TPK, "Samsung player error 0xfe6c00ff", 0, 0, FC_NENHUMA, 0, "tpk_outro"},
    {FP_TIZEN_TPK, NULL, (int)0xFE6C0026, 1, FC_REDE, 0, "tpk_conexao"},
    // Plataforma sem backend proprio: so o generico
    {FP_OUTRA, "Response HTTP 410 Gone", 0, 1, FC_HTTP, 410, "gen_http"},
    {FP_OUTRA, "texto livre qualquer", 0, 0, FC_NENHUMA, 0, "gen_outro"},
  };
  // As MESMAS entradas de tests/fontesonda_causa.c, em todas as plataformas:
  // onde a sonda tem opiniao (classe != NENHUMA) o Farol nunca discorda.
  // NENHUMA = "sem opiniao": o Farol pode classificar a mais (ex.: uMS 100).
  static const char *entradas[] = {
    "40404 server error:40404", "40403 server error:40403",
    "40500 server error:40500", "Response HTTP 410 Gone",
    "2004 player error:1", "2005 player error:1", "2002 player error:1",
    "[host] erro ConnectionFailed", "open: unreachable",
    "3003 player error:2", "4004 player error:2",
    "decoder: unsupported codec", "100 Playing error", "",
  };
  unsigned i;
  for (i = 0; i < sizeof casos / sizeof casos[0]; i++) caso(&casos[i]);
  for (i = 0; i < sizeof entradas / sizeof entradas[0]; i++) {
    int hfs = 0, fs = fontesonda_causa(entradas[i], &hfs), p;
    FarolClasse espera = fs == FS_CAUSA_HTTP ? FC_HTTP
                       : fs == FS_CAUSA_REDE ? FC_REDE
                       : fs == FS_CAUSA_FORMATO ? FC_FORMATO : FC_NENHUMA;
    for (p = FP_WEBOS; p <= FP_OUTRA; p++) {
      FarolErro e;
      farolerro_ler((FarolPlat)p, entradas[i], 0, &e);
      if (espera != FC_NENHUMA && e.classe != espera) {
        printf("FALHOU cruzamento: \"%s\" plat=%d -> classe=%d (sonda=%d)\n",
               entradas[i], p, e.classe, espera);
        falhas++;
      }
      if (espera != FC_NENHUMA && e.classe == FC_HTTP && e.http != hfs) {
        printf("FALHOU cruzamento http: \"%s\" plat=%d -> %d (sonda=%d)\n",
               entradas[i], p, e.http, hfs);
        falhas++;
      }
      if (!cod_valido(e.cod)) {
        printf("FALHOU cruzamento cod fora da lista: \"%s\" plat=%d -> %s\n",
               entradas[i], p, e.cod);
        falhas++;
      }
    }
  }
  // nativo sai do proprio texto; sem texto, fica o codigoDecoder. No .tpk os
  // hex CAPI 0xFE6C00xx tem o bit 31 ativo: em ILP32 (ARM32 do .tpk real)
  // strtol saturaria em LONG_MAX — por isso o parse e strtoul/uint32_t.
  { FarolErro e;
    farolerro_ler(FP_WEBOS, "40403 server error:40403", 0, &e);
    OK(e.nativo == 40403);
    farolerro_ler(FP_TIZEN_TPK, "Samsung player error 0xfe6c0026", 0, &e);
    OK(e.nativo == (int)0xFE6C0026);
    farolerro_ler(FP_TIZEN_TPK, "Samsung player error 0xfe6c002e", 0, &e);
    OK(e.nativo == (int)0xFE6C002E);
    farolerro_ler(FP_TIZEN_TPK, "Samsung player error 0xfe6c00ff", 0, &e);
    OK(e.nativo == (int)0xFE6C00FF);
    farolerro_ler(FP_TIZEN_TPK, "Samsung player error 0XFE6C002E", 0, &e);
    OK(e.nativo == (int)0xFE6C002E);
    farolerro_ler(FP_TIZEN_TPK, "Samsung player error 0xfe6c002e",
                  (int)0xFE6C002E, &e);
    OK(e.nativo == (int)0xFE6C002E);
    // "0x" sem digito nao toca no nativo; codigoDecoder valido nao e
    // sobrescrito nem quando o texto traz outro hex.
    farolerro_ler(FP_TIZEN_TPK, "erro 0x", (int)0xFE6C0026, &e);
    OK(e.nativo == (int)0xFE6C0026);
    farolerro_ler(FP_TIZEN_TPK, "Samsung player error 0xfe6c0026",
                  (int)0xFE6C002E, &e);
    OK(e.nativo == (int)0xFE6C002E);
    farolerro_ler(FP_WEBOS, "700 seek Failure", 0, &e);
    OK(e.nativo == 700);
    farolerro_ler(FP_WEBOS, NULL, -1027, &e);
    OK(e.nativo == -1027);
    OK(farolerro_ler(FP_WEBOS, "40404 server error", 0, NULL) == 0);
    OK(!strcmp(farolerro_classe_nome(FC_AUDIO), "audio"));
    OK(!strcmp(farolerro_prefixo(FP_ANDROID), "m3"));
    // Lista fechada exportada para o redator do Farol 4: n tokens validos.
    OK(farolerro_codigos_n == 28);
    for (i = 0; i < (unsigned)farolerro_codigos_n; i++)
      OK(farolerro_codigos[i] && farolerro_codigos[i][0] &&
         strlen(farolerro_codigos[i]) <= 23);
  }
  if (!falhas) puts("ok farol_erro");
  return falhas != 0;
}
