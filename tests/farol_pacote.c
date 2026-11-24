// Testes do pacote FarolRegistro v1 (issue 11, PLANO.md §2.6):
//   - JSON de ouro byte a byte contra tests/farol_registro_v1.json (trocando
//     apenas o fp ilustrativo pelo SHA-256 real do fingerprint);
//   - teto <= 35 000 B com anel cheio (corta a `linha` do mais antigo e mantem
//     os mais recentes em ordem + `privacidade`), buffer apertado e buffer curto;
//   - janela de 5 min com virada de 32 bits;
//   - fingerprint estavel para os mesmos fatos e diferente ao mudar plataforma,
//     SO maior, backend, erro nativo ou codec;
//   - concorrencia (4 fios gravando no anel/perfil enquanto monta pacotes);
//   - modos auxiliares `--emitir-contrato` e `--regcodigo <id>` para o script
//     provar o contrato fim-a-fim TV <-> servidor <-> tools/farol-ler.mjs.
#define _POSIX_C_SOURCE 200809L
#include "farolpacote.h"
#include "regcodigo.h"
#include "sha256.h"
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t g_relogio_ms;
static uint32_t relogio_falso(void) { return g_relogio_ms; }

/* Parser JSON independente para garantir saida sempre valida. */
static const char *p_str(const char *p) {
  assert(*p++ == '"');
  while (*p && *p != '"') {
    assert((unsigned char)*p >= 32 && *p != '\\');
    p++;
  }
  assert(*p++ == '"');
  return p;
}

static const char *p_val(const char *p, int prof) {
  assert(prof < 10);
  if (*p == '{' || *p == '[') {
    int arr = (*p++ == '[');
    char fim = arr ? ']' : '}';
    if (*p == fim) return p + 1;
    for (;;) {
      if (!arr) {
        p = p_str(p);
        assert(*p++ == ':');
      }
      p = p_val(p, prof + 1);
      if (*p != ',') break;
      p++;
    }
    assert(*p++ == fim);
    return p;
  }
  if (*p == '"') return p_str(p);
  if (!strncmp(p, "true", 4)) return p + 4;
  if (!strncmp(p, "false", 5)) return p + 5;
  if (*p == '-') p++;
  assert(isdigit((unsigned char)*p));
  while (isdigit((unsigned char)*p)) p++;
  if (*p == '.') {
    p++;
    assert(isdigit((unsigned char)*p));
    while (isdigit((unsigned char)*p)) p++;
  }
  return p;
}

static void assert_json_valido(const char *s) {
  assert(s && *s == '{');
  assert(*p_val(s, 0) == '\0');
}

static void sha256_16(const char *s, char out[17]) {
  Sha sh;
  unsigned char d[32];
  char hex64[65];
  sha_ini(&sh);
  sha_add(&sh, (const unsigned char *)s, (unsigned long)strlen(s));
  sha_fim(&sh, d);
  hex(d, 32, hex64);
  memcpy(out, hex64, 16);
  out[16] = '\0';
}

