// Ver faixasmkv.h (#206).
#include "faixasmkv.h"
#include "idioma.h"
#include "linguas.h"
#include "audioinfo.h"
#include <stdio.h>
#include <string.h>

#define SEP "  \xc2\xb7  "
#define MKV_AUDIO 2
#define MKV_LEG   17

const char *faixasmkv_canais(int c) {
  return c == 8 ? "7.1" : c == 6 ? "5.1" : c == 2 ? "2.0" : "";
}

// Idioma da faixa: o do ARQUIVO quando ele etiqueta, senao o do player; e o
// NOME ganha dos dois quando cita outro idioma com todas as letras (mesma
// regra do video.c da LG: "Português" etiquetado eng e comum em release
// remontado).
//
// O ARQUIVO PRIMEIRO desde o #269. Antes era o player, e o player do .tpk da
// Samsung troca o codigo: "jp" para ja, "cz" para cs, "du" para nl, e "fr"
// para as legendas fil e fi do mesmo MKV (medido no log p2p-20). O cabecalho
// diz "pt-BR"/"es-419" onde o player diz "pt"/"es". Com as faixas casadas,
// o MKV e a fonte da verdade.
static void idiomaFinal(const VideoFaixa *f, const MkvFaixa *m, char *id, size_t t) {
  const char *peloNome = ling_do_nome(m->nome);
  snprintf(id, t, "%s", (m->idioma[0] && strcmp(m->idioma, "und")) ? m->idioma : f->idioma);
  if (peloNome && (!id[0] || !ling_casa(peloNome, id))) snprintf(id, t, "%s", peloNome);
}

// O nome repete o idioma ("English", uma palavra so)? Entao nao acrescenta.
static int nomeUtil(const MkvFaixa *m) {
  return m->nome[0] && !(ling_do_nome(m->nome) && !strchr(m->nome, ' '));
}

const char *faixasmkv_codec(const char *c) {
  static const struct { const char *pre, *nome; } T[] = {
    { "A_EAC3", "E-AC3" }, { "A_AC3", "AC3" }, { "A_TRUEHD", "TrueHD" },
    { "A_DTS/LOSSLESS", "DTS-HD MA" }, { "A_DTS/EXPRESS", "DTS-HD" }, { "A_DTS", "DTS" },
    { "A_AAC", "AAC" }, { "A_OPUS", "Opus" }, { "A_FLAC", "FLAC" }, { "A_VORBIS", "Vorbis" },
    { "A_MPEG/L3", "MP3" }, { "A_MPEG/L2", "MP2" }, { "A_PCM", "PCM" }, { "A_MLP", "MLP" },
  };
  size_t i;
  if (!c || !*c) return "";
  for (i = 0; i < sizeof T / sizeof *T; i++)
    if (!strncmp(c, T[i].pre, strlen(T[i].pre))) return T[i].nome;
  return "";
}

// Legenda de IMAGEM (PGS do Blu-ray, VobSub do DVD, DVB): o player do .tpk nao
// a lista. MEDIDO (p2p-20): MKV com 1 legenda S_HDMV/PGS, player com 0.
static int legendaImagem(const MkvFaixa *m) {
  return !strncmp(m->codec, "S_HDMV/", 7) || !strcmp(m->codec, "S_VOBSUB") ||
         !strcmp(m->codec, "S_DVBSUB");
}

// Faixa `m` do arquivo serve para a faixa `f` da TV? Mesmo criterio do
// nv_dvaudio_localizar (video_dvaudio.h) e do prepare() do motor DTS: idioma,
// familia de codec (audioinfo_codec le "A_EAC3" e o "eac3" da TV) e canais; campo
// vazio (ou "und", ou 0) de um dos lados nao contradiz.
static int audioCompativel(const VideoFaixa *f, const MkvFaixa *m) {
  const char *ft = audioinfo_codec(f->codec), *fm = audioinfo_codec(m->codec);
  return (!f->idioma[0] || !m->idioma[0] || !strcmp(m->idioma, "und") || ling_casa(m->idioma, f->idioma)) &&
         (!ft[0] || !fm[0] || !strcmp(ft, fm)) &&
         (f->canais <= 0 || m->canais <= 0 || f->canais == m->canais);
}

