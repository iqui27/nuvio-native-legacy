#include "fontefiltro.h"
#include "linguas.h"
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

/* A ordem e parte do formato salvo: acrescentar escolhas exige migracao.
   Taxonomia do DirectDebridStreamFilter do NuvioTV; somente exclusoes. */
static const char *const resolucoes[] = {"2160p", "1440p", "1080p", "720p", "576p", "480p", "360p", "Desconhecido"};
static const char *const qualidades[] = {"BluRay REMUX", "BluRay", "WEB-DL", "WEBRip", "HDRip", "HC HD-Rip", "DVDRip", "HDTV", "CAM", "TS", "TC", "SCR", "Desconhecido"};
static const char *const visuais[] = {"HDR+DV", "Somente DV", "Somente HDR", "HDR10+", "HDR10", "DV", "HDR", "HLG", "10bit", "3D", "IMAX", "AI", "SDR", "H-OU", "H-SBS", "Desconhecido"};
static const char *const audios[] = {"Atmos", "DD+", "DD", "DTS:X", "DTS-HD MA", "DTS-HD", "DTS-ES", "DTS", "TrueHD", "OPUS", "FLAC", "AAC", "Desconhecido"};
static const char *const canais[] = {"7.1", "6.1", "5.1", "2.0", "Desconhecido"};
static const char *const encodes[] = {"AV1", "HEVC", "AVC", "XviD", "DivX", "Desconhecido"};
static const char *const linguas[] = {"Inglês", "Hindi", "Italiano", "Espanhol", "Francês", "Alemão", "Português", "Polonês", "Tcheco", "Latino", "Japonês", "Coreano", "Chinês", "Multi", "Desconhecido"};
static const struct { const char *nome; const char *const *opcoes; int n; } categorias[] = {
  {"Resolução", resolucoes, 8}, {"Qualidade", qualidades, 13},
  {"Tags visuais", visuais, 16}, {"Tags de áudio", audios, 13},
  {"Canais de áudio", canais, 5}, {"Encodes", encodes, 6},
  {"Idiomas", linguas, 15}
};

int fontefiltro_opcoes_n(int c) { return c >= 0 && c < FONTEFILTRO_N ? categorias[c].n : 0; }
const char *fontefiltro_opcao(int c, int i) { return i >= 0 && i < fontefiltro_opcoes_n(c) ? categorias[c].opcoes[i] : ""; }
const char *fontefiltro_rotulo(int c) { return c >= 0 && c < FONTEFILTRO_N ? categorias[c].nome : ""; }
uint64_t fontefiltro_bit(int c, int i) { return i >= 0 && i < fontefiltro_opcoes_n(c) ? UINT64_C(1) << i : 0; }
uint64_t fontefiltro_desconhecido(int c) { return fontefiltro_bit(c, fontefiltro_opcoes_n(c) - 1); }

