// FAROLINFO — implementacao. O contrato esta em farolinfo.h: codigos e
// fatos, nunca texto livre; sem dado medido, sem linha.
//
// Modelos citados (dono: src/faroltexto.h, issue 35):
//   FT_INFO_QUALI / QUALI_AUDIO / IDIOMA / LEGENDA   (qualidade)
//   FT_INFO_SOBE / FALTAM / MESMO_NIVEL              (continuar)
//   FT_INFO_TERMINA / DUAS_NOITES                    (tempo)
//   FT_INFO_ONDE                                     (onde)
//   FT_INFO_AMIGOS_TERM / AMIGO_TERM / AMIGOS_GOST   (amigos)
//   FT_INFO_SAGA / SAGA_VOCE_VIU                     (saga)
//   FT_INFO_REVER / REVER_SIMPLES                    (rever)
//   FT_INFO_RETOMADA / RETOMADA_SIMPLES              (retomada)
// Modelos que a 35 ainda nao tem e esta issue ja cita (FALTA_TEXTO):
//   FT_INFO_QUALI_SO  "Tem em %s."            (res+fmt sem audio declarado)
//   FT_INFO_SOBE_SO   "A T%d sobe de nota."   (sobe, sem faltam/minutos)
//   FT_INFO_AMIGO_GOST "%s gostou."           (1 amigo, so reacao)
//   FT_INFO_NOTA_CAI  "A T%d cai de nota."    (so a queda; quem para
//     de assistir nao e dado que este modulo recebe — a FT_INFO_CAI da
//     35 afirma abandono e NAO pode sair daqui)

#include "farolinfo.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

// A regra do aceite: 5 ou mais episodios com nota em CADA temporada
// comparada, e diferenca de pelo menos 0,4 (= 4 na escala x10 de notaImdb).
#define FI_MIN_NOTAS 5
#define FI_DIF_MIN   4

const char *farolinfo_k_nome(int k) {
  static const char *const n[FI_K_N] = {
    "qualidade", "continuar", "tempo", "onde",
    "amigos", "saga", "rever", "retomada"
  };
  return (k >= 0 && k < FI_K_N) ? n[k] : "";
}

static void cabeca(FarolInfoLinha *l, int k, const char *m) {
  l->k = k;
  snprintf(l->m, sizeof l->m, "%s", m);
  l->na = 0;
}

static void arg(FarolInfoLinha *l, int i, const char *v) {
  if (i < 0 || i >= FI_ARGS) return;
  snprintf(l->a[i], sizeof l->a[i], "%s", v);
  {
    // Se a copia cortou uma sequencia UTF-8 no meio, recua ate o inicio
    // dela: o byte de inicio (>= C0) sem todos os bytes de continuacao
    // (80..BF) que o caractere pede nao pode fechar a linha.
    size_t len = strlen(l->a[i]), ini = len;
    while (ini > 0 && (unsigned char)l->a[i][ini - 1] >= 0x80 &&
           (unsigned char)l->a[i][ini - 1] < 0xC0)
      ini--;
    if (ini > 0 && (unsigned char)l->a[i][ini - 1] >= 0xC0) {
      unsigned char c = (unsigned char)l->a[i][ini - 1];
      size_t tam = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : 2;
      if (len - (ini - 1) < tam) l->a[i][ini - 1] = '\0';
    }
  }
  if (i + 1 > l->na) l->na = i + 1;
}

static void argNum(FarolInfoLinha *l, int i, long v) {
  char t[24];
  snprintf(t, sizeof t, "%ld", v);
  arg(l, i, t);
}

// --- qualidade ---------------------------------------------------------------

// Rotulo curto da altura, no formato que a casa ja usa ("4K", "1080p").
static void resNome(int altura, char *dst, size_t n) {
  if (altura >= 4000) snprintf(dst, n, "8K");
  else if (altura >= 2000) snprintf(dst, n, "4K");
  else if (altura > 0)   snprintf(dst, n, "%dp", altura);
  else if (n)            dst[0] = '\0';
}

