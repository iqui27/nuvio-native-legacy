#include "descoberta.h"
#include "idioma.h"
#include "ajustes.h"
#include "catordem.h"
#include "fileiras.h"
#include "homeestado.h"
#include "colecoes.h"
#include "marco.h"
#include <SDL2/SDL.h>
#include "catalogo.h"
#include "addons.h"
#include "rede.h"
#include "nuvem.h"
#include "js.h"
#include "trakt.h"
#include "simkl.h"
#include "progresso.h"
#include <stdint.h>   /* uintptr_t: a geracao viaja no argumento do fio */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#define CINEMETA "https://v3-cinemeta.strem.io"
#define TMDB     "https://api.themoviedb.org/3"

// "2026-07-29" -> "29 de julho de 2026". Formato do web, que usa
// `toLocaleDateString(undefined, {month:"long", day:"numeric", year:"numeric"})`
// (metaDetailsScreen.js:1387); o formato numerico que estava aqui antes era
// invencao do port. Entrada que nao casa o padrao ISO sai como veio, e nao
// vazia: melhor mostrar a data crua que engolir o dado.
void desc_data_extenso(const char *iso, char *dst, size_t tam) {
  static const char *MES[12] = {
    "janeiro", "fevereiro", "mar\xc3\xa7o", "abril", "maio", "junho",
    "julho", "agosto", "setembro", "outubro", "novembro", "dezembro"
  };
  if (!iso || !dst || tam == 0) { if (dst && tam) dst[0] = 0; return; }
  if (strlen(iso) >= 10 && iso[4] == '-') {
    int mes = (iso[5] - '0') * 10 + (iso[6] - '0');
    int dia = (iso[8] - '0') * 10 + (iso[9] - '0');
    if (mes >= 1 && mes <= 12) {
      snprintf(dst, tam, i18n("%d de %s de %c%c%c%c"),
               dia, i18n(MES[mes - 1]), iso[0], iso[1], iso[2], iso[3]);
      return;
    }
    snprintf(dst, tam, "%c%c%c%c", iso[0], iso[1], iso[2], iso[3]);
    return;
  }
  snprintf(dst, tam, "%s", iso);
}


// Chave do TMDB, em art/tmdb.txt. SEGREDO do dono (saiu do dist/nuvio.env.js do
// app web) — nao versionar. Sem ela o elenco continua so com nomes.
static char tmdbChave[64];
static char dirArteDesc[512];

void desc_tmdb_definir(const char *chave) {
  if (!chave || !*chave) return;
  // Toda chamada aqui e da API v3 (?api_key=). A conta pode trazer um token v4
  // (JWT "eyJ...", ~200 caracteres): posto em api_key da 401 em tudo — MEDIDO
  // na TV — e nem cabe no campo. Fica a chave do pacote.
  if (strlen(chave) != 32 || !strncmp(chave, "eyJ", 3)) { printf("[desc] tmdb: chave da conta nao e v3, ignorada\n"); return; }
  snprintf(tmdbChave, sizeof tmdbChave, "%s", chave);
  printf("[desc] tmdb: chave da conta\n");
  fflush(stdout);
}

void desc_tmdb(const char *dirArte) {
  char caminho[600];
  FILE *f;
  snprintf(dirArteDesc, sizeof dirArteDesc, "%s", dirArte ? dirArte : ".");
  // Chave DO PACOTE, como o web faz (TMDB_API_KEY do local.properties vira
  // config.js no build). Sem ela, quem nao gravou chave propria na conta ficava
  // sem elenco, ficha, colecao e notas — e quase ninguem grava. A da conta,
  // quando existe, continua ganhando (desc_tmdb_definir chega depois).
#ifdef NV_TMDB_API_KEY
  if (!tmdbChave[0] && sizeof(NV_TMDB_API_KEY) > 1) { snprintf(tmdbChave, sizeof tmdbChave, "%s", NV_TMDB_API_KEY); printf("[desc] tmdb: chave do pacote\n"); }
#endif
  snprintf(caminho, sizeof caminho, "%s/tmdb.txt", dirArte ? dirArte : ".");
  f = fopen(caminho, "r");
  if (!f) return;
  if (fgets(tmdbChave, sizeof tmdbChave, f)) {
    char *fim = tmdbChave + strlen(tmdbChave);
    while (fim > tmdbChave && (fim[-1] == '\n' || fim[-1] == '\r')) *--fim = 0;
  }
  fclose(f);
  printf("[desc] tmdb %s\n", tmdbChave[0] ? "ok" : "ausente");
}

// PORTAO UNICO do TMDB. Todo pedido a api.themoviedb.org passa por aqui, entao
// "tmdb_enabled" da conta (Ajustes -> Integracoes) corta TUDO de uma vez —
// elenco com foto, ficha, trailers, colecao, fontes de colecao do vertudo —
// sem cada consumidor precisar perguntar de novo. "" faz todos os `if` de
// chave cairem fora.
const char *desc_chave_tmdb(void) {
  return ajustes_tmdb_ligado() ? tmdbChave : "";
}
// A RESERVA DE ARTE NAO E INTEGRACAO (#67, 20/09/2026): o ajuste "TMDB" liga
// elenco, ficha, notas — enriquecimento. A reserva so busca a MESMA imagem
// que o metahub nao entregou, e quem chega do app web tem esse ajuste
// desligado por padrao (la e opt-in): com a reserva atras do ajuste, a
// biblioteca do relator continuava sem cartaz depois da 1.3.2. Chave crua,
// sem passar pelo ajuste.
const char *desc_chave_tmdb_reserva(void) { return tmdbChave; }

// strstr que NAO passa de `fim`. O objeto da regiao BR termina antes das
// outras regioes na resposta do TMDB; procurar rent/buy no corpo inteiro
// pegaria o provedor de outra regiao quando a BR nao tivesse.
// Nome de genero em portugues. O Cinemeta devolve os generos SEMPRE em ingles,
// e eles apareciam crus numa interface em portugues — "Filme · Action ·
// Adventure" ao lado de "Programa de TV" traduzido. Nao e gosto: e um bug de
// i18n visivel em toda fileira e em todo detalhe.
//
// Tabela e nao consulta: o conjunto de generos do Stremio e fechado e pequeno,
// e uma viagem de rede por titulo para traduzir duas palavras seria absurdo.
// Genero fora da tabela sai como veio — melhor o ingles que um buraco.
const char *desc_genero_pt(const char *g) {
  // Interface em ingles: o genero do Cinemeta JA e ingles, e traduzir para
  // portugues so para nao ter como voltar seria o bug ao contrario.
  if (ajustes_idioma_ingles()) return g;
  static const struct { const char *en, *pt; } T[] = {
    { "Action",      "Ação" },          { "Adventure",   "Aventura" },
    { "Animation",   "Animação" },      { "Biography",   "Biografia" },
    { "Comedy",      "Comédia" },       { "Crime",       "Crime" },
    { "Documentary", "Documentário" },  { "Drama",       "Drama" },
    { "Family",      "Família" },       { "Fantasy",     "Fantasia" },
    { "Film-Noir",   "Noir" },          { "Game-Show",   "Game show" },
    { "History",     "História" },      { "Horror",      "Terror" },
    { "Music",       "Música" },        { "Musical",     "Musical" },
    { "Mystery",     "Mistério" },      { "News",        "Notícias" },
    { "Reality-TV",  "Reality" },       { "Romance",     "Romance" },
    { "Sci-Fi",      "Ficção científica" },
    { "Science Fiction", "Ficção científica" },
    { "Short",       "Curta" },         { "Sport",       "Esporte" },
    { "Talk-Show",   "Talk show" },     { "Thriller",    "Suspense" },
    { "War",         "Guerra" },        { "Western",     "Faroeste" },
    { "Kids",        "Infantil" },      { "Soap",        "Novela" },
    { "Adult",       "Adulto" },
  };
  size_t i;
  if (!g || !*g) return "";
  for (i = 0; i < sizeof T / sizeof *T; i++)
    if (!strcasecmp(g, T[i].en)) return T[i].pt;
  return g;
}

static const char *ate(const char *ini, const char *fim, const char *agulha) {
  size_t n = strlen(agulha);
  for (; ini && ini + n <= fim; ini++)
    if (*ini == agulha[0] && memcmp(ini, agulha, n) == 0) return ini;
  return NULL;
}

// O decodificador de textura so trabalha com raster (png/jpg/webp/gif); um
// caminho .svg baixa e morre em "resposta nao e imagem". Sufixo basta: TMDB
// e os addons mandam o nome do arquivo limpo, sem query.
static int ehSvg(const char *s) {
  size_t n = s ? strlen(s) : 0;
  return n > 4 && !strcmp(s + n - 4, ".svg");
}

// Primeiro provedor do array `chave` dentro de [ini,fim): nome e logo no
// formato w92 do TMDB. flatrate/rent/buy sao arrays de provedores; o primeiro
// e o principal na pratica (o TMDB ordena por relevancia local).
static int provedorEntre(const char *ini, const char *fim, const char *chave,
                         char *nome, size_t nNome, char *logo, size_t nLogo) {
  const char *k = ate(ini, fim, chave);
  const char *item = k ? strchr(k, '{') : NULL;
  if (!item || item >= fim) return 0;
  const char *fi = js_fim(item);
  if (fi > fim) fi = fim;
  char caminho[128] = "";
  if (!js_texto(item, fi, "provider_name", nome, nNome)) return 0;
  if (js_texto(item, fi, "logo_path", caminho, sizeof caminho) &&
      caminho[0] == '/' && !ehSvg(caminho))
    snprintf(logo, nLogo, "https://image.tmdb.org/t/p/w92%s", caminho);
  return 1;
}

// Preenche foto e personagem do elenco. O Cinemeta da so o NOME; o personagem
// e o retrato vem do TMDB, que precisa de duas viagens: achar o id dele pelo
// id do IMDb e so entao pedir os creditos.
static void fotosDoElenco(CatItem *d, const char *imdbSerie, int serie) {
  char url[400], *corpo;
  const char *chave;
  long idTmdb = 0;
  char logoAntes[512];
  logoAntes[0] = 0;
  if (d->logo[0]) snprintf(logoAntes, sizeof logoAntes, "%s", d->logo);
  if (!d->nElenco) return;
  chave = desc_chave_tmdb();            // "" com a integracao desligada
  if (!chave[0]) return;
  snprintf(url, sizeof url, "%s/find/%s?api_key=%s&external_source=imdb_id",
           TMDB, imdbSerie, chave);
  corpo = rede_baixar(url, 20);
  if (!corpo) return;
  { const char *vet = serie ? "tv_results" : "movie_results";
    const char *p = js_array(corpo, NULL, vet);
    if (p) idTmdb = (long)js_num(p, js_fim(p), "id", 0); }
  free(corpo);
  if (!idTmdb) return;
  d->tmdb = idTmdb;

  // TITULO E SINOPSE NO IDIOMA DA INTERFACE.
  //
  // O titulo que a tela mostrava vinha do CATALOGO do addon — Cinemeta e afins
  // respondem em ingles e nao tem parametro de idioma. Ou seja: o app em
  // portugues, com o TMDB configurado em portugues, mostrava "Motor City" e a
  // sinopse em ingles, e nao havia ajuste que mudasse isso porque nao havia
  // caminho. E o issue #8. Aqui existe: o id do TMDB acabou de ser resolvido
  // logo acima, entao localizar custa UM pedido a mais numa funcao que ja faz
  // tres — e ela roda uma vez por titulo ABERTO, num fio proprio, nao por card
  // da home.
  //
  // js_texto_raiz e nao js_texto: numa resposta /tv o "name" aparece dentro de
  // created_by[], genres[], networks[] e seasons[], e os primeiros vem ANTES do
  // "name" da raiz na ordem que o TMDB emite — a leitura crua traria um GENERO
  // no lugar do titulo da serie. Mesma coisa com "overview" e as temporadas.
  //
  // CAMPO VAZIO NAO SUBSTITUI: o TMDB responde 200 com "" no titulo e na
  // sinopse quando ninguem traduziu aquele filme, e trocar o ingles por vazio
  // deixaria a tela SEM titulo. Falta de traducao continua mostrando o
  // original, que e o comportamento honesto.
  // "Titulo e sinopse" e `tmdb_use_basic_info`: desligado, o texto fica o do
  // catalogo do addon — e o elenco (logo abaixo) segue `tmdb_use_credits`.
  // "Arte localizada" (`tmdb_use_artwork`) vem no MESMO pedido via
  // append_to_response=images: o logo do titulo no idioma configurado
  // substitui o do catalogo, que e quase sempre o ingles do Cinemeta.
  if (ajustes_tmdb_basico() || ajustes_tmdb_arte()) {
    char incImg[64] = "";
    if (ajustes_tmdb_arte())
      // include_image_language aceita a lista; "%.2s" de "pt-BR" da "pt".
      snprintf(incImg, sizeof incImg,
               "&append_to_response=images&include_image_language=%.2s,null,en",
               desc_tmdb_idioma());
    snprintf(url, sizeof url, "%s/%s/%ld?api_key=%s&language=%s%s",
             TMDB, serie ? "tv" : "movie", idTmdb, chave, desc_tmdb_idioma(),
             incImg);
    corpo = rede_baixar(url, 20);
    if (corpo) {
      // Guarda o backdrop do proprio TMDB como uma variante separada. O
      // catalogo continua mandando na arte efetiva por padrao; a escolha do
      // hero pode pedir esta origem sem fazer outra consulta.
      { char fundo[160] = "";
        if (js_texto_raiz(corpo, "backdrop_path", fundo, sizeof fundo) &&
            fundo[0] == '/')
          snprintf(d->backdropTmdb, sizeof d->backdropTmdb,
                   "https://image.tmdb.org/t/p/w1280%s", fundo); }
      if (ajustes_tmdb_basico()) {
        char t[160], sin[900];
        if (js_texto_raiz(corpo, serie ? "name" : "title", t, sizeof t) && t[0])
          snprintf(d->titulo, sizeof d->titulo, "%s", t);
        if (js_texto_raiz(corpo, "overview", sin, sizeof sin) && sin[0])
          snprintf(d->sinopse, sizeof d->sinopse, "%s", sin);
      }
      if (ajustes_tmdb_arte()) {
        // images.logos[]: prefere o do idioma configurado; na falta, o sem
        // idioma (iso_639_1 null le como "") ou o ingles. Vazio nao substitui
        // — mesma regra do titulo, um logo que nao veio nao apaga o atual.
        // SVG TAMBEM NAO SUBSTITUI: o TMDB tem logos em .svg e o
        // decodificador so trabalha com raster — um file_path svg gravado em
        // d->logo vira "resposta nao e imagem" no tex e o titulo fica sem a
        // arte para sempre (o FALHOU e lembrado). Visto no log da TV em
        // 21/09 com /f91b8uWsSaeGRYCj8k73uIFj9pu.svg.
        const char *im = strstr(corpo, "\"images\"");
        const char *imObj = im ? strchr(im, '{') : NULL;
        const char *imFim = imObj ? js_fim(imObj) : NULL;
        const char *p = (imObj && imFim) ? js_array(imObj, imFim, "logos")
                                         : NULL;
        char base[3] = "", local[160] = "", neutro[160] = "", en[160] = "";
        snprintf(base, sizeof base, "%.2s", desc_tmdb_idioma());
        while (p) {
          const char *f = js_fim(p);
          char iso[8] = "", fp[160] = "";
          js_texto(p, f, "iso_639_1", iso, sizeof iso);
          js_texto(p, f, "file_path", fp, sizeof fp);
          if (fp[0] == '/' && !ehSvg(fp)) {
            if      (!strcmp(iso, base)) snprintf(local,  sizeof local,  "%s", fp);
            else if (!iso[0] && !neutro[0]) snprintf(neutro, sizeof neutro, "%s", fp);
            else if (!strcmp(iso, "en") && !en[0]) snprintf(en, sizeof en, "%s", fp);
          }
          p = js_prox(f);
        }
        { const char *esc = local[0] ? local : neutro[0] ? neutro : en;
          if (esc[0])
            snprintf(d->logo, sizeof d->logo,
                     "https://image.tmdb.org/t/p/w500%s", esc); }
        // LIMPA O QUE JA ESTAVA ENVENENADO: item do cache do catalogo pode ter
        // entrado com logo .svg (desta funcao antes do filtro, ou de um addon
        // que mande svg em `logo` — ver deMeta). Sem uso possivel, fora.
        if (ehSvg(d->logo)) d->logo[0] = 0;
        if (d->logo[0] && d->poster[0] && !strcmp(d->logo, d->poster) &&
            logoAntes[0] && strcmp(logoAntes, d->poster))
          snprintf(d->logo, sizeof d->logo, "%s", logoAntes);
        else if (!d->logo[0] && logoAntes[0] && !ehSvg(logoAntes))
          snprintf(d->logo, sizeof d->logo, "%s", logoAntes);
      }
      free(corpo);
    }
  }

  // Elenco com foto e `tmdb_use_credits`. O `free` mora DENTRO do if porque o
  // watch/providers logo abaixo nao depende dele.
  if (ajustes_tmdb_elenco()) {
    snprintf(url, sizeof url, "%s/%s/%ld/credits?api_key=%s",
             TMDB, serie ? "tv" : "movie", idTmdb, chave);
    corpo = rede_baixar(url, 20);
    if (corpo) {
      const char *p = js_array(corpo, NULL, "cast");
      int k = 0;
      while (p && k < d->nElenco) {
        const char *f = js_fim(p);
        char caminhoFoto[128] = "";
        js_texto(p, f, "character", d->elenco[k].papel, sizeof d->elenco[k].papel);
        d->elenco[k].tmdb = (long)js_num(p, f, "id", 0.0);
        if (js_texto(p, f, "profile_path", caminhoFoto, sizeof caminhoFoto) &&
            caminhoFoto[0] == '/')
          snprintf(d->elenco[k].foto, sizeof d->elenco[k].foto,
                   "https://image.tmdb.org/t/p/w185%s", caminhoFoto);
        // O TMDB devolve o elenco na mesma ordem de importancia que o Cinemeta,
        // entao casar por posicao acerta na pratica; casar por nome falharia nos
        // acentos e nos nomes escritos de forma diferente entre as duas bases.
        k++;
        p = js_prox(f);
      }
      // E A LISTA CRESCE: o Cinemeta para em 3-5 nomes no `cast` e era isso que
      // a fileira mostrava (issue #94: "so 3 pessoas no elenco"). `p` ja esta
      // na entrada seguinte a ultima enriquecida; daqui em diante cada entrada
      // do cast do TMDB vira um NOME NOVO, ate o teto do vetor. Sem o TMDB
      // ligado este bloco nem roda — nada muda para quem nao o configurou.
      while (p && d->nElenco < CAT_ELENCO_MAX) {
        const char *f = js_fim(p);
        int k2 = d->nElenco;
        char caminhoFoto[128] = "";
        js_texto(p, f, "name", d->elenco[k2].nome, sizeof d->elenco[k2].nome);
        js_texto(p, f, "character", d->elenco[k2].papel, sizeof d->elenco[k2].papel);
        d->elenco[k2].tmdb = (long)js_num(p, f, "id", 0.0);
        if (js_texto(p, f, "profile_path", caminhoFoto, sizeof caminhoFoto) &&
            caminhoFoto[0] == '/')
          snprintf(d->elenco[k2].foto, sizeof d->elenco[k2].foto,
                   "https://image.tmdb.org/t/p/w185%s", caminhoFoto);
        // Entrada sem nome nao vira pessoa na fileira: pula sem contar.
        if (d->elenco[k2].nome[0]) d->nElenco++;
        p = js_prox(f);
      }
      free(corpo);
    }
  }

  // Onde assistir. Os campos provLogo/provNome existiam no CatItem e NUNCA
  // eram preenchidos no caminho dinamico — o selo do streaming ficava vazio em
  // todo titulo. O TMDB responde por regiao; BR e a do dono.
  snprintf(url, sizeof url, "%s/%s/%ld/watch/providers?api_key=%s",
           TMDB, serie ? "tv" : "movie", idTmdb, chave);
  corpo = rede_baixar(url, 20);
  if (corpo) {
    const char *br = strstr(corpo, "\"BR\"");
    if (br) {
      const char *brObj = strchr(br, '{');
      const char *brFim = brObj ? js_fim(brObj) : NULL;
      if (brObj && brFim && brFim > brObj) {
        // flatrate = incluido na assinatura; rent = aluguel; buy = compra.
        // Se o titulo nao esta em streaming aqui, o selo fica vazio DE
        // PROPOSITO, em vez de anunciar aluguel como se fosse catalogo.
        provedorEntre(brObj, brFim, "\"flatrate\"",
                      d->provNome, sizeof d->provNome,
                      d->provLogo, sizeof d->provLogo);
        provedorEntre(brObj, brFim, "\"rent\"",
                      d->alugNome, sizeof d->alugNome,
                      d->alugLogo, sizeof d->alugLogo);
        provedorEntre(brObj, brFim, "\"buy\"",
                      d->compNome, sizeof d->compNome,
                      d->compLogo, sizeof d->compLogo);
      }
    }
    free(corpo);
  }
}

// Definida adiante, junto do resto do parse de meta do Stremio; declarada aqui
// porque a busca, logo abaixo, monta CatItem a partir da mesma resposta.
static int deMeta(const char *ini, const char *fim, const char *tipo, CatItem *d);

// --- BUSCA POR TITULO --------------------------------------------------------
//
// A tela de busca so filtrava o que ja estava em memoria (um strstr sobre as
// fileiras da home), entao procurar por algo fora das ~12 primeiras linhas de
// cada catalogo nao achava nada — e o dono viu isso como "nao ta procurando em
// tudo". Era verdade: nao havia consulta de rede nenhuma.
//
// O protocolo Stremio expoe busca no mesmo endpoint de catalogo, com o filtro
// no caminho: <base>/catalog/<tipo>/<id>/search=<termo>.json. O Cinemeta, que e
// o catalogo oficial e nao depende dos addons do dono, responde nos dois tipos
// — e por isso e a fonte usada aqui: uma busca que so funcionasse com os addons
// instalados falharia de formas diferentes em cada maquina.
//
// Roda em FIO PROPRIO porque bloqueia (duas viagens), e a tela de busca nao
// pode congelar entre uma tecla e outra.
static char     buscaTermo[96];     // termo JA CONSULTADO
static char     buscaPedido[96];    // termo que os fios devem consultar
static pthread_mutex_t buscaTrava = PTHREAD_MUTEX_INITIALIZER;

// Escapa o termo para caber num caminho de URL. Sem isto um espaco ou acento
// quebra o pedido, e "the invite" — duas palavras, o caso normal — nunca
// chegaria ao servidor.
static void urlEscapar(const char *s, char *dst, size_t tam) {
  static const char *HEX = "0123456789ABCDEF";
  size_t o = 0;
  for (; *s && o + 4 < tam; s++) {
    unsigned char c = (unsigned char)*s;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      dst[o++] = (char)c;
    } else {
      dst[o++] = '%'; dst[o++] = HEX[c >> 4]; dst[o++] = HEX[c & 15];
    }
  }
  dst[o] = 0;
}

static int lerBusca(const char *tipo, const char *termo, CatItem *saida,
                    int max) {
  char url[500], esc[300];
  char *corpo;
  const char *p;
  int n = 0;
  urlEscapar(termo, esc, sizeof esc);
  snprintf(url, sizeof url, "%s/catalog/%s/top/search=%s.json",
           CINEMETA, tipo, esc);
  corpo = rede_baixar(url, 20);
  if (!corpo) return 0;
  p = js_array(corpo, NULL, "metas");
  while (p && n < max) {
    const char *f = js_fim(p);
    if (deMeta(p, f, tipo, &saida[n])) n++;
    p = js_prox(f);
  }
  free(corpo);
  return n;
}

// --- ALVOS DE BUSCA ---------------------------------------------------------
//
// Um "alvo" e um catalogo que aceita busca. Sao os 2 do Cinemeta (que existem
// sempre, independem dos addons do dono) mais os que os manifestos declararem.
// Nos addons do dono sao 8: Xperience (filme/serie), AIOStreams TMDB e TVDB
// (filme/serie cada) e Akashi TV (filme/serie).
//
// Antes so o Cinemeta era consultado, e era isso que o dono via como "nao ta
// procurando em todos os catalogos" — porque de fato nao estava.
#define BUSCA_ALVOS  16
#define BUSCA_POR_ALVO 12          // uma fileira por alvo, 12 cabem na tela
#define BUSCA_FIOS    3            // quantos alvos em voo ao mesmo tempo

typedef struct {
  char base[300];
  char tipo[8];
  char id[96];
  char titulo[96];
  char addon[64];
} AlvoBusca;

static AlvoBusca alvos[BUSCA_ALVOS];
static int       nAlvos;

// Resultado POR ALVO, com a geracao em que foi obtido. Guardar por alvo (e nao
// numa lista unica) e o que permite uma fileira por catalogo, com a origem, e o
// que deixa a tela mostrar o primeiro que responder sem esperar o mais lento.
static struct {
  CatItem itens[BUSCA_POR_ALVO];
  int     n;
  int     geracao;
} resAlvo[BUSCA_ALVOS];

static int  geracao;            // sobe a cada termo novo
static int  proximoAlvo;        // fila de trabalho: proximo indice a consultar
static int  fiosVivos;

void desc_alvos_busca_zerar(void) {
  pthread_mutex_lock(&buscaTrava);
  // O Cinemeta entra SEMPRE e primeiro: e a unica fonte que nao depende de
  // addon nenhum, entao a busca continua funcionando numa instalacao limpa.
  nAlvos = 0;
  { int t; const char *tt[2] = { "movie", "series" };
    const char *rot[2] = { "Filmes", "Séries" };
    for (t = 0; t < 2; t++) {
      AlvoBusca *a = &alvos[nAlvos++];
      snprintf(a->base,  sizeof a->base,  "%s", CINEMETA);
      snprintf(a->tipo,  sizeof a->tipo,  "%s", tt[t]);
      snprintf(a->id,    sizeof a->id,    "%s", "top");
      snprintf(a->titulo,sizeof a->titulo,"%s", rot[t]);
      snprintf(a->addon, sizeof a->addon, "%s", "Cinemeta");
    } }
  memset(resAlvo, 0, sizeof resAlvo);
  pthread_mutex_unlock(&buscaTrava);
}

