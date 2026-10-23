# Teclas CH+ / CH− no lugar das cores (2.0.5)

Pedido do dono: "padronizar onde fala para apertar o botão azul trocar para o
CH+ e o outro botão para o CH−, em todas as plataformas".

## Quem é "o outro botão"

**AMARELO.** Azul abre Salvos, amarelo abre a busca (Spotlight). O Android e o
.tpk já ligavam CH+ → AZUL/Salvos e CH− → F5/Spotlight, então o par é
CH+ = azul e CH− = amarelo. Vermelho (log) e verde (diagnóstico da Live TV)
não são "o outro": seguem como estão (lista no fim).

## Comportamento das teclas (todas as plataformas)

| Contexto | CH+ | CH− | Cor |
|---|---|---|---|
| Fora de zap (Home, telas, Ajustes…) | Salvos / cartão da ilha (= AZUL) | Spotlight, ou fecha o Spotlight (= AMARELO) | AZUL e AMARELO continuam iguais |
| Guia de TV | categoria anterior (inalterado) | categoria seguinte (inalterado) | AZUL abre/fecha o overlay; AMARELO alterna grade/cartões; dicas de cor mantidas. **.wgt:** o CH+ é a única "AZUL", então continua categoria seguinte (como sempre foi); só o CH− salta de verdade |
| Player e PiP com canal no ar | zap de canal (inalterado) | zap de canal (inalterado) | AZUL/AMARELO inalterados; dica de cor mantida. **.wgt:** a casca não traduz a ColorF3Blue (406), então o CH+ segue como AZUL ali (faixa do guia → "Guia completo", PiP → tela cheia) e não zapa; o CH− zapa |
| Segurar CH+ | abre a Central de controle (`src/chsegura.h`, `src/central.h`) | — | toque curto continua fazendo a ação de AZUL |
| Detalhe de fonte (`streams.c`) | — | abre, como AMARELO | AMARELO inalterado |

### Ligações acrescentadas

- **Tizen web `.wgt`, `src/main.c` (`remapCanal`, bloco `__EMSCRIPTEN__`)**: o CH+ (427/F7) segue sempre como a AZUL ("s"/scancode AZUL) — a casca (`tools/tizen-shell.html`) não traduz a ColorF3Blue (406), então o F7 é a única tecla AZUL do Tizen web e não pode virar zap. O CH− (428/F8) é novo: com canal na tela zapa de verdade (PAGEDOWN/CH_DOWN); fora disso é F5 (Spotlight). Antes o CH− não fazia nada fora do zap.
- **LG/webOS, `src/app.c` (`spotAtalho`), `src/spotlight.c`, `src/streams.c`**: o scancode cru 481 (NV_SCANCODE_CH_DOWN, `SDL_webOS.h`) abre/fecha o Spotlight e abre o detalhe de fonte, como AMARELO. O webOS nativo não passa por `remapCanal` — a ligação é o scancode direto, sem remapeamento.
- **Tizen .wgt, `tools/tizen-shell.html`**: ChannelDown (428) registrada e
  traduzida para F8 (keydown/keyup), par do F7 do CH+.
- **Android TV/mobile e .tpk**: já tinham CH+/CH−; só os textos mudaram.
- Testes: `bash tests/wgt_voz.sh` prova 428 → F8 (119) em keydown e keyup;
  `bash tests/teclas_ch.sh` compila o remapeador real para Android, .tpk,
  Emscripten/.wgt e Mac, verificando CH+/CH−, down/up, Salvos/busca e zap
  (no .wgt o CH+ nunca zapa — a única AZUL);
  `bash tests/spot_ch.sh` prova o `spotAtalho` real de app.c: CH− (481)
  abre como AMARELO, menos no guia e no keyup.
  `bash tests/spotlight_regressao.sh` prova que o CH− fecha o Spotlight aberto.
  `bash tests/fonte_detalhe.sh` prova que CH− abre/fecha o detalhe como AMARELO.
