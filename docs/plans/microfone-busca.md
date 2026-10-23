# Botão de microfone nas buscas (Samsung .tpk e .wgt)

## O que é

Nas duas buscas — a da barra lateral e o Spotlight do CH- — o `.wgt` mantém o teclado da TV
para digitar; o `.tpk` usa o teclado do Nuvio Legacy. Ao lado do botão "Digitar pelo celular"
fica um botão de microfone.
OK nele abre o **teclado da Samsung só para usar o microfone dele**:

1. OK no microfone → o teclado da Samsung sobe; o campo diz "Aperte o microfone do controle e
   fale." (não "Ouvindo…": o teclado ainda não ouve).
2. A pessoa aperta o microfone do controle e fala.
3. Concluído (ou o teclado fechando sozinho depois da fala) põe o texto inteiro no campo e
   busca; Voltar cancela sem mexer no campo; o aviso some.

O teclado da Samsung cobre a tela e não dá para escondê-lo e espiar o app. Por isso, enquanto ele
está aberto, **nada muda por baixo dele**: o texto do meio do caminho fica com o teclado
(`sistexto.c`, `micTeclado`), sem busca, resultados ou cartazes montados para ninguém ver. Só o
texto final entra, quando o teclado fecha.

É a **única** via de voz na Samsung: nada de STT da TV, de `webapis.voiceinteraction` nem de
ditado pelo celular, e nenhuma permissão nova (quem ouve é o teclado da Samsung). O canário
`NvTextoCanario=1` do `.tpk` (Texto.cs, STT, de antes) é um build de diagnóstico à parte e não
entra na release.

## Por que o teclado da Samsung

O NuvioTVSmart e o Stremio `.wgt` usam o teclado da Samsung como teclado principal, e o
microfone do controle dita nele. A Samsung não dá a terceiros uma API de busca por voz com
texto livre (a VoiceInteraction entrega comandos e "Selecionar {título}", e só no `.wgt`), nem
uma chamada para o teclado já abrir ouvindo.

## Quando o botão aparece

Ajuste **Tela inicial › Mais opções › Barra lateral › Botão de microfone na busca** (só no
`.tpk` de Tizen 6+ e no `.wgt`; só desta TV):

| Valor | Comportamento |
|---|---|
| **Automático** (padrão) | Aparece, menos na TV que diz não ter microfone (`http://tizen.org/feature/microphone` = false: `Tizen.System.Information` no `.tpk`, `tizen.systeminfo.getCapability` no `.wgt`). Sem resposta da TV, aparece. |
| Ligado | Sempre aparece (TV detectada errado). |
| Desligado | Nunca aparece. |

Sem o campo do teclado da Samsung (o host não conseguiu montar o IME), nunca há botão.

Nos Ajustes a linha segue o padrão das outras: ícone de microfone, a frase de benefício ("Busque
falando, pelo microfone do controle."), a ajuda com os três valores, o "O que muda" no `.tpk` (para
digitar continua o teclado do Nuvio) e uma cena gerada própria (`ajcHMicBusca`): a barra da
busca com o microfone no acento ao lado do celular, e o laço teclado da Samsung → controle
ouvindo → texto chegando. No Automático o controle ganha o selo de "a TV tem microfone"; no
Desligado fica a vaga vazia e o controle apagado. Os valores ficam no seletor da própria tela.

## Peças

- `tizen-tpk/Voz.cs` (build de release; o canário `NvTextoCanario=1` usa `Texto.cs`): um
  `TextField` 2x2 invisível, focável só enquanto o botão o usa, e o IME da Samsung sobre ele.
  Registra o botão com o que a TV respondeu sobre microfone. Nenhuma permissão nova: quem ouve
  é o teclado da Samsung.
- `tools/tizen-shell.html` (`.wgt`): o `<input>` escondido e o `window.nvTexto` que já
  existiam continuam servindo à digitação e também ao botão. `nvTexto.microfone` guarda o
  `getCapability`; o teclado fechando sozinho (blur) com texto entrega, como no `.tpk`.
  A ponte continua disponível sem `window.tizen` (PC/VIDAA), como na integração.
- `src/entrada_texto.c`: `nv_tpk_voz_registrar` (host → C), `texto_sistema_mic_modo` (o
  ajuste) e `micPermitido()` comum ao `.tpk` e ao `.wgt`. No `.tpk` de release anuncia
  `TS_VOZ` sem `TS_TECLADO`. No `.wgt`, com `window.nvTexto`, anuncia `TS_TECLADO` mais
  `TS_VOZ` quando permitido pelo ajuste e pela capacidade: OK no campo abre o teclado da TV,
  mesmo com o botão desligado. `texto_sistema_voz_pelo_teclado()` diz que a voz é esse botão.
