// A FONTE LEMBRADA: assinatura, casamento entre episodios, disco e perfil.
//
//   bash tests/fontepref.sh
//
// Cobre os issues #56 (o episodio seguinte tem de continuar na mesma fonte) e
// #57 (Retomar nao pode abrir a folha de fontes quando ja ha uma escolha), e a
// camada que veio depois deles: o `behaviorHints.bingeGroup` que o addon
// declara, a ordem entre ele e a assinatura de audio, e o vencimento.
//
// Inclui o .c de proposito, como tests/atualizacao.c e tests/recomenda.c:
// normalizar(), tokenEm() e a tabela sao estaticas, e o teste tambem precisa
// forcar um ARRANQUE FRIO (`carregado = 0`), que e a unica forma honesta de
// provar que a escolha volta do disco e nao da memoria. O .sh compila tudo
// MENOS src/fontepref.c.
//
// ONDE ELE ESCREVE: exclusivamente em NUVIO_DADOS, e ele CONFERE isso antes de
// qualquer escrita. Sem essa conferencia o teste grava fontepref-p1.txt dentro
// do ~/.nuvio de quem o roda. Ja aconteceu neste repositorio.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../src/fontepref.c"

#include "dados.h"

static int falhas;
#define CONFERE(cond, ...) do { \
    if (cond) { printf("ok    "); } else { printf("FALHA "); falhas++; } \
    printf(__VA_ARGS__); printf("\n"); } while (0)

// --- fontes de mentira -------------------------------------------------------
//
// Os textos imitam o que os addons mandam de verdade: o Torrentio poe as
// bandeiras no `title` (que vira `descricao`) e o nome do arquivo junto; o
// AIOStreams escreve "Dual Audio" por extenso. O NOME DO ARQUIVO MUDA DE
// EPISODIO PARA EPISODIO — e esse o ponto do teste inteiro.
static void fonte(Stream *s, const char *provedor, const char *rotulo,
                  const char *descricao, const char *url, int altura,
                  int mp4, int dv) {
  memset(s, 0, sizeof *s);
  snprintf(s->provedor, sizeof s->provedor, "%s", provedor);
  snprintf(s->rotulo, sizeof s->rotulo, "%s", rotulo);
  snprintf(s->descricao, sizeof s->descricao, "%s", descricao);
  snprintf(s->url, sizeof s->url, "%s", url);
  s->altura = altura;
  s->mp4 = mp4;
  s->dolbyVision = dv;
}

// A mesma coisa, mais o campo que o addon declara. Separada de `fonte` de
// proposito: as listas das secoes 1 a 7 NAO tem bingeGroup nenhum, e e assim
// que se prova que tudo que existia antes continua funcionando sem o campo.
static void fonteBinge(Stream *s, const char *provedor, const char *rotulo,
                       const char *descricao, const char *url, int altura,
                       const char *binge) {
  fonte(s, provedor, rotulo, descricao, url, altura, 0, 0);
  snprintf(s->bingeGroup, sizeof s->bingeGroup, "%s", binge);
}

