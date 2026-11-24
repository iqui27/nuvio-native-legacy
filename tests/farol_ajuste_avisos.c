// Teste dirigido da issue #27 do Farol: ajuste "Avisos do Farol" em
// Ajustes > Sobre e ajuda ligado a politica de silencio (farolilha.c + ilhasinais.c)
// e ao fluxo sob demanda do Resolver.
#ifndef AJUSTES_TESTE
#define AJUSTES_TESTE 1
#endif
#ifndef RESOLVER_TESTE
#define RESOLVER_TESTE 1
#endif
#include "../src/ajustes.c"
#include "../src/farol.h"
#include "../src/farolanel.h"
#include "../src/farolilha.h"
#include "../src/farolregra.h"
#include "../src/faroltexto.h"
#include "../src/ilha.h"
#include "../src/ilhasinais.h"
#include "../src/resolver.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int buscarOp(const char *consulta, AjusteBuscaResultado *out, int cap) {
  int n = ajustes_buscar(consulta, out, cap);
  for (int i = 0; i < n; i++)
    if (out[i].op == AJ_FAROL_AVISOS) return i;
  return -1;
}

int main(int argc, char **argv) {
  char dir[512];
  assert(argc == 2 && strlen(argv[1]) < sizeof dirAjustes);
  snprintf(dir, sizeof dir, "%s", argv[1]);
  setenv("NUVIO_DADOS", dir, 1);
  dados_iniciar(dir);

  // 1. Padrao (1 = Ligado), tabela de valores, escopo e posicao na tela.
  ajustes_dir(dir);
  ilhasinais_iniciar();
  assert(AJ_FAROL_AVISOS >= 0 && AJ_FAROL_AVISOS < AJ_N);
  assert(strcmp(CHAVE[AJ_FAROL_AVISOS], "farolAvisosLocal") == 0);
  assert(OPCOES[AJ_FAROL_AVISOS].n == 2);
  assert(strcmp(OPCOES[AJ_FAROL_AVISOS].valores[0], FT_ACAO_SO_QUANDO_ABRIR) == 0);
  assert(strcmp(OPCOES[AJ_FAROL_AVISOS].valores[1], FT_ACAO_LIGADO) == 0);
  assert(valorPadrao[AJ_FAROL_AVISOS] == 1);
  assert(uxTemPadrao(AJ_FAROL_AVISOS) && uxValorPadrao(AJ_FAROL_AVISOS) == 1);
  assert(ajustes_farol_avisos() == 1);
  assert(farolilha_ativo() == 1);
  assert(somenteDesteAparelho(AJ_FAROL_AVISOS));
  assert(soNaTv(AJ_FAROL_AVISOS));
  assert(ajustes_escopo(AJ_FAROL_AVISOS) == ESC_TV);
  assert(familiaPreviaOpcao(AJ_FAROL_AVISOS) == AJPV_ABOUT);
  assert(ajCenaSecao(AJ_FAROL_AVISOS) == AJC_SOB_GUIA);
  assert(strcmp(ajVisualIcone(AJ_FAROL_AVISOS), "aj_farol") == 0);
  {
    int vezes = 0;
    const char *sec = "";
    for (int k = 0; k < AJ_N_TELA; k++) {
      if (TELA[k].tipo == IT_SEC) sec = TELA[k].titulo;
      if (TELA[k].tipo == IT_OPC && TELA[k].op == AJ_FAROL_AVISOS) {
        vezes++;
        assert(strcmp(sec, "Sobre e ajuda") == 0);
      }
    }
    assert(vezes == 1);
  }

  // 2. Busca de Ajustes: palavras-chave em PT e EN encontram "Avisos do Farol".
  {
    AjusteBuscaResultado res[AJ_N];
    static const char *const termos[] = {
      "farol", "avisos", "notificacoes", "silenciar", "ajuda", "beacon", "alerts"
    };
    valor[AJ_IDIOMA] = IDIOMA_PT + 1;
    for (size_t i = 0; i < sizeof termos / sizeof *termos; i++) {
      int idx = buscarOp(termos[i], res, AJ_N);
      assert(idx >= 0);
      assert(strcmp(res[idx].titulo, FT_AVISOS_FAROL) == 0);
      assert(strstr(res[idx].caminho, "Sobre e ajuda") != NULL);
      assert(strstr(res[idx].ajuda, FT_AVISOS_FAROL_DESC) != NULL);
    }
  }

  // 3. Troca para 0 ("So quando eu abrir"), persistencia e recarga.
  assert(definirValorDireto(AJ_FAROL_AVISOS, 0) == 1);
  assert(ajustes_farol_avisos() == 0);
  assert(farolilha_ativo() == 0);
  {
    char *txt = dados_ler("ajustes.txt");
    assert(txt && strstr(txt, "farolAvisosLocal 0\n") != NULL);
    free(txt);
  }

  // Simula reinicio: zera em RAM e recarrega do ajustes.txt.
  valor[AJ_FAROL_AVISOS] = 1;
  farolilha_definir_ativo(1);
  ajustes_dir(dir);
  assert(ajustes_farol_avisos() == 0);
  assert(farolilha_ativo() == 0);

  // Restaura ao padrao (1 = Ligado).
  assert(definirValorDireto(AJ_FAROL_AVISOS, uxValorPadrao(AJ_FAROL_AVISOS)) == 1);
  assert(ajustes_farol_avisos() == 1);
  assert(farolilha_ativo() == 1);
  {
    char *txt = dados_ler("ajustes.txt");
    assert(txt && strstr(txt, "farolAvisosLocal 1\n") != NULL);
    free(txt);
  }

  // 4. Politica de silencio na ilha + conserto calado (farolilha + ilhasinais).
  Uint32 agora = 1000u;
  FarolFatos ff;
  farolregra_fatos_iniciar(&ff);
  ff.falhasSeguidas = 2;
  farol_teste_fatos(&ff);

  // Primeira volta para inicializar farolConsSeq na ilha.
  ajustes_farol_avisos_definir(0);
  assert(ajustes_farol_avisos() == 0);
  assert(farolilha_ativo() == 0);
  ilhasinais_passo(agora);
  assert(!ilha_tem("farol"));
  assert(!ilha_tem("farol-conserto"));

  // Mesmo com 2 falhas seguidas e um conserto automatico no anel, ambos calam.
  {
    FarolIlhaFatos fi;
    memset(&fi, 0, sizeof fi);
    fi.falhasSeguidas = 2;
    fi.titulo = "tt27";
    fi.codigo = FAROL_C_FONTE_RECUSOU;
    assert(farolilha_proativo(&fi) == 0);
    assert(farolilha_conserto(FAROL_A_PROXIMA_FONTE, 1) == NULL);
  }
  farol_registrar_consertou(FAROL_A_PROXIMA_FONTE);
  agora += 2500u;
  ilhasinais_passo(agora);
  assert(!ilha_tem("farol"));
  assert(!ilha_tem("farol-conserto"));

  // 5. Fluxo sob demanda do Resolver continua abrindo normalmente com 0.
  resolver_teste_esquecer();
  resolver_abrir();
  assert(resolver_aberto() == 1);
  assert(resolver_fase() == RESOLVER_FASE_SINTOMA);
  resolver_abrir_sintoma(RES_S_NAO_COMECA);
  assert(resolver_aberto() == 1);
  assert(resolver_fase() == RESOLVER_FASE_TESTES);
  {
    SDL_Event esc;
    memset(&esc, 0, sizeof esc);
    esc.type = SDL_KEYDOWN;
    esc.key.keysym.sym = SDLK_ESCAPE;
    resolver_evento(&esc);
    resolver_evento(&esc);
    assert(!resolver_aberto());
  }

  // 6. Voltar para 1 ("Ligado") reativa imediatamente proativo e conserto calado.
  ajustes_farol_avisos_definir(1);
  assert(ajustes_farol_avisos() == 1);
  assert(farolilha_ativo() == 1);
  farol_registrar_consertou(FAROL_A_RECONECTAR_REDE);
  agora += 5000u;
  ilhasinais_passo(agora);
  assert(ilha_tem("farol"));
  assert(ilha_tem("farol-conserto"));

  // 7. Desligar de novo retira imediatamente sem precisar reiniciar.
  ajustes_farol_avisos_definir(0);
  agora += 500u;
  ilhasinais_passo(agora);
  assert(!ilha_tem("farol"));
  assert(!ilha_tem("farol-conserto"));

  farol_teste_fatos(NULL);
  puts("farol_ajuste_avisos: tudo ok");
  return 0;
}
