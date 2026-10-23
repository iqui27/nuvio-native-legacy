#include "idioma.h"
#include "idiomacod.h"
#include "ajustes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------- tabela
//
// ORDENADA POR CHAVE, e a ordem e conferida em tempo de execucao no primeiro
// uso (ver conferirOrdem). Uma tabela desordenada faria a busca binaria falhar
// em SILENCIO: alguns textos traduziriam, outros nao, e o padrao pareceria
// aleatorio. Nao ha teste que pegue isso melhor que a propria busca.
typedef struct { const char *pt, *en; } Par;

static const Par TAB[] = {
#include "idioma_tab.h"
};
#define TAB_N ((int)(sizeof TAB / sizeof *TAB))

// OS OUTROS IDIOMAS moram em tabelas PARALELAS, uma por idioma, com uma entrada
// por linha de idioma_tab.h e na MESMA ordem. A busca binaria roda UMA vez, na
// chave portuguesa, e devolve o indice; o indice serve a qualquer idioma. Por
// isso o custo por linha desenhada e o de antes de haver 5 idiomas, e o cache
// abaixo nao precisa saber qual idioma esta ligado (trocar de idioma nao o
// invalida). Nos arquivos idioma_XX.h cada linha e T("chave pt", "traducao"):
// a macro joga a chave fora — ela so existe para o revisor ler a linha inteira
// e para tools/idiomas.py conferir que o alinhamento nao escorregou.
#define T(chave, traducao) traducao
static const char *const TAB_RO[] = {
#include "idioma_ro.h"
};
static const char *const TAB_UK[] = {
#include "idioma_uk.h"
};
static const char *const TAB_RU[] = {
#include "idioma_ru.h"
};
static const char *const TAB_FR[] = {
#include "idioma_fr.h"
};
static const char *const TAB_DE[] = {
#include "idioma_de.h"
};
static const char *const TAB_ES[] = {
#include "idioma_es.h"
};
static const char *const TAB_IT[] = {
#include "idioma_it.h"
};
static const char *const TAB_NL[] = {
#include "idioma_nl.h"
};
static const char *const TAB_PL[] = {
#include "idioma_pl.h"
};
static const char *const TAB_TR[] = {
#include "idioma_tr.h"
};
static const char *const TAB_PTPT[] = {
#include "idioma_ptpt.h"
};
static const char *const TAB_SV[] = {
#include "idioma_sv.h"
};
static const char *const TAB_DA[] = {
#include "idioma_da.h"
};
static const char *const TAB_NO[] = {
#include "idioma_no.h"
};
static const char *const TAB_CS[] = {
#include "idioma_cs.h"
};
static const char *const TAB_SK[] = {
#include "idioma_sk.h"
};
static const char *const TAB_SL[] = {
#include "idioma_sl.h"
};
static const char *const TAB_HU[] = {
#include "idioma_hu.h"
};
static const char *const TAB_LT[] = {
#include "idioma_lt.h"
};
static const char *const TAB_BS[] = {
#include "idioma_bs.h"
};
static const char *const TAB_SR[] = {
#include "idioma_sr.h"
};
static const char *const TAB_BG[] = {
#include "idioma_bg.h"
};
static const char *const TAB_EL[] = {
#include "idioma_el.h"
};
static const char *const TAB_ID[] = {
#include "idioma_id.h"
};
static const char *const TAB_VI[] = {
#include "idioma_vi.h"
};
static const char *const TAB_JA[] = {
#include "idioma_ja.h"
};
static const char *const TAB_ZHCN[] = {
#include "idioma_zhcn.h"
};
static const char *const TAB_ZHTW[] = {
#include "idioma_zhtw.h"
};
static const char *const TAB_AR[] = {
#include "idioma_ar.h"
};
#undef T
// Uma tabela com o numero errado de linhas desalinharia TODAS as traducoes
// depois dela — falha na compilacao, nao em silencio na tela.
_Static_assert(sizeof TAB_RO / sizeof *TAB_RO == sizeof TAB / sizeof *TAB,
               "idioma_ro.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");
_Static_assert(sizeof TAB_UK / sizeof *TAB_UK == sizeof TAB / sizeof *TAB,
               "idioma_uk.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");
_Static_assert(sizeof TAB_RU / sizeof *TAB_RU == sizeof TAB / sizeof *TAB,
               "idioma_ru.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");
_Static_assert(sizeof TAB_FR / sizeof *TAB_FR == sizeof TAB / sizeof *TAB,
               "idioma_fr.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");
_Static_assert(sizeof TAB_DE / sizeof *TAB_DE == sizeof TAB / sizeof *TAB,
               "idioma_de.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");
