// AGENDA DA RECONSULTA DE VERSAO (01/10/2026), sem rede.
//
// O defeito: a consulta ao GitHub era uma por processo, e a TV guarda o app
// suspenso por dias — o aviso de versao nova so aparecia fechando o app de
// vez. Aqui se confere a regra que decide quando consultar de novo
// (atualizacao_agenda_vence) e as guardas de quem a chama.
//
//   bash tests/atualizacao_agenda.sh
#define NV_VERSAO "1.6.6"
#include "../src/atualizacao.c"
#include "nvtmp.h"
#include <unistd.h>

static int falhas = 0;
#define CONFERE(c, ...) do { if (!(c)) { falhas++; printf("FALHA: " __VA_ARGS__); printf("\n"); } } while (0)

#define H (3600L)
#define MS(s) ((Uint32)((s) * 1000L))

int main(void) {
  const long T0 = 1790000000L;      // um time() qualquer de 2026
  const Uint32 K0 = 5000u;          // SDL_GetTicks da primeira consulta

  // A primeira sai sempre (a home ja esta de pe quando a chamam).
  CONFERE(atualizacao_agenda_vence(T0, 0, K0, 0, 0, 0, 0), "primeira consulta sai");

  // Logo depois, nao: nada de rajada.
  CONFERE(!atualizacao_agenda_vence(T0 + 60, T0, K0 + MS(60), K0, 0, 1, 0), "1 min depois nao");
  CONFERE(!atualizacao_agenda_vence(T0 + 5 * H, T0, K0 + MS(5 * H), K0, 0, 1, 0), "5 h depois ainda nao");

  // App ABERTO o tempo todo: 6 h no monotonico bastam.
  CONFERE(atualizacao_agenda_vence(T0 + 6 * H, T0, K0 + MS(6 * H), K0, 0, 1, 0), "6 h aberto: consulta");

  // TV DORMIU (o caso do dono): o monotonico mal andou, o de parede andou 3 dias.
  CONFERE(atualizacao_agenda_vence(T0 + 72 * H, T0, K0 + MS(15 * 60), K0, 0, 1, 0),
          "3 dias suspenso: consulta ao voltar");
  // ...mas nao se o monotonico diz que a ultima foi ha menos de 10 min
  // (relogio de parede saltando logo depois do boot, NTP acertando a hora).
  CONFERE(!atualizacao_agenda_vence(T0 + 72 * H, T0, K0 + MS(5 * 60), K0, 0, 1, 0),
          "salto de relogio com a ultima ha 5 min: espera");

  // Relogio para tras (hora acertada para o passado): consulta, nao trava.
  CONFERE(atualizacao_agenda_vence(T0 - 10 * H, T0, K0 + MS(20 * 60), K0, 0, 1, 0),
          "relogio para tras: consulta");

  // ACABOU DE VOLTAR do segundo plano: espera o naoAntes, depois sai.
  { Uint32 agora = K0 + MS(20 * 60);
    CONFERE(!atualizacao_agenda_vence(T0 + 72 * H, T0, agora, K0, agora + AT_ESPERA_RETOMAR_MS, 1, 0),
            "recem-retomado: espera");
    CONFERE(atualizacao_agenda_vence(T0 + 72 * H, T0, agora + AT_ESPERA_RETOMAR_MS, K0,
                                     agora + AT_ESPERA_RETOMAR_MS, 1, 0),
            "passou a espera: consulta"); }

  // SEM RESPOSTA: tenta em 30 min, nao em 6 h.
  CONFERE(!atualizacao_agenda_vence(T0 + 20 * 60, T0, K0 + MS(20 * 60), K0, 0, 1, 1), "falhou, 20 min: nao");
  CONFERE(atualizacao_agenda_vence(T0 + 31 * 60, T0, K0 + MS(31 * 60), K0, 0, 1, 1), "falhou, 31 min: tenta");

  // SDL_GetTicks dando a volta (49 dias): a subtracao sem sinal continua certa.
  { Uint32 kUlt = 0xFFFFFFFFu - MS(60);
    CONFERE(!atualizacao_agenda_vence(T0 + 120, T0, kUlt + MS(120), kUlt, 0, 1, 0), "volta do contador, 2 min: nao");
    CONFERE(atualizacao_agenda_vence(T0 + 6 * H, T0, kUlt + MS(6 * H), kUlt, 0, 1, 0), "volta do contador, 6 h: sim"); }

  // GUARDAS: nunca reconsultar com o cartao aberto ou instalando (a consulta
  // reescreveria a URL e o hash que o instalador esta usando).
  CONFERE(podeReconsultar(), "parado: pode");
  aberto = 1;  CONFERE(!podeReconsultar(), "cartao aberto: nao");  aberto = 0;
  estado = AT_INSTALANDO; CONFERE(!podeReconsultar(), "instalando: nao");
  estado = AT_PRONTO;     CONFERE(!podeReconsultar(), "instalado, esperando reinicio: nao");
  estado = AT_PARADO;
  soEncenada = 1; CONFERE(!podeReconsultar(), ".so encenada: nao"); soEncenada = 0;

  // O CARTAO AUTOMATICO VOLTA A CADA TAG NOVA. Antes `mostrado` valia o
  // processo inteiro: achou a 1.6.7, mostrou, e a 1.6.8 dias depois nunca mais.
  { char d[NVT_MAX]; nvt_set(d, sizeof d, "/nv-agenda-XXXXXX");
    if (mkdtemp(d)) setenv("NUVIO_DADOS", d, 1);
    dados_iniciar("deploy/app"); }
  mtx = SDL_CreateMutex();
  pronto = 1;
  snprintf(tagNova, sizeof tagNova, "1.6.7"); geracao = 1;
  atualizacao_mostrar_se_houver();
  CONFERE(aberto, "1.6.7 nova: cartao abre");
  fechar();                                     // grava AT_ARQ = 1.6.7
  atualizacao_mostrar_se_houver();
  CONFERE(!aberto, "mesma geracao: nao reabre");
  geracao++;                                    // consulta trouxe a mesma tag? nao sobe; simula outra
  snprintf(tagNova, sizeof tagNova, "1.6.7");
  atualizacao_mostrar_se_houver();
  CONFERE(!aberto, "1.6.7 ja vista (AT_ARQ): nao reabre");
  snprintf(tagNova, sizeof tagNova, "1.6.8"); geracao++;
  atualizacao_mostrar_se_houver();
  CONFERE(aberto, "1.6.8 chegou dias depois: cartao abre de novo");
  aberto = 0;

  // O BOTAO DOS AJUSTES: com o cartao aberto ou instalando, nao dispara.
  aberto = 1; busca = ATUALIZACAO_BUSCA_NADA;
  atualizacao_procurar_agora();
  CONFERE(atualizacao_busca() == ATUALIZACAO_BUSCA_NADA, "cartao aberto: Procurar nao dispara");
  aberto = 0;

  printf(falhas ? "atualizacao_agenda: %d falhas\n" : "atualizacao_agenda: ok\n", falhas);
  return falhas ? 1 : 0;
}
