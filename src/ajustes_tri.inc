// AJUSTES 2.1 — TRES ILHAS (mockup aprovado pelo dono em 27/10/2026; o pacote
// de decisao esta em docs/reviews/ajustes-fable/README.md).
//
// Uma geracao so, sobre o nucleo 2.0.3 que le o catalogo TELA[]:
//   ilha 1  as secoes do catalogo (SEC), nos tres blocos Assistir / Telas /
//           Conta e sistema, mais a rota "Controle parental";
//   ilha 2  grouped section rows and one-level topic submenus, using the same
//           focar / focavel / maisAberto / ajDesenharLinha catalog routes;
//   ilha 3  o inspetor (previa real, ajuda, Vale para / Padrao) ou o editor.
// D-pad: ↑↓ na ilha, → entra, ← sai, OK abre ou alterna, Voltar sobe um nivel.
// O valor so muda com OK (revisao de 02/10/2026, mantida).
//
// Este arquivo e o ESTADO e os EVENTOS; o desenho mora em ajustes_tri_desenho.inc.
#include "perfilregras.h"
#include "admin.h"
static void ajMovAtualizar(float dt);        // ajustes_ux_desenho.inc
static int aj2EditorPainel(void);            // ajustes_ux_ilha.inc
static void triUtilRolar(float topoLinha, float baseLinha, float topo, float base, float total);   // ajustes_tri_desenho.inc
static float triUtilBase(void);             // idem: o pe da lista das rotas (acima das pilulas)
static int triRota(void);                    // a rota (ilha + rota utilitaria), lida pelos testes
static void ajCenaTrocaAtualizar(float dt);  // ajustes_ux_secoes.inc

enum { TRI_CHIPS = 0, TRI_SECOES, TRI_LISTA };
enum { TRI_C_BUSCA = 0, TRI_C_PRESETS, TRI_C_DIF, TRI_C_TOUR, TRI_C_N };
enum { TRI_U_NADA = 0, TRI_U_DIF, TRI_U_PRESETS, TRI_U_REVISAO, TRI_U_PARENTAL };
// As linhas da rota "Controle parental" (ilha 2), na ordem da tela.
enum { TRI_P_INFANTIL = 0, TRI_P_TRAVAR, TRI_P_PIN, TRI_P_REMOVER_PIN, TRI_P_TEMPO, TRI_P_IDADE,
       TRI_P_BUSCA, TRI_P_COMPRAS, TRI_P_FILMES, TRI_P_PERFIL, TRI_P_N };

static int triIlha = TRI_SECOES;   // onde o foco esta
static int triChip;                // chip do cabecalho em foco (TRI_C_*)
static int triSecFoco;             // secao em foco na ilha 1 (nSecoes = Controle parental)
static int triUtil;                // rota utilitaria na ilha 2 (TRI_U_*)
static int triUtilVolta;           // a rota a que a revisao volta
static int triUtilLinhaVolta;      // e a linha dela (o preset escolhido)
static int triSecaoDeixada = -1;   // a secao de onde ← saiu: → volta a mesma linha; outra entra na 1.ª
static int triLinha, triAcao;      // linha e acao (pilulas) da rota utilitaria
static int triTrocarPerfil;        // "Trocar de perfil" pedido na folha do perfil
static float triChipF[TRI_C_N], triSecF[AJ_MAX_SECOES + 1], triUtilF[AJ_N];
static float triUtilRol, triUtilRolAlvo;   // rolagem das rotas utilitarias: o desenho acha o alvo, triAtualizar segue
static float triSecRolY, triSecRolYAlvo;   // ilha 1: rola ate a secao em foco quando nao cabe (texto grande)
static int ajPontUtil;                     // a rota utilitaria aceita o ponteiro (sem editor/modal, entrada feita)
// Movimento por ilha (nada de fade da tela inteira ao trocar de ilha):
static float triListaF;   // 1 com o foco na lista (a ilha 2 acende)
static float triEditorF;  // 1 com o editor aberto na ilha 3 (entra deslizando)
static float triUtilA;    // a rota utilitaria entra em 220 ms (como paginaA)
static int triUtilVisto = -1;
// A revisao em lote (presets, Diferente do padrao › Restaurar tudo): o que muda
// e o desfazer. So valores comuns; credenciais, acoes e leituras ficam fora.
static int rvN, rvOps[AJ_N], rvAntes[AJ_N], rvDepois[AJ_N];
static int rvUndoN, rvUndoOps[AJ_N], rvUndoAntes[AJ_N], rvUndoDepois[AJ_N], rvUndoPerfil;