static void montar_cenario_dourado(FarolPerfil *perfil, FarolPacoteEntrada *in,
                                   FarolPacoteTentativa tentou[2]) {
  farolperfil_padrao(perfil);
  strcpy(perfil->aparelho.plataforma, "webos");
  strcpy(perfil->aparelho.so, "4.9");
  perfil->aparelho.so_maior = 4;
  strcpy(perfil->aparelho.modelo, "OLED55C9PSA");
  perfil->aparelho.ram_total_mb = 1870;
  perfil->aparelho.ram_livre_mb = 212;
  perfil->aparelho.tela = (FarolPerfilTela){ 3840, 2160, 1, 1, 1, -1 };
  perfil->aparelho.audio = (FarolPerfilAudio){ 1, -1, 1, 1 };
  perfil->aparelho.rede = (FarolPerfilRede){ 0, 0, 0.0 };

  strcpy(perfil->cliente.versao, "2.1.0");
  strcpy(perfil->cliente.commit, "c18c21756");
  strcpy(perfil->cliente.build, "webos-ipk");
  strcpy(perfil->cliente.backend, "starfish");
  strcpy(perfil->cliente.idioma, "pt-BR");
  perfil->cliente.aberto_s = 1842;
  strcpy(perfil->cliente.gpu, "auto:alto");
  perfil->cliente.ajustes.glass_ui = 1;
  strcpy(perfil->cliente.ajustes.qualidade_max, "4k");
  perfil->cliente.ajustes.tamanho_max_gb = 40.0;
  strcpy(perfil->cliente.ajustes.dts, "ac3");
  perfil->cliente.ajustes.dv_mkv = 0;
  perfil->cliente.ajustes.legenda_auto_sync = 1;
  perfil->cliente.addons = (FarolPerfilAddons){
    12, 11, 1, FAROL_ADDON_CATALOGO | FAROL_ADDON_FONTES | FAROL_ADDON_LEGENDAS
  };
  perfil->cliente.debrid_mask = FAROL_DEBRID_TORBOX;
  perfil->cliente.contas_mask = FAROL_CONTA_TRAKT | FAROL_CONTA_SIMKL;

  tentou[0] = (FarolPacoteTentativa){ "proxima_fonte", 0, 6400 };
  tentou[1] = (FarolPacoteTentativa){ "reconexao", 0, 13000 };

  farolpacote_entrada_padrao(in);
  in->gatilho = FAROL_G_FALHA_SEM_CONSERTO;
  in->sintoma = FAROL_PS_NAO_COMECA;
  in->codigo = "TV_FORMATO_REPETE";
  in->estagio = RES_T_TV;
  in->erro.classe = FC_FORMATO;
  in->erro.nativo = 204;
  in->erro.http = 0;
  strcpy(in->erro.cod, "ums_204");
  in->tentou = tentou;
  in->n_tentou = 2;
  in->perfil = perfil;

  in->midia.container = "mkv";
  in->midia.video = "hevc10";
  in->midia.hdr = "dv";
  in->midia.res = "2160p";
  in->midia.fps = 23.976;
  in->midia.mbps = 24.6;
  in->midia.gb = 14.2;
  in->midia.audio = "eac3_51";
  in->midia.legenda = "srt_embutida";
  in->midia.entrega = "debrid_cache";
  in->midia.http = 206;
  in->midia.ttfb_ms = 210;

  in->tem_cadeia = 1;
  in->cadeia[RES_T_INTERNET] = (FarolPacotePasso){ FAROL_EST_OK, -1, -1 };
  in->cadeia[RES_T_ADDONS]   = (FarolPacotePasso){ FAROL_EST_OK, 1920, -1 };
  in->cadeia[RES_T_DEBRID]   = (FarolPacotePasso){ FAROL_EST_OK, -1, -1 };
  in->cadeia[RES_T_FONTE]    = (FarolPacotePasso){ FAROL_EST_OK, -1, 206 };
  in->cadeia[RES_T_TV]       = (FarolPacotePasso){ FAROL_EST_FALHOU, -1, -1 };
  in->cadeia[RES_T_AUDIO]    = (FarolPacotePasso){ FAROL_EST_NAO_TESTADO, -1, -1 };

  farolanel_zerar();
  farolanel_relogio(relogio_falso);
  g_relogio_ms = 5180;
  farol_ev(FM_FONTE, FE_FONTE_ESCOLHIDA, 0, 9120, 142, "2160p_hdr_dv_eac3");
  g_relogio_ms = 6390;
  farol_ev(FM_PLAYER, FE_ABRIR, 0, 1, 3, "starfish");
  g_relogio_ms = 8050;
  farol_ev(FM_VIDEO, FE_ERRO_NATIVO, 2, 204, 0, "ums_204");

  in->eventos = NULL; /* le do anel real */
  in->agora_ms = 10000;
  in->janela_ms = FAROL_PACOTE_JANELA_MS;
}

