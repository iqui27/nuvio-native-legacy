// O CAMPO "Padrão do filtro" de ponta a ponta, no codigo de verdade: padrao
// guardado -> campo aberto -> texto do celular (ou nada) -> "pronto" ->
// fonteregra.txt -> relido. Compila o app inteiro com -DAJUSTES_TESTE, sem
// janela. Os casos sao os que quebravam:
//   - o modelo de espanhol colado do celular perdia "ñ" e as bandeiras e virava
//     "Espa(|n)ol|...|LATAM||": a alternativa vazia casava com TUDO;
//   - uma regex de 400-500 caracteres (o oficial guarda ate 500) era cortada em
//     400 so de abrir o campo e dar OK;
//   - o "apagar" tirava um byte de um caractere UTF-8.
//
//   bash tests/regex_campo.sh
#include "dados.h"
#include "fonteregra.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ajustes_teste_regex_campo(const char *colado, int apagar, char *lido, size_t n);

static int falhas;
#define OK(c, m) do { if (c) printf("ok   %s\n", m); else { printf("FALHA %s\n", m); falhas++; } } while (0)

static int utf8Valido(const char *s) {
  const unsigned char *p = (const unsigned char *)s;
  while (*p) {
    int l = *p < 0x80 ? 1 : *p >= 0xC2 && *p < 0xE0 ? 2 : *p >= 0xE0 && *p < 0xF0 ? 3 : *p >= 0xF0 && *p < 0xF5 ? 4 : 0, i;
    if (!l) return 0;
    for (i = 1; i < l; i++) if ((p[i] & 0xC0) != 0x80) return 0;
    p += l;
  }
  return 1;
}

