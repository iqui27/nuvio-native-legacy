// #202: as regras do auto-play (add-ons/plugins permitidos, escopo, regex
// exigir/preferir, "usar os outros"), a traducao da regex do oficial para
// POSIX, a regex invalida, o perfil e as chaves da conta. Sem rede e sem SDL.
//
//   bash tests/fonteregra.sh
#include "fonteregra.h"
#include "dados.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FonteRegraCfg cfg(int escopo, int regex, int outros) {
  FonteRegraCfg c = { escopo, regex, outros };
  return c;
}

static void traducao(void) {
  char p[512], x[128];
  assert(fonteregra_posix("4K|2160p|Remux", p, sizeof p, x, sizeof x));
  assert(!strcmp(p, "4K|2160p|Remux") && !x[0]);
  assert(fonteregra_posix("(?i)\\d{3,4}p", p, sizeof p, x, sizeof x));
  assert(!strcmp(p, "[0-9]{3,4}p"));
  assert(fonteregra_posix("^(?!.*(CAM|TS)).*1080p", p, sizeof p, x, sizeof x));
  assert(!strcmp(p, "^.*1080p") && !strcmp(x, "CAM|TS"));
  assert(fonteregra_posix("(?:a|b)+?", p, sizeof p, x, sizeof x));
  assert(!strcmp(p, "(a|b)+"));
  // \b vira borda que consome (o "contem" nao muda)
  assert(fonteregra_regex_testar("\\bES\\b", "Movie [ES] 1080p") == 1);
  assert(fonteregra_regex_testar("\\bES\\b", "Series 1080p") == 0);
  assert(fonteregra_regex_testar("\\bES\\b", "ES") == 1);
}

static void regex(void) {
  // Sem caixa, contra o texto inteiro.
  assert(fonteregra_regex_testar("castellano", "Torrentio\n1080p CASTELLANO") == 1);
  assert(fonteregra_regex_testar("castellano", "Torrentio\n1080p English") == 0);
  // Exclusao do oficial: o lookahead negativo vira lista de palavras proibidas.
  assert(fonteregra_regex_testar("^(?!.*(CAM|TS)).*1080p", "Film 1080p WEB") == 1);
  assert(fonteregra_regex_testar("^(?!.*(CAM|TS)).*1080p", "Film 1080p CAM") == 0);
  // Invalida e nao configurada = -1 (ignorada).
  assert(fonteregra_regex_testar("(ESP", "ESP") == -1);
  assert(fonteregra_regex_testar("[abc", "a") == -1);
  assert(fonteregra_regex_testar("", "a") == -1);
  assert(fonteregra_regex_testar("|||", "a") == -1);      // sem letra nem digito
  // Os modelos compilam.
  for (int i = 1; i < fonteregra_modelos(); i++)
    assert(fonteregra_regex_testar(fonteregra_modelo(i), "x") >= 0);
  assert(fonteregra_regex_testar(fonteregra_modelo(1), "Pelicula 1080p Latino") == 1);
  assert(fonteregra_regex_testar(fonteregra_modelo(1), "Movie 1080p English") == 0);
  assert(fonteregra_regex_testar(fonteregra_modelo(1), "Spider-Man Espanol 4K") == 1);
}

