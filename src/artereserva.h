// RESERVA DE ARTE QUANDO O METAHUB NAO RESPONDE.
//
// POR QUE EXISTE: a arte do Cinemeta, da watchlist do Trakt e da biblioteca da
// conta e toda do images.metahub.space, deterministica pelo id do IMDb. Quando
// esse servico cai ou responde 404 (medido em 17-19/09/2026: timeouts de 20 s e
// "Artwork unavailable" em fileiras inteiras, relatos #67 e #55), a fileira
// fica cinza com o TMDB de pe ao lado — e o TMDB tem a MESMA imagem, pelo
// MESMO id, em /find?external_source=imdb_id.
//
// O QUE FAZ: dada a URL do metahub que falhou, devolve a URL equivalente no
// TMDB (poster w342, fundo w1280). Logo nao tem reserva: /find nao traz logo,
// e o endpoint /images e mais uma viagem por item que nao vale a pena aqui.
//
// QUEM CHAMA: tex_cache, no fio de decode, so DEPOIS de o download original
// falhar — a reserva nunca substitui o metahub quando ele responde, entao o
// cache de disco continua guardando a arte sob a URL original e nada mais no
// app precisa saber que a imagem veio de outro lugar.
#ifndef NV_ARTERESERVA_H
#define NV_ARTERESERVA_H
#include <stddef.h>

// 1 e `saida` preenchida quando `url` e do metahub, ha chave do TMDB e o /find
// achou o titulo com a imagem pedida. 0 em qualquer outro caso (URL de outro
// host, TMDB desligado, titulo desconhecido, logo).
int arte_reserva_url(const char *url, char *saida, size_t tam);

// EPISODIOS DO CINEMETA PARA O STILL DE RESERVA (One Piece, 23/09/2026): o
// corpo do /meta/series/<tt>.json. Guarda (temporada, episodio, `released`)
// de cada video de temporada > 0, para a reserva casar o episodio com o do
// TMDB pela data de exibicao e pelo numero absoluto quando a divisao em
// temporadas nao e a mesma — ver reservaStill em artereserva.c. Devolve
// quantos guardou. Copia; `corpo` pode ser liberado depois.
int arte_reserva_episodios(const char *imdb, const char *corpo);

// ARTE DE OUTRO HOST (#67, 20/09/2026): a biblioteca da Owlphibia vinha com
// cartazes de bingecat.com, que responde 404 — e a URL nao carrega o id do
// IMDb, entao a reserva acima nao tinha por onde procurar. O catalogo registra
// aqui, ao publicar, (url do cartaz/fundo -> imdb) para todo host que nao e o
// metahub nem o TMDB; a reserva consulta esta tabela quando a URL nao e do
// metahub. `poster` 1 = cartaz, 0 = fundo. Retorna 1 quando registrou ou
// atualizou a entrada; retorna 0 para entrada invalida ou quando o limite
// bounded da tabela/arena foi esgotado. O limite vale pela sessao do processo:
// nao ha reset da tabela, portanto URLs distintas acumuladas podem esgota-lo.
int arte_reserva_registrar(const char *url, const char *imdb, int poster);

