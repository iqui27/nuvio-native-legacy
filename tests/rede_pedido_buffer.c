/* Allocation boundary and OOM tests exercise the actual receive callbacks. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static size_t maior, chamadas;
static int falhar;
#ifdef NV_TPK40
void nv_tpk40_etapa(const char *s) { (void)s; }
#endif
static void *alocar(void *p, size_t n) {
  chamadas++;
  if (n > maior) maior = n;
  return falhar ? NULL : realloc(p, n);
}
#define realloc alocar
#include "../src/rede.c"
#undef realloc
static int receberFluxo(const void *b, size_t n, void *u) {
  assert(b && n == 32); (*(int *)u)++; return 1;
}
int main(void) {
  RedePedido p = {0}; RedeResposta r = {0}; PedidoCtx c = {0};
  char dados[4096]; memset(dados, 'x', sizeof dados);
  c.p = &p; c.r = &r; c.inicio = pedidoAgoraMs(); c.prazo = c.inicio + 15000;
  c.tetoCorpo = 127; c.tetoCab = 128;
  assert(!pedidoCorpo(dados, 1, 128, &c));
  assert(r.erro == REDE_LIMITE_CORPO && !r.corpo && chamadas == 0);
  r.erro = 0;
  assert(pedidoCorpo(dados, 1, 127, &c) == 127);
  assert(c.corpoCap == 128 && maior == 128 && r.corpo[127] == 0);
  assert(!pedidoCorpo(dados, 1, 1, &c));
  assert(r.erro == REDE_LIMITE_CORPO && r.n_corpo == 127);
  rede_resposta_limpar(&r); c.corpoCap = 0; chamadas = maior = 0;
  {
    const char len[] = "Content-Length: 4294967295\r\n";
    assert(!pedidoCab((void *)len, 1, sizeof len - 1, &c));
    assert(r.erro == REDE_LIMITE_CORPO && !r.corpo && chamadas == 0);
  }
  r.erro = 0; c.vistosCab = 0;
  assert(!pedidoCab(dados, 1, 129, &c));
  assert(r.erro == REDE_LIMITE_CABECALHOS && !r.cabecalhos && chamadas == 0);
  r.erro = 0; c.vistosCab = 0; falhar = 1;
  assert(!pedidoCab(dados, 1, 64, &c));
  assert(r.erro == REDE_MEMORIA && !r.cabecalhos && maior == 129);
  r.erro = 0; chamadas = maior = 0;
  assert(!pedidoCorpo(dados, 1, 64, &c));
  assert(r.erro == REDE_MEMORIA && !r.corpo && maior == 128);
  r.erro = 0; chamadas = 0; falhar = 0;
  assert(!pedidoCorpo(dados, SIZE_MAX, 2, &c));
  assert(r.erro == REDE_LIMITE_CORPO && chamadas == 0);
  r.erro = 0; c.vistosCab = 0;
  assert(!pedidoCab(dados, SIZE_MAX, 2, &c));
  assert(r.erro == REDE_LIMITE_CABECALHOS && chamadas == 0);
  {
    RedeGrupo *g = rede_grupo_criar(); RedeJob *j = rede_job_criar(g);
    p.job = j; r.erro = 0; rede_grupo_avancar(g);
    assert(!pedidoCorpo(dados, 1, 32, &c));
    assert(r.erro == REDE_GERACAO && chamadas == 0);
    rede_grupo_soltar(g); rede_job_soltar(j); p.job = NULL;
  }
  r.erro = 0; c.prazo = pedidoAgoraMs();
  assert(!pedidoCorpo(dados, 1, 32, &c));
  assert(r.erro == REDE_PRAZO && chamadas == 0);
  rede_resposta_limpar(&r);
  c = (PedidoCtx){0}; r = (RedeResposta){0};
  c.p = &p; c.r = &r; c.inicio = pedidoAgoraMs(); c.prazo = c.inicio + 15000;
  p.janela_corpo_ms = 60000; p.max_descartado = 4096;
  chamadas = maior = 0;
  assert(pedidoCorpo(dados, 1, sizeof dados, &c) == sizeof dados);
  assert(!chamadas && !r.corpo && !r.n_corpo && r.n_prefixo == 512);
  assert(!pedidoCorpo(dados, 1, 1, &c) && r.fim_teto && !chamadas);
  assert(c.descartado == 4096); // strict accepted-byte cap; no media allocation
  rede_resposta_limpar(&r);
  p.janela_corpo_ms=0; p.receber_corpo=receberFluxo;
  int recebidos=0; p.corpo_usuario=&recebidos; c.status=302; c.tetoCorpo=64;
  assert(pedidoCorpo(dados,1,32,&c)==32 && !recebidos && !chamadas);
  c.status=200; r.n_corpo=0;
  assert(pedidoCorpo(dados,1,32,&c)==32 && recebidos==1);
  assert(!r.corpo && !chamadas && r.n_corpo==32);
  assert(!pedidoCorpo(dados,1,33,&c) && r.erro==REDE_LIMITE_CORPO);
  puts("rede_pedido_buffer: PASS (before allocation, exact caps, OOM, multiplication, stale/deadline)");
}