// Lookahead positivo "(?=" e lookbehind: o POSIX nao tem, entao o padrao roda no
// motor proprio (regexjs.c). Antes eram rejeitados como invalidos (-1).
static void lookahead(void) {
  // O idioma "E": todas as condicoes valem a partir do inicio do texto.
  const char *e = "^(?=.*\\b1080p\\b)(?=.*\\bWEB-DL\\b)";
  assert(fonteregra_regex_testar(e, "Film 1080p WEB-DL x265") == 1);
  assert(fonteregra_regex_testar(e, "Film 1080p CAM") == 0);       // falta a segunda
  assert(fonteregra_regex_testar(e, "Film WEB-DL 720p") == 0);     // falta a primeira
  // Sem caixa; e, como no oficial (Android e web), o ponto NAO atravessa quebra de
  // linha: o "^(?=.*...)" so enxerga a primeira linha do texto.
  assert(fonteregra_regex_testar(e, "Torrentio film 1080P web-dl") == 1);
  assert(fonteregra_regex_testar(e, "Torrentio\nfilm 1080P\nweb-dl") == 0);
  // Lookahead negativo REAL junto do positivo (nao a lista de exclusao do POSIX).
  assert(fonteregra_regex_testar("^(?=.*1080p)(?!.*\\bDTS\\b)", "Film 1080p DDP5.1") == 1);
  assert(fonteregra_regex_testar("^(?=.*1080p)(?!.*\\bDTS\\b)", "Film 1080p DTS") == 0);
  // Exclusao "do oficial": o DTS na 2a linha escapa do lookahead (o ponto nao
  // atravessa), mas a lista de palavras extraida do (?!...(...)) o pega, como la.
  assert(fonteregra_regex_testar("^(?=.*1080p)(?!.*(DTS|CAM))", "Film 1080p\nDTS") == 0);
  // Lookbehind.
  assert(fonteregra_regex_testar("(?<=WEB-)DL", "Film WEB-DL") == 1);
  assert(fonteregra_regex_testar("(?<=WEB-)DL", "Film BD-DL") == 0);
  { char longo[1008];
    FonteRegraCfg c = cfg(FR_ESCOPO_TODAS, FR_REGEX_EXIGIR, 1);
    memset(longo, 'x', 1000);
    strcpy(longo + 1000, " WEB-DL");
    assert(fonteregra_regex_testar("(?<=WEB-)DL", longo) == 1);
    assert(fonteregra_definir_regex("(?<=WEB-)DL") == 1);
    assert(fonteregra_grupo(&c, "Torrentio", 0, longo) == 0);
    assert(fonteregra_definir_regex("") == 0);
  }
  // (?= escapado ou dentro de colchete nao e lookahead: segue o POSIX.
  assert(fonteregra_regex_testar("a\\(?=b", "a=b") != -1);
  // Invalido continua -1 pelo motor proprio.
  assert(fonteregra_regex_testar("(?=abc", "abc") == -1);
  assert(fonteregra_regex_testar("(?=abc))", "abc") == -1);

  // O padrao do dono, TAL COMO ESCRITO (".", nao ".*"), igual ao que rodava no
  // Nuvio oficial: as quatro condicoes valem no MESMO ponto (um caractere e logo
  // uma palavra), e a unica palavra que as duas listas aceitam ali e "BD-Remux"
  // (o \b depois de "BD" cai no hifen). Entao so casa "BD-Remux". A exclusao do
  // oficial nao compila ("?:" vira palavra), por isso o DTS-HD passa.
  // Conferido contra o JS de verdade: tests/fonteregra_oficial.sh.
  const char *dono =
    "(?=.\\b(2160p|4K|UHD|1080p|FHD|BD|HD|720p|SDR)\\b)"
    "(?=.\\b(SeaDex|Remux|BD-Remux|Dual[\\s-]?Audio|Multi[\\s-]?Audio|Multi|Dub|Eng|Jap|10bit|HEVC|x265|H265|Hi10P|BluRay|WEB-DL|WEBRip|BDRip|BRRip|x264|AVC|H264|8bit|HDTV|DVDRip|HDRip|Subs)\\b)"
    "(?!.(DTS|DTS-HD|DTS:X|.DTS))"
    "(?!.(?:\\bDV\\b|\\bDolby\\s?Vision\\b)(?!.*\\b(HDR10|HDR|Multi)\\b))";
  assert(fonteregra_regex_testar(dono, "Film 1080p BluRay x265") == 0);
  assert(fonteregra_regex_testar(dono, "Film 1080p BD-Remux") == 1);
  assert(fonteregra_regex_testar(dono, "Anime BD-Remux 1080p DTS-HD") == 1);
  assert(fonteregra_regex_testar(dono, "x 1080p x265") == 0);

  // A versao corrigida (^ e ".*"): resolucao E origem, sem DTS, e Dolby Vision so
  // se tiver HDR10/HDR/Multi junto.
  const char *certo =
    "^(?=.*\\b(2160p|4K|UHD|1080p|FHD|BD|HD|720p|SDR)\\b)"
    "(?=.*\\b(SeaDex|Remux|BD-Remux|Dual[\\s-]?Audio|Multi[\\s-]?Audio|Multi|Dub|Eng|Jap|10bit|HEVC|x265|H265|Hi10P|BluRay|WEB-DL|WEBRip|BDRip|BRRip|x264|AVC|H264|8bit|HDTV|DVDRip|HDRip|Subs)\\b)"
    "(?!.*\\b(DTS|DTS-HD|DTS:X)\\b)"
    "(?!(?=.*\\b(DV|Dolby\\s?Vision)\\b)(?!.*\\b(HDR10|HDR|Multi)\\b))";
  assert(fonteregra_regex_testar(certo, "Film 1080p BluRay x265") == 1);
  assert(fonteregra_regex_testar(certo, "Anime 2160p Dual Audio HEVC") == 1);
  assert(fonteregra_regex_testar(certo, "Film 1080p BluRay x265 DTS-HD") == 0);   // DTS
  assert(fonteregra_regex_testar(certo, "Film 1080p BluRay DV") == 0);            // DV sem HDR
  assert(fonteregra_regex_testar(certo, "Film 2160p BluRay DV HDR10") == 1);      // DV com HDR10
  assert(fonteregra_regex_testar(certo, "Film 2160p WEB-DL Dolby Vision") == 0);
  assert(fonteregra_regex_testar(certo, "Film 1080p") == 0);                      // sem origem
  assert(fonteregra_regex_testar(certo, "Film BluRay") == 0);                     // sem resolucao
}

