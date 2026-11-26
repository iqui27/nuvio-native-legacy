// MEDIA POR TEMPORADA do cartao "Notas por episodio" (NuvioMedia/NuvioTV#1689:
// "season averages", para achar de relance a temporada mais forte e a mais
// fraca quando as notas dos episodios sao parecidas). Puro: so a conta; o
// desenho e de notasui_mapa_card.
//
// Notas em DECIMOS (83 = 8,3), 0 = sem nota. Episodio sem nota nao entra na
// media: nada e inventado.
#ifndef NV_NOTASMEDIA_H
#define NV_NOTASMEDIA_H

// Minimo de episodios com nota para a temporada ter media. Com um so, a
// "media" seria a nota dele dita com outro nome.
#define NM_MIN_EPS 2

// Media da temporada, arredondada ao decimo; 0 = sem media. `comNota` (pode
// ser NULL) recebe quantos episodios tinham nota.
int nm_media(const int *notas, int n, int *comNota);

// Indices da melhor e da pior temporada entre as `nt` medias (0 = sem media).
// Empate fica com a primeira. -1 nos dois quando ha menos de duas temporadas
// com media, ou quando todas empatam: nao ha o que comparar.
void nm_extremos(const int *medias, int nt, int *melhor, int *pior);

#endif