int ajustes_pediu_trocar_perfil(void) { int v = triTrocarPerfil; triTrocarPerfil = 0; return v; }
static int triRTL(void) { return ajustes_idioma() == IDIOMA_AR; }

// --- SECOES ---------------------------------------------------------------------
// Os tres blocos do indice (ordem dos SEC de TELA; conferido contra AJS_* em
// ajustes_tri_desenho.inc): Assistir = 4 secoes, Telas = 4, Conta e sistema = 3.
static int triGrupo(int s) { return s < 4 ? 0 : s < 8 ? 1 : 2; }
static const char *triGrupoNome(int g) { return g == 0 ? "Assistir" : g == 1 ? "Telas" : "Conta e sistema"; }
static int triParental(int s) { return s == nSecoes; }
// Quantas opcoes a secao mostra e quantas estao atras de "Mais opcoes".
static void triContar(int sec, int *nOp, int *nAv) {
  int i, o = 0, a = 0;
  // As tecnicas contam mesmo com "Mais opcoes" fechado: visivel() as esconde,
  // mas elas existem (o cabecalho diz "45 opcoes · 36 tecnicas").
  for (i = secIni[sec] + 1; i < secFim(sec); i++)
    if (TELA[i].tipo == IT_OPC && ajOpcaoDisponivel(TELA[i].op) && (visivel(i) || uxAvancada(TELA[i].op))) {
      if (uxAvancada(TELA[i].op)) a++; else o++;
    }
  if (nOp) *nOp = o;
  if (nAv) *nAv = a;
}
static int triDisponivel(int op) {
  if (!ajOpcaoDisponivel(op) || !ajIptvLinhaVisivel(op)) return 0;
  if (op == AJ_FALHA_ADDON_LIMITE || (op == AJ_ICONE_APP && !apoiador_ativo())) return 0;
  if ((op == AJ_PERFIL_PESQ || op == AJ_PERFIL_EDITAR) && !recomenda_ativo()) return 0;
  if (op == AJ_BUSCA_CINEMETA && valor[op] == 0 && !cinemetaInstalado()) return 0;
  return 1;
}
static void triIniciar(void) {
  triSubmenu = -1;
  triIlha = TRI_SECOES; triChip = 0; triUtil = TRI_U_NADA; triUtilVolta = 0;
  triLinha = triAcao = 0; triTrocarPerfil = 0; triUtilRol = triUtilRolAlvo = 0;
  triSecFoco = secAtual; triSecaoDeixada = -1;
}
// Foco direto numa opcao (busca, atalhos, tour): a lista da secao dela.
static void triFocar(int op) {
  (void)op;
  triSubmenu = triDono(focoItem);
  triUtil = TRI_U_NADA; triIlha = TRI_LISTA; triSecFoco = secAtual; focoIndice = 0;
}
// O foco numa secao da ilha 1, sem entrar (D-pad, ponteiro e os testes).
static void triFocarSecao(int i) {
  if (i < 0 || i > nSecoes) return;
  triSubmenu = -1;
  triUtil = TRI_U_NADA; triIlha = TRI_SECOES; focoIndice = 1; triSecFoco = i;
  if (!triParental(i)) focarSecao(i);
}
static void triIrSecoes(void) {
  triSubmenu = -1;
  triIlha = TRI_SECOES; focoIndice = 1; triSecFoco = secAtual; triSecaoDeixada = secAtual; sairArmado = 0; uxRetornarOp = -1;
}
static void triEntrar(int s) {
  triSubmenu = -1;
  if (triParental(s)) { triUtil = TRI_U_PARENTAL; triLinha = triAcao = 0; triIlha = TRI_LISTA; focoIndice = 0; triUtilRol = triUtilRolAlvo = 0; return; }
  focarSecao(s);
  if (s != triSecaoDeixada) focar(primeiroDaSecao(s));   // secao nova: a 1.ª linha
  triSecaoDeixada = -1; focoIndice = 0; triIlha = TRI_LISTA;
}
static void triUtilAbrir(int u) {
  triSubmenu = -1;
  triUtil = u; triLinha = triAcao = 0; triIlha = TRI_LISTA; focoIndice = 0; triUtilRol = triUtilRolAlvo = 0;
  if (u == TRI_U_DIF) { uxListarDiferencas(); uxDifFoco = 0; uxIndice = 1; }
}
static void triUtilFechar(void) {
  int u = triUtil;
  triUtil = TRI_U_NADA; triLinha = triAcao = 0;
  if (u == TRI_U_DIF || uxIndice == 1) focarSecao(secAtual);   // uxIndice volta a secao
  if (u == TRI_U_PARENTAL) { triIlha = TRI_SECOES; focoIndice = 1; triSecFoco = nSecoes; return; }
  triIlha = TRI_CHIPS; focoIndice = 1;
  triChip = u == TRI_U_DIF ? TRI_C_DIF : TRI_C_PRESETS;
}

