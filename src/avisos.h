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

void avisos_iniciar(void);   // depois de dados_iniciar; grava a marca de sessao
void avisos_encerrar(void);  // saida limpa: apaga a marca
void avisos_atualizar(float dt, Uint32 agora);
void avisos_desenhar(Uint32 agora);
// 1 quando consumiu o evento (toast na tela com AZUL/CH+, ou painel aberto).
int  avisos_evento(const SDL_Event *e);
int  avisos_aberto(void);
void avisos_abrir(void);
int  avisos_n_novos(void);

// ACOES PEDIDAS PELO PAINEL, entregues a app.c uma vez cada (o mesmo contrato
// de spainel_pediu_abrir): o IMDb de um titulo a abrir, ou um dos codigos.
const char *avisos_pediu_abrir(void);
enum { AVISOS_NADA = 0, AVISOS_ABRIR_SALVOS, AVISOS_ABRIR_ATUALIZACAO };
int  avisos_pediu(void);

// A LISTA COMO COMPONENTE, para a aba AVISOS do painel de Salvos: quem hospeda
// desenha na caixa que tem, guarda o proprio foco e chama _ok no OK. _ok
// devolve 1 quando a linha abre outra coisa (o hospedeiro fecha); _marcar_lidos
// ao fechar. Uma linha mede AVISOS_LINHA_H.
#define AVISOS_LINHA_H 164.0f
int   avisos_lista_n(void);
float avisos_lista_altura(void);
void  avisos_lista_desenhar(float x, float y, float w, float a, int focoLinha);
int   avisos_lista_ok(int linha);
void  avisos_marcar_lidos(void);

#endif