void desc_alvo_busca(const char *base, const char *tipo, const char *id,
                     const char *titulo, const char *addon) {
  pthread_mutex_lock(&buscaTrava);
  if (nAlvos < BUSCA_ALVOS) {
    AlvoBusca *a = &alvos[nAlvos++];
    snprintf(a->base,   sizeof a->base,   "%s", base ? base : "");
    snprintf(a->tipo,   sizeof a->tipo,   "%s", tipo ? tipo : "");
    snprintf(a->id,     sizeof a->id,     "%s", id ? id : "");
    snprintf(a->titulo, sizeof a->titulo, "%s", titulo ? titulo : "");
    snprintf(a->addon,  sizeof a->addon,  "%s", addon ? addon : "");
  }
  pthread_mutex_unlock(&buscaTrava);
}

// Consulta UM alvo. Devolve quantos itens leu.
static int consultarAlvo(const AlvoBusca *a, const char *termo,
                         CatItem *saida, int max) {
  char url[600], esc[300];
  char *corpo;
  const char *p;
  int n = 0;
  urlEscapar(termo, esc, sizeof esc);
  snprintf(url, sizeof url, "%s/catalog/%s/%s/search=%s.json",
           a->base, a->tipo, a->id, esc);
  // 6 s por alvo, como o web (SEARCH_CATALOG_TIMEOUT 6500). Addon lento nao
  // trava a tela: a fileira dele so aparece quando chegar, e as outras ja
  // estao la.
  corpo = rede_baixar(url, 6);
  if (!corpo) return 0;
  p = js_array(corpo, NULL, "metas");
  while (p && n < max) {
    const char *f = js_fim(p);
    if (deMeta(p, f, a->tipo, &saida[n])) n++;
    p = js_prox(f);
  }
  free(corpo);
  return n;
}

static void *fioBusca(void *arg) {
  (void)arg;
  for (;;) {
    AlvoBusca a;
    char termo[96];
    int meu, g;
    CatItem achados[BUSCA_POR_ALVO];
    int n;

    pthread_mutex_lock(&buscaTrava);
    if (proximoAlvo >= nAlvos || !buscaPedido[0]) {
      fiosVivos--;
      pthread_mutex_unlock(&buscaTrava);
      return NULL;
    }
    meu = proximoAlvo++;
    a = alvos[meu];
    g = geracao;
    snprintf(termo, sizeof termo, "%s", buscaPedido);
    pthread_mutex_unlock(&buscaTrava);

    n = consultarAlvo(&a, termo, achados, BUSCA_POR_ALVO);

    pthread_mutex_lock(&buscaTrava);
    // Geracao velha = o dono digitou outra coisa enquanto isto voltava. O
    // resultado nasceu obsoleto; descartar e mais barato que mostrar e trocar.
    if (g == geracao) {
      memcpy(resAlvo[meu].itens, achados, sizeof(CatItem) * (size_t)n);
      resAlvo[meu].n = n;
      resAlvo[meu].geracao = g;
      snprintf(buscaTermo, sizeof buscaTermo, "%s", termo);
    }
    pthread_mutex_unlock(&buscaTrava);
  }
}

void desc_buscar(const char *termo) {
  int k, faltam;
  if (!termo) return;
  pthread_mutex_lock(&buscaTrava);
  if (!strcmp(termo, buscaPedido)) { pthread_mutex_unlock(&buscaTrava); return; }
  snprintf(buscaPedido, sizeof buscaPedido, "%s", termo);
  geracao++;
  proximoAlvo = 0;
  // Zera a contagem, nao os itens: a tela pode estar desenhando o quadro
  // corrente e ler item pela metade seria pior que uma fileira a menos.
  for (k = 0; k < BUSCA_ALVOS; k++) resAlvo[k].n = 0;
  faltam = BUSCA_FIOS - fiosVivos;
  pthread_mutex_unlock(&buscaTrava);

  // Fios sob demanda: os que ja estao vivos pegam os alvos novos sozinhos,
  // porque leem `proximoAlvo` sob a trava a cada volta.
  for (k = 0; k < faltam; k++) {
    pthread_t t;
    pthread_mutex_lock(&buscaTrava); fiosVivos++; pthread_mutex_unlock(&buscaTrava);
    if (pthread_create(&t, NULL, fioBusca, NULL) != 0) {
      pthread_mutex_lock(&buscaTrava); fiosVivos--; pthread_mutex_unlock(&buscaTrava);
    } else {
      pthread_detach(t);
    }
  }
}

int desc_busca_geracao(void) {
  int g;
  pthread_mutex_lock(&buscaTrava);
  g = geracao;
  pthread_mutex_unlock(&buscaTrava);
  return g;
}

int desc_busca_n_alvos(void) { return nAlvos; }

int desc_busca_alvo_n(int alvo, const char *termo) {
  int n = 0;
  pthread_mutex_lock(&buscaTrava);
  if (alvo >= 0 && alvo < nAlvos && termo && !strcmp(termo, buscaTermo) &&
      resAlvo[alvo].geracao == geracao)
    n = resAlvo[alvo].n;
  pthread_mutex_unlock(&buscaTrava);
  return n;
}

const char *desc_busca_alvo_titulo(int alvo) {
  return (alvo >= 0 && alvo < nAlvos) ? alvos[alvo].titulo : "";
}
const char *desc_busca_alvo_addon(int alvo) {
  return (alvo >= 0 && alvo < nAlvos) ? alvos[alvo].addon : "";
}

int desc_busca_alvo_item(int alvo, int i, CatItem *dst) {
  int ok = 0;
  pthread_mutex_lock(&buscaTrava);
  if (dst && alvo >= 0 && alvo < nAlvos && i >= 0 && i < resAlvo[alvo].n) {
    memcpy(dst, &resAlvo[alvo].itens[i], sizeof *dst);
    ok = 1;
  }
  pthread_mutex_unlock(&buscaTrava);
  return ok;
}

// Compatibilidade com quem ainda pergunta "quantos no total".
int desc_busca_n(const char *termo) {
  int k, t = 0;
  for (k = 0; k < nAlvos; k++) t += desc_busca_alvo_n(k, termo);
  return t;
}


static int buscando;
// Identidade da montagem em voo. Troca de conta/perfil/config invalida o fio
// antigo antes que ele publique ou grave um snapshot privado no contexto novo.
static volatile unsigned montagemGeracao;
// Lido pelo fio de montagem no fim do ciclo e escrito pelo laco principal.
// `volatile` porque sao fios diferentes; nao ha corrida real de valor — o pior
// caso e uma remontagem a mais, que e barata perto de perder o pedido.
static volatile int repetirAoFim;
// GERACAO DO PEDIDO DE REMONTAGEM, e a razao dela existir esta medida.
//
// `repetirAoFim` sozinho nao distingue duas coisas muito diferentes: um pedido
// que chegou ANTES de o ciclo ler a lista de addons — e que portanto ja foi
// atendido por este mesmo ciclo — de um que chegou DEPOIS, e que exige outra
// volta.
//
// MEDIDO na C9, arranque com conta: sync publica os addons da conta em ~2 s, o
// ciclo comeca em 0,9 s e so chega aos manifestos em ~7 s. Ou seja o ciclo em
// curso JA leu a lista nova — e mesmo assim a marca fazia uma segunda volta
// completa, com 14 s de Trakt e manifestos refeitos, terminando em t=40 s em
// vez de t=18 s. A segunda volta produzia exatamente o mesmo resultado da
// primeira: da para conferir no log, as duas linhas de `[desc] catalogos:` sao
// identicas.
static volatile unsigned geracaoPedida;
static unsigned geracaoLida;
// Ver desc_catalogos_fora em descoberta.h.
static int catalogosFora;
// Quantos catalogos NAO DESLIGADOS o teto impediu de pedir na ultima montagem,
// sem o clamp de catalogosFora. Ver o uso em desc_remontar_fileiras.
static int catalogosNaoPedidos;
static pthread_t fio, fioEp;
static int epItem = -1, epTemp, fioEpVivo;

int desc_buscando(void) {
  int v;
  pthread_mutex_lock(&buscaTrava);
  v = (fiosVivos > 0);
  pthread_mutex_unlock(&buscaTrava);
  return v;
}

// Um item do catalogo montado a partir de um meta do Stremio. Devolve 1 se
// deu para aproveitar (precisa de nome e de alguma arte).
// O TIPO COMO ELE APARECE NA TELA. Tres formas, porque as tres telas pedem
// coisas diferentes: singular na linha de genero do card, plural no titulo da
// fileira (e o que o app web escreve — "Canais de TV - Canais"), e o rotulo
// CRU em ingles so para nao repetir o sufixo quando o proprio addon ja o
// escreveu no nome.
//
// Antes disto tudo que nao fosse "series" era FILME, e um canal de TV ao vivo
// aparecia como "Filme" na linha de genero e "- Filme" no titulo da fileira
// (issue #37). Os tipos que o Stremio usa para canal sao `channel` e `tv`; os
// dois chegam nos addons do dono.
static int ehCanal(const char *tipo) {
  return tipo && (!strcmp(tipo, "channel") || !strcmp(tipo, "tv"));
}
static const char *rotuloTipoSing(const char *tipo) {
  if (ehCanal(tipo)) return "Canal";
  return strcmp(tipo, "series") ? "Filme" : "Programa de TV";
}

static int deMeta(const char *ini, const char *fim, const char *tipo, CatItem *d) {
  char v[900];
  memset(d, 0, sizeof *d);
  if (!js_texto(ini, fim, "name", d->titulo, sizeof d->titulo)) return 0;
  // O poster e o unico obrigatorio: sem ele o card fica um retangulo cinza.
  if (!js_texto(ini, fim, "poster", d->poster, sizeof d->poster)) return 0;
  js_texto(ini, fim, "background", d->backdrop, sizeof d->backdrop);
  snprintf(d->backdropCatalogo, sizeof d->backdropCatalogo, "%s", d->backdrop);
  if (strstr(d->backdrop, "image.tmdb.org/t/p/"))
    snprintf(d->backdropTmdb, sizeof d->backdropTmdb, "%s", d->backdrop);
  if (strstr(d->backdrop, "media.trakt.tv/"))
    snprintf(d->backdropTrakt, sizeof d->backdropTrakt, "%s", d->backdrop);
  js_texto(ini, fim, "logo", d->logo, sizeof d->logo);
  // LOGO IGUAL AO POSTER NAO E LOGO. MEDIDO no catalogo gravado da C9 em 18/09:
  // o Xperience manda, para "O Fim da Rua", o MESMO arquivo do TMDB
  // (4kfDP13cYwCx55YP2gLGtcUFZlC.jpg) em `poster` e em `logo` — e um addon
  // preenchendo o campo com o que tem quando nao tem logo. O app confiava e
  // desenhava o poster onde vai a arte do titulo: no hero da home e na pagina
  // do titulo aparecia uma capa pequena no lugar do logo. O dono viu e
  // perguntou por que. Sem logo, o hero escreve o NOME em texto (ver home.c),
  // que e o comportamento certo e ja existia — so nao era alcancado.
  if (d->logo[0] && !strcmp(d->logo, d->poster)) d->logo[0] = 0;
  // LOGO EM SVG TAMBEM NAO ENTRA — o decodificador nao rasteriza vetor, e o
  // FALHOU gravado no tex deixava o titulo sem arte para sempre. Sem logo o
  // hero escreve o nome, que e o fallback certo e ja existente.
  if (ehSvg(d->logo)) d->logo[0] = 0;
  // O TMDB serve o backdrop em /original/, que e 3840x2160. O download nem e o
  // problema (268 KB contra 201 KB do w1280) — o problema e o DECODIFICADO:
  // 8,3 MP viram 33 MB em RAM, mais outros 33 MB na conversao de formato, antes
  // de o SDL_BlitScaled reduzir para o teto de 1920. Num nucleo fraco isso e
  // ~0,5 s por arte, e a cada troca de heroi. Em w1280 sao 3,7 MB e ~9x menos
  // trabalho; o heroi e desenhado a 1920, entao amplia 1,5x — com o degrade e o
  // texto por cima, a diferenca nao aparece, e o tranco aparecia.
  //
  // Feito por reescrita de URL e nao pedindo outro campo porque o Cinemeta so
  // devolve este; a escada do TMDB e w300/w780/w1280/original.
  { char *o = strstr(d->backdrop, "/t/p/original/");
    if (o) {
      char novo[sizeof d->backdrop];
      snprintf(novo, sizeof novo, "%.*s/t/p/w1280/%s",
               (int)(o - d->backdrop), d->backdrop, o + 14);
      snprintf(d->backdrop, sizeof d->backdrop, "%s", novo);
    } }
  if (!d->backdrop[0]) snprintf(d->backdrop, sizeof d->backdrop, "%s", d->poster);
  // A variante de catálogo é a mesma arte que alimenta o card, já com a
  // dimensão segura para a TV. O TMDB/Trakt ficam em campos separados quando
  // chegam por seus próprios caminhos.
  snprintf(d->backdropCatalogo, sizeof d->backdropCatalogo, "%s", d->backdrop);
  if (d->backdropTmdb[0] && strstr(d->backdropTmdb, "image.tmdb.org/t/p/"))
    snprintf(d->backdropTmdb, sizeof d->backdropTmdb, "%s", d->backdrop);

  if (!js_texto(ini, fim, "imdb_id", d->imdb, sizeof d->imdb))
    js_texto(ini, fim, "id", d->imdb, sizeof d->imdb);
  snprintf(d->tipo, sizeof d->tipo, "%s", tipo);

  { // genero: "Filme · Acao · Drama"
    const char *g = js_array(ini, fim, "genres");
    char g1[48] = "", g2[48] = "";
    if (g) {
      const char *f1 = js_fim(g);
      (void)f1;
      // elementos de texto: copiar direto do array
      { const char *p = g; int k = 0;
        while (p && k < 2) {
          char tmp[48]; size_t n = 0;
          if (*p != '"') break;
          p++;
          while (*p && *p != '"' && n + 1 < sizeof tmp) tmp[n++] = *p++;
          tmp[n] = 0;
          // Traduz AQUI, na entrada: o campo `genero` do CatItem e usado por
          // varias telas e todas mostrariam o ingles se a traducao ficasse no
          // desenho.
          if (k == 0) snprintf(g1, sizeof g1, "%s", desc_genero_pt(tmp));
          else        snprintf(g2, sizeof g2, "%s", desc_genero_pt(tmp));
          k++;
          p++;
          while (*p == ' ') p++;
          if (*p != ',') break;
          p++;
          while (*p == ' ') p++;
        } }
    }
    snprintf(d->genero, sizeof d->genero, "%s%s%s%s%s",
             i18n(rotuloTipoSing(tipo)),
             g1[0] ? "  \xc2\xb7  " : "", g1,
             g2[0] ? "  \xc2\xb7  " : "", g2);
  }
  v[0] = 0;
  js_texto(ini, fim, "releaseInfo", v, sizeof v);
  { char dur[24] = "";
    js_texto(ini, fim, "runtime", dur, sizeof dur);
    // "2024–" vira "2024": o travessao de serie em andamento polui a linha.
    { char *tr = strstr(v, "\xe2\x80\x93"); if (tr) *tr = 0; }
    snprintf(d->meta, sizeof d->meta, "%.20s%s%.20s", v,
             (v[0] && dur[0]) ? "  \xc2\xb7  " : "", dur); }
  js_texto(ini, fim, "description", d->sinopse, sizeof d->sinopse);
  // NAO INVENTAR CLASSIFICACAO. Aqui havia um `"14"` cravado, e o efeito era
  // que TODO titulo vindo da rede exibia o selo "14" — o Cinemeta nao manda
  // classificacao etaria, e o valor de reserva virou uma constante disfarcada
  // de dado, desenhada com a mesma confianca de um campo real.
  //
  // Vazio e a resposta honesta: desenhaSeloMeta ja e guardado por
  // `classificacao[0]` no chamador (detail.c), entao o selo simplesmente nao
  // aparece enquanto nao houver valor. Quem preenche de verdade e a ficha do
  // TMDB em extras.c (release_dates -> certification), que chega depois.
  d->classificacao[0] = 0;
  { double nota = js_num(ini, fim, "imdbRating", 0.0);
    d->nota = (int)(nota * 10.0 + 0.5) / 1; }
  if (d->nota > 99) d->nota /= 10;
  return 1;
}

// Le um catalogo (movie|series) de um addon e acrescenta ao vetor.
//
// `respondeu` (pode ser NULL) separa os dois zeros que sairiam iguais: 0 com
// resposta e "o catalogo esta VAZIO hoje", 0 sem resposta e "o addon nao
// respondeu em 8 s". Do lado de fora os dois viravam a mesma fileira ausente, e
// era exatamente por isso que nao dava para dizer o que falhou num arranque.
static int lerCatalogo(const char *base, const char *tipo, const char *id,
                       CatItem *saida, int max, int quantos, int *respondeu) {
  char url[900];
  char *corpo;
  const char *p;
  int n = 0;
  if (respondeu) *respondeu = 0;
  snprintf(url, sizeof url, "%s/catalog/%s/%s.json", base, tipo, id);
  // 8 s e nao 25: um addon fora do ar segurava um dos tres fios por 25 s, e a
  // fileira dele atrasa TODAS as seguintes porque a montagem caminha em ordem.
  // E a mesma licao ja registrada no cache de texturas — la o timeout caiu de
  // 25 para 8 pelo mesmo motivo, com duas URLs mortas travando os dois fios de
  // decode. Um catalogo que nao responde em 8 s nao vai responder.
  corpo = rede_baixar(url, 8);
  if (!corpo) return 0;
  if (respondeu) *respondeu = 1;
  p = js_array(corpo, NULL, "metas");
  while (p && n < max && n < quantos) {
    const char *f = js_fim(p);
    if (deMeta(p, f, tipo, &saida[n])) n++;
    p = js_prox(f);
  }
  free(corpo);
  return n;
}

// --- fileiras da home: catalogos declarados pelos addons ---------------------
// Isto substitui a lista PREF fixa de quatro catalogos. O app web nao tem
// fileira fixa: cada fileira e um catalogo declarado no manifesto de um addon,
// e a ordem/visibilidade/nome saem de `homeCatalogPrefs`. Ver o comentario
// grande em catalogo.h, que traz o algoritmo de sortAndFilterRowsInternal.

// Teto de catalogos declarados somando TODOS os addons.
//
// Era 64, e o Xperience sozinho declara 64 — o `nDecl < DECL_MAX` do laco
// parava ali, e AIOStreams e Akashi TV nunca tinham o manifesto sequer lido.
// Nem as fileiras deles apareciam na home, nem os catalogos de busca deles
// existiam: o app se comportava como se o dono tivesse instalado um addon so.
// O sintoma que chegou primeiro foi a busca ("nao procura em todos os
// catalogos"), mas o teto cortava tudo.
// 512, nao mais 256: issue #42(a), "some rows were not showing no matter
// what". A cota por addon (DECL_MAX/nAd, logo abaixo) ja resolveu o addon
// SOZINHO tomando tudo; o que ela nao resolve e o TOTAL — com muitos addons
// modestos (nenhum sozinho estoura a cota) a SOMA dos catalogos legitimos
// passava de 256 e os ultimos, por addon, eram cortados mesmo cabendo de
// sobra na memoria (Decl tem ~950 bytes; 512 custam ~475 KB, uma unica vez,
// por ciclo). 512 nao e "o numero certo" — nao ha um; e so mais folga antes de
// o mesmo corte voltar a acontecer com addons de sobra.
#define DECL_MAX 512
// Quantos itens cada fileira mostra. A home desenha no maximo MAX_CARDS (12) e
// buscar mais e trafego que ninguem ve.
#define MAX_POR_FILEIRA 12

// filsMontadas = as janelas do bloco QUE ESTA PUBLICADO, e nada alem disso.
// desc_remontar_fileiras republica este vetor por cima do bloco da tela sem
// trocar os itens, entao um `ini` daqui que foi calculado sobre OUTRO lote
// aponta para itens alheios. Medido na LG C9 (log da integracao com o Codex):
// a montagem descartada pela troca de geracao deixava aqui as janelas do lote
// que foi para o free(); o sync remontava por cima do catalogo do PACOTE e
// "Amigos assistindo" (ini=12 n=2) virava "The Martian"/"Project Hail Mary"
// sem nome. Por isso montar() monta em filsLote e so copia para ca junto com
// o cat_definir_tudo que publica aquele mesmo lote. tests/homejanelas.sh.
static CatFileira filsMontadas[CAT_FIL_MAX];
static int nFileirasMontadas;
static CatFileira filsLote[CAT_FIL_MAX];
static int nFilsLote;

typedef struct {
  char chave[192];      // homeCatalogKey:        <addonId>_<tipo>_<catalogoId>
  char desativar[352];  // homeCatalogDisableKey: <base>_<tipo>_<catalogoId>_<nome>
  char titulo[96];
  char tipo[8];
  char id[96];
  const char *base;
  // 1 quando o catalogo aceita BUSCA. O manifesto declara isso em
  // `extra: [{name:"search"}]` (formato novo) ou `extraSupported: ["search"]`
  // (antigo) — os addons do dono usam os dois.
  int buscavel;
  // 1 quando o manifesto marca o extra `search` como `isRequired` — ou seja, o
  // catalogo so responde a quem digitou um termo, e a home nao tem termo. Ele
  // continua servindo a BUSCA; o que ele nao pode e virar fileira. Ver a nota
  // em exigeBusca(), inclusive por que isto NAO vale para os outros extras.
  int exigeParam;
  char nomeAddon[64];   // "Xperience", para a linha "de <addon>" no resultado
} Decl;

// Preferencias do dono, o equivalente local de `homeCatalogPrefs`. Arquivo de
// texto porque o do app web e um localStorage de outro processo — a mesma razao
// que ja valia para o progresso: aquele arquivo pertence a quem o mantem aberto,
// e escrever nele de fora corromperia o estado.
//
//   ordem     <chave>
//   desligada <chave-ou-chave-de-desativar>
//   titulo    <chave><TAB><titulo>
#define PREF_MAX 64
static char prefOrdem[PREF_MAX][192];   static int nPrefOrdem;
static char prefOff[PREF_MAX][352];     static int nPrefOff;
static struct { char chave[192], titulo[96]; } prefTit[PREF_MAX];
static int nPrefTit;
static void lerPrefs(void) {
  char caminho[600], linha[600];
  FILE *f;
  nPrefOrdem = nPrefOff = nPrefTit = 0;
  if (!dirArteDesc[0]) return;
  snprintf(caminho, sizeof caminho, "%s/fileiras.txt", dirArteDesc);
  f = fopen(caminho, "r");
  if (!f) return;
  while (fgets(linha, sizeof linha, f)) {
    char *fim = linha + strlen(linha);
    char *arg;
    while (fim > linha && (fim[-1] == '\n' || fim[-1] == '\r')) *--fim = 0;
    if (!linha[0] || linha[0] == '#') continue;
    arg = strchr(linha, ' ');
    if (!arg) continue;
    *arg++ = 0;
    while (*arg == ' ') arg++;
    if (!strcmp(linha, "ordem") && nPrefOrdem < PREF_MAX) {
      snprintf(prefOrdem[nPrefOrdem++], 192, "%s", arg);
    } else if (!strcmp(linha, "desligada") && nPrefOff < PREF_MAX) {
      snprintf(prefOff[nPrefOff++], 352, "%s", arg);
    } else if (!strcmp(linha, "titulo") && nPrefTit < PREF_MAX) {
      char *tab = strchr(arg, '\t');
      if (!tab) continue;
      *tab++ = 0;
      snprintf(prefTit[nPrefTit].chave, 192, "%s", arg);
      snprintf(prefTit[nPrefTit].titulo, 96, "%s", tab);
      nPrefTit++;
    }
  }
  fclose(f);
  printf("[desc] prefs de fileira: %d na ordem, %d desligadas, %d renomeadas\n",
         nPrefOrdem, nPrefOff, nPrefTit);
}

// A conferencia e contra DUAS chaves, como no web: quem desliga pela tela de
// ajustes grava a chave de desativar (que carrega a URL base e o nome), e quem
// desliga pela ordenacao grava a chave curta.
// O catalogo JA APARECE DENTRO DE UMA PASTA DE COLECAO?
//
// Um addon como o Xperience declara centenas de catalogos, e as colecoes da
// conta agrupam justamente esses mesmos catalogos em pastas. Sem esta pergunta a
// home montava as DUAS coisas: a pasta (uma fileira de atalhos) e, soltos, os
// catalogos que estao dentro dela. O relato descreve exatamente isso — "em vez
// de manter cada colecao como uma fileira, o Nuvio cria fileiras extras para os
// conteudos de dentro" — e o efeito colateral e pior que a duplicata: com teto
// de 16 fileiras, as repetidas consomem as vagas e empurram para fora as
// colecoes que ainda nao entraram. Issue #18.
//
// So conta quando a colecao esta VISIVEL como fileira. Uma pasta cujo grupo foi
// desligado nao pode engolir o catalogo dela junto — senao desligar a colecao
// faria o conteudo sumir de vez, em vez de voltar a aparecer solto.
// Quantos catalogos a regra abaixo barrou JA NA DECLARACAO, no ciclo corrente.
// A linha [col] contava so o que era barrado na remontagem e por isso dizia
// "engolidas=0" enquanto o engolimento acontecia — mais cedo, em outro ponto.
// Um contador que mede metade do caminho ja me custou uma rodada.
static int engolidasNaDeclaracao;

