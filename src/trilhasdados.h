// TRILHAS curadas (2.1): dados locais, nao a tela. Ver trilhas.h.
//
// FONTES DE VERDADE. O catalogo e o servidor/recomendacoes/src/trilhas-dados.js
// versionado (Wikidata CC0; tools/trilhas-wikidata.py --local), e o progresso
// e o historico da conta + a conclusao local (conquistasdados.c). Nenhum passo
// nasce de fixture: sem titulo/ano no catalogo a linha nao existe, e sem
// historico o passo nao e marcado.
#ifndef NV_TRILHASDADOS_H
#define NV_TRILHASDADOS_H

#define CQ_TRILHA_MAX 128

typedef struct { const char *titulo, *imdb; long tmdb; int ano; } CqPasso;
typedef struct { const char *id, *nome, *lema; int n; const CqPasso *p; } CqTrilha;

int cqd_trilha_n(void);
const CqTrilha *cqd_trilha(int i);
const CqTrilha *cqd_trilha_buscar(const char *chave);
// Passos vistos segundo historico/progresso real. Chamada direta; sem fio.
int cqd_trilha_vistos(const CqTrilha *t, unsigned char *visto, int max);
// Indice do proximo passo nao visto, ou -1 quando acabou.
int cqd_trilha_proximo(const CqTrilha *t, unsigned char *visto, int n);
// Trilhas que contem este titulo; devolve ate `max`.
int cqd_trilhas_de(const char *imdb, const char *tipo, const CqTrilha **dst, int max);

#endif
