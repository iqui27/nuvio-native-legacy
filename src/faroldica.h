// CARTAO DE DICA DO FAROL (P2 — PLANO-GOSTO 4.2, issue 48).
//
// P2 aparece como coluna 0 da primeira fileira do Farol na Home (na medida do
// cartaz, vidro --ilha, 2 linhas e o ponto de luz de 10 px no acento).
//
// REGRAS DURAS (aceite + ALMA):
//   - no maximo 1 dica por dia por perfil na Home (cota diaria persistida em
//     disco em farol-dica-dia.txt, por perfil — dispensar ou consumir a dica
//     de hoje NAO libera uma segunda dica no mesmo dia);
//   - dispensada nao volta (avisodisp, chave "faroldica:<id>", por perfil);
//   - cada acao.tipo (abrir | tocar | comparar | escolha | salvar | nada) faz
//     o que diz — enum fechado, nunca URL; comparar (#49) e escolha (#50)
//     ficam indisponiveis enquanto nao houver rota registrada (sem prometer
//     acao nem gastar a cota);
//   - cartao com acao == FP_ACAO_NADA NAO entra na ordem de foco (faroldica_
//     focavel() == 0);
//   - nunca no player, nunca como pilula da Ilha;
//   - sem custo por quadro: faroldica_geracao() e faroldica_dia_atual() nao
//     usam trava nem copia; a Home mantem copia local e so consulta
//     faroldica_atual() quando a geracao ou o dia mudam;
//   - nenhum titulo, nome de perfil ou gosto em log.
#ifndef NV_FAROLDICA_H
#define NV_FAROLDICA_H

#include "farolpeca.h"   // FP_ACAO_*, FAROL_ID_TAM, FAROL_REF_TAM, FAROL_ICONE_TAM
#include "farolcena.h"   // FarolDicaFato, FC_DICA_*
#include <stddef.h>
#include <stdint.h>

#define FAROLDICA_ARQ "farol-dica-dia.txt"

// Estado pronto da dica ativa: ja validado por farolpeca_ler e traduzido por
// faroltexto.h + i18n(). Estrutura plana (sem ponteiros, sem malloc) para a
// Home manter copia local cacheada por faroldica_geracao().
typedef struct {
  int     ativa;                         // 1 = mostrar como coluna 0 da fileira Farol
  int     focavel;                       // 1 = entra na ordem de foco (acao != FP_ACAO_NADA)
  int     perfil;                        // perfil dono desta dica (perfis_ativo())
  int     dia;                           // dia (dias desde 1/1/2026) da cota diaria
  uint8_t acao;                          // FP_ACAO_* (enum fechado)
  char    id[FAROL_ID_TAM];              // id estavel ("d_..."), chave em avisodisp
  char    l1[192];                       // linha 1 ja renderizada/localizada
  char    l2[192];                       // linha 2 opcional ("" = ausente)
  char    alvo[FAROL_REF_TAM];           // "tt..." resolvido ("" para escolha/nada)
  char    alvoTipo[8];                   // "movie" | "series" (ou "")
  char    alvoTitulo[160];               // titulo resolvido do alvo
  char    alvoPoster[512];               // capa opcional (ex.: diario-nomes)
  int     alvoTemporada;                 // > 0 para serie com temporada conhecida
  int     alvoEpisodio;                  // > 0 para serie com episodio conhecido
  char    icone[FAROL_ICONE_TAM];        // nome do icone no atlas (ex.: "aj_farol")
} FarolDica;

// Resultado de faroldica_executar() — cada acao.tipo executa o efeito proprio.
typedef enum {
  FD_EXEC_NADA = 0,      // FP_ACAO_NADA ou sem dica ativa/valida
  FD_EXEC_ABRIR,         // abrir pagina do titulo (catIdx >= 0)
  FD_EXEC_TOCAR,         // tocar o titulo (catIdx >= 0)
  FD_EXEC_COMPARAR,      // abrir Comparar (P4) com alvo
  FD_EXEC_ESCOLHA,       // abrir Assistente de escolha (P5)
  FD_EXEC_SALVAR         // salvo na lista local (salvos_definir) + catalogo
} FarolDicaExecTipo;

typedef struct {
  FarolDicaExecTipo tipo;
  int  ok;                               // 1 = acao executada com sucesso
  int  catIdx;                           // indice no catalogo (>= 0) para abrir/tocar/salvar
  char imdb[FAROL_REF_TAM];              // alvo resolvido ("tt...")
} FarolDicaExec;

// Leitura atomica, sem trava: sobe a cada adocao, dispensa ou invalidacao.
// A Home checa apenas isto + faroldica_dia_atual() por quadro.
unsigned faroldica_geracao(void);
// Dia corrente (dias desde 1/1/2026) pelo relogio real ou injetado, sem trava
// nem copia. Permite virar a validade diaria na Home sem mudar a cena.
int faroldica_dia_atual(void);
// 1 se a acao pode ser prometida/executada agora (abrir/tocar/salvar/nada:
// sempre 1; comparar/escolha: somente quando ha rota registrada de #49/#50).
int faroldica_acao_disponivel(uint8_t acao);