// A VISIBILIDADE DO GRUPO FAZ PARTE DA PERGUNTA, E EU JA A TIREI UMA VEZ.
//
// Tentacao (minha, em 2026-09-09): o nome diz "esta dentro de uma colecao?" e
// o teste de visibilidade parece sobra. Tirei, e quebrei a home do dono na
// mesma tarde. O caso dele, direto do `fileirasui.txt` da C9:
//
//   linha collection_4fdc51c9-…                     1  0  1  Trending
//   linha app.xperience.…_movie_trending_movies     0  2  1  Trending - Filme
//
// (o primeiro numero e `oculta`.) Ele escondeu a COLECAO "Trending" e manteve
// a FILEIRA "Trending - Filme" ligada, na posicao dela, antes de "Directors".
// Sao duas escolhas independentes e as duas sao dele. Engolir o catalogo
// porque ele pertence a uma colecao escondida apaga a fileira que a pessoa
// deixou ligada de proposito — foi exatamente o que aconteceu.
//
// Entao a regra e: a colecao so representa o catalogo ENQUANTO ela aparece.
// Escondida, ela nao representa ninguem, e o catalogo volta a se representar.
// Isso NAO e o #18: la a queixa e sobre colecao dividida em varias fileiras
// com o grupo VISIVEL, e nesse caminho o teste abaixo devolve 1 e engole.
static int dentroDeColecaoVisivelBase(const char *base, const char *tipo,
                                      const char *id) {
  const ColFolder *f;
  char chaveGrupo[96];
  if (!base || !base[0]) return 0;
  f = col_por_catalogo(base, tipo, id);
  if (!f) return 0;
  col_chave_grupo(f->group, chaveGrupo, sizeof chaveGrupo);
  if (fil_oculta(chaveGrupo) || catordem_oculta(chaveGrupo, chaveGrupo)) return 0;
  return 1;
}

static int dentroDeColecaoVisivel(const Decl *d) {
  return dentroDeColecaoVisivelBase(d->base, d->tipo, d->id);
}

static int desligada(const Decl *d) {
  int i;
  if (dentroDeColecaoVisivel(d)) return 1;
  // A escolha feita NA TV (Ajustes -> Fileiras da Home) vem primeiro. Ela e
  // local de proposito e nunca sobe para a conta — a trava esta no topo de
  // catordem.h e repetida em fileiras.h. Sem esta precedencia, desligar uma
  // fileira aqui seria desfeito pelo proximo ciclo de sync.
  if (fil_oculta(d->chave)) return 1;
  // A conta manda junto com o arquivo local, nao no lugar dele: quem desligou
  // uma fileira no app web ve a TV concordar, e quem desligou so na TV
  // continua valendo.
  if (catordem_oculta(d->chave, d->desativar)) return 1;
  for (i = 0; i < nPrefOff; i++)
    if (!strcmp(prefOff[i], d->chave) || !strcmp(prefOff[i], d->desativar)) return 1;
  return 0;
}

// formatCatalogRowTitle (js/ui/screens/home/homeUtils.js:62): primeira letra
// maiuscula e, se o nome ja NAO termina com o rotulo do tipo, " - <tipo>".
// E por isso que a home mostra "For You - Filme" e nao "for you".
static void formatarTitulo(const char *nome, const char *tipo, char *dst, size_t tam) {
  const char *rotulo = i18n(ehCanal(tipo) ? "Canais"
                            : strcmp(tipo, "series") ? "Filme" : "S\xc3\xa9rie");
  const char *cru    = ehCanal(tipo) ? "Channels"
                     : strcmp(tipo, "series") ? "Movie" : "Series";
  size_t ln = strlen(nome), lr = strlen(rotulo), lc = strlen(cru);
  int jaTem = 0;
  if (!nome[0]) { snprintf(dst, tam, "%s", rotulo); return; }
  if (ln >= lr && !strcasecmp(nome + ln - lr, rotulo)) jaTem = 1;
  if (ln >= lc && !strcasecmp(nome + ln - lc, cru))    jaTem = 1;
  if (jaTem) snprintf(dst, tam, "%s", nome);
  else       snprintf(dst, tam, "%s - %s", nome, rotulo);
  if (dst[0] >= 'a' && dst[0] <= 'z') dst[0] = (char)(dst[0] - 32);
}

// Le <base>/manifest.json e acrescenta os catalogos declarados.
// `totalReal`, quando nao NULL, recebe quantos catalogos com tipo+id validos
// o manifesto declara DE VERDADE — inclusive os que passaram de `max` e nao
// couberam em `saida`. Sem isto nao ha como o chamador (a cota por addon, mais
// abaixo) DIZER quantos catalogos ficaram de fora por cota em vez de fingir
// que o addon so tinha `max` mesmo — issue #42(a), "some rows were not
// showing no matter what": a pessoa via o log dizer "8 catalogo(s)
// declarado(s)" para um addon que na verdade declara 40, sem nada apontando
// que os outros 32 foram cortados aqui e nao em lugar nenhum que ela pudesse
// mudar.

// --- MANIFESTOS BAIXADOS ANTES, E EM PARALELO ---------------------------------
//
// MEDIDO na LG: "manifestos lidos" chegava 7 s depois de "amigos assistindo",
// e nesses 7 s a home ja estava montada esperando — porque os manifestos so
// comecavam DEPOIS de o Trakt terminar, e ainda um de cada vez. Nenhuma das
// duas esperas e necessaria: o manifesto de um addon nao depende do Trakt nem
// do manifesto do addon do lado.
//
// Entao a busca comeca no PRIMEIRO instante de montar(), em fios proprios, e o
// laco de leitura (que continua sequencial, porque a COTA de declaracoes
// depende da ordem e da sobra do addon anterior) so espera o que ainda nao
// chegou. Com os 4 addons desta TV o download inteiro cabe dentro do tempo do
// Trakt.
//
// A lista de addons pode ser trocada pelo sync no meio do caminho: por isso a
// URL e COPIADA na largada e o corpo so e entregue a quem pedir exatamente
// aquela URL. Um addon trocado no meio simplesmente nao acha o seu corpo aqui
// e baixa como antes — mais lento, nunca errado.
#define MANI_MAX  12
#define MANI_FIOS  4
static struct {
  char  url[900];
  char *corpo;
  int   pronto;      // 1 = tentativa terminada (corpo pode ser NULL)
} mani[MANI_MAX];

// CACHE DE CORPO DE MANIFESTO ENTRE CICLOS.
//
// Sem isto, maniLargar() liberava os corpos da volta anterior e re-baixava
// TODOS os manifestos a cada desc_repetir() — inclusive os de addons
// desligados, que continuam sendo lidos porque a BUSCA os procura (ver
// lerManifesto). O custo e N HTTP GET por ciclo, e e o overhead que o dono
// viu no log como "recarrega tudo de novo, inclusive os desativados".
//
// A cache guarda uma COPIA propria (strdup) do corpo, key por URL + versao da
// lista de addons. A versao sobe a qualquer mudanca na lista (ligar/desligar,
// instalar, remover, addons_definir_lista), e como a posicao de um addon na
// lista pode mudar junto com a versao, a invalidacao por versao e a unica
// que e segura por posicao — e ela cobre o caso comum (sync periodico com a
// lista identica, desc_repetir disparado por troca de credencial Trakt): ai a
// versao nao muda, todas as URLs batem, e nenhum manifesto e re-baixado.
//
// LIMITE DE MEMORIA: so cacheia corpos menores que MANI_CACHE_BYTES. O
// Xperience declara 605 catalogos e o manifesto dele passa de 200 KB; num
// aparelho Samsung com 6,7 MiB de heap livre (medido, issue #69) guardar 12
// desses seria 2,4 MB persistentes — arriscado. Corpos grandes ficam de fora
// e re-baixam a cada ciclo, como antes; sao a minoria e o custo de baixa-los
// e o mesmo de hoje. Com o teto de 128 KB o pior caso e ~1,5 MB (12 x 128 KB),
// e na pratica ~300 KB (manifestos tipicos tem <30 KB).
#define MANI_CACHE_BYTES (128 * 1024)
static struct {
  char    url[900];
  unsigned versao;
  char   *corpo;
} maniCache[MANI_MAX];

static int  maniN;
static pthread_mutex_t maniTrava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  maniCond  = PTHREAD_COND_INITIALIZER;
static int  maniProx;
// A volta a que os fios pertencem. Um fio de uma volta ANTERIOR — que sobra
// quando a lista de addons muda no meio e ninguem pede aquele corpo — nao pode
// gravar em cima da tabela desta volta nem marcar `pronto` no lugar de outro
// addon. Ele descobre que ficou para tras aqui, joga o proprio download fora e
// sai.
static unsigned maniGeracao;

// --- cache de corpo de manifesto -------------------------------------------
// maniPegar/lerManifesto tomam posse do corpo e o liberam. A cache guarda uma
// COPIA propria (strdup) sob maniTrava, independente do ciclo de vida de
// mani[] — que e de uma volta so. As duas funcoes abaixo rodam sob maniTrava.
static int maniCacheAchar(const char *url, unsigned versao) {
  int i;
  for (i = 0; i < MANI_MAX; i++)
    if (maniCache[i].corpo && maniCache[i].versao == versao &&
        !strcmp(maniCache[i].url, url))
      return i;
  return -1;
}

// Coloca `corpo` (já alocado) na cache, associado a `url`+`versao`. Se a cache
// estiver cheia, reutiliza a entrada mais antiga (versao diferente ou a primeira
// com corpo). Devolve 1 se guardou. Nao duplica `corpo`: toma posse dele.
static int maniCacheColocar(const char *url, unsigned versao, char *corpo) {
  int i, alvo = -1;
  if (!corpo) return 0;
  for (i = 0; i < MANI_MAX; i++) {
    if (!maniCache[i].corpo) { alvo = i; break; }
    // Entrada com versao diferente e stale: reutiliza.
    if (maniCache[i].corpo && maniCache[i].versao != versao) { alvo = i; break; }
  }
  if (alvo < 0) alvo = 0;   // todas vivas: sobrescreve a primeira (LRU simples)
  free(maniCache[alvo].corpo);
  maniCache[alvo].corpo = corpo;
  maniCache[alvo].versao = versao;
  snprintf(maniCache[alvo].url, sizeof maniCache[alvo].url, "%s", url);
  return 1;
}

// API PUBLICA (descoberta.h) — unificacao com a sonda de addons.c. Mesma
// tabela, mesma trava; so a copia que sai/entra muda de dono.
char *desc_manifesto_cache_obter(const char *url, unsigned versao) {
  char *copia = NULL;
  int i;
  pthread_mutex_lock(&maniTrava);
  i = maniCacheAchar(url, versao);
  if (i >= 0) {
    size_t n = strlen(maniCache[i].corpo);
    copia = malloc(n + 1);
    if (copia) memcpy(copia, maniCache[i].corpo, n + 1);
  }
  pthread_mutex_unlock(&maniTrava);
  return copia;
}

void desc_manifesto_cache_guardar(const char *url, unsigned versao, const char *corpo) {
  size_t n;
  char *copia;
  if (!corpo || !*corpo) return;
  n = strlen(corpo);
  if (n >= MANI_CACHE_BYTES) return;   // mesmo teto de memoria do laco interno
  copia = malloc(n + 1);
  if (!copia) return;
  memcpy(copia, corpo, n + 1);
  pthread_mutex_lock(&maniTrava);
  maniCacheColocar(url, versao, copia);   // toma posse da copia, nao do `corpo` recebido
  pthread_mutex_unlock(&maniTrava);
}

// Libera toda a cache. Chamada no logout (desc_esquecer) para nao vazar entre
// contas.
static void maniCacheLimpar(void) {
  int i;
  pthread_mutex_lock(&maniTrava);
  for (i = 0; i < MANI_MAX; i++) {
    free(maniCache[i].corpo);
    maniCache[i].corpo = NULL;
    maniCache[i].url[0] = 0;
    maniCache[i].versao = 0;
  }
  pthread_mutex_unlock(&maniTrava);
}

static void *fioManifesto(void *u) {
  unsigned minha = (unsigned)(uintptr_t)u;
  for (;;) {
    int meu;
    char *corpo;
    char url[900];
    pthread_mutex_lock(&maniTrava);
    if (minha != maniGeracao || maniProx >= maniN) {
      pthread_mutex_unlock(&maniTrava); return NULL;
    }
    meu = maniProx++;
    snprintf(url, sizeof url, "%s", mani[meu].url);
    pthread_mutex_unlock(&maniTrava);
    corpo = rede_baixar(url, 20);
    pthread_mutex_lock(&maniTrava);
    if (minha != maniGeracao) {
      pthread_mutex_unlock(&maniTrava); free(corpo); return NULL;
    }
    mani[meu].corpo = corpo;
    mani[meu].pronto = 1;
    // ARMAZENA NA CACHE uma copia propria, se o corpo couber no teto de
    // memoria. A copia vive enquanto a versao da lista nao mudar; o corpo
    // original e consumido por maniPegar/lerManifesto e liberado por eles.
    if (corpo) {
      size_t n = strlen(corpo);
      if (n < MANI_CACHE_BYTES) {
        char *copia = malloc(n + 1);
        if (copia) { memcpy(copia, corpo, n + 1); maniCacheColocar(url, addons_versao(), copia); }
      }
    }
    pthread_cond_broadcast(&maniCond);
    pthread_mutex_unlock(&maniTrava);
  }
}

// Larga o download de todos os manifestos. Volta na hora.
static void maniLargar(void) {
  int nAd = addons_n(), i, criados = 0;
  unsigned versao = addons_versao();
  pthread_t fios[MANI_FIOS];
  pthread_mutex_lock(&maniTrava);
  // Sobra da volta anterior (ninguem pediu, addon trocado no meio): nao pode
  // virar vazamento nem ser entregue como se fosse desta volta.
  for (i = 0; i < maniN; i++) { free(mani[i].corpo); mani[i].corpo = NULL; }
  maniN = nAd > MANI_MAX ? MANI_MAX : nAd;
  for (i = 0; i < maniN; i++) {
    int cache;
    snprintf(mani[i].url, sizeof mani[i].url, "%s/manifest.json", addons_base(i));
    mani[i].pronto = 0;
    // CACHE HIT: o manifesto deste addon ja foi baixado numa volta com a
    // MESMA versao da lista (lista inalterada). Reaproveita sem rede. A
    // copia da cache e strdup para mani[i].corpo; a cache mantem a sua e
    // vive para a proxima volta. maniPegar/lerManifesto vao liberar a copia
    // que sai daqui, nao a da cache.
    cache = maniCacheAchar(mani[i].url, versao);
    if (cache >= 0) {
      size_t n = strlen(maniCache[cache].corpo);
      char *copia = malloc(n + 1);
      if (copia) {
        memcpy(copia, maniCache[cache].corpo, n + 1);
        mani[i].corpo = copia;
        mani[i].pronto = 1;
      }
    }
  }
  maniProx = 0;
  maniGeracao++;
  // Ninguem mais espera por um `pronto` que a volta passada deixou pendente.
  pthread_cond_broadcast(&maniCond);
  { unsigned g = maniGeracao;
    pthread_mutex_unlock(&maniTrava);
  if (maniN < 1) return;
  // So dispara fios para os slots que NAO vieram da cache. Os outros ja
  // estao prontos e maniPegar os entrega na hora.
  for (i = 0; i < MANI_FIOS && i < maniN; i++) {
    if (mani[i].pronto) continue;
    if (pthread_create(&fios[criados], NULL, fioManifesto,
                       (void *)(uintptr_t)g) == 0) {
      pthread_detach(fios[criados]);
      criados++;
    }
  }
  // Sem fio nenhum o corpo fica NULL e `pronto` fica 0: maniPegar percebe que
  // ninguem esta baixando e baixa no proprio fio, como sempre foi.
  if (!criados) {
    pthread_mutex_lock(&maniTrava);
    maniN = 0;
    pthread_cond_broadcast(&maniCond);
    pthread_mutex_unlock(&maniTrava);
  }
  }   // fecha o bloco `{ unsigned g = maniGeracao;`
}

// O corpo do manifesto de `url`, esperando o download largado por maniLargar se
// ele ainda estiver em curso. A posse passa para quem chamou.
static char *maniPegar(const char *url) {
  int i;
  char *corpo = NULL;
  pthread_mutex_lock(&maniTrava);
  for (i = 0; i < maniN; i++) if (!strcmp(mani[i].url, url)) break;
  if (i < maniN) {
    unsigned g = maniGeracao;
    // A espera acaba tambem quando a volta vira: nesse caso o corpo daqui nao
    // serve mais a ninguem e quem chamou baixa por conta propria.
    while (!mani[i].pronto && g == maniGeracao)
      pthread_cond_wait(&maniCond, &maniTrava);
    if (g == maniGeracao) { corpo = mani[i].corpo; mani[i].corpo = NULL; }
  }
  pthread_mutex_unlock(&maniTrava);
  return corpo;
}

// CATALOGO DE BUSCA NAO PODE VIRAR FILEIRA.
//
// O protocolo Stremio marca em cada `extra` se ele e obrigatorio:
//   "extra":[{"name":"search","isRequired":true}]
// Um catalogo que EXIGE `search` responde vazio ao pedido sem termo que a home
// faz — e a home nao tem termo nenhum para dar. Nada aqui lia esse campo, entao
// esses catalogos viravam fileira, gastavam uma vaga do teto e voltavam vazios
// TODA VEZ, as mesmas. Medido na LG, em toda sessao, com uma delas tendo ate
// ganhado a "vaga garantida" do addon:
//   [desc] vaga garantida: AIOStreams entra em 11 (Debridio TMDB - Search - Filme)
//   [desc] catalogo vazio: Debridio TMDB - Search - Filme
// E parte do "some rows were not showing no matter what" do #42. Eles continuam
// servindo a BUSCA, que e para o que existem — so deixam de fingir que sao
// fileira.
//
// SO `search`, E ISSO E UMA CORRECAO DE UMA REGRA MINHA ANTERIOR. A primeira
// versao cortava QUALQUER extra obrigatorio, e MEDIDO na LG isso derrubou 247
// dos 286 catalogos: addon real declara `{"name":"genre","isRequired":true}` e
// mesmo assim responde a consulta sem genero nenhum. "Christian Bale - Filme"
// era uma dessas — vinha com 12 titulos no log da sessao anterior e sumiu da
// home. Declarar obrigatorio NAO e o mesmo que recusar sem o parametro; a
// unica exigencia que o app sabe que nao consegue satisfazer e o termo de
// busca, porque ele so existe quando alguem digita.
//
// Percorre os objetos de `extra` na faixa deste catalogo. `extraSupported` (o
// formato antigo) e uma lista de STRINGS, sem obrigatoriedade nenhuma.
static int exigeBusca(const char *p, const char *f) {
  const char *ex = strstr(p, "\"extra\"");
  const char *o;
  if (!ex || ex >= f) return 0;
  o = strchr(ex, '[');
  if (!o || o >= f) return 0;
  for (o = strchr(o, '{'); o && o < f; o = strchr(o + 1, '{')) {
    const char *fo = strchr(o, '}');
    const char *req, *nome;
    if (!fo || fo > f) break;
    req = strstr(o, "\"isRequired\"");
    if (req && req < fo) {
      // Aceita `true` e `"true"`: ha manifesto que escreve o booleano como
      // texto, e recusar so o primeiro deixaria o defeito de pe para ele.
      const char *v = req + 12;
      while (*v && (*v == ':' || *v == ' ' || *v == '"')) v++;
      if (!strncmp(v, "true", 4)) {
        // O `name` DESTE extra, e nao qualquer "search" na faixa: um catalogo
        // com `genre` obrigatorio e `search` opcional continua sendo fileira.
        nome = strstr(o, "\"name\"");
        if (nome && nome < fo) {
          const char *w = nome + 6;
          while (*w && (*w == ':' || *w == ' ' || *w == '"')) w++;
          if (!strncmp(w, "search", 6)) return 1;
        }
      }
    }
  }
  return 0;
}

// NOMES DOS CATALOGOS DECLARADOS, por (base, tipo, id) — issue #76. A colecao
// da conta pode vir SEM o titulo da fonte (o export do Xperience manda so
// addonId/type/catalogId), e a pagina da colecao mostrava "mdblist.13914 ·
// Movies" na aba. O manifesto tem o nome; ele passa por aqui a cada volta da
// descoberta, entao fica guardado para quem perguntar. Anel de 512: o
// Xperience sozinho declara 605, mas os que a cota deixa passar sao os que a
// conta pode citar, e o anel gira em vez de recusar.
#define NOMECAT_MAX 512
static struct { char base[300], tipo[8], id[96], nome[96]; } nomeCat[NOMECAT_MAX];
static int nNomeCat, nomeCatProx;
static pthread_mutex_t nomeCatTrava = PTHREAD_MUTEX_INITIALIZER;
static void registrarNomeCatalogo(const char *base, const char *tipo, const char *id, const char *nome) {
  int i;
  if (!base || !nome || !nome[0]) return;
  pthread_mutex_lock(&nomeCatTrava);
  for (i = 0; i < nNomeCat; i++)
    if (!strcmp(nomeCat[i].base, base) && !strcmp(nomeCat[i].tipo, tipo) && !strcmp(nomeCat[i].id, id)) break;
  if (i == nNomeCat) { i = nomeCatProx; nomeCatProx = (nomeCatProx + 1) % NOMECAT_MAX; if (nNomeCat < NOMECAT_MAX) nNomeCat++; }
  snprintf(nomeCat[i].base, sizeof nomeCat[i].base, "%s", base);
  snprintf(nomeCat[i].tipo, sizeof nomeCat[i].tipo, "%s", tipo);
  snprintf(nomeCat[i].id, sizeof nomeCat[i].id, "%s", id);
  snprintf(nomeCat[i].nome, sizeof nomeCat[i].nome, "%s", nome);
  pthread_mutex_unlock(&nomeCatTrava);
}
const char *desc_nome_catalogo(const char *base, const char *tipo, const char *id) {
  static char saida[96];
  int i;
  saida[0] = 0;
  if (!base || !base[0] || !tipo || !id) return saida;
  pthread_mutex_lock(&nomeCatTrava);
  for (i = 0; i < nNomeCat; i++)
    if (!strcmp(nomeCat[i].base, base) && !strcmp(nomeCat[i].tipo, tipo) && !strcmp(nomeCat[i].id, id)) {
      snprintf(saida, sizeof saida, "%s", nomeCat[i].nome); break; }
  pthread_mutex_unlock(&nomeCatTrava);
  return saida;
}

static int lerManifesto(int iAddon, const char *base, Decl *saida, int max,
                         int *totalReal) {
  char url[900], addonId[96] = "", nome[96], tipo[8], id[96];
  char *corpo;
  const char *p, *fim;
  int n = 0, total = 0;
  snprintf(url, sizeof url, "%s/manifest.json", base);
  // Ja largado em paralelo no comeco de montar(); so cai na rede aqui quando
  // este addon nao estava na lista daquele instante.
  corpo = maniPegar(url);
  if (!corpo) corpo = rede_baixar(url, 20);
  if (!corpo) return 0;
  fim = corpo + strlen(corpo);
  // O MESMO CORPO SERVE AOS DOIS LEITORES. Ver addons_manifesto_lido: sem esta
  // linha o id e as capacidades do addon so eram aprendidos por quem abrisse a
  // tela de addons nos Ajustes, e sem o id nenhuma colecao da conta encontrava
  // a URL do proprio addon (issue #10).
  addons_manifesto_lido(iAddon, corpo);
  // Chave da RAIZ: um manifesto tem "id" tambem dentro de catalogs[] e de
  // behaviorHints, e ha addon que escreve "catalogs" antes de "id" — a leitura
  // crua trazia o id de um CATALOGO como se fosse o do addon. Ver js_texto_raiz.
  js_texto_raiz(corpo, "id", addonId, sizeof addonId);
  p = js_array(corpo, fim, "catalogs");
  // Sem `n < max` na condicao: o vetor de fileiras pode encher, mas a varredura
  // continua ate o fim do manifesto porque os catalogos de BUSCA costumam estar
  // no fim dele (o Xperience poe os dele em 603/604 de 605). Quem para de
  // gravar e o `if (n < max)` la dentro.
  while (p) {
    const char *f = js_fim(p);
    tipo[0] = id[0] = nome[0] = 0;
    js_texto(p, f, "type", tipo, sizeof tipo);
    js_texto(p, f, "id",   id,   sizeof id);
    // RAIZ DO OBJETO, e nao a primeira ocorrencia na faixa. O Bingecat escreve
    // `extra: [{name:"skip"}, {name:"genre"}, ...]` ANTES de `name`, e js_texto
    // devolvia o nome do primeiro extra: as fileiras dele apareciam como "Skip
    // - Filme", "Genre - Série" e "Search - Filme" em Ajustes e no log. Foi
    // esse rotulo, e nao o manifesto, que fez a issue #24 parecer um caso de
    // "catalogo que exige parametro" — "Search - Movie" era um catalogo
    // qualquer cujo primeiro extra se chamava search. Mesma armadilha ja
    // descrita para o "id" do addon (js_texto_raiz), um nivel abaixo.
    js_texto_raiz_em(p, f, "name", nome, sizeof nome);
    // Sem tipo ou sem id nao da para montar a URL do catalogo; e um catalogo
    // que nao responde e pior que uma fileira a menos.
    if (tipo[0] && id[0]) {
      Decl local, *d;
      total++;
      registrarNomeCatalogo(base, tipo, id, nome);
      // Vetor cheio: usa um Decl de rascunho so para decidir/registrar a busca.
      d = (n < max) ? &saida[n] : &local;
      memset(d, 0, sizeof *d);
      d->base = base;
      // BUSCA: procura "search" dentro do bloco `extra`/`extraSupported` DESTE
      // catalogo (a faixa [p,f) e o objeto dele, entao nao vaza para o vizinho).
      //
      // So filme e serie. O Akashi declara busca em `event` e `channel`
      // tambem, e o AIOStreams em `collections` — tipos que este app nao tem
      // tela para mostrar. Consultar seria trafego que nao vira nada.
      { const char *ex = strstr(p, "\"extra\"");
        if (!ex || ex >= f) ex = strstr(p, "\"extraSupported\"");
        if (ex && ex < f) {
          const char *sc = strstr(ex, "\"search\"");
          if (sc && sc < f) d->buscavel = 1;
        }
        // CANAL TAMBEM E BUSCAVEL. O que continua de fora e `event` e
        // `collections`, que nao tem tela. Canal tem: o card abre a pagina de
        // titulo e o pedido de stream ja sai com o tipo certo
        // (addons_buscar_streams preserva `tipo`), entao procurar "ESPN" e uma
        // pergunta que este app sabe responder.
        if (strcmp(tipo, "movie") && strcmp(tipo, "series") && !ehCanal(tipo))
          d->buscavel = 0;
        // Registra AQUI, e nao depois varrendo o vetor de Decl.
        //
        // O Xperience declara 605 catalogos e poe os dois de BUSCA nas duas
        // ULTIMAS posicoes (603 e 604). Qualquer teto no vetor de fileiras da
        // home — 64, 256, o numero que for — corta exatamente os catalogos que
        // interessam a busca. Os dois assuntos nao tem por que compartilhar
        // limite: sao 16 alvos de busca contra centenas de fileiras.
        if (d->buscavel) {
          char rotulo[96], nomeAddon[96] = "";
          // Raiz do MANIFESTO: "name" tambem existe em cada catalogs[] e ha
          // manifesto que escreve catalogs antes de name (ver addons.c).
          js_texto_raiz(corpo, "name", nomeAddon, sizeof nomeAddon);
          formatarTitulo(nome, tipo, rotulo, sizeof rotulo);
          desc_alvo_busca(base, tipo, id, rotulo,
                          nomeAddon[0] ? nomeAddon
                                       : (addonId[0] ? addonId : "addon"));
        } }
      d->exigeParam = exigeBusca(p, f);
      snprintf(d->tipo, sizeof d->tipo, "%s", tipo);
      snprintf(d->id,   sizeof d->id,   "%s", id);
      snprintf(d->chave, sizeof d->chave, "%s_%s_%s",
               addonId[0] ? addonId : base, tipo, id);
      snprintf(d->desativar, sizeof d->desativar, "%s_%s_%s_%s", base, tipo, id, nome);
      formatarTitulo(nome, tipo, d->titulo, sizeof d->titulo);
      // Nome legivel do addon, para a linha "de <addon>" sob o titulo da
      // fileira de resultados. O manifesto tem `name`; sem ele fica o id.
      { char an[96] = "";
        js_texto_raiz(corpo, "name", an, sizeof an);
        snprintf(d->nomeAddon, sizeof d->nomeAddon, "%s",
                 an[0] ? an : (addonId[0] ? addonId : "addon")); }
      if (n < max) n++;
    }
    p = js_prox(f);
  }
  free(corpo);
  if (totalReal) *totalReal = total;
  return n;
}

