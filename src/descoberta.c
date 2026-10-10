#include "descoberta.h"
#include "idioma.h"
#include "ajustes.h"
#include "catordem.h"
#include "cotacat.h"
#include "fileiras.h"
#include "homeestado.h"
#include "sessao.h"
#include "colecoes.h"
#include "marco.h"
#include "metaprov.h"
#include <SDL2/SDL.h>
#include "catalogo.h"
#include "addons.h"
#include "rede.h"
#include "nuvem.h"
#include "cwordem.h"
#include "js.h"
#include "recomenda.h"
#include "trakt.h"
#include "simkl.h"
#include "progresso.h"
#include "contalib.h"
#include "vistoep.h"
#include "proximo.h"
#include "perfis.h"
#include "artereserva.h"
#include "artefontes.h"
#include "idbase.h"
#include "cwfrente.h"
#include "servidores.h"
#include "descdebounce.h"
#include "nlanc.h"
#include <stdint.h>   /* uintptr_t: a geracao viaja no argumento do fio */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

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
      // Ano primeiro (japones, chines, hungaro, lituano): o modelo traduzido
      // tem os argumentos na ordem do portugues e nao alcanca esses quatro.
      { const char ano[5] = { iso[0], iso[1], iso[2], iso[3], 0 };
        if (idioma_data_extenso_especial(ajustes_idioma(), dia, mes,
                                         idioma_mes_data(mes, MES[mes - 1]), ano, dst, tam))
          return; }
      snprintf(dst, tam, i18n("%d de %s de %c%c%c%c"),
               dia, idioma_mes_data(mes, MES[mes - 1]), iso[0], iso[1], iso[2], iso[3]);
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
    // Romeno, ucraniano e russo passam pelo portugues, que e a chave da tabela.
    // A traducao e feita AQUI e nao no desenho porque o genero entra em textos
    // montados ("Filme  ·  Drama"), que nunca casariam com uma chave.
    if (!strcasecmp(g, T[i].en))
      return ajustes_idioma() > IDIOMA_EN ? i18n(T[i].pt) : T[i].pt;
  return g;
}

// --- VALORES CRUS DO TMDB/TRAKT/CINEMETA QUE VAO PARA A TELA ------------------
//
// Status ("Ended"), pais ("United States of America") e duracao ("2h 22min")
// chegam em ingles de TODAS as fontes, qualquer que seja o `language=` do
// pedido: o TMDB localiza titulo e sinopse, nunca esses enums. Como o genero
// acima, a traducao mora aqui e nao no desenho, porque o valor entra em texto
// montado (uma linha de tabela, o selo do hero) que nunca casaria com chave.
// Valor fora da tabela sai como veio — melhor o ingles que um buraco.

// Status da obra -> rotulo em portugues (a chave da tabela), ou NULL quando o
// valor e desconhecido. Aceita as grafias do TMDB ("Returning Series"), do
// Trakt ("returning series", "continuing") e de filme ("Post Production").
// `serie` escolhe o genero da palavra: "Cancelada" / "Cancelado". A chave e
// em caixa de frase; o selo do hero a poe em MAIUSCULAS depois de traduzir.
const char *desc_status_chave(const char *raw, int serie) {
  static const struct { const char *en, *pt; } T[] = {
    { "Released", "Lançado" },           { "Post Production", "Em pós-produção" },
    { "In Production", "Em produção" },  { "Planned", "Planejado" },
    { "Rumored", "Rumor" },              { "Returning Series", "Em exibição" },
    { "Continuing", "Em exibição" },     { "Ended", "Finalizada" },
    { "Pilot", "Piloto" },               { "Renewed", "Renovada" },
    { "Upcoming", "Em breve" },
  };
  size_t i;
  if (!raw || !*raw) return NULL;
  if (!strcasecmp(raw, "Canceled") || !strcasecmp(raw, "Cancelled"))
    return serie ? "Cancelada" : "Cancelado";
  for (i = 0; i < sizeof T / sizeof *T; i++)
    if (!strcasecmp(raw, T[i].en)) return T[i].pt;
  return NULL;
}

// Um nome de pais em ingles -> chave em portugues, ou NULL.
static const char *paisChave(const char *en) {
  static const struct { const char *en, *pt; } T[] = {
    { "United States of America", "Estados Unidos" }, { "United States", "Estados Unidos" },
    { "USA", "Estados Unidos" },   { "US", "Estados Unidos" },
    { "United Kingdom", "Reino Unido" }, { "UK", "Reino Unido" },
    { "Canada", "Canadá" },        { "France", "França" },
    { "Germany", "Alemanha" },     { "Italy", "Itália" },
    { "Spain", "Espanha" },        { "Japan", "Japão" },
    { "South Korea", "Coreia do Sul" }, { "Korea, South", "Coreia do Sul" },
    { "Republic of Korea", "Coreia do Sul" }, { "North Korea", "Coreia do Norte" },
    { "China", "China" },          { "Hong Kong", "Hong Kong" },
    { "Taiwan", "Taiwan" },        { "India", "Índia" },
    { "Brazil", "Brasil" },        { "Mexico", "México" },
    { "Argentina", "Argentina" },  { "Australia", "Austrália" },
    { "New Zealand", "Nova Zelândia" },
    { "Russia", "Rússia" },        { "Russian Federation", "Rússia" },
    { "Ukraine", "Ucrânia" },      { "Poland", "Polônia" },
    { "Sweden", "Suécia" },        { "Norway", "Noruega" },
    { "Denmark", "Dinamarca" },    { "Finland", "Finlândia" },
    { "Netherlands", "Países Baixos" }, { "Belgium", "Bélgica" },
    { "Switzerland", "Suíça" },    { "Austria", "Áustria" },
    { "Ireland", "Irlanda" },      { "Portugal", "Portugal" },
    { "Turkey", "Turquia" },       { "Greece", "Grécia" },
    { "Israel", "Israel" },        { "Egypt", "Egito" },
    { "South Africa", "África do Sul" }, { "Thailand", "Tailândia" },
    { "Indonesia", "Indonésia" },  { "Philippines", "Filipinas" },
    { "Colombia", "Colômbia" },    { "Chile", "Chile" },
    { "Peru", "Peru" },            { "Czech Republic", "República Tcheca" },
    { "Czechia", "República Tcheca" }, { "Hungary", "Hungria" },
    { "Romania", "Romênia" },      { "Bulgaria", "Bulgária" },
    { "Iceland", "Islândia" },     { "Luxembourg", "Luxemburgo" },
    { "Iran", "Irã" },             { "Saudi Arabia", "Arábia Saudita" },
    { "United Arab Emirates", "Emirados Árabes Unidos" },
    { "Nigeria", "Nigéria" },      { "Morocco", "Marrocos" },
    { "Croatia", "Croácia" },      { "Serbia", "Sérvia" },
    { "Cuba", "Cuba" },            { "Venezuela", "Venezuela" },
    { "Uruguay", "Uruguai" },      { "Vietnam", "Vietnã" },
    { "Malaysia", "Malásia" },     { "Singapore", "Singapura" },
    { "Pakistan", "Paquistão" },   { "Lebanon", "Líbano" },
    { "Slovakia", "Eslováquia" },  { "Slovenia", "Eslovênia" },
    { "Estonia", "Estônia" },      { "Latvia", "Letônia" },
    { "Lithuania", "Lituânia" },   { "Belarus", "Bielorrússia" },
    { "Kazakhstan", "Cazaquistão" }, { "Soviet Union", "União Soviética" },
    { "West Germany", "Alemanha Ocidental" }, { "East Germany", "Alemanha Oriental" },
    { "Czechoslovakia", "Tchecoslováquia" }, { "Yugoslavia", "Iugoslávia" },
  };
  size_t i;
  for (i = 0; i < sizeof T / sizeof *T; i++)
    if (!strcasecmp(en, T[i].en)) return T[i].pt;
  return NULL;
}

// "United States of America, Canada" -> "Estados Unidos, Canadá" no idioma da
// interface. Separa por virgula, traduz cada nome e junta com ", ".
void desc_pais_txt(const char *lista, char *dst, size_t tam) {
  size_t o = 0;
  const char *p = lista;
  if (!dst || !tam) return;
  dst[0] = 0;
  while (p && *p) {
    char nome[80];
    const char *fim = strchr(p, ','), *t;
    size_t n = fim ? (size_t)(fim - p) : strlen(p);
    while (n && (*p == ' ')) { p++; n--; }
    while (n && p[n - 1] == ' ') n--;
    if (n && n < sizeof nome) {
      memcpy(nome, p, n); nome[n] = 0;
      t = paisChave(nome);
      t = t ? i18n(t) : nome;
      o += (size_t)snprintf(dst + o, tam - o, "%s%s", o ? ", " : "", t);
      if (o >= tam) { dst[tam - 1] = 0; return; }
    }
    p = fim ? fim + 1 : NULL;
  }
}

// Minutos -> "2h 22min" no idioma da interface (as tres formas sao chaves).
void desc_duracao_min(int min, char *dst, size_t tam) {
  if (!dst || !tam) return;
  if (min <= 0) { dst[0] = 0; return; }
  if (min < 60)      snprintf(dst, tam, i18n("%dmin"), min);
  else if (min % 60) snprintf(dst, tam, i18n("%dh %dmin"), min / 60, min % 60);
  else               snprintf(dst, tam, i18n("%dh"), min / 60);
}

// Duracao em TEXTO ("142 min", "2h 22min", "1 h 54 min", "142") -> a mesma
// forma acima. O Cinemeta escreve "min" em ingles, e em russo/ucraniano a
// abreviacao e outra. Texto que nao e so numero+unidade sai como veio.
void desc_duracao_txt(const char *cru, char *dst, size_t tam) {
  int total = 0, achou = 0;
  const char *p = cru;
  if (!dst || !tam) return;
  dst[0] = 0;
  if (!cru || !*cru) return;
  while (*p) {
    int v = 0;
    if (*p == ' ') { p++; continue; }
    if (*p < '0' || *p > '9') { snprintf(dst, tam, "%s", cru); return; }
    while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; }
    while (*p == ' ') p++;
    if (*p == 'h' || *p == 'H') { total += v * 60; while (*p && *p != ' ' && !(*p >= '0' && *p <= '9')) p++; }
    else if (*p == 'm' || *p == 'M') { total += v; while (*p && *p != ' ' && !(*p >= '0' && *p <= '9')) p++; }
    else if (*p == 0) total += v;
    else { snprintf(dst, tam, "%s", cru); return; }
    achou = 1;
  }
  if (!achou) { snprintf(dst, tam, "%s", cru); return; }
  desc_duracao_min(total, dst, tam);
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

