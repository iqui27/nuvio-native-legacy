static int aj2GrupoDe(int s);   // ajustes_ux_v2.inc: o grupo (coluna da grade) da categoria
// #339: AJ2_T_LAYOUT e o segmentado "Painel | Lista" no fim da fileira das pilulas.
enum { AJ2_T_BUSCA = 0, AJ2_T_AV, AJ2_T_PERFIL, AJ2_T_DIF, AJ2_T_LAYOUT, AJ2_T_TOUR, AJ2_T_RESOLVER, AJ2_T_N };
static int aj2TopoVisivel(int t) { return t >= 0 && t < AJ2_T_N && (t != AJ2_T_LAYOUT || ajLayoutSelecionavel()); }
static int ajEmLinha(int op);
static int ajReguaMover(int v, int dx, int dy);
// Interação do seletor: só OK aplica; foco e rascunho nunca persistem.
static int uxVoltar(SDL_Keycode k) {
  return k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE || k == SDLK_DELETE;
}
static int uxOk(SDL_Keycode k) { return k == SDLK_RETURN || k == SDLK_KP_ENTER; }
static void uxNotificar(const char *s) {
  snprintf(uxAviso, sizeof uxAviso, "%s", s);
  uxAvisoAte = SDL_GetTicks() + 4500;
}
static void uxCancelar(void) {
  uxEditor = uxRestaurar = uxConfirmar = uxRodape = uxAvisoRisco = 0;
  emEdicao = 0;
}
static void uxListarDiferencas(void);
static void uxLayoutDisponibilidade(void) {
  if (ajLayoutSelecionavel()) return;
  uxCabLayout = 0;
  if (uxEditor && uxOp == AJ_LAYOUT_AJUSTES) uxCancelar();
  if (focoOp == AJ_LAYOUT_AJUSTES && uxIndice >= 2) {
    int indice = focoIndice, topo = uxTopo;
    focarSecao(secAtual);
    focoIndice = indice; uxTopo = topo;
  }
  if (uxIndice == 1) for (int i = 0; i < uxNDifs; i++)
    if (uxDifs[i] == AJ_LAYOUT_AJUSTES) { uxListarDiferencas(); break; }
  if (uxTopo == AJ2_T_LAYOUT) uxTopo = AJ2_T_PERFIL;
}
static void uxListarDiferencas(void) {
  int i;
  uxNDifs = 0;
  for (i = 0; i < AJ_N_TELA; i++)
    if (TELA[i].tipo == IT_OPC && ajOpcaoDisponivel(TELA[i].op) && uxDiferente(TELA[i].op) &&
        (TELA[i].op != AJ_ICONE_APP || apoiador_ativo())) uxDifs[uxNDifs++] = TELA[i].op;
  if (uxDifFoco >= uxNDifs) uxDifFoco = uxNDifs ? uxNDifs - 1 : 0;
}
static int uxRequisito(int op) {
  switch (op) {
    case AJ_PROXIMO_CONTAGEM: return AJ_PROXIMO_AUTO;
    case AJ_VIDRO_CONTORNO: case AJ_VIDRO_OPAC: case AJ_VIDRO_FOSCO: return AJ_VIDRO;
    case AJ_RAIL: case AJ_RAIL_BLUR: return AJ_RAIL_MODERNA;
    case AJ_HERO_CHEIO: return AJ_HOME_LAYOUT;
    case AJ_HERO_CATALOGOS: return AJ_HERO;
    case AJ_HERO_TRAILER_ESPERA: return AJ_HERO_TRAILER;
    case AJ_HERO_TRAILER_SOM:
      return !lig(AJ_HERO_TRAILER) ? AJ_HERO_TRAILER : -1;
    case AJ_DET_TRAILER_SOM:
      return !lig(AJ_DET_TRAILER_AUTO) ? AJ_DET_TRAILER_AUTO : -1;
    case AJ_EXPANDIR_ATRASO: return AJ_EXPANDIR;
    case AJ_CW_OK: case AJ_CW_FONTE: case AJ_CW_PROXIMO: case AJ_CW_ESTILO: case AJ_CW_THUMB:
    case AJ_CW_FURTHEST: case AJ_CW_NAO_EXIBIDOS: case AJ_CW_ORDEM: case AJ_CW_RETIDO_TAMBEM:
    case AJ_CW_CONCLUIDO: case AJ_CW_MINIMO: return AJ_CW_LIGADO;
    case AJ_CW_BLUR_PROX: return !lig(AJ_CW_LIGADO) ? AJ_CW_LIGADO : AJ_CW_THUMB;
    case AJ_PROF_BORDA: case AJ_PROF_BRILHO: case AJ_PROF_COBERTURA:
    case AJ_PROF_POSTERS: case AJ_PROF_CW: case AJ_PROF_EPS:
    case AJ_PROF_ELENCO: case AJ_PROF_TRAILERS: return AJ_PROF;
    case AJ_SEEKR_FITA: case AJ_SEEKR_AJUSTE: return AJ_SEEKR_LIGADO;
    case AJ_SAIDA_PLAYER: return AJ_RELOGIO;
    case AJ_MANTER_VIDEO: return !lig(AJ_RELOGIO) ? AJ_RELOGIO : AJ_SAIDA_PLAYER;
    case AJ_TMDB_IDIOMA: case AJ_TMDB_ARTE: case AJ_TMDB_FICHA: case AJ_TMDB_DATAS:
    case AJ_TMDB_ELENCO: case AJ_TMDB_PROD: case AJ_TMDB_REDES: case AJ_TMDB_EPS:
    case AJ_TMDB_TRAILERS: case AJ_TMDB_MAIS: case AJ_TMDB_COL: case AJ_TMDB_CW:
      return AJ_TMDB_LIGADO;
    case AJ_MDB_TRAKT: case AJ_MDB_IMDB: case AJ_MDB_TMDB:
    case AJ_MDB_LETTER: case AJ_MDB_TOMATES: case AJ_MDB_AUDIENCIA:
    case AJ_MDB_META: case AJ_MDB_MAL: return AJ_MDB_LIGADO;
    case AJ_NT_TOMATES: case AJ_NT_AUDIENCIA: case AJ_NT_META: case AJ_NT_METAUSER:
    case AJ_NT_TMDB: case AJ_NT_LETTER: case AJ_NT_MAL: case AJ_NT_EBERT: case AJ_NT_SCORE:
      return AJ_MDB_CHAVE;   // 2.0.3: o MDBList liga junto com a nota (notaLigarPar)
    case AJ_POSTER_INST: case AJ_POSTER_TOKEN: case AJ_POSTER_EXTRA:
    case AJ_POSTER_CHAVE: case AJ_POSTER_MODELO: case AJ_POSTER_TESTAR:
    case AJ_ADDON_POSTER: return AJ_POSTER_PROV;
    case AJ_ADDON_LOGO: return lig(AJ_TMDB_LIGADO) ? AJ_TMDB_ARTE : AJ_TMDB_LIGADO;
    default: return -1;
  }
}
// 2.0.3 (M3): "OK liga as duas". Liga, na ordem, os interruptores de que a
// linha depende (ate tres degraus: Continuar > Miniatura > Desfocar) e, se a
// linha tambem e um interruptor desligado, ela junto. Primeiro ensaia em
// valor[] e desfaz: so liga se no fim a linha ficaria ativa. Devolve 2 (liga
// as duas), 1 (liga so o requisito) ou 0 (nao da: o OK mostra o requisito).
static int uxLigarRequisitos(int op, int aplicar) {
  int salvo[AJ_N], ligar[4], n = 0, k, res;
  if (op < 0 || op >= AJ_N || !inativa(op)) return 0;
  memcpy(salvo, valor, sizeof salvo);
  for (k = 0; k < 3 && inativa(op); k++) {
    int req = uxRequisito(op);
    if (req < 0 || !ehInterruptor(req) || lig(req) || inativa(req)) break;
    valor[req] = 0; ligar[n++] = req;
  }
  res = n > 0 && !inativa(op) ? (ehInterruptor(op) && !lig(op) ? 2 : 1) : 0;
  memcpy(valor, salvo, sizeof salvo);
  if (!aplicar || !res) return res;
  for (k = 0; k < n; k++)
    if (!definirValorDireto(ligar[k], 0)) {
      uxNotificar(ligar[k] == AJ_SEEKR_LIGADO && !seekrChave[0] ? seekrPedirChave() :
                  "Não foi possível salvar. O valor anterior foi mantido.");
      return 0;
    }
  if (res == 2) definirValorDireto(op, 0);
  uxNotificar("Ajuste salvo nesta TV.");
  return res;
}
static void uxAbrirEditor(int op) {
  if (op < 0 || op >= AJ_N || !ajOpcaoDisponivel(op) || soLeitura(op)) return;
  uxCancelar();
  uxOp = op; uxOriginal = uxValorAtual(op); uxPendente = uxOriginal;
  uxEditor = inativa(op) ? 2 : 1;
}
static int uxRiscoFinal(int op, int novo) {
  if (op == AJ_FIL_LIMITE && fil_limite_pede_aviso(uxValorAtual(op), novo))
    return SEG_AVISO_FILEIRAS;
  if (op == AJ_ITENS_FILEIRA && novo > 0 && novo > uxValorAtual(op)) return SEG_AVISO_ITENS;
  return 0;
}
static void uxAplicar(void) {
  int antes = uxValorAtual(uxOp), bit;
  // Conta/sync pode ter mudado o valor enquanto o seletor estava aberto.
  if (antes != uxOriginal) {
    uxOriginal = uxPendente = antes;
    uxRestaurar = uxAvisoRisco = uxConfirmar = 0;
    uxNotificar("O valor mudou em outra tela. Confira antes de confirmar.");
    return;
  }
  if (!uxRestaurar && inativa(uxOp)) { uxEditor = 2; return; }
  bit = uxRiscoFinal(uxOp, uxPendente);
  if (bit && !seguro_aviso_visto(bit)) {
    uxAvisoRisco = bit; uxConfirmar = 0; return;
  }
  if (!definirValorDireto(uxOp, uxPendente)) {
    uxNotificar(uxOp == AJ_SEEKR_LIGADO && !seekrChave[0] ? seekrPedirChave() :
                "Não foi possível salvar. O valor anterior foi mantido.");
    return;
  }
  riscoNotar(uxOp, antes);
  uxCancelar();
  uxNotificar(uxOp == AJ_PERFIL_PESQ ? "Pedido enviado ao perfil." : "Ajuste salvo nesta TV.");
  // A lista de diferenças permanece estável até sair dela, mesmo após restaurar.
}
// Interruptor (Ligado/Desligado): OK troca e grava na hora, sem abrir editor nem
// pedir confirmacao. Nenhum interruptor tem aviso de risco (so os numeros de
// memoria tem, e esses continuam passando pelo editor).
static int uxAlternarDireto(int op) {
  int antes, novo;
  if (!ehInterruptor(op) || soLeitura(op) || inativa(op)) return 0;
  uxCancelar();
  antes = uxValorAtual(op);
  novo = antes == 0 ? 1 : 0;
  uxOp = op;
  if (uxRiscoFinal(op, novo)) return 0;
  if (!definirValorDireto(op, novo)) {
    uxNotificar(op == AJ_SEEKR_LIGADO && !seekrChave[0] ? seekrPedirChave() :
                "Não foi possível salvar. O valor anterior foi mantido.");
    return 1;
  }
  riscoNotar(op, antes);
  uxNotificar("Ajuste salvo nesta TV.");
  return 1;
}
// #339: o arranjo dos Ajustes, trocado de onde estiver (pilula do indice,
// segmentado do cabecalho da categoria ou a linha em Aparencia; mesma chave).
static int uxAlternarLayout(void) {
  if (!ajLayoutSelecionavel()) return 1;
  int novo = valor[AJ_LAYOUT_AJUSTES] == 1 ? 0 : 1;
  if (definirValorDireto(AJ_LAYOUT_AJUSTES, novo)) uxNotificar("Ajuste salvo nesta TV.");
  else uxNotificar("Não foi possível salvar. O valor anterior foi mantido.");
  return 1;
}
static int uxEvento(SDL_Keycode k) {
  int layoutOculto = !ajLayoutSelecionavel() && (uxCabLayout || uxTopo == AJ2_T_LAYOUT ||
                       (uxEditor && uxOp == AJ_LAYOUT_AJUSTES));
  uxLayoutDisponibilidade();
  if (layoutOculto) return 1;
  if (uxEditor) {
    if (uxVoltar(k)) {
      if (uxRestaurar || uxAvisoRisco) { uxRestaurar = uxAvisoRisco = uxConfirmar = 0; }
      else uxCancelar();
      return 1;
    }
    if (uxRestaurar || uxAvisoRisco) {
      if (k == SDLK_LEFT || k == SDLK_RIGHT) uxConfirmar = k == SDLK_LEFT;
      if (uxOk(k)) {
        if (!uxConfirmar) { uxRestaurar = uxAvisoRisco = 0; return 1; }
        if (uxAvisoRisco) { seguro_aviso_marcar(uxAvisoRisco); uxAvisoRisco = 0; }
        if (uxRestaurar) uxPendente = uxValorPadrao(uxOp);
        uxAplicar();
      }
      return 1;
    }
    // NA LINHA (segmentado, interruptor, cor de destaque): as setas laterais
    // movem o valor pendente; nao ha rodape de "Restaurar padrao" ali.
    // A REGUA DA COR: ← → na fileira, ↑ ↓ entre as fileiras (claros,
    // profundos, dinamicas, Textura), na ordem da tela.
    if (uxOp == AJ_TEMA && ajEmLinha(uxOp) && uxEditor == 1 &&
        (k == SDLK_LEFT || k == SDLK_RIGHT || k == SDLK_UP || k == SDLK_DOWN)) {
      uxPendente = ajReguaMover(uxPendente, k == SDLK_RIGHT ? 1 : k == SDLK_LEFT ? -1 : 0,
                                k == SDLK_DOWN ? 1 : k == SDLK_UP ? -1 : 0);
      return 1;
    }
    if (ajEmLinha(uxOp) && uxEditor == 1 && (k == SDLK_LEFT || k == SDLK_RIGHT)) {
      uxPendente += k == SDLK_RIGHT ? 1 : -1;
      if (uxPendente < 0) uxPendente = 0;
      if (uxPendente >= nValores(uxOp)) uxPendente = nValores(uxOp) - 1;
      return 1;
    }
    // Barra horizontal: laterais editam, baixo/cima navegam para restaurar.
    if (uxEditor == 1 && OPCOES[uxOp].tipo == OP_NUMERO) {
      if (k == SDLK_UP || k == SDLK_DOWN) {
        uxRodape = k == SDLK_DOWN && uxTemPadrao(uxOp); return 1;
      }
      if (k == SDLK_LEFT || k == SDLK_RIGHT) {
        if (!uxRodape) uxPendente = uxOp == AJ_FIL_LIMITE
          ? fil_limite_passo(uxPendente, k == SDLK_RIGHT ? 1 : -1)
          : limita(uxOp, uxPendente + (k == SDLK_RIGHT ? 1 : -1) * OPCOES[uxOp].passo);
        return 1;
      }
    }
    if (k == SDLK_RIGHT && uxTemPadrao(uxOp)) { uxRodape = 1; return 1; }
    if (k == SDLK_LEFT) { uxRodape = 0; return 1; }
    if (uxOk(k)) {
      if (uxRodape) { uxRestaurar = 1; uxConfirmar = 0; return 1; }
      if (uxEditor == 2) {
        int req = uxRequisito(uxOp);
        if (req >= 0) { uxRetornarOp = uxOp; uxCancelar(); focarOpcao(req); }
        return 1;
      }
      uxAplicar(); return 1;
    }
    if (!uxRodape && uxEditor == 1 && (k == SDLK_UP || k == SDLK_DOWN)) {
      int d = k == SDLK_DOWN ? 1 : -1;
      if (uxOp == AJ_FIL_LIMITE)
        uxPendente = fil_limite_passo(uxPendente, d);   // Android: 40 <-> Ilimitado
      else if (OPCOES[uxOp].tipo == OP_NUMERO)
        uxPendente = limita(uxOp, uxPendente + d * OPCOES[uxOp].passo);
      else {
        int ant = uxPendente;
        do uxPendente += d;
        while (uxOp == AJ_TEX_MB && uxPendente >= 0 && uxPendente < nValores(uxOp) && texAcima(uxPendente));
        if (uxPendente < 0) uxPendente = 0;
        if (uxPendente >= nValores(uxOp)) uxPendente = nValores(uxOp) - 1;
        if (uxOp == AJ_TEX_MB && texAcima(uxPendente)) uxPendente = ant;
      }
    }
    return 1;
  }
  if (uxVoltar(k) && uxVeioBusca && uxRetornarOp < 0) {
    uxPediuBusca = 2; uxVeioBusca = 0; return 1;
  }
  // #339: o segmentado "Painel | Lista" do cabecalho da categoria (Cima na
  // primeira linha). OK troca; a Direita escolhe Lista e a Esquerda Painel (no
  // Painel, a Esquerda sai para o indice como em qualquer linha); Baixo volta
  // para a linha de onde veio, que segue a mesma.
  if (uxCabLayout && !focoIndice) {
    int lista = valor[AJ_LAYOUT_AJUSTES] == 1;
    if (uxOk(k) || (k == SDLK_RIGHT && !lista) || (k == SDLK_LEFT && lista)) return uxAlternarLayout();
    if (uxVoltar(k) || k == SDLK_LEFT) { uxCabLayout = 0; focoIndice = 1; sairArmado = 0; return 1; }
    if (k == SDLK_DOWN) uxCabLayout = 0;
    return 1;
  }
  if (focoIndice) {
    // 2.0.3: o indice e a GRADE do mockup v3 (ajustes_ux_v2.inc). No alto, as
    // tres pilulas (Buscar, Avancadas, Perfil) e embaixo delas os dois cartoes
    // de descoberta (O que o Nuvio faz? sobre Assistir e Telas, Resolver um
    // problema sobre Conta e sistema). Depois, uma coluna por grupo: Cima/Baixo
    // andam na coluna, Esquerda/Direita trocam de coluna na mesma altura, OK
    // abre. Esquerda na primeira peca de uma fileira sai.
    int ini[4] = { 0, nSecoes, nSecoes, nSecoes }, s_;
    for (s_ = nSecoes - 1; s_ >= 0; s_--) ini[aj2GrupoDe(s_)] = s_;
    if (ini[2] < ini[1]) ini[2] = ini[1];
    if (uxTopo >= 0) {
      int t = uxTopo;
      if (uxVoltar(k)) { sair = 1; return 1; }
      if (uxOk(k)) {
        if (t == AJ2_T_BUSCA) { uxVeioBusca = 0; uxPediuBusca = 1; return 1; }
        if (t == AJ2_T_AV) {   // liga/desliga as opcoes avancadas em TODAS as categorias
          int novo = valor[AJ_AVANCADAS] == 0 ? 1 : 0;
          if (definirValorDireto(AJ_AVANCADAS, novo)) uxNotificar("Ajuste salvo nesta TV.");
          else uxNotificar("Não foi possível salvar. O valor anterior foi mantido.");
          return 1;
        }
        if (t == AJ2_T_PERFIL) { focarOpcao(AJ_PERFIL_ATIVO); return 1; }
        // Diferentes do padrao: abre a lista (degrau 1); Voltar devolve a pilula (uxTopo fica nela).
        if (t == AJ2_T_DIF) { uxListarDiferencas(); uxDifFoco = 0; uxIndice = 1; focoIndice = 0; return 1; }
        if (t == AJ2_T_LAYOUT) return uxAlternarLayout();   // o foco fica na pilula
        // "Resolver um problema" e a tela inteira da 2.1 (resolver.h); Voltar
        // nela devolve a este cartao. Os demais abrem o Guia de uso (o tour
        // completo, com "Ligar agora" por funcao, e da 2.1).
        if (t == AJ2_T_RESOLVER) { resolver_abrir(); return 1; }
        guiaAbrir(0); guiaDoIndice = 1;
        return 1;
      }
      if (k == SDLK_LEFT) {
        if (t == AJ2_T_BUSCA || t == AJ2_T_TOUR) { sair = 1; return 1; }
        uxTopo = t == AJ2_T_RESOLVER ? AJ2_T_TOUR : t - 1;
      } else if (k == SDLK_RIGHT) {
        if (t < (ajLayoutSelecionavel() ? AJ2_T_LAYOUT : AJ2_T_DIF) || t == AJ2_T_TOUR) uxTopo = t + 1;
      } else if (k == SDLK_DOWN) {
        if (t <= AJ2_T_AV) uxTopo = AJ2_T_TOUR;
        else if (t == AJ2_T_PERFIL || t == AJ2_T_DIF || t == AJ2_T_LAYOUT) uxTopo = AJ2_T_RESOLVER;
        else {
          int g = t == AJ2_T_TOUR || ajustes_layout_lista() || ajRetrato() ? 0 : 2, dest = ini[g] < nSecoes ? ini[g] : 0;
          uxVeioBusca = 0; focarSecao(dest); uxIndice = dest + 2;
        }
      } else if (k == SDLK_UP) {
        if (t == AJ2_T_TOUR) uxTopo = AJ2_T_BUSCA;
        else if (t == AJ2_T_RESOLVER) uxTopo = AJ2_T_PERFIL;
      }
      return 1;
    }
    { int sec = uxIndice - 2, g = sec < ini[1] ? 0 : sec < ini[2] ? 1 : 2, lin = sec - ini[g], dest = -1;
      if (uxVoltar(k)) { sair = 1; return 1; }
      if (uxOk(k)) { focoIndice = 0; uxCabLayout = 0; return 1; }
      // #339 "Lista": uma coluna so. Cima/Baixo andam por todas as categorias,
      // Cima na primeira sobe para "O que o Nuvio faz?", Esquerda sai e a
      // Direita nao tem para onde ir.
      if (ajustes_layout_lista() || ajRetrato()) {
        if (k == SDLK_UP) {
          if (sec > 0) dest = sec - 1;
          else { uxTopo = AJ2_T_TOUR; uxVeioBusca = 0; return 1; }
        }
        if (k == SDLK_DOWN && sec + 1 < nSecoes) dest = sec + 1;
        if (k == SDLK_LEFT) { sair = 1; return 1; }
        if (dest >= 0 && dest < nSecoes) { focarSecao(dest); uxIndice = dest + 2; uxVeioBusca = 0; }
        return 1;
      }
      if (k == SDLK_UP) {
        if (lin > 0) dest = sec - 1;
        else { uxTopo = g == 2 ? AJ2_T_RESOLVER : AJ2_T_TOUR; uxVeioBusca = 0; return 1; }
      }
      if (k == SDLK_DOWN && sec + 1 < ini[g + 1]) dest = sec + 1;
      if (k == SDLK_LEFT && g == 0) { sair = 1; return 1; }
      if ((k == SDLK_LEFT || k == SDLK_RIGHT) && (k == SDLK_LEFT || g < 2)) {
        int g2 = g + (k == SDLK_RIGHT ? 1 : -1);
        if (ini[g2] < ini[g2 + 1]) {
          dest = ini[g2] + lin;
          if (dest >= ini[g2 + 1]) dest = ini[g2 + 1] - 1;
        }
      }
      if (dest >= 0 && dest < nSecoes) { focarSecao(dest); uxIndice = dest + 2; uxVeioBusca = 0; }
      return 1; }
  }
  if (uxIndice == 1) {
    if (uxVoltar(k) || k == SDLK_LEFT) { focoIndice = 1; return 1; }
    if (k == SDLK_UP && uxDifFoco > 0) uxDifFoco--;
    if (k == SDLK_DOWN && uxDifFoco + 1 < uxNDifs) uxDifFoco++;
    if (uxOk(k) && uxNDifs && !uxAlternarDireto(uxDifs[uxDifFoco])) uxAbrirEditor(uxDifs[uxDifFoco]);
    return 1;
  }
  if (uxVoltar(k) && uxRetornarOp >= 0) {
    int op = uxRetornarOp; uxRetornarOp = -1; focarOpcao(op); return 1;
  }
  if (k == SDLK_UP || k == SDLK_DOWN || k == SDLK_LEFT ||
      k == SDLK_PAGEUP || k == SDLK_PAGEDOWN) uxRetornarOp = -1;
  if (uxVoltar(k) || k == SDLK_LEFT) {
    focoIndice = 1; sairArmado = 0; return 1;
  }
  if (k == SDLK_UP || k == SDLK_DOWN) {
    int dir = k == SDLK_DOWN ? 1 : -1, i;
    for (i = focoItem + dir; i > secIni[secAtual] && i < secFim(secAtual); i += dir)
      if (focavel(i)) { focar(i); return 1; }
    // #339: Cima na primeira linha sobe para o "Painel | Lista" do cabecalho.
    if (dir < 0 && uxIndice >= 2 && !uxVeioBusca && ajLayoutSelecionavel()) uxCabLayout = 1;
    return 1;
  }
  if (k == SDLK_RIGHT) return 1;
  if (uxOk(k) && focoOp >= 0 && uxAlternarDireto(focoOp)) return 1;
  // Linha apagada (2.0.3): OK liga o que falta, como a propria linha diz; quando
  // o requisito nao e um interruptor (layout, chave da conta...), leva ate ele
  // e Voltar devolve aqui. A modal de dependencia ficou para os atalhos.
  if (uxOk(k) && focoOp >= 0 && inativa(focoOp)) {
    int req = uxRequisito(focoOp);
    if (uxLigarRequisitos(focoOp, 1)) return 1;
    if (req >= 0) { int op = focoOp; uxCancelar(); focarOpcao(req); uxRetornarOp = op; return 1; }
    uxAbrirEditor(focoOp); return 1;
  }
  return 0; // Ações existentes: conta, teclado, fileiras, diagnóstico.
}
