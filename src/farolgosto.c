// Modelo de gosto do Farol — ver farolgosto.h (lote sense, issue 37; secoes
// 2.1 e 2.2 do PLANO-GOSTO). Modulo puro: sem SDL, sem rede, sem malloc e sem
// dados.h — o teste linka so este .c, como tests/resolver.sh faz com
// resolverregra.c. O relogio chega injetado por parametro (`dia`),
// nunca por time(): o mesmo binario roda o aquecimento na TV e o teste com
// relogio falso.

#include "farolgosto.h"
#include <stdio.h>
#include <string.h>

_Static_assert(sizeof(FarolAfin) == 10, "FarolAfin tem de ter 10 B");
_Static_assert(sizeof(FarolGosto) <= 6144, "FarolGosto tem de caber em 6144 B");

// ---------------------------------------------------------------------------
// Decaimento: peso *= 2^(-dias/meia). Tabela de 16 entradas (plano 2.2), sem
// pow: FG_DEC[k] = 65536 * 2^(-k/16) cobre um dezesseis-avo de meia-vida; a
// parte inteira vira deslocamento.
static const uint32_t FG_DEC[16] = {
  65536, 62757, 60097, 57549, 55109, 52773, 50535, 48393,
  46341, 44376, 42495, 40693, 38968, 37316, 35734, 34219
};

// Fator Q16.16 de 2^(-q/16), com q em dezesseis-avos de meia-vida.
// q <= 0 nao decai; 256+ avos (16+ meias-vidas) devolvem 0.
static int32_t fatorAvos(int32_t q) {
  int sh;
  uint32_t t;
  if (q <= 0) return 65536;
  sh = (int)(q >> 4);
  if (sh >= 16) return 0;
  t = FG_DEC[q & 15];
  return sh ? (int32_t)((t + ((uint32_t)1 << (sh - 1))) >> sh) : (int32_t)t;
}

// Fator Q16.16 de 2^(-dias/meia). dias <= 0 nao decai (relogio nao anda para
// tras); 16+ meias-vidas devolvem 0.
static int32_t fatorDec(int32_t dias, int meia) {
  if (dias <= 0) return 65536;
  return fatorAvos((int32_t)(((int64_t)dias << 4) / meia));
}

// Versao para o TOQUE (escrita): os dias viram dezesseis-avos de dia e se
// somam ao que ficou pendente em *resto; so os avos de meia-vida completos
// decaem e o resto espera o proximo toque — nenhum pedaco de intervalo se
// perde. Sem o resto, toques diarios com meia-vida de 45/120 dias aplicavam
// fator 1 e ainda avancavam a referencia, congelando o peso para sempre
// (bug da revisao: 45 sessoes diarias deixavam a celula antiga intacta).
// Devolve os avos completos, ou -1 quando o gap nao e positivo (relogio
// atrasado: nada decai e o resto fica como estava).
static int32_t decaiAvos(int32_t gap, int meia, uint8_t *resto) {
  int32_t p;
  if (gap <= 0) return -1;
  p = gap * 16 + *resto;
  *resto = (uint8_t)(p % meia);
  return p / meia;
}

// |peso| projetado em `dia` sem mexer no guardado (decaimento preguiçoso).
static int16_t afinProjetado(const FarolAfin *a, uint16_t dia, int meia) {
  int64_t v;
  int32_t f = fatorDec((int32_t)dia - (int32_t)a->dia, meia);
  v = (int64_t)a->peso * f;
  v = v >= 0 ? (v + 32768) >> 16 : -(((-v) + 32768) >> 16);
  return (int16_t)v;
}