// A RESPOSTA DE UM ADDON, com behaviorHints.
//
// DE ONDE VEM ESTE FORMATO: nao ha, nesta maquina, nenhuma resposta de addon
// gravada com bingeGroup dentro — nem no repositorio, nem nos .wgt/.ipk, nem
// no NuvioWeb-0.3.38-beta (o unico bingeGroup por la esta em codigo JS e num
// mock de teste, scripts/test-plugin-system.mjs). O formato aqui e DEDUZIDO do
// leitor do app web (streamAutoPlaySelector.js:94, que le
// stream.behaviorHints.bingeGroup) e da convencao do Stremio. O que o teste
// prova e o LEITOR, nao o formato.
static const char *JSON_BINGE =
  "{\"streams\":["
    /* 0: o caso normal. filename e proxyHeaders convivem com bingeGroup. */
    "{\"name\":\"Torrentio\\n1080p\","
     "\"title\":\"Serie.S01E01.1080p.DUBLADO.mkv\","
     "\"url\":\"https://exemplo.invalido/dub/1.mkv\","
     "\"behaviorHints\":{\"filename\":\"Serie.S01E01.1080p.DUBLADO.mkv\","
       "\"bingeGroup\":\"torrentio|1080p|dublado\","
       "\"proxyHeaders\":{\"request\":{\"Referer\":\"https://exemplo.invalido\"}}}},"
    /* 1: SEM behaviorHints nenhum. O bingeGroup do 2 vem logo depois no
       texto — e o que a guarda `bh < fim` impede de ser herdado. */
    "{\"name\":\"Torrentio\\n4k\",\"title\":\"Serie.S01E01.2160p.WEB-DL.mp4\","
     "\"url\":\"https://exemplo.invalido/4k/1.mp4\"},"
    /* 2: so o bingeGroup dentro do behaviorHints. */
    "{\"name\":\"AIOStreams\\n1080p\",\"title\":\"Dual Audio\","
     "\"url\":\"https://exemplo.invalido/dual/1.mkv\","
     "\"behaviorHints\":{\"bingeGroup\":\"aiostreams|realdebrid|1080p\"}},"
    /* 3: bingeGroup NULO. pluginManager.js:341 emite exatamente isso, entao
       nao e caso inventado: tem de virar campo vazio, nao a palavra "null". */
    "{\"name\":\"Comet\",\"title\":\"Serie.S01E01.720p.mkv\","
     "\"url\":\"https://exemplo.invalido/sd/1.mkv\","
     "\"behaviorHints\":{\"bingeGroup\":null}},"
    /* 4: SINTETICO, e assumido como tal — nenhum addon real escreve um
       cabecalho chamado bingeGroup. Esta aqui porque e o unico jeito de provar
       que a leitura e de PROFUNDIDADE 1 dentro de behaviorHints: uma busca
       solta devolveria "NAO-E-ESTE". */
    "{\"name\":\"MediaFusion\",\"title\":\"Serie.S01E01.1080p.mkv\","
     "\"url\":\"https://exemplo.invalido/mf/1.mkv\","
     "\"behaviorHints\":{\"proxyHeaders\":{\"request\":{\"bingeGroup\":\"NAO-E-ESTE\"}},"
       "\"bingeGroup\":\"mediafusion|pt|1080p\"}}"
  "]}";

// A lista de um episodio. `ep` entra no nome do arquivo e na url, que e
// exatamente o que muda entre um episodio e o seguinte.
//
//   0  Torrentio 4K DV MP4, sem marca de idioma   <- a que a pontuacao escolhe
//   1  Torrentio 1080p legendado (EN)
//   2  AIOStreams 1080p Dual Audio
//   3  Torrentio 1080p DUBLADO, bandeira do Brasil  <- a que a pessoa escolhe
//   4  Outro Addon 720p
static int listaDoEpisodio(Stream *v, int ep, int comDublado) {
  char d[300], u[200];
  int n = 0;
  snprintf(d, sizeof d, "Serie.S01E%02d.2160p.WEB-DL.DV.HDR.mp4\n1 GB", ep);
  snprintf(u, sizeof u, "https://exemplo.invalido/4k/%d.mp4", ep);
  fonte(&v[n++], "Torrentio", "Torrentio\n4k", d, u, 2160, 1, 1);

  snprintf(d, sizeof d, "Serie.S01E%02d.1080p.WEB.x264\nEnglish", ep);
  snprintf(u, sizeof u, "https://exemplo.invalido/en/%d.mkv", ep);
  fonte(&v[n++], "Torrentio", "Torrentio\n1080p", d, u, 1080, 0, 0);

  snprintf(d, sizeof d, "Serie.S01E%02d.1080p.DUAL.mkv\nDual Audio", ep);
  snprintf(u, sizeof u, "https://exemplo.invalido/dual/%d.mkv", ep);
  fonte(&v[n++], "AIOStreams", "AIOStreams\n1080p", d, u, 1080, 0, 0);

  if (comDublado) {
    snprintf(d, sizeof d, "Serie.S01E%02d.1080p.DUBLADO.mkv\n\xf0\x9f\x87\xa7\xf0\x9f\x87\xb7 Dublado", ep);
    snprintf(u, sizeof u, "https://exemplo.invalido/dub/%d.mkv", ep);
    fonte(&v[n++], "Torrentio", "Torrentio\n1080p", d, u, 1080, 0, 0);
  }

  snprintf(d, sizeof d, "Serie.S01E%02d.720p.mkv", ep);
  snprintf(u, sizeof u, "https://exemplo.invalido/sd/%d.mkv", ep);
  fonte(&v[n++], "Outro Addon", "Outro Addon 720p", d, u, 720, 0, 0);
  return n;
}

