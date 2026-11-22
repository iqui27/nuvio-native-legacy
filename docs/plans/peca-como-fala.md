# Peça como fala, pelo celular (2.1, etapa 1)

Pesquisa de mercado, seção 4.5. O celular lê o QR da TV uma vez, a pessoa fala
(ditado do teclado do celular) ou digita o que quer ver, e a TV mostra três
títulos confirmados no Cinemeta.

## O que a etapa 1 faz

- Ajustes > "Peça como fala" (liga/desliga, local, padrão Ligado) e "Parear
  celular". Nada sobe até parear; desligado, o servidor para.
- `pecaserv.c`: servidor HTTP na rede local, porta fixa 47803 e token de 12
  caracteres gravado em `peca_token.txt`, então o endereço não muda entre
  aberturas. Cima no cartão do QR troca o token e desconecta os celulares.
- `pecafio.c`: uma chamada ao Gemini (`gemini-flash-latest`) com a chave do
  usuário (`gemini.txt`, digitada na página do celular, só no cabeçalho
  `x-goog-api-key`), pedindo 6 candidatos em JSON; cada um é conferido no
  Cinemeta e os já vistos saem. Ficam 3.
- `peca.c`: regra pura (rotas, Host/Origin, limpeza, pedido, resposta,
  escolha do Cinemeta, estado). Teste: `tests/peca.sh`, `tests/pecaserv.sh`.
- Contexto enviado ao modelo: até 40 títulos vistos, em andamento e salvos do
  catálogo carregado. Perfil infantil não usa.

## O que falta

- Teste em TV física (webOS, Tizen .tpk, Android) e com a API real do Gemini:
  só foi exercitado no preview Linux, com Gemini e Cinemeta falsos.
- `.wgt` (Emscripten) e Windows: sem servidor local, a frente fica oculta.
- Fora da rede local: pede relay em servidor próprio (não publicado).
- Pareamento do #798: aqui o token é próprio da frente; unificar com o
  controle pelo celular quando o #798 sair.
- Se a porta 47803 estiver ocupada, cai numa porta efêmera e o endereço salvo
  no celular deixa de valer naquela abertura.
- Fonte tocável: "confirmado" hoje é existir no Cinemeta, não ter stream nos
  addons do usuário.
- Microfone na página (Web Speech exige HTTPS; hoje usa o ditado do teclado).
