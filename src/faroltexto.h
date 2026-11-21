// Farol — todas as falas, num arquivo so.
//
// POR QUE UM ARQUIVO SO. As ~110 frases do Farol (vereditos da tabela 2.4 do
// PLANO, telas da 3.2, estados da 3.3, consertos calados do ALMA 5 e as falas
// novas das issues 29-34 do ADENDO) saem de dezenas de issues paralelas. Se
// cada uma escrevesse o proprio literal, duas telas descreveriam a mesma falha
// com palavras diferentes e a revisao de tom do ALMA nao fecharia nunca. Com
// FT_* qualquer tela pede a frase pelo nome e a chave de traducao e uma so.
//
// A CHAVE E O PORTUGUES, como em todo o app (idioma.h): FT_X ja e a chave que
// i18n() procura em idioma_tab.h. Frase com dado variavel leva marcador de
// printf (%s, %d) — o dado nunca e colado no meio de um literal novo, senao a
// busca da tabela erra calada (FERRAMENTAS.md).
//
// REGRAS DE ESCRITA (conferidas por tests/farol_textos.sh): no maximo 2 linhas
// por fala, cada linha com no maximo 62 caracteres, primeira pessoa quieta
// ("Separei", "Testei"), sem saudacao de bot, sem desculpa corporativa e sem
// nenhuma palavra da lista "nunca diz" do ALMA ("Desculpe", "Ola", "Prontinho",
// "inteligencia artificial", "inesperado").
//
// FAROL_FRASES(X) lista todos os nomes: o teste de C enumera as constantes
// por ele, e tools/idiomas.py confere cada literal nas 29 tabelas-irmas.
#ifndef NV_FAROLTEXTO_H
#define NV_FAROLTEXTO_H