// URL DE ARTE INTEIRA OU NENHUMA (#361). js_texto corta calado no tamanho do
// destino, e um PREFIXO de URL nao e uma URL: o cartaz de provedor com nota
// (pictorium, ~600 caracteres com a query) virava um 404 que ninguem via. Le
// num buffer maior que qualquer campo de arte e so copia se couber; se nao
// couber, o campo fica vazio — quem desenha cai na arte seguinte (metahub,
// cartaz, nome) — e o registro diz qual campo e de quantos bytes.
//   1 = copiada, 0 = ausente, -1 = nao cabe (vazia, com linha no registro).
// `soRaiz`: so a chave da raiz do objeto (js_texto_raiz_em); 0 tenta a raiz e
// depois a primeira de qualquer nivel, a ordem de deMeta (#200).
#define DESC_URL_LIDA 2048
static int urlArteInteira(const char *ini, const char *fim, const char *chave,
                          int soRaiz, char *dst, size_t tam, const char *dono) {
  char v[DESC_URL_LIDA];
  size_t n;
  dst[0] = 0;
  if (!js_texto_raiz_em(ini, fim, chave, v, sizeof v) &&
      (soRaiz || !js_texto(ini, fim, chave, v, sizeof v))) return 0;
  n = strlen(v);
  if (n >= tam) {
    printf("[descoberta] %s de \"%s\" tem %s%zu bytes e o campo guarda %zu: "
           "descartada inteira, nao cortada (#361)\n", chave, dono && dono[0] ? dono : "?",
           n + 1 >= sizeof v ? ">=" : "", n, tam - 1);
    fflush(stdout);
    return -1;
  }
  memcpy(dst, v, n + 1);
  return 1;
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
// `manter`: bits de DESC_MANTER_* dos textos que ja vieram do addon de
// metadados (#176) e o TMDB nao pode trocar.
#define DESC_MANTER_TITULO  1
#define DESC_MANTER_SINOPSE 2
static void fotosDoElenco(CatItem *d, const char *imdbSerie, int serie, int manter) {
  char url[400], *corpo;
  const char *chave;
  long idTmdb = 0;
  char logoAntes[512];
  logoAntes[0] = 0;
  if (d->logo[0]) snprintf(logoAntes, sizeof logoAntes, "%s", d->logo);
  // SEM `if (!d->nElenco) return;`: esta funcao tambem traduz titulo e sinopse
  // (mais abaixo), e um /meta do Cinemeta sem elenco — comum em anime, em
  // titulo novo e em item raso — saia daqui antes de pedir o TMDB, deixando a
  // sinopse em ingles num app em outro idioma. O elenco, que e o que precisa
  // de nomes para casar, fica atras da propria guarda.
  chave = desc_chave_tmdb();            // "" com a integracao desligada
  if (!chave[0]) {
    printf("[desc] elenco %s: sem chave do TMDB (integracao %s)\n", imdbSerie,
           ajustes_tmdb_ligado() ? "ligada, chave vazia" : "desligada");
    fflush(stdout);
    return;
  }
  snprintf(url, sizeof url, "%s/find/%s?api_key=%s&external_source=imdb_id",
           TMDB, imdbSerie, chave);
  corpo = rede_baixar(url, 20);
  // Cada saida muda diz por que: elenco sem foto e sem clique (Samsung, Ted
  // Lasso, 05/10/2026) chegou ao log sem nenhuma linha que apontasse a causa.
  if (!corpo) { printf("[desc] elenco %s: /find do TMDB nao respondeu\n", imdbSerie); fflush(stdout); return; }
  { const char *vet = serie ? "tv_results" : "movie_results";
    const char *p = js_array(corpo, NULL, vet);
    if (p) idTmdb = (long)js_num(p, js_fim(p), "id", 0); }
  free(corpo);
  if (!idTmdb) { printf("[desc] elenco %s: o TMDB nao conhece este id (%s)\n", imdbSerie, serie ? "tv" : "movie"); fflush(stdout); return; }
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
        if (!(manter & DESC_MANTER_TITULO) &&
            js_texto_raiz(corpo, serie ? "name" : "title", t, sizeof t) && t[0])
          snprintf(d->titulo, sizeof d->titulo, "%s", t);
        if (!(manter & DESC_MANTER_SINOPSE) &&
            js_texto_raiz(corpo, "overview", sin, sizeof sin) && sin[0])
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
        // ORDEM (af_tmdb_logo): o do idioma, o ingles, o sem idioma, e
        // nenhuma outra lingua. O do idioma e o PRIMEIRO da lista (o mais
        // votado); era o ultimo, que mudava conforme a ordem da resposta.
        const char *im = strstr(corpo, "\"images\"");
        const char *imObj = im ? strchr(im, '{') : NULL;
        const char *imFim = imObj ? js_fim(imObj) : NULL;
        char base[3] = "", esc[160] = "", escIso[8] = "";
        snprintf(base, sizeof base, "%.2s", desc_tmdb_idioma());
        if (imObj && imFim) {
          size_t tam = (size_t)(imFim - imObj) + 1;
          char *imCopia = (char *)malloc(tam + 1);
          if (imCopia) {
            memcpy(imCopia, imObj, tam); imCopia[tam] = 0;
            af_tmdb_logo(imCopia, base, esc, sizeof esc, escIso, sizeof escIso);
            // O FUNDO do TMDB tambem: o backdrop_path da raiz pode ter letreiro
            // em qualquer lingua (af_tmdb_fundo_padrao).
            if (d->backdropTmdb[0]) {
              const char *fp = strstr(d->backdropTmdb, "/t/p/w1280");
              char ok[160];
              if (fp && af_tmdb_fundo_padrao(imCopia, base, fp + 10, ok, sizeof ok))
                snprintf(d->backdropTmdb, sizeof d->backdropTmdb,
                         "https://image.tmdb.org/t/p/w1280%s", ok);
              else if (fp) d->backdropTmdb[0] = 0;
            }
            free(imCopia);
          }
        }
        // "LOGO DO ADDON" (Ajustes, desligado de fabrica): o logo que o addon
        // mandou no catalogo fica; o do TMDB so entra em quem nao tinha.
        int manterAddon = ajustes_logo_addon() && d->origem[0] && logoAntes[0] &&
                          !ehSvg(logoAntes) && strcmp(logoAntes, d->poster);
        { if (esc[0] && !manterAddon) {
            snprintf(d->logo, sizeof d->logo,
                     "https://image.tmdb.org/t/p/w500%s", esc);
            snprintf(d->logoIdioma, sizeof d->logoIdioma, "%s", escIso);
            snprintf(d->logoIdiomaUrl, sizeof d->logoIdiomaUrl, "%s", d->logo);
          } }
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
  if (ajustes_tmdb_elenco() && d->nElenco > 0) {
    // language= tambem aqui: o nome do PERSONAGEM (`character`) vem localizado
    // quando o TMDB tem (o mesmo pedido, so um parametro a mais).
    snprintf(url, sizeof url, "%s/%s/%ld/credits?api_key=%s&language=%s",
             TMDB, serie ? "tv" : "movie", idTmdb, chave, desc_tmdb_idioma());
    corpo = rede_baixar(url, 20);
    if (corpo) {
      int casados = desc_tmdb_elenco(corpo, d), comFoto = 0, k;
      for (k = 0; k < d->nElenco; k++) comFoto += d->elenco[k].foto[0] != 0;
      printf("[desc] elenco %s: %d nome(s), %d casaram com o TMDB, %d com foto\n", imdbSerie, d->nElenco, casados, comFoto);
      free(corpo);
    } else printf("[desc] elenco %s: /credits do TMDB nao respondeu\n", imdbSerie);
    fflush(stdout);
  } else {
    printf("[desc] elenco %s: sem fotos (%s)\n", imdbSerie,
           !ajustes_tmdb_elenco() ? "\"Elenco do TMDB\" desligado nos ajustes" : "a ficha veio sem nomes");
    fflush(stdout);
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
// "Ocultar nao lancados" (hideUnreleasedContent) nas fileiras da Home e na grade
// de colecao / Ver tudo; ver nlanc.h. Desligado, nunca esconde nada.
static int descOcultaNaoLancado(const CatItem *it) {
  time_t t;
  struct tm tmv;
  if (!ajustes_ocultar_nao_lancados()) return 0;
  t = time(NULL);
  if (!gmtime_r(&t, &tmv)) return 0;
  return nlanc_meta_futuro(it->meta, tmv.tm_year + 1900);
}

// --- DE QUAL ADDON VEIO O ITEM (CatItem.origem) ------------------------------
//
// O detalhe pergunta a ficha (/meta) primeiro a quem PUBLICOU o titulo, e para
// isso o item precisa lembrar quem foi. Guarda-se o "id" do manifesto — nunca a
// base: ela carrega credencial (o Xperience embute um JWT no caminho) e o
// CatItem vai inteiro para o cache em disco. Enquanto o manifesto nao foi lido
// (id vazio) vale "#<hash da base>", que tambem nao expoe nada.
static unsigned hashBaseAddon(const char *b) {
  unsigned h = 2166136261u;
  for (; b && *b; b++) h = (h ^ (unsigned char)*b) * 16777619u;
  return h;
}

// Cinemeta e "origem" tambem, mas nao e um addon da lista do usuario (ou e, com
// outro nome): so o texto "cinemeta" na base o identifica.
static int baseEhCinemeta(const char *base) {
  return base && (strstr(base, "cinemeta") != NULL ||
                  strstr(base, "catalog.nuvio.tv") != NULL);
}

// #303-bis (Cinemeta ainda aparecia na busca): com "Buscar no Cinemeta" desligado,
// o que e do addon Cinemeta sai da busca em TODO lugar, nao so dos alvos de
// rede: tambem das fileiras locais (busca.c, spotlight.c), que varrem as
// fileiras da Home. Casa pela base (o Cinemeta e "cinemeta" na URL), sem tocar
// no catalogo do Nuvio.
int desc_busca_base_oculta(const char *base) {
  char b[NV_ADDON_URL_MAX];
  size_t i;
  if (ajustes_busca_cinemeta() || !base || !base[0]) return 0;
  for (i = 0; base[i] && i + 1 < sizeof b; i++)
    b[i] = (char)((base[i] >= 'A' && base[i] <= 'Z') ? base[i] + 32 : base[i]);
  b[i] = 0;
  return strstr(b, "cinemeta") != NULL;
}

static void origemDaBase(const char *base, char *dst, size_t n) {
  int i, k;
  if (!n) return;
  dst[0] = 0;
  if (!base || !base[0]) return;
  if (baseEhCinemeta(base)) { snprintf(dst, n, "cinemeta"); return; }
  k = addons_n();
  for (i = 0; i < k; i++) {
    const char *b = addons_base(i);
    if (b && !strcmp(b, base)) {
      const char *id = addons_id_manifesto(i);
      if (id && id[0]) snprintf(dst, n, "%s", id);
      else snprintf(dst, n, "#%08x", hashBaseAddon(base));
      return;
    }
  }
}

// O addon da lista a que `origem` se refere, ou -1 (Cinemeta, vazia, addon que
// saiu da lista).
static int addonDaOrigem(const char *origem) {
  int i, k;
  if (!origem || !origem[0] || !strcmp(origem, "cinemeta")) return -1;
  k = addons_n();
  for (i = 0; i < k; i++) {
    if (origem[0] == '#') {
      const char *b = addons_base(i);
      char h[16];
      snprintf(h, sizeof h, "#%08x", hashBaseAddon(b));
      if (b && b[0] && !strcmp(h, origem)) return i;
    } else {
      const char *id = addons_id_manifesto(i);
      if (id && id[0] && !strcmp(id, origem)) return i;
    }
  }
  return -1;
}

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
  char *corpo;
  const char *p;
  int n = 0;
  corpo = metaprov_busca_com(tipo, termo, METAPROV_NUVIO_S, 20, metaprov_get_rede,
                             NULL, NULL);
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

// O PEDIDO A UM ADDON COUBE NO BUFFER? `w` e o retorno do snprintf que o
// montou (addonurl.h). Pedido cortado nao sai: o addon responderia a OUTRA URL
// como se fosse a certa. O log leva o nome do addon e o tamanho, nunca a URL —
// nela viaja a chave de debrid.
static int descPedidoCoube(const char *base, int w, size_t tam) {
  const char *nome = "";
  int i;
  if (w >= 0 && (size_t)w < tam) return 1;
  for (i = 0; base && i < addons_n(); i++)
    if (!strcmp(addons_base(i), base)) { nome = addons_nome(i); break; }
  return nv_addon_pedido_coube(nome, w, tam);
}

typedef struct {
  // NV_ADDON_URL_MAX e nao 300: com 300 a base do Xperience (367) e a do Comet
  // (870) eram cortadas e a busca perguntava a outra URL, em silencio (#201).
  char base[NV_ADDON_URL_MAX];
  char tipo[8];
  char id[96];
  char titulo[96];
  char addon[64];
  int  nuvio;           // 1 = catalogo do Nuvio, com o Cinemeta de reserva
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
static int  respGeracao = -1;   // geracao da ultima resposta aceita (desc_busca_chegou)
static int  proximoAlvo;        // fila de trabalho: proximo indice a consultar
static int  fiosVivos;

void desc_alvos_busca_zerar(void) {
  pthread_mutex_lock(&buscaTrava);
  // O catalogo do Nuvio entra SEMPRE e primeiro (o Cinemeta so responde por
  // ele, quando ele falha): e a fonte que nao depende de addon nenhum, entao a
  // busca continua funcionando numa instalacao limpa.
  nAlvos = 0;
  { int t; const char *tt[2] = { "movie", "series" };
    const char *rot[2] = { "Filmes", "Séries" };
    for (t = 0; t < 2; t++) {
      AlvoBusca *a = &alvos[nAlvos++];
      snprintf(a->base,  sizeof a->base,  "%s", METAPROV_NUVIO_HOST);
      snprintf(a->tipo,  sizeof a->tipo,  "%s", tt[t]);
      snprintf(a->id,    sizeof a->id,    "%s", "popular");
      snprintf(a->titulo,sizeof a->titulo,"%s", rot[t]);
      snprintf(a->addon, sizeof a->addon, "%s", "Nuvio");
      a->nuvio = 1;
    } }
  memset(resAlvo, 0, sizeof resAlvo);
  // ESQUECE O TERMO JA PEDIDO. Sem isto, depois que a lista de addons muda (um
  // addon novo, o ciclo de sync) os resultados eram apagados aqui mas
  // desc_buscar(o mesmo texto) voltava cedo por "termo igual" e nao consultava
  // os alvos novos: a busca ficava so com as pessoas, que vem do TMDB por outro
  // caminho (dono, 05/10: "o spotlight so ta achando pessoas"). Subir a geracao
  // tambem descarta o que os fios ainda estiverem trazendo dos alvos antigos,
  // cujos indices ja nao valem.
  buscaPedido[0] = 0;
  buscaTermo[0] = 0;
  proximoAlvo = 0;
  geracao++;
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
    a->nuvio = 0;
  } else {
    // Teto cheio: o alvo NAO sera consultado. Antes era em silencio, e quem
    // tem addons demais via a busca "nao achar" o titulo local sem nenhuma
    // pista do porque. So o nome do addon vai ao log (a URL leva credencial).
    printf("[desc] busca: teto de %d alvos; \"%s\" (%s) fora da busca\n",
           BUSCA_ALVOS, titulo ? titulo : "", addon ? addon : "");
  }
  pthread_mutex_unlock(&buscaTrava);
}

// Consulta UM alvo. Devolve quantos itens leu.
static int consultarAlvo(const AlvoBusca *a, const char *termo,
                         CatItem *saida, int max) {
  char url[NV_ADDON_PEDIDO_MAX], esc[300];
  char *corpo;
  const char *p;
  int n = 0;
  // 6 s por alvo, como o web (SEARCH_CATALOG_TIMEOUT 6500). Addon lento nao
  // trava a tela: a fileira dele so aparece quando chegar, e as outras ja
  // estao la. O alvo do Nuvio tem 5 s e, se falhar, o Cinemeta mais 6.
  // #231: com "Buscar no Cinemeta" desligado, nem o add-on Cinemeta nem a reserva
  // do catalogo do Nuvio (seg_cine < 0) respondem.
  if (a->nuvio && ajustes_busca_nuvio() == 2) return 0;   // #311: Nuvio fora da busca
  if (!a->nuvio && !ajustes_busca_cinemeta() &&
      (baseEhCinemeta(a->base) || desc_busca_base_oculta(a->base) ||
       !strcasecmp(a->addon, "cinemeta"))) return 0;
  if (a->nuvio) {
    corpo = metaprov_busca_com(a->tipo, termo, METAPROV_NUVIO_S,
                               ajustes_busca_cinemeta() ? 6 : -1,
                               metaprov_get_rede, NULL, NULL);
  } else {
    urlEscapar(termo, esc, sizeof esc);
    if (addons_base_desligada(a->base)) return 0;
    if (!descPedidoCoube(a->base, nv_addon_url(url, sizeof url, a->base, "/catalog/%s/%s/search=%s.json",
                                           a->tipo, a->id, esc), sizeof url)) return 0;
    corpo = rede_baixar(url, 6);
  }
  if (!corpo) return 0;
  p = js_array(corpo, NULL, "metas");
  { char orig[96];
    origemDaBase(a->base, orig, sizeof orig);   // o resultado abre pelo addon que o achou
    while (p && n < max) {
      const char *f = js_fim(p);
      if (deMeta(p, f, a->tipo, &saida[n]) &&
          // O Nuvio devolve "tmdb:<id>" para quem nao tem IMDb; o resultado fica
          // no formato tt... (o Cinemeta de reserva nao entende tmdb:).
          (!a->nuvio || !strncmp(saida[n].imdb, "tt", 2))) {
        snprintf(saida[n].origem, sizeof saida[n].origem, "%s", orig);
        n++;
      }
      p = js_prox(f);
    } }
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
      respGeracao = g;
      snprintf(buscaTermo, sizeof buscaTermo, "%s", termo);
    }
    pthread_mutex_unlock(&buscaTrava);
  }
}

static int cinemetaVisto = 1;   // ultimo valor de ajustes_busca_cinemeta()/_nuvio() visto aqui (#311: junta os dois)

void desc_buscar(const char *termo) {
  int k, faltam;
  if (!termo) return;
  pthread_mutex_lock(&buscaTrava);
  // Mudar "Buscar no Cinemeta" com a mesma palavra ainda guardada nao pode
  // devolver o resultado de antes: o interruptor invalida o termo pedido.
  { int cine = (ajustes_busca_cinemeta() ? 1 : 0) | (ajustes_busca_nuvio() == 2 ? 2 : 0);
    if (cine != cinemetaVisto) { cinemetaVisto = cine; buscaPedido[0] = 0; buscaTermo[0] = 0; } }
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

// Ja chegou ALGUMA resposta para este termo? Ate la a tela mantem as fileiras
// do termo anterior (sem piscar); depois, so as do termo novo.
int desc_busca_chegou(const char *termo) {
  int r;
  pthread_mutex_lock(&buscaTrava);
  r = termo && !strcmp(termo, buscaTermo) && respGeracao == geracao;
  pthread_mutex_unlock(&buscaTrava);
  return r;
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

int desc_busca_alvo_nuvio(int alvo) {
  return (alvo >= 0 && alvo < nAlvos) ? alvos[alvo].nuvio : 0;
}

// #311: quantas FONTES (nomes de addon distintos) a busca consulta de fato, com
// "Nuvio" e "Cinemeta" ja descontados quando desligados. Com uma so, o nome da
// fonte sob o grupo e ruido: a tela o omite. Conta alvos, nao resultados, para
// o rotulo nao aparecer e sumir enquanto as respostas chegam.
int desc_busca_n_fontes(void) {
  int i, j, n = 0;
  for (i = 0; i < nAlvos; i++) {
    const AlvoBusca *a = &alvos[i];
    int repetida = 0;
    if (a->nuvio ? ajustes_busca_nuvio() == 2
                 : (!ajustes_busca_cinemeta() &&
                    (baseEhCinemeta(a->base) || desc_busca_base_oculta(a->base) ||
                     !strcasecmp(a->addon, "cinemeta")))) continue;
    for (j = 0; j < i && !repetida; j++)
      if (!strcasecmp(alvos[j].addon, a->addon)) repetida = 1;
    if (!repetida) n++;
  }
  return n;
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
// 0 = nao espere; 1 = espere (perfil ainda nao escolhido); 2 = espere, com prazo.
static int (*esperaAddons)(void);
void desc_espera_addons_definir(int (*f)(void)) { esperaAddons = f; }
// Lido pelo fio de montagem no fim do ciclo e escrito pelo laco principal.
// `volatile` porque sao fios diferentes; nao ha corrida real de valor — o pior
// caso e uma remontagem a mais, que e barata perto de perder o pedido.
static volatile int repetirAoFim;
// DEBOUNCE (descdebounce.h): quando a ultima volta comecou e se ha inicio agendado.
static NvDescDeb descDeb;
static void descIniciarAdiado(void);
// Pedido da PESSOA (desc_repetir: troca de perfil, idioma, addons, ordem) nao
// passa pelo minimo entre voltas: condena a volta no ar e a seguinte comeca ja.
// Sem isso a volta do perfil que saiu (recente, nao condenada) publicava as
// fileiras dele na Home do perfil novo (#294) e o pedido esperava ate 10 s.
static volatile int descPedidoPessoa;
// Sob listaTrava (junto com buscando e descDeb): o fio adiado leva a marca de
// quando foi agendado e confere contra esta para saber se ja foi atendido.
static volatile unsigned descVoltas;
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
// A VOLTA NO AR JA LEU A LISTA DE ADDONS? Sob listaTrava junto com geracaoLida,
// e e o que desc_repetir_addons consulta para decidir se o pedido ainda e
// atendido por esta volta (nao leu: vai ler a lista nova) ou se ela esta
// condenada (ja leu a velha).
//
// MEDIDO na C9 do dono (1.4.6-dev), escolhendo o perfil 1: "montar: inicio" e
// "[sync] addons: perfil 1 -> 12 linha(s)" no MESMO segundo, a lista lida ~12 s
// depois (depois do Trakt) — portanto a nova —, e mesmo assim a volta inteira
// (45 s: Trakt, 30 s de manifestos, catalogos) foi descartada no fim porque o
// pedido do sync tinha trocado montagemGeracao. Depois, uma segunda volta
// completa.
static pthread_mutex_t listaTrava = PTHREAD_MUTEX_INITIALIZER;
static int listaLidaNaVolta;
// Fim de volta: `buscando` cai sob a mesma trava em que desc_iniciar o testa e marca.
static void buscandoSoltar(void) {
  pthread_mutex_lock(&listaTrava); buscando = 0; pthread_mutex_unlock(&listaTrava);
}
// O QUE ESTA NA TELA E PARCIAL: publicado em partes por uma volta que nao
// chegou ao fim (condenada no meio, ou ainda no ar). A volta seguinte continua
// publicando em partes por cima disso em vez de montar em silencio: sem isto,
// a volta recomecada via cat_n() > 0 (o "Continuar" que a condenada publicou)
// e so mostrava as fileiras da rede no fim.
static volatile int parcialNaTela;
// Ver desc_catalogos_fora em descoberta.h.
static int catalogosFora;
// Quantos catalogos NAO DESLIGADOS o teto impediu de pedir na ultima montagem,
// sem o clamp de catalogosFora. Ver o uso em desc_remontar_fileiras.
static int catalogosNaoPedidos;
static pthread_t fioEp;
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
  //
  // ARTE DA RAIZ DO ITEM (#200). O AIOMetadata manda, no meta que sai do cache
  // dele, `_providerArt: {poster, background, logo}` (a arte do TMDB no idioma
  // configurado) ANTES do `poster` da raiz — que e o ja trocado pela URL de
  // cartaz personalizada (BetterPosters, PostersPlus). js_texto acha a
  // primeira chave de qualquer nivel: o card mostrava o cartaz do TMDB, sem a
  // nota, so nos titulos que vinham do cache dele. A raiz manda; o aninhado so
  // entra quando a raiz nao tem o campo (era o comportamento de antes).
  //
  // Poster comprido demais ate para os 1024 (#361): o titulo fica, sem cartaz
  // e com a linha no registro — sumir da fileira seria o defeito calado.
  if (!urlArteInteira(ini, fim, "poster", 0, d->poster, sizeof d->poster, d->titulo))
    return 0;
  urlArteInteira(ini, fim, "background", 0, d->backdrop, sizeof d->backdrop, d->titulo);
  snprintf(d->backdropCatalogo, sizeof d->backdropCatalogo, "%s", d->backdrop);
  if (strstr(d->backdrop, "image.tmdb.org/t/p/"))
    snprintf(d->backdropTmdb, sizeof d->backdropTmdb, "%s", d->backdrop);
  if (strstr(d->backdrop, "media.trakt.tv/"))
    snprintf(d->backdropTrakt, sizeof d->backdropTrakt, "%s", d->backdrop);
  urlArteInteira(ini, fim, "logo", 0, d->logo, sizeof d->logo, d->titulo);
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
  // So se o poster CABE no fundo (512 contra 1024, #361): o cartaz de ~600
  // caracteres entrava aqui cortado, e o card deitado, o destaque e a busca
  // pedem `backdrop` antes do poster — o 404 era deles. Sem fundo, todos ja
  // caem no metahub pelo id e depois no proprio poster inteiro.
  if (!d->backdrop[0] && strlen(d->poster) < sizeof d->backdrop)
    snprintf(d->backdrop, sizeof d->backdrop, "%s", d->poster);
  // A variante de catálogo é a mesma arte que alimenta o card, já com a
  // dimensão segura para a TV. O TMDB/Trakt ficam em campos separados quando
  // chegam por seus próprios caminhos.
  snprintf(d->backdropCatalogo, sizeof d->backdropCatalogo, "%s", d->backdrop);
  if (d->backdropTmdb[0] && strstr(d->backdropTmdb, "image.tmdb.org/t/p/"))
    snprintf(d->backdropTmdb, sizeof d->backdropTmdb, "%s", d->backdrop);

  if (!js_texto(ini, fim, "imdb_id", d->imdb, sizeof d->imdb))
    js_texto(ini, fim, "id", d->imdb, sizeof d->imdb);
  snprintf(d->tipo, sizeof d->tipo, "%s", tipo);
  // O TIPO DO PROPRIO ITEM VENCE O DO CATALOGO. Catalogo de anime do
  // AIOMetadata declara `type: "anime"` (simkl.trending.anime, mal.*), e o app
  // inteiro decide filme x serie por `tipo == "series"`: "anime" virava filme,
  // o /meta ia ao Cinemeta como /meta/movie/tt13293588 e voltava "My Way Home"
  // (1965, dir. Miklos Jancso) — o detalhe do Mushoku Tensei sem episodios e
  // com o diretor de outro titulo (relato do Mizikashi1, v1.4.2). Cada meta do
  // Stremio traz o proprio `type`; quando ele e filme ou serie, e ele que vale.
  // Na raiz do item: `trailers[]` tambem tem "type" ("Trailer").
  { char proprio[16];
    if (js_texto_raiz_em(ini, fim, "type", proprio, sizeof proprio) &&
        (!strcmp(proprio, "movie") || !strcmp(proprio, "series")))
      snprintf(d->tipo, sizeof d->tipo, "%s", proprio);
    tipo = d->tipo; }
  // O ID DO TMDB QUE O CATALOGO JA TRAZ (23/09/2026). O Cinemeta manda
  // `moviedb_id` em cada item do catalogo — 49 de 49 filmes e 49 de 50 series
  // do topo, e nos 29 conferidos ele e o mesmo id que o /find devolve. Com ele
  // a arte do TMDB no destaque nao paga o /find (artereserva.c). So filme e
  // serie: o id e de /movie ou /tv, e um canal nao tem nenhum dos dois.
  if (!strcmp(tipo, "movie") || !strcmp(tipo, "series")) {
    long mdb = (long)js_num(ini, fim, "moviedb_id", 0.0);
    if (mdb > 0) d->tmdb = mdb;
  }
  // O PAIS, que o catalogo do Cinemeta tambem traz ("Japan", "United States,
  // Canada"). O /meta regrava ao abrir; aqui ele serve a fonte "Anime" do
  // destaque, que so vale para Animacao + Japao (artehero.c).
  js_texto(ini, fim, "country", d->pais, sizeof d->pais);

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
    metaprov_duracao(dur, sizeof dur);   // "148m" do catalogo do Nuvio -> "148 min"
    // "2024–" vira "2024": o travessao de serie em andamento polui a linha. O
    // catalogo do Nuvio usa hifen ASCII ("2011-2019"): mesmo corte, so depois
    // de um ano de 4 digitos (data ISO inteira nao chega aqui).
    { char *tr = strstr(v, "\xe2\x80\x93"); if (tr) *tr = 0; }
    if (v[0] >= '0' && v[0] <= '9' && v[1] && v[2] && v[3] && v[4] == '-') v[4] = 0;
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
  char url[NV_ADDON_PEDIDO_MAX];
  char *corpo;
  const char *p;
  int n = 0;
  if (respondeu) *respondeu = 0;
  if (addons_base_desligada(base)) return 0;   // desligado na conta: nenhuma rede
  if (!descPedidoCoube(base, nv_addon_url(url, sizeof url, base, "/catalog/%s/%s.json", tipo, id),
                       sizeof url)) return 0;
  // 8 s e nao 25: um addon fora do ar segurava um dos tres fios por 25 s, e a
  // fileira dele atrasa TODAS as seguintes porque a montagem caminha em ordem.
  // E a mesma licao ja registrada no cache de texturas — la o timeout caiu de
  // 25 para 8 pelo mesmo motivo, com duas URLs mortas travando os dois fios de
  // decode. Um catalogo que nao responde em 8 s nao vai responder.
  corpo = rede_baixar(url, 8);
  if (!corpo) return 0;
  if (respondeu) *respondeu = 1;
  p = js_array(corpo, NULL, "metas");
  { char orig[96];
    origemDaBase(base, orig, sizeof orig);      // ver CatItem.origem
    while (p && n < max && n < quantos) {
      const char *f = js_fim(p);
      if (deMeta(p, f, tipo, &saida[n]) && !descOcultaNaoLancado(&saida[n])) {
        snprintf(saida[n].origem, sizeof saida[n].origem, "%s", orig);
        n++;
      }
      p = js_prox(f);
    } }
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
#define MAX_POR_FILEIRA DESC_ITENS_POR_FILEIRA   // ver descoberta.h (#163)

// filsMontadas = as janelas do bloco QUE ESTA PUBLICADO, e nada alem disso.
// desc_remontar_fileiras republica este vetor por cima do bloco da tela sem
// trocar os itens, entao um `ini` daqui que foi calculado sobre OUTRO lote
// aponta para itens alheios. Medido na LG C9 (log da integracao com o Codex):
// a montagem descartada pela troca de geracao deixava aqui as janelas do lote
// que foi para o free(); o sync remontava por cima do catalogo do PACOTE e
// "Amigos assistindo" (ini=12 n=2) virava "The Martian"/"Project Hail Mary"
// sem nome. Por isso montar() monta em filsLote e so copia para ca junto com
// o cat_definir_tudo que publica aquele mesmo lote. tests/homejanelas.sh.
_Static_assert(FIL_LIMITE_MAX <= CAT_FIL_MAX, "o limite escolhido em Ajustes cabe no vetor de fileiras");
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
// #327: chave da fileira baixada so para alimentar o destaque (vazia = nenhuma).
static char heroAlimChave[192];

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
  char chaveGrupo[192];
  if (!base || !base[0]) return 0;
  f = col_por_catalogo(base, tipo, id);
  if (!f) return 0;
  col_chave_pasta(f, chaveGrupo, sizeof chaveGrupo);
  if (fil_oculta(chaveGrupo) || catordem_oculta(chaveGrupo, chaveGrupo)) return 0;
  return 1;
}

static int dentroDeColecaoVisivel(const Decl *d) {
  // #327: o que a pessoa adicionou a Home na TV nao e engolido pela pasta.
  if (fil_adicionada_na_tv(d->chave)) return 0;
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
  char  url[NV_ADDON_PEDIDO_MAX];   // <base>/manifest.json inteiro (addonurl.h)
  char *corpo;
  int   ativo, erro;
  int   pronto;      // 1 = tentativa terminada (corpo pode ser NULL)
  int   largado;     // 1 = a montagem desistiu de esperar por ele nesta volta
} mani[MANI_MAX];
// A MONTAGEM NAO ESPERA UM MANIFESTO LENTO PARA SEMPRE (B2, arranque). Os
// manifestos eram lidos em ordem e cada um esperava ate o download de 20 s: um
// addon pendurado (baby-beamup, "falha 28 ... 20009 ms" no log da C9) segurava
// TODOS os catalogos, que so comecam depois de "manifestos lidos". Agora a
// espera por um manifesto acaba em MANI_ESPERA_MS contados da largada (com
// MANI_ESPERA_FOLGA_MS minimos a partir do pedido); o addon segue baixando, o
// corpo entra na cache, e a volta seguinte o encontra pronto. Addon lento mas
// valido nao se perde: ele so entra uma volta depois.
#ifndef MANI_ESPERA_MS
#define MANI_ESPERA_MS 6000
#endif
#ifndef MANI_ESPERA_FOLGA_MS
#define MANI_ESPERA_FOLGA_MS 1500
#endif
static unsigned long long maniLargadaMs;
static unsigned long long descAgoraMs(void);

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
  char    url[NV_ADDON_PEDIDO_MAX];
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

static pthread_mutex_t cargaTrava = PTHREAD_MUTEX_INITIALIZER;
// 1 = a volta em curso foi pedida em segundo plano (desc_repetir_silencioso).
// Um pedido da pessoa (desc_repetir) a derruba para 0; o contrario nao a
// esconde. Escrita so no fio principal.
static volatile int montSilenciosa;
static DescHomeCarga carga;
static Uint32 cargaDesde;
static void cargaFase(int fase) {
  pthread_mutex_lock(&cargaTrava); carga.fase = fase; pthread_mutex_unlock(&cargaTrava);
}
void desc_home_carga(DescHomeCarga *estado) {
  if (!estado) return;
  pthread_mutex_lock(&cargaTrava);
  // O ALERTA DA ILHA (app.c) SO ACENDE PARA UMA VOLTA VISIVEL: a de arranque ou
  // a que a pessoa pediu. O refazer do "Continuar assistindo" (cwVivo: a cada
  // 10 min, a cada saida do player, a cada sync) e as voltas silenciosas do
  // sync nao entram — desc_montando() segue contando as duas para quem espera
  // a home (troca de perfil). Prova: tests/homecarga_ilha.sh.
  carga.ativo = buscando && !montSilenciosa;
  if (carga.ativo && cargaDesde) carga.ms = SDL_GetTicks() - cargaDesde;
  *estado = carga;
  pthread_mutex_unlock(&cargaTrava);
  pthread_mutex_lock(&maniTrava);
  estado->addonsProntos = estado->addonsTotal = 0;
  for (int i = 0; i < maniN; i++) if (mani[i].ativo) {
    estado->addonsTotal++;
    if (mani[i].pronto) { estado->addonsProntos++; estado->falhas += mani[i].erro; }
  }
  pthread_mutex_unlock(&maniTrava);
}

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
    char url[NV_ADDON_PEDIDO_MAX];
    pthread_mutex_lock(&maniTrava);
    if (minha == maniGeracao)
      while (maniProx < maniN && mani[maniProx].pronto) maniProx++;
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
    if (mani[meu].largado && corpo) {
      // Chegou depois de a montagem seguir sem ele: ninguem o consome nesta
      // volta. Fica na cache para a proxima; se nao ha volta no ar, pede uma
      // (silenciosa) para o addon entrar agora e nao so em 5 min.
      free(corpo);
      corpo = NULL;
      mani[meu].pronto = 1;
      pthread_cond_broadcast(&maniCond);
      pthread_mutex_unlock(&maniTrava);
      printf("[desc] manifesto lento chegou depois da montagem seguir sem ele; fica para a proxima volta\n");
      fflush(stdout);
      if (!buscando) desc_repetir_silencioso();
      continue;
    }
    mani[meu].corpo = corpo;
    mani[meu].erro = !corpo;
    mani[meu].pronto = 1;
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
  maniN = 0;
  for (int pass = 0; pass < 2; pass++)
  for (int ad = 0; ad < nAd && maniN < MANI_MAX; ad++) {
    if (!!addons_ativo(ad) != (pass == 0)) continue;
    i = maniN++;
    mani[i].ativo = addons_ativo(ad);
    int cache;
    nv_addon_url(mani[i].url, sizeof mani[i].url, addons_base(ad), "/manifest.json");
    mani[i].pronto = 0; mani[i].erro = 0;
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
  maniLargadaMs = descAgoraMs();
  for (i = 0; i < maniN; i++) mani[i].largado = 0;
  // Ninguem mais espera por um `pronto` que a volta passada deixou pendente.
  pthread_cond_broadcast(&maniCond);
  int pendentes = 0;
  for (i = 0; i < maniN; i++) if (!mani[i].pronto) pendentes++;
  { unsigned g = maniGeracao;
    pthread_mutex_unlock(&maniTrava);
  if (maniN < 1) return;
  // So dispara fios para os slots que NAO vieram da cache. Os outros ja
  // estao prontos e maniPegar os entrega na hora.
  for (i = 0; i < MANI_FIOS && i < pendentes; i++) {
    if (pthread_create(&fios[criados], NULL, fioManifesto,
                       (void *)(uintptr_t)g) == 0) {
      pthread_detach(fios[criados]);
      criados++;
    }
  }
  // Sem fio nenhum o corpo fica NULL e `pronto` fica 0: maniPegar percebe que
  // ninguem esta baixando e baixa no proprio fio, como sempre foi.
  if (!criados && pendentes > 0) {
    pthread_mutex_lock(&maniTrava);
    maniN = 0;
    pthread_cond_broadcast(&maniCond);
    pthread_mutex_unlock(&maniTrava);
  }
  }   // fecha o bloco `{ unsigned g = maniGeracao;`
}

// O corpo do manifesto de `url`, esperando o download largado por maniLargar se
// ele ainda estiver em curso. A posse passa para quem chamou.
static char *maniPegar(const char *url, int esperar, int *tentado) {
  int i;
  *tentado = 0;
  char *corpo = NULL;
  pthread_mutex_lock(&maniTrava);
  for (i = 0; i < maniN; i++) if (!strcmp(mani[i].url, url)) break;
  if (i < maniN) {
    unsigned g = maniGeracao;
    // A espera acaba tambem quando a volta vira: nesse caso o corpo daqui nao
    // serve mais a ninguem e quem chamou baixa por conta propria.
    // Espera em fatias de 10 ms (portavel, sem relogio absoluto de pthread)
    // ate o prazo de MANI_ESPERA_MS da largada, com a folga minima a partir de
    // agora para quem chega tarde (o Trakt costuma levar mais que isso).
    { unsigned long long prazo = maniLargadaMs + MANI_ESPERA_MS,
                         minimo = descAgoraMs() + MANI_ESPERA_FOLGA_MS;
      if (prazo < minimo) prazo = minimo;
      while (esperar && !mani[i].pronto && g == maniGeracao) {
        if (descAgoraMs() >= prazo) {
          mani[i].largado = 1;
          printf("[desc] manifesto lento: %s; a montagem segue sem ele (%llu ms desde a largada)\n",
                 url, (unsigned long long)(descAgoraMs() - maniLargadaMs));
          fflush(stdout);
          break;
        }
        pthread_mutex_unlock(&maniTrava);
        SDL_Delay(10);
        pthread_mutex_lock(&maniTrava);
      } }
    if (g == maniGeracao) {
      *tentado = 1;
      if (mani[i].pronto) { corpo = mani[i].corpo; mani[i].corpo = NULL; }
    }
  }
  pthread_mutex_unlock(&maniTrava);
  return corpo;
}

// Completed failure is a result, not a cache miss requiring another timeout.
// Disabled add-ons may finish probing in the background; Home never waits.
static char *maniObter(const char *url, int ativo) {
  int tentado;
  char *corpo = maniPegar(url, ativo, &tentado);
  if (!corpo && ativo && !tentado) corpo = rede_baixar(url, 20);
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
// `base` aqui e so IDENTIDADE (nunca vira pedido) e fica em 300: 512 entradas
// com NV_ADDON_URL_MAX custariam ~900 KB. Por isso a comparacao e por PREFIXO
// (nomeCatBase): com strcmp, a base cortada de um addon de URL longa nunca
// casava com a inteira e o nome do catalogo dele nunca era achado (#201).
static struct { char base[300], tipo[8], id[96], nome[96]; } nomeCat[NOMECAT_MAX];
#define nomeCatBase(i, b) (!strncmp(nomeCat[i].base, (b), sizeof nomeCat[i].base - 1))
static int nNomeCat, nomeCatProx;
static pthread_mutex_t nomeCatTrava = PTHREAD_MUTEX_INITIALIZER;
static void registrarNomeCatalogo(const char *base, const char *tipo, const char *id, const char *nome) {
  int i;
  if (!base || !nome || !nome[0]) return;
  pthread_mutex_lock(&nomeCatTrava);
  for (i = 0; i < nNomeCat; i++)
    if (nomeCatBase(i, base) && !strcmp(nomeCat[i].tipo, tipo) && !strcmp(nomeCat[i].id, id)) break;
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
    if (nomeCatBase(i, base) && !strcmp(nomeCat[i].tipo, tipo) && !strcmp(nomeCat[i].id, id)) {
      snprintf(saida, sizeof saida, "%s", nomeCat[i].nome); break; }
  // SEM A BASE EXATA (pasta Netflix, 01/10: a aba mostrava
  // "streaming_netflix_movies"): a base que a conta grava na fonte pode nao ser
  // a do addon instalado byte a byte — outra configuracao no caminho, barra no
  // fim. O log de 1.6.5 mostra exatamente isso: o catalogo
  // streaming_netflix_movies do addon instalado nao casa com a fonte da pasta
  // (col_diagnostico nivel 1/2, nunca 3). Cai para (tipo, id): vale so quando
  // TODOS os addons que declaram esse par dao o mesmo nome — dois nomes
  // diferentes e nao ha como escolher, fica vazio.
  if (!saida[0]) {
    int achou = 0;
    for (i = 0; i < nNomeCat; i++)
      if (!strcmp(nomeCat[i].tipo, tipo) && !strcmp(nomeCat[i].id, id) && nomeCat[i].nome[0]) {
        if (!achou) { snprintf(saida, sizeof saida, "%s", nomeCat[i].nome); achou = 1; }
        else if (strcmp(saida, nomeCat[i].nome)) { saida[0] = 0; break; }
      }
  }
  pthread_mutex_unlock(&nomeCatTrava);
  return saida;
}

// --- QUAIS CATALOGOS A COTA LE (issue #126) -----------------------------------
//
// A cota por addon (montar, "COTA POR ADDON") deixa cada addon declarar ate N
// catalogos, e ate a 1.4.5 os N eram os PRIMEIROS do manifesto. O Ultra MAX do
// relato declara 174 e a cota era 32: "[desc] Ultra MAX: 32 catalogo(s)
// declarado(s) (cota 32, manifesto tem 174 — 142 de fora por cota)". O
// catalogo que a pessoa queria estava entre os 142, e nada que ela fizesse na
// TV ou na conta o trazia — o limite de fileiras da home nem chegava a ve-lo,
// e a tela de Fileiras da Home nao o listava para ser escolhido.
//
// AGORA A COTA ESCOLHE, na ordem em que o proprio app monta a home (a regra
// pura esta em cotacat.c, com teste proprio; aqui so se diz o nivel):
//   0. escolhido NA TV (fileiras.c: ligado e na home, ou posto na fila);
//   1. na ordem de catalogos da CONTA (catordem.c), na posicao dela;
//   2. na ordem do arquivo local antigo (fileiras.txt, prefOrdem);
//   3. o resto, na ordem do manifesto — o comportamento de antes;
//   4. desligado (em qualquer das escolhas, ou engolido por colecao visivel):
//      nao vira fileira de jeito nenhum, entao so fica com vaga que sobrar.
// A memoria continua a mesma (o vetor de Decl e o mesmo, com o mesmo teto); o
// custo e uma segunda varredura do manifesto, sem rede.
static int prioCatalogo(const char *chave, const char *desativar,
                        const char *base, const char *tipo, const char *id,
                        int *pos) {
  int k, n;
  *pos = 0;
  if ((dentroDeColecaoVisivelBase(base, tipo, id) && !fil_adicionada_na_tv(chave)) || fil_oculta(chave) ||
      catordem_oculta(chave, desativar))
    return COTA_DESLIGADO;
  for (k = 0; k < nPrefOff; k++)
    if (!strcmp(prefOff[k], chave) || !strcmp(prefOff[k], desativar))
      return COTA_DESLIGADO;
  if ((k = fil_escolhida(chave)) >= 0) { *pos = k; return COTA_ESCOLHIDO_TV; }
  n = catordem_n();
  for (k = 0; k < n; k++)
    if (!strcmp(catordem_chave(k), chave)) { *pos = k; return COTA_ORDEM_CONTA; }
  for (k = 0; k < nPrefOrdem; k++)
    if (!strcmp(prefOrdem[k], chave)) { *pos = k; return COTA_ORDEM_LOCAL; }
  return COTA_MANIFESTO;
}

// Os catalogos que a cota deixou de fora NESTA volta, para montar() os
// registrar em fileiras.c DEPOIS dos candidatos (fil_registrar_se_couber). Se
// entrassem antes, numa lista nova eles tomariam as primeiras posicoes ligadas
// e a home da volta seguinte seria feita deles. So o fio da descoberta mexe.
typedef struct { char chave[192], titulo[96], addon[64], tipo[8]; } ForaCota;
static ForaCota *foraCota;
static int nForaCota, capForaCota;
static void foraCotaGuardar(const Decl *d) {
  if (nForaCota >= capForaCota) {
    int cap = capForaCota ? capForaCota * 2 : 64;
    ForaCota *novo;
    if (cap > FIL_MAX) cap = FIL_MAX;
    if (nForaCota >= cap) return;          // a tabela de fileiras nem caberia
    novo = (ForaCota *)realloc(foraCota, sizeof *foraCota * (size_t)cap);
    if (!novo) return;
    foraCota = novo; capForaCota = cap;
  }
  snprintf(foraCota[nForaCota].chave, sizeof foraCota[nForaCota].chave, "%s", d->chave);
  snprintf(foraCota[nForaCota].titulo, sizeof foraCota[nForaCota].titulo, "%s", d->titulo);
  snprintf(foraCota[nForaCota].addon, sizeof foraCota[nForaCota].addon, "%s", d->nomeAddon);
  snprintf(foraCota[nForaCota].tipo, sizeof foraCota[nForaCota].tipo, "%s", d->tipo);
  nForaCota++;
}
static void foraCotaSoltar(void) {
  free(foraCota); foraCota = NULL; nForaCota = capForaCota = 0;
}

// PRIMEIRA VARREDURA: so decide. Devolve um vetor de 0/1 por catalogo ELEGIVEL
// (tipo+id validos e que nao exige busca), na ordem do manifesto, dizendo quais
// a cota le — ou NULL quando cabem todos (o caso comum, sem custo extra alem da
// contagem) ou quando faltou memoria (vale a regra antiga).
static char *escolherPelaCota(const char *corpo, const char *fim,
                              const char *addonId, const char *base, int max,
                              int *nElegiveis, int *promovidos) {
  const char *p = js_array(corpo, fim, "catalogs");
  CotaPrio *pr = NULL;
  int n = 0, cap = 0;
  char *escolhido;
  if (promovidos) *promovidos = 0;
  while (p) {
    const char *f = js_fim(p);
    char tipo[8] = "", id[96] = "", nome[96] = "", chave[192], desativar[352];
    js_texto(p, f, "type", tipo, sizeof tipo);
    js_texto(p, f, "id", id, sizeof id);
    js_texto_raiz_em(p, f, "name", nome, sizeof nome);
    if (tipo[0] && id[0] && !exigeBusca(p, f)) {
      if (n >= cap) {
        CotaPrio *novo;
        cap = cap ? cap * 2 : 64;
        novo = (CotaPrio *)realloc(pr, sizeof *pr * (size_t)cap);
        if (!novo) { free(pr); return NULL; }
        pr = novo;
      }
      // As MESMAS duas chaves que lerManifesto monta para o Decl.
      snprintf(chave, sizeof chave, "%s_%s_%s", addonId[0] ? addonId : base, tipo, id);
      snprintf(desativar, sizeof desativar, "%s_%s_%s_%s", base, tipo, id, nome);
      pr[n].nivel = prioCatalogo(chave, desativar, base, tipo, id, &pr[n].pos);
      n++;
    }
    p = js_prox(f);
  }
  *nElegiveis = n;
  if (n <= max) { free(pr); return NULL; }
  escolhido = (char *)malloc((size_t)n);
  if (escolhido) {
    int k = cota_escolher(pr, n, max, escolhido);
    if (promovidos) *promovidos = k;
  }
  free(pr);
  return escolhido;
}

// A versao da lista de addons com que a volta leu os manifestos (ver a poda de
// fantasmas em montar()). So o fio da descoberta mexe.
static unsigned versaoManifestos;
// #392: de quem era a lista e qual perfil das fileiras valia quando a volta a leu.
static FilPassada passadaVolta;

// Catalogos que so respondem com busca, somados na volta: nao entram mais no
// vetor de Decl (nao gastam cota), e a linha do log que os contava continua.
static int nSoBuscaVolta;

// Manifestos de addon LIGADO que nao responderam nesta volta. Com algum, o
// snapshot nao e gravado: as chaves daquele addon nem existem nesta volta, e um
// snapshot sem elas as poria atras de todas as outras dali em diante.
static int nManiFalhouVolta;

// `ativo` = 0 para addon DESLIGADO na conta: o manifesto pronto e lido sem esperar
// (addons_manifesto_lido aprende o id — a poda de fileiras e as colecoes da
// conta precisam dele) e os nomes dos catalogos ficam registrados, mas nenhum
// catalogo vira candidato a fileira, fica "fora da cota" ou vira alvo de busca.
// Ate a guarda `addons_tem_catalogo(i)` sair (ver o laco da cota, em montar())
// era esse o efeito, por tabela; sem ela, Pluto TV, Minha TV, FrostView e Fenix
// TV — desligados na conta do dono — ganhavam fileira e ate vaga garantida na
// home da C9 (24/09), e a fonte deles continuava fora das consultas. Desligado
// e desligado nos dois lugares.
static int lerManifesto(int iAddon, const char *base, Decl *saida, int max,
                         int ativo, int *totalReal, int *promovidos) {
  char url[NV_ADDON_PEDIDO_MAX], addonId[96] = "", nome[96], tipo[8], id[96];
  char *corpo, *escolhido;
  const char *p, *fim;
  int n = 0, total = 0, e = 0, nEleg = 0;
  if (totalReal) *totalReal = 0;
  if (promovidos) *promovidos = 0;
  // Ja largado em paralelo no comeco de montar(); so cai na rede aqui quando
  // este addon nao estava na lista daquele instante.
  corpo = descPedidoCoube(base, nv_addon_url(url, sizeof url, base, "/manifest.json"), sizeof url)
          ? maniObter(url, ativo) : NULL;
  if (!corpo) {
    // Sem esta linha o log dizia "0 catalogo(s) declarado(s)", igual a um
    // addon que so tem stream.
    if (ativo) {
      nManiFalhouVolta++;
      printf("[desc]   %s: manifesto sem resposta nesta volta\n", addons_nome(iAddon));
    }
    return 0;
  }
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
  escolhido = ativo ? escolherPelaCota(corpo, fim, addonId, base, max, &nEleg, promovidos)
                   : NULL;
  if (!ativo && promovidos) *promovidos = 0;
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
      int exige = exigeBusca(p, f), guardar = 0;
      registrarNomeCatalogo(base, tipo, id, nome);
      if (!ativo) { p = js_prox(f); continue; }
      // QUEM A COTA LE sai de escolherPelaCota; sem escolha (cabem todos, ou a
      // memoria faltou) vale a regra antiga, os primeiros ate encher. Catalogo
      // que exige busca nao entra nunca: ele e retirado logo depois em montar(),
      // e ate a 1.4.5 ocupava vaga da cota ate la.
      if (exige) nSoBuscaVolta++;
      else {
        guardar = (escolhido && e < nEleg) ? escolhido[e] : (n < max);
        if (n >= max) guardar = 0;
        e++;
        total++;
      }
      // Fora do vetor: usa um Decl de rascunho so para decidir/registrar a busca.
      d = guardar ? &saida[n] : &local;
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
        //
        // "anime" TAMBEM (#176): o AIOMetadata e os addons localizados
        // declaram a busca de anime no tipo proprio, e sem isto o nome local de
        // um anime nunca era consultado. O item volta com tipo "anime" e abre
        // pelo mesmo caminho de tipo incerto que as fileiras de anime da home.
        if (strcmp(tipo, "movie") && strcmp(tipo, "series") && strcmp(tipo, "anime") &&
            !ehCanal(tipo))
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
      d->exigeParam = exige;
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
      if (guardar) n++;
      else if (!exige) foraCotaGuardar(d);
    }
    p = js_prox(f);
  }
  free(escolhido);
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
  const Decl *d;        // so para quem monta; o fio le as copias abaixo
  char base[NV_ADDON_URL_MAX], tipo[8], id[96];
  CatItem itens[MAX_POR_FILEIRA];
  int  n;
  int  respondeu;
  int  pronto;
  int  largada;         // quem monta desistiu de esperar (ver CAT_ESPERA_*)
  unsigned long long inicioMs;   // quando um fio pegou; 0 = na fila
} TarefaCat;

// UMA RODADA TEM DONO COMPARTILHADO: quem monta e cada fio. O ultimo a soltar
// libera. E o que deixa quem monta seguir sem pthread_join — com um catalogo
// largado por lento, ou uma volta condenada no meio — e o fio terminar o
// pedido dele sem escrever em memoria de ninguem.
typedef struct {
  TarefaCat *t;
  int nT, prox, refs, fechada;
  unsigned long long inicioMs, ultimoProntoMs;
} RodadaCat;

// O TETO DO CATALOGO LENTO. Os dois precisam valer juntos, com a fila da rodada
// ja vazia: CAT_ESPERA_SILENCIO_MS sem NENHUM catalogo terminar, e este no ar ha
// CAT_ESPERA_MIN_MS. O primeiro separa "a rede esta lenta para todos" (ai todos
// seguem chegando e ninguem e largado) de "um so esta pendurado"; o segundo
// protege o catalogo que so comecou tarde. O timeout de rede continua 8 s
// (lerCatalogo); isto e quanto a HOME espera, nao quanto o pedido vive.
#ifndef CAT_ESPERA_SILENCIO_MS
#define CAT_ESPERA_SILENCIO_MS 2500
#endif
#ifndef CAT_ESPERA_MIN_MS
#define CAT_ESPERA_MIN_MS      4000
#endif

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
  // Personal-server rows come only from the live Jellyfin snapshot: after a
  // sign-out, token expiry or profile switch they must vanish, not be
  // re-attached from the previous Home.
  return f && f->chave[0] && !servidores_chave_fileira(f->chave) && homeestado_contexto_valido() &&
         homeestado_tem_fileira(f->chave);
}

// Posicao no snapshot; chave fora dele vai para o FIM, estavel. Com a regra
// antiga (`anterior < 0 || (rank >= 0 && ...)`) uma chave sem posicao passava
// na frente de todas as com posicao: o catalogo que entrou no lugar de um vazio
// ia para o topo da home (#195, tests/snapshot_falha.sh).
static int rankSnapshot(const char *chave) {
  int r = homeestado_ordem_fileira(chave);
  return r < 0 ? CAT_FIL_MAX + DECL_MAX : r;
}

static void ordenarPorSnapshot(CatFileira *fil, int n) {
  int i;
  if (!fil || n < 2 || !homeestado_contexto_valido()) return;
  for (i = 1; i < n; i++) {
    CatFileira atual = fil[i];
    int rank = rankSnapshot(atual.chave), j = i;
    while (j > 0) {
      int anterior = rankSnapshot(fil[j - 1].chave);
      if (anterior <= rank) break;
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
  // static pelo mesmo motivo do daLinhaAnterior de montar(): 24 CatItem sao
  // ~375 KB, e o fio que monta tem 2 MB de pilha no Tizen. Um fio so chama.
  static CatItem tmp[MAX_POR_FILEIRA];
  // O TETO DE FILEIRAS VALE AQUI TAMBEM (#195). Linha que ficou de fora
  // porque o limite encheu nao e linha ausente: sem isto o substituto de uma
  // fileira que voltou a responder era reanexado e a home passava do limite.
  if (!lote || !*lote || !n || !cap || !fil || !nFil) return;
  int fixas = 0;
  for (r = 0; r < *nFil; r++) if (!fil[r].base[0]) fixas++;
  int teto = fil_limite() + fixas;
  if (teto > CAT_FIL_MAX) teto = CAT_FIL_MAX;
  for (r = 0; r < cat_n_fileiras() && *nFil < teto; r++) {
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

static pthread_mutex_t catTrava = PTHREAD_MUTEX_INITIALIZER;

static unsigned long long descAgoraMs(void);

// Sob catTrava.
static void rodadaSoltarLocked(RodadaCat *r) {
  if (--r->refs == 0) { free(r->t); free(r); }
}
// Quem monta acabou com a rodada (inteira, ou desistiu): nenhum fio pega
// pedido novo dela, e o ultimo a sair libera.
static void rodadaSoltar(RodadaCat *r) {
  pthread_mutex_lock(&catTrava);
  r->fechada = 1;
  rodadaSoltarLocked(r);
  pthread_mutex_unlock(&catTrava);
}

static void *fioCatalogo(void *u) {
  RodadaCat *r = u;
  for (;;) {
    int meu, got, respondeu = 0;
    TarefaCat *t;
    pthread_mutex_lock(&catTrava);
    if (r->fechada || r->prox >= r->nT) {
      rodadaSoltarLocked(r);
      pthread_mutex_unlock(&catTrava);
      return NULL;
    }
    meu = r->prox++;
    t = &r->t[meu];
    t->inicioMs = descAgoraMs();
    pthread_mutex_unlock(&catTrava);

    // QUANTOS e o ajuste (12/18/24, #163); o balde tem sempre o teto.
    { int q = ajustes_itens_fileira();
      if (q > MAX_POR_FILEIRA) q = MAX_POR_FILEIRA;
      got = lerCatalogo(t->base, t->tipo, t->id, t->itens,
                        MAX_POR_FILEIRA, q, &respondeu); }

    pthread_mutex_lock(&catTrava);
    t->n = got;
    t->respondeu = respondeu;
    t->pronto = 1;
    r->ultimoProntoMs = descAgoraMs();
    pthread_mutex_unlock(&catTrava);
  }
}

// "Continuar assistindo" a partir do progresso local (progresso.c), no mesmo
// formato que trakt_continuar devolve: imdb (composto em serie), tipo,
// porcentagem, temporada/episodio. Reaproveita o catalogo/cache por identidade
// antes do enfeite remoto: um id de provider nao e conhecido pelo Cinemeta e
// uma queda de rede nao pode apagar o registro local. Mais recente primeiro
// (prog_ler ja ordena). Entra o que esta entre
// 1% e o Percentual assistido (ajustes_cw_concluido, 90 de fabrica), os mesmos
// limites de home_registrar_retorno; titulo terminado nao e "continuar". O proximo episodio de uma serie terminada fica para depois.
// Os registros do disco para montarContinuar, UM buffer para continuarLocal e
// filtrarRemoto: sao ~77 KB, e na Samsung (teto do WebAssembly) nao se paga
// isso duas vezes em BSS. Os dois so rodam sob contTrava, um de cada vez.
static ProgRegistro contRegs[PROG_MAX];

static int continuarLocal(CatItem *saida, int max) {
  ProgRegistro *regs = contRegs;
  int k, i, n = 0;
  k = prog_ler(regs, PROG_MAX);
  for (i = 0; i < k && n < max; i++) {
    const ProgRegistro *r = &regs[i];
    CatItem *d;
    double p;
    int j, repetido = 0;
    if (r->durSeg < 60.0) continue;
    p = r->posSeg / r->durSeg;
    if (p < 0.01 || p * 100.0 >= ajustes_cw_concluido()) continue;
    // Uma serie com varios episodios gravados entra UMA vez, no mais recente.
    for (j = 0; j < n; j++) {
      size_t L = idbase_len(saida[j].imdb);   // "kitsu:41370" inteiro, nao "kitsu"
      if (L == strlen(r->contentId) && !strncmp(saida[j].imdb, r->contentId, L)) { repetido = 1; break; }
    }
    if (repetido) continue;
    d = &saida[n];
    memset(d, 0, sizeof *d);
    cat_copiar_por_id(r->contentId, r->episodio > 0 ? "series" : "movie", d);
    // O catalogo pode guardar outro episodio desta obra. Metadados do titulo
    // permanecem; o episodio e o tempo pertencem ao progresso de agora.
    if (d->temporada != r->temporada || d->episodio != r->episodio)
      d->nomeEpisodio[0] = 0;
    d->temporada = r->temporada;
    d->episodio = r->episodio;
    d->progresso = (int)(100.0 * p);
    d->restanteMin = (int)((r->durSeg - r->posSeg) / 60.0 + 0.5);
    // O instante ja esta aqui, no registro: carimbar agora poupa a busca por
    // chave que instanteDaConta faria depois. Ver retomadoMs em catalogo.h.
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
  // O enfeite remoto compacta quem ficou sem poster. Para o progresso local
  // isso era perda de registro (#158), sobretudo kitsu/tmdb e providers
  // offline. Enriquece uma copia e reaplica por identidade; uma falha deixa
  // os metadados conhecidos (ou a reserva da UI), sem inventar arte.
  if (n > 0) {
    CatItem *enfeitadas = malloc(sizeof *enfeitadas * (size_t)n);
    if (enfeitadas) {
      int ni, j;
      memcpy(enfeitadas, saida, sizeof *enfeitadas * (size_t)n);
      // A reserva exibida na rodada offline nao e um titulo resolvido.
      // Enfeitar completa somente campos vazios; deixe-o substituir essa
      // reserva quando a rede voltar, sem apagar a copia que a UI ja tem.
      for (i = 0; i < n; i++)
        if (!strcmp(enfeitadas[i].titulo,
                    i18n(!strcmp(enfeitadas[i].tipo, "series") ? "Programa de TV" : "Filme")))
          enfeitadas[i].titulo[0] = 0;
      ni = trakt_enfeitar_lote(enfeitadas, n);
      for (i = 0; i < n; i++)
        for (j = 0; j < ni; j++)
          if (!strcmp(saida[i].imdb, enfeitadas[j].imdb) &&
              !strcmp(saida[i].tipo, enfeitadas[j].tipo)) {
            saida[i] = enfeitadas[j]; break;
          }
      free(enfeitadas);
    }
    for (i = 0; i < n; i++)
      if (!saida[i].titulo[0])
        snprintf(saida[i].titulo, sizeof saida[i].titulo, "%s",
                 i18n(!strcmp(saida[i].tipo, "series") ? "Programa de TV" : "Filme"));
  }
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
// home_registrar_retorno. Abaixo de 1% nao se comecou, do Percentual assistido
// (90% de fabrica) em diante acabou — e "continuar" nao e nem uma coisa nem a
// outra.
static int emAndamento(int pct) { return pct >= 1 && pct < ajustes_cw_concluido(); }

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

// QUAL "CONTINUAR" E O MAIS NOVO (issue #205). montar() calcula a fileira no
// COMECO da volta e so a publica no fim, segundos depois (manifestos e
// catalogos no meio). Nesse meio o sync chega, os vistos da conta entram e
// desc_refazer_continuar publica a lista certa — e a publicacao do fim de
// montar() a cobria com a velha. MEDIDO no log da Q80A (1.6.5): 1 item as
// 1,6 s (antes dos vistos), refeita com 5 as 2,9 s, "catalogo montado com 73
// titulos" as 4,7 s = 72 do catalogo + 1 — os 4 "a seguir" da conta sumiam ate
// a proxima refacao (sair do player, ou mudar a Fonte do Continuar, que e o
// "volta quando eu mudo o ajuste" do relato).
//
// Cada montarContinuar leva um numero; o fio da refacao diz qual publicou. Os
// dois sob contTrava, e quem publica tambem: assim nenhuma publicacao cai
// entre a copia e a reaplicacao em cwAntesDePublicar/cwDepoisDePublicar.
static unsigned cwGer;          // ++ a cada montarContinuar
static unsigned cwGerNaTela;    // a do ultimo fioContinuar que publicou
static unsigned cwGerMontar;    // a que esta no lote de montar() (um montar por vez)

// O "A SEGUIR" NAO PASSA DO QUE A PESSOA DESMARCOU (Silo, tt14688458, 08/10).
// O remoto diz "ultimo visto T2E10" e o "a seguir" dele e T3E1 (ou T2E11); mas
// T2E7..E10 foram desmarcados NESTA TV, e o proximo de verdade e o T2E7. A regra
// mora aqui, UMA vez, para as tres fontes: o primeiro episodio desmarcado ate a
// posicao do remoto (vistonao_primeira, que descarta a desmarcacao que o remoto
// ja superou — folga de 2 min). `remotoMs` e o instante do ultimo visto do
// remoto. Devolve 1 e troca (*t, *e) quando recuou. Um log por item por montagem.
static int (*cwPrimeira)(const char *, int, int, long long, int *, int *);
void desc_lapides_primeira(int (*primeira)(const char *, int, int, long long, int *, int *)) {
  cwPrimeira = primeira;
}
static int recuarPelaDesmarcacao(const char *serie, int *t, int *e, long long remotoMs) {
  int nt, ne;
  if (!cwPrimeira || *t < 1 || *e < 1) return 0;
  if (!cwPrimeira(serie, *t, *e, remotoMs, &nt, &ne) || (nt == *t && ne == *e)) return 0;
  printf("[desc] continuar assistindo: %s a seguir ajustado pela desmarcacao: T%dE%d -> T%dE%d\n",
         serie, *t, *e, nt, ne);
  *t = nt; *e = ne;
  return 1;
}
// Os ids que ele moveu nesta montagem; montarContinuar os publica (cwordem.c)
// antes de perguntar quem e "a seguir".
static char ajustadosIds[CONT_MAX * 3][sizeof(((CatItem *)0)->imdb)];
static int nAjustados;
static void ajustarASeguir(CatItem *it) {
  char serie[sizeof it->imdb], velho[sizeof it->imdb];
  int t = it->temporada, e = it->episodio;
  snprintf(serie, sizeof serie, "%s", it->imdb);
  { char *dp = strchr(serie, ':'); if (dp) *dp = 0; }
  if (!recuarPelaDesmarcacao(serie, &t, &e, it->retomadoMs)) return;
  snprintf(velho, sizeof velho, "%s", it->imdb);
  it->temporada = t; it->episodio = e;
  snprintf(it->imdb, sizeof it->imdb, "%s:%d:%d", serie, t, e);
  // O que o remoto sabia do episodio de ANTES nao descreve este.
  it->nomeEpisodio[0] = 0;
  it->restanteMin = 0;
  // Episodio antigo: ja foi ao ar (sem isto contaria "sem data de estreia").
  cwo_marcar_estreia(it->imdb, 0);
  if (nAjustados < (int)(sizeof ajustadosIds / sizeof *ajustadosIds))
    snprintf(ajustadosIds[nAjustados++], sizeof ajustadosIds[0], "%s", it->imdb);
}

// Aplica a um lote REMOTO (Trakt ou Simkl) os limites de 1% a 90% e o
// cruzamento com o registro local mais novo. Compacta no lugar; devolve quantos
// ficaram. `aSeguir` diz quais itens sao "a seguir" (entram com 0%).
static int filtrarRemoto(CatItem *v, int n, int (*aSeguir)(const char *),
                         int *fora) {
  int i, w, k = prog_ler(contRegs, PROG_MAX);
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
    // O EPISODIO VELHO DO REMOTO NAO VOLTA POR CIMA DO QUE ESTA TV VIU DEPOIS.
    // O cruzamento acima e por CHAVE (o mesmo episodio). Com outro episodio da
    // mesma serie mais novo no disco — o S1E5 terminado aqui ontem, quando o
    // /sync/playback ainda guarda o S1E3 pausado ha um mes — a conta nao
    // disputava nada (continuarLocal tira o terminado) e o S1E3 voltava a
    // fileira. Agora o item passa a falar do registro mais novo da obra: em
    // andamento, e ele que aparece (e a conta, se tambem o trouxe, fica com o
    // lugar pelo desempate de sempre); terminado, e a semente do card, que
    // passa ao proximo episodio (continuar_desenhar). Remoto que esta ADIANTE
    // na serie nao e episodio antigo e fica como veio. O instante decide, como
    // no resto desta funcao: registro mais VELHO que o paused_at nao manda.
    if (v[i].temporada > 0 && v[i].episodio > 0) {
      size_t L = idbase_len(v[i].imdb);
      const ProgRegistro *r = NULL;
      int j;
      for (j = 0; j < k && !r; j++)
        if (contRegs[j].episodio > 0 && strlen(contRegs[j].contentId) == L &&
            !strncmp(contRegs[j].contentId, v[i].imdb, L)) r = &contRegs[j];
      if (r && r->durSeg > 1.0 && r->lastWatchedMs > v[i].retomadoMs &&
          !(r->temporada == v[i].temporada && r->episodio == v[i].episodio) &&
          !(v[i].temporada > r->temporada ||
            (v[i].temporada == r->temporada && v[i].episodio > r->episodio))) {
        int pct = (int)(100.0 * r->posSeg / r->durSeg);
        if (pct >= 1) {
          if (pct > 100) pct = 100;
          printf("[desc] continuar assistindo: %s T%dE%d do remoto e mais velho que "
                 "T%dE%d visto aqui (%d%%); fica o daqui\n", r->contentId,
                 v[i].temporada, v[i].episodio, r->temporada, r->episodio, pct);
          snprintf(v[i].imdb, sizeof v[i].imdb, "%s:%d:%d", r->contentId,
                   r->temporada ? r->temporada : 1, r->episodio);
          v[i].temporada = r->temporada;
          v[i].episodio = r->episodio;
          v[i].progresso = pct;
          v[i].retomadoMs = r->lastWatchedMs;
          v[i].restanteMin = (int)((r->durSeg - r->posSeg) / 60.0 + 0.5);
          if (v[i].restanteMin < 0) v[i].restanteMin = 0;
          v[i].nomeEpisodio[0] = 0;
        }
      }
    }
    // "A seguir" que passa da desmarcacao da pessoa recua (ajustarASeguir).
    if (v[i].progresso == 0 && v[i].temporada > 0 && v[i].episodio > 0 &&
        aSeguir(v[i].imdb)) ajustarASeguir(&v[i]);
    if (w != i) v[w] = v[i];
    w++;
  }
  return w;
}

// Texto localizado do Continuar assistindo (#176): o Trakt/Simkl/conta so tem o
// titulo e a sinopse em ingles. Definidas junto do cache de /meta, mais abaixo.
static int aplicarLocCache(CatItem *v, int n);
static void localizarContinuarPublicado(void);

// "A SEGUIR" DA CONTA NUVIO (issue #199). LG C9, 1.6.4, sem Trakt e sem
// Simkl: a conta tinha 804 episodios vistos e a fileira so mostrava os 5
// pausados ("0 do Trakt, 0 do Simkl, 5 da conta"), enquanto o app da Shield,
// na mesma conta, mostrava Ted Lasso "Airs in 6 days", American Horror Story
// "Airs tomorrow" e outros "Next up". O web semeia o "a seguir" dos vistos da
// conta quando a fonte e o Nuvio Sync (getContinueWatchingNextUpSeedOptions,
// homeScreen.js:2202); aqui so o Trakt (historico) e o Simkl (next_to_watch)
// semeavam, e a fonte "conta" nunca teve "a seguir".
//
// As sementes saem de contalib (uma por serie, o episodio depois do ultimo
// visto). Serie ja pausada na conta fica fora — o card dela e o do episodio em
// andamento, como o inProgressSeriesIds do web. O enfeite (trakt.c) confere no
// Cinemeta que o episodio existe, vira a temporada quando preciso, descarta a
// serie que acabou e anota a estreia; o filtro de "nao exibidos" e a Ordenacao
// mais abaixo tratam o futuro igual ao do Trakt. Junta com os pausados pelo
// instante (o do episodio-ancora) e devolve quantos ficaram em `lista`.
//
// So sem Trakt e sem Simkl no ar: e quando o web le os vistos da conta
// (shouldUseSupabaseWatchProgressSync) e quando sync.c os aplica. Com um deles
// vinculado, o "a seguir" ja vem dele.
#define CONTA_JANELA_MS (60LL * 24 * 3600 * 1000)
static int contaASeguir(CatItem *lista, int n, int max, int comOutraFonte) {
  static ContaSemente sem[PROX_MAX_BUSCAS];
  static const char *ids[PROX_MAX_BUSCAS];
  static char idsTxt[PROX_MAX_BUSCAS][sizeof(((CatItem *)0)->imdb)];
  CatItem *lote;
  int nSem, nLote = 0, i, j, confirmados, entraram = 0;
  int fVelho = 0, fVisto = 0, fPausado = 0;
  nSem = contalib_sementes_a_seguir(sem, PROX_MAX_BUSCAS,
                                    ajustes_cw_do_episodio_mais_alto());
  if (nSem < 0) {
    printf("[desc] continuar assistindo: a seguir da conta: vistos da conta nao puxados "
           "ou velhos; nenhuma semente\n");
    cwo_conta_definir(NULL, 0);
    return n;
  }
  if (nSem < 1) { cwo_conta_definir(NULL, 0); return n; }
  lote = (CatItem *)malloc(sizeof(CatItem) * PROX_MAX_BUSCAS);
  if (!lote) { cwo_conta_definir(NULL, 0); return n; }
  for (i = 0; i < nSem; i++) {
    CatItem *d = &lote[nLote];
    int ja = 0;
    memset(d, 0, sizeof *d);
    // A semente da conta tambem recua para o primeiro desmarcado.
    recuarPelaDesmarcacao(sem[i].id, &sem[i].temporada, &sem[i].episodio, sem[i].vistoMs);
    snprintf(d->imdb, sizeof d->imdb, "%s:%d:%d", sem[i].id, sem[i].temporada,
             sem[i].episodio);
    for (j = 0; j < n && !ja; j++) ja = mesmaObra(&lista[j], d);
    if (ja) { fPausado++; continue; }
    // Visto em QUALQUER fonte (conta, Trakt, Simkl, esta TV): nao e "a seguir".
    if (vistoep_estado(sem[i].id, sem[i].temporada, sem[i].episodio) == 1) {
      fVisto++;
      continue;
    }
    // Com Trakt/Simkl vinculado, serie sem toque ha mais de 60 dias nao volta
    // so porque a conta a conhece (mesmo corte que o web aplica ao a seguir do
    // Trakt). Sozinha, a conta mantem o comportamento da #199: sem corte.
    if (comOutraFonte && sem[i].vistoMs > 0 &&
        (long long)time(NULL) * 1000LL - sem[i].vistoMs > CONTA_JANELA_MS) {
      fVelho++;
      continue;
    }
    snprintf(d->tipo, sizeof d->tipo, "series");
    d->temporada = sem[i].temporada;
    d->episodio = sem[i].episodio;
    d->progresso = 0;
    d->retomadoMs = sem[i].vistoMs;
    snprintf(idsTxt[nLote], sizeof idsTxt[0], "%s", d->imdb);
    ids[nLote] = idsTxt[nLote];
    nLote++;
  }
  // Publicado ANTES do enfeite: e por ele que trakt.c sabe que tem de conferir.
  cwo_conta_definir(ids, nLote);
  confirmados = nLote ? trakt_enfeitar_lote(lote, nLote) : 0;
  // Junta pelo instante, mais recente primeiro; o lote ja vem assim. Com
  // "Mostrar nao exibidos" desligado o futuro nem disputa lugar: o filtro mais
  // abaixo o tiraria, e o pausado que ele empurrou para fora nao voltaria.
  for (i = 0; i < confirmados; i++) {
    int alvo;
    CwoItem x;
    x.aSeguir = 1;
    x.estreiaMs = cwo_estreia(lote[i].imdb);
    if (!ajustes_cw_mostrar_nao_exibidos() &&
        cwo_futuro(&x, (long long)time(NULL) * 1000LL)) continue;
    if (n < max) alvo = n++;
    else {
      int vel = 0;
      for (j = 1; j < n; j++) if (lista[j].retomadoMs < lista[vel].retomadoMs) vel = j;
      if (lista[vel].retomadoMs >= lote[i].retomadoMs) continue;
      alvo = vel;
    }
    lista[alvo] = lote[i];
    entraram++;
  }
  { int a, b;
    for (a = 1; a < n; a++) {
      CatItem t = lista[a];
      for (b = a - 1; b >= 0 && lista[b].retomadoMs < t.retomadoMs; b--) lista[b + 1] = lista[b];
      lista[b + 1] = t;
    } }
  printf("[desc] continuar assistindo: a seguir da conta: %d da conta, %d ja pausada(s), "
         "%d vista(s) em outra fonte, %d velha(s) (>60 dias), %d consultada(s), "
         "%d confirmada(s), %d na lista\n", nSem, fPausado, fVisto, fVelho, nLote,
         confirmados, entraram);
  free(lote);
  return n;
}

static void *fioReexibirTrakt(void *u) {
  trakt_progresso_ocultar((const char *)u, 0);
  free(u);
  return NULL;
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
  cwGer++;
  nAjustados = 0;
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
  // A mesma obra DENTRO do Trakt tambem entra uma vez (#244: o mesmo episodio
  // 5x). trakt_continuar ja guarda a gravacao mais nova por obra; esta e a rede
  // de seguranca para a chave de episodio x chave de serie e para quem chegar
  // por outro caminho.
  for (i = 0; i < nT; i++) {
    int k, achou = -1;
    for (k = 0; k < nR && achou < 0; k++)
      if (mesmaObra(remotos[k], &doTrakt[i])) achou = k;
    if (achou < 0) { remotos[nR++] = &doTrakt[i]; continue; }
    repetidos++;
    if (instanteDaConta(&doTrakt[i]) > instanteDaConta(remotos[achou])) remotos[achou] = &doTrakt[i];
  }
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
  // Fonte "Conta" escolhida: a conta da o "a seguir" mesmo com Trakt/Simkl
  // vinculado (decisao do dono). Em "Ambas" ele continua sendo do vinculo.
  if (querConta && (fonte == AJ_CWF_CONTA || (!trakt_ativo() && !simkl_ativo())))
    nL = contaASeguir(daConta, nL, CONT_MAX, trakt_ativo() || simkl_ativo());
  else
    cwo_conta_definir(NULL, 0);

  // Os "a seguir" que a desmarcacao moveu (Trakt/Simkl acima; a conta, que ja
  // foi montada em contaASeguir, entra pelo seu proprio registro e pelo id novo).
  { const char *aj[CONT_MAX * 3];
    int k;
    for (k = 0; k < nAjustados; k++) aj[k] = ajustadosIds[k];
    cwo_ajustados_definir(aj, nAjustados); }

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

  // OCULTO PELA PESSOA, EM DISCO (#203): o "Tirar de Continuar assistindo" num
  // item "A seguir". Vale para as tres fontes e sobrevive ao reinicio; a obra
  // VOLTA SOZINHA quando ha episodio novo (instante mais novo que o carimbo, ou
  // registro local mais novo) — para a pessoa nunca ficar presa fora. Ao voltar,
  // desfaz tambem o oculto do Trakt, senao o proximo episodio nao viria de la.
  { int w = 0, ocultos = 0;
    for (i = 0; i < nJ; i++) {
      if (prog_oculto_vence(juntos[i].item->imdb, juntos[i].ms)) { ocultos++; continue; }
      if (prog_oculto_soltar(juntos[i].item->imdb, juntos[i].ms)) {
        char *id = strdup(juntos[i].item->imdb);
        pthread_t t;
        printf("[desc] continuar assistindo: %s voltou (episodio novo)\n", juntos[i].item->imdb);
        if (id && pthread_create(&t, NULL, fioReexibirTrakt, id) == 0) pthread_detach(t);
        else free(id);
      }
      juntos[w++] = juntos[i];
    }
    nJ = w;
    if (ocultos)
      printf("[desc] continuar assistindo: %d oculto(s) pela pessoa\n", ocultos); }

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

  // A ORDENACAO ESCOLHIDA EM AJUSTES (issue #127) — ver cwordem.h. Ate aqui a
  // lista esta pelo instante, que e o modo "Padrao". "Estilo streaming" e
  // "Separar futuros" levam os "a seguir" que ainda nao foram ao ar para o fim,
  // pela estreia; no segundo a home ainda os tira desta fileira e monta
  // "Proximos episodios" com eles. `showUnairedNextUp` desligado os tira de vez
  // (shouldShowNextUpEpisodeForContinueWatching do web): antes esta preferencia
  // tambem nao era lida por ninguem.
  { static CwoItem cwo[CONT_MAX * 3];
    static Cand ordenados[CONT_MAX * 3];
    static int perm[CONT_MAX * 3];
    static const char *futIds[CONT_MAX * 3];
    long long agora = (long long)time(NULL) * 1000LL;
    int modo = ajustes_cw_ordem(), naoExibidos = ajustes_cw_mostrar_nao_exibidos();
    int proxFora = 0, escondidos = 0, semData = 0, principal, nFut = 0, mp, mf, w = 0;
    for (i = 0; i < nJ; i++) {
      const CatItem *c = juntos[i].item;
      CwoItem x;
      x.aSeguir = c->progresso == 0 &&
                  (trakt_e_a_seguir(c->imdb) || simkl_e_a_seguir(c->imdb) ||
                   cwo_conta_a_seguir(c->imdb));
      // "PROXIMO EPISODIO NO CONTINUAR" desligado (#203): sai todo item que e o
      // PROXIMO — o "a seguir" e o episodio terminado que o card trocaria pelo
      // seguinte (continuar_desenhar). Fica so o que esta em andamento.
      if (!ajustes_cw_proximo() &&
          (x.aSeguir || (!strcmp(c->tipo, "series") && c->temporada > 0 && c->episodio > 0 &&
                         c->progresso >= ajustes_cw_concluido()))) {
        proxFora++;
        continue;
      }
      x.estreiaMs = x.aSeguir ? cwo_estreia(c->imdb) : CWO_SEM_DATA;
      // POR ITEM, para o log de campo dizer POR QUE um "a seguir" nao virou
      // futuro: sem data ele conta como exibido (como o `hasAired !== false`
      // do web) e a Ordenacao nao o move.
      if (x.aSeguir && x.estreiaMs == CWO_SEM_DATA) {
        semData++;
        printf("[desc] continuar assistindo: a seguir %s sem data de estreia (conta como exibido)\n",
               c->imdb);
      }
      if (!naoExibidos && cwo_futuro(&x, agora)) {
        escondidos++;
        printf("[desc] continuar assistindo: a seguir %s ainda nao foi ao ar; escondido "
               "(nao exibidos desligado)\n", c->imdb);
        continue;
      }
      juntos[w] = juntos[i];
      cwo[w++] = x;
    }
    nJ = w;
    if (proxFora)
      printf("[desc] continuar assistindo: %d proximo(s) fora (Proximo episodio no Continuar desligado)\n", proxFora);
    principal = cwo_ordenar(cwo, nJ, modo, agora, perm);
    for (i = 0; i < nJ; i++) ordenados[i] = juntos[perm[i]];
    // O CORTE DE `max` COM RESERVA PARA OS FUTUROS (cwo_corte, cwordem.h).
    // Antes era um slice de [exibidos..., futuros...]: com a fileira cheia —
    // 22 candidatos para 12 lugares na C9 do dono — os futuros eram sempre os
    // cortados, e "Separar futuros"/"Estilo streaming" nao mudavam nada.
    cwo_corte(principal, nJ - principal, max, &mp, &mf);
    for (i = 0; i < mp; i++) juntos[i] = ordenados[i];
    for (i = 0; i < mf; i++) {
      juntos[mp + i] = ordenados[principal + i];
      futIds[nFut++] = juntos[mp + i].item->imdb;
    }
    if (modo != CWO_PADRAO)
      for (i = 0; i < mf; i++)
        printf("[desc] continuar assistindo: futuro %s estreia %lld%s\n",
               futIds[i], cwo_estreia(futIds[i]),
               modo == CWO_SEPARAR ? " (Proximos episodios)" : " (no fim)");
    { int cortadosFut = nJ - principal - mf, cortados = principal - mp;
      nJ = mp + mf;
      // Publicado ANTES de cat_trocar_continuar (quem chama publica a fileira
      // depois deste retorno): a home nunca ve a lista nova com o conjunto velho.
      cwo_publicar_futuros(futIds, nFut);
      if (modo != CWO_PADRAO || escondidos || semData)
        printf("[desc] continuar assistindo: ordem %s, %d futuro(s)%s, %d escondido(s), "
               "%d sem data, nao exibidos %s, corte: %d exibido(s) e %d futuro(s) fora\n",
               modo == CWO_SEPARAR ? "separar futuros" : modo == CWO_STREAMING
                                   ? "estilo streaming" : "padrao",
               nFut, modo == CWO_SEPARAR ? " na fileira propria" : " no fim", escondidos,
               semData, naoExibidos ? "ligado" : "desligado", cortados, cortadosFut); } }

  if (nJ > max) nJ = max;
  // "SUMIU DO CONTINUAR ASSISTINDO" (#151, log id 4439): o Trakt respondeu
  // HTTP 500, a parte da conta veio 0 no mesmo ciclo (6 s antes eram 12) e a
  // fileira foi publicada VAZIA. Lista vazia com a fonte remota muda e
  // "nao sei", nao "nada em andamento": fica o que ja estava na tela. Tirar um
  // card a mao nao passa por aqui (desc_tirar_continuar tira do publicado), e
  // o ciclo seguinte com o Trakt de volta refaz a fileira normalmente.
  if (nJ == 0 && querTrakt && trakt_continuar_falhou()) {
    int k = cat_copiar_fileira("continue_watching", saida, max, NULL);
    // A metade que falta provar: por que a conta veio 0. Registros no
    // progresso deste perfil, antes dos filtros de 1-90%.
    if (querConta) {
      ProgRegistro *rd = malloc(sizeof *rd * PROG_MAX);
      if (rd) {
        printf("[desc] continuar assistindo: conta vazia (%d registro(s) no progresso, perfil %d)\n",
               prog_ler(rd, PROG_MAX), perfis_ativo());
        free(rd);
      }
    }
    if (k > 0) {
      printf("[desc] continuar assistindo: Trakt sem resposta e lista vazia; "
             "mantidos os %d card(s) da tela\n", k);
      fflush(stdout);
      free(doSimkl);
      return k;
    }
  }
  for (i = 0; i < nJ; i++) {
    saida[i] = *juntos[i].item;
    if (getenv("NUVIO_CW_LOG"))
      printf("[desc] cw[%d] %s T%dE%d %d%% ms=%lld %s\n", i, saida[i].imdb, saida[i].temporada,
             saida[i].episodio, saida[i].progresso, juntos[i].ms,
             juntos[i].item >= daConta && juntos[i].item < daConta + CONT_MAX ? "conta"
             : doSimkl && juntos[i].item >= doSimkl && juntos[i].item < doSimkl + CONT_MAX ? "simkl"
             : "trakt");
  }
  // O que ja foi localizado antes entra AQUI, sem rede: a fileira nao pisca em
  // ingles a cada refazagem. O que falta e buscado depois de publicada
  // (localizarContinuarPublicado), para o texto novo nunca atrasar a fileira.
  aplicarLocCache(saida, nJ);
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
  // Publica AINDA sob a trava: ver cwGer.
  cat_trocar_continuar(lote, n);
  cwGerNaTela = cwGer;
  pthread_mutex_unlock(&contTrava);
  localizarContinuarPublicado();
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

// A SAIDA DO PLAYER, LOCAL E NO MESMO QUADRO (cwfrente.h). Substitui o
// desc_repetir() que app.c pedia quando o titulo nao estava na fileira: o ciclo
// inteiro (~20 s na TV) publicava a home de novo, o card entrava muito depois
// do pouso na ilha e a carga dele tomava a pilula. A refacao so do Continuar
// ja sai de fecharSessao e reconcilia com a conta/Trakt/Simkl.
//
// Chamada no fio de desenho, depois de o progresso ir para o item
// (cat_salvar_progresso_ep): o card nasce com a barra e o episodio de agora.
// contTrava so por TRYLOCK: um fioContinuar com ela esta na rede, e publica a
// verdade por cima desta logo depois. Com ela, a geracao anda como numa
// refacao — um montar() em curso reaplica esta janela em vez de devolver a
// fileira velha (cwAntesDePublicar).
int desc_continuar_otimista(int indice) {
  static CatItem atual[CONT_MAX], saida[CONT_MAX];
  CatItem novo;
  const CatItem *ci = cat_item(indice);
  int n, k, mudou, travou;
  if (!ci || !ci->imdb[0]) return 0;
  novo = *ci;
  if (novo.progresso < 1 || novo.progresso >= ajustes_cw_concluido()) return 0;
  if (ajustes_cw_fonte() == AJ_CWF_SIMKL && !simkl_ativo()) return 0;
  if (!strcmp(novo.tipo, "series") && novo.temporada > 0 && novo.episodio > 0) {
    char base[64];
    idbase_copiar(ci->imdb, base, sizeof base);
    snprintf(novo.imdb, sizeof novo.imdb, "%s:%d:%d", base, novo.temporada, novo.episodio);
  }
  travou = pthread_mutex_trylock(&contTrava) == 0;
  n = cat_copiar_fileira("continue_watching", atual, CONT_MAX, NULL);
  k = cw_frente_compor(atual, n, &novo, saida, CONT_MAX, &mudou);
  if (mudou) {
    cat_trocar_continuar(saida, k);
    if (travou) cwGerNaTela = ++cwGer;
  }
  if (travou) pthread_mutex_unlock(&contTrava);
  printf("[desc] continuar assistindo: %s %s ao sair do player (local; a refacao confirma)\n",
         novo.imdb, mudou ? "na frente" : "ja estava na frente");
  fflush(stdout);
  return mudou;
}

// AS DUAS METADES DE UMA PUBLICACAO DE montar() (ver cwGer). A primeira toma
// contTrava e, se uma refacao publicou depois de montar() calcular a fileira,
// copia a janela da tela; a segunda, depois de publicar, devolve essa janela
// por cima (cat_trocar_continuar: as outras fileiras so andam o `ini`) e solta
// a trava. Sem refacao no meio: nada copiado, nada reaplicado.
static CatItem *cwAntesDePublicar(int *k) {
  CatItem *c = NULL;
  *k = 0;
  pthread_mutex_lock(&contTrava);
  if (cwGerNaTela > cwGerMontar && (c = malloc(sizeof(CatItem) * CONT_MAX)))
    *k = cat_copiar_fileira("continue_watching", c, CONT_MAX, NULL);
  return c;
}
static void cwDepoisDePublicar(CatItem *c, int k) {
  if (c) {
    printf("[desc] continuar assistindo: a refacao feita durante a montagem fica "
           "(%d item(ns))\n", k);
    // cwGerMontar NAO anda: o lote de montar() segue com a janela velha, e
    // cada publicacao seguinte dele precisa da mesma reaplicacao.
    cat_trocar_continuar(c, k);
    free(c);
  }
  pthread_mutex_unlock(&contTrava);
}
// Toda publicacao de montar() passa por aqui (em partes e a do fim).
static void publicarMontagem(const CatItem *lote, int n, const CatFileira *fils, int nf) {
  int k;
  double t0 = cat_relogio_ms();
  CatItem *cw = cwAntesDePublicar(&k);
  cat_definir_tudo(lote, n, fils, nf);
  cwDepoisDePublicar(cw, k);
  printf("[perf] publicarMontagem: %d itens, %d fileiras, %.1f ms\n", n, nf, cat_relogio_ms() - t0);
  fflush(stdout);
}

// A METADE LOCAL DE "TIRAR DE CONTINUAR ASSISTINDO", toda no fio de quem
// chama e sem rede: apaga a linha de progresso.c, carimba a remocao (a
// refacao seguinte nao traz o item de volta com o remoto mais velho) e tira o
// card da fileira publicada por identidade, bumpando a revisao — a home ve no
// mesmo quadro. A ORDEM importa: o carimbo vem ANTES de tirar, para uma
// cat_trocar_continuar concorrente ou ja enxergar o carimbo (e podar) ou
// publicar antes da remocao (e ser corrigida por ela). Os DELETE remotos sao
// de quem chama. Devolve quantos cards sairam.
// Leitura sem trava, como desc_repetir ja faz com `buscando`: um quadro de
// atraso na resposta nao muda nada para quem pergunta (app.c, uma vez por
// quadro, com teto de tempo).
int desc_montando(void) {
  return buscando || repetirAoFim || descDeb.adiado || cwVivo || cwDeNovo;
}
// A CARGA DA HOME (o aviso "Carregando fileiras…" da ilha) e o ciclo de
// descoberta, nao a refacao so do Continuar assistindo: essa nao consulta add-on
// nenhum, roda a cada saida do player (fecharSessao) e, contada como carga,
// trocava o cartao da pilula pelo aviso no meio do voo do player ate a ilha
// (tests/ilha_voo_cw.sh). desc_montando continua com as duas: a troca de perfil
// espera a fileira certa.
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

// A ORDEM DOS CANDIDATOS A FILEIRA, com as preferencias de AGORA.
//
// Saiu de dentro de montar() para servir a duas perguntas com a mesma regra:
// a montagem (registrar=1: renomeia, registra em fileiras.c e diz no log quem
// ganhou vaga garantida) e a conferencia do fim dela, quando a estrutura mudou
// no meio (registrar=0, sem efeito colateral em fileiras.c): "com a estrutura
// nova, uma montagem pediria algum catalogo que esta nao pediu?". Duas copias
// da regra divergiriam na primeira mudanca, e a conferencia passaria a
// responder sobre uma ordem que ninguem monta.
//
// `nFixas` e quantas fileiras sinteticas (Continuar/Amigos) ja ocupam o teto.
// Devolve quantos indices de `decls` foram escritos em `ordem`.
static int ordenarCandidatos(Decl *decls, int nDecl, int *ordem, int nFixas,
                             int registrar, int *tetoSaida) {
  int nOrdem = 0, j, k;
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

  if (registrar) {
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
    // #392: so com a lista de addons DESTE perfil. Logo depois de uma troca a
    // lista ainda e a do perfil que saiu (a conta responde alguns segundos
    // depois); registra-la despejava as fileiras do perfil novo e acrescentava
    // as do outro ("a ordem da Home muda quando volto ao principal").
    if (fil_passada_valida(&passadaVolta)) {
      for (k = 0; k < nOrdem && k < FIL_MAX; k++)
        fil_registrar_de(&passadaVolta, decls[ordem[k]].chave, decls[ordem[k]].titulo,
                         decls[ordem[k]].nomeAddon, decls[ordem[k]].tipo, -1);
      fil_gravar_registro();
    } else {
      printf("[fileiras] lista de addons ainda e de outro perfil: nada registrado\n");
      fflush(stdout);
    }
  }

  // TETO DE FILEIRAS: o numero escolhido em Ajustes (7 de fabrica),
  // limitado pelo CAT_FIL_MAX do vetor. Ele corta o que vai ser PEDIDO pela
  // rede, e nao o desenho: sete fileiras tem de custar sete GET, senao o
  // ajuste economiza pixel e nao trabalho. As fixas nao fazem GET e nao
  // consomem a cota de catalogos.
  int teto = fil_limite() + nFixas;
  if (teto > CAT_FIL_MAX) teto = CAT_FIL_MAX;
  if (tetoSaida) *tetoSaida = teto;

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
  //
  // RESTRITA EM 24/09, decisao do dono (a regra e os porques estao em
  // cotacat.h, cota_vaga_garantida): so para addon NOVO, nunca por cima de
  // ordem propria, nunca com fileira desligada. Na C9 ela punha Pluto TV,
  // Minha TV, FrostView e Fenix TV — desligados na conta — e o Meu Futebol que
  // ele tinha tirado da home nas posicoes 11 a 13 de uma ordem arrumada a mao.
  { int nAd3 = addons_n(), q, c, dadas;
    static int addonDe[DECL_MAX];
    static char deslig[DECL_MAX];
    CotaAddon ad[16];
    int vagaAd[16], vagaCand[16];
    if (nAd3 > 16) nAd3 = 16;
    for (c = 0; c < nDecl; c++) {
      addonDe[c] = -1;
      deslig[c] = (char)desligada(&decls[c]);
      for (q = 0; q < nAd3; q++) {
        const char *b = addons_base(q);
        if (b && b[0] && decls[c].base && !strcmp(decls[c].base, b)) { addonDe[c] = q; break; }
      }
    }
    for (q = 0; q < nAd3; q++) {
      ad[q].ativo = addons_ativo(q);
      // NOVO: este perfil nunca viu fileira dele (fileiras.c) e nenhuma das
      // declaradas agora esta desligada — na TV, na conta ou engolida por
      // colecao. Uma desligada e uma decisao sobre o addon.
      ad[q].novo = ad[q].ativo && fil_addon_novo(addons_id_manifesto(q), addons_base(q));
      for (c = 0; c < nDecl && ad[q].novo; c++)
        if (addonDe[c] == q && deslig[c]) ad[q].novo = 0;
    }
    dadas = cota_vaga_garantida(ordem, nOrdem, addonDe, deslig, ad, nAd3,
                                teto - nFixas, fil_tem_ordem(), vagaAd, vagaCand, 16);
    if (registrar)
      for (q = 0; q < dadas && q < 16; q++) {
        int pos = 0;
        while (pos < nOrdem && ordem[pos] != vagaCand[q]) pos++;
        printf("[desc] vaga garantida (addon novo): %s entra em %d (%s)\n",
               addons_nome(vagaAd[q]), pos, decls[vagaCand[q]].titulo);
      }
  }

  // Uma resposta nova de manifesto nao muda a estrutura que a pessoa ja
  // aceitou. Enquanto a assinatura owner/perfil/idioma/config continuar
  // valida, as chaves do snapshot vem PRIMEIRO, na ordem dele; o usuario pode
  // acrescentar uma fileira explicitamente em Ajustes, o que invalida a
  // assinatura e libera a proxima montagem. Ainda registramos todas as
  // declaracoes em fileiras.c acima para permitir essa escolha depois.
  //
  // PRIORIDADE, NAO FILTRO (#195). O resto dos candidatos vai DEPOIS delas, e
  // nao para fora: com o snapshot inteiro respondendo o teto enche antes de
  // chegar neles (a mesma home de antes), mas chave do snapshot que falhou,
  // veio vazia, foi engolida por colecao ou sumiu do manifesto nao deixa mais
  // a vaga vazia para sempre. MEDIDO no .tpk 1.6.1 (@tonyh89): "0 pedido(s)
  // ... 1 de 16 fileira(s)" com 16 catalogos na ordem, volta apos volta — as
  // 6 chaves do snapshot tinham sido engolidas e as outras 9 nunca eram pedidas.
  if (homeestado_contexto_valido()) {
    int w = 0, resto[DECL_MAX], nResto = 0;
    for (k = 0; k < nOrdem; k++) {
      const Decl *d = &decls[ordem[k]];
      if (homeestado_tem_fileira(d->chave)) ordem[w++] = ordem[k];
      else resto[nResto++] = ordem[k];
    }
    nOrdem = w;
    // O snapshot aceito define a ordem estável. O manifesto pode reordenar
    // suas declarações entre ciclos sem representar uma escolha do usuário.
    for (k = 1; k < nOrdem; k++) {
      int atual = ordem[k];
      int rank = rankSnapshot(decls[atual].chave), j = k;
      while (j > 0) {
        int anterior = rankSnapshot(decls[ordem[j - 1]].chave);
        if (anterior <= rank) break;
        ordem[j] = ordem[j - 1]; j--;
      }
      ordem[j] = atual;
    }
    for (k = 0; k < nResto; k++) ordem[nOrdem++] = resto[k];
  }
  return nOrdem;
}

// AS DECLARACOES DA ULTIMA MONTAGEM, fora de montar(). Eram `static` dentro
// dela; sairam para estruturaNovaPedeRede poder refazer a escolha no fim da
// mesma volta. So montar() escreve, e so no fio dela.
static Decl declsMontagem[DECL_MAX];
static int nDeclsMontagem;
// 1 para cada declaracao que virou pedido de rede nesta volta.
static char declPedida[DECL_MAX];

// PEDIDOS QUE NAO VIRARAM FILEIRA, MAS CONTINUAM NA ESTRUTURA (#195).
//
// O snapshot do homeestado prioriza as chaves que ele guarda. Ate a 1.6.2 ele
// FILTRAVA: so chave do snapshot era pedida. Um catalogo que falhou uma vez
// (503 ou timeout sem lote anterior, ou resposta vazia) ficava de fora do
// snapshot, o seguinte da ordem tomava a vaga, e dali em diante o que falhou
// nunca mais era pedido — ate a pessoa mexer em algum ajuste. Medido: o Bharat
// Binge responde 503 a rajada de pedidos e "Netflix India" vem vazio. Estes
// entram no snapshot na posicao em que teriam ficado, para a proxima volta
// pedir de novo antes do substituto. So o fio da descoberta mexe.
typedef struct { char chave[192]; int pos; } PendSnap;
static PendSnap pendSnap[CAT_FIL_MAX];
static int nPendSnap;

static void pendSnapGuardar(const char *chave, int pos) {
  int k;
  for (k = 0; k < nPendSnap; k++) if (!strcmp(pendSnap[k].chave, chave)) return;
  if (nPendSnap >= CAT_FIL_MAX) return;
  snprintf(pendSnap[nPendSnap].chave, sizeof pendSnap[0].chave, "%s", chave);
  pendSnap[nPendSnap].pos = pos;
  nPendSnap++;
}

// Grava o snapshot com as fileiras publicadas E os pendentes nas posicoes
// deles. As chaves bastam ao homeestado; o resto da CatFileira fica zerado.
static int salvarSnapshot(const CatFileira *fils, int n, unsigned geracao) {
  static CatFileira todas[CAT_FIL_MAX];
  int i, k, m = 0;
  if (nManiFalhouVolta > 0) {
    printf("[desc] snapshot da home nao gravado: %d manifesto(s) sem resposta "
           "nesta volta\n", nManiFalhouVolta);
    fflush(stdout);
    return 0;
  }
  for (i = 0; i <= n && m < CAT_FIL_MAX; i++) {
    for (k = 0; k < nPendSnap && m < CAT_FIL_MAX; k++)
      if (pendSnap[k].pos == i || (i == n && pendSnap[k].pos > n)) {
        int t, ja = 0;
        for (t = 0; t < n && !ja; t++) if (!strcmp(fils[t].chave, pendSnap[k].chave)) ja = 1;
        if (ja) continue;
        memset(&todas[m], 0, sizeof todas[m]);
        snprintf(todas[m].chave, sizeof todas[m].chave, "%s", pendSnap[k].chave);
        m++;
      }
    if (i < n && m < CAT_FIL_MAX) todas[m++] = fils[i];
  }
  { double t0 = cat_relogio_ms();
    int r = homeestado_salvar_se_geracao(todas, m, geracao);
    printf("[perf] snapshot da home: %d fileiras em %.1f ms\n", m, cat_relogio_ms() - t0);
    fflush(stdout);
    return r; }
}

// A FONTE (dono, perfil, idioma, addons) continua a mesma de quando a volta
// comecou? E so ela que torna o dado buscado imprestavel. homeestado_geracao
// e chamada para o log dizer, na hora, o que mudou.
static int fonteIntacta(const HomeContexto *ini) {
  HomeContexto c;
  (void)homeestado_geracao();
  homeestado_contexto(&c);
  return !(homeestado_mudancas(ini, &c) & HOMEESTADO_MUDOU_FONTE);
}

// COM A ESTRUTURA DE AGORA, UMA MONTAGEM NOVA PEDIRIA ALGO QUE ESTA NAO PEDIU?
//
// Refaz a escolha de montar() — mesma ordem (ordenarCandidatos), mesmo filtro
// (desligada, que ja inclui a colecao que engole), mesmo teto — e anda pelos
// candidatos como o laco de rodadas anda. Candidato que ja virou fileira ocupa
// vaga; candidato pedido que nao respondeu e pulado (a montagem tambem pulou e
// seguiu adiante); o primeiro que NUNCA foi pedido e a resposta "sim", com o
// nome em `qual` para o log.
//
// Casos que isto separa: colecao nova engolindo fileiras (o teto abre vaga
// para catalogos que ficaram de fora), fileira religada ou promovida pela
// ordem da conta, limite maior. Ordem trocada entre fileiras ja buscadas,
// fileira escondida e limite menor dao "nao" — a remontagem sem rede resolve.
static int estruturaNovaPedeRede(const CatFileira *fils, int nFils,
                                 char *qual, size_t tamQual) {
  int ordem[DECL_MAX], nOrdem, teto = 0, nFixas = 0, cheias, k, t;
  if (qual && tamQual) qual[0] = 0;
  if (nDeclsMontagem < 1) return 0;
  for (t = 0; t < nFils; t++)
    if (!strcmp(fils[t].chave, "continue_watching") ||
        !strcmp(fils[t].chave, "social_activity")) nFixas++;
  lerPrefs();
  nOrdem = ordenarCandidatos(declsMontagem, nDeclsMontagem, ordem, nFixas, 0, &teto);
  cheias = nFixas;
  for (k = 0; k < nOrdem && cheias < teto; k++) {
    const Decl *d = &declsMontagem[ordem[k]];
    int repetida = 0;
    if (desligada(d)) continue;
    for (t = 0; t < k && !repetida; t++)
      if (!strcmp(declsMontagem[ordem[t]].chave, d->chave)) repetida = 1;
    if (repetida) continue;
    for (t = 0; t < nFils; t++) if (!strcmp(fils[t].chave, d->chave)) break;
    if (t < nFils) { cheias++; continue; }
    if (declPedida[ordem[k]]) continue;
    if (qual && tamQual) snprintf(qual, tamQual, "%s", d->titulo);
    return 1;
  }
  return 0;
}

static unsigned long long descAgoraMs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (unsigned long long)t.tv_sec * 1000ull + (unsigned long long)t.tv_nsec / 1000000ull;
}

// --- WATCHLIST E COLECAO DO TRAKT, EM PARALELO E NA TELA ASSIM QUE CHEGAM ---
//
// Eram buscadas no FIM de montar(), depois de todos os manifestos e de todos
// os catalogos, e so entravam na tela com a publicacao completa. MEDIDO na C9
// do dono (1.4.6-dev): "[trakt] watchlist: 112" e "collection: 98" aos 360 s,
// 45 s depois de escolher o perfil — o "Trakt demorou muito para aparecer" —
// enquanto as duas respostas nao dependem de addon nenhum.
//
// Agora um fio proprio as busca no primeiro instante da volta, e montar() as
// poe na tela no primeiro ponto em que estiverem prontas: dentro de cada
// publicacao em partes (tela vazia/parcial) ou, com uma home inteira na tela,
// por cat_mesclar_listas — que so marca e acrescenta, sem trocar fileira. No
// fim entram no lote como antes (mesmo lugar, mesma ordem), e a publicacao
// completa e quem poda o que saiu da lista.
//
// O fio e DESTACADO e o pedido tem dono: montar() que desiste no meio (volta
// condenada) larga o pedido e quem termina por ultimo libera.
#define LISTA_TRAKT_MAX TRAKT_LISTA_MAX
typedef struct {
  CatItem *wl, *col;
  int nWl, nCol;
  int pronto, largado;
} ListasTrakt;
static pthread_mutex_t listasTrava = PTHREAD_MUTEX_INITIALIZER;

static void listasLiberar(ListasTrakt *j) {
  if (!j) return;
  free(j->wl); free(j->col); free(j);
}

static CatItem *listaBuscar(const char *qual, int *n) {
  CatItem *v = NULL, *menor;
  *n = trakt_lista_cresc(qual, &v, LISTA_TRAKT_MAX);
  if (*n <= 0) { free(v); *n = 0; return NULL; }
  menor = realloc(v, sizeof(CatItem) * (size_t)*n);
  return menor ? menor : v;
}

static void *fioListas(void *u) {
  ListasTrakt *j = u;
  int nw = 0, nc = 0;
  CatItem *wl = listaBuscar("watchlist", &nw);
  CatItem *col = listaBuscar("collection", &nc);
  pthread_mutex_lock(&listasTrava);
  if (j->largado) {
    pthread_mutex_unlock(&listasTrava);
    free(wl); free(col); free(j);
    return NULL;
  }
  j->wl = wl; j->nWl = nw; j->col = col; j->nCol = nc;
  j->pronto = 1;
  pthread_mutex_unlock(&listasTrava);
  return NULL;
}

// NULL se nem o fio deu para criar: montar() busca no fim, no proprio fio.
static ListasTrakt *listasLargar(void) {
  ListasTrakt *j = calloc(1, sizeof *j);
  pthread_t t;
  if (!j) return NULL;
  if (pthread_create(&t, NULL, fioListas, j) != 0) { free(j); return NULL; }
  pthread_detach(t);
  return j;
}

static int listasProntas(ListasTrakt *j) {
  int r;
  if (!j) return 0;
  pthread_mutex_lock(&listasTrava);
  r = j->pronto;
  pthread_mutex_unlock(&listasTrava);
  return r;
}

// Quem desiste de uma volta larga o pedido: se o fio ja acabou, libera aqui;
// senao ele libera quando acabar.
static void listasAbandonar(ListasTrakt *j) {
  int pronto;
  if (!j) return;
  pthread_mutex_lock(&listasTrava);
  pronto = j->pronto;
  if (!pronto) j->largado = 1;
  pthread_mutex_unlock(&listasTrava);
  if (pronto) listasLiberar(j);
}

// PUBLICACAO EM PARTES, com as listas do Trakt (se ja chegaram) de rabo.
// `n` e o tamanho do lote de verdade; as listas vao para depois dele como
// rascunho — a proxima fileira escreve por cima, e a publicacao seguinte as
// copia de novo. Com o lote sem espaco e sem memoria para crescer, publica sem
// as listas: elas voltam na proxima.
static void publicarParcial(CatItem **lote, int *cap, int n,
                            const CatFileira *fils, int nf, ListasTrakt *j,
                            int *listasNaTela) {
  int extra = 0;
  if (listasProntas(j)) extra = j->nWl + j->nCol;
  if (extra && n + extra > *cap) {
    CatItem *maior = realloc(*lote, sizeof(CatItem) * (size_t)(n + extra));
    if (maior) { *lote = maior; *cap = n + extra; } else extra = 0;
  }
  if (extra) {
    if (j->nWl) memcpy(*lote + n, j->wl, sizeof(CatItem) * (size_t)j->nWl);
    if (j->nCol) memcpy(*lote + n + j->nWl, j->col, sizeof(CatItem) * (size_t)j->nCol);
    if (listasNaTela && !*listasNaTela) {
      *listasNaTela = 1;
      printf("[desc] listas do Trakt na tela junto das fileiras: %d + %d\n", j->nWl, j->nCol);
      marco("listas do trakt na tela");
    }
  }
  publicarMontagem(*lote, n + extra, fils, nf);
  parcialNaTela = 1;
}

typedef struct { pthread_t fio; int viva, n; CatItem itens[8]; } SocialFio;
static void *fioSocial(void *u) {
  SocialFio *f = (SocialFio *)u;
  // "Recursos sociais" desligado: sem feed do Trakt, sem amigos do Nuvio.
  if (!ajustes_social()) { f->n = 0; return NULL; }
  f->n = trakt_social(f->itens, 8);
  f->n = recomenda_social_mesclar(f->itens, f->n, 8);
  return NULL;
}

static void *montar(void *u) {
  // O lote tambem cresce: era dimensionado por CAT_MAX e por isso herdava o
  // mesmo teto arbitrario.
  int cap = 128;
  CatItem *lote = malloc(sizeof(CatItem) * (size_t)cap);
  int n = 0, i;
  int nContinuar = 0, nSocial = 0;
  unsigned socialGeracao;
  unsigned minhaGeracao = montagemGeracao;
  // O CONTEXTO EM QUE ESTA VOLTA BUSCA, em partes (ver homeestado.h). A
  // identidade vale do inicio: o Trakt e lido logo abaixo. Os addons sao
  // recapturados no instante em que a lista e lida, e a estrutura no instante
  // em que a ordem e decidida — e contra ESSES instantes que o fim compara.
  HomeContexto ctxIni;
  char donoIni[64];
  (void)homeestado_geracao();
  homeestado_contexto(&ctxIni);
  snprintf(donoIni, sizeof donoIni, "%s", sessao_usuario() ? sessao_usuario() : "");
  // PUBLICAR EM PARTES SO DURANTE O ARRANQUE DO CATALOGO.
  //
  // A publicacao fileira a fileira existe para a PRIMEIRA home aparecer cedo.
  // Numa volta seguinte (sync que trouxe addons, remontagem pedida, ciclo de
  // 5 min) ela e o oposto disso: a home que estava inteira na tela encolhe
  // para duas fileiras e vai crescendo de novo — MEDIDO no log desta LG,
  // "[home] 13 fileiras na tela" seguido de "6", "8", "9", "11", "13", "14"...
  // Era o "ela fica recarregando" do dono. Com algo na tela, a volta monta em
  // silencio e publica UMA vez no fim — e so se mudou (ver a assinatura).
  // So CW/social na tela ainda nao e uma home completa. O fio de CW pode
  // publica-los antes deste ciclo e, com cat_n()>0, segurava a primeira fileira
  // pronta ate o ultimo catalogo terminar. Leitura coerente sob pubTrava evita
  // tomar o n==0 transitorio de uma troca de bloco por uma tela vazia. Listas,
  // colecoes e qualquer catalogo pronto continuam no modo silencioso.
  // Tela PARCIAL (volta interrompida) continua crescendo: parcialNaTela.
  int progressivo = cat_home_apenas_fixas() || parcialNaTela;
  // As listas do Trakt ja foram para a tela nesta volta (em partes ou mescladas).
  int listasNaTela = 0;
  // Tamanho do lote na ultima publicacao em partes DESTA volta; -1 = nenhuma.
  int nPublicado = -1;
  // Onde a volta estava quando foi condenada, para a linha do log.
  const char *ondeParou = "";
  ListasTrakt *listas = NULL;
  // Versao da lista de addons que maniLargar viu. Ver o relargar na leitura.
  unsigned versaoLargada;
  (void)u;
  pthread_mutex_lock(&cargaTrava);
  memset(&carga, 0, sizeof carga); cargaDesde = SDL_GetTicks();
  pthread_mutex_unlock(&cargaTrava);
  if (!lote) { buscandoSoltar(); return NULL; }

  // O "continue assistindo" vem PRIMEIRO e do Trakt. A home usa as primeiras
  // posicoes do catalogo nessa fileira, entao a ordem aqui e o que define o
  // que aparece la — e o historico tem de ganhar das recomendacoes.
  marco("montar: inicio");
  nManiFalhouVolta = 0; nPendSnap = 0;     // ver salvarSnapshot
  // LIMPEZA UNICA DO #197 (fileiras.c, fil_migrar_197). So com a ordem da conta
  // na mao: ela e a prova de escolha que a limpeza usa; sem ela, fica para a
  // volta em que a ordem chegar. Depois da primeira vez e um teste de inteiro.
  { int nc = catordem_n();
    if (nc > 0) {
      const char **lig = (const char **)malloc(sizeof *lig * (size_t)nc);
      int q, m = 0;
      if (lig) {
        for (q = 0; q < nc; q++) {
          const char *ch = catordem_chave(q);
          if (ch[0] && !catordem_oculta(ch, ch)) lig[m++] = ch;
        }
        // Mudou algo: gravar() sobe fil_revisao e a home remonta sozinha.
        fil_migrar_197(lig, m);
        free(lig);
      }
    } }
  // OS MANIFESTOS COMECAM A CHEGAR AGORA, nao daqui a seis segundos. Ver o
  // cabecalho de maniLargar: eles nao dependem do Trakt, e eram o bloco de 7 s
  // logo depois dele.
  versaoLargada = addons_versao();
  cargaFase(1);
  maniLargar();
  // E a watchlist/colecao do Trakt tambem: ver ListasTrakt.
  listas = listasLargar();
  // VOLTA CONDENADA PARA AQUI, e nao no fim. desc_repetir (credencial, idioma,
  // addons depois de a lista ser lida) troca montagemGeracao, e uma volta com a
  // geracao velha ja estava destinada ao descarte do fim — so que chegava la
  // depois de pagar todos os manifestos e catalogos. MEDIDO na C9 do dono: 45 s
  // de uma volta que ninguem ia ver. Os pontos abaixo sao os seguros: fora de
  // trava, sem fio de catalogo esperando por quem sai.
#define CONDENADA(onde) (minhaGeracao != montagemGeracao ? (ondeParou = (onde), 1) : 0)
  // AS LISTAS DO TRAKT ASSIM QUE CHEGAREM, em qualquer ponto seguro. Com a
  // home inteira na tela (volta silenciosa) so marca e acrescenta; em partes,
  // entram de rabo na publicacao seguinte (ou nesta, se ja houve uma).
#define LISTAS_SE_PRONTAS() do { \
    if (!listasNaTela && listasProntas(listas) && listas->nWl + listas->nCol > 0 && \
        minhaGeracao == montagemGeracao && fonteIntacta(&ctxIni)) { \
      if (!progressivo) { \
        listasNaTela = 1; \
        if (listas->nWl) cat_mesclar_listas(listas->wl, listas->nWl); \
        if (listas->nCol) cat_mesclar_listas(listas->col, listas->nCol); \
        marco("listas do trakt na tela"); \
      } else if (n == nPublicado || n == 0) { \
        /* So com o lote igual ao que esta na tela: o rabo de rascunho nao \
           pode cobrir item que ainda nao foi publicado. */ \
        if (nPublicado < 0) nFileirasMontadas = 0; \
        nPublicado = n; \
        publicarParcial(&lote, &cap, n, filsMontadas, nFileirasMontadas, \
                        listas, &listasNaTela); \
      } \
    } } while (0)
  // AS DUAS FONTES, UNIDAS. Ver o cabecalho de montarContinuar: com Trakt
  // vinculado esta fileira ignorava o progresso da conta Nuvio, que e o que
  // chega do celular do dono.
  // A ATIVIDADE DOS AMIGOS SAI JUNTO COM O CONTINUAR (B2, arranque). Eram em
  // fila: o social so comecava depois do Continuar inteiro (medido na LG: 7 s,
  // Android 2,7 s, em cima de 9-18 s do Continuar) e os catalogos esperam os
  // dois. Nao dependem um do outro: o social vai para um buffer proprio num fio
  // e entra no lote logo depois do Continuar, na mesma ordem de antes.
  socialGeracao = recomenda_geracao();
  SocialFio socFio;
  memset(&socFio, 0, sizeof socFio);
  socFio.viva = pthread_create(&socFio.fio, NULL, fioSocial, &socFio) == 0;
  pthread_mutex_lock(&contTrava);
  nContinuar = montarContinuar(lote, CONT_MAX);
  cwGerMontar = cwGer;
  n += nContinuar;
  marco("trakt continuar assistindo");
  if (CONDENADA("depois do continuar assistindo")) {
    pthread_mutex_unlock(&contTrava);
    if (socFio.viva) pthread_join(socFio.fio, NULL);
    goto condenada;
  }
  // O feed social oficial e uma fileira propria, logo depois do retorno ao
  // que estava sendo visto. Ele vem cedo para nao depender dos manifestos dos
  // addons e usa a mesma credencial Trakt ja carregada.
  // Sob a MESMA trava: trakt_social e montarContinuar compartilham os buffers
  // de trakt_enfeitar_lote com o fio de desc_refazer_continuar.
  // ... E OS AMIGOS DO NUVIO. 251 das 310 contas do servico social nao tem
  // Trakt (docs/ANALISE-ADDONS-AMIGOS.md): para elas a fileira so existia vazia.
  // A uniao (Trakt + amigos mutuos do nosso servico) ja vem pronta do fio.
  pthread_mutex_unlock(&contTrava);
  if (socFio.viva) pthread_join(socFio.fio, NULL);
  else fioSocial(&socFio);
  nSocial = socFio.n;
  if (nSocial > 0) memcpy(lote + n, socFio.itens, sizeof(CatItem) * (size_t)nSocial);
  n += nSocial;
  marco("trakt atividade dos amigos");
  if (CONDENADA("depois da atividade dos amigos")) goto condenada;
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
      fs->socialGeracao = socialGeracao;
    }
    nFileirasMontadas = nf;
    nPublicado = n;
    publicarParcial(&lote, &cap, n, filsMontadas, nf, listas, &listasNaTela);
    marco("continuar assistindo na tela");
  }
  LISTAS_SE_PRONTAS();
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
    Decl *decls = declsMontagem;
    int nDecl = 0, k;
    // static: 40 KB. montar() roda num fio so por vez (ver declsMontagem).
    static CatFileira fil[CAT_FIL_MAX];
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
      fs->socialGeracao = socialGeracao;
    }

    lerPrefs();
    // Zera ANTES de ler os manifestos: cada catalogo com busca se registra
    // sozinho la dentro, na hora em que e lido.
    desc_alvos_busca_zerar();
    foraCotaSoltar();
    nSoBuscaVolta = 0;
    // Sem `nDecl < DECL_MAX` no laco: com o vetor cheio o manifesto do addon
    // seguinte nem era baixado, e AIOStreams e Akashi TV ficavam invisiveis
    // para o app inteiro so porque o Xperience, lido antes, declara 605
    // catalogos. lerManifesto ja para de GRAVAR sozinho quando enche.
    // ESPERA OS ADDONS DA CONTA antes de ler a lista. Esta volta larga no
    // arranque, e o sync so entrega os addons do perfil depois que a pessoa o
    // escolhe: lida vazia, a volta pedia 0 catalogos e PUBLICAVA 2 fileiras por
    // cima das 7 do cache (e gravava o cache menor), e a Home refazia mais duas
    // vezes — 2, 4, 9, 15 fileiras chegando de uma vez, sem aviso na ilha
    // (todo arranque da TCL do dono desde 05/10/2026 a tarde; nos bons os
    // addons chegavam antes: "7 ad13 A 15"). Enquanto o perfil nao foi
    // escolhido espera sem prazo (a tela e a de perfis); depois, ate 10 s.
    // Sem conta, sync falho ou remontagem pedida: segue como antes.
    // Uma espera por execucao: conta sem addon nenhum paga os 10 s uma vez.
    // Quem encerra antes e a chegada dos addons ou qualquer remontagem pedida
    // (o sync pede uma ao aplicar a conta: montagemGeracao muda).
    // Quem sabe de conta e de perfil e o app (desc_espera_addons_definir): os
    // testes que compilam este arquivo sozinho nao tem sessao nem perfis.
    { static int jaEsperou;
      int k;
      // ESPERA O CICLO INTEIRO DA CONTA, nao so os addons: as colecoes e a
      // ordem chegam ~3 s depois deles (TCL: addons 06.05, "sync aplicado"
      // 09.31), e a volta que ja tinha escolhido os catalogos refazia a Home
      // no meio — 14 fileiras, cai para 8, volta para 14 com outras. Enquanto
      // isso a tela e a do cache.
      if (!jaEsperou && esperaAddons && esperaAddons()) {
        Uint32 desde = 0;
        jaEsperou = 1;
        printf("[desc] esperando a conta (addons, colecoes, ordem) antes de montar a Home\n"); fflush(stdout);
        while (minhaGeracao == montagemGeracao && (k = esperaAddons()) != 0) {
          if (k == 2) {   // perfil ja escolhido: o prazo corre
            if (!desde) desde = SDL_GetTicks();
            else if (SDL_GetTicks() - desde > 10000u) break;
          }
          SDL_Delay(50);
        }
        printf("[desc] conta pronta: %d addon(s) (espera encerrada)\n", addons_n()); fflush(stdout);
      } }
    // A LISTA DE ADDONS E LIDA AQUI. Marcar o instante e o que permite dizer,
    // no fim, se um pedido de remontagem que chegou no meio do caminho ja foi
    // atendido por esta volta. Ver geracaoPedida.
    pthread_mutex_lock(&listaTrava);
    geracaoLida = geracaoPedida;
    listaLidaNaVolta = 1;
    { HomeContexto c; homeestado_contexto(&c); ctxIni.addons = c.addons; }
    pthread_mutex_unlock(&listaTrava);
    // A LISTA MUDOU DESDE A LARGADA (o sync entregou os addons do perfil com
    // a volta ja no ar, o caso da escolha de perfil): os downloads em paralelo
    // eram das URLs da lista VELHA, e cada addon da nova caia no rede_baixar
    // serial de lerManifesto — um de cada vez, ate 20 s cada. MEDIDO na C9 do
    // dono: 30 s entre "trakt atividade dos amigos" e "manifestos lidos" com 12
    // addons. Larga de novo com a lista de agora; o cache por URL+versao e o
    // mesmo, e os fios da largada velha saem sozinhos (maniGeracao).
    if (addons_versao() != versaoLargada) {
      printf("[desc] lista de addons mudou desde a largada: manifestos da lista "
             "nova em paralelo\n");
      fflush(stdout);
      versaoLargada = addons_versao();
      maniLargar();
    }
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
    versaoManifestos = addons_versao();
    passadaVolta = fil_passada_ler();
    { int nAd = addons_n();
      int cota = nAd > 0 ? DECL_MAX / nAd : DECL_MAX;
      int folga = 0;
      if (cota < 1) cota = 1;
      for (i = 0; i < nAd; i++) {
        int teto = cota + folga;
        int lidos, real = 0, promovidos = 0;
        LISTAS_SE_PRONTAS();
        if (CONDENADA("lendo os manifestos")) goto condenada;
        if (teto > DECL_MAX - nDecl) teto = DECL_MAX - nDecl;
        lidos = lerManifesto(i, addons_base(i), decls + nDecl, teto,
                             addons_ativo(i), &real, &promovidos);
        nDecl += lidos;
        folga = lidos < cota + folga ? cota + folga - lidos : 0;
        // ISSUE #42(a): a linha de sempre ("N catalogo(s) declarado(s)") nao
        // dizia se N era o TOTAL do addon ou so o que a cota deixou passar —
        // quem lia o log via "8 catalogo(s)" e nao tinha como saber que o
        // addon declarava 40. `real` (o total que o manifesto tem de verdade,
        // contado em lerManifesto mesmo depois de `saida` encher) torna o
        // corte visivel e diz o numero que falta.
        if (!addons_ativo(i))
          printf("[desc]   %s: desligado na conta, nenhum catalogo vira fileira\n",
                 addons_nome(i));
        else if (real > lidos)
          printf("[desc]   %s: %d catalogo(s) declarado(s) (cota %d, "
                 "manifesto tem %d — %d de fora por cota; %d escolhido(s) "
                 "alem da ordem do manifesto)\n",
                 addons_nome(i), lidos, cota, real, real - lidos, promovidos);
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
      cortados += nSoBuscaVolta;
      if (cortados)
        printf("[desc] %d catalogo(s) so respondem com busca e nao viram fileira\n", cortados);
      nDecl = w2; }
    // CANAL E DO GUIA, NAO DA HOME. Ligar um addon de TV ao vivo no guia fazia
    // o catalogo "channel" virar candidato (e, por ser addon novo, ganhar a
    // vaga garantida) e aparecer na home sem a pessoa pedir. So fica quem ela
    // ja escolheu na TV (fil_escolhida); o guia nao depende disto, ele le o
    // manifesto do addon.
    { int r2, w2 = 0, canais = 0;
      for (r2 = 0; r2 < nDecl; r2++) {
        if (ehCanal(decls[r2].tipo) && fil_escolhida(decls[r2].chave) < 0) {
          canais++;
          continue;
        }
        if (w2 != r2) decls[w2] = decls[r2];
        w2++;
      }
      if (canais) printf("[desc] %d catalogo(s) de canal ficam no guia, nao na home\n", canais);
      nDecl = w2; }
    printf("[desc] %d catalogos declarados pelos addons\n", nDecl);
    // FANTASMAS. Addon removido da conta deixava os catalogos dele na lista de
    // fileiras — e na home — ate o proximo login (@rawldon). Aqui TODOS os
    // manifestos desta volta ja foram lidos, entao a lista de addons vivos e
    // completa e a poda e segura: so cai catalogo cujo addon nao esta mais na
    // conta E que ninguem registrou nesta sessao.
    //
    // E SO COM UMA LISTA QUE SE SABE COMPLETA E DESTE PERFIL. Tres condicoes,
    // todas medidas como falha na C9 do dono (24/09, "16 fileira(s) de addon
    // que ja nao existe sairam da lista" no arranque, com 4 addons do pacote
    // contra os 12 da conta do perfil 1):
    //   - a lista veio da conta do perfil ativo (fil_podar_catalogos compara
    //     addons_perfil_da_lista com o perfil da escolha);
    //   - e a MESMA lista com que esta volta leu os manifestos: trocada no
    //     meio, os ids dela ainda estao vazios e nada casaria;
    //   - todo addon tem o id do manifesto: um que nao respondeu nesta volta
    //     teria as fileiras dele tomadas por fantasma.
    // #319: ADDON DESLIGADO NAO TEM MANIFESTO LIDO (nao entra na volta), e
    // contava como "sem manifesto": com UM addon desligado na conta a poda
    // ficava adiada para sempre ("poda adiada: 1 addon(s) sem manifesto" no log
    // S3R7Q0, Pluto TV desligado) e nenhum fantasma saia. So o addon LIGADO sem
    // id e uma volta incompleta; o desligado casa pela base (doAddon) e as
    // linhas dele nao valem nada para a home de qualquer jeito.
    { const char *ids[16], *bases[16];
      int ativos[16];
      int na = addons_n(), q, semId = 0;
      if (na > 16) na = 16;
      for (q = 0; q < na; q++) {
        ids[q] = addons_id_manifesto(q); bases[q] = addons_base(q);
        ativos[q] = addons_ativo(q);
        if (ativos[q] && (!ids[q] || !ids[q][0])) semId++;
      }
      if (addons_versao() != versaoManifestos)
        printf("[fileiras] poda adiada: a lista de addons mudou durante a volta\n");
      else if (semId)
        printf("[fileiras] poda adiada: %d addon(s) sem manifesto lido nesta volta\n", semId);
      else {
        int perfilLista = addons_perfil_da_lista();
        if (fil_podar_catalogos(ids, bases, na, perfilLista)) fil_gravar_registro();
        // As que ficaram (com escolha da pessoa) deixam de ocupar vaga.
        { int m = fil_marcar_sem_addon(ids, bases, ativos, na, perfilLista);
          if (m) printf("[fileiras] %d fileira(s) de addon removido/desligado fora da conta de vagas\n", m); }
      } }

    // ALVOS DE BUSCA. Independem da ordem/filtro das FILEIRAS da home: um
    // catalogo pode estar desativado na home e ainda assim ser bom para
    // procurar (o Akashi so tem busca, nao tem fileira que valha a pena).
    printf("[desc] %d alvos de busca\n", desc_busca_n_alvos());
    marco("manifestos lidos");
    LISTAS_SE_PRONTAS();
    if (CONDENADA("depois dos manifestos")) goto condenada;

    // ensureOrderKeysWithPrefs: a ordem salva primeiro, e as chaves NOVAS
    // acrescentadas no fim. Catalogo que o addon passou a declarar hoje entra
    // por ultimo, nao no meio — e o que evita a home se reorganizar sozinha.
    {
      int ordem[DECL_MAX];
      int nOrdem, teto;
      nDeclsMontagem = nDecl;
      memset(declPedida, 0, sizeof declPedida);
      { HomeContexto c; homeestado_contexto(&c);
        ctxIni.ajustes = c.ajustes; ctxIni.fileiras = c.fileiras;
        ctxIni.ordemConta = c.ordemConta; ctxIni.colecoes = c.colecoes; }
      nOrdem = ordenarCandidatos(decls, nDecl, ordem, nFil, 1, &teto);
      // OS QUE A COTA NAO LEU TAMBEM PODEM SER ESCOLHIDOS (#126). Entram na
      // lista de Fileiras da Home DEPOIS dos candidatos e sem despejar
      // ninguem; escolher um la (ligar/mover para a home) e o que faz a cota da
      // proxima volta le-lo — ver prioCatalogo. Nao custa rede: nenhum deles e
      // pedido ate ser escolhido.
      if (nForaCota > 0) {
        int q;
        for (q = 0; q < nForaCota; q++)
          fil_registrar_se_couber_de(&passadaVolta, foraCota[q].chave, foraCota[q].titulo,
                                  foraCota[q].addon, foraCota[q].tipo);
        fil_gravar_registro();
        printf("[desc] %d catalogo(s) fora da cota listados em Fileiras da Home "
               "para escolha\n", nForaCota);
      }
      foraCotaSoltar();

      cargaFase(2);
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
      // EM RODADAS, e nao num lote so. Com um lote de exatamente `teto`
      // pedidos, cada catalogo que responde VAZIO custa uma fileira a menos na
      // tela: o dono escolheria 7 e veria 5, sem nada dizendo por que. A rodada
      // seguinte pede exatamente o que faltou. O caso comum — todos respondendo
      // — continua sendo UMA rodada, com o mesmo custo de antes.
      while (nFil < teto && cursor < nOrdem) {
        int alvo = teto - nFil;
        RodadaCat *rod;
        TarefaCat *tarefas;
        int nTarefas = 0;
        if (CONDENADA("antes de pedir os catalogos")) goto condenada;
        rod = calloc(1, sizeof *rod);
        tarefas = rod ? calloc((size_t)alvo, sizeof(TarefaCat)) : NULL;
        if (!tarefas) { free(rod); break; }
        rod->t = tarefas;
        rodadas++;
        // ETAPA 1 — escolher as fileiras DESTA rodada. Os filtros (desligada,
        // repetida) sao locais e baratos; fazer isto antes deixa os fios so com
        // a parte cara, que e a rede.
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
          tarefas[nTarefas].d = d;
          // COPIAS para o fio: um fio largado por lento sobrevive a esta volta,
          // e `decls` (estatico) e a lista de addons ja podem ser de outra.
          snprintf(tarefas[nTarefas].base, sizeof tarefas[nTarefas].base, "%s", d->base ? d->base : "");
          snprintf(tarefas[nTarefas].tipo, sizeof tarefas[nTarefas].tipo, "%s", d->tipo);
          snprintf(tarefas[nTarefas].id, sizeof tarefas[nTarefas].id, "%s", d->id);
          nTarefas++;
          declPedida[ordem[cursor]] = 1;
        }
        if (!nTarefas) { free(tarefas); free(rod); break; }
        pedidos += nTarefas;
        rod->nT = nTarefas;
        rod->refs = 1;                       // quem monta
        rod->inicioMs = rod->ultimoProntoMs = descAgoraMs();

        // ETAPA 2 — CAT_FIOS trabalhando na fila, DESTACADOS: quem monta nao
        // espera fio nenhum no fim da rodada (ver RodadaCat).
        { int criados = 0, q;
          for (q = 0; q < CAT_FIOS && q < nTarefas; q++) {
            pthread_t tf;
            pthread_mutex_lock(&catTrava); rod->refs++; pthread_mutex_unlock(&catTrava);
            if (pthread_create(&tf, NULL, fioCatalogo, rod) == 0) { pthread_detach(tf); criados++; }
            else { pthread_mutex_lock(&catTrava); rod->refs--; pthread_mutex_unlock(&catTrava); }
          }
          // Sem NENHUM fio (pthread_create falhou em todos), le em serie no
          // proprio fio: pior desempenho, mesmo resultado. Melhor que home vazia.
          if (!criados) {
            pthread_mutex_lock(&catTrava); rod->refs++; pthread_mutex_unlock(&catTrava);
            fioCatalogo(rod);
          }

          // ETAPA 3 — montar NA ORDEM, publicando cada fileira assim que o balde
          // dela fica pronto. Esperar o balde k nao desperdica tempo: os fios
          // seguem enchendo k+1, k+2 enquanto este e consumido.
          for (k = 0; k < nTarefas && nFil < teto; k++) {
            const Decl *d = tarefas[k].d;
            int got, respondeu, largado = 0;
            int estadoLinha = 0;
            const CatItem *origem = tarefas[k].itens;
            // Rascunho de quem monta para a linha anterior de um catalogo
            // LARGADO: o balde dele ainda e do fio, que pode escrever nele.
            // static: MAX_POR_FILEIRA CatItem passam de 370 KB, e montar() roda num fio so.
            static CatItem daLinhaAnterior[MAX_POR_FILEIRA];
            for (;;) {
              int pr;
              unsigned long long agora = descAgoraMs();
              pthread_mutex_lock(&catTrava);
              pr = tarefas[k].pronto;
              // O CATALOGO LENTO NAO SEGURA A RODADA. Fila vazia (nada mais
              // para comecar), nenhum catalogo terminou ha CAT_ESPERA_SILENCIO_MS
              // e este esta no ar ha CAT_ESPERA_MIN_MS: publica sem ele. MEDIDO
              // na C9 do dono: "[rede] falha 28" (os 8 s do lerCatalogo) era a
              // ultima linha antes do resumo da rodada — os outros ja tinham
              // chegado. A resposta, se vier, o fio joga fora.
              if (!pr && tarefas[k].inicioMs && rod->prox >= rod->nT &&
                  agora - rod->ultimoProntoMs >= CAT_ESPERA_SILENCIO_MS &&
                  agora - tarefas[k].inicioMs >= CAT_ESPERA_MIN_MS) {
                tarefas[k].largada = 1;
                largado = 1;
              }
              pthread_mutex_unlock(&catTrava);
              if (pr || largado) break;
              if (CONDENADA("esperando os catalogos")) { rodadaSoltar(rod); goto condenada; }
              LISTAS_SE_PRONTAS();
              SDL_Delay(10);
            }
            if (largado) {
              got = 0; respondeu = 0;
              printf("[desc] catalogo lento: %s (%s); segue sem ele depois de %llu ms\n",
                     d->titulo, d->nomeAddon,
                     (unsigned long long)(descAgoraMs() - tarefas[k].inicioMs));
            } else {
              got = tarefas[k].n;
              respondeu = tarefas[k].respondeu;
            }
            if (respondeu) responderam++;
            else { pthread_mutex_lock(&cargaTrava); carga.falhas++; pthread_mutex_unlock(&cargaTrava); }
            if (respondeu && !got) {
              // Respondeu SEM `metas`: o catalogo existe e esta vazio hoje. Nao e
              // erro e nao pode virar titulo pendurado na home — a rodada seguinte
              // pede outro no lugar dele.
              vazios++;
              printf("[desc] catalogo vazio: %s\n", d->titulo);
              // Vazio valido: a chave continua na ESTRUTURA (snapshot, via
              // pendSnap), mas nao ocupa vaga na tela (#195). Desde 9b17d38
              // ela virava fileira so com titulo, sem card, e contava no
              // limite: no .tpk 1.6.1 do @tonyh89 os 6 catalogos pedidos
              // vieram vazios e a home ficou com seis titulos sem nada; o
              // Bharat Binge serve "Netflix India" vazio hoje. A rodada
              // seguinte pede outro no lugar, como o laco foi desenhado.
              pendSnapGuardar(d->chave, nFil);
              continue;
            }
            if (!respondeu && !got) {
              // ISSUE #42(a): antes disto o log so tinha o resumo do fim da
              // rodada ("N pedidos, M responderam") — quem quisesse saber QUAL
              // fileira sumiu tinha de adivinhar por subtracao. Nomear o
              // catalogo aqui, igual ao "catalogo vazio" acima, e o que falta
              // para responder "por que esta fileira nao apareceu" por fileira
              // pedida, e nao so por total.
              if (!largado)
                printf("[desc] catalogo sem resposta a tempo: %s (%s)\n",
                       d->titulo, d->nomeAddon);
              // Timeout preserva o ultimo lote bom desta chave. No primeiro
              // arranque, sem snapshot, segue omitida de forma segura.
              got = linhaAnterior(d->chave, daLinhaAnterior, MAX_POR_FILEIRA, NULL);
              origem = daLinhaAnterior;
              // Sem lote anterior a vaga vai para o seguinte, mas a chave fica
              // no snapshot: a proxima volta pede este de novo (#195).
              if (!got) { pendSnapGuardar(d->chave, nFil); continue; }
            }
            GARANTE(MAX_POR_FILEIRA + 2);
            if (got > cap - n) got = cap - n;
            if (got > 0)
              memcpy(lote + n, origem, sizeof(CatItem) * (size_t)got);
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
          pthread_mutex_lock(&cargaTrava); carga.fileiras++; pthread_mutex_unlock(&cargaTrava);
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
          // So publica em partes com a tela VAZIA (ou parcial). Sobre o cache —
          // ou sobre a home da volta anterior — seria um retrocesso visivel: 16
          // fileiras viram 1. Ver `progressivo` no inicio de montar().
          // filsMontadas so muda JUNTO com a publicacao: sem ela o bloco da tela
          // e outro. Estrutura que mudou no meio NAO interrompe: estas fileiras
          // sao da conta/perfil/addons certos, e o fim da volta as rearruma.
          if (progressivo && minhaGeracao == montagemGeracao && fonteIntacta(&ctxIni)) {
            nFileirasMontadas = nFil;
            memcpy(filsMontadas, fil, sizeof(CatFileira) * (size_t)nFil);
            nPublicado = n;
            publicarParcial(&lote, &cap, n, filsMontadas, nFileirasMontadas,
                            listas, &listasNaTela);
          }
          // Bandeira propria: `nFil == 1` nunca acontece aqui porque a fileira
          // "Continuar assistindo" ja ocupou a posicao 0 antes do laco.
          if (!marcouPrimeira) { marcouPrimeira = 1;
                                 marco(progressivo ? "primeira fileira da rede na tela"
                                                  : "primeira fileira da rede pronta (publicacao pendente)"); }
          }
          rodadaSoltar(rod);
        }
      }   /* fim da rodada */
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
      // #327: O DESTAQUE NAO PODE DEPENDER DE UMA FILEIRA DESENHADA. Com todos
      // os catalogos dentro de pastas de colecao (ou fora da Home) nenhuma
      // fileira de catalogo era pedida, o catalogo ficava vazio e o destaque
      // abria sem imagem. Aqui se baixa UM catalogo so para alimentar o
      // destaque: a escolhida em "Catalogos do destaque", ou, sem fileira de
      // catalogo nenhuma, o primeiro da ordem. A fileira entra marcada como
      // fora da Home (ela esta engolida/oculta/na fila), entao a home nao a
      // desenha — so o destaque usa os titulos.
      heroAlimChave[0] = 0;
      if (nFil < CAT_FIL_MAX) {
        const char *hf = fil_hero_fonte();
        const Decl *dh = NULL;
        int temCat = 0, q, ja = 0;
        for (q = 0; q < nFil; q++) if (fil[q].base[0]) { temCat = 1; break; }
        if (hf[0] && !(hf[0] == '*' && !hf[1])) {
          for (q = 0; q < nFil && !ja; q++) ja = !strcmp(fil[q].chave, hf);
          if (!ja) for (q = 0; q < nDecl; q++)
            if (!strcmp(decls[q].chave, hf)) { dh = &decls[q]; break; }
        } else if (!temCat) {
          for (q = 0; q < nOrdem && !dh; q++) if (!fil_oculta(decls[ordem[q]].chave)) dh = &decls[ordem[q]];
          for (q = 0; q < nOrdem && !dh; q++) dh = &decls[ordem[q]];
        }
        if (dh && dh->base && dh->base[0] && (fil_estado_chave(dh->chave) == FIL_FORA ||
                                              fil_estado_chave(dh->chave) == FIL_NA_FILA)) {
          static CatItem hItens[MAX_POR_FILEIRA];
          int resp = 0, qtd = ajustes_itens_fileira(), got;
          if (qtd > MAX_POR_FILEIRA) qtd = MAX_POR_FILEIRA;
          got = lerCatalogo(dh->base, dh->tipo, dh->id, hItens, MAX_POR_FILEIRA, qtd, &resp);
          printf("[desc] destaque: catalogo %s so para o destaque (%d titulo(s))\n", dh->titulo, got);
          if (got > 0) {
            GARANTE(got);
            if (got <= cap - n) {
              CatFileira *f = &fil[nFil++];
              memset(f, 0, sizeof *f);
              snprintf(f->chave,  sizeof f->chave,  "%s", dh->chave);
              snprintf(f->titulo, sizeof f->titulo, "%s", dh->titulo);
              snprintf(f->tipo,   sizeof f->tipo,   "%s", dh->tipo);
              snprintf(f->base,   sizeof f->base,   "%s", dh->base);
              snprintf(f->catId,  sizeof f->catId,  "%s", dh->id);
              memcpy(lote + n, hItens, sizeof(CatItem) * (size_t)got);
              f->ini = n; f->n = got;
              n += got;
              snprintf(heroAlimChave, sizeof heroAlimChave, "%s", dh->chave);
            }
          }
        }
      }
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
      if (CONDENADA("depois dos catalogos")) goto condenada;
    }
  }

  // Watchlist e colecao entram DEPOIS das recomendacoes, e nao antes.
  // A home usa as PRIMEIRAS posicoes do catalogo nas suas fileiras; com as
  // listas do Trakt na frente (e elas passam de 60 itens cada) as fileiras
  // viravam a watchlist inteira e as recomendacoes nunca apareciam. A
  // biblioteca varre o catalogo todo procurando as marcas, entao para ela
  // tanto faz onde estao.
  //
  // Buscadas no COMECO da volta por fioListas (ver ListasTrakt); aqui so se
  // espera o que ainda nao chegou e se copia para o mesmo lugar de sempre.
  // Sem o fio (pthread_create falhou), busca aqui mesmo, como sempre foi.
  if (listas) {
    while (!listasProntas(listas)) SDL_Delay(10);
    if (listas->nWl) {
      GARANTE(listas->nWl);
      if (listas->nWl <= cap - n) {
        memcpy(lote + n, listas->wl, sizeof(CatItem) * (size_t)listas->nWl);
        n += listas->nWl;
      }
    }
    if (listas->nCol) {
      GARANTE(listas->nCol);
      if (listas->nCol <= cap - n) {
        memcpy(lote + n, listas->col, sizeof(CatItem) * (size_t)listas->nCol);
        n += listas->nCol;
      }
    }
    listasLiberar(listas);
    listas = NULL;
  } else {
    GARANTE(TRAKT_LISTA_MAX);
    n += trakt_lista("watchlist",  lote + n, cap - n);
    GARANTE(TRAKT_LISTA_MAX);
    n += trakt_lista("collection", lote + n, cap - n);
  }
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
  // PERSONAL SERVER ROWS (jellyfin.h). Copied from the module's snapshot, no
  // network here; the module fetched them on its own worker. Fixed rows
  // (empty base) so they never take an addon row's place in the limit, placed
  // right after the leading fixed rows (Continue watching, social).
  if (ajustes_jellyfin_ligado() && servidores_conectado()) {
    int cabe = SRV_ITENS_MAX, jn = 0, jnf, k, pos = 0;
    CatItem *jfItens = malloc(sizeof(CatItem) * (size_t)cabe);
    CatFileira jfFils[SRV_FIL_MAX];
    jnf = jfItens ? servidores_fileiras_copiar(jfItens, cabe, jfFils, SRV_FIL_MAX, &jn) : 0;
    for (k = 0; k < nFilsLote; k++)
      if (servidores_chave_fileira(filsLote[k].chave)) { jnf = 0; break; }
    if (jnf > 0 && nFilsLote + jnf <= CAT_FIL_MAX) {
      GARANTE(jn);
      if (jn <= cap - n) {
        memcpy(lote + n, jfItens, sizeof(CatItem) * (size_t)jn);
        for (k = 0; k < jnf; k++) jfFils[k].ini += n;
        n += jn;
        while (pos < nFilsLote && !filsLote[pos].base[0]) pos++;
        memmove(filsLote + pos + jnf, filsLote + pos, sizeof(CatFileira) * (size_t)(nFilsLote - pos));
        memcpy(filsLote + pos, jfFils, sizeof(CatFileira) * (size_t)jnf);
        nFilsLote += jnf;
        printf("[desc] %d personal-server row(s)\n", jnf);
      }
    }
    free(jfItens);
  }
#undef GARANTE

  if (n || nFilsLote > 0) {
    // SO PUBLICA SE MUDOU. A mesma home montada de novo (ciclo de 5 min, sync
    // sem novidade) tem a mesma assinatura que a da tela; republicar seria
    // trocar o vetor, subir a revisao e a home se remontar — foco, rolagem e
    // arte de volta a zero — para mostrar exatamente o que ja mostrava.
    unsigned long antes = cat_assinatura();
    unsigned long depois = cat_assinatura_de(lote, n, filsLote, nFilsLote);
    // O QUE MUDOU DESDE QUE ESTA VOLTA COMECOU, por parte. So a FONTE (dono,
    // perfil, idioma, addons) descarta; ver o bloco da estrutura mais abaixo.
    unsigned estadoFim = homeestado_geracao();
    HomeContexto ctxFim;
    int mudou;
    homeestado_contexto(&ctxFim);
    mudou = homeestado_mudancas(&ctxIni, &ctxFim);
    if (minhaGeracao != montagemGeracao || (mudou & HOMEESTADO_MUDOU_FONTE)) {
      // DESCARTADA SEM PUBLICAR, e por isso RECOMECA. A geracao muda sozinha
      // no arranque (perfil escolhido, catordem/colecoes/addons da conta
      // chegando — tudo entra na assinatura do homeestado), e no log da LG as
      // DUAS montagens da sessao morreram aqui: sem recomecar, ninguem mais
      // montava e a home ficava com o pacote. O pedido de desc_repetir que
      // trocou montagemGeracao tambem passava por aqui e se perdia
      // (repetirAoFim nunca era lido). A volta nova le o contexto atual; ela
      // so descarta de novo se o contexto mudar DE NOVO, entao nao ha laco.
      //
      // DESDE A 1.4.5 SO A FONTE DESCARTA, e a linha diz qual das duas causas
      // foi: um pedido de remontagem (desc_repetir: credencial, addons, idioma)
      // ou a fonte vista pelo homeestado. Colecoes, ordem e limite chegando no
      // meio nao passam mais por aqui.
      { char txt[96];
        if (minhaGeracao != montagemGeracao)
          printf("[desc] montagem descartada: remontagem pedida no meio "
                 "(credencial/addons/idioma); recomecando\n");
        else
          printf("[desc] montagem descartada: %s mudou no meio; recomecando\n",
                 homeestado_mudancas_texto(mudou & HOMEESTADO_MUDOU_FONTE,
                                           txt, sizeof txt)); }
      fflush(stdout);
      free(lote); repetirAoFim = 0; buscandoSoltar();
      // Recomeca JA, sem o minimo entre voltas: a fonte (conta/perfil/addons)
      // mudou ou a pessoa pediu, e a volta so e condenada quando o minimo ja
      // passou (repetirInterno). Esperar 10 s aqui deixava a Home com o pacote
      // ou com o perfil que saiu.
      desc_iniciar();
      return NULL;
    }
    // A tela passa a ter a volta COMPLETA (publicada agora, ou igual a ela).
    parcialNaTela = 0;
    // A partir daqui filsMontadas descreve o bloco da tela nos dois ramos:
    // publicado agora, ou igual (mesma assinatura) ao que ja estava.
    nFileirasMontadas = nFilsLote;
    memcpy(filsMontadas, filsLote, sizeof(CatFileira) * (size_t)nFilsLote);
    if (depois != antes || cat_do_cache()) {
      publicarMontagem(lote, n, filsMontadas, nFileirasMontadas);
      marco("catalogo da rede publicado");
      printf("[desc] catalogo montado com %d titulos\n", n);
    } else {
      marco("catalogo da rede igual ao da tela: nao republicado");
      printf("[desc] catalogo montado com %d titulos, igual ao que esta na tela; mantido\n", n);
    }
    cat_cache_substituido();
    localizarContinuarPublicado();
    if (!(mudou & HOMEESTADO_MUDOU_ESTRUTURA))
      salvarSnapshot(filsMontadas, nFileirasMontadas, estadoFim);

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
    if (!(mudou & HOMEESTADO_MUDOU_ESTRUTURA)) {
      if (col_n() > 0) desc_remontar_fileiras();
    } else {
      // A ESTRUTURA MUDOU NO MEIO, E O QUE CHEGOU CONTINUA VALENDO.
      //
      // Ate a 1.4.4 isto era "montagem descartada" e uma volta inteira de
      // rede. MEDIDO no log do @rawldon (Tizen 6, 241 colecoes): as colecoes
      // da conta chegaram aos 29,6 s, a montagem ja tinha buscado 16 de 16
      // fileiras aos 48,8 s e jogou tudo fora; a home da conta so apareceu aos
      // 85 s, e o cache dessa segunda volta tambem foi recusado.
      //
      // Os itens nao dependem da estrutura — so a escolha e a arrumacao das
      // fileiras dependem. Entao: publica (acima), rearruma sem rede, e so
      // pede rede se a estrutura nova quiser um catalogo que esta volta nao
      // buscou. Mesmo nesse caso o que chegou fica na tela e no cache.
      char txt[96], qual[96];
      printf("[desc] estrutura mudou durante a montagem (%s): o que chegou "
             "vale; remontando sem rede\n",
             homeestado_mudancas_texto(mudou & HOMEESTADO_MUDOU_ESTRUTURA,
                                       txt, sizeof txt));
      fflush(stdout);
      desc_remontar_fileiras();
      if (estruturaNovaPedeRede(filsLote, nFilsLote, qual, sizeof qual)) {
        printf("[desc] a estrutura nova pede catalogo que esta volta nao "
               "buscou (%s): ciclo de rede depois deste\n", qual);
        fflush(stdout);
        desc_repetir_silencioso();
      }
      // Com ciclo de rede pedido (aqui ou pela propria remontagem, quando a
      // colecao engoliu fileiras e sobrou catalogo fora do teto) o snapshot
      // NAO e gravado: ele filtraria a proxima montagem para as chaves de
      // agora, e a que falta nunca seria pedida.
      if (!repetirAoFim) {
        // Snapshot sob a estrutura FINAL, e com a geracao lida AGORA: a de
        // antes da remontagem ja nao e a corrente.
        HomeContexto ctxSalvo;
        unsigned estadoSalvo = homeestado_geracao();
        homeestado_contexto(&ctxSalvo);
        if (!(homeestado_mudancas(&ctxIni, &ctxSalvo) & HOMEESTADO_MUDOU_FONTE))
          salvarSnapshot(filsMontadas, nFileirasMontadas, estadoSalvo);
      }
    }
    // Grava so o resultado COMPLETO, nao as publicacoes parciais: um cache
    // com tres fileiras faria a proxima abertura nascer pela metade e so
    // completar quando a rede respondesse — exatamente o que o cache existe
    // para evitar.
    //
    // O CACHE SO PERGUNTA PELA FONTE. Ele guarda o bloco da tela como esta —
    // ja rearrumado acima — e cat_ler_cache so confere dono, perfil e idioma;
    // estrutura nao e dele. Exigir a geracao inteira do homeestado, como ate a
    // 1.4.4, recusava o cache por qualquer mudanca de estrutura entre o inicio
    // e o fim — no log do @rawldon, 0,9 s depois de a home publicar e
    // registrar as fileiras de colecao (candidato provavel, nao medido) — e o
    // arranque seguinte nascia sem cache.
    // Dono e perfil sao os do INICIO desta volta, e cat_gravar_cache_se_
    // identidade os confere contra os de agora antes e depois de escrever.
    { HomeContexto ctxCache;
      homeestado_contexto(&ctxCache);
      if (!(homeestado_mudancas(&ctxIni, &ctxCache) & HOMEESTADO_MUDOU_FONTE))
        cat_gravar_cache_se_identidade(dirArteDesc, donoIni, ctxIni.perfil);
      else
        printf("[desc] cache descartado: conta/perfil/addons mudou durante a montagem\n");
    }
  } else {
    printf("[desc] nada veio da rede; segue o catalogo do pacote\n");
  }
  fflush(stdout);
  free(lote);
  listasAbandonar(listas);   // so sobra se o lote veio vazio antes de usa-las
  buscandoSoltar();
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
      descIniciarAdiado();
  }
  return NULL;

condenada:
  // VOLTA CONDENADA: ver CONDENADA no comeco. Nada desta volta foi publicado
  // desde que ela foi condenada (as publicacoes em partes conferem a geracao),
  // e a volta nova le credencial, idioma e lista de addons de agora. O pedido
  // que a condenou e atendido por ela, entao a marca de "repetir no fim" cai.
  printf("[desc] montagem interrompida (%s): remontagem pedida "
         "(credencial/addons/idioma); recomecando ja\n", ondeParou);
  fflush(stdout);
  listasAbandonar(listas);
  free(lote);
  repetirAoFim = 0;
  buscandoSoltar();
  desc_iniciar();   // condenada so passa do minimo entre voltas (ver acima)
  return NULL;
#undef CONDENADA
#undef LISTAS_SE_PRONTAS
}

