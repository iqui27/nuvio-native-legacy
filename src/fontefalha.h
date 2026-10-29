#ifndef NV_FONTEFALHA_H
#define NV_FONTEFALHA_H

#include "fontefiltro.h"

// Limites contam links distintos, incluindo o primeiro; nunca repeticoes de
// transporte/decoder do mesmo link. A configuracao nao contem credenciais.
#define FONTEFALHA_MAX_LINKS 10
#define FONTEFALHA_MAX_ADDONS 128
#define FONTEFALHA_CHAVE 40
#define FONTEFALHA_DIRETA (-1)

typedef struct {
  char chave[FONTEFALHA_CHAVE];
  int limite;
  int aoVivo;                    // Device/profile override: skip source filters.
} FonteFalhaAddon;
typedef struct {
  int provedores, addons;
  int ordem[4], limites[4];
  int nAddons;
  FonteFalhaAddon addon[FONTEFALHA_MAX_ADDONS];
  FonteFiltroConfig filtro;
} FonteFalhaConfig;

// Chamar com o diretorio de dados ao iniciar ajustes. Setters gravam antes
// de publicar a mudanca e retornam 0 se a gravacao falhar.
void fontefalha_dir(const char *dir);
// A escolha de compartilhar pertence ao perfil, definida pelos Ajustes.
// A primeira copia privada parte de fontefalha.txt; arquivos existentes sao
// preservados. Selecionar um arquivo recarrega a configuracao sem regrava-lo.
int fontefalha_preparar_privado(const char *arquivo);
int fontefalha_arquivo(const char *arquivo);
void fontefalha_config_ler(void);
void fontefalha_config(FonteFalhaConfig *saida);
int fontefalha_ativa(void);
int fontefalha_provedores(void);
int fontefalha_addons(void);
void fontefalha_filtro(FonteFiltroConfig *saida);
int fontefalha_filtro_definir(const FonteFiltroConfig *filtro);
int fontefalha_filtro_ativo(void);
int fontefalha_definir_provedores(int sim);
int fontefalha_definir_addons(int sim);
int fontefalha_provedor_posicao(int servico);
int fontefalha_provedor_limite(int servico);
int fontefalha_provedor_mover(int servico, int delta);
int fontefalha_provedor_definir_limite(int servico, int limite);
// Identifica INSTANCIA pela base completa, sem guardar sua URL/token.
void fontefalha_addon_chave(const char *base, char *dst, unsigned n);
// Sincroniza addons instalados: preserva a ordem/limites existentes, novos
// entram ao fim. Nao grava nem remove preferencias de addons ausentes.
void fontefalha_addons_registrar(const char *const *chaves, int n);
int fontefalha_addon_posicao(const char *chave);
int fontefalha_addon_limite(const char *chave);
int fontefalha_addon_mover(const char *chave, int delta);
int fontefalha_addon_definir_limite(const char *chave, int limite);
int fontefalha_addon_ao_vivo(const char *chave);
int fontefalha_addon_definir_ao_vivo(const char *chave, int ligado);

typedef struct FonteFalhaSessao FonteFalhaSessao;
typedef struct {
  int indice, nativa, ordemAddon;
  char addon[FONTEFALHA_CHAVE];
  // Hash + arquivo para native; URL + cabecalhos para URL direta. O chamador
  // mantem a string durante a chamada; tentar() guarda uma copia propria.
  const char *identidade;
  FonteFiltroDados filtro;
} FonteFalhaCandidata;
typedef struct { int indice, servico, candidata; } FonteFalhaRota;
enum { FONTEFALHA_FIM = 0, FONTEFALHA_PROXIMA = 1, FONTEFALHA_ESPERAR = 2 };

// Snapshot imutavel por reproducao. Uma sessao pertence a UM fio; o worker
// recebe clone e sua resposta so e aceita pela geracao que o iniciou.
FonteFalhaSessao *fontefalha_sessao_nova(const FonteFalhaConfig *cfg,
                                      unsigned servicosDisponiveis, int preferido);
FonteFalhaSessao *fontefalha_sessao_clonar(const FonteFalhaSessao *sessao);
void fontefalha_sessao_liberar(FonteFalhaSessao *sessao);
void fontefalha_sessao_filtro(const FonteFalhaSessao *sessao, FonteFiltroConfig *saida);
int fontefalha_proxima(FonteFalhaSessao *sessao, const FonteFalhaCandidata *c,
                      int n, int listaCompleta, FonteFalhaRota *rota);
// Reserva um candidato distinto antes de resolver. Falha real consome a
// vaga; URL final duplicada devolve a vaga mas conserva o par como tentado.
int fontefalha_tentar(FonteFalhaSessao *sessao, const FonteFalhaCandidata *c,
                     int servico);
int fontefalha_url_registrar(FonteFalhaSessao *sessao, const char *url);
// Registra a primeira fonte manual ja resolvida; permite continuar sem
// repetir sua URL e conta esse primeiro link no servico que a resolveu.
int fontefalha_semear(FonteFalhaSessao *sessao, const FonteFalhaCandidata *c,
                     int servico, const char *url);
void fontefalha_provedor_recusado(FonteFalhaSessao *sessao, int servico);
void fontefalha_marcar_aberta(FonteFalhaSessao *sessao);
int fontefalha_tentativas(const FonteFalhaSessao *sessao);
int fontefalha_teto(const FonteFalhaSessao *sessao);

#endif