static void grupos(void) {
  FonteRegraCfg c;
  // Nada configurado: tudo grupo 0 (a escolha de antes).
  c = cfg(FR_ESCOPO_TODAS, FR_REGEX_DESLIGADA, 1);
  assert(!fonteregra_ativa(&c));
  assert(fonteregra_grupo(&c, "Torrentio", 0, "x") == 0);
  assert(fonteregra_grupo(&c, "MegaEmbed", 1, "x") == 0);

  // Add-ons permitidos: os espanhois primeiro, os outros depois (padrao).
  assert(fonteregra_alternar(0, "Cuevana ES") == 1);
  assert(fonteregra_alternar(0, "  Torrentio Latino  ") == 1);
  assert(fonteregra_contem(0, "torrentio latino"));
  assert(fonteregra_ativa(&c));
  assert(fonteregra_grupo(&c, "Cuevana ES", 0, "x") == 0);
  assert(fonteregra_grupo(&c, "CUEVANA es", 0, "x") == 0);        // sem caixa
  assert(fonteregra_grupo(&c, "Torrentio", 0, "x") == 2);          // os outros: depois
  // A lista de add-ons nao filtra plugin (listas independentes, como o oficial).
  assert(fonteregra_grupo(&c, "MegaEmbed", 1, "x") == 0);
  // Sem "usar os outros": fora da lista nunca toca.
  c.usarOutros = 0;
  assert(fonteregra_grupo(&c, "Torrentio", 0, "x") == -1);
  assert(fonteregra_grupo_pendente(&c, "Torrentio", 0) == -1);
  assert(fonteregra_grupo_pendente(&c, "Cuevana ES", 0) == 0);
  c.usarOutros = 1;
  assert(fonteregra_grupo_pendente(&c, "Torrentio", 0) == 2);

  // Escopo: so add-ons tira os plugins, E "os outros" NAO os devolve. TCL do
  // dono (2.0.3, 21:24): "Somente add-ons instalados" com "usar os outros"
  // ligado tocou "MegaEmbed - 1080" ("winner other+match"). "Os outros" e a
  // folga das listas de PERMITIDOS; o escopo e filtro: fora dele nunca toca,
  // nem quando nao ha fonte de add-on (a espera segue, ou a lista abre).
  c.escopo = FR_ESCOPO_ADDONS;
  assert(c.usarOutros == 1);
  assert(fonteregra_grupo(&c, "MegaEmbed", 1, "x") == -1);
  assert(fonteregra_grupo_pendente(&c, "MegaEmbed", 1) == -1);
  assert(fonteregra_grupo(&c, "Torrentio", 0, "x") == 2);         // os outros add-ons: sim
  assert(!fonteregra_no_escopo(&c, 1) && fonteregra_no_escopo(&c, 0));  // nem a lembrada
  c.usarOutros = 0;
  assert(fonteregra_grupo(&c, "MegaEmbed", 1, "x") == -1);
  c.escopo = FR_ESCOPO_PLUGINS;
  assert(fonteregra_grupo(&c, "Cuevana ES", 0, "x") == -1);
  c.usarOutros = 1;
  assert(fonteregra_grupo(&c, "Cuevana ES", 0, "x") == -1);      // simetrico: so plugins
  assert(fonteregra_grupo_pendente(&c, "Cuevana ES", 0) == -1);
  c.usarOutros = 0;
  assert(fonteregra_grupo(&c, "MegaEmbed", 1, "x") == 0);
  c = cfg(FR_ESCOPO_TODAS, FR_REGEX_DESLIGADA, 1);

  // Plugins permitidos.
  fonteregra_alternar(1, "VidSrc");
  assert(fonteregra_grupo(&c, "MegaEmbed", 1, "x") == 2);
  assert(fonteregra_grupo(&c, "VidSrc", 1, "x") == 0);
  fonteregra_alternar(1, "VidSrc");      // tira: lista vazia = todos
  assert(fonteregra_n(1) == 0);
  assert(fonteregra_grupo(&c, "MegaEmbed", 1, "x") == 0);

  // Regex PREFERIR: casa antes, sem tirar ninguem; dentro do permitido.
  assert(fonteregra_definir_regex(fonteregra_modelo(1)) == 1);
  assert(fonteregra_modelo_atual() == 1);
  c.regexModo = FR_REGEX_PREFERIR;
  assert(fonteregra_grupo(&c, "Cuevana ES", 0, "1080p Latino") == 0);
  assert(fonteregra_grupo(&c, "Cuevana ES", 0, "1080p English") == 1);
  assert(fonteregra_grupo(&c, "Torrentio", 0, "1080p Castellano") == 2);
  assert(fonteregra_grupo(&c, "Torrentio", 0, "1080p English") == 3);
  // Regex EXIGIR: filtro, em qualquer grupo.
  c.regexModo = FR_REGEX_EXIGIR;
  assert(fonteregra_grupo(&c, "Cuevana ES", 0, "1080p English") == -1);
  assert(fonteregra_grupo(&c, "Torrentio", 0, "1080p Castellano") == 2);
  // Regex invalida: ignorada (nada vira -1 por causa dela).
  assert(fonteregra_definir_regex("(ESP") == -1);
  assert(fonteregra_regex_estado() == -1);
  assert(fonteregra_grupo(&c, "Cuevana ES", 0, "1080p English") == 0);
  assert(fonteregra_definir_regex("") == 0);
}

