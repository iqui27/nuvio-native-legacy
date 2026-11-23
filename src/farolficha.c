// Ficha do titulo — ver farolficha.h.
// Puro: sem SDL, sem rede, sem dados.h, sem malloc. O arquivo do cache usa
// so stdio, pelo caminho que o chamador escolhe (dados_dir() na integracao).
#include "farolficha.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FICHA_MAGIC "FFICHA\x00\x01"   // 6 letras + versao 0.1, 8 bytes
#define FICHA_HDR   12                 // magic(8) + n u16 + tam u16

static uint16_t clamp16(long v) { return v < 0 ? 0 : (v > 65535 ? 65535 : (uint16_t)v); }
static uint8_t  clamp8(long v)  { return v < 0 ? 0 : (v > 255 ? 255 : (uint8_t)v); }

// "en" -> 'e'<<8|'n'. So vale ISO 639-1 de duas letras; o resto vira 0.
static uint16_t idiomaPack(const char *s) {
  if (!s || !isalpha((unsigned char)s[0]) || !isalpha((unsigned char)s[1])) return 0;
  return (uint16_t)((tolower((unsigned char)s[0]) << 8) | tolower((unsigned char)s[1]));
}

// Acrescenta `id` num vetor de int32 sem repetir. Devolve o novo n.
static int addId(int32_t *v, int n, int max, int32_t id) {
  int i;
  if (!id || n >= max) return n;
  for (i = 0; i < n; i++) if (v[i] == id) return n;
  v[n] = id;
  return n + 1;
}

int64_t farolficha_chave(const FarolFicha *f) {
  if (!f) return 0;
  if (f->tmdb > 0) return ((int64_t)f->tipo << 32) | (uint32_t)f->tmdb;
  if (f->reserva > 0) return -(((int64_t)f->tipo << 32) | (uint32_t)f->reserva);
  return 0;
}

// Reserva sem TMDB, em DOIS espacos que nao se tocam: o bit 30 ligado marca
// "hash de identificador" — o imdb do item ("tt..."), que ja e a identidade
// estavel que a reserva precisa, e tambem os ids dos servidores de midia
// ("jf:<srv>.<id>", "em:...", "px:..."), que nunca sao vazios e distinguem
// duas obras de mesmo titulo e ano. O bit 30 apagado e o hash de
// titulo+ano, o ultimo recurso quando nao ha id nenhum. Nenhum dos dois
// espacos encosta no das chaves POSITIVAS de TMDB (farolficha_chave).
static int32_t reservaDeId(const char *id) {
  return (int32_t)(0x40000000u | (mapa_hash_titulo(id) & 0x3fffffffu));
}

static int32_t reservaDeTitulo(const char *titulo, int ano) {
  unsigned h = mapa_hash_titulo(titulo) ^ ((unsigned)ano * 2654435761u);
  h &= 0x3fffffffu;
  return (int32_t)(h ? h : 1);
}

// ---------------------------------------------------------------------------
// LEITURA

int farolficha_ler_tmdb(const char *json, int serie, FarolFichaTmp *tmp, FarolFicha *f) {
  int i;
  if (!json || !tmp || !f || !mapa_ler_ids(json, serie, tmp)) return 0;
  memset(f, 0, sizeof *f);
  f->tmdb    = (int32_t)tmp->tmdb;
  f->tipo    = serie ? FF_SERIE : FF_FILME;
  f->ano     = clamp16(tmp->ano);
  f->idioma  = idiomaPack(tmp->idioma);
  f->minutos = clamp16(tmp->minutos);
  f->nota    = clamp8(tmp->nota);
  f->votos   = clamp16(tmp->votos);
  for (i = 0; i < tmp->nGen && i < MAPA_GEN_MAX; i++)
    f->nGenero = (uint8_t)addId(f->genero, f->nGenero, FAROL_FICHA_GEN, (int32_t)tmp->gen[i].id);
  for (i = 0; i < tmp->nGente && i < MAPA_GENTE_MAX; i++)
    f->nPessoa = (uint8_t)addId(f->pessoa, f->nPessoa, FAROL_FICHA_PESSOA, (int32_t)tmp->gente[i].id);
  for (i = 0; i < tmp->nKw && i < MAPA_KW_MAX; i++)
    f->nTema = (uint8_t)addId(f->tema, f->nTema, FAROL_FICHA_TEMA, (int32_t)tmp->kw[i].id);
  f->fonte = FF_DE_TMDB;
  // Detalhe sem id do TMDB mas com imdb (raro, mas a reserva precisa de uma
  // identidade): a chave fica o hash do imdb, como no caminho do catalogo.
  if (!f->tmdb && tmp->imdb[0]) f->reserva = reservaDeId(tmp->imdb);
  return 1;
}

