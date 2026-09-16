// Frases (Wikiquote) e ficha de producao (Wikidata). Ver seriefrases.h para O
// QUE existe e o que foi medido e descartado; aqui esta o custo e o parse.
//
// --- ORCAMENTO DE REDE --------------------------------------------------------
//
//   QUANDO — so em seriefrases_abrir(), chamado quando a pessoa ENTRA na secao.
//     Abrir a pagina do titulo nao dispara nada.
//
//   QUANTO — 2 pedidos por titulo, no maximo:
//     1) UMA consulta ao Wikidata (SPARQL) que traz de uma vez os fatos JA COM
//        rotulo em portugues E o endereco da pagina do Wikiquote. MEDIDO:
//        0,7 a 1,5 KB de resposta e ~750 ms. A alternativa obvia
//        (wbgetentities do item inteiro) devolve 252 KB para o mesmo titulo —
//        foi medida e recusada por isso, e ainda precisaria de um segundo
//        pedido so para traduzir os Q-id em nomes.
//     2) UM parse=wikitext da pagina do Wikiquote, quando ela existe.
//     Titulo sem pagina no Wikiquote custa 1 pedido; titulo sem item no
//     Wikidata custa 1 e acaba ali.
//
//   CACHE — `frases-<imdb>.txt` em dados_dir(), validade 30 DIAS. Wikiquote e
//     Wikidata mudam em escala de meses, e a coisa mais cara aqui e a viagem,
//     nao o dado. O CACHE NEGATIVO E GRAVADO TAMBEM: sem ele, todo titulo que
//     nao tem nada (a maioria das series, ver a medida em seriefrases.h) pagaria
//     dois pedidos a cada visita para descobrir de novo que nao tem nada.
//
//   ABANDONO — seriefrases_fechar() e conferido entre os dois pedidos.
//
// --- DESENHO ------------------------------------------------------------------
// So gfx_* e texto, pelo mesmo motivo de serieaud.c: o orcamento de texturas ja
// vive encostado no teto na TV do reporter.
#include "seriefrases.h"
#include "rede.h"
#include "js.h"
#include "dados.h"
#include "text.h"
#include "layout.h"
#include "idioma.h"
#include "ajustes.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SF_TEXTO 260
#define SF_QUEM   56
#define SF_VALIDADE_DIAS 30
#define SF_ARQ_V "# nuvio seriefrases v1"

typedef struct { char quem[SF_QUEM]; char texto[SF_TEXTO]; } SfFrase;
typedef struct { char rotulo[40]; char valor[200]; } SfFato;

static SfFrase frases[SF_FRASE_MAX];
static SfFato  fatos[SF_FATO_MAX];
static int     nFrases, nFatos, selecionado;
static char    paginaWq[200];
static int     emPortugues;
static char    imdbAtual[24];

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static char imdbPedido[24], imdbEmCurso[24];
static int  fioVivo, abandonar;
static pthread_t fio;

// --- UTIL --------------------------------------------------------------------

static void soImdb(char *dst, unsigned tam, const char *s) {
  unsigned k = 0;
  if (!s) { if (tam) dst[0] = 0; return; }
  while (s[k] && s[k] != ':' && k + 1 < tam) { dst[k] = s[k]; k++; }
  dst[k] = 0;
}

// Percent-encoding. Existe porque a consulta vai NA URL e tem aspas, chaves,
// colchetes e acentos — nada disso sobrevive cru num GET.
static void urlenc(char *dst, size_t tam, const char *s) {
  static const char HEX[] = "0123456789ABCDEF";
  size_t k = 0;
  for (; *s && k + 4 < tam; s++) {
    unsigned char c = (unsigned char)*s;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      dst[k++] = (char)c;
    } else {
      dst[k++] = '%'; dst[k++] = HEX[c >> 4]; dst[k++] = HEX[c & 15];
    }
  }
  dst[k] = 0;
}

// Valor de uma variavel do resultado SPARQL. O corpo e
//   {"head":…,"results":{"bindings":[{"qe":{"type":"uri","value":"…"},…}]}}
// e um js_texto(corpo, NULL, "value") pegaria o PRIMEIRO "value" do documento,
// que e o de outra variavel. Aqui a faixa e recortada no objeto da variavel
// pedida antes de procurar.
// O ESPACO IMPORTA: o endpoint responde JSON EMBELEZADO — `"qe" : {` com
// espacos dos dois lados dos dois-pontos. A primeira versao daqui procurava
// `"qe":{` cru e nao achava NADA, em nenhum titulo, contra o servidor de
// verdade; o teste passava porque o fixture tinha sido gerado compacto. Por
// isso a busca aqui pula espaco, e o fixture do teste agora e a resposta como
// ela chega no fio.
static int sparqlCampo(const char *corpo, const char *var, char *dst, size_t tam) {
  char chave[24];
  const char *p = corpo;
  size_t n;
  if (tam) dst[0] = 0;
  if (!corpo) return 0;
  snprintf(chave, sizeof chave, "\"%s\"", var);
  n = strlen(chave);
  while ((p = strstr(p, chave)) != NULL) {
    const char *q = p + n;
    while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') q++;
    if (*q != ':') { p += n; continue; }
    q++;
    while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') q++;
    if (*q != '{') { p += n; continue; }
    return js_texto(q, js_fim(q), "value", dst, tam) && dst[0];
  }
  return 0;
}

// "185000000" -> "185 mi". Sem casa decimal acima de 100 milhoes: a terceira
// casa de uma bilheteria nao diz nada a 3 m de distancia.
static void dinheiro(char *dst, size_t tam, double v, const char *simbolo) {
  if (v >= 1000000000.0) snprintf(dst, tam, i18n("%s %.2f bi"), simbolo, v / 1e9);
  else if (v >= 1000000.0) snprintf(dst, tam, i18n("%s %.0f mi"), simbolo, v / 1e6);
  else snprintf(dst, tam, i18n("%s %.0f mil"), simbolo, v / 1e3);
}

// Simbolo da moeda pelo Q-id da unidade. So as tres que aparecem de fato; para
// qualquer outra o FATO E OMITIDO em vez de sair um numero sem unidade —
// "185.000.000" sem moeda e pior que nao mostrar nada.
static const char *moeda(const char *unidade) {
  if (strstr(unidade, "Q4917"))  return "US$";
  if (strstr(unidade, "Q4916"))  return "€";
  if (strstr(unidade, "Q25224")) return "£";
  return NULL;
}

// Deixa no maximo `max` itens de uma lista separada por " · ". As listas do
// Wikidata sao longas (Breaking Bad tem onze premios com rotulo em portugues) e
// o painel tem quatro linhas: sem este corte, o limite de linhas do txt_bloco
// engolia o ULTIMO item pela metade. Foi o que a primeira captura mostrou —
// "vencedores do MTV Movie Award" sem o resto do nome do premio.
static void primeirosItens(char *s, int max) {
  char *p = s;
  int n = 1;
  while ((p = strstr(p, " \xc2\xb7 ")) != NULL) {
    if (++n > max) { *p = 0; return; }
    p += 3;
  }
}

// Quando o valor ainda assim nao cabe no campo, CORTA NO SEPARADOR — nunca no
// meio da palavra. Uma lista com menos itens continua verdadeira; meio nome de
// premio nao.
static void addFato(const char *rotulo, const char *valor) {
  char *d;
  size_t tam;
  if (nFatos >= SF_FATO_MAX || !valor || !valor[0]) return;
  snprintf(fatos[nFatos].rotulo, sizeof fatos[nFatos].rotulo, "%s", rotulo);
  d = fatos[nFatos].valor;
  tam = sizeof fatos[nFatos].valor;
  if (strlen(valor) >= tam) {
    char *sep;
    memcpy(d, valor, tam - 1);
    d[tam - 1] = 0;
    sep = strstr(d, " \xc2\xb7 ");
    if (sep) {
      char *prox = sep;
      while ((prox = strstr(prox + 1, " \xc2\xb7 ")) != NULL) sep = prox;
      *sep = 0;
    }
  } else {
    snprintf(d, tam, "%s", valor);
  }
  nFatos++;
}