- .wgt e Android usam o scancode AZUL para CH+ fora do zap, como .tpk,
  evitando digitar a letra "s" no Spotlight. As ações fora de campos e o zap
  continuam iguais.

## Inventário de textos

| Texto (pt) | Tela | Tecla antes | Tecla agora |
|---|---|---|---|
| "AMARELO abre a busca… AZUL ou CH+ abre Salvos…" | Ajustes › guia de teclas | AMARELO / AZUL ou CH+ | CH− / CH+ |
| "A tecla AMARELA abre o Spotlight…" | Ajustes › guia (Spotlight) | AMARELA | CH− |
| "AMARELO no controle, ou Busca no menu" | Ajustes › guia (Spotlight) | AMARELO | CH− |
| "A tecla amarela abre a busca…" | Novidades 1.7 | amarela | CH− |
| "AMARELO · livre" + corpo | Novidades 1.1 | AMARELO (texto "não faz nada") | trocado por "CH− · a busca" |
| "CH- · o registro" (Android) | Novidades 1.1 | CH- (hífen ASCII) | "Info · o registro" |
| "AZUL · Salvos e o guia" / "CH+ · Salvos e o guia" | Novidades 1.1 | AZUL (LG) / CH+ (Android) | um só: "CH+ na Home" (não promete guia durante zap) |
| "…pílula… AZUL ou CH+ abre, → leva aos Salvos." (2x) | Ajustes › guia; Novidades 1.7 | AZUL ou CH+ | CH+ |
| "AZUL ou CH+ na Home" | Ajustes › guia (ilha) | AZUL ou CH+ | CH+ |
| "AZUL abre o painel com o que você guardou…" | Ajustes › guia (Salvos) | AZUL | CH+ |
| "AZUL › Avisos", "AZUL › Amigos" | Ajustes › guia | AZUL | CH+ (sem chave na tabela; só fonte) |
| "…CH+ ou AZUL retoma de onde parou." | Ajustes › guia (sair do player) | CH+ ou AZUL | CH+ |
| "…(CH+ ou AZUL)" `AJ_SAIDA_PLAYER` | Ajustes › player | CH+ ou AZUL | CH+ |
| Pílula `AZUL` (`TXT_AJ_KBD`) | Ajustes › guia (tecla) | AZUL | CH+ |
| "Painel Social abre com o Azul" | Novidades 2.0 | Azul | CH+ |
| Pílulas `Azul` (2x) | Novidades 2.0 | Azul | CH+ |
| "…um toast quando chega, AZUL abre…" | Novidades 1.3.2 | AZUL (LG) / CH+ (Android) | um só: CH+ |
| "Está na tecla AZUL" | Recomendações (intro) | AZUL (LG) / CH+ | "Está no CH+" |
| "Depois, o botão AZUL / CANAL + abre sua lista…" | Intro de Salvos | AZUL / CANAL + | "Depois, o CH+ abre sua lista…" |
| Desenho da tecla | Intro de Salvos; ilha (aviso e cartão); Novidades 1.7 | pastilha azul (LG), rocker (Samsung), pastilha CH+ (Android) | pastilha "CH+" em todas |
| Desenho do controle | Novidades 1.1 | discos AMARELO/AZUL | pastilhas CH−/CH+; vermelho/verde mantidos |
| "Abre Salvos, abre o guia com um canal no ar e volta do vídeo pequeno." | Novidades 1.1 | AZUL | corpo explica só Salvos para CH+; exceções de zap abaixo |

Traduções: pt e as 28 `idioma_*.h` atualizadas juntas (chaves renomeadas,
duas novas — "CH− · a busca", "Info · o registro" — e 9 obsoletas apagadas).
São 11 chaves renomeadas (319 linhas nos 29 arquivos de tabela), mais a
substituição do corpo de Novidades 1.1 por uma chave CH+ já existente.
No CH− usa-se o sinal de menos U+2212, como na chave já existente
"CH+ e CH− zapeiam no canto".

## Contextos mantidos (CH+/CH− já têm outro significado)

