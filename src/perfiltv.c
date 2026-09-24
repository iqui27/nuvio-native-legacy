// Tabela de padroes por aparelho e regra de comparacao do otimizador. Ver
// perfiltv.h para o porque de morar tudo aqui.
//
// A TABELA (RAM = MemTotal na LG, navigator.deviceMemory no Tizen):
//
//   aparelho            textura  teto   fios rede  heroi
//   LG sem MemTotal       96     160       4       1920
//   LG  < 800 MB          48      64       2       1280   webOS 3 de 2016
//   LG  < 1,2 GB          48      64       2       1280   webOS 3/4 de 1 GB
//   LG  < 2 GB            96     160       4       1920   webOS 4/5 menores
//   LG  < 3 GB (C9)      128     300       4       1920   medido na C9
//   LG >= 3 GB           192     512       4       1920   C1/C2/C3
//   Samsung sem devMem    96      96       2       1280
//   Samsung <= 1 GB       64      64       2       1280
//   Samsung 2 GB          96      96       2       1280*
//   Samsung >= 4 GB      128     128       2       1280*
//   (* 1920 so com Qualidade da imagem = Alta, regra de tetoDoHeroi)
//
// DE ONDE SAIU CADA COLUNA:
// - textura/teto: a escada que ja existia em tex_cache.c (orcamentoMB e
//   tetoPermitidoMB), movida para ca SEM mudar numero. O relatorio da C9 de
//   22/09 (2245 MB, build alto-cache de 300 MB) confirma o degrau de 128: a
//   sessao inteira de diagnostico usou 35 MB e 44 texturas, zero pendentes e
//   zero quentes despejadas — 128 cobre isso com 3,6x de folga, e 300 fica
//   como TETO que o modo Qualidade pode pedir, nao como padrao.
// - LG < 1,2 GB desceu de 64/96 para 48/64 (23/09/2026, registros 1720-1774,
//   MemTotal=964 MB, 36 sessoes). O rss acompanha o orcamento cheio: sessoes
//   com texturas em ~25 MB ficaram em rss ~125 MB, e as que encheram os 64 MB
//   (63,9) ficaram em rss 188-198 MB. Nove sessoes dessa pessoa morreram sem
//   se despedir com ultimo=vivo e rss 112-195 MB. QUE A TV MATOU POR MEMORIA E
//   HIPOTESE (nao ha log do sistema); o que e medido e o rss subir com as
//   texturas. A tela mais cheia do registro usou 26 MB (tela=27/26.2MB), entao
//   48 ainda cobre a tela com folga, e o preco e redecodificar mais ao voltar.
// - fios de rede: o build cria 4 na LG e 2 no Tizen (NV_TEX_FIOS_REDE). Na LG
//   de 1 GB ficam 2 ativos: cada fio segura um corpo baixado ate o decode
//   (ate 12 MB de fundo), e 4 corpos em voo numa TV com ~300 MB livres e o
//   caminho do OOM que no webOS fecha o app sem aviso. No Tizen os 2 seguem:
//   cada fetch passa pelo fio principal (nota de ADD_FIOS em addons.c), e
//   ninguem mediu 1 melhor que 2 na Samsung.
// - heroi: 1920 e a largura do painel (8,3 MB por arte). 1280 custa 3,7 MB e
//   e o teto de fabrica do Tizen (heap fixo de 256 MiB; registro 1450: fundos
//   grandes levaram o heap a 5,7 MiB livres e a Aborted(OOM)). Na LG abaixo de
//   1,2 GB o heroi desce ao mesmo 1280 pelo mesmo motivo de RAM.
#include "perfiltv.h"
#include "layout.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PtvPlataforma ptv_plataforma(void) {
#ifdef __EMSCRIPTEN__
  return PTV_TIZEN;
#else
  return PTV_LG;
#endif
}

const char *ptv_nome(void) {
#if defined(NV_VIDAA)
  return "vidaa";
#elif defined(__EMSCRIPTEN__)
  return "tizen";
#else
  return "lg";
#endif
}

