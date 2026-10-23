// Sonda do cabecalho do MKV que falhou (0 faixas lidas / Range recusado).
// Medido na C9 (remux DV de 108 Mbps): a sonda unica disparou 2 s dentro do
// buffering, a TV ficou ~10 s sem conseguir abrir conexao nenhuma (curl 28 ate
// nas legendas), a sonda leu 0 faixas e o DV nunca mais foi tentado nessa fonte:
// tocou HDR10. Um minuto depois a rede estava normal. Falha de sonda nao e
// "nao e MKV": tenta de novo com espera crescente e so desiste depois de
// NV_DVSONDA_MAX falhas. A decisao e pura; quem agenda e le o relogio e o video.c.
#ifndef NV_VIDEO_DVRETRY_H
#define NV_VIDEO_DVRETRY_H

#define NV_DVSONDA_MAX 3

typedef struct { int falhas; int desistiu; } NvDvSonda;

// Espera (ms) antes da nova tentativa n (1-based): 3 s, 8 s, 20 s.
static inline unsigned nv_dvsonda_espera_ms(int n) {
  return n <= 1 ? 3000u : n == 2 ? 8000u : 20000u;
}

// Registra uma falha. Devolve a espera ate a proxima tentativa, ou 0 se esgotou
// (desistiu = 1: a fonte segue no player nativo, como antes). `*tentativa`
// recebe o numero da nova tentativa (1..NV_DVSONDA_MAX) para o log.
static inline unsigned nv_dvsonda_falhou(NvDvSonda *s, int *tentativa) {
  if (s->falhas >= NV_DVSONDA_MAX) { s->desistiu = 1; return 0; }
  s->falhas++;
  if (tentativa) *tentativa = s->falhas;
  return nv_dvsonda_espera_ms(s->falhas);
}

// Pode disparar agora? So com o prazo vencido E fora de buffering: disparar
// dentro do buffering foi exatamente o que derrubou as conexoes da TV.
static inline int nv_dvsonda_pronta(int prazoVencido, int bufferando) {
  return prazoVencido && !bufferando;
}

static inline void nv_dvsonda_zerar(NvDvSonda *s) { s->falhas = 0; s->desistiu = 0; }

// URL de arquivo .mp4 (o caminho, sem query nem fragmento): MP4 nao tem Tracks de
// Matroska. Cobre o trailer do IMDb, tocado sem que video_definir_mp4 seja
// chamado (C9: a sonda e o retry disparavam nele em segundo plano).
static inline int nv_url_e_mp4(const char *u) {
  size_t n = 0;
  if (!u) return 0;
  while (u[n] && u[n] != '?' && u[n] != '#') n++;
  return n >= 4 && (u[n-4] == '.') && (u[n-3] | 32) == 'm' && (u[n-2] | 32) == 'p' && u[n-1] == '4';
}

#endif