// --- REVISAO EM LOTE (presets e Restaurar tudo) ---------------------------------------
static int triMutavel(int op) {
  return uxTemPadrao(op) && escopoDe(op) != ESC_NENHUM &&
    !ajFonte(op) && !ajLegendaPrincipal(op) && !ajFalhaBackend(op) &&
    op != AJ_FIL_LIMITE && op != AJ_XTREAM_CONEXOES && op != AJ_DIARIO_NUVEM &&
    op != AJ_PROVEDORES_ESCOPO && op != AJ_IPTV_ATIVA && op != AJ_IPTV_CONTA &&
    op != AJ_PERFIL_INFANTIL && op != AJ_AVANCADAS;
}
static int rvPrepararReset(void) {
  rvN = 0;
  for (int i = 0; i < AJ_N; i++)
    if (triMutavel(i) && ajOpcaoDisponivel(i) && uxDiferente(i)) { rvOps[rvN] = i; rvAntes[rvN] = valor[i]; rvDepois[rvN++] = uxValorPadrao(i); }
  return rvN;
}
static int rvEstiloLegenda(int *ops, int *values, int n) {
  VideoLegendaEstilo style = *player_leg_estilo(); int changed = 0;
  for (int i = 0; i < n; i++) if (ajLegendaPrincipal(ops[i])) {
    changed = 1; int v = values[i];
    switch (ops[i]) {
      case AJ_LEG1_TAMANHO: style.tamanho = v; break;
      case AJ_LEG1_FONTE: style.familia = v; break;
      case AJ_LEG1_COR: style.cor = v; break;
      case AJ_LEG1_OPACIDADE: style.opacidade = v; break;
      case AJ_LEG1_FUNDO: style.fundo = v; break;
      case AJ_LEG1_POSICAO: style.posicao = v; break;
      case AJ_LEG1_BORDA: style.borda = v; break;
      case AJ_LEG1_ATRASO: style.atrasoMs = v; break;
      case AJ_LEG1_NEGRITO: style.negrito = !v; break;
      case AJ_LEG1_COR_BORDA: style.corBorda = v; break;
    }
  }
  return !changed || player_leg_estilo_definir(&style, -1);
}
static int rvAplicar(void) {
  if (pfTravado()) return 0;
  for (int i = 0; i < rvN; i++) if (valor[rvOps[i]] != rvAntes[i]) {
    uxNotificar("O valor mudou em outra tela. Confira antes de confirmar."); return 0;
  }
  for (int i = 0; i < rvN; i++) valor[rvOps[i]] = rvDepois[i];
  if (!gravar()) {
    for (int i = 0; i < rvN; i++) valor[rvOps[i]] = rvAntes[i];
    uxNotificar("Não foi possível salvar. O valor anterior foi mantido."); return 0;
  }
  if (!rvEstiloLegenda(rvOps, rvDepois, rvN)) {
    for (int i = 0; i < rvN; i++) valor[rvOps[i]] = rvAntes[i]; gravar();
    uxNotificar("Não foi possível salvar. O valor anterior foi mantido."); return 0;
  }
  rvUndoN = rvN; rvUndoPerfil = edPerfil ? edPerfil : perfis_ativo();
  memcpy(rvUndoOps, rvOps, rvN * sizeof(int));
  memcpy(rvUndoAntes, rvAntes, rvN * sizeof(int));
  memcpy(rvUndoDepois, rvDepois, rvN * sizeof(int));
  ajPenFeaturesApply(); player_leg_estilo_mudou();
  reaplicarEfeitosPerfil(); protegerLocais(); desc_repetir();
  uxNotificar("Ajuste salvo nesta TV."); return 1;
}
static int rvDesfazer(void) {
  if (!rvUndoN || rvUndoPerfil != (edPerfil ? edPerfil : perfis_ativo()) || pfTravado()) return 0;
  for (int i = 0; i < rvUndoN; i++) if (valor[rvUndoOps[i]] != rvUndoDepois[i]) return 0;
  for (int i = 0; i < rvUndoN; i++) valor[rvUndoOps[i]] = rvUndoAntes[i];
  if (!gravar()) {
    for (int i = 0; i < rvUndoN; i++) valor[rvUndoOps[i]] = rvUndoDepois[i];
    return 0;
  }
  if (!rvEstiloLegenda(rvUndoOps, rvUndoAntes, rvUndoN)) {
    for (int i = 0; i < rvUndoN; i++) valor[rvUndoOps[i]] = rvUndoDepois[i]; gravar(); return 0;
  }
  ajPenFeaturesApply(); player_leg_estilo_mudou();
  rvUndoN = 0; reaplicarEfeitosPerfil(); protegerLocais(); desc_repetir(); return 1;
}
#include "ajustes_presets.inc"
#include "ajustes_parental.inc"
// --- PONTEIRO NAS ROTAS UTILITARIAS -----------------------------------------------------
// Linha de preset/revisao/parental, pilula de acao e escolha parental: o clique
// foca (e o ponteiro entrega o OK em seguida, como em toda linha da tela).
static void triPontUtilLinha(int i, int b) {
  (void)b;
  if (!ajPontUtil || !triUtil || triUtil == TRI_U_DIF || i < 0) return;
  triIlha = TRI_LISTA; focoIndice = 0; triLinha = i; triAcao = 0;
}
static void triPontUtilAcao(int i, int b) {
  (void)b;
  if (!ajPontUtil || !triUtil) return;
  triIlha = TRI_LISTA; focoIndice = 0;
  if (triUtil == TRI_U_PRESETS) { ajPenSavedLoad(); triLinha = TRI_PRESET_N + ajPenSavedCount; triAcao = 0; }
  else triAcao = i;
}
static void triPontParentalValor(int v, int b) { (void)b; if (ajPenParentEditor < 0) return; ajPenParentDraft = v; }


