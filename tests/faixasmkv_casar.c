// Ver tests/faixasmkv_casar.sh. Audio: a TV lista MENOS faixas que o arquivo
// (esconde DTS/TrueHD que nao decodifica); o rotulo do cabecalho do MKV tem de
// chegar na faixa certa, e so nela.
#include "../src/faixasmkv.h"
#include "../src/linguas.h"
#include <stdio.h>
#include <string.h>

const char *i18n(const char *s) { return s; }

static int falhas;
static void ok(int c, const char *o, const char *v) {
  printf("  %-60s %s  [%s]\n", o, c ? "ok" : "FALHOU", v ? v : "");
  if (!c) falhas++;
}
static void mkv(MkvFaixa *m, int tipo, const char *codec, const char *idioma, const char *nome, int canais) {
  memset(m, 0, sizeof *m);
  m->tipo = tipo;
  snprintf(m->codec, sizeof m->codec, "%s", codec);
  snprintf(m->idioma, sizeof m->idioma, "%s", idioma);
  snprintf(m->nome, sizeof m->nome, "%s", nome);
  m->canais = canais;
}
// Como o nv_tpk_video_faixa: so o idioma do player; sem codec nem canais.
static void tv(VideoFaixa *f, int numero, const char *idioma) {
  memset(f, 0, sizeof *f);
  f->numero = numero; f->ordinalMkv = -1;
  snprintf(f->idioma, sizeof f->idioma, "%s", idioma);
  snprintf(f->rotulo, sizeof f->rotulo, "TV%d", numero + 1);
}

