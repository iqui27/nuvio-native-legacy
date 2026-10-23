// Codigos dos idiomas da interface e os pedacos de texto que dependem so do
// idioma (mes abreviado, caixa alta em UTF-8). Sem dependencia de ajustes.h:
// cwordem.c e noticias.c sao compilados isolados por testes e nao podem puxar
// a biblioteca grafica so por causa de um numero.
//
// O NUMERO E O QUE FICA GRAVADO em ajustes.txt (AJ_IDIOMA): 0 e 1 sao os dois
// idiomas de sempre, e ficaram onde estavam para nao trocar o idioma de quem ja
// escolheu. Novos entram no FIM; nunca reordenar.
#ifndef NV_IDIOMACOD_H
#define NV_IDIOMACOD_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>

enum { IDIOMA_PT = 0, IDIOMA_EN = 1, IDIOMA_RO = 2, IDIOMA_UK = 3, IDIOMA_RU = 4,
       IDIOMA_FR = 5, IDIOMA_DE = 6, IDIOMA_ES = 7,
       // Os 22 de 2026-09: entram NO FIM, na ordem em que foram integrados.
       // PTPT e o portugues europeu (o PT e o do Brasil); SR e o sérvio em
       // latim; ZHCN/ZHTW sao chines simplificado e tradicional.
       IDIOMA_IT = 8, IDIOMA_NL = 9, IDIOMA_PL = 10, IDIOMA_TR = 11, IDIOMA_PTPT = 12,
       IDIOMA_SV = 13, IDIOMA_DA = 14, IDIOMA_NO = 15, IDIOMA_CS = 16, IDIOMA_SK = 17,
       IDIOMA_SL = 18, IDIOMA_HU = 19, IDIOMA_LT = 20, IDIOMA_BS = 21, IDIOMA_SR = 22,
       IDIOMA_BG = 23, IDIOMA_EL = 24, IDIOMA_ID = 25, IDIOMA_VI = 26, IDIOMA_JA = 27,
       IDIOMA_ZHCN = 28, IDIOMA_ZHTW = 29,
       IDIOMA_AR = 30,
       IDIOMA_N = 31 };

// Codigo ISO 639-1 do idioma da interface ("pt", "en", "ro", "uk", "ru", "fr",
// "de", "es", "it", "nl"... "ja", "zh"). O portugues europeu tambem e "pt", o
// chines simplificado e o tradicional tambem sao "zh" e o noruegues e "no". E o
// mesmo que o Trakt poe em `language` de um comentario, e o prefixo de
// "pt-BR"/"ru-RU" que o TMDB recebe — por isso mora aqui, ao lado dos codigos, e
// nao em cada consumidor.
static inline const char *idioma_iso(int idioma) {
  switch (idioma) {
    case IDIOMA_EN: return "en";
    case IDIOMA_RO: return "ro";
    case IDIOMA_UK: return "uk";
    case IDIOMA_RU: return "ru";
    case IDIOMA_FR: return "fr";
    case IDIOMA_DE: return "de";
    case IDIOMA_ES: return "es";
    case IDIOMA_IT: return "it";
    case IDIOMA_NL: return "nl";
    case IDIOMA_PL: return "pl";
    case IDIOMA_TR: return "tr";
    case IDIOMA_SV: return "sv";
    case IDIOMA_DA: return "da";
    case IDIOMA_NO: return "no";
    case IDIOMA_CS: return "cs";
    case IDIOMA_SK: return "sk";
    case IDIOMA_SL: return "sl";
    case IDIOMA_HU: return "hu";
    case IDIOMA_LT: return "lt";
    case IDIOMA_BS: return "bs";
    case IDIOMA_SR: return "sr";
    case IDIOMA_BG: return "bg";
    case IDIOMA_EL: return "el";
    case IDIOMA_ID: return "id";
    case IDIOMA_VI: return "vi";
    case IDIOMA_JA: return "ja";
    case IDIOMA_ZHCN: case IDIOMA_ZHTW: return "zh";
    case IDIOMA_AR: return "ar";
    default:        return "pt";   // IDIOMA_PT e IDIOMA_PTPT
  }
}