// INICIO DE VOLTA ATOMICO. O teste de `buscando` e a marca ficam sob
// listaTrava, na mesma secao: antes o teste vinha SEM trava e a marca so
// depois, e dois chamadores (o fio adiado acordando, a volta condenada
// recomecando e o quadro) passavam juntos e largavam dois montar(). Nada de rede
// sob a trava; pthread_create fica fora dela (montar toma listaTrava logo).
// `conferirVoltas`: so inicia se nenhuma volta comecou desde `voltasAoAgendar`
// (o fio adiado). `repetirSeOcupado`: com volta no ar, ela repete ao fim em vez
// de o pedido ser ignorado. tests/desc_iniciar_corrida.sh.
enum { DESC_INICIOU, DESC_OCUPADO, DESC_ATENDIDO };
static int descIniciarTravado(int conferirVoltas, unsigned voltasAoAgendar,
                              int repetirSeOcupado) {
  pthread_t th;
  unsigned long long agora = descAgoraMs();
  pthread_mutex_lock(&listaTrava);
  if (conferirVoltas && descVoltas != voltasAoAgendar) {
    pthread_mutex_unlock(&listaTrava);
    return DESC_ATENDIDO;   // uma volta ja comecou depois do pedido
  }
  if (buscando) {
    if (repetirSeOcupado) { descDeb.adiado = 0; repetirAoFim = 1; }   // a volta no ar repete ao fim
    pthread_mutex_unlock(&listaTrava);
    if (!repetirSeOcupado) { printf("[desc] ja montando; pedido ignorado\n"); fflush(stdout); }
    return DESC_OCUPADO;
  }
  buscando = 1;
  listaLidaNaVolta = 0;
  nv_desc_iniciou(&descDeb, agora);
  descVoltas++;
  descPedidoPessoa = 0;   // esta volta atende o pedido da pessoa
  montagemGeracao++;
  pthread_mutex_unlock(&listaTrava);
  if (pthread_create(&th, NULL, montar, NULL) != 0) {
    // NAO FALHAR CALADO. No webOS um pthread_create nunca falhou e o caminho de
    // erro era so uma bandeira; no alvo WASM o fio e um Worker do navegador,
    // que E um recurso limitado e PODE acabar. Quando acaba, a home fica com o
    // catalogo do pacote para sempre e nao ha uma linha no log dizendo por que.
    buscandoSoltar();
    printf("[desc] pthread_create FALHOU: o catalogo nao vai remontar\n");
    fflush(stdout);
  }
  else pthread_detach(th);
  return DESC_INICIOU;
}
void desc_iniciar(void) { (void)descIniciarTravado(0, 0, 0); }

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
    if (heroAlimChave[0] && !strcmp(f->chave, heroAlimChave) &&
        fil_estado_chave(f->chave) != FIL_NA_HOME) continue;   // #327: volta no fim
    if (fil_oculta(f->chave) || catordem_oculta(f->chave, f->chave)) continue;
    if (dentroDeColecaoVisivelBase(f->base, f->tipo, f->catId) &&
        !fil_adicionada_na_tv(f->chave)) { engolidas++; continue; }
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

  teto = fil_limite() + nOut;
  if (teto < 1 || teto > CAT_FIL_MAX) teto = CAT_FIL_MAX;
  for (k = 0; k < nCat && nOut < teto && nOut < CAT_FIL_MAX; k++)
    saidaFil[nOut++] = filsMontadas[idxCat[ordem[k]]];

  // #327: a fileira so do destaque fica, fora da conta do limite.
  if (heroAlimChave[0] && nOut < CAT_FIL_MAX) {
    int ja = 0;
    for (i = 0; i < nOut && !ja; i++) ja = !strcmp(saidaFil[i].chave, heroAlimChave);
    for (i = 0; !ja && i < nFileirasMontadas; i++)
      if (!strcmp(filsMontadas[i].chave, heroAlimChave)) { saidaFil[nOut++] = filsMontadas[i]; break; }
  }
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
    desc_repetir_silencioso();
  }
}

