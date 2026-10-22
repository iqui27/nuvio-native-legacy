// CENTRAL DE AVISOS — o unico lugar onde o app "chama" a pessoa.
//
// Nem webOS nem Tizen entregam notificacao a um app fechado, e este app nao
// tem servico de fundo (ver agendaviso.h). O que da para fazer e avisar com o
// app ABERTO, e antes disto cada coisa avisava do seu jeito: o cartao de
// atualizacao abria sozinho, o lembrete de serie abria outro cartao, e a
// recomendacao de amigo so aparecia para quem abrisse o painel de Salvos.
// Pedido do dono (19/09/2026): um toast quando chega algo, AZUL/CH+ abre, e
// uma lista com tudo — mais um canal para ELE avisar de defeito conhecido
// enquanto nao esta consertado, e um jeito de mandar o registro quando o app
// cai.
//
// CINCO ORIGENS, um mesmo item:
//   rec      recomendacao de amigo (recomenda.c)         -> abre Salvos
//   agenda   episodio de serie com lembrete estreou      -> abre o titulo
//   update   versao nova no GitHub (atualizacao.c)       -> abre o cartao
//   canal    aviso do dono, de avisos.json no proprio    -> so le
//            repositorio (raw.githubusercontent.com)
//   crash    o app fechou sem despedida na ultima vez    -> "Enviar registro"
//
// O CRASH e detectado por MARCA: sessao-viva.txt e gravado no arranque e
// apagado na saida limpa. Marca presente no arranque seguinte = a sessao
// anterior morreu (crash, kill do SAM por memoria, energia). Nao ha como saber
// qual; o registro e que diz. main.c preserva o log da sessao anterior em
// /tmp/nuvio-anterior.log ANTES de truncar o atual, e e esse arquivo que
// "Enviar registro" manda (ultimos 200 KB) para o servico de recomendacoes
// (servidor/recomendacoes, rota /v1/registro). So com o botao; nunca sozinho.
// O log ja sai sem credencial (rede_url_publica em rede.h). No Tizen nao ha
// arquivo: vai so versao, plataforma e a data.
//
// O CANAL DO DONO e um arquivo JSON no repositorio, lido a cada 30 min:
//   [{"id":"2026-09-19-guia","desde":"2026-09-19","ate":"2026-10-01",
//     "plataforma":"todas|lg|tizen","ate_versao":"1.3.1",
//     "titulo":"...","titulo_en":"...","texto":"...","texto_en":"..."}]
// `ate_versao` deixa avisar so quem ainda esta numa versao com o defeito.
//
// DISCIPLINA: rede em fio proprio, estado atras de mutex, o laco de desenho so
// le. Chaves de tela em idioma_tab.h.
#ifndef NV_AVISOS_H
#define NV_AVISOS_H
#include <SDL2/SDL.h>

// Avisa (toast + item na lista) que o idioma da interface foi escolhido sozinho.
// Um item por idioma, entao so uma vez por idioma. Chamado por ajustes.c.
void avisos_idioma_definido(const char *codigo, const char *texto);
// 1 se a sessao ANTERIOR nao se despediu (crash, kill por memoria, energia),
// ja descontada a despedida do Tizen (pagina escondida = a TV fechou em segundo
// plano, nao e queda). Valido depois de avisos_iniciar; o modo seguro (seguro.h)
// decide o que desfazer a partir disto. Existe porque dados_despedida_ler apaga
// o que le: so avisos_iniciar pode pergunta-lo, uma vez.
int  avisos_sessao_anterior_caiu(void);
// Aviso do modo seguro (ajuste desfeito, perfil seguro): entra na lista com toast.
// Um item por `id`; o id leva o numero da sessao, entao cada queda avisa uma vez.
void avisos_modo_seguro(const char *id, const char *titulo, const char *texto);
void avisos_iniciar(void);   // depois de dados_iniciar; grava a marca de sessao
void avisos_encerrar(void);  // saida limpa: apaga a marca
// Ultimo sinal de vida na marca de sessao: evento de janela ("oculto",
// "visivel", "foco-perdido"...) grava na hora; NULL e a batida periodica
// (no maximo 1 a cada 60 s). Ver marcaGravar em avisos.c.
void avisos_sinal(const char *evento, float rssMb);
void avisos_atualizar(float dt, Uint32 agora);
void avisos_desenhar(Uint32 agora);
// 1 quando consumiu o evento (toast na tela com AZUL/CH+, ou painel aberto).
int  avisos_evento(const SDL_Event *e);
int  avisos_aberto(void);
void avisos_abrir(void);
int  avisos_n_novos(void);
// O CARTAO DO CRASH: abre uma vez por crash, quando a home esta de pe e nenhum
// outro cartao esta na frente (app.c chama como agendaviso_mostrar_se_houver).
// Enquanto aberto, avisos_evento come todo o teclado.
void avisos_mostrar_se_houver(void);
int  avisos_cartao_aberto(void);
// TESTE (#99): a linha em foco da central aberta; -1 com ela fechada.
int  avisos_teste_foco(void);

