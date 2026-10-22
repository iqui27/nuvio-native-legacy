#include "mkv.h"
#include "rede.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Quanto do arquivo baixar. O elemento Tracks fica logo apos o SeekHead e o
// Info, antes do primeiro Cluster — na pratica dentro dos primeiros 200 KB.
//
// 320 KB e nao 2 MB. Os 2 MB eram "folga larga" para remux com capa embutida
// antes do Tracks, e custavam caro no unico momento em que esta leitura
// acontece: COM O VIDEO JA TOCANDO, pela mesma conexao e do mesmo servidor.
// MEDIDO na TV do dono — a leitura terminou aos 45,9 s e o buffer entrou em
// falta 1,6 s depois, caindo a 2,8 s e levando 9 s para se recuperar.
//
// O caso que os 2 MB cobriam (capa antes do Tracks) e raro; o custo era pago
// em TODA reproducao. Perder o idioma num arquivo desses e melhor que engasgar
// o video em todos.
#define MKV_TRECHO  (320L * 1024)

// --- EBML: inteiros de tamanho variavel --------------------------------------
//
// O primeiro byte diz, pela posicao do bit 1 mais alto, quantos bytes o numero
// ocupa. No ID esse bit FAZ PARTE do valor (por isso os IDs sao escritos como
// 0x1A45DFA3); no TAMANHO ele e mascara e sai fora. Trocar os dois e o erro
// classico de quem escreve isto pela primeira vez, e o sintoma e a arvore
// inteira sair deslocada.
static int larguraDe(unsigned char b) {
  int i;
  for (i = 0; i < 8; i++) if (b & (0x80 >> i)) return i + 1;
  return 0;                       // byte 0x00: invalido em EBML
}

// Le um ID (mantendo o bit marcador). 0 e o fim ou dado invalido.
static unsigned long lerId(const unsigned char *p, long resta, int *usou) {
  int w, i;
  unsigned long v;
  if (resta < 1) return 0;
  w = larguraDe(p[0]);
  if (w < 1 || w > 4 || resta < w) return 0;
  v = 0;
  for (i = 0; i < w; i++) v = (v << 8) | p[i];
  *usou = w;
  return v;
}

// Le um TAMANHO (removendo o bit marcador). Devolve -1 no invalido e -2 no
// tamanho "desconhecido" (todos os bits de dado em 1), que Segment usa em
// arquivo transmitido ao vivo — ali a leitura continua DENTRO do elemento em
// vez de pular por cima dele.
static long lerTam(const unsigned char *p, long resta, int *usou) {
  int w, i;
  unsigned long v;
  int todosUm = 1;
  if (resta < 1) return -1;
  w = larguraDe(p[0]);
  if (w < 1 || w > 8 || resta < w) return -1;
  v = p[0] & (0xFF >> w);
  if ((unsigned char)(p[0] & (0xFF >> w)) != (unsigned char)(0xFF >> w)) todosUm = 0;
  for (i = 1; i < w; i++) {
    if (p[i] != 0xFF) todosUm = 0;
    v = (v << 8) | p[i];
  }
  *usou = w;
  if (todosUm) return -2;
  return (long)v;
}

// 64 BITS DE PROPOSITO (unsigned long tem 32 na TV ARM de 32 bits). O
// ChapterTimeStart vem em NANOSSEGUNDOS: 7405 s sao 7,4e12, e em 32 bits o
// valor dava a volta a cada 4,29 s. Era o "creditos nomeados em 0s, ultimo
// \"End Credits\" em 0s" do log da C9 (capitulo final de um filme de 7712 s).
static uint64_t lerUint(const unsigned char *p, long n) {
  uint64_t v = 0;
  long i;
  if (n < 1 || n > 8) return 0;
  for (i = 0; i < n; i++) v = (v << 8) | p[i];
  return v;
}

static void lerTexto(const unsigned char *p, long n, char *dst, size_t tam) {
  size_t k = (size_t)n;
  if (k > tam - 1) k = tam - 1;
  memcpy(dst, p, k);
  dst[k] = 0;
  // O Matroska preenche string com NUL a direita; cortar aqui evita que o
  // resto do campo vire lixo na tela.
  { size_t i; for (i = 0; i < k; i++) if (dst[i] == 0) { dst[i] = 0; break; } }
}

// --- ids que interessam ------------------------------------------------------
#define ID_SEGMENT     0x18538067UL
#define ID_TRACKS      0x1654AE6BUL
#define ID_TRACKENTRY  0xAEUL
#define ID_TRACKNUMBER 0xD7UL
#define ID_TRACKTYPE   0x83UL
#define ID_LANGUAGE    0x22B59CUL     // Language (ISO 639-2), o classico
#define ID_LANG_BCP47  0x22B59DUL     // LanguageBCP47 ("pt-BR"), mais novo
#define ID_NAME        0x536EUL
#define ID_CODECID     0x86UL
#define ID_FLAGFORCED  0x55AAUL      // FlagForced: faixa so de letreiros/falas estrangeiras
#define ID_FLAGHEARING 0x55ABUL      // FlagHearingImpaired: SDH (#287)
#define ID_AUDIO       0xE1UL        // Audio (mestre dentro da TrackEntry)
#define ID_CHANNELS    0x9FUL        // Audio > Channels
#define ID_BLOCKADDMAP 0x41E4UL      // BlockAdditionMapping (Dolby Vision config)
#define ID_BLOCKADDTYPE 0x41E7UL     // BlockAddIDType: 'dvcC' / 'dvvC'
#define ID_BLOCKADDDATA 0x41EDUL     // BlockAddIDExtraData: the DOVI record
#define ID_CODECPRIV   0x63A2UL      // CodecPrivate (hvcC no HEVC)

