# Plano: Nuvio nativo no iPhone Duo, iOS 27.2 e Mac

28/10/2026. Base: `integracao/2.1` (34c7984ae). Só plano; nada implementado.
Pedido do dono: mockups do app para iOS 27.2 com o iPhone Duo como aparelho
principal, para abrir a implementação no iOS e no Mac.

## O que já existe e pesa na decisão

| Peça | Estado na 2.1 | Vale para o iOS? |
|---|---|---|
| Núcleo C + SDL2 + GL ES 2, canvas de referência 1920×1080 | Todo o app (339 k linhas em `src/`) | Sim, inteiro |
| Modo Mobile (`NV_TOUCH_UI`, `layout_modo_mobile()`, `rolagemtoque.h`, `seekgesto.h`) | Mergeado 27/10 (`a989cbc06`), **só paisagem**, canvas dentro das barras do sistema | Sim; falta retrato |
| Hospedeiro nativo + `libmain` (padrão do .tpk em .NET e do Android em Kotlin) | Dois portes prontos com o mesmo contrato `video.h` | É o molde do `ios/` |
| Player por plataforma atrás de `video.h` (uMS, avplay, Tizen.Multimedia, Media3, `<video>`) | 5 backends | Falta `video_apple.m` (AVPlayer) |
| Build Mac (`tools/mac.sh`, SDL2 do Homebrew, OpenGL 2.1) | Roda hoje, **sem vídeo** (`video.c:311`, só simulação) | Mac = empacotar isso + o mesmo `video_apple.m` |
| Redimensionamento em tempo de execução | `main.c:1926` já trata `SIZE_CHANGED` quando `NV_TOUCH_UI` | Base do dobrar/abrir |
| Porte tvOS (NV_TVOS, do Dexter) | **Não está nesta árvore.** Os commits "Apple TV" da 2.1 são o *layout* Dinâmica/Apple TV da sidebar, não um alvo tvOS | Nada reaproveitável no repo; o porte do Dexter continua um PR externo a trazer |
| Motor P2P (`p2pmotor.c`, libtorrent) | Opcional por `-DNV_P2P_MOTOR` | **Fora do iOS** (App Store) |
| QuickJS (`pluginjs.c`) | Plugins JS | Ver restrição 2.5.2 abaixo |

## Decisão de arquitetura: núcleo C + canvas GL dentro de um hospedeiro UIKit (recomendado)

Três caminhos foram pesados:

**A. SDL2 puro no iOS** (o `main()` como está, SDL dono da janela). Mais barato, mas o
app não ganha nada do iOS 27.1: sem barras verticais, sem `ArrangementView`, sem
regiões reservadas, sem Liquid Glass. No Duo fica um app "não pensado". Serve só
de spike.

**B. Concha SwiftUI nativa sobre o núcleo C.** Todas as telas reescritas em SwiftUI;
o C vira biblioteca de dados (addons, rede, conta, progresso). Melhor resultado no
Duo, mas são as 11 telas maiores do repo (`ajustes.c` 12 k, `descoberta.c` 10 k,
`detail.c` 9 k, `home.c` 9 k, `guia.c` 8 k, `player.c` 7,5 k...) refeitas em
outra linguagem, fora da linguagem oficial do 2.0.8/DESIGN.md e sem os testes
`*_shot` que hoje provam cada tela. Meses, não semanas.

**C. Híbrido: hospedeiro UIKit/SwiftUI + canvas SDL/GL como conteúdo.** É o mesmo
desenho do .tpk (host .NET) e do Android (Kotlin), só que no iOS o hospedeiro
também entrega o que o Duo pede:

```
NuvioApp (Swift)                                     libnuvio.a (src/*.c, -DNV_IOS -DNV_TOUCH_UI)
 ├─ UITabBarController / UINavigationController     ├─ main() de sempre, canvas 1920×1080 escalado
 │   (barras verticais do 27.1 de graça)             ├─ src/ios.m        ambiente, pastas, insets, hinge, teclado
 ├─ SDLView (GL ES 2) = conteúdo da tela             ├─ src/video_apple.m  AVPlayer atrás do canvas, furo alfa 0
 ├─ AVPlayerLayer (embaixo, como a SurfaceView)      └─ layout.c         recebe safe area + regiões reservadas
 └─ UIHingeInteraction, ReservedRegion → C               por bordas independentes, não por "margem × 2"
```

Por que C:
- Reaproveita o modo Mobile e os dois portes hospedeiro+lib; quem já fez `android.c`
  e `video_android.c` faz `ios.m` e `video_apple.m` pelo mesmo molde (~650 + ~1.400
  linhas de referência).