static int token(const char *s, const char *t) {
  size_t n = strlen(t);
  if (!s || !n) return 0;
  for (const char *p = s; *p; p++)
    if ((p == s || !isalnum((unsigned char)p[-1])) &&
        !strncasecmp(p, t, n) && !isalnum((unsigned char)p[n])) return 1;
  return 0;
}
static int algum(const char *s, const char *a, const char *b, const char *c, const char *d) {
  return token(s,a) || (b && token(s,b)) || (c && token(s,c)) || (d && token(s,d));
}
static int tokenAudio(const char *s, const char *t) {
  size_t n = strlen(t);
  if (token(s,t)) return 1;
  for (const char *p = s; *p; p++) {
    if (p != s && isalnum((unsigned char)p[-1])) continue;
    if (strncasecmp(p,t,n)) continue;
    const char *q = p+n;
    static const char *const canaisAudio[] = {"2.0","5.1","6.1","7.1","2ch","6ch","7ch","8ch"};
    for (size_t i = 0; i < sizeof canaisAudio / sizeof canaisAudio[0]; i++) {
      size_t k = strlen(canaisAudio[i]);
      if (!strncasecmp(q,canaisAudio[i],k) && !isalnum((unsigned char)q[k])) return 1;
    }
  }
  return 0;
}
static int tokenCanal(const char *s, const char *canal) {
  static const char *const prefixos[] = {"ddp","dd+","dd","eac3","e-ac3","ac3","ac-3","aac","dts","truehd"};
  if (token(s,canal)) return 1;
  for (size_t i = 0; i < sizeof prefixos / sizeof prefixos[0]; i++) {
    char combinado[32];
    snprintf(combinado,sizeof combinado,"%s%s",prefixos[i],canal);
    if (token(s,combinado)) return 1;
  }
  return 0;
}
static int tokenEspanhol(const char *s) {
  for (const char *p = s; *p; p++) {
    if ((p != s && isalnum((unsigned char)p[-1])) || strncasecmp(p,"es",2) || isalnum((unsigned char)p[2])) continue;
    /* es-419/es-la e Latino, nao uma segunda faixa de Espanhol. Mantem
       um codigo es independente quando o arquivo anuncia ambos. */
    if (p[2] == '-' || p[2] == '_') {
      if ((!strncasecmp(p+3,"419",3) && !isalnum((unsigned char)p[6])) ||
          (!strncasecmp(p+3,"la",2) && !isalnum((unsigned char)p[5]))) continue;
    }
    return 1;
  }
  return 0;
}
static uint64_t bit(int i) { return UINT64_C(1) << i; }