// hvcC (ISO/IEC 14496-15): lengthSizeMinusOne no byte 21, numOfArrays no 22,
// depois arrays {tipo&0x3F, u16 numNalus, {u16 len, NAL}...}. Marca se algum
// array e de NAL tipo 62 (RPU de Dolby Vision) — lugar NAO padrao, mas e o
// unico outro canto do cabecalho onde um remux poderia ter posto o DV.
static void lerHvcc(const unsigned char *v, long n, MkvFaixa *f) {
  long o;
  int a, nA;
  if (n < 23 || v[0] != 1) return;
  f->nalTam = (v[21] & 3) + 1;
  nA = v[22];
  o = 23;
  for (a = 0; a < nA && o + 3 <= n; a++) {
    int tipo = v[o] & 0x3F, k, nN = (v[o + 1] << 8) | v[o + 2];
    o += 3;
    if (tipo == 62) f->hvccNal62 = 1;
    for (k = 0; k < nN && o + 2 <= n; k++) o += 2 + ((v[o] << 8) | v[o + 1]);
  }
}

// Le os TrackEntry de dentro de um Tracks ja localizado.
static int lerTracks(const unsigned char *p, long n, MkvFaixa *saida, int max) {
  long o = 0;
  int achou = 0;
  while (o < n && achou < max) {
    int ui = 0, ut = 0;
    unsigned long id = lerId(p + o, n - o, &ui);
    long tam;
    if (!id) break;
    tam = lerTam(p + o + ui, n - o - ui, &ut);
    if (tam < 0) break;
    o += ui + ut;
    if (o + tam > n) break;
    if (id == ID_TRACKENTRY) {
      MkvFaixa f;
      long q = 0;
      memset(&f, 0, sizeof f);
      while (q < tam) {
        int vi = 0, vt = 0;
        unsigned long fid = lerId(p + o + q, tam - q, &vi);
        long ftam;
        if (!fid) break;
        ftam = lerTam(p + o + q + vi, tam - q - vi, &vt);
        if (ftam < 0) break;
        q += vi + vt;
        if (q + ftam > tam) break;
        { const unsigned char *v = p + o + q;
          if (fid == ID_TRACKNUMBER) f.numero = (int)lerUint(v, ftam);
          else if (fid == ID_TRACKTYPE) f.tipo = (int)lerUint(v, ftam);
          else if (fid == ID_LANGUAGE || fid == ID_LANG_BCP47) {
            // BCP47 ganha do ISO 639-2 quando os dois existem: "pt-BR" diz
            // mais que "por", e e o que o dono quer ver na lista.
            if (fid == ID_LANG_BCP47 || !f.idioma[0])
              lerTexto(v, ftam, f.idioma, sizeof f.idioma);
          }
          else if (fid == ID_NAME)    lerTexto(v, ftam, f.nome,  sizeof f.nome);
          else if (fid == ID_CODECID) lerTexto(v, ftam, f.codec, sizeof f.codec);
          else if (fid == ID_CODECPRIV) { f.cpN = ftam; lerHvcc(v, ftam, &f); }
          else if (fid == ID_FLAGFORCED) f.forcado = lerUint(v, ftam) != 0;
          else if (fid == ID_FLAGHEARING) f.sdh = lerUint(v, ftam) != 0;
          // Channels mora um nivel abaixo, em Audio. O .tpk nao tem outra
          // fonte para "5.1" (o Tizen.Multimedia.Player so da o idioma, #206).
          else if (fid == ID_AUDIO) {
            long r = 0;
            while (r < ftam) {
              int ai = 0, at = 0;
              unsigned long aid = lerId(v + r, ftam - r, &ai);
              long atam;
              if (!aid) break;
              atam = lerTam(v + r + ai, ftam - r - ai, &at);
              if (atam < 0 || r + ai + at + atam > ftam) break;
              r += ai + at;
              if (aid == ID_CHANNELS) f.canais = (int)lerUint(v + r, atam);
              r += atam;
            }
          }
          // DOVIDecoderConfigurationRecord (ETSI/Dolby): version major, minor,
          // then profile(7) level(6) rpu(1) el(1) bl(1), compatibility id(4).
          else if (fid == ID_BLOCKADDMAP) {
            long r = 0;
            unsigned long tipo = 0;
            const unsigned char *dado = NULL;
            long dadoN = 0;
            while (r < ftam) {
              int ai = 0, at = 0;
              unsigned long aid = lerId(v + r, ftam - r, &ai);
              long atam;
              if (!aid) break;
              atam = lerTam(v + r + ai, ftam - r - ai, &at);
              if (atam < 0 || r + ai + at + atam > ftam) break;
              r += ai + at;
              if (aid == ID_BLOCKADDTYPE) tipo = (unsigned long)lerUint(v + r, atam);
              else if (aid == ID_BLOCKADDDATA) { dado = v + r; dadoN = atam; }
              r += atam;
            }
            f.bamN++; f.bamTipo = tipo;
            if ((tipo == 0x64766343UL || tipo == 0x64767643UL) && dado && dadoN >= 5) {
              f.dvPerfil = dado[2] >> 1;
              f.dvNivel  = ((dado[2] & 1) << 5) | (dado[3] >> 3);
              f.dvRpu    = (dado[3] >> 2) & 1;
              f.dvEl     = (dado[3] >> 1) & 1;
              f.dvBl     = dado[3] & 1;
              f.dvCompat = dado[4] >> 4;
            }
          } }
        q += ftam;
      }
      f.entradaInteira = q == tam;
      if (f.numero > 0) saida[achou++] = f;
    }
    o += tam;
  }
  return achou;
}

