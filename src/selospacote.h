#ifndef NV_SELOSPACOTE_H
#define NV_SELOSPACOTE_H
// PACOTES DE SELOS DO NUVIO (o "Stream badge rules" do app oficial): um pacote
// e {sourceUrl, isActive, filters[], groups[]}; cada filtro e um regex (JS) que
// casa com o nome do arquivo/titulo/descricao da fonte e uma imagem (ou so uma
// pilula de texto com cores). Vem de duas partes, juntadas e limitadas a 3:
//   1. a CONTA: features.stream_badge_settings.stream_badge_rules do blob de
//      ajustes do perfil (um JSON dentro de string);
//   2. os adicionados NESTA TV por URL (selos-p<N>-<k>.json, por perfil).
// A escolha de qual vale (ou "Do Nuvio" = o pacote embutido) e por perfil.
#include <stddef.h>

#define SELOS_MAX_PACOTES 3
#define SELOS_MAX_CASADOS 16

// PACOTES EMBUTIDOS (dono, 05/10/2026: "use o primeiro como o padrao e o
// segundo como a versao colorida"): os dois pacotes do Xperience em
// deploy/app/art/selos (tools/selos-xperience.py). "Do Nuvio" (selospacote_ativo
// == -1) passa a ser o pacote PADRAO deles; com selospacote_colorido(1) vale o
// COLORIDO, e onde ele nao tem selo (streaming, edicoes, FLAC...) o padrao
// cobre. Sem a pasta, selospacote_casar devolve 0 e quem chama usa a deteccao
// antiga (badges.c).
//
// UMA CHAVE para as imagens: com SELOS_IMAGENS_NA_REDE 0 (o que vale) saem do
// pacote da TV, sem depender do CDN do Xperience; com 1 sao baixadas do
// imageURL de cada filtro (e a pasta selos/ pode sair do pacote do app).
#ifndef SELOS_IMAGENS_NA_REDE
#define SELOS_IMAGENS_NA_REDE 0
#endif
// `dir` = a pasta da arte (a mesma de badges_carregar). Barata; pode repetir.
void selospacote_dir_embutidos(const char *dir);
// 1 liga o pacote colorido (Ajustes > Selos coloridos). Sobe a versao se mudou.
void selospacote_colorido(int ligado);
// 1 se os embutidos carregaram (a pasta existe e tem filtros).
int  selospacote_embutidos_ok(void);

// Que arte e a do filtro (quem desenha decide a tinta):
//   SELO_ARTE_PACOTE  pacote do usuario: a imagem sai como veio
//   SELO_ARTE_BRANCA  embutido, padrao: branca, a forma no alfa — tinge
//   SELO_ARTE_COR     embutido, colorido: tem cor propria, vai numa peca escura
typedef enum { SELO_ARTE_PACOTE = 0, SELO_ARTE_BRANCA, SELO_ARTE_COR } SeloArte;

typedef struct {
  const char *nome;        // texto da pilula quando nao ha imagem
  const char *imagem;      // URL (ou caminho) da imagem; "" = sem
  int   temTag, temTexto, temBorda;   // 0 = o pacote nao define
  float tag[4], texto[4], borda[4];   // RGBA 0..1
  int   arte;              // SeloArte
  int   resolucao;         // 1 = do grupo de resolucao (a folha de fontes a tira da fileira)
} SeloFiltro;

typedef enum {
  SELOS_OK = 0,
  SELOS_ERR_JSON,      // nao e JSON, ou nao tem a forma de um pacote
  SELOS_ERR_VAZIO,     // JSON de pacote, mas nenhum filtro valido
  SELOS_ERR_LIMITE,    // ja ha 3 pacotes
  SELOS_ERR_DUPLICADO  // a conta ja tem um pacote com esta URL
} SelosResultado;

// Recarrega do disco se o perfil mudou. Chamada sozinha pelas funcoes abaixo.
void selospacote_iniciar(void);
void selospacote_recarregar(void); // account device snapshot restored on disk

// Blob de ajustes da CONTA (o JSON inteiro). Extrai os pacotes da conta, guarda
// no disco e sinaliza mudanca. Sem a chave no blob, a conta fica sem pacotes.
void selospacote_conta_do_blob(const char *blob);

int  selospacote_n(void);                       // pacotes ao todo (0..3)
const char *selospacote_nome(int i);            // primeiro grupo, ou o host da URL
const char *selospacote_url(int i);
int  selospacote_da_tv(int i);                  // 1 = adicionado nesta TV (removivel)
int  selospacote_n_filtros(int i);              // filtros validos e ativos
int  selospacote_ativo(void);                   // -1 = "Do Nuvio" (o embutido)
void selospacote_escolher(int i);               // -1 ou 0..n-1, guarda no perfil
unsigned selospacote_versao(void);              // sobe a cada mudanca de lista/escolha

// Acrescenta o(s) pacote(s) do texto baixado de `origem` (a URL). Aceita as 3
// formas ({imports}, {streamBadgeRules:{imports}}, {filters,groups}). Na
// primeira parte que entrar, escolhe-o. Devolve SelosResultado.
int  selospacote_adicionar(const char *json, const char *origem);
// Remove o pacote `i` se for da TV; 0 se nao e (a conta remove no web).
int  selospacote_remover(int i);

// Casa a fonte com o pacote ATIVO: ids dos filtros casados, na ordem do
// pacote, sem repetir imagem/nome (como o web). `campos` = arquivo, nome,
// titulo, descricao, addon... (NULL ou "" pulados). So chamar quando
// selospacote_ativo() >= 0, e UMA VEZ por fonte (cache no chamador).
int  selospacote_casar(const char *const *campos, int nc, unsigned short *ids, int max);
// O filtro `id` do pacote ativo; NULL se o id nao vale mais.
const SeloFiltro *selospacote_filtro(unsigned short id);
#endif
