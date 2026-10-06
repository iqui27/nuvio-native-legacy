// Personal servers as ONE extra source origin of addons.c (a file apart so
// servidores.c/plex.c/jellyfin.c compile and are tested without addons).
//
// D2 (owner, 06/10/2026): the server copy is ALWAYS the first source. The
// sources are published through the origin's notice with k = ORIGEM_SERVIDOR,
// which addons.c places before every addon (streams.c keeps them first and
// pontos() puts them on top of the automatic choice).
#include "addons.h"
#include "badges.h"
#include "idioma.h"
#include "servidores.h"
#include "srvidx.h"
#include <stdio.h>

// "Local · Casa" / "Remoto · Casa": the source chip and card say where the
// copy comes from. Badges come from the text the real media streams produced.
void servidores_rotular(Stream *l, int n) {
  int i;
  for (i = 0; i < n; i++) {
    char t[2400], nome[96];
    snprintf(nome, sizeof nome, "%s", l[i].provedor);
    snprintf(l[i].provedor, sizeof l[i].provedor, "%s \xc2\xb7 %s",
             srvidx_host_local(l[i].url) ? i18n("Local") : i18n("Remoto"), nome);
    snprintf(t, sizeof t, "%s %s", l[i].rotulo, l[i].descricao);
    l[i].badges = badges_detectar(t);
  }
}

static int origem(const char *id, const char *tipo, int (*cancelado)(void *), void *ctx,
                  OrigemAviso aviso, void *avisoU, void *saida) {
  Stream **l = (Stream **)saida;
  int n = servidores_consultar(id, tipo, cancelado, ctx, l);
  if (n > 0 && *l) {
    servidores_rotular(*l, n);
    if (aviso) aviso(avisoU, ORIGEM_SERVIDOR, "", 2, *l, n);
  }
  return n;
}

void servidores_ligar_aos_addons(void) { addons_definir_origem_extra(origem, servidores_casamento_ativo); }