// --- capitulos ---------------------------------------------------------------
#define ID_CHAPTERS    0x1043A770UL
#define ID_EDITION     0x45B9UL
#define ID_CHAPATOM    0xB6UL
#define ID_CHAPSTART   0x91UL
#define ID_CHAPDISPLAY 0x80UL
#define ID_CHAPSTRING  0x85UL

// ChapterTimeStart e em NANOSSEGUNDOS ABSOLUTOS, e nao em unidades de
// TimecodeScale — e a excecao do formato, e trocar os dois daria um numero mil
// vezes errado sem parecer errado (um filme de 105 min viraria 105 ms).
static int lerCapitulos(const unsigned char *p, long n, MkvCap *saida, int max) {
  long o = 0;
  int achou = 0;
  while (o < n && achou < max) {
    int ui = 0, ut = 0;
    unsigned long id = lerId(p + o, n - o, &ui);
    long tam;
    if (!id) break;
    tam = lerTam(p + o + ui, n - o - ui, &ut);
    if (tam < 0) break;
    o += ui + ut;
    if (o + tam > n) break;
    if (id == ID_EDITION) {
      // Uma edicao contem os atomos; descer sem pular. So a PRIMEIRA edicao
      // vale: varias edicoes concatenadas bagunçariam a ordem ("ultimo
      // capitulo") e repetiriam nomes.
      if (!achou) achou += lerCapitulos(p + o, tam, saida + achou, max - achou);
    } else if (id == ID_CHAPATOM) {
      MkvCap c;
      long q = 0;
      int temInicio = 0;
      memset(&c, 0, sizeof c);
      while (q < tam) {
        int vi = 0, vt = 0;
        unsigned long fid = lerId(p + o + q, tam - q, &vi);
        long ftam;
        if (!fid) break;
        ftam = lerTam(p + o + q + vi, tam - q - vi, &vt);
        if (ftam < 0) break;
        q += vi + vt;
        if (q + ftam > tam) break;
        { const unsigned char *v = p + o + q;
          if (fid == ID_CHAPSTART) {
            c.inicio = (double)lerUint(v, ftam) / 1000000000.0;
            temInicio = 1;
          } else if (fid == ID_CHAPDISPLAY) {
            // O nome mora um nivel abaixo, em ChapString.
            long r = 0;
            while (r < ftam) {
              int di = 0, dt = 0;
              unsigned long did = lerId(v + r, ftam - r, &di);
              long dtam;
              if (!did) break;
              dtam = lerTam(v + r + di, ftam - r - di, &dt);
              if (dtam < 0) break;
              r += di + dt;
              if (r + dtam > ftam) break;
              if (did == ID_CHAPSTRING) lerTexto(v + r, dtam, c.nome, sizeof c.nome);
              r += dtam;
            }
          } }
        q += ftam;
      }
      if (temInicio) saida[achou++] = c;
    }
    o += tam;
  }
  return achou;
}

// Anda pela arvore ate achar Tracks. Entra em Segment (que e um contentor
// gigante) e PULA o resto — sem o pulo a busca varreria byte a byte e casaria
// com qualquer coincidencia dentro dos dados de video.
// 1 quando a ultima acharTracks achou Tracks CORTADO pelo fim do trecho (lista
// incompleta). So mkv_faixas_do_trecho olha: ali o trecho e o da pre-busca, e
// lista incompleta manda a sonda para a rede como antes.
static int tracksCortado;
// Para o mkv_diag: Tracks visto na ultima acharTracks, e o tamanho declarado.
static int tkAchado;
static long tkTam;
// Onde o Chapters mora, pelo SeekHead (203-capitulos): posicao ABSOLUTA no
// arquivo (-1 = o SeekHead nao disse). 64 bits: Chapters no fim de um remux de
// mais de 2 GB nao cabe num long de 32 bits.
static long long segIni, posChapters;
#define ID_SEEKHEAD 0x114D9B74UL
#define ID_SEEK     0x4DBBUL
#define ID_SEEKID   0x53ABUL
#define ID_SEEKPOS  0x53ACUL

