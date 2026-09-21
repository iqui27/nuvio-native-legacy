// ULTIMAS NOTICIAS DE UM TITULO, para o menu de contexto da Agenda.
//
// Pedido do dono (21/09/2026): "vamos pegar as ultimas noticias de cada
// titulo". Fonte: o RSS de busca do Google News, que nao pede chave, aceita
// qualquer origem e devolve manchete, veiculo e data. So MANCHETES: a TV nao
// abre link, entao a lista e o que se le, nao um indice para clicar.
//
//   https://news.google.com/rss/search?q="<titulo>"&hl=pt-BR&gl=BR&ceid=BR:pt-419
//
// A lingua segue a da interface (ajustes_idioma_ingles). Uma passada de rede
// por titulo, em fio proprio, com cache em memoria e em disco por 6 h
// (noticias-<imdb>.txt, dados_gravar_leve). O laco de desenho so le.
#ifndef NV_NOTICIAS_H
#define NV_NOTICIAS_H

#define NOT_MAX 12

typedef struct {
  char titulo[240];   // manchete, ja sem " - Veiculo" no fim
  char fonte[80];     // veiculo
  char data[16];      // "20 set" / "20 Sep", pronta para desenhar
} Noticia;

// Dispara a busca (uma vez por imdb; repetir e gratis). `serie` so muda a
// palavra de apoio na consulta ("serie"/"filme") para desambiguar titulos.
void noticias_pedir(const char *imdb, const char *titulo, int serie);
// 1 quando a rede ja respondeu (com ou sem manchetes).
int  noticias_respondeu(const char *imdb);
int  noticias_n(const char *imdb);
const Noticia *noticias_item(const char *imdb, int i);

#endif
