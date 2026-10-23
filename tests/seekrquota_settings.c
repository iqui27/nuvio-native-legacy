// Compile the real Settings module, retain only the Seekr paths at link time.
#define NV_SEEKR_API_KEY "package-test-key"
#define dados_gravar seekrTesteGravar
#define dados_apagar seekrTesteApagar
#include "../src/ajustes.c"
#include "nvtmp.h"
#undef dados_gravar
#undef dados_apagar
#include <assert.h>
int dados_gravar(const char *, const char *);
int dados_apagar(const char *);
static int falharChave;
int seekrTesteGravar(const char *nome, const char *texto) {
  return falharChave && !strcmp(nome, "seekr.txt") ? 0 : dados_gravar(nome, texto);
}
int seekrTesteApagar(const char *nome) {
  return falharChave && !strcmp(nome, "seekr.txt") ? 0 : dados_apagar(nome);
}
static char efetiva[96];
void seekr_definir_chave(const char *c) { snprintf(efetiva, sizeof efetiva, "%s", c); }
static int validacoes;
int seekr_validar(const char *c) { validacoes++; return !strcmp(c, efetiva); }
const char *i18n(const char *s) { return s; }
int recomenda_ativo(void) { return 0; }
int apoiador_ativo(void) { return 0; }
static SeekrUso uso = {.usadas=12, .restantes=38, .limite=50, .persistente=1, .reinicioUtc=2000073600};
static int estadoSeekr = SEEKR_PRONTO;
void seekr_uso(SeekrUso *u) { *u = uso; }
void seekr_chave_propria(int p) { uso.chavePropria = p != 0; }   // #312
int seekr_estado(void) { return estadoSeekr; }
const char *seekr_estado_rotulo(int e) {
  return e == SEEKR_ARMAZENAMENTO_INDISPONIVEL ? "Não foi possível guardar o uso do Seekr" :
         e == SEEKR_LIMITE_PROVEDOR ? "Limite temporário do Seekr" : "Miniaturas disponíveis";
}
int seekr_horario_local(long long t, char *saida, size_t n) {
  if (!t) return 0;
  snprintf(saida, n, "%s", "19/05 21:00"); return 1;
}
int main(void) {
  dados_iniciar(nvt(""));
  seekrCarregar();
  assert(!ajustes_seekr_habilitado() && !ajustes_seekr_ligado());
  assert(!efetiva[0]); // Mesmo um -D antigo nao pode restaurar a chave do pacote.
  const char *pedido = "Use sua própria chave do Seekr, gratuita em seekr.tv.";
  assert(!strcmp(seekrMascarada(), pedido));
  valor[AJ_SEEKR_LIGADO] = 0; // Preferencia antiga ligada, mas sem chave propria.
  assert(!ajustes_seekr_ligado());
  assert(uxValorAtual(AJ_SEEKR_LIGADO) == 1);
  assert(!strcmp(textoValor(AJ_SEEKR_LIGADO), "Desligado"));
  assert(!definirValorDireto(AJ_SEEKR_LIGADO, 0));
  assert(!strcmp(uxAviso, pedido));
  assert(uxAlternarDireto(AJ_SEEKR_LIGADO));
  assert(!strcmp(uxAviso, pedido) && !ajustes_seekr_ligado());
  assert(!strcmp(seekrAjuda(AJ_SEEKR_LIGADO), pedido));
  assert(!strcmp(seekrAjuda(AJ_SEEKR_CHAVE), pedido));
  assert(!strcmp(seekrAjuda(AJ_SEEKR_TESTAR), pedido));
  assert(!strcmp(textoLeitura(AJ_SEEKR_TESTAR), pedido));
  skTesteIniciar();
  assert(!skFioVivo && validacoes == 0);
  valor[AJ_SEEKR_LIGADO] = 1;
  uxAviso[0] = 0;
  assert(uxAlternarDireto(AJ_SEEKR_LIGADO));
  assert(!strcmp(uxAviso, pedido) && !ajustes_seekr_habilitado());
  valor[AJ_AVANCADAS] = 0;   /* advanced options shown (global toggle on) */
  int visiveis = 0;
  for (int i = 0; i < AJ_N_TELA; i++)
    if (TELA[i].tipo == IT_OPC && (TELA[i].op == AJ_SEEKR_CHAVE || TELA[i].op == AJ_SEEKR_TESTAR))
      visiveis += visivel(i);
  assert(visiveis == 2); // A package key never hides the personal controls.
  seekrDefinir("personal-test-key"); assert(!strcmp(efetiva, "personal-test-key"));
  valor[AJ_SEEKR_LIGADO] = 0;
  assert(ajustes_seekr_ligado() && uxValorAtual(AJ_SEEKR_LIGADO) == 0);
  assert(!strstr(seekrMascarada(), "personal-test-key"));
  falharChave = 1;
  seekrDefinir("replacement-test-key");
  assert(seekrSalvarFalhou && !strcmp(efetiva, "personal-test-key"));
  char *persistida = dados_ler("seekr.txt");
  assert(persistida && !strcmp(persistida, "personal-test-key")); free(persistida);
  assert(strstr(seekrAjuda(AJ_SEEKR_CHAVE), "chave anterior foi mantida"));
  seekrDefinir(""); assert(seekrSalvarFalhou && !strcmp(efetiva, "personal-test-key"));
  falharChave = 0;
  seekrDefinir("personal-test-key"); assert(!seekrSalvarFalhou);
  seekrCarregar(); assert(!strcmp(efetiva, "personal-test-key"));
  skTesteIniciar(); pthread_join(skFio, NULL); skFioVivo = 0;
  assert(skTesteRes == 1 && validacoes == 1 && !strcmp(skTesteChave, "personal-test-key"));
  seekrDefinir("new-personal-test-key");
  assert(!strcmp(skTesteTexto(), "OK testa")); // A previous key's result is never reused.
  seekrDefinir("personal-test-key");
  // #312: chave pessoal nao tem o teto de 50 desta TV (vale o plano no Seekr).
  assert(uso.chavePropria);
  assert(!strstr(textoLeitura(AJ_SEEKR_TESTAR), "/50"));
  assert(!strstr(seekrAjuda(AJ_SEEKR_LIGADO), "de 50 consultas"));
  assert(strstr(seekrAjuda(AJ_SEEKR_LIGADO), "limites do seu plano"));
  estadoSeekr = SEEKR_LIMITE_PROVEDOR; uso.retryUtc = 2000073600;
  assert(strstr(seekrAjuda(AJ_SEEKR_TESTAR), "Tente após"));
  // Remover a chave desliga o uso efetivo e nunca restaura a chave do pacote.
  seekrDefinir(""); assert(!efetiva[0] && !uso.chavePropria);
  assert(!dados_ler("seekr.txt") && !ajustes_seekr_ligado());
  assert(!strcmp(textoLeitura(AJ_SEEKR_TESTAR), pedido));
  assert(!strcmp(seekrAjuda(AJ_SEEKR_LIGADO), pedido));
  assert(!strcmp(seekrAjuda(AJ_SEEKR_TESTAR), pedido));
  uso.relogioAtrasado = 1;
  assert(!strcmp(seekrAjuda(AJ_SEEKR_TESTAR), pedido));
  assert(!strstr(seekrAjuda(AJ_SEEKR_TESTAR), "hora local"));
  uso.persistente = 0;
  assert(!strstr(textoLeitura(AJ_SEEKR_TESTAR), "12/50"));
  assert(!strcmp(seekrAjuda(AJ_SEEKR_TESTAR), pedido));
  assert(!strstr(seekrAjuda(AJ_SEEKR_TESTAR), "personal-test-key"));
  seekrCarregar(); assert(!efetiva[0]);
  skTesteIniciar(); assert(!skFioVivo && validacoes == 1);
  puts("seekr ajustes: PASS padrao desligado, aviso sem chave e nenhuma validacao");
  puts("seekr ajustes: PASS chave propria, mascara, persistencia, falhas de gravacao e remocao");
  return 0;
}
