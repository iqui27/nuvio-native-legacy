// AJUSTES POR PERFIL (2.1): a folha do perfil, o administrador e o bloqueio.
//
// Incluido por ajustes.c ANTES da interacao (logica e teclas); o desenho mora em
// ajustes_ux_perfil_desenho.inc, depois de ajustes_ux_desenho.inc.
//
// QUEM E QUEM. O administrador e o perfil principal da conta (perfis_principal).
// Qualquer perfil nao infantil pode pedir uma acao de admin, mas ela so roda
// depois do PIN de administrador: o PIN do perfil principal na conta quando ele
// tem um; senao o PIN LOCAL desta TV (admin.c), que so o proprio principal pode
// criar. Perfil infantil nunca abre a folha de admin. O PIN vale ate sair dos
// Ajustes ou trocar de perfil (pfAdminOk guarda para QUAL perfil foi dado).
//
// BLOQUEIO. Perfil infantil com "travar ajustes" nao abre o menu: a tela inteira
// vira a folha de PIN. A conferencia roda a CADA QUADRO (pfTravado), nao so na
// entrada, e o desbloqueio vale para um perfil so: trocar de perfil de dentro dos
// Ajustes para um travado volta a travar na hora.
#include "admin.h"

enum { PF_FECHADA = 0, PF_MENU, PF_EDITAR, PF_COPIAR, PF_INFANTIL, PF_CONFIRMA, PF_BLOQUEIO };
enum { PA_NADA = 0, PA_EDITAR, PA_COPIAR, PA_INFANTIL, PA_DESBLOQ };
enum { PL_FECHAR = 1, PL_EDITAR_OUTRO, PL_COPIAR, PL_RESTAURAR, PL_INFANTIL, PL_TERMINAR, PL_RESTAURAR_ED,
       PL_PERFIL, PL_COPIAR_AGORA, PL_SIM, PL_NAO, PL_DESBLOQ };
#define PF_CAMPO (AJ_N + 7)     // stCampo da modal de PIN de administrador
#define PF_MAX 14

typedef struct { char rot[96], apoio[120]; int id, arg, marca, ativa; } PfLinha;

static int pfTela, pfFoco;
static int pfConfirmaDe;                    // de que lista a confirmacao veio (PF_MENU ou PF_COPIAR)
static int pfConfirmaTipo;                  // PL_COPIAR ou PL_RESTAURAR ou PL_RESTAURAR_ED
static int pfMarca[CONTA_PERFIL_MAX + 1];   // checklist da copia, por lugar na lista
static int pfPend, pfPendAlvo;              // acao esperando o PIN
static int pfAdminOk;                       // profile_index para o qual o PIN ja foi dado (0 = nenhum)
static int pfDesbloq;                       // perfil travado aberto nesta visita
static int pfFase;                          // 1 conta, 2 local, 3 criar, 4 repetir
static char pfPinNovo[ADMIN_PIN_MAX + 2];
static pthread_t admFio;
static atomic_int admEstado;                // 0 livre, 1 verificando, 2 aceito, 3 recusado, 4 sem rede
static int admFioVivo, admIdx, admPerfil;
static char admPin[24];

static const char *pfNome(int idx) {
  int i;
  for (i = 0; i < perfis_n(); i++) { const ContaPerfil *p = perfis_item(i); if (p && p->indice == idx) return p->nome; }
  return "";
}
static int pfEhAdmin(void) { return perfis_ativo() == perfis_principal(); }
static int pfTravado(void) {
  return perfil_ajustes_travados(perfis_ativo()) && pfDesbloq != perfis_ativo();
}
// 0 sem trava, 1 infantil, 2 infantil com ajustes travados (rotulo da lista).
static int pfEstadoInfantil(int idx) {
  return perfil_infantil(idx) ? (perfil_ajustes_travados(idx) ? 2 : 1) : 0;
}

static void pfWipePin(void) { memset(admPin, 0, sizeof admPin); memset(pfPinNovo, 0, sizeof pfPinNovo); teclado_esquecer(); }

static void pfAbrirPin(const char *titulo, const char *dica) {
  teclado_abrir_com(titulo, dica, ADMIN_PIN_MAX, "0123456789", NULL);
  teclado_tipo(TECLADO_TIPO_SENHA);
  teclado_mascarar(1);
  stCampo = PF_CAMPO;
}