// Slot de `id` em arr: acha, usa o primeiro livre ou, cheio, despeja o de
// menor |peso| x frescor (= |peso projetado|); em empate, o mais velho.
// Devolve o indice; slot novo ou despejado zera o resto de decaimento.
static int afinSlot(FarolAfin *arr, uint8_t *resto, int cap, int32_t id,
                    uint16_t dia, int meia) {
  int i, livre = -1;
  for (i = 0; i < cap; i++) {
    if (arr[i].id == id) return i;
    if (livre < 0 && arr[i].id == 0 && arr[i].n == 0 && arr[i].peso == 0)
      livre = i;
  }
  if (livre < 0) {
    int pior = 0;
    int64_t piorV = -1;
    for (i = 0; i < cap; i++) {
      int64_t p = afinProjetado(&arr[i], dia, meia);
      int64_t v = p < 0 ? -p : p;
      if (piorV < 0 || v < piorV ||
          (v == piorV && arr[i].dia < arr[pior].dia)) {
        piorV = v;
        pior = i;
      }
    }
    livre = pior;
    arr[livre].peso = 0;
    arr[livre].n = 0;
  }
  arr[livre].id = id;
  arr[livre].dia = dia;
  resto[livre] = 0;
  return livre;
}

// Toca a afinidade: decai ate `dia` (sem perda cumulativa, via *resto), soma
// delta saturado por n e prende em [-32, +32] (= +-8192 em Q8.8).
// n e dia so avancam.
static void afinToca(FarolAfin *a, uint8_t *resto, int delta, uint16_t dia,
                     int meia) {
  int32_t p;
  int32_t num = delta * 8, den = (int32_t)a->n + 8;
  {
    int32_t q = decaiAvos((int32_t)dia - a->dia, meia, resto);
    int64_t v;
    int32_t f = q < 0 ? 65536 : fatorAvos(q);
    v = (int64_t)a->peso * f;
    v = v >= 0 ? (v + 32768) >> 16 : -(((-v) + 32768) >> 16);
    p = (int32_t)v;
  }
  p += (num + (num >= 0 ? den / 2 : -den / 2)) / den;
  if (p > FAROL_GOSTO_PESOMAX) p = FAROL_GOSTO_PESOMAX;
  if (p < -FAROL_GOSTO_PESOMAX) p = -FAROL_GOSTO_PESOMAX;
  a->peso = (int16_t)p;
  if (a->n < 0xffff) a->n++;
  if (dia > a->dia) a->dia = dia;
}

// Toca uma celula de hora[][]: como o bloco nao tem dia por celula, o diaBase
// do modelo e a referencia — ao tocar, TODAS as celulas decaem ate `dia`
// (40 multiplicacoes) e o bloco inteiro fica com a mesma referencia. O resto
// pendente mora em g->horaResto, entao toques diarios nao perdem o intervalo.
static void horaToca(FarolGosto *g, int fh, int fd, int delta, uint16_t dia) {
  int32_t v;
  int32_t q = decaiAvos((int32_t)dia - g->diaBase, FG_MEIA_HORA,
                        &g->horaResto);
  if (q >= 0) {
    g->diaBase = dia;
    if (q > 0) {
      int32_t f = fatorAvos(q);
      int i, j;
      for (i = 0; i < FG_HORA_F; i++)
        for (j = 0; j < FG_HORA_D; j++) {
          int64_t c = (int64_t)g->hora[i][j] * f;
          c = c >= 0 ? (c + 32768) >> 16 : -(((-c) + 32768) >> 16);
          g->hora[i][j] = (int16_t)c;
        }
    }
  }
  v = (int32_t)g->hora[fh][fd] + delta;
  if (v > 32767) v = 32767;
  if (v < -32768) v = -32768;
  g->hora[fh][fd] = (int16_t)v;
}

static int durFaixa(int min) {
  if (min <= 0) return -1;
  if (min < 30) return 0;
  if (min < 60) return 1;
  if (min < 100) return 2;
  if (min < 140) return 3;
  return 4;
}

void farolgosto_iniciar(FarolGosto *g, uint16_t perfil) {
  memset(g, 0, sizeof *g);
  g->versao = FAROL_GOSTO_VERSAO;
  g->perfil = perfil;
}