uint64_t fontefiltro_valores(int c, const char *s) {
  uint64_t v = 0;
  if (!s || !*s) return 0;
  switch (c) {
    case FONTEFILTRO_RESOLUCAO:
      /* Uma fonte tem uma resolucao; a maior reconhecida e a principal. */
      if (algum(s,"2160p","2160","4k","uhd")) return bit(0);
      if (algum(s,"1440p","1440","2k",NULL)) return bit(1);
      if (algum(s,"1080p","1080","fhd","full hd") || token(s,"fullhd")) return bit(2);
      if (algum(s,"720p","720",NULL,NULL)) return bit(3);
      if (algum(s,"576p","576",NULL,NULL)) return bit(4);
      if (algum(s,"480p","480","sd",NULL)) return bit(5);
      if (algum(s,"360p","360",NULL,NULL)) return bit(6);
      /* HD e apenas um campo estruturado valido: DTS-HD nao e 720p. */
      if (!strcasecmp(s,"hd")) return bit(3);
      return 0;
    case FONTEFILTRO_QUALIDADE:
      if (token(s,"remux")) return bit(0);
      if (algum(s,"bluray","blu-ray","bdrip","brrip")) return bit(1);
      if (algum(s,"web-dl","webdl","web dl",NULL)) return bit(2);
      if (algum(s,"webrip","web-rip","web rip",NULL)) return bit(3);
      if (token(s,"hdrip")) return bit(4);
      if (algum(s,"hd-rip","hcrip","hc hd-rip","hc-hdrip")) return bit(5);
      if (algum(s,"dvdrip","dvd-rip",NULL,NULL)) return bit(6);
      if (token(s,"hdtv")) return bit(7);
      if (algum(s,"cam","hdcam","camrip",NULL)) return bit(8);
      if (algum(s,"ts","hdts","telesync",NULL)) return bit(9);
      if (algum(s,"tc","hdtc","telecine",NULL)) return bit(10);
      if (algum(s,"scr","dvdscr","screener",NULL)) return bit(11);
      return 0;
    case FONTEFILTRO_VISUAL: {
      int dv = algum(s,"dv","dovi","dolby vision","dolby.vision") || token(s,"dolby-vision") || token(s,"dolbyvision");
      int plus = algum(s,"hdr10+","hdr10plus","hdr10 plus",NULL);
      int hdr10 = plus || token(s,"hdr10");
      int hdr = hdr10 || token(s,"hdr") || token(s,"hlg");
      if (dv && hdr) v |= bit(0);
      if (dv && !hdr) v |= bit(1);
      if (hdr && !dv) v |= bit(2);
      if (plus) v |= bit(3);
      if (hdr10) v |= bit(4);
      if (dv) v |= bit(5);
      if (hdr) v |= bit(6);
      if (token(s,"hlg")) v |= bit(7);
      if (algum(s,"10bit","10-bit","10 bit",NULL)) v |= bit(8);
      if (token(s,"3d")) v |= bit(9);
      if (token(s,"imax")) v |= bit(10);
      if (token(s,"ai")) v |= bit(11);
      if (token(s,"sdr")) v |= bit(12);
      if (algum(s,"h-ou","hou","half-ou",NULL)) v |= bit(13);
      if (algum(s,"h-sbs","hsbs","half-sbs",NULL)) v |= bit(14);
      return v;
    }
    case FONTEFILTRO_AUDIO: {
      int ddp = tokenAudio(s,"dd+") || tokenAudio(s,"ddp") || tokenAudio(s,"eac3") || tokenAudio(s,"e-ac3") || token(s,"dolby digital plus");
      if (token(s,"atmos")) v |= bit(0);
      if (ddp) v |= bit(1);
      if (ddp || tokenAudio(s,"dd") || tokenAudio(s,"ac3") || tokenAudio(s,"ac-3") || token(s,"dolby digital")) v |= bit(2);
      if (algum(s,"dts:x","dtsx","dts-x",NULL)) v |= bit(3);
      if (algum(s,"dts-hd ma","dtshd ma","dts-hd.ma","dts.hd.ma") || token(s,"dtshdma") || token(s,"dts hd ma")) v |= bit(4);
      if ((v & bit(4)) || algum(s,"dts-hd","dtshd","dts.hd",NULL)) v |= bit(5);
      if (algum(s,"dts-es","dtses","dts.es",NULL)) v |= bit(6);
      if ((v & (bit(3)|bit(4)|bit(5)|bit(6))) || tokenAudio(s,"dts")) v |= bit(7);
      if (algum(s,"truehd","true hd","true-hd",NULL)) v |= bit(8);
      if (token(s,"opus")) v |= bit(9);
      if (token(s,"flac")) v |= bit(10);
      if (tokenAudio(s,"aac")) v |= bit(11);
      return v;
    }
    case FONTEFILTRO_CANAIS:
      if (tokenCanal(s,"7.1") || token(s,"8ch")) v |= bit(0);
      if (tokenCanal(s,"6.1") || token(s,"7ch")) v |= bit(1);
      if (tokenCanal(s,"5.1") || token(s,"6ch")) v |= bit(2);
      if (tokenCanal(s,"2.0") || token(s,"2ch") || token(s,"stereo")) v |= bit(3);
      return v;
    case FONTEFILTRO_ENCODE:
      if (token(s,"av1")) return bit(0);
      if (algum(s,"hevc","h265","x265","h.265") || token(s,"h-265")) return bit(1);
      if (algum(s,"avc","h264","x264","h.264") || token(s,"h-264")) return bit(2);
      if (token(s,"xvid")) return bit(3);
      if (token(s,"divx")) return bit(4);
      return 0;
    case FONTEFILTRO_LINGUAS: {
      static const char *const nomes[14][4] = {
        {"en","eng","english","ingles"}, {"hi","hin","hindi",NULL},
        {"it","ita","italian","italiano"}, {"es","spa","spanish","espanhol"},
        {"fr","fra","french","frances"}, {"de","deu","german","alemao"},
        {"pt","por","portuguese","portugues"}, {"pl","pol","polish","polones"},
        {"cs","ces","czech","tcheco"}, {"la","latam","latino","es-419"},
        {"ja","jpn","japanese","japones"}, {"ko","kor","korean","coreano"},
        {"zh","zho","chinese","chines"}, {"multi","multilingual","multi-language",NULL}
      };
      for (int i = 0; i < 14; i++) {
        int tem = i == 3 ? tokenEspanhol(s) || algum(s,nomes[i][1],nomes[i][2],nomes[i][3],NULL) :
                          algum(s,nomes[i][0],nomes[i][1],nomes[i][2],nomes[i][3]);
        if (tem) v |= bit(i);
      }
      if (token(s,"fre")) v |= bit(4);
      if (token(s,"ger")) v |= bit(5);
      if (token(s,"cze")) v |= bit(8);
      if (token(s,"chi")) v |= bit(12);
      if (token(s,"es-la") || token(s,"es_la") || token(s,"es_419") || token(s,"latin american")) v |= bit(9);
      return v;
    }
    default: return 0;
  }
}