Aqui a dica de cor continua, porque CH+/CH− fazem zap e a cor é a única
tecla que faz a ação:

- `src/guia.c:4191`: rodapé do guia de canais ("Azul · Guia completo"; no Android já era "CH+"). CH+/CH− saltam categorias no guia; com overlay/canal, mantêm a navegação/zap existente.
- `src/guia.c:4918-4919`: "…Voltar ou Azul fecha".
- `src/player.c:2461`: dica do PiP ("Azul · Tela cheia"; no Android já era "CH+").
- `src/pipintro.c:253`: "Azul volta na hora".
- `src/novidades.c:204` e `src/ajustes_ux_guia_dados.inc:247`: "BAIXO ou AZUL abrem o mini guia" (CH+/CH− zapeiam ali).

## Para o dono decidir (vermelho e verde, mantidos)

- Vermelho: "VERMELHO · o registro" (Novidades 1.1, só LG/Samsung; Android diz "Info").
- Verde: "VERDE · só no Samsung" (Novidades 1.1); "Verde: diagnóstico da Live TV" (`guia.c:4936`);
  "VERDE testa os canais" (guia de teclas e `ajustes_ux_guia_dados.inc:265,325`).
- Amarelo no guia de TV: "AMARELO troca grade e cartões" (`ajustes_ux_guia_dados.inc:25,244`). Aqui
  o CH− é salto de categoria, então a cor continua sendo a única tecla.
- Registro: "Aperte o botão vermelho do controle para abrir e fechar o registro…"
  (duas variantes LG/Samsung, `registro_ilha.inc`); "O registro do app abre no
  botão vermelho"; "O registro só pode ser lido nesta tela (botão vermelho ou
  Ajustes › Sobre e ajuda › Ver o registro na tela)." (`registro_envio.inc`).
- Ajustes/ajuda: "ou botão vermelho"; "Abre o registro do app por cima desta
  tela, ao vivo: o mesmo painel do botão vermelho do controle, para quem não
  tem esse botão."; "Na TV da LG é o botão vermelho do controle. Se a tela
  congelar e o vermelho não responder, o botão verde mostra o painel de arranque.";
  "Na TV da Samsung o vermelho sai do botão de números e cores, abaixo do liga-desliga."
- Diagnóstico: "Testa a rede até o provedor e vários canais, e sugere resolução,
  formato e espera. VERDE no guia também abre."; "Rode o Diagnóstico da Live TV,
  ou aperte VERDE no guia. Ele testa os formatos e diz o que mudar."
- Desenhos mantidos: fileira de cores com vermelho marcado em `registro_ilha.inc`
  (instrução do log); discos vermelho/verde em `novidades11.c`.
- "AZUL", "Azul", "Amarelo", "Verde", "Vermelho" e o antigo resumo sem
  Central ainda existem nas tabelas como entradas sem uso; não aparecem como
  nova dica de atalho. Cores de legenda/tema (`ajustes.c`, `video.c`) não são teclas.

## Central de controle

Segurar CH+ abre a Central de controle; o toque curto faz a ação de CH+/AZUL
(Salvos ou cartão da ilha). Está dito no guia de teclas ("Segurar CH+ abre a
Central de controle."); a lógica é a de `src/chsegura.h` / `src/central.h` e
não mudou.

## Verificação da retomada

- `python3 tools/varredura-i18n.py`, `bash tests/i18n.sh`, `bash tests/wgt_voz.sh`,
  `bash tests/teclas_ch.sh`, `bash tests/spot_ch.sh`, `bash tests/spotlight_regressao.sh`,
  `bash tests/fonte_detalhe.sh`, `bash tests/central.sh`,
  `bash tests/tpk40_teclas.sh`: rc 0.
- `bash tests/player.sh`: rc 134, assert em `tests/player_regression.c:176`
  (`!player_controles_visiveis()` depois de BAIXO). O teste aborta antes do
  cenário de zap; `src/player.c` e `tests/player_regression.c` não foram
  alterados nesta tarefa. Validação física das plataformas não executada.
