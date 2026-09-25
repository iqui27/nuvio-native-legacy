#include "mkv.h"
#include "rede.h"
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

static unsigned long lerUint(const unsigned char *p, long n) {
  unsigned long v = 0;
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
          else if (fid == ID_CODECID) lerTexto(v, ftam, f.codec, sizeof f.codec); }
        q += ftam;
      }
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
      // Uma edicao contem os atomos; descer sem pular.
      achou += lerCapitulos(p + o, tam, saida + achou, max - achou);
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

static int acharTracks(const unsigned char *p, long n, MkvFaixa *saida, int max,
                       MkvCap *caps, int maxCaps, int *nCaps) {
  long o = 0;
  int nFaixas = 0;
  if (nCaps) *nCaps = 0;
  tracksCortado = 0;
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
      if (id == ID_SEGMENT) continue;
      return 0;
    }
    if (id == ID_TRACKS) {
      long disp = n - o;
      long t = tam > disp ? disp : tam;   // cabecalho maior que o trecho baixado
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
      long t = tam > disp ? disp : tam;
      if (nCaps) *nCaps = lerCapitulos(p + o, t, caps, maxCaps);
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
double mkv_creditos_nomeados(const MkvCap *caps, int n) {
  static const char *NOMES[] = { "credit", "crédit", "credito", "crédito",
                                 "end title", "outro", "encerrament" };
  int i, k;
  for (i = n - 1; i >= 0; i--) {
    char m[64];
    size_t j;
    snprintf(m, sizeof m, "%s", caps[i].nome);
    // Minusculas byte a byte. Serve para o ASCII dos rotulos que importam; o
    // acento de "créditos" e comparado como esta, e por isso a lista tem as
    // duas formas.
    for (j = 0; m[j]; j++)
      if (m[j] >= 'A' && m[j] <= 'Z') m[j] = (char)(m[j] - 'A' + 'a');
    for (k = 0; k < (int)(sizeof NOMES / sizeof NOMES[0]); k++)
      if (strstr(m, NOMES[k])) return caps[i].inicio;
  }
  return 0.0;
}


int mkv_faixas_e_caps(const char *url, MkvFaixa *saida, int max,
                      MkvCap *caps, int maxCaps, int *nCaps) {
  char *buf;
  long n = 0;
  int achou;
  if (!url || !url[0] || !saida || max < 1) return 0;
  buf = rede_baixar_trecho(url, 20, 0, MKV_TRECHO - 1, &n);
  if (!buf) return 0;
  // Assinatura EBML. Sem ela nao e Matroska (pode ser MP4, ou um HTML de erro
  // que o servidor devolveu com 200), e seguir seria interpretar lixo.
  if (n < 64 || (unsigned char)buf[0] != 0x1A || (unsigned char)buf[1] != 0x45 ||
      (unsigned char)buf[2] != 0xDF || (unsigned char)buf[3] != 0xA3) {
    free(buf);
    return 0;
  }
  achou = acharTracks((const unsigned char *)buf, n, saida, max,
                      caps, maxCaps, nCaps);
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