int ptv_tex_auto_mb(PtvPlataforma p, long mem) {
  if (p == PTV_TIZEN) {
    if (!mem) return NV_TEX_ORCAMENTO_MB;
    if (mem <= 1024) return 64;
    if (mem < 4096) return 96;
    return 128;
  }
  if (!mem) return NV_TEX_ORCAMENTO_MB;
  if (mem < 1200) return 48;
  if (mem < 2000) return 96;
  if (mem < 3000) return 128;
  return 192;
}

int ptv_tex_teto_mb(PtvPlataforma p, long mem) {
  if (p == PTV_TIZEN) return ptv_tex_auto_mb(p, mem);
  if (!mem) return 160;
  if (mem < 1200) return 64;
  if (mem < 2000) return 160;
  if (mem < 3000) return 300;
  return 512;
}

int ptv_fios_rede_max(PtvPlataforma p) { return p == PTV_TIZEN ? 2 : 4; }

// Teto de heroi que a RAM aceita. No Tizen 1920 so passa com 2 GB ou mais
// (a mesma linha de tetoDoHeroi); abaixo disso, 1280 sempre.
int ptv_heroi_max(PtvPlataforma p, long mem) {
  if (p == PTV_TIZEN) return mem >= 2000 ? 1920 : 1280;
  if (mem && mem < 1200) return 1280;
  return 1920;
}

void ptv_padrao(PtvPlataforma p, long mem, PtvPerfil *out) {
  if (!out) return;
  out->texMb = ptv_tex_auto_mb(p, mem);
  out->fiosRede = (p == PTV_LG && mem && mem < 1200) ? 2 : ptv_fios_rede_max(p);
  // Padrao do Tizen e 1280 mesmo com 2 GB: 1920 la e escolha de quem pos a
  // qualidade em Alta, nunca o padrao (ver a tabela no topo).
  out->heroiLarg = p == PTV_TIZEN ? 1280 : ptv_heroi_max(p, mem);
}

// QUALIDADE = o teto que a RAM permite em tudo. DESEMPENHO = menos pressao:
// o orcamento automatico, metade dos fios (nunca menos que 2 na LG; o Tizen
// ja esta no minimo medido) e heroi em 1280.
void ptv_candidato(PtvPlataforma p, long mem, PtvModo modo, int travado,
                   PtvPerfil *out) {
  if (!out) return;
  if (modo == PTV_DESEMPENHO) {
    ptv_padrao(p, mem, out);
    if (p == PTV_LG && out->fiosRede > 2) out->fiosRede = 2;
    out->heroiLarg = 1280;
  } else {
    out->texMb = ptv_tex_teto_mb(p, mem);
    out->fiosRede = ptv_fios_rede_max(p);
    out->heroiLarg = ptv_heroi_max(p, mem);
    // Tizen: o candidato de Qualidade nao sobe o heroi por conta propria. O
    // 1920 continua dependendo da escolha Alta (tetoDoHeroi), e o teto aqui so
    // deixa de ser o gargalo quando ela existe.
  }
  if (travado > 0) out->texMb = travado;
  ptv_limitar(p, mem, out);
}

int ptv_limitar(PtvPlataforma p, long mem, PtvPerfil *pf) {
  int mudou = 0, teto;
  if (!pf) return 0;
  teto = ptv_tex_teto_mb(p, mem);
  if (pf->texMb < 16) { pf->texMb = ptv_tex_auto_mb(p, mem); mudou = 1; }
  if (pf->texMb > teto) { pf->texMb = teto; mudou = 1; }
  if (pf->fiosRede < 1) { pf->fiosRede = 1; mudou = 1; }
  if (pf->fiosRede > ptv_fios_rede_max(p)) { pf->fiosRede = ptv_fios_rede_max(p); mudou = 1; }
  if (pf->heroiLarg < 1280) { pf->heroiLarg = 1280; mudou = 1; }
  if (pf->heroiLarg > ptv_heroi_max(p, mem)) { pf->heroiLarg = ptv_heroi_max(p, mem); mudou = 1; }
  return mudou;
}