_Static_assert(sizeof TAB_ES / sizeof *TAB_ES == sizeof TAB / sizeof *TAB,
               "idioma_es.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");
_Static_assert(sizeof TAB_IT / sizeof *TAB_IT == sizeof TAB / sizeof *TAB,
               "idioma_it.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");
_Static_assert(sizeof TAB_NL / sizeof *TAB_NL == sizeof TAB / sizeof *TAB,
               "idioma_nl.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");
_Static_assert(sizeof TAB_PL / sizeof *TAB_PL == sizeof TAB / sizeof *TAB,
               "idioma_pl.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");
_Static_assert(sizeof TAB_TR / sizeof *TAB_TR == sizeof TAB / sizeof *TAB,
               "idioma_tr.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");
_Static_assert(sizeof TAB_PTPT / sizeof *TAB_PTPT == sizeof TAB / sizeof *TAB,
               "idioma_ptpt.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");
_Static_assert(sizeof TAB_SV / sizeof *TAB_SV == sizeof TAB / sizeof *TAB,
               "idioma_sv.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");
_Static_assert(sizeof TAB_DA / sizeof *TAB_DA == sizeof TAB / sizeof *TAB,
               "idioma_da.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");
_Static_assert(sizeof TAB_NO / sizeof *TAB_NO == sizeof TAB / sizeof *TAB,
               "idioma_no.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");
_Static_assert(sizeof TAB_CS / sizeof *TAB_CS == sizeof TAB / sizeof *TAB,
               "idioma_cs.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");
_Static_assert(sizeof TAB_SK / sizeof *TAB_SK == sizeof TAB / sizeof *TAB,
               "idioma_sk.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");
_Static_assert(sizeof TAB_SL / sizeof *TAB_SL == sizeof TAB / sizeof *TAB,
               "idioma_sl.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");
_Static_assert(sizeof TAB_HU / sizeof *TAB_HU == sizeof TAB / sizeof *TAB,
               "idioma_hu.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");
_Static_assert(sizeof TAB_LT / sizeof *TAB_LT == sizeof TAB / sizeof *TAB,
               "idioma_lt.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");
_Static_assert(sizeof TAB_BS / sizeof *TAB_BS == sizeof TAB / sizeof *TAB,
               "idioma_bs.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");
_Static_assert(sizeof TAB_SR / sizeof *TAB_SR == sizeof TAB / sizeof *TAB,
               "idioma_sr.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");
_Static_assert(sizeof TAB_BG / sizeof *TAB_BG == sizeof TAB / sizeof *TAB,
               "idioma_bg.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");
_Static_assert(sizeof TAB_EL / sizeof *TAB_EL == sizeof TAB / sizeof *TAB,
               "idioma_el.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");
_Static_assert(sizeof TAB_ID / sizeof *TAB_ID == sizeof TAB / sizeof *TAB,
               "idioma_id.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");
_Static_assert(sizeof TAB_VI / sizeof *TAB_VI == sizeof TAB / sizeof *TAB,
               "idioma_vi.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");
_Static_assert(sizeof TAB_JA / sizeof *TAB_JA == sizeof TAB / sizeof *TAB,
               "idioma_ja.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");
_Static_assert(sizeof TAB_ZHCN / sizeof *TAB_ZHCN == sizeof TAB / sizeof *TAB,
               "idioma_zhcn.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");
_Static_assert(sizeof TAB_ZHTW / sizeof *TAB_ZHTW == sizeof TAB / sizeof *TAB,
               "idioma_zhtw.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");
_Static_assert(sizeof TAB_AR / sizeof *TAB_AR == sizeof TAB / sizeof *TAB,
               "idioma_ar.h: uma linha por entrada de idioma_tab.h (tools/idiomas.py --sincronizar)");

