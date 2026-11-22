# Sessão: a sala decide pelo celular

Origem: pesquisa de mercado, seção 4.6 (dono: "vamos fazer todas"). Este arquivo
descreve a **primeira etapa**, o que ela mede e o que falta.

## O que a primeira etapa faz

Ajustes > Conta e serviços > Sessão > **Sessão pelo celular** abre uma tela com um QR.
Quem está na mesma rede Wi-Fi aponta a câmera, digita um nome e entra, sem instalar
nada. Com todos dentro, OK na TV começa: cada celular recebe os mesmos 20 cartões e
responde sim ou não (botões ou deslizando). O primeiro título a ter sim de todos
aparece na TV ("Todos disseram sim"); OK abre a página dele.

- Sem unanimidade depois de todos verem tudo, ou com OK na TV no meio da votação,
  vence o título com mais sins ("A maioria escolheu"). Sem nenhum sim, OK começa outra
  rodada com títulos novos.
- Quem chega atrasado ainda entra e passa a contar para a unanimidade.
- Voltar fecha a tela e derruba o servidor.

Nada muda para quem não abre a ação: não há porta aberta, fileira nem tela nova fora dela.

## Como é feito

| Peça | Arquivo | Papel |
|---|---|---|
| Regra | `src/sala.c`, `src/sala.h` | Entrada, voto, unanimidade, maioria, estado em JSON. Sem rede nem tela. |
| Tela da TV | `src/salaui.c`, `src/salaui.h` | Baralho, QR, quem entrou e quanto já votou, resultado. |
| Página do celular | `src/salapagina.h` | Uma página só, embutida; nada externo além dos cartazes. |
| Servidor | `src/celular.c` | O mesmo de "Digitar pelo celular", em modo sessão. |

O servidor ganhou uma opção só para a sessão seguinte (`celular_sessao_opcoes`): validade
de 30 min em vez de 5 e `img-src https:` na política de conteúdo, para os cartazes. As
outras aberturas continuam com `default-src 'none'`.

Protocolo (texto no campo `t` do POST, conferido inteiro em `sala_mensagem`):

- `j:<chave>:<nome>` entra; `v:<chave>:<cartão>:<0|1>` vota.
- `<chave>` são 12 hex gerados no celular: 4 de id público + 8 de segredo. O estado que
  todos leem (`GET /<token>/s`) leva só o id público, o nome e quantos cartões cada um viu.

## Cuidados com o visitante

- A página leva só o baralho da rodada (título, ano/duração, endereço do cartaz) e os
  nomes digitados. Nada da conta, do perfil, do histórico ou dos add-ons.
- Cartaz com parâmetro na URL ou maior que 200 bytes (ex.: PostersPlus com chave) não
  vai para o celular; entra o cartaz público do mesmo id.
- Perfil infantil: só entram títulos de classificação livre/10 anos
  (`perfil_infantil_classif_ok`); sem classificação conhecida, fica de fora.
- Nome de visitante: até 24 bytes, sem controles, escapado no JSON e escrito na página
  por `textContent`. Na TV é desenhado sem tradução (`txt_bloco_bruto`).
- Valem as regras de `celular.h`: token no endereço, Host/Origin conferidos, 30 caminhos
  errados derrubam o servidor, nada sai da TV.

## Verificado e não verificado

Verificado no Linux (build de prévia): a ação nos Ajustes, o QR, dois celulares
emulados no Chrome (entrar, botões, deslizar por toque), progresso na TV, unanimidade,
"sem acordo" e nova rodada, abertura do título vencedor, servidor fechado ao sair.
`tests/sala.sh` (regra) e `tests/sala_celular.sh` (servidor + regra com curl).

Não verificado: TV física (LG, Samsung .tpk, Android), celular de verdade lendo o QR,
perfil infantil com catálogo real, catálogo de conta real (a prévia usa o catálogo de
demonstração). Em `celular.h` consta que a escuta na LAN nunca foi medida em LG nem em
Samsung .tpk; se a TV barrar, o QR aparece mas a página não abre. No .wgt (navegador)
não há servidor e a ação não aparece.

## O que falta (da ideia completa)

1. Servidor de retransmissão, para quem não está na mesma rede e para TVs que não
   aceitam conexão de entrada. O Durable Object do Assistir Juntos já tem QR de celular
   e sala; é o caminho natural. Não foi feito aqui: não dá para publicar servidor.
2. Tocar direto o vencedor (hoje abre a página do título).
3. Fila compartilhada e reações na tela depois da escolha.
4. Entrada mais visível (Home, menu, página do título). Hoje fica só nos Ajustes.
5. Escolher de onde vem o baralho (Salvos, um gênero, só o que tem fonte).
6. Voto pelo controle da TV, para quem está sem celular.