// FUNDO DE UMA FONTE QUE O ITEM NAO TROUXE (ajuste "Background do hero").
//
// POR QUE EXISTE: o catalogo do Cinemeta manda `background` =
// images.metahub.space/background/medium/<tt>/img (conferido com curl em
// 22/09 no top de filmes) e nada do TMDB nem do Trakt. Sem a url dessas duas
// fontes no item, escolher "TMDB" ou "Trakt" nos Ajustes caia na arte
// automatica — a MESMA imagem — e o ajuste parecia quebrado.
//
// COMO: artehero.c devolve uma url VIRTUAL deterministica pelo id do IMDb,
//   https://nuvio.invalid/arte/<tmdb|trakt>/<tamanho>/<ttNNN>
// e tex_cache, no fio de rede, pede aqui a url real (TMDB /find ou Trakt
// /search/imdb) antes de baixar. O arquivo fica no cache de disco sob a url
// virtual: a consulta acontece uma vez por titulo e fonte, nao por quadro.
// `.invalid` (RFC 2606) garante que um caminho que nao passe por aqui morre
// no DNS em vez de pedir coisa a um host de verdade.
//
// `tamanho`: TMDB w780|w1280|original, Trakt medium|full. No Tizen o resolvedor
// rebaixa `original` para w1280 e `full` para medium qualquer que seja o pedido — ver
// fundoOriginal() em artehero.c (OOM do registro 1450).
//
// AS FONTES E O QUE VEM DEPOIS DO ID (23/09/2026):
//   tmdb/<tam>/<tt>[/m278|/t1399]    backdrop padrao; com o id do TMDB (o
//                                    `moviedb_id` do Cinemeta) pula o /find
//   tmdbalt/<tam>/<tt>[/m278]        OUTRO backdrop, sem texto, que nao e o
//                                    padrao (af_tmdb_fundos) — "destaque com
//                                    outra arte" com TMDB
//   trakt/<medium|full>/<tt>
//   apple/<1920|1280>/<tt>/<m|s>/<ano>/<titulo>   arte-chave da Apple TV
//   fanart/full/<tt>/<m|s>/<id tmdb>              so com a chave pessoal
//   anime/large/<tt|kitsu:N|mal:N|anilist:N>/<m|s>/<ano>/<titulo>
// O titulo vai codificado por af_codificar (artefontes.h).
#define ARTE_VIRTUAL_PREFIXO "https://nuvio.invalid/arte/"

// 0 = `url` nao e virtual (baixe como esta); 1 = `saida` tem a url real;
// -1 = virtual sem resposta (sem chave, titulo desconhecido, sem fundo): trate
// como download falho, para quem desenha cair na proxima fonte.
//
// Com MEMORIA em RAM (tabela fixa, LRU, thread-safe): a mesma fonte e titulo
// nao voltam a rede na sessao; "a API respondeu sem fundo" vale 5 min.
int arte_fonte_resolver(const char *url, char *saida, size_t tam);

// A url real de uma virtual JA RESOLVIDA nesta sessao, sem rede. 1 = `saida`
// preenchida; 0 = nao e virtual ou ainda nao se sabe.
int arte_fonte_resolvida(const char *url, char *saida, size_t tam);

// A Apple TV busca pelo titulo+ano (trailerapple.c) e devolve o MODELO da url
// do mzstatic ("...{w}x{h}.{f}"). 1 achou, 0 a Apple nao tem, -1 sem resposta.
// Registrado pelo main; sem registro, a fonte Apple nao existe.
void arte_fonte_definir_apple(int (*buscar)(const char *imdb, const char *titulo,
                                            int ano, int serie, char *modelo, size_t n));

// A chave PESSOAL do fanart.tv (Ajustes). "" = sem chave: a fonte nao existe.
// Copiada; nunca vai para log.
void arte_fonte_chave_fanart(const char *chave);

// MESMA IMAGEM? `tex_cache` registra a assinatura (FNV-1a 64 + tamanho) de
// todo corpo de imagem que baixa; arte_mesma_imagem responde 1 quando as duas
// urls (virtuais resolvidas para a real) sao a mesma url ou tem os mesmos
// bytes. 0 = diferentes OU nao se sabe ainda. Nao ve a mesma foto reencodada.
#include <stdint.h>
uint64_t arte_bytes_hash(const void *b, long n);
void arte_bytes_registrar(const char *url, const void *b, long n);
int  arte_mesma_imagem(const char *a, const char *b);

// So para teste: esvazia a memoria do resolvedor / troca o relogio (ms
// monotonico; NULL volta ao do sistema).
void arte_fonte_cache_limpar(void);
void arte_fonte_cache_relogio(unsigned long long (*ms)(void));

#endif