int main(void) {
  MkvFaixa fx[8];
  VideoFaixa a[4];

  // 1) MKV [DTS eng 5.1, AC3 por 5.1], TV lista 1 [por]: a portuguesa ganha o rotulo do arquivo.
  mkv(&fx[0], 1, "V_MPEG4/ISO/AVC", "", "", 0);
  mkv(&fx[1], 2, "A_DTS", "eng", "", 6);
  mkv(&fx[2], 2, "A_AC3", "por", "", 6);
  tv(&a[0], 0, "por");
  faixasmkv_aplicar(a, 1, NULL, 0, fx, 3);
  ok(strstr(a[0].rotulo, "AC3") && strstr(a[0].rotulo, "5.1") && !strstr(a[0].rotulo, "TV1"),
     "DTS eng + AC3 por, TV [por]: rotulo do MKV", a[0].rotulo);
  ok(!strcmp(a[0].codec, "A_AC3") && a[0].canais == 6, "codec e canais vem do arquivo", a[0].codec);

  // O mesmo com o "pt" que o player da Samsung entrega.
  tv(&a[0], 0, "pt");
  faixasmkv_aplicar(a, 1, NULL, 0, fx, 3);
  ok(strstr(a[0].rotulo, "AC3") != NULL, "TV diz \"pt\", arquivo \"por\"", a[0].rotulo);

  // Escondida no meio: [DTS eng, AC3 por, AAC jpn], TV [por, jpn].
  mkv(&fx[3], 2, "A_AAC", "jpn", "Dub", 2);
  tv(&a[0], 0, "por"); tv(&a[1], 1, "ja");
  faixasmkv_aplicar(a, 2, NULL, 0, fx, 4);
  ok(strstr(a[0].rotulo, "AC3") && strstr(a[1].rotulo, "Dub") && strstr(a[1].rotulo, "2.0"),
     "escondida na frente, 2 de 3: cada uma no seu", a[1].rotulo);

  // 2) Ambiguo: duas faixas por no arquivo, a TV lista uma por: nao adivinha.
  mkv(&fx[1], 2, "A_DTS", "eng", "", 6);
  mkv(&fx[2], 2, "A_AC3", "por", "", 6);
  mkv(&fx[3], 2, "A_AC3", "por", "Comentarios", 2);
  tv(&a[0], 0, "por");
  faixasmkv_aplicar(a, 1, NULL, 0, fx, 4);
  ok(!strcmp(a[0].rotulo, "TV1") && !a[0].codec[0], "duas por candidatas: fica o rotulo da TV", a[0].rotulo);

  // Ambiguo pelo idioma vazio da TV (log da QE65Q80A: audio sem idioma).
  tv(&a[0], 0, "");
  faixasmkv_aplicar(a, 1, NULL, 0, fx, 4);
  ok(!strcmp(a[0].rotulo, "TV1"), "TV sem idioma, 3 candidatas: fica o da TV", a[0].rotulo);

  // Desempate pela ordem so quando e inequivoco: TV [por, por], arquivo [DTS eng, AC3 por, AC3 por].
  tv(&a[0], 0, "por"); tv(&a[1], 1, "por");
  faixasmkv_aplicar(a, 2, NULL, 0, fx, 4);
  ok(strstr(a[0].rotulo, "5.1") && strstr(a[1].rotulo, "Comentarios"),
     "TV [por,por] e arquivo com 2 por: pela ordem", a[1].rotulo);

  // Sem nenhuma correspondente: tudo como veio.
  tv(&a[0], 0, "fr");
  faixasmkv_aplicar(a, 1, NULL, 0, fx, 4);
  ok(!strcmp(a[0].rotulo, "TV1"), "idioma que o arquivo nao tem: fica o da TV", a[0].rotulo);

  // Duas da TV disputam a unica por do arquivo: nenhuma leva (nao da para saber qual).
  mkv(&fx[1], 2, "A_DTS", "eng", "", 6);
  mkv(&fx[2], 2, "A_AC3", "por", "", 6);
  mkv(&fx[3], 2, "A_AC3", "eng", "", 6);
  tv(&a[0], 0, "por"); tv(&a[1], 1, "pt");
  faixasmkv_aplicar(a, 2, NULL, 0, fx, 4);
  ok(!strcmp(a[0].rotulo, "TV1") && !strcmp(a[1].rotulo, "TV2"),
     "TV [por, pt] e um so por no arquivo: nenhuma leva", a[0].rotulo);

  // 3) Contagem igual: ordinal, como sempre (a TV nao decide, o arquivo manda).
  mkv(&fx[1], 2, "A_EAC3", "eng", "", 6);
  mkv(&fx[2], 2, "A_AC3", "por", "", 2);
  tv(&a[0], 0, "por"); tv(&a[1], 1, "en");   // a ordem do player manda: a[0] <- fx[1]
  faixasmkv_aplicar(a, 2, NULL, 0, fx, 3);
  ok(strstr(a[0].rotulo, "E-AC3") && strstr(a[1].rotulo, "AC3") && !strstr(a[1].rotulo, "E-AC3"),
     "contagem igual: ordinal, inalterado", a[0].rotulo);

  // #421: player 29 / arquivo 37. Arabic unico deve ganhar codec e ordinal;
  // English com SDH escondida e portugues regional ambiguo nao se adivinham.
  mkv(&fx[0], 17, "S_TEXT/UTF8", "en", "", 0);
  mkv(&fx[1], 17, "S_TEXT/UTF8", "en", "SDH", 0);
  mkv(&fx[2], 17, "S_TEXT/UTF8", "ar", "Arabic", 0);
  mkv(&fx[3], 17, "S_TEXT/UTF8", "ca", "", 0);
  tv(&a[0], 0, "en"); tv(&a[1], 1, "ara");
  faixasmkv_aplicar(NULL, 0, a, 2, fx, 4);
  ok(a[1].ordinalMkv == 2 && !strcmp(a[1].codec, "S_TEXT/UTF8"),
     "#421 Arabic unico: ordinal 2 do arquivo, mesmo com lista menor", a[1].codec);
  ok(a[0].ordinalMkv == -1 && !a[0].codec[0], "English/SDH ambiguo continua na TV", a[0].codec);
  tv(&a[0], 0, ""); tv(&a[1], 1, "ar"); tv(&a[2], 2, "ara");
  faixasmkv_aplicar(NULL, 0, a, 3, fx, 4);
  ok(a[0].ordinalMkv == -1 && a[1].ordinalMkv == -1 && a[2].ordinalMkv == -1,
     "sem idioma ou duas TV disputando Arabic: nao casa", "");

  // Segunda sonda: PGS ja anexada (numero < 0) nao entra na contagem do player.
  // Duas legendas de texto no mesmo idioma: o ordinal so fecha sem as linhas de imagem.
  {
    VideoFaixa leg[4];
    mkv(&fx[0], 17, "S_HDMV/PGS", "eng", "", 0);
    mkv(&fx[1], 17, "S_TEXT/UTF8", "eng", "Full", 0);
    mkv(&fx[2], 17, "S_TEXT/UTF8", "eng", "SDH", 0);
    tv(&leg[0], 0, "eng"); tv(&leg[1], 1, "eng");
    faixasmkv_aplicar(NULL, 0, leg, 2, fx, 3);
    ok(leg[0].ordinalMkv == 1 && leg[1].ordinalMkv == 2 &&
       !strcmp(leg[0].codec, "S_TEXT/UTF8") && !strcmp(leg[1].codec, "S_TEXT/UTF8"),
       "1a sonda: duas eng de texto, ordinal pulando a PGS", leg[0].codec);
    leg[2].numero = -1; leg[2].ordinalMkv = 0;
    snprintf(leg[2].codec, sizeof leg[2].codec, "S_HDMV/PGS");
    faixasmkv_aplicar(NULL, 0, leg, 3, fx, 3);
    ok(leg[0].ordinalMkv == 1 && leg[1].ordinalMkv == 2 &&
       !strcmp(leg[0].codec, "S_TEXT/UTF8") && !strcmp(leg[1].codec, "S_TEXT/UTF8"),
       "2a sonda com PGS anexada: texto continua casado", leg[0].codec);
    ok(leg[2].ordinalMkv == 0 && !strcmp(leg[2].codec, "S_HDMV/PGS"),
       "2a sonda nao reescreve a linha de imagem", leg[2].codec);
    // Idioma unico, com a PGS ja anexada e o mesmo idioma da faixa de texto.
    mkv(&fx[0], 17, "S_HDMV/PGS", "por", "", 0);
    mkv(&fx[1], 17, "S_TEXT/UTF8", "por", "", 0);
    tv(&leg[0], 0, "por");
    leg[1].numero = -1; leg[1].ordinalMkv = 0;
    snprintf(leg[1].codec, sizeof leg[1].codec, "S_HDMV/PGS");
    leg[1].idioma[0] = 0;
    faixasmkv_aplicar(NULL, 0, leg, 2, fx, 2);
    ok(leg[0].ordinalMkv == 1 && !strcmp(leg[0].codec, "S_TEXT/UTF8"),
       "2a sonda: idioma unico nao conta a PGS anexada", leg[0].codec);
  }

  printf("[faixasmkv_idiomas_coerentes] SEM_IMAGEM nao pode contradizer o idioma\n");
  {
    MkvFaixa fx2[2]; VideoFaixa lv[2]; int idx[2] = { 0, 1 };
    mkv(&fx2[0], 17, "S_TEXT/ASS", "eng", "", 0);
    mkv(&fx2[1], 17, "S_TEXT/ASS", "por", "", 0);
    tv(&lv[0], 0, "en"); tv(&lv[1], 1, "pt");
    ok(faixasmkv_idiomas_coerentes(lv, 2, fx2, idx), "idiomas iguais (en/eng, pt/por): coerente", "");
    tv(&lv[1], 1, "es");
    ok(!faixasmkv_idiomas_coerentes(lv, 2, fx2, idx), "TV diz es, arquivo diz por: incoerente", "");
    tv(&lv[1], 1, "");
    ok(faixasmkv_idiomas_coerentes(lv, 2, fx2, idx), "TV sem idioma (LG): nao contradiz", "");
    idx[1] = -1; tv(&lv[1], 1, "es");
    ok(faixasmkv_idiomas_coerentes(lv, 2, fx2, idx), "faixa sem par: ignorada", "");
  }

  printf(falhas ? "faixasmkv_casar: %d FALHA(S)\n" : "faixasmkv_casar: tudo ok\n", falhas);
  return falhas != 0;
}
