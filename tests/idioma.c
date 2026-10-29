// A tabela de traducao esta ORDENADA e responde? Sao as duas unicas maneiras
// de ela falhar: fora de ordem a busca binaria erra em silencio, e chave
// ausente devolve o portugues sem avisar.
//
//   cc tests/idioma.c src/idioma.c -Isrc -I/opt/homebrew/include \
//      -o /tmp/t-idioma && /tmp/t-idioma
//
// Cobre tambem os outros 29 idiomas (ro... zhtw, ver idiomacod.h): toda chave de idioma_tab.h tem de
// voltar traduzida, nao vazia, com os MESMOS marcadores printf e os mesmos \n
// que o portugues — o mesmo que tools/idiomas.py confere no texto, aqui pelo
// caminho que o app usa (i18n), que e onde um desalinhamento de tabela aparece.
// O -I do Homebrew e por causa do ajustes.h, que inclui SDL so pelo tipo de
// evento; nada de SDL e chamado por este teste.
#include "idioma.h"
#include "idiomacod.h"
#include "comentordem.h"
#include <stdio.h>
#include <string.h>

// Dubles: o teste nao sobe ajustes.c inteiro so para saber o idioma.
static int lg = IDIOMA_EN;
int ajustes_idioma(void) { return lg; }
int ajustes_idioma_ingles(void) { return lg == IDIOMA_EN; }

// As chaves, para varrer a tabela inteira sem tocar em idioma.c.
static const struct { const char *pt, *en; } CHAVES[] = {
#include "idioma_tab.h"
};

static int falhas;
static void confere(const char *pt, const char *esperado) {
  const char *r = i18n(pt);
  if (strcmp(r, esperado) != 0) {
    printf("FALHOU: \"%s\" -> \"%s\" (esperava \"%s\")\n", pt, r, esperado);
    falhas++;
  }
}

static void confere_mes(int mes, const char *pt, const char *esperado) {
  const char *r = idioma_mes_data(mes, pt);
  if (strcmp(r, esperado) != 0) {
    printf("FALHOU: mes %d (\"%s\") no idioma %d -> \"%s\" (esperava \"%s\")\n", mes, pt, lg, r, esperado);
    falhas++;
  }
}

// Marcadores printf de `s`, na ordem, um por linha de saida ("%d%s%.1f..."), sem
// os "%%". Comparados como texto: "%d" contra "%s" e diferente.
static void marcadores(const char *s, char *out, size_t cap) {
  const char *inicio = s;
  size_t o = 0;
  while (*s) {
    const char *p;
    // A percent sign after a number is literal UI text, not a format.
    if (*s != '%' || (s > inicio && s[-1] >= '0' && s[-1] <= '9')) { s++; continue; }
    if (s[1] == '%') { s += 2; continue; }
    // flags, largura, precisao e tamanho; so conta se terminar numa conversao
    // (um "50%" solto no fim de "Escuro 50%" nao e marcador).
    p = s + 1;
    while (*p && strchr("-+ #0", *p)) p++;
    while (*p >= '0' && *p <= '9') p++;
    if (*p == '.') { p++; while (*p >= '0' && *p <= '9') p++; }
    while (*p && strchr("hlzjt", *p)) p++;
    if (*p && strchr("diouxXeEfgGcsp", *p) && o + (size_t)(p - s) + 3 < cap) {
      memcpy(out + o, s, (size_t)(p - s) + 1); o += (size_t)(p - s) + 1; out[o++] = ';';
      s = p + 1;
    } else s++;
  }
  out[o] = 0;
}
static int quebras(const char *s) { int n = 0; for (; *s; s++) if (*s == '\n') n++; return n; }