static void pfFechar(void) { pfTela = PF_FECHADA; pfFoco = 0; }

static void pfExecutar(void) {
  int acao = pfPend, alvo = pfPendAlvo;
  pfPend = PA_NADA;
  if (acao == PA_EDITAR) {
    if (ajustes_edicao_iniciar(alvo)) { pfFechar(); uxNotificar("Editando os ajustes de outro perfil. As mudanças valem só para ele."); }
    else uxNotificar("Não foi possível abrir os ajustes desse perfil.");
  } else if (acao == PA_COPIAR) {
    int i, ok = 0, falha = 0, k = 0;
    for (i = 0; i < perfis_n(); i++) {
      const ContaPerfil *p = perfis_item(i);
      if (!p || p->indice == perfis_ativo()) continue;
      if (pfMarca[k++] && p) { if (ajustes_copiar_perfil(perfis_ativo(), p->indice) >= 0) ok++; else falha++; }
    }
    memset(pfMarca, 0, sizeof pfMarca);
    pfFechar();
    uxNotificar(falha ? "Alguns ajustes não puderam ser copiados." : "Ajustes copiados. Quem recebeu pode mudar o que quiser.");
    (void)ok;
  } else if (acao == PA_INFANTIL) {
    int s = pfEstadoInfantil(alvo), prox = (s + 1) % 3;
    if (prox == 0) { perfil_ajustes_travar(alvo, 0); perfil_infantil_definir(alvo, 0); }
    else {
      perfil_infantil_definir(alvo, 1);
      if (prox == 1) perfil_ajustes_travar(alvo, 0);
      else perfil_ajustes_travar(alvo, 1);
      if (s == 0) ajustes_padroes_infantis(alvo);   // padroes seguros so ao MARCAR como infantil
    }
    // Se mexeu no proprio perfil e travou, o bloqueio entra na hora (ate o PIN).
  } else if (acao == PA_DESBLOQ) {
    pfDesbloq = perfis_ativo();
    pfFechar();
  }
}

// Pede a acao de admin: roda ja se o PIN desta visita vale, senao abre o PIN.
static void pfExigirAdmin(int acao, int alvo) {
  const ContaPerfil *pp = NULL;
  int i;
  if (perfil_infantil_ativo() && acao != PA_DESBLOQ) { uxNotificar("Indisponível em perfil infantil."); return; }
  pfPend = acao; pfPendAlvo = alvo;
  if (pfAdminOk == perfis_ativo() && acao != PA_DESBLOQ) { pfExecutar(); return; }
  for (i = 0; i < perfis_n(); i++) { const ContaPerfil *p = perfis_item(i); if (p && p->indice == perfis_principal()) pp = p; }
  if (pp && pp->temPin) { pfFase = 1; admIdx = pp->indice; pfAbrirPin("PIN de administrador", "O PIN do perfil principal da conta."); return; }
  if (admin_pin_definido()) { pfFase = 2; pfAbrirPin("PIN de administrador", "O PIN de administrador desta TV."); return; }
  if (pfEhAdmin() && !perfil_infantil_ativo()) {
    pfFase = 3;
    pfAbrirPin("Crie o PIN de administrador", "De 4 a 8 números. Vale só nesta TV e protege os ajustes dos outros perfis.");
    return;
  }
  pfPend = PA_NADA;
  uxNotificar("Peça ao administrador (perfil principal) para criar o PIN de administrador.");
}

static int pfMudarInfantil(int ligado) {
  if (ligado == perfil_infantil_ativo()) return 1;
  pfExigirAdmin(PA_INFANTIL, perfis_ativo());
  return 1;
}

static void *admTrabalho(void *u) {
  int v = perfis_verificar_pin(admIdx, admPin);
  (void)u;
  memset(admPin, 0, sizeof admPin);
  atomic_store(&admEstado, v > 0 ? 2 : (v == 0 ? 3 : 4));
  return NULL;
}

