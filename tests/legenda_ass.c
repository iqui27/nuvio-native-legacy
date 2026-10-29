// Regressao do parser ASS/SSA (#92). Sem SDL, sem rede: le as fixtures de
// tests/fixtures/ass e confere o que a tela vai receber por legenda_cues.
#include "legenda.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Stub de rede.h: legenda.c chama rede_baixar no fio de download, que este
// teste nunca dispara.
char *rede_baixar(const char *url, int segundos) { (void)url; (void)segundos; return NULL; }
char *rede_baixar_bin(const char *url, int segundos, long *n) { (void)url; (void)segundos; (void)n; return NULL; }
const char *i18n(const char *s) { return s; }

static int falhas;
#define OK(cond, ...) do { if (cond) printf("ok   " __VA_ARGS__); else { printf("FALHA " __VA_ARGS__); falhas++; } printf("\n"); } while (0)

static char *ler(const char *nome) {
  char caminho[400]; FILE *f; long n; char *s;
  snprintf(caminho, sizeof caminho, "tests/fixtures/ass/%s", nome);
  f = fopen(caminho, "rb"); if (!f) { printf("FALHA abrir %s\n", caminho); exit(1); }
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  s = malloc((size_t)n + 1); if (fread(s, 1, (size_t)n, f) != (size_t)n) exit(1); s[n] = 0; fclose(f);
  return s;
}
static int semTagCrua(const LegendaCue *v, int n) {
  int i; for (i = 0; i < n; i++) if (strchr(v[i].texto, '{') || strchr(v[i].texto, '\\')) return 0;
  return 1;
}

