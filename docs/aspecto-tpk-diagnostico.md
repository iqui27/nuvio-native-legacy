# Samsung .tpk — #426 / #341

Diagnóstico opt-in, sem correção de produção nem prova em TV. Referência: HEAD
antes desta alteração, `2.0.4`. As três fotos de #426 foram inspecionadas; não
permitem deduzir a geometria nativa. `BEZMQ6` é a referência fornecida na issue;
o conteúdo desse log não foi obtido nesta sessão.

## Caminho atual (sem NV_ASPECTO_DIAG)

1. `player.c:aspectoQuadro/aspectoRect` usa `video_largura/altura`, calcula
   contain/cover/fill e depois a escala de cada um dos oito modos. Não há SAR/DAR
   separado nessa conta. `aspectoVisivel` intersecta o destino com 1920×1080.
2. `aplicarAspecto` converte destino visível em fonte+destino (coordenadas pares),
   chama `video_janela_fonte`; sem dimensões chama `video_janela`. PiP/animações
   têm caminhos próprios. Reaplica após primeiro quadro/mudança de tamanho.
3. `video_tpk.c:video_janela_fonte` **emula** recorte de fonte: escala o quadro
   inteiro por `dw/sw, dh/sh`, recua a origem, chama `hJanela`. Retém ROI para
   reaplicação. Na produção não chama `SetVideoRoi`.
4. `Video.cs` registra `hJanela` e despacha `Janela` no fio principal, por sessão.
   `GetVideoProperties().Size` alimenta `EV_TAMANHO`. Os quatro projetos incluem
   esse mesmo arquivo. `Program40.cs` não tem um segundo player de filmes.
5. 6+: destino **exatamente** 0,0,1920,1080 vira `LetterBox`; outro retângulo
   vira `Mode=Roi; SetRoi`. 4/5: `JanelaTizen45` restaura janela+LetterBox no
   destino cheio; fora disso muda `janelaVideo.Geometry` e usa `FullScreen`,
   com fallback para ROI apenas se houver erro. Sucesso sem mudança visual
   não dispara fallback.
6. `Program40.cs:194–203` cria a janela ElmSharp rebaixada e `Display(janelaVideo)`.
   `Program.cs:DisplayDoVideo` usa Display NUI; API8/9 tenta associar ao Ecore
   da GLWindow por reflexão; API11 usa a janela principal/GLView. A atribuição
   `p.Display` ocorre antes de Prepare, não a cada ajuste de aspecto.
7. `src/tpk.c` coordena contexto EGL/fios, tamanho de UI e teclas; não escala
   pixels de vídeo. `nv_tpk_log` encaminha o log do host ao `nuvio.log`.
   `player.c` abre um **furo alfa** limitado ao destino visível e cobre o resto
   de preto: isso mascara o plano, não recorta a fonte no decodificador.
8. `AJ_TRAILER_ZOOM_TPK` (`ajustes.c`, `main.c:1331/1812`) alimenta `zoomRoi`.
   6+ só libera ROI fora da tela/zoom com esse opt-in ou `NV_TPK_ZOOM_ROI`.
   4/5 já libera os modos do player por `NV_TPK_PLAYER_RECORTE=1`; trailer
   continua condicionado ao ajuste. O diagnóstico não grava esse ajuste.

## API: existência não é funcionamento comprovado no firmware