#define FAROL_FRASES(X) \
  X(NOME) \
  X(ACHAR_O_QUE_VER) \
  X(AJUDA_KICKER) \
  X(BANNER_FALHA) \
  X(SINTOMA_CORES) \
  X(SINTOMA_CORES_DESC) \
  X(INTENCAO_CURTO) \
  X(INTENCAO_NAO_VI) \
  X(INTENCAO_RETOMAR) \
  X(INTENCAO_SURPRESA) \
  X(REGISTROS_ENVIADOS) \
  X(NAO_TESTADO) \
  X(TESTANDO) \
  X(A_CAUSA_ENCONTRADA) \
  X(RESOLVER_NOME) \
  X(PILULA_FALHA) \
  X(ABRE_O_FAROL) \
  X(OK_ABRE_O_FAROL) \
  X(FONTE_TROCADA) \
  X(FONTE_TROCADA_SIMPLES) \
  X(AUDIO_CONVERTIDO) \
  X(LEGENDA_RESSINC) \
  X(ADDON_ISOLADO) \
  X(VOLTOU_A_TOCAR) \
  X(REDE_RECONECTADA) \
  X(CACHE_LIMPO) \
  X(HDR_DESLIGADO) \
  X(MEMORIA_LIBERADA) \
  X(OBSERVANDO_VIDEO) \
  X(REDE_FORA_L1) \
  X(REDE_FORA_L2) \
  X(REDE_NAO_CHEGA_L1) \
  X(REDE_NAO_CHEGA_L2) \
  X(ADDON_NENHUM_L1) \
  X(ADDON_NENHUM_L2) \
  X(ADDON_LENTO_L1) \
  X(ADDON_LENTO_L2) \
  X(ADDON_LENTO_NOME) \
  X(DEBRID_SEM_PLANO_L1) \
  X(DEBRID_SEM_PLANO_L2) \
  X(DEBRID_DESCONECTOU_L1) \
  X(DEBRID_DESCONECTOU_L2) \
  X(FONTE_RECUSOU_L1) \
  X(FONTE_RECUSOU_L2) \
  X(FONTE_SEM_RANGE_L1) \
  X(FONTE_SEM_RANGE_L2) \
  X(REDE_CURTA_L1) \
  X(REDE_CURTA_L2) \
  X(TV_SEM_MEMORIA_L1) \
  X(TV_SEM_MEMORIA_L2) \
  X(TV_NEGOU_PLAYER_L1) \
  X(TV_NEGOU_PLAYER_L2) \
  X(AUDIO_DTS_L1) \
  X(AUDIO_DTS_L2) \
  X(AUDIO_OUTRA_FAIXA_L1) \
  X(AUDIO_OUTRA_FAIXA_L2) \
  X(AUDIO_SEM_SAIDA_L1) \
  X(AUDIO_SEM_SAIDA_L2) \
  X(IMAGEM_DV_L1) \
  X(IMAGEM_DV_L2) \
  X(TV_FORMATO_L1) \
  X(TV_FORMATO_L2) \
  X(TV_FORMATO_REPETE_L1) \
  X(TV_FORMATO_REPETE_L2) \
  X(PLAYER_PAROU_L1) \
  X(PLAYER_PAROU_L2) \
  X(NADA_ACHADO_L1) \
  X(NADA_ACHADO_L2) \
  X(ACAO_TENTAR) \
  X(ACAO_PROXIMA_FONTE) \
  X(ACAO_RECONECTAR_REDE) \
  X(ACAO_ABRIR_FONTES) \
  X(ACAO_ISOLAR) \
  X(ACAO_ABRIR_DEBRID) \
  X(ACAO_DEBRID_CELULAR) \
  X(ACAO_FONTE_MENOR) \
  X(ACAO_ALIVIAR) \
  X(ACAO_MANDAR_REGISTRO) \
  X(ACAO_ENVIAR_REGISTRO) \
  X(ACAO_CONVERTER) \
  X(ACAO_TROCAR_FAIXA) \
  X(ACAO_FONTE_SEM_DTS) \
  X(ACAO_FONTE_SEM_DV) \
  X(ACAO_ESCONDER_DV) \
  X(ACAO_PLAYER_EXTERNO) \
  X(ACAO_FONTE_FORMATO) \
  X(ACAO_BUFFER_MAIOR) \
  X(ACAO_LIMPAR_CACHE) \
  X(ACAO_DESLIGAR_HDR) \
  X(ACAO_DESLIGAR_HDR_SEMPRE) \
  X(ACAO_DESFAZER) \
  X(ACAO_MANDAR) \
  X(ACAO_AGORA_NAO) \
  X(ACAO_PRONTO) \
  X(ACAO_FECHAR) \
  X(ACAO_VOLTAR) \
  X(ACAO_LIGADO) \
  X(ACAO_SO_QUANDO_ABRIR) \
  X(ACAO_FALAR) \
  X(ACAO_DIGITAR_CELULAR) \
  X(ACAO_TECLADO_TV) \
  X(VAI_JUNTO) \
  X(NUNCA_VAI) \
  X(VAI_MODELO) \
  X(VAI_VERSAO) \
  X(VAI_FORMATO) \
  X(VAI_PONTO) \
  X(VAI_AJUSTES) \
  X(VAI_EVENTOS) \
  X(NUNCA_TITULOS) \
  X(NUNCA_PERFIL) \
  X(NUNCA_ENDERECOS) \
  X(NUNCA_SENHAS) \
  X(NUNCA_IP) \
  X(NUNCA_HISTORICO) \
  X(SOME_30) \
  X(INCLUIR_TEXTO) \
  X(TEXTO_AVISO) \
  X(REGISTRO_SEM_DADOS) \
  X(REGISTRO_ENVIADO_L1) \
  X(REGISTRO_ENVIADO_L2) \
  X(JA_CORRIGIDO_L1) \
  X(JA_CORRIGIDO_L2) \
  X(SONDANDO) \
  X(SONDANDO_FONTE) \
  X(ENVIANDO) \
  X(SEM_NET_ENVIO_L2) \
  X(RECUSOU_L1) \
  X(RECUSOU_L2) \
  X(LIMITE_L1) \
  X(LIMITE_L2) \
  X(SEM_CONTA_L1) \
  X(SEM_CONTA_L2) \
  X(ACAO_FALHOU_L1) \
  X(ACAO_FALHOU_L2) \
  X(NADA_ACHAR_L1) \
  X(NADA_ACHAR_L2) \
  X(ACHA_L1) \
  X(ACHA_L2_NAO_VISTO) \
  X(DOIS_GOSTOS_L1) \
  X(DOIS_GOSTOS_L2) \
  X(RETOMAR_L1) \
  X(RETOMAR_L2) \
  X(BUSCA_FRASE) \
  X(BUSCA_FRASE_PROMPT) \
  X(BUSCA_FRASE_EXEMPLO) \
  X(VOCE_DISSE) \
  X(CONFIRMA_DESCARTA) \
  X(BUSCA_FRASE_PROCURANDO) \
  X(BUSCA_FRASE_OFFLINE) \
  X(BUSCA_FRASE_LIMITE) \
  X(DIGITE_CELULAR) \
  X(OUVINDO) \
  X(AVISOS_FAROL) \
  X(AVISOS_FAROL_DESC) \
  X(CARTAO_FAROL)

