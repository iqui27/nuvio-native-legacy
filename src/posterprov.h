// POSTERES PERSONALIZADOS: um provedor externo que devolve o cartaz JA PRONTO
// (arte sem texto + logo + notas + selos 4K/HDR/DV + faixa Top 10) por URL.
//
// Tres provedores conhecidos e um modelo livre:
//   SpatialPosters  <instancia>/api/poster/{movie|series}/{tt...|tmdb}?fmt=jpeg
//                   [&config=<token>]   (AGPL-3.0: so chamamos os endpoints
//                   HTTP dele, nao copiamos codigo. Instancia publica:
//                   https://spatial-posters.vercel.app)
//   RPDB            https://api.ratingposterdb.com/<chave>/imdb/poster-default/
//                   <tt...>.jpg?fallback=true
//   Modelo proprio  qualquer URL com {imdb} {tmdb} {type} {tipo_tmdb}
//
// SO CARTAZ RETRATO (2:3) DE CARD: home, biblioteca, busca, colecoes, "Mais
// como este". Nao entra em destaque nem em fundo deitado — nenhum dos tres
// provedores serve 16:9.
//
// FLUXO. Quem desenha o card chama posterprov_card(): com o ajuste desligado
// (padrao), ou sem id que o provedor entenda, devolve o `orig` sem tocar em
// nada. Ligado, devolve a URL do provedor e o resto (tex_cache, disco, decode)
// e o caminho de sempre. Se o tex_cache disser que aquela URL FALHOU (HTTP,
// prazo, decode), o item passa a usar o cartaz normal ate o fim da sessao —
// sem isto o card pediria de novo a cada quadro. Falhas seguidas demais abrem
// um DISJUNTOR de 5 min: instancia fora do ar nao pode custar 8 s de prazo por
// card de uma home de 200.
//
// CREDENCIAL. O token do SpatialPosters e a chave do RPDB moram so em
// posteres.txt (dados, por aparelho) e na URL pedida. Nunca no log:
// posterprov_redigir() e o unico jeito de imprimir uma URL de provedor.
#ifndef NV_POSTERPROV_H
#define NV_POSTERPROV_H
#include <stddef.h>

typedef enum {
  PP_DESLIGADO = 0,     // ordem = a de V_POSTER_PROV em ajustes.c e a gravada
  PP_SPATIAL,
  PP_RPDB,
  PP_MODELO
} PosterProv;

#define PP_URL_MAX      512    // o tex_cache guarda o caminho em 512 bytes
#define PP_INSTANCIA_MAX 128
#define PP_TOKEN_MAX    400    // a URL inteira tem de caber em PP_URL_MAX (512)
#define PP_EXTRA_MAX    120
#define PP_CHAVE_MAX     64
#define PP_MODELO_MAX   401    // 400 = TECLADO_LONGO: o modelo colado do celular
                               // passa de 299 (#361/#390); a URL montada segue em PP_URL_MAX
#define PP_INSTANCIA_PADRAO "https://spatial-posters.vercel.app"

typedef struct {
  int  prov;                          // PosterProv
  char instancia[PP_INSTANCIA_MAX];   // SpatialPosters: https://host[:porta]
  char token[PP_TOKEN_MAX + 1];       // SpatialPosters: token de configuracao
  char extra[PP_EXTRA_MAX + 1];       // SpatialPosters: "bs=vetro&side=right"
  char lang[8];                       // SpatialPosters: "pt" (idioma da interface)
  char chave[PP_CHAVE_MAX];           // RPDB
  char modelo[PP_MODELO_MAX];         // Modelo proprio
} PosterProvCfg;

// ---------------------------------------------------------------- entrada
// "spatial.exemplo.com", "http://nas:3000/", "stremio://x/c/tok/manifest.json"
// -> "https://spatial.exemplo.com" (sem barra final, sem caminho). Sem esquema
// vira https://, exceto host de rede local (IP/.local/localhost/porta), que
// vira http://. Vazio NAO e valido. 1 se valido (em `out`).
int posterprov_normalizar_instancia(const char *entrada, char *out, size_t n);

// Token de configuracao a partir do que a pessoa colou: o token puro, a URL do
// manifest (.../c/<token>/manifest.json, ou stremio://), a URL do configure
// (.../c/<token>/configure) ou uma URL de poster com ?config=<token>. Se a
// entrada trouxe um host, `inst` (opcional) recebe a instancia normalizada;
// senao fica vazio. Token so com [A-Za-z0-9_.~=-] e ate PP_TOKEN_MAX. Vazio ->
// 1 com `tok` vazio (apagar). 0 se nao e token valido.
int posterprov_extrair_token(const char *entrada, char *tok, size_t n,
                             char *inst, size_t ni);