// O campo `genero` do CatItem tem DOIS formatos e o catalogo nao diz qual:
//   "Filme  ·  Crime  ·  Drama" — pacote, Cinemeta/descoberta e Trakt: o
//     PRIMEIRO trecho e o rotulo do tipo (ja com i18n() aplicado), que a
//     telas descartam e mapa_generos_locais pula;
//   "Drama" | "Drama · Crime" — Jellyfin, Emby e Plex: a lista crua, sem
//     rotulo nenhum, onde o primeiro trecho JA e genero.
// O rotulo so pode ser o primeiro trecho: ele sendo palavra de tipo o
// formato e o do pacote e ele sai; nao sendo, a lista inteira e de generos
// — "Drama" sozinho nao pode virar nGenero 0.
static int igualSemCaixa(const char *a, const char *b) {
  for (; *a && *b; a++, b++)
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
  return !*a && !*b;
}

// Os rotulos possiveis: as fontes ("Filme", "Programa de TV", "Série") e o
// que i18n() devolve nos idiomas distribuidos (idioma_*.h), mais os ingleses
// que produtores e versoes antigas gravam (biblioteca.c:ehPalavraDeTipo). Se
// entrar idioma novo o rotulo vira um genero A MAIS, nunca um a menos.
static int ehRotuloTipo(const char *seg) {
  static const char *T[] = {
    "Filme", "Movie", "Film", "Filmas", "Película", "Phim", "Ταινία",
    "Филм", "Фильм", "Фільм", "فيلم", "映画", "电影", "電影",
    "Programa de TV", "TV Show", "Dizi", "Emisiune TV", "Phim bộ",
    "Serial", "Serial TV", "Serialas", "Serie", "Serie TV", "Serija",
    "Seriál", "Sorozat", "TV-Sendung", "TV-program", "Tv-program",
    "Émission de TV", "Σειρά", "Сериал", "Телепередача", "Телепрограма",
    "برنامج تلفزيوني", "テレビ番組", "电视节目", "電視節目",
    "Série", "Series", "Show", "Серіал", "シリーズ", "剧集", "劇集"
  };
  size_t i;
  for (i = 0; i < sizeof T / sizeof T[0]; i++)
    if (igualSemCaixa(seg, T[i])) return 1;
  return 0;
}

int farolficha_ler_item(const CatItem *ci, FarolFicha *f) {
  char buf[160], *p;
  int i, primeiro = 1;
  if (!ci || !f) return 0;
  memset(f, 0, sizeof *f);
  f->tipo = !strcmp(ci->tipo, "series") ? FF_SERIE : FF_FILME;
  if (ci->tmdb > 0) f->tmdb = (int32_t)ci->tmdb;
  else if (!strncmp(ci->imdb, "tmdb:", 5)) f->tmdb = (int32_t)atol(ci->imdb + 5);
  f->ano  = clamp16(mapa_ano_meta(ci->meta));
  f->nota = clamp8(ci->nota);
  // Dois formatos de `genero` (ver ehRotuloTipo): o primeiro trecho so sai
  // quando e o rotulo do tipo; a lista crua do servidor entra inteira. Cada
  // nome vira mapa_hash_nome — a convencao de id negativo da reserva local.
  snprintf(buf, sizeof buf, "%s", ci->genero);
  for (p = buf; p && *p && f->nGenero < FAROL_FICHA_GEN; ) {
    char *sep = strstr(p, "\xc2\xb7"), *tok;
    if (sep) *sep = 0;
    tok = p;
    while (*tok == ' ') tok++;
    { size_t k = strlen(tok); while (k && tok[k - 1] == ' ') tok[--k] = 0; }
    if (tok[0] && !(primeiro && ehRotuloTipo(tok)))
      f->nGenero = (uint8_t)addId(f->genero, f->nGenero, FAROL_FICHA_GEN,
                                  (int32_t)mapa_hash_nome(tok));
    primeiro = 0;
    p = sep ? sep + 2 : NULL;
  }
  // direcao: nomes separados por virgula, como na reserva do mapa.
  if (ci->direcao[0]) {
    char buf[128], *p, *q;
    snprintf(buf, sizeof buf, "%s", ci->direcao);
    for (p = buf; p && *p; p = q) {
      q = strchr(p, ',');
      if (q) *q++ = 0;
      while (*p == ' ') p++;
      { size_t k = strlen(p); while (k && p[k - 1] == ' ') p[--k] = 0; }
      if (*p)
        f->nPessoa = (uint8_t)addId(f->pessoa, f->nPessoa, FAROL_FICHA_PESSOA,
                                    (int32_t)mapa_hash_nome(p));
    }
  }
  for (i = 0; i < ci->nElenco && f->nPessoa < FAROL_FICHA_PESSOA; i++)
    if (ci->elenco[i].nome[0])
      f->nPessoa = (uint8_t)addId(f->pessoa, f->nPessoa, FAROL_FICHA_PESSOA,
            (int32_t)(ci->elenco[i].tmdb > 0 ? ci->elenco[i].tmdb
                                            : mapa_hash_nome(ci->elenco[i].nome)));
  f->fonte = FF_DE_CAT;
  // Identidade sem TMDB: o id que o item ja traz — imdb ("tt...") ou o do
  // servidor ("jf:"/"em:"/"px:"), estavel e unico por obra; senao titulo +
  // ano, para dois "Duna" nao dividirem a ficha. reservaDeId e
  // reservaDeTitulo mantem os dois espacos separados pelo bit 30.
  if (!f->tmdb)
    f->reserva = ci->imdb[0] ? reservaDeId(ci->imdb)
                           : reservaDeTitulo(ci->titulo, f->ano);
  return farolficha_chave(f) != 0;
}