// Topic navigation shares the existing list, controls and page animation.
static void triSubEntrar(int item) {
  if (!triTopico(item)) return;
  triSubmenu = item;
  for (int i = item + 1; i < secFim(secAtual) && TELA[i].tipo == IT_OPC; i++)
    if (focavel(i)) { focar(i); break; }
  scrollY = velY = 0; paginaA = 0;
}
static void triSubVoltar(void) {
  int item = triSubmenu;
  triSubmenu = -1; focar(item);
  scrollY = velY = 0; paginaA = 0;
}
// --- EVENTOS -------------------------------------------------------------------------
static int triNavLista(int dir) {
  int i;
  for (i = focoItem + dir; i > secIni[secAtual] && i < secFim(secAtual); i += dir)
    if (focavel(i)) { focar(i); return 1; }
  return 0;
}
static int triChipEvento(SDL_Keycode k) {
  if (uxVoltar(k)) { sair = 1; return 1; }
  if (k == SDLK_LEFT) { if (triChip > 0) triChip--; else sair = 1; return 1; }
  if (k == SDLK_RIGHT) { if (triChip + 1 < TRI_C_N) triChip++; return 1; }
  if (k == SDLK_DOWN) { triIlha = TRI_SECOES; triSecFoco = secAtual; return 1; }
  if (k == SDLK_UP) { pediuRelogio = 1; return 1; }
  if (uxOk(k)) {
    if (triChip == TRI_C_BUSCA) { uxVeioBusca = 0; uxPediuBusca = 1; }
    else if (triChip == TRI_C_PRESETS) triUtilAbrir(TRI_U_PRESETS);
    else if (triChip == TRI_C_DIF) triUtilAbrir(TRI_U_DIF);
    else tourAbrir();
    return 1;
  }
  return 1;
}
static int triSecoesEvento(SDL_Keycode k) {
  if (uxVoltar(k) || k == SDLK_LEFT) { sair = 1; return 1; }
  if (k == SDLK_UP) {
    if (triSecFoco > 0) { triSubmenu = -1; triSecFoco--; if (!triParental(triSecFoco)) focarSecao(triSecFoco); }
    else triIlha = TRI_CHIPS;
    return 1;
  }
  if (k == SDLK_DOWN) {
    if (triSecFoco < nSecoes) { triSubmenu = -1; triSecFoco++; if (!triParental(triSecFoco)) focarSecao(triSecFoco); }
    return 1;
  }
  if (uxOk(k) || (!ajRetrato() && k == SDLK_RIGHT)) { triEntrar(triSecFoco); return 1; }
  return 1;
}
static int triListaEvento(SDL_Keycode k) {
  if (k == SDLK_UP || k == SDLK_DOWN || k == SDLK_LEFT || k == SDLK_PAGEUP || k == SDLK_PAGEDOWN) uxRetornarOp = -1;
  if (uxVoltar(k) || k == SDLK_LEFT) {
    if (triSubmenu >= 0) triSubVoltar(); else triIrSecoes();
    return 1;
  }
  if ((uxOk(k) || k == SDLK_RIGHT) && triTopico(focoItem)) { triSubEntrar(focoItem); return 1; }
  if (k == SDLK_UP || k == SDLK_DOWN) {
    if (!triNavLista(k == SDLK_DOWN ? 1 : -1) && k == SDLK_UP) {
      if (triSubmenu >= 0) { triSubVoltar(); return 1; }
      if (ajRetrato()) triIrSecoes();   // a faixa de secoes fica entre a lista e os chips
      else { triIlha = TRI_CHIPS; focoIndice = 1; triSecFoco = secAtual; }
    }
    return 1;
  }
  if (k == SDLK_RIGHT) return 1;
  if (k == SDLK_PAGEUP || k == SDLK_PAGEDOWN) return 0;   // eventoTela troca a secao
  if (!uxOk(k)) return 1;
  if (focoOp == AJ_PERFIL_ATIVO) { pfAbrir(); return 1; }
  if (focoOp >= 0 && uxAlternarDireto(focoOp)) return 1;
  // Linha apagada (2.0.3): OK liga o que falta, como a propria linha diz; quando
  // o requisito nao e um interruptor, leva ate ele e Voltar devolve aqui.
  if (focoOp >= 0 && inativa(focoOp)) {
    int req = uxRequisito(focoOp);
    if (uxLigarRequisitos(focoOp, 1)) return 1;
    if (req >= 0) { int op = focoOp; uxCancelar(); focarOpcao(req); uxRetornarOp = op; return 1; }
    uxAbrirEditor(focoOp); return 1;
  }
  return 0;   // eventoTela: "Mais opcoes", acoes, teclados e os editores de escolha/numero
}
// Diferente do padrao: a lista (uxDifs) e as pilulas Restaurar tudo / Desfazer.
static int triDifEvento(SDL_Keycode k) {
  int acoes = 1 + (rvUndoN > 0);
  if (uxVoltar(k) || (k == SDLK_LEFT && !triAcao)) { triUtilFechar(); return 1; }
  if (triAcao) {
    if (k == SDLK_UP) { triAcao = 0; return 1; }
    if (k == SDLK_LEFT && triAcao > 1) triAcao--;
    if (k == SDLK_RIGHT && triAcao < acoes) triAcao++;
    if (uxOk(k)) {
      if (triAcao == 1) { if (rvPrepararReset()) { triUtilVolta = TRI_U_DIF; triUtilLinhaVolta = 0; triUtil = TRI_U_REVISAO; triLinha = triAcao = 0; } else uxNotificar("Tudo já está no padrão."); }
      else { if (rvDesfazer()) { uxListarDiferencas(); uxNotificar("Desfeito."); } triAcao = 0; }
    }
    return 1;
  }
  if (k == SDLK_UP) { if (uxDifFoco > 0) uxDifFoco--; else { triIlha = TRI_CHIPS; focoIndice = 1; triUtil = TRI_U_NADA; focarSecao(secAtual); } return 1; }
  if (k == SDLK_DOWN) { if (uxDifFoco + 1 < uxNDifs) uxDifFoco++; else triAcao = 1; return 1; }
  if (uxOk(k) && uxNDifs && !uxAlternarDireto(uxDifs[uxDifFoco])) uxAbrirEditor(uxDifs[uxDifFoco]);
  return 1;
}
static int triPresetsEvento(SDL_Keycode k) {
  int n;
  ajPenSavedLoad();
  n = TRI_PRESET_N + ajPenSavedCount + 1;   // prontos, salvos, "Salvar o estado atual"
  if (uxVoltar(k)) { if (triAcao) triAcao = 0; else triUtilFechar(); return 1; }
  if (k == SDLK_UP) { if (triLinha > 0) triLinha--; else { triUtilFechar(); } triAcao = 0; return 1; }
  if (k == SDLK_DOWN) { if (triLinha + 1 < n) triLinha++; triAcao = 0; return 1; }
  if (k == SDLK_LEFT) { triAcao = 0; return 1; }
  if (k == SDLK_RIGHT) { if (triLinha >= TRI_PRESET_N && triLinha < TRI_PRESET_N + ajPenSavedCount) triAcao = 1; return 1; }
  if (!uxOk(k)) return 1;
  triUtilLinhaVolta = triLinha;
  if (triLinha < TRI_PRESET_N) { if (triPresetPreparar(triLinha)) { triUtilVolta = TRI_U_PRESETS; triUtil = TRI_U_REVISAO; triLinha = triAcao = 0; } else uxNotificar("Este preset já está aplicado."); }
  else if (triLinha == TRI_PRESET_N + ajPenSavedCount) ajPenSavedAsk();
  else if (triAcao) { if (!ajPenSavedDelete(triLinha - TRI_PRESET_N)) uxNotificar("Não foi possível salvar. O valor anterior foi mantido."); triAcao = 0; if (triLinha > 0) triLinha--; }
  else if (ajPenSavedPrepare(triLinha - TRI_PRESET_N)) { triUtilVolta = TRI_U_PRESETS; triUtil = TRI_U_REVISAO; triLinha = triAcao = 0; }
  else uxNotificar("Este preset já está aplicado.");
  return 1;
}
static void triRevisaoFechar(int aplicou) {
  int volta = triUtilVolta;
  triUtil = volta ? volta : TRI_U_NADA; triLinha = triUtilLinhaVolta; triAcao = 0; triUtilVolta = 0;
  if (triUtil == TRI_U_DIF) { uxListarDiferencas(); uxDifFoco = 0; }
  if (!triUtil) { triIlha = TRI_CHIPS; focoIndice = 1; }
  (void)aplicou;
}
static int triRevisaoEvento(SDL_Keycode k) {
  if (uxVoltar(k)) { triRevisaoFechar(0); return 1; }
  if (k == SDLK_DOWN) { if (!triAcao) { if (triLinha + 1 < rvN) triLinha++; else triAcao = 1; } return 1; }
  if (k == SDLK_UP) { if (triAcao) triAcao = 0; else if (triLinha) triLinha--; return 1; }
  if (k == SDLK_LEFT) { triAcao = 1; return 1; }
  if (k == SDLK_RIGHT) { triAcao = 2; return 1; }
  // So "Aplicar mudancas" grava. OK (ou toque) numa linha da previa so leva o
  // foco a Aplicar: quem confirma o lote e o segundo OK, ali.
  if (uxOk(k)) {
    if (triAcao == 2) triRevisaoFechar(0);
    else if (triAcao != 1) triAcao = 1;
    else if (rvAplicar()) triRevisaoFechar(1);
  }
  return 1;
}
static int triParentalEvento(SDL_Keycode k) {
  int alvo = ajPenParentProfile();
  if (ajPenParentEvent(k)) return 1;
  if (uxVoltar(k) || k == SDLK_LEFT) { triUtilFechar(); return 1; }
  if (k == SDLK_UP) { if (triLinha > 0) triLinha--; else { triUtilFechar(); } return 1; }
  if (k == SDLK_DOWN) { if (triLinha + 1 < TRI_P_N) triLinha++; return 1; }
  if (!uxOk(k)) return 1;
  switch (triLinha) {
    case TRI_P_INFANTIL: if (!uxAlternarDireto(AJ_PERFIL_INFANTIL)) uxAbrirEditor(AJ_PERFIL_INFANTIL); break;
    case TRI_P_TRAVAR: if (!perfil_infantil_ativo()) pfExigirAdmin(PA_TRAVAR, alvo); break;
    case TRI_P_PIN:
      if (perfil_infantil_ativo() || pfTravado()) break;
      ajPenPinAsk = 1; ajPenPinRepeat = admin_perfil_pin_definido(perfis_ativo()) ? -1 : 0; ajPenPinOpen(); break;
    case TRI_P_REMOVER_PIN: pfExigirAdmin(PA_REMOVER_PIN, perfis_ativo()); break;
    case TRI_P_TEMPO: ajPenParentOpen(PR_TEMPO); break;
    case TRI_P_IDADE: ajPenParentOpen(PR_IDADE); break;
    case TRI_P_BUSCA: case TRI_P_COMPRAS: {
      int regra = triLinha == TRI_P_BUSCA ? PR_BUSCA_LIVRE : PR_COMPRAS;
      if (perfil_infantil_ativo() || pfTravado()) uxNotificar("Indisponível em perfil infantil.");
      else if (perfil_regra_definir(alvo, regra, !perfil_regra(alvo, regra))) uxNotificar("Ajuste salvo nesta TV.");
      else uxNotificar("Não foi possível salvar. O valor anterior foi mantido.");
      break; }
    case TRI_P_FILMES: ajPenParentOpen(PR_FILMES); break;
    case TRI_P_PERFIL: if (!perfil_infantil_ativo()) { pfTela = PF_LIMITES; pfFoco = 0; } break;
  }
  return 1;
}
static int triUtilEvento(SDL_Keycode k) {
  switch (triUtil) {
    case TRI_U_DIF: return triDifEvento(k);
    case TRI_U_PRESETS: return triPresetsEvento(k);
    case TRI_U_REVISAO: return triRevisaoEvento(k);
    case TRI_U_PARENTAL: return triParentalEvento(k);
    default: return 0;
  }
}
// As setas desta tela sao SEMANTICAS (↑↓ na ilha, → entra, ← sai): nao passam
// pelo vizinho geometrico (navigationMove), que serve as folhas e o tour.
static int triSetaSemantica(SDL_Keycode k) {
  if (k != SDLK_UP && k != SDLK_DOWN && k != SDLK_LEFT && k != SDLK_RIGHT) return 0;
  if (uxEditor || tourAberto || frAberta || filAberta || pfTela || hcAberta || riscoFolha || apoioAberto || guiaAberto) return 0;
  if (teclado_aberto() || iptvForm.tipo || ajPenPinAsk) return 0;
  return 1;   // inclusive a escolha parental (ajPenParentEvent): ↑↓ trocam o valor
}
// Chamado por uxEvento quando nao ha editor aberto. 1 = tratou; 0 = eventoTela
// segue (acoes, teclados, "Mais opcoes", editor de escolha/numero).
static int triEvento(SDL_Keycode k) {
  if (triUtil) return triUtilEvento(k);
  if (uxVeioBusca && uxVoltar(k)) { uxPediuBusca = 2; uxVeioBusca = 0; return 1; }
  if (uxVoltar(k) && uxRetornarOp >= 0) { int op = uxRetornarOp; uxRetornarOp = -1; focarOpcao(op); return 1; }
  if (triIlha == TRI_CHIPS) return triChipEvento(k);
  if (triIlha == TRI_SECOES) return triSecoesEvento(k);
  return triListaEvento(k);
}
// O foco e a rota, para o screenReveal e para os testes de navegacao.
static int __attribute__((unused)) triRota(void) { return triIlha * 10 + triUtil * 100; }   // lido pelos testes (nav21)
static void triAtualizar(float dt) {
  int i, red = ajustes_animacoes_reduzidas();
  float r = red ? 1.0f : 0.0f;
  (void)r;
  for (i = 0; i < TRI_C_N; i++) {
    float alvo = triIlha == TRI_CHIPS && !triUtil && i == triChip ? 1.0f : 0.0f;
    triChipF[i] = red ? alvo : anim_mola(triChipF[i], alvo, dt, alvo > triChipF[i] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  }
  for (i = 0; i <= nSecoes && i < AJ_MAX_SECOES + 1; i++) {
    float alvo = triIlha == TRI_SECOES && i == triSecFoco ? 1.0f : 0.0f;
    triSecF[i] = red ? alvo : anim_mola(triSecF[i], alvo, dt, alvo > triSecF[i] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  }
  for (i = 0; i < AJ_N; i++) {
    int foco = triUtil == TRI_U_DIF ? (i == uxDifFoco && !triAcao) : triUtil ? (i == triLinha && !(triUtil == TRI_U_REVISAO && triAcao)) : 0;
    float alvo = triUtil && triIlha == TRI_LISTA && foco ? 1.0f : 0.0f;
    if (triUtilF[i] == alvo) continue;
    triUtilF[i] = red ? alvo : anim_mola(triUtilF[i], alvo, dt, alvo > triUtilF[i] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  }
  { float alvo = triIlha == TRI_LISTA && !triUtil ? 1.0f : 0.0f;
    triListaF = red ? alvo : anim_mola(triListaF, alvo, dt, alvo > triListaF ? NV_MOLA_FOCO : NV_MOLA_DESFOCO); }
  { float alvo = uxEditor == 1 && uxOp >= 0 && aj2EditorPainel() ? 1.0f : 0.0f;
    triEditorF = red ? alvo : anim_mola(triEditorF, alvo, dt, alvo > triEditorF ? NV_MOLA_FOCO : NV_MOLA_DESFOCO); }
  if (!triUtil) triUtilRolAlvo = 0.0f;
  // No celular o dedo manda enquanto a regiao esta "livre" (ajToqueRolar); a
  // mola volta a seguir o foco na primeira tecla (ajToqueLimpar).
#ifdef NV_TOUCH_UI
  if (!(ajTelaCheia() && ajToqueLivre[AJT_UTIL]))
#endif
  triUtilRol = red ? triUtilRolAlvo : anim_mola(triUtilRol, triUtilRolAlvo, dt, NV_MOLA_FOCO);
#ifdef NV_TOUCH_UI
  if (!(ajTelaCheia() && ajToqueLivre[AJT_SECOES]))
#endif
  triSecRolY = red ? triSecRolYAlvo : anim_mola(triSecRolY, triSecRolYAlvo, dt, NV_MOLA_FOCO);
  if (triUtil != triUtilVisto) { triUtilVisto = triUtil; triUtilA = 0.0f; }
  triUtilA = red ? 1.0f : anim_rampa(triUtilA, 1.0f, dt, 220.0f);
  ajMovAtualizar(dt);
  ajCenaTrocaAtualizar(dt);
}
#ifdef AJUSTES_TESTE
// O andamento do movimento da tela, para o gate de navegacao (nav21): a
// entrada da secao (paginaA), o foco na lista, o editor e a rota utilitaria.
float ajustes_teste_movimento(void) {
  return 0.25f * paginaA + 0.25f * triListaF + 0.25f * triEditorF + 0.25f * triUtilA;
}
#endif