// A REGRA DE RESTAURAR. Os dois lados sao a MESMA amostra com o cache em disco
// quente (o passe frio vem antes e nao entra na conta), entao a diferenca que
// sobra e o que o perfil mudou: fila de decode, fios e teto do heroi.
//
// As folgas existem porque a rede de uma TV oscila e um reteste nao e um
// laboratorio: 25% + 150 ms no tempo de artes (a amostra da C9 levou ~1,9 s;
// 150 ms e menos de um cartaz), 50% + 20 ms no pior quadro e so a partir de
// 50 ms (tres quadros a 60 Hz: abaixo disso nao ha tranco que alguem veja).
// Falha a mais e arte da tela despejada nao tem folga: qualquer uma reprova.
int ptv_depois_pior(const PtvMedida *a, const PtvMedida *b, const char **motivo) {
  const char *m = NULL;
  if (!a || !b) return 0;
  if (b->falhas > a->falhas) m = "Mais artes falharam depois da mudança";
  else if (b->despejosQuentes > a->despejosQuentes) m = "Arte visível foi descartada depois da mudança";
  else if (b->artesMs > a->artesMs + a->artesMs / 4 + 150) m = "Artes ficaram mais lentas depois da mudança";
  else if (b->piorQuadroMs >= 50 && b->piorQuadroMs > a->piorQuadroMs + a->piorQuadroMs / 2 + 20)
    m = "O pior quadro piorou depois da mudança";
  if (motivo) *motivo = m;
  return m != NULL;
}

int ptv_serializar(const PtvPerfil *pf, const char *modo, char *dst, size_t cap) {
  int n;
  if (!pf || !dst || !cap) return 0;
  n = snprintf(dst, cap, "versao=2\nmodo=%s\ntex_mb=%d\nfios_rede=%d\nheroi=%d\n",
               modo ? modo : "", pf->texMb, pf->fiosRede, pf->heroiLarg);
  return n > 0 && (size_t)n < cap;
}

static int campo(const char *txt, const char *chave, int *v) {
  const char *p = txt;
  size_t n = strlen(chave);
  while (p && *p) {
    if (!strncmp(p, chave, n) && p[n] == '=') { *v = atoi(p + n + 1); return 1; }
    p = strchr(p, '\n');
    if (p) p++;
  }
  return 0;
}

int ptv_ler(const char *txt, PtvPerfil *pf) {
  int versao = 0;
  if (!txt || !pf) return 0;
  // A versao 1 so guardava o modo e o orcamento de ANTES: nao diz o que foi
  // aprovado, entao nao vale como perfil.
  if (!campo(txt, "versao", &versao) || versao != 2) return 0;
  return campo(txt, "tex_mb", &pf->texMb) && campo(txt, "fios_rede", &pf->fiosRede) &&
         campo(txt, "heroi", &pf->heroiLarg);
}

const char *ptv_fonte_nome(int f) {
  switch (f) {
    case PTV_FONTE_CATALOGO: return "catalog";
    case PTV_FONTE_METAHUB: return "metahub";
    case PTV_FONTE_TMDB: return "tmdb";
    case PTV_FONTE_TRAKT: return "trakt";
    case PTV_FONTE_APPLE: return "apple";
    case PTV_FONTE_FANART: return "fanart";
    case PTV_FONTE_ANIME: return "anime";
    case PTV_FONTE_TMDB_OUTRO: return "tmdb_outro";
    case PTV_FONTE_LOGO: return "logo";
    default: return "?";
  }
}