static void lerSeekHead(const unsigned char *p, long n) {
  long o = 0;
  while (o < n) {
    int ui = 0, ut = 0;
    unsigned long id = lerId(p + o, n - o, &ui);
    long tam;
    if (!id) return;
    tam = lerTam(p + o + ui, n - o - ui, &ut);
    if (tam < 0) return;
    o += ui + ut;
    if (o + tam > n) return;
    if (id == ID_SEEK) {
      unsigned long alvo = 0;
      long long pos = -1;
      long q = 0;
      while (q < tam) {
        int vi = 0, vt = 0;
        unsigned long fid = lerId(p + o + q, tam - q, &vi);
        long ftam;
        if (!fid) break;
        ftam = lerTam(p + o + q + vi, tam - q - vi, &vt);
        if (ftam < 0) break;
        q += vi + vt;
        if (q + ftam > tam) break;
        if (fid == ID_SEEKID) alvo = (unsigned long)lerUint(p + o + q, ftam);
        else if (fid == ID_SEEKPOS) pos = (long long)lerUint(p + o + q, ftam);
        q += ftam;
      }
      if (alvo == ID_CHAPTERS && pos >= 0) posChapters = segIni + pos;
    }
    o += tam;
  }
}

static int acharTracks(const unsigned char *p, long n, MkvFaixa *saida, int max,
                       MkvCap *caps, int maxCaps, int *nCaps) {
  long o = 0;
  int nFaixas = 0;
  if (nCaps) *nCaps = 0;
  tracksCortado = 0;
  tkAchado = 0; tkTam = 0;
  segIni = 0; posChapters = -1;
  while (o < n) {
    int ui = 0, ut = 0;
    unsigned long id = lerId(p + o, n - o, &ui);
    long tam;
    if (!id) return 0;
    tam = lerTam(p + o + ui, n - o - ui, &ut);
    if (tam == -1) return 0;
    o += ui + ut;
    if (id == ID_SEGMENT || tam == -2) {
      // Segment: descer para dentro. Tamanho desconhecido idem — nao ha por
      // onde pular.
      if (id == ID_SEGMENT) { segIni = o; continue; }
      return 0;
    }
    if (id == ID_SEEKHEAD && o + tam <= n) lerSeekHead(p + o, tam);
    if (id == ID_TRACKS) {
      long disp = n - o;
      long t = tam > disp ? disp : tam;   // cabecalho maior que o trecho baixado
      tkAchado = 1; tkTam = tam;
      nFaixas = lerTracks(p + o, t, saida, max);
      // Tracks cortado pelo trecho: a lista sai INCOMPLETA e o casamento pelo
      // ordinal (mkv_casar_legendas) vai dar "nenhum" — dizer isso no log e o
      // que separa "arquivo esquisito" de "trecho curto" no #92.
      if (tam > disp) {
        tracksCortado = 1;
        printf("[mkv] Tracks tem %ld bytes e o trecho baixado acaba em %ld: %d faixa(s) lidas, lista pode estar incompleta\n",
               tam, disp, nFaixas);
        fflush(stdout);
      }
      // NAO devolve aqui: Chapters vem DEPOIS de Tracks no arquivo, e sair no
      // primeiro achado era o que deixava os capitulos para tras.
      if (!caps || tam > disp) return nFaixas;
      o += tam;
      continue;
    }
    if (id == ID_CHAPTERS && caps && maxCaps > 0) {
      long disp = n - o;
      // Chapters CORTADO pelo trecho: lista pela metade erra o "ultimo
      // capitulo". Fica para a leitura por SeekHead (mkv_capitulos_alem).
      if (tam > disp) { posChapters = o - ui - ut; return nFaixas; }
      if (nCaps) *nCaps = lerCapitulos(p + o, tam, caps, maxCaps);
      posChapters = -1;
      return nFaixas;
    }
    if (o + tam > n) return nFaixas;  // elemento passa do que baixamos
    o += tam;
  }
  return nFaixas;
}