// Texto digitado na modal de PIN. 1 quando reabriu a modal (segunda digitacao).
static int pfDigitado(const char *pin) {
  size_t n = strlen(pin), i;
  int digitos = 1;
  for (i = 0; i < n; i++) if (pin[i] < '0' || pin[i] > '9') digitos = 0;
  if (pfFase == 3) {
    if (!digitos || n < ADMIN_PIN_MIN || n > ADMIN_PIN_MAX) { pfWipePin(); pfPend = PA_NADA; uxNotificar("O PIN precisa ter de 4 a 8 números."); return 0; }
    snprintf(pfPinNovo, sizeof pfPinNovo, "%s", pin);
    teclado_esquecer();
    pfFase = 4;
    pfAbrirPin("Repita o PIN de administrador", "Digite o mesmo PIN de novo.");
    return 1;
  }
  if (pfFase == 4) {
    int igual = !strcmp(pin, pfPinNovo), gravou = igual && admin_pin_definir(pfPinNovo);
    pfWipePin();
    if (!igual) { pfPend = PA_NADA; uxNotificar("Os PINs não conferem."); return 0; }
    if (!gravou) { pfPend = PA_NADA; uxNotificar("Não foi possível guardar o PIN."); return 0; }
    pfAdminOk = perfis_ativo();
    uxNotificar("PIN de administrador criado.");
    pfExecutar();
    return 0;
  }
  if (pfFase == 2) {
    int ok = digitos && admin_pin_conferir(pin);
    pfWipePin();
    if (!ok) { pfPend = PA_NADA; uxNotificar("PIN incorreto"); return 0; }
    pfAdminOk = perfis_ativo();
    pfExecutar();
    return 0;
  }
  if (pfFase == 1) {
    admPerfil = perfis_ativo();
    if (!digitos || atomic_load(&admEstado) == 1) { pfWipePin(); pfPend = PA_NADA; return 0; }
    snprintf(admPin, sizeof admPin, "%s", pin);
    teclado_esquecer();
    if (admFioVivo) { pthread_join(admFio, NULL); admFioVivo = 0; }
    atomic_store(&admEstado, 1);
    if (pthread_create(&admFio, NULL, admTrabalho, NULL) != 0) { memset(admPin, 0, sizeof admPin); atomic_store(&admEstado, 4); }
    else admFioVivo = 1;
    return 0;
  }
  pfWipePin();
  return 0;
}

static void pfPinCancelado(void) { pfWipePin(); pfPend = PA_NADA; }

static void pfRecolher(void) {
  int e = atomic_load(&admEstado);
  if (e < 2) return;
  if (admFioVivo) { pthread_join(admFio, NULL); admFioVivo = 0; }
  atomic_store(&admEstado, 0);
  if (!pfPend || admPerfil != perfis_ativo()) { pfPend = PA_NADA; return; }
  if (e == 2) { pfAdminOk = perfis_ativo(); pfExecutar(); }
  else { pfPend = PA_NADA; uxNotificar(e == 3 ? "PIN incorreto" : "Sem conexão para conferir o PIN."); }
}

// Chamado a cada quadro dos Ajustes: o bloqueio de perfil infantil nunca depende
// de ter sido conferido na entrada.
static void pfVigiar(void) {
  pfRecolher();
  if (pfTravado() && pfTela != PF_BLOQUEIO) {
    ajustes_edicao_encerrar();
    uxCancelar();
    if (guiaAberto) guiaFechar();
    pfTela = PF_BLOQUEIO; pfFoco = 0;
  } else if (!pfTravado() && pfTela == PF_BLOQUEIO) pfFechar();
}

// A entrada nos Ajustes: o PIN e o desbloqueio valem por uma visita.
static void pfEntrada(void) {
  pfAdminOk = 0; pfDesbloq = 0; pfPend = PA_NADA; pfTela = PF_FECHADA; pfFoco = 0;
  memset(pfMarca, 0, sizeof pfMarca);
  if (pfTravado()) { pfTela = PF_BLOQUEIO; }
}
// A saida dos Ajustes encerra a edicao de outro perfil e o PIN.
static void pfSaida(void) {
  ajustes_edicao_encerrar();
  pfWipePin();
  pfAdminOk = 0; pfDesbloq = 0; pfPend = PA_NADA; pfTela = PF_FECHADA;
}

static void pfRotulo(PfLinha *l, int id, const char *rot, const char *apoio) {
  memset(l, 0, sizeof *l);
  l->id = id;
  snprintf(l->rot, sizeof l->rot, "%s", rot);
  snprintf(l->apoio, sizeof l->apoio, "%s", apoio ? apoio : "");
}