// ---------------------------------------------------------- nome e entradas
#define FT_NOME                "Farol"                 // en: "Beacon"
#define FT_ACHAR_O_QUE_VER     "Achar o que ver"
#define FT_AJUDA_KICKER        "Ajustes › Ajuda"       // ja existe
#define FT_BANNER_FALHA        "O último vídeo não abriu · ver o motivo"
#define FT_SINTOMA_CORES       "Cores estranhas"
#define FT_SINTOMA_CORES_DESC  "Verde, roxo, lavado ou cores trocadas"
#define FT_INTENCAO_CURTO      "Curto para hoje"
#define FT_INTENCAO_NAO_VI     "Ainda não vi"
#define FT_INTENCAO_RETOMAR    "De onde parei"
#define FT_INTENCAO_SURPRESA   "Me surpreenda"
#define FT_REGISTROS_ENVIADOS  "Registros enviados · %d"   // ja existe
#define FT_NAO_TESTADO         "não testado"
#define FT_TESTANDO            "testando…"             // ja existe
#define FT_A_CAUSA_ENCONTRADA  "Achamos o motivo"      // ja existe
#define FT_RESOLVER_NOME       "Resolver um problema"  // a chave antiga fica

// ------------------------------------------------------------ gatilhos 3.1
#define FT_PILULA_FALHA        "Algo impediu o vídeo"
#define FT_ABRE_O_FAROL        "abre o Farol"          // tecla desenhada ao lado
#define FT_OK_ABRE_O_FAROL     "OK abre o Farol"

// ------------------------------- conserto calado (ALMA 5, issues 20, 33, 34)
#define FT_FONTE_TROCADA         "A primeira fonte falhou. Troquei para outra em %s."
#define FT_FONTE_TROCADA_SIMPLES "A primeira fonte falhou. Troquei para outra fonte."
#define FT_AUDIO_CONVERTIDO      "Som ajustado para 5.1 compatível com esta TV."
#define FT_LEGENDA_RESSINC       "Legenda ajustada ao tempo do vídeo."
#define FT_ADDON_ISOLADO         "Um add-on lento ficou de fora para a lista abrir mais rápido."
#define FT_VOLTOU_A_TOCAR        "Voltou a tocar."
#define FT_REDE_RECONECTADA      "Rede reconectada."
#define FT_CACHE_LIMPO           "Cache limpo."
#define FT_HDR_DESLIGADO         "HDR desligado para este vídeo."
#define FT_MEMORIA_LIBERADA      "Liberei memória para o vídeo."
#define FT_OBSERVANDO_VIDEO      "Vendo se o vídeo volta a tocar…"

