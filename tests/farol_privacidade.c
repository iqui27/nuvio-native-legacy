// Suite de privacidade ponta a ponta do Farol (PLANO.md §2.5, os 6 itens):
//   1. Canarios: enche todas as entradas (URL com chave, magnet, titulo com
//      acento, perfil, e-mail, IPv4, IPv6, JWT, Authorization: Bearer,
//      user:pass@host, /home/..., PIN, AVPlay, modelo, addon_hex) nos
//      produtores, no perfil e no montador -> ZERO ocorrencias de CANARIO no JSON.
//   2. Lista branca de chaves: parser independente percorre cada chave/caminho
//      do JSON montado e exige presenca tanto na tabela de producao quanto em
//      uma lista independente.
//   3. Propriedade do token: 100 000 cadeias aleatorias (bytes 1..255) por
//      farolanel_token -> so [a-z0-9_.:+-], <= 39 bytes.
//   4. Padroes proibidos no JSON final: ://, @, IPv4, 20+ hex seguidos, %2F,
//      Bearer -> zero ocorrencias (tambem roda sobre arquivo via --auditar).
//   5. Teto: anel cheio (256 eventos) + todos os campos -> <= 35 000 bytes.
//   6. Espelho na tela: as colunas "Vai junto / Nunca vai" cobrem todo grupo
//      do schema; chave nova sem grupo no espelho falha o teste.
#define _POSIX_C_SOURCE 200809L
#include "farolanel.h"
#include "farolerro.h"
#include "farolpacote.h"
#include "farolperfil.h"
#include "farolprodutor.h"
#include "farolredator.h"
#include "faroltexto.h"
#include <assert.h>
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t g_relogio;
static uint32_t relogio_falso(void) { return g_relogio; }

/* Lista independente de caminhos permitidos (espelho do schema §2.6). */
static const char PERMITIDOS[] =
  "|v|gatilho|sintoma|erro|erro.codigo|erro.estagio|erro.nativo|erro.nativo_num|"
  "erro.http|erro.fingerprint|erro.fp|erro.tentou|erro.tentou[]|"
  "erro.tentou[].acao|erro.tentou[].ok|erro.tentou[].ms|"
  "aparelho|aparelho.plataforma|aparelho.so|aparelho.so_maior|aparelho.modelo|"
  "aparelho.ram_total_mb|aparelho.ram_livre_mb|aparelho.tela|aparelho.tela.w|"
  "aparelho.tela.h|aparelho.tela.hdr|aparelho.tela.dv|aparelho.tela.hevc|"
  "aparelho.tela.av1|aparelho.audio|aparelho.audio.dts|aparelho.audio.truehd|"
  "aparelho.audio.eac3|aparelho.audio.ac3|aparelho.rede|aparelho.rede.offline|"
  "aparelho.rede.hosts_com_falha|aparelho.rede.vazao_mbps|"
  "cliente|cliente.versao|cliente.commit|cliente.build|cliente.backend|"
  "cliente.idioma|cliente.aberto_s|cliente.gpu|cliente.ajustes|"
  "cliente.ajustes.glass_ui|cliente.ajustes.qualidade_max|"
  "cliente.ajustes.tamanho_max_gb|cliente.ajustes.dts|cliente.ajustes.dv_mkv|"
  "cliente.ajustes.legenda_auto_sync|cliente.addons|cliente.addons.total|"
  "cliente.addons.ativos|cliente.addons.isolados|cliente.addons.tipos|"
  "cliente.addons.tipos[]|cliente.debrid|cliente.debrid[]|"
  "cliente.contas|cliente.contas[]|"
  "midia|midia.container|midia.video|midia.hdr|midia.res|midia.fps|midia.mbps|"
  "midia.gb|midia.audio|midia.legenda|midia.entrega|midia.http|midia.ttfb_ms|"
  "cadeia|cadeia[]|cadeia[].passo|cadeia[].estado|cadeia[].ms|cadeia[].http|"
  "linha|linha[]|linha[].t|linha[].m|linha[].e|linha[].a|linha[].b|linha[].k|"
  "privacidade|privacidade.redator|privacidade.campos_recusados|";

