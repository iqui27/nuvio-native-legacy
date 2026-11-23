// A BIBLIOTECA DO SIMKL NO DISCO, uma copia por perfil.
//
// O guia de sync do Simkl (api.simkl.org/guides/sync) quer UMA leitura cheia
// por usuario e, dali em diante, so deltas com date_from. Isso exige guardar a
// biblioteca entre arranques: so na memoria, todo arranque seria a "primeira"
// leitura. Este modulo so guarda e le; quem baixa e funde e simkl.c.
//
// O arquivo pertence a UM vinculo: leva a assinatura do token (a mesma ideia do
// jornal de scrobble em simkl.c, simkl-vistos-<assinatura>.txt), nunca o token.
// Outro vinculo no mesmo perfil — outra conta do Simkl, ou a mesma vinculada de
// novo — nao reaproveita a copia: ela e apagada e a leitura cheia recomeca.
// Arquivo ilegivel, de outra versao ou cortado tambem = leitura cheia.
//
// O conteudo e re-obtivel, entao grava por dados_gravar_leve: no Tizen .wgt a
// descarga pode esperar 15 s. Perder a ultima gravacao custa um delta maior no
// proximo arranque (as atividades guardadas sao as da copia), nunca um erro.
#ifndef NV_SIMKLBIB_H
#define NV_SIMKLBIB_H

// Aqui, e nao no .c, porque simklauth.c apaga a copia junto com o vinculo
// (desvincular, sair, 401) usando so o nome — sem depender deste modulo.
#define SIMKLBIB_ARQ_FMT "simkl-biblioteca-p%d.txt"

typedef struct {
  // Corpo de /sync/activities que a biblioteca reflete. O "all" dele e o
  // date_from do proximo delta; os demais carimbos dizem o que mudou.
  char *atividades;
  // {"shows":[...],"movies":[...],"anime":[...]}, a forma de /sync/all-items,
  // para os leitores de simkl.c lerem a copia como leem a resposta da rede.
  char *biblioteca;
} SimklBib;

// Assinatura estavel do token (FNV-1a 64). 0 = sem token.
unsigned long long simklbib_conta(const char *token);

// Le a copia do perfil. 1 = `b` preenchido (simklbib_liberar depois). 0 = nao
// ha copia utilizavel: ausente, ilegivel, de outra versao ou de outro vinculo;
// nos tres ultimos casos o arquivo e apagado. Em 0, `b` fica zerado.
int simklbib_ler(int perfil, unsigned long long conta, SimklBib *b);

// Grava a copia do perfil, atomica (temporario + rename em dados.c). 1 se deu
// certo. Nao confere sessao nem vinculo: quem chama grava sob a trava de pedido
// de simkl.c (pedidoTravar), que ja recusa perfil trocado e Simkl esquecido.
int simklbib_gravar(int perfil, unsigned long long conta, const SimklBib *b);

void simklbib_liberar(SimklBib *b);
void simklbib_apagar(int perfil);

#endif