int farolinfo_qualidade(FarolInfoLinha *sai, int max,
                        const FarolInfoFonte *f, int n, const FarolInfoCaps *tv) {
  int i, bi = -1, nl = 0;
  long bp = -1;
  char res[16];
  const char *fmt, *audio, *leg;

  if (!sai || max < 1 || !f || n <= 0) return 0;
  // So entra fonte que a TV CONFIRMOU tocar (toca == 1): desconhecida
  // (-1) nao gera linha nem vence uma confirmada — a linha so afirma o
  // que esta TV toca. Maior altura; no desempate, DV/HDR que a TV toca
  // de verdade e idioma declarado (a linha cita o que ela toca, e a
  // preferencia ja desempata para a fonte que da a melhor linha honesta).
  for (i = 0; i < n; i++) {
    long p;
    if (f[i].toca != 1) continue;
    p = (long)f[i].altura * 100;
    if (tv && tv->dv == 1 && f[i].dv)  p += 20;
    if (tv && tv->hdr == 1 && f[i].hdr) p += 10;
    if (f[i].audio[0])   p += 5;
    if (f[i].legenda[0]) p += 1;
    if (p > bp) { bp = p; bi = i; }
  }
  if (bi < 0) return 0;

  resNome(f[bi].altura, res, sizeof res);
  // Rotulo de formato so com a evidencia exata: DV e um campo proprio;
  // f[].hdr e o indicador GENERICO — sem distincao HDR10/HLG aqui a casa
  // usa "HDR" (mesma categoria de src/vaitocar.c), nunca "HDR10".
  fmt   = (f[bi].dv && tv && tv->dv == 1) ? "Dolby Vision"
        : (f[bi].hdr && tv && tv->hdr == 1) ? "HDR" : "";
  audio = f[bi].audio;
  leg   = f[bi].legenda;

  if (res[0] && fmt[0] && audio[0]) {
    cabeca(&sai[nl], FI_K_QUALIDADE, "FT_INFO_QUALI");
    arg(&sai[nl], 0, res); arg(&sai[nl], 1, fmt); arg(&sai[nl], 2, audio);
    nl++;
  } else if (res[0] && audio[0]) {
    cabeca(&sai[nl], FI_K_QUALIDADE, "FT_INFO_QUALI_AUDIO");
    arg(&sai[nl], 0, res); arg(&sai[nl], 1, audio);
    nl++;
  } else if (res[0]) {
    char rf[32];
    // Sem audio declarado: "Tem em 4K Dolby Vision." / "Tem em 1080p."
    snprintf(rf, sizeof rf, "%s%s%s", res, fmt[0] ? " " : "", fmt);
    cabeca(&sai[nl], FI_K_QUALIDADE, "FT_INFO_QUALI_SO");
    arg(&sai[nl], 0, rf);
    nl++;
  } else if (audio[0]) {
    cabeca(&sai[nl], FI_K_QUALIDADE, "FT_INFO_IDIOMA");
    arg(&sai[nl], 0, audio);
    nl++;
  }
  if (nl < max && leg[0]) {
    cabeca(&sai[nl], FI_K_QUALIDADE, "FT_INFO_LEGENDA");
    arg(&sai[nl], 0, leg);
    nl++;
  }
  return nl;
}

// --- continuar ---------------------------------------------------------------