static int grupoChar(unsigned char c) { return isalnum(c) && c < 128 ? 1 : c == '.' || c == '_' || c == '-' || c == '+'; }
int fontefiltro_grupos_normalizar(const char *csv, char *saida, unsigned n) {
  char tmp[512] = {0};
  size_t u = 0;
  if (!csv || !saida || !n) return 0;
  size_t comprimento = 0;
  while (comprimento < 512 && csv[comprimento]) comprimento++;
  if (comprimento == 512) return 0;
  for (const char *p = csv; *p;) {
    const char *a = p, *f;
    char grupo[80];
    size_t k;
    while (*p && *p != ',') p++;
    f = p;
    while (a < f && (*a == ' ' || *a == '\t')) a++;
    while (f > a && (f[-1] == ' ' || f[-1] == '\t')) f--;
    k = (size_t)(f-a);
    if (k) {
      if (k >= sizeof grupo) return 0;
      for (size_t i = 0; i < k; i++) if (!grupoChar((unsigned char)a[i])) return 0;
      memcpy(grupo,a,k); grupo[k] = 0;
      int repetido = 0;
      for (const char *q = tmp; *q;) {
        const char *e = strchr(q,','); size_t z = e ? (size_t)(e-q) : strlen(q);
        if (z == k && !strncasecmp(q,grupo,k)) { repetido = 1; break; }
        q += z; if (*q) q++;
      }
      if (!repetido) {
        if (u + k + (u != 0) >= sizeof tmp) return 0;
        if (u) tmp[u++] = ',';
        memcpy(tmp+u,grupo,k); u += k; tmp[u] = 0;
      }
    }
    if (*p) p++;
  }
  if (u >= n) return 0;
  memcpy(saida,tmp,u+1); return 1;
}

int fontefiltro_config_validar(const FonteFiltroConfig *cfg) {
  char grupos[512];
  if (!cfg || cfg->maxGB > FONTEFILTRO_MAX_GB || !memchr(cfg->grupos,0,sizeof cfg->grupos)) return 0;
  for (int c = 0; c < FONTEFILTRO_N; c++)
    if (cfg->excluir[c] & ~((UINT64_C(1) << fontefiltro_opcoes_n(c))-1)) return 0;
  return fontefiltro_grupos_normalizar(cfg->grupos,grupos,sizeof grupos);
}

/* Apenas texto de metadados: URLs e legendas nao sao tags da faixa de audio.
   Copias limitadas evitam depender do tamanho dos campos de um addon. */
static void textoSeguro(const char *s, char *dst, size_t n) {
  size_t u = 0;
  dst[0] = 0;
  if (!s) return;
  while (*s && u + 1 < n) {
    char linha[2048]; size_t k = 0;
    while (*s && *s != '\n') { if (k + 1 < sizeof linha) linha[k++] = *s; s++; }
    linha[k] = 0; if (*s) s++;
    if (strstr(linha,"://") || token(linha,"subtitle") || token(linha,"subtitles") || token(linha,"legendas") || token(linha,"legenda") || token(linha,"subs") || token(linha,"sub")) continue;
    if (u) dst[u++] = '\n';
    size_t z = strlen(linha); if (z > n-u-1) z = n-u-1;
    memcpy(dst+u,linha,z); u += z; dst[u] = 0;
  }
}