// As linhas da lista atual. Devolve quantas.
static int pfLinhas(PfLinha *L) {
  int n = 0, i, k = 0, outros = perfis_n() > 1;
  char b[160];
  if (pfTela == PF_MENU) {
    if (ajustes_edicao_perfil()) {
      snprintf(b, sizeof b, i18n("Terminar a edição de %s"), pfNome(ajustes_edicao_perfil()));
      pfRotulo(&L[n++], PL_TERMINAR, b, i18n("Volta para os seus ajustes"));
      snprintf(b, sizeof b, i18n("Restaurar o padrão de %s"), pfNome(ajustes_edicao_perfil()));
      pfRotulo(&L[n++], PL_RESTAURAR_ED, b, i18n("Só as opções do perfil, não as da TV"));
    } else if (perfil_infantil_ativo()) {
      pfRotulo(&L[n++], PL_RESTAURAR, "Restaurar o padrão deste perfil", "Só as opções deste perfil");
    } else {
      if (outros) pfRotulo(&L[n++], PL_EDITAR_OUTRO, "Editar ajustes de outro perfil", "Pede o PIN de administrador");
      if (outros) pfRotulo(&L[n++], PL_COPIAR, "Copiar meus ajustes para…", "Aparência, player, home e legendas");
      pfRotulo(&L[n++], PL_RESTAURAR, "Restaurar o padrão deste perfil", "Só as opções deste perfil");
      if (outros) pfRotulo(&L[n++], PL_INFANTIL, "Perfil infantil e trava de ajustes…", "Pede o PIN de administrador");
    }
    pfRotulo(&L[n++], PL_FECHAR, "Fechar", NULL);
    return n;
  }
  if (pfTela == PF_EDITAR || pfTela == PF_COPIAR || pfTela == PF_INFANTIL) {
    for (i = 0; i < perfis_n() && n < PF_MAX - 3; i++) {
      const ContaPerfil *p = perfis_item(i);
      if (!p) continue;
      if (pfTela != PF_INFANTIL && p->indice == perfis_ativo()) continue;
      pfRotulo(&L[n], PL_PERFIL, p->nome, "");
      L[n].arg = p->indice;
      if (pfTela == PF_COPIAR) { L[n].marca = pfMarca[k]; L[n].ativa = 1; k++; }
      if (pfTela == PF_INFANTIL) {
        int s = pfEstadoInfantil(p->indice);
        snprintf(L[n].apoio, sizeof L[n].apoio, "%s", s == 2 ? i18n("Infantil, ajustes travados") : s == 1 ? i18n("Infantil") : i18n("Comum"));
      } else if (p->indice == perfis_principal()) snprintf(L[n].apoio, sizeof L[n].apoio, "%s", i18n("Administrador"));
      else if (perfil_infantil(p->indice)) snprintf(L[n].apoio, sizeof L[n].apoio, "%s", i18n("Infantil"));
      n++;
    }
    if (pfTela == PF_COPIAR) {
      int algum = 0;
      for (i = 0; i < k; i++) algum |= pfMarca[i];
      pfRotulo(&L[n], PL_COPIAR_AGORA, "Copiar", algum ? NULL : i18n("Marque ao menos um perfil"));
      L[n].ativa = algum; n++;
    }
    pfRotulo(&L[n++], PL_FECHAR, "Voltar", NULL);
    return n;
  }
  if (pfTela == PF_CONFIRMA) {
    pfRotulo(&L[n++], PL_SIM, pfConfirmaTipo == PL_COPIAR ? "Copiar" : "Restaurar", NULL);
    pfRotulo(&L[n++], PL_NAO, "Cancelar", NULL);
    return n;
  }
  if (pfTela == PF_BLOQUEIO) {
    pfRotulo(&L[n++], PL_DESBLOQ, "Digitar o PIN de administrador", NULL);
    pfRotulo(&L[n++], PL_FECHAR, "Sair dos ajustes", NULL);
    return n;
  }
  return 0;
}

static void pfAbrir(void) { pfTela = PF_MENU; pfFoco = 0; }