// A TV lista MENOS audios que o arquivo (esconde o que nao decodifica: DTS,
// TrueHD). Casa cada faixa da TV com UMA do arquivo por idioma + familia de
// codec + canais. Quando varias faixas da TV tem exatamente os mesmos
// candidatos, a ordem desempata, mas so se candidatos e faixas da TV forem em
// numero igual (ninguem escondido naquele grupo). Sem par seguro: -1, a faixa
// fica com o rotulo da TV. Devolve quantas casaram.
static int casarAudioPorAtributo(int nF, const VideoFaixa *f, const MkvFaixa *fx, const int *cand, int nc,
                                 int *mapa) {
  int k, k2, j, achou = 0, perdeu[64] = {0};
  for (k = 0; k < nF; k++) {
    int m = 0, s = 0, rank = 0;
    for (j = 0; j < nc; j++) m += audioCompativel(&f[k], &fx[cand[j]]);
    if (!m) continue;
    for (k2 = 0; k2 < nF; k2++) {
      int igual = 1;
      for (j = 0; j < nc && igual; j++)
        igual = audioCompativel(&f[k], &fx[cand[j]]) == audioCompativel(&f[k2], &fx[cand[j]]);
      if (igual) { s++; if (k2 < k) rank++; }
    }
    if (m != s) continue;
    for (j = 0; j < nc; j++)
      if (audioCompativel(&f[k], &fx[cand[j]]) && rank-- == 0) { mapa[k] = cand[j]; break; }
  }
  // Duas faixas da TV nao ficam com a mesma do arquivo.
  for (k = 0; k < nF; k++) if (mapa[k] >= 0) for (k2 = k + 1; k2 < nF; k2++)
    if (mapa[k2] == mapa[k]) perdeu[k] = perdeu[k2] = 1;
  for (k = 0; k < nF; k++) { if (perdeu[k]) mapa[k] = -1; if (mapa[k] >= 0) achou++; }
  return achou;
}

// #421: listas truncadas so casam pelo idioma quando o par e unico nos
// DOIS lados. Duas English/SDH, idioma vazio ou alias ambiguo ficam na TV.
int faixasmkv_legendas_por_idioma(const VideoFaixa *f, int nF,
                                 const MkvFaixa *fx, int n, int *mapa) {
  int k, j, outros, achou = 0;
  for (k = 0; k < nF; k++) {
    int candidato = -1, quantos = 0;
    mapa[k] = -1;
    if (!f[k].idioma[0] || !strcmp(f[k].idioma, "und")) continue;
    for (j = 0; j < n; j++)
      if (fx[j].tipo == MKV_LEG && !legendaImagem(&fx[j]) &&
          fx[j].idioma[0] && strcmp(fx[j].idioma, "und") && ling_casa(f[k].idioma, fx[j].idioma)) {
        candidato = j; quantos++;
      }
    if (quantos != 1) continue;
    outros = 0;
    for (j = 0; j < nF; j++) outros += ling_casa(f[j].idioma, fx[candidato].idioma);
    if (outros == 1) { mapa[k] = candidato; achou++; }
  }
  return achou;
}

int faixasmkv_idiomas_coerentes(const VideoFaixa *f, int nF, const MkvFaixa *fx, const int *idx) {
  int k;
  for (k = 0; k < nF; k++) {
    const MkvFaixa *m;
    if (idx[k] < 0) continue;
    m = &fx[idx[k]];
    if (!f[k].idioma[0] || !strcmp(f[k].idioma, "und") || !m->idioma[0] || !strcmp(m->idioma, "und")) continue;
    if (!ling_casa(f[k].idioma, m->idioma)) return 0;
  }
  return 1;
}

// Linha que o player entregou. numero < 0 e PGS pendurada por
// acrescentar_imagem; a segunda sonda nao pode conta-la como faixa da TV.
static int linhaDoPlayer(const VideoFaixa *f) { return !f || f->numero >= 0; }

// CASAMENTO DE UM TIPO. 1) Contagem igual: ordinal, como sempre. 2) Contagem
// diferente na legenda: tira as de imagem, que o player do .tpk nao lista, e
// tenta de novo. 3) Audio com o player listando MENOS (ele esconde DTS/TrueHD):
// por idioma + codec + canais, so o que for inequivoco. Fora disso fica como veio: rotulo errado e pior que
// "Audio 1". (Casar "pelo comeco" quando o player lista MENOS — ele para nas
// 27 primeiras de um MKV com 41, medido — foi tentado e ficou de fora: uma
// faixa escondida no meio desalinha tudo dali em diante sem que o idioma
// denuncie, e "Letreiros" na faixa de dialogo e pior que o codigo do player.)
// `mapa[k]` recebe o indice em `fx` da faixa k do player (-1 = nenhum).
static int casarTipo(int nF, const VideoFaixa *f, const MkvFaixa *fx, int n, int tipo, int *mapa) {
  int cand[64], nc = 0, j, k, pulouImagem = 0, nJog = 0;
  for (k = 0; k < nF; k++) mapa[k] = -1;
  if (nF < 1) return 0;
  for (k = 0; k < nF; k++) nJog += linhaDoPlayer(&f[k]);
  for (j = 0; j < n && nc < 64; j++) if (fx[j].tipo == tipo) cand[nc++] = j;
  if (nc != nJog && tipo == MKV_LEG) {
    int m = 0;
    for (j = 0; j < nc; j++) if (!legendaImagem(&fx[cand[j]])) cand[m++] = cand[j];
    pulouImagem = m != nc;
    nc = m;
  }
  // Audio com a TV listando MENOS que o arquivo (esconde DTS/TrueHD): casa por
  // atributo; as sem par seguro ficam com o rotulo da TV.
  if (nc > nJog && tipo == MKV_AUDIO) return casarAudioPorAtributo(nF, f, fx, cand, nc, mapa) ? 3 : 0;
  if (nc != nJog && tipo == MKV_LEG) {
    VideoFaixa jog[64];
    int mapJ[64], p = 0, r;
    for (k = 0; k < nF && p < 64; k++) if (linhaDoPlayer(&f[k])) jog[p++] = f[k];
    r = faixasmkv_legendas_por_idioma(jog, p, fx, n, mapJ);
    for (k = 0, p = 0; k < nF; k++) if (linhaDoPlayer(&f[k])) mapa[k] = mapJ[p++];
    return r ? 3 : 0;
  }
  if (nc != nJog) return 0;
  for (k = 0, j = 0; k < nF; k++) if (linhaDoPlayer(&f[k])) mapa[k] = cand[j++];
  return pulouImagem ? 2 : 1;
}

