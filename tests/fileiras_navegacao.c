// Navegacao por controle (setas + OK) na folha "Fileiras da Home".
//
// Dono na LG C9: "Nao tem como chegar nesses botoes apertando pro lado; eles
// so sao acessiveis se eu for pro final da lista embaixo". Os chips da barra
// do topo (Ordenar, Atualizar tudo) moravam no FIM da lista; agora moram na
// barra com as abas, e TODO ponto da tela alcanca TODOS os outros sem varrer
// a lista.
//
// Regra verificada (eventoFileiras, sem janela nem desenho):
//   - barra do topo: LEFT/RIGHT andam por aba Na Home, aba Fora, Ordenar
//     (so na Fora) e Atualizar tudo, e voltam; DOWN desce para a lista;
//   - dentro de uma linha da aba Na Home, LEFT/RIGHT andam pelas colunas
//     (Remover, Card, Tamanho) e voltam para a linha;
//   - na aba Fora, RIGHT vai da linha ao botao Adicionar e a regua A-Z,
//     LEFT volta; na regua, UP/DOWN andam de letra em letra;
//   - UP na primeira linha sobe a barra; Escape fecha a folha.
#include "../src/ajustes.c"
#include <assert.h>
#include <stdio.h>

static int falhas;
#define CONFERE(c, msg) do { if (!(c)) { puts("FALHA: " msg); falhas++; } } while (0)

// Estado da folha (somente eventos: nao desenha nada).
static void folha(void) {
  fil_esquecer();
  fil_definir_perfil(17);
  fil_registrar("test_c", "C", "Test", "movie", 1);
  fil_registrar("test_a", "A", "Test", "movie", 1);
  fil_registrar("test_b", "B", "Test", "movie", 1);
  fil_remover(2);         // B: fora da Home (aba 1 tem lista)
  fil_remover(1);         // A: fora tambem
  filAberta = 1; filAba = 0; filNaBarra = 0; filBarra = 0;
  filFoco = 0; filCampo = 0; filPegou = 0; filTopo = 0;
}

static void tecla(SDL_Keycode k) { eventoFileiras(k); }

// Aba Na Home: linhas com colunas, chips da barra via UP + LEFT/RIGHT.
static void navega_home(void) {
  folha();
  filMontarLista();
  int n = filListaN;
  assert(n == 2);   // 0 = Destaque, 1 = C
  CONFERE(filIdx(0) == AJ_FIL_DESTAQUE, "Na Home: linha 0 e o Destaque");

  filFoco = 1;
  tecla(SDLK_RIGHT);
  CONFERE(filCampo == 1, "Na Home: → na linha alcanca a coluna Remover");
  tecla(SDLK_RIGHT);
  CONFERE(filCampo == 2, "Na Home: → alcanca a coluna Card");
  tecla(SDLK_RIGHT);
  CONFERE(filCampo == 3, "Na Home: → alcanca a coluna Tamanho");
  tecla(SDLK_RIGHT);
  CONFERE(filCampo == 3, "Na Home: → na ultima coluna nao passa");
  tecla(SDLK_LEFT); tecla(SDLK_LEFT); tecla(SDLK_LEFT);
  CONFERE(filCampo == 0 && filFoco == 1, "Na Home: ← volta para a linha");

  filFoco = 0;
  tecla(SDLK_UP);
  CONFERE(filNaBarra && filBarra == 0, "Na Home: ↑ no topo sobe a barra na aba atual");
  tecla(SDLK_RIGHT);
  CONFERE(filNaBarra && filBarra == 1 && filAba == 0, "Barra: → alcanca a aba Fora sem trocar");
  tecla(SDLK_RIGHT);
  CONFERE(filBarra == 3, "Barra: → pula o Ordenar (so existe na Fora) e alcanca o Atualizar");
  tecla(SDLK_RIGHT);
  CONFERE(filBarra == 3, "Barra: → no ultimo nao passa");
  tecla(SDLK_RETURN);
  CONFERE(filAviso[0] != '\0', "Barra: OK no Atualizar avisa a sincronizacao");
  filAviso[0] = 0;
  tecla(SDLK_LEFT); tecla(SDLK_LEFT);
  CONFERE(filBarra == 0, "Barra: ← volta pela trilha ate a aba Na Home");
  tecla(SDLK_DOWN);
  CONFERE(!filNaBarra && filAba == 0 && filFoco == 0, "Barra: ↓ volta para a lista");
}

