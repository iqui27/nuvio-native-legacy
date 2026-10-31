---
name: desktop-windows
description: Compilar em cruzado (mingw-w64 no Mac), empacotar (Nuvio-win64.zip) e provar via CrossOver/Wine o Nuvio desktop para Windows x64 (SDL2 + libmpv). Use em qualquer job de Windows, bug relatado no Nuvio.exe, DLL faltando, ou porte de código POSIX para Windows.
---

# Nuvio desktop — Windows x64

Não existe máquina Windows: tudo é compilado no Mac com mingw-w64 e provado
no Wine. Docs: `docs/desktop.md`, `docs/desktop-verification.md`.

## Peças

- Mesmo C de `src/*.c`. Diferenças de plataforma atrás de `_WIN32`; shims POSIX
  em `tools/windows-compat/` (pthread.h, dlfcn.h, arpa/inet.h, sys/*, platform.h).
  Antes de criar `#ifdef _WIN32` novo, veja se o shim já resolve.
- GL: `src/windows_gl.h` (carrega funções GL 2.1; driver OpenGL 2.1 obrigatório).
- Player: libmpv (`libmpv-2.dll` do shinchiro/mpv-winbuild-cmake, fixado por
  SHA-256, + alias `mpv-2.dll`, loader Vulkan). Threads do app via SDL.
- Fora do Windows: servidores HTTP de pareamento/log da TV, relays de mídia,
  motores P2P nativos. Não religar sem pedido.
- Dados: `%APPDATA%\Nuvio\`; `NUVIO_DADOS` sobrepõe.

## Comandos (no Mac)

```sh
brew install sevenzip mingw-w64 mpv
export NUVIO_PROPERTIES="/Users/hrocha/Projetos/Pessoal/LG WEB/NuvioWeb-0.3.38-beta/local.properties"
bash tools/windows.sh            # Nuvio-win64.zip + windows-dlls.txt em /Volumes/ExternalSSD/tmp/rev/desktop/
python3 tools/desktop-wine.py    # abre o .exe no CrossOver/Wine com dados isolados, toca o trailer, lê PNG do framebuffer
bash tests/desktop-final.sh      # inclui link cruzado contra headers e import lib oficiais do Windows
```

Dependências (SDL2, SDL_image, SDL_ttf, curl, zlib, mpv, Vulkan) vêm de
`tools/desktop-deps.sh`, todas com SHA-256 fixado; nunca baixar sem checksum.
Toda DLL importada (transitiva) tem de estar no ZIP: o checker confere.

## Armadilhas

- `libwinpthread-1.dll` é exigida pelo TLS do GCC: precisa ir no ZIP.
- Caminhos: use as funções de dados do app, nunca `/` fixo nem `HOME`.
- Wine ≠ Windows real: prova de Wine vale para fluxo e pixels, não para driver
  GL de fabricante. Diga isso no relatório quando importar.
- O helper do Wine mata o próprio filho e o launcher; não deixe `wine` vivo.
- Captura nunca rouba foco do Mac do dono (mesma regra do `desktop-mac`).

## Prova mínima

Teste-alvo FAIL->PASS, `bash tools/windows.sh` rc 0, `python3 tools/desktop-wine.py`
com frame de vídeo não-preto, `bash tests/desktop-final.sh` rc 0,
`bash tools/build-local.sh` rc 0. Sem publicar.