static void varredura(void) {
  size_t i, k, n = sizeof CHAVES / sizeof *CHAVES;
  // Todos menos o portugues (que devolve a propria chave): o ingles e os 28 de tabela.
  for (k = 1; k < IDIOMA_N; k++) {
    lg = (int)k;
    for (i = 0; i < n; i++) {
      const char *r = i18n(CHAVES[i].pt);
      char mk[128], mr[128];
      if (!r) { printf("FALHOU: idioma %d sem traducao de \"%s\"\n", lg, CHAVES[i].pt); falhas++; continue; }
      if (!*r) r = CHAVES[i].en;   // untranslated table rows use English
      if (lg == IDIOMA_EN && strcmp(r, CHAVES[i].en) != 0) {
        printf("FALHOU: en de \"%s\" -> \"%s\"\n", CHAVES[i].pt, r); falhas++; }
      marcadores(CHAVES[i].pt, mk, sizeof mk); marcadores(r, mr, sizeof mr);
      if (strcmp(mk, mr) != 0 || quebras(CHAVES[i].pt) != quebras(r)) {
        printf("FALHOU: marcadores/quebras de \"%s\" no idioma %d: \"%s\"\n", CHAVES[i].pt, lg, r); falhas++; }
      if (falhas > 30) return;
    }
  }
}