// ACOES PEDIDAS PELO PAINEL, entregues a app.c uma vez cada (o mesmo contrato
// de spainel_pediu_abrir): o IMDb de um titulo a abrir, ou um dos codigos.
const char *avisos_pediu_abrir(void);
// AVISOS_ABRIR_AMIGOS: a linha dos pedidos de amizade (o painel vai para a
// aba Amigos, onde cada pedido tem Aceitar e Recusar).
enum { AVISOS_NADA = 0, AVISOS_ABRIR_SALVOS, AVISOS_ABRIR_ATUALIZACAO, AVISOS_ABRIR_AMIGOS };
int  avisos_pediu(void);

// ENVIO MANUAL DO REGISTRO DESTA SESSAO, pela linha "Enviar registro" dos
// Ajustes. Mesmo destino e mesmo teto (200 KB) do cartao de crash; o estado
// e o mesmo contador: 0 nada, 1 enviando, 2 enviado, 3 falhou.
void avisos_enviar_registro_atual(void);
// Envia somente o relatorio estruturado de uma sessao de diagnostico. O
// sucesso exige HTTP 2xx e um identificador de registro retornado pelo
// servidor; o log geral nao e capturado nem misturado a esta operacao.
int  avisos_enviar_diagnostico(const char *execucao_id, const char *relatorio,
                               char *registro_id, unsigned tam_registro_id);
// Envio automatico (ajuste "Enviar registros sozinho"): chamado por quadro.
void avisos_envio_auto_passo(Uint32 agora);
int  avisos_envio_estado(void);
// O ULTIMO ENVIO, para o painel de envio do registro (registro.c) e o
// inspetor de Ajustes. `motivo` diz por que o manual falhou: o servidor
// respondeu com erro (http), a TV esta sem internet (redesaude.h; nem tenta,
// e manda sozinho quando a rede volta), o prazo de 30 s acabou, a conexao
// caiu, falta login, ou esta compilacao nao tem servidor.
enum { AVISOS_ENVIO_OK = 1, AVISOS_ENVIO_SERVIDOR, AVISOS_ENVIO_OFFLINE,
       AVISOS_ENVIO_PRAZO, AVISOS_ENVIO_CONEXAO, AVISOS_ENVIO_CONTA,
       AVISOS_ENVIO_INDISPONIVEL };
#include <time.h>
typedef struct {
  int    disponivel;          // ha servidor de registros nesta compilacao
  int    estado, motivo, http, linhas, pendenteRede;
  long   bytes;
  time_t quando;              // inicio do ultimo envio manual
  char   codigo[8];           // codigo do ultimo envio manual (vazio sem recibo)
  time_t autoQuando;          // ultimo envio automatico (0 = nenhum)
  int    autoHttp;
  Uint32 autoProximoMs;       // SDL_GetTicks do proximo automatico (0 = nao marcado)
  char   ultimoCodigo[8];     // o ultimo codigo recebido, de qualquer sessao
  time_t ultimoCodigoQuando;
} AvisosEnvio;
int  avisos_envio_info(AvisosEnvio *o);
#ifdef AVISOS_TESTE_ENVIO
void avisos_teste_envio(int estado, int motivo, int http, const char *codigo, long bytes, int linhas);
void avisos_teste_envio_auto(long haSeg, int http);
void avisos_teste_queda(const char *texto);
#endif