// 1 = a interface escreve o decimal com PONTO ("8.4"); 0 = com virgula ("8,4").
// Ingles, japones, chines e arabe (com algarismos ocidentais) usam ponto; os
// demais idiomas usam virgula.
static inline int idioma_ponto_decimal(int idioma) {
  return idioma == IDIOMA_EN || idioma == IDIOMA_JA || idioma == IDIOMA_ZHCN ||
         idioma == IDIOMA_ZHTW || idioma == IDIOMA_AR;
}

// Integer grouping follows the selected UI locale, independent of LC_NUMERIC.
static inline void idioma_inteiro(int idioma, int valor, char *out, unsigned cap) {
  char raw[32];
  const char *sep = idioma_ponto_decimal(idioma) ? "," : ".";
  switch (idioma) {
    case IDIOMA_FR: case IDIOMA_RU: case IDIOMA_UK: case IDIOMA_PL:
    case IDIOMA_SV: case IDIOMA_NO: case IDIOMA_CS: case IDIOMA_SK:
    case IDIOMA_HU: case IDIOMA_LT: sep=" "; break;
    default: break;
  }
  snprintf(raw,sizeof raw,"%d",valor);
  unsigned n=(unsigned)strlen(raw), pos=0;
  for (unsigned i=0;i<n && pos+1<cap;i++) {
    if (i && raw[i-1]!='-' && (n-i)%3==0 && pos+2<cap) out[pos++]=*sep;
    out[pos++]=raw[i];
  }
  if (cap) out[pos]=0;
}

// Troca, NO LUGAR, o ponto decimal de um texto ja montado por virgula quando o
// idioma escreve com virgula: "24.1 mi" -> "24,1 mi". So mexe em ponto entre
// dois digitos, entao "E1. Piloto" e "1.5" distinguem-se. snprintf("%.1f") sai
// sempre com ponto (main.c fixa LC_NUMERIC "C"); toda frase de tela com decimal
// passa por aqui depois do snprintf.
static inline void idioma_decimal_texto(char *s, int idioma) {
  char *p;
  if (!s || idioma_ponto_decimal(idioma)) return;
  for (p = s; *p; p++)
    if (*p == '.' && p > s && p[-1] >= '0' && p[-1] <= '9' && p[1] >= '0' && p[1] <= '9') *p = ',';
}

// 1 = a data escreve o ANO PRIMEIRO e o mes em numero + 月 ("2026年9月21日"): o
// japones e os dois chineses. Ai a data nao passa pelo modelo "%d %s %d" das
// outras linguas, que e o que as tabelas traduzem.
static inline int idioma_data_asiatica(int idioma) {
  return idioma == IDIOMA_JA || idioma == IDIOMA_ZHCN || idioma == IDIOMA_ZHTW;
}

