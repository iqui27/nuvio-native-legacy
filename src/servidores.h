// Personal media servers as one unit: Jellyfin, Emby and Plex.
//
// The app (Home, title page, sources, player, profiles, sign-out) talks to
// THIS file; it fans out to jellyfin.c (instances jellyfin_/emby_) and plex.c.
// Ids route by namespace: "jf:" Jellyfin, "em:" Emby, "px:" Plex (jfid.h).
// Tests that link app modules without any server stub these few symbols (see
// tests/jellyfin_stub.inc).
#ifndef NV_SERVIDORES_H
#define NV_SERVIDORES_H
#include "catalogo.h"
#include "jfid.h"
#include "jellyfin.h"
#include "plex.h"

// Rows a Home can receive from all servers together.
#define SRV_FIL_MAX (JF_FIL_MAX + JF_FIL_MAX + PX_FIL_MAX)
#define SRV_ITENS_MAX (SRV_FIL_MAX * JF_POR_FILEIRA)

void servidores_carregar(void);          // active profile's connections
void servidores_perfil_trocou(void);     // cancels in-flight work, reloads
void servidores_esquecer_todos(void);    // Nuvio account logout: every token file
int  servidores_conectado(void);         // any server signed in
// Offline (index only): a server of this profile has this catalogue title
// ("tt...", "tmdb:...", with or without :season:episode). The source cache
// never keeps a list for such a title: server sources carry a token and a
// play session, and "the server had nothing" is never remembered.
int  servidores_tem(const char *id, const char *tipo);
// Same, and what the title page says: the server's name, kind, best known
// quality and whether it is on the LAN (LAN first when two servers have it).
int  servidores_info(const char *id, const char *tipo, SrvAchado *info);
int  servidores_casamento_ativo(void);   // some server has a matching index
// The page that asks for sources says the title and year (last match rule).
void servidores_dica(const char *id, const char *titulo, int ano);
// Sources of every server of this profile that has this catalogue title.
// BLOCKS (addons.c's extra-source thread). -1 cancelled.
int  servidores_consultar(const char *id, const char *tipo, int (*cancelado)(void *), void *ctx,
                          Stream **saida);
// A source handed out by a personal server (bingeGroup "jellyfin|", "emby|", "plex|").
int  servidores_eh_fonte(const Stream *s);
// servidoresaddons.c: registers the one "servers" origin with addons.c.
void servidores_ligar_aos_addons(void);
// "Local · <server>" / "Remoto · <server>" and badges, on a list of sources.
void servidores_rotular(Stream *l, int n);
#define SRV_REFRESCO_MS (30ull * 60ull * 1000ull)
void servidores_atualizar_periodico(void);   // servidores_passo: rows + index every 30 min
unsigned servidores_fileiras_versao(void);
int  servidores_chave_fileira(const char *chave);
int  servidores_fileiras_copiar(CatItem *itens, int maxItens, CatFileira *fils, int maxFils,
                                int *nItens);
int  servidores_ficha(CatItem *item, CatEp *eps, int maxEps);
int  servidores_fontes_pedir(const char *alvo);
int  servidores_fontes_colher(const char *alvo, Stream **lista, int *n);
// Player check-ins: each module ignores URLs it did not hand out.
void servidores_reproducao_tick(const char *url, double posSeg, double durSeg, int tocando);
void servidores_reproducao_fim(const char *url, double posSeg, double durSeg);
// Per-frame housekeeping for app.c: reloads when the option flips and reports
// whether the Home must be rebuilt. Returns 1 when it should.
int  servidores_passo(void);
#endif
