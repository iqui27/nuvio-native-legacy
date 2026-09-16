// AGENDA: leitura do corpo /tv/<id> do TMDB, a frase que a tela de titulo
// escreve, e os lembretes em disco.
//
//   bash tests/agenda.sh
//
// Inclui src/agenda.c de proposito, como tests/atualizacao.c faz: `lerCorpoTv`
// e estatico — e o parse real, o mesmo que extras.c executa com o corpo vindo
// da rede — e expo-lo so para o teste seria API a mais. O .sh compila tudo
// MENOS src/agenda.c.
//
// ONDE ESTE TESTE ESCREVE, e por que ele confere isso antes de qualquer coisa:
// um teste deste repositorio ja sobrescreveu os dados reais do dono. Aqui o .sh
// exporta NUVIO_DADOS para uma pasta temporaria e a primeira conferencia e que
// dados_dir() e EXATAMENTE ela. Se nao for, o teste aborta sem escrever nada.
#include "../src/agenda.c"
#include "perfis.h"
#include "ajustes.h"
#include <assert.h>

static int falhas = 0;
#define CONFERE(c, ...) do { if (!(c)) { falhas++; printf("FALHA: " __VA_ARGS__); printf("\n"); } } while (0)

static char *lerArquivo(const char *nome) {
  FILE *f = fopen(nome, "rb"); long n; char *s; size_t lido;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  s = malloc((size_t)n + 1);
  if (!s) { fclose(f); return NULL; }
  lido = fread(s, 1, (size_t)n, f); s[lido] = 0; fclose(f);
  return s;
}

// "Reiniciar o app": esquece o que esta na RAM e obriga a releitura do disco.
// E o unico jeito honesto de provar que o lembrete SOBREVIVEU — conferir a
// variavel que acabou de ser escrita nao prova nada sobre o arquivo.
static void reiniciar(void) {
  perfilCarregado = -1;
  nCache = 0;
  nLembretes = 0;
  nLista = 0;
  cacheSujo = 0;
  agenda_iniciar();
}