// Mes abreviado, m0 = 0..11. Em ingles a capitalizacao e a do idioma ("Sep");
// nos demais segue o uso corrente de cada lingua: minuscula em portugues,
// espanhol, italiano, holandes, polones ("set", "sep", "wrz"), maiuscula inicial
// em turco, grego e indonesio ("Eyl", "Σεπ", "Sep"). Japones e chines nao tem
// abreviatura: o mes e o numero mais 月 ("9月"), e o vietnamita escreve "thg 9".
static inline const char *idioma_mes_curto(int idioma, int m0) {
  static const char *EN[] = { "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" };
  static const char *PT[] = { "jan","fev","mar","abr","mai","jun","jul","ago","set","out","nov","dez" };
  static const char *RO[] = { "ian","feb","mar","apr","mai","iun","iul","aug","sep","oct","nov","dec" };
  static const char *UK[] = { "січ","лют","бер","кві","тра","чер","лип","сер","вер","жов","лис","гру" };
  static const char *RU[] = { "янв","фев","мар","апр","мая","июн","июл","авг","сен","окт","ноя","дек" };
  // fr e de: o ponto faz parte da abreviacao ("21 janv.", "21. Jan." fica sem o
  // ponto do dia porque a data curta e "dia mes"); mes que ja e curto nao leva.
  static const char *FR[] = { "janv.","févr.","mars","avr.","mai","juin","juil.","août","sept.","oct.","nov.","déc." };
  static const char *DE[] = { "Jan.","Feb.","März","Apr.","Mai","Juni","Juli","Aug.","Sept.","Okt.","Nov.","Dez." };
  static const char *ES[] = { "ene","feb","mar","abr","may","jun","jul","ago","sep","oct","nov","dic" };
  static const char *IT[] = { "gen","feb","mar","apr","mag","giu","lug","ago","set","ott","nov","dic" };
  static const char *NL[] = { "jan","feb","mrt","apr","mei","jun","jul","aug","sep","okt","nov","dec" };
  static const char *PL[] = { "sty","lut","mar","kwi","maj","cze","lip","sie","wrz","paź","lis","gru" };
  static const char *TR[] = { "Oca","Şub","Mar","Nis","May","Haz","Tem","Ağu","Eyl","Eki","Kas","Ara" };
  static const char *SV[] = { "jan","feb","mars","apr","maj","juni","juli","aug","sep","okt","nov","dec" };
  static const char *DA[] = { "jan","feb","mar","apr","maj","jun","jul","aug","sep","okt","nov","dec" };
  static const char *NO[] = { "jan","feb","mar","apr","mai","jun","jul","aug","sep","okt","nov","des" };
  static const char *CS[] = { "led","úno","bře","dub","kvě","čvn","čvc","srp","zář","říj","lis","pro" };
  static const char *SK[] = { "jan","feb","mar","apr","máj","jún","júl","aug","sep","okt","nov","dec" };
  static const char *SL[] = { "jan.","feb.","mar.","apr.","maj","jun.","jul.","avg.","sep.","okt.","nov.","dec." };
  static const char *HU[] = { "jan.","febr.","márc.","ápr.","máj.","jún.","júl.","aug.","szept.","okt.","nov.","dec." };
  static const char *LT[] = { "saus.","vas.","kov.","bal.","geg.","birž.","liep.","rugp.","rugs.","spal.","lapkr.","gruod." };
  static const char *BS[] = { "jan","feb","mar","apr","maj","jun","jul","aug","sep","okt","nov","dec" };
  static const char *SR[] = { "jan","feb","mar","apr","maj","jun","jul","avg","sep","okt","nov","dec" };
  static const char *BG[] = { "яну","фев","мар","апр","май","юни","юли","авг","сеп","окт","ное","дек" };
  static const char *EL[] = { "Ιαν","Φεβ","Μαρ","Απρ","Μαΐ","Ιουν","Ιουλ","Αυγ","Σεπ","Οκτ","Νοε","Δεκ" };
  static const char *ID[] = { "Jan","Feb","Mar","Apr","Mei","Jun","Jul","Agu","Sep","Okt","Nov","Des" };
  static const char *VI[] = { "thg 1","thg 2","thg 3","thg 4","thg 5","thg 6","thg 7","thg 8","thg 9","thg 10","thg 11","thg 12" };
  static const char *AR[] = { "يناير","فبراير","مارس","أبريل","مايو","يونيو","يوليو","أغسطس","سبتمبر","أكتوبر","نوفمبر","ديسمبر" };
  static const char *CJK[] = { "1月","2月","3月","4月","5月","6月","7月","8月","9月","10月","11月","12月" };
  if (m0 < 0 || m0 > 11) return "";
  switch (idioma) {
    case IDIOMA_EN: return EN[m0];
    case IDIOMA_RO: return RO[m0];
    case IDIOMA_UK: return UK[m0];
    case IDIOMA_RU: return RU[m0];
    case IDIOMA_FR: return FR[m0];
    case IDIOMA_DE: return DE[m0];
    case IDIOMA_ES: return ES[m0];
    case IDIOMA_IT: return IT[m0];
    case IDIOMA_NL: return NL[m0];
    case IDIOMA_PL: return PL[m0];
    case IDIOMA_TR: return TR[m0];
    case IDIOMA_PTPT: return PT[m0];
    case IDIOMA_SV: return SV[m0];
    case IDIOMA_DA: return DA[m0];
    case IDIOMA_NO: return NO[m0];
    case IDIOMA_CS: return CS[m0];
    case IDIOMA_SK: return SK[m0];
    case IDIOMA_SL: return SL[m0];
    case IDIOMA_HU: return HU[m0];
    case IDIOMA_LT: return LT[m0];
    case IDIOMA_BS: return BS[m0];
    case IDIOMA_SR: return SR[m0];
    case IDIOMA_BG: return BG[m0];
    case IDIOMA_EL: return EL[m0];
    case IDIOMA_ID: return ID[m0];
    case IDIOMA_VI: return VI[m0];
    case IDIOMA_JA: case IDIOMA_ZHCN: case IDIOMA_ZHTW: return CJK[m0];
    case IDIOMA_AR: return AR[m0];
    default:        return PT[m0];
  }
}

