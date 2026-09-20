# Portar para Hisense (VIDAA): o que se sabe e o que custaria

Pesquisa de 19/09/2026. Nada aqui foi provado num aparelho: não há TV VIDAA
nesta casa. O que está escrito vem de documentação pública, de dois projetos
que já rodam na plataforma (Stremio e Jellyfin) e do que o nosso próprio port
Tizen já resolveu.

## 1. O que é a plataforma

- VIDAA é o sistema da Hisense (também Toshiba, Sharp, AKAI). **No Brasil e na
  Europa** os Hisense vêm com VIDAA; nos EUA o mesmo modelo (U7, U8) vem com
  Google TV. Antes de prometer algo a alguém, perguntar qual loja a TV tem.
- App VIDAA = **HTML5 em contêiner Chromium**. Não existe binário nativo nem
  pacote como o `.ipk`/`.wgt`: o que a loja e o launcher registram é uma **URL**
  hospedada pelo desenvolvedor, mais ícone PNG (220–400 px). O Jellyfin
  oficial (`org.jellyfin.vidaa`, 34 MiB) empacota o web app, mas isso é via
  programa de parceiro.
- User agent traz `VIDAA` (2020+) ou `Hisense` (até 2019) e a versão do Chrome
  do kernel. Nenhuma fonte pública crava a versão do Chromium por geração
  (U5/U6/U7/U9). O Stremio roda **WASM** e Service Worker lá, então é Chromium
  recente o bastante para nós.
- Versão vista no Brasil pelo QA do Jellyfin: **Hisense 43Q6QUV, VIDAA U09.01,
  chip MTK9603** (H.264 direto, HLS, legendas externas: ok).

## 2. Como um app chega na TV

Dois caminhos, nenhum parecido com o nosso hoje.

**Loja oficial.** Não há portal público de desenvolvedor. É formulário
"Become a partner", revisão, conta no Partner Portal, requisitos técnicos e de
design, QA, publicação. A documentação técnica (WebApp Development Guide) fica
atrás desse muro; a cópia pública que circula é de 2020. Custo não publicado;
relatos falam em taxa para SDK/documentação/publicação. O Stremio Lite entrou
na loja em 12/12/2024 por esse caminho.

**Sideload.** O launcher só instala pelo JS `Hisense_installApp(url, ...)`, e
essa função só responde quando chamada de uma página em `vidaahub.com`. O
truque comunitário (`trialuser/vidaa-appstore`, e o instalador do
`NoobyGains/stremio-vidaa-tv`):

1. Modo desenvolvedor na TV: Ajustes › Sistema › Sobre, digitar **1234**.
2. Rodar na LAN um servidor DNS + HTTPS que responde por `vidaahub.com`.
3. Apontar o DNS da TV para esse PC, abrir `https://vidaahub.com/` no
   navegador da TV (aceitar o certificado), clicar "instalar".
4. Devolver o DNS e reiniciar.

O `hisense://debug` de antes de 2022 **não existe mais** em firmware 2024+.
Em projetores Hisense o firmware aceita a chamada e não instala nada; lá o
jeito é favorito no navegador.

Consequência para nós: **o app precisa de hospedagem**. O `.wgt` do Tizen
carrega tudo de dentro do pacote; na VIDAA o `index.html`, o `.wasm` (~ 10 MB)
e as artes embarcadas seriam servidos de um domínio nosso a cada arranque
(Service Worker faz cache depois do primeiro).

## 3. O que já temos que serve

O port Tizen é exatamente a arquitetura que a VIDAA pede: `src/*.c` em WASM
(Emscripten upstream, `tools/tizen.sh`), SDL2 num `<canvas>`, vídeo por uma
ponte JS atrás do canvas (`src/video_tizen.c`), imagens decodificadas pelo
navegador fora do heap (`src/webp.c`, `navegador_decodificar`), dados em IDBFS,
teclas registradas pela shell (`tools/tizen-shell.html`).

Do que é específico de Samsung, só três peças:

| Peça | Samsung | VIDAA |
|---|---|---|
| Vídeo | `webapis.avplay` (1250 linhas em `video_tizen.c`) | `<video>` HTML5, ou `omi_platform.sendPlatformMessage` (player nativo, mapeado pelo Stremio) |
| Teclas | `tizen.tvinputdevice.registerKey`, Back = 10009 | keyCodes próprios, a descobrir no aparelho; coloridas existem (Stremio usa vermelha/amarela/azul) |
| Empacote | `config.xml` + `.wgt` | URL + ícone; `Hisense_installApp` |

`rede.c`, `dados.c`, `tex_cache.c`, `avisos.c`, tudo o mais: já tem ramo
`__EMSCRIPTEN__` e não sabe em que TV está.

## 4. Os riscos, em ordem

