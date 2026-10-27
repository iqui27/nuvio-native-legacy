/* Specialized Pencil pages keep existing actions and report unsupported controls. */
static void ajPenRules(void) {
  ajPenBase();
  ajPenChip("Ajustes","pl_chevron-left",200,112,124,0,5,0);
  ajKicker("AJUSTES",200,198,1);
  aj4Texto("Regras da TV",TXT_TITULO2,200,212,1500,1);
  aj4Texto("O que a sua TV aguenta. O Nuvio segue estas regras para não travar nem cortar a imagem.",TXT_G20M,200,291,1520,.58f);
  static const char *const heads[]={"O QUE A TV FALA","O QUE O NUVIO NÃO FAZ"};
  static const char *const names[]={"Resolução da interface","HDR","Dolby Vision","DTS","Decodificação","Memória",
    "Recortar a imagem","Trocar de fonte sozinho","Esconder a faixa original","Enviar seu histórico","Forçar efeito pesado","Ligar a voz sem avisar"};
  static const char *const help[]={"A TV desenha até 4K","HDR10 e HDR10+ são entregues como HDR10","Perfil 8, com DV se a TV pedir","Convertido em Dolby Digital ou estéreo","HEVC e AV1 por hardware","O teto de RAM do aparelho",
    "A TV decide o formato; o Nuvio avisa","Só quando você deixa o automático","O original é sempre a primeira opção","Só com o seu comando","Se a TV não aguenta, o efeito não vem","A busca por voz só liga com o botão"};
  static const char *const icons[]={"aj_monitor","aj_sun","aj_sparkles","aj_volume-2","aj_cpu","aj_hard-drive","aj_scan","aj_refresh-cw","aj_eye-off","aj_history","aj_cpu","aj_mic"};
  char ram[64];snprintf(ram,sizeof ram,i18n("%d MB nesta TV"),tex_teto_ram_mb());
  const char *values[]={textoValor(AJ_RESOLUCAO),textoValor(AJ_HDR_UI),textoValor(AJ_DV),textoValor(AJ_DTS_AC3),textoValor(AJ_GPU_EFEITOS),ram};
  for(int c=0;c<2;c++) {
    float x=c?1024:200;ajPenPanel(x,344,800,640,0);ajKicker(heads[c],x+26,366,1);
    for(int r=0;r<6;r++) {
      int i=c*6+r;float y=400+r*84;
      ajIcone(icons[i],x+46,y+24,26,.75f);
      aj4Texto(names[i],TXT_G21B,x+88,y+13,470,1);
      aj4Texto(help[i],TXT_AJ_MINI14,x+88,y+42,650,.4f);
      if(!c) { TxtLinha t=ajTxtC(TXT_G18R,values[i],220);txt_desenhar_alpha(t,x+710-t.w,y+24,.58f); }
    }
  }
  ajPenRodape("Regras da TV",3,3);
}