static void repetirInterno(void);
void desc_repetir(void) {
  montSilenciosa = 0;
  descPedidoPessoa = 1;
  repetirInterno();
}
// Mesmo pedido, sem acender o alerta da ilha: sync, remontagem que a propria
// descoberta pede, lista de addons que mudou sozinha.
void desc_repetir_silencioso(void) {
  if (!buscando) montSilenciosa = 1;
  repetirInterno();
}
// O fio adiado leva no argumento o que precisa (quanto dormir e a marca de
// voltas do AGENDAMENTO): ler descVoltasAoAgendar global depois de dormir lia a
// marca de um agendamento POSTERIOR e largava volta antes do minimo.
typedef struct { unsigned ms, voltas; } DescAdiado;
static void *adiadoFio(void *arg) {
  DescAdiado a = *(DescAdiado *)arg;
  struct timespec t = { a.ms / 1000u, (long)(a.ms % 1000u) * 1000000L };
  free(arg);
  nanosleep(&t, NULL);
  (void)descIniciarTravado(1, a.voltas, 1);
  return NULL;
}
// Inicia uma volta respeitando o minimo entre voltas; pedidos no intervalo
// viram UM agendamento. Nunca bloqueia o chamador.
static void descIniciarAdiado(void) {
  long w;
  pthread_t th;
  DescAdiado *a;
  unsigned voltas;
  if (descPedidoPessoa) { descPedidoPessoa = 0; desc_iniciar(); return; }
  pthread_mutex_lock(&listaTrava);
  w = nv_desc_pedido(&descDeb, descAgoraMs());
  voltas = descVoltas;
  pthread_mutex_unlock(&listaTrava);
  if (w == 0) { desc_iniciar(); return; }
  if (w < 0) {
    printf("[desc] remontagem coalescida: ja ha uma agendada (minimo %llu s entre voltas)\n",
           NV_DESC_MIN_MS / 1000ull);
    fflush(stdout); return;
  }
  printf("[desc] remontagem adiada %ld ms (minimo %llu s entre voltas)\n", w, NV_DESC_MIN_MS / 1000ull);
  fflush(stdout);
  a = malloc(sizeof *a);
  if (a) { a->ms = (unsigned)w; a->voltas = voltas; }
  if (!a || pthread_create(&th, NULL, adiadoFio, a) != 0) {
    free(a);
    pthread_mutex_lock(&listaTrava); descDeb.adiado = 0; pthread_mutex_unlock(&listaTrava);
    desc_iniciar(); return;
  }
  pthread_detach(th);
}
static void repetirInterno(void) {
  geracaoPedida++;
  if (!buscando) { montagemGeracao++; descIniciarAdiado(); return; }
  repetirAoFim = 1;
  // So condena a volta no ar se ela ja rodou o minimo: antes disso o pedido
  // espera o fim dela (que publica o que ja tem) e vira UMA volta nova. O
  // pedido da pessoa condena sempre (descPedidoPessoa).
  if (!descPedidoPessoa && !nv_desc_pode_condenar(&descDeb, descAgoraMs())) {
    printf("[desc] remontagem pedida; volta em curso e recente: termina e repete uma vez\n");
    fflush(stdout);
    return;
  }
  montagemGeracao++;
  printf("[desc] remontagem pedida; a volta em curso para no proximo ponto seguro e recomeca\n");
  fflush(stdout);
}