// ---------------------------------------------------------------------------
// CACHE LRU — vetor ordenado: [0] mais recente, [n-1] na porta de saida.

void farolficha_cache_iniciar(FarolFichaCache *c) {
  if (!c) return;
  c->n = 0;
  c->sujo = 0;
}

static int procurar(const FarolFichaCache *c, int64_t chave) {
  int i;
  for (i = 0; i < c->n; i++) if (farolficha_chave(&c->f[i]) == chave) return i;
  return -1;
}

// Traz f[i] para a frente, empurrando o que estava na frente um para tras.
static void promover(FarolFichaCache *c, int i) {
  FarolFicha t;
  if (i <= 0) return;
  t = c->f[i];
  memmove(&c->f[1], &c->f[0], (size_t)i * sizeof t);
  c->f[0] = t;
}

int farolficha_obter(FarolFichaCache *c, int64_t chave, FarolFicha *dst) {
  int i;
  if (!c || !chave) return 0;
  i = procurar(c, chave);
  if (i < 0) return 0;
  if (dst) *dst = c->f[i];
  promover(c, i);
  c->sujo = 1;   // a ordem muda; a proxima gravacao leva a ordem nova
  return 1;
}

void farolficha_guardar(FarolFichaCache *c, const FarolFicha *f) {
  int64_t chave;
  int i;
  if (!c || !f) return;
  chave = farolficha_chave(f);
  if (!chave) return;
  i = procurar(c, chave);
  if (i >= 0) {
    c->f[i] = *f;
    promover(c, i);
  } else {
    if (c->n < FAROL_FICHA_MAX) c->n++;
    memmove(&c->f[1], &c->f[0], (size_t)(c->n - 1) * sizeof *f);  // a mais velha sai
    c->f[0] = *f;
  }
  c->sujo = 1;
}

// ---------------------------------------------------------------------------
// ARQUIVO — "FFICHA01": magic(8) + n u16 + tam u16 + n fichas de 96 bytes,
// na ordem do cache. Sem cabecalho por ficha: a chave sai dos campos.
// O formato mora no par escrever/ler_mem: arquivo e so um dos transportes —
// quem grava a partir de copia (correcao FAROL38D) usa o par em memoria.

size_t farolficha_escrever(const FarolFichaCache *c, unsigned char *dst,
                           size_t cap) {
  size_t n;
  if (!c || !dst) return 0;
  n = (size_t)FICHA_HDR + (size_t)c->n * sizeof(FarolFicha);
  if (cap < n) return 0;
  memcpy(dst, FICHA_MAGIC, 8);
  dst[8] = (unsigned char)(c->n & 0xff);
  dst[9] = (unsigned char)(c->n >> 8);
  dst[10] = (unsigned char)(sizeof(FarolFicha) & 0xff);
  dst[11] = (unsigned char)(sizeof(FarolFicha) >> 8);
  if (c->n) memcpy(dst + FICHA_HDR, c->f, (size_t)c->n * sizeof(FarolFicha));
  return n;
}