// Arranque frio: e o que acontece quando a TV e desligada. A tabela em memoria
// some e a proxima consulta tem de ir ao disco.
static void reiniciar(void) { nTab = 0; carregado = 0; }

static int arquivoExiste(const char *dir, const char *nome) {
  char c[700];
  FILE *f;
  snprintf(c, sizeof c, "%s/%s", dir, nome);
  f = fopen(c, "rb");
  if (f) fclose(f);
  return f != NULL;
}

int main(void) {
  const char *dir = getenv("NUVIO_DADOS");
  Stream v[6];
  int n, k, auto1;
  char t[FONTEPREF_TRILHA];

  // A TRAVA, antes de tudo. Ver o cabecalho.
  if (!dir || !dir[0]) {
    printf("fontepref: NUVIO_DADOS nao esta no ambiente; recusando rodar\n");
    return 2;
  }
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) {
    printf("fontepref: dados_dir() e \"%s\" e NUVIO_DADOS e \"%s\"; recusando rodar\n",
           dados_dir(), dir);
    return 2;
  }
  printf("fontepref: escrevendo em %s\n", dados_dir());

  // ------------------------------------------ 0. o campo que o addon declara
  // O LEITOR, antes de tudo: sem ele nao ha camada 1 nenhuma. Ver a nota em
  // JSON_BINGE sobre de onde o formato veio.
  { Stream *w = NULL;
    int nb = stream_extrair(JSON_BINGE, "Fixture", &w);
    CONFERE(nb == 5, "as cinco fontes do fixture entraram (%d)", nb);
    if (nb == 5) {
      CONFERE(!strcmp(w[0].bingeGroup, "torrentio|1080p|dublado"),
              "bingeGroup lido de dentro de behaviorHints: \"%s\"", w[0].bingeGroup);
      // A GUARDA `bh < fim`. Sem ela, strstr acha o behaviorHints da fonte
      // SEGUINTE e esta fonte sai com o bingeGroup de outra — calada.
      CONFERE(!w[1].bingeGroup[0],
              "fonte sem behaviorHints NAO herda o da seguinte: \"%s\"", w[1].bingeGroup);
      CONFERE(!strcmp(w[2].bingeGroup, "aiostreams|realdebrid|1080p"),
              "behaviorHints so com bingeGroup tambem le: \"%s\"", w[2].bingeGroup);
      CONFERE(!w[3].bingeGroup[0],
              "\"bingeGroup\":null vira campo vazio, nao a palavra null: \"%s\"",
              w[3].bingeGroup);
      CONFERE(!strcmp(w[4].bingeGroup, "mediafusion|pt|1080p"),
              "a leitura e de profundidade 1: proxyHeaders nao vence: \"%s\"",
              w[4].bingeGroup);
      // O que ja se lia continua lido: o campo novo nao pode ter deslocado
      // nada do que stream_parse.c ja extraia do mesmo objeto.
      CONFERE(!strcmp(w[0].arquivo, "Serie.S01E01.1080p.DUBLADO.mkv"),
              "behaviorHints.filename continua lido: \"%s\"", w[0].arquivo);
      CONFERE(w[1].altura == 2160, "a altura continua saindo do texto (%d)", w[1].altura);
    }
    free(w); }

  // ---------------------------------------------------------------- assinatura
  fonte(&v[0], "Torrentio", "Torrentio\n1080p",
        "Serie.S01E03.1080p.DUBLADO.mkv\n\xf0\x9f\x87\xa7\xf0\x9f\x87\xb7 Dublado",
        "https://x.invalido/a", 1080, 0, 0);
  fontepref_trilha(&v[0], t, sizeof t);
  CONFERE(!strcmp(t, "BR+DUB"), "bandeira e palavra viram uma assinatura so: \"%s\"", t);

  // MESMA FONTE, OUTRA ORDEM E COM ACENTO: a assinatura nao pode mudar. Sem a
  // ordenacao e sem normalizar(), o mesmo grupo escrevendo "Dublado 🇧🇷" no
  // episodio seguinte geraria outra chave e o casamento morreria em silencio.
  fonte(&v[1], "Torrentio", "Torrentio\n1080p",
        "Serie.S01E04.1080p.DUBLADO.mkv\nDUBLAGEM \xf0\x9f\x87\xa7\xf0\x9f\x87\xb7",
        "https://x.invalido/b", 1080, 0, 0);
  fontepref_trilha(&v[1], t, sizeof t);
  CONFERE(!strcmp(t, "BR+DUB"), "ordem invertida da a MESMA assinatura: \"%s\"", t);

  fonte(&v[2], "AIOStreams", "AIOStreams\n1080p", "Dual \xc3\x81udio - Portugu\xc3\xaas",
        "https://x.invalido/c", 1080, 0, 0);
  fontepref_trilha(&v[2], t, sizeof t);
  CONFERE(!strcmp(t, "DUAL+POR"), "acento nao muda a assinatura: \"%s\"", t);

  fonte(&v[3], "Torrentio", "Torrentio\n4k", "Serie.S01E03.2160p.WEB-DL.mp4",
        "https://x.invalido/d", 2160, 1, 0);
  fontepref_trilha(&v[3], t, sizeof t);
  CONFERE(!t[0], "fonte sem marca de idioma tem assinatura vazia: \"%s\"", t);

  // "Dubai" nao e dublagem e "Legacy" nao e legenda: a fronteira de token e o
  // que separa os dois, e ela ja custou caro em stream_parse.c.
  fonte(&v[4], "Torrentio", "Dubai.Legacy.2024.1080p", "Dubai Legacy",
        "https://x.invalido/e", 1080, 0, 0);
  fontepref_trilha(&v[4], t, sizeof t);
  CONFERE(!t[0], "\"Dubai\"/\"Legacy\" nao viram DUB/LEG: \"%s\"", t);

  // ------------------------------------------------- 1. nada lembrado = de hoje
  fontepref_definir_perfil(1);
  fontepref_iniciar();
  n = listaDoEpisodio(v, 1, 1);
  stream_definir_lista(v, n);
  auto1 = stream_automatico();
  CONFERE(auto1 == 0, "sem preferencia, o automatico segue a pontuacao (4K DV MP4): %d", auto1);
  CONFERE(fontepref_escolher("tt1000001:1:1") == -1,
          "sem preferencia, fontepref_escolher devolve -1");
  CONFERE(!fontepref_tem("tt1000001"), "sem preferencia, fontepref_tem e 0");

  // ------------------------------------------- 2. a lembrada e preferida depois
  // A pessoa abre a folha e escolhe a DUBLADA (indice 3), que a pontuacao
  // nunca escolheria.
  CONFERE(fontepref_guardar("tt1000001:1:1", stream_item(3)) == 1,
          "a escolha manual e guardada");
  CONFERE(fontepref_tem("tt1000001:1:9"),
          "a preferencia e do TITULO: qualquer episodio dele a encontra");

  // EPISODIO SEGUINTE: outra lista, outras urls, outros nomes de arquivo.
  n = listaDoEpisodio(v, 2, 1);
  stream_definir_lista(v, n);
  k = fontepref_escolher("tt1000001:1:2");
  CONFERE(k == 3, "no episodio seguinte a lembrada e reencontrada (%d)", k);
  CONFERE(k != stream_automatico(),
          "e ela NAO e a que a pontuacao escolheria (%d)", stream_automatico());
  CONFERE(strstr(stream_item(k)->url, "/dub/") != NULL,
          "e a fonte reencontrada e mesmo a dublada: %s", stream_item(k)->url);

  // MESMO PROVEDOR, OUTRO AUDIO, NAO SERVE. A fonte 1 tambem e "Torrentio
  // 1080p"; casar so pelo provedor devolveria a legendada — que e a queixa do
  // issue #56, nao a solucao dele.
  CONFERE(k != 1, "casar por provedor sozinho nao pode devolver a legendada");

  // ---------------------------- 2b. MESMA MARCA E UMA A MAIS: ainda e a mesma
  // O episodio seguinte vem "BR DUB 5.1 Dual" no mesmo addon: a trilha de hoje
  // ("BR+DUAL+DUB") CONTEM a lembrada ("BR+DUB"). Casar aqui e o que evita a
  // folha reabrir em Retomar com "escolher a fonte" ligado (rawldon, #72).
  n = listaDoEpisodio(v, 2, 1);
  snprintf(v[3].descricao, sizeof v[3].descricao,
           "Serie.S01E02.1080p.DUBLADO.DUAL.mkv\n\xf0\x9f\x87\xa7\xf0\x9f\x87\xb7 Dublado Dual Audio");
  stream_definir_lista(v, n);
  k = fontepref_escolher("tt1000001:1:2");
  CONFERE(k == 3, "trilha de hoje que contem a lembrada casa (%d)", k);
  // E COM UMA MARCA A MENOS, NAO: "Dual Audio" sozinho nao promete o DUB.
  n = listaDoEpisodio(v, 2, 1);
  snprintf(v[3].descricao, sizeof v[3].descricao, "Serie.S01E02.1080p.DUAL.mkv\nDual Audio");
  stream_definir_lista(v, n);
  k = fontepref_escolher("tt1000001:1:2");
  CONFERE(k == -1, "trilha de hoje sem a marca lembrada nao casa (%d)", k);

  // ------------------------------------------ 3. a lembrada sumiu: cai calado
  n = listaDoEpisodio(v, 3, 0);
  stream_definir_lista(v, n);
  CONFERE(fontepref_escolher("tt1000001:1:3") == -1,
          "fonte lembrada que nao existe mais devolve -1");
  CONFERE(stream_automatico() == 0,
          "e o automatico continua respondendo (%d)", stream_automatico());
  CONFERE(fontepref_tem("tt1000001"),
          "a preferencia NAO e apagada so porque faltou hoje");

  // ---------------------------------------------- 4. sobrevive ao arranque
  CONFERE(arquivoExiste(dir, "fontepref-p1.txt"),
          "o arquivo do perfil 1 esta em NUVIO_DADOS");
  reiniciar();
  n = listaDoEpisodio(v, 4, 1);
  stream_definir_lista(v, n);
  k = fontepref_escolher("tt1000001:1:4");
  CONFERE(k == 3, "depois de reiniciar, a escolha volta do disco (%d)", k);
  { const FontePref *f = fontepref_do_titulo("tt1000001");
    CONFERE(f && !strcmp(f->provedor, "Torrentio"), "provedor lido do disco");
    CONFERE(f && !strcmp(f->trilha, "BR+DUB"), "trilha lida do disco: \"%s\"",
            f ? f->trilha : "(nulo)");
    // O rotulo do Torrentio traz um \n de verdade. Se ele fosse gravado cru, a
    // linha do arquivo viraria duas e a leitura seguinte perderia a tabela.
    CONFERE(f && !strchr(f->rotulo, '\n'), "o rotulo gravado nao tem quebra de linha"); }

  // -------------------------------------------- 5. outro perfil nao herda
  fontepref_definir_perfil(2);
  CONFERE(!fontepref_tem("tt1000001"), "o perfil 2 nao herda a escolha do perfil 1");
  CONFERE(fontepref_escolher("tt1000001:1:4") == -1,
          "e por isso o perfil 2 segue o automatico");
  // O perfil 2 escolhe OUTRA fonte no MESMO titulo.
  CONFERE(fontepref_guardar("tt1000001", stream_item(2)) == 1,
          "o perfil 2 guarda a escolha dele");
  k = fontepref_escolher("tt1000001");
  CONFERE(k == 2, "e reencontra a dele, nao a do perfil 1 (%d)", k);
  CONFERE(arquivoExiste(dir, "fontepref-p2.txt"), "o perfil 2 tem arquivo proprio");

  fontepref_definir_perfil(1);
  k = fontepref_escolher("tt1000001");
  CONFERE(k == 3, "de volta ao perfil 1, a escolha dele esta intacta (%d)", k);

  // ------------------------------------------------- 6. o indice preferido
  // stream_preferir guarda um indice DA LISTA CORRENTE. Uma lista nova tem de
  // zerar: o mesmo indice apontaria para outra fonte no episodio seguinte.
  stream_preferir(k);
  CONFERE(stream_preferida() == k, "stream_preferir guarda o indice");
  n = listaDoEpisodio(v, 5, 1);
  stream_definir_lista(v, n);
  CONFERE(stream_preferida() == -1, "lista nova zera o indice preferido");
  stream_preferir(99);
  CONFERE(stream_preferida() == -1, "indice fora da lista e recusado");

  // ------------------------------- 7. bingeGroup igual vence a heuristica
  // A CAMADA 1. A pessoa escolhe uma fonte que declara bingeGroup; no episodio
  // seguinte a MESMA fonte chega com outro provedor no cadastro de addons e
  // SEM marca de idioma nenhuma no texto — a heuristica de audio nao teria
  // como reencontra-la. O bingeGroup tem.
  fontepref_definir_perfil(3);
  fonte(&v[0], "Torrentio", "Torrentio\n4k", "Serie.S02E01.2160p.WEB-DL.DV.mp4",
        "https://exemplo.invalido/4k/1.mp4", 2160, 1, 1);
  fonteBinge(&v[1], "Torrentio", "Torrentio\n1080p",
             "Serie.S02E01.1080p.LEGENDADO.mkv\nEnglish",
             "https://exemplo.invalido/en/1.mkv", 1080, "torrentio|1080p|leg");
  fonteBinge(&v[2], "Torrentio", "Torrentio\n1080p",
             "Serie.S02E01.1080p.DUBLADO.mkv\n\xf0\x9f\x87\xa7\xf0\x9f\x87\xb7 Dublado",
             "https://exemplo.invalido/dub/1.mkv", 1080, "torrentio|1080p|dub");
  stream_definir_lista(v, 3);
  CONFERE(stream_automatico() == 0, "a pontuacao ainda escolheria a 4K (%d)",
          stream_automatico());
  CONFERE(fontepref_guardar("tt1000003:2:1", stream_item(2)) == 1,
          "a escolha com bingeGroup e guardada");
  { const FontePref *f = fontepref_do_titulo("tt1000003");
    CONFERE(f && !strcmp(f->bingeGroup, "torrentio|1080p|dub"),
            "e o bingeGroup foi junto: \"%s\"", f ? f->bingeGroup : "(nulo)"); }

  // EPISODIO SEGUINTE. A fonte lembrada agora vem por OUTRO provedor e sem
  // nenhuma palavra de idioma: so o bingeGroup a identifica.
  fonte(&v[0], "Torrentio", "Torrentio\n4k", "Serie.S02E02.2160p.WEB-DL.DV.mp4",
        "https://exemplo.invalido/4k/2.mp4", 2160, 1, 1);
  fonteBinge(&v[1], "Outro Addon", "Outro Addon 1080p", "Serie.S02E02.1080p.mkv",
             "https://exemplo.invalido/binge/2.mkv", 1080, "torrentio|1080p|dub");
  fonteBinge(&v[2], "Torrentio", "Torrentio\n1080p",
             "Serie.S02E02.1080p.LEGENDADO.mkv\nEnglish",
             "https://exemplo.invalido/en/2.mkv", 1080, "torrentio|1080p|leg");
  stream_definir_lista(v, 3);
  { const FontePref *f = fontepref_do_titulo("tt1000003");
    CONFERE(f && porTrilha(f) == -1,
            "a heuristica de audio SOZINHA nao acharia nada hoje (%d)",
            f ? porTrilha(f) : -2); }
  k = fontepref_escolher("tt1000003:2:2");
  CONFERE(k == 1, "o bingeGroup reencontra a fonte mesmo assim (%d)", k);
  CONFERE(k >= 0 && strstr(stream_item(k)->url, "/binge/") != NULL,
          "e e mesmo a fonte declarada pelo addon: %s",
          k >= 0 ? stream_item(k)->url : "(nenhuma)");
  CONFERE(k != stream_automatico(), "e nao e a que a pontuacao escolheria (%d)",
          stream_automatico());

  // O CAMPO NOVO TEM DE SOBREVIVER AO DISCO. Ele entrou no FIM da linha, e uma
  // coluna a mais so vale se voltar inteira depois de a TV desligar.
  reiniciar();
  { const FontePref *f = fontepref_do_titulo("tt1000003");
    CONFERE(f && !strcmp(f->bingeGroup, "torrentio|1080p|dub"),
            "o bingeGroup volta do disco: \"%s\"", f ? f->bingeGroup : "(nulo)");
    CONFERE(f && !strcmp(f->trilha, "BR+DUB"),
            "e a trilha ao lado dele nao se deslocou: \"%s\"", f ? f->trilha : "(nulo)");
    CONFERE(f && !strchr(f->rotulo, '\t'),
            "e o rotulo, agora separado por TAB, continua inteiro: \"%s\"",
            f ? f->rotulo : "(nulo)"); }
  k = fontepref_escolher("tt1000003:2:2");
  CONFERE(k == 1, "e depois do arranque frio o bingeGroup ainda casa (%d)", k);

  // ARQUIVO DA 1.0.55, com SEIS campos e sem a coluna nova. Ele tem de ser
  // lido inteiro: se o bingeGroup tivesse entrado ANTES do rotulo, o rotulo
  // desta linha pousaria dentro do bingeGroup e casaria com nada.
  { char c[700];
    FILE *g;
    snprintf(c, sizeof c, "%s/fontepref-p5.txt", dir);
    g = fopen(c, "wb");
    if (g) {
      fprintf(g, "# nuvio fontes v1\n");
      fprintf(g, "tt1000005\t%lld\t1080\tBR+DUB\tTorrentio\tTorrentio 1080p\n",
              (long long)time(NULL));
      fclose(g);
    }
    CONFERE(g != NULL, "o arquivo de seis campos da 1.0.55 foi escrito");
    fontepref_definir_perfil(5);
    { const FontePref *f = fontepref_do_titulo("tt1000005");
      CONFERE(f && !strcmp(f->provedor, "Torrentio"),
              "arquivo velho: provedor lido \"%s\"", f ? f->provedor : "(nulo)");
      CONFERE(f && !strcmp(f->trilha, "BR+DUB"),
              "arquivo velho: trilha lida \"%s\"", f ? f->trilha : "(nulo)");
      CONFERE(f && !strcmp(f->rotulo, "Torrentio 1080p"),
              "arquivo velho: o ROTULO nao caiu na coluna nova \"%s\"",
              f ? f->rotulo : "(nulo)");
      CONFERE(f && !f->bingeGroup[0],
              "arquivo velho: bingeGroup vazio, que e a verdade \"%s\"",
              f ? f->bingeGroup : "(nulo)"); } }
  fontepref_definir_perfil(3);

  // ------------------------- 8. bingeGroup que mudou cai na heuristica
  // O addon trocou o rotulo de agrupamento (versao nova, outro servico de
  // debrid) sem trocar de fonte. A camada 1 erra o alvo e a camada 2 — a
  // assinatura de audio da 1.0.55 — tem de pegar.
  fonte(&v[0], "Torrentio", "Torrentio\n4k", "Serie.S02E03.2160p.WEB-DL.DV.mp4",
        "https://exemplo.invalido/4k/3.mp4", 2160, 1, 1);
  fonteBinge(&v[1], "Torrentio", "Torrentio\n1080p",
             "Serie.S02E03.1080p.DUBLADO.mkv\n\xf0\x9f\x87\xa7\xf0\x9f\x87\xb7 Dublado",
             "https://exemplo.invalido/dub/3.mkv", 1080, "torrentio|1080p|dub|v2");
  fonteBinge(&v[2], "Torrentio", "Torrentio\n1080p",
             "Serie.S02E03.1080p.LEGENDADO.mkv\nEnglish",
             "https://exemplo.invalido/en/3.mkv", 1080, "torrentio|1080p|leg");
  stream_definir_lista(v, 3);
  { const FontePref *f = fontepref_do_titulo("tt1000003");
    CONFERE(f && porBinge(f) == -1,
            "nenhum bingeGroup de hoje casa com o guardado (%d)",
            f ? porBinge(f) : -2); }
  k = fontepref_escolher("tt1000003:2:3");
  CONFERE(k == 1, "e a assinatura de audio reencontra a dublada mesmo assim (%d)", k);
  CONFERE(k >= 0 && strstr(stream_item(k)->url, "/dub/") != NULL,
          "e e mesmo a dublada: %s", k >= 0 ? stream_item(k)->url : "(nenhuma)");

  // -------------------- 9. lista sem o campo se comporta como na 1.0.55
  // Preferencia guardada de uma fonte SEM bingeGroup, lista SEM bingeGroup: a
  // camada 1 tem de ficar inerte e o resultado tem de ser exatamente o da
  // versao anterior.
  fontepref_definir_perfil(4);
  n = listaDoEpisodio(v, 8, 1);
  stream_definir_lista(v, n);
  CONFERE(fontepref_guardar("tt1000004:1:8", stream_item(3)) == 1,
          "o perfil 4 guarda a dublada de uma lista sem bingeGroup");
  { const FontePref *f = fontepref_do_titulo("tt1000004");
    CONFERE(f && !f->bingeGroup[0],
            "e o bingeGroup guardado fica vazio: \"%s\"", f ? f->bingeGroup : "(nulo)"); }
  n = listaDoEpisodio(v, 9, 1);
  stream_definir_lista(v, n);
  { const FontePref *f = fontepref_do_titulo("tt1000004");
    CONFERE(f && porBinge(f) == -1, "a camada 1 fica inerte sem o campo (%d)",
            f ? porBinge(f) : -2); }
  k = fontepref_escolher("tt1000004:1:9");
  CONFERE(k == 3, "e o resultado e o mesmo da 1.0.55 (%d)", k);

  // ----------------------------------------- 10. preferencia vencida
  // O prazo esta em FONTEPREF_VALIDADE_S. Aqui o relogio nao anda: o que se
  // mexe e o carimbo da linha, que e o mesmo efeito visto de tras para frente.
  { int idx = achar("tt1000004");
    long long agora = (long long)time(NULL);
    CONFERE(idx >= 0, "a linha do tt1000004 esta na tabela (%d)", idx);

    // NO LIMITE AINDA VALE. `>` e nao `>=` em venceu(): um dia a menos que o
    // prazo nao pode expirar.
    tabela[idx].quandoS = agora - FONTEPREF_VALIDADE_S;
    CONFERE(fontepref_tem("tt1000004"), "exatamente no prazo ainda vale");

    tabela[idx].quandoS = agora - FONTEPREF_VALIDADE_S - 24 * 3600;
    CONFERE(fontepref_do_titulo("tt1000004") == NULL,
            "um dia depois do prazo a preferencia some");
    // ISSUE #57 DO OUTRO LADO: e fontepref_tem que decide se detail.c toca
    // direto ou abre a folha. Vencida, ele tem de voltar a perguntar.
    CONFERE(!fontepref_tem("tt1000004"), "e fontepref_tem volta a ser 0");
    CONFERE(fontepref_escolher("tt1000004:1:9") == -1,
            "vencida, fontepref_escolher devolve -1");
    CONFERE(stream_automatico() == 0, "e o automatico assume (%d)", stream_automatico());

    // A LINHA NAO FOI APAGADA. Leitura nao grava; ela morre na proxima escolha
    // ou como a mais antiga quando a tabela encher.
    CONFERE(achar("tt1000004") == idx, "mas a linha continua no arquivo (%d)",
            achar("tt1000004"));

    // RELOGIO PARA TRAS. Esta TV arranca antes de a rede subir, e ai tudo que
    // foi gravado parece estar no futuro. Isso NAO pode expirar nada — e o
    // ponto em que este arquivo diverge do app web de proposito.
    tabela[idx].quandoS = agora + 30LL * 24 * 3600;
    CONFERE(fontepref_tem("tt1000004"), "carimbo no futuro nao expira (relogio da TV)");

    // SEM CARIMBO vale para sempre: linha de arquivo velho, ou atoll que
    // falhou. Expirar por ausencia de data apagaria a escolha de todo mundo na
    // primeira atualizacao.
    tabela[idx].quandoS = 0;
    CONFERE(fontepref_tem("tt1000004"), "linha sem carimbo nao expira");

    // E REESCOLHER RECARIMBA. Sem a excecao do venceu() em fontepref_guardar,
    // a mesma escolha seria recusada por ser igual e a linha ficaria vencida
    // para sempre.
    tabela[idx].quandoS = agora - FONTEPREF_VALIDADE_S - 24 * 3600;
    CONFERE(fontepref_guardar("tt1000004:1:9", stream_item(3)) == 1,
            "escolher de novo a MESMA fonte recarimba a linha vencida");
    CONFERE(fontepref_tem("tt1000004"), "e ela volta a valer");
    k = fontepref_escolher("tt1000004:1:9");
    CONFERE(k == 3, "e volta a ser reencontrada (%d)", k); }

  fontepref_definir_perfil(1);

  // ------------------------------------------------------- 11. sair da conta
  fontepref_esquecer();
  CONFERE(!arquivoExiste(dir, "fontepref-p1.txt"), "sair da conta apaga o perfil 1");
  CONFERE(!arquivoExiste(dir, "fontepref-p2.txt"), "sair da conta apaga o perfil 2");
  CONFERE(!arquivoExiste(dir, "fontepref-p3.txt"), "e o perfil 3 tambem");
  CONFERE(!arquivoExiste(dir, "fontepref-p4.txt"), "e o perfil 4 tambem");
  CONFERE(!arquivoExiste(dir, "fontepref-p5.txt"), "e o arquivo velho do perfil 5");
  CONFERE(!fontepref_tem("tt1000001"), "e a tabela em memoria fica vazia");
  CONFERE(fontepref_escolher("tt1000001") == -1, "depois de sair, tudo volta ao automatico");

  printf(falhas ? "fontepref: %d falha(s)\n" : "fontepref: tudo ok\n", falhas);
  return falhas ? 1 : 0;
}