int main(void) {
  const char *dirEsperado = getenv("NUVIO_DADOS");
  char *j;

  dados_iniciar(".");

  // --- A CONFERENCIA QUE VEM ANTES DE TUDO ---------------------------------
  if (!dirEsperado || !dirEsperado[0]) {
    printf("FALHA: NUVIO_DADOS nao esta definida. Rode por tests/agenda.sh.\n");
    return 1;
  }
  if (strcmp(dados_dir(), dirEsperado)) {
    printf("FALHA: dados_dir() e [%s], esperado [%s]. Abortando ANTES de escrever.\n",
           dados_dir(), dirEsperado);
    return 1;
  }
  printf("dados em %s\n", dados_dir());

  // Relogio cravado: sem isto o teste passa hoje e falha em duas semanas, e a
  // falha nao seria do codigo.
  agenda_definir_hoje("2026-09-16");

  // --- datas ---------------------------------------------------------------
  CONFERE(agenda_dias("2026-09-16") == 0, "hoje = 0");
  CONFERE(agenda_dias("2026-09-17") == 1, "amanha = 1");
  CONFERE(agenda_dias("2026-09-19") == 3, "19/09 = 3 dias");
  CONFERE(agenda_dias("2026-09-12") == -4, "12/09 = -4 dias");
  CONFERE(agenda_dias("2026-10-16") == 30, "um mes = 30 dias");
  CONFERE(agenda_dias("2027-09-16") == 365, "um ano = 365 dias");
  CONFERE(agenda_dias("") == AG_SEM_DATA, "vazio nao e data");
  CONFERE(agenda_dias("a definir") == AG_SEM_DATA, "texto nao e data");
  // Virada de ano e ano bissexto: a conta e civil, nao mktime.
  agenda_definir_hoje("2026-12-31");
  CONFERE(agenda_dias("2027-01-01") == 1, "31/12 -> 01/01 = 1 dia");
  agenda_definir_hoje("2028-02-28");
  CONFERE(agenda_dias("2028-03-01") == 2, "2028 e bissexto: 28/02 -> 01/03 = 2");
  agenda_definir_hoje("2026-09-16");

  // AS CONFERENCIAS DE TEXTO SAO CONTRA O i18n, e nao contra a palavra em
  // portugues cravada aqui. O app roda nos dois idiomas e a tabela de traducao
  // e mesclada por fora deste trabalho; comparar com "hoje" faria o teste
  // quebrar no dia em que a chave entrar na tabela e a build estiver em ingles.
  // O que se prova e o CAMINHO (a chave certa, a funcao certa), nao a palavra.
  { char q[80], esp[80];
    printf("idioma da build: %s\n", ajustes_idioma_ingles() ? "en" : "pt");
    agenda_quando("2026-09-16", q, sizeof q);
    CONFERE(!strcmp(q, i18n("hoje")), "quando hoje: [%s]", q);
    agenda_quando("2026-09-17", q, sizeof q);
    CONFERE(!strcmp(q, i18n("amanhã")), "quando amanha: [%s]", q);
    agenda_quando("2026-09-19", q, sizeof q);
    snprintf(esp, sizeof esp, i18n("em %d dias"), 3);
    CONFERE(!strcmp(q, esp), "quando em 3 dias: [%s] vs [%s]", q, esp);
    // Longe: a data por extenso, pela MESMA desc_data_extenso do resto do app.
    // Esta e a conferencia que garante que nao nasceu um segundo formatador de
    // data aqui — dois divergem, e divergem em silencio.
    agenda_quando("2026-11-04", q, sizeof q);
    desc_data_extenso("2026-11-04", esp, sizeof esp);
    CONFERE(!strcmp(q, esp), "quando longe = desc_data_extenso: [%s] vs [%s]", q, esp);
    CONFERE(!strchr(q, '/'), "sem data numerica: [%s]", q);
    if (!ajustes_idioma_ingles())
      CONFERE(!strcmp(q, "4 de novembro de 2026"), "por extenso em pt: [%s]", q); }

  // --- as pecas do calendario ----------------------------------------------
  // A linha do tempo desenha o numeral do dia, o mes como cabecalho e o dia da
  // semana em apoio. Cada peca e um recorte da data, e errar uma poe o numeral
  // de um dia sob o nome de outro mes.
  CONFERE(agenda_dia("2026-09-16") == 16, "dia");
  CONFERE(agenda_mes("2026-09-16") == 9, "mes");
  CONFERE(agenda_ano("2026-09-16") == 2026, "ano");
  CONFERE(agenda_dia("nao e data") == 0, "dia de entrada invalida");
  // 16/09/2026 e uma QUARTA. Conferido contra o calendario, nao contra a
  // funcao: uma formula de dia da semana testada com ela mesma passa errada.
  CONFERE(agenda_semana("2026-09-16") == 3, "quarta, veio %d", agenda_semana("2026-09-16"));
  CONFERE(agenda_semana("2026-09-20") == 0, "domingo, veio %d", agenda_semana("2026-09-20"));
  CONFERE(agenda_semana("2026-09-19") == 6, "sabado, veio %d", agenda_semana("2026-09-19"));
  CONFERE(agenda_semana("lixo") == -1, "semana de entrada invalida");
  CONFERE(!strcmp(agenda_mes_nome(9), "setembro"), "nome do mes 9: [%s]", agenda_mes_nome(9));
  CONFERE(agenda_mes_nome(0)[0] == 0 && agenda_mes_nome(13)[0] == 0, "mes fora da faixa");
  CONFERE(agenda_semana_nome(-1)[0] == 0 && agenda_semana_nome(7)[0] == 0, "semana fora da faixa");

  // --- quanto falta, em forma curta para QUALQUER distancia ----------------
  // Diferente de agenda_quando: a partir de uma semana ela devolve a data por
  // extenso, que na linha do tempo ja esta desenhada no numeral ao lado.
  { char f[64];
    agenda_falta("2026-09-16", f, sizeof f);
    CONFERE(!strcmp(f, i18n("hoje")), "falta hoje: [%s]", f);
    agenda_falta("2026-09-17", f, sizeof f);
    CONFERE(!strcmp(f, i18n("amanhã")), "falta amanha: [%s]", f);
    agenda_falta("2026-09-20", f, sizeof f);
    CONFERE(strstr(f, "4") != NULL, "falta 4 dias: [%s]", f);
    // 04/11/2026 sao 49 dias: SETE semanas cravadas.
    agenda_falta("2026-11-04", f, sizeof f);
    CONFERE(strstr(f, "7") != NULL, "falta 7 semanas: [%s]", f);
    printf("falta (04/11): %s\n", f);
    // 16/03/2027 sao 181 dias: seis meses. Nao pode cair em "em 25 semanas".
    agenda_falta("2027-03-16", f, sizeof f);
    CONFERE(strstr(f, "6") != NULL, "falta 6 meses: [%s]", f);
    printf("falta (16/03/27): %s\n", f);
    // Data que JA PASSOU nao tem espera: a estacao dela mostra situacao, nao
    // contagem. Devolver "em -3 dias" seria o metadado inventado de sempre.
    agenda_falta("2026-09-10", f, sizeof f);
    CONFERE(f[0] == 0, "passado nao tem falta: [%s]", f);
    agenda_falta("lixo", f, sizeof f);
    CONFERE(f[0] == 0, "entrada invalida nao tem falta: [%s]", f); }

  // --- situacao, nas duas grafias ------------------------------------------
  CONFERE(agenda_situacao_de("Returning Series") == AG_VOLTANDO, "TMDB Returning Series");
  CONFERE(agenda_situacao_de("returning series") == AG_VOLTANDO, "Trakt returning series");
  CONFERE(agenda_situacao_de("continuing") == AG_VOLTANDO, "Trakt continuing");
  CONFERE(agenda_situacao_de("Ended") == AG_ENCERRADA, "TMDB Ended");
  CONFERE(agenda_situacao_de("ended") == AG_ENCERRADA, "Trakt ended");
  CONFERE(agenda_situacao_de("Canceled") == AG_CANCELADA, "TMDB Canceled");
  CONFERE(agenda_situacao_de("Cancelled") == AG_CANCELADA, "grafia com dois eles");
  CONFERE(agenda_situacao_de("In Production") == AG_PRODUCAO, "In Production");
  CONFERE(agenda_situacao_de("Planned") == AG_PRODUCAO, "Planned");
  CONFERE(agenda_situacao_de("") == AG_DESCONHECIDA, "vazio = desconhecida");
  CONFERE(agenda_situacao_de("Quem Sabe") == AG_DESCONHECIDA, "status novo = desconhecida");

  // --- SERIE QUE VOLTA: a data sai do corpo, sem pedido nenhum -------------
  j = lerArquivo("tests/fixtures/tmdb_tv_voltando.json");
  CONFERE(j != NULL, "fixture tmdb_tv_voltando.json");
  if (j) {
    const AgItem *r;
    char frase[200];
    lerCorpoTv("tt10255564", "Fundação", j);
    free(j);
    r = agenda_registro("tt10255564");
    CONFERE(r != NULL, "registrou a serie que volta");
    if (r) {
      CONFERE(!strcmp(r->dataProx, "2026-09-19"), "air_date do proximo: [%s]", r->dataProx);
      CONFERE(r->temporada == 3, "temporada 3, veio %d", r->temporada);
      CONFERE(r->episodio == 9, "episodio 9, veio %d", r->episodio);
      CONFERE(!strcmp(r->nomeEp, "The Last Empress"), "nome do episodio: [%s]", r->nomeEp);
      // O ULTIMO EPISODIO NAO PODE VIRAR O PROXIMO. Os dois blocos tem
      // `air_date` e `name`; ler o errado poria 12/09 (passado) como estreia.
      CONFERE(!strcmp(r->dataUlt, "2026-09-12"), "ultimo ao ar: [%s]", r->dataUlt);
      CONFERE(r->situacao == AG_VOLTANDO, "situacao voltando, veio %d", r->situacao);
      // OS CINCO CAMPOS QUE O PARSE JOGAVA FORA. Saem do MESMO corpo e sao o
      // que a linha do tempo da Agenda desenha. Um teste por campo porque cada
      // um sai de um lugar diferente do JSON, e quem quebrar um so nao quebra
      // os outros — o sintoma seria uma linha da tela ficando em branco.
      CONFERE(!strcmp(r->sinopse, "O Império responde."), "sinopse do proximo: [%s]", r->sinopse);
      CONFERE(!strcmp(r->tipoEp, "standard"), "episode_type do proximo: [%s]", r->tipoEp);
      CONFERE(!strcmp(r->rede, "Apple TV+"), "networks[0].name: [%s]", r->rede);
      // runtime do EPISODIO, e nao episode_run_time — que nesta fixture e "[]",
      // o caso real de uma serie cujo TMDB nao tem duracao media.
      CONFERE(r->duracao == 58, "runtime do proximo, veio %d", r->duracao);
      CONFERE(r->temporadas == 3, "number_of_seasons, veio %d", r->temporadas);
      // "standard" NAO e marco: um episodio comum nao tem nada a anunciar, e
      // escrever "Episódio" ali seria ruido com cara de dado.
      { char m[80];
        CONFERE(!agenda_marco(r, m, sizeof m), "standard nao vira marco: [%s]", m); }
      { char ap[200];
        agenda_apoio(r, ap, sizeof ap);
        CONFERE(strstr(ap, "Apple TV+") != NULL, "apoio traz a rede: [%s]", ap);
        CONFERE(strstr(ap, "58") != NULL, "apoio traz a duracao: [%s]", ap);
        printf("apoio (volta): %s\n", ap); }
      // A SINOPSE ATRAVESSA O DISCO. O cache e TSV e os campos novos foram para
      // o FIM da linha; se a gravacao e a leitura discordarem, a tela sai sem
      // sinopse e ninguem descobre ate olhar uma captura.
      { const AgItem *r2;
        reiniciar();
        r2 = agenda_registro("tt10255564");
        CONFERE(r2 && !strcmp(r2->sinopse, "O Império responde."),
                "sinopse sobreviveu ao disco: [%s]", r2 ? r2->sinopse : "(nulo)");
        CONFERE(r2 && r2->duracao == 58 && r2->temporadas == 3,
                "duracao e temporadas sobreviveram ao disco"); }
    }
    CONFERE(agenda_frase("tt10255564", frase, sizeof frase), "ha frase para a serie que volta");
    printf("frase (volta): %s\n", frase);
    { char esp[200], q[80];
      agenda_quando("2026-09-19", q, sizeof q);
      snprintf(esp, sizeof esp, i18n("Próximo episódio T%dE%d · %s"), 3, 9, q);
      CONFERE(!strcmp(frase, esp), "frase do proximo: [%s] vs [%s]", frase, esp); }
    // O id COMPOSTO do episodio ("tt...:3:9") tem de cair no mesmo registro.
    CONFERE(agenda_registro("tt10255564:3:9") != NULL, "id composto acha a serie");
  }

  // --- SERIE ENCERRADA: diz que acabou, NAO mostra data de estreia ---------
  j = lerArquivo("tests/fixtures/tmdb_tv_encerrada.json");
  CONFERE(j != NULL, "fixture tmdb_tv_encerrada.json");
  if (j) {
    const AgItem *r;
    char frase[200];
    lerCorpoTv("tt0903747", "Breaking Bad", j);
    free(j);
    r = agenda_registro("tt0903747");
    CONFERE(r != NULL, "registrou a serie encerrada");
    if (r) {
      // ESTA E A CONFERENCIA CENTRAL DO TRABALHO: next_episode_to_air e null,
      // e o registro TEM de ficar sem data. Cair no `last_episode_to_air`
      // aqui mostraria "próximo episódio em 2013" numa serie que acabou.
      CONFERE(r->dataProx[0] == 0, "encerrada nao tem proxima data: [%s]", r->dataProx);
      CONFERE(r->situacao == AG_ENCERRADA, "situacao encerrada, veio %d", r->situacao);
      CONFERE(!strcmp(r->dataUlt, "2013-09-29"), "ultimo ao ar: [%s]", r->dataUlt);
      // SEM PROXIMO, a sinopse e o tipo vem do ULTIMO — e a unica coisa
      // verdadeira que se pode dizer de uma serie que acabou.
      CONFERE(!strcmp(r->sinopse, "O acerto de contas."), "sinopse do ultimo: [%s]", r->sinopse);
      CONFERE(r->duracao == 55, "runtime do ultimo, veio %d", r->duracao);
      { char m[80];
        CONFERE(agenda_marco(r, m, sizeof m), "finale vira marco");
        printf("marco (encerrada): %s\n", m); }
    }
    CONFERE(agenda_frase("tt0903747", frase, sizeof frase), "ha frase para a encerrada");
    printf("frase (encerrada): %s\n", frase);
    { char esp[200], q[80];
      agenda_quando("2013-09-29", q, sizeof q);
      snprintf(esp, sizeof esp, i18n("Série encerrada · último episódio em %s"), q);
      // A frase TEM de ser a do ramo "encerrada" — nao a do proximo episodio
      // com a data do ultimo no lugar dela, que e o erro que se quer impedir.
      CONFERE(!strcmp(frase, esp), "frase da encerrada: [%s] vs [%s]", frase, esp); }
    { char esp[200], q[80];
      agenda_quando("2013-09-29", q, sizeof q);
      snprintf(esp, sizeof esp, i18n("Próximo episódio T%dE%d · %s"), 5, 16, q);
      CONFERE(strcmp(frase, esp) != 0, "nao promete proximo episodio: [%s]", frase); }
    CONFERE(!agenda_pode_lembrar("tt0903747"), "encerrada nao ganha botao de lembrete");
  }

  // --- SERIE CANCELADA -----------------------------------------------------
  j = lerArquivo("tests/fixtures/tmdb_tv_cancelada.json");
  CONFERE(j != NULL, "fixture tmdb_tv_cancelada.json");
  if (j) {
    const AgItem *r;
    char frase[200];
    lerCorpoTv("tt9999991", "Série Cancelada", j);
    free(j);
    r = agenda_registro("tt9999991");
    CONFERE(r && r->dataProx[0] == 0, "cancelada nao tem proxima data");
    CONFERE(r && r->situacao == AG_CANCELADA, "situacao cancelada");
    CONFERE(agenda_frase("tt9999991", frase, sizeof frase), "ha frase para a cancelada");
    printf("frase (cancelada): %s\n", frase);
    { char esp[200], q[80];
      agenda_quando("2023-05-26", q, sizeof q);
      snprintf(esp, sizeof esp, i18n("Série cancelada · último episódio em %s"), q);
      CONFERE(!strcmp(frase, esp), "frase da cancelada: [%s] vs [%s]", frase, esp); }
    CONFERE(!agenda_pode_lembrar("tt9999991"), "cancelada nao ganha botao de lembrete");
  }

  // Serie que volta mas nao tem data anunciada: a frase existe e NAO inventa
  // dia nenhum. E o caso que o TMDB devolve entre temporadas.
  agenda_registrar("tt7777777", "Entre temporadas", "", "Returning Series",
                   0, 0, "", "", "2026-05-02");
  { char frase[200];
    CONFERE(agenda_frase("tt7777777", frase, sizeof frase), "ha frase para 'sem data'");
    printf("frase (sem data): %s\n", frase);
    CONFERE(!strcmp(frase, i18n("Sem data anunciada para o próximo episódio")),
            "diz que nao ha data: [%s]", frase);
    CONFERE(!agenda_pode_lembrar("tt7777777"), "sem data nao ganha botao"); }

  // Titulo desconhecido: NAO ha frase. A linha some, em vez de inventar.
  { char frase[200];
    CONFERE(!agenda_frase("tt0000001", frase, sizeof frase), "titulo sem registro nao tem frase");
    CONFERE(frase[0] == 0, "e a frase sai vazia"); }

  // --- LEMBRETE: liga, e sobrevive a um reinicio ---------------------------
  CONFERE(agenda_pode_lembrar("tt10255564"), "data futura permite lembrete");
  CONFERE(agenda_alternar_lembrete("tt10255564") == 1, "ligou o lembrete");
  CONFERE(agenda_lembrete("tt10255564") == 1, "lembrete ligado em memoria");
  reiniciar();
  CONFERE(agenda_lembrete("tt10255564") == 1, "lembrete SOBREVIVEU ao reinicio");
  { const AgItem *r = agenda_registro("tt10255564");
    CONFERE(r != NULL, "o cache tambem sobreviveu");
    CONFERE(r && !strcmp(r->dataProx, "2026-09-19"),
            "com a data: [%s]", r ? r->dataProx : "(nulo)"); }

  // Desligar tambem grava.
  CONFERE(agenda_alternar_lembrete("tt10255564") == 0, "desligou o lembrete");
  reiniciar();
  CONFERE(agenda_lembrete("tt10255564") == 0, "desligado SOBREVIVEU ao reinicio");
  agenda_alternar_lembrete("tt10255564");   // liga de novo para o resto

  // --- ISOLAMENTO POR PERFIL ----------------------------------------------
  // O perfil 2 nao pode enxergar nem o lembrete nem o calendario do perfil 1.
  // Numa TV de sala isto nao e detalhe: e a lista de series de outra pessoa.
  CONFERE(perfis_ativo() == 1, "o teste comeca no perfil 1 (veio %d)", perfis_ativo());
  perfis_definir_ativo(2);
  reiniciar();
  CONFERE(agenda_lembrete("tt10255564") == 0, "perfil 2 NAO ve o lembrete do perfil 1");
  CONFERE(agenda_registro("tt10255564") == NULL, "perfil 2 NAO ve o cache do perfil 1");
  agenda_registrar("tt1520211", "The Walking Dead", "", "Ended", 0, 0, "", "",
                   "2022-11-20");
  CONFERE(agenda_registro("tt1520211") != NULL, "perfil 2 grava o seu");

  perfis_definir_ativo(1);
  reiniciar();
  CONFERE(agenda_lembrete("tt10255564") == 1, "perfil 1 recupera o seu lembrete");
  CONFERE(agenda_registro("tt1520211") == NULL, "perfil 1 NAO ve o que o perfil 2 gravou");

  // --- ADIAMENTO: data nova zera o aviso ----------------------------------
  // Se o TMDB adiar o episodio, o lembrete continua valendo e o aviso volta a
  // estar por dar. Sem isto, um adiamento avisaria no dia velho e nunca mais.
  agenda_marcar_avisado("tt10255564");
  { int i, avisado = -1;
    for (i = 0; i < nLembretes; i++)
      if (!strcmp(lembretes[i].imdb, "tt10255564")) avisado = lembretes[i].avisado;
    CONFERE(avisado == 1, "marcou como avisado"); }
  agenda_registrar("tt10255564", NULL, NULL, "Returning Series", 3, 9,
                   "The Last Empress", "2026-09-26", "2026-09-12");
  { int i, avisado = -1;
    char data[12] = "";
    for (i = 0; i < nLembretes; i++)
      if (!strcmp(lembretes[i].imdb, "tt10255564")) {
        avisado = lembretes[i].avisado;
        snprintf(data, sizeof data, "%s", lembretes[i].data);
      }
    CONFERE(avisado == 0, "data nova zera o avisado");
    CONFERE(!strcmp(data, "2026-09-26"), "e adota a data nova: [%s]", data); }

  // --- LEMBRETE VENCIDO ----------------------------------------------------
  // O calendario e montado do catalogo, e o teste roda sem catalogo nenhum: o
  // que sustenta a linha aqui e a terceira fonte de agenda_montar — quem tem
  // lembrete entra na lista mesmo sem o catalogo ter publicado.
  agenda_registrar("tt10255564", "Fundação", "", "Returning Series", 3, 9,
                   "The Last Empress", "2026-09-16", "2026-09-12");
  agenda_montar();
  { const AgItem *dev[4];
    int q = agenda_devidos(dev, 4);
    CONFERE(q == 1, "um lembrete vencido hoje, veio %d", q);
    if (q == 1) CONFERE(!strcmp(dev[0]->imdb, "tt10255564"), "e o certo");
    agenda_marcar_avisado("tt10255564");
    q = agenda_devidos(dev, 4);
    CONFERE(q == 0, "avisado uma vez so, veio %d", q); }

  // Data ainda futura nao vence.
  agenda_registrar("tt10255564", NULL, NULL, "Returning Series", 3, 9,
                   "The Last Empress", "2026-09-19", "2026-09-12");
  agenda_montar();
  { const AgItem *dev[4];
    CONFERE(agenda_devidos(dev, 4) == 0, "data futura nao vence"); }

  // --- ORDEM DO CALENDARIO -------------------------------------------------
  // Quem tem data vem antes de quem nao tem, da mais proxima para a mais
  // distante. E o que faz a tela ser um calendario e nao uma lista.
  agenda_registrar("tt2222222", "Depois", "", "Returning Series", 1, 2, "",
                   "2026-10-20", "");
  agenda_registrar("tt3333333", "Antes", "", "Returning Series", 1, 5, "",
                   "2026-09-17", "");
  agenda_registrar("tt4444444", "Acabou", "", "Ended", 0, 0, "", "", "2020-01-01");
  { int i;
    for (i = 0; i < nLembretes; i++) { }   /* nada: so os lembretes puxam a lista */
    agenda_alternar_lembrete("tt2222222");
    agenda_alternar_lembrete("tt3333333");
    agenda_montar();
    CONFERE(agenda_n() >= 3, "o calendario montou %d linhas", agenda_n());
    { int pA = -1, pD = -1, pF = -1;
      for (i = 0; i < agenda_n(); i++) {
        const AgItem *it = agenda_lista(i);
        if (!strcmp(it->imdb, "tt3333333")) pA = i;
        if (!strcmp(it->imdb, "tt2222222")) pD = i;
        if (!strcmp(it->imdb, "tt4444444")) pF = i;
      }
      CONFERE(pA >= 0 && pD >= 0, "as duas com data entraram");
      CONFERE(pA < pD, "17/09 antes de 20/10 (%d < %d)", pA, pD);
      if (pF >= 0) CONFERE(pD < pF, "sem data depois das com data (%d < %d)", pD, pF); } }

  if (falhas) { printf("FALHOU: %d\n", falhas); return 1; }
  puts("PASS: agenda (datas, situacao, parse do TMDB, lembretes, perfis).");
  return 0;
}