// #319: A LISTA DE ADDONS MUDOU (removido na conta, desligado na TV): AS FILEIRAS
// DELES SAEM DA TELA NA HORA, sem esperar o ciclo de rede (~20 s na TV, e a
// Home de uma abertura vinda do cache — catalogo-rede.bin — tem as fileiras do
// conjunto antigo ate la). So fileira de catalogo de addon (base != "") e
// candidata; as fixas (continuar, social, servidores pessoais) nao tem base.
// Casa pela base, a mesma que a montagem gravou em CatFileira.base. Sem lista
// (nenhum addon conhecido) nada sai: lista vazia e "ainda nao sei", nao "tirei
// todos". O ciclo que vem depois publica o conjunto certo por cima.
static int baseDeAddonLigado(const char *base) {
  int i, n = addons_n();
  for (i = 0; i < n; i++)
    if (addons_ativo(i) && !strcmp(addons_base(i), base)) return 1;
  return 0;
}
int desc_tirar_fileiras_de_addons_ausentes(void) {
  static CatFileira publicadas[CAT_FIL_MAX], restam[CAT_FIL_MAX];
  int i, np = 0, nr = 0, tirou = 0;
  if (addons_n() < 1) return 0;
  { int n = cat_n_fileiras();
    for (i = 0; i < n && np < CAT_FIL_MAX; i++) {
      const CatFileira *f = cat_fileira(i);
      if (f) publicadas[np++] = *f;
    } }
  for (i = 0; i < np; i++) {
    if (publicadas[i].base[0] && !baseDeAddonLigado(publicadas[i].base)) { tirou++; continue; }
    restam[nr++] = publicadas[i];
  }
  if (!tirou) return 0;
  // A volta em curso e dona de filsMontadas: mexer nele aqui corre contra ela, e
  // ela termina publicando o conjunto novo de qualquer jeito.
  if (!buscando) {
    int w = 0;
    for (i = 0; i < nFileirasMontadas; i++)
      if (!filsMontadas[i].base[0] || baseDeAddonLigado(filsMontadas[i].base))
        filsMontadas[w++] = filsMontadas[i];
    nFileirasMontadas = w;
  }
  cat_republicar_fileiras(restam, nr);
  printf("[desc] %d fileira(s) de addon removido/desligado sairam da tela na hora\n", tirou);
  fflush(stdout);
  return tirou;
}

// Ver descoberta.h e listaLidaNaVolta. So a LISTA mudou: a volta que ainda nao
// a leu vai ler a nova, e o Trakt que ela ja buscou continua valendo.
void desc_repetir_addons(void) {
  int atendido;
  desc_tirar_fileiras_de_addons_ausentes();
  pthread_mutex_lock(&listaTrava);
  atendido = buscando && !listaLidaNaVolta;
  if (atendido) { geracaoPedida++; repetirAoFim = 1; }
  pthread_mutex_unlock(&listaTrava);
  if (!atendido) { desc_repetir_silencioso(); return; }
  printf("[desc] addons novos: a volta em curso ainda nao leu a lista; atendido por ela\n");
  fflush(stdout);
}

