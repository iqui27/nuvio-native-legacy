// "O que achou?" nos creditos (reacao.c) e a fila de atividade (atividade.c).
//
//   bash tests/reacao.sh
//
// Sem janela e sem rede: a regra de quando perguntar e pura, o cartao e
// exercitado por reacao_player_atualizar/reacao_evento (sem desenhar), e a
// fila e compilada com ATIV_SEM_FIO — os eventos ficam na fila para o teste
// ler, e nenhum POST sai.
#include "reacao.h"
#include "atividade.h"
#include "recomenda.h"
#include "dados.h"
#include "ajustes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int falhas;
#define CHECA(c, msg) do { if (!(c)) { printf("FALHOU: %s (linha %d)\n", msg, __LINE__); falhas++; } } while (0)

static SDL_Event tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  return e;
}

static CatItem item(const char *imdb, const char *tipo, const char *titulo) {
  CatItem c;
  memset(&c, 0, sizeof c);
  snprintf(c.imdb, sizeof c.imdb, "%s", imdb);
  snprintf(c.tipo, sizeof c.tipo, "%s", tipo);
  snprintf(c.titulo, sizeof c.titulo, "%s", titulo);
  snprintf(c.poster, sizeof c.poster, "https://img.exemplo/p.jpg");
  return c;
}

// Quantas linhas da fila contem `trecho`.
static int naFila(const char *trecho) {
  char l[1700];
  int i, n = 0;
  for (i = 0; i < atividade_fila_n(); i++)
    if (atividade_fila_linha(i, l, sizeof l) && strstr(l, trecho)) n++;
  return n;
}

static void regra(void) {
  // FILME de 100 min, sem marcador: 3 min fixos (2.0.3; era 4,5%), nunca antes da metade.
  CHECA(!reacao_regra_perguntar(0, 0, 0, 1000, 6000, 0), "filme no comeco nao pergunta");
  CHECA(!reacao_regra_perguntar(0, 0, 0, 5600, 6000, 0), "filme a 400 s do fim nao pergunta");
  CHECA(reacao_regra_perguntar(0, 0, 0, 5850, 6000, 0), "filme nos 180 s finais pergunta");
  CHECA(!reacao_regra_perguntar(0, 0, 0, 5800, 6000, 0), "filme a 200 s do fim nao pergunta");
  // Marcador aceito (parte final do filme: 12%, 5-15 min) manda, e so ele.
  CHECA(!reacao_regra_perguntar(0, 0, 0, 5400, 6000, 5500), "filme antes do marcador nao pergunta");
  CHECA(reacao_regra_perguntar(0, 0, 0, 5500, 6000, 5500), "filme no marcador pergunta");
  // Marcador no comeco do filme (#115) e recusado: nao pergunta aos 90 s.
  CHECA(!reacao_regra_perguntar(0, 0, 0, 95, 6000, 90), "marcador de abertura recusado");
  // SERIE: episodio do meio da temporada NUNCA pergunta.
  CHECA(!reacao_regra_perguntar(1, 1, 0, 2390, 2400, 0), "meio da temporada nao pergunta");
  // Fim de temporada (proximo e de outra) e ultimo disponivel: nos 40 s finais (2.0.3).
  CHECA(reacao_regra_perguntar(1, 1, 1, 2370, 2400, 0), "fim de temporada pergunta");
  CHECA(reacao_regra_perguntar(1, 0, 0, 2370, 2400, 0), "ultimo episodio pergunta");
  CHECA(!reacao_regra_perguntar(1, 0, 0, 1000, 2400, 0), "ultimo episodio no meio nao pergunta");
  CHECA(reacao_regra_perguntar(1, 0, 0, 2200, 2400, 2200), "ultimo episodio nos creditos pergunta");
  CHECA(!reacao_regra_perguntar(1, 0, 0, 2190, 2400, 2200), "antes do marcador aceito nao pergunta");
  CHECA(!reacao_regra_perguntar(0, 0, 0, 10, 0, 0), "sem duracao nao pergunta");
  CHECA(reacao_nota_trakt(REACAO_GOSTEI) == 8 && reacao_nota_trakt(REACAO_MAIS_MENOS) == 5 &&
        reacao_nota_trakt(REACAO_NAO) == 2 && reacao_nota_trakt(7) == 0, "notas do Trakt 8/5/2");
}