// -------------------------------------------------- vereditos da tabela 2.4
#define FT_REDE_FORA_L1         "A TV está sem internet."    // ja existe
#define FT_REDE_FORA_L2         "Confira o Wi-Fi ou o cabo; tento de novo sozinho."
#define FT_REDE_NAO_CHEGA_L1    "A internet está de pé, mas não chegou até esta fonte."
#define FT_REDE_NAO_CHEGA_L2    "Tento reconectar a rede; se não chegar, separo outra fonte."
#define FT_ADDON_NENHUM_L1      "Falta um add-on de fontes para ter o que tocar."
#define FT_ADDON_NENHUM_L2      "Aperta OK para instalar pelo celular."
#define FT_ADDON_LENTO_L1       "Um add-on demorou demais e segurou a lista."
#define FT_ADDON_LENTO_L2       "Deixo ele de lado por 24 h para o resto abrir rápido."
#define FT_ADDON_LENTO_NOME     "O add-on %s demorou demais e segurou a lista."
#define FT_DEBRID_SEM_PLANO_L1  "Sua conta de cache está sem plano ativo."
#define FT_DEBRID_SEM_PLANO_L2  "Aponta o celular no QR para renovar."
#define FT_DEBRID_DESCONECTOU_L1 "Sua conta de cache desconectou."
#define FT_DEBRID_DESCONECTOU_L2 "Aponta o celular no QR para reconectar sem digitar na TV."
#define FT_FONTE_RECUSOU_L1     "O servidor desta fonte recusou o link."
#define FT_FONTE_RECUSOU_L2     "Separei outra — aperta OK para tocar."
#define FT_FONTE_SEM_RANGE_L1   "Esta fonte não deixa avançar nem retomar."
#define FT_FONTE_SEM_RANGE_L2   "Separei outra que deixa — aperta OK para tocar."
#define FT_REDE_CURTA_L1        "Este arquivo pede mais internet do que a TV tem agora."
#define FT_REDE_CURTA_L2        "Separei uma versão mais leve que abre sem travar."
#define FT_TV_SEM_MEMORIA_L1    "A TV ficou sem memória para abrir o vídeo."
#define FT_TV_SEM_MEMORIA_L2    "Liberei espaço — aperta OK para tentar de novo."
#define FT_TV_NEGOU_PLAYER_L1   "A TV não deixou o app usar o player de vídeo"   // ja existe
#define FT_TV_NEGOU_PLAYER_L2   "Reiniciar a TV costuma resolver. Quer mandar o registro?"
#define FT_AUDIO_DTS_L1         "Sua TV não aceita áudio DTS direto."
#define FT_AUDIO_DTS_L2         "Posso converter para 5.1 compatível agora e tocar."
#define FT_AUDIO_OUTRA_FAIXA_L1 "O áudio principal desta fonte não toca nesta TV."
#define FT_AUDIO_OUTRA_FAIXA_L2 "Troquei para a outra faixa que toca."
#define FT_AUDIO_SEM_SAIDA_L1   "Esta TV não toca o áudio desta fonte."
#define FT_AUDIO_SEM_SAIDA_L2   "Separei outra com som compatível — aperta OK."
#define FT_IMAGEM_DV_L1         "Esta fonte usa um Dolby Vision que esta TV não mostra direito."
#define FT_IMAGEM_DV_L2         "Separei a versão HDR10 — aperta OK para tocar."
#define FT_TV_FORMATO_L1        "A TV não conseguiu tocar o formato desta fonte." // ja existe
#define FT_TV_FORMATO_L2        "Separei outra — aperta OK para tocar."
#define FT_TV_FORMATO_REPETE_L1 "A internet e a fonte estão boas, mas o player desta TV travou."
#define FT_TV_FORMATO_REPETE_L2 "Separei uma fonte em outro formato — aperta OK para tocar."
#define FT_PLAYER_PAROU_L1      "A internet está rápida, mas o vídeo parou dentro do player."
#define FT_PLAYER_PAROU_L2      "Aumento o buffer e reabro de onde parou."
#define FT_NADA_ACHADO_L1       "Testei tudo o que dá para ver daqui e está sem erro."
#define FT_NADA_ACHADO_L2       "Se continua, manda o registro: ele leva o que o app viu."

