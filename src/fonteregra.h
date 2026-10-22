// QUAIS FONTES O AUTOMATICO PODE TOCAR (#202) — paridade com "Auto-play
// streams" do Nuvio oficial (NuvioTV: StreamAutoPlaySelector.kt,
// StreamAutoPlayPolicy.kt, PlayerSettingsDataStore.kt).
//
// O pedido (em espanhol): "quero que o automatico va direto para os meus
// add-ons em espanhol; so se eles nao tiverem fonte, use o resto". Aqui mora
// so a REGRA, sem rede, sem SDL e sem a lista de streams:
//
//   - ADD-ONS PERMITIDOS e PLUGINS PERMITIDOS: duas listas de NOMES (o nome de
//     exibicao, Stream.provedor), independentes, como no oficial. Lista vazia =
//     todos (o "All installed addons" do oficial).
//   - ESCOPO ("Auto-play Source Scope"): todas as fontes, so add-ons
//     instalados, ou so plugins ligados. O indice e o de ajustes (AJ_FONTE_ESCOPO).
//   - REGEX: casada, SEM caixa, contra "addon nome titulo descricao url" (o
//     mesmo texto do oficial). Exigir = filtro (o REGEX_MATCH do oficial);
//     Preferir = as que casam vem antes, as outras continuam na fila.
//     Regex invalida e IGNORADA (a linha de Ajustes diz isso).
//   - "Usar os outros se nao houver" (nosso, o oficial nao tem): sem ele,
//     fonte fora do permitido nunca toca (o oficial abre a lista de fontes).
//     Com ele, ela vai para o FIM da fila, depois de toda permitida. Vale so
//     para as listas de permitidos: fonte FORA DO ESCOPO e sempre -1, com ou
//     sem "os outros" (bloqueador 2.0.3: plugin tocou com "so add-ons").
//
// O resultado por fonte e um GRUPO: -1 = nunca no automatico; 0..3 = ordem
// da fila (0 permitida e casa a regex, 1 permitida sem casar, 2 de fora e
// casa, 3 de fora sem casar). Sem nada configurado tudo e grupo 0 e a escolha
// e exatamente a de antes. fonteauto.h monta a fila grupo a grupo.
//
// As listas e a regex sao POR PERFIL e vao para a conta com as chaves do
// oficial (stream_auto_play_regex, stream_auto_play_selected_addons,
// stream_auto_play_selected_plugins), lidas e costuradas so quando o blob ja
// as tem — a mesma regra de ajustes_mesclar_blob.
#ifndef NV_FONTEREGRA_H
#define NV_FONTEREGRA_H
#include <stddef.h>

enum { FR_ESCOPO_TODAS = 0, FR_ESCOPO_ADDONS = 1, FR_ESCOPO_PLUGINS = 2 };
enum { FR_REGEX_DESLIGADA = 0, FR_REGEX_EXIGIR = 1, FR_REGEX_PREFERIR = 2 };
#define FR_NOMES_MAX 48
#define FR_NOME_MAX  96
#define FR_REGEX_MAX 500          // o oficial (web) corta em 500
#define FR_ORDEM_MAX 96           // nomes na ordem dos add-ons (add-ons e plugins juntos)
#define FR_ORDEM_SEM 98           // posicao de quem nao esta na ordem: depois de todos
// "Usar a ordem" (AJ_FONTE_ORDEM_USO): o indice e o gravado em ajustes.txt.
enum { FR_ORDEM_NAO = 0, FR_ORDEM_DESEMPATE = 1, FR_ORDEM_ESTRITA = 2 };

typedef struct { int escopo, regexModo, usarOutros; } FonteRegraCfg;