int farolficha_ler_mem(FarolFichaCache *c, const unsigned char *b, size_t n) {
  unsigned qtd = 0, tam = 0, i;
  const unsigned char *p;
  if (!c || !b) return 0;
  farolficha_cache_iniciar(c);
  if (n < FICHA_HDR || memcmp(b, FICHA_MAGIC, 8)) return 0;
  qtd = (unsigned)b[8] | ((unsigned)b[9] << 8);
  tam = (unsigned)b[10] | ((unsigned)b[11] << 8);
  // tamanho tem de fechar EXATO: lixo no fim e corte sao a mesma corrupcao.
  if (qtd > FAROL_FICHA_MAX || tam != sizeof(FarolFicha) ||
      n != (size_t)FICHA_HDR + (size_t)qtd * tam) return 0;
  p = b + FICHA_HDR;
  for (i = 0; i < qtd; i++, p += tam) {
    int64_t ch;
    int j;
    memcpy(&c->f[c->n], p, sizeof(FarolFicha));
    ch = farolficha_chave(&c->f[c->n]);
    if (!ch) continue;
    // Duas fichas com a mesma chave: o gravador nao produz isso, entao o
    // arquivo esta corrompido e vale o descarte como qualquer outro.
    for (j = 0; j < c->n; j++)
      if (farolficha_chave(&c->f[j]) == ch) break;
    if (j < c->n) break;
    c->n++;
  }
  if (i != qtd) { c->n = 0; return 0; }
  return 1;
}

int farolficha_ler_arquivo(FarolFichaCache *c, const char *caminho) {
  unsigned char hdr[FICHA_HDR];
  unsigned n = 0, tam = 0, i, ok = 0;
  FILE *fp;
  if (!c || !caminho) return 0;
  farolficha_cache_iniciar(c);
  fp = fopen(caminho, "rb");
  if (!fp) return 0;
  do {
    if (fread(hdr, 1, FICHA_HDR, fp) != FICHA_HDR) break;
    if (memcmp(hdr, FICHA_MAGIC, 8)) break;
    n = (unsigned)hdr[8] | ((unsigned)hdr[9] << 8);
    tam = (unsigned)hdr[10] | ((unsigned)hdr[11] << 8);
    if (n > FAROL_FICHA_MAX || tam != sizeof(FarolFicha)) break;
    for (i = 0; i < n; i++) {
      int64_t ch;
      int j;
      if (fread(&c->f[c->n], 1, sizeof(FarolFicha), fp) != sizeof(FarolFicha)) break;
      ch = farolficha_chave(&c->f[c->n]);
      if (!ch) continue;
      // Duas fichas com a mesma chave: o gravador nao produz isso, entao o
      // arquivo esta corrompido e vale o descarte como qualquer outro.
      for (j = 0; j < c->n; j++)
        if (farolficha_chave(&c->f[j]) == ch) break;
      if (j < c->n) break;
      c->n++;
    }
    if (i != n) break;
    // Lixo no fim tambem e corrupcao: o arquivo tem que terminar aqui.
    if (fgetc(fp) != EOF) { c->n = 0; break; }
    ok = 1;
  } while (0);
  fclose(fp);
  if (!ok) {
    c->n = 0;
    unlink(caminho);   // corrompido: fora, para nao ser lido de novo
    return 0;
  }
  return 1;
}

int farolficha_gravar(FarolFichaCache *c, const char *caminho) {
  unsigned char hdr[FICHA_HDR];
  char tmp[512];
  FILE *fp;
  int ok = 0;
  if (!c || !caminho) return 0;
  snprintf(tmp, sizeof tmp, "%s.tmp", caminho);
  fp = fopen(tmp, "wb");
  if (!fp) return 0;
  memcpy(hdr, FICHA_MAGIC, 8);
  hdr[8] = (unsigned char)(c->n & 0xff);
  hdr[9] = (unsigned char)(c->n >> 8);
  hdr[10] = (unsigned char)(sizeof(FarolFicha) & 0xff);
  hdr[11] = (unsigned char)(sizeof(FarolFicha) >> 8);
  if (fwrite(hdr, 1, FICHA_HDR, fp) == FICHA_HDR &&
      (!c->n || fwrite(c->f, sizeof(FarolFicha), c->n, fp) == c->n))
    ok = !fclose(fp);
  else { fclose(fp); ok = 0; }
  if (!ok) unlink(tmp);
  else if (rename(tmp, caminho)) { unlink(tmp); ok = 0; }
  if (ok) c->sujo = 0;
  return ok;
}