// ----------------------------------------------------- botoes de acao (OK)
#define FT_ACAO_TENTAR            "Tentar de novo"           // ja existe
#define FT_ACAO_PROXIMA_FONTE     "Próxima fonte"
#define FT_ACAO_RECONECTAR_REDE   "Reconectar a rede"
#define FT_ACAO_ABRIR_FONTES      "Abrir Fontes e addons"
#define FT_ACAO_ISOLAR            "Isolar por 24 h"
#define FT_ACAO_ABRIR_DEBRID      "Abrir a conta de cache"
#define FT_ACAO_DEBRID_CELULAR    "Reconectar pelo celular"
#define FT_ACAO_FONTE_MENOR       "Fonte menor"
#define FT_ACAO_ALIVIAR           "Aliviar e reabrir"
#define FT_ACAO_MANDAR_REGISTRO   "Mandar registro"
#define FT_ACAO_ENVIAR_REGISTRO   "Enviar um registro"       // ja existe
#define FT_ACAO_CONVERTER         "Converter e reabrir"
#define FT_ACAO_TROCAR_FAIXA      "Trocar faixa"
#define FT_ACAO_FONTE_SEM_DTS     "Fonte sem DTS"
#define FT_ACAO_FONTE_SEM_DV      "Fonte sem DV"
#define FT_ACAO_ESCONDER_DV       "Esconder Dolby Vision nesta TV"
#define FT_ACAO_PLAYER_EXTERNO    "Player externo"
#define FT_ACAO_FONTE_FORMATO     "Fonte em outro formato"
#define FT_ACAO_BUFFER_MAIOR      "Buffer maior"
#define FT_ACAO_LIMPAR_CACHE      "Limpar o cache"
#define FT_ACAO_DESLIGAR_HDR      "Desligar o HDR neste vídeo"
#define FT_ACAO_DESLIGAR_HDR_SEMPRE "Desligar o HDR sempre nesta TV"
#define FT_ACAO_DESFAZER          "Desfazer"                 // ja existe
#define FT_ACAO_MANDAR            "Mandar"
#define FT_ACAO_AGORA_NAO         "Agora não"                // ja existe
#define FT_ACAO_PRONTO            "Pronto"                   // ja existe
#define FT_ACAO_FECHAR            "Fechar"                   // ja existe
#define FT_ACAO_VOLTAR            "Voltar"                   // ja existe
#define FT_ACAO_LIGADO            "Ligado"                   // ja existe
#define FT_ACAO_SO_QUANDO_ABRIR   "Só quando eu abrir"
#define FT_ACAO_FALAR             "Falar"                    // ja existe
#define FT_ACAO_DIGITAR_CELULAR   "Digitar pelo celular"     // ja existe
#define FT_ACAO_TECLADO_TV        "Teclado da TV"            // ja existe

// --------------------------------------------- Vai junto / Nunca vai (3.2.4)
#define FT_VAI_JUNTO       "Vai junto"               // ja existe
#define FT_NUNCA_VAI       "Nunca vai"
#define FT_VAI_MODELO      "Modelo e sistema desta TV"
#define FT_VAI_VERSAO      "Versão do Nuvio"
#define FT_VAI_FORMATO     "Formato técnico do vídeo (codec, HDR, áudio)"
#define FT_VAI_PONTO       "Onde a reprodução parou"
#define FT_VAI_AJUSTES     "Ajustes que você mudou"
#define FT_VAI_EVENTOS     "Os últimos 5 minutos em eventos técnicos"
#define FT_NUNCA_TITULOS   "Nome de filme ou série"
#define FT_NUNCA_PERFIL    "Seu perfil, conta e amigos"
#define FT_NUNCA_ENDERECOS "Endereços de fontes e add-ons"
#define FT_NUNCA_SENHAS    "Senhas e chaves"         // ja existe
#define FT_NUNCA_IP        "Seu IP"
#define FT_NUNCA_HISTORICO "Seu histórico"           // ja existe
#define FT_SOME_30         "Some sozinho em 30 dias."
#define FT_INCLUIR_TEXTO   "Incluir o registro de texto (pode ter nomes de títulos)"
#define FT_TEXTO_AVISO     "O registro de texto pode ter nomes de títulos e endereços."
#define FT_REGISTRO_SEM_DADOS "Sem título, senha nem conta."