static void testar_dourado(void) {
  FarolPerfil perfil;
  FarolPacoteEntrada in;
  FarolPacoteTentativa tentou[2];
  FarolPacoteRes res;
  char buf[FAROL_JSON_N], esperado[FAROL_JSON_N + 64], fp_real[17];
  FILE *f;
  size_t n;
  char *pos;

  montar_cenario_dourado(&perfil, &in, tentou);
  assert(farolpacote_montar(&in, buf, sizeof buf, &res) == 1);
  assert_json_valido(buf);
  assert(res.bytes == strlen(buf) && res.bytes <= FAROL_JSON_MAX);
  assert(res.campos_recusados == 0);
  assert(res.linha_total == 3 && res.linha_escritos == 3 && res.linha_cortados == 0);
  assert(strcmp(res.fingerprint, "webos-4-starfish-ums_204-mkv-hevc10-dv-eac3") == 0);

  sha256_16(res.fingerprint, fp_real);
  assert(strcmp(res.fp, fp_real) == 0);

  /* Compara byte a byte com tests/farol_registro_v1.json, trocando apenas o
   * fp ilustrativo do plano ("9c1e77ab03f2d4e1") pelo SHA-256 real. */
  f = fopen("tests/farol_registro_v1.json", "rb");
  assert(f != NULL);
  n = fread(esperado, 1, sizeof esperado - 1, f);
  fclose(f);
  esperado[n] = '\0';
  while (n > 0 && (esperado[n - 1] == '\n' || esperado[n - 1] == '\r'))
    esperado[--n] = '\0';
  pos = strstr(esperado, "\"fp\":\"9c1e77ab03f2d4e1\"");
  assert(pos != NULL);
  memcpy(pos + 6, fp_real, 16);
  assert(strcmp(buf, esperado) == 0);
}

/* Extrai os valores de `b` de cada item do array `"linha":[...]` na ordem emitida. */
static int extrair_bs_linha(const char *json, int out_b[FAROL_ANEL_N]) {
  const char *p = strstr(json, "\"linha\":[");
  int n = 0;
  assert(p != NULL);
  p += strlen("\"linha\":[");
  if (*p == ']') return 0;
  while (*p && *p != ']') {
    const char *fb = strstr(p, "\"b\":");
    const char *fim_obj = strchr(p, '}');
    assert(fb && fim_obj && fb < fim_obj);
    assert(n < FAROL_ANEL_N);
    out_b[n++] = (int)strtol(fb + 4, NULL, 10);
    p = fim_obj + 1;
    if (*p == ',') p++;
  }
  return n;
}

