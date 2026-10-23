// Approved v4 presentation. TELA, enum, CHAVE and positional defaults remain intact.
#define AJUSTES_V4 1
static int aj4Atual, aj4Grupo, aj4Grupos = 1, aj4GrupoFoco;
static int aj4Topo = -1, aj4Set, aj4Linha, aj4Util, aj4Escopo, aj4Review, aj4Acao;
static int aj4Opcoes[AJ_N], aj4Ops[AJ_N], aj4Antes[AJ_N], aj4Depois[AJ_N], aj4N;
static int aj4UndoOps[AJ_N], aj4UndoAntes[AJ_N], aj4UndoDepois[AJ_N], aj4UndoN, aj4UndoPerfil;
static int aj4FocoModo[2], aj4Excecao = -1;
static char aj4Consulta[128];
static int aj4BuscaTeclado, aj4BuscaRetorno, aj4BuscaFoco;
static AjusteBuscaResultado aj4Resultados[AJ_N];
static void aj4Iniciar(void) {
  aj4Topo = -1; aj4Util = aj4Review = aj4Acao = aj4Linha = 0; aj4Excecao = -1;
  aj4BuscaTeclado=aj4BuscaRetorno=0;
}
static const char *const AJ4_SECOES[] = {
  "Reprodução", "Áudio e legendas", "Fontes e serviços", "TV ao vivo",
  "Início e biblioteca", "Aparência", "Perfil e conta", "Esta TV e ajuda"
};
static const char *const AJ4_TAREFAS[] = { "Reprodução", "Áudio e legendas", "Minha tela", "Perfil e serviços" };
static const char *const AJ4_IC[] = {
  "aj_circle-play", "aj_languages", "aj_puzzle", "aj_tv-minimal-play",
  "aj_layout-dashboard", "aj_palette", "aj_user-round", "aj_monitor-cog"
};
static const char *const AJ4_SUB[] = {
  "Qualidade, escolha do vídeo e próximo episódio", "Seus idiomas e quando as legendas aparecem",
  "Addons, debrid e servidores pessoais", "Provedor e diagnóstico dos canais",
  "Fileiras, Continuar assistindo e informações dos títulos", "Material, texto, cartazes e movimento",
  "Perfis, contas conectadas e sincronização", "Desempenho, compatibilidade e ajuda"
};
#include "ajustes_ux_v4_mapa.inc"
static int aj4Simples(int op) {
  switch (op) {
    case AJ_QUALIDADE: case AJ_FONTE_MANUAL: case AJ_PROXIMO_AUTO: case AJ_SEEKR_LIGADO: return 1;
    case AJ_AUD_LINGUA: case AJ_LEG_LINGUA: case AJ_LEG_FORCADA: case AJ_LEG_MOSTRAR: return 2;
    case AJ_HOME_LAYOUT: case AJ_ESCALA_TEXTO: case AJ_ANIM: case AJ_DET_BLUR_NAO_VISTOS: return 3;
    case AJ_PERFIL_ATIVO: case AJ_ADDONS: case AJ_TRAKT: case AJ_SYNC: return 4;
    default: return 0;
  }
}
static int aj4Secao(int op) {
  int s = aj4MapaSecao(op);
  if (s >= 0) return s;
  montarTela();
  // Newly added options inherit their compiled section until explicitly grouped.
  static const int antigas[] = {0, 1, 2, 3, 4, 5, 4, 5, 6, 7, 7};
  for (int i = 0; i < AJ_N_TELA; i++) if (TELA[i].tipo == IT_OPC && TELA[i].op == op)
    return antigas[secDoItem[i]];
  return -1;
}
static int aj4GrupoDe(int op) {
  int g = aj4MapaGrupo(op);
  return g >= 0 ? g : AJ4_GRUPO_BASE[aj4Secao(op)];
}
static int aj4ListaBruta(int sec, int grupo, int *ops) {
  unsigned char visto[AJ_N] = {0};
  int n = 0;
  for (int i = 0; i < AJ_N_TELA; i++) if (TELA[i].tipo == IT_OPC) {
    int op = TELA[i].op;
    if (!visto[op] && aj4Secao(op) == sec && (grupo < 0 || aj4GrupoDe(op) == grupo)) {
      visto[op] = 1; ops[n++] = op;
    }
  }
  return n;
}
static int aj4Disponivel(int op) {
  if (!ajOpcaoDisponivel(op) || !ajIptvLinhaVisivel(op)) return 0;
  if (op == AJ_FALHA_ADDON_LIMITE || (op == AJ_ICONE_APP && !apoiador_ativo())) return 0;
  if ((op == AJ_PERFIL_PESQ || op == AJ_PERFIL_EDITAR) && !recomenda_ativo()) return 0;
  if (op == AJ_BUSCA_CINEMETA && valor[op] == 0 && !cinemetaInstalado()) return 0;
  return 1;
}
static int aj4Lista(int sec, int grupo, int *ops) {
  int n = 0, raw[AJ_N], count;
  if (!lig(AJ_AVANCADAS) && aj4Excecao < 0) {
    static const int simples[4][4] = {
      {AJ_QUALIDADE, AJ_FONTE_MANUAL, AJ_PROXIMO_AUTO, AJ_SEEKR_LIGADO},
      {AJ_AUD_LINGUA, AJ_LEG_LINGUA, AJ_LEG_FORCADA, AJ_LEG_MOSTRAR},
      {AJ_HOME_LAYOUT, AJ_ESCALA_TEXTO, AJ_ANIM, AJ_DET_BLUR_NAO_VISTOS},
      {AJ_PERFIL_ATIVO, AJ_ADDONS, AJ_TRAKT, AJ_SYNC}
    };
    if (sec < 0 || sec >= 4) return 0;
    for (int i = 0; i < 4; i++) {
      int op = simples[sec][i];
      if (ajOpcaoDisponivel(op) && (grupo < 0 || aj4GrupoDe(op) == grupo)) ops[n++] = op;
    }
    return n;
  }
  count = aj4ListaBruta(sec, grupo, raw);
  for (int i = 0; i < count; i++) if (aj4Disponivel(raw[i])) ops[n++] = raw[i];
  return n;
}
static int aj4GruposLista(int sec, int *gs) {
  int n = 0, ops[AJ_N], count = aj4Lista(sec, -1, ops);
  for (int i = AJ4_GRUPO_BASE[0]; i < AJ4_N_GRUPOS; i++) {
    for (int j = 0; j < count; j++) if (aj4GrupoDe(ops[j]) == i) { gs[n++] = i; break; }
  }
  return n;
}
static const char *aj4Rotulo(int op) {
  switch (op) {
    case AJ_QUALIDADE: return "Qualidade do vídeo";
    case AJ_FONTE_MANUAL: return "Quem escolhe o vídeo?";
    case AJ_PROXIMO_AUTO: return "Reproduzir o próximo episódio";
    case AJ_SEEKR_LIGADO: return "Miniaturas ao avançar";
    case AJ_AUD_LINGUA: return "Áudio preferido";
    case AJ_LEG_LINGUA: return "Legenda preferida";
    case AJ_LEG_FORCADA: return "Traduzir só falas estrangeiras";
    case AJ_LEG_MOSTRAR: return "Quando mostrar legendas";
    case AJ_ESCALA_TEXTO: return "Tamanho do texto";
    case AJ_ANIM: return "Movimento dos menus";
    case AJ_DET_BLUR_NAO_VISTOS: return "Esconder spoilers dos episódios";
    case AJ_ADDONS: return "Conectar um addon";
    case AJ_TRAKT: return "Contas conectadas";
    case AJ_SYNC: return "Estado da sincronização";
    default: return rotuloOpcao(op);
  }
}
static const char *aj4Ajuda(int op) {
  switch (op) {
    case AJ_QUALIDADE: return "Escolha a maior resolução que o Nuvio pode selecionar.";
    case AJ_FONTE_MANUAL: return "O Nuvio escolhe automaticamente ou mostra a lista de fontes.";
    case AJ_PROXIMO_AUTO: return "Continue a série sem escolher o próximo episódio.";
    case AJ_SEEKR_LIGADO: return "Veja uma miniatura da cena ao avançar no vídeo.";
    case AJ_AUD_LINGUA: return "Seu primeiro idioma ao começar um vídeo.";
    case AJ_LEG_LINGUA: return "Seu idioma preferido para ler as legendas.";
    case AJ_LEG_FORCADA: return "Mostre só a tradução de falas em outro idioma.";
    case AJ_LEG_MOSTRAR: return "Escolha quais idiomas aparecem na lista de legendas.";
    case AJ_ESCALA_TEXTO: return "Aumente textos e controles sem mudar seus cartazes.";
    case AJ_ANIM: return "Reduza o movimento para navegar com mais calma.";
    case AJ_DET_BLUR_NAO_VISTOS: return "Oculte imagens de episódios que você ainda não viu.";
    case AJ_TRAKT: return "Trakt, Simkl e outras contas ficam em Perfil e conta.";
    default: return ajudaOpcao(op);
  }
}
static void aj4Focar(int op) {
  aj4Excecao = op;
  aj4Atual = aj4Secao(op); aj4Grupo = aj4GrupoDe(op); aj4Grupos = 0;
  aj4Topo = -1;
  int n = aj4Lista(aj4Atual, aj4Grupo, aj4Opcoes);
  for (int i = 0; i < n; i++) if (aj4Opcoes[i] == op) aj4Linha = i;
}
static void aj4Entrar(int sec) {
  int gs[AJ4_N_GRUPOS];
  aj4Atual = sec; aj4Excecao = -1; aj4Grupos = lig(AJ_AVANCADAS);
  aj4GrupoFoco = 0; aj4Linha = 0; aj4Topo = -1; aj4Util = 0;
  int n = aj4GruposLista(sec, gs);
  aj4Grupo = n ? gs[0] : AJ4_GRUPO_BASE[sec];
  n = aj4Lista(sec, aj4Grupos ? aj4Grupo : -1, aj4Opcoes);
  if (n) { focarOpcao(aj4Opcoes[0]); aj4Excecao = -1; aj4Atual = sec; aj4Grupos = lig(AJ_AVANCADAS); }
  focoIndice = 0;
}
// Bulk review only owns ordinary stored values. Credentials, actions and external
// subsystem state remain with their existing editors and profile controls.
static int aj4Mutavel(int op) {
  return uxTemPadrao(op) && escopoDe(op) != ESC_NENHUM &&
    !ajFonte(op) && !ajLegendaPrincipal(op) && !ajFalhaBackend(op) &&
    op != AJ_FIL_LIMITE && op != AJ_XTREAM_CONEXOES && op != AJ_DIARIO_NUVEM &&
    op != AJ_PROVEDORES_ESCOPO && op != AJ_IPTV_ATIVA && op != AJ_IPTV_CONTA &&
    op != AJ_PERFIL_INFANTIL && op != AJ_AVANCADAS;
}
static int aj4PrepararReset(int sec, int tv) {
  aj4N = 0;
  for (int i = 0; i < AJ_N; i++) if (aj4Secao(i) >= 0 && (sec < 0 || aj4Secao(i) == sec) &&
      (sec>=-1 || aj4Simples(i)==-sec-1) &&
      aj4Mutavel(i) && (escopoDe(i) == ESC_TV) == tv && uxDiferente(i)) {
    aj4Ops[aj4N] = i; aj4Antes[aj4N] = valor[i]; aj4Depois[aj4N++] = uxValorPadrao(i);
  }
  aj4Review = 1; aj4Linha = 0; aj4Acao = 0; return aj4N;
}
static int aj4PrepararPreset(int preset) {
  static const int ops[3][7] = {
    {AJ_QUALIDADE, AJ_FONTE_PRIORIDADE, -1},
    {AJ_QUALIDADE, AJ_TAM_MAX, AJ_TRAILER_QUAL, AJ_HERO_TRAILER, AJ_DET_TRAILER_AUTO, -1},
    {AJ_QUALIDADE_IMG, AJ_ANIM, AJ_VIDRO, AJ_GPU_EFEITOS, AJ_HERO_TRAILER, AJ_DET_TRAILER_AUTO, AJ_TEX_MB}
  };
  static const int vals[3][7] = {{1,1}, {3,2,3,1,1}, {0,1,1,2,1,1,0}};
  aj4N = 0;
  if (preset < 0 || preset > 2) return 0;
  for (int i = 0; i < 7 && ops[preset][i] >= 0; i++) {
    int op = ops[preset][i];
    if (ajOpcaoDisponivel(op) && valor[op] != vals[preset][i]) {
      aj4Ops[aj4N] = op; aj4Antes[aj4N] = valor[op]; aj4Depois[aj4N++] = vals[preset][i];
    }
  }
  aj4Review = 2; aj4Linha = 0; aj4Acao = 0; return aj4N;
}
static int aj4Aplicar(void) {
  if (pfTravado()) return 0;
  for (int i = 0; i < aj4N; i++) if (valor[aj4Ops[i]] != aj4Antes[i]) {
    uxNotificar("O valor mudou em outra tela. Confira antes de confirmar."); return 0;
  }
  for (int i = 0; i < aj4N; i++) valor[aj4Ops[i]] = aj4Depois[i];
  if (!gravar()) {
    for (int i = 0; i < aj4N; i++) valor[aj4Ops[i]] = aj4Antes[i];
    uxNotificar("Não foi possível salvar. O valor anterior foi mantido."); return 0;
  }
  aj4UndoN = aj4N; aj4UndoPerfil = edPerfil ? edPerfil : perfis_ativo();
  memcpy(aj4UndoOps, aj4Ops, aj4N * sizeof(int));
  memcpy(aj4UndoAntes, aj4Antes, aj4N * sizeof(int));
  memcpy(aj4UndoDepois, aj4Depois, aj4N * sizeof(int));
  reaplicarEfeitosPerfil(); protegerLocais(); desc_repetir();
  aj4Review = 0; uxNotificar("Ajuste salvo nesta TV."); return 1;
}
static int aj4Desfazer(void) {
  if (!aj4UndoN || aj4UndoPerfil != (edPerfil ? edPerfil : perfis_ativo()) || pfTravado()) return 0;
  for (int i = 0; i < aj4UndoN; i++) if (valor[aj4UndoOps[i]] != aj4UndoDepois[i]) return 0;
  for (int i = 0; i < aj4UndoN; i++) valor[aj4UndoOps[i]] = aj4UndoAntes[i];
  if (!gravar()) {
    for (int i = 0; i < aj4UndoN; i++) valor[aj4UndoOps[i]] = aj4UndoDepois[i];
    return 0;
  }
  aj4UndoN = 0; reaplicarEfeitosPerfil(); protegerLocais(); desc_repetir(); return 1;
}
static int aj4Diferencas(int *ops) {
  int n = 0;
  for (int i = 0; i < AJ_N; i++) if (aj4Secao(i) >= 0 && aj4Mutavel(i) && uxDiferente(i) &&
      (escopoDe(i) == ESC_TV) == aj4Escopo) ops[n++] = i;
  return n;
}
static void aj4Buscar(void) {
  aj4Util=3; aj4Linha=0; aj4Acao=0; focoIndice=0; aj4Topo=-1;
  aj4BuscaTeclado=1;
  teclado_abrir_com("Buscar ajuste","Pelo nome ou pelo que quer mudar",127,CUR_ALFA_URL,aj4Consulta);
}
static void ajPontEdValor(int i,int b);
static void aj4PontFoco(int zona,int row) {
  if(zona==0)aj4Topo=row;
  else if(zona==1) { aj4Topo=-1; aj4Linha=row; aj4Acao=0; }
  else if(zona==2) { aj4Topo=-1; aj4GrupoFoco=row; aj4Grupos=1; }
  else if(zona==3) { aj4Topo=-1; aj4Acao=row; }
  else if(zona==4 && uxEditor==1)ajPontEdValor(row,0);
  if(zona==5) { focoIndice=1; aj4Linha=aj4FocoModo[lig(AJ_AVANCADAS)]; }
}
static int aj4Evento(SDL_Keycode k) {
  if (uxEditor) return -1; // Existing draft editor owns its keys and guards.
  if (aj4BuscaRetorno && uxVoltar(k)) {
    aj4BuscaRetorno=0; aj4Util=3; aj4Linha=aj4BuscaFoco; aj4Topo=-1; focoIndice=0; return 1;
  }
  if (uxVeioBusca && uxVoltar(k)) { uxPediuBusca = 2; uxVeioBusca = 0; return 1; }
  if (uxVoltar(k) && uxRetornarOp>=0) { int op=uxRetornarOp; uxRetornarOp=-1; focarOpcao(op); return 1; }
  if (aj4Review) {
    if (uxVoltar(k)) { aj4Review = 0; aj4Linha = 0; aj4Acao = 0; return 1; }
    if (k == SDLK_DOWN && !aj4Acao) { if (aj4Linha + 1 < aj4N) aj4Linha++; else aj4Acao = 1; }
    if (k == SDLK_UP) { if (aj4Acao) aj4Acao = 0; else if (aj4Linha) aj4Linha--; }
    if (k == SDLK_LEFT) aj4Acao = 2;
    if (k == SDLK_RIGHT) aj4Acao = 1;
    if (uxOk(k) && aj4Acao == 1) aj4Aplicar();
    else if (uxOk(k) && aj4Acao == 2) { aj4Review = 0; aj4Linha = 0; aj4Acao = 0; }
    return 1;
  }
  if (aj4Topo >= 0) {
    if (uxVoltar(k)) { if (focoIndice) sair = 1; else { focoIndice = 1; aj4Util = 0; } aj4Topo = -1; return 1; }
    if (k == SDLK_LEFT && aj4Topo) aj4Topo--;
    if (k == SDLK_RIGHT && aj4Topo < 2) aj4Topo++;
    if (k == SDLK_DOWN) aj4Topo = -1;
    if (uxOk(k)) {
      if (aj4Topo == 0) { uxVeioBusca = 0; aj4Buscar(); }
      if (aj4Topo == 1) {
        int old = lig(AJ_AVANCADAS);
        aj4FocoModo[old] = aj4Linha;
        if (definirValorDireto(AJ_AVANCADAS, old ? 1 : 0)) {
          aj4Linha = aj4FocoModo[!old]; aj4Excecao = -1; aj4Set = 0;
          focoIndice = 1; aj4Util = 0; uxNotificar("Suas escolhas continuam salvas.");
        }
      }
      if (aj4Topo == 2) pfAbrir();
    }
    return 1;
  }
  if (aj4Util) {
    if (uxVoltar(k)) { aj4Util = 0; focoIndice = 1; aj4Linha = 0; aj4Acao = 0; return 1; }
    int n = aj4Util == 1 ? aj4Diferencas(aj4Opcoes) : aj4Util==3 ? ajustes_buscar(aj4Consulta,aj4Resultados,AJ_N) : 3;
    if (aj4Util == 1 && (k == SDLK_LEFT || k == SDLK_RIGHT)) {
      aj4Escopo = k == SDLK_RIGHT; aj4Linha = 0; aj4Acao = 0; return 1;
    }
    if (k == SDLK_DOWN) { if (aj4Linha + 1 < n && !aj4Acao) aj4Linha++; else aj4Acao = 1; }
    if (k == SDLK_UP) { if (aj4Acao) aj4Acao = 0; else if (aj4Linha) aj4Linha--; else aj4Topo = 0; }
    if (uxOk(k)) {
      if(aj4Util==3) {
        if(aj4Acao || !n)aj4Buscar();
        else { int row=aj4Linha; aj4Util=0; focarOpcao(aj4Resultados[row].op); aj4BuscaRetorno=1; aj4BuscaFoco=row; }
        return 1;
      }
      if (aj4Acao && aj4UndoN) { aj4Desfazer(); aj4Acao = 0; }
      else if (aj4Acao && aj4Util == 1) aj4PrepararReset(-1, aj4Escopo);
      else if (aj4Util == 2) aj4PrepararPreset(aj4Linha);
      else if (n) { int op = aj4Opcoes[aj4Linha]; focarOpcao(op); uxAbrirEditor(op); }
    }
    return 1;
  }
  if (focoIndice) {
    if (uxVoltar(k)) { sair = 1; return 1; }
    if (k == SDLK_UP) { if (aj4Linha) aj4Linha--; else aj4Topo = 0; }
    if (k == SDLK_DOWN && aj4Linha < (aj4UndoN?7:6)) aj4Linha++;
    if (k == SDLK_LEFT && lig(AJ_AVANCADAS)) { aj4Set = 0; aj4Linha = 0; }
    if (k == SDLK_RIGHT && lig(AJ_AVANCADAS)) { aj4Set = 1; aj4Linha = 0; }
    if (uxOk(k)) {
      if (aj4Linha < 4) aj4Entrar(lig(AJ_AVANCADAS) ? aj4Set * 4 + aj4Linha : aj4Linha);
      else if (aj4Linha == 4 || aj4Linha == 5) { aj4Util = aj4Linha == 4 ? 2 : 1; aj4Linha = 0; aj4Acao = 0; focoIndice = 0; }
      else if (aj4Linha == 6) { aj4Entrar(7); aj4Excecao = AJ_GUIA; aj4Atual = 7; aj4Grupo = aj4GrupoDe(AJ_GUIA); aj4Grupos = 1; }
      else { aj4Desfazer(); }
    }
    return 1;
  }
  if (uxVoltar(k) && uxRetornarOp >= 0) { int op=uxRetornarOp; uxRetornarOp=-1; focarOpcao(op); return 1; }
  if (uxVoltar(k) || k == SDLK_LEFT) {
    if (!aj4Grupos && (lig(AJ_AVANCADAS) || aj4Excecao>=0)) aj4Grupos = 1;
    else { focoIndice = 1; aj4Linha = 0; aj4Excecao = -1; }
    return 1;
  }
  if (aj4Grupos) {
    int gs[AJ4_N_GRUPOS], n = aj4GruposLista(aj4Atual, gs);
    if (k == SDLK_UP) { if (aj4GrupoFoco) aj4GrupoFoco--; else aj4Topo = 0; }
    if (k == SDLK_DOWN && aj4GrupoFoco + 1 < n) aj4GrupoFoco++;
    if ((uxOk(k) || k == SDLK_RIGHT) && n) {
      aj4Grupo = gs[aj4GrupoFoco]; aj4Linha = 0; aj4Grupos = 0;
      n = aj4Lista(aj4Atual, aj4Grupo, aj4Opcoes);
      if (n) { int sec = aj4Atual, ex=aj4Excecao; focarOpcao(aj4Opcoes[0]); aj4Excecao = ex; aj4Atual = sec; }
    }
    return 1;
  }
  int sec = aj4Atual, n = aj4Lista(sec, lig(AJ_AVANCADAS) || aj4Excecao >= 0 ? aj4Grupo : -1, aj4Opcoes);
  if (k == SDLK_RIGHT && aj4Acao) { aj4Escopo = !aj4Escopo; return 1; }
  if (k == SDLK_UP) { if (aj4Acao) aj4Acao = 0; else if (aj4Linha) aj4Linha--; else aj4Topo = 0; }
  if (k == SDLK_DOWN) { if (aj4Linha + 1 < n && !aj4Acao) aj4Linha++; else aj4Acao = 1; }
  if (uxOk(k) && aj4Acao) { aj4PrepararReset(lig(AJ_AVANCADAS) || aj4Excecao>=0 ? sec : -sec-2, aj4Escopo); return 1; }
  if (n) {
    int row = aj4Linha, ex = aj4Excecao, op = aj4Opcoes[row];
    focarOpcao(op); aj4Excecao = ex; aj4Atual = sec; aj4Linha = row;
    if (uxOk(k)) {
      if (inativa(op)) {
        int req=uxRequisito(op);
        if(uxLigarRequisitos(op,1))return 1;
        if(req>=0) { focarOpcao(req); uxRetornarOp=op; return 1; }
      }
      if (op == AJ_PERFIL_ATIVO) { pfAbrir(); return 1; }
      if (!lig(AJ_AVANCADAS) && op == AJ_TRAKT && aj4Excecao<0) {
        focarOpcao(AJ_TRAKT); aj4Util=0; aj4Grupos=1; return 1;
      }
      if (OPCOES[op].tipo != OP_ACAO) { uxAbrirEditor(op); return 1; }
      return 0; // Existing action dispatcher owns keyboards, accounts and panels.
    }
  }
  return 1;
}