int main(void) {
  char lido[FR_REGEX_MAX + 64], longo[1200], esperado[FR_REGEX_MAX + 1];
  const char *esp = fonteregra_modelo(1);   // o modelo de espanhol, com ñ e bandeiras
  int e, k;
  dados_iniciar(NULL);   // NUVIO_DADOS: diretorio temporario do .sh
  fonteregra_carregar();

  // 1. O modelo de espanhol colado do celular chega inteiro e NAO casa com tudo.
  e = ajustes_teste_regex_campo(esp, 0, lido, sizeof lido);
  printf("  guardado: %s\n", lido);
  OK(e == 1 && !strcmp(lido, esp), "modelo de espanhol pelo celular: guardado byte a byte");
  OK(fonteregra_regex_testar(lido, "Movie 1080p English") == 0, "...e nao casa com uma fonte em ingles");
  OK(fonteregra_regex_testar(lido, "Película Español 1080p") == 1, "...e casa com \"Español\"");
  OK(fonteregra_regex_testar(lido, "Torrentio 🇲🇽 1080p") == 1, "...e casa com a bandeira");

  // 2. Regex de 450 vinda da conta: abrir e dar OK nao corta.
  k = snprintf(longo, sizeof longo, "^(?=.*\\b(2160p|1080p)\\b)(?!.*\\b(DTS|TrueHD|CAM)\\b)(?=.*(");
  while (k < 440) k += snprintf(longo + k, sizeof longo - (size_t)k, "Grupo%03d|", k);
  k += snprintf(longo + k, sizeof longo - (size_t)k, "FGT))");
  printf("  regex longa de %d caracteres\n", k);
  OK(k > 400 && k <= FR_REGEX_MAX, "(a regex de teste passa de 400 e cabe em 500)");
  OK(fonteregra_definir_regex(longo) == 1, "regex longa guardada (como viria da conta)");
  e = ajustes_teste_regex_campo(NULL, 0, lido, sizeof lido);
  OK(e == 1 && !strcmp(lido, longo), "abrir o campo e dar OK mantem os mais de 400 caracteres");
  OK(fonteregra_regex_testar(lido, "Movie 2160p WEB-DL DDP-FGT") == 1 &&
     fonteregra_regex_testar(lido, "Movie 2160p DTS-HD-FGT") == 0, "...e ela continua funcionando");

  // 3. Colado do celular com mais que o teto do oficial (500): corta em 500, num
  //    limite de caractere (nunca no meio de um "ñ").
  k = 0;
  while (k < 497) longo[k++] = 'a';
  strcpy(longo + k, "ññññ|b");        // "ñ" ocupa 2 bytes: o corte cai entre dois
  e = ajustes_teste_regex_campo(longo, 0, lido, sizeof lido);
  printf("  colado de %zu bytes -> guardado %zu bytes\n", strlen(longo), strlen(lido));
  OK(strlen(lido) <= FR_REGEX_MAX && strlen(lido) >= FR_REGEX_MAX - 1, "colado acima de 500: corta no teto");
  OK(utf8Valido(lido), "...sem cortar um caractere UTF-8 no meio");

  // 4. Uma regex no estilo da do dono, 400 caracteres exatos, pelo celular:
  //    chega identica e responde como o oficial.
  k = snprintf(esperado, sizeof esperado, "%s",
               "^(?=.*\\b(2160p|4K|UHD|1080p|FHD|BD|HD|720p|SDR)\\b)(?=.*\\b(SeaDex|Remux|BD-Remux|Dual[\\s-]?Audio|"
               "Multi[\\s-]?Audio|Multi|Dub|Eng|Jap|10bit|HEVC|x265|H265|Hi10P|BluRay|WEB-DL|WEBRip|BDRip|BRRip|x264|AVC|"
               "H264|8bit|HDTV|DVDRip|HDRip|Subs)\\b)(?!.*\\b(DTS|DTS-HD|DTS:X)\\b)(?!(?=.*\\b(DV|Dolby\\s?Vision)\\b)"
               "(?!.*\\b(HDR10|HDR|Multi)\\b))(?!.*\\b(TrueHD|MLP|CAM|HDCAM)\\b)(?=.*(Torrentio|Comet|AIO|MediaFusion|");
  while (k < 398) esperado[k++] = 'Q';   // a ultima alternativa completa os 400
  esperado[k++] = ')'; esperado[k++] = ')'; esperado[k] = 0;
  printf("  regex de %zu caracteres\n", strlen(esperado));
  e = ajustes_teste_regex_campo(esperado, 0, lido, sizeof lido);
  OK(strlen(esperado) == 400 && e == 1 && !strcmp(lido, esperado), "regex de 400 caracteres pelo celular: identica e valida");
  OK(fonteregra_regex_testar(lido, "Torrentio 4k HDR Movie.2160p.BluRay.REMUX.HDR10.HEVC.DDP.5.1-FGT") == 1,
     "...casa com a fonte boa");
  OK(fonteregra_regex_testar(lido, "Torrentio 4k Movie.2160p.BluRay.REMUX.HEVC.DTS-HD.MA.7.1-FGT") == 0,
     "...recusa DTS");
  OK(fonteregra_regex_testar(lido, "Torrentio\n4k HDR Movie.2160p.BluRay.REMUX.HDR10.HEVC.DDP.5.1-FGT") == 0,
     "...e, como o oficial, o \"^(?=.*\" so ve a primeira linha");

  // 5. "apagar" tira um CARACTERE: "Español|🇪🇸" menos 2 = "Español|" + nada da bandeira.
  OK(fonteregra_definir_regex("Español|🇪🇸") == 1, "padrao com ñ e bandeira guardado");
  e = ajustes_teste_regex_campo(NULL, 2, lido, sizeof lido);   // a bandeira sao 2 caracteres
  printf("  depois de 2x apagar: %s\n", lido);
  OK(!strcmp(lido, "Español|") && utf8Valido(lido), "apagar 2x tira as duas metades da bandeira, inteiras");
  e = ajustes_teste_regex_campo(NULL, 4, lido, sizeof lido);   // "|", "l", "o", "ñ"
  OK(!strcmp(lido, "Espa") && utf8Valido(lido), "apagar tira o \"ñ\" inteiro (2 bytes)");

  // 6. Vazio apaga.
  e = ajustes_teste_regex_campo("", 0, lido, sizeof lido);
  OK(!lido[0] && e == 0, "vazio apaga");

  printf("%s\n", falhas ? "regex_campo: FALHOU" : "regex_campo: ok");
  return falhas ? 1 : 0;
}