static void testar_teto_e_corte(void) {
  FarolPerfil perfil;
  FarolPacoteEntrada in;
  FarolPacoteTentativa tentou64[FAROL_PACOTE_TENTOU_MAX];
  FarolPacoteRes res;
  char buf[FAROL_JSON_N];
  int bs[FAROL_ANEL_N];
  int i, nb;

  montar_cenario_dourado(&perfil, &in, tentou64);

  /* Caso A: 256 eventos tipicos cabem inteiros dentro de 35 000 B. */
  farolanel_zerar();
  for (i = 0; i < FAROL_ANEL_N; i++) {
    g_relogio_ms = 100000u + (uint32_t)i * 10u;
    farol_ev(FM_VIDEO, FE_ERRO_NATIVO, 2, 204, i, "ums_204");
  }
  in.agora_ms = 100000u + 256u * 10u;
  assert(farolpacote_montar(&in, buf, sizeof buf, &res) == 1);
  assert_json_valido(buf);
  assert(res.bytes <= FAROL_JSON_MAX);
  assert(res.linha_total == FAROL_ANEL_N);
  assert(res.linha_escritos == FAROL_ANEL_N && res.linha_cortados == 0);
  assert(res.campos_recusados == 0);

  /* Caso B: pior caso (64 tentou + campos mais largos + 256 eventos longos) ->
   * ultrapassa 35 000 B sem corte; o montador corta os MAIS ANTIGOS no teto
   * real FAROL_JSON_MAX (35 000 B) e preserva os mais recentes. */
  strcpy(perfil.aparelho.plataforma, "tizen_tpk");
  strcpy(perfil.aparelho.so, "9999.9999.9999");
  perfil.aparelho.so_maior = 99;
  strcpy(perfil.aparelho.modelo, "UN55AB1234ABCDEF");
  perfil.aparelho.ram_total_mb = 2000000000;
  perfil.aparelho.ram_livre_mb = 2000000000;
  perfil.aparelho.tela = (FarolPerfilTela){ 2000000000, 2000000000, -1, -1, -1, -1 };
  perfil.aparelho.audio = (FarolPerfilAudio){ -1, -1, -1, -1 };
  perfil.aparelho.rede = (FarolPerfilRede){ 0, 2000000000, 999999999.9 };
  strcpy(perfil.cliente.versao, "9999.9999.9999");
  strcpy(perfil.cliente.commit, "abcdef123456");
  strcpy(perfil.cliente.build, "tizen-tpk65");
  strcpy(perfil.cliente.backend, "starfish");
  perfil.cliente.aberto_s = 2000000000;
  strcpy(perfil.cliente.gpu, "desconhecido");
  strcpy(perfil.cliente.ajustes.qualidade_max, "1080p");
  perfil.cliente.ajustes.tamanho_max_gb = 999999999.9;
  strcpy(perfil.cliente.ajustes.dts, "desligado");
  perfil.cliente.addons = (FarolPerfilAddons){
    2000000000, 2000000000, 2000000000,
    FAROL_ADDON_CATALOGO | FAROL_ADDON_FONTES | FAROL_ADDON_LEGENDAS | FAROL_ADDON_META
  };
  perfil.cliente.debrid_mask =
    FAROL_DEBRID_TORBOX | FAROL_DEBRID_REALDEBRID | FAROL_DEBRID_ALLDEBRID |
    FAROL_DEBRID_PREMIUMIZE | FAROL_DEBRID_DEBRIDLINK | FAROL_DEBRID_EASYDEBRID;
  perfil.cliente.contas_mask = FAROL_CONTA_TRAKT | FAROL_CONTA_SIMKL;
  in.codigo = "AUDIO_DTS_SEM_SAIDA";
  strcpy(in.erro.cod, "av_not_supported_format");
  in.erro.nativo = -2000000000;
  in.erro.http = -2000000000;
  in.midia.container = "outro";
  in.midia.video = "mpeg2";
  in.midia.hdr = "hdr10plus";
  in.midia.res = "2160p";
  in.midia.fps = 999999999.9;
  in.midia.mbps = 999999999.9;
  in.midia.gb = 999999999.9;
  in.midia.audio = "truehd_atmos";
  in.midia.legenda = "srt_embutida";
  in.midia.entrega = "debrid_cache";
  in.midia.http = 2000000000;
  in.midia.ttfb_ms = 2000000000;
  for (i = 0; i < RES_T_N; i++)
    in.cadeia[i] = (FarolPacotePasso){ FAROL_EST_NAO_TESTADO, 2000000000 + i, 2000000000 + i };
  for (i = 0; i < FAROL_PACOTE_TENTOU_MAX; i++) {
    tentou64[i] = (FarolPacoteTentativa){ "aliviar_memoria", 0, -2000000000 - i };
  }
  in.tentou = tentou64;
  in.n_tentou = FAROL_PACOTE_TENTOU_MAX;

  farolanel_zerar();
  for (i = 0; i < FAROL_ANEL_N; i++) {
    g_relogio_ms = 1000u + (uint32_t)i;
    farol_ev(FM_MEMORIA, FE_FAROL_RESULTADO, 2,
             -2000000000 - i, -2000000000 - i, "av_not_supported_format");
  }
  in.agora_ms = 2000001000u; /* t ~ -2000000000 (11 digitos com sinal) */
  in.janela_ms = 0;          /* sem filtro de janela para encher os 256 */
  assert(farolpacote_montar(&in, buf, sizeof buf, &res) == 1);
  assert_json_valido(buf);
  assert(res.bytes == strlen(buf) && res.bytes <= FAROL_JSON_MAX);
  assert(res.campos_recusados == 0);
  assert(res.linha_total == FAROL_ANEL_N);
  assert(res.linha_cortados > 0);
  assert(res.linha_escritos > 0);
  assert(res.linha_escritos + res.linha_cortados == FAROL_ANEL_N);
  assert(strstr(buf, "\"privacidade\":{\"redator\":1,\"campos_recusados\":0}") != NULL);

  nb = extrair_bs_linha(buf, bs);
  assert(nb == res.linha_escritos);
  assert(bs[0] == -2000000000 - res.linha_cortados);
  assert(bs[nb - 1] == -2000000000 - (FAROL_ANEL_N - 1));
  for (i = 1; i < nb; i++) assert(bs[i] == bs[i - 1] - 1);

  /* Caso C: buffer apertado com sentinelas apos `tam` -> corta mais antigos,
   * nunca escreve fora do buffer, JSON valido com privacidade. */
  {
    char guarda[5000];
    size_t tams[] = { 2600, 4096 };
    size_t k;
    in.n_tentou = 2;
    for (k = 0; k < sizeof tams / sizeof *tams; k++) {
      memset(guarda, '!', sizeof guarda);
      assert(farolpacote_montar(&in, guarda, tams[k], &res) == 1);
      assert_json_valido(guarda);
      assert(res.bytes < tams[k]);
      assert(res.linha_cortados > 0);
      assert(guarda[tams[k]] == '!');
      assert(strstr(guarda, "\"privacidade\":{\"redator\":1,\"campos_recusados\":0}") != NULL);
      nb = extrair_bs_linha(guarda, bs);
      assert(nb == res.linha_escritos);
      if (nb > 0) {
        assert(bs[0] == -2000000000 - res.linha_cortados);
        assert(bs[nb - 1] == -2000000000 - (FAROL_ANEL_N - 1));
      }
    }
  }

  /* Caso D: buffer pequeno demais ou argumentos nulos -> devolve 0 sem estourar. */
  {
    char mini[128];
    memset(mini, '!', sizeof mini);
    assert(farolpacote_montar(&in, mini, 100, &res) == 0);
    assert(mini[100] == '!');
    assert(res.bytes == 0);
    assert(farolpacote_montar(NULL, mini, sizeof mini, &res) == 0);
    assert(farolpacote_montar(&in, NULL, sizeof mini, &res) == 0);
    assert(farolpacote_montar(&in, mini, 2, &res) == 0);
  }

  /* Caso E: janela de 5 min (300 000 ms) com virada de relogio 32 bits. */
  farolanel_zerar();
  g_relogio_ms = 0xFFFF0000u - 400000u; /* > 5 min atras: deve sair */
  farol_ev(FM_PLAYER, FE_ABRIR, 0, 1, 10, "starfish");
  g_relogio_ms = 0xFFFFFF80u;           /* -144 ms antes de 0x00000010u: fica */
  farol_ev(FM_VIDEO, FE_ERRO_NATIVO, 2, 204, 20, "ums_204");
  g_relogio_ms = 0x00000008u;           /* -8 ms antes de 0x00000010u: fica */
  farol_ev(FM_VIDEO, FE_ERRO_NATIVO, 2, 204, 30, "ums_204");
  in.agora_ms = 0x00000010u;
  in.janela_ms = FAROL_PACOTE_JANELA_MS;
  assert(farolpacote_montar(&in, buf, sizeof buf, &res) == 1);
  assert_json_valido(buf);
  assert(res.linha_total == 2 && res.linha_escritos == 2 && res.linha_cortados == 0);
  nb = extrair_bs_linha(buf, bs);
  assert(nb == 2 && bs[0] == 20 && bs[1] == 30);
  assert(strstr(buf, "\"t\":-144") != NULL);
  assert(strstr(buf, "\"t\":-8") != NULL);
}