// A traducao da entrada `i` no idioma `lg` (nunca IDIOMA_PT). Valor vazio cai no
// ingles: uma entrada nova ainda sem traducao aparece em ingles, nao em branco.
static const char *traduzida(int i, int lg) {
  const char *r = NULL;
  switch (lg) {
    case IDIOMA_RO: r = TAB_RO[i]; break;
    case IDIOMA_UK: r = TAB_UK[i]; break;
    case IDIOMA_RU: r = TAB_RU[i]; break;
    case IDIOMA_FR: r = TAB_FR[i]; break;
    case IDIOMA_DE: r = TAB_DE[i]; break;
    case IDIOMA_ES: r = TAB_ES[i]; break;
    case IDIOMA_IT: r = TAB_IT[i]; break;
    case IDIOMA_NL: r = TAB_NL[i]; break;
    case IDIOMA_PL: r = TAB_PL[i]; break;
    case IDIOMA_TR: r = TAB_TR[i]; break;
    case IDIOMA_PTPT: r = TAB_PTPT[i]; break;
    case IDIOMA_SV: r = TAB_SV[i]; break;
    case IDIOMA_DA: r = TAB_DA[i]; break;
    case IDIOMA_NO: r = TAB_NO[i]; break;
    case IDIOMA_CS: r = TAB_CS[i]; break;
    case IDIOMA_SK: r = TAB_SK[i]; break;
    case IDIOMA_SL: r = TAB_SL[i]; break;
    case IDIOMA_HU: r = TAB_HU[i]; break;
    case IDIOMA_LT: r = TAB_LT[i]; break;
    case IDIOMA_BS: r = TAB_BS[i]; break;
    case IDIOMA_SR: r = TAB_SR[i]; break;
    case IDIOMA_BG: r = TAB_BG[i]; break;
    case IDIOMA_EL: r = TAB_EL[i]; break;
    case IDIOMA_ID: r = TAB_ID[i]; break;
    case IDIOMA_VI: r = TAB_VI[i]; break;
    case IDIOMA_JA: r = TAB_JA[i]; break;
    case IDIOMA_ZHCN: r = TAB_ZHCN[i]; break;
    case IDIOMA_ZHTW: r = TAB_ZHTW[i]; break;
    case IDIOMA_AR: r = TAB_AR[i]; break;
    default: break;
  }
  return r && *r ? r : TAB[i].en;
}

// ---------------------------------------------------------------- registro
//
// Levantamento das strings vivas. Guarda o que ja viu para nao reescrever a
// mesma linha a cada quadro — sao ~200 linhas por quadro a 60fps.
#define REG_MAX 1024
static char *vistos[REG_MAX];
static int   nVistos;
static FILE *arqReg;
static int   regTentado;

void idioma_registrar(const char *s) {
  int i;
  if (!s || !*s) return;
  if (!regTentado) {
    const char *caminho = getenv("NUVIO_TEXTO_DUMP");
    regTentado = 1;
    if (caminho && *caminho) arqReg = fopen(caminho, "w");
  }
  if (!arqReg || nVistos >= REG_MAX) return;
  for (i = 0; i < nVistos; i++) if (!strcmp(vistos[i], s)) return;
  vistos[nVistos] = strdup(s);
  if (!vistos[nVistos]) return;
  nVistos++;
  fprintf(arqReg, "%s\n", s);
  fflush(arqReg);
}

// ---------------------------------------------------------------- traducao

const char *idioma_mes_data(int mes, const char *nomePt) {
  static const char *UK[12] = {
    "січня", "лютого", "березня", "квітня", "травня", "червня",
    "липня", "серпня", "вересня", "жовтня", "листопада", "грудня"
  };
  static const char *RU[12] = {
    "января", "февраля", "марта", "апреля", "мая", "июня",
    "июля", "августа", "сентября", "октября", "ноября", "декабря"
  };
  // GENITIVO nas linguas que declinam o mes na data por extenso: "21 września",
  // "21. září", "rugsėjo 21 d.", "21 Σεπτεμβρίου". O calendario ("Wrzesień 2026")
  // continua pedindo o nominativo, que e a traducao da chave.
  // O bulgaro nao declina o mes ("21 септември"), o hungaro, o turco e o
  // vietnamita tambem nao: ficam com a forma da tabela. Japones e chines nem
  // passam por aqui (a data deles e numerica, ver idioma_data_asiatica).
  static const char *PL[12] = {
    "stycznia", "lutego", "marca", "kwietnia", "maja", "czerwca",
    "lipca", "sierpnia", "września", "października", "listopada", "grudnia"
  };
  static const char *CS[12] = {
    "ledna", "února", "března", "dubna", "května", "června",
    "července", "srpna", "září", "října", "listopadu", "prosince"
  };
  static const char *SK[12] = {
    "januára", "februára", "marca", "apríla", "mája", "júna",
    "júla", "augusta", "septembra", "októbra", "novembra", "decembra"
  };
  static const char *SL[12] = {
    "januarja", "februarja", "marca", "aprila", "maja", "junija",
    "julija", "avgusta", "septembra", "oktobra", "novembra", "decembra"
  };
  static const char *LT[12] = {
    "sausio", "vasario", "kovo", "balandžio", "gegužės", "birželio",
    "liepos", "rugpjūčio", "rugsėjo", "spalio", "lapkričio", "gruodžio"
  };
  static const char *BS[12] = {
    "januara", "februara", "marta", "aprila", "maja", "juna",
    "jula", "augusta", "septembra", "oktobra", "novembra", "decembra"
  };
  static const char *SR[12] = {
    "januara", "februara", "marta", "aprila", "maja", "juna",
    "jula", "avgusta", "septembra", "oktobra", "novembra", "decembra"
  };
  static const char *EL[12] = {
    "Ιανουαρίου", "Φεβρουαρίου", "Μαρτίου", "Απριλίου", "Μαΐου", "Ιουνίου",
    "Ιουλίου", "Αυγούστου", "Σεπτεμβρίου", "Οκτωβρίου", "Νοεμβρίου", "Δεκεμβρίου"
  };
  int lg = ajustes_idioma();
  if (mes >= 1 && mes <= 12) {
    switch (lg) {
      case IDIOMA_UK: return UK[mes - 1];
      case IDIOMA_RU: return RU[mes - 1];
      case IDIOMA_PL: return PL[mes - 1];
      case IDIOMA_CS: return CS[mes - 1];
      case IDIOMA_SK: return SK[mes - 1];
      case IDIOMA_SL: return SL[mes - 1];
      case IDIOMA_LT: return LT[mes - 1];
      case IDIOMA_BS: return BS[mes - 1];
      case IDIOMA_SR: return SR[mes - 1];
      case IDIOMA_EL: return EL[mes - 1];
      default: break;
    }
  }
  return i18n(nomePt);
}