// --- LEITURA DOS CATALOGOS EM PARALELO ---------------------------------------
//
// Eram ate 16 GET em SERIE, 25 s de timeout cada. Medido no Mac: 8,7 s entre a
// primeira fileira aparecer (4,0 s) e o catalogo ficar completo (12,8 s), e na
// TV e pior. Sao pedidos INDEPENDENTES — nada em lerCatalogo/deMeta toca estado
// compartilhado (a tabela de generos e const), e rede_baixar ja roda em tres
// fios na busca.
//
// A ORDEM DAS FILEIRAS TEM DE SER PRESERVADA: ela sai de art/fileiras.txt e e
// preferencia do dono. Por isso os fios escrevem cada um no SEU balde e quem
// monta caminha na ordem, esperando o balde k ficar pronto. O resultado e a
// mesma ordem de antes, com o tempo do MAIOR pedido em vez da SOMA.
#define CAT_FIOS 3

typedef struct {
  const Decl *d;
  CatItem itens[MAX_POR_FILEIRA];
  int  n;
  int  respondeu;
  int  pronto;
} TarefaCat;

// Recupera a ultima resposta boa da mesma fileira. O catalogo publicado pode
// ser o snapshot do arranque anterior; manter a janela por CHAVE evita que um
// timeout transforme uma resposta parcial em uma reordenacao visual. A copia e
// feita antes da proxima publicacao, quando os indices ainda apontam para o
// bloco atualmente desenhado.
static int linhaAnterior(const char *chave, CatItem *saida, int max,
                         CatFileira *meta) {
  // The live catalogue can belong to a previous profile even while that
  // profile's add-on URL remains configured. Without the accepted snapshot
  // for the current owner/profile/config, none of its item bodies are safe to
  // reuse.
  if (!homeestado_contexto_valido() || !homeestado_tem_fileira(chave)) return 0;
  return cat_copiar_fileira(chave, saida, max, meta);
}

static int fileiraMontada(const CatFileira *v, int n, const char *chave) {
  int i;
  for (i = 0; i < n; i++) if (!strcmp(v[i].chave, chave)) return 1;
  return 0;
}

static int fileiraPodeSerPreservada(const CatFileira *f) {
  return f && f->chave[0] && homeestado_contexto_valido() &&
         homeestado_tem_fileira(f->chave);
}

static void ordenarPorSnapshot(CatFileira *fil, int n) {
  int i;
  if (!fil || n < 2 || !homeestado_contexto_valido()) return;
  for (i = 1; i < n; i++) {
    CatFileira atual = fil[i];
    int rank = homeestado_ordem_fileira(atual.chave), j = i;
    while (j > 0) {
      int anterior = homeestado_ordem_fileira(fil[j - 1].chave);
      if (anterior < 0 || (rank >= 0 && anterior <= rank)) break;
      fil[j] = fil[j - 1];
      j--;
    }
    fil[j] = atual;
  }
}

// Manifesto ausente/timeout nao e uma ordem para apagar a estrutura anterior.
// Reanexa as linhas ainda nao produzidas nesta rodada, mantendo a chave e os
// itens bons do snapshot. Linhas novas continuam entrando apenas quando foram
// declaradas e responderam nesta rodada.
static void preservarFileirasAusentes(CatItem **lote, int *n, int *cap,
                                       CatFileira *fil, int *nFil) {
  int r;
  CatItem tmp[MAX_POR_FILEIRA];
  if (!lote || !*lote || !n || !cap || !fil || !nFil) return;
  for (r = 0; r < cat_n_fileiras() && *nFil < CAT_FIL_MAX; r++) {
    const CatFileira *old = cat_fileira(r);
    int got, need;
    if (!old || !old->chave[0] || fileiraMontada(fil, *nFil, old->chave)) continue;
    // Reuse bytes only from the accepted snapshot for this exact owner,
    // profile, and configuration. A still-configured URL does not make items
    // from the previous profile safe to copy.
    if (!fileiraPodeSerPreservada(old)) continue;
    got = linhaAnterior(old->chave, tmp, MAX_POR_FILEIRA, NULL);
    if (!got && !old->estado) continue;
    need = *n + got;
    if (need > *cap) {
      int alvo = *cap ? *cap * 2 : 128;
      while (alvo < need) alvo *= 2;
      CatItem *novo = realloc(*lote, sizeof(CatItem) * (size_t)alvo);
      if (!novo) return;
      *lote = novo; *cap = alvo;
    }
    if (got) memcpy(*lote + *n, tmp, sizeof(CatItem) * (size_t)got);
    fil[*nFil] = *old;
    fil[*nFil].ini = *n; fil[*nFil].n = got;
    (*n) += got; (*nFil)++;
  }
}

static TarefaCat *tarefas;
static int  nTarefas, proximaTarefa;
static pthread_mutex_t catTrava = PTHREAD_MUTEX_INITIALIZER;

static void *fioCatalogo(void *u) {
  (void)u;
  for (;;) {
    int meu, got, respondeu = 0;
    const Decl *d;
    pthread_mutex_lock(&catTrava);
    if (proximaTarefa >= nTarefas) { pthread_mutex_unlock(&catTrava); return NULL; }
    meu = proximaTarefa++;
    d = tarefas[meu].d;
    pthread_mutex_unlock(&catTrava);

    got = lerCatalogo(d->base, d->tipo, d->id, tarefas[meu].itens,
                      MAX_POR_FILEIRA, MAX_POR_FILEIRA, &respondeu);

    pthread_mutex_lock(&catTrava);
    tarefas[meu].n = got;
    tarefas[meu].respondeu = respondeu;
    tarefas[meu].pronto = 1;
    pthread_mutex_unlock(&catTrava);
  }
}

// "Continuar assistindo" a partir do progresso local (progresso.c), no mesmo
// formato que trakt_continuar devolve: imdb (composto em serie), tipo,
// porcentagem, temporada/episodio — e o resto vem do Cinemeta pelo mesmo
// enfeite. Mais recente primeiro (prog_ler ja ordena). Entra o que esta entre
// 1% e 90%, os mesmos limites de home_registrar_retorno; titulo terminado nao
// e "continuar". O proximo episodio de uma serie terminada fica para depois.
static int continuarLocal(CatItem *saida, int max) {
  static ProgRegistro regs[PROG_MAX];
  int k, i, n = 0;
  k = prog_ler(regs, PROG_MAX);
  for (i = 0; i < k && n < max; i++) {
    const ProgRegistro *r = &regs[i];
    CatItem *d;
    double p;
    int j, repetido = 0;
    if (r->durSeg < 60.0) continue;
    p = r->posSeg / r->durSeg;
    if (p < 0.01 || p >= 0.90) continue;
    // Uma serie com varios episodios gravados entra UMA vez, no mais recente.
    for (j = 0; j < n; j++) {
      const char *dp = strchr(saida[j].imdb, ':');
      size_t L = dp ? (size_t)(dp - saida[j].imdb) : strlen(saida[j].imdb);
      if (L == strlen(r->contentId) && !strncmp(saida[j].imdb, r->contentId, L)) { repetido = 1; break; }
    }
    if (repetido) continue;
    d = &saida[n];
    memset(d, 0, sizeof *d);
    d->progresso = (int)(100.0 * p);
    // O instante ja esta aqui, no registro: carimbar agora poupa a busca por
    // chave que instanteDaConta faria depois, e sobrevive a compactacao do
    // trakt_enfeitar_lote. Ver retomadoMs em catalogo.h.
    d->retomadoMs = r->lastWatchedMs;
    if (r->episodio > 0) {
      d->temporada = r->temporada;
      d->episodio  = r->episodio;
      snprintf(d->imdb, sizeof d->imdb, "%s:%d:%d", r->contentId,
               r->temporada ? r->temporada : 1, r->episodio);
      snprintf(d->tipo, sizeof d->tipo, "series");
    } else {
      snprintf(d->imdb, sizeof d->imdb, "%s", r->contentId);
      snprintf(d->tipo, sizeof d->tipo, "movie");
    }
    n++;
  }
  n = trakt_enfeitar_lote(saida, n);
  if (n) printf("[desc] continuar assistindo local: %d\n", n);
  return n;
}

// --- "CONTINUAR ASSISTINDO": AS DUAS FONTES, UNIDAS ---------------------------
//
// O DEFEITO (issue #5, "Nuvio Sync Issue"). A linha era
//   `if (nContinuar == 0 && !trakt_ativo()) nContinuar = continuarLocal(...)`
// ou seja: com o Trakt vinculado a fileira saia EXCLUSIVAMENTE do Trakt e o
// progresso da CONTA Nuvio — que e o que chega do celular da pessoa, via
// syncprog.c -> progresso.c, e o que continuarLocal le — era simplesmente
// ignorado. As duas metades do relato sao essa unica linha:
//   "assisto no celular e na TV nao aparece em Continuar assistindo"
//     -> o registro da conta existia no disco e nunca era lido;
//   "aparecem filmes e series que eu nunca assisti"
//     -> vinham do /sync/playback antigo do Trakt, que guarda tudo que algum
//        cliente Trakt ja pausou, e o caminho do Trakt NAO aplicava os limites
//        de 1% a 90% que o caminho local sempre aplicou.
//
// POR QUE A UNIAO E A RESPOSTA CERTA, e nao escolher uma fonte: as duas
// respondem a mesma pergunta sobre universos DIFERENTES. O Trakt sabe o que a
// pessoa assistiu em qualquer cliente Trakt; a conta Nuvio sabe o que ela
// assistiu nos aparelhos Nuvio dela. Escolher uma joga fora metade do
// historico, e foi isso que aconteceu — em qualquer das duas direcoes o dono
// perde. Deduplicar por imdb e ordenar pelo instante mais recente da o que ele
// pediu: uma fileira so, com o que ele assistiu, onde quer que tenha assistido.
//
// QUANDO AS DUAS DISCORDAM DO MESMO TITULO, A MAIS RECENTE GANHA. E ai esta a
// aproximacao que sobra: `trakt_continuar` NAO expoe o `paused_at` que vem no
// JSON de /sync/playback, entao o instante de um item do Trakt so e conhecido
// quando existe um registro local com a mesma chave (prog_por_chave). Sem
// instante, o item do Trakt perde do que tem instante e mantem a posicao
// relativa que o Trakt lhe deu. Expor `paused_at` em trakt.c troca esta regra
// por uma comparacao exata; ate la, dado desconhecido ordena DEPOIS do
// conhecido em vez de virar um instante inventado.
// 12 e nao 8 (19/09, issue #66): com os itens "a seguir" do Trakt entrando
// alem dos pausados, 8 lugares eram todos dos pausados e o "a seguir" nunca
// aparecia. trakt_continuar ordena por instante antes de entregar.
#define CONT_MAX 12

// Os limites que o caminho local sempre teve e o do Trakt nao: os mesmos de
// home_registrar_retorno. Abaixo de 1% nao se comecou, de 90% em diante
// acabou — e "continuar" nao e nem uma coisa nem a outra.
static int emAndamento(int pct) { return pct >= 1 && pct < 90; }

// QUANDO este item foi visto por ultimo, em ms. 0 = nao se sabe.
//
// Tres fontes, na ordem de confianca:
//   1. o instante que veio COM o item. O do Trakt e o `paused_at` do
//      /sync/playback; o da conta e o last_watched que o syncprog ja
//      reconciliou entre celular e TV. Os dois sao carimbados na origem.
//   2. o registro LOCAL desta obra, quando o item nao trouxe instante — e o
//      caso de um item do Trakt que este aparelho tambem assistiu.
//   3. nada. Ai o desempate e a ordem que a propria fonte deu, e nao uma data
//      inventada (ver o campo `ord` em montarContinuar).
//
// O passo 2 existia sozinho antes, e era insuficiente: um titulo que a pessoa
// assiste SO em outro aparelho e por outro cliente Trakt nao tem registro local
// nenhum, entao TODO item do Trakt entrava com instante 0 e a fileira ordenava
// pela ordem de resposta.
static long long instanteDaConta(const CatItem *c) {
  ProgRegistro r;
  char id[24], chave[48];
  int temp = c->temporada, ep = c->episodio;
  if (c->retomadoMs > 0) return c->retomadoMs;
  prog_content_id(id, sizeof id, c->imdb, &temp, &ep);
  prog_chave(chave, sizeof chave, id, temp, ep);
  if (!prog_por_chave(chave, &r)) return 0;
  return r.lastWatchedMs;
}

// 1 quando os dois itens sao a MESMA OBRA. O teste e o de continuarLocal:
// compara so a parte antes do ':', porque "tt123:4:9" e "tt123:1:2" sao dois
// episodios da mesma serie e a fileira mostra a serie uma vez.
static int mesmaObra(const CatItem *a, const CatItem *b) {
  char ia[24], ib[24];
  prog_content_id(ia, sizeof ia, a->imdb, NULL, NULL);
  prog_content_id(ib, sizeof ib, b->imdb, NULL, NULL);
  return ia[0] && !strcmp(ia, ib);
}

// Um candidato da fileira, com o que se sabe sobre QUANDO ele aconteceu.
typedef struct { CatItem *item; long long ms; int ord; } Cand;

// A fileira tambem e refeita FORA do ciclo completo (issue #38), pelo fio de
// desc_refazer_continuar. Os buffers estaticos abaixo — e os de
// trakt_enfeitar_lote, chamado tambem por trakt_social — nao admitem dois
// montadores ao mesmo tempo, entao a trava cobre os DOIS usos em montar().
static pthread_mutex_t contTrava = PTHREAD_MUTEX_INITIALIZER;

// Aplica a um lote REMOTO (Trakt ou Simkl) os limites de 1% a 90% e o
// cruzamento com o registro local mais novo. Compacta no lugar; devolve quantos
// ficaram. `aSeguir` diz quais itens sao "a seguir" (entram com 0%).
static int filtrarRemoto(CatItem *v, int n, int (*aSeguir)(const char *),
                         int *fora) {
  int i, w;
  for (i = 0, w = 0; i < n; i++) {
    // "A SEGUIR" (issue #66) entra com 0%: e o proximo episodio de uma serie
    // cujo ultimo terminou. Nao e "pausado", mas e "continuar".
    if (!aSeguir(v[i].imdb) && !emAndamento(v[i].progresso)) { (*fora)++; continue; }
    // O REGISTRO LOCAL MAIS NOVO VENCE A RESPOSTA REMOTA. Sem este cruzamento
    // a refazagem da fileira (issue #38) lia um /sync/playback que ainda nao
    // recebeu o scrobble que acabamos de mandar: o titulo terminado voltava a
    // aparecer como "em andamento" por alguns minutos. O desempate e por
    // instante — um registro local mais VELHO que o paused_at remoto nao
    // manda em nada.
    { ProgRegistro r; char id[24], chave[48];
      int tt = v[i].temporada, ee = v[i].episodio;
      prog_content_id(id, sizeof id, v[i].imdb, &tt, &ee);
      prog_chave(chave, sizeof chave, id, tt, ee);
      if (prog_por_chave(chave, &r) && r.durSeg > 1.0 &&
          r.lastWatchedMs > v[i].retomadoMs) {
        int pct = (int)(100.0 * r.posSeg / r.durSeg);
        // "A SEGUIR" ABERTO E LARGADO NO COMECO NAO SAI DA FILEIRA. Visto na
        // C9 em 22/09: abrir o "Up next" de Adolescence (S1E2) e voltar aos 30 s
        // gravou 0,8% local, mais novo que o Trakt; o pct virava 0, caia fora
        // de 1-90% e a serie SUMIA do Continuar assistindo — o proximo
        // episodio que o app acabara de oferecer. Abaixo de 1% ele continua
        // sendo "a seguir" (progresso 0); do fim para cima (>90%) sai, como
        // antes, porque ai terminou. Vale para Trakt e Simkl (aSeguir).
        if (pct < 1 && aSeguir(v[i].imdb)) pct = 0;
        else if (!emAndamento(pct)) { (*fora)++; continue; }
        v[i].progresso = pct;
      } }
    if (w != i) v[w] = v[i];
    w++;
  }
  return w;
}

static int montarContinuar(CatItem *saida, int max) {
  // static: dois lotes de 12 CatItem passam de 350 KB e montar() roda uma vez,
  // num fio so — a mesma razao do vetor de Decl mais abaixo.
  static CatItem doTrakt[CONT_MAX], daConta[CONT_MAX];
  // Tres fontes: conta, Trakt e Simkl, cada uma com ate CONT_MAX.
  static Cand juntos[CONT_MAX * 3];
  // Os REMOTOS numa lista so (Trakt e Simkl), por ponteiro. E ela que a conta
  // enfrenta abaixo: para a regra "a mesma obra entra uma vez, a mais recente
  // ganha", Trakt e Simkl sao a mesma pergunta feita a dois servicos.
  static CatItem *remotos[CONT_MAX * 2];
  // O lote do Simkl vai no HEAP e so quando e pedido: 12 CatItem sao ~190 KB,
  // e na Samsung (teto de 128 MiB do WebAssembly) nao se paga isso em BSS para
  // quem nunca vinculou o Simkl.
  CatItem *doSimkl = NULL;
  int nT, nS = 0, nR = 0, nL, nJ = 0, i, j, fora = 0, repetidos = 0;

  // FONTE ESCOLHIDA EM AJUSTES (AJ_CWF_*). 0 = todas, 1 = so a conta Nuvio,
  // 2 = so o Trakt, 3 = so o Simkl. Existe porque quem usa a conta Nuvio e
  // tambem tem Trakt ligado via um outro cliente via o "Continuar" do outro
  // aparelho aparecer aqui sem ter pedido.
  //
  // "AMBAS" INCLUI O SIMKL QUANDO HA VINCULO (issue #110). Quem vinculou o
  // Simkl nesta TV disse que acompanha por ele; deixa-lo de fora do padrao
  // repetiria o defeito do #5 — uma fonte vinculada e ignorada em silencio.
  // Quem nao vinculou nao paga nada: sem token, nenhum pedido sai.
  int fonte = ajustes_cw_fonte();
  int querConta = fonte == AJ_CWF_AMBAS || fonte == AJ_CWF_CONTA;
  int querTrakt = fonte == AJ_CWF_AMBAS || fonte == AJ_CWF_TRAKT;
  int querSimkl = (fonte == AJ_CWF_AMBAS || fonte == AJ_CWF_SIMKL) && simkl_ativo();

  if (max > CONT_MAX) max = CONT_MAX;
  nT = querTrakt ? trakt_continuar(doTrakt, CONT_MAX) : 0;
  // Os limites AGORA valem para todas as fontes. Sem isto, o /sync/playback
  // devolve o que qualquer cliente pausou uma vez — inclusive titulos em 0% e
  // titulos praticamente terminados, que e o "nunca assisti isso" do relato.
  nT = filtrarRemoto(doTrakt, nT, trakt_e_a_seguir, &fora);
  if (querSimkl) {
    doSimkl = (CatItem *)malloc(sizeof(CatItem) * CONT_MAX);
    if (doSimkl) {
      nS = simkl_continuar(doSimkl, CONT_MAX);
      nS = filtrarRemoto(doSimkl, nS, simkl_e_a_seguir, &fora);
    }
  }
  if (fonte == AJ_CWF_SIMKL && !simkl_ativo())
    printf("[desc] continuar assistindo: fonte Simkl sem vinculo; fileira vazia\n");

  // TRAKT E SIMKL NA MESMA OBRA: fica o de instante mais novo. Os dois
  // costumam concordar (muita gente sincroniza um no outro), e dois cards da
  // mesma serie seriam o defeito que continuarLocal ja evita.
  for (i = 0; i < nT; i++) remotos[nR++] = &doTrakt[i];
  for (i = 0; i < nS; i++) {
    int k, achou = -1;
    for (k = 0; k < nR && achou < 0; k++)
      if (mesmaObra(remotos[k], &doSimkl[i])) achou = k;
    if (achou < 0) { remotos[nR++] = &doSimkl[i]; continue; }
    repetidos++;
    if (instanteDaConta(&doSimkl[i]) > instanteDaConta(remotos[achou]))
      remotos[achou] = &doSimkl[i];
  }
  nL = querConta ? continuarLocal(daConta, CONT_MAX) : 0;

  // A CONTA ENTRA PRIMEIRO porque ela e a fonte DATADA (lastWatchedMs, que o
  // syncprog ja reconciliou entre celular e TV). O item remoto que fala da
  // mesma obra sai: manter os dois poria a mesma serie duas vezes na fileira,
  // que e o defeito que continuarLocal ja evitava dentro da propria lista.
  //
  // EXCETO QUANDO O REMOTO E MAIS NOVO NA MESMA OBRA (issue #66): a conta tinha
  // S1E1 a 3% de 8/9 e o Trakt dizia "viu S1E1 inteiro em 19/9, a seguir
  // S1E2"; manter a conta punha na fileira um episodio ja visto, com o selo
  // "a seguir" do outro. O instante decide, como no resto desta funcao.
  { static int pularLocal[CONT_MAX];
    memset(pularLocal, 0, sizeof pularLocal);
    for (i = 0; i < nR; i++)
      for (j = 0; j < nL; j++)
        if (mesmaObra(remotos[i], &daConta[j]) &&
            instanteDaConta(remotos[i]) > instanteDaConta(&daConta[j]))
          pularLocal[j] = 1;
  for (i = 0; i < nL && nJ < (int)(sizeof juntos / sizeof *juntos); i++) {
    if (pularLocal[i]) continue;
    juntos[nJ].item = &daConta[i];
    juntos[nJ].ms   = instanteDaConta(&daConta[i]);
    juntos[nJ].ord  = i;
    nJ++;
  }
  for (i = 0; i < nR && nJ < (int)(sizeof juntos / sizeof *juntos); i++) {
    int repetido = 0;
    for (j = 0; j < nL; j++)
      if (!pularLocal[j] && mesmaObra(remotos[i], &daConta[j])) { repetido = 1; break; }
    if (repetido) { repetidos++; continue; }
    juntos[nJ].item = remotos[i];
    juntos[nJ].ms   = instanteDaConta(remotos[i]);
    // Ordem BASE alta: SO decide quando os dois instantes sao desconhecidos ou
    // iguais. Com o paused_at lido em trakt.c e simkl.c isso ficou raro.
    // Nesse resto de casos a conta vem antes, cada fonte na ordem que deu.
    juntos[nJ].ord  = 1000 + i;
    nJ++;
  } }

  // O QUE A PESSOA TIROU NAO VOLTA COM O REMOTO VELHO. O DELETE do Trakt, do
  // Simkl e da conta sai em fio (ctxmenu.c), e ate cada servidor refletir, o
  // /sync/playback ainda devolve o item — com o paused_at de ANTES da remocao.
  // prog_removido_vence compara esse instante com o da remocao: mais velho
  // fica fora; mais novo (assistiu de novo em outro aparelho, ou aqui) volta.
  { int w = 0, tirados = 0;
    for (i = 0; i < nJ; i++) {
      if (prog_removido_vence(juntos[i].item->imdb, juntos[i].ms)) { tirados++; continue; }
      juntos[w++] = juntos[i];
    }
    nJ = w;
    if (tirados)
      printf("[desc] continuar assistindo: %d tirado(s) pela pessoa, remoto ainda nao refletiu\n",
             tirados); }

  // Insercao: estavel, nJ <= 36, e roda uma vez por ciclo de descoberta.
  for (i = 1; i < nJ; i++) {
    int k = i;
    while (k > 0) {
      long long a = juntos[k].ms, b = juntos[k - 1].ms;
      int troca;
      if (a != b) troca = !b ? 0 : (!a ? 1 : a > b);
      else        troca = juntos[k].ord < juntos[k - 1].ord;
      if (!troca) break;
      { Cand t = juntos[k - 1]; juntos[k - 1] = juntos[k]; juntos[k] = t; }
      k--;
    }
  }

  if (nJ > max) nJ = max;
  for (i = 0; i < nJ; i++) {
    saida[i] = *juntos[i].item;
    if (getenv("NUVIO_CW_LOG"))
      printf("[desc] cw[%d] %s T%dE%d %d%% ms=%lld %s\n", i, saida[i].imdb, saida[i].temporada,
             saida[i].episodio, saida[i].progresso, juntos[i].ms,
             juntos[i].item >= daConta && juntos[i].item < daConta + CONT_MAX ? "conta"
             : doSimkl && juntos[i].item >= doSimkl && juntos[i].item < doSimkl + CONT_MAX ? "simkl"
             : "trakt");
  }
  printf("[desc] continuar assistindo: %d do Trakt, %d do Simkl (%d fora de 1-90%%), "
         "%d da conta, %d repetido(s); %d na fileira\n",
         nT, nS, fora, nL, repetidos, nJ);
  fflush(stdout);
  // O lote do Simkl ja foi COPIADO para `saida` acima; nada mais aponta nele.
  free(doSimkl);
  return nJ;
}

