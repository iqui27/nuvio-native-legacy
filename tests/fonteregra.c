// #202: as regras do auto-play (add-ons/plugins permitidos, escopo, regex
// exigir/preferir, "usar os outros"), a regex do oficial no motor de JS, a
// regex invalida, o perfil e as chaves da conta. Sem rede e sem SDL.
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

// Padroes SEM "(?=" / lookbehind. Antes iam para uma traducao POSIX; agora tudo
// roda no motor de JS, e estes sao os casos que a traducao errava.
static void traducao(void) {
  assert(fonteregra_regex_testar("4K|2160p|Remux", "Film 2160p") == 1);
  assert(fonteregra_regex_testar("(?i)\\d{3,4}p", "Film 1080P") == 1);
  assert(fonteregra_regex_testar("(?:a|b)+?", "xbx") == 1);
  assert(fonteregra_regex_testar("\\bES\\b", "Movie [ES] 1080p") == 1);
  assert(fonteregra_regex_testar("\\bES\\b", "Series 1080p") == 0);
  assert(fonteregra_regex_testar("\\bES\\b", "ES") == 1);
  // Duas bordas no mesmo separador (o POSIX consumia o espaco numa so).
  assert(fonteregra_regex_testar("\\b(Remux|BluRay)\\b.*\\b2160p\\b", "Film Remux 2160p HDR") == 1);
  assert(fonteregra_regex_testar("2160p\\b.*\\bHDR", "Movie.2160p.HDR") == 1);
  assert(fonteregra_regex_testar("\\bWEB\\b-\\bDL\\b", "Film WEB-DL") == 1);
  // Lookahead negativo sem grupo dentro: o POSIX o jogava fora e CAM/DTS passavam.
  assert(fonteregra_regex_testar("(?!.*CAM)1080p", "Film 1080p CAM") == 0);
  assert(fonteregra_regex_testar("(?!.*CAM)1080p", "Film 1080p WEB") == 1);
  assert(fonteregra_regex_testar("1080p(?!.*DTS)", "Film 1080p DTS") == 0);
  assert(fonteregra_regex_testar("1080p(?!.*DTS)", "Film 1080p DDP") == 1);
}