static uint64_t linguasFlags(const char *s) {
  /* Regional indicator UTF-8: flags apenas, nunca substrings de titulos. */
  static const struct { char a,b; int lingua; } flags[] = {
    {'U','S',0},{'G','B',0},{'I','N',1},{'I','T',2},{'E','S',3},{'F','R',4},
    {'D','E',5},{'P','T',6},{'B','R',6},{'P','L',7},{'C','Z',8},{'M','X',9},
    {'J','P',10},{'K','R',11},{'C','N',12},{'T','W',12}
  };
  uint64_t v = 0;
  size_t n = strlen(s);
  for (size_t i = 0; i + 7 < n; i++) {
    const unsigned char *p = (const unsigned char *)s + i;
    if (p[0] != 0xF0 || p[1] != 0x9F || p[2] != 0x87 || p[3] < 0xA6 || p[3] > 0xBF ||
        p[4] != 0xF0 || p[5] != 0x9F || p[6] != 0x87 || p[7] < 0xA6 || p[7] > 0xBF) continue;
    for (size_t j = 0; j < sizeof flags / sizeof flags[0]; j++)
      if (p[3]-0xA6 == flags[j].a-'A' && p[7]-0xA6 == flags[j].b-'A') v |= bit(flags[j].lingua);
    i += 7;
  }
  return v;
}

static uint64_t linguasNome(const char *s) {
  uint64_t v = 0;
  const char *anchor = NULL;
  /* Nome de filme antes das tags nao vira idioma: It / English Patient.
     Idioma entre colchetes e metadado explicito; depois de tags tecnicas
     tambem e seguro reconhecer os codigos curtos. */
  for (const char *p = s; *p; p++) {
    if (*p == '[' || *p == '(') {
      char b[160]; size_t k = 0; const char fim = *p == '[' ? ']' : ')';
      const char *q = p+1;
      while (*q && *q != fim && k+1 < sizeof b) b[k++] = *q++;
      b[k] = 0;
      if (*q == fim) {
        uint64_t marcas = 0; int valido = 1;
        /* Colchetes com palavras de titulo nao sao uma lista de idiomas. */
        for (char *a = b; *a;) {
          char item[80]; size_t z = 0;
          while (*a == ' ' || *a == '\t' || *a == ',' || *a == '/' || *a == '|') a++;
          while (*a && *a != ',' && *a != '/' && *a != '|') { if (z+1 < sizeof item) item[z++] = *a; a++; }
          while (z && (item[z-1] == ' ' || item[z-1] == '\t')) z--;
          item[z] = 0;
          if (!z) continue;
          if (strchr(item,' ') || strchr(item,'\t') || strchr(item,'.') || strchr(item,'_')) { valido = 0; break; }
          uint64_t m = fontefiltro_valores(FONTEFILTRO_LINGUAS,item);
          if (!m) { valido = 0; break; }
          marcas |= m;
        }
        if (valido) v |= marcas;
      }
    }
    if (p == s || !isalnum((unsigned char)p[-1])) {
      static const char *const tags[] = {"2160p","2160","1440p","1440","1080p","1080","720p","720","576p","576","480p","480","360p","360","4k","2k","uhd","fhd","remux","bluray","blu-ray","bdrip","brrip","web-dl","webdl","webrip","hdrip","hdtv","hevc","h265","h.265","x265","avc","h264","h.264","x264","av1","xvid","divx"};
      for (size_t i = 0; i < sizeof tags / sizeof tags[0]; i++) {
        size_t n = strlen(tags[i]);
        if (!strncasecmp(p,tags[i],n) && !isalnum((unsigned char)p[n])) { anchor = p; break; }
      }
      if (anchor) break;
    }
  }
  if (anchor) v |= fontefiltro_valores(FONTEFILTRO_LINGUAS,anchor);
  return v | linguasFlags(s);
}

