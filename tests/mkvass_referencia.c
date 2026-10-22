// Regressao 72470: mesma faixa ASS enorme, janela local completa, sem rede.
#include "../src/mkvass.c"
#include "legref.h"
#include "autosync.h"
#include <assert.h>

char *rede_baixar_bin(const char *u, int s, long *n) { (void)u; (void)s; (void)n; return NULL; }
unsigned rede_pedido_capacidades(void) { return REDE_CAP_JOB; }
int rede_pedir(const RedePedido *p, RedeResposta *r) { (void)p; memset(r, 0, sizeof *r); return 0; }
void rede_resposta_limpar(RedeResposta *r) { free(r->corpo); free(r->cabecalhos); }
static int requests;
static unsigned char *semRede(void *u, const char *url, long long ini, long n, long *tam,
                             int *status, int (*parar)(void *), void *pu) {
  (void)u; (void)url; (void)ini; (void)n; (void)tam; (void)parar; (void)pu;
  requests++; *status = 503; return NULL;
}

int main(void) {
  Fio f = {0};
  const char *url = "local:72470";
  f.g = S.geracao = 1; f.faixa = 5; f.escala = 1000000;
  snprintf(f.urlOrig, sizeof f.urlOrig, "%s", url);
  f.nPontos = 15907; f.pontos = calloc((size_t)f.nPontos, sizeof *f.pontos);
  assert(f.pontos);
  const char *cab = "[Script Info]\nScriptType: v4.00+\n[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";
  assert(corpoAnexar(&f, cab, strlen(cab)));
  // Pontos antes/depois da janela nao foram colhidos. Dentro, nenhum buraco.
  for (int i = 0; i < f.nPontos; i++) f.pontos[i].tempo = (unsigned long)(i * 5000);
  for (int i = 20; i < 107; i++) {
    double t = i * 5.0;
    f.pontos[i].colhido = 1; f.nColhidos++;
    char line[200];
    snprintf(line, sizeof line, "Dialogue: 0,0:%02d:%05.2f,0:%02d:%05.2f,Default,,0,0,0,,Fala numero %d\n",
             (int)t / 60, fmod(t, 60), (int)(t + 1 + (i % 7) * .17) / 60, fmod(t + 1 + (i % 7) * .17, 60), i);
    assert(corpoAnexar(&f, line, strlen(line)));
  }
  // Via usada em producao quando se escolhe uma externa: o fio publica ao sair.
  guardarReferencia(&f);
  LegendaDocumento *local = mkvass_referencia(url, 7, NULL, 0);
  assert(local && "72470: 87/15907 blocos locais devem servir como janela de referencia");
  const LegendaDocumentoInfo *info = legenda_documento_info(local);
  assert(info->sessao == 7 && !strcmp(info->identidade, "mkv-track:5"));
  assert(info->flags & LEGENDA_DOC_JANELA);
  assert(!mkvass_referencia("local:outra", 7, NULL, 0));
  int ex = 5; assert(!mkvass_referencia(url, 7, &ex, 1));
  if (!getenv("TESTE_SEM_FONTE_LOCAL")) legref_fonte_local(mkvass_referencia);
  LegRef *r = legref_criar(semRede, NULL); assert(r);
  LegRefOrcamento orc = { 12LL << 20, 6000, 0 };
  uint64_t id = legref_pedir(r, url, 7, "pt", NULL, 0, &orc); assert(id);
  LegRefStatus st;
  do { usleep(1000); st = legref_status(r); } while (st.fase == LEGREF_LENDO);
  assert(requests == 0 && st.pedidos == 0 && st.bytes == 0 && "nao abrir conexao extra para a mesma faixa local");
  assert(st.fase == LEGREF_PRONTO && st.faixa == 5 && st.feitos == 87);
  LegendaDocumento *ref = legref_tomar(r, id); assert(ref);
  int n; const LegendaCue *v = legenda_documento_dados(local, &n);
  LegendaCue *ext = calloc((size_t)n + 2, sizeof *ext); assert(ext);
  for (int i = 0; i < n; i++) { ext[i] = v[i]; ext[i].inicio += 2.5; ext[i].fim += 2.5; }
  LegendaDocumentoInfo ei = { .sessao = 7, .flags = LEGENDA_DOC_COMPLETO };
  snprintf(ei.identidade, sizeof ei.identidade, "externa");
  LegendaDocumento *prim = legenda_documento_de_cues(ext, n, &ei); assert(prim);
  AutoSyncResultado res = autosync_alinhar(prim, ref, NULL, NULL, NULL, NULL);
  assert(res.estado == AUTOSYNC_ACCEPTED && res.tipo == AUTOSYNC_T_OFFSET && abs(res.offsetMs - 2500) < 30);
  puts("PASS: ASS 87/15907 reaproveitada, offset +2500 ms, zero Ranges auxiliares");
  // Cobertura distante nao deve pesar na janela nem inventar fps/cortes.
  ext[n] = ext[0]; ext[n].inicio = 2000; ext[n].fim = 2002;
  LegendaDocumento *longa = legenda_documento_de_cues(ext, n + 1, &ei);
  res = autosync_alinhar(longa, ref, NULL, NULL, NULL, NULL);
  assert(res.estado == AUTOSYNC_ACCEPTED && res.tipo == AUTOSYNC_T_OFFSET && abs(res.offsetMs - 2500) < 30);
  legenda_documento_liberar(longa);
  f.pontos[60].colhido = 0;
  guardarReferencia(&f);
  LegendaDocumento *hole = mkvass_referencia(url, 7, NULL, 0);
  // O buraco divide a janela; nao pode fingir que o intervalo inteiro veio.
  assert(hole && legenda_documento_info(hole)->janelaInicio > 300.0);
  legenda_documento_liberar(hole);
  for (int i = 0; i < f.nPontos; i++) f.pontos[i].colhido = i >= 20 && i < 23;
  guardarReferencia(&f);
  LegendaDocumento *curta = mkvass_referencia(url, 7, NULL, 0); assert(curta);
  res = autosync_alinhar(prim, curta, NULL, NULL, NULL, NULL);
  assert(res.estado != AUTOSYNC_ACCEPTED && res.offsetMs == 0);
  legenda_documento_liberar(curta);
  f.refInvalida = 1; guardarReferencia(&f);
  assert(!mkvass_referencia(url, 7, NULL, 0)); // forced/letreiro/comentario
  f.refInvalida = 0; f.corpoCortou = 1; guardarReferencia(&f);
  assert(!mkvass_referencia(url, 7, NULL, 0));
  f.corpoCortou = 0;
  int total = f.nPontos; f.nPontos = 7999; guardarReferencia(&f);
  assert(!mkvass_referencia(url, 7, NULL, 0)); // faixa menor usa o caminho completo existente
  f.nPontos = total; f.g = 0; guardarReferencia(&f);
  assert(!mkvass_referencia(url, 7, NULL, 0)); // geracao obsoleta nao publica
  puts("PASS: outra URL, faixa excluida, buraco, janela curta e corpo truncado protegidos");
  legref_destruir(r); legenda_documento_liberar(local); legenda_documento_liberar(ref);
  legenda_documento_liberar(prim); free(ext); free(f.pontos); free(f.corpo);
  return 0;
}