// --- LIMPEZA DE WIKITEXT -----------------------------------------------------
//
// Nao e um parser de MediaWiki e nao pretende ser. O alvo e uma linha de
// dialogo, e o que estraga uma linha de dialogo e sempre a mesma meia duzia de
// marcacoes: [[ligação|texto]], {{modelo}}, ''italico''/'''negrito''',
// <ref>…</ref> e as rubricas entre colchetes ([pausa]).
// `tam` e o tamanho de `s`, e NAO ha corte silencioso: quem chama compara o
// comprimento depois e descarta a fala longa demais em vez de mostrar meia
// frase. A primeira versao truncava aqui dentro e o resultado aparecia na tela
// como "…os que est o f" — uma frase cortada no meio de uma palavra.
static void limparWiki(char *s, size_t tam) {
  char saida[1400];
  size_t k = 0, i = 0, n = strlen(s);
  while (i < n && k + 1 < sizeof saida) {
    if (s[i] == '[' && s[i + 1] == '[') {
      // [[alvo|texto]] fica com o texto; [[alvo]] fica com o alvo.
      const char *fim = strstr(s + i, "]]");
      const char *bar = NULL, *q;
      size_t ini;
      if (!fim) break;
      for (q = s + i + 2; q < fim; q++) if (*q == '|') bar = q;
      ini = (size_t)((bar ? bar + 1 : s + i + 2) - s);
      while (ini < (size_t)(fim - s) && k + 1 < sizeof saida) saida[k++] = s[ini++];
      i = (size_t)(fim - s) + 2;
      continue;
    }
    if (s[i] == '{' && s[i + 1] == '{') {
      const char *fim = strstr(s + i, "}}");
      if (!fim) break;
      i = (size_t)(fim - s) + 2;
      continue;
    }
    if (s[i] == '<') {
      const char *fim;
      // <ref>…</ref> vai INTEIRO, conteudo junto. Tirar so as etiquetas deixa o
      // texto da nota grudado na fala ("…agora" + "x" = "agorax"), que foi
      // exatamente o que o teste pegou.
      if (!strncmp(s + i, "<ref", 4)) {
        fim = strstr(s + i, "</ref>");
        if (fim) { i = (size_t)(fim - s) + 6; continue; }
      }
      fim = strchr(s + i, '>');
      if (!fim) break;
      i = (size_t)(fim - s) + 1;
      continue;
    }
    if (s[i] == '[') {                 // rubrica: [pausa], [para Gordon]
      const char *fim = strchr(s + i, ']');
      if (!fim) break;
      i = (size_t)(fim - s) + 1;
      continue;
    }
    if (s[i] == '\'' && s[i + 1] == '\'') {
      while (i < n && s[i] == '\'') i++;
      continue;
    }
    saida[k++] = s[i++];
  }
  saida[k] = 0;
  // Colapsa espaco e apara.
  { size_t j = 0, w = 0;
    while (saida[j] == ' ') j++;
    for (; saida[j]; j++) {
      if (saida[j] == ' ' && w > 0 && saida[j - 1] == ' ') continue;
      saida[w++] = saida[j];
    }
    while (w > 0 && (saida[w - 1] == ' ' || saida[w - 1] == '\t')) w--;
    saida[w] = 0; }
  // Aspas em volta da frase inteira: a pagina portuguesa envolve toda fala em
  // aspas, e o cartao ja apresenta a frase como frase. Duas camadas de aspas
  // saem na tela como ""assim"".
  { char *a = saida;
    size_t w = strlen(a);
    if (w >= 2 && a[0] == '"' && a[w - 1] == '"') { a[w - 1] = 0; a++; }
    snprintf(s, tam, "%s", a); }
}

// O wikitexto vem como UMA string JSON gigante em parse.wikitext.* — dezenas de
// KB. Nao cabe em buffer de pilha e js_texto nao serve (copia para tamanho
// fixo), entao a string e DESESCAPADA NO PROPRIO BUFFER da resposta: os \n
// escapados viram quebras de verdade e o texto fica pronto para ser lido linha
// a linha. Devolve o inicio do wikitexto dentro de `corpo`, ou NULL.
//
// A busca da chave pula espaco em vez de cravar `"*":"`. O MediaWiki responde
// compacto hoje, mas isso e configuracao do servidor, e o endpoint do Wikidata
// ja provou o contrario: la a resposta vem embelezada, e a primeira versao do
// leitor de SPARQL, que cravava o formato, nao achava NADA contra o servidor de
// verdade enquanto passava no teste.
static char *desescapar(char *corpo) {
  char *w = strstr(corpo, "\"wikitext\"");
  char *le, *es;
  if (w) w = strstr(w, "\"*\"");
  if (!w) return NULL;
  w += 3;
  while (*w == ' ' || *w == '\n' || *w == '\r' || *w == '\t') w++;
  if (*w != ':') return NULL;
  w++;
  while (*w == ' ' || *w == '\n' || *w == '\r' || *w == '\t') w++;
  if (*w != '"') return NULL;
  le = es = ++w;
  while (*le) {
    if (*le == '\\' && le[1] == 'n') { *es++ = '\n'; le += 2; continue; }
    if (*le == '\\' && le[1] == '"') { *es++ = '"';  le += 2; continue; }
    if (*le == '\\' && le[1] == '\\') { *es++ = '\\'; le += 2; continue; }
    if (*le == '\\' && le[1] == 't') { *es++ = ' ';  le += 2; continue; }
    // \uXXXX VIRA UTF-8 DE VERDADE, e nao um espaco. O js.h troca \u por espaco
    // de proposito (la os textos sao para exibicao e vem cheios de emoji), mas
    // aqui isso APAGOU TODOS OS ACENTOS: a pagina portuguesa do Matrix saiu
    // como "Eu sei que voc esta ai fora". O MediaWiki escapa todo caractere
    // fora do ASCII, entao sem esta conversao o pt-wikiquote e ilegivel.
    //
    // A codificacao e feita NO PROPRIO BUFFER: \uXXXX ocupa 6 bytes e o UTF-8
    // de um caractere do BMP ocupa no maximo 3, entao o texto so encolhe.
    // O par substituto (emoji, fora do BMP) vira espaco — nao ha fala de filme
    // que dependa disso, e juntar os dois meios exigiria olhar para frente.
    if (*le == '\\' && le[1] == 'u') {
      unsigned v = 0;
      int d;
      for (d = 0; d < 4; d++) {
        char c = le[2 + d];
        if (c >= '0' && c <= '9') v = v * 16 + (unsigned)(c - '0');
        else if ((c | 32) >= 'a' && (c | 32) <= 'f') v = v * 16 + (unsigned)((c | 32) - 'a' + 10);
        else { v = 0xFFFF; break; }
      }
      le += 6;
      if (v >= 0xD800 && v <= 0xDFFF) { *es++ = ' '; continue; }
      if (v < 0x80) { *es++ = (char)v; }
      else if (v < 0x800) {
        *es++ = (char)(0xC0 | (v >> 6));
        *es++ = (char)(0x80 | (v & 0x3F));
      } else {
        *es++ = (char)(0xE0 | (v >> 12));
        *es++ = (char)(0x80 | ((v >> 6) & 0x3F));
        *es++ = (char)(0x80 | (v & 0x3F));
      }
      continue;
    }
    if (*le == '"') break;
    *es++ = *le++;
  }
  *es = 0;
  return w;
}

// Secoes que NAO sao personagem. Sem esta lista, o "quem" de metade das frases
// de filme sairia como "Taglines" ou "Cast".
static int secaoDeFala(const char *t) {
  static const char *NAO[] = { "Taglines", "Cast", "External links", "See also",
                               "About", "Quotes about", "Songs", "Contents",
                               "Main", "Seasons", NULL };
  int k;
  if (!t[0]) return 0;
  for (k = 0; NAO[k]; k++) if (!strncmp(t, NAO[k], strlen(NAO[k]))) return 0;
  return 1;
}

