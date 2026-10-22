// "OCULTAR NAO LANCADOS" (hideUnreleasedContent) PARA O QUE VEM DO CATALOGO.
//
// O ajuste existia e so a Biblioteca o lia (biblioteca.c). As fileiras da Home
// e a grade de uma colecao / "Ver tudo" mostravam um filme de 2099 mesmo com ele
// ligado (issue #369, item 1).
//
// A regra e a unica que o catalogo prova: o ANO que abre `CatItem.meta`
// ("2099", "2099  ·  120 min", "2099-") e MAIOR que o ano de agora. Sem ano no
// meta (muito addon nao manda releaseInfo) nao se esconde: nao ha como saber, e
// esconder faria sumir titulo bom. Titulo que estreia ainda neste ano tambem
// fica: o meta so tem o ano, nao a data.
#ifndef NV_NLANC_H
#define NV_NLANC_H
// 1 = o meta traz um ano de 4 digitos no inicio e ele e > anoAtual.
// So cabecalho: descoberta.c entra em muitos testes com lista fixa de fontes.
static inline int nlanc_meta_futuro(const char *meta, int anoAtual) {
  int ano = 0;
  if (!meta) return 0;
  for (int i = 0; i < 4; i++) {
    if (meta[i] < '0' || meta[i] > '9') return 0;
    ano = ano * 10 + (meta[i] - '0');
  }
  // "20245" nao e ano: o quinto digito invalida a leitura.
  if (meta[4] >= '0' && meta[4] <= '9') return 0;
  return ano > anoAtual;
}
#endif
