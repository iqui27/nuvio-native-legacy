#include "tirarremoto.h"
#include "syncprog.h"
#include "perfisessao.h"
#include <stdio.h>
#include <string.h>
// Optional only in existing isolated fixtures. Complete apps link these.
extern int perfis_ativo(void) NV_PERFIL_OPCIONAL;
extern const char *sessao_usuario(void) NV_PERFIL_OPCIONAL;
extern int syncprog_remover_sessao(const char *, int, uint64_t, const char *) NV_PERFIL_OPCIONAL;
extern int syncprog_remocao_capturar(SyncProgRemocao *) NV_PERFIL_OPCIONAL;
extern int syncprog_remover_pedido(const SyncProgRemocao *, const char *) NV_PERFIL_OPCIONAL;
int tirarremoto_capturar(TirarRemotoPedido *p, const char *imdb) {
  if (!p || !imdb || !*imdb) return 0;
  memset(p, 0, sizeof *p); p->sessao = nv_perfil_sessao_ler();
  if (!nv_perfil_sessao_travar(p->sessao)) return 0;
  p->perfil = perfis_ativo ? perfis_ativo() : 1;
  const char *usuario = sessao_usuario ? sessao_usuario() : "";
  snprintf(p->usuario, sizeof p->usuario, "%s", usuario ? usuario : "");
  trakt_remocao_capturar(&p->trakt, imdb);
  if (syncprog_remocao_capturar) {
    syncprog_remocao_capturar(&p->conta);
    if (p->conta.autenticada && strcmp(p->usuario, p->conta.usuario)) {
      memset(&p->conta, 0, sizeof p->conta);
    }
  }
  nv_perfil_sessao_destravar(); return 1;
}
void tirarremoto_executar_pedido(const TirarRemotoPedido *p, const char *chave, int ocultar) {
  if (!p) return;
  trakt_remocao_executar(&p->trakt);
  if (ocultar) trakt_progresso_ocultar_destino(&p->trakt.destino, p->trakt.imdb, 1);
  if (syncprog_remover_pedido) syncprog_remover_pedido(&p->conta, chave);
  else if (syncprog_remover_sessao) syncprog_remover_sessao(chave, p->perfil, p->sessao, p->usuario);
  else if (nv_perfil_sessao_atual(p->sessao)) syncprog_remover(chave);
}
void tirarremoto_executar(const char *imdb, const char *chave, int ocultar) {
  TirarRemotoPedido p;
  if (tirarremoto_capturar(&p, imdb)) tirarremoto_executar_pedido(&p, chave, ocultar);
}