// Extrai ate `max` candidatos do wikitext. Dois formatos convivem no Wikiquote
// e os dois aparecem nos titulos medidos:
//   FILME  `== Personagem ==` seguido de `* fala`   (Dark Knight, Matrix)
//   SERIE  `: '''Personagem''': fala`               (Severance, The Bear)
// Devolve quantos achou.
static int extrairFalas(const char *wikitexto, SfFrase *saida, int max) {
  const char *p = wikitexto;
  char secao[SF_QUEM] = "";
  // secaoValida acompanha o cabecalho de NIVEL 2, e uma subsecao NAO o
  // reescreve. MEDIDO na pagina do Breaking Bad: ela e um indice, e a unica
  // coisa parecida com dialogo esta em `=== Dialogue ===` DENTRO de
  // `== About ''{{PAGENAME}}'' ==` — sao citacoes de entrevista da Rolling
  // Stone, nao falas da serie. Sem o nivel, "Dialogue" reabria a secao e o
  // painel enchia de declaracao do Vince Gilligan como se fosse fala do Walter.
  int secaoValida = 1;
  int n = 0;
  while (p && *p && n < max) {
    const char *fim = strchr(p, '\n');
    size_t len = fim ? (size_t)(fim - p) : strlen(p);
    char linha[1200];
    if (len >= sizeof linha) len = sizeof linha - 1;
    memcpy(linha, p, len);
    linha[len] = 0;
    p = fim ? fim + 1 : NULL;

    if (linha[0] == '=') {
      char *q = linha;
      size_t j;
      int nivel = 0;
      while (*q == '=') { nivel++; q++; }
      while (*q == ' ') q++;
      j = strlen(q);
      while (j > 0 && (q[j - 1] == '=' || q[j - 1] == ' ')) q[--j] = 0;
      limparWiki(q, SF_QUEM);
      if (nivel <= 2) {
        snprintf(secao, sizeof secao, "%s", q);
        secaoValida = secaoDeFala(secao);
      }
      continue;
    }
    if (linha[0] != '*' && linha[0] != ':') continue;
    // SECAO QUE NAO E DE PERSONAGEM NAO RENDE FALA, nem com "quem" declarado.
    // "Taglines" traz publicidade, "Cast" traz a ficha de elenco e
    // "About ''X''" traz entrevista — os tres com a mesma marcacao de fala.
    if (!secaoValida) continue;
    { char *q = linha + 1;
      char quem[SF_QUEM] = "";
      while (*q == ' ' || *q == '*' || *q == ':') q++;
      // `'''Personagem''': fala` — o negrito seguido de dois-pontos e o unico
      // sinal confiavel de quem fala.
      if (!strncmp(q, "'''", 3)) {
        char *f = strstr(q + 3, "''':");
        if (f) {
          size_t t = (size_t)(f - (q + 3));
          if (t >= sizeof quem) t = sizeof quem - 1;
          memcpy(quem, q + 3, t);
          quem[t] = 0;
          limparWiki(quem, sizeof quem);
          q = f + 4;
          while (*q == ' ') q++;
        }
      }
      // Linha inteiramente em italico e credito ou rubrica ("''Directed by
      // …''"), nao fala. Descartar ANTES de limpar, que e quando o italico
      // ainda da para ver.
      if (!quem[0] && q[0] == '\'' && q[1] == '\'') continue;
      // SECAO QUE NAO E PERSONAGEM SO VALE COM QUEM FALA DECLARADO. Sem esta
      // linha, os itens de "Taglines" (frases de cartaz, publicidade) entravam
      // como se fossem fala — o teste pegou "Uma frase de cartaz…" na lista.
      if (!quem[0] && secao[0]) snprintf(quem, sizeof quem, "%s", secao);
      limparWiki(q, sizeof linha - (size_t)(q - linha));
      // FALA LONGA DEMAIS E DESCARTADA, nunca cortada: um cartao que termina no
      // meio de uma palavra parece defeito, e "…" no fim de uma citacao sugere
      // que a frase continua sendo dela quando ninguem sabe.
      if (strlen(q) < 25 || strlen(q) > SF_TEXTO - 1) continue;
      if (strchr(q, '|') || !strncmp(q, "File:", 5)) continue;
      snprintf(saida[n].texto, SF_TEXTO, "%s", q);
      snprintf(saida[n].quem, SF_QUEM, "%s", quem);
      n++;
    }
  }
  return n;
}

// --- CACHE -------------------------------------------------------------------

static void nomeArquivo(char *dst, unsigned tam, const char *imdb) {
  char limpo[24];
  unsigned k = 0, j = 0;
  while (imdb[k] && j + 1 < sizeof limpo) {
    char c = imdb[k++];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
      limpo[j++] = c;
  }
  limpo[j] = 0;
  snprintf(dst, tam, "frases-%s.txt", limpo);
}

// Uma linha por registro, com o tipo na frente. Campos separados por TAB, e por
// isso TAB e quebra de linha sao trocados por espaco na hora de gravar: uma
// aspa de dialogo com TAB dentro partiria o registro em dois.
static void gravarCache(void) {
  char nome[64];
  char *buf;
  size_t cap = 8192, k = 0;
  int i;
  if (!imdbAtual[0]) return;
  buf = (char *)malloc(cap);
  if (!buf) return;
  nomeArquivo(nome, sizeof nome, imdbAtual);
  k += (size_t)snprintf(buf + k, cap - k, "%s\n%lld\t%s\t%d\n", SF_ARQ_V,
                        (long long)time(NULL), paginaWq, emPortugues);
  for (i = 0; i < nFatos && k + 1 < cap; i++)
    k += (size_t)snprintf(buf + k, cap - k, "F\t%s\t%s\n", fatos[i].rotulo,
                          fatos[i].valor);
  for (i = 0; i < nFrases && k + 1 < cap; i++)
    k += (size_t)snprintf(buf + k, cap - k, "Q\t%s\t%s\n", frases[i].quem,
                          frases[i].texto);
  dados_gravar_leve(nome, buf);
  free(buf);
}

// 1 quando havia cache VALIDO (inclusive o cache negativo, que e um arquivo com
// cabecalho e nenhum registro).
//
// A LEITURA DO DISCO E DE QUEM CHAMA, e acontece SEM a trava do modulo: pela
// mesma razao anotada em serieaud.c, segurar a trava durante um I/O faria o
// laco de desenho esperar por um arquivo no seriefrases_carregando() seguinte.
static int aplicarCache(char *b, const char *imdb) {
  char *p;
  long long quando = 0;
  (void)imdb;
  if (!b) return 0;
  if (strncmp(b, SF_ARQ_V, strlen(SF_ARQ_V))) return 0;
  p = strchr(b, '\n');
  if (!p) return 0;
  p++;
  paginaWq[0] = 0;
  emPortugues = 0;
  if (sscanf(p, "%lld", &quando) != 1) return 0;
  if ((long long)time(NULL) - quando > (long long)SF_VALIDADE_DIAS * 86400)
    return 0;
  { const char *t1 = strchr(p, '\t');
    if (t1) {
      const char *t2 = strchr(t1 + 1, '\t');
      size_t n = t2 ? (size_t)(t2 - t1 - 1) : 0;
      if (n && n < sizeof paginaWq) { memcpy(paginaWq, t1 + 1, n); paginaWq[n] = 0; }
      if (t2) emPortugues = atoi(t2 + 1);
    } }
  p = strchr(p, '\n');
  nFrases = nFatos = 0;
  while (p) {
    char tipo;
    const char *a, *bb, *fimLinha;
    p++;
    fimLinha = strchr(p, '\n');
    if (!fimLinha || !*p) break;
    tipo = *p;
    a = strchr(p, '\t');
    bb = a ? strchr(a + 1, '\t') : NULL;
    if (a && bb && bb < fimLinha) {
      size_t na = (size_t)(bb - a - 1), nb = (size_t)(fimLinha - bb - 1);
      if (tipo == 'F' && nFatos < SF_FATO_MAX) {
        if (na >= sizeof fatos[0].rotulo) na = sizeof fatos[0].rotulo - 1;
        if (nb >= sizeof fatos[0].valor) nb = sizeof fatos[0].valor - 1;
        memcpy(fatos[nFatos].rotulo, a + 1, na); fatos[nFatos].rotulo[na] = 0;
        memcpy(fatos[nFatos].valor, bb + 1, nb); fatos[nFatos].valor[nb] = 0;
        nFatos++;
      } else if (tipo == 'Q' && nFrases < SF_FRASE_MAX) {
        if (na >= SF_QUEM) na = SF_QUEM - 1;
        if (nb >= SF_TEXTO) nb = SF_TEXTO - 1;
        memcpy(frases[nFrases].quem, a + 1, na); frases[nFrases].quem[na] = 0;
        memcpy(frases[nFrases].texto, bb + 1, nb); frases[nFrases].texto[nb] = 0;
        nFrases++;
      }
    }
    p = (char *)fimLinha;
  }
  return 1;
}