static void testar_fingerprint(void) {
  FarolPerfil p0, p1;
  FarolPacoteEntrada in0, in1;
  FarolPacoteTentativa t[2];
  char fp0[FAROL_PACOTE_FP_MAX], h0[FAROL_PACOTE_FP_HEX_N];
  char fp1[FAROL_PACOTE_FP_MAX], h1[FAROL_PACOTE_FP_HEX_N];

  montar_cenario_dourado(&p0, &in0, t);
  assert(farolpacote_fingerprint(&p0, &in0.erro, &in0.midia, fp0, h0) == 1);
  assert(strcmp(fp0, "webos-4-starfish-ums_204-mkv-hevc10-dv-eac3") == 0);
  assert(strlen(h0) == 16);

  /* Estabilidade: mesmos fatos (mudando RAM, modelo, aberto_s, ttfb) -> IGUAL. */
  p1 = p0; in1 = in0;
  p1.aparelho.ram_livre_mb = 999;
  strcpy(p1.aparelho.modelo, "OLED65G4PSA");
  p1.cliente.aberto_s = 10;
  in1.midia.ttfb_ms = 999;
  assert(farolpacote_fingerprint(&p1, &in1.erro, &in1.midia, fp1, h1) == 1);
  assert(strcmp(fp0, fp1) == 0 && strcmp(h0, h1) == 0);

  /* Audio com variantes de canal normaliza para a mesma familia no fingerprint. */
  in1.midia.audio = "eac3_atmos";
  assert(farolpacote_fingerprint(&p0, &in1.erro, &in1.midia, fp1, h1) == 1);
  assert(strcmp(fp0, fp1) == 0 && strcmp(h0, h1) == 0);

  /* Muda plataforma -> diferente. */
  p1 = p0; strcpy(p1.aparelho.plataforma, "android");
  assert(farolpacote_fingerprint(&p1, &in0.erro, &in0.midia, fp1, h1) == 1);
  assert(strcmp(fp0, fp1) != 0 && strcmp(h0, h1) != 0);

  /* Muda SO maior -> diferente. */
  p1 = p0; p1.aparelho.so_maior = 5;
  assert(farolpacote_fingerprint(&p1, &in0.erro, &in0.midia, fp1, h1) == 1);
  assert(strcmp(fp0, fp1) != 0 && strcmp(h0, h1) != 0);

  /* Muda backend -> diferente. */
  p1 = p0; strcpy(p1.cliente.backend, "media3");
  assert(farolpacote_fingerprint(&p1, &in0.erro, &in0.midia, fp1, h1) == 1);
  assert(strcmp(fp0, fp1) != 0 && strcmp(h0, h1) != 0);

  /* Muda erro nativo -> diferente. */
  in1 = in0; strcpy(in1.erro.cod, "ums_202");
  assert(farolpacote_fingerprint(&p0, &in1.erro, &in0.midia, fp1, h1) == 1);
  assert(strcmp(fp0, fp1) != 0 && strcmp(h0, h1) != 0);

  /* Muda container -> diferente. */
  in1 = in0; in1.midia.container = "mp4";
  assert(farolpacote_fingerprint(&p0, &in0.erro, &in1.midia, fp1, h1) == 1);
  assert(strcmp(fp0, fp1) != 0 && strcmp(h0, h1) != 0);

  /* Muda codec de video -> diferente. */
  in1 = in0; in1.midia.video = "h264";
  assert(farolpacote_fingerprint(&p0, &in0.erro, &in1.midia, fp1, h1) == 1);
  assert(strcmp(fp0, fp1) != 0 && strcmp(h0, h1) != 0);

  /* Muda HDR -> diferente. */
  in1 = in0; in1.midia.hdr = "hdr10";
  assert(farolpacote_fingerprint(&p0, &in0.erro, &in1.midia, fp1, h1) == 1);
  assert(strcmp(fp0, fp1) != 0 && strcmp(h0, h1) != 0);

  /* Muda codec de audio -> diferente. */
  in1 = in0; in1.midia.audio = "dts_51";
  assert(farolpacote_fingerprint(&p0, &in0.erro, &in1.midia, fp1, h1) == 1);
  assert(strcmp(fp0, fp1) != 0 && strcmp(h0, h1) != 0);

  /* Desconhecidos / adversariais -> fingerprint fechado e seguro. */
  p1 = p0;
  strcpy(p1.aparelho.plataforma, "CANARIO");
  p1.aparelho.so_maior = -1;
  strcpy(p1.cliente.backend, "CANARIO");
  in1 = in0;
  strcpy(in1.erro.cod, "av_canario_privado");
  in1.midia.container = "CANARIO";
  in1.midia.video = NULL;
  in1.midia.hdr = "";
  in1.midia.audio = "CANARIO";
  assert(farolpacote_fingerprint(&p1, &in1.erro, &in1.midia, fp1, h1) == 1);
  assert(strcmp(fp1, "outra-0-outro-x-x-x-x-x") == 0);
  assert(strlen(h1) == 16);
  assert(farolpacote_fingerprint(NULL, NULL, NULL, fp1, h1) == 1);
  assert(strcmp(fp1, "outra-0-outro-x-x-x-x-x") == 0);
}