int faixasmkv_overlay(const char *c, int textoSimples) {
  if (!c || !*c) return 0;
  if (!strncmp(c, "S_TEXT/ASS", 10) || !strncmp(c, "S_TEXT/SSA", 10)) return 1;
  return textoSimples && (!strcmp(c, "S_TEXT/UTF8") || !strcmp(c, "S_TEXT/WEBVTT"));
}

// Posicao da TrackEntry `j` entre as legendas do arquivo (o ordinal do mkvass).
static int ordinalLegenda(const MkvFaixa *fx, int j) {
  int k, o = 0;
  for (k = 0; k < j; k++) if (fx[k].tipo == MKV_LEG) o++;
  return o;
}

static int aplicarUma(VideoFaixa *f, const MkvFaixa *m, int ordinal) {
  char id[8], rot[sizeof f->rotulo], base[sizeof f->rotulo];
  idiomaFinal(f, m, id, sizeof id);
  // Sem idioma nenhum, a base e a de sempre ("Audio 1"), refeita e nao lida
  // do rotulo: aplicar duas vezes nao pode dar "Audio 1 · 5.1 · 5.1".
  if (id[0]) snprintf(base, sizeof base, "%s", i18n(ling_nome(id)));
  else if (m->tipo == MKV_AUDIO) snprintf(base, sizeof base, "%s %d", i18n("Áudio"), ordinal);
  else snprintf(base, sizeof base, "%s %d", i18n("Legenda"), ordinal);
  if (m->tipo == MKV_AUDIO) {
    // "Inglês · E-AC3 5.1" (#269: "nem o codec nem o 5.1/7.1"). O nome da
    // faixa, quando util, ja costuma trazer os dois ("DD 5.1", "Japanese 5.1
    // Opus") e vai no lugar do codec; os canais so entram se ele nao disser.
    const char *ch = faixasmkv_canais(m->canais);
    const char *cod = faixasmkv_codec(m->codec);
    int temNome = nomeUtil(m);
    const char *meio = temNome ? m->nome : cod;
    // #293: o player mostra o codec/canais da faixa tocando; a TrackEntry e a
    // unica fonte no .tpk (o Player nao entrega isso).
    snprintf(f->codec, sizeof f->codec, "%s", m->codec);
    f->canais = m->canais;
    int poeCh = ch[0] && !(meio[0] && strstr(meio, ch));
    snprintf(rot, sizeof rot, "%s%s%s%s%s", base,
             (meio[0] || poeCh) ? SEP : "", meio,
             (meio[0] && poeCh) ? " " : "", poeCh ? ch : "");
  } else {
    if (m->nome[0]) snprintf(f->nome, sizeof f->nome, "%s", m->nome);
    // LETREIROS/FORCADA (FlagForced, "Forced", "Signs & Songs"): a pessoa
    // precisa saber que essa nao traduz o dialogo. O nome do arquivo, quando
    // ele ja diz isso ("Forced"), e o que o .wgt mostra; so a flag vira
    // "Forçada" (#287: a flag e de forcada, nao de placas).
    int letreiro = ling_letreiro(m->nome, m->forcado);
    f->letreiro = letreiro;
    f->tipoLeg = ling_tipo_legenda(m->nome, m->forcado, m->sdh);
    if (letreiro && !(m->nome[0] && ling_letreiro(m->nome, 0)))
      snprintf(rot, sizeof rot, "%s%s%s", base, SEP, i18n(ling_tipo_legenda_rotulo(f->tipoLeg)));
    else if (nomeUtil(m))
      snprintf(rot, sizeof rot, "%s%s%s", base, SEP, m->nome);
    else
      snprintf(rot, sizeof rot, "%s", base);
  }
  // Rotulo por ULTIMO, de uma vez, depois do idioma: o desenho le sem trava.
  snprintf(f->idioma, sizeof f->idioma, "%s", id);
  if (strcmp(rot, f->rotulo)) { snprintf(f->rotulo, sizeof f->rotulo, "%s", rot); return 1; }
  return 0;
}