// Parametros extras do SpatialPosters, a alternativa CURTA ao token (um token
// real tem 500+ caracteres e nao cabe na URL de 512 nem no teclado da TV):
// "bs=vetro&side=right&lang=pt". Aceita ?/& na frente, so [A-Za-z0-9_=&.,%-],
// e recusa fmt/format/config/c (o formato e nosso; o token tem campo proprio).
// 1 se valido (normalizado em `out`, vazio = apagar).
int posterprov_extra_normalizar(const char *entrada, char *out, size_t n);

// Modelo livre: http(s)://, so os marcadores {imdb} {tmdb} {type} {tipo_tmdb},
// pelo menos um deles. 1 se valido.
int posterprov_modelo_valido(const char *modelo);
// Para o que a pessoa DIGITA agora: valido e, no pior caso de cada marcador,
// a URL montada cabe em PP_URL_MAX — senao o modelo seria salvo e nunca
// montaria (#390). Nao vale para o que ja esta gravado em posteres.txt: ali
// so posterprov_modelo_valido(), e a montagem recusa a URL real grande demais.
int posterprov_modelo_cabe(const char *modelo);

// ---------------------------------------------------------------- montagem
// URL do cartaz de UM titulo. `imdb` e o id do item ("tt0111161", "tmdb:278",
// "tmdb:t1399" ou outro — canal etc.), `tmdb` o id numerico quando ja se sabe
// (0 = desconhecido), `tipo` o do catalogo ("movie"/"series"; "tv" tambem).
// 1 e `out` preenchido; 0 quando o provedor nao entende o item (id de canal,
// tipo que nao e filme/serie, provedor sem o dado que exige, URL maior que
// PP_URL_MAX). Funcao pura: nao le estado global.
int posterprov_montar_url(const PosterProvCfg *c, const char *imdb, long tmdb,
                          const char *tipo, char *out, size_t n);

// Sem credencial: "https://host/..." Para todo log e tela que citem a URL.
const char *posterprov_redigir(const char *url, char *out, size_t n);

// ---------------------------------------------------------------- estado
void posterprov_configurar(const PosterProvCfg *c);   // zera falhas e disjuntor
const PosterProvCfg *posterprov_cfg(void);
int  posterprov_ativo(void);                          // ligado E configurado

// Estado de uma URL no tex_cache: -1 falhou, 1 pronta, 0 pendente. Definido por
// tex_iniciar; sem ele nada e dado como falho.
void posterprov_hook_estado(int (*fn)(const char *url));
// Relogio em segundos, so para os testes do disjuntor.
void posterprov_hook_relogio(long (*fn)(void));

// A URL a desenhar. NUNCA devolve NULL se `orig` nao for NULL; o ponteiro vale
// enquanto a mesma identidade nao colidir no cache interno (256 posicoes), o
// bastante para "pedir a textura e o aspecto no mesmo trecho".
const char *posterprov_card(const char *imdb, long tmdb, const char *tipo,
                            const char *orig);
// PRECEDENCIA COM A ARTE DO ADDON (ajuste "Pôsteres do addon", desligado de
// fabrica). Ligado, o cartaz que o proprio addon mandou vence o provedor:
// `origem` e o CatItem.origem (vazio = nao veio de catalogo de addon: Trakt,
// Salvos, progresso) e `orig` o poster do item. O poster generico do Cinemeta
// (images.metahub.space, o mesmo para todo addon que so repassa o IMDb) NAO
// conta como arte do addon — ali o provedor continua. Nos outros casos, ou com
// o ajuste desligado, e posterprov_card() sem mudanca nenhuma.
void posterprov_preferir_addon(int sim);
int  posterprov_addon_vence(const char *origem, const char *orig);
const char *posterprov_card_addon(const char *origem, const char *imdb, long tmdb,
                                  const char *tipo, const char *orig);

// 1 quando `url` e uma URL montada pelo provedor ativo (para o portao de
// concorrencia e para redigir o log).
int posterprov_e_provedor(const char *url);

// PORTAO DE CONCORRENCIA. No maximo PP_SIMULT downloads do provedor ao mesmo
// tempo, mesmo com quatro fios de rede: uma home com 200 cards nao pode abrir
// 200 renders numa instancia Vercel gratuita. Bloqueante; chamar em par.
#define PP_SIMULT 3
void posterprov_portao_entrar(void);
void posterprov_portao_sair(void);

// Estatisticas para o diagnostico e os testes.
typedef struct { int falhas, seguidas, disjuntor_aberto; } PosterProvStat;
void posterprov_stat(PosterProvStat *s);

#endif