// O capitulo dos creditos: primeiro pelo NOME, depois pela posicao.
//
// Pelo nome cobre os lancamentos que etiquetam ("End Credits", "Creditos",
// "Outro"). Sem nome util, vale o ULTIMO capitulo — mas so quando ele comeca
// no ultimo quarto do arquivo: em disco com capitulo a cada 5 minutos o ultimo
// e uma cena qualquer, e trata-lo como creditos poria o painel no meio do
// terceiro ato.
//
// O ULTIMO NOME QUE CASA, e nao o primeiro (#115). Remux com "Opening Credits"
// aos 90 s e "End Credits" no fim e comum, e o primeiro casamento punha o
// painel de relacionados no comeco do filme. Varre de tras para frente.
// Nome em minusculas ASCII (o acento de "créditos" fica como esta).
static void nomeMinusculo(const char *nome, char *m, size_t tam) {
  size_t j;
  snprintf(m, tam, "%s", nome);
  for (j = 0; m[j]; j++)
    if (m[j] >= 'A' && m[j] <= 'Z') m[j] = (char)(m[j] - 'A' + 'a');
}
// `palavra` aparece em `m` como palavra inteira (nao "ed" dentro de "bed").
static int temPalavra(const char *m, const char *palavra) {
  size_t n = strlen(palavra);
  const char *p = m;
  while ((p = strstr(p, palavra)) != NULL) {
    int antes = p == m || !((p[-1] >= 'a' && p[-1] <= 'z') || (p[-1] >= '0' && p[-1] <= '9'));
    char d = p[n];
    int depois = !((d >= 'a' && d <= 'z') || (d >= '0' && d <= '9'));
    if (antes && depois) return 1;
    p += n;
  }
  return 0;
}
// Capitulo de ABERTURA que tambem diz "credits" ("Opening Credits", "OP"):
// nao e o fim. 2.0.3.
static int nomeAbertura(const char *m) {
  return strstr(m, "opening") || strstr(m, "intro") || strstr(m, "abertura") ||
         temPalavra(m, "op");
}
// Previa do proximo episodio (anime: "Preview", "Next Episode", "Yokoku"):
// vem DEPOIS do ED e nao e o inicio dos creditos.
static int nomePrevia(const char *m) {
  return strstr(m, "preview") || strstr(m, "next episode") || strstr(m, "next time") ||
         strstr(m, "yokoku") || strstr(m, "avance") || strstr(m, "previa") ||
         strstr(m, "prévia");
}

double mkv_creditos_nomeados(const MkvCap *caps, int n) {
  static const char *NOMES[] = { "credit", "crédit", "credito", "crédito",
                                 "end title", "outro", "encerrament", "ending" };
  int i, k;
  for (i = n - 1; i >= 0; i--) {
    char m[64];
    nomeMinusculo(caps[i].nome, m, sizeof m);
    if (nomeAbertura(m)) continue;
    // ANIME (2.0.3): "Ending" e "ED" sao os creditos. "ED" so como palavra.
    if (temPalavra(m, "ed")) return caps[i].inicio;
    for (k = 0; k < (int)(sizeof NOMES / sizeof NOMES[0]); k++)
      if (strstr(m, NOMES[k])) return caps[i].inicio;
  }
  return 0.0;
}

double mkv_creditos_ultimo(const MkvCap *caps, int n) {
  int i;
  if (!caps || n < 2) return 0.0;
  for (i = n - 1; i > 0; i--) {
    char m[64];
    nomeMinusculo(caps[i].nome, m, sizeof m);
    if (!nomePrevia(m)) return caps[i].inicio;
  }
  return 0.0;
}


// --- capitulos fora da janela (203-capitulos) ----------------------------------
//
// O SeekHead diz onde o Chapters mora; quando ele nao esta na janela do
// cabecalho (remux com capa/fontes antes, ou Chapters no fim do arquivo), UM
// Range busca o elemento INTEIRO — o mesmo que o 203-308 fez com o Tracks de
// 96 KB. Pequeno primeiro (64 KB cobre quase todo Chapters); se o elemento
// declara mais, um segundo pedido com o tamanho exato, limitado a 2 MB.
#define MKV_CAP_PRIMEIRO (64L * 1024)
#define MKV_CAP_TETO     (2L * 1024 * 1024)

static int capsPorPosicao(const char *url, long long pos, MkvCap *caps, int max, int *status) {
  long n = 0, need;
  int ui = 0, ut = 0, achou = 0, st = 0;
  unsigned long id;
  long tam;
  char *b = rede_baixar_trecho_st(url, 15, pos, pos + MKV_CAP_PRIMEIRO - 1, &n, &st, NULL, NULL, 0);
  if (status) *status = st;
  if (!b) return 0;
  id = lerId((const unsigned char *)b, n, &ui);
  tam = id == ID_CHAPTERS ? lerTam((const unsigned char *)b + ui, n - ui, &ut) : -1;
  if (tam < 0 || tam > MKV_CAP_TETO) { free(b); return 0; }
  need = ui + ut + tam;
  if (need > n) {
    free(b);
    b = rede_baixar_trecho_st(url, 15, pos, pos + need - 1, &n, &st, NULL, NULL, 0);
    if (status) *status = st;
    if (!b || n < need) { free(b); return 0; }
  }
  achou = lerCapitulos((const unsigned char *)b + ui + ut, tam, caps, max);
  free(b);
  return achou;
}

int mkv_capitulos_alem(const char *url, const unsigned char *cab, long cabN,
                       MkvCap *caps, int maxCaps, int *status) {
  MkvFaixa fx[MKV_MAX_FAIXAS];
  int n = 0;
  if (status) *status = -1;
  if (!url || !url[0] || !caps || maxCaps < 1 || !cab || cabN < 64 || cab[0] != 0x1A ||
      cab[1] != 0x45 || cab[2] != 0xDF || cab[3] != 0xA3) return 0;
  acharTracks(cab, cabN, fx, MKV_MAX_FAIXAS, caps, maxCaps, &n);
  if (n > 0) return n;                   // ja estava na janela
  if (posChapters < 0) return 0;         // SeekHead nao diz: nao chuta
  n = capsPorPosicao(url, posChapters, caps, maxCaps, status);
  printf("[mkv] capitulos pelo SeekHead (pos %lld): %d\n", posChapters, n);
  fflush(stdout);
  return n;
}

