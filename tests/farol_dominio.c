// Farol — dominio sem esquema nao entra no texto livre do gosto
// (PLANO-GOSTO 4.1, issue 35). A dobra usa separador para tokenizar
// palavra, mas o separador nao pode apagar o rotulo antes do ponto:
// a varredura da revisao r4 (364 rotulos sinteticos, 340 deles
// convertidos a aceite pela dobra nova) volta a dar recusa em todos,
// junto dos dominios nomeados do achado — roda a funcao de producao
// faroltexto_aceita(), nada de grep.
//
//   cc tests/farol_dominio.c src/faroltexto.c -Isrc -o /tmp/t-dominio && /tmp/t-dominio
#include "faroltexto.h"
#include <stdio.h>

static int falhas;
static void caso(const char *s, int espera) {
  int deu = faroltexto_aceita(s);
  if (deu != espera) {
    printf("FALHOU: aceita=%d esperado=%d :: %s\n", deu, espera, s);
    falhas++;
  }
}

static void dominio(const char *host) {
  char frase[80];
  snprintf(frase, sizeof frase, "Veja %s/filme.", host);
  caso(frase, 0);
}

// 364 rotulos da varredura sol-farol35r4/sondas-idn.tsv — todos fora,
// dos convertiveis por WHATWG aos que a ponta ja recusava
static const char *ROTULOS[] = {
  "՚.com", "filme՚.com", "՛.com", "filme՛.com",
  "՜.com", "filme՜.com", "՝.com", "filme՝.com",
  "՞.com", "filme՞.com", "՟.com", "filme՟.com",
  "։.com", "filme։.com", "֊.com", "filme֊.com",
  "־.com", "filme־.com", "׀.com", "filme׀.com",
  "׃.com", "filme׃.com", "׆.com", "filme׆.com",
  "؉.com", "filme؉.com", "؊.com", "filme؊.com",
  "،.com", "filme،.com", "؍.com", "filme؍.com",
  "؛.com", "filme؛.com", "؝.com", "filme؝.com",
  "؞.com", "filme؞.com", "؟.com", "filme؟.com",
  "٪.com", "filme٪.com", "٫.com", "filme٫.com",
  "٬.com", "filme٬.com", "۔.com", "filme۔.com",
  "܀.com", "filme܀.com", "܁.com", "filme܁.com",
  "܂.com", "filme܂.com", "܃.com", "filme܃.com",
  "܄.com", "filme܄.com", "܅.com", "filme܅.com",
  "܆.com", "filme܆.com", "܇.com", "filme܇.com",
  "܈.com", "filme܈.com", "܉.com", "filme܉.com",
  "܊.com", "filme܊.com", "܋.com", "filme܋.com",
  "܌.com", "filme܌.com", "܍.com", "filme܍.com",
  "।.com", "filme।.com", "॥.com", "filme॥.com",
  "॰.com", "filme॰.com", "၊.com", "filme၊.com",
  "။.com", "filme။.com", "၌.com", "filme၌.com",
  "၍.com", "filme၍.com", "၎.com", "filme၎.com",
  "၏.com", "filme၏.com", "჻.com", "filme჻.com",
  "፠.com", "filme፠.com", "፡.com", "filme፡.com",
  "።.com", "filme።.com", "፣.com", "filme፣.com",
  "፤.com", "filme፤.com", "፥.com", "filme፥.com",
  "፦.com", "filme፦.com", "፧.com", "filme፧.com",
  "፨.com", "filme፨.com", "᙮.com", "filme᙮.com",
  "᛫.com", "filme᛫.com", "᛬.com", "filme᛬.com",
  "᛭.com", "filme᛭.com", "᜵.com", "filme᜵.com",
  "᜶.com", "filme᜶.com", "។.com", "filme។.com",
  "៕.com", "filme៕.com", "៖.com", "filme៖.com",
  "៘.com", "filme៘.com", "៙.com", "filme៙.com",
  "៚.com", "filme៚.com", "᠀.com", "filme᠀.com",
  "᠁.com", "filme᠁.com", "᠂.com", "filme᠂.com",
  "᠃.com", "filme᠃.com", "᠄.com", "filme᠄.com",
  "᠅.com", "filme᠅.com", "᠆.com", "filme᠆.com",
  "᠇.com", "filme᠇.com", "᠈.com", "filme᠈.com",
  "᠉.com", "filme᠉.com", "᠊.com", "filme᠊.com",
  "᥄.com", "filme᥄.com", "᥅.com", "filme᥅.com",
  "Ⓜ.com", "filmeⓂ.com", "■.com", "filme■.com",
  "□.com", "filme□.com", "▢.com", "filme▢.com",
  "▣.com", "filme▣.com", "▤.com", "filme▤.com",
  "▥.com", "filme▥.com", "▦.com", "filme▦.com",
  "▧.com", "filme▧.com", "▨.com", "filme▨.com",
  "▩.com", "filme▩.com", "▪.com", "filme▪.com",
  "▫.com", "filme▫.com", "▬.com", "filme▬.com",
  "▭.com", "filme▭.com", "▮.com", "filme▮.com",
  "▯.com", "filme▯.com", "▰.com", "filme▰.com",
  "▱.com", "filme▱.com", "▲.com", "filme▲.com",
  "△.com", "filme△.com", "▴.com", "filme▴.com",
  "▵.com", "filme▵.com", "▶.com", "filme▶.com",
  "▷.com", "filme▷.com", "▸.com", "filme▸.com",
  "▹.com", "filme▹.com", "►.com", "filme►.com",
  "▻.com", "filme▻.com", "▼.com", "filme▼.com",
  "▽.com", "filme▽.com", "▾.com", "filme▾.com",
  "▿.com", "filme▿.com", "◀.com", "filme◀.com",
  "◁.com", "filme◁.com", "◂.com", "filme◂.com",
  "◃.com", "filme◃.com", "◄.com", "filme◄.com",
  "◅.com", "filme◅.com", "◆.com", "filme◆.com",
  "◇.com", "filme◇.com", "◈.com", "filme◈.com",
  "◉.com", "filme◉.com", "◊.com", "filme◊.com",
  "○.com", "filme○.com", "◌.com", "filme◌.com",
  "◍.com", "filme◍.com", "◎.com", "filme◎.com",
  "●.com", "filme●.com", "◐.com", "filme◐.com",
  "◑.com", "filme◑.com", "◒.com", "filme◒.com",
  "◓.com", "filme◓.com", "◔.com", "filme◔.com",
  "◕.com", "filme◕.com", "◖.com", "filme◖.com",
  "◗.com", "filme◗.com", "◘.com", "filme◘.com",
  "◙.com", "filme◙.com", "◚.com", "filme◚.com",
  "◛.com", "filme◛.com", "◜.com", "filme◜.com",
  "◝.com", "filme◝.com", "◞.com", "filme◞.com",
  "◟.com", "filme◟.com", "◠.com", "filme◠.com",
  "◡.com", "filme◡.com", "◢.com", "filme◢.com",
  "◣.com", "filme◣.com", "◤.com", "filme◤.com",
  "◥.com", "filme◥.com", "◦.com", "filme◦.com",
  "◧.com", "filme◧.com", "◨.com", "filme◨.com",
  "◩.com", "filme◩.com", "◪.com", "filme◪.com",
  "◫.com", "filme◫.com", "◬.com", "filme◬.com",
  "◭.com", "filme◭.com", "◮.com", "filme◮.com",
  "◯.com", "filme◯.com", "◰.com", "filme◰.com",
  "◱.com", "filme◱.com", "◲.com", "filme◲.com",
  "◳.com", "filme◳.com", "◴.com", "filme◴.com",
  "◵.com", "filme◵.com", "◶.com", "filme◶.com",
  "◷.com", "filme◷.com", "◸.com", "filme◸.com",
  "◹.com", "filme◹.com", "◺.com", "filme◺.com",
  "◻.com", "filme◻.com", "◼.com", "filme◼.com",
  "◽.com", "filme◽.com", "◾.com", "filme◾.com",
  "◿.com", "filme◿.com", "ⸯ.com", "filmeⸯ.com",
  "々.com", "filme々.com", "〆.com", "filme〆.com",
};