// ---------------------------------------------------- enviado / ja corrigido
#define FT_REGISTRO_ENVIADO_L1 "Registro %s enviado. Ele some sozinho em 30 dias."
#define FT_REGISTRO_ENVIADO_L2 "Cite este código no Discord ou aponte o celular no QR."
#define FT_JA_CORRIGIDO_L1     "Este travamento já foi corrigido na versão %s."
#define FT_JA_CORRIGIDO_L2     "Aperta OK para atualizar agora."

// ------------------------------------------------------------- estados 3.3
#define FT_SONDANDO         "Testando o caminho do vídeo…"
#define FT_SONDANDO_FONTE   "Testando fonte…"
#define FT_ENVIANDO         "Mandando o registro…"
#define FT_SEM_NET_ENVIO_L2 "Guardo o registro e mando sozinho quando ela voltar."
#define FT_RECUSOU_L1       "Não consegui entregar o registro agora."
#define FT_RECUSOU_L2       "Ele fica guardado; tento de novo quando você pedir."
#define FT_LIMITE_L1        "Já recebi registros demais desta TV hoje."
#define FT_LIMITE_L2        "O último código continua valendo."
#define FT_SEM_CONTA_L1     "Para mandar o registro, entre na Conta Nuvio ou no Trakt."
#define FT_SEM_CONTA_L2     "O diagnóstico funciona sem conta."
#define FT_ACAO_FALHOU_L1   "Não deu certo com esta."
#define FT_ACAO_FALHOU_L2   "Quer tentar outra fonte ou mandar o registro?"
#define FT_NADA_ACHAR_L1    "Nada com menos de %d min nos seus catálogos."
#define FT_NADA_ACHAR_L2    "Trouxe três curtos parecidos."

// ---------------------------------------------------- descoberta (2.9, ALMA 5)
#define FT_ACHA_L1          "%d %s com menos de %s."
#define FT_ACHA_L2_NAO_VISTO "Nenhum deles você viu ainda."
#define FT_DOIS_GOSTOS_L1   "Dois filmes que batem com o seu gosto e o de %s."
#define FT_DOIS_GOSTOS_L2   "Os dois têm áudio dublado e legenda sincronizada."
#define FT_RETOMAR_L1       "Você parou no fim do %s."
#define FT_RETOMAR_L2       "Quer seguir do %s ou rever os últimos %d minutos?"

// ------------------------------------------- frase livre (adendo, 30 a 32)
#define FT_BUSCA_FRASE           "Buscar com uma frase"
#define FT_BUSCA_FRASE_PROMPT    "Diga o que quer ver"
#define FT_BUSCA_FRASE_EXEMPLO   "Ex.: um suspense curto para hoje à noite"
#define FT_VOCE_DISSE            "Você disse:"
#define FT_CONFIRMA_DESCARTA     "OK confirma · Voltar descarta"
#define FT_BUSCA_FRASE_PROCURANDO "Procurando o que ver…"
#define FT_BUSCA_FRASE_OFFLINE   "A busca por frase está fora — mostrei o mais perto daqui."
#define FT_BUSCA_FRASE_LIMITE    "A busca por frase já encheu hoje — mostrei o mais perto daqui."
#define FT_DIGITE_CELULAR        "Digite a frase no celular"
#define FT_OUVINDO               "Ouvindo…"               // ja existe

// ----------------------------------------------------- ajustes e novidades
#define FT_AVISOS_FAROL      "Avisos do Farol"
#define FT_AVISOS_FAROL_DESC "O Farol avisa quando o vídeo falha e confirma o conserto."
#define FT_CARTAO_FAROL      "Acha o que ver em segundos e destrava o vídeo."

#endif
