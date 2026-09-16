// O CLIENTE DE VERDADE CONTRA O SERVIDOR DE VERDADE.
//
// tests/recomenda.c intercepta a rede e prova a LOGICA; este prova o
// PROTOCOLO — que a URL, os cabecalhos, o corpo do POST e o ETag sao os que o
// Worker espera. Sao coisas diferentes, e um dublê so confirma o que quem o
// escreveu ja acreditava.
//
// Precisa do servidor local no ar:
//   cd servidor/recomendacoes && npx wrangler@4 dev --local --port 8799
// Sem ele o teste SAI DIZENDO QUE PULOU, e nao falhando: a suite nao pode
// depender de um servico que so existe na maquina de quem o subiu.
//
// A IDENTIDADE E CURTO-CIRCUITADA como em servidor/recomendacoes/teste.sh: uma
// linha em `sessao` com o SHA-256 de "<via>:<token>" poupa a chamada ao
// Supabase, que e o unico ponto do servico que fala com a internet.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NV_REC_URL "http://127.0.0.1:8799"
#include "../src/recomenda.c"

static int falhas;
#define CONFERE(c, ...) do { if (!(c)) { falhas++; printf("FALHA: " __VA_ARGS__); \
                                         printf("\n"); } } while (0)

int main(void) {
  const char *dir = getenv("NUVIO_DADOS");
  const char *cab[3];
  char *saude;

  if (!dir || !dir[0]) {
    printf("recomenda_e2e: NUVIO_DADOS nao esta no ambiente; recusando rodar\n");
    return 2;
  }
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) {
    printf("recomenda_e2e: dados_dir() e \"%s\" e NUVIO_DADOS e \"%s\"; recusando\n",
           dados_dir(), dir);
    return 2;
  }
  rede_preparar();
  saude = rede_baixar(NV_REC_URL "/v1/saude", 4);
  if (!saude || !strstr(saude, "\"ok\"")) {
    free(saude);
    printf("recomenda_e2e: servidor local fora do ar; PULADO\n");
    return 0;
  }
  free(saude);

  // A sessao do usuario e lida do disco por sessao_iniciar: linha 1 token,
  // linha 2 refresh, linha 3 "anonima".
  dados_gravar("sessao.txt", "tok-e2e\n\n0\n");
  sessao_iniciar();
  CONFERE(sessao_token()[0] != 0, "sessao_token devolveu vazio");
  if (!mtx) mtx = SDL_CreateMutex();

  CONFERE(identidade(cab), "identidade montou os cabecalhos");
  CONFERE(strstr(cab[0], "Bearer tok-e2e") != NULL, "Authorization: [%s]", cab[0]);
  CONFERE(!strcmp(cab[1], "X-Nuvio-Auth: nuvio"), "via: [%s]", cab[1]);

  CONFERE(registrar(cab), "POST /v1/eu registrou");
  CONFERE(!strcmp(meuId, "nuvio:e2e"), "id devolvido pelo servidor: [%s]", meuId);

  lerContatos(cab);
  printf("contatos: %d\n", nContatos);
  CONFERE(nContatos >= 1, "o vinculo do .sh apareceu na lista de contatos");

  // Primeira leitura: o servidor responde 200 e manda ETag.
  CONFERE(lerRecs(cab) == 1, "GET /v1/rec respondeu");
  printf("recebidas: %d, cursor %lld, etag %s\n", nItens, cursor, etagRec);
  CONFERE(etagRec[0] == '"', "ETag veio do servidor: [%s]", etagRec);
  CONFERE(nItens >= 1, "a recomendacao plantada pelo .sh chegou (%d)", nItens);
  CONFERE(cursor > 0, "o cursor avancou: %lld", cursor);
  CONFERE(recomenda_n_novas() >= 1, "ela conta para o selo: %d",
          recomenda_n_novas());
  { RecItem r;
    if (recomenda_item(0, &r)) {
      printf("primeira: #%lld \"%s\" de %s, modelo %d\n", r.id, r.titulo,
             r.deNome, r.modelo);
      CONFERE(!strcmp(r.imdb, "tt0111161"), "imdb: [%s]", r.imdb);
      CONFERE(!strcmp(r.titulo, "Um Sonho de Liberdade"), "titulo: [%s]", r.titulo);
      CONFERE(!strcmp(r.deNome, "Amigo E2E"), "quem mandou: [%s]", r.deNome);
      CONFERE(!strcmp(r.ano, "1994"), "ano: [%s]", r.ano);
      CONFERE(r.modelo == 2, "modelo: %d", r.modelo);
      CONFERE(r.criado > 0, "criado: %lld", r.criado);
    } }

  // Enviar de volta, pelo caminho que a interface usa.
  { CatItem ci;
    memset(&ci, 0, sizeof ci);
    snprintf(ci.imdb, sizeof ci.imdb, "%s", "tt0068646");
    snprintf(ci.tipo, sizeof ci.tipo, "%s", "movie");
    snprintf(ci.titulo, sizeof ci.titulo, "%s", "O Poderoso Chefão");
    snprintf(ci.meta, sizeof ci.meta, "%s", "1972 · 2h55");
    CONFERE(recomenda_enviar(&ci, "nuvio:e2e-b", 4, ""), "enviar enfileirou");
    enviarFila(cab);
    CONFERE(recomenda_envio_estado() == REC_ENVIO_OK,
            "o servidor aceitou o envio (estado %d)", recomenda_envio_estado()); }

  // E marcar como vistas, que e o que a aba Social faz ao abrir.
  recomenda_marcar_vistas();
  confirmarVistas(cab);
  CONFERE(nVistoFila == 0, "a fila de vistas foi despachada");

  // Segunda leitura, com o mesmo ETag: TEM de ser 304, e a lista nao muda.
  { int antes = nItens;
    long long cursorAntes = cursor;
    CONFERE(lerRecs(cab) == 1, "a segunda consulta respondeu");
    CONFERE(nItens == antes, "a segunda consulta nao mexeu na lista");
    CONFERE(cursor == cursorAntes, "a segunda consulta nao mexeu no cursor"); }

  printf(falhas ? "recomenda_e2e: %d falhas\n" : "recomenda_e2e: ok\n", falhas);
  return falhas ? 1 : 0;
}