void farolgosto_aplicar(FarolGosto *g, const FarolGostoFicha *f, int delta,
                        uint16_t dia, int faixaHora, int obraNova) {
  int i, fd;
  if (!g || !f) return;
  // Contador de frio (plano 3.4): quem chama marca obraNova so no primeiro
  // evento de cada obra, pela chave da ficha — obras repetidas e fichas sem
  // TMDB ou de reserva contam do jeito certo. Vale mesmo com delta 0: a obra
  // tem ficha, so o sinal e neutro.
  if (obraNova && g->titulos < 0xffffffffu) g->titulos++;
  if (!delta) return;
  for (i = 0; i < 4 && f->genero[i]; i++) {
    int j, s;
    for (j = 0; j < i; j++) if (f->genero[j] == f->genero[i]) break;
    if (j < i) continue;                       // id repetido na ficha: 1x so
    s = afinSlot(g->genero, g->restoGenero, FG_GENERO_N, f->genero[i], dia,
                 FG_MEIA_AFIN);
    afinToca(&g->genero[s], &g->restoGenero[s], delta, dia, FG_MEIA_AFIN);
  }
  for (i = 0; i < 6 && f->pessoa[i]; i++) {
    int j, s;
    for (j = 0; j < i; j++) if (f->pessoa[j] == f->pessoa[i]) break;
    if (j < i) continue;
    s = afinSlot(g->pessoa, g->restoPessoa, FG_PESSOA_N, f->pessoa[i], dia,
                 FG_MEIA_AFIN);
    afinToca(&g->pessoa[s], &g->restoPessoa[s], delta, dia, FG_MEIA_AFIN);
  }
  for (i = 0; i < 8 && f->tema[i]; i++) {
    int j, s;
    for (j = 0; j < i; j++) if (f->tema[j] == f->tema[i]) break;
    if (j < i) continue;
    s = afinSlot(g->tema, g->restoTema, FG_TEMA_N, f->tema[i], dia,
                 FG_MEIA_AFIN);
    afinToca(&g->tema[s], &g->restoTema[s], delta, dia, FG_MEIA_AFIN);
  }
  if (f->ano > 0) {
    int idx = (f->ano - 1920) / 10, s;
    if (idx < 0) idx = 0;
    if (idx > FG_DECADA_N - 1) idx = FG_DECADA_N - 1;
    s = afinSlot(g->decada, g->restoDecada, FG_DECADA_N, 1920 + idx * 10,
                 dia, FG_MEIA_AFIN);
    afinToca(&g->decada[s], &g->restoDecada[s], delta, dia, FG_MEIA_AFIN);
  }
  if (f->idioma) {
    int s = afinSlot(g->idioma, g->restoIdioma, FG_IDIOMA_N, f->idioma, dia,
                     FG_MEIA_AFIN);
    afinToca(&g->idioma[s], &g->restoIdioma[s], delta, dia, FG_MEIA_AFIN);
  }
  fd = durFaixa(f->minutos);
  if (fd >= 0) {
    int idx = fd + (f->tipo == FG_TIPO_SERIE ? 5 : 0), s;
    s = afinSlot(g->duracao, g->restoDuracao, FG_DURACAO_N, idx + 1, dia,
                 FG_MEIA_HORA);
    afinToca(&g->duracao[s], &g->restoDuracao[s], delta, dia, FG_MEIA_HORA);
    if (faixaHora >= 0 && faixaHora < FG_HORA_F)
      horaToca(g, faixaHora, fd, delta, dia);
  }
  g->versao = FAROL_GOSTO_VERSAO;
}

int32_t farolgosto_peso(const FarolGosto *g, int dim, int32_t id, uint16_t dia) {
  const FarolAfin *arr;
  int cap, meia, i;
  if (!g || !id) return 0;
  switch (dim) {
    case FG_DIM_GENERO:  arr = g->genero;  cap = FG_GENERO_N;  meia = FG_MEIA_AFIN; break;
    case FG_DIM_PESSOA:  arr = g->pessoa;  cap = FG_PESSOA_N;  meia = FG_MEIA_AFIN; break;
    case FG_DIM_TEMA:    arr = g->tema;    cap = FG_TEMA_N;    meia = FG_MEIA_AFIN; break;
    case FG_DIM_DECADA:  arr = g->decada;  cap = FG_DECADA_N;  meia = FG_MEIA_AFIN; break;
    case FG_DIM_IDIOMA:  arr = g->idioma;  cap = FG_IDIOMA_N;  meia = FG_MEIA_AFIN; break;
    case FG_DIM_DURACAO: arr = g->duracao; cap = FG_DURACAO_N; meia = FG_MEIA_HORA; break;
    default: return 0;
  }
  for (i = 0; i < cap; i++)
    if (arr[i].id == id) return afinProjetado(&arr[i], dia, meia);
  return 0;
}

