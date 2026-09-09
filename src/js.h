// Leitura tolerante de JSON, compartilhada.
//
// Nao e um analisador completo e nao pretende ser: os formatos daqui (Stremio,
// Cinemeta, Trakt) sao conhecidos e rasos, e o que importa e NUNCA travar com
// campo faltando ou tipo inesperado. Cada funcao devolve o que achou ou nada, e
// quem chama decide. Esta logica nasceu duplicada em addons.c e video.c; virou
// modulo quando o terceiro consumidor apareceu.
#ifndef NV_JS_H
#define NV_JS_H
#include <stddef.h>

// Fim do objeto/array que comeca em `p` (que aponta para '{' ou '['),
// respeitando aspas e escapes.
const char *js_fim(const char *p);

// Valor textual de "chave" dentro de [ini,fim). 1 se achou. Escapes \uXXXX
// viram espaco de proposito: os textos vem cheios de emoji e sao so para
// exibicao — decodificar UTF-16 aqui seria trabalho sem retorno.
int js_texto(const char *ini, const char *fim, const char *chave,
             char *dst, size_t tam);

// Numero de "chave". Exige que o caractere apos a chave seja digito/sinal, o
// que evita casar com um OBJETO de mesmo nome — o caso real e
// {"currentTime":{"currentTime":8580}}, onde a primeira ocorrencia da 0.
double js_num(const char *ini, const char *fim, const char *chave, double padrao);

// Primeiro elemento do array de nome `chave`; NULL se nao houver. Avance com
// js_prox.
const char *js_array(const char *ini, const char *fim, const char *chave);

// Proximo elemento do array a partir do fim do anterior; NULL no fim.
const char *js_prox(const char *fimAnterior);

// Primeiro elemento de um array que e a RAIZ do documento. Toda RPC do
// Supabase responde `[{...},{...}]` sem chave em volta, e js_array — que
// procura por nome — nao tem o que procurar ali. Avance com js_prox.
const char *js_raiz_array(const char *corpo);

// Copia o valor de `chave` como TEXTO JSON CRU, com as chaves e colchetes.
// Existe para o `credential_json` das credenciais: o app repassa aquele objeto
// ao servidor sem interpretar, e reconstrui-lo campo a campo perderia tudo que
// esta versao do app nao conhece. 1 se achou e coube.
int js_bruto(const char *ini, const char *fim, const char *chave,
             char *dst, size_t tam);

// Instante ISO-8601 de dentro de um JSON, em ms desde a epoca. 0 quando nao da
// para ler. Aceita "2026-09-04T18:52:07.123456+00:00", o mesmo com "Z", e
// "2026-09-04 18:52:07".
//
// SO UTC, e de proposito: e o que o Postgres e o Trakt devolvem, e o aparelho
// pode estar com o relogio certo e o fuso errado. O que importa e comparar
// instantes entre si, nao mostrar hora local.
//
// Nasceu private em syncprog.c (last_watched/updated_at). Virou compartilhada
// quando o segundo consumidor apareceu: o `paused_at` do /sync/playback do
// Trakt, que e o que ordena a fileira "Continuar assistindo" quando as duas
// fontes falam da mesma obra. Duas copias de um parser de data divergem — e
// divergem em silencio, porque o sintoma e uma ORDEM errada, nao um erro.
long long js_ms_iso(const char *s);

// Valor textual de uma chave da RAIZ do documento, ignorando as internas.
//
// POR QUE NAO BASTA js_texto. Ele varre o texto e para na PRIMEIRA ocorrencia
// da chave, o que e certo nos documentos rasos para os quais nasceu e errado
// sempre que a mesma chave existe aninhada. Dois casos reais, os dois com
// sintoma silencioso:
//   - manifesto Stremio: ha "id" dentro de catalogs[] e de behaviorHints. Se o
//     autor do addon poe "catalogs" antes de "id", a leitura crua devolve o id
//     de um CATALOGO como se fosse o do addon — e a partir dai
//     addons_base_por_id nunca acha o addon.
//   - resposta /tv do TMDB: "name" aparece em created_by[], genres[],
//     networks[], seasons[] e production_companies[], e os primeiros vem ANTES
//     do "name" da raiz na ordem que o TMDB emite. A leitura crua devolveria um
//     GENERO no lugar do titulo da serie.
//
// Anda pelas chaves de profundidade 1, que sao as unicas fora de qualquer { }
// ou [ ] interno. Nao e um analisador: respeita aspas e escape e nada mais.
// 1 quando achou e coube. Valor que nao e string devolve 0.
int js_texto_raiz(const char *corpo, const char *chave, char *dst, size_t tam);
// O mesmo, para um objeto que nao e o documento inteiro: le as chaves de
// profundidade 1 do primeiro '{' em [ini,fim). E o leitor certo para o "name"
// de um catalogo dentro de catalogs[] — js_texto(p, f, "name") devolve o
// primeiro "name" que encontrar na faixa, e se o objeto escreve `extra` antes
// de `name` (o Bingecat escreve) o primeiro e o nome de um EXTRA. `fim` NULL
// significa "ate o fim da string".
int js_texto_raiz_em(const char *ini, const char *fim, const char *chave,
                     char *dst, size_t tam);

#endif
