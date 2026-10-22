// LOG DE MUDANCA DE AJUSTE: uma linha compacta por mudanca feita pela pessoa,
//   [ajustes] mudou <chave>: <antigo> -> <novo> (origem=ajustes|central|primeira|outro)
// para ranquear, nos logs de producao, os ajustes mais mexidos.
//
// REGRAS. So mudanca iniciada pela pessoa (carga, migracao e valores vindos da
// conta nao passam por aqui). Nunca texto livre nem segredo: ajuste de texto
// registra so "<texto>" ou "<vazio>"; chave com nome de segredo/endereco/PIN
// registra "<oculto>" mesmo que o valor seja numero. A mesma chave mexida de
// novo em menos de AJLOG_RAJADA_MS (segurar a seta num controle deslizante) vira
// UMA linha: o valor antigo do primeiro toque, o novo do ultimo, impressa quando
// a rajada acalma (ajlog_vazar) ou quando outra chave pede a vez.
#ifndef AJLOG_H
#define AJLOG_H

#define AJLOG_RAJADA_MS 2000

typedef enum { AJLOG_AJUSTES = 1, AJLOG_CENTRAL, AJLOG_PRIMEIRA, AJLOG_OUTRO } AjlogOrigem;

// 1 se o nome da chave indica segredo, endereco, PIN, nome ou e-mail.
int  ajlog_chave_sensivel(const char *chave);
// Mudanca de um ajuste numerico (escolha, numero, interruptor).
void ajlog_mudou(const char *chave, int antigo, int novo, AjlogOrigem origem, unsigned agora_ms);
// Mudanca de um ajuste de texto: o conteudo nunca sai daqui.
void ajlog_mudou_texto(const char *chave, const char *antigo, const char *novo,
                       AjlogOrigem origem, unsigned agora_ms);
// Imprime as rajadas paradas ha AJLOG_RAJADA_MS ou mais (chamar a cada quadro).
void ajlog_vazar(unsigned agora_ms);
// Imprime tudo o que esta pendente (saida do app).
void ajlog_vazar_tudo(void);
// Onde as linhas saem (testes); NULL volta ao printf.
void ajlog_saida(void (*fn)(const char *linha));

#endif