static int ordemOk = -1;
static void conferirOrdem(void) {
  int i;
  ordemOk = 1;
  for (i = 1; i < TAB_N; i++) {
    if (strcmp(TAB[i - 1].pt, TAB[i].pt) >= 0) {
      // Falar alto uma vez. Uma tabela fora de ordem nao quebra o app, ela
      // traduz PELA METADE — o pior defeito possivel, porque parece escolha.
      printf("[idioma] TABELA FORA DE ORDEM em %d: \"%s\" antes de \"%s\"\n",
             i, TAB[i - 1].pt, TAB[i].pt);
      fflush(stdout);
      ordemOk = 0;
      return;
    }
  }
}

// CACHE DA BUSCA. text.c chama i18n em TODA linha desenhada, todo quadro;
// com o ingles ligado cada chamada era uma busca binaria de ~11 strcmp em
// 1500 entradas. Medido no Mac (perfil CDP, 35 s de home): 268 ms dentro de
// i18n, a segunda funcao mais cara do fio principal depois de main — e na
// Samsung o mesmo trabalho custa varias vezes mais. Tabela direta de 1024
// posicoes chaveada pelo hash FNV-1a do texto: acerto = 1 passada pelo texto
// + 1 strcmp (positivo) ou 0 strcmp (negativo, confiado pelo hash de 64 bits
// mais o tamanho). Titulo de filme e fragmento de sinopse tambem entram — sao
// justamente os negativos que antes pagavam a busca inteira.
#define I18N_CACHE 1024
static struct { unsigned long long h; unsigned n; int idx; } cache[I18N_CACHE];

const char *i18n(const char *s) {
  int lo = 0, hi = TAB_N - 1;
  unsigned long long h = 1469598103934665603ull;
  unsigned n = 0, slot;
  const unsigned char *p;
  int lg;
  idioma_registrar(s);
  if (!s || !*s) return s;
  lg = ajustes_idioma();
  if (lg == IDIOMA_PT) return s;
  if (ordemOk < 0) conferirOrdem();
  if (!ordemOk) return s;
  for (p = (const unsigned char *)s; *p; p++, n++) { h ^= *p; h *= 1099511628211ull; }
  slot = (unsigned)(h % I18N_CACHE);
  if (cache[slot].h == h && cache[slot].n == n && (cache[slot].idx < 0 || !strcmp(s, TAB[cache[slot].idx].pt)))
    return cache[slot].idx < 0 ? s : traduzida(cache[slot].idx, lg);
  while (lo <= hi) {
    int m = (lo + hi) / 2;
    int c = strcmp(s, TAB[m].pt);
    if (c == 0) { cache[slot].h = h; cache[slot].n = n; cache[slot].idx = m; return traduzida(m, lg); }
    if (c < 0) hi = m - 1; else lo = m + 1;
  }
  cache[slot].h = h; cache[slot].n = n; cache[slot].idx = -1;
  // NAO ADIANTA RECLAMAR AQUI, e eu tentei: text.c chama i18n em cada linha
  // DESENHADA, entao esta funcao ve tambem titulo de filme, sinopse e cada
  // fragmento de quebra de linha. Uma medicao de 25 s no Mac produziu 96
  // avisos — sinopse do Fallout palavra a palavra, letras soltas do relogio —
  // e encheu o teto antes de qualquer texto de interface aparecer. O sinal
  // real ficaria enterrado no log de quem relata. Separar interface de
  // conteudo aqui exigiria a mesma heuristica de portugues que ja falhou tres
  // vezes na varredura estatica, e ela erra igual em "Detalhes" e "Cartazes".
  return s;
}