// --- A CONSULTA --------------------------------------------------------------
//
// UMA consulta traz tudo. O `wdt:P345 "<imdb>"` e a unica ligacao honesta entre
// o id que este app tem e o mundo da Wikimedia: e o proprio Wikidata afirmando
// "este item E este filme do IMDb", nao um casamento por nome (que erra em
// remake, em traducao de titulo e em serie homonima).
//
// MAX(?bil) e a bilheteria MUNDIAL: o Wikidata guarda tambem a domestica e a
// internacional como declaracoes irmas, e a maior das tres e a mundial. A
// unidade vem por SAMPLE e nao por MAX — sao todas a mesma moeda no mesmo
// titulo; se um dia nao forem, o pior caso e o simbolo errado, e por isso
// moeda() so aceita tres Q-id conhecidos e omite o resto.
static const char *CONSULTA =
  "SELECT (SAMPLE(?wqE) AS ?qe)(SAMPLE(?wqP) AS ?qp)(MAX(?orc) AS ?o)"
  "(SAMPLE(?orcU) AS ?ou)(MAX(?bil) AS ?b)(SAMPLE(?bilU) AS ?bu)"
  "(SAMPLE(?nEp) AS ?n)(SAMPLE(?dur) AS ?d)"
  "(GROUP_CONCAT(DISTINCT ?locL;separator=\" \xc2\xb7 \") AS ?loc)"
  "(GROUP_CONCAT(DISTINCT ?preL;separator=\" \xc2\xb7 \") AS ?pr)"
  "(SAMPLE(?emiL) AS ?em)(SAMPLE(?basL) AS ?ba) WHERE {"
  "?f wdt:P345 \"%s\"."
  "OPTIONAL{?wqE schema:about ?f;schema:isPartOf <https://en.wikiquote.org/>}"
  "OPTIONAL{?wqP schema:about ?f;schema:isPartOf <https://pt.wikiquote.org/>}"
  "OPTIONAL{?f p:P2130/psv:P2130 [wikibase:quantityAmount ?orc;wikibase:quantityUnit ?orcU]}"
  "OPTIONAL{?f p:P2142/psv:P2142 [wikibase:quantityAmount ?bil;wikibase:quantityUnit ?bilU]}"
  "OPTIONAL{?f wdt:P1113 ?nEp}OPTIONAL{?f wdt:P2047 ?dur}"
  "OPTIONAL{?f wdt:P915 ?lo. ?lo rdfs:label ?locL FILTER(lang(?locL)=\"pt\")}"
  "OPTIONAL{?f wdt:P166 ?pre. ?pre rdfs:label ?preL FILTER(lang(?preL)=\"pt\")}"
  "OPTIONAL{?f wdt:P449 ?emi. ?emi rdfs:label ?emiL FILTER(lang(?emiL)=\"pt\")}"
  "OPTIONAL{?f wdt:P144 ?bas. ?bas rdfs:label ?basL FILTER(lang(?basL)=\"pt\")}}";

// O User-Agent nao e cortesia: a Wikimedia recusa cliente sem identificacao, e
// um 403 aqui chegaria como "o titulo nao tem nada". Fica EM INGLES de
// proposito — e cabecalho de protocolo, nao texto de tela, e em portugues o
// tests/i18n.sh o acusava de string sem i18n().
static const char *const CAB[] = {
  "User-Agent: NuvioTV/1.0 (TV app; contact via the project repository)",
  "Accept: application/sparql-results+json",
  NULL
};
static const char *const CAB_WQ[] = {
  "User-Agent: NuvioTV/1.0 (TV app; contact via the project repository)",
  NULL
};

// Mesmo motivo do finalizar() de serieaud.c: sem esta continuacao, trocar de
// titulo enquanto a busca esta no ar deixa a pagina seguinte vazia para sempre.
static void *buscar(void *arg);
static void finalizar(const char *imdb) {
  int continuar = 0;
  pthread_mutex_lock(&trava);
  if (strcmp(imdbPedido, imdb)) {
    snprintf(imdbEmCurso, sizeof imdbEmCurso, "%s", imdbPedido);
    abandonar = 0;
    continuar = 1;
  } else {
    fioVivo = 0;
  }
  pthread_mutex_unlock(&trava);
  if (continuar) {
    if (pthread_create(&fio, NULL, buscar, NULL) != 0) {
      pthread_mutex_lock(&trava); fioVivo = 0; pthread_mutex_unlock(&trava);
    } else pthread_detach(fio);
  }
}

static int deveParar(void) {
  int p;
  pthread_mutex_lock(&trava);
  p = abandonar;
  pthread_mutex_unlock(&trava);
  return p;
}

// "https://en.wikiquote.org/wiki/The_Dark_Knight_(film)" -> o titulo da pagina.
static void tituloDaUrl(const char *url, char *dst, size_t tam) {
  const char *p = strstr(url, "/wiki/");
  size_t k = 0;
  if (tam) dst[0] = 0;
  if (!p) return;
  p += 6;
  // O sitelink vem percent-encoded; desfaz para poder pedir a pagina por nome.
  while (*p && k + 1 < tam) {
    if (*p == '%' && p[1] && p[2]) {
      int hi = p[1] <= '9' ? p[1] - '0' : (p[1] | 32) - 'a' + 10;
      int lo = p[2] <= '9' ? p[2] - '0' : (p[2] | 32) - 'a' + 10;
      dst[k++] = (char)((hi << 4) | lo);
      p += 3;
      continue;
    }
    dst[k++] = *p == '_' ? ' ' : *p;
    p++;
  }
  dst[k] = 0;
}