static void perfilEConta(void) {
  char t[600], *saida = NULL;
  const char *blob =
    "{\"version\":1,\"features\":{\"player_settings\":{"
    "\"stream_auto_play_mode\":{\"type\":\"string\",\"value\":\"REGEX_MATCH\"},"
    "\"stream_auto_play_regex\":{\"type\":\"string\",\"value\":\"Latino|Castellano\"},"
    "\"stream_auto_play_selected_addons\":{\"type\":\"string_set\",\"value\":[\"Cuevana ES\", \"Torrentio \\\"ES\\\"\"]},"
    "\"stream_auto_play_selected_plugins\":[]}}}";
  unsigned v0;

  // Perfil 1 com a lista espanhola; perfil 2 vazio.
  fonteregra_limpar(0); fonteregra_limpar(1); fonteregra_definir_regex("");
  fonteregra_alternar(0, "Cuevana ES");
  fonteregra_perfil_guardar(1);
  fonteregra_limpar(0);
  fonteregra_perfil_guardar(2);
  assert(fonteregra_perfil_restaurar(1) && fonteregra_n(0) == 1);
  assert(fonteregra_perfil_restaurar(2) && fonteregra_n(0) == 0);
  assert(!fonteregra_perfil_restaurar(3));        // nunca usado aqui: nada muda

  // Conta -> TV: chaves do oficial, embrulhadas ou nao.
  v0 = fonteregra_versao();
  assert(fonteregra_do_blob(blob) == 2);
  assert(fonteregra_versao() != v0);
  fonteregra_regex(t, sizeof t);
  assert(!strcmp(t, "Latino|Castellano"));
  assert(fonteregra_n(0) == 2 && fonteregra_contem(0, "Torrentio \"ES\""));
  assert(fonteregra_n(1) == 0);
  assert(fonteregra_do_blob(blob) == 0);          // de novo: nada muda

  // TV -> conta: so a chave que existe, no mesmo tipo.
  assert(fonteregra_mesclar(blob, &saida) == 0 && !saida);
  fonteregra_alternar(1, "VidSrc");
  fonteregra_definir_regex("Latino");
  assert(fonteregra_mesclar(blob, &saida) == 2 && saida);
  assert(strstr(saida, "\"stream_auto_play_regex\":{\"type\":\"string\",\"value\":\"Latino\"}"));
  assert(strstr(saida, "\"stream_auto_play_selected_plugins\":[\"VidSrc\"]"));
  assert(strstr(saida, "\"stream_auto_play_mode\":{\"type\":\"string\",\"value\":\"REGEX_MATCH\"}"));
  assert(fonteregra_do_blob(saida) == 0);         // ida e volta estavel
  free(saida); saida = NULL;
  // Blob sem as chaves: nada e inventado.
  assert(fonteregra_mesclar("{\"version\":1,\"features\":{}}", &saida) == 0 && !saida);

  // Disco: fonteregra.txt guarda o estado em uso.
  fonteregra_definir_regex("");
  fonteregra_limpar(0);
  fonteregra_alternar(0, "Cuevana ES");
  fonteregra_carregar();
  assert(fonteregra_n(0) == 1 && fonteregra_contem(0, "Cuevana ES"));
}