int main(void) {
  for (size_t i = 0; i < sizeof ROTULOS / sizeof *ROTULOS; i++)
    dominio(ROTULOS[i]);
  // nomeados do achado e lacunas herdadas que a mesma regra fecha
  dominio("○○.com");          // dois circulos publicos: xn--11ha.com
  dominio("人々.com");         // a marca encerra o rotulo
  dominio("映画〆.com");
  dominio("example.coⓂ");     // IDNA normaliza para example.com
  dominio("例ⸯ.com");
  dominio("123://exemplo.com");
  caso("Veja ⸯexample.comⸯ agora.", 0);   // til e moldura, nao rotulo
  caso("Veja (...ftp://servidor/filme).", 0);
  caso("Veja (-ftp://servidor/filme).", 0);
  caso("Veja (...rtsp2://servidor/filme).", 0);
  caso("候補はこちら：○exemplo.com○", 0);
  // controles: o que deve seguir passando
  caso("今夜の候補は○で印をつけました。", 1);   // ○ textual legitimo
  caso("Marque ○ ou ● no quadro de hoje.", 1);
  caso("O título é ⸯHello.", 1);              // U+2E2F e letra
  caso("Use apenas o sinal 123://", 1);         // sem letra nao ha esquema
  caso("Exemplo de pontuação ://", 1);
  caso("Veja .com/filme.", 1);                  // branco a esquerda: sem rotulo
  printf("farol_dominio: %d dominios + %d casos, %d falha(s)\n",
         (int)(sizeof ROTULOS / sizeof *ROTULOS), 16, falhas);
  return falhas != 0;
}