int16_t farolgosto_hora(const FarolGosto *g, int fh, int fd, uint16_t dia) {
  int64_t c;
  if (!g || fh < 0 || fh >= FG_HORA_F || fd < 0 || fd >= FG_HORA_D) return 0;
  {
    int32_t f = fatorDec(dia > g->diaBase ? (int32_t)dia - g->diaBase : 0,
                         FG_MEIA_HORA);
    c = (int64_t)g->hora[fh][fd] * f;
  }
  c = c >= 0 ? (c + 32768) >> 16 : -(((-c) + 32768) >> 16);
  return (int16_t)c;
}

void farolgosto_recusar(FarolGosto *g, uint32_t h) {
  if (!g || !h || farolgosto_recusado(g, h)) return;
  g->recusado[g->recProx % FG_RECUS_N] = h;
  g->recProx++;
}

int farolgosto_recusado(const FarolGosto *g, uint32_t h) {
  int i, n;
  if (!g || !h) return 0;
  n = g->recProx < FG_RECUS_N ? g->recProx : FG_RECUS_N;
  for (i = 0; i < n; i++)
    if (g->recusado[i] == h) return 1;
  return 0;
}

void farolgosto_mostrado(FarolGosto *g, uint32_t h, uint8_t dia) {
  int i, n;
  if (!g || !h) return;
  n = g->mostProx < FG_MOSTR_N ? g->mostProx : FG_MOSTR_N;
  for (i = 0; i < n; i++)
    if (g->mostrado[i].h == h) {
      if (g->mostrado[i].dia != dia) {
        if (g->mostrado[i].vezes < 0xff) g->mostrado[i].vezes++;
        g->mostrado[i].dia = dia;
      }
      return;
    }
  {
    int s = g->mostProx % FG_MOSTR_N;
    g->mostrado[s].h = h;
    g->mostrado[s].vezes = 1;
    g->mostrado[s].dia = dia;
    g->mostProx++;
  }
}

int farolgosto_mostrado_dias(const FarolGosto *g, uint32_t h) {
  int i, n;
  if (!g || !h) return 0;
  n = g->mostProx < FG_MOSTR_N ? g->mostProx : FG_MOSTR_N;
  for (i = 0; i < n; i++)
    if (g->mostrado[i].h == h) return g->mostrado[i].vezes;
  return 0;
}

// ---------------------------------------------------------------------------
// Texto: "FG <v> <perfil> <titulos> <diaBase>", uma linha por afinidade
// ("g <id> <peso> <n> <dia>"), "h <f> <v0..v4>" para o bloco de hora,
// "r <h...>" (16 por linha) e "m <h> <vezes> <dia> ..." (8 triplas por linha).
// recusado/mostrado saem do mais velho para o mais novo: a leitura enche os
// slots em ordem e o cursor reconstruido aponta de novo para o mais velho —
// e por isso escrever(ler(escrever(g))) sai byte a byte igual.

#define EMITE(...) do {                                            \
    int w = snprintf(dst + pos, cap - pos, __VA_ARGS__);           \
    if (w < 0 || (size_t)w >= cap - pos) return 0;                 \
    pos += (size_t)w;                                              \
  } while (0)

static size_t emiteAfin(char *dst, size_t cap, size_t pos, char k,
                        const FarolAfin *a, int n) {
  int i;
  for (i = 0; i < n; i++)
    if (a[i].id || a[i].n || a[i].peso)
      EMITE("%c %d %d %u %u\n", k, a[i].id, a[i].peso, a[i].n, a[i].dia);
  return pos;
}

