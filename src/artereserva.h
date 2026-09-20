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
// metahub. `poster` 1 = cartaz, 0 = fundo.
void arte_reserva_registrar(const char *url, const char *imdb, int poster);

#endif