// #294: TROCA DE PERFIL. O bloco publicado e as janelas montadas (filsMontadas)
// sao do perfil que saiu: ate a descoberta do novo terminar, a Home mostrava as
// fileiras do outro perfil (logs 2KGKSE/DSWQTX: "19 fileiras na tela, 40 no
// catalogo" logo apos "perfil ativo: 1") e a tela registrava essas chaves no
// arquivo de fileiras do perfil novo. Esvazia os dois; a volta que roda a
// seguir publica o catalogo certo. catalogo-rede.bin ja e por usuario+perfil
// (CacheCab) e recusado quando nao bate.
void desc_esquecer_catalogo_perfil(void) {
  // Com volta em curso ela e dona de filsMontadas e publica o conjunto novo.
  if (!buscando) nFileirasMontadas = 0;
  cat_definir_tudo(NULL, 0, NULL, 0);
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
//
// 12 e nao 4 desde que a ficha "catalogo primeiro" (metaCatalogo) guarda ate
// quatro respostas por titulo aberto (addon de origem, outro addon, Cinemeta,
// ARM): com 4 lugares um titulo sozinho levava o cache inteiro. O id ficou em
// 96 porque "xperience:<id longo>" passava dos 40 e dois ids com o mesmo comeco
// dividiriam a chave.
#define META_CACHE_N 12
static struct { char id[96]; char *corpo; unsigned uso; } metaCache[META_CACHE_N];   // "series/tt..."
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

// --- TEXTO LOCALIZADO (#176) ---------------------------------------------------
//
// O Cinemeta, o Trakt e o Simkl so falam ingles. Quem tem um addon de metadados
// localizado e/ou o TMDB num idioma que nao e o ingles pedia texto localizado e
// via ingles: o ajuste "Prefere a ficha do addon de metadados"
// (ajustes_meta_externo) nao era lido por ninguem, e o Continuar assistindo
// nunca perguntava a fonte nenhuma alem do Trakt.

// O idioma do TMDB configurado nao e o ingles?
static int idiomaNaoIngles(void) {
  const char *l = desc_tmdb_idioma();
  return l && l[0] && strncmp(l, "en", 2) != 0;
}

// FALHA LEMBRADA. Um addon que nao respondeu (ou respondeu 404) a este /meta nao
// e perguntado de novo por 5 min: a ficha do titulo consulta a mesma fonte em
// mais de um lugar (texto, episodios, generos) e um addon lento custava o
// timeout INTEIRO a cada vez — o detalhe parava de responder por ele.
#define META_NEG_N 24
static struct { char chave[96]; long quando; } metaNeg[META_NEG_N];
static int metaNegProx;

static int metaNegAtiva(const char *chave) {
  int i, r = 0;
  pthread_mutex_lock(&metaTrava);
  for (i = 0; i < META_NEG_N; i++)
    if (metaNeg[i].chave[0] && !strcmp(metaNeg[i].chave, chave) &&
        time(NULL) - metaNeg[i].quando < 300) { r = 1; break; }
  pthread_mutex_unlock(&metaTrava);
  return r;
}
static void metaNegGuardar(const char *chave) {
  pthread_mutex_lock(&metaTrava);
  snprintf(metaNeg[metaNegProx].chave, sizeof metaNeg[0].chave, "%s", chave);
  metaNeg[metaNegProx].quando = (long)time(NULL);
  metaNegProx = (metaNegProx + 1) % META_NEG_N;
  pthread_mutex_unlock(&metaTrava);
}
__attribute__((unused))   // so o teste limpa (tests/detalheanime.c)
static void metaNegLimpar(void) {
  pthread_mutex_lock(&metaTrava);
  memset(metaNeg, 0, sizeof metaNeg);
  pthread_mutex_unlock(&metaTrava);
}

// /meta/<tipo>/<id>.json do addon `i` (cache por addon+tipo+id), ou NULL quando
// o addon nao serve (desligado, sem o recurso "meta", e o Cinemeta, que o
// chamador ja tem) ou nao respondeu. A URL carrega credencial: nada aqui a loga.
// `prazo` em segundos: o texto localizado espera 15; a ficha do catalogo 8, para
// um addon lento nao segurar a pagina.
static char *metaDoAddonT(int i, const char *tipo, const char *id, int prazo) {
  char url[NV_ADDON_PEDIDO_MAX], chave[96], idUrl[768], *c;
  const char *base;
  unsigned h;
  if (!addons_ativo(i) || !addons_sondado(i) || !addons_fornece(i, ADD_META)) return NULL;
  if (!nv_addon_id(idUrl, sizeof idUrl, id) || !idUrl[0]) return NULL;
  base = addons_base(i);
  if (!base || !base[0] || strstr(base, "cinemeta")) return NULL;
  h = hashBaseAddon(base);
  snprintf(chave, sizeof chave, "%08x/%s/%s", h, tipo, id);
  c = metaCacheObter(chave);
  if (c) return c;
  if (metaNegAtiva(chave)) return NULL;
  if (!descPedidoCoube(base, nv_addon_url(url, sizeof url, base, "/meta/%s/%s.json", tipo, idUrl),
                       sizeof url)) return NULL;
  c = rede_baixar(url, prazo);
  if (c) metaCacheGuardar(chave, c);
  else metaNegGuardar(chave);
  return c;
}
static char *metaDoAddon(int i, const char *tipo, const char *id) {
  return metaDoAddonT(i, tipo, id, 15);
}

// Nome e descricao da RAIZ do objeto "meta" (js_texto acharia o "name" de um
// video). 1 quando ha nome; "meta":null e resposta de erro dao 0.
static int metaTextos(const char *corpo, char *tit, size_t nt, char *sin, size_t ns) {
  const char *m = corpo ? strstr(corpo, "\"meta\"") : NULL;
  if (tit && nt) tit[0] = 0;
  if (sin && ns) sin[0] = 0;
  if (!m) return 0;
  m += 6;
  while (*m == ' ' || *m == ':' || *m == '\n' || *m == '\t' || *m == '\r') m++;
  if (*m != '{') return 0;
  if (!js_texto_raiz_em(m, NULL, "name", tit, nt) || !tit[0]) return 0;
  if (sin && ns) js_texto_raiz_em(m, NULL, "description", sin, ns);
  return 1;
}

// Titulo e sinopse do primeiro addon de metadados ativo que conhece `id`.
static int textoDoAddon(const char *tipo, const char *id, char *tit, size_t nt,
                        char *sin, size_t ns, const char **nomeAddon) {
  int i, n = addons_n();
  for (i = 0; i < n; i++) {
    char *c = metaDoAddon(i, tipo, id);
    int ok;
    if (!c) continue;
    ok = metaTextos(c, tit, nt, sin, ns);
    free(c);
    if (ok) { if (nomeAddon) *nomeAddon = addons_nome(i); return 1; }
  }
  return 0;
}

// Titulo e sinopse do TMDB no idioma configurado: /find + /tv|movie/<id>.
// `logo` (pode ser NULL): com "Arte localizada" ligada, o logo do TMDB NO
// IDIOMA dos metadados, no mesmo pedido (append_to_response=images). So o do
// idioma: o sem idioma e o ingles sao o que o Continuar ja tem (metahub).
static int textoDoTmdb(const char *tipo, const char *id, char *tit, size_t nt,
                       char *sin, size_t ns, char *logo, size_t nl) {
  const char *chave = desc_chave_tmdb();
  int serie = !strcmp(tipo, "series");
  int querLogo = logo && nl && ajustes_tmdb_arte();
  char url[460], *c;
  long idT = 0;
  if (logo && nl) logo[0] = 0;
  if (!chave[0] || (!ajustes_tmdb_basico() && !querLogo)) return 0;
  snprintf(url, sizeof url, "%s/find/%s?api_key=%s&external_source=imdb_id",
           TMDB, id, chave);
  c = rede_baixar(url, 8);
  if (!c) return 0;
  { const char *p = js_array(c, NULL, serie ? "tv_results" : "movie_results");
    if (p) idT = (long)js_num(p, js_fim(p), "id", 0.0); }
  free(c);
  if (idT <= 0) return 0;
  { char incImg[80] = "";
    if (querLogo)
      snprintf(incImg, sizeof incImg,
               "&append_to_response=images&include_image_language=%.2s",
               desc_tmdb_idioma());
    snprintf(url, sizeof url, "%s/%s/%ld?api_key=%s&language=%s%s",
             TMDB, serie ? "tv" : "movie", idT, chave, desc_tmdb_idioma(), incImg); }
  c = rede_baixar(url, 8);
  if (!c) return 0;
  tit[0] = sin[0] = 0;
  if (ajustes_tmdb_basico()) {
    js_texto_raiz(c, serie ? "name" : "title", tit, nt);
    js_texto_raiz(c, "overview", sin, ns);
  }
  if (querLogo) {
    const char *im = strstr(c, "\"images\"");
    const char *imObj = im ? strchr(im, '{') : NULL;
    const char *imFim = imObj ? js_fim(imObj) : NULL;
    const char *p = (imObj && imFim) ? js_array(imObj, imFim, "logos") : NULL;
    char base[3];
    snprintf(base, sizeof base, "%.2s", desc_tmdb_idioma());
    while (p && !logo[0]) {
      const char *f = js_fim(p);
      char iso[8] = "", fp[160] = "";
      js_texto(p, f, "iso_639_1", iso, sizeof iso);
      js_texto(p, f, "file_path", fp, sizeof fp);
      if (fp[0] == '/' && !ehSvg(fp) && !strcmp(iso, base))
        snprintf(logo, nl, "https://image.tmdb.org/t/p/w500%s", fp);
      p = js_prox(f);
    }
  }
  free(c);
  return tit[0] || sin[0] || (logo && nl && logo[0]);
}

// Logo e fundo da RAIZ do meta do primeiro addon de metadados que conhece `id`
// (#213). E o addon que a pessoa escolheu com idioma (AIOMetadata com ru, por
// exemplo): o logo dele vem no idioma dela. O Continuar assistindo nasce do
// Trakt com arte do metahub/Cinemeta — o logo em ingles do destaque no
// arranque. Raiz e nao primeira chave: o _providerArt do AIOMetadata vem antes
// (#200). SVG nao decodifica e fica fora.
static int arteDoAddon(const char *tipo, const char *id, char *logo, size_t nl,
                       char *fundo, size_t nf) {
  int i, n = addons_n();
  logo[0] = fundo[0] = 0;
  for (i = 0; i < n; i++) {
    char *c = metaDoAddon(i, tipo, id);
    const char *m = c ? strstr(c, "\"meta\"") : NULL;
    if (m) {
      m += 6;
      while (*m == ' ' || *m == ':' || *m == '\n' || *m == '\t' || *m == '\r') m++;
      if (*m == '{') {
        urlArteInteira(m, NULL, "logo", 1, logo, nl, id);
        urlArteInteira(m, NULL, "background", 1, fundo, nf, id);
        if (ehSvg(logo) || strncmp(logo, "http", 4)) logo[0] = 0;
        if (ehSvg(fundo) || strncmp(fundo, "http", 4)) fundo[0] = 0;
      }
    }
    free(c);
    if (logo[0] || fundo[0]) return 1;
  }
  return 0;
}

// Cache do texto localizado: um pedido por titulo e por idioma, nao um por
// refazagem da fileira. Resposta negativa vale 10 min (falha de rede nao vira
// "sem traducao" para a sessao inteira).
//
// GUARDADO EM DISCO (#213). So na memoria, ele nascia vazio a cada abertura: o
// Continuar do Trakt saia com o texto do Cinemeta (ingles) e o destaque, que le
// o primeiro item, mostrava a sinopse em ingles ate a volta da rede trocar 1-2 s
// depois — medido no registro da G5 (ru): "[t] 2110 trakt continuar assistindo"
// com aplicarLocCache sem nada, e o texto russo so depois de fioLocalizar. Com o
// arquivo, o primeiro quadro ja e o do idioma; a entrada do disco serve para
// pintar, e a rede ainda refaz uma vez por sessao (`doDisco`) para nao congelar.
#define LOC_N 64
static struct { char chave[64]; char titulo[160]; char sinopse[900];
                char logo[512]; char fundo[512]; int ok, doDisco; long quando; }
  locCache[LOC_N];
static int locProx;
static pthread_mutex_t locTrava = PTHREAD_MUTEX_INITIALIZER;

static void locChave(char *dst, size_t n, const char *tipo, const char *id) {
  snprintf(dst, n, "%s/%s/%s/%d", tipo, id, desc_tmdb_idioma(), ajustes_meta_externo());
}

// 1 = achou entrada valida; *ok diz se ha texto. Entrada negativa vencida = 0.
// `disco` 0 recusa a entrada que so veio do arquivo (quem pergunta e a rede).
static int locLerEx(const char *chave, char *tit, size_t nt, char *sin, size_t ns,
                    char *logo, size_t nl, char *fundo, size_t nf, int *ok, int disco) {
  int i, achou = 0;
  pthread_mutex_lock(&locTrava);
  for (i = 0; i < LOC_N; i++) {
    if (!locCache[i].chave[0] || strcmp(locCache[i].chave, chave)) continue;
    if (!locCache[i].ok && time(NULL) - locCache[i].quando > 600) break;
    if (locCache[i].doDisco && !disco) break;
    *ok = locCache[i].ok;
    if (*ok) {
      snprintf(tit, nt, "%s", locCache[i].titulo);
      snprintf(sin, ns, "%s", locCache[i].sinopse);
      if (logo && nl) snprintf(logo, nl, "%s", locCache[i].logo);
      if (fundo && nf) snprintf(fundo, nf, "%s", locCache[i].fundo);
    }
    achou = 1;
    break;
  }
  pthread_mutex_unlock(&locTrava);
  return achou;
}

static void locGuardar(const char *chave, const char *tit, const char *sin,
                       const char *logo, const char *fundo, int ok) {
  int i, vaga = -1;
  pthread_mutex_lock(&locTrava);
  for (i = 0; i < LOC_N; i++)
    if (!strcmp(locCache[i].chave, chave)) { vaga = i; break; }
  if (vaga < 0) { vaga = locProx; locProx = (locProx + 1) % LOC_N; }
  snprintf(locCache[vaga].chave, sizeof locCache[vaga].chave, "%s", chave);
  snprintf(locCache[vaga].titulo, sizeof locCache[vaga].titulo, "%s", ok ? tit : "");
  snprintf(locCache[vaga].sinopse, sizeof locCache[vaga].sinopse, "%s", ok ? sin : "");
  snprintf(locCache[vaga].logo, sizeof locCache[vaga].logo, "%s", ok && logo ? logo : "");
  snprintf(locCache[vaga].fundo, sizeof locCache[vaga].fundo, "%s", ok && fundo ? fundo : "");
  locCache[vaga].ok = ok;
  locCache[vaga].doDisco = 0;
  locCache[vaga].quando = (long)time(NULL);
  pthread_mutex_unlock(&locTrava);
}

// --- O ARQUIVO (loc-texto.txt na pasta de dados) ----------------------------
// Uma linha por entrada com texto: chave, titulo, sinopse, logo, fundo, por
// TAB; TAB, quebra e barra escapados. A primeira linha e a identidade (conta e
// perfil), como no cache do catalogo: a url de arte de addon pode carregar
// configuracao, e arquivo de outra conta e descartado. Sem pasta gravavel
// (dados_dir vazio, os testes) nao ha arquivo e tudo segue so na memoria.
#ifdef __EMSCRIPTEN__
#include "dados.h"
#define LOC_FS_TRAVAR()   dados_fs_travar()
#define LOC_FS_LIBERAR()  dados_fs_liberar()
#define LOC_MARCAR_SUJO() dados_marcar_sujo(1)
#else
#define LOC_FS_TRAVAR()   ((void)0)
#define LOC_FS_LIBERAR()  ((void)0)
#define LOC_MARCAR_SUJO() ((void)0)
#endif
const char *dados_dir(void);
static int locDiscoLido;

static int locCaminho(char *dst, size_t n) {
  const char *d = dados_dir();
  if (!d || !d[0]) return 0;
  snprintf(dst, n, "%s/loc-texto.txt", d);
  return 1;
}

static void locEscapar(FILE *f, const char *s) {
  for (; *s; s++) {
    if (*s == '\t') fputs("\\t", f);
    else if (*s == '\n') fputs("\\n", f);
    else if (*s == '\r') continue;
    else if (*s == '\\') fputs("\\\\", f);
    else fputc(*s, f);
  }
}

// Corta o campo em `p` ate o proximo TAB, desescapando. Devolve o resto.
static char *locCampo(char *p, char *dst, size_t n) {
  size_t k = 0;
  while (*p && *p != '\t' && *p != '\n') {
    char c = *p++;
    if (c == '\\' && *p) { c = *p++; c = c == 't' ? '\t' : c == 'n' ? '\n' : c; }
    if (k + 1 < n) dst[k++] = c;
  }
  if (n) dst[k] = 0;
  return *p == '\t' ? p + 1 : p;
}

static void locIdentidade(char *dst, size_t n) {
  const char *u = sessao_usuario();
  snprintf(dst, n, "loc1 %s %d", u ? u : "", perfis_ativo());
}

static void locSalvarDisco(void) {
  char caminho[600], tmp[620], ident[128];
  FILE *f;
  int i, n = 0;
  if (!locCaminho(caminho, sizeof caminho)) return;
  snprintf(tmp, sizeof tmp, "%s.tmp", caminho);
  locIdentidade(ident, sizeof ident);
  LOC_FS_TRAVAR();
  f = fopen(tmp, "w");
  if (!f) { LOC_FS_LIBERAR(); return; }
  fprintf(f, "%s\n", ident);
  pthread_mutex_lock(&locTrava);
  for (i = 0; i < LOC_N; i++) {
    if (!locCache[i].chave[0] || !locCache[i].ok) continue;
    locEscapar(f, locCache[i].chave);   fputc('\t', f);
    locEscapar(f, locCache[i].titulo);  fputc('\t', f);
    locEscapar(f, locCache[i].sinopse); fputc('\t', f);
    locEscapar(f, locCache[i].logo);    fputc('\t', f);
    locEscapar(f, locCache[i].fundo);   fputc('\n', f);
    n++;
  }
  pthread_mutex_unlock(&locTrava);
  if (fclose(f) != 0 || rename(tmp, caminho) != 0) remove(tmp);
  LOC_FS_LIBERAR();
  LOC_MARCAR_SUJO();
  printf("[desc] texto localizado gravado: %d titulo(s)\n", n);
  fflush(stdout);
}

// Uma vez por processo, no primeiro uso. Nao pisa entrada que a rede ja deu.
static void locLerDisco(void) {
  char caminho[600], ident[128], linha[2400];
  FILE *f;
  int n = 0;
  if (__atomic_exchange_n(&locDiscoLido, 1, __ATOMIC_ACQ_REL)) return;
  if (!locCaminho(caminho, sizeof caminho)) return;
  LOC_FS_TRAVAR();
  f = fopen(caminho, "r");
  if (!f) { LOC_FS_LIBERAR(); return; }
  locIdentidade(ident, sizeof ident);
  if (!fgets(linha, sizeof linha, f) || strncmp(linha, ident, strlen(ident)) ||
      (linha[strlen(ident)] != '\n' && linha[strlen(ident)] != 0)) {
    fclose(f);
    remove(caminho);
    LOC_FS_LIBERAR();
    printf("[desc] texto localizado do disco descartado (outra conta/perfil)\n");
    fflush(stdout);
    return;
  }
  pthread_mutex_lock(&locTrava);
  while (fgets(linha, sizeof linha, f) && n < LOC_N) {
    char chave[64], *p = linha;
    int i, vaga = -1;
    p = locCampo(p, chave, sizeof chave);
    if (!chave[0]) continue;
    for (i = 0; i < LOC_N; i++)
      if (!strcmp(locCache[i].chave, chave)) { vaga = -2; break; }
    if (vaga == -2) continue;
    vaga = locProx; locProx = (locProx + 1) % LOC_N;
    snprintf(locCache[vaga].chave, sizeof locCache[vaga].chave, "%s", chave);
    p = locCampo(p, locCache[vaga].titulo, sizeof locCache[vaga].titulo);
    p = locCampo(p, locCache[vaga].sinopse, sizeof locCache[vaga].sinopse);
    p = locCampo(p, locCache[vaga].logo, sizeof locCache[vaga].logo);
    locCampo(p, locCache[vaga].fundo, sizeof locCache[vaga].fundo);
    locCache[vaga].ok = 1;
    locCache[vaga].doDisco = 1;
    locCache[vaga].quando = (long)time(NULL);
    n++;
  }
  pthread_mutex_unlock(&locTrava);
  fclose(f);
  LOC_FS_LIBERAR();
  printf("[desc] texto localizado do disco: %d titulo(s)\n", n);
  fflush(stdout);
}

void desc_loc_apagar(void) {
  char caminho[600];
  pthread_mutex_lock(&locTrava);
  memset(locCache, 0, sizeof locCache);
  locProx = 0;
  pthread_mutex_unlock(&locTrava);
  if (!locCaminho(caminho, sizeof caminho)) return;
  LOC_FS_TRAVAR();
  if (remove(caminho) == 0) LOC_MARCAR_SUJO();
  LOC_FS_LIBERAR();
}

// Id do titulo ("tt123" de "tt123:1:2") e tipo do meta ("movie"/"series") de um
// item. 0 quando nao ha o que perguntar (canal, id que nao e do IMDb).
static int locChaveDoItem(const CatItem *c, char *id, size_t nid, const char **tipo) {
  const char *dp;
  if (!c || strncmp(c->imdb, "tt", 2) || ehCanal(c->tipo)) return 0;
  snprintf(id, nid, "%s", c->imdb);
  dp = strchr(id, ':');
  if (dp) *(char *)dp = 0;
  *tipo = (!strcmp(c->tipo, "movie")) ? "movie" : "series";
  return 1;
}

// Resolve (com rede) titulo e sinopse localizados. Num idioma que nao e o
// ingles, o TMDB primeiro e o addon quando ele nao tem (#209). Em ingles, a
// ordem e a da preferencia:
// com "Prefere a ficha do addon de metadados" o addon vem primeiro; sem ele, o
// TMDB no idioma configurado, e o addon so quando o TMDB nao tem (ou esta
// desligado). Em ingles e sem a preferencia nao ha nada a localizar.
//
// A ARTE (#213) e outra regra: o addon de metadados PRIMEIRO (logo e fundo da
// raiz do meta dele), o logo do TMDB no idioma so na falta. O texto fica com a
// ordem do #209 porque ali o addon em ingles (Ultra MAX, Bingecat) e medido;
// um logo do addon em ingles nao piora nada — o do Continuar ja e o ingles do
// metahub.
static int localizarTexto(const char *tipo, const char *id, char *tit, size_t nt,
                          char *sin, size_t ns, char *logo, size_t nl,
                          char *fundo, size_t nf) {
  char chave[64], logoTmdb[512] = "";
  int ok = 0, externo = ajustes_meta_externo(), naoIng = idiomaNaoIngles(), lido;
  logo[0] = fundo[0] = 0;
  if (!externo && !naoIng) return 0;
  locLerDisco();
  locChave(chave, sizeof chave, tipo, id);
  if (locLerEx(chave, tit, nt, sin, ns, logo, nl, fundo, nf, &lido, 0)) return lido;
  tit[0] = sin[0] = 0;
  // Com a preferencia o addon vem primeiro SO em ingles (#209): num idioma
  // que nao e o ingles o TMDB traduz antes, como na ficha do titulo.
  if (externo && !naoIng) {
    ok = textoDoAddon(tipo, id, tit, nt, sin, ns, NULL);
  } else {
    ok = textoDoTmdb(tipo, id, tit, nt, sin, ns,
                     naoIng ? logoTmdb : NULL, sizeof logoTmdb);
    if (!tit[0] && !sin[0]) ok = textoDoAddon(tipo, id, tit, nt, sin, ns, NULL);
  }
  if (arteDoAddon(tipo, id, logo, nl, fundo, nf)) ok = 1;
  if (!logo[0] && logoTmdb[0]) { snprintf(logo, nl, "%s", logoTmdb); ok = 1; }
  locGuardar(chave, tit, sin, logo, fundo, ok);
  return ok;
}

// Aplica ao item o que o cache ja sabe (sem rede). Campo vazio nao apaga.
// A arte so troca a do proprio item: logo e fundo vao para `logo`/`backdrop`
// (o card deitado e o destaque leem dali); o fundo do addon tambem fica como
// backdropCatalogo, que e a "arte do catalogo" para a fonte escolhida.
// O que o cache sabe do item (sem rede); 0 = nada.
static int locValoresDoItem(const CatItem *c, char *tit, size_t nt, char *sin, size_t ns,
                            char *logo, size_t nl, char *fundo, size_t nf) {
  char id[24], chave[64];
  const char *tipo;
  int ok = 0;
  if (!locChaveDoItem(c, id, sizeof id, &tipo)) return 0;
  locLerDisco();
  locChave(chave, sizeof chave, tipo, id);
  return locLerEx(chave, tit, nt, sin, ns, logo, nl, fundo, nf, &ok, 1) && ok;
}
static int aplicarLocItem(CatItem *c) {
  char tit[160], sin[900], logo[512], fundo[512];
  int ok = 0;
  if (!locValoresDoItem(c, tit, sizeof tit, sin, sizeof sin, logo, sizeof logo,
                        fundo, sizeof fundo)) return 0;
  if (tit[0] && strcmp(tit, c->titulo)) { snprintf(c->titulo, sizeof c->titulo, "%s", tit); ok = 2; }
  if (sin[0] && strcmp(sin, c->sinopse)) { snprintf(c->sinopse, sizeof c->sinopse, "%s", sin); ok = 2; }
  if (logo[0] && strcmp(logo, c->logo)) { snprintf(c->logo, sizeof c->logo, "%s", logo); ok = 2; }
  if (fundo[0] && strcmp(fundo, c->backdrop)) {
    snprintf(c->backdrop, sizeof c->backdrop, "%s", fundo);
    snprintf(c->backdropCatalogo, sizeof c->backdropCatalogo, "%s", fundo);
    ok = 2;
  }
  return ok == 2;
}
static int aplicarLocCache(CatItem *v, int n) {
  int i, mudou = 0;
  if (!ajustes_meta_externo() && !idiomaNaoIngles()) return 0;
  for (i = 0; i < n; i++) mudou += aplicarLocItem(&v[i]);
  return mudou;
}

// O CATALOGO LIDO DO CACHE NO ARRANQUE (#213) passa pelo mesmo texto
// localizado do disco antes do primeiro quadro: o Continuar gravado la pode
// ter saido antes de a traducao chegar. Sem rede; so o fio principal, na
// abertura (home_iniciar), antes de a descoberta comecar a publicar.
int desc_localizar_catalogo_cache(void) {
  int i, n = cat_n(), mudou = 0;
  CatItem *e;
  if (!ajustes_meta_externo() && !idiomaNaoIngles()) return 0;
  e = malloc(sizeof *e);
  if (!e) return 0;
  for (i = 0; i < n; i++) {
    const CatItem *o = cat_item(i);
    if (!o) continue;
    *e = *o;
    if (aplicarLocItem(e)) { cat_atualizar_item(i, e); mudou++; }
  }
  free(e);
  if (mudou) {
    printf("[desc] cache do catalogo: %d titulo(s) no idioma ja no primeiro quadro\n", mudou);
    fflush(stdout);
  }
  return mudou;
}

// Um fio por vez; pedido que chega com o fio no ar troca a lista pendente e
// ganha uma volta a mais (so o ultimo estado interessa, como no refazer do CW).
#define LOC_LOTE 32
static int locIdx[LOC_LOTE], locNIdx;
static volatile int locVivo, locDeNovo;
static pthread_mutex_t locFilaTrava = PTHREAD_MUTEX_INITIALIZER;

static void *fioLocalizar(void *u) {
  int novos = 0;
  (void)u;
  for (;;) {
    int lista[LOC_LOTE], n, k;
    pthread_mutex_lock(&locFilaTrava);
    n = locNIdx;
    memcpy(lista, locIdx, sizeof(int) * (size_t)n);
    locDeNovo = 0;
    pthread_mutex_unlock(&locFilaTrava);
    for (k = 0; k < n; k++) {
      // Copia sob a trava do catalogo: este fio nao vira quadro, entao um
      // ponteiro de cat_item() aqui pode apontar para bloco ja liberado
      // (revisao 2.0.3, como fioSinopseHero; tests/localizar_corrida.sh).
      CatItem *e = malloc(sizeof *e);
      char id[24], imdb[64], tit[160], sin[900], logo[512], fundo[512];
      const char *tipo;
      if (!e) continue;
      if (!cat_copiar_item(lista[k], e) || !locChaveDoItem(e, id, sizeof id, &tipo)) {
        free(e); continue; }
      snprintf(imdb, sizeof imdb, "%s", e->imdb);
      if (!localizarTexto(tipo, id, tit, sizeof tit, sin, sizeof sin,
                          logo, sizeof logo, fundo, sizeof fundo)) { free(e); continue; }
      novos++;
      // O item pode ter mudado de lugar ou de texto enquanto a rede respondia:
      // so titulo, sinopse, logo e fundo entram, e so se o indice ainda e este
      // titulo. Nada mais do item e reescrito.
      if (locValoresDoItem(e, tit, sizeof tit, sin, sizeof sin, logo, sizeof logo,
                           fundo, sizeof fundo))
        cat_aplicar_localizado(lista[k], imdb, tit, sin, logo, fundo);
      free(e);
    }
    pthread_mutex_lock(&locFilaTrava);
    if (locDeNovo) { pthread_mutex_unlock(&locFilaTrava); continue; }
    pthread_mutex_unlock(&locFilaTrava);
    // Uma gravacao por lote, FORA da trava da fila e antes de soltar locVivo
    // (quem espera o fio — o teste — ve o arquivo pronto).
    if (novos) locSalvarDisco();
    pthread_mutex_lock(&locFilaTrava);
    if (locDeNovo) { pthread_mutex_unlock(&locFilaTrava); novos = 0; continue; }
    locVivo = 0;
    pthread_mutex_unlock(&locFilaTrava);
    return NULL;
  }
}

void desc_localizar_indices(const int *idx, int n) {
  pthread_t t;
  if (!idx || n < 1) return;
  if (!ajustes_meta_externo() && !idiomaNaoIngles()) return;
  if (n > LOC_LOTE) n = LOC_LOTE;
  pthread_mutex_lock(&locFilaTrava);
  memcpy(locIdx, idx, sizeof(int) * (size_t)n);
  locNIdx = n;
  if (locVivo) { locDeNovo = 1; pthread_mutex_unlock(&locFilaTrava); return; }
  locVivo = 1;
  pthread_mutex_unlock(&locFilaTrava);
  if (pthread_create(&t, NULL, fioLocalizar, NULL) != 0) locVivo = 0;
  else pthread_detach(t);
}

// A fileira "Continuar assistindo" ja esta na tela: pede o texto localizado dos
// cards. E isto que o destaque (hero) le quando o primeiro item do catalogo e o
// Continuar.
static void localizarContinuarPublicado(void) {
  int r, nf = cat_n_fileiras(), idx[LOC_LOTE], k, n = 0;
  if (!ajustes_meta_externo() && !idiomaNaoIngles()) return;
  for (r = 0; r < nf; r++) {
    const CatFileira *f = cat_fileira(r);
    if (!f || strcmp(f->chave, "continue_watching")) continue;
    for (k = 0; k < f->n && n < LOC_LOTE; k++) idx[n++] = f->ini + k;
    break;
  }
  desc_localizar_indices(idx, n);
}

// SINOPSE DOS CANDIDATOS DO DESTAQUE (08/10, LG C9 2.0.3). Com "hero *" o
// destaque sorteia ate 10 titulos de TODO o catalogo, e uns 28% dele sao itens
// RASOS (lista do Trakt: so nome e a arte sintetica do metahub, ver
// completarRaso). Eles entravam no destaque sem sinopse, porque o /meta so era
// pedido ao ABRIR o detalhe. Aqui o texto vem do MESMO /meta do detalhe
// (metaprov_meta: catalogo do Nuvio, Cinemeta so como reserva), em fio proprio,
// UM pedido por vez e com pausa entre eles — nunca uma rajada de 10.
//
// So a sinopse (e o nome, se o item veio sem) e escrita: a arte do item nao
// muda, senao o fundo do destaque trocaria debaixo de quem esta olhando.
// O resultado vai para o CATALOGO (cat_atualizar_item), entao o detalhe, que
// le o mesmo item, reaproveita sem pedir de novo. Titulo que nao conseguiu
// sinopse (rede, ficha sem descricao) NAO sai do destaque: aparece sem ela, e
// a falha vai para o log, com o id, para nao ser silencio.
#define HSIN_LOTE 10
#define HSIN_PAUSA_MS 150
#define HSIN_NEG_N 32
#define HSIN_NEG_S 600
static int hsIdx[HSIN_LOTE], hsNIdx;
static volatile int hsVivo, hsDeNovo;
static pthread_mutex_t hsTrava = PTHREAD_MUTEX_INITIALIZER;
// Falhas recentes: o conjunto do destaque e refeito a cada mudanca do catalogo,
// e sem esta memoria um titulo sem descricao viraria um pedido por refeita.
static struct { char imdb[24]; long ate; } hsNeg[HSIN_NEG_N];

static int hsNegativo(const char *imdb) {
  int i, r = 0;
  pthread_mutex_lock(&hsTrava);
  for (i = 0; i < HSIN_NEG_N; i++)
    if (hsNeg[i].imdb[0] && !strcmp(hsNeg[i].imdb, imdb) && hsNeg[i].ate > (long)time(NULL)) { r = 1; break; }
  pthread_mutex_unlock(&hsTrava);
  return r;
}
static void hsNegGuardar(const char *imdb) {
  static int prox;
  int i, vaga = -1;
  pthread_mutex_lock(&hsTrava);
  for (i = 0; i < HSIN_NEG_N; i++) if (!strcmp(hsNeg[i].imdb, imdb)) { vaga = i; break; }
  if (vaga < 0) { vaga = prox; prox = (prox + 1) % HSIN_NEG_N; }
  snprintf(hsNeg[vaga].imdb, sizeof hsNeg[vaga].imdb, "%s", imdb);
  hsNeg[vaga].ate = (long)time(NULL) + HSIN_NEG_S;
  pthread_mutex_unlock(&hsTrava);
}

// Preenche a sinopse de UM item raso. -1 = nada a fazer (ja tem sinopse, canal,
// id que nao e do IMDb); 1 = preencheu; 0 = tentou e nao veio texto.
// `get` e o provedor de rede (NULL = o de sempre); o teste passa o dele.
int desc_sinopse_completar_item(CatItem *c, MetaprovGet get, void *ctx) {
  char id[24], tit[160], sin[900];
  const char *tipo;
  char *corpo;
  int prov = -1;
  if (!c || c->sinopse[0] || !locChaveDoItem(c, id, sizeof id, &tipo)) return -1;
  corpo = get ? metaprov_meta_com(tipo, id, 10, get, ctx, &prov)
              : metaprov_meta(tipo, id, 10, &prov);
  tit[0] = sin[0] = 0;
  if (!corpo || !metaTextos(corpo, tit, sizeof tit, sin, sizeof sin) || !sin[0]) {
    const char *porque = corpo ? "a ficha nao tem descricao" : "sem resposta da rede";
    free(corpo);
    printf("[hero] sinopse NAO preenchida %s (%s)\n", id, porque);
    fflush(stdout);
    return 0;
  }
  free(corpo);
  snprintf(c->sinopse, sizeof c->sinopse, "%s", sin);
  if (!c->titulo[0] && tit[0]) snprintf(c->titulo, sizeof c->titulo, "%s", tit);
  printf("[hero] sinopse preenchida %s (%d caracteres, %s)\n", id, (int)strlen(sin),
         prov == METAPROV_CINEMETA ? "Cinemeta" : "catalogo do Nuvio");
  fflush(stdout);
  return 1;
}

static void *fioSinopseHero(void *u) {
  (void)u;
  for (;;) {
    int lista[HSIN_LOTE], n, k;
    pthread_mutex_lock(&hsTrava);
    n = hsNIdx;
    memcpy(lista, hsIdx, sizeof(int) * (size_t)n);
    hsDeNovo = 0;
    pthread_mutex_unlock(&hsTrava);
    for (k = 0; k < n; k++) {
      // Copia sob a trava do catalogo: este fio nao vira quadro, entao um
      // ponteiro de cat_item() aqui pode apontar para bloco ja liberado
      // (revisao 2.0.3, achado 2; tests/herosinopse_corrida.sh).
      CatItem *e = malloc(sizeof *e);
      char imdb[64];
      int r;
      if (!e) continue;
      if (!cat_copiar_item(lista[k], e) || e->sinopse[0]) { free(e); continue; }
      snprintf(imdb, sizeof imdb, "%s", e->imdb);
      if (hsNegativo(imdb)) { free(e); continue; }
      r = desc_sinopse_completar_item(e, NULL, NULL);
      if (r == 0) hsNegGuardar(imdb);
      // O catalogo pode ter sido refeito enquanto a rede respondia: so a
      // sinopse (e o nome, se faltava) entram, e so se o indice ainda e este
      // titulo. Nada mais do item e reescrito.
      if (r == 1) cat_completar_sinopse(lista[k], imdb, e->sinopse, e->titulo);
      free(e);
      SDL_Delay(HSIN_PAUSA_MS);   // um pedido por vez, com folga entre eles
    }
    pthread_mutex_lock(&hsTrava);
    if (hsDeNovo) { pthread_mutex_unlock(&hsTrava); continue; }
    hsVivo = 0;
    pthread_mutex_unlock(&hsTrava);
    return NULL;
  }
}

// Pede a sinopse dos indices `idx` (o conjunto do destaque). Barata e
// repetivel: quem ja tem sinopse, ou falhou ha pouco, e pulado no fio.
void desc_sinopse_hero(const int *idx, int n) {
  pthread_t t;
  int i, falta = 0;
  if (!idx || n < 1) return;
  if (n > HSIN_LOTE) n = HSIN_LOTE;
  for (i = 0; i < n; i++) {
    const CatItem *o = idx[i] >= 0 && idx[i] < cat_n() ? cat_item(idx[i]) : NULL;
    if (o && !o->sinopse[0]) { falta = 1; break; }
  }
  if (!falta) return;
  pthread_mutex_lock(&hsTrava);
  memcpy(hsIdx, idx, sizeof(int) * (size_t)n);
  hsNIdx = n;
  if (hsVivo) { hsDeNovo = 1; pthread_mutex_unlock(&hsTrava); return; }
  hsVivo = 1;
  pthread_mutex_unlock(&hsTrava);
  if (pthread_create(&t, NULL, fioSinopseHero, NULL) != 0) hsVivo = 0;
  else pthread_detach(t);
}

// Publica a parte critica antes de qualquer enriquecimento opcional. Assim a
// fileira de episodios aparece depois da primeira resposta, sem esperar pelas
// duas viagens ao TMDB usadas para foto e personagem do elenco.
// NOTAS DE EPISODIO vindas do TMDB (issue #87). O Cinemeta nao tem voto por
// episodio; o TMDB tem, na resposta de /tv/<id>/season/<n> — episodes[] com
// episode_number e vote_average (0..10). Funcao PURA, chamada tambem pelo
// teste (tests/cateps.c): casa por numero e so escreve nos eps da temporada
// pedida; voto ausente ou zero deixa nota=0, que na tela simplesmente nao
// desenha selo. Devolve quantos episodios ganharam nota ou sinopse (#150).
// ELENCO DO TMDB CASADO POR NOME (#153). Aqui era por POSICAO: foto, papel e
// id da N-esima entrada do TMDB iam para o N-esimo nome do Cinemeta, na
// suposicao de que as duas bases ordenam o elenco igual. Nao ordenam: na foto
// do #153 a fileira mostrou Jacob Tremblay com o rosto e o papel de Shailene
// Woodley, e ela com os dele — dado errado com cara de dado certo.
//
// A comparacao e so de letras e digitos, em minuscula e sem acento latino
// (normElenco): "Zoë", "Zoe" e "ZOE" casam; "J. K. Simmons" e "JK Simmons"
// tambem. Nome que nao casa com ninguem fica SEM foto e sem papel — melhor
// que o de outra pessoa. As entradas do TMDB que nao casaram entram no fim,
// na ordem do TMDB, ate o teto (o #94: o Cinemeta para em 3-5 nomes), sem
// repetir quem ja esta na lista. Pura; devolve quantos nomes ganharam foto
// ou papel por casamento.
static char normDobra(unsigned char segundo) {
  unsigned cp = (unsigned)segundo + 0x40u;
  if (cp >= 0xC0 && cp <= 0xDE && cp != 0xD7) cp += 0x20;
  if (cp >= 0xE0 && cp <= 0xE6) return 'a';
  if (cp == 0xE7)               return 'c';
  if (cp >= 0xE8 && cp <= 0xEB) return 'e';
  if (cp >= 0xEC && cp <= 0xEF) return 'i';
  if (cp == 0xF1)               return 'n';
  if ((cp >= 0xF2 && cp <= 0xF6) || cp == 0xF8) return 'o';
  if (cp >= 0xF9 && cp <= 0xFC) return 'u';
  if (cp == 0xFD || cp == 0xFF) return 'y';
  return 0;
}
static void normElenco(const char *s, char *dst, size_t tam) {
  const unsigned char *p = (const unsigned char *)(s ? s : "");
  size_t k = 0;
  while (*p && k + 1 < tam) {
    unsigned char c = *p++;
    char o = 0;
    if (c >= 'A' && c <= 'Z') o = (char)(c + 32);
    else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) o = (char)c;
    else if (c == 0xC3 && *p) o = normDobra(*p++);
    else if (c >= 0x80) { while ((*p & 0xC0) == 0x80) p++; }
    if (o) dst[k++] = o;
  }
  dst[k] = 0;
}

int desc_tmdb_elenco(const char *json, CatItem *d) {
  enum { MAX_TMDB = 48 };
  struct { char nome[64], papel[64], foto[512]; long id; int usado; } *t;
  char alvo[64], outro[64];
  const char *p;
  int n = 0, i, k, casados = 0;
  if (!json || !d) return 0;
  t = calloc(MAX_TMDB, sizeof *t);
  if (!t) return 0;
  for (p = js_array(json, NULL, "cast"); p && n < MAX_TMDB; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    char caminhoFoto[128] = "";
    js_texto(p, f, "name", t[n].nome, sizeof t[n].nome);
    if (!t[n].nome[0]) continue;   // sem nome nao vira pessoa na fileira
    js_texto(p, f, "character", t[n].papel, sizeof t[n].papel);
    t[n].id = (long)js_num(p, f, "id", 0.0);
    if (js_texto(p, f, "profile_path", caminhoFoto, sizeof caminhoFoto) &&
        caminhoFoto[0] == '/')
      snprintf(t[n].foto, sizeof t[n].foto, "https://image.tmdb.org/t/p/w185%s", caminhoFoto);
    n++;
  }
  // Os nomes que ja estao na lista (do Cinemeta, ou de uma passada anterior).
  for (k = 0; k < d->nElenco; k++) {
    normElenco(d->elenco[k].nome, alvo, sizeof alvo);
    if (!alvo[0]) continue;
    for (i = 0; i < n; i++) {
      if (t[i].usado) continue;
      normElenco(t[i].nome, outro, sizeof outro);
      if (strcmp(alvo, outro)) continue;
      t[i].usado = 1;
      if (t[i].papel[0])
        snprintf(d->elenco[k].papel, sizeof d->elenco[k].papel, "%s", t[i].papel);
      if (t[i].foto[0])
        snprintf(d->elenco[k].foto, sizeof d->elenco[k].foto, "%s", t[i].foto);
      if (t[i].id > 0) d->elenco[k].tmdb = t[i].id;
      casados++;
      break;
    }
  }
  // E A LISTA CRESCE (#94), com quem sobrou do TMDB.
  for (i = 0; i < n && d->nElenco < CAT_ELENCO_MAX; i++) {
    int j = d->nElenco;
    if (t[i].usado) continue;
    snprintf(d->elenco[j].nome, sizeof d->elenco[j].nome, "%s", t[i].nome);
    snprintf(d->elenco[j].papel, sizeof d->elenco[j].papel, "%s", t[i].papel);
    snprintf(d->elenco[j].foto, sizeof d->elenco[j].foto, "%s", t[i].foto);
    d->elenco[j].tmdb = t[i].id;
    d->nElenco++;
  }
  free(t);
  return casados;
}

// O nome que o TMDB devolve quando NAO tem traducao e "Episodio 3" (ou o
// equivalente no idioma pedido): pior que o titulo original. Generico = o
// numero do episodio como palavra inteira e, tirado ele, sobra no maximo UMA
// palavra ("Episode 3", "Серія 3", "Episodul 3", "#3 Episodio"). Titulo de
// verdade com mais de uma palavra sobrando ("Season Finale 3") nao cai aqui.
int desc_nome_episodio_generico(const char *nome, int episodio) {
  char num[16];
  const char *p = nome;
  size_t nn;
  int palavras = 0, achou = 0, emPalavra = 0;
  if (!nome || !nome[0]) return 1;
  snprintf(num, sizeof num, "%d", episodio);
  nn = strlen(num);
  for (; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if (c >= '0' && c <= '9') {
      const char *q = p;
      while (*q >= '0' && *q <= '9') q++;
      if ((size_t)(q - p) == nn && !strncmp(p, num, nn)) achou = 1;
      else if (!emPalavra) { palavras++; emPalavra = 1; }
      p = q - 1;
      continue;
    }
    if (c == ' ' || c == '#' || c == '.' || c == ':' || c == '-' || c == ',') {
      emPalavra = 0;
      continue;
    }
    if (!emPalavra) { palavras++; emPalavra = 1; }
  }
  return achou && palavras <= 1;
}

int desc_tmdb_notas_temporada(const char *json, CatEp *eps, int n,
                              int temporada) {
  return desc_tmdb_notas_temporada_ex(json, eps, n, temporada, DESC_EPT_SINOPSE);
}

int desc_tmdb_notas_temporada_ex(const char *json, CatEp *eps, int n,
                                 int temporada, int textos) {
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
          char sin[sizeof eps[i].sinopse], nome[sizeof eps[i].nome];
          int mudou = 0, soVazio = (textos & DESC_EPT_SO_VAZIO) != 0;
          int runtime = (int)js_num(p, f, "runtime", 0);
          // O TMDB TEM ESTE PAR: e o numero que o TheIntroDB entende sem
          // remapear (2.0.3). Nao conta em `feitos` (nao e texto novo).
          eps[i].tmdbT = temporada; eps[i].tmdbE = num;
          if (!eps[i].duracao[0] && runtime > 0 && runtime < 1440) {
            snprintf(eps[i].duracao, sizeof eps[i].duracao, "%d min", runtime);
            mudou = 1;
          }
          if (v > 0.0) { eps[i].nota = (int)(v * 10.0 + 0.5); mudou = 1; }
          // SINOPSE NO IDIOMA ESCOLHIDO (#150). O pedido ja vai com
          // language=desc_tmdb_idioma(), e o `overview` vinha sendo jogado
          // fora: a sinopse da fileira era a do Cinemeta, sempre em ingles.
          // Vazio (o TMDB sem traducao) deixa a que ja estava.
          if ((textos & DESC_EPT_SINOPSE) &&
              js_texto(p, f, "overview", sin, sizeof sin) && sin[0] &&
              strcmp(sin, eps[i].sinopse) && (!soVazio || !eps[i].sinopse[0])) {
            snprintf(eps[i].sinopse, sizeof eps[i].sinopse, "%s", sin);
            mudou = 1;
          }
          // O NOME (#176: "os nomes dos episodios ficam em ingles"). Sem
          // traducao o TMDB devolve "Episodio 3", pior que o titulo original —
          // por isso o generico nao entra (desc_nome_episodio_generico), e o
          // nome que ja estava fica.
          if ((textos & DESC_EPT_NOME) &&
              js_texto(p, f, "name", nome, sizeof nome) && nome[0] &&
              !desc_nome_episodio_generico(nome, num) &&
              strcmp(nome, eps[i].nome) && (!soVazio || !eps[i].nome[0])) {
            snprintf(eps[i].nome, sizeof eps[i].nome, "%s", nome);
            mudou = 1;
          }
          feitos += mudou;
          break;
        }
    }
    p = js_prox(f);
  }
  return feitos;
}