static void *buscar(void *arg) {
  char imdb[24], consulta[2200], cod[6000], url[6400];
  char *corpo;
  char wqE[300] = "", wqP[300] = "";
  int respondeu = 0;
  (void)arg;

  pthread_mutex_lock(&trava);
  snprintf(imdb, sizeof imdb, "%s", imdbEmCurso);
  pthread_mutex_unlock(&trava);

  // 1. CACHE — inclusive o negativo.
  { int achou;
    char nomeC[64], *bruto;
    nomeArquivo(nomeC, sizeof nomeC, imdb);
    bruto = dados_ler(nomeC);            /* I/O FORA da trava do modulo */
    pthread_mutex_lock(&trava);
    achou = aplicarCache(bruto, imdb);
    pthread_mutex_unlock(&trava);
    free(bruto);
    if (achou) {
      printf("[frases] %s: cache (%d frases, %d fatos), 0 pedidos\n",
             imdb, nFrases, nFatos);
      fflush(stdout);
      finalizar(imdb);
      return NULL;
    } }

  // 2. WIKIDATA: fatos + endereco do Wikiquote, num pedido so.
  snprintf(consulta, sizeof consulta, CONSULTA, imdb);
  urlenc(cod, sizeof cod, consulta);
  snprintf(url, sizeof url, "https://query.wikidata.org/sparql?query=%s", cod);
  corpo = deveParar() ? NULL : rede_baixar_com(url, 20, CAB);
  respondeu = corpo != NULL;
  if (corpo) {
    char v[300], u[120];
    pthread_mutex_lock(&trava);
    if (!strcmp(imdb, imdbPedido)) {
      nFatos = 0;
      if (sparqlCampo(corpo, "o", v, sizeof v) &&
          sparqlCampo(corpo, "ou", u, sizeof u) && moeda(u)) {
        char d[80];
        dinheiro(d, sizeof d, atof(v), moeda(u));
        addFato(i18n("Orçamento"), d);
      }
      if (sparqlCampo(corpo, "b", v, sizeof v) &&
          sparqlCampo(corpo, "bu", u, sizeof u) && moeda(u)) {
        char d[80];
        dinheiro(d, sizeof d, atof(v), moeda(u));
        addFato(i18n("Bilheteria"), d);
      }
      if (sparqlCampo(corpo, "loc", v, sizeof v)) {
        primeirosItens(v, 4);
        addFato(i18n("Filmado em"), v);
      }
      if (sparqlCampo(corpo, "em", v, sizeof v)) addFato(i18n("Emissora"), v);
      if (sparqlCampo(corpo, "ba", v, sizeof v)) addFato(i18n("Baseado em"), v);
      if (sparqlCampo(corpo, "n", v, sizeof v)) {
        char d[60];
        snprintf(d, sizeof d, i18n("%d episódios"), atoi(v));
        addFato(i18n("Ao todo"), d);
      }
      // OS PREMIOS SAO LISTADOS, NUNCA CONTADOS. MEDIDO: a consulta so enxerga
      // o premio que tem nome em portugues no Wikidata, entao "3" nos premios
      // do Dark Knight seria "3 premios com rotulo pt", e escrever "ganhou 3
      // premios" seria falso. A lista nao promete completude; um numero
      // prometeria.
      if (sparqlCampo(corpo, "pr", v, sizeof v)) {
        primeirosItens(v, 3);
        addFato(i18n("Prêmios"), v);
      }
      if (sparqlCampo(corpo, "qp", wqP, sizeof wqP)) { /* prefere o portugues */ }
      sparqlCampo(corpo, "qe", wqE, sizeof wqE);
    }
    pthread_mutex_unlock(&trava);
    free(corpo);
  }

  // 3. WIKIQUOTE, so se houver pagina.
  if (!deveParar() && (wqP[0] || wqE[0])) {
    char pagina[220], cod2[700];
    int pt = wqP[0] != 0;
    tituloDaUrl(pt ? wqP : wqE, pagina, sizeof pagina);
    urlenc(cod2, sizeof cod2, pagina);
    snprintf(url, sizeof url,
             "https://%s.wikiquote.org/w/api.php?action=parse&format=json"
             "&prop=wikitext&page=%s", pt ? "pt" : "en", cod2);
    corpo = rede_baixar_com(url, 20, CAB_WQ);
    if (corpo) {
      // O wikitexto vem como UMA string JSON gigante em parse.wikitext.*. Nao
      // cabe em buffer de pilha, e js_texto nao serve: ele copia para tamanho
      // fixo. O que importa sao as LINHAS, entao os \n escapados viram quebras
      // de verdade no proprio buffer e o resto e lido direto.
      char *w = desescapar(corpo);
      if (w) {
        SfFrase cand[64];
        int n = extrairFalas(w, cand, 64);
        int k;
        pthread_mutex_lock(&trava);
        if (!strcmp(imdb, imdbPedido)) {
          nFrases = 0;
          // ESPALHADAS pelo documento, e nao as primeiras: a pagina de um filme
          // e organizada por personagem, entao as seis primeiras seriam todas
          // do mesmo. Um passo uniforme atravessa a lista inteira e traz gente
          // diferente.
          for (k = 0; k < SF_FRASE_MAX && n > 0; k++) {
            int idx = n <= SF_FRASE_MAX ? k : (k * n) / SF_FRASE_MAX;
            if (idx >= n) break;
            frases[nFrases++] = cand[idx];
          }
          snprintf(paginaWq, sizeof paginaWq, "%s", pagina);
          emPortugues = pt;
        }
        pthread_mutex_unlock(&trava);
      }
      free(corpo);
    }
  }

  pthread_mutex_lock(&trava);
  // GRAVA MESMO VAZIO — mas SO SE O SERVIDOR RESPONDEU. A diferenca nao e
  // detalhe: "este titulo nao tem nada" e um fato que vale 30 dias de cache;
  // "o endpoint nao respondeu" nao vale nada, e gravar o segundo como se fosse
  // o primeiro faz o titulo ficar VAZIO POR UM MES por causa de um tempo
  // esgotado de uma vez.
  //
  // ACONTECEU NA CONFERENCIA AO VIVO (16/09/2026): o query.wikidata.org
  // respondeu com falha 28 (tempo esgotado) para tt0468569, e a versao anterior
  // gravou o vazio. O Wikidata e um servico publico com limite de uso; isto vai
  // acontecer de novo.
  if (!strcmp(imdb, imdbPedido) && respondeu) gravarCache();
  printf("[frases] %s: %d frases, %d fatos\n", imdb, nFrases, nFatos);
  fflush(stdout);
  pthread_mutex_unlock(&trava);
  finalizar(imdb);
  return NULL;
}

// --- API ---------------------------------------------------------------------

void seriefrases_abrir(const char *imdb) {
  char id[24];
  int precisa = 0;
  soImdb(id, sizeof id, imdb);
  if (!id[0]) return;
  pthread_mutex_lock(&trava);
  if (!strcmp(id, imdbAtual)) { pthread_mutex_unlock(&trava); return; }
  nFrases = nFatos = selecionado = 0;
  paginaWq[0] = 0;
  emPortugues = 0;
  snprintf(imdbAtual, sizeof imdbAtual, "%s", id);
  snprintf(imdbPedido, sizeof imdbPedido, "%s", id);
  abandonar = 0;
  if (!fioVivo) {
    snprintf(imdbEmCurso, sizeof imdbEmCurso, "%s", id);
    fioVivo = 1;
    precisa = 1;
  }
  pthread_mutex_unlock(&trava);
  if (precisa) {
    if (pthread_create(&fio, NULL, buscar, NULL) != 0) {
      pthread_mutex_lock(&trava); fioVivo = 0; pthread_mutex_unlock(&trava);
    } else pthread_detach(fio);
  }
}

void seriefrases_fechar(void) {
  pthread_mutex_lock(&trava);
  abandonar = 1;
  pthread_mutex_unlock(&trava);
}

int seriefrases_carregando(void) {
  int v;
  pthread_mutex_lock(&trava);
  v = fioVivo;
  pthread_mutex_unlock(&trava);
  return v;
}

int seriefrases_n(void) { return nFrases; }
const char *seriefrases_quem(int i) {
  return (i >= 0 && i < nFrases) ? frases[i].quem : "";
}
const char *seriefrases_texto(int i) {
  return (i >= 0 && i < nFrases) ? frases[i].texto : "";
}
const char *seriefrases_pagina(void) { return paginaWq; }
int seriefrases_em_portugues(void) { return emPortugues; }
int seriefrases_n_fatos(void) { return nFatos; }
const char *seriefrases_fato_rotulo(int i) {
  return (i >= 0 && i < nFatos) ? fatos[i].rotulo : "";
}
const char *seriefrases_fato_valor(int i) {
  return (i >= 0 && i < nFatos) ? fatos[i].valor : "";
}
void seriefrases_selecionar(int i) {
  if (i < 0) i = 0;
  if (i >= nFrases) i = nFrases - 1;
  selecionado = i < 0 ? 0 : i;
}
int seriefrases_selecionado(void) { return selecionado; }

