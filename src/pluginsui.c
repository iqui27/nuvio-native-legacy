// Ver pluginsui.h. Dois niveis:
//   0 = a lista: [Plugins liga/desliga] [Adicionar repositorio] [repositorios...]
//       OK liga/desliga, -> abre os scrapers do repositorio focado.
//   1 = um repositorio: [Remover repositorio] [scrapers...]
//       OK liga/desliga o scraper; remover pede o OK duas vezes.
// Sem motor neste alvo (plugins_disponivel() == 0) so ha a linha que diz isso.
#include "pluginsui.h"
#include "registro.h"
#include "plugins.h"
#include "ajustes.h"
#include "teclado.h"
#include "idioma.h"
#include <stdio.h>
#include <string.h>

static int nivel, foco, repo, armado, sair, focoSalvo, pediuRelogio;
static char aviso[160];

static const char *ALFA_URL =
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:/.-_?=&%#+~@";

static int linhas(void) {
  if (!plugins_disponivel()) return 1;
  if (nivel == 0) return 2 + plugins_n_repos();
  return 1 + plugins_repo_scrapers(repo, NULL);
}

void pluginsui_abrir(void) {
  nivel = 0; foco = 0; repo = 0; armado = 0; sair = 0; pediuRelogio = 0; aviso[0] = 0;
  // Os manifestos se conferem ao abrir (cache de 6 h; fio proprio).
  if (plugins_disponivel()) plugins_atualizar();
}

int pluginsui_quer_sair(void) { int v = sair; sair = 0; return v; }
int pluginsui_pediu_relogio(void) { int v = pediuRelogio; pediuRelogio = 0; return v; }

static void voltar(void) {
  armado = 0; aviso[0] = 0;
  if (nivel == 1) { nivel = 0; foco = focoSalvo; }
  else sair = 1;
}

static void ok(void) {
  if (!plugins_disponivel()) return;
  aviso[0] = 0;
  if (nivel == 0) {
    if (foco == 0) { plugins_definir_ligado(!plugins_ligado()); return; }
    if (foco == 1) {
      teclado_contexto("Plugins");
      teclado_abrir_com("Endereço do repositório", "A URL do manifest.json. Ex.: raw.githubusercontent.com/usuario/repo/main",
                        TECLADO_LONGO, ALFA_URL, "https://");
      return;
    }
    plugins_alternar_repo(foco - 2);
    return;
  }
  if (foco == 0) {
    if (!armado) { armado = 1; snprintf(aviso, sizeof aviso, "%s", i18n("OK de novo remove o repositório")); return; }
    armado = 0;
    if (plugins_remover_repo(repo)) { nivel = 0; foco = repo + 2 < linhas() ? repo + 2 : linhas() - 1; }
    return;
  }
  plugins_alternar_scraper(repo, foco - 1);
}

static void abrirRepo(void) {
  if (plugins_disponivel() && nivel == 0 && foco >= 2 && foco < linhas()) {
    repo = foco - 2; focoSalvo = foco; nivel = 1; foco = 0; armado = 0; aviso[0] = 0;
  }
}

#ifdef NV_TOUCH_UI
void pluginsui_detalhes(int i, int b) {
  (void)b;
  if (sair || teclado_aberto() || nivel != 0 || i < 2 || i >= linhas()) return;
  foco = i;
  abrirRepo();
}
#endif

void pluginsui_evento(const SDL_Event *e) {
  int n;
  pediuRelogio = 0;
  if (teclado_aberto()) { teclado_evento(e); return; }
  if (e->type != SDL_KEYDOWN) return;
  n = linhas();
  switch (e->key.keysym.sym) {
    case SDLK_UP:   if (foco > 0) { foco--; armado = 0; } else pediuRelogio = 1; break;
    case SDLK_DOWN: if (foco < n - 1) { foco++; armado = 0; } break;
    case SDLK_RETURN: case SDLK_KP_ENTER: ok(); break;
    case SDLK_RIGHT:
      abrirRepo();
      break;
    case SDLK_AC_BACK: case SDLK_ESCAPE: case SDLK_BACKSPACE: case SDLK_LEFT: voltar(); break;
    default: break;
  }
}

// PONTEIRO (#99): a linha sob o cursor ganha o foco (a mesma variavel das
// setas, desarmando o "OK de novo remove" como a seta faz). Com o teclado da
// URL aberto ele tem a tela.
void pluginsui_ponteiro(int i, int b) {
  (void)b;
  if (sair || teclado_aberto() || i < 0 || i >= linhas() || i == foco) return;
  foco = i; armado = 0;
}
int pluginsui_foco(void) { return foco; }

void pluginsui_atualizar(float dt, Uint32 agora) {
  int n;
  if (teclado_aberto()) teclado_atualizar(dt, agora);
  if (teclado_resultado() == TECLADO_PRONTO) {
    int r = plugins_adicionar_repo(teclado_texto());
    snprintf(aviso, sizeof aviso, "%s", i18n(r == 1 ? "Repositório adicionado" :
                                            r == -1 ? "já está na conta" :
                                            r == -2 ? "Lista cheia (24 repositórios)" :
                                            "o endereço precisa começar com http"));
    if (r == 1) { foco = 1 + plugins_n_repos(); registro_aviso_legal(); }
  }
  // A lista pode encolher por fora (sync da conta, outro perfil): foco valido.
  n = linhas();
  if (nivel == 1 && repo >= plugins_n_repos()) { nivel = 0; foco = 0; }
  if (foco >= n) foco = n > 0 ? n - 1 : 0;
}

void pluginsui_desenhar(Uint32 agora) {
  AjPluginsVista v;
  v.nivel = nivel; v.foco = foco; v.repo = repo; v.armado = armado; v.aviso = aviso;
  ajustes_desenhar_plugins(&v);
  if (teclado_aberto()) teclado_desenhar(agora);
}

#ifdef AJUSTES_TESTE
void pluginsui_teste(int n, int f, int r) { nivel = n; foco = f; repo = r; }
#endif
