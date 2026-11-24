// FAROL NA ILHA — "QUANDO FALAR" (issue 20; PLANO 3.1, 3.4 e 5 item 20; ALMA 2 e 4).
//
// O Farol nao tem orbe, nem overlay, nem FBO proprio: ele mora na Ilha que o
// app ja tem (ilha.h fora do player, plrilha.h dentro dele). Este modulo e a
// POLITICA DE SILENCIO, pura (sem SDL, sem rede, sem disco, relogio injetado):
// quem integra (ilhasinais.c) le os fatos, pergunta aqui se pode falar e so
// entao chama a ilha. As regras, na ordem em que valem:
//
//   AVISO PROATIVO ("Algo impediu o vídeo · abre o Farol")
//     - so com falhasSeguidas >= 2 OU paradas5min >= 2;
//     - so JA FORA do player: com o player aberto ou o video andando, cala;
//     - no maximo 1 por titulo por sessao (trocar de titulo pode falar de novo;
//       sem titulo conhecido, "" conta como um titulo);
//     - dispensado com Voltar, o MESMO codigo de veredito nao volta por 24 h
//       (FAROLILHA_DISPENSA_S), nem em outro titulo.
//   CONSERTO CALADO (ALMA 5): so quando o app aplicou a troca SOZINHO (nao a
//     pessoa num botao do Farol). Pilula de 4 s, sem tecla, sem roubar foco,
//     a frase de faroltexto.h. O mesmo conserto repetido dentro de 4 s nao
//     abre uma segunda pilula.
//   ATIVIDADE ("Testando…"): so aparece depois de 300 ms ocupada — um teste
//     rapido nao pisca a pilula.
//   GANCHO DA #27 (Ajustes › Ajuda): farolilha_definir_ativo(0) cala o aviso
//     proativo e o conserto calado. Ligado por padrao.
//
// FIO PRINCIPAL. Estado estatico simples, sem travas e sem estado por fio.
#ifndef NV_FAROLILHA_H
#define NV_FAROLILHA_H
#include <stddef.h>

#define FAROLILHA_FALHAS_MIN    2
#define FAROLILHA_PARADAS_MIN   2
#define FAROLILHA_DISPENSA_S    86400LL     // 24 h
#define FAROLILHA_AVISO_MS      8000u       // P1, 8 s
#define FAROLILHA_CONSERTO_MS   4000u       // a pilula do conserto calado
#define FAROLILHA_ATIVIDADE_MS  300u        // limiar do "Testando…"
#define FAROLILHA_TITULOS       32          // titulos ja avisados na sessao
#define FAROLILHA_DISPENSAS     32          // codigos dispensados guardados
#define FAROLILHA_TITULO_MAX    64

typedef struct {
  int falhasSeguidas;   // FarolFatos.falhasSeguidas
  int paradas5min;      // FarolFatos.paradas5min
  int playerAberto;     // 1 = a tela do player existe (inclusive saindo)
  int videoAndando;     // 1 = ha video tocando/retido em algum plano
  const char *titulo;   // identidade do titulo da falha (imdb); NULL = ""
  int codigo;           // FAROL_C_* do veredito atual
} FarolIlhaFatos;

// Relogio injetado: `seg` = epoch em segundos (a janela de 24 h), `ms` =
// monotonico em milissegundos (300 ms e 4 s). NULL = o relogio do sistema.
void farolilha_relogio(long long (*seg)(void), unsigned (*ms)(void));

// Esquece TUDO (testes): sessao, dispensas, atividade; volta a ligado.
void farolilha_zerar(void);
// Sessao nova (arranque, troca de perfil): o teto por titulo zera; as
// dispensas de 24 h ficam.
void farolilha_sessao_nova(void);

// Gancho da #27: 0 cala o proativo e o conserto calado. Padrao 1.
void farolilha_definir_ativo(int ligado);
int  farolilha_ativo(void);

// 1 = o aviso proativo pode aparecer AGORA. Ao devolver 1 o titulo fica
// marcado como avisado nesta sessao e `codigo` passa a ser o da vez
// (farolilha_codigo_vez), o que Voltar dispensa.
int  farolilha_proativo(const FarolIlhaFatos *f);
// O sintoma (RES_S_*) com que o Farol abre no veredito para estes fatos:
// falhas seguidas = "nao comeca"; paradas = "trava".
int  farolilha_sintoma(const FarolIlhaFatos *f);
int  farolilha_codigo_vez(void);
// Voltar no aviso: o codigo cala por FAROLILHA_DISPENSA_S.
void farolilha_dispensar(int codigo);
// 1 = `codigo` dispensado ha menos de 24 h.
int  farolilha_dispensado(int codigo);

// Conserto calado: `acao` FAROL_A_*, `sozinho` 1 = o app aplicou sem a pessoa.
// Devolve a frase (chave FT_*, ainda sem i18n) para a pilula de 4 s, ou NULL
// para ficar calado.
const char *farolilha_conserto(int acao, int sozinho);
// A frase do conserto calado de cada acao (NULL = acao sem frase calada).
const char *farolilha_conserto_frase(int acao);

// Chamar por quadro com `ocupado` = ha teste em curso. 1 = mostrar a
// atividade (ja passou do limiar de 300 ms).
int  farolilha_atividade(int ocupado);

// Persistencia das dispensas (quem integra grava no disco): linhas
// "<codigo> <epoch>\n". Exportar devolve o tamanho escrito (sem o NUL);
// importar ignora linhas malformadas e as ja vencidas.
size_t farolilha_exportar(char *dst, size_t tam);
void   farolilha_importar(const char *s);

#endif
