# Rust no Nuvio: estudo e piloto

Estado: ESTUDO, sem codigo de produto. Base: `agente/rust-plano` em 8ed4517c (v2.0.3 publicada).
Pergunta do dono: "vamos pensar em implementar Rust nas partes que mais dao problema e que isso resolveria".
Regra deste documento: o que nao foi provado por commit, teste ou banco de quedas esta marcado **suspeita**.
Nao ha `cargo`/`rustup` neste Mac; nada foi instalado nem compilado. Toda a parte de toolchain e leitura de scripts.

## 0. Resumo executivo

1. Os bugs de seguranca de memoria que mais derrubaram gente nao estao nos parsers de dados externos; estao em **vida de objeto e corrida entre fios** (streams/fonteparalela, catalogo, descoberta, trakt, sync, vistoep). Parser de EBML/ASS/JSON tem pouca evidencia historica de memoria (suspeita de que ainda haja buracos, mas nao ha commit que prove).
2. Rust so protege **dentro** da fronteira Rust. O bug que derrubou 28% dos usuarios Android (696afa74) era um `&c` da pilha passado como `void *u` por codigo C (streams.c); reescrever so `fonteparalela.c` em Rust nao o teria impedido, porque a ponte FFI apaga o tipo. So reescrevendo tambem `streams.c` (3583 linhas, 18 includers) o compilador veria o erro.
3. Os sanitizadores (ASan/UBSan/TSan) **ja acharam** todos os bugs classificados abaixo que foram achados antes do usuario; o que falta e rodar de forma obrigatoria (nao ha CI; so 152 dos 548 `tests/*.sh` aceitam `SANITIZE=1`) e cobrir o app inteiro, nao so unidades.
4. Recomendacao: **sanitizadores primeiro** (fase 0, custo baixo, cobre o mesmo conjunto de bugs); piloto Rust opcional e depois, num modulo puro e sem fios (nucleo EBML do `mkv.c`), com objetivo principal de **provar a toolchain** nos 5 alvos, nao de reduzir quedas (a evidencia de quedas nesse modulo e fraca).

## 1. Ranking dos modulos

### 1.1 Metodo

- Janela: `v1.7.0..8ed4517c`, 1511 commits; 861 tocam `src/`; 115 deles com mensagem `fix...`.
- Para cada `src/<mod>.c`: `git log v1.7.0..HEAD --format=%s -- src/<mod>.c` filtrado por pilha, use-after, ASan, estouro/overflow, truncad, buffer, corrida/race, trava/mutex, double free, vazamento, SIGSEGV, queda; e leitura do corpo dos commits que o filtro pegou.
- Classificacao **manual** da amostra que o filtro achou (nao e censo: commits que corrigem bug sem usar essas palavras nao entram). Linhas: `wc -l`. Acoplamento: arquivos `.c`/`.h` de `src/` com `#include "<mod>.h"`.
- Aviso de direcao: quedas em producao vem do banco de 09/10/2026 (dado do dono, citado nos commits/issue; nao reproduzido aqui).

### 1.2 Evidencia de producao usada

