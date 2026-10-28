// Issue #643 fixes: the Simkl history of a LOCAL profile ("perfil:N") must
// respect that profile's own "Recursos sociais" (socialLocal). Off = the
// friend card must come back as "not shared" — empty titles, no watching,
// no liked — exactly like a private remote friend. It never reached the
// permission check because svPerfilDoServidor leaves compartilha=-1 for
// local ids, so the leak was silent.
//
// simkl_vistos_perfil itself is not exercised to the network here: without
// a token file (simkl-p<N>.txt) it returns 0 before any HTTP, which is the
// only deterministic offline path. The memoization only kicks in after a
// real answer; what this pins is that the permission gate runs FIRST, so a
// profile that turned social features off never even asks Simkl.
#include "socialvis.h"
#include "simkl.h"
#include "dados.h"
#include "ajustes.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static SvEvento ev(const char *pid, const char *nome, const char *imdb) {
  SvEvento e;
  memset(&e, 0, sizeof e);
  snprintf(e.pessoaId, sizeof e.pessoaId, "%s", pid);
  snprintf(e.pessoaNome, sizeof e.pessoaNome, "%s", nome);
  e.fonte = SV_FONTE_SIMKL; e.acao = SV_FIM; e.pct = -1; e.restanteMin = -1;
  snprintf(e.imdb, sizeof e.imdb, "%s", imdb);
  snprintf(e.tipo, sizeof e.tipo, "movie");
  snprintf(e.titulo, sizeof e.titulo, "T");
  e.quando = (long long)time(NULL) - 60;
  return e;
}

int main(void) {
  SvEvento v[2];
  SvPerfil p;
  dados_iniciar("deploy/app/art");
  ajustes_dir(dados_dir());

  v[0] = ev("perfil:2", "Dois", "tt0000002");
  v[1] = ev("perfil:3", "Tres", "tt0000003");
  socialvis_definir_feed(v, 2);

  // 1. Sem arquivo de ajustes do perfil: socialLocal vale o padrao (ligado).
  assert(ajustes_social_perfil(2) == 1);
  memset(&p, 0, sizeof p);
  assert(socialvis_perfil("perfil:2", &p));
  assert(p.compartilha == 1);

  // 2. Perfil 2 desliga "Recursos sociais" (socialLocal 1 = Desligado, a
  // segunda opcao do seletor): o card fecha. O feed pode ate
  // trazer um titulo dele — a permissao manda e nada sai na tela.
  assert(dados_gravar("ajustes-p2.txt", "socialLocal 1\n"));
  assert(ajustes_social_perfil(2) == 0);
  memset(&p, 0, sizeof p);
  assert(socialvis_perfil("perfil:2", &p));
  assert(p.compartilha == 0);
  assert(p.a.nTit == 0 && p.a.agora == 0);
  assert(p.nAssistindo == 0 && p.nGostou == 0 && p.nMandou == 0);

  // 3. O perfil ao lado continua aberto: a decisao e por perfil.
  memset(&p, 0, sizeof p);
  assert(socialvis_perfil("perfil:3", &p));
  assert(p.compartilha == 1);

  // 4. Sem token Simkl no perfil pedido: 0 sem tocar a rede (e sem mexer no
  // estado do perfil ativo — simklauth_token_perfil le o arquivo do outro).
  { SimklVisto vs[4];
    assert(!dados_ler("simkl-p3.txt"));
    assert(simkl_vistos_perfil(3, vs, 4) == 0); }

  puts("simkl_amigos ok");
  return 0;
}
