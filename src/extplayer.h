// Player externo (#501): Interno / Externo / Perguntar sempre. Ver extplayer.c.
#ifndef NV_EXTPLAYER_H
#define NV_EXTPLAYER_H

enum { EXTPLAYER_INTERNO = 0, EXTPLAYER_EXTERNO = 1, EXTPLAYER_PERGUNTAR = 2, EXTPLAYER_APP = 3 };
#define EXTPLAYER_APPS_MAX 12

// Qual modo vale para este titulo. Anime usa `modoAnime`; < 0 = igual ao Player.
// EXTPLAYER_APP (um app instalado, `pkg*`) sem o app instalado vira Perguntar
// (`*recuou` = 1, quem chama registra). Com o app, devolve EXTPLAYER_EXTERNO e
// `*pkg` = pacote; nos outros modos `*pkg` = NULL.
int extplayer_escolher(int anime, int modoFilme, const char *pkgFilme,
                       int modoAnime, const char *pkgAnime,
                       int (*instalado)(const char *pkg), const char **pkg, int *recuou);

// `modo` e o valor do ajuste (0 interno, 1 externo, 2 perguntar); `cabs` os
// cabecalhos do stream ("" = nenhum). Cabecalho nao cabe numa Intent: com ele
// o externo cai no interno e `*avisar` = 1 (quem chama avisa o usuario).
int extplayer_decidir(int modo, const char *cabs, int *avisar);

#ifdef NV_ANDROID
// Players de video instalados (nome ordenado). `atualizar` refaz a consulta.
void        extplayer_atualizar_apps(void);
int         extplayer_n_apps(void);
const char *extplayer_app_pkg(int i);
const char *extplayer_app_nome(int i);
int         extplayer_app_instalado(const char *pkg);
// `anime` = o titulo e anime (regra em artehero.h: artehero_eh_anime).
// 1 = tratou (abriu o externo ou abriu a pergunta; `sair` fecha o player);
// 0 = tocar no interno agora. `interno` roda quando a escolha for o interno.
// Nunca registra a URL.
int  extplayer_tentar(const char *url, const char *titulo, int anime,
                      void (*interno)(const char *url), void (*sair)(void));
// FAROL (6B/6C): o cartao do Resolver pediu o externo NA MARRA — sem consultar
// o ajuste Interno/Externo/Perguntar nem abrir "Onde reproduzir?". `cabs` sao
// os cabecalhos da fonte falha ("" = nenhum; com cabecalhos nao cabe na
// Intent). `interno`/`sair` podem ser NULL quando chamado com o player fechado
// (Resolver em Ajustes): nunca dispara o pipeline interno em segundo plano.
// 1 = tratou (abriu o externo ou recuou no interno), 0 = indisponivel/falhou.
int  extplayer_forcar(const char *url, const char *titulo, const char *cabs,
                      void (*interno)(const char *url), void (*sair)(void));
void extplayer_acao(const char *chave, int botao);
void extplayer_passo(unsigned agora);
#endif
#endif