// --- estado (com trava: o fio da verificacao le, a tela escreve) ------------
// Le fonteregra.txt (o estado em uso nesta TV). Chamar depois de dados_iniciar.
void fonteregra_carregar(void);
unsigned fonteregra_versao(void);         // sobe a cada mudanca
void fonteregra_regex(char *dst, size_t tam);
// Guarda o padrao (aparado, ate FR_REGEX_MAX). Devolve fonteregra_regex_estado().
int  fonteregra_definir_regex(const char *padrao);
// 1 = configurada e valida; 0 = vazia (ou sem letra/digito); -1 = invalida.
int  fonteregra_regex_estado(void);
// Listas: plugin 0 = add-ons, 1 = plugins.
int  fonteregra_n(int plugin);
int  fonteregra_nome(int plugin, int k, char *dst, size_t tam);
int  fonteregra_contem(int plugin, const char *nome);   // na lista (sem caixa)
int  fonteregra_alternar(int plugin, const char *nome); // estado novo (1 = na lista)
void fonteregra_limpar(int plugin);

// --- a ordem dos add-ons ----------------------------------------------------
// Uma fila de NOMES (add-ons e plugins juntos), so local por perfil: o oficial
// nao tem chave equivalente (conferido nas chaves stream_auto_play_*).
int  fonteregra_ordem_n(void);
int  fonteregra_ordem_nome(int k, char *dst, size_t tam);
void fonteregra_ordem_definir(const char *const *nomes, int n);
// Posicao do nome na ordem (0 = primeiro); FR_ORDEM_SEM se nao esta nela.
int  fonteregra_ordem_rank(const char *nome);
// "A > B > C" ("" sem ordem), com o sinal de ">" tipografico.
void fonteregra_ordem_texto(char *dst, size_t tam);

// --- a regra ----------------------------------------------------------------
// Grupo da fonte (ver o topo). `nome` = Stream.provedor; `texto` = o que a
// regex le. Pura quanto a cfg; le as listas e a regex compilada.
int  fonteregra_grupo(const FonteRegraCfg *c, const char *nome, int plugin, const char *texto);
// O MELHOR grupo que uma fonte AINDA SEM RESPOSTA deste addon/plugin pode
// trazer: 0 se ele e permitido, 2 se so entra como "os outros", -1 se nunca.
int  fonteregra_grupo_pendente(const FonteRegraCfg *c, const char *nome, int plugin);
// 0 = a fonte (add-on ou plugin) esta fora do escopo e NUNCA toca sozinha,
// nem como a fonte lembrada do titulo (fontepref).
int  fonteregra_no_escopo(const FonteRegraCfg *c, int plugin);
// Alguma regra mexe na fila? (log e "por que" da escolha)
int  fonteregra_ativa(const FonteRegraCfg *c);

// --- regex, sem estado (testes) ---------------------------------------------
// O padrao do oficial (sintaxe Java) em POSIX ERE: \d \s \w, \b, (?:, (?i),
// quantificadores preguicosos. Os lookaheads negativos (?!...(A|B)) saem do
// padrao e as palavras de dentro vao para `excl` ("A|B"), como o oficial faz.
// 1 = traduziu; 0 = nao coube.
int  fonteregra_posix(const char *java, char *posix, size_t tam, char *excl, size_t tamExcl);
// -1 = padrao invalido ou nao configurado; 0 = nao casa; 1 = casa.
int  fonteregra_regex_testar(const char *padrao, const char *texto);

// Modelos prontos de "Modelo de regex". 0 = Personalizado (sem padrao).
int  fonteregra_modelos(void);
const char *fonteregra_modelo(int i);          // o padrao do modelo i (>= 1)
int  fonteregra_modelo_atual(void);            // o modelo igual ao padrao em uso; 0 = nenhum

// --- perfil e conta ---------------------------------------------------------
void fonteregra_perfil_guardar(int perfil);
int  fonteregra_perfil_restaurar(int perfil);
void fonteregra_perfil_esquecer(void);
// Le do blob da conta (so as chaves que existem). Devolve quantas mudaram.
int  fonteregra_do_blob(const char *json);
// Costura os valores locais nas chaves do oficial QUE JA EXISTEM em `base`, no
// mesmo tipo (texto/array). *saida (malloc) so quando algo mudou; devolve
// quantas chaves entraram.
int  fonteregra_mesclar(const char *base, char **saida);

#endif
