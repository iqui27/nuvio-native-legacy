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
  X(L2_PLAYER_EXTERNO) \
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
  X(CARTAO_FAROL) \
  X(FIL_PORQUE_TERMINOU) \
  X(FIL_PESSOA) \
  X(FIL_TEMA) \
  X(FIL_GENERO) \
  X(FIL_HORA) \
  X(FIL_FIMSEMANA) \
  X(FIL_AMIGOS) \
  X(FIL_SALVOS) \
  X(FIL_REVER) \
  X(FIL_FORA_CURVA) \
  X(FIL_RECOMENDADOS) \
  X(FIL_COMECAR) \
  X(FIL_CLIMA) \
  X(FIL_SUB_TEMA) \
  X(MOT_TERMINOU) \
  X(MOT_TERMINOU_TEMA) \
  X(MOT_SEMELHANTE) \
  X(MOT_GENERO) \
  X(MOT_TEMA) \
  X(MOT_PESSOA) \
  X(MOT_DIRETOR) \
  X(MOT_AMIGO) \
  X(MOT_AMIGO_ANON) \
  X(MOT_SALVO) \
  X(MOT_CABE_HOJE) \
  X(MOT_REVER) \
  X(MOT_FORA_CURVA) \
  X(MOT_RECOMENDADO) \
  X(MOT_RECOM_ANON) \
  X(MOT_ESTREIA) \
  X(MOT_CONTRA) \
  X(MOT_AMIGOS) \
  X(DICA_PAROU_L1) \
  X(DICA_PAROU_L2) \
  X(DICA_SALVO_L1) \
  X(DICA_SALVO_L2) \
  X(DICA_ESTREIA) \
  X(DICA_FALTA_L1) \
  X(DICA_FALTA_L2) \
  X(DICA_TEMPORADA) \
  X(DICA_TRAKT) \
  X(DICA_PERGUNTAS) \
  X(DICA_REVER) \
  X(DICA_CURTO) \
  X(INFO_QUALI) \
  X(INFO_QUALI_AUDIO) \
  X(INFO_QUALI_SO) \
  X(INFO_IDIOMA) \
  X(INFO_LEGENDA) \
  X(INFO_SOBE) \
  X(INFO_CAI) \
  X(INFO_FALTAM) \
  X(INFO_MESMO_NIVEL) \
  X(INFO_TERMINA) \
  X(INFO_DUAS_NOITES) \
  X(INFO_ONDE) \
  X(INFO_AMIGOS_TERM) \
  X(INFO_AMIGO_TERM) \
  X(INFO_AMIGOS_GOST) \
  X(INFO_SAGA) \
  X(INFO_SAGA_VOCE_VIU) \
  X(INFO_REVER) \
  X(INFO_REVER_SIMPLES) \
  X(INFO_RETOMADA) \
  X(INFO_RETOMADA_SIMPLES) \
  X(ESC_TEMPO) \
  X(ESC_TEMPO_45) \
  X(ESC_TEMPO_FILME) \
  X(ESC_TEMPO_NOITE) \
  X(ESC_PESO) \
  X(ESC_PESO_LEVE) \
  X(ESC_PESO_INTENSO) \
  X(ESC_COMPANHIA) \
  X(ESC_SO_EU) \
  X(ESC_A_DOIS) \
  X(ESC_CRIANCA) \
  X(ESC_NOVIDADE) \
  X(ESC_NOVO) \
  X(ESC_CONHECIDO) \
  X(ESC_FORMATO) \
  X(ESC_FILME) \
  X(ESC_SERIE) \
  X(ESC_IDIOMA) \
  X(ESC_DUBLADO) \
  X(ESC_LEGENDADO) \
  X(ESC_EPOCA) \
  X(ESC_EPOCA_NOVA) \
  X(ESC_EPOCA_ANTIGA) \
  X(ESC_HUMOR) \
  X(ESC_HUMOR_RIR) \
  X(ESC_HUMOR_EMOCIONAR) \
  X(ESC_HUMOR_TENSAO) \
  X(ESC_CONTINUAR) \
  X(ESC_CONT_SIM) \
  X(ESC_COMECAR) \
  X(ESC_TANTO_FAZ) \
  X(ESC_PASSO) \
  X(CMP_TITULO) \
  X(CMP_DURACAO) \
  X(CMP_NOTA) \
  X(CMP_CLIMA) \
  X(CMP_PORQUE) \
  X(CMP_QUALIDADE) \
  X(CMP_TERMINA) \
  X(CMP_ONDE) \
  X(CMP_AMIGOS) \
  X(CMP_VEREDITO_CABE) \
  X(CMP_VER_QUALIDADE) \
  X(ACHA_CURTOS_L1) \
  X(ACHA_NAO_VIU) \
  X(GOSTO_APRENDEU) \
  X(GOSTO_ZERAR) \
  X(GOSTO_ZERAR_CONF) \
  X(GOSTO_ZERADO) \
  X(GOSTO_RECOMENDAR) \
  X(GOSTO_LLM_SO_ESTA_TV) \
  X(MAIS_ASSIM) \
  X(NAO_E_PARA_MIM) \
  X(COMPARAR_COM) \
  X(NAO_MOSTRO_MAIS) \
  X(ANOTADO) \
  X(ESCOLHENDO) \
  X(PORQUE_TITULO)

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
// Segunda linha dedicada do cartao quando a acao efetiva e PLAYER_EXTERNO: o
// OK abre o player externo, entao a frase promete isso (CORRECAO-FAROL6B).
#define FT_L2_PLAYER_EXTERNO    "Abro no player externo que esta TV tem — aperta OK."
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

