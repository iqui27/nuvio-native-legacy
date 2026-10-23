# Onde Rust compensa no Nuvio — mapa para 2.0.6

Estudo, sem alteração de produto. Snapshot: `9eb8cc2f`, branch `agente/205-rustmapa`, base `integracao/2.0.5`; `git describe`: `v2.0.5-224-g9eb8cc2f`. É o código recebido para planejar 2.0.6, não uma tag 2.0.6 publicada.
Atualiza [rust-piloto.md](rust-piloto.md), escrito em `8ed4517c` (v2.0.3).
Dono (13/10): “e sobre o rust, temos que mapear onde usar”; decisão anterior em `respostas-dono.json`: “apenas o que for melhorar e tb usar rust no que for resolver nossos problemas tb”.
**Recomendação:** concluir sanitizadores obrigatórios; depois autorizar um único piloto `mkvcore`. Não reescrever módulos só pela linguagem. **Suspeita** identifica ganho/risco ainda sem prova; nenhum Rust foi compilado, nenhuma ferramenta instalada.

## 1. Evidência reaberta desde v2.0.3

Método: `git log --no-merges --format='%h %s%n%b' v2.0.3..HEAD -- src/` (383 commits), filtro por crash/UAF/UAR/OOB/race/corrida/overflow/truncamento/heap/ASan/TSan/mutex/pilha e leitura dos achados, APIs e builds. É amostra de mensagens, não auditoria de todos os diffs nem novo censo de quedas.