int mkv_intro_nomeada(const MkvCap *caps, int n, double *ini, double *fim) {
  int i;
  for (i = 0; caps && i < n - 1; i++) {
    char m[64];
    nomeMinusculo(caps[i].nome, m, sizeof m);
    if (nomeAbertura(m) && caps[i + 1].inicio > caps[i].inicio) {
      if (ini) *ini = caps[i].inicio;
      if (fim) *fim = caps[i + 1].inicio;
      return 1;
    }
  }
  return 0;
}

double mkv_previa_nomeada(const MkvCap *caps, int n) {
  char m[64];
  if (!caps || n < 2) return 0.0;
  nomeMinusculo(caps[n - 1].nome, m, sizeof m);
  return nomePrevia(m) ? caps[n - 1].inicio : 0.0;
}

// --- RPU de Dolby Vision em banda (203-dvrpu) ----------------------------------
//
// Ha remux com o RPU (NAL HEVC tipo 62, UNSPEC62) em todo quadro e SEM o dvcC
// no cabecalho: ffmpeg antigo e muxers que nao conhecem BlockAdditionMapping
// copiam o fluxo e perdem a configuracao. O Kodi tambem nao os reconhece
// (DVDDemuxFFmpeg::DetermineHdrType so olha AV_PKT_DATA_DOVI_CONF). Aqui so se
// MEDE: o primeiro quadro de video do primeiro Cluster tem NAL 62 ou nao.
#define ID_CLUSTER     0x1F43B675UL
#define ID_SIMPLEBLOCK 0xA3UL
#define ID_BLOCKGROUP  0xA0UL
#define ID_BLOCK       0xA1UL

static MkvDiag diag = { 0, 0, 0, 0, -1, 0, -1, -1, -1, -1 };
const MkvDiag *mkv_diag(void) { return &diag; }

int mkv_quadro_tem_rpu(const unsigned char *p, long n, int nalTam, int *rpuTipo, int *rpuPerfil) {
  long o = 0;
  if (!p || nalTam < 1 || nalTam > 4) return -1;
  while (o + nalTam + 2 <= n) {
    long len = 0;
    int i;
    for (i = 0; i < nalTam; i++) len = (len << 8) | p[o + i];
    o += nalTam;
    if (len < 2 || o + len > n) return -1;
    if (((p[o] >> 1) & 0x3F) == 62) {
      // rbsp sem os bytes de prevencao (00 00 03) dos primeiros bytes; depois
      // rpu_nal_prefix 0x19, rpu_type u(6), rpu_format u(11) e, se rpu_type
      // for 2, vdr_rpu_profile u(4) (0 = perfil 5; 1 = perfil 7 ou 8).
      unsigned char b[8];
      long k = o + 2;
      int m = 0, zeros = 0;
      while (k < o + len && m < 8) {
        if (zeros >= 2 && p[k] == 3) { zeros = 0; k++; continue; }
        zeros = p[k] == 0 ? zeros + 1 : 0;
        b[m++] = p[k++];
      }
      if (rpuTipo) *rpuTipo = -1;
      if (rpuPerfil) *rpuPerfil = -1;
      if (m >= 5 && b[0] == 0x19) {
        unsigned long bits = ((unsigned long)b[1] << 24) | ((unsigned long)b[2] << 16) |
                             ((unsigned long)b[3] << 8) | b[4];
        int tipo = (int)(bits >> 26);
        if (rpuTipo) *rpuTipo = tipo;
        if (tipo == 2 && rpuPerfil) *rpuPerfil = (int)((bits >> 11) & 0xF);
      }
      return 1;
    }
    o += len;
  }
  return o == n ? 0 : -1;
}

// Um Block/SimpleBlock: numero da faixa (vint), 2 bytes de tempo, 1 de flags.
// Devolve o deslocamento dos dados do quadro, ou -1 (outra faixa, lacing, curto).
static long cabBloco(const unsigned char *p, long n, int faixa) {
  int u = 0;
  long num = lerTam(p, n, &u);
  if (num != faixa || n < u + 3) return -1;
  if ((p[u + 2] >> 1) & 3) return -1;      // lacing: video nao usa
  return u + 3;
}

