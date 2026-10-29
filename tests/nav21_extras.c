#include "../src/extras.c"
void nav21_extras(void){
  nTrailer=2;nRel=3;nCol=3;nEstudio=2;nComent=2;
  notas[EX_IMDB]=80;snprintf(colNome,sizeof colNome,"Offline collection");
  for(int i=0;i<3;i++){snprintf(rel[i].titulo,sizeof rel[i].titulo,"Related %d",i);snprintf(rel[i].imdb,sizeof rel[i].imdb,"tt%07d",i+20);snprintf(col[i].titulo,sizeof col[i].titulo,"Collection %d",i);col[i].tmdb=i+1;}
  for(int i=0;i<2;i++){snprintf(trailer[i].nome,sizeof trailer[i].nome,"Trailer %d",i);snprintf(trailer[i].yt,sizeof trailer[i].yt,"fixture%d",i);snprintf(estudio[i].nome,sizeof estudio[i].nome,"Studio %d",i);coment[i].texto="Offline public comment";snprintf(coment[i].user,sizeof coment[i].user,"Friend %d",i);}
}
