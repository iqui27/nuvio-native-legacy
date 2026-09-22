// O que as ABAS da tela de titulo mostram alem do elenco: nota do Trakt,
// comentarios do Trakt e titulos relacionados.
//
// Tudo isto ja existe no app web (renderExternalRatingsRow, a secao de
// comentarios e a aba "Mais como este"), e nenhum dos tres tinha fonte no port
// — por isso as abas caiam em "Sem informacao". Todos os pedidos falam com a
// api.trakt.tv por IMDb id, sem traducao de identificador no meio: o Trakt
// resolve "tt1234567" direto, e o TMDB nao.
#ifndef NV_EXTRAS_H
#define NV_EXTRAS_H

#define EX_COMENT_MAX 8
#define EX_REL_MAX   12

// Pede tudo de um titulo. Nao bloqueia: dispara um fio. Repetir com o mesmo
// `imdb` nao refaz o pedido. `serie` decide entre /shows e /movies.
// `tmdbId` e o id do titulo no TMDB (0 quando nao se sabe). Serve so a COLECAO,
// que o TMDB expoe apenas por id proprio — nao ha caminho por IMDb.
void extras_pedir(const char *imdb, int serie, long tmdbId);

// Nota do Trakt em 0..100 (0 = ainda nao chegou ou nao existe) e quantos
// votaram. O web mostra a mesma nota que o mdbList devolve para "trakt".
int  extras_nota_trakt(void);
int  extras_votos_trakt(void);

// FONTES DE NOTA, na ordem em que o web as lista (renderExternalRatingsRow,
// metaDetailsScreen.js:3410). Todas menos IMDb e Trakt vem do mdbList, que
// precisa da chave do dono em art/mdblist.txt; sem o arquivo elas ficam em 0 e
// a fileira mostra so as duas que temos por conta propria.
typedef enum {
  EX_TRAKT, EX_IMDB, EX_TMDB, EX_TOMATOES, EX_AUDIENCE, EX_METACRITIC,
  EX_LETTERBOXD, EX_NFONTES
} ExFonte;

// Le art/mdblist.txt. Sem ele o modulo funciona com Trakt e IMDb apenas.
void extras_carregar(const char *dirArte);

// 1 quando existe chave do mdbList (conta ou art/mdblist.txt). A tela de
// Ajustes mostra so o estado — os caracteres nunca saem deste modulo.
int  extras_mdblist_tem_chave(void);

// Chave do mdblist vinda da CONTA. Mesmo motivo do TMDB: enquanto sair de
// art/mdblist.txt (que esta ate com modo 0600), o pacote distribui a chave de
// quem o montou.
void extras_definir_chave(const char *chave);

// Nota da fonte, CRUA multiplicada por 10 (o imdb vem com uma casa decimal e
// precisa caber em inteiro). 0 = nao ha. Divida por 10 e use
// extras_fonte_percentual() para saber se o resultado e "6.2" ou "66%".
int  extras_nota(int fonte);
int  extras_fonte_percentual(int fonte);
const char *extras_fonte_marca(int fonte);
// Caminho ABSOLUTO do arquivo de marca. Ver a nota em extras.c: relativo nao
// funciona porque o diretorio de trabalho do app nao e a pasta da arte.
const char *extras_caminho_marca(int fonte);
// Marca por NOME de arquivo, para as que nao sao fonte de nota (o wordmark do
// Trakt, "trakt_wordmark").
const char *extras_caminho_marca_nome(const char *nome);

// Comentarios: os mais curtidos primeiro, que e a ordem de `comments/likes`.
int  extras_n_comentarios(void);
const char *extras_comentario_usuario(int i);
const char *extras_comentario_texto(int i);
int  extras_comentario_curtidas(int i);
// Nota de QUEM COMENTOU (user_rating do Trakt), 0..10; 0 quando nao avaliou.
// A referencia mostra "10/10  17 curtidas" no rodape do cartao.
int  extras_comentario_nota(int i);

// COMENTARIOS DO EPISODIO, para o seletor "Série | Episódio" que a referencia
// mostra acima dos cartoes.
//
// Sao uma consulta SEPARADA no Trakt
// (/shows/<id>/seasons/<t>/episodes/<e>/comments/likes), nao um recorte da
// lista da serie — os dois conjuntos nao se cruzam. Feita SOB DEMANDA: uma
// viagem por episodio, e so quando o dono escolhe a aba "Episódio".
//
// `imdbSerie` aceita tanto "tt1234567" quanto o "tt1234567:2:4" que a lista de
// episodios usa; a parte depois do primeiro ':' e ignorada.
void extras_pedir_comentarios_ep(const char *imdbSerie, int temporada, int episodio);
int  extras_n_comentarios_ep(void);
// 1 enquanto a consulta esta em voo. Quem desenha usa isto para mostrar
// "carregando" em vez de "nao ha comentarios" — os dois estados sao a lista
// vazia, e confundi-los faz o episodio parecer sem comentario nenhum.
int  extras_comentarios_ep_carregando(void);

