// Onde o app grava o que e DO USUARIO — sessao, perfil ativo, cache de sync.
//
// Ate agora ajustes.c e catalogo.c gravavam dentro de dirArte, a pasta do
// PACOTE. Em modo desenvolvedor isso funciona e por isso passou despercebido;
// num app instalado de verdade a pasta do pacote e o lugar errado, e e a mesma
// para todo mundo que usar o aparelho. Com login, gravar ali significaria a
// sessao de uma pessoa dentro do app de outra.
//
// A pasta e DESCOBERTA, nao chutada: nao ha documentacao publica confiavel de
// onde um app NATIVO do webOS pode escrever, e um caminho fixo errado
// transforma "nao salvou nada" num defeito mudo — o app abre, parece logado, e
// no proximo arranque esqueceu tudo sem nenhuma mensagem. A sonda tenta
// escrever de verdade em cada candidato e registra no log qual venceu.
#ifndef NV_DADOS_H
#define NV_DADOS_H

// Escolhe a pasta. `dirArte` entra como ULTIMO recurso (e o comportamento de
// hoje, e e melhor que nao gravar nada). Chamar uma vez, no arranque.
void dados_iniciar(const char *dirArte);

// Pasta escolhida, sem barra no fim. Nunca NULL depois de dados_iniciar; pode
// ser "" se nenhum candidato aceitou escrita — nesse caso gravar e no-op e o
// log ja disse por que.
const char *dados_dir(void);

// Monta `dados_dir()/nome` em `dst`. Devolve dst, ou NULL se nao ha pasta.
char *dados_caminho(char *dst, unsigned tam, const char *nome);

// Grava `conteudo` em `nome` de forma atomica (temporario + rename). 1 se deu
// certo. Atomico porque perder a sessao por causa de um arquivo escrito pela
// metade e exatamente o tipo de defeito que so aparece no aparelho de outra
// pessoa.
int dados_gravar(const char *nome, const char *conteudo);
// Mesma gravacao, mas a descarga para o IndexedDB pode esperar: e o relogio de
// 15 s em vez do de 700 ms. So para conteudo RE-OBTIVEL — posicao de cursor,
// cache de imagem. Dado do usuario continua em dados_gravar.
int dados_gravar_leve(const char *nome, const char *conteudo);

// Le `nome` inteiro para um buffer novo terminado em NUL (free pelo chamador).
char *dados_ler(const char *nome);

int dados_apagar(const char *nome);

// Descarrega para o armazenamento persistente o que foi gravado desde a ultima
// descarga. No webOS e no Mac nao faz nada: la o fopen ja escreveu em disco. No
// alvo Tizen o arquivo esta em IndexedDB e so vai para la aqui.
//
// Chamar do LACO PRINCIPAL, uma vez por quadro, nao de fio de trabalho. A
// funcao decide sozinha SE e hora de descarregar: quase todo quadro ela custa
// uma leitura de dois inteiros e volta. Ver a politica em dados.c.
void dados_sincronizar(void);

// Diz que alguem gravou no sistema de arquivos por FORA de dados_gravar.
// `leve` = 1 para conteudo re-obtivel (o cache de imagens de tex_cache.c), que
// nao merece pagar uma descarga por si so: perder o ultimo poster baixado custa
// um download, perder a sessao custa um login por QR.
void dados_marcar_sujo(int leve);

// TRAVA DO SISTEMA DE ARQUIVOS, para quem grava por fora deste modulo.
//
// So faz algo no alvo Tizen, e la nao e opcional: o "sistema de arquivos" do
// WASM e uma estrutura JavaScript compartilhada entre os workers e NAO e segura
// entre fios. Ver a nota longa em dados.c — o sintoma de ignorar isto foi o app
// inteiro CONGELAR, sem erro nenhum. Envolver so as ESCRITAS e as remocoes;
// leituras concorrentes com a descarga sao seguras e travar o decode de imagem
// (30 ms) serializaria os fios de decodificacao a toa.
void dados_fs_travar(void);
void dados_fs_liberar(void);

// Telemetria da descarga, para a linha de quadro de main.c: quantas descargas
// desde o ultimo relatorio e o custo SINCRONO da pior delas em ms. Sem isto nao
// ha como distinguir "o pico sumiu" de "o pico mudou de lugar".
extern int    dados_desc_n;
extern double dados_desc_ms;
void dados_desc_zerar(void);

// Identificador ESTAVEL desta instalacao, gerado na primeira execucao e
// gravado. O sync do web manda isto em `p_origin_client_id` para o servidor nao
// devolver ao aparelho a escrita que ele mesmo acabou de fazer — sem um id
// estavel, cada arranque parece um aparelho novo e o eco volta.
const char *dados_cliente_id(void);

// UUID v4 novo a cada chamada. MEDIDO contra o servidor: a RPC
// start_tv_login_session recusa com "Invalid device nonce" qualquer nonce que
// nao seja um UUID — um identificador proprio, mesmo unico, nao passa.
void dados_uuid(char *dst, unsigned tam);

#endif

// 0 quando NADA do que o app grava sobrevive ao fechamento. So acontece no
// alvo Tizen, quando o IDBFS nao monta — e sem ele Trakt, progresso e sync
// gravam na RAM e somem. Aparece no relatorio de 3 s por isso.
int dados_persistente(void);
