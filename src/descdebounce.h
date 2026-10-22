// Debounce das remontagens da Home (descoberta). Medido (2.0.2, 3 pessoas com
// ~579 colecoes): ate 29 voltas em 2 min, cada uma condenada pela seguinte
// ("montagem interrompida ... recomecando ja"), a home nunca assentava e caia
// a 35-40 fps. Regra: no maximo UMA volta nova a cada NV_DESC_MIN_MS; pedidos
// no intervalo viram UM agendamento (coalescido) e nao condenam a volta no ar.
#ifndef NV_DESCDEBOUNCE_H
#define NV_DESCDEBOUNCE_H

// Testes de montagem anteriores ao debounce compilam com -DNV_DESC_MIN_MS=0ull
// (comportamento antigo, sem espera); a regra em si e conferida em descdebounce.c.
#ifndef NV_DESC_MIN_MS
#define NV_DESC_MIN_MS 10000ull
#endif

typedef struct {
  unsigned long long inicio;   // quando a ultima volta comecou
  int iniciou;                 // ja houve alguma volta
  int adiado;                  // ha um inicio agendado
} NvDescDeb;

// Volta no ar pode ser condenada por um pedido novo? So se ja passou o minimo.
static inline int nv_desc_pode_condenar(const NvDescDeb *d, unsigned long long agora) {
  return !d->iniciou || agora - d->inicio >= NV_DESC_MIN_MS;
}

// Pedido de iniciar. 0 = inicie agora; >0 = agende para daqui a esse tanto de
// ms (marca adiado); -1 = ja ha agendamento, este pedido foi coalescido.
static inline long nv_desc_pedido(NvDescDeb *d, unsigned long long agora) {
  unsigned long long dec;
  if (!d->iniciou || agora - d->inicio >= NV_DESC_MIN_MS) return 0;
  if (d->adiado) return -1;
  d->adiado = 1;
  dec = agora - d->inicio;
  return (long)(NV_DESC_MIN_MS - dec);
}

static inline void nv_desc_iniciou(NvDescDeb *d, unsigned long long agora) {
  d->inicio = agora; d->iniciou = 1; d->adiado = 0;
}

#endif