// 1 enquanto a busca de extras esta no ar. Quem desenha usa para mostrar
// esqueleto em vez de secao vazia.
int  extras_carregando(void);
const char *extras_comentario_ep_usuario(int i);
const char *extras_comentario_ep_texto(int i);
int  extras_comentario_ep_curtidas(int i);
int  extras_comentario_ep_nota(int i);

// NOTAS POR EPISODIO, para o painel que o web mostra em SERIE no lugar dos
// cartoes (renderSeriesRatingsPanel, metaDetailsScreen.js:3843): uma fileira de
// temporadas e uma grade de pastilhas "E<n> / nota", coloridas por faixa.
//
// Vem de UMA chamada: /shows/<id>/seasons?extended=episodes,full devolve todas
// as temporadas com a nota de cada episodio junto. Pedir episodio a episodio
// seriam dezenas de chamadas para desenhar uma aba.
// 64 e nao 12: em par com CAT_TEMP_MAX (#79) — a aba de notas cortava South
// Park na T12 tambem.
#define EX_TEMP_MAX 64
#define EX_EP_MAX   30
int  extras_n_temporadas(void);
int  extras_temporada_numero(int t);
int  extras_n_eps(int t);
int  extras_ep_numero(int t, int i);
// Nota do episodio em DECIMOS (72 = 7.2); 0 = sem nota.
int  extras_ep_nota(int t, int i);

// COLECAO (franquia) do filme, para a aba que o web chama pelo nome dela.
// Vem de /movie/<id> -> belongs_to_collection -> /collection/<id>. As partes
// trazem so o id do TMDB, entao abrir uma delas passa pelo mesmo caminho do
// credito de um ator (desc_pedir_titulo_tmdb).
#define EX_COL_MAX 12
const char *extras_colecao_nome(void);
int  extras_n_colecao(void);
const char *extras_colecao_titulo(int i);
const char *extras_colecao_ano(int i);
long extras_colecao_tmdb(int i);

// PRODUTORAS E REDES, para a fileira de logos da pagina de detalhe — no web e
// o renderCompanySections ("Production"/"Network"). Vem do corpo principal
// do /movie|tv/<id>: nenhuma viagem a mais. `rede` distingue NETWORK (so
// serie) de COMPANY, que e o tipo da fonte TMDB que o OK abre no vertudo.
// logo e URL absoluta (image.tmdb.org w185) e fica "" quando o logo e .svg —
// o pacote nao decoda svg; quem desenha cai no nome.
int         extras_n_estudios(void);
// IDIOMA ORIGINAL do titulo ("it", "ja", "en"...), ou "" quando nao se sabe.
// Sai do `original_language` do TMDB, no mesmo corpo que a ficha ja baixa.
// Serve ao ajuste "Áudio: Original" — ver linguas.h.
const char *extras_idioma_original(void);

const char *extras_estudio_nome(int i);
const char *extras_estudio_logo(int i);
long        extras_estudio_tmdb(int i);
int         extras_estudio_rede(int i);

// EPISODIOS JA ASSISTIDOS, do Trakt (/shows/<id>/progress/watched). O card do
// episodio ganha uma mascara e um check quando o dono ja viu. Sem isto, quem
// acompanha uma serie nao tinha como saber onde parou olhando a lista.
int  extras_ep_visto(int temporada, int episodio);
// 1 apenas depois de receber o historico desta obra. Sem resposta nao inferir
// que todos os episodios estao por assistir.
int extras_progresso_pronto(void);
int extras_proximo_episodio(int *temporada, int *episodio);
// QUANTO DA SERIE JA FOI VISTO. Os dois contadores vem do TOPO da mesma
// resposta de /progress/watched que ja e baixada — zero pedido a mais. Devolve
// 0, sem escrever nada, enquanto o historico nao chegou ou quando a serie nao
// tem episodio exibido.
int extras_progresso_serie(int *vistosEp, int *exibidos);