// Data curta ("21 set", "Sep 21, 2026", "9月21日"): a ORDEM e o que muda por
// idioma, e por isso mora ao lado dos nomes dos meses. `mes` e o nome ja
// escolhido (e, se for o caso, ja em caixa alta); `ano` 0 = sem ano.
//   ingles     Sep 21, 2026 / Sep 21
//   ja e zh    2026年9月21日 / 9月21日 (o mes ja vem como "9月")
//   hungaro    2026. szept. 21. / szept. 21.
//   lituano    2026 rugs. 21 / rugs. 21
//   os demais  21 set 2026 / 21 set
static inline void idioma_data_curta(int idioma, int dia, const char *mes, int ano,
                                     char *dst, size_t cap) {
  if (!dst || !cap) return;
  if (idioma == IDIOMA_EN) {
    if (ano) snprintf(dst, cap, "%s %d, %d", mes, dia, ano);
    else     snprintf(dst, cap, "%s %d", mes, dia);
  } else if (idioma_data_asiatica(idioma)) {
    if (ano) snprintf(dst, cap, "%d年%s%d日", ano, mes, dia);
    else     snprintf(dst, cap, "%s%d日", mes, dia);
  } else if (idioma == IDIOMA_HU) {
    if (ano) snprintf(dst, cap, "%d. %s %d.", ano, mes, dia);
    else     snprintf(dst, cap, "%s %d.", mes, dia);
  } else if (idioma == IDIOMA_LT) {
    if (ano) snprintf(dst, cap, "%d %s %d", ano, mes, dia);
    else     snprintf(dst, cap, "%s %d", mes, dia);
  } else {
    if (ano) snprintf(dst, cap, "%d %s %d", dia, mes, ano);
    else     snprintf(dst, cap, "%d %s", dia, mes);
  }
}

// Data POR EXTENSO onde o modelo das tabelas ("%d de %s de %c%c%c%c": dia, mes,
// ano) nao serve porque o ANO VEM PRIMEIRO. Devolve 1 se escreveu (japones,
// chines, hungaro, lituano); 0 = use o modelo traduzido de desc_data_extenso.
//   ja e zh   2026年9月21日   (o mes e numero, nao o nome: `nome` e ignorado)
//   hungaro   2026. szeptember 21.
//   lituano   2026 m. rugsėjo 21 d.   (`nome` ja vem no genitivo)
static inline int idioma_data_extenso_especial(int idioma, int dia, int mes,
                                               const char *nome, const char *ano,
                                               char *dst, size_t cap) {
  if (!dst || !cap) return 0;
  if (idioma_data_asiatica(idioma)) {
    snprintf(dst, cap, "%s年%d月%d日", ano, mes, dia);
    return 1;
  }
  if (idioma == IDIOMA_HU) { snprintf(dst, cap, "%s. %s %d.", ano, nome, dia); return 1; }
  if (idioma == IDIOMA_LT) { snprintf(dst, cap, "%s m. %s %d d.", ano, nome, dia); return 1; }
  return 0;
}

// Cabecalho de mes do calendario ("SETEMBRO 2026"). `nome` ja vem na caixa que o
// cabecalho quer. Japones e chines: "2026年9月" (o nome do mes e "9月");
// hungaro: "2026. SZEPTEMBER"; lituano: "2026 RUGSĖJIS"; os demais: "MES ANO".
static inline void idioma_mes_ano(int idioma, const char *nome, int ano, char *dst, size_t cap) {
  if (!dst || !cap) return;
  if (idioma_data_asiatica(idioma)) snprintf(dst, cap, "%d年%s", ano, nome);
  else if (idioma == IDIOMA_HU)     snprintf(dst, cap, "%d. %s", ano, nome);
  else if (idioma == IDIOMA_LT)     snprintf(dst, cap, "%d %s", ano, nome);
  else                              snprintf(dst, cap, "%s %d", nome, ano);
}