int main(void) {
  for (size_t i = 1; i < sizeof CHAVES / sizeof *CHAVES; i++) {
    if (strcmp(CHAVES[i - 1].pt, CHAVES[i].pt) >= 0) {
      printf("FAIL: keys out of order or duplicated: \"%s\" before \"%s\"\n",
             CHAVES[i - 1].pt, CHAVES[i].pt);
      return 1;
    }
  }
  // Uma de cada tela, incluindo as que tem acento no meio e aspas escapadas.
  confere("Continuar assistindo", "Continue Watching");
  confere("Ajustes", "Settings");
  confere("Quem está assistindo?", "Who's watching?");
  confere("Nenhuma fonte direta disponível. Use Recarregar para tentar novamente.",
          "No direct source available. Use Reload to try again.");
  confere("Ficção científica", "Science Fiction");
  confere("Sáb", "Sat");
  confere("Mostrar \"Continuar assistindo\"", "Show \"Continue Watching\"");

  // Os selos da guia parental: nao sao literais no ponto de desenho (vem de
  // parental_rotulo/parental_gravidade), entao a varredura estatica nao os
  // cobre. Ficaram em portugues ate a v1.0.35 por isso.
  confere("Nudez", "Nudity");
  confere("Violência", "Violence");
  confere("Leve", "Mild");
  confere("Moderado", "Moderate");
  confere("Severo", "Severe");

  // Legendas montadas com %s: a frase pronta nunca casa com chave, entao a
  // parte em portugues tem de ser traduzida ANTES de entrar no formato.
  confere("Seleção do seu catálogo", "A selection from your catalog");
  confere("Conhecido por  %s", "Known for  %s");

  // Sem chave: devolve o proprio texto. E o caso de todo titulo de filme.
  confere("Blade Runner 2049", "Blade Runner 2049");
  confere("", "");

  // Em portugues NADA e traduzido, nem o que esta na tabela.
  lg = IDIOMA_PT;
  confere("Ajustes", "Ajustes");
  confere("Continuar assistindo", "Continuar assistindo");

  // Um de cada idioma novo, com a marca de cada escrita: diacriticos do romeno
  // (virgula embaixo, nao cedilha), cirilico ucraniano (і, ї, є) e russo (ы, э).
  lg = IDIOMA_RO;
  confere("Ajustes", "Setări");
  confere("Continuar assistindo", "Continuă vizionarea");
  confere("Sáb", "Sâm");
  lg = IDIOMA_UK;
  confere("Ajustes", "Налаштування");
  confere("Continuar assistindo", "Продовжити перегляд");
  lg = IDIOMA_RU;
  confere("Ajustes", "Настройки");
  confere("Continuar assistindo", "Продолжить просмотр");
  confere("Blade Runner 2049", "Blade Runner 2049");

  // Trocar de idioma com o cache quente devolve o idioma NOVO, nao o antigo:
  // o cache guarda so o indice da chave, e o idioma e lido a cada chamada.
  lg = IDIOMA_EN; confere("Ajustes", "Settings");
  lg = IDIOMA_RU; confere("Ajustes", "Настройки");
  lg = IDIOMA_EN; confere("Ajustes", "Settings");

  // Mes de uma data por extenso: russo e ucraniano declinam, o resto nao.
  lg = IDIOMA_PT; confere_mes(7, "julho", "julho");
  lg = IDIOMA_EN; confere_mes(7, "julho", "July");
  lg = IDIOMA_RO; confere_mes(7, "julho", "iulie");
  lg = IDIOMA_UK; confere_mes(7, "julho", "липня");
  lg = IDIOMA_RU; confere_mes(7, "julho", "июля");
  lg = IDIOMA_RU; confere_mes(3, "mar\xc3\xa7o", "марта");
  lg = IDIOMA_RU; confere_mes(13, "x", "x");   /* fora de 1..12: cai na tabela */

  // Frances, alemao e espanhol: uma amostra com a marca de cada um (acento
  // agudo/grave, "ß"/umlaut, "¿"/"ñ") e o mes por extenso, que nesses tres nao
  // declina e sai da propria tabela.
  lg = IDIOMA_FR;
  confere("Ajustes", "Réglages");
  confere("Quem está assistindo?", "Qui regarde?");
  confere_mes(7, "julho", "juillet");
  confere_mes(8, "agosto", "août");
  lg = IDIOMA_DE;
  confere("Ajustes", "Einstellungen");
  confere_mes(3, "mar\xc3\xa7o", "März");
  lg = IDIOMA_ES;
  confere("Ajustes", "Ajustes");
  confere("Quem está assistindo?", "¿Quién está viendo?");
  confere_mes(7, "julho", "julio");

  // Mes abreviado e caixa alta: sem lixo no meio de um acento de 2 bytes.
  { char o[32];
    if (strcmp(idioma_mes_curto(IDIOMA_FR, 0), "janv.") || strcmp(idioma_mes_curto(IDIOMA_DE, 2), "März") ||
        strcmp(idioma_mes_curto(IDIOMA_ES, 8), "sep") || idioma_mes_curto(IDIOMA_ES, 12)[0]) {
      printf("FALHOU: mes curto fr/de/es\n"); falhas++; }
    idioma_maiusc(o, sizeof o, "ao\xc3\xbbt");        if (strcmp(o, "AO\xc3\x9bT"))  { printf("FALHOU: maiusc aout\n"); falhas++; }
    idioma_maiusc(o, sizeof o, "m\xc3\xa4rz");        if (strcmp(o, "M\xc3\x84RZ"))  { printf("FALHOU: maiusc marz\n"); falhas++; }
    idioma_maiusc(o, sizeof o, "c\xc5\x93ur");        if (strcmp(o, "C\xc5\x92UR"))  { printf("FALHOU: maiusc coeur\n"); falhas++; }
    idioma_maiusc(o, sizeof o, "a\xc3\x9f");          if (strcmp(o, "A\xc3\x9f"))    { printf("FALHOU: maiusc eszett\n"); falhas++; }
    idioma_maiusc(o, sizeof o, "se\xc3\xb1or");       if (strcmp(o, "SE\xc3\x91OR")) { printf("FALHOU: maiusc senor\n"); falhas++; } }

  // OS 22 DE 2026-09: uma amostra por lingua, com a marca de cada uma — o
  // diacritico que so ela tem, ou a escrita. "Ajustes" e a chave que qualquer
  // desalinhamento de tabela derruba primeiro; "Quem esta assistindo?" leva o
  // acento e a pontuacao propria (¿, ;, ？, ?).
  { static const struct { int l; const char *ajustes, *quem, *status, *pais; } N[] = {
      { IDIOMA_IT,   "Impostazioni", "Chi sta guardando?", "Terminata", "Stati Uniti" },
      { IDIOMA_NL,   "Instellingen", "Wie kijkt er?", "Afgelopen", "Verenigde Staten" },
      { IDIOMA_PL,   "Ustawienia", "Kto oglą" "da?", "Zakoń" "czony", "Stany Zjednoczone" },
      { IDIOMA_TR,   "Ayarlar", "Kim izliyor?", "Sona erdi", "Amerika Birleş" "ik Devletleri" },
      { IDIOMA_PTPT, "Definiçõ" "es", "Quem está a ver?", "Terminada", "Estados Unidos" },
      { IDIOMA_SV,   "Inställningar", "Vem tittar?", "Avslutad", "USA" },
      { IDIOMA_DA,   "Indstillinger", "Hvem ser med?", "Afsluttet", "USA" },
      { IDIOMA_NO,   "Innstillinger", "Hvem ser på?", "Avsluttet", "USA" },
      { IDIOMA_CS,   "Nastavení", "Kdo se dívá?", "Ukončeno", "Spojené státy" },
      { IDIOMA_SK,   "Nastavenia", "Kto pozerá?", "Ukončené", "Spojené štáty" },
      { IDIOMA_SL,   "Nastavitve", "Kdo gleda?", "Zaključeno", "Združ" "ene drž" "ave Amerike" },
      { IDIOMA_HU,   "Beállítások", "Ki nézi?", "Befejezett", "Egyesült Államok" },
      { IDIOMA_LT,   "Nustatymai", "Kas žiūri?", "Baigta", "Jungtinės Amerikos Valstijos" },
      { IDIOMA_BS,   "Postavke", "Ko gleda?", "Završ" "eno", "Sjedinjene Američke Drž" "ave" },
      { IDIOMA_SR,   "Podeš" "avanja", "Ko gleda?", "Završ" "eno", "Sjedinjene Američke Drž" "ave" },
      { IDIOMA_BG,   "Настройки", "Кой гледа?", "Приключил", "Съединени щати" },
      { IDIOMA_EL,   "Ρυθμίσεις", "Ποιος βλέπει;", "Ολοκληρώθηκε", "Ηνωμένες Πολιτείες" },
      { IDIOMA_ID,   "Pengaturan", "Siapa yang menonton?", "Tamat", "Amerika Serikat" },
      { IDIOMA_VI,   "Cài đặt", "Ai đang xem?", "Đã kết thúc", "Hoa Kỳ" },
      { IDIOMA_JA,   "設定", "誰が見ていますか?", "完結", "アメリカ" },
      { IDIOMA_ZHCN, "设置", "谁在观看？", "已完结", "美国" },
      { IDIOMA_ZHTW, "設定", "誰在觀看？", "已完結", "美國" },
    };
    size_t i;
    for (i = 0; i < sizeof N / sizeof *N; i++) {
      lg = N[i].l;
      confere("Ajustes", N[i].ajustes);
      confere("Quem está assistindo?", N[i].quem);
      confere("Finalizada", N[i].status);
      confere("Estados Unidos", N[i].pais);
      confere("Blade Runner 2049", "Blade Runner 2049");   /* titulo: nunca traduzido */
    } }

  // Duracao: a abreviatura de hora e a propria de cada lingua.
  lg = IDIOMA_TR; confere("%dh %dmin", "%dsa %ddk");
  lg = IDIOMA_DA; confere("%dh %dmin", "%dt %dmin");
  lg = IDIOMA_NL; confere("%dh", "%d u");
  lg = IDIOMA_JA; confere("%dmin", "%d 分");
  lg = IDIOMA_ZHTW; confere("%dh", "%d 小時");

  // MES POR EXTENSO no GENITIVO: pl, cs, sk, sl, lt, bs, sr, el declinam ("21
  // września", "rugsėjo 21 d."); hu, tr, vi, bg, id e os demais usam a tabela
  // (o bulgaro, o hungaro e o vietnamita nao declinam).
  lg = IDIOMA_PL;   confere_mes(9, "setembro", "września");
  lg = IDIOMA_CS;   confere_mes(9, "setembro", "září");
  lg = IDIOMA_SK;   confere_mes(3, "março", "marca");
  lg = IDIOMA_SL;   confere_mes(12, "dezembro", "decembra");
  lg = IDIOMA_LT;   confere_mes(9, "setembro", "rugsėjo");
  lg = IDIOMA_BS;   confere_mes(1, "janeiro", "januara");
  lg = IDIOMA_SR;   confere_mes(8, "agosto", "avgusta");
  lg = IDIOMA_EL;   confere_mes(9, "setembro", "Σεπτεμβρίου");
  lg = IDIOMA_HU;   confere_mes(9, "setembro", "szeptember");
  lg = IDIOMA_TR;   confere_mes(9, "setembro", "Eylül");
  lg = IDIOMA_BG;   confere_mes(9, "setembro", "септември");
  lg = IDIOMA_VI;   confere_mes(9, "setembro", "tháng 9");
  lg = IDIOMA_IT;   confere_mes(9, "setembro", "settembre");

  // Mes abreviado dos 22, e a ORDEM da data curta.
  { char o[48];
    static const struct { int l, m; const char *curto; } M[] = {
      { IDIOMA_IT, 0, "gen" }, { IDIOMA_NL, 2, "mrt" }, { IDIOMA_PL, 8, "wrz" },
      { IDIOMA_PL, 9, "paź" }, { IDIOMA_TR, 1, "Ş" "ub" }, { IDIOMA_PTPT, 8, "set" },
      { IDIOMA_SV, 2, "mars" }, { IDIOMA_DA, 4, "maj" }, { IDIOMA_NO, 11, "des" },
      { IDIOMA_CS, 8, "zář" }, { IDIOMA_SK, 5, "jún" }, { IDIOMA_SL, 7, "avg." },
      { IDIOMA_HU, 8, "szept." }, { IDIOMA_LT, 8, "rugs." }, { IDIOMA_BS, 3, "apr" },
      { IDIOMA_SR, 7, "avg" }, { IDIOMA_BG, 0, "яну" }, { IDIOMA_EL, 8, "Σεπ" },
      { IDIOMA_ID, 7, "Agu" }, { IDIOMA_VI, 8, "thg 9" }, { IDIOMA_JA, 8, "9月" },
      { IDIOMA_ZHCN, 11, "12月" }, { IDIOMA_ZHTW, 0, "1月" },
    };
    size_t i;
    for (i = 0; i < sizeof M / sizeof *M; i++)
      if (strcmp(idioma_mes_curto(M[i].l, M[i].m), M[i].curto)) {
        printf("FALHOU: mes curto %d/%d -> \"%s\" (esperava \"%s\")\n", M[i].l, M[i].m,
               idioma_mes_curto(M[i].l, M[i].m), M[i].curto);
        falhas++; }
    // Nenhum idioma tem mes vazio ou repetido.
    { int l, m, n;
      for (l = 0; l < IDIOMA_N; l++)
        for (m = 0; m < 12; m++) {
          if (!idioma_mes_curto(l, m)[0]) { printf("FALHOU: mes curto vazio %d/%d\n", l, m); falhas++; }
          for (n = 0; n < m; n++)
            if (!strcmp(idioma_mes_curto(l, m), idioma_mes_curto(l, n))) {
              printf("FALHOU: mes curto repetido no idioma %d: %d e %d\n", l, n, m); falhas++; }
        } }
    idioma_data_curta(IDIOMA_PT, 21, idioma_mes_curto(IDIOMA_PT, 8), 0, o, sizeof o);
    if (strcmp(o, "21 set")) { printf("FALHOU: data curta pt -> %s\n", o); falhas++; }
    idioma_data_curta(IDIOMA_EN, 21, idioma_mes_curto(IDIOMA_EN, 8), 2025, o, sizeof o);
    if (strcmp(o, "Sep 21, 2025")) { printf("FALHOU: data curta en -> %s\n", o); falhas++; }
    idioma_data_curta(IDIOMA_JA, 21, idioma_mes_curto(IDIOMA_JA, 8), 2025, o, sizeof o);
    if (strcmp(o, "2025年9月21日")) { printf("FALHOU: data curta ja -> %s\n", o); falhas++; }
    idioma_data_curta(IDIOMA_ZHTW, 5, idioma_mes_curto(IDIOMA_ZHTW, 0), 0, o, sizeof o);
    if (strcmp(o, "1月5日")) { printf("FALHOU: data curta zhtw -> %s\n", o); falhas++; }
    idioma_data_curta(IDIOMA_HU, 21, idioma_mes_curto(IDIOMA_HU, 8), 2025, o, sizeof o);
    if (strcmp(o, "2025. szept. 21.")) { printf("FALHOU: data curta hu -> %s\n", o); falhas++; }
    idioma_data_curta(IDIOMA_LT, 21, idioma_mes_curto(IDIOMA_LT, 8), 0, o, sizeof o);
    if (strcmp(o, "rugs. 21")) { printf("FALHOU: data curta lt -> %s\n", o); falhas++; }
    idioma_data_curta(IDIOMA_PL, 3, idioma_mes_curto(IDIOMA_PL, 8), 2024, o, sizeof o);
    if (strcmp(o, "3 wrz 2024")) { printf("FALHOU: data curta pl -> %s\n", o); falhas++; }
    // Data por extenso com o ano na frente, e cabecalho do calendario.
    if (!idioma_data_extenso_especial(IDIOMA_JA, 29, 7, "x", "2026", o, sizeof o) || strcmp(o, "2026年7月29日")) {
      printf("FALHOU: data extenso ja -> %s\n", o); falhas++; }
    if (!idioma_data_extenso_especial(IDIOMA_HU, 29, 7, "július", "2026", o, sizeof o) ||
        strcmp(o, "2026. július 29.")) { printf("FALHOU: data extenso hu -> %s\n", o); falhas++; }
    if (!idioma_data_extenso_especial(IDIOMA_LT, 29, 7, "liepos", "2026", o, sizeof o) ||
        strcmp(o, "2026 m. liepos 29 d.")) { printf("FALHOU: data extenso lt -> %s\n", o); falhas++; }
    if (idioma_data_extenso_especial(IDIOMA_PL, 29, 7, "lipca", "2026", o, sizeof o)) {
      printf("FALHOU: data extenso pl nao devia ser especial\n"); falhas++; }
    idioma_mes_ano(IDIOMA_ZHCN, "9月", 2026, o, sizeof o);
    if (strcmp(o, "2026年9月")) { printf("FALHOU: mes ano zhcn -> %s\n", o); falhas++; }
    idioma_mes_ano(IDIOMA_PT, "SETEMBRO", 2026, o, sizeof o);
    if (strcmp(o, "SETEMBRO 2026")) { printf("FALHOU: mes ano pt -> %s\n", o); falhas++; } }

  // CAIXA ALTA por escrita: turco (i -> İ, ı -> I), grego (sem acento, ς -> Σ),
  // vietnamita (ơ ư đ e o bloco U+1EA0), Latin Estendido-A (pl, cs, sk, hu, lt,
  // bs/sr) e o bulgaro (cirilico).
  { char o[96];
    static const struct { int l; const char *in, *out; } U[] = {
      { IDIOMA_TR, "ocak nisan ışık ş" "ubat ğ çöü",
                   "OCAK Nİ" "SAN IŞ" "IK Ş" "UBAT Ğ ÇÖÜ" },
      { IDIOMA_PT, "ocak", "OCAK" },
      { IDIOMA_EL, "Σεπτέμβριος Μάιος Ιούνιος", "ΣΕΠΤΕΜΒΡΙΟΣ ΜΑΙΟΣ ΙΟΥΝΙΟΣ" },
      { IDIOMA_EL, "ΐ ϊ ΰ ς", "Ι Ι Υ Σ" },
      { IDIOMA_VI, "tháng chín ơ ư đ ĩ ũ ạ ề ệ ỹ", "THÁNG CHÍN Ơ Ư Đ Ĩ Ũ Ạ Ề Ệ Ỹ" },
      { IDIOMA_PL, "wrzesień łódź ą ę ź ż ń ś ć", "WRZESIEŃ ŁÓDŹ Ą Ę Ź Ż Ń Ś Ć" },
      { IDIOMA_CS, "září říjen únor březen ď ť ň ě ů", "ZÁŘÍ ŘÍJEN ÚNOR BŘEZEN Ď Ť Ň Ě Ů" },
      { IDIOMA_SK, "október ľ ĺ ŕ ô ä", "OKTÓBER Ľ Ĺ Ŕ Ô Ä" },
      { IDIOMA_HU, "október ő ű ö ü", "OKTÓBER Ő Ű Ö Ü" },
      { IDIOMA_LT, "rugsėjis ą č ę ė į š ų ū ž", "RUGSĖJIS Ą Č Ę Ė Į Š Ų Ū Ž" },
      { IDIOMA_SR, "đ š ž č ć", "Đ Š Ž Č Ć" },
      { IDIOMA_BG, "януари", "ЯНУАРИ" },
      { IDIOMA_PT, "… abc ÿ œ", "… ABC Ÿ Œ" },  /* depois de um caractere de 3 bytes */
    };
    size_t i;
    for (i = 0; i < sizeof U / sizeof *U; i++) {
      idioma_maiusc_em(U[i].l, o, sizeof o, U[i].in);
      if (strcmp(o, U[i].out)) {
        printf("FALHOU: maiusc idioma %d \"%s\" -> \"%s\" (esperava \"%s\")\n", U[i].l, U[i].in, o, U[i].out);
        falhas++; }
    }
    /* o turco aumenta a linha em bytes; o destino curto trunca sem estourar */
    { char c[6]; size_t n = idioma_maiusc_em(IDIOMA_TR, c, sizeof c, "iiiii");
      if (n >= sizeof c || strlen(c) != n) { printf("FALHOU: maiusc turco estourou o destino\n"); falhas++; } } }

  // Decimal points: English, Japanese, Chinese and Arabic.
  { int l, ponto = 0;
    for (l = 0; l < IDIOMA_N; l++) ponto += idioma_ponto_decimal(l);
    if (ponto != 5 || !idioma_ponto_decimal(IDIOMA_AR) ||
        !idioma_ponto_decimal(IDIOMA_EN) || !idioma_ponto_decimal(IDIOMA_JA) ||
        !idioma_ponto_decimal(IDIOMA_ZHCN) || !idioma_ponto_decimal(IDIOMA_ZHTW) ||
        idioma_ponto_decimal(IDIOMA_PT) || idioma_ponto_decimal(IDIOMA_TR) || idioma_ponto_decimal(IDIOMA_PTPT)) {
      printf("FALHOU: idioma_ponto_decimal\n"); falhas++; } }

  // GENEROS: desc_genero_pt manda o nome portugues pela tabela (descoberta.c). Toda
  // chave que ele usa tem de existir na mestra, senao o genero sai em ingles.
  { static const char *G[] = { "Ação", "Aventura", "Animação", "Biografia",
      "Comé" "dia", "Crime", "Documentário", "Drama", "Família", "Fantasia", "Noir",
      "Game show", "História", "Terror", "Música", "Musical", "Mistério", "Notícias",
      "Reality", "Romance", "Ficção científica", "Curta", "Esporte", "Talk show",
      "Suspense", "Guerra", "Faroeste", "Infantil", "Novela", "Adulto" };
    size_t i, k, n = sizeof CHAVES / sizeof *CHAVES;
    for (i = 0; i < sizeof G / sizeof *G; i++) {
      for (k = 0; k < n && strcmp(CHAVES[k].pt, G[i]); k++) {}
      if (k == n) { printf("FALHOU: genero \"%s\" nao esta na tabela\n", G[i]); falhas++; } } }

  // DETALHE DO TITULO: status, pais e duracao crus (fix/detalhe-traducao). As
  // chaves sao as que desc_status_chave / desc_pais_txt / desc_duracao_min usam.
  lg = IDIOMA_RU;
  confere("Estados Unidos", "\xd0\xa1\xd0\xa8\xd0\x90");
  confere("Em exibi\xc3\xa7\xc3\xa3o", "\xd0\x92 \xd1\x8d\xd1\x84\xd0\xb8\xd1\x80\xd0\xb5");
  confere("%dh %dmin", "%d \xd1\x87 %d \xd0\xbc\xd0\xb8\xd0\xbd");
  lg = IDIOMA_FR;
  confere("Reino Unido", "Royaume-Uni");
  confere("Finalizada", "Termin\xc3\xa9" "e");
  confere("%dh", "%d h");
  lg = IDIOMA_DE;
  confere("Coreia do Sul", "S\xc3\xbc" "dkorea");
  confere("%dmin", "%d Min.");
  lg = IDIOMA_ES;
  confere("Pa\xc3\xad" "ses Baixos", "Pa\xc3\xad" "ses Bajos");
  lg = IDIOMA_EN;
  confere("Pa\xc3\xad" "ses Baixos", "Netherlands");
  confere("Lan\xc3\xa7" "ado", "Released");

  // Codigo ISO do idioma da interface (compara com o `language` do Trakt).
  { static const char *ISO[] = { "pt", "en", "ro", "uk", "ru", "fr", "de", "es",
      "it", "nl", "pl", "tr", "pt", "sv", "da", "no", "cs", "sk", "sl", "hu", "lt",
      "bs", "sr", "bg", "el", "id", "vi", "ja", "zh", "zh", "ar" };
    _Static_assert(sizeof ISO / sizeof *ISO == IDIOMA_N, "um ISO por IDIOMA_*");
    int i;
    for (i = 0; i < IDIOMA_N; i++)
      if (strcmp(idioma_iso(i), ISO[i])) { printf("FALHOU: idioma_iso(%d)\n", i); falhas++; } }

  // COMENTARIOS: os do idioma da interface primeiro, estavel; sem `language`
  // conta como do idioma (nao rebaixa o que talvez seja legivel).
  { ComentAchado v[5];
    const char *L[5] = { "en", "pt", "", "es", "pt" };
    int i, k;
    memset(v, 0, sizeof v);
    for (i = 0; i < 5; i++) { snprintf(v[i].u, sizeof v[i].u, "u%d", i); strcpy(v[i].l, L[i]); }
    k = coment_ordenar(v, 5, "pt");
    if (k != 3 || strcmp(v[0].u, "u1") || strcmp(v[1].u, "u2") || strcmp(v[2].u, "u4") ||
        strcmp(v[3].u, "u0") || strcmp(v[4].u, "u3")) {
      printf("FALHOU: coment_ordenar pt -> %s %s %s %s %s (k=%d)\n",
             v[0].u, v[1].u, v[2].u, v[3].u, v[4].u, k);
      falhas++; }
    k = coment_ordenar(v, 5, "ru");      /* ninguem e russo: so os sem idioma vem na frente */
    if (k != 1 || strcmp(v[0].u, "u2")) { printf("FALHOU: coment_ordenar ru\n"); falhas++; }
    if (!coment_mesmo_idioma("pt-br", "pt") || coment_mesmo_idioma("en", "pt")) {
      printf("FALHOU: coment_mesmo_idioma\n"); falhas++; } }

  // A tabela inteira, em cada idioma.
  varredura();
  lg = IDIOMA_EN;

  if (falhas) { printf("%d falha(s)\n", falhas); return 1; }
  printf("idioma ok\n");
  return 0;
}