int main(void) {
  char *s; LegendaCue *v; int n;

  s = ler("simples.ass");
  OK(legenda_eh_ass(s), "simples: reconhecido como ASS");
  OK(!legenda_eh_ass("1\n00:00:01,000 --> 00:00:02,000\noi\n"), "SRT nao e ASS");
  n = legenda_extrair_ass(s, &v);
  OK(n == 3, "simples: 3 Dialogue, Comment fora (n=%d)", n);
  OK(n == 3 && !strcmp(v[0].texto, "Primeira fala"), "simples: texto do primeiro");
  OK(n == 3 && !strcmp(v[1].texto, "Segunda\nlinha dois"), "simples: \\N vira quebra de linha");
  OK(n == 3 && v[0].inicio == 1.0 && v[0].fim == 3.0, "simples: tempos");
  OK(n == 3 && v[0].cor == 0xFFFFFF && v[2].cor == 0x4080FF, "simples: cor do estilo (&HBBGGRR -> RGB): Default branco, Narrador laranja (c0=%x c2=%x)", n==3?v[0].cor:0, n==3?v[2].cor:0);
  OK(n == 3 && v[2].italico == 1 && v[0].italico == 0, "simples: italico do estilo");
  OK(n == 3 && v[0].resX == 1280 && v[0].resY == 720, "simples: PlayRes");
  OK(semTagCrua(v, n), "simples: nenhuma tag crua");
  free(v); free(s);

  s = ler("posicionado.ass");
  n = legenda_extrair_ass(s, &v);
  OK(n == 4, "posicionado: 4 Dialogue (n=%d)", n);
  OK(n == 4 && v[1].an == 8, "posicionado: \\an8 (an=%d)", n==4?v[1].an:0);
  OK(n == 4 && v[1].negrito == 1 && v[1].cor == 0xFFFF00, "posicionado: estilo Letreiro negrito amarelo (cor=%x)", n==4?v[1].cor:0);
  OK(n == 4 && v[2].posX == 640 && v[2].posY == 100, "posicionado: \\pos (%.0f,%.0f)", n==4?v[2].posX:0, n==4?v[2].posY:0);
  OK(n == 4 && v[2].cor == 0xFF0000, "posicionado: \\c&H0000FF& = vermelho (cor=%x)", n==4?v[2].cor:0);
  OK(n == 4 && !strcmp(v[2].texto, "Texto posicionado"), "posicionado: texto limpo");
  OK(n == 4 && !strcmp(v[3].texto, "Grito normal sussurro"), "posicionado: \\b/\\i tirados do texto (%s)", n==4?v[3].texto:"");
  OK(semTagCrua(v, n), "posicionado: nenhuma tag crua");
  free(v); free(s);

  s = ler("karaoke.ass");
  n = legenda_extrair_ass(s, &v);
  OK(n == 3, "karaoke: 3 Dialogue (n=%d)", n);
  OK(n == 3 && !strcmp(v[0].texto, "Karaoke"), "karaoke: \\k vira texto corrido (%s)", n==3?v[0].texto:"");
  OK(n == 3 && !strcmp(v[1].texto, "Com transformacao"), "karaoke: \\t e \\fad tirados (%s)", n==3?v[1].texto:"");
  OK(n == 3 && v[2].an == 5, "karaoke: \\an5");
  OK(semTagCrua(v, n), "karaoke: nenhuma tag crua");
  free(v); free(s);

  // legenda_extrair reconhece sozinho; SRT continua entrando por ele.
  n = legenda_extrair("1\n00:00:01,000 --> 00:00:02,000\noi\n\n", &v);
  OK(n == 1 && !strcmp(v[0].texto, "oi") && v[0].an == 0 && v[0].cor == -1, "SRT por legenda_extrair: 1 bloco, sem ancora nem cor");
  free(v);

  // #604: SRT/VTT can carry HTML emphasis and SSA overrides. The shared
  // parser must pass clean text and cue emphasis to both subtitle renderers.
  { static const struct { const char *raw, *text; int italic, bold; } cases[] = {
      { "<i>Thought</i>", "Thought", 1, 0 },
      { "<B><i>Strong thought</i></B>", "Strong thought", 1, 1 },
      { "<font color=\"red\"><u>Plain</u></font><br>Next &amp; last", "Plain\nNext & last", 0, 0 },
      { "{\\i1}Thought{\\i0}", "Thought", 1, 0 },
      { "{\\b1\\i1}Strong{\\b0\\i0} plain", "Strong plain", 1, 1 },
      { "{\\an8\\pos(10,20)\\c&HFF&}Plain", "Plain", 0, 0 },
      { "Literal {braces} &lt;3 &gt; 2", "Literal {braces} <3 > 2", 0, 0 },
      { "<i></i>Plain <b> </b>", "Plain  ", 0, 0 },
      { "Broken {\\i1", "Broken {\\i1", 0, 0 }
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
      char body[1024];
      snprintf(body, sizeof body, "1\n00:00:01,000 --> 00:00:03,000\n%s\n\n", cases[i].raw);
      n = legenda_extrair(body, &v);
      OK(n == 1 && !strcmp(v[0].texto, cases[i].text) &&
         v[0].italico == cases[i].italic && v[0].negrito == cases[i].bold &&
         v[0].an == 0 && v[0].cor == -1 && v[0].posX == -1 && v[0].posY == -1,
         "#604 SRT markup case %zu: %s", i, n == 1 ? v[0].texto : "missing cue");
      free(v);
    }
    n = legenda_extrair("WEBVTT\n\n00:01.000 --> 00:03.000\n<i>{\\i1}Thought{\\i0}</i>\n", &v);
    OK(n == 1 && !strcmp(v[0].texto, "Thought") && v[0].italico,
       "#604 VTT: clean emphasized text");
    free(v);
    n = legenda_extrair("[Script Info]\nScriptType: v4.00\n[V4 Styles]\n[Events]\n"
                        "Format: Marked, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
                        "Dialogue: Marked=0,0:00:01.00,0:00:03.00,Default,,0,0,0,,{\\i1}SSA thought\n", &v);
    OK(n == 1 && !strcmp(v[0].texto, "SSA thought") && v[0].italico,
       "#604 SSA: clean emphasized text");
    free(v);
  }

  // VTT compartilha o caminho SRT: cabecalho WEBVTT, identificador opcional e
  // settings depois do fim nao podem virar parte do timestamp.
  n = legenda_extrair("WEBVTT\n\nintro\n00:00:01.000 --> 00:00:02.500 align:start\nola &amp; mundo\n\n", &v);
  OK(n == 1 && !strcmp(v[0].texto, "ola & mundo") && v[0].inicio == 1.0 && v[0].fim == 2.5,
     "VTT: 1 cue, settings e entidades preservados");
  free(v);

  // SSA reduzido sem secoes e com caixa/recuo diferentes ainda e reconhecido
  // como Dialogue, em vez de ser descartado pelo detector ASS.
  n = legenda_extrair("  dialogue: 0,0:00:03.00,0:00:04.00,Default,,0,0,0,,SSA reduzido\n", &v);
  OK(n == 1 && !strcmp(v[0].texto, "SSA reduzido") && legenda_eh_ass("dialogue: 0,0:00:03.00,0:00:04.00,Default,,0,0,0,,x\n"),
     "SSA reduzido: detector case-insensitive e com recuo");
  free(v);

  // A busca de cues e binaria para todos os formatos. SRT/VTT fora de ordem
  // precisa ter a mesma ordenacao que ASS antes de chegar a essa busca.
  const char *foraOrdem = "2\n00:00:10,000 --> 00:00:12,000\nMais tarde\n\n"
                         "1\n00:00:01,000 --> 00:00:03,000\nMais cedo\n\n";
  n = legenda_extrair(foraOrdem, &v);
  OK(n == 2 && v[0].inicio == 1.0 && v[1].inicio == 10.0,
     "SRT fora de ordem: cues ordenados por inicio");
  free(v);
  legenda_definir_corpo(foraOrdem);
  LegendaCue atual;
  OK(legenda_cues(11.0, 0, &atual, 1) == 1 && !strcmp(atual.texto, "Mais tarde"),
     "SRT fora de ordem: fala ativa continua visivel");
  legenda_desligar();

  // NaN passa pelas comparacoes de ordem e envenena a busca binaria; infinito
  // tambem nao representa um instante valido de uma faixa.
  n = legenda_extrair("1\n00:00:nan --> 00:00:05,000\nInvalida\n\n"
                      "2\n00:00:01,000 --> 00:00:inf\nInfinita\n\n"
                      "4\n00:00:erro --> 00:00:05,000\nTruncada\n\n"
                      "3\n00:00:01,000 --> 00:00:03,000\nValida\n\n", &v);
  OK(n == 1 && !strcmp(v[0].texto, "Valida"), "SRT: timestamps nao finitos descartados");
  free(v);
  n = legenda_extrair_ass("Dialogue: 0,0:00:nan,0:00:05.00,Default,,0,0,0,,Invalida\n"
                         "Dialogue: 0,0:00:01.00,0:00:inf,Default,,0,0,0,,Infinita\n"
                         "Dialogue: 0,0:00:01.00,0:00:03.00,Default,,0,0,0,,Valida\n", &v);
  OK(n == 1 && !strcmp(v[0].texto, "Valida"), "ASS: timestamps nao finitos descartados");
  free(v);

  // --- CHARSET: o texto chega ao parser em UTF-8, venha como vier -----------
  { const char *cs; char *u;
    // "Não, você está enganado." em Windows-1252
    static const char c1252[] = "1\r\n00:00:01,000 --> 00:00:02,000\r\nN\xE3o, voc\xEA est\xE1 enganado\x85 \x93sim\x94\r\n";
    u = legenda_utf8(c1252, (long)strlen(c1252), &cs);
    OK(u && !strcmp(cs, "windows-1252") && strstr(u, "Não, você está enganado… “sim”"), "cp1252 vira UTF-8 (%s)", cs);
    n = u ? legenda_extrair(u, &v) : 0;
    OK(n == 1 && strstr(v[0].texto, "você"), "cp1252: o bloco sai com acento");
    if (n) free(v);
    free(u);
    // "Привет, как дела" em Windows-1251
    static const char c1251[] = "\xCF\xF0\xE8\xE2\xE5\xF2, \xEA\xE0\xEA \xE4\xE5\xEB\xE0";
    u = legenda_utf8(c1251, (long)strlen(c1251), &cs);
    OK(u && !strcmp(cs, "windows-1251") && !strcmp(u, "Привет, как дела"), "cp1251 cirilico (%s: %s)", cs, u ? u : "-");
    free(u);
    // Declared language resolves bytes that the legacy detector cannot distinguish.
    static const struct { const char *bytes, *text, *charset, *language, *filename; } extra[] = {
      { "\xC3\xE5\xE9\xE1 \xF3\xEF\xF5 \xEA\xFC\xF3\xEC\xE5", "Γεια σου κόσμε", "windows-1253", "el" },
      { "\xAB\xCA\xE1\xEB\xE7\xEC\xDD\xF1\xE1\xBB, \xE5\xDF\xF0\xE5 \x97 10\x80.", "«КблзмЭсб», еЯре — 10Ђ.", "windows-1251" },
      { "\xC1\xD5\xD4\xCF \xC5\xC9\xCD\xC1\xC9 \xC5\xCB\xCB\xC7\xCD\xC9\xCA\xCF", "БХФП ЕЙНБЙ ЕЛЛЗНЙКП", "windows-1251" },
      { "\xBA\xF3\xF9\xF2 \xBC\xEC\xF9\xF2 \xCA\xE1\xEB\xE7\xEC\xDD\xF1\xE1 \xEA\xFC\xF3\xEC\xE5", "Ίσως Όμως Καλημέρα κόσμε", "windows-1253", "gre" },
      { "\xDDstanbul'da \xFE\xF6yle g\xFCzel", "Ýstanbul'da þöyle güzel", "windows-1252" },
      { "\xDDstanbul", "Ýstanbul", "windows-1252" },
      { "\xDDstanbul, \xFD\xFE\xFD\xF0\xFD g\xF6rd\xFCm. \xD0\xF0 \xDD\xFD \xDE\xFE.", "İstanbul, ışığı gördüm. Ğğ İı Şş.", "windows-1254", "tur" },
      { "\xCF\xF0\xE8\xE2\xE5\xF2, \xEC\xE8\xF0 \x97 \xF1\xF3\xE1\xF2\xE8\xF2\xF0\xFB", "Привет, мир — субтитры", "windows-1251" },
      { "\xD1\xE5\xE3\xEE\xE4\xED\xFF \xF5\xEE\xF0\xEE\xF8\xE8\xE9 \xE4\xE5\xED\xFC. \xDF \xEB\xFE\xE1\xEB\xFE \xF2\xE5\xE1\xFF!", "Сегодня хороший день. Я люблю тебя!", "windows-1251" },
      { "\xC9T\xC9 \xC0 PARIS \x97 No\xEBl", "ÉTÉ À PARIS — Noël", "windows-1252" },
      { "\xDEingvellir \xF0 og \xFE", "Þingvellir ð og þ", "windows-1252" },
      { "\xDE" "a\xF0 er \xFE" "etta.", "Það er þetta.", "windows-1252" },
      { "\xDE" "a\xF0 er n\xFDtt.", "Það er nýtt.", "windows-1252" },
      { "N\xFDtt, n\xFDtt, n\xFDtt!", "Nýtt, nýtt, nýtt!", "windows-1252" },
      { "\xDDmislegt er n\xFDtt.", "Ýmislegt er nýtt.", "windows-1252" },
      { "\xDDttu \xE1 takkann.", "Ýttu á takkann.", "windows-1252" },
      { "\xD3\xEC\xE5\xE9, \xF3\xEC\xE5\xE9, \xF3\xEC\xE5\xE9!", "Умей, умей, умей!", "windows-1251" },
      // Ukrainian є/ї share bytes with accented Greek capitals.
      { "\xD6\xE5 \xBF\xBF \xF3\xEA\xF0\xE0\xBF\xED\xF1\xFC\xEA\xE0 \xEC\xEE\xE2\xE0. \xC2\xEE\xED\xE0 \xF7\xE8\xF2\xE0\xBA \xEA\xED\xE8\xE6\xEA\xE8.", "Це її українська мова. Вона читає книжки.", "windows-1251" },
      // A lone Turkish ş shares its byte with Icelandic þ: preserve the fallback.
      { "Her \xFE" "ey \xE7ok g\xFCzel.", "Her þey çok güzel.", "windows-1252" },
      { "Γεια σου κόσμε — İstanbul", "Γεια σου κόσμε — İstanbul", "utf-8", "el" },
      { "\xDE" "a\xF0 er n\xFDtt.", "Það er nýtt.", "windows-1252", NULL, "film.release.srt" },
      { "\xDF\xED\xE0 \xA2 \xF8\xEA\xEE\xEB\xE5. \xA8\xED \xEF\xF0\xE0\xF6\xF3\xE5 \xA2 \xF8\xEA\xEE\xEB\xE5.", "Яна ў школе. Ён працуе ў школе.", "windows-1251" },
      { "Her \xFE" "ey \xE7ok g\xFCzel.", "Her şey çok güzel.", "windows-1254", "tr" },
      { "\xC3\xE5\xE9\xE1 \xF3\xEF\xF5 \xEA\xFC\xF3\xEC\xE5", "Γεια σου κόσμε", "windows-1253", NULL, "film.ell.srt" },
      { "\xDDttu \xE1 takkann.", "Ýttu á takkann.", "windows-1252", "isl", "film.tr.srt" }
    };
    for (size_t k = 0; k < sizeof extra / sizeof extra[0]; k++) {
      char raw[512];
      snprintf(raw, sizeof raw, "1\n00:00:01,000 --> 00:00:02,000\n%s\n\n", extra[k].bytes);
      u = legenda_utf8_idioma(raw, (long)strlen(raw), extra[k].language, extra[k].filename, &cs);
      n = u ? legenda_extrair(u, &v) : 0;
      OK(u && !strcmp(cs, extra[k].charset) && n == 1 && !strcmp(v[0].texto, extra[k].text),
         "charset %zu: %s (%s)", k, extra[k].text, cs);
      if (n) free(v);
      free(u);
    }
    static const struct { const char *file, *lang; } tags[] = {
      { "film.el.srt", "el" }, { "film.gre.srt", "el" }, { "film.ELL.srt", "el" },
      { "film.tr.srt", "tr" }, { "film.tur.srt", "tr" },
      { "https://host.tr/film.srt?name=.el.", "" }, { "film.greekish.srt", "" }
    };
    for (size_t k = 0; k < sizeof tags / sizeof tags[0]; k++)
      OK(!strcmp(legenda_idioma(NULL, tags[k].file), tags[k].lang), "filename tag %zu", k);
    // Arabe em Windows-1256 (#247, #261)
    static const char c1256[] = "\xE3\xD1\xCD\xC8\xC7\xA1" " \xDF\xED\xDD" " \xCD\xC7\xE1\xDF" " \xC7\xE1\xED\xE6\xE3\xBF" " \xC3\xE4\xC7" " \xC8\xCE\xED\xD1" " \xD4\xDF\xD1\xC7" " \xE1\xDF";
    u = legenda_utf8(c1256, (long)strlen(c1256), &cs);
    OK(u && !strcmp(cs, "windows-1256") && !strcmp(u, "مرحبا، كيف حالك اليوم؟ أنا بخير شكرا لك"), "cp1256 arabe (%s)", cs);
    free(u);
    // Russo com palavras em maiusculas continua 1251
    static const char c1251m[] = "\xC2\xCD\xC8\xCC\xC0\xCD\xC8\xC5! \xCE\xCF\xC0\xD1\xCD\xCE\xD1\xD2\xDC! \xCF\xF0\xE8\xE2\xE5\xF2, \xEA\xE0\xEA" " \xE4\xE5\xEB\xE0, \xE2\xF1\xB8" " \xF5\xEE\xF0\xEE\xF8\xEE";
    u = legenda_utf8(c1251m, (long)strlen(c1251m), &cs);
    OK(u && !strcmp(cs, "windows-1251") && !strcmp(u, "ВНИМАНИЕ! ОПАСНОСТЬ! Привет, как дела, всё хорошо"), "cp1251 com maiusculas (%s)", cs);
    free(u);
    // UTF-8 com BOM: tira o BOM, nao mexe no resto
    static const char bom[] = "\xEF\xBB\xBFol\xC3\xA1";
    u = legenda_utf8(bom, (long)strlen(bom), &cs);
    OK(u && !strcmp(cs, "utf-8") && !strcmp(u, "olá"), "utf-8 com BOM");
    free(u);
    // UTF-16LE com BOM e zeros no meio
    static const char u16[] = { (char)0xFF, (char)0xFE, 'o',0, 'l',0, (char)0xE1,0, '\n',0 };
    u = legenda_utf8(u16, (long)sizeof u16, &cs);
    OK(u && !strcmp(cs, "utf-16le") && !strcmp(u, "olá\n"), "utf-16le");
    free(u);
    static const char u16be[] = { (char)0xFE, (char)0xFF, 0,'o', 0,'k', (char)0xD8,(char)0x3D,(char)0xDE,(char)0x00 };
    u = legenda_utf8(u16be, (long)sizeof u16be, &cs);
    OK(u && !strcmp(cs, "utf-16be") && !strcmp(u, "ok\xF0\x9F\x98\x80"), "utf-16be com par substituto");
    free(u);
    // Latino com UMA palavra de dois acentos seguidos nao vira cirilico
    static const char lat[] = "Cora\xE7\xE3o, n\xE3o, ent\xE3o, ma\xE7\xE3, a\xE7\xE3o, p\xE3o, m\xE3" "e, s\xE3o, irm\xE3";
    u = legenda_utf8(lat, (long)strlen(lat), &cs);
    OK(u && !strcmp(cs, "windows-1252") && strstr(u, "Coração"), "portugues com 'çã' fica 1252 (%s)", cs);
    free(u); }

  printf(falhas ? "legenda_ass: %d falha(s)\n" : "legenda_ass: tudo ok\n", falhas);
  return falhas ? 1 : 0;
}
