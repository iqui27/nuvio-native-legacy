---
name: desktop-mac
description: Compilar, empacotar (Nuvio.app + dmg), provar e consertar o Nuvio desktop no macOS (SDL2 + libmpv, src/desktop_mac.m). Use em qualquer job de desktop Mac, tela larga com sidebar, player libmpv no Mac, dmg, ou bug relatado no app de Mac.
---

# Nuvio desktop — macOS

Docs do repo (fonte de verdade): `docs/desktop.md`, `docs/desktop-verification.md`,
`docs/desktop-polish.md`. Leia antes de explorar.

## Peças

- C comum em `src/*.c`; ponte AppKit em `src/desktop_mac.m` (menu nativo,
  título, fullscreen), cabeçalhos `src/desktop.h`, `src/desktop_native.h`.
- Player: libmpv render API no contexto GL do SDL; vídeo preenche os buracos
  de alfa sob os controles. TV/iOS/Android têm seus próprios backends: não mexer.
- Layout: janela larga = sidebar fixa + busca na barra; janela estreita = layout
  de toque em retrato. Sem barra de dicas.
- Dados: `~/Library/Application Support/Nuvio/`; `NUVIO_DADOS` sobrepõe.
  Teste sempre com pasta descartável, nunca `~/.nuvio` (já destruiu o perfil do dono).

## Comandos

```sh
brew install sevenzip mingw-w64 mpv          # uma vez
export NUVIO_PROPERTIES="/Users/hrocha/Projetos/Pessoal/LG WEB/NuvioWeb-0.3.38-beta/local.properties"
bash tools/mac.sh                            # roda local (dev), dados em ~/.nuvio — só o dono
bash tools/mac-pacote.sh                     # Nuvio.app + Nuvio-mac-arm64.dmg em /Volumes/ExternalSSD/tmp/rev/desktop/
bash tests/desktop-final.sh                  # geometria, rotas, teclado/gamepad, fullscreen, smoke libmpv
python3 tools/desktop-capture.py             # capturas larga/estreita + mock vs app
python3 tools/desktop-proof.py [--mount-only]
```

`tests/desktop-packages.sh` refaz só os pacotes. Sem `NUVIO_PROPERTIES` o
app abre dizendo "Este pacote foi montado sem servidor".

## Regra de captura (dono usa o mesmo Mac)

Nunca roubar foco: abrir com `open -g -j`, `SDL_HINT_MAC_BACKGROUND_APP` /
política accessory (`NV_CAPTURA`), nada de raise/activate. Prova por readback
GL; `screencapture -l` só para vibrancy/cantos. Entrada só por `SDL_PushEvent`
com o ID real da janela (nunca CGEventPost/osascript). Matar as instâncias
abertas no fim. `NUVIO_UI_PREVIEW=1` = catálogo offline determinístico.

## Skills Axiom úteis (só a que servir)

- `axiom-macos` (roteador): janelas, menus, AppKit bridging, sandbox, distribuição.
- Assinatura/notarização: `axiom-security` (code signing). Hoje o app é
  ad-hoc, não notarizado; Gatekeeper pede "Abrir" no Finder.
- GL/Metal e frame pacing: `axiom-graphics`, `axiom-display-performance`.
- Travamento/leak: `axiom-hang-diagnostics`, `axiom-memory-debugging`, `axiom-lldb`.
Ignore as partes de SwiftUI: o app é C + SDL com uma ponte ObjC fina.

## Prova mínima de qualquer mudança

Teste-alvo FAIL->PASS, `bash tests/desktop-final.sh` rc 0, captura do estado
afetado (PNG via readback), `bash tools/build-local.sh` rc 0 (TV intacta).
Nada de publicar, assinar com identidade real ou instalar em outro Mac.