static int nomeTecnico(const char *s) {
  /* Torrentio normalmente poe o nome de release na primeira linha de title,
     sem behaviorHints.filename. Exige evidencias de arquivo/metadados para
     nao interpretar o titulo, elenco ou nome do addon como release. */
  const char *p = s;
  while (*p == ' ' || *p == '\t') p++;
  static const char *const prefixos[] = {"cast:","actors:","actor:","provider:","addon:","source:","elenco:"};
  for (size_t i = 0; i < sizeof prefixos / sizeof prefixos[0]; i++)
    if (!strncasecmp(p,prefixos[i],strlen(prefixos[i]))) return 0;
  if (strstr(p,"://")) return 0;
  uint64_t r = fontefiltro_valores(FONTEFILTRO_RESOLUCAO,p);
  uint64_t q = fontefiltro_valores(FONTEFILTRO_QUALIDADE,p) & UINT64_C(255);
  uint64_t e = fontefiltro_valores(FONTEFILTRO_ENCODE,p);
  size_t n = strlen(p); while (n && isspace((unsigned char)p[n-1])) n--;
  static const char *const exts[] = {".mkv",".mp4",".avi",".m4v",".mov",".wmv",".webm",".m2ts",".ts"};
  int arquivo = 0, ano = 0;
  for (size_t i = 0; i < sizeof exts / sizeof exts[0]; i++) {
    size_t k = strlen(exts[i]);
    if (n >= k && !strncasecmp(p+n-k,exts[i],k)) arquivo = 1;
  }
  for (const char *a = p; *a; a++) {
    if (a != p && isalnum((unsigned char)a[-1])) continue;
    if (!isdigit((unsigned char)a[0])) continue;
    size_t z = 0; unsigned valor = 0;
    while (z < 4 && isdigit((unsigned char)a[z])) { valor = valor*10 + (unsigned)(a[z]-'0'); z++; }
    if (z == 4 && !isalnum((unsigned char)a[z]) && valor >= 1900 && valor <= 2099) { ano = 1; break; }
  }
  return (r && (q || e)) || (arquivo && (r || q || e)) ||
         (ano && (q || e) && strpbrk(p,"._-["));
}

static uint64_t qualidadeFallback(const char *s) {
  char semExt[12290];
  size_t n = strlen(s); if (n >= sizeof semExt) n = sizeof semExt-1;
  memcpy(semExt,s,n); semExt[n] = 0;
  /* .ts e extensao de transporte, nao um rotulo de qualidade Telesync. */
  for (char *p = semExt; *p;) {
    char *fim = strchr(p,'\n'); if (!fim) fim = p+strlen(p);
    char *f = fim; while (f > p && isspace((unsigned char)f[-1])) f--;
    if (f-p >= 3 && !strncasecmp(f-3,".ts",3)) memset(f-3,' ',3);
    p = *fim ? fim+1 : fim;
  }
  return fontefiltro_valores(FONTEFILTRO_QUALIDADE,semExt);
}

static uint64_t linguasTexto(const char *s) {
  uint64_t v = linguasFlags(s);
  while (*s) {
    char linha[2048]; size_t n = 0;
    while (*s && *s != '\n') { if (n+1 < sizeof linha) linha[n++] = *s; s++; }
    linha[n] = 0; if (*s) s++;
    const char *p = linha; while (*p == ' ' || *p == '\t') p++;
    static const char *const prefixos[] = {"languages:","language:","langs:","lang:","audio:","audio languages:","audio language:","idiomas:","idioma:"};
    for (size_t i = 0; i < sizeof prefixos / sizeof prefixos[0]; i++)
      if (!strncasecmp(p,prefixos[i],strlen(prefixos[i]))) { v |= fontefiltro_valores(FONTEFILTRO_LINGUAS,p+strlen(prefixos[i])); break; }
    if (nomeTecnico(p)) v |= linguasNome(p);
  }
  return v;
}

