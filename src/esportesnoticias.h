#ifndef NV_ESPORTESNOTICIAS_H
#define NV_ESPORTESNOTICIAS_H
#define ESP_MAX_NOTICIAS 24
typedef struct { char titulo[256], resumo[1800], imagem[512], origem[96], url[512]; } EspNoticia;
int esportes_noticias_ler(const char *json, EspNoticia *out, int cap);
void esportes_noticias_pedir(int atualizar);
int esportes_noticias_n(void);
const EspNoticia *esportes_noticia(int i);
int esportes_noticias_estado(void); /* 0 loading, 1 ready, -1 failed */
#endif