const char *ptv_fonte_rotulo(int f) {
  switch (f) {
    case PTV_FONTE_CATALOGO: return "Catálogo";
    case PTV_FONTE_METAHUB: return "Metahub";
    case PTV_FONTE_TMDB: return "TMDB";
    case PTV_FONTE_TRAKT: return "Trakt";
    case PTV_FONTE_APPLE: return "Apple TV";
    case PTV_FONTE_FANART: return "fanart.tv";
    case PTV_FONTE_ANIME: return "Anime";
    case PTV_FONTE_TMDB_OUTRO: return "TMDB outro fundo";
    case PTV_FONTE_LOGO: return "Logo";
    default: return "?";
  }
}

int ptv_fonte_ms(const PtvFonte *f) {
  if (!f || f->ok <= 0) return -1;
  return (f->resolveMs + f->downloadMs) / f->ok;
}

int ptv_fonte_da_url(const char *u) {
  if (!u) return 0;
  if (strstr(u, "images.metahub.space")) return PTV_FONTE_METAHUB;
  if (strstr(u, "nuvio.invalid/arte/tmdbalt/")) return PTV_FONTE_TMDB_OUTRO;
  if (strstr(u, "image.tmdb.org") || strstr(u, "nuvio.invalid/arte/tmdb/")) return PTV_FONTE_TMDB;
  if (strstr(u, "trakt.tv") || strstr(u, "nuvio.invalid/arte/trakt/")) return PTV_FONTE_TRAKT;
  if (strstr(u, "mzstatic.com") || strstr(u, "nuvio.invalid/arte/apple/")) return PTV_FONTE_APPLE;
  if (strstr(u, "fanart.tv") || strstr(u, "nuvio.invalid/arte/fanart/")) return PTV_FONTE_FANART;
  if (strstr(u, "kitsu.") || strstr(u, "anilist.co") || strstr(u, "nuvio.invalid/arte/anime/"))
    return PTV_FONTE_ANIME;
  return PTV_FONTE_CATALOGO;
}

int ptv_ajuste_da_fonte(int f, int diferente) {
  if (f < 1 || f > PTV_FONTE_FUNDO_MAX) return -1;
  // Com outra arte, "TMDB" ja e o outro backdrop; o padrao do TMDB nao tem
  // valor de ajuste que o alcance. Sem outra arte e o contrario.
  if (f == PTV_FONTE_TMDB_OUTRO) return diferente ? PTV_FONTE_TMDB : -1;
  if (f == PTV_FONTE_TMDB && diferente) return -1;
  return f;
}

static int lenta(const PtvFonte *f, int h, int b) {
  int mh = ptv_fonte_ms(&f[h]), mb = ptv_fonte_ms(&f[b]);
  if (mb < 0) return 0;                  // base sem medida: nada a comparar
  if (mh < 0) return f[h].falhas > 0;    // a do destaque so falhou
  return mh > 2 * mb && mh > 800;
}