// ============================================ gosto (PLANO-GOSTO, issues 35+)
// Frases das sete pecas da 4.2: FT_FIL_* titulos/sub de fileira (P1),
// FT_DICA_* cartao de dica (P2), FT_MOT_* linhas de motivo (P3 e fileira),
// FT_CMP_* comparar (P4), FT_ESC_* assistente de perguntas (P5), FT_INFO_*
// linhas de informacao (P6) e FT_ACHA_* resultado em cartazes (P7).
// Argumentos: %s nome/titulo/tema/duracao/tempo ("1h32", "5 meses", "23:40"),
// %d numero puro (episodios, minutos, temporada, estrelas). Onde a gramatica
// da lingua nao aceita encaixe (caso, artigo, genero), a irma usa a forma
// neutra com dois-pontos da 5.1 ("Do mesmo tema: viagem no tempo.").

// --------------------------------------------------------- fileiras (P1)
#define FT_FIL_PORQUE_TERMINOU  "Porque você terminou %s"
#define FT_FIL_PESSOA           "Com %s, de quem você gosta"
#define FT_FIL_TEMA             "Do mesmo tema: %s"
#define FT_FIL_GENERO           "Para quem gosta de %s"
#define FT_FIL_HORA             "Para esta hora"
#define FT_FIL_FIMSEMANA        "Para o fim de semana"
#define FT_FIL_AMIGOS           "O que seus amigos gostaram"
#define FT_FIL_SALVOS           "Dos seus salvos"
#define FT_FIL_REVER            "Vale rever"
#define FT_FIL_FORA_CURVA       "Fora do seu costume"
#define FT_FIL_RECOMENDADOS     "Escolhidos para você"
#define FT_FIL_COMECAR          "Para começar, pelo que você já viu"
#define FT_FIL_CLIMA            "No seu clima"
#define FT_FIL_SUB_TEMA         "Mais sobre %s"

// --------------------------------------------------- motivos (3.5, P3 e P1)
#define FT_MOT_TERMINOU         "Porque você terminou %s."
#define FT_MOT_TERMINOU_TEMA    "Você terminou %s e gosta de %s."
#define FT_MOT_SEMELHANTE       "Parecido com %s, que você terminou."
#define FT_MOT_GENERO           "Você gosta de %s."
#define FT_MOT_TEMA             "Do mesmo tema: %s."
#define FT_MOT_PESSOA           "Com %s, que você acompanha."
#define FT_MOT_DIRETOR          "Do diretor de %s, que você viu %d vezes."
#define FT_MOT_AMIGO            "%s gostou. Vocês concordam em %d de cada 10."
#define FT_MOT_AMIGO_ANON       "Um amigo seu gostou."
#define FT_MOT_SALVO            "Está nos seus salvos há %s."
#define FT_MOT_CABE_HOJE        "Cabe hoje: %d min."
#define FT_MOT_REVER            "Você já viu — vale rever."
#define FT_MOT_FORA_CURVA       "Fora do seu costume: %s bem avaliado."
#define FT_MOT_RECOMENDADO      "Indicado por %s."
#define FT_MOT_RECOM_ANON       "Indicado por um amigo."
#define FT_MOT_ESTREIA          "Estreia de %s, que você acompanha."
#define FT_MOT_CONTRA           "Mais lento do que você costuma ver."
#define FT_MOT_AMIGOS           "%s e %s gostaram."