static void regex(void) {
  // Sem caixa, contra o texto inteiro.
  assert(fonteregra_regex_testar("castellano", "Torrentio\n1080p CASTELLANO") == 1);
  assert(fonteregra_regex_testar("castellano", "Torrentio\n1080p English") == 0);
  // Lookahead negativo comum (sem a exclusao extra, e o proprio (?! que recusa).
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

// Lookahead positivo "(?=" e lookbehind (motor proprio, regexjs.c). Antes de ele
// existir eram rejeitados como invalidos (-1).
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
  // Lookahead negativo REAL junto do positivo (nao so a lista de exclusao).
  assert(fonteregra_regex_testar("^(?=.*1080p)(?!.*\\bDTS\\b)", "Film 1080p DDP5.1") == 1);
  assert(fonteregra_regex_testar("^(?=.*1080p)(?!.*\\bDTS\\b)", "Film 1080p DTS") == 0);
  // Sem a exclusao extra do NuvioTVSmart: a regex vale como escrita. O ponto nao
  // atravessa quebra de linha, entao o DTS na 2a linha escapa de ".*"; com
  // "[\s\S]*" o lookahead ve o texto inteiro e o pega.
  assert(fonteregra_regex_testar("^(?=.*1080p)(?!.*(DTS|CAM))", "Film 1080p\nDTS") == 1);
  assert(fonteregra_regex_testar("^(?=[\\s\\S]*1080p)(?![\\s\\S]*(DTS|CAM))", "Film 1080p\nDTS") == 0);
  // A palavra de um (?!...(...)) nao bloqueia sozinha: "DV sem HDR" recusa so DV
  // sem HDR. Antes a exclusao extra virava "nada com DV, HDR ou Multi".
  { const char *dv = "^(?![\\s\\S]*\\bDV\\b(?![\\s\\S]*\\b(HDR|Multi)\\b))";
    assert(fonteregra_regex_testar(dv, "Film 2160p HDR10 x265") == 1);
    assert(fonteregra_regex_testar(dv, "Film 2160p Multi x265") == 1);
    assert(fonteregra_regex_testar(dv, "Film 2160p DV HDR") == 1);
    assert(fonteregra_regex_testar(dv, "Film 2160p DV x265") == 0); }
  // DTS so recusado sem Dual/Multi: o lookahead deixa o Dual com DTS passar, e
  // agora nada mais o derruba pela palavra "DTS".
  { const char *d = "^(?!(?![\\s\\S]*\\b(Dual|Multi)\\b)[\\s\\S]*\\bDTS)";
    assert(fonteregra_regex_testar(d, "Anime 1080p Dual-Audio DTS-HD") == 1);
    assert(fonteregra_regex_testar(d, "Film 1080p DTS-HD") == 0); }
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
  // A resposta lembrada por texto (fonteregra.c, casaLembrandoSemTrava) cai
  // quando a regex muda: o mesmo texto, regex trocada, resposta nova.
  { FonteRegraCfg c = cfg(FR_ESCOPO_TODAS, FR_REGEX_EXIGIR, 1);
    const char *dts = "AIOStreams FHD\n\xF0\x9F\x8E\xA7 DTS-HD MA\nMovie.1080p.BluRay.mkv";
    const char *ac3 = "AIOStreams FHD\n\xF0\x9F\x8E\xA7 DD+ Atmos\nMovie.1080p.WEB-DL.mkv";
    int k;
    assert(fonteregra_definir_regex("^(?![\\s\\S]*\\bDTS)") == 1);
    for (k = 0; k < 3; k++) {   // a segunda e a terceira vem da memoria
      assert(fonteregra_grupo(&c, "AIOStreams", 0, dts) == -1);
      assert(fonteregra_grupo(&c, "AIOStreams", 0, ac3) == 0);
    }
    assert(fonteregra_definir_regex("^(?=[\\s\\S]*\\bDTS)") == 1);
    assert(fonteregra_grupo(&c, "AIOStreams", 0, dts) == 0);
    assert(fonteregra_grupo(&c, "AIOStreams", 0, ac3) == -1);
    // Preferir: o mesmo resultado lembrado, so o grupo muda.
    c.regexModo = FR_REGEX_PREFERIR;
    assert(fonteregra_grupo(&c, "AIOStreams", 0, dts) == 0);
    assert(fonteregra_grupo(&c, "AIOStreams", 0, ac3) == 1);
    // Muitos textos diferentes (mais que a tabela): a resposta continua certa.
    c.regexModo = FR_REGEX_EXIGIR;
    for (k = 0; k < 20000; k++) {
      char t[96];
      snprintf(t, sizeof t, "Fonte %d %s", k, k % 3 ? "DD+" : "DTS");
      assert(fonteregra_grupo(&c, "AIOStreams", 0, t) == (k % 3 ? -1 : 0));
    }
    assert(fonteregra_definir_regex("") == 0); }
  // (?= escapado ou dentro de colchete nao e lookahead.
  assert(fonteregra_regex_testar("a\\(?=b", "a=b") != -1);
  // Invalido continua -1 pelo motor proprio.
  assert(fonteregra_regex_testar("(?=abc", "abc") == -1);
  assert(fonteregra_regex_testar("(?=abc))", "abc") == -1);

  // O padrao do dono, TAL COMO ESCRITO (".", nao ".*"), igual ao que rodava no
  // Nuvio oficial: as quatro condicoes valem no MESMO ponto (um caractere e logo
  // uma palavra), e a unica palavra que as duas listas aceitam ali e "BD-Remux"
  // (o \b depois de "BD" cai no hifen). Entao so casa "BD-Remux", e o DTS-HD logo
  // depois nao e visto pelo "(?!.(DTS..." (so olha um caractere adiante).
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

// Filtro so com texto nao ASCII ("日本語" com Exigir): conta como configurado e
// filtra. Antes exigia letra ou digito ASCII e o filtro ficava desligado em
// silencio (Exigir deixava tudo passar). O oficial (/[a-z0-9]/i) tambem nao o
// liga, mas la isso desliga o auto-play; aqui ele tocaria qualquer fonte.
static void naoAscii(void) {
  FonteRegraCfg c = cfg(FR_ESCOPO_TODAS, FR_REGEX_EXIGIR, 1);
  fonteregra_limpar(0); fonteregra_limpar(1);
  assert(fonteregra_regex_testar("日本語", "Film 日本語 1080p") == 1);
  assert(fonteregra_regex_testar("日本語", "Film English 1080p") == 0);
  assert(fonteregra_regex_testar("[ñÑ]", "Espa\xC3\xB1ol") == 1);
  assert(fonteregra_regex_testar("🇪🇸|🇲🇽", "Pelicula 🇲🇽") == 1);
  assert(fonteregra_definir_regex("日本語") == 1);
  assert(fonteregra_regex_estado() == 1);
  assert(fonteregra_ativa(&c));
  assert(fonteregra_grupo(&c, "A", 0, "Film 日本語 1080p") == 0);
  assert(fonteregra_grupo(&c, "A", 0, "Film English 1080p") == -1);   // Exigir filtra
  assert(fonteregra_definir_regex("") == 0);
  // Sem letra, digito nem texto nao ASCII continua "nao configurado".
  assert(fonteregra_regex_testar("|||", "a") == -1);
  assert(fonteregra_regex_testar("^$", "a") == -1);
}

// Preferir: as flags do padrao ((?s), (?m), (?i)) valem tambem para cada condicao
// compilada sozinha. Antes so o "(?i)" era refeito, e "x\nA" (A so depois de uma
// quebra de linha) falhava a condicao do A mesmo com (?s).
static void flagsNasCondicoes(void) {
  FonteRegraCfg c = cfg(FR_ESCOPO_TODAS, FR_REGEX_PREFERIR, 1);
  fonteregra_limpar(0); fonteregra_limpar(1);
  // dotAll: o ponto atravessa a quebra de linha, entao A esta em "x\nA".
  assert(fonteregra_definir_regex("(?s)^(?=.*A)(?=.*B)") == 1);
  assert(fonteregra_grupo(&c, "T", 0, "x\nA B") == 0);     // cumpre as duas
  assert(fonteregra_grupo(&c, "T", 0, "x\nA") == 1);       // so a do A (falta o B)
  assert(fonteregra_grupo(&c, "T", 0, "x\nB") == 1);       // so a do B
  assert(fonteregra_grupo(&c, "T", 0, "x\ny") == 2);       // nenhuma
  // multiline: o "^" casa depois de cada quebra de linha.
  assert(fonteregra_definir_regex("(?m)^(?=.*A)(?=.*B)") == 1);
  assert(fonteregra_grupo(&c, "T", 0, "x\nA B") == 0);
  assert(fonteregra_grupo(&c, "T", 0, "x\nA y") == 1);     // A na 2a linha, falta o B
  assert(fonteregra_grupo(&c, "T", 0, "x\ny") == 2);
  // as duas juntas, na ordem que for, e com (?i) misturado.
  assert(fonteregra_definir_regex("(?i)(?ms)^(?=.*A)(?![\\s\\S]*CAM)(?=.*B)") == 1);
  assert(fonteregra_grupo(&c, "T", 0, "x\na b") == 0);
  assert(fonteregra_grupo(&c, "T", 0, "x\na") == 1);
  assert(fonteregra_grupo(&c, "T", 0, "x\na b CAM") == 1);
  // Sem flags continua como era: o ponto nao atravessa a quebra de linha.
  assert(fonteregra_definir_regex("^(?=.*A)(?=.*B)") == 1);
  assert(fonteregra_grupo(&c, "T", 0, "x\nA B") == 2);
  assert(fonteregra_definir_regex("") == 0);
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
  assert(fonteregra_grupo(&c, "Torrentio", 0, "x") == FR_GRUPO_OUTROS);    // os outros: depois
  // A lista de add-ons nao filtra plugin (listas independentes, como o oficial).
  assert(fonteregra_grupo(&c, "MegaEmbed", 1, "x") == 0);
  // Sem "usar os outros": fora da lista nunca toca.
  c.usarOutros = 0;
  assert(fonteregra_grupo(&c, "Torrentio", 0, "x") == -1);
  assert(fonteregra_grupo_pendente(&c, "Torrentio", 0) == -1);
  assert(fonteregra_grupo_pendente(&c, "Cuevana ES", 0) == 0);
  c.usarOutros = 1;
  assert(fonteregra_grupo_pendente(&c, "Torrentio", 0) == FR_GRUPO_OUTROS);

  // Escopo: so add-ons tira os plugins, E "os outros" NAO os devolve. TCL do
  // dono (2.0.3, 21:24): "Somente add-ons instalados" com "usar os outros"
  // ligado tocou "MegaEmbed - 1080" ("winner other+match"). "Os outros" e a
  // folga das listas de PERMITIDOS; o escopo e filtro: fora dele nunca toca,
  // nem quando nao ha fonte de add-on (a espera segue, ou a lista abre).
  c.escopo = FR_ESCOPO_ADDONS;
  assert(c.usarOutros == 1);
  assert(fonteregra_grupo(&c, "MegaEmbed", 1, "x") == -1);
  assert(fonteregra_grupo_pendente(&c, "MegaEmbed", 1) == -1);
  assert(fonteregra_grupo(&c, "Torrentio", 0, "x") == FR_GRUPO_OUTROS);   // os outros add-ons: sim
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
  assert(fonteregra_grupo(&c, "MegaEmbed", 1, "x") == FR_GRUPO_OUTROS);
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
  assert(fonteregra_grupo(&c, "Torrentio", 0, "1080p Castellano") == FR_GRUPO_OUTROS);
  assert(fonteregra_grupo(&c, "Torrentio", 0, "1080p English") == FR_GRUPO_OUTROS + 1);
  // Regex EXIGIR: filtro, em qualquer grupo.
  c.regexModo = FR_REGEX_EXIGIR;
  assert(fonteregra_grupo(&c, "Cuevana ES", 0, "1080p English") == -1);
  assert(fonteregra_grupo(&c, "Torrentio", 0, "1080p Castellano") == FR_GRUPO_OUTROS);
  // Regex invalida: ignorada (nada vira -1 por causa dela).
  assert(fonteregra_definir_regex("(ESP") == -1);
  assert(fonteregra_regex_estado() == -1);
  assert(fonteregra_grupo(&c, "Cuevana ES", 0, "1080p English") == 0);
  assert(fonteregra_definir_regex("") == 0);
}

// PREFERIR PELA DISTANCIA: num padrao "^(?=A)(?!B)...", cada lookaround do topo
// e uma condicao; quem falha menos vem antes. Assim ha sempre uma escolha, e a
// mais perto do que a pessoa pediu.
static void distancia(void) {
  FonteRegraCfg c = cfg(FR_ESCOPO_TODAS, FR_REGEX_PREFERIR, 1);
  fonteregra_limpar(0); fonteregra_limpar(1);   // todos permitidos
  // 3 condicoes: resolucao boa, nada de CAM, DV so com HDR.
  const char *r = "^(?=[\\s\\S]*\\b(2160p|1080p)\\b)(?![\\s\\S]*\\bCAM\\b)"
                  "(?!(?=[\\s\\S]*\\bDV\\b)(?![\\s\\S]*\\bHDR))";
  assert(fonteregra_definir_regex(r) == 1);
  assert(fonteregra_grupo(&c, "A", 0, "Film 1080p BluRay") == 0);          // cumpre tudo
  assert(fonteregra_grupo(&c, "A", 0, "Film N/A BluRay") == 1);            // sem resolucao
  assert(fonteregra_grupo(&c, "A", 0, "Film 2160p DV") == 1);              // DV sem HDR
  assert(fonteregra_grupo(&c, "A", 0, "Film N/A CAM") == 2);               // duas
  assert(fonteregra_grupo(&c, "A", 0, "Film N/A CAM DV") == 3);            // as tres
  assert(fonteregra_grupo(&c, "A", 0, "Film 2160p DV HDR10") == 0);
  // Os outros add-ons: a mesma conta depois de todos os permitidos.
  fonteregra_alternar(0, "A");
  assert(fonteregra_grupo(&c, "B", 0, "Film 1080p BluRay") == FR_GRUPO_OUTROS);
  assert(fonteregra_grupo(&c, "B", 0, "Film N/A CAM") == FR_GRUPO_OUTROS + 2);
  assert(fonteregra_grupo_pendente(&c, "B", 0) == FR_GRUPO_OUTROS);
  fonteregra_alternar(0, "A");
  // Exigir continua filtro: so quem cumpre tudo.
  c.regexModo = FR_REGEX_EXIGIR;
  assert(fonteregra_grupo(&c, "A", 0, "Film 1080p BluRay") == 0);
  assert(fonteregra_grupo(&c, "A", 0, "Film N/A BluRay") == -1);
  c.regexModo = FR_REGEX_PREFERIR;
  // Com "(?i)" na frente e colchete com ")" dentro, as partes sao as mesmas.
  assert(fonteregra_definir_regex("(?i)^(?=[\\s\\S]*[)]x)(?![\\s\\S]*\\(y\\))(?=[\\s\\S]*z)") == 1);
  assert(fonteregra_grupo(&c, "A", 0, ")x z") == 0);
  assert(fonteregra_grupo(&c, "A", 0, ")x (y) z") == 1);
  assert(fonteregra_grupo(&c, "A", 0, "(y)") == 3);
  // Sem a forma "^(?..)(?..)": uma condicao so (casa = 0, nao casa = 1).
  assert(fonteregra_definir_regex("ESP|Latino") == 1);
  assert(fonteregra_grupo(&c, "A", 0, "1080p Latino") == 0);
  assert(fonteregra_grupo(&c, "A", 0, "1080p English") == 1);
  assert(fonteregra_definir_regex("(?=[\\s\\S]*a)(?=[\\s\\S]*b)") == 1);   // sem "^": inteira
  assert(fonteregra_grupo(&c, "A", 0, "c") == 1);
  assert(fonteregra_definir_regex("^(?=[\\s\\S]*a)(?=[\\s\\S]*b)c") == 1); // texto solto no fim
  assert(fonteregra_grupo(&c, "A", 0, "x") == 1);
  // Mais condicoes que FR_FALTAS_MAX: o grupo para no teto (nao invade "os outros").
  assert(fonteregra_definir_regex("^(?=a)(?=b)(?=c)(?=d)(?=e)(?=f)(?=g)(?=h)(?=i)(?=j)") == 1);
  assert(fonteregra_grupo(&c, "A", 0, "zzz") == FR_FALTAS_MAX);
  // FALTA 0 SO QUANDO A REGEX INTEIRA CASA: as regex do dono em textos de fonte.
  { static const char *const P[] = {
      "^(?![\\s\\S]*\\b((HD)?(CAM(Rip)?|T[SC])|TELE(SYNC|CINE)|(DVD)?SCR(EENER)?|R5|LQ|WORKPRINT|XviD|DivX|HC|HCSUB|"
      "HARDSUBS?|KORSUBS?|3D|H(alf)?-?(SBS|OU)|SBS|(AI[\\s.-]?)?Upscaled?|SAMPLE|(240|360|480|576)p)\\b)"
      "(?!(?=[\\s\\S]*\\b(DV|DoVi|Dolby[\\s.]?Vision)\\b)(?![\\s\\S]*\\b(HDR|HLG)))",
      "^(?=[\\s\\S]*\\b(2160p|4K|UHD|1080p|FHD|(?<![-\\w])HD|720p|QHD|1440p)\\b)(?![\\s\\S]*\\b(CAM|TS|LQ)\\b)"
      "(?!(?=[\\s\\S]*\\b(DV|Dolby[\\s.]?Vision)\\b)(?![\\s\\S]*\\b(HDR|HLG)))" };
    static const char *const T[] = {
      "AIOStreams \xF0\x9F\x9A\x80" "FHD | SDR\n\xF0\x9F\x8E\xA5 BluRay\n\xF0\x9F\x8E\xA7 DTS-HD MA Movie.1080p.BluRay.mkv",
      "AIOStreams \xE2\x9D\x93 N/A\n\xF0\x9F\x8E\xA5 WEB-DL Movie.WEB-DL.mkv",
      "AIOStreams \xF0\x9F\x94\xA5UHD | DV\n\xF0\x9F\x93\xBA DV Movie.2160p.WEB-DL.DV.mkv",
      "AIOStreams \xF0\x9F\x92\xA9LQ | SDR Movie.480p.CAM.DV.mkv",
      "Torrentio 4k DV HDR Movie.2160p.REMUX.DV.HDR10.TrueHD",
      "Torrentio Movie.2003.576p.DVDRip.TS" };
    unsigned p, t;
    for (p = 0; p < sizeof P / sizeof *P; p++) {
      assert(fonteregra_definir_regex(P[p]) == 1);
      for (t = 0; t < sizeof T / sizeof *T; t++) {
        int g = fonteregra_grupo(&c, "A", 0, T[t]);
        assert(g >= 0 && g <= FR_FALTAS_MAX);
        assert((g == 0) == (fonteregra_regex_testar(P[p], T[t]) == 1));
      }
    }
    // A reject-only: so a LQ/CAM/DV falha as duas condicoes.
    assert(fonteregra_definir_regex(P[0]) == 1);
    assert(fonteregra_grupo(&c, "A", 0, T[3]) == 2);
    assert(fonteregra_grupo(&c, "A", 0, T[2]) == 1);
    assert(fonteregra_grupo(&c, "A", 0, T[1]) == 0); }
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

// PR #489: filtro so com caractere nao ASCII continua ativo (Exigir recusa o
// que nao casa) e o Preferir conta as condicoes com as flags (s/m) do padrao.
static void pr489(void) {
  FonteRegraCfg c = cfg(FR_ESCOPO_TODAS, FR_REGEX_EXIGIR, 1);
  fonteregra_limpar(0); fonteregra_limpar(1);
  assert(fonteregra_definir_regex("日本語") == 1 && fonteregra_regex_estado() == 1);
  assert(fonteregra_grupo(&c, "A", 0, "English") == -1);
  assert(fonteregra_grupo(&c, "A", 0, "Film 日本語") == 0);
  assert(fonteregra_definir_regex("[ñÑ]") == 1 && fonteregra_grupo(&c, "A", 0, "English") == -1);
  assert(fonteregra_definir_regex("🇪🇸|🇲🇽") == 1 && fonteregra_grupo(&c, "A", 0, "English") == -1);
  c.regexModo = FR_REGEX_PREFERIR;
  assert(fonteregra_definir_regex("(?s)^(?=.*A)(?=.*B)(?=.*C)") == 1);
  assert(fonteregra_grupo(&c, "A", 0, "\nABC") == 0);
  assert(fonteregra_grupo(&c, "A", 0, "\nAB") == 1);   // so C falta
  assert(fonteregra_grupo(&c, "A", 0, "A") == 2);
  assert(fonteregra_definir_regex("") == 0);
}

int main(void) {
  const char *d = getenv("NUVIO_DADOS");
  assert(d && *d);
  dados_iniciar(d);
  assert(!strcmp(dados_dir(), d));
  traducao();
  regex();
  lookahead();
  naoAscii();
  flagsNasCondicoes();
  grupos();
  distancia();
  pr489();
  perfilEConta();
  ordem();
  puts("fonteregra: ok");
  return 0;
}
