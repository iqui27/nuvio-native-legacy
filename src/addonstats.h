// LATENCIA POR ADD-ON, POR TV (#202, "tempo de inicio do stream").
//
// Medido no D1 (07/10, 367 inicios): PenguPlay e o ultimo a chegar em quase
// toda busca (mediana 6,3 s, p90 15,8 s), WebStreamr nao responde em 41 de 43
// e ha centenas de "segunda tentativa (30 s)" de add-on mudo. Quem espera por
// eles paga na hora do Play. Aqui fica a memoria do que cada um fez NESTA TV
// (a rede, o provedor e a regiao mudam de casa para casa): uma janela curta
// por identidade de configuracao e perfil, num arquivo pequeno.
//
// O QUE SE FAZ COM ISSO, e so isto:
//   - um add-on MUDO (sem resposta nas ultimas ADDONSTATS_MUDO_IGNORA buscas
//     seguidas) ou CONSISTENTEMENTE MAIS LENTO que a espera da pessoa deixa de
//     segurar a decisao automatica. Ele continua sendo consultado e, quando
//     responde, entra na lista manual como sempre;
//   - a segunda tentativa de quem ja foi mudo cai de 30 s para 10 s, e some
//     quando foi mudo as ultimas ADDONSTATS_MUDO_PULA vezes.
// Nada aqui mexe na ordem nem na qualidade: so em QUANTO se espera.
//
// Puro (sem SDL/rede). O arquivo entra por addonstats_carregar /
// addonstats_serializar; addonstats_ler/salvar ligam isso a dados.h e ficam de
// fora quando NV_ADDONSTATS_PURO esta definido (o teste do Mac).
#ifndef NV_ADDONSTATS_H
#define NV_ADDONSTATS_H

#define ADDONSTATS_JANELA 8          // amostras guardadas por identidade
#define ADDONSTATS_MAX 288           // nomes (128 addons + 160 scrapers de plugin)
#define ADDONSTATS_MUDO_IGNORA 2     // seguidas sem resposta: sai da espera
#define ADDONSTATS_MUDO_PULA 3       // seguidas: sem segunda tentativa
#define ADDONSTATS_SEGUNDA_MUDO_S 10 // segunda tentativa de quem ja foi mudo

// Uma busca real terminou para este add-on. `ms` = tempo desde o disparo
// ate a resposta (ou ate desistir); `respondeu` 0 = mudo (timeout/erro de
// transporte; lista vazia CONTA como resposta).
void addonstats_registrar(const char *nome, unsigned ms, int respondeu);
// Quantas buscas seguidas, da mais recente para tras, ficaram sem resposta.
int  addonstats_mudo_seguidas(const char *nome);
// 1 quando, em pelo menos 3 amostras, 2 de cada 3 passaram de `limiteMs`
// (mudo conta como passou).
int  addonstats_lento(const char *nome, unsigned limiteMs);
// 1 quando deixa de segurar a decisao automatica: mudo seguido ou lento.
// `limiteMs` = a espera de Ajustes; 0 = nao ignora ninguem (sem espera, ou
// "Todos os add-ons").
int  addonstats_ignoravel(const char *nome, unsigned limiteMs);
// Prazo da segunda tentativa deste add-on, em segundos: `padraoS` para quem
// nunca falhou, ADDONSTATS_SEGUNDA_MUDO_S para quem foi mudo na ultima busca,
// 0 = nao tentar de novo (mudo as ultimas ADDONSTATS_MUDO_PULA).
int  addonstats_segunda_s(const char *nome, int padraoS);
// Mediana (ms) das amostras que responderam; 0 sem amostra.
unsigned addonstats_mediana(const char *nome);

// Arquivo de texto: uma linha por chave opaca, "chave\tms:ok ms:ok ...".
int  addonstats_serializar(char *dst, unsigned tam);
void addonstats_carregar(const char *texto);
void addonstats_zerar(void);

#ifndef NV_ADDONSTATS_PURO
// Le/grava dados_dir()/fonte-latencia.txt. Ler uma vez no arranque; salvar e
// barato e so escreve quando algo mudou desde a ultima.
void addonstats_ler(void);
void addonstats_salvar(void);
#endif
#endif