static int contem_icase(const char *hay, const char *needle) {
  size_t n = strlen(needle);
  const char *p;
  for (p = hay; *p; p++) {
    size_t i;
    for (i = 0; i < n && p[i]; i++) {
      if (tolower((unsigned char)p[i]) != tolower((unsigned char)needle[i])) break;
    }
    if (i == n) return 1;
  }
  return 0;
}

/* Item 4: padroes proibidos no JSON final (://, @, IPv4, 20+ hex, %2F, Bearer). */
static void assert_sem_padroes_proibidos(const char *json) {
  const char *p;
  assert(json && *json);
  assert(!strstr(json, "://"));
  assert(!strchr(json, '@'));
  assert(!contem_icase(json, "%2f"));
  assert(!contem_icase(json, "bearer"));
  assert(!contem_icase(json, "canario"));
  for (p = json; *p; p++) {
    if (isdigit((unsigned char)*p) && (p == json || !isdigit((unsigned char)p[-1]))) {
      unsigned a, b, c, d;
      int n = 0;
      if (sscanf(p, "%u.%u.%u.%u%n", &a, &b, &c, &d, &n) == 4 && n > 0 &&
          a < 256 && b < 256 && c < 256 && d < 256 &&
          !isdigit((unsigned char)p[n])) {
        assert(0 && "IPv4 encontrado no JSON final");
      }
    }
    if (strspn(p, "0123456789abcdefABCDEF") >= 20) {
      assert(0 && "sequencia de 20+ hex encontrada no JSON final");
    }
  }
}

/* Item 2: parser JSON independente que confere TODA chave contra a lista branca. */
static const char *ler_str(const char *p, char *out, size_t cap) {
  size_t n = 0;
  assert(*p++ == '"');
  while (*p && *p != '"') {
    assert((unsigned char)*p >= 32 && *p != '\\' && n + 1 < cap);
    out[n++] = *p++;
  }
  assert(*p++ == '"');
  out[n] = '\0';
  return p;
}

static int no_schema_producao(const char *caminho) {
  size_t n = 0, i;
  const FarolCampo *sc = farolredator_schema(&n);
  for (i = 0; i < n; i++) {
    if (strcmp(sc[i].caminho, caminho) == 0) return 1;
  }
  return 0;
}