// Caixa alta em UTF-8 para as escritas que a interface usa: ASCII, Latin-1
// (acentos do portugues, do frances, do alemao e do espanhol; o "ß" fica como
// esta, porque "SS" mudaria o tamanho), o "œ" do frances, todo o Latin Estendido-A
// (polones, tcheco, eslovaco, hungaro, lituano, servo-croata, romeno com breve
// e virgula), o vietnamita (ơ ư đ ĩ ũ e o bloco U+1EA0-1EF9), o grego (sem
// acento na caixa alta, como o idioma pede: "Σεπτέμβριος" -> "ΣΕΠΤΕΜΒΡΙΟΣ"; o
// "ς" final vira "Σ") e o cirilico do russo, do ucraniano e do bulgaro.
// toupper() byte a byte trocaria o primeiro byte de "ação" ou "мая" por lixo.
//
// TURCO. O "i" da caixa alta e o "İ" (com ponto) e o "ı" vira "I": e a unica
// regra que depende do IDIOMA (por isso idioma_maiusc_em), e a unica que
// AUMENTA o tamanho em bytes ("i" 1 byte -> "İ" 2 bytes). Todas as outras
// preservam o tamanho, e isso e o que permite mexer no buffer sem medir antes;
// no turco o destino tem de ter folga (o laco para antes de estourar `tam`).
// Devolve o tamanho escrito.
static inline size_t idioma_maiusc_em(int idioma, char *dst, size_t tam, const char *s) {
  size_t i = 0, o = 0;
  if (!tam) return 0;
  dst[0] = 0;
  if (!s) return 0;
  while (s[i] && o + 3 < tam) {
    unsigned char c = (unsigned char)s[i], d = (unsigned char)s[i + 1];
    unsigned char a = c, b = d, e2 = 0;
    int n = 1;
    if (c < 0x80) {
      if (c >= 'a' && c <= 'z') a = (unsigned char)(c - 32);
      if (c == 'i' && idioma == IDIOMA_TR) { dst[o++] = (char)0xC4; dst[o++] = (char)0xB0; i++; continue; }
    } else if ((c & 0xF0) == 0xE0 && d && s[i + 2]) {
      // 3 bytes: vietnamita (U+1EA0-1EF9, minuscula = impar) e nada mais.
      unsigned char f = (unsigned char)s[i + 2];
      n = 3; e2 = f;
      if (c == 0xE1 && (d == 0xBA || d == 0xBB) && (f & 1)) {
        int cp = 0x1000 | ((d & 0x3F) << 6) | (f & 0x3F);
        if (cp >= 0x1EA1 && cp <= 0x1EF9) e2 = (unsigned char)(f - 1);
      }
    } else if (d) {
      n = 2;
      if (c == 0xC3 && d >= 0xA0 && d <= 0xBE && d != 0xB7) b = (unsigned char)(d - 0x20);
      else if (c == 0xC3 && d == 0xBF) { a = 0xC5; b = 0xB8; }         /* ÿ */
      else if (c == 0xC4 && d == 0xB1) { dst[o++] = 'I'; i += 2; continue; }  /* ı */
      // Latin Estendido-A (U+0100-017F): pares maiuscula/minuscula, com a
      // paridade INVERTIDA entre U+0139 e U+0148 e de U+0179 a U+017E. O
      // caso do "ŀ" (U+0140, o par cruza o byte) fica de fora: so o catalao usa.
      else if (c == 0xC4 && d >= 0x80 && d <= 0xBF) {
        int cp = 0x100 + (d - 0x80);
        if (cp <= 0x137 ? ((cp & 1) && cp != 0x131) : (cp >= 0x13A && !(cp & 1))) b = (unsigned char)(d - 1);
      }
      else if (c == 0xC5 && d >= 0x80 && d <= 0xBF) {
        int cp = 0x140 + (d - 0x80);
        if (cp <= 0x148 ? (cp >= 0x142 && !(cp & 1))
                        : cp >= 0x14B && cp <= 0x177 ? (cp & 1)
                        : (cp >= 0x17A && cp <= 0x17E && !(cp & 1))) b = (unsigned char)(d - 1);
      }
      else if (c == 0xC6 && d == 0xA1) b = 0xA0;                     /* ơ */
      else if (c == 0xC6 && d == 0xB0) b = 0xAF;                     /* ư */
      else if (c == 0xC8 && (d == 0x99 || d == 0x9B)) b = (unsigned char)(d - 1); /* ș ț */
      else if (c == 0xCE && d == 0x86) b = 0x91;                     /* Ά */
      else if (c == 0xCE && (d == 0x88 || d == 0x89 || d == 0x8A)) b = (unsigned char)(d + (d == 0x88 ? 0x0D : d == 0x89 ? 0x0E : 0x0F));
      else if (c == 0xCE && d == 0x8C) b = 0x9F;                     /* Ό */
      else if (c == 0xCE && d == 0x8E) b = 0xA5;                     /* Ύ */
      else if (c == 0xCE && d == 0x8F) b = 0xA9;                     /* Ώ */
      else if (c == 0xCE && (d == 0x90 || d == 0xB0)) b = d == 0x90 ? 0x99 : 0xA5; /* ΐ ΰ */
      else if (c == 0xCE && d == 0xAC) b = 0x91;                     /* ά */
      else if (c == 0xCE && d == 0xAD) b = 0x95;                     /* έ */
      else if (c == 0xCE && d == 0xAE) b = 0x97;                     /* ή */
      else if (c == 0xCE && d == 0xAF) b = 0x99;                     /* ί */
      else if (c == 0xCE && d >= 0xB1 && d <= 0xBF) b = (unsigned char)(d - 0x20);   /* α..ο */
      else if (c == 0xCF && (d == 0x80 || d == 0x81)) { a = 0xCE; b = (unsigned char)(d + 0x20); } /* π ρ */
      else if (c == 0xCF && (d == 0x82 || d == 0x83)) { a = 0xCE; b = 0xA3; }         /* ς σ */
      else if (c == 0xCF && d >= 0x84 && d <= 0x89) { a = 0xCE; b = (unsigned char)(d + 0x20); } /* τ..ω */
      else if (c == 0xCF && d == 0x8A) { a = 0xCE; b = 0x99; }       /* ϊ */
      else if (c == 0xCF && d == 0x8B) { a = 0xCE; b = 0xA5; }       /* ϋ */
      else if (c == 0xCF && d == 0x8C) { a = 0xCE; b = 0x9F; }       /* ό */
      else if (c == 0xCF && d == 0x8D) { a = 0xCE; b = 0xA5; }       /* ύ */
      else if (c == 0xCF && d == 0x8E) { a = 0xCE; b = 0xA9; }       /* ώ */
      else if (c == 0xD0 && d >= 0xB0 && d <= 0xBF) b = (unsigned char)(d - 0x20);
      else if (c == 0xD1 && d >= 0x80 && d <= 0x8F) { a = 0xD0; b = (unsigned char)(d + 0x20); }
      else if (c == 0xD1 && (d == 0x91 || d == 0x94 || d == 0x96 || d == 0x97)) {
        a = 0xD0; b = (unsigned char)(d - 0x10); }                    /* ё є і ї */
      else if (c == 0xD2 && d == 0x91) b = 0x90;                     /* ґ */
    }
    dst[o++] = (char)a;
    if (n >= 2) dst[o++] = (char)(n == 3 ? d : b);
    if (n == 3) dst[o++] = (char)e2;
    i += (size_t)n;
    /* bytes 3 e 4 de uma sequencia longa passam crus no laco seguinte: nenhum
       deles cai nas faixas acima, porque sao bytes de continuacao (0x80-0xBF). */
  }
  dst[o] = 0;
  return o;
}

// Mesma coisa, no idioma que nao muda o "i" (todos menos o turco).
static inline size_t idioma_maiusc(char *dst, size_t tam, const char *s) {
  return idioma_maiusc_em(IDIOMA_PT, dst, tam, s);
}

#endif