// Roda `quadros` quadros do player na mesma posicao.
static void quadros(const CatItem *ci, int serie, double pos, double dur, int temProx,
                    int outraTemp, int n, Uint32 *agora) {
  int i;
  for (i = 0; i < n; i++) {
    *agora += 100;
    reacao_player_atualizar(0.1f, *agora, ci, serie, pos, dur, 0, temProx, outraTemp);
  }
}

static void cartao(void) {
  CatItem f = item("tt0111161", "movie", "Um Sonho de Liberdade");
  CatItem s = item("tt0903747", "series", "Breaking Bad");
  Uint32 agora = 1000;
  SDL_Event e;

  // Duracao ainda instavel (primeiros 8 s): nao abre nem no fim.
  reacao_fechar();
  quadros(&f, 0, 5900, 6000, 0, 0, 20, &agora);
  CHECA(!reacao_aberta(), "duracao instavel nao abre");
  quadros(&f, 0, 5900, 6000, 0, 0, 70, &agora);
  CHECA(reacao_aberta(), "filme nos creditos abre o cartao");
  CHECA(reacao_estado("tt0111161") == REACAO_PENDENTE, "ao abrir ja fica pendente");
  // Com a barra do player em pe o cartao nao toma a tecla (ESQUERDA = avanco).
  e = tecla(SDLK_RIGHT);
  CHECA(!reacao_evento(&e, 1), "controles visiveis: tecla passa");
  // BAIXO nunca e dele (o posplay devolve a barra com ele).
  e = tecla(SDLK_DOWN);
  CHECA(!reacao_evento(&e, 0), "BAIXO passa");
  e = tecla(SDLK_RIGHT); CHECA(reacao_evento(&e, 0), "DIREITA anda");
  e = tecla(SDLK_RETURN); CHECA(reacao_evento(&e, 0), "OK escolhe");
  CHECA(!reacao_aberta(), "responder fecha");
  CHECA(reacao_estado("tt0111161") == REACAO_MAIS_MENOS, "OK na segunda opcao = Mais ou menos");
  // Respondida: outra sessao do player nao pergunta de novo.
  reacao_fechar();
  quadros(&f, 0, 5900, 6000, 0, 0, 100, &agora);
  CHECA(!reacao_aberta(), "respondida nao pergunta de novo");

  // SERIE no meio da temporada: nunca.
  reacao_fechar();
  quadros(&s, 1, 2390, 2400, 1, 0, 100, &agora);
  CHECA(!reacao_aberta(), "serie no meio da temporada nao abre");
  // Fim da temporada: abre (40 s finais, 2.0.3). E some sozinho em 8 s, deixando pendente.
  quadros(&s, 1, 2370, 2400, 1, 1, 1, &agora);
  CHECA(reacao_aberta(), "serie no fim da temporada abre");
  quadros(&s, 1, 2370, 2400, 1, 1, 78, &agora);
  CHECA(reacao_aberta(), "ainda no ar antes dos 8 s");
  quadros(&s, 1, 2370, 2400, 1, 1, 3, &agora);
  CHECA(!reacao_aberta(), "some sozinho em 8 s");
  CHECA(reacao_estado("tt0903747") == REACAO_PENDENTE, "sem resposta fica pendente");
  // A mesma sessao nao reabre (uma vez por sessao do player).
  quadros(&s, 1, 2300, 2400, 1, 1, 100, &agora);
  CHECA(!reacao_aberta(), "uma vez por sessao");

  // PAGINA DO TITULO: pendente aparece; CIMA abre; VOLTAR pula e continua pendente.
  CHECA(reacao_detalhe_pendente(&s), "detalhe ve a pendencia");
  CHECA(!reacao_detalhe_pendente(&f), "respondida nao aparece no detalhe");
  CHECA(reacao_detalhe_abrir(&s) && reacao_aberta(), "detalhe abre o cartao");
  e = tecla(SDLK_UP); CHECA(reacao_evento(&e, 1), "no detalhe o cartao e modal");
  e = tecla(SDLK_ESCAPE); CHECA(reacao_evento(&e, 1), "VOLTAR consome");
  CHECA(!reacao_aberta() && reacao_estado("tt0903747") == REACAO_PENDENTE, "VOLTAR pula e mantem pendente");
  CHECA(reacao_detalhe_abrir(&s), "abre de novo");
  e = tecla(SDLK_RETURN); reacao_evento(&e, 1);
  CHECA(reacao_estado("tt0903747") == REACAO_GOSTEI, "OK no foco inicial = Gostei");
  CHECA(!reacao_detalhe_pendente(&s), "respondida some do detalhe");

  // O arquivo do perfil guarda as duas.
  { char *b = dados_ler("reacoes-p1.txt");
    if (!b) b = dados_ler("reacoes.txt");
    CHECA(b && strstr(b, "tt0111161\t0\t") && strstr(b, "tt0903747\t1\t"), "reacoes gravadas por perfil");
    free(b); }
}

