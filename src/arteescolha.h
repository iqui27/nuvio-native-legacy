// A ARTE QUE A PESSOA ESCOLHEU A MAO, por titulo e por perfil (#142).
//
// POR QUE EXISTE: a politica automatica (artehero.h) acerta quase sempre, mas
// "quase" tem cara: o backdrop padrao do TMDB com o nome gigante do filme atras
// do nosso logo, a foto escura que apaga a Cor viva, o logo em polones que o
// TMDB marcou como sem idioma. Nenhuma regra conserta o gosto de quem esta no
// sofa — a tela "Trocar arte" da pagina do titulo (trocaarte.h) deixa a pessoa
// apontar a foto e o logo, e este modulo LEMBRA a escolha.
//
// O QUE GUARDA: id do titulo -> url do fundo e url do logo, cada uma opcional.
// Url vazia = "Automatico" (a regra de sempre). Titulo com as duas vazias sai
// da tabela.
//
// A CHAVE E O TITULO, NAO O EPISODIO: "tt0903747:2:3" vira "tt0903747" — o
// fundo escolhido vale para a serie inteira, inclusive o destaque de
// Continuar assistindo (que e um episodio). Ids de addon de anime
// ("kitsu:7442:3") cortam no SEGUNDO ':' pelo mesmo motivo.
//
// UM ARQUIVO POR PERFIL (arte-escolhida-p<N>.txt), como fontepref-p<N>.txt e
// fileirasui-p<N>.txt: a foto que o pai escolheu nao e a do filho. Apagado no
// logout (sync_esquecer_usuario), junto de salvos e da fonte lembrada.
//
// CUSTO NO QUADRO: a home pergunta por titulo desenhado, varias vezes por
// quadro (artehero_url_destaque, o logo do card). Tabela vazia devolve na
// primeira linha; com escolhas, um hash FNV e um strcmp — nunca uma varredura.
// Sem SDL e sem rede: so dados.h, para o teste compilar sozinho.
#ifndef NV_ARTEESCOLHA_H
#define NV_ARTEESCOLHA_H
#include <stddef.h>

// Teto da tabela. 160 titulos x ~800 bytes = ~128 KB estaticos — a Samsung ja
// bateu no heap de 256 MiB do WASM, e ninguem escolhe arte de 160 titulos a
// mao. Cheia, a escolha nova toma o lugar da MAIS ANTIGA.
#define ARTEESC_MAX 160
// Url cabe com folga: TMDB ~70, virtual da Apple com titulo codificado ~250.
#define ARTEESC_URL 384

// Le o arquivo do perfil corrente (uma vez; as consultas chamam sozinhas).
void arteesc_iniciar(void);
// Troca de perfil: solta a tabela; o arquivo do perfil novo e lido na proxima
// consulta. Mesma forma de fontepref_definir_perfil.
void arteesc_definir_perfil(int perfil);
// Apaga o arquivo de TODOS os perfis e esvazia a tabela (logout).
void arteesc_esquecer(void);

// "tt123:2:4" -> "tt123"; "kitsu:7442:3" -> "kitsu:7442". `dst` vazio quando
// o id nao serve de chave.
void arteesc_chave(const char *id, char *dst, size_t n);

// A escolha deste titulo, ou NULL quando e Automatico. `id` pode vir com
// episodio. O ponteiro vale ate a proxima escrita na tabela.
const char *arteesc_fundo(const char *id);
const char *arteesc_logo(const char *id);

// Grava a escolha e o arquivo. `url` NULL ou "" = volta ao Automatico.
// Devolve 1 quando a tabela mudou.
int arteesc_definir_fundo(const char *id, const char *url);
int arteesc_definir_logo(const char *id, const char *url);

int arteesc_n(void);

#endif