// ----------------------------------------------------------- dicas (P2)
#define FT_DICA_PAROU_L1        "Você parou em %s, no T%dE%d."
#define FT_DICA_PAROU_L2        "Faz %d semanas que você parou."
#define FT_DICA_SALVO_L1        "Salvo há %s."
#define FT_DICA_SALVO_L2        "Tem %s."
#define FT_DICA_ESTREIA         "Estreia nova de %s."
#define FT_DICA_FALTA_L1        "Faltam %d episódios para fechar a T%d de %s."
#define FT_DICA_FALTA_L2        "Você já viu a maior parte."
#define FT_DICA_TEMPORADA       "Temporada nova de %s chegou."
#define FT_DICA_TRAKT           "Ligar o Trakt traz seu gosto em 1 minuto."
#define FT_DICA_PERGUNTAS       "Três perguntas e eu acho o que ver."
#define FT_DICA_REVER           "Vale rever %s."
#define FT_DICA_CURTO           "Curto para hoje: %s."

// ------------------------------------------------------------ info (P6)
#define FT_INFO_QUALI           "Tem em %s %s, com áudio em %s."
#define FT_INFO_QUALI_AUDIO     "Tem em %s, com áudio em %s."
// Mesma linha sem o trecho de audio: resolucao/formato sem trilha.
#define FT_INFO_QUALI_SO          "Tem em %s."
#define FT_INFO_IDIOMA          "Tem áudio em %s."
#define FT_INFO_LEGENDA         "Tem legenda em %s."
#define FT_INFO_SOBE            "A T%d sobe de nota. Faltam %d episódios de %d min."
#define FT_INFO_CAI             "A T%d cai bastante; muita gente para aqui."
#define FT_INFO_FALTAM          "Faltam %d episódios de %d min."
#define FT_INFO_MESMO_NIVEL     "As temporadas seguem no mesmo nível."
#define FT_INFO_TERMINA         "Termina às %s."
#define FT_INFO_DUAS_NOITES     "Dá em %d noites de %d min."
#define FT_INFO_ONDE            "Também está na %s."
#define FT_INFO_AMIGOS_TERM     "%s e %s terminaram."
#define FT_INFO_AMIGO_TERM      "%s terminou."
#define FT_INFO_AMIGOS_GOST     "%s e %s gostaram."   // mesmo literal de FT_MOT_AMIGOS
#define FT_INFO_SAGA            "É o %dº de %d."
#define FT_INFO_SAGA_VOCE_VIU   "É o %dº de %d — você viu %d."
#define FT_INFO_REVER           "Você viu em %s e deu %d estrelas."
#define FT_INFO_REVER_SIMPLES   "Você viu em %s."
#define FT_INFO_RETOMADA        "Você parou no T%dE%d há %s."
#define FT_INFO_RETOMADA_SIMPLES "Você parou no T%dE%d."