| Evidencia | Onde | Quantos | Categoria |
|---|---|---|---|
| Conferencia de fontes em paralelo: `verificarOuParar` (streams.c ~1098) lendo/escrevendo a `Conferencia` da pilha de `stream_primeira_boa` depois que a pilha foi desfeita (fonteparalela.c ~37). Corrigido em 696afa74 | streams.c, fonteparalela.c | 128 de 456 usuarios Android 2.0.2 (28%), 497 tombstones, +~30 usuarios webOS | use-after-return (UAR) |
| #344 leitura fora do limite: `buscaFazer` (guia.c, `EpgProg ps[24]`) usa `n` devolvido por `epg_faixa`/`xtepg_faixa` que pode ser maior que `cap`. Teste que falha com ASan `stack-buffer-overflow`: 9d78c75a. Correcao so em 2.0.3.1 | guia.c, epg.c, xtepg.c | nao tenho o numero de pessoas | OOB read |
| #361 URL de cartaz comprida cortada por buffer fixo (2220b019); familia: 4f3a2584 (URL de addon cortada em 599), 197468ec (2 a 16 KB ignorada), 4fbf4cb1 (corte silencioso no teto 32 para 64) | descoberta.c, addons.c, sync.c | sem numero | truncamento de buffer fixo |
| Parsers EBML/Matroska: #384 (faixa ASS com mais de 8000 blocos perdia tudo depois da abertura e fechava "completo"; 9ee093ac), 2f047898 (#330 pool baixa Range de geracao velha), cca4147d (capitulos do video anterior publicam no seguinte), b35bcbcf (offsets 64 bits) | mkvass.c, capmkv.c, mkv.c | sem numero | **logica/limite**, nao memoria |
| Corridas | trakt `play[]`/`nPlay` (a29e123d, teste TSan 0fa4dac8); resumo de sync (1c60454d, 20ace9d4); descoberta (0d33a0aa, 70acffaf, 2b4234eb); vistoep (25988de7); colecoes ponteiro cru (57f7cfaf); sessao/token (b324c7d0); tex_cache (e332b1f8); linguas entre fios de addon (74b8d51a) | varios | data race |
| Corrupcao de heap | `cat_acrescentar*` calculava o tamanho fora de `pubTrava` e copiava dentro: `memcpy` passa do bloco, "3 quedas seguidas num .tpk Tizen 9" (6c504c0b); escritor do cache RLE do catalogo, `heap-buffer-overflow` em `rleCodificar` na 2.0.0 (33fd1fec, 52dc67ef) | catalogo.c | heap OOB write |

Consulta a `docs/issues/mapa.json` (356 itens): 16 citam queda/crash/overflow/truncamento no titulo ou nota, entre eles #65, #68, #148, #211, #224, #271, #323, #344, #357 (Zidoo/Ugoos), #384. O mapa quase nao traz **causa** nem **contagem** de pessoas; nao da para ranquear por ele, so confirma que "queda" e tema recorrente. Nenhum item do mapa aponta parser de EBML/ASS/XMLTV como causa de queda (suspeita de ausencia de dado, nao de ausencia de bug).

### 1.3 Tabela por modulo

Categorias: UAF/UAR, OOB (fora do limite, leitura/escrita), TRUNC (buffer fixo), RACE, DF/LEAK, OUTRO. Contagem = commits de correcao achados na amostra acima, nao total.

