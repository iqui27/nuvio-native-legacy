# Fontes distribuídas no Nuvio

Inter Display continua sendo o padrão da interface. As famílias adicionais são Montserrat, Roboto e Atkinson Hyperlegible Next. A seleção da interface e a seleção das legendas são independentes. A primeira fica em Aparência → Fonte da interface; a segunda permanece no painel de estilo da legenda do player. Legendas ASS renderizadas pelo libass mantêm a tipografia definida no arquivo.

Montserrat é uma alternativa de estética geométrica para a proposta visual solicitada. Não é Netflix Sans e não é apresentada como fonte oficial da Netflix.

Os arquivos adicionais são estáticos, fornecidos pelos autores, nos pesos Regular (400), Medium (500) e Bold (700). Nenhum peso foi simulado ou gerado nesta inclusão. As licenças SIL OFL 1.1 acompanham cada família em `deploy/app/fonts/*-OFL.txt`.

## Procedência

- [JulietaUla/Montserrat](https://github.com/JulietaUla/Montserrat/tree/555facfb2a18c72c3c0380f0d9c0f060453a9058) — commit `555facfb2a18c72c3c0380f0d9c0f060453a9058`.
- [googlefonts/atkinson-hyperlegible-next](https://github.com/googlefonts/atkinson-hyperlegible-next/tree/7925f50f649b3813257faf2f4c0b381011f434f1) — commit `7925f50f649b3813257faf2f4c0b381011f434f1`.
- [Roboto v3.016](https://github.com/googlefonts/roboto-3-classic/releases/tag/v3.016) — arquivos `hinted/static/` do pacote oficial.
- [Atkinson Hyperlegible, Braille Institute](https://www.brailleinstitute.org/freefont/) — apresentação da família pelos responsáveis.

## Integridade

| Arquivo | Bytes | SHA-256 |
|---|---:|---|
| `AtkinsonHyperlegibleNext-Bold.ttf` | 66792 | `994414047df66bb4998d01c1cb1eeb4a2ddc4622d1aa56bbb8adbeca7645b041` |
| `AtkinsonHyperlegibleNext-Medium.ttf` | 67080 | `dd50b08b3c560846097d23baaaf6a97ffa20dd077115d23c59df68083b9ea05e` |
| `AtkinsonHyperlegibleNext-Regular.ttf` | 65068 | `88ed5c31a71584c7772963b02d04bef1eb7e3d2e9c8b9cb204339b1f82cf432c` |
| `Montserrat-Bold.ttf` | 454864 | `bc6e854971cea46b463be6f9eef4d9cd52f51cfc1fc0dd90c9d3e6483dc0ec61` |
| `Montserrat-Medium.ttf` | 447320 | `dae47428bb041f9716604e0e07b5b0c8585b3bdd8183362f75c69fe7bb3cfaf4` |
| `Montserrat-Regular.ttf` | 445928 | `3e8abe50c44c82e2242e97d1ec8c0d385c4890cdc50447bcdb8605c81a38cfb2` |
| `Roboto-Bold.ttf` | 465944 | `44f13e570a8b3fc42240abfcb3d72fe298f3dd6a25db0c78d0c2495dbd0e9121` |
| `Roboto-Medium.ttf` | 464100 | `80ce163ec5ecd91a883aa20e80d31dc74971b55cf658dd7c5e8d21f4e9fcb417` |
| `Roboto-Regular.ttf` | 463712 | `9f300202f482ad59f8b13bc3131a295744d14fd530fa3765a0a11ec87264d203` |

Total dos nove arquivos adicionais: 2.940.808 bytes.

## Empacotamento

Os pipelines existentes incluem a pasta `deploy/app/fonts` no pacote nativo e no preload WebAssembly. As licenças devem acompanhar os arquivos de fonte. A inclusão dos arquivos no repositório não comprova instalação nem comportamento na TV.

## Fonte CJK embarcada (japonês e chinês)

A Inter não tem kana nem hanzi. `text.c` manda a linha para uma fonte de reserva quando falta qualquer caractere nela; na LG e no Mac a reserva vem do sistema, e no WASM da Samsung não existe fonte de sistema nenhuma. Por isso o pacote leva `DroidSansFallback-Subset.ttf`.

- Origem: Droid Sans Fallback 2.54, copyright Google 2006, Apache License 2.0 (o texto está em `DroidSansFallback-LICENSE.txt`). Foi lida de `/usr/share/fonts/DroidSansFallback.ttf` de uma LG C9.
- Modificada: subconjunto de 7.943 caracteres (ASCII, Latin-1, pontuação, kana, GB2312 nível 1, JIS X 0208 nível 1, Big5 de uso corrente e tudo o que as tabelas ja/zhcn/zhtw usam), sem hinting. Um subconjunto só dos textos da interface teria 0,34 MB; este tem 1,2 MB porque títulos e sinopses do TMDB em chinês ou japonês também aparecem com a interface em outro idioma.
- Negrito arabe (#335): `NotoNaskhArabic-Bold.ttf` inteiro (OFL, mesma licenca), usado pelo SDL_ttf e pelo libass quando presente; sem ele, arabe fica sem negrito sintetico.
- Regerar: `python3 tools/fonte-cjk.py /tmp/DroidSansFallback.ttf` (precisa de fontTools).
- Quem usa: só a reserva CJK, depois das fontes de sistema (`LG_Display_JP`, `DroidSansFallback`, `LG_Display-Regular`, `LG_Display_HK-Regular`; no Mac Hiragino/STHeiti). `tools/idiomas.py` confere que o que as tabelas ja/zh usam existe nela.
- Ver o que o WASM desenha, no Mac: `NUVIO_SEM_RESERVA_DE_SISTEMA=1 bash tests/idioma_shot.sh /tmp/x 27,28,29`.

| Arquivo | Bytes | SHA-256 |
|---|---:|---|
| `DroidSansFallback-Subset.ttf` | 1226652 | `7f4ff9c7ad1d4dd471e2f24ab27bc48b09d9dec0329a22b524f6664062eb7bc2` |

## Fonte árabe embarcada

A Samsung não traz fonte árabe e o texto saía em quadrados (#253, #258). O pacote leva `NotoNaskhArabic-Subset.ttf`, última da lista `ESC_ARABE` em `text.c`, depois das fontes de sistema (LG, Android, Mac).

- Origem: Noto Naskh Arabic Regular 2.021, The Noto Project Authors, SIL OFL 1.1 (`NotoNaskhArabic-OFL.txt`), de github.com/notofonts/arabic.
- Modificada: subconjunto com ASCII, U+0600–06FF, U+0750–077F e as formas de apresentação U+FB50–FDFF e U+FE70–FEFF, sem hinting e sem tabelas de layout. As formas de apresentação são obrigatórias: `src/bidi.c` faz a junção trocando cada letra por elas, não há HarfBuzz.
- Negrito arabe (#335): `NotoNaskhArabic-Bold.ttf` inteiro (OFL, mesma licenca), usado pelo SDL_ttf e pelo libass quando presente; sem ele, arabe fica sem negrito sintetico.
- Regerar: `python3 tools/fonte-arabe.py /caminho/NotoNaskhArabic-Regular.ttf` (precisa de fontTools).
- Conferir no Mac: `bash tests/text_familias.sh` (reabre o renderer com `NUVIO_SEM_RESERVA_DE_SISTEMA=1`).

| Arquivo | Bytes | SHA-256 |
|---|---:|---|
| `NotoNaskhArabic-Subset.ttf` | 90372 | `a43af24c9307d838d89593fc86342ab024973ffa0d7f71bac76849afe2ce5675` |