// Elementos de um Cluster (ou BlockGroup) a partir de p[0]; `base` e a posicao
// absoluta de p[0] e `fim` o fim do contentor (ou do trecho). 1 = decidiu.
static int rpuNoContentor(const unsigned char *p, long n, long long base, int faixa, int nalTam) {
  long o = 0;
  while (o < n) {
    int ui = 0, ut = 0;
    unsigned long id = lerId(p + o, n - o, &ui);
    long tam, h;
    if (!id) return 0;
    tam = lerTam(p + o + ui, n - o - ui, &ut);
    if (tam < 0) return 0;
    o += ui + ut;
    if (id == ID_BLOCKGROUP) {
      if (rpuNoContentor(p + o, tam > n - o ? n - o : tam, base + o, faixa, nalTam)) return 1;
    } else if (id == ID_SIMPLEBLOCK || id == ID_BLOCK) {
      long disp = tam > n - o ? n - o : tam;
      h = cabBloco(p + o, disp, faixa);
      if (h >= 0) {
        if (tam > n - o) {                 // quadro cortado pelo trecho
          diag.blocoIni = base + o + h; diag.blocoFim = base + o + tam;
          return 1;
        }
        { int r = mkv_quadro_tem_rpu(p + o + h, tam - h, nalTam, &diag.rpuTipo, &diag.rpuPerfil);
          if (r >= 0) diag.quadros++;
          if (r == 1) { diag.rpu = 1; return 1; }
          if (r == 0) { diag.rpu = 0; return 1; } }   // DV poe RPU em TODO quadro
      }
    }
    if (tam > n - o) return 0;
    o += tam;
  }
  return 0;
}

static void diagTrecho(const unsigned char *p, long n, const MkvFaixa *fx, int nf) {
  int faixa = 0, nalTam = 0, j;
  long o = 0;
  memset(&diag, 0, sizeof diag);
  diag.rpu = -1; diag.rpuTipo = diag.rpuPerfil = -1; diag.blocoIni = diag.blocoFim = -1;
  diag.lidos = n; diag.tracksAchado = tkAchado; diag.tracksTam = tkTam;
  diag.tracksInteiro = tkAchado && !tracksCortado;
  for (j = 0; j < nf; j++) if (fx[j].tipo == 1) { faixa = fx[j].numero; nalTam = fx[j].nalTam; break; }
  if (!faixa || !nalTam) return;
  while (o < n) {
    int ui = 0, ut = 0;
    unsigned long id = lerId(p + o, n - o, &ui);
    long tam;
    if (!id) return;
    tam = lerTam(p + o + ui, n - o - ui, &ut);
    if (tam == -1) return;
    o += ui + ut;
    if (id == ID_SEGMENT) continue;
    if (id == ID_CLUSTER) {
      long t = (tam < 0 || tam > n - o) ? n - o : tam;
      if (!rpuNoContentor(p + o, t, o, faixa, nalTam) && diag.quadros > 0 && tam >= 0 && tam <= n - o)
        diag.rpu = 0;                     // Cluster inteiro varrido, quadros sem RPU
      return;
    }
    if (tam < 0 || tam > n - o) return;
    o += tam;
  }
}

int mkv_rpu_alem(const char *url, int nalTam, long teto) {
  long n = 0, quer;
  char *b;
  if (!url || !url[0] || diag.rpu != -1 || diag.blocoIni < 0) return diag.rpu;
  quer = (long)(diag.blocoFim - diag.blocoIni);
  if (quer < 1 || quer > teto) return diag.rpu;
  b = rede_baixar_trecho_st(url, 20, diag.blocoIni, diag.blocoFim - 1, &n, NULL, NULL, NULL, 0);
  if (!b) return diag.rpu;
  if (n == quer) {
    int r = mkv_quadro_tem_rpu((const unsigned char *)b, n, nalTam, &diag.rpuTipo, &diag.rpuPerfil);
    if (r >= 0) { diag.quadros++; diag.rpu = r; }
  }
  free(b);
  return diag.rpu;
}

// Ver a nota em mkv.h (MKV_CONT_*). Estado estatico como `diag`: le-se no
// mesmo fio, logo depois da sonda.
static int ultimoContentor;
static int caixaIso(const unsigned char *t) {
  static const char *tipos[] = { "ftyp", "styp", "moov", "mdat", "free", "skip", "wide", "pdin" };
  size_t i;
  for (i = 0; i < sizeof tipos / sizeof tipos[0]; i++) if (!memcmp(t, tipos[i], 4)) return 1;
  return 0;
}
int mkv_contentor(const unsigned char *p, long n) {
  long i = 0;
  if (!p || n < 4) return MKV_CONT_NADA;
  if (p[0] == 0x1A && p[1] == 0x45 && p[2] == 0xDF && p[3] == 0xA3) return MKV_CONT_MKV;
  if (n >= 8 && caixaIso(p + 4)) return MKV_CONT_MP4;
  if (p[0] == 0x47 && (n <= 188 || p[188] == 0x47)) return MKV_CONT_TS;
  if (n >= 5 && p[4] == 0x47 && (n <= 196 || p[196] == 0x47)) return MKV_CONT_TS;
  if (n >= 3 && p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF) i = 3;   // BOM do UTF-8
  while (i < n && i < 64 && (p[i] == ' ' || p[i] == '\t' || p[i] == '\r' || p[i] == '\n')) i++;
  if (i < n && (p[i] == '<' || p[i] == '{' || p[i] == '[')) return MKV_CONT_TEXTO;
  return MKV_CONT_OUTRO;
}
int mkv_ultimo_contentor(void) { return ultimoContentor; }
int mkv_contentor_definitivo(int c) {
  return c == MKV_CONT_MP4 || c == MKV_CONT_TS || c == MKV_CONT_OUTRO;
}
const char *mkv_contentor_nome(int c) {
  switch (c) {
    case MKV_CONT_MKV:   return "mkv";
    case MKV_CONT_MP4:   return "mp4";
    case MKV_CONT_TS:    return "mpeg-ts";
    case MKV_CONT_TEXTO: return "texto";
    case MKV_CONT_OUTRO: return "outro";
    default:             return "nada";
  }
}