| Commit(s) | Área | O que mudou / evidência | Rust ajudaria? |
|---|---|---|---|
| `8dd273f3`, `9d78c75a`, `3e3044eb` (#344) | `guia`, `epg`, `xtepg` | `ps[24+]` na busca causava SIGSEGV; ASan reproduziu OOB. Retorno limitado a `cap`, defesa no chamador e paginação corrigidos em C | Slice interno ajuda; ABI C ainda precisa limitar retorno. **Problema já fechado** |
| `a6869445` | `streams`, `fontesmortas` | Varredura de candidatas sob mutex e recusa de geração velha; regressões antes/depois | Posse + acesso sob guarda ajudam se o laço e seu estado forem Rust; geração velha é lógica |
| `40942e04` | `atividade`, `sessao` | Consentimento/identidade revalidados após renovação; pedido antigo não remove evento substituto | Race de protocolo, não toda ela data race: `Mutex<T>` sozinho não impede contexto obsoleto |
| `440cd262`, `dc465f05` | `addons`, `watchstate_envio`, `watchstate` | Snapshot do destino sob trava, identidade revalidada antes do POST, limpeza da fila expirada | Tipos ajudam na posse; identidade/validade exigem os mesmos testes de contrato |
| `5c5f62cf` | `descoberta` | Publicação de episódios atômica com invalidação do pedido; regressão em `epsrapido` | Só com protocolo de publicação incluído na fronteira Rust |
| `8bb8b811` | `addons` / leitores de fonte | Campo `fonte` publicado e lido com atômico relaxado | Acesso compartilhado em Rust seguro exige sincronização; ordem continua decisão do protocolo |
| `e7f76855` | `video`, `main` (webOS) | Join do fio Luna antes de liberar handle; snapshot atômico do screensaver. Revisão da queda ao acordar | Wrapper com posse ajuda; o handle/vendor e os callbacks C continuam `unsafe` |
| `f337a029` | `descoberta`, `listas` | Lotes grandes de `CatItem` saem da pilha dos workers para heap | Rust também pode estourar pilha; alocação em heap em C resolve mais barato |
| `40a2f768` | `espaco.inc` | Buffers `PATH_MAX` para `realpath`/fortify Android | Buffer de saída de C permanece obrigação FFI; correção localizada suficiente |
| `891e730d` | `regexjs` | Lookbehind limitado em descrição longa | Bug semântico; reescrever não garante correção |
| `65491e7b`, `00b4a937`, `09ce19a0` | adaptador DTS webOS 3 | Queda na 2ª–4ª sonda; `RTLD_NODELETE` e cache da sonda. `1d7264c1` ressalva o mecanismo suspeito do `dlclose` | Rust não conserta fios internos de biblioteca descarregada |
| `35d0b5b8`, `10709861`, `046fe5f5` | `rede`, `tex_cache` | Android reportava corpo baixado como 0 bytes; conectividade só após retry | Contagem/estado, não corrupção de memória comprovada |
| `dd850f1b`, `ad10c6d9`, `f1875d99` | `legsync`, `autosync` | Referência fraca/incompleta, troca indevida e offset limitado | Política/tempo; Rust não resolve automaticamente |

**O que continua pesando:** `696afa74` (anterior à janela) corrigiu UAR da conferência de fontes: 128/456 Android (28%), 497 tombstones e ~30 webOS segundo o estudo anterior. `catalogo` teve heap OOB em `6c504c0b`, `33fd1fec`/`52dc67ef`. Esses dados são históricos, não taxa atual.
Não achei novo heap-OOB/UAR de escala comparável nesta amostra. Isso não prova ausência de bugs em parsers. Truncamento em buffers fixos, overflow de inteiros, limites de eventos e geração obsoleta precisam de contratos explícitos também em Rust; indexação segura pode trocar corrupção por panic/abort se o parser não usar retornos de erro.

### 1.1 Superfície atual: o que é realmente novo?

`git diff --name-only --diff-filter=A v2.0.3 HEAD -- src/` mostra, entre outros, **`fontesmortas`, `watchstate*`, `webosver`, `espaco.inc`, `memlog`** novos. P2P, DTS, `video_tpk`, `regexjs`, `js`, `legsync`, `autosync`, `xtream`, `proxyts` e `celular` **já existiam em v2.0.3**; são áreas recentes que foram reavaliadas, não módulos todos criados nesta janela.
Linhas abaixo incluem comentários/brancos. Includers = arquivos de `src/` (`.c/.h/.inc/.cpp`, recursivo) com include direto do header homônimo (normalizado pelo basename, inclusive `../js.h` e `dts/dts_engine.h`); não é contagem de chamadas nem de dependentes transitivos.

| Área atual (linhas / includers) | Entrada e limite da proteção | Evidência nova / alternativa barata |
|---|---|---|
| P2P: `p2p` 354/5, `p2pmotor` 965/8, ponte motor 252/0 | Torrent/rede via nuvio-engine/libtorrent externo; ponte Rust não torna C++ seguro | `d1085031`, `d453fd1c`: quota/janela de disco (#334), não UAF/OOB demonstrado; manter testes C + motor |
| `dts/*.c` 1882; adaptador C++ 361; engine 733/4, playback 709/3, pipeline 122/3, subtitles 214/2 | Range concorrente, FFmpeg demux/codec e entrega de frames ao Starfish | Risco de posse **suspeita**; bugs de sonda/ciclo de vida acima. Atualizar/instrumentar dependências antes de substituir pipeline |
| `video_tpk` 1087/0 | ABI `nv_tpk_*`, eventos/host .NET; sem header homônimo | #412 (`53584d9b`, `7a612640`): seek/stop/dispose; testes de estado mais baratos |
| `js` 347/57; `regexjs` 681/3 | Parser JSON **próprio**; regex própria de pacote de selos sobre texto de addon | JSON já endurecido em `73046936` (antigo); regex tem limites de passos/profundidade, fix semântico novo |
| `legsync` 851/6; `autosync` 1000/2; `audvad` 206/2; `audsync` 400/3 | Cues/PCM e alinhamento DSP | Sem nova corrupção comprovada; testes de alinhamento e cancelamento antes de port |
| `xtream` 701/9; `epg` 1327/7; `xtepg` 271/5; `listas` 812/6 | JSON do provedor, XMLTV/M3U e publicação | #344 foi contrato de saída, não parser XMLTV comprovadamente inseguro |
| `rede` 3242/74; `proxyts` 772/4; `celular` 520/3 | Cliente via curl/JNI/XHR; servidor loopback TS/HLS; servidor LAN do QR | Parsing próprio residual e HTTP de entrada: segurança **suspeita**, sem queda atribuída nesta amostra |

Não há servidor HLS próprio em `src/dts/`: usa AVIO/`libavformat`. `proxyts.c` escuta `127.0.0.1` e ingere HLS/TS remoto; `celular.c` escuta `INADDR_ANY`, valida Host/Origin/token e limita corpo/prazo. O motor P2P externo também serve HTTP loopback; sua implementação não está em `src/`. Não atribuir seus parsers ao adaptador DTS.

### 1.2 A fase 0 aconteceu? Sim, com obrigatoriedade

| Medida reproduzida no git | `8ed4517c` | `9eb8cc2f` | `6559dc3d`+runner (este ramo) | Interpretação |
|---|---:|---:|---:|---|
| `tests/*.sh` / `tests/*.c` | 548 / 567 | 666 / 683 | **676 / 692** | Crescimento da suíte |
| Scripts com texto `SANITIZE` | 152 | **183** | **188** | +5; capacidade declarada, agora com flag única e runner |
| Variáveis de sanitizador distintas | `SANITIZE`, `NV_SANITIZERS`, `NV_TSAN`, `TSAN`, `FONTEFALHA_SANITIZE` | idem | **1: `SANITIZE=1` \| `SANITIZE=thread`** | Unificada em `6559dc3d`; nomes antigos removidos dos scripts |
| Scripts com `SANITIZE:-0` (opt-in) | — | — | **179** |Ligados só com `SANITIZE=1` |
| Scripts com `SANITIZE:-1` (ASan por padrão) | — | — | **8** | `capmkv`, `capmkv_url_longa`, `catcachecorrida`, `cateps_corrida`, `colhash_corrida`, `herosinopse_corrida`, `htmlq`, `localizar_corrida` |
| Scripts com linha não comentada `-fsanitize=thread` | **28** | **28** | **28** | Lista estática dentro de `tools/sanitiza.sh`; `sync_resumo_corrida` e `trakt_play_corrida` têm TSan fixo no código e não aceitam ASan |
| Scripts com `-fsanitize=` e sem texto `SANITIZE` | 9 | 10 | **10** | ASan sempre ligado (`descanso_wake`, `js_limites`, `leitura`, `limpa`, `rede_buffer`, `redeurl`, `selospacote`, `tpkdesp`) e TSan obrigatório (`trakt_play_corrida`, `sync_resumo_corrida`) |
| `.github/workflows` versionado | Ausente | Ausente | Ausente | Sem CI no repositório; o gate roda no Mac do dono |
| Runner de sanitizador | Não existe | Não existe | **`tools/sanitiza.sh`** (`5377eb54`) | Reaproveita `tests/*.sh`; ASan+UBSan na suíte e TSan em job separado, nunca no mesmo build; rc≠0 por erro de sanitizador bloqueia; `SKIP` e dependência ausente saem em contagem à parte, nunca como passed |
| `tools/testa-tudo.sh` passa flags de sanitizador | Não | Não | **Sim, com `--sanitizers`** (`6a353afa`) | Integração pode ser bloqueada pelo gate; sem a flag nada muda |
| Harness libFuzzer/AFL em `tests/`/`tools/` | Não identificado | Não identificado | Não identificado | Fixtures/regressões não substituem fuzz de bytes hostis |
| ASan do app inteiro | Manual no histórico | `15cd67e5`: `descanso_wake` exercita GL/cache com ASan/UBSan | idem + suíte completa instrumentada | Automatizado, mas ainda não reproduziu queda da TV |

`tools/depura-mac.sh` continua sendo LLDB, sem `-fsanitize`, e não é o runner. `tpk.sh` tem guards ELF; `tests/tpk40_tls.sh` pode sair 0 com “pulado” sem Docker/binários — o runner conta isso como sem prova, não como passou. Observado no primeiro uso: `ASAN_OPTIONS` não pode levar `detect_leaks` no macOS (o clang aborta o run inteiro). `tests/abrirtitulo.sh` não linka sem `addons_lista_*`, e `tests/p2pmotor_real.sh` depende do motor P2P pré-compilado, incompatível com o `src/` atual — os dois saem como dependência, não como falha. Os testes que ligam a SDL2 do brew (`aovivo`, `cwremover`, `bordafileira`, `atividade_corrida`, `artetamanho`) travam no dyld sob ASan, exatamente o que `tests/bordafileira.sh:5` já descreve: o runner os conta como “sem terminar”, que não é prova nem é erro de sanitizador.

### 1.3 Primeira execução obrigatória (este ramo, Mac do dono, `TMPDIR` em SSD externo)

`bash tools/sanitiza.sh`, suíte inteira, ASan+UBSan em 6 paralelo (`SANITIZA_TIMEOUT=900`) e TSan serial. **588 execuções** = 560 ASan + 28 TSan; os 114 `*_shot`/`cinematic`/`director`/`*_perf` ficam fora, como em `testa-tudo.sh`, e os 2 com TSan fixo contam só no passo thread.

| Veredito | Qtd. | Leitura |
|---|---:|---|
| `ok` | 514 | Passou instrumentado |
| `ok*` | 2 | Passou com casos pulados no log (`herocat_regras`, `recomenda_e2e`) |
| `ERRO` (sanitizer) | **0** | Nenhum relatório de ASan/UBSan/TSan nas 588 execuções |
| `FALHA` | 31 | 24 já falhavam **sem** sanitizer (iguais aqui) |
| `DEP` | 23 | Sem dependência: FFmpeg/curl headers, motor P2P velho, `tpk_x64`, link de `abrirtitulo` |
| `SKIP` | 0 | Nenhum teste pediu pular |
| `TIMEOUT` | 18 | Sem prova; a maioria é a classe SDL2+ASan do dyld |

Os 31 `FALHA` foram rodados de novo **sem** sanitizer: 24 reproduzem igual (`ajustes_padroes`, `ajustes_ux_dados`, `ajustes_ux_interacao`, `detail_eps_repede`, `dts_pipeline`, `espaco`, `gpunivel_ajuda`, `player`, `proxprefetch`, `conta`, `cachearte-wasm`, `carrossel_cede`, `corviva`, `discord_ca_assets`, `dts_sonda_cache`, `dts_sonda_repetida`, `entrada_texto_wgt`, `i18n`, `ponteiro_telas`, `relogiofim`, `salvos_segurar`, `tizen-despedida`, `tizen-globalthis`, `trailer-hero-contract`) — estão fora do escopo desta fase. Os 7 restantes passam sem sanitizer e travam sob ASan (`aovivo`, `cwremover`, `bordafileira`, `atividade_corrida`, `artetamanho`, mais `dvsonda_contentor` e `tpk-responsive`), todos da mesma família SDL2/dyld já anotada em `tests/bordafileira.sh`. **Nenhum deles é erro de memória**: é ASan que não sobe com a SDL2 do brew.

O gate sai em rc=1 por causa dos 24 `FALHA` pré-existentes. Para o piloto isso não é bloqueio novo: é a suíte que já estava vermelha antes da fase 0, e nenhum deles é sanitizer.

## 2. Mapa de candidatos: problema, FFI e custo alternativo

Rust protege o código seguro **dentro** da fronteira. Ponteiros C devem continuar válidos, alinhados e sem aliases conflitantes; `Send/Sync` não validam `void*` de callback. Não transportar `Vec`, `String` ou empréstimos Rust pela ABI.

| Candidato (linhas / includers) | Problema que Rust pode resolver | Forma mínima da FFI | C + sanitizador é mais barato? / decisão |
|---|---|---|---|
| `streams` 3990/20 + `fonteparalela` 106/2 + `fontesmortas` 157/3 | UAR histórico e memória compartilhada; dono Rust do contexto e workers evita empréstimo da pilha | Handle opaco, contexto possuído, cancelamento/join antes do destroy; chamadas HTTP C com buffers e contrato de posse | **Sim hoje**, correções existentes + ASan/TSan. Só portar laço + contexto, não apenas executor; L |
| Núcleo `mkv` 808/6; `mkvass` 3843/6; `capmkv` 212/5 | EBML/NAL/tamanhos de arquivos hostis, somas e conversões 32/64 bits verificadas | Entrada `(u8*, size_t)`, arrays `repr(C)` do chamador + capacidade, status/quantidade/metadados por valor; sem reter ponteiros | Sim para bugs pontuais; fuzz primeiro. **Piloto `mkvcore`**, prevenção ainda **suspeita** |
| `regexjs` 681/3 | Índices, lifetime de programa e texto, profundidade/alocações do motor próprio | Compilar/testar/liberar handle opaco; `(bytes,len)` e resultado limitado; mesmo alocador para liberar | Sim, já há ASan; preservar lookaround/Unicode/limites. Crate `regex` não é substituto semântico (não tem lookaround); S/M |
| `js` 347/57; consumidores `addons` 2866/27, `stream_parse` | JSON não confiável, strings/escapes/faixas sem depender de NUL; offsets verificados | Buffer+len+chave → offset/len ou cópia com cap/status; não devolver referência longeva | **Sim inicialmente** (`js_limites`). Poucas linhas, mas 57 includers e semântica permissiva; M. **Não é QuickJS** |
| `legenda` 967/14; `assrender` 1297/5; `dts_subtitles` 214/2 | Parsing próprio ASS/SRT/VTT e extração de texto; cues com limites de memória/eventos | Bytes+len → cues em arena do chamador ou handle destroy; sem chamar GL/libass dentro do núcleo | Sim, corpus malformado + ASan. Render ASS é libass; PGS é decodificado por FFmpeg, portar wrapper não protege decoder. M; ganho **suspeita** |
| `listas` 812/6; `epg` 1327/7; `xtepg` 271/5; `xtream` 701/9 | M3U/XMLTV/JSON hostis: limites/strings e aritmética no parser próprio | Parser incremental por blocos, eventos copiados para C, cap/erro explícito; download/publicação ficam em C | Sim. #344 já fechado; não portar `guia` 4992/7 por causa dele. M; **suspeita** de outros OOB |
| `celular` 520/3; `proxyts` 772/4; HTTP residual em `rede` 3242/74 | Leitura de request/form/header/playlist com tamanho explícito; exposição LAN/loopback | `(bytes,len)` → request/offsets ou lista limitada; sockets, token/prazo e workers ficam C | **Sim primeiro**: fuzz e revisão de cap/truncamento/framing. Não reescrever curl/JNI/XHR. S/M por parser; benefício **suspeita** |
| `catalogo` 2491/71 + `descoberta` 8578/38 | Heap-OOB histórico, lifetime dos itens publicados e publicação concorrente | Snapshot copiado ou handle com retain/release; nunca pointer C invalidado pela próxima publicação | Sim para falha localizada; API atual de ponteiros torna port amplo caro. L; reavaliar só se houver reincidência medida |
| `atividade` 439/8; `sessao` 834/28; `watchstate_envio` 215/4; `trakt` 2342/26; `sync` 1993/12; `vistoep` 480/21 | Encapsular todo estado sob guarda, sem globais C concorrentes | Comandos/snapshots por valor + identidade/geração; nenhuma leitura externa do estado cru | **Sim**, testes determinísticos + TSan. Rust não evita conta errada/expiração/pedido obsoleto por si só; M/L por subsistema |

**MP4:** `mkv_contentor` apenas identifica caixas ISO; não é demux MP4 completo. Demux real MKV/MP4 no DTS pertence ao FFmpeg e, em outros caminhos, ao player do SO. Mapear um demux Rust novo sem falha atribuída a esse parser seria trabalho especulativo (L).

## 3. Top 5 para nós (retorno potencial, não cinco migrações autorizadas)

| # | Lugar | Por que paga / limite | Próximo passo mais barato |
|---|---|---|---|
| **1** | Posse do failover `streams` + `fonteparalela` | Maior dano comprovado aos usuários + nova correção concorrente. Só ganha se contexto e workers estiverem sob posse Rust; FFI parcial perde a garantia | ASan do fluxo real + TSan/determinismo; port condicionado à reincidência, L |
| **2** | EBML/NAL próprio de `mkv` → `mkvass` | Bytes de CDN, offsets 64 bits em TV 32 bits, núcleo pequeno e `no_std`; sem crash de parser comprovado | **Piloto único `mkvcore`**, M para núcleo / L com alvos |
| **3** | `regexjs` | Motor próprio pequeno, padrão e descrição externos; índices/recursão são fronteira útil. Fix recente é lógico | Fuzz ASan e paridade de lookbehind/Unicode antes de portar, S/M |
| **4** | JSON de addon em `js` | Entrada central de muitos provedores; safe slices podem eliminar classe de leitura inválida. Endurecimento antigo, sem nova queda atribuída | Corpus/fuzz + preservar contrato dos 57 includers, M |
| **5** | Parsing próprio de legendas ASS/SRT/VTT | Arquivos externos complexos e alocação de cues; separar render/decoders de terceiros. PGS não vira seguro com wrapper Rust | Fuzz limites/tempos/texto; depois núcleo próprio, M, ganho **suspeita** |

M3U/XMLTV e HTTP próprio (LAN/loopback/HLS) ficam na fila seguinte: exposição relevante, evidência de corrupção insuficiente para vencer os cinco acima. Se fuzz/queda apontar um deles, sobe no ranking. `catalogo` tem dano histórico real, mas acoplamento/cópias e custo de migração reduzem o retorno atual.
**Não paga agora:** UI/layout, desenho GL, cola SDL, tabelas/i18n; `video_tpk`/cola Starfish, sincronização DSP e políticas de autosync; trocar wrappers de curl, FFmpeg, libass, QuickJS ou libtorrent. Rust pode organizar posse, mas não remove `unsafe` nem corrige lógica/vendor por mudar a linguagem. QuickJS (plugins JS) é separado de `js.c` (JSON).

## 4. Toolchain real por alvo

Tiers/ABIs consultados em [Rust platform support](https://doc.rust-lang.org/rustc/platform-support.html), [ARM Linux](https://doc.rust-lang.org/rustc/platform-support/arm-linux.html) e [Emscripten](https://doc.rust-lang.org/rustc/platform-support/wasm32-unknown-emscripten.html). Fixar versão de rustc no piloto: tiers/pisos mudam. Tier 2 garante builds distribuídos, não execução em nossas TVs.

| Alvo / build atual | Target Rust existente / tier | Condição e encaixe proposto (não implementado) |
|---|---|---|
| webOS ARMv7; `tools/arm.sh`, `tools/Dockerfile`; SDK `arm-webos-linux-gnueabi`; executável `nuvio-proto.arm` | `armv7-unknown-linux-gnueabi`: **2**, sem host tools. `armv7-unknown-linux-gnueabihf`: **2 com host tools**, somente para ABI hardfp | Nosso SDK é EABI softfp; `gnueabi` passa floats em registradores gerais, `gnueabihf` em VFP: não misturar. `std` pré-compilado do target soft atual exige Linux 4.15+/glibc 2.27; F10 registra glibc máx. 2.12 para build webOS. Preferir `no_std`/core; piso real do firmware **não aferido**. Gerar `.a` no host, linkar com GCC do SDK após objetos, como libs P2P existentes |
| Tizen 4/5 ARM `.tpk`; `tools/tpk.sh`, `tools/tpk/Dockerfile`; `libnuvio-tpk40.so` | Mesmo `armv7-unknown-linux-gnueabi`, **2** | C usa `-march=armv7-a -mfloat-abi=softfp -mfpu=neon`. Loader ELF próprio não monta TLS; **não usar std no piloto**. `no_std`, abort, PIC e `.a` na segunda linha `gcc -shared`; passar guards PT_TLS/relocs/`__tls_get_addr`/DT_HASH |
| Tizen 6+ ARM `.tpk`; mesma imagem buster armel/glibc 2.28; `libnuvio.so` | Mesmo target soft, **2** | TLS disponível no loader glibc, mas isso não prova compatibilidade de `std` com kernel/glibc de todas as TVs. Piloto `no_std`, PIC, mesmo `.a` nas duas `.so`; conservar ABI/exportações `nv_tpk_*` e autoatualização |
| Samsung `.wgt` / `tools/tizen.sh` (`emcc -pthread`, memória fixa) | `wasm32-unknown-emscripten`, **2** | `.a` Rust no link emcc, compatibilidade de LLVM/ABI/atomics precisa de prova mesmo sem criar fios Rust. Docs recomendam reconstruir std com nightly `-Zbuild-std` quando ABI local diverge; não prometer que `no_std` elimina todo risco. C permanece se falhar |
| Android arm64/armv7; `tools/android.sh`, Gradle NDK **27.2.12479018**, CMake | `aarch64-linux-android`, `armv7-linux-androideabi`: **2** | `.a` por ABI, linker clang do NDK/API do app; importar no `android/app/src/main/cpp/CMakeLists.txt` e linkar em `main` (`libmain.so`). `std` suportado por bionic, piloto core igual às TVs. Começar com cargo+linker configurado, sem exigir Corrosion/cargo-ndk |
| macOS host; `tools/mac.sh`, `tests/*.sh` com `cc` | `aarch64-apple-darwin`: **1 com host tools**; `x86_64-apple-darwin`: **2 com host tools** (tabela atual) | `.a` para arquitetura do host nos mesmos testes C; executar ambas implementações e corpus. Mac 64 bits não substitui testes de truncamento/layout ARM32 |

| Decisão transversal | Contrato mínimo |
|---|---|
| Link estático | `crate-type=staticlib` incorpora núcleo Rust ao binário C, **não** torna glibc/vendor estáticos. Não trocar para musl para contornar glibc dentro de processo glibc; manter allocator/libc existentes |
| `no_std` vs `std` | Piloto sem heap/TLS/fios: só `core`, panic handler mínimo e `panic=abort`. Se precisar heap, `alloc` exige allocator explícito. `no_std` pode chamar pthread via FFI em projeto futuro: falta de `std` **não obriga C permanente**, mas wrapper seguro e prova por alvo custam mais |
| Flags e exportações | `-DNV_RUST_MKVCORE` por alvo (ligada em Android e Mac, ver seção 7); proposta `NUVIO_RUST=1` nos scripts. `repr(C)`/inteiros de largura fixa, validar `long` 32/64 bits. `--exclude-libs,ALL` pode ocultar símbolos Rust internos **sem impedir chamadas dos objetos C**; só ABI host precisa exportação dinâmica |
| Otimização/tamanho | `opt-level=z`, abort, dead stripping/GC onde possível; LTO Rust interno (não presumir LTO C/Rust compatível). **Suspeita**, não medido: núcleo core custa dezenas de KiB; std pode custar centenas de KiB ou mais. Comparar binário final stripped e pacote, não tamanho do `.a`; sem promessa de delta |
| Guards | Sem novo NEEDED; símbolos GLIBC/GLIBCXX não sobem piso; sem TLS no 4/5; ABI float/relocs corretas; exports `nv_tpk_*` idênticos. `tpk.sh` já imprime GLIBC/NEEDED e barra TLS; falta tornar piso/tamanho gates explícitos |
| Ferramentas | Cargo ausente conforme ambiente informado; nenhum install/build Rust feito. Pin de rustc/targets, hash da `.a` no cache dos objetos e orçamento de tamanho entram **só se piloto aprovado**; scripts atuais não têm etapa Rust |

## 5. Plano concreto, com ponto de parada

Estimativas por pessoa: S = até 2 dias, M = 3–10 dias, L = 2–4 semanas ou mais; **suspeita**, não compromisso de prazo.

| Fase | Entrega mínima / teste de aceite | Esforço | Decisão do dono |
|---|---|---|---|
| **0 — completar sanitizadores** | Inventariar flags reais dos 183 scripts e dos sempre instrumentados; runner obrigatório ASan/UBSan e job TSan separado para os 28 (nunca ambos juntos). Runner reaproveita testes existentes, falha em erro e distingue skip/dependência ausente; não criar framework/helper genérico | M | Escolher runner Mac ou Linux/CI e bloquear integração quando houver falha reproduzível |
| 0 — prova de fluxo/entrada | Repetir roteiro ASan de fontes, guia, publicação/troca de perfil e wake; ampliar corpus + harness de bytes do núcleo EBML, JSON, regex e legendas. Depois HTTP/M3U/XMLTV. Começar fuzz curto (proposta: 1 h por parser selecionado), guardar sementes/crashes; TSan não prova lógica de geração | M | Orçamento de máquina/tempo e prioridade dos parsers; horas limpas não provam segurança |
| **1 — único piloto `mkvcore`** | `rust/mkvcore`, staticlib/no_std/sem dependências, **núcleo EBML/NAL de `mkv.c`**. Escopo: `mkv_contentor`, `mkv_quadro_tem_rpu`, `mkv_faixas_do_trecho` e helpers necessários; rede/diagnóstico ficam C. Aproximadamente **400–500 linhas C** de extração (**suspeita**, inclui capítulos/SeekHead usados pelo caminho), não `mkvass` inteiro | M núcleo; L com alvos/soak | Autorizar experimento/toolchain e teto de tamanho por alvo, reconhecendo que não há redução de quedas demonstrada |
| 1 — fronteira/fallback | Wrapper C mantém assinaturas; núcleo retorna contagens/status/metadados por chamada. `mkv_faixas_do_trecho` hoje toca globais e diagnóstico via `acharTracks`: **não é puro/reentrante só por não fazer HTTP**. Remover esse estado da parte Rust; wrapper preserva comportamento. `-DNV_RUST_MKVCORE` off; sem flag, C atual; nenhum panic atravessa FFI | Incluído | Aceitar fallback C por alvo durante experimento; desligar/remover se benefício não superar manutenção |
| 1 — gate | Testes C existentes `mkv_contentor`, `mkv_dvrpu`, `mkv_caps`, `mkv_legendas` e `mkvass_limites`; nova paridade C/Rust por **campos** (não padding de struct), entradas malformadas/truncadas, comprimentos excessivos, cap=0, NAL inválido, fronteiras >2 GiB/ARM32. Fuzz sem OOB/panic; diferenças de rejeição hostil triadas, não perpetuar UB para “paridade” | Incluído | Aprovar relatório de ganho/tamanho/risco antes de ativar em produto |
| 1 — gate de plataforma | Mac + Android duas ABIs primeiro; webOS softfp, `.tpk` 4/5 e 6+, `.wgt` depois. Guards ELF/ABI/tamanho e reprodução/abertura/fluidez em TV; skip não conta. Um alvo falhou: C nele; nenhum lançamento depende do piloto | Incluído | Dispositivos/tempo de teste e limite de duração do experimento |
| **2 — só após resultado** | Primeiro regex **ou** JSON **ou** legendas conforme fuzz/queda encontrada; não três ports paralelos. Depois M3U/XMLTV/HTTP se evidência justificar. Cada núcleo com cap, flag, fallback, paridade e gate de plataforma | S/M por núcleo | Selecionar um problema comprovado e aprovar custo; parar se C+teste resolver mais barato |
| 2 — concorrência condicional | Se fontes reincidirem, estudar port de posse/contexto + workers, depois catálogo/snapshots. Não vender `Mutex` como solução de conta/geração obsoleta; evitar `unsafe impl Send` que só silencia a prova | L por subsistema | Autorizar redesenho da fronteira e manutenção por alvo; não iniciar pela simples conclusão do piloto |

## 6. Decisões e limites

| Para o dono | Recomendação |
|---|---|
| Sanitizadores obrigatórios antes da migração? Onde executar? | Sim; inventariar modos/ambiente primeiro e usar runner existente disponível, sem presumir que CI externa existe |
| Aprovar piloto único? | `mkvcore`, flag off, orçamento de tamanho e parada após prova de toolchain/entrada hostil; instalar ferramentas só nessa fase aprovada |
| Qual port vem depois? | Somente o que fuzz/queda demonstrar; prioridade histórica de fontes permanece, mas custo é L. Sem autorização para reescrita ampla |

Verificação deste estudo: `bash tests/js_limites.sh` e `bash tests/selospacote.sh` (ASan/UBSan sempre ligados), `SANITIZE=1 bash tests/guiabusca_faixa.sh` e `SANITIZE=1 bash tests/mkvass_limites.sh`: todos rc=0. O último emite warning já existente de inicializador `prebusca`; não executamos TSan, fuzz ou paridade Rust.

Não consultei novo banco de quedas nem executei firmware/toolchains; números de usuários vêm do estudo antigo. Linhas/includers e suporte declarado a sanitizadores são medidas estáticas, não cobertura. Pisos de TV, bytes/tempo de build, ROI dos parsers e portabilidade efetiva continuam **suspeita** até os gates. Este documento não autoriza implementação, publicação ou instalação.

## 7. Relatório do piloto `mkvcore` (fase 1, flag `-DNV_RUST_MKVCORE`)

Feito: `rust/mkvcore` (no_std, staticlib, sem dependências, `panic=abort`, handler → `abort()`, sem estado global, profundidade máx. 8, aritmética checada); `src/mkvcore.h` (structs `#[repr(C)]` campo a campo); `src/mkv.c` com ganchos só sob a flag (assinaturas de `mkv_contentor`, `mkv_quadro_tem_rpu`, `mkv_faixas_do_trecho` e `lerCapitulos` mantidas; rede/diagnóstico seguem C). Ganchos de build: `NUVIO_RUST_MKVCORE` (padrão 1, `=0` desliga) em `tools/mac.sh` e `tools/android.sh`, `-DNUVIO_RUST_MKVCORE=<pasta>` no CMake. **Flag ligada nos builds de produto em 14/10/2026 (dono aprovou)**, por plataforma:

| Plataforma | Estado | Observação |
|---|---|---|
| Android arm64-v8a / armeabi-v7a (`tools/android.sh`) | **ligada** (padrão) | `NUVIO_RUST_MKVCORE=0` volta ao C |
| Mac (`tools/mac.sh`) | **ligada** (padrão) | idem |
| webOS ARM (`tools/arm.sh`) | **ON — 2026-10-19** | Offline `armv7-unknown-linux-gnueabi` links with the SDK softfp ABI; all modes default to Rust, `NUVIO_RUST_MKVCORE=0` keeps C. Required GLIBC remains 2.12; binary 57,987,068 → 58,191,988 bytes. See `docs/issues/rust-mkvcore-webos-abi.md`. |
| Samsung `.tpk` 4/5 e 6+ (`tools/tpk.sh`) | **desligada** | mesmo alvo: `rustup target add armv7-unknown-linux-gnueabi` |
| Samsung `.wgt` WASM (`tools/tizen.sh`) | **desligada** | falta `rustup target add wasm32-unknown-emscripten` |

Alvos instalados hoje: `aarch64-apple-darwin`, `aarch64-linux-android`, `armv7-linux-androideabi`. Nada foi instalado; as três plataformas desligadas seguem no C até o alvo existir e passar o gate de plataforma da seção 5.

**Tamanho** (strip; release `opt-level=z`, LTO):

| Alvo | Medida | Flag off | Flag on | Delta |
|---|---|---|---|---|
| Android arm64-v8a | `libmain.so` inteiro (CMake `main`) | 22 118 216 B | 22 111 992 B | −6 224 B |
| Android armeabi-v7a | `libmain.so` inteiro (CMake `main`) | 16 815 012 B | 16 814 180 B | −832 B |
| Android arm64 / armv7 | só `mkv.c` + núcleo, link isolado | 25 000 / 22 568 B | 18 728 / 15 716 B | −6 272 / −6 852 B |
| Mac (binário) | executável strip | — | — | +432 B |

Delta pequeno: o núcleo Rust troca o C equivalente; `--gc-sections` remove o resto. Sem teto de tamanho violado. Mac: só binário de teste/`mac.sh`, não é alvo de produto.

**Testes** (todos rc=0, flag off **e** on): `mkv_contentor`, `mkv_dvrpu`, `mkv_caps`, `mkv_legendas`, `mkvass_limites`. Novo `tests/mkvcore_paridade.sh`: C×Rust por campos + fuzz limitado (LCG semente fixa, páginas PROT_NONE nas bordas): 25 512 casos, tudo igual. Teste validado por mutação (injeção de diferença é detectada). Android: link completo de `libmain.so` (CMake) com flag on e off, arm64 e armv7, rc=0; símbolos `nv_mkvcore_*` presentes só com flag on.

**Achados no caminho:** (a) `0xFFu8 >> w` com `w=8` (vint de tamanho 8 bytes) estoura em release → Tracks não lidos; corrigido com `u32`. (b) ARM32: `compiler_builtins` referencia `rust_eh_personality`; sem std o link falhava; definido só em `target_arch="arm"`, nunca chamado. (c) 14/10/2026: o link debug `aarch64` do APK (flag ligada por padrão) falhou com o mesmo símbolo indefinido; agora definido em todo alvo (`not(test)`).

**Prova de 14/10/2026 (flag ligada):** `tools/android.sh` rc=0 (APK debug+release, arm64-v8a e armeabi-v7a), `nm` de `libmain.so` mostra `nv_mkvcore_achar_tracks`, `nv_mkvcore_cap_cabecalho` etc. nas duas ABIs; `tools/mac.sh` (compilar) rc=0 com 6 `T _nv_mkvcore_*`; flag desligada (`=0`) sem símbolos. Testes mkv rc=0 com `NV_RUST_MKVCORE=0` e `=1`.

**Diferenças intencionais vs C:** profundidade máx. 8 na recursão; i64 em vez de `long` 32-bit no ARM32; alvo comparado como u64; sem overflow com sinal (UB no C). Rejeição de entrada hostil pode divergir do C nesses casos; não perpetuado para “paridade”.

**Riscos / não coberto:**
- ARM32 só **linkado**, não executado aqui; sem teste em TV nem reprodução/fluidez.
- webOS softfp, Tizen `.tpk`/`.wgt` não tocados.
- `clippy` não instalado: lints `deny` no código não verificados.
- `lerUint` não exportado: teste `credfonte` não cobre o núcleo Rust.
- Caminho de capítulos via HTTP coberto só pelo teste HTTP de `mkv_caps`.
- Medida de tamanho Android é de build com a flag apenas neste worktree; sem soak, sem fuzz longo (só o limitado do teste).
- Nenhuma redução de quedas demonstrada (já dito na fase 1); ativar em produto depende de aprovação do dono.