// 2.0.2: ordem dos add-ons (fonteregra_ordem_*): definir, rank, texto, e o
// arquivo do perfil leva a ordem junto.
static void ordem(void) {
  const char *nomes[4] = { "Torrentio", "Cuevana ES", "torrentio", "AIOStreams" };
  char t[200], n[FR_NOME_MAX];
  assert(fonteregra_ordem_n() == 0);
  assert(fonteregra_ordem_rank("Torrentio") == FR_ORDEM_SEM);
  fonteregra_ordem_definir(nomes, 4);                       // "torrentio" repete: sem caixa
  assert(fonteregra_ordem_n() == 3);
  assert(fonteregra_ordem_rank("torrentio") == 0 && fonteregra_ordem_rank("Cuevana ES") == 1);
  assert(fonteregra_ordem_rank("AIOStreams") == 2 && fonteregra_ordem_rank("Outro") == FR_ORDEM_SEM);
  fonteregra_ordem_texto(t, sizeof t);
  assert(!strcmp(t, "Torrentio \xE2\x80\xBA Cuevana ES \xE2\x80\xBA AIOStreams"));
  // Recarrega do disco e o perfil restaura a ordem.
  fonteregra_carregar();
  assert(fonteregra_ordem_n() == 3 && fonteregra_ordem_nome(1, n, sizeof n) && !strcmp(n, "Cuevana ES"));
  fonteregra_perfil_guardar(7);
  fonteregra_ordem_definir(NULL, 0);
  assert(fonteregra_ordem_n() == 0);
  assert(fonteregra_perfil_restaurar(7) && fonteregra_ordem_n() == 3);
  // A ordem nao entra no blob da conta (o oficial nao tem chave dela).
  { char *saida = NULL;
    fonteregra_mesclar("{\"stream_auto_play_regex\":{\"type\":\"string\",\"value\":\"x\"}}", &saida);
    assert(!saida || !strstr(saida, "Cuevana"));
    free(saida); }
  fonteregra_ordem_definir(NULL, 0);
  fonteregra_perfil_esquecer();
}

int main(void) {
  const char *d = getenv("NUVIO_DADOS");
  assert(d && *d);
  dados_iniciar(d);
  assert(!strcmp(dados_dir(), d));
  traducao();
  regex();
  lookahead();
  grupos();
  perfilEConta();
  ordem();
  puts("fonteregra: ok");
  return 0;
}