- O que o Duo exige é quase todo **de fora do canvas**: barras verticais, folhas
  que desviam da dobra, regiões reservadas, hinge. Isso o hospedeiro faz com
  contêineres do sistema; o C só precisa aprender a receber **insets por borda**
  e a **retângulo da dobra**, o que já começou no Android (canvas dentro do notch).
- `video_apple.m` serve iOS, iPadOS e macOS (AVFoundation é a mesma API), e
  destrava o vídeo no Mac que hoje não existe.
- Mantém os testes `*_shot` e o DESIGN.md como prova.

Riscos do C, com mitigação:
- **GL ES 2 está deprecado no iOS desde o 12** (ainda roda no 26/27). Mitigação:
  SDL 2.30 continua com `SDL_uikitopenglview`; se a Apple remover, ANGLE sobre
  Metal (o SDL2 compila contra ele) ou backend Metal em `gfx.c`. Não bloqueia
  o começo; é dívida conhecida.
- **Aninhar a view do SDL num `UINavigationController` próprio.** SDL cria o seu
  `UIViewController`; a gente pega a `UIWindow` por `SDL_SysWMinfo` e envolve o VC
  do SDL. Spike 1 decide; se não der, cai no plano A' (barras desenhadas pelo C,
  sem verticais) só no primeiro marco.
- **Retrato.** O modo Mobile é só paisagem; o Duo fechado é retrato por natureza
  (466×678 pt). O canvas 1920×1080 escalado para retrato não serve; precisa de
  um canvas lógico retrato (ex.: 1080×1570) com as telas reagindo a
  `layout_modo_mobile()` + largura<altura. É o maior trabalho **dentro** do C.

## Alvos de build

| Alvo | Como | Quando |
|---|---|---|
| iOS 27.1 SDK+ (iPhone, Duo) | `ios/Nuvio.xcodeproj`, SDL2 estático via o projeto Xcode do SDL, `libnuvio.a` por CMake (o mesmo `CMakeLists` do Android, toolchain iOS) | Marco 1 |
| iPadOS | O mesmo binário; o Duo aberto **é** regular×regular, então o layout de iPad sai de graça (e vice-versa: nada de "regular = iPad") | Marco 2 |
| macOS | Nativo (não Catalyst): o `tools/mac.sh` já roda; vira `.app` assinado com `video_apple.m`. Catalyst só se a loja do Mac pedir o mesmo bundle | Marco 3 |
| tvOS | Fora deste plano; entra quando o PR do Dexter (NV_TVOS) chegar. `video_apple.m` serve lá também |

Mínimo: **iOS 27.1 SDK** (Xcode 27.1). É o degrau que liga barras verticais,
`ReservedRegion`, `ArrangementView` e `UIHingeInteraction`. Alvo de
implantação iOS 26 com `@available(iOS 27.1)` nas APIs novas; a partir de
abril/2027 a loja exige SDK 27+.

## Restrições da App Store para um cliente de add-ons estilo Stremio

Não são opinião; são as regras que derrubam apps desse tipo. Cada uma muda o
que vai no binário:

1. **5.2.3 (conteúdo de terceiros sem direito)** — a regra que mantém o Stremio
   oficial fora da App Store (confirmar o estado atual antes do envio). O app
   **não pode embarcar** add-on de pirataria nem lista padrão que aponte para
   isso. Lista embutida no iOS = só Cinemeta/OpenSubtitles/oficiais; o resto vem
   da conta do usuário (o `phone-setup.md` já descreve esse fluxo).
2. **Sem torrent no aparelho.** `NV_P2P_MOTOR` fica de fora; debrid e links
   diretos só. (Também pelo limite de execução em segundo plano.)
3. **2.5.2 (não baixar nem executar código).** QuickJS rodando plugin JS
   baixado é violação. No iOS: plugins JS só os embarcados no bundle, ou trocar
   por `JavaScriptCore` (permitido) — decidir no marco 2.
4. **Manifesto de privacidade** (iOS 17+) e `NSAppTransportSecurity`: add-ons em
   `http://` precisam de exceção declarada; o Review pergunta.
5. **4.2 funcionalidade mínima / 4.1 cópia**: nome, ícone e marca do app
   precisam ser nossos ("Nuvio Legacy"), não "Stremio".
6. **Compras**: nenhuma (debrid é assinado fora). Login por QR com a conta
   Nuvio é aceito; Face ID não existe no Duo (Touch ID no botão lateral).