static const char *ler_json_whitelisted(const char *p, const char *caminho, int prof) {
  assert(prof < 10);
  if (*caminho) {
    char agulha[128];
    assert(snprintf(agulha, sizeof agulha, "|%s|", caminho) < (int)sizeof agulha);
    assert(strstr(PERMITIDOS, agulha) != NULL);
    assert(no_schema_producao(caminho));
  }
  if (*p == '{' || *p == '[') {
    int arr = (*p++ == '[');
    char fim = arr ? ']' : '}';
    if (*p == fim) return p + 1;
    for (;;) {
      char chave[96], prox[128];
      if (arr) {
        assert(snprintf(prox, sizeof prox, "%s[]", caminho) < (int)sizeof prox);
      } else {
        p = ler_str(p, chave, sizeof chave);
        assert(*p++ == ':');
        assert(snprintf(prox, sizeof prox, "%s%s%s",
                        caminho, *caminho ? "." : "", chave) < (int)sizeof prox);
      }
      p = ler_json_whitelisted(p, prox, prof + 1);
      if (*p != ',') break;
      p++;
    }
    assert(*p++ == fim);
    return p;
  }
  if (*p == '"') {
    char val[256];
    return ler_str(p, val, sizeof val);
  }
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

static void assert_json_lista_branca(const char *json) {
  assert(json && *json == '{');
  assert(*ler_json_whitelisted(json, "", 0) == '\0');
}

/* Item 1: canarios ponta a ponta (produtores -> anel -> perfil -> pacote). */
static void item1_canarios(void) {
  static const char *const CANARIOS[] = {
    "https://fonte.privada.invalid/CANARIO_1/video.mkv?key=CANARIO_2&token=CANARIO_3",
    "magnet:?xt=urn:btih:CANARIO_4&dn=CANARIO_5",
    "O Poderoso Chefão CANARIO_6 (1972)",
    "Perfil da Ana CANARIO_7",
    "usuario.CANARIO_8@exemplo.invalid",
    "192.168.10.42",
    "2001:db8:85a3::8a2e:370:7334",
    "eyJhbGciOiJIUzI1NiJ9.CANARIO_9.assinaturaCANARIO_10",
    "Authorization: Bearer CANARIO_11_SEGREDO",
    "https://CANARIO_12:CANARIO_13@xtream.invalid:8080/live/1.ts",
    "/home/CANARIO_14/.nuvio/conta.txt",
    "PIN 7391 CANARIO_15",
    "73914820",
    "CANARIO_16%2Fsegredo",
    "PLAYER_ERROR_CANARIO_17",
    "av_canario_18",
    "OLED55CANARIO19",
    "addon_10472911",
    "CANARIO_20",
    "0123456789abcdef0123456789abcdef"
  };
  unsigned char sal[16] = { 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 };
  char buf[FAROL_JSON_N];
  FarolPacoteRes res;
  size_t k;

  farolanel_zerar();
  farolanel_relogio(relogio_falso);
  g_relogio = 50000;

  /* 1a. Grava todos os canarios no anel pelos produtores reais. */
  for (k = 0; k < sizeof CANARIOS / sizeof *CANARIOS; k++) {
    char hash_host[12];
    g_relogio += 10;
    farol_ev(FM_FONTE, FE_FONTE_ESCOLHIDA, 1, (int32_t)k, 0, CANARIOS[k]);
    farol_ev(FM_PLAYER, FE_ABRIR, 0, 1, 2, CANARIOS[k]);
    /* Texto puro numerico no inicio de video_erro_texto() e interpretado por
     * farolerro_ler como codigo nativo inteiro; o PIN numerico puro e testado
     * em tok, commit, so, versao, modelo e todos os campos textuais abaixo. */
    if (!isdigit((unsigned char)CANARIOS[k][0])) {
      farolprod_erro(FP_TIZEN_WGT, CANARIOS[k], 0, 0);
      farolprod_erro(FP_WEBOS, CANARIOS[k], 204, 0);
      farolprod_recon(FP_ANDROID, CANARIOS[k], 2002, 1, 1500);
    }
    farolredator_hash_host(CANARIOS[k], sal, hash_host);
    farol_ev(FM_ADDON, FE_ADDON_LENTO, 1, 9000, 0, hash_host);
  }

  /* 1b. Enche perfil, parsers, veredito, tentou e midia com cada canario. */
  for (k = 0; k < sizeof CANARIOS / sizeof *CANARIOS; k++) {
    const char *can = CANARIOS[k];
    FarolPerfil perfil;
    FarolTvInfoAndroid ai;
    FarolTvInfoTpk ti;
    FarolTvInfoWgt wi;
    FarolPacoteEntrada in;
    FarolPacoteTentativa tent;
    char tmp_env[256];

    farolperfil_padrao(&perfil);
    snprintf(tmp_env, sizeof tmp_env, "%s|34|%s|%s", can, can, can);
    farolperfil_parse_android_info(tmp_env, &ai);
    snprintf(tmp_env, sizeof tmp_env, "[tv] modelo=%s tizen=%s tela=1920x1080", can, can);
    farolperfil_parse_tpk_tv(tmp_env, &ti);
    snprintf(tmp_env, sizeof tmp_env, "%s|%s", can, can);
    farolperfil_parse_wgt_info(tmp_env, &wi);

    /* Testa o caminho real parsers -> FarolPerfil -> farolpacote_montar: mesmo
     * quando o filtro amplo de modelo aceita ASCII limpo ("OLED55CANARIO19",
     * "Perfil da Ana CANARIO_7"), a barreira fechada do redator impede vazamento. */
    {
      const char *modelos_parsed[3] = { ai.modelo, ti.modelo, wi.modelo };
      int mp;
      for (mp = 0; mp < 3; mp++) {
        farolperfil_padrao(&perfil);
        snprintf(perfil.aparelho.modelo, sizeof perfil.aparelho.modelo, "%s", modelos_parsed[mp]);
        farolpacote_entrada_padrao(&in);
        in.perfil = &perfil;
        in.agora_ms = g_relogio + 100;
        assert(farolpacote_montar(&in, buf, sizeof buf, &res) == 1);
        assert(res.bytes <= FAROL_JSON_MAX);
        assert_json_lista_branca(buf);
        assert_sem_padroes_proibidos(buf);
        assert(!strstr(buf, can));
      }
    }

    /* Tambem injeta direto nos campos do struct (pior caso: chamador sem filtro). */
    snprintf(perfil.aparelho.plataforma, sizeof perfil.aparelho.plataforma, "%.15s", can);
    snprintf(perfil.aparelho.so, sizeof perfil.aparelho.so, "%.23s", can);
    snprintf(perfil.aparelho.modelo, sizeof perfil.aparelho.modelo, "%.32s", can);
    snprintf(perfil.cliente.versao, sizeof perfil.cliente.versao, "%.23s", can);
    snprintf(perfil.cliente.commit, sizeof perfil.cliente.commit, "%.15s", can);
    snprintf(perfil.cliente.build, sizeof perfil.cliente.build, "%.15s", can);
    snprintf(perfil.cliente.backend, sizeof perfil.cliente.backend, "%.15s", can);
    snprintf(perfil.cliente.idioma, sizeof perfil.cliente.idioma, "%.11s", can);
    snprintf(perfil.cliente.gpu, sizeof perfil.cliente.gpu, "%.15s", can);
    snprintf(perfil.cliente.ajustes.qualidade_max, sizeof perfil.cliente.ajustes.qualidade_max, "%.15s", can);
    snprintf(perfil.cliente.ajustes.dts, sizeof perfil.cliente.ajustes.dts, "%.15s", can);

    farolpacote_entrada_padrao(&in);
    in.perfil = &perfil;
    in.codigo = can;
    in.estagio = RES_T_FONTE;
    snprintf(in.erro.cod, sizeof in.erro.cod, "%.23s", can);
    tent = (FarolPacoteTentativa){ can, 0, 1234 };
    in.tentou = &tent;
    in.n_tentou = 1;
    in.midia.container = can;
    in.midia.video = can;
    in.midia.hdr = can;
    in.midia.res = can;
    in.midia.audio = can;
    in.midia.legenda = can;
    in.midia.entrega = can;
    in.tem_cadeia = 1;
    in.agora_ms = g_relogio + 100;
    in.janela_ms = FAROL_PACOTE_JANELA_MS;

    assert(farolpacote_montar(&in, buf, sizeof buf, &res) == 1);
    assert(res.bytes <= FAROL_JSON_MAX);
    assert(res.campos_recusados > 0);
    assert_json_lista_branca(buf);
    assert_sem_padroes_proibidos(buf);
    assert(!strstr(buf, can));
  }
}

/* Item 2: schema de producao inteiro esta na lista branca e sem duplicatas. */
static void item2_schema_lista_branca(void) {
  size_t n = 0, i, k;
  const FarolCampo *sc = farolredator_schema(&n);
  assert(n > 0 && n < 256);
  for (i = 0; i < n; i++) {
    char agulha[128];
    assert(snprintf(agulha, sizeof agulha, "|%s|", sc[i].caminho) < (int)sizeof agulha);
    assert(strstr(PERMITIDOS, agulha) != NULL);
    for (k = 0; k < i; k++)
      assert(strcmp(sc[i].caminho, sc[k].caminho) != 0);
  }
}

/* Item 3: 100 000 cadeias aleatorias (bytes 1..255) por farolanel_token. */
static void item3_propriedade_token(void) {
  uint32_t rng = 0x13579BDFu;
  int k, virou_x = 0;
  for (k = 0; k < 100000; k++) {
    char in_s[80], out[FAROL_TOKEN_N];
    size_t len, i;
    rng = rng * 1664525u + 1013904223u;
    len = 1u + (rng % 64u);
    for (i = 0; i < len; i++) {
      rng = rng * 1664525u + 1013904223u;
      in_s[i] = (char)(1u + (rng % 255u));
    }
    in_s[len] = '\0';
    farolanel_token(in_s, out);
    assert(out[0] != '\0');
    assert(strlen(out) <= FAROL_TOKEN_N - 1);
    assert(strspn(out, "abcdefghijklmnopqrstuvwxyz0123456789_.:+-") == strlen(out));
    if (strcmp(out, "x") == 0) virou_x++;
  }
  assert(virou_x > 90000);
}

/* Item 5: anel cheio (256 eventos) + todos os campos <= 35 000 bytes. */
static void item5_teto_anel_cheio(void) {
  FarolPerfil perfil;
  FarolPacoteEntrada in;
  FarolPacoteTentativa tent[FAROL_PACOTE_TENTOU_MAX];
  FarolPacoteRes res;
  char buf[FAROL_JSON_N];
  int i;

  farolperfil_padrao(&perfil);
  strcpy(perfil.aparelho.plataforma, "webos");
  strcpy(perfil.aparelho.so, "4.9");
  perfil.aparelho.so_maior = 4;
  strcpy(perfil.aparelho.modelo, "OLED55C9PSA");
  perfil.aparelho.ram_total_mb = 1870;
  perfil.aparelho.ram_livre_mb = 212;
  perfil.aparelho.tela = (FarolPerfilTela){ 3840, 2160, 1, 1, 1, -1 };
  perfil.aparelho.audio = (FarolPerfilAudio){ 1, -1, 1, 1 };
  perfil.aparelho.rede = (FarolPerfilRede){ 0, 2, 42.5 };
  strcpy(perfil.cliente.versao, "2.1.0");
  strcpy(perfil.cliente.commit, "c18c21756");
  strcpy(perfil.cliente.build, "webos-ipk");
  strcpy(perfil.cliente.backend, "starfish");
  strcpy(perfil.cliente.idioma, "pt-BR");
  perfil.cliente.aberto_s = 3600;
  strcpy(perfil.cliente.gpu, "auto:alto");
  perfil.cliente.ajustes.glass_ui = 1;
  strcpy(perfil.cliente.ajustes.qualidade_max, "4k");
  perfil.cliente.ajustes.tamanho_max_gb = 40.0;
  strcpy(perfil.cliente.ajustes.dts, "ac3");
  perfil.cliente.ajustes.dv_mkv = 1;
  perfil.cliente.ajustes.legenda_auto_sync = 1;
  perfil.cliente.addons = (FarolPerfilAddons){
    12, 11, 1, FAROL_ADDON_CATALOGO | FAROL_ADDON_FONTES | FAROL_ADDON_LEGENDAS | FAROL_ADDON_META
  };
  perfil.cliente.debrid_mask = FAROL_DEBRID_TORBOX | FAROL_DEBRID_REALDEBRID;
  perfil.cliente.contas_mask = FAROL_CONTA_TRAKT | FAROL_CONTA_SIMKL;

  for (i = 0; i < FAROL_PACOTE_TENTOU_MAX; i++)
    tent[i] = (FarolPacoteTentativa){ "reconectar_rede", i & 1, 9999999 + i };

  farolanel_zerar();
  farolanel_relogio(relogio_falso);
  for (i = 0; i < FAROL_ANEL_N; i++) {
    g_relogio = 500000u + (uint32_t)i;
    farol_ev(FM_FONTE, FE_FONTE_ESCOLHIDA, 1, 9000000 + i, 1000000 + i, "2160p_hdr_dv_eac3");
  }

  farolpacote_entrada_padrao(&in);
  in.gatilho = FAROL_G_FALHA_SEM_CONSERTO;
  in.sintoma = FAROL_PS_NAO_COMECA;
  in.codigo = "TV_FORMATO_REPETE";
  in.estagio = RES_T_TV;
  in.erro.classe = FC_FORMATO;
  in.erro.nativo = 204;
  strcpy(in.erro.cod, "ums_204");
  in.tentou = tent;
  in.n_tentou = FAROL_PACOTE_TENTOU_MAX;
  in.perfil = &perfil;
  in.midia.container = "mkv";
  in.midia.video = "hevc10";
  in.midia.hdr = "dv";
  in.midia.res = "2160p";
  in.midia.fps = 23.976;
  in.midia.mbps = 45.2;
  in.midia.gb = 28.4;
  in.midia.audio = "eac3_51";
  in.midia.legenda = "srt_embutida";
  in.midia.entrega = "debrid_cache";
  in.midia.http = 206;
  in.midia.ttfb_ms = 210;
  in.tem_cadeia = 1;
  for (i = 0; i < RES_T_N; i++)
    in.cadeia[i] = (FarolPacotePasso){ FAROL_EST_OK, 1200 + i, 206 };
  in.agora_ms = 500000u + 299900u;
  in.janela_ms = FAROL_PACOTE_JANELA_MS;

  assert(farolpacote_montar(&in, buf, sizeof buf, &res) == 1);
  assert(res.bytes == strlen(buf) && res.bytes <= FAROL_JSON_MAX);
  assert(res.campos_recusados == 0);
  assert_json_lista_branca(buf);
  assert_sem_padroes_proibidos(buf);
}

/* Item 6: espelho na tela ("Vai junto / Nunca vai") derivado da tabela do schema. */
static void item6_espelho_tela(void) {
  size_t n_sc = 0, n_grp = 0, n_vai = 0, n_nunca = 0, i, k;
  const FarolCampo *sc = farolredator_schema(&n_sc);
  const FarolEspelhoGrupo *grps = farolpacote_espelho_grupos(&n_grp);
  const char *const *vai = farolpacote_vai_junto(&n_vai);
  const char *const *nunca = farolpacote_nunca_vai(&n_nunca);
  unsigned cobertura_vai[16] = { 0 };

  assert(n_sc > 0 && n_grp > 0 && n_vai == 6 && n_nunca == 6);
  assert(n_vai <= sizeof cobertura_vai / sizeof *cobertura_vai);

  /* Todo campo do schema tem de mapear para uma linha real de "Vai junto". */
  for (i = 0; i < n_sc; i++) {
    const char *frase = farolpacote_espelho_para(sc[i].caminho);
    int achou = 0;
    assert(frase != NULL && frase[0] != '\0');
    for (k = 0; k < n_vai; k++) {
      if (strcmp(vai[k], frase) == 0) {
        cobertura_vai[k]++;
        achou = 1;
        break;
      }
    }
    assert(achou == 1);
  }

  /* Nenhuma linha de "Vai junto" pode ficar orfa (sem campo no schema). */
  for (k = 0; k < n_vai; k++) {
    assert(vai[k] && vai[k][0]);
    assert(cobertura_vai[k] > 0);
    for (i = 0; i < k; i++) assert(strcmp(vai[i], vai[k]) != 0);
  }

  /* Cada entrada da tabela de grupos referencia frase valida de FT_VAI_*. */
  for (i = 0; i < n_grp; i++) {
    int achou = 0;
    assert(grps[i].prefixo && grps[i].prefixo[0]);
    assert(grps[i].frase && grps[i].frase[0]);
    for (k = 0; k < n_vai; k++)
      if (strcmp(vai[k], grps[i].frase) == 0) achou = 1;
    assert(achou == 1);
  }

  /* "Nunca vai" lista os 6 itens de faroltexto.h e nenhum deles existe no schema. */
  assert(strcmp(nunca[0], FT_NUNCA_TITULOS) == 0);
  assert(strcmp(nunca[1], FT_NUNCA_PERFIL) == 0);
  assert(strcmp(nunca[2], FT_NUNCA_ENDERECOS) == 0);
  assert(strcmp(nunca[3], FT_NUNCA_SENHAS) == 0);
  assert(strcmp(nunca[4], FT_NUNCA_IP) == 0);
  assert(strcmp(nunca[5], FT_NUNCA_HISTORICO) == 0);
  {
    static const char *const PROIBIDAS[] = {
      "titulo", "url", "email", "senha", "token", "ip",
      "perfil", "historico", "pessoa", "cookie", "ssid", "mac"
    };
    for (i = 0; i < n_sc; i++) {
      const char *p = sc[i].caminho;
      while (*p) {
        size_t slen = strcspn(p, ".[]");
        if (slen > 0) {
          for (k = 0; k < sizeof PROIBIDAS / sizeof *PROIBIDAS; k++) {
            if (strlen(PROIBIDAS[k]) == slen && memcmp(p, PROIBIDAS[k], slen) == 0)
              assert(0 && "segmento proibido no schema");
          }
        }
        p += slen;
        while (*p == '.' || *p == '[' || *p == ']') p++;
      }
    }
  }
}

static void *fio_canario(void *arg) {
  int id = (int)(intptr_t)arg;
  int i;
  for (i = 0; i < 2000; i++) {
    farol_ev(FM_FONTE, FE_FONTE_ESCOLHIDA, 1, id, i, "https://CANARIO_FIO/segredo?k=1");
    farolprod_erro(FP_TIZEN_WGT, "PLAYER_ERROR_CANARIO_FIO", 0, 0);
  }
  return NULL;
}

static void concorrencia_privacidade(void) {
  pthread_t th[4];
  FarolPacoteEntrada in;
  FarolPacoteRes res;
  char buf[FAROL_JSON_N];
  int i;

  farolanel_zerar();
  farolanel_relogio(NULL);
  for (i = 0; i < 4; i++)
    assert(pthread_create(&th[i], NULL, fio_canario, (void *)(intptr_t)i) == 0);

  farolpacote_entrada_padrao(&in);
  in.janela_ms = 0;
  for (i = 0; i < 40; i++) {
    assert(farolpacote_montar(&in, buf, sizeof buf, &res) == 1);
    assert(res.bytes <= FAROL_JSON_MAX);
    assert_json_lista_branca(buf);
    assert_sem_padroes_proibidos(buf);
  }

  for (i = 0; i < 4; i++) assert(pthread_join(th[i], NULL) == 0);
  farolperfil_encerrar();
}

static int auditar_arquivo(const char *caminho) {
  char buf[FAROL_JSON_N + 16];
  FILE *f = fopen(caminho, "rb");
  size_t n;
  if (!f) {
    fprintf(stderr, "farol_privacidade: nao abriu %s\n", caminho);
    return 1;
  }
  n = fread(buf, 1, sizeof buf - 1, f);
  fclose(f);
  buf[n] = '\0';
  while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = '\0';
  assert(n > 0 && n <= FAROL_JSON_MAX);
  assert_json_lista_branca(buf);
  assert_sem_padroes_proibidos(buf);
  return 0;
}

int main(int argc, char **argv) {
  if (argc >= 3 && strcmp(argv[1], "--auditar") == 0)
    return auditar_arquivo(argv[2]);

  item1_canarios();
  item2_schema_lista_branca();
  item3_propriedade_token();
  item5_teto_anel_cheio();
  item6_espelho_tela();
  concorrencia_privacidade();
  farolanel_relogio(NULL);
  puts("farol_privacidade: 6 itens da 2.5 (canarios, lista branca, token, padroes, teto, espelho) e concorrencia ok");
  return 0;
}