static void grupoNome(const char *s, char dst[80]) {
  char nome[2048];
  const char *p, *a;
  size_t n;
  dst[0] = 0;
  if (!s || !*s || strstr(s,"://") || strlen(s) >= sizeof nome) return;
  p = strrchr(s,'/'); if (p) s = p+1;
  p = strrchr(s,'\\'); if (p) s = p+1;
  n = strlen(s); while (n && isspace((unsigned char)s[n-1])) n--;
  memcpy(nome,s,n); nome[n] = 0;
  static const char *const exts[] = {".mkv",".mp4",".avi",".m4v",".mov",".wmv",".webm",".m2ts",".ts"};
  for (size_t i = 0; i < sizeof exts / sizeof exts[0]; i++) {
    size_t k = strlen(exts[i]);
    if (n >= k && !strcasecmp(nome+n-k,exts[i])) { nome[n-k] = 0; n -= k; break; }
  }
  /* Releases como Film.1080p.WEBRip.[YTS.MX].mp4. Colchetes contendo
     resolucao, idioma ou outra tag de formato nao sao grupos. */
  if (n && nome[n-1] == ']') {
    char *inicio = strrchr(nome,'[');
    if (inicio && inicio+1 < nome+n-1) {
      nome[n-1] = 0;
      const char *g = inicio+1; size_t k = strlen(g); int ok = k < 80, letra = 0;
      for (size_t i = 0; i < k; i++) {
        if (!grupoChar((unsigned char)g[i])) ok = 0;
        if (isalpha((unsigned char)g[i])) letra = 1;
      }
      for (int c = 0; c < FONTEFILTRO_N; c++) if (fontefiltro_valores(c,g)) ok = 0;
      if (token(g,"unknown") || token(g,"desconhecido")) ok = 0;
      if (ok && letra) { memcpy(dst,g,k+1); return; }
      return;
    }
  }
  a = strrchr(nome,'-'); if (!a || !a[1]) return;
  a++; n = strlen(a); if (n >= 80) return;
  for (size_t i = 0; i < n; i++) if (!grupoChar((unsigned char)a[i])) return;
  /* Hifens de tags tecnicas nao anunciam um grupo de release. */
  if (!strcasecmp(a,"dl") || !strcasecmp(a,"ray") || !strcasecmp(a,"rip") ||
      !strcasecmp(a,"hd") || !strcasecmp(a,"ma") || !strcasecmp(a,"es") ||
      !strcasecmp(a,"ou") || !strcasecmp(a,"sbs") ||
      fontefiltro_valores(FONTEFILTRO_ENCODE,a) || fontefiltro_valores(FONTEFILTRO_RESOLUCAO,a)) return;
  memcpy(dst,a,n+1);
}

static uint64_t linguasDeclaradas(const char *s) {
  uint64_t v = 0;
  for (const char *p = s; p && *p;) {
    const char *fim = p + strcspn(p, ",|;");
    char item[512], codigo[32];
    size_t n = (size_t)(fim - p);
    if (n >= sizeof item) n = sizeof item - 1;
    memcpy(item, p, n); item[n] = 0;
    ling_normalizar(item, codigo, sizeof codigo);
    uint64_t m = fontefiltro_valores(FONTEFILTRO_LINGUAS, item) |
                 fontefiltro_valores(FONTEFILTRO_LINGUAS, codigo);
    v |= m ? m : fontefiltro_desconhecido(FONTEFILTRO_LINGUAS);
    p = *fim ? fim + 1 : fim;
  }
  return v;
}