int faixasmkv_aplicar(VideoFaixa *aud, int nAud, VideoFaixa *leg, int nLeg,
                      const MkvFaixa *fx, int n) {
  int mapa[64], k, mudou = 0, comoA, comoL;
  if (!fx || n < 1) return 0;
  if (nAud > 64) nAud = 64;
  if (nLeg > 64) nLeg = 64;
  comoA = casarTipo(nAud, aud, fx, n, MKV_AUDIO, mapa);
  for (k = 0; k < nAud; k++) if (mapa[k] >= 0) mudou += aplicarUma(&aud[k], &fx[mapa[k]], k + 1);
  comoL = casarTipo(nLeg, leg, fx, n, MKV_LEG, mapa);
  for (k = 0; k < nLeg; k++) {
    if (mapa[k] < 0) continue;
    mudou += aplicarUma(&leg[k], &fx[mapa[k]], k + 1);
    snprintf(leg[k].codec, sizeof leg[k].codec, "%s", fx[mapa[k]].codec);
    leg[k].ordinalMkv = ordinalLegenda(fx, mapa[k]);
  }
  if ((nAud && comoA != 1) || (nLeg && comoL != 1)) {
    int nA = 0, nL = 0, nJog = 0, j;
    for (j = 0; j < n; j++) { if (fx[j].tipo == MKV_AUDIO) nA++; else if (fx[j].tipo == MKV_LEG) nL++; }
    for (j = 0; leg && j < nLeg; j++) nJog += linhaDoPlayer(&leg[j]);
    printf("[mkv] contagem: arquivo %d audio / %d legenda, player %d / %d; casamento audio=%s legenda=%s\n",
           nA, nL, nAud, nJog,
           !nAud ? "-" : comoA == 1 ? "ordinal" : comoA == 3 ? "by language/codec/channels" : "none, labels kept",
           !nLeg ? "-" : comoL == 1 ? "ordinal" : comoL == 2 ? "without image subs" : comoL == 3 ? "unique language" : "none, labels kept");
    fflush(stdout);
  }
  return mudou;
}

// PGS: o player do .tpk nao lista, o app decodifica (imgsub). So esta.
int faixasmkv_imagem_overlay(const char *c) { return c && !strcmp(c, "S_HDMV/PGS"); }

int faixasmkv_tem_imagem(const MkvFaixa *fx, int n) {
  int j;
  for (j = 0; fx && j < n; j++) if (fx[j].tipo == MKV_LEG && faixasmkv_imagem_overlay(fx[j].codec)) return 1;
  return 0;
}

static int acrescentarOverlay(VideoFaixa *leg, int nLeg, int max, const MkvFaixa *fx, int n, int texto) {
  int j, k, ord = 0;
  for (j = 0; fx && j < n && nLeg < max; j++) {
    if (fx[j].tipo != MKV_LEG) continue;
    ord++;
    if (!(texto ? faixasmkv_overlay(fx[j].codec, 1) : faixasmkv_imagem_overlay(fx[j].codec))) continue;
    for (k = 0; k < nLeg; k++) if (leg[k].ordinalMkv == ord - 1 && !strcmp(leg[k].codec, fx[j].codec)) break;
    if (k < nLeg) continue;
    memset(&leg[nLeg], 0, sizeof leg[nLeg]);
    // Unique negative identities keep recovered text tracks distinct in the selector.
    leg[nLeg].numero = texto ? -2 - (ord - 1) : -1;
    leg[nLeg].stream_index = -1;
    snprintf(leg[nLeg].codec, sizeof leg[nLeg].codec, "%s", fx[j].codec);
    leg[nLeg].ordinalMkv = ord - 1;
    aplicarUma(&leg[nLeg], &fx[j], nLeg + 1);
    nLeg++;
  }
  return nLeg;
}

int faixasmkv_acrescentar_imagem(VideoFaixa *leg, int nLeg, int max, const MkvFaixa *fx, int n) {
  return acrescentarOverlay(leg, nLeg, max, fx, n, 0);
}

int faixasmkv_acrescentar_texto(VideoFaixa *leg, int nLeg, int max, const MkvFaixa *fx, int n) {
  return acrescentarOverlay(leg, nLeg, max, fx, n, 1);
}
