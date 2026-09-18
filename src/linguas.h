// Idiomas de MIDIA: nome legivel, comparacao tolerante a variante, e a
// preferencia de audio/legenda da pessoa.
//
// POR QUE ISTO EXISTE. O port nasceu com os idiomas CRAVADOS: addons.c tinha
// duas listas ("pob","pt-br",... e "eng","en",...) e um comentario dizendo "o
// usuario pediu explicitamente estes dois grupos". Toda legenda que nao fosse
// portugues ou ingles era DESCARTADA em silencio. Isso funcionava para uma
// casa; para quem instalar o pacote e falar espanhol, significa abrir o player
// e nao encontrar legenda nenhuma, sem explicacao.
//
// DE ONDE VEM A PREFERENCIA, nesta ordem:
//   1. Ajustes desta TV, quando a pessoa escolheu explicitamente.
//   2. A CONTA. O blob de ajustes traz, sob `player_settings`:
//        subtitle_preferred_language, subtitle_secondary_language,
//        preferred_audio_language, secondary_preferred_audio_language
//      MEDIDO no app web (profileSettingsSyncService.js:1066). Os valores sao
//      codigos ISO minusculos ("en", "pt"), mais as sentinelas "none"/"off"
//      para legenda e "DEVICE"/"DEFAULT"/"ORIGINAL" para audio.
//   3. NADA. E aqui esta a regra que importa: sem preferencia, NAO SE FILTRA.
//      Mostrar tudo e a resposta honesta para "nao sei o que voce quer"; um
//      filtro chutado esconde a legenda que a pessoa procurava e ela nao tem
//      como saber que houve filtro.
#ifndef NV_LINGUAS_H
#define NV_LINGUAS_H

// Nome para a tela ("pob" -> "Português (BR)"). Sem correspondencia, devolve o
// codigo em MAIUSCULAS, que ao menos identifica.
const char *ling_nome(const char *codigo);

// O codigo casa com a preferencia? Tolera variante e as duas familias ISO:
// pedir "pt" aceita "por", "pob", "pt-BR", "ptb". Preferencia vazia casa com
// TUDO — e o modo sem filtro.
int ling_casa(const char *codigo, const char *pref);

// Preferencias em vigor. Devolvem "" quando nao ha preferencia (sem filtro) e
// "none" quando a pessoa pediu explicitamente NENHUMA legenda.
const char *ling_legenda(void);
const char *ling_legenda2(void);
const char *ling_audio(void);

// Vindas da CONTA (blob de ajustes). Nao sobrescrevem escolha local.
void ling_conta_legenda(const char *v);
void ling_conta_legenda2(const char *v);
void ling_conta_audio(const char *v);

// Vindas dos AJUSTES desta TV. "" volta a seguir a conta.
void ling_local_legenda(const char *v);
void ling_local_audio(const char *v);

// Lista fixa oferecida em Ajustes. O indice 0 e "seguir a conta" e o 1 e "sem
// filtro"; do 2 em diante sao codigos ISO.
// IDIOMA ORIGINAL DO TITULO ABERTO. Quem abre um titulo avisa qual e (vem de
// extras_idioma_original, que le o `original_language` do TMDB); a opcao
// "Original" da tela de Ajustes resolve para ele.
//
// Sem aviso, ou com aviso vazio, "Original" se comporta como "nao trocar de
// faixa" — que e o que o app sempre fez. Chutar um idioma seria pior: a pessoa
// ouviria dublagem sem saber que foi o app que escolheu.
void        ling_definir_original(const char *cod);
const char *ling_original(void);

// Indice de "Original do titulo" dentro da lista de opcoes. E o ULTIMO item de
// proposito: o ajustes.txt grava indice, nao codigo, entao inserir no meio
// reinterpreta o arquivo de quem ja tem ajuste salvo (ver a nota em linguas.c).
#define LING_OPC_ORIGINAL 30
int         ling_opcao_n(void);
const char *ling_opcao_codigo(int i);   // "" para conta, "*" para sem filtro

#endif