int ptv_sugerir_destaque(const PtvFonte f[PTV_N_FONTES], int hero, int card,
                         int ajuste, int diferente, PtvSugestao *o) {
  int k, melhor = -1;
  if (!o) return 0;
  memset(o, 0, sizeof *o);
  if (!f || hero < 1 || hero > PTV_FONTE_FUNDO_MAX || card < 1 || card > PTV_FONTE_FUNDO_MAX) return 0;
  if (diferente) {
    int igual = hero == card || f[hero].iguais > 0;
    int devagar = !igual && lenta(f, hero, card);
    if (!igual && !devagar) return 0;
    for (k = 1; k <= PTV_FONTE_FUNDO_MAX; k++) {
      int aj = ptv_ajuste_da_fonte(k, 1);
      if (k == card || k == hero || aj < 0 || aj == ajuste) continue;
      if (ptv_fonte_ms(&f[k]) < 0 || f[k].iguais > 0 || lenta(f, k, card)) continue;
      if (melhor < 0 || ptv_fonte_ms(&f[k]) < ptv_fonte_ms(&f[melhor])) melhor = k;
    }
    o->base = card;
    o->motivo = igual ? PTV_MOTIVO_IGUAL : PTV_MOTIVO_LENTA;
    if (melhor > 0) { o->fonte = ptv_ajuste_da_fonte(melhor, 1); o->diferente = 1; o->alvo = melhor; }
    else if (devagar) { o->fonte = ajuste; o->diferente = 0; o->alvo = card; }
    else return 0;       // igual ao card e sem outra de verdade: nada a propor
  } else {
    if (ajuste <= 0) return 0;
    for (k = 1; k <= PTV_FONTE_FUNDO_MAX; k++) {
      int aj = ptv_ajuste_da_fonte(k, 0);
      if (aj < 0 || aj == ajuste || ptv_fonte_ms(&f[k]) < 0) continue;
      if (melhor < 0 || ptv_fonte_ms(&f[k]) < ptv_fonte_ms(&f[melhor])) melhor = k;
    }
    if (melhor < 0 || melhor == hero || !lenta(f, hero, melhor)) return 0;
    o->base = melhor; o->fonte = melhor; o->diferente = 0; o->alvo = melhor;
    o->motivo = PTV_MOTIVO_LENTA;
  }
  o->ativa = 1;
  o->lenta = hero;
  o->msLenta = ptv_fonte_ms(&f[hero]);
  o->msBase = ptv_fonte_ms(&f[o->base]);
  return 1;
}

static unsigned be16(const unsigned char *p) { return (unsigned)p[0] << 8 | p[1]; }
static unsigned le16(const unsigned char *p) { return (unsigned)p[1] << 8 | p[0]; }
static unsigned le24(const unsigned char *p) { return (unsigned)p[2] << 16 | (unsigned)p[1] << 8 | p[0]; }

int ptv_dimensoes(const unsigned char *b, long n, int *w, int *h) {
  if (!b || !w || !h) return 0;
  *w = *h = 0;
  if (n >= 24 && !memcmp(b, "\x89PNG\r\n\x1a\n", 8)) {
    *w = (int)(be16(b + 16) << 16 | be16(b + 18));
    *h = (int)(be16(b + 20) << 16 | be16(b + 22));
    return 1;
  }
  if (n >= 10 && (!memcmp(b, "GIF87a", 6) || !memcmp(b, "GIF89a", 6))) {
    *w = (int)le16(b + 6); *h = (int)le16(b + 8);
    return 1;
  }
  if (n >= 30 && !memcmp(b, "RIFF", 4) && !memcmp(b + 8, "WEBP", 4)) {
    if (!memcmp(b + 12, "VP8 ", 4)) {
      *w = (int)(le16(b + 26) & 0x3fff); *h = (int)(le16(b + 28) & 0x3fff); return 1;
    }
    if (!memcmp(b + 12, "VP8L", 4) && n >= 25) {
      unsigned long v = (unsigned long)b[21] | (unsigned long)b[22] << 8 |
                        (unsigned long)b[23] << 16 | (unsigned long)b[24] << 24;
      *w = (int)(v & 0x3fff) + 1; *h = (int)((v >> 14) & 0x3fff) + 1; return 1;
    }
    if (!memcmp(b + 12, "VP8X", 4)) {
      *w = (int)le24(b + 24) + 1; *h = (int)le24(b + 27) + 1; return 1;
    }
    return 0;
  }
  if (n >= 4 && b[0] == 0xFF && b[1] == 0xD8) {
    long i = 2;
    while (i + 9 < n) {
      unsigned m, len;
      if (b[i] != 0xFF) { i++; continue; }
      m = b[i + 1];
      if (m == 0xFF) { i++; continue; }
      if (m == 0xD8 || m == 0x01 || (m >= 0xD0 && m <= 0xD7)) { i += 2; continue; }
      len = be16(b + i + 2);
      if (m >= 0xC0 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC) {
        *h = (int)be16(b + i + 5); *w = (int)be16(b + i + 7);
        return 1;
      }
      if (len < 2) return 0;
      i += 2 + len;
    }
  }
  return 0;
}