// Copia a dica ativa para `out` se ela vale para o perfil ativo e para o dia
// atual (e nao foi dispensada). Devolve 1 se ativa, 0 caso contrario (e zera
// `out`). Chamar somente quando faroldica_geracao() mudou.
int faroldica_atual(FarolDica *out);

// 1 = o cartao entra na ordem de foco; 0 = acao == FP_ACAO_NADA (ou inativo).
int faroldica_focavel(const FarolDica *d);

// Prepara a dica deterministica do dia para o perfil ativo (serie parada ha
// semanas, faltam episodios, salvo antigo, curto para hoje, perguntas) passando
// pelo compositor farolcena_home + validador farolpeca_ler. Respeita os portoes
// ("Recomendar nesta TV", modo seguro), avisodisp e a cota de 1/dia/perfil.
// Barato quando ja avaliou (perfil, dia) — nao repete trabalho no mesmo dia.
// Devolve 1 se ha dica ativa ao final, 0 caso contrario.
int faroldica_preparar(void);

// Submete uma lista de fatos elegiveis (por ordem de prioridade de farolcena)
// para o perfil ativo no dia atual: filtra dispensadas, compoe via
// farolcena_home, valida via farolpeca_ler, aplica a cota 1/dia/perfil e adota.
// Devolve 1 se adotou uma dica, 0 caso contrario.
int faroldica_adotar_fatos(const FarolDicaFato *fatos, int nFatos, int nivelFrio);

// Adota a peca P2 (`dica`) de uma cena FarolCena v1 em JSON (ex.: vinda das
// regras ou do servidor/LLM da issue 52). Valida com farolpeca_ler, filtra
// avisodisp e aplica a cota 1/dia/perfil. `candImdb`/`candTipo`/`candTitulo`
// podem ser NULL quando nCand == 0 ou todas as refs sao "tt...".
// Devolve 1 se adotou, 0 caso contrario.
int faroldica_adotar_cena_json(const char *json, int nCand,
                               const char *const *candImdb,
                               const char *const *candTipo,
                               const char *const *candTitulo);

// Dispensa a dica ativa (grava em avisodisp para o perfil ativo), limpa o
// cartao imediatamente mantendo a cota do dia gasta (nenhuma 2a dica hoje) e
// sobe a geracao. Devolve 1 se havia dica ativa para dispensar.
int faroldica_dispensar(void);
// Dispensa um id especifico em avisodisp; se for a dica ativa, remove-a.
int faroldica_dispensar_id(const char *id);
// 1 se `id` ja foi dispensada para o perfil ativo (avisodisp).
int faroldica_dispensada(const char *id);

// Executa a acao da dica `d`:
//   abrir    -> resolve alvo no catalogo (cat_acrescentar se fora) -> FD_EXEC_ABRIR
//   tocar    -> resolve alvo no catalogo (cat_acrescentar se fora) -> FD_EXEC_TOCAR
//   salvar   -> salvos_definir(&ci, 1) + cat_definir_na_lista_imdb -> FD_EXEC_SALVAR
//   comparar -> dispara callback e grava pedido pendente           -> FD_EXEC_COMPARAR
//   escolha  -> dispara callback e grava pedido pendente           -> FD_EXEC_ESCOLHA
//   nada     -> nao faz nada (ok = 0)                              -> FD_EXEC_NADA
FarolDicaExec faroldica_executar(const FarolDica *d);

// Ganchos e pedidos pendentes para telas futuras/externas (P4 comparar, P5 escolha).
void faroldica_ao_comparar(void (*fn)(const char *imdb));
void faroldica_ao_escolha(void (*fn)(void));
int  faroldica_pediu_comparar(char *imdb, size_t cap);
int  faroldica_pediu_escolha(void);

// Rotulo curto localizado para a acao ("" para FP_ACAO_NADA).
const char *faroldica_rotulo_acao(uint8_t acao);

// Troca de perfil/sessao: limpa o estado em RAM (disco de cota e avisodisp
// permanecem). Sobe a geracao.
void faroldica_esquecer(void);
// Logout / reset completo: limpa cota diaria em RAM e no disco.
void faroldica_esquecer_disco(void);

// Relogio injetavel para testes (epoch em segundos; NULL = time(NULL)).
void faroldica_teste_relogio(long long (*s)(void));
// Reinicia estado em RAM (callbacks, pedidos, cota carregada) para testes.
void faroldica_teste_reiniciar(void);

#ifndef NV_FAROLDICA_PURO
#include "gfx.h"
// Desenha o cartao P2 na caixa `r` (mesma medida do cartaz da fileira), com
// vidro --ilha, ponto de luz de 10 px no acento, icone, ate 2 linhas e rodape
// de acao (omitido em FP_ACAO_NADA). Sem trava nem copia interna; respeita
// `reduzido` (ajustes_animacoes_reduzidas()) e modo solido (!ajustes_vidro()).
void faroldica_desenhar(const FarolDica *d, GfxRect r, float raio,
                        float foco, float alpha, int reduzido, int salvo);
#endif

#endif // NV_FAROLDICA_H