// A LISTA COMO COMPONENTE, para a aba AVISOS do painel de Salvos: quem hospeda
// desenha na caixa que tem, guarda o proprio foco e chama _ok no OK. _ok
// devolve 1 quando a linha abre outra coisa (o hospedeiro fecha); _marcar_lidos
// ao fechar. Uma linha mede AVISOS_LINHA_H, EXCETO a linha em foco de um aviso
// do canal, que cresce para o texto inteiro: o hospedeiro posiciona por
// avisos_lista_y / avisos_lista_altura_linha, que sabem disso.
#include <stddef.h>
#define AVISOS_LINHA_H 164.0f
int   avisos_lista_n(void);
float avisos_lista_altura(void);
float avisos_lista_altura_linha(int linha, int focoLinha);
float avisos_lista_y(int linha, int focoLinha);
void  avisos_lista_desenhar(float x, float y, float w, float a, int focoLinha);
// A mesma lista registrando cada linha no ponteiro (#99): `focar(linha, 0)`
// e de quem hospeda (indices de focoLinha; n = "Dispensar todos"), recortado
// a faixa [clipY0, clipY1). focar NULL = nao registra (= avisos_lista_desenhar).
void  avisos_lista_desenhar_ptr(float x, float y, float w, float a, int focoLinha,
                                void (*focar)(int, int), float clipY0, float clipY1);
// 1 = fez algo e o hospedeiro fecha; 2 = so trocou de lugar (pedidos de
// amizade: o painel vai para a aba Amigos e fica aberto).
int   avisos_lista_ok(int linha);
void  avisos_marcar_lidos(void);

// DISPENSAR (06/10, avisodisp.h). Tira o item da lista e grava a chave por
// perfil: a mesma chave nao volta, nem depois de reiniciar; um evento novo
// (outro episodio, outra versao) volta. "rec" e excecao: responde a
// recomendacao em vez de gravar. A ULTIMA LINHA da lista, quando ha itens, e
// "Dispensar todos": avisos_lista_linhas() = n + 1 (o hospedeiro navega por
// ela; avisos_lista_n() continua sendo a contagem de avisos), e
// avisos_lista_ok(n) dispensa todos e devolve 0 (a lista fica, vazia).
// avisos_lista_item: o id, o titulo para o menu e, numa estreia com o lembrete
// ainda ligado, o imdb para "Remover lembrete" ("" sem). 0 = linha invalida.
void  avisos_dispensar(const char *id);
void  avisos_dispensar_todos(void);
int   avisos_lista_dispensar(int linha);
int   avisos_lista_linhas(void);
int   avisos_lista_item(int linha, char *id, size_t tamId, char *titulo, size_t tamTit,
                        char *imdbLembrete, size_t tamImdb);

// A ilha (ilhacart.c): o episodio novo (AV_AGENDA) ainda nao lido mais
// recente — o id do item e o imdb da serie — e marcar UM item como lido. Uma
// estreia ADIADA nesta sessao ("Depois", avisodisp_sessao_*) nao conta.
int  avisos_estreia_pendente(char *id, size_t tamId, char *imdb, size_t tamImdb);
void avisos_marcar_visto(const char *id);

// Botao do MODAL DA ILHA de um aviso da central (chave "av:<id>", ver
// anunciarItem em avisos.c): `botao` 1..3, o que ilha_aviso_pediu entregou.
// Executa (abrir o titulo, salvar, enviar o registro, abrir a atualizacao) e
// marca o item como lido.
void avisos_ilha_acao(const char *chave, int botao);

#ifdef NV_WEBOS
// Linha [tv] e modelo (para o relato de falha de arranque; arranque.c).
void avisos_tv_linha(char *linha, size_t cap, char *modelo, size_t capModelo);
// POST sincrono, `segundos` de teto, em /v1/registro/arranque. 1 = confirmado.
int  avisos_enviar_arranque(const char *relato, const char *tv, int segundos);
#endif
#endif