// --- REFAZER "CONTINUAR ASSISTINDO" FORA DO CICLO (issue #38) -----------------
//
// A fileira era recomposta so dentro de montar(), uma vez por ciclo de
// descoberta. Entre dois ciclos, quem mudava o progresso — sair do player, ou
// o sync trazendo o que o celular assistiu — atualizava o ITEM no lugar e a
// fileira continuava com o conjunto velho: titulo que entrou em progresso nao
// aparecia, titulo terminado nao saia. Era o "nao atualiza ou demora" do
// relato: a atualizacao existia, mas so o desenho do card a via.
//
// O refazer e o MESMO montarContinuar, em fio proprio porque ele faz rede
// (/sync/playback + um meta por item). Quem pede duas vezes seguidas — sair
// do player no meio de uma refazagem — ganha UMA rodada a mais no fim, nao
// uma fila: so o estado final interessa.
static volatile int cwVivo, cwDeNovo;
void desc_refazer_continuar(void);

static void *fioContinuar(void *u) {
  CatItem lote[CONT_MAX];
  int n;
  (void)u;
  pthread_mutex_lock(&contTrava);
  n = montarContinuar(lote, CONT_MAX);
  pthread_mutex_unlock(&contTrava);
  cat_trocar_continuar(lote, n);
  cwVivo = 0;
  if (cwDeNovo) { cwDeNovo = 0; desc_refazer_continuar(); }
  return NULL;
}

void desc_refazer_continuar(void) {
  pthread_t t;
  if (cwVivo) { cwDeNovo = 1; return; }
  cwVivo = 1;
  if (pthread_create(&t, NULL, fioContinuar, NULL) != 0) cwVivo = 0;
  else pthread_detach(t);
}

// A METADE LOCAL DE "TIRAR DE CONTINUAR ASSISTINDO", toda no fio de quem
// chama e sem rede: apaga a linha de progresso.c, carimba a remocao (a
// refacao seguinte nao traz o item de volta com o remoto mais velho) e tira o
// card da fileira publicada por identidade, bumpando a revisao — a home ve no
// mesmo quadro. A ORDEM importa: o carimbo vem ANTES de tirar, para uma
// cat_trocar_continuar concorrente ou ja enxergar o carimbo (e podar) ou
// publicar antes da remocao (e ser corrigida por ela). Os DELETE remotos sao
// de quem chama. Devolve quantos cards sairam.
int desc_tirar_continuar(const char *imdb, int temporada, int episodio) {
  char chave[192];
  if (!imdb || !imdb[0]) return 0;
  // A chave e montada do mesmo jeito que progresso.c monta ao gravar — com
  // temporada e episodio quando ha —, senao a linha apagada seria outra.
  prog_chave(chave, sizeof chave, imdb, temporada, episodio);
  prog_remover(chave);
  prog_marcar_removido(imdb);
  return cat_tirar_continuar(imdb);
}

static void *montar(void *u) {
  // O lote tambem cresce: era dimensionado por CAT_MAX e por isso herdava o
  // mesmo teto arbitrario.
  int cap = 128;
  CatItem *lote = malloc(sizeof(CatItem) * (size_t)cap);
  int n = 0, i;
  int nContinuar = 0, nSocial = 0;
  unsigned minhaGeracao = montagemGeracao;
  unsigned meuEstado = homeestado_geracao();
  // PUBLICAR EM PARTES SO COM A TELA VAZIA.
  //
  // A publicacao fileira a fileira existe para a PRIMEIRA home aparecer cedo.
  // Numa volta seguinte (sync que trouxe addons, remontagem pedida, ciclo de
  // 5 min) ela e o oposto disso: a home que estava inteira na tela encolhe
  // para duas fileiras e vai crescendo de novo — MEDIDO no log desta LG,
  // "[home] 13 fileiras na tela" seguido de "6", "8", "9", "11", "13", "14"...
  // Era o "ela fica recarregando" do dono. Com algo na tela, a volta monta em
  // silencio e publica UMA vez no fim — e so se mudou (ver a assinatura).
  int progressivo = (cat_n() == 0);
  (void)u;
  if (!lote) { buscando = 0; return NULL; }

  // O "continue assistindo" vem PRIMEIRO e do Trakt. A home usa as primeiras
  // posicoes do catalogo nessa fileira, entao a ordem aqui e o que define o
  // que aparece la — e o historico tem de ganhar das recomendacoes.
  marco("montar: inicio");
  // OS MANIFESTOS COMECAM A CHEGAR AGORA, nao daqui a seis segundos. Ver o
  // cabecalho de maniLargar: eles nao dependem do Trakt, e eram o bloco de 7 s
  // logo depois dele.
  maniLargar();
  // AS DUAS FONTES, UNIDAS. Ver o cabecalho de montarContinuar: com Trakt
  // vinculado esta fileira ignorava o progresso da conta Nuvio, que e o que
  // chega do celular do dono.
  pthread_mutex_lock(&contTrava);
  nContinuar = montarContinuar(lote, CONT_MAX);
  n += nContinuar;
  marco("trakt continuar assistindo");
  // O feed social oficial e uma fileira propria, logo depois do retorno ao
  // que estava sendo visto. Ele vem cedo para nao depender dos manifestos dos
  // addons e usa a mesma credencial Trakt ja carregada.
  // Sob a MESMA trava: trakt_social e montarContinuar compartilham os buffers
  // de trakt_enfeitar_lote com o fio de desc_refazer_continuar.
  nSocial = trakt_social(lote + n, 8);
  pthread_mutex_unlock(&contTrava);
  n += nSocial;
  marco("trakt atividade dos amigos");
  // O historico do Trakt e a PRIMEIRA fileira da home e chega ~1,6 s antes dos
  // manifestos. Publicar aqui poe conteudo na tela nesse instante em vez de
  // segurar tudo ate o fim.
  // Monta direto em filsMontadas: o vetor local `fil` so existe mais abaixo, e
  // criar um aqui so para copiar seria trabalho a toa.
  if (n > 0 && progressivo) {
    int nf = 0;
    if (nContinuar > 0) {
      CatFileira *f0 = &filsMontadas[nf++];
      memset(f0, 0, sizeof *f0);
      snprintf(f0->chave,  sizeof f0->chave,  "continue_watching");
      snprintf(f0->titulo, sizeof f0->titulo, "Continuar assistindo");
      snprintf(f0->tipo,   sizeof f0->tipo,   "movie");
      f0->ini = 0; f0->n = nContinuar;
    }
    if (nSocial > 0) {
      CatFileira *fs = &filsMontadas[nf++];
      memset(fs, 0, sizeof *fs);
      snprintf(fs->chave, sizeof fs->chave, "social_activity");
      snprintf(fs->titulo, sizeof fs->titulo, "Amigos assistindo");
      snprintf(fs->tipo, sizeof fs->tipo, "social");
      fs->ini = nContinuar; fs->n = nSocial;
    }
    nFileirasMontadas = nf;
    cat_definir_tudo(lote, n, filsMontadas, nf);
    marco("continuar assistindo na tela");
  }
#define GARANTE(quantos) do { \
    if (n + (quantos) > cap) { \
      int novoCap = cap; \
      CatItem *maior; \
      while (novoCap < n + (quantos)) novoCap *= 2; \
      maior = realloc(lote, sizeof(CatItem) * (size_t)novoCap); \
      if (maior) { lote = maior; cap = novoCap; } \
    } } while (0)

  // As fileiras vem dos CATALOGOS declarados nos manifestos dos addons, e nao
  // de uma lista fixa. A ordem, o que fica de fora e os nomes seguem o
  // algoritmo do web (sortAndFilterRowsInternal), com as preferencias lidas de
  // art/fileiras.txt.
  {
    // static: 256 entradas passam de 200 KB, e isso nao cabe com folga na
    // pilha de um fio. montar() roda uma vez e num fio so, entao nao ha
    // reentrada que isto quebre.
    static Decl decls[DECL_MAX];
    int nDecl = 0, k;
    CatFileira fil[CAT_FIL_MAX];
    int nFil = 0;
    // A fileira 0 e "Continuar assistindo", que ja foi montada acima. Ela e
    // SINTETICA: nao esta na ordem do web e nao pode ser desligada por chave —
    // no app ela existe sempre que ha progresso.
    if (nContinuar > 0) {
      CatFileira *f0 = &fil[nFil++];
      memset(f0, 0, sizeof *f0);
      snprintf(f0->chave,  sizeof f0->chave,  "continue_watching");
      snprintf(f0->titulo, sizeof f0->titulo, "Continuar assistindo");
      snprintf(f0->tipo,   sizeof f0->tipo,   "movie");
      f0->ini = 0; f0->n = nContinuar;
    }
    if (nSocial > 0) {
      CatFileira *fs = &fil[nFil++];
      memset(fs, 0, sizeof *fs);
      snprintf(fs->chave, sizeof fs->chave, "social_activity");
      snprintf(fs->titulo, sizeof fs->titulo, "Amigos assistindo");
      snprintf(fs->tipo, sizeof fs->tipo, "social");
      fs->ini = nContinuar; fs->n = nSocial;
    }

    lerPrefs();
    // Zera ANTES de ler os manifestos: cada catalogo com busca se registra
    // sozinho la dentro, na hora em que e lido.
    desc_alvos_busca_zerar();
    // Sem `nDecl < DECL_MAX` no laco: com o vetor cheio o manifesto do addon
    // seguinte nem era baixado, e AIOStreams e Akashi TV ficavam invisiveis
    // para o app inteiro so porque o Xperience, lido antes, declara 605
    // catalogos. lerManifesto ja para de GRAVAR sozinho quando enche.
    // A LISTA DE ADDONS E LIDA AQUI. Marcar o instante e o que permite dizer,
    // no fim, se um pedido de remontagem que chegou no meio do caminho ja foi
    // atendido por esta volta. Ver geracaoPedida.
    geracaoLida = geracaoPedida;
    // TODO ADDON TEM O MANIFESTO LIDO, e a guarda `addons_tem_catalogo(i)` que
    // estava aqui foi TIRADA de proposito.
    //
    // Ela era circular: a capacidade que ela consulta sai DESTE mesmo manifesto.
    // Enquanto ninguem abria a tela de addons dos Ajustes a capacidade ficava na
    // suposicao otimista (1) para sempre, entao a guarda nunca cortava nada e o
    // laco lia todos — o comportamento certo, por acidente. Ao passar a aprender
    // as capacidades no arranque (addons_manifesto_lido) o acidente acabou: da
    // SEGUNDA volta em diante um addon com catalogo=0 deixaria de ter o
    // manifesto lido.
    //
    // E ISSO QUEBRARIA A BUSCA, que e o ponto. Os alvos de busca sao
    // registrados dentro de lerManifesto (ver desc_alvo_busca la), e
    // desc_alvos_busca_zerar() limpa a lista no comeco de cada volta: um addon
    // que declare catalogo buscavel sem declarar o recurso "catalog" — e addon
    // real e desleixado com isso — perderia os alvos dele na segunda volta e a
    // busca ficaria menor sem nada dizendo por que.
    //
    // O CATALOGO QUE NAO APARECE NA HOME CONTINUA ATIVO PARA BUSCAR. Essa e a
    // regra, e ela nao depende de teto, de ordem nem de liga/desliga: quem
    // filtra fileira e o laco de rodadas mais abaixo, e ele mexe em `decls`,
    // nunca nos alvos. Um addon sem catalogo nenhum devolve zero Decl e nao
    // registra alvo nenhum, entao o custo de ler o manifesto dele e um pedido
    // por volta e mais nada.
    // COTA POR ADDON, e nao primeiro a chegar leva tudo.
    //
    // O teto de DECL_MAX era repartido por ordem: quem fosse lido antes gastava
    // quantas vagas quisesse. O Xperience declara 605 catalogos e e o segundo da
    // lista, entao ele sozinho tomava as 256 e TODO addon depois dele ficava com
    // ZERO declaracoes — manifesto lido (a busca funciona), mas nenhum catalogo
    // que pudesse virar fileira. Medido nesta TV: as 6 fileiras de catalogo da
    // home eram todas do Xperience; AIOStreams, Akashi TV e Bingecat nao tinham
    // nenhuma. Foi o relato do Bingecat que expos isso, mas o defeito nao e do
    // addon nem do manifesto dele: e de quem reparte as vagas.
    //
    // Subir o teto nao resolve — 605 de um addon so estoura qualquer numero
    // razoavel, e o vetor e estatico. Repartir resolve.
    //
    // A conta: cada addon tem direito a DECL_MAX/n. Quem declara menos que a
    // cota deixa a sobra para os SEGUINTES (`folga`), entao nada se perde
    // quando ha addon pequeno — OpenSubtitles nao declara catalogo nenhum e
    // passa a cota inteira dele adiante. Uma volta so, sem reler manifesto:
    // reler custaria um pedido de rede por addon.
    { int nAd = addons_n();
      int cota = nAd > 0 ? DECL_MAX / nAd : DECL_MAX;
      int folga = 0;
      if (cota < 1) cota = 1;
      for (i = 0; i < nAd; i++) {
        int teto = cota + folga;
        int lidos, real = 0;
        if (teto > DECL_MAX - nDecl) teto = DECL_MAX - nDecl;
        lidos = lerManifesto(i, addons_base(i), decls + nDecl, teto, &real);
        nDecl += lidos;
        folga = lidos < cota + folga ? cota + folga - lidos : 0;
        // ISSUE #42(a): a linha de sempre ("N catalogo(s) declarado(s)") nao
        // dizia se N era o TOTAL do addon ou so o que a cota deixou passar —
        // quem lia o log via "8 catalogo(s)" e nao tinha como saber que o
        // addon declarava 40. `real` (o total que o manifesto tem de verdade,
        // contado em lerManifesto mesmo depois de `saida` encher) torna o
        // corte visivel e diz o numero que falta.
        if (real > lidos)
          printf("[desc]   %s: %d catalogo(s) declarado(s) (cota %d, "
                 "manifesto tem %d — %d de fora por cota)\n",
                 addons_nome(i), lidos, cota, real, real - lidos);
        else
          printf("[desc]   %s: %d catalogo(s) declarado(s) (cota %d)\n",
                 addons_nome(i), lidos, cota);
      } }
    // FORA OS QUE SO RESPONDEM COM TERMO DE BUSCA, antes de ordenacao e cota.
    //
    // Ver exigeBusca(): eles respondem vazio ao unico pedido que a home sabe
    // fazer. Tirar aqui, e nao no laco de rodadas, e o que impede que gastem
    // vaga do teto — inclusive a "vaga garantida" do addon, que era como um
    // addon inteiro acabava representado na home por uma fileira que nunca teve
    // conteudo. Os alvos de BUSCA ja foram registrados dentro de lerManifesto e
    // nao passam por aqui: o catalogo continua buscavel, so deixa de fingir que
    // e fileira.
    { int r2, w2 = 0, cortados = 0;
      for (r2 = 0; r2 < nDecl; r2++) {
        if (decls[r2].exigeParam) {
          if (cortados < 6)
            printf("[desc]   fora da home (so responde com busca): %s\n", decls[r2].titulo);
          cortados++;
          continue;
        }
        if (w2 != r2) decls[w2] = decls[r2];
        w2++;
      }
      if (cortados)
        printf("[desc] %d catalogo(s) so respondem com busca e nao viram fileira\n", cortados);
      nDecl = w2; }
    printf("[desc] %d catalogos declarados pelos addons\n", nDecl);
    // FANTASMAS. Addon removido da conta deixava os catalogos dele na lista de
    // fileiras — e na home — ate o proximo login (@rawldon). Aqui TODOS os
    // manifestos desta volta ja foram lidos, entao a lista de addons vivos e
    // completa e a poda e segura: so cai catalogo cujo addon nao esta mais na
    // conta E que ninguem registrou nesta sessao.
    { const char *ids[16], *bases[16];
      int na = addons_n(), q;
      if (na > 16) na = 16;
      for (q = 0; q < na; q++) { ids[q] = addons_id_manifesto(q); bases[q] = addons_base(q); }
      if (fil_podar_catalogos(ids, bases, na)) fil_gravar_registro(); }

    // ALVOS DE BUSCA. Independem da ordem/filtro das FILEIRAS da home: um
    // catalogo pode estar desativado na home e ainda assim ser bom para
    // procurar (o Akashi so tem busca, nao tem fileira que valha a pena).
    printf("[desc] %d alvos de busca\n", desc_busca_n_alvos());
    marco("manifestos lidos");

    // ensureOrderKeysWithPrefs: a ordem salva primeiro, e as chaves NOVAS
    // acrescentadas no fim. Catalogo que o addon passou a declarar hoje entra
    // por ultimo, nao no meio — e o que evita a home se reorganizar sozinha.
    {
      int ordem[DECL_MAX];
      int nOrdem = 0, j;
      char vistos[DECL_MAX];
      memset(vistos, 0, sizeof vistos);
      for (k = 0; k < nPrefOrdem; k++)
        for (j = 0; j < nDecl; j++)
          if (!vistos[j] && !strcmp(decls[j].chave, prefOrdem[k])) {
            ordem[nOrdem++] = j; vistos[j] = 1; break;
          }
      // INTERCALADO POR ADDON, e nao na ordem em que os manifestos foram
      // lidos. E o MESMO defeito que a cota de declaracoes resolveu um nivel
      // abaixo ("o defeito nao e do addon nem do manifesto dele: e de quem
      // reparte as vagas"), e ele voltava aqui: a home pede so as primeiras N
      // desta lista, e na ordem crua as N eram todas do primeiro addon.
      //
      // MEDIDO na LG do dono ao investigar o #37: 124 catalogos declarados, 6
      // viram fileira, o Xperience declara 72 e vem primeiro — o FrostView,
      // com UM catalogo, ficava em 124o e nunca era pedido. Quem acabou de
      // instalar um addon nao tinha como ver nada dele.
      //
      // Cada addon leva a PRIMEIRA fileira antes de qualquer um levar a
      // segunda; dentro do addon, a ordem do manifesto. Quem tem prefencia
      // salva ja saiu no laco de cima e nao entra nesta partilha.
      { int nAd2 = addons_n();
        for (;;) {
          int pegou = 0, i2;
          for (i2 = 0; i2 < nAd2; i2++) {
            const char *b = addons_base(i2);
            if (!b || !b[0]) continue;
            for (j = 0; j < nDecl; j++)
              if (!vistos[j] && decls[j].base && !strcmp(decls[j].base, b)) {
                ordem[nOrdem++] = j; vistos[j] = 1; pegou = 1; break;
              }
          }
          if (!pegou) break;
        } }
      // Sobra: declaracao cuja base nao casa com addon nenhum da lista atual.
      for (j = 0; j < nDecl; j++) if (!vistos[j]) ordem[nOrdem++] = j;

      // A ordem da CONTA por cima da ordem local, e a regra e UNIAO, nao
      // substituicao: catordem_unir puxa para a frente o que o remoto conhece,
      // na ordem dele, e deixa todo o resto no fim como ja estava. Aplicar a
      // ordem remota crua removeria os catalogos que passaram a existir depois
      // de ela ter sido gravada — era o `{"localItems":54,"remoteItems":43}`
      // de todo boot na OLED65C9, com a home reescrita e nunca convergindo.
      if (catordem_tem_ordem() && nOrdem > 0) {
        const char *chaves[DECL_MAX];
        int saida[DECL_MAX], antes[DECL_MAX], q;
        for (q = 0; q < nOrdem; q++) chaves[q] = decls[ordem[q]].chave;
        memcpy(antes, ordem, sizeof(int) * (size_t)nOrdem);
        q = catordem_unir(chaves, nOrdem, saida, DECL_MAX);
        for (j = 0; j < q; j++) ordem[j] = antes[saida[j]];
        nOrdem = q;
      }

      // ORDEM LOCAL POR CIMA DA DA CONTA, mesma regra de uniao e um motivo a
      // mais: catordem.c e SO LEITURA porque a TV nao pode empurrar de volta
      // (a trava esta no topo de catordem.h), entao sem esta precedencia a
      // ordem que a pessoa arruma aqui seria desfeita pelo proximo sync.
      if (fil_tem_ordem() && nOrdem > 0) {
        const char *chaves[DECL_MAX];
        int saida[DECL_MAX], antes[DECL_MAX], q;
        for (q = 0; q < nOrdem; q++) chaves[q] = decls[ordem[q]].chave;
        memcpy(antes, ordem, sizeof(int) * (size_t)nOrdem);
        q = fil_unir(chaves, nOrdem, saida, DECL_MAX);
        for (j = 0; j < q; j++) ordem[j] = antes[saida[j]];
        nOrdem = q;
      }

      // customTitles ganha do nome do manifesto. Aplicado a TODOS os
      // candidatos, e nao so aos que forem pedidos: e este titulo que a tela de
      // Ajustes mostra, e mostrar la o nome cru do manifesto enquanto a home
      // mostra o renomeado seria a mesma fileira com dois nomes.
      for (k = 0; k < nOrdem; k++) {
        Decl *d = &decls[ordem[k]];
        int t;
        for (t = 0; t < nPrefTit; t++)
          if (!strcmp(prefTit[t].chave, d->chave)) {
            snprintf(d->titulo, sizeof d->titulo, "%s", prefTit[t].titulo);
            break;
          }
      }
      // REGISTRA TODOS OS CANDIDATOS, inclusive os desligados e os que ficam
      // abaixo do limite. E o que deixa a tela de Ajustes PROMOVER um catalogo
      // que hoje nao entra: se ela listasse apenas as fileiras pedidas, o limite
      // viraria uma jaula e nada de fora dele poderia ser escolhido. O teto do
      // registro e o FIL_MAX de fileiras.h, nao o das fileiras desenhadas.
      // O NOME DO ADDON E O TIPO VAO JUNTO: e aqui, e so aqui, que eles sao
      // conhecidos — a tela de Ajustes precisa deles para dizer de onde a
      // fileira vem. `-1` em itens porque nesta altura nenhum catalogo foi
      // pedido ainda; quem sabe a contagem e home.c.
      for (k = 0; k < nOrdem && k < FIL_MAX; k++)
        fil_registrar(decls[ordem[k]].chave, decls[ordem[k]].titulo,
                      decls[ordem[k]].nomeAddon, decls[ordem[k]].tipo, -1);
      fil_gravar_registro();

      // TETO DE FILEIRAS: o numero escolhido em Ajustes (7 de fabrica),
      // limitado pelo CAT_FIL_MAX do vetor. Ele corta o que vai ser PEDIDO pela
      // rede, e nao o desenho: sete fileiras tem de custar sete GET, senao o
      // ajuste economiza pixel e nao trabalho. `nFil` ja conta "Continuar
      // assistindo" e "Amigos assistindo" — sao fileiras na tela como as outras.
      int teto = fil_limite();
      if (teto > CAT_FIL_MAX) teto = CAT_FIL_MAX;

      // UMA VAGA GARANTIDA POR ADDON, e ela vem DEPOIS das duas ordens.
      //
      // A partilha por addon (la em cima) so governa o que a conta nao ordena,
      // e isso nao bastou: MEDIDO na LG do dono com os sete addons, as seis
      // fileiras continuaram sendo as da conta e o FrostView — um catalogo,
      // recem-instalado — seguiu invisivel. A ordem da conta sozinha ja tem
      // mais de seis catalogos, entao o orcamento acaba antes de a partilha ser
      // alcancada. Eu tinha dito na issue #37 que a partilha resolvia; nao
      // resolvia, e a correcao esta publicada la.
      //
      // Aqui a regra e mais forte e o preco esta dito: um addon que ficaria com
      // ZERO fileira toma a vaga do addon que ja tem mais de uma, o mais tarde
      // possivel dentro da janela. Isso mexe de proposito numa ordem que a
      // pessoa arrumou no app web — o que se ganha e a garantia de que instalar
      // um addon mostra alguma coisa dele, que e a pergunta que traz a issue.
      // Quem ja aparece nao perde a vaga; quem perde e a SEGUNDA fileira de
      // quem tem duas.
      { int janela = teto - nFil, i3, nAd3 = addons_n();
        if (janela > nOrdem) janela = nOrdem;
        for (i3 = 0; i3 < nAd3 && janela > 1; i3++) {
          const char *b = addons_base(i3);
          int q, alvo = -1, ceder = -1;
          if (!b || !b[0]) continue;
          for (q = 0; q < janela; q++)
            if (decls[ordem[q]].base && !strcmp(decls[ordem[q]].base, b)) break;
          if (q < janela) continue;                 // ja tem vaga
          for (q = janela; q < nOrdem; q++)
            if (decls[ordem[q]].base && !strcmp(decls[ordem[q]].base, b)) { alvo = q; break; }
          if (alvo < 0) continue;                   // addon sem catalogo declarado
          // Cede a ULTIMA posicao da janela cujo addon ja aparece antes dela.
          { int r, s;
            for (r = janela - 1; r > 0 && ceder < 0; r--) {
              const char *br = decls[ordem[r]].base;
              if (!br) continue;
              for (s = 0; s < r; s++)
                if (decls[ordem[s]].base && !strcmp(decls[ordem[s]].base, br)) { ceder = r; break; }
            } }
          if (ceder < 0) continue;                  // ninguem tem duas: nada a ceder
          { int mov = ordem[alvo], w;
            for (w = alvo; w > ceder; w--) ordem[w] = ordem[w - 1];
            ordem[ceder] = mov; }
          printf("[desc] vaga garantida: %s entra em %d (%s)\n",
                 addons_nome(i3), ceder, decls[ordem[ceder]].titulo);
        } }

      // Uma resposta nova de manifesto nao muda a estrutura que a pessoa ja
      // aceitou. Enquanto a assinatura owner/perfil/idioma/config continuar
      // valida, pedimos apenas chaves presentes no snapshot; o usuario pode
      // acrescentar uma fileira explicitamente em Ajustes, o que invalida a
      // assinatura e libera a proxima montagem. Ainda registramos todas as
      // declaracoes em fileiras.c acima para permitir essa escolha depois.
      if (homeestado_contexto_valido()) {
        int w = 0;
        for (k = 0; k < nOrdem; k++) {
          const Decl *d = &decls[ordem[k]];
          if (homeestado_tem_fileira(d->chave)) ordem[w++] = ordem[k];
        }
        nOrdem = w;
        // O snapshot aceito define a ordem estável. O manifesto pode reordenar
        // suas declarações entre ciclos sem representar uma escolha do usuário.
        for (k = 1; k < nOrdem; k++) {
          int atual = ordem[k];
          int rank = homeestado_ordem_fileira(decls[atual].chave), j = k;
          while (j > 0) {
            int anterior = homeestado_ordem_fileira(decls[ordem[j - 1]].chave);
            if (anterior < 0 || (rank >= 0 && anterior <= rank)) break;
            ordem[j] = ordem[j - 1]; j--;
          }
          ordem[j] = atual;
        }
      }

      int marcouPrimeira = 0;
      // Instrumentacao do arranque. Antes dava para ver o TOTAL de fileiras e
      // nada mais: catalogo que nao respondeu, catalogo vazio e catalogo
      // declarado duas vezes saiam todos como "uma fileira a menos", sem uma
      // linha dizendo qual dos tres foi.
      int pedidos = 0, responderam = 0, vazios = 0, duplicados = 0;
      int cursor = 0, rodadas = 0;
      // Zera POR CICLO. Um contador que so cresce diria, na terceira volta, um
      // numero que e a soma de tres montagens — e a linha [col] existe para
      // descrever a montagem que acabou de acontecer.
      engolidasNaDeclaracao = 0;
      tarefas = calloc(CAT_FIL_MAX, sizeof(TarefaCat));

      // EM RODADAS, e nao num lote so. Com um lote de exatamente `teto`
      // pedidos, cada catalogo que responde VAZIO custa uma fileira a menos na
      // tela: o dono escolheria 7 e veria 5, sem nada dizendo por que. A rodada
      // seguinte pede exatamente o que faltou. O caso comum — todos respondendo
      // — continua sendo UMA rodada, com o mesmo custo de antes.
      while (tarefas && nFil < teto && cursor < nOrdem) {
        int alvo = teto - nFil;
        rodadas++;
        // ETAPA 1 — escolher as fileiras DESTA rodada. Os filtros (desligada,
        // repetida) sao locais e baratos; fazer isto antes deixa os fios so com
        // a parte cara, que e a rede.
        nTarefas = 0; proximaTarefa = 0;
        memset(tarefas, 0, sizeof(TarefaCat) * CAT_FIL_MAX);
        for (; cursor < nOrdem && nTarefas < alvo; cursor++) {
          Decl *d = &decls[ordem[cursor]];
          int t, repetida = 0;
          if (desligada(d)) {
            if (dentroDeColecaoVisivel(d)) engolidasNaDeclaracao++;
            continue;
          }
          // MESMA CHAVE DUAS VEZES = MESMA FILEIRA DUAS VEZES. Acontece quando
          // o mesmo addon entra duas vezes na lista (`addons.txt` mais a conta)
          // ou quando um manifesto declara o catalogo repetido: a chave e
          // <addonId>_<tipo>_<catalogoId> e fica identica, e a home mostrava a
          // fileira em duplicata com os MESMOS titulos. Dois addons DIFERENTES
          // declarando o mesmo id nao caem aqui de proposito — sao catalogos
          // distintos, com conteudo possivelmente distinto.
          for (t = 0; t < nFil; t++)
            if (!strcmp(fil[t].chave, d->chave)) { repetida = 1; break; }
          for (t = 0; !repetida && t < nTarefas; t++)
            if (!strcmp(tarefas[t].d->chave, d->chave)) { repetida = 1; break; }
          if (repetida) { duplicados++; continue; }
          tarefas[nTarefas++].d = d;
        }
        if (!nTarefas) break;
        pedidos += nTarefas;

        // ETAPA 2 — CAT_FIOS trabalhando na fila. Se o calloc falhar ou nao
        // houver o que ler, nTarefas fica 0 e o laco de montagem abaixo nao roda:
        // a home segue com o que ja foi publicado, sem caminho de erro proprio.
        { pthread_t fios[CAT_FIOS];
          int criados = 0, q;
          for (q = 0; q < CAT_FIOS && nTarefas > 0; q++)
            if (pthread_create(&fios[criados], NULL, fioCatalogo, NULL) == 0) criados++;
          // Sem NENHUM fio (pthread_create falhou em todos), le em serie no
          // proprio fio: pior desempenho, mesmo resultado. Melhor que home vazia.
          if (!criados && nTarefas > 0) fioCatalogo(NULL);

          // ETAPA 3 — montar NA ORDEM, publicando cada fileira assim que o balde
          // dela fica pronto. Esperar o balde k nao desperdica tempo: os fios
          // seguem enchendo k+1, k+2 enquanto este e consumido.
          for (k = 0; k < nTarefas && nFil < teto; k++) {
            const Decl *d = tarefas[k].d;
            int got;
            int estadoLinha = 0;
            for (;;) {
              int pr;
              pthread_mutex_lock(&catTrava);
              pr = tarefas[k].pronto;
              pthread_mutex_unlock(&catTrava);
              if (pr) break;
              SDL_Delay(10);
            }
            got = tarefas[k].n;
            if (tarefas[k].respondeu) responderam++;
            if (tarefas[k].respondeu && !got) {
              // Respondeu SEM `metas`: o catalogo existe e esta vazio hoje. Nao e
              // erro e nao pode virar titulo pendurado na home — a rodada seguinte
              // pede outro no lugar dele.
              vazios++;
              printf("[desc] catalogo vazio: %s\n", d->titulo);
              // Vazio valido: conserva a chave da fileira na estrutura. A
              // proxima fileira nao muda de identidade por causa de um feed
              // que respondeu corretamente sem itens.
              estadoLinha = 1;
            }
            if (!tarefas[k].respondeu && !got) {
              // ISSUE #42(a): antes disto o log so tinha o resumo do fim da
              // rodada ("N pedidos, M responderam") — quem quisesse saber QUAL
              // fileira sumiu tinha de adivinhar por subtracao. Nomear o
              // catalogo aqui, igual ao "catalogo vazio" acima, e o que falta
              // para responder "por que esta fileira nao apareceu" por fileira
              // pedida, e nao so por total.
              printf("[desc] catalogo sem resposta a tempo: %s (%s)\n",
                     d->titulo, d->nomeAddon);
              // Timeout preserva o ultimo lote bom desta chave. No primeiro
              // arranque, sem snapshot, segue omitida de forma segura.
              got = linhaAnterior(d->chave, tarefas[k].itens,
                                  MAX_POR_FILEIRA, NULL);
              if (!got) continue;
            }
            GARANTE(MAX_POR_FILEIRA + 2);
            if (got > cap - n) got = cap - n;
            if (got > 0)
              memcpy(lote + n, tarefas[k].itens, sizeof(CatItem) * (size_t)got);
          {
            CatFileira *f = &fil[nFil++];
            memset(f, 0, sizeof *f);
            snprintf(f->chave,  sizeof f->chave,  "%s", d->chave);
            snprintf(f->titulo, sizeof f->titulo, "%s", d->titulo);
            snprintf(f->tipo,   sizeof f->tipo,   "%s", d->tipo);
            snprintf(f->base,   sizeof f->base,   "%s", d->base ? d->base : "");
            snprintf(f->catId,  sizeof f->catId,  "%s", d->id);
            f->ini = n; f->n = got; f->estado = estadoLinha;
          }
          n += got;
          printf("[desc] fileira %d: %s (%d)\n", nFil - 1, d->titulo, got);
          // PUBLICA A CADA FILEIRA, em vez de so no fim.
          //
          // Medido no Mac: o catalogo da rede so aparecia aos 12.991 ms, e ate
          // la a home mostrava apenas o catalogo estatico do pacote. Na TV e
          // pior. A referencia mostra conteudo em 1,7 s — nao porque a rede dela
          // seja mais rapida, mas porque ela mostra o que ja tem.
          //
          // cat_definir_tudo troca o bloco inteiro de uma vez (catalogo.c), entao
          // publicar N vezes e seguro para quem esta desenhando; o custo e uma
          // copia do vetor por fileira, que acontece no fio da descoberta e nao
          // no de desenho.
          // So publica em partes com a tela VAZIA. Sobre o cache — ou sobre a
          // home da volta anterior — seria um retrocesso visivel: 16 fileiras
          // viram 1. Ver `progressivo` no inicio de montar(). filsMontadas so
          // muda JUNTO com a publicacao: sem ela o bloco da tela e outro.
          if (progressivo && minhaGeracao == montagemGeracao && meuEstado == homeestado_geracao()) {
            nFileirasMontadas = nFil;
            memcpy(filsMontadas, fil, sizeof(CatFileira) * (size_t)nFil);
            cat_definir_tudo(lote, n, filsMontadas, nFileirasMontadas);
          }
          // Bandeira propria: `nFil == 1` nunca acontece aqui porque a fileira
          // "Continuar assistindo" ja ocupou a posicao 0 antes do laco.
          if (!marcouPrimeira) { marcouPrimeira = 1;
                                 marco("primeira fileira da rede na tela"); }
          }
          for (q = 0; q < criados; q++) pthread_join(fios[q], NULL);
        }
      }   /* fim da rodada */
      free(tarefas); tarefas = NULL; nTarefas = 0;
      // O QUE O LIMITE DEIXOU DE FORA. `cursor` parou onde o teto encheu, entao
      // o resto de `ordem` nunca foi nem considerado. Conta so o que NAO esta
      // desligado: fileira que a pessoa desligou na mao nao e surpresa e nao
      // deve virar convite para aumentar o limite.
      // NAO E A CONTAGEM CRUA DO QUE SOBROU, e a diferenca importa.
      //
      // A primeira versao disto contava tudo que o cursor nao alcancou e a home
      // dizia "mais 240 catalogos disponiveis" — verdadeiro (o Xperience declara
      // 605) e inutil: le como alarme e promete o que aumentar o limite nao
      // entrega, porque o teto do vetor de fileiras e CAT_FIL_MAX. O numero que
      // serve e QUANTAS FILEIRAS A MAIS a pessoa veria levando o limite ao
      // maximo, que e o que ela pode fazer a respeito.
      { int k, sobraram = 0, cabem = CAT_FIL_MAX - teto;
        for (k = cursor; k < nOrdem; k++)
          if (!desligada(&decls[ordem[k]])) sobraram++;
        if (cabem < 0) cabem = 0;
        catalogosFora = sobraram < cabem ? sobraram : cabem;
        // A CONTAGEM CRUA TAMBEM SERVE, e para outra pergunta. `catalogosFora`
        // e clamped por `cabem` e vira 0 quando o limite ja esta no maximo —
        // certo para o convite da home, inutil para saber se houve catalogo que
        // o teto impediu de PEDIR. Quem precisa disso e desc_remontar_fileiras.
        catalogosNaoPedidos = sobraram; }
      nFilsLote = nFil;
      memcpy(filsLote, fil, sizeof(CatFileira) * (size_t)nFil);
      // UMA LINHA QUE RESPONDE "o que falhou no arranque". As quatro contagens
      // sao as quatro respostas possiveis de um catalogo, e sem separa-las o
      // unico numero disponivel era o total de fileiras — que fica igual quer o
      // addon esteja fora do ar, quer o catalogo esteja vazio.
      printf("[desc] catalogos: %d pedido(s) em %d rodada(s), %d responderam, "
             "%d sem resposta, %d vazio(s), %d repetido(s); %d de %d fileira(s) "
             "no limite\n",
             pedidos, rodadas, responderam, pedidos - responderam, vazios,
             duplicados, nFil, teto);
      if (nFil < teto)
        printf("[desc] o limite de %d fileiras nao foi preenchido: acabaram os "
               "catalogos disponiveis (%d declarados, %d na ordem)\n",
               teto, nDecl, nOrdem);
      fflush(stdout);
    }
  }

  // Watchlist e colecao entram DEPOIS das recomendacoes, e nao antes.
  // A home usa as PRIMEIRAS posicoes do catalogo nas suas fileiras; com as
  // listas do Trakt na frente (e elas passam de 60 itens cada) as fileiras
  // viravam a watchlist inteira e as recomendacoes nunca apareciam. A
  // biblioteca varre o catalogo todo procurando as marcas, entao para ela
  // tanto faz onde estao.
  GARANTE(400);
  n += trakt_lista("watchlist",  lote + n, cap - n);
  GARANTE(400);
  n += trakt_lista("collection", lote + n, cap - n);
  // PLAN TO WATCH DO SIMKL (issue #110), SO QUANDO O "+" SALVA LA. E a mesma
  // marca naLista da watchlist do Trakt, entao o painel de Salvos e a aba
  // Salvos da Biblioteca mostram o Plan to Watch sem saber de onde ele veio.
  // Quem so vinculou o Simkl para a aba Listas nao ve o Plan to Watch
  // misturado nos Salvos sem ter pedido — e nao paga os dois GETs.
  //
  // TITULO QUE JA ESTA NO LOTE (na watchlist do Trakt, ou numa fileira) SO
  // GANHA A MARCA, nao entra de novo: dois CatItem com o mesmo imdb poriam o
  // titulo duas vezes na Biblioteca.
  if (ajustes_salvos_no_simkl() && simkl_ativo()) {
    // DIRETO NO LOTE, e nao num vetor a parte: 300 CatItem sao 4,7 MB, e a
    // Samsung roda com teto de 128 MiB. O lote ja cresce por GARANTE; os
    // repetidos saem na compactacao logo abaixo.
    int k, w, np, novos;
    GARANTE(300);
    np = simkl_plantowatch(lote + n, cap - n < 300 ? cap - n : 300);
    for (k = n, w = n; k < n + np; k++) {
      int jj, ja = 0;
      for (jj = 0; jj < n; jj++)
        if (!strcmp(lote[jj].imdb, lote[k].imdb)) { lote[jj].naLista = 1; ja = 1; break; }
      if (ja) continue;
      if (w != k) lote[w] = lote[k];
      w++;
    }
    novos = w - n;
    n = w;
    if (np) printf("[desc] plantowatch do Simkl: %d, %d novo(s) no catalogo\n", np, novos);
  }
  preservarFileirasAusentes(&lote, &n, &cap, filsLote, &nFilsLote);
  // Reanexar linhas sem resposta acontece depois das chamadas de rede; aplicar
  // a ordem salva ao conjunto inteiro impede que uma resposta parcial desloque
  // essas linhas para o fim da Home.
  ordenarPorSnapshot(filsLote, nFilsLote);
