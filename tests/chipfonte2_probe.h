// Observe the production renderer at its text and status drawing boundaries.
static TxtLinha chipSourceLine, chipQualityLine;
static char chipSourceText[320];
static TxtEstilo chipQualityStyle;
static int chipQualityInk, chipDotCount, chipDotState;
static float chipSourceX, chipSourceY, chipQualityX, chipQualityY, chipDotX, chipDotY;
static void chip_line(TxtLinha line, TxtEstilo style, const char *text, int ink) {
  if (!strncmp(text,"AIOStreams Stable",17)) {
    chipSourceLine=line; snprintf(chipSourceText,sizeof chipSourceText,"%s",text);
  }
  if (!strcmp(text,"4K · Dolby Vision · Atmos 5.1")) {
    chipQualityLine=line; chipQualityStyle=style; chipQualityInk=ink;
  }
}
static TxtLinha chip_linha(TxtEstilo style, const char *text, int r, int g, int b, int a) {
  TxtLinha line=txt_linha(style,text,r,g,b,a); chip_line(line,style,text,r); return line;
}
static TxtLinha chip_corta(TxtEstilo style, const char *text, int r, int g, int b, int a, float max) {
  TxtLinha line=txt_linha_corta(style,text,r,g,b,a,max); chip_line(line,style,text,r); return line;
}
static void chip_text(TxtLinha line, float x, float y, float a) {
  if (line.tex && line.tex==chipSourceLine.tex) { chipSourceX=x; chipSourceY=y; }
  if (line.tex && line.tex==chipQualityLine.tex) { chipQualityX=x; chipQualityY=y; }
  txt_desenhar_alpha(line,x,y,a);
}
static void chip_dot(int state, float x, float y, float a) {
  chipDotCount++; chipDotState=state; chipDotX=x; chipDotY=y;
  vtui_estado_ponto(state,x,y,a);
}
#define txt_linha chip_linha
#define txt_linha_corta chip_corta
#define txt_desenhar_alpha chip_text
#define vtui_estado_ponto chip_dot