// ------------------------------------------------------ assistente (P5)
#define FT_ESC_TEMPO            "Quanto tempo você tem?"
#define FT_ESC_TEMPO_45         "Até 45 minutos"
#define FT_ESC_TEMPO_FILME      "Um filme inteiro"
#define FT_ESC_TEMPO_NOITE      "A noite toda"
#define FT_ESC_PESO             "Leve ou intenso?"
#define FT_ESC_PESO_LEVE        "Suave"             // chave propria: filme leve, nao a severidade
#define FT_ESC_PESO_INTENSO     "Intenso"
#define FT_ESC_COMPANHIA        "Com quem você vai ver?"
#define FT_ESC_SO_EU            "Só eu"
#define FT_ESC_A_DOIS           "A dois"
#define FT_ESC_CRIANCA          "Com criança"
#define FT_ESC_NOVIDADE         "Novo ou conhecido?"
#define FT_ESC_NOVO             "Novo"              // ja existe
#define FT_ESC_CONHECIDO        "Conhecido"
#define FT_ESC_FORMATO          "Filme ou série?"
#define FT_ESC_FILME            "Filme"             // ja existe
#define FT_ESC_SERIE            "Série"             // ja existe
#define FT_ESC_IDIOMA           "Dublado ou legendado?"
#define FT_ESC_DUBLADO          "Dublado"           // ja existe
#define FT_ESC_LEGENDADO        "Legendado"         // ja existe
#define FT_ESC_EPOCA            "De que época?"
#define FT_ESC_EPOCA_NOVA       "Recente"
#define FT_ESC_EPOCA_ANTIGA     "Clássico"          // ja existe
#define FT_ESC_HUMOR            "Que clima você quer?"
#define FT_ESC_HUMOR_RIR        "Para rir"
#define FT_ESC_HUMOR_EMOCIONAR  "Para se emocionar"
#define FT_ESC_HUMOR_TENSAO     "Com tensão"
#define FT_ESC_CONTINUAR        "Continuar algo ou começar do zero?"
#define FT_ESC_CONT_SIM         "Continuar algo"
#define FT_ESC_COMECAR          "Começar do zero"
#define FT_ESC_TANTO_FAZ        "Tanto faz"
#define FT_ESC_PASSO            "%d de %d"

// -------------------------------------------------------- comparar (P4)
#define FT_CMP_TITULO           "Comparar"
#define FT_CMP_DURACAO          "Duração"           // ja existe
#define FT_CMP_NOTA             "Nota"              // ja existe
#define FT_CMP_CLIMA            "Clima"
#define FT_CMP_PORQUE           "Por que você vai gostar"
#define FT_CMP_QUALIDADE        "Qualidade"
#define FT_CMP_TERMINA          "Termina às"
#define FT_CMP_ONDE             "Onde ver"          // ja existe
#define FT_CMP_AMIGOS           "Amigos"            // ja existe
#define FT_CMP_VEREDITO_CABE    "O %s cabe no seu tempo."
#define FT_CMP_VER_QUALIDADE    "Ver qualidade"

// -------------------------------------------------- resultado em cartazes (P7)
#define FT_ACHA_CURTOS_L1       "%d curtos com menos de %d min."
#define FT_ACHA_NAO_VIU         "Nenhum deles você viu ainda."   // ja existe

// ---------------------------------- ajustes, acoes e confirmacoes do gosto
#define FT_GOSTO_APRENDEU       "O que o Farol aprendeu"
#define FT_GOSTO_ZERAR          "Zerar o gosto"
#define FT_GOSTO_ZERAR_CONF     "Apagar o que o Farol aprendeu nesta TV?"
#define FT_GOSTO_ZERADO         "Gosto zerado."
#define FT_GOSTO_RECOMENDAR     "Recomendar nesta TV"
#define FT_GOSTO_LLM_SO_ESTA_TV "Só nesta TV"
#define FT_MAIS_ASSIM           "Mais assim"
#define FT_NAO_E_PARA_MIM       "Não é para mim"
#define FT_COMPARAR_COM         "Comparar com…"
#define FT_NAO_MOSTRO_MAIS      "Certo, não mostro mais."
#define FT_ANOTADO              "Anotado."
#define FT_ESCOLHENDO           "Escolhendo…"
#define FT_PORQUE_TITULO        "Por que você vai gostar"   // mesmo literal de FT_CMP_PORQUE

// faroltexto_aceita (src/faroltexto.c): o filtro do texto livre "x" que o LLM
// manda na cena (PLANO-GOSTO 4.1/5.1). Devolve 1 se a frase pode subir na
// tela — valida UTF-8, no maximo 2 linhas de 62 caracteres, sem URL, sem
// emoji e sem nenhuma palavra da lista "nunca diz" do ALMA 2 — e 0 se nao
// pode; quem compoe cai no modelo "m" de reserva. Modulo puro: sem malloc,
// sem rede, sem dados.h.
int faroltexto_aceita(const char *s);

#endif