static void pfEvento(SDL_Keycode k) {
  PfLinha L[PF_MAX];
  int n = pfLinhas(L), voltar = uxVoltar(k);
  if (n <= 0) { pfFechar(); return; }
  if (pfFoco >= n) pfFoco = n - 1;
  if (pfTela == PF_BLOQUEIO) {
    if (voltar) { sair = 1; return; }   // nunca fecha a folha sem PIN: sai dos Ajustes
  } else if (voltar) {
    if (pfTela == PF_CONFIRMA) pfTela = pfConfirmaDe;
    else if (pfTela == PF_MENU) pfFechar();
    else pfTela = PF_MENU;
    pfFoco = 0;
    return;
  }
  if (k == SDLK_UP && pfFoco > 0) pfFoco--;
  else if (k == SDLK_DOWN && pfFoco + 1 < n) pfFoco++;
  else if (uxOk(k)) {
    const PfLinha *l = &L[pfFoco];
    switch (l->id) {
      case PL_FECHAR:
        if (pfTela == PF_BLOQUEIO) sair = 1;
        else if (pfTela == PF_MENU) pfFechar();
        else { pfTela = PF_MENU; pfFoco = 0; }
        break;
      case PL_EDITAR_OUTRO: pfTela = PF_EDITAR; pfFoco = 0; break;
      case PL_COPIAR: memset(pfMarca, 0, sizeof pfMarca); pfTela = PF_COPIAR; pfFoco = 0; break;
      case PL_INFANTIL: pfTela = PF_INFANTIL; pfFoco = 0; break;
      case PL_RESTAURAR: case PL_RESTAURAR_ED:
        pfConfirmaTipo = l->id; pfConfirmaDe = PF_MENU; pfTela = PF_CONFIRMA; pfFoco = 1; break;
      case PL_TERMINAR: ajustes_edicao_encerrar(); pfFechar(); uxNotificar("Edição encerrada. Voltaram os seus ajustes."); break;
      case PL_PERFIL:
        if (pfTela == PF_EDITAR) pfExigirAdmin(PA_EDITAR, l->arg);
        else if (pfTela == PF_INFANTIL) pfExigirAdmin(PA_INFANTIL, l->arg);
        else if (pfTela == PF_COPIAR) {
          int i, kk = 0;
          for (i = 0; i < pfFoco; i++) if (L[i].id == PL_PERFIL) kk++;
          pfMarca[kk] = !pfMarca[kk];
        }
        break;
      case PL_COPIAR_AGORA:
        if (l->ativa) { pfConfirmaTipo = PL_COPIAR; pfConfirmaDe = PF_COPIAR; pfTela = PF_CONFIRMA; pfFoco = 1; }
        break;
      case PL_SIM:
        if (pfConfirmaTipo == PL_COPIAR) { pfTela = PF_COPIAR; pfExigirAdmin(PA_COPIAR, 0); }
        else {
          int alvo = pfConfirmaTipo == PL_RESTAURAR_ED ? ajustes_edicao_perfil() : perfis_ativo();
          int mudou;
          // Restaurar o perfil que esta em edicao acontece DENTRO da edicao: valor[] e dele.
          if (alvo == ajustes_edicao_perfil() && alvo) {
            int i;
            mudou = 0;
            for (i = 0; i < AJ_N; i++) if (dePerfil(i) && valor[i] != valorPadrao[i]) { valor[i] = valorPadrao[i]; mudou++; }
            gravar(); reaplicarEfeitosPerfil();
          } else mudou = ajustes_restaurar_padrao_perfil(alvo);
          pfFechar();
          uxNotificar(mudou >= 0 ? "Padrão restaurado para este perfil." : "Não foi possível restaurar.");
        }
        break;
      case PL_NAO: pfTela = pfConfirmaDe; pfFoco = 0; break;
      case PL_DESBLOQ: pfExigirAdmin(PA_DESBLOQ, 0); break;
    }
  }
}

// O rotulo do chip do alto do indice: "Perfil: <nome>", ou "Editando: <nome>".
static void pfRotuloChip(char *dst, size_t n) {
  int ed = ajustes_edicao_perfil();
  const char *nome = pfNome(ed ? ed : perfis_ativo());
  if (!nome[0]) nome = i18n("Perfil");
  snprintf(dst, n, ed ? i18n("Editando: %s") : i18n("Perfil: %s"), nome);
}