| Modulo | Linhas | Includers | Dados externos nao confiaveis? | UAF/UAR | OOB | TRUNC | RACE | OUTRO relevante | Pessoas |
|---|---|---|---|---|---|---|---|---|---|
| streams.c + fonteparalela.c | 3583 + 106 | 18 + 2 | sim (JSON de addon, entra por stream_parse/js) | 1 | 0 | 0 | 0 | | **128/456 Android + ~30 webOS** |
| catalogo.c | 2397 | **69** | indireto (recebe da descoberta/addon) | 0 | 3 (memcpy, RLE x2) | 1 (4f3a2584) | 4 (epTrava, item copiado sob trava x2, revisao) | | "3 quedas seguidas" num .tpk; 2.0.0 com corrupcao |
| descoberta.c | 7906 | 38 | sim (catalogos de addon, TMDB, cartaz) | 0 | 0 | 2 (#361, 599) | 3 | | sem numero |
| addons.c | 2654 | 26 | sim (manifesto/JSON/URL de addon) | 0 | 0 | 3 | 1 | 722a165d: copia grande saia da pilha | sem numero |
| trakt.c | 2301 | 26 | sim (HTTP) | 0 | 0 | 0 | 2 | | sem numero |
| sync.c | 2346 | 12 | sim (conta) | 0 | 0 | 2 | 3 | | sem numero |
| vistoep.c | 467 | 21 | nao | 0 | 0 | 0 | 1 | | sem numero |
| guia.c + epg.c + xtepg.c | 4476 + 1316 + 261 | 7 + 7 + 5 | **sim (XMLTV, Xtream, M3U)** | 0 | 1 (#344, **ainda aberto em 8ed4517c**: `ps[24]` com `n` sem `min`) | 0 | 0 | | sem numero |
| mkv.c + capmkv.c + mkvass.c | 808 + 214 + 3726 | 6 + 5 + 6 | **sim (EBML via HTTP Range)** | 0 | 0 | 1 (323b6b2b, URL longa) | 1 (cca4147d) | #384 limite 8000, #330 geracao velha | sem numero |
| assrender.c / legenda.c | 1297 / 940 | 5 / 12 | sim (ASS/SRT) | 0 | 0 | 0 | marcas de corrida no libass (712557ca) | | sem numero |
| js.c | 347 | 54 | sim (JSON) | 0 | 0 | 0 | 0 | 73046936: endurece parse limitado e buffers de resposta | sem numero |
| player.c | 5074 | 20 | nao | 0 | 0 | 0 | 0 | overflow de unsigned no prazo (9df9c50a, b7cbea01): **aritmetica**, nao memoria | sem numero |

Observacoes honestas sobre o ranking:
- Por **gravidade e pessoas afetadas**, o topo e streams/fonteparalela, depois catalogo (heap corruption), depois o conjunto de corridas.
- Por **viabilidade de portar**, o topo e o inverso: o que mais machucou (catalogo, descoberta, streams) tem acoplamento alto (69, 38, 18 includers), usa fios, travas globais e callbacks; o que e puro e portavel (nucleo EBML, `epg_faixa`, parse de URL) quase nao tem historico de queda.
- 6c504c0b, 33fd1fec e 696afa74 nasceram da propria migracao de codigo para fios (2.0.0 a 2.0.2): o risco e **concorrencia introduzida pelo desenho**, nao ausencia de checagem de indice.
- Nao ha evidencia de `sprintf/strcpy/strcat` como causa: so 5 ocorrencias nos modulos acima (mkvass 3, epg 2). O C do projeto ja usa `snprintf` quase em todo lugar; o problema do `buscaFazer` (#344) e `n` de retorno confiado, nao copia sem limite.

## 2. Os 3 a 5 melhores candidatos: fronteira C e o que Rust garantiria

### 2.1 fonteparalela + o laco de stream_primeira_boa (UAR 28%)

- Cruza a fronteira: `int fila[]` emprestado; `FonteVerificar`/`FonteFalhou` (callbacks C que rodam em varios fios); `void *u` (dono: o chamador); `soltarU(void*)` chamado no ultimo fio.
- Quem aloca/libera: `Par` e alocado e liberado dentro do modulo (contagem de referencia manual, `refs = 1 + k`); `u` e do chamador (agora no heap, `ConfDona`).
- Em Rust: `Arc<Par>` mais `Arc<Conferencia>` clonado para cada fio; `thread::spawn` exige `'static + Send`, entao `&c` da pilha **nao compila**. Condvar com `Mutex<[Estado;4]>`.
- Limite: o erro estava no chamador C. Para o compilador ver, `stream_primeira_boa` e a `Conferencia` (o laco de verificacao de streams.c, ~300 a 500 linhas, suspeita do tamanho) precisam ser Rust. O callback `FonteVerificar` continua C (faz HTTP via rede.c) e entra como `extern "C" fn(i32, *mut c_void) -> i32` dentro de um wrapper marcado `unsafe impl Send`; ou seja, a prova de seguranca para nesse wrapper.
- Fica em C: rede.c, o parse do stream, a UI.
- Veredito: ganho real so se o pedaco for grande; **nao** e bom piloto (usa pthread/TLS de `std`, fio, callback).

### 2.2 catalogo.c (heap corruption, corridas)

- Cruza: `Item *` publicado por `cat_publicar`/`cat_acrescentar*`, lido pela UI sem copia (ponteiro emprestado por tempo indefinido), e por 69 arquivos. Dono e alocador: o proprio catalogo, sob `pubTrava`/`epTrava`/`listaTrava`.
- Em Rust: `Mutex<Vec<Item>>`; `Vec::extend` recalcula tamanho sob a mesma guarda (6c504c0b impossivel); `&[Item]` so existe dentro do guard (70acffaf/2b4234eb viram erro de compilacao, tem de clonar). Cache em disco: `Vec<u8>` crescente no lugar do RLE em dois passos (33fd1fec impossivel).
- Problema: a API C entrega `const Item *` para 69 includers que guardam o ponteiro. A ponte teria de copiar na saida (custo de CPU e de memoria na TV; **suspeita** de impacto em fps na home) ou manter o contrato "valido ate a proxima publicacao" que e exatamente a fonte das corridas.
- Veredito: maior ganho de seguranca, maior custo; fase 3, nao piloto.

### 2.3 Parse de EPG (epg.c, xtepg.c, guia.c)

- Cruza: `EpgProg *out, int cap` do chamador (buffer emprestado, `cap` dado), retorno `int n`. Quem aloca: chamador (pilha, `ps[24]`).
- Bug: retorno `n > cap` (contagem total, nao copiada) e o chamador indexa ate `n`. Em Rust: `fn faixa(...) -> &[EpgProg]`/`Vec` carrega o tamanho no tipo; a ponte C de `epg_faixa` escreve `min(n,cap)` e devolve esse valor, o contrato muda. Mas **isto se corrige em C com uma linha** (`if (n > 24) n = 24;`) e o teste 9d78c75a ja o cobre; o ganho de Rust aqui e baixo.
- Dados nao confiaveis: XMLTV/Xtream, entrada grande e hostil e plausivel (suspeita: ha mais OOB nao achados em epg.c, 1316 linhas, 33 `memcpy/snprintf/strtol`).
- Veredito: bom para **hardening em C** (sec. 5), segundo na ordem para Rust.

### 2.4 Nucleo EBML/Matroska (mkv.c, parte pura de capmkv.c/mkvass.c)

- Cruza: `const unsigned char *buf, long n` emprestado (entrada), `MkvFaixa *saida, int max` (saida preenchida pelo modulo), `mkv_quadro_tem_rpu(p,n,...)`, `mkv_contentor(p,n)`, `mkv_faixas_do_trecho(buf,n,saida,max,...)`. Sem fios, sem alocacao, sem callbacks nas funcoes puras ("puro" ja esta escrito no `mkv.h`). O resto de `mkv.c` e `mkvass.c` faz HTTP Range e fica em C.
- Em Rust: `&[u8]` e `&mut [MkvFaixa]`; leitura de vint/ID/tamanho por `get(..)?` (sem indice solto); `u64 -> usize` com `try_from`; sem `std` (so `core`), `panic=abort`.
- Dados nao confiaveis: sim, e o melhor argumento (MKV de CDN de debrid, arquivo grande de 2 GiB+ em 32 bits: b35bcbcf).
- Evidencia de queda nesse modulo: **nenhuma achada** no historico; os bugs de #384/#330/cca4147d sao de logica/estado, que Rust nao previne. Valor aqui e preventivo (suspeita).
- Veredito: **melhor piloto de toolchain**; fraco como piloto de reducao de quedas.

### 2.5 Estado compartilhado entre fios: trakt `play[]`, sync resumo, vistoep, colecoes

- Cruza: globais C acessadas por varios fios e pela UI (`play[]`, `nPlay`, `resumo`, `mapa de vistos`).
- Em Rust: `static X: Mutex<Estado>` torna o acesso sem trava um erro de compilacao (todos os 7 a 8 commits RACE dessa familia).
- Mas: cada um e pequeno (vistoep.c 467 linhas) e **ja corrigido com trava + teste TSan**. O ganho e impedir reincidencia; o custo e portar 21+26+12 includers de API.
- Veredito: bom segundo passo; so depois do piloto e de tipos de ponte estaveis.

## 3. Toolchain por alvo (leitura de scripts; nada compilado)

| Alvo | Como o C e construido hoje | Target Rust provavel | Pontos de atencao |
|---|---|---|---|
| **webOS armv7** | `tools/arm.sh` + `tools/Dockerfile`: buildroot openlgtv `arm-webos-linux-gnueabi` (gcc 14.2); doc F10 diz "ARMv7 softfp, glibc max 2.12" para o que sai dali; executavel unico `nuvio-proto.arm`, `-Wl,--exclude-libs,ALL` | `armv7-unknown-linux-gnueabi` (softfp). **Nao** `...gnueabihf`: misturar ABI de float falha no link ("uses VFP register arguments") | glibc do sysroot 2.12 e **menor** que o minimo que o `std` do Rust assume (suspeita, ~2.17): usar `#![no_std]` + `core`/`alloc` e so as funcoes do C que ja existem (`malloc`, `memcpy`). webOS 3.x (glibc mais antigo; versao exata nao verificada aqui) reforca o `no_std`. Linkar `libcompiler_builtins` junto de `libgcc` do buildroot pode duplicar simbolos (suspeita). |
| **Tizen .tpk 6+** | `tools/tpk.sh` + `tools/tpk/Dockerfile`: Debian buster armel (glibc 2.28), `-march=armv7-a -mfloat-abi=softfp -mfpu=neon`, `libnuvio.so` com `-fvisibility=hidden`, `--exclude-libs,ALL`, `--no-undefined`, carregada por dlopen pelo host .NET | `armv7-unknown-linux-gnueabi` + `-C target-feature=+neon` (confirmar `-mfpu` equivalente) | Compilar Rust **fora** do container (o container e `linux/arm/v5` emulado; nao rodar rustc la), gerar `.a` e linkar dentro. TLS funciona (glibc carrega). Exports: toda funcao `extern "C"` com `#[no_mangle]` precisa de `visibility default` ou fica escondida; o guard `DllImport` do tpk.sh so cobre `nv_*` do host. |
| **Tizen .tpk 4/5** | mesma imagem; segunda `.so` `libnuvio-tpk40.so` (`-DNV_TPK40`, sem `_Thread_local`, `--hash-style=both`). Carregador de ELF proprio em `Program40.cs` **nao monta TLS** e resolve por `DT_HASH`. `tools/release-samsung.sh` roda `tests/tpk40_tls.sh`, que falha com PT_TLS, `R_ARM_TLS_*`, `__tls_get_addr` ou sem `DT_HASH` (#137, #180) | mesmo target | **O `std` do Rust usa TLS** (contador de panico, `thread::current`, `thread_local!`): linkar `std` na `.so` do 4/5 faz o guard falhar (e, sem o guard, derruba o processo no primeiro acesso, como o crash de login de #137). Implicacao: Rust aqui **so como `#![no_std]`**, `panic = "abort"`, `-C panic=abort`, nada de `thread_local!`, nada de `std::thread`/`std::sync` (use `pthread` do C via `extern`). Vale checar `compiler_builtins` e simbolos de TLS do `core` (suspeita: `core` nao emite TLS; a confirmar com `readelf -lW`). O guard existente funciona como rede de seguranca. |
| **.wgt Samsung (wasm)** | `tools/tizen.sh`: `emcc` do projeto, `-pthread -sPTHREAD_POOL_SIZE=N -sINITIAL_MEMORY=268435456 -sALLOW_MEMORY_GROWTH=0 -sABORTING_MALLOC=1`; ~40 `pthread_create` | `wasm32-unknown-emscripten` | Rust+emscripten **com pthreads** exige `-C target-feature=+atomics,+bulk-memory,+mutable-globals` e, na pratica, `-Zbuild-std` (nightly) para `std` com atomics; o `emcc` do projeto e o `emcc` esperado pelo `rustc` precisam ser compativeis (versao de LLVM e ABI de `emscripten`). Em `no_std` sem fios no codigo Rust (piloto 4) o problema some: compila-se `core` normal para `wasm32-unknown-emscripten` e linka-se o `.a` com `-sUSE_PTHREADS`. **Bloqueio provavel** para qualquer modulo Rust que crie fio. |
| **Android armv7 + arm64** | `tools/android.sh` + `android/app/build.gradle.kts` (CMake, `ndk/27.2.12479018`, `c++_static`), `libmain.so` nas duas ABIs; `release-android.sh` exige libs em ambas | `aarch64-linux-android` e `armv7-linux-androideabi` | Tem Rust-sobre-NDK maduro (`cargo-ndk` ou `corrosion` no CMake). `std` com TLS funciona no bionic. Linker clang do NDK. O `.so` final nao troca de nome; o guard "libs nas duas ABIs" continua valendo. Hoje esse e o **alvo de menor risco** e o de maior prejuizo (28%). |

Impactos transversais:
- **Tamanho**: `no_std` + `panic=abort` + `opt-level=z` + LTO costuma somar dezenas de KB por modulo pequeno (suspeita, nao medida aqui; ha que medir e fixar orcamento). Com `std`, centenas de KB e risco de TLS. Nao ha binarios no worktree para comparar baseline; o orcamento do piloto sera "delta <= X% do `libnuvio.so` do mesmo commit", X definido depois de medir o baseline no CI do dono.
- **Tempo de build**: nova etapa `cargo build --release --target ...` por alvo (5 a 6 alvos), cacheavel; `rustc` pode ser lento em Mac (Rosetta nao e usada; e nativo arm64). Suspeita: +1 a 3 min por alvo na primeira vez.
- **Guards de release**: reaproveitar `tests/tpk40_tls.sh` (ja pega TLS), acrescentar: `readelf -d` sem NEEDED novo (`libgcc_s`, `libunwind`), `nm -D` sem simbolo Rust (`_ZN`/`_R`) exportado alem da lista `nv_*`/`rs_*`, delta de tamanho, e checagem de `GLIBC_` maxima (o `tpk.sh` ja imprime `glibc minima`).
- **Auto-atualizacao do .tpk (troca da `.so`)**: a `.so` continua uma unica biblioteca (o `.a` Rust vai linkado dentro dela); a troca e a mesma. Nao muda formato de pacote nem ABI do host. Risco: se o piloto trouxer um NEEDED novo ou exigir um glibc maior que o da TV, a `.so` nova nao carrega e a TV fica sem app; por isso o piloto tem **flag de build e fallback C** (sec. 4).

## 4. Piloto: nucleo EBML puro do mkv.c ("mkvcore")

Escolhi **exatamente um** modulo: as funcoes puras de `src/mkv.c` (`mkv_contentor`, `mkv_quadro_tem_rpu`, `mkv_faixas_do_trecho`) que recebem `(const unsigned char*, long)` e preenchem `MkvFaixa[]`. Justificativa: sem fios, sem alocacao, sem callback, sem TLS, `no_std` serve para os 5 alvos, dados nao confiaveis, ja existe suite (`tests/mkv_contentor.c`, `tests/capmkv.sh`, `tests/mkvass.c`...) que chama a ABI C.

Isto e uma **escolha de toolchain, nao de retorno em quedas**. Se o dono prefere atacar a causa dos 28%, o piloto certo seria 2.1 acima, mas ele exige reescrever streams.c junto e usar fio/TLS (bloqueios do sec. 3).

### 4.1 Como

- Diretorio novo `rust/mkvcore/` (crate `staticlib`, `#![no_std]`, `panic = "abort"`, `opt-level = "z"`, `lto = true`, sem dependencias).
- Ponte: `mkvcore.h` com as mesmas assinaturas C de `mkv.h`; as funcoes viram `extern "C"` com `#[no_mangle]`.
- Flag: `-DNV_RUST_MKVCORE` (e `NUVIO_RUST=1` nos scripts). Sem flag, compila o C de hoje. O `mkv.c` fica com as duas implementacoes lado a lado ate o fim do piloto (`#ifdef NV_RUST_MKVCORE` seleciona qual simbolo exporta).
- **Mesmos testes**: `tests/mkv_contentor.c` e demais continuam chamando a ABI C; nova variante `tests/mkvcore_paridade.c` roda as duas implementacoes com o mesmo corpus (arquivos reais e fuzz curto) e compara byte a byte a saida (`MkvFaixa[]`, retorno).
- Fuzz: `cargo fuzz` ou o corpus de teste em C com mutacao (`afl`), so em host.

### 4.2 Criterios de sucesso

1. Paridade: 100% do corpus dos testes existentes igual ao C; 0 divergencia em N execucoes de fuzz (N a definir).
2. Toolchain: gera o `.a` e passa os guards em **todos** os alvos: webOS (arm.sh), `.tpk` 6+ e 4/5 (`tests/tpk40_tls.sh` sem PT_TLS, com `DT_HASH`), `.wgt` (emcc), Android nas 2 ABIs.
3. Tamanho: delta do `libnuvio.so`/`libmain.so`/`nuvio-proto.arm` <= orcamento fixado apos medir o baseline (proposta: <= 0,5% por alvo; **numero a validar**).
4. Fluidez: fps e tempo de abertura de MKV inalterados (medir na TCL/C9 e numa webOS, com `fluidez_perf_player`-like; nao alterar o caminho de desenho).
5. Quedas: nenhum aumento de assinatura em `mkv*` no banco de quedas na 2.1; **nao** se promete reducao (nao ha baseline de quedas desse modulo, **suspeita** de que seja zero antes e depois).

### 4.3 Plano de saida

- Qualquer falha de guard em qualquer alvo, ou delta de tamanho acima do orcamento: desligar a flag, o C continua o padrao; remover `rust/` num commit.
- Se so um alvo falhar (ex.: wasm), manter Rust nos outros e C naquele (a flag e por alvo).
- Nada no formato de pacote, no manifest ou no host .NET muda; reverter e trocar uma variavel de build.

### 4.4 Versao e esforco

- Sugestao: **2.1**, com a flag desligada por padrao no primeiro candidato e ligada so no Android/preview `.tpk` 6+ numa segunda rodada.
- Esforco (estimativa grosseira, **suspeita**): 1 a 2 semanas para a toolchain nos 5 alvos e a CI; 1 semana para portar ~300 a 400 linhas do nucleo EBML e a paridade; +1 semana de soak em TV real. Total 3 a 4 semanas de uma pessoa, a maior parte e toolchain.

## 5. Alternativas sem Rust, comparadas nos mesmos dados

### 5.1 O que ja existe

- 567 arquivos `tests/*.c`, 548 `tests/*.sh`; **152** scripts aceitam `SANITIZE=1` (ASan+UBSan) e 29 ainda mencionam `thread` (TSan: `SANITIZE=thread`, ex. credfio 05706544).
- `.github/` so tem templates de issue: **nao ha CI**. `tools/testa-tudo.sh` roda a suite leve na maquina do dono, **sem** sanitizador.
- O proprio 696afa74 foi achado com "ASAN no app inteiro no Mac".

### 5.2 Quantos dos bugs do ranking cada ferramenta pegaria

| Bug | ASan/UBSan | TSan | Rust |
|---|---|---|---|
| 696afa74 UAR conferencia (28% Android) | sim, ja pegou (so no app inteiro; o teste unitario da cena 8 foi escrito depois) | nao precisa | so se `streams.c` tambem for Rust |
| 6c504c0b memcpy fora do bloco (catalogo) | sim (`catcorrida.sh`, `heap-buffer-overflow`) | n/a | sim |
| 33fd1fec/52dc67ef RLE heap OOB | sim (`catcachecorrida.sh`) | n/a | sim |
| #344 `ps[24]` OOB (guia) | sim (`guiabusca_faixa.sh`, `stack-buffer-overflow`) | n/a | sim se `faixa` devolve slice |
| #361/4f3a2584/197468ec truncamento | **nao** (nao e UB, o dado so e cortado) | n/a | sim (String), mas so se o tipo atravessa a ponte |
| corridas trakt/sync/vistoep/descoberta/colecoes | n/a | sim, exige o teste de cada cena (como 0fa4dac8, 20ace9d4) | sim (Mutex<T>) |
| #384 limite 8000 blocos | nao | nao | nao (logica) |
| 9df9c50a overflow unsigned no prazo | UBSan sinaliza overflow **com sinal** ou com `-fsanitize=unsigned-integer-overflow` (clang) | n/a | nao em release (`wrapping` por padrao); sim com `overflow-checks` |

Leitura: dos ~10 grupos de bug acima, ASan+TSan teriam pego **todos os que sao UB ou corrida**, e ja pegaram a maioria, **depois** de a versao com o bug sair (2.0.0 e 2.0.2). O que ficou entre o commit e o usuario foi falta de execucao obrigatoria, nao falta de ferramenta. Truncamento e logica nao sao pegos por nenhum dos dois; Rust pega truncamento so quando o tipo atravessa; logica, nenhum.

### 5.3 Pacote proposto (ordem)

1. **CI minima** (GitHub Actions ou script do dono no Mac): `SANITIZE=1` e `SANITIZE=thread` nos 152 + 29 testes ja prontos; falha o PR. Custo: horas. Observar: `e52fae3f` reduziu uma suite de 10 min para ~200 s, sinal de que tempo e um limite; separar em "rapida" e "noturna".
2. **ASan no app inteiro** num alvo do Mac (como foi feito para achar 696afa74) rodando um roteiro fixo (abrir titulo, "Melhor fonte", home, busca, guia, trocar de catalogo) a cada release candidate. Hoje isso e manual.
3. **Auxiliares limitados**: `nv_cpy(dst, cap, src)` que retorna truncamento por valor e loga; `nv_sprintf_ap`; helper `nv_slice(buf, cap, n)` que devolve `min(n,cap)` (fecha a familia #344 e trunca com aviso). Auditar `grep -nE '\b(sprintf|strcpy|strcat)\('` (5 ocorrencias) e todo `int n = xxx_faixa(..., cap)`.
4. **Convencoes de posse**: regra escrita ("buffer devolvido por funcao `*_faixa/_lista` e preenchido ate `cap`, retorno e sempre `<= cap`"; "objeto que fio referencia nunca esta na pilha de quem o cria": `ConfDona`/refcount ja existe, generalizar com helper `Ref`), mais um grep-guard no `.sh` como o que 696afa74 ja pos em `tests/fonteparalela.sh`.
5. **Checklist de endurecimento por release**: `-Wall -Wextra -Wformat=2 -Wstringop-overflow`, `-D_FORTIFY_SOURCE=2` onde o toolchain deixa, `-fstack-protector-strong` nos alvos Linux, `UBSan -fno-sanitize-recover` nos testes, `clang-tidy` ou `scan-build` uma vez por release (suspeita: ruidoso).
6. **Fuzz curto** nos parsers de dado externo (EBML, XMLTV, JSON de addon, ASS) com libFuzzer+ASan em host.

### 5.4 Comparacao

| | Sanitizadores primeiro | Rust (piloto 4) | Rust (reescrever streams/catalogo) |
|---|---|---|---|
| Cobre os bugs que mais derrubaram gente (UAR, heap OOB, corrida) | sim, ja demonstrado | **nao** (modulo escolhido nao os contem) | sim, na area portada |
| Custo | horas a dias | 3 a 4 semanas | meses (suspeita; 7 a 10 mil linhas e 69 includers) |
| Risco em TV | nenhum (so host) | `.so` 4/5 sem TLS, glibc 2.12 no webOS, wasm+pthreads | os mesmos, em codigo critico |
| Acha truncamento/logica | nao | parcial | parcial |
| Reversivel | trivial | flag + fallback C | dificil |

## 6. Recomendacao

1. Agora (2.0.3.x/2.1 beta): **sanitizadores em CI e ASan do app inteiro**, helpers de buffer limitado, convencao de posse. Isso cobre, nos dados deste repositorio, o mesmo conjunto de bugs de memoria que Rust cobriria, a custo muito menor e sem tocar em nenhum alvo de TV.
2. Em paralelo, **corrigir #344 em guia.c** (ainda aberto em 8ed4517c) com `min(n, cap)` em C; e auditar os outros usos de `epg_faixa`/`xtepg_faixa`.
3. So depois: piloto Rust "mkvcore" (sec. 4), como experimento de toolchain com flag e fallback C. Se os guards passarem nos 5 alvos, reavaliar portar `fonteparalela`+`stream_primeira_boa` e o publicador do catalogo, que sao onde esta o ganho verdadeiro de Rust.
4. Nao portar nada que use `std::thread`/`thread_local` para o `.tpk` 4/5 nem para o webOS (glibc 2.12): o guard de TLS e o piso de glibc dizem que `no_std` e a unica forma.

## 7. Limites deste estudo

- Contagens vem de filtro por palavras-chave em mensagens de commit, mais leitura de corpo dos achados; subestima bugs corrigidos sem essas palavras e nao ranqueia por severidade medida.
- Numeros de pessoas afetadas so existem para 696afa74 (informado pelo dono); para os demais, o banco de quedas nao foi consultado aqui.
- Nao ha `cargo`/`rustup` neste Mac, nem binarios de baseline no worktree: tamanhos e tempos de build estao como **suspeita**, e a viabilidade em webOS 3.x (glibc exata), `wasm32-unknown-emscripten` com pthreads e `-mfpu` equivalente precisam de teste real antes da fase 3.
- Nenhum codigo de produto foi alterado.