// FICHA TECNICA do filme, para a secao "Detalhes do Filme".
//
// Tudo isto sai da MESMA chamada /movie/<id> que a colecao ja fazia — o corpo
// trazia status, runtime, release_date e os paises desde sempre, e o parse lia
// so belongs_to_collection e jogava o resto fora. Com
// `append_to_response=release_dates,videos` a mesma viagem passa a trazer
// tambem a classificacao etaria e os trailers. Nenhum pedido de rede novo.
//
// Vazio ("" ou 0) quando ainda nao chegou ou o TMDB nao tem o campo. Quem
// desenha deve OMITIR a linha nesse caso, nunca escrever um valor de reserva:
// ver a nota sobre a classificacao cravada em descoberta.c.
const char *extras_ficha_status(void);          // "Released", "Post Production"
int         extras_ficha_duracao(void);         // minutos; 0 = nao ha
const char *extras_ficha_paises(void);          // "United States of America, Canada"
const char *extras_ficha_classificacao(void);   // "R", "PG-13", "14"
const char *extras_ficha_lancamento(void);      // "2026-01-15"

// TRAILERS. O id do YouTube, nome e miniatura saem do /movie/<id> do TMDB
// (append_to_response=videos). O card e focavel e OK abre o video no app
// nativo da plataforma: navegador do webOS (luna-send), aba do Tizen
// (window.open) ou browser do desktop (open).
#define EX_TRAILER_MAX 6
int         extras_n_trailers(void);
const char *extras_trailer_yt(int i);        // id do video ("dQw4w9WgXcQ")
const char *extras_trailer_nome(int i);      // "Official Trailer"
// URL da miniatura. Previsivel a partir do id, sem chamada de API — e o mesmo
// caminho que o web usa (metaDetailsScreen.js:5728). Passe direto a tex_obter:
// o cache de texturas baixa e guarda qualquer URL sozinho.
const char *extras_trailer_miniatura(int i);
// Abre o trailer no app nativo (browser/YouTube). Sem retorno.
void        extras_trailer_abrir(int i);

// Busca somente `videos` do TMDB para o trailer do hero. Esta fila e separada
// dos extras da pagina de detalhe: trocar o destaque nao limpa creditos,
// ficha ou trailers que outra tela ainda esta desenhando. O pedido e
// single-flight por IMDb e tem geracao, portanto uma resposta de um destaque
// antigo nunca e publicada no titulo novo.
void        extras_hero_trailer_pedir(const char *imdb, int serie, long tmdbId);
// Copia um id valido para `dst` somente quando a resposta completa pertence
// ao IMDb pedido. O snapshot sob trava evita que home combine n e ponteiro de
// duas geracoes durante uma troca A -> B -> A.
int         extras_hero_trailer_obter(const char *imdb, char *dst, unsigned cap);
#ifdef NUVIO_TRAILER_TEST
int         extras_hero_trailer_parse(const char *json, char *dst, unsigned cap);
#endif

// AGENDA DA SERIE — quando sai o proximo episodio, e a situacao da serie.
//
// Sai do MESMO corpo /tv/<id> que ja trazia redes, "mais como este" e a lista
// de temporadas: `status`, `next_episode_to_air` e `last_episode_to_air`
// sempre estiveram la e eram descartados. NENHUM PEDIDO DE REDE NOVO na
// pagina de titulo — a mesma conta da ficha tecnica do filme, acima.
//
// Vazio quando nao chegou ou quando o TMDB nao tem o campo, e ai quem desenha
// OMITE a linha. Serie encerrada devolve `status` com data vazia: e o caso em
// que a interface diz "Série encerrada" em vez de uma data inventada.
//
// Estes campos tambem sao entregues a agenda.c, que os guarda por perfil — os
// acessores aqui existem para o teste e para quem quiser o dado cru da visita
// em curso.
const char *extras_agenda_status(void);       // "Returning Series", "Ended"
const char *extras_agenda_data(void);         // "2026-09-18"; "" = nao ha
const char *extras_agenda_data_ultimo(void);  // ultimo episodio que foi ao ar
const char *extras_agenda_nome_ep(void);
int         extras_agenda_temporada(void);
int         extras_agenda_episodio(void);

// Titulos relacionados, para a aba "Mais como este".
int  extras_n_relacionados(void);
const char *extras_relacionado_titulo(int i);
const char *extras_relacionado_ano(int i);
const char *extras_relacionado_imdb(int i);
// Poster do relacionado (URL). Vem de `extended=images` do Trakt, que devolve o
// caminho sem esquema — o https e acrescentado aqui.
const char *extras_relacionado_poster(int i);

#endif