#undef GARANTE

  if (n || nFilsLote > 0) {
    // SO PUBLICA SE MUDOU. A mesma home montada de novo (ciclo de 5 min, sync
    // sem novidade) tem a mesma assinatura que a da tela; republicar seria
    // trocar o vetor, subir a revisao e a home se remontar — foco, rolagem e
    // arte de volta a zero — para mostrar exatamente o que ja mostrava.
    unsigned long antes = cat_assinatura();
    unsigned long depois = cat_assinatura_de(lote, n, filsLote, nFilsLote);
    if (minhaGeracao != montagemGeracao || meuEstado != homeestado_geracao()) {
      // DESCARTADA SEM PUBLICAR, e por isso RECOMECA. A geracao muda sozinha
      // no arranque (perfil escolhido, catordem/colecoes/addons da conta
      // chegando — tudo entra na assinatura do homeestado), e no log da LG as
      // DUAS montagens da sessao morreram aqui: sem recomecar, ninguem mais
      // montava e a home ficava com o pacote. O pedido de desc_repetir que
      // trocou montagemGeracao tambem passava por aqui e se perdia
      // (repetirAoFim nunca era lido). A volta nova le o contexto atual; ela
      // so descarta de novo se o contexto mudar DE NOVO, entao nao ha laco.
      printf("[desc] montagem descartada: conta/perfil/config mudou no meio; recomecando\n");
      fflush(stdout);
      free(lote); repetirAoFim = 0; buscando = 0;
      desc_iniciar();
      return NULL;
    }
    // A partir daqui filsMontadas descreve o bloco da tela nos dois ramos:
    // publicado agora, ou igual (mesma assinatura) ao que ja estava.
    nFileirasMontadas = nFilsLote;
    memcpy(filsMontadas, filsLote, sizeof(CatFileira) * (size_t)nFilsLote);
    if (depois != antes || cat_do_cache()) {
      cat_definir_tudo(lote, n, filsMontadas, nFileirasMontadas);
      marco("catalogo da rede publicado");
      printf("[desc] catalogo montado com %d titulos\n", n);
    } else {
      marco("catalogo da rede igual ao da tela: nao republicado");
      printf("[desc] catalogo montado com %d titulos, igual ao que esta na tela; mantido\n", n);
    }
    cat_cache_substituido();
    homeestado_salvar_se_geracao(filsMontadas, nFileirasMontadas, meuEstado);

    // A ULTIMA PALAVRA SOBRE AS COLECOES E AQUI. Issue #18, terceira tentativa,
    // e desta vez o problema nao era a REGRA e sim QUANDO ela roda.
    //
    // dentroDeColecaoVisivelBase — quem decide que um catalogo pertence a uma
    // colecao e nao merece fileira propria — so existe dentro de
    // desc_remontar_fileiras(), e quem chamava essa funcao era so o sync, no
    // instante em que as colecoes da conta chegam. MEDIDO nesta LG: as colecoes
    // chegam por volta de 2 s e os manifestos dos addons so aos 13 s. Uma fonte
    // de colecao da conta vem com addonId e SEM URL, e a URL so sai do
    // manifesto — entao, aos 2 s, nenhuma fonte casa com nada e o engolimento
    // roda no unico momento em que nao pode dar certo. Depois disso ninguem
    // chamava de novo, e os catalogos da colecao ficavam soltos na home para
    // sempre. Por isso o conserto do v1.0.21 (resolver a base antes de comparar)
    // estava certo e nunca disparava, e por isso o relator via o mesmo defeito
    // com Xperience E com Ultra Max: nao e do addon, e da ordem de chegada.
    //
    // Aqui os manifestos JA foram lidos (marco "manifestos lidos" vem antes
    // deste, sempre), entao as bases resolvem. A chamada e em memoria — sem
    // HTTP e sem fio novo, ver a nota da propria funcao — e acontece uma vez
    // por ciclo de descoberta, nao num laco.
    //
    // ANTES de cat_gravar_cache de proposito: assim o cache guarda as fileiras
    // JA agrupadas e a proxima abertura nasce certa, em vez de repetir a
    // separacao ate a rede responder de novo.
    if (col_n() > 0) desc_remontar_fileiras();
    // Grava so o resultado COMPLETO, nao as publicacoes parciais: um cache
    // com tres fileiras faria a proxima abertura nascer pela metade e so
    // completar quando a rede respondesse — exatamente o que o cache existe
    // para evitar.
    { char donoEsperado[64]; int perfilEsperado;
      if (homeestado_identidade_geracao(meuEstado, donoEsperado,
                                       sizeof donoEsperado, &perfilEsperado))
        cat_gravar_cache_se_identidade(dirArteDesc, donoEsperado, perfilEsperado);
      else
        printf("[desc] cache descartado: conta/perfil/config mudou durante a montagem\n");
    }
  } else {
    printf("[desc] nada veio da rede; segue o catalogo do pacote\n");
  }
  fflush(stdout);
  free(lote);
  buscando = 0;
  // Um pedido que chegou COM o ciclo no ar roda agora, com as credenciais que
  // entraram no meio do caminho. Zerar a marca antes de disparar evita que uma
  // falha de pthread_create deixe o pedido preso para sempre.
  if (repetirAoFim) {
    repetirAoFim = 0;
    // PEDIDO JA ATENDIDO: nenhum outro chegou depois de esta volta ler a lista
    // de addons, entao repetir daria o mesmo resultado. Ver geracaoPedida.
    if (geracaoLida == geracaoPedida)
      printf("[desc] remontagem dispensada: a lista nova entrou nesta volta\n");
    else
      desc_iniciar();
  }
  return NULL;
}

void desc_iniciar(void) {
  if (buscando) { printf("[desc] ja montando; pedido ignorado\n"); fflush(stdout); return; }
  buscando = 1;
  montagemGeracao++;
  if (pthread_create(&fio, NULL, montar, NULL) != 0) {
    // NAO FALHAR CALADO. No webOS um pthread_create nunca falhou e o caminho de
    // erro era so uma bandeira; no alvo WASM o fio e um Worker do navegador,
    // que E um recurso limitado e PODE acabar. Quando acaba, a home fica com o
    // catalogo do pacote para sempre e nao ha uma linha no log dizendo por que.
    buscando = 0;
    printf("[desc] pthread_create FALHOU: o catalogo nao vai remontar\n");
    fflush(stdout);
  }
  else pthread_detach(fio);
}

// Remontar depois de uma mudanca de credencial (vincular o Trakt, receber a
// chave do TMDB pela conta). Chamar desc_iniciar() direto NAO resolve: se um
// ciclo estiver no ar ele volta calado, e o pedido se perde justamente no caso
// comum — a pessoa vincula o Trakt enquanto o sync do arranque ainda roda, e as
// fileiras do Trakt so aparecem no proximo arranque. Foi o relato "ativa o
// trakt e nao atualiza".
int desc_catalogos_fora(void) { return catalogosFora; }