size_t farolgosto_escrever(const FarolGosto *g, char *dst, size_t cap) {
  size_t pos = 0;
  int i, j, n;
  if (!g || !dst || !cap) return 0;
  EMITE("FG %u %u %u %u\n", g->versao, g->perfil, g->titulos, g->diaBase);
  pos = emiteAfin(dst, cap, pos, 'g', g->genero, FG_GENERO_N);  if (!pos) return 0;
  pos = emiteAfin(dst, cap, pos, 'p', g->pessoa, FG_PESSOA_N);  if (!pos) return 0;
  pos = emiteAfin(dst, cap, pos, 't', g->tema, FG_TEMA_N);      if (!pos) return 0;
  pos = emiteAfin(dst, cap, pos, 'e', g->decada, FG_DECADA_N);  if (!pos) return 0;
  pos = emiteAfin(dst, cap, pos, 'i', g->idioma, FG_IDIOMA_N);  if (!pos) return 0;
  pos = emiteAfin(dst, cap, pos, 'u', g->duracao, FG_DURACAO_N); if (!pos) return 0;
  for (i = 0; i < FG_HORA_F; i++) {
    EMITE("h %d", i);
    for (j = 0; j < FG_HORA_D; j++) EMITE(" %d", g->hora[i][j]);
    EMITE("\n");
  }
  n = g->recProx < FG_RECUS_N ? g->recProx : FG_RECUS_N;
  for (i = 0; i < n; i++) {
    if (i % 16 == 0) EMITE("r");
    EMITE(" %u", g->recusado[(g->recProx - n + i) % FG_RECUS_N]);
    if (i % 16 == 15 || i == n - 1) EMITE("\n");
  }
  n = g->mostProx < FG_MOSTR_N ? g->mostProx : FG_MOSTR_N;
  for (i = 0; i < n; i++) {
    if (i % 8 == 0) EMITE("m");
    {
      int s = (g->mostProx - n + i) % FG_MOSTR_N;
      EMITE(" %u %u %u", g->mostrado[s].h, g->mostrado[s].vezes,
            g->mostrado[s].dia);
    }
    if (i % 8 == 7 || i == n - 1) EMITE("\n");
  }
  return pos;
}

// Inteiro com sinal de [p,e): pula espacos; rc -1 se nao houver numero.
// Satura em +-4G para um campo torto nunca dar overflow.
static int pegaInt(const char **pp, const char *e, int64_t *v) {
  const char *p = *pp;
  int64_t r = 0;
  int neg = 0;
  while (p < e && (*p == ' ' || *p == '\t')) p++;
  if (p >= e) return -1;
  if (*p == '-') { neg = 1; p++; }
  if (p >= e || *p < '0' || *p > '9') return -1;
  while (p < e && *p >= '0' && *p <= '9') {
    r = r * 10 + (*p - '0');
    if (r > 4294967296LL) r = 4294967296LL;
    p++;
  }
  *v = neg ? -r : r;
  *pp = p;
  return 0;
}

static int16_t prende16(int64_t v) {
  if (v > 32767) return 32767;
  if (v < -32768) return -32768;
  return (int16_t)v;
}

static void leAfin(FarolGosto *t, char k, const char *ls, const char *le) {
  int64_t id, peso, n, dia;
  FarolAfin *arr;
  uint8_t *resto;
  int cap, meia, s;
  switch (k) {
    case 'g': arr = t->genero;  resto = t->restoGenero;  cap = FG_GENERO_N;  meia = FG_MEIA_AFIN; break;
    case 'p': arr = t->pessoa;  resto = t->restoPessoa;  cap = FG_PESSOA_N;  meia = FG_MEIA_AFIN; break;
    case 't': arr = t->tema;    resto = t->restoTema;    cap = FG_TEMA_N;    meia = FG_MEIA_AFIN; break;
    case 'e': arr = t->decada;  resto = t->restoDecada;  cap = FG_DECADA_N;  meia = FG_MEIA_AFIN; break;
    case 'i': arr = t->idioma;  resto = t->restoIdioma;  cap = FG_IDIOMA_N;  meia = FG_MEIA_AFIN; break;
    default:  arr = t->duracao; resto = t->restoDuracao; cap = FG_DURACAO_N; meia = FG_MEIA_HORA; break;
  }
  if (pegaInt(&ls, le, &id) || !id || pegaInt(&ls, le, &peso) ||
      pegaInt(&ls, le, &n) || pegaInt(&ls, le, &dia)) return;
  s = afinSlot(arr, resto, cap, (int32_t)id,
               dia < 0 ? 0 : (dia > 0xffff ? 0xffff : (uint16_t)dia), meia);
  // O limite semantico do peso e +-8192 (32 em Q8.8); um arquivo corrompido
  // pode trazer qualquer int16 — prende na faixa logica.
  if (peso > FAROL_GOSTO_PESOMAX) peso = FAROL_GOSTO_PESOMAX;
  if (peso < -FAROL_GOSTO_PESOMAX) peso = -FAROL_GOSTO_PESOMAX;
  arr[s].peso = (int16_t)peso;
  arr[s].n = (uint16_t)(n < 0 ? 0 : (n > 0xffff ? 0xffff : n));
  arr[s].dia = (uint16_t)(dia < 0 ? 0 : (dia > 0xffff ? 0xffff : dia));
}