static void fila(void) {
  CatItem f = item("tt0068646:0:0", "movie", "O \"Poderoso\" \\ Chefao\nII\t");
  CatItem s = item("tt0903747", "series", "Breaking Bad");
  AtivEvento e;
  char buf[2048], nome[64];
  int i;

  // --- JSON com escape ---------------------------------------------------------
  memset(&e, 0, sizeof e);
  snprintf(e.ev, sizeof e.ev, "fim");
  snprintf(e.imdb, sizeof e.imdb, "tt1");
  snprintf(e.midia, sizeof e.midia, "movie");
  snprintf(e.titulo, sizeof e.titulo, "A \"b\" \\ c\nd\té");
  e.pct = 140; e.seg = 61; e.reacao = 5; e.rec = 42;
  CHECA(atividade_json(&e, buf, sizeof buf) > 0, "json monta");
  CHECA(!strcmp(buf, "{\"ev\":\"fim\",\"imdb\":\"tt1\",\"midia\":\"movie\","
                     "\"titulo\":\"A \\\"b\\\" \\\\ c\\u000ad\\u0009é\",\"poster\":\"\","
                     "\"temporada\":0,\"episodio\":0,\"pct\":100,\"seg\":61,\"reacao\":1,\"rec\":42}"),
        "json com escape, pct preso em 100 e reacao normalizada");
  CHECA(atividade_json(&e, buf, 40) == 0 && buf[0] == 0, "buffer pequeno nao corta JSON");
  atividade_id_puro(buf, sizeof buf, "tt123:2:5");
  CHECA(!strcmp(buf, "tt123"), "id puro");

  if (!recomenda_ativo()) {
    // PACOTE SEM SERVICO: nada entra, nem permitido.
    atividade_definir_permitido(1);
    atividade_player_passo(&f, 0, 0, 10, 6000, 1, 0, 0.1f);
    atividade_salvo(&f, 1);
    CHECA(atividade_fila_n() == 0 && !atividade_envia(), "sem URL nada entra na fila");
    CHECA(atividade_origem("tt0068646", nome, sizeof nome) == 0, "sem URL sem origem");
    return;
  }

  // --- origem --------------------------------------------------------------------
  // Duas recomendacoes do mesmo titulo: vale a MAIS NOVA.
  { char linhas[512];
    snprintf(linhas, sizeof linhas,
             "# nuvio recomendacoes v2\n"
             "5\t100\t0\t0\tmovie\t1972\ttrakt:bia\tBia\ttt0068646\t\t\t92\t\tO Poderoso Chefao\n"
             "9\t200\t0\t0\tmovie\t1972\ttrakt:caio\tCaio\ttt0068646\t\t\t92\t\tO Poderoso Chefao\n");
    dados_gravar("recomendacoes.txt", linhas);
    recomenda_iniciar(); }
  CHECA(atividade_origem("tt0068646:1:2", nome, sizeof nome) == 9 && !strcmp(nome, "Caio"),
        "origem pela recomendacao mais nova");
  atividade_marcar_origem(5, "tt0068646", "Bia");
  CHECA(atividade_origem("tt0068646", nome, sizeof nome) == 5 && !strcmp(nome, "Bia"),
        "origem marcada pelo painel ganha da busca");
  CHECA(atividade_origem("tt9999999", nome, sizeof nome) == 0 && !nome[0], "sem origem = 0");

  // --- desligado: nada sai ---------------------------------------------------------
  atividade_definir_permitido(0);
  atividade_player_passo(&f, 0, 0, 10, 6000, 1, 0, 0.1f);
  atividade_player_saiu(20, 6000, 0);
  atividade_salvo(&f, 1);
  CHECA(atividade_fila_n() == 0 && !atividade_envia(), "permitido 0: nada na fila");

  // --- trecho: inicio, progresso a cada 5 min, abandono ---------------------------
  atividade_definir_permitido(1);
  atividade_player_passo(&f, 0, 0, 10, 6000, 0, 0, 0.1f);
  CHECA(atividade_fila_n() == 0, "pausado nao e inicio");
  atividade_player_passo(&f, 0, 0, 10, 6000, 1, 0, 0.1f);
  CHECA(naFila("\"ev\":\"inicio\",\"imdb\":\"tt0068646\"") == 1, "inicio com id puro");
  CHECA(naFila("\"rec\":5}") == 1, "inicio leva a recomendacao de origem");
  CHECA(naFila("\\\"Poderoso\\\" \\\\ Chefao\\u000aII\\u0009") == 1, "titulo escapado na fila");
  for (i = 0; i < 2990; i++) atividade_player_passo(&f, 0, 0, 300, 6000, 1, 0, 0.1f);
  CHECA(naFila("\"ev\":\"progresso\"") == 0, "progresso nao antes de 5 min");
  for (i = 0; i < 1000; i++) atividade_player_passo(&f, 0, 0, 300, 6000, 0, 0, 0.1f);
  CHECA(naFila("\"ev\":\"progresso\"") == 0, "pausado nao conta para os 5 min");
  for (i = 0; i < 10; i++) atividade_player_passo(&f, 0, 0, 300, 6000, 1, 0, 0.1f);
  CHECA(naFila("\"ev\":\"progresso\",") == 1 && naFila("\"pct\":5,\"seg\":300,") == 1,
        "progresso aos 5 min tocando, com seg do trecho");
  atividade_player_saiu(600, 6000, 0);
  CHECA(naFila("\"ev\":\"abandono\"") == 0, "sair do player nao e mais abandono");
  CHECA(naFila("\"pct\":10,\"seg\":0,") == 1 && naFila(",\"parou\":1}") == 1,
        "parada a 10% = progresso com parou=1 (so fecha o agora)");
  atividade_player_saiu(600, 6000, 0);
  CHECA(naFila(",\"parou\":1}") == 1, "saida sem trecho nao repete");

  // --- fim pelos 90% ou creditos, uma vez so ----------------------------------------
  atividade_definir_permitido(0);   // limpa
  CHECA(atividade_fila_n() == 0, "desligar apaga a fila");
  atividade_definir_permitido(1);
  atividade_player_passo(&f, 0, 0, 5000, 6000, 1, 0, 0.1f);
  atividade_player_passo(&f, 0, 0, 5500, 6000, 1, 1, 0.1f);   // creditos
  atividade_player_passo(&f, 0, 0, 5600, 6000, 1, 1, 0.1f);
  CHECA(naFila("\"ev\":\"fim\"") == 1 && naFila("\"pct\":100,") == 1, "fim nos creditos, uma vez");
  atividade_player_saiu(5900, 6000, 1);
  CHECA(atividade_fila_n() == 2, "saida depois do fim nao manda mais nada");

  // --- serie: trocar de episodio fecha o trecho anterior ------------------------------
  atividade_definir_permitido(0); atividade_definir_permitido(1);
  atividade_player_passo(&s, 1, 3, 1500, 2400, 1, 0, 0.1f);
  atividade_player_passo(&s, 1, 4, 10, 2400, 1, 0, 0.1f);
  CHECA(naFila("\"temporada\":1,\"episodio\":3,\"pct\":62,") == 2, "E3: inicio e parada (progresso)");
  CHECA(naFila("\"ev\":\"progresso\",\"imdb\":\"tt0903747\"") == 1, "parada a 62% = progresso");
  CHECA(naFila("\"ev\":\"inicio\",\"imdb\":\"tt0903747\",\"midia\":\"series\",\"titulo\":\"Breaking Bad\","
               "\"poster\":\"https://img.exemplo/p.jpg\",\"temporada\":1,\"episodio\":4") == 1,
        "E4 comeca outro trecho");
  atividade_player_saiu(2300, 2400, 0);
  CHECA(naFila("\"ev\":\"fim\",\"imdb\":\"tt0903747\"") == 1, "parada a 95% = fim");

  // --- salvo e reacao ---------------------------------------------------------------------
  atividade_definir_permitido(0); atividade_definir_permitido(1);
  atividade_salvo(&s, 0);
  CHECA(atividade_fila_n() == 0, "remover da lista nao e evento");
  atividade_salvo(&s, 1);
  CHECA(naFila("\"ev\":\"salvo\",\"imdb\":\"tt0903747\"") == 1, "+ salvou = salvo");
  reacao_responder("tt0068646", REACAO_NAO);
  CHECA(naFila("\"ev\":\"reacao\",\"imdb\":\"tt0068646\"") == 1 && naFila("\"reacao\":-1,") == 1,
        "reacao vai para a fila");

  // --- fila cheia: sai o mais velho; e ela sobrevive no disco ------------------------------
  atividade_definir_permitido(0); atividade_definir_permitido(1);
  for (i = 0; i < ATIV_FILA_MAX + 8; i++) {
    char id[24];
    CatItem x;
    snprintf(id, sizeof id, "tt%07d", 1000 + i);
    x = item(id, "movie", "X");
    atividade_salvo(&x, 1);
  }
  CHECA(atividade_fila_n() == ATIV_FILA_MAX, "fila presa no teto");
  CHECA(atividade_fila_linha(0, buf, sizeof buf) && strstr(buf, "tt0001008"), "o mais velho saiu");
  atividade_iniciar();   // rele do disco
  CHECA(atividade_fila_n() == ATIV_FILA_MAX, "fila relida do disco");
  atividade_definir_permitido(0);
  atividade_iniciar();
  CHECA(atividade_fila_n() == 0, "desligar apaga tambem o arquivo");
}

int main(void) {
  const char *dir = getenv("NUVIO_DADOS");
  if (!dir || !*dir) { puts("reacao: NUVIO_DADOS ausente; recusando rodar"); return 1; }
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) { puts("reacao: dados_dir() nao e NUVIO_DADOS; recusando rodar"); return 1; }
  regra();
  cartao();
  fila();
  if (falhas) { printf("reacao: %d falha(s)\n", falhas); return 1; }
  printf("reacao: tudo ok (%s)\n", recomenda_ativo() ? "com servico" : "sem servico");
  return 0;
}
