// A FONTE LEMBRADA: assinatura, casamento entre episodios, disco e perfil.
//
//   bash tests/fontepref.sh
//
// Cobre os issues #56 (o episodio seguinte tem de continuar na mesma fonte) e
// #57 (Retomar nao pode abrir a folha de fontes quando ja ha uma escolha).
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

  // ------------------------------------------------------- 7. sair da conta
  fontepref_esquecer();
  CONFERE(!arquivoExiste(dir, "fontepref-p1.txt"), "sair da conta apaga o perfil 1");
  CONFERE(!arquivoExiste(dir, "fontepref-p2.txt"), "sair da conta apaga o perfil 2");
  CONFERE(!fontepref_tem("tt1000001"), "e a tabela em memoria fica vazia");
  CONFERE(fontepref_escolher("tt1000001") == -1, "depois de sair, tudo volta ao automatico");

  printf(falhas ? "fontepref: %d falha(s)\n" : "fontepref: tudo ok\n", falhas);
  return falhas ? 1 : 0;
}