void fontefiltro_classificar(const FonteFiltroEntrada *e, FonteFiltroDados *d) {
  char nome[4096], texto[8192], busca[12290];
  if (!d) return;
  memset(d,0,sizeof *d);
  if (!e) return;
  textoSeguro(e->filename,nome,sizeof nome); textoSeguro(e->texto,texto,sizeof texto);
  snprintf(busca,sizeof busca,"%s\n%s",nome,texto);
  const char *campos[FONTEFILTRO_N] = {e->resolucao,e->qualidade,e->visual,e->audio,e->canais,e->encode,e->linguas};
  for (int c = 0; c < FONTEFILTRO_N; c++) {
    d->valores[c] = c == FONTEFILTRO_LINGUAS ? linguasDeclaradas(campos[c]) :
                    fontefiltro_valores(c,campos[c]);
    if (c == FONTEFILTRO_VISUAL || c == FONTEFILTRO_AUDIO || c == FONTEFILTRO_CANAIS) {
      d->valores[c] |= fontefiltro_valores(c,busca);
      /* Recalcula exclusivos/combinado sobre o conjunto anunciado. */
      if (c == FONTEFILTRO_VISUAL) {
        int dv = (d->valores[c] & bit(5)) != 0, hdr = (d->valores[c] & bit(6)) != 0;
        d->valores[c] &= ~(bit(0)|bit(1)|bit(2));
        if (dv && hdr) d->valores[c] |= bit(0);
        if (dv && !hdr) d->valores[c] |= bit(1);
        if (hdr && !dv) d->valores[c] |= bit(2);
      }
      continue;
    }
    if (!d->valores[c] && !(campos[c] && (token(campos[c],"unknown") || token(campos[c],"desconhecido"))))
      d->valores[c] = c == FONTEFILTRO_LINGUAS ? linguasNome(nome) | linguasTexto(texto) :
                      c == FONTEFILTRO_QUALIDADE ? qualidadeFallback(busca) : fontefiltro_valores(c,busca);
  }
  d->tamanhoBytes = e->tamanhoBytes;
  if (e->grupo && *e->grupo && strlen(e->grupo) < sizeof d->grupo) {
    size_t n = strlen(e->grupo); int ok = 1;
    for (size_t i = 0; i < n; i++) if (!grupoChar((unsigned char)e->grupo[i])) ok = 0;
    if (ok) memcpy(d->grupo,e->grupo,n+1);
  }
  if (!d->grupo[0]) grupoNome(nome,d->grupo);
  if (!d->grupo[0]) {
    const char *p = texto;
    while (*p && !d->grupo[0]) {
      char linha[2048]; size_t k = 0;
      while (*p && *p != '\n') { if (k+1 < sizeof linha) linha[k++] = *p; p++; }
      linha[k] = 0; if (*p) p++;
      if (nomeTecnico(linha)) grupoNome(linha,d->grupo);
    }
  }
}

int fontefiltro_permite(const FonteFiltroConfig *cfg, const FonteFiltroDados *d) {
  if (!cfg || !d || d->naoVod) return 1;
  for (int c = 0; c < FONTEFILTRO_N; c++) {
    uint64_t v = d->valores[c] & (fontefiltro_desconhecido(c)-1);
    if (c == FONTEFILTRO_LINGUAS) v |= d->valores[c] & fontefiltro_desconhecido(c);
    if (!v) v = fontefiltro_desconhecido(c);
    if (c == FONTEFILTRO_LINGUAS) {
      if ((v & cfg->excluir[c]) == v) return 0;
    } else if (v & cfg->excluir[c]) return 0;
  }
  if (cfg->maxGB && d->tamanhoBytes > (uint64_t)cfg->maxGB * UINT64_C(1000000000)) return 0;
  if (d->grupo[0]) {
    size_t n = strlen(d->grupo);
    for (const char *p = cfg->grupos; *p;) {
      const char *a = p, *f;
      while (*p && *p != ',') p++;
      f = p;
      while (a < f && (*a == ' ' || *a == '\t')) a++;
      while (f > a && (f[-1] == ' ' || f[-1] == '\t')) f--;
      if ((size_t)(f-a) == n && !strncasecmp(a,d->grupo,n)) return 0;
      if (*p) p++;
    }
  }
  return 1;
}