// --- DESENHO -----------------------------------------------------------------
//
// POR QUE ISTO NAO SAO MAIS CARTOES.
//
// A primeira versao empilhava uma caixa cinza arredondada por fala, com o nome
// do personagem em cinza menor logo abaixo. Na captura, a 3 m, o que se lia
// eram SEIS BOLHAS IGUAIS, uma embaixo da outra — conversa de aplicativo de
// mensagem, nao citacao. O dono olhou a mesma captura e pediu em uma linha:
// "deixa o escrito em forma de citacao". O problema e tipografico, e a correcao
// tambem: nao ha dado novo aqui, nem pedido de rede novo.
//
// O QUE FAZ UM TEXTO LER COMO CITACAO, e de onde veio cada escolha:
//
//   ASPA DE ABERTURA GRANDE, num vao fixo a esquerda (SF_VAO). E o unico sinal
//     que diz "isto e fala de outra pessoa" ANTES de a primeira palavra ser
//     lida. Fica em TXT_TITULO1 (76) com alfa baixo porque e MOBILIA, nao
//     pontuacao: uma aspa no corpo do texto some a 3 m, e uma aspa opaca
//     competiria com a propria frase. O vao e FIXO e nao acompanha a aspa, para
//     que toda fala comece na mesma coluna — varias falas do Wikiquote ja
//     comecam com aspas de dialogo dentro do texto, e uma coluna que dancasse
//     com isso desalinharia a leitura vertical.
//
//     O 76 FOI ESCOLHIDO OLHANDO, e o 56 que estava aqui antes foi REJEITADO
//     olhando. Na captura, a aspa em 56 saia com uns 18 px de tinta dentro de
//     uma coluna de 56 e lia como SIMBOLO DE POLEGADA perdido na margem — o
//     desenho tinha a forma certa e a escala errada, que e o pior dos dois
//     mundos, porque parece descuido em vez de escolha. 76 e o maior corpo que
//     esta base tem (TXT_TITULO1, o titulo do filme na tela de detalhe) e e
//     Bold, e a aspa em Bold aguenta a alfa baixa sem virar um fiapo. Nao ha
//     passo entre 56 e 76.
//
//     E ELA SE REPETE EM TODAS AS FALAS, de proposito. Uma aspa sozinha e
//     ambigua na Inter, cujo glifo “ e feito de dois blocos quase retos; cinco
//     na mesma coluna deixam de ser lidas uma a uma e viram a coluna das
//     citacoes. A repeticao e que resolve a ambiguidade do desenho do glifo.
//
//   A FALA E A HEROINA DA LINHA: TXT_CALLOUT (28/500) no lugar do TXT_DET_SIN
//     (26/400) de antes, com entrelinha 38 (1,36 do corpo) no lugar de 34
//     (1,31). NAO subiu para TXT_HEADLINE (38/500), que era o passo seguinte,
//     por uma razao concreta: TXT_HEADLINE e o estilo do proprio titulo do
//     painel ("Frases"), e fala e cabecalho no mesmo corpo e no mesmo peso
//     apagam a hierarquia — o cabecalho deixa de ser cabecalho. Entre 26 e 38
//     so existe o 28, e e ele.
//
//   ATRIBUICAO COM TRAVESSAO, em linha propria e menor: "— <nome>" e como se
//     assina uma citacao; o nome solto embaixo e legenda de foto. O travessao
//     NAO e string de tela — e marca tipografica, e a varredura de i18n o
//     classifica como formato puro (sobra so o "%s"). Conferido: bash
//     tests/i18n.sh passa limpo com ele, nas tres varreduras.
//
//   SEM CAIXA. O separador e vao branco mais um FILETE, a mesma licao que a
//     Agenda aprendeu ao trocar as caixas das linhas por um eixo. O filete
//     copia a `regua()` da Agenda, nao o eixo dela — os numeros exatos e a
//     unica diferenca que sobrou estao em sfFilete().
//
//     E O FILETE NAO ENCOSTA NA CITACAO EM FOCO, dos dois lados; a razao (uma
//     assimetria que a captura mostrou) esta no fim do laco de desenho.
//
// O QUE FOI MEDIDO E RECUSADO:
//
//   ITALICO. Conferido no arquivo, nao suposto: deploy/app/fonts/ tem
//     InterDisplay-Regular, -Medium e -Bold e NENHUM italico, e as familias de
//     reserva da TV (LG_Display-Light/Regular, DroidSans) tambem nao tem. So
//     restaria TTF_SetFontStyle(TTF_STYLE_ITALIC), que e cisalhamento — e o
//     proprio text.c so aceita sintetico para o NEGRITO das reservas, e ja
//     anota por que (engorda o traco sem redesenhar nada). Oblíquo falso a 3 m
//     nao le como voz, le como defeito de renderizacao. Entao: sem italico.
//
//   ASPA DE FECHAMENTO. Uma citacao aberta e fechada com aspas grandes vira
//     cartao de felicitacao, e a de fechamento teria de flutuar no fim de uma
//     linha de comprimento imprevisivel (a fala quebra em 1, 2 ou 3 linhas).
//     A de abertura sozinha, na coluna, marca a natureza do texto e nao se
//     mexe.
//
// FOCO — o vocabulario nao mudou e nao pode mudar: superficie clara PREENCHIDA
// com texto escuro, sem contorno, em DEGRAU (`sel`, sem meio-termo), como em
// streams.c e salvospainel.c. O que mudou e que a superficie clara agora e a
// UNICA superficie da lista: antes ela disputava com cinco caixas cinza a 5% de
// alfa e o foco era "a caixa mais clara"; agora e "a unica caixa".

#define SF_VAO     56.0f   // coluna da aspa de abertura
#define SF_ASPA_Y  -22.0f  // quanto a aspa sobe para casar com a 1a linha
#define SF_LEAD    38.0f   // entrelinha da fala (TXT_CALLOUT 28 * 1,36)
#define SF_ATRIB   12.0f   // vao entre a ultima linha da fala e a atribuicao
#define SF_PAD_V   16.0f   // folga da superficie de foco acima e abaixo
#define SF_GAP     30.0f   // vao entre uma citacao e a proxima
// 16, e nao 22, por DUAS medidas que deram no mesmo numero. (a) E o mesmo
// avanco que a linha da Agenda usa (o `x - 16.0f` de desenhaConteudo em
// agendaui.c) — as duas telas passam a recortar o foco na mesma distancia. (b)
// A safe area lateral e NV_MARGEM_X = 80 e o painel comeca em NV_CONTENT_PAD =
// 104: com 22 a superficie de foco caia em x = 82, dois pixeis DENTRO da faixa
// de overscan, e o canto arredondado do item selecionado seria a primeira coisa
// que a TV aparia. Com 16 ela para em 88.
#define SF_PAD_H   16.0f   // quanto a superficie de foco avanca para a esquerda
#define SF_LINHAS  3       // maximo de linhas por fala

// Quebra `s` em ate `maxLinhas` linhas de largura `larg`, FECHANDO COM
// RETICENCIAS quando sobrou texto, e devolve a altura usada. Com `desenhar` = 0
// so mede.
//
// POR QUE NAO txt_bloco. Ele corta NO SECO no limite de linhas, sem reticencia
// — defeito real desta base, ja anotado em agendaui.h. Numa citacao isso e pior
// do que numa sinopse: a frase acaba no meio e quem le nao tem como saber se
// foi o Wikiquote que aparou (o cabecalho AVISA que isso acontece) ou se foi o
// app. Com "…" a resposta e nossa e esta dita.
//
// POR QUE NAO agendaui_sinopse(). E a mesma forma — preencher palavra a palavra
// medindo com o PROPRIO txt_linha e entregar o resto a txt_linha_corta, que ja
// sabe fechar com "…" — mas ela e fixa em DUAS linhas e nao devolve altura, e
// aqui a caixa de foco precisa da altura que saiu desta mesma quebra.
//
// MEDIR E DESENHAR SAO O MESMO CODIGO, com um interruptor. A versao anterior
// media chamando txt_bloco com alfa 0 e desenhava com alfa 1 — mesma funcao,
// entao nao divergia. Manter essa propriedade e o unico jeito de a superficie
// de foco nunca errar por uma linha: duas implementacoes da mesma quebra
// divergem, e divergem caladas.
//
// TRADUZ ANTES DE QUEBRAR, pelo mesmo motivo escrito em txt_bloco e em
// txt_linha_corta: a quebra acontece no texto FINAL. Se a frase fosse partida
// em portugues e cada pedaco entregue a txt_linha, nenhum pedaco casaria com
// chave da tabela e o estado vazio ficaria em portugues sob o app em ingles.
static float sfBloco(TxtEstilo est, const char *s, int r, int g, int b,
                     float x, float y, float larg, float lead,
                     int maxLinhas, int desenhar) {
  const char *p;
  int linha = 0;
  s = i18n(s);
  if (!s || !s[0] || larg <= 0.0f || maxLinhas <= 0) return 0.0f;
  p = s;
  while (*p && linha < maxLinhas) {
    if (linha == maxLinhas - 1) {
      // ULTIMA LINHA: txt_linha_corta mede, corta por palavra e fecha com "…"
      // — e nao faz nada disso quando o resto ja cabe.
      if (desenhar)
        txt_desenhar(txt_linha_corta(est, p, r, g, b, 255, larg),
                     x, y + lead * (float)linha);
      linha++;
      break;
    }
    { char l[SF_TEXTO + 16];
      size_t nl = 0;
      l[0] = 0;
      while (*p) {
        const char *ini = p;
        char tent[SF_TEXTO + 16];
        size_t np, k;
        while (*p && *p != ' ' && *p != '\n') p++;
        np = (size_t)(p - ini);
        if (nl + np + 2 >= sizeof tent) { p = ini; break; }
        memcpy(tent, l, nl);
        k = nl;
        if (k) tent[k++] = ' ';
        memcpy(tent + k, ini, np);
        tent[k + np] = 0;
        // A MEDIDA E A MESMA QUE VAI DESENHAR. Estimar por largura media de
        // glifo erra em nome proprio e em maiuscula, e o erro sai como uma
        // linha estourando a coluna — o defeito que este corte existe para
        // evitar.
        if ((float)txt_linha(est, tent, r, g, b, 255).w > larg && l[0]) {
          p = ini;
          break;
        }
        memcpy(l, tent, k + np + 1);
        nl = k + np;
        while (*p == ' ' || *p == '\n') p++;
      }
      if (!l[0]) break;
      if (desenhar)
        txt_desenhar(txt_linha(est, l, r, g, b, 255), x, y + lead * (float)linha);
      linha++; }
  }
  return (float)linha * lead;
}