// Em que tipo(s) perguntar o /meta do Cinemeta, na ordem. Filme e serie sao
// certos: um pedido so. Qualquer outro ("anime" dos catalogos do AIOMetadata,
// vazio de fonte que nao disse) e incerto: serie primeiro — anime e quase
// sempre serie, e so a resposta de serie tem como PROVAR o tipo (temporadas);
// a de filme e aceita sem prova, e foi assim que um anime virou "My Way Home".
int desc_meta_tipos(const char *tipo, const char *saida[2]) {
  if (tipo && !strcmp(tipo, "series")) { saida[0] = "series"; return 1; }
  if (tipo && !strcmp(tipo, "movie"))  { saida[0] = "movie";  return 1; }
  saida[0] = "series";
  saida[1] = "movie";
  return 2;
}

// Chave do cache de /meta: tipo + id, nunca so o id.
void desc_meta_chave(char *dst, size_t n, const char *tipo, const char *id) {
  // provedor + idioma + tipo + id: trocar o idioma dos metadados refaz o pedido.
  metaprov_chave(dst, n, tipo, id);
}

// Temporada de um video. Addon de anime as vezes manda "episode" sem "season"
// (o entry do Kitsu ja e a temporada); sem o campo o video era descartado e a
// serie ficava sem episodio nenhum. Ausente com episodio > 0 conta como 1.
// "season":0 (especiais) continua fora, como sempre foi.
static int videoTemporada(const char *p, const char *f) {
  int t = (int)js_num(p, f, "season", -1);
  if (t < 0 && js_num(p, f, "episode", -1) > 0) t = 1;
  return t;
}

// A resposta do /meta tem ao menos um video com temporada > 0?
int desc_meta_tem_temporadas(const char *corpo) {
  const char *v = corpo ? js_array(corpo, NULL, "videos") : NULL;
  while (v) {
    const char *f = js_fim(v);
    if (videoTemporada(v, f) > 0) return 1;
    v = js_prox(f);
  }
  return 0;
}

// CONJUNTO DE (temporada, episodio) JA VISTOS num videos[]. Um addon de FONTES
// pode responder o /meta com um video por ARQUIVO de torrent (relato de TV LG,
// Breaking Bad: 18529 entradas para 62 episodios), entao contar e cortar tem de
// ser sobre episodios DISTINTOS. Enderecamento aberto, dobra ao passar de meia
// carga: O(n) no corpo inteiro. Memoria pelos pares DISTINTOS, nao pelas
// entradas: 8 KiB (1024 casas) ate 511 pares, o caso do relato (62); 18 mil
// pares distintos chegam a 512 KiB, com 768 KiB no instante da copia.
typedef struct { unsigned long long *v; unsigned cap, n; } EpSet;

static unsigned epSetCasa(const EpSet *s, unsigned long long k) {
  unsigned i = (unsigned)((k * 0x9E3779B97F4A7C15ull) >> 40) & (s->cap - 1);
  while (s->v[i] && s->v[i] != k) i = (i + 1) & (s->cap - 1);
  return i;
}

// 1 = par novo (entrou); 0 = repetido. A chave e procurada ANTES de crescer:
// faltar memoria para a tabela maior nao pode transformar em "novo" um par que
// ja esta nela. Sem crescer, os pares novos seguem entrando ate sobrar uma casa
// vazia (a que encerra a sondagem); so dai em diante o par que nao coube
// responde "novo" sem ser guardado — o pior caso e o de antes do conjunto.
static int epSetNovo(EpSet *s, int t, int e) {
  unsigned long long k = ((unsigned long long)(unsigned)t << 32) | (unsigned)e;
  unsigned i;
  if (!s->v) {
    s->cap = 1024; s->n = 0;
    s->v = calloc(s->cap, sizeof *s->v);
    if (!s->v) return 1;
  }
  i = epSetCasa(s, k);
  if (s->v[i]) return 0;
  if (s->n * 2 >= s->cap) {
    EpSet g = { calloc((size_t)s->cap * 2, sizeof *s->v), s->cap * 2, s->n };
    if (g.v) {
      unsigned j;
      for (j = 0; j < s->cap; j++)
        if (s->v[j]) g.v[epSetCasa(&g, s->v[j])] = s->v[j];
      free(s->v);
      *s = g;
      i = epSetCasa(s, k);
    } else if (s->n + 2 > s->cap) return 1;   // cheia: fica a casa vazia
  }
  s->v[i] = k; s->n++;
  return 1;
}

// Quantos episodios DISTINTOS (temporada > 0) a resposta do /meta traz; em
// `brutos` (opcional), quantos videos com temporada > 0, repeticoes incluidas.
// Video sem numero de episodio nao e repeticao de outro (regra do #328): cada
// um conta.
static int metaContarEpisodios(const char *corpo, int *brutos) {
  const char *v = corpo ? js_array(corpo, NULL, "videos") : NULL;
  EpSet set = { 0 };
  int n = 0, b = 0;
  while (v) {
    const char *f = js_fim(v);
    int t = videoTemporada(v, f);
    if (t > 0) {
      int e = (int)js_num(v, f, "episode", 0);
      b++;
      if (e <= 0 || epSetNovo(&set, t, e)) n++;
    }
    v = js_prox(f);
  }
  free(set.v);
  if (brutos) *brutos = b;
  return n;
}

int desc_meta_n_episodios(const char *corpo) { return metaContarEpisodios(corpo, NULL); }

// O videos[] e uma lista de ARQUIVOS, nao de episodios? Em media mais de tres
// entradas por episodio so acontece em addon de fontes (o agregador de anime
// do #328 repete uma ou duas vezes). Lista pequena nunca e.
// Ser lista de arquivos NAO descarta o corpo por si so: ele perde o empate e a
// preferencia pela ficha do addon, e so e recusado quando nao conhece MAIS
// episodios distintos que a lista com que disputa (episodiosDoAddon,
// episodiosDoCatalogo).
static int metaListaDeArquivos(int distintos, int brutos) {
  return brutos >= 60 && brutos > distintos * 3;
}

// Em par com CAT_EP_MAX (catalogo.c): um titulo que caiba no store nao pode
// truncar no parse, e um que nao caiba trunca aqui em vez de zerar os outros.
#define VIDEOS_MAX 1200

// videos[] de um /meta -> lista de CatEp ORDENADA por (temporada, episodio).
// So entram videos com temporada > 0. Devolve quantos.
static int parsearEpisodios(const char *corpo, CatEp *eps, int max) {
  int n = 0;
  const char *p = js_array(corpo, NULL, "videos");
  EpSet set = { 0 };
  while (p && n < max) {
    const char *f = js_fim(p);
    int t = videoTemporada(p, f);
    int ne = t > 0 ? (int)js_num(p, f, "episode", 0) : 0;
    // Repeticao sai JA AQUI, antes do corte de `max`: descartada so depois de
    // ordenar, as 1200 vagas iam para copias dos primeiros episodios e o resto
    // da serie nem era lido. Fica o primeiro da resposta; sem numero de
    // episodio nao e repeticao.
    if (t > 0 && (ne <= 0 || epSetNovo(&set, t, ne))) {
      CatEp *e = &eps[n];
      char d[24] = "";
      memset(e, 0, sizeof *e);
      e->temporada = t;
      e->episodio = ne;
      js_texto(p, f, "name", e->nome, sizeof e->nome);
      // O id do video (cat_id_stream): e ele que se manda aos addons de fonte
      // quando o titulo nao e do IMDb. Addon que usa "title" no lugar de "name"
      // (o Kitsu) tambem tem o nome lido.
      js_texto_raiz_em(p, f, "id", e->vid, sizeof e->vid);
      if (!e->nome[0]) js_texto(p, f, "title", e->nome, sizeof e->nome);
      js_texto(p, f, "overview", e->sinopse, sizeof e->sinopse);
      if (!e->sinopse[0]) js_texto(p, f, "description", e->sinopse, sizeof e->sinopse);
      js_texto(p, f, "thumbnail", e->thumb, sizeof e->thumb);
      js_texto(p, f, "runtime", e->duracao, sizeof e->duracao);
      metaprov_duracao(e->duracao, sizeof e->duracao);   // "25min" -> "25 min"
      if (!e->duracao[0]) {
        int runtime = (int)js_num(p, f, "runtime", 0);
        if (runtime > 0 && runtime < 1440)
          snprintf(e->duracao, sizeof e->duracao, "%d min", runtime);
      }
      js_texto(p, f, "released", d, sizeof d);
      desc_data_extenso(d, e->data, sizeof e->data);
      n++;
    }
    p = js_prox(f);
  }
  free(set.v);
  for (int i = 1; i < n; i++) {
    CatEp k = eps[i];
    int j;
    for (j = i - 1; j >= 0 &&
         (eps[j].temporada > k.temporada ||
          (eps[j].temporada == k.temporada && eps[j].episodio > k.episodio)); j--)
      eps[j + 1] = eps[j];
    eps[j + 1] = k;
  }
  // Um /meta pode trazer o mesmo (temporada, episodio) mais de uma vez (agregador
  // de anime com varias entradas, #328): cada repeticao virava um cartao
  // identico. A ordenacao acima e estavel, entao ficar com o primeiro da
  // vizinhanca e ficar com o primeiro da resposta.
  if (n > 1) {
    int w = 1;
    for (int i = 1; i < n; i++) {
      // Sem numero de episodio (0) nao e repeticao: todos os videos sem
      // "episode" cairiam num cartao so.
      if (eps[i].episodio > 0 &&
          eps[i].temporada == eps[w - 1].temporada &&
          eps[i].episodio == eps[w - 1].episodio) continue;
      if (w != i) eps[w] = eps[i];
      w++;
    }
    n = w;
  }
  return n;
}

void desc_mesclar_episodios(CatEp *base, int nb, const CatEp *outro, int no, int modo) {
  int i = 0, j = 0;
  while (i < nb && j < no) {
    int c = base[i].temporada != outro[j].temporada
          ? (base[i].temporada < outro[j].temporada ? -1 : 1)
          : (base[i].episodio < outro[j].episodio ? -1
             : base[i].episodio > outro[j].episodio ? 1 : 0);
    if (c < 0) { i++; continue; }
    if (c > 0) { j++; continue; }
    if (modo == DESC_MESCLA_TEXTO) {
      if (outro[j].nome[0])    snprintf(base[i].nome, sizeof base[i].nome, "%s", outro[j].nome);
      if (outro[j].sinopse[0]) snprintf(base[i].sinopse, sizeof base[i].sinopse, "%s", outro[j].sinopse);
    } else {
      if (!base[i].nome[0])    snprintf(base[i].nome, sizeof base[i].nome, "%s", outro[j].nome);
      if (!base[i].sinopse[0]) snprintf(base[i].sinopse, sizeof base[i].sinopse, "%s", outro[j].sinopse);
    }
    // O que o addon nao traz (still, duracao, data) vem da outra lista nos
    // dois modos: uma lista sem imagem e pior que uma com imagem em ingles.
    if (!base[i].thumb[0])   snprintf(base[i].thumb, sizeof base[i].thumb, "%s", outro[j].thumb);
    if (!base[i].duracao[0]) snprintf(base[i].duracao, sizeof base[i].duracao, "%s", outro[j].duracao);
    if (!base[i].data[0])    snprintf(base[i].data, sizeof base[i].data, "%s", outro[j].data);
    i++; j++;
  }
}

// `sobre` (opcional) e o /meta da OUTRA fonte, mesclado por `modo`
// (desc_mesclar_episodios) antes de publicar.
// O TITULO DO FIO DE EPISODIOS (#190). buscarEps guarda o INDICE e depois
// passa segundos na rede; se o catalogo for refeito nesse meio (a refacao de
// "Continuar assistindo" roda com o player aberto), cat_definir_episodios e
// cat_atualizar_item escreviam na posicao velha — a lista e a ficha de um
// titulo iam parar em outro. epAlvo re-resolve pelo id antes de cada escrita;
// -1 = o titulo saiu do catalogo e nada e escrito. Um fio por vez (fioEpVivo).
static char epAlvoId[64];
// Com o id do PROPRIO fio: a cauda do buscarEps (notas por temporada do TMDB,
// dezenas de pedidos em serie longa) roda depois de fioEpVivo ser solto, e o
// proximo titulo ja reescreveu epAlvoId (sem _Thread_local no Tizen 4/5).
static int epAlvoDe(int alvoItem, const char *id) {
  int i = cat_indice_vivo(alvoItem, id);
  if (i != alvoItem) {
    printf("[desc] episodios de %s: catalogo remontou no meio do pedido (%d -> %d)\n",
           id, alvoItem, i);
    fflush(stdout);
  }
  return i;
}
static int epAlvo(int alvoItem) { return epAlvoDe(alvoItem, epAlvoId); }

// AS ABAS DE TEMPORADA SAO AS DA LISTA QUE FOI PUBLICADA (#372). Eram lidas do
// corpo da ficha (o do Nuvio) mesmo quando a lista que entrou era a de um
// addon: Apothecary Diaries publicava 72 episodios do AIOMetadata nas
// temporadas 1-3 com UMA aba (o Nuvio junta tudo na 1), e detail.c so mostra
// episodio de temporada que tem aba. Cada publicacao regrava estas abas; quem
// publica por ultimo e quem as define.
typedef struct { int n, t[CAT_TEMP_MAX]; } TempsPub;

static void temporadasDosEps(const CatEp *eps, int n, TempsPub *tp) {
  int i, j, k;
  tp->n = 0;
  for (i = 0; i < n; i++) {
    int t = eps[i].temporada;
    if (t <= 0) continue;
    for (j = 0; j < tp->n && tp->t[j] < t; j++) {}
    if (j < tp->n && tp->t[j] == t) continue;
    if (tp->n >= CAT_TEMP_MAX) continue;
    for (k = tp->n; k > j; k--) tp->t[k] = tp->t[k - 1];
    tp->t[j] = t;
    tp->n++;
  }
}

static int publicarEpisodios(const char *corpo, int alvoItem, const char *titulo,
                             const char *sobre, int modo, TempsPub *tp) {
  CatEp *eps = malloc(sizeof(CatEp) * VIDEOS_MAX);
  int n = 0;
  if (!eps) return 0;
  n = parsearEpisodios(corpo, eps, VIDEOS_MAX);
  if (tp && n) temporadasDosEps(eps, n, tp);
  if (sobre && n) {
    CatEp *o = malloc(sizeof(CatEp) * VIDEOS_MAX);
    if (o) {
      int no = parsearEpisodios(sobre, o, VIDEOS_MAX);
      desc_mesclar_episodios(eps, n, o, no, modo);
      free(o);
    }
  }
  if (n && (alvoItem = epAlvo(alvoItem)) >= 0) cat_definir_episodios(alvoItem, eps, n);
  free(eps);
  marco("episodios na tela");
  printf("[desc] %s: %d episodios publicados antes dos extras\n", titulo, n);
  fflush(stdout);
  return n;
}

// O Cinemeta as vezes conhece MENOS episodios que o addon de metadados do
// usuario (#174: "Mis muertos tristes" tinha 1 de 4; #175: a serie so estava
// completa noutra fonte). Pergunta o /meta/series de cada addon ativo que
// declara o resource "meta" e, se algum trouxer mais episodios que o Cinemeta,
// publica a lista dele no lugar. So troca por lista MAIOR: quem ja estava
// completo continua como estava. A URL do addon carrega credencial, entao o
// log diz so o nome.
//
// COM `aplicar` (#176: a ficha do addon preferida, ou idioma que nao e o
// ingles sem TMDB para traduzir) o addon tambem manda no TEXTO: se a lista dele
// nao e menor, vira a base (o Cinemeta completa still/duracao/data que faltem);
// se e menor, a do Cinemeta fica e leva nome e sinopse do addon nos episodios
// que os dois tem. Devolve 1 quando o texto do addon entrou.
static int episodiosDoAddon(int alvoItem, const char *serie, const char *titulo,
                            const char *corpoCine, int nCine, int aplicar, TempsPub *tp) {
  char *melhorCorpo = NULL;
  const char *melhorNome = "";
  int melhor = 0, melhorArq = 0, i, n = addons_n(), usouTexto = 0;
  for (i = 0; i < n; i++) {
    char *c2 = metaDoAddon(i, "series", serie);
    int n2, brutos = 0, arq;
    if (!c2) continue;
    // Episodios DISTINTOS: pela contagem bruta, a lista de arquivos de um addon
    // de fontes (18529 "episodios" contra 62) ganhava de qualquer lista real.
    n2 = metaContarEpisodios(c2, &brutos);
    arq = metaListaDeArquivos(n2, brutos);
    // LISTA DE ARQUIVOS SO ENTRA SE SOUBER MAIS EPISODIOS que o Cinemeta (20 em
    // 4 variantes contra 12: os 8 a mais nao se perdem). No empate ou com menos
    // ela nao serve nem de lista nem de texto: o "nome" dela e nome de arquivo.
    if (arq && n2 <= nCine) {
      printf("[desc] %s: %s respondeu %d videos para %d episodios (lista de arquivos) "
             "contra %d do Cinemeta; nao serve de lista de episodios\n",
             titulo, addons_nome(i), brutos, n2, nCine);
      fflush(stdout);
      free(c2);
      continue;
    }
    // Entre addons, mais episodios ganha; no empate, a lista de verdade tira a
    // de arquivos.
    if (n2 > melhor || (n2 == melhor && melhorArq && !arq)) {
      free(melhorCorpo);
      melhorCorpo = c2; melhor = n2; melhorArq = arq; melhorNome = addons_nome(i);
    } else free(c2);
  }
  if (!melhorCorpo) return 0;
  if (melhorArq) {
    // So chegou aqui por saber MAIS episodios. A lista e dela; o texto nao:
    // o nome do Cinemeta entra por cima nos episodios que os dois tem, e o
    // texto do addon nao conta como aplicado (o TMDB segue podendo traduzir).
    printf("[desc] %s: %s tem %d episodios (lista de arquivos) contra %d do Cinemeta; "
           "usando a lista do addon com os nomes do Cinemeta\n",
           titulo, melhorNome, melhor, nCine);
    fflush(stdout);
    publicarEpisodios(melhorCorpo, alvoItem, titulo, corpoCine, DESC_MESCLA_TEXTO, tp);
    arte_reserva_episodios(serie, melhorCorpo);
  } else if (melhor > nCine || (aplicar && melhor >= nCine)) {
    printf("[desc] %s: %s tem %d episodios contra %d do Cinemeta; usando a lista do addon\n",
           titulo, melhorNome, melhor, nCine);
    fflush(stdout);
    publicarEpisodios(melhorCorpo, alvoItem, titulo, aplicar ? corpoCine : NULL,
                      DESC_MESCLA_VAZIOS, tp);
    arte_reserva_episodios(serie, melhorCorpo);
    usouTexto = aplicar;
  } else if (aplicar) {
    printf("[desc] %s: %s tem %d episodios contra %d do Cinemeta; nome e sinopse do addon\n",
           titulo, melhorNome, melhor, nCine);
    fflush(stdout);
    publicarEpisodios(corpoCine, alvoItem, titulo, melhorCorpo, DESC_MESCLA_TEXTO, tp);
    usouTexto = 1;
  }
  free(melhorCorpo);
  return usouTexto;
}

// genres[] de um /meta -> "A · B · C" (o separador do web). Vazio sem generos.
static void generosDe(const char *corpo, char *lista, size_t tam, const char *tipo) {
  const char *g = js_array(corpo, NULL, "genres");
  size_t n3 = 0;
  int nGen = 0;
  lista[0] = 0;
  // "Filme  ·  Drama  ·  Misterio", no formato que deMeta e o catalogo do pacote
  // gravam: o PRIMEIRO trecho e o tipo, e as telas (hero do detalhe,
  // compartilhaGenero) o descartam por ser o tipo. Sem ele o primeiro GENERO era
  // descartado no lugar dele — "Action · Adventure" saia so "Adventure" — e o
  // titulo de um genero so ("Drama") ficava sem genero nenhum na tela.
  // Cada genero passa por desc_genero_pt: o Cinemeta os manda em ingles.
  if (tipo) n3 = (size_t)snprintf(lista, tam, "%s", i18n(rotuloTipoSing(tipo)));
  if (n3 >= tam) n3 = tam - 1;
  while (g && *g == '"' && n3 + 1 < tam) {
    const char *p2 = g + 1;
    char nome[64]; size_t nn = 0;
    while (*p2 && *p2 != '"' && nn + 1 < sizeof nome) nome[nn++] = *p2++;
    nome[nn] = 0;
    { const char *pt = desc_genero_pt(nome);
      int w = snprintf(lista + n3, tam - n3, "%s%s", n3 ? "  \xc2\xb7  " : "", pt);
      if (w < 0 || (size_t)w >= tam - n3) { lista[n3] = 0; break; }
      n3 += (size_t)w; nGen++; }
    if (*p2 == '"') p2++;
    while (*p2 == ' ') p2++;
    g = (*p2 == ',') ? p2 + 1 : NULL;
    while (g && *g == ' ') g++;
  }
  if (!nGen) lista[0] = 0;   // so o tipo nao e lista de generos: o chamador mantem o que tinha
}

// ITEM RASO (#176). Titulo aberto de "Salvos"/Biblioteca nao chega como o da
// busca: o SalvoItem guarda so titulo, poster e meta, e a lista do Trakt
// (trakt_lista) so titulo, imdb e a arte SINTETICA do metahub. Sem sinopse, sem
// id do TMDB, com fundo e logo montados pelo id (que a busca nao tem) e
// classificacao "14" cravada. A busca e as fileiras entram por deMeta, que le
// tudo isso do mesmo /meta que buscarEps ja tem na mao — entao o que falta e
// copiar de la, e o detalhe fica igual em qualquer entrada.
//
// Raso = sem sinopse. Um item completo nao e tocado. Poster e generos ficam
// (o poster ja esta na tela; os generos buscarEps ja regrava).
static void completarRaso(CatItem *dst, const char *corpo, const char *tipo) {
  const char *m = corpo ? strstr(corpo, "\"meta\"") : NULL;
  CatItem *cheio;
  if (!m || !dst || dst->sinopse[0]) return;
  cheio = malloc(sizeof *cheio);
  if (!cheio) return;
  if (deMeta(m, NULL, tipo, cheio)) {
    snprintf(dst->sinopse, sizeof dst->sinopse, "%s", cheio->sinopse);
    if (cheio->meta[0]) snprintf(dst->meta, sizeof dst->meta, "%s", cheio->meta);
    if (cheio->nota > 0) dst->nota = cheio->nota;
    if (cheio->tmdb > 0 && dst->tmdb <= 0) dst->tmdb = cheio->tmdb;
    // Fundo e logo do /meta MANDAM, inclusive o logo vazio: o do metahub que
    // o item raso montou pode nem existir, e a busca desenharia o nome.
    // Com a arte do addon ligada (Ajustes), o fundo e o logo que o catalogo do
    // addon ja trouxe ficam: o /meta so preenche o que falta.
    int fundoAddon = ajustes_fundo_addon() && dst->origem[0] &&
                     dst->backdropCatalogo[0] && strcmp(dst->backdropCatalogo, dst->poster);
    int logoAddon = ajustes_logo_addon() && dst->origem[0] && dst->logo[0];
    if (cheio->backdrop[0] && !fundoAddon) {
      snprintf(dst->backdrop, sizeof dst->backdrop, "%s", cheio->backdrop);
      snprintf(dst->backdropCatalogo, sizeof dst->backdropCatalogo, "%s", cheio->backdropCatalogo);
      snprintf(dst->backdropTmdb, sizeof dst->backdropTmdb, "%s", cheio->backdropTmdb);
      snprintf(dst->backdropTrakt, sizeof dst->backdropTrakt, "%s", cheio->backdropTrakt);
    }
    if (!logoAddon) snprintf(dst->logo, sizeof dst->logo, "%s", cheio->logo);
    if (!strcmp(dst->classificacao, "14")) dst->classificacao[0] = 0;
  }
  free(cheio);
}

// ============================================================================
// FICHA DO TITULO: CATALOGO PRIMEIRO (metaCatalogo)
// ============================================================================
//
// O PROBLEMA (relato do dono). Titulo de catalogo de addon de anime chega com id
// PROPRIO — "kitsu:123", "mal:456", "anilist:789", "xperience:...", "tmdb:..." —
// e abria sem episodio nem ficha: buscarEps so pedia /meta ao Cinemeta, que so
// conhece "tt". Kitsu/AniList entravam so como fonte de ARTE (artefontes.c). Mas
// o addon que PUBLICOU o item quase sempre serve /meta/<tipo>/<id>.json para os
// ids dele, com titulo, sinopse e a lista de episodios (e o id de stream de cada
// um).
//
// A ORDEM, e o porque de cada degrau:
//   a) o addon de ORIGEM do item (CatItem.origem), se o manifesto nao declarou
//      que esse tipo/prefixo nao e dele. E o dono do id: o unico que se sabe
//      que o conhece.
//   b) outros addons com "meta" ATIVOS, na ordem da lista do usuario, cujo
//      idPrefixes casa com o id (addons_aceita_id == 1). Id do IMDb aceita
//      tambem quem nao declarou prefixo (-1), porque "tt" e o id que todo mundo
//      fala. So se ainda falta: uma fonte que ja entregou ficha completa
//      (sinopse + episodios) encerra a busca.
//   c) o Cinemeta, para id do IMDb — e para o "tt" que a propria ficha do addon
//      revelou (imdb_id, imdbId, link do IMDb).
//   d) id que nao e do IMDb, sem "tt" a vista e a ficha incompleta: a API ARM
//      (arm.haglund.dev, publica, sem chave — conferido em 29/09/2026) converte
//      kitsu/mal/anilist/anidb em imdb, e dai o Cinemeta entra como em (c).
//
// A REGRA DE MESCLA: a primeira fonte que responde uma ficha valida e a BASE
// (titulo, sinopse, episodios, generos, elenco). As seguintes so preenchem o que
// a base deixou VAZIO (elenco, direcao, nota, pais, generos, sinopse; e a lista
// de episodios inteira quando a base nao tem nenhuma). Nunca sobrescrevem.
// Episodio a episodio (still, data, duracao) so se mescla quando as duas listas
// estao no MESMO espaco de ids — o Cinemeta e o IMDb; "kitsu:41370" e uma
// temporada, e T2E5 do Cinemeta nao e o E5 dela.
//
// "Usar sempre o Cinemeta" (ajustes_meta_so_cinemeta) desliga tudo isto e o app
// volta ao comportamento de antes.
#define META_FONTES_MAX 4
typedef struct {
  char *corpo[META_FONTES_MAX];    // cada /meta, na ordem de prioridade
  char  nome[META_FONTES_MAX][64];
  int   cine[META_FONTES_MAX];     // 1 = veio do Cinemeta
  int   n;
  int   ehFilme;
  char  tt[24];                    // IMDb conhecido do titulo ("" = nenhum)
} MetaFontes;

static void metaFontesLiberar(MetaFontes *mf) {
  int i;
  for (i = 0; i < mf->n; i++) { free(mf->corpo[i]); mf->corpo[i] = NULL; }
  mf->n = 0;
}

static void metaFontesAdd(MetaFontes *mf, char *corpo, const char *nome, int cine) {
  if (mf->n >= META_FONTES_MAX) { free(corpo); return; }
  mf->corpo[mf->n] = corpo;
  snprintf(mf->nome[mf->n], sizeof mf->nome[0], "%s", nome ? nome : "");
  mf->cine[mf->n] = cine;
  mf->n++;
}

// O objeto "meta" tem nome? (resposta de erro e "meta":null dao 0)
static int metaValida(const char *corpo) {
  char tit[160];
  return metaTextos(corpo, tit, sizeof tit, NULL, 0);
}

// O tipo que a propria ficha diz ("movie"/"series"); "" se nao disse ou disse
// outra coisa ("anime").
static void metaTipoProprio(const char *corpo, char *dst, size_t n) {
  const char *m = corpo ? strstr(corpo, "\"meta\"") : NULL;
  char t[16] = "";
  if (n) dst[0] = 0;
  if (!m) return;
  m += 6;
  while (*m == ' ' || *m == ':' || *m == '\n' || *m == '\t' || *m == '\r') m++;
  if (*m != '{') return;
  if (js_texto_raiz_em(m, NULL, "type", t, sizeof t) &&
      (!strcmp(t, "movie") || !strcmp(t, "series")))
    snprintf(dst, n, "%s", t);
}

// A ficha ja diz tudo que o detalhe pede? Sinopse e, se e serie, episodios.
// Quem esta completa dispensa perguntar a mais fontes.
static int metaCompleta(const char *corpo) {
  char tit[160], sin[900], tp[16];
  if (!metaTextos(corpo, tit, sizeof tit, sin, sizeof sin) || !sin[0]) return 0;
  metaTipoProprio(corpo, tp, sizeof tp);
  if (!strcmp(tp, "movie")) return 1;
  return desc_meta_n_episodios(corpo) > 0;
}

// O IMDb que a ficha revela: imdb_id / imdbId na raiz do "meta", o proprio id
// se for "tt", ou um link imdb.com/title/tt... Vazio se nao ha.
static void metaImdbDaFicha(const char *corpo, char *dst, size_t n) {
  const char *m = corpo ? strstr(corpo, "\"meta\"") : NULL;
  const char *lk;
  char v[32] = "";
  if (n) dst[0] = 0;
  if (!m || n < 4) return;
  m += 6;
  while (*m == ' ' || *m == ':' || *m == '\n' || *m == '\t' || *m == '\r') m++;
  if (*m == '{') {
    if (!js_texto_raiz_em(m, NULL, "imdb_id", v, sizeof v) || strncmp(v, "tt", 2))
      if (!js_texto_raiz_em(m, NULL, "imdbId", v, sizeof v) || strncmp(v, "tt", 2))
        js_texto_raiz_em(m, NULL, "id", v, sizeof v);
    if (!strncmp(v, "tt", 2) && strlen(v) >= 5) { snprintf(dst, n, "%s", v); return; }
  }
  lk = strstr(corpo, "imdb.com/title/tt");
  if (lk) {
    size_t k = 0;
    lk = strstr(lk, "/tt") + 1;                 // o "tt..." que vem depois de /title/
    while (lk[k] && lk[k] != '/' && lk[k] != '"' && lk[k] != '?' && k + 1 < n && k < 20) {
      dst[k] = lk[k]; k++;
    }
    dst[k] = 0;
    if (k < 5) dst[0] = 0;
  }
}

// Cinemeta como fonte de complemento: /meta/<tipo>/<tt>.json, com a MESMA chave
// de cache que o caminho antigo usa (o buscarEps de sempre reaproveita a
// resposta se a ficha do catalogo nao servir). `tipoItem` incerto pergunta
// serie e, sem temporada, filme (desc_meta_tipos).
static char *cinemetaMeta(const char *tipoItem, const char *tt, int *ehFilme) {
  const char *tipos[2];
  int nTipos = desc_meta_tipos(tipoItem, tipos), ti;
  char *corpo = NULL;
  for (ti = 0; ti < nTipos; ti++) {
    char chave[96];
    int ultimo = ti == nTipos - 1;
    free(corpo);
    corpo = NULL;
    desc_meta_chave(chave, sizeof chave, tipos[ti], tt);
    corpo = metaCacheObter(chave);
    if (!corpo) {
      corpo = metaprov_meta(tipos[ti], tt, 10, NULL);   // Nuvio, depois Cinemeta
      if (!corpo) { if (ultimo) break; continue; }
      metaCacheGuardar(chave, corpo);
    }
    if (ehFilme) *ehFilme = strcmp(tipos[ti], "series") != 0;
    if (ultimo || desc_meta_tem_temporadas(corpo)) break;
  }
  if (corpo && !metaValida(corpo)) { free(corpo); corpo = NULL; }
  return corpo;
}

// ARM (arm.haglund.dev): converte id de anime em IMDb. Publica e sem chave; o
// unico dado enviado e o numero do id. "kitsu:41370" -> tt9335498. Cache no
// mesmo metaCache (chave "arm/<id>"). Vazio quando nao ha mapeamento.
static void armImdb(const char *id, char *dst, size_t n) {
  static const struct { const char *pref, *fonte; } MAPA[] = {
    { "kitsu:", "kitsu" }, { "mal:", "myanimelist" }, { "anilist:", "anilist" },
    { "anidb:", "anidb" }, { "myanimelist:", "myanimelist" },
  };
  char chave[96], num[24], url[200], *c;
  size_t k = 0, i;
  const char *fonte = NULL;
  if (n) dst[0] = 0;
  for (i = 0; i < sizeof MAPA / sizeof *MAPA; i++)
    if (!strncmp(id, MAPA[i].pref, strlen(MAPA[i].pref))) {
      fonte = MAPA[i].fonte;
      id += strlen(MAPA[i].pref);
      break;
    }
  if (!fonte) return;
  while (id[k] >= '0' && id[k] <= '9' && k + 1 < sizeof num) { num[k] = id[k]; k++; }
  num[k] = 0;
  if (!k) return;
  snprintf(chave, sizeof chave, "arm/%s:%s", fonte, num);
  c = metaCacheObter(chave);
  if (!c) {
    if (metaNegAtiva(chave)) return;
    snprintf(url, sizeof url, "https://arm.haglund.dev/api/v2/ids?source=%s&id=%s", fonte, num);
    c = rede_baixar(url, 8);
    if (!c) { metaNegGuardar(chave); return; }
    metaCacheGuardar(chave, c);
  }
  { char tt[32] = "";
    if (js_texto_raiz(c, "imdb", tt, sizeof tt) && !strncmp(tt, "tt", 2))
      snprintf(dst, n, "%s", tt); }
  free(c);
}

// Tenta a ficha do addon `i`; guarda em `mf` se valida. Um pedido, no tipo
// declarado (ou no do item).
static int tentarFichaDoAddon(MetaFontes *mf, int i, const char *tipoItem, const char *id) {
  const char *cand[3];
  int nc = 0, k;
  char *c = NULL;
  if (tipoItem && tipoItem[0]) cand[nc++] = tipoItem;
  if (!tipoItem || strcmp(tipoItem, "series")) cand[nc++] = "series";
  if (!tipoItem || strcmp(tipoItem, "movie")) cand[nc++] = "movie";
  // O primeiro tipo que o manifesto nao recusa. Sem nada declarado, o do item.
  for (k = 0; k < nc; k++) if (addons_aceita_id(i, cand[k], id) != 0) break;
  if (k >= nc) return 0;
  c = metaDoAddonT(i, cand[k], id, 8);
  if (!c) return 0;
  if (!metaValida(c)) { free(c); return 0; }
  printf("[desc] ficha do addon %s\n", addons_nome(i));
  fflush(stdout);
  metaFontesAdd(mf, c, addons_nome(i), 0);
  return 1;
}

// Catalogo primeiro vale para este item? Nao para canal, nem com o ajuste
// "Usar sempre o Cinemeta". Id do IMDb so quando o item veio de um addon que
// nao e o Cinemeta (senao a origem JA e o Cinemeta e nada muda).
static int catalogoPrimeiro(const CatItem *c) {
  if (!c || !c->imdb[0] || ehCanal(c->tipo) || ajustes_meta_so_cinemeta()) return 0;
  if (strncmp(c->imdb, "tt", 2)) return 1;
  return addonDaOrigem(c->origem) >= 0;
}

// Resolve as fontes na ordem acima. Devolve 1 quando o detalhe deve seguir o
// caminho do catalogo: ha ficha de um addon, ou (id que nao e do IMDb) ao menos
// a do Cinemeta achada pelo mapeamento. 0 = nada util; quem chama segue o
// caminho antigo (Cinemeta para "tt", nada para o resto).
static int metaCatalogoResolver(const CatItem *orig, MetaFontes *mf) {
  char id[64];
  int origem = addonDaOrigem(orig->origem), i, n = addons_n(), extras = 0;
  int ehImdb = !strncmp(orig->imdb, "tt", 2);
  memset(mf, 0, sizeof *mf);
  idbase_copiar(orig->imdb, id, sizeof id);
  // a) quem publicou
  if (origem >= 0) tentarFichaDoAddon(mf, origem, orig->tipo, id);
  // b) outros addons de metadados, na ordem do usuario
  for (i = 0; i < n && extras < 2 && (!mf->n || !metaCompleta(mf->corpo[0])); i++) {
    int r;
    if (i == origem) continue;
    r = addons_aceita_id(i, orig->tipo, id);
    if (r == 1 || (r == -1 && ehImdb && addons_sondado(i) && addons_fornece(i, ADD_META))) {
      if (tentarFichaDoAddon(mf, i, orig->tipo, id)) extras++;
    }
  }
  // O IMDb do titulo: o proprio id, ou o que a ficha revelou.
  if (ehImdb) snprintf(mf->tt, sizeof mf->tt, "%s", id);
  else for (i = 0; i < mf->n && !mf->tt[0]; i++) metaImdbDaFicha(mf->corpo[i], mf->tt, sizeof mf->tt);
  // d) sem IMDb a vista e ficha incompleta: a API ARM
  if (!mf->tt[0] && !ehImdb && (!mf->n || !metaCompleta(mf->corpo[0])))
    armImdb(id, mf->tt, sizeof mf->tt);
  // c) o Cinemeta completa o que faltar (ou e a base, se nenhum addon serviu)
  if (mf->tt[0]) {
    int fil = 1;
    char tp[16] = "";
    char *cm;
    if (mf->n) metaTipoProprio(mf->corpo[0], tp, sizeof tp);
    cm = cinemetaMeta(tp[0] ? tp : orig->tipo, mf->tt, &fil);
    if (cm) {
      printf("[desc] ficha do catalogo Nuvio/Cinemeta (%s)\n", mf->n ? "complemento" : "por mapeamento");
      fflush(stdout);
      metaFontesAdd(mf, cm, "Cinemeta", 1);
      if (mf->n == 1) mf->ehFilme = fil;
    }
  }
  if (!mf->n) return 0;
  // Tipo da base. A ficha manda; depois o do item; por ultimo, ter episodios.
  { char tp[16];
    metaTipoProprio(mf->corpo[0], tp, sizeof tp);
    if (tp[0]) mf->ehFilme = !strcmp(tp, "movie");
    else if (!strcmp(orig->tipo, "movie")) mf->ehFilme = 1;
    else if (!strcmp(orig->tipo, "series")) mf->ehFilme = 0;
    else mf->ehFilme = !desc_meta_tem_temporadas(mf->corpo[0]);
  }
  // Id do IMDb so com Cinemeta na base nao muda o caminho antigo.
  if (ehImdb && mf->cine[0]) return 0;
  return 1;
}