// Aba Fora: linha -> botao Adicionar -> regua A-Z; chips Ordenar e Atualizar na barra.
static void navega_fora(void) {
  folha();
  tecla(SDLK_UP);            // barra
  tecla(SDLK_RIGHT);         // aba Fora
  tecla(SDLK_RETURN);        // OK troca de aba (fica na barra)
  CONFERE(filAba == 1 && filNaBarra, "Fora: OK na aba troca e fica na barra");
  filMontarLista();
  int n = filListaN;
  assert(n == 2);            // A e B fora da Home

  tecla(SDLK_RIGHT);
  CONFERE(filBarra == 2, "Fora: → da aba alcanca o Ordenar");
  tecla(SDLK_RETURN);
  CONFERE(filForaAgrupada == 0 && filNaBarra, "Fora: OK no Ordenar alterna para alfabetica");
  tecla(SDLK_RETURN);
  CONFERE(filForaAgrupada == 1, "Fora: OK de novo volta para por addon");
  tecla(SDLK_RIGHT);
  CONFERE(filBarra == 3, "Fora: → do Ordenar alcanca o Atualizar");
  tecla(SDLK_LEFT);
  CONFERE(filBarra == 2, "Fora: ← do Atualizar volta ao Ordenar");

  tecla(SDLK_DOWN);
  CONFERE(!filNaBarra && filFoco == 0 && filCampo == 0, "Fora: ↓ da barra entra na lista");

  filFoco = n - 1;
  tecla(SDLK_DOWN);
  CONFERE(filFoco == n - 1, "Fora: fim da lista nao vai a chip nenhum");

  filFoco = 0;
  tecla(SDLK_RIGHT);
  CONFERE(filCampo == 1, "Fora: → na linha alcanca o botao Adicionar");
  tecla(SDLK_RIGHT);
  CONFERE(filCampo == FIL_CAMPO_REGUA, "Fora: → do botao alcanca a regua A-Z");
  tecla(SDLK_RIGHT);
  CONFERE(filCampo == FIL_CAMPO_REGUA, "Fora: → na regua nao passa");
  tecla(SDLK_DOWN);
  CONFERE(filFoco == 1, "Regua: ↓ anda uma letra");
  tecla(SDLK_UP);
  CONFERE(filFoco == 0, "Regua: ↑ volta uma letra");
  tecla(SDLK_UP);
  CONFERE(filNaBarra && filBarra == 1 && filCampo == 0, "Regua: ↑ sem letra antes sobe a barra");
  tecla(SDLK_DOWN);
  filCampo = 0; filFoco = 0;
  tecla(SDLK_RIGHT); tecla(SDLK_RIGHT);
  tecla(SDLK_LEFT);
  CONFERE(filCampo == 1, "Fora: ← da regua volta ao botao");
  tecla(SDLK_LEFT);
  CONFERE(filCampo == 0, "Fora: ← do botao volta a linha");

  tecla(SDLK_UP);
  CONFERE(filNaBarra && filBarra == 1, "Fora: ↑ no topo sobe a barra na aba atual");
  tecla(SDLK_DOWN);
  CONFERE(!filNaBarra && filFoco == 0, "Fora: ↓ desce na mesma linha");
  tecla(SDLK_ESCAPE);
  CONFERE(!filAberta, "Folha: Voltar fecha");
}

int main(void) {
  dados_iniciar(getenv("NUVIO_DADOS"));
  navega_home();
  if (!falhas) puts("PASS: Na Home — colunas por → ←, barra completa por ↑ →");
  navega_fora();
  if (!falhas) puts("PASS: Fora — linha → botao → regua A-Z, chips na barra, nada no fim da lista");
  return falhas ? 1 : 0;
}