- `src/sistexto.c`: com o teclado da Samsung aberto pelo botão, o estado é `ST_DIGITANDO`,
  `st_aviso()` é o aviso do controle, e o texto só sai em `ST_FIM`; tudo limpo quando o teclado
  fecha.
- `src/teclado.c`: o teclado modal (chaves, nomes) não mostra "Falar" para esse caso.
- `src/main.c`: repassa o ajuste a cada quadro (nas outras plataformas não faz nada).
- Ajustes: `src/ajustes.c` (opção, chave local `micBuscaLocal`, ajuda, benefício, "O que muda"),
  `ajustes_ux_tela.inc` (só no `.tpk`), `ajustes_ux_cenas_home.inc` (a cena),
  `ajustes_ux_visual.inc` (ícone), `ajustes_ux_dados.inc` (palavras da busca dos Ajustes),
  `ajustes_ux_secoes.inc` (a cena de referência marca a linha Buscar da barra lateral).
- Busca e Spotlight não mudaram: o botão já existia ao lado do celular, preso a
  `st_voz_disponivel()`.

## Outras plataformas

Android segue com o próprio ditado; LG não anuncia voz (sem botão); o `NuvioTpk40` (Tizen 4/5)
não tem o host do teclado e fica sem botão. O ajuste só aparece no `.tpk` de Tizen 6+ e no `.wgt`.

## Testes

- `tests/tpk_voz.sh`: a ponte com um host fingido (ajuste × microfone, aviso, nada por baixo
  do teclado, Concluído, Voltar, fechar a tela).
- `tests/wgt_voz.sh`: o mesmo no caminho `__EMSCRIPTEN__` (com um `emscripten.h` fingido) e o
  JS de verdade do shell no node (detecção, Concluído, Voltar, Cancel, fechar sozinho).
  `tests/entrada_texto_wgt.sh` (emsdk + Chrome) foi ajustado ao botão.
- `tests/ajustes_ux_dados.sh`, `tests/ajustes_secoes.sh`: o ajuste (3 valores, padrão
  Automático, local) e a tela.
- `tools/idiomas.py`, `tools/varredura-i18n.py`: as 5 strings novas nas 29 tabelas (aviso do
  controle, nome do ajuste, ajuda, benefício, "O que muda").

## Correção da integração do PR #465 (12/10/2026)

- O `.wgt` **mantém** o teclado da TV. Removida a atribuição contrária
  no comentário de `entrada_texto.c`; o botão de microfone permanece.
- `wgt_voz.sh`: **rc 1 → 0** ao exigir `TS_TECLADO | TS_VOZ`,
  `st_ime_disponivel() == 1` e digitação pelo IME. Com o microfone desligado ou ausente,
  `TS_TECLADO` continua disponível. Sem `nvTexto`, nenhuma capacidade é anunciada.
- `wgt_voz_shell.js`: **rc 1 → 0** para a ponte sem Tizen (PC/VIDAA), restaurando a integração.
- Mantidas as chamadas `Cancelar` em `OnPause`/`SoltaTudo`: removê-las faz
  `tpk_voz_host.sh` falhar (**rc 1**) nos casos de pausa, fechamento e abertura enfileirada;
  restaurá-las passa (**rc 0**), incluindo compilação do host nas APIs 8/9/11.
- `ajustes_secoes.sh`: **rc 1 preexistente**, reproduzido com os arquivos do HEAD anterior
  à correção: sete opções `AJ_PROF_*` ausentes do catálogo. Não relacionado ao teclado.

## Release 2.0.7 — #585

On native Tizen, Automatic keeps the microphone button whenever the host is
registered. The TV's `feature/microphone` flag does not detect the remote's
microphone, so a negative response must not hide the Samsung keyboard entry
point. On still forces the button; Off still hides it. WGT detection is unchanged.
The existing settings help describes WGT capability detection; native Tizen
uses host registration instead.

The remote's microphone button opens system search outside the Samsung
keyboard. Open Nuvio's on-screen microphone button first, then dictate while
the Samsung keyboard has focus. This does not remap the remote's system key.

## Falta na TV

Se o teclado da Samsung sobe sobre o app (mais provável no 8/9, que desenha na janela
principal, e no `.wgt`) e se o microfone do controle dita nele. O log diz cada passo:
`[texto] host voz …` no `.tpk`, `[texto] wgt: …` no `.wgt`.