// REMONTA AS FILEIRAS SEM TOCAR NA REDE.
//
// Mudanca de ORDEM (da conta ou daqui), de COLECAO ou do LIMITE nao muda um
// unico item: muda quais fileiras existem e em que sequencia. Ate agora tudo
// isso passava por desc_repetir(), que refaz o ciclo INTEIRO — Trakt, manifestos
// e todos os catalogos de novo. O custo nao era so tempo: como o segundo ciclo
// publica um conjunto de fileiras diferente do primeiro, a home carregava um
// catalogo, trocava por outro e so entao assentava na ordem final. E o que o
// dono descreveu, e o conserto do issue #18 ia deixar isso MAIS visivel, porque
// a partir dele os dois ciclos diferem ainda mais (no primeiro as colecoes ainda
// nao chegaram, entao os catalogos delas viram fileira solta; no segundo, nao).
//
// Aqui a lista de fileiras da ultima montagem e reordenada e filtrada em
// memoria, e republicada por cat_republicar_fileiras. Sem HTTP, sem fio novo.
//
// AS FILEIRAS SINTETIZADAS FICAM ONDE ESTAO. "Continuar assistindo" e "Amigos
// assistindo" nao tem `base` — elas nao sao catalogo de addon e nao participam
// da ordem, senao a uniao as jogaria para o fim como chaves desconhecidas.
void desc_remontar_fileiras(void) {
  static CatFileira saidaFil[CAT_FIL_MAX];
  const char *chaves[CAT_FIL_MAX];
  int idxCat[CAT_FIL_MAX], ordem[CAT_FIL_MAX], antes[CAT_FIL_MAX];
  int i, k, q, nCat = 0, nOut = 0, teto, engolidas = 0;
  if (nFileirasMontadas < 1) return;

  // 1. as fixas primeiro, na ordem em que ja estavam.
  for (i = 0; i < nFileirasMontadas && nOut < CAT_FIL_MAX; i++)
    if (!filsMontadas[i].base[0]) saidaFil[nOut++] = filsMontadas[i];

  // 2. as de catalogo, filtradas pelas mesmas regras de desligada().
  for (i = 0; i < nFileirasMontadas; i++) {
    const CatFileira *f = &filsMontadas[i];
    if (!f->base[0]) continue;
    if (fil_oculta(f->chave) || catordem_oculta(f->chave, f->chave)) continue;
    if (dentroDeColecaoVisivelBase(f->base, f->tipo, f->catId)) { engolidas++; continue; }
    if (nCat < CAT_FIL_MAX) { chaves[nCat] = f->chave; idxCat[nCat] = i; nCat++; }
  }
  // SEMPRE, e nao so quando engoliu: a linha existe para o caso em que ela
  // deveria ter engolido e nao engoliu, que e o #18. Com "colecoes=0" o
  // relator nao tem colecao; com "colecoes>0 sem-base>0" as colecoes chegaram
  // mas o manifesto do addon ainda nao, e por isso nenhuma casa; com
  // "sem-base=0 engolidas=0" o casamento falhou por outro motivo e o problema
  // e a comparacao, nao a ordem de chegada.
  printf("[col] fileiras de catalogo=%d engolidas=%d (%d na declaracao) | "
         "colecoes=%d fontes-sem-base=%d\n", nCat, engolidas,
         engolidasNaDeclaracao, col_n(), col_fontes_sem_base());
  // COM engolidas=0 E colecoes>0, o que falta e ver os DOIS lados da
  // comparacao. A base vai REDIGIDA por rede_url_publica: a do Xperience leva
  // um JWT dentro do caminho, e este log e lido e colado em relato de defeito.
  // O DESPEJO CEGO NAO RESPONDIA A PERGUNTA e foi trocado.
  //
  // Ele imprimia as 8 primeiras fileiras e as 6 primeiras fontes. Com 137
  // pastas as 6 saem todas da PRIMEIRA pasta, e o relator do #18 mandou
  // exatamente isso: seis fontes de "Streaming/Netflix" ao lado de fileiras do
  // Cinemeta e do Xperience que nao tem relacao com elas. Dois lados que nao se
  // encostam nao dizem qual campo falhou.
  //
  // Agora a pergunta e por fileira: ate onde ela chegou (ver col_diagnostico).
  // O nivel separa as causas que hoje se parecem — addon fora de colecao,
  // `type` trocado, `catId` diferente, e o caso em que o casamento deu certo e
  // quem escondeu a fileira foi o grupo oculto.
  if (!engolidas && !engolidasNaDeclaracao && col_n() > 0) {
    int i2;
    for (i2 = 0; i2 < nCat && i2 < 12; i2++) {
      const CatFileira *f2 = &filsMontadas[idxCat[i2]];
      char grupo[64];
      int nivel = col_diagnostico(f2->base, f2->tipo, f2->catId,
                                  grupo, sizeof grupo);
      static const char *porque[4] = {
        "nenhuma colecao usa este addon",
        "colecao tem o addon, mas com outro tipo",
        "colecao tem addon e tipo, mas outro id de catalogo",
        "CASOU — nao engoliu porque o grupo esta oculto"
      };
      printf("[col]   %s (%s): nivel=%d %s%s%s\n",
             f2->catId, f2->tipo, nivel, porque[nivel],
             grupo[0] ? " | grupo=" : "", grupo[0] ? grupo : "");
      // NIVEL 3 PRECISA DIZER QUEM ESCONDEU. Sao duas listas independentes com
      // o mesmo efeito na tela: a escolha feita nesta TV (fileiras.txt) e a que
      // veio da conta. Sem separa-las o relator nao sabe onde desfazer, e eu ja
      // gastei uma rodada supondo que fosse a local quando o arquivo nem existia
      // no aparelho.
      if (nivel == 3) {
        char ch[96];
        col_chave_grupo(grupo, ch, sizeof ch);
        printf("[col]     %s oculto por: %s%s\n", ch,
               fil_oculta(ch) ? "esta TV " : "",
               catordem_oculta(ch, ch) ? "a conta" : "");
      }
    }
  }
  fflush(stdout);

  for (k = 0; k < nCat; k++) ordem[k] = k;
  if (catordem_tem_ordem() && nCat > 0) {
    memcpy(antes, ordem, sizeof(int) * (size_t)nCat);
    q = catordem_unir(chaves, nCat, ordem, CAT_FIL_MAX);
    for (k = 0; k < q; k++) ordem[k] = antes[ordem[k]];
    nCat = q;
  }
  if (fil_tem_ordem() && nCat > 0) {
    const char *c2[CAT_FIL_MAX];
    for (k = 0; k < nCat; k++) c2[k] = filsMontadas[idxCat[ordem[k]]].chave;
    memcpy(antes, ordem, sizeof(int) * (size_t)nCat);
    q = fil_unir(c2, nCat, ordem, CAT_FIL_MAX);
    for (k = 0; k < q; k++) ordem[k] = antes[ordem[k]];
    nCat = q;
  }

  teto = fil_limite();
  if (teto < 1 || teto > CAT_FIL_MAX) teto = CAT_FIL_MAX;
  for (k = 0; k < nCat && nOut < teto && nOut < CAT_FIL_MAX; k++)
    saidaFil[nOut++] = filsMontadas[idxCat[ordem[k]]];

  printf("[desc] fileiras remontadas sem rede: %d de %d%s\n",
         nOut, nFileirasMontadas,
         engolidas ? " (algumas engolidas por colecao)" : "");
  fflush(stdout);
  nFileirasMontadas = nOut;
  memcpy(filsMontadas, saidaFil, sizeof(CatFileira) * (size_t)nOut);
  cat_republicar_fileiras(saidaFil, nOut);

  // A VAGA QUE A COLECAO LIBEROU TEM DE SER PREENCHIDA, e so a rede tem com que.
  //
  // A segunda metade do #18 e esta: "they still take up the 16-row limit and
  // push other collections". Na montagem o filtro roda ANTES de virar tarefa,
  // entao catalogo dentro de colecao nao gasta vaga nenhuma — conferido em
  // tests/colfileiras.sh. Mas quando as colecoes chegam DEPOIS de a montagem ter
  // acabado (sync lento, ou colecao criada no app web com a TV ligada), o teto
  // ja foi gasto: aqui as fileiras engolidas saem e a home fica com MENOS
  // fileiras que o limite, com os catalogos que o teto tinha cortado perdidos
  // para sempre — eles nunca foram baixados, nao ha o que reordenar.
  //
  // Por isso a republicacao vem primeiro (a tela melhora na hora, sem esperar a
  // rede) e o ciclo so e pedido quando as duas condicoes valem: alguma fileira
  // saiu por colecao E havia catalogo que o teto impediu de pedir. Nao ha laco:
  // o ciclo novo ja monta com as colecoes carregadas, entao a proxima passagem
  // por aqui nao engole mais nada. Zerar a contagem torna isso explicito em vez
  // de depender do valor que a montagem seguinte vai escrever.
  if (engolidas && catalogosNaoPedidos > 0) {
    printf("[desc] %d fileira(s) sairam por estarem dentro de colecao e %d "
           "catalogo(s) ficaram fora do teto: pedindo ciclo para preencher\n",
           engolidas, catalogosNaoPedidos);
    fflush(stdout);
    catalogosNaoPedidos = 0;
    desc_repetir();
  }
}

void desc_repetir(void) {
  montagemGeracao++;
  geracaoPedida++;
  if (!buscando) { desc_iniciar(); return; }
  repetirAoFim = 1;
  printf("[desc] remontagem pedida; roda ao fim do ciclo atual\n");
  fflush(stdout);
}

// Logout: solta a cache de manifestos (e da conta que saiu) e zera o estado da
// descoberta para a proxima sessao comecar limpa. A cache de manifestos e a
// unica alocacao persistente entre ciclos que pertence a este modulo.
void desc_esquecer(void) {
  maniCacheLimpar();
}

// --- episodios sob demanda ---------------------------------------------------

// CACHE LRU DO /meta DAS SERIES.
//
// UMA resposta do Cinemeta traz TODAS as temporadas: o `videos` vem inteiro e o
// filtro por temporada acontece aqui embaixo, de graca. Mesmo assim cada troca
// de temporada rebaixava o corpo todo — numa serie longa sao centenas de
// kilobytes de JSON por pilula apertada, e era isso que o dono sentia como
// "demora para atualizar quando troca de temporada".
//
// Guardar apenas a ultima serie fazia voltar ao titulo anterior repetir a
// transferencia inteira. Quatro respostas cobrem a navegacao normal de ida e
// volta sem deixar o uso de memoria crescer sem limite.
#define META_CACHE_N 4
static struct { char id[24]; char *corpo; unsigned uso; } metaCache[META_CACHE_N];
static unsigned metaRelogio;
static pthread_mutex_t metaTrava = PTHREAD_MUTEX_INITIALIZER;

static char *metaCacheObter(const char *id) {
  char *r = NULL;
  pthread_mutex_lock(&metaTrava);
  for (int i = 0; i < META_CACHE_N; i++)
    if (metaCache[i].corpo && !strcmp(metaCache[i].id, id)) {
      metaCache[i].uso = ++metaRelogio;
      r = strdup(metaCache[i].corpo); /* o fio trabalha em copia estavel */
      break;
    }
  pthread_mutex_unlock(&metaTrava);
  return r;
}

static void metaCacheGuardar(const char *id, const char *corpo) {
  int vaga = 0;
  char *copia = strdup(corpo);
  if (!copia) return;
  pthread_mutex_lock(&metaTrava);
  for (int i = 0; i < META_CACHE_N; i++) {
    if (metaCache[i].corpo && !strcmp(metaCache[i].id, id)) { vaga = i; break; }
    if (!metaCache[i].corpo || metaCache[i].uso < metaCache[vaga].uso) vaga = i;
  }
  free(metaCache[vaga].corpo);
  metaCache[vaga].corpo = copia;
  metaCache[vaga].uso = ++metaRelogio;
  snprintf(metaCache[vaga].id, sizeof metaCache[vaga].id, "%s", id);
  pthread_mutex_unlock(&metaTrava);
}

// Publica a parte critica antes de qualquer enriquecimento opcional. Assim a
// fileira de episodios aparece depois da primeira resposta, sem esperar pelas
// duas viagens ao TMDB usadas para foto e personagem do elenco.
// NOTAS DE EPISODIO vindas do TMDB (issue #87). O Cinemeta nao tem voto por
// episodio; o TMDB tem, na resposta de /tv/<id>/season/<n> — episodes[] com
// episode_number e vote_average (0..10). Funcao PURA, chamada tambem pelo
// teste (tests/cateps.c): casa por numero e so escreve nos eps da temporada
// pedida; voto ausente ou zero deixa nota=0, que na tela simplesmente nao
// desenha selo. Devolve quantos episodios ganharam nota.
int desc_tmdb_notas_temporada(const char *json, CatEp *eps, int n,
                              int temporada) {
  const char *p;
  int feitos = 0, i;
  if (!json || !eps || n < 1) return 0;
  p = js_array(json, NULL, "episodes");
  while (p) {
    const char *f = js_fim(p);
    int num = (int)js_num(p, f, "episode_number", -1);
    if (num >= 0) {
      for (i = 0; i < n; i++)
        if (eps[i].temporada == temporada && eps[i].episodio == num) {
          double v = js_num(p, f, "vote_average", 0.0);
          if (v > 0.0) { eps[i].nota = (int)(v * 10.0 + 0.5); feitos++; }
          break;
        }
    }
    p = js_prox(f);
  }
  return feitos;
}

static int publicarEpisodios(const char *corpo, int alvoItem, const char *titulo) {
// Em par com CAT_EP_MAX (catalogo.c): um titulo que caiba no store nao pode
// truncar no parse, e um que nao caiba trunca aqui em vez de zerar os outros.
#define VIDEOS_MAX 1200
  CatEp *eps = malloc(sizeof(CatEp) * VIDEOS_MAX);
  int n = 0;
  if (!eps) return 0;
  const char *p = js_array(corpo, NULL, "videos");
  while (p && n < VIDEOS_MAX) {
    const char *f = js_fim(p);
    int t = (int)js_num(p, f, "season", -1);
    if (t > 0) {
      CatEp *e = &eps[n];
      char d[24] = "";
      memset(e, 0, sizeof *e);
      e->temporada = t;
      e->episodio = (int)js_num(p, f, "episode", 0);
      js_texto(p, f, "name", e->nome, sizeof e->nome);
      js_texto(p, f, "overview", e->sinopse, sizeof e->sinopse);
      js_texto(p, f, "thumbnail", e->thumb, sizeof e->thumb);
      js_texto(p, f, "released", d, sizeof d);
      desc_data_extenso(d, e->data, sizeof e->data);
      n++;
    }
    p = js_prox(f);
  }
  for (int i = 1; i < n; i++) {
    CatEp k = eps[i];
    int j;
    for (j = i - 1; j >= 0 &&
         (eps[j].temporada > k.temporada ||
          (eps[j].temporada == k.temporada && eps[j].episodio > k.episodio)); j--)
      eps[j + 1] = eps[j];
    eps[j + 1] = k;
  }
  if (n) cat_definir_episodios(alvoItem, eps, n);
  free(eps);
  marco("episodios na tela");
  printf("[desc] %s: %d episodios publicados antes dos extras\n", titulo, n);
  fflush(stdout);
  return n;
}

static void *buscarEps(void *u) {
  int alvoItem = epItem;
  const CatItem *orig = cat_item(alvoItem);
  CatItem base;
  const CatItem *it;
  char url[600], *corpo = NULL;
  char serie[24];
  (void)u;
  if (!orig || !orig->imdb[0]) { fioEpVivo = 0; return NULL; }
  // CANAL NAO PASSA AQUI. O Cinemeta so conhece filme/serie por id do IMDb
  // ("tt..."); id de canal e "cs:channel:<hash>". "ehFilme = tipo != series"
  // tratava canal como filme, pedia /meta/movie/<id> com um id que o
  // Cinemeta nunca teve, E o corte no primeiro ':' (abaixo, ao montar
  // `serie`) reduzia TODO canal a "cs" — a mesma chave de cache para todos,
  // entao o primeiro canal aberto "vazava" elenco/genero/nota para os
  // seguintes. Medido: nenhum canal tem tipo "movie" nem "series" (#37).
  if (ehCanal(orig->tipo)) { fioEpVivo = 0; return NULL; }
  // FILME TAMBEM PASSA AQUI. O /meta/movie traz elenco, direcao, generos e
  // nota — antes so os titulos enriquecidos no catalogo tinham elenco, e a
  // pagina do filme abria sem a fileira. O que e so de serie (episodios,
  // temporadas) e pulado abaixo.
  int ehFilme = strcmp(orig->tipo, "series") != 0;
  base = *orig;
  it = &base;
  { const char *dp;
    snprintf(serie, sizeof serie, "%s", it->imdb);
    dp = strchr(serie, ':');
    if (dp) *(char *)dp = 0;
    snprintf(url, sizeof url, "%s/meta/%s/%s.json", CINEMETA,
             ehFilme ? "movie" : "series", serie); }

  corpo = metaCacheObter(serie);
  marco(corpo ? "episodios: meta do cache" : "episodios: baixando meta");

  if (!corpo) {
    corpo = rede_baixar(url, 25);
    if (!corpo) { fioEpVivo = 0; return NULL; }
    metaCacheGuardar(serie, corpo);
  }
  if (!ehFilme) publicarEpisodios(corpo, alvoItem, it->titulo);
  // O MAPA DE EPISODIOS VISTOS NAO E PEDIDO AQUI, e essa linha existe para dizer
  // por que: extras.c JA baixa /shows/<id>/progress/watched ao abrir o titulo,
  // e agora alimenta vistoep de la. Uma versao deste arquivo chegou a pedir de
  // novo — duas requisicoes identicas por titulo, para a segunda sobrescrever a
  // primeira com o mesmo dado.
  // A MESMA resposta traz elenco, direcao e a lista de temporadas. Buscar de
  // novo para cada uma seria tres viagens ao mesmo lugar.
  {
    CatItem edit = *it;
    const char *c = js_array(corpo, NULL, "cast");
    int k = 0;
    while (c && k < CAT_ELENCO_MAX) {
      size_t n2 = 0;
      const char *p2 = c;
      if (*p2 != '"') break;
      p2++;
      while (*p2 && *p2 != '"' && n2 + 1 < sizeof edit.elenco[k].nome)
        edit.elenco[k].nome[n2++] = *p2++;
      edit.elenco[k].nome[n2] = 0;
      edit.elenco[k].papel[0] = 0;   // o Cinemeta nao diz o personagem
      edit.elenco[k].foto[0] = 0;
      k++;
      p2++;
      while (*p2 == ' ') p2++;
      c = (*p2 == ',') ? p2 + 1 : NULL;
      while (c && *c == ' ') c++;
    }
    edit.nElenco = k;
    { const char *dr = js_array(corpo, NULL, "director");
      if (dr && *dr == '"') {
        size_t n2 = 0;
        dr++;
        while (*dr && *dr != '"' && n2 + 1 < sizeof edit.direcao)
          edit.direcao[n2++] = *dr++;
        edit.direcao[n2] = 0;
      } }
    // GENEROS, NOTA E PAIS. Vinham so do CATALOGO, e o catalogo do Cinemeta nao
    // traz nenhum dos tres: a linha de meta ficava com o TIPO ("Programa de TV")
    // no lugar dos generos, sem selo do IMDb e sem pais. O /meta traz os tres, e
    // esta funcao ja tem a resposta na mao — deixar de ler era desperdicio de uma
    // viagem que ja foi paga.
    { const char *g = js_array(corpo, NULL, "genres");
      char lista[160]; size_t n3 = 0;
      lista[0] = 0;
      while (g && *g == '"' && n3 + 1 < sizeof lista) {
        const char *p2 = g + 1;
        if (n3) { // separador do web: espaco, ponto medio, espaco
          if (n3 + 4 >= sizeof lista) break;
          lista[n3++] = ' '; lista[n3++] = '\xc2'; lista[n3++] = '\xb7'; lista[n3++] = ' ';
        }
        while (*p2 && *p2 != '"' && n3 + 1 < sizeof lista) lista[n3++] = *p2++;
        lista[n3] = 0;
        if (*p2 == '"') p2++;
        while (*p2 == ' ') p2++;
        g = (*p2 == ',') ? p2 + 1 : NULL;
        while (g && *g == ' ') g++;
      }
      if (lista[0]) snprintf(edit.genero, sizeof edit.genero, "%s", lista); }
    { double nota = js_num(corpo, NULL, "imdbRating", 0.0);
      // O campo vem como "8.1" (string ou numero); guardamos por 10 para caber
      // em int sem perder a casa decimal, como o resto do catalogo ja faz.
      if (nota > 0.0) {
        int n10 = (int)(nota * 10.0 + 0.5);
        if (n10 > 99) n10 /= 10;      // ja veio multiplicado
        edit.nota = n10;
      } }
    js_texto(corpo, NULL, "country", edit.pais, sizeof edit.pais);
    // Temporadas presentes, sem repetir e em ordem.
    { const char *v = js_array(corpo, NULL, "videos");
      edit.nTemporadas = 0;
      while (v) {
        const char *fv = js_fim(v);
        int t2 = (int)js_num(v, fv, "season", -1);
        if (t2 > 0) {
          int j, achou = 0;
          for (j = 0; j < edit.nTemporadas; j++)
            if (edit.temporadas[j] == t2) { achou = 1; break; }
          if (!achou && edit.nTemporadas < CAT_TEMP_MAX) edit.temporadas[edit.nTemporadas++] = t2;
        }
        v = js_prox(fv);
      }
      { int i2, j2, tmp;
        for (i2 = 0; i2 < edit.nTemporadas; i2++)
          for (j2 = i2 + 1; j2 < edit.nTemporadas; j2++)
            if (edit.temporadas[j2] < edit.temporadas[i2]) {
              tmp = edit.temporadas[i2];
              edit.temporadas[i2] = edit.temporadas[j2];
              edit.temporadas[j2] = tmp;
            } } }
    // Publica texto, generos e temporadas antes do enriquecimento de imagens.
    cat_atualizar_item(alvoItem, &edit);
    marco("detalhe: meta basico na tela");
    { char idBase[24];
      const char *dp;
      snprintf(idBase, sizeof idBase, "%s", it->imdb);
      dp = strchr(idBase, ':');
      if (dp) *(char *)dp = 0;
      fotosDoElenco(&edit, idBase, !strcmp(it->tipo, "series")); }
    cat_atualizar_item(alvoItem, &edit);
    printf("[desc] %s: %d atores, dir='%s', %d temporadas\n",
           edit.titulo, edit.nElenco, edit.direcao, edit.nTemporadas);
    fflush(stdout);

    // NOTA POR EPISODIO (issue #87): o Cinemeta nao tem voto por episodio, o
    // TMDB tem — uma viagem por temporada presente na lista, nao uma por
    // episodio. edit.tmdb ja foi resolvido por fotosDoElenco quando a serie
    // tem elenco; sem elenco (fotosDoElenco sai mais cedo) resolve-se aqui
    // pelo mesmo /find. "Titulo e sinopse" e a porta: quem desligou o TMDB
    // nao quer este trafego. Republica so se alguma nota entrou.
    if (!ehFilme && ajustes_tmdb_basico()) {
      const char *chave2 = desc_chave_tmdb();
      long tmdbId = edit.tmdb;
      if (chave2[0] && tmdbId <= 0) {
        char u2[400];
        char *c3;
        snprintf(u2, sizeof u2,
                 "%s/find/%s?api_key=%s&external_source=imdb_id",
                 TMDB, serie, chave2);
        c3 = rede_baixar(u2, 20);
        if (c3) {
          const char *p3 = js_array(c3, NULL, "tv_results");
          if (p3) tmdbId = (long)js_num(p3, js_fim(p3), "id", 0.0);
          free(c3);
        }
      }
      if (chave2[0] && tmdbId > 0) {
        int neps = cat_n_episodios(alvoItem);
        if (neps > 0) {
          CatEp *tmp = malloc(sizeof(CatEp) * (size_t)neps);
          if (tmp) {
            int preenchidas = 0, i2;
            for (i2 = 0; i2 < neps; i2++) {
              const CatEp *e0 = cat_episodio(alvoItem, i2);
              if (e0) tmp[i2] = *e0; else memset(&tmp[i2], 0, sizeof tmp[i2]);
            }
            for (i2 = 0; i2 < neps; i2++) {
              int s = tmp[i2].temporada, j2, jaFoi = 0;
              for (j2 = 0; j2 < i2; j2++)
                if (tmp[j2].temporada == s) { jaFoi = 1; break; }
              if (jaFoi) continue;
              { char u3[400];
                char *c4;
                snprintf(u3, sizeof u3,
                         "%s/tv/%ld/season/%d?api_key=%s&language=%s",
                         TMDB, tmdbId, s, chave2, desc_tmdb_idioma());
                c4 = rede_baixar(u3, 15);
                if (c4) {
                  preenchidas += desc_tmdb_notas_temporada(c4, tmp, neps, s);
                  free(c4);
                } }
            }
            if (preenchidas > 0) {
              cat_definir_episodios(alvoItem, tmp, neps);
              printf("[desc] %s: notas TMDB em %d episodios\n",
                     edit.titulo, preenchidas);
              fflush(stdout);
            }
            free(tmp);
          }
        }
      }
    }
  }

  free(corpo);
  fioEpVivo = 0;
  return NULL;
}

// Pedido AINDA NAO ATENDIDO, quando um chega com um fio em voo. Antes isto era
// `if (fioEpVivo) return;` — o pedido era largado no chao, e trocar de
// temporada enquanto a anterior carregava deixava a lista na temporada ERRADA
// para sempre, sem nova tentativa. Guardar o ultimo (nao enfileirar todos) e o
// certo: o dono quer a temporada onde ele PAROU, nao as que ele atravessou.
static int pendItem = -1, pendTemp;

// --- VER TUDO ----------------------------------------------------------------
//
// Uma lista SEPARADA do catalogo da home, de proposito: a home guarda 12 por
// fileira e e ela que a biblioteca e a busca varrem. Despejar 200 itens de um
// catalogo ali dentro mudaria o que essas duas telas veem por causa de uma
// navegacao que o dono pode fechar no segundo seguinte.
#define VT_PASSO 100        // `skipStep` padrao do web quando o addon nao diz

static CatItem  vtItens[VT_MAX];
static int      vtN;
static char     vtBase[600], vtTipo[8], vtCat[96], vtGenre[96];
static int      vtPagina, vtFim, vtFioVivo, vtErro;
static unsigned vtGeracao;
// Fonte nao-addon aberta por desc_vertudo_fonte (issue #44): vtProvedor 1 e
// TMDB, 2 e Trakt; vtFonte e uma copia zerada da ColSource para o memcmp de
// "mesma fonte" ser seguro (a struct tem preenchimento).
static ColSource vtFonte;
static int      vtProvedor;
static pthread_mutex_t vtTrava = PTHREAD_MUTEX_INITIALIZER;

// Um item de resultado do TMDB (results[], items[], cast[], crew[],
// parts[]) vira CatItem. O id fica "tmdb:<n>" — nao e imdb, e quem abre
// resolve pelo desc_pedir_titulo_tmdb (vertudo checa o prefixo). O tipo vem
// do media_type do item quando ele existe (combined_credits mistura filme e
// serie); sem ele vale o tipo padrao da fonte.
static int deMetaTmdb(const char *p, const char *f, const char *tipoPadrao,
                      CatItem *d) {
  char v[512];
  memset(d, 0, sizeof *d);
  long id = (long)js_num(p, f, "id", 0.0);
  if (id <= 0) return 0;
  if (!js_texto(p, f, "title", d->titulo, sizeof d->titulo) &&
      !js_texto(p, f, "name", d->titulo, sizeof d->titulo)) return 0;
  if (!js_texto(p, f, "poster_path", v, sizeof v) || v[0] != '/') return 0;
  snprintf(d->poster, sizeof d->poster, "https://image.tmdb.org/t/p/w342%s", v);
  if (js_texto(p, f, "backdrop_path", v, sizeof v) && v[0] == '/')
    // w1280 e nao original: ver a conta do mesmo trecho em deMeta — 33 MB
    // decodificados por arte no nucleo fraco da TV.
    snprintf(d->backdrop, sizeof d->backdrop, "https://image.tmdb.org/t/p/w1280%s", v);
  else snprintf(d->backdrop, sizeof d->backdrop, "%s", d->poster);
  if (js_texto(p, f, "backdrop_path", v, sizeof v) && v[0] == '/')
    snprintf(d->backdropTmdb, sizeof d->backdropTmdb,
             "https://image.tmdb.org/t/p/w1280%s", v);
  { char mt[12] = "";
    js_texto(p, f, "media_type", mt, sizeof mt);
    snprintf(d->tipo, sizeof d->tipo, "%s",
             !strcmp(mt, "tv") ? "series" : mt[0] ? "movie" : tipoPadrao); }
  snprintf(d->imdb, sizeof d->imdb, "tmdb:%ld", id);
  d->tmdb = id;
  { char dt[16] = "";
    if (!js_texto(p, f, "release_date", dt, sizeof dt))
      js_texto(p, f, "first_air_date", dt, sizeof dt);
    snprintf(d->meta, sizeof d->meta, "%.4s", dt); }
  snprintf(d->genero, sizeof d->genero, "%s", i18n(rotuloTipoSing(d->tipo)));
  d->nota = (int)(js_num(p, f, "vote_average", 0.0) * 10.0 + 0.5);
  js_texto(p, f, "overview", d->sinopse, sizeof d->sinopse);
  return 1;
}