int farolinfo_continuar(FarolInfoLinha *sai,
                        const FarolInfoTemp *t, int n, int faltam, int minEp) {
  FarolInfoTemp v[128];
  int nv = 0, i, j, dir = 0, tRef = 0, pares = 0;

  if (!sai || !t || n <= 0) return 0;
  // Ordena uma copia por numero: o chamador nao deve precisar ordenar, e a
  // comparacao e entre temporadas CONSECUTIVAS (T sobe/cai em relacao a T-1).
  for (i = 0; i < n && nv < (int)(sizeof v / sizeof v[0]); i++)
    if (t[i].numero > 0) v[nv++] = t[i];
  for (i = 1; i < nv; i++) {
    FarolInfoTemp x = v[i];
    for (j = i; j > 0 && v[j - 1].numero > x.numero; j--) v[j] = v[j - 1];
    v[j] = x;
  }
  // Vale o par mais recente: e o trecho que a pessoa esta vendo agora.
  // Consecutivo por DIFERENCA (numero == INT_MAX nao estoura o +1).
  for (i = 1; i < nv; i++) {
    int d;
    if (v[i].numero - v[i - 1].numero != 1) continue;
    if (v[i - 1].comNota < FI_MIN_NOTAS || v[i].comNota < FI_MIN_NOTAS) continue;
    if (v[i - 1].notaX10 <= 0 || v[i].notaX10 <= 0) continue;
    pares++;
    d = v[i].notaX10 - v[i - 1].notaX10;
    if (d >= FI_DIF_MIN)       { dir = 1;  tRef = v[i].numero; }
    else if (d <= -FI_DIF_MIN) { dir = -1; tRef = v[i].numero; }
  }
  if (dir < 0) {
    // O dado prova a QUEDA DE NOTA, nao o abandono: FT_INFO_CAI da 35 diz
    // "muita gente para aqui", que exige outro dado. Fato novo, so a nota.
    cabeca(sai, FI_K_CONTINUAR, "FT_INFO_NOTA_CAI");
    argNum(sai, 0, tRef);
    return 1;
  }
  if (dir > 0) {
    if (faltam > 0 && minEp > 0) {
      cabeca(sai, FI_K_CONTINUAR, "FT_INFO_SOBE");
      argNum(sai, 0, tRef); argNum(sai, 1, faltam); argNum(sai, 2, minEp);
    } else {
      cabeca(sai, FI_K_CONTINUAR, "FT_INFO_SOBE_SO");
      argNum(sai, 0, tRef);
    }
    return 1;
  }
  if (faltam > 0 && minEp > 0) {
    cabeca(sai, FI_K_CONTINUAR, "FT_INFO_FALTAM");
    argNum(sai, 0, faltam); argNum(sai, 1, minEp);
    return 1;
  }
  if (pares > 0) {
    // Houve comparacao de verdade e nenhuma mexeu: "seguem no mesmo nivel".
    cabeca(sai, FI_K_CONTINUAR, "FT_INFO_MESMO_NIVEL");
    return 1;
  }
  return 0;
}

// --- tempo -------------------------------------------------------------------

int farolinfo_tempo(FarolInfoLinha *sai, const struct tm *agora,
                    int h12, int minutos, int minNoite) {
  long long s;
  int hh, mm;

  if (!sai || !agora || minutos <= 0) return 0;
  if (minNoite > 0 && minutos > minNoite) {
    // Cabe uma noite destas? Nao: a conta e em noites de `minNoite` minutos.
    // A soma vai em 64 bits e satura: com INT_MAX minutos +1 estouraria int.
    long long noites = ((long long)minutos + minNoite - 1) / minNoite;
    if (noites > INT_MAX) noites = INT_MAX;
    cabeca(sai, FI_K_TEMPO, "FT_INFO_DUAS_NOITES");
    argNum(sai, 0, (long)noites);
    argNum(sai, 1, minNoite);
    return 1;
  }
  // O fim dentro do mesmo relogio de parede: a virada do dia e o mod 24 h.
  // minutos * 60 vai em 64 bits: no WASM `long` tem 32 e estoura com
  // duracoes de anos (o modulo aceita qualquer minutos > 0).
  s = (long long)agora->tm_hour * 3600 + agora->tm_min * 60 + agora->tm_sec
      + (long long)minutos * 60;
  s %= 86400;
  if (s < 0) s += 86400;
  hh = (int)(s / 3600);
  mm = (int)((s % 3600) / 60);
  cabeca(sai, FI_K_TEMPO, "FT_INFO_TERMINA");
  if (h12)
    snprintf(sai->a[0], sizeof sai->a[0], "%d:%02d %s",
             hh % 12 ? hh % 12 : 12, mm, hh < 12 ? "AM" : "PM");
  else
    snprintf(sai->a[0], sizeof sai->a[0], "%02d:%02d", hh, mm);
  sai->na = 1;
  return 1;
}

// --- onde --------------------------------------------------------------------

int farolinfo_onde(FarolInfoLinha *sai, const char *servico) {
  if (!sai || !servico || !servico[0]) return 0;
  cabeca(sai, FI_K_ONDE, "FT_INFO_ONDE");
  arg(sai, 0, servico);
  return 1;
}

// --- amigos ------------------------------------------------------------------