**4.1 Threads.** O build Tizen usa pthreads de verdade (28 arquivos criam fio;
o leque paralelo de `addons.c` é o que derrubou a busca de fontes de 16 s para
2 s). Pthread em WASM exige `SharedArrayBuffer`, que só existe com a página em
**isolamento de origem**: cabeçalhos `Cross-Origin-Opener-Policy: same-origin`
e `Cross-Origin-Embedder-Policy: require-corp` **no servidor que hospeda o
app**. Isso exclui GitHub Pages (não deixa pôr cabeçalho); Cloudflare Pages ou
o mesmo Worker das recomendações servem. E ainda assim ninguém provou que o
contêiner da VIDAA honra o isolamento: a Samsung documenta SAB no `.wgt`; a
Hisense não documenta nada, e o WASM do Stremio (`stremio-core`) é de um fio
só, então o exemplo que existe não responde a pergunta. Se SAB não vier, o
port vira **outro app**: sem fios, ASYNCIFY para tudo, busca de fontes em
série. Não é um dia de trabalho.

**4.2 Vídeo.** O que o Stremio mediu na VIDAA com `<video>`:

- H.264 e HEVC até 4K: decode nativo.
- **Dolby Vision em MKV: nativo em 4K. DV em MP4: derruba o navegador.** O
  inverso da LG, onde o MP4 é o que toca DV (e é por isso que 1.3.2 dá bônus
  ao MP4 só na LG). A regra de pontuação teria um terceiro ramo.
- AV1: não; acima de 4K: tela preta.
- Tecla F (fullscreen): derruba o navegador.
- Referer/cabeçalhos: `<video>` não manda, igual ao AVPlay (#59 continua).
- Faixas de áudio/legenda embutidas: `audioTracks`/`textTracks` do HTML5 são
  parciais em Chromium de TV; o Jellyfin deixou "troca de faixa" como pendente
  até a Hisense confirmar. Nossa `video.h` expõe 30 funções (escolha de
  áudio, legenda embutida, estilo de legenda, buffer, HDR, SDR forçado);
  parte disso degradaria com honestidade como já fazemos no Tizen.

**4.3 Memória.** O heap de 256 MiB fixo do Tizen tende a ser o mesmo teto
(Chromium embarcado, TV de 1–2 GB). O caminho de decode pelo navegador da
1.3.2 já resolve o pior (#69). O Stremio avisa quando o heap JS passa de 85% e
tem watchdog de UI congelada: sinal de que a plataforma mata aba com
frequência.

**4.4 Sem aparelho, sem port.** Não existe emulador VIDAA. A lição do
emulador Tizen (dois dias, nunca subiu) vale dobrada: cada uma das perguntas
acima só se responde numa TV. Sem uma Hisense VIDAA na mesa, o que dá para
fazer é preparar o terreno (item 6) e esperar alguém com a TV testar — e esse
alguém vai testar às cegas junto com a gente.

**4.5 Distribuição.** Sideload por DNS é pedir a cada usuário que rode um
servidor DNS em Python com root na LAN. Público real só pela loja, que é
parceria com a Hisense (revisão, requisitos, prazo deles). O Stremio levou
anos para chegar lá.

## 5. Estimativa

Com uma TV VIDAA na mesa e SAB funcionando:

| Passo | Esforço |
|---|---|
| Hospedagem com COOP/COEP + prova de que WASM/pthread/SDL sobem na TV | 1 dia |
| `tools/vidaa-shell.html`: teclas, `Hisense_*`, ciclo de vida (`window.close`, visibilitychange) | 1–2 dias |
| `src/video_vidaa.c` sobre `<video>`: tocar, buscar, faixas, legenda externa, DV/HDR | 4–6 dias, com a TV do lado |
| Regra de pontuação (MKV para DV na VIDAA), `plataforma` = `vidaa` em avisos/atualização/registro | 1 dia |
| Instalador DNS (adaptar `trialuser/vidaa-appstore`) + docs | 0,5 dia |
| Sobra para o que a TV vai mostrar que ninguém previu | 1 semana |

Sem SAB: somar a reescrita para um fio só, que não cabe numa estimativa
honesta hoje.

## 6. O que dá para fazer agora, sem TV

1. Servir `build/tizen/index.html` + `.wasm` de uma URL com COOP/COEP (o
   Worker das recomendações serve). Custa uma tarde.
2. Pedir num issue a quem tenha Hisense VIDAA que abra essa URL no navegador
   da TV e mande uma foto e o `navigator.userAgent`. Três perguntas saem daí:
   o Chromium é qual, `crossOriginIsolated` é `true`, o canvas desenha.
3. Só com "sim" nas três vale abrir a frente do vídeo.

## Fontes

- https://github.com/trialuser/vidaa-appstore — mecanismo `Hisense_installApp` + DNS
- https://github.com/NoobyGains/stremio-vidaa-tv — codecs medidos, quedas do navegador, modo desenvolvedor 1234, APIs `omi_platform`
- https://github.com/jellyfin/jellyfin-web/issues/8006 — QA em Hisense 43Q6QUV / U09.01 / MTK9603 (Brasil)
- https://github.com/NuvioMedia/NuvioTVSmart/issues/790 — pedido no Nuvio original, fechado "not planned"
- https://blog.stremio.com/stremio-lite-released-for-tvs-with-vidaa-os-hisense-toshiba-sharp-etc/ — loja oficial em 12/12/2024
- https://www.vidaa.com/become-a-partner/ — único caminho para a loja
- https://www.scribd.com/document/825727528/WebApp-Development-Guide-for-VIDAA — guia técnico (cópia de 2020)
- https://deviceatlas.com/blog/list-smart-tv-user-agent-strings — user agents VIDAA/Hisense