// O FILETE que separa duas citacoes (e dois campos da ficha).
//
// DE ONDE SAI CADA NUMERO, conferido no arquivo e nao de memoria. A referencia
// e a `regua()` da Agenda (agendaui.c), que e a linha horizontal neutra
// daquela tela: `regua(xEixo, xDir, yl, 0.62f, 0.14f)`, 1 px de altura. NAO e o
// eixo vertical — esse e 3 px a 0,30 de alfa, mobilia estrutural, forte demais
// para um separador. Uma versao anterior deste comentario dizia "mesma alfa do
// eixo" e estava errada nos dois valores; quem fosse casar as duas telas por
// esse texto erraria por mais que o dobro.
//
// A DIFERENCA QUE SOBRA e a altura, 2 px aqui contra 1 px la, e ela e
// deliberada: a linha da Agenda corre ao lado de um eixo de 3 px que ja da
// estrutura a tela, e aqui o filete e a UNICA linha desenhada entre duas
// citacoes. A alfa desce de 0,14 para 0,13 na mesma conta — o dobro de altura
// com a mesma massa de tinta aproximada. Na captura a 1920 ele aparece de
// perto e some a distancia, que e o que um filete tem de fazer: quem separa as
// citacoes a 3 m e o vao branco, nao o traco.
static void sfFilete(float x, float y, float larg) {
  GfxRect f;
  f.x = x; f.y = y; f.w = larg; f.h = 2.0f;
  gfx_cor(f, 0.0f, 0.70f, 0.71f, 0.76f, 0.13f);
}

// Cabecalho comum aos dois paineis. A linha de procedencia GANHOU DUAS LINHAS
// de espaco em vez de uma com reticencias: o que ela diz no FIM ("podem estar
// aparadas") e a parte honesta da frase, e um corte por largura apagaria
// justamente essa parte. Com sfBloco ela quebra; so se nao couber em duas e que
// perde algo, e ai com "…" dizendo isso.
//
// O VAO DE BAIXO E 26 + SF_PAD_V, e o SF_PAD_V nao e enfeite. O vao tem de ser
// medido contra a coisa mais ALTA que pode nascer logo abaixo dele, e essa
// coisa nao e o texto da primeira citacao: e a SUPERFICIE DE FOCO dela, que
// comeca SF_PAD_V acima do texto. Com 26 puro, a captura com a primeira citacao
// em foco mostrava a superficie clara subindo ate 10 px da linha de
// procedencia, quase encostando nos descendentes dela — o painel parecia ter
// duas coisas grudadas. Somando SF_PAD_V, a superficie para a 26 px da linha,
// que e exatamente o vao que o texto teria se a citacao nao estivesse em foco:
// o cabecalho deixa de se mexer conforme o foco anda.
static float cabecalho(GfxRect r, const char *titulo, const char *fonte) {
  TxtLinha t = txt_linha(TXT_HEADLINE, i18n(titulo), 255, 255, 255, 255);
  float hf;
  txt_desenhar(t, r.x, r.y);
  hf = sfBloco(TXT_DET_META2, fonte, 150, 153, 162,
               r.x, r.y + (float)t.h + 6.0f, r.w, 30.0f, 2, 1);
  return (float)t.h + 6.0f + hf + 26.0f + SF_PAD_V;
}