int farolgosto_ler(FarolGosto *g, const char *txt, size_t len) {
  FarolGosto t;
  const char *p, *e;
  int cab = 0;
  if (!g || !txt) return -1;
  farolgosto_iniciar(&t, 0);
  p = txt;
  e = txt + len;
  while (p < e) {
    const char *ls = p, *le;
    char k;
    while (p < e && *p != '\n') p++;
    le = p;
    if (p < e) p++;
    while (le > ls && (le[-1] == '\r' || le[-1] == ' ' || le[-1] == '\t')) le--;
    while (ls < le && (*ls == ' ' || *ls == '\t')) ls++;
    if (ls == le) continue;
    k = *ls++;
    if (!cab) {
      // A primeira linha nao vazia TEM de ser o cabecalho; versao
      // desconhecida -> o arquivo e ignorado por inteiro.
      int64_t v, perfil, titulos, diaBase;
      if (k != 'F' || ls >= le || *ls != 'G') return -1;
      ls++;
      if (pegaInt(&ls, le, &v) || v != FAROL_GOSTO_VERSAO ||
          pegaInt(&ls, le, &perfil) || pegaInt(&ls, le, &titulos) ||
          pegaInt(&ls, le, &diaBase)) return -1;
      t.perfil = (uint16_t)(perfil < 0 ? 0 : (perfil > 0xffff ? 0xffff : perfil));
      t.titulos = (uint32_t)(titulos < 0 || titulos > 0xffffffffLL
                             ? 0 : titulos);
      t.diaBase = (uint16_t)(diaBase < 0 ? 0 : (diaBase > 0xffff ? 0xffff : diaBase));
      cab = 1;
      continue;
    }
    switch (k) {
      case 'g': case 'p': case 't': case 'e': case 'i': case 'u':
        leAfin(&t, k, ls, le);
        break;
      case 'h': {
        int64_t f, v;
        int j;
        if (pegaInt(&ls, le, &f) || f < 0 || f >= FG_HORA_F) break;
        for (j = 0; j < FG_HORA_D; j++) {
          if (pegaInt(&ls, le, &v)) break;
          t.hora[f][j] = prende16(v);
        }
        break;
      }
      case 'r': {
        int64_t h;
        while (!pegaInt(&ls, le, &h))
          if (h > 0 && h <= 0xffffffffLL) {
            t.recusado[t.recProx % FG_RECUS_N] = (uint32_t)h;
            t.recProx++;
          }
        break;
      }
      case 'm': {
        int64_t h, vz, d;
        while (!pegaInt(&ls, le, &h) && !pegaInt(&ls, le, &vz) &&
               !pegaInt(&ls, le, &d)) {
          if (h > 0 && h <= 0xffffffffLL) {
            int s = t.mostProx % FG_MOSTR_N;
            t.mostrado[s].h = (uint32_t)h;
            t.mostrado[s].vezes = (uint8_t)(vz < 0 ? 0 : (vz > 0xff ? 0xff : vz));
            t.mostrado[s].dia = (uint8_t)(d < 0 ? 0 : (d > 0xff ? 0xff : d));
            t.mostProx++;
          }
        }
        break;
      }
      default: break;                       // linha desconhecida: pula
    }
  }
  if (!cab) return -1;
  *g = t;
  return 0;
}

int farolgosto_nome(uint16_t perfil, char *dst, size_t cap) {
  int w;
  if (!dst || !cap) return -1;
  w = snprintf(dst, cap, "farol-gosto-p%u.txt", perfil);
  return (w < 0 || (size_t)w >= cap) ? -1 : 0;
}
