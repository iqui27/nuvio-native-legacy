// Farol — faroltexto_aceita() filtra o texto livre "x" da cena (PLANO-GOSTO
// 4.1, issue 35). Roda a funcao de producao de src/faroltexto.c sobre 74
// frases boas e 99 ruins, uma de cada trava: largura, 3a linha, UTF-8
// quebrado, controle, URL, emoji e cada palavra da lista "nunca diz" do
// ALMA 2 — em pt e em en, com e sem acento.
//
//   cc tests/farol_aceita.c src/faroltexto.c -Isrc -o /tmp/t-aceita && /tmp/t-aceita
#include "faroltexto.h"
#include <stdio.h>
#include <string.h>

static int falhas;
static void caso(const char *s, int espera, const char *porque) {
  int deu = faroltexto_aceita(s);
  if (deu != espera) {
    printf("FALHOU (%s): aceita=%d esperado=%d :: %s\n",
           porque, deu, espera, s ? s : "(null)");
    falhas++;
  }
}

int main(void) {
  int boas = 0, ruins = 0;
  // Duas linhas validas de 62 ideogramas de 4 bytes: 497 bytes antes do
  // NUL — mais que os 382 que a dobra examinava. A cauda fica fora do
  // alcance de quem corta cedo demais.
  static char cauda_palavra[600], cauda_url[600];
  {
    char *w = cauda_palavra;
    for (int i = 0; i < 62; i++) { memcpy(w, "\xF0\xA0\x80\x80", 4); w += 4; }
    *w++ = '\n';
    for (int i = 0; i < 34; i++) { memcpy(w, "\xF0\xA0\x80\x80", 4); w += 4; }
    strcpy(w, " Hello!");
    memcpy(cauda_url, cauda_palavra, (size_t)(w - cauda_palavra));
    strcpy(cauda_url + (w - cauda_palavra), " x.com");
  }
#define BOM(s)   do { caso((s), 1, "boa");   boas++; } while (0)
#define RUIM(s)  do { caso((s), 0, "ruim");  ruins++; } while (0)

  // ------------------------------- 60 boas -------------------------------
  BOM("Separei alguns para você.");
  BOM("Você parou em Dark, no T2E4.");
  BOM("Vale rever.");
  BOM("Do mesmo tema: viagem no tempo.");
  BOM("Porque você terminou Fleabag.");
  BOM("Um amigo seu gostou.");
  BOM("Cabe hoje: 42 min.");
  BOM("Tem áudio em português.");
  BOM("Termina às 23:40.");
  BOM("As temporadas seguem no mesmo nível.");
  BOM("Comparar com…");
  BOM("A dois · leve · até 45 minutos");
  BOM("Tanto faz");
  BOM("Clássico · Dublado · Legendado");
  BOM("Açaí — o filme é ótimo? Não: é bom.");          // acentos e interrogacao
  BOM("É o 2º de 4 — você viu 3.");
  BOM("Foi a sua cara o tempo todo.");                 // "oi" dentro de "foi"
  BOM("Coincidência feliz: a Ana e o Rafa terminaram.");// "oi" de novo
  BOM("Oitavo episódio da temporada.");                // "oi" no inicio da palavra
  BOM("A escolha é sua.");                             // "escolha" sem "otima"
  BOM("O app achou a fonte certa.");                   // ".app" nao e URL aqui
  BOM("A TV pediu atenção na última cena.");           // ".tv" idem
  BOM("Veja em casa o que quiser, na sua hora.");     // dominio sem ponto nao e URL
  BOM("O site ponto com ficou para depois.");        // "ponto com" falado nao e TLD
  BOM("Duas linhas cabem aqui.\nA segunda fecha o pensamento.");
  BOM("Primeira linha curta.\nSegunda igualmente curta.");
  BOM("Você viu em março e deu 4 estrelas.");
  BOM("Está nos seus salvos há 5 meses.");
  BOM("Do diretor de Fleabag, que você viu 2 vezes.");
  BOM("Parecido com Dark, que você terminou.");
  BOM("No seu clima");
  BOM("Para o fim de semana");
  BOM("O que seus amigos gostaram");
  BOM("Gosto zerado.");
  BOM("Certo, não mostro mais.");
  BOM("Fora do seu costume: drama bem avaliado.");
  BOM("Três perguntas e eu acho o que ver.");
  BOM("Faltam 3 episódios para fechar a T2 de Dark.");
  BOM("Você já viu a maior parte.");
  BOM("Temporada nova de Dark chegou.");
  BOM("Curto para hoje: 22 min.");
  BOM("Filme ou série?");
  BOM("Quanto tempo você tem?");
  BOM("Leve ou intenso?");
  BOM("De que época?");
  BOM("Que clima você quer?");
  BOM("Para rir");
  BOM("Para se emocionar");
  BOM("A T2 sobe de nota. Faltam 4 episódios de 45 min.");
  BOM("É o 1º de 3.");
  BOM("Você parou no T1E3 há 7 semanas.");
  BOM("Because you finished Dark.");                    // en tambem passa
  BOM("Worth rewatching.");
  BOM("Season 2 rates higher. 4 episodes left.");
  BOM("Just me");
  BOM("Dubbed or subtitled?");
  BOM("Может, стоит пересмотреть.");                    // cirilico e texto
  BOM("週末にぴったりの映画。");                          // japones idem
  BOM("للمشاهدة الليلة.");                              // arabe idem
  BOM("Duração · Nota · Amigos · Onde ver");
  BOM("O título é ÞOi.");                                // "oi" dentro de ÞOi nao e palavra
  BOM("O título é Łoi.");                                // idem: Ł e letra, nao fronteira
  BOM("O arquivo awww.tmp foi salvo.");                  // "www." no meio da palavra
  BOM("Exemplo de pontuação ://");                      // "://" solto, sem esquema antes
  BOM("A etiqueta diz título.comédia.");                // ".com" colado em letra nao e TLD
  BOM("Marque o×i.");                                    // "×" nao vira "oi" na dobra
  BOM("അവന്‍");                                    // ZWJ e ortografia legitima (malayalam)
  BOM("Curto para hoje Ⓜ");                             // simbolo de apresentacao textual
  BOM("葛󠄀");                                          // seletor ideografico e CJK legitimo
  BOM("今夜の候補は○で印をつけました。");            // ○ textual, nao emoji (U+25CB)
  BOM("Marque ○ ou ● no quadro de hoje.");            // formas sem apresentacao emoji
  BOM("A resenha diz «imperdível».");                 // aspas angulares sao separador
  BOM("O título é ⸯHello.");                          // U+2E2F e letra (Lm), nao separador
  BOM("Use apenas o sinal 123://");                   // sem letra nao ha esquema

  // ------------------------------- 60 ruins ------------------------------
  RUIM(NULL);                                          // nada
  RUIM("");                                            // vazio
  // largura: 63+ caracteres numa linha
  RUIM("Esta linha passa de sessenta e dois caracteres de proposito, veja.");
  RUIM("Uma frase boa de verdade não pode mesmo passar dos sessenta e dois.");
  RUIM("Linha ok.\nEsta segunda linha passa de sessenta e dois caracteres com folga.");
  RUIM("Áéíóú Áéíóú Áéíóú Áéíóú Áéíóú Áéíóú Áéíóú Áéíóú Áéíóú Áéíóú Áéíó."); // acento conta 1
  // terceira linha e linha vazia
  RUIM("Uma.\nDuas.\nTrês linhas não entram.");
  RUIM("Começa bem\nmas a terceira\nlinha estraga tudo");
  RUIM("Texto.\n\nLinha vazia no meio não vale.");
  RUIM("Termina em quebra\n");
  RUIM("\nComeça com quebra.");
  // UTF-8 quebrado
  RUIM("Caf\xc3\x28");                                 // continuacao invalida
  RUIM("Sozinho \x80 aqui");                           // continuacao solta
  RUIM("Overlong \xC0\xAF");                           // '/' em 2 bytes
  RUIM("Surrogate \xED\xA0\x80");                      // U+D800
  RUIM("Fora \xF5\x80\x80\x80");                       // > U+10FFFF
  RUIM("Truncado \xE2\x82");                           // 3 bytes incompleto
  // controle
  RUIM("Tab\tno meio");
  RUIM("Escape\x1B[0m aqui");
  // URL
  RUIM("Veja em http://exemplo.com");
  RUIM("Abre https://nuvio.tv agora");
  RUIM("Entra em www.nuvio.com.br");
  RUIM("Baixa em ftp://ftp.servidor.net/filme");
  // emoji
  RUIM("Vale a pena 😀");
  RUIM("Nota alta ❤️");
  RUIM("Cinco ⭐ de cinco");
  RUIM("Emoção pura 😢");
  RUIM("Confere ✓ aqui");
  RUIM("Relógio ⌚ marcando");
  RUIM("Acessa filmesonline.com e pronto");            // TLD como palavra
  RUIM("Veja em casa.com o que quiser");               // dominio real
  // palavras proibidas do ALMA 2 (acento, maiuscula e vizinhanca variadas)
  RUIM("Desculpe pelo atraso.");
  RUIM("Mil desculpas pelo transtorno.");
  RUIM("Sorry about that.");
  RUIM("I apologize for the wait.");
  RUIM("My apologies.");
  RUIM("Olá! Como vai você?");
  RUIM("Oi, tudo bem?");
  RUIM("Hello! What can I show you?");
  RUIM("Hi there, friend.");
  RUIM("Hey! Olha isso.");
  RUIM("Prontinho, achei.");
  RUIM("There you go!");
  RUIM("All done — enjoy.");
  RUIM("Ótima escolha, aliás.");
  RUIM("Great choice!");
  RUIM("Excellent choice indeed.");
  RUIM("Uau, que filme.");
  RUIM("Wow, amazing.");
  RUIM("Parabéns pelo gosto.");
  RUIM("Uso inteligência artificial para achar.");
  RUIM("Powered by artificial intelligence.");
  RUIM("Um erro inesperado ocorreu.");
  RUIM("Falha inesperada na fonte.");
  RUIM("An unexpected error occurred.");
  RUIM("Estou aqui para ajudar você.");
  RUIM("How can I help you today?");
  RUIM("How may I help?");
  RUIM("Ficou tudo certinho agora.");
  RUIM("a palavra OI no meio");
  // atras do antigo corte da dobra (382 bytes): duas linhas de pontos de
  // codigo de 4 bytes deixavam a cauda sem exame — " Hello!" e " x.com"
  // no fim da segunda linha passavam
  RUIM(cauda_palavra);
  RUIM(cauda_url);
  // frase proibida quebrada entre linhas ou com espaco duplo
  RUIM("Great\nchoice!");
  RUIM("Great  choice!");
  // dominios que a lista de TLDs nao cobria
  RUIM("Veja em ard.de/filme.");                          // ccTLD de 2 letras
  RUIM("Veja em nü.de.");                                 // idem, apos a dobra de ü
  RUIM("Veja em 电影.com.");                              // rotulo nao ascii + TLD
  // palavra proibida com acento solto (NFD): E + U+0301 vira "É"
  RUIM("PARABE\xCC\x81NS!");
  // jargao cru e culpa ao usuario, exemplos literais do ALMA 2
  RUIM("HTTP 206 Partial Content");
  RUIM("Sua internet está ruim.");
  // controles e separadores que nao contavam
  RUIM("Curto\xC2\x85para hoje.");                        // NEL (C1)
  RUIM("Uma\xE2\x80\xA8" "duas\xE2\x80\xA8" "três");      // U+2028 = quebra (3 linhas)
  RUIM(" \nCurto para hoje.");                            // linha so de espacos
  RUIM("Curto para hoje \xE2\x97\xBD");                   // ◽ emoji de apresentacao padrao
  RUIM("Curto para hoje ▪");                              // U+25AA idem
  // aspas angulares e pontuacao latina sao separador, nao letra: nao
  // reabrem a fronteira de URL nem de palavra proibida
  RUIM("Veja «exemplo.com» para saber mais.");
  RUIM("Veja «www.exemplo.org» para saber mais.");
  RUIM("«Hello!» Uma sugestão para hoje.");
  RUIM("¡Hello! Uma sugestão para hoje.");
  RUIM("¿Hello! Uma sugestão para hoje.");
  RUIM("Veja rtsp2://servidor/filme.");                   // esquema que termina em digito
  // pontuacao de abertura nao protege o esquema: a faixa [a-z0-9+.-]
  // inteira conta — "...ftp://" e "-ftp://" sao URL do mesmo jeito
  RUIM("Veja (...ftp://servidor/filme).");
  RUIM("Veja (-ftp://servidor/filme).");
  RUIM("Veja (...rtsp2://servidor/filme).");
  // o circulo textual e separador, nao letra: nao esconde dominio,
  // "www." nem saudacao entre os dois circulos
  RUIM("候補はこちら：○exemplo.com○");
  RUIM("候補はこちら：○www.exemplo.org○");
  RUIM("候補はこちら：○Hello!○");
  // pontuacao arabe tambem e separador
  RUIM("شاهد هنا،www.exemplo.com، ثم اختر.");
  RUIM("هل نجرب؟Hello! الفيلم جاهز.");
  // separador tokeniza palavra, nao apaga rotulo: sinal imediatamente a
  // esquerda do ponto ainda conta como rotulo — dominio sem esquema que
  // termina em pontuacao ou geometrico segue fora
  RUIM("Veja ○○.com/filme.");
  RUIM("Veja ●.com/filme.");
  RUIM("Veja filme○.com/filme.");
  RUIM("Veja ֆիլմ։.com/filme.");
  RUIM("Veja ֆילմ֊.com/filme.");
  RUIM("Veja filme،.com/filme.");
  // lacunas herdadas que a mesma regra fecha: a marca encerra o rotulo
  RUIM("Veja 人々.com/filme.");
  RUIM("Veja 映画〆.com/filme.");
  RUIM("Veja example.coⓂ/filme.");              // IDNA: example.com
  // U+2E2F segue letra, mas nao alonga o TLD: dominio emoldurado e dominio
  RUIM("Veja ⸯexample.comⸯ agora.");

  printf("farol_aceita: %d boas aceitas, %d ruins recusadas", boas, ruins);
  if (boas != 74 || ruins != 99) {
    printf(" — ERRO: esperado 74+99\n");
    return 1;
  }
  if (falhas) { printf(" — %d falha(s)\n", falhas); return 1; }
  printf("\n");
  return 0;
}