| Mecanismo | Evidência/versionamento | Uso nesta árvore |
|---|---|---|
| `Player.Display`, `Handle` | [TizenFX Player.Properties.cs](https://github.com/Samsung/TizenFX/blob/main/src/Tizen.Multimedia.MediaPlayer/Player/Player.Properties.cs): `since_tizen 3`; Display exige Idle | Associação inicial em todos os hosts; não se reassocia durante playback |
| `DisplaySettings.Mode`, `SetRoi` | [TizenFX PlayerDisplaySettings.cs](https://github.com/Samsung/TizenFX/blob/main/src/Tizen.Multimedia.MediaPlayer/Player/PlayerDisplaySettings.cs): `since_tizen 3`; desde 4 admite SetRoi antes de Mode=Roi | Presentes nos SDKs usados pelos quatro pacotes; ROI é destino |
| LetterBox / FullScreen / CroppedFull / Roi | [Enum oficial](https://samsung.github.io/TizenFX/latest/api/Tizen.Multimedia.PlayerDisplayMode.html); os quatro constam no XML do SDK `Tizen.NET.API4 4.0.1.14164` | Produção usa LetterBox/FullScreen/Roi; diagnóstico também testa CroppedFull |
| `ElmSharp.EvasObject.Geometry` | XML `ElmSharp` do mesmo SDK4: propriedade pública, `since_tizen preview`; [fonte](https://github.com/Samsung/TizenFX/blob/main/src/ElmSharp/EvasObject.cs) | Só host 4/5. API de geometria não garante recorte de plano de vídeo nem origem negativa |
| `Player.SetVideoRoi` | [TizenFX Player.cs](https://github.com/Samsung/TizenFX/blob/main/src/Tizen.Multimedia.MediaPlayer/Player/Player.cs): `since_tizen 5`, fonte normalizada 0..1, após Display, somente Overlay | Não existe no SDK4 gerenciado. Método D chama a mesma função C por P/Invoke, apenas se runtime informar Tizen >=5; símbolo ausente/recusa fica no log |
| Recorte fora do painel | [Porting multimedia](https://github.com/Samsung/tizen-docs/blob/master/docs/platform/porting/multimedia.md): custom ROI é destino | Nenhuma dessas fontes promete que uma TV aceite ROI negativo/maior que o painel |

Declarações P/Invoke conferidas em
[Interop.Display.cs](https://github.com/Samsung/TizenFX/blob/main/src/Tizen.Multimedia.MediaPlayer/Interop/Interop.Display.cs),
[Interop.Player.cs](https://github.com/Samsung/TizenFX/blob/main/src/Tizen.Multimedia.MediaPlayer/Interop/Interop.Player.cs)
e [Interop.Libraries.cs](https://github.com/Samsung/TizenFX/blob/main/src/Tizen.Multimedia.MediaPlayer/Interop/Interop.Libraries.cs).
Não são APIs AVPlay do `.wgt`. Cópias consultadas ficam em `build/aspecto-evidence/`.

## Hipótese, não causa confirmada

Um vídeo 1920×800 em Original produz destino 0,140,1920,800. Como não é o
retângulo cheio, o host 4/5 usa **Geometry + FullScreen**, não LetterBox.
Se a TV conservar o display inicialmente associado a 1920×1080, ignorar ou
limitar a nova janela, FullScreen estica o vídeo; o furo da UI só o mascara.
A marca `janelaMovida` também só é ligada após FullScreen bem-sucedido, então
uma falha após mover a janela pode deixar geometria residual no caminho atual.

Isso explica de forma testável #426 e a ausência de efeito de #341, mas há
alternativas: dimensões codificadas diferentes do DAR, barras embutidas na
fonte, ROI ignorado sem erro, ou comportamento de firmware. Sem BEZMQ6 e uma
Samsung física não se escolhe uma delas como causa. A/B/C/D isolam essas rotas.

## Experimento implementado

Tecla 9 (controle) ou X (teclado) percorre A → B → C → D → OFF, omitindo C
nos hosts 6+ e D quando a versão reportada é anterior a 5/desconhecida.
Antes de ativar, o player usa seu comportamento normal. Enquanto ativo,
o botão de aspecto/tecla 0 alterna Original / Fill / Zoom sem salvar preferências.
Original=contain, Fill=esticar, Zoom=cover central (sem a escala adicional dos
modos de produção). Em 16:9 sem barras, Original e Zoom são iguais: testar
widescreen/4:3. Barras codificadas dentro de um quadro 16:9 não são detectadas.

A usa modos nativos; B usa ROI de destino; C usa janela+FullScreen; D usa ROI
de fonte normalizado. Todos usam o mesmo objetivo geométrico. O furo da UI
fica em tela cheia para não mascarar o resultado. PiP encerra o experimento;
fechar/trocar filme restaura a janela e descarta o Player. OFF restaura fonte,
janela e caminho normal. Falha de reset exige fechar/reabrir o vídeo.

A pílula mostra método, modo e pending/accepted/FAILED. `accepted` significa
rc=0, nunca confirmação visual. Cada pedido gera `[aspecto-diag]` com Tizen,
modelo, dimensões, estado, retângulos, retornos nativos hex e aceitação. O setter
ElmSharp chama APIs void: registra `void:accepted` ou erro, sem inventar rc.
Não há fallback oculto entre os métodos. Nenhuma chamada nova entra sem flag.

## Provas locais

- `bash tools/tpk.sh` antes/depois, sem flag: rc=0, quatro pacotes em cada.
  `.so` comum e 4/5 byte-idênticas; comparador de IL/campos dos quatro hosts:
  zero diferenças (492/501/501/507 entradas). Evidências: `normal-equivalence.log`,
  `normal-sha256.txt`, baseline/normal `.tpk` em `build/aspecto-evidence/`.
- `for t in tests/*tpk*.sh; do bash "$t"; done`: os 11 scripts retornaram 0;
  lista individual em `build/aspecto-evidence/tests-rc.txt`.
  O novo teste liga `Video.cs`/`AspectoDiag.cs` reais a uma biblioteca C simulada;
  cobre A/B/C/D, wide/4:3, reset entre métodos, rc negativos, Tizen4 sem API5,
  dimensões ausentes, metadados, ponte ELF-compatible e teclas com/sem flag.
- `bash tests/trailer-recorte.sh`: rc=0.
- `bash tests/player.sh`: rc=134 em `tests/player_regression.c:161`,
  `!player_controles_visiveis()`. Mesmo erro com `player.c` original de HEAD
  compilado contra as mesmas demais unidades; não é evidência de regressão nova.
- A build normal de referência estava sem configuração de servidor (aviso do
  env.sh): serve à equivalência de compilação, não à distribuição. O diagnóstico
  exige `tools/env.sh --require-core` e usa a configuração local existente.

- `NV_ASPECTO_DIAG=1 NV_ASPECTO_DIAG_VERSION=2.0.5-aspect.1` + configuração
  indicada em ENTREGA, `bash tools/tpk.sh`: rc=0, quatro `.tpk`.
  `python3 build/aspecto-evidence/check-packages.py`: rc=0; conferiu flags no C
  e host, versão interna/manifesto, payload nativo e ausência de arquivos pessoais.
  SHA256 em `build/tpk/SHA256SUMS-aspecto`. `git diff --exit-code` dos manifestos,
  appinfo.json e tizen-config.xml: rc=0. Sem instalação, publicação ou teste em TV.
