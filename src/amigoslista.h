// A ORDEM E O CORTE DA ABA "AMIGOS" do painel (salvospainel.c): quem esta
// assistindo agora sobe na lista de amigos, e as sugestoes (gente que a pessoa
// talvez conheca, secundaria) mostram so as primeiras ate ela pedir mais.
#ifndef NV_AMIGOSLISTA_H
#define NV_AMIGOSLISTA_H

#define AMIGOSLISTA_SUG_CAP 3

// `vivo[i]` != 0 = o amigo i esta assistindo agora. Grava em `ordem` os
// indices: os vivos primeiro, cada metade na ordem de entrada.
static inline void amigoslista_ordem(const unsigned char *vivo, int n, int *ordem) {
  int k = 0, i;
  for (i = 0; i < n; i++) if (vivo[i]) ordem[k++] = i;
  for (i = 0; i < n; i++) if (!vivo[i]) ordem[k++] = i;
}

// Quantas das `n` sugestoes aparecem. Sobrando so uma, mostra tudo: "ver mais
// (1)" ocuparia a linha que a propria sugestao ocuparia.
static inline int amigoslista_sug_visiveis(int n, int aberto) {
  return (aberto || n <= AMIGOSLISTA_SUG_CAP + 1) ? n : AMIGOSLISTA_SUG_CAP;
}

#endif