int farolinfo_amigos(FarolInfoLinha *sai, const FarolInfoAmigo *am, int n) {
  const char *t[2] = { NULL, NULL }, *g[2] = { NULL, NULL };
  int nt = 0, ng = 0, i;

  if (!sai || !am || n <= 0) return 0;
  for (i = 0; i < n; i++) {
    if (!am[i].nome[0]) continue;
    if (am[i].terminou) { if (nt < 2) t[nt] = am[i].nome; nt++; }
    else if (am[i].gostou) { if (ng < 2) g[ng] = am[i].nome; ng++; }
  }
  if (nt >= 2) {
    cabeca(sai, FI_K_AMIGOS, "FT_INFO_AMIGOS_TERM");
    arg(sai, 0, t[0]); arg(sai, 1, t[1]);
    return 1;
  }
  if (nt == 1) {
    cabeca(sai, FI_K_AMIGOS, "FT_INFO_AMIGO_TERM");
    arg(sai, 0, t[0]);
    return 1;
  }
  if (ng >= 2) {
    cabeca(sai, FI_K_AMIGOS, "FT_INFO_AMIGOS_GOST");
    arg(sai, 0, g[0]); arg(sai, 1, g[1]);
    return 1;
  }
  if (ng == 1) {
    cabeca(sai, FI_K_AMIGOS, "FT_INFO_AMIGO_GOST");
    arg(sai, 0, g[0]);
    return 1;
  }
  return 0;
}

// --- saga --------------------------------------------------------------------

int farolinfo_saga(FarolInfoLinha *sai, int pos, int total, int vistos) {
  if (!sai || pos <= 0 || total <= 1) return 0;
  if (pos > total) pos = total;
  if (vistos < 0) vistos = 0;
  if (vistos > total) vistos = total;
  if (vistos > 0) {
    cabeca(sai, FI_K_SAGA, "FT_INFO_SAGA_VOCE_VIU");
    argNum(sai, 0, pos); argNum(sai, 1, total); argNum(sai, 2, vistos);
  } else {
    cabeca(sai, FI_K_SAGA, "FT_INFO_SAGA");
    argNum(sai, 0, pos); argNum(sai, 1, total);
  }
  return 1;
}

// --- rever -------------------------------------------------------------------

int farolinfo_rever(FarolInfoLinha *sai, int ano, int mes, int nota10) {
  char ma[12];

  if (!sai || ano <= 0 || mes < 1 || mes > 12) return 0;
  if (ano > 9999) ano = 9999;
  snprintf(ma, sizeof ma, "%04d-%02d", ano, mes);
  if (nota10 >= 2 && nota10 % 2 == 0) {
    cabeca(sai, FI_K_REVER, "FT_INFO_REVER");
    arg(sai, 0, ma);
    argNum(sai, 1, nota10 / 2);
  } else {
    cabeca(sai, FI_K_REVER, "FT_INFO_REVER_SIMPLES");
    arg(sai, 0, ma);
  }
  return 1;
}

// --- retomada ----------------------------------------------------------------

int farolinfo_retomada(FarolInfoLinha *sai, int t, int e,
                       long long agora, long long quando) {
  long long d;
  char cod[16];

  if (!sai || t <= 0 || e <= 0) return 0;
  if (quando <= 0 || agora <= quando) {
    cabeca(sai, FI_K_RETOMADA, "FT_INFO_RETOMADA_SIMPLES");
    argNum(sai, 0, t); argNum(sai, 1, e);
    return 1;
  }
  d = agora - quando;                     // segundos desde que parou
  if (d < 3600)        snprintf(cod, sizeof cod, "%lldmin", d / 60 > 0 ? d / 60 : 1);
  else if (d < 86400)  snprintf(cod, sizeof cod, "%lldh", d / 3600);
  else if (d < 14 * 86400) snprintf(cod, sizeof cod, "%lldd", d / 86400);
  else if (d < 63LL * 86400) snprintf(cod, sizeof cod, "%lldw", d / (7 * 86400));
  else if (d < 730LL * 86400) snprintf(cod, sizeof cod, "%lldM", d / (30 * 86400));
  else                  snprintf(cod, sizeof cod, "%lldy", d / (365 * 86400));
  cabeca(sai, FI_K_RETOMADA, "FT_INFO_RETOMADA");
  argNum(sai, 0, t); argNum(sai, 1, e); arg(sai, 2, cod);
  return 1;
}