// Elenco/direcao/generos/nota/pais a partir de uma ficha, so nos campos que
// `d` ainda tem vazios (`soVazios`=1) ou todos os que a ficha traz (0).
static void fichaDe(CatItem *d, const char *corpo, int soVazios) {
  const char *c = js_array(corpo, NULL, "cast");
  if (!soVazios || !d->nElenco) {
    int k = 0;
    while (c && k < CAT_ELENCO_MAX) {
      size_t n2 = 0;
      const char *p2 = c;
      if (*p2 != '"') break;
      p2++;
      while (*p2 && *p2 != '"' && n2 + 1 < sizeof d->elenco[k].nome) d->elenco[k].nome[n2++] = *p2++;
      d->elenco[k].nome[n2] = 0;
      d->elenco[k].papel[0] = 0;
      d->elenco[k].foto[0] = 0;
      d->elenco[k].tmdb = 0;
      k++;
      p2++;
      while (*p2 == ' ') p2++;
      c = (*p2 == ',') ? p2 + 1 : NULL;
      while (c && *c == ' ') c++;
    }
    if (k) d->nElenco = k;
  }
  if (!soVazios || !d->direcao[0]) {
    const char *dr = js_array(corpo, NULL, "director");
    if (dr && *dr == '"') {
      size_t n2 = 0;
      dr++;
      while (*dr && *dr != '"' && n2 + 1 < sizeof d->direcao) d->direcao[n2++] = *dr++;
      d->direcao[n2] = 0;
    }
  }
  { char lista[160];
    generosDe(corpo, lista, sizeof lista, strcmp(d->tipo, "movie") ? "series" : "movie");
    // O genero que o catalogo trouxe ("Programa de TV") e o rotulo do tipo, nao
    // um genero: so vale como "ja tem" se veio de uma ficha.
    if (lista[0] && (!soVazios || !d->genero[0])) snprintf(d->genero, sizeof d->genero, "%s", lista); }
  if (!soVazios || d->nota <= 0) {
    double nota = js_num(corpo, NULL, "imdbRating", 0.0);
    if (nota > 0.0) {
      int n10 = (int)(nota * 10.0 + 0.5);
      if (n10 > 99) n10 /= 10;
      d->nota = n10;
    }
  }
  if (!soVazios || !d->pais[0]) js_texto(corpo, NULL, "country", d->pais, sizeof d->pais);
}

// As fontes de COMPLEMENTO (a partir da segunda) preenchem o que a base deixou
// vazio: ficha e sinopse/fundo/logo (completarRaso, que so age em item sem sinopse).
static void completarFicha(CatItem *d, const MetaFontes *mf, const char *tipo) {
  int i;
  for (i = 0; i < mf->n; i++) {
    // A sinopse por si so: completarRaso exige poster na ficha (deMeta), e uma
    // ficha de addon sem poster ainda tem a descricao.
    if (!d->sinopse[0]) {
      char tit[160], sin[900];
      if (metaTextos(mf->corpo[i], tit, sizeof tit, sin, sizeof sin) && sin[0])
        snprintf(d->sinopse, sizeof d->sinopse, "%s", sin);
    }
    if (!i) continue;                    // a base ja foi lida por buscarEps
    fichaDe(d, mf->corpo[i], 1);
    completarRaso(d, mf->corpo[i], tipo);
  }
}

// Episodios do caminho do catalogo. A lista da BASE manda; sem nenhum episodio
// nela, a da primeira fonte que tem. Mescla episodio a episodio (still, data,
// duracao, so vazios) apenas quando as duas listas falam o mesmo id — IMDb.
// Devolve 1 quando os episodios publicados sao de um addon (o TMDB, depois, so
// preenche o que faltar neles). Lista de ARQUIVOS mantida devolve 0: o nome
// dela nao e texto que valha proteger da traducao.
static int episodiosDoCatalogo(int alvoItem, const char *titulo, const char *serie,
                               const MetaFontes *mf, TempsPub *tp) {
  int i, fonte = -1, outro = -1, mesmoId = idbase_e_imdb(serie);
  int nEp[META_FONTES_MAX], arq[META_FONTES_MAX];
  for (i = 0; i < mf->n; i++) {
    int brutos = 0;
    nEp[i] = metaContarEpisodios(mf->corpo[i], &brutos);
    arq[i] = metaListaDeArquivos(nEp[i], brutos);
    if (nEp[i] > 0 && fonte < 0) fonte = i;
  }
  if (fonte < 0) return 0;
  // LISTA DE ARQUIVOS NA BASE (addon de fontes que tambem publica catalogo): a
  // base manda no texto da ficha, mas a lista dela so fica quando conhece MAIS
  // episodios distintos que as outras fontes. Senao vale a maior lista de
  // verdade. So entre listas do mesmo espaco de ids (item do IMDb): "kitsu:1"
  // e uma temporada, e a serie inteira do Cinemeta nao a substitui.
  if (arq[fonte] && mesmoId) {
    int melhor = -1;
    for (i = 0; i < mf->n; i++)
      if (i != fonte && !arq[i] && nEp[i] >= nEp[fonte] && (melhor < 0 || nEp[i] > nEp[melhor]))
        melhor = i;
    if (melhor >= 0) {
      printf("[desc] %s: %s respondeu uma lista de arquivos com %d episodios; "
             "usando os %d de %s\n", titulo, mf->nome[fonte], nEp[fonte], nEp[melhor],
             mf->nome[melhor]);
      fflush(stdout);
      fonte = melhor;
    }
  }
  // Mesmo espaco de ids: a lista do IMDb (Cinemeta) sobre uma de addon so faz
  // sentido quando o item tambem e do IMDb. Procura o Cinemeta entre TODAS as
  // outras fontes: parar na primeira com episodios deixava a lista sem
  // complemento quando havia outro addon no meio.
  for (i = 0; mesmoId && i < mf->n && outro < 0; i++)
    if (i != fonte && nEp[i] > 0 && mf->cine[i]) outro = i;
  // Lista de arquivos que ficou (sabe mais episodios): o nome dela e nome de
  // arquivo, entao o do Cinemeta entra por cima nos episodios que os dois tem.
  publicarEpisodios(mf->corpo[fonte], alvoItem, titulo,
                    outro >= 0 ? mf->corpo[outro] : NULL,
                    arq[fonte] ? DESC_MESCLA_TEXTO : DESC_MESCLA_VAZIOS, tp);
  arte_reserva_episodios(serie, mf->corpo[fonte]);
  // Lista de arquivos nao e "texto do addon" (como em episodiosDoAddon): com 1
  // o TMDB so preencheria vazios e os nomes de arquivo que sobraram ficariam.
  return !mf->cine[fonte] && !arq[fonte];
}

// PRE-BUSCA QUE CEDE. O fio de episodios e um so (epItem, epAlvoId...): uma
// pre-busca do carrossel em voo fazia o pedido do titulo que chegou ficar
// guardado (pendItem) ate ela acabar. Quem pre-busca marca epPreQuer; quando um
// pedido de verdade chega com o fio ocupado por ela, epCancelar sobe e ela
// larga nos pontos de parada abaixo (no maximo uma viagem de rede depois).
static volatile int epPreQuer, epRodaPre, epCancelar;
static int preCede(const char *id) {
  if (!(epRodaPre && epCancelar)) return 0;
  printf("[desc] pre-busca de %s cedeu ao titulo em cena\n", id);
  fflush(stdout);
  return 1;
}

static void *buscarEps(void *u) {
  int alvoItem = epItem;
  const CatItem *orig = cat_item(alvoItem);
  CatItem base;
  const CatItem *it;
  char *corpo = NULL;
  char serie[64];
  char meuId[64];
  MetaFontes mf;
  int viaCatalogo = 0, solto = 0;
  (void)u;
  memset(&mf, 0, sizeof mf);
  if (!orig || !orig->imdb[0]) { fioEpVivo = 0; return NULL; }
  if (preCede(orig->imdb)) { fioEpVivo = 0; return NULL; }
  snprintf(epAlvoId, sizeof epAlvoId, "%s", orig->imdb);
  snprintf(meuId, sizeof meuId, "%s", orig->imdb);
  // COPIA AGORA (#190): o ponteiro de cat_item so vale ate o fim do quadro, e
  // este fio vai para a rede antes de ler o resto dele.
  base = *orig;
  orig = &base;
  // CANAL NAO PASSA AQUI. O Cinemeta so conhece filme/serie por id do IMDb
  // ("tt..."); id de canal e "cs:channel:<hash>". "ehFilme = tipo != series"
  // tratava canal como filme, pedia /meta/movie/<id> com um id que o
  // Cinemeta nunca teve, E o corte no primeiro ':' (abaixo, ao montar
  // `serie`) reduzia TODO canal a "cs" — a mesma chave de cache para todos,
  // entao o primeiro canal aberto "vazava" elenco/genero/nota para os
  // seguintes. Medido: nenhum canal tem tipo "movie" nem "series" (#37).
  if (ehCanal(orig->tipo)) { fioEpVivo = 0; return NULL; }
  // PERSONAL SERVER ITEM: the server is the only source of its page. No
  // Cinemeta, TMDB or addon /meta gets an opaque server id.
  if (jfid_e(orig->imdb)) {
    CatEp *eps = malloc(sizeof(CatEp) * VIDEOS_MAX);
    CatItem ed = base;
    int ne = eps ? servidores_ficha(&ed, eps, VIDEOS_MAX) : -1;
    if (ne >= 0 && (alvoItem = epAlvo(alvoItem)) >= 0) {
      cat_atualizar_item(alvoItem, &ed);
      if (ne > 0) cat_definir_episodios(alvoItem, eps, ne);
    }
    free(eps);
    fioEpVivo = 0;
    return NULL;
  }
  // FILME TAMBEM PASSA AQUI. O /meta/movie traz elenco, direcao, generos e
  // nota — antes so os titulos enriquecidos no catalogo tinham elenco, e a
  // pagina do filme abria sem a fileira. O que e so de serie (episodios,
  // temporadas) e pulado abaixo.
  //
  // O Cinemeta so conhece id do IMDb. "kitsu:123"/"mal:456" (addons de anime)
  // eram cortados no ':' e pedidos como /meta/movie/kitsu.json — e "kitsu"
  // virava a chave de cache de TODOS eles, o mesmo vazamento do #37.
  //
  // CATALOGO PRIMEIRO (metaCatalogo, logo acima): a ficha do addon que PUBLICOU o
  // item, e o Cinemeta so como complemento. Devolve 0 quando nao ha nada
  // util — ai o caminho de sempre: Cinemeta para "tt", nada para o resto.
  if (catalogoPrimeiro(orig)) viaCatalogo = metaCatalogoResolver(orig, &mf);
  if (!viaCatalogo && strncmp(orig->imdb, "tt", 2)) {
    metaFontesLiberar(&mf);
    fioEpVivo = 0;
    return NULL;
  }
  // TIPO INCERTO ("anime" de catalogo do AIOMetadata, ou qualquer outro que
  // nao seja filme nem serie) NAO VIRA FILME POR PADRAO: pergunta como serie
  // e, sem temporada nenhuma, como filme. O que o /meta responder decide, e
  // o tipo resolvido e gravado no item (a pagina, os extras e o TMDB leem
  // dele). Ver desc_meta_tipos.
  const char *tipos[2];
  int nTipos = desc_meta_tipos(orig->tipo, tipos), ti, ehFilme = 1;
  base = *orig;
  it = &base;
  // idbase_copiar: "kitsu:41370" fica inteiro (cortar no primeiro ':' dava
  // "kitsu"); para o IMDb e o mesmo corte de sempre.
  idbase_copiar(it->imdb, serie, sizeof serie);

  if (viaCatalogo) {
    corpo = strdup(mf.corpo[0]);   // o resto do fio libera `corpo` como sempre
    ehFilme = mf.ehFilme;
  } else
  for (ti = 0; ti < nTipos; ti++) {
    char chave[96];
    int ultimo = ti == nTipos - 1;
    // A CHAVE DO CACHE LEVA O TIPO. Era so o id, e /meta/movie/tt13293588 e
    // /meta/series/tt13293588 sao titulos DIFERENTES no Cinemeta: aberto uma
    // vez como filme, o corpo errado ficava no cache e a abertura seguinte,
    // ja como serie, lia "meta do cache" com 0 episodios (log 2043).
    desc_meta_chave(chave, sizeof chave, tipos[ti], serie);
    free(corpo);
    corpo = metaCacheObter(chave);
    marco(corpo ? "episodios: meta do cache" : "episodios: baixando meta");
    if (!corpo) {
      corpo = metaprov_meta(tipos[ti], serie, 25, NULL);   // Nuvio, depois Cinemeta
      if (!corpo) { if (ultimo) break; continue; }
      metaCacheGuardar(chave, corpo);
    }
    ehFilme = strcmp(tipos[ti], "series") != 0;
    if (ultimo || desc_meta_tem_temporadas(corpo)) break;
  }
  if (!corpo || preCede(meuId)) { metaFontesLiberar(&mf); free(corpo); fioEpVivo = 0; return NULL; }
  if (nTipos > 1) {
    const char *resolvido = ehFilme ? "movie" : "series";
    printf("[desc] %s: tipo '%s' do catalogo resolvido como '%s' pelo /meta\n",
           it->titulo, orig->tipo, resolvido);
    snprintf(base.tipo, sizeof base.tipo, "%s", resolvido);
    // Id do TMDB so vale com o tipo certo (/movie/94664 e /tv/94664 sao
    // obras diferentes); o /find abaixo o resolve de novo pelo tipo novo.
    base.tmdb = 0;
  }
  // QUEM MANDA NO TEXTO (#176). Com "Prefere a ficha do addon de metadados"
  // (ajustes_meta_externo, que nenhum codigo lia) o addon manda; com o app num
  // idioma que nao e o ingles e sem o TMDB para traduzir, o addon localizado
  // tambem e a unica fonte que tem o texto na lingua da pessoa. Com o TMDB
  // ligado e sem a preferencia, e ele quem traduz (mais abaixo).
  int externo = ajustes_meta_externo();
  int tmdbTraduz = desc_chave_tmdb()[0] && ajustes_tmdb_basico();
  // Na ficha do catalogo o texto JA e o do addon; a preferencia nao tem o que trocar.
  int aplicar = !viaCatalogo && (externo || (idiomaNaoIngles() && !tmdbTraduz));
  int epsDoAddon = 0;
  TempsPub tp = {0};
  if (!ehFilme && viaCatalogo)
    epsDoAddon = episodiosDoCatalogo(alvoItem, it->titulo, serie, &mf, &tp);
  else if (!ehFilme) {
    int nCine = publicarEpisodios(corpo, alvoItem, it->titulo, NULL, 0, &tp);
    // Temporada e data de cada episodio para a reserva do still: quando o
    // TMDB divide a serie em outras temporadas (One Piece), e por elas que o
    // still do TMDB e achado (artereserva.h).
    arte_reserva_episodios(serie, corpo);
    // A disputa e de distintos contra distintos, os dois do corpo INTEIRO: o
    // nCine publicado para em VIDEOS_MAX, e contra ele uma lista com os mesmos
    // 1300 episodios "sabia mais" (1300 > 1200).
    if (nCine >= VIDEOS_MAX) nCine = desc_meta_n_episodios(corpo);
    epsDoAddon = episodiosDoAddon(alvoItem, serie, it->titulo, corpo, nCine, aplicar, &tp);
  }
  // O MAPA DE EPISODIOS VISTOS NAO E PEDIDO AQUI, e essa linha existe para dizer
  // por que: extras.c JA baixa /shows/<id>/progress/watched ao abrir o titulo,
  // e agora alimenta vistoep de la. Uma versao deste arquivo chegou a pedir de
  // novo — duas requisicoes identicas por titulo, para a segunda sobrescrever a
  // primeira com o mesmo dado.
  // A MESMA resposta traz elenco, direcao e a lista de temporadas. Buscar de
  // novo para cada uma seria tres viagens ao mesmo lugar.
  {
    CatItem edit = *it;
    int manter = 0;
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
    { char lista[160];
      generosDe(corpo, lista, sizeof lista, ehFilme ? "movie" : "series");
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
    // Temporadas presentes, sem repetir e em ordem: as da lista publicada
    // (TempsPub, #372); o corpo so quando nada foi publicado.
    if (tp.n > 0) {
      edit.nTemporadas = tp.n;
      memcpy(edit.temporadas, tp.t, sizeof(int) * (size_t)tp.n);
    } else
    { const char *v = js_array(corpo, NULL, "videos");
      edit.nTemporadas = 0;
      while (v) {
        const char *fv = js_fim(v);
        int t2 = videoTemporada(v, fv);
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
    completarRaso(&edit, corpo, ehFilme ? "movie" : "series");
    if (viaCatalogo) completarFicha(&edit, &mf, ehFilme ? "movie" : "series");
    // TITULO, SINOPSE E GENEROS DO ADDON DE METADADOS (#176). So campo que o
    // addon preencheu troca o do Cinemeta; o que ele nao tem fica como estava.
    if (aplicar) {
      char tA[160], sA[900];
      const char *nomeA = "";
      const char *tipoA = ehFilme ? "movie" : "series";
      if (textoDoAddon(tipoA, serie, tA, sizeof tA, sA, sizeof sA, &nomeA)) {
        char *cA = NULL;
        int i3, n3 = addons_n();
        // O TMDB NUM IDIOMA QUE NAO E O INGLES AINDA TRADUZ POR CIMA (#209). A
        // preferencia e ligada de fabrica (e preferExternalMetaAddonDetail=true
        // no web), e o addon aqui e o PRIMEIRO que declara "meta" — Ultra MAX,
        // Bingecat, que falam ingles. Travar o TMDB deixava a serie em ingles
        // num app e num TMDB em portugues. No web (metaDetailsScreen.js) o
        // "Titulo e sinopse" do TMDB vence a ficha de qualquer addon; o vazio
        // do TMDB (sem traducao) continua deixando o texto do addon.
        int tmdbPorCima = tmdbTraduz && idiomaNaoIngles();
        snprintf(edit.titulo, sizeof edit.titulo, "%s", tA);
        if (!tmdbPorCima) manter |= DESC_MANTER_TITULO;
        if (sA[0]) {
          snprintf(edit.sinopse, sizeof edit.sinopse, "%s", sA);
          if (!tmdbPorCima) manter |= DESC_MANTER_SINOPSE;
        }
        // Generos do mesmo addon (o Cinemeta so tem em ingles).
        for (i3 = 0; i3 < n3 && !cA; i3++) {
          char *c5 = metaDoAddon(i3, tipoA, serie);
          char lista[160];
          if (!c5) continue;
          if (metaTextos(c5, tA, sizeof tA, NULL, 0)) {
            generosDe(c5, lista, sizeof lista, tipoA);
            if (lista[0]) snprintf(edit.genero, sizeof edit.genero, "%s", lista);
            cA = c5;
          } else free(c5);
        }
        free(cA);
        printf("[desc] %s: texto do addon %s\n", edit.titulo, nomeA);
        fflush(stdout);
      }
    }
    // Publica texto, generos e temporadas antes do enriquecimento de imagens.
    if ((alvoItem = epAlvo(alvoItem)) >= 0) cat_atualizar_item(alvoItem, &edit);
    marco("detalhe: meta basico na tela");
    // A FICHA ESTA NA TELA: ESTE FIO JA NAO SEGURA O PROXIMO TITULO (R2). O que
    // resta (fotos do elenco e as notas por temporada do TMDB, uma viagem por
    // temporada: 20+ numa serie longa, segundos) so ENFEITA a pagina. Com
    // fioEpVivo ligado ate o fim, abrir outro titulo nesse meio (uma
    // recomendacao dos detalhes) deixava o pedido da ficha dele guardado em
    // pendItem ate a cauda inteira acabar — pagina aberta e vazia. A cauda
    // escreve pelo id guardado em meuId (epAlvoDe), nao pelo global.
    solto = 1;
    fioEpVivo = 0;
    { char idBase[24];
      const char *dp;
      snprintf(idBase, sizeof idBase, "%s", it->imdb);
      dp = strchr(idBase, ':');
      if (dp) *(char *)dp = 0;
      // O TMDB casa por IMDb. Id de outro espaco ("kitsu:41370") ficaria sem
      // elenco de fotos, e de proposito nao usa o "tt" mapeado: o entry do Kitsu
      // e UMA temporada, e o titulo/sinopse do TMDB da serie inteira
      // sobrescreveriam os dela.
      if (idbase_e_imdb(it->imdb))
        fotosDoElenco(&edit, idBase, !strcmp(it->tipo, "series"), manter);
      else {
        // TVDB / TMDB (resultado de busca de addon: Ted Lasso pelo Spotlight
        // chegava como "tvdb:383203", MEDIDO na TCL 05/10/2026, e saia sem foto
        // e sem clique no elenco). Esses ids sao da SERIE INTEIRA, entao o "tt"
        // que a propria ficha revela vale. O Kitsu continua de fora (acima).
        char tt[32] = "";
        if (strncmp(it->imdb, "kitsu:", 6)) metaImdbDaFicha(corpo, tt, sizeof tt);
        if (tt[0]) {
          printf("[desc] elenco %s: fotos pelo IMDb da ficha (%s)\n", it->imdb, tt); fflush(stdout);
          // ARTE PELO MESMO IMDb: o item de busca com id de fora traz so o
          // POSTER, e a pagina abria com ele esticado no fundo (sem fundo largo
          // nenhum para trocar). O metahub monta as URLs pelo "tt", como o
          // hero ja faz (artehero.c).
          if (!edit.backdrop[0] || !strcmp(edit.backdrop, edit.poster)) {
            snprintf(edit.backdrop, sizeof edit.backdrop, "https://images.metahub.space/background/medium/%s/img", tt);
            printf("[desc] %s: fundo pelo IMDb da ficha\n", it->imdb); fflush(stdout);
          }
          if (!edit.logo[0])
            snprintf(edit.logo, sizeof edit.logo, "https://images.metahub.space/logo/medium/%s/img", tt);
          fotosDoElenco(&edit, tt, !strcmp(it->tipo, "series"), manter);
        } else { printf("[desc] elenco %s: id fora do IMDb, sem fotos (meta pedido como %s)\n", it->imdb, serie); fflush(stdout); }
      } }
    // AS ABAS NAO SAO DESTA CAUDA (#372). Ela roda com fioEpVivo ja solto: o
    // mesmo titulo reaberto (bloco de episodios reciclado) publica outra lista
    // com outras abas, e republicar o `edit` inteiro devolvia as abas velhas
    // (T1 com 72 episodios em T1-T3: T2/T3 carregados e escondidos). As abas
    // ficam as que estao no item; o resto (elenco, arte, texto) e desta cauda.
    if (alvoItem >= 0 && (alvoItem = epAlvoDe(alvoItem, meuId)) >= 0) cat_atualizar_item_sem_abas(alvoItem, &edit);
    printf("[desc] %s: %d atores, dir='%s', %d temporadas\n",
           edit.titulo, edit.nElenco, edit.direcao, edit.nTemporadas);
    fflush(stdout);

    // NOTA POR EPISODIO (issue #87): o Cinemeta nao tem voto por episodio, o
    // TMDB tem — uma viagem por temporada presente na lista, nao uma por
    // episodio. edit.tmdb ja foi resolvido por fotosDoElenco; se o /find dela
    // falhou (rede), resolve-se aqui pelo mesmo /find. "Titulo e sinopse" e a
    // porta: quem desligou o TMDB nao quer este trafego. Republica so se alguma nota entrou.
    if (!ehFilme && ajustes_tmdb_basico() && idbase_e_imdb(serie)) {
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
      if (chave2[0] && tmdbId > 0 && alvoItem >= 0 && (alvoItem = epAlvoDe(alvoItem, meuId)) >= 0) {
        int neps = cat_n_episodios(alvoItem);
        if (neps > 0) {
          CatEp *tmp = malloc(sizeof(CatEp) * (size_t)neps);
          if (tmp) {
            int preenchidas = 0, i2;
            // Sinopse sempre (#150). O NOME so num idioma que nao e o ingles
            // (#176): em ingles o do Cinemeta ja e o do TMDB. Com o texto do
            // addon na lista, o TMDB so preenche o que ficou vazio.
            int textosEp = DESC_EPT_SINOPSE | (idiomaNaoIngles() ? DESC_EPT_NOME : 0) |
                           (epsDoAddon ? DESC_EPT_SO_VAZIO : 0);
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
                  preenchidas += desc_tmdb_notas_temporada_ex(c4, tmp, neps, s, textosEp);
                  free(c4);
                } }
            }
            { int marcados = 0;
              for (i2 = 0; i2 < neps; i2++)
                if (tmp[i2].tmdbE > 0) {
                  const CatEp *e0 = cat_episodio(alvoItem, i2);
                  tmp[i2].tmdbSerie = tmdbId;
                  if (e0 && (e0->tmdbSerie != tmdbId || e0->tmdbE != tmp[i2].tmdbE)) marcados++;
                }
              if (marcados > 0 && preenchidas == 0) preenchidas = -marcados; }
            if (preenchidas != 0 && (alvoItem = epAlvoDe(alvoItem, meuId)) >= 0 &&
                cat_n_episodios(alvoItem) == neps) {
              cat_definir_episodios(alvoItem, tmp, neps);
              printf("[desc] %s: nota/sinopse TMDB em %d episodios\n",
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
  metaFontesLiberar(&mf);
  if (!solto) fioEpVivo = 0;   // solto antes: um fio mais novo pode estar vivo
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
static char     vtBase[NV_ADDON_URL_MAX], vtTipo[8], vtCat[96], vtGenre[96];
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
  char url[NV_ADDON_PEDIDO_MAX], base[NV_ADDON_URL_MAX], type[8], id[96], genre[96], encoded[290], *corpo;
  int raw=0, ocultos=0, skip, cap, prov;unsigned generation;
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
          if (deMetaTmdb(p,f,padrao,&lote[nl])) {
            if (descOcultaNaoLancado(&lote[nl])) ocultos++; else nl++;
          }
        }
        if (!strcasecmp(fonte.tmdbTipo,"COLLECTION")||
            !strcasecmp(fonte.tmdbTipo,"PERSON")||
            !strcasecmp(fonte.tmdbTipo,"DIRECTOR")) semMais=1;
        else {
          long tp=(long)js_num(corpo,NULL,"total_pages",0);
          long pg=(long)js_num(corpo,NULL,"page",skip);
          if (!tp||pg>=tp||(!nl&&!ocultos)) semMais=1;
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
            if (deMetaTrakt(p,f,fonte.midia,&lote[nl])) {
              if (descOcultaNaoLancado(&lote[nl])) ocultos++; else nl++;
            }
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
    { int added=ocultos;   // pagina so de futuros nao encerra a paginacao
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
  { int w;
  if(genre[0])w=nv_addon_url(url,sizeof url,base,"/catalog/%s/%s/genre=%s&skip=%d.json",type,id,encoded,skip);
  else if(skip)w=nv_addon_url(url,sizeof url,base,"/catalog/%s/%s/skip=%d.json",type,id,skip);
  else w=nv_addon_url(url,sizeof url,base,"/catalog/%s/%s.json",type,id);
  corpo=(!addons_base_desligada(base)&&descPedidoCoube(base,w,sizeof url))?rede_baixar(url,10):NULL; }
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
      // Escondido ainda conta como "pagina com novidade": senao uma pagina so de
      // futuros encerraria a paginacao do catalogo.
      if (descOcultaNaoLancado(&it)) { added++; continue; }
      origemDaBase(base, it.origem, sizeof it.origem);
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
  if (fioEpVivo) {
    pendItem = indiceItem; pendTemp = temporada;
    if (epRodaPre) epCancelar = 1;   // a pre-busca em voo cede (ver preCede)
    return;
  }
  // A lista agora e UNICA e cobre todas as temporadas, entao ter qualquer
  // episodio deste titulo ja basta — trocar de aba nao pede nada.
  (void)temporada;
  if (cat_n_episodios(indiceItem) > 0) return;
  epItem = indiceItem; epTemp = temporada;
  epRodaPre = epPreQuer; epPreQuer = 0; epCancelar = 0;
  fioEpVivo = 1;
  if (pthread_create(&fioEp, NULL, buscarEps, NULL) != 0) fioEpVivo = 0;
  else pthread_detach(fioEp);
}

// Ponto unico das entradas (detalhe, player, retomada) e do roteador de app.c:
// pede a lista de episodios do titulo se ela falta. Mesma regra e argumentos do
// roteador: serie, tipo incerto, ou filme sem elenco (o fio traz o /meta).
// Sem pedido duplicado: lista ja carregada ou fio ja a caminho do item = nada.
void desc_episodios_garantir(int indiceItem) {
  const CatItem *ci = cat_item(indiceItem);
  if (!ci || !ci->imdb[0]) return;
  if (!(!strcmp(ci->tipo, "series") || strcmp(ci->tipo, "movie") || ci->nElenco == 0)) return;
  if (cat_n_episodios(indiceItem) > 0 || desc_episodios_carregando(indiceItem)) return;
  desc_episodios(indiceItem, 0);
}

// PRE-BUSCA do carrossel (detail.c): a mesma regra de desc_episodios_garantir,
// mas NUNCA enfileira nem toma o lugar de um pedido de verdade. Fio em voo ou
// pedido guardado = -1 (tente de novo); nada a pedir = 0; pedido saiu = 1.
int desc_episodios_precarregar(int indiceItem) {
  const CatItem *ci = cat_item(indiceItem);
  if (!ci || !ci->imdb[0]) return 0;
  if (!(!strcmp(ci->tipo, "series") || strcmp(ci->tipo, "movie") || ci->nElenco == 0)) return 0;
  if (cat_n_episodios(indiceItem) > 0) return 0;
  if (fioEpVivo || pendItem >= 0) return -1;
  epPreQuer = 1;
  desc_episodios(indiceItem, 0);
  epPreQuer = 0;
  return fioEpVivo ? 1 : 0;
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
// SEMENTE (R2): o que o clique ja sabia do titulo (nome, ano, cartaz). Com ela
// a pagina abre NA HORA, com o layout normal, e o buscarEps preenche o resto.
static CatItem sobSemente;
static int  sobTemSemente;

static void *buscarTitulo(void *arg) {
  char url[200], id[24], *corpo;
  int achou = -1, passo;
  (void)arg;
  snprintf(id, sizeof id, "%s", sobId);

  // O credito de um ator chega com o id do TMDB, nao com o do IMDb — o
  // combined_credits nao traz imdb_id. `external_ids` faz a traducao, e e uma
  // chamada so, feita apenas quando o dono abre o credito.
  if (sobTmdb > 0) {
    // TRADUCAO DE ID NAO E INTEGRACAO (#187), a mesma regra da reserva de arte:
    // com o ajuste TMDB desligado a chave era "" e toda estrela do Explorar
    // (o mapa monta com a reserva) morria em "sem imdb" sem pedido nenhum.
    const char *chave = desc_chave_tmdb()[0] ? desc_chave_tmdb() : desc_chave_tmdb_reserva();
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
    // Com a semente, o /meta NAO e esperado aqui: o titulo entra no catalogo
    // so com o que o clique sabia, a pagina abre, e o buscarEps (disparado na
    // abertura) busca a ficha uma vez so — e a cache dele evita repetir.
    if (sobTemSemente) {
      CatItem s = sobSemente;
      snprintf(s.imdb, sizeof s.imdb, "%s", id);
      s.tmdb = sobTmdb;
      sobIndice = cat_acrescentar(&s);
      printf("[desc] sob demanda tmdb %ld -> %s com semente, indice %d\n", sobTmdb, id, sobIndice);
      fflush(stdout);
      sobFioVivo = 0;
      return NULL;
    }
  }

  for (passo = 0; passo < 2 && achou < 0; passo++) {
    const char *tipo = passo ? "series" : "movie";
    corpo = metaprov_meta(tipo, id, 20, NULL);   // Nuvio, depois Cinemeta
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

static void pedirTmdb(long tmdbId, const char *tipo, int semente) {
  if (tmdbId <= 0 || sobFioVivo) return;
  sobTemSemente = semente;
  sobTmdb = tmdbId;
  snprintf(sobTipo, sizeof sobTipo, "%s", tipo ? tipo : "movie");
  sobId[0] = 0;
  sobIndice = -1;
  sobFioVivo = 1;
  if (pthread_create(&sobFio, NULL, buscarTitulo, NULL) != 0) sobFioVivo = 0;
  else pthread_detach(sobFio);
}
void desc_pedir_titulo_tmdb(long tmdbId, const char *tipo) { pedirTmdb(tmdbId, tipo, 0); }

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
  sobTemSemente = 0;
  snprintf(sobId, sizeof sobId, "%s", id);
  sobTmdb = 0;
  sobIndice = -1;
  sobFioVivo = 1;
  if (pthread_create(&sobFio, NULL, buscarTitulo, NULL) != 0) sobFioVivo = 0;
  else pthread_detach(sobFio);
}

// Como desc_pedir_titulo/_tmdb, mas com o que o clique ja sabia. Sem cartaz ou
// sem nome cai no caminho de sempre (a pagina espera o /meta).
//   imdb "tt...": o titulo entra no catalogo AGORA e o roteador abre no proximo
//   quadro. `tipo` e o do clique ("movie"/"series"; "tv" tambem serve) e DEVE
//   ser certo: o catalogo recusa atualizar um item de filme para serie
//   (cat_atualizar_item compara tipo_base), entao "incerto" nao e opcao aqui.
//   tmdb > 0: um pedido so (external_ids) para achar o IMDb, depois o mesmo.
void desc_pedir_titulo_semente(const char *imdb, long tmdb, const char *tipo,
                               const char *titulo, const char *ano, const char *poster) {
  CatItem s;
  char id[24];
  const char *dp;
  if (sobFioVivo) return;
  if (!titulo || !*titulo || !poster || !*poster || !tipo ||
      (strcmp(tipo, "tv") && strcmp(tipo, "series") && strcmp(tipo, "movie"))) {
    if (tmdb > 0) desc_pedir_titulo_tmdb(tmdb, tipo);
    else desc_pedir_titulo(imdb);
    return;
  }
  memset(&s, 0, sizeof s);
  snprintf(s.titulo, sizeof s.titulo, "%s", titulo);
  snprintf(s.meta, sizeof s.meta, "%.4s", ano ? ano : "");
  snprintf(s.poster, sizeof s.poster, "%s", poster);
  if (strlen(poster) < sizeof s.backdrop)   // cortado seria 404 (#361)
    snprintf(s.backdrop, sizeof s.backdrop, "%s", poster);
  if (tipo && (!strcmp(tipo, "tv") || !strcmp(tipo, "series"))) snprintf(s.tipo, sizeof s.tipo, "series");
  else if (tipo && !strcmp(tipo, "movie")) snprintf(s.tipo, sizeof s.tipo, "movie");
  if (s.tipo[0]) snprintf(s.genero, sizeof s.genero, "%s", i18n(rotuloTipoSing(s.tipo)));
  s.tmdb = tmdb > 0 ? tmdb : 0;
  if (tmdb > 0) {
    sobSemente = s;
    pedirTmdb(tmdb, tipo, 1);
    return;
  }
  if (!imdb || imdb[0] != 't') { desc_pedir_titulo(imdb); return; }
  dp = strchr(imdb, ':');
  { size_t k = dp ? (size_t)(dp - imdb) : strlen(imdb);
    if (k >= sizeof id) k = sizeof id - 1;
    memcpy(id, imdb, k); id[k] = 0; }
  { int j = cat_indice_por_imdb(id);
    if (j >= 0) { sobIndice = j; return; } }
  snprintf(s.imdb, sizeof s.imdb, "%s", id);
  sobIndice = cat_acrescentar(&s);
  printf("[desc] sob demanda %s com semente, indice %d\n", id, sobIndice);
  fflush(stdout);
}

int desc_titulo_pronto(void) { int v = sobIndice; sobIndice = -1; return v; }
int desc_titulo_buscando(void) { return sobFioVivo; }

// Idioma dos textos do TMDB. Antes era so o idioma da interface; agora a
// conta pode escolher outro em Ajustes -> Integracoes (tmdb_language), e
// "Da interface" continua sendo o padrao — ver ajustes_tmdb_idioma().
const char *desc_tmdb_idioma(void) { return ajustes_tmdb_idioma(); }