float seriefrases_desenhar(GfxRect r) {
  char fonte[300];
  float y;
  int i;
  // A PROCEDENCIA E O IDIOMA vao no cabecalho. Frase em ingles sob um app em
  // portugues sem aviso parece defeito de traducao; e o Wikiquote e transcrito
  // por gente, entao "pode estar aparada" nao e modestia, e o estado da fonte.
  if (paginaWq[0])
    snprintf(fonte, sizeof fonte,
             i18n("Wikiquote (%s) · “%s” — transcritas por colaboradores, podem estar aparadas"),
             emPortugues ? "pt" : "en", paginaWq);
  else
    snprintf(fonte, sizeof fonte, "%s", i18n("Wikiquote"));
  y = r.y + cabecalho(r, "Frases", fonte);

  if (nFrases == 0) {
    // DOIS ESTADOS DIFERENTES, ditos diferente. "Nao ha pagina no Wikiquote" e
    // um fato sobre a fonte; "carregando" e um estado nosso.
    //
    // O estado vazio e o COMUM em serie (ver a cobertura medida em
    // seriefrases.h), entao ele nao pode parecer uma falha: quebra em duas
    // linhas em vez de terminar em reticencias.
    //
    // ELE COMECA RENTE A MARGEM (r.x), e nao na coluna das falas. Antes vinha
    // com o vao de SF_VAO "para cair na mesma coluna que as falas cairiam", e a
    // captura do estado vazio desmentiu o argumento: nao HA fala nenhuma na
    // tela para alinhar com ele, e o que sobra e uma linha recuada 56 px sob um
    // cabecalho rente a margem, ao lado de um painel de ficha cuja frase de
    // vazio esta rente a margem. O recuo so se lia como desalinho. A coluna da
    // aspa existe para dar lugar a aspa; sem aspa, ela nao existe.
    const char *s = seriefrases_carregando()
                      ? "Carregando…"
                      : "Este título não tem página de frases no Wikiquote";
    return y - r.y + sfBloco(TXT_BODY, s, 150, 153, 162,
                             r.x, y, r.w, 34.0f, 2, 1);
  }

  for (i = 0; i < nFrases; i++) {
    int sel = (i == selecionado);
    // Texto quase branco fora do foco, quase preto dentro dele. A atribuicao
    // fica um degrau mais apagada dos dois lados: ela e credito, nao fala.
    //
    // O CINZA DA ATRIBUICAO SOBRE O FOCO E 62, e nao 86. Como a superficie
    // passou a ser ajustes_acento(), ela pode ser o violeta #ab47bc, que e a
    // mais escura da tabela de temas (6,6:1 contra o fundo, diz ajustes.c): um
    // 86 em cima dele quase soma com a propria superficie. 62 e o mesmo valor
    // que a Agenda usa na linha secundaria da linha focada (AG_TEXTO_ESCURO e
    // 20, o secundario dela e 62) — aguenta o tema mais escuro e continua um
    // degrau claro abaixo do 18 da fala.
    int cr = sel ? 18 : 232, cg = sel ? 18 : 234, cb = sel ? 22 : 240;
    int ar = sel ? 62 : 150, ag = sel ? 62 : 153, ab = sel ? 68 : 162;
    float largTexto = r.w - SF_VAO - 24.0f;
    float alturaTexto, conteudoH;
    float xTexto = r.x + SF_VAO;
    TxtLinha atrib = { 0, 0, 0 };
    char assinatura[SF_QUEM + 8];

    // MEDE ANTES DE PINTAR: a superficie de foco acompanha o texto, e nao o
    // contrario — uma fala de uma linha dentro de uma superficie de tres le
    // como erro de layout. A medida sai da MESMA sfBloco que desenha.
    alturaTexto = sfBloco(TXT_CALLOUT, frases[i].texto, cr, cg, cb,
                          xTexto, y, largTexto, SF_LEAD, SF_LINHAS, 0);
    conteudoH = alturaTexto;
    // ATRIBUICAO AUSENTE NAO DEIXA BURACO: metade das falas do Wikiquote nao
    // diz quem falou (seriefrases_quem() devolve ""), e aqui isso nao reserva
    // espaco nem desloca a proxima citacao — a altura e somada so quando existe.
    if (frases[i].quem[0]) {
      snprintf(assinatura, sizeof assinatura, "— %s", frases[i].quem);
      atrib = txt_linha_corta(TXT_DET_META2, assinatura, ar, ag, ab, 255,
                              largTexto);
      conteudoH += SF_ATRIB + (float)atrib.h;
    }

    // FOCO: superficie clara PREENCHIDA com texto escuro, sem contorno, em
    // DEGRAU. E o vocabulario decidido em 16/09 e escrito em layout.h. Ela
    // avanca SF_PAD_H para a esquerda para engolir a coluna da aspa: uma
    // superficie que comecasse na coluna do texto deixaria a aspa da fala
    // selecionada clara sobre o claro, invisivel.
    //
    // FORA DO FOCO NAO HA SUPERFICIE NENHUMA. O 0,05 de alfa da versao anterior
    // era o que fazia as seis falas lerem como seis bolhas de conversa.
    //
    // A COR DA SUPERFICIE E ajustes_acento(), e NAO um branco cravado. Estava
    // cravada em (0,94 0,94 0,96) e passava despercebido porque o tema de
    // fabrica E branco — o defeito so apareceria na TV de quem trocou o tema,
    // onde esta seria a unica superficie de foco do aplicativo inteiro que
    // ignora a escolha. layout.h diz a regra com todas as letras no comentario
    // de NV_COR_FOCO ("FOCO = preenchimento na COR DE REALCE (ajustes_acento,
    // branco por padrao) com texto escuro, SEM anel"), e streams.c, agendaui.c,
    // salvospainel.c e ajustes.c todos chamam ajustes_acento aqui.
    if (sel) {
      GfxRect caixa;
      float ar_, ag_, ab_;
      ajustes_acento(&ar_, &ag_, &ab_);
      caixa.x = r.x - SF_PAD_H;
      caixa.y = y - SF_PAD_V;
      caixa.w = r.w + SF_PAD_H;
      caixa.h = conteudoH + SF_PAD_V * 2.0f;
      // O raio do gfx_cor e FRACAO DA ALTURA, nao pixeis: 14 px so viram 14 px
      // depois de divididos pela altura viva desta citacao, que muda com o
      // numero de linhas.
      gfx_cor(caixa, 14.0f / caixa.h, ar_, ag_, ab_, 1.0f);
    }

    { TxtLinha aspa = txt_linha(TXT_TITULO1, "\xe2\x80\x9c", cr, cg, cb, 255);
      // A aspa desce SF_ASPA_Y para o desenho dela alinhar com a PRIMEIRA LINHA
      // da fala: o glifo mora no alto da caixa da fonte de 76 px, e a fala tem
      // 28.
      txt_desenhar_alpha(aspa, r.x - 8.0f, y + SF_ASPA_Y, sel ? 0.20f : 0.24f); }

    sfBloco(TXT_CALLOUT, frases[i].texto, cr, cg, cb,
            xTexto, y, largTexto, SF_LEAD, SF_LINHAS, 1);
    if (atrib.h) txt_desenhar(atrib, xTexto, y + alturaTexto + SF_ATRIB);

    y += conteudoH + SF_PAD_V * 2.0f;
    // FILETE no meio do vao, e nunca depois da ultima: um filete sob o ultimo
    // item promete uma linha que nao existe.
    //
    // E NUNCA ENCOSTADO NA CITACAO EM FOCO, dos DOIS lados. A primeira versao
    // so tinha a condicao do fim da lista, e a captura mostrou o defeito: o
    // filete de CIMA sumia sozinho — ele e desenhado na volta anterior do laco e
    // a superficie de foco, desenhada depois, passava por cima dele — enquanto o
    // de BAIXO ficava. O item selecionado nascia com um traco de um lado so,
    // assimetria que se le como falha de desenho antes de se ler como foco.
    // Podia ter sido resolvido pintando a superficie antes do filete; foi
    // resolvido TIRANDO OS DOIS, que e o que o desenho quer dizer: a superficie
    // clara ja separa esta citacao das vizinhas, e um filete encostado nela
    // seria o mesmo trabalho feito duas vezes.
    if (i + 1 < nFrases && !sel && i + 1 != selecionado)
      sfFilete(r.x, y + SF_GAP * 0.5f, r.w);
    y += SF_GAP;
  }
  return y - r.y - SF_GAP;
}

// A FICHA AO LADO TEM DE PARECER A MESMA TELA. Ela continua sendo o que era —
// uma tabela de rotulo e valor, e o dono disse que esta boa — entao o que muda
// aqui e so o que a ligava ao desenho antigo das frases:
//
//   O ROTULO SOBE PARA TXT_DET_META2 (23). Estava em TXT_CAPTION (22), abaixo
//     do piso de 23 px que text.c registra como minimo do tvOS para TEXTO — e
//     agora e o MESMO estilo da atribuicao da citacao e da linha de
//     procedencia. Tres coisas com a mesma funcao (dizer de onde veio) no mesmo
//     corpo: e isso que faz os dois paineis lerem como uma tela so.
//   O MESMO FILETE entre campos, no mesmo cinza e na mesma alfa.
//   O MESMO RITMO vertical do painel de frases.
//
// A coluna do rotulo encolheu de 220 para 196: no painel estreito da direita
// (592 px na captura) cada pixel que sai do rotulo entra na lista de premios,
// que e o valor que mais quebra linha.
float seriefrases_desenhar_fatos(GfxRect r) {
  float y = r.y + cabecalho(r, "Ficha de produção",
                            i18n("Campos do Wikidata — não é curiosidade escrita por nós"));
  int i;
  if (nFatos == 0) {
    const char *s = seriefrases_carregando()
                      ? "Carregando…"
                      : "O Wikidata não tem estes campos para este título";
    return y - r.y + sfBloco(TXT_BODY, s, 150, 153, 162, r.x, y, r.w, 34.0f, 2, 1);
  }
  for (i = 0; i < nFatos; i++) {
    TxtLinha rot = txt_linha_corta(TXT_DET_META2, fatos[i].rotulo,
                                   150, 153, 162, 255, 188.0f);
    float xv = r.x + 196.0f;
    float h;
    txt_desenhar(rot, r.x, y + 2.0f);
    // O valor QUEBRA EM LINHAS: a lista de premios e de locais passa facil de
    // uma linha. Agora com reticencias quando estoura as quatro — antes o
    // txt_bloco cortava no seco e o ultimo premio terminava no meio.
    h = sfBloco(TXT_BODY, fatos[i].valor, 226, 228, 236, xv, y,
                r.w - 196.0f, 34.0f, 4, 1);
    y += (h > (float)rot.h ? h : (float)rot.h) + SF_GAP * 0.5f;
    if (i + 1 < nFatos) sfFilete(r.x, y, r.w);
    y += SF_GAP * 0.5f;
  }
  return y - r.y - SF_GAP * 0.5f;
}