Caminhos de distribuição, do mais fácil ao mais exposto: Xcode/sideload para o
dono → **TestFlight** para a comunidade (revisão leve, 90 dias) → App Store com
a lista limpa → mercados alternativos na UE (AltStore) se a loja negar.

## O que o Duo muda nas telas (base para os mockups)

Regra única: **largura compacta = tela externa = o app de celular; largura
regular = tela interna = um painel de celular + um painel extra.** Nunca "se Duo".

| Tela | Fechado (466×678) | Aberto (951×669, dobra em 475,5) | Contêiner |
|---|---|---|---|
| Home | Herói + fileiras, barra vertical à direita | Herói cruza a dobra (mídia pode), fileiras com nº par de colunas | TabView + canvas |
| Busca | Lista | Lista \| Detalhe | `NavigationSplitView` |
| Detalhe | Herói, ações, episódios empilhados | Detalhe \| Fontes (ou Episódios) | `ArrangementView(.split)` |
| Fontes | Folha | Folha trailing com barra vertical, nunca na dobra | sheet, `preferredPlacement` |
| Player | Controles sobre o vídeo | Mesa: vídeo em cima, controles/episódios/chat embaixo; livro: vídeo \| lista | `ArrangementView(.overlay)` → `.split` |
| Guia de TV | Grade em paisagem forçada hoje | Grade \| preview do canal | `ArrangementView(.split.axes(.horizontal))` |
| Biblioteca / Listas | Grade 4 col | 4 + 4 colunas, nenhuma na dobra | canvas, `ReservedRegion(.division)` |
| Perfis / Onboarding | Fluxo de uma coluna | Mesmo passo expande em dois painéis, nunca reinicia | cover full-screen |
| Ajustes | Lista → página | Lista \| página | `NavigationSplitView` |

Para o C: `layout_insets(l,t,r,b)` e `layout_dobra(x,y,w,h,ativa)` alimentados
pelo hospedeiro (`safeAreaInsets` + `reservedRegions(kind:)`). Hinge só para
efeito (parallax do herói, "fechar salva"), nunca para layout.

## Marcos

0. **Ambiente** (1 dia): Xcode 27.1, simulador do iPhone Duo no Device Hub, SDL2
   2.30.9 compilando para iOS (projeto Xcode do SDL), CMake iOS para `libnuvio.a`.
1. **Spike: Home no simulador Duo, sem vídeo** (1 semana): `ios/` + `src/ios.m`
   (pastas em `Application Support`, arte copiada como no Android, log no
   `os_log`), VC do SDL dentro de `UINavigationController` para provar barras
   verticais, insets por borda chegando ao `layout`. Prova: abrir/fechar o Duo
   sem perder estado, nada tapado pela câmera.
2. **Retrato no modo Mobile** (2–3 semanas, maior item): canvas lógico retrato,
   Home/Busca/Detalhe/Biblioteca/Ajustes reagindo a largura<altura. Testes
   `*_shot` novos por orientação. É aqui que os mockups aprovados viram código.
3. **Vídeo** (1–2 semanas): `video_apple.m` sobre AVPlayer com o contrato de
   `video_android.c` (faixas, legenda embutida, HDR/DV pelo `AVAsset`,
   reconexão). Furo alfa como nas TVs. Serve Mac no mesmo passo.
4. **Duo de verdade** (1–2 semanas): `ArrangementView` em Detalhe/Player/Guia,
   `ReservedRegion` nas grades, folha de Fontes fora da dobra, hinge nos efeitos.
   Matriz de teste: {fechado, livro, aberto, mesa} × {retrato, paisagem} × {tela
   cheia, Split View esq/dir}.
5. **Mac** (1 semana): `.app` assinado a partir do `tools/mac.sh` + vídeo;
   janela redimensionável já cai no mesmo caminho do dobrar.
6. **Loja** (1 semana): lista de add-ons limpa, sem P2P, manifesto de
   privacidade, capturas do Duo em todas as poses (ganha o selo "Made for
   iPhone Duo"), TestFlight para a comunidade antes da App Store.

Total realista para o primeiro TestFlight (marcos 0–3): **5–7 semanas** de um
agente dedicado; Duo completo e Mac (4–5): mais 2–3. Mockups aprovados pelo dono
antes do marco 2 (regra de UI: PNG aprovada antes de implementar).

## Fora do plano

- tvOS (espera o porte do Dexter).
- Catalyst.
- Backend Metal (só se a Apple tirar o GL ES).
- Android em retrato: cai de graça quando o marco 2 fechar, mas não é meta aqui.
