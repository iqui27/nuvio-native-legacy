#ifndef NV_LEGOPACIDADE_H
#define NV_LEGOPACIDADE_H

// Keep persisted 0/1/2/3 = 100/75/50/25%. New choices are appended, so
// player preferences from older releases retain exactly the same appearance.
#define LEG_OPAC_N 20
#define LEG_OPAC_ROTULOS "100%", "95%", "90%", "85%", "80%", "75%", \
  "70%", "65%", "60%", "55%", "50%", "45%", "40%", "35%", "30%", \
  "25%", "20%", "15%", "10%", "5%"

static inline int leg_opacidade_pct(int modo) {
  static const unsigned char percentuais[LEG_OPAC_N] = {
    100,75,50,25,95,90,85,80,70,65,60,55,45,40,35,30,20,15,10,5
  };
  return percentuais[modo>=0 && modo<LEG_OPAC_N ? modo : 0];
}
static inline int leg_opacidade_modo(int pct) {
  if(pct<5)pct=5;
  if(pct>100)pct=100;
  pct=((pct+2)/5)*5;
  for(int i=0;i<LEG_OPAC_N;i++)if(leg_opacidade_pct(i)==pct)return i;
  return 0;
}
// Settings lists are ordered by percentage; persistence uses stable IDs.
static inline int leg_opacidade_indice(int modo) {
  return (100-leg_opacidade_pct(modo))/5;
}
static inline int leg_opacidade_do_indice(int indice) {
  if(indice<0)indice=0;
  if(indice>=LEG_OPAC_N)indice=LEG_OPAC_N-1;
  return leg_opacidade_modo(100-indice*5);
}
static inline float leg_opacidade_alpha(int modo) {
  return leg_opacidade_pct(modo)/100.0f;
}
static inline int leg_opacidade_byte(int modo) {
  return (leg_opacidade_pct(modo)*255+50)/100;
}
// Secondary subtitles retain a distinct zero meaning "Same as primary".
static inline int leg_opacidade_secundaria_migrar(int antigo) {
  return antigo>=1 && antigo<=4 ? leg_opacidade_indice(antigo-1)+1 : 0;
}
#endif
