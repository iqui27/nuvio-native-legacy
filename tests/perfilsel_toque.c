// Selecionar perfil por toque usa a mesma regra de PIN do controle remoto.
#include <assert.h>
#include <unistd.h>
#include "../src/perfilsel.c"

static ContaPerfil ps[2] = {{.indice = 1}, {.indice = 2, .temPin = 1}};
static int selecionado = 1, total = 2;
const ContaPerfil *perfis_item(int i) { return i >= 0 && i < total ? &ps[i] : NULL; }
int perfis_n(void) { return total; }
int perfis_indice_livre(void) { return total < CONTA_PERFIL_MAX ? total + 1 : 0; }
static int editorOpened, editorAdd;
void perfisedit_abrir(int add) { editorOpened++; editorAdd = add; }
PerfilAcao perfis_acao(int i) { return ps[i].temPin ? PERFIL_ACAO_PIN : PERFIL_ACAO_ENTRAR; }
void perfis_definir_ativo(int i) { selecionado = i; }
static char pinVisto[16]; static _Atomic int verificacoes;
int perfis_verificar_pin(int i, const char *p) {
  (void)i; snprintf(pinVisto, sizeof pinVisto, "%s", p); atomic_fetch_add(&verificacoes, 1); return 0;
}
SyncEstado sync_estado(void) { return SYNC_FALHOU; }
int main(void) {
  foco = 0; pinDe = -1; preparando = verificando = concluido = 0;
  perfilFocar(1, 2); assert(foco == 1);
  perfilAtivar(1, 2); assert(pinDe == 1 && selecionado == 1 && !concluido);
  perfilAtivar(0, 1); assert(selecionado == 1 && pinDe == 1 && !concluido);
  pinFocar(0, 1); eventoPin(SDLK_RETURN); assert(!strcmp(pin, "1"));
  pinFocar(PS_PIN_ZERO, 1); eventoPin(SDLK_RETURN); assert(!strcmp(pin, "10"));
  pinFocar(PS_PIN_APAGAR, 1); eventoPin(SDLK_RETURN); assert(!strcmp(pin, "1"));
  verificando = 1; int anterior = pinFoco;
  pinFocar(3, 1); assert(pinFoco == anterior);
  verificando = 0; eventoPin(SDLK_ESCAPE); eventoPin(SDLK_ESCAPE); assert(pinDe == -1);

  // #289: no OK key; the 4th digit verifies by itself, number keys type.
  perfilAtivar(1, 2); assert(pinDe == 1 && pinFoco == PS_PIN_INICIO);
  eventoPin(SDLK_1); eventoPin(SDLK_KP_2); eventoPin(SDLK_3);
  assert(!strcmp(pin, "123") && !verificando && atomic_load(&verificacoes) == 0);
  eventoPin(SDLK_BACKSPACE); assert(!strcmp(pin, "12"));
  eventoPin(SDLK_3);
  pinFocar(PS_PIN_ZERO, 1); eventoPin(SDLK_RETURN);   // 4th digit from the pad
  assert(verificando);
  for (int i = 0; i < 2000 && !atomic_load(&resultadoPin); i++) usleep(1000);
  assert(atomic_load(&verificacoes) == 1 && !strcmp(pinVisto, "1230"));
  assert(atomic_load(&resultadoPin) == -1);   // stub says wrong
  eventoPin(SDLK_5); assert(strlen(pin) <= PS_PIN_LEN);  // ignored while checking
  verificando = 0; atomic_store(&resultadoPin, 0); pin[0] = 0;
  // The blank cell is never focusable: down from "7" lands on "0", left from
  // "0" stays, and touch on it does nothing.
  pinFoco = 6; eventoPin(SDLK_DOWN); assert(pinFoco == PS_PIN_ZERO);
  eventoPin(SDLK_LEFT); assert(pinFoco == PS_PIN_ZERO);
  pinFocar(PS_PIN_VAZIO, 1); assert(pinFoco == PS_PIN_ZERO);
  eventoPin(SDLK_RIGHT); assert(pinFoco == PS_PIN_APAGAR);
  eventoPin(SDLK_7); eventoPin(SDLK_RETURN); assert(pin[0] == 0);  // delete
  eventoPin(SDLK_ESCAPE); assert(pinDe == -1);
  puts("perfilsel #289: 4th digit verifies, number keys, no OK key, blank cell skipped");
  preparando = 1; perfilAtivar(0, 1); assert(!concluido);
  preparando = 0; perfilAtivar(0, 2); assert(!concluido); // alvo antigo/identidade trocada
  perfilAtivar(0, 1); assert(concluido && selecionado == 1);
  repetir = 0; perfisRetentar(0, 0); assert(!repetir);
  total = 0; perfisRetentar(0, 0); assert(repetir);
  puts("perfilsel toque: PIN, bloqueio durante preparo, identidade e retentativa ok");
  total = 2; concluido = 0;
  assert(nFila() == 3);
  tileAtivar(2, 0); assert(editorOpened == 1 && editorAdd);
  botaoAtivar(1, 0); assert(editorOpened == 2 && !editorAdd && fase == 1);
  preparando = 1; tileAtivar(2, 0); botaoAtivar(1, 0); assert(editorOpened == 2);
  preparando = 0; pinDe = 0; tileAtivar(2, 0); assert(editorOpened == 2);
  pinDe = -1; total = CONTA_PERFIL_MAX;
  assert(nFila() == CONTA_PERFIL_MAX && !podeAdicionar());
  botaoAtivar(0, 0); assert(editorOpened == 2);
  botaoAtivar(1, 0); assert(editorOpened == 3 && !editorAdd);
  puts("profiles 2.1: add tile, manage footer, capacity and input guards pass");
}