// Entrada de lista do Trakt ({type, movie:{...}} / {type, show:{...}}). So
// entra o que tem ids.imdb — sem ele nao ha como resolver o titulo em nenhum
// catalogo. O poster NAO vem na resposta da lista (o web le entity.images,
// que so chega com extended=full; mesmo assim nem toda lista os traz) — quem
// preenche e o trakt_enfeitar_lote depois do parse da pagina.
static int deMetaTrakt(const char *p, const char *f, const char *midia,
                       CatItem *d) {
  char bruto[1400], ids[400], v[64];
  const char *e, *fe;
  int serie;
  memset(d, 0, sizeof *d);
  // O objeto do titulo fica sob "movie" ou "show"; a midia da fonte diz qual,
  // mas aceita os dois porque listas mistas existem. js_bruto devolve o
  // objeto cru, com as chaves — o parse acontece sobre essa copia.
  serie = !strcasecmp(midia, "TV");
  if (!js_bruto(p, f, serie ? "show" : "movie", bruto, sizeof bruto)) {
    serie = !serie;
    if (!js_bruto(p, f, serie ? "show" : "movie", bruto, sizeof bruto))
      return 0;
  }
  e = bruto; fe = bruto + strlen(bruto);
  if (!js_bruto(e, fe, "ids", ids, sizeof ids) ||
      !js_texto(ids, ids + strlen(ids), "imdb", v, sizeof v) || !v[0])
      return 0;
  snprintf(d->imdb, sizeof d->imdb, "%s", v);
  if (!js_texto(e, fe, "title", d->titulo, sizeof d->titulo) &&
      !js_texto(e, fe, "name", d->titulo, sizeof d->titulo)) return 0;
  { long ano = (long)js_num(e, fe, "year", 0.0);
    if (ano) snprintf(d->meta, sizeof d->meta, "%ld", ano); }
  snprintf(d->tipo, sizeof d->tipo, "%s", serie ? "series" : "movie");
  snprintf(d->genero, sizeof d->genero, "%s", i18n(rotuloTipoSing(d->tipo)));
  return 1;
}

// Traduz o objeto "filters" do editor do site para parametros do /discover
// do TMDB — mesma lista de chaves que applyTmdbDiscoverFilters do web.
static void tmdbFiltros(char *q, size_t n, const char *filtros, int isTv) {
  const char *fim = filtros ? filtros + strlen(filtros) : NULL;
  static const struct { const char *js, *url; int soTv; } M[] = {
    {"withGenres","with_genres",0}, {"withoutGenres","without_genres",0},
    {"voteAverageGte","vote_average.gte",0}, {"voteAverageLte","vote_average.lte",0},
    {"voteCountGte","vote_count.gte",0},
    {"withOriginalLanguage","with_original_language",0},
    {"withOriginCountry","with_origin_country",0},
    {"withKeywords","with_keywords",0}, {"withoutKeywords","without_keywords",0},
    {"withCompanies","with_companies",0}, {"withoutCompanies","without_companies",0},
    {"withNetworks","with_networks",1},
    {"withRuntimeGte","with_runtime.gte",0}, {"withRuntimeLte","with_runtime.lte",0},
  };
  char v[128];
  size_t u;
  if (!filtros || *filtros != '{') return;
  for (u = 0; u < sizeof M / sizeof *M; u++) {
    if (M[u].soTv && !isTv) continue;
    if (js_texto(filtros, fim, M[u].js, v, sizeof v) && v[0])
      snprintf(q + strlen(q), n - strlen(q), "&%s=%s", M[u].url, v);
  }
  if (js_texto(filtros, fim, "releaseDateGte", v, sizeof v) && v[0])
    snprintf(q + strlen(q), n - strlen(q), "&%s.gte=%s",
             isTv ? "first_air_date" : "release_date", v);
  if (js_texto(filtros, fim, "releaseDateLte", v, sizeof v) && v[0])
    snprintf(q + strlen(q), n - strlen(q), "&%s.lte=%s",
             isTv ? "first_air_date" : "release_date", v);
  { long ano = (long)js_num(filtros, fim, "year", 0.0);
    if (ano > 0)
      snprintf(q + strlen(q), n - strlen(q), "&%s=%ld",
               isTv ? "first_air_date_year" : "year", ano); }
  if (js_texto(filtros, fim, "withWatchProviders", v, sizeof v) && v[0]) {
    char reg[8] = "";
    js_texto(filtros, fim, "watchRegion", reg, sizeof reg);
    snprintf(q + strlen(q), n - strlen(q), "&watch_region=%s&with_watch_providers=%s",
             reg[0] ? reg : "US", v);
  }
}

// Monta a URL da pagina `pagina` da fonte TMDB. Espelha fetchTmdbSourceItems
// do web: COLLECTION, LIST, PERSON/DIRECTOR e discover. Devolve o nome do
// array de itens na resposta via `vetor` ("results", "items", "cast", "crew"
// ou "parts") e 0 quando falta a chave da API.
static const char *tmdbMontarUrl(const ColSource *f, int pagina,
                                 char *url, size_t n) {
  int isTv = !strcasecmp(f->midia, "TV") || !strcasecmp(f->tmdbTipo, "NETWORK");
  const char *mt = isTv ? "tv" : "movie";
  const char *chave = desc_chave_tmdb();
  if (!chave[0]) return NULL;
  if (!strcasecmp(f->tmdbTipo, "COLLECTION") && f->tmdbId > 0) {
    snprintf(url, n, "%s/collection/%ld?api_key=%s&language=%s",
             TMDB, f->tmdbId, chave, desc_tmdb_idioma());
    return "parts";
  }
  if (!strcasecmp(f->tmdbTipo, "LIST") && f->tmdbId > 0) {
    snprintf(url, n, "%s/list/%ld?api_key=%s&language=%s&page=%d",
             TMDB, f->tmdbId, chave, desc_tmdb_idioma(), pagina);
    return "items";
  }
  if ((!strcasecmp(f->tmdbTipo, "PERSON") || !strcasecmp(f->tmdbTipo, "DIRECTOR"))
      && f->tmdbId > 0) {
    snprintf(url, n, "%s/person/%ld/combined_credits?api_key=%s&language=%s",
             TMDB, f->tmdbId, chave, desc_tmdb_idioma());
    return !strcasecmp(f->tmdbTipo, "DIRECTOR") ? "crew" : "cast";
  }
  { char q[1400] = "";
    snprintf(q, sizeof q, "&sort_by=%s",
             f->ordenar[0] ? f->ordenar
                           : isTv ? "first_air_date.desc" : "popularity.desc");
    tmdbFiltros(q, sizeof q, f->filtros, isTv);
    if (!strcasecmp(f->tmdbTipo, "COMPANY") && f->tmdbId > 0)
      snprintf(q + strlen(q), sizeof q - strlen(q), "&with_companies=%ld", f->tmdbId);
    else if (!strcasecmp(f->tmdbTipo, "NETWORK") && f->tmdbId > 0) {
      // Rede: igual ao web — so o que ja saiu, e com_status exclui pilotos
      // cancelados e producoes sem data.
      char hoje[12] = "";
      { time_t t = time(NULL); struct tm tmv;
        if (localtime_r(&t, &tmv))
          snprintf(hoje, sizeof hoje, "%04d-%02d-%02d",
                   tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday); }
      snprintf(q + strlen(q), sizeof q - strlen(q),
               "&with_networks=%ld&first_air_date.lte=%s&with_status=0|3|4",
               f->tmdbId, hoje[0] ? hoje : "9999-12-31");
    } else if (f->tmdbId > 0)
      snprintf(q + strlen(q), sizeof q - strlen(q), "&%s=%ld",
               isTv ? "with_networks" : "with_companies", f->tmdbId);
    snprintf(url, n, "%s/discover/%s?api_key=%s&language=%s&page=%d%s",
             TMDB, mt, chave, desc_tmdb_idioma(), pagina, q);
    return "results";
  }
}

static void *fioVerTudo(void *u) {
  (void)u;
  for (;;) {
  char url[1600], base[600], type[8], id[96], genre[96], encoded[290], *corpo;
  int raw=0, skip, cap, prov;unsigned generation;
  ColSource fonte;
  pthread_mutex_lock(&vtTrava);
  skip=vtPagina;generation=vtGeracao;prov=vtProvedor;
  memset(&fonte,0,sizeof fonte);fonte=vtFonte;
  snprintf(base,sizeof base,"%s",vtBase);snprintf(type,sizeof type,"%s",vtTipo);
  snprintf(id,sizeof id,"%s",vtCat);snprintf(genre,sizeof genre,"%s",vtGenre);
  pthread_mutex_unlock(&vtTrava);

  // FONTE NAO-ADDON (issue #44): pasta de colecao do site cujo conteudo vem
  // do TMDB ou do Trakt, nao de um catalogo de addon. vtPagina aqui e o
  // NUMERO da pagina (comeca em 1), nao o skip de itens.
  if (prov) {
    CatItem lote[48]; int nl=0, semMais=0, ok=0;
    corpo=NULL;
    if (prov==1) {
      const char *vetor=tmdbMontarUrl(&fonte,skip,url,sizeof url);
      if (vetor) corpo=rede_baixar(url,12);
      if (corpo) {
        const char *padrao=!strcasecmp(fonte.midia,"TV")?"series":"movie";
        const char *p=js_array(corpo,NULL,vetor);
        for(;p&&nl<48;p=js_prox(js_fim(p))) {
          const char *f=js_fim(p);raw++;
          // DIRECTOR: a resposta e crew[] inteira e so entra quem tem
          // job=Director — o web filtra igual.
          if (!strcasecmp(fonte.tmdbTipo,"DIRECTOR")) {
            char job[48]="";
            js_texto(p,f,"job",job,sizeof job);
            if (strcasecmp(job,"director")) continue;
          }
          if (deMetaTmdb(p,f,padrao,&lote[nl])) nl++;
        }
        if (!strcasecmp(fonte.tmdbTipo,"COLLECTION")||
            !strcasecmp(fonte.tmdbTipo,"PERSON")||
            !strcasecmp(fonte.tmdbTipo,"DIRECTOR")) semMais=1;
        else {
          long tp=(long)js_num(corpo,NULL,"total_pages",0);
          long pg=(long)js_num(corpo,NULL,"page",skip);
          if (!tp||pg>=tp||!nl) semMais=1;
        }
        ok=1;
      }
    } else {
      // Lista publica do Trakt: so precisa do client id do aplicativo, nao
      // do token da pessoa (o web faz igual — buildTraktHeaders).
      const char *cli=nuvem_trakt_cliente();
      if (cli[0]) {
        char chave[160];
        const char *cab[]={"trakt-api-version: 2",NULL,NULL};
        snprintf(chave,sizeof chave,"trakt-api-key: %s",cli);
        cab[1]=chave;
        snprintf(url,sizeof url,
          "https://api.trakt.tv/lists/%ld/items/%s?page=%d&limit=40&sort_by=%s&sort_how=%s",
          fonte.traktLista,!strcasecmp(fonte.midia,"TV")?"show":"movie",
          skip,fonte.ordenar[0]?fonte.ordenar:"rank",
          fonte.ordem[0]?fonte.ordem:"asc");
        corpo=rede_baixar_com(url,12,cab);
        if (corpo) {
          const char *p=*corpo=='['?js_raiz_array(corpo):NULL;
          for(;p&&nl<48;p=js_prox(js_fim(p))) {
            const char *f=js_fim(p);raw++;
            if (deMetaTrakt(p,f,fonte.midia,&lote[nl])) nl++;
          }
          // rede_baixar nao devolve cabecalhos; menos que a pagina cheia e o
          // fim da lista (o web le X-Pagination-Page-Count, mesmo efeito).
          if (raw<40) semMais=1;
          ok=1;
        }
      }
    }
    free(corpo);
    // Itens do Trakt chegam sem poster: o lote inteiro ganha arte pelo
    // Cinemeta de uma vez, como a fileira "Continuar assistindo" ja faz.
    if (prov==2&&nl) trakt_enfeitar_lote(lote,nl);
    pthread_mutex_lock(&vtTrava);
    if(generation!=vtGeracao){pthread_mutex_unlock(&vtTrava);continue;}
    { int added=0;
      for(int i=0;i<nl&&vtN<VT_MAX;i++){
        int dup=0;
        for(int j=0;j<vtN;j++)
          if(lote[i].imdb[0]&&!strcmp(vtItens[j].imdb,lote[i].imdb)
             &&!strcmp(vtItens[j].tipo,lote[i].tipo)){dup=1;break;}
        if(!dup){vtItens[vtN++]=lote[i];added++;}
      }
      vtErro=!ok;
      if(ok){if(semMais||!added||vtN>=VT_MAX)vtFim=1;else vtPagina=skip+1;}
      vtFioVivo=0; }
    pthread_mutex_unlock(&vtTrava);
    return NULL;
  }

  int z=0;
  for(const unsigned char *c=(const unsigned char *)genre;*c&&z<(int)sizeof encoded-4;c++) {
    if((*c>='a'&&*c<='z')||(*c>='A'&&*c<='Z')||(*c>='0'&&*c<='9')||*c=='-'||*c=='_')encoded[z++]=*c;
    else {snprintf(encoded+z,4,"%%%02X",*c);z+=3;}
  }encoded[z]=0;
  if(genre[0])snprintf(url,sizeof url,"%s/catalog/%s/%s/genre=%s&skip=%d.json",base,type,id,encoded,skip);
  else if(skip)snprintf(url,sizeof url,"%s/catalog/%s/%s/skip=%d.json",base,type,id,skip);
  else snprintf(url,sizeof url,"%s/catalog/%s/%s.json",base,type,id);
  corpo=rede_baixar(url,10);
  cap=strstr(id,"top100")?100:strstr(id,"top250")?250:VT_MAX;
  pthread_mutex_lock(&vtTrava);
  if(generation!=vtGeracao){pthread_mutex_unlock(&vtTrava);free(corpo);continue;}
  pthread_mutex_unlock(&vtTrava);
  const char *first=corpo?js_array(corpo,NULL,"metas"):NULL;
  int valid=corpo&&strstr(corpo,"\"metas\"");
  int added=0;
  for(const char *p=first;p;p=js_prox(js_fim(p))) {
    const char *f=js_fim(p);raw++;
    CatItem it;
    if (deMeta(p, f, type, &it)) {
      pthread_mutex_lock(&vtTrava);
      int duplicate=0;
      for(int i=0;i<vtN;i++)if(it.imdb[0]&&!strcmp(vtItens[i].imdb,it.imdb)&&!strcmp(vtItens[i].tipo,it.tipo)){duplicate=1;break;}
      if(generation==vtGeracao&&vtN<cap&&!duplicate){vtItens[vtN++]=it;added++;}
      pthread_mutex_unlock(&vtTrava);
    }
  }
  free(corpo);
  pthread_mutex_lock(&vtTrava);
  if(generation!=vtGeracao){pthread_mutex_unlock(&vtTrava);continue;}
  vtErro=!valid;
  // Skip usa quantidade recebida, não 100 presumidos. Muitos addons entregam
  // 20/50 por página. Repetição sem novos ids também termina a paginação.
  if(valid){vtPagina+=raw;if(!raw||!added||vtN>=cap)vtFim=1;}
  vtFioVivo=0;
  pthread_mutex_unlock(&vtTrava);
  return NULL;
  }
}

static void vtDisparar(void) {
  pthread_t t;
  pthread_attr_t at;
  pthread_mutex_lock(&vtTrava);
  if (vtFioVivo || vtFim || (!vtBase[0] && !vtProvedor)) {pthread_mutex_unlock(&vtTrava);return;}
  vtFioVivo = 1;
  vtErro=0;
  // fioVerTudo empilha buffers grandes; no macOS o stack padrao do pthread
  // estoura (collections.sh: Bus error em ___chkstk_darwin).
  pthread_attr_init(&at);
  pthread_attr_setstacksize(&at, 2u * 1024u * 1024u);
  if (pthread_create(&t, &at, fioVerTudo, NULL) != 0) vtFioVivo = 0;
  else pthread_detach(t);
  pthread_attr_destroy(&at);
  pthread_mutex_unlock(&vtTrava);
}

void desc_vertudo_abrir(const char *base, const char *tipo, const char *catId) {
  desc_vertudo_filtro(base,tipo,catId,"");
}
void desc_vertudo_filtro(const char *base, const char *tipo, const char *catId,const char *genre) {
  if (!base || !tipo || !catId) return;
  pthread_mutex_lock(&vtTrava);
  // Mesmo catalogo que ja esta aberto: mantem o que ja foi lido em vez de
  // recomecar do zero (o dono pode ter voltado e entrado de novo).
  if (!vtProvedor && !strcmp(vtBase, base) && !strcmp(vtTipo, tipo) && !strcmp(vtCat, catId)
      && !strcmp(vtGenre,genre?genre:"") && vtN > 0) {
    pthread_mutex_unlock(&vtTrava);
    return;
  }
  snprintf(vtBase, sizeof vtBase, "%s", base);
  snprintf(vtTipo, sizeof vtTipo, "%s", tipo);
  snprintf(vtCat,  sizeof vtCat,  "%s", catId);
  snprintf(vtGenre,sizeof vtGenre,"%s",genre?genre:"");
  vtProvedor=0;
  vtN = 0; vtPagina = 0; vtFim = 0;vtErro=0;vtGeracao++;
  pthread_mutex_unlock(&vtTrava);
  vtDisparar();
}

// Abre uma fonte nao-addon de pasta de colecao (issue #44): "tmdb" ou
// "trakt". Mesmo protocolo do catalogo de addon — geracao nova, memoria
// guardada entre aberturas da mesma fonte — so muda quem responde.
void desc_vertudo_fonte(const ColSource *s) {
  ColSource copia;
  if (!s || !s->prov[0]) return;
  memset(&copia, 0, sizeof copia); copia = *s;
  pthread_mutex_lock(&vtTrava);
  if (vtProvedor && !memcmp(&vtFonte, &copia, sizeof copia) && vtN > 0) {
    pthread_mutex_unlock(&vtTrava); return;
  }
  vtFonte = copia;
  vtProvedor = !strcmp(s->prov, "trakt") ? 2 : 1;
  vtBase[0] = 0;
  vtN = 0; vtPagina = 1; vtFim = 0; vtErro = 0; vtGeracao++;
  pthread_mutex_unlock(&vtTrava);
  vtDisparar();
}

void desc_vertudo_mais(void) { vtDisparar(); }
int  desc_vertudo_n(void) { pthread_mutex_lock(&vtTrava);int n=vtN;pthread_mutex_unlock(&vtTrava);return n; }
int  desc_vertudo_carregando(void) { pthread_mutex_lock(&vtTrava);int n=vtFioVivo;pthread_mutex_unlock(&vtTrava);return n; }
int  desc_vertudo_fim(void) { pthread_mutex_lock(&vtTrava);int n=vtFim;pthread_mutex_unlock(&vtTrava);return n; }
int  desc_vertudo_erro(void) { pthread_mutex_lock(&vtTrava);int n=vtErro;pthread_mutex_unlock(&vtTrava);return n; }
void desc_vertudo_fechar(void) { /* guarda o que leu; ver desc_vertudo_abrir */ }

int desc_vertudo_item(int i, CatItem *dst) {
  int ok = 0;
  pthread_mutex_lock(&vtTrava);
  if (dst && i >= 0 && i < vtN) { memcpy(dst, &vtItens[i], sizeof *dst); ok = 1; }
  pthread_mutex_unlock(&vtTrava);
  return ok;
}

void desc_episodios(int indiceItem, int temporada) {
  if (fioEpVivo) { pendItem = indiceItem; pendTemp = temporada; return; }
  // A lista agora e UNICA e cobre todas as temporadas, entao ter qualquer
  // episodio deste titulo ja basta — trocar de aba nao pede nada.
  (void)temporada;
  if (cat_n_episodios(indiceItem) > 0) return;
  epItem = indiceItem; epTemp = temporada;
  fioEpVivo = 1;
  if (pthread_create(&fioEp, NULL, buscarEps, NULL) != 0) fioEpVivo = 0;
  else pthread_detach(fioEp);
}

int desc_episodios_carregando(int indiceItem) {
  return (fioEpVivo && epItem == indiceItem) || pendItem == indiceItem;
}

// Chamada por quadro por quem desenha, para o pedido guardado sair assim que o
// fio anterior desocupar.
void desc_episodios_pendente(void) {
  int i, t;
  if (fioEpVivo || pendItem < 0) return;
  i = pendItem; t = pendTemp;
  pendItem = -1;
  desc_episodios(i, t);
}

// --- TITULO SOB DEMANDA -------------------------------------------------------
//
// Abrir um credito da filmografia de um ator, ou um item de "Mais como este",
// exige meta de um titulo que o catalogo do dono NAO tem. Antes esses itens
// ficavam apagados e nao abriam, o que deixava a filmografia decorativa.
//
// O meta vem do Cinemeta, a mesma fonte do resto do catalogo, e o item entra no
// FIM do vetor (cat_acrescentar). O tipo nao e conhecido de antemao — o TMDB
// diz "movie"/"tv" no credito, mas o relacionado do Trakt nao —, entao tenta-se
// filme e, se nao houver, serie. Duas chamadas no pior caso, uma no comum.
static char sobId[24];
static long sobTmdb;          // quando > 0, o id do IMDb ainda precisa ser resolvido
static char sobTipo[8];
static int  sobIndice = -1;   // resultado, consumido por desc_titulo_pronto
static int  sobFioVivo;
static pthread_t sobFio;

static void *buscarTitulo(void *arg) {
  char url[200], id[24], *corpo;
  int achou = -1, passo;
  (void)arg;
  snprintf(id, sizeof id, "%s", sobId);

  // O credito de um ator chega com o id do TMDB, nao com o do IMDb — o
  // combined_credits nao traz imdb_id. `external_ids` faz a traducao, e e uma
  // chamada so, feita apenas quando o dono abre o credito.
  if (sobTmdb > 0) {
    const char *chave = desc_chave_tmdb();
    id[0] = 0;
    if (chave && chave[0]) {
      snprintf(url, sizeof url, "%s/%s/%ld/external_ids?api_key=%s", TMDB,
               strcmp(sobTipo, "tv") ? "movie" : "tv", sobTmdb, chave);
      corpo = rede_baixar(url, 15);
      if (corpo) { js_texto(corpo, NULL, "imdb_id", id, sizeof id); free(corpo); }
    }
    if (!id[0] || id[0] != 't') {
      printf("[desc] sob demanda tmdb %ld -> sem imdb\n", sobTmdb); fflush(stdout);
      sobIndice = -1; sobFioVivo = 0; return NULL;
    }
    // Ja temos? Entao e so abrir.
    { int j = cat_indice_por_imdb(id);
      if (j >= 0) { sobIndice = j; sobFioVivo = 0; return NULL; } }
  }

  for (passo = 0; passo < 2 && achou < 0; passo++) {
    const char *tipo = passo ? "series" : "movie";
    snprintf(url, sizeof url, "%s/meta/%s/%s.json", CINEMETA, tipo, id);
    corpo = rede_baixar(url, 20);
    if (!corpo) continue;
    { const char *m = strstr(corpo, "\"meta\"");
      CatItem it;
      if (m && deMeta(m, NULL, tipo, &it)) {
        // O id do proprio pedido manda: o Cinemeta as vezes devolve o campo
        // vazio, e sem ele o titulo entraria no catalogo sem chave e nao
        // poderia ser reaberto nem casar com progresso.
        if (!it.imdb[0]) snprintf(it.imdb, sizeof it.imdb, "%s", id);
        achou = cat_acrescentar(&it);
      } }
    free(corpo);
  }
  printf("[desc] sob demanda %s -> indice %d\n", id, achou); fflush(stdout);
  sobIndice = achou;
  sobFioVivo = 0;
  return NULL;
}

void desc_pedir_titulo_tmdb(long tmdbId, const char *tipo) {
  if (tmdbId <= 0 || sobFioVivo) return;
  sobTmdb = tmdbId;
  snprintf(sobTipo, sizeof sobTipo, "%s", tipo ? tipo : "movie");
  sobId[0] = 0;
  sobIndice = -1;
  sobFioVivo = 1;
  if (pthread_create(&sobFio, NULL, buscarTitulo, NULL) != 0) sobFioVivo = 0;
  else pthread_detach(sobFio);
}

void desc_pedir_titulo(const char *imdb) {
  char id[24];
  const char *dp;
  if (!imdb || imdb[0] != 't' || sobFioVivo) return;
  // Corta o sufixo de episodio, se vier: o meta e do TITULO.
  dp = strchr(imdb, ':');
  if (dp) { size_t k = (size_t)(dp - imdb);
            if (k >= sizeof id) k = sizeof id - 1;
            memcpy(id, imdb, k); id[k] = 0; }
  else snprintf(id, sizeof id, "%s", imdb);
  if (cat_indice_por_imdb(id) >= 0) return;   // ja temos
  snprintf(sobId, sizeof sobId, "%s", id);
  sobTmdb = 0;
  sobIndice = -1;
  sobFioVivo = 1;
  if (pthread_create(&sobFio, NULL, buscarTitulo, NULL) != 0) sobFioVivo = 0;
  else pthread_detach(sobFio);
}

int desc_titulo_pronto(void) { int v = sobIndice; sobIndice = -1; return v; }
int desc_titulo_buscando(void) { return sobFioVivo; }

// Idioma dos textos do TMDB. Antes era so o idioma da interface; agora a
// conta pode escolher outro em Ajustes -> Integracoes (tmdb_language), e
// "Da interface" continua sendo o padrao — ver ajustes_tmdb_idioma().
const char *desc_tmdb_idioma(void) { return ajustes_tmdb_idioma(); }