static void testar_minimo_e_perfil_null(void) {
  FarolPacoteEntrada in;
  FarolPacoteRes res;
  char buf[FAROL_JSON_N];

  farolanel_zerar();
  farolpacote_entrada_padrao(&in);
  in.perfil = NULL; /* exercita farolperfil_obter() */
  assert(farolpacote_montar(&in, buf, sizeof buf, &res) == 1);
  assert_json_valido(buf);
  assert(res.bytes <= FAROL_JSON_MAX);
  assert(res.campos_recusados == 0);
  assert(strstr(buf, "\"codigo\":\"NENHUM\"") != NULL);
  assert(farolpacote_sintoma_de_resolver(RES_S_NAO_COMECA) == FAROL_PS_NAO_COMECA);
  assert(farolpacote_sintoma_de_resolver(RES_S_TRAVA) == FAROL_PS_TRAVA);
  assert(farolpacote_sintoma_de_resolver(RES_S_SOM) == FAROL_PS_SEM_SOM);
  assert(farolpacote_sintoma_de_resolver(RES_S_LEGENDA) == FAROL_PS_LEGENDA);
  assert(farolpacote_sintoma_de_resolver(RES_S_GERAL) == FAROL_PS_OUTRO);
  farolperfil_encerrar();
}

static void *fio_produtor(void *arg) {
  int id = (int)(intptr_t)arg;
  int i;
  for (i = 0; i < 4000; i++) {
    farol_ev(FM_VIDEO, FE_ERRO_NATIVO, 2, 204, id * 10000 + i, "ums_204");
    if ((i & 63) == 0) farolperfil_definir_modelo("OLED55C9PSA");
  }
  return NULL;
}

