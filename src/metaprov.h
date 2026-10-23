// Provedor de metadados: catalogo oficial do Nuvio primeiro, Cinemeta so como
// reserva.
//
// O Cinemeta (v3-cinemeta.strem.io) e lento, estoura o tempo e so fala ingles.
// O catalogo do Nuvio (catalog.nuvio.tv, id com.nuvio.tmdb.catalogs) responde
// no MESMO formato Stremio (/meta/<tipo>/<tt>.json, /catalog/.../search=<q>.json)
// e e localizado por um segmento de caminho com a configuracao em JSON:
//   https://catalog.nuvio.tv/%7B%22language%22%3A%22pt-BR%22%7D/meta/movie/tt....json
//
// Este modulo so monta URLs e decide o fallback; quem baixa e um callback
// (rede_baixar_st por padrao), para o teste trocar a rede por respostas falsas.
// Sem __thread, sem extensoes GNU: compila em Emscripten e nas Tizen 4/5.
#ifndef METAPROV_H
#define METAPROV_H

#include <stddef.h>

enum { METAPROV_NUVIO = 0, METAPROV_CINEMETA = 1 };

#define METAPROV_NUVIO_HOST   "https://catalog.nuvio.tv"
#define METAPROV_CINEMETA_URL "https://v3-cinemeta.strem.io"
#define METAPROV_NUVIO_S      5     // prazo do catalogo do Nuvio, em segundos
#define METAPROV_PAUSA_S      60    // apos falha de REDE, o Nuvio descansa

// Baixa `url` em `seg` segundos. Devolve o corpo (malloc) SO para 2xx, senao
// NULL; *st recebe o codigo HTTP (0 = sem rede/estouro de tempo).
typedef char *(*MetaprovGet)(const char *url, int seg, int *st, void *ctx);

// Codigo TMDB do idioma dos metadados ("pt-BR"...; "en-US" se vazio/invalido).
const char *metaprov_idioma(void);

// "https://catalog.nuvio.tv/<JSON {"language":"<lang>"} escapado>".
void metaprov_base_nuvio(char *dst, size_t n, const char *lang);

// URL de /meta (prov NUVIO usa `lang`; CINEMETA ignora). tipo "movie"|"series".
void metaprov_url_meta(char *dst, size_t n, int prov, const char *tipo,
                       const char *id, const char *lang);
// URL de busca. `termo` e escapado aqui (nunca vai cru para o caminho).
void metaprov_url_busca(char *dst, size_t n, int prov, const char *tipo,
                        const char *termo, const char *lang);

// Chave de cache em memoria: provedor + idioma + tipo + id. Trocar o idioma
// muda a chave e refaz o pedido; o TIPO continua na chave (filme e serie de
// mesmo id sao titulos diferentes).
void metaprov_chave(char *dst, size_t n, const char *tipo, const char *id);

// O corpo e um /meta aproveitavel? ({"meta":{...nao vazio, com "name"...}}).
int metaprov_meta_valido(const char *corpo);
// O corpo e uma busca valida? (tem o array "metas"; vazio e valido).
int metaprov_busca_valida(const char *corpo);

// /meta com reserva. `seg_cine` = prazo do Cinemeta (o do chamador de sempre).
// *prov (opcional) diz quem respondeu; NULL em corpo = ninguem.
char *metaprov_meta_com(const char *tipo, const char *id, int seg_cine,
                        MetaprovGet get, void *ctx, int *prov);
char *metaprov_meta(const char *tipo, const char *id, int seg_cine, int *prov);
// So o Cinemeta (sem passar pelo catalogo do Nuvio); NULL se falhar ou vier
// sem ficha valida. Para quem precisa conferir o que o Nuvio nao listou.
char *metaprov_meta_cinemeta_com(const char *tipo, const char *id, int seg,
                                 MetaprovGet get, void *ctx);

// Busca com reserva: Cinemeta so quando o Nuvio FALHA (rede/HTTP/JSON
// invalido); resposta valida e vazia nao cai para o Cinemeta.
char *metaprov_busca_com(const char *tipo, const char *termo, int seg_nuvio,
                         int seg_cine, MetaprovGet get, void *ctx, int *prov);

// Duracao no formato da casa. O catalogo do Nuvio manda "148m" (filme) e
// "25min" (episodio); o Cinemeta, "148 min". Numero puro seguido de m/min, com
// ou sem espaco, vira "N min"; qualquer outra coisa fica como veio.
void metaprov_duracao(char *s, size_t n);

// Callback padrao (rede_baixar_st sem cabecalhos).
char *metaprov_get_rede(const char *url, int seg, int *st, void *ctx);

// Memoria do "Nuvio fora do ar" (so p/ teste: zera a pausa).
void metaprov_zerar_pausa(void);

#endif
