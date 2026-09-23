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
// `tamanho`: TMDB w1280|original, Trakt medium|full. No Tizen o resolvedor
// rebaixa `original` para w1280 e `full` para medium qualquer que seja o pedido — ver
// fundoOriginal() em artehero.c (OOM do registro 1450).
#define ARTE_VIRTUAL_PREFIXO "https://nuvio.invalid/arte/"

// 0 = `url` nao e virtual (baixe como esta); 1 = `saida` tem a url real;
// -1 = virtual sem resposta (sem chave, titulo desconhecido, sem fundo): trate
// como download falho, para quem desenha cair na proxima fonte.
int arte_fonte_resolver(const char *url, char *saida, size_t tam);

#endif