static void testar_concorrencia(void) {
  pthread_t th[4];
  FarolPacoteEntrada in;
  FarolPacoteRes res;
  char buf[FAROL_JSON_N];
  int i;

  farolanel_zerar();
  farolanel_relogio(NULL);
  farolperfil_iniciar();
  for (i = 0; i < 4; i++)
    assert(pthread_create(&th[i], NULL, fio_produtor, (void *)(intptr_t)i) == 0);

  farolpacote_entrada_padrao(&in);
  in.perfil = NULL;
  in.janela_ms = 0;
  for (i = 0; i < 60; i++) {
    assert(farolpacote_montar(&in, buf, sizeof buf, &res) == 1);
    assert_json_valido(buf);
    assert(res.bytes <= FAROL_JSON_MAX);
  }

  for (i = 0; i < 4; i++) assert(pthread_join(th[i], NULL) == 0);
  farolperfil_encerrar();
}

static int emitir_contrato(void) {
  FarolPerfil perfil;
  FarolPacoteEntrada in;
  FarolPacoteTentativa tentou[FAROL_PACOTE_TENTOU_MAX];
  FarolPacoteRes res;
  char buf[FAROL_JSON_N];
  int i;

  /* 1. Pacote dourado completo */
  montar_cenario_dourado(&perfil, &in, tentou);
  assert(farolpacote_montar(&in, buf, sizeof buf, &res) == 1);
  puts(buf);

  /* 2. Pacote com anel cheio + corte pelo teto (outro fingerprint para nao deduplicar) */
  strcpy(perfil.aparelho.plataforma, "tizen_tpk");
  strcpy(perfil.aparelho.so, "9999.9999.9999");
  perfil.aparelho.so_maior = 99;
  strcpy(perfil.aparelho.modelo, "UN55AB1234ABCDEF");
  perfil.aparelho.ram_total_mb = 2000000000;
  perfil.aparelho.ram_livre_mb = 2000000000;
  perfil.aparelho.tela = (FarolPerfilTela){ 2000000000, 2000000000, -1, -1, -1, -1 };
  perfil.aparelho.audio = (FarolPerfilAudio){ -1, -1, -1, -1 };
  perfil.aparelho.rede = (FarolPerfilRede){ 0, 2000000000, 999999999.9 };
  strcpy(perfil.cliente.versao, "9999.9999.9999");
  strcpy(perfil.cliente.commit, "abcdef123456");
  strcpy(perfil.cliente.build, "tizen-tpk65");
  strcpy(perfil.cliente.backend, "starfish");
  perfil.cliente.aberto_s = 2000000000;
  strcpy(perfil.cliente.gpu, "desconhecido");
  strcpy(perfil.cliente.ajustes.qualidade_max, "1080p");
  perfil.cliente.ajustes.tamanho_max_gb = 999999999.9;
  strcpy(perfil.cliente.ajustes.dts, "desligado");
  perfil.cliente.addons = (FarolPerfilAddons){
    2000000000, 2000000000, 2000000000,
    FAROL_ADDON_CATALOGO | FAROL_ADDON_FONTES | FAROL_ADDON_LEGENDAS | FAROL_ADDON_META
  };
  perfil.cliente.debrid_mask =
    FAROL_DEBRID_TORBOX | FAROL_DEBRID_REALDEBRID | FAROL_DEBRID_ALLDEBRID |
    FAROL_DEBRID_PREMIUMIZE | FAROL_DEBRID_DEBRIDLINK | FAROL_DEBRID_EASYDEBRID;
  in.codigo = "AUDIO_DTS_SEM_SAIDA";
  strcpy(in.erro.cod, "av_not_supported_format");
  in.erro.nativo = -2000000000;
  in.erro.http = -2000000000;
  in.midia.container = "outro";
  in.midia.video = "mpeg2";
  in.midia.hdr = "hdr10plus";
  in.midia.fps = 999999999.9;
  in.midia.mbps = 999999999.9;
  in.midia.gb = 999999999.9;
  in.midia.audio = "truehd_atmos";
  in.midia.http = 2000000000;
  in.midia.ttfb_ms = 2000000000;
  for (i = 0; i < RES_T_N; i++)
    in.cadeia[i] = (FarolPacotePasso){ FAROL_EST_NAO_TESTADO, 2000000000 + i, 2000000000 + i };
  for (i = 0; i < FAROL_PACOTE_TENTOU_MAX; i++)
    tentou[i] = (FarolPacoteTentativa){ "aliviar_memoria", 0, -2000000000 - i };
  in.tentou = tentou;
  in.n_tentou = FAROL_PACOTE_TENTOU_MAX;
  farolanel_zerar();
  for (i = 0; i < FAROL_ANEL_N; i++) {
    g_relogio_ms = 1000u + (uint32_t)i;
    farol_ev(FM_MEMORIA, FE_FAROL_RESULTADO, 2, -2000000000 - i, -2000000000 - i, "av_not_supported_format");
  }
  in.agora_ms = 2000001000u;
  in.janela_ms = 0;
  assert(farolpacote_montar(&in, buf, sizeof buf, &res) == 1);
  assert(res.bytes <= FAROL_JSON_MAX && res.linha_cortados > 0);
  puts(buf);

  /* 3. Pacote minimo (desconhecidos / padrao) */
  farolanel_zerar();
  farolpacote_entrada_padrao(&in);
  farolperfil_padrao(&perfil);
  in.perfil = &perfil;
  assert(farolpacote_montar(&in, buf, sizeof buf, &res) == 1);
  puts(buf);
  return 0;
}

int main(int argc, char **argv) {
  if (argc >= 2 && strcmp(argv[1], "--emitir-contrato") == 0)
    return emitir_contrato();
  if (argc >= 3 && strcmp(argv[1], "--regcodigo") == 0) {
    char cod[8];
    if (!regcodigo_de_id(argv[2], cod)) return 1;
    puts(cod);
    return 0;
  }

  testar_dourado();
  testar_teto_e_corte();
  testar_fingerprint();
  testar_minimo_e_perfil_null();
  testar_concorrencia();
  farolanel_relogio(NULL);
  puts("farol_pacote: dourado, teto, janela, fingerprint e concorrencia ok");
  return 0;
}