int mkv_faixas_e_caps(const char *url, MkvFaixa *saida, int max,
                      MkvCap *caps, int maxCaps, int *nCaps) {
  char *buf;
  long n = 0;
  int achou;
  ultimoContentor = MKV_CONT_NADA;
  if (!url || !url[0] || !saida || max < 1) return 0;
  buf = rede_baixar_trecho(url, 20, 0, MKV_TRECHO - 1, &n);
  if (!buf) return 0;
  ultimoContentor = mkv_contentor((const unsigned char *)buf, n);
  // Assinatura EBML. Sem ela nao e Matroska (pode ser MP4, ou um HTML de erro
  // que o servidor devolveu com 200), e seguir seria interpretar lixo.
  if (n < 64 || (unsigned char)buf[0] != 0x1A || (unsigned char)buf[1] != 0x45 ||
      (unsigned char)buf[2] != 0xDF || (unsigned char)buf[3] != 0xA3) {
    free(buf);
    return 0;
  }
  achou = acharTracks((const unsigned char *)buf, n, saida, max,
                      caps, maxCaps, nCaps);
  diagTrecho((const unsigned char *)buf, n, saida, achou);
  if (caps && nCaps && !*nCaps && posChapters >= 0)
    *nCaps = capsPorPosicao(url, posChapters, caps, maxCaps, NULL);
  free(buf);
  printf("[mkv] %d faixas e %d capitulos lidos do cabecalho (%ld bytes)\n",
         achou, (nCaps && caps) ? *nCaps : 0, n);
  fflush(stdout);
  return achou;
}

int mkv_faixas_do_trecho(const unsigned char *buf, long n, MkvFaixa *saida, int max,
                         MkvCap *caps, int maxCaps, int *nCaps) {
  int achou;
  if (nCaps) *nCaps = 0;
  if (!buf || n < 64 || !saida || max < 1 || buf[0] != 0x1A || buf[1] != 0x45 ||
      buf[2] != 0xDF || buf[3] != 0xA3) return 0;
  achou = acharTracks(buf, n, saida, max, caps, maxCaps, nCaps);
  diagTrecho(buf, n, saida, achou);
  if (tracksCortado) return 0;
  return achou;
}

int mkv_faixas(const char *url, MkvFaixa *saida, int max) {
  return mkv_faixas_e_caps(url, saida, max, NULL, 0, NULL);
}

// Ver a nota em mkv.h. Puro: sem rede, sem estado — e o que o teste
// tests/mkv_legendas.sh exercita com um MKV de varias faixas ASS.
int mkv_casar_legendas(const MkvFaixa *fx, int n, const int *tvNum, int nTv,
                       int *idxFx) {
  int leg[MKV_MAX_FAIXAS], nLeg = 0, i, j, ok;
  const int TIPO_LEG = 17;
  if (!fx || !tvNum || !idxFx || nTv < 1) return MKV_CASA_NADA;
  for (i = 0; i < nTv; i++) idxFx[i] = -1;
  for (j = 0; j < n && nLeg < MKV_MAX_FAIXAS; j++)
    if (fx[j].tipo == TIPO_LEG) leg[nLeg++] = j;
  if (!nLeg) return MKV_CASA_NADA;

  // ORDINAL: a contagem precisa bater. Se a TV escondeu uma faixa (codec que
  // ela nao le), o ordinal dela ja nao aponta para a mesma TrackEntry, e casar
  // assim trocaria idiomas em silencio.
  // Cada ordinal uma vez so: repetido, alguma faixa da TV nao e quem diz ser.
  ok = nTv == nLeg;
  { unsigned char visto[MKV_MAX_FAIXAS] = {0};
    for (i = 0; ok && i < nTv; i++) {
      if (tvNum[i] < 0 || tvNum[i] >= nLeg || visto[tvNum[i]]) ok = 0;
      else visto[tvNum[i]] = 1;
    } }
  if (ok) {
    for (i = 0; i < nTv; i++) idxFx[i] = leg[tvNum[i]];
    return MKV_CASA_ORDINAL;
  }

  ok = 1;
  for (i = 0; ok && i < nTv; i++) {
    int achou = -1;
    for (j = 0; j < nLeg; j++) if (fx[leg[j]].numero == tvNum[i]) { achou = leg[j]; break; }
    if (achou < 0) ok = 0; else idxFx[i] = achou;
  }
  if (ok) return MKV_CASA_NUMERO;
  for (i = 0; i < nTv; i++) idxFx[i] = -1;
  return MKV_CASA_NADA;
}
